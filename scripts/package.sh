#!/bin/sh

set APP_META_DIR=$1

echo $1

if [ -f $APP_META_DIR/appinfo.json ]; then
  echo 'Application metadata is not present'
  exit 1
fi

if [ -z $ARCH ]; then
  echo 'ARCH is not set'
  exit 1
fi

if [ -z $WEBOS_CLI_TV ]; then
  echo 'webOS SDK not found, please check your WEBOS_CLI_TV variable'
  exit 1
fi

cp -f $APP_META_DIR/* pkg_$ARCH
$WEBOS_CLI_TV/ares-package pkg_$ARCH