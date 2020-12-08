#include "webos.h"

#include <QDebug>

WebOSVideoDecoder::WebOSVideoDecoder(bool testOnly)
{
    qDebug() << "webOS Video decoder";
}

WebOSVideoDecoder::~WebOSVideoDecoder()
{

}

bool WebOSVideoDecoder::initialize(PDECODER_PARAMETERS params) 
{
    qDebug() << "WebOSVideoDecoder::initialize";
    return true;
}

bool WebOSVideoDecoder::isHardwareAccelerated() 
{
    return false;
}

bool WebOSVideoDecoder::isAlwaysFullScreen() 
{
    return true;
}

int WebOSVideoDecoder::getDecoderCapabilities() 
{
    int capabilities = 0;

    if (!isHardwareAccelerated()) {
        // Slice up to 4 times for parallel CPU decoding, once slice per core
        int slices = qMin(MAX_SLICES, SDL_GetCPUCount());
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Encoder configured for %d slices per frame",
                    slices);
        capabilities |= CAPABILITY_SLICES_PER_FRAME(slices);
    }

    return capabilities;
}

int WebOSVideoDecoder::getDecoderColorspace() 
{
    return COLORSPACE_REC_601;
}

QSize WebOSVideoDecoder::getDecoderMaxResolution() 
{
    return QSize(1920, 1080);
}

int WebOSVideoDecoder::submitDecodeUnit(PDECODE_UNIT du) 
{
    SDL_assert(!m_TestOnly);

    return DR_OK;
}

void WebOSVideoDecoder::renderFrameOnMainThread() 
{

}