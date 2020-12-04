#include "streaming-controller.h"

#include <QtQml>
#include <QtDebug>
#include <QVideoFrame>
#include <QVideoSurfaceFormat>
#include <QAbstractVideoBuffer>
#include <string>
#include <fstream>
#include <streambuf>

void StreamingController::registerQmlType()
{
    qmlRegisterType<StreamingController>(
        "Moonlight.Streaming", 0, 1,
        "StreamingController");
}

StreamingController::StreamingController(QObject *parent)
    : QObject(parent), mp()
{
    qDebug("Streaming Controller Created");
}

StreamingController::~StreamingController()
{
}

void StreamingController::testPlay() 
{
    qDebug("testPlay()");
    bool ret = false;
    std::ifstream t("assets/umc_opts.json");
    std::string opts((std::istreambuf_iterator<char>(t)),
                 std::istreambuf_iterator<char>());
    qDebug("load options: %s", opts.c_str());
    ret = mp.load("file:///media/developer/apps/usr/palm/applications/com.limelight.webos/assets/temp/twinning.mp4", kMedia, opts);
    qDebug("load() = %d", ret);
    // uMediaServer::rect_t wd(0, 0, 1920, 1080);
    // ret = mp.setDisplayWindow(wd);
    // qDebug("setDisplayWindow() = %d", ret);
    ret = mp.notifyForeground();
    qDebug("notifyForeground() = %d", ret);
    ret = mp.play();
    qDebug("play() = %d", ret);
}

MediaPlayer::MediaPlayer()
{

}

MediaPlayer::~MediaPlayer()
{

}