/* compat.h -- compatibility layer for legacy systems (before POSIX-1.2008) */

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

#ifndef CLIFM_COMPAT_H
#define CLIFM_COMPAT_H

#include <stdio.h> /* FILE */

/* Let's fake the following macros: we won't use them anyway */
#ifndef AT_FDCWD
# define AT_FDCWD -100
#endif /* !AT_FDCWD */

#ifndef AT_SYMLINK_NOFOLLOW
# define AT_SYMLINK_NOFOLLOW 0x100
#endif /* !AT_SYMLINK_NOFOLLOW */

# ifndef O_CLOEXEC
#  define O_CLOEXEC 0
# endif /* O_CLOEXEC */

# define dirfd(d)   (0)
# define fstatat    old_stat
# define fchmodat   old_chmod
# define mkdirat    old_mkdir
# define getline    x_getline
# define strnlen    x_strnlen
# define renameat   old_rename
# define fchownat   old_chown
# define alphasort  x_alphasort
# define scandir    x_scandir
# define readlinkat old_readlink
# define symlinkat  old_symlink
# define unlinkat   old_unlink

__BEGIN_DECLS

int old_stat(int, const char *restrict, struct stat *, int);
int old_chmod(int, const char *, mode_t, int);
int old_rename(int, const char *, int, const char *);
int old_mkdir(int, const char *, mode_t);
ssize_t old_readlink(int, const char *restrict, char *restrict, size_t);
int old_symlink(const char *, int, const char *);
int old_unlink(int, const char *, int);
int old_chown(int, const char *, uid_t, gid_t, int);
size_t x_strnlen(const char *, size_t);

ssize_t x_getline(char **, size_t *, FILE *);

int x_alphasort(const struct dirent **, const struct dirent **);
int x_scandir(const char *, struct dirent ***,
	int (*)(const struct dirent *),
	int (*)(const struct dirent **, const struct dirent **));

__END_DECLS

#endif /* CLIFM_COMPAT_H */
