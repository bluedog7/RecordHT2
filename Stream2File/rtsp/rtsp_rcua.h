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

#ifndef	RTSP_RCUA_H
#define	RTSP_RCUA_H

#include "rtsp_parse.h"

#ifdef BACKCHANNEL
#if __WINDOWS_OS__
#include "audio_capture_dsound.h"
#elif defined(ANDROID)
#include "audio_input.h"
#endif
#endif

#ifdef OVER_HTTP
#include "http.h"
#endif


#define AV_TYPE_VIDEO       0
#define AV_TYPE_AUDIO       1
#define AV_TYPE_METADATA    2
#define AV_TYPE_BACKCHANNEL 3

#define AV_MAX_CHS          4

#define AV_VIDEO_CH         AV_TYPE_VIDEO
#define AV_AUDIO_CH         AV_TYPE_AUDIO
#define AV_METADATA_CH      AV_TYPE_METADATA
#define AV_BACK_CH          AV_TYPE_BACKCHANNEL


typedef enum rtsp_client_states
{
	RCS_NULL = 0,	
	RCS_OPTIONS,
	RCS_DESCRIBE,	
	RCS_INIT_V,		
	RCS_INIT_A,	
	RCS_INIT_M,
	RCS_INIT_BC,
	RCS_READY,		
	RCS_PLAYING,	
	RCS_RECORDING,	
} RCSTATE;

typedef struct rtsp_client_media_channel
{
    SOCKET          udp_fd;                     // udp socket
    char			ctl[64];                    // control string
    uint16          setup;                      // whether the media channel already be setuped
    uint16	        r_port;                     // remote udp port
	uint16	        l_port;                     // local udp port
	uint16	        interleaved;	            // rtp channel values
    char            destination[32];            // multicast address
    int				cap_count;                  // Local number of capabilities
	uint8	        cap[MAX_AVN];               // Local capability
	char			cap_desc[MAX_AVN][MAX_AVDESCLEN];
} RCMCH;

typedef struct rtsp_client_user_agent
{
	uint32	        used_flag	: 1;    // used flag
	uint32	        rtp_tcp		: 1;    // rtp over tcp
	uint32          mast_flag   : 1;    // use rtp multicast, set by user
	uint32	        rtp_mcast	: 1;    // use rtp multicase, set by stack
	uint32	        rtp_tx		: 1;    // rtp sending flag
	uint32          need_auth   : 1;    // need auth flag
	uint32          auth_mode   : 2;    // 0 - baisc; 1 - digest
	uint32 	        gp_cmd      : 1;	// is support get_parameter command
    uint32 	        backchannel : 1;    // audio backchannle flag
    uint32 	        send_bc_data: 1;    // audio backchannel data sending flag
    uint32          replay      : 1;    // replay flag
    uint32          over_http   : 1;    // rtsp over http flag
    uint32	        reserved	: 19;

	int				state;              // state, RCSTATE
	SOCKET			fd;                 // socket handler
	uint32			keepalive_time;     // keepalive time		    	

	char			ripstr[128];		// remote ip
	uint16	        rport;				// rtsp server port

	uint32	        cseq;               // seq no				
	char			sid[64];			// Session ID
	char			uri[256];			// rtsp://221.10.50.195:554/cctv.sdp
	char			cbase[256];			// Content-Base: rtsp://221.10.50.195:554/broadcast.sdp/
    char            user_agent[64];     // user agent string 
	int				session_timeout;	// session timeout value
	
	char			rcv_buf[2052];		
	int				rcv_dlen;			
	int				rtp_t_len;
	int				rtp_rcv_len;
	char *			rtp_rcv_buf;

    RCMCH           channels[AV_MAX_CHS];   // media channels

    HD_AUTH_INFO 	auth_info;

#ifdef BACKCHANNEL
    UA_RTP_INFO		bc_rtp_info;        // audio rtp info

#if __WINDOWS_OS__
    CAudioCapture * audio_captrue;
#elif defined(ANDROID)
    AudioInput    * audio_input;
#endif

#endif

#ifdef REPLAY
    uint32          scale_flag          : 1;
    uint32          rate_control_flag   : 1;
    uint32          immediate_flag      : 1;
    uint32          frame_flag          : 1;
    uint32          frame_interval_flag : 1;
    uint32          range_flag          : 1;
    uint32          replay_reserved     : 26;
    
    int			    scale;              // scale info, when not set the rata control flag, the scale is valid.
                                        //  It shall be either 100.0 or -100.0, to indicate forward or reverse playback respectively. 
                                        //  If it is not present, forward playback is assumed
										//  100 means 1.0, Divide by 100 when using
	int				rate_control;       // rate control flag, 
								        // 1-the stream is delivered in real time using standard RTP timing mechanisms
								        //  0-the stream is delivered as fast as possible, using only the flow control provided by the transport to limit the delivery rate
	int				immediate;          // 1 - immediately start playing from the new location, cancelling any existing PLAY command.
							            //	The first packet sent from the new location shall have the D (discontinuity) bit set in its RTP extension header. 
	int				frame;              // 0 - all frames
								        // 1 - I-frame and P-frame
								        // 2 - I-frame
	int				frame_interval;     // I-frame interval, unit is milliseconds	
	time_t          replay_start;       // replay start time
	time_t          replay_end;         // replay end time
#endif

#ifdef OVER_HTTP
    uint16          http_port;          // rtsp over http port
    HTTPREQ         rtsp_send;
    HTTPREQ         rtsp_recv;
#endif
} RCUA;


#ifdef __cplusplus
extern "C" {
#endif

/*************************************************************************/
HRTSP_MSG * rua_build_describe(RCUA * p_rua);
HRTSP_MSG * rua_build_setup(RCUA * p_rua,int type);
HRTSP_MSG * rua_build_play(RCUA * p_rua);
HRTSP_MSG * rua_build_pause(RCUA * p_rua);
HRTSP_MSG * rua_build_teardown(RCUA * p_rua);
HRTSP_MSG * rua_build_get_parameter(RCUA * p_rua);
HRTSP_MSG * rua_build_options(RCUA * p_rua);

/*************************************************************************/
BOOL 		rua_get_media_info(RCUA * p_rua, HRTSP_MSG * rx_msg);

BOOL        rua_get_sdp_video_desc(RCUA * p_rua, const char * key, int * pt, char * p_sdp, int max_len);
BOOL        rua_get_sdp_audio_desc(RCUA * p_rua, const char * key, int * pt, char * p_sdp, int max_len);

BOOL        rua_get_sdp_h264_desc(RCUA * p_rua, int * pt, char * p_sdp, int max_len);
BOOL        rua_get_sdp_h264_params(RCUA * p_rua, int * pt, char * p_sps_pps, int max_len);

BOOL        rua_get_sdp_h265_desc(RCUA * p_rua, int * pt, char * p_sdp, int max_len);
BOOL        rua_get_sdp_h265_params(RCUA * p_rua, int * pt, BOOL * donfield, char * p_vps, int vps_len, char * p_sps, int sps_len, char * p_pps, int pps_len);

BOOL        rua_get_sdp_mp4_desc(RCUA * p_rua, int * pt, char * p_sdp, int max_len);
BOOL 		rua_get_sdp_mp4_params(RCUA * p_rua, int * pt, char * p_cfg, int max_len);

BOOL        rua_get_sdp_aac_desc(RCUA * p_rua, int * pt, char * p_sdp, int max_len);
BOOL        rua_get_sdp_aac_params(RCUA * p_rua, int *pt, int *sizelength, int *indexlength, int *indexdeltalength, char * p_cfg, int max_len);

BOOL        rua_init_udp_connection(RCUA * p_rua, int av_t);
BOOL        rua_init_mc_connection(RCUA * p_rua, int av_t);

/*************************************************************************/
void 		rcua_send_rtsp_msg(RCUA * p_rua,HRTSP_MSG * tx_msg);

#define     rcua_send_free_rtsp_msg(p_rua,tx_msg) \
                do { \
                    rcua_send_rtsp_msg(p_rua,tx_msg); \
                    rtsp_free_msg(tx_msg); \
                } while(0)


#ifdef __cplusplus
}
#endif

#endif	//	RTSP_RCUA_H




