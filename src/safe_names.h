/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2016-2025, L. Abramovich <leo.clifm@outlook.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

/* safe_names.h */

#ifndef SAFE_NAMES_H
#define SAFE_NAMES_H

/* Macros and array used to print unsafe names description messages.
 * Used by validate_filename(). */
#define UNSAFE_DASH      0
#define UNSAFE_MIME      1
#define UNSAFE_ELN       2
#define UNSAFE_FASTBACK  3
#define UNSAFE_BTS_CONST 4
#define UNSAFE_CONTROL   5
#define UNSAFE_META      6
#define UNSAFE_LEADING_TILDE 7
#define UNSAFE_LEADING_WHITESPACE 8
#define UNSAFE_TRAILING_WHITESPACE 9
#define UNSAFE_ILLEGAL_UTF8 10
#define UNSAFE_TOO_LONG  11
#define UNSAFE_NOT_PORTABLE 12

#define PORTABLE_CHARSET "abcdefghijklmnopqrstuvwxyz\
ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-_"
#define SHELL_META_CHARS "*?[]<>|(){}&=`^!\\;$"

#define FILE_TYPE_CHARS "bcCdDfFghlLoOpPstux"

__BEGIN_DECLS

int validate_filename(char **name, const int is_md);

__END_DECLS

#endif /* SAFE_NAMES_H */
