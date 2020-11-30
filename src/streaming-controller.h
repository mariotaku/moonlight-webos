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

private:
    int currentFrame;
    QAbstractVideoSurface *_surface = nullptr;
    QVideoSurfaceFormat _format;
    int _bytesPerLine;

    GstElement *pipeline;
};

#endif //STREAMING_CONTROLLER_HEADER_H