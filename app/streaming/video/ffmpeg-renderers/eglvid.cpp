// vim: noai:ts=4:sw=4:softtabstop=4:expandtab
#include "eglvid.h"

#include "path.h"
#include "streaming/session.h"
#include "streaming/streamutils.h"

#include <QDir>

#include <Limelight.h>
#include <unistd.h>

#include <SDL_egl.h>
#include <SDL_hints.h>
#include <SDL_opengles2.h>
#include <SDL_render.h>
#include <SDL_syswm.h>

#ifndef EGL_VERSION_1_5
typedef intptr_t EGLAttrib;
#endif

#if !defined(EGL_VERSION_1_5) || !defined(EGL_EGL_PROTOTYPES)
typedef EGLDisplay (EGLAPIENTRYP PFNEGLGETPLATFORMDISPLAYPROC) (EGLenum platform, void *native_display, const EGLAttrib *attrib_list);
#endif

#ifndef EGL_EXT_platform_base
typedef EGLDisplay (EGLAPIENTRYP PFNEGLGETPLATFORMDISPLAYEXTPROC) (EGLenum platform, void *native_display, const EGLint *attrib_list);
#endif

// These are EGL extensions, so some platform headers may not provide them
#ifndef EGL_PLATFORM_WAYLAND_KHR
#define EGL_PLATFORM_WAYLAND_KHR 0x31D8
#endif
#ifndef EGL_PLATFORM_X11_KHR
#define EGL_PLATFORM_X11_KHR 0x31D5
#endif

/* TODO:
 *  - handle more pixel formats
 *  - handle software decoding
 */

/* DOC/misc:
 *  - https://kernel-recipes.org/en/2016/talks/video-and-colorspaces/
 *  - http://www.brucelindbloom.com/
 *  - https://learnopengl.com/Getting-started/Shaders
 *  - https://github.com/stunpix/yuvit
 *  - https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.601_conversion
 *  - https://www.renesas.com/eu/en/www/doc/application-note/an9717.pdf
 *  - https://www.xilinx.com/support/documentation/application_notes/xapp283.pdf
 *  - https://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.709-6-201506-I!!PDF-E.pdf
 *  - https://www.khronos.org/registry/OpenGL/extensions/OES/OES_EGL_image_external.txt
 *  - https://gist.github.com/rexguo/6696123
 *  - https://wiki.libsdl.org/CategoryVideo
 */

#define EGL_LOG(Category, ...) SDL_Log ## Category(\
        SDL_LOG_CATEGORY_APPLICATION, \
        "EGLRenderer: " __VA_ARGS__)

SDL_Window* EGLRenderer::s_LastFailedWindow = nullptr;

EGLRenderer::EGLRenderer(IFFmpegRenderer *backendRenderer)
    :
        m_SwPixelFormat(AV_PIX_FMT_NONE),
        m_EGLDisplay(EGL_NO_DISPLAY),
        m_Textures{0},
        m_ShaderProgram(0),
        m_Context(0),
        m_Window(nullptr),
        m_Backend(backendRenderer),
        m_VAO(0),
        m_ColorSpace(AVCOL_SPC_NB),
        m_ColorFull(false),
        m_BlockingSwapBuffers(false),
        m_glEGLImageTargetTexture2DOES(nullptr),
        m_glGenVertexArraysOES(nullptr),
        m_glBindVertexArrayOES(nullptr),
        m_glDeleteVertexArraysOES(nullptr),
        m_DummyRenderer(nullptr)
{
    SDL_assert(backendRenderer);
    SDL_assert(backendRenderer->canExportEGL());

    // Save these global parameters so we can restore them in our destructor
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &m_OldContextProfileMask);
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &m_OldContextMajorVersion);
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &m_OldContextMinorVersion);
}

EGLRenderer::~EGLRenderer()
{
    if (m_Context) {
        // Reattach the GL context to the main thread for destruction
        SDL_GL_MakeCurrent(m_Window, m_Context);
        if (m_ShaderProgram) {
            glDeleteProgram(m_ShaderProgram);
        }
        if (m_VAO) {
            SDL_assert(m_glDeleteVertexArraysOES != nullptr);
            m_glDeleteVertexArraysOES(1, &m_VAO);
        }
        SDL_GL_DeleteContext(m_Context);
    }

    if (m_DummyRenderer) {
        SDL_DestroyRenderer(m_DummyRenderer);
    }
#if SDL_VERSION_ATLEAST(2, 0, 6)
    // Reset the global properties back to what they were before
    SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER, "0");
#endif
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, m_OldContextProfileMask);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, m_OldContextMajorVersion);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, m_OldContextMinorVersion);
}

bool EGLRenderer::prepareDecoderContext(AVCodecContext*, AVDictionary**)
{
    /* Nothing to do */

    EGL_LOG(Info, "Using EGL renderer");

    return true;
}

void EGLRenderer::notifyOverlayUpdated(Overlay::OverlayType)
{
    // TODO: FIXME
}

bool EGLRenderer::isPixelFormatSupported(int, AVPixelFormat pixelFormat)
{
    // Remember to keep this in sync with EGLRenderer::renderFrame()!
    switch (pixelFormat)
    {
    case AV_PIX_FMT_NV12:
        return true;
    default:
        return false;
    }
}

int EGLRenderer::loadAndBuildShader(int shaderType,
                                    const char *file) {
    GLuint shader = glCreateShader(shaderType);
    if (!shader || shader == GL_INVALID_ENUM) {
        EGL_LOG(Error, "Can't create shader: %d", glGetError());
        return 0;
    }

    auto sourceData = Path::readDataFile(file);
    GLint len = sourceData.size();
    const char *buf = sourceData.data();

    glShaderSource(shader, 1, &buf, &len);
    glCompileShader(shader);
    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
        char shaderLog[512];
        glGetShaderInfoLog(shader, sizeof (shaderLog), nullptr, shaderLog);
        EGL_LOG(Error, "Cannot load shader \"%s\": %s", file, shaderLog);
        return 0;
    }

    return shader;
}

bool EGLRenderer::openDisplay(unsigned int platform, void* nativeDisplay)
{
    PFNEGLGETPLATFORMDISPLAYPROC eglGetPlatformDisplayProc;
    PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXTProc;

    m_EGLDisplay = EGL_NO_DISPLAY;

    // NB: eglGetPlatformDisplay() and eglGetPlatformDisplayEXT() have slightly different definitions
    eglGetPlatformDisplayProc = (typeof(eglGetPlatformDisplayProc))eglGetProcAddress("eglGetPlatformDisplay");
    eglGetPlatformDisplayEXTProc = (typeof(eglGetPlatformDisplayEXTProc))eglGetProcAddress("eglGetPlatformDisplayEXT");

    if (m_EGLDisplay == EGL_NO_DISPLAY) {
        // eglGetPlatformDisplay() is part of the EGL 1.5 core specification
        if (eglGetPlatformDisplayProc != nullptr) {
            m_EGLDisplay = eglGetPlatformDisplayProc(platform, nativeDisplay, nullptr);
            if (m_EGLDisplay == EGL_NO_DISPLAY) {
                EGL_LOG(Warn, "eglGetPlatformDisplay() failed: %d", eglGetError());
            }
        }
    }

    if (m_EGLDisplay == EGL_NO_DISPLAY) {
        // eglGetPlatformDisplayEXT() is an extension for EGL 1.4
        const EGLExtensions eglExtensions(EGL_NO_DISPLAY);
        if (eglExtensions.isSupported("EGL_EXT_platform_base")) {
            if (eglGetPlatformDisplayEXTProc != nullptr) {
                m_EGLDisplay = eglGetPlatformDisplayEXTProc(platform, nativeDisplay, nullptr);
                if (m_EGLDisplay == EGL_NO_DISPLAY) {
                    EGL_LOG(Warn, "eglGetPlatformDisplayEXT() failed: %d", eglGetError());
                }
            }
            else {
                EGL_LOG(Warn, "EGL_EXT_platform_base supported but no eglGetPlatformDisplayEXT() export!");
            }
        }
    }

    if (m_EGLDisplay == EGL_NO_DISPLAY) {
        // Finally, if all else fails, use eglGetDisplay()
        m_EGLDisplay = eglGetDisplay((EGLNativeDisplayType)nativeDisplay);
        if (m_EGLDisplay == EGL_NO_DISPLAY) {
            EGL_LOG(Error, "eglGetDisplay() failed: %d", eglGetError());
        }
    }

    return m_EGLDisplay != EGL_NO_DISPLAY;
}

bool EGLRenderer::compileShader() {
    SDL_assert(!m_ShaderProgram);
    SDL_assert(m_SwPixelFormat != AV_PIX_FMT_NONE);

    // XXX: TODO: other formats
    SDL_assert(m_SwPixelFormat == AV_PIX_FMT_NV12);

    bool ret = false;

    GLuint vertexShader = loadAndBuildShader(GL_VERTEX_SHADER, "egl.vert");
    if (!vertexShader)
        return false;

    GLuint fragmentShader = loadAndBuildShader(GL_FRAGMENT_SHADER, "egl.frag");
    if (!fragmentShader)
        goto fragError;

    m_ShaderProgram = glCreateProgram();
    if (!m_ShaderProgram) {
        EGL_LOG(Error, "Cannot create shader program");
        goto progFailCreate;
    }

    glAttachShader(m_ShaderProgram, vertexShader);
    glAttachShader(m_ShaderProgram, fragmentShader);
    glLinkProgram(m_ShaderProgram);
    int status;
    glGetProgramiv(m_ShaderProgram, GL_LINK_STATUS, &status);
    if (status) {
        ret = true;
    } else {
        char shader_log[512];
        glGetProgramInfoLog(m_ShaderProgram, sizeof (shader_log), nullptr, shader_log);
        EGL_LOG(Error, "Cannot link shader program: %s", shader_log);
        glDeleteProgram(m_ShaderProgram);
        m_ShaderProgram = 0;
    }

progFailCreate:
    glDeleteShader(fragmentShader);
fragError:
    glDeleteShader(vertexShader);
    return ret;
}

bool EGLRenderer::initialize(PDECODER_PARAMETERS params)
{
    m_Window = params->window;

    if (params->videoFormat == VIDEO_FORMAT_H265_MAIN10) {
        // EGL doesn't support rendering YUV 10-bit textures yet
        return false;
    }

    // It's not safe to attempt to opportunistically create a GLES2
    // renderer prior to 2.0.10. If GLES2 isn't available, SDL will
    // attempt to dereference a null pointer and crash Moonlight.
    // https://bugzilla.libsdl.org/show_bug.cgi?id=4350
    // https://hg.libsdl.org/SDL/rev/84618d571795
    if (!SDL_VERSION_ATLEAST(2, 0, 10)) {
        EGL_LOG(Error, "Not supported until SDL 2.0.10");
        return false;
    }

    // HACK: Work around bug where renderer will repeatedly fail with:
    // SDL_CreateRenderer() failed: Could not create GLES window surface
    if (m_Window == s_LastFailedWindow) {
        EGL_LOG(Error, "SDL_CreateRenderer() already failed on this window!");
        return false;
    }

#if SDL_VERSION_ATLEAST(2, 0, 6)
    /*
     * Create a dummy renderer in order to craft an accelerated SDL Window.
     * Request opengl ES 3.0 context, otherwise it will SIGSEGV
     * https://gitlab.freedesktop.org/mesa/mesa/issues/1011
     */
    SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER, "1");
#endif
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    int renderIndex;
    int maxRenderers = SDL_GetNumRenderDrivers();
    SDL_assert(maxRenderers >= 0);

    SDL_RendererInfo renderInfo;
    for (renderIndex = 0; renderIndex < maxRenderers; ++renderIndex) {
        if (SDL_GetRenderDriverInfo(renderIndex, &renderInfo))
            continue;
        if (!strcmp(renderInfo.name, "opengles2")) {
            SDL_assert(renderInfo.flags & SDL_RENDERER_ACCELERATED);
            break;
        }
    }
    if (renderIndex == maxRenderers) {
        EGL_LOG(Error, "Could not find a suitable SDL_Renderer");
        return false;
    }

    if (!(m_DummyRenderer = SDL_CreateRenderer(m_Window, renderIndex, SDL_RENDERER_ACCELERATED))) {
        EGL_LOG(Error, "SDL_CreateRenderer() failed: %s", SDL_GetError());
        s_LastFailedWindow = m_Window;
        return false;
    }

    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(params->window, &info)) {
        EGL_LOG(Error, "SDL_GetWindowWMInfo() failed: %s", SDL_GetError());
        return false;
    }
    switch (info.subsystem) {
#ifdef SDL_VIDEO_DRIVER_WAYLAND
    case SDL_SYSWM_WAYLAND:
        if (!openDisplay(EGL_PLATFORM_WAYLAND_KHR, info.info.wl.display)) {
            return false;
        }
        break;
#endif
#ifdef SDL_VIDEO_DRIVER_X11
    case SDL_SYSWM_X11:
        if (!openDisplay(EGL_PLATFORM_X11_KHR, info.info.x11.display)) {
            return false;
        }
        break;
#endif
    default:
        EGL_LOG(Error, "not compatible with SYSWM");
        return false;
    }

    if (m_EGLDisplay == EGL_NO_DISPLAY) {
        EGL_LOG(Error, "Cannot get EGL display: %d", eglGetError());
        return false;
    }

    if (!(m_Context = SDL_GL_CreateContext(params->window))) {
        EGL_LOG(Error, "Cannot create OpenGL context: %s", SDL_GetError());
        return false;
    }
    if (SDL_GL_MakeCurrent(params->window, m_Context)) {
        EGL_LOG(Error, "Cannot use created EGL context: %s", SDL_GetError());
        return false;
    }

    const EGLExtensions eglExtensions(m_EGLDisplay);
    if (!eglExtensions.isSupported("EGL_KHR_image_base") &&
        !eglExtensions.isSupported("EGL_KHR_image")) {
        EGL_LOG(Error, "EGL_KHR_image unsupported");
        return false;
    }
    else if (!SDL_GL_ExtensionSupported("GL_OES_EGL_image")) {
        EGL_LOG(Error, "GL_OES_EGL_image unsupported");
        return false;
    }

    if (!m_Backend->initializeEGL(m_EGLDisplay, eglExtensions))
        return false;

    if (!(m_glEGLImageTargetTexture2DOES = (typeof(m_glEGLImageTargetTexture2DOES))eglGetProcAddress("glEGLImageTargetTexture2DOES"))) {
        EGL_LOG(Error,
                "EGL: cannot retrieve `glEGLImageTargetTexture2DOES` address");
        return false;
    }

    // Vertex arrays are an extension on OpenGL ES 2.0
    if (SDL_GL_ExtensionSupported("GL_OES_vertex_array_object")) {
        m_glGenVertexArraysOES = (typeof(m_glGenVertexArraysOES))eglGetProcAddress("glGenVertexArraysOES");
        m_glBindVertexArrayOES = (typeof(m_glBindVertexArrayOES))eglGetProcAddress("glBindVertexArrayOES");
        m_glDeleteVertexArraysOES = (typeof(m_glDeleteVertexArraysOES))eglGetProcAddress("glDeleteVertexArraysOES");
    }
    else {
        // They are included in OpenGL ES 3.0 as part of the standard
        m_glGenVertexArraysOES = (typeof(m_glGenVertexArraysOES))eglGetProcAddress("glGenVertexArrays");
        m_glBindVertexArrayOES = (typeof(m_glBindVertexArrayOES))eglGetProcAddress("glBindVertexArray");
        m_glDeleteVertexArraysOES = (typeof(m_glDeleteVertexArraysOES))eglGetProcAddress("glDeleteVertexArrays");
    }

    if (!m_glGenVertexArraysOES || !m_glBindVertexArrayOES || !m_glDeleteVertexArraysOES) {
        EGL_LOG(Error, "Failed to find VAO functions");
        return false;
    }

    /* Compute the video region size in order to keep the aspect ratio of the
     * video stream.
     */
    SDL_Rect src, dst;
    src.x = src.y = dst.x = dst.y = 0;
    src.w = params->width;
    src.h = params->height;
    SDL_GetWindowSize(m_Window, &dst.w, &dst.h);
    StreamUtils::scaleSourceToDestinationSurface(&src, &dst);

    glViewport(dst.x, dst.y, dst.w, dst.h);

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    if (params->enableVsync) {
        SDL_GL_SetSwapInterval(1);
        m_BlockingSwapBuffers = true;
    } else {
        SDL_GL_SetSwapInterval(0);
    }

    SDL_GL_SwapWindow(params->window);

    glGenTextures(EGL_MAX_PLANES, m_Textures);
    for (size_t i = 0; i < EGL_MAX_PLANES; ++i) {
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_Textures[i]);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    GLenum err = glGetError();
    if (err != GL_NO_ERROR)
        EGL_LOG(Error, "OpenGL error: %d", err);

    // Detach the context from this thread, so the render thread can attach it
    SDL_GL_MakeCurrent(m_Window, nullptr);

    return err == GL_NO_ERROR;
}

const float *EGLRenderer::getColorMatrix() {
    /* The conversion matrices are shamelessly stolen from linux:
     * drivers/media/platform/imx-pxp.c:pxp_setup_csc
     */
    static const float bt601Lim[] = {
        1.1644f, 1.1644f, 1.1644f,
        0.0f, -0.3917f, 2.0172f,
        1.5960f, -0.8129f, 0.0f
    };
    static const float bt601Full[] = {
        1.0f, 1.0f, 1.0f,
        0.0f, -0.3441f, 1.7720f,
        1.4020f, -0.7141f, 0.0f
    };
    static const float bt709Lim[] = {
        1.1644f, 1.1644f, 1.1644f,
        0.0f, -0.2132f, 2.1124f,
        1.7927f, -0.5329f, 0.0f
    };
    static const float bt709Full[] = {
        1.0f, 1.0f, 1.0f,
        0.0f, -0.1873f, 1.8556f,
        1.5748f, -0.4681f, 0.0f
    };
    static const float bt2020Lim[] = {
        1.1644f, 1.1644f, 1.1644f,
        0.0f, -0.1874f, 2.1418f,
        1.6781f, -0.6505f, 0.0f
    };
    static const float bt2020Full[] = {
        1.0f, 1.0f, 1.0f,
        0.0f, -0.1646f, 1.8814f,
        1.4746f, -0.5714f, 0.0f
    };

    switch (m_ColorSpace) {
        case AVCOL_SPC_SMPTE170M:
        case AVCOL_SPC_BT470BG:
            EGL_LOG(Info, "BT-601 pixels");
            return m_ColorFull ? bt601Full : bt601Lim;
        case AVCOL_SPC_BT709:
            EGL_LOG(Info, "BT-709 pixels");
            return m_ColorFull ? bt709Full : bt709Lim;
        case AVCOL_SPC_BT2020_NCL:
        case AVCOL_SPC_BT2020_CL:
            EGL_LOG(Info, "BT-2020 pixels");
            return m_ColorFull ? bt2020Full : bt2020Lim;
    };
    EGL_LOG(Warn, "unknown color space: %d, falling back to BT-601",
            m_ColorSpace);
    return bt601Lim;
}

bool EGLRenderer::specialize() {
    SDL_assert(!m_VAO);

    // Attach our GL context to the render thread
    SDL_GL_MakeCurrent(m_Window, m_Context);

    if (!compileShader())
        return false;

    // The viewport should have the aspect ratio of the video stream
    static const float vertices[] = {
        // pos .... // tex coords
        1.0f, 1.0f, 1.0f, 0.0f,
        1.0f, -1.0f, 1.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 1.0f,
        -1.0f, 1.0f, 0.0f, 0.0f,

    };
    static const unsigned int indices[] = {
        0, 1, 3,
        1, 2, 3,
    };

    glUseProgram(m_ShaderProgram);

    unsigned int VBO, EBO;
    m_glGenVertexArraysOES(1, &m_VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    m_glBindVertexArrayOES(m_VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof (vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof (indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof (float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    m_glBindVertexArrayOES(0);

    int yuvmatLocation = glGetUniformLocation(m_ShaderProgram, "yuvmat");
    glUniformMatrix3fv(yuvmatLocation, 1, GL_FALSE, getColorMatrix());

    static const float limitedOffsets[] = { 16.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f };
    static const float fullOffsets[] = { 0.0f, 128.0f / 255.0f, 128.0f / 255.0f };

    int offLocation = glGetUniformLocation(m_ShaderProgram, "offset");
    glUniform3fv(offLocation, 1, m_ColorFull ? fullOffsets : limitedOffsets);

    int colorPlane = glGetUniformLocation(m_ShaderProgram, "plane1");
    glUniform1i(colorPlane, 0);
    colorPlane = glGetUniformLocation(m_ShaderProgram, "plane2");
    glUniform1i(colorPlane, 1);

    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        EGL_LOG(Error, "OpenGL error: %d", err);
    }

    return err == GL_NO_ERROR;
}

void EGLRenderer::renderFrame(AVFrame* frame)
{
    EGLImage imgs[EGL_MAX_PLANES];

    if (frame == nullptr) {
        // End of stream - unbind the GL context
        SDL_GL_MakeCurrent(m_Window, nullptr);
        return;
    }

    if (frame->hw_frames_ctx != nullptr) {
        // Find the native read-back format and load the shader
        if (m_SwPixelFormat == AV_PIX_FMT_NONE) {
            auto hwFrameCtx = (AVHWFramesContext*)frame->hw_frames_ctx->data;

            m_SwPixelFormat = hwFrameCtx->sw_format;
            SDL_assert(m_SwPixelFormat != AV_PIX_FMT_NONE);

            EGL_LOG(Info, "Selected read-back format: %d", m_SwPixelFormat);

            // XXX: TODO: Handle other pixel formats
            SDL_assert(m_SwPixelFormat == AV_PIX_FMT_NV12);
            m_ColorSpace = frame->colorspace;
            m_ColorFull = frame->color_range == AVCOL_RANGE_JPEG;

            if (!specialize()) {
                m_SwPixelFormat = AV_PIX_FMT_NONE;
                return;
            }
        }

        ssize_t plane_count = m_Backend->exportEGLImages(frame, m_EGLDisplay, imgs);
        if (plane_count < 0)
            return;
        for (ssize_t i = 0; i < plane_count; ++i) {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_Textures[i]);
            m_glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, imgs[i]);
        }
    } else {
        // TODO: load texture for SW decoding ?
        EGL_LOG(Error, "EGL rendering only supports hw frames");
        return;
    }

    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(m_ShaderProgram);
    m_glBindVertexArrayOES(m_VAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    SDL_GL_SwapWindow(m_Window);

    if (m_BlockingSwapBuffers) {
        // This glClear() forces us to block until the buffer swap is
        // complete to continue rendering. Mesa won't actually wait
        // for the swap with just glFinish() alone. Waiting here keeps us
        // in lock step with the display refresh rate. If we don't wait
        // here, we'll stall on the first GL call next frame. Doing the
        // wait here instead allows more time for a newer frame to arrive
        // for next renderFrame() call.
        glClear(GL_COLOR_BUFFER_BIT);
        glFinish();
    }

    if (frame->hw_frames_ctx != nullptr)
        m_Backend->freeEGLImages(m_EGLDisplay, imgs);
}
