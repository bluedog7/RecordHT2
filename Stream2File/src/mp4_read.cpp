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
#include "mp4_ctx.h"
#include "mp4_read.h"


/**************************************************************************/

int mp4_parse_h264(MP4CTX * p_ctx, int trackid)
{
    GF_AVCConfig * avcc;
    
    avcc = gf_isom_avc_config_get(p_ctx->handler, trackid, 1);
    if (avcc)
    {
        if (gf_list_count(avcc->sequenceParameterSets) > 0)
        {
            GF_AVCConfigSlot * sps = (GF_AVCConfigSlot *)gf_list_get(avcc->sequenceParameterSets, 0);
            if (sps)
            {
                p_ctx->ctxf_sps_f = 1;
                p_ctx->sps_len = sps->size;
                memcpy(p_ctx->sps, sps->data, sps->size);
            }
        }

        if (gf_list_count(avcc->pictureParameterSets) > 0)
        {
            GF_AVCConfigSlot * pps = (GF_AVCConfigSlot *)gf_list_get(avcc->pictureParameterSets, 0);
            if (pps)
            {
                p_ctx->ctxf_pps_f = 1;
                p_ctx->pps_len = pps->size;
                memcpy(p_ctx->pps, pps->data, pps->size);
            }
        }

        gf_odf_avc_cfg_del(avcc);
    }

    return 1;
}

int mp4_parse_h265(MP4CTX * p_ctx, int trackid)
{
    GF_HEVCConfig *hvcc;
    
    hvcc = gf_isom_hevc_config_get(p_ctx->handler, trackid, 1);
    if (!hvcc) 
    {
        hvcc = gf_isom_lhvc_config_get(p_ctx->handler, trackid, 1);
    }
		
    if (hvcc)
    {
        uint32 i;
        uint32 cnt = gf_list_count(hvcc->param_array);

        for (i = 0; i < cnt; i++)
        {
            GF_HEVCParamArray * param = (GF_HEVCParamArray *)gf_list_get(hvcc->param_array, i);
            if (param)
            {
                if (param->type == GF_HEVC_NALU_VID_PARAM)
                {
                    if (gf_list_count(param->nalus) > 0)
                    {
                        GF_AVCConfigSlot * vps = (GF_AVCConfigSlot *)gf_list_get(param->nalus, 0);
                        if (vps)
                        {
                            p_ctx->ctxf_vps_f = 1;
                            p_ctx->vps_len = vps->size;
                            memcpy(p_ctx->vps, vps->data, vps->size);
                        }
                    }
                }
                else if (param->type == GF_HEVC_NALU_SEQ_PARAM)
                {
                    if (gf_list_count(param->nalus) > 0)
                    {
                        GF_AVCConfigSlot * sps = (GF_AVCConfigSlot *)gf_list_get(param->nalus, 0);
                        if (sps)
                        {
                            p_ctx->ctxf_sps_f = 1;
                            p_ctx->sps_len = sps->size;
                            memcpy(p_ctx->sps, sps->data, sps->size);
                        }
                    }
                }
                else if (param->type == GF_HEVC_NALU_PIC_PARAM)
                {
                    if (gf_list_count(param->nalus) > 0)
                    {
                        GF_AVCConfigSlot * pps = (GF_AVCConfigSlot *)gf_list_get(param->nalus, 0);
                        if (pps)
                        {
                            p_ctx->ctxf_pps_f = 1;
                            p_ctx->pps_len = pps->size;
                            memcpy(p_ctx->pps, pps->data, pps->size);
                        }
                    }
                }
            }
        }

        gf_odf_hevc_cfg_del(hvcc);
    }

    return 1;
}

int mp4_parse_video(MP4CTX * p_ctx, int trackid)
{
    if (GF_OK != gf_isom_get_visual_info(p_ctx->handler, trackid, 1, (uint32 *)&p_ctx->v_width, (uint32 *)&p_ctx->v_height))
    {
        return 0;
    }

    p_ctx->v_track_id = trackid;
    p_ctx->i_frame_video = gf_isom_get_sample_count(p_ctx->handler, trackid);

    uint32 timescale = gf_isom_get_media_timescale(p_ctx->handler, trackid);
    u64 duration = gf_isom_get_media_duration(p_ctx->handler, trackid);
    int r_duration = 0;

    if (timescale > 0)
    {
        r_duration = (int) (duration / timescale);
    }

    if (r_duration > 0)
    {
        p_ctx->v_fps = p_ctx->i_frame_video / r_duration;
    }
    
    uint32 subtype = gf_isom_get_media_subtype(p_ctx->handler, trackid, 1);

    switch (subtype)
    {
    case GF_ISOM_SUBTYPE_AVC_H264:
	case GF_ISOM_SUBTYPE_AVC2_H264:
	case GF_ISOM_SUBTYPE_AVC3_H264:
	case GF_ISOM_SUBTYPE_AVC4_H264:
	    memcpy(p_ctx->v_fcc, "H264", 4);
	    mp4_parse_h264(p_ctx, trackid);
	    break;

	case GF_ISOM_SUBTYPE_HVC1:
	case GF_ISOM_SUBTYPE_HEV1:
	case GF_ISOM_SUBTYPE_HVC2:
	case GF_ISOM_SUBTYPE_HEV2:
	case GF_ISOM_SUBTYPE_HVT1:
	case GF_ISOM_SUBTYPE_LHV1:
	case GF_ISOM_SUBTYPE_LHE1:
	    memcpy(p_ctx->v_fcc, "H265", 4);
	    mp4_parse_h265(p_ctx, trackid);
	    break;

	default:
	    return 0;
    }
            
    return 1;
}

int mp4_parse_audio(MP4CTX * p_ctx, int trackid)
{
    int ret = 0;
    uint8 bps = 0;
    
    if (GF_OK != gf_isom_get_audio_info(p_ctx->handler, trackid, 1, (uint32 *)&p_ctx->a_rate, (uint32 *)&p_ctx->a_chns, &bps))
    {
        return 0;
    }

    p_ctx->a_track_id = trackid;
    p_ctx->i_frame_audio = gf_isom_get_sample_count(p_ctx->handler, trackid);

    GF_ESD * esd;
    uint32 subtype = gf_isom_get_media_subtype(p_ctx->handler, trackid, 1);

    switch (subtype)
    {
    case GF_ISOM_SUBTYPE_MPEG4:
        esd = gf_isom_get_esd(p_ctx->handler, trackid, 1);
        if (esd)
        {
            if (esd->decoderConfig->streamType == GF_STREAM_AUDIO)
            {
                switch (esd->decoderConfig->objectTypeIndication) 
                {
                case GPAC_OTI_AUDIO_AAC_MPEG4:
                    p_ctx->a_fmt = AUDIO_FORMAT_AAC;
                    ret = 1;
                    break;
                }
            }

            gf_odf_desc_del((GF_Descriptor *) esd);
	    }
	    break;

	default:
	    return 0;    
    }
            
    return ret;
}

/**************************************************************************/

MP4CTX * mp4_read_open(const char * filename)
{
    uint32 i;
	uint32 nb_tracks;
	
    MP4CTX * p_ctx = (MP4CTX *)malloc(sizeof(MP4CTX));
	if (NULL == p_ctx)
	{
		log_print(HT_LOG_ERR, "%s, malloc fail!!!\r\n", __FUNCTION__);
		return NULL;
	}

	memset(p_ctx, 0, sizeof(MP4CTX));

	p_ctx->ctxf_read = 1;

	p_ctx->handler = gf_isom_open(filename, GF_ISOM_OPEN_READ, NULL);
	if (NULL == p_ctx->handler)
	{
		log_print(HT_LOG_ERR, "%s, gf_isom_open [%s] failed!!!\r\n", __FUNCTION__, filename);
		goto read_err;
	}

	strncpy(p_ctx->filename, filename, sizeof(p_ctx->filename));

	nb_tracks = gf_isom_get_track_count(p_ctx->handler);

	for (i=0; i<nb_tracks; i++) 
	{
	    uint32 mediatype = gf_isom_get_media_type(p_ctx->handler, i+1);
	    
        if (mediatype == GF_ISOM_MEDIA_VISUAL)
        {
            p_ctx->ctxf_video = mp4_parse_video(p_ctx, i+1);
        }
        else if (mediatype == GF_ISOM_MEDIA_AUDIO)
        {
            p_ctx->ctxf_audio = mp4_parse_audio(p_ctx, i+1);
        }
	}

	return p_ctx;

read_err:

	if (p_ctx && p_ctx->handler)
	{
		gf_isom_close(p_ctx->handler);
	}
	
	if (p_ctx)
	{
		free(p_ctx);
    }
    
	return NULL;

}

void mp4_read_close(MP4CTX * p_ctx)
{
    if (p_ctx == NULL)
	{
		return;
    }

	gf_isom_close(p_ctx->handler);
	p_ctx->handler = NULL;

	free(p_ctx);
}

int mp4_read_pkt(MP4CTX * p_ctx, MP4PKT * p_pkt)
{
    if (p_ctx == NULL || p_ctx->handler == NULL)
	{
		return -1;
    }

    if (p_ctx->v_frame_idx >= p_ctx->i_frame_video && p_ctx->a_frame_idx >= p_ctx->i_frame_audio)
    {
        return 0;
    }

    int trackid;
    int frameindex;
    GF_ISOSample * sample;

    double vratio = 1.0;
    double aratio = 1.0;

    if (p_ctx->ctxf_video && p_ctx->i_frame_video > 0)
    {
        vratio = (double)p_ctx->v_frame_idx / p_ctx->i_frame_video;
    }

    if (p_ctx->ctxf_audio && p_ctx->i_frame_audio > 0)
    {
        aratio = (double)p_ctx->a_frame_idx / p_ctx->i_frame_audio;
    }
    
    if (vratio < aratio)
    {
        if (p_ctx->ctxf_video && p_ctx->v_frame_idx < p_ctx->i_frame_video)
        {
            trackid = p_ctx->v_track_id;
            frameindex = p_ctx->v_frame_idx++;
            p_pkt->type = PACKET_TYPE_VIDEO;
        }
        else if (p_ctx->ctxf_audio && p_ctx->a_frame_idx < p_ctx->i_frame_audio)
        {
            trackid = p_ctx->a_track_id;
            frameindex = p_ctx->a_frame_idx++;
            p_pkt->type = PACKET_TYPE_AUDIO;
        }
        else
        {
            return -1;
        }
    }
    else
    {
        if (p_ctx->ctxf_audio && p_ctx->a_frame_idx < p_ctx->i_frame_audio)
        {
            trackid = p_ctx->a_track_id;
            frameindex = p_ctx->a_frame_idx++;
            p_pkt->type = PACKET_TYPE_AUDIO;
        }
        else if (p_ctx->ctxf_video && p_ctx->v_frame_idx < p_ctx->i_frame_video)
        {
            trackid = p_ctx->v_track_id;
            frameindex = p_ctx->v_frame_idx++;
            p_pkt->type = PACKET_TYPE_VIDEO;
        }
        else
        {
            return -1;
        }
    }

    sample = gf_isom_get_sample(p_ctx->handler, trackid, frameindex+1, NULL);
    if (sample)
    {
        if (p_pkt->rbuf == NULL || p_pkt->mlen < sample->dataLength)    // The buffer allocated earlier is not long enough
    	{
    		if (p_pkt->rbuf)
    		{
    			free(p_pkt->rbuf);
    			p_pkt->rbuf = NULL;
    			p_pkt->dbuf = NULL;
    			p_pkt->mlen = 0;
    		}
    		
    		p_pkt->rbuf = (char *)malloc(sample->dataLength+128);	    // Reserve RTP header and slice header length
    		if (p_pkt->rbuf == NULL)
    		{
    		    gf_isom_sample_del(&sample);
    		    
    			log_print(HT_LOG_ERR, "%s, malloc failed\r\n", __FUNCTION__);
    			return -1;
    		}

    		p_pkt->mlen = sample->dataLength;
    		p_pkt->dbuf = p_pkt->rbuf+128;
    	}

    	p_pkt->len = sample->dataLength;
    	memcpy(p_pkt->dbuf, sample->data, sample->dataLength);

    	gf_isom_sample_del(&sample);
    }
    else
    {
        return -1;
    }
    
    return 1;
}

int mp4_seek_pos(MP4CTX * p_ctx, long pos)
{
    if (p_ctx == NULL || p_ctx->handler == NULL)
	{
		return -1;
    }
    
    if (p_ctx->ctxf_video)
    {
        uint32 timescale = gf_isom_get_media_timescale(p_ctx->handler, p_ctx->v_track_id);
        u64 duration = gf_isom_get_media_duration(p_ctx->handler, p_ctx->v_track_id);
        long r_duration = (long)(duration / (double)timescale) * 1000;

        if (pos <= r_duration)
        {
            p_ctx->v_frame_idx = (int)(p_ctx->i_frame_video * ((double)pos / r_duration));
        }
        else
        {
            return 0;
        }
    }

    if (p_ctx->ctxf_audio)
    {
        uint32 timescale = gf_isom_get_media_timescale(p_ctx->handler, p_ctx->a_track_id);
        u64 duration = gf_isom_get_media_duration(p_ctx->handler, p_ctx->a_track_id);
        long r_duration = (long)(duration / (double)timescale) * 1000;

        if (pos <= r_duration)
        {
            p_ctx->a_frame_idx = (int)(p_ctx->i_frame_audio * ((double)pos / r_duration));
        }
        else
        {
            return 0;
        }
    }
    
    return 1;
}

int mp4_seek_back_pos(MP4CTX * p_ctx)
{
    return 0;
}

int mp4_seek_tail(MP4CTX * p_ctx)
{
    return 0;
}



