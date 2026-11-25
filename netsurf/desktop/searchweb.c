/*
 * Copyright 2014 Vincent Sanders <vince@netsurf-browser.org>
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
 * \brief core web search facilities implementation.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "utils/utils.h"
#include "utils/log.h"
#include "utils/url.h"
#include "utils/nsoption.h"
#include "netsurf/content.h"
#include "content/hlcache.h"

#include "desktop/searchweb.h"
#include "desktop/scripture.h"
#include "desktop/gui_internal.h"

struct search_provider {
	char *name; /**< readable name such as 'google', 'yahoo', etc */
	char *hostname; /**< host address such as www.google.com */
	char *searchstring; /** < such as "www.google.com?search=%s" */
	char *ico; /** < location of domain's favicon */
	hlcache_handle *ico_handle;
};

static struct search_web_ctx_s {
	struct search_provider *providers; /* web search providers */
	size_t providers_count; /* number of providers */

	size_t current; /* current provider */

	hlcache_handle *default_ico_handle;

	/* JESUSSAVES-v1.1.1 AUDIT-FIX S1: the provider fields point into this
	 * one block; it is owned here instead of being recovered through
	 * providers[0].name (which broke as soon as the file started with a
	 * blank line, a comment or a byte-order mark). */
	char *blob;
} search_web_ctx;


/* JESUSSAVES-v1.1.1: DuckDuckGo (HTML endpoint, works without JavaScript) is the
 * built-in fallback and the only shipped provider. The favicon field is left
 * empty on purpose: the bundled search icon is used, so no request is made
 * to the provider just to draw an icon. */
static const char *default_providers = "DuckDuckGo|duckduckgo.com|https://html.duckduckgo.com/html/?q=%s||\n";

static const char *default_search_icon_url = "resource:icons/search.png";


/**
 * Read providers file.
 *
 * Allocates storage of sufficient size for the providers file and
 * reads the entire file in.
 *
 * \param fname The filename to read.
 * \param providers_out A pointer to place the result buffer in.
 * \param providers_size_out Size of buffer.
 * \return NSERROR_OK and providers_out updated or appropriate error code.
 */
static nserror
read_providers(const char *fname,
	       char **providers_out,
	       size_t *providers_size_out)
{
	FILE *providersf;
	long ftellsize;
	size_t fsize;
	char *providersd;

	if (fname == NULL) {
		return NSERROR_BAD_PARAMETER;
	}

	providersf = fopen(fname, "r");
	if (providersf == NULL) {
		return NSERROR_NOT_FOUND;
	}

	if (fseek(providersf, 0, SEEK_END) != 0) {
		fclose(providersf);
		return NSERROR_INVALID;
	}

	ftellsize = ftell(providersf);
	if (ftellsize < 0) {
		fclose(providersf);
		return NSERROR_INVALID;
	}
	fsize = ftellsize;

	if (fseek(providersf, 0, SEEK_SET) != 0) {
		fclose(providersf);
		return NSERROR_INVALID;
	}

	providersd = malloc(fsize + 1);
	if (providersd == NULL) {
		fclose(providersf);
		return NSERROR_NOMEM;
	}

	if (fread(providersd, 1, fsize, providersf) != fsize) {
		fclose(providersf);
		free(providersd);
		return NSERROR_BAD_SIZE;
	}
	providersd[fsize] = 0; /* ensure null terminated */

	fclose(providersf);

	*providers_out = providersd;
	*providers_size_out = fsize;

	return NSERROR_OK;
}

/**
 * parse search providers from a memory block.
 *
 * JESUSSAVES-v1.1.1 AUDIT-FIX S2: rewritten. The old parser counted newlines to
 * size the array, so a blank line, a CRLF file, a file without a final newline
 * or one malformed line made the whole file fail (or scanned into the next
 * line's fields). Now: blank lines and '#' comments are skipped, CR/LF and a
 * UTF-8 BOM are tolerated, a malformed line is skipped instead of rejecting
 * the file, the favicon field is optional and only http(s) templates that
 * contain a %s marker are accepted.
 *
 * \param providersd The provider info data (NUL terminated, modified in place).
 * \param providers_size The size of the provider data.
 * \param providers_out The resulting provider array.
 * \param providers_count The number of providers in the output array.
 * \return NSERROR_OK on success or error code on failure.
 */
static nserror
parse_providers(char *providersd,
		size_t providers_size,
		struct search_provider **providers_out,
		size_t *providers_count)
{
	size_t pcount = 0; /* number of providers */
	size_t pcap = 0; /* allocated providers */
	struct search_provider *providers = NULL;
	char *line = providersd;
	char *next;

	(void)providers_size; /* the caller guarantees NUL termination */

	/* skip a UTF-8 byte order mark */
	if ((unsigned char)line[0] == 0xEF &&
	    (unsigned char)line[1] == 0xBB &&
	    (unsigned char)line[2] == 0xBF) {
		line += 3;
	}

	while (*line != '\0') {
		char *field[4];
		char *p;
		size_t len;
		int nf;

		/* isolate this line */
		next = strchr(line, '\n');
		if (next != NULL) {
			*next = '\0';
			next++;
		} else {
			next = line + strlen(line);
		}
		len = strlen(line);
		if (len > 0 && line[len - 1] == '\r') {
			line[len - 1] = '\0';
		}

		/* skip blank and comment lines */
		if (*line == '\0' || *line == '#') {
			line = next;
			continue;
		}

		/* split: name|hostname|searchstring[|favicon[|comment]] */
		field[0] = line;
		nf = 1;
		for (p = line; *p != '\0' && nf < 4; p++) {
			if (*p == '|') {
				*p = '\0';
				field[nf++] = p + 1;
			}
		}
		if (nf < 3) {
			NSLOG(netsurf, WARNING, "ignoring malformed search provider line");
			line = next;
			continue;
		}
		if (nf == 3) {
			field[3] = p; /* empty favicon: points at the line end */
		} else {
			p = strchr(field[3], '|');
			if (p != NULL) {
				*p = '\0'; /* trailing free text is a comment */
			}
		}

		/* only plain web search templates with a term marker */
		if (field[0][0] == '\0' ||
		    (strncmp(field[2], "https://", SLEN("https://")) != 0 &&
		     strncmp(field[2], "http://", SLEN("http://")) != 0) ||
		    strstr(field[2], "%s") == NULL) {
			NSLOG(netsurf, WARNING, "ignoring unusable search provider '%s'", field[0]);
			line = next;
			continue;
		}

		if (pcount == pcap) {
			struct search_provider *np;
			size_t ncap = (pcap == 0) ? 8 : pcap * 2;

			np = realloc(providers, ncap * sizeof(*providers));
			if (np == NULL) {
				free(providers);
				return NSERROR_NOMEM;
			}
			providers = np;
			pcap = ncap;
		}

		providers[pcount].name = field[0];
		providers[pcount].hostname = field[1];
		providers[pcount].searchstring = field[2];
		providers[pcount].ico = field[3];
		providers[pcount].ico_handle = NULL;
		pcount++;

		line = next;
	}

	if (pcount == 0) {
		free(providers);
		return NSERROR_INVALID;
	}

	*providers_out = providers;
	*providers_count = pcount;

	return NSERROR_OK;
}

/**
 * create a url for a search provider and a term
 *
 * \param provider The provider to use.
 * \param term The term being searched for.
 * \param url_out The resulting url.
 * \return NSERROR_OK on success or appropriate error code.
 */
static nserror
make_search_nsurl(struct search_provider *provider,
		const char *term,
		nsurl **url_out)
{
	nserror ret;
	nsurl *url;
	char *eterm; /* escaped term */
	const char *searchstr; /* the providers search string */
	const char *sp;
	char *urlstr; /* the escaped term substituted into the provider */
	char *dp;
	size_t eterm_len;
	size_t tmpl_len;
	size_t nsubst = 0; /* number of %s markers in the template */
	size_t urlstr_len;

	/* escape the search term and join it to the search url */
	ret = url_escape(term, true, NULL, &eterm);
	if (ret != NSERROR_OK) {
		return ret;
	}

	searchstr = provider->searchstring;
	eterm_len = strlen(eterm);
	tmpl_len = strlen(searchstr);

	/* JESUSSAVES-v1.1.1 AUDIT-FIX S3: size the buffer for EVERY %s marker.
	 * The old code allocated room for one substitution, so a template with
	 * two markers overflowed the heap. "%%" is now a literal percent sign
	 * (the shipped Wikipedia line was written as "%%3A" and sent "%%"). */
	for (sp = searchstr; *sp != '\0'; sp++) {
		if (sp[0] == '%' && sp[1] == 's') {
			nsubst++;
			sp++;
		} else if (sp[0] == '%' && sp[1] == '%') {
			sp++;
		}
	}
	if (eterm_len != 0 &&
	    nsubst > (SIZE_MAX - tmpl_len - 1) / eterm_len) {
		free(eterm);
		return NSERROR_NOMEM;
	}
	urlstr_len = tmpl_len + (nsubst * eterm_len) + 1;

	urlstr = malloc(urlstr_len);
	if (urlstr == NULL) {
		free(eterm);
		return NSERROR_NOMEM;
	}

	/* composite search url */
	dp = urlstr;
	for (sp = searchstr; *sp != '\0'; sp++) {
		if (sp[0] == '%' && sp[1] == 's') {
			memcpy(dp, eterm, eterm_len);
			dp += eterm_len;
			sp++;
		} else if (sp[0] == '%' && sp[1] == '%') {
			*dp++ = '%';
			sp++;
		} else {
			*dp++ = *sp;
		}
	}
	*dp = '\0'; /* ensure string is NULL-terminated */
	free(eterm);

	ret = nsurl_create(urlstr, &url);
	free(urlstr);
	if (ret != NSERROR_OK) {
		return ret;
	}

	*url_out = url;
	return NSERROR_OK;
}

/**
 * callback for hlcache icon fetch events.
 */
static nserror
search_web_ico_callback(hlcache_handle *ico,
			const hlcache_event *event,
			void *pw)
{
	struct search_provider *provider = pw;

	switch (event->type) {

	case CONTENT_MSG_DONE:
		NSLOG(netsurf, INFO, "icon '%s' retrieved",
		      nsurl_access(hlcache_handle_get_url(ico)));
		guit->search_web->provider_update(provider->name,
						  content_get_bitmap(ico));
		break;

	case CONTENT_MSG_ERROR:
		NSLOG(netsurf, INFO, "icon %s error: %s",
		      nsurl_access(hlcache_handle_get_url(ico)),
		      event->data.errordata.errormsg);

		hlcache_handle_release(ico);
		/* clear reference to released handle */
		provider->ico_handle = NULL;
		break;

	default:
		break;
	}

	return NSERROR_OK;
}

/**
 * Decide whether free text typed in the URL bar looks like an address.
 *
 * JESUSSAVES-v1.1.1 AUDIT-FIX S4: the core used to accept ANY text that
 * "https://" + text could parse, so a single word such as "jesus" or a phrase
 * such as "John 3:16" was sent to a host of that name and the web search was
 * never reached. Text is now an address only if it has no white space or
 * control characters and has a scheme, a dot, a port, a path, an IPv6
 * literal or is "localhost".
 *
 * \param term The text the user typed.
 * \return true if the text should be treated as an address.
 */
static bool search_web_term_is_address(const char *term)
{
	const char *p;
	bool has_hint = false;

	if (term[0] == '\0') {
		return false;
	}

	for (p = term; *p != '\0'; p++) {
		unsigned char c = (unsigned char)*p;

		if (c <= 0x20 || c == 0x7f) {
			return false; /* white space / control character */
		}
		if (c == '.' || c == ':' || c == '/' || c == '[') {
			has_hint = true;
		}
	}

	return has_hint || (strcasecmp(term, "localhost") == 0);
}

/* exported interface documented in desktop/searchweb.h */
nserror
search_web_omni(const char *term,
		enum search_web_omni_flags flags,
		struct nsurl **url_out)
{
	nserror ret;
	nsurl *url;
	char *eterm; /* encoded/altered search term */

	if (term == NULL) {
		return NSERROR_BAD_PARAMETER;
	}

	/* JESUSSAVES-v1.1.1: a Bible reference such as "John 3:16" or "1 Cor 13"
	 * opens the passage (KJV) instead of being searched or treated as a host.
	 * Checked before the address test because "john3:16" would otherwise
	 * look like host:port. */
	if ((flags & SEARCH_WEB_OMNI_SEARCHONLY) == 0 &&
	    nsoption_bool(scripture_urlbar)) {
		scripture_ref sref;

		if (scripture_parse(term, strlen(term), &sref)) {
			char *surl = scripture_url(&sref);

			if (surl == NULL) {
				return NSERROR_NOMEM;
			}
			ret = nsurl_create(surl, &url);
			free(surl);
			if (ret == NSERROR_OK) {
				*url_out = url;
				return NSERROR_OK;
			}
		}
	}

	if ((flags & SEARCH_WEB_OMNI_SEARCHONLY) == 0 &&
	    search_web_term_is_address(term)) {

		/* first check to see if the term is a url */
		ret = nsurl_create(term, &url);
		if (ret == NSERROR_OK) {
			*url_out = url;
			return NSERROR_OK;
		}

		/* try with adding default scheme */
		eterm = malloc(strlen(term) + SLEN("https://") + 1);
		if (eterm == NULL) {
			return NSERROR_NOMEM;
		}
		sprintf(eterm, "https://%s", term);
		ret = nsurl_create(eterm, &url);
		free(eterm);
		if (ret == NSERROR_OK) {
			*url_out = url;
			return NSERROR_OK;
		}
	}

	if ((flags & SEARCH_WEB_OMNI_SEARCHONLY) == 0) {
		/* do not pass to search if user has disabled the option */
		if (nsoption_bool(search_url_bar) == false) {
			return NSERROR_BAD_URL;
		}
	}

	/* must be initialised */
	if (search_web_ctx.providers == NULL) {
		return NSERROR_INIT_FAILED;
	}

	/* turn search into a nsurl */
	ret = make_search_nsurl(&search_web_ctx.providers[search_web_ctx.current], term, &url);
	if (ret != NSERROR_OK) {
		return ret;
	}

	*url_out = url;
	return NSERROR_OK;
}

/* exported interface documented in desktop/searchweb.h */
nserror search_web_get_provider_bitmap(struct bitmap **bitmap_out)
{
	struct search_provider *provider;
	struct bitmap *ico_bitmap = NULL;

	/* must be initialised */
	if (search_web_ctx.providers == NULL) {
		return NSERROR_INIT_FAILED;
	}

	provider = &search_web_ctx.providers[search_web_ctx.current];

	/* set the icon now (if we can) at least to the default */
	if (provider->ico_handle != NULL) {
		ico_bitmap = content_get_bitmap(provider->ico_handle);
	}
	if ((ico_bitmap == NULL) &&
	    (search_web_ctx.default_ico_handle != NULL)) {
		ico_bitmap = content_get_bitmap(search_web_ctx.default_ico_handle);
	}

	*bitmap_out = ico_bitmap;
	return NSERROR_OK;
}


/* exported interface documented in desktop/searchweb.h */
nserror search_web_select_provider(int selection)
{
	struct search_provider *provider;
	struct bitmap *ico_bitmap = NULL;

	/* must be initialised */
	if (search_web_ctx.providers == NULL) {
		return NSERROR_INIT_FAILED;
	}

	/* negative value just selects whatevers current */
	if (selection >= 0) {
		/* ensure selection lies within acceptable range */
		if ((size_t)selection < search_web_ctx.providers_count) {
			search_web_ctx.current = selection;
		} else {
			/* out of range */
			search_web_ctx.current = 0;
		}
	}

	provider = &search_web_ctx.providers[search_web_ctx.current];

	/* set the icon now (if we can) at least to the default */
	if (provider->ico_handle != NULL) {
		ico_bitmap = content_get_bitmap(provider->ico_handle);
	}
	if ((ico_bitmap == NULL) &&
	    (search_web_ctx.default_ico_handle != NULL)) {
		ico_bitmap = content_get_bitmap(search_web_ctx.default_ico_handle);
	}
	/* update the callback with the provider change. Bitmap may
	 * be NULL at this point.
	 */
	guit->search_web->provider_update(provider->name, ico_bitmap);


	/* if the providers icon has not been retrieved get it now
	 * (JESUSSAVES-v1.1.1: an empty favicon field means "use the default icon") */
	if (provider->ico_handle == NULL && provider->ico[0] != '\0') {
		nsurl *icon_nsurl;
		nserror ret;

		/* create search icon url */
		ret = nsurl_create(provider->ico, &icon_nsurl);
		if (ret != NSERROR_OK) {
			return ret;
		}

		ret = hlcache_handle_retrieve(icon_nsurl, 0, NULL, NULL,
					      search_web_ico_callback,
					      provider,
					      NULL, CONTENT_IMAGE,
					      &provider->ico_handle);
		nsurl_unref(icon_nsurl);
		if (ret != NSERROR_OK) {
			provider->ico_handle = NULL;
			return ret;
		}
	}

	return NSERROR_OK;
}

/**
 * callback for hlcache icon fetch events.
 */
static nserror
default_ico_callback(hlcache_handle *ico,
		     const hlcache_event *event,
		     void *pw)
{
	struct search_web_ctx_s *ctx = pw;

	switch (event->type) {

	case CONTENT_MSG_DONE:
		NSLOG(netsurf, INFO, "default icon '%s' retrieved",
		      nsurl_access(hlcache_handle_get_url(ico)));

		/* only set to default icon if providers icon has no handle */
		if (ctx->providers[search_web_ctx.current].ico_handle == NULL) {
			guit->search_web->provider_update(
				ctx->providers[search_web_ctx.current].name,
				content_get_bitmap(ico));
		}
		break;

	case CONTENT_MSG_ERROR:
		NSLOG(netsurf, INFO, "icon %s error: %s",
		      nsurl_access(hlcache_handle_get_url(ico)),
		      event->data.errordata.errormsg);

		hlcache_handle_release(ico);
		/* clear reference to released handle */
		ctx->default_ico_handle = NULL;
		break;

	default:
		break;
	}

	return NSERROR_OK;
}

/* exported interface documented in desktop/searchweb.h */
ssize_t search_web_iterate_providers(ssize_t from, const char **name)
{
	if (from < 0)
		return -1;

	if ((size_t)from >= search_web_ctx.providers_count)
		return -1;

	*name = search_web_ctx.providers[from].name;

	return from + 1;
}


/* exported interface documented in desktop/searchweb.h */
nserror search_web_init(const char *provider_fname)
{
	nserror ret;
	char *providers = NULL;
	size_t providers_size = 0;
	nsurl *icon_nsurl;

	/* create search icon url */
	ret = nsurl_create(default_search_icon_url, &icon_nsurl);
	if (ret != NSERROR_OK) {
		return ret;
	}

	/* get a list of providers */
	ret = read_providers(provider_fname, &providers, &providers_size);
	if (ret == NSERROR_OK) {
		/* parse list of providers */
		ret = parse_providers(providers,
				      providers_size,
				      &search_web_ctx.providers,
				      &search_web_ctx.providers_count);
		if (ret != NSERROR_OK) {
			/* JESUSSAVES-v1.1.1 AUDIT-FIX S5: an unusable file now falls
			 * back to the built-in list instead of failing start-up */
			NSLOG(netsurf, WARNING, "search providers file unusable, using built-in list");
			free(providers);
			providers = NULL;
		}
	}
	if (ret != NSERROR_OK) {
		providers = strdup(default_providers);
		if (providers == NULL) {
			nsurl_unref(icon_nsurl);
			return NSERROR_NOMEM;
		}
		providers_size = strlen(providers);
		ret = parse_providers(providers,
				      providers_size,
				      &search_web_ctx.providers,
				      &search_web_ctx.providers_count);
		if (ret != NSERROR_OK) {
			free(providers);
			nsurl_unref(icon_nsurl); /* S5: was leaked on this path */
			return ret;
		}
	}
	search_web_ctx.blob = providers;
	search_web_ctx.current = 0;

	/* get default search icon */
	ret = hlcache_handle_retrieve(icon_nsurl,
				      0,
				      NULL,
				      NULL,
				      default_ico_callback,
				      &search_web_ctx,
				      NULL,
				      CONTENT_IMAGE,
				      &search_web_ctx.default_ico_handle);
	nsurl_unref(icon_nsurl);
	if (ret != NSERROR_OK) {
		search_web_ctx.default_ico_handle = NULL;
		free(search_web_ctx.providers);
		search_web_ctx.providers = NULL;
		search_web_ctx.providers_count = 0;
		free(search_web_ctx.blob);
		search_web_ctx.blob = NULL;
		return ret;
	}


	return NSERROR_OK;
}

/* exported interface documented in desktop/searchweb.h */
nserror search_web_finalise(void)
{
	size_t pidx;

	/* must be initialised */
	if (search_web_ctx.providers == NULL) {
		return NSERROR_INIT_FAILED;
	}

	if (search_web_ctx.default_ico_handle != NULL) {
		hlcache_handle_release(search_web_ctx.default_ico_handle);
		search_web_ctx.default_ico_handle = NULL;
	}
	for (pidx = 0; pidx < search_web_ctx.providers_count; pidx++) {
		if (search_web_ctx.providers[pidx].ico_handle != NULL) {
			hlcache_handle_release(search_web_ctx.providers[pidx].ico_handle);
		}
	}

	/* All the search provider data is held in a single block for
	 * efficiency; S1: that block is owned by the context. */
	free(search_web_ctx.blob);
	search_web_ctx.blob = NULL;

	free(search_web_ctx.providers);
	search_web_ctx.providers = NULL;
	search_web_ctx.providers_count = 0;
	search_web_ctx.current = 0;

	return NSERROR_OK;
}
