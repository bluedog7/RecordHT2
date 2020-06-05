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

#include "sys_inc.h"
#include "rtp.h"
#include "word_analyse.h"
#include "rtsp_parse.h"
#include "rtsp_rcua.h"
#include "rfc_md5.h"
#include "base64.h"
#include "rtsp_util.h"

/***********************************************************************/
BOOL rua_calc_auth_digest(RCUA * p_rua, HD_AUTH_INFO * auth_info, const char * method)
{
    MD5_CTX Md5Ctx;
	HASH HA1;
	HASH HA2;
	HASH HA3;

	HASHHEX HA1Hex;
	HASHHEX HA2Hex;
	HASHHEX HA3Hex;	

	MD5Init(&Md5Ctx);
	MD5Update(&Md5Ctx, (uint8 *)auth_info->auth_name, (int)strlen(auth_info->auth_name));
	MD5Update(&Md5Ctx, (uint8 *)&(":"), 1);
	MD5Update(&Md5Ctx, (uint8 *)auth_info->auth_realm, (int)strlen(auth_info->auth_realm));
	MD5Update(&Md5Ctx, (uint8 *)&(":"), 1);
	MD5Update(&Md5Ctx, (uint8 *)auth_info->auth_pwd, (int)strlen(auth_info->auth_pwd));
	MD5Final(HA1, &Md5Ctx);

	BinToHexStr(HA1, HA1Hex);

	MD5Init(&Md5Ctx);
	MD5Update(&Md5Ctx, (uint8 *)method, (int)strlen(method));
	MD5Update(&Md5Ctx, (uint8 *)&(":"), 1);
	MD5Update(&Md5Ctx, (uint8 *)auth_info->auth_uri, (int)strlen(auth_info->auth_uri));
	MD5Final(HA2, &Md5Ctx);

	BinToHexStr(HA2, HA2Hex);

	MD5Init(&Md5Ctx);
	MD5Update(&Md5Ctx, (uint8 *)HA1Hex, HASHHEXLEN);
	MD5Update(&Md5Ctx, (uint8 *)&(":"), 1);
	MD5Update(&Md5Ctx, (uint8 *)auth_info->auth_nonce, (int)strlen(auth_info->auth_nonce));
	MD5Update(&Md5Ctx, (uint8 *)&(":"), 1);

	if (auth_info->auth_qop[0] != '\0') 
	{
		MD5Update(&Md5Ctx, (uint8 *)auth_info->auth_ncstr, (int)strlen(auth_info->auth_ncstr));
		MD5Update(&Md5Ctx, (uint8 *)&(":"), 1);
		MD5Update(&Md5Ctx, (uint8 *)auth_info->auth_cnonce, (int)strlen(auth_info->auth_cnonce));
		MD5Update(&Md5Ctx, (uint8 *)&(":"), 1);
		MD5Update(&Md5Ctx, (uint8 *)auth_info->auth_qop, (int)strlen(auth_info->auth_qop));
		MD5Update(&Md5Ctx, (uint8 *)&(":"), 1);
	};
	
	MD5Update(&Md5Ctx, (uint8 *)HA2Hex, HASHHEXLEN);
	MD5Final(HA3, &Md5Ctx);

	BinToHexStr(HA3, HA3Hex);

	strcpy(auth_info->auth_response, HA3Hex);
	
	return TRUE;
}

void rua_build_auth_line(RCUA * p_rua, HRTSP_MSG * tx_msg, const char * p_method)
{
	if (0 == p_rua->need_auth)
    {
    	return;
    }
    
    if (p_rua->auth_mode == 1)	// digest
    {
        rua_calc_auth_digest(p_rua, &p_rua->auth_info, p_method);

        if (p_rua->auth_info.auth_qop[0] != '\0')
        {
            rtsp_add_tx_msg_line(tx_msg,"Authorization","Digest username=\"%s\", realm=\"%s\", "
    	    	"nonce=\"%s\", uri=\"%s\", response=\"%s\", qop=\"auth\", algorithm=\"MD5\", cnonce=\"%s\", nc=%s",
    			p_rua->auth_info.auth_name, p_rua->auth_info.auth_realm, p_rua->auth_info.auth_nonce, 
    			p_rua->auth_info.auth_uri, p_rua->auth_info.auth_response, p_rua->auth_info.auth_cnonce,
    			p_rua->auth_info.auth_ncstr);
        }
        else
        {
    	    rtsp_add_tx_msg_line(tx_msg,"Authorization","Digest username=\"%s\", realm=\"%s\", "
    	    	"nonce=\"%s\", uri=\"%s\", response=\"%s\", algorithm=\"MD5\"",
    			p_rua->auth_info.auth_name, p_rua->auth_info.auth_realm, p_rua->auth_info.auth_nonce, 
    			p_rua->auth_info.auth_uri, p_rua->auth_info.auth_response);
		}
	}
	else if (p_rua->auth_mode == 0) // basic
	{
		char buff[512] = {'\0'};
		char basic[512] = {'\0'};
		sprintf(buff, "%s:%s", p_rua->auth_info.auth_name, p_rua->auth_info.auth_pwd);
		base64_encode((uint8 *)buff, (int)strlen(buff), basic, sizeof(basic));
		rtsp_add_tx_msg_line(tx_msg, "Authorization", "Basic %s", basic);
	}
}

HRTSP_MSG * rua_build_options(RCUA * p_rua)
{
	HRTSP_MSG * tx_msg = rtsp_get_msg_buf();
	if (tx_msg == NULL)
	{
		log_print(HT_LOG_ERR, "%s, rtsp_get_msg_buf return NULL!!!\r\n", __FUNCTION__);
		return NULL;
	}

	tx_msg->msg_type = 0;
	tx_msg->msg_sub_type = RTSP_MT_OPTIONS;

	rtsp_add_tx_msg_fline(tx_msg, "OPTIONS", "%s RTSP/1.0", p_rua->uri);

	rtsp_add_tx_msg_line(tx_msg, "CSeq", "%u", p_rua->cseq);

	rua_build_auth_line(p_rua, tx_msg, "OPTIONS");    

	if (p_rua->sid[0] != '\0')
	{
		rtsp_add_tx_msg_line(tx_msg, "Session", "%s", p_rua->sid);
	}
	
	rtsp_add_tx_msg_line(tx_msg, "User-Agent", p_rua->user_agent);

	return tx_msg;
}

HRTSP_MSG * rua_build_describe(RCUA * p_rua)
{
	HRTSP_MSG * tx_msg = rtsp_get_msg_buf();
	if (tx_msg == NULL)
	{
		log_print(HT_LOG_ERR, "%s, rtsp_get_msg_buf return NULL!!!\r\n", __FUNCTION__);
		return NULL;
	}

	tx_msg->msg_type = 0;
	tx_msg->msg_sub_type = RTSP_MT_DESCRIBE;

	rtsp_add_tx_msg_fline(tx_msg, "DESCRIBE", "%s RTSP/1.0", p_rua->uri);

	rtsp_add_tx_msg_line(tx_msg, "CSeq", "%u", p_rua->cseq);

	rua_build_auth_line(p_rua, tx_msg, "DESCRIBE");
	
	rtsp_add_tx_msg_line(tx_msg, "Accept", "application/sdp");
	rtsp_add_tx_msg_line(tx_msg, "User-Agent", p_rua->user_agent);

#ifdef BACKCHANNEL
	if (p_rua->backchannel)
	{
		rtsp_add_tx_msg_line(tx_msg, "Require", "www.onvif.org/ver20/backchannel");
	}
#endif

	return tx_msg;
}

void rua_build_setup_fline(HRTSP_MSG * tx_msg, const char * ctl, const char * uri)
{
    if (strncmp(ctl, "rtsp://", 7) == 0)
	{
		rtsp_add_tx_msg_fline(tx_msg, "SETUP", "%s RTSP/1.0", ctl);
	}	
	else	
	{
		int len = (int)strlen(uri);
		
		if (uri[len-1] == '/')
		{
			rtsp_add_tx_msg_fline(tx_msg, "SETUP", "%s%s RTSP/1.0", uri, ctl);
		}
		else
		{
			rtsp_add_tx_msg_fline(tx_msg, "SETUP", "%s/%s RTSP/1.0", uri, ctl);
		}
	}
}

HRTSP_MSG * rua_build_setup(RCUA * p_rua,int av_t)
{
	HRTSP_MSG * tx_msg = rtsp_get_msg_buf();
	if (tx_msg == NULL)
	{
		log_print(HT_LOG_ERR, "%s, rtsp_get_msg_buf return NULL!!!\r\n", __FUNCTION__);
		return NULL;
	}

	tx_msg->msg_type = 0;
	tx_msg->msg_sub_type = RTSP_MT_SETUP;

    rua_build_setup_fline(tx_msg, p_rua->channels[av_t].ctl, p_rua->uri);
	
	rtsp_add_tx_msg_line(tx_msg, "CSeq", "%u", p_rua->cseq);

	if (p_rua->sid[0] != '\0')
	{
		rtsp_add_tx_msg_line(tx_msg, "Session", "%s", p_rua->sid);
    }
    
	rua_build_auth_line(p_rua, tx_msg, "SETUP");    

    if (p_rua->rtp_tcp) // TCP
	{
	    rtsp_add_tx_msg_line(tx_msg, "Transport", "RTP/AVP/TCP;unicast;interleaved=%u-%u", 
	        p_rua->channels[av_t].interleaved, p_rua->channels[av_t].interleaved+1);	
    }
    else if (p_rua->rtp_mcast) // rtp multicase
    {
        rtsp_add_tx_msg_line(tx_msg, "Transport", "RTP/AVP;multicast;port=%u-%u", 
            p_rua->channels[av_t].r_port, p_rua->channels[av_t].r_port+1);
    }
    else
    {
        rtsp_add_tx_msg_line(tx_msg, "Transport", "RTP/AVP;unicast;client_port=%u-%u", 
            p_rua->channels[av_t].l_port, p_rua->channels[av_t].l_port+1);
    }
	
	rtsp_add_tx_msg_line(tx_msg, "User-Agent", p_rua->user_agent);

#ifdef BACKCHANNEL
	if (av_t == AV_TYPE_BACKCHANNEL) // backchannel
	{
		rtsp_add_tx_msg_line(tx_msg, "Require", "www.onvif.org/ver20/backchannel");
	}
#endif

#ifdef REPLAY
    if (p_rua->replay)
    {
        rtsp_add_tx_msg_line(tx_msg, "Require", "onvif-replay");
    }
#endif

	return tx_msg;
}

HRTSP_MSG * rua_build_play(RCUA * p_rua)
{
	HRTSP_MSG * tx_msg = rtsp_get_msg_buf();
	if (tx_msg == NULL)
	{
		log_print(HT_LOG_ERR, "%s, rtsp_get_msg_buf return NULL!!!\r\n", __FUNCTION__);
		return NULL;
	}

	tx_msg->msg_type = 0;
	tx_msg->msg_sub_type = RTSP_MT_PLAY;

	rtsp_add_tx_msg_fline(tx_msg, "PLAY", "%s RTSP/1.0", p_rua->uri);

	rtsp_add_tx_msg_line(tx_msg, "CSeq", "%u", p_rua->cseq);

	rua_build_auth_line(p_rua, tx_msg, "PLAY");
	
	rtsp_add_tx_msg_line(tx_msg, "Session", "%s", p_rua->sid);

#ifndef REPLAY
	rtsp_add_tx_msg_line(tx_msg, "Range", "npt=0.0-");
#endif

	rtsp_add_tx_msg_line(tx_msg, "User-Agent", p_rua->user_agent);

#ifdef BACKCHANNEL
	if (p_rua->backchannel) // backchannel
	{
		rtsp_add_tx_msg_line(tx_msg, "Require", "www.onvif.org/ver20/backchannel");
	}
#endif

#ifdef REPLAY
    if (p_rua->replay)
    {
        rtsp_add_tx_msg_line(tx_msg, "Require", "onvif-replay");

        if (p_rua->scale_flag)
        {
            rtsp_add_tx_msg_line(tx_msg, "Scale", "%8.2f", p_rua->scale / 100.0);
        }

        if (p_rua->rate_control_flag)
        {
            rtsp_add_tx_msg_line(tx_msg, "Rate-Control", "%s", p_rua->rate_control ? "yes" : "no");
        }

        if (p_rua->immediate_flag && p_rua->immediate)
        {
            rtsp_add_tx_msg_line(tx_msg, "Immediate", "yes");
        }

        if (p_rua->frame_flag)
        {
            if (1 == p_rua->frame)
            {
                rtsp_add_tx_msg_line(tx_msg, "Frames", "predicted");
            }
            else if (2 == p_rua->frame)
            {
                if (p_rua->frame_interval_flag && p_rua->frame_interval)
                {
                    rtsp_add_tx_msg_line(tx_msg, "Frames", "intra/%d", p_rua->frame_interval);
                }
                else
                {
                    rtsp_add_tx_msg_line(tx_msg, "Frames", "intra");
                }
            }
        }

		if (p_rua->range_flag && p_rua->replay_start > 0)
		{
			char range[256];
			char start[32], end[32];
		    struct tm *t1;	

			t1 = gmtime(&p_rua->replay_start);

			sprintf(start, "%04d%02d%02dT%02d%02d%02dZ",
				t1->tm_year+1900, t1->tm_mon+1, t1->tm_mday,
				t1->tm_hour, t1->tm_min, t1->tm_sec);

	        if (p_rua->replay_end > 0)
	        {
	            t1 = gmtime(&p_rua->replay_end);

				sprintf(end, "%04d%02d%02dT%02d%02d%02dZ",
					t1->tm_year+1900, t1->tm_mon+1, t1->tm_mday,
					t1->tm_hour, t1->tm_min, t1->tm_sec);

				sprintf(range, "clock=%s-%s", start, end);	
	        }
	        else
	        {
	            sprintf(range, "clock=%s-", start);
	        }
			
			rtsp_add_tx_msg_line(tx_msg, "Range", range);
		}
    }
#endif

	return tx_msg;
}

HRTSP_MSG * rua_build_pause(RCUA * p_rua)
{
	HRTSP_MSG * tx_msg = rtsp_get_msg_buf();
	if (tx_msg == NULL)
	{
		log_print(HT_LOG_ERR, "%s::rtsp_get_msg_buf return NULL!!!\r\n", __FUNCTION__);
		return NULL;
	}

	tx_msg->msg_type = 0;
	tx_msg->msg_sub_type = RTSP_MT_PAUSE;

	rtsp_add_tx_msg_fline(tx_msg, "PAUSE", "%s RTSP/1.0", p_rua->uri);

	rtsp_add_tx_msg_line(tx_msg, "CSeq", "%u", p_rua->cseq);

	rua_build_auth_line(p_rua, tx_msg, "PAUSE");
	
	rtsp_add_tx_msg_line(tx_msg, "Session", "%s", p_rua->sid);
	rtsp_add_tx_msg_line(tx_msg, "User-Agent", p_rua->user_agent);

	return tx_msg;
}

HRTSP_MSG * rua_build_get_parameter(RCUA * p_rua)
{
	HRTSP_MSG * tx_msg = rtsp_get_msg_buf();
	if (tx_msg == NULL)
	{
		log_print(HT_LOG_ERR, "%s, rtsp_get_msg_buf return NULL!!!\r\n", __FUNCTION__);
		return NULL;
	}

	tx_msg->msg_type = 0;
	tx_msg->msg_sub_type = RTSP_MT_GET_PARAMETER;

	rtsp_add_tx_msg_fline(tx_msg, "GET_PARAMETER", "%s RTSP/1.0", p_rua->uri);

	rtsp_add_tx_msg_line(tx_msg, "CSeq", "%u", p_rua->cseq);

	rua_build_auth_line(p_rua, tx_msg, "GET_PARAMETER");
	
	rtsp_add_tx_msg_line(tx_msg, "Session", "%s", p_rua->sid);
	rtsp_add_tx_msg_line(tx_msg, "User-Agent", p_rua->user_agent);

	return tx_msg;
}


HRTSP_MSG * rua_build_teardown(RCUA * p_rua)
{
	HRTSP_MSG * tx_msg = rtsp_get_msg_buf();
	if (tx_msg == NULL)
	{
		log_print(HT_LOG_ERR, "%s, rtsp_get_msg_buf return NULL!!!\r\n", __FUNCTION__);
		return NULL;
	}

	tx_msg->msg_type = 0;
	tx_msg->msg_sub_type = RTSP_MT_TEARDOWN;

	rtsp_add_tx_msg_fline(tx_msg, "TEARDOWN", "%s RTSP/1.0", p_rua->uri);

	rtsp_add_tx_msg_line(tx_msg, "CSeq", "%u", p_rua->cseq);

	rua_build_auth_line(p_rua, tx_msg, "TEARDOWN");
	
	rtsp_add_tx_msg_line(tx_msg, "Session", "%s", p_rua->sid);
	rtsp_add_tx_msg_line(tx_msg, "User-Agent", p_rua->user_agent);

#ifdef BACKCHANNEL
	if (p_rua->backchannel) // backchannel
	{
		rtsp_add_tx_msg_line(tx_msg, "Require", "www.onvif.org/ver20/backchannel");
	}
#endif

	return tx_msg;
}

BOOL rua_get_media_info(RCUA * p_rua, HRTSP_MSG * rx_msg)
{
	if (rx_msg == NULL || p_rua == NULL)
	{
		return FALSE;
	}
	
	if (rtsp_msg_with_sdp(rx_msg))
	{
		rtsp_get_remote_cap(rx_msg, "video", &(p_rua->channels[AV_VIDEO_CH].cap_count), 
		    p_rua->channels[AV_VIDEO_CH].cap, &p_rua->channels[AV_VIDEO_CH].r_port);
		rtsp_get_remote_cap_desc(rx_msg, "video", p_rua->channels[AV_VIDEO_CH].cap_desc);

#ifdef BACKCHANNEL
		if (p_rua->backchannel)
		{
			rtsp_get_remote_cap(rx_msg, "audio", &(p_rua->channels[AV_AUDIO_CH].cap_count), 
			    p_rua->channels[AV_AUDIO_CH].cap, &p_rua->channels[AV_AUDIO_CH].r_port, "recvonly");
			rtsp_get_remote_cap_desc(rx_msg, "audio", p_rua->channels[AV_AUDIO_CH].cap_desc, "recvonly");
			
			rtsp_get_remote_cap(rx_msg, "audio", &(p_rua->channels[AV_BACK_CH].cap_count), 
			    p_rua->channels[AV_BACK_CH].cap, &p_rua->channels[AV_BACK_CH].r_port, "sendonly");
			rtsp_get_remote_cap_desc(rx_msg, "audio", p_rua->channels[AV_BACK_CH].cap_desc, "sendonly");
		}
		else
		{
			rtsp_get_remote_cap(rx_msg, "audio", &(p_rua->channels[AV_AUDIO_CH].cap_count), 
			    p_rua->channels[AV_AUDIO_CH].cap, &p_rua->channels[AV_AUDIO_CH].r_port);
			rtsp_get_remote_cap_desc(rx_msg, "audio", p_rua->channels[AV_AUDIO_CH].cap_desc);
		}
#else
		rtsp_get_remote_cap(rx_msg, "audio", &(p_rua->channels[AV_AUDIO_CH].cap_count), 
		    p_rua->channels[AV_AUDIO_CH].cap, &p_rua->channels[AV_AUDIO_CH].r_port);
		rtsp_get_remote_cap_desc(rx_msg, "audio", p_rua->channels[AV_AUDIO_CH].cap_desc);
#endif

#ifdef METADATA
		rtsp_get_remote_cap(rx_msg, "application", &(p_rua->channels[AV_METADATA_CH].cap_count), 
		    p_rua->channels[AV_METADATA_CH].cap, &p_rua->channels[AV_METADATA_CH].r_port);
		rtsp_get_remote_cap_desc(rx_msg, "application", p_rua->channels[AV_METADATA_CH].cap_desc);
#endif

		return TRUE;
	}
	
	return FALSE;
}

void rcua_send_rtsp_msg(RCUA * p_rua, HRTSP_MSG * tx_msg)
{
	char  * tx_buf;
	int     offset=0;
	char    rtsp_tx_buffer[2048];
    SOCKET  fd = -1;
    
	if (tx_msg == NULL)
	{
	    return;
	}

#ifdef OVER_HTTP
    if (p_rua->over_http)
    {
        fd = p_rua->rtsp_send.cfd;
    }
    else 
#endif 
    fd = p_rua->fd;
    
    if (fd <= 0)
    {
        return;
    }

	tx_buf = rtsp_tx_buffer;

	offset += sprintf(tx_buf+offset, "%s %s\r\n", tx_msg->first_line.header, tx_msg->first_line.value_string);

	HDRV * pHdrV = (HDRV *)pps_lookup_start(&(tx_msg->rtsp_ctx));
	while (pHdrV != NULL)
	{
		offset += sprintf(tx_buf+offset, "%s: %s\r\n", pHdrV->header, pHdrV->value_string);
		pHdrV = (HDRV *)pps_lookup_next(&(tx_msg->rtsp_ctx), pHdrV);
	}
	pps_lookup_end(&(tx_msg->rtsp_ctx));

	offset += sprintf(tx_buf+offset, "\r\n");

	if (tx_msg->sdp_ctx.node_num != 0)
	{
		pHdrV = (HDRV *)pps_lookup_start(&(tx_msg->sdp_ctx));
		while (pHdrV != NULL)
		{
			if ((strcmp(pHdrV->header,"pidf") == 0) || (strcmp(pHdrV->header,"text/plain") == 0))
			{
				offset += sprintf(tx_buf+offset, "%s\r\n", pHdrV->value_string);
			}	
			else
			{
				if (pHdrV->header[0] != '\0')
				{
					offset += sprintf(tx_buf+offset, "%s=%s\r\n", pHdrV->header, pHdrV->value_string);
				}	
				else
				{
					offset += sprintf(tx_buf+offset, "%s\r\n", pHdrV->value_string);
				}	
			}

			pHdrV = (HDRV *)pps_lookup_next(&(tx_msg->sdp_ctx), pHdrV);
		}
		pps_lookup_end(&(tx_msg->sdp_ctx));
	}

	log_print(HT_LOG_DBG, "TX >> %s\r\n", tx_buf);

#ifdef OVER_HTTP
    if (p_rua->over_http)
    {
        char tx_buf_base64[4096];
        
        base64_encode((uint8 *)tx_buf, offset, tx_buf_base64, sizeof(tx_buf_base64));

        tx_buf = tx_buf_base64;
        offset = strlen(tx_buf_base64);
	}
#endif

	int slen = send(fd, tx_buf, offset, 0);
	if (slen <= 0)
	{
		log_print(HT_LOG_ERR, "%s, send message failed!!!\r\n", __FUNCTION__);
	}
}

BOOL rua_get_sdp_video_desc(RCUA * p_rua, const char * key, int * pt, char * p_sdp, int max_len)
{
    int payload_type = 0, i;
	int rtpmap_len = (int)strlen("a=rtpmap:");
    char key_str[64] = {'\0'};

    strncpy(key_str, key, sizeof(key_str)-1);
    
	for (i=0; i<MAX_AVN; i++)
	{
		char * ptr = p_rua->channels[AV_VIDEO_CH].cap_desc[i];
		if (memcmp(ptr, "a=rtpmap:", rtpmap_len) == 0)
		{
			char pt_buf[16];
			char code_buf[64] = {'\0'};
			int next_offset = 0;

			ptr += rtpmap_len;

			if (GetLineWord(ptr, 0, (int)strlen(ptr), pt_buf, sizeof(pt_buf), &next_offset, WORD_TYPE_NUM) == FALSE)
			{
				return FALSE;
			}
			
			GetLineWord(ptr, next_offset, (int)strlen(ptr)-next_offset, code_buf, sizeof(code_buf),  &next_offset, WORD_TYPE_STRING);

			if (memcmp(uppercase(code_buf), uppercase(key_str), strlen(key_str)) == 0)
			{
				payload_type = atoi(pt_buf);
				if (payload_type <= 0)
				{
					return FALSE;
				}
				
				break;
			}
		}
	}

	if (payload_type == 0)
	{
		return FALSE;
	}
	
    if (pt)
    {
	    *pt = payload_type;
	}

	if (p_sdp == NULL)
	{
		return TRUE;
	}
	
	p_sdp[0] = '\0';

	char fmtp_buf[32];
	int fmtp_len = sprintf(fmtp_buf, "a=fmtp:%u", payload_type);

	for (i=0; i<MAX_AVN; i++)
	{
		char * ptr = p_rua->channels[AV_VIDEO_CH].cap_desc[i];
		if (memcmp(ptr, fmtp_buf, fmtp_len) == 0)
		{
			ptr += rtpmap_len+1;
			strncpy(p_sdp, ptr, max_len);
			break;
		}
	}

	return TRUE;
}

BOOL rua_get_sdp_audio_desc(RCUA * p_rua, const char * key, int * pt, char * p_sdp, int max_len)
{
    int payload_type = 0, i;
	int rtpmap_len = (int)strlen("a=rtpmap:");
    char key_str[64] = {'\0'};

    strncpy(key_str, key, sizeof(key_str)-1);
    
	for (i=0; i<MAX_AVN; i++)
	{
		char * ptr = p_rua->channels[AV_AUDIO_CH].cap_desc[i];
		if (memcmp(ptr, "a=rtpmap:", rtpmap_len) == 0)
		{
			char pt_buf[16];
			char code_buf[64] = {'\0'};
			int next_offset = 0;
			ptr += rtpmap_len;

			if (GetLineWord(ptr, 0, (int)strlen(ptr), pt_buf, sizeof(pt_buf), &next_offset, WORD_TYPE_NUM) == FALSE)
			{
				return FALSE;
			}
			
			GetLineWord(ptr, next_offset, (int)strlen(ptr)-next_offset, code_buf, sizeof(code_buf),  &next_offset, WORD_TYPE_STRING);
			
			if (memcmp(uppercase(code_buf), uppercase(key_str), strlen(key_str)) == 0)
			{
				payload_type = atoi(pt_buf);				
				if (payload_type <= 0)
				{
					return FALSE;
				}
				
				break;
			}
		}
	}

	if (payload_type == 0)
	{
		return FALSE;
	}
	
    if (pt)
    {
	    *pt = payload_type;
	}

	if (p_sdp == NULL)
	{
		return TRUE;
	}
	
	p_sdp[0] = '\0';

	char fmtp_buf[32];
	int fmtp_len = sprintf(fmtp_buf, "a=fmtp:%u", payload_type);

	for (i=0; i<MAX_AVN; i++)
	{
		char * ptr = p_rua->channels[AV_AUDIO_CH].cap_desc[i];
		if (memcmp(ptr, fmtp_buf, fmtp_len) == 0)
		{
			ptr += rtpmap_len+1;
			strncpy(p_sdp, ptr, max_len);
			break;
		}
	}

	return TRUE;
}

BOOL rua_get_sdp_h264_desc(RCUA * p_rua, int * pt, char * p_sdp, int max_len)
{
    return rua_get_sdp_video_desc(p_rua, "H264/90000", pt, p_sdp, max_len);
}

BOOL rua_get_sdp_h264_params(RCUA * p_rua, int * pt, char * p_sps_pps, int max_len)
{
    BOOL ret = FALSE;
    char sdp[1024] = {'\0'};
    
    if (rua_get_sdp_h264_desc(p_rua, pt, sdp, sizeof(sdp)) == FALSE)
    {
        return FALSE;
    }
    
    char * p_substr = strstr(sdp, "sprop-parameter-sets=");
	if (p_substr != NULL)
	{
		p_substr += strlen("sprop-parameter-sets=");
		char * p_tmp = p_substr;

		while (*p_tmp != ' ' && *p_tmp != ';' && *p_tmp != '\0') 
		{
			p_tmp++;
		}
		
		int sps_base64_len = (int)(p_tmp - p_substr);
		if (sps_base64_len < max_len)
		{
			memcpy(p_sps_pps, p_substr, sps_base64_len);
			p_sps_pps[sps_base64_len] = '\0';
			ret = TRUE;
		}
	}

	return ret;
}

BOOL rua_get_sdp_h265_desc(RCUA * p_rua, int * pt, char * p_sdp, int max_len)
{
    return rua_get_sdp_video_desc(p_rua, "H265/90000", pt, p_sdp, max_len);
}

BOOL rua_get_sdp_h265_params(RCUA * p_rua, int * pt, BOOL * donfield, char * p_vps, int vps_len, char * p_sps, int sps_len, char * p_pps, int pps_len)
{
    BOOL ret = FALSE;
    char sdp[1024] = {'\0'};
    
    if (rua_get_sdp_h265_desc(p_rua, pt, sdp, sizeof(sdp)) == FALSE)
    {
        return FALSE;
    }
    
    char * p_substr = strstr(sdp, "sprop-depack-buf-nalus=");
	if (p_substr != NULL)
	{
		p_substr += strlen("sprop-depack-buf-nalus=");
		char * p_tmp = p_substr;

		while (*p_tmp != ' ' && *p_tmp != ';' && *p_tmp != '\0')
		{
			p_tmp++;
		}
		
		int len = (int)(p_tmp - p_substr);
		if (len > 0)
		{
			if (donfield)
			{
			    *donfield = (atoi(p_substr) > 0 ? TRUE : FALSE);
			}    
		}
		else
		{
			return FALSE;
		}
	}
	
	p_substr = strstr(sdp, "sprop-vps=");
	if (p_substr != NULL)
	{
		p_substr += strlen("sprop-vps=");
		char * p_tmp = p_substr;

		while (*p_tmp != ' ' && *p_tmp != ';' && *p_tmp != '\0')
		{
			p_tmp++;
		}
		
		int len = (int)(p_tmp - p_substr);
		if (len < vps_len)
		{
			memcpy(p_vps, p_substr, len);
			p_vps[len] = '\0';
		}
		else
		{
			return FALSE;
		}
	}

	p_substr = strstr(sdp, "sprop-sps=");
	if (p_substr != NULL)
	{
		p_substr += strlen("sprop-sps=");
		char * p_tmp = p_substr;

		while (*p_tmp != ' ' && *p_tmp != ';' && *p_tmp != '\0')
		{
			p_tmp++;
		}
		
		int len = (int)(p_tmp - p_substr);
		if (len < sps_len)
		{
			memcpy(p_sps, p_substr, len);
			p_sps[len] = '\0';
		}
		else
		{
			return FALSE;
		}
	}

	p_substr = strstr(sdp, "sprop-pps=");
	if (p_substr != NULL)
	{
		p_substr += strlen("sprop-pps=");
		char * p_tmp = p_substr;

		while (*p_tmp != ' ' && *p_tmp != ';' && *p_tmp != '\0')
		{
			p_tmp++;
		}
		
		int len = (int)(p_tmp - p_substr);
		if (len < pps_len)
		{
			memcpy(p_pps, p_substr, len);
			p_pps[len] = '\0';
		}
		else
		{
			return FALSE;
		}
	}

	return TRUE;
}

BOOL rua_get_sdp_mp4_desc(RCUA * p_rua, int * pt, char * p_sdp, int max_len)
{
    return rua_get_sdp_video_desc(p_rua, "MP4V-ES/90000", pt, p_sdp, max_len);
}

BOOL rua_get_sdp_mp4_params(RCUA * p_rua, int * pt, char * p_cfg, int max_len)
{
    BOOL ret = FALSE;
    char sdp[1024] = {'\0'};
    
    if (rua_get_sdp_mp4_desc(p_rua, pt, sdp, sizeof(sdp)) == FALSE)
    {
        return FALSE;
    }
    
    char * p_substr = strstr(sdp, "config=");
    if (p_substr != NULL)
    {
    	p_substr += strlen("config=");
    	char * p_tmp = p_substr;

    	while (*p_tmp != ' ' && *p_tmp != ';' && *p_tmp != '\0')
    	{
    		p_tmp++;
    	}
    	
    	int cfg_len = (int)(p_tmp - p_substr);
    	if (cfg_len < max_len)
    	{
    		memcpy(p_cfg, p_substr, cfg_len);
    		p_cfg[cfg_len] = '\0';
    		ret = TRUE;
    	}
    	else
    	{
    		ret = FALSE;
    	}
    }

    return ret;
}

BOOL rua_get_sdp_aac_desc(RCUA * p_rua, int * pt, char * p_sdp, int max_len)
{
    return rua_get_sdp_audio_desc(p_rua, "MPEG4-GENERIC", pt, p_sdp, max_len);
}

BOOL rua_get_sdp_aac_params(RCUA * p_rua, int *pt, int *sizelength, int *indexlength, int *indexdeltalength, char * p_cfg, int max_len)
{
    BOOL ret = FALSE;
    char sdp[1024] = {'\0'};
    
    if (rua_get_sdp_aac_desc(p_rua, pt, sdp, sizeof(sdp)) == FALSE)
    {
        return FALSE;
    }

    char * p_substr = strstr(sdp, "config=");
	if (p_substr != NULL)
	{
		p_substr += strlen("config=");
		char * p_tmp = p_substr;

		while (*p_tmp != ' ' && *p_tmp != ';' && *p_tmp != '\0')
		{
			p_tmp++;
		}
		
		int len = (int)(p_tmp - p_substr);
		if (len < max_len)
		{
			memcpy(p_cfg, p_substr, len);
			p_cfg[len] = '\0';
		}
		else
		{
		    return FALSE;
		}
	}

	p_substr = strstr(sdp, "sizelength=");
	if (p_substr != NULL)
	{
		p_substr += strlen("sizelength=");
		char * p_tmp = p_substr;

		while (*p_tmp != ' ' && *p_tmp != ';' && *p_tmp != '\0')
		{
			p_tmp++;
		}
		
		int len = (int)(p_tmp - p_substr);
		if (len > 0)
		{
			*sizelength = atoi(p_substr);
		}
	}

	p_substr = strstr(sdp, "indexlength=");
	if (p_substr != NULL)
	{
		p_substr += strlen("indexlength=");
		char * p_tmp = p_substr;

		while (*p_tmp != ' ' && *p_tmp != ';' && *p_tmp != '\0')
		{
			p_tmp++;
		}
		
		int len = (int)(p_tmp - p_substr);
		if (len > 0)
		{
			*indexlength = atoi(p_substr);
		}
	}

	p_substr = strstr(sdp, "indexdeltalength=");
	if (p_substr != NULL)
	{
		p_substr += strlen("indexdeltalength=");
		char * p_tmp = p_substr;

		while (*p_tmp != ' ' && *p_tmp != ';' && *p_tmp != '\0')
		{
			p_tmp++;
		}
		
		int len = (int)(p_tmp - p_substr);
		if (len > 0)
		{
			*indexdeltalength = atoi(p_substr);
		}
	}

    return TRUE;
}

BOOL rua_init_udp_connection(RCUA * p_rua, int av_t)
{
    uint16 port;
    int try_cnt = 0;
	int addr_len;
	struct sockaddr_in addr;

	SOCKET fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd <= 0)
	{
		log_print(HT_LOG_ERR, "%s, socket SOCK_DGRAM error!\n", __FUNCTION__);
		return FALSE;
	}

	addr_len = sizeof(addr);
    port = rtsp_get_udp_port();
    
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);

	while (try_cnt < 3)
	{
    	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    	{
    		log_print(HT_LOG_ERR, "%s, bind udp socket fail,error = %s\r\n", __FUNCTION__, sys_os_get_socket_error());

    		try_cnt++;
    		port = rtsp_get_udp_port();
    		addr.sin_port = htons(port);
    	}
    	else
    	{
    	    break;
    	}
	}

	if (try_cnt == 3)
	{
	    closesocket(fd);
		return FALSE;
	}

	int len = 1024*1024;
	if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char*)&len, sizeof(int)))
	{
		log_print(HT_LOG_ERR, "%s, setsockopt SO_RCVBUF error!\r\n", __FUNCTION__);
	}

    p_rua->channels[av_t].udp_fd = fd;
    p_rua->channels[av_t].l_port = port;

	return TRUE;
}

BOOL rua_init_mc_connection(RCUA * p_rua, int av_t)
{
    uint16 port;
    char destination[32];
    struct sockaddr_in addr;
    struct ip_mreq mcast;

    port = p_rua->channels[av_t].l_port = p_rua->channels[av_t].r_port;

    strcpy(destination, p_rua->channels[av_t].destination);
    
#ifdef BACKCHANNEL
	if (AV_TYPE_BACKCHANNEL == av_t) // backchannel
	{
		port = 0;   // just for data sending
	}
#endif
    
	SOCKET fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd <= 0)
	{
		log_print(HT_LOG_ERR, "%s, socket SOCK_DGRAM error!\n", __FUNCTION__);
		return FALSE;
	}

    addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = get_default_if_ip();
	addr.sin_port = htons(port);

	/* reuse socket addr */
	int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt))) 
    {  
        log_print(HT_LOG_WARN, "%s, setsockopt SO_REUSEADDR error!\r\n", __FUNCTION__);
    }

    int len = 1024*1024;
	if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char*)&len, sizeof(int)))
	{
		log_print(HT_LOG_ERR, "%s, setsockopt SO_RCVBUF error!\r\n", __FUNCTION__);
	}
    
	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
	{
		log_print(HT_LOG_ERR, "%s, Bind udp socket fail,error = %s\r\n", __FUNCTION__, sys_os_get_socket_error());
		closesocket(fd);
		return FALSE;
	}

    int ttl = 255;
    setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&ttl, sizeof(ttl));
    
	mcast.imr_multiaddr.s_addr = inet_addr(destination);
	mcast.imr_interface.s_addr = get_default_if_ip();

	if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mcast, sizeof(mcast)) < 0)
	{
		log_print(HT_LOG_ERR, "%s, setsockopt IP_ADD_MEMBERSHIP error!%s\r\n", __FUNCTION__, sys_os_get_socket_error());
		closesocket(fd);
		return FALSE;
	}

    p_rua->channels[av_t].udp_fd = fd;

	return TRUE;
}


