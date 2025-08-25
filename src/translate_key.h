/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2016-2025, L. Abramovich <leo.clifm@outlook.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
*/

/* translate_key.h */

#ifndef TRANSLATE_KEY_H
#define TRANSLATE_KEY_H

#define ALT_CSI        0x9b /* 8-bit CSI (alternate sequence) */
#define CSI_INTRODUCER 0x5b /* [ */
#define SS3_INTRODUCER 0x4f /* O */

__BEGIN_DECLS

char *translate_key(char *str);

__END_DECLS

#endif /* TRANSLATE_KEY_H */
