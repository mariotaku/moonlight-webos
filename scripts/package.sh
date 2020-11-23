#!/bin/sh

if [ -z $ARCH ]; then
  echo 'ARCH is not set'
  exit 1
fi

if [ -z $WEBOS_CLI_TV ]; then
  echo 'webOS SDK not found, please check your WEBOS_CLI_TV variable'
  exit 1
fi

$WEBOS_CLI_TV/ares-package pkg_$ARCH