/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2025 L. Abramovich <leo.clifm@outlook.com>
*/

/* long_view.h */

#ifndef LONG_VIEW_H
#define LONG_VIEW_H

/* These macros define the max length for each properties field.
 * These lengths are made based on how each field is built (i.e. displayed).
 * We first construct and store (in the stack, to avoid expensive heap
 * allocation) the appropeiate value, and then print them all
 * (print_entry_props()). */

/* 14 colors + 15 single chars + NUL byte */
#define PERM_STR_LEN  ((MAX_COLOR * 14) + 16) /* construct_file_perms() */

#define TIME_STR_LEN  (MAX_TIME_STR + (MAX_COLOR * 4) + 2 + 1) /* construct_timestamp() */
/* construct_human_size() returns a string of at most MAX_HUMAN_SIZE chars (helpers.h) */
#define SIZE_STR_LEN  (MAX_HUMAN_SIZE + (MAX_COLOR * 3) + 10) /* construct_file_size() */
/* 2 colors + 2 names + (space + NUL byte) + DIM */
#define ID_STR_LEN    ((MAX_COLOR * 2) + (NAME_MAX * 2) + 2 + 4)
/* Max inode number able to hold: 999 billions! Padding could be as long
 * as max inode lenght - 1 */
#define INO_STR_LEN   ((MAX_COLOR * 2) + ((12 + 1) * 2) + 4)

#define LINKS_STR_LEN ((MAX_COLOR * 2) + 32)
/* File counter */
#define FC_STR_LEN    ((MAX_COLOR * 2) + 32)
/* File allocated blocks */
#define BLK_STR_LEN   ((MAX_COLOR * 2) + 32)

#define MAX_PROP_STR (PERM_STR_LEN + TIME_STR_LEN + SIZE_STR_LEN \
	+ ID_STR_LEN + INO_STR_LEN + LINKS_STR_LEN + FC_STR_LEN + BLK_STR_LEN + 16)
/* Since PropFieldsGap is at most 2, we need at most two characters per field,
 * except the last one, totaling 14 bytes, leaving enough room for the NUL
 * terminating character as well. */

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
