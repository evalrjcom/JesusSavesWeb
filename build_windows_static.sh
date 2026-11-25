#!/bin/bash
# JesusSaves v1.1.1 - reproducible static Windows x86_64 build (Debian/Ubuntu + mingw-w64).
# Set SRC to the source dir; edit the other /home/claude paths at the top of each script.
# Run deps64.sh (third-party libs), nslibs64.sh (NetSurf libs), then nsbuild64.sh.
# Regenerate the embedded KJV (only if needed): python3 tools/gen_kjv.py verses-1769.json netsurf/desktop/scripture_kjv.c
set -e
D=$(dirname "$0"); bash "$D/deps64.sh"; bash "$D/nslibs64.sh"; bash "$D/nsbuild64.sh" package-windows libnetsurf
