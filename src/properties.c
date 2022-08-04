/* properties.c -- functions to get files properties */

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

#include "helpers.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/capability.h>
#include <sys/sysmacros.h>
#endif
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <sys/types.h>
#include <wchar.h>

#include "aux.h"
#include "checks.h"
#include "colors.h"
#include "misc.h"

#ifndef major
# define major(x) ((x >> 8) & 0x7F)
#endif
#ifndef minor
# define minor(x) (x & 0xFF)
#endif

#ifndef DT_DIR
# define DT_DIR 4
#endif

static char *
get_link_color(char *name, int *link_dir, const int dsize)
{
	struct stat a;
	char *color = no_c;

	if (stat(name, &a) == -1)
		return color;

	if (S_ISDIR(a.st_mode)) {
		*link_dir = (follow_symlinks == 1 && dsize == 1) ? 1 : 0;
		color = get_dir_color(name, a.st_mode, a.st_nlink);
	} else {
		color = get_file_color(name, &a);
	}

	return color;
}

static int
get_properties(char *filename, const int dsize)
{
	if (!filename || !*filename)
		return EXIT_FAILURE;

	/* Remove ending slash and leading dot-slash (./) */
	size_t len = strlen(filename);
	if (len > 1 && filename[len - 1] == '/')
		filename[len - 1] = '\0';

	if (*filename == '.' && *(filename + 1) == '/' && *(filename + 2))
		filename += 2;

	/* Check file existence */
	struct stat attr;
	if (lstat(filename, &attr) == -1) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "pr: %s: %s\n", filename, strerror(errno));
		return EXIT_FAILURE;
	}

	/* Get file size */
	char *size_type = get_size_unit(FILE_SIZE);

	/* Get file type (and color) */
	char file_type = 0;
	char *linkname = (char *)NULL, *color = (char *)NULL;

	char *cnum_val = do_c, /* Color for properties octal value */
		 *ctype = dn_c,    /* Color for file type */
		 *cid = BOLD,      /* Color for UID and GID */
		 *csize = dz_c,    /* Color for file size */
		 *cdate = dd_c,    /* Color for dates */
		 *cbold = BOLD,    /* Just bold */
		 *cend = df_c;     /* Ending olor */

	if (attr.st_uid == user.uid || attr.st_gid == user.gid)
		cid = dg_c;

	char ext_color[MAX_COLOR];

	switch (attr.st_mode & S_IFMT) {
	case S_IFREG: {
		char *ext = (char *)NULL;
		file_type = '-';
		if (light_mode == 1)
			color = fi_c;
		else if (check_file_access(&attr) == 0)
			color = nf_c;
		else if (attr.st_mode & S_ISUID)
			color = su_c;
		else if (attr.st_mode & S_ISGID)
			color = sg_c;
		else {
#ifdef _LINUX_CAP
			cap_t cap = cap_get_file(filename);
			if (cap) {
				color = ca_c;
				cap_free(cap);
			} else if (attr.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
#else
			if (attr.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
#endif
				color = FILE_SIZE == 0 ? ee_c : ex_c;
			}

			else if (FILE_SIZE == 0)
				color = ef_c;
			else if (attr.st_nlink > 1)
				color = mh_c;
			else {
				ext = check_ext == 1 ? strrchr(filename, '.') : (char *)NULL;
				if (ext) {
					char *extcolor = get_ext_color(ext);
					if (extcolor) {
						sprintf(ext_color, "\x1b[%sm", extcolor);
						color = ext_color;
						extcolor = (char *)NULL;
					} else  { /* No matching extension found */
						color = fi_c;
					}
				} else {
					color = fi_c;
				}
			}
		}
	} break;
	case S_IFDIR:
		file_type = 'd';
		ctype = di_c;
		if (light_mode == 1)
			color = di_c;
		else if (check_file_access(&attr) == 0)
			color = nd_c;
		else
			color = get_dir_color(filename, attr.st_mode, attr.st_nlink);
		break;
	case S_IFLNK:
		file_type = 'l';
		ctype = ln_c;
		if (light_mode == 1) {
			color = ln_c;
		} else {
			linkname = realpath(filename, (char *)NULL);
			color = linkname ? ln_c : or_c;
		}
		break;
	case S_IFSOCK: file_type = 's';
		color = ctype = so_c;
		break;
	case S_IFBLK:
		file_type = 'b';
		color = ctype = bd_c;
		break;
	case S_IFCHR:
		file_type = 'c';
		color = ctype = cd_c;
		break;
	case S_IFIFO:
		file_type = 'p';
		color = ctype = pi_c;
		break;
	default:
		file_type = '?';
		color = no_c;
		break;
	}

	/* File permissions */
	char read_usr = '-', write_usr = '-', exec_usr = '-',
	     read_grp = '-', write_grp = '-', exec_grp = '-',
	     read_others = '-', write_others = '-', exec_others = '-';

	/* Colors for permissions bits */
	char *cu1 = dn_c, *cu2 = dn_c, *cu3 = dn_c,
		 *cg1 = dn_c, *cg2 = dn_c, *cg3 = dn_c,
		 *co1 = dn_c, *co2 = dn_c, *co3 = dn_c;

	mode_t val = (attr.st_mode & (mode_t)~S_IFMT);
	if (val & S_IRUSR) { read_usr = 'r'; cu1 = dr_c; }
	if (val & S_IWUSR) { write_usr = 'w'; cu2 = dw_c; }
	if (val & S_IXUSR) {
		exec_usr = 'x';
		cu3 = S_ISDIR(attr.st_mode) ? dxd_c : dxr_c;
	}

	if (val & S_IRGRP) { read_grp = 'r'; cg1 = dr_c; }
	if (val & S_IWGRP) { write_grp = 'w'; cg2 = dw_c; }
	if (val & S_IXGRP) {
		exec_grp = 'x';
		cg3 = S_ISDIR(attr.st_mode) ? dxd_c : dxr_c;
	}

	if (val & S_IROTH) { read_others = 'r'; co1 = dr_c; }
	if (val & S_IWOTH) { write_others = 'w'; co2 = dw_c; }
	if (val & S_IXOTH) {
		exec_others = 'x';
		co3 = S_ISDIR(attr.st_mode) ? dxd_c : dxr_c;
	}

	if (attr.st_mode & S_ISUID) {
		(val & S_IXUSR) ? (exec_usr = 's') : (exec_usr = 'S');
		cu3 = dp_c;
	}
	if (attr.st_mode & S_ISGID) {
		(val & S_IXGRP) ? (exec_grp = 's') : (exec_grp = 'S');
		cg3 = dp_c;
	}
	if (attr.st_mode & S_ISVTX) {
		(val & S_IXOTH) ? (exec_others = 't'): (exec_others = 'T');
		co3 = dp_c;
	}

	if (colorize == 0) {
		cdate = df_c;
		csize = df_c;
		cid = df_c;
		cnum_val = df_c;
		color = df_c;
		ctype = df_c;
		cend = df_c;
		cbold = df_c;
		cu1 = df_c;
		cu2 = df_c;
		cu3 = df_c;
		cg1 = df_c;
		cg2 = df_c;
		cg3 = df_c;
		co1 = df_c;
		co2 = df_c;
		co3 = df_c;
	}

	/* Get number of links to the file */
	nlink_t link_n = attr.st_nlink;

	/* Get owner and group names */
	uid_t owner_id = attr.st_uid; /* owner ID */
	gid_t group_id = attr.st_gid; /* group ID */
	struct group *group;
	struct passwd *owner;
	group = getgrgid(group_id);
	owner = getpwuid(owner_id);

	char *wname = (char *)NULL;
	size_t wlen = wc_xstrlen(filename);
	if (wlen == 0)
		wname = truncate_wname(filename);

	char *t_ctype = savestring(ctype, strlen(ctype));
	remove_bold_attr(&t_ctype);

	/* Print file properties */
	printf("(%s%04o%s)%s%c%s/%s%c%s%c%s%c%s/%s%c%s%c%s%c%s/%s%c%s%c%s%c%s%s "
		   "Links: %s%zu%s ",
	    cnum_val, attr.st_mode & 07777, cend,
	    t_ctype, file_type, cend,
	    cu1, read_usr, cu2, write_usr, cu3, exec_usr, cend,
	    cg1, read_grp, cg2, write_grp, cg3, exec_grp, cend,
	    co1, read_others, co2, write_others, co3, exec_others, cend,
	    is_acl(filename) ? "+" : "", cbold, (size_t)link_n, cend);

	free(t_ctype);

	int link_to_dir = 0;

	if (file_type == 0) {
		printf("\tName: %s%s%s\n", no_c, wname ? wname : filename, df_c);
	} else if (file_type != 'l') {
		printf("\tName: %s%s%s\n", color, wname ? wname : filename, df_c);
	} else if (linkname) {
		char *link_color = get_link_color(linkname, &link_to_dir, dsize);
		printf("\tName: %s%s%s -> %s%s%s\n", color, wname ? wname : filename, df_c,
			link_color, linkname, NC);
		free(linkname);
	} else { /* Broken link */
		char link[PATH_MAX] = "";
		ssize_t ret = readlinkat(AT_FDCWD, filename, link, PATH_MAX);
		if (ret) {
			printf(_("\tName: %s%s%s -> %s (broken link)\n"), color, wname ? wname : filename,
			    df_c, link);
		} else {
			printf("\tName: %s%s%s -> ???\n", color, wname ? wname : filename, df_c);
		}
	}

	free(wname);

	/* Stat information */
#define MAX_TIME_STR 128
/* Our resulting time string won't go usually beyond 27 chars. But since this length
 * is locale dependent (at least %b), let's use a much larger buffer size */

	/* Last modification time */
	time_t time = (time_t)attr.st_mtime;
	struct tm tm;
	localtime_r(&time, &tm);
	char mod_time[MAX_TIME_STR];

	if (time) {
		strftime(mod_time, sizeof(mod_time), "%b %d %H:%M:%S %Y %z", &tm);
	} else {
		*mod_time = '-';
		mod_time[1] = '\0';
	}

	/* Last access time */
	time = (time_t)attr.st_atime;
	localtime_r(&time, &tm);
	char access_time[MAX_TIME_STR];

	if (time) {
		strftime(access_time, sizeof(access_time), "%b %d %H:%M:%S %Y %z", &tm);
	} else {
		*access_time = '-';
		access_time[1] = '\0';
	}

	/* Last properties change time */
	time = (time_t)attr.st_ctime;
	localtime_r(&time, &tm);
	char change_time[MAX_TIME_STR];
	if (time) {
		strftime(change_time, sizeof(change_time), "%b %d %H:%M:%S %Y %z", &tm);
	} else {
		*change_time = '-';
		change_time[1] = '\0';
	}

	/* Creation (birth) time */
#if defined(HAVE_ST_BIRTHTIME) || defined(__BSD_VISIBLE)
#ifdef __OpenBSD__
	time = attr.__st_birthtim.tv_sec;
#else
	time = attr.st_birthtime;
#endif
	localtime_r(&time, &tm);
	char creation_time[MAX_TIME_STR];
	if (!time) {
		*creation_time = '-';
		creation_time[1] = '\0';
	} else {
		strftime(creation_time, sizeof(creation_time), "%b %d %H:%M:%S %Y %z", &tm);
	}
#elif defined(_STATX)
	struct statx attrx;
	statx(AT_FDCWD, filename, AT_SYMLINK_NOFOLLOW, STATX_BTIME, &attrx);
	time = (time_t)attrx.stx_btime.tv_sec;
	localtime_r(&time, &tm);
	char creation_time[MAX_TIME_STR];

	if (!time) {
		*creation_time = '-';
		creation_time[1] = '\0';
	} else {
		strftime(creation_time, sizeof(creation_time), "%b %d %H:%M:%S %Y %z", &tm);
	}
#endif /* _STATX */

	if (colorize == 1)
		printf("%s", BOLD);
	switch (file_type) {
	case 'd': printf(_("Directory")); break;
	case 's': printf(_("Socket")); break;
	case 'l': printf(_("Symbolic link")); break;
	case 'b': printf(_("Block special file")); break;
	case 'c': printf(_("Character special file")); break;
	case 'p': printf(_("Fifo")); break;
	case '-': printf(_("Regular file")); break;
	default: break;
	}
	if (colorize == 1)
		printf("%s", cend);

#if defined(__OpenBSD__) || defined(__APPLE__) || defined(__i386__) || defined(__ANDROID__)
	printf(_("\tBlocks: %s%lld%s"), cbold, attr.st_blocks, cend);
#else
	printf(_("\tBlocks: %s%ld%s"), cbold, attr.st_blocks, cend);
#endif
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) \
|| defined(__APPLE__) || defined(__HAIKU__)
	printf(_("\tIO Block: %s%d%s"), cbold, attr.st_blksize, cend);
#else
	printf(_("\tIO Block: %s%ld%s"), cbold, attr.st_blksize, cend);
#endif
#if defined(__OpenBSD__) || defined(__APPLE__) || defined(__i386__) || defined(__ANDROID__)
	printf(_("\tInode: %s%llu%s\n"), cbold, attr.st_ino, cend);
#else
	printf(_("\tInode: %s%zu%s\n"), cbold, attr.st_ino, cend);
#endif
#if defined(__OpenBSD__) || defined(__APPLE__) || defined(__HAIKU__)
	printf(_("Device: %s%d%s"), cbold, attr.st_dev, cend);
#elif defined(__i386__) || defined(__ANDROID__)
	printf(_("Device: %s%lld%s"), cbold, attr.st_dev, cend);
#else
	printf(_("Device: %s%zu%s"), cbold, attr.st_dev, cend);
#endif
	printf(_("\tUid: %s%u (%s)%s"), cid, attr.st_uid, !owner ? _("unknown")
			: owner->pw_name, cend);
	printf(_("\tGid: %s%u (%s)%s\n"), cid, attr.st_gid, !group ? _("unknown")
			: group->gr_name, cend);

	/* Print file timestamps */
	printf(_("Access: \t%s%s%s\n"), cdate, access_time, cend);
	printf(_("Modify: \t%s%s%s\n"), cdate, mod_time, cend);
	printf(_("Change: \t%s%s%s\n"), cdate, change_time, cend);

#if defined(HAVE_ST_BIRTHTIME) || defined(__BSD_VISIBLE) || defined(_STATX)
	printf(_("Birth: \t\t%s%s%s\n"), cdate, creation_time, cend);
#endif

	/* Print size */
	if (!S_ISDIR(attr.st_mode) && link_to_dir == 0) {
		printf(_("Size: \t\t%s%s%s\n"), csize, size_type ? size_type : "?", cend);
		goto END;
	}

	if (dsize == 0)
		goto END;

	fputs(_("Total size: \t"), stdout);
	char _path[PATH_MAX]; *_path = '\0';
	if (link_to_dir == 1)
		snprintf(_path, sizeof(_path), "%s/", filename);
	fputs(HIDE_CURSOR, stdout);
	fputs("Calculating... ", stdout);
	fflush(stdout);
	off_t total_size = dir_size(*_path ? _path : filename);
	MOVE_CURSOR_LEFT(15);
	ERASE_TO_RIGHT;
	fputs(UNHIDE_CURSOR, stdout);
	if (S_ISDIR(attr.st_mode) && attr.st_nlink == 2 && total_size == 4)
		total_size = 0; /* Empty directory */
	if (total_size == -1) {
		puts("?");
		goto END;
	}

	char *human_size = get_size_unit(total_size * (xargs.si == 1 ? 1000 : 1024));
	if (human_size) {
		printf("%s%s%s\n", csize, human_size, cend);
		free(human_size);
	} else {
		puts("?");
	}

END:
	free(size_type);
	return EXIT_SUCCESS;
}

/* Compose the properties line for the current file name
 * This function is called by list_dir(), in listing.c for each file name
 * in the current directory when running on long view mode */
int
print_entry_props(const struct fileinfo *props, size_t max, const size_t ug_max, const size_t ino_max)
{
	/* Let's get file attributes and the corresponding colors */

	char file_type = 0; /* File type indicator */
	char *ctype = dn_c, /* Color for file type */
		 *cdate = dd_c, /* Color for dates */
		 *cid = df_c,   /* Color for UID and GID */
		 *csize = props->dir ? dz_c : df_c, /* Directories size */
		 *cend = df_c;  /* Ending Color */

	if (props->uid == user.uid || props->gid == user.gid)
		cid = dg_c;

	switch (props->mode & S_IFMT) {
	case S_IFREG: file_type =  '-'; break;
	case S_IFDIR: file_type =  'd'; ctype = di_c; break;
	case S_IFLNK: file_type =  'l'; ctype = ln_c; break;
	case S_IFSOCK: file_type = 's'; ctype = so_c; break;
	case S_IFBLK: file_type =  'b'; ctype = bd_c; break;
	case S_IFCHR: file_type =  'c'; ctype = cd_c; break;
	case S_IFIFO: file_type =  'p'; ctype = pi_c; break;
	default: file_type =       '?'; break;
	}

	/* File permissions bit char */
	char read_usr = '-', write_usr = '-', exec_usr = '-',
	     read_grp = '-', write_grp = '-', exec_grp = '-',
	     read_others = '-', write_others = '-', exec_others = '-';

	/* Colors for each field of the permissions string */
	char *cu1 = dn_c, *cu2 = dn_c, *cu3 = dn_c,
		 *cg1 = dn_c, *cg2 = dn_c, *cg3 = dn_c,
		 *co1 = dn_c, *co2 = dn_c, *co3 = dn_c;

	mode_t val = (props->mode & (mode_t)~S_IFMT);
	if (val & S_IRUSR) { read_usr = 'r'; cu1 = dr_c; }
	if (val & S_IWUSR) { write_usr = 'w'; cu2 = dw_c; }
	if (val & S_IXUSR) { exec_usr = 'x'; cu3 = props->dir ? dxd_c : dxr_c; }

	if (val & S_IRGRP) { read_grp = 'r'; cg1 = dr_c; }
	if (val & S_IWGRP) { write_grp = 'w'; cg2 = dw_c; }
	if (val & S_IXGRP) { exec_grp = 'x'; cg3 = props->dir ? dxd_c : dxr_c; }

	if (val & S_IROTH) { read_others = 'r'; co1 = dr_c; }
	if (val & S_IWOTH) { write_others = 'w'; co2 = dw_c; }
	if (val & S_IXOTH) { exec_others = 'x'; co3 = props->dir ? dxd_c : dxr_c; }

	if (props->mode & S_ISUID) {
		(val & S_IXUSR) ? (exec_usr = 's') : (exec_usr = 'S');
		cu3 = dp_c;
	}
	if (props->mode & S_ISGID) {
		(val & S_IXGRP) ? (exec_grp = 's') : (exec_grp = 'S');
		cg3 = dp_c;
	}
	if (props->mode & S_ISVTX) {
		(val & S_IXOTH) ? (exec_others = 't') : (exec_others = 'T');
		co3 = dp_c;
	}

	if (colorize == 0) {
		cdate = df_c;
		csize = df_c;
		cid = df_c;
		ctype = df_c;
		cend = df_c;
		cu1 = df_c;
		cu2 = df_c;
		cu3 = df_c;
		cg1 = df_c;
		cg2 = df_c;
		cg3 = df_c;
		co1 = df_c;
		co2 = df_c;
		co3 = df_c;
	}

	char *t_ctype = savestring(ctype, strlen(ctype));
	remove_bold_attr(&t_ctype);

	/* Let's compose each properties field individually to be able to
	 * print only the desired ones. This is specified via the PropFields
	 * option in the config file */

				/* ###########################
				 * #      1. FILE NAME       #
				 * ########################### */

	/*  If file name length is greater than max, truncate it
	 * to max (later a tilde (~) will be appended to let the user know
	 * the file name was truncated) */
	char tname[PATH_MAX * sizeof(wchar_t)];
	int trim = 0;

	/* Handle file names with embedded control characters */
	size_t plen = props->len;
	char *wname = (char *)NULL;
	if (props->len == 0) {
		wname = truncate_wname(props->name);
		plen = wc_xstrlen(wname);
	}

	size_t cur_len = (size_t)DIGINUM(files + 1) + 1 + plen;
#ifndef _NO_ICONS
	if (icons) {
		cur_len += 3;
		max += 3;
	}
#endif

	int diff = 0;
	if (cur_len > max) {
		int rest = (int)(cur_len - max);
		trim = 1;
		xstrsncpy(tname, wname ? wname : props->name, (PATH_MAX * sizeof(wchar_t)) - 1);
		int a = (int)plen - rest - 1;
		if (a < 0)
			a = 0;
		if (unicode)
			diff = u8truncstr(tname, (size_t)(a));
		else
			tname[a] = '\0';

		cur_len -= (size_t)rest;
	}

	/* Calculate pad for each file name */
	int pad = (int)(max - cur_len);
	if (pad < 0)
		pad = 0;

	if (!trim || !unicode)
		mbstowcs((wchar_t *)tname, wname ? wname : props->name, PATH_MAX);

	free(wname);

	char trim_diff[14];
	*trim_diff = '\0';
	if (diff > 0)
		snprintf(trim_diff, sizeof(trim_diff), "\x1b[%dC", diff);

				/* ###########################
				 * #     2. PERMISSIONS      #
				 * ########################### */

	char attr_s[(MAX_COLOR * 14) + 16]; /* 14 colors + 15 single chars + NUL byte */
	if (prop_fields.perm == PERM_SYMBOLIC) {
		snprintf(attr_s, sizeof(attr_s),
			"%s%c%s/%s%c%s%c%s%c%s/%s%c%s%c%s%c%s/%s%c%s%c%s%c%s ",
			t_ctype, file_type, cend,
			cu1, read_usr, cu2, write_usr, cu3, exec_usr, cend,
			cg1, read_grp, cg2, write_grp, cg3, exec_grp, cend,
			co1, read_others, co2, write_others, co3, exec_others, cend);
//			is_acl(props->name) ? "+" : "");
	} else if (prop_fields.perm == PERM_NUMERIC) {
		snprintf(attr_s, sizeof(attr_s), "%s%04o%s ", do_c, props->mode & 07777, cend);
	} else {
		*attr_s = '\0';
	}

				/* ###########################
				 * #    3. USER/GROUP IDS    #
				 * ########################### */

	int ug_pad = 0, u = 0, g = 0;
	/* Let's suppose that IDs go up to 999 billions (12 digits)
	 * 2 IDs + pad (at most 12 more digits) == 36
	 * 39 == 36 + colon + space + NUL byte */
	char id_s[(MAX_COLOR * 2) + 39];
	if (prop_fields.ids == 1) {
		/* Calculate right pad for UID:GID string */
		u = DIGINUM(props->uid), g = DIGINUM(props->gid);
		if (u + g < (int)ug_max)
			ug_pad = (int)ug_max - u;
		snprintf(id_s, sizeof(id_s), "%s%u:%-*u%s ", cid, props->uid, ug_pad, props->gid, cend);
	} else {
		*id_s = '\0';
	}

				/* ###############################
				 * #        4. FILE TIME         #
				 * ############################### */

	/* Whether time is access, modification, or status change, this value is set
	 * by list_dir, in listing.c (file_info[n].ltime) before calling this function */
	char file_time[128];
	char time_s[128 + (MAX_COLOR * 2) + 2]; /* mod_time + 2 colors + space + NUL byte */
	if (prop_fields.time != 0) {
		if (props->ltime) {
			struct tm t;
			localtime_r(&props->ltime, &t);
			snprintf(file_time, sizeof(file_time), "%d-%02d-%02d %02d:%02d", t.tm_year + 1900,
				t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min);
		} else {
			strcpy(file_time, "-               ");
		}
		snprintf(time_s, sizeof(time_s), "%s%s%s ", cdate, *file_time ? file_time : "?", cend);
	} else {
		*file_time = '\0';
		*time_s = '\0';
	}

				/* ###########################
				 * #       5. FILE SIZE      #
				 * ########################### */

	/* size_s is either file size or "major,minor" IDs in case of special
	 * files (char and block devs) */
	char *size_type = (char *)NULL;
	/* get_size_unit() returns a string of at most MAX_UNIT_SIZE chars (see aux.h) */
	char size_s[MAX_UNIT_SIZE + (MAX_COLOR * 2) + 1];
	if (prop_fields.size == 1) {
		if (props->rdev == 0 || xargs.disk_usage_analyzer == 1) {
			if (full_dir_size == 1 && props->dir == 1)
				size_type = get_size_unit(props->size * (xargs.si == 1 ? 1000 : 1024));
			else
				size_type = get_size_unit(props->size);

			snprintf(size_s, sizeof(size_s), "%s%s%s", csize, size_type
				? size_type : "?", cend);
		} else {
			snprintf(size_s, sizeof(size_s), "%d,%d", major(props->rdev),
				minor(props->rdev));
		}
	} else {
		*size_s = '\0';
	}

	char ino_s[12 + 1]; /* Max inode number able to hold: 999 billions! */
	*ino_s = '\0';
	if (prop_fields.inode == 1)
#if defined (__APPLE__) || defined(__OpenBSD__) || defined(__i386__)
		snprintf(ino_s, sizeof(ino_s), "%-*llu ", (int)ino_max, props->inode);
#elif defined(__ANDROID__)
		snprintf(ino_s, sizeof(ino_s), "%-*lu ", (int)ino_max, props->inode);
#else
		snprintf(ino_s, sizeof(ino_s), "%-*zu ", (int)ino_max, props->inode);
#endif

	/* Print stuff */

#ifndef _NO_ICONS
	printf("%s%s%c%s%s%ls%s%s%-*s%s\x1b[0m%s%c\x1b[0m " /* File name*/
		   "%s" /* Inode */
		   "%s" /* Permissions */
		   "%s" /* User and group ID */
		   "%s" /* Time */
		   "%s\n", /* Size / device info */
		colorize ? props->icon_color : "",
		icons ? props->icon : "", icons ? ' ' : 0, df_c,

		colorize ? props->color : "",
		(wchar_t *)tname, trim_diff,
		light_mode ? "" : df_c, pad, "", df_c,
		trim ? tt_c : "", trim ? '~' : 0,

		prop_fields.inode ? ino_s : "",
		prop_fields.perm != 0 ? attr_s : "",
		prop_fields.ids == 1 ? id_s : "",
		prop_fields.time != 0 ? time_s : "",
		prop_fields.size == 1 ? size_s : "");
#else
	printf("%s%ls%s%s%-*s%s\x1b[0m%s%c\x1b[0m " /* File name*/
		   "%s" /* Inode */
		   "%s" /* Permissions */
		   "%s" /* User and group ID */
		   "%s" /* Time */
		   "%s\n", /* Size / device info */

	    colorize ? props->color : "",
		(wchar_t *)tname, trim_diff,
	    light_mode ? "" : df_c, pad, "", df_c,
	    trim ? tt_c : "", trim ? '~' : 0,

		prop_fields.inode ? ino_s : "",
		prop_fields.perm != 0 ? attr_s : "",
		prop_fields.ids == 1 ? id_s : "",
		prop_fields.time != 0 ? time_s : "",
		prop_fields.size == 1 ? size_s : "");
#endif

	free(t_ctype);
	free(size_type);

	return EXIT_SUCCESS;
}

int
properties_function(char **args)
{
	if (!args)
		return EXIT_FAILURE;

	size_t i;
	int exit_status = EXIT_SUCCESS;
	int _dir_size = 0;

	if (*args[0] == 'p' && args[0][1] == 'p' && !args[0][2])
		_dir_size = 1;

	/* If "pr file..." */
	for (i = 1; i <= args_n; i++) {
		if (strchr(args[i], '\\')) {
			char *deq_file = dequote_str(args[i], 0);
			if (!deq_file) {
				_err(ERR_NO_STORE, NOPRINT_PROMPT, _("pr: %s: Error dequoting file name\n"), args[i]);
				exit_status = EXIT_FAILURE;
				continue;
			}

			strcpy(args[i], deq_file);
			free(deq_file);
		}

		if (get_properties(args[i], _dir_size) != 0)
			exit_status = EXIT_FAILURE;
	}

	return exit_status;
}
