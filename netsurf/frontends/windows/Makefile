#
# Makefile for NetSurf Windows target
#
# This file is part of NetSurf 


VERSION_MAJ := $(shell sed -n '/_major/{s/.* = \([0-9]*\).*/\1/;p;}' desktop/version.c)
VERSION_MIN := $(shell sed -n '/_minor/{s/.* = \([0-9]*\).*/\1/;p;}' desktop/version.c)
# JESUSSAVES-v1.1.1: the installer shows the JesusSaves version, not the upstream NetSurf one
JESUSSAVES_VER_MAJ := 1
JESUSSAVES_VER_MIN := 1
JESUSSAVES_VER_BLD := 1
# a 64-bit compiler selects the 64-bit install directory / registry view in the installer
JESUSSAVES_NSIS_ARCH := $(if $(findstring x86_64,$(shell $(CC) -dumpmachine 2>/dev/null)),-DWIN64=1,)


LDFLAGS += -L${GCCSDK_INSTALL_ENV}/lib 
CFLAGS += -I${GCCSDK_INSTALL_ENV}/include/

$(eval $(call pkg_config_find_and_add,libcares,Cares))
$(eval $(call pkg_config_find_and_add,zlib,ZLib))

# libraries for windows API
# JESUSSAVES-v1.1.1: -lwinpthread provides clock_gettime() for libnsutils (MinGW-w64 has
# no other source for it); -static -static-libgcc make JesusSaves.exe a fully static
# executable that needs no DLL besides the ones Windows itself ships.
LDFLAGS += -lgnurx -lwinpthread -lgdi32 -lcomctl32 -lws2_32 -lmsimg32 -lshlwapi -lcrypt32 -mwindows -static -static-libgcc

# JESUSSAVES-v1.1.1: size and hardening flags.
#  -Os + function/data sections + --gc-sections drop unused code from the static libraries.
#  --dynamicbase (ASLR), --high-entropy-va (64-bit ASLR), --nxcompat (DEP) and the stack
#  protector / FORTIFY checks make memory-corruption bugs much harder to exploit.
#  --no-insert-timestamp makes the exe byte-for-byte reproducible.
#  libssp must come last on the link line (it provides __stack_chk_fail).
CFLAGS += -Os -ffunction-sections -fdata-sections -fstack-protector-strong -D_FORTIFY_SOURCE=2
LDFLAGS += -Wl,--gc-sections -Wl,--dynamicbase -Wl,--nxcompat -Wl,--high-entropy-va -Wl,--no-insert-timestamp -lssp

# JESUSSAVES-v1.1.1: strip by default; use the strip that matches the compiler
# (x86_64-w64-mingw32-gcc -> x86_64-w64-mingw32-strip).  Override with STRIP=...
STRIP := $(patsubst %gcc,%strip,$(lastword $(CC)))

CFLAGS += -U__STRICT_ANSI__ -mwin32
# only windows versions after XP are supported
# https://msdn.microsoft.com/en-gb/library/windows/desktop/aa383745
CFLAGS += '-DWINVER=0x0501'
CFLAGS += '-D_WIN32_WINNT=0x0501'
CFLAGS += '-D_WIN32_WINDOWS=0x0501'
CFLAGS += '-D_WIN32_IE=0x0501'

#installed resource path
CFLAGS += '-DNETSURF_WINDOWS_RESPATH="$(NETSURF_WINDOWS_RESPATH)"'

# JESUSSAVES-v1.1.1: no -g in release builds (debug info was ~10 MB of the exe)
WSCFLAGS := -std=c99 -Dnswin32 -DCURL_STATICLIB -DCARES_STATICLIB

CFLAGS += $(WSCFLAGS)
LDFLAGS += $(WSCFLAGS)

# The filter and target for split messages
MESSAGES_FILTER=win
MESSAGES_TARGET=$(FRONTEND_RESOURCES_DIR)

# ----------------------------------------------------------------------------
# built-in resource setup
# ----------------------------------------------------------------------------

$(OBJROOT)/windows_resource.o: $(FRONTEND_RESOURCES_DIR)/resource.rc $(addsuffix /Messages,$(addprefix $(MESSAGES_TARGET)/,$(MESSAGES_LANGUAGES)))
	$(VQ)echo " WINDRES: compiling windows resources"
	${Q}$(WINDRES) $< -O coff -o $@

S_RESOURCES := windows_resource.o

# ----------------------------------------------------------------------------
# Source file setup
# ----------------------------------------------------------------------------

# sources purely for the windows build
S_FRONTEND := main.c window.c gui.c clipboard.c drawable.c plot.c findfile.c \
	  font.c bitmap.c about.c prefs.c download.c fetch.c file.c	\
	  local_history.c schedule.c windbg.c pointers.c \
	  corewindow.c hotlist.c cookies.c global_history.c

# This is the final source build list
# Note this is deliberately *not* expanded here as common and image
#   are not yet available
SOURCES = $(S_COMMON) $(S_IMAGE) $(S_BROWSER) $(S_FRONTEND) $(S_RESOURCES)
EXETARGET := JesusSaves.exe  # JESUSSAVES-v1.1.1

# ----------------------------------------------------------------------------
# Install target
# ----------------------------------------------------------------------------

install-windows:

# ----------------------------------------------------------------------------
# Package target
# ----------------------------------------------------------------------------

package-windows: jesussaves-installer.exe

WIN_RES_OBJ := installer.nsi NetSurf.ico netsurf.png welcome.html default.css
WIN_RES_INS_OBJ := $(addprefix $(FRONTEND_RESOURCES_DIR)/,$(WIN_RES_OBJ)) $(OBJROOT)/messages-en

# deal with making installer generation verbose
ifeq ($(Q),)
NSIS_VERBOSE := 4
else
NSIS_VERBOSE := 0
endif

# installer messages generation
$(OBJROOT)/messages-en: $(MESSAGES_TARGET)/en/Messages
	$(VQ)echo "   MSGCP: $< $@"
	$(Q)$(INSTALL) -m 644 -T $< $@

jesussaves-installer.exe: $(EXETARGET) $(POSTEXES) $(WIN_RES_INS_OBJ)
	$(VQ)echo "MAKENSIS: $@"
	$(Q)makensis -V$(NSIS_VERBOSE) -NOCD -DOBJROOT=$(OBJROOT) -DRESDIR=$(FRONTEND_RESOURCES_DIR) -DVERSIONMAJOR=$(JESUSSAVES_VER_MAJ) -DVERSIONMINOR=$(JESUSSAVES_VER_MIN) -DVERSIONBUILD=$(JESUSSAVES_VER_BLD) $(JESUSSAVES_NSIS_ARCH) -DOUTFNAME=$@ $(FRONTEND_RESOURCES_DIR)/installer.nsi

clean-installer:
	$(VQ)echo "   CLEAN: jesussaves-installer.exe"
	$(Q)$(RM) jesussaves-installer.exe
CLEANS += clean-installer
