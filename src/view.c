/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* view.c -- home of the 'view' command */

#ifndef _NO_LIRA

#include "helpers.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

#ifdef __OpenBSD__
typedef char *rl_cpvfunc_t;
typedef void rl_macro_print_func_t (const char *, const char *, int, const char *);
# include <ereadline/readline/readline.h>
#else
# include <readline/readline.h>
#endif /* __OpenBSD__ */

#if defined(MAC_OS_X_RENAMEAT_SYS_STDIO_H)
# include <sys/stdio.h> /* renameat(2) */
#endif /* MAC_OS_X_RENAMEAT_SYS_STDIO_H */

#include "aux.h" /* open_f functions, is_cmd_in_path, construct_human_size */
#include "file_operations.h" /* open_file */
#include "init.h" /* get_sel_files */
#include "listing.h" /* reload_dirlist */
#include "messages.h" /* VIEW_USAGE */
#include "misc.h" /* xerror, print_reload_msg */
#include "selection.h" /* save_sel */
#include "spawn.h" /* launch_execve */
#include "tabcomp.h" /* tab_complete */

static int
preview_edit(char *app)
{
	if (!config_dir) {
		xerror("view: Configuration directory not found\n");
		return FUNC_FAILURE;
	}

	char *file = (char *)NULL;
	if (alt_preview_file && *alt_preview_file) {
		file = alt_preview_file;
	} else {
		const size_t len = config_dir_len + 15;
		file = xnmalloc(len, sizeof(char));
		snprintf(file, len, "%s/preview.clifm", config_dir);
	}

	int ret = FUNC_SUCCESS;
	if (app) {
		char *cmd[] = {app, file, NULL};
		ret = launch_execv(cmd, FOREGROUND, E_NOFLAG);
	} else {
		ret = open_file(file);
	}

	if (file != alt_preview_file)
		free(file);

	return ret;
}

static size_t
remove_empty_thumbnails(void)
{
	DIR *dir;
	struct dirent *ent;

	if ((dir = opendir(thumbnails_dir)) == NULL)
		return 0;

	struct stat a;
	char buf[PATH_MAX + 1];
	size_t removed = 0;

	while ((ent = readdir(dir)) != NULL) {
		if (SELFORPARENT(ent->d_name))
			continue;

		snprintf(buf, sizeof(buf), "%s/%s", thumbnails_dir, ent->d_name);
		if (lstat(buf, &a) == -1 || !S_ISREG(a.st_mode) || a.st_size > 0)
			continue;

		printf(_("view: '%s': Removing empty thumbnail... "), ent->d_name);

		if (unlinkat(XAT_FDCWD, buf, 0) == -1) {
			printf("%s\n", strerror(errno));
		} else {
			puts("OK");
			removed++;
		}
	}

	closedir(dir);

	return removed;
}

static off_t
remove_dangling_thumb(const char *basename, const char *abs_path,
	const struct stat *attr)
{
	off_t size_sum = 0;

	printf(_("view: '%s': Removing dangling thumbnail... "), basename);
	if (unlinkat(XAT_FDCWD, abs_path, 0) == -1) {
		printf("%s\n", strerror(errno));
		return (off_t)-1;
	} else {
		puts("OK");
		size_sum += conf.apparent_size == 1
			? attr->st_size : attr->st_blocks * S_BLKSIZE;
	}

	return size_sum;
}

static size_t
remove_thumbs_not_in_db(char **thumbs, off_t *size_sum, int *errors)
{
	char **t = thumbs;
	size_t rem = 0;

	if (!t)
		return rem;

	DIR *dir = opendir(thumbnails_dir);
	if (!dir)
		return rem;

	char tmp[PATH_MAX + 1];
	struct dirent *ent;
	size_t i = 0;

	while ((ent = readdir(dir)) != NULL) {
		if (SELFORPARENT(ent->d_name)
		|| strcmp(ent->d_name, "CACHEDIR.TAG") == 0
		|| strcmp(ent->d_name, THUMBNAILS_INFO_FILE) == 0)
			continue;

		int found = 0;
		for (i = 0; t[i]; i++) {
			if (*ent->d_name == *t[i] && strcmp(ent->d_name, t[i]) == 0) {
				found = 1;
				break;
			}
		}

		if (found == 1)
			continue;

		snprintf(tmp, sizeof(tmp), "%s/%s", thumbnails_dir, ent->d_name);

		struct stat a;
		if (lstat(tmp, &a) == -1) {
			xerror("view: '%s': %s\n", ent->d_name, strerror(errno));
			continue;
		}

		const off_t ret = remove_dangling_thumb(ent->d_name, tmp, &a);
		if (ret != (off_t)-1) {
			rem++;
			(*size_sum) += ret;
		} else {
			(*errors)++;
		}
	}

	closedir(dir);

	return rem;
}

/* Remove dangling thumbnails from the thumbnails directory by checking the
 * $XDG_CACHE_HOME/clifm/thumbnails/.thumbs.info file.
 *
 * The info file is created by the 'clifmimg' script: every time a new
 * thumbnail is generated, a new entry is added to this file.
 * Each entry has this form: THUMB_FILE@FILE_URI
 * THUMB_FILE is the name of the thumbnail file (i.e. an MD5 hash of
 * FILE_URI followed by a file extension, either jpg or png).
 * FILE_URI is the file URI for the absolute path to the original filename.
 *
 * If THUMB_FILE does not exist, the entry is removed from the info file.
 * If both THUMB_FILE and FILE_URI exist, the entry is preserved.
 * If FILE_URI does not exist, the current entry is removed and
 * THUMB_FILE gets deleted.
 * Finally, unregistered thumbnail files (not found in the database),
 * get deteled as well. */
static int
purge_thumbnails_cache(void)
{
	if (!thumbnails_dir || !*thumbnails_dir)
		return FUNC_FAILURE;

	struct stat a;
	if (stat(thumbnails_dir, &a) == -1 || !S_ISDIR(a.st_mode)) {
		xerror(_("view: The thumbnails directory does not exist, is not a "
			"directory, or there are no thumbnails\n"));
		return FUNC_FAILURE;
	}

	size_t rem_files = remove_empty_thumbnails();

	char thumb_file[PATH_MAX + 1];
	snprintf(thumb_file, sizeof(thumb_file), "%s/%s",
		thumbnails_dir, THUMBNAILS_INFO_FILE);

	if (lstat(thumb_file, &a) == -1) {
		xerror(_("view: Cannot access '%s': %s\n"), thumb_file, strerror(errno));
		return FUNC_FAILURE;
	}

	if (!S_ISREG(a.st_mode)) {
		xerror(_("view: '%s': Not a regular file\n"), thumb_file);
		return FUNC_FAILURE;
	}

	char tmp_file[PATH_MAX + 1];
	snprintf(tmp_file, sizeof(tmp_file), "%s/%s", thumbnails_dir, TMP_FILENAME);
	int tmp_fd = mkstemp(tmp_file);
	if (tmp_fd == -1) {
		xerror(_("view: Cannot create temporary file '%s': %s\n"),
			tmp_file, strerror(errno));
		return FUNC_FAILURE;
	}

	FILE *tmp_fp = fdopen(tmp_fd, "w");
	if (!tmp_fp) {
		xerror(_("view: Cannot open temporary file '%s': %s\n"),
			tmp_file, strerror(errno));
		unlinkat(XAT_FDCWD, tmp_file, 0);
		close(tmp_fd);
		return FUNC_FAILURE;
	}

	int fd = 0;
	FILE *fp = open_fread(thumb_file, &fd);
	if (!fp) {
		xerror(_("view: Cannot open '%s': %s\n"), thumb_file, strerror(errno));
		unlinkat(XAT_FDCWD, tmp_file, 0);
		close(tmp_fd);
		return FUNC_FAILURE;
	}

	off_t size_sum = 0;
	int errors = 0;
	char tfile[PATH_MAX + 1]; /* Bigger than line is enough. This just avoids
	a compiler warning. */

	/* Let's keep a record of all thumbnail files in the database.
	 * We use this list to found unregistered thumbnails (not in
	 * the database). */
	char buf[PATH_MAX + 1];
	size_t thumbs_in_db_c = 0;
	while (fgets(buf, (int)sizeof(buf), fp) != NULL)
		thumbs_in_db_c++;

	fseek(fp, 0L, SEEK_SET);

	char **thumbs_in_db = xnmalloc(thumbs_in_db_c + 1, sizeof(char *));
	thumbs_in_db_c = 0;

	char *line = (char *)NULL;
	size_t line_size = 0;

	while (getline(&line, &line_size, fp) > 0) {
		char *p = strchr(line, '@');
		if (!p || strncmp(p + 1, "file:///", 8) != 0)
			/* Malformed entry: remove it. */
			continue;

		*p = '\0';
		p += 8;

		const size_t len = strlen(p);
		if (len > 1 && p[len - 1] == '\n')
			p[len - 1] = '\0';

		snprintf(tfile, sizeof(tfile), "%s/%s", thumbnails_dir, line);
		struct stat b;
		if (lstat(tfile, &b) == -1) {
			/* Thumbnail file does not exist: remove this entry */
			printf(_("view: '%s' does not exist. Entry removed.\n"), line);
			rem_files++;
			continue;
		}

		char *abs_path = p;
		if (strchr(p, '%'))
			abs_path = url_decode(p);

		const int retval = lstat(abs_path, &a);

		if (abs_path != p)
			free(abs_path);

		if (retval != -1) {
			/* Both the thumbnail file and the original file exist. */
			thumbs_in_db[thumbs_in_db_c] = savestring(line, strlen(line));
			thumbs_in_db_c++;
			fprintf(tmp_fp, "%s@file://%s\n", line, p);
			continue;
		}

		/* The thumbnail file exist, but the original file does not:
		 * remove this entry and the corresponding thumbnail file. */
		const off_t ret = remove_dangling_thumb(line, tfile, &b);
		if (ret != (off_t)-1) {
			rem_files++;
			size_sum += ret;
		} else {
			errors++;
		}
	}

	renameat(XAT_FDCWD, tmp_file, XAT_FDCWD, thumb_file);

	close(fd);
	close(tmp_fd);
	free(line);

	thumbs_in_db[thumbs_in_db_c] = (char *)NULL;
	rem_files += remove_thumbs_not_in_db(thumbs_in_db, &size_sum, &errors);

	for (size_t i = 0; thumbs_in_db[i]; i++)
		free(thumbs_in_db[i]);
	free(thumbs_in_db);

	if (rem_files > 0) {
		const char *human = construct_human_size(size_sum);
		print_reload_msg(SET_SUCCESS_PTR, xs_cb, _("Removed %zu "
			"thumbnail(s): %s freed\n"),
			rem_files, human ? human : UNKNOWN_STR);
	} else {
		if (errors == 0)
			puts(_("view: No dangling thumbnails"));
	}

	return errors == 0 ? FUNC_SUCCESS : FUNC_FAILURE;
}

int
preview_function(char **args)
{
#ifdef _NO_FZF
	xerror("%s: view: fzf: %s\n", PROGRAM_NAME, _(NOT_AVAILABLE));
	return FUNC_FAILURE;
#endif /* _NO_FZF */

	if (args && args[0]) {
		if (IS_HELP(args[0])) {
			puts(VIEW_USAGE);
			return FUNC_SUCCESS;
		}
		if (*args[0] == 'e' && strcmp(args[0], "edit") == 0)
			return preview_edit(args[1]);
		if (*args[0] == 'p' && strcmp(args[0], "purge") == 0)
			return purge_thumbnails_cache();
	}

	const size_t seln_bk = sel_n;

	const int fzf_preview_bk = conf.fzf_preview;
	enum tab_mode tabmode_bk = tabmode;
	const int fzftab_bk = fzftab;

	if (tabmode != FZF_TAB) {
		if (is_cmd_in_path("fzf") == 0) {
			err(0, NOPRINT_PROMPT, "%s: fzf: %s\n", PROGRAM_NAME, NOTFOUND_MSG);
			return E_NOTFOUND; /* 127, as required by exit(1) */
		}
	}

	conf.fzf_preview = 1;
	tabmode = FZF_TAB;
	fzftab = 1;

	rl_delete_text(0, rl_end);
	rl_point = rl_end = 0;
	rl_redisplay();

	flags |= PREVIEWER;
	tab_complete('?');
	flags &= ~PREVIEWER;

	tabmode = tabmode_bk;
	conf.fzf_preview = fzf_preview_bk;
	fzftab = fzftab_bk;

	if (sel_n > seln_bk) {
		save_sel();
		get_sel_files();
	}

	if (conf.autols == 1) {
		putchar('\n');
		reload_dirlist();
	}
#if defined(RL_READLINE_VERSION) && RL_READLINE_VERSION >= 0x0700
	else { /* Only available since readline 7.0 */
		rl_clear_visible_line();
	}
#endif /* READLINE >= 7.0 */

	if (sel_n > seln_bk) {
		print_reload_msg(NULL, NULL, _("%zu file(s) selected\n"), sel_n - seln_bk);
		print_reload_msg(NULL, NULL, _("%zu total selected file(s)\n"), sel_n);
	}

	return FUNC_SUCCESS;
}
#else
void *_skip_me_view;
#endif /* !_NO_LIRA */
