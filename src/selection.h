/* selection.h */

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
