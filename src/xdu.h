/* xdu.h */

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

#ifndef CLIFM_XDU_H
#define CLIFM_XDU_H

__BEGIN_DECLS

void dir_info(const char *dir, const int first_level, struct dir_info_t *info);
#ifdef USE_DU1
off_t dir_size(char *dir, const int first_level, int *status);
#else
off_t dir_size(const char *dir, const int first_level, int *status);
#endif /* USE_DU1 */

__END_DECLS

#endif /* CLIFM_XDU_H */
