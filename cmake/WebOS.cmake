# QMake is not yet included in SDK itself, that one we needed is located in build-webos/BUILD/sysroots/x86_64-linux
set(OE_QMAKE_PATH_EXTERNAL_HOST_BINS "/opt/webos-sdk-x86_64/1.0.g/sysroots/x86_64-linux/usr/bin/qt5")

# This is required to make build work
list(APPEND CMAKE_PREFIX_PATH "$ENV{OECORE_TARGET_SYSROOT}/usr/lib/cmake/SDL2")
list(APPEND CMAKE_PREFIX_PATH "$ENV{OECORE_TARGET_SYSROOT}/usr/lib/cmake/Qt5")
list(APPEND CMAKE_PREFIX_PATH "$ENV{OECORE_TARGET_SYSROOT}/usr/lib/cmake/Qt5Network")

add_definitions(-DQ_OS_WEBOS)