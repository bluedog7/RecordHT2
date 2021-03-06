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

#ifndef	__H_HTTP_H__
#define	__H_HTTP_H__

#ifdef HTTPS
#include "openssl/ssl.h"
#endif

/***************************************************************************************/
typedef enum http_request_msg_type
{
	HTTP_MT_NULL = 0,
	HTTP_MT_GET,
	HTTP_MT_HEAD,
	HTTP_MT_MPOST,
	HTTP_MT_MSEARCH,
	HTTP_MT_NOTIFY,
	HTTP_MT_POST,
	HTTP_MT_SUBSCRIBE,
	HTTP_MT_UNSUBSCRIBE,
} HTTP_MT;

/***************************************************************************************/
typedef enum http_content_type
{
	CTT_NULL = 0,
	CTT_SDP,
	CTT_TXT,
	CTT_HTM,
	CTT_XML,
	CTT_BIN,
	CTT_JPG,
	CTT_RTSP_TUNNELLED,
	CTT_MULTIPART
} HTTPCTT;

#define ctt_is_string(type)	(type == CTT_XML || type == CTT_HTM || type == CTT_TXT || type == CTT_SDP)


typedef struct _http_msg_content
{
	uint32	    msg_type;
	uint32	    msg_sub_type;
	HDRV        first_line;

	PPSN_CTX    hdr_ctx;
	PPSN_CTX    ctt_ctx;

	int         hdr_len;
	int         ctt_len;
	HTTPCTT     ctt_type;
	char        boundary[256];

	char *      msg_buf;
	int         buf_offset;

	uint32	    remote_ip;
	uint16	    remote_port;
	uint16	    local_port;
} HTTPMSG;

/*************************************************************************/
typedef struct http_client
{
	SOCKET      cfd;
	uint32	    rip;
	uint32	    rport;

	uint32	    guid;

	char        rcv_buf[2048];
	char *      dyn_recv_buf;
	int         rcv_dlen;
	int         hdr_len;
	int	        ctt_len;
	char *      p_rbuf;				// --> rcv_buf or dyn_recv_buf
	int         mlen;				// = sizeof(rcv_buf) or size of dyn_recv_buf
} HTTPCLN;

typedef struct http_req
{	
	SOCKET      cfd;    
	uint32	    port;
	char	    host[256];
	char	    url[256];
	char	    user[64];
	char	    pass[64];

	char 	    action[256];
	char	    rcv_buf[2048];
	char *	    dyn_recv_buf;
	int		    rcv_dlen;
	int		    hdr_len;
	int		    ctt_len;
	char *	    p_rbuf;				// --> rcv_buf or dyn_recv_buf
	int		    mlen;				// = sizeof(rcv_buf) or size of dyn_recv_buf

	HTTPMSG *   rx_msg;

	BOOL 	    need_auth;
	int  	    auth_mode;    		// 0 - baisc; 1 - digest
	HD_AUTH_INFO auth_info;

	int         https;
	
#ifdef HTTPS
	SSL *       ssl;
#endif
} HTTPREQ;

/*************************************************************************/
typedef struct http_srv_s
{
	int         r_flag;

	SOCKET      sfd;
	int         sport;
	uint32	    saddr;

	uint32	    guid;

	PPSN_CTX *  cln_fl;
	PPSN_CTX *  cln_ul;

	pthread_t   rx_tid;
} HTTPSRV;

#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}
#endif

#endif	//	__H_HTTP_H__




