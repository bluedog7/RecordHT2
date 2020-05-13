#! /bin/sh

CUR=$PWD

if [ ! -f $CUR/ffmpeg/lib/linux/libavcodec.so.58 ]; then 
ln -s $CUR/ffmpeg/lib/linux/libavcodec.so.58.18.100 $CUR/ffmpeg/lib/linux/libavcodec.so.58
fi

if [ ! -f $CUR/ffmpeg/lib/linux/libavcodec.so ]; then 
ln -s $CUR/ffmpeg/lib/linux/libavcodec.so.58.18.100 $CUR/ffmpeg/lib/linux/libavcodec.so
fi

if [ ! -f $CUR/ffmpeg/lib/linux/libavdevice.so.58 ]; then 
ln -s $CUR/ffmpeg/lib/linux/libavdevice.so.58.3.100 $CUR/ffmpeg/lib/linux/libavdevice.so.58
fi

if [ ! -f $CUR/ffmpeg/lib/linux/libavdevice.so ]; then
ln -s $CUR/ffmpeg/lib/linux/libavdevice.so.58.3.100 $CUR/ffmpeg/lib/linux/libavdevice.so
fi

if [ ! -f $CUR/ffmpeg/lib/linux/libavfilter.so.7 ]; then
ln -s $CUR/ffmpeg/lib/linux/libavfilter.so.7.16.100 $CUR/ffmpeg/lib/linux/libavfilter.so.7
fi

if [ ! -f $CUR/ffmpeg/lib/linux/libavfilter.so ]; then
ln -s $CUR/ffmpeg/lib/linux/libavfilter.so.7.16.100 $CUR/ffmpeg/lib/linux/libavfilter.so
fi

if [ ! -f $CUR/ffmpeg/lib/linux/libavformat.so.58 ]; then
ln -s $CUR/ffmpeg/lib/linux/libavformat.so.58.12.100 $CUR/ffmpeg/lib/linux/libavformat.so.58
fi

if [ ! -f $CUR/ffmpeg/lib/linux/libavformat.so ]; then
ln -s $CUR/ffmpeg/lib/linux/libavformat.so.58.12.100 $CUR/ffmpeg/lib/linux/libavformat.so
fi

if [ ! -f $CUR/ffmpeg/lib/linux/libavutil.so.56 ]; then
ln -s $CUR/ffmpeg/lib/linux/libavutil.so.56.14.100 $CUR/ffmpeg/lib/linux/libavutil.so.56
fi

if [ ! -f $CUR/ffmpeg/lib/linux/libavutil.so ]; then 
ln -s $CUR/ffmpeg/lib/linux/libavutil.so.56.14.100 $CUR/ffmpeg/lib/linux/libavutil.so
fi

if [ ! -f $CUR/ffmpeg/lib/linux/libpostproc.so.55 ]; then 
ln -s $CUR/ffmpeg/lib/linux/libpostproc.so.55.1.100 $CUR/ffmpeg/lib/linux/libpostproc.so.55
fi

if [ ! -f $CUR/ffmpeg/lib/linux/libpostproc.so ]; then 
ln -s $CUR/ffmpeg/lib/linux/libpostproc.so.55.1.100 $CUR/ffmpeg/lib/linux/libpostproc.so
fi

if [ ! -f $CUR/ffmpeg/lib/linux/libswresample.so.3 ]; then 
ln -s $CUR/ffmpeg/lib/linux/libswresample.so.3.1.100 $CUR/ffmpeg/lib/linux/libswresample.so.3
fi

if [ ! -f $CUR/ffmpeg/lib/linux/libswresample.so ]; then 
ln -s $CUR/ffmpeg/lib/linux/libswresample.so.3.1.100 $CUR/ffmpeg/lib/linux/libswresample.so
fi

if [ ! -f $CUR/ffmpeg/lib/linux/libswscale.so.5 ]; then 
ln -s $CUR/ffmpeg/lib/linux/libswscale.so.5.1.100 $CUR/ffmpeg/lib/linux/libswscale.so.5
fi

if [ ! -f $CUR/ffmpeg/lib/linux/libswscale.so ]; then 
ln -s $CUR/ffmpeg/lib/linux/libswscale.so.5.1.100 $CUR/ffmpeg/lib/linux/libswscale.so
fi

if [ ! -f $CUR/ffmpeg/lib/linux/libopus.so.0 ]; then 
ln -s $CUR/ffmpeg/lib/linux/libopus.so.0.6.0 $CUR/ffmpeg/lib/linux/libopus.so.0
fi

if [ ! -f $CUR/ffmpeg/lib/linux/libopus.so ]; then 
ln -s $CUR/ffmpeg/lib/linux/libopus.so.0.6.0 $CUR/ffmpeg/lib/linux/libopus.so
fi

if [ ! -f $CUR/ffmpeg/lib/linux/libx264.so ]; then 
ln -s $CUR/ffmpeg/lib/linux/libx264.so.157 $CUR/ffmpeg/lib/linux/libx264.so
fi

if [ ! -f $CUR/ffmpeg/lib/linux/libx265.so ]; then 
ln -s $CUR/ffmpeg/lib/linux/libx265.so.116 $CUR/ffmpeg/lib/linux/libx265.so
fi

if [ ! -f $CUR/openssl/lib/linux/libssl.so ]; then 
ln -s $CUR/openssl/lib/linux/libssl.so.1.1 $CUR/openssl/lib/linux/libssl.so
fi

if [ ! -f $CUR/openssl/lib/linux/libcrypto.so ]; then 
ln -s $CUR/openssl/lib/linux/libcrypto.so.1.1 $CUR/openssl/lib/linux/libcrypto.so
fi

if [ ! -f $CUR/zlib/lib/linux/libz.so.1 ]; then 
ln -s $CUR/zlib/lib/linux/libz.so.1.2.11 $CUR/zlib/lib/linux/libz.so.1
fi

if [ ! -f $CUR/zlib/lib/linux/libz.so ]; then 
ln -s $CUR/zlib/lib/linux/libz.so.1.2.11 $CUR/zlib/lib/linux/libz.so
fi

if [ ! -f $CUR/gpac/lib/linux/libgpac.so.7 ]; then 
ln -s $CUR/gpac/lib/linux/libgpac.so.7.8.0 $CUR/gpac/lib/linux/libgpac.so.7
fi

if [ ! -f $CUR/gpac/lib/linux/libgpac.so ]; then 
ln -s $CUR/gpac/lib/linux/libgpac.so.7.8.0 $CUR/gpac/lib/linux/libgpac.so
fi

echo "make symbolic link finish!"
