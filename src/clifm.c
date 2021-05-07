
			/*  ########################################
			 *  #               CliFM                  #
			 *  # The anti-eye-candy/KISS file manager #
			 *  ######################################## */

/*
 *  GPL2+ License
 *
 * Copyright (C) 2016-2021, L. Abramovich <johndoe.arch@outlook.com>
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
 *
 */

#define VERSION "1.0"
#define AUTHOR "L. Abramovich"
#define CONTACT "johndoe.arch@outlook.com"
#define WEBSITE "https://github.com/leo-arch/clifm"
#define DATE "April 10, 2021"
#define LICENSE "GPL2+"

#if defined(__linux__) && !defined(_BE_POSIX)
#  define _GNU_SOURCE
#else
#  define _POSIX_C_SOURCE 200809L
#  define _DEFAULT_SOURCE
#  if __FreeBSD__
#    define __XSI_VISIBLE 700
#    define __BSD_VISIBLE 1
#  endif
#endif

/* Support large files on ARM or 32-bit machines */
#if defined(__arm__) || defined(__i386__)
#   define _FILE_OFFSET_BITS 64
#endif

/* #define __SIZEOF_WCHAR_T__ 4 */

/* Linux */
/* The only Linux-specific function I use, and the only function
 * requiring _GNU_SOURCE, is statx(), and only to get files creation
 * (birth) date in the properties function.
 * However, the statx() is conditionally added at compile time, so that,
 * if _BE_POSIX is defined (pass the -D_BE_POSIX option to the compiler),
 * the program will just ommit the call to the function, being thus
 * completely POSIX-2008 compliant. */

/* DEFAULT_SOURCE enables strcasecmp() and realpath() functions,
and DT_DIR (and company) and S_ISVTX macros */

/* FreeBSD*/
/* Up tp now, two features are disabled in FreeBSD: file capabilities and
 * immutable bit checks */

/* The following C libraries are located in /usr/include */
#include <stdio.h> /* (f)printf, s(n)printf, scanf, fopen, fclose, unlink,
					fgetc, fputc, perror, rename, sscanf, getline */
#include <string.h> /* str(n)cpy, str(n)cat, str(n)cmp, strlen, strstr,
					memset */
#include <stdlib.h> /* getenv, malloc, calloc, free, atoi, realpath,
					EXIT_FAILURE and EXIT_SUCCESS macros */
#include <stdarg.h> /* va_list */
#include <dirent.h> /* scandir */
#include <unistd.h> /* sleep, readlink, chdir, symlink, access, exec,
					* isatty, setpgid, getpid, getuid, gethostname,
					* tcsetpgrp, tcgetattr, __environ, STDIN_FILENO,
					* STDOUT_FILENO, and STDERR_FILENO macros */
#include <sys/stat.h> /* stat, lstat, mkdir */
#include <sys/wait.h> /* waitpid, wait */
#include <sys/ioctl.h> /* ioctl */
#include <sys/acl.h> /* acl_get_file(), acl_get_entry() */
#include <time.h> /* localtime, strftime, clock (to time functions) */
#include <grp.h> /* getgrgid */
#include <signal.h> /* trap signals */
#include <pwd.h> /* getcwd, getpid, geteuid, getpwuid */
#include <glob.h> /* glob */
#include <termios.h> /* struct termios */
#include <locale.h> /* setlocale */
#include <errno.h>
#include <getopt.h> /* getopt_long */
#include <fcntl.h> /* O_RDONLY, O_DIRECTORY, and AT_* macros */
#include <readline/history.h>
#include <readline/readline.h>
/* Readline: This function allows the user to move back and forth with
 * the arrow keys in the prompt. I've tried scanf, getchar, getline,
 * fscanf, fgets, and none of them does the trick. Besides, readline
 * provides TAB completion and history. Btw, readline is what Bash uses
 * for its prompt */
#include <libintl.h> /* gettext */
#include <limits.h>

#if __linux__
#  include <sys/capability.h> /* cap_get_file. NOTE: This header exists
in FreeBSD, but is deprecated */
#  include <linux/limits.h> /* PATH_MAX (4096), NAME_MAX (255) macros */
#  include <linux/version.h> /* LINUX_VERSION_CODE && KERNEL_VERSION
								macros */
#  include <linux/fs.h> /* FS_IOC_GETFLAGS, S_IMMUTABLE_FL macros */
#endif /* __linux__ */

#include <uchar.h> /* char32_t and char16_t types */

#include <wordexp.h> /* For command substitution */
#include <sys/statvfs.h>
#include <regex.h>
#include <wchar.h>

/* _GNU_SOURCE is only defined if __linux__ is defined and _BE_POSIX
 * is not defined */
#ifdef _GNU_SOURCE
#  if (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 28))
#    if LINUX_VERSION_CODE >= KERNEL_VERSION(4,11,0)
#      define _STATX
#    endif /* LINUX_VERSION (4.11) */
#  endif /* __GLIBC__ */
#endif /* _GNU_SOURCE */

/* Because capability.h is deprecated in BSD */
#if __linux__
#  if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
#    define _LINUX_CAP
#  endif /* LINUX_VERSION (2.6.24)*/
#endif /* __linux__ */

#include "clifm.h" /* A few custom functions */
#include "icons.h"
#include "helpers.h"
#include "globals.h"
#include "xfunctions.h"

#define VERSION "1.0"
#define AUTHOR "L. Abramovich"
#define CONTACT "johndoe.arch@outlook.com"
#define WEBSITE "https://github.com/leo-arch/clifm"
#define DATE "April 10, 2021"
#define LICENSE "GPL2+"

//
// global variables
//

/* Without this variable, TCC complains that __dso_handle is an
 * undefined symbol and won't compile */
#if __TINYC__
void* __dso_handle;
#endif

//
// Forward declarations
//

static int bookmarks_function(char **);
static int exec_cmd(char **);
static int list_dir(void);
static int mime_import(char *);
static int mime_edit(char **);
static char **parse_input_str(char *);

//
// function definitions
//

static char *
home_tilde(const char *new_path)
/* Reduce "$HOME" to tilde ("~"). The new_path variable is always either
 * "$HOME" or "$HOME/file", that's why there's no need to check for
 * "/file" */
{
	if (!home_ok || !new_path || !*new_path)
		return (char *)NULL;

	char *path_tilde = (char *)NULL;

	/* If path == HOME */
	if (new_path[1] == user_home[1] && strcmp(new_path, user_home) == 0) {
		path_tilde = (char *)xnmalloc(2, sizeof(char));
		path_tilde[0] = '~';
		path_tilde[1] = '\0';
	}

	/* If path == HOME/file */
	else if (new_path[1] == user_home[1]
	&& strncmp(new_path, user_home, user_home_len) == 0) {
		path_tilde = (char *)xnmalloc(strlen(new_path + user_home_len + 1) + 3,
									  sizeof(char));
		sprintf(path_tilde, "~/%s", new_path + user_home_len + 1);
	}

	else {
		path_tilde = (char *)xnmalloc(strlen(new_path) + 1, sizeof(char));
		strcpy(path_tilde, new_path);
	}

	return path_tilde;
}

static inline void
set_term_title(const char *dir)
{
	printf("\033]2;%s - %s\007", PROGRAM_NAME, dir);
	fflush(stdout);
}

static int
xchdir(const char *dir, const int set_title)
/* Make sure DIR exists, it is actually a directory and is readable.
 * Only then change directory */
{
	DIR *dirp = opendir(dir);

	if (!dirp)
		return -1;

	closedir(dirp);

	int ret;
	ret = chdir(dir);

	if (set_title && ret == 0 && xargs.cwd_in_title == 1) {
		if (*dir == '/' && dir[1] == 'h') {
			char *tmp = home_tilde(dir);
			if (tmp) {
				set_term_title(tmp);
				free(tmp);
			}
			else
				set_term_title(dir);
		}
		else
			set_term_title(dir);
	}

	return ret;
}

static int
is_number(const char *restrict str)
/* Check whether a given string contains only digits. Returns 1 if true
 * and 0 if false. It does not work with negative numbers */
{
	for (;*str; str++)
		if (*str > '9' || *str < '0')
			return 0;

	return 1;
}

static char *
get_size_unit(off_t size)
/* Convert FILE_SIZE to human readeable form */
{
	size_t max = 9;
	/* Max size type length == 9 == "1023.99K\0" */
	char *p = malloc(max * sizeof(char));

	if (!p)
		return (char *)NULL;

	char *str = p;
	p = (char *)NULL;

	size_t n = 0;
	float s = (float)size;

	while (s > 1024) {
		s = s/1024;
		++n;
	}

	int x = (int)s;
	/* If x == s, then s is an interger; else, it's float
	 * We don't want to print the reminder when it is zero */

	const char *const u = "BKMGTPEZY";
	snprintf(str, max, "%.*f%c", (s == x) ? 0 : 2, (double)s, u[n]);

	return str;
}

static char *
fastback(const char *str)
/* Convert ... n into ../.. n */
{
	if (!str || !*str)
		return (char *)NULL;

	char *p = (char *)str;
	size_t dots = 0;

	char *rem = (char *)NULL;
	while (*p) {
		if (*p != '.') {
			rem = p;
			break;
		}
		dots++;
		p++;
	}

	if (dots <= 2)
		 return (char *)NULL;

	char *q = (char *)NULL;
	if (rem)
		q = (char *)xnmalloc((dots * 3 + strlen(rem) + 2), sizeof(char));
	else
		q = (char *)xnmalloc((dots * 3), sizeof(char));
	
	q[0] = '.';
	q[1] = '.';

	size_t i, c = 2;
	for (i = 2; c < dots;) {
		q[i++] = '/';
		q[i++] = '.';
		q[i++] = '.';
		c++;
	}

	q[i] = '\0';

	if (rem) {
		if (*rem != '/') {
			q[i] = '/';
			q[i + 1] = '\0';
		}
		strcat(q, rem);
	}

	return q;
}

static char *
xitoa(int n)
/* Transform an integer (N) into a string of chars */
{
	static char buf[32] = {0};
	int i = 30, rem;

	if (!n)
		return "0";

	while (n && i) {
		rem = n / 10;
		buf[i] = '0' + (n - (rem * 10));
		n = rem;
		--i;
	}

	return &buf[++i];
}

static int
is_internal_c(const char *restrict cmd)
/* Check CMD against a list of internal commands */
{
	const char *int_cmds[] = {
		"?", "help",
		"ac", "ad",
		"acd", "autocd",
		"actions",
		"alias",
		"ao", "auto-open",
		"b", "back",
		"bh", "fh",
		"bm", "bookmarks",
		"br", "bulk",
		"c", "cp",
		"cc", "colors",
		"cd",
		"cl", "columns",
		"cmd", "commands",
		"cs", "colorschemes",
		"ds", "desel",
		"edit",
		"exp", "export",
		"ext",
		"f", "forth",
		"fc",
		"ff", "folders-first",
		"fs",
		"ft", "filter",
		"history",
		"hf", "hidden",
		"icons",
		"jump", "je", "jc", "jp", "jo",
		"kb", "keybinds",
		"l", "ln", "le",
		"lm",
		"log",
		"m", "mv",
		"md", "mkdir",
		"mf",
		"mm", "mime",
		"mp", "mountpoints",
		"msg", "messages",
		"n", "net",
		"o", "open",
		"opener",
		"p", "pp", "pr", "prop",
		"path", "cwd",
		"pf", "prof", "profile",
		"pg", "pager",
		"pin", "unpin",
		"r", "rm",
		"rf", "refresh",
		"rl", "reload",
		"s", "sel",
		"sb", "selbox",
		"shell",
		"splash",
		"st", "sort",
		"t", "tr", "trash",
		"te",
		"tips",
		"touch",
		"u", "undel", "untrash",
		"uc", "unicode",
		"unlink",
		"ver", "version",
		"ws",
		"x", "X",
		NULL };

	int found = 0;
	int i = (int)(sizeof(int_cmds) / sizeof(char *)) - 1;

	while (--i >= 0) {
		if (*cmd == *int_cmds[i] && strcmp(cmd, int_cmds[i]) == 0) {
			found = 1;
			break;
		}
	}

	if (found)
		return 1;

	/* Check for the search and history functions as well */
	else if ((*cmd == '/' && access(cmd, F_OK) != 0)
	|| (*cmd == '!' && (_ISDIGIT(cmd[1])
	|| (cmd[1] == '-' && _ISDIGIT(cmd[2])) || cmd[1] == '!')))
		return 1;

	return 0;
}

static char *
split_fusedcmd(char *str)
{
	if (!str || !*str || *str == ';' || *str == ':' || *str == '\\')
		return (char *)NULL;

	if (strchr(str, '/'))
		return (char *)NULL;

	/* The buffer size is the double of STR, just in case each subtr
	 * needs to be splitted */
	char *buf = (char *)xnmalloc(((strlen(str) * 2) + 2), sizeof(char));

	char *p = str, *pp = str;
	char *q = buf;
	size_t c = 0;

	while(*p) {
		/* Transform "cmdeln" into "cmd eln" */
		if (*p > '0' && *p <= '9' && c && *(p - 1) >= 'a'
		&& *(p - 1) <= 'z') {

			char tmp = *p;
			*p = '\0';

			if (!is_internal_c(pp)) {
				*p = tmp;
				*(q++) = *(p++);
				continue;
			}

			*p = tmp;
			*(q++) = ' ';
			*(q++) = *(p++);
		}

		else {
			if (*p == ' ' && *(p + 1))
				pp = p + 1;
			*(q++) = *(p++);
		}

		c++;
	}

	*q = '\0';

	/* Readjust the buffer size */
	size_t len = strlen(buf);
	buf = (char *)xrealloc(buf, (len + 1) * sizeof(char));

	return buf;
}

static int
run_in_foreground(pid_t pid)
{
	int status = 0;

	/* The parent process calls waitpid() on the child */
	if (waitpid(pid, &status, 0) > 0) {
		if (WIFEXITED(status) && !WEXITSTATUS(status)) {
			/* The program terminated normally and executed successfully
			 * (WEXITSTATUS(status) == 0) */
			return EXIT_SUCCESS;
		}
		else if (WIFEXITED(status) && WEXITSTATUS(status)) {
			/* Program terminated normally, but returned a
			 * non-zero status. Error codes should be printed by the
			 * program itself */
			return WEXITSTATUS(status);
		}
		else {
			/* The program didn't terminate normally. In this case too,
			 * error codes should be printed by the program */
			return EXCRASHERR;
		}
	}
	else {
		/* waitpid() failed */
		fprintf(stderr, "%s: waitpid: %s\n", PROGRAM_NAME,
				strerror(errno));
		return errno;
	}

	return EXIT_FAILURE; /* Never reached */
}

static void
run_in_background(pid_t pid)
{
	int status = 0;
	/* Keep it in the background */
	waitpid(pid, &status, WNOHANG); /* or: kill(pid, SIGCONT); */
}

static int
launch_execle(const char *cmd)
/* Execute a command using the system shell (/bin/sh), which takes care
 * of special functions such as pipes and stream redirection, and special
 * chars like wildcards, quotes, and escape sequences. Use only when the
 * shell is needed; otherwise, launch_execve() should be used instead. */
{
	if (!cmd)
		return EXNULLERR;

	/* Reenable SIGCHLD, in case it was disabled. Otherwise, waitpid won't
	 * be able to catch error codes coming from the child */
	signal(SIGCHLD, SIG_DFL);

	int status;
	pid_t pid = fork();
	if (pid < 0) {
		fprintf(stderr, "%s: fork: %s\n", PROGRAM_NAME, strerror(errno));
		return EXFORKERR;
	}

	else if (pid == 0) {
		/* Reenable signals only for the child, in case they were
		 * disabled for the parent */
		signal(SIGHUP, SIG_DFL);
		signal(SIGINT, SIG_DFL);
		signal(SIGQUIT, SIG_DFL);
		signal(SIGTERM, SIG_DFL);

		/* Get shell base name */
		char *name = strrchr(sys_shell, '/');

		execl(sys_shell, name ? name + 1 : sys_shell, "-c", cmd, NULL);
		fprintf(stderr, "%s: %s: execle: %s\n", PROGRAM_NAME, sys_shell,
				strerror(errno));
		_exit(errno);
	}
	/* Get command status */
	else if (pid > 0) {
		/* The parent process calls waitpid() on the child */
		if (waitpid(pid, &status, 0) > 0) {
			if (WIFEXITED(status) && !WEXITSTATUS(status)) {
				/* The program terminated normally and executed
				 * successfully */
				return EXIT_SUCCESS;
			}
			else if (WIFEXITED(status) && WEXITSTATUS(status)) {
				/* Either "command not found" (WEXITSTATUS(status) == 127),
				 * "permission denied" (not executable) (WEXITSTATUS(status) ==
				 * 126) or the program terminated normally, but returned a
				 * non-zero status. These exit codes will be handled by the
				 * system shell itself, since we're using here execle() */
				return WEXITSTATUS(status);
			}
			else {
				/* The program didn't terminate normally */
				return EXCRASHERR;
			}
		}
		else {
			/* Waitpid() failed */
			fprintf(stderr, "%s: waitpid: %s\n", PROGRAM_NAME,
					strerror(errno));
			return errno;
		}
	}

	/* Never reached */
	return EXIT_FAILURE;
}

static int
launch_execve(char **cmd, int bg, int xflags)
/* Execute a command and return the corresponding exit status. The exit
 * status could be: zero, if everything went fine, or a non-zero value
 * in case of error. The function takes as first arguement an array of
 * strings containing the command name to be executed and its arguments
 * (cmd), an integer (bg) specifying if the command should be
 * backgrounded (1) or not (0), and a flag to control file descriptors */
{
	if (!cmd)
		return EXNULLERR;

	/* Reenable SIGCHLD, in case it was disabled. Otherwise, waitpid
	 * won't be able to catch error codes coming from the child. */
	signal(SIGCHLD, SIG_DFL);

	pid_t pid = fork();
	if (pid < 0) {
		fprintf(stderr, "%s: fork: %s\n", PROGRAM_NAME, strerror(errno));
		return errno;
	}

	else if (pid == 0) {
		if (!bg) {
			/* If the program runs in the foreground, reenable signals
			 * only for the child, in case they were disabled for the
			 * parent */
			signal(SIGHUP, SIG_DFL);
			signal(SIGINT, SIG_DFL);
			signal(SIGQUIT, SIG_DFL);
			signal(SIGTERM, SIG_DFL);
		}

		if (xflags) {
			int fd = open("/dev/null", O_WRONLY, 0200);

			if (xflags & E_NOSTDIN)
				dup2(fd, STDIN_FILENO);

			if (xflags & E_NOSTDOUT)
				dup2(fd, STDOUT_FILENO);

			if (xflags & E_NOSTDERR)
				dup2(fd, STDERR_FILENO);

			close(fd);
		}

		execvp(cmd[0], cmd);
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, cmd[0],
				strerror(errno));
		_exit(errno);
	}

	/* Get command status (pid > 0) */
	else {
		if (bg) {
			run_in_background(pid);
			return EXIT_SUCCESS;
		}
		else
			return run_in_foreground(pid);
	}

	/* Never reached */
	return EXIT_FAILURE;
}

static inline void
free_dirlist(void)
{
	if (!file_info || !files)
		return;

	int i = (int)files;

	while (--i >= 0)
		free(file_info[i].name);

	free(file_info);
	file_info = (struct fileinfo *)NULL;
}

static void
print_dirhist_map(void)
{
	size_t i;

	for (i = 0; i < (size_t)dirhist_total_index; i++) {

		if (i != (size_t)dirhist_cur_index)
			continue;

		if (i > 0 && old_pwd[i - 1])
			printf("%zu %s\n", i, old_pwd[i - 1]);

		printf("%zu %s%s%s\n", i + 1, dh_c,
			   old_pwd[i], df_c);

		if (i + 1 < (size_t)dirhist_total_index && old_pwd[i + 1])
			printf("%zu %s\n", i + 2, old_pwd[i + 1]);

		break;
	}
}

static void
get_file_icon(const char *file, int n)
/* Set the icon field to the corresponding icon for FILE. If not found,
 * set the default icon */
{
	file_info[n].icon = DEF_FILE_ICON;
	file_info[n].icon_color = DEF_FILE_ICON_COLOR;

	if (!file)
		return;

	int i = (int)(sizeof(icon_filenames) / sizeof(struct icons_t));

	while (--i >= 0) {

		if (TOUPPER(*file) == TOUPPER(*icon_filenames[i].name)
		&& strcasecmp(file, icon_filenames[i].name) == 0) {
			file_info[n].icon = icon_filenames[i].icon;
			file_info[n].icon_color = icon_filenames[i].color;
			break;
		}
	}
}

static void
get_dir_icon(const char *dir, int n)
/* Set the icon field to the corresponding icon for DIR. If not found,
 * set the default icon */
{
	/* Default values for directories */
	file_info[n].icon = DEF_DIR_ICON;
	file_info[n].icon_color = DEF_DIR_ICON_COLOR;

	if (!dir)
		return;

	int i = (int)(sizeof(icon_dirnames) / sizeof(struct icons_t));

	while (--i >= 0) {

		if (TOUPPER(*dir) == TOUPPER(*icon_dirnames[i].name)
		&& strcasecmp(dir, icon_dirnames[i].name) == 0) {
			file_info[n].icon = icon_dirnames[i].icon;
			file_info[n].icon_color = icon_dirnames[i].color;
			break;
		}
	}
}

static void
get_ext_icon(const char *restrict ext, int n)
/* Set the icon field to the corresponding icon for EXT. If not found,
 * set the default icon */
{
	file_info[n].icon = DEF_FILE_ICON;
	file_info[n].icon_color = DEF_FILE_ICON_COLOR;

	if (!ext)
		return;

	ext++;

	int i = (int)(sizeof(icon_ext) / sizeof(struct icons_t));

	while (--i >= 0) {

		/* Tolower */
		char c = (*ext >= 'A' && *ext <= 'Z')
				 ? (*ext - 'A' + 'a') : *ext;

		if (c == *icon_ext[i].name
		&& strcasecmp(ext, icon_ext[i].name) == 0) {
			file_info[n].icon = icon_ext[i].icon;
			file_info[n].icon_color = icon_ext[i].color;
			break;
		}
	}
}

static char *
get_ext_color(const char *ext)
/* Returns a pointer to the corresponding color code for EXT, if some
 * color was defined */
{
	if (!ext || !ext_colors_n)
		return (char *)NULL;

	ext++;

	int i = (int)ext_colors_n;
	while (--i >= 0) {

		if (!ext_colors[i] || !*ext_colors[i] || !ext_colors[i][2])
			continue;

		char *p = (char *)ext, *q = ext_colors[i];

		/* +2 because stored extensions have this form: *.ext */
		q += 2;

		size_t match = 1;
		while (*p) {
			if (*p++ != *q++) {
				match = 0;
				break;
			}
		}

		if (!match || *q != '=')
			continue;

		return ++q;
	}

	return (char *)NULL;
}

static int
print_entry_props(struct fileinfo *props, size_t max)
{
	/* Get file size */
	char *size_type = get_size_unit(props->size);

	/* Get file type indicator */
	char file_type = 0;

	switch (props->mode & S_IFMT) {

	case S_IFREG: file_type = '-'; break;
	case S_IFDIR: file_type = 'd'; break;
	case S_IFLNK: file_type = 'l'; break;
	case S_IFSOCK: file_type = 's'; break;
	case S_IFBLK: file_type = 'b'; break;
	case S_IFCHR: file_type = 'c'; break;
	case S_IFIFO: file_type = 'p'; break;
	default: file_type = '?';
	}

	/* Get file permissions */
	char read_usr = '-', write_usr = '-', exec_usr = '-',
		 read_grp = '-', write_grp = '-', exec_grp = '-',
		 read_others = '-', write_others = '-', exec_others = '-';

	mode_t val = (props->mode & ~S_IFMT);
	if (val & S_IRUSR) read_usr = 'r';
	if (val & S_IWUSR) write_usr = 'w';
	if (val & S_IXUSR) exec_usr = 'x';

	if (val & S_IRGRP) read_grp = 'r';
	if (val & S_IWGRP) write_grp = 'w';
	if (val & S_IXGRP) exec_grp = 'x';

	if (val & S_IROTH) read_others = 'r';
	if (val & S_IWOTH) write_others = 'w';
	if (val & S_IXOTH) exec_others = 'x';

	if (props->mode & S_ISUID)
		(val & S_IXUSR) ? (exec_usr = 's') : (exec_usr = 'S');
	if (props->mode & S_ISGID)
		(val & S_IXGRP) ? (exec_grp = 's') : (exec_grp = 'S');

	/* Get modification time */
	char mod_time[128];

	if (props->ltime) {
		struct tm *t = localtime(&props->ltime);
		snprintf(mod_time, 128, "%d-%02d-%02d %02d:%02d", t->tm_year + 1900,
				t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min);
	}
	else
		strcpy(mod_time, "-               ");

	/* Get owner and group names */
/*  struct group *group;
	struct passwd *owner;
	group = getgrgid(props->uid);
	owner = getpwuid(props->gid); */

	/*  If filename length is greater than max, truncate it
	 * to max (later a tilde (~) will be appended to let the user know
	 * the file name was truncated) */
	char trim_name[NAME_MAX];
	int trim = 0;

	size_t cur_len = props->eln_n + 1 + props->len;
	if (icons) {
		cur_len += 3;
		max += 3;
	}

	if (cur_len > max) {
		int rest = (int)(cur_len - max);
		trim = 1;
		strcpy(trim_name, props->name);
		if (unicode)
			u8truncstr(trim_name, (size_t)(props->len - rest - 1));
		else
			trim_name[props->len - rest - 1] = '\0';
		cur_len -= rest;
	}

	/* Calculate pad for each file */
	int pad;

	pad = (int)(max - cur_len);

	if (pad < 0)
		pad = 0;

	int sticky = 0;
	if (props->mode & S_ISVTX)
		sticky = 1;

	printf("%s%s%c%s%s%s%-*s%s%c %c/%c%c%c/%c%c%c/%c%c%c%s  "
			"%u:%u  %s  %s\n",
			colorize ? props->icon_color : "",
			icons ? props->icon : "", icons ? ' ' : 0,
			colorize ? props->color : "",
			!trim ? props->name : trim_name,
			light_mode ? "" : df_c, pad, "", df_c,
			trim ? '~' : 0, file_type,
			read_usr, write_usr, exec_usr,
			read_grp, write_grp, exec_grp,
			read_others, write_others, sticky ? 't' : exec_others,
			is_acl(props->name) ? "+" : "",
/*          !owner ? _("?") : owner->pw_name,
			!group ? _("?") : group->gr_name, */
			props->uid, props->gid,
			*mod_time ? mod_time : "?",
			size_type ? size_type : "?");

	if (size_type)
		free(size_type);

	return EXIT_SUCCESS;
}

static int
namecmp(const char* s1, const char* s2)
{
	/* Do not take initial dot into account */
	if (*s1 == '.')
		s1++;

	if (*s2 == '.')
		s2++;

	char ac = *s1, bc = *s2;

	if (!case_sensitive) {
		ac = TOUPPER(*s1);
		bc = TOUPPER(*s2);
	}

	if (bc > ac)
		return -1;

	if (bc < ac)
		return 1;

	if (!case_sensitive)
		return strcasecmp(s1, s2);

	return strcmp(s1, s2);
}

static int
entrycmp(const void *a, const void *b)
{
	const struct fileinfo *pa = (struct fileinfo *)a;
	const struct fileinfo *pb = (struct fileinfo *)b;

	if (list_folders_first) {
		if (pb->dir != pa->dir) {
			if (pb->dir)
				return 1;

			return -1;
		}
	}

	int ret = 0, st = sort;

#ifndef _GNU_SOURCE
	if (st == SVER)
		st = SNAME;
#endif

	if (light_mode && (st == SOWN || st == SGRP))
		st = SNAME;

	switch(st) {

	case SSIZE:
		if (pa->size > pb->size)
			ret = 1;
		else if (pa->size < pb->size)
			ret = -1;
		break;

	case SATIME: /* fallthrough */
	case SBTIME: /* fallthrough */
	case SCTIME: /* fallthrough */
	case SMTIME:
		if (pa->time > pb->time)
			ret = 1;
		else if (pa->time < pb->time)
			ret = -1;
		break;

#ifdef _GNU_SOURCE
	case SVER:
		ret = strverscmp(pa->name, pb->name);
		break;
#endif

	case SEXT: {
		char *aext = (char *)NULL, *bext = (char *)NULL, *val;
		val = strrchr(pa->name, '.');
		if (val && val != pa->name)
			aext = val + 1;

		val = strrchr(pb->name, '.');
		if (val && val != pb->name)
			bext = val + 1;

		if (aext || bext) {
			if (!aext)
				ret = -1;
			else if (!bext)
				ret = 1;

			else
				ret = strcasecmp(aext, bext);
		}
		}
		break;

	case SINO:
		if (pa->inode > pb->inode)
			ret = 1;
		else if (pa->inode < pb->inode)
			ret = -1;
		break;

	case SOWN:
		if (pa->uid > pb->uid)
			ret = 1;
		else if (pa->uid < pb->uid)
			ret = -1;
		break;

	case SGRP:
		if (pa->gid > pb->gid)
			ret = 1;
		else if (pa->gid < pb->gid)
			ret = -1;
		break;
	}

	if (!ret)
		ret = namecmp(pa->name, pb->name);

	if (!sort_reverse)
		return ret;

	return (ret - (ret * 2));
}

static int
filter_function(const char *arg)
{
	if (!arg) {
		printf(_("Current filter: %s\n"), filter ? filter : "none");
		return EXIT_SUCCESS;
	}

	if (*arg == '-' && strcmp(arg, "--help") == 0) {
		puts(_("Usage: ft, filter [unset] [REGEX]"));
		return EXIT_SUCCESS;
	}

	if (*arg == 'u' && strcmp(arg, "unset") == 0) {
		if (filter) {
			free(filter);
			filter = (char *)NULL;
			regfree(&regex_exp);
			puts(_("Filter unset"));
		}

		else
			puts(_("No filter set"));

		return EXIT_SUCCESS;
	}

	if (filter)
		free(filter);

	regfree(&regex_exp);

	filter = savestring(arg, strlen(arg));

	if (regcomp(&regex_exp, filter, REG_NOSUB|REG_EXTENDED)
	!= EXIT_SUCCESS) {
		fprintf(stderr, _("%s: '%s': Invalid regular expression\n"),
				PROGRAM_NAME, filter);
		free(filter);
		filter = (char *)NULL;
		regfree(&regex_exp);
	}

	else
		puts(_("New filter successfully set"));

	return EXIT_SUCCESS;
}

static void
check_env_filter(void)
{
	if (filter)
		return;

	char *p = getenv("CLIFM_FILTER");

	if (!p)
		return;

	filter = savestring(p, strlen(p));
}

static int
count_dir(const char *dir_path) /* Readdir version */
/* Count files in DIR_PATH, including self and parent. */
{
	if (!dir_path)
		return -1;

	DIR *dir_p;

	if ((dir_p = opendir(dir_path)) == NULL) {
		if (errno == ENOMEM)
			exit(EXIT_FAILURE);
		else
			return -1;
	}

	int file_count = 0;
	struct dirent *ent;

	while ((ent = readdir(dir_p)))
		file_count++;

	closedir(dir_p);

	return file_count;
}

static void
copy_plugins(void)
{
	if (!CONFIG_DIR_GRAL)
		return;

	char usr_share_plugins_dir[] = "/usr/share/clifm/plugins";

	/* Make sure the system pĺugins dir exists and is not empty */
	if (count_dir(usr_share_plugins_dir) <= 2)
		return;

	char *cp_comm[] = { "cp", "-r", usr_share_plugins_dir,
					   CONFIG_DIR_GRAL, NULL };
	launch_execve(cp_comm, FOREGROUND, E_NOFLAG);
}

static char *
rl_no_hist(const char *prompt)
{
	stifle_history(0); /* Prevent readline from using the history
	setting */
	char *input = readline(prompt);
	unstifle_history(); /* Reenable history */
	read_history(HIST_FILE); /* Reload history lines from file */

	if (input) {

		/* Make sure input isn't empty string */
		if (!*input) {
			free(input);
			return (char *)NULL;
		}

		/* Check we have some non-blank char */
		int no_blank = 0;
		char *p = input;

		while (*p) {
			if (*p != ' ' && *p != '\n' && *p != '\t') {
				no_blank = 1;
				break;
			}
			p++;
		}

		if (!no_blank) {
			free(input);
			return (char *)NULL;
		}

		return input;
	}

	return (char *)NULL;
}

static int
batch_link(char **args)
{
	if (!args)
		return EXIT_FAILURE;

	if (!args[1] || (*args[1] == '-'
	&& strcmp(args[1], "--help") == 0)) {
		puts(_("Usage: bl [FILE(s)]"));
		return EXIT_SUCCESS;
	}

	char *suffix = (char *)NULL;
	while (!suffix) {
		suffix = rl_no_hist(_("Enter links suffix ('n' for none): "));

		if (!suffix)
			continue;

		if (!*suffix) {
			free(suffix);
			continue;
		}
	}

	size_t i;

	int exit_status = EXIT_SUCCESS;
	char tmp[NAME_MAX];

	for (i = 1; args[i]; i++) {
		char *linkname = (char *)NULL;

		if (*suffix == 'n' && !suffix[1])
			linkname = args[i];

		else {
			snprintf(tmp, NAME_MAX, "%s%s", args[i], suffix);
			linkname = tmp;
		}

		char *ptr = strrchr(linkname, '/');

		if (symlink(args[i], ptr ? ++ptr : linkname) == -1) {
			exit_status = EXIT_FAILURE;
			fprintf(stderr, _("%s: %s: Cannot create symlink: %s\n"),
					PROGRAM_NAME, ptr ? ptr : linkname, strerror(errno));
		}
	}

	if (exit_status == EXIT_SUCCESS && cd_lists_on_the_fly) {
		free_dirlist();

		if (list_dir() != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;
	}

	free(suffix);
	return exit_status;
}

static void
set_env(void)
{
	if (xargs.stealth_mode == 1)
		return;

	/* Set a few environment variables, mostly useful to run custom
	 * scripts via the actions function */
	/* CLIFM env variable is set to one when CliFM is running, so that
	 * external programs can determine if they were spawned by CliFM */
	setenv("CLIFM", "1", 1);

	setenv("CLIFM_PROFILE", alt_profile ? alt_profile : "default", 1);

	if (SEL_FILE)
		setenv("CLIFM_SELFILE", SEL_FILE, 1);
}

static int
add_to_jumpdb(const char *dir)
{
	if (xargs.no_dirjump == 1 || !dir || !*dir)
		return EXIT_FAILURE;

	if (!jump_db) {
		jump_db = (struct jump_t *)xnmalloc(1, sizeof(struct jump_t));
		jump_n = 0;
	}

	int i = (int)jump_n, new_entry = 1;
	while (--i >= 0) {

		if (dir[1] == jump_db[i].path[1]
		&& strcmp(jump_db[i].path, dir) == 0) {
			jump_db[i].visits++;
			jump_db[i].last_visit = time(NULL);
			new_entry = 0;
			break;
		}
	}

	if (!new_entry)
		return EXIT_SUCCESS;

	jump_db = (struct jump_t *)xrealloc(jump_db, (jump_n + 2)
									* sizeof(struct jump_t));
	jump_db[jump_n].visits = 1;
	time_t now = time(NULL);
	jump_db[jump_n].first_visit = now;
	jump_db[jump_n].last_visit = now;
	jump_db[jump_n].rank = 0;
	jump_db[jump_n].keep = 0;
	jump_db[jump_n++].path = savestring(dir, strlen(dir));

	jump_db[jump_n].path = (char *)NULL;
	jump_db[jump_n].visits = 0;
	jump_db[jump_n].rank = 0;
	jump_db[jump_n].keep = 0;
	jump_db[jump_n].first_visit = -1;
	jump_db[jump_n].last_visit = -1;

	return EXIT_SUCCESS;
}

static void
load_jumpdb(void)
/* Reconstruct the jump database from database file */
{
	if (xargs.no_dirjump ==  1 || !config_ok || !CONFIG_DIR)
		return;

	size_t dir_len = strlen(CONFIG_DIR);
	char *JUMP_FILE = (char *)xnmalloc(dir_len + 10, sizeof(char));
	snprintf(JUMP_FILE, dir_len + 10, "%s/jump.cfm", CONFIG_DIR);

	struct stat attr;

	if (stat(JUMP_FILE, &attr) == -1) {
		free(JUMP_FILE);
		return;
	}

	FILE *fp = fopen(JUMP_FILE, "r");

	if (!fp) {
		free(JUMP_FILE);
		return;
	}

	char tmp_line[PATH_MAX];
	size_t jump_lines = 0;

	while (fgets(tmp_line, (int)sizeof(tmp_line), fp)) {
		if (*tmp_line != '\n' && *tmp_line >= '0' && *tmp_line <= '9')
			jump_lines++;
	}

	if (!jump_lines) {
		free(JUMP_FILE);
		fclose(fp);
		return;
	}

	jump_db = (struct jump_t *)xnmalloc(jump_lines + 2, sizeof(struct jump_t));

	fseek(fp, 0L, SEEK_SET);

	size_t line_size = 0;
	char *line = (char *)NULL;
	ssize_t line_len = 0;

	while ((line_len = getline(&line, &line_size, fp)) > 0) {

		if (!*line || *line == '\n')
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

	fclose(fp);

	free(line);
	free(JUMP_FILE);

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

static void
save_jumpdb(void)
/* Store the jump database into a file */
{
	if (xargs.no_dirjump == 1 || !config_ok || !CONFIG_DIR
	|| !jump_db || jump_n == 0)
		return;

	char *JUMP_FILE = (char *)xnmalloc(strlen(CONFIG_DIR) + 10, sizeof(char));
	sprintf(JUMP_FILE, "%s/jump.cfm", CONFIG_DIR);

	FILE *fp = fopen(JUMP_FILE, "w+");

	if (!fp) {
		free(JUMP_FILE);
		return;
	}

	int i, reduce = 0, tmp_rank = 0, total_rank = 0;
	time_t now = time(NULL);

	/* Calculate both total rank sum and rank for each entry */
	i = (int)jump_n;
	while (--i >= 0) {

		int days_since_first = (int)(now - jump_db[i].first_visit) / 60 / 60 / 24;
		int rank = days_since_first > 1 ? ((int)jump_db[i].visits * 100)
							/ days_since_first : ((int)jump_db[i].visits * 100);

		int hours_since_last = (int)(now - jump_db[i].last_visit) / 60 / 60;

		tmp_rank = rank;
		if (hours_since_last == 0)          /* Last hour */
			rank = JHOUR(tmp_rank);
		else if (hours_since_last <= 24)    /* Last day */
			rank = JDAY(tmp_rank);
		else if (hours_since_last <= 168)   /* Last week */
			rank = JWEEK(tmp_rank);
		else                                 /* More than a week */
			rank = JOLDER(tmp_rank);

		/* Do not remove bookmarked, pinned, or workspaced directories */
		/* Add bonus points */
		int j = (int)bm_n;
		while (--j >= 0) {
			if (bookmarks[j].path[1] == jump_db[i].path[1]
			&& strcmp(bookmarks[j].path, jump_db[i].path) == 0) {
				jump_db[i].rank += BOOKMARK_BONUS;
				jump_db[i].keep = 1;
				break;
			}
		}

		if (pinned_dir && pinned_dir[1] == jump_db[i].path[1]
		&& strcmp(pinned_dir, jump_db[i].path) == 0) {
			jump_db[i].rank += PINNED_BONUS;
			jump_db[i].keep = 1;
		}

		j = MAX_WS;
		while (--j >= 0) {
			if (ws[j].path && ws[j].path[1] == jump_db[i].path[1]
			&& strcmp(jump_db[i].path, ws[j].path) == 0) {
				jump_db[i].rank += WORKSPACE_BONUS;
				jump_db[i].keep = 1;
				break;
			}
		}

		jump_db[i].rank = rank;
		total_rank += rank;
	}

	/* Once we have the total rank, check if we need to recude ranks,
	 * and write entries into the database */
	if (total_rank > max_jump_total_rank)
		reduce = (total_rank / max_jump_total_rank) + 1;

	int jump_num = 0;

	for (i = 0; i < (int)jump_n; i++) {

		if (reduce) {
			tmp_rank = jump_db[i].rank;
			jump_db[i].rank = tmp_rank / reduce;
		}

		/* Forget directories ranked below MIN_JUMP_RANK */
		if (jump_db[i].keep != 1 && (jump_db[i].rank <= 0
		|| jump_db[i].rank < min_jump_rank))
			continue;

		jump_num++;

		fprintf(fp, "%zu:%ld:%ld:%s\n", jump_db[i].visits,
				jump_db[i].first_visit, jump_db[i].last_visit,
				jump_db[i].path);
	}

	fprintf(fp, "@%d\n", total_rank);

	fclose(fp);
	free(JUMP_FILE);
}

static void
unset_xargs(void)
{
	xargs.splash = xargs.hidden = xargs.longview = UNSET;
	xargs.autocd = xargs.auto_open = xargs.ext = xargs.ffirst = UNSET;
	xargs.sensitive = xargs.unicode = xargs.pager = xargs.path = UNSET;
	xargs.light = xargs.cd_list_auto = xargs.sort = xargs.dirmap = UNSET;
	xargs.config = xargs.stealth_mode = xargs.restore_last_path = UNSET;
	xargs.tips = xargs.disk_usage = xargs.trasrm = UNSET;
	xargs.classify = xargs.share_selbox = xargs.rl_vi_mode = UNSET;
	xargs.max_dirhist = xargs.sort_reverse = xargs.files_counter = UNSET;
	xargs.welcome_message = xargs.clear_screen = UNSET;
	xargs.logs = xargs.max_path = xargs.bm_file = UNSET;
	xargs.expand_bookmarks = xargs.only_dirs = xargs.noeln = UNSET;
	xargs.list_and_quit = xargs.color_scheme = xargs.cd_on_quit = UNSET;
	xargs.no_dirjump = xargs.icons = xargs.no_colors = UNSET;
	xargs.icons_use_file_color = xargs.no_columns = UNSET;
	xargs.case_sens_dirjump = xargs.case_sens_path_comp = UNSET;
	xargs.cwd_in_title = UNSET;
}

static inline void
print_div_line(void)
{
	fputs(dl_c, stdout);

	int i;
	for (i = (int)term_cols; i--;)
		putchar(div_line_char);

	fputs(df_c, stdout);
	fflush(stdout);
}

static void
print_disk_usage(void)
/* Print free/total space for the filesystem of the current working
 * directory */
{
	if (!ws || !ws[cur_ws].path || !*ws[cur_ws].path)
		return;

	struct statvfs stat;

	if (statvfs(ws[cur_ws].path, &stat) != EXIT_SUCCESS) {
		_err('w', PRINT_PROMPT, "statvfs: %s\n", strerror(errno));
		return;
	}

	char *free_space = get_size_unit((off_t)(stat.f_frsize * stat.f_bavail));

	char *size = get_size_unit((off_t)(stat.f_blocks * stat.f_frsize));

	printf("%s->%s %s/%s\n", mi_c, df_c, free_space ? free_space : "?",
		   size ?  size : "?");

	free(free_space);
	free(size);

	return;
}

static int
get_last_path(void)
/* Set PATH to last visited directory and CUR_WS to last used
 * workspace */
{
	if (!CONFIG_DIR)
		return EXIT_FAILURE;

	char *last_file = (char *)xnmalloc(strlen(CONFIG_DIR) + 7, sizeof(char));
	sprintf(last_file, "%s/.last", CONFIG_DIR);

	struct stat last_attrib;
	if (stat(last_file, &last_attrib) == -1) {
		free(last_file);
		return EXIT_FAILURE;
	}

	FILE *last_fp = fopen(last_file, "r");

	if (!last_fp) {
		_err('w', PRINT_PROMPT, _("%s: Error retrieving last "
			 "visited directory\n"), PROGRAM_NAME);
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

	while (fgets(line, (int)sizeof(line), last_fp)) {

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

	fclose(last_fp);
	free(last_file);

	return EXIT_SUCCESS;
}

static int
load_pinned_dir(void)
/* Restore pinned dir from file */
{
	if (!config_ok)
		return EXIT_FAILURE;

	char *pin_file = (char *)xnmalloc(strlen(CONFIG_DIR) + 6, sizeof(char));
	sprintf(pin_file, "%s/.pin", CONFIG_DIR);

	struct stat attr;

	if (lstat(pin_file, &attr) == -1) {
		free(pin_file);
		return EXIT_FAILURE;
	}

	FILE *fp = fopen(pin_file, "r");

	if (!fp) {
		_err('w', PRINT_PROMPT, _("%s: Error retrieving pinned "
			 "directory\n"), PROGRAM_NAME);
		free(pin_file);
		return EXIT_FAILURE;
	}

	char line[PATH_MAX] = "";
	fgets(line, (int)sizeof(line), fp);

	if (!*line || !strchr(line, '/')) {
		free(pin_file);
		fclose(fp);
		return EXIT_FAILURE;
	}

	if (pinned_dir) {
		free(pinned_dir);
		pinned_dir = (char *)NULL;
	}

	pinned_dir = savestring(line, strlen(line));

	fclose(fp);

	free(pin_file);

	return EXIT_SUCCESS;
}

static int
is_color_code(const char *str)
/* Check if STR has the format of a color code string (a number or a
 * semicolon list (max 12 fields) of numbers of at most 3 digits each).
 * Returns 1 if true and 0 if false. */
{
	if(!str || !*str)
		return 0;

	size_t digits = 0, semicolon = 0;

	while(*str) {

		if (*str >= '0' && *str <= '9')
			digits++;

		else if (*str == ';') {

			if (*(str + 1) == ';') /* Consecutive semicolons */
				return 0;

			digits = 0;
			semicolon++;
		}

		/* Neither digit nor semicolon */
		else if (*str != '\n')
			return 0;

		str++;
	}

	/* No digits at all, ending semicolon, more than eleven fields, or
	 * more than three consecutive digits */
	if (!digits || digits > 3 || semicolon > 11)
		return 0;

	/* At this point, we have a semicolon separated string of digits (3
	 * consecutive max) with at most 12 fields. The only thing not
	 * validated here are numbers themselves */

	return 1;
}

static char *
strip_color_line(const char *str, char mode)
/* Strip color lines from the config file (FiletypeColors, if mode is
 * 't', and ExtColors, if mode is 'x') returning the same string
 * containing only allowed characters */
{
	if (!str || !*str)
		return (char *)NULL;

	char *buf = (char *)xnmalloc(strlen(str) + 1, sizeof(char));
	size_t len = 0;

	switch(mode) {

		case 't': /* di=01;31: */
			while(*str) {
				if ((*str >= '0' && *str <= '9')
				|| (*str >= 'a' && *str <= 'z')
				|| *str == '=' || *str == ';' || *str == ':')
					buf[len++] = *str;
				str++;
			}
		break;

		case 'x': /* *.tar=01;31: */
			while(*str) {
				if ((*str >= '0' && *str <= '9')
				|| (*str >= 'a' && *str <= 'z')
				|| (*str >= 'A' && *str <= 'Z')
				|| *str == '*' || *str == '.' || *str == '='
				|| *str == ';' || *str == ':')
					buf[len++] = *str;
				str++;
			}
		break;
	}

	if (!len || !*buf) {
		free(buf);
		return (char *)NULL;
	}

	buf[len] = '\0';

	return buf;
}

static void
free_colors(void)
{
	/* Reset whatever value was loaded */
	*bm_c = '\0';
	*dl_c = '\0';
	*el_c = '\0';
	*mi_c = '\0';
	*tx_c = '\0';
	*df_c = '\0';
	*dc_c = '\0';
	*wc_c = '\0';
	*dh_c = '\0';
	*li_c = '\0';
	*ti_c = '\0';
	*em_c = '\0';
	*wm_c = '\0';
	*nm_c = '\0';
	*si_c = '\0';
	*nd_c = '\0';
	*nf_c = '\0';
	*di_c = '\0';
	*ed_c = '\0';
	*ne_c = '\0';
	*ex_c = '\0';
	*ee_c = '\0';
	*bd_c = '\0';
	*ln_c = '\0';
	*mh_c = '\0';
	*or_c = '\0';
	*so_c = '\0';
	*pi_c = '\0';
	*cd_c = '\0';
	*fi_c = '\0';
	*ef_c = '\0';
	*su_c = '\0';
	*sg_c = '\0';
	*ca_c = '\0';
	*st_c = '\0';
	*tw_c = '\0';
	*ow_c = '\0';
	*no_c = '\0';
	*uf_c = '\0';
	return;
}

static int
set_colors(const char *colorscheme, int env)
/* Open the config file, get values for filetype and extension colors
 * and copy these values into the corresponding variable. If some value
 * is not found, or if it's a wrong value, the default is set. */
{
	char *filecolors = (char *)NULL, *extcolors = (char *)NULL,
		 *ifacecolors = (char *)NULL;

	*dir_ico_c = '\0';

	/* Set a pointer to the current color scheme */
	if (colorscheme && *colorscheme && color_schemes) {

		char *def_cscheme = (char *)NULL;

		size_t i;

		for (i = 0; color_schemes[i]; i++) {

			if (*colorscheme == *color_schemes[i]
			&& strcmp(colorscheme, color_schemes[i]) == 0) {
				cur_cscheme = color_schemes[i];
				break;
			}

			if (*color_schemes[i] == 'd'
			&& strcmp(color_schemes[i], "default") == 0)
				def_cscheme = color_schemes[i];
		}

		if (!cur_cscheme) {
			_err('w', PRINT_PROMPT, _("%s: %s: No such color scheme. "
				 "Falling back to the default one\n"), PROGRAM_NAME,
				 colorscheme);

			if (def_cscheme)
				cur_cscheme = def_cscheme;
		}
	}

	/* env is true only when the function is called from main() */
	if (env) {
		/* Try to get colors from environment variables */
		char *env_filecolors = getenv("CLIFM_FILE_COLORS");
		char *env_extcolors = getenv("CLIFM_EXT_COLORS");
		char *env_ifacecolors = getenv("CLIFM_IFACE_COLORS");

		if (env_filecolors && !filecolors)
			filecolors = savestring(env_filecolors, strlen(env_filecolors));

		env_filecolors = (char *)NULL;

		if (env_extcolors && !extcolors)
			extcolors = savestring(env_extcolors, strlen(env_extcolors));

		env_extcolors = (char *)NULL;

		if (env_ifacecolors && !ifacecolors)
			ifacecolors = savestring(env_ifacecolors, strlen(env_ifacecolors));

		env_ifacecolors = (char *)NULL;
	}

	if (config_ok && (!filecolors || !extcolors || !ifacecolors)) {
	/* Get color lines, for both file types and extensions, from
	 * COLORSCHEME file */

		char *colorscheme_file = (char *)xnmalloc(strlen(COLORS_DIR)
								+ strlen(colorscheme) + 6, sizeof(char));
		sprintf(colorscheme_file, "%s/%s.cfm", COLORS_DIR,
				colorscheme ? colorscheme : "default");

		FILE *fp_colors = fopen(colorscheme_file, "r");

		if (fp_colors) {

			/* If called from the color scheme function, reset all
			 * color values before proceeding */
			if (!env)
				free_colors();

			char *line = (char *)NULL;
			ssize_t line_len = 0;
			size_t line_size = 0;
			int file_type_found = 0, ext_type_found = 0,
				iface_found = 0, dir_icon_found = 0;

			while ((line_len = getline(&line, &line_size,
			fp_colors)) > 0) {

				/* Interface colors */
				if (!ifacecolors && *line == 'I'
				&& strncmp(line, "InterfaceColors=", 16) == 0) {
					iface_found = 1;
					char *opt_str = strchr(line, '=');

					if (!opt_str)
						continue;

					opt_str++;

					char *color_line = strip_color_line(opt_str, 't');

					if (!color_line)
						continue;

					ifacecolors = savestring(color_line, strlen(color_line));
					free(color_line);
				}

				/* Filetype Colors */
				if (!filecolors && *line == 'F'
				&& strncmp(line, "FiletypeColors=", 15) == 0) {
					file_type_found = 1;
					char *opt_str = strchr(line, '=');

					if (!opt_str)
						continue;

					opt_str++;

					char *color_line = strip_color_line(opt_str, 't');

					if (!color_line)
						continue;

					filecolors = savestring(color_line, strlen(color_line));
					free(color_line);
				}

				/* File extension colors */
				if (!extcolors && *line == 'E'
				&& strncmp(line, "ExtColors=", 10) == 0) {
					ext_type_found = 1;
					char *opt_str = strchr(line, '=');

					if (!opt_str)
						continue;

					opt_str++;

					char *color_line = strip_color_line(opt_str, 'x');

					if (!color_line)
						continue;

					extcolors = savestring(color_line, strlen(color_line));
					free(color_line);
				}

				/* Dir icons Color */
				if (*line == 'D'
				&& strncmp(line, "DirIconsColor=", 14) == 0) {
					dir_icon_found = 1;
					char *opt_str = strchr(line, '=');

					if (!opt_str)
						continue;

					if (!*(++opt_str))
						continue;

					if (*opt_str == '\'' || *opt_str == '"')
						opt_str++;

					if (!*opt_str)
						continue;

					int nl_removed = 0;
					if (line[line_len - 1] == '\n') {
						line[line_len - 1] = '\0';
						nl_removed = 1;
					}

					int end_char = (int)line_len - 1;

					if (nl_removed)
						end_char--;

					if (line[end_char] == '\''
					|| line[end_char] == '"')
						line[end_char] = '\0';

					sprintf(dir_ico_c, "\x1b[%sm", opt_str);
				}

				if (file_type_found && ext_type_found
				&& iface_found && dir_icon_found)
					break;
			}

			free(line);
			line = (char *)NULL;

			fclose(fp_colors);
		}

		/* If fopen failed */
		else {
			if (!env) {
				fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME,
						colorscheme_file, strerror(errno));
				free(colorscheme_file);
				return EXIT_FAILURE;
			}

			else {
				_err('w', PRINT_PROMPT, _("%s: %s: No such color "
					 "scheme. Falling back to the default one\n"),
					 PROGRAM_NAME, colorscheme);
			}
		}

		free(colorscheme_file);
		colorscheme_file = (char *)NULL;
	}

			/* ##############################
			 * #    FILE EXTENSION COLORS   #
			 * ############################## */

	/* Split the colors line into substrings (one per color) */

	if (!extcolors) {

		/* Unload current extension colors */
		if (ext_colors_n) {
			int i = (int)ext_colors_n;

			while (--i >= 0)
				free(ext_colors[i]);

			free(ext_colors);
			ext_colors = (char **)NULL;
			free(ext_colors_len);
			ext_colors_n = 0;
		}
	}

	else {

		char *p = extcolors, *buf = (char *)NULL;
		size_t len = 0;
		int eol = 0;

		if (ext_colors_n) {
			int i =  (int)ext_colors_n;

			while (--i >= 0)
				free(ext_colors[i]);

			free(ext_colors);
			ext_colors = (char **)NULL;
			free(ext_colors_len);
			ext_colors_n = 0;
		}

		while(!eol) {
			switch (*p) {

			case '\0': /* fallthrough */
			case '\n': /* fallthrough */
			case ':':
				buf[len] = '\0';
				ext_colors = (char **)xrealloc(ext_colors,
									(ext_colors_n + 1) * sizeof(char *));
				ext_colors[ext_colors_n++] = savestring(buf, len);
				*buf = '\0';

				if (!*p)
					eol = 1;

				len = 0;
				p++;
				break;

			default:
				buf = (char *)xrealloc(buf, (len + 2) * sizeof(char));
				buf[len++] = *(p++);
				break;
			}
		}

		p = (char *)NULL;
		free(extcolors);
		extcolors = (char *)NULL;

		if (buf) {
			free(buf);
			buf = (char *)NULL;
		}

		if (ext_colors) {
			ext_colors = (char **)xrealloc(ext_colors, (ext_colors_n + 1)
										   * sizeof(char *));
			ext_colors[ext_colors_n] = (char *)NULL;
		}

		/* Make sure we have valid color codes and store the length
		 * of each stored extension: this length will be used later
		 * when listing files */
		ext_colors_len = (size_t *)xnmalloc(ext_colors_n, sizeof(size_t));

		int i =  (int)ext_colors_n;
		while (--i >= 0) {
			char *ret = strrchr(ext_colors[i], '=');

			if (!ret || !is_color_code(ret + 1)) {
				*ext_colors[i] = '\0';
				ext_colors_len[i] = 0;
				continue;
			}

			size_t j, ext_len = 0;

			for (j = 2; ext_colors[i][j]
			&& ext_colors[i][j] != '='; j++)
				ext_len++;

			ext_colors_len[i] = ext_len;
		}
	}

			/* ##############################
			 * #      INTERFACE COLORS      #
			 * ############################## */

	if (!ifacecolors) {

		/* Free and reset whatever value was loaded */
		*bm_c = '\0';
		*dl_c = '\0';
		*el_c = '\0';
		*mi_c = '\0';
		*tx_c = '\0';
		*df_c = '\0';
		*dc_c = '\0';
		*wc_c = '\0';
		*dh_c = '\0';
		*li_c = '\0';
		*ti_c = '\0';
		*em_c = '\0';
		*wm_c = '\0';
		*nm_c = '\0';
		*si_c = '\0';
	}

	else {

		char *p = ifacecolors, *buf = (char *)NULL,
			 **colors = (char **)NULL;
		size_t len = 0, words = 0;
		int eol = 0;

		while(!eol) {
			switch (*p) {

			case '\0': /* fallthrough */
			case '\n': /* fallthrough */
			case ':':
				buf[len] = '\0';
				colors = (char **)xrealloc(colors, (words + 1) * sizeof(char *));
				colors[words++] = savestring(buf, len);
				*buf = '\0';

				if (!*p)
					eol = 1;

				len = 0;
				p++;
				break;

			default:
				buf = (char *)xrealloc(buf, (len + 2) * sizeof(char));
				buf[len++] = *(p++);
				break;
			}
		}

		p = (char *)NULL;
		free(ifacecolors);
		ifacecolors = (char *)NULL;

		if (buf) {
			free(buf);
			buf = (char *)NULL;
		}

		if (colors) {
			colors = (char **)xrealloc(colors, (words + 1)
									   * sizeof(char *));
			colors[words] = (char *)NULL;
		}

		int i = (int)words;
		/* Set the color variables */
		while (--i >= 0) {

			if (*colors[i] == 't' && strncmp(colors[i], "tx=", 3) == 0) {

				if (!is_color_code(colors[i] + 3))
					/* zero the corresponding variable as a flag for
					 * the check after this for loop to prepare the
					 * variable to hold the default color */
					*tx_c = '\0';

				else
					snprintf(tx_c, MAX_COLOR + 2, "\001\x1b[%sm\002",
							 colors[i] + 3);
			}

			else if (*colors[i] == 'b' && strncmp(colors[i], "bm=", 3) == 0) {

				if (!is_color_code(colors[i] + 3))
					*bm_c = '\0';

				else
					snprintf(bm_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);
			}

			else if (*colors[i] == 'l' && strncmp(colors[i], "li=", 3) == 0) {

				if (!is_color_code(colors[i] + 3))
					*li_c = '\0';

				else
					snprintf(li_c, MAX_COLOR + 2, "\001\x1b[%sm\002",
							 colors[i] + 3);
			}

			else if (*colors[i] == 't' && strncmp(colors[i], "ti=", 3) == 0) {
				if (!is_color_code(colors[i] + 3))
					*ti_c = '\0';
				else
					snprintf(ti_c, MAX_COLOR + 2, "\001\x1b[%sm\002",
							 colors[i] + 3);
			}

			else if (*colors[i] == 'e' && strncmp(colors[i], "em=", 3) == 0) {
				if (!is_color_code(colors[i] + 3))
					*em_c = '\0';
				else
					snprintf(em_c, MAX_COLOR + 2, "\001\x1b[%sm\002",
							 colors[i] + 3);
			}

			else if (*colors[i] == 'w' && strncmp(colors[i], "wm=", 3) == 0) {
				if (!is_color_code(colors[i] + 3))
					*wm_c = '\0';
				else
					snprintf(wm_c, MAX_COLOR + 2, "\001\x1b[%sm\002",
							 colors[i] + 3);
			}

			else if (*colors[i] == 'n' && strncmp(colors[i], "nm=", 3) == 0) {
				if (!is_color_code(colors[i] + 3))
					*nm_c = '\0';
				else
					snprintf(nm_c, MAX_COLOR + 2, "\001\x1b[%sm\002",
							 colors[i] + 3);
			}

			else if (*colors[i] == 's' && strncmp(colors[i], "si=", 3) == 0) {
				if (!is_color_code(colors[i] + 3))
					*si_c = '\0';
				else
					snprintf(si_c, MAX_COLOR + 2, "\001\x1b[%sm\002",
							 colors[i] + 3);
			}

			else if (*colors[i] == 'e' && strncmp(colors[i], "el=", 3) == 0) {
				if (!is_color_code(colors[i] + 3))
					*el_c = '\0';
				else
					snprintf(el_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);
			}

			else if (*colors[i] == 'm' && strncmp(colors[i], "mi=", 3) == 0) {
				if (!is_color_code(colors[i] + 3))
					*mi_c = '\0';
				else
					snprintf(mi_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);
			}

			else if (*colors[i] == 'd' && strncmp(colors[i], "dl=", 3) == 0) {
				if (!is_color_code(colors[i] + 3))
					*dl_c = '\0';
				else
					snprintf(dl_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);
			}

			else if (*colors[i] == 'd' && strncmp(colors[i], "df=", 3) == 0) {
				if (!is_color_code(colors[i] + 3))
					*df_c = '\0';
				else
					snprintf(df_c, MAX_COLOR - 1, "\x1b[%s;49m", colors[i] + 3);
			}

			else if (*colors[i] == 'd' && strncmp(colors[i], "dc=", 3) == 0) {
				if (!is_color_code(colors[i] + 3))
					*dc_c = '\0';
				else
					snprintf(dc_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);
			}

			else if (*colors[i] == 'w' && strncmp(colors[i], "wc=", 3) == 0) {
				if (!is_color_code(colors[i] + 3))
					*wc_c = '\0';
				else
					snprintf(wc_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);
			}

			else if (*colors[i] == 'd' && strncmp(colors[i], "dh=", 3) == 0) {
				if (!is_color_code(colors[i] + 3))
					*dh_c = '\0';
				else
					snprintf(dh_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);
			}

			free(colors[i]);
		}

		free(colors);
		colors = (char **)NULL;
	}

	if (!filecolors) {
		*nd_c = '\0';
		*nf_c = '\0';
		*di_c = '\0';
		*ed_c = '\0';
		*ne_c = '\0';
		*ex_c = '\0';
		*ee_c = '\0';
		*bd_c = '\0';
		*ln_c = '\0';
		*mh_c = '\0';
		*or_c = '\0';
		*so_c = '\0';
		*pi_c = '\0';
		*cd_c = '\0';
		*fi_c = '\0';
		*ef_c = '\0';
		*su_c = '\0';
		*sg_c = '\0';
		*ca_c = '\0';
		*st_c = '\0';
		*tw_c = '\0';
		*ow_c = '\0';
		*no_c = '\0';
		*uf_c = '\0';

		/* Set the LS_COLORS environment variable with default values */
		char lsc[] = DEF_LS_COLORS;

		if (setenv("LS_COLORS", lsc, 1) == -1)
			fprintf(stderr, _("%s: Error registering environment "
					"colors\n"), PROGRAM_NAME);
	}

	else {
		/* Set the LS_COLORS environment variable to use CliFM own
		 * colors. In this way, files listed for TAB completion will
		 * use CliFM colors instead of system colors */

		/* Strip CLiFM custom filetypes (nd, ne, nf, ed, ef, ee, uf,
		 * bm, el, mi, dl, tx, df, dc, wc, dh, li, ti, em, wm, nm, si,
		 * and ca), from filecolors to construct a valid value for
		 * LS_COLORS */
		size_t buflen = 0, linec_len = strlen(filecolors);
		char *ls_buf = (char *)NULL;
		int i = 0;

		ls_buf = (char *)xnmalloc(linec_len + 1, sizeof(char));

		while (filecolors[i]) {

			int rem = 0;

			if ((int)i < (int)(linec_len - 3) && (
			(filecolors[i] == 'n' && (filecolors[i+1] == 'd'
			|| filecolors[i+1] == 'e' || filecolors[i+1] == 'f'))
			|| (filecolors[i] == 'e' && (filecolors[i+1] == 'd'
			|| filecolors[i+1] == 'f' || filecolors[i+1] == 'e'))
			|| (filecolors[i] == 'u' && filecolors[i+1] == 'f')
			|| (filecolors[i] == 'c' && filecolors[i+1] == 'a') )
			&& filecolors[i+2] == '=') {

				/* If one of the above is found, move to the next
				 * color code */
				rem = 1;
				for (i += 3; filecolors[i] && filecolors[i] != ':'; i++);
			}

			if (filecolors[i]) {

				if (!rem)
					ls_buf[buflen++] = filecolors[i];
			}

			else
				break;

			i++;
		}

		if (buflen) {
			ls_buf[buflen] = '\0';

			if (setenv("LS_COLORS", ls_buf, 1) == -1)
				fprintf(stderr, _("%s: Error registering environment "
								  "colors\n"), PROGRAM_NAME);
			free(ls_buf);
			ls_buf = (char *)NULL;
		}

		/* Split the colors line into substrings (one per color) */
		char *p = filecolors, *buf = (char *)NULL, **colors = (char **)NULL;
		size_t len = 0, words = 0;
		int eol = 0;

		while(!eol) {
			switch (*p) {

			case '\0': /* fallthrough */
			case '\n': /* fallthrough */
			case ':':
				buf[len] = '\0';
				colors = (char **)xrealloc(colors, (words + 1) * sizeof(char *));
				colors[words++] = savestring(buf, len);
				*buf = '\0';

				if (!*p)
					eol = 1;

				len = 0;
				p++;
				break;

			default:
				buf = (char *)xrealloc(buf, (len + 2) * sizeof(char));
				buf[len++] = *(p++);
				break;
			}
		}

		p = (char *)NULL;
		free(filecolors);
		filecolors = (char *)NULL;

		if (buf) {
			free(buf);
			buf = (char *)NULL;
		}

		if (colors) {
			colors = (char **)xrealloc(colors, (words + 1) * sizeof(char *));
			colors[words] = (char *)NULL;
		}

		/* Set the color variables */
		i = (int)words;
		while (--i >= 0) {

			if (*colors[i] == 'd' && strncmp(colors[i], "di=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*di_c = '\0';
				else
					snprintf(di_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 'd' && strncmp(colors[i], "df=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*df_c = '\0';
				else
					snprintf(df_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 'd' && strncmp(colors[i], "dc=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*dc_c = '\0';
				else
					snprintf(dc_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 'w' && strncmp(colors[i], "wc=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*wc_c = '\0';
				else
					snprintf(wc_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 'd' && strncmp(colors[i], "dh=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*dh_c = '\0';
				else
					snprintf(dh_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);
			else if (*colors[i] == 'n' && strncmp(colors[i], "nd=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*nd_c = '\0';
				else
					snprintf(nd_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 'e' && strncmp(colors[i], "ed=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*ed_c = '\0';
				else
					snprintf(ed_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 'n' && strncmp(colors[i], "ne=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*ne_c = '\0';
				else
					snprintf(ne_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 'f' && strncmp(colors[i], "fi=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*fi_c = '\0';
				else
					snprintf(fi_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 'e' && strncmp(colors[i], "ef=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*ef_c = '\0';
				else
					snprintf(ef_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 'n' && strncmp(colors[i], "nf=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*nf_c = '\0';
				else
					snprintf(nf_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 'l' && strncmp(colors[i], "ln=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*ln_c = '\0';
				else
					snprintf(ln_c, MAX_COLOR - 1, "\x1b[%sm", colors[i]
							 + 3);

			else if (*colors[i] == 'o' && strncmp(colors[i], "or=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*or_c = '\0';
				else
					snprintf(or_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 'e' && strncmp(colors[i], "ex=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*ex_c = '\0';
				else
					snprintf(ex_c, MAX_COLOR-1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 'e' && strncmp(colors[i], "ee=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*ee_c = '\0';
				else
					snprintf(ee_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 'b' && strncmp(colors[i], "bd=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*bd_c = '\0';
				else
					snprintf(bd_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 'c' && strncmp(colors[i], "cd=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*cd_c = '\0';
				else
					snprintf(cd_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 'p' && strncmp(colors[i], "pi=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*pi_c = '\0';
				else
					snprintf(pi_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 's' && strncmp(colors[i], "so=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*so_c = '\0';
				else
					snprintf(so_c, MAX_COLOR-1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 's' && strncmp(colors[i], "su=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*su_c = '\0';
				else
					snprintf(su_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 's' && strncmp(colors[i], "sg=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*sg_c = '\0';
				else
					snprintf(sg_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 't' && strncmp(colors[i], "tw=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*tw_c = '\0';
				else
					snprintf(tw_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 's' && strncmp(colors[i], "st=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*st_c = '\0';
				else
					snprintf(st_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 'o' && strncmp(colors[i], "ow=", 3) == 0)
				if (!is_color_code(colors[i]+3))
					*ow_c = '\0';
				else
					snprintf(ow_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 'c' && strncmp(colors[i], "ca=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*ca_c = '\0';
				else
					snprintf(ca_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 'n' && strncmp(colors[i], "no=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*no_c = '\0';
				else
					snprintf(no_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 'm' && strncmp(colors[i], "mh=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*mh_c = '\0';
				else
					snprintf(mh_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 'u' && strncmp(colors[i], "uf=", 3) == 0) {
				if (!is_color_code(colors[i] + 3))
					*uf_c = '\0';
				else
					snprintf(uf_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);
			}

			free(colors[i]);
		}

		free(colors);
		colors = (char **)NULL;
	}

	/* If some color was not set or it was a wrong color code, set the
	 * default */
	if (!*el_c)
		strcpy(el_c, DEF_EL_C);
	if (!*mi_c)
		strcpy(mi_c, DEF_MI_C);
	if (!*dl_c)
		strcpy(dl_c, DEF_DL_C);
	if (!*df_c)
		strcpy(df_c, DEF_DF_C);
	if (!*dc_c)
		strcpy(dc_c, DEF_DC_C);
	if (!*wc_c)
		strcpy(wc_c, DEF_WC_C);
	if (!*dh_c)
		strcpy(dh_c, DEF_DH_C);
	if (!*tx_c)
		strcpy(tx_c, DEF_TX_C);
	if (!*li_c)
		strcpy(li_c, DEF_LI_C);
	if (!*ti_c)
		strcpy(ti_c, DEF_TI_C);
	if (!*em_c)
		strcpy(em_c, DEF_EM_C);
	if (!*wm_c)
		strcpy(wm_c, DEF_WM_C);
	if (!*nm_c)
		strcpy(nm_c, DEF_NM_C);
	if (!*si_c)
		strcpy(si_c, DEF_SI_C);
	if (!*bm_c)
		strcpy(bm_c, DEF_BM_C);

	if (!*di_c)
		strcpy(di_c, DEF_DI_C);
	if (!*nd_c)
		strcpy(nd_c, DEF_ND_C);
	if (!*ed_c)
		strcpy(ed_c, DEF_ED_C);
	if (!*ne_c)
		strcpy(ne_c, DEF_NE_C);
	if (!*fi_c)
		strcpy(fi_c, DEF_FI_C);
	if (!*ef_c)
		strcpy(ef_c, DEF_EF_C);
	if (!*nf_c)
		strcpy(nf_c, DEF_NF_C);
	if (!*ln_c)
		strcpy(ln_c, DEF_LN_C);
	if (!*or_c)
		strcpy(or_c, DEF_OR_C);
	if (!*pi_c)
		strcpy(pi_c, DEF_PI_C);
	if (!*so_c)
		strcpy(so_c, DEF_SO_C);
	if (!*bd_c)
		strcpy(bd_c, DEF_BD_C);
	if (!*cd_c)
		strcpy(cd_c, DEF_CD_C);
	if (!*su_c)
		strcpy(su_c, DEF_SU_C);
	if (!*sg_c)
		strcpy(sg_c, DEF_SG_C);
	if (!*st_c)
		strcpy(st_c, DEF_ST_C);
	if (!*tw_c)
		strcpy(tw_c, DEF_TW_C);
	if (!*ow_c)
		strcpy(ow_c, DEF_OW_C);
	if (!*ex_c)
		strcpy(ex_c, DEF_EX_C);
	if (!*ee_c)
		strcpy(ee_c, DEF_EE_C);
	if (!*ca_c)
		strcpy(ca_c, DEF_CA_C);
	if (!*no_c)
		strcpy(no_c, DEF_NO_C);
	if (!*uf_c)
		strcpy(uf_c, DEF_UF_C);
	if (!*mh_c)
		strcpy(mh_c, DEF_MH_C);

	if (!*dir_ico_c)
		strcpy(dir_ico_c, DEF_DIR_ICO_C);

	return EXIT_SUCCESS;
}

static void
check_options(void)
/* If some option was not set, set it to the default value */
{
	if (!usr_cscheme)
		usr_cscheme = savestring("default", 7);

	/* Do no override command line options */
	if (xargs.cwd_in_title == UNSET)
		xargs.cwd_in_title = DEF_CWD_IN_TITLE;

	if (cp_cmd == UNSET)
		cp_cmd = DEF_CP_CMD;

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

	if (tr_as_rm == UNSET) {
		if (xargs.trasrm == UNSET)
			tr_as_rm = DEF_TRASRM;
		else
			tr_as_rm = xargs.trasrm;
	}

	if (only_dirs == UNSET) {
		if (xargs.only_dirs == UNSET)
			only_dirs = DEF_ONLY_DIRS;
		else
			only_dirs = xargs.only_dirs;
	}

	if (colorize == UNSET) {
		if (xargs.no_colors == UNSET)
			colorize = DEF_COLORS;
		else
			colorize = xargs.no_colors;
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

	if (div_line_char == UNSET)
		div_line_char = DEF_DIV_LINE_CHAR;

	if (max_hist == UNSET)
		max_hist = DEF_MAX_HIST;

	if (max_log == UNSET)
		max_log = DEF_MAX_LOG;

	if (!sys_shell) {
		sys_shell = get_sys_shell();

		if (!sys_shell)
			sys_shell = savestring(FALLBACK_SHELL, strlen(FALLBACK_SHELL));
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

static void
create_tmp_files(void)
{
	size_t pnl_len = strlen(PNL);

	/* #### CHECK THE TMP DIR #### */

	/* If the temporary directory doesn't exist, create it. I create
	 * the parent directory (/tmp/clifm) with 1777 permissions (world
	 * writable with the sticky bit set), so that every user is able
	 * to create files in here, but only the file's owner can remove
	 * or modify them */

	size_t user_len = strlen(user);
	TMP_DIR = (char *)xnmalloc(pnl_len + user_len + 7, sizeof(char));
	snprintf(TMP_DIR, pnl_len + 6, "/tmp/%s", PNL);

	struct stat file_attrib;

	if (stat(TMP_DIR, &file_attrib) == -1) {

		char *md_cmd[] = { "mkdir", "-pm1777", TMP_DIR, NULL };

		if (launch_execve(md_cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS) {
			_err('e', PRINT_PROMPT, _("%s: '%s': Error creating "
				 "temporary directory\n"), PROGRAM_NAME, TMP_DIR);
		}
	}

	/* Once the parent directory exists, create the user's directory to
	 * store the list of selected files:
	 * TMP_DIR/clifm/username/.selbox_PROFILE. I use here very
	 * restrictive permissions (700), since only the corresponding user
	 * must be able to read and/or modify this list */

	snprintf(TMP_DIR, pnl_len + user_len + 7, "/tmp/%s/%s", PNL, user);

	if (stat(TMP_DIR, &file_attrib) == -1) {

		char *md_cmd2[] = { "mkdir", "-pm700", TMP_DIR, NULL };

		if (launch_execve(md_cmd2, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS) {
			selfile_ok = 0;
			_err('e', PRINT_PROMPT, _("%s: '%s': Error creating "
				 "temporary directory\n"), PROGRAM_NAME, TMP_DIR);
		}
	}

	/* If the directory exists, check it is writable */
	else if (access(TMP_DIR, W_OK) == -1) {

		if (!SEL_FILE) {
			selfile_ok = 0;
			_err('w', PRINT_PROMPT, "%s: '%s': Directory not writable. "
				 "Selected files will be lost after program exit\n",
				 PROGRAM_NAME, TMP_DIR);
		}
	}

	/* If the config directory isn't available, define an alternative
	 * selection file in /tmp */
	if (!SEL_FILE && xargs.stealth_mode != 1) {

		size_t tmp_dir_len = strlen(TMP_DIR);

		if (!share_selbox) {
			size_t prof_len = 0;

			if (alt_profile)
				prof_len = strlen(alt_profile);
			else
				prof_len = 7; /* Lenght of "default" */

			SEL_FILE = (char *)xnmalloc(tmp_dir_len + prof_len + 9,
										sizeof(char));
			sprintf(SEL_FILE, "%s/selbox_%s", TMP_DIR,
					(alt_profile) ? alt_profile : "default");
		}

		else {
			SEL_FILE = (char *)xnmalloc(tmp_dir_len + 8, sizeof(char));
			sprintf(SEL_FILE, "%s/selbox", TMP_DIR);
		}

		_err('w', PRINT_PROMPT, _("%s: '%s': Using a temporary "
			 "directory for the Selection Box. Selected files won't "
			 "be persistent accros reboots"), PROGRAM_NAME, TMP_DIR);
	}
}

static void
set_sel_file(void)
/* Define the file for the Selection Box */
{
	if (SEL_FILE) {
		free(SEL_FILE);
		SEL_FILE = (char *)NULL;
	}

	if (!CONFIG_DIR)
		return;

	size_t config_len = strlen(CONFIG_DIR);

	if (!share_selbox) {
		/* Private selection box is stored in the profile
		 * directory */
		SEL_FILE = (char *)xnmalloc(config_len + 9, sizeof(char));

		sprintf(SEL_FILE, "%s/selbox", CONFIG_DIR);
	}

	else {
		/* Common selection box is stored in the general
		 * configuration directory */
		SEL_FILE = (char *)xnmalloc(config_len + 17, sizeof(char));
		sprintf(SEL_FILE, "%s/.config/%s/selbox",
				user_home, PNL);
	}

	return;
}

static void
get_aliases(void)
{
	if (!config_ok)
		return;

	FILE *config_file_fp;
	config_file_fp = fopen(CONFIG_FILE, "r");
	if (!config_file_fp) {
		_err('e', PRINT_PROMPT, "%s: alias: '%s': %s\n",
			 PROGRAM_NAME, CONFIG_FILE, strerror(errno));
		return;
	}

	if (aliases_n) {
		int i = (int)aliases_n;
		while (--i >= 0)
			free(aliases[i]);
		free(aliases);
		aliases = (char **)NULL;
		aliases_n = 0;
	}


	char *line = (char *)NULL;
	size_t line_size = 0;
	ssize_t line_len = 0;

	while ((line_len = getline(&line, &line_size,
	config_file_fp)) > 0) {

		if (*line == 'a' && strncmp(line, "alias ", 6) == 0) {
			char *alias_line = strchr(line, ' ');
			if (alias_line) {
				alias_line++;
				aliases = (char **)xrealloc(aliases, (aliases_n + 1)
											* sizeof(char *));
				aliases[aliases_n++] = savestring(alias_line,
											strlen(alias_line));
			}
		}
	}

	free(line);

	fclose(config_file_fp);
}

static int
load_dirhist(void)
{
	if (!config_ok)
		return EXIT_FAILURE;

	FILE *fp = fopen(DIRHIST_FILE, "r");

	if (!fp)
		return EXIT_FAILURE;

	size_t dirs = 0;

	char tmp_line[PATH_MAX];

	while (fgets(tmp_line, (int)sizeof(tmp_line), fp))
		dirs++;

	if (!dirs) {
		fclose(fp);
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

		old_pwd[dirhist_total_index] = (char *)xnmalloc(line_len
											   + 1, sizeof(char));
		strcpy(old_pwd[dirhist_total_index++], line);
	}

	old_pwd[dirhist_total_index] = (char *)NULL;

	free(line);

	dirhist_cur_index = dirhist_total_index - 1;

	return EXIT_SUCCESS;
}

static void
get_prompt_cmds(void)
{
	if (!config_ok)
		return;

	FILE *config_file_fp;
	config_file_fp = fopen(CONFIG_FILE, "r");
	if (!config_file_fp) {
		_err('e', PRINT_PROMPT, "%s: prompt: '%s': %s\n",
			 PROGRAM_NAME, CONFIG_FILE, strerror(errno));
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

	while ((line_len = getline(&line, &line_size,
	config_file_fp)) > 0) {

		if (prompt_line_found) {
			if (strncmp(line, "#END OF PROMPT", 14) == 0)
				break;
			if (*line != '#') {
				prompt_cmds = (char **)xrealloc(prompt_cmds,
									(prompt_cmds_n + 1) * sizeof(char *));
				prompt_cmds[prompt_cmds_n++] = savestring(
											line, strlen(line));
			}
		}

		else if (strncmp(line, "#PROMPT", 7) == 0)
			prompt_line_found = 1;
	}

	free(line);

	fclose(config_file_fp);
}

static int
xalphasort(const struct dirent **a, const struct dirent **b)
/* Same as alphasort, but is uses strcmp instead of sctroll, which is
 * slower. However, bear in mind that, unlike strcmp(), strcoll() is locale
 * aware. Use only with C and english locales */
{
	int ret = 0;

	/* The if statements prevent strcmp from running in every
	 * call to the function (it will be called only if the first
	 * character of the two strings is the same), which makes the
	 * function faster */
	if ((*a)->d_name[0] > (*b)->d_name[0])
		ret = 1;

	else if ((*a)->d_name[0] < (*b)->d_name[0])
		ret = -1;

	else
		ret = strcmp((*a)->d_name, (*b)->d_name);

	if (!sort_reverse)
		return ret;

	/* If sort_reverse, return the opposite value */
	return (ret - (ret * 2));
}

static int
skip_nonexec(const struct dirent *ent)
{
	if (access(ent->d_name, R_OK) == -1)
		return 0;

	return 1;
}

static void
get_path_programs(void)
/* Get the list of files in PATH, plus CliFM internal commands, and send
 * them into an array to be read by my readline custom auto-complete
 * function (my_rl_completion) */
{
	struct dirent ***commands_bin = (struct dirent ***)xnmalloc(
									path_n, sizeof(struct dirent));
	int i, j, l = 0, total_cmd = 0;
	int *cmd_n = (int *)xnmalloc(path_n, sizeof(int));

	i = (int)path_n;
	while (--i >= 0) {

		if (!paths[i] || !*paths[i] || xchdir(paths[i], NO_TITLE) == -1) {
			cmd_n[i] = 0;
			continue;
		}

		cmd_n[i] = scandir(paths[i], &commands_bin[i], skip_nonexec,
						   xalphasort);
		/* If paths[i] directory does not exist, scandir returns -1.
		 * Fedora, for example, adds $HOME/bin and $HOME/.local/bin to
		 * PATH disregarding if they exist or not. If paths[i] dir is
		 * empty do not use it either */
		if (cmd_n[i] > 0)
			total_cmd += (size_t)cmd_n[i];
	}

	xchdir(ws[cur_ws].path, NO_TITLE);

	/* Add internal commands */
	/* Get amount of internal cmds (elements in INTERNAL_CMDS array) */
	size_t internal_cmd_n = (sizeof(INTERNAL_CMDS) /
						  sizeof(INTERNAL_CMDS[0])) - 1;

	bin_commands = (char **)xnmalloc(total_cmd + internal_cmd_n +
								aliases_n + actions_n + 2, sizeof(char *));

	i = (int)internal_cmd_n;
	while (--i >= 0)
		bin_commands[l++] = savestring(INTERNAL_CMDS[i],
									  strlen(INTERNAL_CMDS[i]));

	/* Add commands in PATH */
	i = (int)path_n;
	while (--i >= 0) {

		if (cmd_n[i] <= 0)
			continue;

		j = cmd_n[i];
		while (--j >= 0) {

			bin_commands[l++] = savestring(commands_bin[i][j]->d_name,
								  strlen(commands_bin[i][j]->d_name));
			free(commands_bin[i][j]);
		}

		free(commands_bin[i]);
	}

	free(commands_bin);
	free(cmd_n);

	/* Now add aliases, if any */
	if (aliases_n) {

		i = (int)aliases_n;
		while (--i >= 0) {

			int index = strcntchr(aliases[i], '=');

			if (index != -1) {
				bin_commands[l] = (char *)xnmalloc((size_t)index + 1,
												   sizeof(char));
				strncpy(bin_commands[l++], aliases[i], (size_t)index);
			}
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

	path_progsn = (size_t)l;
	bin_commands[l] = (char *)NULL;
}

static char *
get_cmd_path(const char *cmd)
/* Get the path of a given command from the PATH environment variable.
 * It basically does the same as the 'which' Unix command */
{
	char *cmd_path = (char *)NULL;
	size_t i;

	cmd_path = (char *)xnmalloc(PATH_MAX + 1, sizeof(char));

	for (i = 0; i < path_n; i++) { /* Get each path from PATH */
		/* Append cmd to each path and check if it exists and is
		 * executable */
		snprintf(cmd_path, PATH_MAX, "%s/%s", paths[i], cmd);

		if (access(cmd_path, X_OK) == 0)
			return cmd_path;
	}

	/* If cmd was not found */
	free(cmd_path);

	return (char *)NULL;
}

static void
edit_xresources(void)
{
	if (xargs.stealth_mode == 1)
		return;

	/* Check if ~/.Xresources exists and eightBitInput is set to
	 * false. If not, create the file and set the corresponding
	 * value */
	char xresources[PATH_MAX] = "";
	sprintf(xresources, "%s/.Xresources", user_home);

	FILE *xresources_fp = fopen(xresources, "a+");

	if (!xresources_fp) {
		_err('e', PRINT_PROMPT, "%s: fopen: '%s': %s\n",
			 PROGRAM_NAME, xresources, strerror(errno));
		return;
	}

	/* Since I'm looking for very specific lines, which are
	 * fixed lines far below MAX_LINE, I don't care to get
	 * any of the remaining lines truncated */
#if __FreeBSD__
		fseek(xresources_fp, 0, SEEK_SET);
#endif
	char line[256] = "";
	int eight_bit = 0, cursor = 0, function = 0;

	while (fgets(line, (int)sizeof(line), xresources_fp)) {

		if (strncmp(line, "XTerm*eightBitInput: false",
		26) == 0)
			eight_bit = 1;

		else if (strncmp(line, "XTerm*modifyCursorKeys: 1",
		25) == 0)
			cursor = 1;

		else if (strncmp(line, "XTerm*modifyFunctionKeys: 1",
		27) == 0)
			function = 1;
	}

	if (!eight_bit || !cursor || !function) {
		/* Set the file position indicator at the end of
		 * the file */
		fseek(xresources_fp, 0L, SEEK_END);

		if (!eight_bit)
			fputs("\nXTerm*eightBitInput: false\n",
				  xresources_fp);

		if (!cursor)
			fputs("\nXTerm*modifyCursorKeys: 1\n",
				  xresources_fp);

		if (!function)
			fputs("\nXTerm*modifyFunctionKeys: 1\n",
				  xresources_fp);

		char *xrdb_path = get_cmd_path("xrdb");

		if (xrdb_path) {
			char *res_file = (char *)xnmalloc(
									 strlen(user_home) + 13, sizeof(char));
			sprintf(res_file, "%s/.Xresources", user_home);
			char *cmd[] = { "xrdb", "merge", res_file,
							NULL };
			launch_execve(cmd, FOREGROUND, E_NOFLAG);
			free(res_file);
		}

		_err('w', PRINT_PROMPT, _("%s: Restart your %s for "
			 "changes to ~/.Xresources to take effect. "
			 "Otherwise, %s keybindings might not work as "
			 "expected.\n"), PROGRAM_NAME, (xrdb_path)
			 ? _("terminal") : _("X session"),
			 PROGRAM_NAME);

		if (xrdb_path)
			free(xrdb_path);
	}

	fclose(xresources_fp);

	return;
}

static void
define_config_file_names(void)
{
	size_t pnl_len = strlen(PNL);

	/* If $XDG_CONFIG_HOME is set, use it for the config file.
	 * Else, fall back to $HOME/.config */
	char *xdg_config_home = getenv("XDG_CONFIG_HOME");

	size_t xdg_config_home_len = 0;

	if (xdg_config_home) {
		xdg_config_home_len = strlen(xdg_config_home);

		CONFIG_DIR_GRAL = (char *)xnmalloc(xdg_config_home_len
							 + pnl_len + 2, sizeof(char));
		sprintf(CONFIG_DIR_GRAL, "%s/%s", xdg_config_home, PNL);

		xdg_config_home = (char *)NULL;
	}

	else {
		CONFIG_DIR_GRAL = (char *)xnmalloc(user_home_len + pnl_len
								+ 11, sizeof(char));
		sprintf(CONFIG_DIR_GRAL, "%s/.config/%s", user_home, PNL);
	}

	size_t config_gral_len = strlen(CONFIG_DIR_GRAL);

	/* alt_profile will not be NULL whenever the -P option is used
	 * to run the program under an alternative profile */
	if (alt_profile) {
		CONFIG_DIR = (char *)xnmalloc(config_gral_len
							 + strlen(alt_profile) + 11, sizeof(char));
		sprintf(CONFIG_DIR, "%s/profiles/%s", CONFIG_DIR_GRAL,
				alt_profile);
	}

	else {
		CONFIG_DIR = (char *)xnmalloc(config_gral_len + 18, sizeof(char));
		sprintf(CONFIG_DIR, "%s/profiles/default", CONFIG_DIR_GRAL);
	}

	if (alt_kbinds_file) {
		KBINDS_FILE = savestring(alt_kbinds_file, strlen(alt_kbinds_file));
		free(alt_kbinds_file);
		alt_kbinds_file = (char *)NULL;
	}

	else {
		/* Keybindings per user, not per profile */
		KBINDS_FILE = (char *)xnmalloc(config_gral_len + 13, sizeof(char));
		sprintf(KBINDS_FILE, "%s/keybindings",
				CONFIG_DIR_GRAL);
	}

	COLORS_DIR = (char *)xnmalloc(config_gral_len + 8, sizeof(char));
	sprintf(COLORS_DIR, "%s/colors", CONFIG_DIR_GRAL);

	PLUGINS_DIR = (char *)xnmalloc(config_gral_len + 9, sizeof(char));
	sprintf(PLUGINS_DIR, "%s/plugins", CONFIG_DIR_GRAL);

	TRASH_DIR = (char *)xnmalloc(user_home_len + 20, sizeof(char));
	sprintf(TRASH_DIR, "%s/.local/share/Trash", user_home);

	size_t trash_len = strlen(TRASH_DIR);

	TRASH_FILES_DIR = (char *)xnmalloc(trash_len + 7, sizeof(char));
	sprintf(TRASH_FILES_DIR, "%s/files", TRASH_DIR);

	TRASH_INFO_DIR = (char *)xnmalloc(trash_len + 6, sizeof(char));
	sprintf(TRASH_INFO_DIR, "%s/info", TRASH_DIR);

	size_t config_len = strlen(CONFIG_DIR);

	DIRHIST_FILE = (char *)xnmalloc(config_len + 13, sizeof(char));
	sprintf(DIRHIST_FILE, "%s/dirhist.cfm", CONFIG_DIR);

	if (!alt_bm_file) {
		BM_FILE = (char *)xnmalloc(config_len + 15, sizeof(char));
		sprintf(BM_FILE, "%s/bookmarks.cfm", CONFIG_DIR);
	}

	else {
		BM_FILE = savestring(alt_bm_file, strlen(alt_bm_file));
		free(alt_bm_file);
		alt_bm_file = (char *)NULL;
	}

	LOG_FILE = (char *)xnmalloc(config_len + 9, sizeof(char));
	sprintf(LOG_FILE, "%s/log.cfm", CONFIG_DIR);

	HIST_FILE = (char *)xnmalloc(config_len + 13, sizeof(char));
	sprintf(HIST_FILE, "%s/history.cfm", CONFIG_DIR);

	if (!alt_config_file) {
		CONFIG_FILE = (char *)xnmalloc(config_len + pnl_len + 4, sizeof(char));
		sprintf(CONFIG_FILE, "%s/%src", CONFIG_DIR, PNL);
	}

	else {
		CONFIG_FILE = savestring(alt_config_file, strlen(alt_config_file));
		free(alt_config_file);
		alt_config_file = (char *)NULL;
	}

	PROFILE_FILE = (char *)xnmalloc(config_len + 13, sizeof(char));
	sprintf(PROFILE_FILE, "%s/profile.cfm", CONFIG_DIR);

	MSG_LOG_FILE = (char *)xnmalloc(config_len + 14, sizeof(char));
	sprintf(MSG_LOG_FILE, "%s/messages.cfm", CONFIG_DIR);

	MIME_FILE = (char *)xnmalloc(config_len + 14, sizeof(char));
	sprintf(MIME_FILE, "%s/mimelist.cfm", CONFIG_DIR);

	ACTIONS_FILE = (char *)xnmalloc(config_len + 13, sizeof(char));
	sprintf(ACTIONS_FILE, "%s/actions.cfm", CONFIG_DIR);

	return;
}

static int
create_config(const char *file)
{
	FILE *config_fp = fopen(file, "w");

	if (!config_fp) {
		fprintf(stderr, "%s: fopen: %s: %s\n", PROGRAM_NAME,
				file, strerror(errno));
		return EXIT_FAILURE;
	}

	/* Do not translate anything in the config file */
	fprintf(config_fp,

"\t\t###########################################\n\
\t\t#                  CLIFM                  #\n\
\t\t#  The anti-eye-candy, KISS file manager  #\n\
\t\t###########################################\n\n"

"# This is the configuration file for CliFM\n\n"

"# Color schemes are stored in the colors directory. By default,\n\
# the 'default' color scheme is used. Visit %s\n\
# to get a few more\n\
ColorScheme=default\n\n"

"# The amount of files contained by a directory is informed next\n\
# to the directory name. However, this feature might slow things down when,\n\
# for example, listing files on a remote server. The filescounter can be\n\
# disabled here, via the --no-files-counter option, or using the 'fc'\n\
# command while in the program itself.\n\
FilesCounter=true\n\n"

"# The character used to construct the line dividing the list of files and\n\
# the prompt. DividingLineChar accepts both literal characters (in single\n\
# quotes) and decimal numbers.\n\
DividingLineChar='-'\n\n"

"# If set to true, print a map of the current position in the directory\n\
# history list, showing previous, current, and next entries\n\
DirhistMap=false\n\n"

"# Use a regex expression to exclude filenames when listing files.\n\
# Example: .*~$ to exclude backup files (ending with ~). Do not quote\n\
# the regular expression\n\
Filter=\n\n"

"# Set the default copy command. Available options are: 0 = cp,\n\
# 1 = advcp, and 2 = wcp. Both 1 and 2 add a progress bar to cp.\n\
cpCmd=0\n\n"

"# Set the default move command. Available options are: 0 = mv,\n\
# and 1 = advmv. 1 adds a progress bar to mv.\n\
mvCmd=0\n\n"

"# The prompt line is built using string literals and/or one or more of\n\
# the following escape sequences:\n"
"# \\xnn: The character whose hexadecimal code is nn.\n\
# \\e: Escape character\n\
# \\h: The hostname, up to the first dot\n\
# \\u: The username\n\
# \\H: The full hostname\n\
# \\n: A newline character\n\
# \\r: A carriage return\n\
# \\a: A bell character\n\
# \\d: The date, in abbrevieted form (ex: 'Tue May 26')\n\
# \\s: The name of the shell (everything after the last slash) currently used\n\
# by CliFM\n\
# \\S: The number of the current workspace\n\
# \\l: Print an 'L' if running in light mode\n\
# \\t: The time, in 24-hour HH:MM:SS format\n\
# \\T: The time, in 12-hour HH:MM:SS format\n\
# \\@: The time, in 12-hour am/pm format\n\
# \\A: The time, in 24-hour HH:MM format\n\
# \\w: The full current working directory, with $HOME abbreviated with a tilde\n\
# \\W: The basename of $PWD, with $HOME abbreviated with a tilde\n\
# \\p: A mix of the two above, it abbreviates the current working directory \n\
# only if longer than PathMax (a value defined in the configuration file).\n\
# \\z: Exit code of the last executed command. :) if success and :( in case of\n\
# error\n\
# \\$ '#', if the effective user ID is 0, and '$' otherwise\n\
# \\nnn: The character whose ASCII code is the octal value nnn\n\
# \\\\: A backslash\n\
# \\[: Begin a sequence of non-printing characters. This is mostly used to\n\
# add color to the prompt line\n\
# \\]: End a sequence of non-printing characters\n\n"

"Prompt=\"%s\"\n\n",

			COLORS_REPO, DEFAULT_PROMPT);

	fprintf(config_fp,
"# MaxPath is only used for the /p option of the prompt: the current working\n\
# directory will be abbreviated to its basename (everything after last slash)\n\
# whenever the current path is longer than MaxPath.\n\
MaxPath=40\n\n"

"WelcomeMessage=true\n\
SplashScreen=false\n\
ShowHiddenFiles=false\n\
LongViewMode=false\n\
LogCmds=false\n\n"

"# Minimum length at which a filename can be trimmed in long view mode\n\
# (including ELN length and spaces)\n\
MinFilenameTrim=20\n\n"

"# When a directory rank in the jump database is below MinJumpRank, it\n\
# will be forgotten\n\
MinJumpRank=10\n\n"

"# When the sum of all ranks in the jump database reaches MaxJumpTotalRank,\n\
# all ranks will be reduced 10%%, and those falling below MinJumpRank will\n\
# be deleted\n\
MaxJumpTotalRank=100000\n\n"

"# Should CliFM be allowed to run external, shell commands?\n\
ExternalCommands=false\n\n"

" Write the last visited directory to $XDG_CONFIG_HOME/clifm/.last to be\n\
# later accessed by the corresponding shell function at program exit.\n\
# To enable this feature consult the manpage.\n\
CdOnQuit=false\n\n"

"# If set to true, a command name that is the name of a directory or a\n\
# file is executed as if it were the argument to the the 'cd' or the \n\
# 'open' commands respectivelly: 'cd DIR' works the same as just 'DIR'\n\
# and 'open FILE' works the same as just 'FILE'.\n\
Autocd=true\n\
AutoOpen=true\n\n"

"# If set to true, expand bookmark names into the corresponding bookmark\n\
# path: if the bookmark is \"name=/path\", \"name\" will be interpreted\n\
# as /path. TAB completion is also available for bookmark names.\n\
ExpandBookmarks=false\n\n"

"# In light mode, extra filetype checks (except those provided by\n\
# the d_type field of the dirent structure (see readdir(3))\n\
# are disabled to speed up the listing process. stat(3) and access(3)\n\
# are not executed at all, so that we cannot know in advance if a file\n\
# is readable by the current user, if it is executable, SUID, SGID, if a\n\
# symlink is broken, and so on. The file extension check is ignored as\n\
# well, so that the color per extension feature is disabled.\n\
LightMode=false\n\n");

	fprintf(config_fp,
"# If running with colors, append directory indicator and files counter\n\
# to directories. If running without colors (via the --no-colors option),\n\
# append filetype indicator at the end of filenames: '/' for directories,\n\
# '@' for symbolic links, '=' for sockets, '|' for FIFO/pipes, '*'\n\
# for for executable files, and '?' for unknown file types. Bear in mind\n\
# that when running in light mode the check for executable files won't be\n\
# performed, and thereby no inidicator will be added to executable files.\n\
Classify=true\n\n"

"# Should the Selection Box be shared among different profiles?\n\
ShareSelbox=false\n\n"

"# Choose the resource opener to open files with their default associated\n\
# application. If not set, 'lira', CLiFM's built-in opener, is used.\n\
Opener=\n\n"

"# Set the shell to be used when running external commands. Defaults to the\n\
# user's shell as specified in '/etc/passwd'.\n\
SystemShell=\n\n"

"# Only used when opening a directory via a new CliFM instance (with the 'x'\n\
# command), this option specifies the command to be used to launch a\n\
# terminal emulator to run CliFM on it.\n\
TerminalCmd='%s'\n\n"

"# Choose sorting method: 0 = none, 1 = name, 2 = size, 3 = atime\n\
# 4 = btime (ctime if not available), 5 = ctime, 6 = mtime, 7 = version\n\
# (name if note available) 8 = extension, 9 = inode, 10 = owner, 11 = group\n\
# NOTE: the 'version' method is not available on FreeBSD\n\
Sort=1\n\
# By default, CliFM sorts files from less to more (ex: from 'a' to 'z' if\n\
# using the \"name\" method). To invert this ordering, set SortReverse to\n\
# true (you can also use the --sort-reverse option or the 'st' command)\n\
SortReverse=false\n\n"

"Tips=true\n\
ListFoldersFirst=true\n\
CdListsAutomatically=true\n\
CaseSensitiveList=false\n\
CaseSensitiveDirJump=true\n\
CaseSensitivePathComp=true\n\
Unicode=false\n\
Pager=false\n\
MaxHistory=1000\n\
MaxDirhist=100\n\
MaxLog=500\n\
DiskUsage=false\n\n"

"# If set to true, clear the screen before listing files\n\
ClearScreen=true\n\n"

"# If not specified, StartingPath defaults to the current working\n\
# directory.\n\
StartingPath=\n\n"

"# If set to true, start CliFM in the last visited directory (and in the\n\
# last used workspace). This option overrides StartingPath.\n\
RestoreLastPath=false\n\n"

"# If set to true, the 'r' command executes 'trash' instead of 'rm' to\n\
# prevent accidental deletions.\n\
TrashAsRm=false\n\n"

"# Set readline editing mode: 0 for vi and 1 for emacs (default).\n\
RlEditMode=1\n\n"

"#END OF OPTIONS\n\n",

			DEFAULT_TERM_CMD);

	fputs(

"#ALIASES\n\
#alias ls='ls --color=auto -A'\n\n"

"#PROMPT COMMANDS\n\n"
"# Write below the commands you want to be executed before the prompt.\n\
# Ex:\n\
#date | awk '{print $1\", \"$2,$3\", \"$4}'\n\n"
"#END OF PROMPT COMMANDS\n\n", config_fp);

	fclose(config_fp);

	return EXIT_SUCCESS;
}

static void
create_def_cscheme(void)
{
	if (!COLORS_DIR)
		return;

	char *cscheme_file = (char *)xnmalloc(strlen(COLORS_DIR) + 13,
										  sizeof(char));

	sprintf(cscheme_file, "%s/default.cfm", COLORS_DIR);

	/* If the file already exists, do nothing */
	struct stat attr;

	if (stat(cscheme_file, &attr) != -1) {
		free(cscheme_file);
		return;
	}

	FILE *fp = fopen(cscheme_file, "w+");

	if (!fp) {
		_err('w', PRINT_PROMPT, "%s: Error creating default color "
			 "scheme file\n", PROGRAM_NAME);
		free(cscheme_file);
		return;
	}

	fprintf(fp, "# CliFM default color scheme\n\n\
# FiletypeColors defines the color used for filetypes when listing files, \n\
# just as InterfaceColors defines colors for CliFM interface. Both make\n\
# use of the same format used by the LS_COLORS environment variable. Thus,\n\
# \"di=01;34\" means that (non-empty) directories will be listed in bold blue.\n\
# Color codes are traditional ANSI escape sequences less the escape char and\n\
# the final 'm'. 8 bit, 256 colors, and RGB colors are supported.\n\
# A detailed explanation of all these codes can be found in the manpage.\n\n"

"FiletypeColors=\"%s\"\n\n"

"InterfaceColors=\"%s\"\n\n"

"# Same as FiletypeColors, but for file extensions. The format is always\n\
# *.EXT=COLOR\n"
"ExtColors=\"%s\"\n\n"

"DirIconsColor=\"00;33\"\n",
			DEF_FILE_COLORS, DEF_IFACE_COLORS, DEF_EXT_COLORS);

	fclose(fp);

	free(cscheme_file);

	return;
}

static void
create_config_files(void)
{
	int ret = -1;
	struct stat file_attrib;

			/* #############################
			 * #        TRASH DIRS         #
			 * ############################# */

	if (stat(TRASH_DIR, &file_attrib) == -1) {
		char *trash_files = (char *)NULL;
		trash_files = (char *)xnmalloc(strlen(TRASH_DIR) + 7, sizeof(char));

		sprintf(trash_files, "%s/files", TRASH_DIR);
		char *trash_info = (char *)NULL;
		trash_info = (char *)xnmalloc(strlen(TRASH_DIR) + 6, sizeof(char));

		sprintf(trash_info, "%s/info", TRASH_DIR);
		char *cmd[] = { "mkdir", "-p", trash_files, trash_info, NULL };

		ret = launch_execve(cmd, FOREGROUND, E_NOFLAG);
		free(trash_files);
		free(trash_info);

		if (ret != EXIT_SUCCESS) {
			trash_ok = 0;
			_err('w', PRINT_PROMPT, _("%s: mkdir: '%s': Error "
				 "creating trash directory. Trash function "
				 "disabled\n"), PROGRAM_NAME, TRASH_DIR);
		}
	}

	/* If it exists, check it is writable */
	else if (access (TRASH_DIR, W_OK) == -1) {
		trash_ok = 0;
		_err('w', PRINT_PROMPT, _("%s: '%s': Directory not writable. "
			 "Trash function disabled\n"), PROGRAM_NAME, TRASH_DIR);
	}

				/* ####################
				 * #    CONFIG DIR    #
				 * #################### */

	/* If the config directory doesn't exist, create it */
	/* Use the GNU mkdir to let it handle parent directories */
	if (stat(CONFIG_DIR, &file_attrib) == -1) {
		char *tmp_cmd[] = { "mkdir", "-p", CONFIG_DIR, NULL };

		if (launch_execve(tmp_cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS) {

			config_ok = 0;

			_err('e', PRINT_PROMPT, _("%s: mkdir: '%s': Error "
				 "creating configuration directory. Bookmarks, "
				 "commands logs, and command history are disabled. "
				 "Program messages won't be persistent. Using "
				 "default options\n"), PROGRAM_NAME, CONFIG_DIR);

			return;
		}
	}

	/* If it exists, check it is writable */
	else if (access(CONFIG_DIR, W_OK) == -1) {

		config_ok = 0;

		_err('e', PRINT_PROMPT, _("%s: '%s': Directory not writable. "
			 "Bookmarks, commands logs, and commands history are "
			 "disabled. Program messages won't be persistent. "
			 "Using default options\n"), PROGRAM_NAME, CONFIG_DIR);

		return;
	}

				/* #####################
				 * #    CONFIG FILE    #
				 * #####################*/

	if (stat(CONFIG_FILE, &file_attrib) == -1) {

		if (create_config(CONFIG_FILE) == EXIT_SUCCESS)
			config_ok = 1;

		else
			config_ok = 0;
	}

	if (!config_ok)
		return;

				/* ######################
				 * #    PROFILE FILE    #
				 * ###################### */

	if (stat(PROFILE_FILE, &file_attrib) == -1) {

		FILE *profile_fp = fopen(PROFILE_FILE, "w");

		if (!profile_fp) {
			_err('e', PRINT_PROMPT, "%s: fopen: '%s': %s\n",
				 PROGRAM_NAME, PROFILE_FILE, strerror(errno));
		}

		else {
			fprintf(profile_fp, _("#%s profile\n\
#Write here the commands you want to be executed at startup\n\
#Ex:\n#echo -e \"%s, the anti-eye-candy/KISS file manager\"\n"),
					PROGRAM_NAME, PROGRAM_NAME);
			fclose(profile_fp);
		}
	}

				/* #####################
				 * #    COLORS DIR     #
				 * ##################### */

	if (stat(COLORS_DIR, &file_attrib) == -1) {

		char *cmd[] = { "mkdir", COLORS_DIR, NULL };

		if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS) {
			_err('w', PRINT_PROMPT, _("%s: mkdir: Error "
				 "creating colors directory. Using the default "
				 "color scheme\n"), PROGRAM_NAME);
		}
	}

	/* Generate the default color scheme file */
	create_def_cscheme();

				/* #####################
				 * #      PLUGINS      #
				 * #####################*/

	if (stat(PLUGINS_DIR, &file_attrib) == -1) {
		char *cmd[] = { "mkdir", PLUGINS_DIR, NULL };

		if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
			_err('e', PRINT_PROMPT, _("%s: mkdir: Error "
				 "creating scripts directory. The "
				 "actions function is disabled\n"), PROGRAM_NAME);
		else
			copy_plugins();
	}

				/* #####################
				 * #    ACTIONS FILE   #
				 * #####################*/

	if (stat(ACTIONS_FILE, &file_attrib) == -1) {
		FILE *actions_fp = fopen(ACTIONS_FILE, "w");

		if (!actions_fp) {
			_err('e', PRINT_PROMPT, "%s: '%s': %s\n", PROGRAM_NAME,
				 ACTIONS_FILE, strerror(errno));
		}

		else {
			fprintf(actions_fp, "######################\n"
				"# %s actions file #\n"
				"######################\n\n"
				"# Define here your custom actions. Actions are "
				"custom command names\n"
				"# binded to a shell script located in "
				"$XDG_CONFIG_HOME/clifm/PROFILE/scripts.\n"
				"# Actions can be executed directly from "
				"%s command line, as if they\n"
				"# were any other command, and the associated "
				"script will be executed\n"
				"# instead. All parameters passed to the action "
				"command will be passed\n"
				"# to the action script as well.\n\n"
				"i=img_viewer.sh\n"
				"kbgen=kbgen\n"
				"vid=vid_viewer.sh\n"
				"ptot=pdf_viewer.sh\n"
				"music=music_player.sh\n"
				"update=update.sh\n"
				"wall=wallpaper_setter.sh\n"
				"dragon=dragondrop.sh\n"
				"+=finder.sh\n"
				"++=jumper.sh\n"
				"-=fzfnav.sh\n"
				"*=fzfsel.sh\n"
				"**=fzfdesel.sh\n"
				"h=fzfhist.sh\n"
				"//=rgfind.sh\n"
				"ih=ihelp.sh\n",
				PROGRAM_NAME, PROGRAM_NAME);

			fclose(actions_fp);
		}
	}

				/* #####################
				 * #     MIME FILE     #
				 * #####################*/

	if (stat(MIME_FILE, &file_attrib) == 0)
		return;

	_err('n', PRINT_PROMPT, _("%s created a new MIME list file "
		 "(%s). It is recommended to edit this file (entering "
		 "'mm edit' or pressing F6) to add the programs "
		 "you use and remove those you don't. This will make "
		 "the process of opening files faster and smoother\n"),
		 PROGRAM_NAME, MIME_FILE);


	/* Try importing MIME associations from the system, and in
	 * case nothing can be imported, create an empty MIME list
	 * file */
	if (mime_import(MIME_FILE) != EXIT_SUCCESS) {

		FILE *mime_fp = fopen(MIME_FILE, "w");

		if (!mime_fp) {
			_err('e', PRINT_PROMPT, "%s: fopen: '%s': %s\n",
				 PROGRAM_NAME, MIME_FILE, strerror(errno));
		}

		else {

			if (!(flags & GUI))
				fputs("text/plain=nano;vim;vi;emacs;ed\n"
					  "*.cfm=nano;vim;vi;emacs;ed\n", mime_fp);

			else
				fputs("text/plain=gedit;kate;pluma;mousepad;"
					  "leafpad;nano;vim;vi;emacs;ed\n"
					  "*.cfm=gedit;kate;pluma;mousepad;leafpad;"
					  "nano;vim;vi;emacs;ed\n", mime_fp);

			fclose(mime_fp);
		}
	}
}

static void
read_config(void)
{
	int ret = -1;

	FILE *config_fp = fopen(CONFIG_FILE, "r");

	if (!config_fp) {
		_err('e', PRINT_PROMPT, _("%s: fopen: '%s': %s. Using "
			 "default values.\n"), PROGRAM_NAME, CONFIG_FILE,
			 strerror(errno));
		return;
	}

	if (xargs.rl_vi_mode == 1)
		rl_vi_editing_mode(1, 0);

	div_line_char = UNSET;
#define MAX_BOOL 6
	/* starting path(14) + PATH_MAX + \n(1)*/
	char line[PATH_MAX + 15];

	while (fgets(line, (int)sizeof(line), config_fp)) {

		if (*line == '\n' || (*line == '#' && line[1] != 'E'))
			continue;

		if (*line == '#'
		&& strncmp(line, "#END OF OPTIONS", 15) == 0)
			break;

		/* Check for the xargs.splash flag. If -1, it was
		 * not set via command line, so that it must be
		 * set here */
		else if (xargs.splash == UNSET && *line == 'S'
		&& strncmp(line, "SplashScreen=", 13) == 0) {

			char opt_str[MAX_BOOL] = ""; /* false (5) + 1 */
			ret = sscanf(line, "SplashScreen=%5s\n", opt_str);
			/* According to cppcheck: "sscanf() without field
			 * width limits can crash with huge input data".
			 * Field width limits = %5s */

			if (ret == -1)
				continue;

			if (strncmp(opt_str, "true", 4) == 0)
				splash_screen = 1;

			else if (strncmp(opt_str, "false", 5) == 0)
				splash_screen = 0;
		}

		else if (xargs.case_sens_dirjump == UNSET && *line == 'C'
		&& strncmp(line, "CaseSensitiveDirJump=", 21) == 0) {

			char opt_str[MAX_BOOL] = ""; /* false (5) + 1 */
			ret = sscanf(line, "CaseSensitiveDirJump=%5s\n", opt_str);

			if (ret == -1)
				continue;

			if (strncmp(opt_str, "true", 4) == 0)
				case_sens_dirjump = 1;

			else if (strncmp(opt_str, "false", 5) == 0)
				case_sens_dirjump = 0;
		}

		else if (xargs.case_sens_path_comp == UNSET && *line == 'C'
		&& strncmp(line, "CaseSensitivePathComp=", 22) == 0) {

			char opt_str[MAX_BOOL] = ""; /* false (5) + 1 */
			ret = sscanf(line, "CaseSensitivePathComp=%5s\n", opt_str);

			if (ret == -1)
				continue;

			if (strncmp(opt_str, "true", 4) == 0)
				case_sens_path_comp = 1;

			else if (strncmp(opt_str, "false", 5) == 0)
				case_sens_path_comp = 0;
		}

		else if (!filter && *line == 'F'
		&& strncmp(line, "Filter=", 7) == 0) {

			char *opt_str = strchr(line, '=');

			if (!opt_str)
				continue;

			size_t len = strlen(opt_str);
			if (opt_str[len - 1] == '\n')
				opt_str[len - 1] = '\0';

			if (!*(++opt_str))
				continue;

			filter = savestring(opt_str, len);
		}

		else if (!usr_cscheme && *line == 'C'
		&& strncmp(line, "ColorScheme=", 12) == 0) {

			char *opt_str = (char *)NULL;
			opt_str = strchr(line, '=');

			if (!opt_str)
				continue;

			size_t len = strlen(opt_str);
			if (opt_str[len - 1] == '\n')
				opt_str[len - 1] = '\0';

			if (!*(++opt_str))
				continue;

			usr_cscheme = savestring(opt_str, len);
		}

		else if (xargs.light == UNSET && *line == 'L'
		&& strncmp(line, "LightMode=", 10) == 0) {

			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "LightMode=%5s\n", opt_str);

			if (ret == -1)
				continue;

			if (strncmp(opt_str, "true", 4) == 0)
				light_mode = 1;

			else if (strncmp(opt_str, "false", 5) == 0)
				light_mode = 0;
		}

		else if (xargs.trasrm == UNSET && *line == 'T'
		&& strncmp(line, "TrashAsRm=", 10) == 0) {

			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "TrashAsRm=%5s\n", opt_str);

			if (ret == -1)
				continue;

			if (strncmp(opt_str, "true", 4) == 0)
				tr_as_rm = 1;

			else if (strncmp(opt_str, "false", 5) == 0)
				tr_as_rm = 0;
		}

		else if (xargs.cd_on_quit == UNSET && *line == 'C'
		&& strncmp(line, "CdOnQuit=", 9) == 0) {

			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "CdOnQuit=%5s\n", opt_str);

			if (ret == -1)
				continue;

			if (strncmp(opt_str, "true", 4) == 0)
				cd_on_quit = 1;

			else if (strncmp(opt_str, "false", 5) == 0)
				cd_on_quit = 0;
		}

		else if (xargs.expand_bookmarks == UNSET && *line == 'E'
		&& strncmp(line, "ExpandBookmarks=", 16) == 0) {

			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "ExpandBookmarks=%5s\n",
						 opt_str);
			if (ret == -1)
				continue;

			if (strncmp(opt_str, "true", 4) == 0)
				expand_bookmarks = 1;

			else if (strncmp(opt_str, "false", 5) == 0)
				expand_bookmarks = 0;
		}

		else if (xargs.restore_last_path == UNSET && *line == 'R'
		&& strncmp(line, "RestoreLastPath=", 16) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "RestoreLastPath=%5s\n",
						 opt_str);
			if (ret == -1)
				continue;

			if (strncmp(opt_str, "true", 4) == 0)
				restore_last_path = 1;

			else if (strncmp(opt_str, "false", 5) == 0)
				restore_last_path = 0;
		}

		else if (!opener && *line == 'O'
		&& strncmp(line, "Opener=", 7) == 0) {

			char *opt_str = (char *)NULL;
			opt_str = straft(line, '=');

			if (!opt_str)
				continue;

			char *tmp = remove_quotes(opt_str);

			if (!tmp) {
				free(opt_str);
				continue;
			}

			opener = savestring(tmp, strlen(tmp));
			free(opt_str);
		}

		else if (xargs.tips == UNSET && *line == 'T'
		&& strncmp(line, "Tips=", 5) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "Tips=%5s\n", opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "false", 5) == 0)
				tips = 0;
			else if (strncmp(opt_str, "true", 4) == 0)
				tips = 1;
		}

		else if (xargs.disk_usage == UNSET  && *line == 'D'
		&& strncmp(line, "DiskUsage=", 10) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "DiskUsage=%5s\n", opt_str);

			if (ret == -1)
				continue;

			if (strncmp(opt_str, "true", 4) == 0)
				disk_usage = 1;

			else if (strncmp(opt_str, "false", 5) == 0)
				disk_usage = 0;
		}

		else if (xargs.autocd == UNSET  && *line == 'A'
		&& strncmp(line, "Autocd=", 7) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "Autocd=%5s\n", opt_str);

			if (ret == -1)
				continue;

			if (strncmp(opt_str, "true", 4) == 0)
				autocd = 1;

			else if (strncmp(opt_str, "false", 5) == 0)
				autocd = 0;
		}

		else if (xargs.auto_open == UNSET  && *line == 'A'
		&& strncmp(line, "AutoOpen=", 9) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "AutoOpen=%5s\n", opt_str);

			if (ret == -1)
				continue;

			if (strncmp(opt_str, "true", 4) == 0)
				auto_open = 1;

			else if (strncmp(opt_str, "false", 5) == 0)
				auto_open = 0;
		}

		else if (xargs.dirmap == UNSET  && *line == 'D'
		&& strncmp(line, "DirhistMap=", 11) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "DirhistMap=%5s\n", opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				dirhist_map = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				dirhist_map = 0;
		}

		else if (xargs.classify == UNSET && *line == 'C'
		&& strncmp(line, "Classify=", 9) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "Classify=%5s\n", opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				classify = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				classify = 0;
		}

		else if (xargs.share_selbox == UNSET && *line == 'S'
		&& strncmp(line, "ShareSelbox=", 12) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "ShareSelbox=%5s\n", opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				share_selbox = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				share_selbox = 0;
		}

		else if (*line == 'M' && strncmp(line, "MinJumpRank=", 12) == 0) {
			int opt_num = 0;
			ret = sscanf(line, "MinJumpRank=%d\n", &opt_num);
			if (ret == -1 || opt_num < INT_MIN || opt_num > INT_MAX)
				continue;
			min_jump_rank = opt_num;
		}

		else if (*line == 'M' && strncmp(line, "MaxJumpTotalRank=", 17) == 0) {
			int opt_num = 0;
			ret = sscanf(line, "MaxJumpTotalRank=%d\n", &opt_num);
			if (ret == -1 || opt_num < INT_MIN || opt_num > INT_MAX)
				continue;
			max_jump_total_rank = opt_num;
		}

		else if (xargs.sort == UNSET && *line == 'S'
		&& strncmp(line, "Sort=", 5) == 0) {
			int opt_num = 0;
			ret = sscanf(line, "Sort=%d\n", &opt_num);
			if (ret == -1)
				continue;
			if (opt_num >= 0 && opt_num <= SORT_TYPES)
				sort = opt_num;
			else /* default (sort by name) */
				sort = DEF_SORT;
		}

		else if (*line == 'M' && strncmp(line, "MinFilenameTrim=", 16) == 0) {
			int opt_num = 0;
			ret = sscanf(line, "MinFilenameTrim=%d\n", &opt_num);
			if (ret == -1)
				continue;
			if (opt_num > 0)
				min_name_trim = opt_num;
			else /* default */
				min_name_trim = DEF_MIN_NAME_TRIM;
		}

		else if (*line == 'c' && strncmp(line, "cpCmd=", 6) == 0) {
			int opt_num = 0;
			ret = sscanf(line, "cpCmd=%d\n", &opt_num);
			if (ret == -1)
				continue;
			if (opt_num >= 0 && opt_num <= 2)
				cp_cmd = opt_num;
			else /* default (sort by name) */
				cp_cmd = DEF_CP_CMD;
		}

		else if (*line == 'm' && strncmp(line, "mvCmd=", 6) == 0) {
			int opt_num = 0;
			ret = sscanf(line, "mvCmd=%d\n", &opt_num);
			if (ret == -1)
				continue;
			if (opt_num == 0 || opt_num == 1)
				mv_cmd = opt_num;
			else /* default (sort by name) */
				mv_cmd = DEF_MV_CMD;
		}

		else if (*line == 'R' && strncmp(line, "RlEditMode=0", 12) == 0) {
			rl_vi_editing_mode(1, 0);
			/* By default, readline uses emacs editing
			 * mode */
		}

		else if (xargs.max_dirhist == UNSET && *line == 'M'
		&& strncmp(line, "MaxDirhist=", 11) == 0) {
			int opt_num = 0;
			ret = sscanf(line, "MaxDirhist=%d\n", &opt_num);
			if (ret == -1)
				continue;
			if (opt_num >= 0)
				max_dirhist = opt_num;
			else /* default */
				max_dirhist = DEF_MAX_DIRHIST;
		}

		else if (xargs.sort_reverse == UNSET && *line == 'S'
		&& strncmp(line, "SortReverse=", 12) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "SortReverse=%5s\n", opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				sort_reverse = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				sort_reverse = 0;
		}

		else if (xargs.files_counter == UNSET && *line == 'F'
		&& strncmp(line, "FilesCounter=", 13) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "FilesCounter=%5s\n", opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				files_counter = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				files_counter = 0;
		}

		else if (xargs.welcome_message == UNSET
		 && *line == 'W' && strncmp(line, "WelcomeMessage=",
		15) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "WelcomeMessage=%5s\n",
						 opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				welcome_message = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				welcome_message = 0;
		}

		else if (xargs.clear_screen == UNSET && *line == 'C'
		&& strncmp(line, "ClearScreen=", 12) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "ClearScreen=%5s\n",
						 opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				clear_screen = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				clear_screen = 0;
		}

		else if (xargs.hidden == UNSET && *line == 'S'
		&& strncmp(line, "ShowHiddenFiles=", 16) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "ShowHiddenFiles=%5s\n",
						 opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				show_hidden = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				show_hidden = 0;
		}

		else if (xargs.longview == UNSET && *line == 'L'
		&& strncmp(line, "LongViewMode=", 13) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "LongViewMode=%5s\n",
						 opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				long_view = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				long_view = 0;
		}

		else if (xargs.ext == UNSET && *line == 'E'
		&& strncmp(line, "ExternalCommands=", 17) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "ExternalCommands=%5s\n",
						 opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				ext_cmd_ok = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				ext_cmd_ok = 0;
		}

		else if (xargs.logs == UNSET && *line == 'L'
		&& strncmp(line, "LogCmds=", 8) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "LogCmds=%5s\n", opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				logs_enabled = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				logs_enabled = 0;
		}

		else if (*line == 'S' && strncmp(line, "SystemShell=", 12) == 0) {
			if (sys_shell) {
				free(sys_shell);
				sys_shell = (char *)NULL;
			}
			char *opt_str = straft(line, '=');
			if (!opt_str)
				continue;

			char *tmp = remove_quotes(opt_str);
			if (!tmp) {
				free(opt_str);
				continue;
			}

			if (*tmp == '/') {
				if (access(tmp, F_OK|X_OK) != 0) {
					free(opt_str);
					continue;
				}

				sys_shell = savestring(tmp, strlen(tmp));
			}

			else {
				char *shell_path = get_cmd_path(tmp);

				if (!shell_path) {
					free(opt_str);
					continue;
				}

				sys_shell = savestring(shell_path, strlen(shell_path));
				free(shell_path);
			}

			free(opt_str);
		}

		else if (*line == 'T' && strncmp(line, "TerminalCmd=", 12) == 0) {

			if (term) {
				free(term);
				term = (char *)NULL;
			}

			char *opt_str = straft(line, '=');
			if (!opt_str)
				continue;

			char *tmp = remove_quotes(opt_str);
			if (!tmp) {
				free(opt_str);
				continue;
			}

			term = savestring(tmp, strlen(tmp));
			free(opt_str);
		}

		else if (xargs.ffirst == UNSET  && *line == 'L'
		&& strncmp(line, "ListFoldersFirst=", 17) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "ListFoldersFirst=%5s\n",
						 opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				list_folders_first = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				list_folders_first = 0;
		}

		else if (xargs.cd_list_auto == UNSET && *line == 'C'
		&& strncmp(line, "CdListsAutomatically=",
		21) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "CdListsAutomatically=%5s\n",
						 opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				cd_lists_on_the_fly = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				cd_lists_on_the_fly = 0;
		}

		else if (xargs.sensitive == UNSET && *line == 'C'
		&& strncmp(line, "CaseSensitiveList=", 18) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "CaseSensitiveList=%5s\n",
					   opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				case_sensitive = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				case_sensitive = 0;
		}

		else if (xargs.unicode == UNSET && *line == 'U'
		&& strncmp(line, "Unicode=", 8) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "Unicode=%5s\n", opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				unicode = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				unicode = 0;
		}

		else if (xargs.pager == UNSET && *line == 'P'
		&& strncmp(line, "Pager=", 6) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "Pager=%5s\n", opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				pager = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				pager = 0;
		}

		else if (*line == 'P' && strncmp(line, "Prompt=", 7) == 0) {
			if (encoded_prompt)
				free(encoded_prompt);
			encoded_prompt = straft(line, '=');
		}

		else if (xargs.max_path == UNSET && *line == 'M'
		&& strncmp(line, "MaxPath=", 8) == 0) {
			int opt_num = 0;
			sscanf(line, "MaxPath=%d\n", &opt_num);
			if (opt_num <= 0)
				continue;
			max_path = opt_num;
		}

		else if (*line == 'D' && strncmp(line, "DividingLineChar=", 17) == 0) {
			/* Accepts both chars and decimal integers */
			char opt_c = -1;
			sscanf(line, "DividingLineChar='%c'", &opt_c);
			if (opt_c == -1) {
				int num = -1;
				sscanf(line, "DividingLineChar=%d", &num);
				if (num == -1)
					div_line_char = DEF_DIV_LINE_CHAR;
				else
					div_line_char = (char)num;
			}
			else
				div_line_char = opt_c;
		}

		else if (*line == 'M' && strncmp(line, "MaxHistory=", 11) == 0) {
			int opt_num = 0;
			sscanf(line, "MaxHistory=%d\n", &opt_num);
			if (opt_num <= 0)
				continue;
			max_hist = opt_num;
		}

		else if (*line == 'M' && strncmp(line, "MaxLog=", 7) == 0) {
			int opt_num = 0;
			sscanf (line, "MaxLog=%d\n", &opt_num);
			if (opt_num <= 0)
				continue;
			max_log = opt_num;
		}

		else if (xargs.path == UNSET && cur_ws == UNSET && *line == 'S'
		&& strncmp(line, "StartingPath=", 13) == 0) {
			char *opt_str = straft(line, '=');
			if (!opt_str)
				continue;

			char *tmp = remove_quotes(opt_str);
			if (!tmp) {
				free(opt_str);
				continue;
			}

			/* If starting path is not NULL, and exists,
			 * and is a directory, and the user has
			 * appropriate permissions, set path to starting
			 * path. If any of these conditions is false,
			 * path will be set to default, that is, CWD */
			if (xchdir(tmp, SET_TITLE) == 0) {
				free(ws[cur_ws].path);
				ws[cur_ws].path = savestring(tmp, strlen(tmp));
			}
			else {
				_err('w', PRINT_PROMPT, _("%s: '%s': %s. "
					 "Using the current working directory "
					 "as starting path\n"), PROGRAM_NAME,
					 tmp, strerror(errno));
			}
			free(opt_str);
		}
	}

	fclose(config_fp);

	if (filter) {
		ret = regcomp(&regex_exp, filter, REG_NOSUB|REG_EXTENDED);

		if (ret != EXIT_SUCCESS) {
			_err('w', PRINT_PROMPT, _("%s: '%s': Invalid regular "
				 "expression\n"), PROGRAM_NAME, filter);
			free(filter);
			filter = (char *)NULL;
			regfree(&regex_exp);
		}
	}

	return;
}

static void
init_config(void)
/* Set up CliFM directories and config files. Load the user's
 * configuration from clifmrc */
{
	if (xargs.stealth_mode == 1) {

		_err(0, PRINT_PROMPT, _("%s: Running in stealth mode: trash, "
			 "persistent selection and directory history, just as "
			 "bookmarks, logs and configuration files, are "
			 "disabled.\n"), PROGRAM_NAME);

		config_ok = 0;

		return;
	}

	/* Store a pointer to the current LS_COLORS value to be used by
	 * external commands */
	ls_colors_bk = getenv("LS_COLORS");

	if (!home_ok)
		return;

	define_config_file_names();

	create_config_files();

	if (config_ok)
		read_config();

	/* "XTerm*eightBitInput: false" must be set in HOME/.Xresources
	 * to make some keybindings like Alt+letter work correctly in
	 * xterm-like terminal emulators */
	/* However, there is no need to do this if using the linux console,
	 * since we are not in a graphical environment */
	if (xargs.stealth_mode != 1 && (flags & GUI)
	&& strncmp(getenv("TERM"), "xterm", 5) == 0)
		edit_xresources();
}

static int
reload_config(void)
{
	/* Free everything */
	free(CONFIG_DIR_GRAL);
	free(CONFIG_DIR);
	free(TRASH_DIR);
	free(TRASH_FILES_DIR);
	free(TRASH_INFO_DIR);
	CONFIG_DIR = TRASH_DIR = TRASH_FILES_DIR = (char *)NULL;
	TRASH_INFO_DIR = (char *)NULL;

	free(BM_FILE);
	free(LOG_FILE);
	free(HIST_FILE);
	free(DIRHIST_FILE);
	BM_FILE = LOG_FILE = HIST_FILE = DIRHIST_FILE = (char *)NULL;

	free(CONFIG_FILE);
	free(PROFILE_FILE);
	free(MSG_LOG_FILE);
	CONFIG_FILE = PROFILE_FILE = MSG_LOG_FILE = (char *)NULL;

	free(MIME_FILE);
	free(PLUGINS_DIR);
	free(ACTIONS_FILE);
	free(KBINDS_FILE);
	MIME_FILE = PLUGINS_DIR = ACTIONS_FILE = KBINDS_FILE = (char *)NULL;

	free(COLORS_DIR);
	free(TMP_DIR);
	free(SEL_FILE);
	TMP_DIR = COLORS_DIR = SEL_FILE = (char *)NULL;

	if (filter) {
		regfree(&regex_exp);
		free(filter);
		filter = (char *)NULL;
	}

	if (opener) {
		free(opener);
		opener = (char *)NULL;
	}

	if (encoded_prompt) {
		free(encoded_prompt);
		encoded_prompt = (char *)NULL;
	}

	if (term) {
		free(term);
		term = (char *)NULL;
	}

	if (sys_shell) {
		free(sys_shell);
		sys_shell = (char *)NULL;
	}

	/* Reset all variables */

	splash_screen = welcome_message = ext_cmd_ok = pager = UNSET;
	show_hidden = clear_screen = list_folders_first = long_view = UNSET;
	unicode = case_sensitive = cd_lists_on_the_fly = share_selbox = UNSET;
	autocd = auto_open = restore_last_path = dirhist_map = UNSET;
	disk_usage = tips = logs_enabled = sort = files_counter = UNSET;
	light_mode = classify = cd_on_quit = columned = tr_as_rm = UNSET;
	no_eln = min_name_trim = case_sens_dirjump = case_sens_path_comp = UNSET;
	min_jump_rank = max_jump_total_rank = UNSET;

	shell_terminal = no_log = internal_cmd = recur_perm_error_flag = 0;
	is_sel = sel_is_last = print_msg = kbind_busy = dequoted = 0;
	mime_match = sort_switch = sort_reverse = kbind_busy = 0;
	shell_terminal = no_log = internal_cmd = dequoted = 0;
	shell_is_interactive = recur_perm_error_flag = mime_match = 0;
	recur_perm_error_flag = is_sel = sel_is_last = print_msg = 0;

	pmsg = nomsg;

	home_ok = config_ok = trash_ok = selfile_ok = 1;

	/* Set up config files and options */
	init_config();

	/* If some option was not set, set it to the default value*/
	check_options();

	set_sel_file();

	create_tmp_files();

	set_colors(usr_cscheme ? usr_cscheme : "default", 1);

	free(usr_cscheme);
	usr_cscheme = (char *)NULL;

	/* If some option was set via command line, keep that value
	 * for any profile */
	if (xargs.case_sens_dirjump != UNSET)
		case_sens_dirjump = xargs.case_sens_dirjump;
	if (xargs.case_sens_path_comp != UNSET)
		case_sens_path_comp = xargs.case_sens_path_comp;
	if (xargs.noeln != UNSET)
		no_eln = xargs.noeln;
	if (xargs.trasrm != UNSET)
		tr_as_rm = xargs.trasrm;
	if (xargs.no_colors != UNSET)
		colorize = xargs.no_colors;
	if (xargs.no_columns != UNSET)
		columned = xargs.no_columns;
	if (xargs.cd_on_quit != UNSET)
		cd_on_quit = xargs.cd_on_quit;
	if (xargs.ext != UNSET)
		ext_cmd_ok = xargs.ext;
	if (xargs.splash != UNSET)
		splash_screen = xargs.splash;
	if (xargs.light != UNSET)
		light_mode = xargs.light;
	if (xargs.sort != UNSET)
		sort = xargs.sort;
	if (xargs.hidden != UNSET)
		show_hidden = xargs.hidden;
	if (xargs.longview != UNSET)
		long_view = xargs.longview;
	if (xargs.ffirst != UNSET)
		list_folders_first = xargs.ffirst;
	if (xargs.cd_list_auto != UNSET)
		cd_lists_on_the_fly = xargs.cd_list_auto;
	if (xargs.sensitive != UNSET)
		case_sensitive = xargs.sensitive;
	if (xargs.unicode != UNSET)
		unicode = xargs.unicode;
	if (xargs.pager != UNSET)
		pager = xargs.pager;
	if (xargs.dirmap != UNSET)
		dirhist_map = xargs.dirmap;
	if (xargs.autocd != UNSET)
		autocd = xargs.autocd;
	if (xargs.auto_open != UNSET)
		auto_open = xargs.auto_open;
	if (xargs.restore_last_path != UNSET)
		restore_last_path = xargs.restore_last_path;
	if (xargs.tips != UNSET)
		tips = xargs.tips;
	if (xargs.disk_usage != UNSET)
		disk_usage = xargs.disk_usage;
	if (xargs.classify != UNSET)
		classify = xargs.classify;
	if (xargs.share_selbox != UNSET)
		share_selbox = xargs.share_selbox;
	if (xargs.max_dirhist != UNSET)
		max_dirhist = xargs.max_dirhist;
	if (xargs.sort_reverse != UNSET)
		sort_reverse = xargs.sort_reverse;
	if (xargs.files_counter != UNSET)
		files_counter = xargs.files_counter;
	if (xargs.welcome_message != UNSET)
		welcome_message = xargs.welcome_message;
	if (xargs.clear_screen != UNSET)
		clear_screen = xargs.clear_screen;
	if (xargs.logs != UNSET)
		logs_enabled = xargs.logs;
	if (xargs.max_path != UNSET)
		max_path = xargs.max_path;
	if (xargs.expand_bookmarks != UNSET)
		expand_bookmarks = xargs.expand_bookmarks;
	if (xargs.only_dirs != UNSET)
		only_dirs = xargs.only_dirs;
	if (xargs.tips != UNSET)
		tips = xargs.tips;
	if (xargs.icons != UNSET)
		icons = xargs.icons;

	/* Free the aliases and prompt_cmds arrays to be allocated again */
	int i = dirhist_total_index;

	while (--i >= 0)
		free(old_pwd[i]);

	free(old_pwd);

	old_pwd = (char **)NULL;

	if (jump_db) {
		for (i = 0; jump_db[i].path; i++)
			free(jump_db[i].path);

		free(jump_db);
		jump_db = (struct jump_t *)NULL;
	}

	jump_n = 0;

	i = (int)aliases_n;
	while (--i >= 0)
		free(aliases[i]);

	i = (int)prompt_cmds_n;
	while (--i >= 0)
		free(prompt_cmds[i]);

	aliases_n = prompt_cmds_n = dirhist_total_index = 0;

	get_aliases();

	get_prompt_cmds();

	load_dirhist();

	load_jumpdb();

	/* Set the current poistion of the dirhist index to the last
	 * entry */
	dirhist_cur_index = dirhist_total_index - 1;

	set_env();

	return EXIT_SUCCESS;
}

static int
is_quote_char(const char c)
/* Simply check a single chartacter (c) against the quoting characters
 * list defined in the qc global array (which takes its values from
 * rl_filename_quote_characters */
{
	if (c == '\0' || !qc)
		return -1;

	char *p = qc;

	while (*p) {
		if (c == *(p++))
			return 1;
	}

	return 0;
}

static char *
dequote_str(char *text, int mt)
/* This function simply deescapes whatever escaped chars it founds in
 * TEXT, so that readline can compare it to system filenames when
 * completing paths. Returns a string containing text without escape
 * sequences */
{
	if (!text || !*text)
		return (char *)NULL;

	/* At most, we need as many bytes as text (in case no escape sequence
	 * is found)*/
	char *buf = (char *)NULL;
	buf = (char *)xnmalloc(strlen(text) + 1, sizeof(char));
	size_t len = 0;

	while(*text) {

		switch (*text) {

			case '\\': buf[len++] = *(++text); break;

			default: buf[len++] = *text; break;
		}

		text++;
	}

	buf[len] = '\0';

	return buf;
}

static void
colors_list(const char *ent, const int i, const int pad, const int new_line)
/* Print ENTRY using color codes and I as ELN, right padding PAD
 * chars and terminating ENTRY with or without a new line char (NEW_LINE
 * 1 or 0 respectivelly) */
{
	size_t i_digits = (size_t)DIGINUM(i);

							/* Num (i) + space + null byte */
	char *index = (char *)xnmalloc(i_digits + 2, sizeof(char));

	if (i > 0) /* When listing files in CWD */
		sprintf(index, "%d ", i);

	else if (i == -1) /* ELN for entry could not be found */
		sprintf(index, "? ");

	/* When listing files NOT in CWD (called from search function and
	 * first argument is a path: "/search_str /path") 'i' is zero. In
	 * this case, no index should be printed at all */
	else
		index[0] = '\0';

	struct stat file_attrib;

	if (lstat(ent, &file_attrib) == -1) {
		fprintf(stderr, "%s%s%s%s%-*s%s%s", el_c, index, df_c,
				uf_c, pad, ent, df_c, new_line ? "\n" : "");
		free(index);
		return;
	}

	char *linkname = (char *)NULL;
	char ext_color[MAX_COLOR] = "";
	char *color = fi_c;

#ifdef _LINUX_CAP
	cap_t cap;
#endif

	switch (file_attrib.st_mode & S_IFMT) {

	case S_IFREG:

		if (access(ent, R_OK) == -1) color = nf_c;

		else if (file_attrib.st_mode & S_ISUID) /* set uid file */
			color = su_c;

		else if (file_attrib.st_mode & S_ISGID) /* set gid file */
			color = sg_c;

		else {
#ifdef _LINUX_CAP
			cap = cap_get_file(ent);

			if (cap) {
				color = ca_c;
				cap_free(cap);
			}

			else if (file_attrib.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)) {
#else

			if (file_attrib.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)) {
#endif
				if (file_attrib.st_size == 0)
					color = ee_c;
				else
					color = ex_c;
			}

			else if (file_attrib.st_size == 0)
				color = ef_c;

			else if (file_attrib.st_nlink > 1)
				color = mh_c;

			else {
				char *ext = (strrchr(ent, '.'));

				if (ext) {
					char *extcolor = get_ext_color(ext);

					if (extcolor) {
						snprintf(ext_color, MAX_COLOR, "\x1b[%sm",
								 extcolor);
						color = ext_color;
						extcolor = (char *)NULL;
					}

					ext = (char *)NULL;
				}
			}
		}

		break;

	case S_IFDIR:

		if (access(ent, R_OK|X_OK) != 0)
			color = nd_c;

		else {
			int is_oth_w = 0;

			if (file_attrib.st_mode & S_IWOTH)
				is_oth_w = 1;

			int files_dir = count_dir(ent);

			color = (file_attrib.st_mode & S_ISVTX) ? (is_oth_w
					? tw_c : st_c) : (is_oth_w ? ow_c :
					/* If folder is empty, it contains only "."
					 * and ".." (2 elements). If not mounted (ex:
					 * /media/usb) the result will be zero. */
					(files_dir == 2 || files_dir == 0) ? ed_c
					: di_c);
		}

		break;

	case S_IFLNK:
		linkname = realpath(ent, (char *)NULL);

		if (linkname) {
			color = ln_c;
			free(linkname);
		}

		else color = or_c;

		break;

	case S_IFIFO: color = pi_c; break;
	case S_IFBLK: color = bd_c; break;
	case S_IFCHR: color = cd_c; break;
	case S_IFSOCK: color = so_c; break;

	/* In case all the above conditions are false... */
	default: color = no_c; break;
	}

	printf("%s%s%s%s%s%s%s%-*s", el_c, index, df_c, color,
		   ent, df_c, new_line ? "\n" : "", pad, "");

	free(index);
}

static int
edit_link(char *link)
/* Relink symlink to new path */
{
	if (!link || !*link)
		return EXIT_FAILURE;

	/* Dequote the filename, if necessary */
	if (strchr(link, '\\')) {
		char *tmp = dequote_str(link, 0);

		if (!tmp) {
			fprintf(stderr, _("%s: %s: Error dequoting file\n"),
					PROGRAM_NAME, link);
			return EXIT_FAILURE;
		}

		strcpy(link, tmp);
		free(tmp);
	}

	/* Check we have a valid symbolic link */
	struct stat file_attrib;

	if (lstat(link, &file_attrib) == -1) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, link,
				strerror(errno));
		return EXIT_FAILURE;
	}

	if ((file_attrib.st_mode & S_IFMT) != S_IFLNK) {
		fprintf(stderr, _("%s: %s: Not a symbolic link\n"),
				PROGRAM_NAME, link);
		return EXIT_FAILURE;
	}

	/* Get file pointed to by symlink and report to the user */
	char *real_path = realpath(link, NULL);

	if (!real_path)
		printf(_("%s%s%s currently pointing to nowhere (broken link)\n"),
			   or_c, link, df_c);
	else {
		printf(_("%s%s%s currently pointing to "), ln_c, link, df_c);
		colors_list(real_path, NO_ELN, NO_PAD, PRINT_NEWLINE);
		free(real_path);
		real_path = (char *)NULL;
	}

	char *new_path = (char *)NULL;

	/* Enable autocd and auto-open (in case they are not already
	 * enabled) to allow TAB completion for ELN's */
	int autocd_status = autocd, auto_open_status = auto_open;
	autocd = auto_open = 1;

	while (!new_path) {
		new_path = rl_no_hist(_("New path ('q' to quit): "));

		if (!new_path)
			continue;

		if (!*new_path) {
			free(new_path);
			new_path = (char *)NULL;
			continue;
		}

		if (*new_path == 'q' && !new_path[1]) {
			free(new_path);
			return EXIT_SUCCESS;
		}
	}

	/* Set autocd and auto-open to their original values */
	autocd = autocd_status;
	auto_open = auto_open_status;

	/* If an ELN, replace by the corresponding filename */
	if (is_number(new_path)) {
		int i_new_path = atoi(new_path) - 1;
		if (file_info[i_new_path].name) {
			new_path = (char *)xrealloc(new_path,
								(strlen(file_info[i_new_path].name)
								+ 1) * sizeof(char));
			strcpy(new_path, file_info[i_new_path].name);
		}
	}

	/* Remove terminating space. TAB completion puts a final space
	 * after file names */
	size_t path_len = strlen(new_path);
	if (new_path[path_len - 1] == ' ')
		new_path[path_len - 1] = '\0';

	/* Dequote new path, if needed */
	if(strchr(new_path, '\\')) {
		char *tmp = dequote_str(new_path, 0);

		if (!tmp) {
			fprintf(stderr, _("%s: %s: Error dequoting file\n"),
					PROGRAM_NAME, new_path);
			free(new_path);
			return EXIT_FAILURE;
		}

		strcpy(new_path, tmp);
		free(tmp);
	}

	/* Check new_path existence and warn the user if it does not
	 * exist */
	if (lstat(new_path, &file_attrib) == -1) {

		printf("'%s': %s\n", new_path, strerror(errno));
		char *answer = (char *)NULL;
		while (!answer) {
			answer= rl_no_hist(_("Relink as a broken symbolic link? "
								"[y/n] "));

			if (!answer)
				continue;

			if (!*answer) {
				free(answer);
				answer = (char *)NULL;
				continue;
			}

			if (*answer != 'y' && *answer != 'n') {
				free(answer);
				answer = (char *)NULL;
				continue;
			}

			if (answer[1]) {
				free(answer);
				answer = (char *)NULL;
				continue;
			}

			if (*answer == 'y') {
				free(answer);
				break;
			}

			else {
				free(answer);
				free(new_path);
				return EXIT_SUCCESS;
			}
		}
	}

	/* Finally, relink the symlink to new_path */
	char *cmd[] = { "ln", "-sfn", new_path, link, NULL };
	if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS) {
		free(new_path);
		return EXIT_FAILURE;
	}

	real_path = realpath(link, NULL);

	printf(_("%s%s%s successfully relinked to "), real_path ? ln_c
		   : or_c, link, df_c);
	colors_list(new_path, NO_ELN, NO_PAD, PRINT_NEWLINE);

	free(new_path);

	if (real_path)
		free(real_path);

	return EXIT_SUCCESS;
}

static int
load_actions(void)
/* Store actions from the actions file into a struct */
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
	FILE *actions_fp = fopen(ACTIONS_FILE, "r");

	if (!actions_fp)
		return EXIT_FAILURE;

	size_t line_size = 0;
	char *line = (char *)NULL;
	ssize_t line_len = 0;

	while ((line_len = getline(&line, &line_size, actions_fp)) > 0) {

		if (!line || !*line || *line == '#' || *line == '\n')
			continue;

		if (line[line_len - 1] == '\n')
			line[line_len - 1] = '\0';

		char *tmp = (char *)NULL;
		tmp= strrchr(line, '=');

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

	return EXIT_SUCCESS;
}

static size_t
get_path_env(void)
/* Store all paths in the PATH environment variable into a globally
 * declared array (paths) */
{
	size_t i = 0;

	/* Get the value of the PATH env variable */
	char *path_tmp = (char *)NULL;

#if __linux__
	for (i = 0; __environ[i]; i++) {
		if (strncmp(__environ[i], "PATH", 4) == 0) {
			path_tmp = straft(__environ[i], '=');
			break;
		}
	}

#else
	path_tmp = savestring(getenv("PATH"), strlen(getenv("PATH")));
#endif

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

static int
create_iso(char *in_file, char *out_file)
{
	int exit_status = EXIT_SUCCESS;
	struct stat file_attrib;

	if (lstat(in_file, &file_attrib) == -1) {
		fprintf(stderr, "archiver: %s: %s\n", in_file,
				strerror(errno));
		return EXIT_FAILURE;
	}

	/* If IN_FILE is a directory */
	if ((file_attrib.st_mode & S_IFMT) == S_IFDIR) {
		char *cmd[] = { "mkisofs", "-R", "-o", out_file, in_file, NULL };

		if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;
	}

	/* If IN_FILE is a block device */
	else if ((file_attrib.st_mode & S_IFMT) == S_IFBLK) {

		char *if_option = (char *)xnmalloc(strlen(in_file) + 4, sizeof(char));
		sprintf(if_option, "if=%s", in_file);

		char *of_option = (char *)xnmalloc(strlen(out_file) + 4, sizeof(char));
		sprintf(of_option, "of=%s", out_file);

		char *cmd[] = { "sudo", "dd", if_option, of_option, "bs=64k",
						"conv=noerror,sync", "status=progress", NULL };

		if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;

		free(if_option);
		free(of_option);
	}

	else {
		fprintf(stderr, "archiver: %s: Invalid file format\nFile "
				"should be either a directory or a block device\n", in_file);
		return EXIT_FAILURE;
	}

	return exit_status;
}

static void
add_to_dirhist(const char *dir_path)
/* Add DIR_PATH to visited directory history (old_pwd) */
{
/*  static size_t end_counter = 11, mid_counter = 11; */

	/* If already at the end of dirhist, add new entry */
	if (dirhist_cur_index + 1 >= dirhist_total_index) {

		/* Do not add anything if new path equals last entry in
		 * directory history */
		if ((dirhist_total_index - 1) >= 0
		&& old_pwd[dirhist_total_index - 1]
		&& *(dir_path + 1) == *(old_pwd[dirhist_total_index - 1] + 1)
		&& strcmp(dir_path, old_pwd[dirhist_total_index - 1]) == 0)
			return;

		/* Realloc only once per 10 operations */
/*      if (end_counter > 10) {
			end_counter = 1; */
			/* 20: Realloc dirhist_total + (2 * 10) */
			old_pwd = (char **)xrealloc(old_pwd,
									(size_t)(dirhist_total_index + 2)
									* sizeof(char *));
/*      }

		end_counter++; */

		dirhist_cur_index = dirhist_total_index;
		old_pwd[dirhist_total_index++] = savestring(dir_path, strlen(dir_path));
		old_pwd[dirhist_total_index] = (char *)NULL;
	}

	/* I not at the end of dirhist, add previous AND new entry */
	else {
/*      if (mid_counter > 10) {
			mid_counter = 1; */
			/* 30: Realloc dirhist_total + (3 * 10) */
			old_pwd = (char **)xrealloc(old_pwd,
							(size_t)(dirhist_total_index + 3) * sizeof(char *));
/*      }

		mid_counter++; */

		old_pwd[dirhist_total_index++] = savestring(
								old_pwd[dirhist_cur_index],
								strlen(old_pwd[dirhist_cur_index]));

		dirhist_cur_index = dirhist_total_index;
		old_pwd[dirhist_total_index++] = savestring(dir_path, strlen(dir_path));

		old_pwd[dirhist_total_index] = (char *)NULL;
	}
}

static int
handle_iso(char *file)
{
	int exit_status = EXIT_SUCCESS;

	/* Use 7z to
	 * list (l)
	 * extract (e)
	 * extrat to dir (x -oDIR FILE)
	 * test (t) */

	printf(_("%s[e]%sxtract %s[E]%sxtract-to-dir %s[l]%sist "
		  "%s[t]%stest %s[m]%sount %s[q]%suit\n"), bold, df_c, bold,
		  df_c, bold, df_c, bold, df_c, bold, df_c, bold, df_c);

	char *operation = (char *)NULL;
	char sel_op = 0;

	while (!operation) {
		operation = rl_no_hist(_("Operation: "));

		if (!operation)
			continue;

		if (operation && (!operation[0] || operation[1] != '\0')) {
			free(operation);
			operation = (char *)NULL;
			continue;
		}

		switch(*operation) {
			case 'e': /* fallthrough */
			case 'E': /* fallthrough */
			case 'l': /* fallthrough */
			case 'm': /* fallthrough */
			case 't':
				sel_op = *operation;
				free(operation);
			break;

			case 'q':
				free(operation);
				return EXIT_SUCCESS;

			default:
				free(operation);
				operation = (char *)NULL;
			break;
		}

		if (sel_op)
			break;
	}

	char *ret = strchr(file, '\\');
	if (ret) {
		char *deq_file = dequote_str(file, 0);
		if (deq_file) {
			strcpy(file, deq_file);
			free(deq_file);
		}
		ret = (char *)NULL;
	}

	switch(sel_op) {

		/* ########## EXTRACT #######*/
		case 'e': {
			/* 7z x -oDIR FILE (use FILE as DIR) */
			char *o_option = (char *)xnmalloc(strlen(file) + 7, sizeof(char));
			sprintf(o_option, "-o%s.dir", file);

			/* Construct and execute cmd */
			char *cmd[] = { "7z", "x", o_option, file, NULL };
			if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;

			free(o_option);
		}
		break;

		/* ########## EXTRACT TO DIR ####### */
		case 'E': {
			/* 7z x -oDIR FILE (ask for DIR) */
			char *ext_path = (char *)NULL;

			while (!ext_path) {
				ext_path = rl_no_hist(_("Extraction path: "));

				if (!ext_path)
					continue;

				if (ext_path && !*ext_path) {
					free(ext_path);
					ext_path = (char *)NULL;
					continue;
				}
			}

			char *o_option = (char *)xnmalloc(strlen(ext_path) + 3,
										sizeof(char));
			sprintf(o_option, "-o%s", ext_path);

			/* Construct and execute cmd */
			char *cmd[] = { "7z", "x", o_option, file, NULL };

			if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;

			free(ext_path);
			free(o_option);
			ext_path = (char *)NULL;
		}
		break;

		/* ########## LIST ####### */
		case 'l': {
			/* 7z l FILE */
			char *cmd[] = { "7z", "l", file, NULL };

			if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;
			}
		break;

		/* ########## MOUNT ####### */

		case 'm': {
			/* Create mountpoint */
			char *mountpoint = (char *)NULL;

			if (xargs.stealth_mode == 1) {
				mountpoint = (char *)xnmalloc(strlen(file) + 19, sizeof(char));

				sprintf(mountpoint, "/tmp/clifm-mounts/%s", file);
			}

			else {
				mountpoint = (char *)xnmalloc(strlen(CONFIG_DIR)
										  + strlen(file) + 9, sizeof(char));

				sprintf(mountpoint, "%s/mounts/%s", CONFIG_DIR, file);
			}

			char *dir_cmd[] = { "mkdir", "-pm700", mountpoint, NULL };

			if (launch_execve(dir_cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS) {
				free(mountpoint);
				return EXIT_FAILURE;
			}

			/* Construct and execute cmd */
			char *cmd[] = { "sudo", "mount", "-o", "loop", file,
							mountpoint, NULL };

			if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS) {
				free(mountpoint);
				return EXIT_FAILURE;
			}

			/* List content of mountpoint */
			if (xchdir(mountpoint, SET_TITLE) == -1) {
				fprintf(stderr, "archiver: %s: %s\n", mountpoint,
						strerror(errno));
				free(mountpoint);
				return EXIT_FAILURE;
			}

			free(ws[cur_ws].path);
			ws[cur_ws].path = savestring(mountpoint, strlen(mountpoint));
			add_to_jumpdb(ws[cur_ws].path);

			if (cd_lists_on_the_fly) {
				free_dirlist();
				if (list_dir() != EXIT_SUCCESS)
					exit_status = EXIT_FAILURE;
				add_to_dirhist(ws[cur_ws].path);
			}
			else
				printf("%s: Successfully mounted on %s\n", file,
					   mountpoint);

			free(mountpoint);
		}
		break;

		/* ########## TEST #######*/
		case 't': {
			/* 7z t FILE */
			char *cmd[] = { "7z", "t", file, NULL };

			if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;
			}
		break;
	}

	return exit_status;
}

static int
check_iso(char *file)
/* Run the 'file' command on FILE and look for "ISO 9660" and
 * string in its output. Returns zero if found, one if not, and -1
 * in case of error */
{
	if (!file || !*file) {
		fputs(_("Error opening temporary file\n"), stderr);
		return -1;
	}

	char ISO_TMP_FILE[PATH_MAX] = "";
	char *rand_ext = gen_rand_str(6);

	if (!rand_ext)
		return -1;

	if (xargs.stealth_mode == 1)
		sprintf(ISO_TMP_FILE, "/tmp/clifm-archiver.%s", rand_ext);
	else
		sprintf(ISO_TMP_FILE, "%s/archiver.%s", TMP_DIR, rand_ext);

	free(rand_ext);

	FILE *file_fp = fopen(ISO_TMP_FILE, "w");

	if (!file_fp) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME,
				ISO_TMP_FILE, strerror(errno));
		return -1;
	}

	FILE *file_fp_err = fopen("/dev/null", "w");

	if (!file_fp_err) {
		fprintf(stderr, "%s: /dev/null: %s\n", PROGRAM_NAME,
				strerror(errno));
		fclose(file_fp);
		return -1;
	}

	int stdout_bk = dup(STDOUT_FILENO); /* Store original stdout */
	int stderr_bk = dup(STDERR_FILENO); /* Store original stderr */

	/* Redirect stdout to the desired file */
	if (dup2(fileno(file_fp), STDOUT_FILENO) == -1 ) {
		fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
		fclose(file_fp);
		fclose(file_fp_err);
		return -1;
	}

	/* Redirect stderr to /dev/null */
	if (dup2(fileno(file_fp_err), STDERR_FILENO) == -1) {
		fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
		fclose(file_fp);
		fclose(file_fp_err);
		return -1;
	}

	fclose(file_fp);
	fclose(file_fp_err);

	char *cmd[] = { "file", "-b", file, NULL };
	int retval = launch_execve(cmd, FOREGROUND, E_NOFLAG);

	dup2(stdout_bk, STDOUT_FILENO); /* Restore original stdout */
	dup2(stderr_bk, STDERR_FILENO); /* Restore original stderr */
	close(stdout_bk);
	close(stderr_bk);

	if (retval != EXIT_SUCCESS)
		return -1;

	int is_iso = 0;

	if (access(ISO_TMP_FILE, F_OK) == 0) {

		file_fp = fopen(ISO_TMP_FILE, "r");

		if (file_fp) {
			char line[255] = "";
			fgets(line, (int)sizeof(line), file_fp);
			char *ret = strstr(line, "ISO 9660");

			if (ret)
				is_iso = 1;

			fclose(file_fp);
		}
		unlink(ISO_TMP_FILE);
	}

	if (is_iso)
		return EXIT_SUCCESS;

	return EXIT_FAILURE;
}

static void
print_tips(int all)
/* Print either all tips (if ALL == 1) or just a random one (ALL == 0) */
{
	const char *TIPS[] = {
		"Try the autocd and auto-open functions: run 'FILE' instead "
		"of 'open FILE' or 'cd FILE'",
		"Add a new entry to the mimelist file with 'mm edit' or F6",
		"Do not forget to take a look at the manpage",
		"Need more speed? Try the light mode (Alt-y)",
		"The Selection Box is shared among different instances of CliFM",
		"Select files here and there with the 's' command",
		"Use wildcards and regular expressions with the 's' command: "
		"'s *.c' or 's .*\\.c$'",
		"ELN's and the 'sel' keyword work for shell commands as well: "
		"'file 1 sel'",
		"Press TAB to automatically expand an ELN: 'o 2' -> TAB -> "
		"'o FILENAME'",
		"Easily copy everything in CWD into another directory: 's * "
		"&& c sel ELN/DIR'",
		"Use ranges (ELN-ELN) to easily move multiple files: 'm 3-12 "
		"ELN/DIR'",
		"Trash files with a simple 't ELN'",
		"Get mime information for a file: 'mm info ELN'",
		"If too many files are listed, try enabling the pager ('pg on')",
		"Once in the pager, go backwards pressing the keyboard shortcut "
		"provided by your terminal emulator",
		"Once in the pager, press 'q' to stop it",
		"Press 'Alt-l' to switch to long view mode",
		"Search for files using the slash command: '/*.png'",
		"The search function allows regular expressions: '/^c'",
		"Add a new bookmark by just entering 'bm ELN/FILE'",
		"Use c, l, m, md, and r instead of cp, ln, mv, mkdir, and rm",
		"Access a remote file system using the 'net' command",
		"Manage default associated applications with the 'mime' command",
		"Go back and forth in the directory history with 'Alt-j' and 'Alt-k' "
		"or Shift-Left and Shift-Right",
		"Open a new instance of CliFM with the 'x' command: 'x ELN/DIR'",
		"Send a command directly to the system shell with ';CMD'",
		"Run the last executed command by just running '!!'",
		"Import aliases from file using 'alias import FILE'",
		"List available aliases by running 'alias'",
		"Create aliases to easily run your preferred commands",
		"Open and edit the configuration file with 'edit'",
		"Find a description for each CLiFM command by running 'cmd'",
		"Print the currently used color codes list by entering 'cc'",
		"Press 'Alt-i' to toggle hidden files on/off",
		"List mountpoints by pressing 'Alt-m'",
		"Allow the use of shell commands with the -x option: 'clifm -x'",
		"Go to the root directory by just pressing 'Alt-r'",
		"Go to the home directory by just pressing 'Alt-e'",
		"Press 'F8' to open and edit current color scheme",
		"Press 'F9' to open and edit the keybindings file",
		"Press 'F10' to open and edit the configuration file",
		"Press 'F11' to open and edit the bookmarks file",
		"Customize the starting path using the -p option: 'clifm -p PATH'",
		"Use the 'o' command to open files and directories: 'o 12'",
		"Bypass the resource opener specifying an application: 'o 12 "
		"leafpad'",
		"Open a file and send it to the background running 'o 24 &'",
		"Create a custom prompt editing the configuration file",
		"Customize color codes using the configuration file",
		"Open the bookmarks manager by just pressing 'Alt-b'",
		"Chain commands using ; and &&: 's 2 7-10; r sel'",
		"Add emojis to the prompt by copying them to the Prompt line "
		"in the configuration file",
		"Create a new profile running 'pf add PROFILE' or 'clifm -P "
		"PROFILE'",
		"Switch profiles using 'pf set PROFILE'",
		"Delete a profile using 'pf del PROFILE'",
		"Copy selected files into CWD by just running 'v sel' or "
		"pressing Ctrl-Alt-v",
		"Use 'p ELN' to print file properties for ELN",
		"Deselect all selected files by pressing 'Alt-d'",
		"Select all files in CWD by pressing 'Alt-a'",
		"Jump to the Selection Box by pressing 'Alt-s'",
		"Restore trashed files using the 'u' command",
		"Empty the trash bin running 't clear'",
		"Press Alt-f to toggle list-folders-first on/off",
		"Use the 'fc' command to disable the files counter",
		"Take a look at the splash screen with the 'splash' command",
		"Have some fun trying the 'bonus' command",
		"Launch the default system shell in CWD using ':' or ';'",
		"Use 'Alt-z' and 'Alt-x' to switch sorting methods",
		"Reverse sorting order using the 'rev' option: 'st rev'",
		"Compress and decompress files using the 'ac' and 'ad' "
		"commands respectivelly",
		"Rename multiple files at once with the bulk rename function: "
		"'br *.txt'",
		"Need no more tips? Disable this feature in the configuration "
		"file",
		"Need root privileges? Launch a new instance of CLifM as root "
		"running the 'X' command",
		"Create custom commands and features using the 'actions' command",
		"Create a fresh configuration file by running 'edit gen'",
		"Use 'ln edit' (or 'le') to edit symbolic links",
		"Change default keyboard shortcuts by editing the keybindings file (F9)",
		"Keep in sight previous and next visited directories enabling the "
		"DirhistMap option in the configuration file",
		"Leave no traces at all running in stealth mode",
		"Pin a file via the 'pin' command and then use it with the "
		"period keyword (,). Ex: 'pin DIR' and then 'cd ,'",
		"Switch between color schemes using the 'cs' command",
		"Use the 'j' command to quickly navigate through visited "
		"directories",
		"Switch workspaces pressing Alt-[1-4]",
		"Use the 'ws' command to list available workspaces",
		"Take a look at available plugins using the 'actions' command",
		"Space is not needed: enter 'p12' instead of 'p 12'",
		"When searching or selecting files, use the exclamation mark "
		"to reverse the meaning of a pattern",
		"Enable the TrashAsRm option to prevent accidental deletions",
		"Don't like ELN's? Disable them using the -e option",
		NULL
	};

	size_t tipsn = (sizeof(TIPS) / sizeof(TIPS[0])) - 1;

	if (all) {
		size_t i;
		for (i = 0; i < tipsn; i++)
			printf("%sTIP %zu%s: %s\n", bold, i, df_c, TIPS[i]);

		return;
	}

	srand((unsigned int)time(NULL));
	printf("%sTIP%s: %s\n", bold, df_c, TIPS[rand() % tipsn]);
}

static int
is_compressed(char *file, int test_iso)
/* Run the 'file' command on FILE and look for "archive" and
 * "compressed" strings in its output. Returns zero if compressed,
 * one if not, and -1 in case of error.
 * test_iso is used to determine if ISO files should be checked as
 * well: this is the case when called from open_function() or
 * mime_open(), since both need to check compressed and ISOs as
 * well (and there is no need to run two functions (is_compressed and
 * check_iso), when we can run just one) */
{
	if (!file || !*file) {
		fputs(_("Error opening temporary file\n"), stderr);
		return -1;
	}

	char *rand_ext = gen_rand_str(6);

	if (!rand_ext)
		return -1;

	char ARCHIVER_TMP_FILE[PATH_MAX];

	if (xargs.stealth_mode == 1)
		sprintf(ARCHIVER_TMP_FILE, "/tmp/clifm-archiver.%s", rand_ext);

	else
		sprintf(ARCHIVER_TMP_FILE, "%s/archiver.%s", TMP_DIR, rand_ext);

	free(rand_ext);

	if (access(ARCHIVER_TMP_FILE, F_OK) == 0)
		unlink(ARCHIVER_TMP_FILE);

	FILE *file_fp = fopen(ARCHIVER_TMP_FILE, "w");

	if (!file_fp) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME,
				ARCHIVER_TMP_FILE, strerror(errno));
		return -1;
	}

	FILE *file_fp_err = fopen("/dev/null", "w");

	if (!file_fp_err) {
		fprintf(stderr, "%s: /dev/null: %s\n", PROGRAM_NAME, strerror(errno));
		fclose(file_fp);
		return -1;
	}

	int stdout_bk = dup(STDOUT_FILENO); /* Store original stdout */
	int stderr_bk = dup(STDERR_FILENO); /* Store original stderr */

	/* Redirect stdout to the desired file */
	if (dup2(fileno(file_fp), STDOUT_FILENO) == -1 ) {
		fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
		fclose(file_fp);
		fclose(file_fp_err);
		return -1;
	}

	/* Redirect stderr to /dev/null */
	if (dup2(fileno(file_fp_err), STDERR_FILENO) == -1) {
		fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
		fclose(file_fp);
		fclose(file_fp_err);
		return -1;
	}

	fclose(file_fp);
	fclose(file_fp_err);

	char *cmd[] = { "file", "-b", file, NULL };
	int retval = launch_execve(cmd, FOREGROUND, E_NOFLAG);

	dup2(stdout_bk, STDOUT_FILENO); /* Restore original stdout */
	dup2(stderr_bk, STDERR_FILENO); /* Restore original stderr */
	close(stdout_bk);
	close(stderr_bk);

	if (retval != EXIT_SUCCESS)
		return -1;

	int compressed = 0;

	if (access(ARCHIVER_TMP_FILE, F_OK) == 0) {

		file_fp = fopen(ARCHIVER_TMP_FILE, "r");

		if (file_fp) {
			char line[255];
			fgets(line, (int)sizeof(line), file_fp);
			char *ret = strstr(line, "archive");

			if (ret)
				compressed = 1;

			else {
				ret = strstr(line, "compressed");

				if (ret)
					compressed = 1;

				else if (test_iso) {
					ret = strstr(line, "ISO 9660");

					if (ret)
						compressed = 1;
				}
			}

			fclose(file_fp);
		}

		unlink(ARCHIVER_TMP_FILE);
	}

	if (compressed)
		return EXIT_SUCCESS;

	return EXIT_FAILURE;
}

static int
zstandard(char *in_file, char *out_file, char mode, char op)
/* If MODE is 'c', compress IN_FILE producing a zstandard compressed
 * file named OUT_FILE. If MODE is 'd', extract, test or get
 * information about IN_FILE. OP is used only for the 'd' mode: it
 * tells if we have one or multiple file. Returns zero on success and
 * one on error */
{
	int exit_status = EXIT_SUCCESS;

	char *deq_file = dequote_str(in_file, 0);

	if (!deq_file) {
		fprintf(stderr, _("archiver: %s: Error dequoting filename\n"),
				in_file);
		return EXIT_FAILURE;
	}

	if (mode == 'c') {

		if (out_file) {
			char *cmd[] = { "zstd", "-zo", out_file, deq_file, NULL };
			if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;
		}

		else {
			char *cmd[] = { "zstd", "-z", deq_file, NULL };

			if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;
		}

		free(deq_file);
		return exit_status;
	}

	/* mode == 'd' */

	/* op is non-zero when multiple files, including at least one
	 * zst file, are passed to the archiver function */
	if (op != 0) {
		char option[3] = "";

		switch(op) {
			case 'e': strcpy(option, "-d"); break;
			case 't': strcpy(option, "-t"); break;
			case 'i': strcpy(option, "-l"); break;
		}

		char *cmd[] = { "zstd", option, deq_file, NULL };

		exit_status = launch_execve(cmd, FOREGROUND, E_NOFLAG);

		free(deq_file);

		if (exit_status != EXIT_SUCCESS)
			return EXIT_FAILURE;

		return EXIT_SUCCESS;
	}


	printf(_("%s[e]%sxtract %s[t]%sest %s[i]%snfo %s[q]%suit\n"),
		   bold, df_c, bold, df_c, bold, df_c, bold, df_c);

	char *operation = (char *)NULL;

	while (!operation) {
		operation = rl_no_hist(_("Operation: "));

		if (!operation)
			continue;

		if (operation && (!operation[0] || operation[1] != '\0')) {
			free(operation);
			operation = (char *)NULL;
			continue;
		}

		switch(*operation) {
			case 'e': {
				char *cmd[] = { "zstd", "-d", deq_file, NULL };
				if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
					exit_status = EXIT_FAILURE;
			}
			break;

			case 't': {
				char *cmd[] = { "zstd", "-t", deq_file, NULL };
				if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
					exit_status = EXIT_FAILURE;
			}
			break;

			case 'i': {
				char *cmd[] = { "zstd", "-l", deq_file, NULL };
				if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
					exit_status = EXIT_FAILURE;
			}
			break;

			case 'q':
				free(operation);
				free(deq_file);
				return EXIT_SUCCESS;

			default:
				free(operation);
				operation = (char *)NULL;
			break;
		}
	}

	free(operation);
	free(deq_file);

	return exit_status;
}

static char *
escape_str(const char *str)
/* Take a string and returns the same string escaped. If nothing to be
 * escaped, the original string is returned */
{
	if (!str)
		return (char *)NULL;

	size_t len = 0;
	char *buf = (char *)NULL;

	buf = (char *)xnmalloc(strlen(str) * 2 + 1, sizeof(char));

	while(*str) {
		if (is_quote_char(*str))
			buf[len++] = '\\';
		buf[len++] = *(str++);
	}

	buf[len] = '\0';

	if (buf)
		return buf;

	return (char *)NULL;
}

static int
archiver(char **args, char mode)
/* Handle archives and/or compressed files (ARGS) according to MODE:
 * 'c' for archiving/compression, and 'd' for dearchiving/decompression
 * (including listing, extracting, repacking, and mounting). Returns
 * zero on success and one on error. Depends on 'zstd' for Zdtandard
 * files 'atool' and 'archivemount' for the remaining types. */
{
	size_t i;
	int exit_status = EXIT_SUCCESS;

	if (!args[1])
		return EXIT_FAILURE;

	if (mode == 'c') {

			/* ##################################
			 * #        1 - COMPRESSION         #
			 * ##################################*/

		/* Get archive name/type */

		puts(_("Use extension to specify archive/compression type.\n"
			   "Defaults to .tar.gz"));
		char *name = (char *)NULL;
		while (!name) {
			name = rl_no_hist(_("Filename ('q' to quit): "));

			if (!name)
				continue;

			if (!*name) {
				free(name);
				name = (char *)NULL;
				continue;
			}

			if (*name == 'q' && name[1] == '\0') {
				free(name);
				return EXIT_SUCCESS;
			}
		}

				/* ##########################
				 * #        ZSTANDARD       #
				 * ########################## */

		char *ret = strrchr(name, '.');
		if (strcmp(ret, ".zst") == 0) {

			/* Multiple files */
			if (args[2]) {

				printf(_("\n%sNOTE%s: Zstandard does not support "
					   "compression of multiple files into one single "
					   "compressed file. Files will be compressed rather "
					   "into multiple compressed files using original "
					   "filenames\n"), bold, df_c);

				for (i = 1; args[i]; i++) {
					if (zstandard(args[i], NULL, 'c', 0)
					!= EXIT_SUCCESS)
						exit_status = EXIT_FAILURE;
				}
			}

			/* Only one file */
			else
				exit_status = zstandard(args[1], name, 'c', 0);

			free(name);

			return exit_status;
		}


				/* ##########################
				 * #        ISO 9660        #
				 * ########################## */

		if (strcmp(ret, ".iso") == 0) {
			exit_status = create_iso(args[1], name);
			free(name);
			return exit_status;
		}


				/* ##########################
				 * #          OTHERS        #
				 * ########################## */

		/* Escape the string, if needed */
		char *esc_name = escape_str(name);
		free(name);

		if (!esc_name) {
			fprintf(stderr, _("archiver: %s: Error escaping "
					"string\n"), name);
			return EXIT_FAILURE;
		}

		/* Construct the command */
		char *cmd = (char *)NULL;
		char *ext_ok = strchr(esc_name, '.');
		size_t cmd_len = strlen(esc_name) + 10 + ((!ext_ok) ? 8 : 0);

		cmd = (char *)xcalloc(cmd_len, sizeof(char));

		/* If name has no extension, add the default */
		sprintf(cmd, "atool -a %s%s", esc_name, (!ext_ok) ? ".tar.gz" : "");

		for (i = 1; args[i]; i++) {
			cmd_len += strlen(args[i]) + 1;
			cmd = (char *)xrealloc(cmd, (cmd_len + 1) * sizeof(char));
			strcat(cmd, " ");
			strcat(cmd, args[i]);
		}

		if (launch_execle(cmd) != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;

		free(cmd);
		free(esc_name);

		return exit_status;
	}


	/* mode == 'd' */

			/* ##################################
			 * #      2 - DECOMPRESSION         #
			 * ##################################*/

	/* Exit if at least one non-compressed file is found */
	for (i = 1; args[i]; i++) {
		char *deq = (char *)NULL;

		if (strchr(args[i], '\\')) {
			deq = dequote_str(args[i], 0);
			strcpy(args[i], deq);
			free(deq);
		}

		if (is_compressed(args[i], 1) != 0) {
			fprintf(stderr, _("archiver: %s: Not an "
					"archive/compressed file\n"), args[i]);
			return EXIT_FAILURE;
		}
	}

				/* ##########################
				 * #        ISO 9660        #
				 * ########################## */

	char *ret = strrchr(args[1], '.');

	if ((ret && strcmp(ret, ".iso") == 0)
	|| check_iso(args[1]) == 0)
		return handle_iso(args[1]);

				/* ##########################
				 * #        ZSTANDARD       #
				 * ########################## */

	/* Check if we have at least one Zstandard file */

	int zst_index = -1;
	size_t files_num = 0;

	for (i = 1; args[i]; i++) {
		files_num++;
		if (args[i][strlen(args[i]) -1] == 't') {
			char *retval = strrchr(args[i], '.');
			if (retval) {
				if (strcmp(retval, ".zst") == 0)
					zst_index = (int)i;
			}
		}
	}

	if (zst_index != -1) {

		/* Multiple files */
		if (files_num > 1) {

			printf(_("%sNOTE%s: Using Zstandard\n"), bold, df_c);
			printf(_("%s[e]%sxtract %s[t]%sest %s[i]%snfo %s[q]%suit\n"),
				   bold, df_c, bold, df_c, bold, df_c, bold, df_c);

			char *operation = (char *)NULL;
			char sel_op = 0;
			while(!operation) {
				operation = rl_no_hist(_("Operation: "));

				if (!operation)
					continue;

				if (operation && (!operation[0]
				|| operation[1] != '\0')) {
					free(operation);
					operation = (char *)NULL;
					continue;
				}

				switch(*operation) {
					case 'e': /* fallthrough */
					case 't': /* fallthrough */
					case 'i':
						sel_op = *operation;
					break;

					case 'q':
						free(operation);
						return EXIT_SUCCESS;

					default:
						free(operation);
						operation = (char *)NULL;
					break;
				}
			}

			for (i = 1; args[i]; i++) {
				if (zstandard(args[i], NULL, 'd', sel_op)
				!= EXIT_SUCCESS)
					exit_status = EXIT_FAILURE;
			}

			free(operation);
			return exit_status;
		}

		/* Just one file */
		else {
			if (zstandard(args[zst_index], NULL, 'd', 0) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;

			return exit_status;
		}
	}

				/* ##########################
				 * #          OTHERS        #
				 * ########################## */

	/* 1) Get operation to be performed
	 * ################################ */

	printf(_("%s[e]%sxtract %s[E]%sxtract-to-dir %s[l]%sist "
		  "%s[m]%sount %s[r]%sepack %s[q]%suit\n"), bold, df_c, bold,
		  df_c, bold, df_c, bold, df_c, bold, df_c, bold, df_c);

	char *operation = (char *)NULL;
	char sel_op = 0;

	while (!operation) {
		operation = rl_no_hist(_("Operation: "));

		if (!operation)
			continue;

		if (operation && (!operation[0] || operation[1] != '\0')) {
			free(operation);
			operation = (char *)NULL;
			continue;
		}

		switch(*operation) {
			case 'e': /* fallthrough */
			case 'E': /* fallthrough */
			case 'l': /* fallthrough */
			case 'm': /* fallthrough */
			case 'r':
				sel_op = *operation;
				free(operation);
			break;

			case 'q':
				free(operation);
				return EXIT_SUCCESS;

			default:
				free(operation);
				operation = (char *)NULL;
			break;
		}

		if (sel_op)
			break;
	}

	/* 2) Prepare files based on operation
	 * #################################### */

	char *dec_files = (char *)NULL;

	switch(sel_op) {
		case 'e':  /* fallthrough */
		case 'r': {

			/* Store all filenames into one single variable */
			size_t len = 1;
			dec_files = (char *)xnmalloc(len, sizeof(char));
			*dec_files = '\0';

			for (i = 1; args[i]; i++) {

				/* Escape the string, if needed */
				char *esc_name = escape_str(args[i]);
				if (!esc_name)
					continue;

				len += strlen(esc_name) + 1;
				dec_files = (char *)xrealloc(dec_files, (len + 1)
											 * sizeof(char));
				strcat(dec_files, " ");
				strcat(dec_files, esc_name);

				free(esc_name);
			}
		}
		break;

		case 'E':
		case 'l':
		case 'm': {

			/* These operation won't be executed via the system shell,
			 * so that we need to deescape files if necessary */
			for (i = 1; args[i]; i++) {

				 if (strchr(args[i], '\\')) {
					char *deq_name = dequote_str(args[i], 0);

					if (!deq_name) {
						fprintf(stderr, _("archiver: %s: Error "
								"dequoting filename\n"), args[i]);
						return EXIT_FAILURE;
					}

					strcpy(args[i], deq_name);
					free(deq_name);
					deq_name = (char *)NULL;
				}
			}
		}
		break;
	}

	/* 3) Construct and run the corresponding commands
	 * ############################################### */

	switch(sel_op) {

			/* ########## EXTRACT ############## */

		case 'e': {
			char *cmd = (char *)NULL;
			cmd = (char *)xnmalloc(strlen(dec_files) + 13, sizeof(char));

			sprintf(cmd, "atool -x -e %s", dec_files);

			if (launch_execle(cmd) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;

			free(cmd);
			free(dec_files);
		}
		break;

		/* ########## EXTRACT TO DIR ############## */

		case 'E':
			for (i = 1; args[i]; i++) {

				/* Ask for extraction path */
				printf(_("%sFile%s: %s\n"), bold, df_c, args[i]);

				char *ext_path = (char *)NULL;

				while (!ext_path) {
					ext_path = rl_no_hist(_("Extraction path: "));

					if (!ext_path)
						continue;

					if (ext_path && !*ext_path) {
						free(ext_path);
						ext_path = (char *)NULL;
						continue;
					}
				}

				/* Construct and execute cmd */
				char *cmd[] = { "atool", "-X", ext_path, args[i], NULL };

				if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
					exit_status = EXIT_FAILURE;

				free(ext_path);
				ext_path = (char *)NULL;
			}
		break;

			/* ########## LIST ############## */

		case 'l':
			for (i = 1; args[i]; i++) {

				printf(_("%s%sFile%s: %s\n"), (i > 1) ? "\n" : "",
					   bold, df_c, args[i]);

				char *cmd[] = { "atool", "-l", args[i], NULL };

				if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
					exit_status = EXIT_FAILURE;
			}
		break;

			/* ########## MOUNT ############## */

		case 'm':
			for (i = 1; args[i]; i++) {

				/* Create mountpoint */
				char *mountpoint = (char *)NULL;

				if (xargs.stealth_mode == 1) {
					mountpoint = (char *)xnmalloc(strlen(args[i]) + 19,
										sizeof(char));

					sprintf(mountpoint, "/tmp/clifm-mounts/%s",
							args[i]);
				}

				else {
					mountpoint = (char *)xnmalloc(strlen(CONFIG_DIR)
										+ strlen(args[i]) + 9, sizeof(char));

					sprintf(mountpoint, "%s/mounts/%s", CONFIG_DIR,
							args[i]);
				}

				char *dir_cmd[] = { "mkdir", "-pm700", mountpoint, NULL };

				if (launch_execve(dir_cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS) {
					free(mountpoint);
					return EXIT_FAILURE;
				}

				/* Construct and execute cmd */
				char *cmd[] = { "archivemount", args[i], mountpoint, NULL };

				if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS) {
					free(mountpoint);
					continue;
				}

				/* List content of mountpoint if there is only
				 * one archive */
				if (files_num > 1) {
					printf(_("%s%s%s: Succesfully mounted "
							"on %s\n"), bold, args[i], df_c, mountpoint);
					free(mountpoint);
					continue;
				}

				if (xchdir(mountpoint, SET_TITLE) == -1) {
					fprintf(stderr, "archiver: %s: %s\n", mountpoint,
							strerror(errno));
					free(mountpoint);
					return EXIT_FAILURE;
				}

				free(ws[cur_ws].path);
				ws[cur_ws].path = (char *)xcalloc(strlen(mountpoint) + 1,
									   sizeof(char));
				strcpy(ws[cur_ws].path, mountpoint);

				free(mountpoint);

				add_to_jumpdb(ws[cur_ws].path);

				if (cd_lists_on_the_fly) {
					free_dirlist();
					if (list_dir() != EXIT_SUCCESS)
						exit_status = EXIT_FAILURE;
					add_to_dirhist(ws[cur_ws].path);
				}
			}
		break;

			/* ########## REPACK ############## */

		case 'r': {
			/* Ask for new archive/compression format */
			puts(_("Enter 'q' to quit"));

			char *format = (char *)NULL;
			while (!format) {
				format = rl_no_hist(_("New format (Ex: .tar.xz): "));
				if (!format)
					continue;

				if (!*format || (*format != '.' && *format != 'q')) {
					free(format);
					format = (char *)NULL;
					continue;
				}

				if (*format == 'q' && format[1] == '\0') {
					free(format);
					free(dec_files);
					return EXIT_SUCCESS;
				}
			}

			/* Construct and execute cmd */
			char *cmd = (char *)NULL;
			cmd = (char *)xnmalloc(strlen(format) + strlen(dec_files)
								   + 16, sizeof(char));
			sprintf(cmd, "arepack -F %s -e %s", format, dec_files);

			if (launch_execle(cmd) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;

			free(format);
			free(dec_files);
			free(cmd);
		}
		break;
	}

	return exit_status;
}

static void
print_sort_method(void)
{
	printf(_("%s->%s Sorted by: "), mi_c, df_c);

	switch(sort) {
		case SNONE: puts(_("none")); break;
		case SNAME: printf(_("name %s\n"), (sort_reverse) ? "[rev]" : "");
			break;
		case SSIZE: printf(_("size %s\n"), (sort_reverse) ? "[rev]" : "");
			break;
		case SATIME: printf(_("atime %s\n"), (sort_reverse) ? "[rev]" : "");
			break;
		case SBTIME:
#if defined(HAVE_ST_BIRTHTIME) \
			|| defined(__BSD_VISIBLE) || defined(_STATX)
				printf(_("btime %s\n"), (sort_reverse) ? "[rev]" : "");
#else
				printf(_("btime (not available: using 'ctime') %s\n"),
					   (sort_reverse) ? "[rev]" : "");
#endif
			break;
		case SCTIME: printf(_("ctime %s\n"), (sort_reverse) ? "[rev]" : "");
			break;
		case SMTIME: printf(_("mtime %s\n"), (sort_reverse) ? "[rev]" : "");
			break;
#if __FreeBSD__ || _BE_POSIX
		case SVER: printf(_("version (not available: using 'name') %s\n"),
					   (sort_reverse) ? "[rev]" : "");
#else
		case SVER: printf(_("version %s\n"), (sort_reverse) ? "[rev]" : "");
#endif
			break;
		case SEXT: printf(_("extension %s\n"), (sort_reverse) ? "[rev]" : "");
			break;
		case SINO: printf(_("inode %s\n"), (sort_reverse) ? "[rev]" : "");
			break;
		case SOWN:
			if (light_mode)
				printf(_("owner (not available: using 'name') %s\n"),
					   (sort_reverse) ? "[rev]" : "");
			else
				printf(_("owner %s\n"), (sort_reverse) ? "[rev]" : "");
			break;
		case SGRP:
			if (light_mode)
				printf(_("group (not available: using 'name') %s\n"),
					   (sort_reverse) ? "[rev]" : "");
			else
				printf(_("group %s\n"), (sort_reverse) ? "[rev]" : "");
			break;
	}
}

static int
sort_function(char **arg)
{
	int exit_status = EXIT_FAILURE;

	/* No argument: Just print current sorting method */
	if (!arg[1]) {

		printf(_("Sorting method: "));

		switch(sort) {
			case SNONE:
				printf(_("none %s\n"), (sort_reverse) ? "[rev]": "");
				break;
			case SNAME:
				printf(_("name %s\n"), (sort_reverse) ? "[rev]": "");
				break;
			case SSIZE:
				printf(_("size %s\n"), (sort_reverse) ? "[rev]": "");
				break;
			case SATIME:
				printf(_("atime %s\n"), (sort_reverse) ? "[rev]": "");
				break;
			case SBTIME:
#if defined(HAVE_ST_BIRTHTIME) \
			|| defined(__BSD_VISIBLE) || defined(_STATX)
				printf(_("btime %s\n"), (sort_reverse) ? "[rev]": "");
#else
				printf(_("ctime %s\n"), (sort_reverse) ? "[rev]": "");
#endif
				break;
			case SCTIME:
				printf(_("ctime %s\n"), (sort_reverse) ? "[rev]": "");
				break;
			case SMTIME:
				printf(_("mtime %s\n"), (sort_reverse) ? "[rev]": "");
				break;
			case SVER:
#if __FreeBSD__ || _BE_POSIX
				printf(_("name %s\n"), (sort_reverse) ? "[rev]": "");
#else
				printf(_("version %s\n"), (sort_reverse) ? "[rev]": "");
#endif
				break;
			case SEXT: printf(_("extension %s\n"), (sort_reverse)
						   ? "[rev]" : "");
				break;
			case SINO:
				printf(_("inode %s\n"), (sort_reverse) ? "[rev]": "");
				break;
			case SOWN:
				printf(_("owner %s\n"), (sort_reverse) ? "[rev]": "");
				break;
			case SGRP:
				printf(_("group %s\n"), (sort_reverse) ? "[rev]": "");
				break;
		}

		return EXIT_SUCCESS;
	}

	/* Argument is alphanumerical string */
	else if (!is_number(arg[1])) {
		if (strcmp(arg[1], "rev") == 0) {

			if (sort_reverse)
				sort_reverse = 0;
			else
				sort_reverse = 1;

			if (cd_lists_on_the_fly) {
				/* sort_switch just tells list_dir() to print a line
				 * with the current sorting method at the end of the
				 * files list */
				sort_switch = 1;
				free_dirlist();
				exit_status = list_dir();
				sort_switch = 0;
			}

			return exit_status;
		}

		/* If arg1 is not a number and is not "rev", the fputs()
		 * above is executed */
	}

	/* Argument is a number */
	else {
		int int_arg = atoi(arg[1]);

		if (int_arg >= 0 && int_arg <= SORT_TYPES) {
			sort = int_arg;

			if (arg[2] && strcmp(arg[2], "rev") == 0) {
				if (sort_reverse)
					sort_reverse = 0;
				else
					sort_reverse = 1;
			}

			if (cd_lists_on_the_fly) {
				sort_switch = 1;
				free_dirlist();
				exit_status = list_dir();
				sort_switch = 0;
			}

			return exit_status;
		}
	}

	/* If arg1 is a number but is not in the range 0-SORT_TYPES,
	 * error */

	fputs(_("Usage: st [METHOD] [rev]\nMETHOD: 0 = none, "
			"1 = name, 2 = size, 3 = atime, 4 = btime, "
			"5 = ctime, 6 = mtime, 7 = version, 8 = extension, "
			"9 = inode, 10 = owner, 11 = group\n"), stderr);

	return EXIT_FAILURE;
}

static int
alphasort_insensitive(const struct dirent **a, const struct dirent **b)
/* This is a modification of the alphasort function that makes it case
 * insensitive. It also sorts without taking the initial dot of hidden
 * files into account. Note that strcasecmp() isn't locale aware. Use
 * only with C and english locales */
{
	int ret = strcasecmp(((*a)->d_name[0] == '.') ? (*a)->d_name + 1 :
						(*a)->d_name, ((*b)->d_name[0] == '.') ?
						(*b)->d_name + 1 : (*b)->d_name);

	if (!sort_reverse)
		return ret;

	return (ret - (ret * 2));
}

static int
remote_ftp(char *address, char *options)
{
	if (xargs.stealth_mode == 1) {
		printf("%s: Access to remote filesystems is disabled in "
			   "stealth mode\n", PROGRAM_NAME);
		return EXIT_SUCCESS;
	}

#if __FreeBSD__
	fprintf(stderr, _("%s: FTP is not yet supported on FreeBSD\n"),
			PROGRAM_NAME);
	return EXIT_FAILURE;
#endif

	if (!address || !*address)
		return EXIT_FAILURE;

	char *tmp_addr = savestring(address, strlen(address));

	char *p = tmp_addr;
	while (*p) {
		if (*p == '/')
			*p = '_';
		p++;
	}

	char *rmountpoint = (char *)xnmalloc(strlen(TMP_DIR)
									 + strlen(tmp_addr) + 9, sizeof(char));

	sprintf(rmountpoint, "%s/remote/%s", TMP_DIR, tmp_addr);
	free(tmp_addr);

	struct stat file_attrib;

	if (stat(rmountpoint, &file_attrib) == -1) {

		char *mkdir_cmd[] = { "mkdir", "-p", rmountpoint, NULL };

		if (launch_execve(mkdir_cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS) {
			fprintf(stderr, _("%s: %s: Cannot create mountpoint\n"),
					PROGRAM_NAME, rmountpoint);
			free(rmountpoint);
			return EXIT_FAILURE;
		}
	}

	else if (count_dir(rmountpoint) > 2) {
		fprintf(stderr, _("%s: %s: Mounpoint not empty\n"),
				PROGRAM_NAME, rmountpoint);
		free(rmountpoint);
		return EXIT_FAILURE;
	}

	/* CurlFTPFS does not require sudo */
	char *cmd[] = { "curlftpfs", address, rmountpoint, (options) ? "-o"
					: NULL, (options) ? options: NULL, NULL };
	int error_code = launch_execve(cmd, FOREGROUND, E_NOFLAG);

	if (error_code) {
		free(rmountpoint);
		return EXIT_FAILURE;
	}

	if (xchdir(rmountpoint, SET_TITLE) != 0) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, rmountpoint,
				strerror(errno));
		free(rmountpoint);
		return EXIT_FAILURE;
	}

	free(ws[cur_ws].path);
	ws[cur_ws].path = savestring(rmountpoint, strlen(rmountpoint));

	free(rmountpoint);

	if (cd_lists_on_the_fly) {
		free_dirlist();
		if (list_dir() != EXIT_SUCCESS)
			error_code = EXIT_FAILURE;
	}

	return error_code;

}

static int
remote_smb(char *address, char *options)
{
	if (xargs.stealth_mode == 1) {
		printf("%s: Access to remote filesystems is disabled in "
			   "stealth mode\n", PROGRAM_NAME);
		return EXIT_SUCCESS;
	}

#if __FreeBSD__
	fprintf(stderr, _("%s: SMB is not yet supported on FreeBSD\n"),
			PROGRAM_NAME);
	return EXIT_FAILURE;
#endif

	/* smb://[USER@]HOST[/SERVICE][/REMOTE-DIR] */

	if (!address || !*address)
		return EXIT_FAILURE;

	int free_address = 0;
	char *ruser = (char *)NULL, *raddress = (char *)NULL;
	char *tmp = strchr(address, '@');

	if (tmp) {
		*tmp = '\0';
		ruser = savestring(address, strlen(address));

		raddress = savestring(tmp + 1, strlen(tmp + 1));
		free_address = 1;
	}

	else
		raddress = address;

	char *addr_tmp = (char *)xnmalloc(strlen(raddress) + 3, sizeof(char));
	sprintf(addr_tmp, "//%s", raddress);

	char *p = raddress;

	while (*p) {
		if (*p == '/')
			*p = '_';
		p++;
	}

	char *rmountpoint = (char *)xnmalloc(strlen(raddress)
									 + strlen(TMP_DIR) + 9, sizeof(char));
	sprintf(rmountpoint, "%s/remote/%s", TMP_DIR, raddress);

	int free_options = 0;
	char *roptions = (char *)NULL;

	if (ruser) {
		roptions = (char *)xnmalloc(strlen(ruser) + strlen(options)
									+ 11, sizeof(char));
		sprintf(roptions, "username=%s,%s", ruser, options);
		free_options = 1;
	}
	else
		roptions = options;

	/* Create the mountpoint, if it doesn't exist */
	struct stat file_attrib;

	if (stat(rmountpoint, &file_attrib) == -1) {
		char *mkdir_cmd[] = { "mkdir", "-p", rmountpoint, NULL };

		if (launch_execve(mkdir_cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS) {

			if (free_options)
				free(roptions);

			if (ruser)
				free(ruser);

			if (free_address)
				free(raddress);

			free(rmountpoint);
			free(addr_tmp);

			fprintf(stderr, _("%s: %s: Cannot create mountpoint\n"),
					PROGRAM_NAME, rmountpoint);

			return EXIT_FAILURE;
		}
	}

	/* If the mountpoint already exists, check if it is empty */
	else if (count_dir(rmountpoint) > 2) {
		fprintf(stderr, _("%s: %s: Mountpoint not empty\n"),
				PROGRAM_NAME, rmountpoint);

		if (free_options)
			free(roptions);

		if (ruser)
			free(ruser);

		if (free_address)
			free(raddress);

		free(rmountpoint);
		free(addr_tmp);

		return EXIT_FAILURE;
	}

	int error_code = 1;

	/* Create and execute the SMB command */
	if (!(flags & ROOT_USR)) {
		char *cmd[] = { "sudo", "-u", "root", "mount.cifs", addr_tmp,
						rmountpoint, (roptions) ? "-o" : NULL,
						(roptions) ? roptions : NULL, NULL };
		error_code = launch_execve(cmd, FOREGROUND, E_NOFLAG);
	}

	else {
		char *cmd[] = { "mount.cifs", addr_tmp, rmountpoint, (roptions)
						? "-o" : NULL, (roptions) ? roptions : NULL, NULL };
		error_code = launch_execve(cmd, FOREGROUND, E_NOFLAG);
	}

	if (free_options)
		free(roptions);

	if (ruser)
		free(ruser);

	if (free_address)
		free(raddress);

	free(addr_tmp); 

	if (error_code) {
		free(rmountpoint);
		return EXIT_FAILURE;
	}

	/* If successfully mounted, chdir into mountpoint */
	if (xchdir(rmountpoint, SET_TITLE) != 0) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, rmountpoint,
				strerror(errno));
		free(rmountpoint);
		return EXIT_FAILURE;
	}

	free(ws[cur_ws].path);
	ws[cur_ws].path = savestring(rmountpoint, strlen(rmountpoint));

	free(rmountpoint);

	if (cd_lists_on_the_fly) {
		free_dirlist();
		if (list_dir() != EXIT_SUCCESS)
			error_code = EXIT_FAILURE;
	}

	return error_code;
}

static int
remote_ssh(char *address, char *options)
{
	if (xargs.stealth_mode == 1) {
		printf("%s: Access to remote filesystems is disabled in "
			   "stealth mode\n", PROGRAM_NAME);
		return EXIT_SUCCESS;
	}

#if __FreeBSD__
	fprintf(stderr, _("%s: SFTP is not yet supported on FreeBSD"),
			PROGRAM_NAME);
	return EXIT_FAILURE;
#endif

	if (!config_ok)
		return EXIT_FAILURE;

/*  char *sshfs_path = get_cmd_path("sshfs");
	if (!sshfs_path) {
		fprintf(stderr, _("%s: sshfs: Program not found.\n"),
				PROGRAM_NAME);
		return EXIT_FAILURE;
	} */

	if(!address || !*address)
		return EXIT_FAILURE;

	/* Create mountpoint */
	char *rname = savestring(address, strlen(address));

	/* Replace all slashes in address by underscore to construct the
	 * mounpoint name */
	char *p = rname;
	while (*p) {
		if (*p == '/')
			*p = '_';
		p++;
	}

	char *rmountpoint = (char *)xnmalloc(strlen(TMP_DIR) + strlen(rname)
										 + 9, sizeof(char));
	sprintf(rmountpoint, "%s/remote/%s", TMP_DIR, rname);
	free(rname);

	/* If the mountpoint doesn't exist, create it */
	struct stat file_attrib;
	if (stat(rmountpoint, &file_attrib) == -1) {
		char *mkdir_cmd[] = { "mkdir", "-p", rmountpoint, NULL };

		if (launch_execve(mkdir_cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS) {
			fprintf(stderr, _("%s: %s: Cannot create mountpoint\n"),
					PROGRAM_NAME, rmountpoint);
			free(rmountpoint);
			return EXIT_FAILURE;
		}
	}

	/* If it exists, make sure it is not populated */
	else if (count_dir(rmountpoint) > 2) {
		fprintf(stderr, _("%s: %s: Mounpoint not empty\n"),
				PROGRAM_NAME, rmountpoint);
		free(rmountpoint);
		return EXIT_FAILURE;
	}

	/* Construct the command */

	int error_code = 1;

	if ((flags & ROOT_USR)) {
		char *cmd[] = { "sshfs", address, rmountpoint, (options) ? "-o"
						: NULL, (options) ? options: NULL, NULL };
		error_code = launch_execve(cmd, FOREGROUND, E_NOFLAG);
	}

	else {
		char *cmd[] = { "sudo", "sshfs", address, rmountpoint, "-o",
						"allow_other", (options) ? "-o" : NULL,
						(options) ? options : NULL, NULL};
		error_code = launch_execve(cmd, FOREGROUND, E_NOFLAG);
	}

	if (error_code != EXIT_SUCCESS) {
		free(rmountpoint);
		return EXIT_FAILURE;
	}

	/* If successfully mounted, chdir into mountpoint */
	if (xchdir(rmountpoint, SET_TITLE) != 0) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, rmountpoint,
				strerror(errno));
		free(rmountpoint);
		return EXIT_FAILURE;
	}

	free(ws[cur_ws].path);
	ws[cur_ws].path = savestring(rmountpoint, strlen(rmountpoint));
	free(rmountpoint);

	if (cd_lists_on_the_fly) {
		free_dirlist();
		if (list_dir() != EXIT_SUCCESS)
			error_code = EXIT_FAILURE;
	}

	return error_code;
}

static char **
get_substr(char *str, const char ifs)
/* Get all substrings from STR using IFS as substring separator, and,
 * if there is a range, expand it. Returns an array containing all
 * substrings in STR plus expandes ranges, or NULL if: STR is NULL or
 * empty, STR contains only IFS(s), or in case of memory allocation
 * error */
{
	if (!str || *str == '\0')
		return (char **)NULL;

	/* ############## SPLIT THE STRING #######################*/

	char **substr = (char **)NULL;
	void *p = (char *)NULL;
	size_t str_len = strlen(str);
	size_t length = 0, substr_n = 0;

	char *buf = (char *)xnmalloc(str_len + 1, sizeof(char));

	while (*str) {
		while (*str != ifs && *str != '\0' && length < (str_len + 1))
			buf[length++] = *(str++);
		if (length) {
			buf[length] = '\0';
			p = (char *)realloc(substr, (substr_n + 1) * sizeof(char *));
			if (!p) {
				/* Free whatever was allocated so far */
				size_t i;

				for (i = 0; i < substr_n; i++)
					free(substr[i]);

				free(substr);

				return (char **)NULL;
			}
			substr = (char **)p;
			p = (char *)calloc(length + 1, sizeof(char));

			if (!p) {
				size_t i;

				for (i = 0; i < substr_n; i++)
					free(substr[i]);

				free(substr);

				return (char **)NULL;
			}

			substr[substr_n] = p;
			p = (char *)NULL;
			strncpy(substr[substr_n++], buf, length);
			length = 0;
		}
		else
			str++;
	}

	free(buf);

	if (!substr_n)
		return (char **)NULL;

	size_t i = 0, j = 0;
	p = (char *)realloc(substr, (substr_n + 1) * sizeof(char *));

	if (!p) {

		for (i = 0; i < substr_n; i++)
			free(substr[i]);

		free(substr);
		substr = (char **)NULL;

		return (char **)NULL;
	}

	substr = (char **)p;
	p = (char *)NULL;
	substr[substr_n] = (char *)NULL;

	/* ################### EXPAND RANGES ######################*/

	int afirst = 0, asecond = 0, ranges_ok = 0;

	for (i = 0; substr[i]; i++) {
		/* Check if substr is a valid range */
		ranges_ok = 0;
		/* If range, get both extremes of it */
		for (j = 1; substr[i][j]; j++) {
			if (substr[i][j] == '-') {
				/* Get strings before and after the dash */
				char *first = strbfr(substr[i], '-');

				if (!first)
					break;

				char *second = straft(substr[i], '-');

				if (!second) {
					free(first);
					break;
				}

				/* Make sure it is a valid range */
				if (is_number(first) && is_number(second)) {
					afirst = atoi(first), asecond = atoi(second);

					if (asecond <= afirst) {
						free(first);
						free(second);
						break;
					}

					/* We have a valid range */
					ranges_ok = 1;
					free(first);
					free(second);
				}
				else {
					free(first);
					free(second);
					break;
				}
			}
		}

		if (!ranges_ok)
			continue;

		/* If a valid range */
		size_t k = 0, next = 0;
		char **rbuf = (char **)NULL;
		rbuf = (char **)xcalloc((substr_n + (size_t)(asecond - afirst)
								+ 1), sizeof(char *));

		/* Copy everything before the range expression
		 * into the buffer */
		for (j = 0; j < i; j++)
			rbuf[k++] = savestring(substr[j], strlen(substr[j]));

		/* Copy the expanded range into the buffer */
		for (j = (size_t)afirst; j <= (size_t)asecond; j++) {
			rbuf[k] = (char *)xcalloc((size_t)DIGINUM((int)j) + 1, sizeof(char));
			sprintf(rbuf[k++], "%zu", j);
		}

		/* Copy everything after the range expression into
		 * the buffer, if anything */
		if (substr[i+1]) {
			next = k;

			for (j = (i + 1); substr[j]; j++) {
				rbuf[k++] = savestring(substr[j], strlen(substr[j]));
			}
		}

		else /* If there's nothing after last range, there's no next
		either */
			next = 0;

		/* Repopulate the original array with the expanded range and
		 * remaining strings */
		substr_n = k;
		for (j = 0; substr[j]; j++)
			free(substr[j]);

		substr = (char **)xrealloc(substr, (substr_n + 1) * sizeof(char *));

		for (j = 0;j < substr_n; j++) {
			substr[j] = savestring(rbuf[j], strlen(rbuf[j]));
			free(rbuf[j]);
		}

		free(rbuf);

		substr[j] = (char *)NULL;

		/* Proceede only if there's something after the last range */
		if (next)
			i = next;

		else
			break;
	}

	/* ############## REMOVE DUPLICATES ###############*/

	char **dstr = (char **)NULL;
	size_t len = 0, d;

	for (i = 0; i < substr_n; i++) {
		int duplicate = 0;

		for (d = (i + 1); d < substr_n; d++) {

			if (*substr[i] == *substr[d]
			&& strcmp(substr[i], substr[d]) == 0) {
				duplicate = 1;
				break;
			}
		}

		if (duplicate) {
			free(substr[i]);
			continue;
		}

		dstr = (char **)xrealloc(dstr, (len+1) * sizeof(char *));
		dstr[len++] = savestring(substr[i], strlen(substr[i]));
		free(substr[i]);
	}

	free(substr);

	dstr = (char **)xrealloc(dstr, (len + 1) * sizeof(char *));
	dstr[len] = (char *)NULL;

	return dstr;
}

static int
new_instance(char *dir, int sudo)
/* Open DIR in a new instance of the program (using TERM, set in the config
 * file, as terminal emulator) */
{
	if (!term) {
		fprintf(stderr, _("%s: Default terminal not set. Use the "
				"configuration file to set one\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	if (!(flags & GUI)) {
		fprintf(stderr, _("%s: Function only available for graphical "
				"environments\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	/* Get absolute path of executable name of itself */
	char *self = realpath("/proc/self/exe", NULL);

	if (!self) {
		fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
		return EXIT_FAILURE;
	}

	if (!dir) {
		free(self);
		return EXIT_FAILURE;
	}

	char *deq_dir = dequote_str(dir, 0);

	if (!deq_dir) {
		fprintf(stderr, _("%s: %s: Error dequoting filename\n"),
				PROGRAM_NAME, dir);
		free(self);
		return EXIT_FAILURE;
	}

	struct stat file_attrib;

	if (stat(deq_dir, &file_attrib) == -1) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, deq_dir,
				strerror(errno));
		free(self);
		free(deq_dir);
		return EXIT_FAILURE;
	}

	if ((file_attrib.st_mode & S_IFMT) != S_IFDIR) {
		fprintf(stderr, _("%s: %s: Not a directory\n"),
				PROGRAM_NAME, deq_dir);
		free(self);
		free(deq_dir);
		return EXIT_FAILURE;
	}

	char *path_dir = (char *)NULL;

	if (*deq_dir != '/') {
		path_dir = (char *)xnmalloc(strlen(ws[cur_ws].path) + strlen(deq_dir)
									+ 2, sizeof(char));
		sprintf(path_dir, "%s/%s", ws[cur_ws].path, deq_dir);
	}
	else
		path_dir = deq_dir;

/*  char *cmd = (char *)xnmalloc(strlen(term) + strlen(self)
								 + strlen(path_dir) + 13, sizeof(char));
	sprintf(cmd, "%s %s -p \"%s\" &", term, self, path_dir);

	int ret = launch_execle(cmd);
	free(cmd); */

	char **tmp_term = (char **)NULL, **tmp_cmd = (char **)NULL;

	if (strcntchr(term, ' ') != -1) {

		tmp_term = get_substr(term, ' ');

		if (tmp_term) {
			size_t i;

			for (i = 0; tmp_term[i]; i++);

			size_t num = i;
			tmp_cmd = (char **)xrealloc(tmp_cmd, (i + (sudo ? 5 : 4))
										* sizeof(char *));
			for (i = 0; tmp_term[i]; i++) {
				tmp_cmd[i] = savestring(tmp_term[i], strlen(tmp_term[i]));
				free(tmp_term[i]);
			}
			free(tmp_term);

			i = num - 1;

			int plus = 1;

			if (sudo) {
				tmp_cmd[i + plus] = (char *)xnmalloc(strlen(self) + 1,
												  sizeof(char));
				strcpy(tmp_cmd[i + plus], "sudo");
				plus++;
			}

			tmp_cmd[i + plus] = (char *)xnmalloc(strlen(self) + 1, sizeof(char));
			strcpy(tmp_cmd[i + plus++], self);
			tmp_cmd[i + plus] = (char *)xnmalloc(3, sizeof(char));
			strcpy(tmp_cmd[i + plus++], "-p\0");
			tmp_cmd[i + plus] = (char *)xnmalloc(strlen(path_dir)
											  + 1, sizeof(char));
			strcpy(tmp_cmd[i + plus++], path_dir);
			tmp_cmd[i + plus] = (char *)NULL;
		}
	}

	int ret = -1;

	if (tmp_cmd) {
		ret = launch_execve(tmp_cmd, BACKGROUND, E_NOFLAG);

		for (size_t i = 0; tmp_cmd[i]; i++)
			free(tmp_cmd[i]);
		free(tmp_cmd);
	}

	else {
		fprintf(stderr, _("%s: No option specified for '%s'\n"
				"Trying '%s -e %s -p %s'\n"), PROGRAM_NAME, term,
				term, self, ws[cur_ws].path);

		if (sudo) {
			char *cmd[] = { term, "-e", "sudo", self, "-p", path_dir, NULL };
			ret = launch_execve(cmd, BACKGROUND, E_NOFLAG);
		}

		else {
			char *cmd[] = { term, "-e", self, "-p", path_dir, NULL };
			ret = launch_execve(cmd, BACKGROUND, E_NOFLAG);
		}
	}

	if (*deq_dir != '/')
		free(path_dir);

	free(deq_dir);
	free(self);

	if (ret != EXIT_SUCCESS)
		fprintf(stderr, _("%s: Error lauching new instance\n"), PROGRAM_NAME);

	return ret;
}

static void
add_to_cmdhist(const char *cmd)
{
	if (!cmd)
		return;

	/* For readline */
	add_history(cmd);

	if (config_ok)
		append_history(1, HIST_FILE);

	/* For us */
	/* Add the new input to the history array */
	size_t cmd_len = strlen(cmd);
	history = (char **)xrealloc(history, (size_t)(current_hist_n + 2)
								* sizeof(char *));
	history[current_hist_n++] = savestring(cmd, cmd_len);

	history[current_hist_n] = (char *)NULL;
}

static int
record_cmd(char *input)
/* Returns 1 if INPUT should be stored in history and 0 if not */
{
	/* NULL input */
	if (!input)
		return 0;

	/* Blank lines */
	unsigned int blank = 1;
	char *p = input;

	while (*p) {
		if (*p > ' ') {
			blank = 0;
			break;
		}
		p++;
	}

	if (blank)
		return 0;

	/* Rewind the pointer to the beginning of the input line */
	p = input;

	/* Commands starting with space */
	if (*p == ' ')
		return 0;

	/* Exit commands */
	switch (*p) {

	case 'q':
		if (*(p + 1) == '\0' || strcmp(p, "quit") == 0)
			return 0;
		break;

	case 'Q':
		if (*(p + 1) == '\0')
			return 0;
		break;

	case 'e':
		if (*(p + 1) == 'x' && strcmp(p, "exit") == 0)
			return 0;
		break;

	case 'z':
		if (*(p + 1) == 'z' && *(p + 2) == '\0')
			return 0;
		break;

	case 's':
		if (*(p + 1) == 'a' && strcmp(p, "salir") == 0)
			return 0;
		break;

	case 'c':
		if (*(p + 1) == 'h' && strcmp(p, "chau") == 0)
			return 0;
		break;

	default: break;
	}

	/* History */
	if (*p == '!' && (_ISDIGIT(*(p + 1))
	|| (*(p + 1) == '-' && _ISDIGIT(*(p + 2)))
	|| ((*(p + 1) == '!') && *(p + 2) == '\0')))
		return 0;

	/* Consequtively equal commands in history */
	if (history && history[current_hist_n - 1]
	&& *p == *history[current_hist_n - 1]
	&& strcmp(p, history[current_hist_n - 1]) == 0)
		return 0;

	return 1;
}

static int
alias_import(char *file)
{
	if (xargs.stealth_mode == 1) {
		printf("%s: The alias function is disabled in stealth mode\n",
			   PROGRAM_NAME);
		return EXIT_SUCCESS;
	}

	if (!file)
		return EXIT_FAILURE;

	char rfile[PATH_MAX] = "";
	rfile[0] = '\0';

/*  if (*file == '~' && *(file + 1) == '/') { */

	if (*file == '~') {
		char *file_exp = tilde_expand(file);

		if (!file_exp) {
			fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, file, strerror(errno));
			return EXIT_FAILURE;
		}

		realpath(file_exp, rfile);
		free(file_exp);
	}

	else
		realpath(file, rfile);

	if (rfile[0] == '\0') {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, file, strerror(errno));
		return EXIT_FAILURE;
	}

	if (access(rfile, F_OK|R_OK) != 0) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, rfile, strerror(errno));
		return EXIT_FAILURE;
	}

	/* Open the file to import aliases from */
	FILE *fp = fopen(rfile, "r");

	if (!fp) {
		fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, rfile, strerror(errno));
		return EXIT_FAILURE;
	}

	/* Open CLiFM config file as well */
	FILE *config_fp = fopen(CONFIG_FILE, "a");

	if (!config_fp) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, CONFIG_FILE,
				strerror(errno));
		fclose(fp);
		return EXIT_FAILURE;
	}

	size_t line_size = 0, i;
	char *line = (char *)NULL;
	ssize_t line_len = 0;
	size_t alias_found = 0, alias_imported = 0;
	int first = 1;

	while ((line_len = getline(&line, &line_size, fp)) > 0) {

		if (strncmp(line, "alias ", 6) == 0) {
			alias_found++;

			/* If alias name conflicts with some internal command,
			 * skip it */
			char *alias_name = strbtw(line, ' ', '=');

			if (!alias_name)
				continue;

			if (is_internal_c(alias_name)) {
				fprintf(stderr, _("%s: Alias conflicts with "
						"internal command\n"), alias_name);
				free(alias_name);
				continue;
			}

			char *p = line + 6; /* p points now to the beginning of the
			alias name (because "alias " == 6) */

			/* Only accept single quoted aliases commands */
			char *tmp = strchr(p, '=');

			if (!tmp)
				continue;

			if (*(++tmp) != '\'') {
				free(alias_name);
				continue;
			}

			/* If alias already exists, skip it too */
			int exists = 0;

			for (i = 0; i < aliases_n; i++) {
				int alias_len = strcntchr(aliases[i], '=');

				if (alias_len != -1
				&& strncmp(aliases[i], p, (size_t)alias_len + 1) == 0) {
					exists = 1;
					break;
				}
			}

			if (!exists) {
				if (first) {
					first = 0;
					fputs("\n\n", config_fp);
				}

				alias_imported++;

				/* Write the new alias into CLiFM config file */
				fputs(line, config_fp);
			}
			else
				fprintf(stderr, _("%s: Alias already exists\n"),
						alias_name);

			free(alias_name);
		}
	}

	free(line);

	fclose(fp);
	fclose(config_fp);

	/* No alias was found in FILE */
	if (alias_found == 0) {
		fprintf(stderr, _("%s: %s: No alias found\n"), PROGRAM_NAME,
				rfile);
		return EXIT_FAILURE;
	}

	/* Aliases were found in FILE, but none was imported (either because
	 * they conflicted with internal commands or the alias already
	 * existed) */
	else if (alias_imported == 0) {
		fprintf(stderr, _("%s: No alias imported\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	/* If some alias was found and imported, print the corresponding
	 * message and update the aliases array */
	if (alias_imported > 1)
		printf(_("%s: %zu aliases were successfully imported\n"),
				PROGRAM_NAME, alias_imported);

	else
		printf(_("%s: 1 alias was successfully imported\n"),
			   PROGRAM_NAME);

	/* Add new aliases to the internal list of aliases */
	get_aliases();

	/* Add new aliases to the commands list for TAB completion */
	if (bin_commands) {

		for (i = 0; bin_commands[i]; i++)
			free(bin_commands[i]);

		free(bin_commands);
		bin_commands = (char  **)NULL;
	}

	get_path_programs();

	return EXIT_SUCCESS;
}

static int *
get_hex_num(const char *str)
/* Given this value: \xA0\xA1\xA1, return an array of integers with
 * the integer values for A0, A1, and A2 respectivelly */
{
	size_t i = 0;
	int *hex_n = (int *)calloc(3, sizeof(int));

	while (*str) {

		if (*str == '\\') {

			if (*(str + 1) == 'x') {
				str += 2;
				char *tmp = calloc(3, sizeof(char));
				strncpy(tmp, str, 2);

				if (i >= 3)
					hex_n = xrealloc(hex_n, (i + 1) * sizeof(int *));

				hex_n[i++] = hex2int(tmp);

				free(tmp);
			}
			else
				break;
		}
		str++;
	}

	hex_n = xrealloc(hex_n, (i + 1) * sizeof(int));
	hex_n[i] = -1; /* -1 marks the end of the int array */

	return hex_n;
}

static char *
decode_prompt(const char *line)
/* Decode the prompt string (encoded_prompt global variable) taken from
 * the configuration file. Based on the decode_prompt_string function
 * found in an old bash release (1.14.7). */
{
	if(!line)
		return (char *)NULL;

#define CTLESC '\001'
#define CTLNUL '\177'

	char *temp = (char *)NULL, *result = (char *)NULL;
	size_t result_len = 0;
	int c;

	while((c = *line++)) {
		/* We have a escape char */
		if (c == '\\') {

			/* Now move on to the next char */
			c = *line;

			switch (c) {

			case 'z': /* Exit status of last executed command */
				temp = (char *)xnmalloc(3, sizeof(char));
				temp[0] = ':';
				temp[1] = (exit_code) ? '(' : ')';
				temp[2] = '\0';
				goto add_string;

			case 'x': /* Hex numbers */
			{
				/* Go back one char, so that we have "\x ... n", which
				 * is what the get_hex_num() requires */
				line--;
				/* get_hex_num returns an array on integers corresponding
				 * to the hex codes found in line up to the fisrt non-hex
				 * expression */
				int *hex = get_hex_num(line);
				int n = 0, i = 0, j;
				/* Count how many hex expressions were found */
				while (hex[n++] != -1);
				n--;
				/* 2 + n == CTLEST + 0x00 + amount of hex numbers*/
				temp = xnmalloc(2 + (size_t)n, sizeof(char));
				/* Construct the line: "\001hex1hex2...n0x00"*/
				temp[0] = CTLESC;
				for (j = 1; j < (1 + n); j++)
					temp[j] = (char)hex[i++];
				temp[1 + n] = '\0';
				/* Set the line pointer after the first non-hex
				 * expression to continue processing */
				line += (i * 4);
				c = 0;
				free(hex);
				goto add_string;
			}

			case 'e': /* Escape char */
				temp = xnmalloc(3, sizeof(char));
				line++;
				temp[0] = CTLESC;
				/* 27 (dec) == 033 (octal) == 0x1b (hex) == \e */
				temp[1] = 27;
				temp[2] = '\0';
				c = 0;
				goto add_string;

			case '0': /* Octal char */
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			{
				char octal_string[4];
				int n;

				strncpy(octal_string, line, 3);
				octal_string[3] = '\0';

				n = read_octal(octal_string);
				temp = xnmalloc(3, sizeof(char));

				if (n == CTLESC || n == CTLNUL) {
					line += 3;
					temp[0] = CTLESC;
					temp[1] = (char)n;
					temp[2] = '\0';
				}
				else if (n == -1) {
					temp[0] = '\\';
					temp[1] = '\0';
				}
				else {
					line += 3;
					temp[0] = (char)n;
					temp[1] = '\0';
				}

				c = 0;
				goto add_string;
			}

			case 'c': /* Program name */
				temp = savestring(PNL, strlen(PNL));
				goto add_string;

			case 't': /* Time: 24-hour HH:MM:SS format */
			case 'T': /* 12-hour HH:MM:SS format */
			case 'A': /* 24-hour HH:MM format */
			case '@': /* 12-hour HH:MM:SS am/pm format */
			case 'd': /* Date: abrev_weak_day, abrev_month_day month_num */
			{
				time_t rawtime = time(NULL);
				struct tm *tm = localtime(&rawtime);
				if (c == 't') {
					char time[9] = "";
					strftime(time, sizeof(time), "%H:%M:%S", tm);
					temp = savestring(time, sizeof(time));
				}
				else if (c == 'T') {
					char time[9] = "";
					strftime(time, sizeof(time), "%I:%M:%S", tm);
					temp = savestring(time, sizeof(time));
				}
				else if (c == 'A') {
					char time[6] = "";
					strftime(time, sizeof(time), "%H:%M", tm);
					temp = savestring(time, sizeof(time));
				}
				else if (c == '@') {
					char time[12] = "";
					strftime(time, sizeof(time), "%I:%M:%S %p", tm);
					temp = savestring(time, sizeof(time));
				}
				else { /* c == 'd' */
					char time[12] = "";
					strftime(time, sizeof(time), "%a %b %d", tm);
					temp = savestring(time, sizeof(time));
				}
				goto add_string;
			}

			case 'u': /* User name */
				temp = savestring(user, strlen(user));
				goto add_string;

			case 'h': /* Hostname up to first '.' */
			case 'H': /* Full hostname */
				temp = savestring(hostname, strlen(hostname));
				if (c == 'h') {
					int ret = strcntchr(hostname, '.');
					if (ret != -1) {
						temp[ret] = '\0';
					}
				}
				goto add_string;

			case 's': /* Shell name (after last slash)*/
				{
				if (!sys_shell) {
					line++;
					break;
				}
				char *shell_name = strrchr(sys_shell, '/');
				temp = savestring(shell_name + 1, strlen(shell_name) - 1);
				goto add_string;
				}

			case 'S': { /* Current workspace */
				char s[12];
				sprintf(s, "%d", cur_ws + 1);
				temp = savestring(s, 1);
				goto add_string;
				}

			case 'l': { /* Current mode */
				char s[2];
				s[0] = (light_mode ? 'L' : '\0');
				s[1] = '\0';
				temp = savestring(s, 1);
				goto add_string;
				}

			case 'p':
			case 'w': /* Full PWD */
			case 'W': /* Short PWD */
				{
				if (!ws[cur_ws].path) {
					line++;
					break;
				}

				/* Reduce HOME to "~" */
				int free_tmp_path = 0;
				char *tmp_path = (char *)NULL;
				if (strncmp(ws[cur_ws].path, user_home,
				user_home_len) == 0)
					tmp_path = home_tilde(ws[cur_ws].path);
				if (!tmp_path) {
					tmp_path = ws[cur_ws].path;
				}
				else
					free_tmp_path = 1;

				if (c == 'W') {
					char *ret = (char *)NULL;
					/* If not root dir (/), get last dir name */
					if (!(*tmp_path == '/' && !*(tmp_path + 1)))
						ret = strrchr(tmp_path, '/');

					if (!ret)
						temp = savestring(tmp_path, strlen(tmp_path));
					else
						temp = savestring(ret + 1, strlen(ret) - 1);
				}

				/* Reduce path only if longer than max_path */
				else if (c == 'p') {
					if (strlen(tmp_path) > (size_t)max_path) {
						char *ret = (char *)NULL;
						ret = strrchr(tmp_path, '/');
						if (!ret)
							temp = savestring(tmp_path,
											  strlen(tmp_path));
						else
							temp = savestring(ret + 1, strlen(ret) - 1);
					}
					else
						temp = savestring(tmp_path, strlen(tmp_path));
				}

				else /* If c == 'w' */
					temp = savestring(tmp_path, strlen(tmp_path));

				if (free_tmp_path)
					free(tmp_path);

				goto add_string;
				}

			case '$': /* '$' or '#' for normal and root user */
				if ((flags & ROOT_USR))
					temp = savestring("#", 1);
				else
					temp = savestring("$", 1);
				goto add_string;

			case 'a': /* Bell character */
			case 'r': /* Carriage return */
			case 'n': /* New line char */
				temp = savestring(" ", 1);
				if (c == 'n')
					temp[0] = '\n';
				else if (c == 'r')
					temp[0] = '\r';
				else
					temp[0] = '\a';
				goto add_string;

			case '[': /* Begin a sequence of non-printing characters.
			Mostly used to add color sequences. Ex: \[\033[1;34m\] */
			case ']': /* End the sequence */
				temp = xnmalloc(3, sizeof(char));
				temp[0] = '\001';
				temp[1] = (c == '[') ? RL_PROMPT_START_IGNORE
						  : RL_PROMPT_END_IGNORE;
				temp[2] = '\0';
				goto add_string;

			case '\\': /* Literal backslash */
				temp = savestring ("\\", 1);
				goto add_string;

			default:
				temp = savestring("\\ ", 2);
				temp[1] = (char)c;

			add_string:
				if (c)
					line++;
				result_len += strlen(temp);
				if (!result)
					result = (char *)xcalloc(result_len + 1, sizeof(char));
				else
					result = (char *)xrealloc(result, (result_len + 1)
											  * sizeof(char));
				strcat(result, temp);
				free(temp);
				break;
			}
		}

		/* If not escape code, check for command substitution, and if not,
		 * just add whatever char is there */
		else {
			/* Remove non-escaped quotes */
			if (c == '\'' || c == '"')
				continue;

			/* Command substitution */
			if (c == '$' && *line == '(') {
				
				/* Look for the ending parenthesis */
				int tmp = strcntchr(line, ')');

				if (tmp == -1)
					continue;

				/* Copy the cmd to be substituted and pass it to wordexp */
				char *tmp_str = (char *)xnmalloc(strlen(line) + 2, sizeof(char));
				sprintf(tmp_str, "$%s", line);

				tmp_str[tmp + 2] = '\0';
				line += tmp + 1;
				wordexp_t wordbuf;
				if (wordexp(tmp_str, &wordbuf, 0) != EXIT_SUCCESS) {
					free(tmp_str);
					continue;
				}

				free(tmp_str);

				if (wordbuf.we_wordc) {
					size_t j;
					for (j = 0; j < wordbuf.we_wordc; j++) {

						size_t word_len = strlen(wordbuf.we_wordv[j]);
						result_len += word_len;

						if (!result)
							result = (char *)xcalloc(result_len + 2, sizeof(char));
						else
							result = (char *)xrealloc(result, (result_len + 2)
													  * sizeof(char));
						strcat(result, wordbuf.we_wordv[j]);

						/* If not the last word in cmd output, add an space */
						if (j < wordbuf.we_wordc - 1) {
							result[result_len++] = ' ';
							result[result_len] = '\0';
						}
					}
				}

				wordfree(&wordbuf);
				continue;
			}

			result = (char *)xrealloc(result, (result_len + 2)
									  * sizeof(char));
			result[result_len++] = (char)c;
			result[result_len] = '\0';
		}
	}

	/* Remove trailing new line char, if any */
	if (result[result_len - 1] == '\n')
		result[result_len - 1] = '\0';

	return result;
}

static int
create_bm_file(void)
{
	if (!BM_FILE)
		return EXIT_FAILURE;

	struct stat file_attrib;

	if (stat(BM_FILE, &file_attrib) == -1) {
		FILE *fp = fopen(BM_FILE, "w+");

		if (!fp) {
			_err('e', PRINT_PROMPT, "bookmarks: '%s': %s\n", BM_FILE,
				 strerror(errno));
			return EXIT_FAILURE;
		}

		else {
			fprintf(fp, "### This is %s bookmarks file ###\n\n"
					"# Empty and commented lines are ommited\n"
					"# The bookmarks syntax is: [shortcut]name:path\n"
					"# Example:\n"
					"[c]clifm:%s\n", PROGRAM_NAME, CONFIG_DIR ?
					CONFIG_DIR : "path/to/file");
			fclose(fp);
		}
	}

	return EXIT_SUCCESS;
}

static int
load_bookmarks(void)
{
	if (create_bm_file() == EXIT_FAILURE)
		return EXIT_FAILURE;

	if (!BM_FILE)
		return EXIT_FAILURE;

	FILE *bm_fp = fopen(BM_FILE, "r");

	if (!bm_fp)
		return EXIT_FAILURE;

	size_t bm_total = 0;
	char tmp_line[256];

	while (fgets(tmp_line, (int)sizeof(tmp_line), bm_fp)) {

		if (!*tmp_line || *tmp_line == '#' || *tmp_line == '\n')
			continue;

		bm_total++;
	}

	if (!bm_total) {
		fclose(bm_fp);
		return EXIT_SUCCESS;
	}

	fseek(bm_fp, 0L, SEEK_SET);

	bookmarks = (struct bookmarks_t *)xnmalloc(bm_total + 1,
									sizeof(struct bookmarks_t));

	size_t line_size = 0;
	char *line = (char *)NULL;
	ssize_t line_len = 0;

	while ((line_len = getline(&line, &line_size, bm_fp)) > 0) {

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
		if (!tmp)
			bookmarks[bm_n].name = (char *)NULL;

		else {
			*tmp = '\0';
			bookmarks[bm_n].name = savestring(line, strlen(line));
		}

		if (!*(++tmp)) {
			bookmarks[bm_n++].path = (char *)NULL;
			continue;
		}

		else
			bookmarks[bm_n++].path = savestring(tmp, strlen(tmp));
	}

	free(line);
	fclose(bm_fp);

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

static void
save_last_path(void)
/* Store last visited directory for the restore last path and the
 * cd on quit functions */
{
	if (!config_ok || !CONFIG_DIR)
		return;

	char *last_dir = (char *)xnmalloc(strlen(CONFIG_DIR) + 7, sizeof(char));
	sprintf(last_dir, "%s/.last", CONFIG_DIR);

	FILE *last_fp;

	last_fp = fopen(last_dir, "w");

	if (!last_fp) {
		fprintf(stderr, _("%s: Error saving last visited "
				"directory\n"), PROGRAM_NAME);
		free(last_dir);
		return;
	}

	size_t i;
	for (i = 0; i < MAX_WS; i++) {
		if (ws[i].path) {
			/* Mark current workspace with an asterisk. It will
			 * be read at startup by get_last_path */
			if ((size_t)cur_ws == i)
				fprintf(last_fp, "*%zu:%s\n", i, ws[i].path);
			else
				fprintf(last_fp, "%zu:%s\n", i, ws[i].path);
		}
	}

	fclose(last_fp);

	char *last_dir_tmp = xnmalloc(strlen(CONFIG_DIR_GRAL) + 7, sizeof(char *));
	sprintf(last_dir_tmp, "%s/.last", CONFIG_DIR_GRAL);

	if (cd_on_quit) {
		char *cmd[] = { "cp", "-p", last_dir, last_dir_tmp,
						NULL };

		launch_execve(cmd, FOREGROUND, E_NOFLAG);
	}

	/* If not cd on quit, remove the file */
	else {
		char *cmd[] = { "rm", "-f", last_dir_tmp, NULL };

		launch_execve(cmd, FOREGROUND, E_NOFLAG);
	}

	free(last_dir_tmp);
	free(last_dir);

	return;
}

static int
get_profile_names(void)
{
	if (!CONFIG_DIR_GRAL)
		return EXIT_FAILURE;

	char *pf_dir = (char *)xnmalloc(strlen(CONFIG_DIR_GRAL) + 10, sizeof(char));
	sprintf(pf_dir, "%s/profiles", CONFIG_DIR_GRAL);

	struct dirent **profs = (struct dirent **)NULL;
	int files_n = scandir(pf_dir, &profs, NULL, xalphasort);

	free(pf_dir);
	pf_dir = (char *)NULL;

	if (files_n == -1)
		return EXIT_FAILURE;

	size_t i, pf_n = 0;

	for (i = 0; i < (size_t)files_n; i++) {

		if (profs[i]->d_type == DT_DIR
		/* Discard ".", "..", and hidden dirs */
		&& *profs[i]->d_name != '.') {
			profile_names = (char **)xrealloc(profile_names, (pf_n + 1)
											  * sizeof(char *));
			profile_names[pf_n++] = savestring(profs[i]->d_name,
										strlen(profs[i]->d_name));
		}

		free(profs[i]);
	}

	free(profs);
	profs = (struct dirent **)NULL;

	profile_names = (char **)xrealloc(profile_names, (pf_n + 1)
									  * sizeof(char *));
	profile_names[pf_n] = (char *)NULL;

	return EXIT_SUCCESS;
}

static int
profile_add(const char *prof)
{
	if (!prof)
		return EXIT_FAILURE;

	int found = 0;
	size_t i;

	for (i = 0; profile_names[i]; i++) {

		if (*prof == *profile_names[i]
		&& strcmp(prof, profile_names[i]) == 0) {
			found = 1;
			break;
		}
	}

	if (found) {
		fprintf(stderr, _("%s: %s: Profile already exists\n"),
				PROGRAM_NAME, prof);
		return EXIT_FAILURE;
	}

	if (!home_ok) {
		fprintf(stderr, _("%s: %s: Cannot create profile: Home "
		"directory not found\n"), PROGRAM_NAME, prof);
		return EXIT_FAILURE;
	}

	size_t pnl_len = strlen(PNL);
	/* ### GENERATE PROGRAM'S CONFIG DIRECTORY NAME ### */
	char *NCONFIG_DIR = (char *)xnmalloc(strlen(CONFIG_DIR_GRAL)
										+ strlen(prof) + 11, sizeof(char));
	sprintf(NCONFIG_DIR, "%s/profiles/%s", CONFIG_DIR_GRAL, prof);

	/* #### CREATE THE CONFIG DIR #### */
	char *tmp_cmd[] = { "mkdir", "-p", NCONFIG_DIR, NULL };
	int ret = launch_execve(tmp_cmd, FOREGROUND, E_NOFLAG);

	if (ret != EXIT_SUCCESS) {
		fprintf(stderr, _("%s: mkdir: %s: Error creating "
				"configuration directory\n"), PROGRAM_NAME,
				NCONFIG_DIR);

		free(NCONFIG_DIR);

		return EXIT_FAILURE;
	}

	/* If the config dir is fine, generate config file names */
	int error_code = 0;
	size_t config_len = strlen(NCONFIG_DIR);

	char *NCONFIG_FILE = (char *)xcalloc(config_len + pnl_len + 4,
										 sizeof(char));
	sprintf(NCONFIG_FILE, "%s/%src", NCONFIG_DIR, PNL);
	char *NHIST_FILE = (char *)xcalloc(config_len + 13, sizeof(char));
	sprintf(NHIST_FILE, "%s/history.cfm", NCONFIG_DIR);
	char *NPROFILE_FILE = (char *)xcalloc(config_len + pnl_len + 10,
										  sizeof(char));
	sprintf(NPROFILE_FILE, "%s/%s_profile", NCONFIG_DIR, PNL);
	char *NMIME_FILE = (char *)xcalloc(config_len + 14, sizeof(char));
	sprintf(NMIME_FILE, "%s/mimelist.cfm", NCONFIG_DIR);

/*  char *NMSG_LOG_FILE = (char *)xcalloc(config_len + 14, sizeof(char));
	sprintf(NMSG_LOG_FILE, "%s/messages.cfm", NCONFIG_DIR);
	char *NBM_FILE = (char *)xcalloc(config_len + 15, sizeof(char));
	sprintf(NBM_FILE, "%s/bookmarks.cfm", NCONFIG_DIR);
	char *NLOG_FILE = (char *)xcalloc(config_len + 9, sizeof(char));
	sprintf(NLOG_FILE, "%s/log.cfm", NCONFIG_DIR);
	char *NLOG_FILE_TMP = (char *)xcalloc(config_len + 13, sizeof(char));
	sprintf(NLOG_FILE_TMP, "%s/log_tmp.cfm", NCONFIG_DIR); */

	/* Create config files */

	/* #### CREATE THE HISTORY FILE #### */
	FILE *hist_fp = fopen(NHIST_FILE, "w+");

	if (!hist_fp) {
		fprintf(stderr, "%s: fopen: %s: %s\n", PROGRAM_NAME,
				NHIST_FILE, strerror(errno));
		error_code = EXIT_FAILURE;
	}

	else {
		/* To avoid malloc errors in read_history(), do not create
		 * an empty file */
		fputs("edit\n", hist_fp);
		fclose(hist_fp);
	}

	/* #### CREATE THE MIME CONFIG FILE #### */
	/* Try importing MIME associations from the system, and in case
	 * nothing can be imported, create an empty MIME associations
	 * file */
	ret = mime_import(NMIME_FILE);

	if (ret != EXIT_SUCCESS) {
		FILE *mime_fp = fopen(NMIME_FILE, "w");

		if (!mime_fp) {
			fprintf(stderr, "%s: fopen: %s: %s\n", PROGRAM_NAME,
					NMIME_FILE, strerror(errno));
			error_code = EXIT_FAILURE;
		}

		else {
			if ((flags & GUI))
				fputs("text/plain=gedit;kate;pluma;mousepad;"
					  "leafpad;nano;vim;vi;emacs;ed\n"
					  "*.cfm=gedit;kate;pluma;mousepad;leafpad;"
					  "nano;vim;vi;emacs;ed\n", mime_fp);
			else
				fputs("text/plain=nano;vim;vi;emacs\n"
					  "*.cfm=nano;vim;vi;emacs;ed\n", mime_fp);
			fclose(mime_fp);
		}
	}

	/* #### CREATE THE PROFILE FILE #### */
	FILE *profile_fp = fopen(NPROFILE_FILE, "w");

	if (!profile_fp) {
		fprintf(stderr, _("%s: Error creating the profile file\n"),
				PROGRAM_NAME);
		error_code = EXIT_FAILURE;
	}

	else {
		fprintf(profile_fp, _("#%s profile\n"
				"#Write here the commands you want to be executed at "
				"startup\n#Ex:\n#echo -e \"%s, the anti-eye-candy/KISS "
				"file manager\"\n"), PROGRAM_NAME, PROGRAM_NAME);
		fclose(profile_fp);
	}

	/* #### CREATE THE CONFIG FILE #### */
		error_code = create_config(NCONFIG_FILE);

	/* Free stuff */

	free(NCONFIG_DIR);
	free(NCONFIG_FILE);
/*  free(NBM_FILE);
	free(NLOG_FILE);
	free(NMSG_LOG_FILE);
	free(NLOG_FILE_TMP); */
	free(NHIST_FILE);
	free(NPROFILE_FILE);
	free(NMIME_FILE);

	if (error_code == EXIT_SUCCESS) {
		printf(_("%s: '%s': Profile succesfully created\n"), PROGRAM_NAME, prof);

		for (i = 0; profile_names[i]; i++)
			free(profile_names[i]);

		get_profile_names();
	}

	else
		fprintf(stderr, _("%s: %s: Error creating profile\n"),
				PROGRAM_NAME, prof);

	return error_code;
}

static int
profile_del(const char *prof)
{
	if (xargs.stealth_mode == 1) {
		printf("%s: The profile function is disabled in stealth mode\n",
			   PROGRAM_NAME);
		return EXIT_SUCCESS;
	}

	if (!prof)
		return EXIT_FAILURE;

	/* Check if prof is a valid profile */
	int found = 0;
	size_t i;
	for (i = 0; profile_names[i]; i++) {
		if (*prof == *profile_names[i]
		&& strcmp(prof, profile_names[i]) == 0) {
			found = 1;
			break;
		}
	}

	if (!found) {
		fprintf(stderr, _("%s: %s: No such profile\n"), PROGRAM_NAME, prof);
		return EXIT_FAILURE;
	}

	char *tmp = (char *)xnmalloc(strlen(CONFIG_DIR_GRAL)
								+ strlen(prof) + 11, sizeof(char));
	sprintf(tmp, "%s/profiles/%s", CONFIG_DIR_GRAL, prof);

	char *cmd[] = { "rm", "-r", tmp, NULL };
	int ret = launch_execve(cmd, FOREGROUND, E_NOFLAG);
	free(tmp);

	if (ret == EXIT_SUCCESS) {
		printf(_("%s: '%s': Profile successfully removed\n"),
				PROGRAM_NAME, prof);

		for (i = 0; profile_names[i]; i++)
			free(profile_names[i]);

		get_profile_names();

		return EXIT_SUCCESS;
	}

	fprintf(stderr, _("%s: %s: Error removing profile\n"),
			PROGRAM_NAME, prof);
	return EXIT_FAILURE;
}

static void
check_file_size(char *log_file, int max)
/* Keep only the last MAX records in LOG_FILE */
{
	if (!config_ok)
		return;

	/* Create the file, if it doesn't exist */
	FILE *log_fp = (FILE *)NULL;
	struct stat file_attrib;

	if (stat(log_file, &file_attrib) == -1) {
		log_fp = fopen(log_file, "w");

		if (!log_fp) {
			_err(0, NOPRINT_PROMPT, "%s: '%s': %s\n", PROGRAM_NAME,
				 log_file, strerror(errno));
		}
		else
			fclose(log_fp);

		return; /* Return anyway, for, being a new empty file, there's
		no need to truncate it */
	}

	/* Once we know the files exists, keep only max logs */
	log_fp = fopen (log_file, "r");

	if (!log_fp) {
		_err(0, NOPRINT_PROMPT, "%s: log: %s: %s\n", PROGRAM_NAME,
		 log_file, strerror(errno));
		return;
	}

	int logs_num = 0, c;

	/* Count newline chars to get amount of lines in the log file */
	while ((c = fgetc(log_fp)) != EOF) {
		if (c == '\n')
			logs_num++;
	}

	if (logs_num <= max) {
		fclose(log_fp);
		return;
	}

	/* Set the file pointer to the beginning of the log file */
	fseek(log_fp, 0, SEEK_SET);

	/* Create a temp file to store only newest logs */
	char *rand_ext = gen_rand_str(6);

	if (!rand_ext) {
		fclose(log_fp);
		return;
	}

	char *tmp_file = (char *)xnmalloc(strlen(CONFIG_DIR) + 12, sizeof(char));
	sprintf(tmp_file, "%s/log.%s", CONFIG_DIR, rand_ext);
	free(rand_ext);

	FILE *log_fp_tmp = fopen(tmp_file, "w+");

	if (!log_fp_tmp) {
		fprintf(stderr, "log: %s: %s", tmp_file, strerror(errno));
		fclose(log_fp);
		free(tmp_file);
		return;
	}

	int i = 1;
	size_t line_size = 0;
	char *line_buff = (char *)NULL;
	ssize_t line_len = 0;

	while ((line_len = getline(&line_buff, &line_size, log_fp)) > 0) {

		/* Delete old entries = copy only new ones */
		if (i++ >= logs_num - (max - 1))
			fprintf(log_fp_tmp, "%s", line_buff);
	}

	free(line_buff);
	fclose(log_fp_tmp);
	fclose(log_fp);
	unlink(log_file);
	rename(tmp_file, log_file);
	free(tmp_file);

	return;
}

static char *
parse_usrvar_value(const char *str, const char c)
{
	if (c == '\0' || !str) 
		return (char *)NULL;

	/* Get whatever comes after c */
	char *tmp = (char *)NULL;
	tmp = strchr(str, c);

	/* Since we don't want c in our string, move on to the next char */
	tmp++;

	/* If nothing remains */
	if (!tmp)
		return (char *)NULL;

	/* Remove leading quotes */
	if ( *tmp == '"' || *tmp == '\'' )
		tmp++;

	/* Remove trailing spaces, tabs, new line chars, and quotes */
	size_t tmp_len = strlen(tmp), i;

	for (i = tmp_len - 1; tmp[i] && i > 0; i--) {

		if (tmp[i] != ' ' && tmp[i] != '\t' && tmp[i] != '"'
		&& tmp[i] != '\'' && tmp[i] != '\n')
			break;

		else
			tmp[i] = '\0';
	}

	if (!tmp)
		return (char *)NULL;

	/* Copy the result string into a buffer and return it */
	char *buf = (char *)NULL;
	buf = savestring(tmp, strlen(tmp));
	tmp=(char *)NULL;

	if (buf)
		return buf;

	return (char *)NULL;
}

static int
create_usr_var(char *str)
{
	char *name = strbfr(str, '=');
	char *value = parse_usrvar_value(str, '=');

	if (!name) {

		if (value)
			free(value);

		fprintf(stderr, _("%s: Error getting variable name\n"),
				PROGRAM_NAME);

		return EXIT_FAILURE;
	}

	if (!value) {
		free(name);

		fprintf(stderr, _("%s: Error getting variable value\n"),
				PROGRAM_NAME);

		return EXIT_FAILURE;
	}

	usr_var = xrealloc(usr_var, (size_t)(usrvar_n + 1)
					   * sizeof(struct usrvar_t));
	usr_var[usrvar_n].name = savestring(name, strlen(name));
	usr_var[usrvar_n++].value = savestring(value, strlen(value));

	free(name);
	free(value);

	return EXIT_SUCCESS;
}

static void
exec_profile(void)
{
	if (!config_ok || !PROFILE_FILE)
		return;

	FILE *fp = fopen(PROFILE_FILE, "r");

	if (!fp)
		return;

	size_t line_size = 0;
	char *line = (char *)NULL;
	ssize_t line_len = 0;

	while ((line_len = getline(&line, &line_size, fp)) > 0) {

		/* Skip empty and commented lines */
		if (!*line || *line == '\n' || *line == '#')
			continue;

		/* Remove trailing new line char */
		if (line[line_len - 1] == '\n')
			line[line_len - 1] = '\0';

		if (strchr(line, '=') && !_ISDIGIT(*line))
			create_usr_var(line);

		/* Parse line and execute it */
		else if (strlen(line) != 0) {
			args_n = 0;

			char **cmds = parse_input_str(line);

			if (cmds) {
				no_log = 1;
				exec_cmd(cmds);
				no_log = 0;
				int i = (int)args_n + 1;
				while (--i >= 0)
					free(cmds[i]);
				free(cmds);
				cmds = (char **)NULL;
			}
			args_n = 0;
		}
	}

	free(line);

	fclose(fp);
}

static void
free_bookmarks(void)
{
	if (!bm_n)
		return;

	size_t i;

	for (i = 0; i < bm_n; i++) {
		if (bookmarks[i].shortcut)
			free(bookmarks[i].shortcut);

		if (bookmarks[i].name)
			free(bookmarks[i].name);

		if (bookmarks[i].path)
			free(bookmarks[i].path);
	}

	free(bookmarks);
	bookmarks = (struct bookmarks_t *)NULL;

	for (i = 0; bookmark_names[i]; i++)
		free(bookmark_names[i]);

	free(bookmark_names);
	bookmark_names = (char **)NULL;

	bm_n = 0;

	return;
}

static int
get_history(void)
{
	if (!config_ok)
		return EXIT_FAILURE;

	if (current_hist_n == 0) /* Coming from main() */
		history = (char **)xcalloc(1, sizeof(char *));

	else { /* Only true when comming from 'history clear' */
		size_t i;

		for (i = 0; history[i]; i++)
			free(history[i]);

		history = (char **)xrealloc(history, 1 * sizeof(char *));
		current_hist_n = 0;
	}

	FILE *hist_fp = fopen(HIST_FILE, "r");

	if (!hist_fp) {
		_err('e', PRINT_PROMPT, "%s: history: '%s': %s\n",
			 PROGRAM_NAME, HIST_FILE,  strerror(errno));
		return EXIT_FAILURE;
	}

	size_t line_size = 0;
	char *line_buff = (char *)NULL;
	ssize_t line_len = 0;

	while ((line_len = getline(&line_buff, &line_size, hist_fp)) > 0) {

		line_buff[line_len - 1] = '\0';

		history = (char **)xrealloc(history, (current_hist_n + 2)
									* sizeof(char *));
		history[current_hist_n++] = savestring(line_buff, (size_t)line_len);
	}

	history[current_hist_n] = (char *)NULL;

	free(line_buff);
	fclose (hist_fp);

	return EXIT_SUCCESS;
}

static int
profile_set(const char *prof)
/* Switch profile to PROF */
{
	if (xargs.stealth_mode == 1) {
		printf("%s: The profile function is disabled in stealth mode\n",
			   PROGRAM_NAME);
		return EXIT_SUCCESS;
	}

	if (!prof)
		return EXIT_FAILURE;

	/* Check if prof is a valid profile */
	int found = 0;
	int i;

	for (i = 0; profile_names[i]; i++) {

		if (*prof == *profile_names[i]
		&& strcmp(prof, profile_names[i]) == 0) {
			found = 1;
			break;
		}
	}

	if (!found) {
		fprintf(stderr, _("%s: %s: No such profile\nTo add a new "
				"profile enter 'pf add PROFILE'\n"), PROGRAM_NAME, prof);

		return EXIT_FAILURE;
	}

	/* If changing to the current profile, do nothing */
	if ((!alt_profile && *prof == 'd' && strcmp(prof, "default") == 0)
	|| (alt_profile && *prof == *alt_profile
	&& strcmp(prof, alt_profile) == 0)) {

		printf(_("%s: '%s' is the current profile\n"), PROGRAM_NAME,
			   prof);

		return EXIT_SUCCESS;
	}

	if (restore_last_path)
		save_last_path();

	if (alt_profile) {
		free(alt_profile);
		alt_profile = (char *)NULL;
	}

	/* Set the new profile value */
	/* Default profile == (alt_profile == NULL) */
	if (*prof != 'd' || strcmp(prof, "default") != 0)
		alt_profile = savestring(prof, strlen(prof));

	/* Reset everything */
	reload_config();

	/* Check whether we have a working shell */
	if (access(sys_shell, X_OK) == -1) {
		_err('w', PRINT_PROMPT, _("%s: %s: System shell not found. Please "
			 "edit the configuration file to specify a working shell.\n"),
			 PROGRAM_NAME, sys_shell);
	}

	i = (int)usrvar_n;
	while (--i >= 0) {
		free(usr_var[i].name);
		free(usr_var[i].value);
	}
	usrvar_n = 0;

	i = (int)kbinds_n;
	while (--i >= 0) {
		free(kbinds[i].function);
		free(kbinds[i].key);
	}
	kbinds_n = 0;

	i = (int)actions_n;
	while (--i >= 0) {
		free(usr_actions[i].name);
		free(usr_actions[i].value);
	}
	actions_n = 0;

/*  my_rl_unbind_functions();

	create_kbinds_file();

	load_keybinds();

	rl_unbind_function_in_map(rl_hidden, rl_get_keymap());
	rl_bind_keyseq(find_key("toggle-hidden"), rl_hidden);
	my_rl_bind_functions(); */

	exec_profile();

	if (msgs_n) {
		i = (int)msgs_n;
		while (--i >= 0)
			free(messages[i]);
	}
	msgs_n = 0;

	if (config_ok) {
		/* Limit the log files size */
		check_file_size(LOG_FILE, max_log);
		check_file_size(MSG_LOG_FILE, max_log);

		/* Reset history */
		if (access(HIST_FILE, F_OK|W_OK) == 0) {
			clear_history(); /* This is for readline */
			read_history(HIST_FILE);
			history_truncate_file(HIST_FILE, max_hist);
		}

		else {
			FILE *hist_fp = fopen(HIST_FILE, "w");

			if (hist_fp) {
				fputs("edit\n", hist_fp);
				fclose(hist_fp);
			}

			else {
				_err('w', PRINT_PROMPT, _("%s: Error opening the "
					 "history file\n"), PROGRAM_NAME);
			}
		}

		get_history(); /* This is only for us */
	}

	free_bookmarks();
	load_bookmarks();

	load_actions();

	/* Reload PATH commands (actions are profile specific) */
	if (bin_commands) {

		for (i = 0; bin_commands[i]; i++)
			free(bin_commands[i]);

		free(bin_commands);
		bin_commands = (char  **)NULL;
	}

	if (paths) {

		i = (int)path_n;
		while (--i >= 0)
			free(paths[i]);
	}

	path_n = (size_t)get_path_env();

	get_path_programs();

	i = MAX_WS;
	while (--i >= 0) {
		free(ws[i].path);
		ws[i].path = (char *)NULL;
	}

	cur_ws = UNSET;

	if (restore_last_path)
		get_last_path();

	if (cur_ws == UNSET)
		cur_ws = DEF_CUR_WS;

	if (!ws[cur_ws].path) {
		char cwd[PATH_MAX] = "";
		getcwd(cwd, sizeof(cwd));
		if (!*cwd) {
			fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
			exit(EXIT_FAILURE);
		}
		ws[cur_ws].path = savestring(cwd, strlen(cwd));
	}

	if (xchdir(ws[cur_ws].path, SET_TITLE) == -1) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, ws[cur_ws].path,
				strerror(errno));
		return EXIT_FAILURE;
	}

	int exit_status = EXIT_SUCCESS;

	if (cd_lists_on_the_fly) {
		free_dirlist();
		exit_status = list_dir();
	}

	return exit_status;
}

static int
profile_function(char **comm)
{
	if (xargs.stealth_mode == 1) {
		printf("%s: The profile function is disabled in stealth mode\n",
			   PROGRAM_NAME);
		return EXIT_SUCCESS;
	}

	int exit_status = EXIT_SUCCESS;

	if (comm[1]) {
		if (*comm[1] == '-' && strcmp(comm[1], "--help") == 0)
			puts(_("Usage: pf, prof, profile [ls, list] [set, add, "
				   "del PROFILE]"));

		/* List profiles */
		else if (comm[1] && (strcmp(comm[1], "ls") == 0
		|| strcmp(comm[1], "list") == 0)) {
			size_t i;

			for (i = 0; profile_names[i]; i++)
				printf("%s\n", profile_names[i]);
		}

		/* Create a new profile */
		else if (strcmp(comm[1], "add") == 0)

			if (comm[2]) {
				exit_status = profile_add(comm[2]);
			}

			else {
				fputs("Usage: pf, prof, profile add PROFILE\n", stderr);
				exit_status = EXIT_FAILURE;
			}

		/* Delete a profile */
		else if (*comm[1] == 'd' && strcmp(comm[1], "del") == 0)
			if (comm[2])
				exit_status = profile_del(comm[2]);
			else {
				fputs("Usage: pf, prof, profile del PROFILE\n", stderr);
				exit_status = EXIT_FAILURE;
		}

		/* Switch to another profile */
		else if (*comm[1] == 's' && strcmp(comm[1], "set") == 0) {

			if (comm[2])
				exit_status = profile_set(comm[2]);

			else {
				fputs("Usage: pf, prof, profile set PROFILE\n", stderr);
				exit_status = EXIT_FAILURE;
			}
		}

		/* None of the above == error */
		else {

			fputs(_("Usage: pf, prof, profile [set, add, del PROFILE]\n"),
				  stderr);

			exit_status = EXIT_FAILURE;
		}
	}

	/* If only "pr" print the current profile name */
	else if (!alt_profile)
		printf("%s: profile: default\n", PROGRAM_NAME);

	else
		printf("%s: profile: '%s'\n", PROGRAM_NAME, alt_profile);

	return exit_status;
}

static char *
get_app(const char *mime, const char *ext)
/* Get application associated to a given MIME filetype or file extension.
 * Returns the first matching line in the MIME file or NULL if none is
 * found */
{
	if (!mime)
		return (char *)NULL;

	FILE *defs_fp = fopen(MIME_FILE, "r");

	if (!defs_fp) {
		fprintf(stderr, _("%s: %s: Error opening file\n"),
				PROGRAM_NAME, MIME_FILE);
		return (char *)NULL;
	}

	int found = 0, cmd_ok = 0;
	size_t line_size = 0;
	char *line = (char *)NULL, *app = (char *)NULL;
	ssize_t line_len = 0;

	while ((line_len = getline(&line, &line_size, defs_fp)) > 0) {
		found = mime_match = 0; /* Global variable to tell mime_open()
		if the application is associated to the file's extension or MIME
		type */
		if (*line == '#' || *line == '[' || *line == '\n')
			continue;

		char *tmp = strchr(line, '=');

		if (!tmp || !*(tmp + 1))
			continue;

		/* Truncate line in '=' to get only the ext/mimetype pattern/string */
		*tmp = '\0';
		regex_t regex;

		if (ext && *line == 'E' && line[1] == ':') {
			if (regcomp(&regex, line + 2, REG_NOSUB|REG_EXTENDED) == 0
			&& regexec(&regex, ext, 0, NULL, 0) == 0)
				found = 1;
		}

		else if (regcomp(&regex, line, REG_NOSUB|REG_EXTENDED) == 0
		&& regexec(&regex, mime, 0, NULL, 0) == 0)
			found = mime_match = 1;

		regfree(&regex);

		if (!found)
			continue;

		tmp++; /* We don't want the '=' char */

		size_t tmp_len = strlen(tmp);
		app = (char *)xrealloc(app, (tmp_len + 1) * sizeof(char));
		size_t app_len = 0;
		while (*tmp) {
			app_len = 0;
			/* Split the appplications line into substrings, if
			 * any */
			while (*tmp != '\0' && *tmp != ';' && *tmp != '\n'
			&& *tmp != '\'' && *tmp != '"')
				app[app_len++] = *(tmp++);

			while (*tmp == ' ') /* Remove leading spaces */
				tmp++;

			if (app_len) {
				app[app_len] = '\0';
				/* Check each application existence */
				char *file_path = (char *)NULL;
				/* If app contains spaces, the command to check is
				 * the string before the first space */
/*              int ret = strcntchr(app, ' ');

				if (ret != -1) {
					char *app_tmp = savestring(app, app_len);
					app_tmp[ret] = '\0';
					file_path = get_cmd_path(app_tmp);
					free(app_tmp);
				} */
				char *ret = strchr(app, ' ');
				if (ret) {
					*ret = '\0';
					file_path = get_cmd_path(app);
					*ret = ' ';
				}

				else
					file_path = get_cmd_path(app);

				if (file_path) {
					/* If the app exists, break the loops and
					 * return it */
					free(file_path);
					file_path = (char *)NULL;
					cmd_ok = 1;
				}

				else
					continue;
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
	}
	else {
		if (app)
			free(app);
	}

	return (char *)NULL;
}

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
	if (dup2(fileno(file_fp), STDOUT_FILENO) == -1 ) {
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

	char *cmd[] = { "file", "--mime-type", file, NULL };
	int ret = launch_execve(cmd, FOREGROUND, E_NOFLAG);

	dup2(stdout_bk, STDOUT_FILENO); /* Restore original stdout */
	dup2(stderr_bk, STDERR_FILENO); /* Restore original stderr */
	close(stdout_bk);
	close(stderr_bk);

	if (ret != EXIT_SUCCESS)
		return (char *)NULL;

	char *mime_type = (char *)NULL;

	if (access(MIME_TMP_FILE, F_OK) == 0) {
		file_fp = fopen(MIME_TMP_FILE, "r");

		if (file_fp) {
			char line[255] = "";
			fgets(line, (int)sizeof(line), file_fp);
			char *tmp = strrchr(line, ' ');

			if (tmp) {
				size_t len = strlen(tmp);

				if (tmp[len - 1] == '\n')
					tmp[len - 1] = '\0';

				mime_type = savestring(tmp + 1, strlen(tmp) - 1);
			}

			fclose(file_fp);
		}

		unlink(MIME_TMP_FILE);
	}

	if (mime_type)
		return mime_type;

	return (char *)NULL;
}

static int
mime_open(char **args)
/* Open a file according to the application associated to its MIME type
 * or extension. It also accepts the 'info' and 'edit' arguments, the
 * former providing MIME info about the corresponding file and the
 * latter opening the MIME list file */
{
	/* Check arguments */
	if (!args[1] || (*args[1] == '-' && strcmp(args[1], "--help") == 0)) {
		puts(_("Usage: mm, mime [info ELN/FILENAME] [edit]"));
		return EXIT_FAILURE;
	}

	/* Check the existence of the 'file' command. */
	char *file_path_tmp = (char *)NULL;

	if ((file_path_tmp = get_cmd_path("file")) == NULL) {
		fprintf(stderr, _("%s: file: Command not found\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	free(file_path_tmp);
	file_path_tmp = (char *)NULL;

	char *file_path = (char *)NULL, *deq_file = (char *)NULL;
	int info = 0, file_index = 0;

	if (*args[1] == 'e' && strcmp(args[1], "edit") == 0) {
		return mime_edit(args);
	}

	else if (*args[1] == 'i' && strcmp(args[1], "info") == 0) {

		if (!args[2]) {
			fputs(_("Usage: mm, mime info FILENAME\n"), stderr);
			return EXIT_FAILURE;
		}

		if (strchr(args[2], '\\')) {
			deq_file = dequote_str(args[2], 0);
			file_path = realpath(deq_file, NULL);
			free(deq_file);
			deq_file = (char *)NULL;
		}

		else
			file_path = realpath(args[2], NULL);

		if (!file_path) {
			fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, args[2],
				(is_number(args[2]) == 1) ? "No such ELN" : strerror(errno));
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

		if (strchr(args[1], '\\')) {
			deq_file = dequote_str(args[1], 0);
			file_path = realpath(deq_file, NULL);
			free(deq_file);
			deq_file = (char *)NULL;
		}

		else
			file_path = realpath(args[1], NULL);

		if (!file_path) {
			fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, args[1],
					strerror(errno));
			return -1;
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
	char *mime = get_mime(file_path);
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

		if (info)
			fputs(_("Associated application: None\n"), stderr);

		else {

			/* If an archive/compressed file, run the archiver function */
			if (is_compressed(file_path, 1) == 0) {

				char *tmp_cmd[] = { "ad", file_path, NULL };

				int exit_status = archiver(tmp_cmd, 'd');

				free(file_path);
				free(mime);

				if (ext)
					free(ext);

				return exit_status;
			}

			else
				fprintf(stderr, _("%s: %s: No associated application "
						"found\n"), PROGRAM_NAME, args[1]);
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
	 * the filename and the final NULL string (spaces + 3) */
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
		}

		else
			p++;
	}

	cmd[pos++] = savestring(file_path, strlen(file_path));
	cmd[pos] = (char *)NULL;

	int ret = launch_execve(cmd, (*args[args_num - 1] == '&'
			&& !args[args_num - 1][1]) ? BACKGROUND : FOREGROUND, E_NOSTDERR);

	free(file_path);
	free(app);

	int i = pos;
	while (--i >= 0)
		free(cmd[i]);
	free(cmd);

	return ret;
}

static int
mime_import(char *file)
/* Import MIME definitions from the system into FILE. This function will
 * only be executed if the MIME file is not found or when creating a new
 * profile. Returns zero if some association is found in the system
 * 'mimeapps.list' files, or one in case of error or no association
 * found */
{

	/* Open the internal MIME file */
	FILE *mime_fp = fopen(file, "w");

	if (!mime_fp)
		return EXIT_FAILURE;

	/* If not in X, just specify a few basic associations to make sure
	 * that at least 'mm edit' will work ('vi' should be installed in
	 * almost any Unix computer) */
	if (!(flags & GUI)) {
		fputs("text/plain=nano;vim;vi;emacs;ed\n"
			  "*.cfm=nano;vim;vi;emacs;ed\n", mime_fp);
		fclose(mime_fp);
		return EXIT_SUCCESS;
	}

	if (!user_home) {
		fclose(mime_fp);
		return EXIT_FAILURE;
	}

	/* Create a list of possible paths for the 'mimeapps.list' file */
	size_t home_len = strlen(user_home);
	char *config_path = (char *)NULL, *local_path = (char *)NULL;
	config_path = (char *)xcalloc(home_len + 23, sizeof(char));
	local_path = (char *)xcalloc(home_len + 41, sizeof(char));
	sprintf(config_path, "%s/.config/mimeapps.list", user_home);
	sprintf(local_path, "%s/.local/share/applications/mimeapps.list", user_home);

	char *mime_paths[] = { config_path, local_path,
						   "/usr/local/share/applications/mimeapps.list",
						   "/usr/share/applications/mimeapps.list",
						   "/etc/xdg/mimeapps.list", NULL };

	/* Check each mimeapps.list file and store its associations into
	 * FILE */
	size_t i;
	for (i = 0; mime_paths[i]; i++) {

		FILE *sys_mime_fp = fopen(mime_paths[i], "r");
		if (!sys_mime_fp)
			continue;

		size_t line_size = 0;
		char *line = (char *)NULL;
		ssize_t line_len = 0;

		/* Only store associations in the "Default Applications" section */
		int da_found = 0;

		while ((line_len = getline(&line, &line_size,
		sys_mime_fp)) > 0) {

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
			}
		}

		free(line);
		line = (char *)NULL;

		fclose(sys_mime_fp);
	}

	/* Make sure there is an entry for text/plain and *.cfm files, so
	 * that at least 'mm edit' will work. Gedit, kate, pluma, mousepad,
	 * and leafpad, are the default text editors of Gnome, KDE, Mate,
	 * XFCE, and LXDE respectivelly */
	fputs("text/plain=gedit;kate;pluma;mousepad;leafpad;nano;vim;"
		  "vi;emacs;ed\n"
		  "*.cfm=gedit;kate;pluma;mousepad;leafpad;nano;vim;vi;"
		  "emacs;ed\n", mime_fp);

	fclose(mime_fp);

	free(config_path);
	free(local_path);

	return EXIT_SUCCESS;
}

static int
mime_edit(char **args)
{
	int exit_status = EXIT_SUCCESS;

	if (!args[2]) {
		char *cmd[] = { "mime", MIME_FILE, NULL };

		if (mime_open(cmd) != 0) {
			fputs(_("Try 'mm, mime edit APPLICATION'\n"), stderr);
			exit_status = EXIT_FAILURE;
		}

	}

	else {
		char *cmd[] = { args[2], MIME_FILE, NULL };
		if (launch_execve(cmd, FOREGROUND, E_NOSTDERR) != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;
	}

	return exit_status;
}

static int
bulk_rename(char **args)
/* Rename a bulk of files (ARGS) at once. Takes files to be renamed
 * as arguments, and returns zero on success and one on error. The
 * procedude is quite simple: filenames to be renamed are copied into
 * a temporary file, which is opened via the mime function and shown
 * to the user to modify it. Once the filenames have been modified and
 * saved, modifications are printed on the screen and the user is
 * asked whether to perform the actual bulk renaming (via mv) or not.
 * I took this bulk rename method, just because it is quite simple and
 * KISS, from the fff filemanager. So, thanks fff! BTW, this method
 * is also implemented by ranger and nnn */
{
	if (!args[1])
		return EXIT_FAILURE;

	int exit_status = EXIT_SUCCESS;

	char BULK_FILE[PATH_MAX] = "";
	if (xargs.stealth_mode == 1)
		sprintf(BULK_FILE, "/tmp/.clifm_bulk_rename");
	else
		sprintf(BULK_FILE, "%s/.bulk_rename", TMP_DIR);

	FILE *bulk_fp;

	bulk_fp = fopen(BULK_FILE, "w+");
	if (!bulk_fp) {
		_err('e', PRINT_PROMPT, "bulk: '%s': %s\n", BULK_FILE,
			 strerror(errno));
		return EXIT_FAILURE;
	}

	size_t i, arg_total = 0;

	/* Copy all files to be renamed to the bulk file */
	for (i = 1; args[i]; i++) {

		/* Dequote filename, if necessary */
		if (strchr(args[i], '\\')) {
			char *deq_file = dequote_str(args[i], 0);

			if (!deq_file) {
				fprintf(stderr, _("bulk: %s: Error dequoting "
						"filename\n"), args[i]);
				continue;
			}

			strcpy(args[i], deq_file);
			free(deq_file);
		}

		fprintf(bulk_fp, "%s\n", args[i]);
	}

	arg_total = i;

	fclose(bulk_fp);

	/* Store the last modification time of the bulk file. This time
	 * will be later compared to the modification time of the same
	 * file after shown to the user */
	struct stat file_attrib;
	stat(BULK_FILE, &file_attrib);
	time_t mtime_bfr = (time_t)file_attrib.st_mtime;

	/* Open the bulk file via the mime function */
	char *cmd[] = { "mm", BULK_FILE, NULL };
	mime_open(cmd);

	/* Compare the new modification time to the stored one: if they
	 * match, nothing was modified */
	stat(BULK_FILE, &file_attrib);

	if (mtime_bfr == (time_t)file_attrib.st_mtime) {

		puts(_("bulk: Nothing to do"));

		if (unlink(BULK_FILE) == -1) {
			_err('e', PRINT_PROMPT, "%s: '%s': %s\n", PROGRAM_NAME,
				 BULK_FILE, strerror(errno));
			exit_status = EXIT_FAILURE;
		}

		return exit_status;
	}

	bulk_fp = fopen(BULK_FILE, "r");

	if (!bulk_fp) {
		_err('e', PRINT_PROMPT, "bulk: '%s': %s\n", BULK_FILE,
			 strerror(errno));
		return EXIT_FAILURE;
	}

	/* Go back to the beginning of the bulk file */
	fseek(bulk_fp, 0, SEEK_SET);

	/* Make sure there are as many lines in the bulk file as files
	 * to be renamed */
	size_t file_total = 1;
	char tmp_line[256];

	while (fgets(tmp_line, (int)sizeof(tmp_line), bulk_fp))
		file_total++;

	if (arg_total != file_total) {

		fputs(_("bulk: Line mismatch in rename file\n"), stderr);

		fclose(bulk_fp);

		if (unlink(BULK_FILE) == -1)
			_err('e', PRINT_PROMPT, "%s: '%s': %s\n", PROGRAM_NAME,
				 BULK_FILE, strerror(errno));

		return EXIT_FAILURE;
	}

	/* Go back to the beginning of the bulk file, again */
	fseek(bulk_fp, 0L, SEEK_SET);

	size_t line_size = 0;
	char *line = (char *)NULL;
	ssize_t line_len = 0;
	int modified = 0;

	i = 1;

	/* Print what would be done */
	while ((line_len = getline(&line, &line_size, bulk_fp)) > 0) {

		if (line[line_len - 1] == '\n')
			line[line_len - 1] = '\0';

		if (strcmp(args[i], line) != 0) {
			printf("%s %s->%s %s\n", args[i], mi_c, df_c, line);
			modified++;
		}

		i++;
	}

	/* If no filename was modified */
	if (!modified) {
		puts(_("bulk: Nothing to do"));

		if (unlink(BULK_FILE) == -1) {
			_err('e', PRINT_PROMPT, "%s: '%s': %s\n", PROGRAM_NAME,
				 BULK_FILE, strerror(errno));
			exit_status = EXIT_FAILURE;
		}

		free(line);
		fclose(bulk_fp);

		return exit_status;
	}

	/* Ask the user for confirmation */
	char *answer = (char *)NULL;

	while (!answer) {
		answer = readline(_("Continue? [y/N] "));

		if (strlen(answer) > 1) {
			free(answer);
			answer = (char *)NULL;
			continue;
		}

		switch(*answer) {
			case 'y': /* fallthrough */
			case 'Y': break;

			case 'n': /* fallthrough */
			case 'N': /* fallthrough */
			case '\0':
				free(answer);
				free(line);
				fclose(bulk_fp);
				return EXIT_SUCCESS;

			default:
				free(answer);
				answer = (char *)NULL;
				break;
		}
	}

	free(answer);

	/* Once again */
	fseek(bulk_fp, 0L, SEEK_SET);

	i = 1;

	/* Compose the mv commands and execute them */
	while ((line_len = getline(&line, &line_size, bulk_fp)) > 0) {

		if (line[line_len - 1] == '\n')
			line[line_len - 1] = '\0';

		if (strcmp(args[i], line) != 0) {
			char *tmp_cmd[] = { "mv", args[i], line, NULL };

			if (launch_execve(tmp_cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;
		}

		i++;
	}

	free(line);

	fclose(bulk_fp);

	if (unlink(BULK_FILE) == -1) {
		_err('e', PRINT_PROMPT, "%s: '%s': %s\n", PROGRAM_NAME,
			 BULK_FILE, strerror(errno));
		exit_status = EXIT_FAILURE;
	}

	if (cd_lists_on_the_fly) {
		free_dirlist();
		if (list_dir() != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;
	}

	return exit_status;
}

static char *
export(char **filenames, int open)
/* Export files in CWD (if FILENAMES is NULL), or files in FILENAMES,
 * into a temporary file. Return the address of this empt file if
 * success (it must be freed) or NULL in case of error */
{
	char *rand_ext = gen_rand_str(6);

	if (!rand_ext)
		return (char *)NULL;

	char *tmp_file = (char *)xnmalloc(strlen(TMP_DIR) + 14, sizeof(char));
	sprintf(tmp_file, "%s/.clifm%s", TMP_DIR, rand_ext);
	free(rand_ext);

	FILE *fp = fopen(tmp_file, "w");

	if (!fp) {
		free(tmp_file);
		return (char *)NULL;
	}

	size_t i;

	/* If no argument, export files in CWD */
	if (!filenames[1]) {
		for (i = 0; file_info[i].name; i++)
			fprintf(fp, "%s\n", file_info[i].name);
	}

	else {
		for (i = 1; filenames[i]; i++) {
			if (*filenames[i] == '.' && (!filenames[i][1]
			|| (filenames[i][1] == '.' && !filenames[i][2])))
				continue;

			fprintf(fp, "%s\n", filenames[i]);
		}
	}

	fclose(fp);

	if (!open)
		return tmp_file;

	char *cmd[] = { "mime", tmp_file, NULL };

	int ret = mime_open(cmd);

	if (ret == EXIT_SUCCESS)
		return tmp_file;

	else {
		free(tmp_file);
		return (char *)NULL;
	}
}

static int
edit_actions(void)
{
	if (xargs.stealth_mode == 1) {
		printf("%s: Access to configuration files is not allowed in "
			   "stealth mode\n", PROGRAM_NAME);
		return EXIT_SUCCESS;
	}

	/* Get actions file's current modification time */
	struct stat file_attrib;

	if (stat(ACTIONS_FILE, &file_attrib) == -1) {
		fprintf(stderr, "actions: %s: %s\n", ACTIONS_FILE, strerror(errno));
		return EXIT_FAILURE;
	}

	time_t mtime_bfr = (time_t)file_attrib.st_mtime;

	char *cmd[] = { "mm", ACTIONS_FILE, NULL };

	int ret = mime_open(cmd);

	if  (ret != EXIT_SUCCESS)
		return EXIT_FAILURE;

	/* Get modification time after opening the file */
	stat(ACTIONS_FILE, &file_attrib);

	/* If modification times differ, the file was modified after being
	 * opened */
	if (mtime_bfr != (time_t)file_attrib.st_mtime) {

		/* Reload the array of available actions */
		if (load_actions() != EXIT_SUCCESS)
			return EXIT_FAILURE;

		/* Reload PATH commands as well to add new action(s) */
		if (bin_commands) {
			size_t i;
			for (i = 0; bin_commands[i]; i++)
				free(bin_commands[i]);

			free(bin_commands);
			bin_commands = (char  **)NULL;
		}

		if (paths) {
			size_t i;
			for (i = 0; i < path_n; i++)
				free(paths[i]);
		}

		path_n = (size_t)get_path_env();

		get_path_programs();

	}

	return EXIT_SUCCESS;
}

static int
create_kbinds_file(void)
{
	if (!config_ok)
		return EXIT_FAILURE;

	struct stat file_attrib;

	if (stat(KBINDS_FILE, &file_attrib) != -1)
		return EXIT_SUCCESS;

	FILE *fp = fopen(KBINDS_FILE, "w");

	if (!fp) {
		_err('w', PRINT_PROMPT, "%s: '%s': %s\n", PROGRAM_NAME,
			 KBINDS_FILE, strerror(errno));
		return EXIT_FAILURE;
	}

	fprintf(fp, "# %s keybindings file\n\n\
# Use the 'kbgen' plugin (compile it first: gcc -o kbgen kbgen.c) to \n\
# find out the escape code for the key o key sequence you want. Use \n\
# either octal, hexadecimal codes or symbols.\n\
# Ex: For Alt-/ (in rxvt terminals) 'kbgen' will print the following \n\
# lines:\n\
# Hex  | Oct | Symbol\n\
# ---- | ---- | ------\n\
# \\x1b | \\033 | ESC (\\e)\n\
# \\x2f | \\057 | /\n\
# In this case, the keybinding, if using symbols, is: \"\\e/:function\"\n\
# In case you prefer the hex codes it would be: \\x1b\\x2f:function.\n\
# GNU emacs escape sequences are also allowed (ex: \"\\M-a\", Alt-a\n\
# in most keyboards, or \"\\C-r\" for Ctrl-r).\n\
# Some codes, especially those involving keys like Ctrl or the arrow\n\
# keys, vary depending on the terminal emulator and the system settings.\n\
# These keybindings should be set up thus on a per terminal basis.\n\
# You can also consult the terminfo database via the infocmp command.\n\
# See terminfo(5) and infocmp(1).\n\
\n\
# Alt-j\n\
previous-dir:\\M-j\n\
# Shift-left (rxvt)\n\
previous-dir2:\\e[d\n\
# Shift-left (xterm)\n\
previous-dir3:\\e[2D\n\
# Shift-left (others)\n\
previous-dir4:\\e[1;2D\n\
\n\
# Alt-k\n\
next-dir:\\M-k\n\
# Shift-right (rxvt)\n\
next-dir2:\\e[c\n\
# Shift-right (xterm)\n\
next-dir3:\\e[2C\n\
# Shift-right (others)\n\
next-dir4:\\e[1;2C\n\
first-dir:\\C-\\M-j\n\
last-dir:\\C-\\M-k\n\
\n\
# Alt-u\n\
parent-dir:\\M-u\n\
# Shift-up (rxvt)\n\
parent-dir2:\\e[a\n\
# Shift-up (xterm)\n\
parent-dir3:\\e[2A\n\
# Shift-up (others)\n\
parent-dir4:\\e[1;2A\n\
\n\
# Alt-e\n\
home-dir:\\M-e\n\
# Home key (rxvt)\n\
home-dir2:\\e[7~\n\
# Home key (xterm)\n\
home-dir3:\\e[H\n\
home-dir4:\n\
\n\
# Alt-r\n\
root-dir:\\M-r\n\
# Alt-/ (rxvt)\n\
root-dir2:\\e/\n\
#root-dir3:\n\
\n\
pinned-dir:\\M-p\n\
\n\
# Help\n\
# F1-3\n\
show-manpage:\\eOP\n\
show-cmds:\\eOQ\n\
show-kbinds:\\eOR\n\
\n\
new-instance:\\C-x\n\
previous-profile:\\C-\\M-o\n\
next-profile:\\C-\\M-p\n\
archive-sel:\\C-\\M-a\n\
rename-sel:\\C-\\M-r\n\
remove-sel:\\C-\\M-d\n\
trash-sel:\\C-\\M-t\n\
untrash-all:\\C-\\M-u\n\
paste-sel:\\C-\\M-v\n\
move-sel:\\C-\\M-n\n\
export-sel:\\C-\\M-e\n\
open-sel:\\C-\\M-g\n\
bookmark-sel:\\C-\\M-b\n\
refresh-screen:\\C-r\n\
clear-line:\\M-c\n\
clear-msgs:\\M-t\n\
show-dirhist:\\M-h\n\
toggle-hidden:\\M-i\n\
toggle-hidden2:\\M-.\n\
toggle-light:\\M-y\n\
toggle-long:\\M-l\n\
sort-previous:\\M-z\n\
sort-next:\\M-x\n\
bookmarks:\\M-b\n\
select-all:\\M-a\n\
deselect-all:\\M-d\n\
mountpoints:\\M-m\n\
folders-first:\\M-f\n\
selbox:\\M-s\n\
lock:\\M-o\n\
# F6-12\n\
open-mime:\\e[17~\n\
open-jump-db:\\e[18~\n\
edit-color-scheme:\\e[19~\n\
open-keybinds:\\e[20~\n\
open-config:\\e[21~\n\
open-bookmarks:\\e[23~\n\
quit:\\e[24~\n\n\
# Plugins\n\
# 1) Make sure your plugin is in the plugins directory (or use any of the\n\
# plugins in there)\n\
# 2) Link pluginx to your plugin using the 'actions edit' command. Ex:\n\
\"plugin1=myplugin.sh\"\n\
# 3) Set a keybinding here for pluginx. Ex: \"plugin1:\\M-7\"\n\n\
#plugin1:\n\
#plugin2:\n\
#plugin3:\n\
#plugin4:\n", PROGRAM_NAME);

	fclose(fp);

	return EXIT_SUCCESS;
}

static int
kbinds_reset(void)
{
	int exit_status = EXIT_SUCCESS;
	struct stat file_attrib;

	if (stat(KBINDS_FILE, &file_attrib) == -1)
		exit_status = create_kbinds_file();

	else {
		char *cmd[] = { "rm", KBINDS_FILE, NULL };
		if (launch_execve(cmd, FOREGROUND, E_NOFLAG) == EXIT_SUCCESS)
			exit_status = create_kbinds_file();
		else
			exit_status = EXIT_FAILURE;
	}

	if (exit_status == EXIT_SUCCESS)
		_err('n', PRINT_PROMPT, _("%s: Restart the program for changes "
			 "to take effect\n"), PROGRAM_NAME);

	return exit_status;
}

static int
kbinds_edit(void)
{
	if (xargs.stealth_mode == 1) {
		printf("%s: Access to configuration files is not allowed in "
			   "stealth mode\n", PROGRAM_NAME);
		return EXIT_SUCCESS;
	}

	if (!KBINDS_FILE)
		return EXIT_FAILURE;

	struct stat file_attrib;

	if (stat(KBINDS_FILE, &file_attrib) == -1) {
		create_kbinds_file();
		stat(KBINDS_FILE, &file_attrib);
	}

	time_t mtime_bfr = (time_t)file_attrib.st_mtime;

	char *cmd[] = { "mm", KBINDS_FILE, NULL };
	int ret = mime_open(cmd);

	if (ret != EXIT_SUCCESS)
		return EXIT_FAILURE;

	stat(KBINDS_FILE, &file_attrib);

	if (mtime_bfr == (time_t)file_attrib.st_mtime)
		return EXIT_SUCCESS;

	_err('n', PRINT_PROMPT, _("%s: Restart the program for changes to "
		 "take effect\n"), PROGRAM_NAME);

	return EXIT_SUCCESS;
}

static int
kbinds_function(char **args)
{
	if (!args)
		return EXIT_FAILURE;

	if (!args[1]) {
		size_t i;
		for (i = 0; i < kbinds_n; i++) {
			printf("%s: %s\n", kbinds[i].key, kbinds[i].function);
		}

		return EXIT_SUCCESS;
	}

	if (*args[1] == '-' && strcmp(args[1], "--help") == 0) {
		puts(_("Usage: kb, keybinds [edit] [reset]"));
		return EXIT_SUCCESS;
	}

	if (*args[1] == 'e' && strcmp(args[1], "edit") == 0)
		return kbinds_edit();

	if (*args[1] == 'r' && strcmp(args[1], "reset") == 0)
		return kbinds_reset();

	fputs(_("Usage: kb, keybinds [edit] [reset]\n"), stderr);
	return EXIT_FAILURE;
}

static int
get_link_ref(const char *link)
/* Return the filetype of the file pointed to by LINK, or -1 in case of
 * error. Possible return values:
	S_IFDIR: 40000 (octal) / 16384 (decimal, integer)
	S_IFREG: 100000 / 32768
	S_IFLNK: 120000 / 40960
	S_IFSOCK: 140000 / 49152
	S_IFBLK: 60000 / 24576
	S_IFCHR: 20000 / 8192
	S_IFIFO: 10000 / 4096
* See the inode manpage */
{
	if (!link)
		return (-1);

	char *linkname = realpath(link, (char *)NULL);
	if (linkname) {
		struct stat file_attrib;
		stat(linkname, &file_attrib);
		free(linkname);
		return (int)(file_attrib.st_mode & S_IFMT);
	}

	return (-1);
}

static char **
check_for_alias(char **args)
/* Returns the parsed aliased command in an array of strings, if
 * matching alias is found, or NULL if not. */
{
	if (!aliases_n || !aliases || !args)
		return (char **)NULL;

	char *aliased_cmd = (char *)NULL;
	size_t cmd_len = strlen(args[0]);
	char tmp_cmd[PATH_MAX * 2 + 1];
	snprintf(tmp_cmd, sizeof(tmp_cmd), "%s=", args[0]);

	register int i = (int)aliases_n;
	while (--i >= 0) {

		if (!aliases[i])
			continue;
		/* Look for this string: "command=", in the aliases file */
		/* If a match is found */

		if (*aliases[i] != *args[0] ||
		strncmp(tmp_cmd, aliases[i], cmd_len + 1) != 0)
			continue;

		/* Get the aliased command */
		aliased_cmd = strbtw(aliases[i], '\'', '\'');

		if (!aliased_cmd)
			return (char **)NULL;

		if (!*aliased_cmd) { /* zero length */
			free(aliased_cmd);
			return (char **)NULL;
		}

		args_n = 0; /* Reset args_n to be used by parse_input_str() */

		/* Parse the aliased cmd */
		char **alias_comm = parse_input_str(aliased_cmd);
		free(aliased_cmd);

		if (!alias_comm) {
			args_n = 0;
			fprintf(stderr, _("%s: Error parsing aliased command\n"),
					PROGRAM_NAME);
			return (char **)NULL;
		}

		register size_t j;

		/* Add input parameters, if any, to alias_comm */
		if (args[1]) {
			for (j = 1; args[j]; j++) {
				alias_comm = (char **)xrealloc(alias_comm,
							(++args_n + 2) * sizeof(char *));
				alias_comm[args_n] = savestring(args[j],
										strlen(args[j]));
			}
		}

		/* Add a terminating NULL string */
		alias_comm[args_n + 1] = (char *)NULL;

		/* Free original command */
		for (j = 0; args[j]; j++)
			free(args[j]);
		free(args);

		return alias_comm;
	}

	return (char **)NULL;
}

static void
exec_chained_cmds(char *cmd)
/* Execute chained commands (cmd1;cmd2 and/or cmd1 && cmd2). The function
 * is called by parse_input_str() if some non-quoted double ampersand or
 * semicolon is found in the input string AND at least one of these
 * chained commands is internal */
{
	if (!cmd)
		return;

	size_t i = 0, cmd_len = strlen(cmd);
	for (i = 0; i < cmd_len; i++) {
		char *str = (char *)NULL;
		size_t len = 0, cond_exec = 0;

		/* Get command */
		str = (char *)xcalloc(strlen(cmd) + 1, sizeof(char));
		while (cmd[i] && cmd[i] != '&' && cmd[i] != ';') {
			str[len++] = cmd[i++];
		}

		/* Should we execute conditionally? */
		if (cmd[i] == '&')
			cond_exec = 1;

		/* Execute the command */
		if (str) {
			char **tmp_cmd = parse_input_str(str);
			free(str);

			if (tmp_cmd) {
				int error_code = 0;
				size_t j;
				char **alias_cmd = check_for_alias(tmp_cmd);

				if (alias_cmd) {

					if (exec_cmd(alias_cmd) != 0)
						error_code = 1;

					for (j = 0; alias_cmd[j]; j++)
						free(alias_cmd[j]);

					free(alias_cmd);
				}

				else {

					if (exec_cmd(tmp_cmd) != 0)
						error_code = 1;

					for (j = 0; j <= args_n; j++)
						free(tmp_cmd[j]);

					free(tmp_cmd);
				}

				/* Do not continue if the execution was condtional and
				 * the previous command failed */
				if (cond_exec && error_code)
					break;
			}
		}
	}
}

static int
set_shell(char *str)
/* Set STR as the program current shell */
{
	if (!str || !*str)
		return EXIT_FAILURE;

	/* IF no slash in STR, check PATH env variable for a file named STR
	 * and get its full path*/
	char *full_path = (char *)NULL;

	if (strcntchr(str, '/') == -1)
		full_path = get_cmd_path(str);

	char *tmp = (char *)NULL;

	if (full_path)
		tmp = full_path;

	else
		tmp = str;

	if (access(tmp, X_OK) == -1) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, tmp, strerror(errno));
		return EXIT_FAILURE;
	}

	if (sys_shell)
		free(sys_shell);

	sys_shell = savestring(tmp, strlen(tmp));
	printf(_("Successfully set '%s' as %s default shell\n"), sys_shell,
		   PROGRAM_NAME);

	if (full_path)
		free(full_path);

	return EXIT_SUCCESS;
}

static size_t
get_colorschemes(void)
{
	if (!COLORS_DIR)
		return 0;

	struct stat attr;

	if (stat(COLORS_DIR, &attr) == -1)
		return 0;

	int schemes_total = count_dir(COLORS_DIR);

	if (schemes_total <= 2)
		return 0;

	color_schemes = (char **)xcalloc((size_t)schemes_total + 2, sizeof(char *));

	size_t i = 0;

	DIR *dir_p;
	struct dirent *ent;

	/* count_dir already opened and read this directory succesfully,
	 * so that we don't need to check opendir for errors */
	dir_p = opendir(COLORS_DIR);

	while ((ent = readdir(dir_p)) != NULL) {

		/* Skipp . and .. */
		char *name = ent->d_name;

		if (*name == '.' && (!name[1]
		|| (name[1] == '.' && !name[2])))
			continue;

		char *ret = strchr(name, '.');

		/* If the file contains not dot, or if its extension is not
		 * .cfm, or if it's just a hidden file named ".cfm", skip it */
		if (!ret || strcmp(ret, ".cfm") != 0 || ret == name)
			continue;

		*ret = '\0';

		color_schemes[i++] = savestring(name, strlen(name));
	}

	color_schemes[i] = (char *)NULL;

	closedir(dir_p);

	return i;
}

static int
is_internal(const char *cmd)
/* Check cmd against a list of internal commands. Used by parse_input_str()
 * to know if it should perform additional expansions, like glob, regex,
 * tilde, and so on. Only internal commands dealing with filenames
 * should be checked here */
{
	const char *int_cmds[] = {
		"cd",
		"o", "open",
		"s", "sel",
		"p", "pr", "prop",
		"r",
		"t", "tr", "trash",
		"mm", "mime",
		"bm", "bookmarks",
		"br", "bulk",
		"ac", "ad",
		"exp", "export",
		"pin",
		"jc", "jp",
		"bl", "le",
		"te",
		NULL
		};

	int found = 0;
	int i = (int)(sizeof(int_cmds) / sizeof(char *)) - 1;

	while (--i >= 0) {
		if (*cmd == *int_cmds[i] && strcmp(cmd, int_cmds[i]) == 0) {
			found = 1;
			break;
		}
	}

	if (found)
		return 1;

	/* Check for the search function as well */
	else if (*cmd == '/' && access(cmd, F_OK) != 0)
		return 1;

	return 0;
}

static int
quote_detector(char *line, int index)
/* Used by readline to check if a char in the string being completed is
 * quoted or not */
{
	if (index > 0 && line[index - 1] == '\\'
	&& !quote_detector(line, index - 1))
		return 1;

	return 0;
}

static char **
split_str(const char *str)
/* This function takes a string as argument and split it into substrings
 * taking tab, new line char, and space as word delimiters, except when
 * they are preceded by a quote char (single or double quotes) or in
 * case of command substitution ($(cmd) or `cmd`), in which case
 * eveything after the corresponding closing char is taken as one single
 * string. It also escapes spaecial chars. It returns an array of
 * splitted strings (without leading and terminating spaces) or NULL if
 * str is NULL or if no substring was found, i.e., if str contains
 * only spaces. */
{
	if (!str)
		return (char **)NULL;

	size_t buf_len = 0, words = 0, str_len = 0;
	char *buf = (char *)NULL;
	buf = (char *)xnmalloc(1, sizeof(char));;
	int quote = 0, close = 0;
	char **substr = (char **)NULL;

	while (*str) {
		switch (*str) {

		/* Command substitution */
		case '$': /* fallthrough */
		case '`':

			/* Define the closing char: If "$(" then ')', else '`' */
			if (*str == '$') {
				/* If escaped, it has no special meaning */
				if ((str_len && *(str - 1) == '\\')
				|| *(str + 1) != '(') {
					buf = (char *)xrealloc(buf, (buf_len + 1) * sizeof(char *));
					buf[buf_len++] = *str;
					break;
				}

				else
					close = ')';
			}

			else {
				/* If escaped, it has no special meaning */
				if (str_len && *(str - 1) == '\\') {
					buf = (char *)xrealloc(buf, (buf_len + 1) * sizeof(char *));
					buf[buf_len++] = *str;
					break;
				}

				else {
					/* If '`' advance one char. Otherwise the while
					 * below will stop at first char, which is not
					 * what we want */
					close = *str;
					str++;
					buf = (char *)xrealloc(buf, (buf_len + 1) * sizeof(char *));
					buf[buf_len++] = '`';
				}
			}

			/* Copy everything until null byte or closing char */
			while (*str && *str != close) {
				buf = (char *)xrealloc(buf, (buf_len + 1) * sizeof(char *));
				buf[buf_len++] = *(str++);
			}

			/* If the while loop stopped with a null byte, there was
			 * no ending close (either ')' or '`')*/
			if (!*str) {
				fprintf(stderr, _("%s: Missing '%c'\n"), PROGRAM_NAME,
						close);

				free(buf);
				buf = (char *)NULL;
				int i = (int)words;

				while (--i >= 0)
					free(substr[i]);

				free(substr);

				return (char **)NULL;
			}

			/* Copy the closing char and add an space: this function
			 * takes space as word breaking char, so that everything
			 * in the buffer will be copied as one single word */
			buf = (char *)xrealloc(buf, (buf_len + 2) * sizeof(char *));
			buf[buf_len++] = *str;
			buf[buf_len] = ' ';

		break;

		case '\'':
		case '"':
			/* If the quote is escaped, it has no special meaning */
			if (str_len && *(str - 1) == '\\') {
				buf = (char *)xrealloc(buf, (buf_len + 1) * sizeof(char *));
				buf[buf_len++] = *str;
				break;
			}

			/* If not escaped, move on to the next char */
			quote = *str;
			str++;

			/* Copy into the buffer whatever is after the first quote
			 * up to the last quote or NULL */
			while (*str && *str != quote) {

				/* If char has special meaning, escape it */
				if (is_quote_char(*str)) {
					buf = (char *)xrealloc(buf, (buf_len + 1) * sizeof(char *));
					buf[buf_len++] = '\\';
				}

				buf = (char *)xrealloc(buf, (buf_len + 1) * sizeof(char *));
				buf[buf_len++] = *(str++);
			}

			/* The above while breaks with NULL or quote, so that if
			 * *str is a null byte there was not ending quote */
			if (!*str) {
				fprintf(stderr, _("%s: Missing '%c'\n"), PROGRAM_NAME, quote);

				/* Free the current buffer and whatever was already
				 * allocated */
				free(buf);
				buf = (char *)NULL;
				int i = (int)words;

				while (--i >= 0)
					free(substr[i]);

				free(substr);

				return (char **)NULL;
			}
			break;

		/* TAB, new line char, and space are taken as word breaking
		 * characters */
		case '\t':
		case '\n':
		case ' ':

			/* If escaped, just copy it into the buffer */
			if (str_len && *(str - 1) == '\\') {
				buf = (char *)xrealloc(buf, (buf_len + 1) * sizeof(char *));
				buf[buf_len++] = *str;
			}

			/* If not escaped, break the string */
			else {
				/* Add a terminating null byte to the buffer, and, if
				 * not empty, dump the buffer into the substrings
				 * array */
				buf[buf_len] = '\0';

				if (buf_len > 0) {
					substr = (char **)xrealloc(substr, (words + 1)
											   * sizeof(char *));
					substr[words] = savestring(buf, buf_len);
					words++;
				}

				/* Clear te buffer to get a new string */
				memset(buf, '\0', buf_len);
				buf_len = 0;
			}
			break;

		/* If neither a quote nor a breaking word char nor command
		 * substitution, just dump it into the buffer */
		default:
			buf = (char *)xrealloc(buf, (buf_len + 1) * sizeof(char *));
			buf[buf_len++] = *str;
			break;
		}

		str++;
		str_len++;
	}

	/* The while loop stops when the null byte is reached, so that the
	 * last substring is not printed, but still stored in the buffer.
	 * Therefore, we need to add it, if not empty, to our subtrings
	 * array */
	buf[buf_len] = '\0';

	if (buf_len > 0) {

		if (!words)
			substr = (char **)xcalloc(words + 1, sizeof(char *));

		else
			substr = (char **)xrealloc(substr, (words + 1) * sizeof(char *));
		substr[words] = savestring(buf, buf_len);
		words++;
	}

	free(buf);
	buf = (char *)NULL;

	if (words) {
		/* Add a final null string to the array */
		substr = (char **)xrealloc(substr, (words + 1) * sizeof(char *));
		substr[words] = (char *)NULL;

		args_n = words - 1;
		return substr;
	}

	else {
		args_n = 0; /* Just in case, but I think it's not needed */
		return (char **)NULL;
	}
}

static int
cd_function(char *new_path)
/* Change CliFM working directory to NEW_PATH */
{
	/* If no argument, change to home */
	if (!new_path || !*new_path) {

		if (!user_home) {
			fprintf(stderr, _("%s: cd: Home directory not found\n"),
					PROGRAM_NAME);
			return EXIT_FAILURE;
		}

		if (xchdir(user_home, SET_TITLE) != EXIT_SUCCESS) {
			fprintf(stderr, "%s: cd: %s: %s\n", PROGRAM_NAME,
					user_home, strerror(errno));
			return EXIT_FAILURE;
		}

		free(ws[cur_ws].path);
		ws[cur_ws].path = savestring(user_home, strlen(user_home));
	}

	/* If we have some argument, dequote it, resolve it with realpath(),
	 * cd into the resolved path, and set the path variable to this
	 * latter */
	else {

		if (strchr(new_path, '\\')) {
			char *deq_path = dequote_str(new_path, 0);

			if (deq_path) {
				strcpy(new_path, deq_path);
				free(deq_path);
			}
		}

		char *real_path = realpath(new_path, NULL);

		if (!real_path) {
			fprintf(stderr, "%s: cd: %s: %s\n", PROGRAM_NAME,
					new_path, strerror(errno));
			return EXIT_FAILURE;
		}

		if (xchdir(real_path, SET_TITLE) != EXIT_SUCCESS) {
			fprintf(stderr, "%s: cd: %s: %s\n", PROGRAM_NAME,
					real_path, strerror(errno));
			free(real_path);
			return EXIT_FAILURE;
		}

		free(ws[cur_ws].path);
		ws[cur_ws].path = savestring(real_path, strlen(real_path));

		free(real_path);
	}

	int exit_status = EXIT_SUCCESS;

	if (cd_lists_on_the_fly) {
		free_dirlist();

		if (list_dir() != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;
	}

	add_to_dirhist(ws[cur_ws].path);
	add_to_jumpdb(ws[cur_ws].path);

	return exit_status;
}

static int
open_function(char **cmd)
{
	if (!cmd)
		return EXIT_FAILURE;

	if (strchr(cmd[1], '\\')) {
		char *deq_path = (char *)NULL;
		deq_path = dequote_str(cmd[1], 0);

		if (!deq_path) {
			fprintf(stderr, _("%s: %s: Error dequoting filename\n"),
					PROGRAM_NAME, cmd[1]);
			return EXIT_FAILURE;
		}

		strcpy(cmd[1], deq_path);
		free(deq_path);
	}

	char *file = cmd[1];

	/* Check file existence */
	struct stat file_attrib;

	if (stat(file, &file_attrib) == -1) {
		fprintf(stderr, "%s: open: %s: %s\n", PROGRAM_NAME, cmd[1],
				strerror(errno));
		return EXIT_FAILURE;
	}

	/* Check file type: only directories, symlinks, and regular files
	 * will be opened */

	char no_open_file = 1, file_type[128];
		 /* Reserve a good amount of bytes for filetype: it cannot be
		  * known beforehand how many bytes the TRANSLATED string will
		  * need */

	switch((file_attrib.st_mode & S_IFMT)) {
	case S_IFBLK:
		/* Store filetype to compose and print the error message, if
		 * necessary */
		strcpy(file_type, _("block device"));
		break;

	case S_IFCHR:
		strcpy(file_type, _("character device"));
		break;

	case S_IFSOCK:
		strcpy(file_type, _("socket"));
		break;

	case S_IFIFO:
		strcpy(file_type, _("FIFO/pipe"));
		break;

	case S_IFDIR:
		return cd_function(file);

	case S_IFREG:

		/* If an archive/compressed file, call archiver() */
		if (is_compressed(file, 1) == 0) {
			char *tmp_cmd[] = { "ad", file, NULL };
			return archiver(tmp_cmd, 'd');
		}

		no_open_file = 0;
		break;

	default:
		strcpy(file_type, _("unknown file type"));
		break;
	}

	/* If neither directory nor regular file nor symlink (to directory
	 * or regular file), print the corresponding error message and
	 * exit */
	if (no_open_file) {
		fprintf(stderr, _("%s: %s (%s): Cannot open file. Try "
				"'APPLICATION FILENAME'.\n"), PROGRAM_NAME,
				cmd[1], file_type);
		return EXIT_FAILURE;
	}

	/* At this point we know the file to be openend is either a regular
	 * file or a symlink to a regular file. So, just open the file */

	if (!cmd[2] || (*cmd[2] == '&' && !cmd[2][1] )) {

		if (opener) {
			char *tmp_cmd[] = { opener, file, NULL };

			int ret = launch_execve(tmp_cmd,
						strcmp(cmd[args_n], "&") == 0 ? BACKGROUND
						: FOREGROUND, E_NOSTDERR);

			if (ret != EXIT_SUCCESS)
				return EXIT_FAILURE;

			return EXIT_SUCCESS;
		}

		else if (!(flags & FILE_CMD_OK)) {
			fprintf(stderr, _("%s: file: Command not found. Specify an "
							  "application to open the file\nUsage: "
							  "open ELN/FILENAME [APPLICATION]\n"), 
							  PROGRAM_NAME);
			return EXIT_FAILURE;
		}

		else {
			int ret = mime_open(cmd);

			/* The return value of mime_open could be zero
			 * (EXIT_SUCCESS), if success, one (EXIT_FAILURE) if error
			 * (in which case the following error message should be
			 * printed), and -1 if no access permission, in which case
			 * no error message should be printed, since the
			 * corresponding message is printed by mime_open itself */
			if (ret == EXIT_FAILURE) {
				fputs("Add a new entry to the mimelist file ('mime "
					  "edit' or F6) or run 'open FILE APPLICATION'\n",
					  stderr);
				return EXIT_FAILURE;
			}

			return EXIT_SUCCESS;
		}
	}

	/* If some application was specified to open the file */
	char *tmp_cmd[] = { cmd[2], file, NULL };

	int ret = launch_execve(tmp_cmd, (cmd[args_n]
							&& strcmp(cmd[args_n], "&") == 0)
							? BACKGROUND : FOREGROUND, E_NOSTDERR);

	if (ret != EXIT_SUCCESS)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

static int
run_action(char *action, char **args)
/* The core of this function was taken from NNN's run_selected_plugin
 * function and modified to fit our needs. Thanks NNN! */
{
	int exit_status = EXIT_SUCCESS;
	char *cmd = (char *)NULL;
	size_t len = 0, action_len = strlen(action);

		/* #####################################
		 * #    1) CREATE CMD TO BE EXECUTED   #
		 * ##################################### */

	/* Remove terminating new line char */
	if (action[action_len -1] == '\n')
		action[action_len -1] = '\0';

	if (strchr(action, '/')) {
		len = action_len;
		cmd = savestring(action, len);
	}

	/* If not a path, PLUGINS_DIR is assumed */
	else {
		len = action_len + strlen(PLUGINS_DIR) + 2;
		cmd = (char *)xnmalloc(len, sizeof(char));
		sprintf(cmd, "%s/%s", PLUGINS_DIR, action);
	}

	/* Check if the action file exists and is executable */
	if (access(cmd, F_OK|X_OK) == -1) {
		fprintf(stderr, "actions: %s: %s\n", cmd, strerror(errno));
		free(cmd);
		return EXIT_FAILURE;
	}

	/* Append arguments to command */
	size_t i;
	for (i = 1; args[i]; i++) {
		len += strlen(args[i]) + 2;
		cmd = (char *)xrealloc(cmd, len * sizeof(char));
		strcat(cmd, " ");
		strcat(cmd, args[i]);
	}

			/* ##############################
			 * #    2) CREATE A PIPE FILE   #
			 * ############################## */

	char *rand_ext = gen_rand_str(6);

	if (!rand_ext) {
		free(cmd);
		return EXIT_FAILURE;
	}

	char fifo_path[PATH_MAX];
	sprintf(fifo_path, "%s/.pipe.%s", TMP_DIR, rand_ext);
	free(rand_ext);

	setenv("CLIFM_BUS", fifo_path, 1);

	if (mkfifo(fifo_path, 0600) != EXIT_SUCCESS) {
		free(cmd);
		printf("%s: %s\n", fifo_path, strerror(errno));
		return EXIT_FAILURE;
	}

	/* ################################################
	 * #   3) EXEC CMD & LET THE CHILD WRITE TO PIPE  #
	 * ################################################ */

	if (fork() == EXIT_SUCCESS) {

		/* Child: write-only end of the pipe */
		int wfd = open(fifo_path, O_WRONLY | O_CLOEXEC);

		if (wfd == -1)
			_exit(EXIT_FAILURE);

		launch_execle(cmd);

		close(wfd);
		_exit(EXIT_SUCCESS);
	}

	free(cmd);

		/* ########################################
		 * #    4) LET THE PARENT READ THE PIPE   #
		 * ######################################## */

	/* Parent: read-only end of the pipe */
	int rfd;

	do
		rfd = open(fifo_path, O_RDONLY);
	while (rfd == -1 && errno == EINTR);

	char buf[PATH_MAX] = "";
	ssize_t buf_len = 0;

	do
		buf_len = read(rfd, buf, sizeof(buf));
	while (buf_len == -1 && errno == EINTR);

	close(rfd);

	/* If the pipe is empty */
	if (!*buf) {
		unlink(fifo_path);
		return EXIT_SUCCESS;
	}

	if (buf[buf_len - 1] == '\n')
		buf[buf_len - 1] = '\0';

	/* If a valid file */
	struct stat attr;

	if (lstat(buf, &attr) != -1) {
		char *o_cmd[] = { "o", buf, NULL };
		exit_status = open_function(o_cmd);
	}

	/* If not a file, take it as a command*/
	else {
		size_t old_args = args_n;
		args_n = 0;

		char **_cmd = parse_input_str(buf);

		if (_cmd) {

			char **alias_cmd = check_for_alias(_cmd);

			if (alias_cmd) {
				exit_status = exec_cmd(alias_cmd);

				for (i = 0; alias_cmd[i]; i++)
					free(alias_cmd[i]);

				free(alias_cmd);
			}

			else {
				exit_status = exec_cmd(_cmd);

				for (i = 0; i <= args_n; i++)
					free(_cmd[i]);

				free(_cmd);
			}
		}

		args_n = old_args;
	}

	/* Remove the pipe file */
	unlink(fifo_path);

	return exit_status;
}

static int
surf_hist(char **comm)
{
	if (*comm[1] == 'h' && (strcmp(comm[1], "h") == 0
	|| strcmp(comm[1], "hist") == 0)) {
		/* Show the list of already visited directories */
		int i;
		for (i = 0; i < dirhist_total_index; i++) {
			if (i == dirhist_cur_index)
				printf("%d %s%s%s\n", i + 1, dh_c,
					   old_pwd[i], df_c);

			else
				printf("%d %s\n", i + 1, old_pwd[i]);
		}

		return EXIT_SUCCESS;
	}

	if (*comm[1] == 'c' && strcmp(comm[1], "clear") == 0) {
		int i = dirhist_total_index;
		while (--i >= 0)
			free(old_pwd[i]);

		dirhist_cur_index = dirhist_total_index = 0;
		add_to_dirhist(ws[cur_ws].path);

		return EXIT_SUCCESS;
	}

	int exit_status = EXIT_FAILURE;

	if (*comm[1] == '!' && is_number(comm[1] + 1) == 1) {
		/* Go the the specified directory (first arg) in the directory
		 * history list */
		int atoi_comm = atoi(comm[1] + 1);
		if (atoi_comm > 0 && atoi_comm <= dirhist_total_index) {

			int ret = xchdir(old_pwd[atoi_comm - 1], SET_TITLE);

			if (ret == 0) {
				free(ws[cur_ws].path);
				ws[cur_ws].path = (char *)xnmalloc(strlen(
										old_pwd[atoi_comm - 1])
										+ 1, sizeof(char));
				strcpy(ws[cur_ws].path, old_pwd[atoi_comm - 1]);

				dirhist_cur_index = atoi_comm - 1;

				exit_status = EXIT_SUCCESS;

				if (cd_lists_on_the_fly) {
					free_dirlist();
					exit_status = list_dir();
				}
			}

			else
				fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME,
						old_pwd[atoi_comm - 1], strerror(errno));
		}
		else
			fprintf(stderr, _("history: %d: No such ELN\n"),
					atoi(comm[1] + 1));
	}
	else
		fputs(_("history: Usage: b/f [hist] [clear] [!ELN]\n"), stderr);

	return exit_status;
}

static int
back_function(char **comm)
/* Go back one entry in dirhist */
{
	if (!comm)
		return EXIT_FAILURE;

	if (comm[1]) {
		if (*comm[1] == '-' && strcmp(comm[1], "--help") == 0) {
			puts(_("Usage: back, b [h, hist] [clear] [!ELN]"));
			return EXIT_SUCCESS;
		}

		return surf_hist(comm);
	}

	/* If just 'back', with no arguments */

	/* If first path in current dirhist was reached, do nothing */
	if (dirhist_cur_index <= 0)
		return EXIT_SUCCESS;

	int exit_status = EXIT_FAILURE;

	dirhist_cur_index--;

	if (xchdir(old_pwd[dirhist_cur_index], SET_TITLE) == EXIT_SUCCESS) {

		free(ws[cur_ws].path);
		ws[cur_ws].path = savestring(old_pwd[dirhist_cur_index],
							strlen(old_pwd[dirhist_cur_index]));

		exit_status = EXIT_SUCCESS;

		add_to_jumpdb(ws[cur_ws].path);

		if (cd_lists_on_the_fly) {
			free_dirlist();
			exit_status = list_dir();
		}
	}

	else
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME,
				old_pwd[dirhist_cur_index], strerror(errno));

	return exit_status;
}

static int
forth_function(char **comm)
/* Go forth one entry in dirhist */
{
	if (!comm)
		return EXIT_FAILURE;

	if (comm[1]) {
		if (*comm[1] == '-' && strcmp(comm[1], "--help") == 0) {
			puts(_("Usage: forth, f [h, hist] [clear] [!ELN]"));
			return EXIT_SUCCESS;
		}

		return surf_hist(comm);
	}

	/* If just 'forth', with no arguments */

	/* If last path in dirhist was reached, do nothing */
	if (dirhist_cur_index + 1 >= dirhist_total_index)
		return EXIT_SUCCESS;

	dirhist_cur_index++;

	int exit_status = EXIT_FAILURE;
	if (xchdir(old_pwd[dirhist_cur_index], SET_TITLE) == EXIT_SUCCESS) {

		free(ws[cur_ws].path);
		ws[cur_ws].path = savestring(old_pwd[dirhist_cur_index],
							strlen(old_pwd[dirhist_cur_index]));

		add_to_jumpdb(ws[cur_ws].path);

		exit_status = EXIT_SUCCESS;

		if (cd_lists_on_the_fly) {
			free_dirlist();
			exit_status = list_dir();
		}
	}

	else
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME,
				old_pwd[dirhist_cur_index], strerror(errno));

	return exit_status;
}

static int
list_mountpoints(void)
/* List available mountpoints and chdir into one of them */
{
	FILE *mp_fp = fopen("/proc/mounts", "r");

	if (!mp_fp) {
		fprintf(stderr, "%s: mp: fopen: /proc/mounts: %s\n",
				PROGRAM_NAME, strerror(errno));
		return EXIT_FAILURE;
	}

	printf(_("%sMountpoints%s\n\n"), bold, df_c);

	char **mountpoints = (char **)NULL;
	size_t mp_n = 0;
	int exit_status = EXIT_SUCCESS;

	size_t line_size = 0;
	char *line = (char *)NULL;
	ssize_t line_len = 0;

	while ((line_len = getline(&line, &line_size, mp_fp)) > 0) {

		/* Do not list all mountpoints, but only those corresponding
		 * to a block device (/dev) */
		if (strncmp(line, "/dev/", 5) == 0) {
			char *str = (char *)NULL;
			size_t counter = 0;

			/* use strtok() to split LINE into tokens using space as
			 * IFS */
			str = strtok(line, " ");
			size_t dev_len = strlen(str);

			char *device = savestring(str, dev_len);
			/* Print only the first two fileds of each /proc/mounts
			 * line */
			while (str && counter < 2) {

				if (counter == 1) { /* 1 == second field */
					printf("%s%zu%s %s%s%s (%s)\n", el_c, mp_n + 1,
						   df_c, (access(str, R_OK|X_OK) == 0)
						   ? di_c : nd_c, str, df_c,
						   device);
					/* Store the second field (mountpoint) into an
					 * array */
					mountpoints = (char **)xrealloc(mountpoints,
										   (mp_n + 1) * sizeof(char *));
					mountpoints[mp_n++] = savestring(str, strlen(str));
				}

				str = strtok(NULL, " ,");
				counter++;
			}

			free(device);
		}
	}

	free(line);
	line = (char *)NULL;
	fclose(mp_fp);

	/* This should never happen: There should always be a mountpoint,
	 * at least "/" */
	if (mp_n <= 0) {
		fputs(_("mp: There are no available mountpoints\n"), stdout);
		return EXIT_SUCCESS;
	}

	putchar('\n');
	/* Ask the user and chdir into the selected mountpoint */
	char *input = (char *)NULL;

	while (!input)
		input = rl_no_hist(_("Choose a mountpoint ('q' to quit): "));

	if (!(*input == 'q' && *(input + 1) == '\0')) {
		int atoi_num = atoi(input);

		if (atoi_num > 0 && atoi_num <= (int)mp_n) {
			
			if (xchdir(mountpoints[atoi_num - 1], SET_TITLE) == EXIT_SUCCESS) {

				free(ws[cur_ws].path);
				ws[cur_ws].path = savestring(mountpoints[atoi_num - 1],
									strlen(mountpoints[atoi_num - 1]));

				if (cd_lists_on_the_fly) {
					free_dirlist();

					if (list_dir() != EXIT_SUCCESS)
						exit_status =  EXIT_FAILURE;
				}

				add_to_dirhist(ws[cur_ws].path);
				add_to_jumpdb(ws[cur_ws].path);
			}

			else {
				fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME,
						mountpoints[atoi_num - 1], strerror(errno));
				exit_status = EXIT_FAILURE;
			}
		}
		else {
			fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME,
					mountpoints[atoi_num - 1], strerror(errno));
			exit_status = EXIT_FAILURE;
		}
	}

	/* Free stuff and exit */
	if (input)
		free(input);

	int i = (int)mp_n;
	while (--i >= 0)
		free(mountpoints[i]);

	free(mountpoints);

	return exit_status;
}

static int *
expand_range(char *str, int listdir)
/* Expand a range of numbers given by str. It will expand the range
 * provided that both extremes are numbers, bigger than zero, equal or
 * smaller than the amount of files currently listed on the screen, and
 * the second (right) extreme is bigger than the first (left). Returns
 * an array of int's with the expanded range or NULL if one of the
 * above conditions is not met */
{
	if (strcntchr(str, '-') == -1)
		return (int *)NULL;

	char *first = (char *)NULL;
	first = strbfr(str, '-');

	if (!first)
		return (int *)NULL;

	if (!is_number(first)) {
		free(first);
		return (int *)NULL;
	}

	char *second = (char *)NULL;
	second = straft(str, '-');

	if (!second) {
		free(first);
		return (int *)NULL;
	}

	if (!is_number(second)) {
		free(first);
		free(second);
		return (int *)NULL;
	}

	int afirst = atoi(first), asecond = atoi(second);
	free(first);
	free(second);

	if (listdir) {

		if (afirst <= 0 || afirst > (int)files || asecond <= 0
		|| asecond > (int)files || afirst >= asecond)
			return (int *)NULL;
	}

	else {
		if (afirst >= asecond)
			return (int *)NULL;
	}

	int *buf = (int *)NULL;
	buf = (int *)xcalloc((size_t)(asecond - afirst) + 2, sizeof(int));

	size_t i, j = 0;

	for (i = (size_t)afirst; i <= (size_t)asecond; i++)
		buf[j++] = (int)i;

	return buf;
}

static int
recur_perm_check(const char *dirname)
/* Recursively check directory permissions (write and execute). Returns
 * zero if OK, and one if at least one subdirectory does not have
 * write/execute permissions */
{
	DIR *dir;
	struct dirent *ent;

	if (!(dir = opendir(dirname)))
		return EXIT_FAILURE;

	while ((ent = readdir(dir)) != NULL) {

		if (ent->d_type == DT_DIR) {
			char dirpath[PATH_MAX] = "";

			if (*ent->d_name == '.' && (!ent->d_name[1]
			|| (ent->d_name[1] == '.' && !ent->d_name[2])))
				continue;

			snprintf(dirpath, PATH_MAX, "%s/%s", dirname, ent->d_name);

			if (access(dirpath, W_OK|X_OK) != 0) {
				/* recur_perm_error_flag needs to be a global variable.
				  * Otherwise, since this function calls itself
				  * recursivelly, the flag would be reset upon every
				  * new call, without preserving the error code, which
				  * is what the flag is aimed to do. On the other side,
				  * if I use a local static variable for this flag, it
				  * will never drop the error value, and all subsequent
				  * calls to the function will allways return error
				  * (even if there's no actual error) */
				recur_perm_error_flag = 1;
				fprintf(stderr, _("%s: Permission denied\n"), dirpath);
			}

			recur_perm_check(dirpath);
		}
	}

	closedir(dir);

	if (recur_perm_error_flag)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

static int
wx_parent_check(char *file)
/* Check whether the current user has enough permissions (write, execute)
 * to modify the contents of the parent directory of 'file'. 'file' needs
 * to be an absolute path. Returns zero if yes and one if no. Useful to
 * know if a file can be removed from or copied into the parent. In case
 * FILE is a directory, the function checks all its subdirectories for
 * appropriate permissions, including the immutable bit */
{
	struct stat file_attrib;
	int exit_status = -1, ret = -1;
	size_t file_len = strlen(file);

	if (file[file_len - 1] == '/')
		file[file_len - 1] = '\0';

	if (lstat(file, &file_attrib) == -1) {
		fprintf(stderr, _("%s: No such file or directory\n"), file);
		return EXIT_FAILURE;
	}

	char *parent = strbfrlst(file, '/');
	if (!parent) {
		/* strbfrlst() will return NULL if file's parent is root (/),
		 * simply because in this case there's nothing before the last
		 * slash. So, check if file's parent dir is root */
		if (file[0] == '/' && strcntchr(file + 1, '/') == -1) {
			parent = (char *)xnmalloc(2, sizeof(char));
			parent[0] = '/';
			parent[1] = '\0';
		}

		else {
			fprintf(stderr, _("%s: %s: Error getting parent "
					"directory\n"), PROGRAM_NAME, file);
			return EXIT_FAILURE;
		}
	}

	switch (file_attrib.st_mode & S_IFMT) {

	/* DIRECTORY */
	case S_IFDIR:
		ret = check_immutable_bit(file);

		if (ret == -1) {
			/* Error message is printed by check_immutable_bit() itself */
			exit_status = EXIT_FAILURE;
		}

		else if (ret == 1) {
			fprintf(stderr, _("%s: Directory is immutable\n"), file);
			exit_status = EXIT_FAILURE;
		}

		/* Check the parent for appropriate permissions */
		else if (access(parent, W_OK|X_OK) == 0) {
			int files_n = count_dir(parent);

			if (files_n > 2) {
				/* I manually check here subdir because recur_perm_check()
				 * will only check the contents of subdir, but not subdir
				 * itself */
				/* If the parent is ok and not empty, check subdir */
				if (access(file, W_OK|X_OK) == 0) {
					/* If subdir is ok and not empty, recusivelly check
					 * subdir */
					files_n = count_dir(file);

					if (files_n > 2) {
						/* Reset the recur_perm_check() error flag. See
						 * the note in the function block. */
						recur_perm_error_flag = 0;

						if (recur_perm_check(file) == 0) {
							exit_status = EXIT_SUCCESS;
						}

						else
							/* recur_perm_check itself will print the
							 * error messages */
							exit_status = EXIT_FAILURE;
					}

					else /* Subdir is ok and empty */
						exit_status = EXIT_SUCCESS;
				}

				else { /* No permission for subdir */
					fprintf(stderr, _("%s: Permission denied\n"),
							file);
					exit_status = EXIT_FAILURE;
				}
			}

			else
				exit_status = EXIT_SUCCESS;
		}

		else { /* No permission for parent */
			fprintf(stderr, _("%s: Permission denied\n"), parent);
			exit_status = EXIT_FAILURE;
		}
		break;

	/* REGULAR FILE */
	case S_IFREG:
		ret = check_immutable_bit(file);

		if (ret == -1) {
			/* Error message is printed by check_immutable_bit()
			 * itself */
			exit_status = EXIT_FAILURE;
		}

		else if (ret == 1) {
			fprintf(stderr, _("%s: File is immutable\n"), file);
			exit_status = EXIT_FAILURE;
		}

		else if (parent) {

			if (access(parent, W_OK|X_OK) == 0)
				exit_status = EXIT_SUCCESS;

			else {
				fprintf(stderr, _("%s: Permission denied\n"), parent);
				exit_status = EXIT_FAILURE;
			}
		}

		break;

	/* SYMLINK, SOCKET, AND FIFO PIPE */
	case S_IFSOCK:
	case S_IFIFO:
	case S_IFLNK:
		/* Symlinks, sockets and pipes do not support immutable bit */
		if (parent) {

			if (access(parent, W_OK|X_OK) == 0)
				exit_status = EXIT_SUCCESS;

			else {
				fprintf(stderr, _("%s: Permission denied\n"), parent);
				exit_status = EXIT_FAILURE;
			}
		}
		break;

	/* DO NOT TRASH BLOCK AND CHAR DEVICES */
	default:
		fprintf(stderr, _("%s: trash: %s (%s): Unsupported file type\n"),
				PROGRAM_NAME, file, ((file_attrib.st_mode & S_IFMT) == S_IFBLK)
				? "Block device" : ((file_attrib.st_mode & S_IFMT) == S_IFCHR)
				? "Character device" : "Unknown filetype");
		exit_status = EXIT_FAILURE;
		break;
	}

	if (parent)
		free(parent);

	return exit_status;
}

static int
trash_element(const char *suffix, struct tm *tm, char *file)
{
	/* Check file's existence */
	struct stat file_attrib;

	if (lstat(file, &file_attrib) == -1) {
		fprintf(stderr, "%s: trash: %s: %s\n", PROGRAM_NAME, file,
				strerror(errno));
		return EXIT_FAILURE;
	}

	/* Check whether the user has enough permissions to remove file */
	/* If relative path */
	char full_path[PATH_MAX] = "";

	if (*file != '/') {
		/* Construct absolute path for file */
		snprintf(full_path, PATH_MAX, "%s/%s", ws[cur_ws].path, file);
		if (wx_parent_check(full_path) != 0)
			return EXIT_FAILURE;
	}

	/* If absolute path */
	else if (wx_parent_check(file) != 0)
		return EXIT_FAILURE;

	int ret = -1;

	/* Create the trashed file name: orig_filename.suffix, where SUFFIX is
	 * current date and time */
	char *filename = (char *)NULL;

	if (*file != '/') /* If relative path */
		filename = straftlst(full_path, '/');
	else /* If absolute path */
		filename = straftlst(file, '/');

	if (!filename) {
		fprintf(stderr, _("%s: trash: %s: Error getting filename\n"),
				PROGRAM_NAME, file);
		return EXIT_FAILURE;
	}
	/* If the length of the trashed file name (orig_filename.suffix) is
	 * longer than NAME_MAX (255), trim the original filename, so that
	 * (original_filename_len + 1 (dot) + suffix_len) won't be longer
	 * than NAME_MAX */
	size_t filename_len = strlen(filename), suffix_len = strlen(suffix);
	int size = (int)(filename_len + suffix_len + 1) - NAME_MAX;

	if (size > 0) {
		/* If SIZE is a positive value, that is, the trashed file name
		 * exceeds NAME_MAX by SIZE bytes, reduce the original file name
		 * SIZE bytes. Terminate the original file name (FILENAME) with
		 * a tilde (~), to let the user know it is trimmed */
		filename[filename_len - (size_t)size - 1] = '~';
		filename[filename_len - (size_t)size] = '\0';
	}

											/* 2 = dot + null byte */
	size_t file_suffix_len = filename_len + suffix_len + 2;
	char *file_suffix = (char *)xnmalloc(file_suffix_len, sizeof(char));
	/* No need for memset. sprintf adds the terminating null byte by
	 * itself */
	sprintf(file_suffix, "%s.%s", filename, suffix);

	/* Copy the original file into the trash files directory */
	char *dest = (char *)NULL;
	dest = (char *)xnmalloc(strlen(TRASH_FILES_DIR)
							+ strlen(file_suffix) + 2, sizeof(char));
	sprintf(dest, "%s/%s", TRASH_FILES_DIR, file_suffix);

	char *tmp_cmd[] = { "cp", "-a", file, dest, NULL };

	free(filename);

	ret = launch_execve(tmp_cmd, FOREGROUND, E_NOFLAG);
	free(dest);
	dest = (char *)NULL;

	if (ret != EXIT_SUCCESS) {
		fprintf(stderr, _("%s: trash: %s: Failed copying file to "
				"Trash\n"), PROGRAM_NAME, file);
		free(file_suffix);
		return EXIT_FAILURE;
	}

	/* Generate the info file */
	size_t info_file_len = strlen(TRASH_INFO_DIR) + strlen(file_suffix) + 12;

	char *info_file = (char *)xnmalloc(info_file_len, sizeof(char));
	sprintf(info_file, "%s/%s.trashinfo", TRASH_INFO_DIR, file_suffix);

	FILE *info_fp = fopen(info_file, "w");

	if (!info_fp) { /* If error creating the info file */
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, info_file,
				strerror(errno));
		/* Remove the trash file */
		char *trash_file = (char *)NULL;
		trash_file = (char *)xnmalloc(strlen(TRASH_FILES_DIR)
									  + strlen(file_suffix) + 2, sizeof(char));
		sprintf(trash_file, "%s/%s", TRASH_FILES_DIR, file_suffix);

		char *tmp_cmd2[] = { "rm", "-r", trash_file, NULL };

		ret = launch_execve (tmp_cmd2, FOREGROUND, E_NOFLAG);

		free(trash_file);

		if (ret != EXIT_SUCCESS)
			fprintf(stderr, _("%s: trash: %s/%s: Failed removing trash "
					"file\nTry removing it manually\n"), PROGRAM_NAME,
					TRASH_FILES_DIR, file_suffix);

		free(file_suffix);
		free(info_file);

		return EXIT_FAILURE;
	}

	else { /* If info file was generated successfully */
		/* Encode path to URL format (RF 2396) */
		char *url_str = (char *)NULL;

		if (*file != '/')
			url_str = url_encode(full_path);
		else
			url_str = url_encode(file);

		if (!url_str) {
			fprintf(stderr, _("%s: trash: %s: Failed encoding path\n"),
					PROGRAM_NAME, file);
			fclose(info_fp);

			free(info_file);
			free(file_suffix);
			return EXIT_FAILURE;
		}

		/* Write trashed file information into the info file */
		fprintf(info_fp,
				"[Trash Info]\nPath=%s\nDeletionDate=%d%d%dT%d:%d:%d\n",
				url_str, tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
				tm->tm_hour, tm->tm_min, tm->tm_sec);
		fclose(info_fp);
		free(url_str);
		url_str = (char *)NULL;
	}

	/* Remove the file to be trashed */
	char *tmp_cmd3[] = { "rm", "-r", file, NULL };
	ret = launch_execve(tmp_cmd3, FOREGROUND, E_NOFLAG);

	/* If remove fails, remove trash and info files */
	if (ret != EXIT_SUCCESS) {
		fprintf(stderr, _("%s: trash: %s: Failed removing file\n"),
				PROGRAM_NAME, file);
		char *trash_file = (char *)NULL;
		trash_file = (char *)xnmalloc(strlen(TRASH_FILES_DIR)
									 + strlen(file_suffix) + 2, sizeof(char));
		sprintf(trash_file, "%s/%s", TRASH_FILES_DIR, file_suffix);

		char *tmp_cmd4[] = { "rm", "-r", trash_file, info_file, NULL };
		ret = launch_execve(tmp_cmd4, FOREGROUND, E_NOFLAG);
		free(trash_file);

		if (ret != EXIT_SUCCESS) {
			fprintf(stderr, _("%s: trash: Failed removing temporary "
							  "files from Trash.\nTry removing them "
							  "manually\n"), PROGRAM_NAME);
			free(file_suffix);
			free(info_file);
			return EXIT_FAILURE;
		}
	}

	free(info_file);
	free(file_suffix);
	return EXIT_SUCCESS;
}

static int
skip_files(const struct dirent *ent)
{
	/* In case a directory isn't reacheable, like a failed
	 * mountpoint... */
/*  struct stat file_attrib;

	if (lstat(entry->d_name, &file_attrib) == -1) {
		fprintf(stderr, _("stat: cannot access '%s': %s\n"),
				entry->d_name, strerror(errno));
		return 0;
	} */

	/* Skip "." and ".." */
	if (*ent->d_name == '.' && (!ent->d_name[1]
	|| (ent->d_name[1] == '.' && !ent->d_name[2])))
		return 0;

	/* Skip files matching FILTER */
	if (filter && regexec(&regex_exp, ent->d_name, 0, NULL, 0)
	== EXIT_SUCCESS)
		return 0;

	/* If not hidden files */
	if (!show_hidden && *ent->d_name == '.')
		return 0;

	return 1;
}

static int
remove_from_trash(void)
{
	/* List trashed files */
	/* Change CWD to the trash directory. Otherwise, scandir() will fail */
	if (xchdir(TRASH_FILES_DIR, NO_TITLE) == -1) {
		_err(0, NOPRINT_PROMPT, "%s: trash: '%s': %s\n", PROGRAM_NAME,
			 TRASH_FILES_DIR, strerror(errno));
		return EXIT_FAILURE;
	}

	size_t i = 0;
	struct dirent **trash_files = (struct dirent **)NULL;
	int files_n = scandir(TRASH_FILES_DIR, &trash_files,
						  skip_files, (unicode) ? alphasort
						  : (case_sensitive) ? xalphasort
						  : alphasort_insensitive);

	if (files_n) {
		printf(_("%sTrashed files%s\n\n"), bold, df_c);

		for (i = 0; i < (size_t)files_n; i++)
			colors_list(trash_files[i]->d_name, (int)i + 1, NO_PAD,
						PRINT_NEWLINE);
	}

	else {
		puts(_("trash: There are no trashed files"));

		/* Restore CWD and return */
		if (xchdir(ws[cur_ws].path, NO_TITLE) == -1) {
			_err(0, NOPRINT_PROMPT, "%s: trash: '%s': %s\n",
				 PROGRAM_NAME, ws[cur_ws].path, strerror(errno));
		}

		return EXIT_SUCCESS;
	}

	/* Restore CWD and continue */
	if (xchdir(ws[cur_ws].path, NO_TITLE) == -1) {
		_err(0, NOPRINT_PROMPT, "%s: trash: '%s': %s\n", PROGRAM_NAME,
			 ws[cur_ws].path, strerror(errno));
		return EXIT_FAILURE;
	}

	/* Get user input */
	printf(_("\n%sEnter 'q' to quit.\n"), df_c);
	char *line = (char *)NULL, **rm_elements = (char **)NULL;

	while (!line)
		line = rl_no_hist(_("File(s) to be removed (ex: 1 2-6, or *): "));

	rm_elements = get_substr(line, ' ');
	free(line);

	if (!rm_elements)
		return EXIT_FAILURE;

	/* Remove files */
	char rm_file[PATH_MAX] = "", rm_info[PATH_MAX] = "";
	int ret = -1, exit_status = EXIT_SUCCESS;

	/* First check for exit, wildcard, and non-number args */
	for (i = 0; rm_elements[i]; i++) {

		/* Quit */
		if (strcmp(rm_elements[i], "q") == 0) {
			size_t j;

			for (j = 0; rm_elements[j]; j++)
				free(rm_elements[j]);

			free(rm_elements);

			for (j = 0; j < (size_t)files_n; j++)
				free(trash_files[j]);

			free(trash_files);

			return exit_status;
		}

		/* Asterisk */
		else if (strcmp(rm_elements[i], "*") == 0) {
			size_t j;
			for (j = 0; j < (size_t)files_n; j++) {

				snprintf(rm_file, PATH_MAX, "%s/%s", TRASH_FILES_DIR,
						 trash_files[j]->d_name);
				snprintf(rm_info, PATH_MAX, "%s/%s.trashinfo",
						 TRASH_INFO_DIR, trash_files[j]->d_name);

				char *tmp_cmd[] = { "rm", "-r", rm_file, rm_info, NULL };

				ret = launch_execve(tmp_cmd, FOREGROUND, E_NOFLAG);

				if (ret != EXIT_SUCCESS) {
					fprintf(stderr, _("%s: trash: Error trashing %s\n"),
							 PROGRAM_NAME, trash_files[j]->d_name);
					exit_status = EXIT_FAILURE;
				}

				free(trash_files[j]);
			}

			free(trash_files);

			for (j = 0; rm_elements[j]; j++)
				free(rm_elements[j]);

			free(rm_elements);

			return exit_status;
		}

		else if (!is_number(rm_elements[i])) {

			fprintf(stderr, _("%s: trash: %s: Invalid ELN\n"),
					PROGRAM_NAME, rm_elements[i]);
			exit_status = EXIT_FAILURE;

			size_t j;

			for (j = 0; rm_elements[j]; j++)
				free(rm_elements[j]);

			free(rm_elements);

			for (j = 0; j < (size_t)files_n; j++)
				free(trash_files[j]);

			free(trash_files);

			return exit_status;
		}
	}

	/* If all args are numbers, and neither 'q' nor wildcard */
	int rm_num;
	for (i = 0; rm_elements[i]; i++) {

		rm_num = atoi(rm_elements[i]);

		if (rm_num <= 0 || rm_num > files_n) {
			fprintf(stderr, _("%s: trash: %d: Invalid ELN\n"),
					PROGRAM_NAME, rm_num);
			free(rm_elements[i]);
			exit_status = EXIT_FAILURE;
			continue;
		}

		snprintf(rm_file, PATH_MAX, "%s/%s", TRASH_FILES_DIR,
				 trash_files[rm_num - 1]->d_name);
		snprintf(rm_info, PATH_MAX, "%s/%s.trashinfo", TRASH_INFO_DIR,
				 trash_files[rm_num - 1]->d_name);

		char *tmp_cmd2[] = { "rm", "-r", rm_file, rm_info, NULL };

		ret = launch_execve(tmp_cmd2, FOREGROUND, E_NOFLAG);

		if (ret != EXIT_SUCCESS) {
			fprintf(stderr, _("%s: trash: Error trashing %s\n"),
					PROGRAM_NAME, trash_files[rm_num - 1]->d_name);
			exit_status = EXIT_FAILURE;
		}

		free(rm_elements[i]);
	}

	free(rm_elements);

	for (i = 0; i < (size_t)files_n; i++)
		free(trash_files[i]);

	free(trash_files);

	return exit_status;
}

static int
untrash_element(char *file)
{
	if (!file)
		return EXIT_FAILURE;

	char undel_file[PATH_MAX] = "", undel_info[PATH_MAX] = "";
	snprintf(undel_file, PATH_MAX, "%s/%s", TRASH_FILES_DIR, file);
	snprintf(undel_info, PATH_MAX, "%s/%s.trashinfo", TRASH_INFO_DIR,
			 file);

	FILE *info_fp;
	info_fp = fopen(undel_info, "r");

	if (info_fp) {

		char *orig_path = (char *)NULL;
		/* The max length for line is Path=(5) + PATH_MAX + \n(1) */
		char line[PATH_MAX + 6];

		memset(line, '\0', PATH_MAX + 6);

		while (fgets(line, (int)sizeof(line), info_fp)) {
			if (strncmp(line, "Path=", 5) == 0)
				orig_path = straft(line, '=');
		}

		fclose (info_fp);

		/* If original path is NULL or empty, return error */
		if (!orig_path)
			return EXIT_FAILURE;

/*      if (strcmp(orig_path, "") == 0) { */
		if (*orig_path == '\0') {
			free(orig_path);
			return EXIT_FAILURE;
		}

		/* Remove new line char from original path, if any */
		size_t orig_path_len = strlen(orig_path);
		if (orig_path[orig_path_len - 1] == '\n')
			orig_path[orig_path_len - 1] = '\0';

		/* Decode original path's URL format */
		char *url_decoded = url_decode(orig_path);

		if (!url_decoded) {
			fprintf(stderr, _("%s: undel: %s: Failed decoding path\n"),
					PROGRAM_NAME, orig_path);
			free(orig_path);
			return EXIT_FAILURE;
		}

		free(orig_path);
		orig_path = (char *)NULL;

		/* Check existence and permissions of parent directory */
		char *parent = (char *)NULL;
		parent = strbfrlst(url_decoded, '/');

		if (!parent) {
			/* strbfrlst() returns NULL is file's parent is root (simply
			 * because there's nothing before last slash in this case).
			 * So, check if file's parent is root. Else returns */

			if (url_decoded[0] == '/'
			&& strcntchr(url_decoded + 1, '/') == -1) {
				parent = (char *)xnmalloc(2, sizeof(char));
				parent[0] = '/';
				parent[1] = '\0';
			}

			else {
				free(url_decoded);
				return EXIT_FAILURE;
			}
		}

		if (access(parent, F_OK) != 0) {
			fprintf(stderr, _("%s: undel: %s: No such file or "
					"directory\n"), PROGRAM_NAME, parent);
			free(parent);
			free(url_decoded);
			return EXIT_FAILURE;
		}

		if (access(parent, X_OK|W_OK) != 0) {
			fprintf(stderr, _("%s: undel: %s: Permission denied\n"), 
					PROGRAM_NAME, parent);
			free(parent);
			free(url_decoded);
			return EXIT_FAILURE;
		}

		free(parent);

		char *tmp_cmd[] = { "cp", "-a", undel_file, url_decoded, NULL };

		int ret = -1;
		ret = launch_execve(tmp_cmd, FOREGROUND, E_NOFLAG);
		free(url_decoded);

		if (ret == EXIT_SUCCESS) {
			char *tmp_cmd2[] = { "rm", "-r", undel_file, undel_info, NULL };
			ret = launch_execve(tmp_cmd2, FOREGROUND, E_NOFLAG);

			if (ret != EXIT_SUCCESS) {
				fprintf(stderr, _("%s: undel: %s: Failed removing "
								  "info file\n"), PROGRAM_NAME, undel_info);
				return EXIT_FAILURE;
			}

			else
				return EXIT_SUCCESS;
		}

		else {
			fprintf(stderr, _("%s: undel: %s: Failed restoring trashed "
							  "file\n"), PROGRAM_NAME, undel_file);
			return EXIT_FAILURE;
		}
	}

	else { /* !info_fp */
		fprintf(stderr, _("%s: undel: Info file for '%s' not found. "
						  "Try restoring the file manually\n"),
						  PROGRAM_NAME, file);
		return EXIT_FAILURE;
	}

	return EXIT_FAILURE; /* Never reached */
}

static int
untrash_function(char **comm)
{
	if (xargs.stealth_mode == 1) {
		printf("%s: The trash function is disabled in stealth mode\n",
			   PROGRAM_NAME);
		return EXIT_SUCCESS;
	}

	if (!comm)
		return EXIT_FAILURE;

	if (!trash_ok) {
		fprintf(stderr, _("%s: Trash function disabled\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	/* Change CWD to the trash directory to make scandir() work */
	if (xchdir(TRASH_FILES_DIR, NO_TITLE) == -1) {
		_err(0, NOPRINT_PROMPT, "%s: undel: '%s': %s\n", PROGRAM_NAME,
			 TRASH_FILES_DIR, strerror(errno));
		return EXIT_FAILURE;
	}

	/* Get trashed files */
	struct dirent **trash_files = (struct dirent **)NULL;
	int trash_files_n = scandir(TRASH_FILES_DIR, &trash_files,
								skip_files, (unicode) ? alphasort
								: (case_sensitive) ? xalphasort
								: alphasort_insensitive);
	if (trash_files_n <= 0) {

		puts(_("trash: There are no trashed files"));

		if (xchdir(ws[cur_ws].path, NO_TITLE) == -1) {
			_err(0, NOPRINT_PROMPT, "%s: undel: '%s': %s\n",
				 PROGRAM_NAME, ws[cur_ws].path, strerror(errno));
			return EXIT_FAILURE;
		}

		return EXIT_SUCCESS;
	}

	int exit_status = EXIT_SUCCESS;
	/* if "undel all" (or "u a" or "u *") */
	if (comm[1] && (strcmp(comm[1], "*") == 0
	|| strcmp(comm[1], "a") == 0 || strcmp(comm[1], "all") == 0)) {
		size_t j;

		for (j = 0; j < (size_t)trash_files_n; j++) {

			if (untrash_element(trash_files[j]->d_name) != 0)
				exit_status = EXIT_FAILURE;

			free(trash_files[j]);
		}

		free(trash_files);

		if (xchdir(ws[cur_ws].path, NO_TITLE) == -1) {
			_err(0, NOPRINT_PROMPT, "%s: undel: '%s': %s\n",
				 PROGRAM_NAME, ws[cur_ws].path, strerror(errno));
			return EXIT_FAILURE;
		}

		return exit_status;
	}

	/* List trashed files */
	printf(_("%sTrashed files%s\n\n"), bold, df_c);
	size_t i;

	for (i = 0; i < (size_t)trash_files_n; i++)
		colors_list(trash_files[i]->d_name, (int)i + 1, NO_PAD,
					PRINT_NEWLINE);

	/* Go back to previous path */
	if (xchdir(ws[cur_ws].path, NO_TITLE) == -1) {
		_err(0, NOPRINT_PROMPT, "%s: undel: '%s': %s\n", PROGRAM_NAME,
			 ws[cur_ws].path, strerror(errno));
		return EXIT_FAILURE;
	}

	/* Get user input */
	printf(_("\n%sEnter 'q' to quit.\n"), df_c);
	int undel_n = 0;
	char *line = (char *)NULL, **undel_elements = (char **)NULL;
	while (!line)
		line = rl_no_hist(_("File(s) to be undeleted (ex: 1 2-6, or *): "));

	undel_elements = get_substr(line, ' ');
	free(line);
	line = (char *)NULL;
	if (undel_elements) {

		for (i = 0; undel_elements[i]; i++)
			undel_n++;
	}

	else
		return EXIT_FAILURE;

	/* First check for quit, *, and non-number args */
	int free_and_return = 0;

	for(i = 0; i < (size_t)undel_n; i++) {

		if (strcmp(undel_elements[i], "q") == 0)
			free_and_return = 1;

		else if (strcmp(undel_elements[i], "*") == 0) {
			size_t j;

			for (j = 0; j < (size_t)trash_files_n; j++)

				if (untrash_element(trash_files[j]->d_name) != 0)
					exit_status = EXIT_FAILURE;

			free_and_return = 1;
		}

		else if (!is_number(undel_elements[i])) {
			fprintf(stderr, _("undel: %s: Invalid ELN\n"), undel_elements[i]);
			exit_status = EXIT_FAILURE;
			free_and_return = 1;
		}
	}

	/* Free and return if any of the above conditions is true */
	if (free_and_return) {
		size_t j = 0;

		for (j = 0; j < (size_t)undel_n; j++)
			free(undel_elements[j]);

		free(undel_elements);

		for (j = 0; j < (size_t)trash_files_n; j++)
			free(trash_files[j]);

		free(trash_files);

		return exit_status;
	}

	/* Undelete trashed files */
	int undel_num;
	for(i = 0; i < (size_t)undel_n; i++) {
		undel_num = atoi(undel_elements[i]);

		if (undel_num <= 0 || undel_num > trash_files_n) {
			fprintf(stderr, _("%s: undel: %d: Invalid ELN\n"),
					PROGRAM_NAME, undel_num);
			free(undel_elements[i]);
			continue;
		}

		/* If valid ELN */
		if (untrash_element(trash_files[undel_num - 1]->d_name) != 0)
			exit_status = EXIT_FAILURE;

		free(undel_elements[i]);
	}

	free(undel_elements);

	/* Free trashed files list */
	for (i = 0; i < (size_t)trash_files_n; i++)
		free(trash_files[i]);

	free(trash_files);

	/* If some trashed file still remains, reload the undel screen */
	trash_n = count_dir(TRASH_FILES_DIR);

	if (trash_n <= 2)
		trash_n = 0;

	if (trash_n)
		untrash_function(comm);

	return exit_status;
}

static int
trash_clear(void)
{
	struct dirent **trash_files = (struct dirent **)NULL;
	int files_n = -1, exit_status = EXIT_SUCCESS;

	if (xchdir(TRASH_FILES_DIR, NO_TITLE) == -1) {
		_err(0, NOPRINT_PROMPT, "%s: trash: '%s': %s\n", PROGRAM_NAME,
			 TRASH_FILES_DIR, strerror(errno));
		return EXIT_FAILURE;
	}

	files_n = scandir(TRASH_FILES_DIR, &trash_files, skip_files, xalphasort);

	if (!files_n) {
		puts(_("trash: There are no trashed files"));
		return EXIT_SUCCESS;
	}

	size_t i;

	for (i = 0; i < (size_t)files_n; i++) {
		size_t info_file_len = strlen(trash_files[i]->d_name) + 11;
		char *info_file = (char *)xnmalloc(info_file_len, sizeof(char));
		sprintf(info_file, "%s.trashinfo", trash_files[i]->d_name);

		char *file1 = (char *)NULL;
		file1 = (char *)xnmalloc(strlen(TRASH_FILES_DIR) +
						strlen(trash_files[i]->d_name) + 2, sizeof(char));

		sprintf(file1, "%s/%s", TRASH_FILES_DIR, trash_files[i]->d_name);

		char *file2 = (char *)NULL;
		file2 = (char *)xnmalloc(strlen(TRASH_INFO_DIR) +
						strlen(info_file) + 2, sizeof(char));
		sprintf(file2, "%s/%s", TRASH_INFO_DIR, info_file);

		char *tmp_cmd[] = { "rm", "-r", file1, file2, NULL };

		int ret = launch_execve(tmp_cmd, FOREGROUND, E_NOFLAG);

		free(file1);
		free(file2);

		if (ret != EXIT_SUCCESS) {
			fprintf(stderr, _("%s: trash: %s: Error removing "
					"trashed file\n"), PROGRAM_NAME, trash_files[i]->d_name);
			exit_status = EXIT_FAILURE;
			/* If there is at least one error, return error */
		}

		free(info_file);
		free(trash_files[i]);
	}

	free(trash_files);

	if (xchdir(ws[cur_ws].path, NO_TITLE) == -1) {
		_err(0, NOPRINT_PROMPT, "%s: trash: '%s': %s\n", PROGRAM_NAME,
			 ws[cur_ws].path, strerror(errno));
		return EXIT_FAILURE;
	}

	return exit_status;
}

static int
trash_function (char **comm)
{
	if (xargs.stealth_mode == 1) {
		printf("%s: The trash function is disabled in "
			   "stealth mode\n", PROGRAM_NAME);
		return EXIT_SUCCESS;
	}

	/* Create trash dirs, if necessary */
/*  struct stat file_attrib;
	if (stat (TRASH_DIR, &file_attrib) == -1) {
		char *trash_files = NULL;
		trash_files = xcalloc(strlen(TRASH_DIR) + 7, sizeof(char));
		sprintf(trash_files, "%s/files", TRASH_DIR);
		char *trash_info=NULL;
		trash_info = xcalloc(strlen(TRASH_DIR) + 6, sizeof(char));
		sprintf(trash_info, "%s/info", TRASH_DIR);
		char *cmd[] = { "mkdir", "-p", trash_files, trash_info, NULL };
		int ret = launch_execve (cmd, FOREGROUND, E_NOFLAG);
		free(trash_files);
		free(trash_info);
		if (ret != EXIT_SUCCESS) {
			_err(0, NOPRINT_PROMPT, _("%s: mkdir: '%s': Error creating "
				 "trash directory\n"), PROGRAM_NAME, TRASH_DIR);
			return;
		}
	} */

	if (!comm)
		return EXIT_FAILURE;

	if (!trash_ok || !config_ok) {
		fprintf(stderr, _("%s: Trash function disabled\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	/* List trashed files ('tr' or 'tr ls') */
	if (!comm[1] || strcmp(comm[1], "ls") == 0 
			|| strcmp(comm[1], "list") == 0) {
		/* List files in the Trash/files dir */

		if (xchdir(TRASH_FILES_DIR, NO_TITLE) == -1) {
			_err(0, NOPRINT_PROMPT, "%s: trash: %s: %s\n",
				 PROGRAM_NAME, TRASH_FILES_DIR, strerror(errno));
			return EXIT_FAILURE;
		}

		struct dirent **trash_files = (struct dirent **)NULL;
		int files_n = scandir(TRASH_FILES_DIR, &trash_files,
							  skip_files, (unicode) ? alphasort :
							  (case_sensitive) ? xalphasort :
							  alphasort_insensitive);
		if (files_n) {
			size_t i;

			for (i = 0; i < (size_t)files_n; i++) {
				colors_list(trash_files[i]->d_name, (int)i + 1, NO_PAD,
							PRINT_NEWLINE);
				free(trash_files[i]);
			}

			free(trash_files);
		}

		else
			puts(_("trash: There are no trashed files"));

		if (xchdir(ws[cur_ws].path, NO_TITLE) == -1) {
			_err(0, NOPRINT_PROMPT, "%s: trash: '%s': %s\n",
				 PROGRAM_NAME, ws[cur_ws].path, strerror(errno));
			return EXIT_FAILURE;
		}

		else
			return EXIT_SUCCESS;
	}

	else {

		/* Create suffix from current date and time to create unique
		 * filenames for trashed files */
		int exit_status = EXIT_SUCCESS;
		time_t rawtime = time(NULL);
		struct tm *tm = localtime(&rawtime);
		char date[64] = "";

		strftime(date, sizeof(date), "%b %d %H:%M:%S %Y", tm);

		char suffix[68] = "";

		snprintf(suffix, 67, "%d%d%d%d%d%d", tm->tm_year + 1900,
				tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min,
				tm->tm_sec);

		/* Remove file(s) from Trash */
		if (strcmp(comm[1], "del") == 0 || strcmp(comm[1], "rm") == 0)
			exit_status = remove_from_trash();

		else if (strcmp(comm[1], "clear") == 0)
			trash_clear();

		else {
			/* Trash files passed as arguments */
			size_t i;

			for (i = 1; comm[i]; i++) {
				char *deq_file = dequote_str(comm[i], 0);
				char tmp_comm[PATH_MAX] = "";

				if (deq_file[0] == '/') /* If absolute path */
					strcpy(tmp_comm, deq_file);

				else { /* If relative path, add path to check against
					TRASH_DIR */
					snprintf(tmp_comm, PATH_MAX, "%s/%s", ws[cur_ws].path,
							 deq_file);
				}

				/* Some filters: you cannot trash wathever you want */
				/* Do not trash any of the parent directories of TRASH_DIR,
				 * that is, /, /home, ~/, ~/.local, ~/.local/share */
				if (strncmp(tmp_comm, TRASH_DIR, strlen(tmp_comm)) == 0) {
					fprintf(stderr, _("trash: Cannot trash '%s'\n"), tmp_comm);
					exit_status = EXIT_FAILURE;
					free(deq_file);
					continue;
				}

				/* Do no trash TRASH_DIR itself nor anything inside it,
				 * that is, already trashed files */
				else if (strncmp(tmp_comm, TRASH_DIR,
						 strlen(TRASH_DIR)) == 0) {
					puts(_("trash: Use 'trash del' to remove trashed files"));
					exit_status = EXIT_FAILURE;
					free(deq_file);
					continue;
				}

				struct stat file_attrib;

				if (lstat(deq_file, &file_attrib) == -1) {
					fprintf(stderr, _("trash: %s: %s\n"), deq_file,
							strerror(errno));
					exit_status = EXIT_FAILURE;
					free(deq_file);
					continue;
				}

				/* Do not trash block or character devices */
				else {
					if ((file_attrib.st_mode & S_IFMT) == S_IFBLK) {
						fprintf(stderr, _("trash: %s: Cannot trash a "
								"block device\n"), deq_file);
						exit_status = EXIT_FAILURE;
						free(deq_file);
						continue;
					}

					else if ((file_attrib.st_mode & S_IFMT) == S_IFCHR) {
						fprintf(stderr, _("trash: %s: Cannot trash a "
								"character device\n"), deq_file);
						exit_status = EXIT_FAILURE;
						free(deq_file);
						continue;
					}
				}

				/* Once here, everything is fine: trash the file */
				exit_status = trash_element(suffix, tm, deq_file);
				/* The trash_element() function will take care of
				 * printing error messages, if any */
				free(deq_file);
			}
		}

		return exit_status;
	}
}

static int
get_sel_files(void)
/* Get current entries in the Selection Box, if any. */
{
	if (!selfile_ok || !config_ok)
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
	FILE *sel_fp = fopen(SEL_FILE, "r");

/*  sel_elements = xcalloc(1, sizeof(char *)); */
	if (!sel_fp)
		return EXIT_FAILURE;

	/* Since this file contains only paths, we can be sure no line
	 * length will be larger than PATH_MAX */
	char line[PATH_MAX] = "";

	while (fgets(line, (int)sizeof(line), sel_fp)) {

		size_t len = strlen(line);

		if (line[len - 1 ] == '\n')
			line[len - 1] = '\0';

		if (!*line || *line == '#')
			continue;

		sel_elements = (char **)xrealloc(sel_elements, (sel_n + 1)
										 * sizeof(char *));
		sel_elements[sel_n++] = savestring(line, len);
	}

	fclose(sel_fp);

	return EXIT_SUCCESS;
}

static char *
prompt(void)
/* Print the prompt and return the string entered by the user (to be
 * parsed later by parse_input_str()) */
{
	/* Remove all final slash(es) from path, if any */
	size_t path_len = strlen(ws[cur_ws].path), i;

	for (i = path_len - 1; ws[cur_ws].path[i] && i > 0; i--) {

		if (ws[cur_ws].path[i] != '/')
			break;
		else
			ws[cur_ws].path[i] = '\0';
	}

	if (welcome_message) {
		printf(_("%sCliFM, the anti-eye-candy, KISS file manager%s\n"
			   "Enter '?' or press F[1-3] for instructions.\n"), wc_c, df_c);
		welcome_message = 0;
	}

	/* Print the tip of the day (only on first run) */
	if (tips) {
		static int first_run = 1;

		if (first_run) {
			print_tips(0);
			first_run = 0;
		}
	}

	/* Execute prompt commands, if any, and only if external commands
	 * are allowed */
	if (ext_cmd_ok && prompt_cmds_n > 0)

		for (i = 0; i < prompt_cmds_n; i++)
			launch_execle(prompt_cmds[i]);

	/* Update trash and sel file indicator on every prompt call */
	if (trash_ok) {
		trash_n = count_dir(TRASH_FILES_DIR);

		if (trash_n <= 2)
			trash_n = 0;
	}

	get_sel_files();

	/* Messages are categorized in three groups: errors, warnings, and
	 * notices. The kind of message should be specified by the function
	 * printing the message itself via a global enum: pmsg, with the
	 * following values: nomsg, error, warning, and notice. */
	char msg_str[MAX_COLOR + 1 + 16] = "";

	if (msgs_n) {
		/* Errors take precedence over warnings, and warnings over
		 * notices. That is to say, if there is an error message AND a
		 * warning message, the prompt will always display the error
		 * message sign: a red 'E'. */
		switch (pmsg) {
		case nomsg: break;
		case error: sprintf(msg_str, "%sE%s", em_c, NC_b); break;
		case warning: sprintf(msg_str, "%sW%s", wm_c, NC_b); break;
		case notice: sprintf(msg_str, "%sN%s", nm_c, NC_b); break;
		default: break;
		}
	}

	/* Generate the prompt string */

	/* First, grab and decode the prompt line of the config file (stored
	 * in encoded_prompt at startup) */
	char *decoded_prompt = decode_prompt(encoded_prompt);

	/* Emergency prompt, just in case decode_prompt fails */
	if (!decoded_prompt) {
		fprintf(stderr, _("%s: Error decoding prompt line. Using an "
				"emergency prompt\n"), PROGRAM_NAME);

		decoded_prompt = (char *)xnmalloc(9, sizeof(char));

		sprintf(decoded_prompt, "\001\x1b[0m\002> ");
	}

	size_t decoded_prompt_len;

	if (unicode)
		decoded_prompt_len = u8_xstrlen(decoded_prompt);
	else
		decoded_prompt_len = strlen(decoded_prompt);

	size_t prompt_length = (size_t)(decoded_prompt_len
		+ (xargs.stealth_mode == 1 ? 16 : 0)
		+ (sel_n ? 16 : 0) + (trash_n ? 16 : 0) + ((msgs_n && pmsg)
		? 16 : 0) + 6 + sizeof(tx_c) + 1);

	/* 16 = color_b({red,green,yellow}_b)+letter (sel, trash, msg)+NC_b;
	 * 6 = NC_b
	 * 1 = null terminating char */

	char *the_prompt = (char *)xnmalloc(prompt_length, sizeof(char));
/*  char the_prompt[prompt_length]; */

	snprintf(the_prompt, prompt_length, "%s%s%s%s%s%s%s%s%s%s",
		(msgs_n && pmsg) ? msg_str : "", (xargs.stealth_mode == 1)
		? si_c : "", (xargs.stealth_mode == 1)
		? "S\001\x1b[0m\002" : "", (trash_n) ? ti_c : "",
		(trash_n) ? "T\001\x1b[0m\002" : "", (sel_n) ? li_c
		: "", (sel_n) ? "*\001\x1b[0m\002" : "", decoded_prompt,
		NC_b, tx_c);

	free(decoded_prompt);

	/* Print error messages, if any. 'print_errors' is set to true by
	 * log_msg() with the PRINT_PROMPT flag. If NOPRINT_PROMPT is
	 * passed instead, 'print_msg' will be false and the message will
	 * be printed in place by log_msg() itself, without waiting for
	 * the next prompt */
	if (print_msg) {
		fputs(messages[msgs_n - 1], stderr);

		print_msg = 0; /* Print messages only once */
	}

	args_n = 0;

	/* Print the prompt and get user input */
	char *input = (char *)NULL;
	input = readline(the_prompt);
	free(the_prompt);

	if (!input)
	/* Same as 'input == NULL': input is a pointer poiting to no
	 * memory address whatsover */
		return (char *)NULL;

	if (!*input) {
	/* input is not NULL, but a pointer poiting to a memory address
	 * whose first byte is the null byte (\0). In other words, it is
	 * an empty string */
		free(input);
		input = (char *)NULL;
		return (char *)NULL;
	}

	/* Keep a literal copy of the last entered command to compose the
	 * commands log, if needed and enabled */
	if (logs_enabled) {
		if (last_cmd)
			free(last_cmd);

		last_cmd = savestring(input, strlen(input));
	}

	/* Do not record empty lines, exit, history commands, consecutively
	 * equal inputs, or lines starting with space */
	if (record_cmd(input))
		add_to_cmdhist(input);

	return input;
}

static int
load_keybinds(void)
/* Store keybinds from the keybinds file into a struct */
{
	if (!config_ok)
		return EXIT_FAILURE;

	/* Free the keybinds struct array */
	if (kbinds_n) {
		int i = (int)kbinds_n;

		while (--i >= 0) {
			free(kbinds[i].function);
			free(kbinds[i].key);
		}

		free(kbinds);

		kbinds = (struct kbinds_t *)xnmalloc(1, sizeof(struct kbinds_t));

		kbinds_n = 0;
	}

	/* Open the keybinds file */
	FILE *fp = fopen(KBINDS_FILE, "r");

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
		tmp = strchr(line, ':');

		if (!tmp || !*(tmp + 1))
			continue;

		/* Now copy left and right value of each keybind into the
		 * keybinds struct */
		kbinds = xrealloc(kbinds, (kbinds_n + 1) * sizeof(struct kbinds_t));

		kbinds[kbinds_n].key = savestring(tmp + 1, strlen(tmp + 1));

		*tmp = '\0';

		kbinds[kbinds_n++].function = savestring(line, strlen(line));
	}

	free(line);

	return EXIT_SUCCESS;
}

static char *
find_key(char *function)
{
	if (!kbinds_n)
		return (char *)NULL;

	int n = (int)kbinds_n;

	while (--n >= 0) {
		if (*function != *kbinds[n].function)
			continue;

		if (strcmp(function, kbinds[n].function) == 0)
			return kbinds[n].key;
	}

	return (char *)NULL;
}

static int
keybind_exec_cmd(char *str)
/* Runs any command recognized by CliFM via a keybind. Example:
 * keybind_exec_cmd("sel *") */
{
	size_t old_args = args_n;
	args_n = 0;

	int exit_status = EXIT_FAILURE;

	char **cmd = parse_input_str(str);
	putchar('\n');

	if (cmd) {

		exit_status = exec_cmd(cmd);

		/* While in the bookmarks or mountpoints screen, the kbind_busy
		 * flag will be set to 1 and no keybinding will work. Once the
		 * corresponding function exited, set the kbind_busy flag to zero,
		 * so that keybindings work again */
		if (kbind_busy)
			kbind_busy = 0;

		int i = (int)args_n + 1;
		while (--i >= 0)
			free(cmd[i]);
		free(cmd);

		/* This call to prompt() just updates the prompt in case it was
		 * modified, for example, in case of chdir, files selection, and
		 * so on */
		char *buf = prompt();
		free(buf);
	}

	args_n = old_args;

	return exit_status;
}

static int
rl_refresh(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	if (clear_screen)
		CLEAR;
	keybind_exec_cmd("rf");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_parent_dir(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	/* If already root dir, do nothing */
	if (*ws[cur_ws].path == '/' && !ws[cur_ws].path[1])
		return EXIT_SUCCESS;

	keybind_exec_cmd("cd ..");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_root_dir(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	/* If already root dir, do nothing */
	if (*ws[cur_ws].path == '/' && !ws[cur_ws].path[1])
		return EXIT_SUCCESS;

	keybind_exec_cmd("cd /");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_home_dir(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	/* If already in home, do nothing */
	if (*ws[cur_ws].path == *user_home
	&& strcmp(ws[cur_ws].path, user_home) == 0)
		return EXIT_SUCCESS;

	keybind_exec_cmd("cd");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_next_dir(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	/* If already at the end of dir hist, do nothing */
	if (dirhist_cur_index + 1 == dirhist_total_index)
		return EXIT_SUCCESS;

	keybind_exec_cmd("f");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_first_dir(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	/* If already at the beginning of dir hist, do nothing */
	if (dirhist_cur_index == 0)
		return EXIT_SUCCESS;

	keybind_exec_cmd("b !1");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_last_dir(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	/* If already at the end of dir hist, do nothing */
	if (dirhist_cur_index + 1 == dirhist_total_index)
		return EXIT_SUCCESS;

	char cmd[PATH_MAX + 4];
	sprintf(cmd, "b !%d", dirhist_total_index);
	keybind_exec_cmd(cmd);

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_previous_dir(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	/* If already at the beginning of dir hist, do nothing */
	if (dirhist_cur_index == 0)
		return EXIT_SUCCESS;

	keybind_exec_cmd("b");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_long(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	long_view = long_view ? 0 : 1;

	if (clear_screen)
		CLEAR;

	keybind_exec_cmd("rf");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_folders_first(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	list_folders_first = list_folders_first ? 0 : 1;

	if (cd_lists_on_the_fly) {
		if (clear_screen)
			CLEAR;
		free_dirlist();
		/* Without this putchar(), the first entries of the directories
		 * list are printed in the prompt line */
		putchar('\n');
		list_dir();
	}

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_light(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	light_mode = light_mode ? 0 : 1;

	if (clear_screen)
		CLEAR;

	keybind_exec_cmd("rf");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_hidden(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	show_hidden = show_hidden ? 0 : 1;

	if (cd_lists_on_the_fly) {
		if (clear_screen)
			CLEAR;
		free_dirlist();
		putchar('\n');
		list_dir();
	}

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_open_config(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("edit");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_open_keybinds(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("kb edit");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_open_cscheme(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("cs e");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_open_bm_file(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("bm edit");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_open_jump_db(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("je");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_open_mime(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("mm edit");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_mountpoints(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	/* Call the function only if it's not already running */
	kbind_busy = 1;
	keybind_exec_cmd("mp");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_select_all(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("s ^");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_deselect_all(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("ds *");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_bookmarks(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	kbind_busy = 1;
	keybind_exec_cmd("bm");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_selbox(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("ds");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_clear_line(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	/* 1) Clear text typed so far (\x1b[2K) and move cursor to the
	 * beginning of the current line (\r) */
	write(STDOUT_FILENO, "\x1b[2K\r", 5);

	/* 2) Clear the readline buffer */
	rl_delete_text(0, rl_end);
	rl_end = rl_point = 0;

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_sort_next(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	sort++;
	if (sort > SORT_TYPES)
		sort = 0;

	if (cd_lists_on_the_fly) {
		if (clear_screen)
			CLEAR;
		sort_switch = 1;
		free_dirlist();
		putchar('\n');
		list_dir();
		sort_switch = 0;
	}

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_sort_previous(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	sort--;
	if (sort < 0)
		sort = SORT_TYPES;

	if (cd_lists_on_the_fly) {
		if (clear_screen)
			CLEAR;
		sort_switch = 1;
		free_dirlist();
		putchar('\n');
		list_dir();
		sort_switch = 0;
	}

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_lock(int count, int key)
{
	int ret = EXIT_SUCCESS;

	rl_deprep_terminal();

#if __FreeBSD__
	char *cmd[] = { "lock", NULL };
	ret = launch_execve(cmd, FOREGROUND, E_NOFLAG);
#elif __linux__
	char *cmd[] = { "vlock", NULL };
	ret = launch_execve(cmd, FOREGROUND, E_NOFLAG);
#endif

	rl_prep_terminal(0);
	rl_reset_line_state();

	if (ret != EXIT_SUCCESS)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

static int
rl_remove_sel(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	rl_deprep_terminal();

	kb_shortcut = 1;
	keybind_exec_cmd("r sel");
	kb_shortcut = 0;

	rl_prep_terminal(0);
	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_export_sel(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	kb_shortcut = 1;
	keybind_exec_cmd("exp sel");
	kb_shortcut = 0;

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_move_sel(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	kb_shortcut = 1;
	keybind_exec_cmd("m sel");
	kb_shortcut = 0;

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_rename_sel(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	kb_shortcut = 1;
	keybind_exec_cmd("br sel");
	kb_shortcut = 0;

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_paste_sel(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	kb_shortcut = 1;
	keybind_exec_cmd("c sel");
	kb_shortcut = 0;

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_quit(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

//  keybind_exec_cmd("q");

	return EXIT_SUCCESS;
}

static int
rl_previous_profile(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	int prev_prof, i, cur_prof = -1, total_profs = 0;
	for (i = 0; profile_names[i]; i++) {
		total_profs++;

		if (!alt_profile) {
			if (*profile_names[i] == 'd'
			&& strcmp(profile_names[i], "default") == 0) {
				cur_prof = i;
			}
		}

		else {
			if (*alt_profile == *profile_names[i]
			&& strcmp(alt_profile, profile_names[i]) == 0) {
				cur_prof = i;
			}
		}
	}

	if (cur_prof == -1 || !profile_names[cur_prof])
		return EXIT_FAILURE;

	prev_prof = cur_prof - 1;
	total_profs--;

	if (prev_prof < 0 || !profile_names[prev_prof])
		prev_prof = total_profs;

	if (clear_screen) {
		CLEAR;
	}
	else
		putchar('\n');

	if (profile_set(profile_names[prev_prof]) == EXIT_SUCCESS) {
		printf(_("%s->%s Switched to profile '%s'\n"), mi_c, df_c,
			   profile_names[prev_prof]);
		char *input = prompt();
		free(input);
	}

	return EXIT_SUCCESS;
}

static int
rl_next_profile(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	int next_prof, i, cur_prof = -1, total_profs = 0;
	for (i = 0; profile_names[i]; i++) {
		total_profs++;

		if (!alt_profile) {
			if (*profile_names[i] == 'd'
			&& strcmp(profile_names[i], "default") == 0) {
				cur_prof = i;
			}
		}

		else {
			if (*alt_profile == *profile_names[i]
			&& strcmp(alt_profile, profile_names[i]) == 0) {
				cur_prof = i;
			}
		}
	}

	if (cur_prof == -1 || !profile_names[cur_prof])
		return EXIT_FAILURE;

	next_prof = cur_prof + 1;
	total_profs--;

	if (next_prof > (int)total_profs || !profile_names[next_prof])
		next_prof = 0;

	if (clear_screen) {
		CLEAR;
	}
	else
		putchar('\n');

	if (profile_set(profile_names[next_prof]) == EXIT_SUCCESS) {
		printf(_("%s->%s Switched to profile '%s'\n"), mi_c, df_c,
			   profile_names[next_prof]);
		char *input = prompt();
		free(input);
	}

	return EXIT_SUCCESS;
}

static int
rl_dirhist(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("bh");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_archive_sel(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("ac sel");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_new_instance(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("x .");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_clear_msgs(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("msg clear");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_trash_sel(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("t sel");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_untrash_all(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("u *");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_open_sel(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	if (sel_n == 0 || !sel_elements[sel_n - 1]) {
		fprintf(stderr, _("\n%s: No selected files\n"), PROGRAM_NAME);
		rl_reset_line_state();
		return EXIT_FAILURE;
	}

	char cmd[PATH_MAX + 3];
	sprintf(cmd, "o %s", sel_elements[sel_n -  1]);

	keybind_exec_cmd(cmd);

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_bm_sel(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	if (sel_n == 0 || !sel_elements[sel_n - 1]) {
		fprintf(stderr, _("\n%s: No selected files\n"), PROGRAM_NAME);
		rl_reset_line_state();
		return EXIT_FAILURE;
	}

	char cmd[PATH_MAX + 6];
	sprintf(cmd, "bm a %s", sel_elements[sel_n -  1]);

	keybind_exec_cmd(cmd);

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_kbinds_help (int count, int key)
{
	char *cmd[] = { "man", "-P", "less -p ^\"KEYBOARD SHORTCUTS\"", PNL,
					NULL };
	if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

static int
rl_cmds_help (int count, int key)
{
	char *cmd[] = { "man", "-P", "less -p ^COMMANDS", PNL, NULL };
	if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

static int
rl_manpage (int count, int key)
{
	char *cmd[] = { "man", PNL, NULL };

	if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

static int
rl_pinned_dir(int count, int key)
{
	if (!pinned_dir) {
		printf(_("%s: No pinned file\n"), PROGRAM_NAME);
		return EXIT_SUCCESS;
	}

	keybind_exec_cmd(",");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_ws1(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("ws 1");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_ws2(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("ws 2");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_ws3(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("ws 3");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_ws4(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("ws 4");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_plugin1(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("plugin1");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_plugin2(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("plugin2");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_plugin3(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("plugin3");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_plugin4(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("plugin4");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static void
readline_kbinds(void)
{
/* To get the keyseq value for a given key do this in an Xterm terminal:
 * C-v and then press the key (or the key combination). So, for example,
 * C-v, C-right arrow gives "[[1;5C", which here should be written like
 * this:
 * "\\x1b[1;5C" */

			/* ##############################
			 * #        KEYBINDINGS         #
			 * ##############################*/
	if (KBINDS_FILE) {
		/* Help */
		rl_bind_keyseq(find_key("show-manpage"), rl_manpage);
		rl_bind_keyseq(find_key("show-cmds"), rl_cmds_help);
		rl_bind_keyseq(find_key("show-kbinds"), rl_kbinds_help);

		/* Navigation */
		/* Define multiple keybinds for different terminals:
		 * rxvt, xterm, kernel console */
/*      rl_bind_keyseq("\\M-[D", rl_test); // Left arrow key
		rl_bind_keyseq("\\M-+", rl_test); */
		rl_bind_keyseq(find_key("parent-dir"), rl_parent_dir);
		rl_bind_keyseq(find_key("parent-dir2"), rl_parent_dir);
		rl_bind_keyseq(find_key("parent-dir3"), rl_parent_dir);
		rl_bind_keyseq(find_key("parent-dir4"), rl_parent_dir);
		rl_bind_keyseq(find_key("previous-dir"), rl_previous_dir);
		rl_bind_keyseq(find_key("previous-dir2"), rl_previous_dir);
		rl_bind_keyseq(find_key("previous-dir3"), rl_previous_dir);
		rl_bind_keyseq(find_key("previous-dir4"), rl_previous_dir);
		rl_bind_keyseq(find_key("next-dir"), rl_next_dir);
		rl_bind_keyseq(find_key("next-dir2"), rl_next_dir);
		rl_bind_keyseq(find_key("next-dir3"), rl_next_dir);
		rl_bind_keyseq(find_key("next-dir4"), rl_next_dir);
		rl_bind_keyseq(find_key("home-dir"), rl_home_dir);
		rl_bind_keyseq(find_key("home-dir2"), rl_home_dir);
		rl_bind_keyseq(find_key("home-dir3"), rl_home_dir);
		rl_bind_keyseq(find_key("root-dir"), rl_root_dir);
		rl_bind_keyseq(find_key("root-dir2"), rl_root_dir);
		rl_bind_keyseq(find_key("root-dir3"), rl_root_dir);

		rl_bind_keyseq(find_key("first-dir"), rl_first_dir);
		rl_bind_keyseq(find_key("last-dir"), rl_last_dir);

		rl_bind_keyseq(find_key("pinned-dir"), rl_pinned_dir);
		rl_bind_keyseq(find_key("workspace1"), rl_ws1);
		rl_bind_keyseq(find_key("workspace2"), rl_ws2);
		rl_bind_keyseq(find_key("workspace3"), rl_ws3);
		rl_bind_keyseq(find_key("workspace4"), rl_ws4);

		/* Operations on files */
		rl_bind_keyseq(find_key("bookmark-sel"), rl_bm_sel);
		rl_bind_keyseq(find_key("archive-sel"), rl_archive_sel);
		rl_bind_keyseq(find_key("open-sel"), rl_open_sel);
		rl_bind_keyseq(find_key("export-sel"), rl_export_sel);
		rl_bind_keyseq(find_key("move-sel"), rl_move_sel);
		rl_bind_keyseq(find_key("rename-sel"), rl_rename_sel);
		rl_bind_keyseq(find_key("remove-sel"), rl_remove_sel);
		rl_bind_keyseq(find_key("trash-sel"), rl_trash_sel);
		rl_bind_keyseq(find_key("untrash-all"), rl_untrash_all);
		rl_bind_keyseq(find_key("paste-sel"), rl_paste_sel);
		rl_bind_keyseq(find_key("select-all"), rl_select_all);
		rl_bind_keyseq(find_key("deselect-all"), rl_deselect_all);

		/* Config files */
		rl_bind_keyseq(find_key("open-mime"), rl_open_mime);
		rl_bind_keyseq(find_key("open-jump-db"), rl_open_jump_db);
		rl_bind_keyseq(find_key("edit-color-scheme"), rl_open_cscheme);
		rl_bind_keyseq(find_key("open-config"), rl_open_config);
		rl_bind_keyseq(find_key("open-keybinds"), rl_open_keybinds);
		rl_bind_keyseq(find_key("open-bookmarks"), rl_open_bm_file);

		/* Settings */
		rl_bind_keyseq(find_key("clear-msgs"), rl_clear_msgs);
		rl_bind_keyseq(find_key("next-profile"), rl_next_profile);
		rl_bind_keyseq(find_key("previous-profile"), rl_previous_profile);
		rl_bind_keyseq(find_key("quit"), rl_quit);
		rl_bind_keyseq(find_key("lock"), rl_lock);
		rl_bind_keyseq(find_key("refresh-screen"), rl_refresh);
		rl_bind_keyseq(find_key("clear-line"), rl_clear_line);
		rl_bind_keyseq(find_key("toggle-hidden"), rl_hidden);
		rl_bind_keyseq(find_key("toggle-hidden2"), rl_hidden);
		rl_bind_keyseq(find_key("toggle-long"), rl_long);
		rl_bind_keyseq(find_key("toggle-light"), rl_light);
		rl_bind_keyseq(find_key("folders-first"), rl_folders_first);
		rl_bind_keyseq(find_key("sort-previous"), rl_sort_previous);
		rl_bind_keyseq(find_key("sort-next"), rl_sort_next);

		rl_bind_keyseq(find_key("new-instance"), rl_new_instance);
		rl_bind_keyseq(find_key("show-dirhist"), rl_dirhist);
		rl_bind_keyseq(find_key("bookmarks"), rl_bookmarks);
		rl_bind_keyseq(find_key("mountpoints"), rl_mountpoints);
		rl_bind_keyseq(find_key("selbox"), rl_selbox);

		/* Plugins */
		rl_bind_keyseq(find_key("plugin1"), rl_plugin1);
		rl_bind_keyseq(find_key("plugin2"), rl_plugin2);
		rl_bind_keyseq(find_key("plugin3"), rl_plugin3);
		rl_bind_keyseq(find_key("plugin4"), rl_plugin4);
	}

	/* If no kbinds file is found, set the defaults */
	else {
		/* Help */
		rl_bind_keyseq("\\eOP", rl_manpage);
		rl_bind_keyseq("\\eOQ", rl_cmds_help);
		rl_bind_keyseq("\\eOR", rl_kbinds_help);

		/* Navigation */
		rl_bind_keyseq("\\M-u", rl_parent_dir);
		rl_bind_keyseq("\\e[a", rl_parent_dir);
		rl_bind_keyseq("\\e[2A", rl_parent_dir);
		rl_bind_keyseq("\\e[1;2A", rl_parent_dir);
		rl_bind_keyseq("\\M-j", rl_previous_dir);
		rl_bind_keyseq("\\e[d", rl_previous_dir);
		rl_bind_keyseq("\\e[2D", rl_previous_dir);
		rl_bind_keyseq("\\e[1;2D", rl_previous_dir);
		rl_bind_keyseq("\\M-k", rl_next_dir);
		rl_bind_keyseq("\\e[c", rl_next_dir);
		rl_bind_keyseq("\\e[2C", rl_next_dir);
		rl_bind_keyseq("\\e[1;2C", rl_next_dir);
		rl_bind_keyseq("\\M-e", rl_home_dir);
		rl_bind_keyseq("\\e[7~", rl_home_dir);
		rl_bind_keyseq("\\e[H", rl_home_dir);
		rl_bind_keyseq("\\M-r", rl_root_dir);
		rl_bind_keyseq("\\e/", rl_root_dir);

		rl_bind_keyseq("\\C-\\M-j", rl_first_dir);
		rl_bind_keyseq("\\C-\\M-k", rl_last_dir);

		/* Operations on files */
		rl_bind_keyseq("\\C-\\M-b", rl_bm_sel);
		rl_bind_keyseq("\\C-\\M-a", rl_archive_sel);
		rl_bind_keyseq("\\C-\\M-g", rl_open_sel);
		rl_bind_keyseq("\\C-\\M-e", rl_export_sel);
		rl_bind_keyseq("\\C-\\M-n", rl_move_sel);
		rl_bind_keyseq("\\C-\\M-r", rl_rename_sel);
		rl_bind_keyseq("\\C-\\M-d", rl_remove_sel);
		rl_bind_keyseq("\\C-\\M-t", rl_trash_sel);
		rl_bind_keyseq("\\C-\\M-u", rl_untrash_all);
		rl_bind_keyseq("\\C-\\M-v", rl_paste_sel);
		rl_bind_keyseq("\\M-a", rl_select_all);
		rl_bind_keyseq("\\M-d", rl_deselect_all);

		/* Config files */
		rl_bind_keyseq("\\e[17~", rl_open_mime);
		rl_bind_keyseq("\\e[18~", rl_open_jump_db);
		rl_bind_keyseq("\\e[19~", rl_open_cscheme);
		rl_bind_keyseq("\\e[20~", rl_open_keybinds);
		rl_bind_keyseq("\\e[21~", rl_open_config);
		rl_bind_keyseq("\\e[23~", rl_open_bm_file);

		/* Settings */
		rl_bind_keyseq("\\M-t", rl_clear_msgs);
/*      rl_bind_keyseq("", rl_next_profile);
		rl_bind_keyseq("", rl_previous_profile); */
		rl_bind_keyseq("\\e[24~", rl_quit);
		rl_bind_keyseq("\\M-o", rl_lock);
		rl_bind_keyseq("\\C-r", rl_refresh);
		rl_bind_keyseq("\\M-c", rl_clear_line);
		rl_bind_keyseq("\\M-i", rl_hidden);
		rl_bind_keyseq("\\M-.", rl_hidden);
		rl_bind_keyseq("\\M-l", rl_long);
		rl_bind_keyseq("\\M-y", rl_light);
		rl_bind_keyseq("\\M-f", rl_folders_first);
		rl_bind_keyseq("\\M-z", rl_sort_previous);
		rl_bind_keyseq("\\M-x", rl_sort_next);

		rl_bind_keyseq("\\C-x", rl_new_instance);
		rl_bind_keyseq("\\M-h", rl_dirhist);
		rl_bind_keyseq("\\M-b", rl_bookmarks);
		rl_bind_keyseq("\\M-m", rl_mountpoints);
		rl_bind_keyseq("\\M-s", rl_selbox);
	}
}

static void
save_pinned_dir(void)
/* Store pinned directory for the next session */
{
	if (pinned_dir && config_ok) {

		char *pin_file = (char *)xnmalloc(strlen(CONFIG_DIR) + 7,
										  sizeof(char));
		sprintf(pin_file, "%s/.pin", CONFIG_DIR);

		FILE *fp = fopen(pin_file, "w");

		if (!fp)
			fprintf(stderr, _("%s: Error storing pinned "
					"directory\n"), PROGRAM_NAME);

		else {
			fprintf(fp, "%s", pinned_dir);
			fclose(fp);
		}

		free(pin_file);
	}

	return;
}

static int
save_dirhist(void)
{
	if (!DIRHIST_FILE)
		return EXIT_FAILURE;

	if (!old_pwd || !old_pwd[0])
		return EXIT_SUCCESS;

	FILE *fp = fopen(DIRHIST_FILE, "w");

	if (!fp) {
		fprintf(stderr, _("%s: Could not save directory history: %s\n"),
				PROGRAM_NAME, strerror(errno));
		return EXIT_FAILURE;
	}

	size_t i;

	for (i = 0; old_pwd[i]; i++)
		fprintf(fp, "%s\n", old_pwd[i]);

	fclose(fp);

	return EXIT_SUCCESS;
}

static void
free_stuff(void)
/* This function is called by atexit() to clear whatever is there at exit
 * time and avoid thus memory leaks */
{
	int i = 0;

	if (STDIN_TMP_DIR) {
		char *rm_cmd[] = { "rm", "-rd", "--", STDIN_TMP_DIR, NULL };
		launch_execve(rm_cmd, FOREGROUND, E_NOFLAG);
		free(STDIN_TMP_DIR);
	}

	if (color_schemes) {
		for (i = 0; color_schemes[i]; i++)
			free(color_schemes[i]);

		free(color_schemes);
	}
	if (xargs.stealth_mode != 1) {
		save_pinned_dir();
		save_jumpdb();
	}

	if (jump_db) {

		i = (int)jump_n;
		while (--i >= 0)
			free(jump_db[i].path);

		free(jump_db);
	}

	if (pinned_dir)
		free(pinned_dir);

	if (filter) {
		regfree(&regex_exp);
		free(filter);
	}

	free_bookmarks();

	if (eln_as_file_n)
		free(eln_as_file);

	save_dirhist();

	if (restore_last_path || cd_on_quit)
		save_last_path();

	if (ext_colors_n)
		free(ext_colors_len);

	if (opener)
		free(opener);

	if (encoded_prompt)
		free(encoded_prompt);

	if (profile_names) {
		for (i = 0; profile_names[i]; i++)
			free(profile_names[i]);
		free(profile_names);
	}

	if (alt_profile)
		free(alt_profile);

	free_dirlist();

	if (sel_n > 0) {
		i = (int)sel_n;
		while (--i >= 0)
			free(sel_elements[i]);
		free(sel_elements);
	}

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

	if (history) {
		i = (int)current_hist_n;
		while (--i >= 0)
			free(history[i]);

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

	i = (int)aliases_n;
	while (--i >= 0)
		free(aliases[i]);
	free(aliases);

	i = (int)prompt_cmds_n;
	while (--i >= 0)
		free(prompt_cmds[i]);
	free(prompt_cmds);

	if (flags & FILE_CMD_OK)
		free(file_cmd_path);

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

	free(user);
	free(user_home);
	free(sys_shell);

	i = MAX_WS;
	while (--i >= 0)
		if (ws[i].path)
			free(ws[i].path);
	free(ws);

	free(CONFIG_DIR_GRAL);
	free(CONFIG_DIR);
	free(TRASH_DIR);
	free(TRASH_FILES_DIR);
	free(TRASH_INFO_DIR);
	free(TMP_DIR);
	free(BM_FILE);
	free(LOG_FILE);
	free(HIST_FILE);
	free(DIRHIST_FILE);
	free(CONFIG_FILE);
	free(PROFILE_FILE);
	free(MSG_LOG_FILE);
	free(SEL_FILE);
	free(MIME_FILE);
	free(PLUGINS_DIR);
	free(ACTIONS_FILE);
	free(KBINDS_FILE);
	free(COLORS_DIR);

	/* Restore the color of the running terminal */
	fputs("\x1b[0;39;49m", stdout);
}

static void
file_cmd_check(void)
/* Check if the 'file' command is available: it is needed by the mime
 * function */
{
	file_cmd_path = get_cmd_path("file");

	if (!file_cmd_path) {
		flags &= ~FILE_CMD_OK;
		_err('n', PRINT_PROMPT, _("%s: 'file' command not found. "
			 "Specify an application when opening files. Ex: 'o 12 nano' "
			 "or just 'nano 12'\n"), PROGRAM_NAME);
	}

	else
		flags |= FILE_CMD_OK;
}

static void
set_signals_to_ignore(void)
{
/*  signal(SIGINT, signal_handler); C-c */
	signal(SIGINT, SIG_IGN); /* C-c */
	signal(SIGQUIT, SIG_IGN); /* C-\ */
	signal(SIGTSTP, SIG_IGN); /* C-z */
/*  signal(SIGTERM, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN); */
}

static void
handle_stdin()
{
	/* If files are passed via stdin, we need to disable restore
	 * last path in order to correctly understand relative paths */
	restore_last_path = 0;

	/* Max input size: 512 * (512 * 1024)
	 * 512 chunks of 524288 bytes (512KiB) each
	 * == (65535 * PATH_MAX)
	 * == 262MiB of data ((65535 * PATH_MAX) / 1024) */

	size_t chunk = 512 * 1024, chunks_n = 1,
		   total_len = 0, max_chunks = 512;
	ssize_t input_len = 0;

	/* Initial buffer allocation == 1 chunk */
	char *buf = (char *)xnmalloc(chunk, sizeof(char));

	while (chunks_n < max_chunks) {
		input_len = read(STDIN_FILENO, buf + total_len, chunk);

		/* Error */
		if (input_len < 0) {
			free(buf);
			return;
		}

		/* Nothing else to be read */
		if (input_len == 0)
			break;

		total_len += input_len;
		chunks_n++;

		/* Append a new chunk of memory to the buffer */
		buf = (char *)xrealloc(buf, (chunks_n + 1) * chunk);
	}

	if (total_len == 0)
		goto FREE_N_EXIT;

	/* Null terminate the input buffer */
	buf[total_len] = '\0';

	/* Create tmp dir to store links to files */
	char *rand_ext = gen_rand_str(6);
	if (!rand_ext)
		goto FREE_N_EXIT;

	if (TMP_DIR) {
		STDIN_TMP_DIR = (char *)xnmalloc(strlen(TMP_DIR) + 14, sizeof(char));
		sprintf(STDIN_TMP_DIR, "%s/clifm.%s", TMP_DIR, rand_ext);
	}

	else {
		STDIN_TMP_DIR = (char *)xnmalloc(18, sizeof(char));
		sprintf(STDIN_TMP_DIR, "/tmp/clifm.%s", rand_ext);
	}

	free(rand_ext);

	char *cmd[] = { "mkdir", "-p", STDIN_TMP_DIR, NULL };
	if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
		goto FREE_N_EXIT;

	/* Get CWD: we need it to preppend it to relative paths */
	char *cwd = (char *)NULL;
	cwd = getcwd(NULL, 0);
	if (!cwd)
		goto FREE_N_EXIT;

	/* Get substrings from buf */
	char *p = buf, *q = buf;

	while (*p) {

		if (!*p || *p == '\n') {
			*p = '\0';

			/* Create symlinks (in tmp dir) to each valid file in
			 * the buffer */

			/* If file does not exist */
			struct stat attr;
			if (lstat(q, &attr) == -1)
				continue;

			/* Construct source and destiny files */
			char *tmp_file = strrchr(q, '/');

			if (!tmp_file || !*(++tmp_file))
				tmp_file = q;

			char source[PATH_MAX];

			if (*q != '/' || !q[1])
				snprintf(source, PATH_MAX, "%s/%s", cwd, q);

			else
				strncpy(source, q, PATH_MAX);

			char dest[PATH_MAX];
			sprintf(dest, "%s/%s", STDIN_TMP_DIR, tmp_file);

			if (symlink(source, dest) == -1)
				_err('w', PRINT_PROMPT, "ln: '%s': %s\n", q, strerror(errno));

			q = p + 1;
		}

		p++;
	}

	/* chdir to tmp dir and update path var */
	if (xchdir(STDIN_TMP_DIR, SET_TITLE) == -1) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, STDIN_TMP_DIR,
				strerror(errno));

		char *rm_cmd[] = { "rm" , "-drf", STDIN_TMP_DIR, NULL };
		launch_execve(rm_cmd, FOREGROUND, E_NOFLAG);

		free(cwd);
		goto FREE_N_EXIT;
	}

	free(cwd);

	if (ws[cur_ws].path)
		free(ws[cur_ws].path);

	ws[cur_ws].path = savestring(STDIN_TMP_DIR, strlen(STDIN_TMP_DIR));

	goto FREE_N_EXIT;

	FREE_N_EXIT:
		free(buf);

		/* Go back to tty */
		dup2(STDOUT_FILENO, STDIN_FILENO);

		if (cd_lists_on_the_fly) {
			free_dirlist();
			list_dir();
			add_to_dirhist(ws[cur_ws].path);
		}

		return;
}

static void
init_shell(void)
/* Keep track of attributes of the shell. Make sure the shell is running
 * interactively as the foreground job before proceeding.
 * Taken from:
 * https://www.gnu.org/software/libc/manual/html_node/Initializing-the-Shell.html#Initializing-the-Shell
 * */
{
	/* If shell is not interactive */
	if (!isatty(STDIN_FILENO)) {
		handle_stdin();
		return;
	}

	/* Loop until we are in the foreground */
	while (tcgetpgrp(STDIN_FILENO) != (own_pid = getpgrp()))
		kill (- own_pid, SIGTTIN);

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

static char *
my_rl_quote(char *text, int mt, char *qp)
/* Performs bash-style filename quoting for readline (put a backslash
 * before any char listed in rl_filename_quote_characters.
 * Modified version of:
 * https://utcc.utoronto.ca/~cks/space/blog/programming/ReadlineQuotingExample*/
{
	/* NOTE: mt and qp arguments are not used here, but are required by
	 * rl_filename_quoting_function */

	/*
	 * How it works: P and R are pointers to the same memory location
	 * initialized (calloced) twice as big as the line that needs to be
	 * quoted (in case all chars in the line need to be quoted); TP is a
	 * pointer to TEXT, which contains the string to be quoted. We move
	 * through TP to find all chars that need to be quoted ("a's" becomes
	 * "a\'s", for example). At this point we cannot return P, since this
	 * pointer is at the end of the string, so that we return R instead,
	 * which is at the beginning of the same string pointed to by P.
	 * */
	char *r = (char *)NULL, *p = (char *)NULL, *tp = (char *)NULL;

	size_t text_len = strlen(text);
	/* Worst case: every character of text needs to be escaped. In this
	 * case we need 2x text's bytes plus the NULL byte. */
	p = (char *)xnmalloc((text_len * 2) + 1, sizeof(char));
	r = p;

	if (r == NULL)
		return (char *)NULL;

	/* Escape whatever char that needs to be escaped */
	for (tp = text; *tp; tp++) {

		if (is_quote_char(*tp))
			*p++ = '\\';

		*p++ = *tp;
	}

	/* Add a final null byte to the string */
	*p = '\0';

	return r;
}

static char *
my_rl_path_completion(const char *text, int state)
/* This is the filename_completion_function() function of an old Bash
 * release (1.14.7) modified to fit CliFM needs */
{
	/* state is zero before completion, and 1 ... n after getting
	 * possible completions. Example:
	 * cd Do[TAB] -> state 0
	 * cuments/ -> state 1
	 * wnloads/ -> state 2
	 * */

	/* Dequote string to be completed (text), if necessary */
	static char *tmp_text = (char *)NULL;

	if (strchr(text, '\\')) {
		char *p = savestring(text, strlen(text));

		tmp_text = dequote_str(p, 0);

		free(p);

		p = (char *)NULL;

		if (!tmp_text)
			return (char *)NULL;
	}

	if (*text == '.' && text[1] == '.' && text[2] == '.') {

		char *p = savestring(text, strlen(text));
		tmp_text = fastback(p);

		free(p);
		p = (char *)NULL;

		if (!tmp_text)
			return (char *)NULL;
	}

	/* Perhaps I should add bookmarks here */

	int rl_complete_with_tilde_expansion = 0;
	/* ~/Doc -> /home/user/Doc */

	static DIR *directory;
	static char *filename = (char *)NULL;
	static char *dirname = (char *)NULL;
	static char *users_dirname = (char *)NULL;
	static size_t filename_len;
	static int match, ret;
	struct dirent *ent = (struct dirent *)NULL;
	static int exec = 0, exec_path = 0;
	static char *dir_tmp = (char *)NULL;
	static char tmp[PATH_MAX] = "";

	/* If we don't have any state, then do some initialization. */
	if (!state) {
		char *temp;

		if (dirname)
			free(dirname);
		if (filename)
			free(filename);
		if (users_dirname)
			free(users_dirname);

						/* tmp_text is true whenever text was dequoted */
		size_t text_len = strlen((tmp_text) ? tmp_text : text);
		if (text_len)
			filename = savestring((tmp_text) ? tmp_text : text, text_len);

		else
			filename = savestring("", 1);

		if (!*text)
			text = ".";

		if (text_len)
			dirname = savestring((tmp_text) ? tmp_text : text, text_len);

		else
			dirname = savestring("", 1);

		if (dirname[0] == '.' && dirname[1] == '/')
			exec = 1;

		else
			exec = 0;

		/* Get everything after last slash */
		temp = strrchr(dirname, '/');

		if (temp) {
			strcpy(filename, ++temp);
			*temp = '\0';
		}

		else
			strcpy(dirname, ".");

		/* We aren't done yet.  We also support the "~user" syntax. */

		/* Save the version of the directory that the user typed. */
		size_t dirname_len = strlen(dirname);

		users_dirname = savestring(dirname, dirname_len);
/*      { */
			char *temp_dirname;
			int replace_dirname;

			temp_dirname = tilde_expand(dirname);
			free(dirname);
			dirname = temp_dirname;

			replace_dirname = 0;

			if (rl_directory_completion_hook)
				replace_dirname = (*rl_directory_completion_hook) (&dirname);

			if (replace_dirname) {
				free(users_dirname);
				users_dirname = savestring(dirname, dirname_len);
			}
/*      } */
		directory = opendir(dirname);
		filename_len = strlen(filename);

		rl_filename_completion_desired = 1;


	}

	if (tmp_text) {
		free(tmp_text);
		tmp_text = (char *)NULL;
	}

	/* Now that we have some state, we can read the directory. If we found
	 * a match among files in dir, break the loop and print the match */

	match = 0;

	size_t dirname_len = strlen(dirname);

	/* This block is used only in case of "/path/./" to remove the
	 * ending "./" from dirname and to be able to perform thus the
	 * executable check via access() */
	exec_path = 0;

	if (dirname_len > 2) {

		if (dirname[dirname_len - 3] == '/'
		&& dirname[dirname_len - 2] == '.'
		&& dirname[dirname_len - 1] == '/') {
			dir_tmp = savestring(dirname, dirname_len);

			if (dir_tmp) {
				dir_tmp[dirname_len - 2] = '\0';
				exec_path = 1;
			}
		}
	}

	/* ############### COMPLETION FILTER ################## */
	/* #        This is the heart of the function         #
	 * #################################################### */

	while (directory && (ent = readdir(directory))) {

		/* If the user entered nothing before TAB (ex: "cd [TAB]") */
		if (!filename_len) {

			/* Exclude "." and ".." as possible completions */
			if (SELFORPARENT(ent->d_name))
				continue;

			/* If 'cd', match only dirs or symlinks to dir */
			if (*rl_line_buffer == 'c'
			&& strncmp(rl_line_buffer, "cd ", 3) == 0) {
				ret = -1;

				switch (ent->d_type) {
				case DT_LNK:
					if (dirname[0] == '.' && !dirname[1])
						ret = get_link_ref(ent->d_name);
					else {
						snprintf(tmp, PATH_MAX, "%s%s", dirname,
								 ent->d_name);
						ret = get_link_ref(tmp);
					}

					if (ret == S_IFDIR)
						match = 1;

					break;

				case DT_DIR:
					match = 1;
					break;

				default:
					break;
				}
			}

			/* If 'open', allow only reg files, dirs, and symlinks */
			else if (*rl_line_buffer == 'o'
			&& (strncmp(rl_line_buffer, "o ", 2) == 0
			|| strncmp(rl_line_buffer, "open ", 5) == 0)) {
				ret = -1;
				switch (ent->d_type) {

				case DT_LNK:

					if (dirname[0] == '.' && !dirname[1])
						ret = get_link_ref(ent->d_name);

					else {
						snprintf(tmp, PATH_MAX, "%s%s", dirname,
								 ent->d_name);
						ret = get_link_ref(tmp);
					}

					if (ret == S_IFDIR || ret == S_IFREG)
						match = 1;

					break;

				case DT_REG:
				case DT_DIR:
					match = 1;
					break;

				default:
					break;
				}
			}

			/* If 'trash', allow only reg files, dirs, symlinks, pipes
			 * and sockets. You should not trash a block or a character
			 * device */
			else if (*rl_line_buffer == 't'
			&& (strncmp(rl_line_buffer, "t ", 2) == 0
			|| strncmp(rl_line_buffer, "tr ", 2) == 0
			|| strncmp(rl_line_buffer, "trash ", 6) == 0)) {

				if (ent->d_type != DT_BLK && ent->d_type != DT_CHR)
					match = 1;
			}

			/* If "./", list only executable regular files */
			else if (exec) {

				if (ent->d_type == DT_REG
				&& access(ent->d_name, X_OK) == 0)
					match = 1;
			}

			/* If "/path/./", list only executable regular files */
			else if (exec_path) {

				if (ent->d_type == DT_REG) {
					/* dir_tmp is dirname less "./", already
					 * allocated before the while loop */
					snprintf(tmp, PATH_MAX, "%s%s", dir_tmp,
							 ent->d_name);

					if (access(tmp, X_OK) == 0)
						match = 1;
				}
			}

			/* No filter for everything else. Just print whatever is
			 * there */
			else
				match = 1;
		}

		/* If there is at least one char to complete (ex: "cd .[TAB]") */
		else {
			/* Check if possible completion match up to the length of
			 * filename. */
			if (case_sens_path_comp) {
				if (*ent->d_name != *filename
				|| (strncmp(filename, ent->d_name, filename_len) != 0))
					continue;
			}
			else {
				if (TOUPPER(*ent->d_name) != TOUPPER(*filename)
				|| (strncasecmp(filename, ent->d_name, filename_len) != 0))
					continue;
			}

			if (*rl_line_buffer == 'c'
			&& strncmp(rl_line_buffer, "cd ", 3) == 0) {
				ret = -1;
				switch (ent->d_type) {
				case DT_LNK:

					if (dirname[0] == '.' && !dirname[1])
						ret = get_link_ref(ent->d_name);

					else {
						snprintf(tmp, PATH_MAX, "%s%s", dirname,
								 ent->d_name);
						ret = get_link_ref(tmp);
					}

					if (ret == S_IFDIR)
						match = 1;

					break;

				case DT_DIR:
					match = 1;
					break;

				default:
					break;
				}
			}

			else if (*rl_line_buffer == 'o'
			&& (strncmp(rl_line_buffer, "o ", 2) == 0
			|| strncmp(rl_line_buffer, "open ", 5) == 0)) {
				ret = -1;
				switch (ent->d_type) {
				case DT_REG: /* fallthrough */
				case DT_DIR:
					match = 1;
					break;

				case DT_LNK:

					if (dirname[0] == '.' && !dirname [1]) {
						ret = get_link_ref(ent->d_name);
					}

					else {
						snprintf(tmp, PATH_MAX, "%s%s", dirname,
								 ent->d_name);
						ret = get_link_ref(tmp);
					}

					if (ret == S_IFDIR || ret == S_IFREG)
						match = 1;

					break;

				default:
					break;
				}
			}

			else if (*rl_line_buffer == 't'
			&& (strncmp(rl_line_buffer, "t ", 2) == 0
			|| strncmp(rl_line_buffer, "tr ", 3) == 0
			|| strncmp(rl_line_buffer, "trash ", 6) == 0)) {

				if (ent->d_type != DT_BLK
				&& ent->d_type != DT_CHR)
					match = 1;
			}

			else if (exec) {

				if (ent->d_type == DT_REG
				&& access(ent->d_name, X_OK) == 0)
					match = 1;
			}

			else if (exec_path) {

				if (ent->d_type == DT_REG) {
					snprintf(tmp, PATH_MAX, "%s%s", dir_tmp, ent->d_name);
					if (access(tmp, X_OK) == 0)
						match = 1;
				}
			}

			else
				match = 1;
		}

		if (match)
			break;
	}

	if (dir_tmp) { /* == exec_path */
		free(dir_tmp);
		dir_tmp = (char *)NULL;
	}

	/* readdir() returns NULL on reaching the end of directory stream.
	 * So that if entry is NULL, we have no matches */

	if (!ent) { /* == !match */
		if (directory) {
			closedir(directory);
			directory = (DIR *)NULL;
		}

		if (dirname) {
			free(dirname);
			dirname = (char *)NULL;
		}

		if (filename) {
			free(filename);
			filename = (char *)NULL;
		}

		if (users_dirname) {
			free(users_dirname);
			users_dirname = (char *)NULL;
		}

		return (char *)NULL;
	}

	/* We have a match */
	else {
		char *temp = (char *)NULL;

		/* dirname && (strcmp(dirname, ".") != 0) */
		if (dirname && (dirname[0] != '.' || dirname[1])) {

			if (rl_complete_with_tilde_expansion
			&& *users_dirname == '~') {
				size_t dirlen = strlen(dirname);
				temp = (char *)xcalloc(dirlen + strlen(ent->d_name)
									   + 2, sizeof(char));
				strcpy(temp, dirname);
				/* Canonicalization cuts off any final slash present.
				 * We need to add it back. */

				if (dirname[dirlen - 1] != '/') {
					temp[dirlen] = '/';
					temp[dirlen + 1] = '\0';
				}
			}

			else {
				  temp = (char *)xcalloc(strlen(users_dirname) +
										 strlen(ent->d_name) + 1, sizeof(char));
				  strcpy(temp, users_dirname);
			}
			strcat(temp, ent->d_name);
		}

		else
			temp = savestring(ent->d_name, strlen(ent->d_name));

		return (temp);
	}
}

static char *
hist_generator(const char *text, int state)
/* Used by history completion */
{
	static int i;
	static size_t len;
	char *name;

	if (!state) {
		i = 0;
		len = strlen(text);
	}

	/* Look for cmd history entries for a match */
	while ((name = history[i++]) != NULL) {
		if (strncmp(name, text, len) == 0)
			return strdup(name);
	}

	return (char *)NULL;
}

static char *
jump_generator(const char *text, int state)
/* Expand string into matching path in the jump database. Used by
 * j, jc, and jp commands */
{
	static int i;
	char *name;

	if (!state)
		i = 0;

	if (!jump_db)
		return (char *)NULL;

	/* Look for matches in the dirhist list */
	while ((name = jump_db[i++].path) != NULL) {

		/* Exclude CWD */
		if (name[1] == ws[cur_ws].path[1]
		&& strcmp(name, ws[cur_ws].path) == 0)
			continue;

		/* Filter by parent */
		if (rl_line_buffer[1] == 'p') {
			if (!strstr(ws[cur_ws].path, name))
				continue;
		}

		/* Filter by child */
		else if (rl_line_buffer[1] == 'c') {
			if (!strstr(name, ws[cur_ws].path))
				continue;
		}

		if (strstr(name, text))
			return strdup(name);
	}

	return (char *)NULL;
}

static char *
jump_entries_generator(const char *text, int state)
/* Expand jump order number into the corresponding path. Used by the
 * jo command */
{
	static size_t i;
	char *name;

	if (!state)
		i=0;

	int num_text = atoi(text);

	/* Check list of jump entries for a match */
	while (i <= jump_n && (name = jump_db[i++].path) != NULL)
		if (*name == *jump_db[num_text - 1].path
		&& strcmp(name, jump_db[num_text - 1].path) == 0)
			return strdup(name);

	return (char *)NULL;
}

static char *
cschemes_generator(const char *text, int state)
{
	static int i;
	static size_t len;
	char *name;

	if (!state) {
		i = 0;
		len = strlen(text);
	} /* The state variable is zero only the first time the function is
	called, and a non-zero positive in later calls. This means that i
	and len will be necessarilly initialized the first time */

	if (!color_schemes)
		return (char *)NULL;

	/* Look for color schemes in color_schemes for a match */
	while ((name = color_schemes[i++]) != NULL) {
		if (strncmp(name, text, len) == 0)
			return strdup(name);
	}

	return (char *)NULL;
}

static char *
profiles_generator(const char *text, int state)
/* Used by profiles completion */
{
	static int i;
	static size_t len;
	char *name;

	if (!state) {
		i = 0;
		len = strlen(text);
	} /* The state variable is zero only the first time the function is
	called, and a non-zero positive in later calls. This means that i
	and len will be necessarilly initialized the first time */

	/* Look for profiles in profile_names for a match */
	while ((name = profile_names[i++]) != NULL) {
		if (strncmp(name, text, len) == 0)
			return strdup(name);
	}

	return (char *)NULL;
}

static char *
bookmarks_generator(const char *text, int state)
/* Used by bookmarks completion */
{
	static int i;
	static size_t len;
	char *name;

	if (!state) {
		i = 0;
		len = strlen(text);
	}

	/* Look for bookmarks in bookmark names for a match */
	while ((name = bookmark_names[i++]) != NULL) {
		if (strncmp(name, text, len) == 0)
			return strdup(name);
	}

	return (char *)NULL;
}

static char *
filenames_gen_text(const char *text, int state)
/* Used by ELN expansion */
{
	static size_t i, len = 0;
	char *name;
	rl_filename_completion_desired = 1;
	/* According to the GNU readline documention: "If it is set to a
	 * non-zero value, directory names have a slash appended and
	 * Readline attempts to quote completed filenames if they contain
	 * any embedded word break characters." To make the quoting part
	 * work I had to specify a custom quoting function (my_rl_quote) */
	if (!state) { /* state is zero only the first time readline is
	executed */
		i = 0;
		len = strlen(text);
	}

	/* Check list of currently displayed files for a match */
	while (i < files && (name = file_info[i++].name) != NULL)
		if (case_sens_path_comp ? strncmp(name, text, len) == 0
		: strncasecmp(name, text, len) == 0)
			return strdup(name);

	return (char *)NULL;
}

static char *
filenames_gen_eln(const char *text, int state)
/* Used by ELN expansion */
{
	static size_t i;
	char *name;
	rl_filename_completion_desired = 1;

	if (!state)
		i = 0;

	int num_text = atoi(text);

	/* Check list of currently displayed files for a match */
	while (i < files && (name = file_info[i++].name) != NULL)
		if (*name == *file_info[num_text - 1].name
		&& strcmp(name, file_info[num_text - 1].name) == 0)
			return strdup(name);

	return (char *)NULL;
}

static char *
bin_cmd_generator(const char *text, int state)
/* Used by commands completion */
{
	static int i;
	static size_t len;
	char *name;

	if (!state) {
		i = 0;
		len = strlen(text);
	}

	while ((name = bin_commands[i++]) != NULL) {
		if (*text == *name && strncmp(name, text, len) == 0)
			return strdup(name);
	}

	return (char *)NULL;
}

static char **
my_rl_completion(const char *text, int start, int end)
{
	char **matches = (char **)NULL;

	if (start == 0) { /* Only for the first word entered in the prompt */

		/* Commands completion */
		if (end == 0) { /* If text is empty, do nothing */
			/* Prevent readline from attempting path completion if
			* rl_completion matches returns NULL */
			rl_attempted_completion_over = 1;
			return (char **)NULL;
		}

		/* History cmd completion */
		if (*text == '!')
			matches = rl_completion_matches(text + 1, &hist_generator);

		/* If autocd or auto-open, try to expand ELN's first */
		if (!matches && (autocd || auto_open)) {
			if (*text >= '1' && *text <= '9') {
				int num_text = atoi(text);

				if (is_number(text) && num_text > 0 && num_text <= (int)files)
					matches = rl_completion_matches(text, &filenames_gen_eln);
			}

			if (!matches && *text != '/') {
				matches = rl_completion_matches(text, &filenames_gen_text);
			}
		}

		/* Bookmarks completion */
		if (!matches && (autocd || auto_open) && expand_bookmarks)
			matches = rl_completion_matches(text, &bookmarks_generator);

		/* If neither autocd nor auto-open, try to complete with
		 * command names */
		if (!matches)
			matches = rl_completion_matches(text, &bin_cmd_generator);
	}

	/* Second word or more */
	else {

			/* #### ELN AND JUMP ORDER EXPANSION ### */

		/* Perform this check only if the first char of the string to be
		 * completed is a number in order to prevent an unnecessary call
		 * to atoi */
		if (*text >= '1' && *text <= '9') {

			int num_text = atoi(text);

					/* Dirjump: jo command */
			if (*rl_line_buffer == 'j' && rl_line_buffer[1] == 'o'
			&& rl_line_buffer[2] == ' ') {
				if (is_number(text) && num_text > 0
				&& num_text <= (int)jump_n) {
					matches = rl_completion_matches(text,
							  &jump_entries_generator);
				}
			}

					/* ELN expansion */
			else if (is_number(text) && num_text > 0
			&& num_text <= (int)files)
				matches = rl_completion_matches(text, &filenames_gen_eln);
		}

				/* ### DIRJUMP COMPLETION ### */
					/* j, jc, jp commands */
		else if (*rl_line_buffer == 'j' && (rl_line_buffer[1] == ' '
		|| ((rl_line_buffer[1] == 'c' || rl_line_buffer[1] == 'p')
		&& rl_line_buffer[2] == ' ')
		|| strncmp(rl_line_buffer, "jump ", 5) == 0))

			matches = rl_completion_matches(text, &jump_generator);

				/* ### BOOKMARKS COMPLETION ### */

		else if (*rl_line_buffer == 'b'
		&& (rl_line_buffer[1] == 'm' || rl_line_buffer[1] == 'o')
		&& (strncmp(rl_line_buffer, "bm ", 3) == 0
		|| strncmp(rl_line_buffer, "bookmarks ", 10) == 0)) {
			rl_attempted_completion_over = 1;
			matches = rl_completion_matches(text, &bookmarks_generator);
		}

				/* ### COLOR SCHEMES COMPLETION ### */
		else if (*rl_line_buffer == 'c' && ((rl_line_buffer[1] == 's'
		&& rl_line_buffer[2] == ' ')
		|| strncmp(rl_line_buffer, "colorschemes ", 13) == 0)) {
				matches = rl_completion_matches(text,
						  &cschemes_generator);
		}

				/* ### PROFILES COMPLETION ### */

		else if (*rl_line_buffer == 'p'
		&& (rl_line_buffer[1] == 'r' || rl_line_buffer[1] == 'f')
		&& (strncmp(rl_line_buffer, "pf set ", 7) == 0
		|| strncmp(rl_line_buffer, "profile set ", 12) == 0
		|| strncmp(rl_line_buffer, "pf del ", 7) == 0
		|| strncmp(rl_line_buffer, "profile del ", 12) == 0)) {
			rl_attempted_completion_over = 1;
			matches = rl_completion_matches(text, &profiles_generator);
		}

		else if (expand_bookmarks) {
			matches = rl_completion_matches(text, &bookmarks_generator);
		}
	}

				/* ### PATH COMPLETION ### */

	/* If none of the above, readline will attempt
	 * path completion instead via my custom my_rl_path_completion() */
	return matches;
}

static int
pin_directory(char *dir)
{
	if (!dir || !*dir)
		return EXIT_FAILURE;

	struct stat attr;

	if (lstat(dir, &attr) == -1) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, dir,
				strerror(errno));
		return EXIT_FAILURE;
	}

	if (pinned_dir) {
		free(pinned_dir);
		pinned_dir = (char *)NULL;
	}

	/* If absolute path */
	if (*dir == '/')
		pinned_dir =  savestring(dir, strlen(dir));

	else { /* If relative path */

		if (strcmp(ws[cur_ws].path, "/") == 0) {
			pinned_dir = (char *)xnmalloc(strlen(dir) + 2, sizeof(char));
			sprintf(pinned_dir, "/%s", dir);
		}

		else {
			pinned_dir = (char *)xnmalloc(strlen(dir)
						+ strlen(ws[cur_ws].path) + 2, sizeof(char));
			sprintf(pinned_dir, "%s/%s", ws[cur_ws].path, dir);
		}
	}

	printf(_("%s: Succesfully pinned '%s'\n"), PROGRAM_NAME, dir);

	return EXIT_SUCCESS;
}

static int
unpin_dir(void)
{
	if (!pinned_dir) {
		printf(_("%s: No pinned file\n"), PROGRAM_NAME);
		return EXIT_SUCCESS;
	}

	if (CONFIG_DIR && xargs.stealth_mode != 1) {

		int cmd_error = 0;
		char *pin_file = (char *)xnmalloc(strlen(CONFIG_DIR) + 7,
											  sizeof(char));
		sprintf(pin_file, "%s/.pin", CONFIG_DIR);

		if (unlink(pin_file) == -1) {
			fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, pin_file,
					strerror(errno));
			cmd_error = 1;
		}

		free(pin_file);

		if (cmd_error)
			return EXIT_FAILURE;
	}

	printf(_("Succesfully unpinned %s\n"), pinned_dir);

	free(pinned_dir);
	pinned_dir = (char *)NULL;

	return EXIT_SUCCESS;
}

static int
cschemes_function(char **args)
{
	if (xargs.stealth_mode == 1) {
		fprintf(stderr, _("%s: The color schemes function is "
				"disabled in stealth mode\nTIP: To change the current "
				"color scheme use the following environment "
				"variables: CLIFM_FILE_COLORS, CLIFM_IFACE_COLORS, "
				"and CLIFM_EXT_COLORS\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	if (!args)
		return EXIT_FAILURE;

	if (!args[1]) {

		if (!cschemes_n) {
			printf(_("%s: No color schemes found\n"), PROGRAM_NAME);
			return EXIT_SUCCESS;
		}

		size_t i;

		for (i = 0; color_schemes[i]; i++) {

			if (cur_cscheme == color_schemes[i])
				printf("%s%s%s\n", mi_c, color_schemes[i], df_c);

			else
				printf("%s\n", color_schemes[i]);
		}

		return EXIT_SUCCESS;
	}

	if (*args[1] == '-' && strcmp(args[1], "--help") == 0) {
		puts(_("Usage: cs, colorschemes [edit] [COLORSCHEME]"));
		return EXIT_SUCCESS;
	}

	if (*args[1] == 'e' && (!args[1][1]
	|| strcmp(args[1], "edit") == 0)) {
		char file[PATH_MAX] = "";
		sprintf(file, "%s/%s.cfm", COLORS_DIR, cur_cscheme);

		struct stat attr;
		stat(file, &attr);
		time_t mtime_bfr = (time_t)attr.st_mtime;

		char *cmd[] = { "mm", file, NULL };
		int ret = mime_open(cmd);

		if (ret != EXIT_FAILURE) {

			stat(file, &attr);

			if (mtime_bfr != (time_t)attr.st_mtime
			&& set_colors(cur_cscheme, 0) == EXIT_SUCCESS
			&& cd_lists_on_the_fly) {

				free_dirlist();
				list_dir();
			}
		}

		return ret;

	}

	if (*args[1] == 'n' && (!args[1][1]
	|| strcmp(args[1], "name") == 0)) {
		printf(_("%s: current color scheme: %s\n"), PROGRAM_NAME,
			   cur_cscheme ? cur_cscheme : "?");
		return EXIT_SUCCESS;
	}

	size_t i, cs_found = 0;

	for (i = 0; color_schemes[i]; i++) {
		if (*args[1] == *color_schemes[i]
		&& strcmp(args[1], color_schemes[i]) == 0) {

			cs_found = 1;

			if (set_colors(args[1], 0) == EXIT_SUCCESS) {

				cur_cscheme = color_schemes[i];

				switch_cscheme = 1;

				if (cd_lists_on_the_fly) {
					free_dirlist();
					list_dir();
				}

				switch_cscheme = 0;

				return EXIT_SUCCESS;
			}
		}
	}

	if (!cs_found)
		fprintf(stderr, _("%s: No such color scheme\n"), PROGRAM_NAME);

	return EXIT_FAILURE;
}

static int
edit_jumpdb(void)
{
	if (!config_ok || !CONFIG_DIR)
		return EXIT_FAILURE;

	save_jumpdb();

	char *JUMP_FILE = (char *)xnmalloc(strlen(CONFIG_DIR) + 10,
									   sizeof(char));
	sprintf(JUMP_FILE, "%s/jump.cfm", CONFIG_DIR);

	struct stat attr;

	if (stat(JUMP_FILE, &attr) == -1) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, JUMP_FILE,
				strerror(errno));
		free(JUMP_FILE);
		return EXIT_FAILURE;
	}

	time_t mtime_bfr = (time_t)attr.st_mtime;

	char *cmd[] = { "o", JUMP_FILE, NULL };
	open_function(cmd);

	stat(JUMP_FILE, &attr);

	if (mtime_bfr == (time_t)attr.st_mtime) {
		free(JUMP_FILE);
		return EXIT_SUCCESS;
	}

	if (jump_db) {
		int i = (int)jump_n;

		while (--i >= 0)
			free(jump_db[i].path);

		free(jump_db);
		jump_db = (struct jump_t *)NULL;
		jump_n = 0;
	}

	load_jumpdb();

	free(JUMP_FILE);

	return EXIT_SUCCESS;
}

static int
dirjump(char **args)
/* Jump into best ranked directory matched by ARGS */
{
	if (xargs.no_dirjump == 1) {
		printf(_("%s: Directory jumper function disabled\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	time_t now = time(NULL);

	int reduce = 0;

	/* If the sum total of ranks is greater than max, divide each entry
	 * to make the sum total less than or equal to max */
	if (jump_total_rank > max_jump_total_rank)
		reduce = (jump_total_rank / max_jump_total_rank) + 1;

	/* If no parameter, print the list of entries in the jump
	 * database together with the corresponding information */
	if (!args[1] && args[0][1] != 'e') {

		if (!jump_n) {
			printf("%s: Database still empty\n", PROGRAM_NAME);
			return EXIT_SUCCESS;
		}

		puts(_("NOTE: First time access is displayed in days, while last "
			 "time access is displayed in hours"));
		puts(_("NOTE 2: An asterisk next rank values means that the "
			 "corresponding directory is bookmarked, pinned, or currently "
			 "used in some workspace\n"));
		puts(_("Order\tVisits\tFirst\tLast\tRank\tDirectory"));

		size_t i, ranks_sum = 0, visits_sum = 0;

		for (i = 0; i < jump_n; i++) {

			int days_since_first = (int)(now - jump_db[i].first_visit) / 60 / 60 / 24;
			int hours_since_last = (int)(now - jump_db[i].last_visit) / 60 / 60;

			int rank;
			rank = days_since_first > 1
				   ? ((int)jump_db[i].visits * 100) / days_since_first
				   : (int)jump_db[i].visits * 100;

			int tmp_rank = rank;
			if (hours_since_last == 0)          /* Last hour */
				rank = JHOUR(tmp_rank);
			else if (hours_since_last <= 24)    /* Last day */
				rank = JDAY(tmp_rank);
			else if (hours_since_last <= 168)   /* Last week */
				rank = JWEEK(tmp_rank);
			else                                /* More than a week */
				rank = JOLDER(tmp_rank);

			int j = (int)bm_n, BPW = 0; /* Bookmarked, pinned or workspace */
			while (--j >= 0) {
				if (bookmarks[j].path[1] == jump_db[i].path[1]
				&& strcmp(bookmarks[j].path, jump_db[i].path) == 0) {
					rank += BOOKMARK_BONUS;
					BPW = 1;
					break;
				}
			}

			if (pinned_dir && pinned_dir[1] == jump_db[i].path[1]
			&& strcmp(pinned_dir, jump_db[i].path) == 0) {
				rank += PINNED_BONUS;
				BPW = 1;
			}

			j = MAX_WS;
			while (--j >= 0) {
				if (ws[j].path && ws[j].path[1] == jump_db[i].path[1]
				&& strcmp(jump_db[i].path, ws[j].path) == 0) {
					rank += WORKSPACE_BONUS;
					BPW = 1;
					break;
				}
			}

			if (reduce) {
				tmp_rank = rank;
				rank = tmp_rank / reduce;
			}

			ranks_sum += rank;
			visits_sum += jump_db[i].visits;

			if (ws[cur_ws].path[1] == jump_db[i].path[1]
			&& strcmp(ws[cur_ws].path, jump_db[i].path) == 0) {
				printf("  %s%zu\t %zu\t %d\t %d\t%d%c\t%s%s \n", mi_c,
					   i + 1, jump_db[i].visits, days_since_first,
					   hours_since_last, rank, BPW ? '*' : 0,
					   jump_db[i].path, df_c);
			}

			else
				printf("  %zu\t %zu\t %d\t %d\t%d%c\t%s \n", i + 1,
					   jump_db[i].visits, days_since_first,
					   hours_since_last, rank,
					   BPW ? '*' : 0, jump_db[i].path);
		}

		printf("\nTotal rank: %zu/%d\nTotal visits: %zu\n", ranks_sum,
				max_jump_total_rank, visits_sum);

		return EXIT_SUCCESS;
	}

	if (args[1] && *args[1] == '-' && strcmp(args[1], "--help") == 0) {
		puts(_("Usage: j, jc, jp, jl [STRING ...], jo [NUM], je"));
		return EXIT_SUCCESS;
	}

	enum jump jump_opt = none;

	switch(args[0][1]) {

	case 'e': return edit_jumpdb();
	case 'c': jump_opt = jchild; break;
	case 'p': jump_opt = jparent; break;
	case 'o': jump_opt = jorder; break;
	case 'l': jump_opt = jlist; break;
	case '\0':
		jump_opt = none;
		break;

	default:
		fprintf(stderr, _("%s: '%c': Invalid option\n"), PROGRAM_NAME,
				args[0][1]);
		fputs(_("Usage: j, jc, jp, jl [STRING ...], jo [NUM], je\n"),
			  stderr);
		return EXIT_FAILURE;
	break;
	}

	if (jump_opt == jorder) {

		if (!args[1]) {
			fputs(_("Usage: j, jc, jp, jl [STRING ...], jo [NUM], je\n"),
				  stderr);
			return EXIT_FAILURE;
		}

		if (!is_number(args[1]))
				return cd_function(args[1]);

		else {

			int int_order = atoi(args[1]);
			if (int_order <= 0 || int_order > (int)jump_n) {
				fprintf(stderr, _("%s: %d: No such order number\n"),
						PROGRAM_NAME, int_order);
				return EXIT_FAILURE;
			}

			return cd_function(jump_db[int_order - 1].path);
		}
	}

	/* If ARG is an actual directory, just cd into it */
	struct stat attr;

	if (!args[2] && lstat(args[1], &attr) != -1)
		return cd_function(args[1]);

	/* Jump into a visited directory using ARGS as filter(s) */
	size_t i;
	int j, match = 0;

	char **matches = (char **)xnmalloc(jump_n + 1, sizeof(char *));
	size_t *visits = (size_t *)xnmalloc(jump_n + 1, sizeof(size_t));
	time_t *first = (time_t *)xnmalloc(jump_n + 1, sizeof(time_t));
	time_t *last = (time_t *)xnmalloc(jump_n + 1, sizeof(time_t));

	for (i = 1; args[i]; i++) {

		/* 1) Using the first parameter, get a list of matches in the
		 * database */
		if (!match) {

			j = (int)jump_n;
			while (--j >= 0) {

				if (case_sens_dirjump ? !strstr(jump_db[j].path, args[i])
				: !strcasestr(jump_db[j].path, args[i]))
					continue;

				/* Exclue CWD */
				if (jump_db[j].path[1] == ws[cur_ws].path[1]
				&& strcmp(jump_db[j].path, ws[cur_ws].path) == 0)
					continue;

				int exclude = 0;

				/* Filter matches according to parent or
				 * child options */
				switch(jump_opt) {
				case jparent:
					if (!strstr(ws[cur_ws].path, jump_db[j].path))
						exclude = 1;
				break;

				case jchild:
					if (!strstr(jump_db[j].path, ws[cur_ws].path))
						exclude = 1;

				case none:
				default:
					break;
				}

				if (exclude)
					continue;

				visits[match] = jump_db[j].visits;
				first[match] = jump_db[j].first_visit;
				last[match] = jump_db[j].last_visit;
				matches[match++] = jump_db[j].path;
			}
		}

		/* 2) Once we have the list of matches, perform a reverse
		 * matching process, that is, excluding non-matches,
		 * using subsequent parameters */
		else {
			j = (int)match;
			while (--j >= 0) {

				if (!matches[j] || !*matches[j]
				|| (case_sens_dirjump ? !strstr(matches[j], args[i])
				: !strcasestr(matches[j], args[i]))) {

					matches[j] = (char *)NULL;
					continue;
				}
			}
		}
	}

	/* 3) If something remains, we have at least one match */

	/* 4) Further filter the list of matches by frecency, so that only
	 * the best ranked directory will be returned */

	int found = 0, exit_status = EXIT_FAILURE,
		best_ranked = 0, max = -1, k;

	j = match;
	while (--j >= 0) {

		if (!matches[j])
			continue;

		found = 1;

		if (jump_opt == jlist)
			printf("%s\n", matches[j]);

		else {
			int days_since_first = (int)(now - first[j]) / 60 / 60 / 24;

			/* Calculate the rank as frecency. The algorithm is based
			 * on Mozilla, zoxide, and z.lua. See:
			 * "https://wiki.mozilla.org/User:Mconnor/Past/PlacesFrecency"
			 * "https://github.com/ajeetdsouza/zoxide/wiki/Algorithm#aging"
			 * "https://github.com/skywind3000/z.lua#aging" */
			int rank;
			rank = days_since_first > 0 ? ((int)visits[j] * 100) / days_since_first
							: ((int)visits[j] * 100);

			int hours_since_last = (int)(now - last[j]) / 60 / 60;

			/* Credit or penalty based on last directory access */
			int tmp_rank = rank;
			if (hours_since_last == 0)          /* Last hour */
				rank = JHOUR(tmp_rank);
			else if (hours_since_last <= 24)    /* Last day */
				rank = JDAY(tmp_rank);
			else if (hours_since_last <= 168)   /* Last week */
				rank = JWEEK(tmp_rank);
			else                                /* More than a week */
				rank = JOLDER(tmp_rank);

			/* Matches in directory basename have extra credit */
			char *tmp = strrchr(matches[j], '/');
			if (tmp && *(++tmp)) {
				if (strstr(tmp, args[args_n]))
					rank += BASENAME_BONUS;
			}

			/* Bookmarked directories have extra credit */
			k = (int)bm_n;
			while (--k >= 0) {
				if (bookmarks[k].path[1] == matches[j][1]
				&& strcmp(bookmarks[k].path, matches[j]) == 0) {
					rank += BOOKMARK_BONUS;
					break;
				}
			}

			if (pinned_dir && pinned_dir[1] == matches[j][1]
			&& strcmp(pinned_dir, matches[j]) == 0)
				rank += PINNED_BONUS;

			k = MAX_WS;
			while (--k >= 0) {
				if (ws[k].path && ws[k].path[1] == matches[j][1]
				&& strcmp(ws[k].path, matches[j]) == 0) {
					rank += WORKSPACE_BONUS;
					break;
				}
			}

			if (reduce) {
				tmp_rank = rank;
				rank = tmp_rank / reduce;
			}

			if (rank > max) {
				max = rank;
				best_ranked = j;
			}
		}
	}

	if (!found) {
		printf(_("%s: No matches found\n"), PROGRAM_NAME);
		exit_status = EXIT_FAILURE;
	}

	else if (jump_opt != jlist)
		exit_status = cd_function(matches[best_ranked]);

	free(matches);
	free(first);
	free(last);
	free(visits);

	return exit_status;
}

static int
workspaces(char *str)
{
	if (!str || !*str) {
		int i;
		for (i = 0; i < MAX_WS; i++) {
			if (i == cur_ws)
				printf("%s%d: %s%s\n", mi_c, i + 1, ws[i].path, df_c);
			else
				printf("%d: %s\n", i + 1, ws[i].path ? ws[i].path
					   : "none");
		}

		return EXIT_SUCCESS;
	}

	if (*str == '-' && strcmp(str, "--help") == 0) {
		puts(_("Usage: ws [NUM, +, -]"));
		return EXIT_SUCCESS;
	}

	int tmp_ws = 0;

	if (is_number(str)) {
		int istr = atoi(str);

		if (istr <= 0 || istr > MAX_WS) {
			fprintf(stderr, _("%s: %d: Invalid workspace number\n"),
					PROGRAM_NAME, istr);
			return EXIT_FAILURE;
		}

		tmp_ws = istr - 1;

		if (tmp_ws == cur_ws)
			return EXIT_FAILURE;
	}

	else if (*str == '+' && !str[1]) {
		if ((cur_ws + 1) < MAX_WS)
			tmp_ws = cur_ws + 1;
		else
			return EXIT_FAILURE;
	}

	else if (*str == '-' && !str[1]) {
		if ((cur_ws - 1) >= 0)
			tmp_ws = cur_ws - 1;
		else
			return EXIT_FAILURE;
	}

	/* If new workspace has no path yet, copy the path of the current
	 * workspace */
	if (!ws[tmp_ws].path)
		ws[tmp_ws].path = savestring(ws[cur_ws].path,
									 strlen(ws[cur_ws].path));

	if (xchdir(ws[tmp_ws].path, SET_TITLE) == -1) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, ws[tmp_ws].path,
				strerror(errno));
		return EXIT_FAILURE;
	}

	cur_ws = tmp_ws;

	int exit_status =  EXIT_SUCCESS;

	if (cd_lists_on_the_fly) {
		free_dirlist();
		exit_status = list_dir();
	}

	add_to_dirhist(ws[cur_ws].path);

	return exit_status;
}

static int
save_sel(void)
/* Save selected elements into a tmp file. Returns 1 if success and 0
 * if error. This function allows the user to work with multiple
 * instances of the program: he/she can select some files in the
 * first instance and then execute a second one to operate on those
 * files as he/she whises. */
{
	if (!selfile_ok || !config_ok)
		return EXIT_FAILURE;

	if (sel_n == 0) {
		if (unlink(SEL_FILE) == -1) {
			fprintf(stderr, "%s: sel: %s: %s\n", PROGRAM_NAME,
					SEL_FILE, strerror(errno));
			return EXIT_FAILURE;
		}
		else
			return EXIT_SUCCESS;
	}

	FILE *sel_fp = fopen(SEL_FILE, "w");

	if (!sel_fp) {
		_err(0, NOPRINT_PROMPT, "%s: sel: %s: %s\n", PROGRAM_NAME,
			 SEL_FILE, strerror(errno));
		return EXIT_FAILURE;
	}

	size_t i;
	for (i = 0; i < sel_n; i++) {
		fputs(sel_elements[i], sel_fp);
		fputc('\n', sel_fp);
	}

	fclose(sel_fp);

	return EXIT_SUCCESS;
}

/* MOVE TO CLIFM.H */
static int
check_regex(char *str)
{
	if (!str || !*str)
		return EXIT_FAILURE;

	int char_found = 0;
	char *p = str;

	while (*p) {
		/* If STR contains at least one of the following chars */
		if (*p == '*' || *p == '?' || *p == '[' || *p == '{'
		|| *p == '^' || *p == '.' || *p == '|' || *p == '+'
		|| *p == '$') {
			char_found = 1;
			break;
		}

		p++;
	}

	/* And if STR is not a filename, take it as a possible regex */
	if (char_found)
		if (access(str, F_OK) == -1)
			return EXIT_SUCCESS;

	return EXIT_FAILURE;
}

static int
select_file(char *file)
{
	if (!file || !*file)
		return 0;

	/* Check if the selected element is already in the selection
	 * box */
	int exists = 0, new_sel = 0, j;

	j = (int)sel_n;
	while (--j >= 0) {
		if (*file == *sel_elements[j]
		&& strcmp(sel_elements[j], file) == 0) {
			exists = 1;
			break;
		}
	}

	if (!exists) {
		sel_elements = (char **)xrealloc(sel_elements, (sel_n + 2)
										 * sizeof(char *));
		sel_elements[sel_n++] = savestring(file, strlen(file));
		sel_elements[sel_n] = (char *)NULL;
		new_sel++;
	}

	else
		fprintf(stderr, _("%s: sel: %s: Already selected\n"), 
				PROGRAM_NAME, file);

	return new_sel;
}

static int
sel_regex(char *str, const char *sel_path, mode_t filetype)
{
	if (!str || !*str)
		return -1;

	char *pattern = str;

	int invert = 0;
	if (*pattern == '!') {
		pattern++;
		invert = 1;
	}

	regex_t regex;
	if (regcomp(&regex, pattern, REG_NOSUB|REG_EXTENDED)
	!= EXIT_SUCCESS) {
		fprintf(stderr, _("%s: sel: %s: Invalid regular "
				"expression\n"), PROGRAM_NAME, str);

		regfree(&regex);
		return -1;
	}

	int new_sel = 0, i;

	if (!sel_path) { /* Check pattern (STR) against files in CWD */
		i = (int)files;
		while (--i >= 0) {

			if (filetype && file_info[i].type != filetype)
				continue;

			char tmp_path[PATH_MAX];
			sprintf(tmp_path, "%s/%s", ws[cur_ws].path, file_info[i].name);

			if (regexec(&regex, file_info[i].name, 0, NULL, 0)
			== EXIT_SUCCESS) {
				if (!invert)
					new_sel += select_file(tmp_path);
			}
			else if (invert)
				new_sel += select_file(tmp_path);
		}
	}

	else { /* Check pattern against files in SEL_PATH */

		struct dirent **list = (struct dirent **)NULL;
		int filesn = scandir(sel_path, &list, skip_files, xalphasort);

		if (filesn == -1) {
			fprintf(stderr, "sel: %s: %s\n", sel_path, strerror(errno));
			return -1;
		}

		mode_t t = 0;
		if (filetype) {

			switch(filetype) {
			case DT_DIR: t = S_IFDIR; break;
			case DT_REG: t = S_IFREG; break;
			case DT_LNK: t = S_IFLNK; break;
			case DT_SOCK: t = S_IFSOCK; break;
			case DT_FIFO: t = S_IFIFO; break;
			case DT_BLK: t = S_IFBLK; break;
			case DT_CHR: t = S_IFCHR; break;
			}
		}

		i = (int)filesn;
		while (--i >= 0) {

			if (filetype) {
				struct stat attr;

				if (lstat(list[i]->d_name, &attr) != -1) {
					if ((attr.st_mode & S_IFMT) != t) {
						free(list[i]);
						continue;
					}
				}
			}

			char *tmp_path =  (char *)xnmalloc(strlen(sel_path)
										+ strlen(list[i]->d_name)
										+ 2, sizeof(char));
			sprintf(tmp_path, "%s/%s", sel_path, list[i]->d_name);

			if (regexec(&regex, list[i]->d_name, 0, NULL, 0)
			== EXIT_SUCCESS) {
				if (!invert)
					new_sel += select_file(tmp_path);
			}

			else if (invert)
				new_sel += select_file(tmp_path);

			free(tmp_path);
			free(list[i]);
		}

		free(list);
	}

	regfree(&regex);

	return new_sel;
}

static off_t
dir_size(char *dir)
{
	char *rand_ext = gen_rand_str(6);
	if (!rand_ext)
		return -1;

	char DU_TMP_FILE[15];
	sprintf(DU_TMP_FILE, "/tmp/du.%s", rand_ext);
	free(rand_ext);

	if (!dir)
		return -1;

	FILE *du_fp = fopen(DU_TMP_FILE, "w");
	int stdout_bk = dup(STDOUT_FILENO); /* Save original stdout */
	dup2(fileno(du_fp), STDOUT_FILENO); /* Redirect stdout to the desired
	file */ 
	fclose(du_fp);

/*  char *cmd = (char *)NULL;
	cmd = (char *)xcalloc(strlen(dir) + 10, sizeof(char));
	sprintf(cmd, "du -sh '%s'", dir);
	//int ret = launch_execle(cmd);
	launch_execle(cmd);
	free(cmd); */

	char *cmd[] = { "du", "--block-size=1", "-s", dir, NULL };
	launch_execve(cmd, FOREGROUND, E_NOSTDERR);

	dup2(stdout_bk, STDOUT_FILENO); /* Restore original stdout */
	close(stdout_bk);

/*  if (ret != 0) {
		puts("???");
		return;
	} */

	off_t retval = -1;

	if (access(DU_TMP_FILE, F_OK) == 0) {

		du_fp = fopen(DU_TMP_FILE, "r");

		if (du_fp) {
			/* I only need here the first field of the line, which is a
			 * file size and could only take a few bytes, so that 32
			 * bytes is more than enough */
			char line[32] = "";
			fgets(line, (int)sizeof(line), du_fp);

			char *file_size = strbfr(line, '\t');

			if (file_size) {
				retval = (off_t)atoll(file_size);
				free(file_size);
			}

			fclose(du_fp);
		}

		unlink(DU_TMP_FILE);
	}

	return retval;
}

static int
sel_glob(char *str, const char *sel_path, mode_t filetype)
{
	if (!str || !*str)
		return -1;

	glob_t gbuf;
	char *pattern = str;
	int invert = 0, ret = -1;

	if (*pattern == '!') {
		pattern++;
		invert = 1;
	}

	ret = glob(pattern, 0, NULL, &gbuf);

	if (ret == GLOB_NOSPACE || ret == GLOB_ABORTED) {
		globfree(&gbuf);
		return -1;
	}

	if (ret == GLOB_NOMATCH) {
		globfree(&gbuf);
		return 0;
	}

	char **matches = (char **)NULL;
	int i, j = 0, k = 0;
	struct dirent **ent = (struct dirent **)NULL;

	if (invert) {
		if (!sel_path) {
			matches = (char **)xnmalloc(files + 2, sizeof(char *));

			i = (int)files;
			while (--i >= 0) {

				if (filetype && file_info[i].type != filetype)
					continue;

				int found = 0;
				j = (int)gbuf.gl_pathc;
				while (--j >= 0) {
					if (*file_info[i].name == *gbuf.gl_pathv[j]
					&& strcmp(file_info[i].name, gbuf.gl_pathv[j]) == 0) {
						found = 1;
						break;
					}
				}

				if (!found)
					matches[k++] = file_info[i].name;
			}
		}

		else {
			ret = scandir(sel_path, &ent, skip_files, xalphasort);

			if (ret == -1) {
				fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME,
						sel_path, strerror(errno));
				globfree(&gbuf);
				return -1;
			}

			matches = (char **)xnmalloc((size_t)ret + 2, sizeof(char *));

			i = ret;
			while (--i >= 0) {
				if (filetype && ent[i]->d_type != filetype)
					continue;

				j = (int)gbuf.gl_pathc;
				while (--j >= 0) {
					if (*ent[i]->d_name == *gbuf.gl_pathv[j]
					&& strcmp(ent[i]->d_name, gbuf.gl_pathv[j]) == 0)
						break;
				}

				if (!gbuf.gl_pathv[j])
					matches[k++] = ent[i]->d_name;
			}
		}
	}

	else {
		matches = (char **)xnmalloc(gbuf.gl_pathc + 2,
									sizeof(char *));
		mode_t t = 0;
		if (filetype) {

			switch(filetype) {
			case DT_DIR: t = S_IFDIR; break;
			case DT_REG: t = S_IFREG; break;
			case DT_LNK: t = S_IFLNK; break;
			case DT_SOCK: t = S_IFSOCK; break;
			case DT_FIFO: t = S_IFIFO; break;
			case DT_BLK: t = S_IFBLK; break;
			case DT_CHR: t = S_IFCHR; break;
			}
		}

		i = (int)gbuf.gl_pathc;
		while (--i >= 0) {

			/* We need to run stat(3) here, so that the d_type macros
			 * won't work: convert them into st_mode macros */
			if (filetype) {
				struct stat attr;
				if (lstat(gbuf.gl_pathv[i], &attr) == -1)
					continue;

				if ((attr.st_mode & S_IFMT) != t)
					continue;
			}

			if (*gbuf.gl_pathv[i] == '.' && (!gbuf.gl_pathv[i][1]
			|| (gbuf.gl_pathv[i][1] == '.' && !gbuf.gl_pathv[i][2])))
				continue;

			matches[k++] = gbuf.gl_pathv[i];
		}
	}

	matches[k] = (char *)NULL;
	int new_sel = 0;

	i = k;
	while (--i >= 0) {

		if (!matches[i])
			continue;
		
		if (!sel_path) {

			if (*matches[i] == '/')
				new_sel += select_file(matches[i]);

			else {
				char *tmp = (char *)xnmalloc(strlen(ws[cur_ws].path)
									+ strlen(matches[i]) + 2, sizeof(char));
				sprintf(tmp, "%s/%s", ws[cur_ws].path, matches[i]);
				new_sel += select_file(tmp);
				free(tmp);
			}
		}

		else {
			char *tmp = (char *)xnmalloc(strlen(sel_path)
							+ strlen(matches[i]) + 2, sizeof(char));
			sprintf(tmp, "%s/%s", sel_path, matches[i]);
			new_sel += select_file(tmp);
			free(tmp);
		}
	}

	free(matches);
	globfree(&gbuf);

	if (invert && sel_path) {
		i = ret;
		while (--i >= 0)
			free(ent[i]);
		free(ent);
	}

	return new_sel;
}

static int
sel_function(char **args)
{
	if (!args)
		return EXIT_FAILURE;

	if (!args[1] || (*args[1] == '-' && args[1][1] == '-'
	&& strcmp(args[1], "--help") == 0)) {
		puts(_("Usage: s, sel ELN/FILE... [[!]PATTERN] [-FILETYPE] "
			   "[:PATH]"));
		return EXIT_SUCCESS;
	}

	char *sel_path = (char *)NULL;
	mode_t filetype = 0;
	int i, ifiletype = 0, isel_path = 0, new_sel = 0;
	
	for (i = 1; args[i]; i++) {

		if (*args[i] == '-') {
			ifiletype = i;
			filetype = (mode_t)*(args[i] + 1);
		}

		if (*args[i] == ':') {
			isel_path = i;
			sel_path = args[i] + 1;
		}

		if (*args[i] == '~') {
			char *exp_path = tilde_expand(args[i]); 

			if (!exp_path) {
				fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, args[i],
						strerror(errno));
				return EXIT_FAILURE;
			}

			args[i] = (char *)xrealloc(args[i], (strlen(exp_path) + 1) *
									   sizeof(char));
			strcpy(args[i], exp_path);
			free(exp_path);
		}
	}

	if (filetype) {
		/* Convert filetype into a macro that can be decoded by stat().
		 * If file type is specified, matches will be checked against
		 * this value */
		switch (filetype) {
		case 'd': filetype = DT_DIR; break;
		case 'r': filetype = DT_REG; break;
		case 'l': filetype = DT_LNK; break;
		case 's': filetype = DT_SOCK; break;
		case 'f': filetype = DT_FIFO; break;
		case 'b': filetype = DT_BLK; break;
		case 'c': filetype = DT_CHR; break;

		default:
			fprintf(stderr, _("%s: '%c': Unrecognized filetype\n"),
					PROGRAM_NAME, (char)filetype);
			return EXIT_FAILURE;
		}
	}

	char dir[PATH_MAX];

	if (sel_path) {

		size_t sel_path_len = strlen(sel_path);
		if (sel_path[sel_path_len - 1] == '/')
			sel_path[sel_path_len - 1] = '\0';

		char *tmp_dir = xnmalloc(PATH_MAX + 1, sizeof(char));

		if (strchr(sel_path, '\\')) {
			char *deq_str = dequote_str(sel_path, 0);
			if (deq_str) {
				strcpy(sel_path, deq_str);
				free(deq_str);
			}
		}

		strcpy(tmp_dir, sel_path);

		if (*sel_path == '.')
			realpath(sel_path, tmp_dir);

		if (*sel_path == '~') {
			char *exp_path = tilde_expand(sel_path);
			if (!exp_path) {
				fprintf(stderr, _("%s: Error expanding path\n"),
						PROGRAM_NAME);
				free(tmp_dir);
				return EXIT_FAILURE;
			}
			strcpy(tmp_dir, exp_path);
			free(exp_path);
		}

		if (*tmp_dir != '/') {
			snprintf(dir, PATH_MAX, "%s/%s", ws[cur_ws].path,
					 tmp_dir);
		}
		else
			strcpy(dir, tmp_dir);

		free(tmp_dir);

		if (access(dir, X_OK) == -1) {
			fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, dir,
					strerror(errno));
			return EXIT_FAILURE;
		}

		if (xchdir(dir, NO_TITLE) == -1) {
			fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, dir,
					strerror(errno));
			return EXIT_FAILURE;
		}
	}

	char *pattern = (char *)NULL;

	for (i = 1; args[i]; i++) {

		if (i == ifiletype || i == isel_path)
			continue;

/*      int invert = 0; */
	
		if (check_regex(args[i]) == EXIT_SUCCESS) {
			pattern = args[i];

			if (*pattern == '!') {
				pattern++;
/*              invert = 1; */
			}
		}

		if (!pattern) {

			if (strchr(args[i], '\\')) {
				char *deq_str = dequote_str(args[i], 0);
				if (deq_str) {
					strcpy(args[i], deq_str);
					free(deq_str);
				}
			}

			char *tmp = (char *)NULL;

			if (*args[i] != '/') {
				if (!sel_path) {
					tmp = (char *)xnmalloc(strlen(ws[cur_ws].path)
								+ strlen(args[i]) + 2, sizeof(char));
					sprintf(tmp, "%s/%s", ws[cur_ws].path, args[i]);
				}

				else {
					tmp = (char *)xnmalloc(strlen(dir)
								+ strlen(args[i]) + 2, sizeof(char));
					sprintf(tmp, "%s/%s", dir, args[i]);
				}

				struct stat fattr;
				if (lstat(tmp, &fattr) == -1) {
					fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME,
							args[i], strerror(errno));
				}
				else
					new_sel += select_file(tmp);
				free(tmp);
			}

			else {
				struct stat fattr;
				if (lstat(args[i], &fattr) == -1) {
					fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME,
							args[i], strerror(errno));
				}
				else
					new_sel += select_file(args[i]);
			}
		}

		else {
			/* We have a pattern */
			/* GLOB */
			int ret = -1;
			ret = sel_glob(args[i], sel_path ? dir : NULL,
						   filetype ? filetype : 0);

			/* If glob failed, try REGEX */
			if (ret <= 0) {
				ret = sel_regex(args[i], sel_path ? dir : NULL,
								filetype);
				if (ret > 0)
					new_sel += ret;
			}
			else
				new_sel += ret;
		}
	}

	if (new_sel > 0) {
		if (save_sel() != EXIT_SUCCESS) {
			_err('e', PRINT_PROMPT, _("%s: Error writing selected files "
			"to the selections file\n"), PROGRAM_NAME);
		}
	}

	if (sel_path && xchdir(ws[cur_ws].path, NO_TITLE) == -1) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, ws[cur_ws].path,
				strerror(errno));
		return EXIT_FAILURE;
	}

	if (new_sel <= 0) {
		if (pattern)
			fprintf(stderr, _("%s: No matches found\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	/* Get total size of sel files */
	struct stat sattr;

	i = (int)sel_n;
	while (--i >= 0) {

		if (lstat(sel_elements[i], &sattr) != -1) {

/*          if ((sattr.st_mode & S_IFMT) == S_IFDIR) {
				off_t dsize = dir_size(sel_elements[i]);
				total_sel_size += dsize;
			}
			else */
				total_sel_size += sattr.st_size;
		}
	}

	/* Print entries */
	if (sel_n > 10)
		printf(_("%zu files are now in the Selection Box\n"),
				 sel_n);

	else if (sel_n > 0) {
		printf(_("%zu selected %s:\n\n"), sel_n, (sel_n == 1)
				? _("file") : _("files"));

		for (i = 0; i < (int)sel_n; i++)
			colors_list(sel_elements[i], (int)i + 1, NO_PAD,
						PRINT_NEWLINE);
	}

	/* Print total size */
	char *human_size = get_size_unit(total_sel_size);

	if (sel_n > 10)
		printf(_("Total size: %s\n"), human_size);

	else if (sel_n > 0)
		printf(_("\n%s%sTotal size%s: %s\n"), df_c, bold, df_c, human_size);

	free(human_size);

	return EXIT_SUCCESS;
}

static void
show_sel_files(void)
{
	if (clear_screen)
		CLEAR;

	printf(_("%s%sSelection Box%s\n"), df_c, bold, df_c);

	int reset_pager = 0;

	if (sel_n == 0)
		puts(_("Empty"));

	else {
		putchar('\n');
		struct winsize w;
		ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
		int c;
		size_t counter = 0;
		unsigned short term_rows = w.ws_row;
		term_rows -= 2;
		size_t i;

		for(i = 0; i < sel_n; i++) {
/*          if (pager && counter > (term_rows-2)) { */

			if (pager && counter > (size_t)term_rows) {
				switch(c = xgetchar()) {
				/* Advance one line at a time */
				case 66:  /* fallthrough */ /* Down arrow */
				case 10:  /* fallthrough */ /* Enter */
				case 32: /* Space */
					break;
				/* Advance one page at a time */
				case 126: counter=0; /* Page Down */
					break;
				/* Stop paging (and set a flag to reenable the pager
				 * later) */
				case 99:  /* fallthrough */ /* 'c' */
				case 112:  /* fallthrough */ /* 'p' */
				case 113: pager=0, reset_pager=1; /* 'q' */
					break;
				/* If another key is pressed, go back one position.
				 * Otherwise, some filenames won't be listed.*/
				default: i--; continue;
					break;
				}
			}

			counter++;
			colors_list(sel_elements[i], (int)i + 1, NO_PAD,
						PRINT_NEWLINE);
		}

		char *human_size = get_size_unit(total_sel_size);

		printf(_("\n%s%sTotal size%s: %s\n"), df_c, bold, df_c,
			   human_size);

		free(human_size);
	}

	if (reset_pager)
		pager = 1;
}

static int
deselect(char **comm)
{
	if (!comm)
		return EXIT_FAILURE;

	if (comm[1] && (strcmp(comm[1], "*") == 0
	|| strcmp(comm[1], "a") == 0 || strcmp(comm[1], "all") == 0)) {

		if (sel_n > 0) {
			int i = (int)sel_n;

			while (--i >= 0)
				free(sel_elements[i]);

			sel_n = total_sel_size = 0;

			if (save_sel() != 0)
				return EXIT_FAILURE;
			else
				return EXIT_SUCCESS;
		}
		else {
			puts(_("desel: There are no selected files"));
			return EXIT_SUCCESS;
		}
	}

	register int i; 

	if (clear_screen)
		CLEAR;

	printf(_("%sSelection Box%s\n"), bold, df_c);

	if (sel_n == 0) {
		puts(_("Empty"));
		return EXIT_SUCCESS;
	}

	putchar('\0');

	for (i = 0; i < (int)sel_n; i++)
		colors_list(sel_elements[i], (int)i + 1, NO_PAD, PRINT_NEWLINE);

	char *human_size = get_size_unit(total_sel_size);
	printf(_("\n%s%sTotal size%s: %s\n"), df_c, bold, df_c, human_size);
	free(human_size);

	printf(_("\n%sEnter '%c' to quit.\n"), df_c, 'q');
	size_t desel_n = 0;
	char *line = NULL, **desel_elements = (char **)NULL;

	while (!line)
		line = rl_no_hist(_("File(s) to be deselected (ex: 1 2-6, or *): "));

	desel_elements = get_substr(line, ' ');
	free(line);

	if (!desel_elements)
		return EXIT_FAILURE;

	for (i = 0; desel_elements[i]; i++)
		desel_n++;

	i = (int)desel_n;
	while (--i >= 0) { /* Validation */

		/* If not a number */
		if (!is_number(desel_elements[i])) {

			if (strcmp(desel_elements[i], "q") == 0) {

				i = (int)desel_n;
				while (--i >= 0)
					free(desel_elements[i]);

				free(desel_elements);

				return EXIT_SUCCESS;
			}

			else if (strcmp(desel_elements[i], "*") == 0) {

				/* Clear the sel array */
				i = (int)sel_n;
				while (--i >= 0)
					free(sel_elements[i]);

				sel_n = total_sel_size = 0;

				i = (int)desel_n;
				while (--i >= 0)
					free(desel_elements[i]);

				int exit_status = EXIT_SUCCESS;

				if (save_sel() != 0)
					exit_status = EXIT_FAILURE;

				free(desel_elements);

				if (cd_lists_on_the_fly) {
					free_dirlist();
					if (list_dir() != EXIT_SUCCESS)
						exit_status = EXIT_FAILURE;
				}

				return exit_status;
			}

			else {
				printf(_("desel: '%s': Invalid element\n"),
						desel_elements[i]);
				int j = (int)desel_n;

				while (--j >= 0)
					free(desel_elements[j]);

				free(desel_elements);

				return EXIT_FAILURE;
			}
		}

		/* If a number, check it's a valid ELN */
		else {
			int atoi_desel = atoi(desel_elements[i]);

			if (atoi_desel == 0 || (size_t)atoi_desel > sel_n) {
				printf(_("desel: '%s': Invalid ELN\n"),
						 desel_elements[i]);

				int j = (int)desel_n;
				while (--j >= 0)
					free(desel_elements[j]);

				free(desel_elements);

				return EXIT_FAILURE;
			}
		}
	}

	/* If a valid ELN and not asterisk... */
	/* Store the full path of all the elements to be deselected in a new
	 * array (desel_path). I need to do this because after the first
	 * rearragement of the sel array, that is, after the removal of the
	 * first element, the index of the next elements changed, and cannot
	 * thereby be found by their index. The only way to find them is to
	 * compare string by string */
	char **desel_path = (char **)NULL;
	desel_path = (char **)xnmalloc(desel_n, sizeof(char *));

	i = (int)desel_n;
	while (--i >= 0) {
		int desel_int = atoi(desel_elements[i]);
		desel_path[i] = savestring(sel_elements[desel_int - 1],
							strlen(sel_elements[desel_int - 1]));
	}

	/* Search the sel array for the path of the element to deselect and
	 * store its index */
	struct stat desel_attrib;

	i = (int)desel_n;
	while (--i >= 0) {
		int j, k, desel_index = 0;

		k = (int)sel_n;
		while (--k >= 0) {

			if (strcmp(sel_elements[k], desel_path[i]) == 0) {

				/* Sustract size from total size */
				if (lstat(sel_elements[k], &desel_attrib) != -1) {

					if ((desel_attrib.st_mode & S_IFMT) == S_IFDIR)
						total_sel_size -= dir_size(sel_elements[k]);
					else
						total_sel_size -= desel_attrib.st_size;
				}

				desel_index = k;
				break;
			}
		}

		/* Once the index was found, rearrange the sel array removing the
		 * deselected element (actually, moving each string after it to
		 * the previous position) */
		for (j = desel_index; j < (int)(sel_n - 1); j++) {
			sel_elements[j] = (char *)xrealloc(sel_elements[j],
									(strlen(sel_elements[j + 1]) + 1)
									* sizeof(char));
			strcpy(sel_elements[j], sel_elements[j + 1]);
		}
	}

	/* Free the last DESEL_N elements from the old sel array. They won't
	 * be used anymore, for they contain the same value as the last
	 * non-deselected element due to the above array rearrangement */
	for (i = 1; i <= (int)desel_n; i++)
		if ((int)(sel_n - i) >= 0 && sel_elements[sel_n - i])
			free(sel_elements[sel_n - i]);

	/* Reallocate the sel array according to the new size */
	sel_n = (sel_n - desel_n);

	if ((int)sel_n < 0)
		sel_n = total_sel_size = 0;

	if (sel_n)
		sel_elements = (char **)xrealloc(sel_elements, sel_n * sizeof(char *));

	/* Deallocate local arrays */
	i = (int)desel_n;
	while (--i >= 0) {
		free(desel_path[i]);
		free(desel_elements[i]);
	}

	free(desel_path);
	free(desel_elements);

	if (args_n > 0) {

		for (i = 1; i <= (int)args_n; i++)
			free(comm[i]);

		comm = (char **)xrealloc(comm, 1 * sizeof(char *));
		args_n = 0;
	}

	int exit_status = EXIT_SUCCESS;

	if (save_sel() != 0)
		exit_status = EXIT_FAILURE;

	/* If there is still some selected file, reload the desel screen */
	if (sel_n)
		if (deselect(comm) != 0)
			exit_status = EXIT_FAILURE;

	return exit_status;
}

static int
search_glob(char **comm, int invert)
/* List matching filenames in the specified directory */
{
	if (!comm || !comm[0])
		return EXIT_FAILURE;

	char *search_str = (char *)NULL, *search_path = (char *)NULL;
	mode_t file_type = 0;
	struct stat file_attrib;

	/* If there are two arguments, the one starting with '-' is the
	 * filetype and the other is the path */
	if (comm[1] && comm[2]) {

		if (comm[1][0] == '-') {
			file_type = (mode_t)comm[1][1];
			search_path = comm[2];
		}

		else if (comm[2][0] == '-') {
			file_type = (mode_t)comm[2][1];
			search_path = comm[1];
		}

		else
			search_path = comm[1];
	}

	/* If just one argument, '-' indicates filetype. Else, we have a
	 * path */
	else if (comm[1]) {

		if (comm[1][0] == '-')
			file_type = (mode_t)comm[1][1];
		else
			search_path = comm[1];
	}

	/* If no arguments, search_path will be NULL and file_type zero */

	int recursive = 0;

	if (file_type) {

		/* Convert filetype into a macro that can be decoded by stat().
		 * If file type is specified, matches will be checked against
		 * this value */
		switch (file_type) {
		case 'd': file_type = invert ? DT_DIR : S_IFDIR; break;
		case 'r': file_type = invert ? DT_REG : S_IFREG; break;
		case 'l': file_type = invert ? DT_LNK : S_IFLNK; break;
		case 's': file_type = invert ? DT_SOCK : S_IFSOCK; break;
		case 'f': file_type = invert ? DT_FIFO : S_IFIFO; break;
		case 'b': file_type = invert ? DT_BLK : S_IFBLK; break;
		case 'c': file_type = invert ? DT_CHR : S_IFCHR; break;
		case 'x': recursive = 1; break;

		default:
			fprintf(stderr, _("%s: '%c': aUnrecognized filetype\n"),
					PROGRAM_NAME, (char)file_type);
			return EXIT_FAILURE;
		}
	}

	if (recursive) {
		char *cmd[] = { "find", (search_path && *search_path) ? search_path
						: ".", "-name", comm[0] + 1, NULL };
		launch_execve(cmd, FOREGROUND, E_NOFLAG);
		return EXIT_SUCCESS;
	}

	/* If we have a path ("/str /path"), chdir into it, since
	 * glob() works on CWD */
	if (search_path && *search_path) {

		/* Deescape the search path, if necessary */
		if (strchr(search_path, '\\')) {
			char *deq_dir = dequote_str(search_path, 0);

			if (!deq_dir) {
				fprintf(stderr, _("%s: %s: Error dequoting filename\n"),
						PROGRAM_NAME, comm[1]);
				return EXIT_FAILURE;
			}

			strcpy(search_path, deq_dir);
			free(deq_dir);
		}

		size_t path_len = strlen(search_path);
		if (search_path[path_len - 1] == '/')
			search_path[path_len - 1] = '\0';

		/* If search is current directory */
		if ((*search_path == '.' && !search_path[1]) ||
		(search_path[1] == ws[cur_ws].path[1]
		&& strcmp(search_path, ws[cur_ws].path) == 0))
			search_path = (char *)NULL;

		else if (xchdir(search_path, NO_TITLE) == -1) {
			fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, search_path,
					strerror(errno));
			return EXIT_FAILURE;
		}
	}

	int i;

	char *tmp = comm[0];

	if (invert)
		tmp++;

	/* Search for globbing char */
	int glob_char_found = 0;
	for (i = 1; tmp[i]; i++) {
		if (tmp[i] == '*' || tmp[i] == '?'
		|| tmp[i] == '[' || tmp[i] == '{'
		/* Consider regex chars as well: we don't want this "r$"
		 * to become this "*r$*" */
		|| tmp[i] == '|' || tmp[i] == '^'
		|| tmp[i] == '+' || tmp[i] == '$'
		|| tmp[i] == '.') {
			glob_char_found = 1;
			break;
		}
	}

	/* If search string is just "STR" (no glob chars), change it
	 * to "*STR*" */
	size_t search_str_len = 0;

	if (!glob_char_found) {
		search_str_len = strlen(comm[0]);

		comm[0] = (char *)xrealloc(comm[0], (search_str_len + 2) *
								   sizeof(char));
		tmp = comm[0];
		if (invert) {
			++tmp;
			search_str_len = strlen(tmp);
		}

		tmp[0] = '*';
		tmp[search_str_len] = '*';
		tmp[search_str_len + 1] = '\0';
		search_str = tmp;
	}

	else
		search_str = tmp + 1;

	/* Get matches, if any */
	glob_t globbed_files;
	int ret = glob(search_str, GLOB_BRACE, NULL, &globbed_files);

	if (ret != 0) {
		puts(_("Glob: No matches found. Trying regex..."));

		globfree(&globbed_files);

		if (search_path) {
			/* Go back to the directory we came from */
			if (xchdir(ws[cur_ws].path, NO_TITLE) == -1)
				fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME,
						ws[cur_ws].path, strerror(errno));
		}

		return EXIT_FAILURE;
	}

	/* We have matches */
	int last_column = 0, scandir_files = 0, found = 0, columns_n = 0;
	size_t flongest = 0;

	/* We need to store pointers to matching filenames in array of
	 * pointers, just as the filename length (to construct the
	 * columned output), and, if searching in CWD, its index (ELN)
	 * in the dirlist array as well */
	char **pfiles = (char **)NULL;
	int *eln = (int *)0;
	size_t *files_len = (size_t *)0;
	struct dirent **ent = (struct dirent **)NULL;

	if (invert) {
		if (!search_path) {
			int k;

			pfiles = (char **)xnmalloc(files + 1, sizeof(char *));
			eln = (int *)xnmalloc(files + 1, sizeof(int));
			files_len = (size_t *)xnmalloc(files + 1, sizeof(size_t));

			for (k = 0; file_info[k].name; k++) {
				int l, f = 0;

				for (l = 0; globbed_files.gl_pathv[l]; l++) {
					if (*globbed_files.gl_pathv[l] == *file_info[k].name
					&& strcmp(globbed_files.gl_pathv[l], file_info[k].name)
					== 0) {
						f = 1;
						break;
					}
				}

				if (!f) {

					if (file_type && file_info[k].type != file_type)
						continue;

					eln[found] = (int)(k + 1);
					files_len[found] = file_info[k].len
									   + file_info[k].eln_n + 1;
					if (files_len[found] > flongest)
						flongest = files_len[found];

					pfiles[found++] = file_info[k].name;
				}
			}
		}

		else {
			scandir_files = scandir(search_path, &ent, skip_files,
									xalphasort);

			if (scandir_files != -1) {

				pfiles = (char **)xnmalloc((size_t)scandir_files + 1,
										   sizeof(char *));
				eln = (int *)xnmalloc((size_t)scandir_files + 1,
											 sizeof(int));
				files_len = (size_t *)xnmalloc((size_t)scandir_files + 1,
											   sizeof(size_t));

				int k, l;

				for (k = 0; k < scandir_files; k++) {
					int f = 0;

					for (l = 0; globbed_files.gl_pathv[l]; l++) {
						if (*ent[k]->d_name == *globbed_files.gl_pathv[l]
						&& strcmp(ent[k]->d_name,
						globbed_files.gl_pathv[l]) == 0) {
							f = 1;
							break;
						}
					}

					if (!f) {

						if (file_type && ent[k]->d_type != file_type)
							continue;

						eln[found] = -1;
						files_len[found] = unicode
								? wc_xstrlen(ent[k]->d_name)
								: strlen(ent[k]->d_name);

						if (files_len[found] > flongest)
							flongest = files_len[found];

						pfiles[found++] = ent[k]->d_name;
					}
				}
			}
		}
	}

	else { /* No invert search */

		pfiles = (char **)xnmalloc(globbed_files.gl_pathc + 1,
								   sizeof(char *));
		eln = (int *)xnmalloc(globbed_files.gl_pathc + 1, sizeof(int));
		files_len = (size_t *)xnmalloc(globbed_files.gl_pathc
											+ 1, sizeof(size_t));

		for (i = 0; globbed_files.gl_pathv[i]; i++) {

			if (*globbed_files.gl_pathv[i] == '.'
			&& (!globbed_files.gl_pathv[i][1]
			|| (globbed_files.gl_pathv[i][1] == '.'
			&& !globbed_files.gl_pathv[i][2])))
				continue;

			if (file_type) {

				/* Simply skip all files not matching file_type */
				if (lstat(globbed_files.gl_pathv[i], &file_attrib) == -1)
					continue;

				if ((file_attrib.st_mode & S_IFMT) != file_type)
					continue;
			}

			pfiles[found] = globbed_files.gl_pathv[i];

			/* Get the longest filename in the list */

			/* If not searching in CWD, we only need to know the file's
			 * length (no ELN) */
			if (search_path) {

				/* This will be passed to colors_list(): -1 means no ELN */
				eln[found] = -1;

				files_len[found] = unicode ? wc_xstrlen(pfiles[found])
								   : strlen(pfiles[found]);

				if (files_len[found] > flongest)
					flongest = files_len[found];

				found++;
			}

			/* If searching in CWD, take into account the file's ELN
			 * when calculating its legnth */
			else {
				size_t j;

				for (j = 0; file_info[j].name; j++) {

					if (*pfiles[found] != *file_info[j].name
					|| strcmp(pfiles[found], file_info[j].name) != 0)
						continue;

					eln[found] = (int)(j + 1);

					files_len[found] = file_info[j].len + file_info[j].eln_n + 1;

					if (files_len[found] > flongest)
						flongest = files_len[found];
				}

				found ++;
			}
		}
	}

	/* Print the results using colors and columns */
	if (found) {

		struct winsize w;
		ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
		unsigned short tcols = w.ws_col;

		if (flongest <= 0 || flongest > tcols)
			columns_n = 1;

		else
			columns_n = (int)(tcols / (flongest + 1));

		if (columns_n > found)
			columns_n = found;

		for (i = 0; i < found; i++) {

			if (!pfiles[i])
				continue;

			if ((i + 1) % columns_n == 0)
				last_column = 1;
			else
				last_column = 0;

			colors_list(pfiles[i], (eln[i] && eln[i] != -1) ? eln[i] : 0,
						(last_column || i == (found - 1)) ? 0 :
						(int)(flongest - files_len[i]) + 1,
						(last_column || i == found - 1) ? 1 : 0);
			/* Second argument to colors_list() is:
			 * 0: Do not print any ELN
			 * Positive number: Print positive number as ELN
			 * -1: Print "?" instead of an ELN */
		}

		printf(_("Matches found: %d\n"), found);
	}

/*  else
		printf(_("%s: No matches found\n"), PROGRAM_NAME); */

	/* Free stuff */
	if (invert && search_path) {
		i = scandir_files;
		while (--i >= 0)
			free(ent[i]);
		free(ent);
	}

	free(eln);
	free(files_len);
	free(pfiles);
	globfree(&globbed_files);

	/* If needed, go back to the directory we came from */
	if (search_path) {
		if (xchdir(ws[cur_ws].path, NO_TITLE) == -1) {
			fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME,
					ws[cur_ws].path, strerror(errno));
			return EXIT_FAILURE;
		}
	}

	if (!found)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

static int
search_regex(char **comm, int invert)
/* List matching (or not marching, if inverse is set to 1) filenames
 * in the specified directory */
{
	if (!comm || !comm[0])
		return EXIT_FAILURE;

	char *search_str = (char *)NULL, *search_path = (char *)NULL;
	mode_t file_type = 0;

	/* If there are two arguments, the one starting with '-' is the
	 * filetype and the other is the path */
	if (comm[1] && comm[2]) {

		if (*comm[1] == '-') {
			file_type = (mode_t)*(comm[1] + 1);
			search_path = comm[2];
		}

		else if (*comm[2] == '-') {
			file_type = (mode_t)*(comm[2] + 1);
			search_path = comm[1];
		}

		else
			search_path = comm[1];
	}

	/* If just one argument, '-' indicates filetype. Else, we have a
	 * path */
	else if (comm[1]) {
		if (*comm[1] == '-')
			file_type = (mode_t)*(comm[1] + 1);
		else
			search_path = comm[1];
	}

	/* If no arguments, search_path will be NULL and file_type zero */

	if (file_type) {

		/* If file type is specified, matches will be checked against
		 * this value */
		switch (file_type) {
		case 'd': file_type = DT_DIR; break;
		case 'r': file_type = DT_REG; break;
		case 'l': file_type = DT_LNK; break;
		case 's': file_type = DT_SOCK; break;
		case 'f': file_type = DT_FIFO; break;
		case 'b': file_type = DT_BLK; break;
		case 'c': file_type = DT_CHR; break;

		default:
			fprintf(stderr, _("%s: '%c': Unrecognized filetype\n"),
					PROGRAM_NAME, (char)file_type);
			return EXIT_FAILURE;
		}
	}

	struct dirent **reg_dirlist = (struct dirent **)NULL;
	int tmp_files = 0;

	/* If we have a path ("/str /path"), chdir into it, since
	 * regex() works on CWD */
	if (search_path && *search_path) {

		/* Deescape the search path, if necessary */
		if (strchr(search_path, '\\')) {
			char *deq_dir = dequote_str(search_path, 0);

			if (!deq_dir) {
				fprintf(stderr, _("%s: %s: Error dequoting filename\n"),
						PROGRAM_NAME, comm[1]);
				return EXIT_FAILURE;
			}

			strcpy(search_path, deq_dir);
			free(deq_dir);
		}

		size_t path_len = strlen(search_path);
		if (search_path[path_len - 1] == '/')
			search_path[path_len - 1] = '\0';

		if ((*search_path == '.' && !search_path[1])
		|| (search_path[1] == ws[cur_ws].path[1]
		&& strcmp(search_path, ws[cur_ws].path) == 0))
			search_path = (char *)NULL;

		if (search_path && *search_path) {
			if (xchdir(search_path, NO_TITLE) == -1) {
				fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME,
						search_path, strerror(errno));
				return EXIT_FAILURE;
			}

			tmp_files = scandir(".", &reg_dirlist, skip_files, xalphasort);

	/*      tmp_files = scandir(".", &reg_dirlist, skip_files,
							sort == 0 ? NULL : sort == 1 ? m_alphasort
							: sort == 2 ? size_sort : sort == 3
							? atime_sort : sort == 4 ? btime_sort
							: sort == 5 ? ctime_sort : sort == 6
							? mtime_sort : sort == 7 ? m_versionsort
							: sort == 8 ? ext_sort : inode_sort); */

			if (tmp_files == -1) {
				fprintf(stderr, "scandir: %s: %s\n", search_path,
						strerror(errno));

				if (xchdir(ws[cur_ws].path, NO_TITLE) == -1)
					fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME,
							ws[cur_ws].path, strerror(errno));

				return EXIT_FAILURE;
			}
		}
	}

	size_t i;

	/* Search for regex expression */
	int regex_found = check_regex(comm[0] + 1);

	/* If search string is just "STR" (no regex chars), change it
	 * to ".*STR.*" */
	if (regex_found == EXIT_FAILURE) {
		size_t search_str_len = strlen(comm[0]);

		comm[0] = (char *)xrealloc(comm[0], (search_str_len + 5) *
								   sizeof(char));

		char *tmp_str = (char *)xnmalloc(search_str_len + 1, sizeof(char));

		strcpy(tmp_str, comm[0] + (invert ? 2 : 1));

		*comm[0] = '.';
		*(comm[0] + 1) = '*';
		*(comm[0] + 2) = '\0';
		strcat(comm[0], tmp_str);
		free(tmp_str);
		*(comm[0] + search_str_len + 1) = '.';
		*(comm[0] + search_str_len + 2) = '*';
		*(comm[0] + search_str_len + 3) = '\0';
		search_str = comm[0];
	}

	else
		search_str = comm[0] + (invert ? 2 : 1);

	/* Get matches, if any, using regular expressions */
	regex_t regex_files;
	int ret = regcomp(&regex_files, search_str, REG_NOSUB|REG_EXTENDED);

	if (ret != EXIT_SUCCESS) {
		fprintf(stderr, _("'%s': Invalid regular expression\n"), search_str);

		regfree(&regex_files);

		if (search_path) {
			for (i = 0; i < (size_t)tmp_files; i++)
				free(reg_dirlist[i]);

			free(reg_dirlist);

			if (xchdir(ws[cur_ws].path, NO_TITLE) == -1)
				fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME,
						ws[cur_ws].path, strerror(errno));
		}

		return EXIT_FAILURE;
	}

	size_t found = 0;
	int *regex_index = (int *)xnmalloc((search_path ? (size_t)tmp_files
									   : files) + 1, sizeof(int));

	for (i = 0; i < (search_path ? (size_t)tmp_files : files); i++) {
		if (regexec(&regex_files, (search_path ? reg_dirlist[i]->d_name
		: file_info[i].name), 0, NULL, 0) == EXIT_SUCCESS) {
			if (!invert)
				regex_index[found++] = (int)i;
		}
		else if (invert)
			regex_index[found++] = (int)i;
	}

	regfree(&regex_files);

	if (!found) {
		fprintf(stderr, _("No matches found\n"));
		free(regex_index);

		if (search_path) {

			int j = tmp_files;
			while (--j >= 0)
				free(reg_dirlist[j]);

			free(reg_dirlist);

			if (xchdir(ws[cur_ws].path, NO_TITLE) == -1)
					fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME,
							ws[cur_ws].path, strerror(errno));
		}

		return EXIT_FAILURE;
	}

	/* We have matches */
	int last_column = 0;
	size_t flongest = 0, total_cols = 0, type_ok = 0;

	size_t *files_len = (size_t *)xnmalloc(found + 1, sizeof(size_t));
	int *match_type = (int *)xnmalloc(found + 1, sizeof(int));
 

	/* Get the longest filename in the list */
	int j = (int)found;
	while (--j >= 0) {

		/* Simply skip all files not matching file_type */
		if (file_type) {

			match_type[j] = 0;

			if (search_path) {
				if (reg_dirlist[regex_index[j]]->d_type != file_type)
					continue;
			}

			else if (file_info[regex_index[j]].type != file_type)
				continue;
		}

		/* Amount of non-filtered files */
		type_ok++;
		/* Index of each non-filtered files */
		match_type[j] = 1;

		/* If not searching in CWD, we only need to know the file's
		 * length (no ELN) */
		if (search_path) {
			files_len[j] = unicode ? wc_xstrlen(
				  reg_dirlist[regex_index[j]]->d_name)
				  : strlen(reg_dirlist[regex_index[j]]->d_name);

			if (files_len[j] > flongest)
				flongest = files_len[j];
		}

		/* If searching in CWD, take into account the file's ELN
		 * when calculating its legnth */
		else {
			files_len[j] = file_info[regex_index[j]].len
					+ (size_t)DIGINUM(regex_index[j] + 1) + 1;

			if (files_len[j] > flongest)
				flongest = files_len[j];
		}
	}

	if (type_ok) {

		struct winsize w;
		ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
		unsigned short terminal_cols = w.ws_col;

		if (flongest <= 0 || flongest > terminal_cols)
			total_cols = 1;
		else
			total_cols = (size_t)terminal_cols / (flongest + 1);

		if (total_cols > type_ok)
			total_cols = type_ok;

		/* cur_col: Current columns number */
		size_t cur_col = 0, counter = 0;

		for (i = 0; i < found; i++) {

			if (match_type[i] == 0)
				continue;

			/* Print the results using colors and columns */
			cur_col++;

			/* If the current file is in the last column or is the last
			 * listed file, we need to print no pad and a newline char.
			 * Else, print the corresponding pad, to equate the longest
			 * file length, and no newline char */
			if (cur_col == total_cols) {
				last_column = 1;
				cur_col = 0;
			}
			else
				last_column = 0;

			/* Counter: Current amount of non-filtered files: if
			 * COUNTER equals TYPE_OK (total amount of non-filtered
			 * files), we have the last file to be printed */
			counter++;

			colors_list(search_path ? reg_dirlist[regex_index[i]]->d_name
					: file_info[regex_index[i]].name, search_path ? NO_ELN
					: regex_index[i] + 1, (last_column
					|| counter == type_ok) ? NO_PAD
					: (int)(flongest - files_len[i]) + 1,
					(last_column || counter == type_ok)
					? PRINT_NEWLINE : NO_NEWLINE);

		}

		printf(_("Matches found: %zu\n"), counter);
	}

	else
		fputs(_("No matches found\n"), stderr);

	/* Free stuff */
	free(files_len);
	free(match_type);
	free(regex_index);
	regfree(&regex_files);

	/* If needed, go back to the directory we came from */
	if (search_path) {
		j = tmp_files;
		while (--j >= 0)
			free(reg_dirlist[j]);

		free(reg_dirlist);

		if (xchdir(ws[cur_ws].path, NO_TITLE) == -1) {
			fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME,
					ws[cur_ws].path, strerror(errno));
			return EXIT_FAILURE;
		}
	}

	if (type_ok)
		return EXIT_SUCCESS;

	return EXIT_FAILURE;
}

static char **
bm_prompt(void)
{
	char *bm_sel = (char *)NULL;
	printf(_("%s%s\nEnter '%c' to edit your bookmarks or '%c' to quit.\n"),
			NC_b, df_c, 'e', 'q');

	while (!bm_sel)
		bm_sel = rl_no_hist(_("Choose a bookmark: "));

	char **comm_bm = get_substr(bm_sel, ' ');
	free(bm_sel);

	return comm_bm;
}

static int
bookmark_del(char *name)
{
	FILE *bm_fp = NULL;
	bm_fp = fopen(BM_FILE, "r");
	if (!bm_fp)
		return EXIT_FAILURE;

	size_t i = 0;

	/* Get bookmarks from file */
	size_t line_size = 0;
	char *line = (char *)NULL, **bms = (char **)NULL;
	size_t bmn = 0;
	ssize_t line_len = 0;

	while ((line_len = getline(&line, &line_size, bm_fp)) > 0) {
		if (!line || !*line || *line == '#' || *line == '\n')
			continue;

		int slash = 0;
		for (i = 0; i < (size_t)line_len; i++) {
			if (line[i] == '/') {
				slash = 1;
				break;
			}
		}

		if (!slash)
			continue;

		if (line[line_len - 1] == '\n')
			line[line_len - 1] = '\0';

		bms = (char **)xrealloc(bms, (bmn + 1) * sizeof(char *));
		bms[bmn++] = savestring(line, (size_t)line_len);
	}

	free(line);
	line = (char *)NULL;

	if (!bmn) {
		puts(_("bookmarks: There are no bookmarks"));
		fclose(bm_fp);
		return EXIT_SUCCESS;
	}

	char **del_elements = (char **)NULL;
	int cmd_line = -1; 
	/* This variable let us know two things: a) bookmark name was
	 * specified in command line; b) the index of this name in the
	 * bookmarks array. It is initialized as -1 since the index name
	 * could be zero */

	if (name) {
		for (i = 0; i < bmn; i++) {
			char *bm_name = strbtw(bms[i], ']', ':');

			if (!bm_name)
				continue;

			if (strcmp(name, bm_name) == 0) {
				free(bm_name);
				cmd_line = (int)i;
				break;
			}

			free(bm_name);
		}
	}

	/* If a valid bookmark name was passed in command line, copy the
	 * corresponding bookmark index (plus 1, as if it were typed in the
	 * bookmarks screen) to the del_elements array */
	if (cmd_line != -1) {
		del_elements = (char **)xnmalloc(2, sizeof(char *));
		del_elements[0] = (char *)xnmalloc((size_t)DIGINUM(cmd_line + 1)
										   + 1, sizeof(char));
		sprintf(del_elements[0], "%d", cmd_line + 1);
		del_elements[1] = (char *)NULL;
	}

	/* If bookmark name was passed but it is not a valid bookmark */
	else if (name) {
		fprintf(stderr, _("bookmarks: %s: No such bookmark\n"), name);

		for (i = 0; i < bmn; i++)
			free(bms[i]);

		free(bms);
		fclose(bm_fp);

		return EXIT_FAILURE;
	}

	/* If not name, list bookmarks and get user input */
	else {
		printf(_("%sBookmarks%s\n\n"), bold, df_c);

		for (i = 0; i < bmn; i++)
			printf("%s%zu %s%s%s\n", el_c, i + 1, bm_c, bms[i],
				   df_c);

		/* Get user input */
		printf(_("\n%sEnter '%c' to quit.\n"), df_c, 'q');
		char *input = (char *)NULL;

		while (!input)
			input = rl_no_hist(_("Bookmark(s) to be deleted "
							   "(ex: 1 2-6, or *): "));

		del_elements = get_substr(input, ' ');
		free(input);
		input = (char *)NULL;

		if (!del_elements) {

			for (i = 0; i < bmn; i++)
				free(bms[i]);

			free(bms);
			fclose(bm_fp);
			fprintf(stderr, _("bookmarks: Error parsing input\n"));

			return EXIT_FAILURE;
		}
	}

	/* We have input */
	/* If quit */
	/* I inspect all substrings entered by the user for "q" before any
	 * other value to prevent some accidental deletion, like "1 q", or
	 * worst, "* q" */
	for (i = 0; del_elements[i]; i++) {
		int quit = 0;

		if (strcmp(del_elements[i], "q") == 0)
			quit = 1;

		else if (is_number(del_elements[i])
		&& (atoi(del_elements[i]) <= 0
		|| atoi(del_elements[i]) > (int)bmn)) {
			fprintf(stderr, _("bookmarks: %s: No such bookmark\n"),
					del_elements[i]);
			quit = 1;
		}

		if (quit) {

			for (i = 0; i < bmn; i++)
				free(bms[i]);

			free(bms);

			for (i = 0; del_elements[i]; i++)
				free(del_elements[i]);

			free(del_elements);
			fclose(bm_fp);

			return EXIT_SUCCESS;
		}
	}

	/* If "*", simply remove the bookmarks file */
	/* If there is some "*" in the input line (like "1 5 6-9 *"), it
	 * makes no sense to remove singles bookmarks: Just delete all of
	 * them at once */
	for (i = 0; del_elements[i]; i++) {

		if (strcmp(del_elements[i], "*") == 0) {
			/* Create a backup copy of the bookmarks file, just in case */
			char *bk_file = (char *)NULL;
			bk_file = (char *)xcalloc(strlen(CONFIG_DIR) + 14,
									  sizeof(char));
			sprintf(bk_file, "%s/bookmarks.bk", CONFIG_DIR);
			char *tmp_cmd[] = { "cp", BM_FILE, bk_file, NULL };

			int ret = launch_execve(tmp_cmd, FOREGROUND, E_NOFLAG);
			/* Remove the bookmarks file, free stuff, and exit */

			if (ret == EXIT_SUCCESS) {
				unlink(BM_FILE);
				printf(_("bookmarks: All bookmarks were deleted\n "
						 "However, a backup copy was created (%s)\n"),
						  bk_file);
				free(bk_file);
				bk_file = (char *)NULL;
			}

			else
				printf(_("bookmarks: Error creating backup file. No "
						 "bookmark was deleted\n"));

			for (i = 0; i < bmn; i++)
				free(bms[i]);

			free(bms);

			for (i = 0; del_elements[i]; i++)
				free(del_elements[i]);

			free(del_elements);

			fclose(bm_fp);

			/* Update bookmark names for TAB completion */
/*          get_bm_names(); */
			free_bookmarks();
			load_bookmarks();

			/* If the argument "*" was specified in command line */
			if (cmd_line != -1)
				fputs(_("All bookmarks succesfully removed\n"), stdout);

			return EXIT_SUCCESS;
		}
	}

	/* Remove single bookmarks */
	/* Open a temporary file */
	char *tmp_file = (char *)NULL;
	tmp_file = (char *)xcalloc(strlen(CONFIG_DIR) + 8, sizeof(char));
	sprintf(tmp_file, "%s/bm_tmp", CONFIG_DIR);

	FILE *tmp_fp = fopen(tmp_file, "w+");
	if (!tmp_fp) {
		for (i = 0; i < bmn; i++)
			free(bms[i]);

		free(bms);
		fclose(bm_fp);
		fprintf(stderr, _("bookmarks: Error creating temporary file\n"));

		return EXIT_FAILURE;
	}

	/* Go back to the beginning of the bookmarks file */
	fseek(bm_fp, 0, SEEK_SET);

	/* Dump into the tmp file everything except bookmarks marked for
	 * deletion */

	line_len = 0;
	line_size = 0;
	char *lineb = (char *)NULL;

	while ((line_len = getline(&lineb, &line_size, bm_fp)) > 0) {

		if (lineb[line_len - 1] == '\n')
			lineb[line_len - 1] = '\0';

		int bm_found = 0;
		size_t j;

		for (j = 0; del_elements[j]; j++) {

			if (!is_number(del_elements[j]))
				continue;

			if (strcmp(bms[atoi(del_elements[j]) - 1], lineb) == 0)
				bm_found = 1;
		}

		if (bm_found)
			continue;

		fprintf(tmp_fp, "%s\n", lineb);
	}

	free(lineb);
	lineb = (char *)NULL;

	/* Free stuff */
	for (i = 0; del_elements[i]; i++)
		free(del_elements[i]);

	free(del_elements);

	for (i = 0; i < bmn; i++)
		free(bms[i]);

	free(bms);

	/* Close files */
	fclose(bm_fp);
	fclose(tmp_fp);

	/* Remove the old bookmarks file and make the tmp file the new
	 * bookmarks file*/
	unlink(BM_FILE);
	rename(tmp_file, BM_FILE);
	free(tmp_file);
	tmp_file = (char *)NULL;

	/* Update bookmark names for TAB completion */
/*  get_bm_names(); */
	free_bookmarks();
	load_bookmarks();

	/* If the bookmark to be removed was specified in command line */
	if (cmd_line != -1)
		printf(_("Successfully removed '%s'\n"), name);

	return EXIT_SUCCESS;
}

static int
bookmark_add(char *file)
{
	if (!file)
		return EXIT_FAILURE;

	int mod_file = 0;
	/* If not absolute path, prepend current path to file */
	if (*file != '/') {
		char *tmp_file = (char *)NULL;
		tmp_file = (char *)xnmalloc((strlen(ws[cur_ws].path)
								+ strlen(file) + 2), sizeof(char));
		sprintf(tmp_file, "%s/%s", ws[cur_ws].path, file);
		file = tmp_file;
		tmp_file = (char *)NULL;
		mod_file = 1;
	}

	/* Check if FILE is an available path */

	FILE *bm_fp = fopen(BM_FILE, "r");
	if (!bm_fp) {
		fprintf(stderr, _("bookmarks: Error opening the bookmarks "
				"file\n"));
		if (mod_file)
			free(file);

		return EXIT_FAILURE;
	}

	int dup = 0;
	char **bms = (char **)NULL;
	size_t line_size = 0, i, bmn = 0;
	char *line = (char *)NULL;
	ssize_t line_len = 0;

	while ((line_len = getline(&line, &line_size, bm_fp)) > 0) {

		if (!line || !*line || *line == '#' || *line == '\n')
			continue;

		char *tmp_line = (char *)NULL;
		tmp_line = strchr (line, '/');

		if (tmp_line) {
			size_t tmp_line_len = strlen(tmp_line);

			if (tmp_line_len && tmp_line[tmp_line_len - 1] == '\n')
				tmp_line[tmp_line_len - 1] = '\0';

			if (strcmp(tmp_line, file) == 0) {
				fprintf(stderr, _("bookmarks: %s: Path already "
								   "bookmarked\n"), file);
				dup = 1;
				break;
			}

			tmp_line = (char *)NULL;
		}

		/* Store lines: used later to check hotkeys */
		bms = (char **)xrealloc(bms, (bmn + 1) * sizeof(char *));
		bms[bmn++] = savestring(line, strlen(line));

	}

	free(line);
	line = (char *)NULL;
	fclose(bm_fp);

	if (dup) {
		for (i = 0; i < bmn; i++)
			free(bms[i]);

		free(bms);

		if (mod_file)
			free(file);

		return EXIT_FAILURE;
	}

	/* If path is available */

	char *name = (char *)NULL, *hk = (char *)NULL, *tmp = (char *)NULL;

	/* Ask for data to construct the bookmark line. Both values could be
	 * NULL */
	puts(_("Bookmark line example: [sc]name:path"));
	hk = rl_no_hist("Shortcut: ");

	/* Check if hotkey is available */
	if (hk) {
		char *tmp_line = (char *)NULL;

		for (i = 0; i < bmn; i++) {
			tmp_line = strbtw(bms[i], '[', ']');

			if (tmp_line) {

				if (strcmp(hk, tmp_line) == 0) {
					fprintf(stderr, _("bookmarks: %s: This shortcut is "
									  "already in use\n"), hk);

					dup = 1;
					free(tmp_line);
					break;
				}

				free(tmp_line);
			}
		}
	}

	if (dup) {
		if (hk)
			free(hk);

		for (i = 0; i < bmn; i++)
			free(bms[i]);

		free(bms);

		if (mod_file)
			free(file);

		return EXIT_FAILURE;
	}

	name = rl_no_hist("Name: ");

	if (name) {
		/* Check name is not duplicated */

		char *tmp_line = (char *)NULL;
		for (i = 0; i < bmn; i++) {
			tmp_line = strbtw(bms[i], ']', ':');

			if (tmp_line) {

				if (strcmp(name, tmp_line) == 0) {
					fprintf(stderr, _("bookmarks: %s: This name is "
									  "already in use\n"), name);

					dup = 1;
					free(tmp_line);
					break;
				}
				free(tmp_line);
			}
		}

		if (dup) {
			free(name);

			if (hk)
				free(hk);

			for (i = 0; i < bmn; i++)
				free(bms[i]);

			free(bms);

			if (mod_file)
				free(file);

			return EXIT_FAILURE;
		}

		/* Generate the bookmark line */
		if (hk) { /* name AND hk */
			tmp = (char *)xcalloc(strlen(hk) + strlen(name)
								  + strlen(file) + 5, sizeof(char));
			sprintf(tmp, "[%s]%s:%s\n", hk, name, file);
			free(hk);
		}

		else { /* Only name */
			tmp = (char *)xnmalloc(strlen(name) + strlen(file) + 3,
								  sizeof(char));
			sprintf(tmp, "%s:%s\n", name, file);
		}

		free(name);
		name = (char *)NULL;
	}

	else if (hk) { /* Only hk */
		tmp = (char *)xnmalloc(strlen(hk) + strlen(file) + 4,
							  sizeof(char));
		sprintf(tmp, "[%s]%s\n", hk, file);
		free(hk);
		hk = (char *)NULL;
	}

	else { /* Neither shortcut nor name: only path */
		tmp = (char *)xnmalloc(strlen(file) + 2, sizeof(char));
		sprintf(tmp, "%s\n", file);
	}

	for (i = 0; i < bmn; i++)
		free(bms[i]);

	free(bms);

	bms = (char **)NULL;

	if (!tmp) {
		fprintf(stderr, _("bookmarks: Error generating the bookmark line\n"));
		return EXIT_FAILURE;
	}

	/* Once we have the bookmark line, write it to the bookmarks file */

	bm_fp = fopen(BM_FILE, "a+");
	if (!bm_fp) {
		fprintf(stderr, _("bookmarks: Error opening the bookmarks file\n"));
		free(tmp);
		return EXIT_FAILURE;
	}

	if (mod_file)
		free(file);

	if (fseek(bm_fp, 0L, SEEK_END) == -1) {
		fprintf(stderr, _("bookmarks: Error opening the bookmarks file\n"));
		free(tmp);
		fclose(bm_fp);
		return EXIT_FAILURE;
	}

	/* Everything is fine: add the new bookmark to the bookmarks file */
	fprintf(bm_fp, "%s", tmp);
	fclose(bm_fp);
	printf(_("File succesfully bookmarked\n"));
	free(tmp);

	/* Update bookmark names for TAB completion */
/*  get_bm_names();  */
	free_bookmarks();
	load_bookmarks();

	return EXIT_SUCCESS;
}

static int
edit_bookmarks(char *cmd)
{
	int exit_status = EXIT_SUCCESS;

	if (!cmd) {

		if (opener) {
			char *tmp_cmd[] = { opener, BM_FILE, NULL };

			if (launch_execve(tmp_cmd, FOREGROUND, E_NOSTDERR) != EXIT_SUCCESS) {
				fprintf(stderr, _("%s: Cannot open the bookmarks file"),
						PROGRAM_NAME);
				exit_status = EXIT_FAILURE;
			}
		}

		else {
			char *tmp_cmd[] = { "mm", BM_FILE, NULL };

			exit_status = mime_open(tmp_cmd);
		}
	}

	else {
		char *tmp_cmd[] = { cmd, BM_FILE, NULL };

		int ret = launch_execve(tmp_cmd, FOREGROUND, E_NOSTDERR);

		if (ret != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;
	}

	return exit_status;
}

static int
open_bookmark(void)
{
	/* If no bookmarks */
	if (bm_n == 0) {
		printf(_("Bookmarks: There are no bookmarks\nEnter 'bm edit' "
				 "or press F11 to edit the bookmarks file. You can "
				 "also enter 'bm add PATH' to add a new bookmark\n"));
		return EXIT_SUCCESS;
	}

	/* We have bookmarks... */
	struct stat file_attrib;

	if (clear_screen)
		CLEAR;

	printf(_("%sBookmarks Manager%s\n\n"), bold, df_c);

	/* Print bookmarks taking into account the existence of shortcut,
	 * name, and path for each bookmark */
	size_t i, eln = 0;

	for (i = 0; i < bm_n; i++) {

		if (!bookmarks[i].path || !*bookmarks[i].path)
			continue;

		eln++;

		int is_dir = 0, sc_ok = 0, name_ok = 0, non_existent = 0;

		int path_ok = stat(bookmarks[i].path, &file_attrib);

		if (bookmarks[i].shortcut)
			sc_ok = 1;

		if (bookmarks[i].name)
			name_ok = 1;

		if (path_ok == -1)
			non_existent = 1;

		else {
			switch((file_attrib.st_mode & S_IFMT)) {

				case S_IFDIR: is_dir = 1; break;

				case S_IFREG: break;

				default: non_existent = 1; break;
			}
		}

		printf("%s%zu%s %s%c%s%c%s %s%s%s\n", el_c, eln, df_c,
			   bold, sc_ok ? '[' : 0, sc_ok ? bookmarks[i].shortcut
			   : "", sc_ok ? ']' : 0, df_c, non_existent ? gray
			   : (is_dir ? bm_c : fi_c), name_ok ? bookmarks[i].name
			   : bookmarks[i].path, df_c);
	}

	/* User selection. Display the prompt */
	char **arg = bm_prompt();

	if (!arg || !*arg)
		return EXIT_FAILURE;

	int exit_status = EXIT_SUCCESS;

	/* Case "edit" */
	if (*arg[0] == 'e' && (!arg[0][1] || strcmp(arg[0], "edit") == 0)) {

		stat(BM_FILE, &file_attrib);
		time_t mtime_bfr = (time_t)file_attrib.st_mtime;

		edit_bookmarks(arg[1] ? arg[1] : NULL);

		stat(BM_FILE, &file_attrib);

		if (mtime_bfr != (time_t)file_attrib.st_mtime) {
			free_bookmarks();
			load_bookmarks();
		}

		for (i = 0; arg[i]; i++)
			free(arg[i]);

		free(arg);

		arg = (char **)NULL;

		char *tmp_cmd[] = { "bm", NULL };
		bookmarks_function(tmp_cmd);

		return EXIT_SUCCESS;
	}

	/* Case "quit" */
	if (*arg[0] == 'q' && (!arg[0][1] || strcmp(arg[0], "quit") == 0))
		goto FREE_AND_EXIT;

	char *tmp_path = (char *)NULL;

	/* Get the corresponding bookmark path */

	/* If an ELN */
	if (is_number(arg[0])) {

		int num = atoi(arg[0]);
		if (num <= 0 || (size_t)num > bm_n) {
			fprintf(stderr, _("Bookmarks: %d: No such ELN\n"), num);
			exit_status = EXIT_FAILURE;
			goto FREE_AND_EXIT;
		}

		else
			tmp_path = bookmarks[num - 1].path;
	}

	/* If string, check shortcuts and names */
	else {

		for (i = 0; i < bm_n; i++) {

			if ((bookmarks[i].shortcut
			&& *arg[0] == *bookmarks[i].shortcut
			&& strcmp(arg[0], bookmarks[i].shortcut) == 0)
			|| (bookmarks[i].name
			&& *arg[0] == *bookmarks[i].name
			&& strcmp(arg[0], bookmarks[i].name) == 0)) {

				if (bookmarks[i].path) {
					char *tmp_cmd[] = { "o", bookmarks[i].path,
										arg[1] ? arg[1] : NULL,
										NULL };

					exit_status = open_function(tmp_cmd);
					goto FREE_AND_EXIT;
				}

				fprintf(stderr, _("%s: %s: Invalid bookmark\n"),
						PROGRAM_NAME, arg[0]);
				exit_status = EXIT_FAILURE;
				goto FREE_AND_EXIT;
			}
		}
	}

	if (!tmp_path) {
		fprintf(stderr, _("Bookmarks: %s: No such bookmark\n"),
				arg[0]);
		exit_status = EXIT_FAILURE;
		goto FREE_AND_EXIT;
	}

	char *tmp_cmd[] = { "o", tmp_path, arg[1] ? arg[1] : NULL, NULL };

	exit_status = open_function(tmp_cmd);
	goto FREE_AND_EXIT;

	FREE_AND_EXIT: {

		for (i = 0; arg[i]; i++)
			free(arg[i]);

		free(arg);

		arg = (char **)NULL;

		return exit_status;
	}
}

static int
bookmarks_function(char **cmd)
{
	if (xargs.stealth_mode == 1) {
		printf(_("%s: Access to configuration files is not allowed in "
			   "stealth mode\n"), PROGRAM_NAME);
		return EXIT_SUCCESS;
	}

	if (!config_ok) {
		fprintf(stderr, _("Bookmarks function disabled\n"));
		return EXIT_FAILURE;
	}

	/* If the bookmarks file doesn't exist, create it. NOTE: This file
	 * should be created at startup (by get_bm_names()), but we check
	 * it again here just in case it was meanwhile deleted for some
	 * reason */

	/* If no arguments */
	if (!cmd[1])
		return open_bookmark();

	/* Check arguments */

	/* Add a bookmark */
	if (*cmd[1] == 'a' && (!cmd[1][1] || strcmp(cmd[1], "add") == 0)) {

		if (!cmd[2]) {
			printf(_("Usage: bookmarks, bm [a, add PATH]\n"));
			return EXIT_SUCCESS;
		}

		if (access(cmd[2], F_OK) != 0) {
			fprintf(stderr, _("Bookmarks: %s: %s\n"), cmd[2],
					strerror(errno));
			return EXIT_FAILURE;
		}

		return bookmark_add(cmd[2]);
	}

	/* Delete bookmarks */
	if (*cmd[1] == 'd' && (!cmd[1][1] || strcmp(cmd[1], "del") == 0))
		return bookmark_del(cmd[2] ? cmd[2] : NULL);

	/* Edit */
	if (*cmd[1] == 'e' && (!cmd[1][1] || strcmp(cmd[1], "edit") == 0))
		return edit_bookmarks(cmd[2] ? cmd[2] : NULL);

	/* Shortcut or bm name */
	size_t i;
	for (i = 0; i < bm_n; i++) {

		if ((bookmarks[i].shortcut
		&& *cmd[1] == *bookmarks[i].shortcut
		&& strcmp(cmd[1], bookmarks[i].shortcut) == 0)
		|| (bookmarks[i].name
		&& *cmd[1] == *bookmarks[i].name
		&& strcmp(cmd[1], bookmarks[i].name) == 0)) {

			if (bookmarks[i].path) {
				char *tmp_cmd[] = { "o", bookmarks[i].path,
									cmd[2] ? cmd[2] : NULL, NULL };

				return open_function(tmp_cmd);
			}

			fprintf(stderr, _("Bookmarks: %s: Invalid bookmark\n"),
					cmd[1]);
			return EXIT_FAILURE;
		}
	}

	fprintf(stderr, _("Bookmarks: %s: No such bookmark\n"), cmd[1]);

	return EXIT_FAILURE;
}

static int
get_properties(char *filename, int dsize)
{
	if (!filename || !*filename)
		return EXIT_FAILURE;

	size_t len = strlen(filename);
	if (filename[len - 1] == '/')
		filename[len - 1] = '\0';

	struct stat file_attrib;

	/* Check file existence */
	if (lstat(filename, &file_attrib) == -1) {
		fprintf(stderr, "%s: pr: '%s': %s\n", PROGRAM_NAME, filename,
				strerror(errno));
		return EXIT_FAILURE;
	}

	/* Get file size */
	char *size_type = get_size_unit(file_attrib.st_size);

	/* Get file type (and color): */
	int sticky = 0;
	char file_type = 0;
	char *linkname = (char *)NULL, *color = (char *)NULL;

	switch (file_attrib.st_mode & S_IFMT) {

	case S_IFREG: {

		char *ext = (char *)NULL;

		file_type = '-';

		if (access(filename, R_OK) == -1)
			color = nf_c;

		else if (file_attrib.st_mode & S_ISUID)
			color = su_c;

		else if (file_attrib.st_mode & S_ISGID)
			color = sg_c;

		else {
#ifdef _LINUX_CAP
			cap_t cap = cap_get_file(filename);

			if (cap) {
				color = ca_c;
				cap_free(cap);
			}

			else if (file_attrib.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)) {
#else

			if (file_attrib.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)) {
#endif

				if (file_attrib.st_size == 0) color = ee_c;

				else color = ex_c;
			}

			else if (file_attrib.st_size == 0) color = ef_c;

			else if (file_attrib.st_nlink > 1) color = mh_c;

			else {
				ext = strrchr(filename, '.');

				if (ext) {

					char *extcolor = get_ext_color(ext);

					if (extcolor) {
						char ext_color[MAX_COLOR] = "";
						sprintf(ext_color, "\x1b[%sm", extcolor);
						color = ext_color;
						extcolor = (char *)NULL;
					}

					else /* No matching extension found */
						color = fi_c;
				}

				else
					color = fi_c;
			}
		}
		}
		break;

	case S_IFDIR:
		file_type = 'd';

		if (access(filename, R_OK|X_OK) != 0)
			color = nd_c;

		else {
			int is_oth_w = 0;
			if (file_attrib.st_mode & S_ISVTX) sticky = 1;

			if (file_attrib.st_mode & S_IWOTH) is_oth_w = 1;

			int files_dir = count_dir(filename);

			color = sticky ? (is_oth_w ? tw_c : st_c)
				: is_oth_w ? ow_c :
				((files_dir == 2 || files_dir == 0) ? ed_c : di_c);
		}

		break;

	case S_IFLNK:
		file_type = 'l';

		linkname = realpath(filename, (char *)NULL);

		if (linkname)
			color = ln_c;
		else
			color = or_c;
		break;

	case S_IFSOCK: file_type = 's'; color = so_c; break;

	case S_IFBLK: file_type = 'b'; color = bd_c; break;

	case S_IFCHR: file_type = 'c'; color = cd_c; break;

	case S_IFIFO: file_type = 'p'; color = pi_c; break;

	default: file_type = '?'; color = no_c;
	}

	/* Get file permissions */
	char read_usr = '-', write_usr = '-', exec_usr = '-',
		 read_grp = '-', write_grp = '-', exec_grp = '-',
		 read_others = '-', write_others = '-', exec_others = '-';

	mode_t val = (file_attrib.st_mode & ~S_IFMT);
	if (val & S_IRUSR) read_usr = 'r';
	if (val & S_IWUSR) write_usr = 'w';
	if (val & S_IXUSR) exec_usr = 'x';

	if (val & S_IRGRP) read_grp = 'r';
	if (val & S_IWGRP) write_grp = 'w';
	if (val & S_IXGRP) exec_grp = 'x';

	if (val & S_IROTH) read_others = 'r';
	if (val & S_IWOTH) write_others = 'w';
	if (val & S_IXOTH) exec_others = 'x';

	if (file_attrib.st_mode & S_ISUID)
		(val & S_IXUSR) ? (exec_usr = 's') : (exec_usr = 'S');
	if (file_attrib.st_mode & S_ISGID)
		(val & S_IXGRP) ? (exec_grp = 's') : (exec_grp = 'S');

	/* Get number of links to the file */
	nlink_t link_n = file_attrib.st_nlink;

	/* Get modification time */
	time_t time = (time_t)file_attrib.st_mtim.tv_sec;
	struct tm *tm = localtime(&time);
	char mod_time[128] = "";

	if (time)
		/* Store formatted (and localized) date-time string into
		 * mod_time */
		strftime(mod_time, sizeof(mod_time), "%b %d %H:%M:%S %Y", tm);

	else
		mod_time[0] = '-';

	/* Get owner and group names */
	uid_t owner_id = file_attrib.st_uid; /* owner ID */
	gid_t group_id = file_attrib.st_gid; /* group ID */
	struct group *group;
	struct passwd *owner;
	group = getgrgid(group_id);
	owner = getpwuid(owner_id);

	/* Print file properties */
	printf("(%04o)%c/%c%c%c/%c%c%c/%c%c%c%s %zu %s %s %s %s ",
			file_attrib.st_mode & 07777, file_type,
			read_usr, write_usr, exec_usr, read_grp,
			write_grp, exec_grp, read_others, write_others,
			(sticky) ? 't' : exec_others,
			is_acl(filename) ? "+" : "", link_n,
			(!owner) ? _("unknown") : owner->pw_name,
			(!group) ? _("unknown") : group->gr_name,
			(size_type) ? size_type : "?",
			(mod_time[0] != '\0') ? mod_time : "?");

	if (file_type && file_type != 'l')
		printf("%s%s%s\n", color, filename, df_c);

	else if (linkname) {
		printf("%s%s%s -> %s\n", color, filename, df_c, linkname);
		free(linkname);
	}

	else { /* Broken link */
		char link[PATH_MAX] = "";
		ssize_t ret = readlink(filename, link, PATH_MAX);

		if (ret) {
			printf(_("%s%s%s -> %s (broken link)\n"), color, filename,
				   df_c, link);
		}

		else
			printf("%s%s%s -> ???\n", color, filename, df_c);
	}

	/* Stat information */

	/* Last access time */
	time = (time_t)file_attrib.st_atim.tv_sec;
	tm = localtime(&time);
	char access_time[128] = "";

	if (time)
		/* Store formatted (and localized) date-time string into
		 * access_time */
		strftime(access_time, sizeof(access_time), "%b %d %H:%M:%S %Y", tm);

	else
		access_time[0] = '-';

	/* Last properties change time */
	time = (time_t)file_attrib.st_ctim.tv_sec;
	tm = localtime(&time);
	char change_time[128] = "";

	if (time)
		strftime(change_time, sizeof(change_time), "%b %d %H:%M:%S %Y", tm);

	else
		change_time[0] = '-';

	/* Get creation (birth) time */
#if defined(HAVE_ST_BIRTHTIME) || defined(__BSD_VISIBLE)
		time = file_attrib.st_birthtime;
		tm = localtime(&time);
		char creation_time[128] = "";

		if (!time)
			creation_time[0] = '-';

		else
			strftime(creation_time, sizeof(creation_time),
					 "%b %d %H:%M:%S %Y", tm);
#elif defined(_STATX)
		struct statx attrx;
		statx(AT_FDCWD, filename, AT_SYMLINK_NOFOLLOW, STATX_BTIME, &attrx);
		time = (time_t)attrx.stx_btime.tv_sec;
		tm = localtime(&time);
		char creation_time[128] = "";

		if (!time)
			creation_time[0] = '-';

		else
			strftime(creation_time, sizeof(creation_time),
					 "%b %d %H:%M:%S %Y", tm);
#endif

	switch (file_type) {
		case 'd': printf(_("Directory")); break;
		case 's': printf(_("Socket")); break;
		case 'l': printf(_("Symbolic link")); break;
		case 'b': printf(_("Block special file")); break;
		case 'c': printf(_("Character special file")); break;
		case 'p': printf(_("Fifo")); break;
		case '-': printf(_("Regular file")); break;
		default: break;
	}

	printf(_("\tBlocks: %ld"), file_attrib.st_blocks);
#if __FreeBSD__
	printf(_("\tIO Block: %d"), file_attrib.st_blksize);
#else
	printf(_("\tIO Block: %ld"), file_attrib.st_blksize);
#endif
	printf(_("\tInode: %zu\n"), file_attrib.st_ino);
	printf(_("Device: %zu"), file_attrib.st_dev);
	printf(_("\tUid: %u (%s)"), file_attrib.st_uid, (!owner)
			? _("unknown") : owner->pw_name);
	printf(_("\tGid: %u (%s)\n"), file_attrib.st_gid, (!group)
			? _("unknown") : group->gr_name);

	/* Print file timestamps */
	printf(_("Access: \t%s\n"), access_time);
	printf(_("Modify: \t%s\n"), mod_time);
	printf(_("Change: \t%s\n"), change_time);

#if defined(HAVE_ST_BIRTHTIME) || defined(__BSD_VISIBLE) \
	|| defined(_STATX)
		printf(_("Birth: \t\t%s\n"), creation_time);
#endif

	/* Print size */

	if ((file_attrib.st_mode & S_IFMT) == S_IFDIR) {

		if (dsize) {
			fputs(_("Total size: \t"), stdout);
			off_t total_size = dir_size(filename);

			if (total_size != -1) {
				char *human_size = get_size_unit(total_size);

				if (human_size) {
					printf("%s\n", human_size);
					free(human_size);
				}

				else
					puts("?");
			}

			else
				puts("?");
		}
	}

	else
		printf(_("Size: \t\t%s\n"), size_type ? size_type : "?");

	if (size_type)
		free(size_type);

	return EXIT_SUCCESS;
}

static int
properties_function(char **comm)
{
	if(!comm)
		return EXIT_FAILURE;

	size_t i;
	int exit_status = EXIT_SUCCESS;
	int dir_size = 0;

	if (*comm[0] == 'p' && comm[0][1] == 'p' && !comm[0][2])
		dir_size = 1;

	/* If "pr file file..." */
	for (i = 1; i <= args_n; i++) {

		if (strchr(comm[i], '\\')) {
			char *deq_file = dequote_str(comm[i], 0);

			if (!deq_file) {
				fprintf(stderr, _("%s: %s: Error dequoting filename\n"),
						PROGRAM_NAME, comm[i]);
				exit_status = EXIT_FAILURE;
				continue;
			}

			strcpy(comm[i], deq_file);
			free(deq_file);
		}

		if (get_properties(comm[i], dir_size) != 0)
			exit_status = EXIT_FAILURE;
	}

	return exit_status;
}

static int
hidden_function(char **comm)
{
	int exit_status = EXIT_SUCCESS;

	if (strcmp(comm[1], "status") == 0)
		printf(_("%s: Hidden files %s\n"), PROGRAM_NAME,
			   (show_hidden) ? _("enabled") : _("disabled"));

	else if (strcmp(comm[1], "off") == 0) {
		if (show_hidden == 1) {
			show_hidden = 0;

			if (cd_lists_on_the_fly) {
				free_dirlist();
				exit_status = list_dir();
			}
		}
	}

	else if (strcmp(comm[1], "on") == 0) {
		if (show_hidden == 0) {
			show_hidden = 1;

			if (cd_lists_on_the_fly) {
				free_dirlist();
				exit_status = list_dir();
			}
		}
	}

	else
		fputs(_("Usage: hidden, hf [on, off, status]\n"), stderr);

	return exit_status;
}

static int
log_function(char **comm)
/* Log 'comm' into LOG_FILE */
{
	/* If cmd logs are disabled, allow only "log" commands */
	if (!logs_enabled) {
		if (strcmp(comm[0], "log") != 0)
			return EXIT_SUCCESS;
	}

	if (!config_ok)
		return EXIT_FAILURE;

	int clear_log = 0;

	/* If the command was just 'log' */
	if (*comm[0] == 'l' && strcmp(comm[0], "log") == 0 && !comm[1]) {

		FILE *log_fp;
		log_fp = fopen(LOG_FILE, "r");

		if (!log_fp) {
			_err(0, NOPRINT_PROMPT, "%s: log: '%s': %s\n",
				 PROGRAM_NAME, LOG_FILE, strerror(errno));
			return EXIT_FAILURE;
		}

		else {
			size_t line_size = 0;
			char *line_buff = (char *)NULL;
			ssize_t line_len = 0;

			while ((line_len = getline(&line_buff, &line_size, log_fp)) > 0)
				fputs(line_buff, stdout);

			free(line_buff);

			fclose(log_fp);
			return EXIT_SUCCESS;
		}
	}

	else if (*comm[0] == 'l' && strcmp(comm[0], "log") == 0 && comm[1]) {

		if (*comm[1] == 'c' && strcmp(comm[1], "clear") == 0)
			clear_log = 1;

		else if (*comm[1] == 's' && strcmp(comm[1], "status") == 0) {
			printf(_("Logs %s\n"), (logs_enabled) ? _("enabled")
				   : _("disabled"));
			return EXIT_SUCCESS;
		}

		else if (*comm[1] == 'o' && strcmp(comm[1], "on") == 0) {

			if (logs_enabled)
				puts(_("Logs already enabled"));

			else {
				logs_enabled = 1;
				puts(_("Logs successfully enabled"));
			}

			return EXIT_SUCCESS;
		}

		else if (*comm[1] == 'o' && strcmp(comm[1], "off") == 0) {
			/* If logs were already disabled, just exit. Otherwise, log
			 * the "log off" command */
			if (!logs_enabled) {
				puts(_("Logs already disabled"));
				return EXIT_SUCCESS;
			}

			else {
				puts(_("Logs succesfully disabled"));
				logs_enabled = 0;
			}
		}
	}

	/* Construct the log line */

	if (!last_cmd) {
		if (!logs_enabled) {
			/* When cmd logs are disabled, "log clear" and "log off" are
			 * the only commands that can reach this code */

			if (clear_log) {
				last_cmd = (char *)xnmalloc(10, sizeof(char));
				strcpy(last_cmd, "log clear");
			}

			else {
				last_cmd = (char *)xnmalloc(8, sizeof(char));
				strcpy(last_cmd, "log off");
			}
		}
		/* last_cmd should never be NULL if logs are enabled (this
		 * variable is set immediately after taking valid user input
		 * in the prompt function). However ... */

		else {
			last_cmd = (char *)xnmalloc(23, sizeof(char));
			strcpy(last_cmd, _("Error getting command!"));
		}
	}

	char *date = get_date();
	size_t log_len = strlen(date) + strlen(ws[cur_ws].path)
					 + strlen(last_cmd) + 6;
	char *full_log = (char *)xnmalloc(log_len, sizeof(char));

	snprintf(full_log, log_len, "[%s] %s:%s\n", date, ws[cur_ws].path,
			 last_cmd);

	free(date);

	free(last_cmd);
	last_cmd = (char *)NULL;

	/* Write the log into LOG_FILE */
	FILE *log_fp;
	/* If not 'log clear', append the log to the existing logs */

	if (!clear_log)
		log_fp = fopen(LOG_FILE, "a");

	/* Else, overwrite the log file leaving only the 'log clear'
	 * command */
	else
		log_fp = fopen(LOG_FILE, "w+");

	if (!log_fp) {
		_err('e', PRINT_PROMPT, "%s: log: '%s': %s\n", PROGRAM_NAME,
			 LOG_FILE, strerror(errno));
		free(full_log);
		return EXIT_FAILURE;
	}

	else { /* If LOG_FILE was correctly opened, write the log */
		fputs(full_log, log_fp);
		free(full_log);
		fclose(log_fp);

		return EXIT_SUCCESS;
	}
}

static int
history_function(char **comm)
{
	if (!config_ok) {
		fprintf(stderr, _("%s: History function disabled\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	/* If no arguments, print the history list */
	if (args_n == 0) {
		size_t i;
		for (i = 0; i < current_hist_n; i++)
			printf("%zu %s\n", i + 1, history[i]);
		return EXIT_SUCCESS;
	}

	/* If 'history clear', guess what, clear the history list! */
	if (args_n == 1 && strcmp(comm[1], "clear") == 0) {
		FILE *hist_fp = fopen(HIST_FILE, "w+");

		if (!hist_fp) {
			_err(0, NOPRINT_PROMPT, "%s: history: %s: %s\n",
				 PROGRAM_NAME, HIST_FILE, strerror(errno));
			return EXIT_FAILURE;
		}

		/* Do not create an empty file */
		fprintf(hist_fp, "%s %s\n", comm[0], comm[1]);
		fclose(hist_fp);

		/* Reset readline history */
		clear_history();
		read_history(HIST_FILE);
		history_truncate_file(HIST_FILE, max_hist);

		/* Update the history array */
		int exit_status = EXIT_SUCCESS;

		if (get_history() != 0)
			exit_status = EXIT_FAILURE;

		if (log_function(comm) != 0)
			exit_code = EXIT_FAILURE;

		return exit_status;
	}

	/* If 'history -n', print the last -n elements */
	if (args_n == 1 && comm[1][0] == '-' && is_number(comm[1]+1)) {
		int num = atoi(comm[1] + 1);

		if (num < 0 || num > (int)current_hist_n)
			num = (int)current_hist_n;

		size_t i;

		for (i = current_hist_n - (size_t)num; i < current_hist_n; i++)
			printf("%zu %s\n", i + 1, history[i]);

		return EXIT_SUCCESS;
	}

	/* None of the above */
	puts(_("Usage: history [clear] [-n]"));

	return EXIT_SUCCESS;
}

static int
run_history_cmd(const char *cmd)
/* Takes as argument the history cmd less the first exclamation mark.
 * Example: if exec_cmd() gets "!-10" it pass to this function "-10",
 * that is, comm + 1 */
{
	/* If "!n" */
	int exit_status = EXIT_SUCCESS;
	size_t i;

	if (is_number(cmd)) {
		int num = atoi(cmd);

		if (num > 0 && num < (int)current_hist_n) {
			size_t old_args = args_n;

			if (record_cmd(history[num - 1]))
				add_to_cmdhist(history[num - 1]);

			char **cmd_hist = parse_input_str(history[num - 1]);

			if (cmd_hist) {

				char **alias_cmd = check_for_alias(cmd_hist);

				if (alias_cmd) {
					/* If an alias is found, the function frees cmd_hist
					 * and returns alias_cmd in its place to be executed
					 * by exec_cmd() */

					if (exec_cmd(alias_cmd) != 0)
						exit_status = EXIT_FAILURE;

					for (i = 0; alias_cmd[i]; i++)
						free(alias_cmd[i]);

					free(alias_cmd);

					alias_cmd = (char **)NULL;
				}

				else {
					if (exec_cmd(cmd_hist) != 0)
						exit_status = EXIT_FAILURE;

					for (i = 0; cmd_hist[i]; i++)
						free(cmd_hist[i]);

					free(cmd_hist);
				}

				args_n = old_args;

				return exit_status;
			}
			fprintf(stderr, _("%s: Error parsing history command\n"),
					PROGRAM_NAME);
			return EXIT_FAILURE;
		}
		else
			fprintf(stderr, _("%s: !%d: event not found\n"), PROGRAM_NAME, num);
		return EXIT_FAILURE;
	}

	/* If "!!", execute the last command */
	else if (*cmd == '!' && !cmd[1]) {
		size_t old_args = args_n;

		if (record_cmd(history[current_hist_n - 1]))
			add_to_cmdhist(history[current_hist_n - 1]);

		char **cmd_hist = parse_input_str(history[current_hist_n - 1]);

		if (cmd_hist) {

			char **alias_cmd = check_for_alias(cmd_hist);

			if (alias_cmd) {

				if (exec_cmd(alias_cmd) != 0)
					exit_status = EXIT_FAILURE;

				for (i = 0; alias_cmd[i]; i++)
					free(alias_cmd[i]);

				free(alias_cmd);

				alias_cmd = (char **)NULL;
			}

			else {
				if (exec_cmd(cmd_hist) != 0)
					exit_status = EXIT_FAILURE;

				for (i = 0; cmd_hist[i]; i++)
					free(cmd_hist[i]);

				free(cmd_hist);
			}

			args_n = old_args;
			return exit_status;
		}
		fprintf(stderr, _("%s: Error parsing history command\n"),
				PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	/* If "!-n" */
	else if (*cmd == '-') {
		/* If not number or zero or bigger than max... */
		int acmd = atoi(cmd + 1);

		if (!is_number(cmd + 1) || acmd == 0
		|| acmd > (int)current_hist_n - 1) {
			fprintf(stderr, _("%s: !%s: Event not found\n"), PROGRAM_NAME, cmd);
			return EXIT_FAILURE;
		}

		size_t old_args = args_n;
		char **cmd_hist = parse_input_str(history[current_hist_n
										  - (size_t)acmd - 1]);
		if (cmd_hist) {

			char **alias_cmd = check_for_alias(cmd_hist);

			if (alias_cmd) {
				if (exec_cmd(alias_cmd) != 0)
					exit_status = EXIT_FAILURE;

				for (i = 0; alias_cmd[i]; i++)
					free(alias_cmd[i]);

				free(alias_cmd);

				alias_cmd = (char **)NULL;
			}

			else {
				if (exec_cmd(cmd_hist) != 0)
					exit_status = EXIT_FAILURE;

				for (i = 0; cmd_hist[i]; i++)
					free(cmd_hist[i]);

				free(cmd_hist);
			}

			if (record_cmd(history[current_hist_n - (size_t)acmd - 1]))
				add_to_cmdhist(history[current_hist_n - (size_t)acmd - 1]);

			args_n = old_args;
			return exit_status;
		}

		if (record_cmd(history[current_hist_n - (size_t)acmd - 1]))
			add_to_cmdhist(history[current_hist_n - (size_t)acmd - 1]);

		fprintf(stderr, _("%s: Error parsing history command\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	else if ((*cmd >= 'a' && *cmd <= 'z')
	|| (*cmd >= 'A' && *cmd <= 'Z')) {

		size_t len = strlen(cmd), old_args = args_n;;

		for (i = 0; history[i]; i++) {

			if (*cmd == *history[i]
			&& strncmp(cmd, history[i], len) == 0) {

				char **cmd_hist = parse_input_str(history[i]);

				if (cmd_hist) {

					char **alias_cmd = check_for_alias(cmd_hist);

					if (alias_cmd) {

						if (exec_cmd(alias_cmd) != EXIT_SUCCESS)
							exit_status = EXIT_FAILURE;

						for (i = 0; alias_cmd[i]; i++)
							free(alias_cmd[i]);

						free(alias_cmd);

						alias_cmd = (char **)NULL;
					}

					else {
						if (exec_cmd(cmd_hist) != EXIT_SUCCESS)
							exit_status = EXIT_FAILURE;

						for (i = 0; cmd_hist[i]; i++)
							free(cmd_hist[i]);

						free(cmd_hist);
					}

					args_n = old_args;
					return exit_status;
				}
			}
		}

		fprintf(stderr, _("%s: !%s: Event not found\n"), PROGRAM_NAME, cmd);

		return EXIT_FAILURE;
	}

	else {
		printf(_("Usage:\n\
!!: Execute the last command.\n\
!n: Execute the command number 'n' in the history list.\n\
!-n: Execute the last-n command in the history list.\n"));
		return EXIT_SUCCESS;
	}
}

static int
regen_config(void)
/* Regenerate the configuration file and create a back up of the old
 * one */
{
	int config_found = 1;
	struct stat config_attrib;

	if (stat(CONFIG_FILE, &config_attrib) == -1) {
		puts(_("No configuration file found"));
		config_found = 0;
	}

	if (config_found) {
		time_t rawtime = time(NULL);
		struct tm *t = localtime(&rawtime);

		char date[18];
		strftime(date, 18, "%Y%m%d@%H:%M:%S", t);

		char *bk = (char *)xnmalloc(strlen(CONFIG_FILE) + strlen(date)
									+ 2, sizeof(char));
		sprintf(bk, "%s.%s", CONFIG_FILE, date);

		char *cmd[] = { "mv", CONFIG_FILE, bk, NULL };

		if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS) {
			free(bk);
			return EXIT_FAILURE;
		}

		printf(_("Old configuration file stored as '%s'\n"), bk);
		free(bk);
	}

	if (create_config(CONFIG_FILE) != EXIT_SUCCESS)
		return EXIT_FAILURE;

	printf(_("New configuration file written to '%s'\n"), CONFIG_FILE);

	reload_config();

	return EXIT_SUCCESS;
}

static int
edit_function (char **comm)
/* Edit the config file, either via the mime function or via the first
 * passed argument (Ex: 'edit nano'). The 'gen' option regenerates
 * the configuration file and creates a back up of the old one. */
{
	if (xargs.stealth_mode == 1) {
		printf(_("%s: Access to configuration files is not allowed in "
			   "stealth mode\n"), PROGRAM_NAME);
		return EXIT_SUCCESS;
	}

	if (comm[1] && strcmp(comm[1], "gen") == 0)
		return regen_config();

	if (!config_ok) {
		fprintf(stderr, _("%s: Cannot access the configuration file\n"),
				PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	/* Get modification time of the config file before opening it */
	struct stat file_attrib;

	/* If, for some reason (like someone erasing the file while the
	 * program is running) clifmrc doesn't exist, call init_config()
	 * to recreate the configuration file. Then run 'stat' again to
	 * reread the attributes of the file */
	if (stat(CONFIG_FILE, &file_attrib) == -1) {
		create_config(CONFIG_FILE);
		stat(CONFIG_FILE, &file_attrib);
	}

	time_t mtime_bfr = (time_t)file_attrib.st_mtime;

	int ret = EXIT_SUCCESS;

	/* If there is an argument... */
	if (comm[1]) {
		char *cmd[] = { comm[1], CONFIG_FILE, NULL };
		ret = launch_execve(cmd, FOREGROUND, E_NOSTDERR);
	}

	/* If no application has been passed as 2nd argument */
	else {
		if (!(flags & FILE_CMD_OK)) {
			fprintf(stderr, _("%s: file: Command not found. Try "
					"'edit APPLICATION'\n"), PROGRAM_NAME);
			ret = EXIT_FAILURE;
		}

		else {
			char *cmd[] = { "mime", CONFIG_FILE, NULL };
			ret = mime_open(cmd);
		}
	}

	if (ret != EXIT_SUCCESS)
		return EXIT_FAILURE;

	/* Get modification time after opening the config file */
	stat(CONFIG_FILE, &file_attrib);
	/* If modification times differ, the file was modified after being
	 * opened */

	if (mtime_bfr != (time_t)file_attrib.st_mtime) {
		/* Reload configuration only if the config file was modified */

		reload_config();
		welcome_message = 0;

		if (cd_lists_on_the_fly) {
			free_dirlist();
			ret = list_dir();
		}
	}

	return ret;
}

static void
color_codes (void)
/* List color codes for file types used by the program */
{
	if (!colorize) {
		printf(_("%s: Currently running without colors\n"),
			   PROGRAM_NAME);
		return;
	}

	if (ext_colors_n)
		printf(_("%sFile type colors%s\n\n"), bold, df_c);

	printf(_(" %sfile name%s: Directory with no read permission (nd)\n"),
		   nd_c, df_c);
	printf(_(" %sfile name%s: File with no read permission (nf)\n"),
		   nf_c, df_c);
	printf(_(" %sfile name%s: Directory* (di)\n"), di_c, df_c);
	printf(_(" %sfile name%s: EMPTY directory (ed)\n"), ed_c, df_c);
	printf(_(" %sfile name%s: EMPTY directory with no read "
			 "permission (ne)\n"), ne_c, df_c);
	printf(_(" %sfile name%s: Executable file (ex)\n"), ex_c, df_c);
	printf(_(" %sfile name%s: Empty executable file (ee)\n"), ee_c, df_c);
	printf(_(" %sfile name%s: Block special file (bd)\n"), bd_c, df_c);
	printf(_(" %sfile name%s: Symbolic link* (ln)\n"), ln_c, df_c);
	printf(_(" %sfile name%s: Broken symbolic link (or)\n"), or_c, df_c);
	printf(_(" %sfile name%s: Multi-hardlink (mh)\n"), mh_c, df_c);
	printf(_(" %sfile name%s: Socket file (so)\n"), so_c, df_c);
	printf(_(" %sfile name%s: Pipe or FIFO special file (pi)\n"), pi_c, df_c);
	printf(_(" %sfile name%s: Character special file (cd)\n"), cd_c, df_c);
	printf(_(" %sfile name%s: Regular file (fi)\n"), fi_c, df_c);
	printf(_(" %sfile name%s: Empty (zero-lenght) file (ef)\n"), ef_c, df_c);
	printf(_(" %sfile name%s: SUID file (su)\n"), su_c, df_c);
	printf(_(" %sfile name%s: SGID file (sg)\n"), sg_c, df_c);
	printf(_(" %sfile name%s: File with capabilities (ca)\n"), ca_c, df_c);
	printf(_(" %sfile name%s: Sticky and NOT other-writable "
			 "directory* (st)\n"),  st_c, df_c);
	printf(_(" %sfile name%s: Sticky and other-writable "
			 "directory* (tw)\n"),  tw_c, df_c);
	printf(_(" %sfile name%s: Other-writable and NOT sticky "
			 "directory* (ow)\n"),  ow_c, df_c);
	printf(_(" %sfile name%s: Unknown file type (no)\n"), no_c, df_c);
	printf(_(" %sfile name%s: Unaccessible (non-stat'able) file "
		   "(uf)\n"), uf_c, df_c);

	printf(_("\n*The slash followed by a number (/xx) after directories "
			 "or symbolic links to directories indicates the amount of "
			 "files contained by the corresponding directory, excluding "
			 "self (.) and parent (..) directories.\n"));
	printf(_("\nThe value in parentheses is the code that is to be used "
			 "to modify the color of the corresponding filetype in the "
			 "color scheme file (in the \"FiletypeColors\" line), "
			 "using the same ANSI style color format used by dircolors. "
			 "By default, %s uses only 8 colors, but you can use 256 "
			 "and RGB colors as well.\n\n"), PROGRAM_NAME);

	if (ext_colors_n) {
		size_t i, j;

		printf(_("%sExtension colors%s\n\n"), bold, df_c);
		for (i = 0; i < ext_colors_n; i++) {
			char *ret = strrchr(ext_colors[i], '=');

			if (!ret)
				continue;

			printf(" \x1b[%sm", ret + 1);

			for (j = 0; ext_colors[i][j] != '='; j++)
				putchar(ext_colors[i][j]);

			puts("\x1b[0m");
		}

		putchar('\n');
	}
}

static int
list_commands (void)
/* Instead of recreating here the commands description, just jump to the
 * corresponding section in the manpage */
{
	char *cmd[] = { "man", "-P", "less -p ^COMMANDS", PNL, NULL };
	if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

static void
help_function (void)
{
	printf(_("%s %s (%s), by %s\n"), PROGRAM_NAME, VERSION, DATE, AUTHOR);

	printf(_("\nUSAGE: %s %s\n\
\n -a, --no-hidden\t\t do not show hidden files (default)\
\n -A, --show-hidden\t\t show hidden files\
\n -b, --bookmarks-file=FILE\t specify an alternative bookmarks file\
\n -c, --config-file=FILE\t\t specify an alternative configuration file\
\n -e, --no-eln\t\t\t do not print ELN (entry list number) at \
\n              the left of each filename \
\n -f, --no-folders-first\t\t do not list folders first\
\n -F, --folders-first\t\t list folders first (default)\
\n -g, --pager\t\t\t enable the pager\
\n -G, --no-pager\t\t\t disable the pager (default)\
\n -h, --help\t\t\t show this help and exit\
\n -i, --no-case-sensitive\t no case-sensitive files listing (default)\
\n -I, --case-sensitive\t\t case-sensitive files listing\
\n -k, --keybindings-file=FILE\t specify an alternative keybindings file\
\n -l, --no-long-view\t\t disable long view mode (default)\
\n -L, --long-view\t\t enable long view mode\
\n -m, --dihist-map\t\t enable the directory history map\
\n -o, --no-list-on-the-fly\t 'cd' works as the shell 'cd' command\
\n -O, --list-on-the-fly\t\t 'cd' lists files on the fly (default)\
\n -p, --path PATH\t\t use PATH as %s starting path\
\n -P, --profile=PROFILE\t\t use (or create) PROFILE as profile\
\n -s, --splash \t\t\t enable the splash screen\
\n -S, --stealth-mode \t\t leave no trace on the host system.\
\n              Nothing is read from any file nor any file \
\n              is created: all settings are set to the \
\n              default value. However, most settings can \
\n              be controlled via command line options\
\n -u, --no-unicode \t\t disable unicode\
\n -U, --unicode \t\t\t enable unicode to correctly list filenames \
\n              containing accents, tildes, umlauts, \
\n              non-latin letters, etc. This option is \
\n              enabled by default for non-english locales\
\n -v, --version\t\t\t show version details and exit\
\n -w, --workspace=NUM\t\t start in workspace NUM\
\n -x, --ext-cmds\t\t\t allow the use of external commands\
\n -y, --light-mode\t\t enable the light mode\
\n -z, --sort=METHOD\t\t sort files by METHOD, where METHOD \
\n              could be: 0 = none, 1 = name, 2 = size, \
\n              3 = atime, 4 = btime, 5 = ctime, \
\n              6 = mtime, 7 = version, 8 = extension, \
\n              9 = inode, 10 = owner, 11 = group"),
		   PNL, GRAL_USAGE, PROGRAM_NAME);



	printf("\
\n     --case-ins-dirjump\t consult the jump database ignoring \
\n              case\
\n     --case-ins-path-comp\t TAB complete paths ignoring case\
\n     --cd-on-quit\t\t write last visited path to \
\n              $XDG_CONFIG_HOME/clifm/.last to be accessed\
\n              later by a shell funtion. See the manpage\
\n     --color-scheme=NAME\t use color scheme NAME\
\n     --cwd-in-title\t\t print current directory in terminal \
\n              window title\
\n     --disk-usage\t\t show disk usage (free/total) for the\
\n              filesystem to which the current directory \
\n              belongs\
\n     --enable-logs\t\t enable program logs\
\n     --expand-bookmarks\t\t expand bookmark names into the \
\n              corresponding bookmark paths. TAB \
\n              completion for bookmark names is also \
\n              available\
\n     --icons\t\t\t enable icons\
\n     --icons-use-file-color\t icons color follows file color\
\n     --list-and-quit\t\t list files and quit. It may be used\
\n              in conjunction with -p\
\n     --max-dirhist\t\t maximum number of visited directories to \
\n              remember\
\n     --max-files=NUM\t\t list only up to NUM files\
\n     --max-path=NUM\t\t set the maximun number of characters \
\n              after which the current directory in the \
\n              prompt line will be abreviated to the \
\n              directory base name (if \\z is used in \
\n              the prompt\
\n     --no-dir-jumper\t\t disable the directory jumper function\
\n     --no-cd-auto\t\t by default, %s changes to directories \
\n\t\t\t\tby just specifying the corresponding ELN \
\n              (e.g. '12' instead of 'cd 12'). This \
\n              option forces the use of 'cd'\
\n     --no-classify\t\tDo not append filetype indicators\
\n     --no-clear-screen\t\t do not clear the screen when listing \
\n              directories\
\n     --no-colors\t\t disable filetype colors for files listing \
\n     --no-columns\t\t disable columned files listing\
\n     --no-files-counter\t\t disable the files counter for \
\n              directories. This option is especially \
\n              useful to speed up the listing process; \
\n              counting files in directories is expensive\
\n     --no-open-auto\t\t same as no-cd-auto, but for files\
\n     --no-tips\t\t\t disable startup tips\
\n     --no-welcome-message\t disable the welcome message\
\n     --only-dirs\t\t list only directories and symbolic links\
\n              to directories\
\n     --open=FILE\t run as a stand-alone resource opener: open\
\n              FILE and exit\
\n     --opener=APPLICATION\t resource opener to use instead of 'lira',\
\n              %s built-in opener\
\n     --restore-last-path\t save last visited directory to be \
\n              restored in the next session\
\n     --rl-vi-mode\t\t set readline to vi editing mode (defaults \
\n              to emacs editing mode)\
\n     --share-selbox\t\t make the Selection Box common to \
\n              different profiles\
\n     --sort-reverse\t\t sort in reverse order, for example: z-a \
\n              instead of a-z, which is the default order)\
\n     --trash-as-rm\t\t the 'r' command executes 'trash' instead of \
				'rm' to prevent accidental deletions\n",
		 PROGRAM_NAME, PROGRAM_NAME);

	puts(_("\nBUILT-IN COMMANDS:\n\n\
 ELN/FILE/DIR (auto-open and autocd functions)\n\
 /PATTERN [DIR] [-filetype] [-x] (quick search)\n\
 ;[CMD], :[CMD] (run CMD via the system shell)\n\
 ac, ad ELN/FILE ... (archiving functions)\n\
 acd, autocd [on, off, status]\n\
 actions [edit]\n\
 alias [import FILE]\n\
 ao, auto-open [on, off, status]\n\
 b, back [h, hist] [clear] [!ELN]\n\
 bl ELN/FILE ... (batch links)\n\
 bm, bookmarks [a, add PATH] [d, del] [edit] [SHORTCUT or NAME]\n\
 br, bulk ELN/FILE ...\n\
 c, l [e, edit], m, md, r (copy, link, move, makedir, and remove)\n\
 cc, colors\n\
 cd [ELN/DIR]\n\
 cl, columns [on, off]\n\
 cmd, commands\n\
 cs, colorscheme [edit] [COLORSCHEME]\n\
 ds, desel [*, a, all]\n\
 edit [APPLICATION]\n\
 exp, export [ELN/FILE ...]\n\
 ext [on, off, status]\n\
 f, forth [h, hist] [clear] [!ELN]\n\
 fc, filescounter [on, off, status]\n\
 ff, folders-first [on, off, status]\n\
 fs\n\
 ft, filter [unset] [REGEX]\n\
 hf, hidden [on, off, status]\n\
 history [clear] [-n]\n\
 icons [on, off]\n\
 j, jc, jp, jl [STRING ...] jo [NUM], je (directory jumper function)\n\
 kb, keybinds [edit] [reset]\n\
 lm [on, off] (lightmode)\n\
 log [clear]\n\
 mf NUM (List up to NUM files)\n\
 mm, mime [info ELN/FILE] [edit] (resource opener)\n\
 mp, mountpoints\n\
 msg, messages [clear]\n\
 n, net [smb, ftp, sftp]://ADDRESS [OPTIONS]\n\
 o, open [ELN/FILE] [APPLICATION]\n\
 opener [default] [APPLICATION]\n\
 p, pr, pp, prop [ELN/FILE ... n]\n\
 path, cwd\n\
 pf, prof, profile [ls, list] [set, add, del PROFILE]\n\
 pg, pager [on, off, status]\n\
 pin [FILE/DIR]\n\
 q, quit, exit\n\
 Q\n\
 rf, refresh\n\
 rl, reload\n\
 s, sel ELN/FILE... [[!]PATTERN] [-FILETYPE] [:PATH]\n\
 sb, selbox\n\
 shell [SHELL]\n\
 splash\n\
 st, sort [METHOD] [rev]\n\
 t, tr, trash [ELN/FILE ... n] [ls, list] [clear] [del, rm]\n\
 te [FILE(s)]\n\
 tips\n\
 u, undel, untrash [*, a, all]\n\
 uc, unicode [on, off, status]\n\
 unpin\n\
 v, vv, paste sel [DESTINY]\n\
 ver, version\n\
 ws [NUM, +, -] (workspaces)\n\
 x, X [ELN/DIR] (new instance)\n"));

	puts(_("Run 'cmd' (F2) or consult the manpage (F1) for "
		   "more information about each of these commands. You "
		   "can also try the 'ih' action to run the interactive "
		   "help plugin (depends on fzf). Just enter 'ih', that's "
		   "it.\n"));

	printf(_("DEFAULT KEYBOARD SHORTCUTS:\n\n"
" M-c: Clear the current command line buffer\n\
 M-f: Toggle list-folders-first on/off\n\
 C-r: Refresh the screen\n\
 M-l: Toggle long view mode on/off\n\
 M-m: List mountpoints\n\
 M-t: Clear messages\n\
 M-h: Show directory history\n\
 M-i, M-.: Toggle hidden files on/off\n\
 M-s: Open the Selection Box\n\
 M-a: Select all files in the current working directory\n\
 M-d: Deselect all selected files\n\
 M-r: Change to the root directory\n\
 M-e, Home: Change to the home directory\n\
 M-u, S-Up: Change to the parent directory\n\
 M-j, S-Left: Change to previous visited directory\n\
 M-k, S-Right: Change to next visited directory\n\
 M-o: Lock terminal\n\
 M-p: Change to pinned directory\n\
 M-1: Switch to workspace 1\n\
 M-2: Switch to workspace 2\n\
 M-3: Switch to workspace 3\n\
 M-4: Switch to workspace 4\n\
 C-M-j: Change to first visited directory\n\
 C-M-k: Change to last visited directory\n\
 C-M-o: Switch to previous profile\n\
 C-M-p: Switch to next profile\n\
 C-M-a: Archive selected files\n\
 C-M-e: Export selected files\n\
 C-M-r: Rename selected files\n\
 C-M-d: Remove selected files\n\
 C-M-t: Trash selected files\n\
 C-M-u: Restore trashed files\n\
 C-M-b: Bookmark last selected file or directory\n\
 C-M-g: Open/change-into last selected file/directory\n\
 C-M-n: Move selected files into the current working directory\n\
 C-M-v: Copy selected files into the current working directory\n\
 M-y: Toggle light mode on/off\n\
 M-z: Switch to previous sorting method\n\
 M-x: Switch to next sorting method\n\
 C-x: Launch a new instance\n\
 F1: Manual page\n\
 F2: Commands help\n\
 F3: Keybindings help\n\
 F6: Open the MIME list file\n\
 F7: Open the jump database file\n\
 F8: Open the current color scheme file\n\
 F9: Open the keybindings file\n\
 F10: Open the configuration file\n\
 F11: Open the bookmarks file\n\
 F12: Quit\n\n"
"NOTE: C stands for Ctrl, S for Shift, and M for Meta (Alt key in "
"most keyboards)\n\n"));

	puts(_("Run the 'colors' or 'cc' command to see the list "
		   "of currently used color codes.\n"));

	puts(_("The configuration and profile files allow you to customize "
		   "colors, define some prompt commands and aliases, and more. "
		   "For a full description consult the manpage."));
}

static void
free_software (void)
{
	puts(_("Excerpt from 'What is Free Software?', by Richard Stallman. \
Source: https://www.gnu.org/philosophy/free-sw.html\n \
\n\"'Free software' means software that respects users' freedom and \
community. Roughly, it means that the users have the freedom to run, \
copy, distribute, study, change and improve the software. Thus, 'free \
software' is a matter of liberty, not price. To understand the concept, \
you should think of 'free' as in 'free speech', not as in 'free beer'. \
We sometimes call it 'libre software', borrowing the French or Spanish \
word for 'free' as in freedom, to show we do not mean the software is \
gratis.\n\
\nWe campaign for these freedoms because everyone deserves them. With \
these freedoms, the users (both individually and collectively) control \
the program and what it does for them. When users don't control the \
program, we call it a 'nonfree' or proprietary program. The nonfree \
program controls the users, and the developer controls the program; \
this makes the program an instrument of unjust power. \n\
\nA program is free software if the program's users have the four \
essential freedoms:\n\n\
- The freedom to run the program as you wish, for any purpose \
(freedom 0).\n\
- The freedom to study how the program works, and change it so it does \
your computing as you wish (freedom 1). Access to the source code is a \
precondition for this.\n\
- The freedom to redistribute copies so you can help your neighbor \
(freedom 2).\n\
- The freedom to distribute copies of your modified versions to others \
(freedom 3). By doing this you can give the whole community a chance to \
benefit from your changes. Access to the source code is a precondition \
for this. \n\
\nA program is free software if it gives users adequately all of these \
freedoms. Otherwise, it is nonfree. While we can distinguish various \
nonfree distribution schemes in terms of how far they fall short of \
being free, we consider them all equally unethical (...)\""));
}

static void
version_function (void)
{
	printf(_("%s %s (%s), by %s\nContact: %s\nWebsite: "
		   "%s\nLicense: %s\n"), PROGRAM_NAME, VERSION, DATE,
		   AUTHOR, CONTACT, WEBSITE, LICENSE);
}

static void
splash (void)
{
	printf("\n%s                         xux\n"
	"       :xuiiiinu:.......u@@@u........:xunninnu;\n"
	"    .xi#@@@@@@@@@n......x@@@l.......x#@@@@@@@@@:...........:;unnnu;\n"
	"  .:i@@@@lnx;x#@@i.......l@@@u.....x#@@lu;:;;..;;nnll#llnnl#@@@@@@#u.\n"
	"  .i@@@i:......::........;#@@#:....i@@@x......;@@@@@@@@@@@@@#iuul@@@n.\n"
	"  ;@@@#:..........:nin:...n@@@n....n@@@nunlll;;@@@@i;:xl@@@l:...:l@@@u.\n"
	"  ;#@@l...........x@@@l...;@@@#:...u@@@@@@@@@n:i@@@n....i@@@n....;#@@#;.\n"
	"  .l@@@;...........l@@@x...i@@@u...x@@@@iux;:..;#@@@x...:#@@@;....n@@@l.\n"
	"  .i@@@x...........u@@@i...;@@@l....l@@@;.......u@@@#:...;nin:.....l@@@u.\n"
	"  .n@@@i:..........:l@@@n...xnnx....u@@@i........i@@@i.............x@@@#:\n"
	"   :l@@@i...........:#@@@;..........:@@@@x.......:l@@@u.............n@@@n.\n"
	"    :l@@@i;.......unni@@@#:.:xnlli;..;@@@#:.......:l@@u.............:#@@n.\n"
	"     ;l@@@@#lnuxxi@@@i#@@@##@@@@@#;...xlln.         :.                ;:.\n"
	"      :xil@@@@@@@@@@l:u@@@@##lnx;.\n"
	"         .:xuuuunnu;...;ux;.", d_cyan);

	printf(_("\n\t\t   %sThe anti-eye-candy/KISS file manager\n%s"),
		   white, df_c);

	if (splash_screen) {
		printf(_("\n\t\t\tPress any key to continue... "));
		xgetchar(); putchar('\n');
	}
	else
		putchar('\n');
}

static void
bonus_function (void)
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
		"An Archer knows her target like the back of her hands",
		"\"I only know that I know nothing\" (Socrates)",
		"(Learned) Ignorance is the true outcome of wisdom (Nicholas "
		"of Cusa)",
		"True intelligence is about questions, not about answers",
		"Humanity is just an arrow released towards God",
		"Buzz is right: infinity is our only and ultimate goal",
		"That stain will never ever be erased (La 12)",
		"\"A work of art is never finished, but adandoned\" (J. L. Guerrero)",
		"At the beginning, software was hardware; but today hardware is "
		"being absorbed by software", NULL };

	size_t num = (sizeof(phrases) / sizeof(phrases[0])) - 1;

	srand((unsigned int)time(NULL));
	puts(phrases[rand() % num]);
}

static int
list_dir_light(void)
/* List files in the current working directory (global variable 'path').
 * Unlike list_dir(), however, this function uses no color and runs
 * neither stat() nor count_dir(), which makes it quite faster. Return
 * zero on success or one on error */
{
/*  clock_t start = clock(); */

	DIR *dir;

	struct dirent *ent;

	if ((dir = opendir(ws[cur_ws].path)) == NULL) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, ws[cur_ws].path,
				strerror(errno));
		return EXIT_FAILURE;
	}

	errno = 0;
	longest = 0;
	register unsigned int n = 0;
	unsigned int total_dents = 0, count = 0;

	file_info = (struct fileinfo *)xnmalloc(ENTRY_N + 2,
								   sizeof(struct fileinfo));

	while ((ent = readdir(dir))) {

		char *ename = ent->d_name;

		/* Skip self and parent directories */
		if (*ename == '.' && (!ename[1] || (ename[1] == '.'
		&& !ename[2])))
			continue;

		/* Skip files matching FILTER */
		if (filter && regexec(&regex_exp, ename, 0, NULL, 0)
		== EXIT_SUCCESS)
			continue;

		if (!show_hidden && *ename == '.')
			continue;

		if (only_dirs && ent->d_type != DT_DIR)
			continue;

		if (count > ENTRY_N) {
			count = 0;
			total_dents = n + ENTRY_N;
			file_info = xrealloc(file_info, (total_dents + 2)
							* sizeof(struct fileinfo));
		}

		file_info[n].name = (char *)xnmalloc(NAME_MAX + 1, sizeof(char));

		if (!unicode)
			file_info[n].len = (xstrsncpy(file_info[n].name, ename,
										  NAME_MAX + 1) - 1);
		else {
			xstrsncpy(file_info[n].name, ename, NAME_MAX + 1);
			file_info[n].len = wc_xstrlen(ename);
		}

		/* ################  */
		file_info[n].dir = (ent->d_type == DT_DIR) ? 1 : 0;
		file_info[n].symlink = (ent->d_type == DT_LNK) ? 1 : 0;
		file_info[n].type = ent->d_type;
		file_info[n].inode = ent->d_ino;
		file_info[n].linkn = 1;
		file_info[n].size = 1;
		file_info[n].color = (char *)NULL;

		file_info[n].icon = DEF_FILE_ICON;
		file_info[n].icon_color = DEF_FILE_ICON_COLOR;

		file_info[n].exec = 0;
		file_info[n].ruser = 1;
		file_info[n].filesn = 0;

		file_info[n].time = 0;

		switch(file_info[n].type) {

			case DT_DIR:
				if (icons) {
					get_dir_icon(file_info[n].name, (int)n);

					/* If set from the color scheme file */
					if (*dir_ico_c)
						file_info[n].icon_color = dir_ico_c;
				}

				files_counter
				? (file_info[n].filesn = (count_dir(ename) - 2))
				: (file_info[n].filesn = 1);

				if (file_info[n].filesn > 0)
					file_info[n].color = di_c;

				else if (file_info[n].filesn == 0)
					file_info[n].color = ed_c;

				else {
					file_info[n].color = nd_c;
					file_info[n].icon = ICON_LOCK;
					file_info[n].icon_color = YELLOW;
				}

				break;

			case DT_LNK:
				file_info[n].icon = ICON_LINK;
				file_info[n].color = ln_c;
				break;

			case DT_REG: file_info[n].color = fi_c; break;

			case DT_SOCK: file_info[n].color = so_c; break;

			case DT_FIFO: file_info[n].color = pi_c; break;

			case DT_BLK: file_info[n].color = bd_c; break;

			case DT_CHR: file_info[n].color = cd_c; break;

			case DT_UNKNOWN: file_info[n].color = uf_c; break;

			default: file_info[n].color = df_c; break;
		}

		if (xargs.icons_use_file_color == 1 && icons)
			file_info[n].icon_color = file_info[n].color;

		n++;
		count++;
	}

	file_info[n].name = (char *)NULL;

	files = (size_t)n;

	if (n == 0) {
		printf("%s. ..%s\n", colorize ? di_c : df_c, df_c);
		free(file_info);
		if (closedir(dir) == -1)
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (sort)
		qsort(file_info, n, sizeof(*file_info), entrycmp);

	/* Get terminal current amount of rows and columns */
	struct winsize w;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
	/* ws_col and ws_row are both unsigned short int according to
	 * /bits/ioctl-types.h */
	term_cols = w.ws_col; /* This one is global */
	unsigned short term_rows = w.ws_row;

	int reset_pager = 0, c, i;
	register size_t counter = 0;

	size_t columns_n = 1;

	/* Get the longest filename */

	if (columned || long_view) {
		i = (int)n;
		while (--i >= 0) {
			size_t total_len = 0;
			file_info[i].eln_n = no_eln ? -1 : DIGINUM(i + 1);
			total_len = file_info[i].eln_n + 1 + file_info[i].len;

			if (!long_view && classify) {
				if (file_info[i].dir)
					total_len += 2;

				if (file_info[i].filesn > 0 && files_counter)
					total_len += DIGINUM(file_info[i].filesn);

				if (!file_info[i].dir && !colorize) {
					switch(file_info[i].type) {
						case DT_REG:
							if (file_info[i].exec)
								total_len += 1;
							break;
						case DT_LNK: /* fallthrough */
						case DT_SOCK: /* fallthrough */
						case DT_FIFO: /* fallthrough */
						case DT_UNKNOWN:
							total_len += 1;
							break;
					}
				}
			}

			if (total_len > longest)
				longest = total_len;
		}

		if (icons && !long_view && columned)
			longest += 3;
	}

				/* ########################
				 * #    LONG VIEW MODE    #
				 * ######################## */

	if (long_view) {

		struct stat lattr;
		int space_left = (int)term_cols - MAX_PROP_STR;

		if (space_left < min_name_trim)
			space_left = min_name_trim;

		if ((int)longest < space_left)
			space_left = (int)longest;

		int k = (int)files;
		for (i = 0; i < k; i++) {

			if (max_files != UNSET && i == max_files)
				break;

			if (lstat(file_info[i].name, &lattr) == -1)
				continue;

			if (pager) {

				if (counter > (size_t)(term_rows - 2)) {

					fputs("\x1b[7;97m--Mas--\x1b[0;49m", stdout);

					switch (c = xgetchar()) {

					/* Advance one line at a time */
					case 66: /* fallthrough */ /* Down arrow */
					case 10: /* fallthrough */ /* Enter */
					case 32: /* Space */
						break;

					/* Advance one page at a time */
					case 126: counter = 0; /* Page Down */
						break;

					case 63: /* fallthrough */ /* ? */
					case 104: { /* h: Print pager help */
						CLEAR;

						fputs(_("?, h: help\n"
						"Down arrow, Enter, Space: Advance one line\n"
						"Page Down: Advance one page\n"
						"q: Stop pagging\n"), stdout);

						int l = (int)term_rows - 5;
						while (--l >= 0)
							putchar('\n');

						fputs("\x1b[7;97m--Mas--\x1b[0;49m", stdout);

						i -= (term_rows - 1);

						if (i < 0)
							i = 0;

						counter = 0;
						xgetchar();
						CLEAR;
					}
					break;

					/* Stop paging (and set a flag to reenable the pager
					 * later) */
					case 99:  /* 'c' */
					case 112: /* 'p' */
					case 113: pager = 0, reset_pager = 1; /* 'q' */
						break;

					/* If another key is pressed, go back one position.
					 * Otherwise, some filenames won't be listed.*/
					default:
						i--;
						fputs("\r\x1b[K\x1b[3J", stdout);
						continue;
					}

					fputs("\r\x1b[K\x1b[3J", stdout);
				}

				counter++;
			}

			file_info[i].uid = lattr.st_uid;
			file_info[i].gid = lattr.st_gid;
			file_info[i].ltime = (time_t)lattr.st_mtim.tv_sec;
			file_info[i].mode = lattr.st_mode;
			file_info[i].size = lattr.st_size;

			/* Print ELN. The remaining part of the line will be
			 * printed by print_entry_props() */
			if (!no_eln)
				printf("%s%d%s ", el_c, i + 1, df_c);

			print_entry_props(&file_info[i], (size_t)space_left);
		}

		goto END;
	}

				/* ########################
				 * #   NORMAL VIEW MODE   #
				 * ######################## */

	int last_column = 0;

	/* Get possible amount of columns for the dirlist screen */
	if (!columned)
		columns_n = 1;

	else {
		columns_n = (size_t)term_cols / (longest + 1); /* +1 for the
		space between file names */

		/* If longest is bigger than terminal columns, columns_n will
		 * be negative or zero. To avoid this: */
		if (columns_n < 1)
			columns_n = 1;

		/* If we have only three files, we don't want four columns */
		if (columns_n > (size_t)n)
			columns_n = (size_t)n;
	}

	int nn = (int)n;
	size_t cur_cols = 0;
	for (i = 0; i < nn; i++) {

		if (max_files != UNSET && i == max_files)
			break;

		/* A basic pager for directories containing large amount of
		 * files. What's missing? It only goes downwards. To go
		 * backwards, use the terminal scrollback function */
		if (pager) {
			/* Run the pager only once all columns and rows fitting in
			 * the screen are filled with the corresponding filenames */
			if (last_column
			&& counter > columns_n * ((size_t)term_rows - 2)) {

				printf("\x1b[7;97m--Mas--\x1b[0;49m");

				switch (c = xgetchar()) {

				/* Advance one line at a time */
				case 66: /* fallthrough */ /* Down arrow */
				case 10: /* fallthrough */ /* Enter */
				case 32: /* Space */
					break;

				/* Advance one page at a time */
				case 126: counter = 0; /* Page Down */
					break;

				case 63: /* fallthrough */ /* ? */
				case 104: { /* h: Print pager help */
						CLEAR;

						fputs(_("?, h: help\n"
						"Down arrow, Enter, Space: Advance one line\n"
						"Page Down: Advance one page\n"
						"q: Stop pagging\n"), stdout);

						int l = (int)term_rows - 5;
						while (--l >= 0)
							putchar('\n');

						fputs("\x1b[7;97m--Mas--\x1b[0;49m", stdout);

						i -= ((term_rows * columns_n) - 1);

						if (i < 0)
							i = 0;

						counter = 0;
						xgetchar();
						CLEAR;
					}
					break;

				/* Stop paging (and set a flag to reenable the pager
				 * later) */
				case 99:  /* 'c' */
				case 112: /* 'p' */
				case 113: pager = 0, reset_pager = 1; /* 'q' */
					break;

				/* If another key is pressed, go back one position.
				 * Otherwise, some filenames won't be listed.*/
				default:
					i--;
					fputs("\r\x1b[K\x1b[3J", stdout);
					continue;
				}

				fputs("\r\x1b[K\x1b[3J", stdout);
			}

			counter++;
		}

		if (++cur_cols == columns_n) {
			cur_cols = 0;
			last_column = 1;
		}
		else
			last_column = 0;

		file_info[i].eln_n = no_eln ? -1 : DIGINUM(i + 1);

		int ind_char = 1;

		if (!classify)
			ind_char = 0;

		if (colorize) {
			ind_char = 0;
			if (icons) {
				if (xargs.icons_use_file_color == 1)
					file_info[i].icon_color = file_info[i].color;

				if (no_eln)
					printf("%s%s %s%s%s", file_info[i].icon_color,
						   file_info[i].icon, file_info[i].color,
						   file_info[i].name, df_c);
				else
					printf("%s%d%s %s%s %s%s%s", el_c, i + 1, df_c,
						   file_info[i].icon_color, file_info[i].icon,
						   file_info[i].color, file_info[i].name, df_c);
			}
			else {
				if (no_eln)
					printf("%s%s%s", file_info[i].color,
						   file_info[i].name, df_c);
				else
					printf("%s%d%s %s%s%s", el_c, i + 1, df_c,
						   file_info[i].color, file_info[i].name, df_c);
			}

			if (file_info[i].dir && classify) {
				fputs(" /", stdout);
				if (file_info[i].filesn > 0 && files_counter)
					fputs(xitoa(file_info[i].filesn), stdout);
			}
		}

		else {
			if (icons) {
				if (no_eln)
					printf("%s %s", file_info[i].icon, file_info[i].name);
				else
					printf("%s%d%s %s %s", el_c, i + 1, df_c,
						   file_info[i].icon, file_info[i].name);
			}

			else {
				if (no_eln)
					printf("%s", file_info[i].name);
				else
					printf("%s%d%s %s", el_c, i + 1, df_c, file_info[i].name);
			}

			if (classify) {
				switch(file_info[i].type) {

					case DT_DIR:
						ind_char = 0;
						fputs(" /", stdout);
						if (file_info[i].filesn > 0 && files_counter)
							fputs(xitoa(file_info[i].filesn), stdout);
						break;

					case DT_FIFO: putchar('|'); break;

					case DT_LNK: putchar('@'); break;

					case DT_SOCK: putchar('='); break;

					case DT_UNKNOWN: putchar('?'); break;

					default: ind_char = 0; break;
				}
			}
		}

		if (!last_column) {

			/* Add spaces needed to equate the longest filename length */
			int cur_len = (int)file_info[i].eln_n + 1 + (icons ? 3 : 0)
						  + (int)file_info[i].len + (ind_char ? 1: 0);
			if (classify) {
				if (file_info[i].dir)
					cur_len += 2;
				if (file_info[i].filesn > 0 && files_counter
				&& file_info[i].ruser)
					cur_len += DIGINUM((int)file_info[i].filesn);
			}

			int diff = (int)longest - cur_len;

			register int j;
			for (j = diff + 1; j--;)
				putchar(' ');
		}
		else
			putchar('\n');
	}

	if (!last_column)
		putchar('\n');

END:
	if (closedir(dir) == -1)
		return EXIT_FAILURE;

	if (xargs.list_and_quit == 1)
		exit(exit_code);

	if (reset_pager)
		pager = 1;

	/* Print a dividing line between the files list and the
	 * prompt */
	print_div_line();

	if (max_files != UNSET && (int)files > max_files)
		printf("%d/%zu\n", max_files, files);

	if (dirhist_map) {
		/* Print current, previous, and next entries */
		print_dirhist_map();

		print_div_line();
	}

	if (disk_usage)
		print_disk_usage();

	if (sort_switch)
		print_sort_method();

/*  clock_t end = clock();
	printf("list_dir time: %f\n", (double)(end-start)/CLOCKS_PER_SEC); */

	return EXIT_SUCCESS;
}

static int
list_dir(void)
/* List files in the current working directory. Uses filetype colors
 * and columns. Return zero on success or one on error */
{
/*  clock_t start = clock(); */

	if (clear_screen)
		CLEAR;

	if (light_mode)
		return list_dir_light();

	DIR *dir;

	struct dirent *ent;
	struct stat attr;

	if ((dir = opendir(ws[cur_ws].path)) == NULL) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, ws[cur_ws].path,
				strerror(errno));
		return EXIT_FAILURE;
	}

	int fd = dirfd(dir);

		/* ##########################################
		 * #    GATHER AND STORE FILE INFORMATION   #
		 * ########################################## */

	errno = 0;
	longest = 0;
	register unsigned int n = 0;
	unsigned int total_dents = 0, count = 0;

	file_info = (struct fileinfo *)xnmalloc(ENTRY_N + 2,
								   sizeof(struct fileinfo));

	while ((ent = readdir(dir))) {

		char *ename = ent->d_name;

		/* Skip self and parent directories */
		if (*ename == '.' && (!ename[1] || (ename[1] == '.'
		&& !ename[2])))
			continue;

		/* Skip files matching FILTER */
		if (filter && regexec(&regex_exp, ename, 0, NULL, 0)
		== EXIT_SUCCESS)
			continue;

		if (!show_hidden && *ename == '.')
			continue;

		if (only_dirs && ent->d_type != DT_DIR)
			continue;

		if (fstatat(fd, ename, &attr, AT_SYMLINK_NOFOLLOW) == -1)
			continue;

		if (count > ENTRY_N) {
			count = 0;
			total_dents = n + ENTRY_N;
			file_info = xrealloc(file_info, (total_dents + 2)
							* sizeof(struct fileinfo));
		}

		file_info[n].name = (char *)xnmalloc(NAME_MAX + 1, sizeof(char));

		if (!unicode) {
			file_info[n].len = (xstrsncpy(file_info[n].name, ename,
										  NAME_MAX + 1) - 1);
		}
		else {
			xstrsncpy(file_info[n].name, ename, NAME_MAX + 1);
			file_info[n].len = wc_xstrlen(ename);
		}

		file_info[n].dir = (ent->d_type == DT_DIR) ? 1 : 0;
		file_info[n].symlink = (ent->d_type == DT_LNK) ? 1 : 0;
		file_info[n].exec = 0;
		file_info[n].type = ent->d_type;
		file_info[n].inode = ent->d_ino;
		file_info[n].linkn = attr.st_nlink;
		file_info[n].size = attr.st_size;

		if (long_view) {
			file_info[n].uid = attr.st_uid;
			file_info[n].gid = attr.st_gid;
			file_info[n].ltime = (time_t)attr.st_mtim.tv_sec;
			file_info[n].mode = attr.st_mode;
		}
		else if (sort == SOWN || sort == SGRP) {
			file_info[n].uid = attr.st_uid;
			file_info[n].gid = attr.st_gid;
		}

		file_info[n].color = (char *)NULL;

		file_info[n].icon = DEF_FILE_ICON;
		file_info[n].icon_color = DEF_FILE_ICON_COLOR;

		file_info[n].ruser = 1;
		file_info[n].filesn = 0;

		switch(sort) {
			case SATIME: file_info[n].time = (time_t)attr.st_atime; break;
#if defined(HAVE_ST_BIRTHTIME) || defined(__BSD_VISIBLE)
			case SBTIME: file_info[n].time = (time_t)attr.st_birthtime; break;
#elif defined(_STATX)
			case SBTIME: {
				struct statx attx;
				if (statx(AT_FDCWD, ename, AT_SYMLINK_NOFOLLOW,
				STATX_BTIME, &attx) == -1)
					file_info[n].time = 0;
				else
					file_info[n].time = (time_t)attx.stx_btime.tv_sec;
				}
				break;
#else
			case SBTIME: file_info[n].time = (time_t)attr.st_ctime; break;
#endif
			case SCTIME: file_info[n].time = (time_t)attr.st_ctime; break;
			case SMTIME: file_info[n].time = (time_t)attr.st_mtime; break;
			default: file_info[n].time = 0; break;
		}

		switch(file_info[n].type) {

			case DT_DIR:
				if (icons) {
					get_dir_icon(file_info[n].name, (int)n);

					/* If set from the color scheme file */
					if (*dir_ico_c)
						file_info[n].icon_color = dir_ico_c;
				}
				files_counter
				? (file_info[n].filesn = (count_dir(ename) - 2))
				: (file_info[n].filesn = 1);
				if (file_info[n].filesn > 0) {        /* S_ISVTX*/
					file_info[n].color = (attr.st_mode & 01000)
					? ((attr.st_mode & 00002) ?  tw_c : st_c)
					: ((attr.st_mode & 00002) ? ow_c : di_c);
								   /* S_ISWOTH*/
				}
				else if (file_info[n].filesn == 0)
					file_info[n].color = (attr.st_mode & 01000)
					? ((attr.st_mode & 00002) ?  tw_c : st_c)
					: ((attr.st_mode & 00002) ? ow_c : ed_c);
				else {
					file_info[n].color = nd_c;
					file_info[n].icon = ICON_LOCK;
					file_info[n].icon_color = YELLOW;
				}

				break;

			case DT_LNK:
				file_info[n].icon = ICON_LINK;
				struct stat attrl;
				if (fstatat(fd, ename, &attrl, 0) == -1)
					file_info[n].color = or_c;
				else {
					if (S_ISDIR(attrl.st_mode)) {
						file_info[n].dir = 1;
						files_counter
						? (file_info[n].filesn = (count_dir(ename) - 2))
						: (file_info[n].filesn = 0);
					}
					file_info[n].color = ln_c;
				}
				break;

			case DT_REG: {
#ifdef _LINUX_CAP
				cap_t cap;
#endif
				/* Do not perform the access check if the user is root */
				if (!(flags & ROOT_USR)
				&& access(file_info[n].name, F_OK|R_OK) == -1) {
					file_info[n].color = nf_c;
					file_info[n].icon = ICON_LOCK;
					file_info[n].icon_color = YELLOW;
				}
				else if (attr.st_mode & 04000) { /* SUID */
					file_info[n].exec = 1;
					file_info[n].color = su_c;
					file_info[n].icon = ICON_EXEC;
				}
				else if (attr.st_mode & 02000) { /* SGID */
					file_info[n].exec = 1;
					file_info[n].color = sg_c;
					file_info[n].icon = ICON_EXEC;
				}

#ifdef _LINUX_CAP
				else if ((cap = cap_get_file(ename))) {
					file_info[n].color = ca_c;
					cap_free(cap);
				}
#endif

				else if ((attr.st_mode & 00100) /* Exec */
				|| (attr.st_mode & 00010) || (attr.st_mode & 00001)) {
					file_info[n].exec = 1;
					file_info[n].icon = ICON_EXEC;
					if (file_info[n].size == 0)
						file_info[n].color = ee_c;
					else
						file_info[n].color = ex_c;
				}

				else if (file_info[n].size == 0)
					file_info[n].color = ef_c;

				else if (file_info[n].linkn > 1) /* Multi-hardlink */
					file_info[n].color = mh_c;

				/* Check extension color only if some is defined */
				else if (ext_colors_n) {

					char *ext = strrchr(file_info[n].name, '.');
					/* Make sure not to take a hidden file for a file
					 * extension */

					if (ext && ext != file_info[n].name) {
						if (icons)
							get_ext_icon(ext, (int)n);

						char *extcolor = get_ext_color(ext);

						if (extcolor) {
							char ext_color[MAX_COLOR] = "";
							sprintf(ext_color, "\x1b[%sm", extcolor);
							file_info[n].color = ext_color;
							extcolor = (char *)NULL;
						}
						else /* No matching extension found */
							file_info[n].color = fi_c;
					}
					else { /* Bare regular file */
						file_info[n].color = fi_c;
						if (icons)
							get_file_icon(file_info[n].name, (int)n);
					}
				}
				else
					file_info[n].color = fi_c;
				}
				break;

			case DT_SOCK: file_info[n].color = so_c; break;

			case DT_FIFO: file_info[n].color = pi_c; break;

			case DT_BLK: file_info[n].color = bd_c; break;

			case DT_CHR: file_info[n].color = cd_c; break;

			case DT_UNKNOWN: file_info[n].color = uf_c; break;

			default: file_info[n].color = df_c; break;
		}

		if (xargs.icons_use_file_color == 1 && icons)
			file_info[n].icon_color = file_info[n].color;

		n++;
		count++;
	}

	file_info[n].name = (char *)NULL;

	files = n;

	if (n == 0) {
		printf("%s. ..%s\n", colorize ? di_c : df_c, df_c);
		free(file_info);
		if (closedir(dir) == -1)
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

		/* #############################################
		 * #    SORT FILES ACCORDING TO SORT METHOD    #
		 * ############################################# */

	if (sort)
		qsort(file_info, n, sizeof(*file_info), entrycmp);

		/* ##########################################
		 * #    GET INFO TO PRINT COLUMNED OUTPUT   #
		 * ########################################## */

	/* Get terminal current amount of rows and columns */
	struct winsize w;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
	/* ws_col and ws_row are both unsigned short int according to
	 * /bits/ioctl-types.h */
	term_cols = w.ws_col; /* This one is global */
	unsigned short term_rows = w.ws_row;

	int reset_pager = 0;
	int c, i;
	register size_t counter = 0;

	size_t columns_n = 1;

	/* Get the longest filename */
	if (columned || long_view) {
		i = (int)n;
		while (--i >= 0) {
			size_t total_len = 0;
			file_info[i].eln_n = no_eln ? -1 : DIGINUM(i + 1);
			total_len = file_info[i].eln_n + 1 + file_info[i].len;

			if (!long_view && classify) {

				if (file_info[i].dir)
					total_len += 2;

				if (file_info[i].filesn > 0 && files_counter)
					total_len += DIGINUM(file_info[i].filesn);

				if (!file_info[i].dir && !colorize) {
					switch(file_info[i].type) {
						case DT_REG:
							if (file_info[i].exec)
								total_len += 1;
							break;
						case DT_LNK: /* fallthrough */
						case DT_SOCK: /* fallthrough */
						case DT_FIFO: /* fallthrough */
						case DT_UNKNOWN:
							total_len += 1;
							break;
					}
				}
			}

			if (total_len > longest)
				longest = total_len;
		}

		if (icons && !long_view && columned)
			longest += 3;
	}

				/* ########################
				 * #    LONG VIEW MODE    #
				 * ######################## */

	if (long_view) {
		int space_left = term_cols - MAX_PROP_STR;
		/* SPACE_LEFT is the max space that should be used to print the
		 * filename (plus space char) */

		/* Do not allow SPACE_LEFT to be less than MIN_NAME_TRIM,
		 * especially because the result of the above operation could
		 * be negative */
		if (space_left < min_name_trim)
			space_left = min_name_trim;

		if ((int)longest < space_left)
			space_left = longest;

		int k = (int)files;
		for (i = 0; i < k; i++) {

			if (max_files != UNSET && i == max_files)
				break;

			if (pager) {

				if (counter > (size_t)(term_rows - 2)) {

					fputs("\x1b[7;97m--Mas--\x1b[0;49m", stdout);

					switch (c = xgetchar()) {

					/* Advance one line at a time */
					case 66: /* fallthrough */ /* Down arrow */
					case 10: /* fallthrough */ /* Enter */
					case 32: /* Space */
						break;

					/* Advance one page at a time */
					case 126: counter = 0; /* Page Down */
						break;

					case 63: /* fallthrough */ /* ? */
					case 104: { /* h: Print pager help */
						CLEAR;

						fputs(_("?, h: help\n"
						"Down arrow, Enter, Space: Advance one line\n"
						"Page Down: Advance one page\n"
						"q: Stop pagging\n"), stdout);

						int l = (int)term_rows - 5;
						while (--l >= 0)
							putchar('\n');

						fputs("\x1b[7;97m--Mas--\x1b[0;49m", stdout);

						i -= (term_rows - 1);

						if (i < 0) i = 0;

						counter = 0;
						xgetchar();
						CLEAR;
					}
					break;

					/* Stop paging (and set a flag to reenable the pager
					 * later) */
					case 99:  /* 'c' */
					case 112: /* 'p' */
					case 113: pager = 0, reset_pager = 1; /* 'q' */
						break;

					/* If another key is pressed, go back one position.
					 * Otherwise, some filenames won't be listed.*/
					default:
						i--;
						fputs("\r\x1b[K\x1b[3J", stdout);
						continue;
					}

					fputs("\r\x1b[K\x1b[3J", stdout);
				}

				counter++;
			}

			/* Print ELN. The remaining part of the line will be
			 * printed by print_entry_props() */
			if (!no_eln)
				printf("%s%d%s ", el_c, i + 1, df_c);

			print_entry_props(&file_info[i], (size_t)space_left);
		}

		goto END;
	}

				/* ########################
				 * #   NORMAL VIEW MODE   #
				 * ######################## */

	int last_column = 0;

	/* Get amount of columns needed to print files in CWD  */
	if (!columned)
		columns_n = 1;

	else {
		columns_n = (size_t)term_cols / (longest + 1); /* +1 for the
		space between file names */

		/* If longest is bigger than terminal columns, columns_n will
		 * be negative or zero. To avoid this: */
		if (columns_n < 1)
			columns_n = 1;

		/* If we have only three files, we don't want four columns */
		if (columns_n > (size_t)n)
			columns_n = (size_t)n;
	}

	int nn = (int)n;
	size_t cur_cols = 0;
	for (i = 0; i < nn; i++) {

		if (max_files != UNSET && i == max_files)
			break;

				/* ######################
				 * #   A SIMPLE PAGER   #
				 * ###################### */

		/* A basic pager for directories containing large amount of
		 * files. What's missing? It only goes downwards. To go
		 * backwards, use the terminal scrollback function */
		if (pager) {
			/* Run the pager only once all columns and rows fitting in
			 * the screen are filled with the corresponding filenames */
			if (last_column
			&& counter > columns_n * ((size_t)term_rows - 2)) {

				fputs("\x1b[7;97m--Mas--\x1b[0;49m", stdout);

				switch (c = xgetchar()) {

				/* Advance one line at a time */
				case 66: /* fallthrough */ /* Down arrow */
				case 10: /* fallthrough */ /* Enter */
				case 32: /* Space */
					break;

				/* Advance one page at a time */
				case 126: counter = 0; /* Page Down */
					break;

				case 63: /* fallthrough */ /* ? */
				case 104: { /* h: Print pager help */
						CLEAR;

						fputs(_("?, h: help\n"
						"Down arrow, Enter, Space: Advance one line\n"
						"Page Down: Advance one page\n"
						"q: Stop pagging\n"), stdout);

						int l = (int)term_rows - 5;
						while (--l >= 0)
							putchar('\n');

						fputs("\x1b[7;97m--Mas--\x1b[0;49m", stdout);

						i -= ((term_rows * columns_n) - 1);

						if (i < 0) i = 0;

						counter = 0;
						xgetchar();
						CLEAR;
					}
					break;

				/* Stop paging (and set a flag to reenable the pager
				 * later) */
				case 99:  /* fallthrough */ /* 'c' */
				case 112: /* fallthrough */ /* 'p' */
				case 113: pager = 0, reset_pager = 1; /* 'q' */
					break;

				/* If another key is pressed, go back one position.
				 * Otherwise, some filenames won't be listed.*/
				default:
					i--;
					fputs("\r\x1b[K\x1b[3J", stdout);
					continue;
				}

				fputs("\r\x1b[K\x1b[3J", stdout);
			}

			counter++;
		}

		/* Determine if current entry is in the last column, in which
		 * case a new line char will be appended */
		if (++cur_cols == columns_n) {
			cur_cols = 0;
			last_column = 1;
		}
		else
			last_column = 0;

		file_info[i].eln_n = no_eln ? -1 : DIGINUM(i + 1);

		int ind_char = 1;

		if (!classify)
			ind_char = 0;

			/* #################################
			 * #    PRINT THE CURRENT ENTRY    #
			 * ################################# */

		if (colorize) {

			ind_char = 0;
			if (icons) {
				if (no_eln)
					printf("%s%s %s%s%s", file_info[i].icon_color,
						   file_info[i].icon, file_info[i].color,
						   file_info[i].name, df_c);
				else
					printf("%s%d%s %s%s %s%s%s", el_c, i + 1, df_c,
						   file_info[i].icon_color, file_info[i].icon,
						   file_info[i].color, file_info[i].name, df_c);
			}

			else {
				if (no_eln)
					printf("%s%s%s", file_info[i].color,
						   file_info[i].name, df_c);
				else
					printf("%s%d%s %s%s%s", el_c, i + 1, df_c,
						   file_info[i].color, file_info[i].name, df_c);
			}

			if (classify) {
				/* Append directory indicator and files counter */
				switch(file_info[i].type) {

					case DT_DIR:
						fputs(" /", stdout);
						if (file_info[i].filesn > 0 && files_counter)
							fputs(xitoa(file_info[i].filesn), stdout);
						break;

					case DT_LNK:
						if (file_info[i].dir)
							fputs(" /", stdout);
						if (file_info[i].filesn > 0 && files_counter)
							fputs(xitoa(file_info[i].filesn), stdout);
						break;
				}
			}
		}

		/* No color */
		else {
			if (icons) {
				if (no_eln)
					printf("%s %s", file_info[i].icon, file_info[i].name);
				else
					printf("%s%d%s %s %s", el_c, i + 1, df_c,
						   file_info[i].icon, file_info[i].name);
			}

			else {
				if (no_eln)
					fputs(file_info[i].name, stdout);
				else {
					printf("%s%d%s %s", el_c, i + 1, df_c, file_info[i].name);
/*                  fputs(el_c, stdout);
					fputs(xitoa(i + 1), stdout);
					fputs(df_c, stdout);
					putchar(' ');
					fputs(file_info[i].name, stdout); */
				}
			}

			if (classify) {
				/* Append filetype indicator */
				switch(file_info[i].type) {

					case DT_DIR:
						ind_char = 0;
						fputs(" /", stdout);
						if (file_info[i].filesn > 0 && files_counter)
							fputs(xitoa(file_info[i].filesn), stdout);
						break;

					case DT_FIFO: putchar('|'); break;

					case DT_LNK:
						if (file_info[i].dir) {
							ind_char = 0;
							fputs(" /", stdout);
							if (file_info[i].filesn > 0 && files_counter)
								fputs(xitoa(file_info[i].filesn), stdout);
						}
						else
							putchar('@');

						break;

					case DT_REG:
						if (file_info[i].exec)
							putchar('*');
						else
							ind_char = 0;
						break;

					case DT_SOCK: putchar('='); break;

					case DT_UNKNOWN: putchar('?'); break;

					default: ind_char = 0;
				}
			}
		}

		if (!last_column) {

			/* Add spaces needed to equate the longest filename length */
			int cur_len = (int)file_info[i].eln_n + 1 + (icons ? 3 : 0)
						  + (int)file_info[i].len + (ind_char ? 1 : 0);
			if (file_info[i].dir && classify) {
				cur_len += 2;
				if (file_info[i].filesn > 0 && files_counter
				&& file_info[i].ruser)
					cur_len += DIGINUM((int)file_info[i].filesn);
			}

			int diff = (int)longest - cur_len;

			register int j;
			for (j = diff + 1; j--;)
				putchar(' ');
		}
		else
			putchar('\n');
	}

	if (!last_column)
		putchar('\n');

				/* #########################
				 * #   POST LISTING STUFF  #
				 * ######################### */

END:
	if (closedir(dir) == -1)
		return EXIT_FAILURE;

	if (xargs.list_and_quit == 1)
		exit(exit_code);

	if (reset_pager)
		pager = 1;

	/* Print a dividing line between the files list and the
	 * prompt */
	print_div_line();

	if (max_files != UNSET && (int)files > max_files)
		printf("%d/%zu\n", max_files, files);

	if (dirhist_map) {
		/* Print current, previous, and next entries */
		print_dirhist_map();

		print_div_line();
	}

	if (disk_usage)
		print_disk_usage();

	if (sort_switch)
		print_sort_method();

/*  clock_t end = clock();
	printf("list_dir time: %f\n", (double)(end-start)/CLOCKS_PER_SEC); */

	return EXIT_SUCCESS;
}

static int
run_and_refresh(char **comm)
/* Run a command via execle() and refresh the screen in case of success */
{
	if (!comm)
		return EXIT_FAILURE;

	log_function(comm);

	size_t i = 0, total_len = 0;

	for (i = 0; i <= args_n; i++)
		total_len += strlen(comm[i]);

	char *tmp_cmd = (char *)NULL;
	tmp_cmd = (char *)xcalloc(total_len + (i + 1) + 1, sizeof(char));

	for (i = 0; i <= args_n; i++) {
		strcat(tmp_cmd, comm[i]);
		strcat(tmp_cmd, " ");
	}

	int ret = launch_execle(tmp_cmd);
	free(tmp_cmd);

	if (ret != EXIT_SUCCESS)
		return EXIT_FAILURE;
		/* Error messages will be printed by launch_execve() itself */

	/* If 'rm sel' and command is successful, deselect everything */
	if (is_sel && *comm[0] == 'r' && comm[0][1] ==  'm'
	&& (!comm[0][2] || comm[0][2] == ' ')) {

		int j = (int)sel_n;
		while (--j >= 0)
			free(sel_elements[j]);

		sel_n = 0;
		save_sel();
	}

	/* When should the screen actually be refreshed:
	* 1) Creation or removal of file (in current files list)
	* 2) The contents of a directory (in current files list) changed */
	if (cd_lists_on_the_fly && strcmp(comm[1], "--help") != 0
	&& strcmp(comm[1], "--version") != 0) {
		free_dirlist();
		list_dir();
	}

	return EXIT_SUCCESS;
}

static int
copy_function(char **comm)
{
	log_function(comm);

	if (!is_sel)
		return run_and_refresh(comm);

	char *tmp_cmd = (char *)NULL;
	size_t total_len = 0, i = 0;

	for (i = 0; i <= args_n; i++)
		total_len += strlen(comm[i]);

	tmp_cmd = (char *)xcalloc(total_len + (i + 1) + 2, sizeof(char));

	for (i = 0; i <= args_n; i++) {
		strcat(tmp_cmd, comm[i]);
		strcat(tmp_cmd, " ");
	}

	if (sel_is_last)
		strcat(tmp_cmd, ".");

	int ret = 0;
	ret = launch_execle(tmp_cmd);
	free(tmp_cmd);

	if (ret != EXIT_SUCCESS)
		return EXIT_FAILURE;

	if (copy_n_rename) { /* pp */
		char **tmp = (char **)xnmalloc(sel_n + 3, sizeof(char *));
		tmp[0] = savestring("br", 2);

		size_t j;
		for (j = 0; j < sel_n; j++) {
			size_t arg_len = strlen(sel_elements[j]);

			if (sel_elements[j][arg_len - 1] == '/')
				sel_elements[j][arg_len - 1] = '\0';

			if (*comm[args_n] == '~') {
				char *exp_dest = tilde_expand(comm[args_n]);
				comm[args_n] = xrealloc(comm[args_n],
						(strlen(exp_dest) + 1) * sizeof(char));
				strcpy(comm[args_n], exp_dest);
				free(exp_dest);
			}

			size_t dest_len = strlen(comm[args_n]);
			if (comm[args_n][dest_len - 1] == '/') {
				comm[args_n][dest_len - 1] = '\0';
			}

			char dest[PATH_MAX];
			strcpy(dest, (sel_is_last
				|| strcmp(comm[args_n], ".") == 0)
				? ws[cur_ws].path : comm[args_n]);

			char *ret_val = strrchr(sel_elements[j], '/');

			char *tmp_str = (char *)xnmalloc(strlen(dest)
						+ strlen(ret_val + 1) + 2, sizeof(char));

			sprintf(tmp_str, "%s/%s", dest, ret_val + 1);

			tmp[j + 1] = savestring(tmp_str, strlen(tmp_str));
			free(tmp_str);
		}

		tmp[j + 1] = (char *)NULL;

		bulk_rename(tmp);

		for (i = 0; tmp[i]; i++)
			free(tmp[i]);

		free(tmp);

		copy_n_rename = 0;

		return EXIT_SUCCESS;
	}

	/* If 'mv sel' and command is successful deselect everything,
	 * since sel files are note there anymore */
	if (*comm[0] == 'm' && comm[0][1] == 'v'
	&& (!comm[0][2] || comm[0][2] == ' ')) {

		for (i = 0; i < sel_n; i++)
			free(sel_elements[i]);

		sel_n = 0;
		save_sel();
	}

	if (cd_lists_on_the_fly) {
		free_dirlist();
		list_dir();
	}

	return EXIT_SUCCESS;
}

static int
remove_file(char **args)
{
	int cwd = 0, exit_status = EXIT_SUCCESS;

	char **rm_cmd = (char **)xnmalloc(args_n + 4, sizeof(char *));
	int i, j = 3, dirs = 0;

	for (i = 1; args[i]; i++) {

		/* Check if at least one file is in the current directory. If not,
		 * there is no need to refresh the screen */
		if (!cwd) {
			char *ret = strchr(args[i], '/');
			/* If there's no slash, or if slash is the last char and
			 * the file is not root "/", we have a file in CWD */
			if (!ret || (!*(ret + 1) && ret != args[i]))
				cwd = 1;
		}

		char *tmp = (char *)NULL;
		if (strchr(args[i], '\\')) {
			tmp = dequote_str(args[i], 0);

			if (tmp) {
				/* Start storing filenames in 3: 0 is for 'rm', and 1
				 * and 2 for parameters, including end of parameters (--) */
				rm_cmd[j++] = savestring(tmp, strlen(tmp));
				free(tmp);
			}
			else {
				fprintf(stderr, "%s: %s: Error dequoting filename\n",
						PROGRAM_NAME, args[i]);
				continue;
			}
		}

		else
			rm_cmd[j++] = savestring(args[i], strlen(args[i]));

		struct stat attr;
		if (!dirs && lstat(rm_cmd[j - 1], &attr) != -1
		&& (attr.st_mode & S_IFMT) == S_IFDIR)
			dirs = 1;
	}

	rm_cmd[j] = (char *)NULL;

	rm_cmd[0] = savestring("rm", 2);
	if (dirs)
		rm_cmd[1] = savestring("-dIr", 4);
	else
		rm_cmd[1] = savestring("-I", 2);
	rm_cmd[2] = savestring("--", 2);

	if (launch_execve(rm_cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
		exit_status = EXIT_FAILURE;
	else {
		if (cwd && cd_lists_on_the_fly && strcmp(args[1], "--help") != 0
		&& strcmp(args[1], "--version") != 0) {
			free_dirlist();
			exit_status = list_dir();
		}
	}

	for (i = 0; rm_cmd[i]; i++)
		free(rm_cmd[i]);

	free(rm_cmd);

	return exit_status;
}

static int
exec_cmd(char **comm)
/* Take the command entered by the user, already splitted into substrings
 * by parse_input_str(), and call the corresponding function. Return zero
 * in case of success and one in case of error */
{
/*  if (!comm || *comm[0] == '\0')
		return EXIT_FAILURE; */

	fputs(df_c, stdout);

	/* Exit flag. exit_code is zero (sucess) by default. In case of error
	 * in any of the functions below, it will be set to one (failure).
	 * This will be the value returned by this function. Used by the \z
	 * escape code in the prompt to print the exit status of the last
	 * executed command */
	exit_code = EXIT_SUCCESS;

	/* User defined actions */
	if (actions_n) {
		size_t i;
		for (i = 0; i < actions_n; i++) {
			if (*comm[0] == *usr_actions[i].name
			&& strcmp(comm[0], usr_actions[i].name) == 0) {
				exit_code = run_action(usr_actions[i].value, comm);
				return exit_code;
			}
		}
	}

	/* User defined variables */
	if (flags & IS_USRVAR_DEF) {
		flags &= ~IS_USRVAR_DEF;

		exit_code = create_usr_var(comm[0]);
		return exit_code;
	}

	if (comm[0][0] == ';' || comm[0][0] == ':') {
		if (!comm[0][1]) {
			/* If just ":" or ";", launch the default shell */
			char *cmd[] = { sys_shell, NULL };
			if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
				exit_code = EXIT_FAILURE;
			return exit_code;
		}

		/* If double semi colon or colon (or ";:" or ":;") */
		else if (comm[0][1] == ';' || comm[0][1] == ':') {
			fprintf(stderr, _("%s: '%s': Syntax error\n"),
					PROGRAM_NAME, comm[0]);
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}
	}

				/* ###############################
				 * #     AUTOCD & AUTOOPEN (1)   #
				 * ############################### */

	if (autocd || auto_open) {
		/* Expand tilde */
		if (*comm[0] == '~') {
			char *exp_path = tilde_expand(comm[0]);
			if (exp_path) {
				comm[0] = (char *)xrealloc(comm[0], (strlen(exp_path)
										   + 1) * sizeof(char));
				strcpy(comm[0], exp_path);
				free(exp_path);
			}
		}

		/* Deescape the string (only if filename) */
		if (strchr(comm[0], '\\')) {
			char *deq_str = dequote_str(comm[0], 0);
			if (deq_str) {
				if (access(deq_str, F_OK) == 0)
					strcpy(comm[0], deq_str);
				free(deq_str);
			}
		}
	}

	/* Only autocd or auto-open here if not absolute path and if there
	 * is no second argument or if second argument is "&" */
	if (*comm[0] != '/' && (autocd || auto_open)
	&& (!comm[1] || (*comm[1] == '&' && !comm[1][1]))) {

		char *tmp = comm[0];
		size_t i, tmp_len = strlen(tmp);

		if (tmp[tmp_len - 1] == '/')
			tmp[tmp_len - 1] = '\0';

		for (i = files; i--;) {

			if (*tmp != *file_info[i].name)
				continue;

			if (strcmp(tmp, file_info[i].name) != 0)
				continue;

			if (autocd && (file_info[i].type == DT_DIR
			|| file_info[i].dir == 1)) {
				exit_code = cd_function(tmp);
				return exit_code;
			}

			else if (auto_open && (file_info[i].type == DT_REG
			|| file_info[i].type == DT_LNK)) {
				char *cmd[] = { "open", comm[0],
								(comm[1]) ? comm[1] : NULL, NULL };
				exit_code = open_function(cmd);
				return exit_code;
			}

			else
				break;
		}
	}

	/* The more often a function is used, the more on top should it be
	 * in this if...else..if chain. It will be found faster this way. */

	/* ####################################################
	 * #                 BUILTIN COMMANDS                 #
	 * ####################################################*/


	/*          ############### CD ##################     */
	if (*comm[0] == 'c' && comm[0][1] == 'd' && !comm[0][2]) {
		if (!comm[1])
			exit_code = cd_function(NULL);

		else if (*comm[1] == '-' && strcmp(comm[1], "--help") == 0)
			puts(_("Usage: cd [ELN/DIR]"));

		/* Remotes */
		else if (*comm[1] == 's' && strncmp(comm[1], "sftp://", 7) == 0)
			exit_code = remote_ssh(comm[1] + 7, (comm[2]) ? comm[2]
								   : NULL);
		else if (*comm[1] == 's' && strncmp(comm[1], "smb://", 6) == 0)
			exit_code = remote_smb(comm[1] + 6, (comm[2]) ? comm[2]
								   : NULL);
		else if (*comm[1] == 'f' && strncmp(comm[1], "ftp://", 6) == 0)
			exit_code = remote_ftp(comm[1] + 6, (comm[2]) ? comm[2]
								   : NULL);

		else
			exit_code = cd_function(comm[1]);
	}

	/*         ############### OPEN ##################     */
	else if (*comm[0] == 'o' && (!comm[0][1]
	|| strcmp(comm[0], "open") == 0)) {

		if (!comm[1]) {
			puts(_("Usage: o, open ELN/FILE [APPLICATION]"));
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}

		else if (*comm[1] == '-' && strcmp(comm[1], "--help") == 0)
			puts(_("Usage: o, open ELN/FILE [APPLICATION]"));

		/* Remotes */
		else if (*comm[1] == 's' && strncmp(comm[1], "sftp://", 7) == 0)
			exit_code = remote_ssh(comm[1] + 7, (comm[2]) ? comm[2]
								   : NULL);
		else if (*comm[1] == 's' && strncmp(comm[1], "smb://", 6) == 0)
			exit_code = remote_smb(comm[1] + 6, (comm[2]) ? comm[2]
								   : NULL);
		else if (*comm[1] == 'f' && strncmp(comm[1], "ftp://", 6) == 0)
			exit_code = remote_ftp(comm[1] + 6, (comm[2]) ? comm[2]
								   : NULL);

		else
			exit_code = open_function(comm);
	}


	/*   ############## DIRECTORY JUMPER ##################     */
	else if (*comm[0] == 'j' && (!comm[0][1]
	|| ((comm[0][1] == 'c' || comm[0][1] == 'p'
	|| comm[0][1] == 'e' || comm[0][1] == 'o' || comm[0][1] == 'l')
	&& !comm[0][2]))) {
		exit_code = dirjump(comm);
		return exit_code;
	}

	/*       ############### REFRESH ##################     */
	else if (*comm[0] == 'r' && ((comm[0][1] == 'f' && !comm[0][2])
	|| strcmp(comm[0], "refresh") == 0)) {

		if (cd_lists_on_the_fly) {
			free_dirlist();
			exit_code = list_dir();
		}
	}

	/*         ############### BOOKMARKS ##################     */
	else if (*comm[0] == 'b' && ((comm[0][1] == 'm' && !comm[0][2])
	|| strcmp(comm[0], "bookmarks") == 0)) {
		if (comm[1] && strcmp(comm[1], "--help") == 0) {
			puts(_("Usage: bm, bookmarks [a, add FILE] [d, del] "
				 "[edit]"));
			return EXIT_SUCCESS;
		}
		/* Disable keyboard shortcuts. Otherwise, the function will
		 * still be waiting for input while the screen have been taken
		 * by another function */
		kbind_busy = 1;
		/* Disable TAB completion while in Bookmarks */
		rl_attempted_completion_function = NULL;
		exit_code = bookmarks_function(comm);
		/* Reenable TAB completion */
		rl_attempted_completion_function = my_rl_completion;
		/* Reenable keyboard shortcuts */
		kbind_busy = 0;
	}

	/*       ############### BACK AND FORTH ##################     */
	else if (*comm[0] == 'b' && (!comm[0][1]
	|| strcmp(comm[0], "back") == 0))
		exit_code = back_function (comm);

	else if (*comm[0] == 'f' && (!comm[0][1]
	|| strcmp(comm[0], "forth") == 0))
		exit_code = forth_function (comm);

	else if ((*comm[0] == 'b' && comm[0][1] == 'h' && !comm[0][2])
	|| (*comm[0] == 'f' && comm[0][1] == 'h' && !comm[0][2])) {
		int i;
		for (i = 0; i < dirhist_total_index; i++) {
			if (i == dirhist_cur_index)
				printf("%d %s%s%s\n", i + 1, dh_c,
						old_pwd[i], df_c);
			else 
				printf("%d %s\n", i + 1, old_pwd[i]);
		}
	}

	/*     ############### COPY AND MOVE ##################     */
	else if ((*comm[0] == 'c' && (!comm[0][1]
	|| (comm[0][1] == 'p' && !comm[0][2])))

	|| (*comm[0] == 'm' && (!comm[0][1]
	|| (comm[0][1] == 'v' && !comm[0][2])))

	|| (*comm[0] == 'v' && (!comm[0][1] || (comm[0][1] == 'v'
	&& !comm[0][2])))

	|| (*comm[0] == 'p' && strcmp(comm[0], "paste") == 0)) {

		if (((*comm[0] == 'c' || *comm[0] == 'v') && !comm[0][1])
		|| (*comm[0] == 'v' && comm[0][1] == 'v' && !comm[0][2])
		|| strcmp(comm[0], "paste") == 0) {

			if (*comm[0] == 'v' && comm[0][1] == 'v' && !comm[0][2])
				copy_n_rename = 1;

			comm[0] = (char *)xrealloc(comm[0], 12 * sizeof(char *));
			if (cp_cmd == CP_CP)
				strcpy(comm[0], "cp -iRp");
			else if (cp_cmd == CP_ADVCP)
				strcpy(comm[0], "advcp -giRp");
			else
				strcpy(comm[0], "wcp");
		}

		else if (*comm[0] == 'm' && !comm[0][1]) {
			comm[0] = (char *)xrealloc(comm[0], 10 * sizeof(char *));
			if (mv_cmd == MV_MV)
				strcpy(comm[0], "mv -i");
			else
				strcpy(comm[0], "advmv -gi");
		}

		kbind_busy = 1;
		exit_code = copy_function(comm);
		kbind_busy = 0;
	}

	/*         ############### TRASH ##################     */
	else if (*comm[0] == 't' && (!comm[0][1]
	|| strcmp(comm[0], "tr") == 0 || strcmp(comm[0], "trash") == 0)) {
		if (comm[1] && strcmp(comm[1], "--help") == 0) {
			puts(_("Usage: t, tr, trash [ELN/FILE ... n] "
				 "[ls, list] [clear] [del, rm]"));
			return EXIT_SUCCESS;
		}

		exit_code = trash_function(comm);

		if (is_sel) { /* If 'tr sel', deselect everything */
			int i = (int)sel_n;

			while (--i >= 0)
				free(sel_elements[i]);

			sel_n = 0;

			if (save_sel() != 0)
				exit_code = EXIT_FAILURE;
		}
	}

	else if (*comm[0] == 'u' && (!comm[0][1]
	|| strcmp(comm[0], "undel") == 0
	|| strcmp(comm[0], "untrash") == 0)) {
		if (comm[1] && strcmp(comm[1], "--help") == 0) {
			puts(_("Usage: u, undel, untrash [*, a, all]"));
			return EXIT_SUCCESS;
		}

		kbind_busy = 1;
		rl_attempted_completion_function = NULL;
		exit_code = untrash_function(comm);
		rl_attempted_completion_function = my_rl_completion;
		kbind_busy = 0;
	}

	/*         ############### SELECTION ##################     */
	else if (*comm[0] == 's' && (!comm[0][1] || strcmp(comm[0], "sel") == 0))
		exit_code = sel_function(comm);

	else if (*comm[0] == 's' && (strcmp(comm[0], "sb") == 0
	|| strcmp(comm[0], "selbox") == 0))
		show_sel_files();

	else if (*comm[0] == 'd' && (strcmp(comm[0], "ds") == 0
	|| strcmp(comm[0], "desel") == 0)) {
		if (comm[1] && strcmp(comm[1], "--help") == 0) {
			puts(_("Usage: desel, ds [*, a, all]"));
			return EXIT_SUCCESS;
		}

		kbind_busy = 1;
		rl_attempted_completion_function = NULL;
		exit_code = deselect(comm);
		rl_attempted_completion_function = my_rl_completion;
		kbind_busy = 0;
	}

	/*  ############### SOME SHELL CMD WRAPPERS ##################  */
	else if ((*comm[0] == 'r' || *comm[0] == 'm' || *comm[0] == 't'
	|| *comm[0] == 'l' || *comm[0] == 'c' || *comm[0] == 'u')
	&& (strcmp(comm[0], "rm") == 0 || strcmp(comm[0], "mkdir") == 0
	|| strcmp(comm[0], "touch") == 0 || strcmp(comm[0], "ln") == 0
	|| strcmp(comm[0], "chmod") == 0 || strcmp(comm[0], "unlink") == 0
	|| strcmp(comm[0], "r") == 0 || strcmp(comm[0], "l") == 0
	|| strcmp(comm[0], "md") == 0 || strcmp(comm[0], "le") == 0)) {

		if (*comm[0] == 'l' && !comm[0][1]) {
			comm[0] = (char *)xrealloc(comm[0], 7 * sizeof(char *));
			strcpy(comm[0], "ln -sn");
		}

		else if (*comm[0] == 'r' && !comm[0][1]) {
			exit_code = remove_file(comm);
			return exit_code;
		}

		else if (*comm[0] == 'm' && comm[0][1] == 'd' && !comm[0][2]) {
			comm[0] = (char *)xrealloc(comm[0], 9 * sizeof(char *));
			strcpy(comm[0], "mkdir -p");
		}

		if (*comm[0] == 'l' && comm[0][1] == 'e' && !comm[0][2]) {
			if (!comm[1]) {
				fputs(_("Usage: le SYMLINK\n"), stderr);
				exit_code = EXIT_FAILURE;
				return EXIT_FAILURE;
			}

			exit_code = edit_link(comm[1]);
			return exit_code;
		}

		else if (*comm[0] == 'l' && comm[0][1] == 'n' && !comm[0][2]) {
			if (comm[1] && (strcmp(comm[1], "edit") == 0
			|| strcmp(comm[1], "e") == 0)) {
				if (!comm[2]) {
					fputs(_("Usage: ln edit SYMLINK\n"), stderr);
					exit_code = EXIT_FAILURE;
					return EXIT_FAILURE;
				}
				exit_code = edit_link(comm[2]);
				return exit_code;
			}
		}

		kbind_busy = 1;
		exit_code = run_and_refresh(comm);
		kbind_busy = 0;
	}

	/*    ############### TOGGLE EXEC ##################     */
	else if (*comm[0] == 't' && comm[0][1] == 'e' && !comm[0][2]) {
		if (!comm[1] || (*comm[1] == '-' && strcmp(comm[1], "--help") == 0)) {
			puts(_("Usage: te FILE(s)"));
			return EXIT_SUCCESS;
		}

		size_t j;
		for (j = 1; comm[j]; j++) {
			struct stat attr;

			if (strchr(comm[j], '\\')) {
				char *tmp = dequote_str(comm[j], 0);
				if (tmp) {
					strcpy(comm[j], tmp);
					free(tmp);
				}
			}

			if (lstat(comm[j], &attr) == -1) {
				fprintf(stderr, "stat: %s: %s\n", comm[j], strerror(errno));
				exit_code = EXIT_FAILURE;
				continue;
			}

			if (xchmod(comm[j], attr.st_mode) == -1)
				exit_code = EXIT_FAILURE;
		}

		if (exit_code == EXIT_SUCCESS)
			printf(_("%s: Toggled executable bit on %zu file(s)\n"),
				   PROGRAM_NAME, args_n);

		return exit_code;
	}

	/*    ############### PINNED FILE ##################     */
	else if (*comm[0] == 'p' && strcmp(comm[0], "pin") == 0) {

		if (comm[1]) {
			if (*comm[1] == '-' && strcmp(comm[1], "--help") == 0)
				puts("Usage: pin FILE/DIR");
			else
				exit_code = pin_directory(comm[1]);
		}

		else {
			if (pinned_dir)
				printf(_("pinned file: %s\n"), pinned_dir);
			else
				puts(_("No pinned file"));
		}
	}

	else if (*comm[0] == 'u' && strcmp(comm[0], "unpin") == 0)
		return (exit_code = unpin_dir());

	/*    ############### PROPERTIES ##################     */
	else if (*comm[0] == 'p' && (!comm[0][1]
	|| strcmp(comm[0], "pr") == 0 || strcmp(comm[0], "pp") == 0
	|| strcmp(comm[0], "prop") == 0)) {
		if (!comm[1]) {
			fputs(_("Usage: p, pr, pp, prop [ELN/FILE ... n]\n"),
				  stderr);
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}

		else if (*comm[1] == '-' && strcmp(comm[1], "--help") == 0) {
			puts(_("Usage: p, pr, pp, prop [ELN/FILE ... n]"));
			return EXIT_SUCCESS;
		}

		exit_code = properties_function(comm);
	}

	/*     ############### SEARCH ##################     */
	else if (*comm[0] == '/' && access(comm[0], F_OK) != 0) {
								/* If not absolute path */
		/* Try first globbing, and if no result, try regex */
		if (search_glob(comm, (comm[0][1] == '!') ? 1 : 0)
		== EXIT_FAILURE)
			exit_code = search_regex(comm, (comm[0][1] == '!') ? 1 : 0);
		else
			exit_code = EXIT_SUCCESS;
	}

	/*      ############### HISTORY ##################     */
	else if (*comm[0] == '!' && comm[0][1] != ' '
	&& comm[0][1] != '\t' && comm[0][1] != '\n' && comm[0][1] != '='
	&& comm[0][1] != '(')
		exit_code = run_history_cmd(comm[0] + 1);

	/*    ############### BATCH LINK ##################     */
	else if (*comm[0] == 'b' && comm[0][1] == 'l' && !comm[0][2]) {
		exit_code = batch_link(comm);
		return exit_code;
	}

	/*    ############### BULK RENAME ##################     */
	else if (*comm[0] == 'b' && ((comm[0][1] == 'r' && !comm[0][2])
	|| strcmp(comm[0], "bulk") == 0)) {

		if (!comm[1]) {
			fputs(_("Usage: br, bulk ELN/FILE ...\n"), stderr);
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}

		if (strcmp(comm[1], "--help") == 0) {
			puts(_("Usage: br, bulk ELN/FILE ..."));
			return EXIT_SUCCESS;
		}

		exit_code = bulk_rename(comm);
	}

	/*      ################ SORT ##################     */
	else if (*comm[0] == 's' && ((comm[0][1] == 't' && !comm[0][2])
	|| strcmp(comm[0], "sort") == 0)) {
		if (comm[1] && strcmp(comm[1], "--help") == 0) {
			puts(_("Usage: st [METHOD] [rev]\nMETHOD: 0 = none, "
				   "1 = name, 2 = size, 3 = atime, 4 = btime, "
				   "5 = ctime, 6 = mtime, 7 = version, "
				   "8 = extension, 9 = inode, 10 = owner, 11 = group"));
			return EXIT_SUCCESS;
		}

		exit_code = sort_function(comm);
	}

	/*   ################ ARCHIVER ##################     */
	else if (*comm[0] == 'a' && ((comm[0][1] == 'c'
	|| comm[0][1] == 'd') && !comm[0][2])) {

		if (!comm[1] || (*comm[1] == '-'
		&& strcmp(comm[1], "--help") == 0)) {
			puts(_("Usage: ac, ad ELN/FILE ..."));
			return EXIT_SUCCESS;
		}

		if (comm[0][1] == 'c')
			exit_code = archiver(comm, 'c');
		else
			exit_code = archiver(comm, 'd');

		return exit_code;
	}

	/* ##################################################
	 * #                 MINOR FUNCTIONS                #
	 * ##################################################*/

	else if (*comm[0] == 'w' && comm[0][1] == 's' && !comm[0][2]) {
		exit_code = workspaces(comm[1] ? comm[1] : NULL);
		return exit_code;
	}

	else if(*comm[0] == 'f' && ((comm[0][1] == 't' && !comm[0][2])
	|| strcmp(comm[0], "filter") == 0)) {
		exit_code = filter_function(comm[1]);
		return exit_code;
	}

	else if (*comm[0] == 'c' && ((comm[0][1] == 'l' && !comm[0][2])
	|| strcmp(comm[0], "columns") == 0)) {
		if (!comm[1] || (*comm[1] == '-'
		&& strcmp(comm[1], "--help") == 0))
			puts(_("Usage: cl, columns [on, off]"));

		else if (*comm[1] == 'o' && comm[1][1] == 'n' && !comm[1][2]) {
			columned = 1;

			if (cd_lists_on_the_fly) {
				free_dirlist();
				exit_code = list_dir();
			}
		}

		else if (*comm[1] == 'o' && strcmp(comm[1], "off") == 0) {
			columned = 0;

			if (cd_lists_on_the_fly) {
				free_dirlist();
				exit_code = list_dir();
			}
		}

		else {
			fputs(_("Usage: cl, columns [on, off]\n"), stderr);
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}
	}

	else if (*comm[0] == 'i' && strcmp(comm[0], "icons") == 0) {

		if (!comm[1] || (*comm[1] == '-'
		&& strcmp(comm[1], "--help") == 0))
			puts(_("Usage: icons [on, off]"));

		else if (*comm[1] == 'o' && comm[1][1] == 'n' && !comm[1][2]) {
			icons = 1;

			if (cd_lists_on_the_fly) {
				free_dirlist();
				exit_code = list_dir();
			}
		}

		else if (*comm[1] == 'o' && strcmp(comm[1], "off") == 0) {
			icons = 0;

			if (cd_lists_on_the_fly) {
				free_dirlist();
				exit_code = list_dir();
			}
		}

		else {
			fputs(_("Usage: icons [on, off]\n"), stderr);
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}

		return EXIT_SUCCESS;
	}

	else if (*comm[0] == 'c' && ((comm[0][1] == 's' && !comm[0][2])
	|| strcmp(comm[0], "colorschemes") == 0)) {
		exit_code = cschemes_function(comm);
		return exit_code;
	}

	else if (*comm[0] == 'k' && ((comm[0][1] == 'b' && !comm[0][2])
	|| strcmp(comm[0], "keybinds") == 0)) {
		exit_code = kbinds_function(comm);
		return exit_code;
	}

	else if (*comm[0] == 'e' && (strcmp(comm[0], "exp") == 0
	|| strcmp(comm[0], "export") == 0)) {

		if (comm[1] && *comm[1] == '-'
		&& strcmp(comm[1], "--help") == 0) {
			puts(_("Usage: exp, export [FILE(s)]"));
			return EXIT_SUCCESS;
		}

		char *ret = export(comm, 1);

		if (ret) {
			printf("Files exported to: %s\n", ret);
			free(ret);
			return EXIT_SUCCESS;
		}

		exit_code = EXIT_FAILURE;
		return EXIT_FAILURE;
	}

	else if (*comm[0] == 'o' && strcmp(comm[0], "opener") == 0) {

		if (!comm[1]) {
			printf("opener: %s\n", (opener) ? opener : "lira (built-in)");
			return EXIT_SUCCESS;
		}

		if (*comm[1] == '-' && strcmp(comm[1], "--help") == 0) {
			puts(_("Usage: opener APPLICATION"));
			return EXIT_SUCCESS;
		}

		if (opener) {
			free(opener);
			opener = (char *)NULL;
		}

		if (strcmp(comm[1], "default") != 0) {
			opener = (char *)xcalloc(strlen(comm[1]) + 1, sizeof(char));
			strcpy(opener, comm[1]);
		}

		printf(_("opener: Opener set to '%s'\n"), (opener) ? opener
			   : "lira (built-in)");
	}

					/* #### TIPS #### */
	else if (*comm[0] == 't' && strcmp(comm[0], "tips") == 0)
		print_tips(1);

					/* #### ACTIONS #### */
	else if (*comm[0] == 'a' && strcmp(comm[0], "actions") == 0) {
		if (!comm[1]) {
			if (actions_n) {
				size_t i;
				for (i = 0; i < actions_n; i++)
					printf("%s %s->%s %s\n", usr_actions[i].name,
						   mi_c, df_c, usr_actions[i].value);
			}
			else {
				puts(_("actions: No actions defined. Use the 'actions "
				"edit' command to add some"));
			}
		}

		else if (strcmp(comm[1], "edit") == 0) {
			exit_code = edit_actions();
			return exit_code;
		}

		else if (strcmp(comm[1], "--help") == 0)
			puts("Usage: actions [edit]");

		else {
			fputs("Usage: actions [edit]\n", stderr);
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}
	}

					/* #### LIGHT MODE #### */
	else if (*comm[0] == 'l' && comm[0][1] == 'm' && !comm[0][2]) {
		if (comm[1]) {
			if (*comm[1] == 'o' && strcmp(comm[1], "on") == 0) {
				light_mode = 1;
				puts(_("Light mode is on"));
			}

			else if (*comm[1] == 'o' && strcmp(comm[1], "off") == 0) {
				light_mode = 0;
				puts(_("Light mode is off"));
			}

			else {
				puts(_("Usage: lm [on, off]"));
				exit_code = EXIT_FAILURE;
			}
		}

		else {
			fputs(_("Usage: lm [on, off]\n"), stderr);
			exit_code = EXIT_FAILURE;
		}
	}

	/*    ############### RELOAD ##################     */
	else if (*comm[0] == 'r' && ((comm[0][1] == 'l' && !comm[0][2])
	|| strcmp(comm[0], "reload") == 0)) {
		exit_code = reload_config();
		welcome_message = 0;

		if (cd_lists_on_the_fly) {
			free_dirlist();
			if (list_dir() != EXIT_SUCCESS)
				exit_code = EXIT_FAILURE;
		}

		return exit_code;
	}

					/* #### NEW INSTANCE #### */
	else if ((*comm[0] == 'x' || *comm[0] == 'X') && !comm[0][1]) {
		if (comm[1]) {
			if (*comm[1] == '-' && strcmp(comm[1], "--help") == 0) {
				puts(_("Usage: x, X [DIR]"));
				return EXIT_SUCCESS;
			}

			else if (*comm[0] == 'x')
				exit_code = new_instance(comm[1], 0);
			else /* Run as root */
				exit_code = new_instance(comm[1], 1);
		}

		/* Run new instance in CWD */
		else {
			if (*comm[0] == 'x')
				exit_code = new_instance(ws[cur_ws].path, 0);
			else
				exit_code = new_instance(ws[cur_ws].path, 1);
		}

		return exit_code;
	}

						/* #### NET #### */
	else if (*comm[0] == 'n' && (!comm[0][1]
	|| strcmp(comm[0], "net") == 0)) {

		if (!comm[1]) {
			puts(_("Usage: n, net [sftp, smb, ftp]://ADDRESS [OPTIONS]"));
			return EXIT_SUCCESS;
		}

		if (*comm[1] == 's' && strncmp(comm[1], "sftp://", 7) == 0)
			exit_code = remote_ssh(comm[1] + 7, (comm[2]) ? comm[2]
								   : NULL);

		else if (*comm[1] == 's' && strncmp(comm[1], "smb://", 6) == 0)
			exit_code = remote_smb(comm[1] + 6, (comm[2]) ? comm[2]
								   : NULL);

		else if (*comm[1] == 'f' && strncmp(comm[1], "ftp://", 6) == 0)
			exit_code = remote_ftp(comm[1] + 6, (comm[2]) ? comm[2]
								   : NULL);

		else {
			fputs(_("Usage: n, net [sftp, smb, ftp]://ADDRESS [OPTIONS]\n"),
				  stderr);
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}
	}

						/* #### MIME #### */
	else if (*comm[0] == 'm' && ((comm[0][1] == 'm' && !comm[0][2])
	|| strcmp(comm[0], "mime") == 0))
		exit_code = mime_open(comm);

	else if (*comm[0] == 'l' && comm[0][1] == 's' && !comm[0][2]
	&& !cd_lists_on_the_fly) {
		free_dirlist();
		exit_code = list_dir();

		if (get_sel_files() != EXIT_SUCCESS)
			exit_code = EXIT_FAILURE;
	}

					/* #### PROFILE #### */
	else if (*comm[0] == 'p' && ((comm[0][1] == 'f' && !comm[0][2])
	|| strcmp(comm[0], "prof") == 0 || strcmp(comm[0], "profile") == 0))
		exit_code = profile_function(comm);

					/* #### MOUNTPOINTS #### */
	else if (*comm[0] == 'm' && ((comm[0][1] == 'p' && !comm[0][2])
	|| strcmp(comm[0], "mountpoints") == 0)) {

		if (comm[1] && strcmp(comm[1], "--help") == 0)
			puts(_("Usage: mp, mountpoints"));

		else {
			kbind_busy = 1;
			rl_attempted_completion_function = NULL;
			exit_code = list_mountpoints();
			rl_attempted_completion_function = my_rl_completion;
			kbind_busy = 0;
		}
	}

					/* #### MAX FILES #### */
	else if (*comm[0] == 'm' && comm[0][1] == 'f' && !comm[0][2]) {
		if (!comm[1]) {
			printf(_("Max files: %d\n"), max_files);
			return EXIT_SUCCESS;
		}

		if (*comm[1] == '-' && strcmp(comm[1], "--help") == 0) {
			puts(_("Usage: mf [NUM]"));
			return EXIT_SUCCESS;
		}

		if (strcmp(comm[1], "-1") != 0 && !is_number(comm[1])) {
			fprintf(stderr, _("%s: Usage: mf [NUM]\n"), PROGRAM_NAME);
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}

		int inum = atoi(comm[1]);
		if (inum < -1) {
			max_files = inum;
			fprintf(stderr, _("%s: %d: Invalid number\n"), PROGRAM_NAME,
					inum);
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}

		max_files = inum;
		if (max_files == -1)
			puts(_("Max files unset"));
		else
			printf(_("Max files set to %d\n"), max_files);
		return EXIT_SUCCESS;
	}

					/* #### EXT #### */
	else if (*comm[0] == 'e' && comm[0][1] == 'x' && comm[0][2] == 't'
	&& !comm[0][3]) {

		if (!comm[1]) {
			puts(_("Usage: ext [on, off, status]"));
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}

		else if (*comm[1] == '-' && strcmp(comm[1], "--help") == 0)
			puts(_("Usage: ext [on, off, status]"));

		else {
			if (*comm[1] == 's' && strcmp(comm[1], "status") == 0)
				printf(_("%s: External commands %s\n"), PROGRAM_NAME,
						(ext_cmd_ok) ? _("enabled") : _("disabled"));

			else if (*comm[1] == 'o' && strcmp(comm[1], "on") == 0) {
				ext_cmd_ok = 1;
				printf(_("%s: External commands enabled\n"), PROGRAM_NAME);
			}

			else if (*comm[1] == 'o' && strcmp(comm[1], "off") == 0) {
				ext_cmd_ok = 0;
				printf(_("%s: External commands disabled\n"), PROGRAM_NAME);
			}

			else {
				fputs(_("Usage: ext [on, off, status]\n"), stderr);
				exit_code = EXIT_FAILURE;
			}
		}
	}

					/* #### PAGER #### */
	else if (*comm[0] == 'p' && ((comm[0][1] == 'g' && !comm[0][2])
	|| strcmp(comm[0], "pager") == 0)) {

		if (!comm[1]) {
			puts(_("Usage: pager, pg [on, off, status]"));
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}

		else if (*comm[1] == '-' && strcmp(comm[1], "--help") == 0)
			puts(_("Usage: pg, pager [on, off, status]"));

		else {
			if (*comm[1] == 's' && strcmp(comm[1], "status") == 0)
				printf(_("%s: Pager %s\n"), PROGRAM_NAME,
					   (pager) ? _("enabled") : _("disabled"));

			else if (*comm[1] == 'o' && strcmp(comm[1], "on") == 0) {
				pager = 1;
				printf(_("%s: Pager enabled\n"), PROGRAM_NAME);
			}

			else if (*comm[1] == 'o' && strcmp(comm[1], "off") == 0) {
				pager = 0;
				printf(_("%s: Pager disabled\n"), PROGRAM_NAME);
			}
			else {
				fputs(_("Usage: pg, pager [on, off, status]\n"), stderr);
				exit_code = EXIT_FAILURE;
			}
		}
	}

					/* #### FILES COUNTER #### */
	else if (*comm[0] == 'f' && ((comm[0][1] == 'c' && !comm[0][2])
	|| strcmp(comm[0], "filescounter") == 0)) {
		if (!comm[1]) {
			fputs(_("Usage: fc, filescounter [on, off, status]"), stderr);
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}

		if (*comm[1] == 'o' && strcmp(comm[1], "on") == 0) {
			files_counter = 1;
			puts(_("Filescounter is enabled"));
			return EXIT_SUCCESS;
		}

		if (*comm[1] == 'o' && strcmp(comm[1], "off") == 0) {
			files_counter = 0;
			puts(_("Filescounter is disabled"));
			return EXIT_SUCCESS;
		}

		if (*comm[1] == 's' && strcmp(comm[1], "status") == 0) {
			if (files_counter)
				puts(_("Filescounter is enabled"));
			else
				puts(_("Filescounter is disabled"));
			return EXIT_SUCCESS;
		}

		else {
			fputs(_("Usage: fc, filescounter [on, off, status]\n"),
				  stderr);
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}
	}

					/* #### UNICODE #### */
	else if (*comm[0] == 'u' && ((comm[0][1] == 'c' && !comm[0][2])
	|| strcmp(comm[0], "unicode") == 0)) {
		if (!comm[1]) {
			fputs(_("Usage: unicode, uc [on, off, status]\n"), stderr);
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}
		else if (*comm[1] == '-' && strcmp(comm[1], "--help") == 0)
			puts(_("Usage: unicode, uc [on, off, status]"));

		else {
			if (*comm[1] == 's' && strcmp(comm[1], "status") == 0)
				printf(_("%s: Unicode %s\n"), PROGRAM_NAME,
						(unicode) ? _("enabled") : _("disabled"));

			else if (*comm[1] == 'o' && strcmp(comm[1], "on") == 0) {
				unicode = 1;
				printf(_("%s: Unicode enabled\n"), PROGRAM_NAME);
			}

			else if (*comm[1] == 'o' && strcmp(comm[1], "off") == 0) {
				unicode = 0;
				printf(_("%s: Unicode disabled\n"), PROGRAM_NAME);
			}

			else {
				fputs(_("Usage: unicode, uc [on, off, status]\n"), stderr);
				exit_code = EXIT_FAILURE;
			}
		}
	}

				/* #### FOLDERS FIRST #### */
	else if (*comm[0] == 'f' && ((comm[0][1] == 'f' && !comm[0][2])
	|| strcmp(comm[0], "folders-first") == 0)) {

		if (cd_lists_on_the_fly == 0)
			return EXIT_SUCCESS;

		if (!comm[1]) {
			fputs(_("Usage: ff, folders-first [on, off, status]\n"), stderr);
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}

		if (*comm[1] == '-' && strcmp(comm[1], "--help") == 0) {
			puts(_("Usage: ff, folders-first [on, off, status]"));
			return EXIT_SUCCESS;
		}

		int status = list_folders_first;

		if (*comm[1] == 's' && strcmp(comm[1], "status") == 0)
			printf(_("%s: Folders first %s\n"), PROGRAM_NAME,
					 (list_folders_first) ? _("enabled") : _("disabled"));

		else if (*comm[1] == 'o' && strcmp(comm[1], "on") == 0)
			list_folders_first = 1;

		else if (*comm[1] == 'o' && strcmp(comm[1], "off") == 0)
			list_folders_first = 0;

		else {
			fputs(_("Usage: ff, folders-first [on, off, status]\n"), stderr);
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}

		if (list_folders_first != status) {
			if (cd_lists_on_the_fly) {
				free_dirlist();
				exit_code = list_dir();
			}
		}
	}

				/* #### LOG #### */
	else if (*comm[0] == 'l' && strcmp(comm[0], "log") == 0) {
		if (comm[1] && *comm[1] == '-'
		&& strcmp(comm[1], "--help") == 0) {
			puts(_("Usage: log [clear]"));
			return EXIT_SUCCESS;
		}

		/* I make this check here, and not in the function itself,
		 * because this function is also called by other instances of
		 * the program where no message should be printed */
		if (!config_ok) {
			fprintf(stderr, _("%s: Log function disabled\n"), PROGRAM_NAME);
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}

		exit_code = log_function(comm);
	}

				/* #### MESSAGES #### */
	else if (*comm[0] == 'm' && (strcmp(comm[0], "msg") == 0
	|| strcmp(comm[0], "messages") == 0)) {

		if (comm[1] && strcmp(comm[1], "--help") == 0) {
			puts(_("Usage: messages, msg [clear]"));
			return EXIT_SUCCESS;
		}

		if (comm[1] && strcmp(comm[1], "clear") == 0) {
			if (!msgs_n) {
				printf(_("%s: There are no messages\n"), PROGRAM_NAME);
				return EXIT_SUCCESS;
			}

			size_t i;

			for (i = 0; i < (size_t)msgs_n; i++)
				free(messages[i]);

			msgs_n = 0;
			pmsg = nomsg;
		}

		else {
			if (msgs_n) {
				size_t i;
				for (i = 0; i < (size_t)msgs_n; i++)
					printf("%s", messages[i]);
			}

			else
				printf(_("%s: There are no messages\n"), PROGRAM_NAME);
		}
	}

				/* #### ALIASES #### */
	else if (*comm[0] == 'a' && strcmp(comm[0], "alias") == 0) {
		if (comm[1]) {
			if (*comm[1] == '-' && strcmp(comm[1], "--help") == 0) {
				puts(_("Usage: alias [import FILE]"));
				return EXIT_SUCCESS;
			}

			else if (*comm[1] == 'i' && strcmp(comm[1], "import") == 0) {

				if (!comm[2]) {
					fprintf(stderr, _("Usage: alias import FILE\n"));
					exit_code = EXIT_FAILURE;
					return EXIT_FAILURE;
				}

				exit_code = alias_import(comm[2]);
				return exit_code;
			}
		}

		if (aliases_n) {
			size_t i;
			for (i = 0; i < aliases_n; i++)
				printf("%s", aliases[i]);
		}
	}

				/* #### SHELL #### */
	else if (*comm[0] == 's' && strcmp(comm[0], "shell") == 0) {
		if (!comm[1]) {
			if (sys_shell)
				printf("%s: shell: %s\n", PROGRAM_NAME, sys_shell);
			else
				printf(_("%s: shell: unknown\n"), PROGRAM_NAME);
		}

		else if (*comm[1] == '-' && strcmp(comm[1], "--help") == 0)
			puts(_("Usage: shell [SHELL]"));
		else
			exit_code = set_shell(comm[1]);
	}

				/* #### EDIT #### */
	else if (*comm[0] == 'e' && strcmp(comm[0], "edit") == 0)
		exit_code = edit_function(comm);

				/* #### HISTORY #### */
	else if (*comm[0] == 'h' && strcmp(comm[0], "history") == 0)
		exit_code = history_function(comm);

			  /* #### HIDDEN FILES #### */
	else if (*comm[0] == 'h' && ((comm[0][1] == 'f' && !comm[0][2])
	|| strcmp(comm[0], "hidden") == 0)) {
		if (!comm[1]) {
			fputs(_("Usage: hidden, hf [on, off, status]\n"), stderr);
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}

		else if (*comm[1] == '-' && strcmp(comm[1], "--help") == 0) {
			/* The same message is in hidden_function(), and printed
			 * whenever an invalid argument is entered */
			puts(_("Usage: hidden, hf [on, off, status]"));
			return EXIT_SUCCESS;
		}
		else
			exit_code = hidden_function(comm);
	}

					/* #### AUTOCD #### */
	else if (*comm[0] == 'a' && (strcmp(comm[0], "acd") == 0
	|| strcmp(comm[0], "autocd") == 0)) {

		if (!comm[1]) {
			fputs(_("Usage: acd, autocd [on, off, status]\n"), stderr);
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}

		if (strcmp(comm[1], "on") == 0) {
			autocd = 1;
			printf(_("%s: autocd is enabled\n"), PROGRAM_NAME);
		}

		else if (strcmp(comm[1], "off") == 0) {
			autocd = 0;
			printf(_("%s: autocd is disabled\n"), PROGRAM_NAME);
		}

		else if (strcmp(comm[1], "status") == 0) {
			if (autocd)
				printf(_("%s: autocd is enabled\n"), PROGRAM_NAME);
			else
				printf(_("%s: autocd is disabled\n"), PROGRAM_NAME);
		}

		else if (*comm[1] == '-' && strcmp(comm[1], "--help") == 0)
			puts(_("Usage: acd, autocd [on, off, status]"));

		else {
			fputs(_("Usage: acd, autocd [on, off, status]\n"), stderr);
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}
	}

					/* #### AUTO-OPEN #### */
	else if (*comm[0] == 'a' && ((comm[0][1] == 'o' && !comm[0][2])
	|| strcmp(comm[0], "auto-open") == 0)) {

		if (!comm[1]) {
			fputs(_("Usage: ao, auto-open [on, off, status]\n"), stderr);
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}

		if (strcmp(comm[1], "on") == 0) {
			auto_open = 1;
			printf(_("%s: auto-open is enabled\n"), PROGRAM_NAME);
		}

		else if (strcmp(comm[1], "off") == 0) {
			auto_open = 0;
			printf(_("%s: auto-open is disabled\n"), PROGRAM_NAME);
		}

		else if (strcmp(comm[1], "status") == 0) {
			if (auto_open)
				printf(_("%s: auto-open is enabled\n"), PROGRAM_NAME);
			else
				printf(_("%s: auto-open is disabled\n"), PROGRAM_NAME);
		}

		else if (strcmp(comm[1], "--help") == 0)
			puts(_("Usage: ao, auto-open [on, off, status]"));

		else {
			fputs(_("Usage: ao, auto-open [on, off, status]\n"), stderr);
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}
	}

					/* #### COMMANDS #### */
	else if (*comm[0] == 'c' && (strcmp(comm[0], "cmd") == 0
	|| strcmp(comm[0], "commands") == 0))
		exit_code = list_commands();

			  /* #### AND THESE ONES TOO #### */
	/* These functions just print stuff, so that the value of exit_code
	 * is always zero, that is to say, success */
	else if (strcmp(comm[0], "path") == 0 || strcmp(comm[0], "cwd") == 0)
		printf("%s\n", ws[cur_ws].path);

	else if ((*comm[0] == '?' && !comm[0][1])
	|| strcmp(comm[0], "help") == 0)
		help_function();

	else if (*comm[0] == 'c' && ((comm[0][1] == 'c' && !comm[0][2])
	|| strcmp(comm[0], "colors") == 0))
		color_codes();

	else if (*comm[0] == 'v' && (strcmp(comm[0], "ver") == 0
	|| strcmp(comm[0], "version") == 0))
		version_function();

	else if (*comm[0] == 'f' && comm[0][1] == 's' && !comm[0][2])
		free_software();

	else if (*comm[0] == 'b' && strcmp(comm[0], "bonus") == 0)
		bonus_function();

	else if (*comm[0] == 's' && strcmp(comm[0], "splash") == 0)
		splash();

					/* #### QUIT #### */
	else if ((*comm[0] == 'q' && !comm[0][1])
	|| strcmp(comm[0], "quit") == 0 || strcmp(comm[0], "exit") == 0) {

		/* Free everything and exit */
		int i = (int)args_n + 1;
		while (--i >= 0)
			free(comm[i]);
		free(comm);

		exit(exit_code);
	}

	else if (*comm[0] == 'Q' && !comm[1]) {
		int i = (int)args_n + 1;
		while (--i >= 0)
			free(comm[i]);
		free(comm);

		cd_on_quit = 1;
		exit(exit_code);
	}

	else {

				/* ###############################
				 * #     AUTOCD & AUTOOPEN (2)   #
				 * ############################### */

		struct stat file_attrib;
		if (stat(comm[0], &file_attrib) == 0) {
			if ((file_attrib.st_mode & S_IFMT) == S_IFDIR) {

				if (autocd) {
					exit_code = cd_function(comm[0]);
					return exit_code;
				}

				fprintf(stderr, _("%s: %s: Is a directory\n"),
						PROGRAM_NAME, comm[0]);
				exit_code = EXIT_FAILURE;
				return EXIT_FAILURE;
			}

			else if (auto_open && (file_attrib.st_mode & S_IFMT)
			== S_IFREG) {
				/* Make sure we have not an executable file */
				if (!(file_attrib.st_mode
				& (S_IXUSR|S_IXGRP|S_IXOTH))) {

					char *cmd[] = { "open", comm[0], (comm[1])
									? comm[1] : NULL, NULL };
					exit_code = open_function(cmd);
					return exit_code;
				}
			}
		}

	/* ####################################################
	 * #                EXTERNAL/SHELL COMMANDS           #
	 * ####################################################*/


		/* LOG EXTERNAL COMMANDS
		* 'no_log' will be true when running profile or prompt commands */
		if (!no_log)
			exit_code = log_function(comm);

		/* PREVENT UNGRACEFUL EXIT */
		/* Prevent the user from killing the program via the 'kill',
		 * 'pkill' or 'killall' commands, from within CliFM itself.
		 * Otherwise, the program will be forcefully terminated without
		 * freeing allocated memory */
		if ((*comm[0] == 'k' || *comm[0] == 'p')
		&& (strcmp(comm[0], "kill") == 0
		|| strcmp(comm[0], "killall") == 0
		|| strcmp(comm[0], "pkill") == 0)) {
			size_t i;
			for (i = 1; i <= args_n; i++) {
				if ((strcmp(comm[0], "kill") == 0
				&& atoi(comm[i]) == (int)own_pid)
				|| ((strcmp(comm[0], "killall") == 0
				|| strcmp(comm[0], "pkill") == 0)
				&& strcmp(comm[i], argv_bk[0]) == 0)) {
					fprintf(stderr, _("%s: To gracefully quit enter"
									  "'quit'\n"), PROGRAM_NAME);
					exit_code = EXIT_FAILURE;
					return EXIT_FAILURE;
				}
			}
		}

		/* CHECK WHETHER EXTERNAL COMMANDS ARE ALLOWED */
		if (!ext_cmd_ok) {
			fprintf(stderr, _("%s: External commands are not allowed. "
					"Run 'ext on' to enable them.\n"), PROGRAM_NAME);
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}

		if (*comm[0] == *argv_bk[0] && strcmp(comm[0], argv_bk[0]) == 0) {
			fprintf(stderr, "%s: Nested instances are not allowed\n",
					PROGRAM_NAME);
			return EXIT_FAILURE;
		}

		/*
		 * By making precede the command by a colon or a semicolon, the
		 * user can BYPASS CliFM parsing, expansions, and checks to be
		 * executed DIRECTLY by the system shell (execle). For example:
		 * if the amount of files listed on the screen (ELN's) is larger
		 * or equal than 644 and the user tries to issue this command:
		 * "chmod 644 filename", CLIFM will take 644 to be an ELN, and
		 * will thereby try to expand it into the corresponding filename,
		 * which is not what the user wants. To prevent this, simply run
		 * the command as follows: ";chmod 644 filename" */

		if (*comm[0] == ':' || *comm[0] == ';') {
			/* Remove the colon from the beginning of the first argument,
			 * that is, move the pointer to the next (second) position */
			char *comm_tmp = comm[0] + 1;
			/* If string == ":" or ";" */
			if (!comm_tmp || !*comm_tmp) {
				fprintf(stderr, _("%s: '%c': Syntax error\n"),
						PROGRAM_NAME, *comm[0]);
				exit_code = EXIT_FAILURE;
				return EXIT_FAILURE;
			}
			else
				strcpy(comm[0], comm_tmp);
		}

		/* #### RUN THE EXTERNAL COMMAND #### */

		/* Store the command and each argument into a single array to be
		 * executed by execle() using the system shell (/bin/sh -c) */
		char *ext_cmd = (char *)NULL;
		size_t ext_cmd_len = strlen(comm[0]);
		ext_cmd = (char *)xnmalloc(ext_cmd_len + 1, sizeof(char));
		strcpy(ext_cmd, comm[0]);

		register size_t i;
		if (args_n) { /* This will be false in case of ";cmd" or ":cmd" */

			for (i = 1; i <= args_n; i++) {
				ext_cmd_len += strlen(comm[i]) + 1;
				ext_cmd = (char *)xrealloc(ext_cmd, (ext_cmd_len + 1)
										   * sizeof(char));
				strcat(ext_cmd, " ");
				strcat(ext_cmd, comm[i]);
			}
		}

		/* Since we modified LS_COLORS, store its current value and unset
		 * it. Some shell commands use LS_COLORS to display their outputs
		 * ("ls -l", for example, use the "no" value to print file
		 * properties). So, we unset it to prevent wrong color output
		 * for external commands. The disadvantage of this procedure is
		 * that if the user uses a customized LS_COLORS, unsetting it
		 * set its value to default, and the customization is lost. */

#if __FreeBSD__
		char *my_ls_colors = (char *)NULL, *p = (char *)NULL;
		/* For some reason, when running on FreeBSD Valgrind complains
		 * about overlapping source and destiny in setenv() if I just
		 * copy the address returned by getenv() instead of the string
		 * itself. Not sure why, but this makes the error go away */
		p = getenv("LS_COLORS");
		my_ls_colors = (char *)xnmalloc(strlen(p) + 1, sizeof(char *));
		strcpy(my_ls_colors, p);
		p = (char *)NULL;

#else
		static char *my_ls_colors = (char *)NULL;
		my_ls_colors = getenv("LS_COLORS");
#endif

		if (ls_colors_bk && *ls_colors_bk != '\0')
			setenv("LS_COLORS", ls_colors_bk, 1);
		else
			unsetenv("LS_COLORS");

		if (launch_execle(ext_cmd) != EXIT_SUCCESS)
			exit_code = EXIT_FAILURE;

		free(ext_cmd);

		/* Restore LS_COLORS value to use CliFM colors */
		setenv("LS_COLORS", my_ls_colors, 1);

#if __FreeBSD__
		free(my_ls_colors);
#endif

		/* Reload the list of available commands in PATH for TAB completion.
		 * Why? If this list is not updated, whenever some new program is
		 * installed, renamed, or removed from some of the pathsin PATH
		 * while in CliFM, this latter needs to be restarted in order
		 * to be able to recognize the new program for TAB completion */

		int j;
		if (bin_commands) {
			j = (int)path_progsn;
			while (--j >= 0)
				free(bin_commands[j]);

			free(bin_commands);

			bin_commands = (char  **)NULL;
		}

		if (paths) {
			j = (int)path_n;
			while (--j >= 0)
				free(paths[j]);
		}

		path_n = (size_t)get_path_env();
		get_path_programs();
	}

	return exit_code;
}

static inline int
is_bin_cmd(const char *str)
/* Return one if STR is a command in PATH or zero if not */
{
	char *p = (char *)str, *q = (char *)str;
	int index = 0, space_index = -1;

	while (*p) {
		if (*p == ' ') {
			*p = '\0';
			space_index = index;
			break;
		}
		p++;
		index++;
	}

	size_t i;
	for (i = 0; bin_commands[i]; i++) {
		if (*q == *bin_commands[i] && q[1] == bin_commands[i][1]
		&& strcmp(q, bin_commands[i]) == 0) {
			if (space_index != -1)
				q[space_index] = ' ';
			return 1;
		}
	}

	if (space_index != -1)
		q[space_index] = ' ';

	return 0;
}

static inline int
digit_found(const char *str)
/* Returns 0 if digit is found and preceded by a letter in STR, or one if not */
{
	char *p = (char *)str;
	int c = 0;

	while(*p) {
		if (c++ && _ISDIGIT(*p) && _ISALPHA(*(p - 1)))
			return 1;
		p++;
	}

	return 0;
}

static char **
parse_input_str(char *str)
/*
 * This function is one of the keys of CliFM. It will perform a series of
 * actions:
 * 1) Take the string stored by readline and get its substrings without
 * spaces.
 * 2) In case of user defined variable (var=value), it will pass the
 * whole string to exec_cmd(), which will take care of storing the
 * variable;
 * 3) If the input string begins with ';' or ':' the whole string is
 * send to exec_cmd(), where it will be directly executed by the system
 * shell (via launch_execle()) to prevent all of the expansions made
 * here.
 * 4) The following expansions (especific to CLiFM) are performed here:
 * ELN's, "sel" keyword, ranges of numbers (ELN's), pinned dir and
 * bookmark names, and, for internal commands only, tilde, braces,
 * wildcards, command and paramenter substitution, and regex expansion
 * are performed here as well.
 * These expansions are the most import part of this function.
 */

/* NOTE: Though filenames could consist of everything except of slash
 * and null characters, POSIX.1 recommends restricting filenames to
 * consist of the following characters: letters (a-z, A-Z), numbers
 * (0-9), period (.), dash (-), and underscore ( _ ).

 * NOTE 2: There is no any need to pass anything to this function, since
 * the input string I need here is already in the readline buffer. So,
 * instead of taking the buffer from a function parameter (str) I could
 * simply use rl_line_buffer. However, since I use this function to
 * parse other strings, like history lines, I need to keep the str
 * argument */

{
	register size_t i = 0;
	int fusedcmd_ok = 0;

/** ###################### */
	/* Before splitting 'CMDNUM' into 'CMD NUM', make sure CMDNUM is not
	 * a cmd in PATH (for example, md5sum) */
	if (digit_found(str) && !is_bin_cmd(str)) {
		char *p = split_fusedcmd(str);
		if (p) {
			fusedcmd_ok = 1;
			str = p;
			p = (char *)NULL;
		}
	}
/** ###################### */

		   /* ########################################
			* #    0) CHECK FOR SPECIAL FUNCTIONS    #
			* ########################################*/

	int chaining = 0, cond_cmd = 0, send_shell = 0;

				/* ###########################
				 * #  0.a) RUN AS EXTERNAL   #
				 * ###########################*/

	/* If invoking a command via ';' or ':' set the send_shell flag to
	 * true and send the whole string to exec_cmd(), in which case no
	 * expansion is made: the command is send to the system shell as
	 * is. */
	if (*str == ';' || *str == ':')
		send_shell = 1;

	if (!send_shell) {
		for (i = 0; str[i]; i++) {

				/* ##################################
				 * #   0.b) CONDITIONAL EXECUTION   #
				 * ##################################*/

			/* Check for chained commands (cmd1;cmd2) */
			if (!chaining && str[i] == ';' && i > 0
			&& str[i - 1] != '\\')
				chaining = 1;

			/* Check for conditional execution (cmd1 && cmd 2)*/
			if (!cond_cmd && str[i] == '&' && i > 0
			&& str[i - 1] != '\\' && str[i + 1] && str[i + 1] == '&')
				cond_cmd = 1;

				/* ##################################
				 * #   0.c) USER DEFINED VARIABLE   #
				 * ##################################*/

			/* If user defined variable send the whole string to
			 * exec_cmd(), which will take care of storing the
			 * variable. */
			if (!(flags & IS_USRVAR_DEF) && str[i] == '=' && i > 0
			&& str[i - 1] != '\\' && str[0] != '=') {
				/* Remove leading spaces. This: '   a="test"' should be
				 * taken as a valid variable declaration */
				char *p = str;
				while (*p == ' ' || *p == '\t')
					p++;

				/* If first non-space is a number, it's not a variable
				 * name */
				if (!_ISDIGIT(*p)) {
					int space_found = 0;
					/* If there are no spaces before '=', take it as a
					 * variable. This check is done in order to avoid
					 * taking as a variable things like:
					 * 'ls -color=auto' */
					while (*p != '=') {
						if (*(p++) == ' ')
							space_found = 1;
					}

					if (!space_found)
						flags |= IS_USRVAR_DEF;
				}

				p = (char *)NULL;
			}
		}
	}

	/* If chained commands, check each of them. If at least one of them
	 * is internal, take care of the job (the system shell does not know
	 * our internal commands and therefore cannot execute them); else,
	 * if no internal command is found, let it to the system shell */
	if (chaining || cond_cmd) {

		/* User defined variables are always internal, so that there is
		 * no need to check whatever else is in the command string */
		if (flags & IS_USRVAR_DEF) {
			exec_chained_cmds(str);
			if (fusedcmd_ok)
				free(str);
			return (char **)NULL;
		}

		register size_t j = 0;
		size_t str_len = strlen(str), len = 0, internal_ok = 0;
		char *buf = (char *)NULL;

		/* Get each word (cmd) in STR */
		buf = (char *)xcalloc(str_len + 1, sizeof(char));
		for (j = 0; j < str_len; j++) {

			while (str[j] && str[j] != ' ' && str[j] != ';'
			&& str[j] != '&') {
				buf[len++] = str[j++];
			}

			if (strcmp(buf, "&&") != 0) {
				if (is_internal_c(buf)) {
					internal_ok = 1;
					break;
				}
			}

			memset(buf, '\0', len);
			len = 0;
		}

		free(buf);
		buf = (char *)NULL;

		if (internal_ok) {
			exec_chained_cmds(str);
			if (fusedcmd_ok)
				free(str);
			return (char **)NULL;
		}
	}

	if (flags & IS_USRVAR_DEF || send_shell) {
		/* Remove leading spaces, again */
		char *p = str;
		while (*p == ' ' || *p == '\t')
			p++;

		args_n = 0;

		char **cmd = (char **)NULL;
		cmd = (char **)xnmalloc(2, sizeof(char *));
		cmd[0] = savestring(p, strlen(p));
		cmd[1] = (char *)NULL;

		p = (char *)NULL;

		if (fusedcmd_ok)
			free(str);

		return cmd;
		/* If ";cmd" or ":cmd" the whole input line will be send to
		 * exec_cmd() and will be executed by the system shell via
		 * execle(). Since we don't run split_str() here, dequoting
		 * and deescaping is performed directly by the system shell */
	}

		/* ################################################
		 * #     1) SPLIT INPUT STRING INTO SUBSTRINGS    #
		 * ################################################ */

	/* split_str() returns an array of strings without leading,
	 * terminating and double spaces. */
	char **substr = split_str(str);

/** ###################### */
	if (fusedcmd_ok) /* Just in case split_fusedcmd returned NULL */
		free(str);
/** ###################### */

	/* NOTE: isspace() not only checks for space, but also for new line,
	 * carriage return, vertical and horizontal TAB. Be careful when
	 * replacing this function. */

	if (!substr)
		return (char **)NULL;

					/* ######################
					 * #     TRASH AS RM    #
					 * ###################### */

	if (tr_as_rm && *substr[0] == 'r' && !substr[0][1]) {
		substr[0] = (char *)xrealloc(substr[0], 3 * sizeof(char));
		*substr[0] = 't';
		substr[0][1] = 'r';
		substr[0][2] = '\0';
	}

				/* ##############################
				 * #   2) BUILTIN EXPANSIONS    #
				 * ##############################

	 * Ranges, sel, ELN, pinned dirs, bookmarks, and internal variables.
	 * These expansions are specific to CliFM. To be able to use them
	 * even with external commands, they must be expanded here, before
	 * sending the input string, in case the command is external, to
	 * the system shell */

	is_sel = 0, sel_is_last = 0;

	size_t int_array_max = 10, ranges_ok = 0;
	int *range_array = (int *)xnmalloc(int_array_max, sizeof(int));

	for (i = 0; i <= args_n; i++) {

		register size_t j = 0;

			/* ######################################
			 * #     2.a) FASTBACK EXPANSION        #
			 * ###################################### */

		if (*substr[i] == '.' && substr[i][1] == '.' && substr[i][2] == '.') {
			char *tmp = fastback(substr[i]);
			if (tmp) {
				substr[i] = (char *)xrealloc(substr[i], (strlen(tmp) + 1)
											 * sizeof(char));
				strcpy(substr[i], tmp);
				free(tmp);
			}
		}

			/* ######################################
			 * #     2.b) PINNED DIR EXPANSION      #
			 * ###################################### */

		if (*substr[i] == ',' && !substr[i][1] && pinned_dir) {
			substr[i] = (char *)xrealloc(substr[i], (strlen(pinned_dir)
										 + 1) * sizeof(char));
			strcpy(substr[i], pinned_dir);
		}

			/* ######################################
			 * #      2.c) BOOKMARKS EXPANSION      #
			 * ###################################### */

		/* Expand bookmark names into paths */
		if (expand_bookmarks) {

			int bm_exp = 0;

			for (j = 0; j < bm_n; j++) {

				if (bookmarks[j].name && *substr[i] == *bookmarks[j].name
				&& strcmp(substr[i], bookmarks[j].name) == 0) {

					/* Do not expand bookmark names that conflicts
					 * with a filename in CWD */
					int conflict = 0, k = (int)files;

					while (--k >= 0) {
						if (*bookmarks[j].name == *file_info[k].name
						&& strcmp(bookmarks[j].name, file_info[k].name)
						== 0) {
							conflict = 1;
							break;
						}
					}

					if (!conflict && bookmarks[j].path) {
						substr[i] = (char *)xrealloc(substr[i],
									(strlen(bookmarks[j].path) + 1)
									* sizeof(char));
						strcpy(substr[i], bookmarks[j].path);

						bm_exp =  1;

						break;
					}
				}
			}

			/* Do not perform further checks on the expanded bookmark */
			if (bm_exp)
				continue;
		}

		/* ############################################# */

		size_t substr_len = strlen(substr[i]);

		/* Check for ranges */
		for (j = 0; substr[i][j]; j++) {

			/* If some alphabetic char, besides '-', is found in the
			 * string, we have no range */
			if (substr[i][j] != '-' && !_ISDIGIT(substr[i][j]))
				break;

			/* If a range is found, store its index */
			if (j > 0 && j < substr_len && substr[i][j] == '-' &&
			_ISDIGIT(substr[i][j - 1]) && _ISDIGIT(substr[i][j + 1]))
				if (ranges_ok < int_array_max)
					range_array[ranges_ok++] = (int)i;
		}

		/* Expand 'sel' only as an argument, not as command */
		if (i > 0 && *substr[i] == 's' && strcmp(substr[i], "sel") == 0)
			is_sel = (short)i;
	}

			/* ####################################
			 * #       2.d) RANGES EXPANSION      #
			 * ####################################*/

	 /* Expand expressions like "1-3" to "1 2 3" if all the numbers in
	  * the range correspond to an ELN */

	if (ranges_ok) {
		size_t old_ranges_n = 0;
		register size_t r = 0;

		for (r = 0; r < ranges_ok; r++) {
			size_t ranges_n = 0;
			int *ranges = expand_range(substr[range_array[r] +
									   (int)old_ranges_n], 1);
			if (ranges) {
				register size_t j = 0;

				for (ranges_n = 0; ranges[ranges_n]; ranges_n++);

				char **ranges_cmd = (char **)NULL;

				ranges_cmd = (char **)xcalloc(args_n + ranges_n + 2,
											  sizeof(char *));

				for (i = 0; i < (size_t)range_array[r] + old_ranges_n; i++)
					ranges_cmd[j++] = savestring(substr[i], strlen(substr[i]));

				for (i = 0; i < ranges_n; i++) {
					ranges_cmd[j] = (char *)xcalloc((size_t)DIGINUM(ranges[i])
													+ 1, sizeof(int));
					sprintf(ranges_cmd[j++], "%d", ranges[i]);
				}

				for (i = (size_t)range_array[r] + old_ranges_n + 1;
				i <= args_n; i++) {
					ranges_cmd[j++] = savestring(substr[i],
										strlen(substr[i]));
				}

				ranges_cmd[j] = NULL;
				free(ranges);

				for (i = 0; i <= args_n; i++)
					free(substr[i]);

				substr = (char **)xrealloc(substr, (args_n + ranges_n + 2)
										   * sizeof(char *));

				for (i = 0; i < j; i++) {
					substr[i] = savestring(ranges_cmd[i], strlen(ranges_cmd[i]));
					free(ranges_cmd[i]);
				}

				free(ranges_cmd);
				args_n = j - 1;
			}

			old_ranges_n += (ranges_n - 1);
		}
	}

	free(range_array);

				/* ##########################
				 * #   2.e) SEL EXPANSION   #
				 * ##########################*/

/*  if (is_sel && *substr[0] != '/') { */
	if (is_sel) {

		if ((size_t)is_sel == args_n)
			sel_is_last = 1;

		if (sel_n) {
			register size_t j = 0;
			char **sel_array = (char **)NULL;
			sel_array = (char **)xnmalloc(args_n + sel_n + 2, sizeof(char *));

			for (i = 0; i < (size_t)is_sel; i++)
				sel_array[j++] = savestring(substr[i], strlen(substr[i]));

			for (i = 0; i < sel_n; i++) {
				/* Escape selected filenames and copy them into tmp
				 * array */
				char *esc_str = escape_str(sel_elements[i]);

				if (esc_str) {
					sel_array[j++] = savestring(esc_str, strlen(esc_str));
					free(esc_str);
					esc_str = (char *)NULL;
				}

				else {
					fprintf(stderr, _("%s: %s: Error quoting filename\n"),
							PROGRAM_NAME, sel_elements[j]);
					/* Free elements selected thus far and and all the
					 * input substrings */
					register size_t k = 0;

					for (k = 0; k < j; k++)
						free(sel_array[k]);

					free(sel_array);

					for (k = 0; k <= args_n; k++)
						free(substr[k]);

					free(substr);

					return (char **)NULL;
				}
			}

			for (i = (size_t)is_sel + 1; i <= args_n; i++)
				sel_array[j++] = savestring(substr[i], strlen(substr[i]));

			for (i = 0; i <= args_n; i++)
				free(substr[i]);

			substr = (char **)xrealloc(substr, (args_n + sel_n + 2)
									   * sizeof(char *));

			for (i = 0; i < j; i++) {
				substr[i] = savestring(sel_array[i], strlen(sel_array[i]));
				free(sel_array[i]);
			}

			free(sel_array);
			substr[i] = (char *)NULL;
			args_n = j - 1;
		}

		else {
			/* 'sel' is an argument, but there are no selected files. */
			fprintf(stderr, _("%c%s: There are no selected files%c"),
					kb_shortcut ? '\n' : '\0', PROGRAM_NAME,
					kb_shortcut ? '\0' : '\n');

			register size_t j = 0;

			for (j = 0; j <= args_n; j++)
				free(substr[j]);

			free(substr);

			return (char **)NULL;
		}
	}

	int stdin_dir_ok = 0;

	if (STDIN_TMP_DIR && strcmp(ws[cur_ws].path, STDIN_TMP_DIR) == 0)
		stdin_dir_ok = 1;

	for (i = 0; i <= args_n; i++) {

				/* ##########################
				 * #   2.f) ELN EXPANSION   #
				 * ##########################*/

		/* If autocd is set to false, i must be bigger than zero because
		 * the first string in comm_array, the command name, should NOT
		 * be expanded, but only arguments. Otherwise, if the expanded
		 * ELN happens to be a program name as well, this program will
		 * be executed, and this, for sure, is to be avoided */

		/* The 'sort', 'mf', 'ws', and 'jo' commands take digits as
		 * arguments. So, do not expand ELN's in these cases */
		if (strcmp(substr[0], "mf") != 0
		&& strcmp(substr[0], "st") != 0
		&& strcmp(substr[0], "ws") != 0
		&& strcmp(substr[0], "sort") != 0
		&& strcmp(substr[0], "jo") != 0) {

			if (is_number(substr[i])) {

				/* Expand first word only if autocd is set to true */
				if (i == 0 && !autocd && !auto_open)
					continue;

				int num = atoi(substr[i]);
				/* Expand numbers only if there is a corresponding ELN */

				/* Do not expand ELN if there is a file named as the
				 * ELN */
				if (eln_as_file_n) {
					int conflict = 0;

					if (eln_as_file_n > 1) {
						size_t j;

						for (j = 0; j < eln_as_file_n; j++) {
							if (atoi(file_info[eln_as_file[j]].name) == num) {
								conflict = num;
								/* One conflicting filename is enough */
								break;
							}
						}
					}

					else {
						if (atoi(file_info[eln_as_file[0]].name) == num)
							conflict = num;
					}

					if (conflict) {
						size_t j;

						for (j = 0; j <= args_n; j++)
							free(substr[j]);

						free(substr);

						fprintf(stderr, _("%s: %d: ELN-filename "
								"conflict. Bypass internal expansions "
								"to fix this issue: ';CMD "
								"FILENAME'\n"), PROGRAM_NAME,
								conflict);

						return (char **)NULL;
					}
				}

				if (num > 0 && num <= (int)files) {
					/* Replace the ELN by the corresponding escaped
					 * filename */
					int j = num - 1;
					char *esc_str = escape_str(file_info[j].name);

					if (esc_str) {

						if (file_info[j].dir && 
						file_info[j].name[file_info[j].len - 1] != '/') {
							substr[i] = (char *)xrealloc(substr[i],
										(strlen(esc_str) + 2) * sizeof(char));
							sprintf(substr[i], "%s/", esc_str);
						}

						else {
							substr[i] = (char *)xrealloc(substr[i],
										(strlen(esc_str) + 1) * sizeof(char));
							strcpy(substr[i], esc_str);
						}

						free(esc_str);
						esc_str = (char *)NULL;
					}

					else {
						fprintf(stderr, _("%s: %s: Error quoting "
							"filename\n"), PROGRAM_NAME, file_info[num-1].name);
						/* Free whatever was allocated thus far */

						for (j = 0; j <= (int)args_n; j++)
							free(substr[j]);

						free(substr);

						return (char **)NULL;
					}
				}
			}
		}

		/* #############################################
		 * #   2.g) USER DEFINED VARIABLES EXPANSION   #
		 * #############################################*/

		if (substr[i][0] == '$' && substr[i][1] != '('
		&& substr[i][1] != '{') {
			char *var_name = strchr(substr[i], '$');
			if (var_name && *(++var_name)) {
				int j = (int)usrvar_n;

				while (--j >= 0) {

					if (*var_name == *usr_var[j].name
					&& strcmp(var_name, usr_var[j].name) == 0) {
						substr[i] = (char *)xrealloc(substr[i],
								(strlen(usr_var[j].value) + 1) * sizeof(char));
						strcpy(substr[i], usr_var[j].value);
						break;
					}
				}
			}

			else {
				fprintf(stderr, _("%s: %s: Error getting variable name\n"),
						PROGRAM_NAME, substr[i]);
				size_t j;

				for (j = 0; j <= args_n; j++)
					free(substr[j]);
				free(substr);

				return (char **)NULL;
			}
		}

		/* We are in STDIN_TMP_DIR: Expand symlinks to target */
		if (stdin_dir_ok) {
			char *real_path = realpath(substr[i], NULL);

			if (real_path) {
				substr[i] = (char *)xrealloc(substr[i],
							(strlen(real_path) + 1) * sizeof(char));
				strcpy(substr[i], real_path);
				free(real_path);
			}
		}
	}

	/* #### 3) NULL TERMINATE THE INPUT STRING ARRAY #### */
	substr = (char **)xrealloc(substr, sizeof(char *) * (args_n + 2));
	substr[args_n + 1] = (char *)NULL;

	if (!is_internal(substr[0]))
		return substr;

	/* #############################################################
	 * #               ONLY FOR INTERNAL COMMANDS                  #
	 * #############################################################*/

	/* Some functions of CliFM are purely internal, that is, they are not
	 * wrappers of a shell command and do not call the system shell at all.
	 * For this reason, some expansions normally made by the system shell
	 * must be made here (in the lobby [got it?]) in order to be able to
	 * understand these expansions at all. */

		/* ###############################################
		 * #   3) WILDCARD, BRACE, AND TILDE EXPANSION   #
		 * ############################################### */

	int *glob_array = (int *)xnmalloc(int_array_max, sizeof(int));
	int *word_array = (int *)xnmalloc(int_array_max, sizeof(int));
	size_t glob_n = 0, word_n = 0;

	for (i = 0; substr[i]; i++) {

		/* Do not perform any of the expansions below for selected
		 * elements: they are full path filenames that, as such, do not
		 * need any expansion */
		if (is_sel) { /* is_sel is true only for the current input and if
			there was some "sel" keyword in it */
			/* Strings between is_sel and sel_n are selected filenames */
			if (i >= (size_t)is_sel && i <= sel_n)
				continue;
		}

		/* Ignore the first string of the search function: it will be
		 * expanded by the search function itself */
		if (substr[0][0] == '/' && i == 0)
			continue;

		/* Tilde expansion is made by glob() */
		if (*substr[i] == '~') {
			if (glob_n < int_array_max)
				glob_array[glob_n++] = (int)i;
		}

		register size_t j = 0;

		for (j = 0; substr[i][j]; j++) {

			/* Brace and wildcard expansion is made by glob()
			 * as well */
			if ((substr[i][j] == '*' || substr[i][j] == '?'
			|| substr[i][j] == '[' || substr[i][j] == '{')
			&& substr[i][j + 1] != ' ') {
			/* Strings containing these characters are taken as
			 * wildacard patterns and are expanded by the glob
			 * function. See man (7) glob */
				if (glob_n < int_array_max)
					glob_array[glob_n++] = (int)i;
			}

			/* Command substitution is made by wordexp() */
			if (substr[i][j] == '$' && (substr[i][j + 1] == '('
			|| substr[i][j + 1] == '{')) {
				if (word_n < int_array_max)
					word_array[word_n++] = (int)i;
			}

			if (substr[i][j] == '`' && substr[i][j + 1] != ' ') {
				if (word_n < int_array_max)
					word_array[word_n++] = (int)i;
			}
		}
	}

	/* Do not expand if command is deselect, sel or untrash, just to
	 * allow the use of "*" for desel and untrash ("ds *" and "u *")
	 * and to let the sel function handle patterns itself */
	if (glob_n && strcmp(substr[0], "s") != 0
	&& strcmp(substr[0], "sel") != 0
	&& strcmp(substr[0], "ds") != 0
	&& strcmp(substr[0], "desel") != 0
	&& strcmp(substr[0], "u") != 0
	&& strcmp(substr[0], "undel") != 0
	&& strcmp(substr[0], "untrash") != 0) {
	 /* 1) Expand glob
		2) Create a new array, say comm_array_glob, large enough to store
		   the expanded glob and the remaining (non-glob) arguments
		   (args_n+gl_pathc)
		3) Copy into this array everything before the glob
		   (i=0;i<glob_char;i++)
		4) Copy the expanded elements (if none, copy the original element,
		   comm_array[glob_char])
		5) Copy the remaining elements (i=glob_char+1;i<=args_n;i++)
		6) Free the old comm_array and fill it with comm_array_glob
	  */
		size_t old_pathc = 0;
		/* glob_array stores the index of the globbed strings. However,
		 * once the first expansion is done, the index of the next globbed
		 * string has changed. To recover the next globbed string, and
		 * more precisely, its index, we only need to add the amount of
		 * files matched by the previous instances of glob(). Example:
		 * if original indexes were 2 and 4, once 2 is expanded 4 stores
		 * now some of the files expanded in 2. But if we add to 4 the
		 * amount of files expanded in 2 (gl_pathc), we get now the
		 * original globbed string pointed by 4.
		*/
		register size_t g = 0;
		for (g = 0; g < (size_t)glob_n; g++){
			glob_t globbuf;

			if (glob(substr[glob_array[g] + (int)old_pathc],
			GLOB_BRACE|GLOB_TILDE, NULL, &globbuf) != EXIT_SUCCESS) {
				globfree(&globbuf);
				continue;
			}

			if (globbuf.gl_pathc) {
				register size_t j = 0;
				char **glob_cmd = (char **)NULL;

				glob_cmd = (char **)xcalloc(args_n + globbuf.gl_pathc
											+ 1, sizeof(char *));

				for (i = 0; i < ((size_t)glob_array[g] + old_pathc); i++)
					glob_cmd[j++] = savestring(substr[i], strlen(substr[i]));

				for (i = 0; i < globbuf.gl_pathc; i++) {

					/* Do not match "." or ".." */
					if (strcmp(globbuf.gl_pathv[i], ".") == 0
					|| strcmp(globbuf.gl_pathv[i], "..") == 0)
						continue;

					/* Escape the globbed filename and copy it */
					char *esc_str = escape_str(globbuf.gl_pathv[i]);

					if (esc_str) {
						glob_cmd[j++] = savestring(esc_str, strlen(esc_str));
						free(esc_str);
					}

					else {
						fprintf(stderr, _("%s: %s: Error quoting "
							"filename\n"), PROGRAM_NAME, globbuf.gl_pathv[i]);
						register size_t k = 0;

						for (k = 0; k < j; k++)
							free(glob_cmd[k]);

						free(glob_cmd);
						glob_cmd = (char **)NULL;

						for (k = 0; k <= args_n; k++)
							free(substr[k]);

						free(substr);

						globfree(&globbuf);

						return (char **)NULL;
					}
				}

				for (i = (size_t)glob_array[g] + old_pathc + 1;
				i <= args_n; i++)
					glob_cmd[j++] = savestring(substr[i], strlen(substr[i]));

				glob_cmd[j] = (char *)NULL;

				for (i = 0; i <= args_n; i++)
					free(substr[i]);

				free(substr);
				substr = glob_cmd;
				glob_cmd = (char **)NULL;

				args_n = j - 1;
			}

			old_pathc += (globbuf.gl_pathc - 1);
			globfree(&globbuf);
		}
	}

	free(glob_array);

		/* #############################################
		 * #    4) COMMAND & PARAMETER SUBSTITUTION    #
		 * ############################################# */

	if (word_n) {

		size_t old_pathc = 0;

		register size_t w = 0;
		for (w = 0; w < (size_t)word_n; w++) {
			wordexp_t wordbuf;
			if (wordexp(substr[word_array[w] + (int)old_pathc],
			&wordbuf, 0) != EXIT_SUCCESS) {
				wordfree(&wordbuf);
				continue;
			}

			if (wordbuf.we_wordc) {
				register size_t j = 0;
				char **word_cmd = (char **)NULL;

				word_cmd = (char **)xcalloc(args_n + wordbuf.we_wordc
											+ 1, sizeof(char *));

				for (i = 0; i < ((size_t)word_array[w] + old_pathc);
				i++)
					word_cmd[j++] = savestring(substr[i], strlen(substr[i]));

				for (i = 0; i < wordbuf.we_wordc; i++) {
					/* Escape the globbed filename and copy it*/
					char *esc_str = escape_str(wordbuf.we_wordv[i]);

					if (esc_str) {
						word_cmd[j++] = savestring(esc_str, strlen(esc_str));
						free(esc_str);
					}

					else {
						fprintf(stderr, _("%s: %s: Error quoting "
							  "filename\n"), PROGRAM_NAME, wordbuf.we_wordv[i]);

						register size_t k = 0;

						for (k = 0; k < j; k++)
							free(word_cmd[k]);

						free(word_cmd);

						word_cmd = (char **)NULL;

						for (k = 0; k <= args_n; k++)
							free(substr[k]);

						free(substr);

						return (char **)NULL;
					}
				}

				for (i = (size_t)word_array[w] + old_pathc + 1;
				i <= args_n; i++)
					word_cmd[j++] = savestring(substr[i], strlen(substr[i]));

				word_cmd[j] = (char *)NULL;

				for (i = 0; i <= args_n; i++)
					free(substr[i]);

				free(substr);
				substr = word_cmd;
				word_cmd = (char **)NULL;

				args_n = j - 1;
			}

			old_pathc += (wordbuf.we_wordc - 1);
			wordfree(&wordbuf);
		}
	}

	free(word_array);

	if ((*substr[0] == 'd' || *substr[0] == 'u')
	&& (strcmp(substr[0], "desel") == 0
	|| strcmp(substr[0], "undel") == 0
	|| strcmp(substr[0], "untrash") == 0)) {

		/* Null terminate the input string array (again) */
		substr = (char **)xrealloc(substr, (args_n + 2) * sizeof(char *));
		substr[args_n + 1] = (char *)NULL;

		return substr;
	}
		/* #############################################
		 * #             5) REGEX EXPANSION            #
		 * ############################################# */

	if (*substr[0] == 's' && (!substr[0][1]
	|| strcmp(substr[0], "sel") == 0))
		return substr;

	char **regex_files = (char **)xnmalloc(files + args_n + 2, sizeof(char *));

	size_t j, r_files = 0;

	for (i = 0; substr[i]; i++) {

		if (r_files > (files + args_n))
			break;

		/* Ignore the first string of the search function: it will be
		 * expanded by the search function itself */
		if (*substr[0] == '/') {
			regex_files[r_files++] = substr[i];
			continue;
		}

		if (check_regex(substr[i]) != EXIT_SUCCESS) {
			regex_files[r_files++] = substr[i];
			continue;
		}

		regex_t regex;

		if (regcomp(&regex, substr[i], REG_NOSUB|REG_EXTENDED)
		!= EXIT_SUCCESS) {
/*          fprintf(stderr, "%s: %s: Invalid regular expression",
					PROGRAM_NAME, substr[i]); */
			regfree(&regex);
			regex_files[r_files++] = substr[i];
			continue;
		}

		int reg_found = 0;

		for (j = 0; j < files; j++) {

			if (regexec(&regex, file_info[j].name, 0, NULL, 0) == EXIT_SUCCESS) {
				regex_files[r_files++] = file_info[j].name;
				reg_found = 1;
			}
		}

		if (!reg_found)
			regex_files[r_files++] = substr[i];

		regfree(&regex);
	}

	if (r_files) {
		regex_files[r_files] = (char *)NULL;

		char **tmp_files = (char **)xnmalloc(r_files + 2, sizeof(char *));
		size_t k = 0;
		for (j = 0; regex_files[j]; j++)
			tmp_files[k++] = savestring(regex_files[j], strlen(regex_files[j]));

		tmp_files[k] = (char *)NULL;

		for (j = 0; j <= args_n; j++)
			free(substr[j]);

		free(substr);

		substr = tmp_files;
		tmp_files = (char **)NULL;

		args_n = k - 1;

		free(tmp_files);
	}

	free(regex_files);

	substr = (char **)xrealloc(substr, (args_n + 2) * sizeof(char *));
	substr[args_n + 1] = (char *)NULL;

	return substr;
}

static int
initialize_readline(void)
{
	/* #### INITIALIZE READLINE (what a hard beast to tackle!!) #### */

	/* Set the name of the program using readline. Mostly used for
	 * conditional constructs in $HOME/.inputrc */
	rl_readline_name = argv_bk[0];

	 /* Enable tab auto-completion for commands (in PATH) in case of
	  * first entered string (if autocd and/or auto-open are enabled, check
	  * for paths as well). The second and later entered strings will
	  * be autocompleted with paths instead, just like in Bash, or with
	  * listed filenames, in case of ELN's. I use a custom completion
	  * function to add command and ELN completion, since readline's
	  * internal completer only performs path completion */

	/* Define a function for path completion.
	 * NULL means to use filename_entry_function (), the default
	 * filename completer. */
	rl_completion_entry_function = my_rl_path_completion;

	/* Pointer to alternative function to create matches.
	 * Function is called with TEXT, START, and END.
	 * START and END are indices in RL_LINE_BUFFER saying what the
	 * boundaries of TEXT are.
	 * If this function exists and returns NULL then call the value of
	 * rl_completion_entry_function to try to match, otherwise use the
	 * array of strings returned. */
	rl_attempted_completion_function = my_rl_completion;
	rl_ignore_completion_duplicates = 1;

	/* I'm using here a custom quoting function. If not specified,
	 * readline uses the default internal function. */
	rl_filename_quoting_function = my_rl_quote;

	/* Tell readline what char to use for quoting. This is only the
	 * readline internal quoting function, and for custom ones, like the
	 * one I use above. However, custom quoting functions, though they
	 * need to define their own quoting chars, won't be called at all
	 * if this variable isn't set. */
	rl_completer_quote_characters = "\"'";
	rl_completer_word_break_characters = " ";

	/* Whenever readline finds any of the following chars, it will call
	 * the quoting function */
	rl_filename_quote_characters = " \t\n\"\\'`@$><=,;|&{[()]}?!*^";
	/* According to readline documentation, the following string is
	 * the default and the one used by Bash: " \t\n\"\\'`@$><=;|&{(" */

	/* Executed immediately before calling the completer function, it
	 * tells readline if a space char, which is a word break character
	 * (see the above rl_completer_word_break_characters variable) is
	 * quoted or not. If it is, readline then passes the whole string
	 * to the completer function (ex: "user\ file"), and if not, only
	 * wathever it found after the space char (ex: "file")
	 * Thanks to George Brocklehurst for pointing out this function:
	 * https://thoughtbot.com/blog/tab-completion-in-gnu-readline*/
	rl_char_is_quoted_p = quote_detector;

	/* This function is executed inmediately before path completion. So,
	 * if the string to be completed is, for instance, "user\ file" (see
	 * the above comment), this function should return the dequoted
	 * string so it won't conflict with system filenames: you want
	 * "user file", because "user\ file" does not exist, and, in this
	 * latter case, readline won't find any matches */
	rl_filename_dequoting_function = dequote_str;

	/* Initialize the keyboard bindings function */
	readline_kbinds();

	return EXIT_SUCCESS;
}

static void
external_arguments(int argc, char **argv)
/* Evaluate external arguments, if any, and change initial variables to
 * its corresponding value */
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
		{"ext-cmds", no_argument, 0, 'x'},
		{"light", no_argument, 0, 'y'},
		{"sort", required_argument, 0, 'z'},

		/* Only long options */
		{"no-cd-auto", no_argument, 0, 0},
		{"no-open-auto", no_argument, 0, 1},
		{"restore-last-path", no_argument, 0, 2},
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
		{"case-ins-dirjump", no_argument, 0, 30},
		{"case-ins-path-comp", no_argument, 0, 31},
		{"cwd-in-title", no_argument, 0, 32},
		{"open", required_argument, 0, 33},
		{0, 0, 0, 0}
	};

	/* Change whenever a new (only) long option is added */
	int long_opts = 33;

	int optc;
	/* Variables to store arguments to options (-c, -p and -P) */
	char *path_value = (char *)NULL, *alt_profile_value = (char *)NULL,
		 *config_value = (char *)NULL, *kbinds_value = (char *)NULL,
		 *bm_value = (char *)NULL;

	while ((optc = getopt_long(argc, argv,
	"+aAb:c:efFgGhiIk:lLmoOp:P:sSUuvw:xyz:", longopts,
	(int *)0)) != EOF) {
		/* ':' and '::' in the short options string means 'required' and
		 * 'optional argument' respectivelly. Thus, 'p' and 'P' require
		 * an argument here. The plus char (+) tells getopt to stop
		 * processing at the first non-option (and non-argument) */
		switch (optc) {

		case 0:
			xargs.autocd = autocd = 0;
		break;

		case 1:
			xargs.auto_open = auto_open = 0;
		break;

		case 2:
			xargs.restore_last_path = restore_last_path = 1;
		break;

		case 3:
			xargs.tips = tips = 0;
		break;

		case 4:
			xargs.disk_usage = disk_usage = 1;
		break;

		case 6:
			xargs.classify = classify = 0;
		break;

		case 7:
			xargs.share_selbox = share_selbox = 1;
		break;

		case 8:
			xargs.rl_vi_mode = 1;
		break;

		case 9: {
			if (!is_number(optarg)) break;
			int opt_int = atoi(optarg);
			if (opt_int >= 0)
				xargs.max_dirhist = max_dirhist = opt_int;
		}
		break;

		case 10:
			xargs.sort_reverse = sort_reverse = 1;
		break;

		case 11:
			xargs.files_counter = files_counter = 0;
		break;

		case 12:
			xargs.welcome_message = welcome_message = 0;
		break;

		case 13:
			xargs.clear_screen = clear_screen = 0;
		break;

		case 15:
			xargs.logs = logs_enabled = 1;
		break;

		case 16: {
			if (!is_number(optarg)) break;
			int opt_int = atoi(optarg);
			if (opt_int >= 0)
			xargs.max_path = max_path = opt_int;
		}
		break;

		case 17:
			opener = savestring(optarg, strlen(optarg));
		break;

		case 18:
			xargs.expand_bookmarks = expand_bookmarks = 1;
		break;

		case 19:
			xargs.only_dirs = only_dirs = 1;
		break;

		case 20:
			xargs.list_and_quit = 1;
		break;

		case 21:
			usr_cscheme = savestring(optarg, strlen(optarg));
		break;

		case 22:
			xargs.cd_on_quit = cd_on_quit = 1;
		break;

		case 23:
			xargs.no_dirjump = 1;
		break;

		case 24:
			xargs.icons = icons = 1;
		break;

		case 25:
			xargs.icons = icons = 1;
			xargs.icons_use_file_color = 1;
		break;

		case 26:
			xargs.no_columns = 1;
			columned = 0;
		break;

		case 27:
			xargs.no_colors = 1;
			colorize = 0;
		break;

		case 28:
			if (!is_number(optarg)) break;
			int opt_int = atoi(optarg);
			if (opt_int >= 0)
			xargs.max_files = max_files = opt_int;
		break;

		case 29:
			xargs.trasrm = tr_as_rm = 1;
		break;

		case 30:
			xargs.case_sens_dirjump = case_sens_dirjump = 0;
		break;

		case 31:
			xargs.case_sens_path_comp = case_sens_path_comp = 0;
		break;

		case 32:
			xargs.cwd_in_title = 1;
		break;

		case 33: {
			struct stat attr;

			if (stat(optarg, &attr) == -1) {
				fprintf(stderr, "%s: %s: %s", PROGRAM_NAME, optarg,
						strerror(errno));
				exit(EXIT_FAILURE);
			}

			if ((attr.st_mode & S_IFMT) != S_IFDIR) {
				TMP_DIR = (char *)xnmalloc(5, sizeof(char));
				strcpy(TMP_DIR, "/tmp");
				MIME_FILE = (char *)xnmalloc(PATH_MAX, sizeof(char));
				snprintf(MIME_FILE, PATH_MAX,
						"%s/.config/clifm/profiles/%s/mimelist.cfm",
						getenv("HOME"), alt_profile ? alt_profile : "default");
				char *cmd[] = { "mm", optarg, NULL };
				int ret = mime_open(cmd);
				exit(ret);
			}

			printf(_("%s: %s: Is a directory\n"), PROGRAM_NAME, optarg);
			exit(EXIT_FAILURE);

/*			flags |= START_PATH;
			path_value = optarg;
			xargs.path = 1; */
		}
		break;

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

		case 'e':
			xargs.noeln = no_eln = 1;
		break;

		case 'f':
			flags &= ~FOLDERS_FIRST;
			list_folders_first = xargs.ffirst = 0;
			break;

		case 'F':
			flags |= FOLDERS_FIRST;
			list_folders_first = xargs.ffirst = 1;
			break;

		case 'g':
			pager = xargs.pager = 1;
			break;

		case 'G':
			pager = xargs.pager = 0;
			break;

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

		case 'k':
			kbinds_value = optarg;
			break;

		case 'l':
			long_view = xargs.longview = 0;
			break;

		case 'L':
			long_view = xargs.longview = 1;
			break;

		case 'm':
			dirhist_map = xargs.dirmap = 1;
			break;

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

		case 'S':
			xargs.stealth_mode = 1;
			break;


		case 'u':
			unicode = xargs.unicode = 0;
			break;

		case 'U':
			unicode = xargs.unicode = 1;
			break;

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
			}
			break;

		case 'x':
			ext_cmd_ok = xargs.ext = 1;
			break;

		case 'y':
			light_mode = xargs.light = 1;
			break;

		case 'z':
			{
				int arg = atoi(optarg);

				if (!is_number(optarg) || arg < 0 || arg > SORT_TYPES)
					sort = 1;
				else
					sort = arg;

				xargs.sort = sort;
			}
			break;

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
			}

			else {
				fprintf(stderr, _("%s: unknown option character '\\%x'\n"),
						 PROGRAM_NAME, (unsigned int)optopt);
			}

			exit(EXIT_FAILURE);

		default: break;
		}
	}

	/* Positional parameters */
	int i = optind;
	if (argv[i]) {
		flags |= START_PATH;
		path_value = argv[i];
		xargs.path = 1;
	}
/*	while (argv[i]) {
		printf("%d: %s\n", i, argv[i]);
		i++;
	} */

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
		}

		else {
			alt_bm_file = savestring(bm_value, strlen(bm_value));
			_err('n', PRINT_PROMPT, _("%s: Loaded alternative "
				 "bookmarks file\n"), PROGRAM_NAME);
		}
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
		}

		else {
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
		}

		else {
			alt_config_file = savestring(config_value, strlen(config_value));
			_err('n', PRINT_PROMPT, _("%s: Loaded alternative "
				 "configuration file\n"), PROGRAM_NAME);
		}

		if (config_exp)
			free(config_exp);
	}

	if ((flags & START_PATH) && path_value) {
		char *path_exp = (char *)NULL;

		if (*path_value == '~') {
			path_exp = tilde_expand(path_value);
			path_value = path_exp;
		}

		if (xchdir(path_value, SET_TITLE) == 0) {
			if (cur_ws == UNSET)
				cur_ws = DEF_CUR_WS;
			if (ws[cur_ws].path)
				free(ws[cur_ws].path);

			ws[cur_ws].path = savestring(path_value, strlen(path_value));
		}

		else { /* Error changing directory */
			if (xargs.list_and_quit == 1) {
				fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME,
						path_value, strerror(errno));
				exit(EXIT_FAILURE);
			}

			_err('w', PRINT_PROMPT, "%s: %s: %s\n", PROGRAM_NAME,
				 path_value, strerror(errno));
		}

		if (path_exp)
			free(path_exp);
	}

	if ((flags & ALT_PROFILE) && alt_profile_value) {
		if (alt_profile)
			free(alt_profile);

		alt_profile = savestring(alt_profile_value,
								strlen(alt_profile_value));
	}
}

				/**
				 * #############################
				 * #           MAIN            #
				 * #############################
				 * */

int
main(int argc, char *argv[])
{
	/* #########################################################
	 * #            0) INITIAL CONDITIONS                      #
	 * #########################################################*/

	/* Though this program might perfectly work on other architectures,
	 * I just didn't test anything beyond x86 and ARM */
#if !defined __x86_64__ && !defined __i386__ && !defined __ARM_ARCH
		fprintf(stderr, "Unsupported CPU architecture\n");
		exit(EXIT_FAILURE);
#endif /* __x86_64__ */

	/* Though this program might perfectly work on other OSes, especially
	 * Unices, I just didn't make any test */
#if !defined __linux__  && !defined linux && !defined __linux \
	&& !defined __gnu_linux__ && !defined __FreeBSD__
		fprintf(stderr, _("%s: Unsupported operating system\n"),
				PROGRAM_NAME);
		exit(EXIT_FAILURE);
#endif

				/* #################################
				 * #     1) INITIALIZATION         #
				 * #  Basic config and variables   #
				 * #################################*/

	/* If running the program locally, that is, not from a path in PATH,
	 * remove the leading "./" to get the correct program invocation
	 * name */
	if (*argv[0] == '.' && *(argv[0] + 1) == '/')
		argv[0] += 2;

	/* Use the locale specified by the environment */
	setlocale(LC_ALL, "");

	/* If the locale isn't English, set 'unicode' to true to correctly
	 * list (sort and padding) filenames containing non-7bit ASCII chars
	 * like accents, tildes, umlauts, non-latin letters, and so on */
	if (strncmp(getenv("LANG"), "en", 2) != 0)
		unicode = 1;

	// Initialize gettext() for translations
	bindtextdomain("clifm", "/usr/share/locale");
	textdomain("clifm");

	/* Store external arguments to be able to rerun external_arguments()
	 * in case the user edits the config file, in which case the program
	 * must rerun init_config(), get_aliases(), get_prompt_cmds(), and
	 * then external_arguments() */
	argc_bk = argc;
	argv_bk = (char **)xnmalloc((size_t)argc, sizeof(char *));

	register int i = argc;

	while (--i >= 0)
		argv_bk[i] = savestring(argv[i], strlen(argv[i]));

	/* Register the function to be called at normal exit, either via
	 * exit() or main's return. The registered function will not be
	 * executed when abnormally exiting the program, e.g., via the KILL
	 * signal */
	atexit(free_stuff);

	/* Get user's home directory */
	user_home = get_user_home();

	if (!user_home || access(user_home, W_OK) == -1) {
		/* If no user's home, or if it's not writable, there won't be
		 * any config nor trash directory. These flags are used to
		 * prevent functions from trying to access any of these
		 * directories */
		home_ok = config_ok = trash_ok = 0;
		/* Print message: trash, bookmarks, command logs, commands
		 * history and program messages won't be stored */
		_err('e', PRINT_PROMPT, _("%s: Cannot access the home directory. "
			 "Trash, bookmarks, commands logs, and commands history are "
			 "disabled. Program messages and selected files won't be "
			 "persistent. Using default options\n"), PROGRAM_NAME);
	}
	else
		user_home_len = strlen(user_home);

	/* Get user name */
	user = get_user();

	if (!user) {
		user = (char *)xnmalloc(2, sizeof(char));
		user[0] = '?';
		user[1] = '\0';
		_err('e', PRINT_PROMPT, _("%s: Error getting username\n"),
			 PROGRAM_NAME);
	}

	if (geteuid() == 0)
		flags |= ROOT_USR;

	/* Running in a graphical environment? */
#if __linux__
	if (getenv("DISPLAY") != NULL
	&& strncmp(getenv("TERM"), "linux", 5) != 0)
#else
	if (getenv("DISPLAY") != NULL)
#endif
		flags |= GUI;

	/* Get paths from PATH environment variable. These paths will be
	 * used later by get_path_programs (for the autocomplete function)
	 * and get_cmd_path() */
	path_n = (size_t)get_path_env();

	/* Manage external arguments, but only if any: argc == 1 equates to
	 * no argument, since this '1' is just the program invokation name.
	 * External arguments will override initialization values
	 * (init_config) */

	ws = (struct ws_t *)xnmalloc(MAX_WS, sizeof(struct ws_t));
	i = MAX_WS;
	while (--i >= 0)
		ws[i].path = (char *)NULL;

	/* Set all external arguments flags to uninitialized state */
	unset_xargs();

	if (argc > 1)
		external_arguments(argc, argv);
		/* external_arguments is executed before init_config because, if
		 * specified (-P option), it sets the value of alt_profile, which
		 * is then checked by init_config */

	check_env_filter();

	/* Initialize program paths and files, set options from the config
	 * file, if they were not already set via external arguments, and
	 * load sel elements, if any. All these configurations are made
	 * per user basis */
	init_config();

	check_options();

	set_sel_file();

	create_tmp_files();

	cschemes_n = get_colorschemes();

	set_colors(usr_cscheme ? usr_cscheme : "default", 1);

	free(usr_cscheme);
	usr_cscheme = (char *)NULL;

	if (splash_screen) {
		splash();
		splash_screen = 0;
		CLEAR;
	}

	/* Last path is overriden by the -p option in the command line */
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

		getcwd(cwd, sizeof(cwd));

		if (!*cwd || strlen(cwd) == 0) {
			if (user_home)
				ws[cur_ws].path = savestring(user_home, strlen(user_home));

			else {
				if (access("/", R_OK|X_OK) == -1) {
					fprintf(stderr, "%s: /: %s\n", PROGRAM_NAME,
							strerror(errno));
					exit(EXIT_FAILURE);
				}

				else
					ws[cur_ws].path = savestring("/\0", 2);
			}
		}

		else
			ws[cur_ws].path = savestring(cwd, strlen(cwd));
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

	/* Set terminal window title */
	if (xargs.cwd_in_title == 0) {
		printf("\033]2;%s\007", PROGRAM_NAME);
		fflush(stdout);
	}
	else {
		char *tmp = (char *)NULL;
		if (ws[cur_ws].path[1] == 'h')
			tmp = home_tilde(ws[cur_ws].path);
		printf("\033]2;%s - %s\007", PROGRAM_NAME, tmp ? tmp : ws[cur_ws].path);
		fflush(stdout);
		if (tmp)
			free(tmp);
	}

	exec_profile();

	load_dirhist();

	add_to_dirhist(ws[cur_ws].path);

	/* Start listing as soon as possible to speed up startup time */
	if (cd_lists_on_the_fly && isatty(STDIN_FILENO))
		list_dir();

	create_kbinds_file();

	load_bookmarks();

	load_keybinds();

	load_jumpdb();
	if (!jump_db || xargs.path == 1)
		add_to_jumpdb(ws[cur_ws].path);

	load_actions();

	/* ##### READLINE ##### */
	initialize_readline();

	/* Copy the list of quote chars to a global variable to be used
	 * later by some of the program functions like split_str(),
	 * my_rl_quote(), is_quote_char(), and my_rl_dequote() */
	qc = savestring(rl_filename_quote_characters,
					strlen(rl_filename_quote_characters));

	check_file_size(DIRHIST_FILE, max_dirhist);

	/* Check whether we have a working shell */
	if (access(sys_shell, X_OK) == -1) {
		_err('w', PRINT_PROMPT, _("%s: %s: System shell not found. "
			 "Please edit the configuration file to specify a working "
			 "shell.\n"), PROGRAM_NAME, sys_shell);
	}

	get_aliases();

	/* Get the list of available applications in PATH to be used by my
	 * custom TAB-completion function */
	get_path_programs();

	get_prompt_cmds();

	get_sel_files();

	if (trash_ok) {
		trash_n = count_dir(TRASH_FILES_DIR);
		if (trash_n <= 2)
			trash_n = 0;
	}

	if (gethostname(hostname, sizeof(hostname)) == -1) {
		hostname[0] = '?';
		hostname[1] = '\0';

		_err('e', PRINT_PROMPT, _("%s: Error getting hostname\n"), PROGRAM_NAME);
	}

	init_shell();

	if (config_ok) {
		// Limit the log files size
		check_file_size(LOG_FILE, max_log);
		check_file_size(MSG_LOG_FILE, max_log);

		// Get history
		struct stat file_attrib;

		if (stat(HIST_FILE, &file_attrib) == 0
		&& file_attrib.st_size != 0) {
		/* If the size condition is not included, and in case of a zero
		 * size file, read_history() produces malloc errors */
			/* Recover history from the history file */
			read_history(HIST_FILE); /* This line adds more leaks to
			readline */
			/* Limit the size of the history file to max_hist lines */
			history_truncate_file(HIST_FILE, max_hist);
		}

		/* If the history file doesn't exist, create it */
		else {
			FILE *hist_fp = fopen(HIST_FILE, "w+");

			if (!hist_fp) {
				_err('w', PRINT_PROMPT, "%s: fopen: '%s': %s\n",
					 PROGRAM_NAME, HIST_FILE, strerror(errno));
			}

			else {
				/* To avoid malloc errors in read_history(), do not
				 * create an empty file */
				fputs("edit\n", hist_fp);
				/* There is no need to run read_history() here, since
				 * the history file is still empty */
				fclose(hist_fp);
			}
		}
	}

	/* Store history into an array to be able to manipulate it */
	get_history();

	/* Check if the 'file' command is available */
	if (!opener)
		file_cmd_check();

	get_profile_names();

	load_pinned_dir();

	set_env();


	//
	// Main program loop
	//

	/* This is the main structure of any basic shell
	1 - initialize
	2 - Grab user input
	3 - Parse user input
	4 - Execute command
	5 - goto step 2 and repeat till the program ends
	See https://brennan.io/2015/01/16/write-a-shell-in-c/
	*/

	// Infinite loop to keep the program running
	while (1) {
		char *input = prompt();

		if (!input)
			continue;

		char **cmd = parse_input_str(input);

		free(input);
		input = (char *)NULL;

		if (!cmd)
			continue;

		// Execute input string
		char **alias_cmd = check_for_alias(cmd);

		if (alias_cmd) {
			/* If an alias is found, check_for_alias() frees cmd
			 * and returns alias_cmd in its place to be executed by
			 * exec_cmd() */
			exec_cmd(alias_cmd);

			for (i = 0; alias_cmd[i]; i++)
				free(alias_cmd[i]);

			free(alias_cmd);
			alias_cmd = (char **)NULL;
		} else {
			exec_cmd(cmd);

			i = (int)args_n + 1;
			while (--i >= 0)
				free(cmd[i]);

			free(cmd);
			cmd = (char **)NULL;
		}
	}

	return exit_code; // Never reached
}
