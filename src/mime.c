/* mime.c -- functions controlling Lira, the resource opener */

/*
 * This file is part of CliFM
 * 
 * Copyright (C) 2016-2022, L. Abramovich <johndoe.arch@outlook.com>
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
#include <time.h>
#include <sys/stat.h>
#include <readline/readline.h>

#ifndef _NO_MAGIC
#include <magic.h>
#endif

#ifndef _NO_ARCHIVING
#include "archives.h"
#endif

#include "aux.h"
#include "checks.h"
#include "config.h"
#include "exec.h"
#include "mime.h"
#include "messages.h"
#include "navigation.h"
#include "readline.h"
#include "misc.h"
#include "sanitize.h"
#include "listing.h"

/* Expand all environment variables in the string S
 * Returns the expanded string or NULL on error */
static char *
expand_env(char *s)
{
	char *p = strchr(s, '$');
	if (!p) return (char *)NULL;

	int buf_size = PATH_MAX;
	p = (char *)xnmalloc((size_t)buf_size, sizeof(char));
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

		size_t env_len = strlen(env);
		int rem = buf_size - (int)(p - ret) - 1;
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

/* Move the pointer in LINE immediately after X or !X */
static char *
skip_line_prefix(char *line)
{
	char *p = line;

	if (!(flags & GUI)) {
		if (*p != '!' || *(p + 1) != 'X' || *(p + 2) != ':')
			return (char *)NULL;
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

/* Test PATTERN against either FILENAME or the mime-type MIME
 * Returns zero in case of a match, and 1 otherwise */
static int
test_pattern(const char *pattern, const char *filename, const char *mime)
{
	int ret = EXIT_FAILURE;
	regex_t regex;

	if (filename && (*pattern == 'N' || *pattern == 'E')
	&& *(pattern + 1) == ':') {
		if (regcomp(&regex, pattern + 2, REG_NOSUB | REG_EXTENDED) == 0
		&& regexec(&regex, filename, 0, NULL, 0) == 0)
			ret = EXIT_SUCCESS;
	} else {
		if (regcomp(&regex, pattern, REG_NOSUB | REG_EXTENDED) == 0
		&& regexec(&regex, mime, 0, NULL, 0) == 0) {
			mime_match = 1;
			ret = EXIT_SUCCESS;
		}
	}

	regfree(&regex);
	return ret;
}

/* Return 1 if APP is a valid and existent application. Zero otherwise */
static int
check_app_existence(char **app)
{
	if (*(*app) == 'a' && *(*app + 1) == 'd' && !*(*app + 2))
		/* No need to check: 'ad' is an internal command */
		return 1;

	/* Expand tilde */
	if (*(*app) == '~' && *(*app + 1) == '/' && *(*app + 2)) {
		size_t len = strlen(user.home) + strlen(*app);
		char *_path = (char *)xnmalloc(len, sizeof(char));
		sprintf(_path, "%s/%s", user.home, *app + 2);
		if (access(_path, X_OK) == -1) {
			free(_path);
			return 0;
		}

		*app = (char *)xrealloc(*app, (len + 2) * sizeof(char));
		strcpy(*app, _path);
		free(_path);
		return 1;
	}

	/* Either a command name or an absolute path */
	char *_path = get_cmd_path(*app);
	if (_path) {
		free(_path);
		return 1;
	}

	return 0;
}

/* Return the first NULL terminated cmd in LINE or NULL */
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
		if (!app) { line++; continue; }

		if (strchr(app, '$')) { /* Environment variable */
			char *t = expand_env(app);
			if (t) { free(app);	app = t; }
		}

		if (xargs.secure_cmds == 1
		&& sanitize_cmd(app, SNT_MIME) != EXIT_SUCCESS) {
			free(app);
			continue;
		}

		/* If app contains spaces, the command to check is the string
		 * before the first space */
		char *ret = strchr(app, ' ');
		if (ret) *ret = '\0';

		int exists = check_app_existence(&app);

		if (ret) *ret = ' ';
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
 * found */
static char *
get_app(const char *mime, const char *filename)
{
	if (!mime || !mime_file || !*mime_file)
		return (char *)NULL;

	FILE *defs_fp = fopen(mime_file, "r");
	if (!defs_fp) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "mime: %s: %s\n", mime_file, strerror(errno));
		return (char *)NULL;
	}

	size_t line_size = 0;
	char *line = (char *)NULL, *app = (char *)NULL;

	/* Each line has this form: prefix:pattern=cmd;cmd;cmd... */
	while (getline(&line, &line_size, defs_fp) > 0) {
		char *pattern = (char *)NULL, *cmds = (char *)NULL;
		if (skip_line(line, &pattern, &cmds) == 1)
			continue;
		/* PATTERN points now to the beginning of the null terminated pattern,
		 * while CMDS points to the beginning of the list of opening cmds */

		mime_match = 0;
		/* Global. Are we matching a MIME type? It will be set by test_pattern */
		if (test_pattern(pattern, filename, mime) == EXIT_FAILURE)
			continue;

		if ((app = retrieve_app(cmds)))
			break;
	}

	free(line);
	fclose(defs_fp);
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
	if (!cookie) {
//		fprintf(stderr, "%s: xmagic: %s\n", PROGRAM_NAME, strerror(errno));
		return (char *)NULL;
	}

	magic_load(cookie, NULL);

	const char *mime = magic_file(cookie, file);
	if (!mime) {
/*		const char *err_str = magic_error(cookie);
		if (err_str)
			fprintf(stderr, "%s: xmagic: %s\n", PROGRAM_NAME, err_str); */
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
		fputs(_("Error opening temporary file\n"), stderr);
		return (char *)NULL;
	}

	char *rand_ext = gen_rand_str(6);
	if (!rand_ext)
		return (char *)NULL;

	char mime_tmp_file[PATH_MAX];
	snprintf(mime_tmp_file, PATH_MAX, "%s/mime.%s", tmp_dir, rand_ext);
	free(rand_ext);

	if (access(mime_tmp_file, F_OK) == 0)
		unlink(mime_tmp_file);

	FILE *file_fp = fopen(mime_tmp_file, "w");
	if (!file_fp) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "mime: fopen: %s: %s\n",
			mime_tmp_file, strerror(errno));
		return (char *)NULL;
	}

	FILE *file_fp_err = fopen("/dev/null", "w");
	if (!file_fp_err) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "mime: /dev/null: %s\n", strerror(errno));
		fclose(file_fp);
		return (char *)NULL;
	}

	int stdout_bk = dup(STDOUT_FILENO); /* Store original stdout */
	int stderr_bk = dup(STDERR_FILENO); /* Store original stderr */

	/* Redirect stdout to the desired file */
	if (dup2(fileno(file_fp), STDOUT_FILENO) == -1) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "mime: %s\n", strerror(errno));
		fclose(file_fp);
		fclose(file_fp_err);
		return (char *)NULL;
	}

	/* Redirect stderr to /dev/null */
	if (dup2(fileno(file_fp_err), STDERR_FILENO) == -1) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "mime: %s\n", strerror(errno));
		fclose(file_fp);
		fclose(file_fp_err);
		return (char *)NULL;
	}

	fclose(file_fp);
	fclose(file_fp_err);

	char *cmd[] = {"file", "--mime-type", file, NULL};
	int ret = launch_execve(cmd, FOREGROUND, E_NOFLAG);

	dup2(stdout_bk, STDOUT_FILENO); /* Restore original stdout */
	dup2(stderr_bk, STDERR_FILENO); /* Restore original stderr */
	close(stdout_bk);
	close(stderr_bk);

	if (ret != EXIT_SUCCESS)
		return (char *)NULL;

	if (access(mime_tmp_file, F_OK) != 0)
		return (char *)NULL;

	file_fp = fopen(mime_tmp_file, "r");
	if (!file_fp) {
		unlink(mime_tmp_file);
		return (char *)NULL;
	}

	char *mime_type = (char *)NULL;

	char line[255] = "";
	if (fgets(line, (int)sizeof(line), file_fp) == NULL) {
		fclose(file_fp);
		unlink(mime_tmp_file);
		return (char *)NULL;
	}

	char *tmp = strrchr(line, ' ');
	if (tmp) {
		size_t len = strlen(tmp);
		if (tmp[len - 1] == '\n')
			tmp[len - 1] = '\0';
		mime_type = savestring(tmp + 1, strlen(tmp) - 1);
	}

	fclose(file_fp);
	unlink(mime_tmp_file);
	return mime_type;
}
#endif /* !_NO_MAGIC */

/* Import MIME associations from the system and store them into FILE.
 * Returns the amount of associations found, if any, or -1 in case of error
 * or no association found */
static int
mime_import(char *file)
{
#if defined(__HAIKU__)
	fprintf(stderr, "mime: Importing MIME associations is not supported on Haiku\n");
	return (-1);
#elif defined(__APPLE__)
	fprintf(stderr, "mime: Importing MIME associations is not supported on MacOS\n");
	return (-1);
#endif

	if (!(flags & GUI)) { /* Not in X, exit */
		fprintf(stderr, _("mime: Nothing was imported. No graphical environment found\n"));
		return (-1);
	}

	if (!user.home) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, _("mime: Error getting home directory\n"));
		return (-1);
	}

	/* Open the new mimelist file */
	FILE *mime_fp = fopen(file, "w");
	if (!mime_fp) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "mime: fopen: %s: %s\n", file, strerror(errno));
		return (-1);
	}

	/* Create a list of possible paths for the 'mimeapps.list' file as
	 * specified by the Freedesktop specification */
	size_t home_len = strlen(user.home);
	char *config_path = (char *)NULL, *local_path = (char *)NULL;
	config_path = (char *)xnmalloc(home_len + 23, sizeof(char));
	local_path = (char *)xnmalloc(home_len + 41, sizeof(char));
	sprintf(config_path, "%s/.config/mimeapps.list", user.home);
	sprintf(local_path, "%s/.local/share/applications/mimeapps.list", user.home);

	char *mime_paths[] = {config_path, local_path,
	    "/usr/local/share/applications/mimeapps.list",
	    "/usr/share/applications/mimeapps.list",
	    "/etc/xdg/mimeapps.list", NULL};

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
			if (!header_found && (strncmp(line, "[Default Applications]", 22) == 0
			|| strncmp(line, "[Added Associations]", 20) == 0)) {
				header_found = 1;
				continue;
			}

			if (header_found == 1) {
				if (*line == '[')
					break;
				if (*line == '#' || *line == '\n')
					continue;

				int index = strcntchr(line, '.');
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
		fprintf(stderr, _("mime: Nothing was imported. No MIME association found\n"));

	fclose(mime_fp);
	return mime_defs;
}

static int
mime_edit(char **args)
{
	if (xargs.stealth_mode == 1) {
		printf("%s: mime: %s\n", PROGRAM_NAME, STEALTH_DISABLED);
		return EXIT_SUCCESS;
	}

	if (!mime_file || !*mime_file) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "mime: The mimelist file name is undefined\n");
		return EXIT_FAILURE;
	}

	int exit_status = EXIT_SUCCESS;
	struct stat a;
	if (stat(mime_file, &a) == -1) {
		if (create_mime_file(mime_file, 1) != EXIT_SUCCESS) {
			_err(ERR_NO_STORE, NOPRINT_PROMPT, "mime: Cannot access "
				"the mimelist file. %s\n", strerror(ENOENT));
			return ENOENT;
		}
		if (stat(mime_file, &a) == -1) {
			_err(ERR_NO_STORE, NOPRINT_PROMPT, "mime: %s: %s\n", mime_file, strerror(errno));
			return errno;
		}
	}

	time_t prev = a.st_mtime;

	if (!args[2]) {
		char *cmd[] = {"mime", mime_file, NULL};
		open_in_foreground = 1;
		if (mime_open(cmd) != 0) {
			fputs(_("Try 'mm, mime edit APPLICATION'\n"), stderr);
			exit_status = EXIT_FAILURE;
		}
		open_in_foreground = 0;

	} else {
		char *cmd[] = {args[2], mime_file, NULL};
		if (launch_execve(cmd, FOREGROUND, E_NOSTDERR) != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;
	}

	stat(mime_file, &a);
	if (a.st_mtime != prev) {
		reload_dirlist();
		print_reload_msg(_(CONFIG_FILE_UPDATED));
	}

	return exit_status;
}

static char *
get_filename(char *file_path)
{
	char *f = strrchr(file_path, '/');
	if (f && *(++f))
		return f;

/*	if (f)
		return (f + 1); */

	return (char *)NULL;
}

/* Get user input for the 'open with' function */
static char *
get_user_input(int *a, const size_t *nn)
{
	char *input = (char *)NULL;
	while (!input) {
		input = rl_no_hist(_("Choose an application ('q' to quit): "));
		if (!input)
			continue;
		if (!*input) {
			free(input);
			input = (char *)NULL;
			continue;
		}
		if (*input == 'q' && !*(input + 1)) {
			free(input);
			input = (char *)NULL;
			*a = -1;
			break;
		}

		if (!is_number(input)) {
			free(input);
			input = (char *)NULL;
			continue;
		}

		*a = atoi(input);
		if (*a <= 0 || *a > (int)*nn) {
			free(input);
			input = (char *)NULL;
			continue;
		}
	}

	return input;
}

static int
mime_list_open(char **apps, char *file)
{
	if (!apps || !file)
		return EXIT_FAILURE;

	char **n = (char **)NULL;
	size_t nn = 0;

	size_t i, j = 0;
	for (i = 0; apps[i]; i++) {
		int rep = 0;

		/* Do not list duplicated entries */
		for (j = 0; j < i; j++) {
			if (*apps[i] == *apps[j] && strcmp(apps[i], apps[j]) == 0) {
				rep = 1;
				break;
			}
		}

		if (rep == 1)
			continue;

		n = (char **)xrealloc(n, (nn + 1) * sizeof(char *));
		n[nn] = apps[i];
		nn++;
	}

	if (!n)
		return EXIT_FAILURE;

	int pad = DIGINUM(nn + 1);
	for (i = 0; i < nn; i++)
		printf("%s%*zu%s %s\n", el_c, pad, i + 1, df_c, n[i]);

	int a = 0;
	char *input = get_user_input(&a, &nn);

	int ret = EXIT_FAILURE;

	if (!input || a <= 0) {
		free(input);
		free(n);
		if (!input && a == -1 && autols == 1) {
			reload_dirlist();
			return 0;
		}
		return ret;
	}

	char *app = n[a - 1];
	if (!app) {
		free(input);
		free(n);
		return ret;
	}

	if (strchr(app, ' ')) {
		size_t f = 0;
		char **cmd = split_str(app, NO_UPDATE_ARGS);
		for (i = 0; cmd[i]; i++) {
			if (*cmd[i] == '&') {
				bg_proc = 1;
				free(cmd[i]);
				cmd[i] = (char *)NULL;
				continue;
			}
			if (*cmd[i] == '%' && cmd[i][1] == 'f') {
				cmd[i] = (char *)xrealloc(cmd[i], (strlen(file) + 1) * sizeof(char));
				strcpy(cmd[i], file);
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
		 * ir to the cmd array */
		if (f == 0) {
			cmd = (char **)xrealloc(cmd, (i + 2) * sizeof(char *));
			cmd[i] = savestring(file, strlen(file));
			cmd[i + 1] = (char *)NULL;
		}

		if (launch_execve(cmd, bg_proc ? BACKGROUND : FOREGROUND,
		bg_proc ? E_NOSTDERR : E_NOFLAG) == EXIT_SUCCESS)
			ret = EXIT_SUCCESS;

		for (i = 0; cmd[i]; i++)
			free(cmd[i]);
		free(cmd);
	} else {
#ifndef _NO_ARCHIVING
		/* We have just a command name: no parameter, no placeholder */
		if (*app == 'a' && app[1] == 'd' && !app[2]) {
			/* 'ad' is the internal archiver command */
			char *cmd[] = {"ad", file, NULL};
			ret = archiver(cmd, 'd');
		} else
#endif /* _NO_ARCHIVING */
		{
			char *env = (char *)NULL;
			if (*app == '$' && app[1] >= 'A' && app[2] <= 'Z')
				env = expand_env(app);

			char *cmd[] = {env ? env : app, file, NULL};
			if (launch_execve(cmd, bg_proc ? BACKGROUND : FOREGROUND,
			bg_proc ? E_NOSTDERR : E_NOFLAG) == EXIT_SUCCESS)
				ret = EXIT_SUCCESS;
			free(env);
		}
	}

/*	bg_proc = 0; */

	free(input);
	free(n);

	return ret;
}

char **
mime_open_with_tab(char *filename, const char *prefix)
{
	if (!filename || !mime_file)
		return (char **)NULL;

	char *name = (char *)NULL,
		 *deq_file = (char *)NULL,
		 *mime = (char *)NULL,
		 *file_name = (char *)NULL,
		 **apps = (char **)NULL;

	int free_name = 1;
	if (*filename == '~') {
		char *ename = tilde_expand(filename);
		if (!ename)
			return (char **)NULL;
		name = ename;
		free_name = 0;
	}

	if (strchr(name ? name : filename, '\\')) {
		deq_file = dequote_str(name ? name : filename, 0);
		name = realpath(deq_file, NULL);
		free(deq_file);
		deq_file = (char *)NULL;
	}

	if (!name) {
		name = realpath(filename, NULL);
		if (!name)
			goto FAIL;
	}

#ifndef _NO_MAGIC
	mime = xmagic(name, MIME_TYPE);
#else
	mime = get_mime(name);
#endif
	if (!mime)
		goto FAIL;

	file_name = get_filename(name);

	FILE *defs_fp = fopen(mime_file, "r");
	if (!defs_fp)
		goto FAIL;

	size_t appsn = 1;

	/* This is the first element in the matches array, which contains
	 * the already matched string */
	apps = (char **)xnmalloc(appsn + 1, sizeof(char *));
	if (prefix) {
		apps[0] = savestring(prefix, strlen(prefix));
	} else {
		apps[0] = (char *)xnmalloc(1, sizeof(char));
		*apps[0] = '\0';
	}

	size_t prefix_len = 0;
	if (prefix)
		prefix_len = strlen(prefix);

	size_t line_size = 0;
	char *line = (char *)NULL, *app = (char *)NULL;

	while (getline(&line, &line_size, defs_fp) > 0) {
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
		regex_t regex;

		int found = 0;
		if (file_name && (*p == 'N' || *p == 'E') && *(p + 1) == ':') {
			if (regcomp(&regex, p + 2, REG_NOSUB | REG_EXTENDED) == 0
			&& regexec(&regex, file_name, 0, NULL, 0) == 0)
				found = 1;
		} else if (regcomp(&regex, p, REG_NOSUB | REG_EXTENDED) == 0
		&& regexec(&regex, mime, 0, NULL, 0) == 0) {
			found = 1;
		}

		regfree(&regex);

		if (!found)
			continue;

		tmp++; /* We don't want the '=' char */

		size_t tmp_len = strlen(tmp);
		app = (char *)xrealloc(app, (tmp_len + 1) * sizeof(char));

		while (*tmp) {
			size_t app_len = 0;
			/* Split the appplications line into substrings, if
			 * any */
			while (*tmp != '\0' && *tmp != ';'
			&& *tmp != '\n' && *tmp != '\'' && *tmp != '"') {
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

			if (prefix) {
				if (strncmp(prefix, app, prefix_len) != 0)
					continue;
			}
			app[app_len] = '\0';

			/* Do not list duplicated entries */
			size_t i, dup = 0;
			for (i = 0; i < appsn; i++) {
				if (*app == *apps[i] && strcmp(app, apps[i]) == 0) {
					dup = 1;
					break;
				}
			}
			if (dup == 1)
				continue;

			/* Check each application existence */
			char *file_path = (char *)NULL;

			/* Expand environment variables */
			char *appb = (char *)NULL;
			if (strchr(app, '$')) {
				char *t = expand_env(app);
				if (t) {
					/* appb: A copy of the original string: let's display
					 * the env var name itself instead of its expanded
					 * value */
					appb = savestring(app, strlen(app));
					/* app: the expanded value */
					app = (char *)xrealloc(app, (app_len + strlen(t) + 1) * sizeof(char));
					strcpy(app, t);
					free(t);
				} else {
					continue;
				}
			}

			/* If app contains spaces, the command to check is
			 * the string before the first space */
			char *ret = strchr(app, ' ');
			if (ret)
				*ret = '\0';

			if (*app == '~') {
				file_path = tilde_expand(app);
				if (access(file_path, X_OK) != 0) {
					free(file_path);
					file_path = (char *)NULL;
				}
			}
			/* Do not allow APP to be plain "clifm", since
			 * nested executions of clifm are not allowed */
			else if (*app == PNL[0] && strcmp(app, PNL) == 0) {
				;
			} else if (*app == '/') {
				if (access(app, X_OK) == 0)
					file_path = app;
			/* 'ad' is an internal command, no need to check it */
			} else if (*app == 'a' && app[1] == 'd' && !app[2]){
				file_path = savestring("ad", 2);
			} else {
				file_path = get_cmd_path(app);
			}

			if (ret)
				*ret = ' ';

			if (!file_path)
				continue;

			/* If the app exists, store it into the APPS array */
			if (*app != '/') {
				free(file_path);
				file_path = (char *)NULL;
			}

			apps = (char **)xrealloc(apps, (appsn + 2) * sizeof(char *));
			/* appb is not NULL if we have an environment variable */
			if (appb) {
				apps[appsn] = savestring(appb, strlen(appb));
				appsn++;
				free(appb);
			} else {
				apps[appsn] = savestring(app, strlen(app));
				appsn++;
			}

			tmp++;
		}
	}

	apps[appsn] = (char *)NULL;

	/* If only one match */
	if (appsn == 2) {
		apps[0] = (char *)xrealloc(apps[0], (strlen(apps[1]) + 1) * sizeof(char));
		strcpy(apps[0], apps[1]);
		free(apps[1]);
		apps[1] = (char *)NULL;
	}

	free(line);
	fclose(defs_fp);

	free(app);
	free(mime);
	free(name);

	return apps;

FAIL:
	free(mime);
	if (free_name)
		free(name);

	return (char **)NULL;
}

static int
run_cmd_noargs(char *arg, char *name)
{
	errno = 0;
	char *cmd[] = {arg, name, NULL};
	int ret = EXIT_SUCCESS;

#ifndef _NO_ARCHIVING
	if (*arg == 'a' && arg[1] == 'd' && !arg[2])
		ret = archiver(cmd, 'd');
	else
		ret = launch_execve(cmd, bg_proc ? BACKGROUND : FOREGROUND, E_NOSTDERR);
#else
	ret = launch_execve(cmd, bg_proc ? BACKGROUND : FOREGROUND, E_NOSTDERR);
#endif

	if (ret == EXIT_SUCCESS)
		return EXIT_SUCCESS;

	_err(ERR_NO_STORE, NOPRINT_PROMPT, "mime: %s: %s\n", arg, strerror(ret));
	return EXIT_FAILURE;
}

static void
append_params(char **args, char *name, char ***cmd)
{
	size_t i, n = 1, f = 0;
	for (i = 1; args[i]; i++) {
		if (*args[i] == '%' && *(args[i] + 1) == 'f' && !*(args[i] + 2)) {
			f = 1;
			(*cmd)[n] = savestring(name, strlen(name));
			n++;
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
	size_t i;
	for (i = 0; args[i]; i++);

	char **cmd = (char **)xnmalloc(i + 2, sizeof(char *));
	cmd[0] = savestring(args[0], strlen(args[0]));

	append_params(args, name, &cmd);

	int ret = launch_execve(cmd, bg_proc ? BACKGROUND : FOREGROUND,
			bg_proc ? E_NOSTDERR : E_NOFLAG);

	for (i = 0; cmd[i]; i++)
		free(cmd[i]);
	free(cmd);

	if (ret == EXIT_SUCCESS)
		return EXIT_SUCCESS;
	return EXIT_FAILURE;
}

static int
join_and_run(char **args, char *name)
{
	if (!args || !args[0])
		return EXIT_FAILURE;

	/* Just an application name */
	if (!args[1])
		return run_cmd_noargs(args[0], name);

	/* Application name plus parameters */
	return run_cmd_plus_args(args, name);
}

/* Display available opening applications for FILENAME, get user input,
 * and open the file */
int
mime_open_with(char *filename, char **args)
{
	if (!filename || !mime_file)
		return EXIT_FAILURE;

	char *name = (char *)NULL,
		 *deq_file = (char *)NULL,
		 *mime = (char *)NULL,
		 *file_name = (char *)NULL,
		 **apps = (char **)NULL;

	size_t appsn = 0;

	if (strchr(filename, '\\')) {
		deq_file = dequote_str(filename, 0);
		name = realpath(deq_file, NULL);
		free(deq_file);
		deq_file = (char *)NULL;
	}

	if ((name && *name == '~') || *filename == '~') {
		char *ename = tilde_expand(name ? name : filename);
		if (!ename)
			goto FAIL;
		free(name);
		name = ename;
	}

	if (!name) {
		name = realpath(filename, NULL);
		if (!name)
			return EXIT_FAILURE;
	}

	/* ow FILE APP [ARGS]
	 * We already have the opening app. Just join the app, option
	 * parameters, and file name, and execute the command */
	if (args) {
		int ret = join_and_run(args, name);
		free(name);
		return ret;
	}

	/* Find out the appropriate opening application via either mime type
	 * or file name */
#ifndef _NO_MAGIC
	mime = xmagic(name, MIME_TYPE);
#else
	mime = get_mime(name);
#endif
	if (!mime)
		goto FAIL;

	file_name = get_filename(name);

	FILE *defs_fp = fopen(mime_file, "r");
	if (!defs_fp)
		goto FAIL;

	size_t line_size = 0;
	char *line = (char *)NULL, *app = (char *)NULL;

	while (getline(&line, &line_size, defs_fp) > 0) {
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
		regex_t regex;

		int found = 0;
		if (file_name && (*p == 'N' || *p == 'E') && *(p + 1) == ':') {
			if (regcomp(&regex, p + 2, REG_NOSUB | REG_EXTENDED) == 0
			&& regexec(&regex, file_name, 0, NULL, 0) == 0)
				found = 1;
		} else {
			if (regcomp(&regex, p, REG_NOSUB | REG_EXTENDED) == 0
			&& regexec(&regex, mime, 0, NULL, 0) == 0)
				found = 1;
		}

		regfree(&regex);

		if (!found)
			continue;

		tmp++; /* We don't want the '=' char */

		size_t tmp_len = strlen(tmp);
		app = (char *)xrealloc(app, (tmp_len + 1) * sizeof(char));

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
				 * value */
				appb = savestring(app, strlen(app));
				/* app: the expanded value */
				app = (char *)xrealloc(app, (app_len + strlen(t) + 1)
						* sizeof(char));
				strcpy(app, t);
				free(t);
			}

			/* If app contains spaces, the command to check is
			 * the string before the first space */
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
			 * nested executions of clifm are not allowed */
			else if (*app == PNL[0] && strcmp(app, PNL) == 0) {
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

			if (ret)
				*ret = ' ';

			if (!file_path)
				continue;

			/* If the app exists, store it into the APPS array */
			if (*app != '/') {
				free(file_path);
				file_path = (char *)NULL;
			}
			apps = (char **)xrealloc(apps, (appsn + 2) * sizeof(char *));
			/* appb is not NULL if we have an environment variable */
			if (appb) {
				apps[appsn] = savestring(appb, strlen(appb));
				free(appb);
			} else {
				apps[appsn] = savestring(app, strlen(app));
			}

			appsn++;
			tmp++;
		}
	}

	free(line);
	fclose(defs_fp);

	free(app);
	free(mime);

	if (!apps) {
		free(name);
		return EXIT_FAILURE;
	}

	apps[appsn] = (char *)NULL;

	int ret = mime_list_open(apps, name);

	size_t i;
	for (i = 0; apps[i]; i++)
		free(apps[i]);
	free(apps);
	free(name);

	return ret;

FAIL:
	free(mime);
	free(name);

	return EXIT_FAILURE;
}

/* Open URL using the application associated to text/html MIME-type in
 * the mimelist file. Returns zero if success and 1 on error */
int
mime_open_url(char *url)
{
	if (!url || !*url)
		return EXIT_FAILURE;

	char *app = get_app("text/html", 0);
	if (!app)
		return EXIT_FAILURE;

	char *p = strchr(app, ' ');
	if (p)
		*p = '\0';

	char *cmd[] = {app, url, NULL};
	int ret = launch_execve(cmd, FOREGROUND, E_NOFLAG);
	free(app);

	if (ret != EXIT_SUCCESS)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

static int
import_mime(void)
{
	char *suffix = gen_rand_str(6);
	char new[PATH_MAX];
	snprintf(new, sizeof(new), "%s.%s", mime_file, suffix ? suffix : "5i0TM#");
	free(suffix);

	int mime_defs = mime_import(new);
	if (mime_defs > 0) {
		printf(_("%d MIME association(s) imported from the system\n"
			"File stored as %s\nAdd these new associations to your mimelist "
			"file running 'mm edit'\n"), mime_defs, new);
		return EXIT_SUCCESS;
	}

	return EXIT_FAILURE;
}

static int
mime_info(char *arg, char **fpath, char **deq)
{
	if (!arg) {
		fprintf(stderr, "%s\n", _(MIME_USAGE));
		return EXIT_FAILURE;
	}

	if (strchr(arg, '\\')) {
		*deq = dequote_str(arg, 0);
		*fpath = realpath(*deq, NULL);
		free(*deq);
		*deq = (char *)NULL;
	} else {
		*fpath = realpath(arg, NULL);
	}

	if (!*fpath) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "mime: %s: %s\n", arg,
		    (is_number(arg) == 1) ? _("No such ELN") : strerror(errno));
		return EXIT_FAILURE;
	}

	if (access(*fpath, R_OK) == -1) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "mime: %s: %s\n", *fpath, strerror(errno));
		free(*fpath);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
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
		*deq = dequote_str(f, 0);
		*fpath = realpath(*deq, NULL);
		free(*deq);
		*deq = (char *)NULL;
	}

	if (!*fpath) {
		*fpath = realpath(f, NULL);
		if (!*fpath) {
			_err(ERR_NO_STORE, NOPRINT_PROMPT, "mime: %s: %s\n", f, strerror(errno));
			return EXIT_FAILURE;
		}
	}

	if (access(*fpath, R_OK) == -1) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "mime: %s: %s\n", *fpath, strerror(errno));
		free(*fpath);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

/* Handle mime when no opening app has been found */
static int
handle_no_app(const int info, char **fpath, char **mime, const char *arg)
{
	if (info) {
		fputs(_("Associated application: None\n"), stderr);
	} else {
#ifndef _NO_ARCHIVING
		/* If an archive/compressed file, run the archiver function */
		if (is_compressed(*fpath, 1) == 0) {
			char *tmp_cmd[] = {"ad", *fpath, NULL};
			int exit_status = archiver(tmp_cmd, 'd');

			free(*fpath);
			free(*mime);

			return exit_status;
		} else {
			fprintf(stderr, _("mime: %s: No associated application found\n"), arg);
		}
#else
		fprintf(stderr, _("mime: %s: No associated application found\n"), arg);
#endif
	}

	free(*fpath);
	free(*mime);

	return EXIT_FAILURE;
}

static int
print_error_no_mime(char **fpath)
{
	_err(ERR_NO_STORE, NOPRINT_PROMPT, _("mime: Error getting mime-type\n"));
	free(*fpath);
	return EXIT_FAILURE;
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

	return EXIT_SUCCESS;
}

#ifndef _NO_ARCHIVING
static int
run_archiver(char **fpath, char **app)
{
	char *cmd[] = {"ad", *fpath, NULL};
	int exit_status = archiver(cmd, 'd');

	free(*fpath);
	free(*app);

	return exit_status;
}
#endif /* _NO_ARCHIVING */

static void
set_exec_flags(char *s, int *f)
{
	if (*s == 'E') {
		*f |= E_NOSTDERR;
		if (*(s + 1) == 'O')
			*f |= E_NOSTDOUT;
	} else if (*s == 'O') {
		*f |= E_NOSTDOUT;
		if (*(s + 1) == 'E')
			*f |= E_NOSTDERR;
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
			a[i] = (char *)xrealloc(a[i], (strlen(fpath) + 1) * sizeof(char));
			strcpy(a[i], fpath);
			f = 1;
			continue;
		}

		if (*a[i] == '!') {
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
			a[i] = (char *)xrealloc(a[i], (strlen(p) + 1) * sizeof(char *));
			strcpy(a[i], p);
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
		free(*fpath);
		return EXIT_FAILURE;
	}

	int exec_flags = 0;
	size_t i = 0;
	size_t f = expand_app_fields(&cmd, &i, *fpath, &exec_flags);
	size_t n = i;

	/* If no %f placeholder was found, append file name */
	if (f == 0) {
		cmd = (char **)xrealloc(cmd, (i + 2) * sizeof(char *));
		cmd[i] = savestring(*fpath, strlen(*fpath));
		cmd[i + 1] = (char *)NULL;
		n++;
	}

	int ret = launch_execve(cmd, (bg_proc && !open_in_foreground)
			? BACKGROUND : FOREGROUND, exec_flags);

	for (i = 0; i < n; i++)
		free(cmd[i]);
	free(cmd);

	free(*app);
	free(*fpath);

	if (ret == EXIT_SUCCESS)
		return EXIT_SUCCESS;
	return EXIT_FAILURE;
}

#ifdef _NO_MAGIC
/* Check the existence of the 'file' command */
static int
check_file_cmd(void)
{
	char *p = get_cmd_path("file");
	if (!p) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, _("mime: file: Command not found\n"));
		return EXIT_FAILURE;
	}

	free(p);
	return EXIT_SUCCESS;
}
#endif /* _NO_MAGIC */

static int
print_mime_help(void)
{
	puts(_(MIME_USAGE));
	return EXIT_FAILURE;
}

/* Open a file according to the application associated to its MIME type
 * or extension. It also accepts the 'info' and 'edit' arguments, the
 * former providing MIME info about the corresponding file and the
 * latter opening the MIME list file */
int
mime_open(char **args)
{
	if (!args[1] || IS_HELP(args[1]))
		return print_mime_help();

	if (*args[1] == 'i' && strcmp(args[1], "import") == 0)
		return import_mime();

	if (*args[1] == 'e' && strcmp(args[1], "edit") == 0)
		return mime_edit(args);

	char *file_path = (char *)NULL, *deq_file = (char *)NULL;
	int info = 0, file_index = 0;

	if (*args[1] == 'i' && strcmp(args[1], "info") == 0) {
		if (mime_info(args[2], &file_path, &deq_file) == EXIT_FAILURE)
			return EXIT_FAILURE;
		info = 1;
		file_index = 2;
	} else {
		if (get_open_file_path(args, &file_path, &deq_file) == EXIT_FAILURE)
			/* Return -1 to prevent the caller from printing the error message */
			return (-1);
		file_index = 1;
	}

	if (!file_path) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "%s: %s\n", args[file_index], strerror(errno));
		return EXIT_FAILURE;
	}

	/* Get file's MIME type */
#ifndef _NO_MAGIC
	char *mime = xmagic(file_path, MIME_TYPE);
#else
	if (check_file_cmd() == EXIT_FAILURE) {
		free(file_path);
		return EXIT_FAILURE;
	}
	char *mime = get_mime(file_path);
#endif

	if (!mime)
		return print_error_no_mime(&file_path);

	char *filename = get_filename(file_path);

	if (info)
		print_info_name_mime(filename, mime);

	/* Get default application for MIME or filename */
	char *app = get_app(mime, filename);
	if (!app)
		return handle_no_app(info, &file_path, &mime, args[1]);

	if (info)
		return print_mime_info(&app, &file_path, &mime);

	free(mime);

	/* Construct and execute the command */
#ifndef _NO_ARCHIVING
	if (*app == 'a' && app[1] == 'd' && !app[2])
		return run_archiver(&file_path, &app);
#endif

	return run_mime_app(&app, &file_path);
}
#else
void *__skip_me_lira;
#endif /* !_NO_LIRA */
