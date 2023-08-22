/* compat.c -- compatibility later for legacy systems (before POSIX-1.2008) */

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

#include "helpers.h"

#include <unistd.h>
#include <stdio.h>

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
	return chmod(path, mode);
}

/* Replacement for renameat(2) */
int
old_rename(int olddirfd, const char *oldpath, int newdirfd, const char *newpath)
{
	UNUSED(olddirfd); UNUSED(newdirfd);
	return rename(oldpath, newpath);
}

/* Replacement for mkdirat(2) */
int
old_mkdir(int dirfd, const char *pathname, mode_t mode)
{
	UNUSED(dirfd);
	return mkdir(pathname, mode);
}

/* Replacement for readlinkat(2) */
ssize_t
old_readlink(int dirfd, const char *restrict pathname,
	char *restrict buf, size_t bufsiz)
{
	UNUSED(dirfd);
	return readlink(pathname, buf, bufsiz);
}

/* Replacement for symlinkat(2) */
int
old_symlink(const char *target, int newdirfd, const char *linkpath)
{
	UNUSED(newdirfd);
	return symlink(target, linkpath);
}

/* Replacement for unlinkat(2) */
int
old_unlink(int dirfd, const char *pathname, int lflags)
{
	UNUSED(dirfd); UNUSED(lflags);
	return unlink(pathname);
}

/* Replacement for fchownat(2) */
int
old_chown(int fd, const char *path, uid_t owner, gid_t group, int flag)
{
	UNUSED(fd); UNUSED(flag);
	return chown(path, owner, group);
}

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
