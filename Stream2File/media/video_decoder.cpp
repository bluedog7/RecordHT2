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

#include "video_decoder.h"
#include "avcodec_mutex.h"

CVideoDecoder::CVideoDecoder()
{
    m_bInited = FALSE;
    m_nCodec = VIDEO_CODEC_NONE;
	
	m_pCodec = NULL;
	m_pContext = NULL;
	m_pFrame = NULL;

	m_pCallback = NULL;
	m_pUserdata = NULL;
}

CVideoDecoder::~CVideoDecoder()
{
	uninit();
}

void CVideoDecoder::uninit()
{
	flush();
	
	if (m_pContext)
	{
		avcodec_thread_close(m_pContext);
		avcodec_free_context(&m_pContext);		
	}

	if (m_pFrame)
	{
	    av_frame_free(&m_pFrame);
	}

	m_bInited = FALSE;
}

BOOL CVideoDecoder::init(int codec)
{
	if (VIDEO_CODEC_H264 == codec)
	{
		m_pCodec = avcodec_find_decoder(AV_CODEC_ID_H264);
	}
	else if (VIDEO_CODEC_JPEG == codec)
	{
		m_pCodec = avcodec_find_decoder(AV_CODEC_ID_MJPEG);		
	}
	else if (VIDEO_CODEC_MP4 == codec)
	{
		m_pCodec = avcodec_find_decoder(AV_CODEC_ID_MPEG4);		
	}
	else if (VIDEO_CODEC_H265 == codec)
	{
		m_pCodec = avcodec_find_decoder(AV_CODEC_ID_H265);		
	}
	
	if (m_pCodec == NULL)
	{
	    log_print(HT_LOG_ERR, "%s, m_pCodec is NULL\r\n", __FUNCTION__);
		return FALSE;
	}
	
	m_pContext = avcodec_alloc_context3(m_pCodec);
	if (NULL == m_pContext)
	{
	    log_print(HT_LOG_ERR, "%s, avcodec_alloc_context3 failed\r\n", __FUNCTION__);
		return FALSE;
	}

	if (VIDEO_CODEC_H264 == codec)
	{
		m_pContext->flags2 |= AV_CODEC_FLAG2_CHUNKS;
	}	

	if (m_pCodec->capabilities & AV_CODEC_CAP_TRUNCATED) 
	{
    	m_pContext->flags |= AV_CODEC_FLAG_TRUNCATED;
	}

    m_pContext->flags |= AV_CODEC_FLAG_LOW_DELAY;
	m_pContext->flags2 |= AV_CODEC_FLAG2_FAST;

	/* ***** Output always the frames ***** */
    m_pContext->flags |= AV_CODEC_FLAG_OUTPUT_CORRUPT;

	if (avcodec_thread_open(m_pContext, m_pCodec, NULL) < 0)
	{
	    log_print(HT_LOG_ERR, "%s, avcodec_thread_open failed\r\n", __FUNCTION__);
		return FALSE;
	}

	m_pFrame = av_frame_alloc();
	if (NULL == m_pFrame)
	{	
	    log_print(HT_LOG_ERR, "%s, av_frame_alloc failed\r\n", __FUNCTION__);
		return FALSE;
	}

	m_nCodec = codec;
	m_bInited = TRUE;
	
	return TRUE;
}

BOOL CVideoDecoder::decode(AVPacket * pkt)
{
	if (!m_bInited)
	{
		return FALSE;
	}
	
	int ret = avcodec_send_packet(m_pContext, pkt);
	if (ret < 0) 
	{       
		log_print(HT_LOG_ERR, "%s, error sending a packet for decoding\r\n", __FUNCTION__); 
		return FALSE;   
	}  
	
	while (ret >= 0)
	{        
		ret = avcodec_receive_frame(m_pContext, m_pFrame);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
		{
			return TRUE; 
		}		
		else if (ret < 0)
		{     
			log_print(HT_LOG_ERR, "%s, error during decoding\r\n", __FUNCTION__);
			return FALSE;     
		} 

		render();	
	}

	return TRUE;
}

BOOL CVideoDecoder::decode(uint8 * data, int len)
{
	AVPacket packet;
	
	av_init_packet(&packet);

	packet.data = data;
	packet.size = len;

	return decode(&packet);
}

int CVideoDecoder::render()
{
	if (m_pCallback)
	{
		m_pCallback(m_pFrame, m_pUserdata);
	}

	return 1;
}

void CVideoDecoder::flush()
{
	if (NULL == m_pContext || 
		NULL == m_pContext->codec || 
		!(m_pContext->codec->capabilities | AV_CODEC_CAP_DELAY))
	{
		return;
	}
	
	decode(NULL);
}




