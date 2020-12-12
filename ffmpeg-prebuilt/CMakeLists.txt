cmake_minimum_required (VERSION 3.10)

project(ffmpeg-prebuilt CXX)

add_library(libavcodec SHARED IMPORTED GLOBAL)

set_target_properties(libavcodec PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES      ${CMAKE_CURRENT_SOURCE_DIR}/include
    IMPORTED_LOCATION                  ${CMAKE_CURRENT_SOURCE_DIR}/lib/libavcodec.a
)

add_library(libavutil SHARED IMPORTED GLOBAL)

set_target_properties(libavutil PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES      ${CMAKE_CURRENT_SOURCE_DIR}/include
    IMPORTED_LOCATION                  ${CMAKE_CURRENT_SOURCE_DIR}/lib/libavutil.a
)