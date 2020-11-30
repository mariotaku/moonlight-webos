#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtQuick>

#include "streaming-controller.h"

int main(int argc, char *argv[])
{
    /* Initialize GStreamer */
    gst_init(&argc, &argv);

    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;

    StreamingController::registerQmlType();

    engine.rootContext()->setContextProperty("initialView", "qrc:/ComputersView.qml");
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;
    qDebug("Platform name %s", qPrintable(app.platformName()));

    return app.exec();
}