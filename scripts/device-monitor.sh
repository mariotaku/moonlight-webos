#!/bin/sh

CMAKE_SOURCE_DIR=$1
MON_TYPE=$2

if [ -z $CMAKE_SOURCE_DIR ]; then
  echo 'Source dir not specified!'
  exit 1
fi

case ${MON_TYPE} in
  "mem")
    MON_COL=rss
    MON_UNIT='KB'
    ;;
  "cpu")
    MON_COL=pcpu
    MON_UNIT='%'
    ;;
  *)
    echo "Unrecognized monitor type ${MON_TYPE}"
    exit 1
    ;;
esac


APP_META_DIR=$CMAKE_SOURCE_DIR/webos-metadata

PKG_NAME=$(jq -r .id ${APP_META_DIR}/appinfo.json)
EXE_NAME=$(jq -r .main ${APP_META_DIR}/appinfo.json)

DEVICE=hometv-nopass

ssh $DEVICE "while true; do ps -o $MON_COL,command -A | grep ${PKG_NAME} | grep -v gdb | grep -v grep; sleep 1; done" \
  | awk '{print $1; fflush();}' \
  | $CMAKE_SOURCE_DIR/tools/ttyplot-amd64-linux -t "${PKG_NAME} Memory Usage" -u $MON_UNIT