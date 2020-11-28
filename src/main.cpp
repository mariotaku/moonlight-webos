#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtQuick>

#include "streaming-controller.h"

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;

    StreamingController::registerQmlType();

    engine.rootContext()->setContextProperty("initialView", "qrc:/ComputersView.qml");
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}