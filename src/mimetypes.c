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

#define INIT_BUF_SIZE 2048

static void
check_hash_conflicts(void)
{
	for (size_t i = 0; user_mimetypes[i].mimetype; i++) {
		for (size_t j = i + 1; user_mimetypes[j].mimetype; j++) {
			if (user_mimetypes[i].ext_hash == user_mimetypes[j].ext_hash)
				*user_mimetypes[i].ext = '\0';
		}
	}
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

static void
parse_shared_mime_info_db(FILE *fp)
{
	size_t buf_size = INIT_BUF_SIZE;
	size_t n = 0;
	/* The longest line in /etc/mime.types is 100 bytes. */
	char line[PATH_MAX + 1];
	*line = '\0';

	user_mimetypes = xnmalloc(buf_size, sizeof(struct mime_t));

	int header_start = 0;
	size_t mimetype_len = 0;
	char mimetype[NAME_MAX];
	*mimetype = '\0';

	while (fgets(line, (int)sizeof(line), fp)) {
		if (!*line || *line == '\n')
			continue;

		char *p = strchr(line, '\n');
		if (p)
			*p = '\0';

		char *line_ptr = line;
		while (*line_ptr == ' ' || *line_ptr == '\t')
			line_ptr++;

		if (!*line_ptr || *line_ptr != '<')
			continue;

		if (header_start == 0
		&& strncmp(line_ptr, "<mime-type type=\"", 17) == 0) {
			header_start = 1;
			char *val = line_ptr + 17;
			char *end = strchr(val, '"');
			if (end) *end = '\0';
			snprintf(mimetype, sizeof(mimetype), "%s", val);
			mimetype_len = end ? (size_t)(end - val) : strlen(mimetype);
			continue;
		}

		if (strcmp(line_ptr, "</mime-type>") == 0) { // End of block
			header_start = mimetype_len = 0;
			*mimetype = '\0';
			continue;
		}

		if (header_start == 0)
			continue;

		if (*line_ptr != '<' || line_ptr[1] != 'g'
		|| strncmp(line_ptr, "<glob pattern=\"*.", 17) != 0)
			continue;

		char *ext = line_ptr + 17;
		char *end = strchr(ext, '"');
		if (end)
			*end = '\0';

		if (n + 1 >= buf_size) {
			buf_size *= 2;
			user_mimetypes = xnrealloc(user_mimetypes,
				buf_size, sizeof(struct mime_t));
		}

		const size_t ext_len = end ? (size_t)(end - ext) : strlen(ext);

		user_mimetypes[n].ext = savestring(ext, ext_len);
		user_mimetypes[n].ext_hash = hashme(ext, conf.case_sens_list);
		user_mimetypes[n++].mimetype = savestring(mimetype, mimetype_len);
	}

	if (n == 0) {
		free(user_mimetypes);
		user_mimetypes = (struct mime_t *)NULL;
		return;
	}

	user_mimetypes[n].mimetype = (char *)NULL;
	user_mimetypes[n].ext = (char *)NULL;
	user_mimetypes[n].ext_hash = 0;

	if (n != buf_size) {
		user_mimetypes =
			realloc(user_mimetypes, (n + 1) * sizeof(struct mime_t));
	}

	check_hash_conflicts();
}

static void
parse_mime_types_file(FILE *fp)
{
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

		const char *mimetype = strtok(line, " \t:");
		if (!mimetype)
			continue;

		const size_t mime_len = strlen(mimetype);

		char *ext = (char *)NULL;
		while ((ext = strtok(NULL, " \t")) != NULL) {
			if (n + 1 >= buf_size) {
				buf_size *= 2;
				user_mimetypes = xnrealloc(user_mimetypes,
					buf_size, sizeof(struct mime_t));
			}

			if (*ext == '*' && ext[1] == '.' && ext[2])
				ext += 2;

			size_t ext_len = strlen(ext);
			if (ext_len > 1 && ext[ext_len - 1] == ';')
				ext[--ext_len] = '\0';

			user_mimetypes[n].ext = savestring(ext, ext_len);
			user_mimetypes[n].ext_hash = hashme(ext, conf.case_sens_list);
			user_mimetypes[n++].mimetype = savestring(mimetype, mime_len);
		}
	}

	if (n == 0) {
		free(user_mimetypes);
		user_mimetypes = (struct mime_t *)NULL;
		return;
	}

	user_mimetypes[n].mimetype = (char *)NULL;
	user_mimetypes[n].ext = (char *)NULL;
	user_mimetypes[n].ext_hash = 0;

	if (n != buf_size) {
		user_mimetypes =
			realloc(user_mimetypes, (n + 1) * sizeof(struct mime_t));
	}

	check_hash_conflicts();
}

/* Extract extension to MIME-type mappings from the file specified by
 * get_mimetypes_file() and store them in the user_mimetypes global array.
 * Returns FUNC_SUCCESS in case of success or FUNC_FAILURE in case of error.
 *
 * If a duplicated extension is found in the resulting user_mimetypes array,
 * the first one is nullified by setting its first character to the NUL byte
 * (only the last one is preserved).
 *
 * It can parse a MIME Types file format file (e.g., /etc/mime.types or
 * ~/.mime.types), also handling subtly different formats like those
 * found in /etc/nginx/mime.types and /usr/share/mime/globs.
 * Also, it supports Shared MIME-info XML databases, like
 * /usr/share/mime/packages/freedesktop.org.xml */
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

	int c = fgetc(fp);
	const int is_xml = ((char)c == '<');
	ungetc(c, fp);

	/* Store mappings in the user_mimetypes global array. */
	if (is_xml == 1)
		parse_shared_mime_info_db(fp);
	else
		parse_mime_types_file(fp);

	fclose(fp);

	return FUNC_SUCCESS;
}
