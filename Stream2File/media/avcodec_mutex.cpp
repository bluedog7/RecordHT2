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

#include "sys_inc.h"
#include "avcodec_mutex.h"

void * g_avcodec_mutex = sys_os_create_mutex();

int avcodec_thread_open(AVCodecContext *avctx, const AVCodec *codec, AVDictionary **options)
{
    int ret;
    
    sys_os_mutex_enter(g_avcodec_mutex);
    ret = avcodec_open2(avctx, codec, options);
    sys_os_mutex_leave(g_avcodec_mutex);

    return ret;
}

int avcodec_thread_close(AVCodecContext *avctx)
{
    int ret;
    
    sys_os_mutex_enter(g_avcodec_mutex);
    ret = avcodec_close(avctx);
    sys_os_mutex_leave(g_avcodec_mutex);

    return ret;
}



