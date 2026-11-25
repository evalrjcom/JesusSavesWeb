#!/bin/bash
# NetSurf libraries (static) for the JesusSaves Windows x86_64 build, plus the host-side nsgenbind
set -u
R=${JS_ROOT:-$HOME/jesussaves-build}   # JesusSaves v1.1.1: portable root
P=$R/mw64          # target prefix
H=$R/host          # native host tools prefix
S=${SRC:-$(cd "$(dirname "$0")" && pwd)}   # source checkout (this directory)
mkdir -p $R/build
T=x86_64-w64-mingw32
mkdir -p $H $P
export PKG_CONFIG_LIBDIR=$P/lib/pkgconfig PKG_CONFIG_PATH=
log() { echo "=== $*"; }
run() { log "$1"; shift; "$@" || { echo "FAILED: $1"; exit 1; }; }

# 1. native build system + nsgenbind (runs on the build machine)
(cd $S/buildsystem && env -u PKG_CONFIG_LIBDIR make PREFIX=$H install) || { echo FAILED buildsystem-host; exit 1; }
(cd $S/buildsystem && make PREFIX=$P install) || { echo FAILED buildsystem-target; exit 1; }
(cd $S/nsgenbind && env -u PKG_CONFIG_LIBDIR -u CC -u AR make PREFIX=$H NSSHARED=$H/share/netsurf-buildsystem install) || { echo FAILED nsgenbind; exit 1; }

export CC=$T-gcc AR=$T-ar RANLIB=$T-ranlib
export CFLAGS="-O2 -DXML_STATIC -DCURL_STATICLIB -DCARES_STATICLIB -I$P/include"
export LDFLAGS="-L$P/lib"
for lib in libwapcaplet libparserutils libhubbub libcss libdom libnsbmp libnsgif librosprite libsvgtiny libnsutils libnspsl libnslog libutf8proc; do
  log "$lib"
  (cd $S/$lib && make clean >/dev/null 2>&1; make -j2 PREFIX=$P NSSHARED=$P/share/netsurf-buildsystem install) > $R/build/lib-$lib.log 2>&1 || { echo "FAILED: $lib (see $R/build/lib-$lib.log)"; tail -15 $R/build/lib-$lib.log; exit 1; }
done
echo NSLIBS64-DONE
