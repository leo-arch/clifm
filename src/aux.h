/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* aux.h */

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
void clear_term_img(void);
char *construct_human_size(const off_t size);
filesn_t count_dir(const char *dir, const int pop);
char from_hex(const char c);
char *gen_backup_file(const char *file, const int human);
char *gen_date_suffix(const struct tm tm, const int human);
void gen_time_str(char *buf, const size_t size, const time_t curtime);
#if defined(__sun) && defined(ST_BTIME)
struct timespec get_birthtime(const char *filename);
#endif /* __sun && ST_BTIME */
char *get_cmd_path(const char *cmd);
char *get_cwd(char *buf, const size_t buflen, const int check_workspace);
mode_t get_dt(const mode_t mode);
int  get_link_ref(const char *link);
int  get_rgb(const char *hex, int *attr, int *r, int *g, int *b);
size_t hashme(const char *str, const int case_sensitive);
char *hex2rgb(const char *hex);
int  is_cmd_in_path(const char *cmd);
char *make_filename_unique(const char *file);
char *normalize_path(char *src, const size_t src_len);
int  octal2int(const char *restrict str);
int  open_config_file(char *app, char *file);
FILE *open_fread(const char *name, int *fd);
FILE *open_fwrite(const char *name, int *fd);
FILE *open_fappend(const char *name);
void press_any_key_to_continue(const int init_newline);
void print_file_name(char *fname, const int isdir);
void rl_ring_bell(void);
void set_fzf_preview_border_type(void);
int  should_expand_eln(const char *text, char *cmd_name);
char *url_encode(const char *str, const int file_uri);
char *url_decode(const char *str);
int  utf8_bytes(unsigned char c);
filesn_t xatof(const char *s);
int  xatoi(const char *s);
const char *xitoa(long long n);
char xgetchar(void);
char *xgetenv(const char *s, const int alloc);
void *xmemrchr(const void *s, const int c, size_t n);
int  xmkdir(const char *dir, const mode_t mode);
ssize_t xreadlink(const int fd, char *restrict path, char *restrict buf,
	const size_t bufsize);
void xregerror(const char *cmd_name, const char *pattern, const int errcode,
	const regex_t regexp, const int prompt_err);

__END_DECLS

#endif /* AUX_H */
