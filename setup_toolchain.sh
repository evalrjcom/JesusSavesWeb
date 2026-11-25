#!/bin/bash
# Toolchain glue for the static Windows build.  JesusSaves v1.1.2
#  - $JS_ROOT/mwbin: unversioned names for the win32-thread-model mingw-w64 tools (NetSurf's
#    windows Makefile.tools looks for them via GCCSDK_INSTALL_CROSSBIN)
#  - regex.h / libgnurx.a shim on top of TRE (run after deps64.sh)
set -e
R=${JS_ROOT:-$HOME/jesussaves-build}
T=x86_64-w64-mingw32
mkdir -p "$R/mwbin" "$R/mw64/include" "$R/mw64/lib"
for pair in "$T-gcc:$T-gcc-win32" "$T-gcc-ar:$T-gcc-ar-win32" "$T-strip:$T-strip" "$T-windres:$T-windres"; do
  link=${pair%%:*}; tool=$(command -v "${pair##*:}") || { echo "missing ${pair##*:} (apt install g++-mingw-w64-x86-64-win32 binutils-mingw-w64-x86-64)"; exit 1; }
  ln -sf "$tool" "$R/mwbin/$link"
done
if [ -f "$R/mw64/lib/libtre.a" ]; then
  printf '#include <tre/regex.h>\n' > "$R/mw64/include/regex.h"
  cp -f "$R/mw64/lib/libtre.a" "$R/mw64/lib/libgnurx.a"
fi
echo "TOOLCHAIN-OK"
