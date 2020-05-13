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

#ifndef	PPSTACK_H
#define PPSTACK_H


/***************************************************************************************/

typedef struct PPSN	//ppstack_node
{
	unsigned long   prev_node;
	unsigned long   next_node;
	unsigned long   node_flag;	//0:idle£»1:in FreeList 2:in UsedList 
} PPSN;

typedef struct PPSN_CTX
{
	char *          fl_base;	
	uint32		    head_node;	
	uint32		    tail_node;
	uint32		    node_num;
	uint32		    low_offset;
	uint32		    high_offset;
	uint32		    unit_size;
	void *          ctx_mutex;
	uint32		    pop_cnt;
	uint32		    push_cnt;
} PPSN_CTX;

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************************/

HT_API PPSN_CTX   * pps_ctx_fl_init(unsigned long node_num, unsigned long content_size, BOOL bNeedMutex);
HT_API PPSN_CTX   * pps_ctx_fl_init_assign(char *  mem_addr, unsigned long mem_len, unsigned long node_num, unsigned long content_size, BOOL bNeedMutex);

HT_API void         pps_fl_free(PPSN_CTX * fl_ctx);
HT_API void         pps_fl_reinit(PPSN_CTX * fl_ctx);

HT_API BOOL         pps_fl_push(PPSN_CTX * pps_ctx, void * content_ptr);
HT_API BOOL         pps_fl_push_tail(PPSN_CTX * pps_ctx, void * content_ptr);
HT_API void       * pps_fl_pop(PPSN_CTX * pps_ctx);

HT_API PPSN_CTX   * pps_ctx_ul_init(PPSN_CTX * fl_ctx, BOOL bNeedMutex);
HT_API BOOL         pps_ctx_ul_init_assign(PPSN_CTX * ul_ctx, PPSN_CTX * fl_ctx, BOOL bNeedMutex);
HT_API BOOL         pps_ctx_ul_init_nm(PPSN_CTX * fl_ctx, PPSN_CTX * ul_ctx);

HT_API void         pps_ul_reinit(PPSN_CTX * ul_ctx);
HT_API void         pps_ul_free(PPSN_CTX * ul_ctx);

HT_API BOOL         pps_ctx_ul_del(PPSN_CTX * ul_ctx, void * content_ptr);
HT_API PPSN       * pps_ctx_ul_del_node_unlock(PPSN_CTX * ul_ctx, PPSN * p_node);
HT_API void       * pps_ctx_ul_del_unlock(PPSN_CTX * ul_ctx, void * content_ptr);

HT_API BOOL         pps_ctx_ul_add(PPSN_CTX * ul_ctx, void * content_ptr);
HT_API BOOL         pps_ctx_ul_add_head(PPSN_CTX * ul_ctx, void * content_ptr);

HT_API uint32       pps_get_index(PPSN_CTX * pps_ctx, void * content_ptr);
HT_API void       * pps_get_node_by_index(PPSN_CTX * pps_ctx, unsigned long index);

/***************************************************************************************/
HT_API void       * pps_lookup_start(PPSN_CTX * pps_ctx);
HT_API void       * pps_lookup_next(PPSN_CTX * pps_ctx, void * ct_ptr);
HT_API void         pps_lookup_end(PPSN_CTX * pps_ctx);

HT_API void       * pps_lookback_start(PPSN_CTX * pps_ctx);
HT_API void       * pps_lookback_next(PPSN_CTX * pps_ctx, void * ct_ptr);
HT_API void         pps_lookback_end(PPSN_CTX * pps_ctx);

HT_API void         pps_wait_mutex(PPSN_CTX * pps_ctx);
HT_API void         pps_post_mutex(PPSN_CTX * pps_ctx);

HT_API BOOL         pps_safe_node(PPSN_CTX * pps_ctx, void * content_ptr);
HT_API BOOL         pps_idle_node(PPSN_CTX * pps_ctx, void * content_ptr);
HT_API BOOL         pps_exist_node(PPSN_CTX * pps_ctx, void * content_ptr);
HT_API BOOL         pps_used_node(PPSN_CTX * pps_ctx, void * content_ptr);

/***************************************************************************************/
HT_API int          pps_node_count(PPSN_CTX * pps_ctx);
HT_API void       * pps_get_head_node(PPSN_CTX * pps_ctx);
HT_API void       * pps_get_tail_node(PPSN_CTX * pps_ctx);
HT_API void       * pps_get_next_node(PPSN_CTX * pps_ctx, void * content_ptr);
HT_API void       * pps_get_prev_node(PPSN_CTX * pps_ctx, void * content_ptr);

#ifdef __cplusplus
}
#endif

#endif // PPSTACK_H


