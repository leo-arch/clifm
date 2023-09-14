/* compat.c -- compatibility layer for legacy systems (before POSIX-1.2008) */

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

#if defined(CLIFM_LEGACY) /*|| (defined(_POSIX_C_SOURCE) \
&& _POSIX_C_SOURCE < 200809L) || (defined(_XOPEN_SOURCE) \
&& _XOPEN_SOURCE < 700) */

#include "helpers.h"

#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

/* -------------------- *AT functions -------------------------- */

/* Replacement for fstatat(2) */
int
old_stat(int fd, const char *restrict path, struct stat *sb, int flag)
{
	UNUSED(fd);
	if (flag == AT_SYMLINK_NOFOLLOW)
		return lstat(path, sb);

	return stat(path, sb);
}

/* Replacement for fchmodat(2) */
int
old_chmod(int fd, const char *path, mode_t mode, int flag)
{
	UNUSED(fd); UNUSED(flag);
	return chmod(path, mode); /* flawfinder: ignore */
}

/* Replacement for renameat(2) */
int
old_rename(int olddirfd, const char *oldpath, int newdirfd, const char *newpath)
{
	UNUSED(olddirfd); UNUSED(newdirfd);
	return rename(oldpath, newpath); /* flawfinder: ignore */
}

/* Replacement for mkdirat(2) */
int
old_mkdir(int dirfd, const char *pathname, mode_t mode)
{
	UNUSED(dirfd);
	return mkdir(pathname, mode); /* flawfinder: ignore */
}

/* Replacement for readlinkat(2) */
ssize_t
old_readlink(int dirfd, const char *restrict pathname,
	char *restrict buf, size_t bufsiz)
{
	UNUSED(dirfd);
	return readlink(pathname, buf, bufsiz); /* flawfinder: ignore */
}

/* Replacement for symlinkat(2) */
int
old_symlink(const char *target, int newdirfd, const char *linkpath)
{
	UNUSED(newdirfd);
	return symlink(target, linkpath); /* flawfinder: ignore */
}

/* Replacement for unlinkat(2) */
int
old_unlink(int dirfd, const char *pathname, int lflags)
{
	UNUSED(dirfd); UNUSED(lflags);
	return unlink(pathname); /* flawfinder: ignore */
}

/* Replacement for fchownat(2) */
int
old_chown(int fd, const char *path, uid_t owner, gid_t group, int flag)
{
	UNUSED(fd); UNUSED(flag);
	return chown(path, owner, group); /* flawfinder: ignore */
}

/* ----------------- strnlen ------------------------ */

/* strnlen(3) is not specified in POSIX-1.2001 */
size_t
x_strnlen(const char *s, size_t max)
{
	size_t n = 0;

	while (*s && n < max) {
		s++;
		n++;
	}

	return n;
}

/* ------------------------- scandir ------------------------ */

/* Taken from glibc 2.34, licensed GPL2.1+.
 * Modified code is licensed GPL2+. */

#ifndef _D_ALLOC_NAMLEN
# ifdef _DIRENT_HAVE_D_NAMLEN
#  define _D_EXACT_NAMLEN(d) ((d)->d_namlen)
#  define _D_ALLOC_NAMLEN(d) (_D_EXACT_NAMLEN(d) + 1)
# else
#  define _D_EXACT_NAMLEN(d) (strlen((d)->d_name))
#  ifdef _DIRENT_HAVE_D_RECLEN
#   define _D_ALLOC_NAMLEN(d) (((char *)(d) + (d)->d_reclen) - &(d)->d_name[0])
#  else
#   define _D_ALLOC_NAMLEN(d) (sizeof(d)->d_name > 1 ? sizeof(d)->d_name \
                   : _D_EXACT_NAMLEN(d) + 1)
#  endif /* _DIRENT_HAVE_RECLEN */
# endif /* _DIRENT_HAVE_D_TYPE */
#endif /* !_D_ALLOC_NAMLEN */

struct scandir_cancel_struct {
	DIR *dp;
	void *v;
	size_t cnt;
};

/* Replacement for alphasort(3) */
int
x_alphasort(const struct dirent **a, const struct dirent **b)
{
	return strcoll((*a)->d_name, (*b)->d_name);
}

static void
scandir_cancel_handler(void *arg)
{
	struct scandir_cancel_struct *cp = arg;
	void **v = cp->v;

	for (size_t i = 0; i < cp->cnt; ++i)
		free(v[i]);
	free(v);

	closedir(cp->dp);
}

/* Replacement for scandir(3) */
int
x_scandir(const char *dir, struct dirent ***namelist,
	int (*select)(const struct dirent *),
	int (*cmp)(const struct dirent **, const struct dirent **))
{
	if (!dir || !*dir)
		return (-1);

	DIR *dp = opendir(dir);
	if (dp == NULL)
		return (-1);

	struct dirent **v = NULL;
	size_t vsize = 0;
	struct dirent *d;
	struct scandir_cancel_struct c = { .dp = dp };
	int result = 0;

	int save = errno;
	errno = 0;

	while ((d = readdir(dp)) != NULL) {
		if (select != NULL) {
			int selected = (*select)(d);
			errno = 0;
			if (!selected)
				continue;
		}

		if (c.cnt == vsize) {
			if (vsize == 0)
				vsize = 10;
			else
				vsize *= 2;

			struct dirent **new =
				(struct dirent **)realloc(v, vsize * sizeof(*v));
			if (new == NULL)
				break;

			c.v = v = new;
		}

		size_t dsize = (size_t)(&d->d_name[_D_ALLOC_NAMLEN(d)] - (char *)d);
		struct dirent *vnew = malloc(dsize);
		if (vnew == NULL)
			break;

		v[c.cnt] = (struct dirent *)memcpy(vnew, d, dsize);
		c.cnt++;

		errno = 0;
	}

	if (errno == 0) {
		closedir(dp);

		if (cmp != NULL && v != NULL)
			qsort(v, c.cnt, sizeof(*v),
				(int (*)(const void *, const void *))cmp);

		*namelist = v;
		result = (int)c.cnt;
	} else {
		scandir_cancel_handler(&c);
		result = -1;
	}

	if (result > 0)
		errno = save;

	return result;
}

/* ------------------------ getline ------------------------ */

/* Code originally written by Michael Burr, and released into the public domain
 * https://stackoverflow.com/questions/12167946/how-do-i-read-an-arbitrarily-long-line-in-c/12169132#12169132
 *
 * All changes are licensed under GPL-2.0-or-later. */

/* Figure out an appropriate new allocation size that's not too small or
 * too big.
 * These numbers seem to work pretty well for most text files.
 * The function returns the input value if it decides that new allocation
 * block would be just too big (the caller should handle this as an error). */
static size_t
nx_getdelim_get_realloc_size(size_t current_size)
{
	const size_t k_min_realloc_inc = 32;
	const size_t k_max_realloc_inc = 1024;

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
	char *tmp = (char *)NULL;
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
	 * changed the block size, so we have to check again. */
	if (tmp && ((count + 2) <= tmp_size)) {
		tmp[count] = ch;
		count++;
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
	char *line = (char *)NULL;
	size_t size = 0;
	size_t count = 0;
	int err = 0;

	if (!lineptr || !n)
		return -EINVAL;

	line = *lineptr;
	size = *n;

	for (;;) {
		int ch = fgetc(stream);

		if (ch == EOF)
			break;

		ssize_t result = nx_getdelim_append(&line, &size, count, (char)ch);

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

	/* Compilers, at least GCC and Clang, emit a warning here about a
	 * possible memory leak. That's true, but according to getline(3),
	 * which we are emulating here, it's the caller responsibility to
	 * free the line pointer (LINEPTR) passed to the function. So, the
	 * "leak" is intended. */
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

#else
void *skip_me_compat;
#endif /* CLIFM_LEGACY */
