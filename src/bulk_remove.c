/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2025 L. Abramovich <leo.clifm@outlook.com>
*/

/* bulk_remove.c -- bulk remove files */

#include "helpers.h"

#include <dirent.h> /* scandir() */
#include <string.h> /* strnlen() */
#include <sys/stat.h> /* (l)stat() */
#include <fcntl.h> /* unlinkat() */
#include <unistd.h> /* unlinkat() */
#include <errno.h>

#include "aux.h" /* xnmalloc, open_fwrite(), is_cmd_in_path(), count_dir() */
#include "file_operations.h" // open_file() */
#include "messages.h" /* RR_USAGE */
#include "misc.h" /* xerror() */
#include "spawn.h" /* launch_execv() */

#define BULK_RM_TMP_FILE_HEADER "# Clifm - Remove files in bulk\n\
# Remove the filenames you want to be deleted, save, and quit the\n\
# editor (you will be asked for confirmation).\n\
# Quit the editor without saving to cancel the operation.\n\n"

#define IS_RR_COMMENT(s) (*(s) == '#' && (s)[1] == ' ')
#define BULK_APP(s)      ((*(s) == ':' && (s)[1]) ? (s) + 1 : (s))

static int
parse_bulk_remove_params(char *s1, char *s2, char **app, char **target)
{
	if (!s1 || !*s1) { /* No parameters */
		/* TARGET defaults to CWD and APP to default associated app */
		*target = workspaces[cur_ws].path;
		return FUNC_SUCCESS;
	}

	int ret = 0;
	struct stat a;
	if ((ret = stat(s1, &a)) == -1 || !S_ISDIR(a.st_mode)) {
		if (is_cmd_in_path(BULK_APP(s1)) == 0) {
			/* S1 is neither a directory nor a valid application */
			int ec = (ret == -1 && *s1 == ':') ? E_NOTFOUND : ENOTDIR;
			if (ec == ENOTDIR)
				xerror("rr: '%s': %s\n", s1, strerror(ec));
			else
				xerror("rr: '%s': %s\n", BULK_APP(s1), NOTFOUND_MSG);
			return ec;
		}

		/* S1 is an application name. TARGET defaults to CWD */
		*target = workspaces[cur_ws].path;
		*app = BULK_APP(s1);
		return FUNC_SUCCESS;
	}

	/* S1 is a valid directory */
	size_t tlen = strlen(s1);
	if (tlen > 2 && s1[tlen - 1] == '/')
		s1[tlen - 1] = '\0';
	*target = s1;

	if (!s2 || !*s2) /* No S2. APP defaults to default associated app */
		return FUNC_SUCCESS;

	if (is_cmd_in_path(BULK_APP(s2)) == 1) {
		/* S2 is a valid application name */
		*app = BULK_APP(s2);
		return FUNC_SUCCESS;
	}

	/* S2 is not a valid application name */
	xerror("rr: '%s': %s\n", BULK_APP(s2), NOTFOUND_MSG);
	return E_NOTFOUND;
}

static int
create_tmp_file(char **file, int *fd, struct stat *attr)
{
	size_t tmp_len = strlen(xargs.stealth_mode == 1 ? P_tmpdir : tmp_dir);
	size_t file_len = tmp_len + (sizeof(TMP_FILENAME) - 1) + 2;

	*file = xnmalloc(file_len, sizeof(char));
	snprintf(*file, file_len, "%s/%s", xargs.stealth_mode == 1
		? P_tmpdir : tmp_dir, TMP_FILENAME);

	*fd = mkstemp(*file);
	if (*fd == -1) {
		xerror("rr: mkstemp: '%s': %s\n", *file, strerror(errno));
		free(*file);
		return FUNC_FAILURE;
	}

	if (fstat(*fd, attr) == -1) {
		xerror("rr: fstat: '%s': %s\n", *file, strerror(errno));
		unlinkat(*fd, *file, 0);
		close(*fd);
		free(*file);
		return FUNC_FAILURE;
	}

	return FUNC_SUCCESS;
}

static char
get_file_suffix(const mode_t type)
{
	switch (type) {
	case DT_DIR: return DIR_CHR;
	case DT_REG: return 0;
	case DT_LNK: return LINK_CHR;
	case DT_SOCK: return SOCK_CHR;
	case DT_FIFO: return FIFO_CHR;
#ifdef SOLARIS_DOORS
	case DT_DOOR: return DOOR_CHR;
//	case DT_PORT: return 0;
#endif /* SOLARIS_DOORS */
#ifdef S_IFWHT
	case DT_WHT: return WHT_CHR;
#endif /* S_IFWHT */
	case DT_UNKNOWN: return UNKNOWN_CHR;
	default: return 0;
	}
}

static void
write_name(FILE *fp, const char *name, const mode_t type)
{
#ifndef _DIRENT_HAVE_D_TYPE
	UNUSED(type);
	char s = 0;
	struct stat a;
	if (lstat(name, &a) != -1)
		s = get_file_suffix(a.st_mode);
#else
	char s = get_file_suffix(type);
#endif /* !_DIRENT_HAVE_D_TYPE */

	if (s)
		fprintf(fp, "%s%c\n", name, s);
	else
		fprintf(fp, "%s\n", name);
}

static int
write_files_to_tmp(struct dirent ***a, filesn_t *n, const char *target,
	char *tmpfile)
{
	int fd = 0;
	FILE *fp = open_fwrite(tmpfile, &fd);
	if (!fp) {
		err('e', PRINT_PROMPT, "rr: fopen: '%s': %s\n",
			tmpfile, strerror(errno));
		return errno;
	}

	fprintf(fp, "%s", _(BULK_RM_TMP_FILE_HEADER));

	if (target == workspaces[cur_ws].path) {
		if (files == 0)
			goto EMPTY_DIR;

		filesn_t i;
		for (i = 0; i < files; i++)
			write_name(fp, file_info[i].name, file_info[i].type);
	} else {
		if (count_dir(target, CPOP) <= 2)
			goto EMPTY_DIR;

		*n = scandir(target, a, NULL, alphasort);
		if (*n == -1) {
			int tmp_err = errno;
			xerror("rr: '%s': %s", target, strerror(errno));
			fclose(fp);
			return tmp_err;
		}

		filesn_t i;
		for (i = 0; i < *n; i++) {
			if (SELFORPARENT((*a)[i]->d_name))
				continue;
#ifndef _DIRENT_HAVE_D_TYPE
			write_name(fp, (*a)[i]->d_name, 0);
#else
			write_name(fp, (*a)[i]->d_name, (*a)[i]->d_type);
#endif /* !_DIRENT_HAVE_D_TYPE */
		}
	}

	fclose(fp);
	return FUNC_SUCCESS;

EMPTY_DIR:
	xerror(_("rr: '%s': Directory empty\n"), target);
	fclose(fp);
	return FUNC_FAILURE;
}

static int
open_tmp_file(struct dirent ***a, const filesn_t n, char *tmpfile, char *app)
{
	int exit_status = FUNC_SUCCESS;
	filesn_t i;

	if (!app || !*app) {
		open_in_foreground = 1;
		exit_status = open_file(tmpfile);
		open_in_foreground = 0;

		if (exit_status == FUNC_SUCCESS)
			return FUNC_SUCCESS;

		xerror(_("rr: '%s': Cannot open file\n"), tmpfile);
		goto END;
	}

	char *cmd[] = {app, tmpfile, NULL};
	exit_status = launch_execv(cmd, FOREGROUND, E_NOFLAG);

	if (exit_status == FUNC_SUCCESS)
		return FUNC_SUCCESS;

END:
	for (i = 0; i < n && *a && (*a)[i]; i++)
		free((*a)[i]);
	free(*a);

	return exit_status;
}

static char **
get_files_from_tmp_file(const char *tmpfile, const char *target, const filesn_t n)
{
	size_t nfiles = (target == workspaces[cur_ws].path)
		? (size_t)files : (size_t)n;
	char **tmp_files = xnmalloc(nfiles + 2, sizeof(char *));

	FILE *fp = fopen(tmpfile, "r");
	if (!fp)
		return (char **)NULL;

	size_t i = 0;
	char line[PATH_MAX + 1]; *line = '\0';

	while (fgets(line, (int)sizeof(line), fp)) {
		if (!*line || IS_RR_COMMENT(line) || *line == '\n')
			continue;

		size_t len = strlen(line);
		if (line[len - 1] == '\n') {
			line[len - 1] = '\0';
			len--;
		}

		if (len > 0 && (line[len - 1] == '/' || line[len - 1] == '@'
		|| line[len - 1] == '=' || line[len - 1] == '|'
		|| line[len - 1] == '?') ) {
			line[len - 1] = '\0';
			len--;
		}

		tmp_files[i] = savestring(line, len);
		i++;
	}

	tmp_files[i] = (char *)NULL;
	fclose(fp);

	return tmp_files;
}

/* If FILE is not found in LIST, returns one; zero otherwise. */
static int
remove_this_file(char *file, char **list)
{
	if (SELFORPARENT(file))
		return 0;

	for (size_t i = 0; list[i]; i++) {
		if (*file == *list[i] && strcmp(file, list[i]) == 0)
			return 0;
	}

	return 1;
}

static char **
get_remove_files(const char *target, char **tmp_files,
	struct dirent ***a, const filesn_t n)
{
	size_t i, j = 1;
	size_t l = (target == workspaces[cur_ws].path) ? (size_t)files : (size_t)n;
	char **rem_files = xnmalloc(l + 3, sizeof(char *));
	rem_files[0] = savestring("rr", 2);

	if (target == workspaces[cur_ws].path) {
		for (i = 0; i < (size_t)files; i++) {
			if (remove_this_file(file_info[i].name, tmp_files) == 1) {
				rem_files[j] = savestring(file_info[i].name,
					strlen(file_info[i].name));
				j++;
			}
		}

		rem_files[j] = (char *)NULL;
		return rem_files;
	}

	for (i = 0; i < (size_t)n; i++) {
		if (remove_this_file((*a)[i]->d_name, tmp_files) == 1) {
			char p[PATH_MAX + NAME_MAX + 3];
			if (*target == '/') {
				snprintf(p, sizeof(p), "%s/%s", target, (*a)[i]->d_name);
			} else {
				snprintf(p, sizeof(p), "%s/%s/%s", workspaces[cur_ws].path,
					target, (*a)[i]->d_name);
			}

			rem_files[j] = savestring(p, strnlen(p, sizeof(p)));
			j++;
		}
		free((*a)[i]);
	}

	free(*a);
	rem_files[j] = (char *)NULL;

	return rem_files;
}

static int
diff_files(char *tmp_file, const filesn_t n)
{
	FILE *fp = fopen(tmp_file, "r");
	if (!fp) {
		xerror("br: '%s': %s\n", tmp_file, strerror(errno));
		return 0;
	}

	char line[PATH_MAX + 6]; *line = '\0';
	filesn_t c = 0;

	while (fgets(line, (int)sizeof(line), fp)) {
		if (*line != '\0' && *line != '#' && *line != '\n')
			c++;
	}

	fclose(fp);

	return (c < n ? 1 : 0);
}

static void
free_dirent(struct dirent ***a, const filesn_t n)
{
	filesn_t i = n;
	while (--i >= 0)
		free((*a)[i]);
	free(*a);
}

static int
nothing_to_do(char **tmp_file, struct dirent ***a,
	const filesn_t n, const int fd)
{
	puts(_("rr: Nothing to do"));

	if (unlink(*tmp_file) == 1)
		xerror("rr: unlink: '%s': %s\n", *tmp_file, strerror(errno));

	close(fd);
	free(*tmp_file);
	free_dirent(a, n);

	return FUNC_SUCCESS;
}

int
bulk_remove(char *s1, char *s2)
{
	if (virtual_dir == 1) {
		xerror(_("%s: rr: Feature not allowed in virtual "
			"directories\n"), PROGRAM_NAME);
		return FUNC_SUCCESS;
	}

	if (s1 && IS_HELP(s1)) {
		puts(_(RR_USAGE));
		return FUNC_SUCCESS;
	}

	char *app = (char *)NULL;
	char *target = (char *)NULL;
	int fd = 0, ret = 0, i = 0;
	filesn_t n = 0;

	char dpath[PATH_MAX + 1]; *dpath = '\0';
	if (s1 && *s1 && *s1 != ':' && strchr(s1, '\\')) {
		char *tmp = unescape_str(s1, 0);
		if (tmp) {
			xstrsncpy(dpath, tmp, sizeof(dpath));
			free(tmp);
		}
	}

	ret = parse_bulk_remove_params(*dpath ? dpath : s1, s2, &app, &target);

	if (ret != FUNC_SUCCESS)
		return ret;

	struct stat attr;
	char *tmp_file = (char *)NULL;
	if ((ret = create_tmp_file(&tmp_file, &fd, &attr)) != FUNC_SUCCESS)
		return ret;

	const time_t old_mtime = attr.st_mtime;
	const ino_t old_ino = attr.st_ino;
	const dev_t old_dev = attr.st_dev;

	struct dirent **a = (struct dirent **)NULL;
	if ((ret = write_files_to_tmp(&a, &n, target, tmp_file)) != FUNC_SUCCESS)
		goto END;

	if ((ret = open_tmp_file(&a, n, tmp_file, app)) != FUNC_SUCCESS)
		goto END;

	/* Make sure the tmp file we're about to read is the same as the one
	 * we originally created. */
	if (lstat(tmp_file, &attr) == -1 || !S_ISREG(attr.st_mode)
	|| attr.st_ino != old_ino || attr.st_dev != old_dev) {
		ret = FUNC_FAILURE;
		xerror("%s\n", _("rr: Temporary file changed on disk! Aborting."));
		free_dirent(&a, n);
		goto END;
	}

	const filesn_t num = (target == workspaces[cur_ws].path) ? files : n - 2;
	if (old_mtime == attr.st_mtime || diff_files(tmp_file, num) == 0)
		return nothing_to_do(&tmp_file, &a, n, fd);

	char **rfiles = get_files_from_tmp_file(tmp_file, target, n);
	if (!rfiles)
		goto END;

	char **rem_files = get_remove_files(target, rfiles, &a, n);
	if (!rem_files)
		goto FREE_N_EXIT;

	ret = remove_files(rem_files);

	for (i = 0; rem_files[i]; i++)
		free(rem_files[i]);
	free(rem_files);

FREE_N_EXIT:
	for (i = 0; rfiles[i]; i++)
		free(rfiles[i]);
	free(rfiles);

END:
	if (unlinkat(fd, tmp_file, 0) == -1) {
		err('w', PRINT_PROMPT, "rr: unlink: '%s': %s\n",
			tmp_file, strerror(errno));
	}

	close(fd);
	free(tmp_file);
	return ret;
}
