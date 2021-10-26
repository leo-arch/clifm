/* init.c -- functions controlling the program initialization */

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

#include "helpers.h"

#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#ifdef __NetBSD__
#include <ctype.h>
#endif

#include "aux.h"
#include "checks.h"
#include "config.h"
#include "exec.h"
#include "init.h"
#include "mime.h"
#include "misc.h"
#include "navigation.h"
#include "sort.h"
#include "string.h"
#include "history.h"
#include "file_operations.h"

//struct user_t user;

/* 
 * functions
 */

int
get_sys_shell(void)
{
	char l[PATH_MAX] = "";
	ssize_t ret = readlinkat(AT_FDCWD, "/bin/sh", l, PATH_MAX);

	if (!*l || ret == -1)
		return SHELL_NONE;

	char *s = (char *)NULL;
	char *p = strrchr(l, '/');
	if (p && *(++p))
		s = p;
	else
		s = l;

	if (*s == 'b' && strcmp(s, "bash") == 0)
		return SHELL_BASH;
	if (*s == 'd' && strcmp(s, "dash") == 0)
		return SHELL_DASH;
	if (*s == 'f' && strcmp(s, "fish") == 0)
		return SHELL_FISH;
	if (*s == 'z' && strcmp(s, "zsh") == 0)
		return SHELL_ZSH;

	return SHELL_NONE;
}

#ifndef _NO_GETTEXT
/* Initialize gettext for translations support */
int
init_gettext(void)
{
	char locale_dir[PATH_MAX];
	snprintf(locale_dir, PATH_MAX - 1, "%s/locale", data_dir
			? data_dir : "/usr/share");
	bindtextdomain(PNL, locale_dir);
	textdomain(PNL);
	return EXIT_SUCCESS;

}
#endif

int
backup_argv(int argc, char **argv)
{
	argc_bk = argc;
	argv_bk = (char **)xnmalloc((size_t)argc + 1, sizeof(char *));

	register int i = argc;
	while (--i >= 0)
		argv_bk[i] = savestring(argv[i], strlen(argv[i]));
	argv_bk[argc] = (char *)NULL;

	return EXIT_SUCCESS;
}

int
init_workspaces(void)
{
	ws = (struct ws_t *)xnmalloc(MAX_WS, sizeof(struct ws_t));
	int i = MAX_WS;
	while (--i >= 0)
		ws[i].path = (char *)NULL;

	return EXIT_SUCCESS;
}

int
get_home(void)
{
	if (access(user.home, W_OK) == -1) {
		/* If no user's home, or if it's not writable, there won't be
		 * any config nor trash directory. These flags are used to
		 * prevent functions from trying to access any of these
		 * directories */
		home_ok = 0;
		config_ok = 0;
#ifndef _NO_TRASH
		trash_ok = 0;
#endif
		/* Print message: trash, bookmarks, command logs, commands
		 * history and program messages won't be stored */
		_err('e', PRINT_PROMPT, _("%s: Cannot access the home directory. "
				  "Trash, bookmarks, commands logs, and commands history are "
				  "disabled. Program messages and selected files won't be "
				  "persistent. Using default options\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	user_home_len = strlen(user.home);
	return EXIT_SUCCESS;
}

int
init_history(void)
{
	/* Limit the log files size */
	check_file_size(log_file, max_log);
	check_file_size(msg_log_file, max_log);

	/* Get history */
	struct stat attr;
	if (stat(hist_file, &attr) == 0 && attr.st_size != 0) {
		/* If the size condition is not included, and in case of a zero
		 * size file, read_history() produces malloc errors */
		/* Recover history from the history file */
		read_history(hist_file); /* This line adds more leaks to readline */
		/* Limit the size of the history file to max_hist lines */
		history_truncate_file(hist_file, max_hist);
	} else {
	/* If the history file doesn't exist, create it */
		FILE *hist_fp = fopen(hist_file, "w+");
		if (!hist_fp) {
			_err('w', PRINT_PROMPT, "%s: fopen: '%s': %s\n",
			    PROGRAM_NAME, hist_file, strerror(errno));
		} else {
			/* To avoid malloc errors in read_history(), do not
			 * create an empty file */
			fputs("edit\n", hist_fp);
			/* There is no need to run read_history() here, since
			 * the history file is still empty */
			fclose(hist_fp);
		}
	}

	return EXIT_SUCCESS;
}

int
set_start_path(void)
{
	/* Last path is overriden by positional parameters in the command line */
	if (restore_last_path)
		get_last_path();

	if (cur_ws == UNSET)
		cur_ws = DEF_CUR_WS;

	if (cur_ws > MAX_WS - 1) {
		cur_ws = DEF_CUR_WS;
		_err('w', PRINT_PROMPT, _("%s: %zu: Invalid workspace."
			"\nFalling back to workspace %zu\n"), PROGRAM_NAME,
		    cur_ws, cur_ws + 1);
	}

	/* If path was not set (neither in the config file nor via command
	 * line nor via the RestoreLastPath option), set the default (CWD),
	 * and if CWD is not set, use the user's home directory, and if the
	 * home cannot be found either, try the root directory, and if
	 * there's no access to the root dir either, exit.
	 * Bear in mind that if you launch CliFM through a terminal emulator,
	 * say xterm (xterm -e clifm), xterm will run a shell, say bash, and
	 * the shell will read its config file. Now, if this config file
	 * changes the CWD, this will be the CWD for CliFM */
	if (!ws[cur_ws].path) {
		char cwd[PATH_MAX] = "";
		if (getcwd(cwd, sizeof(cwd)) == NULL) {}

		if (!*cwd || strlen(cwd) == 0) {
			if (user_home) {
				ws[cur_ws].path = savestring(user_home, strlen(user_home));
			} else {
				if (access("/", R_OK | X_OK) == -1) {
					fprintf(stderr, "%s: /: %s\n", PROGRAM_NAME,
					    strerror(errno));
					exit(EXIT_FAILURE);
				} else {
					ws[cur_ws].path = savestring("/", 1);
				}
			}
		} else {
			ws[cur_ws].path = savestring(cwd, strlen(cwd));
		}
	}

	/* Make path the CWD */
	/* If chdir(path) fails, set path to cwd, list files and print the
	 * error message. If no access to CWD either, exit */
	if (xchdir(ws[cur_ws].path, NO_TITLE) == -1) {
		_err('e', PRINT_PROMPT, "%s: chdir: '%s': %s\n", PROGRAM_NAME,
		    ws[cur_ws].path, strerror(errno));

		char cwd[PATH_MAX] = "";
		if (getcwd(cwd, sizeof(cwd)) == NULL) {
			_err(0, NOPRINT_PROMPT, _("%s: Fatal error! Failed "
					"retrieving current working directory\n"), PROGRAM_NAME);
			exit(EXIT_FAILURE);
		}

		if (ws[cur_ws].path)
			free(ws[cur_ws].path);
		ws[cur_ws].path = savestring(cwd, strlen(cwd));
	}

	return EXIT_SUCCESS;
}

/* Get the system data directory (usually /usr/share) */
void
get_data_dir(void)
{
	/* First try standard values for DATADIR */
	char *data_dirs[] = {
		"/usr/share",
		"/usr/local/share",
#if defined(__HAIKU__)
		"/boot/system/non-packaged/data",
		"/boot/system/data",
#endif
		NULL };

	struct stat attr;
	size_t i;

	for (i = 0; data_dirs[i]; i++) {
		char tmp[PATH_MAX];
		snprintf(tmp, PATH_MAX - 1, "%s/%s", data_dirs[i], PNL);
		if (stat(tmp, &attr) == EXIT_SUCCESS) {
			data_dir = (char *)xrealloc(data_dir, (strlen(data_dirs[i]) + 1)
										* sizeof(char));
			strcpy(data_dir, data_dirs[i]);
			break;
		}
	}

/*	if (data_dir)
		return; */
	return;

	/* If not found, try to get DATADIR from executable's path */
/*	data_dir = get_cmd_path(PNL);

	if (!data_dir)
		return;

	size_t j = strlen(data_dir),
		   count = 0;

	while (--j >= 0) {
		if (data_dir[j] == '/')
			count++;
		if (count == 2) {
			data_dir[j] = '\0';
			break;
		}
	}

	char tmp[PATH_MAX];
	snprintf(tmp, PATH_MAX - 1, "%s/share/%s", data_dir, PNL);
	if (stat(tmp, &attr) == EXIT_SUCCESS) {
		snprintf(tmp, PATH_MAX - 1, "%s/share", data_dir);
		data_dir = (char *)xrealloc(data_dir, (strlen(tmp) + 1) * sizeof(char));
		strcpy(data_dir, tmp);
		return;
	} */
}

void
check_env_filter(void)
{
	if (filter)
		return;

	char *p = getenv("CLIFM_FILTER");
	if (!p)
		return;

	if (*p == '!') {
		filter_rev = 1;
		p++;
	} else {
		filter_rev = 0;
	}
	
	filter = savestring(p, strlen(p));
}

char *
get_date(void)
{
	time_t rawtime = time(NULL);
	struct tm tm;
	localtime_r(&rawtime, &tm);
	size_t date_max = 128;

	char *p = (char *)malloc((date_max + 1) * sizeof(char)), *date;
	if (p) {
		date = p;
		p = (char *)NULL;
	} else {
		return (char *)NULL;
	}

	strftime(date, date_max, "%Y-%m-%dT%T%z", &tm);
	return date;
}

static pid_t
get_own_pid(void)
{
	pid_t pid;

	/* Get the process id */
	pid = getpid();

	if (pid < 0)
		return 0;
	return pid;
}

/* Returns pointer to user data struct, exits if not found */
struct user_t
get_user(void)
{
	struct passwd *pw;
	struct user_t tmp_user;

	pw = getpwuid(geteuid());
	if (!pw) {
		_err('e', NOPRINT_PROMPT, _("%s: Cannot detect user data. Exiting early"),
			PROGRAM_NAME);
		exit(-1);
	}

	tmp_user.uid = pw->pw_uid;
	tmp_user.gid = pw->pw_gid;
	char *p = getenv("HOME");
	if (!p)
		tmp_user.home = savestring(pw->pw_dir, strlen(pw->pw_dir));
	else
		tmp_user.home = savestring(p, strlen(p));
	tmp_user.name = savestring(pw->pw_name, strlen(pw->pw_name));
	tmp_user.shell = savestring(pw->pw_shell, strlen(pw->pw_shell));

	if (!tmp_user.home || !tmp_user.name || !tmp_user.shell) {
		_err('e', NOPRINT_PROMPT, _("%s: Cannot detect user data. Exiting"),
			PROGRAM_NAME);
		exit(-1);
	}

	tmp_user.home_len = strlen(tmp_user.home);
	return tmp_user;
}

/* Reconstruct the jump database from database file */
void
load_jumpdb(void)
{
	if (xargs.no_dirjump == 1 || !config_ok || !config_dir)
		return;

	size_t dir_len = strlen(config_dir);
	char *jump_file = (char *)xnmalloc(dir_len + 10, sizeof(char));
	snprintf(jump_file, dir_len + 10, "%s/jump.cfm", config_dir);

	int fd;
	FILE *fp = open_fstream_r(jump_file, &fd);
	if (!fp) {
		free(jump_file);
		return;
	}

	char tmp_line[PATH_MAX];
	size_t jump_lines = 0;

	while (fgets(tmp_line, (int)sizeof(tmp_line), fp)) {
		if (*tmp_line != '\n' && *tmp_line >= '0' && *tmp_line <= '9')
			jump_lines++;
	}

	if (!jump_lines) {
		free(jump_file);
		close_fstream(fp, fd);
		return;
	}

	jump_db = (struct jump_t *)xnmalloc(jump_lines + 2, sizeof(struct jump_t));

	fseek(fp, 0L, SEEK_SET);

	size_t line_size = 0;
	char *line = (char *)NULL;
	ssize_t line_len = 0;

	while ((line_len = getline(&line, &line_size, fp)) > 0) {
		if (!*line || *line == '\n' || *line == '#')
			continue;
		if (*line == '@') {
			if (line[line_len - 1] == '\n')
				line[line_len - 1] = '\0';
			if (is_number(line + 1))
				jump_total_rank = atoi(line + 1);
			continue;
		}
		if (*line < '0' || *line > '9')
			continue;

		if (line[line_len - 1] == '\n')
			line[line_len - 1] = '\0';

		char *tmp = strchr(line, ':');
		if (!tmp)
			continue;

		*tmp = '\0';
		if (!*(++tmp))
			continue;

		int visits = 1;

		if (is_number(line))
			visits = atoi(line);

		char *tmpb = strchr(tmp, ':');
		if (!tmpb)
			continue;

		*tmpb = '\0';

		if (!*(++tmpb))
			continue;

		time_t first = 0;

		if (is_number(tmp))
			first = (time_t)atoi(tmp);

		char *tmpc = strchr(tmpb, ':');
		if (!tmpc)
			continue;

		*tmpc = '\0';

		if (!*(++tmpc))
			continue;

		/* Purge the database from non-existent directories */
		if (access(tmpc, F_OK) == -1)
			continue;

		jump_db[jump_n].visits = (size_t)visits;
		jump_db[jump_n].first_visit = first;

		if (is_number(tmpb))
			jump_db[jump_n].last_visit = (time_t)atoi(tmpb);
		else
			jump_db[jump_n].last_visit = 0; /* UNIX Epoch */

		jump_db[jump_n].keep = 0;
		jump_db[jump_n].rank = 0;
		jump_db[jump_n++].path = savestring(tmpc, strlen(tmpc));
	}

	close_fstream(fp, fd);
	free(line);
	free(jump_file);

	if (!jump_n) {
		free(jump_db);
		jump_db = (struct jump_t *)NULL;
		return;
	}

	jump_db[jump_n].path = (char *)NULL;
	jump_db[jump_n].rank = 0;
	jump_db[jump_n].keep = 0;
	jump_db[jump_n].visits = 0;
	jump_db[jump_n].first_visit = -1;
}

int
load_bookmarks(void)
{
	if (create_bm_file() == EXIT_FAILURE)
		return EXIT_FAILURE;

	if (!bm_file)
		return EXIT_FAILURE;

	int fd;
	FILE *fp = open_fstream_r(bm_file, &fd);
	if (!fp)
		return EXIT_FAILURE;

	size_t bm_total = 0;
	char tmp_line[256];
	while (fgets(tmp_line, (int)sizeof(tmp_line), fp)) {
		if (!*tmp_line || *tmp_line == '#' || *tmp_line == '\n')
			continue;
		bm_total++;
	}

	if (!bm_total) {
		close_fstream(fp, fd);
		return EXIT_SUCCESS;
	}

	fseek(fp, 0L, SEEK_SET);

	bookmarks = (struct bookmarks_t *)xnmalloc(bm_total + 1,
	    sizeof(struct bookmarks_t));
	size_t line_size = 0;
	char *line = (char *)NULL;
	ssize_t line_len = 0;

	while ((line_len = getline(&line, &line_size, fp)) > 0) {
		if (!*line || *line == '\n' || *line == '#')
			continue;
		if (line[line_len - 1] == '\n')
			line[line_len - 1] = '\0';

		/* Neither hotkey nor name, but only a path */
		if (*line == '/') {
			bookmarks[bm_n].shortcut = (char *)NULL;
			bookmarks[bm_n].name = (char *)NULL;
			bookmarks[bm_n++].path = savestring(line, strlen(line));
			continue;
		}

		if (*line == '[') {
			char *p = line;
			p++;
			char *tmp = strchr(line, ']');
			if (!tmp) {
				bookmarks[bm_n].shortcut = (char *)NULL;
				bookmarks[bm_n].name = (char *)NULL;
				bookmarks[bm_n++].path = (char *)NULL;
				continue;
			}

			*tmp = '\0';

			bookmarks[bm_n].shortcut = savestring(p, strlen(p));

			tmp++;
			p = tmp;
			tmp = strchr(p, ':');

			if (!tmp) {
				bookmarks[bm_n].name = (char *)NULL;
				if (*p)
					bookmarks[bm_n++].path = savestring(p, strlen(p));
				else
					bookmarks[bm_n++].path = (char *)NULL;
				continue;
			}

			*tmp = '\0';
			bookmarks[bm_n].name = savestring(p, strlen(p));

			if (!*(++tmp)) {
				bookmarks[bm_n++].path = (char *)NULL;
				continue;
			}

			bookmarks[bm_n++].path = savestring(tmp, strlen(tmp));
			continue;
		}

		/* No shortcut. Let's try with name */
		bookmarks[bm_n].shortcut = (char *)NULL;
		char *tmp = strchr(line, ':');

		/* No name either */
		if (!tmp) {
			bookmarks[bm_n].name = (char *)NULL;
			bookmarks[bm_n++].path = (char *)NULL;
			continue;
		}

		*tmp = '\0';
		bookmarks[bm_n].name = savestring(line, strlen(line));

		if (!*(++tmp)) {
			bookmarks[bm_n++].path = (char *)NULL;
			continue;
		} else {
			bookmarks[bm_n++].path = savestring(tmp, strlen(tmp));
		}
	}

	free(line);
	close_fstream(fp, fd);

	if (!bm_n) {
		free(bookmarks);
		bookmarks = (struct bookmarks_t *)NULL;
		return EXIT_SUCCESS;
	}

	/* bookmark_names array shouldn't exist: is only used for bookmark
	 * completion. xbookmarks[i].name should be used instead, but is
	 * currently not working */

	size_t i, j = 0;
	bookmark_names = (char **)xnmalloc(bm_n + 2, sizeof(char *));

	for (i = 0; i < bm_n; i++) {
		if (!bookmarks[i].name || !*bookmarks[i].name)
			continue;
		bookmark_names[j++] = savestring(bookmarks[i].name,
		    strlen(bookmarks[i].name));
	}

	bookmark_names[j] = (char *)NULL;
	return EXIT_SUCCESS;
}

/* Store actions from the actions file into a struct */
int
load_actions(void)
{
	if (!config_ok)
		return EXIT_FAILURE;

	/* Free the actions struct array */
	if (actions_n) {
		int i = (int)actions_n;
		while (--i >= 0) {
			free(usr_actions[i].name);
			free(usr_actions[i].value);
		}

		free(usr_actions);
		usr_actions = (struct actions_t *)xnmalloc(1, sizeof(struct actions_t));
		actions_n = 0;
	}

	/* Open the actions file */
	int fd;
	FILE *fp = open_fstream_r(actions_file, &fd);
	if (!fp)
		return EXIT_FAILURE;

	size_t line_size = 0;
	char *line = (char *)NULL;
	ssize_t line_len = 0;

	while ((line_len = getline(&line, &line_size, fp)) > 0) {
		if (!line || !*line || *line == '#' || *line == '\n')
			continue;
		if (line[line_len - 1] == '\n')
			line[line_len - 1] = '\0';

		char *tmp = (char *)NULL;
		tmp = strrchr(line, '=');
		if (!tmp)
			continue;

		/* Now copy left and right value of each action into the
		 * actions struct */
		usr_actions = xrealloc(usr_actions, (size_t)(actions_n + 1)
								* sizeof(struct actions_t));
		usr_actions[actions_n].value = savestring(tmp + 1, strlen(tmp + 1));
		*tmp = '\0';
		usr_actions[actions_n++].name = savestring(line, strlen(line));
	}

	free(line);
	close_fstream(fp, fd);
	return EXIT_SUCCESS;
}

static inline void
reset_remotes_values(const size_t i)
{
	remotes[i].name = (char *)NULL;
	remotes[i].desc = (char *)NULL;
	remotes[i].mountpoint = (char *)NULL;
	remotes[i].mount_cmd = (char *)NULL;
	remotes[i].unmount_cmd = (char *)NULL;
	remotes[i].auto_unmount = 0;
	remotes[i].auto_mount = 0;
	remotes[i].mounted = 0;
}

/* Load remotes information from FILE */
int
load_remotes(void)
{
	if (!remotes_file || !*remotes_file)
		return EXIT_FAILURE;

	int fd;
	FILE *fp = open_fstream_r(remotes_file, &fd);
	if (!fp) {
		fprintf(stderr, "%s: %s\n", remotes_file, strerror(errno));
		return EXIT_FAILURE;
	}

	size_t n = 0;
	remotes = (struct remote_t *)xnmalloc(n + 1, sizeof(struct remote_t));
	reset_remotes_values(n);

	size_t line_sz = 0;
	char *line = (char *)NULL;

	while (getline(&line, &line_sz, fp) > 0) {
		if (!*line || *line == '#' || *line == '\n')
			continue;
		if (*line == '[') {
			if (remotes[n].name)
				n++;
			remotes = (struct remote_t *)xrealloc(
					remotes, (n + 2) * sizeof(struct remote_t));
			reset_remotes_values(n);

			char *name = strbtw(line, '[', ']');
			if (!name)
				continue;
			if (!*name) {
				free(name);
				name = (char *)NULL;
				continue;
			}
			remotes[n].name = (char *)xrealloc(remotes[n].name,
							(strlen(name) + 1) * sizeof(char));
			strcpy(remotes[n].name, name);
			free(name);
			name = (char *)NULL;
		}

		if (!remotes[n].name)
			continue;

		char *ret = strchr(line, '=');
		if (!ret)
			continue;
		if (!*(++ret))
			continue;

		size_t ret_len = strlen(ret);
		if (ret[ret_len - 1] == '\n')
			ret[--ret_len] = '\0';

		char *deq_str = remove_quotes(ret);
		if (deq_str)
			ret = deq_str;

		if (strncmp(line, "Comment=", 8) == 0) {
			remotes[n].desc = (char *)xrealloc(remotes[n].desc,
							(ret_len + 1) * sizeof(char));
			strcpy(remotes[n].desc, ret);
		} else if (strncmp(line, "Mountpoint=", 11) == 0) {
			char *tmp = (char *)NULL; 
			if (*ret == '~')
				tmp = tilde_expand(ret);
			remotes[n].mountpoint = (char *)xrealloc(remotes[n].mountpoint,
								((tmp ? strlen(tmp) : ret_len) + 1)
								* sizeof(char));
			strcpy(remotes[n].mountpoint, tmp ? tmp : ret);
			free(tmp);
			if (count_dir(remotes[n].mountpoint, CPOP) > 2)
				remotes[n].mounted = 1;
		} else if (strncmp(line, "MountCmd=", 9) == 0) {
			int replaced = 0;
			if (remotes[n].mountpoint) {
				char *rep = replace_substr(ret, "%m", remotes[n].mountpoint);
				if (rep) {
					remotes[n].mount_cmd = (char *)xrealloc(
								remotes[n].mount_cmd,
								(strlen(rep) + 1) * sizeof(char));
					strcpy(remotes[n].mount_cmd, rep);
					free(rep);
					replaced = 1;
				}
			}

			if (!replaced) {
				remotes[n].mount_cmd = (char *)xrealloc(remotes[n].mount_cmd,
									(ret_len + 1) * sizeof(char));
				strcpy(remotes[n].mount_cmd, ret);
			}
		} else if (strncmp(line, "UnmountCmd=", 11) == 0) {
			int replaced = 0;
			if (remotes[n].mountpoint) {
				char *rep = replace_substr(ret, "%m", remotes[n].mountpoint);
				if (rep) {
					remotes[n].unmount_cmd = (char *)xrealloc(
							remotes[n].unmount_cmd,
							(strlen(rep) + 1) * sizeof(char));
					strcpy(remotes[n].unmount_cmd, rep);
					free(rep);
					replaced = 1;
				}
			}

			if (!replaced) {
				remotes[n].mount_cmd = (char *)xrealloc(remotes[n].unmount_cmd,
								(ret_len + 1) * sizeof(char));
				strcpy(remotes[n].unmount_cmd, ret);
			}
		} else if (strncmp(line, "AutoUnmount=", 12) == 0) {
			if (strcmp(ret, "true") == 0)
				remotes[n].auto_unmount = 1;
		} else if (strncmp(line, "AutoMount=", 10) == 0) {
			if (strcmp(ret, "true") == 0)
				remotes[n].auto_mount = 1;
		}
	}

	free(line);
	close_fstream(fp, fd);

	if (remotes[n].name) {
		++n;
		remotes[n].name = (char *)NULL;
	}

	remotes_n = n;
	return EXIT_SUCCESS;
}

static void
open_reg_exit(char *filename)
{
	char *homedir = getenv("HOME");
	if (!homedir) {
		fprintf(stderr, "%s: Could not retrieve the home directory\n",
				PROGRAM_NAME);
		exit(EXIT_FAILURE);
	}
	tmp_dir = savestring(P_tmpdir, P_tmpdir_len);
	size_t mime_file_len = strlen(homedir) + (alt_profile
					? strlen(alt_profile) : 7) + 38;
	mime_file = (char *)xnmalloc(mime_file_len, sizeof(char));
	sprintf(mime_file, "%s/.config/clifm/profiles/%s/mimelist.cfm",
			homedir, alt_profile ? alt_profile : "default");
	int ret = open_file(filename);
	exit(ret);
}

/* Evaluate external arguments, if any, and change initial variables to
 * its corresponding value */
void
external_arguments(int argc, char **argv)
{
	/* Disable automatic error messages to be able to handle them
	 * myself via the '?' case in the switch */
	opterr = optind = 0;

	/* Link long (--option) and short options (-o) for the getopt_long
	 * function */
	static struct option longopts[] = {
	    {"no-hidden", no_argument, 0, 'a'},
	    {"show-hidden", no_argument, 0, 'A'},
	    {"bookmarks-file", no_argument, 0, 'b'},
	    {"config-file", no_argument, 0, 'c'},
	    {"config-dir", required_argument, 0, 'D'},
	    {"no-eln", no_argument, 0, 'e'},
	    {"no-folders-first", no_argument, 0, 'f'},
	    {"folders-first", no_argument, 0, 'F'},
	    {"pager", no_argument, 0, 'g'},
	    {"no-pager", no_argument, 0, 'G'},
	    {"help", no_argument, 0, 'h'},
	    {"no-case-sensitive", no_argument, 0, 'i'},
	    {"case-sensitive", no_argument, 0, 'I'},
	    {"keybindings-file", no_argument, 0, 'k'},
	    {"no-long-view", no_argument, 0, 'l'},
	    {"long-view", no_argument, 0, 'L'},
	    {"dirhist-map", no_argument, 0, 'm'},
	    {"no-list-on-the-fly", no_argument, 0, 'o'},
	    {"list-on-the-fly", no_argument, 0, 'O'},
	    {"path", required_argument, 0, 'p'},
	    {"profile", required_argument, 0, 'P'},
	    {"splash", no_argument, 0, 's'},
	    {"stealth-mode", no_argument, 0, 'S'},
	    {"unicode", no_argument, 0, 'U'},
	    {"no-unicode", no_argument, 0, 'u'},
	    {"version", no_argument, 0, 'v'},
	    {"workspace", required_argument, 0, 'w'},
	    {"no-ext-cmds", no_argument, 0, 'x'},
	    {"light-mode", no_argument, 0, 'y'},
	    {"sort", required_argument, 0, 'z'},

	    /* Only long options */
	    {"no-cd-auto", no_argument, 0, 0},
	    {"no-open-auto", no_argument, 0, 1},
	    {"no-restore-last-path", no_argument, 0, 2},
	    {"no-tips", no_argument, 0, 3},
	    {"disk-usage", no_argument, 0, 4},
	    {"no-classify", no_argument, 0, 6},
	    {"share-selbox", no_argument, 0, 7},
	    {"rl-vi-mode", no_argument, 0, 8},
	    {"max-dirhist", required_argument, 0, 9},
	    {"sort-reverse", no_argument, 0, 10},
	    {"no-files-counter", no_argument, 0, 11},
	    {"no-welcome-message", no_argument, 0, 12},
	    {"no-clear-screen", no_argument, 0, 13},
	    {"enable-logs", no_argument, 0, 15},
	    {"max-path", required_argument, 0, 16},
	    {"opener", required_argument, 0, 17},
	    {"expand-bookmarks", no_argument, 0, 18},
	    {"only-dirs", no_argument, 0, 19},
	    {"list-and-quit", no_argument, 0, 20},
	    {"color-scheme", required_argument, 0, 21},
	    {"cd-on-quit", no_argument, 0, 22},
	    {"no-dir-jumper", no_argument, 0, 23},
	    {"icons", no_argument, 0, 24},
	    {"icons-use-file-color", no_argument, 0, 25},
	    {"no-columns", no_argument, 0, 26},
	    {"no-colors", no_argument, 0, 27},
	    {"max-files", required_argument, 0, 28},
	    {"trash-as-rm", no_argument, 0, 29},
	    {"case-sens-dirjump", no_argument, 0, 30},
	    {"case-sens-path-comp", no_argument, 0, 31},
	    {"cwd-in-title", no_argument, 0, 32},
	    {"open", required_argument, 0, 33},
	    {"print-sel", no_argument, 0, 34},
	    {"no-suggestions", no_argument, 0, 35},
	    {"autojump", no_argument, 0, 36},
	    {"highlight", no_argument, 0, 37},
	    {"no-file-cap", no_argument, 0, 38},
	    {"no-file-ext", no_argument, 0, 39},
	    {"no-follow-symlink", no_argument, 0, 40},
		{"control-d-exits", no_argument, 0, 41},
		{"int-vars", no_argument, 0, 42},
		{"fzftab", no_argument, 0, 43},
		{"warn-wrong-cmd", no_argument, 0, 44},
	    {0, 0, 0, 0}};

	/* Increment whenever a new (only) long option is added */
	int long_opts = 33;
	int optc;
	/* Variables to store arguments to options (-c, -p and -P) */
	char *path_value = (char *)NULL,
		 *alt_profile_value = (char *)NULL,
	     *alt_dir_value = (char *)NULL,
	     *config_value = (char *)NULL,
	     *kbinds_value = (char *)NULL,
	     *bm_value = (char *)NULL;

	while ((optc = getopt_long(argc, argv,
		    "+aAb:c:D:efFgGhiIk:lLmoOp:P:sSUuvw:xyz:", longopts,
		    (int *)0)) != EOF) {
		/* ':' and '::' in the short options string means 'required' and
		 * 'optional argument' respectivelly. Thus, 'p' and 'P' require
		 * an argument here. The plus char (+) tells getopt to stop
		 * processing at the first non-option (and non-argument) */
		switch (optc) {

		case 0: xargs.autocd = autocd = 0; break;
		case 1:	xargs.auto_open = auto_open = 0; break;
		case 2:	xargs.restore_last_path = restore_last_path = 0; break;
		case 3: xargs.tips = tips = 0; break;
		case 4:	xargs.disk_usage = disk_usage = 1; break;

		case 6:	xargs.classify = classify = 0; break;
		case 7:	xargs.share_selbox = share_selbox = 1; break;
		case 8:	xargs.rl_vi_mode = 1; break;

		case 9: {
			if (!is_number(optarg))
				break;
			int opt_int = atoi(optarg);
			if (opt_int >= 0 && opt_int <= INT_MAX)
				xargs.max_dirhist = max_dirhist = opt_int;
		} break;

		case 10: xargs.sort_reverse = sort_reverse = 1; break;
		case 11: xargs.files_counter = files_counter = 0; break;
		case 12: xargs.welcome_message = welcome_message = 0; break;
		case 13: xargs.clear_screen = clear_screen = 0; break;

		case 15: xargs.logs = logs_enabled = 1;	break;

		case 16: {
			if (!is_number(optarg))
				break;
			int opt_int = atoi(optarg);
			if (opt_int >= 0 && opt_int <= INT_MAX)
				xargs.max_path = max_path = opt_int;
		} break;

		case 17: opener = savestring(optarg, strlen(optarg)); break;
		case 18: xargs.expand_bookmarks = expand_bookmarks = 1; break;
		case 19: xargs.only_dirs = only_dirs = 1; break;
		case 20: xargs.list_and_quit = 1; break;
		case 21: usr_cscheme = savestring(optarg, strlen(optarg)); break;
		case 22: xargs.cd_on_quit = cd_on_quit = 1; break;
		case 23: xargs.no_dirjump = 1; break;
#ifndef _NO_ICONS
		case 24: xargs.icons = icons = 1; break;
		case 25:
			xargs.icons = icons = 1;
			xargs.icons_use_file_color = 1;
			break;
#else
		case 24: /* fallthrough */
		case 25:
			fprintf(stderr, _("%s: icons: %s\n"), PROGRAM_NAME, _(NOT_AVAILABLE));
			exit(EXIT_FAILURE);
#endif
		case 26:
			xargs.columns = 0;
			columned = 0;
			break;

		case 27:
			xargs.colorize = 0;
			colorize = 0;
#ifndef _NO_HIGHLIGHT
			xargs.highlight = highlight = 0;
#endif
			break;

		case 28:
			if (!is_number(optarg))
				break;
			int opt_int = atoi(optarg);
			if (opt_int >= 0 && opt_int <= INT_MAX)
				xargs.max_files = max_files = opt_int;
			break;

		case 29:
#ifndef _NO_TRASH
			xargs.trasrm = tr_as_rm = 1; break;
#else
		fprintf(stderr, _("%s: trash: %s\n"), PROGRAM_NAME, _(NOT_AVAILABLE));
			exit(EXIT_FAILURE);
#endif
		case 30: xargs.case_sens_dirjump = case_sens_dirjump = 1; break;
		case 31: xargs.case_sens_path_comp = case_sens_path_comp = 1; break;
		case 32: xargs.cwd_in_title = 1; break;

		case 33: {
			struct stat attr;
			if (stat(optarg, &attr) == -1) {
				fprintf(stderr, "%s: %s: %s", PROGRAM_NAME, optarg,
				    strerror(errno));
				exit(EXIT_FAILURE);
			}

			if ((attr.st_mode & S_IFMT) != S_IFDIR) {
				open_reg_exit(optarg);
			} else {
				printf(_("%s: %s: Is a directory\n"), PROGRAM_NAME, optarg);
				exit(EXIT_FAILURE);
			}

			/*			flags |= START_PATH;
			path_value = optarg;
			xargs.path = 1; */
		} break;

		case 34: xargs.printsel = 1; break;
#ifndef _NO_SUGGESTIONS
		case 35: xargs.suggestions = suggestions = 0; break;
#endif
		case 36: xargs.autojump = autojump = 0; break;
#ifndef _NO_HIGHLIGHT
		case 37: xargs.highlight = highlight = 1; break;
#else
		case 37:
			fprintf(stderr, _("%s: highlight: %s\n"), PROGRAM_NAME, _(NOT_AVAILABLE));
				exit(EXIT_FAILURE);
#endif /* !_NO_HIGHLIGHT */
		case 38: xargs.check_cap = check_cap = 0; break;
		case 39: xargs.check_ext = check_ext = 0; break;
		case 40: xargs.follow_symlinks = follow_symlinks = 0; break;
		case 41: xargs.control_d_exits = control_d_exits = 1; break;
		case 42: xargs.int_vars = int_vars = 1; break;
#ifndef _NO_FZF
		case 43: {
			char *p = get_cmd_path("fzf");
			if (p) {
				xargs.fzftab = 1;
				free(p);
			} else {
				_err('w', PRINT_PROMPT, _("%s: FZF not found. Falling back "
					"to standard TAB completion\n"), PROGRAM_NAME);
			}
			}
			break;
#else
		case 43:
			fprintf(stderr, _("%s: fzftab: %s\n"), PROGRAM_NAME, _(NOT_AVAILABLE));
				exit(EXIT_FAILURE);
#endif /* !_NO_FZF */
		case 44:
			xargs.warn_wrong_cmd = warn_wrong_cmd = 1; break;

		case 'a':
			flags &= ~HIDDEN; /* Remove HIDDEN from 'flags' */
			show_hidden = xargs.hidden = 0;
			break;

		case 'A':
			flags |= HIDDEN; /* Add HIDDEN to 'flags' */
			show_hidden = xargs.hidden = 1;
			break;

		case 'b':
			xargs.bm_file = 1;
			bm_value = optarg;
			break;

		case 'c':
			xargs.config = 1;
			config_value = optarg;
			break;

		case 'D': alt_dir_value = optarg; break;
		case 'e': xargs.noeln = no_eln = 1;	break;

		case 'f':
			flags &= ~FOLDERS_FIRST;
			list_folders_first = xargs.ffirst = 0;
			break;

		case 'F':
			flags |= FOLDERS_FIRST;
			list_folders_first = xargs.ffirst = 1;
			break;

		case 'g': pager = xargs.pager = 1; break;
		case 'G': pager = xargs.pager = 0; break;

		case 'h':
			flags |= HELP;
			/* Do not display "Press any key to continue" */
			flags |= EXT_HELP;
			help_function();
			exit(EXIT_SUCCESS);

		case 'i':
			flags &= ~CASE_SENS;
			case_sensitive = xargs.sensitive = 0;
			break;

		case 'I':
			flags |= CASE_SENS;
			case_sensitive = xargs.sensitive = 1;
			break;

		case 'k': kbinds_value = optarg; break;
		case 'l': long_view = xargs.longview = 0; break;
		case 'L': long_view = xargs.longview = 1; break;
		case 'm': dirhist_map = xargs.dirmap = 1; break;

		case 'o':
			flags &= ~ON_THE_FLY;
			cd_lists_on_the_fly = xargs.cd_list_auto = 0;
			break;

		case 'O':
			flags |= ON_THE_FLY;
			cd_lists_on_the_fly = xargs.cd_list_auto = 1;
			break;

		case 'p':
			flags |= START_PATH;
			path_value = optarg;
			xargs.path = 1;
			break;

		case 'P':
			flags |= ALT_PROFILE;
			alt_profile_value = optarg;
			break;

		case 's':
			flags |= SPLASH;
			splash_screen = xargs.splash = 1;
			break;

		case 'S': xargs.stealth_mode = 1; break;
		case 'u': unicode = xargs.unicode = 0; break;
		case 'U': unicode = xargs.unicode = 1; break;

		case 'v':
			flags |= PRINT_VERSION;
			version_function();
			exit(EXIT_SUCCESS);

		case 'w': {
			if (!is_number(optarg))
				break;
			int iopt = atoi(optarg);

			if (iopt >= 0 && iopt <= MAX_WS)
				cur_ws = iopt - 1;
		} break;

		case 'x': ext_cmd_ok = xargs.ext = 0; break;
		case 'y': light_mode = xargs.light = 1; break;

		case 'z': {
			if (!is_number(optarg))
				break;
			int arg = atoi(optarg);
			if (arg < 0 || arg > SORT_TYPES)
				sort = 1;
			else
				sort = arg;
			xargs.sort = sort;
		} break;

		case '?': /* If some unrecognized option was found... */

			/* Options that requires an argument */
			/* Short options */
			switch (optopt) {
			case 'b': /* fallthrough */
			case 'c': /* fallthrough */
			case 'k': /* fallthrough */
			case 'p': /* fallthrough */
			case 'P': /* fallthrough */
			case 'w': /* fallthrough */
			case 'z':
				fprintf(stderr, _("%s: option requires an argument -- "
						  "'%c'\nTry '%s --help' for more information.\n"),
				    PROGRAM_NAME, optopt, PNL);
				exit(EXIT_FAILURE);
			}

			/* Long options */
			if (optopt >= 0 && optopt <= long_opts) {
				fprintf(stderr, _("%s: option requires an argument\nTry '%s "
					"--help' for more information.\n"), PROGRAM_NAME, PNL);
				exit(EXIT_FAILURE);
			}

			/* If unknown option is printable... */
			if (isprint(optopt)) {
				fprintf(stderr, _("%s: invalid option -- '%c'\nUsage: "
						  "%s %s\nTry '%s --help' for more information.\n"),
				    PROGRAM_NAME, optopt, GRAL_USAGE, PNL, PNL);
			} else {
				fprintf(stderr, _("%s: unknown option character '\\%x'\n"),
				    PROGRAM_NAME, (unsigned int)optopt);
			}

			exit(EXIT_FAILURE);

		default:
			break;
		}
	}

	/* Positional parameters. If a directory, use it as CliFM starting
	 * path. Otherwise, open the file with the associated application
	 * and exit */
	int i = optind;
	if (argv[i]) {
		struct stat attr;
		char *_exp_path = tilde_expand(argv[i]);
		if (_exp_path) {
			if (stat(_exp_path, &attr) == -1) {
				fprintf(stderr, "%s: %s: %s", PROGRAM_NAME, _exp_path,
					strerror(errno));
				free(_exp_path);
				exit(EXIT_FAILURE);
			}
			free(_exp_path);
		} else {
			fprintf(stderr, _("%s: Error expanding tilde\n"), PROGRAM_NAME);
			exit(EXIT_FAILURE);
		}

		if ((attr.st_mode & S_IFMT) != S_IFDIR)
			open_reg_exit(argv[i]);

		flags |= START_PATH;
		path_value = argv[i];
		xargs.path = 1;
	}

	if (bm_value) {
		char *bm_exp = (char *)NULL;

		if (*bm_value == '~') {
			bm_exp = tilde_expand(bm_value);
			bm_value = bm_exp;
		}

		if (access(bm_value, R_OK) == -1) {
			_err('e', PRINT_PROMPT, _("%s: %s: %s\n"
						  "Falling back to the default bookmarks file\n"),
			    PROGRAM_NAME, bm_value, strerror(errno));
		} else {
			alt_bm_file = savestring(bm_value, strlen(bm_value));
			_err('n', PRINT_PROMPT, _("%s: Loaded alternative "
						  "bookmarks file\n"), PROGRAM_NAME);
		}
	}

	if (alt_dir_value) {
		char *dir_exp = (char *)NULL;

		if (*alt_dir_value == '~') {
			dir_exp = tilde_expand(alt_dir_value);
			alt_dir_value = dir_exp;
		}

		int dir_ok = 1;
		struct stat attr;
		if (stat(alt_dir_value, &attr) == -1) {
			char *tmp_cmd[] = {"mkdir", "-p", alt_dir_value, NULL};
			int ret = launch_execve(tmp_cmd, FOREGROUND, E_NOSTDERR);
			if (ret != EXIT_SUCCESS) {
				_err('e', PRINT_PROMPT, _("%s: %s: Cannot create directory "
				"(error %d)\nFalling back to default configuration directory\n"),
					PROGRAM_NAME, alt_dir_value, ret);
				dir_ok = 0;
			}
		}

		if (access(alt_dir_value, W_OK) == -1) {
			if (dir_ok) {
				_err('e', PRINT_PROMPT, _("%s: %s: %s\n"
					"Falling back to default configuration directory\n"),
					PROGRAM_NAME, alt_dir_value, strerror(errno));
			}
		} else {
			alt_config_dir = savestring(alt_dir_value, strlen(alt_dir_value));
			_err(0, PRINT_PROMPT, _("%s: %s: Using alternative "
				"configuration directory\n"), PROGRAM_NAME, alt_config_dir);
		}

		if (dir_exp)
			free(dir_exp);
	}

	if (kbinds_value) {
		char *kbinds_exp = (char *)NULL;
		if (*kbinds_value == '~') {
			kbinds_exp = tilde_expand(kbinds_value);
			kbinds_value = kbinds_exp;
		}

		/*      if (alt_kbinds_file) {
			free(alt_kbinds_file);
			alt_kbinds_file = (char *)NULL;
		} */

		if (access(kbinds_value, R_OK) == -1) {
			_err('e', PRINT_PROMPT, _("%s: %s: %s\n"
						  "Falling back to the default keybindings file\n"),
			    PROGRAM_NAME, kbinds_value, strerror(errno));
			/*          xargs.config = -1; */
		} else {
			alt_kbinds_file = savestring(kbinds_value, strlen(kbinds_value));
			_err('n', PRINT_PROMPT, _("%s: Loaded alternative "
				"keybindings file\n"), PROGRAM_NAME);
		}

		if (kbinds_exp)
			free(kbinds_exp);
	}

	if (xargs.config && config_value) {
		char *config_exp = (char *)NULL;

		if (*config_value == '~') {
			config_exp = tilde_expand(config_value);
			config_value = config_exp;
		}

		/*      if (alt_config_file) {
			free(alt_config_file);
			alt_config_file = (char *)NULL;
		} */

		if (access(config_value, R_OK) == -1) {
			_err('e', PRINT_PROMPT, _("%s: %s: %s\n"
				"Falling back to default\n"), PROGRAM_NAME,
			    config_value, strerror(errno));
			xargs.config = -1;
		} else {
			alt_config_file = savestring(config_value, strlen(config_value));
			_err('n', PRINT_PROMPT, _("%s: Loaded alternative "
				"configuration file\n"), PROGRAM_NAME);
		}

		if (config_exp)
			free(config_exp);
	}

	if ((flags & START_PATH) && path_value) {
		char *path_exp = (char *)NULL;
		char path_tmp[PATH_MAX];

		if (*path_value == '~') {
			path_exp = tilde_expand(path_value);
			xstrsncpy(path_tmp, path_exp, PATH_MAX);
		} else if (*path_value != '/') {
			snprintf(path_tmp, PATH_MAX - 1, "%s/%s", getenv("PWD"), path_value);
		} else {
			xstrsncpy(path_tmp, path_value, PATH_MAX);
		}

		if (xchdir(path_tmp, SET_TITLE) == 0) {
			if (cur_ws == UNSET)
				cur_ws = DEF_CUR_WS;
			if (ws[cur_ws].path)
				free(ws[cur_ws].path);

			ws[cur_ws].path = savestring(path_tmp, strlen(path_tmp));
		} else { /* Error changing directory */
			if (xargs.list_and_quit == 1) {
				fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME,
				    path_tmp, strerror(errno));
				exit(EXIT_FAILURE);
			}

			_err('w', PRINT_PROMPT, "%s: %s: %s\n", PROGRAM_NAME,
			    path_tmp, strerror(errno));
		}

		if (path_exp)
			free(path_exp);
	}

	if ((flags & ALT_PROFILE) && alt_profile_value) {
		if (alt_profile)
			free(alt_profile);
		alt_profile = savestring(alt_profile_value, strlen(alt_profile_value));
	}
}

void
unset_xargs(void)
{
	xargs.autojump = UNSET;
	xargs.auto_open = UNSET;
	xargs.autocd = UNSET;
	xargs.bm_file = UNSET;
	xargs.case_sens_dirjump = UNSET;
	xargs.case_sens_path_comp = UNSET;
	xargs.cd_list_auto = UNSET;
	xargs.check_cap = UNSET;
	xargs.check_ext = UNSET;
	xargs.cd_on_quit = UNSET;
	xargs.classify = UNSET;
	xargs.clear_screen = UNSET;
	xargs.color_scheme = UNSET;
	xargs.config = UNSET;
	xargs.control_d_exits = UNSET;
	xargs.cwd_in_title = UNSET;
	xargs.dirmap = UNSET;
	xargs.disk_usage = UNSET;
	xargs.expand_bookmarks = UNSET;
	xargs.ext = UNSET;
	xargs.ffirst = UNSET;
	xargs.files_counter = UNSET;
	xargs.follow_symlinks = UNSET;
#ifndef _NO_FZF
	xargs.fzftab = UNSET;
#endif
	xargs.hidden = UNSET;
#ifndef _NO_HIGHLIGHT
	xargs.highlight = UNSET;
#endif
#ifndef _NO_ICONS
	xargs.icons = UNSET;
	xargs.icons_use_file_color = UNSET;
#endif
	xargs.int_vars = UNSET;
	xargs.light = UNSET;
	xargs.list_and_quit = UNSET;
	xargs.logs = UNSET;
	xargs.longview = UNSET;
	xargs.max_dirhist = UNSET;
	xargs.max_path = UNSET;
	xargs.colorize = UNSET;
	xargs.columns = UNSET;
	xargs.no_dirjump = UNSET;
	xargs.noeln = UNSET;
	xargs.only_dirs = UNSET;
	xargs.path = UNSET;
	xargs.pager = UNSET;
	xargs.printsel = UNSET;
	xargs.restore_last_path = UNSET;
	xargs.rl_vi_mode = UNSET;
	xargs.sensitive = UNSET;
	xargs.share_selbox = UNSET;
	xargs.sort = UNSET;
	xargs.sort_reverse = UNSET;
	xargs.splash = UNSET;
	xargs.stealth_mode = UNSET;
#ifndef _NO_SUGGESTIONS
	xargs.suggestions = UNSET;
#endif
	xargs.tips = UNSET;
#ifndef _NO_TRASH
	xargs.trasrm = UNSET;
#endif
	xargs.unicode = UNSET;
	xargs.welcome_message = UNSET;
	xargs.warn_wrong_cmd = UNSET;
}

/* Keep track of attributes of the shell. Make sure the shell is running
 * interactively as the foreground job before proceeding.
 * Taken from:
 * https://www.gnu.org/software/libc/manual/html_node/Initializing-the-Shell.html#Initializing-the-Shell
 * */
void
init_shell(void)
{
	/* If shell is not interactive */
	if (!isatty(STDIN_FILENO)) {
		handle_stdin();
		return;
	}

	/* Loop until we are in the foreground */
	while (tcgetpgrp(STDIN_FILENO) != (own_pid = getpgrp()))
		kill(-own_pid, SIGTTIN);

	/* Ignore interactive and job-control signals */
	set_signals_to_ignore();
	/* Put ourselves in our own process group */
	own_pid = get_own_pid();

	if (flags & ROOT_USR) {
		/* Make the shell pgid (process group id) equal to its pid */
		/* Without the setpgid line below, the program cannot be run
		 * with sudo, but it can be run nonetheless by the root user */
		if (setpgid(own_pid, own_pid) < 0) {
			_err(0, NOPRINT_PROMPT, "%s: setpgid: %s\n", PROGRAM_NAME,
			    strerror(errno));
			exit(EXIT_FAILURE);
		}
	}

	/* Grab control of the terminal */
	tcsetpgrp(STDIN_FILENO, own_pid);
	/* Save default terminal attributes for shell */
	tcgetattr(STDIN_FILENO, &shell_tmodes);
	return;
}

/* Get current entries in the Selection Box, if any. */
int
get_sel_files(void)
{
	if (!selfile_ok || !config_ok || !sel_file)
		return EXIT_FAILURE;

	/* First, clear the sel array, in case it was already used */
	if (sel_n > 0) {
		int i = (int)sel_n;
		while (--i >= 0)
			free(sel_elements[i]);
	}

	/*  free(sel_elements); */

	sel_n = 0;

	/* Open the tmp sel file and load its contents into the sel array */
	int fd;
	FILE *fp = open_fstream_r(sel_file, &fd);
	/*  sel_elements = xcalloc(1, sizeof(char *)); */
	if (!fp)
		return EXIT_FAILURE;

	/* Since this file contains only paths, we can be sure no line
	 * length will be larger than PATH_MAX */
	char line[PATH_MAX] = "";
	while (fgets(line, (int)sizeof(line), fp)) {
		size_t len = strlen(line);

		if (line[len - 1] == '\n')
			line[len - 1] = '\0';

		if (!*line || *line == '#')
			continue;

		sel_elements = (char **)xrealloc(sel_elements, (sel_n + 1) * sizeof(char *));
		sel_elements[sel_n++] = savestring(line, len);
	}

	close_fstream(fp, fd);
	return EXIT_SUCCESS;
}

size_t
get_cdpath(void)
{
	char *p = getenv("CDPATH");
	if (!p || !*p)
		return 0;

	char *t = savestring(p, strlen(p));

	/* Get each path in CDPATH */
	size_t i, n = 0, len = 0;
	for (i = 0; t[i]; i++) {
		/* Store path in CDPATH in a tmp buffer */
		char buf[PATH_MAX];
		while (t[i] && t[i] != ':')
			buf[len++] = t[i++];
		buf[len] = '\0';

		/* Make room in cdpaths for a new path */
		cdpaths = (char **)xrealloc(cdpaths, (n + 2) * sizeof(char *));

		/* Dump the buffer into the global cdpaths array */
		cdpaths[n++] = savestring(buf, len);

		len = 0;
		if (!t[i])
			break;
	}

	cdpaths[n] = (char *)NULL;

	free(t);
	return n;
}

/* Store all paths in the PATH environment variable into a globally
 * declared array (paths) */
size_t
get_path_env(void)
{
	size_t i = 0;
	/* Get the value of the PATH env variable */
	char *path_tmp = (char *)NULL;
	char *ptr = getenv("PATH");
	if (!ptr || !*ptr)
		return 0;
	path_tmp = savestring(ptr, strlen(ptr));

	if (!path_tmp)
		return 0;

	/* Get each path in PATH */
	size_t path_num = 0, length = 0;
	for (i = 0; path_tmp[i]; i++) {
		/* Store path in PATH in a tmp buffer */
		char buf[PATH_MAX];
		while (path_tmp[i] && path_tmp[i] != ':')
			buf[length++] = path_tmp[i++];
		buf[length] = '\0';

		/* Make room in paths for a new path */
		paths = (char **)xrealloc(paths, (path_num + 1) * sizeof(char *));

		/* Dump the buffer into the global paths array */
		paths[path_num] = savestring(buf, length);

		path_num++;
		length = 0;
		if (!path_tmp[i])
			break;
	}

	free(path_tmp);
	return path_num;
}

/* Set PATH to last visited directory and CUR_WS to last used
 * workspace */
int
get_last_path(void)
{
	if (!config_dir)
		return EXIT_FAILURE;

	char *last_file = (char *)xnmalloc(strlen(config_dir) + 7, sizeof(char));
	sprintf(last_file, "%s/.last", config_dir);

	int fd;
	FILE *fp = open_fstream_r(last_file, &fd);
	if (!fp) {
		free(last_file);
		return EXIT_FAILURE;
	}

	/*  size_t i;
	for (i = 0; i < MAX_WS; i++) {

		if (ws[i].path) {
			free(ws[i].path);
			ws[i].path = (char *)NULL;
		}
	} */

	char line[PATH_MAX] = "";
	while (fgets(line, (int)sizeof(line), fp)) {
		char *p = line;
		if (!*p || !strchr(p, '/'))
			continue;
		if (!strchr(p, ':'))
			continue;

		size_t len = strlen(p);
		if (p[len - 1] == '\n')
			p[len - 1] = '\0';

		int cur = 0;
		if (*p == '*') {
			if (!*(++p))
				continue;
			cur = 1;
		}

		int ws_n = *p - '0';
		if (cur && cur_ws == UNSET)
			cur_ws = ws_n;

		if (ws_n >= 0 && ws_n < MAX_WS && !ws[ws_n].path)
			ws[ws_n].path = savestring(p + 2, strlen(p + 2));
	}

	close_fstream(fp, fd);
	free(last_file);
	return EXIT_SUCCESS;
}

/* Restore pinned dir from file */
int
load_pinned_dir(void)
{
	if (!config_ok)
		return EXIT_FAILURE;

	char *pin_file = (char *)xnmalloc(strlen(config_dir) + 6, sizeof(char));
	sprintf(pin_file, "%s/.pin", config_dir);

	int fd;
	FILE *fp = open_fstream_r(pin_file, &fd);
	if (!fp) {
		free(pin_file);
		return EXIT_FAILURE;
	}

	char line[PATH_MAX] = "";
	if (fgets(line, (int)sizeof(line), fp) == NULL) {
		free(pin_file);
		close_fstream(fp, fd);
		return EXIT_FAILURE;
	}

	if (!*line || !strchr(line, '/')) {
		free(pin_file);
		close_fstream(fp, fd);
		return EXIT_FAILURE;
	}

	if (pinned_dir) {
		free(pinned_dir);
		pinned_dir = (char *)NULL;
	}

	pinned_dir = savestring(line, strlen(line));
	close_fstream(fp, fd);
	free(pin_file);
	return EXIT_SUCCESS;
}

/* Get the list of files in PATH, plus CliFM internal commands, and send
 * them into an array to be read by my readline custom auto-complete
 * function (my_rl_completion) */
void
get_path_programs(void)
{
	int i, l = 0, total_cmd = 0;
	int *cmd_n = (int *)0;
	struct dirent ***commands_bin = (struct dirent ***)NULL;

	if (ext_cmd_ok) {
		char cwd[PATH_MAX] = "";
		if (getcwd(cwd, sizeof(cwd)) == NULL) {}

		commands_bin = (struct dirent ***)xnmalloc(
						path_n, sizeof(struct dirent));
		cmd_n = (int *)xnmalloc(path_n, sizeof(int));

		i = (int)path_n;
		while (--i >= 0) {
			if (!paths[i] || !*paths[i] || xchdir(paths[i], NO_TITLE) == -1) {
				cmd_n[i] = 0;
				continue;
			}

			cmd_n[i] = scandir(paths[i], &commands_bin[i],
						light_mode ? NULL : skip_nonexec, xalphasort);
			/* If paths[i] directory does not exist, scandir returns -1.
			 * Fedora, for example, adds $HOME/bin and $HOME/.local/bin to
			 * PATH disregarding if they exist or not. If paths[i] dir is
			 * empty do not use it either */
			if (cmd_n[i] > 0)
				total_cmd += cmd_n[i];
		}
		xchdir(cwd, NO_TITLE);
	}

	/* Add internal commands */
	size_t internal_cmd_n = 0;
	while (internal_cmds[internal_cmd_n])
		internal_cmd_n++;

	bin_commands = (char **)xnmalloc((size_t)total_cmd + internal_cmd_n +
					     aliases_n + actions_n + 2, sizeof(char *));

	i = (int)internal_cmd_n;
	while (--i >= 0)
		bin_commands[l++] = savestring(internal_cmds[i],
		    strlen(internal_cmds[i]));

	/* Now add aliases, if any */
	if (aliases_n) {
		i = (int)aliases_n;
		while (--i >= 0) {
			bin_commands[l++] = savestring(aliases[i].name,
				strlen(aliases[i].name));
		}
	}

	/* And user defined actions too, if any */
	if (actions_n) {
		i = (int)actions_n;
		while (--i >= 0) {
			bin_commands[l++] = savestring(usr_actions[i].name,
			    strlen(usr_actions[i].name));
		}
	}

	if (ext_cmd_ok && total_cmd) {
		/* And finally, add commands in PATH */
		i = (int)path_n;
		while (--i >= 0) {
			if (!cmd_n[i] || cmd_n[i] <= 0)
				continue;

			int j = cmd_n[i];
			while (--j >= 0) {
				bin_commands[l++] = savestring(commands_bin[i][j]->d_name,
					strlen(commands_bin[i][j]->d_name));
				free(commands_bin[i][j]);
			}

			free(commands_bin[i]);
		}

		free(commands_bin);
		free(cmd_n);
	}

	path_progsn = (size_t)l;
	bin_commands[l] = (char *)NULL;
}

void
get_aliases(void)
{
	if (!config_ok)
		return;

	int fd;
	FILE *fp = open_fstream_r(config_file, &fd);
	if (!fp) {
		_err('e', PRINT_PROMPT, "%s: alias: '%s': %s\n",
		    PROGRAM_NAME, config_file, strerror(errno));
		return;
	}


	/* Free the aliases struct array */
	if (aliases_n) {
		int i = (int)aliases_n;
		while (--i >= 0) {
			free(aliases[i].name);
			free(aliases[i].cmd);
		}

		free(aliases);
		aliases = (struct alias_t *)xnmalloc(1, sizeof(struct alias_t));
		aliases_n = 0;
	}

	char *line = (char *)NULL;
	size_t line_size = 0;

	while (getline(&line, &line_size, fp) > 0) {
		if (*line == 'a' && strncmp(line, "alias ", 6) == 0) {
			char *s = strchr(line, ' ');
			if (!s || !*(++s))
				continue;
			char *p = strchr(s, '=');
			if (!p || !*(p + 1))
				continue;
			*(p++) = '\0';

			/* Skip duplicated aliases names */
			int i = (int)aliases_n, exists = 0;
			while (--i >= 0) {
				if (!aliases[i].name)
					continue;
				if (*s == *aliases[i].name && strcmp(s, aliases[i].name) == 0) {
					exists = 1;
					break;
				}
			}
			if (exists)
				continue;

			aliases = (struct alias_t *)xrealloc(aliases, (aliases_n + 1)
						* sizeof(struct alias_t));
			aliases[aliases_n].name = savestring(s, strlen(s));
			if (*p == '\'')
				aliases[aliases_n++].cmd = strbtw(p, '\'', '\'');
			else if (*p == '"')
				aliases[aliases_n++].cmd = strbtw(p, '"', '"');
		}
	}

	free(line);
	close_fstream(fp, fd);
}

int
load_dirhist(void)
{
	if (!config_ok)
		return EXIT_FAILURE;

	int fd;
	FILE *fp = open_fstream_r(dirhist_file, &fd);
	if (!fp)
		return EXIT_FAILURE;

	size_t dirs = 0;

	char tmp_line[PATH_MAX];
	while (fgets(tmp_line, (int)sizeof(tmp_line), fp))
		dirs++;

	if (!dirs) {
		close_fstream(fp, fd);
		return EXIT_SUCCESS;
	}

	old_pwd = (char **)xnmalloc(dirs + 2, sizeof(char *));

	fseek(fp, 0L, SEEK_SET);

	size_t line_size = 0;
	char *line = (char *)NULL;
	ssize_t line_len = 0;

	dirhist_total_index = 0;

	while ((line_len = getline(&line, &line_size, fp)) > 0) {
		if (!line || !*line || *line == '\n')
			continue;
		if (line[line_len - 1] == '\n')
			line[line_len - 1] = '\0';
		old_pwd[dirhist_total_index] = (char *)xnmalloc((size_t)line_len + 1,
										sizeof(char));
		strcpy(old_pwd[dirhist_total_index++], line);
	}

	close_fstream(fp, fd);
	old_pwd[dirhist_total_index] = (char *)NULL;
	free(line);
	dirhist_cur_index = dirhist_total_index - 1;
	return EXIT_SUCCESS;
}

void
get_prompt_cmds(void)
{
	if (!config_ok)
		return;

	int fd;
	FILE *fp = open_fstream_r(config_file, &fd);
	if (!fp) {
		_err('e', PRINT_PROMPT, "%s: prompt: '%s': %s\n",
		    PROGRAM_NAME, config_file, strerror(errno));
		return;
	}

	if (prompt_cmds_n) {
		size_t i;
		for (i = 0; i < prompt_cmds_n; i++)
			free(prompt_cmds[i]);
		free(prompt_cmds);
		prompt_cmds = (char **)NULL;
		prompt_cmds_n = 0;
	}

	int prompt_line_found = 0;
	char *line = (char *)NULL;
	size_t line_size = 0;
	ssize_t line_len = 0;

	while ((line_len = getline(&line, &line_size, fp)) > 0) {
		if (prompt_line_found) {
			if (strncmp(line, "#END OF PROMPT", 14) == 0)
				break;
			if (*line != '#') {
				prompt_cmds = (char **)xrealloc(prompt_cmds,
				    (prompt_cmds_n + 1) * sizeof(char *));
				prompt_cmds[prompt_cmds_n++] = savestring(
				    line, (size_t)line_len);
			}
		} else if (strncmp(line, "#PROMPT", 7) == 0) {
			prompt_line_found = 1;
		}
	}

	free(line);
	close_fstream(fp, fd);
}

/* If some option was not set, set it to the default value */
void
check_options(void)
{
	if (!usr_cscheme)
		usr_cscheme = savestring("default", 7);

	if (!wprompt_str)
		wprompt_str = savestring(DEF_WPROMPT_STR, strlen(DEF_WPROMPT_STR));

	/* Do no override command line options */
	if (xargs.cwd_in_title == UNSET)
		xargs.cwd_in_title = DEF_CWD_IN_TITLE;

	if (xargs.control_d_exits == UNSET)
		control_d_exits = DEF_CONTROL_D_EXITS;

	if (elnpad == UNSET)
		elnpad = DEF_ELNPAD;

	if (cp_cmd == UNSET)
		cp_cmd = DEF_CP_CMD;

	if (check_cap == UNSET)
		check_cap = DEF_CHECK_CAP;

	if (check_ext == UNSET)
		check_ext = DEF_CHECK_EXT;

	if (follow_symlinks == UNSET)
		follow_symlinks = DEF_FOLLOW_SYMLINKS;

	if (mv_cmd == UNSET)
		mv_cmd = DEF_MV_CMD;

	if (min_name_trim == UNSET)
		min_name_trim = DEF_MIN_NAME_TRIM;

	if (min_jump_rank == UNSET)
		min_jump_rank = DEF_MIN_JUMP_RANK;

	if (max_jump_total_rank == UNSET)
		max_jump_total_rank = DEF_MAX_JUMP_TOTAL_RANK;

	if (no_eln == UNSET) {
		if (xargs.noeln == UNSET)
			no_eln = DEF_NOELN;
		else
			no_eln = xargs.noeln;
	}

	if (prompt_style == UNSET)
		prompt_style = DEF_PROMPT_STYLE;

#ifndef _NO_HIGHLIGHT
	if (highlight == UNSET) {
		if (xargs.highlight == UNSET)
			highlight = DEF_HIGHLIGHT;
		else
			highlight = xargs.highlight;
	}
#endif

	if (warn_wrong_cmd == UNSET) {
		if (xargs.warn_wrong_cmd == UNSET)
			warn_wrong_cmd = DEF_WARN_WRONG_CMD;
		else
			warn_wrong_cmd = xargs.warn_wrong_cmd;
	}

#ifndef _NO_FZF
	if (fzftab == UNSET) {
		if (xargs.fzftab == UNSET)
			fzftab = DEF_FZFTAB;
		else
			fzftab = xargs.fzftab;
	}
#endif

#ifndef _NO_ICONS
	if (icons == UNSET) {
		if (xargs.icons == UNSET)
			icons = DEF_ICONS;
		else
			icons = xargs.icons;
	}
#endif

#ifndef _NO_SUGGESTIONS
	if (suggestions == UNSET) {
		if (xargs.suggestions == UNSET)
			suggestions = DEF_SUGGESTIONS;
		else
			suggestions = xargs.suggestions;
	}

	if (!suggestion_strategy)
		suggestion_strategy = savestring(DEF_SUG_STRATEGY, SUG_STRATS);

	if (suggest_filetype_color == UNSET)
		suggest_filetype_color = DEF_SUG_FILETYPE_COLOR;
#endif

	if (int_vars == UNSET) {
		if (xargs.int_vars == UNSET)
			int_vars = DEF_INT_VARS;
		else
			int_vars = xargs.int_vars;
	}

	if (print_selfiles == UNSET) {
		if (xargs.printsel == UNSET)
			print_selfiles = DEF_PRINTSEL;
		else
			print_selfiles = xargs.printsel;
	}

	if (max_printselfiles == UNSET)
		max_printselfiles = DEF_MAXPRINTSEL;

	if (case_sens_dirjump == UNSET) {
		if (xargs.case_sens_dirjump == UNSET)
			case_sens_dirjump = DEF_CASE_SENS_DIRJUMP;
		else
			case_sens_dirjump = xargs.case_sens_dirjump;
	}

	if (case_sens_path_comp == UNSET) {
		if (xargs.case_sens_path_comp == UNSET)
			case_sens_path_comp = DEF_CASE_SENS_PATH_COMP;
		else
			case_sens_path_comp = xargs.case_sens_path_comp;
	}
#ifndef _NO_TRASH
	if (tr_as_rm == UNSET) {
		if (xargs.trasrm == UNSET)
			tr_as_rm = DEF_TRASRM;
		else
			tr_as_rm = xargs.trasrm;
	}
#endif
	if (xargs.stealth_mode == 1 && !opener)
		opener = savestring(FALLBACK_OPENER, strlen(FALLBACK_OPENER));

	if (only_dirs == UNSET) {
		if (xargs.only_dirs == UNSET)
			only_dirs = DEF_ONLY_DIRS;
		else
			only_dirs = xargs.only_dirs;
	}

	if (colorize == UNSET) {
		if (xargs.colorize == UNSET)
			colorize = DEF_COLORS;
		else
			colorize = xargs.colorize;
	}

	if (expand_bookmarks == UNSET) {
		if (xargs.expand_bookmarks == UNSET)
			expand_bookmarks = DEF_EXPAND_BOOKMARKS;
		else
			expand_bookmarks = xargs.expand_bookmarks;
	}

	if (splash_screen == UNSET) {
		if (xargs.splash == UNSET)
			splash_screen = DEF_SPLASH_SCREEN;
		else
			splash_screen = xargs.splash;
	}

	if (welcome_message == UNSET) {
		if (xargs.welcome_message == UNSET)
			welcome_message = DEF_WELCOME_MESSAGE;
		else
			welcome_message = xargs.welcome_message;
	}

	if (show_hidden == UNSET) {
		if (xargs.hidden == UNSET)
			show_hidden = DEF_SHOW_HIDDEN;
		else
			show_hidden = xargs.hidden;
	}

	if (files_counter == UNSET) {
		if (xargs.files_counter == UNSET)
			files_counter = DEF_FILES_COUNTER;
		else
			files_counter = xargs.files_counter;
	}

	if (long_view == UNSET) {
		if (xargs.longview == UNSET)
			long_view = DEF_LONG_VIEW;
		else
			long_view = xargs.longview;
	}

	if (ext_cmd_ok == UNSET) {
		if (xargs.ext == UNSET)
			ext_cmd_ok = DEF_EXT_CMD_OK;
		else
			ext_cmd_ok = xargs.ext;
	}

	if (pager == UNSET) {
		if (xargs.pager == UNSET)
			pager = DEF_PAGER;
		else
			pager = xargs.pager;
	}

	if (max_dirhist == UNSET) {
		if (xargs.max_dirhist == UNSET)
			max_dirhist = DEF_MAX_DIRHIST;
		else
			max_dirhist = xargs.max_dirhist;
	}

	if (clear_screen == UNSET) {
		if (xargs.clear_screen == UNSET)
			clear_screen = DEF_CLEAR_SCREEN;
		else
			clear_screen = xargs.clear_screen;
	}

	if (list_folders_first == UNSET) {
		if (xargs.ffirst == UNSET)
			list_folders_first = DEF_LIST_FOLDERS_FIRST;
		else
			list_folders_first = xargs.ffirst;
	}

	if (cd_lists_on_the_fly == UNSET) {
		if (xargs.cd_list_auto == UNSET)
			cd_lists_on_the_fly = DEF_CD_LISTS_ON_THE_FLY;
		else
			cd_lists_on_the_fly = xargs.cd_list_auto;
	}

	if (case_sensitive == UNSET) {
		if (xargs.sensitive == UNSET)
			case_sensitive = DEF_CASE_SENSITIVE;
		else
			case_sensitive = xargs.sensitive;
	}

	if (unicode == UNSET) {
		if (xargs.unicode == UNSET)
			unicode = DEF_UNICODE;
		else
			unicode = xargs.unicode;
	}

	if (max_path == UNSET) {
		if (xargs.max_path == UNSET)
			max_path = DEF_MAX_PATH;
		else
			max_path = xargs.max_path;
	}

	if (logs_enabled == UNSET) {
		if (xargs.logs == UNSET)
			logs_enabled = DEF_LOGS_ENABLED;
		else
			logs_enabled = xargs.logs;
	}

	if (light_mode == UNSET) {
		if (xargs.light == UNSET)
			light_mode = DEF_LIGHT_MODE;
		else
			light_mode = xargs.light;
	}

	if (classify == UNSET) {
		if (xargs.classify == UNSET)
			classify = DEF_CLASSIFY;
		else
			classify = xargs.classify;
	}

	if (share_selbox == UNSET) {
		if (xargs.share_selbox == UNSET)
			share_selbox = DEF_SHARE_SELBOX;
		else
			share_selbox = xargs.share_selbox;
	}

	if (sort == UNSET) {
		if (xargs.sort == UNSET)
			sort = DEF_SORT;
		else
			sort = xargs.sort;
	}

	if (sort_reverse == UNSET) {
		if (xargs.sort_reverse == UNSET)
			sort_reverse = DEF_SORT_REVERSE;
		else
			sort_reverse = xargs.sort_reverse;
	}

	if (tips == UNSET) {
		if (xargs.tips == UNSET)
			tips = DEF_TIPS;
		else
			tips = xargs.tips;
	}

	if (autocd == UNSET) {
		if (xargs.autocd == UNSET)
			autocd = DEF_AUTOCD;
		else
			autocd = xargs.autocd;
	}

	if (auto_open == UNSET) {
		if (xargs.auto_open == UNSET)
			auto_open = DEF_AUTO_OPEN;
		else
			auto_open = xargs.auto_open;
	}

	if (autojump == UNSET) {
		if (xargs.autojump == UNSET)
			autojump = DEF_AUTOJUMP;
		else
			autojump = xargs.autojump;
		if (autojump == 1)
			autocd = 1;
	}

	if (cd_on_quit == UNSET) {
		if (xargs.cd_on_quit == UNSET)
			cd_on_quit = DEF_CD_ON_QUIT;
		else
			cd_on_quit = xargs.cd_on_quit;
	}

	if (dirhist_map == UNSET) {
		if (xargs.dirmap == UNSET)
			dirhist_map = DEF_DIRHIST_MAP;
		else
			dirhist_map = xargs.dirmap;
	}

	if (disk_usage == UNSET) {
		if (xargs.disk_usage == UNSET)
			disk_usage = DEF_DISK_USAGE;
		else
			disk_usage = xargs.disk_usage;
	}

	if (restore_last_path == UNSET) {
		if (xargs.restore_last_path == UNSET)
			restore_last_path = DEF_RESTORE_LAST_PATH;
		else
			restore_last_path = xargs.restore_last_path;
	}

/*	if (!*div_line_char) {
		*div_line_char = DEF_DIV_LINE_CHAR;
		div_line_char[1] = '\0';
	} */

	if (max_hist == UNSET)
		max_hist = DEF_MAX_HIST;

	if (max_log == UNSET)
		max_log = DEF_MAX_LOG;

	if (!user.shell) {
		struct user_t tmp_user = get_user();
		user.shell = tmp_user.shell;

		/* We don't need these values of the user struct: free(d) them */
		free(tmp_user.name);
		free(tmp_user.home);

		if (!user.shell)
			user.shell = savestring(FALLBACK_SHELL, strlen(FALLBACK_SHELL));
	}

	if (!term)
		term = savestring(DEFAULT_TERM_CMD, strlen(DEFAULT_TERM_CMD));

	if (!encoded_prompt)
		encoded_prompt = savestring(DEFAULT_PROMPT, strlen(DEFAULT_PROMPT));

	/* Since in stealth mode we have no access to the config file, we
	 * cannot use 'lira', since it relays on a file.
	 * Set it thus to xdg-open, if not already set via command line */
	if (xargs.stealth_mode == 1 && !opener)
		opener = savestring(FALLBACK_OPENER, strlen(FALLBACK_OPENER));
}
