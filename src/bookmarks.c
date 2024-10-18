/* bookmarks.c -- bookmarking functions */

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

#include <errno.h>
#include <readline/readline.h> /* tilde_expand() */
#include <string.h>
#include <unistd.h>

#include "aux.h" /* xatoi(), normalize_path(), open_fread(), open_fwrite() */
#include "bookmarks.h" /* bookmarks_function() */
#include "checks.h" /* is_number() */
#include "colors.h" /* get_entry_color() */
#include "file_operations.h" /* open_function() */
#include "init.h" /* load_bookmarks() */
#include "messages.h" /* STEALTH_DISABLED */
#include "misc.h" /* err(), xerror() */
#include "listing.h" /* reload_dirlist() */
#include "readline.h" /* rl_no_hist() */
#include "spawn.h" /* launch_execv() */

void
free_bookmarks(void)
{
	if (bm_n == 0)
		return;

	size_t i;
	for (i = 0; i < bm_n; i++) {
		free(bookmarks[i].shortcut);
		free(bookmarks[i].name);
		free(bookmarks[i].path);
	}

	free(bookmarks);
	bookmarks = (struct bookmarks_t *)NULL;
	bm_n = 0;
}

void
reload_bookmarks(void)
{
	free_bookmarks();
	load_bookmarks();
}

static char **
bm_prompt(const int print_header)
{
	char *bm = (char *)NULL;
	if (print_header)
		printf(_("%s%s\nEnter '%c' to edit your bookmarks or '%c' to quit\n"
			"Choose a bookmark (by ELN, shortcut, or name):\n"),
			NC, df_c, 'e', 'q');

	char bm_str[(MAX_COLOR * 2) + 7];
	snprintf(bm_str, sizeof(bm_str), "\001%s\002>\001%s\002 ", mi_c, tx_c);
	while (!bm)
		bm = rl_no_hist(bm_str);

	char **cmd = (char **)NULL;
	/* "e/edit" is the only command that needs to split the input string:
	 * the second field may be an opening application. */
	if (*bm == 'e' && (bm[1] == ' ' || strncmp(bm, "edit ", 5) == 0)) {
		flags |= IN_BOOKMARKS_SCREEN;
		cmd = split_str(bm, NO_UPDATE_ARGS);
		flags &= ~IN_BOOKMARKS_SCREEN;
		free(bm);
	} else {
		char *tmp = strchr(bm, '\\') ? unescape_str(bm, 0) : (char *)NULL;
		cmd = xnmalloc(2, sizeof(char *));
		cmd[0] = tmp ? tmp : bm;
		cmd[1] = (char *)NULL;
		if (tmp)
			free(bm);
	}

	return cmd;
}

static int
edit_bookmarks(char *cmd, const int flag)
{
	struct stat a;
	if (stat(bm_file, &a) == -1) {
		xerror("bookmarks: '%s': %s\n", bm_file, strerror(errno));
		return errno;
	}

	const time_t prev = a.st_mtime;

	const int ret = open_config_file(cmd, bm_file);
	if (ret != FUNC_SUCCESS) {
		if (!cmd)
			xerror("%s\n", _("bookmarks: Cannot open the bookmarks file"));
		return ret;
	}

	if (stat(bm_file, &a) == -1) {
		xerror("bookmarks: '%s': %s\n", bm_file, strerror(errno));
		return errno;
	}

	if (prev != a.st_mtime) {
		reload_bookmarks();

		if (flag == NO_BM_SCREEN) {
			reload_dirlist();
			print_reload_msg(_("File modified. Bookmarks reloaded\n"));
		}
	}

	return FUNC_SUCCESS;
}

static size_t
get_longest_shortcut(void)
{
	if (bm_n == 0 || !bookmarks)
		return 0;

	size_t l = 0;
	int i = (int)bm_n;

	while (--i >= 0) {
		if (!bookmarks[i].shortcut || !*bookmarks[i].shortcut)
			continue;

		const size_t slen = strlen(bookmarks[i].shortcut);
		if (slen > l)
			l = slen;
	}

	return l;
}

/* Print the list of available bookmarks. */
static void
print_bookmarks(void)
{
	HIDE_CURSOR;
	printf(_("%sBookmarks Manager%s\n\n"), BOLD, df_c);

	struct stat attr;
	const int eln_pad = DIGINUM(bm_n);
	const size_t ls = get_longest_shortcut();
	size_t i;

	/* Print bookmarks, taking into account shortcut, name, and path. */
	for (i = 0; i < bm_n; i++) {
		if (!bookmarks[i].path || !*bookmarks[i].path)
			continue;

		const int sc_ok = bookmarks[i].shortcut ? 1 : 0;

		int sc_pad = (int)ls;
		if (sc_ok == 0 && ls > 0)
			sc_pad += 2; // No shortcut. Let's count '[' and ']'
		if (sc_ok == 1)
			sc_pad -= (int)strlen(bookmarks[i].shortcut);
		if (sc_pad < 0)
			sc_pad = 0;

		char *color = stat(bookmarks[i].path, &attr) == -1
			? uf_c : get_entry_color(bookmarks[i].path, &attr);

		printf("%s%s%*zu%s%s%s%s%s%s%s%-*s %s%s%s\n",
			NC, el_c, eln_pad, i + 1, df_c, // ELN
			ls > 0 ? " " : "",

			sc_ok == 1 ? "[" : "", sc_ok == 1 ? mi_c : bk_c, // Shortcut
			sc_ok == 1 ? bookmarks[i].shortcut : "", df_c,
			sc_ok == 1 ? "]" : "", sc_pad, "",

			color, bookmarks[i].name ? bookmarks[i].name
			: bookmarks[i].path, df_c);
	}

	UNHIDE_CURSOR;
}

static int
edit_bookmarks_func(char **arg)
{
	edit_bookmarks(arg[1], BM_SCREEN);

	size_t i;
	for (i = 0; arg[i]; i++)
		free(arg[i]);
	free(arg);

	char *tmp_cmd[] = {"bm", NULL};
	bookmarks_function(tmp_cmd);
	return FUNC_SUCCESS;
}

/* Return a pointer to the bookmark path (in the bookmarks array)
 * corresponding to ARG (either an ELN or a string). Otherwise return NULL.
 * The return value of this function must not be free'd. */
static char *
get_bm_path(char *arg)
{
	/* If an ELN */
	if (is_number(arg)) {
		const int num = atoi(arg);
		if (num <= 0 || (size_t)num > bm_n) {
			xerror(_("%s: No such ELN\n"), arg);
			return (char *)NULL;
		}
		return bookmarks[num - 1].path;
	}

	char *name = remove_quotes(arg);

	/* If string, check shortcuts and names */
	size_t i;
	for (i = 0; i < bm_n; i++) {
		if ((bookmarks[i].shortcut && *name == *bookmarks[i].shortcut
		&& strcmp(name, bookmarks[i].shortcut) == 0)
		|| (bookmarks[i].name && *name == *bookmarks[i].name
		&& strcmp(name, bookmarks[i].name) == 0)) {
			if (bookmarks[i].path)
				return bookmarks[i].path;

			xerror(_("'%s': Invalid bookmark\n"), name);
			return (char *)NULL;
		}
	}

	xerror(_("'%s': No such bookmark\n"), name);
	return (char *)NULL;
}

static void
free_bm_input(char ***p)
{
	size_t i;

	for (i = 0; (*p)[i]; i++)
		free((*p)[i]);
	free(*p);
	*p = (char **)NULL;
}

/* This function takes care of the bookmarks screen.
 * It prints available bookmarks, gets user input, and opens the appropriate
 * bookmark. */
int
open_bookmark(void)
{
	if (!bookmarks || bm_n == 0) {
		puts(_(NO_BOOKMARKS));
		return FUNC_SUCCESS;
	}

	if (conf.clear_screen > 0)
		CLEAR;

	int exit_status = FUNC_SUCCESS;
	int header_printed = 0;
	int is_dir = 0;

	print_bookmarks();

	char **arg = (char **)NULL;
	while (!arg) {
		arg = bm_prompt(header_printed == 1 ? NO_BM_HEADER : PRINT_BM_HEADER);
		header_printed = 1;
		if (!arg)
			continue;

		if (*arg[0] == 'e' && (!arg[0][1] || strcmp(arg[0], "edit") == 0))
			return edit_bookmarks_func(arg);

		if (*arg[0] == 'q' && (!arg[0][1] || strcmp(arg[0], "quit") == 0))
			break;

		char *tmp_path = get_bm_path(arg[0]);
		if (!tmp_path) {
			free_bm_input(&arg);
			continue;
		}

		struct stat a; /* If not a dir, refresh files list */
		if (stat(tmp_path, &a) != -1 && S_ISDIR(a.st_mode))
			is_dir = 1;

		char *tmp_cmd[] = {"o", tmp_path, arg[1], NULL};
		exit_status = open_function(tmp_cmd);

		if (exit_status != FUNC_SUCCESS) {
			free_bm_input(&arg);
			continue;
		}

		break;
	}

	free_bm_input(&arg);

	if (conf.autols == 1 && is_dir == 0)
		reload_dirlist();

	return FUNC_SUCCESS;
}

/* Open a bookmark by either shortcut or name. */
static int
bm_open(char **cmd)
{
	int exit_status = FUNC_FAILURE;
	char *p = unescape_str(cmd[1], 0);
	if (!p)
		p = cmd[1];

	size_t i;
	for (i = 0; i < bm_n; i++) {
		if (!(bookmarks[i].shortcut && *p == *bookmarks[i].shortcut
		&& strcmp(p, bookmarks[i].shortcut) == 0)

		&& !(bookmarks[i].name && *p == *bookmarks[i].name
		&& strcmp(p, bookmarks[i].name) == 0))
			continue;

		if (!bookmarks[i].path) {
			xerror(_("'%s': Invalid bookmark\n"), p);
			goto END;
		}

		char *tmp_cmd[] = {"o", bookmarks[i].path, cmd[2], NULL};
		exit_status = open_function(tmp_cmd);
		goto END;
	}

	xerror(_("'%s': No such bookmark\n"), p);

END:
	if (p != cmd[1])
		free(p);

	return exit_status;
}

/* Check the file FILE against the list of bookmarked paths.
 * Returns 1 if found, or 0 otherwise. */
static int
check_bm_path(char *file)
{
	if (bm_n == 0 || !bookmarks)
		return 0;

	char *p = normalize_path(file, strlen(file));
	char *new_path = p ? p : file;

	int i = (int)bm_n;
	while (--i >= 0) {
		if (!bookmarks[i].path || strcmp(new_path, bookmarks[i].path) != 0)
			continue;

		xerror(_("bookmarks: '%s': Already bookmarked as '%s'\n"),
			new_path, bookmarks[i].name ? bookmarks[i].name
			: (bookmarks[i].shortcut ? bookmarks[i].shortcut : _("unnamed")) );
		free(p);
		return 1;
	}

	free(p);
	return 0;
}

static int
name_is_reserved_keyword(const char *name)
{
	if (!name || !*name)
		return 0;

	if ( ((*name == 'e' || *name == 'd' || *name == 'a' || *name == 'q')
	&& !*(name + 1)) || strcmp(name, "quit") == 0
	|| strcmp(name, "edit") == 0 || strcmp(name, "del") == 0
	|| strcmp(name, "add") == 0 || strcmp(name, "reload") == 0) {
		xerror(_("bookmarks: '%s': Reserved bookmark keyword\n"), name);
		return 1;
	}

	if (strchr(name, ':') || strchr(name, '[') || strchr(name, ']')) {
		xerror(_("bookmarks: '%s': Colons and square brackets "
			"are not allowed\n"), name);
		return 1;
	}

	return 0;
}

/* Check the name NAME against the list of used bookmark names.
 * Rerturns the index of the used bookmark (in the bookmarks array) if found,
 * or -1 otherwise.
 * If ADD is set to 1, NAME will be compared to reserved bookmark keywords
 * as well, and a message will be printed if a matching name is found. */
static int
check_bm_name(const char *name, const int add)
{
	if (bm_n == 0 || !bookmarks)
		return (-1);

	if (add == 1 && name_is_reserved_keyword(name) == 1)
		return 0;

	int i = (int)bm_n;
	while (--i >= 0) {
		if (!bookmarks[i].name || *name != *bookmarks[i].name
		|| strcmp(name, bookmarks[i].name) != 0)
			continue;

		if (add == 1)
			xerror(_("bookmarks: '%s': Name already in use\n"), name);
		return i;
	}

	return (-1);
}

/* Same as check_bm_name(), but for shortcuts. */
static int
check_bm_shortcut(const char *shortcut, const int add)
{
	if (bm_n == 0 || !bookmarks)
		return (-1);

	if (add == 1 && name_is_reserved_keyword(shortcut) == 1)
		return 0;

	int i = (int)bm_n;
	while (--i >= 0) {
		if (!bookmarks[i].shortcut || *shortcut != *bookmarks[i].shortcut
		|| strcmp(shortcut, bookmarks[i].shortcut) != 0)
			continue;

		if (add == 1)
			xerror(_("bookmarks: '%s': Shortcut already in use\n"), shortcut);
		return i;
	}

	return (-1);
}

/* Bookmark the file FILE as NAME and shortcut SHORTCUT.
 * FILE and NAME are guarranteed to be non-NULL.
 * FILE is already dequoted. */
static int
bookmark_add(char *file, char *name, char *shortcut)
{
	if (check_bm_path(file) == 1 && rl_get_y_or_n("Continue? [y/n] ") == 0)
		return FUNC_SUCCESS;

	int exit_status = FUNC_FAILURE;
	char *p = unescape_str(name, 0);
	char *n = p ? p : name;
	char *q = (char *)NULL;
	char *s = (char *)NULL;

	if (check_bm_name(n, 1) != -1)
		goto ERROR;

	if (shortcut) {
		q = unescape_str(shortcut, 0);
		s = q ? q : shortcut;

		if (check_bm_shortcut(s, 1) != -1)
			goto ERROR;
	}

	/* Once we have path, name and (optionally) shortcut, write it to the
	 * bookmarks file. */
	FILE *bm_fp = open_fappend(bm_file);
	if (!bm_fp) {
		exit_status = errno;
		xerror("bookmarks: '%s': %s\n", bm_file, strerror(errno));
		goto ERROR;
	}

	if (fseek(bm_fp, 0L, SEEK_END) == -1) {
		exit_status = errno;
		xerror("bookmarks: '%s': %s\n", bm_file, strerror(errno));
		fclose(bm_fp);
		goto ERROR;
	}

	char *np = normalize_path(file, strlen(file));

	/* Everything is fine: add the new bookmark to the bookmarks file */
	if (s)
		fprintf(bm_fp, "[%s]%s:%s\n", s, n, np ? np : file);
	else
		fprintf(bm_fp, "%s:%s\n", n, np ? np : file);

	fclose(bm_fp);

	puts(_("File succesfully bookmarked"));
	if (s)
		printf("[%s]%s %s%s%s %s\n", s, n, mi_c, SET_MSG_PTR,
			tx_c, np ? np : file);
	else
		printf("%s %s%s%s %s\n", n, mi_c, SET_MSG_PTR, tx_c, np ? np : file);

	free(np);
	free(p);
	free(q);

	reload_bookmarks(); /* Update bookmarks for TAB completion. */

	return FUNC_SUCCESS;

ERROR:
	free(p);
	free(q);
	return exit_status;
}

/* Create a new bookmark using the fields provided by CMD: path, name, and
 * (optionally) shortcut. */
static int
add_bookmark(char **cmd)
{
	if (!cmd || !cmd[0] || !cmd[1]) {
		puts(BM_ADD_NO_PARAM);
		return FUNC_SUCCESS;
	}

	if (bm_n == MAX_BOOKMARKS) {
		xerror("%s\n", _("bookmarks: Cannot add any more bookmarks"));
		return FUNC_FAILURE;
	}

	char *p = unescape_str(cmd[0], 0);
	if (!p) {
		xerror(_("bookmarks: '%s': Error unescaping file name\n"), cmd[0]);
		return FUNC_FAILURE;
	}

	if (access(p, F_OK) != 0) {
		xerror("bookmarks: '%s': %s\n", p, strerror(errno));
		free(p);
		return FUNC_FAILURE;
	}

	int ret = bookmark_add(p, cmd[1], cmd[2]);
	free(p);
	return ret;
}

/* Go through the list of bookmarks and set the first byte of the path of
 * matching bookmarks to NUL to mark it for deletion.
 * Used by del_bookmarks() to remove marked bookmarks. */
static size_t
mark_bookmarks_for_deletion(char **args, int *exit_status)
{
	*exit_status = FUNC_SUCCESS;
	size_t i, counter = 0;

	for (i = 0; args[i]; i++) {
		char *p = unescape_str(args[i], 0);
		char *name = p ? p : args[i];
		int index = -1;

		if ( ((index = check_bm_name(name, 0)) != -1
		|| (index = check_bm_shortcut(name, 0)) != -1)
		&& *bookmarks[index].path) {
			*bookmarks[index].path = '\0';
			printf("'%s'\n", name);
			counter++;
		} else {
			xerror(_("'%s': No such bookmark\n"), name);
			*exit_status = FUNC_FAILURE;
		}

		free(p);
	}

	return counter;
}

/* Extract name and shortcut from the bookmark line LINE and store it into
 * a bookmarks_t struct. */
static struct bookmarks_t
extract_shortcut_and_name(char *line)
{
	struct bookmarks_t bm = {0};

	if (!line || !*line)
		return bm;

	if (*line == '[') {
		bm.shortcut = strbtw(line, '[', ']');
		bm.name = strbtw(line, ']', ':');
	} else {
		char *p = strchr(line, ':');
		if (p) {
			*p = '\0';
			bm.name = savestring(line, strlen(line));
			*p = ':';
		}
	}

	return bm;
}

/* Return 0 if the bookmarks line LINE contains a bookmark marked for deletion,
 * in which case del_bookmarks() will remove the line from the bookmarks file.
 * Otherwise, 1 is returned. */
static int
keep_bm_line(char *line)
{
	char *p = (char *)NULL;
	if (*line == '#' || *line == '\n' || !(p = strchr(line, '/')))
		return 1;

	*p = '\0';

	struct bookmarks_t bm = extract_shortcut_and_name(line);
	int keep = 1;
	size_t i;

	for (i = 0; i < bm_n; i++) {
		if (!bookmarks[i].path || *bookmarks[i].path) /* Not marked for deletion */
			continue;

		if ((bookmarks[i].shortcut && bm.shortcut
		&& strcmp(bookmarks[i].shortcut, bm.shortcut) == 0)
		|| (bookmarks[i].name && bm.name
		&& strcmp(bookmarks[i].name, bm.name) == 0)) {
			keep = 0;
			break;
		}
	}

	*p = '/';

	free(bm.name);
	free(bm.shortcut);

	return keep;
}

/* Delete bookmarks passed via ARGS. */
static int
del_bookmarks(char **args)
{
	if (!bookmarks || bm_n == 0) {
		puts(NO_BOOKMARKS);
		return FUNC_SUCCESS;
	}

	if (!args || !args[0]) {
		xerror("%s\n", BM_DEL_NO_PARAM);
		return FUNC_FAILURE;
	}

	char *rstr = gen_rand_str(RAND_SUFFIX_LEN);
	char tmp_file[PATH_MAX + 2 + RAND_SUFFIX_LEN];
	snprintf(tmp_file, sizeof(tmp_file), "%s.%s", bm_file,
		rstr ? rstr : "gX6&55#0fa");
	free(rstr);

	int fd = 0;
	FILE *fp = open_fread(bm_file, &fd);
	if (!fp) {
		xerror(_("'%s': %s\nbookmarks: Error reading the bookmarks file\n"),
			bm_file, strerror(errno));
		return errno;
	}

	int tmp_fd = 0;
	FILE *tmp_fp = open_fwrite(tmp_file, &tmp_fd);
	if (!tmp_fp) {
		const int saved_errno = errno;
		xerror(_("'%s': %s\nbookmarks: Error creating temporary file\n"),
			tmp_file, strerror(errno));
		fclose(fp);
		return saved_errno;
	}

	int exit_status = FUNC_SUCCESS;
	const size_t n = mark_bookmarks_for_deletion(args, &exit_status);

	if (n == 0) {
		fclose(fp);
		fclose(tmp_fp);
		unlink(tmp_file);
		return exit_status;
	}

	size_t removed = 0;
	char *line = (char *)NULL;
	size_t line_size = 0;

	while (getline(&line, &line_size, fp) > 0) {
		if (keep_bm_line(line) == 1)
			fprintf(tmp_fp, "%s", line);
		else
			removed++;
	}

	free(line);

	fclose(fp);
	fclose(tmp_fp);

	if (removed > 0) {
		if (rename(tmp_file, bm_file) == 0) {
			print_reload_msg(_("Removed %zu bookmark(s)\n"), removed);
		} else {
			xerror(_("bookmarks: Error updating bookmarks\nCannot rename "
				"temporary file '%s': %s\n"), tmp_file, strerror(errno));
			return errno;
		}
	} else {
		unlink(tmp_file);
	}

	reload_bookmarks();
	return exit_status;
}

/* Handle bookmarks: run the corresponding function according to CMD. */
int
bookmarks_function(char **cmd)
{
	if (xargs.stealth_mode == 1) {
		printf("%s: bookamrks: %s\n", PROGRAM_NAME, STEALTH_DISABLED);
		return FUNC_SUCCESS;
	}

	if (config_ok == 0) {
		xerror(_("%s: Bookmarks function disabled\n"), PROGRAM_NAME);
		return FUNC_FAILURE;
	}

	if (!cmd[1]) /* No arguments: load the bookmarks screen. */
		return open_bookmark();

	if (*cmd[1] == 'a' && (!cmd[1][1] || strcmp(cmd[1], "add") == 0))
		return add_bookmark(cmd + 2);

	if (*cmd[1] == 'd' && (!cmd[1][1] || strcmp(cmd[1], "del") == 0))
		return del_bookmarks(cmd + 2);

	if (*cmd[1] == 'e' && (!cmd[1][1] || strcmp(cmd[1], "edit") == 0))
		return edit_bookmarks(cmd[2], NO_BM_SCREEN);

	if (*cmd[1] == 'r' && (!cmd[1][1] || strcmp(cmd[1], "reload") == 0)) {
		reload_bookmarks();
		return FUNC_SUCCESS;
	}

	/* Shortcut or bm name: open it. */
	return bm_open(cmd);
}
