/***************************************************************************************
 *
 *  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
 *
 *  By downloading, copying, installing or using the software you agree to this license.
 *  If you do not agree to this license, do not download, install, 
 *  copy or use the software.
 *
 *  Copyright (C) 2014-2020, Happytimesoft Corporation, all rights reserved.
 *
 *  Redistribution and use in binary forms, with or without modification, are permitted.
 *
 *  Unless required by applicable law or agreed to in writing, software distributed 
 *  under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 *  CONDITIONS OF ANY KIND, either express or implied. See the License for the specific
 *  language governing permissions and limitations under the License.
 *
****************************************************************************************/

#include <string.h>
#include <stdio.h>
#include "sys_inc.h"
#include "rtp.h"
#include "rtsp_cln.h"
#include "mpeg4.h"
#include "mpeg4_rtp_rx.h"
#include "bit_vector.h"
#include "base64.h"
#include "rtsp_util.h"

#ifdef BACKCHANNEL
#include "rtsp_backchannel.h"
#endif

#ifdef OVER_HTTP
#include "http_parse.h"
#include "http_cln.h"
#endif

/***************************************************************************************/
void * rtsp_tcp_rx_thread(void * argv)
{
	CRtspClient * pRtsp = (CRtspClient *)argv;

	pRtsp->tcp_rx_thread();

	return NULL;
}

void * rtsp_udp_rx_thread(void * argv)
{
	CRtspClient * pRtsp = (CRtspClient *)argv;

	pRtsp->udp_rx_thread();

	return NULL;
}

int video_data_cb(uint8 * p_data, int len, uint32 ts, uint32 seq, void * p_userdata)
{
	CRtspClient * pthis = (CRtspClient *)p_userdata;

	pthis->rtsp_video_data_cb(p_data, len, ts, seq);

	return 0;
}

int audio_data_cb(uint8 * p_data, int len, uint32 ts, uint32 seq, void * p_userdata)
{
	CRtspClient * pthis = (CRtspClient *)p_userdata;

	pthis->rtsp_audio_data_cb(p_data, len, ts, seq);

	return 0;
}



/***************************************************************************************/
CRtspClient::CRtspClient(void)
{
	m_pNotify = NULL;
	m_pUserdata = NULL;
	m_pVideoCB = NULL;
	m_pAudioCB = NULL;
#ifdef METADATA	
	m_pMetadataCB = NULL;
#endif	
	m_pMutex = sys_os_create_mutex();
	
    m_bRunning = TRUE;
	m_tcpRxTid = 0;
	m_udpRxTid = 0;

	memset(&h265rxi, 0, sizeof(H265RXI));
	memset(&aacrxi, 0, sizeof(AACRXI));
	memset(&rtprxi, 0, sizeof(RTPRXI));

	set_default();
}

CRtspClient::~CRtspClient(void)
{
	rtsp_close();

	if (m_pMutex)
	{
		sys_os_destroy_sig_mutex(m_pMutex);
		m_pMutex = NULL;
	}
}

void CRtspClient::set_default()
{
    memset(&m_rua, 0, sizeof(RCUA));
    memset(&m_url, 0, sizeof(m_url));
    memset(&m_ip, 0, sizeof(m_ip));

    m_nport = 554;
    m_rua.rtp_tcp = 1;  // default RTP over RTSP
	m_rua.session_timeout = 60;
	strcpy(m_rua.user_agent, "happytimesoft rtsp client");
        
    m_pAudioConfig = NULL;
    m_nAudioConfigLen = 0;
        
    m_VideoCodec = VIDEO_CODEC_NONE;
    m_AudioCodec = AUDIO_CODEC_NONE;
    m_nSamplerate = 0;
    m_nChannels = 0;
    
#ifdef BACKCHANNEL
    m_bcAudioCodec = AUDIO_CODEC_NONE;
    m_nbcSamplerate = 0;
    m_nbcChannels = 0;
#endif
}

BOOL CRtspClient::rtsp_client_start()
{	
	m_rua.rport = m_nport;
	strcpy(m_rua.ripstr, m_ip);
	strcpy(m_rua.uri, m_url);

	if (rua_init_connect(&m_rua) == FALSE)
	{
		log_print(HT_LOG_ERR, "%s, rua_init_connect fail!!!\r\n", __FUNCTION__);
		return FALSE;
	}

	m_rua.cseq = 1;

	HRTSP_MSG * tx_msg = rua_build_options(&m_rua);
	if (tx_msg)
	{
		rcua_send_free_rtsp_msg(&m_rua, tx_msg);
	}

	this->m_rua.state = RCS_OPTIONS;

	return TRUE;
}

BOOL CRtspClient::rua_init_connect(RCUA * p_rua)
{
	SOCKET fd = tcp_connect_timeout(get_address_by_name(p_rua->ripstr), p_rua->rport, 5000);
	if (fd > 0)
	{
		int len = 1024*1024;
		
		if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char*)&len, sizeof(int)))
		{
			log_print(HT_LOG_ERR, "%s, setsockopt SO_RCVBUF error!\n", __FUNCTION__);
			return FALSE;
		}
		
		p_rua->fd = fd;

		return TRUE;
	}

	return FALSE;
}

void CRtspClient::rtsp_send_h264_params(RCUA * p_rua)
{
	int pt;
	char sps[1000], pps[1000] = {'\0'};
	
	if (!rua_get_sdp_h264_params(p_rua, &pt, sps, sizeof(sps)))
	{
		return;
	}

	char * ptr = strchr(sps, ',');
	if (ptr && ptr[1] != '\0')
	{
		*ptr = '\0';
		strcpy(pps, ptr+1);
	}

	uint8 sps_pps[1000];
	sps_pps[0] = 0x0;
	sps_pps[1] = 0x0;
	sps_pps[2] = 0x0;
	sps_pps[3] = 0x1;
	
	int len = base64_decode(sps, sps_pps+4, sizeof(sps_pps)-4);
	if (len <= 0)
	{
		return;
	}

	sys_os_mutex_enter(m_pMutex);
	
	if (m_pVideoCB)
	{
		m_pVideoCB(sps_pps, len+4, 0, 0, m_pUserdata);
	}
	
	if (pps[0] != '\0')
	{		
		len = base64_decode(pps, sps_pps+4, sizeof(sps_pps)-4);
		if (len > 0)
		{
			if (m_pVideoCB)
			{
				m_pVideoCB(sps_pps, len+4, 0, 0, m_pUserdata);
			}
		}
	}

	sys_os_mutex_leave(m_pMutex);
}

void CRtspClient::rtsp_send_h265_params(RCUA * p_rua)
{
	int pt;
	char vps[512] = {'\0'}, sps[512] = {'\0'}, pps[512] = {'\0'};
			
	if (!rua_get_sdp_h265_params(p_rua, &pt, &h265rxi.rxf_don, vps, sizeof(vps)-1, sps, sizeof(sps)-1, pps, sizeof(pps)-1))
	{
		return;
	}

	uint8 buff[1024];
	buff[0] = 0x0;
	buff[1] = 0x0;
	buff[2] = 0x0;
	buff[3] = 0x1;

	sys_os_mutex_enter(m_pMutex);
	
	if (vps[0] != '\0')
	{
		int len = base64_decode(vps, buff+4, sizeof(buff)-4);
		if (len <= 0)
		{
			return;
		}

		if (m_pVideoCB)
		{
			m_pVideoCB(buff, len+4, 0, 0, m_pUserdata);
		}
	}

	if (sps[0] != '\0')
	{
		int len = base64_decode(sps, buff+4, sizeof(buff)-4);
		if (len <= 0)
		{
			return;
		}

		if (m_pVideoCB)
		{
			m_pVideoCB(buff, len+4, 0, 0, m_pUserdata);
		}
	}

	if (pps[0] != '\0')
	{
		int len = base64_decode(pps, buff+4, sizeof(buff)-4);
		if (len <= 0)
		{
			return;
		}

		if (m_pVideoCB)
		{
			m_pVideoCB(buff, len+4, 0, 0, m_pUserdata);
		}
	}

	sys_os_mutex_leave(m_pMutex);
}

void CRtspClient::rtsp_get_mpeg4_config(RCUA * p_rua)
{
	int pt;
	char config[1000];
	
	if (!rua_get_sdp_mp4_params(p_rua, &pt, config, sizeof(config)-1))
	{
		return;
	}

    uint32  configLen;
	uint8 * configData = mpeg4_parse_config(config, configLen);
    if (configData)
    {				
    	mpeg4rxi.hdr_len = configLen;
    	memcpy(mpeg4rxi.p_buf, configData, configLen);

        delete[] configData;   
    }
}

void CRtspClient::rtsp_get_aac_config(RCUA * p_rua)
{
	int pt = 0;
	int sizelength = 13;
	int indexlength = 3;
	int indexdeltalength = 3;
	char config[128];
	
	if (rua_get_sdp_aac_params(p_rua, &pt, &sizelength, &indexlength, &indexdeltalength, config, sizeof(config)))
	{
		m_pAudioConfig = mpeg4_parse_config(config, m_nAudioConfigLen);
	}
	
	aacrxi.size_length = sizelength;
	aacrxi.index_length = indexlength;
	aacrxi.index_delta_length = indexdeltalength;
}


/***********************************************************************
*
* Close RCUA
*
************************************************************************/
void CRtspClient::rtsp_client_stop(RCUA * p_rua)
{
	if (p_rua->fd > 0)
	{
		HRTSP_MSG * tx_msg = rua_build_teardown(p_rua);
		if (tx_msg)
		{
			rcua_send_free_rtsp_msg(p_rua,tx_msg);
		}	
	}
}

BOOL CRtspClient::rtsp_setup_channel(RCUA * p_rua, int av_t)
{
	HRTSP_MSG * tx_msg = NULL;
	
	p_rua->state = RCS_INIT_V + av_t;

	if (p_rua->rtp_tcp)
	{
	    p_rua->channels[av_t].interleaved = av_t * 2;
	}
	else if (p_rua->rtp_mcast)
	{
		if (p_rua->channels[av_t].r_port)
        {
        	p_rua->channels[av_t].l_port = p_rua->channels[av_t].r_port;
        }
        else
        {
            p_rua->channels[av_t].l_port = p_rua->channels[av_t].r_port = rtsp_get_udp_port();
        }
	}
	else
	{
		if (rua_init_udp_connection(p_rua, av_t) == FALSE)
		{
	   		return FALSE;
	    }
	}
	
	tx_msg = rua_build_setup(p_rua, av_t);
	if (tx_msg)
	{
		rcua_send_free_rtsp_msg(p_rua, tx_msg);
	}
	
	return TRUE;		
}

BOOL CRtspClient::rtsp_get_transport_info(RCUA * p_rua, HRTSP_MSG * rx_msg, int av_t)
{
	BOOL ret = FALSE;
	
	if (p_rua->rtp_tcp)
	{
	    ret = rtsp_get_tcp_transport_info(rx_msg, &p_rua->channels[av_t].interleaved);	
	}
	else if (p_rua->rtp_mcast)
	{
	    ret = rtsp_get_mc_transport_info(rx_msg, p_rua->channels[av_t].destination, &p_rua->channels[av_t].r_port);
	}
	else
	{
	    ret = rtsp_get_udp_transport_info(rx_msg, &p_rua->channels[av_t].l_port, &p_rua->channels[av_t].r_port);
    }

    return ret;
}

BOOL CRtspClient::rtsp_options_res(RCUA * p_rua, HRTSP_MSG * rx_msg)
{
	HRTSP_MSG * tx_msg = NULL;
	
	if (rx_msg->msg_sub_type == 200)
	{
		// get supported command list
		p_rua->gp_cmd = rtsp_is_support_get_parameter_cmd(rx_msg);
		
		p_rua->cseq++;
		
		tx_msg = rua_build_describe(p_rua);
		if (tx_msg)
		{
			rcua_send_free_rtsp_msg(p_rua, tx_msg);
		}

		p_rua->state = RCS_DESCRIBE;
	}
	else if (rx_msg->msg_sub_type == 401 && !p_rua->need_auth)
	{
	    p_rua->need_auth = TRUE;
	    
	    if (rtsp_get_digest_info(rx_msg, &(p_rua->auth_info)))
		{
		    p_rua->auth_mode = 1;
			sprintf(p_rua->auth_info.auth_uri, "%s", p_rua->uri);                
		}
		else
		{
		    p_rua->auth_mode = 0;
		}

		p_rua->cseq++;
		
		tx_msg = rua_build_options(p_rua);
    	if (tx_msg)
    	{
    		rcua_send_free_rtsp_msg(p_rua, tx_msg);
    	}
	}
	else if (rx_msg->msg_sub_type == 401 && p_rua->need_auth)
	{
	    send_notify(RTSP_EVE_AUTHFAILED);
	    return FALSE;
	}
	else
	{
	    return FALSE;
	}

	return TRUE;
}

BOOL CRtspClient::rtsp_describe_res(RCUA * p_rua, HRTSP_MSG * rx_msg)
{
	HRTSP_MSG * tx_msg = NULL;
	
	if (rx_msg->msg_sub_type == 200)
	{
		// Session
		rtsp_get_session_info(rx_msg, p_rua->sid, sizeof(p_rua->sid)-1, &p_rua->session_timeout);

		char cseq_buf[32];
		char cbase[256];

		rtsp_get_msg_cseq(rx_msg, cseq_buf, sizeof(cseq_buf)-1);

		// Content-Base			
		if (rtsp_get_cbase_info(rx_msg, cbase, sizeof(cbase)-1))
		{
			strncpy(p_rua->uri, cbase, sizeof(p_rua->uri)-1);
		}

		rtsp_find_sdp_control(rx_msg, p_rua->channels[AV_VIDEO_CH].ctl, "video", sizeof(p_rua->channels[AV_VIDEO_CH].ctl)-1);

#ifdef BACKCHANNEL
		if (p_rua->backchannel)
		{
			p_rua->backchannel = rtsp_find_sdp_control(rx_msg, p_rua->channels[AV_BACK_CH].ctl, "audio", sizeof(p_rua->channels[AV_BACK_CH].ctl)-1, "sendonly");
			if (p_rua->backchannel)
			{
				rtsp_find_sdp_control(rx_msg, p_rua->channels[AV_AUDIO_CH].ctl, "audio", sizeof(p_rua->channels[AV_AUDIO_CH].ctl)-1, "recvonly");
			}
			else
			{
				rtsp_find_sdp_control(rx_msg, p_rua->channels[AV_AUDIO_CH].ctl, "audio", sizeof(p_rua->channels[AV_AUDIO_CH].ctl)-1);
			}
		}
		else
		{
			rtsp_find_sdp_control(rx_msg, p_rua->channels[AV_AUDIO_CH].ctl, "audio", sizeof(p_rua->channels[AV_AUDIO_CH].ctl)-1);
		}
#else
		rtsp_find_sdp_control(rx_msg, p_rua->channels[AV_AUDIO_CH].ctl, "audio", sizeof(p_rua->channels[AV_AUDIO_CH].ctl)-1);
#endif

#ifdef METADATA
		rtsp_find_sdp_control(rx_msg, p_rua->channels[AV_METADATA_CH].ctl, "application", sizeof(p_rua->channels[AV_METADATA_CH].ctl)-1);
#endif

		if (rua_get_media_info(p_rua, rx_msg))
		{
			rtsp_get_video_media_info();
			rtsp_get_audio_media_info();

#ifdef BACKCHANNEL
			if (p_rua->backchannel)
			{
				rtsp_get_bc_media_info();
			}
#endif
		}

		if (p_rua->mast_flag)
		{
			p_rua->rtp_mcast = rtsp_is_support_mcast(rx_msg);
		}

        p_rua->cseq++;
        
		// Send SETUP
		if (p_rua->channels[AV_VIDEO_CH].ctl[0] != '\0')
		{
			if (!rtsp_setup_channel(p_rua, AV_VIDEO_CH))
			{
				return FALSE;
			}
		}
		else if (p_rua->channels[AV_AUDIO_CH].ctl[0] != '\0')
		{
			if (!rtsp_setup_channel(p_rua, AV_AUDIO_CH))
			{
				return FALSE;
			}
		}
		else
		{
			return FALSE;
		}
	}	
	else if (rx_msg->msg_sub_type == 401 && !p_rua->need_auth)
	{
	    p_rua->need_auth = TRUE;
	    
	    if (rtsp_get_digest_info(rx_msg, &(p_rua->auth_info)))
		{
		    p_rua->auth_mode = 1;
			sprintf(p_rua->auth_info.auth_uri, "%s", p_rua->uri);                
		}
		else
		{
		    p_rua->auth_mode = 0;
		}

		p_rua->cseq++;
		
		tx_msg = rua_build_describe(p_rua);
    	if (tx_msg)
    	{
    		rcua_send_free_rtsp_msg(p_rua, tx_msg);
    	}
	}
	else if (rx_msg->msg_sub_type == 401 && p_rua->need_auth)
	{
	    send_notify(RTSP_EVE_AUTHFAILED);
	    return FALSE;
	}
#ifdef BACKCHANNEL	
	else if (p_rua->backchannel) // the server don't support backchannel
	{
		p_rua->backchannel = 0;

		p_rua->cseq++;
		
		tx_msg = rua_build_describe(p_rua);
		if (tx_msg)
		{
			rcua_send_free_rtsp_msg(p_rua, tx_msg);
		}

		p_rua->state = RCS_DESCRIBE;		
	}
#endif
    else
    {
        return FALSE;
    }
    
	return TRUE;
}

BOOL CRtspClient::rtsp_setup_video_res(RCUA * p_rua, HRTSP_MSG * rx_msg)
{
	HRTSP_MSG * tx_msg = NULL;
	
	if (rx_msg->msg_sub_type == 200)
	{
		char cbase[256];
		
		// Content-Base			
		if (rtsp_get_cbase_info(rx_msg, cbase, sizeof(cbase)-1))
		{
			strncpy(p_rua->uri, cbase, sizeof(p_rua->uri)-1);
			sprintf(p_rua->auth_info.auth_uri, "%s", p_rua->uri);
		}
		
		// Session
		if (p_rua->sid[0] == '\0')
		{
			rtsp_get_session_info(rx_msg, p_rua->sid, sizeof(p_rua->sid)-1, &p_rua->session_timeout);
        }

		if (!rtsp_get_transport_info(p_rua, rx_msg, AV_TYPE_VIDEO))
		{
			return FALSE;
		}
		
        if (p_rua->rtp_mcast)
		{
			if (rua_init_mc_connection(p_rua, AV_TYPE_VIDEO) == FALSE)
			{
		   		return FALSE;
		    }
		}
        
		if (p_rua->sid[0] == '\0')
		{
			sprintf(p_rua->sid, "%x%x", rand(), rand());
		}
		
		p_rua->cseq++;		

		if (p_rua->channels[AV_AUDIO_CH].ctl[0] != '\0')
		{
			if (!rtsp_setup_channel(p_rua, AV_AUDIO_CH))
			{
				return FALSE;
			}
		}
#ifdef METADATA		
		else if (p_rua->channels[AV_METADATA_CH].ctl[0] != '\0')
		{
			if (!rtsp_setup_channel(p_rua, AV_METADATA_CH))
			{
				return FALSE;
			}
		}
#endif		
#ifdef BACKCHANNEL
		else if (p_rua->channels[AV_BACK_CH].ctl[0] != '\0')
		{
			if (!rtsp_setup_channel(p_rua, AV_BACK_CH))
			{
				return FALSE;
			}
		}
#endif		
		else
		{
		    if (make_prepare_play() == FALSE)
		    {
		        return FALSE;
		    }
		    
			// only video without audio
			tx_msg = rua_build_play(p_rua);
			if (tx_msg)
			{
				rcua_send_free_rtsp_msg(p_rua,tx_msg);
			}
			
			p_rua->state = RCS_READY;
		}
	}
	else if (p_rua->rtp_tcp) // maybe the server don't support rtp over tcp, try rtp over udp
	{
		p_rua->rtp_tcp = 0;

        p_rua->cseq++;
        
	    if (!rtsp_setup_channel(p_rua, AV_VIDEO_CH))
		{
			return FALSE;
		}
	}
	else
	{
	    return FALSE;
	}

	return TRUE;
}

BOOL CRtspClient::rtsp_setup_audio_res(RCUA * p_rua, HRTSP_MSG * rx_msg)
{
	HRTSP_MSG * tx_msg = NULL;
	
	if (rx_msg->msg_sub_type == 200)
	{
		// Session
		if (p_rua->sid[0] == '\0')
		{
			rtsp_get_session_info(rx_msg, p_rua->sid, sizeof(p_rua->sid)-1, &p_rua->session_timeout);
        }
        
        if (p_rua->sid[0] == '\0')
		{
			sprintf(p_rua->sid, "%x%x", rand(), rand());
		}

		if (!rtsp_get_transport_info(p_rua, rx_msg, AV_TYPE_AUDIO))
		{
			return FALSE;
		}
		
        if (p_rua->rtp_mcast)
		{
			if (rua_init_mc_connection(p_rua, AV_TYPE_AUDIO) == FALSE)
			{
		   		return FALSE;
		    }
		}

		p_rua->cseq++;

#ifdef METADATA
		if (p_rua->channels[AV_METADATA_CH].ctl[0] != '\0')
		{
			if (!rtsp_setup_channel(p_rua, AV_METADATA_CH))
			{
				return FALSE;
			}
		}
		else 
#endif		
#ifdef BACKCHANNEL
		if (p_rua->channels[AV_BACK_CH].ctl[0] != '\0')
		{
			if (!rtsp_setup_channel(p_rua, AV_BACK_CH))
			{
				return FALSE;
			}
		}
		else
#endif	
		{
		    if (make_prepare_play() == FALSE)
		    {
		        return FALSE;
		    }
		    
			tx_msg = rua_build_play(p_rua);
			if (tx_msg)
			{
				rcua_send_free_rtsp_msg(p_rua,tx_msg);
			}
			
			p_rua->state = RCS_READY;
		}
	}
	else if (p_rua->rtp_tcp) // maybe the server don't support rtp over tcp, try rtp over udp
	{
		p_rua->rtp_tcp = 0;

        p_rua->cseq++;
        
	    if (!rtsp_setup_channel(p_rua, AV_AUDIO_CH))
		{
			return FALSE;
		}
	}
	else
	{
		// error handle

		return FALSE;
	}

	return TRUE;
}

BOOL CRtspClient::rtsp_play_res(RCUA * p_rua, HRTSP_MSG * rx_msg)
{
	if (rx_msg->msg_sub_type == 200)
	{
		p_rua->state = RCS_PLAYING;
		p_rua->keepalive_time = sys_os_get_ms();

		log_print(HT_LOG_DBG, "%s, session timeout : %d\n", __FUNCTION__, p_rua->session_timeout);

		if (m_AudioCodec == AUDIO_CODEC_AAC)
		{
			rtsp_get_aac_config(p_rua);
		}
		
		send_notify(RTSP_EVE_CONNSUCC);

		if (m_VideoCodec == VIDEO_CODEC_H264)
		{
			rtsp_send_h264_params(p_rua);
		}	
		else if (m_VideoCodec == VIDEO_CODEC_MP4)
		{
			rtsp_get_mpeg4_config(p_rua);
		}
		else if (m_VideoCodec == VIDEO_CODEC_H265)
		{
			rtsp_send_h265_params(p_rua);
		}

#ifdef BACKCHANNEL
		if (p_rua->backchannel)
		{
			rtsp_init_backchannel(p_rua);
		}
#endif		
	}
	else
	{
		//error handle
		return FALSE;
	}

	return TRUE;
}

#ifdef METADATA

BOOL CRtspClient::rtsp_setup_metadata_res(RCUA * p_rua, HRTSP_MSG * rx_msg)
{
	HRTSP_MSG * tx_msg = NULL;
	
	if (rx_msg->msg_sub_type == 200)
	{
		// Session
		if (p_rua->sid[0] == '\0')
		{
			rtsp_get_session_info(rx_msg, p_rua->sid, sizeof(p_rua->sid)-1, &p_rua->session_timeout);
        }
        
        if (p_rua->sid[0] == '\0')
		{
			sprintf(p_rua->sid, "%x%x", rand(), rand());
		}

		if (!rtsp_get_transport_info(p_rua, rx_msg, AV_TYPE_METADATA))
		{
			return FALSE;
		}
		
        if (p_rua->rtp_mcast)
		{
			if (rua_init_mc_connection(p_rua, AV_TYPE_METADATA) == FALSE)
			{
		   		return FALSE;
		    }
		}

		p_rua->cseq++;		

#ifdef BACKCHANNEL
		if (p_rua->channels[AV_BACK_CH].ctl[0] != '\0')
		{
			if (!rtsp_setup_channel(p_rua, AV_BACK_CH))
			{
				return FALSE;
			}
		}
		else
#endif
        {
    	    if (make_prepare_play() == FALSE)
    	    {
    	        return FALSE;
    	    }
    	    
    		tx_msg = rua_build_play(p_rua);
    		if (tx_msg)
    		{
    			rcua_send_free_rtsp_msg(p_rua,tx_msg);
    		}
    		
    		p_rua->state = RCS_READY;
		}
	}
	else
	{
		// error handle

		return FALSE;
	}

	return TRUE;
}

void CRtspClient::set_metadata_cb(metadata_cb cb)
{
	sys_os_mutex_enter(m_pMutex);
	m_pMetadataCB = cb;
	sys_os_mutex_leave(m_pMutex);
}

void CRtspClient::metadata_rtp_rx(uint8 * lpData, int rlen, unsigned seq, unsigned ts)
{
	sys_os_mutex_enter(m_pMutex);
	
	if (m_pMetadataCB)
    {    	
        m_pMetadataCB(lpData, rlen, ts, seq, m_pUserdata);
    }

    sys_os_mutex_leave(m_pMutex);
}

#endif // METADATA

#ifdef BACKCHANNEL

int CRtspClient::get_bc_flag()
{
	return m_rua.backchannel;
}

void CRtspClient::set_bc_flag(int flag)
{
	m_rua.backchannel = flag;
}

int CRtspClient::get_bc_data_flag()
{
	if (m_rua.backchannel)
	{
		return m_rua.send_bc_data;
	}

	return -1; // unsupport backchannel
}

void CRtspClient::set_bc_data_flag(int flag)
{
	m_rua.send_bc_data = flag;
}

BOOL CRtspClient::rtsp_setup_backchannel_res(RCUA * p_rua, HRTSP_MSG * rx_msg)
{
	HRTSP_MSG * tx_msg = NULL;
	
	if (rx_msg->msg_sub_type == 200)
	{
		// Session
		if (p_rua->sid[0] == '\0')
		{
			rtsp_get_session_info(rx_msg, p_rua->sid, sizeof(p_rua->sid)-1, &p_rua->session_timeout);
        }

        if (p_rua->sid[0] == '\0')
		{
			sprintf(p_rua->sid, "%x%x", rand(), rand());
		}

		if (!rtsp_get_transport_info(p_rua, rx_msg, AV_TYPE_BACKCHANNEL))
		{
			return FALSE;
		}
		
        if (p_rua->rtp_mcast)
		{
			if (rua_init_mc_connection(p_rua, AV_TYPE_BACKCHANNEL) == FALSE)
			{
		   		return FALSE;
		    }
		}
		
		p_rua->bc_rtp_info.rtp_cnt = 0;
		p_rua->bc_rtp_info.rtp_pt = p_rua->channels[AV_BACK_CH].cap[0];
		p_rua->bc_rtp_info.rtp_ssrc = rand();
		
		p_rua->cseq++;

		if (make_prepare_play() == FALSE)
	    {
	        return FALSE;
	    }
		
		tx_msg = rua_build_play(p_rua);
		if (tx_msg)
		{
			rcua_send_free_rtsp_msg(p_rua, tx_msg);
		}
		
		p_rua->state = RCS_READY;
	}
	else
	{
		// error handle

		return FALSE;
	}

	return TRUE;
}

BOOL CRtspClient::rtsp_get_bc_media_info()
{
	if (m_rua.channels[AV_BACK_CH].cap_count == 0)
	{
		return FALSE;
	}

	if (m_rua.channels[AV_BACK_CH].cap[0] == 0)
	{
		m_bcAudioCodec = AUDIO_CODEC_G711U;
	}
	else if (m_rua.channels[AV_BACK_CH].cap[0] == 8)
	{
		m_bcAudioCodec = AUDIO_CODEC_G711A;
	}
	else if (m_rua.channels[AV_BACK_CH].cap[0] == 9)
	{
		m_bcAudioCodec = AUDIO_CODEC_G722;
	}

	int i;
	int rtpmap_len = strlen("a=rtpmap:");

	for (i=0; i<MAX_AVN; i++)
	{
		char * ptr = m_rua.channels[AV_BACK_CH].cap_desc[i];
		
		if (memcmp(ptr, "a=rtpmap:", rtpmap_len) == 0)
		{
			char pt_buf[16];
			char code_buf[64];
			int next_offset = 0;

			ptr += rtpmap_len;

			if (GetLineWord(ptr, 0, strlen(ptr), pt_buf, sizeof(pt_buf), &next_offset, WORD_TYPE_NUM) == FALSE)
			{
				return FALSE;
			}
			
			GetLineWord(ptr, next_offset, strlen(ptr)-next_offset, code_buf, sizeof(code_buf),  &next_offset, WORD_TYPE_STRING);

			uppercase(code_buf);

			if (strstr(code_buf, "G726-32"))
			{
				m_bcAudioCodec = AUDIO_CODEC_G726;
			}
			else if (strstr(code_buf, "G722"))
			{
				m_bcAudioCodec = AUDIO_CODEC_G722;
			}
			else if (strstr(code_buf, "PCMU"))
			{
				m_bcAudioCodec = AUDIO_CODEC_G711U;
			}
			else if (strstr(code_buf, "PCMA"))
			{
				m_bcAudioCodec = AUDIO_CODEC_G711A;
			}
			else if (strstr(code_buf, "MPEG4-GENERIC"))
			{
				m_bcAudioCodec = AUDIO_CODEC_AAC;
			}
			else if (strstr(code_buf, "OPUS"))
			{
				m_bcAudioCodec = AUDIO_CODEC_OPUS;
			}

			char * p = strchr(code_buf, '/');
			if (p)
			{
				p++;
				
				char * p1 = strchr(p, '/');
				if (p1)
				{
					*p1 = '\0';
					m_nbcSamplerate = atoi(p);

					p1++;
					if (p1 && *p1 != '\0')
					{
						m_nbcChannels = atoi(p1);
					}
					else
					{
						m_nbcChannels = 1;
					}
				}
				else
				{
					m_nbcSamplerate = atoi(p);
					m_nbcChannels = 1;
				}
			}

			break;
		}
	}

    if (m_bcAudioCodec == AUDIO_CODEC_G722)
    {
        m_nbcSamplerate = 16000;
        m_nbcChannels = 1;
    }
    
	return TRUE;
}

BOOL CRtspClient::rtsp_init_backchannel(RCUA * p_rua)
{
#if __WINDOWS_OS__
	if (CDSoundAudioCapture::getDeviceNums() <= 0)
	{
		return FALSE;
	}
	
	p_rua->audio_captrue = CDSoundAudioCapture::getInstance(0);
	if (NULL == p_rua->audio_captrue)
	{
		return FALSE;
	}
	
	p_rua->audio_captrue->addCallback(rtsp_bc_cb, p_rua);
	p_rua->audio_captrue->initCapture(m_bcAudioCodec, m_nbcSamplerate, m_nbcChannels, 0);
	
	return p_rua->audio_captrue->startCapture();
#elif defined(ANDROID)
    p_rua->audio_input = new AudioInput;
	if (NULL == p_rua->audio_input)
	{
		return FALSE;
	}

	p_rua->audio_input->addCallback(rtsp_bc_cb, p_rua);
	p_rua->audio_input->init(m_bcAudioCodec, m_nbcSamplerate, m_nbcChannels);
	
	return p_rua->audio_input->start();
#else
	return FALSE;
#endif
}

#endif // BACKCHANNEL

#ifdef REPLAY

int CRtspClient::get_replay_flag()
{
    return m_rua.replay;
}

void CRtspClient::set_replay_flag(int flag)
{
    m_rua.replay = flag;
}

void CRtspClient::set_scale(double scale)
{
    if (scale != 0)
    {
        m_rua.scale_flag = 1;
        m_rua.scale = scale * 100;
    }
    else
    {
        m_rua.scale_flag = 0;
        m_rua.scale = 0;
    }
}

void CRtspClient::set_rate_control_flag(int flag)
{
    m_rua.rate_control_flag = 1;
    m_rua.rate_control = flag;
}

void CRtspClient::set_immediate_flag(int flag)
{
    m_rua.immediate_flag = 1;
    m_rua.immediate = flag;
}

void CRtspClient::set_frames_flag(int flag, int interval)
{
    if (flag == 1 || flag == 2)
    {
        m_rua.frame_flag = 1;
        m_rua.frame = flag;
    }
    else
    {
        m_rua.frame_flag = 0;
        m_rua.frame = 0;
    }

    if (interval > 0)
    {
        m_rua.frame_interval_flag = 1;
        m_rua.frame_interval = interval;
    }
    else
    {
        m_rua.frame_interval_flag = 0;
        m_rua.frame_interval = 0;
    }
}

void CRtspClient::set_replay_range(time_t start, time_t end)
{
	m_rua.range_flag = 1;
	m_rua.replay_start = start;
	m_rua.replay_end = end;
}

#endif // REPLAY

#ifdef OVER_HTTP

void CRtspClient::set_rtsp_over_http(int flag, int port)
{
    if (flag)
    {
        m_rua.mast_flag = 0;
        m_rua.rtp_tcp = 1;
    }
    
    m_rua.over_http = flag;
    m_rua.http_port = port;
}

int CRtspClient::rtsp_build_http_get_req(void * p_user, char * bufs, char * cookie)
{
    int offset = 0;
    HTTPREQ * p_req = (HTTPREQ *) p_user;
    
    offset += sprintf(bufs+offset, "GET %s HTTP/1.1\r\n", p_req->url);
	offset += sprintf(bufs+offset, "Host: %s:%d\r\n", p_req->host, p_req->port);
	offset += sprintf(bufs+offset, "User-Agent: Happytimesoft rtsp client\r\n");
	offset += sprintf(bufs+offset, "x-sessioncookie: %s\r\n", cookie);
	offset += sprintf(bufs+offset, "Accept: application/x-rtsp-tunnelled\r\n");
	offset += sprintf(bufs+offset, "Pragma: no-cache\r\n");
	offset += sprintf(bufs+offset, "Cache-Control: no-cache\r\n");

	if (p_req->need_auth)
    {
    	offset += http_build_auth_msg(p_req, "GET", bufs+offset);        
	}
	
	offset += sprintf(bufs+offset, "\r\n");

    log_print(HT_LOG_DBG, "TX >> %s\r\n\r\n", bufs);

    return offset;
}

int CRtspClient::rtsp_build_http_post_req(void * p_user, char * bufs, char * cookie)
{
    int offset = 0;
    HTTPREQ * p_req = (HTTPREQ *) p_user;
    
    offset += sprintf(bufs+offset, "POST %s HTTP/1.1\r\n", p_req->url);
	offset += sprintf(bufs+offset, "Host: %s:%d\r\n", p_req->host, p_req->port);
	offset += sprintf(bufs+offset, "User-Agent: Happytimesoft rtsp client\r\n");
	offset += sprintf(bufs+offset, "x-sessioncookie: %s\r\n", cookie);
	offset += sprintf(bufs+offset, "Content-Type: application/x-rtsp-tunnelled\r\n");
	offset += sprintf(bufs+offset, "Pragma: no-cache\r\n");
	offset += sprintf(bufs+offset, "Cache-Control: no-cache\r\n");
	offset += sprintf(bufs+offset, "Content-Length: 32767\r\n");

	if (p_req->need_auth)
    {
    	offset += http_build_auth_msg(p_req, "POST", bufs+offset);        
	}
	
	offset += sprintf(bufs+offset, "\r\n");

    log_print(HT_LOG_DBG, "TX >> %s\r\n\r\n", bufs);

    return offset;
}

BOOL CRtspClient::rtsp_over_http_start()
{
    int offset = 0;
    char buff[2048];
    char cookie[100];
    HTTPREQ * p_http;
    static int cnt = 1;

    srand((uint32)time(NULL)+cnt++);
    sprintf(cookie, "%x%x%x", rand(), rand(), sys_os_get_ms());

RETRY:

    p_http = &m_rua.rtsp_recv;

    p_http->port = m_nport;
    strcpy(p_http->host, m_ip);
    strcpy(p_http->url, m_suffix);

    // First try the stream address port, 
    // If the connection fails, try to connect to the configured port
    
    p_http->cfd = tcp_connect_timeout(get_address_by_name(p_http->host), p_http->port, 5*1000);
	if (p_http->cfd <= 0)
	{
	    if (p_http->port == m_rua.http_port)
	    {
	        log_print(HT_LOG_ERR, "%s, tcp_connect_timeout failed\r\n", __FUNCTION__);
    	    return FALSE;
	    }
	    
	    p_http->port = m_rua.http_port;
	    p_http->cfd = tcp_connect_timeout(get_address_by_name(p_http->host), p_http->port, 5*1000);
	    if (p_http->cfd <= 0)
	    {
    	    log_print(HT_LOG_ERR, "%s, tcp_connect_timeout failed\r\n", __FUNCTION__);
    	    return FALSE;
	    }
	}

    offset = rtsp_build_http_get_req(p_http, buff, cookie);
    
	if (!http_cln_tx(p_http, buff, offset))
	{
	    log_print(HT_LOG_ERR, "%s, http_cln_tx failed\r\n", __FUNCTION__);
	    return FALSE;
	}

	if (!http_cln_rx_timeout(p_http, 10*1000))
	{
	    log_print(HT_LOG_ERR, "%s, http_cln_rx_timeout failed\r\n", __FUNCTION__);
	    return FALSE;
	}

    if (p_http->rx_msg->msg_sub_type == 401)
    {
        if (p_http->need_auth == FALSE)
        {
            if (http_get_digest_info(p_http->rx_msg, &p_http->auth_info))
			{
				http_cln_free_req(p_http);
				
				p_http->auth_mode = 1;
				strcpy(p_http->auth_info.auth_uri, p_http->url);
				strcpy(p_http->auth_info.auth_name, m_rua.auth_info.auth_name);
				strcpy(p_http->auth_info.auth_pwd, m_rua.auth_info.auth_pwd);			
			}
			else
			{
				http_cln_free_req(p_http);
				
				p_http->auth_mode = 0;
                strcpy(p_http->user, m_rua.auth_info.auth_name);
                strcpy(p_http->pass, m_rua.auth_info.auth_pwd);
			}
			
            p_http->need_auth = TRUE;

            goto RETRY;
        }

        send_notify(RTSP_EVE_AUTHFAILED);
        return FALSE;
    }
    else if (p_http->rx_msg->msg_sub_type != 200 || p_http->rx_msg->ctt_type != CTT_RTSP_TUNNELLED)
    {
        log_print(HT_LOG_ERR, "%s, msg_sub_type=%d, ctt_type=%d\r\n", 
            __FUNCTION__, p_http->rx_msg->msg_sub_type, p_http->rx_msg->ctt_type);
        return FALSE;
    }

    p_http = &m_rua.rtsp_send;

    p_http->port =  m_rua.rtsp_recv.port;
    strcpy(p_http->host, m_ip);
    strcpy(p_http->url, m_suffix);

	offset = rtsp_build_http_post_req(p_http, buff, cookie);

	p_http->cfd = tcp_connect_timeout(get_address_by_name(p_http->host), p_http->port, 5*1000);
	if (p_http->cfd <= 0)
	{
	    log_print(HT_LOG_ERR, "%s, tcp_connect_timeout failed\r\n", __FUNCTION__);
	    return FALSE;
	}
	
	if (!http_cln_tx(p_http, buff, offset))
	{
	    log_print(HT_LOG_ERR, "%s, http_cln_tx failed\r\n", __FUNCTION__);
	    return FALSE;
	}

    m_rua.cseq = 1;
    m_rua.rtp_tcp = 1;
    strcpy(m_rua.ripstr, m_ip);
    strcpy(m_rua.uri, m_url);
    
	HRTSP_MSG * tx_msg = rua_build_options(&m_rua);
	if (tx_msg)
	{
		rcua_send_free_rtsp_msg(&m_rua, tx_msg);
	}

	m_rua.state = RCS_OPTIONS;

	return TRUE;
}

#endif // OVER_HTTP

BOOL CRtspClient::rtsp_client_state(RCUA * p_rua, HRTSP_MSG * rx_msg)
{
    BOOL ret = TRUE;
	HRTSP_MSG * tx_msg = NULL;

	if (rx_msg->msg_type == 0)	// Request message?
	{
		return FALSE;
	}

	switch (p_rua->state)
	{
	case RCS_NULL:
		break;

	case RCS_OPTIONS:
		ret = rtsp_options_res(p_rua, rx_msg);
		break;
		
	case RCS_DESCRIBE:
		ret = rtsp_describe_res(p_rua, rx_msg);
		break;

	case RCS_INIT_V:
		ret = rtsp_setup_video_res(p_rua, rx_msg);
		break;

	case RCS_INIT_A:
		ret = rtsp_setup_audio_res(p_rua, rx_msg);
		break;

#ifdef METADATA
	case RCS_INIT_M:
		ret = rtsp_setup_metadata_res(p_rua, rx_msg);
		break;
#endif

#ifdef BACKCHANNEL
	case RCS_INIT_BC:
		ret = rtsp_setup_backchannel_res(p_rua, rx_msg);
		break;
#endif

	case RCS_READY:
		ret = rtsp_play_res(p_rua, rx_msg);
		break;

	case RCS_PLAYING:
		break;

	case RCS_RECORDING:
		break;
	}		

	return ret;
}

BOOL CRtspClient::make_prepare_play()
{    
	if (m_rua.channels[AV_VIDEO_CH].ctl[0] != '\0')
	{
		if (m_VideoCodec == VIDEO_CODEC_H264)
		{
			h264_rxi_init(&h264rxi, video_data_cb, this);
		}
		else if (m_VideoCodec == VIDEO_CODEC_H265)
		{
			h265_rxi_init(&h265rxi, video_data_cb, this);
		}
		else if (m_VideoCodec == VIDEO_CODEC_JPEG)
		{
			mjpeg_rxi_init(&mjpegrxi, video_data_cb, this);
		}
		else if (m_VideoCodec == VIDEO_CODEC_MP4)
		{
			mpeg4_rxi_init(&mpeg4rxi, video_data_cb, this);
		}
	}
	
    if (m_rua.channels[AV_AUDIO_CH].ctl[0] != '\0')
    {
		if (m_AudioCodec == AUDIO_CODEC_AAC)
		{
			aac_rxi_init(&aacrxi, audio_data_cb, this);
		}
		else if (AUDIO_CODEC_NONE != m_AudioCodec)
		{
			pcm_rxi_init(&pcmrxi, audio_data_cb, this);
		}
    }

	if (!m_rua.rtp_tcp)
	{
    	m_udpRxTid = sys_os_create_thread((void *)rtsp_udp_rx_thread, this);
    }

    return TRUE;
}

void CRtspClient::tcp_data_rx(uint8 * lpData, int rlen)
{
	RILF * p_rilf = (RILF *)lpData;
	uint8 * p_rtp = (uint8 *)p_rilf + 4;
	uint32 rtp_len = rlen - 4;

	if (rtp_len >= 2 && RTP_PT_IS_RTCP(p_rtp[1])) // now, don't handle rtcp packet ...
	{
		return;
	}
	
	if (p_rilf->channel == m_rua.channels[AV_VIDEO_CH].interleaved)
	{
		if (VIDEO_CODEC_H264 == m_VideoCodec)
		{
			h264_rtp_rx(&h264rxi, p_rtp, rtp_len);
		}
		else if (VIDEO_CODEC_JPEG == m_VideoCodec)
		{
			mjpeg_rtp_rx(&mjpegrxi, p_rtp, rtp_len);
		}
		else if (VIDEO_CODEC_MP4 == m_VideoCodec)
		{
			mpeg4_rtp_rx(&mpeg4rxi, p_rtp, rtp_len);
		}
		else if (VIDEO_CODEC_H265 == m_VideoCodec)
		{
			h265_rtp_rx(&h265rxi, p_rtp, rtp_len);
		}
	}
	else if (p_rilf->channel == m_rua.channels[AV_AUDIO_CH].interleaved)
	{
		if (AUDIO_CODEC_AAC == m_AudioCodec)
		{
			aac_rtp_rx(&aacrxi, p_rtp, rtp_len);
		}
		else if (AUDIO_CODEC_NONE != m_AudioCodec)
		{
			pcm_rtp_rx(&pcmrxi, p_rtp, rtp_len);
		}
	}
#ifdef METADATA	
	else if (p_rilf->channel == m_rua.channels[AV_METADATA_CH].interleaved)
	{
		if (rtp_data_rx(&rtprxi, p_rtp, rtp_len))
		{
			metadata_rtp_rx(rtprxi.p_data, rtprxi.len, rtprxi.prev_ts, rtprxi.prev_seq);
		}
	}
#endif	
}

void CRtspClient::udp_data_rx(uint8 * lpData, int rlen, int type)
{
	uint8 * p_rtp = lpData;
	uint32 rtp_len = rlen;

	if (rtp_len >= 2 && RTP_PT_IS_RTCP(p_rtp[1])) // now, don't handle rtcp packet ...
	{
		return;
	}
	
	if (AV_TYPE_VIDEO == type)
	{
		if (VIDEO_CODEC_H264 == m_VideoCodec)
		{
			h264_rtp_rx(&h264rxi, p_rtp, rtp_len);
		}
		else if (VIDEO_CODEC_JPEG == m_VideoCodec)
		{
			mjpeg_rtp_rx(&mjpegrxi, p_rtp, rtp_len);
		}
		else if (VIDEO_CODEC_MP4 == m_VideoCodec)
		{
			mpeg4_rtp_rx(&mpeg4rxi, p_rtp, rtp_len);
		}
		else if (VIDEO_CODEC_H265 == m_VideoCodec)
		{
			h265_rtp_rx(&h265rxi, p_rtp, rtp_len);
		}
	}
	else if (AV_TYPE_AUDIO == type)
	{
		if (AUDIO_CODEC_AAC == m_AudioCodec)
		{
			aac_rtp_rx(&aacrxi, p_rtp, rtp_len);
		}
		else if (AUDIO_CODEC_NONE != m_AudioCodec)
		{
			pcm_rtp_rx(&pcmrxi, p_rtp, rtp_len);
		}
	}
#ifdef METADATA	
	else if (AV_TYPE_METADATA == type)
	{
		if (rtp_data_rx(&rtprxi, p_rtp, rtp_len))
		{
			metadata_rtp_rx(rtprxi.p_data, rtprxi.len, rtprxi.prev_ts, rtprxi.prev_seq);
		}
	}
#endif	
}

int CRtspClient::rtsp_msg_parser(RCUA * p_rua)
{
	int rtsp_pkt_len = rtsp_pkt_find_end(p_rua->rcv_buf);
	if (rtsp_pkt_len == 0) // wait for next recv
	{
		return RTSP_PARSE_MOREDATE;
	}
	
	HRTSP_MSG * rx_msg = rtsp_get_msg_buf();
	if (rx_msg == NULL)
	{
		log_print(HT_LOG_ERR, "%s, rtsp_get_msg_buf return null!!!\r\n", __FUNCTION__);
		return RTSP_PARSE_FAIL;
	}
	
	memcpy(rx_msg->msg_buf, p_rua->rcv_buf, rtsp_pkt_len);
	rx_msg->msg_buf[rtsp_pkt_len] = '\0';

	log_print(HT_LOG_DBG, "RX << %s\r\n", rx_msg->msg_buf);

	int parse_len = rtsp_msg_parse_part1(rx_msg->msg_buf, rtsp_pkt_len, rx_msg);
	if (parse_len != rtsp_pkt_len)	//parse error
	{
		log_print(HT_LOG_ERR, "%s, rtsp_msg_parse_part1=%d, rtsp_pkt_len=%d!!!\r\n", __FUNCTION__, parse_len, rtsp_pkt_len);
		rtsp_free_msg(rx_msg);

		p_rua->rcv_dlen = 0;
		return RTSP_PARSE_FAIL;
	}
	
	if (rx_msg->ctx_len > 0)	
	{
		if (p_rua->rcv_dlen < (parse_len + rx_msg->ctx_len))
		{
			rtsp_free_msg(rx_msg);
			return RTSP_PARSE_MOREDATE;
		}

		memcpy(rx_msg->msg_buf+rtsp_pkt_len, p_rua->rcv_buf+rtsp_pkt_len, rx_msg->ctx_len);
        rx_msg->msg_buf[rtsp_pkt_len+rx_msg->ctx_len] = '\0';

        log_print(HT_LOG_DBG, "%s\r\n", rx_msg->msg_buf+rtsp_pkt_len);
        
		int sdp_parse_len = rtsp_msg_parse_part2(rx_msg->msg_buf+rtsp_pkt_len, rx_msg->ctx_len, rx_msg);
		if (sdp_parse_len != rx_msg->ctx_len)
		{
		}
		parse_len += rx_msg->ctx_len;
	}
	
	if (parse_len < p_rua->rcv_dlen)
	{
		while (p_rua->rcv_buf[parse_len] == ' ' || p_rua->rcv_buf[parse_len] == '\r' || p_rua->rcv_buf[parse_len] == '\n')
		{
			parse_len++;
		}
		
		memmove(p_rua->rcv_buf, p_rua->rcv_buf + parse_len, p_rua->rcv_dlen - parse_len);
		p_rua->rcv_dlen -= parse_len;
	}
	else
	{
		p_rua->rcv_dlen = 0;
	}
	
	int ret = rtsp_client_state(p_rua, rx_msg);
	
	rtsp_free_msg(rx_msg);

	return ret ? RTSP_PARSE_SUCC : RTSP_PARSE_FAIL;
}

int CRtspClient::rtsp_tcp_rx()
{
	RCUA * p_rua = &(this->m_rua);
    SOCKET fd = -1;

#ifdef OVER_HTTP    
    if (p_rua->over_http)
    {
        fd = p_rua->rtsp_recv.cfd;
    }
    else 
#endif
    fd = p_rua->fd;
    
	if (fd <= 0)
	{
		return -1;
	}
	
    int sret;
    fd_set fdread;
    struct timeval tv = {0, 100*1000};

    FD_ZERO(&fdread);
    FD_SET(fd, &fdread); 
    
    sret = select((int)(fd+1), &fdread, NULL, NULL, &tv); 
    if (sret == 0) // Time expired 
    { 
        return RTSP_RX_TIMEOUT; 
    }
    else if (!FD_ISSET(fd, &fdread))
    {
        return RTSP_RX_TIMEOUT;
    }
    
	if (p_rua->rtp_rcv_buf == NULL || p_rua->rtp_t_len == 0)
	{
		int rlen = recv(fd, p_rua->rcv_buf+p_rua->rcv_dlen, 2048-p_rua->rcv_dlen, 0);
		if (rlen <= 0)
		{
			log_print(HT_LOG_WARN, "%s, thread exit, ret = %d, err = %s\r\n", __FUNCTION__, rlen, sys_os_get_socket_error());	//recv error, connection maybe disconn?
			return RTSP_RX_FAIL;
		}

		p_rua->rcv_dlen += rlen;

		if (p_rua->rcv_dlen < 16)
		{
			return RTSP_RX_SUCC;
		}
	}
	else
	{
		int rlen = recv(fd, p_rua->rtp_rcv_buf+p_rua->rtp_rcv_len, p_rua->rtp_t_len-p_rua->rtp_rcv_len, 0);
		if (rlen <= 0)
		{
			log_print(HT_LOG_WARN, "%s, thread exit, ret = %d, err = %s\r\n", __FUNCTION__, rlen, sys_os_get_socket_error());	//recv error, connection maybe disconn?
			return RTSP_RX_FAIL;
		}

		p_rua->rtp_rcv_len += rlen;
		if (p_rua->rtp_rcv_len == p_rua->rtp_t_len)
		{
			tcp_data_rx((uint8*)p_rua->rtp_rcv_buf, p_rua->rtp_rcv_len);
			
			free(p_rua->rtp_rcv_buf);
			p_rua->rtp_rcv_buf = NULL;
			p_rua->rtp_rcv_len = 0;
			p_rua->rtp_t_len = 0;
		}
		
		return RTSP_RX_SUCC;
	}

rx_point:

	if (rtsp_is_rtsp_msg(p_rua->rcv_buf))	//Is RTSP Packet?
	{
		int ret = rtsp_msg_parser(p_rua);
		if (ret == RTSP_PARSE_FAIL)
		{
			return RTSP_RX_FAIL;
		}
		else if (ret == RTSP_PARSE_MOREDATE)
		{
			return RTSP_RX_SUCC;
		}

		if (p_rua->rcv_dlen >= 16)
		{
			goto rx_point;
		}
	}
	else
	{
		RILF * p_rilf = (RILF *)(p_rua->rcv_buf);
		if (p_rilf->magic != 0x24)
		{		
			log_print(HT_LOG_WARN, "%s, p_rilf->magic[0x%02X]!!!\r\n", __FUNCTION__, p_rilf->magic);
			return RTSP_RX_FAIL;
		}
		
		uint16 rtp_len = ntohs(p_rilf->rtp_len);
		if (rtp_len > (p_rua->rcv_dlen - 4))
		{
			if (p_rua->rtp_rcv_buf)
			{
				free(p_rua->rtp_rcv_buf);
			}
			
			p_rua->rtp_rcv_buf = (char *)malloc(rtp_len+4);
			if (p_rua->rtp_rcv_buf == NULL) 
			{
			    return RTSP_RX_FAIL;
			}
			
			memcpy(p_rua->rtp_rcv_buf, p_rua->rcv_buf, p_rua->rcv_dlen);
			p_rua->rtp_rcv_len = p_rua->rcv_dlen;
			p_rua->rtp_t_len = rtp_len+4;

			p_rua->rcv_dlen = 0;

			return RTSP_RX_SUCC;
		}
			
		tcp_data_rx((uint8*)p_rilf, rtp_len+4);

		p_rua->rcv_dlen -= rtp_len+4;
		if (p_rua->rcv_dlen > 0)
		{
			memmove(p_rua->rcv_buf, p_rua->rcv_buf+rtp_len+4, p_rua->rcv_dlen);
		}

		if (p_rua->rcv_dlen >= 16)
		{
			goto rx_point;
		}
	}

	return RTSP_RX_SUCC;
}

int CRtspClient::rtsp_udp_rx()
{
	int i, max_fd = 0;
    fd_set fdr;
    
	FD_ZERO(&fdr);

    for (i = 0; i < AV_METADATA_CH; i++)
    {
        if (m_rua.channels[i].udp_fd)
        {
            FD_SET(m_rua.channels[i].udp_fd, &fdr);
            max_fd = (max_fd >= (int)m_rua.channels[i].udp_fd)? max_fd : (int)m_rua.channels[i].udp_fd;
        }
    }

	struct timeval tv = {0, 100*1000};

	int sret = select(max_fd+1, &fdr, NULL, NULL, &tv);
	if (sret <= 0)
	{
		return RTSP_RX_TIMEOUT;
    }

    int alen;
    char buf[2048];
    struct sockaddr_in addr;
    
	memset(&addr, 0, sizeof(addr));
	alen = sizeof(struct sockaddr_in);

    for (i = 0; i < AV_METADATA_CH; i++)
    {
        if (m_rua.channels[i].udp_fd && FD_ISSET(m_rua.channels[i].udp_fd, &fdr))
        {
            int rlen = recvfrom(m_rua.channels[i].udp_fd, buf, sizeof(buf), 0, (struct sockaddr *)&addr, (socklen_t*)&alen);
        	if (rlen <= 12)
        	{
        		log_print(HT_LOG_ERR, "%s, recvfrom return %d, err[%s]!!!\r\n", __FUNCTION__, rlen, sys_os_get_socket_error());
        	}
        	else
        	{
        	    udp_data_rx((uint8*)buf, rlen, i);
        	}
        }
    }
			
    return RTSP_RX_SUCC;
}

BOOL CRtspClient::rtsp_start(const char * url, const char * ip, int port, const char * user, const char * pass)
{
	if (m_rua.state != RCS_NULL)
	{
		rtsp_play();
		return TRUE;
	}

	if (user && user != m_rua.auth_info.auth_name)
	{
		strcpy(m_rua.auth_info.auth_name, user);
	}

	if (pass && pass != m_rua.auth_info.auth_pwd)
	{
		strcpy(m_rua.auth_info.auth_pwd, pass);
	}
	
	if (url && url != m_url)
	{
		strcpy(m_url, url);
	}
	
	if (ip && ip != m_ip)
	{
		strcpy(m_ip, ip);
	}
	
	m_nport = port;

#ifdef OVER_HTTP
    // Some rtsp servers do not support http prefixes
    if (memcmp(m_url, "http://", 7) == 0)
    {
        memcpy(m_url, "rtsp", 4);
    }
#endif

    m_bRunning = TRUE;
	m_tcpRxTid = sys_os_create_thread((void *)rtsp_tcp_rx_thread, this);
	if (m_tcpRxTid == 0)
	{
		log_print(HT_LOG_ERR, "%s, sys_os_create_thread failed!!!\r\n", __FUNCTION__);
		return FALSE;
	}
	
	return TRUE;
}

BOOL CRtspClient::rtsp_start(const char * url, char * user, char * pass)
{
    char* username = NULL;
	char* password = NULL;
	char* address = NULL;
	char const * suffix = NULL;
	int   urlPortNum = 554;
	
	if (!parse_url(url, username, password, address, urlPortNum, &suffix))
	{
		return FALSE;
	}
	
	if (username)
	{
	    strcpy(m_rua.auth_info.auth_name, username);
		delete[] username;
		username = m_rua.auth_info.auth_name;
	}
	else 
	{
	    username = user;
	}
	
	if (password)
	{
		strcpy(m_rua.auth_info.auth_pwd, password);
		delete[] password;
		password = m_rua.auth_info.auth_pwd;
	}
	else
	{
	    password = pass;
	}

	if (suffix)
	{
	    strncpy(m_suffix, suffix, sizeof(m_suffix) - 1);
	}

	strncpy(m_ip, address, sizeof(m_ip) - 1);
	delete[] address; 

	m_nport = urlPortNum;

    return rtsp_start(url, m_ip, m_nport, username, password);
}

void CRtspClient::copy_str_from_url(char* dest, char const* src, unsigned len) 
{
	// Normally, we just copy from the source to the destination.  However, if the source contains
	// %-encoded characters, then we decode them while doing the copy:
	while (len > 0) 
	{
		int nBefore = 0;
		int nAfter = 0;

		if (*src == '%' && len >= 3 && sscanf(src+1, "%n%2hhx%n", &nBefore, dest, &nAfter) == 1) 
		{
			unsigned codeSize = nAfter - nBefore; // should be 1 or 2

			++dest;
			src += (1 + codeSize);
			len -= (1 + codeSize);
		} 
		else 
		{
			*dest++ = *src++;
			--len;
		}
	}
	
	*dest = '\0';
}

BOOL CRtspClient::parse_url(char const* url, char*& user, char*& pass, char*& addr, int& port, char const** suffix) 
{
	do 
	{
		// Parse the URL as "rtsp://[<username>[:<password>]@]<server-address-or-name>[:<port>][/<stream-name>]"
        int ishttp = 0;
		char const* prefix = "rtsp://";
		unsigned const prefixLength = 7;
#ifdef OVER_HTTP
        if (strncasecmp(url, "http://", 7) == 0)
        {
            ishttp = 1;
        }
        else 
#endif
		if (strncasecmp(url, prefix, prefixLength) != 0) 
		{
			log_print(HT_LOG_ERR, "%s, URL is not of the form \"%s\"\r\n", __FUNCTION__, prefix);
			break;
		}

		unsigned const parseBufferSize = 100;
		char parseBuffer[parseBufferSize];
		char const* from = &url[prefixLength];

		// Check whether "<username>[:<password>]@" occurs next.
		// We do this by checking whether '@' appears before the end of the URL, or before the first '/'.
		user = pass = addr = NULL; // default return values
		char const* colonPasswordStart = NULL;
		char const* p;
		
		for (p = from; *p != '\0' && *p != '/'; ++p) 
		{
			if (*p == ':' && colonPasswordStart == NULL) 
			{
				colonPasswordStart = p;
			} 
			else if (*p == '@') 
			{
				// We found <username> (and perhaps <password>).  Copy them into newly-allocated result strings:
				if (colonPasswordStart == NULL)
				{
					colonPasswordStart = p;
				}
				
				char const* usernameStart = from;
				unsigned usernameLen = (unsigned)(colonPasswordStart - usernameStart);
				user= new char[usernameLen + 1] ; // allow for the trailing '\0'
				copy_str_from_url(user, usernameStart, usernameLen);

				char const* passwordStart = colonPasswordStart;
				if (passwordStart < p) ++passwordStart; // skip over the ':'
				unsigned passwordLen = (unsigned)(p - passwordStart);
				pass = new char[passwordLen + 1]; // allow for the trailing '\0'
				copy_str_from_url(pass, passwordStart, passwordLen);

				from = p + 1; // skip over the '@'
				break;
			}
		}

		// Next, parse <server-address-or-name>
		char* to = &parseBuffer[0];
		unsigned i;
		
		for (i = 0; i < parseBufferSize; ++i) 
		{
			if (*from == '\0' || *from == ':' || *from == '/') 
			{
				// We've completed parsing the address
				*to = '\0';
				break;
			}
			*to++ = *from++;
		}
		
		if (i == parseBufferSize) 
		{
			log_print(HT_LOG_ERR, "%s, URL is too long\r\n", __FUNCTION__);
			break;
		}

		addr = new char[strlen(parseBuffer)+1] ;
		strcpy(addr, parseBuffer);		

#ifdef OVER_HTTP
        if (ishttp)
        {
            port = 80;
        }
        else 
#endif
		port = 554; // default value
		char nextChar = *from;
		if (nextChar == ':') 
		{
			int portNumInt;
			if (sscanf(++from, "%d", &portNumInt) != 1) 
			{
				log_print(HT_LOG_ERR, "%s, No port number follows ':'\r\n", __FUNCTION__);
				break;
			}
			
			if (portNumInt < 1 || portNumInt > 65535) 
			{
				log_print(HT_LOG_ERR, "%s, Bad port number\r\n", __FUNCTION__);
				break;
			}
			
			port = portNumInt;
			
			while (*from >= '0' && *from <= '9')
			{
				++from; // skip over port number
			}
		}

		// The remainder of the URL is the suffix:
		if (suffix != NULL)
		{
			*suffix = from;
		}
		
		return TRUE;
	} while (0);

  	return FALSE;
}

BOOL CRtspClient::rtsp_play()
{
	m_rua.cseq++;
	
	HRTSP_MSG * tx_msg = rua_build_play(&m_rua);	
	if (tx_msg)
	{
		rcua_send_free_rtsp_msg(&m_rua, tx_msg);
	}
	
	return TRUE;
}

BOOL CRtspClient::rtsp_stop()
{
    if (RCS_NULL == m_rua.state)
    {
        return TRUE;
    }
    
	m_rua.cseq++;
	
	HRTSP_MSG * tx_msg = rua_build_teardown(&m_rua);
	if (tx_msg)
	{
		rcua_send_free_rtsp_msg(&m_rua, tx_msg);	
	}
	
	m_rua.state = RCS_NULL;
	
	return TRUE;
}

BOOL CRtspClient::rtsp_pause()
{
	m_rua.cseq++;
	
	HRTSP_MSG * tx_msg = rua_build_pause(&m_rua);	
	if (tx_msg)
	{
		rcua_send_free_rtsp_msg(&m_rua, tx_msg);	
	}
	
	return TRUE;
}

BOOL CRtspClient::rtsp_close()
{
	sys_os_mutex_enter(m_pMutex);
	m_pAudioCB = NULL;
	m_pVideoCB = NULL;
#ifdef METADATA	
	m_pMetadataCB = NULL;
#endif
	m_pNotify = NULL;
	m_pUserdata = NULL;
	sys_os_mutex_leave(m_pMutex);
	
    m_bRunning = FALSE;
	while (m_tcpRxTid != 0)
	{
		usleep(10*1000);
	}

	while (m_udpRxTid != 0)
	{
		usleep(10*1000);
	}

    for (int i = 0; i < AV_MAX_CHS; i++)
    {
        if (m_rua.channels[i].udp_fd > 0)
    	{
    		closesocket(m_rua.channels[i].udp_fd);
    		m_rua.channels[i].udp_fd = 0;
    	}
    }

#ifdef BACKCHANNEL
#if __WINDOWS_OS__
	if (m_rua.audio_captrue)
	{
		m_rua.audio_captrue->delCallback(rtsp_bc_cb, &m_rua);
		m_rua.audio_captrue->freeInstance(0);
		m_rua.audio_captrue = NULL;
	}
#elif defined(ANDROID)
    if (m_rua.audio_input)
    {
        delete m_rua.audio_input;
		m_rua.audio_input = NULL;
    }
#endif	
#endif
    
	if (m_VideoCodec == VIDEO_CODEC_H264)
	{
		h264_rxi_deinit(&h264rxi);
	}
	else if (m_VideoCodec == VIDEO_CODEC_H265)
	{
		h265_rxi_deinit(&h265rxi);
	}
	else if (m_VideoCodec == VIDEO_CODEC_JPEG)
	{
		mjpeg_rxi_deinit(&mjpegrxi);
	}
	else if (m_VideoCodec == VIDEO_CODEC_MP4)
	{
		mpeg4_rxi_deinit(&mpeg4rxi);
	}

	if (m_AudioCodec == AUDIO_CODEC_AAC)
	{
		aac_rxi_deinit(&aacrxi);
	}
	else if (m_AudioCodec != AUDIO_CODEC_NONE)
	{
		pcm_rxi_deinit(&pcmrxi);
	}

	memset(&rtprxi, 0, sizeof(RTPRXI));
	
	if (m_pAudioConfig)
	{
		delete [] m_pAudioConfig;
		m_pAudioConfig = NULL;
	}

#ifdef OVER_HTTP
    http_cln_free_req(&m_rua.rtsp_recv);
    http_cln_free_req(&m_rua.rtsp_send);
#endif

    set_default();
	
	return TRUE;
}

void CRtspClient::rtsp_video_data_cb(uint8 * p_data, int len, uint32 ts, uint32 seq)
{
	sys_os_mutex_enter(m_pMutex);
	if (m_pVideoCB)
	{
		m_pVideoCB(p_data, len, ts, seq, m_pUserdata);
	}
	sys_os_mutex_leave(m_pMutex);
}

void CRtspClient::rtsp_audio_data_cb(uint8 * p_data, int len, uint32 ts, uint32 seq)
{
	sys_os_mutex_enter(m_pMutex);
	if (m_pAudioCB)
	{
		m_pAudioCB(p_data, len, ts, seq, m_pUserdata);
	}
	sys_os_mutex_leave(m_pMutex);
}

void CRtspClient::rtsp_keep_alive()
{
	uint32 ms = sys_os_get_ms();
	if (ms - m_rua.keepalive_time >= (uint32)(m_rua.session_timeout - 10) * 1000)
	{
		m_rua.keepalive_time = ms;
		
		m_rua.cseq++;
		
		HRTSP_MSG * tx_msg;

		if (m_rua.gp_cmd) // the rtsp server supports GET_PARAMETER command
		{
			tx_msg = rua_build_get_parameter(&m_rua);
		}
		else
		{
			tx_msg = rua_build_options(&m_rua);
		}
		
		if (tx_msg)
    	{
    		rcua_send_free_rtsp_msg(&m_rua, tx_msg);
    	}
	}
}

void CRtspClient::tcp_rx_thread()
{
	int  ret;
	int  tm_count = 0;
    BOOL nodata_notify = FALSE;
    
	send_notify(RTSP_EVE_CONNECTING);

#ifdef OVER_HTTP
    if (m_rua.over_http)
    {
        if (!(rtsp_over_http_start()))
        {
            send_notify(RTSP_EVE_CONNFAIL);
		    goto rtsp_rx_exit;
        }
    }
    else 
#endif
	if (!rtsp_client_start())	
	{
		send_notify(RTSP_EVE_CONNFAIL);
		goto rtsp_rx_exit;
	}
    
	while (m_bRunning)
	{
	    ret = rtsp_tcp_rx();
	    if (ret == RTSP_RX_FAIL)
	    {
	        break;
	    }
	    else if (m_rua.rtp_tcp)
	    {
		    if (ret == RTSP_RX_TIMEOUT)
	    	{
	    		tm_count++;
		        if (tm_count >= 100 && !nodata_notify)    // in 10s without data
		        {
		            nodata_notify = TRUE;
		            send_notify(RTSP_EVE_NODATA);
		        }
	        }
	        else // should be RTSP_RX_SUCC
	        {
	        	if (nodata_notify)
		        {
		            nodata_notify = FALSE;
		            send_notify(RTSP_EVE_RESUME);
		        }
		        
	        	tm_count = 0;
	        }
        }
        else
        {
        	if (ret == RTSP_RX_TIMEOUT)
        	{
        		usleep(100*1000);
        	}
        }

	    if (m_rua.state == RCS_PLAYING)
	    {	    	
	    	rtsp_keep_alive();
	    }
	}

    if (m_rua.fd > 0)
	{
		closesocket(m_rua.fd);
		m_rua.fd = 0;
	}

    if (m_rua.rtp_rcv_buf)
    {
        free(m_rua.rtp_rcv_buf);
        m_rua.rtp_rcv_buf = NULL;
    }
    
	send_notify(RTSP_EVE_STOPPED);

rtsp_rx_exit:

	m_tcpRxTid = 0;
	log_print(HT_LOG_DBG, "%s, exit\r\n", __FUNCTION__);
}

void CRtspClient::udp_rx_thread()
{
	int  ret;
	int  tm_count = 0;
    BOOL nodata_notify = FALSE;
    
    while (m_bRunning)
	{
	    ret = rtsp_udp_rx();
	    if (ret == RTSP_RX_FAIL)
	    {
	        break;
	    }
	    else if (ret == RTSP_RX_TIMEOUT)
    	{
    		tm_count++;
	        if (tm_count >= 100 && !nodata_notify)    // in 10s without data
	        {
	            nodata_notify = TRUE;
	            send_notify(RTSP_EVE_NODATA);
	        }
        }
        else // should be RTSP_RX_SUCC
        {
        	if (nodata_notify)
	        {
	            nodata_notify = FALSE;
	            send_notify(RTSP_EVE_RESUME);
	        }
	        
        	tm_count = 0;
        }
	}

	m_udpRxTid = 0;
	
	log_print(HT_LOG_DBG, "%s, exit\r\n", __FUNCTION__);
}

void CRtspClient::send_notify(int event)
{
	sys_os_mutex_enter(m_pMutex);
	
	if (m_pNotify)
	{
		m_pNotify(event, m_pUserdata);
	}

	sys_os_mutex_leave(m_pMutex);
}

void CRtspClient::get_h264_params()
{
	rtsp_send_h264_params(&m_rua);
}

BOOL CRtspClient::get_h264_params(uint8 * p_sps, int * sps_len, uint8 * p_pps, int * pps_len)
{
	char sps[1000], pps[1000] = {'\0'};
	
	if (!rua_get_sdp_h264_params(&m_rua, NULL, sps, sizeof(sps)))
	{
		return FALSE;
	}

	char * ptr = strchr(sps, ',');
	if (ptr && ptr[1] != '\0')
	{
		*ptr = '\0';
		strcpy(pps, ptr+1);
	}

	uint8 sps_pps[1000];
	sps_pps[0] = 0x0;
	sps_pps[1] = 0x0;
	sps_pps[2] = 0x0;
	sps_pps[3] = 0x1;
	
	int len = base64_decode(sps, sps_pps+4, sizeof(sps_pps)-4);
	if (len <= 0)
	{
		return FALSE;
	}

    if ((sps_pps[4] & 0x1f) == 7)
    {
	    memcpy(p_sps, sps_pps, len+4);
	    *sps_len = len+4;
	}
	else if ((sps_pps[4] & 0x1f) == 8)
	{
	    memcpy(p_pps, sps_pps, len+4);
		*pps_len = len+4;
	}
	
	if (pps[0] != '\0')
	{		
		len = base64_decode(pps, sps_pps+4, sizeof(sps_pps)-4);
		if (len > 0)
		{
		    if ((sps_pps[4] & 0x1f) == 7)
            {
        	    memcpy(p_sps, sps_pps, len+4);
        	    *sps_len = len+4;
        	}
        	else if ((sps_pps[4] & 0x1f) == 8)
        	{
        	    memcpy(p_pps, sps_pps, len+4);
        		*pps_len = len+4;
        	}
		}
	}

	return TRUE;
}

BOOL CRtspClient::get_h264_sdp_desc(char * p_sdp, int max_len)
{
	return rua_get_sdp_h264_desc(&m_rua, NULL, p_sdp, max_len);
}

BOOL CRtspClient::get_h265_sdp_desc(char * p_sdp, int max_len)
{
	return rua_get_sdp_h265_desc(&m_rua, NULL, p_sdp, max_len);
}

BOOL CRtspClient::get_mp4_sdp_desc(char * p_sdp, int max_len)
{	
	return rua_get_sdp_mp4_desc(&m_rua, NULL, p_sdp, max_len);
}

BOOL CRtspClient::get_aac_sdp_desc(char * p_sdp, int max_len)
{
	return rua_get_sdp_aac_desc(&m_rua, NULL, p_sdp, max_len);
}

void CRtspClient::get_h265_params()
{
	rtsp_send_h265_params(&m_rua);
}

BOOL CRtspClient::get_h265_params(uint8 * p_sps, int * sps_len, uint8 * p_pps, int * pps_len, uint8 * p_vps, int * vps_len)
{
    int pt;
    BOOL don;
	char vps[1000] = {'\0'}, sps[1000] = {'\0'}, pps[1000] = {'\0'};
	
	if (!rua_get_sdp_h265_params(&m_rua, &pt, &don, vps, sizeof(vps), sps, sizeof(sps), pps, sizeof(pps)))
	{
		return FALSE;
	}

	uint8 buff[1000];
	buff[0] = 0x0;
	buff[1] = 0x0;
	buff[2] = 0x0;
	buff[3] = 0x1;
	
	if (vps[0] != '\0')
	{
		int len = base64_decode(vps, buff+4, sizeof(buff)-4);
		if (len <= 0)
		{
			return FALSE;
		}

        memcpy(p_vps, buff, len+4);
	    *vps_len = len+4;
	}

	if (sps[0] != '\0')
	{
		int len = base64_decode(sps, buff+4, sizeof(buff)-4);
		if (len <= 0)
		{
			return FALSE;
		}

		memcpy(p_sps, buff, len+4);
	    *sps_len = len+4;
	}

	if (pps[0] != '\0')
	{
		int len = base64_decode(pps, buff+4, sizeof(buff)-4);
		if (len <= 0)
		{
			return FALSE;
		}

		memcpy(p_pps, buff, len+4);
	    *pps_len = len+4;
	}

	return TRUE;
}	

BOOL CRtspClient::rtsp_get_video_media_info()
{
	if (m_rua.channels[AV_VIDEO_CH].cap_count == 0)
	{
		return FALSE;
	}

	if (m_rua.channels[AV_VIDEO_CH].cap[0] == 26)
	{
		m_VideoCodec = VIDEO_CODEC_JPEG;
	}

	int i;
	int rtpmap_len = (int)strlen("a=rtpmap:");

	for (i=0; i<MAX_AVN; i++)
	{
		char * ptr = m_rua.channels[AV_VIDEO_CH].cap_desc[i];
		if (memcmp(ptr, "a=rtpmap:", rtpmap_len) == 0)
		{
			char pt_buf[16];
			char code_buf[64];
			int next_offset = 0;
			ptr += rtpmap_len;

			if (GetLineWord(ptr, 0, (int)strlen(ptr), pt_buf, sizeof(pt_buf), &next_offset, WORD_TYPE_NUM) == FALSE)
				return FALSE;
			
			GetLineWord(ptr, next_offset, (int)strlen(ptr)-next_offset, code_buf, sizeof(code_buf),  &next_offset, WORD_TYPE_STRING);

			if (strcasecmp(code_buf, "H264/90000") == 0)
			{
				m_VideoCodec = VIDEO_CODEC_H264;
			}
			else if (strcasecmp(code_buf, "JPEG/90000") == 0)
			{
				m_VideoCodec = VIDEO_CODEC_JPEG;
			}
			else if (strcasecmp(code_buf, "MP4V-ES/90000") == 0)
			{
				m_VideoCodec = VIDEO_CODEC_MP4;
			}
			else if (strcasecmp(code_buf, "H265/90000") == 0)
			{
				m_VideoCodec = VIDEO_CODEC_H265;
			}

			break;
		}
	}

	return TRUE;
}

BOOL CRtspClient::rtsp_get_audio_media_info()
{
	if (m_rua.channels[AV_AUDIO_CH].cap_count == 0)
	{
		return FALSE;
	}

	if (m_rua.channels[AV_AUDIO_CH].cap[0] == 0)
	{
		m_AudioCodec = AUDIO_CODEC_G711U;
	}
	else if (m_rua.channels[AV_AUDIO_CH].cap[0] == 8)
	{
		m_AudioCodec = AUDIO_CODEC_G711A;
	}
	else if (m_rua.channels[AV_AUDIO_CH].cap[0] == 9)
	{
		m_AudioCodec = AUDIO_CODEC_G722;
	}

	int i;
	int rtpmap_len = (int)strlen("a=rtpmap:");

	for (i=0; i<MAX_AVN; i++)
	{
		char * ptr = m_rua.channels[AV_AUDIO_CH].cap_desc[i];
		if (memcmp(ptr, "a=rtpmap:", rtpmap_len) == 0)
		{
			char pt_buf[16];
			char code_buf[64];
			int next_offset = 0;

			ptr += rtpmap_len;

			if (GetLineWord(ptr, 0, (int)strlen(ptr), pt_buf, sizeof(pt_buf), &next_offset, WORD_TYPE_NUM) == FALSE)
			{
				return FALSE;
			}
			
			GetLineWord(ptr, next_offset, (int)strlen(ptr)-next_offset, code_buf, sizeof(code_buf),  &next_offset, WORD_TYPE_STRING);

			uppercase(code_buf);

			if (strstr(code_buf, "G726-32"))
			{
				m_AudioCodec = AUDIO_CODEC_G726;
			}
			else if (strstr(code_buf, "G722"))
			{
				m_AudioCodec = AUDIO_CODEC_G722;
			}
			else if (strstr(code_buf, "PCMU"))
			{
				m_AudioCodec = AUDIO_CODEC_G711U;
			}
			else if (strstr(code_buf, "PCMA"))
			{
				m_AudioCodec = AUDIO_CODEC_G711A;
			}
			else if (strstr(code_buf, "MPEG4-GENERIC"))
			{
				m_AudioCodec = AUDIO_CODEC_AAC;
			}
			else if (strstr(code_buf, "OPUS"))
			{
				m_AudioCodec = AUDIO_CODEC_OPUS;
			}

			char * p = strchr(code_buf, '/');
			if (p)
			{
				p++;
				
				char * p1 = strchr(p, '/');
				if (p1)
				{
					*p1 = '\0';
					m_nSamplerate = atoi(p);

					p1++;
					if (p1 && *p1 != '\0')
					{
						m_nChannels = atoi(p1);
					}
					else
					{
						m_nChannels = 1;
					}
				}
				else
				{
					m_nSamplerate = atoi(p);
					m_nChannels = 1;
				}
			}

			break;
		}
	}

    if (m_AudioCodec == AUDIO_CODEC_G722)
    {
        m_nSamplerate = 16000;
        m_nChannels = 1;
    }
    
	return TRUE;
}

void CRtspClient::set_notify_cb(notify_cb notify, void * userdata) 
{
	sys_os_mutex_enter(m_pMutex);	
	m_pNotify = notify;
	m_pUserdata = userdata;
	sys_os_mutex_leave(m_pMutex);
}

void CRtspClient::set_video_cb(video_cb cb) 
{
	sys_os_mutex_enter(m_pMutex);
	m_pVideoCB = cb;
	sys_os_mutex_leave(m_pMutex);
}

void CRtspClient::set_audio_cb(audio_cb cb)
{
	sys_os_mutex_enter(m_pMutex);
	m_pAudioCB = cb;
	sys_os_mutex_leave(m_pMutex);
}

void CRtspClient::set_rtp_multicast(int flag)
{
    if (flag)
	{
		m_rua.rtp_tcp = 0;
	}
	
	m_rua.mast_flag = flag;
}

void CRtspClient::set_rtp_over_udp(int flag)
{
    if (flag)
    {
        m_rua.rtp_tcp = 0;
    }
    else
    {
        m_rua.rtp_tcp = 1;
    }
}


