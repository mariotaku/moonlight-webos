#ifndef STREAMING_CONTROLLER_HEADER_H
#define STREAMING_CONTROLLER_HEADER_H

#include <QAbstractVideoSurface>
#include <QVideoSurfaceFormat>
#include <QVideoFrame>

extern "C"
{
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
}

class StreamingController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QAbstractVideoSurface *videoSurface READ videoSurface WRITE setVideoSurface)
public:
    explicit StreamingController(QObject *parent = nullptr);
    ~StreamingController();

    static void registerQmlType();

    QAbstractVideoSurface *videoSurface() const
    {
        return _surface;
    };
    void setVideoSurface(QAbstractVideoSurface *s);
    void setFormat(QVideoSurfaceFormat fmt);

private:
    void onNativeResolutionChanged();
    void closeSurface();
    void startSurface();
    
    void gstSetup();
    void gstDestroy();

    QVideoSurfaceFormat formatForCaps(const GstCaps *caps, int *bytesPerLine);

    GstFlowReturn gstHandlePreroll(GstAppSink *sink);
    GstFlowReturn gstHandleSample(GstSample *sample);

    static GstFlowReturn gst_cb_new_preroll(GstAppSink *sink, gpointer *data)
    {
        return reinterpret_cast<StreamingController *>(data)->gstHandlePreroll(sink);
    }

    static GstFlowReturn gst_cb_new_sample(GstAppSink *sink, gpointer *data)
    {
        GstSample *sample;
        /* Retrieve the buffer */
        g_signal_emit_by_name (sink, "pull-sample", &sample);
        GstFlowReturn ret = reinterpret_cast<StreamingController *>(data)->gstHandleSample(sample);
        gst_sample_unref (sample);
        return ret;
    }

private slots:
    void queuedRender();

private:
    int currentFrame;
    QAbstractVideoSurface *_surface = nullptr;
    QVideoSurfaceFormat _format;
    int _bytesPerLine;
    QVideoFrame _frame;

    GstElement *pipeline, *source, *sink;
    GstBus *bus;
    GstSample *sample;
};

#endif //STREAMING_CONTROLLER_HEADER_H