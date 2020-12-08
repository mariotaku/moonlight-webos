cmake_minimum_required(VERSION 3.1)

# This will prevent VSCode plugin from throwing errors
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

# Add folder where are supportive functions
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Include webOS SDK related functions
include(WebOS)

project(moonlight-webos LANGUAGES C CXX)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)

# option(BUILD_SHARED_LIBS "Build static libs instead of shared" OFF)
add_subdirectory(moonlight-common-c)
add_subdirectory(qmdnsengine)
add_subdirectory(opus)

# the `pkg_check_modules` function is created with this call
find_package(PkgConfig REQUIRED) 
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(OPENSSL REQUIRED openssl)

# these calls create special `PkgConfig::<MODULE>` variables
find_package(Qt5 COMPONENTS Core Multimedia Quick REQUIRED)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/pkg_$ENV{ARCH}/")
set(EXECUTABLE_NAME "moonlight")

# Update with `find app/ -name '*.cpp' -printf '    "%p"\n' | grep -vFf app/unsupported-src.list`
add_executable(${EXECUTABLE_NAME} 
    "app/backend/boxartmanager.cpp"
    "app/backend/computermanager.cpp"
    "app/backend/identitymanager.cpp"
    "app/backend/nvapp.cpp"
    "app/backend/nvcomputer.cpp"
    "app/backend/nvhttp.cpp"
    "app/backend/nvpairingmanager.cpp"
    "app/controller/streaming.cpp"
    "app/gui/appmodel.cpp"
    "app/gui/computermodel.cpp"
    "app/main.cpp"
    "app/path.cpp"
    "app/settings/streamingpreferences.cpp"
    "app/streaming/session.cpp"
    "app/wm.cpp"
    "app/qml.qrc"
    "app/resources.qrc"
)

set_target_properties(${EXECUTABLE_NAME} PROPERTIES
    CXX_STANDARD          11
    CXX_STANDARD_REQUIRED ON
    AUTOMOC               ON
    AUTORCC               ON
)

target_include_directories(${EXECUTABLE_NAME} PRIVATE "app")
target_include_directories(${EXECUTABLE_NAME} SYSTEM PRIVATE ${SDL2_INCLUDE_DIRS})
target_compile_definitions(${EXECUTABLE_NAME} PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)

message(INFO ${CMAKE_THREAD_LIBS_INIT})

target_link_libraries(${EXECUTABLE_NAME} PRIVATE
Threads::Threads
 Qt5::Core Qt5::Multimedia Qt5::Quick Qt5::Network # QT libraries
 ${SDL2_LIBRARIES} # SDL libraries
 ${OPENSSL_LIBRARIES}
 ${OPENSSL_LIBRARIES}
 -pthread # Must be some way to fix that
 moonlight-common-c qmdnsengine opus
)

add_custom_target("webos-package" COMMAND ${CMAKE_SOURCE_DIR}/scripts/package.sh ${CMAKE_SOURCE_DIR}
    DEPENDS ${EXECUTABLE_NAME}
)

add_custom_target("device-install" COMMAND ${CMAKE_SOURCE_DIR}/scripts/device-install.sh ${CMAKE_SOURCE_DIR}
    DEPENDS "webos-package"
)

add_custom_target("device-launch" COMMAND ${CMAKE_SOURCE_DIR}/scripts/device-launch.sh ${CMAKE_SOURCE_DIR}
    DEPENDS "device-install"
)