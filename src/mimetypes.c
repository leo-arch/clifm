/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* mimetypes.c -- read, parse, and store info from mime.types files */

#include "helpers.h"

#include <string.h> /* strdup, strlen, strchr, strtok */

#include "aux.h" /* hashme() */

#define INIT_BUF_SIZE 1024

static void
check_hash_conflicts(void)
{
	for (size_t i = 0; user_mimetypes[i].mimetype; i++)
		for (size_t j = i + 1; user_mimetypes[j].mimetype; j++)
			if (user_mimetypes[i].ext_hash == user_mimetypes[j].ext_hash)
				*user_mimetypes[i].ext = '\0';
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
	int fd = 0;
	FILE *fp = open_fread(mimetypes_file, &fd);
	if (!fp || fstatat(fd, mimetypes_file, &a, 0) == -1 || !S_ISREG(a.st_mode)) {
		if (fp)
			fclose(fp);
		free(mimetypes_file);
		return FUNC_FAILURE;
	}

	free(mimetypes_file);

	size_t buf_size = INIT_BUF_SIZE;
	size_t n = 0;
	/* The longest line in /etc/mime.types is 100 bytes. */
	char line[PATH_MAX + 1];
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

		const size_t mime_len = strlen(mimetype);

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

	check_hash_conflicts();

	if (n != buf_size) {
		user_mimetypes =
			realloc(user_mimetypes, (n + 1) * sizeof(struct mime_t));
	}

	return FUNC_SUCCESS;
}
