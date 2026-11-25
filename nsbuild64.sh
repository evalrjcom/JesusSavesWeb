#!/bin/bash
# Build JesusSaves.exe (static, x86_64) from the source tree.  JesusSaves v1.1.2
# Usage: ./nsbuild64.sh [make targets, default: all]   e.g. package-windows libnetsurf
set -o pipefail
R=${JS_ROOT:-$HOME/jesussaves-build}
export P=$R/mw64 H=$R/host
export PATH=$H/bin:$PATH PKG_CONFIG_LIBDIR=$P/lib/pkgconfig PKG_CONFIG_PATH=
export GCCSDK_INSTALL_ENV=$P GCCSDK_INSTALL_CROSSBIN=$R/mwbin
export CFLAGS="-O2 -DXML_STATIC" LDFLAGS="-static -static-libgcc" AR=x86_64-w64-mingw32-ar
cd ${SRC:-$(cd "$(dirname "$0")" && pwd)}/netsurf || exit 1
make -j$(nproc) TARGET=windows "$@"
rc=$?
echo "BUILD-EXIT: $rc"
exit $rc
