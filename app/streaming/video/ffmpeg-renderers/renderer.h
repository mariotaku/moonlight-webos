#pragma once

#include <SDL.h>

#include "streaming/video/decoder.h"
#include "streaming/video/overlaymanager.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

#ifdef HAVE_EGL
// SDL_egl.h have too many conflicts, we will do without
typedef void *EGLDisplay;
typedef void *EGLImage;
#define EGL_MAX_PLANES 4

class EGLExtensions {
public:
    EGLExtensions(EGLDisplay dpy);
    ~EGLExtensions() {}
    bool isSupported(const QString &extension) const;
private:
    const QStringList m_Extensions;
};

#endif

#define RENDERER_ATTRIBUTE_FULLSCREEN_ONLY 0x01
#define RENDERER_ATTRIBUTE_1080P_MAX 0x02

class IFFmpegRenderer : public Overlay::IOverlayRenderer {
public:
    virtual bool initialize(PDECODER_PARAMETERS params) = 0;
    virtual bool prepareDecoderContext(AVCodecContext* context, AVDictionary** options) = 0;
    virtual void renderFrame(AVFrame* frame) = 0;

    virtual bool needsTestFrame() {
        // No test frame required by default
        return false;
    }

    virtual int getDecoderCapabilities() {
        // No special capabilities by default
        return 0;
    }

    virtual int getRendererAttributes() {
        // No special attributes by default
        return 0;
    }

    virtual int getDecoderColorspace() {
        // Rec 601 is default
        return COLORSPACE_REC_601;
    }

    virtual bool isRenderThreadSupported() {
        // Render thread is supported by default
        return true;
    }

    virtual bool isDirectRenderingSupported() {
        // The renderer can render directly to the display
        return true;
    }

    virtual enum AVPixelFormat getPreferredPixelFormat(int videoFormat) {
        if (videoFormat == VIDEO_FORMAT_H265_MAIN10) {
            // 10-bit YUV 4:2:0
            return AV_PIX_FMT_P010;
        }
        else {
            // Planar YUV 4:2:0
            return AV_PIX_FMT_YUV420P;
        }
    }

    virtual bool isPixelFormatSupported(int videoFormat, enum AVPixelFormat pixelFormat) {
        // By default, we only support the preferred pixel format
        return getPreferredPixelFormat(videoFormat) == pixelFormat;
    }

    // IOverlayRenderer
    virtual void notifyOverlayUpdated(Overlay::OverlayType) override {
        // Nothing
    }

#ifdef HAVE_EGL
    // By default we can't do EGL
    virtual bool canExportEGL() {
        return false;
    }

    virtual bool initializeEGL(EGLDisplay,
                               const EGLExtensions &) {
        return false;
    }

    virtual ssize_t exportEGLImages(AVFrame *,
                                    EGLDisplay,
                                    EGLImage[EGL_MAX_PLANES]) {
        return -1;
    }

    // Free the ressources allocated during the last `exportEGLImages` call
    virtual void freeEGLImages(EGLDisplay, EGLImage[EGL_MAX_PLANES]) {}
#endif
};
