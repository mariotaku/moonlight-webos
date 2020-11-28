#include "streaming-controller.h"

#include <QtQml>
#include <QtDebug>
#include <QVideoFrame>
#include <QVideoSurfaceFormat>
#include <QAbstractVideoBuffer>

#include "qgstvideobuffer.h"

struct YuvFormat
{
    QVideoFrame::PixelFormat pixelFormat;
    const gchar *fmt;
    int bitsPerPixel;
};

static const YuvFormat qt_yuvColorLookup[] =
    {
        {QVideoFrame::Format_YUV420P, "I420", 8},
        {QVideoFrame::Format_YV12, "YV12", 8},
        {QVideoFrame::Format_UYVY, "UYVY", 16},
        {QVideoFrame::Format_YUYV, "YUY2", 16},
        {QVideoFrame::Format_NV12, "NV12", 8},
        {QVideoFrame::Format_NV21, "NV21", 8},
        {QVideoFrame::Format_AYUV444, "AYUV", 32}};

static int indexOfYuvColor(QVideoFrame::PixelFormat format)
{
    const int count = sizeof(qt_yuvColorLookup) / sizeof(YuvFormat);

    for (int i = 0; i < count; ++i)
        if (qt_yuvColorLookup[i].pixelFormat == format)
            return i;

    return -1;
}

static int indexOfYuvColor(const gchar *fmt)
{
    const int count = sizeof(qt_yuvColorLookup) / sizeof(YuvFormat);

    for (int i = 0; i < count; ++i)
        if (qstrcmp(fmt, qt_yuvColorLookup[i].fmt) == 0)
            return i;

    return -1;
}

struct RgbFormat
{
    QVideoFrame::PixelFormat pixelFormat;
    unsigned int bitsPerPixel;
    unsigned int depth;
    unsigned int endianness;
    unsigned int red;
    unsigned int green;
    unsigned int blue;
    unsigned int alpha;
};

static const RgbFormat qt_rgbColorLookup[] =
    {
        {QVideoFrame::Format_RGB32, 32, 24, 4321, 0x0000FF00, 0x00FF0000, 0xFF000000, 0x00000000},
        {QVideoFrame::Format_RGB32, 32, 24, 1234, 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00000000},
        {QVideoFrame::Format_BGR32, 32, 24, 4321, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x00000000},
        {QVideoFrame::Format_BGR32, 32, 24, 1234, 0x000000FF, 0x0000FF00, 0x00FF0000, 0x00000000},
        {QVideoFrame::Format_ARGB32, 32, 24, 4321, 0x0000FF00, 0x00FF0000, 0xFF000000, 0x000000FF},
        {QVideoFrame::Format_ARGB32, 32, 24, 1234, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000},
        {QVideoFrame::Format_RGB24, 24, 24, 4321, 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00000000},
        {QVideoFrame::Format_BGR24, 24, 24, 4321, 0x000000FF, 0x0000FF00, 0x00FF0000, 0x00000000},
        {QVideoFrame::Format_RGB565, 16, 16, 1234, 0x0000F800, 0x000007E0, 0x0000001F, 0x00000000}};

static int indexOfRgbColor(
    int bits, int depth, int endianness, int red, int green, int blue, int alpha)
{
    const int count = sizeof(qt_rgbColorLookup) / sizeof(RgbFormat);

    for (int i = 0; i < count; ++i)
    {
        if (qt_rgbColorLookup[i].bitsPerPixel == bits && qt_rgbColorLookup[i].depth == depth && qt_rgbColorLookup[i].endianness == endianness && qt_rgbColorLookup[i].red == red && qt_rgbColorLookup[i].green == green && qt_rgbColorLookup[i].blue == blue && qt_rgbColorLookup[i].alpha == alpha)
        {
            return i;
        }
    }
    return -1;
}

void StreamingController::registerQmlType()
{
    qmlRegisterType<StreamingController>(
        "Moonlight.Streaming", 0, 1,
        "StreamingController");
}

StreamingController::StreamingController(QObject *parent)
    : QObject(parent), _surface(0)
{
    qDebug("Streaming Controller Created");

    gstSetup();
}

StreamingController::~StreamingController()
{
    gstDestroy();

    closeSurface();
    // End session here?
}

void StreamingController::setVideoSurface(QAbstractVideoSurface *s)
{
    closeSurface();
    _surface = s;
    startSurface();
}

void StreamingController::setFormat(QVideoSurfaceFormat fmt)
{
    _format = fmt;

    closeSurface();
    startSurface();
}

void StreamingController::onNativeResolutionChanged()
{
    if (!_surface || !_surface->isActive())
        return;
    QSize res = _surface->nativeResolution();
    qDebug("Video Surface res changed: %d*%d", res.width(), res.height());
}

void StreamingController::closeSurface()
{
    if (_surface && _surface->isActive())
    {
        _surface->stop();
    }
}

void StreamingController::startSurface()
{
    if (_surface && !_surface->isActive() && _format.isValid())
    {
        _format = _surface->nearestFormat(_format);
        QObject::connect(_surface, &QAbstractVideoSurface::nativeResolutionChanged,
                         this, &StreamingController::onNativeResolutionChanged);
        _surface->start(_format);
    }
}

void StreamingController::gstSetup()
{

    GstStateChangeReturn ret;

    GstElement *source, *sink;
    GstPad *src_pad;
    /* Create the elements */
    source = gst_element_factory_make("videotestsrc", "source");
    sink = gst_element_factory_make("appsink", "sink");

    /* Create the empty pipeline */
    pipeline = gst_pipeline_new("test-pipeline");

    if (!pipeline || !source || !sink)
    {
        g_printerr("Not all elements could be created.\n");
        return;
    }

    /* Build the pipeline */
    gst_bin_add_many(GST_BIN(pipeline), source, sink, NULL);
    if (gst_element_link(source, sink) != TRUE)
    {
        g_printerr("Elements could not be linked.\n");
        gst_object_unref(pipeline);
        return;
    }

    /* Modify the source's properties */
    g_object_set(source, "pattern", 0, NULL);
    src_pad = gst_element_get_static_pad(source, "src_%u");
    g_print ("Obtained request pad %s for audio branch.\n", gst_pad_get_name (src_pad));

    /* Modify the sink's properties */
    g_object_set(sink, "emit-signals", 1, NULL);

    g_signal_connect(sink, "new-preroll", G_CALLBACK(gst_cb_new_preroll), this);
    g_signal_connect(sink, "new-sample", G_CALLBACK(gst_cb_new_sample), this);

    /* Start playing */
    ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        g_printerr("Unable to set the pipeline to the playing state.\n");
        gst_object_unref(pipeline);
        return;
    }
}

void StreamingController::gstDestroy()
{
    /* Free resources */
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
}

void StreamingController::queuedRender()
{
if (!_surface || !_surface->isActive())
    {
        return;
    }
    _surface->present(_frame);
}

GstFlowReturn StreamingController::gstHandlePreroll(GstSample *preroll)
{
    if (preroll == NULL)
    {
        return GST_FLOW_EOS;
    }
    else
    {
        const GstCaps *caps;
        caps = gst_sample_get_caps(preroll);
        if (caps == NULL)
        {
            return GST_FLOW_OK;
        }
        setFormat(formatForCaps(caps, &_bytesPerLine));
        return GST_FLOW_OK;
    }
}

GstFlowReturn StreamingController::gstHandleSample(GstSample *sample)
{
    if (!_surface || !_surface->isActive())
    {
        return GST_FLOW_OK;
    }
    if (sample == NULL)
    {
        return GST_FLOW_EOS;
    }
    else
    {
        GstBuffer *buffer;
        buffer = gst_sample_get_buffer(sample);

        QAbstractVideoBuffer *videoBuffer = 0;
        videoBuffer = new QGstVideoBuffer(buffer, _bytesPerLine);

        _frame = QVideoFrame(videoBuffer, _format.frameSize(), _format.pixelFormat());

        QMetaObject::invokeMethod(this, "queuedRender", Qt::QueuedConnection);
        return GST_FLOW_OK;
    }
}

QVideoSurfaceFormat StreamingController::formatForCaps(const GstCaps *caps, int *bytesPerLine)
{
    const GstStructure *structure = gst_caps_get_structure(caps, 0);

    QVideoFrame::PixelFormat pixelFormat = QVideoFrame::Format_Invalid;
    int bitsPerPixel = 0;

    QSize size;
    gst_structure_get_int(structure, "width", &size.rwidth());
    gst_structure_get_int(structure, "height", &size.rheight());

    if (qstrcmp(gst_structure_get_name(structure), "video/x-raw") == 0)
    {
        const gchar *fmt = NULL;
        fmt = gst_structure_get_string(structure, "format");

        int index = indexOfYuvColor(fmt);
        if (index != -1)
        {
            pixelFormat = qt_yuvColorLookup[index].pixelFormat;
            bitsPerPixel = qt_yuvColorLookup[index].bitsPerPixel;
        }
    }
    else if (qstrcmp(gst_structure_get_name(structure), "video/x-raw-rgb") == 0)
    {
        int depth = 0;
        int endianness = 0;
        int red = 0;
        int green = 0;
        int blue = 0;
        int alpha = 0;

        gst_structure_get_int(structure, "bpp", &bitsPerPixel);
        gst_structure_get_int(structure, "depth", &depth);
        gst_structure_get_int(structure, "endianness", &endianness);
        gst_structure_get_int(structure, "red_mask", &red);
        gst_structure_get_int(structure, "green_mask", &green);
        gst_structure_get_int(structure, "blue_mask", &blue);
        gst_structure_get_int(structure, "alpha_mask", &alpha);

        int index = indexOfRgbColor(bitsPerPixel, depth, endianness, red, green, blue, alpha);

        if (index != -1)
            pixelFormat = qt_rgbColorLookup[index].pixelFormat;
    }

    if (pixelFormat != QVideoFrame::Format_Invalid)
    {
        QVideoSurfaceFormat format(size, pixelFormat);

        QPair<int, int> rate;
        gst_structure_get_fraction(structure, "framerate", &rate.first, &rate.second);

        if (rate.second)
            format.setFrameRate(qreal(rate.first) / rate.second);

        gint aspectNum = 0;
        gint aspectDenum = 0;
        if (gst_structure_get_fraction(
                structure, "pixel-aspect-ratio", &aspectNum, &aspectDenum))
        {
            if (aspectDenum > 0)
                format.setPixelAspectRatio(aspectNum, aspectDenum);
        }

        if (bytesPerLine)
            *bytesPerLine = ((size.width() * bitsPerPixel / 8) + 3) & ~3;

        return format;
    }

    return QVideoSurfaceFormat();
}