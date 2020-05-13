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

#ifndef HTTP_PARSE_H
#define HTTP_PARSE_H

#include "sys_inc.h"
#include "http.h"


#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************************/
HT_API BOOL 	  http_msg_buf_init(int num);
HT_API void 	  http_msg_buf_deinit();

/***********************************************************************/
HT_API BOOL 	  http_is_http_msg(char * msg_buf);
HT_API int        http_pkt_find_end(char * p_buf);
HT_API void 	  http_headl_parse(char * pline, int llen, HTTPMSG * p_msg);
HT_API int 	      http_line_parse(char * p_buf, int max_len, char sep_char, PPSN_CTX * p_ctx);
HT_API BOOL       http_get_headline_uri(HTTPMSG * rx_msg, char * p_uri, int size);
HT_API int 	      http_ctt_parse(HTTPMSG * p_msg);
HT_API int 	      http_msg_parse(char * msg_buf, int msg_buf_len, HTTPMSG * msg);
HT_API int 	      http_msg_parse_part1(char * p_buf, int buf_len, HTTPMSG * msg);
HT_API int 	      http_msg_parse_part2(char * p_buf, int buf_len, HTTPMSG * msg);
HT_API HDRV  	* http_find_headline(HTTPMSG * msg, const char * head);
HT_API HDRV     * http_find_headline_next(HTTPMSG * msg, const char * head, HDRV * hrv);
HT_API char  	* http_get_headline(HTTPMSG * msg, const char * head);
HT_API HDRV  	* http_find_ctt_headline(HTTPMSG * msg, const char * head);
HT_API char  	* http_get_ctt(HTTPMSG * msg);
HT_API BOOL       http_get_auth_digest_info(HTTPMSG * rx_msg, HD_AUTH_INFO * p_auth);

HT_API HTTPMSG  * http_get_msg_buf();
HT_API HTTPMSG  * http_get_msg_large_buf(int size);
HT_API void 	  http_msg_ctx_init(HTTPMSG * msg);
HT_API void 	  http_free_msg_buf(HTTPMSG * msg);
HT_API uint32     http_idle_msg_buf_num();

/***********************************************************************/
HT_API void 	  http_free_msg(HTTPMSG * msg);
HT_API void 	  http_free_msg_content(HTTPMSG * msg);
HT_API void 	  http_free_msg_ctx(HTTPMSG * msg, int type);


#ifdef __cplusplus
}
#endif

#endif



