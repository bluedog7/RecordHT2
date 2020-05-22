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

#ifndef R2F_H
#define R2F_H

#include "sys_inc.h"
#include "hqueue.h"
#include "r2f_rua.h"


#define R2F_NTF_SRC     1
#define R2F_EXIT        10

typedef struct
{
    uint32      task_flag   : 1;
    uint32      reserved    : 31;
    
    HQUEUE    * msg_queue;

    pthread_t   tid_task;
} R2F_CLS;

typedef struct
{
	uint32      msg_src;        // message type
	uint32      msg_dua;        // message destination unit
	uint32      msg_evt;        // event / command value
} RIMG;

#ifdef __cplusplus
extern "C" {
#endif

BOOL r2f_start();
void r2f_stop();
int  r2f_record_aac(RUA * p_rua, uint8 * pdata, int len);
int  r2f_record_audio(RUA * p_rua, uint8 * pdata, int len);
int  r2f_record_video(RUA * p_rua, uint8 * pdata, int len, uint32 ts);
BOOL r2f_switch_check(RUA * p_rua); 
void r2f_file_switch(RUA * p_rua);
int  r2f_notify_callback(int event, void * puser);


#ifdef __cplusplus
}
#endif

#endif // R2F_H


