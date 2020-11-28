#!/bin/sh

CMAKE_SOURCE_DIR=$1

if [ -z $CMAKE_SOURCE_DIR ]; then
  echo 'Source dir not specified!'
  exit 1
fi

APP_META_DIR=$CMAKE_SOURCE_DIR/webos-metadata
ASSETS_DIR=$CMAKE_SOURCE_DIR/assets
SRC_DIR=$CMAKE_SOURCE_DIR/src

if [ ! -f $APP_META_DIR/appinfo.json ]; then
  echo 'Application metadata is not present'
  exit 1
fi

if [ -z $ARCH ]; then
  echo 'ARCH is not set'
  exit 1
fi

PKG_DEST=pkg_$ARCH

if [ -z $WEBOS_CLI_TV ]; then
  echo 'webOS SDK not found, please check your WEBOS_CLI_TV variable'
  exit 1
fi

rm -rf $PKG_DEST/$APP_META_DIR
rm -rf $PKG_DEST/$ASSETS_DIR

cp -r $APP_META_DIR $PKG_DEST/
cp -r $ASSETS_DIR $PKG_DEST/
$WEBOS_CLI_TV/ares-package pkg_$ARCH