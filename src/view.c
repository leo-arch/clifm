/* view.c -- home of the 'view' command */

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

#ifndef _NO_LIRA

#include "helpers.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

#ifdef __OpenBSD__
typedef char *rl_cpvfunc_t;
# include <ereadline/readline/readline.h>
#else
# include <readline/readline.h>
#endif /* __OpenBSD__ */

#include "aux.h" // open_f functions, is_cmd_in_path, construct_human_size
#include "file_operations.h" // open_file
#include "init.h" // get_sel_files
#include "listing.h" // reload_dirlist
#include "messages.h" // VIEW_USAGE
#include "misc.h" // xerror, print_reload_msg
#include "selection.h" // save_sel
#include "spawn.h" // launch_execve
#include "tabcomp.h" // tab_complete

static int
preview_edit(char *app)
{
	if (!config_dir) {
		xerror("view: Configuration directory not found\n");
		return FUNC_FAILURE;
	}

	const size_t len = config_dir_len + 15;
	char *file = xnmalloc(len, sizeof(char));
	snprintf(file, len, "%s/preview.clifm", config_dir);

	int ret = FUNC_SUCCESS;
	if (app) {
		char *cmd[] = {app, file, NULL};
		ret = launch_execv(cmd, FOREGROUND, E_NOFLAG);
	} else {
		ret = open_file(file);
	}

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
		if (lstat(buf, &a) == -1 || !S_ISREG(a.st_mode))
			continue;

		if (a.st_size == 0 && unlink(buf) != -1)
			removed++;
	}

	closedir(dir);

	return removed;
}

/* Remove dangling thumbnails from the thumbnails directory by checking the
 * $XDG_CACHE_HOME/clifm/thumbnails/thumbnails.info file.
 *
 * The info file is created by the 'clifmimg' script: every time a new
 * thumbnail is generated, a new entry is added to this file.
 * Each entry has this form: THUMB@ORIG
 * THUMB is the name of the thumbnail file (i.e. an MD5 hash of the original
 * file followed by a file extension, either jpg or png).
 * ORIG is the absolute path to the original file name.
 *
 * If THUMB does not exist, the entry is removed from the info file.
 * If both THUMB and ORIG exist, the entry is preserved.
 * Finally, if ORIG does not exist, the current entry is removed and THUMB
 * gets deleted. */
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
		unlink(tmp_file);
		return FUNC_FAILURE;
	}

	int fd = 0;
	FILE *fp = open_fread(thumb_file, &fd);
	if (!fp) {
		xerror(_("view: Cannot open '%s': %s\n"), thumb_file, strerror(errno));
		fclose(tmp_fp);
		unlink(tmp_file);
		return FUNC_FAILURE;
	}

	off_t size_sum = 0;
	int errors = 0;
	char tfile[PATH_MAX + 40]; /* Bigger than line is enough. This just avoids
	a compiler warning. */

	/* MD5 hash (32 bytes) + '.' + extension (usually 3 bytes) + '@'
	 * + absolute path + new line char + NUL char */
	char line[PATH_MAX + 39];
	while (fgets(line, (int)sizeof(line), fp) != NULL) {
		char *p = strchr(line, '@');
		if (!p || p[1] != '/') /* Malformed entry: remove it. */
			continue;

		*p = '\0';
		p++;
		const size_t len = strlen(p);
		if (len > 1 && p[len - 1] == '\n')
			p[len - 1] = '\0';

		snprintf(tfile, sizeof(tfile), "%s/%s", thumbnails_dir, line);
		struct stat b;
		if (lstat(tfile, &b) == -1)
			/* Thumbnail file does not exist: remove this entry */
			continue;

		if (lstat(p, &a) != -1) {
			/* Both the thumbnail file and the original file exist. */
			fprintf(tmp_fp, "%s@%s\n", line, p);
			continue;
		}

		/* The thumbnail file exist, but the original file does not:
		 * remove this entry and the corresponding thumbnail file. */

		printf(_("view: '%s': Removing dangling thumbnail... "), line);
		if (unlink(tfile) == -1) {
			printf("%s\n", strerror(errno));
			errors++;
		} else {
			puts(_("OK"));
			rem_files++;
			size_sum += conf.apparent_size == 1
				? b.st_size : b.st_blocks * S_BLKSIZE;
		}
	}

	fclose(fp);
	fclose(tmp_fp);

	rename(tmp_file, thumb_file);
	unlink(tmp_file);

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
