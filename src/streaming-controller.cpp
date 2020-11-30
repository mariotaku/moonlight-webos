#include "streaming-controller.h"

#include <QtQml>
#include <QtDebug>
#include <QVideoFrame>
#include <QVideoSurfaceFormat>
#include <QAbstractVideoBuffer>

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

    GstElement *bin, *sink;
    /* Build the pipeline */
    // pipeline = gst_parse_launch("playbin uri=file:///media/developer/apps/usr/palm/applications/com.limelight.webos/assets/temp/twinning.mp4", NULL);
    pipeline = gst_parse_launch("filesrc location=/media/developer/apps/usr/palm/applications/com.limelight.webos/assets/temp/twinning.mp4 ! qtdemux name=demuxer demuxer. ! queue ! aac_audiodec ! audioconvert ! pulsesink demuxer. ! queue ! h264parse ! lxvideodec ! fakesink", NULL);

    sink = gst_bin_get_by_name(GST_BIN(pipeline), "vsink");
    if (!pipeline || !sink)
    {
        g_printerr("Not all elements could be created.\n");
        return;
    }

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
