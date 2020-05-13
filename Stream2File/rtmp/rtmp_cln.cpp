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
#include "rtmp_cln.h"
#include "h264.h"
#include "media_format.h"


void * rtmp_rx_thread(void * argv)
{
    CRtmpClient * pRtmpClient = (CRtmpClient *) argv;

    pRtmpClient->rx_thread();

    return NULL;
}

CRtmpClient::CRtmpClient()
: m_pRtmp(NULL)
, m_bRunning(FALSE)
, m_bPause(FALSE)
, m_tidRx(0)
, m_bH264ParamSets(FALSE)
, m_nAudioConfigLen(0)
, m_nVideoCodec(VIDEO_CODEC_NONE)
, m_nAudioCodec(AUDIO_CODEC_NONE)
, m_nSamplerate(0)
, m_nChannels(0)
, m_pNotify(NULL)
, m_pUserdata(NULL)
, m_pVideoCB(NULL)
, m_pAudioCB(NULL)
{
    memset(m_url, 0, sizeof(m_url));
    memset(m_user, 0, sizeof(m_user));
    memset(m_pass, 0, sizeof(m_pass));

    memset(&m_h26PparamSets, 0, sizeof(m_h26PparamSets));
    memset(&m_pAudioConfig, 0, sizeof(m_pAudioConfig));
	
	m_pMutex = sys_os_create_mutex();
}

CRtmpClient::~CRtmpClient()
{
    rtmp_close();

    if (m_pMutex)
    {
        sys_os_destroy_sig_mutex(m_pMutex);
        m_pMutex = NULL;
    }
}

BOOL CRtmpClient::rtmp_start(const char * url, const char * user, const char * pass)
{
    if (url)
    {
        strncpy(m_url, url, sizeof(m_url));
    }

    if (user)
    {
        strncpy(m_user, user, sizeof(m_user));
    }

    if (pass)
    {
        strncpy(m_pass, pass, sizeof(m_pass));
    }
    
    m_bRunning = TRUE;
    m_tidRx = sys_os_create_thread((void *)rtmp_rx_thread, this);
    
    return TRUE;
}

BOOL CRtmpClient::rtmp_play()
{
    if (m_pRtmp && m_bPause)
    {
        RTMP_Pause(m_pRtmp, 0);
        m_bPause = 0;
    }

    return TRUE;
}

BOOL CRtmpClient::rtmp_stop()
{
    return rtmp_close();
}

BOOL CRtmpClient::rtmp_pause()
{
    if (m_pRtmp && !m_bPause)
    {
        RTMP_Pause(m_pRtmp, 1);
        m_bPause = 1;
    }
    
    return TRUE;
}

BOOL CRtmpClient::rtmp_close()
{
    sys_os_mutex_enter(m_pMutex);
	m_pAudioCB = NULL;
	m_pVideoCB = NULL;
	m_pNotify = NULL;
	m_pUserdata = NULL;
	sys_os_mutex_leave(m_pMutex);
	
    m_bRunning = FALSE;

    if (m_pRtmp && m_pRtmp->m_sb.sb_socket)
    {
        closesocket(m_pRtmp->m_sb.sb_socket);
    }
    
    while (m_tidRx)
    {
        usleep(10*1000);
    }
    
    if (m_pRtmp)
    {
        RTMP_Close(m_pRtmp);
        RTMP_Free(m_pRtmp);

        m_pRtmp = NULL;
    }
    
    return TRUE;
}

BOOL CRtmpClient::rtmp_connect()
{
    rtmp_send_notify(RTMP_EVE_CONNECTING);
    
    m_pRtmp = RTMP_Alloc();
    if (NULL == m_pRtmp)
    {
        log_print(HT_LOG_ERR, "%s, RTMP_Alloc failed\r\n", __FUNCTION__);
        goto FAILED;
    }

    RTMP_Init(m_pRtmp);

    // set connection timeout,default 30s
    m_pRtmp->Link.timeout = 30;
		
    if (RTMP_SetupURL(m_pRtmp, m_url) == FALSE)
    {
        log_print(HT_LOG_ERR, "%s, RTMP_SetupURL failed. url=%s\r\n", __FUNCTION__, m_url);
        goto FAILED;
    }

    m_pRtmp->Link.lFlags |= RTMP_LF_LIVE;
    
    RTMP_SetBufferMS(m_pRtmp, DEF_BUFTIME);

    if (!RTMP_Connect(m_pRtmp, NULL))
    {
        log_print(HT_LOG_ERR, "%s, RTMP_Connect failed. url=%s\r\n", __FUNCTION__, m_url);
        goto FAILED;
    }

    if (!RTMP_ConnectStream(m_pRtmp, 0))
    {
        log_print(HT_LOG_ERR, "%s, RTMP_ConnectStream failed. url=%s\r\n", __FUNCTION__, m_url);
        goto FAILED;
    }

    return TRUE;

FAILED: 

    if (m_pRtmp)
    {
        RTMP_Close(m_pRtmp);
        RTMP_Free(m_pRtmp);

        m_pRtmp = NULL;
    }
    
    return FALSE;
}

int CRtmpClient::rtmp_h264_rx(RTMPPacket * packet)
{
    uint8 * data = (uint8 *) packet->m_body;

    uint8 frametype = (data[0] >> 4);
    BOOL bkeyframe = FALSE;

    if (frametype == 1)         // keyframe
    {
        bkeyframe = TRUE;
    }
    else if (frametype == 2)    // inter frame
    {
        bkeyframe = FALSE;
    }

	if (data[1] == 0x00)   // AVC sequence header
	{
	    int spsnum = (data[10] & 0x1f);
        int number_sps = 11;
        int count_sps = 1;
        
        while (count_sps <= spsnum)
        {
            int spslen = (data[number_sps] & 0x000000FF) << 8 | (data[number_sps + 1] & 0x000000FF);
            if (spslen > sizeof(m_h26PparamSets.sps) - 4)
            {
                log_print(HT_LOG_WARN, "%s, spslen = %d\r\n", spslen);
                
                spslen = sizeof(m_h26PparamSets.sps) - 4;
            }
            
            number_sps += 2;

            if (1 == count_sps)
            {
                m_h26PparamSets.sps[0] = 0;
                m_h26PparamSets.sps[1] = 0;
                m_h26PparamSets.sps[2] = 0;
                m_h26PparamSets.sps[3] = 1;
                m_h26PparamSets.sps_len = spslen + 4;
                memcpy(m_h26PparamSets.sps + 4, data + number_sps, spslen);
            }
            
            number_sps += spslen;
            count_sps++;
        }


        int ppsnum = (data[number_sps] & 0x1f);
        int number_pps = number_sps + 1;
        int count_pps = 1;
        
        while (count_pps <= ppsnum)
        {
            int ppslen = (data[number_pps] & 0x000000FF) << 8 | (data[number_pps + 1] & 0x000000FF);
            if (ppslen > sizeof(m_h26PparamSets.pps) - 4)
            {
                log_print(HT_LOG_WARN, "%s, spslen = %d\r\n", ppslen);
                
                ppslen = sizeof(m_h26PparamSets.pps) - 4;
            }
            
            number_pps += 2;
            
            if (1 == count_pps)
            {
                m_h26PparamSets.pps[0] = 0;
                m_h26PparamSets.pps[1] = 0;
                m_h26PparamSets.pps[2] = 0;
                m_h26PparamSets.pps[3] = 1;
                m_h26PparamSets.pps_len = ppslen + 4;
                memcpy(m_h26PparamSets.pps + 4, data + number_pps, ppslen);
            }
            
            number_pps += ppslen;
            count_pps++;
        }

        if (!m_bH264ParamSets && m_h26PparamSets.pps_len > 0 && m_h26PparamSets.sps_len > 0)
        {
            m_bH264ParamSets = TRUE;

            rtmp_send_notify(RTMP_EVE_VIDEOREADY);

            rtmp_video_data_cb(m_h26PparamSets.sps, m_h26PparamSets.sps_len, 0);
            rtmp_video_data_cb(m_h26PparamSets.pps, m_h26PparamSets.pps_len, 0);
        }
	}
	else if (data[1] == 0x01)	// AVC NALU
	{
	    int len = 0;
        int num = 5;

        while (num < (int)packet->m_nBodySize)
        {
            len = (data[num] & 0x000000FF) << 24 | (data[num + 1] & 0x000000FF) << 16 | 
                  (data[num + 2] & 0x000000FF) << 8 | (data[num + 3] & 0x000000FF);

            num = num + 4;

            data[num-4]= 0;
            data[num-3]= 0;
            data[num-2]= 0;
            data[num-1]= 1;

            rtmp_video_data_cb(data + num - 4, len + 4, packet->m_nTimeStamp);
            
            num = num + len;
        }
	}
	else
	{
		log_print(HT_LOG_WARN, "%s, tag type [%02x]!!!\r\n", __FUNCTION__, data[1]);
	}

	return 0;
}

int CRtmpClient::rtmp_video_rx(RTMPPacket * packet)
{
    int ret = 0;
    uint8 v_type = (uint8) packet->m_body[0];
	v_type = (v_type & 0x0f);

    if (v_type == 2)        // Sorenson H.263
    {
    }
    else if (v_type == 3)   // Screen video
    {
    }
    else if (v_type == 4)   // On2 VP6
    {
    }
    else if (v_type == 5)   // On2 VP6 with alpha channel
    {
    }
    else if (v_type == 6)   // Screen video version 2
    {
    }
    else if (v_type == 7)   // AVC
    {
        m_nVideoCodec = VIDEO_CODEC_H264;

        ret = rtmp_h264_rx(packet);
    }

	return ret;
}

int CRtmpClient::rtmp_aac_rx(RTMPPacket * packet)
{
	uint8 * data = (uint8 *)packet->m_body;

    if (packet->m_nBodySize < 4)
    {
        return -1;
    }
    
	// AAC Packet Type: 0x00 - AAC Sequence Header; 0x01 - AAC Raw
	if (data[1] == 0x00)
	{
	    int nAudioSampleType;
	    
		log_print(HT_LOG_DBG, "%s, len[%u] data[%02X %02X %02X %02X]\r\n",
			__FUNCTION__, packet->m_nBodySize, data[0], data[1], data[2], data[3]);

		memcpy(m_pAudioConfig, data+2, 2);
		m_nAudioConfigLen = 2;
		m_nChannels = ((data[0] & 0x01) ? 2 : 1);   // Whether stereo (0: Mono, 1: Stereo)
        nAudioSampleType = (data[0] & 0x0c) >> 2;   // Audio sample rate (0: 5.5kHz, 1:11KHz, 2:22 kHz, 3:44 kHz)
        
        switch (nAudioSampleType)
        {
        case 0:
            m_nSamplerate = 5500;
            break;

        case 1:
            m_nSamplerate = 11025;
            break;

        case 2:
            m_nSamplerate = 22050;
            break; 

        case 3:
            m_nSamplerate = 44100;
            break;

        default:
            m_nSamplerate = 44100;
            break;
        }

        uint16 audioSpecificConfig = 0;	
        
        audioSpecificConfig = (data[2] & 0xff) << 8;
        audioSpecificConfig += 0x00ff & data[3];
        
        uint8 nAudioObjectType = (audioSpecificConfig & 0xF800) >> 11;	
        uint8 nSampleFrequencyIndex = (audioSpecificConfig & 0x0780) >> 7;
        uint8 nChannels = (audioSpecificConfig & 0x78) >> 3;
        uint8 nFrameLengthFlag = (audioSpecificConfig & 0x04) >> 2;
        uint8 nDependOnCoreCoder = (audioSpecificConfig & 0x02) >> 1;
        uint8 nExtensionFlag = audioSpecificConfig & 0x01;

	    m_nChannels = nChannels;

	    switch (nSampleFrequencyIndex)
	    {
	    case 0:
            m_nSamplerate = 96000;
            break;

        case 1:
            m_nSamplerate = 88200;
            break;
            
        case 2:
            m_nSamplerate = 64000;
            break;
            
        case 3:
            m_nSamplerate = 48000;
            break;
            
        case 4:
            m_nSamplerate = 44100;
            break;
            
        case 5:
            m_nSamplerate = 32000;
            break;
            
        case 6:
            m_nSamplerate = 24000;
            break;

        case 7:
            m_nSamplerate = 22050;
            break;

        case 8:
            m_nSamplerate = 16000;
            break;     

        case 9:
            m_nSamplerate = 12000;
            break;

        case 10:
            m_nSamplerate = 11025;
            break;

        case 11:
            m_nSamplerate = 8000;
            break;

        case 12:
            m_nSamplerate = 7350;
            break;
            
        default:
            m_nSamplerate = 44100;
            break;
	    }
	    
        rtmp_send_notify(RTMP_EVE_AUDIOREADY);
	}
	else if (data[1] == 0x01)
	{
	    rtmp_audio_data_cb(data + 2, packet->m_nBodySize - 2, packet->m_nTimeStamp);
	}

	return 0;
}

int CRtmpClient::rtmp_audio_rx(RTMPPacket * packet)
{
    int ret = 0;
	uint8 a_type = (uint8) packet->m_body[0];
	a_type = a_type >> 4;
	
	if (a_type == 0)	    // Linear PCM, platform endian
	{
	}
	else if (a_type == 1)	//  ADPCM
	{
	}
	else if (a_type == 2)   // MP3
	{
	}
	else if (a_type == 3)   // Linear PCM, little endian
	{
	}
	else if (a_type == 7)   // G711A
	{
	    m_nAudioCodec = AUDIO_CODEC_G711A;
	}
	else if (a_type == 8)   // G711U
	{
	    m_nAudioCodec = AUDIO_CODEC_G711U;
	}
	else if (a_type == 10)	// AAC
	{
	    m_nAudioCodec = AUDIO_CODEC_AAC;
	    
		ret = rtmp_aac_rx(packet);
	}
	else if (a_type == 11)	// Speex
	{
	}
	else if (a_type == 14)	// MP3 8-Khz
	{
	}
	
	return ret;
}

int CRtmpClient::rtmp_metadata_rx(RTMPPacket * packet)
{
    AMFObject obj;	
    AVal val;	
    AMFObjectProperty * property;	
    AMFObject subObject;	
    
    int nRes = AMF_Decode(&obj, packet->m_body, packet->m_nBodySize, FALSE);	
    if (nRes < 0)	
    {		
        log_print(HT_LOG_ERR, "%s, error decoding invoke packet\r\n", __FUNCTION__);
        return -1;	
    } 	

    AMF_Dump(&obj);

    for (int n = 0; n < obj.o_num; n++)
    {
        property = AMF_GetProp(&obj, NULL, n);
        if (NULL == property)
        {
            continue;
        }

        if (property->p_type == AMF_OBJECT)
        {				
            AMFProp_GetObject(property, &subObject);
            
            for (int m = 0; m < subObject.o_num; m++)
            {					
                property = AMF_GetProp(&subObject, NULL, m);					
                if (NULL == property)
                {
                    continue;
                }
                
                if (property->p_type == AMF_OBJECT)						
                { 						
                }						
                else if (property->p_type == AMF_BOOLEAN)						
                {							
                    int bVal = AMFProp_GetBoolean(property);							
                    if (strncmp("stereo", property->p_name.av_val, property->p_name.av_len) == 0)							
                    {								
                        m_nChannels = bVal > 0 ? 2 : 1;							
                    }						
                }						
                else if (property->p_type == AMF_NUMBER)						
                {							
                    double dVal = AMFProp_GetNumber(property);							
                    if (strncmp("width", property->p_name.av_val, property->p_name.av_len) == 0)							
                    {								
                        //nVideoWidth = (int)dVal;							
                    }							
                    else if (strcasecmp("height", property->p_name.av_val) == 0)							
                    {								
                        //nVideoHeight = (int)dVal;							
                    }							
                    else if (strcasecmp("framerate", property->p_name.av_val) == 0)							
                    {								
                        //nFrameRate = (int)dVal;							
                    }							
                    else if (strcasecmp("videocodecid", property->p_name.av_val) == 0)							
                    {								
                        //nVideoCodecId = (int)dVal;							
                    }							
                    else if (strcasecmp("audiosamplerate", property->p_name.av_val) == 0)							
                    {								
                        m_nSamplerate = (int)dVal;							
                    }							
                    else if (strcasecmp("audiosamplesize", property->p_name.av_val) == 0)							
                    {								
                        //nAudioSampleSize = (int)dVal;							
                    }							
                    else if (strcasecmp("audiocodecid", property->p_name.av_val) == 0)							
                    {								
                        //nAudioCodecId = (int)dVal;							
                    }							
                    else if (strcasecmp("filesize", property->p_name.av_val) == 0)							
                    {								
                        //nFileSize = (int)dVal;							
                    }						
                }
                else if (property->p_type == AMF_STRING)						
                {							
                    AMFProp_GetString(property, &val);						
                } 					
            }			
        }			
        else			
        {				
            AMFProp_GetString(property, &val);			
        }
    }	

    AMF_Reset(&obj);
    
	return 0;
}

void CRtmpClient::rx_thread()
{
    int ret = 0;
    
    if (!rtmp_connect())
    {
        m_tidRx = 0;
        rtmp_send_notify(RTMP_EVE_CONNFAIL);
        return;
    }

    RTMPPacket pc = { 0 };

    while (m_bRunning)
    {
        if (RTMP_ReadPacket(m_pRtmp, &pc) && RTMPPacket_IsReady(&pc))
        {
            if (!pc.m_nBodySize)
            {
                continue;
            }

            if (pc.m_packetType == RTMP_PACKET_TYPE_VIDEO && RTMP_ClientPacket(m_pRtmp, &pc))
            {
                ret = rtmp_video_rx(&pc);
            }
            else if (pc.m_packetType == RTMP_PACKET_TYPE_AUDIO && RTMP_ClientPacket(m_pRtmp, &pc))
            {
                ret = rtmp_audio_rx(&pc);
            }
            else if (pc.m_packetType == RTMP_PACKET_TYPE_INFO && RTMP_ClientPacket(m_pRtmp, &pc))
            {
                rtmp_metadata_rx(&pc);
            }

            RTMPPacket_Free(&pc);

            if (ret < 0)
            {
                log_print(HT_LOG_ERR, "%s, ret = %d\r\n", __FUNCTION__, ret);
                break;
            }
        }
    }
    
    m_tidRx = 0;

    rtmp_send_notify(RTMP_EVE_STOPPED);
}

void CRtmpClient::rtmp_send_notify(int event)
{
	sys_os_mutex_enter(m_pMutex);
	
	if (m_pNotify)
	{
		m_pNotify(event, m_pUserdata);
	}

	sys_os_mutex_leave(m_pMutex);
}

void CRtmpClient::rtmp_video_data_cb(uint8 * p_data, int len, uint32 ts)
{
	sys_os_mutex_enter(m_pMutex);
	if (m_pVideoCB)
	{
		m_pVideoCB(p_data, len, ts, m_pUserdata);
	}
	sys_os_mutex_leave(m_pMutex);
}

void CRtmpClient::rtmp_audio_data_cb(uint8 * p_data, int len, uint32 ts)
{
	sys_os_mutex_enter(m_pMutex);
	if (m_pAudioCB)
	{
		m_pAudioCB(p_data, len, ts, m_pUserdata);
	}
	sys_os_mutex_leave(m_pMutex);
}

void CRtmpClient::get_h264_params()
{
    if (m_h26PparamSets.sps_len > 0)
    {
        rtmp_video_data_cb(m_h26PparamSets.sps, m_h26PparamSets.sps_len, 0);
    }

    if (m_h26PparamSets.pps_len > 0)
    {
        rtmp_video_data_cb(m_h26PparamSets.pps, m_h26PparamSets.pps_len, 0);
    }
}

BOOL CRtmpClient::get_h264_params(uint8 * p_sps, int * sps_len, uint8 * p_pps, int * pps_len)
{
    if (m_h26PparamSets.sps_len > 0)
    {
        *sps_len = m_h26PparamSets.sps_len;
        memcpy(p_sps, m_h26PparamSets.sps, m_h26PparamSets.sps_len);
    }

    if (m_h26PparamSets.pps_len > 0)
    {
        *pps_len = m_h26PparamSets.pps_len;
        memcpy(p_pps, m_h26PparamSets.pps, m_h26PparamSets.pps_len);
    }

    return TRUE;
}

void CRtmpClient::set_notify_cb(rtmp_notify_cb notify, void * userdata) 
{
	sys_os_mutex_enter(m_pMutex);	
	m_pNotify = notify;
	m_pUserdata = userdata;
	sys_os_mutex_leave(m_pMutex);
}

void CRtmpClient::set_video_cb(rtmp_video_cb cb) 
{
	sys_os_mutex_enter(m_pMutex);
	m_pVideoCB = cb;
	sys_os_mutex_leave(m_pMutex);
}

void CRtmpClient::set_audio_cb(rtmp_audio_cb cb)
{
	sys_os_mutex_enter(m_pMutex);
	m_pAudioCB = cb;
	sys_os_mutex_leave(m_pMutex);
}


