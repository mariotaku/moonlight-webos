cmake_minimum_required(VERSION 3.1)

# This will prevent VSCode plugin from throwing errors
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")
project(moonlight-webos LANGUAGES C CXX)

if ( ${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR} )
    MESSAGE(FATAL_ERROR "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there. You may need to remove CMakeCache.txt." )
endif()

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# This is required to make build work
list(APPEND CMAKE_PREFIX_PATH "$ENV{OECORE_TARGET_SYSROOT}/usr/lib/cmake/Qt5")

set(MOONLIGHT_CORE_DIR "${CMAKE_SOURCE_DIR}/moonlight-common-c/")
set(RS_DIR "${MOONLIGHT_CORE_DIR}/reedsolomon/")
set(ENET_DIR "${MOONLIGHT_CORE_DIR}/enet/")
file(GLOB RS_SRC "${RS_DIR}/*.c")
file(GLOB ENET_SRC "${ENET_DIR}/*.c")
file(GLOB MOONLIGHT_CORE_SRC "${MOONLIGHT_CORE_DIR}/src/*.c")
add_library(moonlight_core STATIC ${RS_SRC} ${ENET_SRC} ${MOONLIGHT_CORE_SRC})
target_include_directories(moonlight_core PRIVATE ${RS_DIR} "${ENET_DIR}/include" "${MOONLIGHT_CORE_DIR}/src")
set_target_properties(moonlight_core PROPERTIES COMPILE_FLAGS "-Wno-missing-braces -DHAS_SOCKLEN_T=1 -DHAS_FCNTL=1 -DNO_MSGAPI=1")

# the `pkg_check_modules` function is created with this call
find_package(PkgConfig REQUIRED) 

# these calls create special `PkgConfig::<MODULE>` variables
pkg_check_modules(GST REQUIRED IMPORTED_TARGET gstreamer-1.0)

# QMake is not yet included in SDK itself, that one we needed is located in build-webos/BUILD/sysroots/x86_64-linux
set(OE_QMAKE_PATH_EXTERNAL_HOST_BINS "/opt/webos-sdk-x86_64/1.0.g/sysroots/x86_64-linux/usr/bin/qt5")

find_package(Qt5 COMPONENTS Core Multimedia Quick REQUIRED)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/pkg_$ENV{ARCH}/")
set(EXECUTABLE_NAME "moonlight")

add_executable(${EXECUTABLE_NAME} "src/main.cpp" "src/streaming-controller.cpp" "qml/qml.qrc")
set_target_properties(${EXECUTABLE_NAME} PROPERTIES AUTOMOC TRUE)
target_include_directories(${EXECUTABLE_NAME} SYSTEM PRIVATE ${GST_INCLUDE_DIRS})
target_compile_definitions(${EXECUTABLE_NAME} PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)
target_link_libraries(${EXECUTABLE_NAME} PRIVATE moonlight_core Qt5::Core Qt5::Multimedia Qt5::Quick ${GST_LIBRARIES})

set(APP_META_DIR "${CMAKE_SOURCE_DIR}/webos-metadata")

add_custom_target("package" COMMAND ${CMAKE_SOURCE_DIR}/scripts/package.sh ${CMAKE_SOURCE_DIR}
    DEPENDS ${EXECUTABLE_NAME}
)

add_custom_target("device-install" COMMAND ${CMAKE_SOURCE_DIR}/scripts/device-install.sh ${CMAKE_SOURCE_DIR}
    DEPENDS "package"
)

add_custom_target("device-launch" COMMAND ${CMAKE_SOURCE_DIR}/scripts/device-launch.sh ${CMAKE_SOURCE_DIR}
    DEPENDS "device-install"
)