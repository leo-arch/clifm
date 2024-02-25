/* dothidden.c */

/* DESCRIPTION: files named in a .hidden file in the current directory
 * are hidden when dotfiles are not shown (ShowHiddenFiles is set to false).
 * This feature is supported by most major GUI file managers, like Dolphin
 * and Nautilus (though haven't seen it in any terminal file manager).
 * Our implementation, however, does support wildcards. */

/*
 * This file is part of CliFM
 *
 * Copyright (C) 2016-2024, L. Abramovich <leo.clifm@outlook.com>
 * All rights reserved.

 * CliFM is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * CliFM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
*/

#include "helpers.h"

#include <stdio.h>  /* fgets, fopen, fclose, fseek */
#include <string.h> /* strcmp, strlen */

#include "aux.h"       /* xnmalloc, open_fread */
#include "checks.h"    /* check_glob_char */
#include "dothidden.h" /* dothidden_t, DOTHIDDEN_FILE */
#include "strings.h"   /* savestring */

/* Read .hidden file in the current directory and return a struct containing
 * the names of the files listed in it, expanding wildacards, if any.
 * Empty lines and lines containing a slash are ignored.
 * The length of each entry is stored in the "len" field of the struct. */
struct dothidden_t *
load_dothidden(void)
{
	struct dothidden_t *h = (struct dothidden_t *)NULL;
	FILE *fp;
	int fd = 0;
	struct stat a;

	if (lstat(DOTHIDDEN_FILE, &a) == -1 || !S_ISREG(a.st_mode)
	|| a.st_size == 0 || !(fp = open_fread(DOTHIDDEN_FILE, &fd)))
		return h;

	char line[NAME_MAX + 1];
	size_t lines = 0;
	while (fgets(line, 2, fp) != NULL)
		lines++;

	if (lines == 0) {
		fclose(fp);
		return h;
	}

	fseek(fp, 0L, SEEK_SET);

	h = xnmalloc(lines + 1, sizeof(struct dothidden_t));

	size_t counter = 0;
	while (fgets(line, sizeof(line), fp) != NULL) {
		if (!*line || *line == '\n' || strchr(line, '/'))
			continue;

		size_t len = strlen(line);
		if (len > 0 && line[len - 1] == '\n') {
			line[len - 1] = '\0';
			len--;
		}

		if (check_glob_char(line, GLOB_ONLY) == 0) {
			h[counter].name = savestring(line, len);
			h[counter].len = len;
			counter++;
			continue;
		}

		/* We have wildcards. Expand it. */
		glob_t gbuf;
		if (glob(line, GLOB_BRACE, NULL, &gbuf) != 0) {
			globfree(&gbuf);
			continue;
		}

		lines += gbuf.gl_pathc;
		h = xnrealloc(h, lines, sizeof(struct dothidden_t));
		size_t i;

		for (i = 0; i < gbuf.gl_pathc; i++) {
			/* Exclude self and parent dirs, just as dot-files */
			if (!gbuf.gl_pathv[i] || !*gbuf.gl_pathv[i]
			|| *gbuf.gl_pathv[i] == '.')
				continue;

			len = strlen(gbuf.gl_pathv[i]);
			h[counter].name = savestring(gbuf.gl_pathv[i], len);
			h[counter].len = len;
			counter++;
		}

		globfree(&gbuf);
	}

	h[counter].name = (char *)NULL;
	h[counter].len = 0;

	fclose(fp);
	return h;
}

/* Return 1 if the file name NAME is contained in the list of dot-hidden
 * files H. Otherwise, return 0. */
int
check_dothidden(const char *restrict name, struct dothidden_t **h)
{
	if (!name || !*name || !*h)
		return 0;

	const size_t namelen = strlen(name);
	size_t i;

	for (i = 0; (*h)[i].name; i++) {
		if (*name == *(*h)[i].name && namelen == (*h)[i].len
		&& strcmp(name, (*h)[i].name) == 0)
			return 1;
	}

	return 0;
}

/* Free the dothidden_t struct H */
void
free_dothidden(struct dothidden_t **h)
{
	if (!*h)
		return;

	size_t i;
	for (i = 0; (*h)[i].name; i++)
		free((*h)[i].name);

	free(*h);
}
