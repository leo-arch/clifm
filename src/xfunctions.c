// 
// contains some useful functions
//

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

#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <wchar.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <libintl.h>

#include "globals.h"

int xstrcmp(const char *s1, const char *s2) {
	/* I use 256 for error code since it does not represent any ASCII code
	 * (the extended version goes up to 255) */
	if (!s1 || !s2)
		return 256;

	while (*s1) {
		if (*s1 != *s2)
			return (*s1 - *s2);
		s1++;
		s2++;
	}

	if (*s2)
		return (0 - *s2);

	return 0;
}

int xstrncmp(const char *s1, const char *s2, size_t n) {
	if (!s1 || !s2)
		return 256;

	size_t c = 0;
	while (*s1 && c++ < n) {
		if (*s1 != *s2)
			return (*s1 - *s2);
		s1++;
		s2++;
	}

	if (c == n)
		return 0;

	if (*s2)
		return (0 - *s2);

	return 0;
}

char * xstrcpy(char *buf, const char *restrict str) {
	if (!str)
		return (char *)NULL;

	while (*str)
		*(buf++) = *(str++);

	*buf = '\0';

	return buf;
}

char * xstrncpy(char *buf, const char *restrict str, size_t n) {
	if (!str)
		return (char *)NULL;

	size_t c = 0;
	while (*str && c++ < n)
		*(buf++) = *(str++);

	*buf = '\0';

	return buf;
}

size_t xstrsncpy(char *restrict dst, const char *restrict src, size_t n) {
	// Taken from NNN's source code: very clever
	char *end = memccpy(dst, src, '\0', n);

	if (!end) {
		dst[n - 1] = '\0';
		end = dst + n;
	}

	return end - dst;
}

size_t wc_xstrlen(const char *restrict str)
{
	size_t len;
#ifndef _BE_POSIX
	wchar_t * const wbuf = (wchar_t *)len_buf;

	/* Convert multi-byte to wide char */
	len = mbstowcs(wbuf, str, NAME_MAX);
	len = wcswidth(wbuf, len);
#else
	len = u8_xstrlen(str);
#endif

	return len;
}

int u8truncstr(char *restrict str, size_t n) {
	/* Truncate an UTF-8 string at length N. Returns zero if truncated and
	 * one if not */
	size_t len = 0;

	while (*(str++)) {

		/* Do not count continuation bytes (used by multibyte, that is,
		 * wide or non-ASCII characters) */
		if ((*str & 0xc0) != 0x80) {
			len++;

			if (len == n) {
				*str = '\0';
				return EXIT_SUCCESS;
			}
		}
	}

	return EXIT_FAILURE;
}

size_t u8_xstrlen(const char *restrict str) {
	/* An strlen implementation able to handle unicode characters. Taken from:
	 * https://stackoverflow.com/questions/5117393/number-of-character-cells-used-by-string
	 * Explanation: strlen() counts bytes, not chars. Now, since ASCII chars
	 * take each 1 byte, the amount of bytes equals the amount of chars.
	 * However, non-ASCII or wide chars are multibyte chars, that is, one char
	 * takes more than 1 byte, and this is why strlen() does not work as
	 * expected for this kind of chars: a 6 chars string might take 12 or
	 * more bytes */
	size_t len = 0;

	while (*(str++)) {
		if ((*str & 0xc0) != 0x80)
			len++;
	}

	return len;
}

#ifndef __FreeBSD__
size_t xstrlen(const char *restrict s) {
	/* Taken from NNN's source code */
#if !defined(__GLIBC__)
	return strlen(s);
#else
	return (char *)rawmemchr(s, '\0') - s;
#endif
}
#endif // __FreeBSD__

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

int xchmod(const char *file, mode_t mode) {
	/* Set or unset S_IXUSR, S_IXGRP, and S_IXOTH */
	(0100 & mode) ? (mode &= ~0111) : (mode |= 0111);

	if (chmod(file, mode) == -1) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, file, strerror(errno));
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int xgetchar(void) {
/* Unlike getchar(), gets key pressed immediately, without the need to
 * wait for new line (Enter)
 * Taken from:
 * https://stackoverflow.com/questions/12710582/how-can-i-capture-a-key-stroke-immediately-in-linux */
	struct termios oldt, newt;
	int ch;

	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~(ICANON|ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	ch = getchar();
	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

	return ch;
}
