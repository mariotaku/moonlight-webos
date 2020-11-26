#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtQuick>
#include "avcodec-check.h"

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QGuiApplication app(argc, argv);
    qmlRegisterType<AVCodecChecker>("com.limelight.webos", 1, 0, "AVCodecChecker");

    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}