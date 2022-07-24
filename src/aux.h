/* aux.h */

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

#ifndef AUX_H
#define AUX_H

#include <time.h>

/* Max size type length for the value returned by get_size_type() */
#define MAX_UNIT_SIZE 10 /* "1023.99YB\0" */

int __expand_eln(const char *text);
char *abbreviate_file_name(char *str);
void close_fstream(FILE *fp, int fd);
int count_dir(const char *dir, int pop);
off_t dir_size(char *dir);
char from_hex(char c);
//char *from_octal(char *s);
char *gen_date_suffix(struct tm tm);
char *get_cmd_path(const char *cmd);
int get_cursor_position(int *c, int *r);
mode_t get_dt(const mode_t mode);
/*int *get_hex_num(const char *str); */
int get_link_ref(const char *link);
char *get_size_unit(off_t size);
//int get_term_bgcolor(const int ifd, const int ofd);
char *hex2rgb(char *hex);
char *normalize_path(char *src, size_t src_len);
FILE *open_fstream_r(char *name, int *fd);
FILE *open_fstream_w(char *name, int *fd);
int read_octal(char *str);
void remove_bold_attr(char **str);
void rl_ring_bell(void);
char *url_encode(char *str);
char *url_decode(char *str);
int xatoi(const char *s);
char *xitoa(int n);
char xgetchar(void);
int xmkdir(char *dir, mode_t mode);

/* Some memory wrapper functions */
void *xrealloc(void *ptr, size_t size);
void *xcalloc(size_t nmemb, size_t size);
void *xnmalloc(size_t nmemb, size_t size);

#endif /* AUX_H */
