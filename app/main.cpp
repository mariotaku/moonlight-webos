#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QNetworkProxyFactory>
#include <QtQuick>

// Don't let SDL hook our main function, since Qt is already
// doing the same thing. This needs to be before any headers
// that might include SDL.h themselves.
#define SDL_MAIN_HANDLED
#include <SDL.h>

#ifdef HAVE_FFMPEG
#include "streaming/video/ffmpeg.h"
#endif

#if defined(Q_OS_WIN32)
#include "antihookingprotection.h"
#elif defined(Q_OS_LINUX)
#include <openssl/ssl.h>
#endif

#include "cli/quitstream.h"
#include "cli/startstream.h"
#include "cli/commandlineparser.h"
#include "path.h"
#include "utils.h"
#include "gui/computermodel.h"
#include "gui/appmodel.h"
#include "backend/computermanager.h"
#include "streaming/session.h"
#include "settings/streamingpreferences.h"
#include "gui/sdlgamepadkeynavigation.h"

#ifdef HAVE_GSTREAMER
extern "C" {
#include <gst/gst.h>
}
#endif

int main(int argc, char *argv[])
{
    #ifdef HAVE_GSTREAMER
    gst_init (&argc, &argv);
    #endif
    SDL_SetMainReady();

    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    // Initialize paths for standard installation
    Path::initialize(false);

    // We don't want system proxies to apply to us
    QNetworkProxyFactory::setUseSystemConfiguration(false);

    // Clear any default application proxy
    QNetworkProxy noProxy(QNetworkProxy::NoProxy);
    QNetworkProxy::setApplicationProxy(noProxy);

    // Register custom metatypes for use in signals
    qRegisterMetaType<NvApp>("NvApp");
    
    SDL_version compileVersion;
    SDL_VERSION(&compileVersion);
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Compiled with SDL %d.%d.%d",
                compileVersion.major, compileVersion.minor, compileVersion.patch);

    SDL_version runtimeVersion;
    SDL_GetVersion(&runtimeVersion);
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Running with SDL %d.%d.%d",
                runtimeVersion.major, runtimeVersion.minor, runtimeVersion.patch);

    // Allow the display to sleep by default. We will manually use SDL_DisableScreenSaver()
    // and SDL_EnableScreenSaver() when appropriate. This hint must be set before
    // initializing the SDL video subsystem to have any effect.
    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");

#if SDL_VERSION_ATLEAST(2, 0, 8)
    // For SDL backends that support it, use double buffering instead of triple buffering
    // to save a frame of latency. This doesn't matter for MMAL or DRM renderers since they
    // are drawing directly to the screen without involving SDL, but it may matter for other
    // future KMSDRM platforms that use SDL for rendering.
    SDL_SetHint(SDL_HINT_VIDEO_DOUBLE_BUFFER, "1");
#endif

    if (SDL_InitSubSystem(SDL_INIT_TIMER) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_InitSubSystem(SDL_INIT_TIMER) failed: %s",
                     SDL_GetError());
        return -1;
    }

#ifdef STEAM_LINK
    // Steam Link requires that we initialize video before creating our
    // QGuiApplication in order to configure the framebuffer correctly.
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_InitSubSystem(SDL_INIT_VIDEO) failed: %s",
                     SDL_GetError());
        return -1;
    }
#endif

    // Use atexit() to ensure SDL_Quit() is called. This avoids
    // racing with object destruction where SDL may be used.
    atexit(SDL_Quit);

    // Avoid the default behavior of changing the timer resolution to 1 ms.
    // We don't want this all the time that Moonlight is open. We will set
    // it manually when we start streaming.
    SDL_SetHint(SDL_HINT_TIMER_RESOLUTION, "0");

    // Disable minimize on focus loss by default. Users seem to want this off by default.
    SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");

    // SDL 2.0.12 changes the default behavior to use the button label rather than the button
    // position as most other software does. Set this back to 0 to stay consistent with prior
    // releases of Moonlight.
    SDL_SetHint("SDL_GAMECONTROLLER_USE_BUTTON_LABELS", "0");

#ifdef QT_DEBUG
    // Allow thread naming using exceptions on debug builds. SDL doesn't use SEH
    // when throwing the exceptions, so we don't enable it for release builds out
    // of caution.
    SDL_SetHint("SDL_WINDOWS_DISABLE_THREAD_NAMING", "0");
#endif

    QGuiApplication app(argc, argv);
    app.setApplicationName("com.limelight.webos");

    qmlRegisterType<ComputerModel>("ComputerModel", 1, 0, "ComputerModel");
    qmlRegisterType<AppModel>("AppModel", 1, 0, "AppModel");
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
    qmlRegisterUncreatableType<Session>("Session", 1, 0, "Session", "Session cannot be created from QML");
#else
    qmlRegisterType<Session>("Session", 1, 0, "Session");
#endif
    qmlRegisterSingletonType<ComputerManager>("ComputerManager", 1, 0,
                                              "ComputerManager",
                                              [](QQmlEngine *, QJSEngine *) -> QObject * {
                                                  return new ComputerManager();
                                              });
    qmlRegisterSingletonType<SdlGamepadKeyNavigation>("SdlGamepadKeyNavigation", 1, 0,
                                                    "SdlGamepadKeyNavigation",
                                                    [](QQmlEngine*, QJSEngine*) -> QObject* {
                                                        return new SdlGamepadKeyNavigation();
                                                    });
    qmlRegisterSingletonType<StreamingPreferences>("StreamingPreferences", 1, 0,
                                                   "StreamingPreferences",
                                                   [](QQmlEngine *, QJSEngine *) -> QObject * {
                                                       return new StreamingPreferences();
                                                   });

    // Create the identity manager on the main thread
    IdentityManager::get();

    QQmlApplicationEngine engine;
    QString initialView = "qrc:/gui/PcView.qml";

    // GlobalCommandLineParser parser;
    // switch (parser.parse(app.arguments())) {
    // case GlobalCommandLineParser::NormalStartRequested:
    //     initialView = "qrc:/gui/PcView.qml";
    //     break;
    // case GlobalCommandLineParser::StreamRequested:
    //     {
    //         initialView = "qrc:/gui/CliStartStreamSegue.qml";
    //         StreamingPreferences* preferences = new StreamingPreferences(&app);
    //         StreamCommandLineParser streamParser;
    //         streamParser.parse(app.arguments(), preferences);
    //         QString host    = streamParser.getHost();
    //         QString appName = streamParser.getAppName();
    //         auto launcher   = new CliStartStream::Launcher(host, appName, preferences, &app);
    //         engine.rootContext()->setContextProperty("launcher", launcher);
    //         break;
    //     }
    // case GlobalCommandLineParser::QuitRequested:
    //     {
    //         initialView = "qrc:/gui/CliQuitStreamSegue.qml";
    //         QuitCommandLineParser quitParser;
    //         quitParser.parse(app.arguments());
    //         auto launcher = new CliQuitStream::Launcher(quitParser.getHost(), &app);
    //         engine.rootContext()->setContextProperty("launcher", launcher);
    //         break;
    //     }
    // }

    engine.rootContext()->setContextProperty("initialView", initialView);

    // Load the main.qml file
    engine.load(QUrl(QStringLiteral("qrc:/gui/main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;
    int err = app.exec();

    // Give worker tasks time to properly exit. Fixes PendingQuitTask
    // sometimes freezing and blocking process exit.
    QThreadPool::globalInstance()->waitForDone(30000);

    return err;
}