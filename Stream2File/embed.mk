################OPTION###################
OUTPUT = stream2file
CCOMPILE = gcc
CPPCOMPILE = g++
COMPILEOPTION += -c -O3 -fPIC
COMPILEOPTION += -DMP4_FORMAT
COMPILEOPTION += -DRTMP_STREAM
COMPILEOPTION += -DMJPEG_STREAM

ifneq ($(findstring MP4_FORMAT, $(COMPILEOPTION)),)
COMPILEOPTION += -DGPAC_HAVE_CONFIG_H
endif

LINK = g++
LINKOPTION += -o $(OUTPUT)
INCLUDEDIR += -I.
INCLUDEDIR += -I./bm
INCLUDEDIR += -I./rtp
INCLUDEDIR += -I./rtsp
INCLUDEDIR += -I./http
INCLUDEDIR += -I./media
INCLUDEDIR += -I./src
INCLUDEDIR += -I./rtmp
INCLUDEDIR += -I./librtmp
INCLUDEDIR += -I./gpac/include
INCLUDEDIR += -I./zlib/include
INCLUDEDIR += -I./openssl/include
INCLUDEDIR += -I./ffmpeg/include
LIBDIRS += -L./gpac/lib/linux
LIBDIRS += -L./zlib/lib/linux
LIBDIRS += -L./openssl/lib/linux
LIBDIRS += -L./ffmpeg/lib/linux
OBJS += bm/word_analyse.o
OBJS += bm/util.o
OBJS += bm/sys_log.o
OBJS += bm/sys_buf.o
OBJS += bm/linked_list.o
OBJS += bm/rfc_md5.o
OBJS += bm/ppstack.o
OBJS += bm/hqueue.o
OBJS += bm/hxml.o
OBJS += bm/xml_node.o
OBJS += bm/sys_os.o
OBJS += bm/base64.o
OBJS += rtp/aac_rtp_rx.o
OBJS += rtp/bit_vector.o
OBJS += rtp/h264_rtp_rx.o
OBJS += rtp/h265_rtp_rx.o
OBJS += rtp/mjpeg_rtp_rx.o
OBJS += rtp/mjpeg_tables.o
OBJS += rtp/mpeg4.o
OBJS += rtp/mpeg4_rtp_rx.o
OBJS += rtp/pcm_rtp_rx.o
OBJS += rtp/rtp_rx.o
OBJS += rtp/h264_util.o
OBJS += rtp/h265_util.o
OBJS += rtp/media_util.o
OBJS += rtsp/rtsp_cln.o
OBJS += rtsp/rtsp_parse.o
OBJS += rtsp/rtsp_rcua.o
OBJS += rtsp/rtsp_util.o
OBJS += src/avi_write.o
OBJS += src/r2f.o
OBJS += src/r2f_cfg.o
OBJS += src/r2f_rua.o
OBJS += main.o

ifneq ($(findstring OVER_HTTP, $(COMPILEOPTION)),)
OBJS += http/http_cln.o
OBJS += http/http_parse.o
endif

ifneq ($(findstring BACKCHANNEL, $(COMPILEOPTION)),)
OBJS += rtsp/rtsp_backchannel.o
endif

ifneq ($(findstring MP4_FORMAT, $(COMPILEOPTION)),)
OBJS += src/mp4_write.o
endif

ifneq ($(findstring RTMP_STREAM, $(COMPILEOPTION)),)
OBJS += rtmp/rtmp_cln.o
OBJS += librtmp/amf.o
OBJS += librtmp/hashswf.o
OBJS += librtmp/log.o
OBJS += librtmp/parseurl.o
OBJS += librtmp/rtmp.o
endif

ifneq ($(findstring MJPEG_STREAM, $(COMPILEOPTION)),)
OBJS += http/http_cln.o
OBJS += http/http_mjpeg_cln.o
OBJS += http/http_parse.o
endif

ffmpeg := 0

ifneq ($(findstring AUDIO_CONV, $(COMPILEOPTION)),)
ffmpeg := 1
OBJS += media/audio_decoder.o
OBJS += media/audio_encoder.o
OBJS += media/avcodec_mutex.o
endif

ifneq ($(findstring VIDEO_CONV, $(COMPILEOPTION)),)
ffmpeg := 1
OBJS += media/video_decoder.o
OBJS += media/video_encoder.o
ifeq ($(findstring AUDIO_CONV, $(COMPILEOPTION)),)
OBJS += media/avcodec_mutex.o
endif
endif

SHAREDLIB += -lpthread

ifneq ($(findstring MP4_FORMAT, $(COMPILEOPTION)),)
SHAREDLIB += -lgpac
endif

ifneq ($(findstring RTMP_STREAM, $(COMPILEOPTION)),)
SHAREDLIB += -lssl
SHAREDLIB += -lcrypto
SHAREDLIB += -lz
endif

ifeq ($(ffmpeg), 1)
SHAREDLIB += -lavformat
SHAREDLIB += -lavcodec
SHAREDLIB += -lswresample
SHAREDLIB += -lavutil
SHAREDLIB += -lswscale
SHAREDLIB += -lx264
SHAREDLIB += -lx265
SHAREDLIB += -lopus
endif

APPENDLIB = 
PROC_OPTION = DEFINE=_PROC_ MODE=ORACLE LINES=true CODE=CPP
ESQL_OPTION = -g
################OPTION END################
ESQL = esql
PROC = proc
$(OUTPUT):$(OBJS) $(APPENDLIB)
	./mklinks.sh
	$(LINK) $(LINKOPTION) $(LIBDIRS)   $(OBJS) $(SHAREDLIB) $(APPENDLIB) 

clean: 
	rm -f $(OBJS)
	rm -f $(OUTPUT)
all: clean $(OUTPUT)
.PRECIOUS:%.cpp %.c %.C
.SUFFIXES:
.SUFFIXES:  .c .o .cpp .ecpp .pc .ec .C .cc .cxx

.cpp.o:
	$(CPPCOMPILE) -c -o $*.o $(COMPILEOPTION) $(INCLUDEDIR)  $*.cpp
	
.cc.o:
	$(CCOMPILE) -c -o $*.o $(COMPILEOPTION) $(INCLUDEDIR)  $*.cpp

.cxx.o:
	$(CPPCOMPILE) -c -o $*.o $(COMPILEOPTION) $(INCLUDEDIR)  $*.cpp

.c.o:
	$(CCOMPILE) -c -o $*.o $(COMPILEOPTION) $(INCLUDEDIR) $*.c

.C.o:
	$(CPPCOMPILE) -c -o $*.o $(COMPILEOPTION) $(INCLUDEDIR) $*.C	

.ecpp.C:
	$(ESQL) -e $(ESQL_OPTION) $(INCLUDEDIR) $*.ecpp 
	
.ec.c:
	$(ESQL) -e $(ESQL_OPTION) $(INCLUDEDIR) $*.ec
	
.pc.cpp:
	$(PROC)  CPP_SUFFIX=cpp $(PROC_OPTION)  $*.pc
