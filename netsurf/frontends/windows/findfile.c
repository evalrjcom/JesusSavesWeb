/*
 * Copyright 2008 Daniel Silverstone <dsilvers@netsurf-browser.org>
 * Copyright 2009 Mark Benjamin <netsurf-browser.org.MarkBenjamin@dfgh.net>
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

#include <winsock2.h>
#include <windows.h>

#include <limits.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils/log.h"
#include "utils/url.h"
#include "utils/utils.h"
#include "utils/filepath.h"

#include "windows/findfile.h"

/** Create an array of valid paths to search for resources.
 *
 * The idea is that all the complex path computation to find resources
 * is performed here, once, rather than every time a resource is
 * searched for.
 */
char **
nsws_init_resource(const char *resource_path)
{
	char **pathv; /* resource path string vector */
	char **respath; /* resource paths vector */
	const char *lang = NULL;
	char *winpath;
	DWORD wlen;
	int pathi;
	char *slsh;
	static const char suffix[] = "\\windows\\res";

	pathv = filepath_path_to_strvec(resource_path);
	if (pathv == NULL)
		return NULL;

	/* JESUSSAVES-v1.1.2 AUDIT-FIX W1: the buffer was unchecked, the module
	 * name could be returned truncated and unterminated (a long install
	 * path), and strncat() was bounded by MAX_PATH instead of by the space
	 * left, so a long path overflowed the heap block. */
	winpath = malloc(MAX_PATH + sizeof(suffix));
	if (winpath == NULL) {
		filepath_free_strvec(pathv);
		return NULL;
	}
	wlen = GetModuleFileName(NULL, winpath, MAX_PATH);
	if (wlen == 0 || wlen >= MAX_PATH) {
		/* failed or truncated: do not search next to the executable */
		free(winpath);
		winpath = NULL;
	} else {
		winpath[wlen] = '\0';
		slsh = strrchr(winpath, '\\');
		if (slsh != NULL)
			*slsh = 0;
		strcat(winpath, suffix); /* room reserved above */
	}

	pathi = 0;
	while (pathv[pathi] != NULL)
		pathi++;
	if (winpath != NULL) { /* filepath_path_to_strvec() always leaves spare, zeroed slots */
		pathv[pathi] = winpath;
	} else {
		free(winpath);
	}

	respath = filepath_generate(pathv, &lang);

	filepath_free_strvec(pathv);

	return respath;
}

/**
 * Stand-in for POSIX realpath() on Windows.
 *
 * JESUSSAVES-v1.1.2 AUDIT-FIX W2: the old stub used strncpy(dst, src,
 * PATH_MAX), which does not terminate the result when the source is
 * PATH_MAX characters or longer. The copy is now always terminated and a
 * too-long path is reported as failure (NULL).
 */
static char *realpath(const char *path, char *resolved_path)
{
	size_t len = strlen(path);

	if (len >= PATH_MAX) {
		return NULL;
	}
	memcpy(resolved_path, path, len + 1);
	return resolved_path;
}

/**
 * Append src to the string in dst (capacity cap), failing if it does not fit.
 *
 * \return true if the whole of src was appended
 */
static bool nsws_append(char *dst, size_t cap, const char *src)
{
	size_t dl = strlen(dst);
	size_t sl = strlen(src);

	if (dl + sl + 1 > cap) {
		return false;
	}
	memcpy(dst + dl, src, sl + 1);
	return true;
}


/**
 * Locate a shared resource file by searching known places in order.
 *
 * Search order is: ~/.netsurf/, $NETSURFRES/ (where NETSURFRES is an
 * environment variable), then the path specified in
 * NETSURF_WINDOWS_RESPATH in the Makefile then .\\res\\ [windows paths]
 *
 * JESUSSAVES-v1.1.2 AUDIT-FIX W3: every strcpy()/strcat() into the PATH_MAX
 * stack buffer and into the caller's buffer (HOME, NETSURFRES and the file
 * name are all caller/environment controlled) is now length checked; an
 * unset HOME no longer reaches snprintf("%s"); a failed getcwd() no longer
 * leaves the buffer uninitialised.
 *
 * \param  buf      buffer to write to.  must be at least PATH_MAX chars
 * \param  filename file to look for
 * \param  def      default to return if file not found
 * \return The passed in buffer
 */

char *nsws_find_resource(char *buf, const char *filename, const char *def)
{
	char *cdir = getenv("HOME");
	char t[PATH_MAX];

	if (cdir != NULL) {
		NSLOG(netsurf, INFO, "Found Home %s", cdir);
		t[0] = '\0';
		if (nsws_append(t, sizeof(t), cdir) &&
		    nsws_append(t, sizeof(t), "/.netsurf/") &&
		    nsws_append(t, sizeof(t), filename) &&
		    (realpath(t, buf) != NULL) && (access(buf, R_OK) == 0))
			return buf;
	}

	cdir = getenv("NETSURFRES");

	if (cdir != NULL) {
		if (realpath(cdir , buf) != NULL &&
		    nsws_append(buf, PATH_MAX, "/") &&
		    nsws_append(buf, PATH_MAX, filename) &&
		    access(buf, R_OK) == 0)
			return buf;
	}

	t[0] = '\0';
	if (nsws_append(t, sizeof(t), NETSURF_WINDOWS_RESPATH) &&
	    nsws_append(t, sizeof(t), filename) &&
	    (realpath(t, buf) != NULL) && (access(buf, R_OK) == 0))
		return buf;

	if (getcwd(t, sizeof(t)) != NULL &&
	    nsws_append(t, sizeof(t), "\\res\\") &&
	    nsws_append(t, sizeof(t), filename)) {
		NSLOG(netsurf, INFO, "looking in %s", t);
		if ((realpath(t, buf) != NULL) && (access(buf, R_OK) == 0))
			return buf;
	}

	if (def[0] == '~') {
		cdir = getenv("HOME");
		if (cdir == NULL) {
			cdir = "";
		}
		snprintf(t, PATH_MAX, "%s%s", cdir, def + 1);
		if (realpath(t, buf) == NULL) {
			snprintf(buf, PATH_MAX, "%s", t);
		}
	} else {
		if (realpath(def, buf) == NULL) {
			snprintf(buf, PATH_MAX, "%s", def);
		}
	}

	return buf;
}


/*
 * Local Variables:
 * c-basic-offset: 8
 * End:
 */

