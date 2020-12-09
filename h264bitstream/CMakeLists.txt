cmake_minimum_required(VERSION 3.1)
project(h264bitstream)

set(PROJECT_NAME "h264bitstream")
set(PROJECT_DESCRIPTION "A library to read and write H.264 video bitstreams, in particular to examine or modify headers.")
set(PROJECT_AUTHOR "aizvorski")
set(PROJECT_URL "https://github.com/aizvorski/h264bitstream")

set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 1)
set(PROJECT_VERSION_PATCH 0)
set(PROJECT_VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH})

set(BIN_INSTALL_DIR bin CACHE STRING "Binary installation directory relative to the install prefix")
set(LIB_INSTALL_DIR lib CACHE STRING "Library installation directory relative to the install prefix")
set(INCLUDE_INSTALL_DIR include CACHE STRING "Header installation directory relative to the install prefix")

set(HEADERS
    "h264bitstream/bs.h"
    "h264bitstream/h264_avcc.h"
    "h264bitstream/h264_sei.h"
    "h264bitstream/h264_slice_data.h"
    "h264bitstream/h264_stream.h"
)

set(SRC
    "h264bitstream/h264_avcc.c"
    "h264bitstream/h264_nal.c"
    "h264bitstream/h264_sei.c"
    "h264bitstream/h264_slice_data.c"
    "h264bitstream/h264_stream.c"
)

add_library(h264bitstream ${INCLUDES} ${SRC})

set_target_properties(h264bitstream PROPERTIES
    PUBLIC_HEADER         ${SRC}
    VERSION               ${PROJECT_VERSION}
    SOVERSION             ${PROJECT_VERSION_MAJOR}
)

target_include_directories(h264bitstream PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/h264bitstream>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
    "$<INSTALL_INTERFACE:${INCLUDE_INSTALL_DIR}>"
)

install(TARGETS h264bitstream
    EXPORT        h264bitstream-export
    RUNTIME       DESTINATION "${BIN_INSTALL_DIR}"
    LIBRARY       DESTINATION "${LIB_INSTALL_DIR}"
    ARCHIVE       DESTINATION "${LIB_INSTALL_DIR}"
    PUBLIC_HEADER DESTINATION "${INCLUDE_INSTALL_DIR}"
)

install(EXPORT h264bitstream-export
    FILE        h264bitstream-config.cmake
    DESTINATION "${LIB_INSTALL_DIR}/cmake/h264bitstream"
)

include(CMakePackageConfigHelpers)

write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/h264bitstream-config-version.cmake"
    VERSION       ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/h264bitstream-config-version.cmake"
    DESTINATION "${LIB_INSTALL_DIR}/cmake/h264bitstream"
)

set(CPACK_PACKAGE_INSTALL_DIRECTORY "${PROJECT_NAME}")
set(CPACK_PACKAGE_VENDOR "${PROJECT_AUTHOR}")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})

set(CPACK_COMPONENT_DOCUMENTATION_DISPLAY_NAME "Documentation")
set(CPACK_COMPONENT_DOCUMENTATION_DESCRIPTION "Documentation generated for the library")
set(CPACK_COMPONENT_EXAMPLES_DISPLAY_NAME "Examples")
set(CPACK_COMPONENT_EXAMPLES_DESCRIPTION "Sample applications using the library")

include(CPack)
