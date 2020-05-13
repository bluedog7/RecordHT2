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


void print_help()
{
    printf("aviread <filename>\r\n");
    printf("-h print this help\r\n");
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

int main(int argc, char * argv[])
{
    if (argc < 2)
    {
        print_help();        
        return -1;
    }

    if (strcasecmp(argv[1], "-h") == 0)
    {
        print_help();
        return 0;        
    }

    char filename[256] = {'\0'};

    strncpy(filename, argv[1], sizeof(filename)-1);
    
    if (filename[0] == '\0')
    {
        print_help();
        return 0;
    }

    char extname[32] = {'\0'};

    strncpy(extname, get_externname(filename), sizeof(extname)-1);

    if (strcasecmp(extname, "avi"))
    {
        printf("%s is not avi file\r\n", filename);
        print_help();
        return -1;
    }
    
    AVICTX * p_ctx = avi_read_open(filename);
    if (NULL == p_ctx)
    {
        printf("avi_read_open (%s) failed\r\n", filename);
        return -1;
    }

    printf("filename : %s\r\n", filename);
    printf("have video : %d\r\n", p_ctx->ctxf_video);
    
    if (p_ctx->ctxf_video)
    {
        char codec[8] = {'\0'};
        memcpy(codec, p_ctx->v_fcc, 4);
        
        printf("video codec : %s\r\n", codec);
        printf("video width : %d\r\n", p_ctx->v_width);
        printf("video height : %d\r\n", p_ctx->v_height);
        printf("video fps : %d\r\n", p_ctx->v_fps);
        printf("video total frame numbers : %d\r\n", p_ctx->i_frame_video);
    }
    
    printf("have audio : %d\r\n", p_ctx->ctxf_audio);

    if (p_ctx->ctxf_audio)
    {
        char codec[8] = {'\0'};

        switch(p_ctx->a_fmt)
        {
        case AUDIO_FORMAT_PCM:
            strcpy(codec, "PCM"); 
            break;
        case AUDIO_FORMAT_ALAW : 
            strcpy(codec, "G711A"); 
            break;
        case AUDIO_FORMAT_MULAW : 
            strcpy(codec, "G711U"); 
            break;
        case AUDIO_FORMAT_G726: 
            strcpy(codec, "G726"); 
            break;
        case AUDIO_FORMAT_G722: 
            strcpy(codec, "G722"); 
            break;
		case AUDIO_FORMAT_MP3: 
		    strcpy(codec, "MP3");
		    break;
		case AUDIO_FORMAT_AAC: 
		    strcpy(codec, "AAC"); 
		    break;
		default: 
		    strcpy(codec, "Unknow"); 
		    break;
        }
        
        printf("audio codec : %s\r\n", codec);
        printf("audio sample rate : %d\r\n", p_ctx->a_rate);
        printf("audio channels : %d\r\n", p_ctx->a_chns);
        printf("audio total frame numbers : %d\r\n", p_ctx->i_frame_audio);
    }

    AVIPKT pkt;
	memset(&pkt, 0, sizeof(pkt));

	int v_idx = 0, a_idx = 0;
	
	while (1)
	{
		if (avi_read_pkt(p_ctx, &pkt) > 0)
		{
			if (pkt.type == PACKET_TYPE_VIDEO)      // video
			{
			    printf("get video packet, idx : %d, len : %d\r\n", ++v_idx, pkt.len);
			}
			else if (pkt.type == PACKET_TYPE_AUDIO) // audio
			{
			    printf("get audio packet, idx : %d, len : %d\r\n", ++a_idx, pkt.len);
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
    
	avi_read_close(p_ctx);
	
    return 0;
}




