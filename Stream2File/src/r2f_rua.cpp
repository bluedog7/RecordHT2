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
#include "r2f_rua.h"


/***********************************************************************/

PPSN_CTX *		rua_fl;             // rua free list
PPSN_CTX *		rua_ul;             // rua used list


/***********************************************************************/
void rua_proxy_init()
{
	rua_fl = pps_ctx_fl_init(MAX_NUM_RUA, sizeof(RUA), TRUE);
	rua_ul = pps_ctx_ul_init(rua_fl, TRUE);
}

void rua_proxy_deinit()
{
	if (rua_ul)
	{
	    pps_ul_free(rua_ul);
	    rua_ul = NULL;
	}

	if (rua_fl)
	{
	    pps_fl_free(rua_fl);
	    rua_fl = NULL;
	}
}

RUA * rua_get_idle()
{
	RUA * p_rua = (RUA *)pps_fl_pop(rua_fl);
	if (p_rua)
	{
		memset(p_rua, 0, sizeof(RUA));
	}
	else
	{
		log_print(HT_LOG_ERR, "%s, don't have idle rua!!!\r\n", __FUNCTION__);
	}

	return p_rua;
}

void rua_set_online(RUA * p_rua)
{
	pps_ctx_ul_add(rua_ul, p_rua);
	p_rua->used_flag = 1;
}

void rua_set_idle(RUA * p_rua)
{
	pps_ctx_ul_del(rua_ul, p_rua);

	memset(p_rua, 0, sizeof(RUA));
	
	pps_fl_push(rua_fl, p_rua);
}

RUA * rua_lookup_start()
{
	return (RUA *)pps_lookup_start(rua_ul);
}

RUA * rua_lookup_next(RUA * p_rua)
{
	return (RUA *)pps_lookup_next(rua_ul, p_rua);
}

void rua_lookup_stop()
{
	pps_lookup_end(rua_ul);
}

uint32 rua_get_index(RUA * p_rua)
{
	return pps_get_index(rua_fl, p_rua);
}

RUA * rua_get_by_index(uint32 index)
{
	return (RUA *)pps_get_node_by_index(rua_fl, index);
}



