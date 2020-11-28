#!/bin/sh

CMAKE_SOURCE_DIR=$1

if [ -z $CMAKE_SOURCE_DIR ]; then
  echo 'Source dir not specified!'
  exit 1
fi

APP_META_DIR=$CMAKE_SOURCE_DIR/webos-metadata

PKG_NAME=$(jq -r .id ${APP_META_DIR}/appinfo.json)
EXE_NAME=$(jq -r .main ${APP_META_DIR}/appinfo.json)

DEVICE=hometv-nopass

ssh $DEVICE "while true; do ps -o rss,command -A | grep ${PKG_NAME} | grep -v grep; sleep 1; done" \
  | awk '{print $1; fflush();}' \
  | $CMAKE_SOURCE_DIR/tools/ttyplot-amd64-linux -t "${PKG_NAME} Memory Usage" -u KB