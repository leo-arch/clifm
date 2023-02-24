/* aux.h */

/*
 * This file is part of CliFM
 * 
 * Copyright (C) 2016-2023, L. Abramovich <johndoe.arch@outlook.com>
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

#ifdef RL_READLINE_VERSION
# if RL_READLINE_VERSION >= 0x0801
#  define _READLINE_HAS_ACTIVATE_MARK
# endif /* RL_READLINE_VERSION >= 0x0801 */
#endif /* RL_READLINE_VERSION */

/* Max size type length for the value returned by get_size_type() */
#define MAX_UNIT_SIZE 10 /* "1023.99YB\0" */
#define LONG_TIME_STR "%a %b %d %T %Y %z" /* Used by history and trash */

__BEGIN_DECLS

int  _expand_eln(const char *);
char *abbreviate_file_name(char *);
void close_fstream(FILE *, int);
int  count_dir(const char *, int);
off_t dir_size(char *);
char from_hex(char);
//char *from_octal(char *s);
char *gen_date_suffix(struct tm);
void gen_time_str(char *, const size_t, const time_t);
char *get_cmd_path(const char *);
int  get_rgb(char *, int *, int *, int *, int *);
void clear_term_img(void);
mode_t get_dt(const mode_t);
/*int *get_hex_num(const char *str); */
int  get_link_ref(const char *);
char *get_size_unit(off_t);
//int  get_term_bgcolor(const int ifd, const int ofd);
char *hex2rgb(char *);
char *normalize_path(char *, size_t);
FILE *open_fstream_r(char *, int *);
FILE *open_fstream_w(char *, int *);
int  read_octal(char *);
void rl_ring_bell(void);
void set_fzf_preview_border_type(void);
char *url_encode(char *);
char *url_decode(char *);
int  xatoi(const char *);
char *xitoa(int);
char xgetchar(void);
int  xmkdir(char *, mode_t);

/* Some memory wrapper functions */
void *xrealloc(void *, size_t);
void *xcalloc(size_t, size_t);
void *xnmalloc(size_t, size_t);

__END_DECLS

#endif /* AUX_H */
