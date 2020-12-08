#pragma once

#include <functional>

#include "decoder.h"

#define HAVE_WEBOS

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
    bool m_TestOnly;
};