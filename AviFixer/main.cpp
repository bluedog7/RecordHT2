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
#include "avi_read.h"
#include "avi_write.h"


void print_help()
{
    printf("avifixer options <filename>\r\n");
    printf("-h print this help\r\n");
    printf("-r remove the input file\r\n");
    printf("-o <filename> the fixed file save path\r\n");
}

char * get_externname(char * path)
{
    int len = strlen(path);

    for (int i = len - 1; i >= 0; i--)
    {
        if (path[i] == '.')
        {
            if (i < len - 1)
            {
                return path+i+1;
            }
        }
    }

    return NULL;
}

char * get_filename(char * path)
{
    int len = strlen(path);
    static char filename[256];

    for (int i = len - 1; i >= 0; i--)
    {
        if (path[i] == '.')
        {
            if (i > 0)
            {
                strncpy(filename, path, i);
                return filename;
            }
        }
    }

    return NULL;
}

char * get_filepath(char * path)
{
    int len = strlen(path);
    static char filepath[256];

    for (int i = len - 1; i >= 0; i--)
    {
#if __WINDOWS_OS__
        if (path[i] == '\\')
#else
        if (path[i] == '/')
#endif
        {
            if (i > 0)
            {
                strncpy(filepath, path, i);
                return filepath;
            }
        }
    }

    return NULL;
}

int main(int argc, char * argv[])
{
    if (argc < 2)
    {
        print_help();        
        return -1;
    }

    int ridx_flag = 0;
    char input_path[256] = {'\0'};
    char output_path[256] = {'\0'};
    
    for (int i = 1; i < argc; i++)
    {
        if (strcasecmp(argv[i], "-h") == 0)
        {
            print_help();
            return 0;        
		}
        else if (strcasecmp(argv[i], "-r") == 0)
        {
            ridx_flag = 1;
        }
        else if (strcasecmp(argv[i], "-o") == 0)
        {
            if (i++ >= argc)
            {
                print_help();
                return -1;
            }

            strncpy(output_path, argv[i], sizeof(output_path)-1);
        }
        else 
        {
            strncpy(input_path, argv[i], sizeof(input_path)-1);
        }
    }

    if (input_path[0] == '\0')
    {
        print_help();
        return 0;
    }

    char filename[256] = {'\0'};
    char externname[32] = {'\0'};

    strncpy(externname, get_externname(input_path), sizeof(externname)-1);
    strncpy(filename, get_filename(input_path), sizeof(filename)-1);

    if (strcasecmp(externname, "avi"))
    {
        printf("%s is not avi file\r\n", input_path);
        print_help();
        return -1;
    }

    AVICTX * p_src = avi_read_open(input_path);
    if (NULL == p_src)
    {
        printf("avi_read_open (%s) failed\r\n", input_path);
        return -1;
    }

    if (output_path[0] == '\0')
    {
        sprintf(output_path, "%s_fixed.%s", filename, externname);
    }

#if __WINDOWS_OS__
    if (NULL == strchr(output_path, '\\')) // the output path not include full path
#else
    if (NULL == strchr(output_path, '/'))  // the output path not include full path
#endif
    {
        if (get_filepath(input_path))
        {
            char fullpath[256];
#if __WINDOWS_OS__            
            sprintf(fullpath, "%s\\%s", get_filepath(input_path), output_path);
#else
            sprintf(fullpath, "%s/%s", get_filepath(input_path), output_path);
#endif
            strcpy(output_path, fullpath);
        }
    }

    AVICTX * p_dst = avi_write_open(output_path);
    if (NULL == p_dst)
    {        
		printf("avi_write_open (%s) failed\r\n", output_path);
        return -1;
    }

    p_dst->ctxf_audio = p_src->ctxf_audio;
	p_dst->ctxf_video = p_src->ctxf_video;
	
	memcpy(p_dst->v_fcc, p_src->v_fcc, 4);
	p_dst->v_fps = p_src->v_fps ? p_src->v_fps : 25;
	p_dst->v_width = p_src->v_width;
	p_dst->v_height = p_src->v_height;
	p_dst->ctxf_sps_f = 1;

	p_dst->a_rate = p_src->a_rate;
	p_dst->a_fmt = p_src->a_fmt;
	p_dst->a_chns = p_src->a_chns;
	
	avi_write_header(p_dst);

	AVIPKT pkt;
	memset(&pkt, 0, sizeof(pkt));
	
	while (1)
	{
		if (avi_read_pkt(p_src, &pkt) > 0)
		{
		    int keyflag = 1;
			
			if (pkt.type == PACKET_TYPE_VIDEO)      // video
			{
			    avi_write_video(p_dst, pkt.dbuf, pkt.len, keyflag);
			}
			else if (pkt.type == PACKET_TYPE_VIDEO) // audio
			{
			    avi_write_audio(p_dst, pkt.dbuf, pkt.len);
			}
		}
		else
		{
			break;
		}	
	}

    if (pkt.rbuf)
    {
        free(pkt.rbuf);
    }

    p_dst->s_time = p_src->e_time = 0;
    
	avi_write_close(p_dst);
	avi_read_close(p_src);

	if (ridx_flag)
	{
#if __WINDOWS_OS__	
	    remove(input_path);
	    
	    char indexname[256];
        sprintf(indexname, "%s.idx", input_path);
        remove(indexname);
#else
		char cmds[256];

		sprintf(cmds, "rm -f %s", input_path);
		system(cmds);
		
		sprintf(cmds, "rm -f %s.idx", input_path);
		system(cmds);
#endif
	}

	printf("%s fixed, output path %s\r\n", input_path, output_path);
    
	return 0;
}




