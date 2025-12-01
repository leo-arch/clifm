/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2025 L. Abramovich <leo.clifm@outlook.com>
*/

/* selection.h */

#ifndef SELECTION_H
#define SELECTION_H

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) \
|| defined(__DragonFly__)
# include <sys/ioctl.h>
#endif /* BSD */

__BEGIN_DECLS

int  deselect(char **args);
void list_selected_files(void);
int  sel_function(char **args);
int  select_file(char *file);
int  save_sel(void);
int  deselect_all(void);

__END_DECLS

#endif /* _SELECTION_H */
