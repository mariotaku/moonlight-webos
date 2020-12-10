#pragma once

#include <functional>

#include "decoder.h"

extern "C" {
#include <gst/gst.h>
}

class WebOSVideoDecoder : public IVideoDecoder {
public:
    WebOSVideoDecoder(bool testOnly);
    virtual ~WebOSVideoDecoder() override;
    virtual bool initialize(PDECODER_PARAMETERS params) override;
    virtual bool isHardwareAccelerated() override;
    virtual bool isAlwaysFullScreen() override;
    virtual int getDecoderCapabilities() override;
    virtual int getDecoderColorspace() override;
    virtual QSize getDecoderMaxResolution() override;
    virtual int submitDecodeUnit(PDECODE_UNIT du) override;
    virtual void renderFrameOnMainThread() override;
private:

    static GstFlowReturn gstSinkNewPreroll(GstElement *sink, gpointer self);
    static GstFlowReturn gstSinkNewSample(GstElement *sink, gpointer self);
    
    GstElement* m_Source;
    GstElement* m_Sink;
    GstElement* m_Pipeline;

    SDL_Renderer* m_Renderer;
    
    bool m_NeedsSpsFixup;
    bool m_TestOnly;
};