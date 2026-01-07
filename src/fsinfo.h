/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* fsinfo.h */

#ifndef FSINFO_H
#define FSINFO_H

__BEGIN_DECLS

#if defined(LINUX_FSINFO)
char *get_fs_type_name(const char *file, int *remote);
#if !defined(__CYGWIN__) && !defined(__ANDROID__)
char *get_dev_name(const dev_t dev);
#endif /* !__CYGWIN__ && !__ANDROID__ */
char *get_dev_name_mntent(const char *file);
#elif defined(HAVE_STATFS)
void get_dev_info(const char *file, char **devname, char **devtype);
#elif defined(__sun)
char *get_dev_mountpoint(const char *file);
#endif /* LINUX_FSINFO */

__END_DECLS

#endif /* FS_INFO */
