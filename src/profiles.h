/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* profiles.h */

#ifndef PROFILES_H
#define PROFILES_H

__BEGIN_DECLS

int get_profile_names(void);
int profile_function(char **args);
int profile_set(const char *prof);
int validate_profile_name(const char *name);

__END_DECLS

#endif /* PROFILES_H */
