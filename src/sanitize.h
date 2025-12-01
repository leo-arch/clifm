/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2025 L. Abramovich <leo.clifm@outlook.com>
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
