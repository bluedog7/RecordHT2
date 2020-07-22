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
#include "r2f.h"
#include "r2f_cfg.h"
#include "r2f_rua.h"
#include "avi_write.h"
#include "media_util.h"
#include "rtsp_util.h"
#ifdef MP4_FORMAT
#include "mp4_write.h"
#endif
#ifdef RTMP_STREAM
#include "log.h"
#include "rtmp_cln.h"
#endif
#include "mysql.h"
#include <cstring>
#include <thread>
using namespace std;

#pragma comment(lib,"libmysql.lib")
/***************************************************************************************/
#define R2F_MAJOR_VERSION   2
#define R2F_MINOR_VERSION   4

/***************************************************************************************/

R2F_CLS g_r2f_cls;

/***************************************************************************************/

#if defined(MP4_FORMAT) && defined(AUDIO_CONV)

void r2f_audio_decode_cb(AVFrame * frame, void *pUserdata)
{
    RUA * p_rua = (RUA *)pUserdata;
    if (p_rua->aencoder)
    {
        p_rua->aencoder->encode(frame);
    }
}

void r2f_audio_encode_cb(uint8 *data, int size, int nbsamples, void *pUserdata)
{
	RUA * p_rua = (RUA *)pUserdata;

	r2f_record_aac(p_rua, data, size);
}

BOOL r2f_init_audio_recodec(RUA * p_rua, int codec, int sr, int chs)
{
    if (p_rua->adecoder)
    {
        delete p_rua->adecoder;
        p_rua->adecoder = NULL;
    }

    if (p_rua->aencoder)
    {
        delete p_rua->aencoder;
        p_rua->aencoder = NULL;
    }
    
    p_rua->adecoder = new CAudioDecoder();
    if (p_rua->adecoder)
    {
        if (p_rua->adecoder->init(codec, sr, chs, NULL, 0))
        {
            p_rua->adecoder->setCallback(r2f_audio_decode_cb, p_rua);
        }
        else
        {
            log_print(HT_LOG_ERR, "%s, audio decoder init failed\r\n", __FUNCTION__);

            delete p_rua->adecoder;
            p_rua->adecoder = NULL;
            return FALSE;
        }
    }
    else
    {
        log_print(HT_LOG_ERR, "%s, new audio decoder failed\r\n", __FUNCTION__);
        return FALSE;
    }

    p_rua->aencoder = new CAudioEncoder();
    if (p_rua->aencoder)
    {
        AudioEncoderParam param;
        memset(&param, 0, sizeof(param));

        param.SrcSamplerate = sr;
        param.SrcChannels = chs;
        param.SrcSamplefmt = AV_SAMPLE_FMT_S16;
        param.DstCodec = AUDIO_CODEC_AAC;
        param.DstSamplerate = sr;
        param.DstChannels = chs;
        param.DstBitrate = 0;
        param.DstSamplefmt = AV_SAMPLE_FMT_S16;
        
        if (p_rua->aencoder->init(&param))
        {
            p_rua->aencoder->addCallback(r2f_audio_encode_cb, p_rua);
        }
        else
        {
            log_print(HT_LOG_ERR, "%s, audio encoder init failed\r\n", __FUNCTION__);

            delete p_rua->adecoder;
            p_rua->adecoder = NULL;

            delete p_rua->aencoder;
            p_rua->aencoder = NULL;
            
            return FALSE;
        }
    }
    else
    {
        log_print(HT_LOG_ERR, "%s, new audio encoder failed\r\n", __FUNCTION__);

        delete p_rua->adecoder;
        p_rua->adecoder = NULL;
        return FALSE;
    }

    return TRUE;
}

#endif

#ifdef VIDEO_CONV

void r2f_video_decode_cb(AVFrame * frame, void *pUserdata)
{
    RUA * p_rua = (RUA *)pUserdata;
    if (p_rua->vencoder)
    {
        p_rua->vencoder->encode(frame);
    }
}

void r2f_video_encode_cb(uint8 *data, int size, void *pUserdata)
{
	RUA * p_rua = (RUA *)pUserdata;

	r2f_record_video_ex(p_rua, data, size, VIDEO_CODEC_H264);
}

BOOL r2f_init_video_recodec(RUA * p_rua, int codec, int w, int h, int fps)
{
    if (p_rua->vdecoder)
    {
        delete p_rua->vdecoder;
        p_rua->vdecoder = NULL;
    }

    if (p_rua->vencoder)
    {
        delete p_rua->vencoder;
        p_rua->vencoder = NULL;
    }
    
    p_rua->vdecoder = new CVideoDecoder();
    if (p_rua->vdecoder)
    {
        if (p_rua->vdecoder->init(codec))
        {
            p_rua->vdecoder->setCallback(r2f_video_decode_cb, p_rua);
        }
        else
        {
            log_print(HT_LOG_ERR, "%s, video decoder init failed\r\n", __FUNCTION__);

            delete p_rua->vdecoder;
            p_rua->vdecoder = NULL;
            return FALSE;
        }
    }
    else
    {
        log_print(HT_LOG_ERR, "%s, new video decoder failed\r\n", __FUNCTION__);
        return FALSE;
    }

    p_rua->vencoder = new CVideoEncoder();
    if (p_rua->vencoder)
    {
        VideoEncoderParam param;
        memset(&param, 0, sizeof(param));

        param.SrcWidth = w;
        param.SrcHeight = h;
        param.SrcPixFmt = AV_PIX_FMT_YUV420P;
        param.DstCodec = VIDEO_CODEC_H264;
        param.DstWidth = w;
        param.DstHeight = h;
        param.DstFramerate = fps > 0 ? fps : 25;
        
        if (p_rua->vencoder->init(&param))
        {
            p_rua->vencoder->addCallback(r2f_video_encode_cb, p_rua);
        }
        else
        {
            log_print(HT_LOG_ERR, "%s, video encoder init failed\r\n", __FUNCTION__);

            delete p_rua->vdecoder;
            p_rua->vdecoder = NULL;

            delete p_rua->vencoder;
            p_rua->vencoder = NULL;
            
            return FALSE;
        }
    }
    else
    {
        log_print(HT_LOG_ERR, "%s, new video encoder failed\r\n", __FUNCTION__);

        delete p_rua->vdecoder;
        p_rua->vdecoder = NULL;
        return FALSE;
    }

    return TRUE;
}

#endif

#ifdef RTMP_STREAM

void rtmp_log_callback(int level, const char *format, va_list vl)
{
	int lvl = HT_LOG_DBG;
	int offset = 0;
	char str[2048] = "";

	offset += vsnprintf(str, 2048-1, format, vl);

	offset += snprintf(str+offset, 2048-1-offset, "\r\n");
	
	if (level == RTMP_LOGCRIT)
	{
		lvl = HT_LOG_FATAL;
	}
	else if (level == RTMP_LOGERROR)
	{
		lvl = HT_LOG_ERR;
	}
	else if (level == RTMP_LOGWARNING)
	{
		lvl = HT_LOG_WARN;
	}
	else if (level == RTMP_LOGINFO)
	{
		lvl = HT_LOG_INFO;
	}
	else if (level == RTMP_LOGDEBUG)
	{
		lvl = HT_LOG_DBG;
	}

	log_print(level, str);
}

void rtmp_set_rtmp_log()
{
    RTMP_LogLevel lvl;
    
    RTMP_LogSetCallback(rtmp_log_callback);

    switch (g_r2f_cfg.log_level)
    {
    case HT_LOG_TRC:
        lvl = RTMP_LOGINFO; // RTMP_LOGALL;
        break;
        
    case HT_LOG_DBG:
        lvl = RTMP_LOGINFO; // RTMP_LOGDEBUG;
        break;

    case HT_LOG_INFO:
        lvl = RTMP_LOGINFO;
        break;

    case HT_LOG_WARN:
        lvl = RTMP_LOGWARNING;
        break;

    case HT_LOG_ERR:
        lvl = RTMP_LOGERROR;
        break;

    case HT_LOG_FATAL:
        lvl = RTMP_LOGCRIT;
        break;

    default:
        lvl = RTMP_LOGERROR;
        break;
    }

    RTMP_LogSetLevel(lvl);
}

int rtmp_audio_callback(uint8 * pdata, int len, uint32 ts, void *puser)
{
    // log_print(HT_LOG_DBG, "%s, len = %d, ts = %u, seq = %d\r\n", __FUNCTION__, len, ts, seq);

    return r2f_record_audio((RUA *)puser, pdata, len);
}

int rtmp_video_callback(uint8 * pdata, int len, uint32 ts, void *puser)
{
    // log_print(HT_LOG_DBG, "%s, len = %d, ts = %u, seq = %d\r\n", __FUNCTION__, len, ts, seq);

    return r2f_record_video((RUA *)puser, pdata, len);
}

void rtmp_reconn(RUA * p_rua)
{
    char url[256], user[64], pass[64];
    CRtmpClient * p_rtmp = p_rua->rtmp;

	strcpy(url, p_rtmp->get_url());
	strcpy(user, p_rtmp->get_user());
	strcpy(pass, p_rtmp->get_pass());

	log_print(HT_LOG_DBG, "%s, url = %s, user = %s, pass = %s\r\n", __FUNCTION__, url, user, pass);

	p_rtmp->rtmp_close();

	p_rtmp->set_notify_cb(r2f_notify_callback, p_rua);
    p_rtmp->set_audio_cb(rtmp_audio_callback);
    p_rtmp->set_video_cb(rtmp_video_callback);

	if (!p_rtmp->rtmp_start(url, user, pass))
	{
	    log_print(HT_LOG_ERR, "%s, rtsp_start failed. %s\r\n", __FUNCTION__, url);
	}
}

#endif // RTMP_STREAM

#ifdef MJPEG_STREAM

int mjpeg_video_callback(uint8 * pdata, int len, void *puser)
{
    return r2f_record_video((RUA *)puser, pdata, len);
}

void mjpeg_reconn(RUA * p_rua)
{
    char url[256], user[64], pass[64];
    CHttpMjpeg * p_mjpeg = p_rua->mjpeg;

	strcpy(url, p_mjpeg->get_url());
	strcpy(user, p_mjpeg->get_user());
	strcpy(pass, p_mjpeg->get_pass());

	log_print(HT_LOG_DBG, "%s, url = %s, user = %s, pass = %s\r\n", __FUNCTION__, url, user, pass);

	p_mjpeg->mjpeg_stop();

	p_mjpeg->set_notify_cb(r2f_notify_callback, p_rua);
    p_mjpeg->set_video_cb(mjpeg_video_callback);

	if (!p_mjpeg->mjpeg_start(url, user, pass))
	{
	    log_print(HT_LOG_ERR, "%s, rtsp_start failed. %s\r\n", __FUNCTION__, url);
	}
}

#endif // MJPEG_STREAM

int r2f_notify_callback(int event, void * puser)
{
    RIMG msg;
    RUA * p_rua = (RUA *)puser;
    
    log_print(HT_LOG_DBG, "%s, event = %d\r\n", __FUNCTION__, event);
    
	memset(&msg, 0, sizeof(RIMG));
	
	msg.msg_src = R2F_NTF_SRC;
	msg.msg_dua = rua_get_index(p_rua);
	msg.msg_evt = event;

    if (!hqBufPut(g_r2f_cls.msg_queue, (char *) &msg))
    {
    	log_print(HT_LOG_ERR, "%s, hqBufPut failed\r\n", __FUNCTION__);
    	return -1;
    }

    int vcodec = VIDEO_CODEC_NONE;
    int acodec = AUDIO_CODEC_NONE;
    int sr = 8000, ch = 1;
    int a_extra_len = 0, v_extra_len = 0;
    uint8 a_extra[128], v_extra[1024];
            
    if (RTSP_EVE_CONNSUCC == event)
	{
	    CRtspClient * p_rtsp = p_rua->rtsp;
	    vcodec = p_rtsp->video_codec();
	    acodec = p_rtsp->audio_codec();

	    sr = p_rtsp->get_audio_samplerate();
	    ch = p_rtsp->get_audio_channels();

        if (VIDEO_CODEC_H264 == vcodec)
        {
            uint8 sps[512], pps[512];
			int sps_len = 0, pps_len = 0;
            
            if (p_rtsp->get_h264_params(sps, &sps_len, pps, &pps_len))
            {
                memcpy(v_extra, sps, sps_len);
	            memcpy(v_extra + sps_len, pps, pps_len);

	            v_extra_len = sps_len + pps_len;
	        }
        }
        else if (VIDEO_CODEC_H265 == vcodec)
        {
            uint8 vps[512], sps[512], pps[512];
            int vps_len = 0, sps_len = 0, pps_len = 0;
                
            if (p_rtsp->get_h265_params(sps, &sps_len, pps, &pps_len, vps, &vps_len))
            {
                memcpy(v_extra, vps, vps_len);
	            memcpy(v_extra + vps_len, sps, sps_len);
	            memcpy(v_extra + vps_len + sps_len, pps, pps_len);

	            v_extra_len = vps_len + sps_len + pps_len;
            }
        }

        if (AUDIO_CODEC_AAC == acodec)
        {
            a_extra_len = p_rtsp->get_audio_config_len();
            if (a_extra_len > 0 && a_extra_len <= sizeof(a_extra))
            {
                memcpy(a_extra, p_rtsp->get_audio_config(), a_extra_len);
            }
            else
            {
                a_extra_len = 0;
            }
        }
	}
#ifdef RTMP_STREAM
    else if (RTMP_EVE_VIDEOREADY == event)
	{
	    CRtmpClient * p_rtmp = p_rua->rtmp;
	    vcodec = p_rtmp->video_codec();

        if (VIDEO_CODEC_H264 == vcodec)
        {
            uint8 sps[512], pps[512];
			int sps_len = 0, pps_len = 0;
            
            if (p_rtmp->get_h264_params(sps, &sps_len, pps, &pps_len))
            {
                memcpy(v_extra, sps, sps_len);
	            memcpy(v_extra + sps_len, pps, pps_len);

	            v_extra_len = sps_len + pps_len;
	        }
        }
	}
    else if (RTMP_EVE_AUDIOREADY == event)
	{
	    CRtmpClient * p_rtmp = p_rua->rtmp;
	    acodec = p_rtmp->audio_codec();

        if (AUDIO_CODEC_AAC == acodec)
        {
            a_extra_len = p_rtmp->get_audio_config_len();
            if (a_extra_len > 0 && a_extra_len <= sizeof(a_extra))
            {
                memcpy(a_extra, p_rtmp->get_audio_config(), a_extra_len);
            }
            else
            {
                a_extra_len = 0;
            }
        }
	}
#endif	
#ifdef MJPEG_STREAM	
	else if (event == MJPEG_EVE_CONNSUCC)
	{
	    vcodec = VIDEO_CODEC_JPEG;
	}
#endif	
    else
    {
        return 0;
    }

    // save to AVI file
    
    if (R2F_FMT_AVI == p_rua->filefmt)
    {
	    AVICTX * p_avictx = p_rua->avictx;

	    // set video infomation
	    
	    if (VIDEO_CODEC_H264 == vcodec)
	    {
	        avi_set_video_info(p_avictx, p_rua->framerate, 0, 0, "H264");
            
	        if (v_extra_len > 0)
	        {
				avi_set_video_extra_info(p_avictx, v_extra, v_extra_len);
	        }
	    }
	    else if (VIDEO_CODEC_H265 == vcodec)
	    {
	        avi_set_video_info(p_avictx, p_rua->framerate, 0, 0, "H265");

            if (v_extra_len > 0)
	        {
				avi_set_video_extra_info(p_avictx, v_extra, v_extra_len);
	        }
	    }
	    else if (VIDEO_CODEC_MP4 == vcodec)
	    {
	        avi_set_video_info(p_avictx, p_rua->framerate, 0, 0, "MP4V");
	    }
	    else if (VIDEO_CODEC_JPEG == vcodec)
	    {
#ifdef VIDEO_CONV
            // Transcode JPEG format to H264 format
            avi_set_video_info(p_avictx, p_rua->framerate, 0, 0, "H264");
#else
	        avi_set_video_info(p_avictx, p_rua->framerate, 0, 0, "JPEG");
#endif	        
	    }

	    // set audio information
	    
	    if (AUDIO_CODEC_G711A == acodec)
	    {
	        avi_set_audio_info(p_avictx, ch, sr, AUDIO_FORMAT_ALAW);
	    }
	    else if (AUDIO_CODEC_G711U == acodec)
	    {
	        avi_set_audio_info(p_avictx, ch, sr, AUDIO_FORMAT_MULAW);
	    }
	    else if (AUDIO_CODEC_G726 == acodec)
	    {
	        avi_set_audio_info(p_avictx, ch, sr, AUDIO_FORMAT_G726);
	    }
	    else if (AUDIO_CODEC_G722 == acodec)
	    {
	        avi_set_audio_info(p_avictx, ch, sr, AUDIO_FORMAT_G722);
	    }
	    else if (AUDIO_CODEC_AAC == acodec)
	    {
	        avi_set_audio_info(p_avictx, ch, sr, AUDIO_FORMAT_AAC);

	        if (a_extra_len > 0)
	        {
	            avi_set_audio_extra_info(p_avictx, a_extra, a_extra_len);
	        }
	    }

#ifdef RTMP_STREAM
        // set the default video setting
	    if (RTMP_EVE_AUDIOREADY == event && !p_avictx->ctxf_video)
	    {
	        avi_set_video_info(p_avictx, p_rua->framerate, 0, 0, "H264");
	    }

        // set the default audio setting
        if (RTMP_EVE_VIDEOREADY == event && !p_avictx->ctxf_audio)
        {
            uint8 extra[2] = {0x12, 0x10};
            
            avi_set_audio_info(p_avictx, 2, 44100, AUDIO_FORMAT_AAC);
            avi_set_audio_extra_info(p_avictx, extra, 2);
        }
#endif

	    avi_update_header(p_avictx);
    }
#ifdef MP4_FORMAT	 
    else if (R2F_FMT_MP4 == p_rua->filefmt) //save to MP4 file
    {
        MP4CTX * p_mp4ctx = p_rua->mp4ctx;    	    
	    	    
	    if (VIDEO_CODEC_H264 == vcodec)
	    {
	        mp4_set_video_info(p_mp4ctx, p_rua->framerate, 0, 0, "H264");
	    }
	    else if (VIDEO_CODEC_H265 == vcodec)
	    {
	        mp4_set_video_info(p_mp4ctx, p_rua->framerate, 0, 0, "H265");
	    }
	    else if (VIDEO_CODEC_MP4 == vcodec)
	    {
	        mp4_set_video_info(p_mp4ctx, p_rua->framerate, 0, 0, "MP4V");
	    }
	    else if (VIDEO_CODEC_JPEG == vcodec)
	    {
#ifdef VIDEO_CONV
            // Transcode JPEG format to H264 format
            mp4_set_video_info(p_mp4ctx, p_rua->framerate, 0, 0, "H264");
#else            
	        mp4_set_video_info(p_mp4ctx, p_rua->framerate, 0, 0, "JPEG");
#endif
	    }
            
	    if (AUDIO_CODEC_AAC == acodec)
	    {
	        mp4_set_audio_info(p_mp4ctx, ch, sr, AUDIO_FORMAT_AAC);

	        if (a_extra_len > 0)
	        {
	            mp4_set_audio_extra_info(p_mp4ctx, a_extra, a_extra_len);  
	        }
	    }
#ifdef AUDIO_CONV
        else if (AUDIO_CODEC_NONE != acodec)
        {
            if (r2f_init_audio_recodec(p_rua, acodec, sr, ch))
            {
                int extralen = 0;
                uint8 * extradata = NULL;

                p_rua->aencoder->getExtraData(&extradata, &extralen);

                mp4_set_audio_info(p_rua->mp4ctx, ch, sr, AUDIO_FORMAT_AAC);
                mp4_set_audio_extra_info(p_rua->mp4ctx, extradata, extralen);
            }
        }
#endif

	    mp4_update_header(p_mp4ctx);	
    }
#endif // MP4_FORMAT

    p_rua->starttime = time(NULL);
    
    return 0;
}

int rtsp_audio_callback(uint8 * pdata, int len, uint32 ts, uint16 seq, void * puser)
{
    // log_print(HT_LOG_DBG, "%s, len = %d, ts = %u, seq = %d\r\n", __FUNCTION__, len, ts, seq);

    return r2f_record_audio((RUA *)puser, pdata, len);
}

int rtsp_video_callback(uint8 * pdata, int len, uint32 ts, uint16 seq, void * puser)
{
    // log_print(HT_LOG_DBG, "%s, len = %d, ts = %u, seq = %d\r\n", __FUNCTION__, len, ts, seq);

    return r2f_record_video((RUA *)puser, pdata, len);
}

/***************************************************************/
void rtsp_reconn(RUA * p_rua)
{
    char url[256], user[64], pass[64];
    CRtspClient * p_rtsp = p_rua->rtsp;

	strcpy(url, p_rtsp->get_url());
	strcpy(user, p_rtsp->get_user());
	strcpy(pass, p_rtsp->get_pass());

	log_print(HT_LOG_DBG, "%s, url = %s, user = %s, pass = %s\r\n", __FUNCTION__, url, user, pass);

	p_rtsp->rtsp_close();

	p_rtsp->set_notify_cb(r2f_notify_callback, p_rua);
    p_rtsp->set_audio_cb(rtsp_audio_callback);
    p_rtsp->set_video_cb(rtsp_video_callback);

	if (!p_rtsp->rtsp_start(url, user, pass))
	{
	    log_print(HT_LOG_ERR, "%s, rtsp_start failed. %s\r\n", __FUNCTION__, url);
	}
}

int r2f_record_aac(RUA * p_rua, uint8 * pdata, int len)
{
    int ret = -1;
    int chs = 1;
    int sr = 8000;
    int rate_idx = 0;
    uint16 frame_len = len + 7;
    uint8 adts[7];

    if (p_rua->rtsp_flag && p_rua->rtsp)
    {
        chs = p_rua->rtsp->get_audio_channels();
        sr = p_rua->rtsp->get_audio_samplerate();
    }
#ifdef RTMP_STREAM        
    else if (p_rua->rtmp_flag && p_rua->rtmp)
    {
        chs = p_rua->rtmp->get_audio_channels();
        sr = p_rua->rtmp->get_audio_samplerate();
    }
#endif
    else
    {
        return -1;
    }
    
    adts[0] = 0xff;
    adts[1] = 0xf9;
    adts[2] = (2 - 1) << 6; // profile, AAC-LC

    switch (sr)
    {
    case 96000:
        rate_idx = 0;
        break;

    case 88200:
        rate_idx = 1;
        break;
        
    case 64000:
        rate_idx = 2;
        break;
        
    case 48000:
        rate_idx = 3;
        break;
        
    case 44100:
        rate_idx = 4;
        break;
        
    case 32000:
        rate_idx = 5;
        break;
        
    case 24000:
        rate_idx = 6;
        break;

    case 22050:
        rate_idx = 7;
        break;

    case 16000:
        rate_idx = 8;
        break;     

    case 12000:
        rate_idx = 9;
        break;

    case 11025:
        rate_idx = 10;
        break;

    case 8000:
        rate_idx = 11;
        break;

    case 7350:
        rate_idx = 12;
        break;
        
    default:
        rate_idx = 4;
        break;
    }

    adts[2] |= (rate_idx << 2);
    adts[2] |= (chs & 0x4) >> 2;
    adts[3] =  (chs & 0x3) << 6;
    adts[3] |= (frame_len & 0x1800) >> 11;
    adts[4] =  (frame_len & 0x1FF8) >> 3;
    adts[5] =  (frame_len & 0x7) << 5; 

    /* buffer fullness (0x7FF for VBR) over 5 last bits*/
    
    adts[5] |= 0x1F;
    
    adts[6] = 0xFC;
    adts[6] |= (len / 1024) & 0x03; // set raw data blocks. 

    int size = len + 7;
    char * buff = (char *) malloc(size);
    if (buff)
    {
        memcpy(buff, adts, 7);
        memcpy(buff+7, pdata, len);

        if (R2F_FMT_AVI == p_rua->filefmt)
        {
            ret = avi_write_audio(p_rua->avictx, buff, size);
        }
#ifdef MP4_FORMAT        
        else if (R2F_FMT_MP4 == p_rua->filefmt)
        {
            ret = mp4_write_audio(p_rua->mp4ctx, buff, size);
        }
#endif

        free(buff);
    }
    
    return ret;
}

int r2f_record_audio(RUA * p_rua, uint8 * pdata, int len)
{
    int ret = -1, codec;

    if (p_rua->rtsp_flag && p_rua->rtsp)
    {
        codec = p_rua->rtsp->audio_codec();
    }
#ifdef RTMP_STREAM    
    else if (p_rua->rtmp_flag && p_rua->rtmp)
    {
        codec = p_rua->rtmp->audio_codec();
    }
#endif    
    else 
    {
        return -1;
    }
        
    if (codec == AUDIO_CODEC_AAC)
    {
        r2f_record_aac(p_rua, pdata, len);
    }
    else
    {
        if (R2F_FMT_AVI == p_rua->filefmt)
        {
            ret = avi_write_audio(p_rua->avictx, pdata, len);
        }
#ifdef MP4_FORMAT        
        else if (R2F_FMT_MP4 == p_rua->filefmt)
        {
#ifdef AUDIO_CONV        
            if (p_rua->adecoder)
            {
                p_rua->adecoder->decode(pdata, len);
            }
#endif
        }
#endif
    }

    if (r2f_switch_check(p_rua))
    {
        r2f_file_switch(p_rua);
    }

    return ret;
}

int r2f_record_video_internal(RUA * p_rua, uint8 * pdata, int len, int codec)
{
    if (R2F_FMT_AVI == p_rua->filefmt)
    {
        AVICTX * p_avictx = p_rua->avictx;
        
        if (p_avictx->v_fps == 0)
        {
            avi_calc_fps(p_avictx, pdata, len);
        }

        if (p_avictx->v_width == 0 || p_avictx->v_height == 0)
        {
#ifdef VIDEO_CONV
            if (NULL == p_rua->vencoder && codec == VIDEO_CODEC_JPEG)
            {
                memcpy(p_avictx->v_fcc, "JPEG", 4); // Temporarily modified to JPEG format
            }
#endif
            avi_parse_video_size(p_avictx, pdata, len);
            
            if (p_avictx->v_width && p_avictx->v_height)
            {
#ifdef VIDEO_CONV
                if (codec == VIDEO_CODEC_JPEG)
                {
                    r2f_init_video_recodec(p_rua, codec, p_avictx->v_width, p_avictx->v_height, p_rua->framerate);

                    memcpy(p_avictx->v_fcc, "H264", 4); // Cancel temporary modification
                }
#endif
                avi_update_header(p_avictx);
            }
        }

        int key = 0;
        
        if (VIDEO_CODEC_H264 == codec)
        {
            uint8 nalu_t = (pdata[4] & 0x1F);
            key = (nalu_t == 5);
        }
        else if (VIDEO_CODEC_H265 == codec)
        {
            uint8 nalu_t = (pdata[4] >> 1) & 0x3F;
            key = (nalu_t >= 16 && nalu_t <= 21);
        }  
        else if (VIDEO_CODEC_JPEG == codec)
        {
#ifdef VIDEO_CONV
            if (p_rua->vdecoder)
            {
                p_rua->vdecoder->decode(pdata, len);
            }
            
            return 0;
#else
            key = 1;
#endif
        }
        
        avi_write_video(p_avictx, pdata, len, key);
    }
#ifdef MP4_FORMAT
    else if (R2F_FMT_MP4 == p_rua->filefmt)
    {
        MP4CTX * p_mp4ctx = p_rua->mp4ctx;
        
        if (p_mp4ctx->v_fps == 0)
        {
            mp4_calc_fps(p_mp4ctx, pdata, len);
        }

        if (p_mp4ctx->v_width == 0 || p_mp4ctx->v_height == 0)
        {
#ifdef VIDEO_CONV
            if (NULL == p_rua->vencoder && codec == VIDEO_CODEC_JPEG)
            {
                memcpy(p_mp4ctx->v_fcc, "JPEG", 4); // Temporarily modified to JPEG format
            }
#endif        
            mp4_parse_video_size(p_mp4ctx, pdata, len);
            
            if (p_mp4ctx->v_width && p_mp4ctx->v_height)
            {
#ifdef VIDEO_CONV
                if (codec == VIDEO_CODEC_JPEG)
                {
                    r2f_init_video_recodec(p_rua, codec, p_mp4ctx->v_width, p_mp4ctx->v_height, p_rua->framerate);

                    memcpy(p_mp4ctx->v_fcc, "H264", 4); // Cancel temporary modification
                }
#endif 
                mp4_update_header(p_mp4ctx);
            }
        }

        int key = 0;
        
        if (VIDEO_CODEC_H264 == codec)
        {
            uint8 nalu_t = (pdata[4] & 0x1F);
            key = (nalu_t == 5);
        }
        else if (VIDEO_CODEC_H265 == codec)
        {
            uint8 nalu_t = (pdata[4] >> 1) & 0x3F;
            key = (nalu_t >= 16 && nalu_t <= 21);
        }  
        else if (VIDEO_CODEC_JPEG == codec)
        {
#ifdef VIDEO_CONV
            if (p_rua->vdecoder)
            {
                p_rua->vdecoder->decode(pdata, len);
            }
            
            return 0;
#else
            key = 1;
#endif            
        }
        
        mp4_write_video(p_mp4ctx, pdata, len, key);
	}
#endif	
	else
	{
	    return -1;
	}
	
    if (r2f_switch_check(p_rua))
    {
        r2f_file_switch(p_rua);
    }
    
	return 0;
}

int r2f_record_video_ex(RUA * p_rua, uint8 * pdata, int len, int codec)
{
    if (VIDEO_CODEC_H264 == codec || VIDEO_CODEC_H265 == codec)
    {
        int s_len = 0, n_len = 0, parse_len = len;
    	uint8 * p_cur = pdata;
        
    	while (p_cur)
    	{
    		uint8 * p_next = avc_split_nalu(p_cur, parse_len, &s_len, &n_len);
    		if (n_len < 5)
    		{
    		    // without start code
    		    r2f_record_video_internal(p_rua, p_cur, parse_len, codec);
    			return 0;
            }

    		r2f_record_video_internal(p_rua, p_cur, n_len, codec);
    		
    		parse_len -= n_len;
    		p_cur = p_next;
    	}
	}
	else
	{
	    r2f_record_video_internal(p_rua, pdata, len, codec);
	}

	return 0;
}

int r2f_record_video(RUA * p_rua, uint8 * pdata, int len)
{
    int codec;
    
    if (p_rua->rtsp_flag && p_rua->rtsp)
    {
        codec = p_rua->rtsp->video_codec();
    }
#ifdef RTMP_STREAM    
    else if (p_rua->rtmp_flag && p_rua->rtmp)
    {
        codec = p_rua->rtmp->video_codec();
    }
#endif
#ifdef MJPEG_STREAM
    else if (p_rua->mjpeg_flag && p_rua->mjpeg)
    {
        codec = VIDEO_CODEC_JPEG;
    }
#endif
    else 
    {
        return -1;
    }

    return r2f_record_video_ex(p_rua, pdata, len, codec);
}

void r2f_notify_handler(RUA * p_rua, uint32 evt)
{
    if (RTSP_EVE_STOPPED == evt || RTSP_EVE_CONNFAIL == evt || 
        RTSP_EVE_NODATA == evt || RTSP_EVE_NOSIGNAL == evt)
	{
		rtsp_reconn(p_rua);
		
		usleep(100*1000);
	}
#ifdef RTMP_STREAM
    else if (RTMP_EVE_STOPPED == evt || RTMP_EVE_CONNFAIL == evt || 
        RTMP_EVE_NODATA == evt || RTMP_EVE_NOSIGNAL == evt)
	{
		rtmp_reconn(p_rua);
		
		usleep(100*1000);
	}
#endif
#ifdef MJPEG_STREAM
    else if (MJPEG_EVE_CONNFAIL == evt || MJPEG_EVE_NOSIGNAL == evt || 
        MJPEG_EVE_NODATA == evt || MJPEG_EVE_STOPPED == evt)
    {
        mjpeg_reconn(p_rua);
		
		usleep(100*1000);
    }
#endif
}

void * r2f_task_thread(void * argv)
{
    RIMG msg;
	
	while (g_r2f_cls.task_flag)
	{
		if (hqBufGet(g_r2f_cls.msg_queue, (char *) &msg))
		{
		    switch (msg.msg_src)
		    {
		    case R2F_NTF_SRC:
		        r2f_notify_handler(rua_get_by_index(msg.msg_dua), msg.msg_evt);
		        break;

		    case R2F_EXIT:
		        goto exit_flag;
		        break;
		    }
		}
	}

exit_flag:

	g_r2f_cls.tid_task = 0;

	log_print(HT_LOG_INFO, "%s, exit\r\n", __FUNCTION__);

	return NULL;
}

BOOL r2f_rua_start(RUA * p_rua)
{
    BOOL ret = FALSE;
    
    if (p_rua->rtsp_flag)
    {
        CRtspClient * p_rtsp = p_rua->rtsp;
        
        p_rtsp->set_notify_cb(r2f_notify_callback, p_rua);
        p_rtsp->set_audio_cb(rtsp_audio_callback);
        p_rtsp->set_video_cb(rtsp_video_callback);

        ret = p_rtsp->rtsp_start(p_rua->url, p_rua->user, p_rua->pass);
    }
#ifdef RTMP_STREAM    
    else if (p_rua->rtmp_flag)
    {
        CRtmpClient * p_rtmp = p_rua->rtmp;
        
        p_rtmp->set_notify_cb(r2f_notify_callback, p_rua);
        p_rtmp->set_audio_cb(rtmp_audio_callback);
        p_rtmp->set_video_cb(rtmp_video_callback);

        ret = p_rtmp->rtmp_start(p_rua->url, p_rua->user, p_rua->pass);
    }
#endif
#ifdef MJPEG_STREAM
    else if (p_rua->mjpeg_flag)
    {
        CHttpMjpeg * p_mjpeg = p_rua->mjpeg;

        p_mjpeg->set_notify_cb(r2f_notify_callback, p_rua);
        p_mjpeg->set_video_cb(mjpeg_video_callback);

        ret = p_mjpeg->mjpeg_start(p_rua->url, p_rua->user, p_rua->pass);
    }
#endif

    if (ret)
    {
        printf("stream2file : %s ==> %s\r\n", p_rua->url, p_rua->savepath);
    }
    
    return ret;
}

const char * r2f_fmt_str(int fmt)
{
    if (R2F_FMT_AVI == fmt)
    {
        return "avi";
    }
#ifdef MP4_FORMAT    
    else if (R2F_FMT_MP4 == fmt)
    {
        return "mp4";
    }
#endif

    return "avi";
}

BOOL r2f_get_url_host(const char * url, char * host, int hostlen)
{
    if (memcmp(url, "rtsp://", 7) == 0)
    {
        int port;
        char * user = NULL;
        char * pass = NULL;
        char * address = NULL;   
        
        if (!CRtspClient::parse_url(url, user, pass, address, port, NULL))
        {
            log_print(HT_LOG_ERR, "%s, invalid rtsp url %s\r\n", __FUNCTION__, url);
            return FALSE;
        }

        if (user)
        {
            delete[] user;
        }

        if (pass)
        {
            delete[] pass;
        }

        if (NULL == address)
        {
            log_print(HT_LOG_ERR, "%s, invalid address %s\r\n", __FUNCTION__, url);
            return FALSE;
        }

        strncpy(host, address, hostlen);

        delete[] address;

        return TRUE;
    }
#ifdef RTMP_STREAM
    else if (memcmp(url, "rtmp://", 7) == 0 || 
        memcmp(url, "rtmpt://", 8) == 0 || 
	    memcmp(url, "rtmps://", 8) == 0 || 
	    memcmp(url, "rtmpe://", 8) == 0 || 
	    memcmp(url, "rtmpfp://", 9) == 0 || 
	    memcmp(url, "rtmpte://", 9) == 0 || 
	    memcmp(url, "rtmpts://", 9) == 0)
    {
        int protocol;
        uint32 port;
        AVal avhost, playpath, app;
        
        if (!RTMP_ParseURL(url, &protocol, &avhost, &port, &playpath, &app))
        {
            log_print(HT_LOG_ERR, "%s, invalid rtmp url %s\r\n", __FUNCTION__, url);
            return FALSE;
        }

        if (avhost.av_len < hostlen)
        {
            strncpy(host, avhost.av_val, avhost.av_len);
        }

        return TRUE;
    }
#endif
#ifdef MJPEG_STREAM
    else if (memcmp(url, "http://", 7) == 0 || memcmp(url, "https://", 8) == 0)
    {
        int port = 80;
        int https = 0;
        char * user = NULL;
        char * pass = NULL;
        char * address = NULL;	
    	char const * suffix = NULL;
    	
    	if (!CHttpMjpeg::parse_url(url, user, pass, address, port, &suffix, https))
    	{
    		return FALSE;
    	}
    	
    	if (user)
    	{
    		delete[] user;
    	}
    	
    	if (pass)
    	{
    		delete[] pass;
    	}

        if (NULL == address)
        {
            log_print(HT_LOG_ERR, "%s, invalid address %s\r\n", __FUNCTION__, url);
            return FALSE;
        }

        strncpy(host, address, hostlen);

        delete[] address;

        return TRUE;
    }
#endif

    return FALSE;
}

BOOL r2f_filepath(const char * url, const char * cfgpath, int fmt, char * filepath, int pathlen)
{
    char host[64] = {'\0'};
    char timestr[32] = {'\0'};
    time_t nowtime;
	struct tm *t1;
	static int cnt = 0;
	
    if (cfgpath[0] != '\0')
    {
        if (access(cfgpath, 0) == -1) // the path not exist
        {
            log_print(HT_LOG_ERR, "%s, path %s not exist\r\n", __FUNCTION__, cfgpath);
    		return FALSE;
        }
    }
    
    if (!r2f_get_url_host(url, host, sizeof(host)-1))
    {
        log_print(HT_LOG_ERR, "%s, r2f_get_url_host, %s\r\n", __FUNCTION__, url);
        return FALSE;
    }

	time(&nowtime);
	t1 = localtime(&nowtime);

    srand((uint32)time(NULL)+cnt++);
	snprintf(timestr, sizeof(timestr)-1, "%04d_%02d_%02d_%02d_%02d_%02d_%03d",
		t1->tm_year+1900, t1->tm_mon+1, t1->tm_mday,
		t1->tm_hour, t1->tm_min, t1->tm_sec, rand());    

    if (cfgpath[0] != '\0')
    {
        int cfgpathlen = strlen(cfgpath);

        if (cfgpath[cfgpathlen-1] == '\\' || cfgpath[cfgpathlen-1] == '/')
        {
            snprintf(filepath, pathlen, "%s%s_%s.%s", cfgpath, host, timestr, r2f_fmt_str(fmt));
        }
        else
        {
#if __WINDOWS_OS__
            snprintf(filepath, pathlen, "%s\\%s_%s.%s", cfgpath, host, timestr, r2f_fmt_str(fmt));
#else
            snprintf(filepath, pathlen, "%s/%s_%s.%s", cfgpath, host, timestr, r2f_fmt_str(fmt));
#endif
        }
    }
    else
    {
        snprintf(filepath, pathlen, "%s_%s.%s", host, timestr, r2f_fmt_str(fmt));
    }
    
    return TRUE;
}

BOOL r2f_switch_check(RUA * p_rua)
{
    uint64 tlen = 0;

    if (p_rua->filefmt == R2F_FMT_AVI)
    {
        AVICTX * p_ctx = p_rua->avictx;
        tlen = ftell(p_ctx->f);
    }
#ifdef MP4_FORMAT    
    else if (p_rua->filefmt == R2F_FMT_MP4)
    {
        MP4CTX * p_ctx = p_rua->mp4ctx;
        if (p_ctx!=NULL)
        tlen = gf_isom_get_file_size(p_ctx->handler);
    }
#endif

    // Switch according to the recording size
    if (p_rua->recordsize > 0 && tlen > (long)p_rua->recordsize * 1024)
    {
        return TRUE;
    }

    // Switch according to the recording duration
    if (p_rua->recordtime > 0 && time(NULL) - p_rua->starttime > p_rua->recordtime)
    {
        return TRUE;
    }
 
    return FALSE;
}

void r2f_file_switch(RUA * p_rua)
{
    if (p_rua->filefmt == R2F_FMT_AVI)
    {
        AVICTX * p_ctx;
        AVICTX * p_oldctx = p_rua->avictx;
        
        r2f_filepath(p_rua->url, p_rua->cfgpath, p_rua->filefmt, p_rua->savepath, sizeof(p_rua->savepath)-1);

        log_print(HT_LOG_DBG, "%s, filepath\r\n", __FUNCTION__, p_rua->savepath);
        
        p_ctx = avi_write_open(p_rua->savepath);
        if (NULL == p_ctx)
        {
            return;
        }
     
        p_ctx->ctxf_video = p_oldctx->ctxf_video;
        p_ctx->ctxf_audio = p_oldctx->ctxf_audio;
     
        if (p_ctx->ctxf_video)
        {
            p_ctx->ctxf_sps_f = p_oldctx->ctxf_sps_f;
            avi_set_video_info(p_ctx, p_oldctx->v_fps, p_oldctx->v_width, p_oldctx->v_height, p_oldctx->v_fcc);
            avi_set_video_extra_info(p_ctx, p_oldctx->v_extra, p_oldctx->v_extra_len);
        }
     
        if (p_ctx->ctxf_audio)
        {
            avi_set_audio_info(p_ctx, p_oldctx->a_chns, p_oldctx->a_rate, p_oldctx->a_fmt);
            avi_set_audio_extra_info(p_ctx, p_oldctx->a_extra, p_oldctx->a_extra_len);
        }
        
        avi_write_close(p_oldctx);
     
        avi_update_header(p_ctx);

        if (p_ctx->ctxf_video)
        {
            if (memcmp(p_ctx->v_fcc, "H264", 4) == 0)
            {
                uint8 sps[512];            
                uint8 pps[512];
                int sps_len = sizeof(sps);
                int pps_len = sizeof(pps);

                if (p_rua->rtsp_flag)
                {
                    CRtspClient * p_rtsp = p_rua->rtsp;
                    
                    if (p_rtsp->get_h264_params(sps, &sps_len, pps, &pps_len))
                    {
                        avi_write_video(p_ctx, sps, sps_len, 0);
                        avi_write_video(p_ctx, pps, pps_len, 0);
                    }
                }
#ifdef RTMP_STREAM                
                else if (p_rua->rtmp_flag)
                {
                    CRtmpClient * p_rtmp = p_rua->rtmp;
                    
                    if (p_rtmp->get_h264_params(sps, &sps_len, pps, &pps_len))
                    {
                        avi_write_video(p_ctx, sps, sps_len, 0);
                        avi_write_video(p_ctx, pps, pps_len, 0);
                    }
                }
#endif                
            }
            else if (memcmp(p_ctx->v_fcc, "H265", 4) == 0)
            {
                uint8 sps[512];            
                uint8 pps[512];
                uint8 vps[512];
                int sps_len = sizeof(sps);
                int pps_len = sizeof(pps);
                int vps_len = sizeof(vps);

                if (p_rua->rtsp_flag)
                {
                    CRtspClient * p_rtsp = p_rua->rtsp;
                    
                    if (p_rtsp->get_h265_params(sps, &sps_len, pps, &pps_len, vps, &vps_len))
                    {
                        avi_write_video(p_ctx, vps, vps_len, 0);
                        avi_write_video(p_ctx, sps, sps_len, 0);
                        avi_write_video(p_ctx, pps, pps_len, 0);
                    }
                }
            }
        }
        
        p_rua->avictx = p_ctx;
    }
#ifdef MP4_FORMAT    
    else if (p_rua->filefmt == R2F_FMT_MP4)
    {
        MP4CTX * p_ctx;
        MP4CTX * p_oldctx = p_rua->mp4ctx;
        
        r2f_filepath(p_rua->url, p_rua->cfgpath, p_rua->filefmt, p_rua->savepath, sizeof(p_rua->savepath)-1);

        log_print(HT_LOG_DBG, "%s, filepath\r\n", __FUNCTION__, p_rua->savepath);
        
        p_ctx = mp4_write_open(p_rua->savepath);
        if (NULL == p_ctx)
        {
            return;
        }
     
        p_ctx->ctxf_video = p_oldctx->ctxf_video;
        p_ctx->ctxf_audio = p_oldctx->ctxf_audio;
     
        if (p_ctx->ctxf_video)
        {
            p_ctx->ctxf_sps_f = p_oldctx->ctxf_sps_f;
            mp4_set_video_info(p_ctx, p_oldctx->v_fps, p_oldctx->v_width, p_oldctx->v_height, p_oldctx->v_fcc);
        }
     
        if (p_ctx->ctxf_audio)
        {
            mp4_set_audio_info(p_ctx, p_oldctx->a_chns, p_oldctx->a_rate, p_oldctx->a_fmt);
            mp4_set_audio_extra_info(p_ctx, p_oldctx->a_extra, p_oldctx->a_extra_len);
        }
        
        mp4_write_close(p_oldctx);
     
        mp4_update_header(p_ctx);

        if (p_ctx->ctxf_video)
        {
            if (memcmp(p_ctx->v_fcc, "H264", 4) == 0)
            {
                uint8 sps[512];            
                uint8 pps[512];
                int sps_len = sizeof(sps);
                int pps_len = sizeof(pps);

                if (p_rua->rtsp_flag)
                {
                    CRtspClient * p_rtsp = p_rua->rtsp;
                    
                    if (p_rtsp->get_h264_params(sps, &sps_len, pps, &pps_len))
                    {
                        mp4_write_video(p_ctx, sps, sps_len, 0);
                        mp4_write_video(p_ctx, pps, pps_len, 0);
                    }
                }
#ifdef RTMP_STREAM                
                else if (p_rua->rtmp_flag)
                {
                    CRtmpClient * p_rtmp = p_rua->rtmp;
                    
                    if (p_rtmp->get_h264_params(sps, &sps_len, pps, &pps_len))
                    {
                        mp4_write_video(p_ctx, sps, sps_len, 0);
                        mp4_write_video(p_ctx, pps, pps_len, 0);
                    }
                }
#endif                
            }
            else if (memcmp(p_ctx->v_fcc, "H265", 4) == 0)
            {
                uint8 sps[512];            
                uint8 pps[512];
                uint8 vps[512];
                int sps_len = sizeof(sps);
                int pps_len = sizeof(pps);
                int vps_len = sizeof(vps);

                if (p_rua->rtsp_flag)
                {
                    CRtspClient * p_rtsp = p_rua->rtsp;
                    
                    if (p_rtsp->get_h265_params(sps, &sps_len, pps, &pps_len, vps, &vps_len))
                    {
                        mp4_write_video(p_ctx, vps, vps_len, 0);
                        mp4_write_video(p_ctx, sps, sps_len, 0);
                        mp4_write_video(p_ctx, pps, pps_len, 0);
                    }
                }
            }
        }
        
        p_rua->mp4ctx = p_ctx;
    }
#endif // MP4_FORMAT

    p_rua->starttime = time(NULL);

    printf("stream2file : %s ==> %s\r\n", p_rua->url, p_rua->savepath);
    
    log_print(HT_LOG_DBG, "%s, exit\r\n", __FUNCTION__);
}

BOOL r2f_init(STREAM2FILE * p_r2f)
{
    BOOL ret = FALSE;
	RUA * p_rua = rua_get_idle();
	if (NULL == p_rua)
	{
		log_print(HT_LOG_ERR, "%s, rua_get_idle return null\r\n", __FUNCTION__);
		return FALSE;
	}

    p_rua->filefmt = p_r2f->filefmt;
    strcpy(p_rua->url, p_r2f->url);
    strcpy(p_rua->user, p_r2f->user);
    strcpy(p_rua->pass, p_r2f->pass);
    strcpy(p_rua->cfgpath, p_r2f->savepath);
    p_rua->framerate = p_r2f->framerate;
    p_rua->recordsize = p_r2f->recordsize;
    p_rua->recordtime = p_r2f->recordtime;

    if (memcmp(p_rua->url, "rtsp://", 7) == 0)
    {
        p_rua->rtsp_flag = 1;
    }
#ifdef RTMP_STREAM
    else if (memcmp(p_rua->url, "rtmp://", 7) == 0 || 
        memcmp(p_rua->url, "rtmpt://", 8) == 0 || 
	    memcmp(p_rua->url, "rtmps://", 8) == 0 || 
	    memcmp(p_rua->url, "rtmpe://", 8) == 0 || 
	    memcmp(p_rua->url, "rtmpfp://", 9) == 0 || 
	    memcmp(p_rua->url, "rtmpte://", 9) == 0 || 
	    memcmp(p_rua->url, "rtmpts://", 9) == 0)
    {
        p_rua->rtmp_flag = 1;
    }
#endif
#ifdef MJPEG_STREAM
    else if (memcmp(p_rua->url, "http://", 7) == 0 || memcmp(p_rua->url, "https://", 8) == 0)
    {
        p_rua->mjpeg_flag = 1;
    }
#endif
    else
    {
        rua_set_idle(p_rua);
        
        log_print(HT_LOG_ERR, "%s, invalid url %s\r\n", __FUNCTION__, p_rua->url);
		return FALSE;
    }
    
    if (!r2f_filepath(p_rua->url, p_rua->cfgpath, p_rua->filefmt, p_rua->savepath, sizeof(p_rua->savepath)-1))
    {
        rua_set_idle(p_rua);

        log_print(HT_LOG_ERR, "%s, r2f_filepath failed\r\n", __FUNCTION__);
        return FALSE;
    }

    if (R2F_FMT_AVI == p_rua->filefmt)
    {
        p_rua->avictx = avi_write_open(p_rua->savepath);
        if (NULL == p_rua->avictx)
        {
            rua_set_idle(p_rua);

            log_print(HT_LOG_ERR, "%s, avi_write_open failed. %s\r\n", __FUNCTION__, p_rua->savepath);
            return FALSE;
        }
    }
#ifdef MP4_FORMAT    
    else if (R2F_FMT_MP4 == p_rua->filefmt)
    {
        p_rua->mp4ctx = mp4_write_open(p_rua->savepath);
        if (NULL == p_rua->mp4ctx)
        {
            rua_set_idle(p_rua);

            log_print(HT_LOG_ERR, "%s, mp4_write_open failed. %s\r\n", __FUNCTION__, p_rua->savepath);
            return FALSE;
        }
    }
#endif    
    else
    {
        rua_set_idle(p_rua);        

        log_print(HT_LOG_ERR, "%s, invalid file format %d.\r\n", __FUNCTION__, p_rua->filefmt);
        return FALSE;
    }

    if (p_rua->rtsp_flag)
    {    
        p_rua->rtsp = new CRtspClient;
    }
#ifdef RTMP_STREAM    
    else if (p_rua->rtmp_flag)
    {
        p_rua->rtmp = new CRtmpClient;
    }
#endif
#ifdef MJPEG_STREAM
    else if (p_rua->mjpeg_flag)
    {
        p_rua->mjpeg = new CHttpMjpeg();
    }
#endif

    if ((p_rua->rtsp_flag && NULL == p_rua->rtsp)
#ifdef RTMP_STREAM    
        || (p_rua->rtmp_flag && NULL == p_rua->rtmp)
#endif
#ifdef MJPEG_STREAM
        || (p_rua->mjpeg_flag && NULL == p_rua->mjpeg)
#endif
        )
    {
        if (R2F_FMT_AVI == p_rua->filefmt)
        {
            avi_write_close(p_rua->avictx);
        }
#ifdef MP4_FORMAT        
        else if (R2F_FMT_MP4 == p_rua->filefmt)
        {
            mp4_write_close(p_rua->mp4ctx);
        }
#endif

        rua_set_idle(p_rua);        

        log_print(HT_LOG_ERR, "%s, new failed\r\n", __FUNCTION__);
        return FALSE;
    }
    
    rua_set_online(p_rua);

	return r2f_rua_start(p_rua);
}
BOOL r2f_init_ex(STREAM2FILE* p_r2f, char* strfilename, int pnum)
{
    BOOL ret = FALSE;
    RUA* p_rua = rua_get_idle();
    if (NULL == p_rua)
    {
        log_print(HT_LOG_ERR, "%s, rua_get_idle return null\r\n", __FUNCTION__);
        return FALSE;
    }

    p_rua->filefmt = p_r2f->filefmt;
    strcpy(p_rua->url, p_r2f->url);
    strcpy(p_rua->user, p_r2f->user);
    strcpy(p_rua->pass, p_r2f->pass);
    strcpy(p_rua->cfgpath, p_r2f->savepath);
    p_rua->framerate = p_r2f->framerate;
    p_rua->recordsize = p_r2f->recordsize;
    p_rua->recordtime = p_r2f->recordtime;
    p_rua->pnum = pnum;

    if (memcmp(p_rua->url, "rtsp://", 7) == 0)
    {
        p_rua->rtsp_flag = 1;
    }
#ifdef RTMP_STREAM
    else if (memcmp(p_rua->url, "rtmp://", 7) == 0 ||
        memcmp(p_rua->url, "rtmpt://", 8) == 0 ||
        memcmp(p_rua->url, "rtmps://", 8) == 0 ||
        memcmp(p_rua->url, "rtmpe://", 8) == 0 ||
        memcmp(p_rua->url, "rtmpfp://", 9) == 0 ||
        memcmp(p_rua->url, "rtmpte://", 9) == 0 ||
        memcmp(p_rua->url, "rtmpts://", 9) == 0)
    {
        p_rua->rtmp_flag = 1;
    }
#endif
#ifdef MJPEG_STREAM
    else if (memcmp(p_rua->url, "http://", 7) == 0 || memcmp(p_rua->url, "https://", 8) == 0)
    {
        p_rua->mjpeg_flag = 1;
    }
#endif
    else
    {
        rua_set_idle(p_rua);

        log_print(HT_LOG_ERR, "%s, invalid url %s\r\n", __FUNCTION__, p_rua->url);
        return FALSE;
    }

    if (!r2f_filepath(p_rua->url, p_rua->cfgpath, p_rua->filefmt, p_rua->savepath, sizeof(p_rua->savepath) - 1))
    {
        rua_set_idle(p_rua);

        log_print(HT_LOG_ERR, "%s, r2f_filepath failed\r\n", __FUNCTION__);
        return FALSE;
    }
    strcpy(p_rua->savepath, strfilename);
    if (R2F_FMT_AVI == p_rua->filefmt)
    {
        p_rua->avictx = avi_write_open(p_rua->savepath);
        if (NULL == p_rua->avictx)
        {
            rua_set_idle(p_rua);

            log_print(HT_LOG_ERR, "%s, avi_write_open failed. %s\r\n", __FUNCTION__, p_rua->savepath);
            return FALSE;
        }
    }
#ifdef MP4_FORMAT    
    else if (R2F_FMT_MP4 == p_rua->filefmt)
    {
        p_rua->mp4ctx = mp4_write_open(p_rua->savepath);
        if (NULL == p_rua->mp4ctx)
        {
            rua_set_idle(p_rua);

            log_print(HT_LOG_ERR, "%s, mp4_write_open failed. %s\r\n", __FUNCTION__, p_rua->savepath);
            return FALSE;
        }
    }
#endif    
    else
    {
        rua_set_idle(p_rua);

        log_print(HT_LOG_ERR, "%s, invalid file format %d.\r\n", __FUNCTION__, p_rua->filefmt);
        return FALSE;
    }

    if (p_rua->rtsp_flag)
    {
        p_rua->rtsp = new CRtspClient;
    }
#ifdef RTMP_STREAM    
    else if (p_rua->rtmp_flag)
    {
        p_rua->rtmp = new CRtmpClient;
    }
#endif
#ifdef MJPEG_STREAM
    else if (p_rua->mjpeg_flag)
    {
        p_rua->mjpeg = new CHttpMjpeg();
    }
#endif

    if ((p_rua->rtsp_flag && NULL == p_rua->rtsp)
#ifdef RTMP_STREAM    
        || (p_rua->rtmp_flag && NULL == p_rua->rtmp)
#endif
#ifdef MJPEG_STREAM
        || (p_rua->mjpeg_flag && NULL == p_rua->mjpeg)
#endif
        )
    {
        if (R2F_FMT_AVI == p_rua->filefmt)
        {
            avi_write_close(p_rua->avictx);
        }
#ifdef MP4_FORMAT        
        else if (R2F_FMT_MP4 == p_rua->filefmt)
        {
            mp4_write_close(p_rua->mp4ctx);
        }
#endif

        rua_set_idle(p_rua);

        log_print(HT_LOG_ERR, "%s, new failed\r\n", __FUNCTION__);
        return FALSE;
    }

    rua_set_online(p_rua);

    return r2f_rua_start(p_rua);
}
#define DB_ADDRESS "127.0.0.1"

#define DB_ID "root"

#define DB_PASS "apmsetup"

#define DB_NAME "simprec_use"

BOOL r2f_start(char* pid)
{
    printf("IBST Record Server V%d.%d[for %s]\r\n", R2F_MAJOR_VERSION, R2F_MINOR_VERSION, pid);
    if (!r2f_read_config())
    {
        log_print(HT_LOG_ERR, "%s, r2f_read_config failed\r\n", __FUNCTION__);
        return FALSE;
    }
    char lfname[256];

    sprintf(lfname, ".\\log\\ibstrecord_%s.log", pid);

    if (g_r2f_cfg.log_enable)
	{
        log_init(lfname);
		log_set_level(g_r2f_cfg.log_level);
	}

	g_r2f_cls.msg_queue = hqCreate(10, sizeof(RIMG), HQ_GET_WAIT | HQ_PUT_WAIT);
	if (NULL == g_r2f_cls.msg_queue)
	{
		log_print(HT_LOG_ERR, "%s, create queue failed\r\n", __FUNCTION__);
		return FALSE;
	}

	g_r2f_cls.task_flag = 1;
	g_r2f_cls.tid_task = sys_os_create_thread((void *)r2f_task_thread, NULL);

    sys_buf_init(4 * MAX_NUM_RUA);
    rtsp_msg_buf_init(4 * MAX_NUM_RUA);
	rua_proxy_init();

#ifdef RTMP_STREAM
    rtmp_set_rtmp_log();
#endif

#ifdef MJPEG_STREAM
    http_msg_buf_init(2 * MAX_NUM_RUA);
#endif
    MYSQL mysql;
    MYSQL_RES* m_Res;
    MYSQL_ROW row;
    char daytime[100];
    char dbip[20];
    char start[3];
    char end[3];
    char fps[3];
    char fname[256];
    TCHAR DBIP[20] = { 0 };
    TCHAR START[3] = { 0 };
    TCHAR END[3] = { 0 };
    TCHAR BACKUP[3] = { 0 };
    TCHAR FPS[3] = { 0 };
    char query[256];
    int backup = 0;
    int len = 20;
    int m_num, m_process[1024], m_pnum;
    int m_fps;
    m_num = 0;
    m_pnum = 0;
    TCHAR strPathIni[100] = TEXT(".\\simprec.ini");

    ::GetPrivateProfileString(_T("SectionA"), _T("Server"), _T("127.0.0.1"), DBIP, 20, strPathIni);
    ::GetPrivateProfileString(_T("SectionA"), _T("Start"), _T("1"), START, 3, strPathIni);
    ::GetPrivateProfileString(_T("SectionA"), _T("End"), _T("6"), END, 3, strPathIni);
    ::GetPrivateProfileString(_T("SectionA"), _T("Backup"), _T("0"), BACKUP, 3, strPathIni);
    ::GetPrivateProfileString(_T("SectionA"), _T("FPS"), _T("25"), FPS, 3, strPathIni);
    printf("Server IP [%S] ROOM[%S-%S] BACKUP[%S] FPS[%S] \n", DBIP, START, END, BACKUP, FPS);
    mysql_init(&mysql);
    WideCharToMultiByte(CP_ACP, 0, DBIP, 20, dbip, len, NULL, NULL);
    WideCharToMultiByte(CP_ACP, 0, START, 3, start, len, NULL, NULL);
    WideCharToMultiByte(CP_ACP, 0, END, 3, end, len, NULL, NULL);
    WideCharToMultiByte(CP_ACP, 0, FPS, 3, fps, len, NULL, NULL);
    m_fps = atoi(fps);
    if (!mysql_real_connect(&mysql, dbip, DB_ID, DB_PASS, DB_NAME, 3306, 0, 0))
    {
        log_print(HT_LOG_ERR, "%s, mysql connect  failed,\r\n", __FUNCTION__);

    }
    if (_tcscmp(BACKUP, _T("1")) == 0)
    {
        sprintf_s(query, "select SEQ,CMD,ARG,NO,STATUS from cmdq where STATUS='B' AND room >=%s AND room <=%s ORDER BY SEQ", start, end);
        backup = 1;
    }
    else
        sprintf_s(query, "select SEQ,CMD,ARG,NO,STATUS,ARG2 from cmdq where STATUS='Y' AND room >=%s AND room <=%s AND no=%s ORDER BY SEQ", start, end, pid);
    int complete = 0;
    while (complete == 0)
    {
        if (mysql_query(&mysql, query))
        {	// ???? ??u
            // return;
        }
        m_Res = mysql_store_result(&mysql);
        if (m_Res != NULL)
            while ((row = mysql_fetch_row(m_Res)) != NULL)
            {
                try
                {
                    if (strcmp(row[1], "RECORD") == 0)
                    {
                        char rtspcmd[512];// = TEXT(row[2]);
                        char filename[256];
                        sprintf(rtspcmd, "%s", row[2]);
                        // char* address[, adress2[256], user[256], passwdip[256], fileparam[256], file, filename;
                        char* cmd, * cmd2, * cmd3, * address, user[50], passwordip[256], password2[256], * pass, * password, * add, address2[512], * fname, fname2[256];
                        // cmd = strtok(rtspcmd, " ");
                        address = rtspcmd;
                        /*                   address = strtok(NULL, " ");
                                           address = strtok(NULL, " ");
                                           filename = strtok(NULL, " ");
                                           filename = strtok(NULL, " ");
                                           filename = strtok(NULL, " ");
                                           filename = strtok(NULL, " ");
                                           filename = strtok(NULL, " ");
                                           filename = strtok(NULL, " ");
                                           filename = strtok(NULL, " ");
                                           filename = strtok(NULL, " ");*/
                        sprintf_s(filename, "%s", row[5]);
                        //   filename = strtok(NULL, " ");
                       //    address = strtok(NULL, " ");
                        sprintf(passwordip, "%s", address);
                        password = strtok(passwordip, ":");
                        password = strtok(NULL, ":");
                        sprintf(user, "%s", password);
                        password = strtok(NULL, ":");
                        sprintf(password2, "%s", password);
                        sprintf(passwordip, "%s", address);
                        pass = strtok(password2, "@");
                        add = strtok(passwordip, "@");
                        add = strtok(NULL, " ");
                        // opt = strtok(NULL, " ");
                        if (strncmp(rtspcmd, "http://", 7) == 0)
                        {
                            sprintf(address2, "%s", rtspcmd);
                            sprintf(user, "  admin");
                            sprintf(pass, "12345");
                        }
                        else
                            sprintf(address2, "rtsp://%s", add);
                        //  m_process[m_num] = atoi(row[3]);
                        STREAM2FILE* testr2f;
                        testr2f = (STREAM2FILE*)malloc(sizeof(STREAM2FILE));
                        memset(testr2f, 0, sizeof(STREAM2FILE));
                        strncpy(testr2f->url, address2, sizeof(testr2f->url) - 1);
                        strncpy(testr2f->user, user + 2, sizeof(testr2f->user) - 1);
                        strncpy(testr2f->pass, pass, sizeof(testr2f->pass) - 1);
                        strncpy(testr2f->savepath, "", sizeof(testr2f->savepath) - 1);
                        testr2f->filefmt = R2F_FMT_MP4;
                        testr2f->framerate = 0;
                        testr2f->recordsize = 0;
                        testr2f->recordtime = 0;
                        uint32 i = 0;
                        for (int i = 0; i < strlen(filename); i++)
                        {
                            if (filename[i] == '/')
                                filename[i] = '\\';
                        }
                        // fname = strtok(filename, ".");
                        sprintf(fname2, "%s", filename);
                        if (!r2f_init_ex(testr2f, fname2, atoi(row[3])))
                        {
                            printf("%s, r2f_init failed, %s\r\n", __FUNCTION__, testr2f->url);
                            log_print(HT_LOG_ERR, "%s, r2f_init failed, %s\r\n", __FUNCTION__, testr2f->url);
                        }
                        log_print(HT_LOG_INFO, "[%s],%d record process is started[%s] \n", __FUNCTION__, atoi(row[3]),fname2);
                        printf("[%s],%d record process is started[%s] \n", __FUNCTION__, atoi(row[3]), fname2);
                        char sqlcmd[256];
                        sprintf_s(sqlcmd, "UPDATE cmdq SET STATUS = 'N' where seq=%s", row[0]);
                        mysql_query(&mysql, sqlcmd);
                        delete testr2f;
                    }
                    else
                    {
                        int fnum = -1;
                        uint32 i = 0;
                        for (i = 0; i < MAX_NUM_RUA; i++)
                        {
                            RUA* p_rua = rua_get_by_index(i);
                            time_t rawtime;
                            long mov_no;


                            if (NULL != p_rua && p_rua->pnum == atoi(row[3]))
                            {
                                fnum = i;
                                rawtime = (const time_t)p_rua->mp4ctx->s_time;
                                mov_no = p_rua->mp4ctx->s_time;

                                if (p_rua->rtsp)
                                {
                                    p_rua->rtsp->rtsp_close();
                                    delete p_rua->rtsp;
                                    p_rua->rtsp = NULL;
                                }
                                if (p_rua->avictx)
                                {
                                    avi_write_close(p_rua->avictx);
                                    p_rua->avictx = NULL;
                                }
                                if (p_rua->mp4ctx)
                                {
                                    mp4_write_close(p_rua->mp4ctx);

                                }

                                struct tm* dt;
                                char timestr[30];
                                char buffer[30];

                                p_rua->mp4ctx = NULL;
                                dt = localtime(&rawtime);
                                // use any strftime format spec here
                               // strftime(timestr, sizeof(timestr), "%m%d%H%M%y", dt);
                               // sprintf(buffer, "%s", timestr);
                                sprintf_s(fname, "%s start time is [%d:%d:%d]", p_rua->savepath, dt->tm_hour, dt->tm_min, dt->tm_sec);
                                //cthread[threadcnt] = new thread(ConvertThread, fname);
                               // threadcnt++;
                                //if (threadcnt > 255) threadcnt = 0;
                                time_t time_now;
                                struct tm* tm;

                                time(&time_now);
                                tm = localtime(&time_now);


                                // ConvertThread(p_rua->savepath);
                                log_print(HT_LOG_INFO, "[%s]", fname);
                                printf("[%s]\n", fname);
                                log_print(HT_LOG_INFO, "[%s],[%d:%d:%d] %d record process is terminated \n", __FUNCTION__, tm->tm_hour, tm->tm_min, tm->tm_sec, p_rua->pnum);
                                printf("[%s],[%d:%d:%d] %d record process is terminated \n", __FUNCTION__, tm->tm_hour, tm->tm_min, tm->tm_sec, p_rua->pnum);


                                char sqlcmd[256];
                                sprintf_s(sqlcmd, "UPDATE kn_simprec_mov SET rtsp_start=%ld where no=%d", rawtime, p_rua->pnum);
                                log_print(HT_LOG_INFO, "%s\n", sqlcmd);
                                printf("%s\n", sqlcmd);
                                mysql_query(&mysql, sqlcmd);
                                rua_set_idle(p_rua);
                                complete = 1;
                                break;
                            }
                        }
                        //if (fnum != -1)
                        {

                            char sqlcmd[256];
                            sprintf_s(sqlcmd, "UPDATE cmdq SET STATUS = 'N' where seq=%s", row[0]);
                            mysql_query(&mysql, sqlcmd);

                        }
                    }
                }
                catch (int exceptionCode)
                {
                    log_print(HT_LOG_ERR, "Error %d in command proccesing\n", exceptionCode);
                    printf("Error %d in command proccesing\n", exceptionCode);
                }
            }
        if (m_Res)
        {
            mysql_free_result(m_Res);
        }
        Sleep(100);
    }

/*	STREAM2FILE * p_r2f = g_r2f_cfg.r2f;
	while (p_r2f)
	{
		if (!r2f_init(p_r2f))
		{
		    log_print(HT_LOG_ERR, "%s, r2f_init failed, %s\r\n", __FUNCTION__, p_r2f->url);
		}
		
		p_r2f = p_r2f->next;
	} */

	return TRUE;
}

void r2f_stop()
{
    uint32 i = 0;

    for (i = 0; i < MAX_NUM_RUA; i++)
    {
        RUA * p_rua = rua_get_by_index(i);

        if (NULL == p_rua || !p_rua->used_flag)
        {
            continue;
        }

        if (p_rua->rtsp)
        {
            p_rua->rtsp->rtsp_close();
            delete p_rua->rtsp;
            p_rua->rtsp = NULL;
        }

#ifdef RTMP_STREAM
        if (p_rua->rtmp)
        {
            p_rua->rtmp->rtmp_close();
            delete p_rua->rtmp;
            p_rua->rtmp = NULL;
        }
#endif

        if (p_rua->avictx)
        {
            avi_write_close(p_rua->avictx);
            p_rua->avictx = NULL;
        }

#ifdef MP4_FORMAT
        if (p_rua->mp4ctx)
        {
            mp4_write_close(p_rua->mp4ctx);
            p_rua->mp4ctx = NULL;
        }

#ifdef AUDIO_CONV
        if (p_rua->adecoder)
        {
            delete p_rua->adecoder;
            p_rua->adecoder = NULL;
        }

        if (p_rua->aencoder)
        {
            delete p_rua->aencoder;
            p_rua->aencoder = NULL;
        }
#endif

#endif

#ifdef VIDEO_CONV
        if (p_rua->vdecoder)
        {
            delete p_rua->vdecoder;
            p_rua->vdecoder = NULL;
        }

        if (p_rua->vencoder)
        {
            delete p_rua->vencoder;
            p_rua->vencoder = NULL;
        }
#endif

        rua_set_idle(p_rua);
    }

    g_r2f_cls.task_flag = 0;

    RIMG msg;    
	memset(&msg, 0, sizeof(RIMG));
	
	msg.msg_src = R2F_EXIT;

    if (!hqBufPut(g_r2f_cls.msg_queue, (char *) &msg))
    {
    	log_print(HT_LOG_ERR, "%s, hqBufPut failed\r\n", __FUNCTION__);
    }

    while (g_r2f_cls.tid_task)
    {
        usleep(10*1000);
    }

    hqDelete(g_r2f_cls.msg_queue);

    rua_proxy_deinit();
    sys_buf_deinit();
	rtsp_msg_buf_deinit();

#ifdef MJPEG_STREAM
    http_msg_buf_deinit();
#endif

    r2f_free_r2fs(&g_r2f_cfg.r2f);    

    log_print(HT_LOG_DBG, "r2f_stop finished\r\n");

    log_close();
}



