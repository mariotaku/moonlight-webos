cmake_minimum_required(VERSION 3.6)

set(ENV{PKG_CONFIG_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/libs/webos/lib/pkgconfig:$ENV{PKG_CONFIG_PATH})
set(CMAKE_INCLUDE_CURRENT_DIR ON)

project(moonlight-webos LANGUAGES C CXX)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)

option(BUILD_SHARED_LIBS "Build static libs instead of shared" OFF)
add_subdirectory(moonlight-common-c)
add_subdirectory(qmdnsengine)
add_subdirectory(opus)
add_subdirectory(libs/webos)

# the `pkg_check_modules` function is created with this call
find_package(PkgConfig REQUIRED) 
pkg_check_modules(SDL2 REQUIRED sdl2 SDL2_ttf)
pkg_check_modules(OPENSSL REQUIRED openssl)
pkg_check_modules(WAYLAND wayland-client)
pkg_check_modules(EGL egl glesv2)
# pkg_check_modules(FFMPEG REQUIRED libavcodec>=58 libavutil>=56)
# pkg_check_modules(GST gstreamer-1.0 gstreamer-base-1.0 gstreamer-app-1.0)

# these calls create special `PkgConfig::<MODULE>` variables
find_package(Qt5 COMPONENTS Core Multimedia Quick REQUIRED)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/pkg_$ENV{ARCH}/")
set(EXECUTABLE_NAME "moonlight")

set(BASE_SRC
    "app/backend/nvapp.cpp"
    "app/main.cpp"
    "app/backend/computerseeker.cpp"
    "app/backend/identitymanager.cpp"
    "app/backend/nvcomputer.cpp"
    "app/backend/nvhttp.cpp"
    "app/backend/nvpairingmanager.cpp"
    "app/backend/computermanager.cpp"
    "app/backend/boxartmanager.cpp"
    "app/backend/richpresencemanager.cpp"
    "app/cli/commandlineparser.cpp"
    "app/cli/quitstream.cpp"
    "app/cli/startstream.cpp"
    "app/settings/mappingfetcher.cpp"
    "app/settings/streamingpreferences.cpp"
    "app/streaming/input/abstouch.cpp"
    "app/streaming/input/gamepad.cpp"
    "app/streaming/input/input.cpp"
    "app/streaming/input/keyboard.cpp"
    "app/streaming/input/mouse.cpp"
    "app/streaming/input/reltouch.cpp"
    "app/streaming/session.cpp"
    "app/streaming/audio/audio.cpp"
    "app/streaming/audio/renderers/sdlaud.cpp"
    "app/gui/computermodel.cpp"
    "app/gui/appmodel.cpp"
    "app/streaming/streamutils.cpp"
    "app/backend/autoupdatechecker.cpp"
    "app/path.cpp"
    "app/settings/mappingmanager.cpp"
    "app/gui/sdlgamepadkeynavigation.cpp"
    "app/streaming/video/overlaymanager.cpp"
    "app/backend/systemproperties.cpp"
    "app/wm.cpp"
)

set(QRC_SRC
    "app/qml-webos.qrc"
    "app/resources-webos.qrc"
)

set(WEBOS_SRC
    "app/streaming/video/webos.cpp"
)

set(FFMPEG_SRC
    "app/streaming/video/ffmpeg.cpp"
    "app/streaming/video/ffmpeg-renderers/sdlvid.cpp"
    "app/streaming/video/ffmpeg-renderers/cuda.cpp"
    "app/streaming/video/ffmpeg-renderers/pacer/pacer.cpp"
    "app/streaming/video/ffmpeg-renderers/pacer/nullthreadedvsyncsource.cpp"
)

set(EGL_SRC
    "app/streaming/video/ffmpeg-renderers/eglvid.cpp"
    "app/streaming/video/ffmpeg-renderers/egl_extensions.cpp"
)

add_executable(${EXECUTABLE_NAME} ${BASE_SRC} ${QRC_SRC})

set_target_properties(${EXECUTABLE_NAME} PROPERTIES
    CXX_STANDARD          11
    CXX_STANDARD_REQUIRED ON
    AUTOMOC               ON
    AUTORCC               ON
)

target_include_directories(${EXECUTABLE_NAME} PRIVATE "app")
target_include_directories(${EXECUTABLE_NAME} SYSTEM PRIVATE ${SDL2_INCLUDE_DIRS})

target_compile_definitions(${EXECUTABLE_NAME} PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)
target_compile_definitions(${EXECUTABLE_NAME} PRIVATE VERSION_STR="0.0.1")

target_link_libraries(${EXECUTABLE_NAME} PRIVATE
    Qt5::Core Qt5::Multimedia Qt5::Quick Qt5::Network # QT libraries
    ${SDL2_LIBRARIES} ${OPENSSL_LIBRARIES}
    moonlight-common-c qmdnsengine opus pthread
)

if(WAYLAND_FOUND)
    target_compile_definitions(${EXECUTABLE_NAME} PRIVATE HAS_WAYLAND)
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE ${WAYLAND_LIBRARIES})
endif(WAYLAND_FOUND)

if (FFMPEG_FOUND)
    add_subdirectory(h264bitstream)
    target_sources(${EXECUTABLE_NAME} PRIVATE ${FFMPEG_SRC})
    target_compile_definitions(${EXECUTABLE_NAME} PRIVATE HAVE_FFMPEG)
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE h264bitstream libavcodec libavutil)

    if (EGL_FOUND)
        target_sources(${EXECUTABLE_NAME} PRIVATE ${EGL_SRC})
        target_compile_definitions(${EXECUTABLE_NAME} PRIVATE HAVE_EGL)
        target_link_libraries(${EXECUTABLE_NAME} PRIVATE ${EGL_LIBRARIES})
    endif(EGL_FOUND)

endif(FFMPEG_FOUND)

if (GST_FOUND)
    target_sources(${EXECUTABLE_NAME} PRIVATE ${WEBOS_SRC})
    target_compile_definitions(${EXECUTABLE_NAME} PRIVATE HAVE_GST)
    target_include_directories(${EXECUTABLE_NAME} SYSTEM PRIVATE ${GST_INCLUDE_DIRS})
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE ${GST_LIBRARIES})
endif(GST_FOUND)

add_custom_target("webos-package" COMMAND ${CMAKE_SOURCE_DIR}/scripts/build-webos-ipk.sh ${CMAKE_SOURCE_DIR}
    DEPENDS ${EXECUTABLE_NAME}
)

add_custom_target("device-install" COMMAND ${CMAKE_SOURCE_DIR}/scripts/device-install.sh ${CMAKE_SOURCE_DIR}
    DEPENDS "webos-package"
)

add_custom_target("device-launch" COMMAND CMAKE_SOURCE_DIR=${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/scripts/device-launch.sh)