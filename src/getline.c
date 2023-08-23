/* getline.c - getline, for legacy systems where the POSIX/GNU version isn't
 * available*/

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

/* Code originally written by Michael Burr, and released into the public domain
 * https://stackoverflow.com/questions/12167946/how-do-i-read-an-arbitrarily-long-line-in-c/12169132#12169132
 *
 * All changes are licensed under GPL-2.0-or-later. */

#include "helpers.h"

#include <stdio.h> /* fgetc(), ferror(), feof() */
#include <errno.h>
#include <limits.h>

/* Figure out an appropriate new allocation size that's not too small or
 * too big.
 * These numbers seem to work pretty well for most text files.
 * The function returns the input value if it decides that new allocation
 * block would be just too big (the caller should handle this as an error). */
static size_t
nx_getdelim_get_realloc_size(size_t current_size)
{
	enum {
		k_min_realloc_inc = 32,
		k_max_realloc_inc = 1024,
	};

	if (SSIZE_MAX < current_size)
		return current_size;

	if (current_size <= k_min_realloc_inc)
		return current_size + k_min_realloc_inc;

	if (current_size >= k_max_realloc_inc)
		return current_size + k_max_realloc_inc;

	return current_size * 2;
}

/* Adds a new character to the buffer (LINEPTR), reallocating as necessary
 * to ensure the character and a following null terminator can fit. */
static int
nx_getdelim_append(char **lineptr, size_t *bufsize, size_t count, char ch)
{
	char* tmp = NULL;
	size_t tmp_size = 0;

	if (!lineptr || !bufsize)
		return (-1);

	if (count >= (((size_t)SSIZE_MAX) + 1))
		/* Writing more than SSIZE_MAX to the buffer isn't supported */
		return (-1);

	tmp = *lineptr;
	tmp_size = tmp ? *bufsize : 0;

	/* Make room for the current character (plus the null byte) */
	if ((count + 2) > tmp_size) {
		tmp_size = nx_getdelim_get_realloc_size(tmp_size);

		tmp = (char *)realloc(tmp, tmp_size);
		if (!tmp)
			return (-1);
	}

	*lineptr = tmp;
	*bufsize = tmp_size;

	/* Remember, the reallocation size calculation might not have 
	 * changed the block size, so we have to check again */
	if (tmp && ((count + 2) <= tmp_size)) {
		tmp[count++] = ch;
		tmp[count] = 0;
		return 1;
	}

	return (-1);
}


/* Read data into a dynamically resizable buffer until EOF or until a
 * delimiter character is found. The returned data will be null terminated
 * (unless there's an error allocating memory that prevents it).
 *
 * Returns the number of characters placed in the returned buffer, including
 * the delimiter character, but not including the terminating null byte. */
static ssize_t
nx_getdelim(char **lineptr, size_t *n, int delim, FILE *stream)
{
	ssize_t result = 0;    
	char *line = (char *)NULL;
	size_t size = 0;
	size_t count = 0;
	int err = 0;
	int ch = 0;

	if (!lineptr || !n)
		return -EINVAL;

	line = *lineptr;
	size = *n;

	for (;;) {
		ch = fgetc(stream);

		if (ch == EOF)
			break;

		result = nx_getdelim_append(&line, &size, count, (char)ch);

		/* Check for error adding to the buffer (ie., out of memory) */
		if (result < 0) {
			err = -ENOMEM;
			break;
		}

		++count;

		/* Check if we're done because we've found the delimiter */
		if ((unsigned char)ch == (unsigned char)delim)
			break;

		/* Check if we're passing the maximum supported buffer size */
		if (count > SSIZE_MAX) {
			err = -EOVERFLOW;
			break;
		}
	}

	/* Update the caller's data */
	*lineptr = line;
	*n = size;

	/* Check for various error returns */
	if (err != 0)
		return err;

	if (ferror(stream))
		return 0;

	if (feof(stream) && (count == 0)) {
		if (nx_getdelim_append(&line, &size, count, 0) < 0) {
			return -ENOMEM;
		}
	}

	return (ssize_t)count;
}

static ssize_t
x_getdelim(char **lineptr, size_t *n, char delim, FILE *stream)
{
	ssize_t retval = nx_getdelim(lineptr, n, delim, stream);

	if (retval < 0) {
		errno = -(int)retval;
		retval = -1;
	}

	if (retval == 0)
		retval = -1;

	return retval;
}

/* Implementation of getline(3) */
ssize_t
x_getline(char **lineptr, size_t *n, FILE *stream)
{
	return x_getdelim(lineptr, n, '\n', stream);
}
