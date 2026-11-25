/*
 * Copyright 2019 Vincent Sanders <vince@netsurf-browser.org>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "testament.h"

#include "desktop/version.h"

/* JESUSSAVES-v1.1.1: version is shown as "JesusSaves <netsurf_version>" by the About
 * dialogs. JESUSSAVES_BUILD_DATE defaults to the compiler date, so every rebuild
 * is stamped; override with -DJESUSSAVES_BUILD_DATE=\"...\" for reproducible builds. */
#ifndef JESUSSAVES_BUILD_DATE
#define JESUSSAVES_BUILD_DATE __DATE__
#endif
const char * const netsurf_version =
	"1.1.1 (based on NetSurf 3.11, modified by the JesusSaves project, built " JESUSSAVES_BUILD_DATE ")";
const int netsurf_version_major = 3;
const int netsurf_version_minor = 11;
