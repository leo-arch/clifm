/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* fsinfo.h */

#ifndef FSINFO_H
#define FSINFO_H

#include <sys/statvfs.h>
#define DEV_NO_NAME "-" /* String used when no filesystem name/type is found */

__BEGIN_DECLS

int get_mnt_info(const char *file, char **devname, char **fstype,
	struct statvfs *a, const dev_t dev);

__END_DECLS

#endif /* FS_INFO */
