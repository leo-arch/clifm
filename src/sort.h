/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2025 L. Abramovich <leo.clifm@outlook.com>
*/

/* sort.h */

#ifndef SORT_H
#define SORT_H

/* Needed by qsort(3) to use compare_string() as comparison function */
typedef int QSFUNC(const void *, const void *);

__BEGIN_DECLS

int  alphasort_insensitive(const struct dirent **a, const struct dirent **b);
int  compare_strings(char **s1, char **s2);
int  entrycmp(const void *a, const void *b);
char *num_to_sort_name(const int n, const int abbrev);
void print_sort_method(void);
int  skip_files(const struct dirent *ent);
int  sort_function(char **arg);
int  xalphasort(const struct dirent **a, const struct dirent **b);

__END_DECLS

#endif /* SORT_H */
