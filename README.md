### 
Deprecated. Further developent will start in another repo soon.

### Build IPK

1. To start, you'll need to have webOS SDK, and NDK installed. Instead of bloated version from LG Developers, 
you can check out [SDK from webOS OSE](https://www.webosose.org/docs/tools/sdk/sdk-download/).
Environment variable `WEBOS_CLI_TV` should also be set.
For NDK, please check out [this tutorial](https://github.com/webosbrew/meta-lg-webos-ndk).
2. Check out this repo, follow standard CMake build procedure to configure the project.
3. Use `make webos-package` to make IPK package.


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
