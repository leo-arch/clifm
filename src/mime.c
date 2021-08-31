/* mime.c -- functions controlling Lira, the resource opener */

/*
 * This file is part of CliFM
 * 
 * Copyright (C) 2016-2021, L. Abramovich <johndoe.arch@outlook.com>
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

#include "archives.h"
#include "aux.h"
#include "checks.h"
#include "exec.h"
#include "mime.h"
#include "messages.h"
#include "navigation.h"

/* Get application associated to a given MIME file type or file extension.
 * Returns the first matching line in the MIME file or NULL if none is
 * found */
static char *
get_app(const char *mime, const char *ext)
{
	if (!mime)
		return (char *)NULL;

	if (!mime_file || !*mime_file)
		return (char *)NULL;

	FILE *defs_fp = fopen(mime_file, "r");
	if (!defs_fp) {
		fprintf(stderr, _("%s: %s: Error opening file\n"),
		    PROGRAM_NAME, mime_file);
		return (char *)NULL;
	}

	int found = 0, cmd_ok = 0;
	size_t line_size = 0;
	char *line = (char *)NULL, *app = (char *)NULL;
/*	ssize_t line_len = 0; */

	while (getline(&line, &line_size, defs_fp) > 0) {
		found = mime_match = 0; /* Global variable to tell mime_open()
		if the application is associated to the file's extension or MIME
		type */
		if (*line == '#' || *line == '[' || *line == '\n')
			continue;

		char *p = line;
		if (!(flags & GUI)) {
			if (*p != '!' || *(p + 1) != 'X' || *(p + 2) != ':')
				continue;
			else
				p += 3;
		} else {
			if (*p == '!' && *(p + 1) == 'X')
				continue;
			if (*p == 'X' && *(p + 1) == ':')
				p += 2;
		}

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
				/* If app contains spaces, the command to check is
				 * the string before the first space */
				char *ret = strchr(app, ' ');
				if (ret) {
					*ret = '\0';
					if (*app == '~') {
						file_path = tilde_expand(app);
						if (access(file_path, X_OK) != 0) {
							free(file_path);
							file_path = (char *)NULL;
						}
					} else if (*app == '/') {
						if (access(app, X_OK) == 0) {
							file_path = app;
						}
					} else {
						file_path = get_cmd_path(app);
					}
					*ret = ' ';
				} else {
					if (*app == '~') {
						file_path = tilde_expand(app);
						if (access(file_path, X_OK) != 0) {
							free(file_path);
							file_path = (char *)NULL;
						}
					}
					else if (*app == '/') {
						if (access(app, X_OK) == 0) {
							file_path = app;
						}
					} else {
						file_path = get_cmd_path(app);
					}
				}

				if (file_path) {
					/* If the app exists, break the loops and
					 * return it */
					if (*app != '/') {
						free(file_path);
						file_path = (char *)NULL;
					}
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

	if (found) {
		if (app)
			return app;
	} else {
		if (app)
			free(app);
	}

	return (char *)NULL;
}

#ifndef _NO_MAGIC
/* Get FILE's MIME type using the libmagic library */
static char *
xmagic(const char *file)
{
	if (!file || !*file)
		return (char *)NULL;

	magic_t cookie = magic_open(MAGIC_MIME_TYPE | MAGIC_ERROR);
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

#else
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

	char MIME_TMP_FILE[PATH_MAX] = "";
	sprintf(MIME_TMP_FILE, "%s/mime.%s", TMP_DIR, rand_ext);
	free(rand_ext);

	if (access(MIME_TMP_FILE, F_OK) == 0)
		unlink(MIME_TMP_FILE);

	FILE *file_fp = fopen(MIME_TMP_FILE, "w");
	if (!file_fp) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, MIME_TMP_FILE,
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

	if (access(MIME_TMP_FILE, F_OK) != 0)
		return (char *)NULL;

	file_fp = fopen(MIME_TMP_FILE, "r");
	if (!file_fp) {
		unlink(MIME_TMP_FILE);
		return (char *)NULL;
	}

	char *mime_type = (char *)NULL;

	char line[255] = "";
	if (fgets(line, (int)sizeof(line), file_fp) == NULL) {
		fclose(file_fp);
		unlink(MIME_TMP_FILE);
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
	unlink(MIME_TMP_FILE);
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

	if (mime_defs <= 0) {
		fclose(mime_fp);
		fprintf(stderr, _("%s: Nothing was imported. No MIME definitions "
			"found\n"), PROGRAM_NAME);
		return (-1);
	}

	/* Make sure there is an entry for text/plain and *.cfm files, so
	 * that at least 'mm edit' will work. Gedit, kate, pluma, mousepad,
	 * and leafpad, are the default text editors of Gnome, KDE, Mate,
	 * XFCE, and LXDE respectivelly */
	fputs("X:text/plain=gedit;kate;pluma;mousepad;leafpad;nano;vim;"
	      "vi;emacs;ed\n"
	      "X:E:^cfm$=gedit;kate;pluma;mousepad;leafpad;nano;vim;vi;"
	      "emacs;ed\n", mime_fp);

	fclose(mime_fp);
	return mime_defs;
}

static int
mime_edit(char **args)
{
	int exit_status = EXIT_SUCCESS;

	if (!args[2]) {
		char *cmd[] = {"mime", mime_file, NULL};
		if (mime_open(cmd) != 0) {
			fputs(_("Try 'mm, mime edit APPLICATION'\n"), stderr);
			exit_status = EXIT_FAILURE;
		}

	} else {
		char *cmd[] = {args[2], mime_file, NULL};
		if (launch_execve(cmd, FOREGROUND, E_NOSTDERR) != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;
	}

	return exit_status;
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

	/* Check the existence of the 'file' command. */
/*	char *file_path_tmp = (char *)NULL;
	if ((file_path_tmp = get_cmd_path("file")) == NULL) {
		fprintf(stderr, _("%s: file: Command not found\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	free(file_path_tmp);
	file_path_tmp = (char *)NULL; */

	char *file_path = (char *)NULL,
		 *deq_file = (char *)NULL;
	int info = 0,
		file_index = 0;

	if (*args[1] == 'e' && strcmp(args[1], "edit") == 0) {
		return mime_edit(args);
	}

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
			return -1;
		}

		struct stat a;
		if (lstat(file_path, &a) == 0 && (a.st_mode & S_IFMT) == S_IFDIR) {
			int _exit_status = cd_function(file_path);
			free(file_path);
			return _exit_status;
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
			return -1;
		}

		file_index = 1;
	}

	if (!file_path) {
		fprintf(stderr, "%s: %s\n", args[file_index], strerror(errno));
		return EXIT_FAILURE;
	}

	/* Get file's mime-type */
#ifndef _NO_MAGIC
	char *mime = xmagic(file_path);
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

	/* Get file extension, if any */
	char *ext = (char *)NULL;
	char *filename = strrchr(file_path, '/');
	if (filename) {
		filename++; /* Remove leading slash */
		if (*filename == '.')
			filename++; /* Skip leading dot if hidden */

		char *ext_tmp = strrchr(filename, '.');
		if (ext_tmp) {
			ext_tmp++; /* Remove dot from extension */
			ext = savestring(ext_tmp, strlen(ext_tmp));
			ext_tmp = (char *)NULL;
		}

		filename = (char *)NULL;
	}

	if (info)
		printf(_("Extension: %s\n"), ext ? ext : "None");

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

				if (ext)
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

		if (ext)
			free(ext);

		return EXIT_FAILURE;
	}

	if (info) {
		/* In case of "cmd args" print only cmd */
		char *ret = strchr(app, ' ');
		if (ret)
			*ret = '\0';

		printf(_("Associated application: %s (%s)\n"), app,
		    mime_match ? "MIME" : "ext");

		free(file_path);
		free(mime);
		free(app);

		if (ext)
			free(ext);

		return EXIT_SUCCESS;
	}

	free(mime);

	if (ext)
		free(ext);

	/* If not info, open the file with the associated application */

	/* Get number of arguments to check for final ampersand */
	int args_num = 0;
	for (args_num = 0; args[args_num]; args_num++);

	/* Construct the command and run it */

	/* Two pointers to store different positions in the APP string */
	char *p = app;
	char *pp = app;

	/* The number of spaces in APP is (at least) the number of paramenters
	 * passed to the command. Extra spaces will be ignored */
	size_t spaces = 0;
	while (*p) {
		if (*(p++) == ' ')
			spaces++;
	}

	/* To the number of spaces/parametes we need to add the command itself,
	 * the file name and the final NULL string (spaces + 3) */
	char **cmd = (char **)xnmalloc(spaces + 3, sizeof(char *));

	/* Rewind P to the beginning of APP */
	p = pp;

	/* Store each substring in APP into a two dimensional array (CMD) */
	int pos = 0;
	while (1) {
		if (!*p) {
			if (*pp)
				cmd[pos++] = savestring(pp, strlen(pp));
			break;
		}

		if (*p == ' ') {
			*p = '\0';
			if (*pp)
				cmd[pos++] = savestring(pp, strlen(pp));
			pp = ++p;
		} else {
			p++;
		}
	}

	/* If %f placeholder is found, replace it by FILE_PATH. Else, append
	 * FILE_PATH to the end of CMD */
	int i = pos,
		found = 0;
	while (--i >= 0) {
		if (*cmd[i] == '%' && *(cmd[i] + 1) == 'f') {
			found = 1;
			break;
		}
	}

	if (found) {
		cmd[i] = (char *)xrealloc(cmd[i], (strlen(file_path) + 1) * sizeof(char));
		strcpy(cmd[i], file_path);
	} else {
		cmd[pos++] = savestring(file_path, strlen(file_path));
	}

	cmd[pos] = (char *)NULL;

	int ret = launch_execve(cmd, bg_proc ? BACKGROUND : FOREGROUND, E_NOSTDERR);

	free(file_path);
	free(app);

	i = pos;
	while (--i >= 0)
		free(cmd[i]);
	free(cmd);
	return ret;
}
#endif /* !_NO_LIRA */
