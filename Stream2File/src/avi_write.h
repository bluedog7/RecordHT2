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

#ifndef AVI_WRITE_H
#define AVI_WRITE_H

#include "sys_inc.h"
#include "avi.h"


#ifdef __cplusplus
extern "C" {
#endif

void 	avi_free_idx(AVICTX * p_ctx);
int 	avi_write_idx(AVICTX * p_ctx);
void 	avi_set_dw(void * p, uint32 dw);
int 	avi_end(AVICTX * p_ctx);
AVICTX* avi_write_open(const char * filename);
int 	avi_write_video(AVICTX * p_ctx, void * p_data, uint32 len, int b_key);
int 	avi_write_audio(AVICTX * p_ctx, void * p_data, uint32 len);
void 	avi_write_close(AVICTX * p_ctx);
void 	avi_set_video_info(AVICTX * p_ctx, int fps, int width, int height, const char fcc[4]);
void    avi_set_video_extra_info(AVICTX * p_ctx, uint8 * extra, int extra_len);
void 	avi_set_audio_info(AVICTX * p_ctx, int chns, int rate, uint16 fmt);
void    avi_set_audio_extra_info(AVICTX * p_ctx, uint8 * extra, int extra_len);
void 	avi_build_video_hdr(AVICTX * p_ctx);
void 	avi_build_audio_hdr(AVICTX * p_ctx);
int 	avi_write_header(AVICTX * p_ctx);
int 	avi_update_header(AVICTX * p_ctx);
int	 	avi_calc_fps(AVICTX * p_ctx, uint8 * p_data, uint32 len);
int 	avi_parse_video_size(AVICTX * p_ctx, uint8 * p_data, uint32 len);


#ifdef __cplusplus
}
#endif


#endif // AVI_WRITE_H



