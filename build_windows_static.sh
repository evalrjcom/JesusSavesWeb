#!/bin/bash
# JesusSaves v1.1.2 - reproducible static Windows x86_64 build (Debian/Ubuntu + mingw-w64).
# Needs: g++-mingw-w64-x86-64-win32 binutils-mingw-w64-x86-64 win-iconv-mingw-w64-dev nsis cmake
#        autoconf automake libtool gettext autopoint flex bison pkg-config python3 git make gcc zlib1g-dev gperf perl xsltproc
# Everything is built under $JS_ROOT (default ~/jesussaves-build).  Result: netsurf/JesusSaves.exe,
# netsurf/jesussaves-installer.exe, netsurf/build/Linux-windows/libnetsurf.a
# Embedded KJV (only to regenerate): python3 tools/gen_kjv.py verses-1769.json netsurf/desktop/scripture_kjv.c
set -e
D=$(cd "$(dirname "$0")" && pwd)
bash "$D/fetch_deps.sh"
bash "$D/setup_toolchain.sh"
bash "$D/deps64.sh"
bash "$D/setup_toolchain.sh"     # again: the regex shim needs TRE from deps64.sh
bash "$D/nslibs64.sh"
bash "$D/nsbuild64.sh" package-windows libnetsurf
