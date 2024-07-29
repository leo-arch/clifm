/* properties.h -- Home of the p/pp, pc, oc, and stats commands */

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

#ifndef PROPERTIES_H
#define PROPERTIES_H

#ifdef LINUX_FILE_ATTRS
/* On some Linux distros, some of these flags are not defined (in linux/fs.h).
 * On Debian, for example, FS_DAX_FL, FS_CASEFOLD_FL, and FS_VERITY_FL are undefined.
 * Let's define them ourselves. */
# define XFS_SECRM_FL        0x00000001 /* Secure deletion */
# define XFS_UNRM_FL         0x00000002 /* Undelete */
# define XFS_COMPR_FL        0x00000004 /* Compress file */
# define XFS_SYNC_FL         0x00000008 /* Synchronous updates */
# define XFS_IMMUTABLE_FL    0x00000010 /* Immutable file */
# define XFS_APPEND_FL       0x00000020 /* writes to file may only append */
# define XFS_NODUMP_FL       0x00000040 /* do not dump file */
# define XFS_NOATIME_FL      0x00000080 /* do not update atime */
# define XFS_NOCOMP_FL       0x00000400 /* Don't compress */
# define XFS_ENCRYPT_FL      0x00000800 /* Encrypted file */
# define XFS_INDEX_FL        0x00001000 /* hash-indexed directory */
# define XFS_JOURNAL_DATA_FL 0x00004000 /* Reserved for ext3 */
# define XFS_NOTAIL_FL       0x00008000 /* file tail should not be merged */
# define XFS_DIRSYNC_FL      0x00010000 /* dirsync behaviour (directories only) */
# define XFS_TOPDIR_FL       0x00020000 /* Top of directory hierarchies*/
# define XFS_EXTENT_FL       0x00080000 /* Extents */
# define XFS_VERITY_FL       0x00100000 /* Verity protected inode */
# define XFS_NOCOW_FL        0x00800000 /* Do not cow file */
# define XFS_DAX_FL          0x02000000 /* Inode is DAX */
# define XFS_INLINE_DATA_FL  0x10000000 /* Reserved for ext4 */
# define XFS_PROJINHERIT_FL  0x20000000 /* Create with parents projid */
# define XFS_CASEFOLD_FL     0x40000000 /* Folder is case insensitive */
#endif /* LINUX_FILE_ATTRS */

/* Max size for a file size color (used by get_color_size()) */
#define MAX_SHADE_LEN 26 /* "\x1b[0;x;38;2;xxx;xxx;xxxm\0" */

/* Struct used by get_file_perms() */
struct perms_t {
	/* Field colors */
	char *cur;
	char *cuw;
	char *cux;
	char *cgr;
	char *cgw;
	char *cgx;
	char *cor;
	char *cow;
	char *cox;
	/* Field values */
	char ur;
	char uw;
	char ux;
	char gr;
	char gw;
	char gx;
	char or;
	char ow;
	char ox;
	char pad[7];
};

__BEGIN_DECLS

void do_stat_and_exit(const int full_stat);
void get_color_age(const time_t t, char *str, const size_t len);
void get_color_size(const off_t s, char *str, const size_t len);
struct perms_t
     get_file_perms(const mode_t mode);
int  properties_function(char **args, const int follow_link);
void print_analysis_stats(const off_t total, const off_t largest,
	const char *color, const char *name);
int  set_file_perms(char **args);
int  set_file_owner(char **args);

__END_DECLS

#endif /* PROPERTIES_H */
