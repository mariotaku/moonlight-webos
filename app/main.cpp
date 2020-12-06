#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtQuick>
#include <QImage>

#include "controller/streaming.h"
#include "gui/computermodel.h"
#include "backend/computermanager.h"
#include "settings/streamingpreferences.h"

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QGuiApplication app(argc, argv);
    app.setApplicationName("com.limelight.webos");

    qmlRegisterType<ComputerModel>("ComputerModel", 1, 0, "ComputerModel");
    qmlRegisterSingletonType<ComputerManager>("ComputerManager", 1, 0,
                                              "ComputerManager",
                                              [](QQmlEngine*, QJSEngine*) -> QObject* {
                                                  return new ComputerManager();
                                              });
    qmlRegisterSingletonType<StreamingPreferences>("StreamingPreferences", 1, 0,
                                                "StreamingPreferences",
                                                [](QQmlEngine*, QJSEngine*) -> QObject* {
                                                    return new StreamingPreferences();
                                                });

    StreamingController::registerQmlType();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("initialView", "qrc:/gui/ComputersView.qml");
    engine.load(QUrl(QStringLiteral("qrc:/gui/main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}