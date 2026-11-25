/*
 * Copyright 2026 The JesusSaves project
 *
 * This file is part of JesusSaves, a modified build of NetSurf.
 * Licensed under the GNU General Public License, version 2.
 */

/**
 * \file
 * JesusSaves v1.1.1: Bible reference parser and offline KJV lookup.
 */

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "desktop/scripture.h"

#define SCRIPTURE_MAXREF 96	/* longest text considered a reference */

static const char * const book_names[66] = {
	"Genesis", "Exodus", "Leviticus", "Numbers", "Deuteronomy", "Joshua",
	"Judges", "Ruth", "1 Samuel", "2 Samuel", "1 Kings", "2 Kings",
	"1 Chronicles", "2 Chronicles", "Ezra", "Nehemiah", "Esther", "Job",
	"Psalms", "Proverbs", "Ecclesiastes", "Song of Solomon", "Isaiah",
	"Jeremiah", "Lamentations", "Ezekiel", "Daniel", "Hosea", "Joel",
	"Amos", "Obadiah", "Jonah", "Micah", "Nahum", "Habakkuk", "Zephaniah",
	"Haggai", "Zechariah", "Malachi", "Matthew", "Mark", "Luke", "John",
	"Acts", "Romans", "1 Corinthians", "2 Corinthians", "Galatians",
	"Ephesians", "Philippians", "Colossians", "1 Thessalonians",
	"2 Thessalonians", "1 Timothy", "2 Timothy", "Titus", "Philemon",
	"Hebrews", "James", "1 Peter", "2 Peter", "1 John", "2 John", "3 John",
	"Jude", "Revelation"
};

/* Normalised (lower case, no spaces/dots) names and abbreviations.  Very
 * short words that are common English or numbers in other contexts (is, am,
 * ex, re, ti, la, ...) are deliberately left out so that ordinary searches
 * such as "ti 84" are not hijacked. */
static const struct { const char *key; int book; } aliases[] = {
	{"genesis",1},{"gen",1},{"gn",1},
	{"exodus",2},{"exod",2},{"exo",2},
	{"leviticus",3},{"lev",3},{"lv",3},
	{"numbers",4},{"num",4},{"nm",4},{"nb",4},
	{"deuteronomy",5},{"deut",5},{"deu",5},{"dt",5},
	{"joshua",6},{"josh",6},{"jos",6},{"jsh",6},
	{"judges",7},{"judg",7},{"jdg",7},{"jg",7},
	{"ruth",8},{"rut",8},
	{"1samuel",9},{"1sam",9},{"1sa",9},{"1sm",9},
	{"2samuel",10},{"2sam",10},{"2sa",10},{"2sm",10},
	{"1kings",11},{"1kgs",11},{"1ki",11},
	{"2kings",12},{"2kgs",12},{"2ki",12},
	{"1chronicles",13},{"1chron",13},{"1chr",13},{"1ch",13},
	{"2chronicles",14},{"2chron",14},{"2chr",14},{"2ch",14},
	{"ezra",15},{"ezr",15},
	{"nehemiah",16},{"neh",16},
	{"esther",17},{"esth",17},{"est",17},
	{"job",18},{"jb",18},
	{"psalms",19},{"psalm",19},{"ps",19},{"psa",19},{"pslm",19},{"pss",19},
	{"proverbs",20},{"prov",20},{"pro",20},{"prv",20},{"pr",20},
	{"ecclesiastes",21},{"eccles",21},{"eccl",21},{"ecc",21},
	{"songofsolomon",22},{"songofsongs",22},{"song",22},{"sos",22},
		{"canticles",22},
	{"isaiah",23},{"isa",23},
	{"jeremiah",24},{"jer",24},
	{"lamentations",25},{"lam",25},
	{"ezekiel",26},{"ezek",26},{"eze",26},{"ezk",26},
	{"daniel",27},{"dan",27},{"dn",27},
	{"hosea",28},{"hos",28},
	{"joel",29},{"jl",29},
	{"amos",30},{"amo",30},
	{"obadiah",31},{"obad",31},{"oba",31},{"ob",31},
	{"jonah",32},{"jnh",32},
	{"micah",33},{"mic",33},{"mc",33},
	{"nahum",34},{"nah",34},
	{"habakkuk",35},{"hab",35},{"hb",35},
	{"zephaniah",36},{"zeph",36},{"zep",36},{"zp",36},
	{"haggai",37},{"hag",37},{"hg",37},
	{"zechariah",38},{"zech",38},{"zec",38},{"zc",38},
	{"malachi",39},{"mal",39},{"ml",39},
	{"matthew",40},{"matt",40},{"mat",40},{"mt",40},
	{"mark",41},{"mrk",41},{"mar",41},{"mk",41},
	{"luke",42},{"luk",42},{"lk",42},
	{"john",43},{"joh",43},{"jhn",43},{"jn",43},
	{"acts",44},{"act",44},
	{"romans",45},{"rom",45},{"rm",45},
	{"1corinthians",46},{"1cor",46},{"1co",46},
	{"2corinthians",47},{"2cor",47},{"2co",47},
	{"galatians",48},{"gal",48},
	{"ephesians",49},{"eph",49},{"ephes",49},
	{"philippians",50},{"phil",50},{"php",50},
	{"colossians",51},{"col",51},
	{"1thessalonians",52},{"1thess",52},{"1thes",52},{"1th",52},
	{"2thessalonians",53},{"2thess",53},{"2thes",53},{"2th",53},
	{"1timothy",54},{"1tim",54},{"1ti",54},
	{"2timothy",55},{"2tim",55},{"2ti",55},
	{"titus",56},{"tit",56},
	{"philemon",57},{"philem",57},{"phm",57},
	{"hebrews",58},{"heb",58},
	{"james",59},{"jas",59},{"jm",59},
	{"1peter",60},{"1pet",60},{"1pe",60},{"1pt",60},
	{"2peter",61},{"2pet",61},{"2pe",61},{"2pt",61},
	{"1john",62},{"1jn",62},{"1jo",62},{"1joh",62},
	{"2john",63},{"2jn",63},{"2jo",63},{"2joh",63},
	{"3john",64},{"3jn",64},{"3jo",64},{"3joh",64},
	{"jude",65},{"jd",65},
	{"revelation",66},{"revelations",66},{"rev",66},{"rv",66},
		{"apocalypse",66},
};

/* ------------------------------------------------------------------ KJV */

static struct {
	char *text;		/* inflated "b:c:v<TAB>text\n" records */
	uint32_t *off;		/* record start offsets */
	uint32_t *key;		/* b*1000000 + c*1000 + v, ascending */
	size_t count;
	bool tried;		/* load attempted (never retry a failure) */
} kjv;

#define KEY(b, c, v) ((uint32_t)(b) * 1000000u + (uint32_t)(c) * 1000u + (uint32_t)(v))

static bool kjv_load(void)
{
	size_t i, n = 0, line;
	uLongf rawlen;
	char *p;

	if (kjv.tried) {
		return kjv.count != 0;
	}
	kjv.tried = true;

	rawlen = (uLongf)scripture_kjv_raw_len;
	kjv.text = malloc(rawlen + 1);
	if (kjv.text == NULL) {
		return false;
	}
	if (uncompress((Bytef *)kjv.text, &rawlen, scripture_kjv_z,
		       (uLong)scripture_kjv_z_len) != Z_OK ||
	    rawlen != scripture_kjv_raw_len) {
		goto fail;
	}
	kjv.text[rawlen] = '\0';

	for (i = 0; i < rawlen; i++) {
		if (kjv.text[i] == '\n') {
			n++;
		}
	}
	kjv.off = malloc(n * sizeof(*kjv.off));
	kjv.key = malloc(n * sizeof(*kjv.key));
	if (kjv.off == NULL || kjv.key == NULL) {
		goto fail;
	}

	line = 0;
	p = kjv.text;
	while (line < n && *p != '\0') {
		char *e;
		unsigned long b, c, v;

		b = strtoul(p, &e, 10);
		if (*e != ':') goto fail;
		c = strtoul(e + 1, &e, 10);
		if (*e != ':') goto fail;
		v = strtoul(e + 1, &e, 10);
		if (*e != '\t' || b < 1 || b > 66 || c > 999 || v > 999) goto fail;

		kjv.off[line] = (uint32_t)(e + 1 - kjv.text);
		kjv.key[line] = KEY(b, c, v);
		if (line > 0 && kjv.key[line] <= kjv.key[line - 1]) goto fail;
		line++;

		p = strchr(e, '\n');
		if (p == NULL) goto fail;
		p++;
	}
	if (line != n) {
		goto fail;
	}
	kjv.count = n;
	return true;

fail:
	scripture_finalise();
	kjv.tried = true;	/* do not retry */
	return false;
}

void scripture_finalise(void)
{
	free(kjv.text);
	free(kjv.off);
	free(kjv.key);
	memset(&kjv, 0, sizeof(kjv));
}

/** first index whose key >= k */
static size_t kjv_lower_bound(uint32_t k)
{
	size_t lo = 0, hi = kjv.count;

	while (lo < hi) {
		size_t mid = lo + (hi - lo) / 2;
		if (kjv.key[mid] < k) {
			lo = mid + 1;
		} else {
			hi = mid;
		}
	}
	return lo;
}

/** number of chapters in book (0 if unknown) */
static int kjv_chapters(int book)
{
	size_t i = kjv_lower_bound(KEY(book + 1, 0, 0));

	if (i == 0 || kjv.key[i - 1] / 1000000u != (uint32_t)book) {
		return 0;
	}
	return (int)((kjv.key[i - 1] / 1000u) % 1000u);
}

/** number of verses in chapter (0 if unknown) */
static int kjv_verses(int book, int chapter)
{
	size_t i = kjv_lower_bound(KEY(book, chapter + 1, 0));

	if (i == 0 || kjv.key[i - 1] / 1000u != KEY(book, chapter, 0) / 1000u) {
		return 0;
	}
	return (int)(kjv.key[i - 1] % 1000u);
}

/* --------------------------------------------------------------- parsing */

static int book_lookup(const char *key)
{
	size_t i;

	for (i = 0; i < sizeof(aliases) / sizeof(aliases[0]); i++) {
		if (strcmp(aliases[i].key, key) == 0) {
			return aliases[i].book;
		}
	}
	return 0;
}

/** Normalise a book name: lower case, drop spaces and dots, fold
 *  first/second/third and 1st/2nd/3rd to a digit.  false on other chars. */
static bool normalise_book(const char *s, size_t len, char *out, size_t outlen)
{
	size_t i, o = 0;
	char tmp[SCRIPTURE_MAXREF];

	if (len >= sizeof(tmp)) {
		return false;
	}
	for (i = 0; i < len; i++) {
		unsigned char c = (unsigned char)s[i];
		if (c == ' ' || c == '.' || c == '\t') {
			continue;
		}
		if (!isalnum(c)) {
			return false;
		}
		tmp[o++] = (char)tolower(c);
	}
	tmp[o] = '\0';

	if (strncmp(tmp, "first", 5) == 0) {
		snprintf(out, outlen, "1%s", tmp + 5);
	} else if (strncmp(tmp, "second", 6) == 0) {
		snprintf(out, outlen, "2%s", tmp + 6);
	} else if (strncmp(tmp, "third", 5) == 0) {
		snprintf(out, outlen, "3%s", tmp + 5);
	} else if (isdigit((unsigned char)tmp[0]) &&
		   (strncmp(tmp + 1, "st", 2) == 0 ||
		    strncmp(tmp + 1, "nd", 2) == 0 ||
		    strncmp(tmp + 1, "rd", 2) == 0) &&
		   isalpha((unsigned char)tmp[3])) {
		snprintf(out, outlen, "%c%s", tmp[0], tmp + 3);
	} else {
		snprintf(out, outlen, "%s", tmp);
	}
	return out[0] != '\0';
}

/** parse digits at *p, advance; false if none or too large */
static bool take_number(const char **p, int *val)
{
	long v = 0;
	const char *s = *p;

	if (!isdigit((unsigned char)*s)) {
		return false;
	}
	while (isdigit((unsigned char)*s)) {
		v = v * 10 + (*s - '0');
		if (v > 999) {
			return false;
		}
		s++;
	}
	*p = s;
	*val = (int)v;
	return true;
}

bool scripture_parse(const char *text, size_t len, scripture_ref *ref)
{
	char buf[SCRIPTURE_MAXREF];
	char key[SCRIPTURE_MAXREF];
	const char *p, *end;
	size_t i, start, numpos = 0;
	bool seen_alpha = false;
	int book, ch, vs = 0, ve = 0, nch;
	bool has_colon = false;

	if (text == NULL || ref == NULL || len == 0 || len >= sizeof(buf)) {
		return false;
	}
	memcpy(buf, text, len);
	buf[len] = '\0';

	/* trim white space, brackets, quotes and sentence punctuation */
	start = 0;
	while (buf[start] != '\0' && strchr(" \t\r\n([\"'", buf[start]) != NULL) {
		start++;
	}
	i = strlen(buf);
	while (i > start && strchr(" \t\r\n)].,;!?\"'", buf[i - 1]) != NULL) {
		buf[--i] = '\0';
	}
	if (buf[start] == '\0') {
		return false;
	}

	/* the chapter starts at the first digit after the first letter */
	for (i = start; buf[i] != '\0'; i++) {
		if (isalpha((unsigned char)buf[i])) {
			seen_alpha = true;
		} else if (seen_alpha && isdigit((unsigned char)buf[i])) {
			numpos = i;
			break;
		}
	}
	if (numpos == 0) {
		return false;
	}
	if (!normalise_book(buf + start, numpos - start, key, sizeof(key))) {
		return false;
	}
	book = book_lookup(key);
	if (book == 0) {
		return false;
	}

	p = buf + numpos;
	if (!take_number(&p, &ch)) {
		return false;
	}
	if (*p == ':') {
		has_colon = true;
		p++;
		if (!take_number(&p, &vs)) {
			return false;
		}
		ve = vs;
		if (*p == '-') {
			p++;
			if (!take_number(&p, &ve)) {
				return false;
			}
		} else if (strncmp(p, "\xE2\x80\x93", 3) == 0) { /* en dash */
			p += 3;
			if (!take_number(&p, &ve)) {
				return false;
			}
		}
	}
	end = p;
	while (*end == ' ') {
		end++;
	}
	if (*end != '\0') {
		return false;
	}

	if (!kjv_load()) {
		return false;
	}
	nch = kjv_chapters(book);
	if (!has_colon && nch == 1) {
		/* one-chapter books: "Jude 3" means verse 3 */
		vs = ve = ch;
		ch = 1;
	}
	if (ch < 1 || ch > nch) {
		return false;
	}
	if (has_colon || vs != 0) {
		int max = kjv_verses(book, ch);
		if (vs < 1 || vs > max || ve < vs) {
			return false;
		}
		if (ve > max) {
			ve = max;
		}
	}
	ref->book = book;
	ref->chapter = ch;
	ref->vstart = vs;
	ref->vend = ve;
	return true;
}

bool scripture_find(const char *text, size_t len, scripture_ref *ref)
{
	size_t k;

	if (text == NULL || ref == NULL) {
		return false;
	}
	for (k = 0; k < len; k++) {
		/* locate "digits:digits[-digits]" and try to attach a book name */
		size_t d0, d1, rend;
		int tokens;

		if (!isdigit((unsigned char)text[k]) ||
		    (k > 0 && isdigit((unsigned char)text[k - 1]))) {
			continue;
		}
		d0 = k;
		d1 = k;
		while (d1 < len && isdigit((unsigned char)text[d1])) d1++;
		if (d1 >= len || text[d1] != ':' || d1 + 1 >= len ||
		    !isdigit((unsigned char)text[d1 + 1])) {
			continue;
		}
		rend = d1 + 1;
		while (rend < len && isdigit((unsigned char)text[rend])) rend++;
		if (rend + 1 < len && text[rend] == '-' &&
		    isdigit((unsigned char)text[rend + 1])) {
			rend++;
			while (rend < len && isdigit((unsigned char)text[rend])) rend++;
		}
		/* try the longest book phrase (up to 4 words) first */
		for (tokens = 4; tokens >= 1; tokens--) {
			size_t s = d0;
			int t;

			for (t = 0; t < tokens && s > 0; t++) {
				while (s > 0 && text[s - 1] == ' ') s--;
				while (s > 0 && isalnum((unsigned char)text[s - 1])) s--;
			}
			if (s < d0 && d0 - s < 40 &&
			    scripture_parse(text + s, rend - s, ref) &&
			    ref->vstart != 0) {
				return true;
			}
		}
		k = rend;
	}
	return false;
}

/* ----------------------------------------------------------------- output */

const char *scripture_book_name(int book)
{
	return (book >= 1 && book <= 66) ? book_names[book - 1] : NULL;
}

void scripture_label(const scripture_ref *ref, char *buf, size_t buflen)
{
	const char *name = scripture_book_name(ref->book);

	if (buflen == 0) {
		return;
	}
	if (name == NULL) {
		buf[0] = '\0';
	} else if (ref->vstart == 0) {
		snprintf(buf, buflen, "%s %d", name, ref->chapter);
	} else if (ref->vend > ref->vstart) {
		snprintf(buf, buflen, "%s %d:%d-%d", name, ref->chapter,
			 ref->vstart, ref->vend);
	} else {
		snprintf(buf, buflen, "%s %d:%d", name, ref->chapter, ref->vstart);
	}
}

char *scripture_url(const scripture_ref *ref)
{
	char label[96], *url;
	size_t i, o;
	static const char head[] = "https://www.biblegateway.com/passage/?search=";
	static const char tail[] = "&version=NKJV";

	scripture_label(ref, label, sizeof(label));
	url = malloc(sizeof(head) + sizeof(tail) + 3 * strlen(label));
	if (url == NULL) {
		return NULL;
	}
	memcpy(url, head, sizeof(head) - 1);
	o = sizeof(head) - 1;
	for (i = 0; label[i] != '\0'; i++) {
		unsigned char c = (unsigned char)label[i];
		if (isalnum(c) || c == '-') {
			url[o++] = (char)c;
		} else {
			o += (size_t)sprintf(url + o, "%%%02X", c);
		}
	}
	memcpy(url + o, tail, sizeof(tail));
	return url;
}

size_t scripture_text(const scripture_ref *ref, char *buf, size_t buflen)
{
	size_t i, o = 0;
	int v, first, last;
	bool multi, cut = false;

	if (buf == NULL || buflen < 8) {
		return 0;
	}
	buf[0] = '\0';
	if (!kjv_load()) {
		return 0;
	}
	first = ref->vstart != 0 ? ref->vstart : 1;
	last = ref->vstart != 0 ? ref->vend : kjv_verses(ref->book, ref->chapter);
	multi = last > first;

	for (v = first; v <= last && !cut; v++) {
		size_t idx = kjv_lower_bound(KEY(ref->book, ref->chapter, v));
		const char *t, *e;
		size_t tl, room = buflen - 1 - o;
		char num[16] = "";

		if (idx >= kjv.count ||
		    kjv.key[idx] != KEY(ref->book, ref->chapter, v)) {
			continue;
		}
		t = kjv.text + kjv.off[idx];
		e = strchr(t, '\n');
		tl = e != NULL ? (size_t)(e - t) : strlen(t);
		if (multi) {
			snprintf(num, sizeof(num), "%s%d ", o ? " " : "", v);
		}
		if (strlen(num) + tl > room) {
			cut = true;
		}
		for (i = 0; num[i] != '\0' && o < buflen - 1; i++) {
			buf[o++] = num[i];
		}
		for (i = 0; i < tl && o < buflen - 1; i++) {
			buf[o++] = t[i];
		}
	}
	buf[o] = '\0';
	if (cut && o >= 4) {
		memcpy(buf + o - 3, "...", 4);
	}
	return o;
}

/** percent-decode (and '+' to space) src into dst; NUL terminated */
static size_t url_decode(const char *src, char *dst, size_t dstlen)
{
	size_t o = 0;

	while (*src != '\0' && o + 1 < dstlen) {
		if (src[0] == '%' && isxdigit((unsigned char)src[1]) &&
		    isxdigit((unsigned char)src[2])) {
			char h[3] = { src[1], src[2], '\0' };
			dst[o++] = (char)strtol(h, NULL, 16);
			src += 3;
		} else {
			dst[o++] = (*src == '+') ? ' ' : *src;
			src++;
		}
	}
	dst[o] = '\0';
	return o;
}

bool scripture_preview(const char *link_text, const char *url,
		char *out, size_t outlen)
{
	scripture_ref ref;
	char label[96], verse[400], dec[512];
	bool found = false;

	if (out == NULL || outlen == 0) {
		return false;
	}
	out[0] = '\0';

	if (link_text != NULL && scripture_parse(link_text, strlen(link_text), &ref)) {
		found = true;
	} else if (url != NULL) {
		const char *q = strstr(url, "://");
		size_t n;

		/* only the path and query: the host name is never a reference */
		q = (q != NULL) ? strchr(q + 3, '/') : url;
		if (q != NULL) {
			n = url_decode(q, dec, sizeof(dec));
			found = scripture_find(dec, n, &ref);
		}
	}
	if (!found || scripture_text(&ref, verse, sizeof(verse)) == 0) {
		return false;
	}
	scripture_label(&ref, label, sizeof(label));
	snprintf(out, outlen, "%s (KJV): %s", label, verse);
	return true;
}
