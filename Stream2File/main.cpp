/***************************************************************************************
 *
 *  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
 *
 *  By downloading, copying, installing or using the software you agree to this license.
 *  If you do not agree to this license, do not download, install, 
 *  copy or use the software.
 *
 *  Copyright (C) 2014-2019, Happytimesoft Corporation, all rights reserved.
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
#include "rtsp_parse.h"
#include "r2f.h"


#if __LINUX_OS__

void sig_handler(int sig)
{
    log_print(HT_LOG_DBG, "%s, sig=%d\r\n", __FUNCTION__, sig);

    r2f_stop();

    exit(0);
}

#elif __WINDOWS_OS__

BOOL WINAPI sig_handler(DWORD dwCtrlType)
{
    switch (dwCtrlType)
    {
    case CTRL_C_EVENT:      // Ctrl+C
        log_print(HT_LOG_DBG, "%s, CTRL+C\r\n", __FUNCTION__);
        break;
        
    case CTRL_BREAK_EVENT: // Ctrl+Break
        log_print(HT_LOG_DBG, "%s, CTRL+Break\r\n", __FUNCTION__);
        break;
        
    case CTRL_CLOSE_EVENT: // Closing the consolewindow
        log_print(HT_LOG_DBG, "%s, Closing\r\n", __FUNCTION__);
        break;
    }

    r2f_stop();
    
    // Return TRUE if handled this message,further handler functions won't be called.
    // Return FALSE to pass this message to further handlers until default handler calls ExitProcess().
    
     return FALSE;
}

#endif

#if defined(VIDEO_CONV) || defined(AUDIO_CONV)

static void av_log_callback(void* ptr, int level, const char* fmt, va_list vl)   
{
    int htlv = HT_LOG_INFO;
	char buff[4096];

	if (AV_LOG_QUIET == level)
	{
	    return;
	}
	
	vsnprintf(buff, sizeof(buff), fmt, vl);

    if (AV_LOG_TRACE == level || AV_LOG_VERBOSE == level)
    {
        htlv = HT_LOG_TRC;
    }
	else if (AV_LOG_DEBUG == level)
	{
	    htlv = HT_LOG_DBG;
	}
	else if (AV_LOG_INFO == level)
    {
        htlv = HT_LOG_INFO;
    }
    else if (AV_LOG_WARNING == level)
    {
        htlv = HT_LOG_WARN;
    }
    else if (AV_LOG_ERROR == level)
    {
        htlv = HT_LOG_ERR;
    }
    else if (AV_LOG_FATAL == level || AV_LOG_PANIC == level)
    {
        htlv = HT_LOG_FATAL;
    }
	
	log_print(htlv, "%s", buff);
}

#endif

int main(int argc, char * argv[])
{
    char lfname[256];
    network_init();
    if (argc < 2) return -1;
    sprintf(lfname, ".\\LOG\\ibstrecord_%s.log", argv[1]);

#if __LINUX_OS__

#if !defined(DEBUG) && !defined(_DEBUG)
    daemon_init();
#endif

    // Ignore broken pipes
    signal(SIGPIPE, SIG_IGN);
    
#endif

#if __LINUX_OS__
    signal(SIGINT, sig_handler);
    signal(SIGKILL, sig_handler);
    signal(SIGTERM, sig_handler);    
#elif __WINDOWS_OS__
    SetConsoleCtrlHandler(sig_handler, TRUE);
#endif

#if defined(VIDEO_CONV) || defined(AUDIO_CONV)
#if defined(DEBUG) || defined(_DEBUG)
    av_log_set_callback(av_log_callback);
#endif
    av_log_set_level(AV_LOG_QUIET);
#endif

    log_init(lfname);
    log_set_level(HT_LOG_DBG);
		
    r2f_start(argv[1]);

	
/*	for (;;)
	{
#if defined(DEBUG) || defined(_DEBUG)		
	    if (getchar() == 'q')
	    {
	        r2f_stop();
	        break;
	    }
#endif 

		sleep(5);
	} */
	
	return 0;
}


