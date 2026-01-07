/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* config.h */

#ifndef CONFIG_H
#define CONFIG_H

#define DUMP_CONFIG_STR  0
#define DUMP_CONFIG_INT  1
#define DUMP_CONFIG_BOOL 2
#define DUMP_CONFIG_CHR  3
#define DUMP_CONFIG_STR_NO_QUOTE 4

__BEGIN_DECLS

int  config_edit(char **args);
int  create_bm_file(void);
int  create_kbinds_file(void);
int  create_main_config_file(char *file);
int  create_mime_file(char *file, const int new_prof);
void create_tmp_files(void);
void init_config(void);
int  reload_config(void);
int  config_reload(const char *arg);
void set_env(const int reload);
#ifndef _NO_FZF
int  get_fzf_border_type(const char *line);
int  get_fzf_height(char *line);
#endif /* !_NO_FZF */
void set_sel_file(void);
void set_time_style(char *line, char **str, const int ptime);

__END_DECLS

#endif /* CONFIG_H */
