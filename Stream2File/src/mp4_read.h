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

#ifndef MP4_READ_H
#define MP4_READ_H

#include "sys_inc.h"
#include "mp4_ctx.h"

#ifdef __cplusplus
extern "C" {
#endif

MP4CTX* mp4_read_open(const char * filename);
void 	mp4_read_close(MP4CTX * p_ctx);
int 	mp4_read_pkt(MP4CTX * p_ctx, MP4PKT * p_pkt);
int 	mp4_seek_pos(MP4CTX * p_ctx, long pos);
int 	mp4_seek_back_pos(MP4CTX * p_ctx);
int 	mp4_seek_tail(MP4CTX * p_ctx);

#ifdef __cplusplus
}
#endif


#endif // MP4_READ_H


