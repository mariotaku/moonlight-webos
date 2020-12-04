#ifndef STREAMING_CONTROLLER_HEADER_H
#define STREAMING_CONTROLLER_HEADER_H

#include <QAbstractVideoSurface>
#include <QVideoSurfaceFormat>
#include <QVideoFrame>

#include <uMediaClient.h>

class MediaPlayer: public uMediaServer::uMediaClient
{
public:
    explicit MediaPlayer();
    ~MediaPlayer();
};

class StreamingController : public QObject
{
    Q_OBJECT
public:
    explicit StreamingController(QObject *parent = nullptr);
    ~StreamingController();

    static void registerQmlType();
    Q_INVOKABLE void testPlay();

private:
    MediaPlayer mp;
};

#endif //STREAMING_CONTROLLER_HEADER_H