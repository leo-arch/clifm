/* aux.c -- functions that do not fit in any other file */

/*
 * This file is part of CliFM
 *
 * Copyright (C) 2016-2023, L. Abramovich <leo.clifm@outlook.com>
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

/* These four functions: from_hex, to_hex, url_encode, and
 * url_decode, are taken from "http://www.geekhideout.com/urlcode.shtml"
 * (under Public domain) and modified to comform to RFC 2395, as recommended
 * by the freedesktop trash specification.
 * All changes are licensed under GPL-2.0-or-later. */

#include "helpers.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>
#include <readline/readline.h>

#include "aux.h"
#include "exec.h"
#include "misc.h"
#include "checks.h"
#ifndef _NO_HIGHLIGHT
# include "highlight.h"
#endif /* !_NO_HIGHLIGHT */

void
press_any_key_to_continue(const int init_newline)
{
	HIDE_CURSOR;
	fprintf(stderr, _("%sPress any key to continue... "),
		init_newline == 1 ? "\n" : "");
	fflush(stderr);
	xgetchar();
	putchar('\n');
	UNHIDE_CURSOR;
}

/* Print the regex error ERRCODE (returned by either regcomp(3) or regexec(3)),
 * whose compiled regular expression is REGEXP and original pattern PATTERN,
 * in the next prompt if PROMPT_ERR == 1, or immediately otherwise. */
void
xregerror(const char *cmd_name, const char *pattern, const int errcode,
	const regex_t regexp, const int prompt_err)
{
	const size_t err_len = regerror(errcode, &regexp, NULL, 0);
	char *buf = xnmalloc(err_len + 1, sizeof(char));
	regerror(errcode, &regexp, buf, err_len);

	if (prompt_err == 1)
		err('w', PRINT_PROMPT, "%s: %s: %s\n", cmd_name, pattern, buf);
	else
		xerror("%s: %s: %s\n", cmd_name, pattern, buf);

	free(buf);
}

#ifndef _NO_ICONS
/* Generate a hash of the string STR (case sensitively if CASE_SENTITIVE is
 * set to 1).
 * Based on the sdbm algorithm (see http://www.cse.yorku.ca/~oz/hash.html),
 * released under the public-domain. */
size_t
hashme(const char *str, const int case_sensitive) {
	size_t hash = 0;

	/* Two while loops, so that we don't need to check CASE_SENSITIVE for
	 * each character in STR */
	if (case_sensitive == 1) {
		while (*str) {
			hash = (size_t)*str	+ (hash << 6) + (hash << 16) - hash;
			str++;
		}
	} else {
		while (*str) {
			hash = (size_t)TOUPPER(*str) + (hash << 6) + (hash << 16) - hash;
			str++;
		}
	}

	return hash;
}
#endif /* !_NO_ICONS */

void
gen_time_str(char *buf, const size_t size, const time_t curtime)
{
	struct tm t;

	if (curtime >= 0 && localtime_r(&curtime, &t)) {
		*buf = '\0';
		strftime(buf, size, DEF_TIME_STYLE_LONG, &t);
		return;
	}

	*buf = '-';
	buf[1] = '\0';
}

/* Store the fzf preview window border style to later fix coordinates if
 * needed (set_fzf_env_vars() in tabcomp.c) */
void
set_fzf_preview_border_type(void)
{
#ifdef _NO_LIRA
	return;
#endif /* _NO_LIRA */

	fzf_preview_border_type = FZF_BORDER_ROUNDED; /* fzf default */

	char *p = (conf.fzftab_options && *conf.fzftab_options)
		? strstr(conf.fzftab_options, "border-") : (char *)NULL;
	if (!p || !*(p + 7)) {
		char *q = getenv("FZF_DEFAULT_OPTS");
		p = q ? strstr(q, "border-") : (char *)NULL;
		if (!p || !*(p + 7))
			return;
	}

	switch (*(p + 7)) {
	case 'b':
		if (*(p + 8) == 'o' && *(p + 9) == 't')
			fzf_preview_border_type = FZF_BORDER_BOTTOM;
		else
			fzf_preview_border_type = FZF_BORDER_BOLD;
		break;
	case 'd': fzf_preview_border_type = FZF_BORDER_DOUBLE; break;
	case 'h': fzf_preview_border_type = FZF_BORDER_HORIZ; break;
	case 'l': fzf_preview_border_type = FZF_BORDER_LEFT; break;
	case 'n': fzf_preview_border_type = FZF_BORDER_NONE; break;
	case 'r': fzf_preview_border_type = FZF_BORDER_ROUNDED; break;
	case 's': fzf_preview_border_type = FZF_BORDER_SHARP; break;
	case 't': fzf_preview_border_type = FZF_BORDER_TOP; break;
	case 'v': fzf_preview_border_type = FZF_BORDER_VERT; break;
	default: break;
	}
}

/* Remove images printed on the kitty terminal */
static void
kitty_clear(void)
{
//	printf("\033_Ga=d,t=d\033\\");
//	fflush(stdout);
//	char *cmd[] = { "printf", "\033_Ga=d\033\\", NULL };
	char *cmd[] = {"kitty", "icat", "--clear", "--silent",
		"--transfer-mode=stream", NULL};
	launch_execv(cmd, 0, 0);
}

/* Remove any image printed by ueberzug.
 * This function assumes:
 * 1) That ueberzug was launched using the json parser (default).
 * 2) That the pipe was created and exported as FIFO_UEBERZUG.
 * 3) That the identifier string was set to "clifm-preview". */
static void
ueberzug_clear(char *file)
{
	int fd = 0;
	FILE *fp = open_fwrite(file, &fd);
	if (!fp)
		return;

	fprintf(fp, "{\"action\": \"remove\", \"identifier\": \"clifm-preview\"}\n");
	fclose(fp);
}

/* Let's clear images printed on the terminal screen, either via
 * ueberzug(1) or the kitty image protocol. */
void
clear_term_img(void)
{
	char *p = getenv("CLIFM_FIFO_UEBERZUG");
	if (p) {
		ueberzug_clear(p);
		return;
	}

	if (!(flags & KITTY_TERM))
		return;

	/* This works, but it's too slow. Replace by ACP escape codes (both for
	 * displaying and removing the image). Take a look at how ranger does it. */

	/* CLIFM_KITTY_IMG is set by the clifmrun plugin to a temp file name,
	 * which is then created (empty) by the clifmimg plugin every time an
	 * image is displayed via the kitty protocol. */
	p = getenv("CLIFM_KITTY_IMG");
	struct stat a;
	if (p && stat(p, &a) != -1) {
		kitty_clear();
		unlinkat(XAT_FDCWD, p, 0);
	}
}

/* Return a pointer to the first digit found in STR or NULL if none is found. */
static char *
find_digit(char *str)
{
	if (!str || !*str)
		return (char *)NULL;

	while (*str) {
		if (*str >= '1' && *str <= '9')
			return str;
		str++;
	}

	return (char *)NULL;
}

/* Check whether a given command needs ELN's to be expanded/completed/suggested.
 * Returns 1 if yes or 0 if not. */
int
should_expand_eln(const char *text)
{
	char *l = rl_line_buffer;
	if (!l || !*l || !is_number(text))
		return 0;

	/* Exclude 'ws' and 'st/sort' commands. */
	if ((*l == 'w' && l[1] == 's' && !l[2])
	|| (*l == 's' && ((l[1] == 't' && !l[2]) || strcmp(l + 1, "ort") == 0)))
		return 0;

	const filesn_t a = xatof(text);
	if (a <= 0 || a > files) /* Only expand numbers matching ELN's */
		return 0;

	if (words_num == 1) { /* First word */
		if (file_info[a - 1].dir && conf.autocd == 0)
			return 0;
		if (file_info[a - 1].dir == 0 && conf.auto_open == 0)
			return 0;
	}

	char *p = strchr(l, ' ');
	char t = ' ';
	if (!p && (p = find_digit(l)) )
		t = *p;

	if (!p)
		return 1;

	*p = '\0';
	flags |= STATE_COMPLETING;
	if (is_internal_c(l) && !is_internal_f(l)) {
		*p = t;
		flags &= ~STATE_COMPLETING;
		return 0;
	}

	flags &= ~STATE_COMPLETING;
	*p = t;

	return 1;
}

#if !defined(_POSIX_C_SOURCE) || _POSIX_C_SOURCE >= 199309L
/* Sleep for MSEC milliseconds. */
/* Taken from https://stackoverflow.com/questions/1157209/is-there-an-alternative-sleep-function-in-c-to-milliseconds */
static int
msleep(const long msec)
{
	struct timespec ts;
	int res;

	if (msec < 0) {
		errno = EINVAL;
		return (-1);
	}

	ts.tv_sec = msec / 1000;
	ts.tv_nsec = (msec % 1000) * 1000000;

	do {
		res = nanosleep(&ts, &ts);
	} while (res && errno == EINTR);

	return res;
}
#endif /* _POSIX_C_SOURCE >= 199309L */

/* Convert the file named STR (as absolute path) into a more friendly format.
 * Change absolute paths into:
 * "./" if file is in CWD
 * "~" if file is in HOME
 * The reformated file name is returned if actually reformated, in which case
 * the returned value should be free'd by the caller.
 * Otherwise, a pointer to the original string is returned and must not be
 * free'd by the caller.
 * Usage example:
 * char *ret = abbreviate_file_name(str);
 * ...
 * if (ret && ret != str)
 *     free(ret); */
char *
abbreviate_file_name(char *str)
{
	if (!str || !*str)
		return (char *)NULL;

	char *name = (char *)NULL;
	const size_t len = strlen(str);
	const size_t wlen = (workspaces && workspaces[cur_ws].path)
		? strlen(workspaces[cur_ws].path) : 0;

	/* If STR is in CWD -> ./STR */
	if (workspaces && workspaces[cur_ws].path && wlen > 1 && len > wlen
	&& strncmp(str, workspaces[cur_ws].path, wlen) == 0
	&& *(str + wlen) == '/') {
		const size_t name_len = strlen(str + wlen + 1) + 3;
		name = xnmalloc(name_len, sizeof(char));
		snprintf(name, name_len, "./%s", str + wlen + 1);
		return name;
	}

	/* If STR is in HOME, reduce HOME to tilde (~) */
	int free_ptr = 0;
	char *tmp = home_tilde(str, &free_ptr);
	if (tmp && tmp != str) {
		name = savestring(tmp, strlen(tmp));
		if (free_ptr == 1)
			free(tmp);
		return name;
	}

	return str;
}

/* Return the current working directory from any of the following sources and
 * in this order:
 * 1 - Path of the current workspace (if CHECK_WORKSPACE is set to 1).
 * 2 - PWD environment variable (if not --secure-env-full)
 * 3 - getcwd(3), in which case the value is stored in BUF, whose size is BUFLEN.
 * */
char *
get_cwd(char *buf, const size_t buflen, const int check_workspace)
{
	if (check_workspace == 1 && workspaces && cur_ws >= 0
	&& workspaces[cur_ws].path)
		return workspaces[cur_ws].path;

	char *tmp = xargs.secure_env_full != 1 ? getenv("PWD") : (char *)NULL;
	if (tmp)
		return tmp;

	char *ret = getcwd(buf, buflen);
	tmp = (ret && *buf) ? buf : (char *)NULL;

	return tmp;
}

/* OpenBSD implementation of memrchr(), licensed MIT.
 * Modified code is licensed GPL2+. */
static const void *
xmemrchr(const void *s, const int c, size_t n)
{
	const unsigned char *cp;

	if (n != 0) {
		cp = (unsigned char *)s + n;
		do {
			if (*(--cp) == (unsigned char)c)
				return((void *)cp);
		} while (--n != 0);
	}

	return (NULL);
}

/* Canonicalize/normalize the path SRC without resolving symlinks.
 * SRC is deescaped if necessary.
 * ~/./.. are resolved.
 * The resolved path is returned and must be free'd by the caller. */
char *
normalize_path(char *src, const size_t src_len)
{
	if (!src || !*src)
		return (char *)NULL;

	/* Deescape SRC */
	char *tmp = (char *)NULL;
	const int is_escaped = *src == '\\';

	if (strchr(src, '\\')) {
		tmp = unescape_str(src, 0);
		if (!tmp) {
			xerror(_("%s: '%s': Error unescaping string\n"), PROGRAM_NAME, src);
			return (char *)NULL;
		}

		const size_t tlen = strlen(tmp);
		if (tlen > 0 && tmp[tlen - 1] == '/')
			tmp[tlen - 1] = '\0';

		xstrsncpy(src, tmp, tlen + 1);
		free(tmp);
		tmp = (char *)NULL;
	}

	/* Expand tilde */
	if (is_escaped == 0 && *src == '~') {
		tmp = tilde_expand(src);
		if (!tmp) {
			xerror(_("%s: '%s': Error expanding tilde\n"), PROGRAM_NAME, src);
			return (char *)NULL;
		}
		const size_t tlen = strlen(tmp);
		if (tlen > 0 && tmp[tlen - 1] == '/')
			tmp[tlen - 1] = '\0';

		if (!strstr(tmp, "/.."))
			return tmp;
	}

	char *s = tmp ? tmp : src;
	const size_t l = tmp ? strlen(tmp) : src_len;

	/* Resolve references to . and .. */
	char *res = (char *)NULL;
	size_t res_len = 0;

	if (l == 0 || *s != '/') {
		/* Relative path */
		char p[PATH_MAX]; *p = '\0';
		char *cwd = get_cwd(p, sizeof(p), 1);
		if (!cwd || !*cwd) {
			xerror(_("%s: Error getting current directory\n"), PROGRAM_NAME);
			free(tmp);
			return (char *)NULL;
		}

		const size_t pwd_len = strlen(cwd);
		if (pwd_len == 1 && *cwd == '/') {
			/* If CWD is root (/) do not copy anything. Just create a buffer
			 * big enough to hold "/dir", which will be appended next */
			res = xnmalloc(l + 2, sizeof(char));
			res_len = 0;
		} else {
			res = xnmalloc(pwd_len + 1 + l + 1, sizeof(char));
			memcpy(res, cwd, pwd_len);
			res_len = pwd_len;
		}
	} else {
		res = xnmalloc(l + 1, sizeof(char));
		res_len = 0;
	}

	const char *ptr;
	const char *end = &s[l];
	const char *next;

	for (ptr = s; ptr < end; ptr = next + 1) {
		size_t len;
		next = memchr(ptr, '/', (size_t)(end - ptr));
		if (!next)
			next = end;
		len = (size_t)(next - ptr);

		switch (len) {
		case 0: continue;

		case 1:
			if (*ptr == '.')
				continue;
			break;

		case 2:
			if (ptr[0] == '.' && ptr[1] == '.') {
				const char *slash = xmemrchr(res, '/', res_len);
				if (slash)
					res_len = (size_t)(slash - res);
				continue;
			}
			break;
		}

		res[res_len] = '/';
		res_len++;
		memcpy(&res[res_len], ptr, len);
		res_len += len;
	}

	if (res_len == 0) {
		res[res_len] = '/';
		res_len++;
	}

	res[res_len] = '\0';

	if (res_len > 1 && res[res_len - 1] == '/')
		res[res_len - 1] = '\0';

	if (s == tmp)
		free(s);

	return res;
}

/* Ring the terminal bell according to any of the following modes:
 * AUDIBLE, FLASH, or VISIBLE. */
void
rl_ring_bell(void)
{
	switch (bell) {
	case BELL_AUDIBLE:
		RING_BELL; fflush(stderr); return;

#if !defined(_POSIX_C_SOURCE) || _POSIX_C_SOURCE >= 199309L
	case BELL_FLASH:
		SET_RVIDEO;
		fflush(stderr);
		msleep(VISIBLE_BELL_DELAY);
		UNSET_RVIDEO;
		fflush(stderr);
		return;

# ifdef READLINE_HAS_ACTIVATE_MARK /* Readline >= 8.1 */
	case BELL_VISIBLE: {
		int point = rl_point;

		rl_mark = 0;
		char *p = get_last_chr(rl_line_buffer, ' ', rl_point);
		if (p && p != rl_line_buffer && *(++p))
			rl_mark = (int)(p - rl_line_buffer);

		if (rl_end > 1 && rl_line_buffer[rl_end - 1] == ' ')
			rl_point--;

		rl_activate_mark();
		rl_redisplay();
		msleep(VISIBLE_BELL_DELAY);
		rl_deactivate_mark();

#  ifndef _NO_HIGHLIGHT
		if (conf.highlight && !wrong_cmd) {
			rl_point = rl_mark;
			recolorize_line();
		}
#  endif /* !_NO_HIGHLIGHT */
		rl_point = point;
		return;
	}
# endif /* READLINE_HAS_ACTIVATE_MARK */
#endif /* _POSIX_C_SOURCE >= 199309L */

	case BELL_NONE: /* fallthrough */
	default: return;
	}
}

/* Generate a time string (based on the struct tm TM) with this form:
 * YYYYMMDDHHMMSS.
 * This function is used mostly by the trash function to generate unique
 * suffixes for trashed files. */
char *
gen_date_suffix(const struct tm tm)
{
	char *suffix = xnmalloc(68, sizeof(char));
	snprintf(suffix, 67, "%04d%02d%02d%02d%02d%02d", tm.tm_year + 1900,
	    tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

	return suffix;
}

/* Create directory DIR with permissions set to MODE (this latter modified
 * by a restrictive umask value: 077). */
int
xmkdir(char *dir, const mode_t mode)
{
	mode_t old_mask = umask(0077);
	int ret = mkdirat(XAT_FDCWD, dir, mode);
	umask(old_mask);

	if (ret == -1)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

/* Open a file for read only. Return a file stream associated to the file
 * named NAME and updates FD to hold the corresponding file descriptor.
 * NOTE: As stated here, file streams are to be preferred over file descriptors:
 * https://www.gnu.org/software/libc/manual/html_node/Streams-and-File-Descriptors.html */
FILE *
open_fread(const char *name, int *fd)
{
	errno = 0;
	if (!name || !*name) {
		errno = EINVAL;
		return (FILE *)NULL;
	}

	*fd = open(name, O_RDONLY);
	if (*fd == -1)
		return (FILE *)NULL;

	FILE *fp = fdopen(*fd, "r");
	if (!fp) {
		close(*fd);
		return (FILE *)NULL;
	}

	return fp;
}

/* Create a file for writing (truncating it to zero length if it already exists,
 * and with permissions 600).
 * Return a file stream associated to the file named NAME and update FD to hold
 * the corresponding file descriptor. */
FILE *
open_fwrite(const char *name, int *fd)
{
	errno = 0;
	if (!name || !*name) {
		errno = EINVAL;
		return (FILE *)NULL;
	}

	*fd = open(name, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	if (*fd == -1)
		return (FILE *)NULL;

	FILE *fp = fdopen(*fd, "w");
	if (!fp) {
		close(*fd);
		return (FILE *)NULL;
	}

	return fp;
}

/* Open a file for appending (using permissions 600).
 * Return a file stream associated to the file named NAME or NULL in case of
 * error. */
FILE *
open_fappend(const char *name)
{
	errno = 0;
	if (!name || !*name) {
		errno = EINVAL;
		return (FILE *)NULL;
	}

	int fd = open(name, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
	if (fd == -1)
		return (FILE *)NULL;

	FILE *fp = fdopen(fd, "a");
	if (!fp) {
		close(fd);
		return (FILE *)NULL;
	}

	return fp;
}

/* Transform S_IFXXX (MODE) into the corresponding DT_XXX constant */
inline mode_t
get_dt(const mode_t mode)
{
	switch (mode & S_IFMT) {
	case S_IFREG:  return DT_REG;
	case S_IFDIR:  return DT_DIR;
	case S_IFLNK:  return DT_LNK;
	case S_IFIFO:  return DT_FIFO;
	case S_IFSOCK: return DT_SOCK;
	case S_IFBLK:  return DT_BLK;
	case S_IFCHR:  return DT_CHR;
#ifndef _BE_POSIX
# ifdef SOLARIS_DOORS
	case S_IFDOOR: return DT_DOOR;
	case S_IFPORT: return DT_PORT;
# endif /* SOLARIS_DOORS */
# ifdef S_IFWHT
	case S_IFWHT:  return DT_WHT;
# endif
# ifdef S_ARCH1
	case S_ARCH1:  return DT_ARCH1;
	case S_ARCH2:  return DT_ARCH2;
# endif
#endif /* !_BE_POSIX */
	default:       return DT_UNKNOWN;
	}
}

static int
hex2int(const char *str)
{
	int i, n[2] = { 0 };
	for (i = 1; i >= 0; i--) {
		if (str[i] >= '0' && str[i] <= '9') {
			n[i] = str[i] - '0';
		} else {
			switch (str[i]) {
			case 'A': /* fallthrough */
			case 'a': n[i] = 10; break;
			case 'B': /* fallthrough */
			case 'b': n[i] = 11; break;
			case 'C': /* fallthrough */
			case 'c': n[i] = 12; break;
			case 'D': /* fallthrough */
			case 'd': n[i] = 13; break;
			case 'E': /* fallthrough */
			case 'e': n[i] = 14; break;
			case 'F': /* fallthrough */
			case 'f': n[i] = 15; break;
			default: break;
			}
		}
	}

	return ((n[0] * 16) + n[1]);
}

/* Disassemble the hex color HEX into attribute, R, G, and B values.
 * Based on https://mprog.wordpress.com/c/miscellaneous/convert-hexcolor-to-rgb-decimal */
int
get_rgb(char *hex, int *attr, int *r, int *g, int *b)
{
	if (!hex || !*hex)
		return (-1);

	if (*hex == '#') {
		if (!*(hex + 1))
			return (-1);
		hex++;
	}

	char tmp[3];
	tmp[2] = '\0';

	tmp[0] = *hex; tmp[1] = *(hex + 1);
	*r = hex2int(tmp);

	tmp[0] = *(hex + 2); tmp[1] = *(hex + 3);
	*g = hex2int(tmp);

	tmp[0] = *(hex + 4); tmp[1] = *(hex + 5);
	*b = hex2int(tmp);

	*attr = 0;
	if (*(hex + 6) == '-' && *(hex + 7) && *(hex + 7) >= '1'
	&& *(hex + 7) <= '9' && !*(hex + 8))
		*attr = *(hex + 7) - '0';

	if (xargs.no_bold == 1 && *attr == 1)
		*attr = 0;

	return 0;
}

/* Convert hex color HEX into RGB format (as a color code)
 * One color attribute can be added to the hex color as follows:
 * RRGGBB-[1-9], where 1-9 could be:
 * 1: Bold or increased intensity
 * 2: Faint, decreased intensity or dim
 * 3: Italic (Not widely supported)
 * 4: Underline
 * 5: Slow blink
 * 6: Rapid blink
 * 7: Reverse video or invert
 * 8: Conceal or hide (Not widely supported)
 * 9: Crossed-out or strike
 *
 * Example: ffaff00-4 -> 4;38;2;250;255;0
 *
 * At this point we know HEX is a valid hex color code (see is_hex_color() in
 * colors.c).
 * If using this function outside CliFM, make sure to validate HEX yourself
 *
 */
char *
hex2rgb(char *hex)
{
	int attr = 0, r = 0, g = 0, b = 0;
	if (get_rgb(hex, &attr, &r, &g, &b) == -1)
		return (char *)NULL;

	snprintf(tmp_color, sizeof(tmp_color), "%d;38;2;%d;%d;%d", attr, r, g, b);
	return tmp_color;
}

/* Count files in the directory DIR, including self and parent. If POP is set
 * to 1, the function will just check if the directory is populated (it has at
 * least 3 files, including self and parent).
 * Returns -1 in case of error or an integer (0-INTMAX_MAX) in case of success. */
filesn_t
count_dir(const char *dir, const int pop)
{
	if (!dir)
		return (-1);

	DIR *p;

	if ((p = opendir(dir)) == NULL) {
		if (errno == ENOMEM)
			exit(ENOMEM);
		return (-1);
	}
/* Let's set the O_NOATIME flag (Linux only) to prevent updating atime, avoiding
 * thus unnecessary disk writes. */
/*#ifdef O_NOATIME
	int fd = dirfd(p);
	if (fd != -1) {
		int fflags = fcntl(fd, F_GETFL);
		if (fflags != -1) {
			fflags |= O_NOATIME;
			fcntl(fd, F_SETFL, fflags);
		}
	}
#endif // O_NOATIME */

	filesn_t c = 0;

	while (readdir(p)) {
		if (c > FILESN_MAX - 1)
			break;

		c++;
		if (pop && c > 2)
			break;
	}

	closedir(p);
	return c;
}

/* Get the path of a the command CMD inspecting all paths in the PATH
 * environment variable (it basically does the same as which(1)).
 * Returns the appropriate path or NULL in case of error (in which case
 * errno is set to either EINVAL or ENOENT). */
char *
get_cmd_path(const char *cmd)
{
	errno = 0;
	if (!cmd || !*cmd) {
		errno = EINVAL;
		return (char *)NULL;
	}

	char *cmd_path = (char *)NULL;

	if (*cmd == '~') {
		char *p = tilde_expand(cmd);
		if (p && access(p, X_OK) == 0)
			cmd_path = savestring(p, strlen(p));
		return cmd_path;
	}

	if (*cmd == '/') {
		if (access(cmd, X_OK) == 0)
			cmd_path = savestring(cmd, strlen(cmd));
		return cmd_path;
	}

	cmd_path = xnmalloc(PATH_MAX + 1, sizeof(char));

	size_t i;
	for (i = 0; i < path_n; i++) { /* Check each path in PATH */
		if (!paths[i].path || !*paths[i].path)
			continue;

		/* Skip '.' (CWD) if running with secure environment */
		if ((xargs.secure_env == 1 || xargs.secure_env_full == 1)
		&& *paths[i].path == '.' && !paths[i].path[1])
			continue;

		snprintf(cmd_path, PATH_MAX, "%s/%s", paths[i].path, cmd); /* NOLINT */
		if (access(cmd_path, X_OK) == 0)
			return cmd_path;
	}

	errno = ENOENT;
	free(cmd_path);
	return (char *)NULL;
}

/* Convert SIZE to human readable form (at most 2 decimal places).
 * Returns a pointer to a string of at most MAX_UNIT_SIZE.
 * We follow here the du(1) notation: K, M, G... for powers of 1024
 * and KB, MB, KB... for power of 1000. */
char *
construct_human_size(const off_t size)
{
	/* MAX_UNIT_SIZE == 10 == "1023.99YB\0" */
	static char str[MAX_UNIT_SIZE];

	const float base = xargs.si == 1 ? 1000 : 1024;
	static float mult_factor = 0;
	if (mult_factor == 0)
		mult_factor = 1.0f / base;

	size_t n = 0;
	float s = (float)size;

	while (s >= base) {
		s = s * mult_factor; /* == (s = s / base), but faster */
		++n;
	}

	int x = (int)s;
	/* If (s == 0 || s - (float)x) == 0, then S has no reminder (zero)
	 * We don't want to print the reminder when it is zero.
	 *
	 * R: Ronnabyte, Q: Quettabyte. It's highly unlikely to have files of
	 * such huge sizes (and even less) in the near future, but anyway... */
	static const char *const u = "BKMGTPEZYRQ";
	snprintf(str, MAX_UNIT_SIZE, "%.*f%c%c",
		(s == 0.00f || s - (float)x == 0.00f) ? 0 : 2,
		(double)s,
		u[n],
		(u[n] != 'B' && xargs.si == 1) ? 'B' : 0);

	return str;
}

#ifdef USE_XDU
struct hlink_t {
	dev_t dev;
	ino_t ino;
};

static struct hlink_t *xdu_hardlinks = {0};
static size_t xdu_hardlink_n = 0;

static inline int
check_xdu_hardlinks(const dev_t dev, const ino_t ino)
{
	if (!xdu_hardlinks || xdu_hardlink_n == 0)
		return 0;

	size_t i;
	for (i = 0; i < xdu_hardlink_n; i++) {
		if (dev == xdu_hardlinks[i].dev && ino == xdu_hardlinks[i].ino)
			return 1;
	}

	return 0;
}

static inline void
add_xdu_hardlink(const dev_t dev, const ino_t ino)
{
	xdu_hardlinks = xnrealloc(xdu_hardlinks, xdu_hardlink_n + 1,
		sizeof(struct hlink_t));

	xdu_hardlinks[xdu_hardlink_n].dev = dev;
	xdu_hardlinks[xdu_hardlink_n].ino = ino;
	xdu_hardlink_n++;
}

static inline void
free_xdu_hardlinks(void)
{
	free(xdu_hardlinks);
	xdu_hardlinks = (struct hlink_t *)NULL;
	xdu_hardlink_n = 0;
}

/* The st_size member of a stat struct is meaningful only:
 * 1. When computing disk usage (not apparent sizes).
 * 2. If apparent sizes, only for symlinks and regular files.
 * NOTE: Here we add shared memory object and typed memory object just to
 * match the check made by du(1). These objects are not implmented on most
 * systems, but this might change in the future. */
static inline int
usable_st_size(struct stat const *s)
{
	return (conf.apparent_size != 1 || S_ISLNK(s->st_mode)
		|| S_ISREG(s->st_mode) || S_TYPEISSHM(s) || S_TYPEISTMO(s));
}

/* Read no more than this many directory entries at a time. Without this
 * limit, processing a directory with 4,000,000 entries requires ~1GiB of
 * memory, and handling 64M entries would require 16GiB of memory. */
//#define MAX_READDIR_ENTRIES 100000

/* Hand-made implementation of du(1) providing only those features required
 * by clifm.
 *
 * Returns the full size of the directory DIR in bytes, computing apparent
 * sizes, if conf.apparent_size is set to 1, or disk usage otherwise. In case
 * of error, STATUS is set to the appropiate error code (errno): in this case,
 * the return value may be zero, if DIR couldn't be read for some reason, or
 * a positive integer representing the size of the entries that have been
 * actually processed.
 *
 * FIRST_LEVEL must be always 1 when calling this function (this value will
 * be zero whenever the function calls itself recursively).
 *
 * NOTE: Old versions of du (at least up to 8.30) count the size of directories
 * themselves when computing apparent sizes. At least since 9.3, this is not
 * the case anymore (See
 * https://git.savannah.gnu.org/gitweb/?p=coreutils.git;a=commit;h=110bcd28386b1f47a4cd876098acb708fdcbbb25).
 * As stated in 'info du': "Apparent sizes are meaningful only for regular
 * files and symbolic links. Other file types do not contribute to apparent
 * size." We follow here the last behavior. */
off_t
dir_size(char *dir, const int first_level, int *status)
{
	if (!dir || !*dir) {
		*status = ENOENT;
		return 0;
	}

	struct stat a;
//	const dev_t base_dev = lstat(dir, &a) != -1 ? a.st_dev : 0;
	off_t size = 0;
	DIR *p;

	if ((p = opendir(dir)) == NULL) {
		*status = errno;
		return 0;
	}

	/* Compute the size of the base directory itself */
	if (conf.apparent_size != 1 && first_level == 1
	&& stat(dir, &a) != -1)
		size += (a.st_blocks * S_BLKSIZE);

	struct dirent *ent;

	while ((ent = readdir(p)) != NULL) {
		if (SELFORPARENT(ent->d_name))
			continue;

		char buf[PATH_MAX + 1];
		snprintf(buf, sizeof(buf), "%s/%s", dir, ent->d_name);

		if (lstat(buf, &a) == -1) {
			*status = errno;
			continue;
		}

/*		if (base_dev > 0 && base_dev != a.st_dev) {
			*status = EXDEV; // File is on a different file system.
			continue;
		} */

		if (S_ISDIR(a.st_mode)) {
			/* Even if a subdirectory is unreadable or we can't chdir into
			 * it, do let its size contribute to the total (provided we're
			 * not computing apparent sizes). */
			if (conf.apparent_size != 1)
				size += (a.st_blocks * S_BLKSIZE);

			if (user.uid != 0
			&& check_file_access(a.st_mode, a.st_uid, a.st_gid) == 0)
				*status = EACCES;
			else
				size += dir_size(buf, 0, status);

			continue;
		}

		if (usable_st_size(&a) == 0)
			continue;

		if (a.st_nlink > 1) {
			if (check_xdu_hardlinks(a.st_dev, a.st_ino) == 1)
				continue;
			else
				add_xdu_hardlink(a.st_dev, a.st_ino);
		}

		size += conf.apparent_size == 1 ? a.st_size
			: (a.st_blocks * S_BLKSIZE);
	}

	closedir(p);

	if (first_level == 1)
		free_xdu_hardlinks();

	return size;
}
#else
/* Return the full size of the directory DIR using du(1).
 * The size is reported in bytes if SIZE_IN_BYTES is set to 1.
 * Otherwise, human format is used.
 * STATUS is updated to the command exit code. */
off_t
dir_size(char *dir, const int size_in_bytes, int *status)
{
	if (!dir || !*dir)
		return (-1);

	char file[PATH_MAX];
#if !defined(__OpenBSD__)
	snprintf(file, sizeof(file), "%s/.duXXXXXX", xargs.stealth_mode == 1
		? P_tmpdir : tmp_dir); /* NOLINT */
#else
	snprintf(file, sizeof(file), "%s/.duXXXXXXXXXX", xargs.stealth_mode == 1
		? P_tmpdir : tmp_dir); /* NOLINT */
#endif /* !__OpenBSD__ */

	int fd = mkstemp(file);
	if (fd == -1)
		return (-1);

	int stdout_bk = dup(STDOUT_FILENO); /* Save original stdout */
	if (stdout_bk == -1) {
		close(fd);
		unlink(file);
		return (-1);
	}

	/* Redirect stdout to the desired file */
	dup2(fd, STDOUT_FILENO);
	close(fd);

	if (bin_flags & (GNU_DU_BIN_DU | GNU_DU_BIN_GDU)) {
		char *block_size = (char *)NULL;

		if (size_in_bytes == 1) {
			block_size = "--block-size=1";
		} else {
			if (xargs.si == 1)
				block_size = "--block-size=KB";
			else
				block_size = "--block-size=K";
		}

		char *bin = (bin_flags & GNU_DU_BIN_DU) ? "du" : "gdu";
		if (conf.apparent_size != 1) {
			char *cmd[] = {bin, "-s", block_size, "--", dir, NULL};
			*status = launch_execv(cmd, FOREGROUND, E_NOSTDERR);
		} else {
			char *cmd[] = {bin, "-s", "--apparent-size", block_size,
				"--", dir, NULL};
			*status = launch_execv(cmd, FOREGROUND, E_NOSTDERR);
		}
	} else {
		char *cmd[] = {"du", "-ks", "--", dir, NULL};
		*status = launch_execv(cmd, FOREGROUND, E_NOSTDERR);
	}

	dup2(stdout_bk, STDOUT_FILENO); /* Restore original stdout */
	close(stdout_bk);

	FILE *fp = open_fread(file, &fd);
	if (!fp) {
		unlink(file);
		return (-1);
	}

	off_t retval = -1;
	/* We only need here the first field of the line, which is a file
	 * size and usually takes only a few digits: since a yottabyte takes 26
	 * digits, 32 is more than enough. */
	char line[32]; *line = '\0';
	if (fgets(line, (int)sizeof(line), fp) == NULL)
		goto END;

	char *p = strchr(line, '\t');
	if (p && p != line) {
		*p = '\0';
		retval = (off_t)atoll(line);
	}

END:
	fclose(fp);
	unlink(file);
	return retval;
}
#endif /* USE_XDU */

/* Return the file type of the file pointed to by LINK, or -1 in case of
 * error. Possible return values:
S_IFDIR: 40000 (octal) / 16384 (decimal, integer)
S_IFREG: 100000 / 32768
S_IFLNK: 120000 / 40960
S_IFSOCK: 140000 / 49152
S_IFBLK: 60000 / 24576
S_IFCHR: 20000 / 8192
S_IFIFO: 10000 / 4096
 * See the inode manpage. */
int
get_link_ref(const char *link)
{
	errno = 0;
	if (!link || !*link) {
		errno = EINVAL;
		return (-1);
	}

	struct stat a;
	if (stat(link, &a) == -1)
		return (-1);

	return (int)(a.st_mode & S_IFMT);
}

/* Transform an integer (N) into a string of chars.
 * This exists because some operating systems do not support itoa(3). */
char *
xitoa(long long n)
{
	if (!n)
		return "0";

	static char buf[32] = {0};
	long long i = 30;

	while (n && i) {
		long long rem = n / 10;
		buf[i] = (char)('0' + (n - (rem * 10)));
		n = rem;
		--i;
	}

	return &buf[++i];
}

/* Convert the string S into a number in the range of valid ELN's
 * (1 - FILESN_MAX). Returns this value if valid or -1 in case of error. */
filesn_t
xatof(const char *s)
{
	errno = 0;
	const long long ret = strtoll(s, NULL, 10);

#if FILESN_MAX < LLONG_MAX
	/* 1 - FILESN_MAX */
	if (ret < 1 || ret > FILESN_MAX) {
#elif FILESN_MAX > LLONG_MAX
	/* 1 - LLONG_MAX-1 */
	if (ret < 1 || ret == LLONG_MAX) {
#else
	/* 1 - FILESN_MAX-1 */
	if (ret < 1 || ret >= FILESN_MAX) {
#endif /* FILESN_MAX < LLONG_MAX */
		errno = ERANGE;
		return (-1);
	}

	return (filesn_t)ret;
}

/* A secure atoi implementation to prevent integer under- and over-flow.
 * Returns the corresponding integer, if valid, or INT_MIN if invalid,
 * setting errno to ERANGE. */
int
xatoi(const char *s)
{
	errno = 0;
	const long ret = strtol(s, NULL, 10);

	if (ret < INT_MIN || ret > INT_MAX) {
		errno = ERANGE;
		return INT_MIN;
	}

	return (int)ret;
}

/* Unlike getchar(3) this function does not wait for newline ('\n').
https://stackoverflow.com/questions/12710582/how-can-i-capture-a-key-stroke-immediately-in-linux 
*/
char
xgetchar(void)
{
	struct termios oldt, newt;
	char c = 0;

	if (tcgetattr(STDIN_FILENO, &oldt) == -1) {
		xerror("%s: tcgetattr: %s\n", PROGRAM_NAME, strerror(errno));
		return 0;
	}
	newt = oldt;
	newt.c_lflag &= (tcflag_t)~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	c = (char)getchar(); /* flawfinder: ignore */
	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

	return c;
}

/* Converts a hex char to its integer value. */
char
from_hex(char c)
{
	return (char)(isdigit(c) ? c - '0' : tolower(c) - 'a' + 10);
}

/* Converts an integer value to its hex form. */
static char
to_hex(char c)
{
	static const char hex[] = "0123456789ABCDEF";
	return hex[c & 15];
}

/* Returns a url-encoded version of STR. */
char *
url_encode(char *str)
{
	if (!str || !*str)
		return (char *)NULL;

	char *buf = xnmalloc((strlen(str) * 3) + 1, sizeof(char));
	/* The max lenght of our buffer is 3 times the length of STR plus
	 * 1 extra byte for the null byte terminator: each char in STR will
	 * be, if encoded, %XX (3 chars) */

	/* Copies of STR and BUF pointers to be able to increase and/or decrease
	 * them without loosing the original memory location */
	char *pstr = str, *pbuf = buf;

	for (; *pstr; pstr++) {
#if defined(__CYGWIN__)
		/* GCC on CYGWIN complains about subscript having type 'char' */
		if (isalnum((unsigned char)*pstr) || *pstr == '-' || *pstr == '_'
		|| *pstr == '.'
#else
		if (isalnum(*pstr) || *pstr == '-' || *pstr == '_' || *pstr == '.'
#endif /* __CYGWIN__ */
		|| *pstr == '~' || *pstr == '/') {
			/* Do not encode any of the above chars */
			*pbuf = *pstr;
			pbuf++;
		} else {
			/* Encode char to URL format. Example: space char to %20 */
			*pbuf = '%';
			pbuf++;
			*pbuf = to_hex(*pstr >> 4);
			pbuf++;
			*pbuf = to_hex(*pstr & 15);
			pbuf++;
		}
	}

	*pbuf = '\0';
	return buf;
}

/* Returns a url-decoded version of STR. */
char *
url_decode(char *str)
{
	if (!str || !*str)
		return (char *)NULL;

	char *buf = xnmalloc(strlen(str) + 1, sizeof(char));
	/* The decoded string will be at most as long as the encoded string */

	char *pstr = str, *pbuf = buf;

	for (; *pstr; pstr++) {
		if (*pstr == '%') {
			if (pstr[1] && pstr[2]) {
				/* Decode URL code. Example: %20 to space char */
				/* Left shift and bitwise OR operations */
				*pbuf = (char)(from_hex(pstr[1]) << 4 | from_hex(pstr[2]));
				pbuf++;
				pstr += 2;
			}
		} else {
			*pbuf = *pstr;
			pbuf++;
		}
	}

	*pbuf = '\0';
	return buf;
}

/* Convert octal string into integer.
 * Based on: https://www.geeksforgeeks.org/program-octal-decimal-conversion/
 * Used by decode_prompt() to make things like this work: \033[1;34m */
int
read_octal(char *str)
{
	if (!str || !*str)
		return (-1);

	int n = atoi(str);
	if (n == INT_MIN)
		return (-1);

	int num = n;
	int dec_value = 0;

	/* Initializing base value to 1, i.e 8^0 */
	int base = 1;

	int temp = num;
	while (temp) {
		/* Extracting last digit */
		int last_digit = temp % 10;
		temp = temp / 10;

		/* Multiplying last digit with appropriate
		 * base value and adding it to dec_value */
		dec_value += last_digit * base;
		base = base * 8;
	}

	return dec_value;
}
