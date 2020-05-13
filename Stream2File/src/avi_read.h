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

#ifndef AVI_READ_H
#define AVI_READ_H

#include "sys_inc.h"
#include "avi.h"


#ifdef __cplusplus
extern "C" {
#endif

AVICTX* avi_read_open(const char * filename);
void 	avi_read_close(AVICTX * p_ctx);
int 	avi_read_pkt(AVICTX * p_ctx, AVIPKT * p_pkt);
int 	avi_seek_pos(AVICTX * p_ctx, long pos, long total);
int 	avi_seek_back_pos(AVICTX * p_ctx);
int 	avi_seek_tail(AVICTX * p_ctx);

#ifdef __cplusplus
}
#endif


#endif // AVI_READ_H



