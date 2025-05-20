/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2016-2025, L. Abramovich <leo.clifm@outlook.com>
 *
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
 */

/* aux.c -- functions that do not fit in any other file */

/* These four functions: from_hex, to_hex, url_encode, and
 * url_decode, are taken from "http://www.geekhideout.com/urlcode.shtml"
 * (under Public domain) and modified to comform to RFC 2395, as recommended
 * by the freedesktop trash specification.
 * All changes are licensed under GPL-2.0-or-later. */

#include "helpers.h"

#if defined(__sun) && defined(ST_BTIME)
# include <attr.h> /* getattrat, nvlist_lookup_uint64_array, nvlist_free */
#endif /* __sun && ST_BTIME */
#include <errno.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>
#include <readline/readline.h>

#include "aux.h"
#include "misc.h"
#include "checks.h" /* is_exec_cmd() */
#include "file_operations.h" /* open_file() */
#ifndef _NO_HIGHLIGHT
# include "highlight.h"
#endif /* !_NO_HIGHLIGHT */
#include "spawn.h" /* launch_execv() */

/* Open the file FILE with APP (if not NULL, or with the default associated
 * application otherwise). Returns the exit code returned by the opening
 * application. */
int
open_config_file(char *app, char *file)
{
	if (!file || !*file)
		return FUNC_FAILURE;

	if (app && *app) {
		char *cmd[] = {app, file, NULL};
		return launch_execv(cmd, FOREGROUND, E_NOFLAG);
	}

	open_in_foreground = 1;
	const int ret = open_file(file);
	open_in_foreground = 0;

	return ret;
}

/* Return the number of bytes in a UTF-8 sequence by inspecting only the
 * leading byte (C).
 * Taken from
 * https://stackoverflow.com/questions/22790900/get-length-of-multibyte-utf-8-sequence */
int
utf8_bytes(unsigned char c)
{
    c >>= 4;
    c &= 7;

    if (c == 4)
		return 2;

	return c - 3;
}

void
press_any_key_to_continue(const int init_newline)
{
	const int saved_errno = errno;
	HIDE_CURSOR;
	fprintf(stderr, _("%sPress any key to continue... "),
		init_newline == 1 ? "\n" : "");
	fflush(stderr);
	xgetchar();
	putchar('\n');
	UNHIDE_CURSOR;
	errno = saved_errno;
}

/* Print the file named FNAME, quoted if it contains a space.
 * A slash is appended if FNAME is a directory (ISDIR >= 1). */
void
print_file_name(char *fname, const int isdir)
{
	char *tmp_name = (char *)NULL;
	if (wc_xstrlen(fname) == 0)
		tmp_name = replace_invalid_chars(fname);

	char *name = tmp_name ? tmp_name : fname;

	if (detect_space(name) == 1) {
		if (strchr(name, '\''))
			printf("\"%s%s\"\n", name, isdir >= 1 ? "/" : "");
		else
			printf("'%s%s'\n", name, isdir >= 1 ? "/" : "");
	} else {
		fputs(name, stdout);
		puts(isdir >= 1 ? "/" : "");
	}

	free(tmp_name);
}

/* Return the value of the environment variable S, allocated via malloc if
 * ALLOC is 1, or just a pointer to the value if 0. */
char *
xgetenv(const char *s, const int alloc)
{
	if (!s || !*s)
		return (char *)NULL;

	char *p = getenv(s);
	if (p && *p)
		return (alloc == 1 ? strdup(p) : p);

	return (char *)NULL;
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

/* Generate a hash of the string STR (case sensitively if CASE_SENTITIVE is
 * set to 1).
 * Based on the sdbm algorithm (see http://www.cse.yorku.ca/~oz/hash.html),
 * released under the public-domain. */
size_t
hashme(const char *restrict str, const int case_sensitive)
{
	size_t hash = 0;

	/* Two while loops, so that we don't need to check CASE_SENSITIVE for
	 * each character in STR. */
	if (case_sensitive != 1) {
		while (*str) {
			hash = (size_t)TOUPPER(*str) + (hash << 6) + (hash << 16) - hash;
			str++;
		}
	} else {
		while (*str) {
			hash = (size_t)*str + (hash << 6) + (hash << 16) - hash;
			str++;
		}
	}

	return hash;
}

#if defined(__sun) && defined(ST_BTIME)
struct timespec
get_birthtime(const char *filename)
{
	struct timespec ts;
	ts.tv_sec = ts.tv_nsec = (time_t)-1;
	nvlist_t *response;

	if (getattrat(XAT_FDCWD, XATTR_VIEW_READWRITE, filename, &response) != 0)
		return ts;

	uint64_t *val;
	uint_t n;

	if (nvlist_lookup_uint64_array(response, A_CRTIME, &val, &n) == 0
	&& n >= 2 && val[0] <= LONG_MAX && val[1] < 1000000000 * 2 /* for leap seconds */) {
		ts.tv_sec = (time_t)val[0];
		ts.tv_nsec = (time_t)val[1];
	}

	nvlist_free(response);

	return ts;
}
#endif /* __sun && ST_BTIME */

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
	if (!p || !p[7]) {
		char *q = getenv("FZF_DEFAULT_OPTS");
		p = q ? strstr(q, "border-") : (char *)NULL;
		if (!p || !p[7])
			return;
	}

	switch (p[7]) {
	case 'b':
		if (p[8] == 'o' && p[9] == 't')
			fzf_preview_border_type = FZF_BORDER_BOTTOM;
		else if (p[8] == 'l')
			fzf_preview_border_type = FZF_BORDER_BLOCK;
		else
			fzf_preview_border_type = FZF_BORDER_BOLD;
		break;
	case 'd': fzf_preview_border_type = FZF_BORDER_DOUBLE; break;
	case 'h': fzf_preview_border_type = FZF_BORDER_HORIZ; break;
	case 'l': fzf_preview_border_type = FZF_BORDER_LEFT; break;
	case 'n': fzf_preview_border_type = FZF_BORDER_NONE; break;
	case 'r':
		fzf_preview_border_type = p[8] == 'o'
			? FZF_BORDER_ROUNDED : FZF_BORDER_RIGHT;
		break;
	case 's': fzf_preview_border_type = FZF_BORDER_SHARP; break;
	case 't':
		if (p[8] == 'o')
			fzf_preview_border_type = FZF_BORDER_TOP;
		else
			fzf_preview_border_type = FZF_BORDER_THINBLOCK;
		break;
	case 'v': fzf_preview_border_type = FZF_BORDER_VERT; break;
	default: break;
	}
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

/* Let's clear images printed on the terminal screen via ueberzug(1). */
void
clear_term_img(void)
{
	static char fu[PATH_MAX + 1] = "";
	char *p = (char *)NULL;

	if (!*fu) {
		p = getenv("CLIFM_FIFO_UEBERZUG");
		if (!p || !*p)
			return;

		xstrsncpy(fu, p, sizeof(fu));
	}

	ueberzug_clear(fu);
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
should_expand_eln(const char *text, char *cmd_name)
{
	char *l = cmd_name ? cmd_name : rl_line_buffer;

	/* Do not expand numbers starting with zero */
	if (!l || !*l || *text == '0' || !is_number(text))
		return 0;

	/* Exclude 'ws', 'mf', and 'st/sort' commands. */
	if ((*l == 'w' && l[1] == 's' && !l[2]) || (*l == 'm' && l[1] == 'f' && !l[2])
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
	if (!p && (p = find_digit(l)))
		t = *p;

	if (p) /* Either space of fused ELN. */
		*p = '\0';
	flags |= STATE_COMPLETING;
	const int ret = (is_internal_cmd(l, NO_FNAME_NUM, 1, 1)) ? 0 : 1;
	flags &= ~STATE_COMPLETING;
	if (p)
		*p = t;

	return ret;
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
 * The reformated filename is returned if actually reformated, in which case
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

void *
xmemrchr(const void *s, const int c, size_t n)
{
#ifdef HAVE_MEMRCHR
	return memrchr(s, c, n);
#else
	/* OpenBSD implementation of memrchr(), licensed MIT.
	 * Modified code is licensed GPL2+. */
	const unsigned char *cp;

	if (n != 0) {
		cp = (unsigned char *)s + n;
		do {
			if (*(--cp) == (unsigned char)c)
				return((void *)cp);
		} while (--n != 0);
	}

	return (NULL);
#endif /* HAVE_MEMRCHR */
}

/* Canonicalize/normalize the path SRC without resolving symlinks.
 * SRC is unescaped if necessary.
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
		char p[PATH_MAX + 1]; *p = '\0';
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
	switch (conf.bell_style) {
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
gen_date_suffix(const struct tm tm, const int human)
{
	char *suffix = NULL;

	if (human == 0) {
		suffix = xnmalloc(68, sizeof(char));
		snprintf(suffix, 67, "%04d%02d%02d%02d%02d%02d", tm.tm_year + 1900,
			tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	} else {
		char date[18] = "";
		const size_t bytes = strftime(date, sizeof(date), "%Y%m%d-%H:%M:%S", &tm);
		if (bytes > 0)
			suffix = savestring(date, bytes);
	}

	return suffix;
}

char *
gen_backup_file(const char *file, const int human)
{
	time_t rawtime = time(NULL);
	struct tm t;
	char *suffix = localtime_r(&rawtime, &t) ? gen_date_suffix(t, human) : NULL;
	if (!suffix) {
		xerror(_("kb: Cannot generate time suffix string for "
			"the backup file\n"));
		return NULL;
	}

	char backup[PATH_MAX + 1];
	snprintf(backup, sizeof(backup), "%s-%s", file, suffix);

	free(suffix);

	return strdup(backup);
}

/* Create directory DIR with permissions set to MODE (this latter modified
 * by a restrictive umask value: 077). */
int
xmkdir(const char *dir, const mode_t mode)
{
	mode_t old_mask = umask(0077); /* flawfinder: ignore */
	int ret = mkdirat(XAT_FDCWD, dir, mode);
	umask(old_mask); /* flawfinder: ignore */

	if (ret == -1)
		return FUNC_FAILURE;

	return FUNC_SUCCESS;
}

/* Same as readlinkat(3), but resolves relative symbolic links and NUL
 * terminates the returned string (BUF). */
ssize_t
xreadlink(const int fd, char *restrict path, char *restrict buf,
	const size_t bufsize)
{
	buf[0] = '\0';
	ssize_t buf_len = readlinkat(fd, path, buf, bufsize - 1);
	if (buf_len == -1)
		return (-1);

	buf[buf_len] = '\0';

	if (buf_len > 1 && buf[buf_len - 1] == '/')
		buf[buf_len - 1] = '\0';

	int rem_slash = 0;
	size_t path_len = strlen(path);
	if (path_len > 1 && path[path_len - 1] == '/') {
		path[path_len - 1] = '\0';
		rem_slash = 1;
	}

	char *p = (char *)NULL;
	if (*buf != '/' && (p = strrchr(path, '/'))) { /* Relative link */
		*p = '\0';

		char *temp = savestring(buf, strlen(buf));

		if (p == path) /* Root */
			snprintf(buf, bufsize, "/%s", temp);
		else
			snprintf(buf, bufsize, "%s/%s", path, temp);

		*p = '/';
		free(temp);
	}

	if (rem_slash == 1)
		path[path_len - 1] = '/';

	return buf_len;
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

static unsigned char hex_chars[256] = {
	['0'] = 0, ['1'] = 1, ['2'] = 2, ['3'] = 3, ['4'] = 4,
	['5'] = 5, ['6'] = 6, ['7'] = 7, ['8'] = 8, ['9'] = 9,
	['a'] = 10, ['b'] = 11, ['c'] = 12, ['d'] = 13, ['e'] = 14, ['f'] = 15,
	['A'] = 10, ['B'] = 11, ['C'] = 12, ['D'] = 13, ['E'] = 14, ['F'] = 15
};

static int
hex2int(const char *str)
{
	return ((hex_chars[(int)str[0]] * 16) + hex_chars[(int)str[1]]);
}

/* Disassemble the hex color HEX into attribute, R, G, and B values.
 * Based on https://mprog.wordpress.com/c/miscellaneous/convert-hexcolor-to-rgb-decimal */
int
get_rgb(char *hex, int *attr, int *r, int *g, int *b)
{
	if (!hex || !*hex)
		return (-1);

	if (*hex == '#') {
		if (!hex[1])
			return (-1);
		hex++;
	}

	char *h = hex;

	/* Convert 3-digits HEX to 6-digits */
	static char buf[9];
	if (h[0] && h[1] && h[2] && (!h[3] || h[3] == '-') ) {
		buf[0] = buf[1] = h[0];
		buf[2] = buf[3] = h[1];
		buf[4] = buf[5] = h[2];

		if (!h[3] || !h[4]) {
			buf[6] = '\0';
		} else {
			buf[6] = '-';
			buf[7] = h[4];
			buf[8] = '\0';
		}

		h = buf;
	}

	char tmp[3];
	tmp[2] = '\0';

	tmp[0] = h[0]; tmp[1] = h[1];
	*r = hex2int(tmp);

	tmp[0] = h[2]; tmp[1] = h[3];
	*g = hex2int(tmp);

	tmp[0] = h[4]; tmp[1] = h[5];
	*b = hex2int(tmp);

	*attr = -1; /* Attribute unset */
	if (h[6] == '-' && h[7] && h[7] >= '0'
	&& h[7] <= '9' && !h[8])
		*attr = h[7] - '0';

	if (xargs.no_bold == 1 && *attr == 1)
		*attr = -1;

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
	int attr = -1, r = 0, g = 0, b = 0;
	if (get_rgb(hex, &attr, &r, &g, &b) == -1)
		return (char *)NULL;

	if (attr == -1) /* No attribute */
		snprintf(tmp_color, sizeof(tmp_color), "38;2;%d;%d;%d", r, g, b);
	else
		snprintf(tmp_color, sizeof(tmp_color), "%d;38;2;%d;%d;%d", attr, r, g, b);
	return tmp_color;
}

/* Count files in the directory DIR, including self and parent. If POP is set
 * to 1, the function will just check if the directory is populated (it has at
 * least 3 files, including self and parent).
 * Returns -1 in case of error or an integer (0-FILESN_MAX) in case of success. */
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

	size_t c = 0;

	if (pop) {
		while (readdir(p)) {
			c++;
			if (c > 2)
				break;
		}
	} else {
		while (readdir(p))
			/* It is extremely unlikely for a directory to have more than
			 * SIZE_MAX entries. Even if it actually happens, c would wrap
			 * around to zero, leading thus to a wrong count (in the
			 * the absolute worst scenario). */
			c++;
	}

	closedir(p);
	return (filesn_t)(c > FILESN_MAX ? FILESN_MAX : c);
}

/* Get the path of the command CMD inspecting all paths in the PATH
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
		if (p && is_exec_cmd(p) == 1)
			cmd_path = p;
		return cmd_path;
	}

	if (*cmd == '/') {
		if (is_exec_cmd(cmd) == 1)
			cmd_path = savestring(cmd, strlen(cmd));
		return cmd_path;
	}

	cmd_path = xnmalloc(PATH_MAX + 2, sizeof(char));

	size_t i;
	for (i = 0; i < path_n; i++) { /* Check each path in PATH */
		if (!paths[i].path || !*paths[i].path)
			continue;

		/* Skip '.' (CWD) if running with secure environment */
		if ((xargs.secure_env == 1 || xargs.secure_env_full == 1)
		&& *paths[i].path == '.' && !paths[i].path[1])
			continue;

		snprintf(cmd_path, PATH_MAX + 1, "%s/%s", paths[i].path, cmd);
		if (is_exec_cmd(cmd_path) == 1)
			return cmd_path;
	}

	errno = ENOENT;
	free(cmd_path);
	return (char *)NULL;
}

/* Same thing as get_cmd_path(), but returns 1 in case of success or 0
 * otherwise instead of the absolute path to CMD. Unlike get_cmd_path(),
 * it does not allocate memory in the heap, so that it's faster. */
int
is_cmd_in_path(const char *cmd)
{
	errno = 0;
	if (!cmd || !*cmd) {
		errno = EINVAL;
		return 0;
	}

	if (*cmd == '~') {
		char *p = tilde_expand(cmd);
		const int ret = (p && is_exec_cmd(p) == 1);
		free(p);
		return ret;
	}

	if (*cmd == '/')
		return is_exec_cmd(cmd);

	char cmd_path[PATH_MAX + 1];
	const int is_secure_env =
		(xargs.secure_env == 1 || xargs.secure_env_full == 1);

	size_t i;
	for (i = 0; i < path_n; i++) { /* Check each path in PATH */
		if (!paths[i].path || !*paths[i].path)
			continue;

		/* Skip '.' (CWD) if running with secure environment */
		if (*paths[i].path == '.' && !paths[i].path[1] && is_secure_env == 1)
			continue;

		snprintf(cmd_path, sizeof(cmd_path), "%s/%s", paths[i].path, cmd);
		if (is_exec_cmd(cmd_path) == 1)
			return 1;
	}

	errno = ENOENT;
	return 0;
}

/* Convert SIZE to human readable form (at most 2 decimal places).
 * Returns a pointer to a string of at most MAX_UNIT_SIZE.
 * We use KiB, MiB, GiB... suffixes for powers of 1024
 * and kB, MB, GB... for power of 1000. */
char *
construct_human_size(const off_t size)
{
	static char str[MAX_HUMAN_SIZE + 2];

	if (size < 0)
		return UNKNOWN_STR;

	const float base = xargs.si == 1 ? 1000 : 1024;
	static float mult_factor = 0;
	if (mult_factor == 0)
		mult_factor = 1.0f / base;

	size_t n = 0;
	float s = (float)size;

	while (s >= base) {
		s = s * mult_factor; /* == (s = s / base), but faster */
		n++;
	}

	const int x = (int)s;
	/* If (s == 0 || s - (float)x == 0), then S has no reminder (zero)
	 * We don't want to print the reminder when it is zero.
	 *
	 * R: Ronnabyte, Q: Quettabyte. It's highly unlikely to have files of
	 * such huge sizes (and even less) in the near future. Even more, since
	 * our original value is off_t (whose maximum value is INT64_MAX,
	 * i.e. 2^63 - 1), we will never reach even a Zettabyte. */
	static const char *const u = "BKMGTPEZYRQ";
	snprintf(str, sizeof(str), "%.*f %c%s",
		(s == 0.00f || s - (float)x == 0.00f) ? 0 : 2,
		(double)s,
		(u[n] == 'K' && xargs.si == 1) ? 'k' : u[n],
		u[n] != 'B' ? (xargs.si == 1 ? "B" : "iB") : "");

	return str;
}

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

static char const *nums[102] = {
	"0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
	"10", "11", "12", "13", "14", "15", "16", "17", "18", "19",
	"20", "21", "22", "23", "24", "25", "26", "27", "28", "29",
	"30", "31", "32", "33", "34", "35", "36", "37", "38", "39",
	"40", "41", "42", "43", "44", "45", "46", "47", "48", "49",
	"50", "51", "52", "53", "54", "55", "56", "57", "58", "59",
	"60", "61", "62", "63", "64", "65", "66", "67", "68", "69",
	"70", "71", "72", "73", "74", "75", "76", "77", "78", "79",
	"80", "81", "82", "83", "84", "85", "86", "87", "88", "89",
	"90", "91", "92", "93", "94", "95", "96", "97", "98", "99",
	"100", NULL
	};

/* Transform an integer (N) into a string of chars.
 * This exists because most operating systems do not provide itoa(3).
 * It handles integer numbers between 0 and LLONG_MAX. */
const char *
xitoa(long long n)
{
	if (n >= 0 && n <= 100)
		return nums[n];

	static char buf[MAX_INT_STR] = {0};
	/* We start writing at the end of the buffer, which has MAX_INT_STR bytes
	 * (32), that is, from index 0 to 31. Index 31 is reserved for the NUL
	 * terminator, so that we must start writing at index 30. */
	long long i = MAX_INT_STR - 2;

	while (n > 0 && i > 0) {
		long long rem = n / 10;
		buf[i] = (char)('0' + (n - (rem * 10)));
		n = rem;
		--i;
	}

	return &buf[i + 1];
}

/* Convert the string S into a number in the range of valid ELN's
 * (1 - FILESN_MAX). Returns this value if valid or -1 in case of error. */
filesn_t
xatof(const char *s)
{
	if (!s[1] && *s > '0' && *s <= '9')
		return (filesn_t)(*s - '0');

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
	if (!s[1] && *s >= '0' && *s <= '9')
		return (*s - '0');

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

/* Return an URL-encoded version of STR, prefixed with "file://" if
 * FILE_URI is set to 1. */
char *
url_encode(char *str, const int file_uri)
{
	if (!str || !*str)
		return (char *)NULL;

	const size_t len = (strlen(str) * 3) + 1 + (file_uri == 1 ? 7 : 0);
	char *buf = xnmalloc(len, sizeof(char));
	/* The max lenght of our buffer is 3 times the length of STR (plus
	 * 7 (length of "file://") if FILE_URI is set to 1, plus 1 extra byte
	 * for the null byte terminator): each byte in STR will be, if
	 * encoded, %XX (3 chars). */

	/* Copies of STR and BUF pointers to be able to increase and/or decrease
	 * them without losing the original memory location. */
	const char *pstr = str;
	char *pbuf = buf;

	if (file_uri == 1) {
		buf[0] = 'f'; buf[1] = 'i'; buf[2] = 'l'; buf[3] = 'e';
		buf[4] = ':'; buf[5] = '/'; buf[6] = '/'; buf[7] = '\0';
		pbuf += 7;
	}

	for (; *pstr; pstr++) {
		if (IS_ALNUM(*pstr) || *pstr == '-' || *pstr == '_' || *pstr == '.'
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

/* Convert the octal string STR into an integer value.
 * Based on: https://www.geeksforgeeks.org/program-octal-decimal-conversion/
 * Used by decode_prompt() to make things like this work: \033[1;34m */
int
read_octal(const char *str)
{
	if (!str || !*str)
		return (-1);

	const int num = atoi(str);
	if (num == INT_MIN)
		return (-1);

	int dec_value = 0;
	int base = 1; /* Initializing base value to 1, i.e 8^0 */
	int temp = num;

	while (temp != 0) {
		/* Extract the last digit. */
		const int last_digit = temp % 10;
		temp = temp / 10;

		/* Multiply the last digit with appropriate
		 * base value and add it to dec_value. */
		dec_value += last_digit * base;
		base = base * 8;
	}

	return dec_value;
}
