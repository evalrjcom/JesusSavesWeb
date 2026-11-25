/*
 * Copyright 2014, 2015 Vincent Sanders <vince@netsurf-browser.org>
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
 * Windows file operation table implementation.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <windows.h>

#include "utils/errors.h"
#include "utils/nsurl.h"
#include "utils/log.h"
#include "utils/utils.h"
#include "utils/corestrings.h"
#include "utils/url.h"
#include "utils/file.h"
#include "utils/string.h"
#include "netsurf/browser_window.h"

#include "windows/file.h"

/**
 * Generate a windows path from one or more component elemnts.
 *
 * If a string is allocated it must be freed by the caller.
 *
 * @param[in,out] str pointer to string pointer if this is NULL enough
 *                    storage will be allocated for the complete path.
 * @param[in,out] size The size of the space available if \a str not
 *                     NULL on input and if not NULL set to the total
 *                     output length on output.
 * @param[in] nelm The number of elements.
 * @param[in] ap The elements of the path as string pointers.
 * @return NSERROR_OK and the complete path is written to str
 *         or error code on faliure.
 */
static nserror windows_mkpath(char **str, size_t *size, size_t nelm, va_list ap)
{
	return vsnstrjoin(str, size, '\\', nelm, ap);
}


/**
 * Get the basename of a file using windows path handling.
 *
 * This gets the last element of a path and returns it.
 *
 * @param[in] path The path to extract the name from.
 * @param[in,out] str Pointer to string pointer if this is NULL enough
 *                    storage will be allocated for the path element.
 * @param[in,out] size The size of the space available if \a
 *                     str not NULL on input and set to the total
 *                     output length on output.
 * @return NSERROR_OK and the complete path is written to str
 *         or error code on faliure.
 */
static nserror windows_basename(const char *path, char **str, size_t *size)
{
	const char *leafname;
	char *fname;

	if (path == NULL) {
		return NSERROR_BAD_PARAMETER;
	}

	leafname = strrchr(path, '\\');
	if (!leafname) {
		leafname = path;
	} else {
		leafname += 1;
	}

	fname = strdup(leafname);
	if (fname == NULL) {
		return NSERROR_NOMEM;
	}

	*str = fname;
	if (size != NULL) {
		*size = strlen(fname);
	}
	return NSERROR_OK;
}


/**
 * Create a path from a nsurl using windows file handling.
 *
 * @param[in] url The url to encode.
 * @param[out] path_out A string containing the result path which should
 *                      be freed by the caller.
 * @return NSERROR_OK and the path is written to \a path or error code
 *         on faliure.
 */
static nserror windows_nsurl_to_path(struct nsurl *url, char **path_out)
{
	lwc_string *urlpath;
	char *path;
	bool match;
	lwc_string *scheme;
	nserror res;

	if ((url == NULL) || (path_out == NULL)) {
		return NSERROR_BAD_PARAMETER;
	}

	scheme = nsurl_get_component(url, NSURL_SCHEME);

	if (lwc_string_caseless_isequal(scheme, corestring_lwc_file,
					&match) != lwc_error_ok)
	{
		lwc_string_unref(scheme); /* JESUSSAVES-v1.1.2 AUDIT-FIX W10: was leaked */
		return NSERROR_BAD_PARAMETER;
	}
	lwc_string_unref(scheme);
	if (match == false) {
		return NSERROR_BAD_PARAMETER;
	}

	urlpath = nsurl_get_component(url, NSURL_PATH);
	if (urlpath == NULL) {
		return NSERROR_BAD_PARAMETER;
	}

	res = url_unescape(lwc_string_data(urlpath),
			   lwc_string_length(urlpath),
			   NULL,
			   &path);
	lwc_string_unref(urlpath);
	if (res != NSERROR_OK) {
		return res;
	}

	/* if there is a drive: prefix treat path as DOS filename.
	 * W11: the old test read path[2] without checking the length, so a
	 * file URL whose path was one character long ("file:///a" is fine,
	 * "file:" + "/" is not) read past the end of the string. */
	if (strlen(path) >= 3 && ((path[2] == ':') ||  (path[2] == '|'))) {
		char *sidx; /* slash index */

		/* move the string down to remove leading / note the
		 * strlen is *not* copying too much data as we are
		 * moving the null too!
		 */
		memmove(path, path + 1, strlen(path));

		/* swap / for \ in one pass (was quadratic) */
		for (sidx = path; *sidx != '\0'; sidx++) {
			if (*sidx == '/') {
				*sidx = '\\';
			}
		}
	}
	/* if the path does not have a drive letter we return the
	 * complete path.
	 */
	/** @todo Need to check returning the unaltered path in this
	 * case is correct
	 */

	*path_out = path;

	return NSERROR_OK;
}


/**
 * Create a nsurl from a path using windows file handling.
 *
 * Perform the necessary operations on a path to generate a nsurl.
 *
 * @param[in] path The path to convert.
 * @param[out] url_out pointer to recive the nsurl, The returned url
 *                     should be unreferenced by the caller.
 * @return NSERROR_OK and the url is placed in \a url or error code on
 *         faliure.
 */
static nserror windows_path_to_nsurl(const char *path, struct nsurl **url_out)
{
	nserror ret;
	char *fpath; /* path with forward slashes (and wine drive if needed) */
	char *epath; /* percent-escaped path */
	char *urlstr;
	char *sidx; /* slash index */
	size_t plen;
	size_t urllen;

	if ((path == NULL) || (url_out == NULL) || (*path == 0)) {
		return NSERROR_BAD_PARAMETER;
	}

	/* JESUSSAVES-v1.1.2 AUDIT-FIX W12: the path was pasted into the URL
	 * unescaped (the old "@todo check if this should be url escaping"), so
	 * a file called "a#1.html", "what?.html" or "100%.html" opened a
	 * different resource -- the same defect fixed for the GTK front end in
	 * v1.0.0 (F6). The path is now percent-escaped, keeping '/' and ':'. */
	plen = strlen(path);
	fpath = malloc(plen + 3); /* optional "Z:" + NUL */
	if (fpath == NULL) {
		return NSERROR_NOMEM;
	}
	if (*path == '/') {
		/* unix style path start, so try wine Z: */
		memcpy(fpath, "Z:", 2);
		memcpy(fpath + 2, path, plen + 1);
	} else {
		memcpy(fpath, path, plen + 1);
	}
	for (sidx = fpath; *sidx != '\0'; sidx++) {
		if (*sidx == '\\') {
			*sidx = '/';
		}
	}

	ret = url_escape(fpath, false, ":/", &epath);
	free(fpath);
	if (ret != NSERROR_OK) {
		return ret;
	}

	urllen = strlen(epath) + FILE_SCHEME_PREFIX_LEN + 1;
	urlstr = malloc(urllen);
	if (urlstr == NULL) {
		free(epath);
		return NSERROR_NOMEM;
	}
	snprintf(urlstr, urllen, "%s%s", FILE_SCHEME_PREFIX, epath);
	free(epath);

	ret = nsurl_create(urlstr, url_out);
	free(urlstr);

	return ret;
}


/**
 * Ensure that all directory elements needed to store a filename exist.
 *
 * @param fname The filename to ensure the path to exists.
 * @return NSERROR_OK on success or error code on failure.
 */
static nserror windows_mkdir_all(const char *fname)
{
	char *dname;
	char *sep;
	struct stat sb;

	dname = strdup(fname);
	if (dname == NULL) {
		return NSERROR_NOMEM; /* W13: was dereferenced unchecked */
	}

	sep = strrchr(dname, '\\');
	if (sep == NULL) {
		/* no directory separator path is just filename so its ok */
		free(dname);
		return NSERROR_OK;
	}

	*sep = 0; /* null terminate directory path */

	if (stat(dname, &sb) == 0) {
		free(dname);
		if (S_ISDIR(sb.st_mode)) {
			/* path to file exists and is a directory */
			return NSERROR_OK;
		}
		return NSERROR_NOT_DIRECTORY;
	}
	*sep = '\\'; /* restore separator */

	sep = dname;
	while (*sep == '\\') {
		sep++;
	}
	while ((sep = strchr(sep, '\\')) != NULL) {
		*sep = 0;
		if (stat(dname, &sb) != 0) {
			if (nsmkdir(dname, S_IRWXU) != 0) {
				/* could not create path element */
				free(dname);
				return NSERROR_NOT_FOUND;
			}
		} else {
			if (! S_ISDIR(sb.st_mode)) {
				/* path element not a directory */
				free(dname);
				return NSERROR_NOT_DIRECTORY;
			}
		}
		*sep = '\\'; /* restore separator */
		/* skip directory separators */
		while (*sep == '\\') {
			sep++;
		}
	}

	free(dname);
	return NSERROR_OK;
}

/* windows file handling */
static struct gui_file_table file_table = {
	.mkpath = windows_mkpath,
	.basename = windows_basename,
	.nsurl_to_path = windows_nsurl_to_path,
	.path_to_nsurl = windows_path_to_nsurl,
	.mkdir_all = windows_mkdir_all,
};

struct gui_file_table *win32_file_table = &file_table;
