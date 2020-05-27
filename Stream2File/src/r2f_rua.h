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

#ifndef R2F_RUA_H
#define R2F_RUA_H

#include "rtsp_cln.h"
#include "avi.h"
#ifdef MP4_FORMAT
#include "mp4_ctx.h"
#ifdef AUDIO_CONV
#include "audio_decoder.h"
#include "audio_encoder.h"
#endif
#endif
#ifdef RTMP_STREAM
#include "rtmp_cln.h"
#endif
#ifdef MJPEG_STREAM
#include "http_mjpeg_cln.h"
#endif

#ifdef DEMO
#define MAX_NUM_RUA			2
#else
#define MAX_NUM_RUA			100
#endif

#define R2F_FMT_AVI         0
#define R2F_FMT_MP4         1

typedef struct
{
    uint32  used_flag : 1;      // used flag
    uint32  rtsp_flag : 1;      // rtsp stream
    uint32  rtmp_flag : 1;      // rtmp stream
    uint32  mjpeg_flag: 1;      // mjpeg stream
	uint32  reserved  : 28;     // reserved
	
    char    url[256];           // url address
    char    user[32];           // login user
    char    pass[32];           // login pass    
    char    cfgpath[256];       // recording configured save path    
    char    savepath[256];      // recording save full path
    int     filefmt;            // R2F_FMT_AVI or R2F_FMT_MP4
    int     pnum;               //  Process number For Record Server 
    uint32  framerate;          // video recording frame rate
    time_t  starttime;          // start recording time, unit is second
    uint32  recordsize;         // Recording size configured for each recording, unit is kbyte
    uint32  recordtime;         // Recording time configured for each recording, unit is second

    CRtspClient * rtsp;         // rtsp client 
#ifdef RTMP_STREAM
    CRtmpClient * rtmp;         // rtmp client
#endif
#ifdef MJPEG_STREAM
    CHttpMjpeg  * mjpeg;        // mjpeg client
#endif

    AVICTX* avictx;             // avi context
#ifdef MP4_FORMAT
    MP4CTX      * mp4ctx;       // mp4 context

#ifdef AUDIO_CONV
    CAudioDecoder * adecoder;   // audio decoder
    CAudioEncoder * aencoder;   // audio encoder
#endif
#endif
} RUA;

#ifdef __cplusplus
extern "C" {
#endif

void    rua_proxy_init();
void    rua_proxy_deinit();
RUA   * rua_get_idle();
void    rua_set_online(RUA * p_rua);
void    rua_set_idle(RUA * p_rua);
RUA   * rua_lookup_start();
RUA   * rua_lookup_next(RUA * p_rua);
void    rua_lookup_stop();
uint32  rua_get_index(RUA * p_rua);
RUA   * rua_get_by_index(uint32 index);

#ifdef __cplusplus
}
#endif


#endif // R2F_RUA_H



