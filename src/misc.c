/* misc.c -- functions that do not fit in any other file */

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

#include "helpers.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#if defined(__NetBSD__) || defined(__FreeBSD__)
# include <sys/param.h>
# include <sys/sysctl.h>
#endif

#ifdef __OpenBSD__
typedef char *rl_cpvfunc_t;
# include <ereadline/readline/readline.h>
#else
# include <readline/readline.h>
#endif

/*
#if defined(__HAIKU__)
#include <private/libs/compat/freebsd_network/compat/sys/mount.h>
#include <private/libs/compat/freebsd_network/compat/sys/sysctl.h>
#endif */
#include <time.h>
#include <unistd.h>
#ifdef LINUX_INOTIFY
# include <sys/inotify.h>
#endif

/*
#ifdef __linux__
#include <libudev.h>
#endif */

#include "aux.h"
#include "bookmarks.h"
#include "checks.h"
#include "exec.h"
#include "history.h"
#include "init.h"
#include "jump.h"
#include "listing.h"
#include "navigation.h"
#include "readline.h"
#include "strings.h"
#include "remotes.h"
#include "messages.h"
#include "file_operations.h"

/* Set ELN color according to the current workspace */
void
set_eln_color(void)
{
	switch(cur_ws) {
	case 0: strcpy(el_c, *ws1_c ? ws1_c : DEF_EL_C); break;
	case 1: strcpy(el_c, *ws2_c ? ws2_c : DEF_EL_C); break;
	case 2: strcpy(el_c, *ws3_c ? ws3_c : DEF_EL_C); break;
	case 3: strcpy(el_c, *ws4_c ? ws4_c : DEF_EL_C); break;
	case 4: strcpy(el_c, *ws5_c ? ws5_c : DEF_EL_C); break;
	case 5: strcpy(el_c, *ws6_c ? ws6_c : DEF_EL_C); break;
	case 6: strcpy(el_c, *ws7_c ? ws7_c : DEF_EL_C); break;
	case 7: strcpy(el_c, *ws8_c ? ws8_c : DEF_EL_C); break;
	default: strcpy(el_c, DEF_EL_C); break;
	}
}

/* Custom POSIX implementation of GNU asprintf() modified to log program
 * messages.
 * MSG_TYPE is one of: 'e', 'f', 'w', 'n', zero (meaning this
 * latter that no message mark (E, W, or N) will be added to the prompt).
 * Messages with a msg_type on 'n' (or -1) are not logged
 * 'f' means that the message must be printed forcefully, even if identical
 * to the previous one, without printing any message mark.
 * MSG_TYPE also accepts -1 and -2 as values:
 * -1: Print the message but do not log it
 * -2: Log but do not store the message into the messages array
 * PROMPT_FLAG tells whether to print the message immediately before the next
 * prompt or rather in place.
 * Based on littlstar's xasprintf implementation:
 * https://github.com/littlstar/asprintf.c/blob/master/asprintf.c*/
__attribute__((__format__(__printf__, 3, 0)))
/* We use __attribute__ here to silence clang warning: "format string is
 * not a string literal" */
int
_err(int msg_type, int prompt_flag, const char *format, ...)
{
	va_list arglist, tmp_list;

	va_start(arglist, format);
	va_copy(tmp_list, arglist);
	int size = vsnprintf((char *)NULL, 0, format, tmp_list);
	va_end(tmp_list);

	if (size < 0) {
		va_end(arglist);
		return EXIT_FAILURE;
	}

	char *buf = (char *)xnmalloc((size_t)size + 1, sizeof(char));
	vsprintf(buf, format, arglist);
	va_end(arglist);

	/* If the new message is the same as the last message, skip it */
	if (msgs_n > 0 && msg_type != 'f' && strcmp(messages[msgs_n - 1], buf) == 0)
		{free(buf); return EXIT_SUCCESS;}

	if (buf) {
		if (msg_type >= 'e') {
			switch (msg_type) {
			case 'e': pmsg = ERROR; msgs.error++; break;
			case 'w': pmsg = WARNING; msgs.warning++; break;
			case 'n': pmsg = NOTICE; msgs.notice++; break;
			default: pmsg = NOMSG; break;
			}
		}

		int logme = msg_type == ERR_NO_LOG ? 0 : (msg_type == 'n' ? -1 : 1);
		int add_to_msgs_list = 1;
		if (msg_type == ERR_NO_STORE) {
			add_to_msgs_list = 0;
			logme = 1;
//			prompt_flag = NOPRINT_PROMPT;
		}
		log_msg(buf, prompt_flag, logme, add_to_msgs_list);

		free(buf);
		return EXIT_SUCCESS;
	}

	return EXIT_FAILURE;
}

/* Print format string MSG, as "> MSG" (colored), if autols is on, or just
 * as "MSG" if off.
 * This function is used to inform the user about changes that require a
 * a files list reload (either upon files or interface modifications) */
__attribute__((__format__(__printf__, 1, 0)))
int
print_reload_msg(const char *msg, ...)
{
	va_list arglist, tmp_list;

	va_start(arglist, msg);
	va_copy(tmp_list, arglist);
	int size = vsnprintf((char *)NULL, 0, msg, tmp_list);
	va_end(tmp_list);

	if (size < 0) {
		va_end(arglist);
		return EXIT_FAILURE;
	}

	if (autols == 1)
		printf("%s->%s ", mi_c, df_c);

	char *buf = (char *)xnmalloc((size_t)size + 1, sizeof(char));

	vsprintf(buf, msg, arglist);
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
		_err('w', PRINT_PROMPT, "%s: inotify: %s\n", PROGRAM_NAME, strerror(errno));
		return;
	}

	inotify_wd = inotify_add_watch(inotify_fd, workspaces[cur_ws].path, INOTIFY_MASK);
	if (inotify_wd > 0)
		watch = 1;
}

void
read_inotify(void)
{
	if (inotify_fd == UNSET)
		return;

	int i;
	struct inotify_event *event;
	char inotify_buf[EVENT_BUF_LEN];

	memset((void *)inotify_buf, '\0', EVENT_BUF_LEN);
	i = (int)read(inotify_fd, inotify_buf, EVENT_BUF_LEN);

	if (i <= 0) {
#ifdef INOTIFY_DEBUG
		puts("INOTIFY_RETURN");
#endif /* INOTIFY_DEBUG */
		return;
	}

	int ignore_event = 0, refresh = 0;
	for (char *ptr = inotify_buf;
	ptr + ((struct inotify_event *)ptr)->len < inotify_buf + i;
	ptr += sizeof(struct inotify_event) + event->len) {
		event = (struct inotify_event *)ptr;

#ifdef INOTIFY_DEBUG
		printf("%s (%u:%d): ", *event->name ? event->name : NULL, event->len, event->wd);
#endif /* INOTIFY_DEBUG */

		if (!event->wd) {
#ifdef INOTIFY_DEBUG
			puts("INOTIFY_BREAK");
#endif /* INOTIFY_DEBUG */
			break;
		}

		if (event->mask & IN_CREATE) {
#ifdef INOTIFY_DEBUG
			puts("IN_CREATE");
#endif /* INOTIFY_DEBUG */
			struct stat a;
			if (event->len && lstat(event->name, &a) != 0) {
				/* The file was created, but doesn't exist anymore */
				ignore_event = 1;
			}
		}

		/* A file was renamed */
		if (event->mask & IN_MOVED_TO) {
			int j = (int)files;
			while (--j >= 0) {
				if (*file_info[j].name == *event->name
				&& strcmp(file_info[j].name, event->name) == 0)
					break;
			}

			if (j < 0) {
				ignore_event = 0;
			} else {
				/* If destiny file name is already in the files list,
				 * ignore this event */
				ignore_event = 1;
			}
		}

		if (event->mask & IN_DELETE) {
#ifdef INOTIFY_DEBUG
			puts("IN_DELETE");
#endif /* INOTIFY_DEBUG */
			struct stat a;
			if (event->len && lstat(event->name, &a) == 0)
				/* The file was removed, but is still there (recreated) */
				ignore_event = 1;
		}

#ifdef INOTIFY_DEBUG
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
#endif /* INOTIFY_DEBUG */

		if (!ignore_event && (event->mask & INOTIFY_MASK))
			refresh = 1;
	}

	if (refresh) {
#ifdef INOTIFY_DEBUG
		puts("INOTIFY_REFRESH");
#endif /* INOTIFY_DEBUG */
		reload_dirlist();
	} else {
#ifdef INOTIFY_DEBUG
		puts("INOTIFY_RESET");
#endif /* INOTIFY_DEBUG */
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

	int i, refresh = 0;
	int count = kevent(kq, NULL, 0, event_data, 4096, &timeout);

	for (i = 0; i < count; i++) {
		if (event_data[i].fflags & KQUEUE_FFLAGS) {
			refresh = 1;
			break;
		}
	}

	if (refresh == 1) {
		reload_dirlist();
		return;
	}

	if (event_fd >= 0) {
		close(event_fd);
		event_fd = -1;
		watch = 0;
	}
}
#endif /* LINUX_INOTIFY */

void
set_term_title(char *str)
{
	int free_tmp = 0;
	char *tmp = (char *)NULL;
	tmp = home_tilde(str, &free_tmp);

	printf("\033]2;%s - %s\007", PROGRAM_NAME, tmp ? tmp : str);
	fflush(stdout);

	if (free_tmp == 1)
		free(tmp);
}

static int
unset_filter(void)
{
	if (!_filter) {
		puts(_("No filter set"));
		return EXIT_SUCCESS;
	}

	free(_filter);
	_filter = (char *)NULL;
	regfree(&regex_exp);
	if (autols == 1)
		reload_dirlist();
	puts(_("Filter unset"));
	filter_rev = 0;

	return EXIT_SUCCESS;
}

static int
compile_filter(void)
{
	if (regcomp(&regex_exp, _filter, REG_NOSUB | REG_EXTENDED) != EXIT_SUCCESS) {
		fprintf(stderr, _("%s: '%s': Invalid regular expression\n"),
		    PROGRAM_NAME, _filter);
		free(_filter);
		_filter = (char *)NULL;
		regfree(&regex_exp);
	} else {
		if (autols == 1)
			reload_dirlist();
		print_reload_msg(_("%s: New filter successfully set\n"), _filter);
	}

	return EXIT_SUCCESS;
}

int
filter_function(char *arg)
{
	if (!arg) {
		printf(_("Current filter: %c%s\n"), filter_rev ? '!' : 0,
				_filter ? _filter : "none");
		return EXIT_SUCCESS;
	}

	if (IS_HELP(arg)) {
		puts(_(FILTER_USAGE));
		return EXIT_SUCCESS;
	}

	if (*arg == 'u' && strcmp(arg, "unset") == 0)
		return unset_filter();

	if (_filter)
		free(_filter);

	regfree(&regex_exp);

	if (*arg == '!') {
		filter_rev = 1;
		arg++;
	} else {
		filter_rev = 0;
	}

	char *p = arg;
	if (*arg == '\'' || *arg == '"')
		p = remove_quotes(arg);

	_filter = savestring(p, strlen(p));

	return compile_filter();
}

/* Print either all tips (if ALL == 1) or just a random one (ALL == 0) */
void
print_tips(int all)
{
	const char *TIPS[] = {
		"Try the autocd and auto-open functions: run 'FILE' instead "
		"of 'open FILE' or 'cd FILE'",
		"Add a new entry to the mimelist file with 'mm edit' or F6",
		"Do not forget to take a look at the manpage",
		"Need more speed? Try the light mode (Alt-y)",
		"The Selection Box is shared among different instances of CliFM",
		"Select files here and there with the 's' command",
		"Use wildcards and regular expressions to select files: "
		"'s *.c' or 's .*\\.c$'",
		"ELN's and the 'sel' keyword work for shell commands as well: "
		"'ls -ld 1 sel'",
		"Press TAB to automatically expand an ELN: 's 2<TAB>' -> 's FILENAME'",
		"Easily copy everything in CWD into another directory: 's * && c sel ELN/DIR'",
		"Use ranges (ELN-ELN) to easily move multiple files: 'm 3-12 ELN/DIR'",
		"Trash files with a simple 't ELN'",
		"Get mime information for a file: 'mm info ELN'",
		"If too many files are listed, try enabling the pager ('pg on')",
		"Once in the pager, go backwards pressing the keyboard shortcut "
		"provided by your terminal emulator",
		"Once in the pager, press 'q' to stop it",
		"Press Alt-l to switch to long view mode",
		"Search for files using the slash command: '/*.png'",
		"The search function allows regular expressions: '/^c'",
		"Add a new bookmark by just entering 'bm a ELN/FILE'",
		"Use c, l, m, md, and r instead of cp, ln, mv, mkdir, and rm",
		"Access a remote file system using the 'net' command",
		"Manage default associated applications with the 'mime' command",
		"Go back and forth in the directory history with Alt-j and Alt-k "
		"or Shift-Left and Shift-Right",
		"Open a new instance of CliFM with the 'x' command: 'x ELN/DIR'",
		"Send a command directly to the system shell with ';CMD'",
		"Run the last executed command: '!!'",
		"Access the commands history list: '!<TAB>'",
		"Access the search patterns history list: '/<TAB>'",
		"Import aliases from file using 'alias import FILE'",
		"List available aliases by running 'alias'",
		"Create aliases to easily run your preferred commands (F10)",
		"Open and edit the configuration file: 'edit' or F10",
		"Get a brief description for each CliFM command by running 'cmd'",
		"Print the currently used color codes list by entering 'cc'",
		"Press Alt-. to toggle hidden files on/off",
		"List mountpoints by pressing Alt-m",
		"Disallow the use of shell commands with the -x option: 'clifm -x'",
		"Go to the root directory by just pressing Alt-r",
		"Go to the home directory by just pressing Alt-e",
		"Press F8 to open and edit the current color scheme",
		"Press F9 to open and edit the keybindings file",
		"Press F10 to open and edit the configuration file",
		"Press F11 to open and edit the bookmarks file",
		"Set the starting path: 'clifm PATH'",
		"Use the 'o' command to open files and directories: 'o 12'",
		"Open a file or directory by just typing its ELN or file name",
		"Bypass the resource opener specifying an application: '12 leafpad'",
		"Open a file and send it to the background: '24&'",
		"Create a custom prompt editing the prompts file ('prompt edit')",
		"Customize your color scheme: 'cs edit' or F6",
		"Open the bookmarks manager with Alt-b",
		"Chain commands using ; and &&: 's 2 7-10; r sel'",
		"Add emojis to the prompt by copying them to the Prompt line "
		"in the prompts file ('prompt edit')",
		"Create a new profile running 'pf add PROFILE' or 'clifm -P PROFILE'",
		"Switch profiles using 'pf set PROFILE'",
		"Delete a profile using 'pf del PROFILE'",
		"Copy selected files into CWD: 'c sel' or Ctrl-Alt-v",
		"Use 'p ELN' to print file properties for ELN",
		"Deselect all selected files with Alt-d",
		"Select all files in the current directory: 'Alt-a'",
		"Jump to the Selection Box by pressing Alt-s",
		"Restore trashed files using the 'u' command",
		"Empty the trash can: 't empty'",
		"Press Alt-g to toggle list-directories-first on/off",
		"Use the 'fc' command to toggle the files counter on/off",
		"Take a look at the splash screen with the 'splash' command",
		"Have some fun trying the 'bonus' command",
		"Launch the default system shell in CWD using ':' or ';'",
		"Use Alt-z and 'Alt-x to switch sorting methods",
		"Reverse sorting order using the 'rev' option: 'st rev'",
		"Compress and decompress files using the 'ac' and 'ad' "
		"commands respectively: 'ac sel' or 'ad FILE.zip'",
		"Rename multiple files at once: 'br *.txt'",
		"Need no more tips? Disable this feature in the configuration file",
		"Need root privileges? Launch a new instance of CliFM as root "
		"running the 'X' command (note the uppercase)",
#ifdef __linux__
		"Manage removable devices via the 'media' command",
#endif
		"Create a fresh configuration file: 'edit reset'",
		"Use 'ln edit' (or 'le') to edit symbolic links",
		"Change default keyboard shortcuts by editing the keybindings file (F9)",
		"Keep in sight previous and next visited directories enabling the "
		"DirhistMap option in the configuration file (F10)",
		"Leave no traces at all running in stealth mode (-S)",
		"Pin a file via the 'pin' command and then use it with the "
		"period keyword (,). Ex: 'pin DIR' and then 'cd ,'",
		"Switch between color schemes using the 'cs' command",
		"Try the 'j' command to quickly jump into any visited directory",
		"Switch workspaces by pressing Alt-[1-4]",
		"Use the 'ws' command to list available workspaces",
		"Take a look at available plugins using the 'actions' command",
		"Space is not needed: enter 'p12' instead of 'p 12'",
		"When searching or selecting files, use the exclamation mark "
		"to reverse the meaning of a pattern: 's !*.pdf'",
		"Enable the TrashAsRm option to prevent accidental deletions",
		"Don't like ELN's? Disable them using the -e command line switch",
		"Use the 'n' command to create multiple files and/or directories: 'n FILE DIR/'",
		"Add prompt commands via the 'promptcmd' keyword: 'edit' (F10)",
		"Need git integration? Consult the manpage",
		"Accept a given suggestion by pressing the Right arrow key",
		"Accept only the first suggested word by pressing Alt-f or Alt-Right",
		"Enter 'c sel' to copy selected files into the current directory",
		"Press Alt-q to delete the last typed word",
		"Check ELN ranges by pressing TAB: '1-12<TAB>'",
		"Operate on specific selected files: 'sel<TAB>'",
		"Use the 'ow' command to open a file with an specific application",
		"Limit the amount of files listed on the screen via the 'mf' command",
		"Set a maximum file name length for listed files via the MaxFilenameLen "
		"option in the configuration file (F10)",
		"Use the 'm' command to interactively rename a file: 'm 12'",
		"Set options on a per directory basis via the autocommands function",
		"Clean up non-ASCII file names using the 'bleach' command",
		"Running in an untrusted environment? Try the --secure-env and "
		"--secure-cmds command line switches",
		"Get help for any internal command via the -h or --help parameters: 'p -h'",
		"Run in disk usage analyzer mode using the -t command line switch",
		"Enable icons with 'icons on'",
		"Quickly change to a parent directory using the 'bd' command",
		"Use 'stats' to print statistics on files in the current directory",
		"Customize the warning prompt by setting WarningPrompt in the prompts file ('prompt edit')",
		"Enter '-' to run the fzfnav plugin (includes files preview)",
		"Create multiple links at once using the 'bl' command",
		"Organize your files using tags. Try 'tag --help'",
		"Remove files in bulk using a text editor with 'rr'",
		"Press Ctrl-Alt-l to toggle max file name length on/off",
		"Easily send files to a remote location with the 'cr' command",
		"Quickly switch prompts via 'prompt NAME'",
		"Press Alt-TAB to toggle the disk usage analyzer mode",
		"Press Ctrl-Alt-l to toggle max file name length on/off",
		"Fuzzy completion is supported: 'dwn<TAB> -> Downloads'. Enable it via '--fuzzy-match'",
		"Wildcards can be expanded via TAB: 's *.c<TAB>'",
		"Try the help topics: 'help <TAB>'",
		"List symlinks in the current directory: '=l<TAB>'. Enter 'help file-details' for more information",
		"Use PropFields in the configuration file to toggle fields on/off in long view mode",
		NULL};

	size_t tipsn = (sizeof(TIPS) / sizeof(TIPS[0])) - 1;

	if (all == 1) {
		size_t i;
		for (i = 0; i < tipsn; i++) {
			printf("%s%sTIP %zu%s: %s\n",
				colorize ? df_c : "", colorize ? BOLD : "",
				i, colorize ? df_c : "", TIPS[i]);
		}
		return;
	}

	srand((unsigned int)time(NULL));
	printf("%s%sTIP%s: %s\n", colorize ? df_c : "",
		colorize ? BOLD : "", colorize ? df_c : "",
		TIPS[rand() % (int)tipsn]);
}

/* Check whether the conditions to run the new_instance function are
 * fulfilled */
static inline int
check_new_instance_init_conditions(const char *dir, const int sudo)
{
	if (!term) {
		UNUSED(dir); UNUSED(sudo);
		fprintf(stderr, _("%s: Default terminal not set. Use the "
				"configuration file (F10) to set it\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	if (!(flags & GUI)) {
		UNUSED(dir); UNUSED(sudo);
		fprintf(stderr, _("%s: Function only available for graphical "
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
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "%s: %s: %s\n", PROGRAM_NAME, *dir, strerror(errno));
		return errno;
	}

	if (!S_ISDIR(attr.st_mode)) {
		fprintf(stderr, _("%s: %s: Not a directory\n"), PROGRAM_NAME, *dir);
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
		path_dir = (char *)xnmalloc(strlen(workspaces[cur_ws].path)
					+ strlen(*dir) + 2, sizeof(char));
		sprintf(path_dir, "%s/%s", workspaces[cur_ws].path, *dir);
		free(*dir);
		*dir = (char *)NULL;
	} else {
		path_dir = *dir;
	}

	return path_dir;
}

/* Get command to be executed by the new_instance function, only if
 * TERM (global) contains spaces. Otherwise, new_instance will try
 * TERM -e */
static char **
get_cmd(char *dir, char *_sudo, char *self, const int sudo)
{
	if (!strchr(term, ' '))	return (char **)NULL;

	char **tmp_term = get_substr(term, ' ');
	if (!tmp_term) return (char **)NULL;

	int i;
	for (i = 0; tmp_term[i]; i++);

	int num = i;
	char **cmd = (char **)xnmalloc((size_t)i + (sudo ? 4 : 3), sizeof(char *));

	for (i = 0; tmp_term[i]; i++) {
		cmd[i] = savestring(tmp_term[i], strlen(tmp_term[i]));
		free(tmp_term[i]);
	}
	free(tmp_term);

	i = num - 1;
	int plus = 1;

	if (sudo) {
		cmd[i + plus] = (char *)xnmalloc(strlen(self) + 1, sizeof(char));
		strcpy(cmd[i + plus], _sudo);
		plus++;
	}

	cmd[i + plus] = (char *)xnmalloc(strlen(self) + 1, sizeof(char));
	strcpy(cmd[i + plus], self);
	plus++;
	cmd[i + plus] = (char *)xnmalloc(strlen(dir) + 1, sizeof(char));
	strcpy(cmd[i + plus], dir);
	plus++;
	cmd[i + plus] = (char *)NULL;

	return cmd;
}

/* Launch a new instance using CMD. If CMD is NULL, try TERM -e
 * Returns the exit status of this execution */
static int
launch_new_instance_cmd(char ***cmd, char **self, char **_sudo, char **dir, int sudo)
{
	int ret = 0;

	if (*cmd) {
		ret = launch_execve(*cmd, BACKGROUND, E_NOFLAG);
		size_t i;
		for (i = 0; (*cmd)[i]; i++)
			free((*cmd)[i]);
		free(*cmd);
	} else {
		fprintf(stderr, _("%s: No option specified for '%s'\n"
				"Trying '%s -e %s %s'\n"), PROGRAM_NAME, term,
				term, *self, workspaces[cur_ws].path);
		if (sudo) {
			char *tcmd[] = {term, "-e", *_sudo, *self, *dir, NULL};
			ret = launch_execve(tcmd, BACKGROUND, E_NOFLAG);
		} else {
			char *tcmd[] = {term, "-e", *self, *dir, NULL};
			ret = launch_execve(tcmd, BACKGROUND, E_NOFLAG);
		}
	}

	free(*_sudo);
	free(*self);
	free(*dir);

	return ret;
}

/* After the last line of new_instance */
// cppcheck-suppress syntaxError

/* Open DIR in a new instance of the program (using TERM, set in the config
 * file, as terminal emulator) */
int
new_instance(char *dir, const int sudo)
{
	if (check_new_instance_init_conditions(dir, sudo) == EXIT_FAILURE)
		return EXIT_FAILURE;

	if (!dir)
		return EINVAL;

	char *_sudo = (char *)NULL;
	if (sudo && !(_sudo = get_sudo_path()))
		return errno;

	char *deq_dir = dequote_str(dir, 0);
	if (!deq_dir) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, _("%s: %s: Error dequoting file name\n"),
			PROGRAM_NAME, dir);
		free(_sudo);
		return EXIT_FAILURE;
	}

	char *self = get_cmd_path(PNL);
	if (!self) {
		free(_sudo);
		free(deq_dir);
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "%s: %s: %s\n", PROGRAM_NAME, PNL, strerror(errno));
		return errno;
	}

	int ret = check_dir(&deq_dir);
	if (ret != EXIT_SUCCESS) {
		free(deq_dir); free(self), free(_sudo);
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

	char rfile[PATH_MAX] = "";
	rfile[0] = '\0';

	if (*file == '~') {
		char *file_exp = tilde_expand(file);
		if (!file_exp) {
			_err(ERR_NO_STORE, NOPRINT_PROMPT, "%s: %s: %s\n", PROGRAM_NAME, file, strerror(errno));
			return EXIT_FAILURE;
		}

		if (realpath(file_exp, rfile) == NULL) {
			_err(ERR_NO_STORE, NOPRINT_PROMPT, "%s: %s: %s\n", PROGRAM_NAME,
				file_exp, strerror(errno));
			free(file_exp);
			return EXIT_FAILURE;
		}
		free(file_exp);
	} else {
		if (realpath(file, rfile) == NULL) {
			_err(ERR_NO_STORE, NOPRINT_PROMPT, "%s: %s: %s\n", PROGRAM_NAME,
				file, strerror(errno));
			return EXIT_FAILURE;
		}
	}

	if (rfile[0] == '\0') {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "%s: %s: %s\n", PROGRAM_NAME, file, strerror(errno));
		return EXIT_FAILURE;
	}

	/* Open the file to import aliases from */
	int fd;
	FILE *fp = open_fstream_r(rfile, &fd);
	if (!fp) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "%s: '%s': %s\n", PROGRAM_NAME, rfile, strerror(errno));
		return EXIT_FAILURE;
	}

	/* Open CliFM's config file as well */
	FILE *config_fp = fopen(config_file, "a");
	if (!config_fp) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "%s: %s: %s\n", PROGRAM_NAME,
			config_file, strerror(errno));
		close_fstream(fp, fd);
		return EXIT_FAILURE;
	}

	size_t line_size = 0, i;
	char *line = (char *)NULL;
	size_t alias_found = 0, alias_imported = 0;
	int first = 1;

	while (getline(&line, &line_size, fp) > 0) {
		if (*line == 'a' && strncmp(line, "alias ", 6) == 0) {
			alias_found++;

			/* If alias name conflicts with some internal command,
			 * skip it */
			char *alias_name = strbtw(line, ' ', '=');
			if (!alias_name)
				continue;

			if (is_internal_c(alias_name)) {
				fprintf(stderr, _("%s: Alias conflicts with internal command\n"), alias_name);
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
				fprintf(stderr, _("%s: Alias already exists\n"),
				    alias_name);
			}

			free(alias_name);
		}
	}

	free(line);
	close_fstream(fp, fd);
	fclose(config_fp);

	/* No alias was found in FILE */
	if (alias_found == 0) {
		fprintf(stderr, _("%s: %s: No alias found\n"), PROGRAM_NAME, rfile);
		return EXIT_FAILURE;
	}

	/* Aliases were found in FILE, but none was imported (either because
	 * they conflicted with internal commands or the alias already
	 * existed) */
	else {
		if (alias_imported == 0) {
			fprintf(stderr, _("%s: No alias imported\n"), PROGRAM_NAME);
			return EXIT_FAILURE;
		}
	}

	/* If some alias was found and imported, print the corresponding
	 * message and update the aliases array */
	if (alias_imported > 1) {
		printf(_("%s: %zu aliases were successfully imported\n"),
				PROGRAM_NAME, alias_imported);
	} else {
		printf(_("%s: 1 alias was successfully imported\n"), PROGRAM_NAME);
	}

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

/* Store last visited directory for the restore last path and the
 * cd on quit functions. Current workspace/path will be marked with an
 * asterisk. It will be read at startup by get_last_path */
void
save_last_path(void)
{
	if (config_ok == 0 || !config_dir || !config_dir_gral) return;

	char *last_dir = (char *)xnmalloc(config_dir_len + 7, sizeof(char));
	sprintf(last_dir, "%s/.last", config_dir);

	FILE *last_fp = fopen(last_dir, "w");
	if (!last_fp) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, _("%s: Error saving last visited directory: %s\n"),
			PROGRAM_NAME, strerror(errno));
		free(last_dir);
		return;
	}

	for (size_t i = 0; i < MAX_WS; i++) {
		if (workspaces[i].path) {
			if ((size_t)cur_ws == i)
				fprintf(last_fp, "*%zu:%s\n", i, workspaces[i].path);
			else
				fprintf(last_fp, "%zu:%s\n", i, workspaces[i].path);
		}
	}

	fclose(last_fp);

	char *last_dir_tmp = xnmalloc(strlen(config_dir_gral) + 7, sizeof(char *));
	sprintf(last_dir_tmp, "%s/.last", config_dir_gral);

	if (cd_on_quit == 1) {
		char *cmd[] = {"cp", "-p", last_dir, last_dir_tmp, NULL};
		launch_execve(cmd, FOREGROUND, E_NOFLAG);
	} else { /* If not cd on quit, remove the file */
		char *cmd[] = {"rm", "-f", "--", last_dir_tmp, NULL};
		launch_execve(cmd, FOREGROUND, E_NOFLAG);
	}

	free(last_dir_tmp);
	free(last_dir);
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
	size_t tmp_len = strlen(tmp), i;

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
create_usr_var(char *str)
{
	if (!str || !*str)
		return EXIT_FAILURE;

	char *p = strchr(str, '=');
	if (!p || p == str)
		return EXIT_FAILURE;

	*p = '\0';
	char *name = (char *)xnmalloc((size_t)(p - str + 1), sizeof(char));
	strcpy(name, str);
	*p = '=';

	char *value = parse_usrvar_value(str, '=');

	if (!value) {
		free(name);
		_err(ERR_NO_STORE, NOPRINT_PROMPT, _("%s: Error getting variable value\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	usr_var = xrealloc(usr_var, (size_t)(usrvar_n + 2) * sizeof(struct usrvar_t));
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
free_remotes(int exit)
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

int
expand_prompt_name(char *name)
{
	if (!name || !*name || prompts_n == 0)
		return EXIT_FAILURE;

	char *p = remove_quotes(name);
	if (!p || !*p || strchr(p, '\\'))
		return EXIT_FAILURE;

	int i = (int)prompts_n;
	while (--i >= 0) {
		if (*p != *prompts[i].name || strcmp(p, prompts[i].name) != 0)
			continue;
		if (prompts[i].regular) {
			free(encoded_prompt);
			encoded_prompt = savestring(prompts[i].regular, strlen(prompts[i].regular));
		}
		if (prompts[i].warning) {
			free(wprompt_str);
			wprompt_str = savestring(prompts[i].warning, strlen(prompts[i].warning));
		}
		prompt_notif = prompts[i].notifications;
		warning_prompt = prompts[i].warning_prompt_enabled;

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
		xchmod(stdin_tmp_dir, "0700");

		char *rm_cmd[] = {"rm", "-r", "--", stdin_tmp_dir, NULL};
		int ret = launch_execve(rm_cmd, FOREGROUND, E_NOFLAG);
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

/* This function is called by atexit() to clear whatever is there at exit
 * time and avoid thus memory leaks */
void
free_stuff(void)
{
	int i = 0;

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
#endif

	free_prompts();
	free(prompts_file);
	free_autocmds();
	free_tags();
	free_remotes(1);

	if (xargs.stealth_mode != 1)
		save_jumpdb();

	save_dirhist();

	if (restore_last_path || cd_on_quit)
		save_last_path();

	free(alt_profile);
	free_bookmarks();
	free(encoded_prompt);
/*	free(right_prompt); */
	free_dirlist();
	free(opener);
	free(wprompt_str);
	free(fzftab_options);

	remove_virtual_dir();

	i = (int)cschemes_n;
	while (i-- > 0)
		free(color_schemes[i]);
	free(color_schemes);
	free(usr_cscheme);

	if (jump_db) {
		i = (int)jump_n;
		while (--i >= 0)
			free(jump_db[i].path);
		free(jump_db);
	}

	if (pinned_dir)
		free(pinned_dir);

	if (_filter) {
		regfree(&regex_exp);
		free(_filter);
	}

	if (ext_colors_n)
		free(ext_colors_len);

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
			free(paths[i]);
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

	if (argv_bk) {
		i = argc_bk;
		while (--i >= 0)
			free(argv_bk[i]);
		free(argv_bk);
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
		while (--i >= 0)
			free(ext_colors[i]);
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
	free(log_file);
	free(mime_file);
	free(plugins_dir);
	free(profile_file);
	free(remotes_file);

#ifndef _NO_SUGGESTIONS
	free(suggestion_buf);
	free(suggestion_strategy);
#endif

	free(sel_file);
	free(tmp_dir);
	free(user.name);
	free(user.home);
	free(user.shell);
#ifndef _NO_TRASH
	free(trash_dir);
	free(trash_files_dir);
	free(trash_info_dir);
#endif
	free(tags_dir);
	free(term);
	free(quote_chars);
	rl_clear_history();
//	rl_clear_visible_line();

/*
#if defined(__clang__)
	Keymap km = rl_get_keymap();
	xrl_discard_keymap(km);
#endif // __clang__ */

	/* Restore the color of the running terminal */
	if (colorize == 1 && xargs.list_and_quit != 1)
		fputs("\x1b[0;39;49m", stdout);
}

/* Get current terminal dimensions and store them in TERM_COLS and
 * TERM_ROWS (globals). These values will be updated upon SIGWINCH */
void
get_term_size(void)
{
	struct winsize w;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
	term_cols = w.ws_col;
	term_rows = w.ws_row;
}

/* Get new window size and update/refresh the screen accordingly */
static void
sigwinch_handler(int sig)
{
	UNUSED(sig);
	if (xargs.refresh_on_resize == 0 || pager == 1 || kbind_busy == 1)
		return;

	get_term_size();
	flags |= DELAYED_REFRESH;
}

void
set_signals_to_ignore(void)
{
	signal(SIGINT, SIG_IGN);  /* C-c */
	signal(SIGQUIT, SIG_IGN); /* C-\ */
	signal(SIGTSTP, SIG_IGN); /* C-z */
	signal(SIGWINCH, sigwinch_handler);
/*	signal(SIGUSR1, sigusr_handler);
	signal(SIGUSR2, sigusr_handler); */
}

static int
create_virtual_dir(const int user_provided)
{
	if (!stdin_tmp_dir || !*stdin_tmp_dir) {
		if (user_provided == 1) {
			_err('e', PRINT_PROMPT, "%s: Empty buffer for virtual "
				"directory name. Trying with default value\n", PROGRAM_NAME);
		} else {
			_err('e', PRINT_PROMPT, "%s: Empty buffer for virtual "
				"directory name\n", PROGRAM_NAME);
		}
		return EXIT_FAILURE;
	}

	char *cmd[] = {"mkdir", "-p", "--", stdin_tmp_dir, NULL};
	int ret = 0;
	if ((ret = launch_execve(cmd, FOREGROUND, E_MUTE)) != EXIT_SUCCESS) {
		if (user_provided == 1) {
			_err('e', PRINT_PROMPT, "%s: mkdir: %s: %s. Trying with default value\n",
				PROGRAM_NAME, stdin_tmp_dir, strerror(ret));
		} else {
			_err('e', PRINT_PROMPT, "%s: mkdir: %s: %s\n",
				PROGRAM_NAME, stdin_tmp_dir, strerror(ret));
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
	restore_last_path = 0;
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
	char *buf = (char *)xnmalloc(chunk, sizeof(char));

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
		buf = (char *)xrealloc(buf, (chunks_n + 1) * chunk);
	}

	if (total_len == 0)
		goto FREE_N_EXIT;

	/* Null terminate the input buffer */
	buf[total_len] = '\0';

	/* Create tmp dir to store links to files */
	char *suffix = (char *)NULL;

	if (!stdin_tmp_dir || (exit_status = create_virtual_dir(1)) != EXIT_SUCCESS) {
		free(stdin_tmp_dir);

		suffix = gen_rand_str(6);
		char *temp = tmp_dir ? tmp_dir : P_tmpdir;
		stdin_tmp_dir = (char *)xnmalloc(strlen(temp) + 13, sizeof(char));
		sprintf(stdin_tmp_dir, "%s/vdir.%s", temp, suffix ? suffix : "nTmp0B");
		free(suffix);

		if ((exit_status = create_virtual_dir(0)) != EXIT_SUCCESS)
			goto FREE_N_EXIT;
	}

	if (xargs.stealth_mode != 1)
		setenv("CLIFM_VIRTUAL_DIR", stdin_tmp_dir, 1);

	/* Get CWD: we need it to prepend it to relative paths */
	char *cwd = (char *)NULL;
	cwd = getcwd(NULL, 0);
	if (!cwd) {
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
				_err('w', PRINT_PROMPT, "%s: %s: %s\n", PROGRAM_NAME, q, strerror(errno));
				goto END;
			}

			/* Construct source and destiny files */

			/* symlink(3) doesn't like file names ending with slash */
			size_t slen = strlen(q);
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
					_err('w', PRINT_PROMPT, "%s: %s: Error formatting file name\n",
						PROGRAM_NAME, q);
					goto END;
				}
				free_tmp_file = 1;
			}

			char source[PATH_MAX];
			if (*q != '/' || !q[1])
				snprintf(source, PATH_MAX, "%s/%s", cwd, q);
			else
				xstrsncpy(source, q, PATH_MAX);

			char dest[PATH_MAX + 1];
			snprintf(dest, PATH_MAX, "%s/%s", stdin_tmp_dir, tmp_file);

			if (symlink(source, dest) == -1) {
				if (errno == EEXIST && xargs.virtual_dir_full_paths != 1) {
					/* File already exists: append a random six digits suffix */
					suffix = gen_rand_str(6);
					char tmp[PATH_MAX + 8];
					snprintf(tmp, sizeof(tmp), "%s.%s", dest, suffix ? suffix : "#dn7R4");
					if (symlink(source, tmp) == -1)
						_err('w', PRINT_PROMPT, "symlink: %s: %s\n", q, strerror(errno));
					else
						_err('w', PRINT_PROMPT, "symlink: %s: Destiny exists. Created "
							"as %s\n", q, tmp);
					free(suffix);
				} else {
					_err('w', PRINT_PROMPT, "symlink: %s: %s\n", q, strerror(errno));
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
		_err(0, NOPRINT_PROMPT, "%s: Empty file names buffer. Nothing to do\n", PROGRAM_NAME);
		if (getenv("CLIFM_VT_RUNNING")) {
			fprintf(stderr, "Press any key to continue... ");
			xgetchar();
		}
		free(cwd);
		free(buf);
		exit(EXIT_FAILURE);
	}

	/* Make the virtual dir read only */
	xchmod(stdin_tmp_dir, "0500");

	/* chdir to tmp dir and update path var */
	if (xchdir(stdin_tmp_dir, SET_TITLE) == -1) {
		exit_status = errno;
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "cd: %s: %s\n", stdin_tmp_dir, strerror(errno));

		xchmod(stdin_tmp_dir, "0700");

		char *rm_cmd[] = {"rm", "-r", "--", stdin_tmp_dir, NULL};
		int ret = launch_execve(rm_cmd, FOREGROUND, E_NOFLAG);
		if (ret != EXIT_SUCCESS)
			exit_status = ret;

		free(cwd);
		goto FREE_N_EXIT;
	}

	free(cwd);

	if (workspaces[cur_ws].path)
		free(workspaces[cur_ws].path);

	workspaces[cur_ws].path = savestring(stdin_tmp_dir, strlen(stdin_tmp_dir));
	goto FREE_N_EXIT;

FREE_N_EXIT:
	free(buf);

	/* Go back to tty */
	dup2(STDOUT_FILENO, STDIN_FILENO);

	if (autols == 1) {
		reload_dirlist();
		add_to_dirhist(workspaces[cur_ws].path);
	}

	return exit_status;
}

/* Save pinned in a file */
static int
save_pinned_dir(void)
{
	if (!pinned_dir || config_ok == 0)
		return EXIT_FAILURE;

	char *pin_file = (char *)xnmalloc(config_dir_len + 7, sizeof(char));
	sprintf(pin_file, "%s/.pin", config_dir);

	FILE *fp = fopen(pin_file, "w");
	if (!fp) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, _("%s: Error storing pinned directory: %s\n"),
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
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "%s: %s: %s\n", PROGRAM_NAME, dir, strerror(errno));
		return EXIT_FAILURE;
	}

	if (pinned_dir)
		{free(pinned_dir); pinned_dir = (char *)NULL;}

	/* If absolute path */
	if (*dir == '/') {
		pinned_dir = savestring(dir, strlen(dir));
	} else { /* If relative path */
		if (strcmp(workspaces[cur_ws].path, "/") == 0) {
			pinned_dir = (char *)xnmalloc(strlen(dir) + 2, sizeof(char));
			sprintf(pinned_dir, "/%s", dir);
		} else {
			pinned_dir = (char *)xnmalloc(strlen(dir)
						+ strlen(workspaces[cur_ws].path) + 2, sizeof(char));
			sprintf(pinned_dir, "%s/%s", workspaces[cur_ws].path, dir);
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
		char *pin_file = (char *)xnmalloc(config_dir_len + 7, sizeof(char));
		sprintf(pin_file, "%s/.pin", config_dir);
		if (unlink(pin_file) == -1) {
			_err(ERR_NO_STORE, NOPRINT_PROMPT, "%s: %s: %s\n", PROGRAM_NAME,
				pin_file, strerror(errno));
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

/* Instead of recreating here the commands description, just jump to the
 * corresponding section in the manpage */
int
list_commands(void)
{
	char cmd[PATH_MAX];
	snprintf(cmd, PATH_MAX - 1, "export PAGER=\"less -p '^[0-9]+\\.[[:space:]]COMMANDS'\"; man %s\n",
			PNL);
	if (launch_execle(cmd) != EXIT_SUCCESS)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

#if !defined(__HAIKU__)
/* Retrieve pager path, first from PAGER, then try less(1), and finally
 * more(1). If none is found returns NULL */
static char *
get_pager(void)
{
	char *_pager = (char *)NULL;
	char *p = getenv("PAGER");
	if (p)
		_pager = savestring(p, strlen(p));
	else {
		p = get_cmd_path("less");
		if (p) {
			_pager = savestring(p, strlen(p));
			free(p);
		} else {
			p = get_cmd_path("more");
			if (p) {
				_pager = savestring(p, strlen(p));
				free(p);
			}
		}
	}

	return _pager;
}
#endif /* !__HAIKU__ */

/* Help topics */
static void
print_more_info(void)
{
	puts(_("For more information consult the manpage and/or the Wiki (https://github.com/leo-arch/clifm/wiki)"));
}

static int
print_archives_topic(void)
{
	puts(ARCHIVE_USAGE); return EXIT_SUCCESS;
}

static int
print_autocmds_topic(void)
{
	puts(AUTOCMDS_USAGE);
	putchar('\n');
	print_more_info();
	return EXIT_SUCCESS;
}

static int
print_basics_topic(void)
{
	puts(_("Run '?' and consult the BASIC FILE OPERATIONS section"));
	return EXIT_SUCCESS;
}

static int
print_bookmarks_topic(void)
{
	puts(BOOKMARKS_USAGE); return EXIT_SUCCESS;
}

static int
print_desktop_notifications_topic(void)
{
	puts(DESKTOP_NOTIFICATIONS_USAGE); return EXIT_SUCCESS;
}

static int
print_dir_jumper_topic(void)
{
	puts(JUMP_USAGE); return EXIT_SUCCESS;
}

static int
print_file_tags_topic(void)
{
	puts(TAG_USAGE); return EXIT_SUCCESS;
}

static int
print_file_attributes_topic(void)
{
	puts(FILE_DETAILS);
	putchar('\n');
	puts(FILE_SIZE_USAGE);
	putchar('\n');
	puts(FILTER_USAGE);
	return EXIT_SUCCESS;
}

static int
print_navigation_topic(void)
{
	puts(_("Run '?' and consult the NAVIGATION section"));
	return EXIT_SUCCESS;
}

static int
print_plugins_topic(void)
{
	puts(ACTIONS_USAGE);
	putchar('\n');
	print_more_info();
	return EXIT_SUCCESS;
}

static int
print_profiles_topic(void)
{
	puts(PROFILES_USAGE); return EXIT_SUCCESS;
}

static int
print_remotes_topic(void)
{
	puts(NET_USAGE); return EXIT_SUCCESS;
}

static int
print_resource_opener_topic(void)
{
	puts(MIME_USAGE); return EXIT_SUCCESS;
}

static int
print_selection_topic(void)
{
	puts(SEL_USAGE); return EXIT_SUCCESS;
}

static int
print_search_topic(void)
{
	puts(SEARCH_USAGE); return EXIT_SUCCESS;
}

static int
print_theming_topic(void)
{
	puts(_("Take a look at the colorscheme, prompt, and edit commands"));
	print_more_info();
	return EXIT_SUCCESS;
}

static int
print_trash_topic(void)
{
	puts(TRASH_USAGE); return EXIT_SUCCESS;
}

static int
run_help_topic(char *topic)
{
	if (*topic == 'a' && strcmp(topic, "archives") == 0)
		return print_archives_topic();
	if (*topic == 'a' && strcmp(topic, "autocommands") == 0)
		return print_autocmds_topic();
	if (*topic == 'b' && strcmp(topic, "basics") == 0)
		return print_basics_topic();
	if (*topic == 'b' && strcmp(topic, "bookmarks") == 0)
		return print_bookmarks_topic();
	if (*topic == 'd' && strcmp(topic, "desktop-notifications") == 0)
		return print_desktop_notifications_topic();
	if (*topic == 'd' && strcmp(topic, "dir-jumper") == 0)
		return print_dir_jumper_topic();
	if (*topic == 'f' && strcmp(topic, "file-details") == 0)
		return print_file_attributes_topic();
	if (*topic == 'f' && strcmp(topic, "file-tags") == 0)
		return print_file_tags_topic();
	if (*topic == 'n' && strcmp(topic, "navigation") == 0)
		return print_navigation_topic();
	if (*topic == 'p' && strcmp(topic, "plugins") == 0)
		return print_plugins_topic();
	if (*topic == 'p' && strcmp(topic, "profiles") == 0)
		return print_profiles_topic();
	if (*topic == 'r' && strcmp(topic, "remotes") == 0)
		return print_remotes_topic();
	if (*topic == 'r' && strcmp(topic, "resource-opener") == 0)
		return print_resource_opener_topic();
	if (*topic == 's' && strcmp(topic, "selection") == 0)
		return print_selection_topic();
	if (*topic == 's' && strcmp(topic, "search") == 0)
		return print_search_topic();
	if (*topic == 't' && strcmp(topic, "theming") == 0)
		return print_theming_topic();
	if (*topic == 't' && strcmp(topic, "trash") == 0)
		return print_trash_topic();

	fprintf(stderr, "%s: help: %s: No such help topic\n", PROGRAM_NAME, topic);
	return EXIT_FAILURE;
}

int
quick_help(char *topic)
{
	if (topic && *topic)
		return run_help_topic(topic);

#ifdef __HAIKU__
	printf("%s                                %s\n\n%s",
		ASCII_LOGO, _PROGRAM_NAME, QUICK_HELP);
	puts(_("\nNOTE: Some keybindings on Haiku might differ. Take a look "
		"at your current keybindings via the 'kb' command"));
	return EXIT_SUCCESS;
#else
	char *_pager = (char *)NULL;
	if (xargs.stealth_mode == 1 || !(_pager = get_pager())) {
		printf("%s                                %s\n\n%s",
			ASCII_LOGO, _PROGRAM_NAME, QUICK_HELP);
		return EXIT_SUCCESS;
	}

	char tmp_file[PATH_MAX];
	snprintf(tmp_file, PATH_MAX - 1, "%s/%s", xargs.stealth_mode == 1
		? P_tmpdir : tmp_dir, TMP_FILENAME);

	int fd = mkstemp(tmp_file);
	if (fd == -1) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "%s: %s: Error creating temporary file: %s\n",
			PROGRAM_NAME, tmp_file, strerror(errno));
		free(_pager);
		return EXIT_FAILURE;
	}

	FILE *fp;
	fp = open_fstream_w(tmp_file, &fd);
	if (!fp) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "%s: fopen: %s: %s\n", PROGRAM_NAME,
			tmp_file, strerror(errno));
		free(_pager);
		return EXIT_FAILURE;
	}

	dprintf(fd, "%s                                %s\n\n%s",
			ASCII_LOGO, _PROGRAM_NAME, QUICK_HELP);

	int ret;
	if (*_pager == 'l' && strcmp(_pager, "less") == 0) {
		char *cmd[] = {_pager, "-FIRX", tmp_file, NULL};
		ret = launch_execve(cmd, FOREGROUND, E_NOFLAG);
	} else {
		char *cmd[] = {_pager, tmp_file, NULL};
		ret = launch_execve(cmd, FOREGROUND, E_NOFLAG);
	}
	unlink(tmp_file);

	close_fstream(fp, fd);
	free(_pager);

	if (ret != EXIT_SUCCESS)
		return ret;

	if (autols == 1)
		reload_dirlist();
	return EXIT_SUCCESS;
#endif
}

void
help_function(void)
{
	fputs(NC, stdout);
	printf("%s\n", ASCII_LOGO);
	printf(_("%s %s (%s), by %s\n"), PROGRAM_NAME, VERSION, DATE, AUTHOR);
	printf("\nUSAGE: %s %s\n%s%s", PNL, GRAL_USAGE, _(SHORT_OPTIONS), _(LONG_OPTIONS));

	puts(_(CLIFM_COMMANDS));
	puts(_(CLIFM_KEYBOARD_SHORTCUTS));
	puts(_(HELP_END_NOTE));
}

void
free_software(void)
{
	puts(_(FREE_SOFTWARE));
}

void
version_function(void)
{
	printf(_("%s %s (%s)\n%s\nLicense %s\nWritten by %s\n"), PROGRAM_NAME,
		VERSION, DATE, CONTACT, LICENSE, AUTHOR);
}

void
splash(void)
{
	printf("\n%s%s\n\n%s%s\t\t       %s%s\n           %s\n",
		colorize ? D_CYAN : "", ASCII_LOGO_BIG, df_c,
		BOLD, df_c, _PROGRAM_NAME, _(PROGRAM_DESC));

	if (splash_screen) {
		printf(_("\n            Press any key to continue... "));
		xgetchar();
		putchar('\n');
	} else {
		putchar('\n');
	}
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
	    "\"The best code is written with the delete key (Someone, somewhere, sometime)",
	    "\"I'm selling these fine leather jackets (Indy)",
	    "\"I pray to God to make me free of God\" (Meister Eckhart)",
	    "¡Truco y quiero retruco mierda!",
	    "The only truth is that there is no truth",
	    "\"This is a lie\" (The liar paradox)",
	    "\"There are two ways to write error-free programs; only the third one works\" (Alan J. Perlis)",
	    "The man who sold the world was later sold by the big G",
	    "A programmer is always one year older than herself",
	    "A smartphone is anything but smart",
	    "And he did it: he killed the one who killed him",
	    ">++('>",
	    ":(){:|:&};:",
	    "Keep it simple, stupid",
	    "If ain't broken, brake it",
	    "\"I only know that I know nothing\" (Socrates)",
	    "(Learned) Ignorance is the true outcome of wisdom (Nicholas "
	    "of Cusa)",
	    "True intelligence is about questions, not about answers",
	    "Humanity is just an arrow released towards God",
	    "Buzz is right: infinity is our only and ultimate goal",
	    "That stain will never ever be erased (La 12)",
	    "\"A work of art is never finished, but adandoned\" (J. L. Guerrero)",
	    "At the beginning, software was hardware; but today hardware is "
	    "being absorbed by software",
	    "\"What you're referring to as Linux, is in fact, GNU/Linux\" (RMS)",
	    "\"Given enough eyeballs, all bugs are shallow.\" (Linus's law)",
	    "\"We're gonna need a bigger boat.\" (Caleb)",
	    "\"Ein Verletzter, Alarm, Alarm!\"",
	    "\"There is not knowledge that is not power\"",
	    NULL};

	size_t num = (sizeof(phrases) / sizeof(phrases[0])) - 1;
	srand((unsigned int)time(NULL));
	puts(phrases[rand() % (int)num]);
}
