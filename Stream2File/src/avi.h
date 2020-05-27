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

#ifndef AVI_H
#define	AVI_H

#include "format.h"

/* Flags in avih */
#define AVIF_HASINDEX       0x00000010  // Index at end of file?
#define AVIF_ISINTERLEAVED  0x00000100	// is interleaved?
#define AVIF_TRUSTCKTYPE    0x00000800  // Use CKType to find key frames?

#define AVIIF_KEYFRAME      0x00000010L // this frame is a key frame.

#pragma pack(push)
#pragma pack(1)

typedef struct avi_riff
{
	uint32		riff;
	uint32		len;
	uint32		type;
} AVIRIFF;

typedef struct avi_pkt
{
	uint32		type;					// packet type : PACKET_TYPE_UNKNOW, PACKET_TYPE_VIDEO,PACKET_TYPE_AUDIO
	char *		rbuf;					// buffer pointer
	uint32		mlen;					// buffer size
	char *      dbuf;                   // data pointer
	uint32		len;					// packet length
} AVIPKT;

typedef struct avi_main_header
{
	uint32		dwMicroSecPerFrame;		// render time for per frame, unit is ns
	uint32		dwMaxBytesPerSec;		// maximum data transfer rate
	uint32		dwPaddingGranularity;	// The length of the record block needs to be a multiple of this value, usually 2048
	uint32		dwFlags;				// Special attributes of AVI files, such as whether to include index blocks, whether audio and video data is interleaved
	uint32		dwTotalFrames;			// The total number of frames in the file
	uint32		dwInitialFrames;		// How much frames is needed before starting to play
	uint32		dwStreams;				// The number of data streams contained in the file
	uint32		dwSuggestedBufferSize;	// The recommended size of the buffer, usually the sum of the data needed to store an image and synchronize the sound
	uint32		dwWidth;				// Image width
	uint32		dwHeight;				// Image height
	uint32		dwReserved[4];			// Reserved
} AVIMHDR;

typedef struct avi_stream_header
{
	uint32		fccType;				// 4 bytes, indicating the type of data stream, vids for video data stream, auds audio data stream
	uint32		fccHandler;				// 4 bytes, indicating the driver code for data stream decompression
	uint32		dwFlags;				// Data stream attribute
	uint16		wPriority;				// Play priority of this stream
	uint16		wLanguage;				// Audio language code
	uint32		dwInitialFrames;		// How much frames is needed before starting to play
	uint32		dwScale;				// The amount of data, the size of each video or the sample size of the audio
	uint32		dwRate;					// dwScale / dwRate = Number of samples per second
	uint32		dwStart;				// The location where the data stream starts playing, in dwScale
	uint32		dwLength;				// The amount of data in the data stream, in dwScale
	uint32		dwSuggestedBufferSize;	// Recommended buffer size
	uint32		dwQuality;				// Decompress quality parameters, the larger the value, the better the quality
	uint32		dwSampleSize;			// Sample size of the audio
	
	struct 
	{
		short	left;
		short	top;
		short	right;
		short	bottom;
	} rcFrame;
} AVISHDR;

typedef struct bitmap_info_header
{
	uint32		biSize;
	int			biWidth;
	int			biHeight;
	uint16		biPlanes;
	uint16		biBitCount;
	uint32		biCompression;
	uint32		biSizeImage;
	int			biXPelsPerMeter;
	int			biYPelsPerMeter;
	uint32		biClrUsed;
	uint32		biClrImportant;
} BMPHDR;

typedef struct wave_format
{
	uint16		wFormatTag;         	// format type
	uint16		nChannels;          	// number of channels (i.e. mono, stereo...)
	uint32		nSamplesPerSec;     	// sample rate
	uint32		nAvgBytesPerSec;    	// for buffer estimation
	uint16		nBlockAlign;        	// block size of data
	uint16		wBitsPerSample;     	// number of bits per sample of mono data
	uint16		cbSize;             	// the count in bytes of the size of

	// extra information (after cbSize)
} WAVEFMT;

#pragma pack(pop)

typedef struct avi_file_context
{
	uint32		ctxf_read	: 1;	    // Read mode
	uint32		ctxf_write	: 1;	    // Write mode
	uint32		ctxf_idx	: 1;	    // File has index
	uint32		ctxf_audio	: 1;	    // Has audio stream
	uint32		ctxf_video	: 1;	    // Has video stream
	uint32		ctxf_sps_f	: 1;	    // Auxiliary calculation of image size usage, already filled in SPS in avcc
	uint32		ctxf_pps_f	: 1;	    // Auxiliary calculation of image size usage, already filled in PPS in avcc	
	uint32		ctxf_idx_m	: 1;	    // Index data write mode: = 1, memory mode; = 0, temporary file
	uint32      ctxf_nalu   : 1;
	uint32      ctxf_iframe : 1;
	uint32		ctxf_res	: 22;

	AVIMHDR		avi_hdr;                // AVI main header
	AVISHDR		str_v;                  // Video stream header
	BMPHDR		bmp;                    // BITMAP format
	AVISHDR		str_a;                  // Audio stream header    
	WAVEFMT		wave;                   // WAVE format

	FILE *		f;					    // Read and write file handle
	uint32		flen;				    // Total file length, used when reading
	char		filename[256];		    // File full path
	void *		mutex;				    // Write, close mutex

	uint32		v_fps;				    // Video frame rate
	char		v_fcc[4];			    // Video compression standard, "H264","H265","JPEG","MP4V"
	int			v_width;			    // Video width
	int			v_height;			    // Video height
    uint8 * 	v_extra;
	int			v_extra_len;

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

	int			i_movi;				    // Where the real data starts
	int			i_movi_end;			    // End of data, where the index starts
	int			i_riff;				    // After the index, the length of the entire file

	int			pkt_offset;			    // Packet offset when reading
	int			index_offset;		    // Index position when reading
	int			back_index;			    // The first read index position when reading in reverse order, usually sps/vps

	int			i_frame_video;		    // Video frame read and write count
	int			i_frame_audio;		    // Audio frame read and write count

	uint32		s_time;				    // Start recording time = first packet write time
	uint32		e_time;				    // The time when a package was recently written

	int			i_idx_max;			    // The maximum number of indexes currently allocated
	int			i_idx;				    // Current index number (video index + audio index)
	int *		idx;				    // Index array

	FILE *		idx_f;				    // Index temporary file
	int			idx_fix[128];		    // Index file data is enough to write once for one sector
	int			idx_fix_off;		    // The index data has been stored in the offset of idx_fix
} AVICTX;

#endif	// AVI_H



