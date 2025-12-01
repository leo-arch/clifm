/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2025 L. Abramovich <leo.clifm@outlook.com>
*/

/* compat.h */

#ifndef CLIFM_COMPAT_H
#define CLIFM_COMPAT_H

#include <stdio.h> /* FILE */

/* Let's fake the following macros: we won't use them anyway. */
#ifndef AT_FDCWD
# define AT_FDCWD (-100)
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
/* A dummy value, since we're not using file descriptors. Functions using
 * them are replaced by functions using plain filenames instead. */
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
