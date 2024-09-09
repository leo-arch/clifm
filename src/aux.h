/* aux.h */

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

#ifndef AUX_H
#define AUX_H

#include <time.h>
#include "mem.h"

#ifdef RL_READLINE_VERSION
# if RL_READLINE_VERSION >= 0x0801
#  define READLINE_HAS_ACTIVATE_MARK
# endif /* RL_READLINE_VERSION >= 0x0801 */
#endif /* RL_READLINE_VERSION */

__BEGIN_DECLS

char *abbreviate_file_name(char *str);
char *construct_human_size(const off_t size);
filesn_t count_dir(const char *dir, const int pop);

char from_hex(char c);
char *gen_date_suffix(const struct tm tm);
void gen_time_str(char *buf, const size_t size, const time_t curtime);
char *get_cwd(char *buf, const size_t buflen, const int check_workspace);
size_t hashme(const char *str, const int case_sensitive);
#if defined(__sun) && defined(ST_BTIME)
struct timespec get_birthtime(const char *filename);
#endif /* __sun && ST_BTIME */
char *get_cmd_path(const char *cmd);
int  get_rgb(char *hex, int *attr, int *r, int *g, int *b);
void clear_term_img(void);
mode_t get_dt(const mode_t mode);
int  get_link_ref(const char *link);
char *hex2rgb(char *hex);
char *normalize_path(char *src, const size_t src_len);
FILE *open_fread(const char *name, int *fd);
FILE *open_fwrite(const char *name, int *fd);
FILE *open_fappend(const char *name);
void press_any_key_to_continue(const int init_newline);
void print_file_name(char *fname, const int isdir);
int  read_octal(char *str);
void rl_ring_bell(void);
void set_fzf_preview_border_type(void);
int  should_expand_eln(const char *text);
char *url_encode(char *str);
char *url_decode(char *str);
int  utf8_bytes(unsigned char c);
filesn_t xatof(const char *s);
int  xatoi(const char *s);
const char *xitoa(long long n);
char xgetchar(void);
char *xgetenv(const char *s, const int alloc);
int  xmkdir(char *dir, const mode_t mode);
ssize_t xreadlink(const int fd, char *restrict path, char *restrict buf,
	const size_t bufsize);
void xregerror(const char *cmd_name, const char *pattern, const int errcode,
	const regex_t regexp, const int prompt_err);

__END_DECLS

#endif /* AUX_H */
