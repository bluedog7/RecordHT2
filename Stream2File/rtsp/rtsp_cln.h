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

#ifndef RTSP_CLN_H
#define RTSP_CLN_H

#include "sys_buf.h"
#include "rtsp_rcua.h"
#include "media_format.h"
#include "h264_rtp_rx.h"
#include "mjpeg_rtp_rx.h"
#include "h265_rtp_rx.h"
#include "mpeg4_rtp_rx.h"
#include "aac_rtp_rx.h"
#include "pcm_rtp_rx.h"


typedef int (*notify_cb)(int, void *);
typedef int (*video_cb)(uint8 *, int, uint32, uint16, void *);
typedef int (*audio_cb)(uint8 *, int, uint32, uint16, void *);
typedef int (*metadata_cb)(uint8 *, int, uint32, uint16, void *);


#define RTSP_EVE_STOPPED    0
#define RTSP_EVE_CONNECTING 1
#define RTSP_EVE_CONNFAIL   2
#define RTSP_EVE_CONNSUCC   3
#define RTSP_EVE_NOSIGNAL   4
#define RTSP_EVE_RESUME     5
#define RTSP_EVE_AUTHFAILED 6
#define RTSP_EVE_NODATA   	7

#define RTSP_RX_FAIL        -1
#define RTSP_RX_TIMEOUT     1
#define RTSP_RX_SUCC        2

#define RTSP_PARSE_FAIL		-1
#define RTSP_PARSE_MOREDATE 0
#define RTSP_PARSE_SUCC		1



class CRtspClient
{
public:
	CRtspClient(void);
	~CRtspClient(void);

public:
	BOOL    rtsp_start(const char * suffix, const char * ip, int port, const char * user, const char * pass);
	BOOL    rtsp_start(const char * url, char * user, char * pass);
	BOOL    rtsp_play();
	BOOL    rtsp_stop();
	BOOL    rtsp_pause();
	BOOL    rtsp_close();

	RCUA *  get_rua() {return &m_rua;}
	char *  get_url() {return m_url;}
	char *  get_ip() {return  m_ip;}
	int     get_port() {return m_nport;}
	char *  get_user() {return m_rua.auth_info.auth_name;}
	char *  get_pass() {return m_rua.auth_info.auth_pwd;}
	void    set_notify_cb(notify_cb notify, void * userdata);
	void    set_video_cb(video_cb cb);
	void    set_audio_cb(audio_cb cb);
	void    set_metadata_cb(metadata_cb cb);

	/**
      * @desc : Set rtp multicast 
      * @params :
      *    flag : 1 - enable rtp multcast, 0 - disable rtp multcast
      */
	void    set_rtp_multicast(int flag);

	/**
      * @desc : Set rtp over udp 
      * @params :
      *    flag : 1 - enable rtp over udp, 0 - disable rtp over udp
      */
	void    set_rtp_over_udp(int flag);

	/**
      * @desc : Set rtsp over http 
      * @params :
      *    flag : 1 - enable rtsp over http, 0 - disable rtsp over http
      *    port : if flag = 1, specify the http port
      */
	void    set_rtsp_over_http(int flag, int port);

    /**
      * @desc : Set the data rx timeout, if the data is not received within the specified time, 
      *         send RTSP_EVE_NODATA notification
      * @params :
      *    timeout : Timeout value, unit is second
      */
	void    set_rx_timeout(int timeout);

	/**
      * @desc : Get H264, SPS, PPS parameters, and return through the video callback function
      */
	void 	get_h264_params();

	/**
      * @desc : Get H264, SPS, PPS parameters
      * @params
      *     p_sps : Receive H264 SPS data, the buffer size is not less than 256
      *     sps_len : Receive H264 SPS buffer length
      *     p_pps : Receive H264 PPS data, the buffer size is not less than 256
      *     pps_len : Receive H264 PPS buffer length
      */
	BOOL 	get_h264_params(uint8 * p_sps, int * sps_len, uint8 * p_pps, int * pps_len);

	/**
      * @desc : Get H265, VPS, SPS, PPS parameters, and return through the video callback function
      */
	void 	get_h265_params();

	/**
      * @desc : Get H265, VPS, SPS, PPS parameters
      * @params
      *     p_sps : Receive H264 SPS data, the buffer size is not less than 256
      *     sps_len : Receive H264 SPS buffer length
      *     p_pps : Receive H264 PPS data, the buffer size is not less than 256
      *     pps_len : Receive H264 PPS buffer length
      *     p_vps : Receive H264 VPS data, the buffer size is not less than 256
      *     vps_len : Receive H264 VPS buffer length
      */
	BOOL    get_h265_params(uint8 * p_sps, int * sps_len, uint8 * p_pps, int * pps_len, uint8 * p_vps, int * vps_len);

    /**
      * @desc : If the describe request response returned by the server has an H264 SDP description, get the H264 SDP description
      * @params
      *     p_sdp : Receive H264 SDP description
      *     max_len : Maximum length of buffer p_sdp
      */
	BOOL    get_h264_sdp_desc(char * p_sdp, int max_len);

	/**
      * @desc : If the describe request response returned by the server has an H265 SDP description, get the H265 SDP description
      * @params
      *     p_sdp : Receive H265 SDP description
      *     max_len : Maximum length of buffer p_sdp
      */
	BOOL    get_h265_sdp_desc(char * p_sdp, int max_len);

	/**
      * @desc : If the describe request response returned by the server has an H265 SDP description, get the MP4 SDP description
      * @params
      *     p_sdp : Receive MP4 SDP description
      *     max_len : Maximum length of buffer p_sdp
      */
	BOOL    get_mp4_sdp_desc(char * p_sdp, int max_len);

	/**
      * @desc : If the describe request response returned by the server has an H265 SDP description, get the AAC SDP description
      * @params
      *     p_sdp : Receive AAC SDP description
      *     max_len : Maximum length of buffer p_sdp
      */
	BOOL    get_aac_sdp_desc(char * p_sdp, int max_len);

    /**
      * @desc : Get video codec
      * @retrun : reference the media_format.h, VIDEO_CODEC_H264, ...
      */
	int     audio_codec() {return m_AudioCodec;}

	/**
      * @desc : Get audio codec
      * @retrun : reference the media_format.h, AUDIO_CODEC_AAC, ...
      */
	int     video_codec() {return m_VideoCodec;}

	/**
      * @desc : Get audio sample rate
      */
	int     get_audio_samplerate() {return m_nSamplerate;}

	/**
      * @desc : Get audio channel numbers
      */
	int     get_audio_channels() {return m_nChannels;}

	/**
      * @desc : Get audio config data, for AAC
      */
	uint8 * get_audio_config() {return m_pAudioConfig;}

	/**
      * @desc : Get audio config data length, for AAC
      */
	int     get_audio_config_len() {return m_nAudioConfigLen;}

    /**
      * @desc : get the audio backchannel flag
      */
    int     get_bc_flag();

    /**
      * @desc : set the audio backchannel flag
      * @params
      *    flag : 1 - enable audio backchannel; 0 - disable audio backchannel
      */
    void    set_bc_flag(int flag);

    /**
      * @desc : get the audio backchannel data sending flag
      */
    int     get_bc_data_flag();

    /**
      * @desc : set the audio backchannel data sending flag
      * @params
      *    flag : 1 - enable audio backchannel data sending; 0 - disable audio backchannel data sending
      */
    void    set_bc_data_flag(int flag);

    /**
      * @desc : get the replay flag
      */
    int     get_replay_flag();

    /**
      * @desc : set the replay flag
      * @params
      *    flag : 1 - enable replay; 0 - disable replay
      */
    void    set_replay_flag(int flag);

    /**
      * @desc : set the scale info
      * @params
      *    scale : when not set the rata control flag, the scale is valid.
      *            It shall be either 1.0 or -1.0, to indicate forward or reverse playback respectively. 
      *            If it is not present, forward playback is assumed
      */
    void    set_scale(double scale);

    /**
      * @desc : set the rate control flag
      * @params
      *    flag : 
      *        1-the stream is delivered in real time using standard RTP timing mechanisms
      *        0-the stream is delivered as fast as possible, using only the flow control provided by the transport to limit the delivery rate
      */
    void    set_rate_control_flag(int flag);

    /**
      * @desc : set the immediate flag
      * @params
      *    flag : 
      *        1 - immediately start playing from the new location, cancelling any existing PLAY command.
      *            The first packet sent from the new location shall have the D (discontinuity) bit set in its RTP extension header. 
      */
    void    set_immediate_flag(int flag);

    /**
      * @desc : set the frame flag
      * @params
      *    flag : 
      *      0 - all frames
      *      1 - I-frame and P-frame
      *      2 - I-frame
      * interval : 
      *      when flag = 2, set the I-frame interval
      */
    void    set_frames_flag(int flag, int interval);

    /**
      * @desc : set replay range
      * @params
      *    start : the replay start timestamp
      *    end : the replay end timestamp
      */
    void    set_replay_range(time_t start, time_t end);

    void    tcp_rx_thread();
    void    udp_rx_thread();
    void    rtsp_video_data_cb(uint8 * p_data, int len, uint32 ts, uint32 seq);
    void    rtsp_audio_data_cb(uint8 * p_data, int len, uint32 ts, uint32 seq);

    static BOOL parse_url(char const* url, char*& user, char*& pass, char*& addr, int& port, char const** suffix);
    static void copy_str_from_url(char* dest, char const* src, uint32 len);
    
private:
    void    set_default();
    BOOL    rtsp_client_start();
	BOOL    rua_init_connect(RCUA * p_rua);
	void    rtsp_client_stop(RCUA * p_rua);
	BOOL    rtsp_client_state(RCUA * p_rua, HRTSP_MSG * rx_msg);	
    int     rtsp_tcp_rx();
    int     rtsp_udp_rx();
    int     rtsp_msg_parser(RCUA * p_rua);
    void    rtsp_keep_alive();
    BOOL    rtsp_setup_channel(RCUA * p_rua, int av_t);
    
    BOOL    make_prepare_play();
    BOOL    rtsp_options_res(RCUA * p_rua, HRTSP_MSG * rx_msg);
    BOOL    rtsp_describe_res(RCUA * p_rua, HRTSP_MSG * rx_msg);
    BOOL    rtsp_setup_video_res(RCUA * p_rua, HRTSP_MSG * rx_msg);
    BOOL    rtsp_setup_audio_res(RCUA * p_rua, HRTSP_MSG * rx_msg);  
    BOOL    rtsp_setup_metadata_res(RCUA * p_rua, HRTSP_MSG * rx_msg);
    BOOL    rtsp_play_res(RCUA * p_rua, HRTSP_MSG * rx_msg);

    void    metadata_rtp_rx(uint8 * lpData, int rlen, uint32 seq, uint32 ts);

    BOOL    rtsp_setup_backchannel_res(RCUA * p_rua, HRTSP_MSG * rx_msg);
    BOOL    rtsp_init_backchannel(RCUA * p_rua);
    BOOL    rtsp_get_bc_media_info();

    void    send_notify(int event);

    BOOL    rtsp_get_transport_info(RCUA * p_rua, HRTSP_MSG * rx_msg, int av_type);
    BOOL 	rtsp_get_video_media_info();
    BOOL 	rtsp_get_audio_media_info();
    
	void 	tcp_data_rx(uint8 * lpData, int rlen);
	void 	udp_data_rx(uint8 * lpData, int rlen, int type);
		
	void 	rtsp_send_h264_params(RCUA * p_rua);    
	void    rtsp_send_h265_params(RCUA * p_rua);
	void	rtsp_get_mpeg4_config(RCUA * p_rua);
	void    rtsp_get_aac_config(RCUA * p_rua);

    int     rtsp_build_http_get_req(void * p_user, char * bufs, char * cookie);
    int     rtsp_build_http_post_req(void * p_user, char * bufs, char * cookie);
    BOOL    rtsp_over_http_start();
    
private:
	RCUA		    m_rua;
	char            m_url[256];
	char            m_ip[128];
	char            m_suffix[128];
	int             m_nport;
	
	notify_cb       m_pNotify;
	void *          m_pUserdata;
	video_cb        m_pVideoCB;
	audio_cb        m_pAudioCB;
	metadata_cb     m_pMetadataCB;
	void *			m_pMutex;
	int             m_nRxTimeout;

    union {
	    H264RXI     h264rxi;
	    H265RXI     h265rxi;
	    MJPEGRXI    mjpegrxi;
	    MPEG4RXI    mpeg4rxi;
	};

    union {
        AACRXI      aacrxi;
        PCMRXI      pcmrxi;
    };

    RTPRXI          rtprxi;

    uint8 *         m_pAudioConfig;
	uint32          m_nAudioConfigLen;
	
	BOOL            m_bRunning;
	pthread_t       m_tcpRxTid;
	pthread_t       m_udpRxTid;

	int		        m_VideoCodec;
	
	int     		m_AudioCodec;
	int             m_nSamplerate;
	int             m_nChannels;

    int     		m_bcAudioCodec;
	int             m_nbcSamplerate;
	int             m_nbcChannels;
};



#endif	// RTSP_CLN_H



