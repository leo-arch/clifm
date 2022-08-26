/* sanitize.h */

/*
 * This file is part of CliFM
 * 
 * Copyright (C) 2016-2022, L. Abramovich <johndoe.arch@outlook.com>
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
#define BLACKLISTED_CHARS "<>|;&$`"

int sanitize_cmd(char *cmd, int type);
void sanitize_cmd_environ(void);
void restore_cmd_environ(void);
int xsecure_env(const int mode);
#endif /* SANITIZE_H */
