#include "webos.h"
#include "streaming/streamutils.h"

#include <QDebug>

WebOSVideoDecoder::WebOSVideoDecoder(bool testOnly)
    : m_Renderer(nullptr)
{
    qDebug() << "webOS Video decoder";
}

WebOSVideoDecoder::~WebOSVideoDecoder()
{

    if (m_Renderer != nullptr) {
        SDL_DestroyRenderer(m_Renderer);
    }
}

bool WebOSVideoDecoder::initialize(PDECODER_PARAMETERS params) 
{
    qDebug() << "WebOSVideoDecoder::initialize";
    Uint32 rendererFlags = SDL_RENDERER_ACCELERATED;

    if (params->videoFormat == VIDEO_FORMAT_H265_MAIN10) {
        // SDL doesn't support rendering YUV 10-bit textures yet
        return false;
    }

    if ((SDL_GetWindowFlags(params->window) & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN) {
        // In full-screen exclusive mode, we enable V-sync if requested. For other modes, Windows and Mac
        // have compositors that make rendering tear-free. Linux compositor varies by distro and user
        // configuration but doesn't seem feasible to detect here.
        if (params->enableVsync) {
            rendererFlags |= SDL_RENDERER_PRESENTVSYNC;
        }
    }

#ifdef Q_OS_WIN32
    // We render on a different thread than the main thread which is handling window
    // messages. Without D3DCREATE_MULTITHREADED, this can cause a deadlock by blocking
    // on a window message being processed while the main thread is blocked waiting for
    // the render thread to finish.
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DIRECT3D_THREADSAFE, "1", SDL_HINT_OVERRIDE);
#endif

    m_Renderer = SDL_CreateRenderer(params->window, -1, rendererFlags);
    if (!m_Renderer) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_CreateRenderer() failed: %s",
                     SDL_GetError());
        return false;
    }

    // SDL_CreateRenderer() can end up having to recreate our window (SDL_RecreateWindow())
    // to ensure it's compatible with the renderer's OpenGL context. If that happens, we
    // can get spurious SDL_WINDOWEVENT events that will cause us to (again) recreate our
    // renderer. This can lead to an infinite to renderer recreation, so discard all
    // SDL_WINDOWEVENT events after SDL_CreateRenderer().
    SDL_PumpEvents();
    SDL_FlushEvent(SDL_WINDOWEVENT);

    // Calculate the video region size, scaling to fill the output size while
    // preserving the aspect ratio of the video stream.
    SDL_Rect src, dst;
    src.x = src.y = 0;
    src.w = params->width;
    src.h = params->height;
    dst.x = dst.y = 0;
    SDL_GetRendererOutputSize(m_Renderer, &dst.w, &dst.h);
    StreamUtils::scaleSourceToDestinationSurface(&src, &dst);

    // Ensure the viewport is set to the desired video region
    SDL_RenderSetViewport(m_Renderer, &dst);

    // Draw a black frame until the video stream starts rendering
    SDL_SetRenderDrawColor(m_Renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(m_Renderer);
    SDL_RenderPresent(m_Renderer);
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
    return QSize(3840, 2160);
}

int WebOSVideoDecoder::submitDecodeUnit(PDECODE_UNIT du) 
{
    SDL_assert(!m_TestOnly);

    return DR_OK;
}

void WebOSVideoDecoder::renderFrameOnMainThread() 
{
    SDL_SetRenderDrawColor(m_Renderer, 255, 0, 0, 255);
    SDL_RenderClear(m_Renderer);
    SDL_RenderPresent(m_Renderer);
}