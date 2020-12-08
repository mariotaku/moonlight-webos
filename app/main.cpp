#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtQuick>
#include <QImage>

#include "path.h"
#include "gui/computermodel.h"
#include "gui/appmodel.h"
#include "backend/computermanager.h"
#include "settings/streamingpreferences.h"

#include "controller/streaming.h"

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    // Initialize paths for standard installation
    Path::initialize(false);

    // Register custom metatypes for use in signals
    qRegisterMetaType<NvApp>("NvApp");

    QGuiApplication app(argc, argv);
    app.setApplicationName("com.limelight.webos");

    qmlRegisterType<ComputerModel>("ComputerModel", 1, 0, "ComputerModel");
    qmlRegisterType<AppModel>("AppModel", 1, 0, "AppModel");
    qmlRegisterSingletonType<ComputerManager>("ComputerManager", 1, 0,
                                              "ComputerManager",
                                              [](QQmlEngine *, QJSEngine *) -> QObject * {
                                                  return new ComputerManager();
                                              });
    qmlRegisterSingletonType<StreamingPreferences>("StreamingPreferences", 1, 0,
                                                   "StreamingPreferences",
                                                   [](QQmlEngine *, QJSEngine *) -> QObject * {
                                                       return new StreamingPreferences();
                                                   });

    StreamingController::registerQmlType();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("initialView", "qrc:/gui/PcView.qml");
    engine.load(QUrl(QStringLiteral("qrc:/gui/main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}