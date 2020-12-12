#include "webos.h"
#include "streaming/streamutils.h"

#include <QDebug>

extern "C" {
#include <gst/app/gstappsrc.h>
#include "webos/lxvideo.h"
}
#include <fstream>
#define MAX_SPS_EXTRA_SIZE 16

WebOSVideoDecoder::WebOSVideoDecoder(bool testOnly)
    : m_Renderer(nullptr),
      m_Pipeline(nullptr),
      m_NeedsSpsFixup(false),
      m_TestOnly(testOnly)
{
    qDebug() << "webOS Video decoder";
}

WebOSVideoDecoder::~WebOSVideoDecoder()
{
    if (m_Pipeline != nullptr) {
        gst_object_unref (m_Pipeline);
    }
    if (m_Renderer != nullptr) {
        SDL_DestroyRenderer(m_Renderer);
    }
}

bool WebOSVideoDecoder::initialize(PDECODER_PARAMETERS params) 
{
    qDebug() << "WebOSVideoDecoder::initialize";

    GstElement *pipeline, *source, *sink;
    /* Create the elements */
    pipeline = gst_parse_launch("appsrc name=src ! h264parse ! lxvideodec ! lxvideosink name=sink", NULL);
    source = gst_bin_get_by_name(GST_BIN(pipeline), "src");
    sink = gst_bin_get_by_name(GST_BIN(pipeline), "sink");

    if (!pipeline || !source || !sink) {
        g_printerr ("Not all elements could be created.\n");
        return false;
    }

    GstCaps *srccaps;
    srccaps = gst_caps_new_simple("video/x-h264", NULL);
    gst_base_src_set_caps(GST_BASE_SRC(source), srccaps);

    // g_signal_connect(sink, "new-preroll", G_CALLBACK(gstSinkNewPreroll), this);
    // g_signal_connect(sink, "new-sample", G_CALLBACK(gstSinkNewSample), this);

    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    m_Source = source;
    m_Sink = sink;
    m_Pipeline = pipeline;

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
    PLENTRY entry = du->bufferList;
    GstFlowReturn err;
    SDL_assert(!m_TestOnly);
    

    int requiredBufferSize = du->fullLength;
    if (du->frameType == FRAME_TYPE_IDR) {
        // Add some extra space in case we need to do an SPS fixup
        requiredBufferSize += MAX_SPS_EXTRA_SIZE;
    }

    GstBuffer * buf;
    buf = gst_buffer_new_allocate(NULL, requiredBufferSize, NULL);

    int offset = 0;
    while (entry != nullptr) {
        gst_buffer_fill(buf, offset, entry->data, entry->length);
        offset += entry->length;
        entry = entry->next;
    }

    if (du->frameType == FRAME_TYPE_IDR) {
        GST_BUFFER_FLAG_UNSET(buf, GST_BUFFER_FLAG_DELTA_UNIT);
    }
    else {
        GST_BUFFER_FLAG_SET(buf, GST_BUFFER_FLAG_DELTA_UNIT);
    }
    
    err = gst_app_src_push_buffer(GST_APP_SRC(m_Source), buf);
    if (err) {
        qWarning("gst src push_buffer()=%d", err);
    }
    return DR_OK;
}

void WebOSVideoDecoder::renderFrameOnMainThread() 
{
    SDL_SetRenderDrawColor(m_Renderer, 255, 0, 0, 255);
    SDL_RenderClear(m_Renderer);
    SDL_RenderPresent(m_Renderer);
}

static int num_frames = 0;

GstFlowReturn WebOSVideoDecoder::gstSinkNewPreroll(GstElement *sink, gpointer self)
{
    GstSample *sample;
    GstFlowReturn ret;
    g_signal_emit_by_name(sink, "pull-preroll", &sample);
    GstCaps *caps = gst_sample_get_caps(sample);
    qDebug() << gst_caps_to_string(caps);
    GstBuffer *buf = gst_sample_get_buffer(sample);
    if (gst_buffer_get_size(buf) == sizeof(LXDEBuffer)) {
        LXDEBuffer lxbuf;
        gst_buffer_extract(buf, 0, &lxbuf, sizeof(LXDEBuffer));

        num_frames = 0;

        qDebug("LXDEBuffer(width=%d, height=%d, addr_y=%lx, addr_c=%lx)", lxbuf.width, lxbuf.height, lxbuf.addr_y, lxbuf.addr_c);
    }
    /* Free the sample now that we are done with it */
    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

GstFlowReturn WebOSVideoDecoder::gstSinkNewSample(GstElement *sink, gpointer self)
{
    GstSample *sample;
    GstFlowReturn ret;
    g_signal_emit_by_name(sink, "pull-sample", &sample);
    GstBuffer *buf = gst_sample_get_buffer(sample);
    if (gst_buffer_get_size(buf) == sizeof(LXDEBuffer)) {
        LXDEBuffer lxbuf;
        gst_buffer_extract(buf, 0, &lxbuf, sizeof(LXDEBuffer));

        if (num_frames == 300)
        {
            std::fstream myFile("/media/developer/temp/preroll.luma",std::ios::out | std::ios::binary);
            myFile.write((char*)lxbuf.element, lxbuf.width * lxbuf.height);
            myFile.flush();
            myFile.close();
        }

        num_frames++;
    }
    /* Free the sample now that we are done with it */
    gst_sample_unref(sample);
    return GST_FLOW_OK;
}