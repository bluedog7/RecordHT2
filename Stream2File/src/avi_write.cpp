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
#include "avi.h"
#include "avi_write.h"
#include "h264.h"
#include "h265.h"
#include "mjpeg.h"
#include "media_util.h"
#include "h264_util.h"
#include "h265_util.h"
#include "bit_vector.h"
#include <math.h>


int avi_write_uint16_(AVICTX * p_ctx, uint16 w)
{
	return fwrite(&w, 2, 1, p_ctx->f);
}

int avi_write_uint32_(AVICTX * p_ctx, uint32 dw)
{
	return fwrite(&dw, 4, 1, p_ctx->f);
}

int avi_write_fourcc_(AVICTX * p_ctx, const char fcc[4])
{
	return fwrite(fcc, 4, 1, p_ctx->f);
}

int avi_write_buffer_(AVICTX * p_ctx, char * p_data, int len)
{
	return fwrite(p_data, len, 1, p_ctx->f);
}

#define avi_write_uint16(p_ctx, w) do{ if(avi_write_uint16_(p_ctx, w) != 1) goto w_err; }while(0)
#define avi_write_uint32(p_ctx, w) do{ if(avi_write_uint32_(p_ctx, w) != 1) goto w_err; }while(0)
#define avi_write_fourcc(p_ctx, fcc) do{ if(avi_write_fourcc_(p_ctx, fcc) != 1) goto w_err; }while(0)
#define avi_write_buffer(p_ctx, p_data, len) do{ if(avi_write_buffer_(p_ctx, p_data, len) != 1) goto w_err; }while(0)

void avi_free_idx(AVICTX * p_ctx)
{
	if (p_ctx->idx)
	{
		free(p_ctx->idx);
		p_ctx->idx = NULL;
	}
	
	if (p_ctx->idx_f)
	{
		fclose(p_ctx->idx_f);
		p_ctx->idx_f = NULL;
	}

	p_ctx->i_idx = 0;
	p_ctx->i_idx_max = 0;
}

int avi_write_idx(AVICTX * p_ctx)
{
	avi_write_fourcc(p_ctx, "idx1");
	avi_write_uint32(p_ctx,  p_ctx->i_idx * 16);

	if (p_ctx->ctxf_idx_m == 1)
	{
		if (p_ctx->i_idx > 0)
		{
			if (fwrite( p_ctx->idx, p_ctx->i_idx * 16, 1, p_ctx->f) != 1)
			{
				return -1;
			}	

			fflush(p_ctx->f);
		}
	}
	else if (p_ctx->idx_f)
	{
		// Write index data in a fixed buffer to an index file
		if (p_ctx->idx_fix_off > 0)
		{
			if (fwrite(p_ctx->idx_fix, p_ctx->idx_fix_off * 4, 1, p_ctx->idx_f) != 1)
			{
				goto w_err;
            }
            
			fflush(p_ctx->idx_f);

			p_ctx->idx_fix_off = 0;
		}

		// Write index temporary file data to the avi file
		
		fseek(p_ctx->idx_f, 0, SEEK_END);
		
		int idx_len = ftell(p_ctx->idx_f);
		
		if (idx_len != (p_ctx->i_idx * 16))
		{
			log_print(HT_LOG_ERR, "%s, idx real len[%d] != idx file len[%d],idx_fix_off[%d]!!!\r\n", 
			    __FUNCTION__, p_ctx->i_idx * 16, idx_len, p_ctx->idx_fix_off * 4);
			return -1;
		}

		fseek(p_ctx->f, 0, SEEK_END);
		fseek(p_ctx->idx_f, 0, SEEK_SET);
		
		int rlen;
		
		do
		{
			rlen = fread(p_ctx->idx_fix, 1, sizeof(p_ctx->idx_fix), p_ctx->idx_f);
			if (rlen <= 0)
			{
				break;
			}
			
			if (fwrite(p_ctx->idx_fix, rlen, 1, p_ctx->f) != 1)
			{
				log_print(HT_LOG_ERR, "%s, write idx into avi file failed!!!\r\n", __FUNCTION__);
				return -1;
			}
		} while(rlen > 0);
		
		fflush(p_ctx->f);
	}

	return 0;

w_err:

	return -1;
}

void avi_set_dw(void * p, uint32 dw)
{
	uint8 * ptr = (uint8 *)p;

	ptr[0] = (dw      ) & 0xff;
	ptr[1] = (dw >> 8 ) & 0xff;
	ptr[2] = (dw >> 16) & 0xff;
	ptr[3] = (dw >> 24) & 0xff;
}

int avi_end(AVICTX * p_ctx)
{
	if (p_ctx->f == NULL)
	{
		return -1;
    }
    
	p_ctx->i_movi_end = ftell(p_ctx->f);

	if (avi_write_idx(p_ctx) < 0)
	{
		goto end_err;
    }
    
	p_ctx->i_riff = ftell(p_ctx->f);

	if (avi_write_header(p_ctx) < 0)
	{
		goto end_err;
    }
    
	// AVI file has been completed, delete the temporary index file
	if (p_ctx->idx_f)
	{
		fclose(p_ctx->idx_f);
		p_ctx->idx_f = NULL;
		
#if __WINDOWS_OS__
        char filename[256];
        sprintf(filename, "%s.idx", p_ctx->filename);
        remove(filename);
#else

		char cmds[256];
		sprintf(cmds, "rm -f %s.idx", p_ctx->filename);
		system(cmds);
#endif		
	}

end_err:

	if (p_ctx->f)
	{
		fclose(p_ctx->f);
		p_ctx->f = NULL;
	}

	avi_free_idx(p_ctx);

	return 0;
}

/**************************************************************************/
AVICTX * avi_write_open(const char * filename)
{
	AVICTX * p_ctx = (AVICTX *)malloc(sizeof(AVICTX));
	if (p_ctx == NULL)
	{
		log_print(HT_LOG_ERR, "%s, malloc fail!!!\r\n", __FUNCTION__);
		return NULL;
	}

	memset(p_ctx, 0, sizeof(AVICTX));

	p_ctx->ctxf_write = 1;

	p_ctx->f = fopen(filename, "wb+");
	if (p_ctx->f == NULL)
	{
		log_print(HT_LOG_ERR, "%s, fopen [%s] failed!!!\r\n", __FUNCTION__, filename);
		goto write_err;
	}

	strncpy(p_ctx->filename, filename, sizeof(p_ctx->filename));

	char idx_path[256];
	sprintf(idx_path, "%s.idx", filename);
	
	p_ctx->idx_f = fopen(idx_path, "wb+");
	if (p_ctx->idx_f == NULL)
	{
		log_print(HT_LOG_ERR, "%s, fopen [%s] failed!!!\r\n", __FUNCTION__, idx_path);
		goto write_err;
	}

	p_ctx->mutex = sys_os_create_mutex();

	return p_ctx;

write_err:

	if (p_ctx)
	{
		// An error has been written in the front, and it needs to be judged whether it can be closed.
		if (p_ctx->f)
		{
			fclose(p_ctx->f);
		}
		
		if (p_ctx->idx_f)
		{
			fclose(p_ctx->idx_f);
		}	
	}
	
	if (p_ctx)
	{
		free(p_ctx);
    }
    
	return NULL;
}

int avi_write_video_start(AVICTX * p_ctx, uint32 len, int b_key)
{
	int ret = -1;

    if (NULL == p_ctx)
	{
		return -1;
    }
    
    sys_os_mutex_enter(p_ctx->mutex);
    
	if (NULL == p_ctx || NULL == p_ctx->f)
	{
		return -1;
    }

	int i_pos = ftell(p_ctx->f);

	avi_write_fourcc(p_ctx, "00dc");
	avi_write_uint32(p_ctx, len);

	// Write index, memory mode and temporary file
	if (p_ctx->ctxf_idx_m == 1)
	{
		if (p_ctx->i_idx_max <= p_ctx->i_idx)
		{
			p_ctx->i_idx_max += 1000;
			p_ctx->idx = (int *)realloc(p_ctx->idx, p_ctx->i_idx_max * 16);
			if (p_ctx->idx == NULL)
			{
				log_print(HT_LOG_ERR, "%s, realloc ret null!!!\r\n", __FUNCTION__);
			}
		}

		if (p_ctx->idx)
		{
			memcpy(&p_ctx->idx[4*p_ctx->i_idx+0], "00dc", 4);
			avi_set_dw(&p_ctx->idx[4*p_ctx->i_idx+1], b_key ? AVIIF_KEYFRAME : 0);
			avi_set_dw(&p_ctx->idx[4*p_ctx->i_idx+2], i_pos);
			avi_set_dw(&p_ctx->idx[4*p_ctx->i_idx+3], len);
		
			p_ctx->i_idx++;
		}
	}
	else if (p_ctx->idx_f)
	{
		memcpy(&p_ctx->idx_fix[p_ctx->idx_fix_off + 0], "00dc", 4);
		avi_set_dw(&p_ctx->idx_fix[p_ctx->idx_fix_off + 1], b_key ? AVIIF_KEYFRAME : 0);
		avi_set_dw(&p_ctx->idx_fix[p_ctx->idx_fix_off + 2], i_pos);
		avi_set_dw(&p_ctx->idx_fix[p_ctx->idx_fix_off + 3], len);

		p_ctx->idx_fix_off += 4;
		
		if (p_ctx->idx_fix_off == (sizeof(p_ctx->idx_fix) / sizeof(int)))
		{
			if (fwrite(p_ctx->idx_fix, sizeof(p_ctx->idx_fix), 1, p_ctx->idx_f) != 1)
			{
				goto w_err;
			}
			
			fflush(p_ctx->idx_f);

			p_ctx->idx_fix_off = 0;
		}

		p_ctx->i_idx++;
	}

	p_ctx->i_frame_video++;
	
	return 0;

w_err:

	log_print(HT_LOG_ERR, "%s, ret[%d] err[%d] [%s]!!!\r\n", __FUNCTION__, ret, errno, strerror(errno));

	if (p_ctx->f)
	{
		fclose(p_ctx->f);
		p_ctx->f = NULL;
	}
	
	if (p_ctx->idx_f)
	{
		fclose(p_ctx->idx_f);
		p_ctx->idx_f = NULL;
	}

	sys_os_mutex_leave(p_ctx->mutex);

	return -1;
}

int avi_write_video_data(AVICTX * p_ctx, void * p_data, uint32 len)
{
	int ret = fwrite(p_data, len, 1, p_ctx->f);
	if (ret != 1)
	{
		goto w_err;
    }
    
	fflush(p_ctx->f);

	return len;

w_err:

	log_print(HT_LOG_ERR, "%s, ret[%d]err[%d][%s]!!!\r\n", __FUNCTION__, ret, errno, strerror(errno));

	if (p_ctx->f)
	{
		fclose(p_ctx->f);
		p_ctx->f = NULL;
	}
	
	if (p_ctx->idx_f)
	{
		fclose(p_ctx->idx_f);
		p_ctx->idx_f = NULL;
	}

	sys_os_mutex_leave(p_ctx->mutex);

	return -1;
}

int avi_write_video_end(AVICTX * p_ctx, int wlen)
{
	if (p_ctx->f == NULL)
	{
		return -1;
    }
    
	if (wlen & 0x01)	/* pad */
	{
		fputc(0, p_ctx->f);
    }
    
	int ret = ftell(p_ctx->f);

	sys_os_mutex_leave(p_ctx->mutex);
	
	return ret;
}

int avi_write_video(AVICTX * p_ctx, void * p_data, uint32 len, int b_key)
{
	int ret = -1;

    if (NULL == p_ctx)
    {
        return -1;
    }
    
    sys_os_mutex_enter(p_ctx->mutex);
    
	if (NULL == p_ctx || NULL == p_ctx->f)
	{
		return -1;
    }

	int i_pos = ftell(p_ctx->f);

	avi_write_fourcc(p_ctx, "00dc");
	avi_write_uint32(p_ctx, len);

	if (fwrite(p_data, len, 1, p_ctx->f) != 1)
	{
		goto w_err;
	}
	
	fflush(p_ctx->f);

	if (len & 0x01)	/* pad */
	{
		fputc(0, p_ctx->f);
    }
    
	if (p_ctx->ctxf_idx_m == 1)
	{
		if (p_ctx->i_idx_max <= p_ctx->i_idx)
		{
			p_ctx->i_idx_max += 1000;
			p_ctx->idx = (int *)realloc(p_ctx->idx, p_ctx->i_idx_max * 16);
			if (p_ctx->idx == NULL)
			{
				log_print(HT_LOG_ERR, "%s, realloc ret null!!!\r\n", __FUNCTION__);
			}
		}

		if (p_ctx->idx)
		{
			memcpy(&p_ctx->idx[4*p_ctx->i_idx+0], "00dc", 4);
			avi_set_dw(&p_ctx->idx[4*p_ctx->i_idx+1], b_key ? AVIIF_KEYFRAME : 0);
			avi_set_dw(&p_ctx->idx[4*p_ctx->i_idx+2], i_pos);
			avi_set_dw(&p_ctx->idx[4*p_ctx->i_idx+3], len);
		
			p_ctx->i_idx++;
		}
	}
	else if (p_ctx->idx_f)
	{
		memcpy(&p_ctx->idx_fix[p_ctx->idx_fix_off + 0], "00dc", 4);
		avi_set_dw(&p_ctx->idx_fix[p_ctx->idx_fix_off + 1], b_key ? AVIIF_KEYFRAME : 0);
		avi_set_dw(&p_ctx->idx_fix[p_ctx->idx_fix_off + 2], i_pos);
		avi_set_dw(&p_ctx->idx_fix[p_ctx->idx_fix_off + 3], len);

		p_ctx->idx_fix_off += 4;
		
		if (p_ctx->idx_fix_off == (sizeof(p_ctx->idx_fix) / sizeof(int)))
		{
			if (fwrite(p_ctx->idx_fix, sizeof(p_ctx->idx_fix), 1, p_ctx->idx_f) != 1)
			{
				goto w_err;
            }
            
			fflush(p_ctx->idx_f);

			p_ctx->idx_fix_off = 0;
		}

		p_ctx->i_idx++;
	}

	p_ctx->i_frame_video++;

	ret = ftell(p_ctx->f);

	if (p_ctx->s_time == 0)
	{
		p_ctx->s_time = sys_os_get_ms();
		p_ctx->e_time = p_ctx->s_time;
	}
	else
	{
		p_ctx->e_time = sys_os_get_ms();
	}

w_err:

	if (ret < 0)
	{
		log_print(HT_LOG_ERR, "%s, ret[%d] err[%d] [%s]!!!\r\n", __FUNCTION__, ret, errno, strerror(errno));

		if (p_ctx->f)
		{
			fclose(p_ctx->f);
			p_ctx->f = NULL;
		}
		
		if (p_ctx->idx_f)
		{
			fclose(p_ctx->idx_f);
			p_ctx->idx_f = NULL;
		}
	}

	sys_os_mutex_leave(p_ctx->mutex);
	
	return ret;
}

int avi_write_audio(AVICTX * p_ctx, void * p_data, uint32 len)
{
	int ret = -1;

    if (NULL == p_ctx)
    {
        return -1;
    }
    
    sys_os_mutex_enter(p_ctx->mutex);
    
	if (NULL == p_ctx || NULL == p_ctx->f)
	{
		return -1;
    }

	int i_pos = ftell(p_ctx->f);

	/* chunk header */
	avi_write_fourcc(p_ctx, "01wb");
	avi_write_uint32(p_ctx, len);

	if (fwrite(p_data, len, 1, p_ctx->f) != 1)
	{
		goto w_err;
    }
    
	if (len & 0x01)	/* pad */
	{
		fputc(0, p_ctx->f);
    }
    
	if (p_ctx->ctxf_idx_m == 1)
	{
		if (p_ctx->i_idx_max <= p_ctx->i_idx)
		{
			p_ctx->i_idx_max += 1000;
			p_ctx->idx = (int *)realloc(p_ctx->idx, p_ctx->i_idx_max * 16);
			if (p_ctx->idx == NULL)
			{
				log_print(HT_LOG_ERR, "%s, realloc ret null!!!\r\n", __FUNCTION__);
			}
		}

		if (p_ctx->idx)
		{
			memcpy(&p_ctx->idx[4*p_ctx->i_idx+0], "01wb", 4);
			avi_set_dw(&p_ctx->idx[4*p_ctx->i_idx+1], AVIIF_KEYFRAME);
			avi_set_dw(&p_ctx->idx[4*p_ctx->i_idx+2], i_pos);
			avi_set_dw(&p_ctx->idx[4*p_ctx->i_idx+3], len);
		
			p_ctx->i_idx++;
		}
	}
	else if (p_ctx->idx_f)
	{
		memcpy(&p_ctx->idx_fix[p_ctx->idx_fix_off + 0], "01wb", 4);
		avi_set_dw(&p_ctx->idx_fix[p_ctx->idx_fix_off + 1], AVIIF_KEYFRAME);
		avi_set_dw(&p_ctx->idx_fix[p_ctx->idx_fix_off + 2], i_pos);
		avi_set_dw(&p_ctx->idx_fix[p_ctx->idx_fix_off + 3], len);

		p_ctx->idx_fix_off += 4;
		
		if (p_ctx->idx_fix_off == (sizeof(p_ctx->idx_fix) / sizeof(int)))
		{
			if (fwrite(p_ctx->idx_fix, sizeof(p_ctx->idx_fix), 1, p_ctx->idx_f) != 1)
			{
				goto w_err;
			}
			
			p_ctx->idx_fix_off = 0;
		}

		p_ctx->i_idx++;
	}

	p_ctx->i_frame_audio++;
	ret = ftell(p_ctx->f);

w_err:

	if (ret < 0)
	{
		log_print(HT_LOG_ERR, "%s, ret[%d]!!!\r\n", __FUNCTION__, ret);

		if (p_ctx->f)
		{
			fclose(p_ctx->f);
			p_ctx->f = NULL;
		}
		
		if (p_ctx->idx_f)
		{
			fclose(p_ctx->idx_f);
			p_ctx->idx_f = NULL;
		}
	}

	sys_os_mutex_leave(p_ctx->mutex);
	
	return ret;
}

void avi_write_close(AVICTX * p_ctx)
{
	if (NULL == p_ctx)
	{
		return;
    }
    
	sys_os_mutex_enter(p_ctx->mutex);

	if (NULL == p_ctx)
	{
		return;
    }

	avi_end(p_ctx);
	avi_free_idx(p_ctx);

    sys_os_mutex_leave(p_ctx->mutex);
    
	sys_os_destroy_sig_mutex(p_ctx->mutex);

	free(p_ctx);
}

void avi_set_video_info(AVICTX * p_ctx, int fps, int width, int height, const char fcc[4])
{
	memcpy(p_ctx->v_fcc, fcc, 4); // "H264","H265","JPEG","MP4V"
	p_ctx->v_fps = fps;
	p_ctx->v_width  = width;
	p_ctx->v_height = height;

	p_ctx->ctxf_video = 1;
}

void avi_set_audio_info(AVICTX * p_ctx, int chns, int rate, uint16 fmt, uint8 * extra, int extra_len)
{
	p_ctx->a_chns = chns;
	p_ctx->a_rate = rate;
	p_ctx->a_fmt = fmt;
	p_ctx->a_extra = extra;
	p_ctx->a_extra_len = extra_len;

	p_ctx->ctxf_audio = 1;
}

void avi_build_video_hdr(AVICTX * p_ctx)
{
	if (p_ctx->ctxf_video == 0)
	{
		return;
    }
    
	if (p_ctx->s_time < p_ctx->e_time && p_ctx->i_frame_video > 1)
	{
		// Final correction to the actual frame rate
		float fps = (float) (p_ctx->i_frame_video * 1000.0) / (p_ctx->e_time - p_ctx->s_time);
		p_ctx->v_fps = (uint32)(fps + 0.5);

		log_print(HT_LOG_DBG, "%s, stime=%u, etime=%u, frames=%d, fps=%d\r\n", 
		    __FUNCTION__, p_ctx->s_time, p_ctx->e_time, p_ctx->i_frame_video, p_ctx->v_fps);
	}

    memcpy(&p_ctx->str_v.fccHandler, p_ctx->v_fcc, 4);
    
	p_ctx->str_v.fccType			= mmioFOURCC('v','i','d','s');	
	p_ctx->str_v.dwFlags			= 0;	// Contains AVITF_ flags 
	p_ctx->str_v.wPriority			= 0;
	p_ctx->str_v.wLanguage			= 0;
	p_ctx->str_v.dwInitialFrames	= 0;
	p_ctx->str_v.dwScale			= 1;	
	p_ctx->str_v.dwRate				= (p_ctx->v_fps == 0)? 25 : p_ctx->v_fps;	// dwRate / dwScale == samplessecond 
	p_ctx->str_v.dwStart			= 0;
	p_ctx->str_v.dwLength			= p_ctx->i_frame_video; // In units above..
	p_ctx->str_v.dwSuggestedBufferSize = 1024*1024;
	p_ctx->str_v.dwQuality			= -1;
	p_ctx->str_v.dwSampleSize		= 0;
	p_ctx->str_v.rcFrame.left		= 0;
	p_ctx->str_v.rcFrame.top		= 0;
	p_ctx->str_v.rcFrame.right		= p_ctx->v_width;
	p_ctx->str_v.rcFrame.bottom		= p_ctx->v_height;

    memcpy(&p_ctx->bmp.biCompression, p_ctx->v_fcc, 4);
    
	p_ctx->bmp.biSize			= sizeof(BMPHDR);
	p_ctx->bmp.biWidth			= p_ctx->v_width;
	p_ctx->bmp.biHeight			= p_ctx->v_height;
	p_ctx->bmp.biPlanes			= 1;
	p_ctx->bmp.biBitCount		= 24;	
	p_ctx->bmp.biSizeImage		= p_ctx->v_width * p_ctx->v_height * 3;
	p_ctx->bmp.biXPelsPerMeter	= 0;
	p_ctx->bmp.biYPelsPerMeter	= 0;
	p_ctx->bmp.biClrUsed		= 0;
	p_ctx->bmp.biClrImportant	= 0;
}

void avi_build_audio_hdr(AVICTX * p_ctx)
{
	if (p_ctx->ctxf_audio == 0)
	{
		return;
    }
    
	p_ctx->str_a.fccType			= mmioFOURCC('a','u','d','s');
	p_ctx->str_a.fccHandler			= 1;
	p_ctx->str_a.dwFlags			= 0;	// Contains AVITF_ flags 
	p_ctx->str_a.wPriority			= 0;
	p_ctx->str_a.wLanguage			= 0;
	p_ctx->str_a.dwInitialFrames	= 0;
	p_ctx->str_a.dwScale			= 1;	
	p_ctx->str_a.dwRate				= p_ctx->a_rate;	// 8000
	p_ctx->str_a.dwStart			= 0;
	p_ctx->str_a.dwLength			= p_ctx->i_frame_audio;
	p_ctx->str_a.dwSuggestedBufferSize = 80*1024 ;
	p_ctx->str_a.dwQuality			= -1;
	p_ctx->str_a.dwSampleSize		= p_ctx->a_chns;
	p_ctx->str_a.rcFrame.left		= 0;
	p_ctx->str_a.rcFrame.top		= 0;
	p_ctx->str_a.rcFrame.right		= 0;
	p_ctx->str_a.rcFrame.bottom		= 0;

    int bps, blkalign, bytespersec;

    if (p_ctx->a_fmt == AUDIO_FORMAT_ALAW)      // G711A
    {
        bps = 8;
        blkalign = p_ctx->a_chns;
        bytespersec = p_ctx->a_rate * p_ctx->a_chns;
    }
    else if (p_ctx->a_fmt == AUDIO_FORMAT_MULAW) // G711U
    {
        bps = 8;
        blkalign = p_ctx->a_chns;
        bytespersec = p_ctx->a_rate * p_ctx->a_chns;
    }
    else if (p_ctx->a_fmt == AUDIO_FORMAT_AAC) // AAC
    {
        bps = 16;
        blkalign = 768 * p_ctx->a_chns; /* maximum bytes per frame */
        bytespersec = p_ctx->a_rate * p_ctx->a_chns;
    }
    else if (p_ctx->a_fmt == AUDIO_FORMAT_G726) // G726
    {
        bps = 8;
        blkalign = p_ctx->a_chns;
        bytespersec = p_ctx->a_rate * p_ctx->a_chns;
    }
    else if (p_ctx->a_fmt == AUDIO_FORMAT_G722) // G722
    {
        bps = 4;
        blkalign = p_ctx->a_chns;
        bytespersec = p_ctx->a_rate * p_ctx->a_chns;
    }
    else
    {
        bps = 8;
        blkalign = p_ctx->a_chns;
        bytespersec = p_ctx->a_rate * p_ctx->a_chns;
    }

    p_ctx->str_a.dwSampleSize		= blkalign;
    
	p_ctx->wave.wFormatTag			= p_ctx->a_fmt;
	p_ctx->wave.nChannels			= p_ctx->a_chns;	
	p_ctx->wave.nSamplesPerSec		= p_ctx->a_rate;
	p_ctx->wave.nAvgBytesPerSec		= bytespersec;
	p_ctx->wave.nBlockAlign			= blkalign;
	p_ctx->wave.wBitsPerSample		= bps;
	p_ctx->wave.cbSize				= p_ctx->a_extra_len;
}

int avi_write_header(AVICTX * p_ctx)
{
	if (p_ctx->f == NULL)
	{
		return -1;
    }

    log_print(HT_LOG_DBG, "%s, enter...\r\n", __FUNCTION__);
    
	avi_build_video_hdr(p_ctx);
	avi_build_audio_hdr(p_ctx);

	fseek(p_ctx->f, 0, SEEK_SET);

	int avih_len = sizeof(AVIMHDR) + 8;

	int strl_v_len = sizeof(AVISHDR) + 8 + sizeof(BMPHDR) + 8;
	int strl_a_len = sizeof(AVISHDR) + 8 + sizeof(WAVEFMT) + 8;
	int s_v_ll = strl_v_len + 12;
	int s_a_ll = strl_a_len + 12;

	int extra_len = 0;
	int pad_len = 0;

	if (p_ctx->a_extra && p_ctx->a_extra_len)
	{
	    extra_len = p_ctx->a_extra_len;

	    if (p_ctx->a_extra_len & 0x01)
	    {
	        pad_len = 1;
	    }
	}

	avi_write_fourcc(p_ctx, "RIFF");
	avi_write_uint32(p_ctx, p_ctx->i_riff > 0 ? p_ctx->i_riff - 8 : 0xFFFFFFFF); // Total file length - ('RIFF') - 4 
	avi_write_fourcc(p_ctx, "AVI ");

	avi_write_fourcc(p_ctx, "LIST");
	
	if (p_ctx->ctxf_audio == 1)
	{
		avi_write_uint32(p_ctx,  4 + avih_len + s_v_ll + s_a_ll + extra_len + pad_len);	// List data length + 4("hdrl")
	}	
	else
	{
		avi_write_uint32(p_ctx,  4 + avih_len + s_v_ll);	// List data length + 4("hdrl")
    }
    
	avi_write_fourcc(p_ctx, "hdrl");

	avi_write_fourcc(p_ctx, "avih");
	avi_write_uint32(p_ctx, sizeof(AVIMHDR));  //4*16 -8

	if (p_ctx->v_fps == 0)
	{
		p_ctx->avi_hdr.dwMicroSecPerFrame	= 1000000 / 25;	        // Video frame interval (in microseconds)
	}	
	else
	{
		p_ctx->avi_hdr.dwMicroSecPerFrame	= 1000000 / p_ctx->v_fps;	// Video frame interval (in microseconds)
    }
    
	p_ctx->avi_hdr.dwMaxBytesPerSec		= 0xffffffff;				// The maximum data rate of this AVI file
	p_ctx->avi_hdr.dwPaddingGranularity	= 0;						// Granularity of data padding
	p_ctx->avi_hdr.dwFlags				= AVIF_HASINDEX|AVIF_ISINTERLEAVED|AVIF_TRUSTCKTYPE;
	p_ctx->avi_hdr.dwTotalFrames		= p_ctx->i_frame_video;		// Total number of frames
	p_ctx->avi_hdr.dwInitialFrames		= 0;						// Specify the initial number of frames for the interactive format

	if (p_ctx->ctxf_audio == 1)
	{
		p_ctx->avi_hdr.dwStreams		= 2;						// The number of streams included in this file
	}	
	else
	{
		p_ctx->avi_hdr.dwStreams		= 1;						// The number of streams included in this file
    }
    
	p_ctx->avi_hdr.dwSuggestedBufferSize= 1000000;					// It is recommended to read the cache size of this file (should be able to accommodate the largest block)
	p_ctx->avi_hdr.dwWidth				= p_ctx->v_width;			// The width of the video in pixels
	p_ctx->avi_hdr.dwHeight				= p_ctx->v_height;			// The height of the video in pixels

	avi_write_buffer(p_ctx, (char*)&p_ctx->avi_hdr, sizeof(AVIMHDR));

	avi_write_fourcc(p_ctx, "LIST");
	avi_write_uint32(p_ctx,  4 + sizeof(AVISHDR) + 8 + sizeof(BMPHDR) + 8);
	avi_write_fourcc(p_ctx, "strl");						        // How many streams are there in the file, and how many 'strl' sublists there are

	avi_write_fourcc(p_ctx, "strh");
	avi_write_uint32(p_ctx,  sizeof(AVISHDR));
	avi_write_buffer(p_ctx, (char*)&p_ctx->str_v, sizeof(AVISHDR));

	avi_write_fourcc(p_ctx, "strf");
	avi_write_uint32(p_ctx,  sizeof(BMPHDR));
	avi_write_buffer(p_ctx, (char*)&p_ctx->bmp, sizeof(BMPHDR));

	if (p_ctx->ctxf_audio == 1)
	{
		avi_write_fourcc(p_ctx, "LIST");
		avi_write_uint32(p_ctx,  4 + sizeof(AVISHDR) + 8 + sizeof(WAVEFMT) + 8 + extra_len + pad_len);
		avi_write_fourcc(p_ctx, "strl");

		avi_write_fourcc(p_ctx, "strh");
		avi_write_uint32(p_ctx,  sizeof(AVISHDR));
		avi_write_buffer(p_ctx, (char*)&p_ctx->str_a, sizeof(AVISHDR));

		avi_write_fourcc(p_ctx, "strf");
		avi_write_uint32(p_ctx,  sizeof(WAVEFMT) + extra_len);
		avi_write_buffer(p_ctx, (char*)&p_ctx->wave, sizeof(WAVEFMT));

		if (p_ctx->a_extra && p_ctx->a_extra_len)
		{
		    avi_write_buffer(p_ctx, (char*)p_ctx->a_extra, p_ctx->a_extra_len);

		    if (p_ctx->a_extra_len & 0x01)	/* pad */
        	{
        		fputc(0, p_ctx->f);
            }
		}
	}

	avi_write_fourcc(p_ctx, "LIST");
	avi_write_uint32(p_ctx,  p_ctx->i_movi_end > 0 ? (p_ctx->i_movi_end - p_ctx->i_movi + 4) : 0xFFFFFFFF);
	avi_write_fourcc(p_ctx, "movi");

	fflush(p_ctx->f);

	p_ctx->i_movi = ftell(p_ctx->f);
	
	if (p_ctx->i_movi < 0)
	{
		goto w_err;
    }
    
	return 0;

w_err:

	if (p_ctx->f)
	{
		fclose(p_ctx->f);
		p_ctx->f = NULL;
	}

	return -1;
}

int avi_update_header(AVICTX * p_ctx)
{
    log_print(HT_LOG_DBG, "%s, enter...\r\n", __FUNCTION__);

    if (NULL == p_ctx)
    {
        return -1;
    }
    
    sys_os_mutex_enter(p_ctx->mutex);

    if (NULL == p_ctx || NULL == p_ctx->f)
	{
		return -1;
    }
    
	if (avi_write_header(p_ctx) == 0)
	{
		if (p_ctx->f)
		{
			fseek(p_ctx->f, 0, SEEK_END);

			sys_os_mutex_leave(p_ctx->mutex);
			return 0;
		}
	}

    sys_os_mutex_leave(p_ctx->mutex);
	return -1;
}

int avi_calc_fps(AVICTX * p_ctx, uint8 * p_data, uint32 len, uint32 ts)
{
	int i;
	
	if (p_ctx->prev_ts == 0)
	{
	    p_ctx->prev_ts = ts;
		return 0;
	}
	
	if (p_ctx->prev_ts == ts)
	{
		return 0;
    }

    int count = ARRAY_SIZE(p_ctx->delta_ts);

	uint32 delta_ts = ts - p_ctx->prev_ts;
	
	for (i=0; i<count; i++)
	{
		if (p_ctx->delta_ts[i] == 0)
		{
			p_ctx->delta_ts[i] = delta_ts;
			break;
		}
	}

	int delta_finish = 1;
	
	for (i=0; i<count; i++)
	{
		if (p_ctx->delta_ts[i] == 0)
		{
			delta_finish = 0;
			break;
		}
	}
	
	if (delta_finish == 1)
	{
	    delta_ts = 0;
	    
	    for (i=0; i<count; i++)
	    {
	        delta_ts += p_ctx->delta_ts[i];   
        }
	    
		delta_ts /= count;
		
		float fps = (float) 90000.0 / delta_ts;
		
		p_ctx->v_fps = (uint32)(fps + 0.5);

        log_print(HT_LOG_DBG, "%s, fps=%d\r\n", __FUNCTION__, (int)fps);

        // If fps is equal to 0, recalculate fps
        if (p_ctx->v_fps == 0)
        {
            p_ctx->prev_ts = 0;
            memset(p_ctx->delta_ts, 0, sizeof(uint32)*count);
        }
        
		return (int)fps;
	}

	return 0;
}

int avi_parse_video_size(AVICTX * p_ctx, uint8 * p_data, uint32 len)
{
	uint8 nalu_t;

	if (p_ctx == NULL || p_data == NULL || len < 5)
	{
		return -1;
    }
    
	if (p_ctx->v_width && p_ctx->v_height)
	{
		return 0;
    }
    
	// Need to parse width X height

	if (memcmp(p_ctx->v_fcc, "H264", 4) == 0)
	{        
		int s_len = 0, n_len = 0, parse_len = len;
		uint8 * p_cur = p_data;
        
		while (p_cur)
		{
			uint8 * p_next = avc_split_nalu(p_cur, parse_len, &s_len, &n_len);
			if (n_len < 5)
			{
				return 0;
            }
            
			nalu_t = (p_cur[s_len] & 0x1F);
			
			int b_start;
			nal_t nal;
			
			nal.i_payload = n_len-s_len-1;
			nal.p_payload = p_cur+s_len+1;
			nal.i_type = nalu_t;

			if (nalu_t == H264_NAL_SPS && p_ctx->ctxf_sps_f == 0)	
			{
				h264_t parse;
				h264_parser_init(&parse);

				h264_parser_parse(&parse, &nal, &b_start);
				log_print(HT_LOG_INFO, "%s, H264 width[%d],height[%d]\r\n", __FUNCTION__, parse.i_width, parse.i_height);
				p_ctx->v_width = parse.i_width;
				p_ctx->v_height = parse.i_height;
				p_ctx->ctxf_sps_f = 1;
				return 0;
			}
			
			parse_len -= n_len;
			p_cur = p_next;
		}
	}
	else if (memcmp(p_ctx->v_fcc, "H265", 4) == 0)
	{
		int s_len, n_len = 0, parse_len = len;
		uint8 * p_cur = p_data;

		while (p_cur)
		{
			uint8 * p_next = avc_split_nalu(p_cur, parse_len, &s_len, &n_len);
			if (n_len < 5)
			{
				return 0;
            }
            
			nalu_t = (p_cur[s_len] >> 1) & 0x3F;
			
			if (nalu_t == HEVC_NAL_SPS && p_ctx->ctxf_sps_f == 0)	
			{
				h265_t parse;
				h265_parser_init(&parse);

				if (h265_parser_parse(&parse, p_cur+4, n_len-s_len) == 0)
				{
					log_print(HT_LOG_INFO, "%s, H265 width[%d],height[%d]\r\n", __FUNCTION__, parse.pic_width_in_luma_samples, parse.pic_height_in_luma_samples);
					p_ctx->v_width = parse.pic_width_in_luma_samples;
					p_ctx->v_height = parse.pic_height_in_luma_samples;
					p_ctx->ctxf_sps_f = 1;
					return 0;
				}
			}
			
			parse_len -= n_len;
			p_cur = p_next;
		}
	}
    else if (memcmp(p_ctx->v_fcc, "JPEG", 4) == 0)
    {
        uint32 offset = 0;
        int size_chunk = 0;
        
        while (offset < len - 8 && p_data[offset] == 0xFF)
        {
            if (p_data[offset+1] == MARKER_SOF0)
            {
                int h = ((p_data[offset+5] << 8) | p_data[offset+6]);
                int w = ((p_data[offset+7] << 8) | p_data[offset+8]);
                log_print(HT_LOG_INFO, "%s, MJPEG width[%d],height[%d]\r\n", __FUNCTION__, w, h);
                p_ctx->v_width = w;
			    p_ctx->v_height = h;
                break;
            }
            else if (p_data[offset+1] == MARKER_SOI)
            {
                offset += 2;
            }
            else
            {
                size_chunk = ((p_data[offset+2] << 8) | p_data[offset+3]);
                offset += 2 + size_chunk;
            }
        }
    }
    else if (memcmp(p_ctx->v_fcc, "MP4V", 4) == 0)
    {
        uint32 pos = 0;
        int vol_f = 0;
        int vol_pos = 0;
        int vol_len = 0;

        while (pos < len - 4)
        {
            if (p_data[pos] == 0 && p_data[pos+1] == 0 && p_data[pos+2] == 1)
            {
                if (p_data[pos+3] >= 0x20 && p_data[pos+3] <= 0x2F)
                {
                    vol_f = 1;
                    vol_pos = pos+4;
                }
                else if (vol_f)
                {
                    vol_len = pos - vol_pos;
                    break;
                }
            }

            pos++;
        }

        if (!vol_f)
        {
            return 0;
        }
        else if (vol_len <= 0)
        {
            vol_len = len - vol_pos;
        }

        int vo_ver_id;
                    
        BitVector bv(&p_data[vol_pos], 0, vol_len*8);

        bv.skipBits(1);     /* random access */
        bv.skipBits(8);     /* vo_type */

        if (bv.get1Bit())   /* is_ol_id */
        {
            vo_ver_id = bv.getBits(4); /* vo_ver_id */
            bv.skipBits(3); /* vo_priority */
        }

        if (bv.getBits(4) == 15) // aspect_ratio_info
        {
            bv.skipBits(8);     // par_width
            bv.skipBits(8);     // par_height
        }

        if (bv.get1Bit()) /* vol control parameter */
        {
            int chroma_format = bv.getBits(2);

            bv.skipBits(1);     /* low_delay */

            if (bv.get1Bit()) 
            {    
                /* vbv parameters */
                bv.getBits(15);   /* first_half_bitrate */
                bv.skipBits(1);
                bv.getBits(15);   /* latter_half_bitrate */
                bv.skipBits(1);
                bv.getBits(15);   /* first_half_vbv_buffer_size */
                bv.skipBits(1);
                bv.getBits(3);    /* latter_half_vbv_buffer_size */
                bv.getBits(11);   /* first_half_vbv_occupancy */
                bv.skipBits(1);
                bv.getBits(15);   /* latter_half_vbv_occupancy */
                bv.skipBits(1);
            }                        
        }

        int shape = bv.getBits(2); /* vol shape */
        
        if (shape == 3 && vo_ver_id != 1) 
        {
            bv.skipBits(4);  /* video_object_layer_shape_extension */
        }

        bv.skipBits(1); 
        
        int framerate = bv.getBits(16);

        int time_increment_bits = (int) (log(framerate - 1.0) * 1.44269504088896340736 + 1); // log2(framerate - 1) + 1
        if (time_increment_bits < 1)
        {
            time_increment_bits = 1;
        }
        
        bv.skipBits(1);
        
        if (bv.get1Bit() != 0)     /* fixed_vop_rate  */
        {
            bv.skipBits(time_increment_bits);    
        }
        
        if (shape != 2)
        {
            if (shape == 0) 
            {
                bv.skipBits(1);
                int w = bv.getBits(13);
                bv.skipBits(1);
                int h = bv.getBits(13);

                log_print(HT_LOG_INFO, "%s, MPEG4 width[%d],height[%d]\r\n", __FUNCTION__, w, h);
                p_ctx->v_width = w;
    		    p_ctx->v_height = h;
            }
        }        
    }
    
	return 0;
}



