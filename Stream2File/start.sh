#! /bin/sh

CUR=$PWD
export LD_LIBRARY_PATH=$CUR/ffmpeg/lib/linux:$CUR/zlib/lib/linux:$CUR/openssl/lib/linux:$CUR/gpac/lib/linux
./stream2file