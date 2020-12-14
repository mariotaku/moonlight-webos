#!/bin/sh

if [ -d ./app/deploy/webos ]; then
CMAKE_SOURCE_DIR=./
fi

if [ -z $CMAKE_SOURCE_DIR ]; then
  echo 'Source dir not specified!'
  exit 1
fi

APP_META_DIR=$CMAKE_SOURCE_DIR/app/deploy/webos

PKG_NAME=$(jq -r .id ${APP_META_DIR}/appinfo.json)
EXE_NAME=$(jq -r .main ${APP_META_DIR}/appinfo.json)

DEVICE=hometv-nopass

ssh $DEVICE << EOF
killall ${EXE_NAME}
export APPID=${PKG_NAME}
cd apps/usr/palm/applications/${PKG_NAME}/
if [ -d assets/debug.env ]; then
  . assets/debug.env
fi
./${EXE_NAME} $@
EOF