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
#include "avi_read.h"


/**************************************************************************/

// Find the video description
/**
 * RIFF('AVI '   --RIFF file header, the data type of the block is AVI
 *      LIST('hdrl'  --hdrl list
 *            avih(<MainAVIHeader>)  --The avi sub-block starts, the length of the line is 64 bytes.
 *            LIST('strl'  --Strl list, is a list of streams
 *                  strh(<Stream header>)  --Stream header, 64 bytes in length
 *                  strf(<Stream format>)  --Stream format information sub-block describing the data in the stream
 *                  .. additional header data  --Strd optional extra header information||strn optional stream name
 *                 )
 *            ...
 *           )
 *      LIST('movi'   --Movi list block, which contains the actual data of the stream, can be sub-blocks, or can be organized into rec lists
 *             { LIST('rec'  --The data in a rec list should be read from disk at one time.
 *                      SubChunk...
 *                       )
 *            | SubChunk } ....    
 *          )
 *      [ <AVIIndex> ]  --Index block
 *     )
**/

// find 'hdrl' list, ret offset, llen = len of list
int avi_find_hdr_list(AVICTX * p_ctx, int * llen)
{
	AVIRIFF riff;
	uint32 offset = 12, rlen;
	uint32 tlen = p_ctx->flen - 12;

	while (offset < tlen)
	{
		fseek(p_ctx->f, offset, SEEK_SET);
		rlen = fread(&riff, sizeof(riff), 1, p_ctx->f);
		
		if (rlen != 1)
		{
		    log_print(HT_LOG_ERR, "%s, fread failed!!!\r\n", __FUNCTION__);
			return -1;
        }
        
		if (riff.len > tlen)
		{
		    log_print(HT_LOG_ERR, "%s, riff.len=%d, tlen=%d\r\n", __FUNCTION__, riff.len, tlen);
			return -1;
        }
        
		if (riff.riff == mmioFOURCC('L','I','S','T'))
		{
			if (riff.type == mmioFOURCC('h','d','r','l'))	// header list
			{
				*llen = riff.len - 4;
				return (offset + 12);
			}

			if ((riff.len & 1) == 1)
			{
			    riff.len++;
			}
			
			offset += 8 + riff.len;
		}
	}

	return -1;
}

int avi_parse_stream_list(AVICTX * p_ctx, int offset, int llen)
{
	AVIRIFF riff;
	uint32 rlen, prev_mmio = 0;
	uint32 tlen = offset+llen;

	int v_sh_f = 0, v_sf_f = 0;	// video stream header and format read flag
	int a_sh_f = 0, a_sf_f = 0;	// audio stream header and format read flag

	while (offset < (int) tlen)
	{
		fseek(p_ctx->f, offset, SEEK_SET);
		rlen = fread(&riff, sizeof(riff), 1, p_ctx->f);
		
		if (rlen != 1)
		{
		    log_print(HT_LOG_ERR, "%s, rlen=%d\r\n", __FUNCTION__, rlen);
			return -1;
        }
        
		if (riff.len > tlen)
		{
		    log_print(HT_LOG_ERR, "%s, riff.len=%d, tlen=%d\r\n", __FUNCTION__, riff.len, tlen);
			return -1;
        }
        
		if (riff.riff == mmioFOURCC('s','t','r','h'))
		{
			if (riff.len != sizeof(AVISHDR))
			{
				log_print(HT_LOG_ERR, "%s, riff.len=%d, sizeof(AVISHDR)=%d\r\n", __FUNCTION__, riff.len, sizeof(AVISHDR));
				return -1;
			}

			offset += 8;
			fseek(p_ctx->f, offset, SEEK_SET);

			if (riff.type == mmioFOURCC('v','i','d','s'))		//  Video stream header
			{
				if (fread(&(p_ctx->str_v), sizeof(AVISHDR), 1, p_ctx->f) != 1)
				{
					log_print(HT_LOG_ERR, "%s, fread AVISHDR failed\r\n", __FUNCTION__);
					return -1;
				}
				
				v_sh_f = 1;
			}
			else if (riff.type == mmioFOURCC('a','u','d','s'))	//  Audio stream header
			{
				if (fread(&(p_ctx->str_a), sizeof(AVISHDR), 1, p_ctx->f) != 1)
				{
					log_print(HT_LOG_ERR, "%s, fread AVISHDR failed\r\n", __FUNCTION__);
					return -1;
				}
				
				a_sh_f = 1;
			}
			
			prev_mmio = riff.type;
		}
		else if (riff.riff == mmioFOURCC('s','t','r','f'))
		{
			offset += 8;
			fseek(p_ctx->f, offset, SEEK_SET);

			if (prev_mmio == mmioFOURCC('v','i','d','s'))		//  Video stream format
			{
				if (riff.len != sizeof(BMPHDR))
				{
					log_print(HT_LOG_ERR, "%s, riff.len=%d, sizeof(BMPHDR)=%d\r\n", __FUNCTION__, riff.len, sizeof(BMPHDR));
					return -1;
				}

				if (fread(&(p_ctx->bmp), sizeof(BMPHDR), 1, p_ctx->f) != 1)
				{
					log_print(HT_LOG_ERR, "%s, fread BMPHDR failed\r\n", __FUNCTION__);
					return -1;
				}
				
				v_sf_f = 1;	
			}
			else if (prev_mmio == mmioFOURCC('a','u','d','s'))	//  Audio stream format
			{
				if (riff.len != sizeof(WAVEFMT))
				{
					log_print(HT_LOG_ERR, "%s, riff.len=%d, sizeof(WAVEFMT)=%d\r\n", __FUNCTION__, riff.len, sizeof(WAVEFMT));
					return -1;
				}

				if (fread(&(p_ctx->wave), sizeof(WAVEFMT), 1, p_ctx->f) != 1)
				{
					log_print(HT_LOG_ERR, "%s, fread WAVEFMT failed\r\n", __FUNCTION__);
					return -1;
				}
				
				a_sf_f = 1;	
			}
		}
		else
		{
			offset += 8;
		}
		
		if ((riff.len & 1) == 1)
		{
		    riff.len++;
		}
		
		offset += riff.len;
	}

	if (v_sh_f && v_sf_f)
	{
		p_ctx->ctxf_video = 1;
    }
    
	if (a_sh_f && a_sf_f)
	{
		p_ctx->ctxf_audio = 1;
    }
    
	if (p_ctx->ctxf_video == 1 || p_ctx->ctxf_audio == 1)
	{
		return 0;
    }
    
	return -1;
}

int avi_parse_header(AVICTX * p_ctx)
{
	AVIRIFF riff;
	uint32 rlen;

	int avih_f = 0;				// avi header read flag

	int llen = 0;
	int offset = avi_find_hdr_list(p_ctx, &llen);
	
	if (offset <= 0 || llen <= 0)
	{
		return -1;
    }
    
	int tlen = offset+llen;

	while (offset < tlen)
	{
		fseek(p_ctx->f, offset, SEEK_SET);
		rlen = fread(&riff, sizeof(riff), 1, p_ctx->f);
		
		if (rlen != 1)
		{
		    log_print(HT_LOG_ERR, "%s, fread failed!!!\r\n", __FUNCTION__);
			return -1;
        }
        
		if (riff.len > (uint32) tlen)
		{
		    log_print(HT_LOG_ERR, "%s, riff.len=%d, tlen=%d\r\n", __FUNCTION__, riff.len, tlen);
			return -1;
        }
        
		if (riff.riff == mmioFOURCC('a','v','i','h'))
		{
			if (riff.len != sizeof(AVIMHDR))
			{
				log_print(HT_LOG_ERR, "%s, riff.len=%d, sizeof(AVIMHDR)=%d\r\n", __FUNCTION__, riff.len, sizeof(AVIMHDR));
				return -1;
			}
			
			offset += 8;
			fseek(p_ctx->f, offset, SEEK_SET);
			
			if (fread(&(p_ctx->avi_hdr), sizeof(AVIMHDR), 1, p_ctx->f) != 1)
			{
				log_print(HT_LOG_ERR, "%s, fread failed!!!\r\n", __FUNCTION__);
				return -1;
			}

			avih_f = 1;
		}
		else if (riff.riff == mmioFOURCC('L','I','S','T'))
		{
			offset += 8;
			
			if (riff.type == mmioFOURCC('s','t','r','l'))	// Analyze current stream description strh strf ....
			{
				avi_parse_stream_list(p_ctx, offset+4, riff.len-4);
			}
		}
		else
		{
			offset += 8;
		}
		
		if ((riff.len & 1) == 1)
		{
		    riff.len++;
		}
		
		offset += riff.len;
	}

	if (avih_f == 0)
	{
	    log_print(HT_LOG_ERR, "%s, avih_f=0\r\n", __FUNCTION__);
		return -1;
    }
    
	if (p_ctx->ctxf_video == 1 || p_ctx->ctxf_audio == 1)
	{
		return 0;
    }
    
	return -1;
}

int avi_load_movi_list(AVICTX * p_ctx)
{
	AVIRIFF riff;
	uint32 offset = 12, rlen;
	uint32 tlen = p_ctx->flen - 12;

	while (offset < tlen)
	{
		fseek(p_ctx->f, offset, SEEK_SET);
		rlen = fread(&riff, sizeof(riff), 1, p_ctx->f);
		
		if (rlen != 1)
		{
		    log_print(HT_LOG_ERR, "%s, rlen = %d\r\n", __FUNCTION__, rlen);
			return -1;
        }
        
		if (riff.riff == mmioFOURCC('L','I','S','T'))
		{
			if (riff.type == mmioFOURCC('m','o','v','i')) // movi list
			{
				p_ctx->i_movi = offset + 12;
				
				if (riff.len > tlen)	// File not completed
				{
					p_ctx->i_movi_end = tlen;
				}	
				else
				{
					p_ctx->i_movi_end = offset + riff.len + 8;
				}
				
				return offset;
			}
			else
			{
				if (riff.len > tlen)	// File not completed
				{
				    log_print(HT_LOG_ERR, "%s, riff.len = %d, tlen = %d\r\n", __FUNCTION__, riff.len, tlen);
					return -1;
                }
                
				if ((riff.len & 1) == 1)
				{
				    riff.len++;
				}
				
				offset += 8 + riff.len;
			}
		}
	}

	return -1;
}

int avi_load_idx(AVICTX * p_ctx)
{
	AVIRIFF riff;
	uint32 offset = 12, rlen;
	uint32 tlen = p_ctx->flen - 12;
	int idx_len = 0, idx_offset = 0;

	while (offset < tlen)
	{
		fseek(p_ctx->f, offset, SEEK_SET);
		rlen = fread(&riff, sizeof(riff), 1, p_ctx->f);
		
		if (rlen != 1)
		{
		    log_print(HT_LOG_ERR, "%s, rlen = %d\r\n", __FUNCTION__, rlen);
			return -1;
        }
        
		if (riff.len > tlen)
		{
		    log_print(HT_LOG_ERR, "%s, riff.len=%d, tlen=%d\r\n", __FUNCTION__, riff.len, tlen);
			return -1;
        }
        
		if (riff.riff == mmioFOURCC('L','I','S','T'))
		{
			if (riff.type == mmioFOURCC('m','o','v','i'))	// movi list
			{
				idx_offset = offset + 8 + riff.len;
			}

			if ((riff.len & 1) == 1)
			{
			    riff.len++;
			}
			
			offset += 8 + riff.len;
		}
		else if (riff.riff == mmioFOURCC('i','d','x','1'))
		{
			idx_offset = offset + 8;
			idx_len = riff.len;
			
			if ((idx_len + idx_offset) == p_ctx->flen)	// Fully correct length
			{
				p_ctx->ctxf_idx = 1;
				break;
			}
			else if ((idx_len + idx_offset) > (int) p_ctx->flen) // The index part is not finished, and the exception is closed.
			{
				idx_len = p_ctx->flen - idx_offset;
			}
		}
	}

	if (idx_len == 0)
	{
		// The file does not end last: the file being recorded, the file that was abnormally powered off
		char idx_path[128];
		sprintf(idx_path, "%s.idx", p_ctx->filename);
		p_ctx->idx_f = fopen(idx_path, "rb");
		if (p_ctx->idx_f == NULL)
		{
		    log_print(HT_LOG_ERR, "%s, p_ctx->idx_f == NULL\r\n", __FUNCTION__);
			return -1;
        }
        
		idx_len = ftell(p_ctx->f);
		if (idx_len > 0)
		{
			p_ctx->idx = (int *)malloc(idx_len);
			if (p_ctx->idx == NULL)
			{
				fclose(p_ctx->idx_f);
				p_ctx->idx_f = NULL;

				log_print(HT_LOG_ERR, "%s, p_ctx->idx == NULL\r\n", __FUNCTION__);
				
				return -1;
			}

			fseek(p_ctx->f, idx_offset, SEEK_SET);
			rlen = fread(p_ctx->idx, idx_len, 1, p_ctx->f);

			fclose(p_ctx->idx_f);
			p_ctx->idx_f = NULL;

			if (rlen == 1)
			{
				p_ctx->i_idx = idx_len / 16;
				return idx_len;
			}
		}
	}
	else if (idx_len && idx_offset)
	{
		p_ctx->idx = (int *)malloc(idx_len);
		if (p_ctx->idx == NULL)
		{
		    log_print(HT_LOG_ERR, "%s, p_ctx->idx == NULL\r\n", __FUNCTION__);
			return -1;
        }
        
		fseek(p_ctx->f, idx_offset, SEEK_SET);
		rlen = fread(p_ctx->idx, idx_len, 1, p_ctx->f);
		
		if (rlen == 1)
		{
			p_ctx->i_idx = idx_len / 16;
			return idx_len;
		}
	}

	return 0;
}

int avi_ctx_init(AVICTX * p_ctx)
{
	int rlen = 0, flen;
	AVIRIFF riff;

	fseek(p_ctx->f, 0, SEEK_END);
	flen = ftell(p_ctx->f);
	fseek(p_ctx->f, 0, SEEK_SET);

	rlen = fread(&riff, sizeof(riff), 1, p_ctx->f);
	if (rlen != 1)
	{
	    log_print(HT_LOG_ERR, "%s, fread failed!!!\r\n", __FUNCTION__);
		return -1;
	}

	if (riff.riff != mmioFOURCC('R','I','F','F') || riff.type != mmioFOURCC('A', 'V', 'I', ' '))
	{
	    log_print(HT_LOG_ERR, "%s, invalid riff!!!\r\n", __FUNCTION__);
		return -1;
	}

	if (flen != (riff.len + 8))
	{
	    log_print(HT_LOG_WARN, "%s, flen=%d, riff.len=%d\r\n", __FUNCTION__, flen, riff.len);
	}

	p_ctx->flen = flen;

	if (avi_parse_header(p_ctx) < 0)
	{
	    log_print(HT_LOG_ERR, "%s, avi_parse_header failed\r\n", __FUNCTION__);
		return -1;
	}
	
	if (avi_load_movi_list(p_ctx) <= 0)
	{
	    log_print(HT_LOG_ERR, "%s, avi_load_movi_list failed\r\n", __FUNCTION__);
		return -1;
    }
    
	avi_load_idx(p_ctx);

	p_ctx->pkt_offset = p_ctx->i_movi;

	p_ctx->i_frame_video = p_ctx->str_v.dwLength;
	p_ctx->i_frame_audio = p_ctx->str_a.dwLength;

	p_ctx->v_width = p_ctx->bmp.biWidth;
	p_ctx->v_height = p_ctx->bmp.biHeight;

	if (p_ctx->str_v.dwScale != 0)
	{
		p_ctx->v_fps = p_ctx->str_v.dwRate / p_ctx->str_v.dwScale;
    }
    
	memcpy(p_ctx->v_fcc, &p_ctx->str_v.fccHandler, 4);

	p_ctx->a_rate = p_ctx->wave.nSamplesPerSec;
	p_ctx->a_chns = p_ctx->wave.nChannels;
	p_ctx->a_fmt = p_ctx->wave.wFormatTag;

	return 0;
}

/**************************************************************************/

AVICTX * avi_read_open(const char * filename)
{
	AVICTX * p_ctx = (AVICTX *)malloc(sizeof(AVICTX));
	if (NULL == p_ctx)
	{
		log_print(HT_LOG_ERR, "%s, malloc fail!!!\r\n", __FUNCTION__);
		return NULL;
	}

	memset(p_ctx, 0, sizeof(AVICTX));

	p_ctx->ctxf_read = 1;

	p_ctx->f = fopen(filename, "rb");
	if (NULL == p_ctx->f)
	{
		log_print(HT_LOG_ERR, "%s, fopen [%s] failed!!!\r\n", __FUNCTION__, filename);
		goto read_err;
	}

	strncpy(p_ctx->filename, filename, sizeof(p_ctx->filename));
	
    // p_ctx->mutex = sys_os_create_mutex();
	
	if (avi_ctx_init(p_ctx) < 0)
	{
		avi_read_close(p_ctx);

		log_print(HT_LOG_ERR, "%s, avi_ctx_init failed!!!\r\n", __FUNCTION__);
		return NULL;
	}

	char v_codec[16];
	
	memset(v_codec, 0, sizeof(v_codec));
	memcpy(v_codec, p_ctx->v_fcc, 4);
	
	log_print(HT_LOG_INFO, "%s, avi[%s],video codec[%s],fps[%d],size[%d x %d],audio codec[%u].\r\n", __FUNCTION__,
		filename, v_codec, p_ctx->v_fps, p_ctx->bmp.biWidth, p_ctx->bmp.biHeight, p_ctx->wave.wFormatTag);

	return p_ctx;

read_err:

	if (p_ctx && p_ctx->f)
	{
		fclose(p_ctx->f);
	}
	
	if (p_ctx)
	{
		free(p_ctx);
    }
    
	return NULL;
}

void avi_read_close(AVICTX * p_ctx)
{
	if (p_ctx == NULL)
	{
		return;
    }
    
    // sys_os_mutex_enter(p_ctx->mutex);

	fclose(p_ctx->f);
	p_ctx->f = NULL;

	if (p_ctx->idx)
	{
		free(p_ctx->idx);
		p_ctx->idx = NULL;
	}

    // sys_os_destroy_sig_mutex(p_ctx->mutex);

	free(p_ctx);
}

int avi_read_pkt(AVICTX * p_ctx, AVIPKT * p_pkt)
{
	char riff[4];	    // packet type: 01wb 00dc
	uint32 len = 0;	    // packet length

	if (p_ctx->pkt_offset >= p_ctx->i_movi_end)
	{
		// Already read the end of the packet, the latter should be the index
		return 0;
	}

	if (p_ctx->f == NULL)
	{
		log_print(HT_LOG_ERR, "%s, p_ctx->f is null!!!\r\n", __FUNCTION__);
		return -1;
	}

	fseek(p_ctx->f, p_ctx->pkt_offset, SEEK_SET);
	
	if (fread(riff, 4, 1, p_ctx->f) != 1)
	{
		log_print(HT_LOG_ERR, "%s, fread filed\r\n", __FUNCTION__);
		return -1;
	}
	
	if (fread(&len, 4, 1, p_ctx->f) != 1)
	{
		log_print(HT_LOG_ERR, "%s, fread failed\r\n", __FUNCTION__);
		return -1;
	}

	if (len < 0 || len > (uint32)(p_ctx->i_movi_end - p_ctx->i_movi) || len > (1024 * 1024))
	{
		log_print(HT_LOG_ERR, "%s, invalid len (%d)\r\n", __FUNCTION__, len);
		return -1;
	}

	if (p_pkt->rbuf == NULL || p_pkt->mlen < len)	// The buffer allocated earlier is not long enough // Continue to use the previous buffer
	{
		if (p_pkt->rbuf)
		{
			free(p_pkt->rbuf);
			p_pkt->rbuf = NULL;
			p_pkt->dbuf = NULL;
			p_pkt->mlen = 0;
		}
		
		p_pkt->rbuf = (char *)malloc(len+128);	// Reserve RTP header and slice header length
		if (p_pkt->rbuf == NULL)
		{
			log_print(HT_LOG_ERR, "%s, malloc failed\r\n", __FUNCTION__);
			return -1;
		}

		p_pkt->mlen = len;
		p_pkt->dbuf = p_pkt->rbuf+128;
	}
	
	if (fread(p_pkt->dbuf, len, 1, p_ctx->f) != 1)
	{		
		free(p_pkt->rbuf);
		p_pkt->rbuf = NULL;
		p_pkt->dbuf = NULL;
		p_pkt->mlen = 0;

		log_print(HT_LOG_ERR, "%s, fread len %d\r\n", __FUNCTION__, len);
		
		return -1;
	}
	
	p_pkt->len = len;

	if (riff[2] == 'd' && riff[3] == 'c')       // video
	{
	    p_pkt->type = PACKET_TYPE_VIDEO;
	}
	else if (riff[2] == 'w' && riff[3] == 'b')  // audio
	{
	    p_pkt->type = PACKET_TYPE_AUDIO;
	}
	else 
	{
	    p_pkt->type = PACKET_TYPE_UNKNOW;
	}

	if ((len & 1) == 1)	
	{
	    len++;
    }
    
	p_ctx->pkt_offset += 8 + len;	// Update next read offset
	p_ctx->back_index = p_ctx->index_offset;
	p_ctx->index_offset++;	        // Update next read offset

	return len;
}

int avi_seek_pos(AVICTX * p_ctx, long pos, long total)
{
	if (p_ctx == NULL)
	{
		log_print(HT_LOG_ERR, "%s, p_ctx is null!!!\r\n", __FUNCTION__);
		return -1;
	}
	
	if (pos > total)
	{
		log_print(HT_LOG_ERR, "%s, pos[%ld] > total[%ld]!!!\r\n", __FUNCTION__, pos, total);
		return -1;
	}

    // log_print(HT_LOG_ERR, "%s, pos[%d],total[%d],fname[%s]...\r\n", __FUNCTION__, pos, total, p_ctx->filename);

	// Pos is the relative time, total is the total duration
	if (p_ctx->ctxf_idx != 1 || p_ctx->idx == NULL)	// In case the index is incomplete, the index should be rebuilt
	{
		log_print(HT_LOG_ERR, "%s, avif_idx[%d],idx[%p]!!!\r\n", __FUNCTION__, p_ctx->ctxf_idx, p_ctx->idx);
		return -1;
	}

	if (p_ctx->i_frame_video < 1 || total < 1 || pos < 0)
	{
		log_print(HT_LOG_ERR, "%s, i_frame_video[%d],total[%d],pos[%d]!!!\r\n", __FUNCTION__, p_ctx->i_frame_video, total, pos);
		return -1;
	}

	double rate = p_ctx->i_frame_video * 1.0 / total;	// How many frames per second
	int index = (int)(pos * rate);
	
	if (index >= p_ctx->i_idx)
	{
		log_print(HT_LOG_ERR, "%s, index[%d],i_idx[%d]!!!\r\n", __FUNCTION__, index, p_ctx->i_idx);
		return -1;
	}

    // log_print(HT_LOG_ERR, "%s, index[%d],pos[%d],rate[%.2f],i_frame_video[%d].\r\n", __FUNCTION__, index, pos, rate, p_ctx->i_frame_video);

	while (index < p_ctx->i_idx && p_ctx->idx)
	{
		if ((p_ctx->idx[index * 4] != mmioFOURCC('0','0','d','c')) || (p_ctx->idx[index * 4 + 1] != AVIIF_KEYFRAME))
		{
			index++;
			continue;
		}
		
		long fpos_prev = ftell(p_ctx->f);
		int fpos = p_ctx->idx[index * 4 + 2];
		int spos = fseek(p_ctx->f, fpos, SEEK_SET);
		
		if (spos < 0)
		{
			log_print(HT_LOG_ERR, "%s, index[%d], fpos[%d], prev pos[%ld]!!!\r\n", __FUNCTION__, index, fpos, fpos_prev);
			return -1;
		}
		else
		{
			p_ctx->pkt_offset = fpos;
			p_ctx->index_offset = index;
			p_ctx->back_index = index;
			
			log_print(HT_LOG_ERR, "%s, index[%d], new pos[%d], set pos[%d], prev pos[%ld].\r\n", __FUNCTION__, index, fpos, spos, fpos_prev);

			return 0;
		}
	}
	
    // log_print(HT_LOG_ERR, "%s, index[%d],i_idx[%d], seek failed!!!\r\n", __FUNCTION__, index, p_ctx->i_idx);
	
	return -1;
}

int avi_seek_back_pos(AVICTX * p_ctx)
{
	int index = p_ctx->back_index - 1;
	if (index < 0 || p_ctx->idx == NULL)
	{
		return -1;
    }
    
	while (index >= 0 && index < p_ctx->i_idx)
	{
		if ((p_ctx->idx[index * 4] != mmioFOURCC('0','0','d','c')) || (p_ctx->idx[index * 4 + 1] != AVIIF_KEYFRAME))
		{
			index--;
			continue;
		}
		
		long fpos_prev = ftell(p_ctx->f);
		int fpos = p_ctx->idx[index * 4 + 2];
		int spos = fseek(p_ctx->f, fpos, SEEK_SET);
		
		if (spos < 0)
		{
		    // log_print(HT_LOG_ERR, "%s, index[%d], fpos[%d], prev pos[%ld]!!!\r\n", __FUNCTION__, index, fpos, fpos_prev);
			return -1;
		}
		else
		{
			p_ctx->pkt_offset = fpos;
			p_ctx->index_offset = index;
			p_ctx->back_index = index;
		    // log_print(HT_LOG_ERR, "%s, index[%d], new pos[%d], set pos[%d], prev pos[%ld].\r\n", __FUNCTION__, index, fpos, spos, fpos_prev);
			return 0;
		}
	}

	return -1;
}

int avi_seek_tail(AVICTX * p_ctx)
{
	int index = p_ctx->i_idx - 1;
	if (index < 0 || p_ctx->idx == NULL)
	{
		return -1;
    }
    
	while (index >= 0 && index < p_ctx->i_idx)
	{
		if ((p_ctx->idx[index * 4] != mmioFOURCC('0','0','d','c')) || (p_ctx->idx[index * 4 + 1] != AVIIF_KEYFRAME))
		{
			index--;
			continue;
		}
		
		long fpos_prev = ftell(p_ctx->f);
		int fpos = p_ctx->idx[index * 4 + 2];
		int spos = fseek(p_ctx->f, fpos, SEEK_SET);

		if (spos < 0)
		{
		    // log_print(HT_LOG_ERR, "%s, index[%d], fpos[%d], prev pos[%ld]!!!\r\n", __FUNCTION__, index, fpos, fpos_prev);
			return -1;
		}
		else
		{
			p_ctx->pkt_offset = fpos;
			p_ctx->index_offset = index;
			p_ctx->back_index = index;
		    // log_print(HT_LOG_ERR, "%s, index[%d], new pos[%d], set pos[%d], prev pos[%ld].\r\n", __FUNCTION__, index, fpos, spos, fpos_prev);
			return 0;
		}
	}

	return -1;
}



