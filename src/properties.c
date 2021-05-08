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

#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/capability.h>
#include <sys/types.h>
#include <grp.h>
#include <pwd.h>
#include <fcntl.h>

#include "aux.h"
#include "colors.h"
#include "checks.h"

int
get_properties(char *filename, int dsize)
{
	if (!filename || !*filename)
		return EXIT_FAILURE;

	size_t len = strlen(filename);
	if (filename[len - 1] == '/')
		filename[len - 1] = '\0';

	struct stat file_attrib;

	/* Check file existence */
	if (lstat(filename, &file_attrib) == -1) {
		fprintf(stderr, "%s: pr: '%s': %s\n", PROGRAM_NAME, filename,
				strerror(errno));
		return EXIT_FAILURE;
	}

	/* Get file size */
	char *size_type = get_size_unit(file_attrib.st_size);

	/* Get file type (and color): */
	int sticky = 0;
	char file_type = 0;
	char *linkname = (char *)NULL, *color = (char *)NULL;

	switch (file_attrib.st_mode & S_IFMT) {

	case S_IFREG: {

		char *ext = (char *)NULL;

		file_type = '-';

		if (access(filename, R_OK) == -1)
			color = nf_c;

		else if (file_attrib.st_mode & S_ISUID)
			color = su_c;

		else if (file_attrib.st_mode & S_ISGID)
			color = sg_c;

		else {
#ifdef _LINUX_CAP
			cap_t cap = cap_get_file(filename);

			if (cap) {
				color = ca_c;
				cap_free(cap);
			}

			else if (file_attrib.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)) {
#else

			if (file_attrib.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)) {
#endif

				if (file_attrib.st_size == 0) color = ee_c;

				else color = ex_c;
			}

			else if (file_attrib.st_size == 0) color = ef_c;

			else if (file_attrib.st_nlink > 1) color = mh_c;

			else {
				ext = strrchr(filename, '.');

				if (ext) {

					char *extcolor = get_ext_color(ext);

					if (extcolor) {
						char ext_color[MAX_COLOR] = "";
						sprintf(ext_color, "\x1b[%sm", extcolor);
						color = ext_color;
						extcolor = (char *)NULL;
					}

					else /* No matching extension found */
						color = fi_c;
				}

				else
					color = fi_c;
			}
		}
		}
		break;

	case S_IFDIR:
		file_type = 'd';

		if (access(filename, R_OK|X_OK) != 0)
			color = nd_c;

		else {
			int is_oth_w = 0;
			if (file_attrib.st_mode & S_ISVTX) sticky = 1;

			if (file_attrib.st_mode & S_IWOTH) is_oth_w = 1;

			int files_dir = count_dir(filename);

			color = sticky ? (is_oth_w ? tw_c : st_c)
				: is_oth_w ? ow_c :
				((files_dir == 2 || files_dir == 0) ? ed_c : di_c);
		}

		break;

	case S_IFLNK:
		file_type = 'l';

		linkname = realpath(filename, (char *)NULL);

		if (linkname)
			color = ln_c;
		else
			color = or_c;
		break;

	case S_IFSOCK: file_type = 's'; color = so_c; break;

	case S_IFBLK: file_type = 'b'; color = bd_c; break;

	case S_IFCHR: file_type = 'c'; color = cd_c; break;

	case S_IFIFO: file_type = 'p'; color = pi_c; break;

	default: file_type = '?'; color = no_c;
	}

	/* Get file permissions */
	char read_usr = '-', write_usr = '-', exec_usr = '-',
		 read_grp = '-', write_grp = '-', exec_grp = '-',
		 read_others = '-', write_others = '-', exec_others = '-';

	mode_t val = (file_attrib.st_mode & ~S_IFMT);
	if (val & S_IRUSR) read_usr = 'r';
	if (val & S_IWUSR) write_usr = 'w';
	if (val & S_IXUSR) exec_usr = 'x';

	if (val & S_IRGRP) read_grp = 'r';
	if (val & S_IWGRP) write_grp = 'w';
	if (val & S_IXGRP) exec_grp = 'x';

	if (val & S_IROTH) read_others = 'r';
	if (val & S_IWOTH) write_others = 'w';
	if (val & S_IXOTH) exec_others = 'x';

	if (file_attrib.st_mode & S_ISUID)
		(val & S_IXUSR) ? (exec_usr = 's') : (exec_usr = 'S');
	if (file_attrib.st_mode & S_ISGID)
		(val & S_IXGRP) ? (exec_grp = 's') : (exec_grp = 'S');

	/* Get number of links to the file */
	nlink_t link_n = file_attrib.st_nlink;

	/* Get modification time */
	time_t time = (time_t)file_attrib.st_mtim.tv_sec;
	struct tm *tm = localtime(&time);
	char mod_time[128] = "";

	if (time)
		/* Store formatted (and localized) date-time string into
		 * mod_time */
		strftime(mod_time, sizeof(mod_time), "%b %d %H:%M:%S %Y", tm);

	else
		mod_time[0] = '-';

	/* Get owner and group names */
	uid_t owner_id = file_attrib.st_uid; /* owner ID */
	gid_t group_id = file_attrib.st_gid; /* group ID */
	struct group *group;
	struct passwd *owner;
	group = getgrgid(group_id);
	owner = getpwuid(owner_id);

	/* Print file properties */
	printf("(%04o)%c/%c%c%c/%c%c%c/%c%c%c%s %zu %s %s %s %s ",
			file_attrib.st_mode & 07777, file_type,
			read_usr, write_usr, exec_usr, read_grp,
			write_grp, exec_grp, read_others, write_others,
			(sticky) ? 't' : exec_others,
			is_acl(filename) ? "+" : "", link_n,
			(!owner) ? _("unknown") : owner->pw_name,
			(!group) ? _("unknown") : group->gr_name,
			(size_type) ? size_type : "?",
			(mod_time[0] != '\0') ? mod_time : "?");

	if (file_type && file_type != 'l')
		printf("%s%s%s\n", color, filename, df_c);

	else if (linkname) {
		printf("%s%s%s -> %s\n", color, filename, df_c, linkname);
		free(linkname);
	}

	else { /* Broken link */
		char link[PATH_MAX] = "";
		ssize_t ret = readlink(filename, link, PATH_MAX);

		if (ret) {
			printf(_("%s%s%s -> %s (broken link)\n"), color, filename,
				   df_c, link);
		}

		else
			printf("%s%s%s -> ???\n", color, filename, df_c);
	}

	/* Stat information */

	/* Last access time */
	time = (time_t)file_attrib.st_atim.tv_sec;
	tm = localtime(&time);
	char access_time[128] = "";

	if (time)
		/* Store formatted (and localized) date-time string into
		 * access_time */
		strftime(access_time, sizeof(access_time), "%b %d %H:%M:%S %Y", tm);

	else
		access_time[0] = '-';

	/* Last properties change time */
	time = (time_t)file_attrib.st_ctim.tv_sec;
	tm = localtime(&time);
	char change_time[128] = "";

	if (time)
		strftime(change_time, sizeof(change_time), "%b %d %H:%M:%S %Y", tm);

	else
		change_time[0] = '-';

	/* Get creation (birth) time */
#if defined(HAVE_ST_BIRTHTIME) || defined(__BSD_VISIBLE)
		time = file_attrib.st_birthtime;
		tm = localtime(&time);
		char creation_time[128] = "";

		if (!time)
			creation_time[0] = '-';

		else
			strftime(creation_time, sizeof(creation_time),
					 "%b %d %H:%M:%S %Y", tm);
#elif defined(_STATX)
		struct statx attrx;
		statx(AT_FDCWD, filename, AT_SYMLINK_NOFOLLOW, STATX_BTIME, &attrx);
		time = (time_t)attrx.stx_btime.tv_sec;
		tm = localtime(&time);
		char creation_time[128] = "";

		if (!time)
			creation_time[0] = '-';

		else
			strftime(creation_time, sizeof(creation_time),
					 "%b %d %H:%M:%S %Y", tm);
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

	printf(_("\tBlocks: %ld"), file_attrib.st_blocks);
#if __FreeBSD__
	printf(_("\tIO Block: %d"), file_attrib.st_blksize);
#else
	printf(_("\tIO Block: %ld"), file_attrib.st_blksize);
#endif
	printf(_("\tInode: %zu\n"), file_attrib.st_ino);
	printf(_("Device: %zu"), file_attrib.st_dev);
	printf(_("\tUid: %u (%s)"), file_attrib.st_uid, (!owner)
			? _("unknown") : owner->pw_name);
	printf(_("\tGid: %u (%s)\n"), file_attrib.st_gid, (!group)
			? _("unknown") : group->gr_name);

	/* Print file timestamps */
	printf(_("Access: \t%s\n"), access_time);
	printf(_("Modify: \t%s\n"), mod_time);
	printf(_("Change: \t%s\n"), change_time);

#if defined(HAVE_ST_BIRTHTIME) || defined(__BSD_VISIBLE) \
	|| defined(_STATX)
		printf(_("Birth: \t\t%s\n"), creation_time);
#endif

	/* Print size */

	if ((file_attrib.st_mode & S_IFMT) == S_IFDIR) {

		if (dsize) {
			fputs(_("Total size: \t"), stdout);
			off_t total_size = dir_size(filename);

			if (total_size != -1) {
				char *human_size = get_size_unit(total_size);

				if (human_size) {
					printf("%s\n", human_size);
					free(human_size);
				}

				else
					puts("?");
			}

			else
				puts("?");
		}
	}

	else
		printf(_("Size: \t\t%s\n"), size_type ? size_type : "?");

	if (size_type)
		free(size_type);

	return EXIT_SUCCESS;
}

int
print_entry_props(struct fileinfo *props, size_t max)
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

	mode_t val = (props->mode & ~S_IFMT);
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

	/* Get modification time */
	char mod_time[128];

	if (props->ltime) {
		struct tm *t = localtime(&props->ltime);
		snprintf(mod_time, 128, "%d-%02d-%02d %02d:%02d", t->tm_year + 1900,
				t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min);
	}
	else
		strcpy(mod_time, "-               ");

	/* Get owner and group names */
/*  struct group *group;
	struct passwd *owner;
	group = getgrgid(props->uid);
	owner = getpwuid(props->gid); */

	/*  If filename length is greater than max, truncate it
	 * to max (later a tilde (~) will be appended to let the user know
	 * the file name was truncated) */
	char trim_name[NAME_MAX];
	int trim = 0;

	size_t cur_len = props->eln_n + 1 + props->len;
	if (icons) {
		cur_len += 3;
		max += 3;
	}

	if (cur_len > max) {
		int rest = (int)(cur_len - max);
		trim = 1;
		strcpy(trim_name, props->name);
		if (unicode)
			u8truncstr(trim_name, (size_t)(props->len - rest - 1));
		else
			trim_name[props->len - rest - 1] = '\0';
		cur_len -= rest;
	}

	/* Calculate pad for each file */
	int pad;

	pad = (int)(max - cur_len);

	if (pad < 0)
		pad = 0;

	int sticky = 0;
	if (props->mode & S_ISVTX)
		sticky = 1;

	printf("%s%s%c%s%s%s%-*s%s%c %c/%c%c%c/%c%c%c/%c%c%c%s  "
			"%u:%u  %s  %s\n",
			colorize ? props->icon_color : "",
			icons ? props->icon : "", icons ? ' ' : 0,
			colorize ? props->color : "",
			!trim ? props->name : trim_name,
			light_mode ? "" : df_c, pad, "", df_c,
			trim ? '~' : 0, file_type,
			read_usr, write_usr, exec_usr,
			read_grp, write_grp, exec_grp,
			read_others, write_others, sticky ? 't' : exec_others,
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
	if(!comm)
		return EXIT_FAILURE;

	size_t i;
	int exit_status = EXIT_SUCCESS;
	int dir_size = 0;

	if (*comm[0] == 'p' && comm[0][1] == 'p' && !comm[0][2])
		dir_size = 1;

	/* If "pr file file..." */
	for (i = 1; i <= args_n; i++) {

		if (strchr(comm[i], '\\')) {
			char *deq_file = dequote_str(comm[i], 0);

			if (!deq_file) {
				fprintf(stderr, _("%s: %s: Error dequoting filename\n"),
						PROGRAM_NAME, comm[i]);
				exit_status = EXIT_FAILURE;
				continue;
			}

			strcpy(comm[i], deq_file);
			free(deq_file);
		}

		if (get_properties(comm[i], dir_size) != 0)
			exit_status = EXIT_FAILURE;
	}

	return exit_status;
}
