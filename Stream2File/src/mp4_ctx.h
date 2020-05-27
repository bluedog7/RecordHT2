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

#ifndef MP4_CTX_H
#define MP4_CTX_H

#include "format.h"

extern "C"
{
#include "gpac/isomedia.h"
#include "gpac/internal/media_dev.h"
#include "gpac/constants.h"
}

typedef struct mp4_pkt
{
	uint32		type;					// packet type : PACKET_TYPE_UNKNOW, PACKET_TYPE_VIDEO,PACKET_TYPE_AUDIO
	char *		rbuf;					// buffer pointer
	uint32		mlen;					// buffer size
	char *      dbuf;                   // data pointer
	uint32		len;					// packet length
} MP4PKT;

typedef struct mp4_file_context
{
    uint32		ctxf_read	: 1;	    // Read mode
	uint32		ctxf_write	: 1;	    // Write mode
	uint32		ctxf_audio	: 1;	    // Has audio stream
	uint32		ctxf_video	: 1;	    // Has video stream
	uint32		ctxf_sps_f	: 1;	    // Auxiliary calculation of image size usage, already filled in SPS in avcc
	uint32		ctxf_pps_f	: 1;	    // Auxiliary calculation of image size usage, already filled in PPS in avcc
	uint32		ctxf_vps_f	: 1;	    // Auxiliary calculation of image size usage, already filled in VPS in avcc
    uint32      ctxf_nalu   : 1;
	uint32      ctxf_iframe : 1;
	uint32		ctxf_res	: 23;

    int         v_track_id;             // video track id     
    uint32      v_stream_idx;           // video stream index
    uint32      v_timestamp;            // video timestamp     
    int         a_track_id;             // audio track id     
    uint32      a_stream_idx;           // audio stream index
    uint32      a_timestamp;            // audio timestamp
    
    GF_ISOFile *handler;                // Read and write file handle
	char		filename[256];		    // File full path
	void *		mutex;				    // Write, close mutex
	
    uint32		v_fps;				    // Video frame rate
	char		v_fcc[4];			    // Video compression standard, "H264","H265","JPEG","MP4V"
	int			v_width;			    // Video width
	int			v_height;			    // Video height

    uint8       vps[512];
    int         vps_len;
    uint8       sps[512];
    int         sps_len;
    uint8       pps[512];
    int         pps_len;
    
	int			a_rate;				    // Sampling frequency
	uint16		a_fmt;				    // Audio compression standard
	int			a_chns;				    // Number of audio channels
	uint8 * 	a_extra;
	int			a_extra_len;

    int         v_frame_idx;
    int         a_frame_idx;
    
	int			i_frame_video;		    // Video frame read and write count
	int			i_frame_audio;		    // Audio frame read and write count

	uint32		s_time;				    // Start recording time = first packet write time
	uint32		e_time;				    // The time when a package was recently written
} MP4CTX;


#endif // MP4_CTX_H


