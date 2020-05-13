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

#ifndef FORMAT_H
#define	FORMAT_H

#define PACKET_TYPE_UNKNOW  -1
#define PACKET_TYPE_VIDEO   0
#define PACKET_TYPE_AUDIO   1

#define AUDIO_FORMAT_PCM    (0x0001)
#define AUDIO_FORMAT_G726   (0x0064)    
#define AUDIO_FORMAT_G722   (0x0065)
#define AUDIO_FORMAT_MP3    (0x0055)
#define AUDIO_FORMAT_AAC    (0x00FF)
#define AUDIO_FORMAT_ALAW   (0x0006)
#define AUDIO_FORMAT_MULAW  (0x0007)

#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3) \
			((uint32)(uint8)(ch0) | ((uint32)(uint8)(ch1) << 8) | \
			((uint32)(uint8)(ch2) << 16) | ((uint32)(uint8)(ch3) << 24 ))
#define mmioFOURCC(ch0, ch1, ch2, ch3)  MAKEFOURCC(ch0, ch1, ch2, ch3)
#endif

#endif


