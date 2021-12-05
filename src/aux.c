/* aux.c -- functions that do not fit in any other file */

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

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <limits.h>
#include <readline/readline.h>

#include "aux.h"
#include "exec.h"
#include "misc.h"
#ifndef _NO_HIGHLIGHT
#include "highlight.h"
#endif

#ifdef RL81
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
#endif

void
rl_ring_bell(void)
{
	switch(bell) {
	case BELL_NONE: return;

	case BELL_AUDIBLE:
		fputs("\007", stderr);
		fflush(stderr);
		return;

/* rl_activate_mark and rl_deactivate mark are available only since
 * readline 8.1 */
#ifdef RL81
	case BELL_VISIBLE: {
		int point = rl_point;
		rl_mark = rl_last_word_start;
		if (rl_end > 1 && rl_line_buffer[rl_end - 1] == ' ')
			rl_point--;
		rl_activate_mark();
		rl_redisplay();
		msleep(VISIBLE_BELL_DELAY);
		rl_deactivate_mark();
#ifndef _NO_HIGHLIGHT
		if (highlight && !wrong_cmd) {
			rl_point = rl_mark;
			recolorize_line();
		}
#endif /* !_NO_HIGHLIGHT */
		rl_point = point;
		return;
		}
#endif /* RL81 */

	default: return;
	}

	return;
}

/* The following three functions were taken from
 * https://github.com/antirez/linenoise/blob/master/linenoise.c
 * and modified to fir our needs: they are used to get current cursor
 * position (both vertical and horizontal) by the suggestions system */

/* Set the terminal into raw mode. Return 0 on success and -1 on error */
static int
enable_raw_mode(const int fd)
{
	struct termios raw;

	if (!isatty(STDIN_FILENO))
		goto FAIL;

	if (tcgetattr(fd, &orig_termios) == -1)
		goto FAIL;

	raw = orig_termios;  /* modify the original mode */
	/* input modes: no break, no CR to NL, no parity check, no strip char,
	 * * no start/stop output control. */
	raw.c_iflag &= (tcflag_t)~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	/* output modes - disable post processing */
	raw.c_oflag &= (tcflag_t)~(OPOST);
	/* control modes - set 8 bit chars */
	raw.c_cflag |= (CS8);
	/* local modes - choing off, canonical off, no extended functions,
	 * no signal chars (^Z,^C) */
	raw.c_lflag &= (tcflag_t)~(ECHO | ICANON | IEXTEN | ISIG);
    /* control chars - set return condition: min number of bytes and timer.
     * We want read to return every single byte, without timeout. */
	raw.c_cc[VMIN] = 1; raw.c_cc[VTIME] = 0; /* 1 byte, no timer */

	/* put terminal in raw mode after flushing */
	if (tcsetattr(fd, TCSAFLUSH, &raw) < 0)
		goto FAIL;

	return 0;

FAIL:
	errno = ENOTTY;
	return -1;
}

static int
disable_raw_mode(const int fd)
{
	if (tcsetattr(fd, TCSAFLUSH, &orig_termios) != -1)
		return EXIT_SUCCESS;
	return EXIT_FAILURE;
}

/* Use the "ESC [6n" escape sequence to query the cursor position (both
 * vertical and horizontal) and store both values into global variables.
 * Return 0 on success and 1 on error */
int
get_cursor_position(const int ifd, const int ofd)
{
	char buf[32];
	int cols, rows;
	unsigned int i = 0;

	if (enable_raw_mode(ifd) == -1)
		return EXIT_FAILURE;

	/* Report cursor location */
	if (write(ofd, "\x1b[6n", 4) != 4)
		goto FAIL;

	/* Read the response: "ESC [ rows ; cols R" */
	while (i < sizeof(buf) - 1) {
		if (read(ifd, buf + i, 1) != 1)
			break;
		if (buf[i] == 'R')
			break;
		i++;
	}
	buf[i] = '\0';

	/* Parse it */
	if (buf[0] != _ESC || buf[1] != '[')
		goto FAIL;
	if (sscanf(buf + 2, "%d;%d", &rows, &cols) != 2)
		goto FAIL;

	currow = rows;
	curcol = cols;

	disable_raw_mode(ifd);
	return EXIT_SUCCESS;

FAIL:
	disable_raw_mode(ifd);
	return EXIT_FAILURE;
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
	snprintf(suffix, 67, "%d%d%d%d%d%d", tm.tm_year + 1900,
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
	close(fd);
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

/*
static int
hex2int(char *str)
{
	int i, n[2] = { 0 };
	for (i = 1; i >= 0; i--) {
		if (str[i] >= '0' && str[i] <= '9') {
			n[i] = str[i] - '0';
		} else {
			switch (str[i]) {
			case 'A':
			case 'a':
				n[i] = 10;
				break;
			case 'B':
			case 'b':
				n[i] = 11;
				break;
			case 'C':
			case 'c':
				n[i] = 12;
				break;
			case 'D':
			case 'd':
				n[i] = 13;
				break;
			case 'E':
			case 'e':
				n[i] = 14;
				break;
			case 'F':
			case 'f':
				n[i] = 15;
				break;
			default:
				break;
			}
		}
	}

	return ((n[0] * 16) + n[1]);
} */

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
 * The function will just check if the directory is populated (it has at
 * least 3 files, including self and parent)*/
int
count_dir(const char *dir, int pop)
{
	if (!dir)
		return (-1);

	DIR *p;
	if ((p = opendir(dir)) == NULL) {
		if (errno == ENOMEM)
			exit(EXIT_FAILURE);
		else
			return (-1);
	}

	int c = 0;

	while (readdir(p)) {
		c++;
		if (pop && c > 2)
			break;
	}

	closedir(p);
	return c;
}

/* Get the path of a given command from the PATH environment variable.
 * It basically does the same as the 'which' Unix command */
char *
get_cmd_path(const char *cmd)
{
	char *cmd_path = (char *)xnmalloc(PATH_MAX + 1, sizeof(char));

	size_t i;
	for (i = 0; i < path_n; i++) { /* Check each path in PATH */
		/* Append cmd to each path and check if it exists and is
		 * executable */
		snprintf(cmd_path, PATH_MAX, "%s/%s", paths[i], cmd);
		if (access(cmd_path, X_OK) == 0)
			return cmd_path;
	}

	free(cmd_path);
	return (char *)NULL;
}

/* Convert FILE_SIZE to human readeable form */
char *
get_size_unit(off_t size)
{
#define MAX_UNIT_SIZE 9
	/* Max size type length == 9 == "1023.99K\0" */
	char *str = xnmalloc(MAX_UNIT_SIZE, sizeof(char));

	size_t n = 0;
	float s = (float)size;

	while (s > 1024) {
		s = s / 1024;
		++n;
	}

	int x = (int)s;
	/* If s - x == 0, then S has no reminder (zero)
	 * We don't want to print the reminder when it is zero */

	const char *const u = "BKMGTPEZY";
	snprintf(str, MAX_UNIT_SIZE, "%.*f%c", (s == 0 || s - (float)x == 0)
			? 0 : 2, (double)s, u[n]);

	return str;
}

off_t
dir_size(char *dir)
{
	if (!dir || !*dir)
		return (-1);

	char file[PATH_MAX];
	snprintf(file, PATH_MAX, "%s/duXXXXXX", P_tmpdir);

	int fd = mkstemp(file);
	if (fd == -1)
		return (-1);

	int stdout_bk = dup(STDOUT_FILENO); /* Save original stdout */
	dup2(fd, STDOUT_FILENO); /* Redirect stdout to the desired file */
	close(fd);

	char *cmd[] = {"du", "-ks", dir, NULL};
	launch_execve(cmd, FOREGROUND, E_NOSTDERR);

	dup2(stdout_bk, STDOUT_FILENO); /* Restore original stdout */
	close(stdout_bk);

	FILE *fp = open_fstream_r(file, &fd);
	if (!fp) {
		unlink(file);
		return (-1);
	}

	off_t retval = -1;
	/* I only need here the first field of the line, which is a
	 * file size and could only take a few bytes, so that 32
	 * bytes is more than enough */
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

int
xatoi(const char *s)
{
	long ret = strtol(s, NULL, 10);
	if (ret == LONG_MAX || ret == LONG_MIN) {
		fprintf(stderr, "%s: strtol: %s: %s\n", PROGRAM_NAME, s, strerror(errno));
		exit(EXIT_FAILURE);
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
		exit(EXIT_FAILURE);
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
		exit(EXIT_FAILURE);
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
		exit(EXIT_FAILURE);
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
	char c;

	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= (tcflag_t)~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	c = (char)getchar();
	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

	return c;
}

/* The following four functions (from_hex, to_hex, url_encode, and
 * url_decode) were taken from "http://www.geekhideout.com/urlcode.shtml"
 * and modified to comform to RFC 2395, as recommended by the
 * freedesktop trash specification */

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
	static char hex[] = "0123456789ABCDEF";
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
		if (isalnum(*pstr) || *pstr == '-' || *pstr == '_' || *pstr == '.'
		|| *pstr == '~' || *pstr == '/') {
			/* Do not encode any of the above chars */
			*pbuf++ = *pstr;
		} else {
			/* Encode char to URL format. Example: space char to %20 */
			*pbuf++ = '%';
			*pbuf++ = to_hex(*pstr >> 4); /* Right shift operation */
			*pbuf++ = to_hex(*pstr & 15); /* Bitwise AND operation */
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
	/* The decoded string will be at most as long as the encoded
	 * string */

	char *pstr, *pbuf;
	pstr = str;
	pbuf = buf;
	for (; *pstr; pstr++) {
		if (*pstr == '%') {
			if (pstr[1] && pstr[2]) {
				/* Decode URL code. Example: %20 to space char */
				/* Left shift and bitwise OR operations */
				*pbuf++ = (char)(from_hex(pstr[1]) << 4 | from_hex(pstr[2]));
				pstr += 2;
			}
		} else {
			*pbuf++ = *pstr;
		}
	}

	*pbuf = '\0';
	return buf;
}

/* Convert octal string into integer.
 * Taken from: https://www.geeksforgeeks.org/program-octal-decimal-conversion/
 * Used by decode_prompt() to make things like this work: \033[1;34m */
int
read_octal(char *str)
{
	if (!str || !*str)
		return (-1);

	int n = atoi(str);
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
