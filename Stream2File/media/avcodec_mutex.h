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

#ifndef AVCODEC_MUTEX_H
#define AVCODEC_MUTEX_H

extern "C" {
#include "libavcodec/avcodec.h"
}

#ifdef __cplusplus
extern "C" {
#endif

int avcodec_thread_open(AVCodecContext *avctx, const AVCodec *codec, AVDictionary **options);
int avcodec_thread_close(AVCodecContext *avctx);

#ifdef __cplusplus
}
#endif


#endif


