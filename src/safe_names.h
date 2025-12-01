/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2025 L. Abramovich <leo.clifm@outlook.com>
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
