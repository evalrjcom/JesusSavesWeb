#!/bin/bash
# Fetch the pinned third-party sources for the static Windows build.  JesusSaves v1.1.2
# Sources go to $JS_ROOT/deps (default ~/jesussaves-build/deps); existing checkouts are kept.
set -e
R=${JS_ROOT:-$HOME/jesussaves-build}
mkdir -p "$R/deps"; cd "$R/deps"
clone() { [ -d "$2" ] || git clone --depth 1 --branch "$1" "$3" "$2"; }
clone v1.3.1          zlib          https://github.com/madler/zlib.git
clone openssl-3.0.13  openssl       https://github.com/openssl/openssl.git
clone cares-1_27_0    c-ares        https://github.com/c-ares/c-ares.git
clone curl-8_5_0      curl          https://github.com/curl/curl.git
clone R_2_6_1         expat         https://github.com/libexpat/libexpat.git
clone v1.6.43         libpng        https://github.com/pnggroup/libpng.git
clone 2.1.5.1         libjpeg-turbo https://github.com/libjpeg-turbo/libjpeg-turbo.git
# TRE (regex library standing in for libgnurx) has no release tag we rely on: pin the commit
if [ ! -d tre ]; then
  git clone https://github.com/laurikari/tre.git tre
  git -C tre checkout 71bfcaf
fi
echo "FETCH-DONE"
