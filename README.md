# JesusSaves

JesusSaves v1.1.1 is a small, fast web browser based on NetSurf 3.11 (GPLv2), with a Windows static build.

- DuckDuckGo search only (address bar and home page)
- Home page and default bookmarks point to Bible reference sites, evangelism and Baptist sites, and sound teaching, including christpath.org
- DNS locked to Cloudflare 1.1.1.3 / 1.0.0.3 (cannot be changed in the app)
- Scripture-aware address bar: type `John 3:16` to open the passage
- Verse preview: hover a Bible reference link to see the KJV text (full KJV built in, works offline)
- Static, hardened Windows x86_64 build: `JesusSaves.exe`, `jesussaves-installer.exe`

## Build (Windows, cross-compiled with mingw-w64)

See `build_windows_static.sh` (runs `deps64.sh`, `nslibs64.sh`, `nsbuild64.sh`). Set `SRC` to the checkout directory.
Changes are listed in `JESUSSAVES_CHANGELOG_v1.1.1.md`.

## Licence

GNU General Public License v2. NetSurf is the work of the NetSurf Developers; modifications by the JesusSaves project. The embedded KJV text (1769) is in the public domain.
