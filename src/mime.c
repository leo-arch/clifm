/* mime.c -- functions controlling Lira, the resource opener */

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

#ifndef _NO_LIRA

#include "helpers.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <readline/tilde.h>

#ifndef _NO_MAGIC
# include <magic.h>
#endif /* !_NO_MAGIC */

#if defined(_NO_MAGIC) && !defined(_BE_POSIX)
# include <paths.h>
# ifndef _PATH_DEVNULL
#  define _PATH_DEVNULL "/dev/null"
# endif /* _PATH_DEVNULL */
#else
# define _PATH_DEVNULL "/dev/null"
#endif /* _NO_MAGIC && !_BE_POSIX */

#ifndef _NO_ARCHIVING
#include "archives.h"
#endif /* !_NO_ARCHIVING */
#include "aux.h"
#include "checks.h"
#include "config.h"
#include "listing.h"
#include "messages.h"
#include "mime.h"
#include "misc.h"
#include "readline.h"
#include "sanitize.h"
#include "spawn.h"

static char *err_name = (char *)NULL;

/* Expand all environment variables in the string S.
 * Returns the expanded string or NULL on error. */
static char *
expand_env(char *s)
{
	char *p = strchr(s, '$');
	if (!p)
		return (char *)NULL;

	const int buf_size = PATH_MAX;
	p = xnmalloc((size_t)buf_size, sizeof(char));
	char *ret = p;

	while (*s) {
		if (*s != '$') {
			*p = *s;
			p++; s++;
			continue;
		}

		char *env = (char *)NULL;
		char *r = strchr(s, ' ');
		if (r) *r = '\0';
		env = getenv(s + 1);
		if (r) *r = ' ';
		if (!env) {
			free(ret);
			return (char *)NULL;
		}

		const size_t env_len = strlen(env);
		const int rem = buf_size - (int)(p - ret) - 1;
		if (rem >= (int)env_len) {
			memccpy(p, env, 0, (size_t)rem);
			p += env_len;
		} else {
			break;
		}

		if (r) *r = '\0';
		s += strlen(s);
		if (r) *r = ' ';
	}

	*p = '\0';
	return ret;
}

/* Move the pointer in LINE immediately after prefix (X or !X)
 * Returns NULL if there's nothing after prefix, if prefix is "!X" and we
 * are in a graphical environment, or if prefix is "X" and we're not in
 * a graphical environment. Otherwise, it returns the corresponding pointer */
static char *
skip_line_prefix(char *line)
{
	if (!line || !*line)
		return (char *)NULL;

	char *p = line;

	if (!(flags & GUI)) {
		if (*p == 'X' && *(p + 1) == ':')
			return (char *)NULL;
		if (*p == '!' && *(p + 1) == 'X' && *(p + 2) == ':')
			p += 3;
	} else {
		if (*p == '!' && *(p + 1) == 'X')
			return (char *)NULL;
		if (*p == 'X' && *(p + 1) == ':')
			p += 2;
	}

	return p;
}

/* Should we skip the line LINE?
 * Returns 1 if true and 0 otherwise
 * If true, the pointer PATTERN will point to the beginning of the
 * null terminated pattern, and the pointer CMDS will point to the
 * beginnning of the list of opening applications */
static int
skip_line(char *line, char **pattern, char **cmds)
{
	if (*line == '#' || *line == '[' || *line == '\n')
		return 1;

	*pattern = skip_line_prefix(line);
	if (!*pattern)
		return 1;
	/* PATTERN points now to the beginning of the pattern */

	*cmds = strchr(*pattern, '=');
	if (!*cmds || !*(*cmds + 1))
		return 1;

	/* Truncate line in '=' to get only the name/mimetype pattern */
	*(*cmds) = '\0';
	(*cmds)++;
	/* CMDS points now to the beginning of the list of opening cmds */
	return 0;
}

/* Test PATTERN against either FILENAME or the mime-type MIME.
 * Returns zero in case of a match, and 1 otherwise. */
static int
test_pattern(const char *pattern, const char *filename, const char *mime)
{
	int ret = FUNC_FAILURE;
	regex_t regex;

	if (filename && (*pattern == 'N' || *pattern == 'E')
	&& *(pattern + 1) == ':') {
		if (regcomp(&regex, pattern + 2,
		REG_NOSUB | REG_EXTENDED | REG_ICASE) == 0
		&& regexec(&regex, filename, 0, NULL, 0) == 0)
			ret = FUNC_SUCCESS;
	} else {
		if (regcomp(&regex, pattern, REG_NOSUB | REG_EXTENDED) == 0
		&& regexec(&regex, mime, 0, NULL, 0) == 0) {
			mime_match = 1;
			ret = FUNC_SUCCESS;
		}
	}

	regfree(&regex);
	return ret;
}

/* Return 1 if APP is a valid and existent application (2 if it's located in
 * the home directory), or zero otherwise. */
static int
check_app_existence(char **app, char **arg)
{
	if (*(*app) == 'a' && *(*app + 1) == 'd' && !*(*app + 2))
		/* No need to check: 'ad' is an internal command. */
		return 1;

	/* Expand tilde */
	if (*(*app) == '~' && *(*app + 1) == '/' && *(*app + 2)) {
		size_t len = user.home_len + strlen(*app);
		const size_t cmd_len = len > 0 ? len - 1 : 0;
		len += *arg ? strlen(*arg) + 1 : 0;

		char *tmp_cmd = xnmalloc(len, sizeof(char));
		snprintf(tmp_cmd, len, "%s/%s", user.home, *app + 2);

		if (access(tmp_cmd, X_OK) == -1) {
			free(tmp_cmd);
			return 0;
		}

		if (*arg) /* Append arguments */
			snprintf(tmp_cmd + cmd_len, len, " %s", *arg);

		*app = xnrealloc(*app, len + 2, sizeof(char));
		xstrsncpy(*app, tmp_cmd, len + 2);
		free(tmp_cmd);
		return 2;
	}

	/* Either a command name or an absolute path */
	char *cmd_path = get_cmd_path(*app);
	if (cmd_path) {
		free(cmd_path);
		return 1;
	}

	return 0;
}

/* Return a copy the first cmd found in LINE (NULL terminated) or NULL.
 * LINE is modified to point past the end of the returned cmd name.
 * Example: if LINE is "cmd1;cmd2", "cmd1" is returned and LINE points now
 * to ";cmd2". */
static char *
get_cmd_from_line(char **line)
{
	char *l = *line;
	char tmp[PATH_MAX];
	size_t len = 0;

	/* Get the first field in LINE */
	while (*l != '\0' && *l != ';' && *l != '\n'
	&& *l != '\'' && *l != '"' && len < PATH_MAX - 1) {
		tmp[len] = *l;
		len++;
		l++;
	}

	tmp[len] = '\0';
	*line = l;

	if (len == 0)
		return (char *)NULL;

	return savestring(tmp, len);
}

/* Return the first valid and existent opening application in LINE or NULL */
static char *
retrieve_app(char *line)
{
	while (*line) {
		char *app = get_cmd_from_line(&line);
		if (!app) {
			line++; /* LINE points now to the next app field */
			continue;
		}

		if (strchr(app, '$')) { /* Environment variable */
			char *t = expand_env(app);
			if (t) {
				free(app);
				app = t;
			}
		}

		if (xargs.secure_cmds == 1
		&& sanitize_cmd(app, SNT_MIME) != FUNC_SUCCESS) {
			free(app);
			continue;
		}

		/* If app contains spaces, the command to check is the string
		 * before the first space. */
		char *ret = strchr(app, ' ');
		if (ret)
			*ret = '\0';

		const size_t param_len = (ret && *(ret + 1)) ? strlen(ret + 1) : 0;

		char *params = (char *)NULL;
		if (*app == '~' && param_len > 0)
			params = savestring(ret + 1, param_len);

		int exists = check_app_existence(&app, &params);
		free(params);

		if (exists != 2 && ret) /* App not in HOME */
			*ret = ' ';

		if (exists == 0) {
			free(app);
			continue;
		}

		return app; /* Valid app. Return it */
	}

	return (char *)NULL; /* No app was found */
}

/* Get application associated to a given MIME type or file name.
 * Returns the first matching line in the MIME file or NULL if none is
 * found. */
static char *
get_app(const char *mime, const char *filename)
{
	if (!mime || !mime_file || !*mime_file)
		return (char *)NULL;

	int fd = 0;
	FILE *fp = open_fread(mime_file, &fd);
	if (!fp) {
		xerror("%s: '%s': %s\n", err_name, mime_file, strerror(errno));
		return (char *)NULL;
	}

	size_t line_size = 0;
	char *line = (char *)NULL;
	char *app = (char *)NULL;

	/* Each line has this form: prefix:pattern=cmd;cmd;cmd... */
	while (getline(&line, &line_size, fp) > 0) {
		char *pattern = (char *)NULL;
		char *cmds = (char *)NULL;

		if (skip_line(line, &pattern, &cmds) == 1)
			continue;
		/* PATTERN points now to the beginning of the null terminated pattern,
		 * while CMDS points to the beginning of the list of opening cmds. */

		mime_match = 0;
		/* Global. Are we matching a MIME type? It will be set by test_pattern. */
		if (test_pattern(pattern, filename, mime) == FUNC_FAILURE)
			continue;

		if ((app = retrieve_app(cmds)))
			break;
	}

	free(line);
	fclose(fp);
	return app;
}

#ifndef _NO_MAGIC
/* Get FILE's MIME type using the libmagic library */
char *
xmagic(const char *file, const int query_mime)
{
	if (!file || !*file)
		return (char *)NULL;

	magic_t cookie = magic_open(query_mime ? (MAGIC_MIME_TYPE | MAGIC_ERROR)
		: MAGIC_ERROR);
	if (!cookie)
		return (char *)NULL;

	magic_load(cookie, NULL);

	const char *mime = magic_file(cookie, file);
	if (!mime) {
		magic_close(cookie);
		return (char *)NULL;
	}

	char *str = savestring(mime, strlen(mime));
	magic_close(cookie);
	return str;
}

#else /* _NO_MAGIC */
static char *
get_mime(char *file)
{
	if (!file || !*file) {
		xerror("%s\n", _("Error opening temporary file"));
		return (char *)NULL;
	}

	char *mime_type = (char *)NULL;
	char *rand_ext = gen_rand_str(RAND_SUFFIX_LEN);

	char tmp_file[PATH_MAX + 1];
	snprintf(tmp_file, sizeof(tmp_file), "%s/mime.%s", tmp_dir,
		rand_ext ? rand_ext : "Hu?+6545jk");
	free(rand_ext);

	int fd = 0;
	FILE *fp_out = open_fwrite(tmp_file, &fd);
	if (!fp_out) {
		xerror("%s: fopen: '%s': %s\n", err_name, tmp_file, strerror(errno));
		return (char *)NULL;
	}

	FILE *fp_err = fopen(_PATH_DEVNULL, "w");
	if (!fp_err) {
		xerror("%s: '%s': %s\n", err_name, _PATH_DEVNULL, strerror(errno));
		goto END;
	}

	int stdout_bk = dup(STDOUT_FILENO); /* Store original stdout */
	int stderr_bk = dup(STDERR_FILENO); /* Store original stderr */

	if (stdout_bk == -1 || stderr_bk == -1)
		goto ERROR;

	/* Redirect stdout to the desired file */
	if (dup2(fileno(fp_out), STDOUT_FILENO) == -1)
		goto ERROR;

	/* Redirect stderr to /dev/null */
	if (dup2(fileno(fp_err), STDERR_FILENO) == -1)
		goto ERROR;

	fclose(fp_out);
	fclose(fp_err);

/* --mime-type is only available since file 4.24 (Mar, 2008), while the -i
 * flag (-I in MacOS) is supported since 3.30 (Apr, 2000).
 * NOTE: the -i flag in the POSIX file(1) specification is a completely
 * different thing. */
#ifdef __APPLE__
	char *cmd[] = {"file", "-bI", file, NULL};
#else
	char *cmd[] = {"file", "-bi", file, NULL};
#endif /* __APPLE__ */
	int ret = launch_execv(cmd, FOREGROUND, E_NOFLAG);

	dup2(stdout_bk, STDOUT_FILENO); /* Restore original stdout */
	dup2(stderr_bk, STDERR_FILENO); /* Restore original stderr */
	close(stdout_bk);
	close(stderr_bk);

	if (ret != FUNC_SUCCESS
	|| (fp_out = fopen(tmp_file, "r")) == NULL) {
		unlink(tmp_file);
		return (char *)NULL;
	}

	/* According to the RFC-4288, both type and subtype of a MIME type cannot
	 * be longer than 127 characters each, So adding the separating slash, we
	 * get a max of 255 characters.
	 * See https://datatracker.ietf.org/doc/html/rfc4288#section-4.2 */
	char line[NAME_MAX + 1]; *line = '\0';
	if (fgets(line, (int)sizeof(line), fp_out) == NULL)
		goto END;

	char *s = strrchr(line, ';');
	if (s)
		*s = '\0';

	size_t len = strlen(line);
	if (len > 0 && line[len - 1] == '\n') {
		line[len - 1] = '\0';
		len--;
	}

	mime_type = len > 0 ? savestring(line, len) : (char *)NULL;

END:
	fclose(fp_out);
	unlink(tmp_file);

	return mime_type;

ERROR:
	xerror("%s: %s\n", err_name, strerror(errno));
	fclose(fp_out);
	fclose(fp_err);
	unlink(tmp_file);
	close(stdout_bk);
	close(stderr_bk);
	return (char *)NULL;
}
#endif /* !_NO_MAGIC */

/* Import MIME associations from the system and store them into FILE.
 * Returns the amount of associations found, if any, or -1 in case of error
 * or no association found */
static int
mime_import(char *file)
{
#if defined(__HAIKU__)
	xerror("%s: Importing MIME associations is not supported "
		"on Haiku\n", err_name);
	return (-1);
#elif defined(__APPLE__)
	xerror("%s: Importing MIME associations is not supported "
		"on MacOS\n", err_name);
	return (-1);
#endif /* __HAIKU__ */

	if (!(flags & GUI)) { /* Not in X, exit */
		xerror(_("%s: Nothing was imported. No graphical "
			"environment found\n"), err_name);
		return (-1);
	}

	if (!user.home) {
		xerror(_("%s: Error getting home directory\n"), err_name);
		return (-1);
	}

	/* Open the new mimelist file */
	int fd = 0;
	FILE *mime_fp = open_fwrite(file, &fd);
	if (!mime_fp) {
		xerror("%s: fopen: '%s': %s\n", err_name, file, strerror(errno));
		return (-1);
	}

	/* Create a list of possible paths for the 'mimeapps.list' file as
	 * specified by the Freedesktop specification */
	const size_t home_len = strlen(user.home);
	char *config_path = (char *)NULL, *local_path = (char *)NULL;
	config_path = xnmalloc(home_len + 23, sizeof(char));
	local_path = xnmalloc(home_len + 41, sizeof(char));
	/* xnmalloc will exit in case of error. However, GCC-13's analyzer
	 * complains about both vars not being checked for NULL. So, let's add
	 * the corresponding checks to silence the warning. */
	if (!config_path || !local_path) {
		free(config_path);
		free(local_path);
		fclose(mime_fp);
		return (-1);
	}

	snprintf(config_path, home_len + 23,
		"%s/.config/mimeapps.list", user.home);
	snprintf(local_path, home_len + 41,
		"%s/.local/share/applications/mimeapps.list", user.home);

	const char *const mime_paths[] = {config_path, local_path,
	    "/usr/local/share/applications/mimeapps.list",
	    "/usr/share/applications/mimeapps.list",
	    "/etc/xdg/mimeapps.list",
	    NULL};

	/* Check each mimeapps.list file and store its associations into FILE */
	size_t i;
	int mime_defs = 0;

	for (i = 0; mime_paths[i]; i++) {
		printf("Checking %s ...\n", mime_paths[i]);
		FILE *sys_mime_fp = fopen(mime_paths[i], "r");
		if (!sys_mime_fp)
			continue;

		size_t line_size = 0;
		char *line = (char *)NULL;
		/* Only store associations in the "Default Applications" section */
		int header_found = 0;

		while (getline(&line, &line_size, sys_mime_fp) > 0) {
			if (header_found == 0
			&& (strncmp(line, "[Default Applications]", 22) == 0
			|| strncmp(line, "[Added Associations]", 20) == 0)) {
				header_found = 1;
				continue;
			}

			if (header_found == 1) {
				if (*line == '[')
					break;
				if (*line == '#' || *line == '\n')
					continue;

				const int index = strcntchr(line, '.');
				if (index != -1)
					line[index] = '\0';

				fprintf(mime_fp, "%s\n", line);
				mime_defs++;
			}
		}

		free(line);
		line = (char *)NULL;
		fclose(sys_mime_fp);
	}

	free(config_path);
	free(local_path);

	if (mime_defs == 0)
		xerror(_("%s: Nothing was imported. No MIME association "
			"found\n"), err_name);

	fclose(mime_fp);
	return mime_defs;
}

static int
mime_edit(char **args)
{
	if (xargs.stealth_mode == 1) {
		printf("%s: mime: %s\n", PROGRAM_NAME, STEALTH_DISABLED);
		return FUNC_SUCCESS;
	}

	if (!mime_file || !*mime_file) {
		xerror("%s: The mimelist file name is undefined\n", err_name);
		return FUNC_FAILURE;
	}

	int exit_status = FUNC_SUCCESS;
	struct stat a;
	if (stat(mime_file, &a) == -1) {
		if (create_mime_file(mime_file, 1) != FUNC_SUCCESS) {
			xerror("%s: Cannot access the mimelist file: %s\n",
				err_name, strerror(ENOENT));
			return ENOENT;
		}
		if (stat(mime_file, &a) == -1) {
			xerror("%s: '%s': %s\n", err_name, mime_file, strerror(errno));
			return errno;
		}
	}

	const time_t prev = a.st_mtime;

	if (!args[2]) {
		char *cmd[] = {"mime", mime_file, NULL};
		open_in_foreground = 1;
		if (mime_open(cmd) != 0) {
			fputs(_("Try 'mm edit APPLICATION'\n"), stderr);
			exit_status = FUNC_FAILURE;
		}
		open_in_foreground = 0;

	} else {
		char *cmd[] = {args[2], mime_file, NULL};
		exit_status = launch_execv(cmd, FOREGROUND, E_NOFLAG);
		if (exit_status != FUNC_SUCCESS)
			return exit_status;
	}

	if (stat(mime_file, &a) != -1 && a.st_mtime != prev) {
		reload_dirlist();
		print_reload_msg(_(CONFIG_FILE_UPDATED));
	}

	return exit_status;
}

static char *
get_basename(char *file_path)
{
	char *f = strrchr(file_path, '/');
	if (f && *(++f))
		return f;

	return (char *)NULL;
}

/* Get user input for the 'open with' function.
 * NUM is a pointer to an integer storing the item selected by the user.
 * MAX is the amount of available items. */
static int
get_user_input(const size_t max)
{
	char *input = (char *)NULL;

	while (!input) {
		input = rl_no_hist(_("Choose an application ('q' to quit): "));
		if (!input || !*input) {
			free(input);
			input = (char *)NULL;
			continue;
		}

		if (*input == 'q' && !input[1]) {
			free(input);
			return (-1);
		}

		if (!is_number(input)) {
			free(input);
			input = (char *)NULL;
			continue;
		}

		int num = atoi(input);
		if (num <= 0 || num > (int)max) {
			free(input);
			input = (char *)NULL;
			continue;
		}

		free(input);
		return num;
	}

	return (-1);
}

static void
set_exec_flags(char *str, int *exec_flags)
{
	if (*str == 'E') {
		*exec_flags |= E_NOSTDERR;
		if (*(str + 1) == 'O')
			*exec_flags |= E_NOSTDOUT;
	} else if (*str == 'O') {
		*exec_flags |= E_NOSTDOUT;
		if (*(str + 1) == 'E')
			*exec_flags |= E_NOSTDERR;
	}
}

/* Expand %f placeholder, stderr/stdout flags, and environment variables
 * in the opening application line */
static size_t
expand_app_fields(char ***cmd, size_t *n, char *fpath, int *exec_flags)
{
	size_t f = 0, i;
	char **a = *cmd;
	*exec_flags = E_NOFLAG;

	for (i = 0; a[i]; i++) {
		/* Expand %f pĺaceholder */
		if (*a[i] == '%' && *(a[i] + 1) == 'f') {
			const size_t fpath_len = strlen(fpath);
			a[i] = xnrealloc(a[i], fpath_len + 1, sizeof(char));
			xstrsncpy(a[i], fpath, fpath_len + 1);
			f = 1;
			continue;
		}

		if (*a[i] == '!' && (a[i][1] == 'E' || a[i][1] == 'O')) {
			set_exec_flags(a[i] + 1, exec_flags);
			free(a[i]);
			a[i] = (char *)NULL;
			continue;
		}

		/* Expand environment variable */
		if (*a[i] == '$' && *(a[i] + 1) >= 'A' && *(a[i] + 1) <= 'Z') {
			char *p = expand_env(a[i]);
			if (!p)
				continue;

			const size_t p_len = strlen(p);
			a[i] = xnrealloc(a[i], p_len + 1, sizeof(char *));
			xstrsncpy(a[i], p, p_len + 1);
			free(p);
			continue;
		}

		/* Check if the command needs to be backgrounded */
		if (*a[i] == '&') {
			bg_proc = 1;
			free(a[i]);
			a[i] = (char *)NULL;
		}
	}

	*n = i;
	return f;
}

/* Open the file FPATH via the application APP */
static int
run_mime_app(char **app, char **fpath)
{
	char **cmd = split_str(*app, NO_UPDATE_ARGS);
	if (!cmd) {
		free(*app);
#ifndef __CYGWIN__
		free(*fpath);
#endif /* !__CYGWIN__ */
		return FUNC_FAILURE;
	}

	int exec_flags = 0;
	size_t i = 0;
	const size_t f = expand_app_fields(&cmd, &i, *fpath, &exec_flags);
	size_t n = i;

	/* If no %f placeholder was found, append file name */
	if (f == 0) {
		cmd = xnrealloc(cmd, i + 2, sizeof(char *));
		cmd[i] = savestring(*fpath, strlen(*fpath));
		cmd[i + 1] = (char *)NULL;
		n++;
	}

	const int ret = launch_execv(cmd, (bg_proc && !open_in_foreground)
		? BACKGROUND : FOREGROUND, exec_flags);

	for (i = 0; i < n; i++)
		free(cmd[i]);
	free(cmd);

	free(*app);
#ifndef __CYGWIN__
	free(*fpath);
#endif /* !__CYGWIN */

	return ret;
}

static int
split_and_run(char *app, char *file)
{
	size_t f = 0;
	size_t i;
	char **cmd = split_str(app, NO_UPDATE_ARGS);
	int exec_flags = E_NOFLAG;

	for (i = 0; cmd[i]; i++) {
		if (*cmd[i] == '!' && (cmd[i][1] == 'E' || cmd[i][1] == 'O')) {
			set_exec_flags(cmd[i] + 1, &exec_flags);
			free(cmd[i]);
			cmd[i] = (char *)NULL;
			continue;
		}

		if (*cmd[i] == '&') {
			bg_proc = 1;
			free(cmd[i]);
			cmd[i] = (char *)NULL;
			continue;
		}
		if (*cmd[i] == '%' && cmd[i][1] == 'f') {
			const size_t file_len = strlen(file);
			cmd[i] = xnrealloc(cmd[i], file_len + 1, sizeof(char));
			xstrsncpy(cmd[i], file, file_len + 1);
			f = 1;
		}
		if (*cmd[i] == '$' && cmd[i][1] >= 'A' && cmd[i][1] <= 'Z') {
			char *env = expand_env(cmd[i]);
			if (env) {
				free(cmd[i]);
				cmd[i] = env;
			}
		}
	}

	/* If file to be opened was not specified via %f, append
	 * it to the cmd array. */
	if (f == 0) {
		cmd = xnrealloc(cmd, i + 2, sizeof(char *));
		cmd[i] = savestring(file, strlen(file));
		cmd[i + 1] = (char *)NULL;
	}

	const int ret =
		launch_execv(cmd, bg_proc ? BACKGROUND : FOREGROUND, exec_flags);

	for (i = 0; cmd[i]; i++)
		free(cmd[i]);
	free(cmd);

	return ret;
}

static int
run_cmd(char *app, char *file)
{
#ifndef _NO_ARCHIVING
	if (*app == 'a' && app[1] == 'd' && !app[2]) {
		/* 'ad' is the internal archiver command */
		char *cmd[] = {"ad", file, NULL};
		return archiver(cmd, 'd');
	}
#endif /* !_NO_ARCHIVING */

	char *env = (char *)NULL;
	if (*app == '$' && app[1] >= 'A' && app[1] <= 'Z')
		env = expand_env(app);

	char *cmd[] = {env ? env : app, file, NULL};
	const int ret = launch_execv(cmd, bg_proc ? BACKGROUND : FOREGROUND,
		bg_proc ? E_NOSTDERR : E_NOFLAG);

	free(env);
	return ret;
}

static int
mime_list_open(char **apps, char *file)
{
	if (!apps || !file)
		return FUNC_FAILURE;

	size_t i;
	for (i = 0; apps[i]; i++);
	const int pad = DIGINUM(i + 1);

	for (i = 0; apps[i]; i++)
		printf("%s%*zu%s %s\n", el_c, pad, i + 1, df_c, apps[i]);

	int n = get_user_input(i);
	if (n == -1) {
		if (conf.autols == 1)
			reload_dirlist();
		return FUNC_SUCCESS;
	}

	char *app = apps[n - 1];
	if (!app)
		return FUNC_FAILURE;

	int ret = FUNC_FAILURE;

	if (strchr(app, ' '))
		ret = split_and_run(app, file);
	else /* We have just a command name: no parameter nor placeholder */
		ret = run_cmd(app, file);

	return ret;
}

static int
is_dup_entry(const char *prefix, char **apps, const char *app)
{
	size_t i;

	for (i = prefix ? 1 : 0; apps && apps[i]; i++) {
		if (*app == *apps[i] && strcmp(app, apps[i]) == 0)
			return 1;
	}

	return 0;
}

/* Return the list of opening apps for FILE_NAME, whose MIME type is MIME,
 * reading the file whose file pointer is FP.
 * If PREFIX is not NULL, we're TAB completing.
 * If ONLY_NAMES is 1, we're TAB completing for 'edit' subcommands (in which
 * case we want only command names, not parameters). */
static char **
get_apps_from_file(FILE *fp, char *file_name, const char *mime,
	const char *prefix, const int only_names)
{
	size_t line_size = 0;
	char *line = (char *)NULL;
	char *app = (char *)NULL;
	char **apps = (char **)NULL;
	size_t appsn = prefix != NULL ? 1 : 0;
	const size_t prefix_len = prefix ? strlen(prefix) : 0;

	char *base_name = get_basename(file_name);

	while (getline(&line, &line_size, fp) > 0) {
		if (*line == '#' || *line == '[' || *line == '\n')
			continue;

		char *p = skip_line_prefix(line);
		if (!p)
			continue;

		char *tmp = strchr(p, '=');
		if (!tmp || !*(tmp + 1))
			continue;

		/* Truncate line in '=' to get only the ext/mimetype pattern/string */
		*tmp = '\0';

		if (test_pattern(p, base_name, mime) != 0)
			continue;

		tmp++; /* We don't want the '=' char */

		app = xnrealloc(app, strlen(tmp) + 1, sizeof(char));

		while (*tmp) {
			size_t app_len = 0;
			/* Split the appplications line into substrings, if any */
			while (*tmp != '\0' && *tmp != ';' && *tmp != '\n' && *tmp != '\''
			&& *tmp != '"') {
				app[app_len] = *tmp;
				app_len++;
				tmp++;
			}

			while (*tmp == ' ') /* Remove leading spaces */
				tmp++;

			if (app_len == 0) {
				tmp++;
				continue;
			}

			app[app_len] = '\0';

			if (prefix_len > 0 && strncmp(prefix, app, prefix_len) != 0)
				continue;

			/* Do not list duplicated entries */
			if (is_dup_entry(prefix, apps, app) == 1)
				continue;

			/* Check each application existence */
			char *file_path = (char *)NULL;

			/* Expand environment variables */
			char *appb = (char *)NULL;
			if (strchr(app, '$')) {
				char *t = expand_env(app);
				if (!t)
					continue;
				/* appb: A copy of the original string: let's display
				 * the env var name itself instead of its expanded
				 * value. */
				appb = savestring(app, strlen(app));
				/* app: the expanded value. */
				const size_t tlen = strlen(t);
				app = xnrealloc(app, app_len + tlen + 1, sizeof(char));
				xstrsncpy(app, t, app_len + tlen + 1);
				free(t);
			}

			/* If app contains spaces, the command to check is
			 * the string before the first space. */
			char *ret = strchr(app, ' ');
			if (ret)
				*ret = '\0';

			if (*app == '~') {
				file_path = tilde_expand(app);
				if (file_path && access(file_path, X_OK) != 0) {
					free(file_path);
					file_path = (char *)NULL;
				}
			}

			/* Do not allow APP to be plain "clifm", since
			 * nested executions of clifm are not allowed. */
			else if (*app == PROGRAM_NAME[0] && strcmp(app, PROGRAM_NAME) == 0) {
				;
			} else if (*app == '/') {
				if (access(app, X_OK) == 0) {
					file_path = app;
				}
			} else if (*app == 'a' && app[1] == 'd' && !app[2]) {
				file_path = savestring("ad", 2);
			} else {
				file_path = get_cmd_path(app);
			}

			if (ret && only_names == 0)
				*ret = ' ';

			if (!file_path)
				continue;

			/* If the app exists, store it into the APPS array */
			if (*app != '/') {
				free(file_path);
				file_path = (char *)NULL;
			}
			apps = xnrealloc(apps, appsn + 2, sizeof(char *));

			/* appb is not NULL if we have an environment variable. */
			if (appb) {
				apps[appsn] = savestring(appb, strlen(appb));
				free(appb);
			} else {
				apps[appsn] = savestring(app, strlen(app));
			}

			appsn++;
			apps[appsn] = (char *)NULL;

			tmp++;
		}
	}

	free(line);
	free(app);

	return apps;
}

static char *
construct_filename(char *filename)
{
	char *name = (char *)NULL;

	if (*filename == '~') {
		char *tmp = tilde_expand(filename);
		if (!tmp)
			return (char *)NULL;
		name = tmp;
	} else {
		if (*filename == '\'' || *filename == '"') {
			char *tmp = savestring(filename + 1, strlen(filename + 1));
			name = remove_quotes(tmp);
		}
	}

	if (strchr(name ? name : filename, '\\')) {
		char *deq_file = unescape_str(name ? name : filename, 0);
		if (!deq_file) {
			free(name);
			return (char *)NULL;
		}

		free(name);
		name = realpath(deq_file, NULL);
		free(deq_file);
	}

	if (!name) {
		name = realpath(filename, NULL);
		if (!name)
			return (char *)NULL;
	}

	return name;
}

/* "ow FILENAME <TAB>" or "CMD edit <TAB>"
 * Return available applications, taken from the mimelist file, to open
 * the file FILENAME, where PREFIX is the partially entered word.
 * If ONLY_NAMES is set to 1 (which is the case when completing opening
 * applications for the 'edit' subcommand), only command names are returned
 * (not parameters). */
char **
mime_open_with_tab(char *filename, const char *prefix, const int only_names)
{
	if (!filename || !mime_file)
		return (char **)NULL;

	char *name = construct_filename(filename);
	if (!name)
		return (char **)NULL;

#ifndef _NO_MAGIC
	char *mime = xmagic(name, MIME_TYPE);
#else
	err_name = "lira";
	char *mime = get_mime(name);
#endif /* !_NO_MAGIC */
	if (!mime) {
		free(name);
		return (char **)NULL;
	}

	FILE *fp = fopen(mime_file, "r");
	if (!fp) {
		free(name);
		free(mime);
		return (char **)NULL;
	}

	/* Do not let PREFIX be NULL, so that get_apps_from_file() knows
	 * we're TAB completing. */
	char **apps = get_apps_from_file(fp, name, mime,
		prefix ? prefix : "", only_names);

	fclose(fp);
	free(mime);
	free(name);

	/* The first element in the matches array must contain the
	 * already matched string. */
	if (!apps) {
		apps = xnmalloc(2, sizeof(char *));
		apps[1] = (char *)NULL;
	}

	if (prefix) {
		apps[0] = savestring(prefix, strlen(prefix));
	} else {
		apps[0] = xnmalloc(1, sizeof(char));
		*apps[0] = '\0';
	}

	size_t appsn;
	for (appsn = 0; apps[appsn]; appsn++);

	/* If only one match */
	if (appsn == 2) {
		const size_t src_len = strlen(apps[1]);
		apps[0] = xnrealloc(apps[0], src_len + 1, sizeof(char));
		xstrsncpy(apps[0], apps[1], src_len + 1);
		free(apps[1]);
		apps[1] = (char *)NULL;
	}

	return apps;
}

static int
run_cmd_noargs(char *arg, char *name)
{
	errno = 0;
	char *cmd[] = {arg, name, NULL};
	int ret = FUNC_SUCCESS;

#ifndef _NO_ARCHIVING
	if (*arg == 'a' && arg[1] == 'd' && !arg[2])
		ret = archiver(cmd, 'd');
	else
		ret = launch_execv(cmd, bg_proc ? BACKGROUND : FOREGROUND, E_NOSTDERR);
#else
	ret = launch_execv(cmd, bg_proc ? BACKGROUND : FOREGROUND, E_NOSTDERR);
#endif /* !_NO_ARCHIVING */

	if (ret == FUNC_SUCCESS)
		return FUNC_SUCCESS;

	xerror("%s: %s: %s\n", err_name, arg,
		ret == E_NOTFOUND ? NOTFOUND_MSG
		: (ret == E_NOEXEC ? NOEXEC_MSG
		: strerror(ret)));

	return ret;
}

static void
append_params(char **args, char *name, char ***cmd, int *exec_flags)
{
	size_t i, n = 1, f = 0;
	for (i = 1; args[i]; i++) {
		if (*args[i] == '%' && *(args[i] + 1) == 'f' && !*(args[i] + 2)) {
			f = 1;
			(*cmd)[n] = savestring(name, strlen(name));
			n++;
			continue;
		}

		if (*args[i] == '!' && (args[i][1] == 'E' || args[i][1] == 'O')) {
			set_exec_flags(args[i] + 1, exec_flags);
			continue;
		}

		if (*args[i] == '$' && *(args[i] + 1) >= 'A'
		&& *(args[i] + 1) <= 'Z') {
			char *env = expand_env(args[i]);
			if (env) {
				(*cmd)[n] = savestring(env, strlen(env));
				n++;
			}
			continue;
		}

		if (*args[i] == '&') {
			bg_proc = 1;
		} else {
			(*cmd)[n] = savestring(args[i], strlen(args[i]));
			n++;
		}
	}

	if (f == 0) {
		(*cmd)[n] = savestring(name, strlen(name));
		n++;
	}

	(*cmd)[n] = (char *)NULL;
}

static int
run_cmd_plus_args(char **args, char *name)
{
	if (!args || !args[0])
		return FUNC_FAILURE;

	size_t i;
	for (i = 0; args[i]; i++);

	char **cmd = xnmalloc(i + 2, sizeof(char *));
	cmd[0] = savestring(args[0], strlen(args[0]));

	int exec_flags = E_NOFLAG;
	append_params(args, name, &cmd, &exec_flags);

	const int ret =
		launch_execv(cmd, bg_proc ? BACKGROUND : FOREGROUND, exec_flags);

	for (i = 0; cmd[i]; i++)
		free(cmd[i]);
	free(cmd);

	return ret;
}

static int
join_and_run(char **args, char *name)
{
	/* Application name plus parameters (array): 'ow FILE CMD ARG...' */
	if (args[1])
		return run_cmd_plus_args(args, name);

	/* Just an application name: 'ow FILE CMD' */
	if (!strchr(args[0], ' '))
		return run_cmd_noargs(args[0], name);

	/* Command is a quoted string: 'ow FILE "CMD ARG ARG..."' */
	char *deq_str = unescape_str(args[0], 0);
	char **ss = split_str(deq_str ? deq_str : args[0], NO_UPDATE_ARGS);
	free(deq_str);

	if (!ss)
		return FUNC_FAILURE;

	const int ret = run_cmd_plus_args(ss, name);

	size_t i;
	for (i = 0; ss[i]; i++)
		free(ss[i]);
	free(ss);

	return ret;
}

/* "ow FILE [APP]" command (open-with).
 * Display available opening applications for FILENAME, get user input,
 * and open the file. */
int
mime_open_with(char *filename, char **args)
{
	if (!filename || !mime_file)
		return FUNC_FAILURE;

	err_name = "lira";

	char *name = normalize_path(filename, strlen(filename));
	if (!name)
		return FUNC_FAILURE;

	struct stat a;
	if (lstat(name, &a) == -1) {
		xerror("%s: '%s': %s\n", err_name, filename, strerror(errno));
		free(name);
		return errno;
	}

	/* ow FILE APP [ARGS]
	 * We already have the opening app. Just join the app, option
	 * parameters, and file name, and execute the command. */
	if (args && args[0]) {
		const int ret = join_and_run(args, name);
		free(name);
		return ret;
	}

	/* Find out the appropriate opening application via either mime type
	 * or file name. */
#ifndef _NO_MAGIC
	char *mime = xmagic(name, MIME_TYPE);
#else
	char *mime = get_mime(name);
#endif /* !_NO_MAGIC */
	if (!mime)
		goto FAIL;

	FILE *fp = fopen(mime_file, "r");
	if (!fp)
		goto FAIL;

	char **apps = get_apps_from_file(fp, name, mime, NULL, 0);

	fclose(fp);
	free(mime);

	if (!apps) {
		free(name);
		return FUNC_FAILURE;
	}

	const int ret = mime_list_open(apps, name);

	size_t i;
	for (i = 0; apps[i]; i++)
		free(apps[i]);
	free(apps);
	free(name);

	return ret;

FAIL:
	free(mime);
	free(name);
	return FUNC_FAILURE;
}

/* Open URL using the application associated to text/html MIME-type in
 * the mimelist file. Returns zero on success and >0 on error.
 * For the time being, this function is only executed via --open or --preview. */
int
mime_open_url(char *url)
{
	if (!url || !*url)
		return FUNC_FAILURE;

	err_name = (xargs.open == 1 || xargs.preview == 1) ? PROGRAM_NAME : "lira";

	char *app = get_app("text/html", 0);
	if (!app) /* The error message might not be printed by get_app(). Fix. */
		return FUNC_FAILURE;

	char *p = strchr(app, ' ');
	if (p)
		*p = '\0';

	char *cmd[] = {app, url, NULL};
	const int ret = launch_execv(cmd, FOREGROUND, E_NOFLAG);
	free(app);

	return ret;
}

static int
import_mime(void)
{
	char *suffix = gen_rand_str(RAND_SUFFIX_LEN);
	char new[PATH_MAX + 1];
	snprintf(new, sizeof(new), "%s.%s", mime_file,
		suffix ? suffix : "5i0TM#r3j&");
	free(suffix);

	const int mime_defs = mime_import(new);
	if (mime_defs > 0) {
		printf(_("%d MIME association(s) imported from the system.\n"
			"File stored as '%s'\nAdd these new associations to your mimelist "
			"file running 'mm edit'.\n"), mime_defs, new);
		return FUNC_SUCCESS;
	}

	return FUNC_FAILURE;
}

static int
mime_info(char *arg, char **fpath, char **deq)
{
	if (!arg) {
		fprintf(stderr, "%s\n", _(MIME_USAGE));
		return FUNC_FAILURE;
	}

	if (strchr(arg, '\\')) {
		*deq = unescape_str(arg, 0);
		*fpath = realpath(*deq, NULL);
		free(*deq);
		*deq = (char *)NULL;
	} else {
		*fpath = realpath(arg, NULL);
	}

	if (!*fpath) {
		const int isnum = is_number(arg);
		xerror("%s: '%s': %s\n", err_name, arg, isnum == 1
			? _("No such ELN") : strerror(errno));
		return isnum == 1 ? FUNC_FAILURE : errno;
	}

	if (access(*fpath, R_OK) == -1) {
		xerror("%s: '%s': %s\n", err_name, *fpath, strerror(errno));
		free(*fpath);
		*fpath = (char *)NULL;
		return errno;
	}

	return FUNC_SUCCESS;
}

/* Get the full path of the file to be opened by mime
 * Returns 0 on success and 1 on error */
static int
get_open_file_path(char **args, char **fpath, char **deq)
{
	char *f = (char *)NULL;
	if (*args[1] == 'o' && strcmp(args[1], "open") == 0 && args[2])
		f = args[2];
	else
		f = args[1];

	/* Only dequote the file name if coming from the mime command */
	if (*args[0] == 'm' && strchr(f, '\\')) {
		*deq = unescape_str(f, 0);
		*fpath = realpath(*deq, NULL);
		free(*deq);
		*deq = (char *)NULL;
	}

	if (!*fpath) {
		*fpath = realpath(f, NULL);
		if (!*fpath) {
			xerror("%s: '%s': %s\n", err_name, f, strerror(errno));
			return errno;
		}
	}

	return FUNC_SUCCESS;
}

/* Handle mime when no opening app has been found */
static int
handle_no_app(const int info, char **fpath, char **mime, const char *arg)
{
	if (xargs.preview == 1) {
		/* When running the previewer, MIME_FILE points to the path to
		 * preview.clifm file. */
		xerror(_("shotgun: '%s': No associated application found\n"
			"Fix this in the configuration file:\n%s\n"), arg, mime_file);
		return FUNC_FAILURE;
	}

	if (info) {
		fputs(_("Associated application: None\n"), stderr);
	} else {
#ifndef _NO_ARCHIVING
		/* If an archive/compressed file, run the archiver function */
		if (is_compressed(*fpath, 1) == 0) {
			char *tmp_cmd[] = {"ad", *fpath, NULL};
			const int ret = archiver(tmp_cmd, 'd');

			free(*fpath);
			free(*mime);

			return ret;
		} else {
			xerror(_("%s: '%s': No associated application found\n"),
				err_name, arg);
		}
#else
		xerror(_("%s: '%s': No associated application found\n"),
			err_name, arg);
#endif /* !_NO_ARCHIVING */
	}

	free(*fpath);
	free(*mime);

	return FUNC_FAILURE;
}

static int
print_error_no_mime(char **fpath)
{
	xerror(_("%s: Error getting mime-type\n"), err_name);
	free(*fpath);
	return FUNC_FAILURE;
}

static void
print_info_name_mime(char *filename, char *mime)
{
	printf(_("Name: %s\n"), filename ? filename : _("None"));
	printf(_("MIME type: %s\n"), mime);
}

static int
print_mime_info(char **app, char **fpath, char **mime)
{
	if (*(*app) == 'a' && (*app)[1] == 'd' && !(*app)[2]) {
		printf(_("Associated application: ad [built-in] [%s]\n"),
			mime_match ? "MIME" : "FILENAME");
	} else {
		printf(_("Associated application: %s [%s]\n"), *app,
			mime_match ? "MIME" : "FILENAME");
	}

	free(*fpath);
	free(*mime);
	free(*app);

	return FUNC_SUCCESS;
}

#ifndef _NO_ARCHIVING
static int
run_archiver(char **fpath, char **app)
{
	char *cmd[] = {"ad", *fpath, NULL};
	const int exit_status = archiver(cmd, 'd');

	free(*fpath);
	free(*app);

	return exit_status;
}
#endif /* _NO_ARCHIVING */

#ifdef _NO_MAGIC
/* Check the existence of the 'file' command */
static int
check_file_cmd(void)
{
	char *p = get_cmd_path("file");
	if (!p) {
		xerror("%s: Cannot retrieve MIME type: 'file' command "
			"not found\n", err_name);
		return FUNC_FAILURE;
	}

	free(p);
	return FUNC_SUCCESS;
}
#endif /* _NO_MAGIC */

static int
print_mime_help(void)
{
	puts(_(MIME_USAGE));
	return FUNC_SUCCESS;
}

/* Open a file according to the application associated to its MIME type
 * or extension. It also accepts the 'info' and 'edit' arguments, the
 * former providing MIME info about the corresponding file and the
 * latter opening the MIME list file. */
int
mime_open(char **args)
{
	if (!args[1] || IS_HELP(args[1]))
		return print_mime_help();

	err_name = (xargs.open == 1 || xargs.preview == 1) ? PROGRAM_NAME : "lira";

	if (*args[1] == 'i' && strcmp(args[1], "import") == 0)
		return import_mime();

	if (*args[1] == 'e' && strcmp(args[1], "edit") == 0)
		return mime_edit(args);

	char *file_path = (char *)NULL;
	char *deq_file = (char *)NULL;
	int info = 0, file_index = 0;

	if (*args[1] == 'i' && strcmp(args[1], "info") == 0) {
		const int ret = mime_info(args[2], &file_path, &deq_file);
		if (ret != FUNC_SUCCESS)
			return ret;
		info = 1;
		file_index = 2;
	} else {
		const int ret = get_open_file_path(args, &file_path, &deq_file);
		if (ret != FUNC_SUCCESS)
			return ret;
		file_index = 1;
	}

	if (!file_path) {
		xerror("%s: %s\n", args[file_index], strerror(errno));
		return FUNC_FAILURE;
	}

	/* Get file's MIME type */
#ifndef _NO_MAGIC
	char *mime = xmagic(file_path, MIME_TYPE);
#else
	if (check_file_cmd() == FUNC_FAILURE) {
		free(file_path);
		return FUNC_FAILURE;
	}
	char *mime = get_mime(file_path);
#endif /* !_NO_MAGIC */

	if (!mime)
		return print_error_no_mime(&file_path);

	char *filename = get_basename(file_path);

	if (info == 1)
		print_info_name_mime(filename, mime);

	/* Get default application for MIME or filename */
	char *app = get_app(mime, filename);
	if (!app)
		return handle_no_app(info, &file_path, &mime, args[1]);

	if (info == 1)
		return print_mime_info(&app, &file_path, &mime);

	free(mime);

	/* Construct and execute the command */
#ifndef _NO_ARCHIVING
	if (*app == 'a' && app[1] == 'd' && !app[2])
		return run_archiver(&file_path, &app);
#endif /* !_NO_ARCHIVING */

#ifdef __CYGWIN__
	// Some Windows programs, like Word and Powerpoint (but not Excel!!), do
	// not like absolute paths when the file name contains spaces. So, let's
	// pass the file name as it was passed to this function, without
	// expanding it to an absolute path.
	// This hack must be removed as soon as the real cause is discovered:
	// why Word/Powerpoint fails to open absolute paths when the file name
	// contains spaces?
	free(file_path);
	return run_mime_app(&app, &args[file_index]);
#else
	return run_mime_app(&app, &file_path);
#endif /* __CYGWIN__ */
}
#else
void *_skip_me_lira;
#endif /* !_NO_LIRA */
