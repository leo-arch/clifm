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

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <limits.h>
#include <readline/readline.h>

#include "aux.h"
#include "exec.h"
#include "misc.h"
#include "checks.h"
#ifndef _NO_HIGHLIGHT
# include "highlight.h"
#endif

void
gen_time_str(char *buf, const size_t size, const time_t _time)
{
	if (_time >= 0) {
		struct tm tm;
		localtime_r(&_time, &tm);
		strftime(buf, size, DEF_TIME_STYLE_LONG, &tm);
	} else {
		*buf = '-';
		buf[1] = '\0';
	}
}

/* Store the fzf preview window border style to later fix coordinates if
 * needed (set_fzf_env_vars() in tabcomp.c) */
void
set_fzf_preview_border_type(void)
{
	fzf_preview_border_type = FZF_BORDER_ROUNDED; /* fzf default */

	char *p = (conf.fzftab_options && *conf.fzftab_options)
		? strstr(conf.fzftab_options, "border-") : (char *)NULL;
	if (!p || !*(p + 7)) {
		char *q = getenv("FZF_DEFAULT_OPTS");
		p = q ? strstr(q, "border-") : (char *)NULL;
		if (!p || !*(p + 7))
			return;
	}

	switch(*(p + 7)) {
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
	launch_execve(cmd, 0, 0);
}

/* Remove any image printed by ueberzug.
 * This function assumes:
 * 1) That ueberzug was launched using the json parser (default)
 * 2) That the pipe was created and exported as FIFO_UEBERZUG
 * 3) That the indentifier string was set to "clifm-preview" */
static void
ueberzug_clear(const char *file)
{
	FILE *fp = fopen(file, "w");
	if (!fp)
		return;

	fprintf(fp, "{\"action\": \"remove\", \"identifier\": \"clifm-preview\"}\n");
	fclose(fp);
}

/* Let's clear images printed on the terminal screen, either via
 * ueberzug(1) or the kitty image protocol */
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
	 * displaying and removing the image). Take a look at how ranger does it */

	/* CLIFM_KITTY_IMG is set by the clifmrun plugin to a temp file name,
	 * which is then created (empty) by the clifmimg plugin every time an
	 * image is displayed via the kitty protocol */
	p = getenv("CLIFM_KITTY_IMG");
	struct stat a;
	if (p && stat(p, &a) != -1) {
		kitty_clear();
		unlinkat(AT_FDCWD, p, 0);
	}
}

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

/* Check whether a given command needs ELN's to be expanded/completed/suggested
 * Returns 1 if yes or 0 if not */
int
_expand_eln(const char *text)
{
	char *l = rl_line_buffer;
	if (!l || !*l || !is_number(text))
		return 0;

	int a = atoi(text); /* Only expand numbers matching ELN's */
	if (a <= 0 || a > (int)files)
		return 0;

	if (nwords == 1) { /* First word */
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

/* Sleep for MSEC milliseconds */
/* Taken from https://stackoverflow.com/questions/1157209/is-there-an-alternative-sleep-function-in-c-to-milliseconds */
static int
msleep(long msec)
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

/* Convert the file named STR (as absolute path) into a more friendly format
 * Change absolute paths into:
 * "./" if file is in CWD
 * "~" if file is in HOME
 * The reformated file name is returned if actually reformated, in which case
 * the returned value should be freed by the caller
 * Otherwise, a pointer to the original string is returned and must not be
 * freed by the caller
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
	size_t len = strlen(str);
	size_t wlen = (workspaces && workspaces[cur_ws].path)
		? strlen(workspaces[cur_ws].path) : 0;

	/* If STR is in CWD -> ./STR */
	if (workspaces && workspaces[cur_ws].path && wlen > 1 && len > wlen
	&& strncmp(str, workspaces[cur_ws].path, wlen) == 0
	&& *(str + wlen) == '/') {
		name = (char *)xnmalloc(strlen(str + wlen + 1) + 3, sizeof(char));
		sprintf(name, "./%s", str + wlen + 1);
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

char *
normalize_path(char *src, size_t src_len)
{
	if (!src || !*src)
		return (char *)NULL;

	/* Deescape SRC */
	char *tmp = (char *)NULL;

	if (strchr(src, '\\')) {
		tmp = dequote_str(src, 0);
		if (!tmp) {
			_err(ERR_NO_STORE, NOPRINT_PROMPT, _("%s: %s: Error deescaping "
				"string\n"), PROGRAM_NAME, src);
			return (char *)NULL;
		}
		size_t tlen = strlen(tmp);
		if (tlen > 0 && tmp[tlen - 1] == '/')
			tmp[tlen - 1] = '\0';
		strcpy(src, tmp);
		free(tmp);
		tmp = (char *)NULL;
	}

	/* Expand tilde */
	if (*src == '~') {
		tmp = tilde_expand(src);
		if (!tmp) {
			_err(ERR_NO_STORE, NOPRINT_PROMPT, _("%s: %s: Error expanding "
				"tilde\n"), PROGRAM_NAME, src);
			return (char *)NULL;
		}
		size_t tlen = strlen(tmp);
		if (tlen > 0 && tmp[tlen - 1] == '/')
			tmp[tlen - 1] = '\0';

		if (!strstr(tmp, "/.."))
			return tmp;
	}

	char *s = tmp ? tmp : src;
	size_t l = tmp ? strlen(tmp) : src_len;

	/* Resolve references to . and .. */
	char *res;
	size_t res_len;

	if (l == 0 || *s != '/') {
		/* Relative path */
		size_t pwd_len;
		pwd_len = strlen(workspaces[cur_ws].path);
		if (pwd_len == 1 && *workspaces[cur_ws].path == '/') {
			/* If CWD is root (/) do not copy anything. Just create a buffer
			 * big enough to hold "/dir", which will be appended next */
			res = (char *)xnmalloc(l + 2, sizeof(char));
			res_len = 0;
		} else {
			res = (char *)xnmalloc(pwd_len + 1 + l + 1, sizeof(char));
			memcpy(res, workspaces[cur_ws].path, pwd_len);
			res_len = pwd_len;
		}
	} else {
		res = (char *)xnmalloc(l + 1, sizeof(char));
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

		switch(len) {
		case 0: continue;

		case 1:
			if (*ptr == '.')
				continue;
			break;

		case 2:
			if (ptr[0] == '.' && ptr[1] == '.') {
#if !defined(__HAIKU__) && !defined(_BE_POSIX) && !defined(__APPLE__)
				const char *slash = memrchr(res, '/', res_len);
#else
				const char *slash = strrchr(res, '/');
#endif
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

void
rl_ring_bell(void)
{
	switch(bell) {
	case BELL_AUDIBLE:
		RING_BELL; fflush(stderr); return;

	case BELL_FLASH:
		SET_RVIDEO;
		fflush(stderr);
		msleep(VISIBLE_BELL_DELAY);
		UNSET_RVIDEO;
		fflush(stderr);
		return;

#ifdef _READLINE_HAS_ACTIVATE_MARK
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

# ifndef _NO_HIGHLIGHT
		if (conf.highlight && !wrong_cmd) {
			rl_point = rl_mark;
			recolorize_line();
		}
# endif /* !_NO_HIGHLIGHT */
		rl_point = point;
		return;
	}
#endif /* _READLINE_HAS_ACTIVATE_MARK */

	case BELL_NONE: /* fallthrough */
	default: return;
	}
}

/*
int
get_term_bgcolor(const int ifd, const int ofd)
{
	char buf[32] = {0};
	unsigned int i = 0;

	if (enable_raw_mode(ifd) == -1)
		return EXIT_FAILURE;

	// Report terminal background color
	if (write(ofd, "\x1b]11;?\007", 7) != 7)
		goto FAIL;

	// Read the response: "ESC ] 11 ; rgb:COLOR BEL"
	while (i < sizeof(buf) - 1) {
		if (i > 22)
			break;
		if (read(ifd, buf + i, 1) != 1)
			break;
		printf("%d:'%c'\n", buf[i], buf[i]);
		i++;
	}
	buf[i] = '\0';

	char *p = strchr(buf, ':');
	if (!p || !*(++p))
		goto FAIL;
	term_bgcolor = savestring(p, strlen(p));

	disable_raw_mode(ifd);
	return EXIT_SUCCESS;

FAIL:
	disable_raw_mode(ifd);
	return EXIT_FAILURE;
} */

char *
gen_date_suffix(struct tm tm)
{
	char date[64] = "";
	strftime(date, sizeof(date), "%b %d %H:%M:%S %Y", &tm);

	char *suffix = (char *)xnmalloc(68, sizeof(char));
	snprintf(suffix, 67, "%d%d%d%d%d%d", tm.tm_year + 1900, /* NOLINT */
	    tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

	return suffix;
}

/* Create directory DIR with permissions set to MODE */
int
xmkdir(char *dir, mode_t mode)
{
	mode_t old_mask = umask(0);
	int ret = mkdirat(AT_FDCWD, dir, mode);
	umask(old_mask);
	if (ret == -1)
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
}

/* Open a file for read only. Return a file stream associated to a file
 * descriptor (FD) for the file named NAME */
FILE *
open_fstream_r(char *name, int *fd)
{
	if (!name || !*name)
		return (FILE *)NULL;

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

/* Create a file for writing. Return a file stream associated to a file
 * descriptor (FD) for the file named NAME */
FILE *
open_fstream_w(char *name, int *fd)
{
	if (!name || !*name)
		return (FILE *)NULL;

	*fd = open(name, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
	if (*fd == -1)
		return (FILE *)NULL;

	FILE *fp = fdopen(*fd, "w");
	if (!fp) {
		close(*fd);
		return (FILE *)NULL;
	}

	return fp;
}

/* Close file stream FP and file descriptor FD */
void
close_fstream(FILE *fp, int fd)
{
	fclose(fp);
//	No need to close FD: it is automatically closed by fclose(3)
	UNUSED(fd);
//	close(fd);
}

/* Transform S_IFXXX (MODE) into the corresponding DT_XXX constant */
inline mode_t
get_dt(const mode_t mode)
{
	switch (mode & S_IFMT) {
	case S_IFBLK: return DT_BLK;
	case S_IFCHR: return DT_CHR;
	case S_IFDIR: return DT_DIR;
	case S_IFIFO: return DT_FIFO;
	case S_IFLNK: return DT_LNK;
	case S_IFREG: return DT_REG;
	case S_IFSOCK: return DT_SOCK;
	default: return DT_UNKNOWN;
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

/* Disassemble the hex color HEX into attribute, R, G, and B values
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

	tmp[0] = *hex, tmp[1] = *(hex + 1);
	*r = hex2int(tmp);

	tmp[0] = *(hex + 2), tmp[1] = *(hex + 3);
	*g = hex2int(tmp);

	tmp[0] = *(hex + 4), tmp[1] = *(hex + 5);
	*b = hex2int(tmp);

	*attr = 0;
	if (*(hex + 6) == '-' && *(hex + 7) && *(hex + 7) >= '1'
	&& *(hex + 7) <= '9' && !*(hex + 8))
		*attr = *(hex + 7) - '0';

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

/* Given this value: \xA0\xA1\xA2, return an array of integers with
 * the integer values for A0, A1, and A2 respectivelly */
/*int *
get_hex_num(const char *str)
{
	size_t i = 0;
	int *hex_n = (int *)xnmalloc(3, sizeof(int));

	while (*str) {
		if (*str != '\\') {
			str++;
			continue;
		}

		if (*(str + 1) != 'x')
			break;

		str += 2;
		char *tmp = xnmalloc(3, sizeof(char));
		xstrsncpy(tmp, str, 2);

		if (i >= 3)
			hex_n = xrealloc(hex_n, (i + 1) * sizeof(int *));

		hex_n[i++] = hex2int(tmp);

		free(tmp);
		tmp = (char *)NULL;
		str++;
	}

	hex_n = xrealloc(hex_n, (i + 1) * sizeof(int));
	hex_n[i] = -1; // -1 marks the end of the int array

	return hex_n;
} */

/* Count files in DIR_PATH, including self and parent. If POP is set to 1,
 * the function will just check if the directory is populated (it has at
 * least 3 files, including self and parent) */
int
count_dir(const char *dir, int pop)
{
	if (!dir)
		return (-1);

	DIR *p;
	if ((p = opendir(dir)) == NULL) {
		if (errno == ENOMEM)
			exit(ENOMEM);
		return (-1);
	}

	unsigned int c = 0;

	while (readdir(p)) {
		c++;
		if (c > (unsigned int)INT_MAX) {
			--c;
			break;
		}

		if (pop && c > 2)
			break;
	}

	closedir(p);
	return (int)c;
}

/* Get the path of a given command from the PATH environment variable.
 * It basically does the same as the 'which' Unix command */
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

	cmd_path = (char *)xnmalloc(PATH_MAX + 1, sizeof(char));

	size_t i;
	for (i = 0; i < path_n; i++) { /* Check each path in PATH */
	/* Append cmd to each path and check if it exists and is executable */
		snprintf(cmd_path, PATH_MAX, "%s/%s", paths[i].path, cmd); /* NOLINT */
		if (access(cmd_path, X_OK) == 0)
			return cmd_path;
	}

	errno = ENOENT;
	free(cmd_path);
	return (char *)NULL;
}

/* Convert SIZE to human readeable form (at most 2 decimal places)
 * Returns a string of at most MAX_UNIT_SIZE, defined in aux.h */
char *
get_size_unit(off_t size)
{
	/* MAX_UNIT_SIZE == 10 == "1023.99YB\0" */
	char *str = xnmalloc(MAX_UNIT_SIZE, sizeof(char));

	float base = xargs.si == 1 ? 1000 : 1024;

	size_t n = 0;
	float s = (float)size;

	while (s >= base) {
		s = s / base;
		++n;
	}

	int x = (int)s;
	/* If (s - x || s - (float)x) == 0, then S has no reminder (zero)
	 * We don't want to print the reminder when it is zero */

	/* R: Ronnabyte, Q: Quettabyte. It's highly unlikely to have files of
	 * these huge sizes in the near future, but anyway... */
	const char *const u = "BKMGTPEZYRQ";
	snprintf(str, MAX_UNIT_SIZE, "%.*f%c%c",
		(s == 0 || s - (float)x == 0) ? 0 : 2, /* NOLINT */
		(double)s,
		u[n],
		(u[n] != 'B' && xargs.si == 1) ? 'B' : 0);

	return str;
}

off_t
dir_size(char *dir, const int size_in_bytes)
{
	if (!dir || !*dir)
		return (-1);

	char file[PATH_MAX];
#if !defined(__OpenBSD__)
	snprintf(file, sizeof(file), "%s/duXXXXXX", xargs.stealth_mode == 1
		? P_tmpdir : tmp_dir); /* NOLINT */
#else
	snprintf(file, sizeof(file), "%s/duXXXXXXXXXX", xargs.stealth_mode == 1
		? P_tmpdir : tmp_dir); /* NOLINT */
#endif /* !__OpenBSD__ */

	int fd = mkstemp(file);
	if (fd == -1)
		return (-1);

	int stdout_bk = dup(STDOUT_FILENO); /* Save original stdout */

	/* Redirect stdout to the desired file */
	int r = dup2(fd, STDOUT_FILENO);
	close(fd);
	if (r == -1)
		return (-1);

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
			launch_execve(cmd, FOREGROUND, E_NOSTDERR);
		} else {
			char *cmd[] = {bin, "-s", "--apparent-size", block_size,
				"--", dir, NULL};
			launch_execve(cmd, FOREGROUND, E_NOSTDERR);
		}
	} else {
		char *cmd[] = {"du", "-ks", "--", dir, NULL};
		launch_execve(cmd, FOREGROUND, E_NOSTDERR);
	}

	dup2(stdout_bk, STDOUT_FILENO); /* Restore original stdout */
	close(stdout_bk);

	FILE *fp = open_fstream_r(file, &fd);
	if (!fp) {
		unlink(file);
		return (-1);
	}

	off_t retval = -1;
	/* We only need here the first field of the line, which is a file
	 * size and usually takes only a few digits: since a yottabyte takes 26
	 * digits, 32 is more than enough */
	char line[32];
	if (fgets(line, (int)sizeof(line), fp) == NULL) {
		close_fstream(fp, fd);
		unlink(file);
		return (-1);
	}

	char *p = strchr(line, '\t');
	if (p && p != line) {
		*p = '\0';
		retval = (off_t)atoll(line);
	}

	close_fstream(fp, fd);
	unlink(file);
	return retval;
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
 * See the inode manpage */
int
get_link_ref(const char *link)
{
	if (!link)
		return (-1);

	char *linkname = realpath(link, (char *)NULL);
	if (!linkname)
		return (-1);

	struct stat attr;
	int ret = stat(linkname, &attr);
	free(linkname);
	if (ret == -1)
		return (-1);
	return (int)(attr.st_mode & S_IFMT);
}

/* Transform an integer (N) into a string of chars
 * This exists because some Operating systems do not support itoa */
char *
xitoa(int n)
{
	if (!n)
		return "0";

	static char buf[32] = {0};
	int i = 30;

	while (n && i) {
		int rem = n / 10;
		buf[i] = (char)('0' + (n - (rem * 10)));
		n = rem;
		--i;
	}

	return &buf[++i];
}

/* A secure atoi implementation to prevent integer under- and over-flow.
 * Returns the corresponding integer, if valid, or INT_MIN if invalid,
 * setting errno to ERANGE */
int
xatoi(const char *s)
{
	long ret = strtol(s, NULL, 10);

	if (ret < INT_MIN || ret > INT_MAX) {
		errno = ERANGE;
		return INT_MIN;
	}

	return (int)ret;
}

/* Some memory wrapper functions */
void *
xrealloc(void *ptr, size_t size)
{
	void *p = realloc(ptr, size);

	if (!p) {
		_err(0, NOPRINT_PROMPT, _("%s: %s failed to allocate %zu bytes\n"),
			PROGRAM_NAME, __func__, size);
		exit(ENOMEM);
	}

	return p;
}

void *
xcalloc(size_t nmemb, size_t size)
{
	void *p = calloc(nmemb, size);

	if (!p) {
		_err(0, NOPRINT_PROMPT, _("%s: %s failed to allocate %zu bytes\n"),
			PROGRAM_NAME, __func__, nmemb * size);
		exit(ENOMEM);
	}

	return p;
}

void *
xnmalloc(size_t nmemb, size_t size)
{
	void *p = malloc(nmemb * size);

	if (!p) {
		_err(0, NOPRINT_PROMPT, _("%s: %s failed to allocate %zu bytes\n"),
			PROGRAM_NAME, __func__, nmemb * size);
		exit(ENOMEM);
	}

	return p;
}

/* Unlike getchar this does not wait for newline('\n')
https://stackoverflow.com/questions/12710582/how-can-i-capture-a-key-stroke-immediately-in-linux 
*/
char
xgetchar(void)
{
	struct termios oldt, newt;
	char c = 0;

	if (tcgetattr(STDIN_FILENO, &oldt) == -1) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "%s: tcgetattr: %s\n",
			PROGRAM_NAME, strerror(errno));
		return 0;
	}
	newt = oldt;
	newt.c_lflag &= (tcflag_t)~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	c = (char)getchar(); /* flawfinder: ignore */
	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

	return c;
}

/* Converts a hex char to its integer value */
char
from_hex(char c)
{
	return (char)(isdigit(c) ? c - '0' : tolower(c) - 'a' + 10);
}

/* Converts an integer value to its hex form */
static char
to_hex(char c)
{
	static const char hex[] = "0123456789ABCDEF";
	return hex[c & 15];
}

/* Returns a url-encoded version of str */
char *
url_encode(char *str)
{
	if (!str || !*str)
		return (char *)NULL;

	char *buf = (char *)xnmalloc((strlen(str) * 3) + 1, sizeof(char));
	/* The max lenght of our buffer is 3 times the length of STR plus
	 * 1 extra byte for the null byte terminator: each char in STR will
	 * be, if encoded, %XX (3 chars) */

	/* Copies of STR and BUF pointers to be able
	 * to increase and/or decrease them without loosing the original
	 * memory location */
	char *pstr, *pbuf;
	pstr = str;
	pbuf = buf;

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
			*pbuf = to_hex(*pstr >> 4); /* Right shift operation */
			pbuf++;
			*pbuf = to_hex(*pstr & 15); /* Bitwise AND operation */
			pbuf++;
		}
	}

	*pbuf = '\0';
	return buf;
}

/* Returns a url-decoded version of str */
char *
url_decode(char *str)
{
	if (!str || !*str)
		return (char *)NULL;

	char *buf = (char *)xnmalloc(strlen(str) + 1, sizeof(char));
	/* The decoded string will be at most as long as the encoded string */

	char *pstr, *pbuf;
	pstr = str;
	pbuf = buf;
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
