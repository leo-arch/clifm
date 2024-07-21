/* compat.h -- compatibility layer for legacy systems (before POSIX-1.2008) */

/*
 * This file is part of CliFM
 *
 * Copyright (C) 2016-2024, L. Abramovich <leo.clifm@outlook.com>
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

#ifndef CLIFM_COMPAT_H
#define CLIFM_COMPAT_H

#include <stdio.h> /* FILE */

/* Let's fake the following macros: we won't use them anyway. */
#ifndef AT_FDCWD
# define AT_FDCWD -100
#endif /* !AT_FDCWD */

#ifndef AT_SYMLINK_NOFOLLOW
# define AT_SYMLINK_NOFOLLOW 0x100
#endif /* !AT_SYMLINK_NOFOLLOW */

#ifndef O_CLOEXEC
# define O_CLOEXEC 0
#endif /* O_CLOEXEC */

#ifdef dirfd
/* Just in case: on some systems (like NetBSD) dirfd is a macro (defined in
 * dirent.h) */
# undef dirfd
#endif /* dirfd */
#define dirfd(d)   (0)

#define alphasort  x_alphasort
#define fchownat   old_chown
#define fchmodat   old_chmod
#define fstatat    old_stat
#define getline    x_getline
#define mkdirat    old_mkdir
#define readlinkat old_readlink
#define renameat   old_rename
#define scandir    x_scandir
#define strnlen    x_strnlen
#define symlinkat  old_symlink
#define unlinkat   old_unlink

__BEGIN_DECLS

int old_chmod(int fd, const char *path, mode_t mode, int flag);
int old_chown(int fd, const char *path, uid_t owner, gid_t group, int flag);
int old_mkdir(int dirfd, const char *pathname, mode_t mode);
ssize_t old_readlink(int dirfd, const char *restrict pathname,
	char *restrict buf, size_t bufsiz);
char *old_realpath(const char *restrict path, char *restrict resolved_path);
int old_rename(int olddirfd, const char *oldpath, int newdirfd,
	const char *newpath);
int old_stat(int fd, const char *restrict path, struct stat *sb, int flag);
int old_symlink(const char *target, int newdirfd, const char *linkpath);
int old_unlink(int dirfd, const char *pathname, int lflags);
int x_alphasort(const struct dirent **a, const struct dirent **b);
ssize_t x_getline(char **lineptr, size_t *n, FILE *stream);
int x_scandir(const char *dir, struct dirent ***namelist,
	int (*select)(const struct dirent *),
	int (*cmp)(const struct dirent **, const struct dirent **));
size_t x_strnlen(const char *s, size_t max);

__END_DECLS

#endif /* CLIFM_COMPAT_H */
