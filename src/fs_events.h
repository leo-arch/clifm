/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* fs_events.h */

#ifndef FS_EVENTS_H
#define FS_EVENTS_H

__BEGIN_DECLS

void set_events_checker(void);
void check_fs_events(const int is_internal_cmd);

__END_DECLS

#endif /* FS_EVENTS_H */
