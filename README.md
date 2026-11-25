# JesusSaves

[![Build and test](https://github.com/evalrjcom/JesusSavesWeb/actions/workflows/build.yml/badge.svg)](https://github.com/evalrjcom/JesusSavesWeb/actions/workflows/build.yml)

JesusSaves v1.1.2 is a small, fast web browser based on NetSurf 3.11 (GPLv2), with a Windows static build.

- DuckDuckGo search only (address bar and home page)
- Home page and default bookmarks point to Bible reference sites, evangelism and Baptist sites, and sound teaching, including christpath.org
- DNS locked to Cloudflare 1.1.1.3 / 1.0.0.3 (cannot be changed in the app)
- Scripture-aware address bar: type `John 3:16` to open the passage
- Typing a verse (e.g. John 3:16) opens it in the NKJV on Bible Gateway. Verse preview: hover a Bible reference link to see the KJV text (full KJV built in, works offline)
- Static, hardened Windows x86_64 build: `JesusSaves.exe`, `jesussaves-installer.exe`

## Build (Windows, cross-compiled with mingw-w64)

`build_windows_static.sh` runs the whole chain: `fetch_deps.sh` (pinned third-party sources), `setup_toolchain.sh`, `deps64.sh` (OpenSSL, curl, c-ares, ...), `nslibs64.sh` (NetSurf libraries) and `nsbuild64.sh` (the browser, installer and `libnetsurf.a`). Everything is built under `JS_ROOT` (default `~/jesussaves-build`).

## Continuous integration

`.github/workflows/build.yml` runs on every push and pull request:

- **Project invariants:** no leftover references to the old project name, DNS pin still 1.1.1.3 / 1.0.0.3, DuckDuckGo the only search provider, one version everywhere.
- **Scripture tests:** the Bible-reference parser and KJV lookup under ASan/UBSan.
- **Static Windows build:** builds the exe, installer and static libraries, then checks that only Windows system DLLs are imported, ASLR/DEP are on and the exe is stripped. Build outputs are uploaded as an artifact with SHA-256 sums.
- **Release:** pushing a tag such as `v1.1.2` (must match the source version) publishes a GitHub release with those files.
Changes are listed in `JESUSSAVES_CHANGELOG_v1.1.2.md`.

## Licence

GNU General Public License v2. NetSurf is the work of the NetSurf Developers; modifications by the JesusSaves project. The embedded KJV text (1769) is in the public domain.
