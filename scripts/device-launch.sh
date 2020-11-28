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

ssh $DEVICE "apps/usr/palm/applications/${PKG_NAME}/${EXE_NAME}"