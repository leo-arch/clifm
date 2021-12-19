/* properties.c -- functions to get files properties */

/*
 * This file is part of CliFM
 * 
 * Copyright (C) 2016-2021, L. Abramovich <johndoe.arch@outlook.com>
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
	char *size_type = get_size_unit(attr.st_size);

	/* Get file type (and color) */
	char file_type = 0;
	char *linkname = (char *)NULL,
		 *color = (char *)NULL;

	switch (attr.st_mode & S_IFMT) {
	case S_IFREG: {
		char *ext = (char *)NULL;
		file_type = '-';
		if (light_mode)
			color = fi_c;
		else if (access(filename, R_OK) == -1)
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
				if (attr.st_size == 0)
					color = ee_c;
				else
					color = ex_c;
			}

			else if (attr.st_size == 0)
				color = ef_c;
			else if (attr.st_nlink > 1)
				color = mh_c;
			else {
				ext = strrchr(filename, '.');
				if (ext) {
					char *extcolor = get_ext_color(ext);
					if (extcolor) {
						char ext_color[MAX_COLOR] = "";
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
		if (light_mode)
			color = di_c;
		else if (access(filename, R_OK | X_OK) != 0) {
			color = nd_c;
		} else
			color = get_dir_color(filename, attr.st_mode);

		break;
	case S_IFLNK:
		file_type = 'l';
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
		color = so_c;
		break;
	case S_IFBLK:
		file_type = 'b';
		color = bd_c;
		break;
	case S_IFCHR:
		file_type = 'c';
		color = cd_c;
		break;
	case S_IFIFO:
		file_type = 'p';
		color = pi_c;
		break;
	default:
		file_type = '?';
		color = no_c;
	}

	/* Get file permissions */
	char read_usr = '-', write_usr = '-', exec_usr = '-',
	     read_grp = '-', write_grp = '-', exec_grp = '-',
	     read_others = '-', write_others = '-', exec_others = '-';

	mode_t val = (attr.st_mode & (mode_t)~S_IFMT);
	if (val & S_IRUSR) read_usr = 'r';
	if (val & S_IWUSR) write_usr = 'w';
	if (val & S_IXUSR) exec_usr = 'x';

	if (val & S_IRGRP) read_grp = 'r';
	if (val & S_IWGRP) write_grp = 'w';
	if (val & S_IXGRP) exec_grp = 'x';

	if (val & S_IROTH) read_others = 'r';
	if (val & S_IWOTH) write_others = 'w';
	if (val & S_IXOTH) exec_others = 'x';

	if (attr.st_mode & S_ISUID)
		(val & S_IXUSR) ? (exec_usr = 's') : (exec_usr = 'S');
	if (attr.st_mode & S_ISGID)
		(val & S_IXGRP) ? (exec_grp = 's') : (exec_grp = 'S');
	if (attr.st_mode & S_ISVTX)
		(val & S_IXOTH) ? (exec_others = 't'): (exec_others = 'T');

	/* Get number of links to the file */
	nlink_t link_n = attr.st_nlink;

	/* Get modification time */
	time_t time = (time_t)attr.st_mtim.tv_sec;
	struct tm tm;
	localtime_r(&time, &tm);
	char mod_time[128] = "";

	if (time)
		/* Store formatted (and localized) date-time string into
		 * mod_time */
		strftime(mod_time, sizeof(mod_time), "%b %d %H:%M:%S %Y", &tm);
	else
		mod_time[0] = '-';

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
	printf("(%04o)%c/%c%c%c/%c%c%c/%c%c%c%s %zu %s %s %s %s ",
	    attr.st_mode & 07777, file_type,
	    read_usr, write_usr, exec_usr, read_grp,
	    write_grp, exec_grp, read_others, write_others, exec_others,
	    is_acl(filename) ? "+" : "", (size_t)link_n,
	    (!owner) ? _("unknown") : owner->pw_name,
	    (!group) ? _("unknown") : group->gr_name,
	    (size_type) ? size_type : "?",
	    (mod_time[0] != '\0') ? mod_time : "?");

	if (file_type && file_type != 'l') {
		printf("%s%s%s\n", color, wname ? wname : filename, df_c);
	} else if (linkname) {
		printf("%s%s%s -> %s\n", color, wname ? wname : filename, df_c, linkname);
		free(linkname);
	} else { /* Broken link */
		char link[PATH_MAX] = "";
		ssize_t ret = readlinkat(AT_FDCWD, filename, link, PATH_MAX);

		if (ret) {
			printf(_("%s%s%s -> %s (broken link)\n"), color, wname ? wname : filename,
			    df_c, link);
		} else {
			printf("%s%s%s -> ???\n", color, wname ? wname : filename, df_c);
		}
	}

	free(wname);

	/* Stat information */
	/* Last access time */
	time = (time_t)attr.st_atim.tv_sec;
	localtime_r(&time, &tm);
	char access_time[128] = "";

	if (time)
		/* Store formatted (and localized) date-time string into
		 * access_time */
		strftime(access_time, sizeof(access_time), "%b %d %H:%M:%S %Y", &tm);
	else
		access_time[0] = '-';

	/* Last properties change time */
	time = (time_t)attr.st_ctim.tv_sec;
	localtime_r(&time, &tm);
	char change_time[128] = "";
	if (time)
		strftime(change_time, sizeof(change_time), "%b %d %H:%M:%S %Y", &tm);
	else
		change_time[0] = '-';

		/* Get creation (birth) time */
#if defined(HAVE_ST_BIRTHTIME) || defined(__BSD_VISIBLE)
#ifdef __OpenBSD__
	time = attr.__st_birthtim.tv_sec;
#else
	time = attr.st_birthtime;
#endif
	localtime_r(&time, &tm);
	char creation_time[128] = "";
	if (!time)
		creation_time[0] = '-';
	else
		strftime(creation_time, sizeof(creation_time),
		    "%b %d %H:%M:%S %Y", &tm);
#elif defined(_STATX)
	struct statx attrx;
	statx(AT_FDCWD, filename, AT_SYMLINK_NOFOLLOW, STATX_BTIME, &attrx);
	time = (time_t)attrx.stx_btime.tv_sec;
	localtime_r(&time, &tm);
	char creation_time[128] = "";

	if (!time) {
		creation_time[0] = '-';
	} else {
		strftime(creation_time, sizeof(creation_time),
		    "%b %d %H:%M:%S %Y", &tm);
	}
#endif

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
#ifdef __OpenBSD__
	printf(_("\tBlocks: %lld"), attr.st_blocks);
#else
	printf(_("\tBlocks: %ld"), attr.st_blocks);
#endif
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
	printf(_("\tIO Block: %d"), attr.st_blksize);
#else
	printf(_("\tIO Block: %ld"), attr.st_blksize);
#endif
#ifdef __OpenBSD__
	printf(_("\tInode: %llu\n"), attr.st_ino);
#else
	printf(_("\tInode: %zu\n"), attr.st_ino);
#endif
#ifdef __OpenBSD__
	printf(_("Device: %d"), attr.st_dev);
#else
	printf(_("Device: %zu"), attr.st_dev);
#endif
	printf(_("\tUid: %u (%s)"), attr.st_uid, (!owner) ? _("unknown")
			: owner->pw_name);
	printf(_("\tGid: %u (%s)\n"), attr.st_gid, (!group) ? _("unknown")
			: group->gr_name);

	/* Print file timestamps */
	printf(_("Access: \t%s\n"), access_time);
	printf(_("Modify: \t%s\n"), mod_time);
	printf(_("Change: \t%s\n"), change_time);

#if defined(HAVE_ST_BIRTHTIME) || defined(__BSD_VISIBLE) || defined(_STATX)
	printf(_("Birth: \t\t%s\n"), creation_time);
#endif

	/* Print size */
	if ((attr.st_mode & S_IFMT) == S_IFDIR) {
		if (dsize) {
			fputs(_("Total size: \t"), stdout);
			off_t total_size = dir_size(filename);
			if (total_size != -1) {
				char *human_size = get_size_unit(total_size * 1024);
				if (human_size) {
					printf("%s\n", human_size);
					free(human_size);
				} else {
					puts("?");
				}
			} else {
				puts("?");
			}
		}
	} else {
		printf(_("Size: \t\t%s\n"), size_type ? size_type : "?");
	}

	free(size_type);
	return EXIT_SUCCESS;
}

int
print_entry_props(const struct fileinfo *props, size_t max)
{
	/* Get file size */
	char *size_type = get_size_unit(props->size);

	/* Get file type indicator */
	char file_type = 0;

	switch (props->mode & S_IFMT) {
	case S_IFREG: file_type = '-'; break;
	case S_IFDIR: file_type = 'd'; break;
	case S_IFLNK: file_type = 'l'; break;
	case S_IFSOCK: file_type = 's'; break;
	case S_IFBLK: file_type = 'b'; break;
	case S_IFCHR: file_type = 'c'; break;
	case S_IFIFO: file_type = 'p'; break;
	default: file_type = '?';
	}

	/* Get file permissions */
	char read_usr = '-', write_usr = '-', exec_usr = '-',
	     read_grp = '-', write_grp = '-', exec_grp = '-',
	     read_others = '-', write_others = '-', exec_others = '-';

	mode_t val = (props->mode & (mode_t)~S_IFMT);
	if (val & S_IRUSR) read_usr = 'r';
	if (val & S_IWUSR) write_usr = 'w';
	if (val & S_IXUSR) exec_usr = 'x';

	if (val & S_IRGRP) read_grp = 'r';
	if (val & S_IWGRP) write_grp = 'w';
	if (val & S_IXGRP) exec_grp = 'x';

	if (val & S_IROTH) read_others = 'r';
	if (val & S_IWOTH) write_others = 'w';
	if (val & S_IXOTH) exec_others = 'x';

	if (props->mode & S_ISUID)
		(val & S_IXUSR) ? (exec_usr = 's') : (exec_usr = 'S');
	if (props->mode & S_ISGID)
		(val & S_IXGRP) ? (exec_grp = 's') : (exec_grp = 'S');
	if (props->mode & S_ISVTX)
		(val & S_IXOTH) ? (exec_others = 't'): (exec_others = 'T');

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

	/* Get owner and group names */
	/*  struct group *group;
	struct passwd *owner;
	group = getgrgid(props->uid);
	owner = getpwuid(props->gid); */

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

#ifndef _NO_ICONS
	printf("%s%s%c%s%s%ls\x1b[%dC%s%-*s%s%s %c/%c%c%c/%c%c%c/%c%c%c%s  "
	       "%u:%u  %s  %s\n",
	    colorize ? props->icon_color : "",
	    icons ? props->icon : "", icons ? ' ' : 0, df_c,
#else
	printf("%s%ls\x1b[%dC%s%-*s%s%s %c/%c%c%c/%c%c%c/%c%c%c%s  "
	       "%u:%u  %s  %s\n",
#endif
	    colorize ? props->color : "",
		(wchar_t *)tname, diff > 0 ? diff : -1,
	    light_mode ? "" : df_c, pad, "", df_c,
	    trim ? "\x1b[1;31m~\x1b[0m" : "", file_type,
	    read_usr, write_usr, exec_usr,
	    read_grp, write_grp, exec_grp,
	    read_others, write_others, exec_others,
	    is_acl(props->name) ? "+" : "",
	    /*          !owner ? _("?") : owner->pw_name,
			!group ? _("?") : group->gr_name, */
	    props->uid, props->gid,
	    *mod_time ? mod_time : "?",
	    size_type ? size_type : "?");

	if (size_type)
		free(size_type);

	return EXIT_SUCCESS;
}

int
properties_function(char **comm)
{
	if (!comm)
		return EXIT_FAILURE;

	size_t i;
	int exit_status = EXIT_SUCCESS;
	int _dir_size = 0;

	if (*comm[0] == 'p' && comm[0][1] == 'p' && !comm[0][2])
		_dir_size = 1;

	/* If "pr file file..." */
	for (i = 1; i <= args_n; i++) {
		if (strchr(comm[i], '\\')) {
			char *deq_file = dequote_str(comm[i], 0);
			if (!deq_file) {
				fprintf(stderr, _("%s: %s: Error dequoting file name\n"),
				    PROGRAM_NAME, comm[i]);
				exit_status = EXIT_FAILURE;
				continue;
			}

			strcpy(comm[i], deq_file);
			free(deq_file);
		}

		if (get_properties(comm[i], _dir_size) != 0)
			exit_status = EXIT_FAILURE;
	}

	return exit_status;
}
