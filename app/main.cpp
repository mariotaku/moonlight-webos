#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>
#if QT_VERSION >= QT_VERSION_CHECK(5, 7, 0)
#include <QQuickStyle>
#endif
#include <QMutex>
#include <QtDebug>
#include <QNetworkProxyFactory>
#include <QPalette>
#include <QFont>
#include <QCursor>
#include <QElapsedTimer>
#include <QFile>
#include <QTranslator>
#include <QtQuick>

// Don't let SDL hook our main function, since Qt is already
// doing the same thing. This needs to be before any headers
// that might include SDL.h themselves.
#define SDL_MAIN_HANDLED
#include <SDL.h>

#ifdef HAVE_FFMPEG
#include "streaming/video/ffmpeg.h"
#endif

#ifdef HAVE_GST
#include <gst/gst.h>
#endif

#if defined(Q_OS_WIN32) && defined(Q_PROCESSOR_X86)
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
#include "backend/autoupdatechecker.h"
#include "backend/systemproperties.h"
#include "streaming/session.h"
#include "settings/streamingpreferences.h"
#include "gui/sdlgamepadkeynavigation.h"

#if !defined(QT_DEBUG) && defined(Q_OS_WIN32)
// Log to file for release Windows builds
#define USE_CUSTOM_LOGGER
#define LOG_TO_FILE
#elif defined(Q_OS_UNIX) && !defined(Q_OS_DARWIN)
// Use stdout logger on all Linux/BSD builds
#define USE_CUSTOM_LOGGER
#elif !defined(QT_DEBUG) && defined(Q_OS_DARWIN)
// Log to file for release Mac builds
#define USE_CUSTOM_LOGGER
#define LOG_TO_FILE
#else
// For debug Windows and Mac builds, use default logger
#endif

#ifdef USE_CUSTOM_LOGGER
static QElapsedTimer s_LoggerTime;
static QTextStream s_LoggerStream(stdout);
static QMutex s_LoggerLock;
#ifdef LOG_TO_FILE
#define MAX_LOG_LINES 10000
static int s_LogLinesWritten = 0;
static bool s_LogLimitReached = false;
static QFile* s_LoggerFile;
#endif

void logToLoggerStream(QString& message)
{
    QMutexLocker lock(&s_LoggerLock);

#ifdef LOG_TO_FILE
    if (s_LogLimitReached) {
        return;
    }
    else if (s_LogLinesWritten == MAX_LOG_LINES) {
        s_LoggerStream << "Log size limit reached!";
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        s_LoggerStream << Qt::endl;
#else
        s_LoggerStream << endl;
#endif
        s_LogLimitReached = true;
        return;
    }
    else {
        s_LogLinesWritten++;
    }
#endif

    s_LoggerStream << message;
    s_LoggerStream.flush();
}

void sdlLogToDiskHandler(void*, int category, SDL_LogPriority priority, const char* message)
{
    QString priorityTxt;

    switch (priority) {
    case SDL_LOG_PRIORITY_VERBOSE:
        priorityTxt = "Verbose";
        break;
    case SDL_LOG_PRIORITY_DEBUG:
        priorityTxt = "Debug";
        break;
    case SDL_LOG_PRIORITY_INFO:
        priorityTxt = "Info";
        break;
    case SDL_LOG_PRIORITY_WARN:
        priorityTxt = "Warn";
        break;
    case SDL_LOG_PRIORITY_ERROR:
        priorityTxt = "Error";
        break;
    case SDL_LOG_PRIORITY_CRITICAL:
        priorityTxt = "Critical";
        break;
    default:
        priorityTxt = "Unknown";
        break;
    }

    QTime logTime = QTime::fromMSecsSinceStartOfDay(s_LoggerTime.elapsed());
    QString txt = QString("%1 - SDL %2 (%3): %4\n").arg(logTime.toString()).arg(priorityTxt).arg(category).arg(message);

    logToLoggerStream(txt);
}

void qtLogToDiskHandler(QtMsgType type, const QMessageLogContext&, const QString& msg)
{
    QString typeTxt;

    switch (type) {
    case QtDebugMsg:
        typeTxt = "Debug";
        break;
    case QtInfoMsg:
        typeTxt = "Info";
        break;
    case QtWarningMsg:
        typeTxt = "Warning";
        break;
    case QtCriticalMsg:
        typeTxt = "Critical";
        break;
    case QtFatalMsg:
        typeTxt = "Fatal";
        break;
    }

    QTime logTime = QTime::fromMSecsSinceStartOfDay(s_LoggerTime.elapsed());
    QString txt = QString("%1 - Qt %2: %3\n").arg(logTime.toString()).arg(typeTxt).arg(msg);

    logToLoggerStream(txt);
}

#ifdef HAVE_FFMPEG

void ffmpegLogToDiskHandler(void* ptr, int level, const char* fmt, va_list vl)
{
    char lineBuffer[1024];
    static int printPrefix = 1;

    if ((level & 0xFF) > av_log_get_level()) {
        return;
    }

    av_log_format_line(ptr, level, fmt, vl, lineBuffer, sizeof(lineBuffer), &printPrefix);

    QTime logTime = QTime::fromMSecsSinceStartOfDay(s_LoggerTime.elapsed());
    QString txt = QString("%1 - FFmpeg: %2").arg(logTime.toString()).arg(lineBuffer);

    logToLoggerStream(txt);
}

#endif

#endif

#ifdef Q_OS_WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <DbgHelp.h>

static UINT s_HitUnhandledException = 0;

LONG WINAPI UnhandledExceptionHandler(struct _EXCEPTION_POINTERS *ExceptionInfo)
{
    // Only write a dump for the first unhandled exception
    if (InterlockedCompareExchange(&s_HitUnhandledException, 1, 0) != 0) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    WCHAR dmpFileName[MAX_PATH];
    swprintf_s(dmpFileName, L"%ls\\Moonlight-%I64u.dmp",
               (PWCHAR)QDir::toNativeSeparators(Path::getLogDir()).utf16(), QDateTime::currentSecsSinceEpoch());
    QString qDmpFileName = QString::fromUtf16((const char16_t*)dmpFileName);
    HANDLE dumpHandle = CreateFileW(dmpFileName, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (dumpHandle != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION info;

        info.ThreadId = GetCurrentThreadId();
        info.ExceptionPointers = ExceptionInfo;
        info.ClientPointers = FALSE;

        DWORD typeFlags = MiniDumpWithIndirectlyReferencedMemory |
                MiniDumpIgnoreInaccessibleMemory |
                MiniDumpWithUnloadedModules |
                MiniDumpWithThreadInfo;

        if (MiniDumpWriteDump(GetCurrentProcess(),
                               GetCurrentProcessId(),
                               dumpHandle,
                               (MINIDUMP_TYPE)typeFlags,
                               &info,
                               nullptr,
                               nullptr)) {
            qCritical() << "Unhandled exception! Minidump written to:" << qDmpFileName;
        }
        else {
            qCritical() << "Unhandled exception! Failed to write dump:" << GetLastError();
        }

        CloseHandle(dumpHandle);
    }
    else {
        qCritical() << "Unhandled exception! Failed to open dump file:" << qDmpFileName << "with error" << GetLastError();
    }

    // Let the program crash and WER collect a dump
    return EXCEPTION_CONTINUE_SEARCH;
}

#endif

int main(int argc, char *argv[])
{
#ifdef HAVE_GST
    gst_init (&argc, &argv);
#endif
    SDL_SetMainReady();

    // Set the app version for the QCommandLineParser's showVersion() command
    QCoreApplication::setApplicationVersion(VERSION_STR);

    // Set these here to allow us to use the default QSettings constructor.
    // These also ensure that our cache directory is named correctly. As such,
    // it is critical that these be called before Path::initialize().
    QCoreApplication::setOrganizationName("Moonlight Game Streaming Project");
    QCoreApplication::setOrganizationDomain("moonlight-stream.com");
#ifdef Q_OS_WEBOS
    // Application name must be set properly in order to make sound working
    QCoreApplication::setApplicationName("com.limelight.webos");
#else
    QCoreApplication::setApplicationName("Moonlight");
#endif
    if (QFile(QDir::currentPath() + "/portable.dat").exists()) {
        qInfo() << "Running in portable mode from:" << QDir::currentPath();
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, QDir::currentPath());
        QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, QDir::currentPath());

        // Initialize paths for portable mode
        Path::initialize(true);
    }
    else {
        // Initialize paths for standard installation
        Path::initialize(false);
    }

#ifdef USE_CUSTOM_LOGGER
#ifdef LOG_TO_FILE
    QDir tempDir(Path::getLogDir());
    s_LoggerFile = new QFile(tempDir.filePath(QString("Moonlight-%1.log").arg(QDateTime::currentSecsSinceEpoch())));
    if (s_LoggerFile->open(QIODevice::WriteOnly)) {
        qInfo() << "Redirecting log output to " << s_LoggerFile->fileName();
        s_LoggerStream.setDevice(s_LoggerFile);
    }
#endif

    s_LoggerTime.start();
    qInstallMessageHandler(qtLogToDiskHandler);
    SDL_LogSetOutputFunction(sdlLogToDiskHandler, nullptr);

#ifdef HAVE_FFMPEG
    av_log_set_callback(ffmpegLogToDiskHandler);
#endif

#endif

#ifdef Q_OS_WIN32
    // Create a crash dump when we crash on Windows
    SetUnhandledExceptionFilter(UnhandledExceptionHandler);
#endif

#if defined(Q_OS_WIN32) && defined(Q_PROCESSOR_X86)
    // Force AntiHooking.dll to be statically imported and loaded
    // by ntdll on x86/x64 platforms by calling a dummy function.
    // AntiHooking.dll is not currently built for ARM64.
    AntiHookingDummyImport();
#elif defined(Q_OS_LINUX)
    // Force libssl.so to be directly linked to our binary, so
    // linuxdeployqt can find it and include it in our AppImage.
    // QtNetwork will pull it in via dlopen().
    SSL_free(nullptr);
#endif

    // Avoid using High DPI on EGLFS. It breaks font rendering.
    // https://bugreports.qt.io/browse/QTBUG-64377
    //
    // NB: We can't use QGuiApplication::platformName() here because it is only
    // set once the QGuiApplication is created, which is too late to enable High DPI :(
    if (WMUtils::isRunningWindowManager()) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        // Enable High DPI support on Qt 5.x. It is always enabled on Qt 6.0
        QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        // Enable fractional High DPI scaling on Qt 5.14 and later
        QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif
    }
    else {
#ifndef STEAM_LINK
        if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
            qInfo() << "Unable to detect Wayland or X11, so EGLFS will be used by default. Set QT_QPA_PLATFORM to override this.";
            qputenv("QT_QPA_PLATFORM", "eglfs");

            if (!QFile("/dev/dri").exists()) {
                qWarning() << "Unable to find a KMSDRM display device!";
                qWarning() << "On the Raspberry Pi, you must enable the 'fake KMS' driver in raspi-config to use Moonlight outside of the GUI environment.";
            }
        }
#endif
    }

    // This avoids using the default keychain for SSL, which may cause
    // password prompts on macOS.
    qputenv("QT_SSL_USE_TEMPORARY_KEYCHAIN", "1");

#if defined(Q_OS_WIN32) && QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    if (!qEnvironmentVariableIsSet("QT_OPENGL")) {
        // On Windows, use ANGLE so we don't have to load OpenGL
        // user-mode drivers into our app. OGL drivers (especially Intel)
        // seem to crash Moonlight far more often than DirectX.
        qputenv("QT_OPENGL", "angle");
    }
#endif

#if defined(Q_OS_DARWIN) && QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (!qEnvironmentVariableIsSet("QSG_RHI_BACKEND")) {
        // The Metal backend in Qt 6.0.0 causes really strange issues transitioning to
        // full-screen in our SDL window (hangs in SDL's Cocoa_SetWindowFullscreenSpace())
        // and breaks drawing our status updates in the StreamSegue, so use OpenGL like
        // Qt 5 does until we figure out the cause of these issues.
        qputenv("QSG_RHI_BACKEND", "opengl");
    }
#endif

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
    SDL_SetHint(SDL_HINT_WINDOWS_DISABLE_THREAD_NAMING, "0");
#endif

    QGuiApplication app(argc, argv);

    QTranslator translator;
    qDebug() << "Translation loaded:" << translator.load(QString(":/languages/qml_") + QLocale::system().name());
    app.installTranslator(&translator);

    // After the QGuiApplication is created, the platform stuff will be initialized
    // and we can set the SDL video driver to match Qt.
    if (WMUtils::isRunningWayland() && QGuiApplication::platformName() == "xcb") {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Detected XWayland. This will probably break hardware decoding! Try running with QT_QPA_PLATFORM=wayland or switch to X11.");
    }
    else if (QGuiApplication::platformName().startsWith("wayland")) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Detected Wayland");
        qputenv("SDL_VIDEODRIVER", "wayland");
    }

#ifdef STEAM_LINK
    // Qt 5.9 from the Steam Link SDK is not able to load any fonts
    // since the Steam Link doesn't include any of the ones it looks
    // for. We know it has NotoSans so we will explicitly ask for that.
    if (app.font().family().isEmpty()) {
        qWarning() << "SL HACK: No default font - using NotoSans";

        QFont fon("NotoSans");
        app.setFont(fon);
    }

    // Move the mouse to the bottom right so it's invisible when using
    // gamepad-only navigation.
    QCursor().setPos(0xFFFF, 0xFFFF);
#elif !SDL_VERSION_ATLEAST(2, 0, 11) && defined(Q_OS_LINUX) && (defined(__arm__) || defined(__aarch64__))
    if (qgetenv("SDL_VIDEO_GL_DRIVER").isEmpty() && QGuiApplication::platformName() == "eglfs") {
        // Look for Raspberry Pi GLES libraries. SDL 2.0.10 and earlier needs some help finding
        // the correct libraries for the KMSDRM backend if not compiled with the RPI backend enabled.
        if (SDL_LoadObject("libbrcmGLESv2.so") != nullptr) {
            qputenv("SDL_VIDEO_GL_DRIVER", "libbrcmGLESv2.so");
        }
        else if (SDL_LoadObject("/opt/vc/lib/libbrcmGLESv2.so") != nullptr) {
            qputenv("SDL_VIDEO_GL_DRIVER", "/opt/vc/lib/libbrcmGLESv2.so");
        }
    }
#endif

#ifndef Q_OS_DARWIN
    // Set the window icon except on macOS where we want to keep the
    // modified macOS 11 style rounded corner icon.
    app.setWindowIcon(QIcon(":/res/moonlight.svg"));
#endif

    // Register our C++ types for QML
    qmlRegisterType<ComputerModel>("ComputerModel", 1, 0, "ComputerModel");
    qmlRegisterType<AppModel>("AppModel", 1, 0, "AppModel");
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
    qmlRegisterUncreatableType<Session>("Session", 1, 0, "Session", "Session cannot be created from QML");
#else
    qmlRegisterType<Session>("Session", 1, 0, "Session");
#endif
    qmlRegisterSingletonType<ComputerManager>("ComputerManager", 1, 0,
                                              "ComputerManager",
                                              [](QQmlEngine*, QJSEngine*) -> QObject* {
                                                  return new ComputerManager();
                                              });
    qmlRegisterSingletonType<AutoUpdateChecker>("AutoUpdateChecker", 1, 0,
                                                "AutoUpdateChecker",
                                                [](QQmlEngine*, QJSEngine*) -> QObject* {
                                                    return new AutoUpdateChecker();
                                                });
    qmlRegisterSingletonType<SystemProperties>("SystemProperties", 1, 0,
                                               "SystemProperties",
                                               [](QQmlEngine*, QJSEngine*) -> QObject* {
                                                   return new SystemProperties();
                                               });
    qmlRegisterSingletonType<SdlGamepadKeyNavigation>("SdlGamepadKeyNavigation", 1, 0,
                                                      "SdlGamepadKeyNavigation",
                                                      [](QQmlEngine*, QJSEngine*) -> QObject* {
                                                          return new SdlGamepadKeyNavigation();
                                                      });
    qmlRegisterSingletonType<StreamingPreferences>("StreamingPreferences", 1, 0,
                                                   "StreamingPreferences",
                                                   [](QQmlEngine*, QJSEngine*) -> QObject* {
                                                       return new StreamingPreferences();
                                                   });

    // Create the identity manager on the main thread
    IdentityManager::get();

#ifndef Q_OS_WINRT
    // Use the dense material dark theme by default
    if (!qEnvironmentVariableIsSet("QT_QUICK_CONTROLS_STYLE")) {
        qputenv("QT_QUICK_CONTROLS_STYLE", "Material");
    }
#else
    // Use universal dark on WinRT
    if (!qEnvironmentVariableIsSet("QT_QUICK_CONTROLS_STYLE")) {
        qputenv("QT_QUICK_CONTROLS_STYLE", "Universal");
    }
#endif
    if (!qEnvironmentVariableIsSet("QT_QUICK_CONTROLS_MATERIAL_THEME")) {
        qputenv("QT_QUICK_CONTROLS_MATERIAL_THEME", "Dark");
    }
    if (!qEnvironmentVariableIsSet("QT_QUICK_CONTROLS_MATERIAL_ACCENT")) {
        qputenv("QT_QUICK_CONTROLS_MATERIAL_ACCENT", "Purple");
    }
    if (!qEnvironmentVariableIsSet("QT_QUICK_CONTROLS_MATERIAL_VARIANT")) {
        qputenv("QT_QUICK_CONTROLS_MATERIAL_VARIANT", "Dense");
    }
    if (!qEnvironmentVariableIsSet("QT_QUICK_CONTROLS_UNIVERSAL_THEME")) {
        qputenv("QT_QUICK_CONTROLS_UNIVERSAL_THEME", "Dark");
    }

    QQmlApplicationEngine engine;
    QString initialView;

#ifndef Q_OS_WEBOS
    // webOS will pass their own parameter, which isn't recognized by Moonlight 
    GlobalCommandLineParser parser;
    switch (parser.parse(app.arguments())) {
    case GlobalCommandLineParser::NormalStartRequested:
        initialView = "qrc:/gui/PcView.qml";
        break;
    case GlobalCommandLineParser::StreamRequested:
        {
            initialView = "qrc:/gui/CliStartStreamSegue.qml";
            StreamingPreferences* preferences = new StreamingPreferences(&app);
            StreamCommandLineParser streamParser;
            streamParser.parse(app.arguments(), preferences);
            QString host    = streamParser.getHost();
            QString appName = streamParser.getAppName();
            auto launcher   = new CliStartStream::Launcher(host, appName, preferences, &app);
            engine.rootContext()->setContextProperty("launcher", launcher);
            break;
        }
    case GlobalCommandLineParser::QuitRequested:
        {
            initialView = "qrc:/gui/CliQuitStreamSegue.qml";
            QuitCommandLineParser quitParser;
            quitParser.parse(app.arguments());
            auto launcher = new CliQuitStream::Launcher(quitParser.getHost(), &app);
            engine.rootContext()->setContextProperty("launcher", launcher);
            break;
        }
    }
#else
    initialView = "qrc:/gui/webos/PcView.qml";
#endif

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
