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

/* sanitize.h */

#ifndef SANITIZE_H
#define SANITIZE_H

#define ALLOWED_CHARS_NET "abcdefghijklmnopqrstuvwxyz\
ABCDEFGHIJKLMNOPQRSTUVWXYZ\
0123456789 -_.,/="

#define ALLOWED_CHARS_MIME "abcdefghijklmnopqrstuvwxyz\
ABCDEFGHIJKLMNOPQRSTUVWXYZ\
0123456789 -_.,%&"

/* Used to sanitize DISPLAY env variable */
#define ALLOWED_CHARS_DISPLAY "abcdefghijklmnopqrstuvwxyz\
ABCDEFGHIJKLMNOPQRSTUVWXYZ\
0123456789-_.,:"

/* Used to sanitize TZ, LANG, and TERM env variables */
#define ALLOWED_CHARS_MISC "abcdefghijklmnopqrstuvwxyz\
ABCDEFGHIJKLMNOPQRSTUVWXYZ\
0123456789-_.,"

/* Used to sanitize commands in gral */
#define ALLOWED_CHARS_GRAL "abcdefghijklmnopqrstuvwxyz\
ABCDEFGHIJKLMNOPQRSTUVWXYZ\
0123456789 -_.,/'\""

/* Used to sanitize commands based on a blacklist
 * Let's reject the following shell features:
 * <>: Input/output redirection
 * |: Pipes/OR list
 * &: AND list
 * $`: Command substitution/environment variables
 * ;: Sequential execution */
//#define BLACKLISTED_CHARS "<>|;&$`"

__BEGIN_DECLS

int sanitize_cmd(const char *str, const int type);
int xsecure_env(const int mode);

__END_DECLS

#endif /* SANITIZE_H */
