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
#include "r2f_cfg.h"
#include "xml_node.h"
#include "r2f_rua.h"


/***********************************************************/

R2F_CFG g_r2f_cfg;

/***********************************************************/

STREAM2FILE * r2f_add_r2f(STREAM2FILE ** p_r2f)
{
	STREAM2FILE * p_tmp;
	STREAM2FILE * p_new_r2f = (STREAM2FILE *) malloc(sizeof(STREAM2FILE));
	if (NULL == p_new_r2f)
	{
		return NULL;
	}

	memset(p_new_r2f, 0, sizeof(STREAM2FILE));

	p_tmp = *p_r2f;
	if (NULL == p_tmp)
	{
		*p_r2f = p_new_r2f;
	}
	else
	{
		while (p_tmp && p_tmp->next)
		{
		    p_tmp = p_tmp->next;
        }
        
		p_tmp->next = p_new_r2f;
	}	

	return p_new_r2f;
}

void r2f_free_r2fs(STREAM2FILE ** p_r2f)
{
	STREAM2FILE * p_next;
	STREAM2FILE * p_tmp = *p_r2f;

	while (p_tmp)
	{
		p_next = p_tmp->next;		
		free(p_tmp);
		
		p_tmp = p_next;
	}

	*p_r2f = NULL;
}

/***********************************************************/
int r2f_to_filefmt(const char * fmt)
{
    if (strcasecmp(fmt, "avi") == 0)
    {
        return R2F_FMT_AVI;
    }
#ifdef MP4_FORMAT    
    else if (strcasecmp(fmt, "mp4") == 0)
    {
        return R2F_FMT_MP4;
    }
#endif

    return R2F_FMT_AVI;
}

BOOL r2f_parse_r2f(XMLN * p_node, STREAM2FILE * p_r2f)
{
    XMLN * p_url;	
	XMLN * p_user;
	XMLN * p_pass;
	XMLN * p_savepath;
	XMLN * p_filefmt;
	XMLN * p_framerate;
	XMLN * p_recordsize;
	XMLN * p_recordtime;

	p_url = xml_node_get(p_node, "url");
	if (p_url && p_url->data)
	{
		strncpy(p_r2f->url, p_url->data, sizeof(p_r2f->url)-1);
	}
	else
	{
		return FALSE;
	}

	p_user = xml_node_get(p_node, "user");
	if (p_user && p_user->data)
	{
		strncpy(p_r2f->user, p_user->data, sizeof(p_r2f->user)-1);
	}

	p_pass = xml_node_get(p_node, "pass");
	if (p_pass && p_pass->data)
	{
		strncpy(p_r2f->pass, p_pass->data, sizeof(p_r2f->pass)-1);
	}

	p_savepath = xml_node_get(p_node, "savepath");
	if (p_savepath && p_savepath->data)
	{
		strncpy(p_r2f->savepath, p_savepath->data, sizeof(p_r2f->savepath)-1);
	}

	p_filefmt = xml_node_get(p_node, "filefmt");
	if (p_filefmt && p_filefmt->data)
	{
		p_r2f->filefmt = r2f_to_filefmt(p_filefmt->data);
	}

	p_framerate = xml_node_get(p_node, "framerate");
	if (p_framerate && p_framerate->data)
	{
		p_r2f->framerate = atoi(p_framerate->data);
	}
	
	p_recordsize = xml_node_get(p_node, "recordsize");
	if (p_recordsize && p_recordsize->data)
	{
		p_r2f->recordsize = atoi(p_recordsize->data);
	}

	p_recordtime = xml_node_get(p_node, "recordtime");
	if (p_recordtime && p_recordtime->data)
	{
		p_r2f->recordtime = atoi(p_recordtime->data);
	}

	return TRUE;
}

BOOL r2f_parse_config(char * xml_buff, int rlen)
{
	XMLN * p_node;	
	XMLN * p_log_enable;
	XMLN * p_log_level;
	XMLN * p_stream2file;

	p_node = xxx_hxml_parse(xml_buff, rlen);
	if (NULL == p_node)
	{
		return FALSE;
	}
	
   	p_log_enable = xml_node_get(p_node, "log_enable");
	if (p_log_enable && p_log_enable->data)
	{
		g_r2f_cfg.log_enable = atoi(p_log_enable->data);
	}

	p_log_level = xml_node_get(p_node, "log_level");
	if (p_log_level && p_log_level->data)
	{
		g_r2f_cfg.log_level = atoi(p_log_level->data);
	}
	
	int cnt = 0;
	
	p_stream2file = xml_node_get(p_node, "stream2file");
	while (p_stream2file && stricmp(p_stream2file->name, "stream2file") == 0)
	{
		STREAM2FILE r2f;
		memset(&r2f, 0, sizeof(r2f));
		
		if (r2f_parse_r2f(p_stream2file, &r2f))
		{
			STREAM2FILE * p_info = r2f_add_r2f(&g_r2f_cfg.r2f);
			if (p_info)
			{
				cnt++;				
				memcpy(p_info, &r2f, sizeof(STREAM2FILE));
			}
		}

#ifdef DEMO
		if (cnt >= MAX_NUM_RUA)
		{
			break;
		}
#endif

		p_stream2file = p_stream2file->next;
	}

	xml_node_del(p_node);
	
	return TRUE;
}


BOOL r2f_read_config()
{
	BOOL ret = FALSE;
	int len, rlen;
	FILE * fp = NULL;
	char * xml_buff = NULL;

    // read config file
	
	fp = fopen("stream2file.cfg", "r");
	if (NULL == fp)
	{
	    log_print(HT_LOG_DBG, "%s, open file stream2file.cfg failed\r\n", __FUNCTION__);
		goto FAILED;
	}
	
	fseek(fp, 0, SEEK_END);
	
	len = ftell(fp);
	if (len <= 0)
	{
		goto FAILED;
	}
	fseek(fp, 0, SEEK_SET);
	
	xml_buff = (char *) malloc(len + 1);	
	if (NULL == xml_buff)
	{
		goto FAILED;
	}

	rlen = fread(xml_buff, 1, len, fp);
	if (rlen > 0)
	{
	    xml_buff[rlen] = '\0';
	    ret = r2f_parse_config(xml_buff, rlen);
	}
	else
	{
	    log_print(HT_LOG_ERR, "%s, rlen = %d, len = %d\r\n", __FUNCTION__, rlen, len);
	}

FAILED:

	if (fp)
	{
		fclose(fp);
	}

	if (xml_buff)
	{
		free(xml_buff);
	}

	return ret;
}



