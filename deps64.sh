#!/bin/bash
# Third-party static dependencies for the JesusSaves Windows (x86_64) build
set -ex
# JesusSaves v1.1.2: all paths hang off JS_ROOT (default ~/jesussaves-build) so the same
# scripts work locally and in GitHub Actions.  Sources come from fetch_deps.sh.
R=${JS_ROOT:-$HOME/jesussaves-build}
export P=${P:-$R/mw64}
T=x86_64-w64-mingw32
export CC=$T-gcc AR=$T-ar RANLIB=$T-ranlib STRIP=$T-strip RC=$T-windres
export PKG_CONFIG_LIBDIR=$P/lib/pkgconfig PKG_CONFIG_PATH=
export CFLAGS="-O2 -g0" 
mkdir -p $P/include $P/lib
D=$R/deps
B=$R/build/out64; mkdir -p $B
TC="-DCMAKE_SYSTEM_NAME=Windows -DCMAKE_SYSTEM_PROCESSOR=x86_64 -DCMAKE_C_COMPILER=$T-gcc -DCMAKE_RC_COMPILER=$T-windres -DCMAKE_FIND_ROOT_PATH=$P -DCMAKE_INSTALL_PREFIX=$P -DCMAKE_BUILD_TYPE=Release"

step() { [ -f $B/.done-$1 ] && { echo "skip $1"; return 1; }; return 0; }
done_() { touch $B/.done-$1; }

if step zlib; then
  cd $D/zlib && make distclean >/dev/null 2>&1 || true
  CHOST=$T ./configure --prefix=$P --static && make -j2 && make install && done_ zlib
fi
if step openssl; then
  cd $D/openssl && env -u CC -u AR -u RANLIB -u STRIP -u RC ./Configure mingw64 no-shared no-tests --cross-compile-prefix=$T- --prefix=$P --openssldir=$P/ssl && make -j2 build_libs && make install_dev && { [ -d $P/lib64 ] && { cp -f $P/lib64/*.a $P/lib/; mkdir -p $P/lib/pkgconfig; cp -f $P/lib64/pkgconfig/*.pc $P/lib/pkgconfig/; }; true; } && done_ openssl
fi
if step cares; then
  cd $D/c-ares && rm -rf bld && cmake -S . -B bld $TC -DCARES_STATIC=ON -DCARES_SHARED=OFF -DCARES_BUILD_TOOLS=OFF -DCARES_BUILD_TESTS=OFF && cmake --build bld -j2 && cmake --install bld && done_ cares
fi
if step expat; then
  cd $D/expat/expat && rm -rf bld && cmake -S . -B bld $TC -DEXPAT_SHARED_LIBS=OFF -DEXPAT_BUILD_TOOLS=OFF -DEXPAT_BUILD_EXAMPLES=OFF -DEXPAT_BUILD_TESTS=OFF -DEXPAT_BUILD_DOCS=OFF && cmake --build bld -j2 && cmake --install bld && done_ expat
fi
if step libpng; then
  cd $D/libpng && rm -rf bld && cmake -S . -B bld $TC -DPNG_SHARED=OFF -DPNG_STATIC=ON -DPNG_TESTS=OFF -DPNG_TOOLS=OFF -DZLIB_ROOT=$P -DZLIB_LIBRARY=$P/lib/libz.a -DZLIB_INCLUDE_DIR=$P/include && cmake --build bld -j2 && cmake --install bld && done_ libpng
fi
if step jpeg; then
  cd $D/libjpeg-turbo && rm -rf bld && cmake -S . -B bld $TC -DENABLE_SHARED=OFF -DENABLE_STATIC=ON -DWITH_TURBOJPEG=OFF -DWITH_SIMD=0 -DWITH_JAVA=0 && cmake --build bld -j2 && cmake --install bld && done_ jpeg
fi
if step curl; then
  cd $D/curl && { [ -f configure ] || autoreconf -fi; }
  rm -rf bld && mkdir bld && cd bld
  LIBS="-lws2_32 -lgdi32 -lcrypt32 -ladvapi32 -luser32 -lbcrypt -liphlpapi" CPPFLAGS="-DCARES_STATICLIB" \
  ../configure --host=$T --prefix=$P --disable-shared --enable-static \
    --with-openssl=$P --enable-ares=$P --with-zlib=$P \
    --without-libpsl --without-nghttp2 --without-nghttp3 --without-ngtcp2 --without-brotli --without-zstd \
    --without-libidn2 --without-libssh2 --without-librtmp \
    --disable-ldap --disable-ldaps --disable-rtsp --disable-dict --disable-telnet \
    --disable-tftp --disable-pop3 --disable-imap --disable-smtp --disable-gopher --disable-mqtt \
    --disable-manual --disable-ftp --disable-file --disable-smb --disable-docs --disable-threaded-resolver \
    && make -j2 -C lib && make -C lib install && make -C include install && make install-pkgconfigDATA && done_ curl
fi
if step tre; then
  cd $D/tre && { [ -f configure ] || ./utils/autogen.sh; }
  rm -rf bld && mkdir bld && cd bld
  ../configure --host=$T --prefix=$P --disable-shared --enable-static --disable-approx --disable-agrep \
    && make -j2 -C lib && make -C lib install && make -C include install 2>/dev/null; true
  done_ tre
fi
# JesusSaves v1.1.2: fail loudly if anything is missing (set -e does not see failures inside the
# `if step ...; then a && b && c; fi` chains above)
for f in lib/libz.a lib/libssl.a lib/libcrypto.a lib/libcares.a lib/libexpat.a lib/libpng.a \
         lib/libjpeg.a lib/libcurl.a lib/libtre.a lib/pkgconfig/libcurl.pc lib/pkgconfig/libssl.pc \
         lib/pkgconfig/zlib.pc include/curl/curl.h include/openssl/ssl.h include/tre/regex.h; do
  [ -f "$P/$f" ] || { echo "DEPS64 FAILED: missing $P/$f"; exit 1; }
done
echo DEPS64-PART1-DONE
