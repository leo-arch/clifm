/* aux.h */

/*
 * This file is part of CliFM
 *
 * Copyright (C) 2016-2023, L. Abramovich <leo.clifm@outlook.com>
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

#ifndef AUX_H
#define AUX_H

#include <time.h>
#include "mem.h"

#ifdef RL_READLINE_VERSION
# if RL_READLINE_VERSION >= 0x0801
#  define READLINE_HAS_ACTIVATE_MARK
# endif /* RL_READLINE_VERSION >= 0x0801 */
#endif /* RL_READLINE_VERSION */

/* Max size type length for the value returned by get_size_type() */
#define MAX_UNIT_SIZE 10 /* "1023.99YB\0" */

__BEGIN_DECLS

char *abbreviate_file_name(char *);
char *construct_human_size(const off_t);
char *get_cwd(char *, const size_t, const int);
filesn_t count_dir(const char *, const int);
off_t dir_size(char *, const int, int *);
char from_hex(char);
char *gen_date_suffix(const struct tm);
void gen_time_str(char *, const size_t, const time_t);
char *get_cmd_path(const char *);
int  get_rgb(char *, int *, int *, int *, int *);
void clear_term_img(void);
mode_t get_dt(const mode_t);
int  get_link_ref(const char *);
char *hex2rgb(char *);
char *normalize_path(char *, const size_t);
FILE *open_fread(char *, int *);
FILE *open_fwrite(char *, int *);
FILE *open_fappend(char *);
int  read_octal(char *);
void rl_ring_bell(void);
void set_fzf_preview_border_type(void);
int  should_expand_eln(const char *);
char *url_encode(char *);
char *url_decode(char *);
filesn_t xatof(const char *s);
int  xatoi(const char *);
char *xitoa(long long);
char xgetchar(void);
int  xmkdir(char *, const mode_t);
void xregerror(const char *, const char *, const int, const regex_t, const int);

#ifndef _NO_ICONS
size_t hashme(const char *, const int);
#endif /* !_NO_ICONS */

__END_DECLS

#endif /* AUX_H */
