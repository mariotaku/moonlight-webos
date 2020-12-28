### Build FFMPEG

```
./configure --disable-everything \
--enable-cross-compile --target-os=linux --arch=arm --cross-prefix=arm-webos-linux-gnueabi- \
--pkg-config=pkg-config --prefix=$HOME/Projects/moonlight-webos/libs/webos \
--sysroot=/opt/webos-sdk-x86_64/1.0.g/sysroots/armv7a-neon-webos-linux-gnueabi \
--enable-decoder=h264,hevc \
--disable-avdevice --disable-avformat --disable-avfilter \
--disable-swscale --disable-swresample \
--disable-ffmpeg --disable-ffnvcodec --disable-ffplay --disable-ffprobe --disable-doc
```