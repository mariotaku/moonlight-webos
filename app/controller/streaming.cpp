#include "streaming.h"

#include <QtCore>
#include <QtQml>

void StreamingController::registerQmlType()
{
    qmlRegisterType<StreamingController>(
        "Moonlight.Streaming", 0, 1,
        "StreamingController");
}

StreamingController::StreamingController(QObject *parent)
    : QObject(parent)
{
    qDebug("Streaming Controller Created");
}

StreamingController::~StreamingController()
{
}

void StreamingController::testPlay() 
{
}