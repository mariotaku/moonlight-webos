cmake_minimum_required(VERSION 3.1)

# This will prevent VSCode plugin from throwing errors
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

# Add folder where are supportive functions
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Include webOS SDK related functions
include(WebOS)

project(moonlight-webos LANGUAGES C CXX)

option(BUILD_SHARED_LIBS "Build static libs instead of shared" OFF)
add_subdirectory(moonlight-common-c)
add_subdirectory(qmdnsengine)

# the `pkg_check_modules` function is created with this call
find_package(PkgConfig REQUIRED) 

# these calls create special `PkgConfig::<MODULE>` variables
find_package(Qt5 COMPONENTS Core Multimedia Quick REQUIRED)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/pkg_$ENV{ARCH}/")
set(EXECUTABLE_NAME "moonlight")

# Update with `find src/ -name '*.cpp' -printf '    "%p"\n'`
add_executable(${EXECUTABLE_NAME} 
    "app/gui/computermodel.cpp"
    "app/main.cpp"
    "app/streaming-controller.cpp"
    "qml/qml.qrc"
)

set_target_properties(${EXECUTABLE_NAME} PROPERTIES
    CXX_STANDARD          11
    CXX_STANDARD_REQUIRED ON
    AUTOMOC               ON
    AUTORCC               ON
)

target_include_directories(${EXECUTABLE_NAME} PRIVATE "app")
target_compile_definitions(${EXECUTABLE_NAME} PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)

target_link_libraries(${EXECUTABLE_NAME} PRIVATE
 Qt5::Core Qt5::Multimedia Qt5::Quick Qt5::Network # QT libraries
 moonlight-common-c qmdnsengine
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