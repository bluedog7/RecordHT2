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

#ifndef MP4_WRITE_H
#define MP4_WRITE_H

#include "mp4_ctx.h"

#ifdef __cplusplus
extern "C" {
#endif

MP4CTX * mp4_write_open(char * filename);
void     mp4_write_close(MP4CTX * p_ctx);
void     mp4_set_video_info(MP4CTX * p_ctx, int fps, int width, int height, const char fcc[4]);
void     mp4_set_audio_info(MP4CTX * p_ctx, int chns, int rate, uint16 fmt);
void     mp4_set_audio_extra_info(MP4CTX * p_ctx, uint8 * extra, int extra_len);
int      mp4_write_header(MP4CTX * p_ctx);
int      mp4_update_header(MP4CTX * p_ctx);
int      mp4_write_audio(MP4CTX * p_ctx, void * p_data, uint32 len);
int      mp4_write_video(MP4CTX * p_ctx, void * p_data, uint32 len, int b_key);
int      mp4_calc_fps(MP4CTX * p_ctx, uint8 * p_data, uint32 len);
int      mp4_parse_video_size(MP4CTX * p_ctx, uint8 * p_data, uint32 len);

#ifdef __cplusplus
}
#endif

#endif


