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


/*  #######################
 *  #  CLIFM CUSTOM LIB   #
 *  ######################*/

#include "helpers.h"

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <termios.h>
#include <ctype.h>
#include <string.h>

#include "aux.h"
#include "exec.h"
#include "misc.h"

/* Given this value: \xA0\xA1\xA1, return an array of integers with
 * the integer values for A0, A1, and A2 respectivelly */
int * get_hex_num(const char *str) {
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

int count_dir(const char *dir_path) /* Readdir version */
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

/* Get the path of a given command from the PATH environment variable.
 * It basically does the same as the 'which' Unix command */
char * get_cmd_path(const char *cmd) {
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

/* Convert FILE_SIZE to human readeable form */
char * get_size_unit(off_t size) {
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

off_t dir_size(char *dir) {
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

	char *cmd[] = { "du", "--block-size=1", "-s", dir, NULL };
	launch_execve(cmd, FOREGROUND, E_NOSTDERR);

	dup2(stdout_bk, STDOUT_FILENO); /* Restore original stdout */
	close(stdout_bk);

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
int get_link_ref(const char *link) {
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

// Transform an integer (N) into a string of chars
// this exists because some Operating systems do not suppoit itoa
char * xitoa(int n) {
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

// some memory wrapper functions
void * xrealloc(void *ptr, size_t size) {
	void *new_ptr = realloc(ptr, size);

	if (!new_ptr) {
		free(ptr);
		_err(0, NOPRINT_PROMPT, _("%s: %s failed to allocate "
					"%zu bytes\n"), PROGRAM_NAME, __func__, size);
		exit(EXIT_FAILURE);
	}

	return new_ptr;
}

void * xcalloc(size_t nmemb, size_t size) {
	void *new_ptr = calloc(nmemb, size);

	if (!new_ptr) {
		_err(0, NOPRINT_PROMPT, _("%s: %s failed to allocate "
					"%zu bytes\n"), PROGRAM_NAME, __func__, nmemb * size);
		exit(EXIT_FAILURE);
	}

	return new_ptr;
}

void * xnmalloc(size_t nmemb, size_t size) {
	void *new_ptr = malloc(nmemb * size);

	if (!new_ptr) {
		_err(0, NOPRINT_PROMPT, _("%s: %s failed to allocate %zu "
					"bytes\n"), PROGRAM_NAME, __func__, nmemb * size);
		exit(EXIT_FAILURE);
	}

	return new_ptr;
}

// unlike getchar this does not wait for newline('\n')
// https://stackoverflow.com/questions/12710582/how-can-i-capture-a-key-stroke-immediately-in-linux
char xgetchar(void)
{
	struct termios oldt, newt;
	char ch;

	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~(ICANON|ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	ch = getchar();
	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

	return ch;
}

/* The following four functions (from_hex, to_hex, url_encode, and
 * url_decode) were taken from "http://www.geekhideout.com/urlcode.shtml"
 * and modified to comform to RFC 2395, as recommended by the
 * freedesktop trash specification */

// Converts a hex char to its integer value
char from_hex(char c) {
	return isdigit(c) ? c - '0' : tolower(c) - 'a' + 10;
}

// Converts an integer value to its hex form
char to_hex(char c) {
	static char hex[] = "0123456789ABCDEF";
	return hex[c & 15];
}

// Returns a url-encoded version of str
char * url_encode(char *str) {
	if (!str || *str == 0x00)
		return (char *)NULL;

	char *p;
	p = (char *)calloc((strlen(str) * 3) + 1, sizeof(char));
	/* The max lenght of our buffer is 3 times the length of STR plus
	 * 1 extra byte for the null byte terminator: each char in STR will
	 * be, if encoded, %XX (3 chars) */
	if (!p)
		return (char *)NULL;

	char *buf;
	buf = p;
	p = (char *)NULL;

	/* Copies of STR and BUF pointers to be able
	 * to increase and/or decrease them without loosing the original
	 * memory location */
	char *pstr, *pbuf;
	pstr = str;
	pbuf = buf;

	for (; *pstr; pstr++) {
		if (isalnum (*pstr) || *pstr == '-' || *pstr == '_'
				|| *pstr == '.' || *pstr == '~' || *pstr == '/')
			/* Do not encode the above chars */
			*pbuf++ = *pstr;
		else {
			/* Encode char to URL format. Example: space char to %20 */
			*pbuf++ = '%';
			*pbuf++ = to_hex(*pstr >> 4); /* Right shift operation */
			*pbuf++ = to_hex(*pstr & 15); /* Bitwise AND operation */
		}
	}

	return buf;
}

// Returns a url-decoded version of str
char * url_decode(char *str) {
	if (!str || str[0] == 0x00)
		return (char *)NULL;

	char *p = (char *)NULL;
	p = (char *)calloc(strlen(str) + 1, sizeof(char));
	/* The decoded string will be at most as long as the encoded
	 * string */

	if (!p)
		return (char *)NULL;

	char *buf;
	buf = p;
	p = (char *)NULL;

	char *pstr, *pbuf;
	pstr = str;
	pbuf = buf;
	for ( ; *pstr; pstr++) {
		if (*pstr == '%') {
			if (pstr[1] && pstr[2]) {
				/* Decode URL code. Example: %20 to space char */  
				/* Left shift and bitwise OR operations */
				*pbuf++ = from_hex(pstr[1]) << 4 | from_hex(pstr[2]);
				pstr += 2;
			}
		}
		else
			*pbuf++ = *pstr;
	}

	return buf;
}

/* Convert octal string into integer.
 * Taken from: https://www.geeksforgeeks.org/program-octal-decimal-conversion/
 * Used by decode_prompt() to make things like this work: \033[1;34m */
int read_octal(char *str) {
	if (!str)
		return -1;

	int n = atoi(str);
	int num = n;
	int dec_value = 0;

	/* Initializing base value to 1, i.e 8^0 */
	int base = 1;

	int temp = num;
	while (temp) {

		// Extracting last digit
		int last_digit = temp % 10;
		temp = temp / 10;

		/* Multiplying last digit with appropriate
		 * base value and adding it to dec_value */
		dec_value += last_digit * base;

		base = base * 8;
	}

	return dec_value;
}

int hex2int(char *str)
{
	int i, n[2];
	for (i = 1; i >= 0; i--) {
		if (str[i] >= '0' && str[i] <= '9')
			n[i] = str[i] - 0x30;
		else {
			switch(str[i]) {
				case 'A': case 'a': n[i] = 10; break;
				case 'B': case 'b': n[i] = 11; break;
				case 'C': case 'c': n[i] = 12; break;
				case 'D': case 'd': n[i] = 13; break;
				case 'E': case 'e': n[i] = 14; break;
				case 'F': case 'f': n[i] = 15; break;
				default: break;
			}
		}
	}

	return ((n[0] * 16) + n[1]);
}
