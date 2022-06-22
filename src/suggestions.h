/* suggestions.h */

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

#ifndef SUGGESTIONS_H
#define SUGGESTIONS_H

int check_cmds(const char *str, const size_t len, const int print);
void clear_suggestion(const int free_sug);
void free_suggestion(void);
void print_suggestion(char *str, size_t offset, char *color);
void remove_suggestion_not_end(void);
int recover_from_wrong_cmd(void);
int rl_suggestions(const unsigned char c);

#endif /* SUGGESTIONS_H */
