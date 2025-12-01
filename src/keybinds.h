/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2025 L. Abramovich <leo.clifm@outlook.com>
*/

/* keybinds.h */

#ifndef KEYBINDS_H
#define KEYBINDS_H

/* Let's use these word delimiters to print first suggested word
 * and to delete last typed word */
#define WORD_DELIMITERS " /.-_=,:;@+*&$#<>%~|({[]})¿?¡!"

__BEGIN_DECLS

void disable_rl_conflicting_kbinds(void);
int  keybind_exec_cmd(char *str);
int  kbinds_function(char **args);
int  load_keybinds(void);
void readline_kbinds(void);
int  rl_quit(int count, int key);
int  rl_toggle_dirs_first(int count, int key);
int  rl_toggle_hidden_files(int count, int key);
int  rl_toggle_light_mode(int count, int key);
int  rl_toggle_long_view(int count, int key);
int  rl_toggle_max_filename_len(int count, int key);

__END_DECLS

#endif /* KEYBINDS_H */
