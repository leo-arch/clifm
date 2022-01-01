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
#include "exec.h"
#include "mime.h"
#include "messages.h"
#include "navigation.h"
#include "readline.h"
#include "misc.h"
#include "sanitize.h"

#include "strings.h"

/* Expand all environment variables in the string S
 * Returns the expanded string or NULL on error */
static char *
expand_env(char *s)
{
	char *p = strchr(s, '$');
	if (!p)
		return (char *)NULL;

	int buf_size = PATH_MAX;
	p = (char *)xnmalloc((size_t)buf_size, sizeof(char));
	char *ret = p;

	while (*s) {
		if (*s != '$') {
			*(p++) = *(s++);
			continue;
		}

		char *env = (char *)NULL;
		char *r = strchr(s, ' ');
		if (r)
			*r = '\0';
		env = getenv(s + 1);
		if (r)
			*r = ' ';
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

		if (r)
			*r = '\0';
		s += strlen(s);
		if (r)
			*r = ' ';
	}

	*p = '\0';
	return ret;
}

/* Move the pointer in LINE immediately after !X or X */
static char *
skip_line_prefix(char *line)
{
	char *p = line;
	if (!(flags & GUI)) {
		if (*p != '!' || *(p + 1) != 'X' || *(p + 2) != ':')
			return (char *)NULL;
		else
			p += 3;
	} else {
		if (*p == '!' && *(p + 1) == 'X')
			return (char *)NULL;
		if (*p == 'X' && *(p + 1) == ':')
			p += 2;
	}

	return p;
}

/* Get application associated to a given MIME file type or file extension.
 * Returns the first matching line in the MIME file or NULL if none is
 * found */
static char *
get_app(const char *mime, const char *ext)
{
	if (!mime || !mime_file || !*mime_file)
		return (char *)NULL;

	/* Directories are always opened by CliFM itself, except in the case
	 * of the open-with command (ow) */
	if (*mime == 'i' && strcmp(mime, "inode/directory") == 0) {
		char *p = savestring("clifm", 5);
		mime_match = 1;
		return p;
	}

	FILE *defs_fp = fopen(mime_file, "r");
	if (!defs_fp) {
		fprintf(stderr, _("%s: %s: Error opening file\n"),
		    PROGRAM_NAME, mime_file);
		return (char *)NULL;
	}

	int found = 0, cmd_ok = 0;
	size_t line_size = 0;
	char *line = (char *)NULL,
		 *app = (char *)NULL;

	while (getline(&line, &line_size, defs_fp) > 0) {
		found = mime_match = 0; /* Global variable to tell mime_open()
		if the application is associated to the file's extension or MIME
		type */
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

		if (ext && *p == 'E' && *(p + 1) == ':') {
			if (regcomp(&regex, p + 2, REG_NOSUB | REG_EXTENDED) == 0
			&& regexec(&regex, ext, 0, NULL, 0) == 0)
				found = 1;
		} else if (regcomp(&regex, p, REG_NOSUB | REG_EXTENDED) == 0
		&& regexec(&regex, mime, 0, NULL, 0) == 0) {
			found = mime_match = 1;
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
			while (*tmp != '\0' && *tmp != ';' && *tmp != '\n' && *tmp != '\''
			&& *tmp != '"')
				app[app_len++] = *(tmp++);

			while (*tmp == ' ') /* Remove leading spaces */
				tmp++;

			if (app_len) {
				app[app_len] = '\0';
				/* Check each application existence */
				char *file_path = (char *)NULL;

				/* Expand environment variables */
				if (strchr(app, '$')) {
					char *t = expand_env(app);
					if (t) {
						app = (char *)xrealloc(app, (strlen(t) + 1) * sizeof(char));
						strcpy(app, t);
						free(t);
					}
				}

				if (xargs.secure_cmds == 1
				&& sanitize_cmd(app, SNT_MIME) != EXIT_SUCCESS) {
					_err('w', PRINT_PROMPT, "Lira: %s: Command contains "
						"unsafe characters\n", app);
					continue;
				}

				/* If app contains spaces, the command to check is
				 * the string before the first space */
				char *ret = strchr(app, ' ');
				if (ret)
					*ret = '\0';

				if (*app == 'a' && app[1] == 'd' && !app[2]) {
					/* No need to check: ad is an internal command */
					cmd_ok = 1;
					if (ret)
						*ret = ' ';
					break;
				} else {
					file_path = get_cmd_path(app);
				}

				if (ret)
					*ret = ' ';

				if (file_path) {
					/* If the app exists, break the loops and
					 * return it */
					free(file_path);
					file_path = (char *)NULL;
					cmd_ok = 1;
				} else {
					continue;
				}
			}

			if (cmd_ok)
				break;
			tmp++;
		}

		if (cmd_ok)
			break;
	}

	free(line);
	fclose(defs_fp);

	if (found)
		return app;
	else
		free(app);

	return (char *)NULL;
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

	char *str = (char *)xnmalloc(strlen(mime) + 1, sizeof(char));
	strcpy(str, mime);
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

	char mime_tmp_file[PATH_MAX] = "";
	sprintf(mime_tmp_file, "%s/mime.%s", tmp_dir, rand_ext);
	free(rand_ext);

	if (access(mime_tmp_file, F_OK) == 0)
		unlink(mime_tmp_file);

	FILE *file_fp = fopen(mime_tmp_file, "w");
	if (!file_fp) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, mime_tmp_file,
		    strerror(errno));
		return (char *)NULL;
	}

	FILE *file_fp_err = fopen("/dev/null", "w");
	if (!file_fp_err) {
		fprintf(stderr, "%s: /dev/null: %s\n", PROGRAM_NAME, strerror(errno));
		fclose(file_fp);
		return (char *)NULL;
	}

	int stdout_bk = dup(STDOUT_FILENO); /* Store original stdout */
	int stderr_bk = dup(STDERR_FILENO); /* Store original stderr */

	/* Redirect stdout to the desired file */
	if (dup2(fileno(file_fp), STDOUT_FILENO) == -1) {
		fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
		fclose(file_fp);
		fclose(file_fp_err);
		return (char *)NULL;
	}

	/* Redirect stderr to /dev/null */
	if (dup2(fileno(file_fp_err), STDERR_FILENO) == -1) {
		fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
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

/* Import MIME definitions from the system and store them into FILE.
 * Returns the amount of definitions found, if any, or -1 in case of error
 * or no association found */
static int
mime_import(char *file)
{
#ifdef __HAIKU__
	fprintf(stderr, "%s: Importing MIME definitions is not supported on Haiku\n",
			PROGRAM_NAME);
	return (-1);
#endif
	/* If not in X, exit) */
	if (!(flags & GUI)) {
		fprintf(stderr, _("%s: Nothing was imported. No graphical "
				"environment found\n"), PROGRAM_NAME);
		return (-1);
	}

	if (!user.home) {
		fprintf(stderr, _("%s: Error getting home directory\n"), PROGRAM_NAME);
		return (-1);
	}

	/* Open the local MIME file */
	FILE *mime_fp = fopen(file, "w");
	if (!mime_fp) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, file, strerror(errno));
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

	/* Check each mimeapps.list file and store its associations into
	 * FILE */
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
		int da_found = 0;

		while (getline(&line, &line_size, sys_mime_fp) > 0) {
			if (!da_found && strncmp(line, "[Default Applications]", 22) == 0) {
				da_found = 1;
				continue;
			}

			if (da_found) {
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

	if (mime_defs == 0) {
		fprintf(stderr, _("%s: Nothing was imported. No MIME definitions "
			"found\n"), PROGRAM_NAME);
	}

	/* Make sure there is an entry for text/plain files, so that at least
	 * 'mm edit' will work */
	fputs("X:text/plain=nano;vim;vi;emacs;ed;vis;micro;kakoune;"
		  "gedit;kate;pluma;mousepad;leafpad;fetchpad\n"
		  "!X:text/plain=nano;vim;vi;emacs;ed;vis;micro;kakoune\n",
		  mime_fp);

	fclose(mime_fp);
	return mime_defs;
}

static int
mime_edit(char **args)
{
	int exit_status = EXIT_SUCCESS;

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

	return exit_status;
}

static char *
get_file_ext(char *name)
{
	char *ext = (char *)NULL;
	char *f = strrchr(name, '/');
	if (f) {
		f++; /* Skip leading slash */
		if (*f == '.')
			f++; /* Skip leading dot if hidden */

		char *e = strrchr(f, '.');
		if (e) {
			e++; /* Remove dot from extension */
			ext = savestring(e, strlen(e));
		}
	}

	return ext;
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
		if (i > 0) {
			/* Do not list duplicated entries */
			for (j = 0; j < i; j++) {
				if (*apps[i] == *apps[j] && strcmp(apps[i], apps[j]) == 0) {
					rep = 1;
					break;
				}
			}
		}
		if (rep)
			continue;

		n = (char **)xrealloc(n, (nn + 1) * sizeof(char *));
		n[nn++] = apps[i];
	}

	if (!n)
		return EXIT_FAILURE;

	int pad = DIGINUM(nn + 1);
	for (i = 0; i < nn; i++)
		printf("%s%*zu%s %s\n", el_c, pad, i + 1, df_c, n[i]);

	int a = 0;
	char *input = get_user_input(&a, &nn);

	int ret = EXIT_FAILURE;
	if (input && a > 0 && n[a - 1]) {
		if (strchr(n[a - 1], ' ')) {
			size_t f = 0;
			char **cmd = split_str(n[a - 1], NO_UPDATE_ARGS);

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
			if (*n[a - 1] == 'a' && n[a - 1][1] == 'd' && !n[a - 1][2]) {
				/* 'ad' is the internal archiver command */
				char *cmd[] = {"ad", file, NULL};
				ret = archiver(cmd, 'd');
			} else {
				char *cmd[] = {n[a - 1], file, NULL};
				if (launch_execve(cmd, bg_proc ? BACKGROUND : FOREGROUND,
				bg_proc ? E_NOSTDERR : E_NOFLAG) == EXIT_SUCCESS) {
					ret = EXIT_SUCCESS;
				}
			}
#else
			char *cmd[] = {n[a - 1], file, NULL};
			if (launch_execve(cmd, bg_proc ? BACKGROUND : FOREGROUND,
			bg_proc ? E_NOSTDERR : E_NOFLAG) == EXIT_SUCCESS)
				ret = EXIT_SUCCESS;
#endif /* NO_ARCHIVING */
		}

		bg_proc = 0;
	}

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
		 *ext = (char *)NULL,
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

	if (!name)
		name = realpath(filename, NULL);

	if (!name)
		goto FAIL;

#ifndef _NO_MAGIC
	mime = xmagic(name, MIME_TYPE);
#else
	mime = get_mime(name);
#endif
	if (!mime)
		goto FAIL;

	ext = get_file_ext(name);

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
		if (ext && *p == 'E' && *(p + 1) == ':') {
			if (regcomp(&regex, p + 2, REG_NOSUB | REG_EXTENDED) == 0
			&& regexec(&regex, ext, 0, NULL, 0) == 0)
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

			if (app_len) {
				if (prefix) {
					if (strncmp(prefix, app, prefix_len) != 0)
						continue;
				}
				app[app_len] = '\0';
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
						app = (char *)xrealloc(app, (app_len + strlen(t) + 1)
								* sizeof(char));
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
				/* Do not allow APP to be plain "clifm", since
				 * nested executions of clifm are not allowed */
				} else if (*app == PNL[0] && strcmp(app, PNL) == 0) {
					;
				} else if (*app == '/') {
					if (access(app, X_OK) == 0) {
						file_path = app;
					}
				/* 'ad' is an internal command, no need to check it */
				} else if (*app == 'a' && app[1] == 'd' && !app[2]){
					file_path = savestring("ad", 2);
				} else {
					file_path = get_cmd_path(app);
				}
				if (ret)
					*ret = ' ';

				if (file_path) {
					/* If the app exists, store it into the APPS array */
					if (*app != '/') {
						free(file_path);
						file_path = (char *)NULL;
					}

					apps = (char **)xrealloc(apps, (appsn + 2) * sizeof(char *));
					/* appb is not NULL if we have an environment variable */
					if (appb) {
						apps[appsn++] = savestring(appb, strlen(appb));
						free(appb);
					} else {
						apps[appsn++] = savestring(app, strlen(app));
					}
				} else {
					continue;
				}
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
	free(ext);
	free(name);

	return apps;

FAIL:
	free(mime);
	free(ext);
	if (free_name)
		free(name);

	return (char **)NULL;
}

static int
join_and_run(char **args, char *name)
{
	/* Just an aplication name */
	if (!args[1]) {
		errno = 0;
		char *cmd[] = {args[0], name, NULL};
		int ret = EXIT_SUCCESS;
#ifndef _NO_ARCHIVING
		if (*args[0] == 'a' && args[0][1] == 'd' && !args[0][2]) {
			ret = archiver(cmd, 'd');
		} else {
			ret = launch_execve(cmd, bg_proc ? BACKGROUND : FOREGROUND,
				E_NOSTDERR);
		}
#else
		ret = launch_execve(cmd, bg_proc ? BACKGROUND : FOREGROUND, E_NOSTDERR);
#endif
		if (ret == EXIT_SUCCESS)
			return EXIT_SUCCESS;
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, args[0], strerror(ret));
		return EXIT_FAILURE;
	}

	size_t i, n = 1, f = 0;
	for (i = 0; args[i]; i++);
	char **cmd = (char **)xnmalloc(i + 2, sizeof(char *));
	cmd[0] = savestring(args[0], strlen(args[0]));

	for (i = 1; args[i]; i++) {
		if (*args[i] == '%' && *(args[i] + 1) == 'f' && !*(args[i] + 2)) {
			f = 1;
			cmd[n] = savestring(name, strlen(name));
			n++;
		} else if (*args[i] == '$' && *(args[i] + 1) >= 'A'
		&& *(args[i] + 1) <= 'Z') {
			char *env = expand_env(args[i]);
			if (env) {
				cmd[n] = savestring(env, strlen(env));
				n++;
			}
		} else if (*args[i] == '&') {
			bg_proc = 1;
		} else {
			cmd[n] = savestring(args[i], strlen(args[i]));
			n++;
		}
	}

	if (f == 0) {
		cmd[n] = savestring(name, strlen(name));
		n++;
	}

	cmd[n] = (char *)NULL;

	int ret = launch_execve(cmd, bg_proc ? BACKGROUND : FOREGROUND,
			bg_proc ? E_NOSTDERR : E_NOFLAG);

	for (i = 0; cmd[i]; i++)
		free(cmd[i]);
	free(cmd);

	if (ret == EXIT_SUCCESS)
		return EXIT_SUCCESS;
	return EXIT_FAILURE;
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
		 *ext = (char *)NULL,
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

	if (!name)
		name = realpath(filename, NULL);

	if (!name)
		return EXIT_FAILURE;

	/* ow FILE APP [ARGS]
	 * We already have the opening app. Just join the app, option
	 * parameters, and file name, and execute the command */
	if (args) {
		int ret = join_and_run(args, name);
		free(name);
		return ret;
	}

	/* Find out the appropriate opening application via either mime type
	 * or file extension */
#ifndef _NO_MAGIC
	mime = xmagic(name, MIME_TYPE);
#else
	mime = get_mime(name);
#endif
	if (!mime)
		goto FAIL;

	ext = get_file_ext(name);

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
		if (ext && *p == 'E' && *(p + 1) == ':') {
			if (regcomp(&regex, p + 2, REG_NOSUB | REG_EXTENDED) == 0
			&& regexec(&regex, ext, 0, NULL, 0) == 0)
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
			while (*tmp != '\0' && *tmp != ';' && *tmp != '\n' && *tmp != '\''
			&& *tmp != '"')
				app[app_len++] = *(tmp++);

			while (*tmp == ' ') /* Remove leading spaces */
				tmp++;

			if (app_len) {
				app[app_len] = '\0';
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
						app = (char *)xrealloc(app, (app_len + strlen(t) + 1)
								* sizeof(char));
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
					if (file_path && access(file_path, X_OK) != 0) {
						free(file_path);
						file_path = (char *)NULL;
					}
				/* Do not allow APP to be plain "clifm", since
				 * nested executions of clifm are not allowed */
				} else if (*app == PNL[0] && strcmp(app, PNL) == 0) {
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

				if (file_path) {
					/* If the app exists, store it into the APPS array */
					if (*app != '/') {
						free(file_path);
						file_path = (char *)NULL;
					}
					apps = (char **)xrealloc(apps, (appsn + 2) * sizeof(char *));
					/* appb is not NULL if we have an environment variable */
					if (appb) {
						apps[appsn++] = savestring(appb, strlen(appb));
						free(appb);
					} else {
						apps[appsn++] = savestring(app, strlen(app));
					}
				} else {
					continue;
				}
			}

			tmp++;
		}
	}

	free(line);
	fclose(defs_fp);

	free(app);
	free(mime);
	free(ext);

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
	free(ext);
	free(name);

	return EXIT_FAILURE;
}

/* Open a file according to the application associated to its MIME type
 * or extension. It also accepts the 'info' and 'edit' arguments, the
 * former providing MIME info about the corresponding file and the
 * latter opening the MIME list file */
int
mime_open(char **args)
{
	/* Check arguments */
	if (!args[1] || (*args[1] == '-' && strcmp(args[1], "--help") == 0)) {
		puts(_(MIME_USAGE));
		return EXIT_FAILURE;
	}

	if (args[1] && *args[1] == 'i' && strcmp(args[1], "import") == 0) {
		time_t rawtime = time(NULL);
		struct tm tm;
		localtime_r(&rawtime, &tm);
		char date[64];
		strftime(date, sizeof(date), "%b %d %H:%M:%S %Y", &tm);

		char suffix[68];
		snprintf(suffix, 67, "%d%d%d%d%d%d", tm.tm_year + 1900,
		    tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min,
		    tm.tm_sec);

		char new[PATH_MAX];
		snprintf(new, PATH_MAX - 1, "%s.%s", mime_file, suffix);
		rename(mime_file, new);

		int mime_defs = mime_import(mime_file);
		if (mime_defs > 0) {
			printf(_("%s: %d MIME definition(s) imported from the system. "
				"Old MIME list file stored as %s\n"),
				PROGRAM_NAME, mime_defs, new);
			return EXIT_SUCCESS;
		} else {
			rename(new, mime_file);
			return EXIT_FAILURE;
		}
	}

#ifdef _NO_MAGIC
	/* Check the existence of the 'file' command. */
	char *file_path_tmp = (char *)NULL;
	if ((file_path_tmp = get_cmd_path("file")) == NULL) {
		fprintf(stderr, _("%s: file: Command not found\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	free(file_path_tmp);
#endif /* _NO_MAGIC */

	char *file_path = (char *)NULL,
		 *deq_file = (char *)NULL;
	int info = 0, file_index = 0;

	if (*args[1] == 'e' && strcmp(args[1], "edit") == 0)
		return mime_edit(args);

	else if (*args[1] == 'i' && strcmp(args[1], "info") == 0) {
		if (!args[2]) {
			fprintf(stderr, "%s\n", _(MIME_USAGE));
			return EXIT_FAILURE;
		}

		if (strchr(args[2], '\\')) {
			deq_file = dequote_str(args[2], 0);
			file_path = realpath(deq_file, NULL);
			free(deq_file);
			deq_file = (char *)NULL;
		} else {
			file_path = realpath(args[2], NULL);
		}

		if (!file_path) {
			fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, args[2],
			    (is_number(args[2]) == 1) ? _("No such ELN") : strerror(errno));
			return EXIT_FAILURE;
		}

		if (access(file_path, R_OK) == -1) {
			fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, file_path,
			    strerror(errno));
			free(file_path);
			return EXIT_FAILURE;
		}

		info = 1;
		file_index = 2;
	}

	else {
		/* Only dequote the file name if coming from the mime command */
		if (*args[0] == 'm' && strchr(args[1], '\\')) {
			deq_file = dequote_str(args[1], 0);
			file_path = realpath(deq_file, NULL);
			free(deq_file);
			deq_file = (char *)NULL;
		}
		if (!file_path)
			file_path = realpath(args[1], NULL);

		if (!file_path) {
			fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, args[1],
			    strerror(errno));
			return (-1);
		}

		if (access(file_path, R_OK) == -1) {
			fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, file_path,
			    strerror(errno));
			free(file_path);
			/* Since this function is called by open_function, and since
			 * this latter prints an error message itself whenever the
			 * exit code of mime_open is EXIT_FAILURE, and since we
			 * don't want that message in this case, return -1 instead
			 * to prevent that message from being printed */
			return (-1);
		}

		file_index = 1;
	}

	if (!file_path) {
		fprintf(stderr, "%s: %s\n", args[file_index], strerror(errno));
		return EXIT_FAILURE;
	}

	/* Get file's MIME type */
#ifndef _NO_MAGIC
	char *mime = xmagic(file_path, MIME_TYPE);
#else
	char *mime = get_mime(file_path);
#endif
	if (!mime) {
		fprintf(stderr, _("%s: Error getting mime-type\n"), PROGRAM_NAME);
		free(file_path);
		return EXIT_FAILURE;
	}

	if (info)
		printf(_("MIME type: %s\n"), mime);

	char *ext = get_file_ext(file_path);

	if (info)
		printf(_("Extension: %s\n"), ext ? ext : _("None"));

	/* Get default application for MIME or extension */
	char *app = get_app(mime, ext);
	if (!app) {
		if (info) {
			fputs(_("Associated application: None\n"), stderr);
		} else {
#ifndef _NO_ARCHIVING
			/* If an archive/compressed file, run the archiver function */
			if (is_compressed(file_path, 1) == 0) {
				char *tmp_cmd[] = {"ad", file_path, NULL};
				int exit_status = archiver(tmp_cmd, 'd');

				free(file_path);
				free(mime);
				free(ext);

				return exit_status;
			} else {
				fprintf(stderr, _("%s: %s: No associated application "
						"found\n"), PROGRAM_NAME, args[1]);
			}
#else
			fprintf(stderr, _("%s: %s: No associated application "
					"found\n"), PROGRAM_NAME, args[1]);
#endif
		}

		free(file_path);
		free(mime);
		free(ext);

		return EXIT_FAILURE;
	}

	if (info) {
		if (*app == 'a' && app[1] == 'd' && !app[2]) {
			printf(_("Associated application: ad [built-in] [%s]\n"),
				mime_match ? "MIME" : "ext");
		} else {
			printf(_("Associated application: %s [%s]\n"), app,
				mime_match ? "MIME" : "ext");
		}

		free(file_path);
		free(mime);
		free(app);
		free(ext);

		return EXIT_SUCCESS;
	}

	free(mime);
	free(ext);

	/* If not info, open the file with the associated application */
#ifndef _NO_ARCHIVING
	if (*app == 'a' && app[1] == 'd' && !app[2]) {
		char *cmd[] = {"ad", file_path, NULL};
		int exit_status = archiver(cmd, 'd');
		free(file_path);
		free(app);
		return exit_status;
	}
#endif

	int ret = EXIT_FAILURE;
	char **cmd = split_str(app, NO_UPDATE_ARGS);
	if (!cmd)
		goto ERROR;

	size_t i, f = 0, n = 0;
	for (i = 0; cmd[i]; i++) {
		if (*cmd[i] == '%' && *(cmd[i] + 1) == 'f') {
			cmd[i] = (char *)xrealloc(cmd[i], (strlen(file_path) + 1)
					* sizeof(char *));
			strcpy(cmd[i], file_path);
			f = 1;
		} else if (*cmd[i] == '$' && *(cmd[i] + 1) >= 'A'
		&& *(cmd[i] + 1) <= 'Z') {
			char *p = expand_env(cmd[i]);
			if (!p)
				continue;
			cmd[i] = (char *)xrealloc(cmd[i], (strlen(p) + 1) * sizeof(char *));
			strcpy(cmd[i], p);
			free(p);
		} else {
			if (*cmd[i] == '&') {
				free(cmd[i]);
				cmd[i] = (char *)NULL;
				bg_proc = 1;
			}
		}
	}

	n = i;
	if (f == 0) {
		cmd = (char **)xrealloc(cmd, (i + 2) * sizeof(char *));
		cmd[i] = savestring(file_path, strlen(file_path));
		cmd[i + 1] = (char *)NULL;
		n++;
	}

	ret = launch_execve(cmd, (bg_proc && !open_in_foreground)
			? BACKGROUND : FOREGROUND, bg_proc ? E_NOSTDERR : E_NOFLAG);

	for (i = 0; i < n; i++)
		free(cmd[i]);
	free(cmd);

ERROR:
	free(file_path);
	free(app);
	return ret;
}
#else
void *__skip_me_lira;
#endif /* !_NO_LIRA */
