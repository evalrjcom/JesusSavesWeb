/*
 * Copyright 2016 Vincent Sanders <vince@netsurf-browser.org>
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

/**
 * \file
 * internet structures and defines
 *
 * This allows the obtaining of standard bsd sockets and associated
 * functions in a uniform way despite any oddities in headers and
 * supported API between OS.
 *
 * \note This functionality was previously provided as a side effect of the
 *  utils config header include.
 */

#ifndef _NETSURF_UTILS_INET_H_
#define _NETSURF_UTILS_INET_H_

#include "utils/config.h"

#ifdef HAVE_POSIX_INET_HEADERS

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>

#else

#include <winsock2.h>
#include <ws2tcpip.h>

#ifndef EAFNOSUPPORT
#define EAFNOSUPPORT WSAEAFNOSUPPORT
#endif

#endif


/* AUDIT-FIX B2 (v1.0.1): where the platform has no inet_aton()/inet_pton()
 * NetSurf supplies its own, and used to export them under the standard names.
 * On Windows that collides with the real inet_pton exported by ws2_32 as soon
 * as anything else in the link imports it (current libcurl and c-ares do):
 * "multiple definition of `inet_pton'".  It only went unnoticed because the
 * core was never linked as a library.  The fallbacks keep their behaviour but
 * now live in NetSurf's own namespace. */
#ifndef HAVE_INETATON
#define inet_aton netsurf__inet_aton
int inet_aton(const char *cp, struct in_addr *inp);
#endif

#ifndef HAVE_INETPTON
#define inet_pton netsurf__inet_pton
int inet_pton(int af, const char *src, void *dst);
#endif

#endif
