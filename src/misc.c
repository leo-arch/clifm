/* misc.c -- functions that do not fit in any other file */

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

/* The err function is based on littlstar's asprintf implementation
 * (https://github.com/littlstar/asprintf.c/blob/master/asprintf.c),
 * licensed under MIT.
 * All changes are licensed under GPL-2.0-or-later. */

#include "helpers.h"

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h> /* tcsetattr */

#ifdef __sun
# include <sys/termios.h> /* TIOCGWINSZ */
#endif /* __sun */

#if defined(__NetBSD__) || defined(__FreeBSD__)
# include <sys/param.h>
# include <sys/sysctl.h>
#endif /* __NetBSD__ || __FreeBSD__ */

#ifdef __OpenBSD__
typedef char *rl_cpvfunc_t;
# include <ereadline/readline/readline.h>
#else
# include <readline/readline.h>
#endif /* __OpenBSD__ */

#include <time.h>
#include <unistd.h>
#ifdef LINUX_INOTIFY
# include <sys/inotify.h>
#endif /* LINUX_INOTIFY */

#include "aux.h"
#include "bookmarks.h"
#include "checks.h"
#include "exec.h"
#include "file_operations.h"
#include "history.h"
#include "init.h"
#include "jump.h"
#include "listing.h"
#include "messages.h"
#include "navigation.h"
#include "readline.h"
#include "remotes.h"

char *
gen_diff_str(const int diff)
{
	if (diff == 1)
		return "\x1b[1C";
	if (diff == 2)
		return "\x1b[2C";
	if (diff == 3)
		return "\x1b[3C";

	static char diff_str[14];
	snprintf(diff_str, sizeof(diff_str), "\x1b[%dC", diff);

	return diff_str;
}

int
is_blank_name(const char *s)
{
	if (!s || !*s)
		return 1;

	int blank = 1;

	while (*s) {
		if (*s != ' ' && *s != '\n' && *s != '\t') {
			blank = 0;
			break;
		}
		s++;
	}

	return blank;
}

/* Prompt for a new name using MSG as prompt.
 * If OLD_NAME is not NULL, it will be used as template for the new name.
 * If the entered name is quoted, it will be returned verbatim (without quotes),
 * and QUOTED will be set to 1 (the caller should not perform expansions on
 * this name). */
char *
get_newname(const char *msg, char *old_name, int *quoted)
{
	xrename = rl_nohist = 1;
	int poffset_bk = prompt_offset;
	prompt_offset = 3;

	char *n = (old_name && *old_name) ? unescape_str(old_name, 0) : (char *)NULL;
	char *input = secondary_prompt((msg && *msg) ? msg : "> ",
		n ? n : (char *)NULL);
	free(n);

	char *new_name = (char *)NULL;
	if (!input)
		goto END;

	char *p = remove_quotes(input);
	if (!p || !*p) /* Input was "" (empty string) */
		goto END;

	if (p != input) {
		/* Quoted: copy input verbatim (without quotes) */
		*quoted = 1;
		new_name = savestring(p, strlen(p));
		goto END;
	}

	char *deq = unescape_str(p, 0);
	char *tmp = deq ? deq : p;

	size_t len = strlen(tmp);
	int i = len > 1 ? (int)len - 1 : 0;
	while (tmp[i] == ' ') {
		tmp[i] = '\0';
		i--;
	}

	new_name = savestring(tmp, (size_t)i + 1);
	free(deq);

END:
	free(input);
	xrename = rl_nohist = 0;
	prompt_offset = poffset_bk;

	return new_name;
}

/* Set ELN color according to the current workspace */
void
set_eln_color(void)
{
	switch (cur_ws) {
	case 0: xstrsncpy(el_c, *ws1_c ? ws1_c : DEF_EL_C, sizeof(el_c)); break;
	case 1: xstrsncpy(el_c, *ws2_c ? ws2_c : DEF_EL_C, sizeof(el_c)); break;
	case 2: xstrsncpy(el_c, *ws3_c ? ws3_c : DEF_EL_C, sizeof(el_c)); break;
	case 3: xstrsncpy(el_c, *ws4_c ? ws4_c : DEF_EL_C, sizeof(el_c)); break;
	case 4: xstrsncpy(el_c, *ws5_c ? ws5_c : DEF_EL_C, sizeof(el_c)); break;
	case 5: xstrsncpy(el_c, *ws6_c ? ws6_c : DEF_EL_C, sizeof(el_c)); break;
	case 6: xstrsncpy(el_c, *ws7_c ? ws7_c : DEF_EL_C, sizeof(el_c)); break;
	case 7: xstrsncpy(el_c, *ws8_c ? ws8_c : DEF_EL_C, sizeof(el_c)); break;
	default: xstrsncpy(el_c, DEF_EL_C, sizeof(el_c)); break;
	}
}

/* Custom POSIX implementation of GNU asprintf() modified to log program
 * messages.
 *
 * MSG_TYPE is one of: 'e', 'f', 'w', 'n', zero (meaning this
 * latter that no message mark (E, W, or N) will be added to the prompt).
 * If MSG_TYPE is 'n' the message is not not logged.
 * 'f' means that the message must be printed forcefully, even if identical
 * to the previous one, without printing any message mark.
 * MSG_TYPE also accepts ERR_NO_LOG (-1) and ERR_NO_STORE (-2) as values:
 * ERR_NO_LOG: Print the message but do not log it.
 * ERR_NO_STORE: Log but do not store the message into the messages array.
 *
 * PROMPT_FLAG tells whether to print the message immediately before the next
 * prompt or rather in place.
 *
 * This function guarantees not to modify the value of errno, usually passed
 * as part of the format string to print error messages.
 * */
__attribute__((__format__(__printf__, 3, 0)))
/* We use __attribute__ here to silence clang warning: "format string is
 * not a string literal" */
int
err(const int msg_type, const int prompt_flag, const char *format, ...)
{
	const int saved_errno = errno;
	va_list arglist;

	va_start(arglist, format);
	int size = vsnprintf((char *)NULL, 0, format, arglist);
	va_end(arglist);

	if (size < 0)
		goto ERROR;

	const size_t n = (size_t)size + 1;
	char *buf = xnmalloc(n, sizeof(char));
	va_start(arglist, format);
	size = vsnprintf(buf, n, format, arglist);
	va_end(arglist);

	if (size < 0 || !buf || !*buf)
		{free(buf); goto ERROR;}

	/* If the new message is the same as the last message, skip it. */
	if (msgs_n > 0 && msg_type != 'f' && *messages[msgs_n - 1] == *buf
	&& strcmp(messages[msgs_n - 1], buf) == 0)
		{free(buf); goto ERROR;}

	if (msg_type >= 'e') {
		switch (msg_type) {
		case 'e': pmsg = ERROR; msgs.error++; break;
		case 'w': pmsg = WARNING; msgs.warning++; break;
		case 'n': pmsg = NOTICE; msgs.notice++; break;
		default:  pmsg = NOMSG; break;
		}
	}

	int logme = msg_type == ERR_NO_LOG ? 0 : (msg_type == 'n' ? -1 : 1);
	int add_to_msgs_list = 1;

	if (msg_type == ERR_NO_STORE) {
		add_to_msgs_list = 0;
		logme = 1;
	}

	log_msg(buf, prompt_flag, logme, add_to_msgs_list);

	free(buf);
	errno = saved_errno;
	return EXIT_SUCCESS;

ERROR:
	errno = saved_errno;
	return EXIT_FAILURE;
}

/* Print format string MSG, as "> MSG" (colored), if autols is on, or just
 * as "MSG" if off.
 * This function is used to inform the user about changes that require a
 * a files list reload (either upon files or interface modifications). */
__attribute__((__format__(__printf__, 1, 0)))
int
print_reload_msg(const char *msg, ...)
{
	va_list arglist;

	va_start(arglist, msg);
	const int size = vsnprintf((char *)NULL, 0, msg, arglist);
	va_end(arglist);

	if (size < 0)
		return EXIT_FAILURE;

	if (conf.autols == 1)
		printf("%s->%s ", mi_c, df_c);

	char *buf = xnmalloc((size_t)size + 1, sizeof(char));

	va_start(arglist, msg);
	vsnprintf(buf, (size_t)size + 1, msg, arglist);
	va_end(arglist);

	fputs(buf, stdout);
	free(buf);

	return EXIT_SUCCESS;
}

#ifdef LINUX_INOTIFY
void
reset_inotify(void)
{
	watch = 0;

	if (inotify_wd >= 0) {
		inotify_rm_watch(inotify_fd, inotify_wd);
		inotify_wd = -1;
	}

	if (inotify_fd != UNSET)
		close(inotify_fd);
	inotify_fd = inotify_init1(IN_NONBLOCK);
	if (inotify_fd < 0) {
		err('w', PRINT_PROMPT, "%s: inotify: %s\n",
			PROGRAM_NAME, strerror(errno));
		return;
	}

	/* If CWD is a symlink to a directory and it does not end with a slash,
	 * inotify_add_watch(3) fails with ENOTDIR. */
	char rpath[PATH_MAX + 1];
	snprintf(rpath, sizeof(rpath), "%s/", workspaces[cur_ws].path);

	inotify_wd = inotify_add_watch(inotify_fd, rpath, INOTIFY_MASK);
	if (inotify_wd > 0)
		watch = 1;
	else
		err('w', PRINT_PROMPT, "%s: inotify: '%s': %s\n",
			PROGRAM_NAME, rpath, strerror(errno));
}

void
read_inotify(void)
{
	if (inotify_fd == UNSET)
		return;

	int i;
	struct inotify_event *event;
	char inotify_buf[EVENT_BUF_LEN];

	memset((void *)inotify_buf, '\0', sizeof(inotify_buf));
	i = (int)read(inotify_fd, inotify_buf, sizeof(inotify_buf));

	if (i <= 0) {
# ifdef INOTIFY_DEBUG
		puts("INOTIFY_RETURN");
# endif /* INOTIFY_DEBUG */
		return;
	}

	int ignore_event = 0;
	int refresh = 0;

	for (char *ptr = inotify_buf;
	ptr + ((struct inotify_event *)ptr)->len < inotify_buf + i;
	ptr += sizeof(struct inotify_event) + event->len) {
		event = (struct inotify_event *)ptr;

# ifdef INOTIFY_DEBUG
		printf("%s (%u:%d): ", *event->name
			? event->name : NULL, event->len, event->wd);
# endif /* INOTIFY_DEBUG */

		if (!event->wd) {
# ifdef INOTIFY_DEBUG
			puts("INOTIFY_BREAK");
# endif /* INOTIFY_DEBUG */
			break;
		}

		if (event->mask & IN_CREATE) {
# ifdef INOTIFY_DEBUG
			puts("IN_CREATE");
# endif /* INOTIFY_DEBUG */
			struct stat a;
			if (event->len && lstat(event->name, &a) != 0) {
				/* The file was created, but doesn't exist anymore */
				ignore_event = 1;
			}
		}

		/* A file was renamed */
		if (event->mask & IN_MOVED_TO) {
# ifdef INOTIFY_DEBUG
			puts("IN_MOVED_TO");
# endif /* INOTIFY_DEBUG */
			filesn_t j = files;
			while (--j >= 0) {
				if (*file_info[j].name == *event->name
				&& strcmp(file_info[j].name, event->name) == 0)
					break;
			}

			/* If destiny file name is already in the files list (j >= 0),
			 * ignore this event. */
			ignore_event = (j < 0) ? 0 : 1;
		}

		if (event->mask & IN_DELETE) {
# ifdef INOTIFY_DEBUG
			puts("IN_DELETE");
# endif /* INOTIFY_DEBUG */
			struct stat a;
			if (event->len && lstat(event->name, &a) == 0)
				/* The file was removed, but is still there (recreated) */
				ignore_event = 1;
		}

# ifdef INOTIFY_DEBUG
		if (event->mask & IN_DELETE_SELF)
			puts("IN_DELETE_SELF");
		if (event->mask & IN_MOVE_SELF)
			puts("IN_MOVE_SELF");
		if (event->mask & IN_MOVED_FROM)
			puts("IN_MOVED_FROM");
		if (event->mask & IN_MOVED_TO)
			puts("IN_MOVED_TO");
		if (event->mask & IN_IGNORED)
			puts("IN_IGNORED");
# endif /* INOTIFY_DEBUG */

		if (ignore_event == 0 && (event->mask & INOTIFY_MASK))
			refresh = 1;
	}

	if (refresh == 1 && exit_code == EXIT_SUCCESS) {
# ifdef INOTIFY_DEBUG
		puts("INOTIFY_REFRESH");
# endif /* INOTIFY_DEBUG */
		reload_dirlist();
	} else {
# ifdef INOTIFY_DEBUG
		puts("INOTIFY_RESET");
# endif /* INOTIFY_DEBUG */
		/* Reset the inotify watch list */
		reset_inotify();
	}

	return;
}
#elif defined(BSD_KQUEUE)
/* Insert the following lines in the for loop to debug kqueue:
if (event_data[i].fflags & NOTE_DELETE)
	puts("NOTE_DELETE");
if (event_data[i].fflags & NOTE_WRITE)
	puts("NOTE_WRITE");
if (event_data[i].fflags & NOTE_EXTEND)
	puts("NOTE_EXTEND");
if (event_data[i].fflags & NOTE_ATTRIB)
	puts("NOTE_ATTRIB");
if (event_data[i].fflags & NOTE_LINK)
	puts("NOTE_LINK");
if (event_data[i].fflags & NOTE_RENAME)
	puts("NOTE_RENAME");
if (event_data[i].fflags & NOTE_REVOKE)
	puts("NOTE_REVOKE"); */
void
read_kqueue(void)
{
	struct kevent event_data[NUM_EVENT_SLOTS];
	memset((void *)event_data, '\0', sizeof(struct kevent) * NUM_EVENT_SLOTS);

	int i;
	const int count = kevent(kq, NULL, 0, event_data, 4096, &timeout);

	for (i = 0; i < count; i++) {
		if (event_data[i].fflags & KQUEUE_FFLAGS) {
			reload_dirlist();
			return;
		}
	}
}
#endif /* LINUX_INOTIFY */

void
set_term_title(char *str)
{
	int free_tmp = 0;
	char *tmp = home_tilde(str, &free_tmp);

	printf("\033]2;%s - %s\007", PROGRAM_NAME, tmp ? tmp : str);
	fflush(stdout);

	if (free_tmp == 1)
		free(tmp);
}

void
set_filter_type(const char c)
{
	if (c == '=')
		filter.type = FILTER_FILE_TYPE;
	else if (c == '@')
		filter.type = FILTER_MIME_TYPE; /* UNIMPLEMENTED */
	else
		filter.type = FILTER_FILE_NAME;
}

static int
unset_filter(void)
{
	if (!filter.str) {
		puts(_("No filter set"));
		return EXIT_SUCCESS;
	}

	free(filter.str);
	filter.str = (char *)NULL;
	filter.rev = 0;
	filter.type = FILTER_NONE;
	regfree(&regex_exp);

	if (conf.autols == 1)
		reload_dirlist();

	puts(_("Filter unset"));
	return EXIT_SUCCESS;
}

static int
validate_file_type_filter(void)
{
	if (!filter.str || !*filter.str || *filter.str != '='
	|| !*(filter.str + 1) || *(filter.str + 2))
		return EXIT_FAILURE;

	const char c = *(filter.str + 1);
	if (c == 'b' || c == 'c' || c == 'd' || c == 'f'
	|| c == 'l' || c == 'p' || c == 's')
		return EXIT_SUCCESS;

	if (conf.light_mode == 1)
		return EXIT_FAILURE;

	if (c == 'g' || c == 'h' || c == 'o' || c == 't'
	|| c == 'u' || c == 'x')
		return EXIT_SUCCESS;

	return EXIT_FAILURE;
}

static int
compile_filter(void)
{
	if (filter.type == FILTER_FILE_NAME) {
		const int ret =
			regcomp(&regex_exp, filter.str, REG_NOSUB | REG_EXTENDED);
		if (ret != EXIT_SUCCESS) {
			xregerror("ft", filter.str, ret, regex_exp, 0);
			regfree(&regex_exp);
			goto ERR;
		}
	} else if (filter.type == FILTER_FILE_TYPE) {
		if (validate_file_type_filter() != EXIT_SUCCESS) {
			xerror("%s\n", _("ft: Invalid file type filter"));
			goto ERR;
		}
	} else {
		xerror("%s\n", _("ft: Invalid filter"));
		goto ERR;
	}

	if (conf.autols == 1)
		reload_dirlist();

	print_reload_msg(_("%s%s: New filter successfully set\n"),
		filter.rev == 1 ? "!" : "", filter.str);

	return EXIT_SUCCESS;

ERR:
	free(filter.str);
	filter.str = (char *)NULL;
	filter.type = FILTER_NONE;
	return EXIT_FAILURE;
}

int
filter_function(char *arg)
{
	if (!arg) {
		printf(_("Current filter: %c%s\n"), filter.rev == 1 ? '!' : 0,
			filter.str ? filter.str : "none");
		return EXIT_SUCCESS;
	}

	if (IS_HELP(arg)) {
		puts(_(FILTER_USAGE));
		return EXIT_SUCCESS;
	}

	if (*arg == 'u' && strcmp(arg, "unset") == 0)
		return unset_filter();

	free(filter.str);
	regfree(&regex_exp);

	if (*arg == '!') {
		filter.rev = 1;
		arg++;
	} else {
		filter.rev = 0;
	}

	char *p = arg;
	if (*arg == '\'' || *arg == '"') {
		p = remove_quotes(arg);
		if (!p) {
			xerror("%s\n", _("ft: Error removing quotes: Filter unset"));
			return EXIT_FAILURE;
		}
	}

	set_filter_type(*p);
	filter.str = savestring(p, strlen(p));

	return compile_filter();
}

/* Check whether the conditions to run the new_instance function are
 * fulfilled */
static inline int
check_new_instance_init_conditions(void)
{
	if (!conf.term) {
		xerror(_("%s: Default terminal not set. Use the "
			"configuration file (F10) to set it\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	if (!(flags & GUI)) {
		xerror(_("%s: Function only available for graphical "
			"environments\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

/* Just check that DIR exists and is a directory */
static inline int
check_dir(char **dir)
{
	int ret = EXIT_SUCCESS;
	struct stat attr;
	if (stat(*dir, &attr) == -1) {
		xerror("%s: '%s': %s\n", PROGRAM_NAME, *dir, strerror(errno));
		return errno;
	}

	if (!S_ISDIR(attr.st_mode)) {
		xerror(_("%s: '%s': Not a directory\n"), PROGRAM_NAME, *dir);
		return ENOTDIR;
	}

	return ret;
}

/* Construct absolute path for DIR */
static inline char *
get_path_dir(char **dir)
{
	char *path_dir = (char *)NULL;

	if (*(*dir) != '/') {
		const size_t len = strlen(workspaces[cur_ws].path) + strlen(*dir) + 2;
		path_dir = xnmalloc(len, sizeof(char));
		snprintf(path_dir, len, "%s/%s", workspaces[cur_ws].path, *dir);
		free(*dir);
		*dir = (char *)NULL;
	} else {
		path_dir = *dir;
	}

	return path_dir;
}

/* Get command to be executed by the new_instance function, only if
 * CONF.TERM (global) contains spaces. Otherwise, new_instance will try
 * "CONF.TERM clifm". */
static char **
get_cmd(char *dir, char *_sudo, char *self, const int sudo)
{
	if (!conf.term || !strchr(conf.term, ' '))
		return (char **)NULL;

	char **tmp_term = get_substr(conf.term, ' ');
	if (!tmp_term)
		return (char **)NULL;

	int i;
	for (i = 0; tmp_term[i]; i++);

	int num = i;
	char **cmd = xnmalloc((size_t)i + (sudo == 1 ? 4 : 3), sizeof(char *));

	for (i = 0; tmp_term[i]; i++) {
		cmd[i] = savestring(tmp_term[i], strlen(tmp_term[i]));
		free(tmp_term[i]);
	}
	free(tmp_term);

	i = num - 1;
	int plus = 1;

	size_t len = 0;
	if (sudo == 1) {
		len = strlen(self);
		cmd[i + plus] = xnmalloc(len + 1, sizeof(char));
		xstrsncpy(cmd[i + plus], _sudo, len + 1);
		plus++;
	}

	len = strlen(self);
	cmd[i + plus] = xnmalloc(len + 1, sizeof(char));
	xstrsncpy(cmd[i + plus], self, len + 1);
	plus++;
	len = strlen(dir);
	cmd[i + plus] = xnmalloc(len + 1, sizeof(char));
	xstrsncpy(cmd[i + plus], dir, len + 1);
	plus++;
	cmd[i + plus] = (char *)NULL;

	return cmd;
}

/* Print the command CMD and ask the user for confirmation.
 * Returns 1 if yes or 0 if no. */
int
confirm_sudo_cmd(char **cmd)
{
	if (!cmd)
		return 0;

	size_t i;
	for (i = 0; cmd[i]; i++) {
		fputs(cmd[i], stdout);
		putchar(' ');
	}
	if (i > 0)
		putchar('\n');

	return rl_get_y_or_n(_("Run command? [y/n] "));
}

/* Launch a new instance using CMD. If CMD is NULL, try "CONF.TERM clifm".
 * Returns the exit status of the executed command. */
static int
launch_new_instance_cmd(char ***cmd, char **self, char **_sudo,
	char **dir, int sudo)
{
	int ret = 0;
#if defined(__HAIKU__)
	sudo = 0;
#endif /* __HAIKU__ */

//	setenv("CLIFM_OWN_CHILD", "1", 1);

	if (*cmd) {
		ret = (sudo == 0 || confirm_sudo_cmd(*cmd) == 1)
			? launch_execv(*cmd, BACKGROUND, E_NOFLAG)
			: EXIT_SUCCESS;
		size_t i;
		for (i = 0; (*cmd)[i]; i++)
			free((*cmd)[i]);
		free(*cmd);
	} else {
		if (sudo == 1) {
			char *tcmd[] = {conf.term, *_sudo, *self, *dir, NULL};
			ret = (confirm_sudo_cmd(tcmd) == 1)
				? launch_execv(tcmd, BACKGROUND, E_NOFLAG)
				: EXIT_SUCCESS;
		} else {
			char *tcmd[] = {conf.term, *self, *dir, NULL};
			ret = launch_execv(tcmd, BACKGROUND, E_NOFLAG);
		}
	}

	free(*_sudo);
	free(*self);
	free(*dir);

//	unsetenv("CLIFM_OWN_CHILD");

	return ret;
}

/* After the last line of new_instance */
// cppcheck-suppress syntaxError

/* Open DIR in a new instance of the program (using TERM, set in the config
 * file, as terminal emulator). */
int
new_instance(char *dir, const int sudo)
{
	if (check_new_instance_init_conditions() == EXIT_FAILURE)
		return EXIT_FAILURE;

	if (!dir)
		return EINVAL;

	char *_sudo = (char *)NULL;
#ifndef __HAIKU__
	if (sudo == 1 && !(_sudo = get_sudo_path()))
		return errno;
#endif /* !__HAIKU__ */

	char *deq_dir = unescape_str(dir, 0);
	if (!deq_dir) {
		free(_sudo);
		xerror(_("%s: '%s': Cannot escape file name\n"), PROGRAM_NAME, dir);
		return EXIT_FAILURE;
	}

	char *self = get_cmd_path(PROGRAM_NAME);
	if (!self) {
		free(_sudo); free(deq_dir);
		xerror("%s: %s: %s\n", PROGRAM_NAME, PROGRAM_NAME, strerror(errno));
		return errno;
	}

	const int ret = check_dir(&deq_dir);
	if (ret != EXIT_SUCCESS) {
		free(deq_dir); free(self); free(_sudo);
		return ret;
	}

	char *path_dir = get_path_dir(&deq_dir);
	char **cmd = get_cmd(path_dir, _sudo, self, sudo);
	return launch_new_instance_cmd(&cmd, &self, &_sudo, &path_dir, sudo);
}

int
alias_import(char *file)
{
	if (xargs.stealth_mode == 1) {
		printf("%s: alias: %s\n", PROGRAM_NAME, STEALTH_DISABLED);
		return EXIT_SUCCESS;
	}

	if (!file)
		return EXIT_FAILURE;

	char rfile[PATH_MAX + 1];
	rfile[0] = '\0';

	if (*file == '~') {
		char *file_exp = tilde_expand(file);
		if (!file_exp) {
			xerror("%s: '%s': %s\n", PROGRAM_NAME, file, strerror(errno));
			return EXIT_FAILURE;
		}

		if (realpath(file_exp, rfile) == NULL) {
			xerror("%s: '%s': %s\n", PROGRAM_NAME, file_exp, strerror(errno));
			free(file_exp);
			return EXIT_FAILURE;
		}
		free(file_exp);
	} else {
		if (realpath(file, rfile) == NULL) {
			xerror("%s: '%s': %s\n", PROGRAM_NAME, file, strerror(errno));
			return EXIT_FAILURE;
		}
	}

	if (rfile[0] == '\0') {
		xerror("%s: '%s': %s\n", PROGRAM_NAME, file, strerror(errno));
		return EXIT_FAILURE;
	}

	/* Open the file to import aliases from */
	int fd;
	FILE *fp = open_fread(rfile, &fd);
	if (!fp) {
		xerror("%s: '%s': %s\n", PROGRAM_NAME, rfile, strerror(errno));
		return EXIT_FAILURE;
	}

	/* Open CliFM's config file as well */
	FILE *config_fp = open_fappend(config_file);
	if (!config_fp) {
		xerror("%s: '%s': %s\n", PROGRAM_NAME, config_file, strerror(errno));
		fclose(fp);
		return EXIT_FAILURE;
	}

	size_t line_size = 0, i;
	char *line = (char *)NULL;
	size_t alias_found = 0, alias_imported = 0;
	int first = 1;

	while (getline(&line, &line_size, fp) > 0) {
		if (*line != 'a' || strncmp(line, "alias ", 6) != 0)
			continue;

		alias_found++;

		/* If alias name conflicts with some internal command, skip it */
		char *alias_name = strbtw(line, ' ', '=');
		if (!alias_name)
			continue;

		if (is_internal_c(alias_name)) {
			xerror(_("'%s': Alias conflicts with internal "
				"command\n"), alias_name);
			free(alias_name);
			continue;
		}

		char *p = line + 6; /* p points now to the beginning of the
		alias name (because "alias " == 6) */

		char *tmp = strchr(p, '=');
		if (!tmp) {
			free(alias_name);
			continue;
		}
		if (!*(++tmp)) {
			free(alias_name);
			continue;
		}
		if (*tmp != '\'' && *tmp != '"') {
			free(alias_name);
			continue;
		}

		*(tmp - 1) = '\0';
		/* If alias already exists, skip it too */
		int exists = 0;
		for (i = 0; i < aliases_n; i++) {
			if (*p == *aliases[i].name && strcmp(aliases[i].name, p) == 0) {
				exists = 1;
				break;
			}
		}

		*(tmp - 1) = '=';

		if (exists == 0) {
			if (first == 1) {
				first = 0;
				fputs("\n\n", config_fp);
			}

			alias_imported++;

			/* Write the new alias into CliFM's config file */
			fputs(line, config_fp);
		} else {
			xerror(_("'%s': Alias already exists\n"), alias_name);
		}

		free(alias_name);
	}

	free(line);
	fclose(fp);
	fclose(config_fp);

	/* No alias was found in FILE */
	if (alias_found == 0) {
		xerror(_("%s: '%s': No alias found\n"), PROGRAM_NAME, rfile);
		return EXIT_FAILURE;
	}

	/* Aliases were found in FILE, but none was imported (either because
	 * they conflicted with internal commands or the alias already
	 * existed). */
	else {
		if (alias_imported == 0) {
			xerror(_("%s: No alias imported\n"), PROGRAM_NAME);
			return EXIT_FAILURE;
		}
	}

	/* If some alias was found and imported, print the corresponding
	 * message and update the aliases array. */
	printf(_("%s: %zu alias(es) imported\n"), PROGRAM_NAME, alias_imported);

	/* Add new aliases to the internal list of aliases */
	get_aliases();

	/* Add new aliases to the commands list for TAB completion */
	if (bin_commands) {
		for (i = 0; bin_commands[i]; i++)
			free(bin_commands[i]);
		free(bin_commands);
		bin_commands = (char **)NULL;
	}
	get_path_programs();

	return EXIT_SUCCESS;
}

char *
parse_usrvar_value(const char *str, const char c)
{
	if (c == '\0' || !str)
		return (char *)NULL;

	/* Get whatever comes after c */
	char *tmp = strchr(str, c);
	if (!tmp || !*(++tmp))
		return (char *)NULL;

	/* Remove leading quotes */
	if (*tmp == '"' || *tmp == '\'')
		tmp++;

	/* Remove trailing spaces, tabs, new line chars, and quotes */
	const size_t tmp_len = strlen(tmp);
	size_t i;

	for (i = tmp_len - 1; tmp[i] && i > 0; i--) {
		if (tmp[i] != ' ' && tmp[i] != '\t' && tmp[i] != '"' && tmp[i] != '\''
		&& tmp[i] != '\n')
			break;
		else
			tmp[i] = '\0';
	}

	if (!*tmp)
		return (char *)NULL;

	char *buf = savestring(tmp, strlen(tmp));
	return buf;
}

int
create_usr_var(const char *str)
{
	if (!str || !*str)
		return EXIT_FAILURE;

	char *p = strchr(str, '=');
	if (!p || p == str)
		return EXIT_FAILURE;

	*p = '\0';
	const size_t len = (size_t)(p - str);
	char *name = xnmalloc(len + 1, sizeof(char));
	xstrsncpy(name, str, len + 1);
	*p = '=';

	char *value = parse_usrvar_value(str, '=');

	if (!value) {
		free(name);
		xerror(_("%s: Error getting variable value\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	usr_var = xnrealloc(usr_var, (size_t)(usrvar_n + 2), sizeof(struct usrvar_t));
	usr_var[usrvar_n].name = savestring(name, strlen(name));
	usr_var[usrvar_n].value = savestring(value, strlen(value));
	usrvar_n++;

	usr_var[usrvar_n].name = (char *)NULL;
	usr_var[usrvar_n].value = (char *)NULL;

	free(name);
	free(value);
	return EXIT_SUCCESS;
}

void
free_autocmds(void)
{
	int i = (int)autocmds_n;
	while (--i >= 0) {
		free(autocmds[i].pattern);
		free(autocmds[i].cmd);
		autocmds[i].color_scheme = (char *)NULL;
	}
	free(autocmds);
	autocmds = (struct autocmds_t *)NULL;
	autocmds_n = 0;
	autocmd_set = 0;
}

void
free_tags(void)
{
	int i = (int)tags_n;
	while (--i >= 0)
		free(tags[i]);
	free(tags);
	tags = (char **)NULL;
	tags_n = 0;
}

int
free_remotes(const int exit)
{
	if (exit)
		autounmount_remotes();

	size_t i;
	for (i = 0; i < remotes_n; i++) {
		free(remotes[i].name);
		free(remotes[i].desc);
		free(remotes[i].mountpoint);
		free(remotes[i].mount_cmd);
		free(remotes[i].unmount_cmd);
	}
	free(remotes);
	remotes_n = 0;

	return EXIT_SUCCESS;
}

/* Load both regular and warning prompt, if enabled, from the prompt name NAME.
 * Return EXIT_SUCCESS if found or EXIT_FAILURE if not. */
int
expand_prompt_name(char *name)
{
	if (!name || !*name || prompts_n == 0)
		return EXIT_FAILURE;

	char *p = remove_quotes(name);
	if (!p || !*p || strchr(p, '\\')) /* Exclude prompt codes. */
		return EXIT_FAILURE;

	int i = (int)prompts_n;
	while (--i >= 0) {
		if (*p != *prompts[i].name || strcmp(p, prompts[i].name) != 0)
			continue;

		if (prompts[i].regular) {
			free(conf.encoded_prompt);
			conf.encoded_prompt = savestring(prompts[i].regular,
				strlen(prompts[i].regular));
		}

		if (prompts[i].warning) {
			free(conf.wprompt_str);
			conf.wprompt_str = savestring(prompts[i].warning,
				strlen(prompts[i].warning));
		}

		prompt_notif = prompts[i].notifications;

		if (xargs.warning_prompt != 0)
			conf.warning_prompt = prompts[i].warning_prompt_enabled;

		xstrsncpy(cur_prompt_name, prompts[i].name, sizeof(cur_prompt_name));

		return EXIT_SUCCESS;
	}

	return EXIT_FAILURE;
}

void
free_prompts(void)
{
	int i = (int)prompts_n;
	while (--i >= 0) {
		free(prompts[i].name);
		free(prompts[i].regular);
		free(prompts[i].warning);
	}
	free(prompts);
	prompts = (struct prompts_t *)NULL;
	prompts_n = 0;
}

static void
remove_virtual_dir(void)
{
	struct stat a;
	if (stdin_tmp_dir && stat(stdin_tmp_dir, &a) != -1) {
		xchmod(stdin_tmp_dir, "0700", 1);

		char *rm_cmd[] = {"rm", "-r", "--", stdin_tmp_dir, NULL};
		const int ret = launch_execv(rm_cmd, FOREGROUND, E_NOFLAG);
		if (ret != EXIT_SUCCESS)
			exit_code = ret;
		free(stdin_tmp_dir);
	}
	unsetenv("CLIFM_VIRTUAL_DIR");
}

/*
#if defined(__clang__)
// Free the storage associated with MAP
static void
xrl_discard_keymap(Keymap map)
{
	if (map == 0)
		return;

	int i;
	for (i = 0; i < KEYMAP_SIZE; i++) {
		switch (map[i].type) {
		case ISFUNC: break;

		case ISKMAP:
			// GCC (but not clang) complains about this if compiled with -pedantic
			// See discussion here: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=83584
			xrl_discard_keymap((Keymap)map[i].function);
			break;

		case ISMACR:
			// GCC (not clang) complains about this one too
			free((char *)map[i].function);
			break;
		}
	}
}
#endif // __clang__ */

void
free_workspaces_filters(void)
{
	size_t i;
	for (i = 0; i < MAX_WS; i++) {
		free(workspace_opts[i].filter.str);
		workspace_opts[i].filter.str = (char *)NULL;
		workspace_opts[i].filter.rev = 0;
		workspace_opts[i].filter.type = FILTER_NONE;
	}
}

void
save_last_path(char *last_path_tmp)
{
	if (config_ok == 0 || !config_dir || !config_dir_gral)
		return;

	char *last_path = xnmalloc(config_dir_len + 7, sizeof(char));
	snprintf(last_path, config_dir_len + 7, "%s/.last", config_dir);

	int fd = 0;
	FILE *last_fp = open_fwrite(last_path, &fd);
	if (!last_fp) {
		xerror(_("%s: Error saving last visited directory: %s\n"),
			PROGRAM_NAME, strerror(errno));
		free(last_path);
		return;
	}

	for (size_t i = 0; i < MAX_WS; i++) {
		if (!workspaces[i].path)
			continue;

		if ((size_t)cur_ws == i)
			fprintf(last_fp, "*%zu:%s\n", i, workspaces[i].path);
		else
			fprintf(last_fp, "%zu:%s\n", i, workspaces[i].path);
	}

	fclose(last_fp);

	if (conf.cd_on_quit == 1 && last_path_tmp
	&& symlinkat(last_path, XAT_FDCWD, last_path_tmp) == -1) {
		xerror(_("%s: cd-on-quit: Cannot create symbolic link '%s': %s\n"),
			PROGRAM_NAME, last_path_tmp, strerror(errno));
	}

	free(last_path);
}

/* Store last visited directory for the restore-last-path and the
 * cd-on-quit functions. Current workspace/path will be marked with an
 * asterisk. It will be read at startup by get_last_path() and at exit by
 * cd_on_quit.sh, if enabled. */
static void
handle_last_path(void)
{
	if (!config_dir_gral) /* NULL if running with --open or --preview */
		return;

	/* The cd_on_quit.sh function changes the shell current directory to the
	 * directory specifcied in ~/.config/clifm/.last if this file is found (it
	 * is actually a symlink to ~/.config/clifm/profiles/PROFILE/.last).
	 * Remove the file to prevent the function from changing the directory
	 * if cd-on-quit is disabled (e.g., not exiting via 'Q'). If necessary,
	 * it will be recreated by save_last_path() below. */
	const size_t len = strlen(config_dir_gral) + 7;
	char *last_path_tmp = xnmalloc(len, sizeof(char));
	snprintf(last_path_tmp, len, "%s/.last", config_dir_gral);

	struct stat a;
	if (lstat(last_path_tmp, &a) != -1
	&& unlinkat(XAT_FDCWD, last_path_tmp, 0) == -1)
		xerror("unlink: '%s': %s\n", last_path_tmp, strerror(errno));

	if (conf.restore_last_path == 1 || conf.cd_on_quit == 1)
		save_last_path(last_path_tmp);

	free(last_path_tmp);
}

/* This function is called by atexit() to clear whatever is there at exit
 * time and avoid thus memory leaks */
void
free_stuff(void)
{
	int i = 0;

	free(alt_config_dir);
	free(alt_trash_dir);
	free(alt_config_file);
	free(alt_bm_file);
	free(alt_kbinds_file);
	free(alt_preview_file);
	free(alt_profile);

#ifdef LINUX_FSINFO
	if (ext_mnt) {
		for (i = 0; ext_mnt[i].mnt_point; i++)
			free(ext_mnt[i].mnt_point);
		free(ext_mnt);
	}
#endif /* LINUX_FSINFO */

#ifdef RUN_CMD
	free(cmd_line_cmd);
#endif /* RUN_CMD */

#if !defined(_NO_ICONS)
	free(name_icons_hashes);
	free(dir_icons_hashes);
	free(ext_icons_hashes);
#endif /* !_NO_ICONS */

	free(conf.time_str);
	free(conf.ptime_str);

#ifdef LINUX_INOTIFY
	/* Shutdown inotify */
	if (inotify_wd >= 0)
		inotify_rm_watch(inotify_fd, inotify_wd);
	if (inotify_fd != UNSET)
		close(inotify_fd);
#elif defined(BSD_KQUEUE)
	if (event_fd >= 0)
		close(event_fd);
	if (kq != UNSET)
		close(kq);
#endif /* LINUX_INOTIFY */

	free_prompts();
	free(prompts_file);
	free_autocmds();
	free_tags();
	free_remotes(1);

	if (xargs.stealth_mode != 1)
		save_jumpdb();

	handle_last_path();

	free(bin_name);
	free_bookmarks();
	free(conf.encoded_prompt);
/*	free(right_prompt); */
	free_dirlist();
	free(conf.opener);
	free(conf.wprompt_str);
	free(conf.fzftab_options);
	free(conf.welcome_message_str);

	remove_virtual_dir();

	i = (int)cschemes_n;
	while (i-- > 0)
		free(color_schemes[i]);
	free(color_schemes);
	free(conf.usr_cscheme);

	if (jump_db) {
		i = (int)jump_n;
		while (--i >= 0)
			free(jump_db[i].path);
		free(jump_db);
	}

	if (pinned_dir)
		free(pinned_dir);

//	ADD FILTER TYPE CHECK!
	if (filter.str) {
		regfree(&regex_exp);
		free(filter.str);
	}

	if (conf.histignore_regex) {
		regfree(&regex_hist);
		free(conf.histignore_regex);
	}

	if (conf.dirhistignore_regex) {
		regfree(&regex_dirhist);
		free(conf.dirhistignore_regex);
	}

	free_workspaces_filters();

	if (profile_names) {
		for (i = 0; profile_names[i]; i++)
			free(profile_names[i]);
		free(profile_names);
	}

	if (sel_n > 0) {
		i = (int)sel_n;
		while (--i >= 0)
			free(sel_elements[i].name);
		free(sel_elements);
	}
	free(sel_devino);

	if (bin_commands) {
		i = (int)path_progsn;
		while (--i >= 0)
			free(bin_commands[i]);
		free(bin_commands);
	}

	if (paths) {
		i = (int)path_n;
		while (--i >= 0)
			free(paths[i].path);
		free(paths);
	}

	if (cdpaths) {
		i = (int)cdpath_n;
		while (--i >= 0)
			free(cdpaths[i]);
		free(cdpaths);
	}

	if (history) {
		i = (int)current_hist_n;
		while (--i >= 0)
			free(history[i].cmd);
		free(history);
	}

	if (dirhist_total_index) {
		i = (int)dirhist_total_index;
		while (--i >= 0)
			free(old_pwd[i]);
		free(old_pwd);
	}

	i = (int)aliases_n;
	while (--i >= 0) {
		free(aliases[i].name);
		free(aliases[i].cmd);
	}
	free(aliases);

	i = (int)kbinds_n;
	while (--i >= 0) {
		free(kbinds[i].function);
		free(kbinds[i].key);
	}
	free(kbinds);

	i = (int)usrvar_n;
	while (--i >= 0) {
		free(usr_var[i].name);
		free(usr_var[i].value);
	}
	free(usr_var);

	i = (int)actions_n;
	while (--i >= 0) {
		free(usr_actions[i].name);
		free(usr_actions[i].value);
	}
	free(usr_actions);

	i = (int)prompt_cmds_n;
	while (--i >= 0)
		free(prompt_cmds[i]);
	free(prompt_cmds);

	if (msgs_n) {
		i = (int)msgs_n;
		while (--i >= 0)
			free(messages[i]);
		free(messages);
	}

	if (ext_colors_n) {
		i = (int)ext_colors_n;
		while (--i >= 0) {
			free(ext_colors[i].name);
			free(ext_colors[i].value);
		}
		free(ext_colors);
	}

	if (workspaces && workspaces[0].path) {
		i = MAX_WS;
		while (--i >= 0) {
			if (workspaces[i].path)
				free(workspaces[i].path);
			if (workspaces[i].name)
				free(workspaces[i].name);
		}
		free(workspaces);
	}

	free(actions_file);
	free(bm_file);
	free(data_dir);
	free(colors_dir);
	free(config_dir_gral);
	free(config_dir);
	free(config_file);
	free(dirhist_file);
	free(hist_file);
	free(kbinds_file);
	free(msgs_log_file);
	free(cmds_log_file);
	free(mime_file);
	free(plugins_dir);
	free(profile_file);
	free(remotes_file);

#ifndef _NO_SUGGESTIONS
	free(suggestion_buf);
	free(conf.suggestion_strategy);
#endif /* !_NO_SUGGESTIONS */

	free(sel_file);
	free(tmp_rootdir);
	free(tmp_dir);
	free(user.name);
	free(user.home);
	free(user.shell);
	free(user.shell_basename);

	free(user.groups);

#ifndef _NO_TRASH
	free(trash_dir);
	free(trash_files_dir);
	free(trash_info_dir);
#endif /* !_NO_TRASH */
	free(tags_dir);
	free(conf.term);
	free(quote_chars);

	rl_clear_history();
	rl_free_undo_list();
	rl_clear_pending_input();

/*
#if defined(__clang__)
	Keymap km = rl_get_keymap();
	xrl_discard_keymap(km);
#endif // __clang__ */

#ifdef CLIFM_TEST_INPUT
	if (rl_instream)
		fclose(rl_instream);
#endif /* CLIFM_TEST_INPUT */

	/* Restore the color of the running terminal */
	if (conf.colorize == 1 && xargs.list_and_quit != 1)
		RESTORE_COLOR;

	int ret = 0;
	if ((ret = restore_shell()) < 0) {
		fprintf(stderr, "%s: tcsetattr: %s\n", PROGRAM_NAME, strerror(ret));
		exit(ret);
	}
}

/* Get current terminal dimensions and store them in TERM_COLS and
 * TERM_LINES (globals). These values will be updated upon SIGWINCH.
 * In case of error, we fallback to 80x24. */
void
get_term_size(void)
{
	struct winsize w;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1) {
		term_cols = 80;
		term_lines = 24;
		return;
	}

	term_cols =  w.ws_col > 0 ? w.ws_col : 80;
	term_lines = w.ws_row > 0 ? w.ws_row : 24;
}

#ifndef _BE_POSIX
/* Get new window size and update/refresh the screen accordingly */
static void
sigwinch_handler(int sig)
{
	UNUSED(sig);
	if (xargs.refresh_on_resize == 0 || conf.pager == 1 || kbind_busy == 1)
		return;

	get_term_size();
	flags |= DELAYED_REFRESH;
}
#endif /* _BE_POSIX */

void
set_signals_to_ignore(void)
{
	signal(SIGINT, SIG_IGN);  /* C-c */
	signal(SIGQUIT, SIG_IGN); /* C-\ */
	signal(SIGTSTP, SIG_IGN); /* C-z */
#ifndef _BE_POSIX
	signal(SIGWINCH, sigwinch_handler);
#endif /* _BE_POSIX */
}

static int
create_virtual_dir(const int user_provided)
{
	if (!stdin_tmp_dir || !*stdin_tmp_dir) {
		if (user_provided == 1) {
			err('e', PRINT_PROMPT, "%s: Empty buffer for virtual "
				"directory name. Trying with default value\n", PROGRAM_NAME);
		} else {
			err('e', PRINT_PROMPT, "%s: Empty buffer for virtual "
				"directory name\n", PROGRAM_NAME);
		}
		return EXIT_FAILURE;
	}

	char *cmd[] = {"mkdir", "-p", "--", stdin_tmp_dir, NULL};
	int ret = 0;
	if ((ret = launch_execv(cmd, FOREGROUND, E_MUTE)) != EXIT_SUCCESS) {
		char *errmsg = (ret == E_NOTFOUND ? NOTFOUND_MSG
			: (ret == E_NOEXEC ? NOEXEC_MSG : (char *)NULL));

		if (user_provided == 1) {
			err('e', PRINT_PROMPT, "%s: mkdir: '%s': %s. Trying with "
				"default value\n", PROGRAM_NAME, stdin_tmp_dir,
				errmsg ? errmsg : strerror(ret));
		} else {
			err('e', PRINT_PROMPT, "%s: mkdir: '%s': %s\n",
				PROGRAM_NAME, stdin_tmp_dir,
				errmsg ? errmsg : strerror(ret));
		}
		return ret;
	}

	return EXIT_SUCCESS;
}

int
handle_stdin(void)
{
	/* If files are passed via stdin, we need to disable restore
	 * last path in order to correctly understand relative paths */
	conf.restore_last_path = 0;
	int exit_status = EXIT_SUCCESS;

	/* Max input size: 512 * (512 * 1024)
	 * 512 chunks of 524288 bytes (512KiB) each
	 * == (65535 * PATH_MAX)
	 * == 262MiB of data ((65535 * PATH_MAX) / 1024) */

	size_t chunk = 512 * 1024,
		   chunks_n = 1,
		   total_len = 0,
		   max_chunks = 512;

	ssize_t input_len = 0;

	/* Initial buffer allocation == 1 chunk */
	char *buf = xnmalloc(chunk, sizeof(char));

	while (chunks_n < max_chunks) {
		input_len = read(STDIN_FILENO, buf + total_len, chunk);

		/* Error */
		if (input_len < 0) {
			free(buf);
			return EXIT_FAILURE;
		}

		/* Nothing else to be read */
		if (input_len == 0)
			break;

		total_len += (size_t)input_len;
		chunks_n++;

		/* Append a new chunk of memory to the buffer */
		buf = xnrealloc(buf, chunks_n + 1, chunk);
	}

	if (total_len == 0)
		goto FREE_N_EXIT;

	/* Null terminate the input buffer */
	buf[total_len] = '\0';

	/* Create tmp dir to store links to files */
	char *suffix = (char *)NULL;

	if (!stdin_tmp_dir || (exit_status = create_virtual_dir(1)) != EXIT_SUCCESS) {
		free(stdin_tmp_dir);

		suffix = gen_rand_str(RAND_SUFFIX_LEN);
		char *temp = tmp_dir ? tmp_dir : P_tmpdir;
		const size_t tmp_len = strlen(temp) + 13;
		stdin_tmp_dir = xnmalloc(tmp_len, sizeof(char));
		snprintf(stdin_tmp_dir, tmp_len, "%s/vdir.%s", temp,
			suffix ? suffix : "nTmp0B9&54");
		free(suffix);

		if ((exit_status = create_virtual_dir(0)) != EXIT_SUCCESS)
			goto FREE_N_EXIT;
	}

	if (xargs.stealth_mode != 1)
		setenv("CLIFM_VIRTUAL_DIR", stdin_tmp_dir, 1);

	/* Get CWD: we need it to prepend it to relative paths. */
	char t[PATH_MAX + 1] = "";
	char *cwd = get_cwd(t, sizeof(t), 0);

	if (!cwd || !*cwd) {
		exit_status = errno;
		goto FREE_N_EXIT;
	}

	/* Get substrings from buf */
	char *p = buf, *q = buf;
	size_t links_counter = 0;

	while (*p) {
		if (!*p || *p == '\n') {
			*p = '\0';

			/* Create symlinks (in tmp dir) to each valid file in the buffer */
			if (SELFORPARENT(q))
				goto END;

			struct stat attr;
			if (lstat(q, &attr) == -1) {
				err('w', PRINT_PROMPT, "%s: '%s': %s\n",
					PROGRAM_NAME, q, strerror(errno));
				goto END;
			}

			/* Construct source and destiny files */

			/* symlink(3) doesn't like file names ending with slash */
			const size_t slen = strlen(q);
			if (slen > 1 && q[slen - 1] == '/')
				q[slen - 1] = '\0';

			/* Should we construct destiny file as full path or using only the
			 * last path component (the file's basename)? */
			char *tmp_file = (char *)NULL;
			int free_tmp_file = 0;
			if (xargs.virtual_dir_full_paths != 1) {
				tmp_file = strrchr(q, '/');
				if (!tmp_file || !*(++tmp_file))
					tmp_file = q;
			} else {
				tmp_file = replace_slashes(q, ':');
				if (!tmp_file) {
					err('w', PRINT_PROMPT, "%s: '%s': Error formatting "
						"file name\n", PROGRAM_NAME, q);
					goto END;
				}
				free_tmp_file = 1;
			}

			char source[PATH_MAX + 1];
			if (*q != '/' || !q[1])
				snprintf(source, sizeof(source), "%s/%s", cwd, q);
			else
				xstrsncpy(source, q, sizeof(source));

			char dest[PATH_MAX + 1];
			snprintf(dest, sizeof(dest), "%s/%s", stdin_tmp_dir, tmp_file);

			if (symlink(source, dest) == -1) {
				if (errno == EEXIST && xargs.virtual_dir_full_paths != 1) {
					/* File already exists: append a random suffix */
					suffix = gen_rand_str(RAND_SUFFIX_LEN);
					char tmp[PATH_MAX + 12];
					snprintf(tmp, sizeof(tmp), "%s.%s",
						dest, suffix ? suffix : "#dn7R4.d6?");
					if (symlink(source, tmp) == -1)
						err('w', PRINT_PROMPT, "symlink: '%s': %s\n",
							q, strerror(errno));
					else
						err('w', PRINT_PROMPT, "symlink: '%s': Destiny exists. "
							"Created as %s\n", q, tmp);
					free(suffix);
				} else {
					err('w', PRINT_PROMPT, "symlink: '%s': %s\n",
						q, strerror(errno));
				}
			} else {
				links_counter++;
			}

			if (free_tmp_file == 1)
				free(tmp_file);

END:
			q = p + 1;
		}

		p++;
	}

	if (links_counter == 0) { /* No symlink was created. Exit */
		dup2(STDOUT_FILENO, STDIN_FILENO);
		err(0, NOPRINT_PROMPT, "%s: Empty file names buffer. "
			"Nothing to do\n", PROGRAM_NAME);
		if (getenv("CLIFM_VT_RUNNING"))
			press_any_key_to_continue(0);

		free(buf);
		exit(EXIT_FAILURE);
	}

	/* Make the virtual dir read only */
	xchmod(stdin_tmp_dir, "0500", 1);

	/* chdir to tmp dir and update path var */
	if (xchdir(stdin_tmp_dir, SET_TITLE) == -1) {
		exit_status = errno;
		xerror("cd: '%s': %s\n", stdin_tmp_dir, strerror(errno));

		xchmod(stdin_tmp_dir, "0700", 1);

		char *rm_cmd[] = {"rm", "-r", "--", stdin_tmp_dir, NULL};
		int ret = launch_execv(rm_cmd, FOREGROUND, E_NOFLAG);
		if (ret != EXIT_SUCCESS)
			exit_status = ret;

		free(cwd);
		goto FREE_N_EXIT;
	}

	if (workspaces[cur_ws].path)
		free(workspaces[cur_ws].path);

	workspaces[cur_ws].path = savestring(stdin_tmp_dir, strlen(stdin_tmp_dir));
	goto FREE_N_EXIT;

FREE_N_EXIT:
	free(buf);

	/* Go back to tty */
	dup2(STDOUT_FILENO, STDIN_FILENO);

	if (conf.autols == 1) {
		reload_dirlist();
		add_to_dirhist(workspaces[cur_ws].path);
	}

	return exit_status;
}

/* Save pinned directory into a file. */
static int
save_pinned_dir(void)
{
	if (!pinned_dir || config_ok == 0)
		return EXIT_FAILURE;

	char *pin_file = xnmalloc(config_dir_len + 7, sizeof(char));
	snprintf(pin_file, config_dir_len + 7, "%s/.pin", config_dir);

	int fd = 0;
	FILE *fp = open_fwrite(pin_file, &fd);
	if (!fp) {
		xerror(_("%s: Error storing pinned directory: %s\n"),
			PROGRAM_NAME, strerror(errno));
	} else {
		fprintf(fp, "%s", pinned_dir);
		fclose(fp);
	}

	free(pin_file);

	return EXIT_SUCCESS;
}

int
pin_directory(char *dir)
{
	if (!dir || !*dir) return EXIT_FAILURE;

	struct stat attr;
	if (lstat(dir, &attr) == -1) {
		xerror("%s: '%s': %s\n", PROGRAM_NAME, dir, strerror(errno));
		return EXIT_FAILURE;
	}

	if (pinned_dir)
		{free(pinned_dir); pinned_dir = (char *)NULL;}

	const size_t dir_len = strlen(dir);

	/* If absolute path */
	if (*dir == '/') {
		pinned_dir = savestring(dir, dir_len);
	} else { /* If relative path */
		if (strcmp(workspaces[cur_ws].path, "/") == 0) {
			pinned_dir = xnmalloc(dir_len + 2, sizeof(char));
			snprintf(pinned_dir, dir_len + 2, "/%s", dir);
		} else {
			const size_t plen = dir_len + strlen(workspaces[cur_ws].path) + 2;
			pinned_dir = xnmalloc(plen, sizeof(char));
			snprintf(pinned_dir, plen, "%s/%s", workspaces[cur_ws].path, dir);
		}
	}

	if (xargs.stealth_mode == 1 || save_pinned_dir() == EXIT_SUCCESS)
		goto END;

	free(pinned_dir);
	pinned_dir = (char *)NULL;
	return EXIT_FAILURE;

END:
	printf(_("%s: Succesfully pinned '%s'\n"), PROGRAM_NAME, dir);
	return EXIT_SUCCESS;
}

int
unpin_dir(void)
{
	if (!pinned_dir) {
		printf(_("%s: No pinned file\n"), PROGRAM_NAME);
		return EXIT_SUCCESS;
	}

	if (config_dir && xargs.stealth_mode != 1) {
		int cmd_error = 0;
		char *pin_file = xnmalloc(config_dir_len + 7, sizeof(char));
		snprintf(pin_file, config_dir_len + 7, "%s/.pin", config_dir);
		if (unlink(pin_file) == -1) {
			xerror("%s: '%s': %s\n", PROGRAM_NAME, pin_file, strerror(errno));
			cmd_error = 1;
		}

		free(pin_file);
		if (cmd_error == 1)
			return EXIT_FAILURE;
	}

	printf(_("Succesfully unpinned %s\n"), pinned_dir);

	free(pinned_dir);
	pinned_dir = (char *)NULL;
	return EXIT_SUCCESS;
}

void
free_software(void)
{
	puts(_(FREE_SOFTWARE));
}

void
version_function(void)
{
#ifndef _BE_POSIX
	const char *posix = "";
#else
	const char *posix = "-POSIX";
#endif /* _BE_POSIX */
	printf(_("%s %s%s (%s)\n%s\nLicense %s\nWritten by %s\n"), PROGRAM_NAME,
		VERSION, posix, DATE, CONTACT, LICENSE, AUTHOR);
}

void
splash(void)
{
	printf("\n%s%s\n\n%s%s\t\t       %s%s\n           %s\n",
		conf.colorize ? REG_CYAN : "", ASCII_LOGO_BIG, df_c,
		BOLD, df_c, PROGRAM_NAME_UPPERCASE, _(PROGRAM_DESC));

	HIDE_CURSOR;
	if (conf.splash_screen) {
		printf("\n            ");
		fflush(stdout);
		press_any_key_to_continue(0);
	} else {
		putchar('\n');
	}

	UNHIDE_CURSOR;
}

void
bonus_function(void)
{
	char *phrases[] = {
		"\"Vamos Boca Juniors Carajo!\" (La mitad + 1)",
		"\"Hey! Look behind you! A three-headed monkey! (G. Threepweed)",
		"\"Free as in free speech, not as in free beer\" (R. M. S)",
		"\"Nothing great has been made in the world without passion\" (G. W. F. Hegel)",
		"\"Simplicity is the ultimate sophistication\" (Leo Da Vinci)",
		"\"Yo vendí semillas de alambre de púa, al contado, y me lo agradecieron\" (Marquitos, 9 Reinas)",
		"\"I'm so happy, because today I've found my friends, they're in my head\" (K. D. Cobain)",
		"\"The best code is written with the delete key\" (Someone, somewhere, sometime)",
		"\"I'm selling these fine leather jackets\" (Indy)",
		"\"If you've been feeling increasingly stupid lately, you're not alone\" (Zak McKracken)",
		"\"I pray to God to make me free of God\" (Meister Eckhart)",
		"¡Truco y quiero retruco mierda!",
		"\"The are no facts, only interpretations\" (F. Nietzsche)",
		"\"This is a lie\" (The liar paradox)",
		"\"There are two ways to write error-free programs; only the third one works\" (Alan J. Perlis)",
		"The man who sold the world was later sold by the big G",
		"A programmer is always one year older than themself",
		"A smartphone is anything but smart",
		"And he did it: he killed the man who killed him",
		">++('>",
		":(){:|:&};:",
		"Keep it simple, stupid",
		"If ain't broken, brake it",
		"\"I only know that I know nothing\" (Socrates)",
		"(Learned) Ignorance is the true outcome of wisdom (Nicholas "
		"of Cusa)",
		"True intelligence is about questions, not answers",
		"Humanity is just an arrow released towards God",
		"Buzz is right: infinity is our only and ultimate aim",
		"That stain will never ever be erased (La 12)",
		"\"Una obra de arte no se termina, se abandona\" (L. J. Guerrero)",
		"At the beginning, software was hardware; but today hardware is "
		"being absorbed by software",
		"\"Juremos con gloria morir\"",
		"\"Given enough eyeballs, all bugs are shallow.\" (Linus' law)",
		"\"We're gonna need a bigger boat.\" (Caleb)",
		"\"Ein Verletzter, Alarm, Alarm!\"",
		"\"There is not knowledge that is not power\"",
		"idkfa",
		"This is the second best file manager I've ever seen!",
		"Winners don't use TUIs",
		"\"La inmortalidad es baladí\" (J. L. Borges)",
		"\"Computer updated [...] Establish communications, priority alpha\"",
		"\"Step one: find plans, step two: save world, step three: get out of my house!\" (Dr. Fred)",
		"\"Leave my loneliness unbroken!, quit the bust above my door! Quoth the raven: Nevermore.\" (E. A. Poe)",
		NULL};

	const size_t num = (sizeof(phrases) / sizeof(phrases[0])) - 1;
#ifndef HAVE_ARC4RANDOM
	srandom((unsigned int)time(NULL));
	puts(phrases[random() % (int)num]);
#else
	puts(phrases[arc4random_uniform((uint32_t)num)]);
#endif /* !HAVE_ARC4RANDOM */
}
