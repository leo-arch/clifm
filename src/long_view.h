/* long_view.h - Construct entries in long view mode */

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

#ifndef LONG_VIEW_H
#define LONG_VIEW_H

/* These macros define the max length for each properties field.
 * These lengths are made based on how each field is built (i.e. displayed).
 * We first construct and store (in the stack, to avoid expensive heap
 * allocation) the appropeiate value, and then print them all
 * (print_entry_props()). */

/* 14 colors + 15 single chars + NUL byte */
#define PERM_STR_LEN  ((MAX_COLOR * 14) + 16) /* construct_file_perms() */

#define TIME_STR_LEN  (MAX_TIME_STR + (MAX_COLOR * 2) + 2) /* construct_timestamp() */
/* construct_human_size() returns a string of at most MAX_HUMAN_SIZE chars (helpers.h) */
#define SIZE_STR_LEN  (MAX_HUMAN_SIZE + (MAX_COLOR * 3) + 10) /* construct_file_size() */
/* 2 colors + 2 names + (space + NUL byte) + DIM */
#define ID_STR_LEN    ((MAX_COLOR * 2) + (NAME_MAX * 2) + 2 + 4)
/* Max inode number able to hold: 999 billions! Padding could be as long
 * as max inode lenght - 1 */
#define INO_STR_LEN   ((MAX_COLOR * 2) + ((12 + 1) * 2) + 4)

#define LINKS_STR_LEN ((MAX_COLOR * 2) + 32)
/* Files counter */
#define FC_STR_LEN    ((MAX_COLOR * 2) + 32)
/* File allocated blocks */
#define BLK_STR_LEN   ((MAX_COLOR * 2) + 32)

/* Macros to calculate relative timestamps (used by calc_relative_time()) */
#define RT_SECOND 1
#define RT_MINUTE (time_t)(60  * RT_SECOND)
#define RT_HOUR   (time_t)(60  * RT_MINUTE)
#define RT_DAY    (time_t)(24  * RT_HOUR)
#define RT_WEEK   (time_t)(7   * RT_DAY)
#define RT_MONTH  (time_t)(30  * RT_DAY)
#define RT_YEAR   (time_t)(365 * RT_DAY)

__BEGIN_DECLS

int print_entry_props(const struct fileinfo *props,
	const struct maxes_t *maxes, const int have_xattr);

__END_DECLS

#endif /* LONG_VIEW_H */
