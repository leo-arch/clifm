/* mimetypes.c -- read, parse, and store info from mime.types files */

/*
 * This file is part of Clifm
 *
 * Copyright (C) 2016-2025, L. Abramovich <leo.clifm@outlook.com>
 * All rights reserved.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
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

#include <string.h> /* strdup, strlen, strchr, strtok */

#include "aux.h" /* hashme() */

#define INIT_BUF_SIZE 1024

typedef int QSFUNC(const void *, const void *);

static int
hash_sort(const void *a, const void *b)
{
	struct mime_t *pa = (struct mime_t *)a;
	struct mime_t *pb = (struct mime_t *)b;

	if (pa->ext_hash > pb->ext_hash)
		return 1;
	if (pa->ext_hash < pb->ext_hash)
		return -1;
	return 0;
}

static int
check_mime_conflict(struct mime_t *m)
{
	size_t i, j;
	for (i = 0; m[i].mimetype; i++) {
		for (j = i + 1; m[j].mimetype; j++) {
			if (m[i].ext_hash == m[j].ext_hash)
				return 1;
		}
	}

	return 0;
}

static char *
get_mimetypes_file(void)
{
	char *p = (char *)NULL;

	if (xargs.secure_env != 1 && xargs.secure_env_full != 1) {
		p = getenv("CLIFM_MIMETYPES_FILE");
		if (p && *p)
			return strdup(p);
	}

	if (!user.home || !*user.home)
		return (char *)NULL;

	const size_t len = user.home_len + 13;
	p = xnmalloc(len, sizeof(char));
	snprintf(p, len, "%s/.mime.types", user.home);

	return p;
}

int
load_user_mimetypes(void)
{
	char *mimetypes_file = get_mimetypes_file();
	if (!mimetypes_file)
		return FUNC_FAILURE;

	struct stat a;
	if (lstat(mimetypes_file, &a) == -1 || !S_ISREG(a.st_mode)) {
		free(mimetypes_file);
		return FUNC_FAILURE;
	}

	FILE *fp = fopen(mimetypes_file, "r");
	free(mimetypes_file);

	if (!fp)
		return FUNC_FAILURE;

	size_t buf_size = INIT_BUF_SIZE;
	size_t n = 0;
	char line[PATH_MAX];
	*line = '\0';

	user_mimetypes = xnmalloc(buf_size, sizeof(struct mime_t));

	while (fgets(line, (int)sizeof(line), fp)) {
		if (!*line || *line <= '0')
			continue;

		char *p = strchr(line, '\n');
		if (p)
			*p = '\0';

		const char *mimetype = strtok(line, " \t");
		if (!mimetype)
			continue;

		size_t mime_len = strlen(mimetype);

		char *ext = (char *)NULL;
		while ((ext = strtok(NULL, " \t")) != NULL) {
			if (n == buf_size) {
				buf_size *= 2;
				user_mimetypes = xnrealloc(user_mimetypes,
					buf_size, sizeof(struct mime_t));
			}

			user_mimetypes[n].ext = savestring(ext, strlen(ext));
			user_mimetypes[n].ext_hash = hashme(ext, conf.case_sens_list);
			user_mimetypes[n].mimetype = savestring(mimetype, mime_len);
			n++;
		}
	}

	fclose(fp);

	if (n == 0) {
		free(user_mimetypes);
		user_mimetypes = (struct mime_t *)NULL;
		return FUNC_SUCCESS;
	}

	user_mimetypes[n].mimetype = (char *)NULL;
	user_mimetypes[n].ext = (char *)NULL;
	user_mimetypes[n].ext_hash = 0;

	if (n != buf_size)
		user_mimetypes =
			realloc(user_mimetypes, (n + 1) * sizeof(struct mime_t));

	if (check_mime_conflict(user_mimetypes) != 0) {
		user_mimetypes[0].ext_hash = (size_t)-1;
	} else {
		qsort(user_mimetypes, n, sizeof(struct mime_t), (QSFUNC *)hash_sort);
	}

	return FUNC_SUCCESS;
}
