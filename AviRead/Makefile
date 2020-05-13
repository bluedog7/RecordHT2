################OPTION###################
OUTPUT = aviread
CCOMPILE = gcc
CPPCOMPILE = g++
COMPILEOPTION += -c -O3 -fPIC
LINK = g++
LINKOPTION = -o $(OUTPUT)
INCLUDEDIR += -I.
INCLUDEDIR += -I../Stream2File/bm
INCLUDEDIR += -I../Stream2File/rtp
INCLUDEDIR += -I../Stream2File/rtsp
INCLUDEDIR += -I../Stream2File/src
LIBDIRS = 
OBJS += ../Stream2File/bm/sys_log.o
OBJS += ../Stream2File/bm/sys_os.o
OBJS += ../Stream2File/src/avi_read.o
OBJS += main.o
SHAREDLIB = -lpthread
APPENDLIB = 
PROC_OPTION = DEFINE=_PROC_ MODE=ORACLE LINES=true CODE=CPP
ESQL_OPTION = -g
################OPTION END################
ESQL = esql
PROC = proc
$(OUTPUT):$(OBJS) $(APPENDLIB)
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
