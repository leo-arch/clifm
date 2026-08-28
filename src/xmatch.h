/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* xmatch.h */

#ifndef CLIFM_XMATCH_H
#define CLIFM_XMATCH_H

#include "helpers.h" /* struct fileinfo */

#include <regex.h>

#include <fnmatch.h>
#ifndef FNM_CASEFOLD
# define FNM_CASEFOLD 0
#endif
#ifndef FNM_EXTMATCH
# define FNM_EXTMATCH 0
#endif

#define IS_GLOB_PREFIX(s) ((s)[0] == 'g' && (s)[1] == 'l' \
	&& (s)[2] == ':' && (s)[3])
#define IS_REGEX_PREFIX(s) ((s)[0] == 'r' && (s)[1] == 'e' \
	&& (s)[2] == ':' && (s)[3])

typedef struct {
	struct fileinfo *gl_finfo; /* Info about found matches */
	size_t gl_matches; /* Number of matches found */
	size_t cap; /* Initial buffer size */
	int    free_names; /* Tell xglobfree whether names should be free'd */
} xglob_t;

typedef struct {
	struct fileinfo *re_finfo;
	size_t re_matches;
	size_t cap;
	int    free_names;
} xregex_t;

__BEGIN_DECLS

int xglob(const char *dirpath, const char *pattern, xglob_t *out,
	int gl_flags, const mode_t filetype, const int do_stat,
	const int store_basename);
void xglobfree(xglob_t *g);

int xregex(const char *dirpath, const char *pattern, xregex_t *out,
	int re_flags, const mode_t filetype, const int do_stat,
	const int store_basename);
void xregfree(xregex_t *r);

__END_DECLS

#endif /* CLIFM_XMATCH_H */
