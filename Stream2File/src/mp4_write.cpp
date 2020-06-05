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

#ifdef MP4_FORMAT

#include "sys_inc.h"
#include "mp4_ctx.h"
#include "mp4_write.h"
#include "h264.h"
#include "h265.h"
#include "mjpeg.h"
#include "media_util.h"
#include "h264_util.h"
#include "h265_util.h"
#include "bit_vector.h"
#include <math.h>
#include "rtsp_util.h"
#include "format.h"

MP4CTX * mp4_write_open(char * filename)
{
    MP4CTX * p_ctx = (MP4CTX *)malloc(sizeof(MP4CTX));
	if (p_ctx == NULL)
	{
		log_print(HT_LOG_ERR, "%s, malloc fail!!!\r\n", __FUNCTION__);
		return NULL;
	}

	memset(p_ctx, 0, sizeof(MP4CTX));

    p_ctx->ctxf_write = 1;

    strncpy(p_ctx->filename, filename, sizeof(p_ctx->filename));

    p_ctx->handler = gf_isom_open(filename, GF_ISOM_OPEN_WRITE, NULL);
    if (NULL == p_ctx->handler)
    {
        free(p_ctx);
        
        log_print(HT_LOG_ERR, "%s, MP4Create failed!!! %s\r\n", __FUNCTION__, filename);
        return NULL;
    }

    gf_isom_set_brand_info(p_ctx->handler, GF_ISOM_BRAND_MP42, 0);
    
	p_ctx->mutex = sys_os_create_mutex();
    
	return p_ctx;
}

void mp4_write_close(MP4CTX * p_ctx)
{
    if (p_ctx == NULL)
	{
		return;
    }
    
	sys_os_mutex_enter(p_ctx->mutex);

    if (p_ctx->handler)
    {
	    gf_isom_close(p_ctx->handler);
	}

	if (p_ctx->a_extra)
	{
	    free(p_ctx->a_extra);
	}

	sys_os_mutex_leave(p_ctx->mutex);

	sys_os_destroy_sig_mutex(p_ctx->mutex);

	free(p_ctx);
}

void mp4_set_video_info(MP4CTX * p_ctx, int fps, int width, int height, const char fcc[4])
{
    memcpy(p_ctx->v_fcc, fcc, 4); // "H264","H265","JPEG","MP4V"

	p_ctx->v_fps = fps;
	p_ctx->v_width  = width;
	p_ctx->v_height = height;

	p_ctx->ctxf_video = 1;
}

void mp4_set_audio_info(MP4CTX * p_ctx, int chns, int rate, uint16 fmt)
{
    p_ctx->a_chns = chns;
	p_ctx->a_rate = rate;
	p_ctx->a_fmt = fmt;

	p_ctx->ctxf_audio = 1;
}

void mp4_set_audio_extra_info(MP4CTX * p_ctx, uint8 * extra, int extra_len)
{
    if (NULL != extra && extra_len > 0)
    {
        p_ctx->a_extra = (uint8*) malloc(extra_len);
    	if (p_ctx->a_extra)
    	{
    	    memcpy(p_ctx->a_extra, extra, extra_len);
    	    p_ctx->a_extra_len = extra_len;
        }
    }
}

int mp4_write_jpeg_info(MP4CTX * p_ctx)
{
    GF_Err err;
    uint32 fps = (p_ctx->v_fps > 0) ? p_ctx->v_fps : 25;

    if (p_ctx->v_track_id > 0)
    {
        return 0;
    }
    
    p_ctx->v_track_id = gf_isom_new_track(p_ctx->handler, 0, GF_ISOM_MEDIA_VISUAL, fps);
    if (0 == p_ctx->v_track_id)
    {
        log_print(HT_LOG_ERR, "%s, gf_isom_new_track failed\r\n", __FUNCTION__);
        return -1;
    }
    
    err = gf_isom_set_track_enabled(p_ctx->handler, p_ctx->v_track_id, 1);
    if (GF_OK != err)
    {
        log_print(HT_LOG_ERR, "%s, gf_isom_set_track_enabled failed\r\n", __FUNCTION__);
        return -1;
    }

    GF_ESD * p_esd = gf_odf_desc_esd_new(0);
	if (NULL == p_esd)
    {
        log_print(HT_LOG_ERR, "%s, gf_odf_desc_esd_new failed\r\n", __FUNCTION__);
        return -1;
    }

	p_esd->ESID = gf_isom_get_track_id(p_ctx->handler, p_ctx->v_track_id);
	p_esd->decoderConfig->streamType = GF_STREAM_VISUAL;
	p_esd->decoderConfig->objectTypeIndication = GPAC_OTI_IMAGE_JPEG;
	p_esd->decoderConfig->bufferSizeDB = 200*1024;
    p_esd->decoderConfig->avgBitrate = 8*200*1024;
    p_esd->decoderConfig->maxBitrate = 8*200*1024;
	p_esd->slConfig->timestampResolution = fps;
	
	err = gf_isom_new_mpeg4_description(p_ctx->handler, p_ctx->v_track_id, p_esd, NULL, NULL, &p_ctx->v_stream_idx);
    if (GF_OK != err)
    {
        log_print(HT_LOG_ERR, "%s, gf_isom_new_mpeg4_description failed\r\n", __FUNCTION__);
        return -1;
    }
    
    err = gf_isom_set_visual_info(p_ctx->handler, p_ctx->v_track_id, p_ctx->v_stream_idx, p_ctx->v_width, p_ctx->v_height);
    if (GF_OK != err)
    {
        log_print(HT_LOG_ERR, "%s, gf_isom_set_visual_info failed\r\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int mp4_write_aac_info(MP4CTX * p_ctx)
{
    GF_Err err;

    if (p_ctx->a_track_id > 0)
    {
        return 0;
    }
    
    p_ctx->a_track_id = gf_isom_new_track(p_ctx->handler, 0, GF_ISOM_MEDIA_AUDIO, p_ctx->a_rate);
    if (0 == p_ctx->a_track_id)
    {
        log_print(HT_LOG_ERR, "%s, gf_isom_new_track failed\r\n", __FUNCTION__);
        return -1;
    }
    
	err = gf_isom_set_track_enabled(p_ctx->handler, p_ctx->a_track_id, 1);
    if (GF_OK != err)
    {
        log_print(HT_LOG_ERR, "%s, gf_isom_set_track_enabled failed\r\n", __FUNCTION__);
        return -1;
    }
    
	GF_ESD * p_esd = gf_odf_desc_esd_new(0);
	if (NULL == p_esd)
    {
        log_print(HT_LOG_ERR, "%s, gf_odf_desc_esd_new failed\r\n", __FUNCTION__);
        return -1;
    }
	
	p_esd->ESID = gf_isom_get_track_id(p_ctx->handler, p_ctx->a_track_id);
	p_esd->OCRESID = p_esd->ESID;
	p_esd->decoderConfig->streamType = GF_STREAM_AUDIO;
	p_esd->decoderConfig->objectTypeIndication = GPAC_OTI_AUDIO_AAC_MPEG4;
	p_esd->slConfig->timestampResolution = p_ctx->a_rate;
	p_esd->decoderConfig->decoderSpecificInfo = (GF_DefaultDescriptor *)gf_odf_desc_new(GF_ODF_DSI_TAG);
	p_esd->decoderConfig->decoderSpecificInfo->data = (char *)p_ctx->a_extra;
	p_esd->decoderConfig->decoderSpecificInfo->dataLength = p_ctx->a_extra_len;
	
	err = gf_isom_new_mpeg4_description(p_ctx->handler, p_ctx->a_track_id, p_esd, NULL, NULL, &p_ctx->a_stream_idx);
    if (GF_OK != err)
    {
        log_print(HT_LOG_ERR, "%s, gf_isom_new_mpeg4_description failed\r\n", __FUNCTION__);
        return -1;
    }
    
	err = gf_isom_set_audio_info(p_ctx->handler, p_ctx->a_track_id, p_ctx->a_stream_idx, p_ctx->a_rate, p_ctx->a_chns, 16, GF_IMPORT_AUDIO_SAMPLE_ENTRY_NOT_SET);
    if (GF_OK != err)
    {
        log_print(HT_LOG_ERR, "%s, gf_isom_set_audio_info failed\r\n", __FUNCTION__);
        return -1;
    }
    
	return 0;
}

int mp4_write_header(MP4CTX * p_ctx)
{
    int ret = 0;

    if (p_ctx->ctxf_video)
    {
        if (memcmp(p_ctx->v_fcc, "JPEG", 4) == 0)
        {
            ret = mp4_write_jpeg_info(p_ctx);
        }
    }

    if (p_ctx->ctxf_audio)
    {
        if (p_ctx->a_fmt == AUDIO_FORMAT_AAC) // AAC
        {
            ret = mp4_write_aac_info(p_ctx);
        }
    }
    
    return ret;
}

int mp4_update_header(MP4CTX * p_ctx)
{
    if (NULL == p_ctx || NULL == p_ctx->handler)
	{
		return -1;
    }

    sys_os_mutex_enter(p_ctx->mutex);
    
    mp4_write_header(p_ctx);

    if (p_ctx->v_track_id > 0)
    {
        gf_isom_set_media_timescale(p_ctx->handler, p_ctx->v_track_id, p_ctx->v_fps > 0 ? p_ctx->v_fps : 25, GF_FALSE);
    }

    sys_os_mutex_leave(p_ctx->mutex);
    
    return 0;
}

int mp4_write_audio_frame(MP4CTX * p_ctx, void * p_data, uint32 len)
{
    int ret = 0;
    GF_Err err;
    uint32 ts = rtsp_get_timestamp(p_ctx->a_rate);
    
    if (p_ctx->a_track_id == 0)
    {
        return -1;
    }

    if (p_ctx->a_timestamp == 0)
    {
        p_ctx->a_timestamp = ts;
    }
    
    GF_ISOSample * p_sample = gf_isom_sample_new();
    
    p_sample->IsRAP = (SAPType)1;
    p_sample->dataLength = len;
    p_sample->data = (char *)p_data;
    p_sample->DTS = ts - p_ctx->a_timestamp;
    p_sample->CTS_Offset = 0;

    err = gf_isom_add_sample(p_ctx->handler, p_ctx->a_track_id, p_ctx->a_stream_idx, p_sample);			
    if (GF_OK != err)
    {
        ret = -1;
        log_print(HT_LOG_ERR, "%s, gf_isom_add_sample failed, err=%d\r\n", __FUNCTION__, err);
    }

    p_sample->data = NULL;
    p_sample->dataLength = 0;
    
    gf_isom_sample_del(&p_sample);

    if (ret == 0)
    {
        p_ctx->i_frame_audio++;
    }
    
    return ret;
}

int mp4_write_audio(MP4CTX * p_ctx, void * p_data, uint32 len)
{
    int ret = 0;

    if (p_ctx->ctxf_video)
    {
        if (memcmp(p_ctx->v_fcc, "H264", 4) == 0 || 
            memcmp(p_ctx->v_fcc, "H265", 4) == 0)
        {
            if (!p_ctx->ctxf_iframe)
            {
                return 0;
            }
        }
    }
    
    sys_os_mutex_enter(p_ctx->mutex);
    
    if (p_ctx->a_fmt == AUDIO_FORMAT_AAC) // AAC
    {
		uint8 * p_buf = (uint8 *)p_data;

        if (p_buf[0] == 0xFF && (p_buf[1] & 0xF0) == 0xF0)
        {
            ret = mp4_write_audio_frame(p_ctx, p_buf + 7, len - 7);
        }    
        else
        {
            ret = mp4_write_audio_frame(p_ctx, p_buf, len);
		}
    }

    sys_os_mutex_leave(p_ctx->mutex);
    
    return ret;
}

int mp4_write_h264_nalu(MP4CTX * p_ctx)
{
    GF_Err err;
    uint32 fps = (p_ctx->v_fps > 0) ? p_ctx->v_fps : 25;
    
    p_ctx->v_track_id = gf_isom_new_track(p_ctx->handler, 0, GF_ISOM_MEDIA_VISUAL, fps);
    if (0 == p_ctx->v_track_id)
    {
        log_print(HT_LOG_ERR, "%s, gf_isom_new_track failed\r\n", __FUNCTION__);
        return -1;
    }
    
    err = gf_isom_set_track_enabled(p_ctx->handler, p_ctx->v_track_id, 1);
    if (GF_OK != err)
    {
        log_print(HT_LOG_ERR, "%s, gf_isom_set_track_enabled failed\r\n", __FUNCTION__);
        return -1;
    }

    GF_AVCConfig * p_avc_cfg = gf_odf_avc_cfg_new();
    if (NULL == p_avc_cfg)
    {
        log_print(HT_LOG_ERR, "%s, gf_odf_avc_cfg_new failed\r\n", __FUNCTION__);
        return -1;
    }
    
	err = gf_isom_avc_config_new(p_ctx->handler, p_ctx->v_track_id, p_avc_cfg, NULL, NULL, &p_ctx->v_stream_idx);
	if (GF_OK != err)
    {
        log_print(HT_LOG_ERR, "%s, gf_isom_avc_config_new failed\r\n", __FUNCTION__);
        return -1;
    }
    
	err = gf_isom_set_visual_info(p_ctx->handler, p_ctx->v_track_id, p_ctx->v_stream_idx, p_ctx->v_width, p_ctx->v_height);
    if (GF_OK != err)
    {
        log_print(HT_LOG_ERR, "%s, gf_isom_set_visual_info failed\r\n", __FUNCTION__);
        return -1;
    }
    
	p_avc_cfg->configurationVersion = 1;
	p_avc_cfg->AVCProfileIndication = p_ctx->sps[1];
	p_avc_cfg->profile_compatibility = p_ctx->sps[2];
	p_avc_cfg->AVCLevelIndication = p_ctx->sps[3];

	GF_AVCConfigSlot sps;
	memset(&sps, 0, sizeof(sps));

	sps.size = p_ctx->sps_len;
	sps.data = (char *)p_ctx->sps;

    gf_list_add(p_avc_cfg->sequenceParameterSets, &sps);

    GF_AVCConfigSlot pps;
	memset(&pps, 0, sizeof(pps));
	
    pps.size = p_ctx->pps_len;
	pps.data = (char *)p_ctx->pps;

	gf_list_add(p_avc_cfg->pictureParameterSets, &pps);

	err = gf_isom_avc_config_update(p_ctx->handler, p_ctx->v_track_id, p_ctx->v_stream_idx, p_avc_cfg);
    if (GF_OK != err)
    {
        log_print(HT_LOG_ERR, "%s, gf_isom_avc_config_update failed\r\n", __FUNCTION__);
        return -1;
    }
    
    p_avc_cfg->pictureParameterSets = NULL;
	p_avc_cfg->sequenceParameterSets = NULL;

	gf_odf_avc_cfg_del(p_avc_cfg);

	p_ctx->ctxf_nalu = 1;

	return 0;
}

int mp4_write_h265_nalu(MP4CTX * p_ctx)
{
    int idx;
    GF_Err err;
    uint32 fps = (p_ctx->v_fps > 0) ? p_ctx->v_fps : 25;
    
    p_ctx->v_track_id = gf_isom_new_track(p_ctx->handler, 0, GF_ISOM_MEDIA_VISUAL, fps);
    if (0 == p_ctx->v_track_id)
    {
        log_print(HT_LOG_ERR, "%s, gf_isom_new_track failed\r\n", __FUNCTION__);
        return -1;
    }
    
    err = gf_isom_set_track_enabled(p_ctx->handler, p_ctx->v_track_id, 1);
    if (GF_OK != err)
    {
        log_print(HT_LOG_ERR, "%s, gf_isom_set_track_enabled failed\r\n", __FUNCTION__);
        return -1;
    }

    GF_HEVCConfig * p_hevc_cfg = gf_odf_hevc_cfg_new();
    if (NULL == p_hevc_cfg)
    {
        log_print(HT_LOG_ERR, "%s, gf_odf_hevc_cfg_new failed\r\n", __FUNCTION__);
        return -1;
    }
    
	p_hevc_cfg->nal_unit_size = 4;

	err = gf_isom_hevc_config_new(p_ctx->handler, p_ctx->v_track_id, p_hevc_cfg, NULL, NULL, &p_ctx->v_stream_idx);
	if (GF_OK != err)
    {
        log_print(HT_LOG_ERR, "%s, gf_isom_hevc_config_new failed\r\n", __FUNCTION__);
        return -1;
    }
    
	err = gf_isom_set_nalu_extract_mode(p_ctx->handler, p_ctx->v_track_id, GF_ISOM_NALU_EXTRACT_INSPECT);
	if (GF_OK != err)
    {
        log_print(HT_LOG_ERR, "%s, gf_isom_set_nalu_extract_mode failed\r\n", __FUNCTION__);
        return -1;
    }
    
	err = gf_isom_set_cts_packing(p_ctx->handler, p_ctx->v_track_id, GF_TRUE);
    if (GF_OK != err)
    {
        log_print(HT_LOG_ERR, "%s, gf_isom_set_cts_packing failed\r\n", __FUNCTION__);
        return -1;
    }

	p_hevc_cfg->configurationVersion = 1;

	HEVCState hevc;
	memset(&hevc, 0 ,sizeof(HEVCState));

    idx = gf_media_hevc_read_vps((char*)p_ctx->vps, p_ctx->vps_len, &hevc);
    hevc.vps[idx].crc = gf_crc_32((char*)p_ctx->vps, p_ctx->vps_len);
    p_hevc_cfg->avgFrameRate = hevc.vps[idx].rates[0].avg_pic_rate;
    p_hevc_cfg->constantFrameRate = hevc.vps[idx].rates[0].constand_pic_rate_idc;
    p_hevc_cfg->numTemporalLayers = hevc.vps[idx].max_sub_layers;
    p_hevc_cfg->temporalIdNested = hevc.vps[idx].temporal_id_nesting;

    GF_AVCConfigSlot vps;
	memset(&vps, 0, sizeof(vps));
	
	GF_HEVCParamArray vps_param;
	memset(&vps_param, 0, sizeof(vps_param));
	
	vps_param.nalus = gf_list_new();
    gf_list_add(p_hevc_cfg->param_array, &vps_param);
    vps_param.array_completeness = 1;
    vps_param.type = GF_HEVC_NALU_VID_PARAM;
    vps.id = idx;
    vps.size = p_ctx->vps_len;
    vps.data = (char *)p_ctx->vps;
    gf_list_add(vps_param.nalus, &vps);
    
    idx = gf_media_hevc_read_sps((char*)p_ctx->sps, p_ctx->sps_len, &hevc);
    hevc.sps[idx].crc = gf_crc_32((char*)p_ctx->sps, p_ctx->sps_len);
    p_hevc_cfg->profile_space = hevc.sps[idx].ptl.profile_space;
    p_hevc_cfg->tier_flag = hevc.sps[idx].ptl.tier_flag;
    p_hevc_cfg->profile_idc = hevc.sps[idx].ptl.profile_idc;

    GF_AVCConfigSlot sps;
	memset(&sps, 0, sizeof(sps));
	
	GF_HEVCParamArray sps_param;
	memset(&sps_param, 0, sizeof(sps_param));
	
	sps_param.nalus = gf_list_new();
    gf_list_add(p_hevc_cfg->param_array, &sps_param);
    sps_param.array_completeness = 1;
    sps_param.type = GF_HEVC_NALU_SEQ_PARAM;
    sps.id = idx;
    sps.size = p_ctx->sps_len;
    sps.data = (char *)p_ctx->sps;
    gf_list_add(sps_param.nalus, &sps);

    idx = gf_media_hevc_read_pps((char*)p_ctx->pps, p_ctx->pps_len, &hevc);
    hevc.pps[idx].crc = gf_crc_32((char*)p_ctx->pps, p_ctx->pps_len);

    GF_AVCConfigSlot pps;
	memset(&pps, 0, sizeof(pps));
	
	GF_HEVCParamArray pps_param;
	memset(&pps_param, 0, sizeof(pps_param));
	
	pps_param.nalus = gf_list_new();
    gf_list_add(p_hevc_cfg->param_array, &pps_param);
    pps_param.array_completeness = 1;
    pps_param.type = GF_HEVC_NALU_PIC_PARAM;
    pps.id = idx;
    pps.size = p_ctx->pps_len;
    pps.data = (char *)p_ctx->pps;
    gf_list_add(pps_param.nalus, &pps);

    err = gf_isom_set_visual_info(p_ctx->handler, p_ctx->v_track_id, p_ctx->v_stream_idx, p_ctx->v_width, p_ctx->v_height);
    if (GF_OK != err)
    {
        log_print(HT_LOG_ERR, "%s, gf_isom_set_visual_info failed\r\n", __FUNCTION__);
        return -1;
    }
    
	err = gf_isom_hevc_config_update(p_ctx->handler, p_ctx->v_track_id, p_ctx->v_stream_idx, p_hevc_cfg);
	if (GF_OK != err)
    {
        log_print(HT_LOG_ERR, "%s, gf_isom_hevc_config_update failed\r\n", __FUNCTION__);
        return -1;
    }

    if (sps_param.nalus)
    {
        gf_list_del(sps_param.nalus);
    }

    if (pps_param.nalus)
    {
        gf_list_del(pps_param.nalus);
    }

    if (vps_param.nalus)
    {
        gf_list_del(vps_param.nalus);
    }

    p_hevc_cfg->param_array = NULL;
    
	gf_odf_hevc_cfg_del(p_hevc_cfg);

    p_ctx->ctxf_nalu = 1;
    
	return 0;
}

int mp4_write_video_frame(MP4CTX * p_ctx, void * p_data, uint32 len, int b_key)
{
    int ret = 0;
    uint32 nlen;
    GF_Err err;
    
	GF_ISOSample * p_sample = gf_isom_sample_new();
	if (NULL == p_sample)
	{
	    log_print(HT_LOG_ERR, "%s, gf_isom_sample_new failed\r\n", __FUNCTION__);
        return -1;
	}
	
	memset(p_sample, 0, sizeof(GF_ISOSample));

    if (memcmp(p_ctx->v_fcc, "H264", 4) == 0 || memcmp(p_ctx->v_fcc, "H265", 4) == 0)
    {
        nlen = htonl(len - 4);
        memcpy(p_data, &nlen, 4);
	}
	
	p_sample->IsRAP = (SAPType)b_key;
	p_sample->dataLength = len;
	p_sample->data = (char *)p_data;
	p_sample->DTS = p_ctx->v_timestamp;
	p_sample->CTS_Offset = 0;
	
	err = gf_isom_add_sample(p_ctx->handler, p_ctx->v_track_id, p_ctx->v_stream_idx, p_sample);			
	if (GF_OK != err)
	{
	    ret = -1;
		log_print(HT_LOG_ERR, "%s, gf_isom_add_sample failed, err=%d\r\n", __FUNCTION__, err);
	}
    
	p_sample->data = NULL;
	p_sample->dataLength = 0;
	
	gf_isom_sample_del(&p_sample);

    if (ret == 0)
    {
        p_ctx->v_timestamp += 1;
        p_ctx->i_frame_video++;
        
    	if (p_ctx->s_time == 0)
    	{
    		p_ctx->s_time = sys_os_get_ms();
    		p_ctx->e_time = p_ctx->s_time;
    	}
    	else
    	{
    		p_ctx->e_time = sys_os_get_ms();
    	}
	}
	
    return ret;
}

int mp4_write_h264(MP4CTX * p_ctx, void * p_data, uint32 len, int b_key)
{
    uint8 nalu = (((uint8 *)p_data)[4] & 0x1f);

    if (H264_NAL_SPS == nalu)
    {
        if (0 == p_ctx->sps_len)
        {
            memcpy(p_ctx->sps, (uint8 *)p_data+4, len-4);
            p_ctx->sps_len = len-4;

            if (p_ctx->sps_len > 0 && p_ctx->pps_len > 0)
            {
                if (mp4_write_h264_nalu(p_ctx) < 0)
                {
                    log_print(HT_LOG_ERR, "%s, mp4_write_h264_nalu failed\r\n", __FUNCTION__);
                    return -1;
                }
            }
        }
    }
    else if (H264_NAL_PPS == nalu)
    {
        if (0 == p_ctx->pps_len)
        {
            memcpy(p_ctx->pps, (uint8 *)p_data+4, len-4);
            p_ctx->pps_len = len-4;

            if (p_ctx->sps_len > 0 && p_ctx->pps_len > 0)
            {
                if (mp4_write_h264_nalu(p_ctx) < 0)
                {
                    log_print(HT_LOG_ERR, "%s, mp4_write_h264_nalu failed\r\n", __FUNCTION__);
                    return -1;
                }
            }
        }
    }
    else if (H264_NAL_SEI == nalu)
    {
    }
    else if (p_ctx->ctxf_nalu)
    {
        if (!p_ctx->ctxf_iframe)
        {
            if (b_key)
            {
                if (mp4_write_video_frame(p_ctx, p_data, len, b_key) < 0)
                {
                    log_print(HT_LOG_ERR, "%s, mp4_write_video_frame failed\r\n", __FUNCTION__);
                    return -1;
                }
                else
                {
                    p_ctx->ctxf_iframe = 1;
                }
            }
        }
        else
        {
            if (mp4_write_video_frame(p_ctx, p_data, len, b_key) < 0)
            {
                log_print(HT_LOG_ERR, "%s, mp4_write_video_frame failed\r\n", __FUNCTION__);
                return -1;
            }
        }
    }

    return 0;
}

int mp4_write_h265(MP4CTX * p_ctx, void * p_data, uint32 len, int b_key)
{
    uint8 nalu = ((((uint8 *)p_data)[4] >> 1) & 0x3F);

    if (HEVC_NAL_SPS == nalu)
    {
        if (0 == p_ctx->sps_len)
        {
            memcpy(p_ctx->sps, (uint8 *)p_data+4, len-4);
            p_ctx->sps_len = len-4;

            if (p_ctx->sps_len > 0 && p_ctx->pps_len > 0 && p_ctx->vps_len > 0)
            {
                if (mp4_write_h265_nalu(p_ctx) < 0)
                {
                    log_print(HT_LOG_ERR, "%s, mp4_write_h264_nalu failed\r\n", __FUNCTION__);
                    return -1;
                }
            }
        }
    }
    else if (HEVC_NAL_PPS == nalu)
    {
        if (0 == p_ctx->pps_len)
        {
            memcpy(p_ctx->pps, (uint8 *)p_data+4, len-4);
            p_ctx->pps_len = len-4;

            if (p_ctx->sps_len > 0 && p_ctx->pps_len > 0 && p_ctx->vps_len > 0)
            {
                if (mp4_write_h265_nalu(p_ctx) < 0)
                {
                    log_print(HT_LOG_ERR, "%s, mp4_write_h264_nalu failed\r\n", __FUNCTION__);
                    return -1;
                }
            }
        }
    }
    else if (HEVC_NAL_VPS == nalu)
    {
        if (0 == p_ctx->vps_len)
        {
            memcpy(p_ctx->vps, (uint8 *)p_data+4, len-4);
            p_ctx->vps_len = len-4;

            if (p_ctx->sps_len > 0 && p_ctx->pps_len > 0 && p_ctx->vps_len > 0)
            {
                if (mp4_write_h265_nalu(p_ctx) < 0)
                {
                    log_print(HT_LOG_ERR, "%s, mp4_write_h264_nalu failed\r\n", __FUNCTION__);
                    return -1;
                }
            }
        }
    }
    else if (HEVC_NAL_SEI_PREFIX == nalu || HEVC_NAL_SEI_SUFFIX == nalu)
    {
    }
    else if (p_ctx->ctxf_nalu)
    {
        if (!p_ctx->ctxf_iframe)
        {
            if (b_key)
            {
                if (mp4_write_video_frame(p_ctx, p_data, len, b_key) < 0)
                {
                    log_print(HT_LOG_ERR, "%s, mp4_write_video_frame failed\r\n", __FUNCTION__);
                    return -1;
                }
                else
                {
                    p_ctx->ctxf_iframe = 1;
                }
            }
        }
        else
        {
            if (mp4_write_video_frame(p_ctx, p_data, len, b_key) < 0)
            {
                log_print(HT_LOG_ERR, "%s, mp4_write_video_frame failed\r\n", __FUNCTION__);
                return -1;
            }
        }
    }

    return 0;
}

int mp4_write_video(MP4CTX * p_ctx, void * p_data, uint32 len, int b_key)
{
    int ret = 0;
    
    sys_os_mutex_enter(p_ctx->mutex);
    
    if (memcmp(p_ctx->v_fcc, "H264", 4) == 0)
    {
        ret = mp4_write_h264(p_ctx, p_data, len, b_key);
    }
    else if (memcmp(p_ctx->v_fcc, "H265", 4) == 0)
    {
        ret = mp4_write_h265(p_ctx, p_data, len, b_key);
    }
    else if (memcmp(p_ctx->v_fcc, "JPEG", 4) == 0)
    {
        ret = mp4_write_video_frame(p_ctx, p_data, len, b_key);
    }

    sys_os_mutex_leave(p_ctx->mutex);
    
    return ret;
}

int mp4_calc_fps(MP4CTX * p_ctx, uint8 * p_data, uint32 len)
{
    if (p_ctx->v_fps == 0 && p_ctx->i_frame_video >= 30)
    {
        float fps = (float) (p_ctx->i_frame_video * 1000.0) / (p_ctx->e_time - p_ctx->s_time);
		p_ctx->v_fps = (uint32)(fps + 0.5);

		if (p_ctx->v_fps > 0)
        {
            gf_isom_set_media_timescale(p_ctx->handler, p_ctx->v_track_id, p_ctx->v_fps, GF_TRUE);
        }

        log_print(HT_LOG_DBG, "%s, stime=%u, etime=%u, frames=%d, fps=%d\r\n", 
		    __FUNCTION__, p_ctx->s_time, p_ctx->e_time, p_ctx->i_frame_video, p_ctx->v_fps);
    }

    return 0;
}

int mp4_parse_video_size(MP4CTX * p_ctx, uint8 * p_data, uint32 len)
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
		nalu_t = p_data[4] & 0x1F;
		
		if (nalu_t != H264_NAL_SPS)
		{
			return 0;
        }
        
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

#endif // MP4_FORMAT



