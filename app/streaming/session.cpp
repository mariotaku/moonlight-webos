#include "session.h"
#include "settings/streamingpreferences.h"
#include "streaming/streamutils.h"
#include "backend/richpresencemanager.h"

#include <Limelight.h>
#include <SDL.h>
#include "utils.h"

#ifdef HAVE_FFMPEG
#include "video/ffmpeg.h"
#endif

#ifdef HAVE_SLVIDEO
#include "video/slvid.h"
#endif

#ifdef HAVE_WEBOS
#include "video/webos.h"
#endif

#ifdef Q_OS_WIN32
// Scaling the icon down on Win32 looks dreadful, so render at lower res
#define ICON_SIZE 32
#else
#define ICON_SIZE 64
#endif

#include <openssl/rand.h>

#include <QtEndian>
#include <QCoreApplication>
#include <QThreadPool>
#include <QPainter>
#include <QImage>
#include <QGuiApplication>
#include <QCursor>

#define CONN_TEST_SERVER "qt.conntest.moonlight-stream.org"

// Running the connection process asynchronously seems to reliably
// cause a crash in QSGRenderThread on Wayland and strange crashes
// elsewhere. Until these are figured out, avoid the async connect
// thread on Linux and BSDs.
#if !defined(Q_OS_UNIX) || defined(Q_OS_DARWIN)
#define USE_ASYNC_CONNECT_THREAD 1
#endif

CONNECTION_LISTENER_CALLBACKS Session::k_ConnCallbacks = {
    Session::clStageStarting,
    nullptr,
    Session::clStageFailed,
    nullptr,
    Session::clConnectionTerminated,
    Session::clLogMessage,
    Session::clRumble,
    Session::clConnectionStatusUpdate
};

Session* Session::s_ActiveSession;
QSemaphore Session::s_ActiveSessionSemaphore(1);

void Session::clStageStarting(int stage)
{
    // We know this is called on the same thread as LiStartConnection()
    // which happens to be the main thread, so it's cool to interact
    // with the GUI in these callbacks.
    emit s_ActiveSession->stageStarting(QString::fromLocal8Bit(LiGetStageName(stage)));

#ifndef USE_ASYNC_CONNECT_THREAD
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    QCoreApplication::sendPostedEvents();
#endif
}

void Session::clStageFailed(int stage, int errorCode)
{
    // Perform the port test now, while we're on the async connection thread and not blocking the UI.
    s_ActiveSession->m_PortTestResults = LiTestClientConnectivity(CONN_TEST_SERVER, 443, LiGetPortFlagsFromStage(stage));

    emit s_ActiveSession->stageFailed(QString::fromLocal8Bit(LiGetStageName(stage)), errorCode);

#ifndef USE_ASYNC_CONNECT_THREAD
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    QCoreApplication::sendPostedEvents();
#endif
}

void Session::clConnectionTerminated(int errorCode)
{
    s_ActiveSession->m_PortTestResults = LiTestClientConnectivity(CONN_TEST_SERVER, 443, LiGetPortFlagsFromTerminationErrorCode(errorCode));

    // Display the termination dialog if this was not intended
    switch (errorCode) {
    case ML_ERROR_GRACEFUL_TERMINATION:
        break;

    case ML_ERROR_NO_VIDEO_TRAFFIC:
        s_ActiveSession->m_UnexpectedTermination = true;
        emit s_ActiveSession->displayLaunchError(tr("No video received from host. Check the host PC's firewall and port forwarding rules."));
        break;

    case ML_ERROR_NO_VIDEO_FRAME:
        s_ActiveSession->m_UnexpectedTermination = true;
        emit s_ActiveSession->displayLaunchError(tr("Your network connection isn't performing well. Reduce your video bitrate setting or try a faster connection."));
        break;

    default:
        s_ActiveSession->m_UnexpectedTermination = true;
        emit s_ActiveSession->displayLaunchError(tr("Connection terminated"));
        break;
    }

    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Connection terminated: %d",
                 errorCode);

    // Push a quit event to the main loop
    SDL_Event event;
    event.type = SDL_QUIT;
    event.quit.timestamp = SDL_GetTicks();
    SDL_PushEvent(&event);
}

void Session::clLogMessage(const char* format, ...)
{
    va_list ap;

    va_start(ap, format);
    SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION,
                    SDL_LOG_PRIORITY_INFO,
                    format,
                    ap);
    va_end(ap);
}

void Session::clRumble(unsigned short controllerNumber, unsigned short lowFreqMotor, unsigned short highFreqMotor)
{
    // The input handler can be closed during the stream if LiStopConnection() hasn't completed yet
    // but the stream has been stopped by the user. In this case, just discard the rumble.
    SDL_AtomicLock(&s_ActiveSession->m_InputHandlerLock);
    if (s_ActiveSession->m_InputHandler != nullptr) {
        s_ActiveSession->m_InputHandler->rumble(controllerNumber, lowFreqMotor, highFreqMotor);
    }
    SDL_AtomicUnlock(&s_ActiveSession->m_InputHandlerLock);
}

void Session::clConnectionStatusUpdate(int connectionStatus)
{
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Connection status update: %d",
                connectionStatus);

    if (!s_ActiveSession->m_Preferences->connectionWarnings) {
        return;
    }

    if (s_ActiveSession->m_MouseEmulationRefCount > 0) {
        // Don't display the overlay if mouse emulation is already using it
        return;
    }

    switch (connectionStatus)
    {
    case CONN_STATUS_POOR:
        if (s_ActiveSession->m_StreamConfig.bitrate > 5000) {
            strcpy(s_ActiveSession->m_OverlayManager.getOverlayText(Overlay::OverlayStatusUpdate), "Slow connection to PC\nReduce your bitrate");
        }
        else {
            strcpy(s_ActiveSession->m_OverlayManager.getOverlayText(Overlay::OverlayStatusUpdate), "Poor connection to PC");
        }
        s_ActiveSession->m_OverlayManager.setOverlayTextUpdated(Overlay::OverlayStatusUpdate);
        s_ActiveSession->m_OverlayManager.setOverlayState(Overlay::OverlayStatusUpdate, true);
        break;
    case CONN_STATUS_OKAY:
        s_ActiveSession->m_OverlayManager.setOverlayState(Overlay::OverlayStatusUpdate, false);
        break;
    }
}

bool Session::chooseDecoder(StreamingPreferences::VideoDecoderSelection vds,
                            SDL_Window* window, int videoFormat, int width, int height,
                            int frameRate, bool enableVsync, bool enableFramePacing, bool testOnly, IVideoDecoder*& chosenDecoder)
{
    DECODER_PARAMETERS params;

    params.width = width;
    params.height = height;
    params.frameRate = frameRate;
    params.videoFormat = videoFormat;
    params.window = window;
    params.enableVsync = enableVsync;
    params.enableFramePacing = enableFramePacing;
    params.vds = vds;

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "V-sync %s",
                enableVsync ? "enabled" : "disabled");

#ifdef HAVE_SLVIDEO
    chosenDecoder = new SLVideoDecoder(testOnly);
    if (chosenDecoder->initialize(&params)) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "SLVideo video decoder chosen");
        return true;
    }
    else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Unable to load SLVideo decoder");
        delete chosenDecoder;
        chosenDecoder = nullptr;
    }
#endif

#ifdef HAVE_FFMPEG
    chosenDecoder = new FFmpegVideoDecoder(testOnly);
    if (chosenDecoder->initialize(&params)) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "FFmpeg-based video decoder chosen");
        return true;
    }
    else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Unable to load FFmpeg decoder");
        delete chosenDecoder;
        chosenDecoder = nullptr;
    }
#endif

#ifdef HAVE_WEBOS
    chosenDecoder = new WebOSVideoDecoder(testOnly);
    if (chosenDecoder->initialize(&params)) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "webOS system video decoder chosen");
        return true;
    }
    else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Unable to load webOS decoder");
        delete chosenDecoder;
        chosenDecoder = nullptr;
    }
#endif

#if !defined(HAVE_FFMPEG) && !defined(HAVE_SLVIDEO) && !defined(HAVE_WEBOS)
#error No video decoding libraries available!
#endif

    // If we reach this, we didn't initialize any decoders successfully
    return false;
}

int Session::drSetup(int videoFormat, int width, int height, int frameRate, void *, int)
{
    s_ActiveSession->m_ActiveVideoFormat = videoFormat;
    s_ActiveSession->m_ActiveVideoWidth = width;
    s_ActiveSession->m_ActiveVideoHeight = height;
    s_ActiveSession->m_ActiveVideoFrameRate = frameRate;

    // Defer decoder setup until we've started streaming so we
    // don't have to hide and show the SDL window (which seems to
    // cause pointer hiding to break on Windows).

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Video stream is %dx%dx%d (format 0x%x)",
                width, height, frameRate, videoFormat);

    return 0;
}

int Session::drSubmitDecodeUnit(PDECODE_UNIT du)
{
    // Use a lock since we'll be yanking this decoder out
    // from underneath the session when we initiate destruction.
    // We need to destroy the decoder on the main thread to satisfy
    // some API constraints (like DXVA2). If we can't acquire it,
    // that means the decoder is about to be destroyed, so we can
    // safely return DR_OK and wait for m_NeedsIdr to be set by
    // the decoder reinitialization code.

    if (SDL_AtomicTryLock(&s_ActiveSession->m_DecoderLock)) {
        if (s_ActiveSession->m_NeedsIdr) {
            // If we reset our decoder, we'll need to request an IDR frame
            s_ActiveSession->m_NeedsIdr = false;
            SDL_AtomicUnlock(&s_ActiveSession->m_DecoderLock);
            return DR_NEED_IDR;
        }

        IVideoDecoder* decoder = s_ActiveSession->m_VideoDecoder;
        if (decoder != nullptr) {
            int ret = decoder->submitDecodeUnit(du);
            SDL_AtomicUnlock(&s_ActiveSession->m_DecoderLock);
            return ret;
        }
        else {
            SDL_AtomicUnlock(&s_ActiveSession->m_DecoderLock);
            return DR_OK;
        }
    }
    else {
        // Decoder is going away. Ignore anything coming in until
        // the lock is released.
        return DR_OK;
    }
}

bool Session::isHardwareDecodeAvailable(SDL_Window* window,
                                        StreamingPreferences::VideoDecoderSelection vds,
                                        int videoFormat, int width, int height, int frameRate)
{
    IVideoDecoder* decoder;

    if (!chooseDecoder(vds, window, videoFormat, width, height, frameRate, true, false, true, decoder)) {
        return false;
    }

    bool ret = decoder->isHardwareAccelerated();

    delete decoder;

    return ret;
}

bool Session::populateDecoderProperties(SDL_Window* window)
{
    IVideoDecoder* decoder;

    if (!chooseDecoder(m_Preferences->videoDecoderSelection,
                       window,
                       m_StreamConfig.enableHdr ? VIDEO_FORMAT_H265_MAIN10 :
                            (m_StreamConfig.supportsHevc ? VIDEO_FORMAT_H265 : VIDEO_FORMAT_H264),
                       m_StreamConfig.width,
                       m_StreamConfig.height,
                       m_StreamConfig.fps,
                       true, false, true, decoder)) {
        return false;
    }

    m_VideoCallbacks.capabilities = decoder->getDecoderCapabilities();

    m_StreamConfig.colorSpace = decoder->getDecoderColorspace();

    delete decoder;

    return true;
}

Session::Session(NvComputer* computer, NvApp& app, StreamingPreferences *preferences)
    : m_Preferences(preferences ? preferences : new StreamingPreferences(this)),
      m_Computer(computer),
      m_App(app),
      m_Window(nullptr),
      m_VideoDecoder(nullptr),
      m_DecoderLock(0),
      m_NeedsIdr(false),
      m_AudioDisabled(false),
      m_DisplayOriginX(0),
      m_DisplayOriginY(0),
      m_PendingWindowedTransition(false),
      m_UnexpectedTermination(true), // Failure prior to streaming is unexpected
      m_InputHandler(nullptr),
      m_InputHandlerLock(0),
      m_MouseEmulationRefCount(0),
      m_AsyncConnectionSuccess(false),
      m_PortTestResults(0),
      m_OpusDecoder(nullptr),
      m_AudioRenderer(nullptr),
      m_AudioSampleCount(0),
      m_DropAudioEndTime(0)
{
}

Session::Session(QObject *parent)
{
    qCritical() << "It has been created via QML and that's an error";
}

// NB: This may not get destroyed for a long time! Don't put any vital cleanup here.
// Use Session::exec() or DeferredSessionCleanupTask instead.
Session::~Session()
{
    // Acquire session semaphore to ensure all cleanup is done before the destructor returns
    // and the object is deallocated.
    s_ActiveSessionSemaphore.acquire();
    s_ActiveSessionSemaphore.release();
}

bool Session::initialize()
{
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_InitSubSystem(SDL_INIT_VIDEO) failed: %s",
                     SDL_GetError());
        return false;
    }

    // Create a hidden window to use for decoder initialization tests
    SDL_Window* testWindow = SDL_CreateWindow("", 0, 0, 1280, 720,
                                              SDL_WINDOW_HIDDEN | StreamUtils::getPlatformWindowFlags());
    if (!testWindow) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Failed to create test window with platform flags: %s",
                    SDL_GetError());

        testWindow = SDL_CreateWindow("", 0, 0, 1280, 720, SDL_WINDOW_HIDDEN);
        if (!testWindow) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Failed to create window for hardware decode test: %s",
                         SDL_GetError());
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
            return false;
        }
    }

    qInfo() << "Server GPU:" << m_Computer->gpuModel;
    qInfo() << "Server GFE version:" << m_Computer->gfeVersion;

    LiInitializeVideoCallbacks(&m_VideoCallbacks);
    m_VideoCallbacks.setup = drSetup;
    m_VideoCallbacks.submitDecodeUnit = drSubmitDecodeUnit;

    LiInitializeStreamConfiguration(&m_StreamConfig);
    m_StreamConfig.width = m_Preferences->width;
    m_StreamConfig.height = m_Preferences->height;
    m_StreamConfig.fps = m_Preferences->fps;
    m_StreamConfig.bitrate = m_Preferences->bitrateKbps;
    m_StreamConfig.hevcBitratePercentageMultiplier = 75;

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Video bitrate: %d kbps",
                m_StreamConfig.bitrate);

    RAND_bytes(reinterpret_cast<unsigned char*>(m_StreamConfig.remoteInputAesKey),
               sizeof(m_StreamConfig.remoteInputAesKey));

    // Only the first 4 bytes are populated in the RI key IV
    RAND_bytes(reinterpret_cast<unsigned char*>(m_StreamConfig.remoteInputAesIv), 4);

    switch (m_Preferences->audioConfig)
    {
    case StreamingPreferences::AC_STEREO:
        m_StreamConfig.audioConfiguration = AUDIO_CONFIGURATION_STEREO;
        break;
    case StreamingPreferences::AC_51_SURROUND:
        m_StreamConfig.audioConfiguration = AUDIO_CONFIGURATION_51_SURROUND;
        break;
    case StreamingPreferences::AC_71_SURROUND:
        m_StreamConfig.audioConfiguration = AUDIO_CONFIGURATION_71_SURROUND;
        break;
    }

    LiInitializeAudioCallbacks(&m_AudioCallbacks);
    m_AudioCallbacks.init = arInit;
    m_AudioCallbacks.cleanup = arCleanup;
    m_AudioCallbacks.decodeAndPlaySample = arDecodeAndPlaySample;
    m_AudioCallbacks.capabilities = getAudioRendererCapabilities(m_StreamConfig.audioConfiguration);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Audio channel count: %d",
                CHANNEL_COUNT_FROM_AUDIO_CONFIGURATION(m_StreamConfig.audioConfiguration));
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Audio channel mask: %X",
                CHANNEL_MASK_FROM_AUDIO_CONFIGURATION(m_StreamConfig.audioConfiguration));

    switch (m_Preferences->videoCodecConfig)
    {
    case StreamingPreferences::VCC_AUTO:
        // TODO: Determine if HEVC is better depending on the decoder
        m_StreamConfig.supportsHevc =
                isHardwareDecodeAvailable(testWindow,
                                          m_Preferences->videoDecoderSelection,
                                          VIDEO_FORMAT_H265,
                                          m_StreamConfig.width,
                                          m_StreamConfig.height,
                                          m_StreamConfig.fps);
#ifdef Q_OS_DARWIN
        {
            // Prior to GFE 3.11, GFE did not allow us to constrain
            // the number of reference frames, so we have to fixup the SPS
            // to allow decoding via VideoToolbox on macOS. Since we don't
            // have fixup code for HEVC, just avoid it if GFE is too old.
            QVector<int> gfeVersion = NvHTTP::parseQuad(m_Computer->gfeVersion);
            if (gfeVersion.isEmpty() || // Very old versions don't have GfeVersion at all
                    gfeVersion[0] < 3 ||
                    (gfeVersion[0] == 3 && gfeVersion[1] < 11)) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Disabling HEVC on macOS due to old GFE version");
                m_StreamConfig.supportsHevc = false;
            }
        }
#endif
        m_StreamConfig.enableHdr = false;
        break;
    case StreamingPreferences::VCC_FORCE_H264:
        m_StreamConfig.supportsHevc = false;
        m_StreamConfig.enableHdr = false;
        break;
    case StreamingPreferences::VCC_FORCE_HEVC:
        m_StreamConfig.supportsHevc = true;
        m_StreamConfig.enableHdr = false;
        break;
    case StreamingPreferences::VCC_FORCE_HEVC_HDR:
        m_StreamConfig.supportsHevc = true;
        m_StreamConfig.enableHdr = true;
        break;
    }

    switch (m_Preferences->windowMode)
    {
    default:
    case StreamingPreferences::WM_FULLSCREEN_DESKTOP:
        m_FullScreenFlag = SDL_WINDOW_FULLSCREEN_DESKTOP;
        break;
    case StreamingPreferences::WM_FULLSCREEN:
        m_FullScreenFlag = SDL_WINDOW_FULLSCREEN;
        break;
    }

#if !SDL_VERSION_ATLEAST(2, 0, 11)
    // HACK: Using a full-screen window breaks mouse capture on the Pi's LXDE
    // GUI environment. Force the session to use windowed mode (which won't
    // really matter anyway because the MMAL renderer always draws full-screen).
    if (qgetenv("DESKTOP_SESSION") == "LXDE-pi") {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Forcing windowed mode on LXDE-Pi");
        m_FullScreenFlag = 0;
    }
#endif

    // Check for validation errors/warnings and emit
    // signals for them, if appropriate
    bool ret = validateLaunch(testWindow);

    if (ret) {
        // Populate decoder-dependent properties.
        // Must be done after validateLaunch() since m_StreamConfig is finalized.
        ret = populateDecoderProperties(testWindow);
    }

    SDL_DestroyWindow(testWindow);

    if (!ret) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return false;
    }

    return true;
}

void Session::emitLaunchWarning(QString text)
{
    // Emit the warning to the UI
    emit displayLaunchWarning(text);

    // Wait a little bit so the user can actually read what we just said.
    // This wait is a little longer than the actual toast timeout (3 seconds)
    // to allow it to transition off the screen before continuing.
    uint32_t start = SDL_GetTicks();
    while (!SDL_TICKS_PASSED(SDL_GetTicks(), start + 3500)) {
        // Pump the UI loop while we wait
        SDL_Delay(5);
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        QCoreApplication::sendPostedEvents();
    }
}

bool Session::validateLaunch(SDL_Window* testWindow)
{
    QStringList warningList;

    if (m_Preferences->absoluteMouseMode && !m_App.isAppCollectorGame) {
        emitLaunchWarning(tr("Your selection to enable remote desktop mouse mode may cause problems in games."));
    }

    if (m_Preferences->videoDecoderSelection == StreamingPreferences::VDS_FORCE_SOFTWARE) {
        if (m_Preferences->videoCodecConfig == StreamingPreferences::VCC_FORCE_HEVC_HDR) {
            emitLaunchWarning(tr("HDR is not supported with software decoding."));
            m_StreamConfig.enableHdr = false;
        }
        else {
            emitLaunchWarning(tr("Your settings selection to force software decoding may cause poor streaming performance."));
        }
    }

    if (m_Preferences->unsupportedFps && m_StreamConfig.fps > 60) {
        emitLaunchWarning(tr("Using unsupported FPS options may cause stuttering or lag."));

        if (m_Preferences->enableVsync) {
            emitLaunchWarning(tr("V-sync will be disabled when streaming at a higher frame rate than the display."));
        }
    }

    if (m_StreamConfig.supportsHevc) {
        bool hevcForced = m_Preferences->videoCodecConfig == StreamingPreferences::VCC_FORCE_HEVC ||
                m_Preferences->videoCodecConfig == StreamingPreferences::VCC_FORCE_HEVC_HDR;

        if (m_Preferences->videoDecoderSelection == StreamingPreferences::VDS_AUTO && // Force hardware decoding checked below
                m_Preferences->videoCodecConfig != StreamingPreferences::VCC_AUTO && // Already checked in initialize()
                !isHardwareDecodeAvailable(testWindow,
                                           m_Preferences->videoDecoderSelection,
                                           VIDEO_FORMAT_H265,
                                           m_StreamConfig.width,
                                           m_StreamConfig.height,
                                           m_StreamConfig.fps)) {
            if (hevcForced) {
                emitLaunchWarning(tr("Using software decoding due to your selection to force HEVC without GPU support. This may cause poor streaming performance."));
            }
            else {
                emitLaunchWarning(tr("This PC's GPU doesn't support HEVC decoding."));
                m_StreamConfig.supportsHevc = false;
            }
        }

        if (hevcForced) {
            if (m_Computer->maxLumaPixelsHEVC == 0) {
                emitLaunchWarning(tr("Your host PC GPU doesn't support HEVC. "
                                     "A GeForce GTX 900-series (Maxwell) or later GPU is required for HEVC streaming."));

                // Moonlight-common-c will handle this case already, but we want
                // to set this explicitly here so we can do our hardware acceleration
                // check below.
                m_StreamConfig.supportsHevc = false;
            }
        }
    }

    if (m_StreamConfig.enableHdr) {
        // Turn HDR back off unless all criteria are met.
        m_StreamConfig.enableHdr = false;

        // Check that the app supports HDR
        if (!m_App.hdrSupported) {
            emitLaunchWarning(tr("%1 doesn't support HDR10.").arg(m_App.name));
        }
        // Check that the server GPU supports HDR
        else if (!(m_Computer->serverCodecModeSupport & 0x200)) {
            emitLaunchWarning(tr("Your host PC GPU doesn't support HDR streaming. "
                                 "A GeForce GTX 1000-series (Pascal) or later GPU is required for HDR streaming."));
        }
        else if (!isHardwareDecodeAvailable(testWindow,
                                            m_Preferences->videoDecoderSelection,
                                            VIDEO_FORMAT_H265_MAIN10,
                                            m_StreamConfig.width,
                                            m_StreamConfig.height,
                                            m_StreamConfig.fps)) {
            emitLaunchWarning(tr("This PC's GPU doesn't support HEVC Main10 decoding for HDR streaming."));
        }
        else {
            // TODO: Also validate display capabilites

            // Validation successful so HDR is good to go
            m_StreamConfig.enableHdr = true;
        }
    }

    if (m_StreamConfig.width >= 3840) {
        // Only allow 4K on GFE 3.x+
        if (m_Computer->gfeVersion.isEmpty() || m_Computer->gfeVersion.startsWith("2.")) {
            emitLaunchWarning(tr("GeForce Experience 3.0 or higher is required for 4K streaming."));

            m_StreamConfig.width = 1920;
            m_StreamConfig.height = 1080;
        }
    }

    // Test if audio works at the specified audio configuration
    bool audioTestPassed = testAudio(m_StreamConfig.audioConfiguration);

    // Gracefully degrade to stereo if surround sound doesn't work
    if (!audioTestPassed && CHANNEL_COUNT_FROM_AUDIO_CONFIGURATION(m_StreamConfig.audioConfiguration) > 2) {
        audioTestPassed = testAudio(AUDIO_CONFIGURATION_STEREO);
        if (audioTestPassed) {
            m_StreamConfig.audioConfiguration = AUDIO_CONFIGURATION_STEREO;
            emitLaunchWarning(tr("Your selected surround sound setting is not supported by the current audio device."));
        }
    }

    // If nothing worked, warn the user that audio will not work
    m_AudioDisabled = !audioTestPassed;
    if (m_AudioDisabled) {
        emitLaunchWarning(tr("Failed to open audio device. Audio will be unavailable during this session."));
    }

    // Check for unmapped gamepads
    if (!SdlInputHandler::getUnmappedGamepads().isEmpty()) {
        emitLaunchWarning(tr("An attached gamepad has no mapping and won't be usable. Visit the Moonlight help to resolve this."));
    }

    // NVENC will fail to initialize when using dimensions over 4096 and H.264.
    if (m_StreamConfig.width > 4096 || m_StreamConfig.height > 4096) {
        if (m_Computer->maxLumaPixelsHEVC == 0) {
            emit displayLaunchError(tr("Your host PC's GPU doesn't support streaming video resolutions over 4K."));
            return false;
        }
        else if (!m_StreamConfig.supportsHevc) {
            emit displayLaunchError(tr("Video resolutions over 4K are only supported by the HEVC codec."));
            return false;
        }
    }

    if (m_Preferences->videoDecoderSelection == StreamingPreferences::VDS_FORCE_HARDWARE &&
            !m_StreamConfig.enableHdr && // HEVC Main10 was already checked for hardware decode support above
            !isHardwareDecodeAvailable(testWindow,
                                       m_Preferences->videoDecoderSelection,
                                       m_StreamConfig.supportsHevc ? VIDEO_FORMAT_H265 : VIDEO_FORMAT_H264,
                                       m_StreamConfig.width,
                                       m_StreamConfig.height,
                                       m_StreamConfig.fps)) {
        if (m_Preferences->videoCodecConfig == StreamingPreferences::VCC_AUTO) {
            emit displayLaunchError(tr("Your selection to force hardware decoding cannot be satisfied due to missing hardware decoding support on this PC's GPU."));
        }
        else {
            emit displayLaunchError(tr("Your codec selection and force hardware decoding setting are not compatible. This PC's GPU lacks support for decoding your chosen codec."));
        }

        // Fail the launch, because we won't manage to get a decoder for the actual stream
        return false;
    }

    return true;
}

class DeferredSessionCleanupTask : public QRunnable
{
public:
    DeferredSessionCleanupTask(Session* session) :
        m_Session(session) {}

private:
    virtual ~DeferredSessionCleanupTask() override
    {
        // Allow another session to start now that we're cleaned up
        Session::s_ActiveSession = nullptr;
        Session::s_ActiveSessionSemaphore.release();
    }

    void run() override
    {
        // Only quit the running app if our session terminated gracefully
        bool shouldQuit =
                !m_Session->m_UnexpectedTermination &&
                m_Session->m_Preferences->quitAppAfter;

        // Notify the UI
        if (shouldQuit) {
            emit m_Session->quitStarting();
        }
        else {
            emit m_Session->sessionFinished(m_Session->m_PortTestResults);
        }

        // Finish cleanup of the connection state
        LiStopConnection();

        // Perform a best-effort app quit
        if (shouldQuit) {
            NvHTTP http(m_Session->m_Computer->activeAddress, m_Session->m_Computer->serverCert);

            // Logging is already done inside NvHTTP
            try {
                http.quitApp();
            } catch (const GfeHttpResponseException&) {
            } catch (const QtNetworkReplyException&) {
            }

            // Session is finished now
            emit m_Session->sessionFinished(m_Session->m_PortTestResults);
        }
    }

    Session* m_Session;
};

void Session::getWindowDimensions(int& x, int& y,
                                  int& width, int& height)
{
    int displayIndex = 0;
    bool fullScreen;

    if (m_Window != nullptr) {
        displayIndex = SDL_GetWindowDisplayIndex(m_Window);
        SDL_assert(displayIndex >= 0);
        fullScreen = (SDL_GetWindowFlags(m_Window) & SDL_WINDOW_FULLSCREEN);
    }

#ifndef Q_OS_WEBOS
    // Create our window on the same display that Qt's UI
    // was being displayed on.
    else {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Qt UI screen is at (%d,%d)",
                    m_DisplayOriginX, m_DisplayOriginY);
        for (int i = 0; i < SDL_GetNumVideoDisplays(); i++) {
            SDL_Rect displayBounds;

            if (SDL_GetDisplayBounds(i, &displayBounds) == 0) {
                if (displayBounds.x == m_DisplayOriginX &&
                        displayBounds.y == m_DisplayOriginY) {
                    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                                "SDL found matching display %d",
                                i);
                    displayIndex = i;
                    break;
                }
            }
            else {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "SDL_GetDisplayBounds(%d) failed: %s",
                            i, SDL_GetError());
            }
        }

        fullScreen = (m_Preferences->windowMode != StreamingPreferences::WM_WINDOWED);
    }

    SDL_Rect usableBounds;
    if (fullScreen && SDL_GetDisplayBounds(displayIndex, &usableBounds) == 0) {
        width = usableBounds.w;
        height = usableBounds.h;
    }
    else if (SDL_GetDisplayUsableBounds(displayIndex, &usableBounds) == 0) {
        width = usableBounds.w;
        height = usableBounds.h;

        if (m_Window != nullptr) {
            int top, left, bottom, right;

            if (SDL_GetWindowBordersSize(m_Window, &top, &left, &bottom, &right) == 0) {
                width -= left + right;
                height -= top + bottom;
            }
            else {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Unable to get window border size: %s",
                            SDL_GetError());
            }

            // If the stream window can fit within the usable drawing area with 1:1
            // scaling, do that rather than filling the screen.
            if (m_StreamConfig.width < width && m_StreamConfig.height < height) {
                width = m_StreamConfig.width;
                height = m_StreamConfig.height;
            }
        }
    }
    else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_GetDisplayUsableBounds() failed: %s",
                     SDL_GetError());

        width = m_StreamConfig.width;
        height = m_StreamConfig.height;
    }
#else
    width = 1920;
    height = 1080;
#endif
    x = y = SDL_WINDOWPOS_CENTERED_DISPLAY(displayIndex);
}

void Session::updateOptimalWindowDisplayMode()
{
    SDL_DisplayMode desktopMode, bestMode, mode;
    int displayIndex = SDL_GetWindowDisplayIndex(m_Window);

    // Try the current display mode first. On macOS, this will be the normal
    // scaled desktop resolution setting.
    if (SDL_GetDesktopDisplayMode(displayIndex, &desktopMode) == 0) {
        // If this doesn't fit the selected resolution, use the native
        // resolution of the panel (unscaled).
        if (desktopMode.w < m_ActiveVideoWidth || desktopMode.h < m_ActiveVideoHeight) {
            if (!StreamUtils::getRealDesktopMode(displayIndex, &desktopMode)) {
                return;
            }
        }
    }
    else {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "SDL_GetDesktopDisplayMode() failed: %s",
                    SDL_GetError());
        return;
    }

    // Start with the native desktop resolution and try to find
    // the highest refresh rate that our stream FPS evenly divides.
    bestMode = desktopMode;
    bestMode.refresh_rate = 0;
    for (int i = 0; i < SDL_GetNumDisplayModes(displayIndex); i++) {
        if (SDL_GetDisplayMode(displayIndex, i, &mode) == 0) {
            if (mode.w == desktopMode.w && mode.h == desktopMode.h &&
                    mode.refresh_rate % m_StreamConfig.fps == 0) {
                if (mode.refresh_rate > bestMode.refresh_rate) {
                    bestMode = mode;
                }
            }
        }
    }

    if (bestMode.refresh_rate == 0) {
        // We may find no match if the user has moved a 120 FPS
        // stream onto a 60 Hz monitor (since no refresh rate can
        // divide our FPS setting). We'll stick to the default in
        // this case.
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "No matching refresh rate found; using desktop mode");
        bestMode = desktopMode;
    }

    if ((SDL_GetWindowFlags(m_Window) & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN) {
        // Only print when the window is actually in full-screen exclusive mode,
        // otherwise we're not actually using the mode we've set here
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Chosen best display mode: %dx%dx%d",
                    bestMode.w, bestMode.h, bestMode.refresh_rate);
    }

    SDL_SetWindowDisplayMode(m_Window, &bestMode);
}

void Session::toggleFullscreen()
{
    bool fullScreen = !(SDL_GetWindowFlags(m_Window) & m_FullScreenFlag);

    if (fullScreen) {
        if (m_FullScreenFlag == SDL_WINDOW_FULLSCREEN && m_InputHandler->isCaptureActive()) {
            // Confine the cursor to the window if we're capturing input
            SDL_SetWindowGrab(m_Window, SDL_TRUE);
        }

#if SDL_VERSION_ATLEAST(2, 0, 5)
        SDL_SetWindowResizable(m_Window, SDL_FALSE);
#endif
        SDL_SetWindowFullscreen(m_Window, m_FullScreenFlag);
    }
    else {
        // Unconfine the cursor
        SDL_SetWindowGrab(m_Window, SDL_FALSE);

        SDL_SetWindowFullscreen(m_Window, 0);
#if SDL_VERSION_ATLEAST(2, 0, 5)
        SDL_SetWindowResizable(m_Window, SDL_TRUE);
#endif
        // Reposition the window when the resize is complete
        m_PendingWindowedTransition = true;
    }
}

void Session::notifyMouseEmulationMode(bool enabled)
{
    m_MouseEmulationRefCount += enabled ? 1 : -1;
    SDL_assert(m_MouseEmulationRefCount >= 0);

    // We re-use the status update overlay for mouse mode notification
    if (m_MouseEmulationRefCount > 0) {
        strcpy(m_OverlayManager.getOverlayText(Overlay::OverlayStatusUpdate), "Gamepad mouse mode active\nLong press Start to deactivate");
        m_OverlayManager.setOverlayTextUpdated(Overlay::OverlayStatusUpdate);
        m_OverlayManager.setOverlayState(Overlay::OverlayStatusUpdate, true);
    }
    else {
        m_OverlayManager.setOverlayState(Overlay::OverlayStatusUpdate, false);
    }
}

class AsyncConnectionStartThread : public QThread
{
public:
    AsyncConnectionStartThread(Session* session) :
        QThread(nullptr),
        m_Session(session) {}

    void run() override
    {
        m_Session->m_AsyncConnectionSuccess = m_Session->startConnectionAsync();
    }

    Session* m_Session;
};

// Called in a non-main thread
bool Session::startConnectionAsync()
{
    // Wait 1.5 seconds before connecting to let the user
    // have time to read any messages present on the segue
#ifdef USE_ASYNC_CONNECT_THREAD
    SDL_Delay(1500);
#else
    uint32_t start = SDL_GetTicks();
    while (!SDL_TICKS_PASSED(SDL_GetTicks(), start + 1500)) {
        // Pump the UI loop while we wait since we're not async
        SDL_Delay(5);
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        QCoreApplication::sendPostedEvents();
    }
#endif

    // The UI should have ensured the old game was already quit
    // if we decide to stream a different game.
    Q_ASSERT(m_Computer->currentGameId == 0 ||
             m_Computer->currentGameId == m_App.id);

    // SOPS will set all settings to 720p60 if it doesn't recognize
    // the chosen resolution. Avoid that by disabling SOPS when it
    // is not streaming a supported resolution.
    bool enableGameOptimizations = false;
    for (const NvDisplayMode &mode : m_Computer->displayModes) {
        if (mode.width == m_StreamConfig.width &&
                mode.height == m_StreamConfig.height) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Found host supported resolution: %dx%d",
                        mode.width, mode.height);
            enableGameOptimizations = m_Preferences->gameOptimizations;
            break;
        }
    }

    try {
        NvHTTP http(m_Computer->activeAddress, m_Computer->serverCert);
        if (m_Computer->currentGameId != 0) {
            http.resumeApp(&m_StreamConfig);
        }
        else {
            http.launchApp(m_App.id, &m_StreamConfig,
                           enableGameOptimizations,
                           m_Preferences->playAudioOnHost,
                           m_InputHandler->getAttachedGamepadMask());
        }
    } catch (const GfeHttpResponseException& e) {
        emit displayLaunchError(tr("GeForce Experience returned error: %1").arg(e.toQString()));
        return false;
    } catch (const QtNetworkReplyException& e) {
        emit displayLaunchError(e.toQString());
        return false;
    }

    QByteArray hostnameStr = m_Computer->activeAddress.toLatin1();
    QByteArray siAppVersion = m_Computer->appVersion.toLatin1();

    SERVER_INFORMATION hostInfo;
    hostInfo.address = hostnameStr.data();
    hostInfo.serverInfoAppVersion = siAppVersion.data();

    // Older GFE versions didn't have this field
    QByteArray siGfeVersion;
    if (!m_Computer->gfeVersion.isEmpty()) {
        siGfeVersion = m_Computer->gfeVersion.toLatin1();
    }
    if (!siGfeVersion.isEmpty()) {
        hostInfo.serverInfoGfeVersion = siGfeVersion.data();
    }

    if (m_Preferences->packetSize != 0) {
        // Override default packet size and remote streaming detection
        // NB: Using STREAM_CFG_AUTO will cap our packet size at 1024 for remote hosts.
        m_StreamConfig.streamingRemotely = STREAM_CFG_LOCAL;
        m_StreamConfig.packetSize = m_Preferences->packetSize;
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Using custom packet size: %d bytes",
                    m_Preferences->packetSize);
    }
    else {
        // isReachableOverVpn() does network I/O, so we only attempt to check
        // VPN reachability if we've already contacted the PC successfully
        if (m_Computer->isReachableOverVpn()) {
            // It looks like our route to this PC is over a VPN.
            // Treat it as remote even if the target address is in RFC 1918 address space.
            m_StreamConfig.streamingRemotely = STREAM_CFG_REMOTE;
            m_StreamConfig.packetSize = 1024;
        }
        else {
            m_StreamConfig.streamingRemotely = STREAM_CFG_AUTO;
            m_StreamConfig.packetSize = 1392;
        }
    }

    int err = LiStartConnection(&hostInfo, &m_StreamConfig, &k_ConnCallbacks,
                                &m_VideoCallbacks,
                                m_AudioDisabled ? nullptr : &m_AudioCallbacks,
                                NULL, 0, NULL, 0);
    if (err != 0) {
        // We already displayed an error dialog in the stage failure
        // listener.
        return false;
    }

    emit connectionStarted();
    return true;
}

void Session::exec(int displayOriginX, int displayOriginY)
{
    m_DisplayOriginX = displayOriginX;
    m_DisplayOriginY = displayOriginY;

    // Complete initialization in this deferred context to avoid
    // calling expensive functions in the constructor (during the
    // process of loading the StreamSegue).
    //
    // NB: This initializes the SDL video subsystem, so it must be
    // called on the main thread.
    if (!initialize()) {
        emit sessionFinished(0);
        return;
    }

    // Wait for any old session to finish cleanup
    s_ActiveSessionSemaphore.acquire();

    // We're now active
    s_ActiveSession = this;

    // Initialize the gamepad code with our preferences
    // NB: m_InputHandler must be initialize before starting the connection.
    m_InputHandler = new SdlInputHandler(*m_Preferences, m_Computer,
                                         m_StreamConfig.width,
                                         m_StreamConfig.height);

    // Kick off the async connection thread while we sit here and pump the event loop
    AsyncConnectionStartThread asyncConnThread(this);
#ifdef USE_ASYNC_CONNECT_THREAD
    asyncConnThread.start();
    while (!asyncConnThread.wait(10)) {
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        QCoreApplication::sendPostedEvents();
    }
#else
    asyncConnThread.run();
#endif

    // Pump the event loop one last time to ensure we pick up any events from
    // the thread that happened while it was in the final successful QThread::wait().
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    QCoreApplication::sendPostedEvents();

    // If the connection failed, clean up and abort the connection.
    if (!m_AsyncConnectionSuccess) {
        delete m_InputHandler;
        m_InputHandler = nullptr;
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        QThreadPool::globalInstance()->start(new DeferredSessionCleanupTask(this));
        return;
    }

    int x, y, width, height;
    getWindowDimensions(x, y, width, height);

#ifdef STEAM_LINK
    // We need a little delay before creating the window or we will trigger some kind
    // of graphics driver bug on Steam Link that causes a jagged overlay to appear in
    // the top right corner randomly.
    SDL_Delay(500);
#endif

    m_Window = SDL_CreateWindow("Moonlight",
                                x,
                                y,
                                width,
                                height,
                                SDL_WINDOW_ALLOW_HIGHDPI | StreamUtils::getPlatformWindowFlags());
    if (!m_Window) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "SDL_CreateWindow() failed with platform flags: %s",
                    SDL_GetError());

        m_Window = SDL_CreateWindow("Moonlight",
                                    x,
                                    y,
                                    width,
                                    height,
                                    SDL_WINDOW_ALLOW_HIGHDPI);
        if (!m_Window) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "SDL_CreateWindow() failed: %s",
                         SDL_GetError());

            delete m_InputHandler;
            m_InputHandler = nullptr;
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
            QThreadPool::globalInstance()->start(new DeferredSessionCleanupTask(this));
            return;
        }
    }

    m_InputHandler->setWindow(m_Window);

#ifndef Q_OS_WEBOS
    // webOS doesn't support SVG rendering
    QSvgRenderer svgIconRenderer(QString(":/res/moonlight.svg"));
    QImage svgImage(ICON_SIZE, ICON_SIZE, QImage::Format_RGBA8888);
    svgImage.fill(0);

    QPainter svgPainter(&svgImage);
    svgIconRenderer.render(&svgPainter);
    SDL_Surface* iconSurface = SDL_CreateRGBSurfaceWithFormatFrom((void*)svgImage.constBits(),
                                                                  svgImage.width(),
                                                                  svgImage.height(),
                                                                  32,
                                                                  4 * svgImage.width(),
                                                                  SDL_PIXELFORMAT_RGBA32);
#else
    SDL_Surface* iconSurface = nullptr;
#endif

#ifndef Q_OS_DARWIN
    // Other platforms seem to preserve our Qt icon when creating a new window.
    if (iconSurface != nullptr) {
        // This must be called before entering full-screen mode on Windows
        // or our icon will not persist when toggling to windowed mode
        SDL_SetWindowIcon(m_Window, iconSurface);
    }
#endif

#ifndef Q_OS_WEBOS
    // For non-full screen windows, call getWindowDimensions()
    // again after creating a window to allow it to account
    // for window chrome size.
    if (m_Preferences->windowMode == StreamingPreferences::WM_WINDOWED) {
        getWindowDimensions(x, y, width, height);

        // We must set the size before the position because centering
        // won't work unless it knows the final size of the window.
        SDL_SetWindowSize(m_Window, width, height);
        SDL_SetWindowPosition(m_Window, x, y);

        // Passing SDL_WINDOW_RESIZABLE to set this during window
        // creation causes our window to be full screen for some reason
        SDL_SetWindowResizable(m_Window, SDL_TRUE);
    }
    else {
        // Update the window display mode based on our current monitor
        updateOptimalWindowDisplayMode();
        // Enter full screen
        SDL_SetWindowFullscreen(m_Window, m_FullScreenFlag);
    }
#else
    SDL_SetWindowFullscreen(m_Window, m_FullScreenFlag);
#endif

    bool needsFirstEnterCapture = false;
    bool needsPostDecoderCreationCapture = false;

    // HACK: For Wayland, we wait until we get the first SDL_WINDOWEVENT_ENTER
    // event where it seems to work consistently on GNOME. For other platforms,
    // especially where SDL may call SDL_RecreateWindow(), we must only capture
    // after the decoder is created.
    if (strcmp(SDL_GetCurrentVideoDriver(), "wayland") == 0) {
        // Native Wayland: Capture on SDL_WINDOWEVENT_ENTER
        needsFirstEnterCapture = true;
    }
    else {
        // X11/XWayland: Capture after decoder creation
        needsPostDecoderCreationCapture = true;
    }

    // Stop text input. SDL enables it by default
    // when we initialize the video subsystem, but this
    // causes an IME popup when certain keys are held down
    // on macOS.
    SDL_StopTextInput();

    // Disable the screen saver
    SDL_DisableScreenSaver();

    // Hide Qt's fake mouse cursor on EGLFS systems
    if (QGuiApplication::platformName() == "eglfs") {
        QGuiApplication::setOverrideCursor(QCursor(Qt::BlankCursor));
    }

    // Set timer resolution to 1 ms on Windows for greater
    // sleep precision and more accurate callback timing.
    SDL_SetHint(SDL_HINT_TIMER_RESOLUTION, "1");

    int currentDisplayIndex = SDL_GetWindowDisplayIndex(m_Window);

    // Now that we're about to stream, any SDL_QUIT event is expected
    // unless it comes from the connection termination callback where
    // (m_UnexpectedTermination is set back to true).
    m_UnexpectedTermination = false;

    // Start rich presence to indicate we're in game
    RichPresenceManager presence(*m_Preferences, m_App.name);

    // Hijack this thread to be the SDL main thread. We have to do this
    // because we want to suspend all Qt processing until the stream is over.
    SDL_Event event;
    for (;;) {
        // We explicitly use SDL_PollEvent() and SDL_Delay() because
        // SDL_WaitEvent() has an internal SDL_Delay(10) inside which
        // blocks this thread too long for high polling rate mice and high
        // refresh rate displays.
        if (!SDL_PollEvent(&event)) {
#ifndef STEAM_LINK
            SDL_Delay(1);
#else
            // Waking every 1 ms to process input is too much for the low performance
            // ARM core in the Steam Link, so we will wait 10 ms instead.
            SDL_Delay(10);
#endif
            presence.runCallbacks();
            continue;
        }
        switch (event.type) {
        case SDL_QUIT:
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Quit event received");
            goto DispatchDeferredCleanup;

        case SDL_USEREVENT:
            switch (event.user.code) {
            case SDL_CODE_FRAME_READY:
                m_VideoDecoder->renderFrameOnMainThread();
                break;
            case SDL_CODE_HIDE_CURSOR:
                SDL_ShowCursor(SDL_DISABLE);
                break;
            case SDL_CODE_SHOW_CURSOR:
                SDL_ShowCursor(SDL_ENABLE);
                break;
            default:
                SDL_assert(false);
            }
            break;

        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                m_InputHandler->notifyFocusLost();
            }
            else if (event.window.event == SDL_WINDOWEVENT_LEAVE) {
                m_InputHandler->notifyMouseLeave();
            }

            // Capture the mouse on SDL_WINDOWEVENT_ENTER if needed
            if (needsFirstEnterCapture && event.window.event == SDL_WINDOWEVENT_ENTER) {
                m_InputHandler->setCaptureActive(true);
                needsFirstEnterCapture = false;
            }

            // We want to recreate the decoder for resizes (full-screen toggles) and the initial shown event.
            // We use SDL_WINDOWEVENT_SIZE_CHANGED rather than SDL_WINDOWEVENT_RESIZED because the latter doesn't
            // seem to fire when switching from windowed to full-screen on X11.
            if (event.window.event != SDL_WINDOWEVENT_SIZE_CHANGED && event.window.event != SDL_WINDOWEVENT_SHOWN) {
                // Check that the window display hasn't changed. If it has, we want
                // to recreate the decoder to allow it to adapt to the new display.
                // This will allow Pacer to pull the new display refresh rate.
                if (SDL_GetWindowDisplayIndex(m_Window) == currentDisplayIndex) {
                    break;
                }
            }

            // Complete any repositioning that was deferred until
            // the resize from full-screen to windowed had completed.
            // If we try to do this immediately, the resize won't take effect
            // properly on Windows.
            if (m_PendingWindowedTransition) {
                m_PendingWindowedTransition = false;

                int x, y, width, height;
                getWindowDimensions(x, y, width, height);

                SDL_SetWindowSize(m_Window, width, height);
                SDL_SetWindowPosition(m_Window, x, y);
            }

            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Recreating renderer for window event: %d (%d %d)",
                        event.window.event,
                        event.window.data1,
                        event.window.data2);

            // Fall through
        case SDL_RENDER_DEVICE_RESET:
        case SDL_RENDER_TARGETS_RESET:

            SDL_AtomicLock(&m_DecoderLock);

            // Destroy the old decoder
            delete m_VideoDecoder;

            // Flush any other pending window events that could
            // send us back here immediately
            SDL_PumpEvents();
            SDL_FlushEvent(SDL_WINDOWEVENT);

            // Update the window display mode based on our current monitor
            currentDisplayIndex = SDL_GetWindowDisplayIndex(m_Window);
            updateOptimalWindowDisplayMode();

            // Now that the old decoder is dead, flush any events it may
            // have queued to reset itself (if this reset was the result
            // of state loss).
            SDL_PumpEvents();
            SDL_FlushEvent(SDL_RENDER_DEVICE_RESET);
            SDL_FlushEvent(SDL_RENDER_TARGETS_RESET);

            {
                // If the stream exceeds the display refresh rate (plus some slack),
                // forcefully disable V-sync to allow the stream to render faster
                // than the display.
                int displayHz = StreamUtils::getDisplayRefreshRate(m_Window);
                bool enableVsync = m_Preferences->enableVsync;
                if (displayHz + 5 < m_StreamConfig.fps) {
                    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                                "Disabling V-sync because refresh rate limit exceeded");
                    enableVsync = false;
                }

                // Choose a new decoder (hopefully the same one, but possibly
                // not if a GPU was removed or something).
                if (!chooseDecoder(m_Preferences->videoDecoderSelection,
                                   m_Window, m_ActiveVideoFormat, m_ActiveVideoWidth,
                                   m_ActiveVideoHeight, m_ActiveVideoFrameRate,
                                   enableVsync,
                                   enableVsync && m_Preferences->framePacing,
                                   false,
                                   s_ActiveSession->m_VideoDecoder)) {
                    SDL_AtomicUnlock(&m_DecoderLock);
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                                 "Failed to recreate decoder after reset");
                    emit displayLaunchError(tr("Unable to initialize video decoder. Please check your streaming settings and try again."));
                    goto DispatchDeferredCleanup;
                }

                // As of SDL 2.0.12, SDL_RecreateWindow() doesn't carry over mouse capture
                // or mouse hiding state to the new window. By capturing after the decoder
                // is set up, this ensures the window re-creation is already done.
                if (needsPostDecoderCreationCapture) {
                    m_InputHandler->setCaptureActive(true);
                    needsFirstEnterCapture = false;
                }
            }

            // Request an IDR frame to complete the reset
            m_NeedsIdr = true;

            SDL_AtomicUnlock(&m_DecoderLock);
            break;

        case SDL_KEYUP:
        case SDL_KEYDOWN:
            m_InputHandler->handleKeyEvent(&event.key);
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            m_InputHandler->handleMouseButtonEvent(&event.button);
            break;
        case SDL_MOUSEMOTION:
            m_InputHandler->handleMouseMotionEvent(&event.motion);
            break;
        case SDL_MOUSEWHEEL:
            m_InputHandler->handleMouseWheelEvent(&event.wheel);
            break;
        case SDL_CONTROLLERAXISMOTION:
            m_InputHandler->handleControllerAxisEvent(&event.caxis);
            break;
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP:
            m_InputHandler->handleControllerButtonEvent(&event.cbutton);
            break;
        case SDL_CONTROLLERDEVICEADDED:
        case SDL_CONTROLLERDEVICEREMOVED:
            m_InputHandler->handleControllerDeviceEvent(&event.cdevice);
            break;
        case SDL_JOYDEVICEADDED:
            m_InputHandler->handleJoystickArrivalEvent(&event.jdevice);
            break;
        case SDL_FINGERDOWN:
        case SDL_FINGERMOTION:
        case SDL_FINGERUP:
            m_InputHandler->handleTouchFingerEvent(&event.tfinger);
            break;
        }
    }

DispatchDeferredCleanup:
    // Uncapture the mouse and hide the window immediately,
    // so we can return to the Qt GUI ASAP.
    m_InputHandler->setCaptureActive(false);
    SDL_EnableScreenSaver();
    SDL_SetHint(SDL_HINT_TIMER_RESOLUTION, "0");
    if (QGuiApplication::platformName() == "eglfs") {
        QGuiApplication::restoreOverrideCursor();
    }

    // Raise any keys that are still down
    m_InputHandler->raiseAllKeys();

    // Destroy the input handler now. Any rumble callbacks that
    // occur after this point will be discarded. This must be destroyed
    // before allow the UI to continue execution or it could interfere
    // with SDLGamepadKeyNavigation.
    SDL_AtomicLock(&m_InputHandlerLock);
    delete m_InputHandler;
    m_InputHandler = nullptr;
    SDL_AtomicUnlock(&m_InputHandlerLock);

    // Destroy the decoder, since this must be done on the main thread
    SDL_AtomicLock(&m_DecoderLock);
    delete m_VideoDecoder;
    m_VideoDecoder = nullptr;
    SDL_AtomicUnlock(&m_DecoderLock);

    // This must be called after the decoder is deleted, because
    // the renderer may want to interact with the window
    SDL_DestroyWindow(m_Window);

    if (iconSurface != nullptr) {
        SDL_FreeSurface(iconSurface);
    }

    SDL_QuitSubSystem(SDL_INIT_VIDEO);

    // Cleanup can take a while, so dispatch it to a worker thread.
    // When it is complete, it will release our s_ActiveSessionSemaphore
    // reference.
    QThreadPool::globalInstance()->start(new DeferredSessionCleanupTask(this));
}

