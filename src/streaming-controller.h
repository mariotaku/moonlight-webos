#ifndef STREAMING_CONTROLLER_HEADER_H
#define STREAMING_CONTROLLER_HEADER_H

#include <QAbstractVideoSurface>
#include <QVideoSurfaceFormat>
#include <QVideoFrame>

class StreamingController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QAbstractVideoSurface *videoSurface READ videoSurface WRITE setVideoSurface)
public:
    explicit StreamingController(QObject *parent = nullptr);
    ~StreamingController();

    static void registerQmlType();

    QAbstractVideoSurface *videoSurface() const {
        return _surface;
    };
    void setVideoSurface(QAbstractVideoSurface *s);
    void setFormat(QSize size, QVideoFrame::PixelFormat pxfmt);

private:
    void onNativeResolutionChanged();
    void closeSurface();
    void startSurface();

private:
    int currentFrame;
    QAbstractVideoSurface *_surface = nullptr;
    QVideoSurfaceFormat _format;
    QVideoFrame _frame;
};

#endif //STREAMING_CONTROLLER_HEADER_H