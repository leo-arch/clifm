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
#endif
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <sys/types.h>
#include <wchar.h>

#include "aux.h"
#include "checks.h"
#include "colors.h"

#ifndef DT_DIR
# define DT_DIR 4
#endif

static int
get_properties(char *filename, const int dsize)
{
	if (!filename || !*filename)
		return EXIT_FAILURE;

	/* Remove ending slash and leading dot-slash (./) */
	size_t len = strlen(filename);
	if (filename[len - 1] == '/')
		filename[len - 1] = '\0';

	if (*filename == '.' && *(filename + 1) == '/' && *(filename + 2))
		filename += 2;

	/* Check file existence */
	struct stat attr;
	if (lstat(filename, &attr) == -1) {
		fprintf(stderr, "%s: pr: '%s': %s\n", PROGRAM_NAME, filename,
		    strerror(errno));
		return EXIT_FAILURE;
	}

	/* Get file size */
	char *size_type = get_size_unit(FILE_SIZE);

	/* Get file type (and color) */
	char file_type = 0;
	char *linkname = (char *)NULL, *color = (char *)NULL;

	char *cnum_val = PR_NUM_VAL, /* Color for properties octal value */
		 *ctype = PR_NONE,       /* Color for file type */
		 *cid = PR_ID,           /* Color for UID and GID */
		 *csize = PR_SIZE,       /* Color for file size */
		 *cdate = PR_DATE,       /* Color for dates */
		 *cbold = BOLD,          /* Just bold */
		 *cend = NC;             /* Ending olor */

	char ext_color[MAX_COLOR];

	switch (attr.st_mode & S_IFMT) {
	case S_IFREG: {
		char *ext = (char *)NULL;
		file_type = '-';
		if (light_mode)
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
				if (FILE_SIZE == 0)
					color = ee_c;
				else
					color = ex_c;
			}

			else if (FILE_SIZE == 0)
				color = ef_c;
			else if (attr.st_nlink > 1)
				color = mh_c;
			else {
				ext = strrchr(filename, '.');
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
		if (light_mode)
			color = di_c;
		else if (check_file_access(&attr) == 0)
			color = nd_c;
		else
			color = get_dir_color(filename, attr.st_mode);
		break;
	case S_IFLNK:
		file_type = 'l';
		ctype = ln_c;
		if (light_mode) {
			color = ln_c;
		} else {
			linkname = realpath(filename, (char *)NULL);
			if (linkname)
				color = ln_c;
			else
				color = or_c;
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

	/* Get file permissions */
	char read_usr = '-', write_usr = '-', exec_usr = '-',
	     read_grp = '-', write_grp = '-', exec_grp = '-',
	     read_others = '-', write_others = '-', exec_others = '-';

	char *cu1 = PR_NONE, *cu2 = PR_NONE, *cu3 = PR_NONE,
		 *cg1 = PR_NONE, *cg2 = PR_NONE, *cg3 = PR_NONE,
		 *co1 = PR_NONE, *co2 = PR_NONE, *co3 = PR_NONE;

	mode_t val = (attr.st_mode & (mode_t)~S_IFMT);
	if (val & S_IRUSR) { read_usr = 'r'; cu1 = PR_READ; }
	if (val & S_IWUSR) { write_usr = 'w'; cu2 = PR_WRITE; }
	if (val & S_IXUSR) { exec_usr = 'x'; cu3 = PR_EXEC; }

	if (val & S_IRGRP) { read_grp = 'r'; cg1 = PR_READ; }
	if (val & S_IWGRP) { write_grp = 'w'; cg2 = PR_WRITE; }
	if (val & S_IXGRP) { exec_grp = 'x'; cg3 = PR_EXEC; }

	if (val & S_IROTH) { read_others = 'r'; co1 = PR_READ; }
	if (val & S_IWOTH) { write_others = 'w'; co2 = PR_WRITE; }
	if (val & S_IXOTH) { exec_others = 'x'; co3 = PR_EXEC; }

	if (attr.st_mode & S_ISUID) {
		(val & S_IXUSR) ? (exec_usr = 's') : (exec_usr = 'S');
		cu3 = PR_SPECIAL;
	}
	if (attr.st_mode & S_ISGID) {
		(val & S_IXGRP) ? (exec_grp = 's') : (exec_grp = 'S');
		cg3 = PR_SPECIAL;
	}
	if (attr.st_mode & S_ISVTX) {
		(val & S_IXOTH) ? (exec_others = 't'): (exec_others = 'T');
		co3 = PR_SPECIAL;
	}

	if (colorize == 0 || props_color == 0) {
		cdate = EMPTY_STR;
		csize = EMPTY_STR;
		cid = EMPTY_STR;
		cnum_val = EMPTY_STR;
		color = EMPTY_STR;
		ctype = EMPTY_STR;
		cend = EMPTY_STR;
		cbold = EMPTY_STR;
		cu1 = EMPTY_STR;
		cu2 = EMPTY_STR;
		cu3 = EMPTY_STR;
		cg1 = EMPTY_STR;
		cg2 = EMPTY_STR;
		cg3 = EMPTY_STR;
		co1 = EMPTY_STR;
		co2 = EMPTY_STR;
		co3 = EMPTY_STR;
	}

	/* Get number of links to the file */
	nlink_t link_n = attr.st_nlink;

	/* Get modification time */
	time_t time = (time_t)attr.st_mtim.tv_sec;
	struct tm tm;
	localtime_r(&time, &tm);
	char mod_time[128];

	if (time) {
		strftime(mod_time, sizeof(mod_time), "%b %d %H:%M:%S %Y", &tm);
	} else {
		*mod_time = '-';
		mod_time[1] = '\0';
	}

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

	/* Print file properties */
	printf("(%s%04o%s)%s%c%s/%s%c%s%c%s%c%s/%s%c%s%c%s%c%s/%s%c%s%c%s%c%s%s "
//		   "%zu %s%s %s%s %s%s%s %s%s%s ",
		   "Links: %s%zu%s ",
	    cnum_val, attr.st_mode & 07777, cend,
	    ctype, file_type, cend,
	    cu1, read_usr, cu2, write_usr, cu3, exec_usr, cend,
	    cg1, read_grp, cg2, write_grp, cg3, exec_grp, cend,
	    co1, read_others, co2, write_others, co3, exec_others, cend,
	    is_acl(filename) ? "+" : "", cbold, (size_t)link_n, cend);
/*	    cid, !owner ? _("unknown") : owner->pw_name,
	    !group ? _("unknown") : group->gr_name, cend,
	    csize, size_type ? size_type : "?", cend,
	    cdate, mod_time[0] != '\0' ? mod_time : "?", cend); */

	if (file_type && file_type != 'l') {
		printf("\tName: %s%s%s\n", color, wname ? wname : filename, df_c);
	} else if (linkname) {
		printf("\tName: %s%s%s -> %s\n", color, wname ? wname : filename, df_c, linkname);
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
	/* Last access time */
	time = (time_t)attr.st_atim.tv_sec;
	localtime_r(&time, &tm);
	char access_time[128];

	if (time) {
		strftime(access_time, sizeof(access_time), "%b %d %H:%M:%S %Y", &tm);
	} else {
		*access_time = '-';
		access_time[1] = '\0';
	}

	/* Last properties change time */
	time = (time_t)attr.st_ctim.tv_sec;
	localtime_r(&time, &tm);
	char change_time[128];
	if (time) {
		strftime(change_time, sizeof(change_time), "%b %d %H:%M:%S %Y", &tm);
	} else {
		*change_time = '-';
		change_time[0] = '\0';
	}

	/* Get creation (birth) time */
#if defined(HAVE_ST_BIRTHTIME) || defined(__BSD_VISIBLE)
#ifdef __OpenBSD__
	time = attr.__st_birthtim.tv_sec;
#else
	time = attr.st_birthtime;
#endif
	localtime_r(&time, &tm);
	char creation_time[128];
	if (!time) {
		*creation_time = '-';
		creation_time[1] = '\0';
	} else {
		strftime(creation_time, sizeof(creation_time),
		    "%b %d %H:%M:%S %Y", &tm);
	}
#elif defined(_STATX)
	struct statx attrx;
	statx(AT_FDCWD, filename, AT_SYMLINK_NOFOLLOW, STATX_BTIME, &attrx);
	time = (time_t)attrx.stx_btime.tv_sec;
	localtime_r(&time, &tm);
	char creation_time[128];

	if (!time) {
		*creation_time = '-';
		creation_time[1] = '\0';
	} else {
		strftime(creation_time, sizeof(creation_time),
		    "%b %d %H:%M:%S %Y", &tm);
	}
#endif /* _STATX */

	if (colorize == 1 && props_color == 1)
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
	if (colorize == 1 && props_color == 1)
		printf("%s", NC);

#ifdef __OpenBSD__
	printf(_("\tBlocks: %s%lld%s"), cbold, attr.st_blocks, cend);
#else
	printf(_("\tBlocks: %s%ld%s"), cbold, attr.st_blocks, cend);
#endif
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
	printf(_("\tIO Block: %s%d%s"), cbold, attr.st_blksize, cend);
#else
	printf(_("\tIO Block: %s%ld%s"), cbold, attr.st_blksize, cend);
#endif
#ifdef __OpenBSD__
	printf(_("\tInode: %s%llu%s\n"), cbold, attr.st_ino, cend);
#else
	printf(_("\tInode: %s%zu%s\n"), cbold, attr.st_ino, cend);
#endif
#ifdef __OpenBSD__
	printf(_("Device: %s%d%s"), cbold, attr.st_dev, cend);
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
	if (!S_ISDIR(attr.st_mode)) {
		printf(_("Size: \t\t%s%s%s\n"), csize, size_type ? size_type : "?", cend);
		goto END;
	}

	if (!dsize)
		goto END;

	fputs(_("Total size: \t"), stdout);
	off_t total_size = dir_size(filename);
	if (total_size == -1) {
		puts("?");
		goto END;
	}

	char *human_size = get_size_unit(total_size * 1024);
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

int
print_entry_props(const struct fileinfo *props, size_t max, const size_t ug_max)
{
	char *size_type;
	if (full_dir_size == 1 && props->type == DT_DIR)
		size_type = get_size_unit(props->size * 1024);
	else
		size_type = get_size_unit(props->size);

	char file_type = 0;    /* File type indicator */
	char *ctype = PR_NONE, /* Color for file type */
		 *cdate = PR_DATE, /* Color for dates */
		 *cid = PR_ID,     /* Color for UID and GID */
		 *csize = PR_SIZE, /* Color for file size */
		 *cend = NC;       /* Ending Color */

	switch (props->mode & S_IFMT) {
	case S_IFREG: file_type = '-'; break;
	case S_IFDIR: file_type = 'd'; ctype = di_c; break;
	case S_IFLNK: file_type = 'l'; ctype = ln_c; break;
	case S_IFSOCK: file_type = 's'; ctype = so_c; break;
	case S_IFBLK: file_type = 'b'; ctype = bd_c; break;
	case S_IFCHR: file_type = 'c'; ctype = cd_c; break;
	case S_IFIFO: file_type = 'p'; ctype = pi_c; break;
	default: file_type = '?'; break;
	}

	/* Get file permissions */
	char read_usr = '-', write_usr = '-', exec_usr = '-',
	     read_grp = '-', write_grp = '-', exec_grp = '-',
	     read_others = '-', write_others = '-', exec_others = '-';

	/* Colors for each field of the properties string */
	char *cu1 = PR_NONE, *cu2 = PR_NONE, *cu3 = PR_NONE,
		 *cg1 = PR_NONE, *cg2 = PR_NONE, *cg3 = PR_NONE,
		 *co1 = PR_NONE, *co2 = PR_NONE, *co3 = PR_NONE;

	mode_t val = (props->mode & (mode_t)~S_IFMT);
	if (val & S_IRUSR) { read_usr = 'r'; cu1 = PR_READ; }
	if (val & S_IWUSR) { write_usr = 'w'; cu2 = PR_WRITE; }
	if (val & S_IXUSR) { exec_usr = 'x'; cu3 = PR_EXEC; }

	if (val & S_IRGRP) { read_grp = 'r'; cg1 = PR_READ; }
	if (val & S_IWGRP) { write_grp = 'w'; cg2 = PR_WRITE; }
	if (val & S_IXGRP) { exec_grp = 'x'; cg3 = PR_EXEC; }

	if (val & S_IROTH) { read_others = 'r'; co1 = PR_READ; }
	if (val & S_IWOTH) { write_others = 'w'; co2 = PR_WRITE; }
	if (val & S_IXOTH) { exec_others = 'x'; co3 = PR_EXEC; }

	if (props->mode & S_ISUID) {
		(val & S_IXUSR) ? (exec_usr = 's') : (exec_usr = 'S');
		cu3 = PR_SPECIAL;
	}
	if (props->mode & S_ISGID) {
		(val & S_IXGRP) ? (exec_grp = 's') : (exec_grp = 'S');
		cg3 = PR_SPECIAL;
	}
	if (props->mode & S_ISVTX) {
		(val & S_IXOTH) ? (exec_others = 't'): (exec_others = 'T');
		co3 = PR_SPECIAL;
	}

	if (colorize == 0 || props_color == 0) {
		cdate = EMPTY_STR;
		csize = EMPTY_STR;
		cid = EMPTY_STR;
		ctype = EMPTY_STR;
		cend = EMPTY_STR;
		cu1 = EMPTY_STR;
		cu2 = EMPTY_STR;
		cu3 = EMPTY_STR;
		cg1 = EMPTY_STR;
		cg2 = EMPTY_STR;
		cg3 = EMPTY_STR;
		co1 = EMPTY_STR;
		co2 = EMPTY_STR;
		co3 = EMPTY_STR;
	}

	/* Get modification time */
	char mod_time[128];
	if (props->ltime) {
		struct tm t;
		localtime_r(&props->ltime, &t);
		snprintf(mod_time, 128, "%d-%02d-%02d %02d:%02d", t.tm_year + 1900,
		    t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min);
	} else {
		strcpy(mod_time, "-               ");
	}

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

	size_t cur_len = 0;
	cur_len = (size_t)DIGINUM(files + 1) + 1 + plen;
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

	/* Calculate pad for each file */
	int pad;
	pad = (int)(max - cur_len);
	if (pad < 0)
		pad = 0;

	if (!trim || !unicode)
		mbstowcs((wchar_t *)tname, wname ? wname : props->name, PATH_MAX);

	free(wname);

	char trim_diff[14];
	*trim_diff = '\0';
	if (diff > 0)
		snprintf(trim_diff, sizeof(trim_diff), "\x1b[%dC", diff);

	/* Calculate right pad for UID:GID string */
	int ug_pad = 0, u = DIGINUM(props->uid), g = DIGINUM(props->gid);
	if (u + g < (int)ug_max)
		ug_pad = (int)ug_max - u;

#ifndef _NO_ICONS
	printf("%s%s%c%s%s%ls%s%s%-*s%s\x1b[0m%s%c\x1b[0m "
		   "%s%c%s/%s%c%s%c%s%c%s/%s%c%s%c%s%c%s/%s%c%s%c%s%c%s%s "
		   "%s%u:%-*u%s %s%s%s %s%s%s\n",
	    colorize ? props->icon_color : "",
	    icons ? props->icon : "", icons ? ' ' : 0, df_c,
#else
	printf("%s%ls%s%s%-*s%s\x1b[0m%s%c\x1b[0m "
		   "%s%c%s/%s%c%s%c%s%c%s/%s%c%s%c%s%c%s/%s%c%s%c%s%c%s%s "
	       "%s%u:%-*u%s %s%s%s %s%s%s\n",
#endif
	    colorize ? props->color : "",
		(wchar_t *)tname, trim_diff,
	    light_mode ? "" : df_c, pad, "", df_c,
	    trim ? tt_c : "", trim ? '~' : 0, ctype, file_type, cend,
	    cu1, read_usr, cu2, write_usr, cu3, exec_usr, cend,
	    cg1, read_grp, cg2, write_grp, cg3, exec_grp, cend,
	    co1, read_others, co2, write_others, co3, exec_others, cend,
	    is_acl(props->name) ? "+" : "",
	    cid, props->uid, ug_pad, props->gid, cend,
	    cdate, *mod_time ? mod_time : "?", cend,
	    csize, size_type ? size_type : "?", cend);

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

	/* If "pr file file..." */
	for (i = 1; i <= args_n; i++) {
		if (strchr(args[i], '\\')) {
			char *deq_file = dequote_str(args[i], 0);
			if (!deq_file) {
				fprintf(stderr, _("%s: %s: Error dequoting file name\n"),
				    PROGRAM_NAME, args[i]);
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
