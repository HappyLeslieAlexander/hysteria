#!/bin/sh
set -eu

: "${FREEBSD_BRANCH:=13.4 14.3 15.0}"

for v in ${FREEBSD_BRANCH}; do
  echo "[ko] target FreeBSD ${v}"
  make -C ko clean SYSDIR=/usr/src/sys || true
  make -C ko PROFILE=release SYSDIR=/usr/src/sys
  make -C ko clean SYSDIR=/usr/src/sys || true
done
