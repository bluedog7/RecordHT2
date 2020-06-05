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

#ifndef VIDEO_DECODER_H
#define VIDEO_DECODER_H

#include "sys_inc.h"
#include "media_format.h"

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"
#include "libswscale/swscale.h"
#include "libavformat/avformat.h"
}

typedef void (*VDCB)(AVFrame * frame, void *pUserdata);

class CVideoDecoder
{
public:
	CVideoDecoder();
	~CVideoDecoder();

public:
	BOOL init(int codec);
	void uninit();
	
	BOOL decode(uint8 * data, int len);
	BOOL decode(AVPacket *pkt);
	void setCallback(VDCB pCallback, void * pUserdata) {m_pCallback = pCallback; m_pUserdata = pUserdata;}

private:
	int  render();
    void flush();
    
private:
	BOOL			m_bInited;
    int		        m_nCodec;
	
	AVCodec *       m_pCodec;
	AVCodecContext* m_pContext;
	AVFrame *		m_pFrame;

	VDCB            m_pCallback;
    void *          m_pUserdata;
};

#endif


