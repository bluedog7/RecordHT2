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

#ifndef	SYS_BUF_H
#define	SYS_BUF_H

/***************************************************************************************/
#define MAX_AVN				8	
#define MAX_AVDESCLEN		500	
#define MAX_USRL			64	
#define MAX_PWDL			32	
#define MAX_NUML			64
#define MAX_UA_ALT_NUM		8


/***************************************************************************************/
typedef struct header_value
{
	char	header[32];
	char *	value_string;
} HDRV;

typedef struct ua_rtp_info
{
	int     rtp_cnt;				
	uint32	rtp_ssrc;				
	uint32	rtp_ts;					
	uint8	rtp_pt;					
} UA_RTP_INFO;

typedef struct
{
    /* rtcp sender statistics */
    
    int64   last_rtcp_ntp_time;
    int64   first_rtcp_ntp_time;
    uint32  packet_count;
    uint32  octet_count;
    uint32  last_octet_count;
    int     first_packet;
    char    cname[64];
} UA_RTCP_INFO;

typedef struct http_digest_auth_info
{
	char    auth_name[MAX_USRL];
	char    auth_pwd[64];
	char    auth_uri[256];			
	char    auth_qop[32];
	char    auth_nonce[128];
	char    auth_cnonce[128];
	char    auth_realm[128];
	char    auth_opaque[128];
	int     auth_nc;
	char    auth_ncstr[12];
	char    auth_response[36];
} HD_AUTH_INFO;

#ifdef __cplusplus
extern "C" {
#endif

extern HT_API PPSN_CTX * hdrv_buf_fl;

/***********************************************************************/
HT_API BOOL     net_buf_init(int num, int size);
HT_API void     net_buf_deinit();

HT_API char   * net_buf_get_idle();
HT_API void     net_buf_free(char * rbuf);
HT_API uint32   net_buf_get_size();
HT_API uint32   net_buf_idle_num();

/***********************************************************************/
HT_API BOOL     hdrv_buf_init(int num);
HT_API void     hdrv_buf_deinit();

HT_API HDRV   * hdrv_buf_get_idle();
HT_API void     hdrv_buf_free(HDRV * pHdrv);
HT_API uint32   hdrv_buf_idle_num();

HT_API void     hdrv_ctx_ul_init(PPSN_CTX * ul_ctx);
HT_API void     hdrv_ctx_free(PPSN_CTX * p_ctx);

/***********************************************************************/
HT_API BOOL     sys_buf_init(int nums);
HT_API void     sys_buf_deinit();
/***********************************************************************/


#ifdef __cplusplus
}
#endif

#endif	// SYS_BUF_H



