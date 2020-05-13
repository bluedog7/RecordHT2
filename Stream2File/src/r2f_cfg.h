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

#ifndef R2F_CFG_H
#define R2F_CFG_H

typedef struct _STREAM2FILE
{
    struct _STREAM2FILE * next;
    
    char    url[512];
    char    user[32];
    char    pass[32];
    char    savepath[256];
    int     filefmt;
    uint32  framerate;
    uint32  recordsize;
    uint32  recordtime;
} STREAM2FILE;

typedef struct
{
    BOOL    log_enable;         // log enable 
    int     log_level;          // log level

    STREAM2FILE * r2f;
} R2F_CFG;

#ifdef __cplusplus
extern "C"{
#endif

extern R2F_CFG g_r2f_cfg;

void r2f_free_r2fs(STREAM2FILE ** p_r2f);
BOOL r2f_read_config();

#ifdef __cplusplus
}
#endif


#endif // R2F_CFG_H


