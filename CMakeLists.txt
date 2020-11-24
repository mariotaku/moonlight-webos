cmake_minimum_required(VERSION 3.1)

set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")
project(moonlight-webos LANGUAGES CXX)

if ( ${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR} )
    MESSAGE(FATAL_ERROR "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there. You may need to remove CMakeCache.txt." )
endif()

# QMake is not yet included in SDK itself, that one we needed is located in build-webos/BUILD/sysroots/x86_64-linux
set(OE_QMAKE_PATH_EXTERNAL_HOST_BINS "/opt/webos-sdk-x86_64/1.0.g/sysroots/x86_64-linux/usr/bin/qt5")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# This is required to make build work
set(Qt5_DIR "$ENV{OECORE_TARGET_SYSROOT}/usr/lib/cmake/Qt5")
find_package(Qt5 COMPONENTS Core Quick REQUIRED)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/pkg_$ENV{ARCH}/")
set(EXECUTABLE_NAME "moonlight")

add_executable(${EXECUTABLE_NAME} "src/main.cpp" "src/qml.qrc")
target_compile_definitions(${EXECUTABLE_NAME} PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)
target_link_libraries(${EXECUTABLE_NAME} PRIVATE Qt5::Core Qt5::Quick)

set(APP_META_DIR "${CMAKE_SOURCE_DIR}/webos-metadata")

add_custom_command(TARGET "${EXECUTABLE_NAME}" POST_BUILD
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/package.sh ${APP_META_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Packaging into IPK file..."
)