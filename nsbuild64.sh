#!/bin/bash
# Build JesusSaves.exe (static, x86_64) from the source tree
export P=/home/claude/mw64 H=/home/claude/host
export PATH=$H/bin:$PATH PKG_CONFIG_LIBDIR=$P/lib/pkgconfig PKG_CONFIG_PATH=
export GCCSDK_INSTALL_ENV=$P GCCSDK_INSTALL_CROSSBIN=/home/claude/mwbin
export CFLAGS="-O2 -DXML_STATIC" LDFLAGS="-static -static-libgcc" AR=x86_64-w64-mingw32-ar
cd ${SRC:-$HOME/jesussaves-3.11_v1.1.1}/netsurf
make -j$(nproc) TARGET=windows "$@"
echo "BUILD-EXIT: $?"
