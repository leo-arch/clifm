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
#if defined(__linux__)
# include <sys/capability.h>
# include <sys/sysmacros.h>
#endif
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <sys/types.h>
#include <wchar.h>

#if defined(__OpenBSD__) || defined(__NetBSD__) \
|| defined(__FreeBSD__) || defined(__APPLE__)
# include <inttypes.h> /* uintmax_t, intmax_t */
#endif

#include "aux.h"
#include "checks.h"
#include "colors.h"
#include "misc.h"

/* Required by the pc command */
#include "readline.h"
#include "file_operations.h"

#ifndef major
# define major(x) ((x >> 8) & 0x7F)
#endif
#ifndef minor
# define minor(x) (x & 0xFF)
#endif

#ifndef DT_DIR
# define DT_DIR 4
#endif

#define TIME_STR "%a %b %d %H:%M:%S %Y %z"
#define MAX_TIME_STR 128
/* Our resulting time string won't go usually beyond 29 chars. But since this
 * length is locale dependent (at least %b), let's use a much larger buffer */

struct perms_t {
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
};

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

static off_t
get_total_size(const int link_to_dir, char *filename)
{
	off_t total_size = 0;

	char _path[PATH_MAX]; *_path = '\0';
	if (link_to_dir == 1)
		snprintf(_path, sizeof(_path), "%s/", filename);

	if (term_caps.suggestions == 0) {
		fputs("Retrieving file size... ", stdout);
		fflush(stdout);
		total_size = dir_size(*_path ? _path : filename);
		fputs("\r                       \r", stdout);
		fputs(_("Total size: \t"), stdout);
	} else {
		fputs(_("Total size: \t"), stdout);
		HIDE_CURSOR;
		fputs("Calculating... ", stdout);
		fflush(stdout);
		total_size = dir_size(*_path ? _path : filename);
		MOVE_CURSOR_LEFT(15);
		ERASE_TO_RIGHT;
		UNHIDE_CURSOR;
	}

	return total_size;
}

/* Returns a struct perms_t with the symbolic value and color for each
 * properties field of a file with mode MODE */
static struct perms_t
get_file_perms(const mode_t mode)
{
	struct perms_t p;

	p.ur = p.uw = p.ux = '-';
	p.gr = p.gw = p.gx = '-';
	p.or = p.ow = p.ox = '-';

	p.cur = p.cuw = p.cux = dn_c;
	p.cgr = p.cgw = p.cgx = dn_c;
	p.cor = p.cow = p.cox = dn_c;

	mode_t val = (mode & (mode_t)~S_IFMT);
	if (val & S_IRUSR) { p.ur = 'r'; p.cur = dr_c; }
	if (val & S_IWUSR) { p.uw = 'w'; p.cuw = dw_c; }
	if (val & S_IXUSR)
		{ p.ux = 'x'; p.cux = S_ISDIR(mode) ? dxd_c : dxr_c; }

	if (val & S_IRGRP) { p.gr = 'r'; p.cgr = dr_c; }
	if (val & S_IWGRP) { p.gw = 'w'; p.cgw = dw_c; }
	if (val & S_IXGRP)
		{ p.gx = 'x'; p.cgx = S_ISDIR(mode) ? dxd_c : dxr_c; }

	if (val & S_IROTH) { p.or = 'r'; p.cor = dr_c; }
	if (val & S_IWOTH) { p.ow = 'w'; p.cow = dw_c; }
	if (val & S_IXOTH)
		{ p.ox = 'x'; p.cox = S_ISDIR(mode) ? dxd_c : dxr_c; }

	if (mode & S_ISUID) {
		p.ux = (val & S_IXUSR) ? 's' : 'S';
		p.cux = dp_c;
	}
	if (mode & S_ISGID) {
		p.gx = (val & S_IXGRP) ? 's' : 'S';
		p.cgx = dp_c;
	}
	if (mode & S_ISVTX) {
		p.ox = (val & S_IXOTH) ? 't' : 'T';
		p.cox = dp_c;
	}

	if (colorize == 0) {
		p.cur = p.cuw = p.cux = df_c;
		p.cgr = p.cgw = p.cgx = df_c;
		p.cor = p.cow = p.cox = df_c;
	}

	return p;
}

/* Returns EXIT_SUCCESS if the permissions string S (in octal notation)
 * is a valid permissions string or EXIT_FAILURE otherwise */
static int
validate_octal_perms(char *s, const size_t l)
{
	if (l > 4 || l < 3) {
		fprintf(stderr, _("pc: %s digits. Either 3 or 4 are "
			"expected\n"), l > 4 ? _("Too many") : _("Too few"));
		return EXIT_FAILURE;
	}

	size_t i;
	for (i = 0; s[i]; i++) {
		if (s[i] < '0' || s[i] > '7') {
			fprintf(stderr, _("pc: %c: Invalid digit. Values in the range 0-7 "
				"are expected for each field\n"), s[i]);
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}

/* Validate each field of a symbolic permissions string
 * Returns EXIT_SUCCESS on success and EXIT_FAILURE on error */
static int
validate_symbolic_perms(const char *s)
{
	size_t i;
	for (i = 0; i < 9; i++) {
		switch(i) {
		case 0: /* fallthrough */
		case 3: /* fallthrough */
		case 6:
			if (s[i] != '-' && s[i] != 'r') {
				fprintf(stderr, _("pc: Invalid character in field %zu: "
					"%s-r%s are expected\n"), i + 1, BOLD, NC);
				return EXIT_FAILURE;
			}
			break;

		case 1: /* fallthrough */
		case 4: /* fallthrough */
		case 7:
			if (s[i] != '-' && s[i] != 'w') {
				fprintf(stderr, _("pc: Invalid character in field %zu: "
					"%s-w%s are expected\n"), i + 1, BOLD, NC);
				return EXIT_FAILURE;
			}
			break;

		case 2: /* fallthrough */
		case 5:
			if (s[i] != '-' && s[i] != 'x' && TOUPPER(s[i]) != 'S') {
				fprintf(stderr, _("pc: Invalid character in field %zu: "
					"%s-xsS%s are expected\n"), i + 1, BOLD, NC);
				return EXIT_FAILURE;
			}
			break;

		case 8:
			if (s[i] != '-' && s[i] != 'x' && TOUPPER(s[i]) != 'T') {
				fprintf(stderr, _("pc: Invalid character in field %zu: "
					"%s-xtT%s are expected\n"), i + 1, BOLD, NC);
				return EXIT_FAILURE;
			}
			break;

		default: return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}

/* Returns EXIT_SUCCESS if the permissions string S is a valid permissions
 * string or EXIT_FAILURE otherwise */
static int
validate_new_perms(char *s)
{
	size_t l = strlen(s);
	if (*s >= '0' && *s <= '9')
		return validate_octal_perms(s, l);

	if (l != 9) {
		fprintf(stderr, _("pc: %s characters: 9 are expected\n"),
			l < 9 ? _("Too few") : _("Too many"));
		return EXIT_FAILURE;
	}

	return validate_symbolic_perms(s);
}

/* Convert permissions in symbolic notation given by S into octal notation
 * and return it as a string */
static char *
perm2octal(char *s)
{
	int a, b, c, d;
	a = b = c = d = 0;

	if (s[0] != '-') b += 4;
	if (s[1] != '-') b += 2;
	if (s[2] != '-' && s[2] != 'S') b += 1;

	if (s[3] != '-') c += 4;
	if (s[4] != '-') c += 2;
	if (s[5] != '-' && s[5] != 'S') c += 1;

	if (s[6] != '-') d += 4;
	if (s[7] != '-') d += 2;
	if (s[8] != '-' && s[8] != 'T') d += 1;

	if (TOUPPER(s[2]) == 'S') a += 4;
	if (TOUPPER(s[5]) == 'S') a += 2;
	if (TOUPPER(s[8]) == 'T') a += 1;

	char *p = (char *)xnmalloc(32, sizeof(char));
	sprintf(p, "%d%d%d%d", a, b, c, d);

	return p;
}

/* Ask the user to edit permissions given by STR and return the edited string
 * If DIFF is set to 0, we are editing a single file or multiple files with the
 * same set of permissions. Otherwise, we have multiple files with different
 * sets of permissions */
static char *
get_new_perms(char *str, const int diff)
{
	int poffset_bk = prompt_offset;
	prompt_offset = 3;
	xrename = 2; /* Completely disable TAB completion */

	if (diff == 1)
		printf(_("Files with different sets of permissions\n"
			"Only shared permission bits are set in the template\n"));
	char m[NAME_MAX];
	snprintf(m, sizeof(m), _("Edit file permissions ('Ctrl-d' to quit)\n"
		"\001%s\002>\001%s\002 "), mi_c, tx_c);

	alt_rl_prompt(m, str);

	char *new_perms = (char *)NULL;
	if (rl_callback_handler_input) {
		new_perms = savestring(rl_callback_handler_input, strlen(rl_callback_handler_input));
		free(rl_callback_handler_input);
		rl_callback_handler_input = (char *)NULL;
	}

	xrename = 0;
	prompt_offset = poffset_bk;

	if (diff == 0 && new_perms && *str == *new_perms && strcmp(str, new_perms) == 0) {
		fprintf(stderr, _("pc: Nothing to do\n"));
		free(new_perms);
		new_perms = (char *)NULL;
	}

	return new_perms;
}

/* Returns a struct perms_t with only those permission bits shared by
 * all files in S set. DIFF is set to 1 in case files in S have different
 * permission sets. Otherwise, it is set to 0. */
static struct perms_t
get_common_perms(char **s, int *diff)
{
	*diff = 0;
	struct stat a, b;
	struct perms_t p;
	p.ur = p.gr = p.or = 'r';
	p.uw = p.gw = p.ow = 'w';
	p.ux = p.gx = p.ox = 'x';
	int suid = 1, sgid = 1, sticky = 1;

	int i;
	for (i = 0; s[i]; i++) {
		if (stat(s[i], &a) == -1)
			continue;
		if (i > 0 && a.st_mode != b.st_mode)
			*diff = 1;

		mode_t val = (a.st_mode & (mode_t)~S_IFMT);
		if (!(val & S_IRUSR)) p.ur = '-';
		if (!(val & S_IWUSR)) p.uw = '-';
		if (!(val & S_IXUSR)) p.ux = '-';

		if (!(val & S_IRGRP)) p.gr = '-';
		if (!(val & S_IWGRP)) p.gw = '-';
		if (!(val & S_IXGRP)) p.gx = '-';

		if (!(val & S_IROTH)) p.or = '-';
		if (!(val & S_IWOTH)) p.ow = '-';
		if (!(val & S_IXOTH)) p.ox = '-';

		if (!(a.st_mode & S_ISUID)) suid = 0;
		if (!(a.st_mode & S_ISGID)) sgid = 0;
		if (!(a.st_mode & S_ISVTX)) sticky = 0;

		b = a;
	}

	if (suid == 1) p.ux = p.ux == 'x' ? 's' : 'S';
	if (sgid == 1) p.gx = p.gx == 'x' ? 's' : 'S';
	if (sticky == 1) p.ox = p.ox == 'x' ? 't' : 'T';

	return p;
}

/* Returns the permissions string of files passed as arguments (S).
 * If only a single file or multiple files with the same set of permissions,
 * the actual permissions are returned. Otherwise, only shared permissions
 * bits are set in the permissions template.
 * DIFF is set to 1 if files in S have different permission sets. Otherwise,
 * it is set to 0. */
static char *
get_perm_str(char **s, int *diff)
{
	char *ptr = (char *)xnmalloc(10, sizeof(char));
	*diff = 0;

	if (s[1]) { /* Multiple files */
		struct perms_t p = get_common_perms(s, diff);
		sprintf(ptr, "%c%c%c%c%c%c%c%c%c",
			p.ur, p.uw, p.ux, p.gr, p.gw, p.gx, p.or, p.ow, p.ox);
		return ptr;
	}

	/* Single file */
	struct stat a;
	if (stat(s[0], &a) == -1) {
		fprintf(stderr, "stat: %s: %s\n", s[0], strerror(errno));
		free(ptr);
		return (char *)NULL;
	}

	struct perms_t p = get_file_perms(a.st_mode);
	sprintf(ptr, "%c%c%c%c%c%c%c%c%c",
		p.ur, p.uw, p.ux, p.gr, p.gw, p.gx, p.or, p.ow, p.ox);

	return ptr;
}

/* Change permissions of files passed via ARGS */
int
set_file_perms(char **args)
{
	if (!args || !args[1] || IS_HELP(args[1])) {
		puts(PC_USAGE);
		return EXIT_SUCCESS;
	}

	int j;
	for (j = 1; args[j]; j++) {
		if (!strchr(args[j], '\\'))
			continue;
		char *t = dequote_str(args[j], 0);
		if (t) {
			free(args[j]);
			args[j] = t;
		}
	}

	int diff = 0; /* Either a single file o multiple files with same perms */
	char *pstr = get_perm_str(args + 1, &diff);
	if (!pstr) return errno;

	char *new_perms = get_new_perms(pstr, diff);
	free(pstr);

	if (!new_perms) return EXIT_SUCCESS;

	if (validate_new_perms(new_perms) != EXIT_SUCCESS) {
		free(new_perms);
		return EXIT_FAILURE;
	}

	char *octal_str = IS_DIGIT(*new_perms) ? new_perms : perm2octal(new_perms);

	int ret = EXIT_SUCCESS;
	size_t i, n = 0;
	mode_t mode = (mode_t)strtol(octal_str, 0, 8);
	for (i = 1; args[i]; i++) {
		if (fchmodat(AT_FDCWD, args[i], mode, 0) == EXIT_SUCCESS) {
			n++;
		} else {
			fprintf(stderr, "pc: %s: %s\n", args[i], strerror(errno));
			ret = errno;
		}
		fflush(stdout);
	}

	if (n > 0)
		printf(_("pc: Applied new permissions to %zu file(s)\n"), n);

	free(new_perms);
	if (octal_str != new_perms)
		free(octal_str);
	return ret;
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

	switch (attr.st_mode & S_IFMT) {
	case S_IFREG: {
		file_type = '-';
		color = get_regfile_color(filename, &attr);
	} break;

	case S_IFDIR:
		file_type = 'd';
		ctype = di_c;
		if (colorize == 0)
			color = di_c;
		else if (check_file_access(&attr) == 0)
			color = nd_c;
		else
			color = get_dir_color(filename, attr.st_mode, attr.st_nlink);
		break;

	case S_IFLNK:
		file_type = 'l';
		ctype = ln_c;
		if (colorize == 0) {
			color = ln_c;
		} else {
			linkname = realpath(filename, (char *)NULL);
			color = linkname ? ln_c : or_c;
		}
		break;

	case S_IFSOCK:
		file_type = 's';
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

	if (!color)
		color = fi_c;

	if (colorize == 0) {
		cdate = df_c;
		csize = df_c;
		cid = df_c;
		cnum_val = df_c;
		color = df_c;
		ctype = df_c;
		cend = df_c;
		cbold = df_c;
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

	char *t_ctype = savestring(ctype, strnlen(ctype, MAX_COLOR));
	remove_bold_attr(&t_ctype);

	/* Print file properties */
	struct perms_t perms = get_file_perms(attr.st_mode);
	printf(_("(%s%04o%s)%s%c%s/%s%c%s%c%s%c%s/%s%c%s%c%s%c%s/%s%c%s%c%s%c%s%s "
		"Links: %s%zu%s "),
		cnum_val, attr.st_mode & 07777, cend,
		t_ctype, file_type, cend,
		perms.cur, perms.ur, perms.cuw, perms.uw, perms.cux, perms.ux, cend,
		perms.cgr, perms.gr, perms.cgw, perms.gw, perms.cgx, perms.gx, cend,
		perms.cor, perms.or, perms.cow, perms.ow, perms.cox, perms.ox, cend,
		is_acl(filename) ? "+" : "", cbold, (size_t)link_n, cend);

	free(t_ctype);

	int link_to_dir = 0;

	if (file_type == 0) {
		printf(_("\tName: %s%s%s\n"), no_c, wname ? wname : filename, df_c);
	} else if (file_type != 'l') {
		printf(_("\tName: %s%s%s\n"), color, wname ? wname : filename, df_c);
	} else if (linkname) {
		char *link_color = get_link_color(linkname, &link_to_dir, dsize);
		char *n = abbreviate_file_name(linkname);
		printf(_("\tName: %s%s%s -> %s%s%s\n"), color, wname ? wname : filename, df_c,
			link_color, n ? n : linkname, NC);
		free(linkname);
		free(n);
	} else { /* Broken link */
		char link[PATH_MAX] = "";
		ssize_t ret = readlinkat(AT_FDCWD, filename, link, sizeof(link));
		if (ret) {
			printf(_("\tName: %s%s%s -> %s (broken link)\n"), color, wname ? wname : filename,
			    df_c, link);
		} else {
			printf(_("\tName: %s%s%s -> ???\n"), color, wname ? wname : filename, df_c);
		}
	}

	free(wname);

	/* Stat information */

	/* Last modification time */
	time_t time = (time_t)attr.st_mtime;
	struct tm tm;
	localtime_r(&time, &tm);
	char mod_time[MAX_TIME_STR];

	if (time) {
		strftime(mod_time, sizeof(mod_time), TIME_STR, &tm);
	} else {
		*mod_time = '-';
		mod_time[1] = '\0';
	}

	/* Last access time */
	time = (time_t)attr.st_atime;
	localtime_r(&time, &tm);
	char access_time[MAX_TIME_STR];

	if (time) {
		strftime(access_time, sizeof(access_time), TIME_STR, &tm);
	} else {
		*access_time = '-';
		access_time[1] = '\0';
	}

	/* Last properties change time */
	time = (time_t)attr.st_ctime;
	localtime_r(&time, &tm);
	char change_time[MAX_TIME_STR];
	if (time) {
		strftime(change_time, sizeof(change_time), TIME_STR, &tm);
	} else {
		*change_time = '-';
		change_time[1] = '\0';
	}

	/* Creation (birth) time */
#if defined(HAVE_ST_BIRTHTIME) || defined(__BSD_VISIBLE)
# ifdef __OpenBSD__
	time = attr.__st_birthtim.tv_sec;
# else
	time = attr.st_birthtime;
# endif
	localtime_r(&time, &tm);
	char creation_time[MAX_TIME_STR];
	if (!time) {
		*creation_time = '-';
		creation_time[1] = '\0';
	} else {
		strftime(creation_time, sizeof(creation_time), TIME_STR, &tm);
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
		strftime(creation_time, sizeof(creation_time), TIME_STR, &tm);
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

	printf(_("\tBlocks: %s%jd%s"), cbold, (intmax_t)attr.st_blocks, cend);
	printf(_("\tIO Block: %s%jd%s"), cbold, (intmax_t)attr.st_blksize, cend);
	printf(_("\tInode: %s%ju%s\n"), cbold, (uintmax_t)attr.st_ino, cend);
	dev_t d = (S_ISCHR(attr.st_mode) || S_ISBLK(attr.st_mode)) ? attr.st_rdev : attr.st_dev;
	printf(_("Device: %s%ju,%ju%s"), cbold, (uintmax_t)major(d), (uintmax_t)minor(d), cend);

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

	off_t total_size = get_total_size(link_to_dir, filename);

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

static char *
get_ext_info_long(const char *name, const size_t name_len, int *trim, size_t *ext_len)
{
	char *ext_name = (char *)NULL;

	char *e = strrchr(name, '.');
	if (e && e != name && *(e + 1)) {
		ext_name = e;
		*trim = TRIM_EXT;
		if (unicode == 0)
			*ext_len = name_len - (size_t)(ext_name - name);
		else
			*ext_len = wc_xstrlen(ext_name);
	}

	if ((int)*ext_len >= max_name_len || (int)*ext_len <= 0) {
		*ext_len = 0;
		*trim = TRIM_NO_EXT;
	}

	return ext_name;
}

/* Compose the properties line for the current file name
 * This function is called by list_dir(), in listing.c for each file name
 * in the current directory when running in long view mode */
int
print_entry_props(const struct fileinfo *props, size_t max, const size_t ug_max,
	const size_t ino_max, const size_t fc_max)
{
	/* Let's get file properties and the corresponding colors */

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

	if (colorize == 0) {
		cdate = df_c;
		csize = df_c;
		cid = df_c;
		ctype = df_c;
		cend = df_c;
	}

	char *t_ctype = savestring(ctype, strnlen(ctype, MAX_COLOR));
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
	char *ext_name = (char *)NULL;
	if (cur_len > max) {
		int rest = (int)(cur_len - max);
		trim = TRIM_NO_EXT;
		size_t ext_len = 0;
		ext_name = get_ext_info_long(props->name, plen, &trim, &ext_len);

		xstrsncpy(tname, wname ? wname : props->name, sizeof(tname) - 1);
		int a = (int)plen - rest - 1 - (int)ext_len;
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
		struct perms_t perms = get_file_perms(props->mode);
		snprintf(attr_s, sizeof(attr_s),
			"%s%c%s/%s%c%s%c%s%c%s/%s%c%s%c%s%c%s/%s%c%s%c%s%c%s ",
			t_ctype, file_type, cend,
			perms.cur, perms.ur, perms.cuw, perms.uw, perms.cux, perms.ux, cend,
			perms.cgr, perms.gr, perms.cgw, perms.gw, perms.cgx, perms.gx, cend,
			perms.cor, perms.or, perms.cow, perms.ow, perms.cox, perms.ox, cend);
/*			is_acl(props->name) ? "+" : ""); */
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
			/* Let's use the ISO 8601 date format (YYYY-MM-DD) plus time (HH:MM) */
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
		if (!(S_ISCHR(props->mode) || S_ISBLK(props->mode))
		|| xargs.disk_usage_analyzer == 1) {
			if (full_dir_size == 1 && props->dir == 1)
				size_type = get_size_unit(props->size * (xargs.si == 1 ? 1000 : 1024));
			else
				size_type = get_size_unit(props->size);

			snprintf(size_s, sizeof(size_s), "%s%s%s", csize, size_type
				? size_type : "?", cend);
		} else {
			snprintf(size_s, sizeof(size_s), "%ju,%ju", (uintmax_t)major(props->rdev),
				(uintmax_t)minor(props->rdev));
		}
	} else {
		*size_s = '\0';
	}

				/* ###########################
				 * #      6. FILE INODE      #
				 * ########################### */

	/* Max inode number able to hold: 999 billions! Padding could be as long
	 * as max inode lenght - 1 */
	char ino_s[(12 + 1) * 2];
	*ino_s = '\0';
	if (prop_fields.inode == 1)
		snprintf(ino_s, sizeof(ino_s), "%-*ju ", (int)ino_max, (uintmax_t)props->inode);

				/* ############################
				 * #  7. FILES COUNTER (DIRS) #
				 * ############################ */

	char fc_str[(MAX_COLOR * 2) + 32];
	*fc_str = '\0';
	/* FC_MAX is zero if there are no subdirs in the current dir (or all
	 * of them are empty) */
	if (files_counter == 1 && fc_max > 0) {
		if (props->dir == 1 && props->filesn > 0) {
			snprintf(fc_str, sizeof(fc_str), "%s%-*d%s ", fc_c, (int)fc_max,
				(int)props->filesn, df_c);
		} else {
			snprintf(fc_str, sizeof(fc_str), "%-*c ", (int)fc_max, ' ');
		}
	}

	/* Print stuff */

#ifndef _NO_ICONS
	printf("%s%s%c%s%s%ls%s%s%-*s%s\x1b[0m%s%c\x1b[0m%s%s  " /* File name*/
		   "%s" /* Files counter for dirs */
		   "\x1b[0m%s" /* Inode */
		   "%s" /* Permissions */
		   "%s" /* User and group ID */
		   "%s" /* Time */
		   "%s\n", /* Size / device info */
		colorize ? props->icon_color : "",
		icons ? props->icon : "", icons ? ' ' : 0, df_c,

		colorize ? props->color : "",
		(wchar_t *)tname, trim_diff,
		light_mode ? "\x1b[0m" : df_c, pad, "", df_c,
		trim ? tt_c : "", trim ? '~' : 0,
		trim == TRIM_EXT ? props->color : "",
		trim == TRIM_EXT ? ext_name : "",

		*fc_str ? fc_str : "",
		prop_fields.inode == 1 ? ino_s : "",
		prop_fields.perm != 0 ? attr_s : "",
		prop_fields.ids == 1 ? id_s : "",
		prop_fields.time != 0 ? time_s : "",
		prop_fields.size == 1 ? size_s : "");
#else
	printf("%s%ls%s%s%-*s%s\x1b[0m%s%c\x1b[0m%s%s  " /* File name*/
		   "%s" /* Files counter for dirs */
		   "\x1b[0m%s" /* Inode */
		   "%s" /* Permissions */
		   "%s" /* User and group ID */
		   "%s" /* Time */
		   "%s\n", /* Size / device info */

	    colorize ? props->color : "",
		(wchar_t *)tname, trim_diff,
	    light_mode ? "\x1b[0m" : df_c, pad, "", df_c,
	    trim ? tt_c : "", trim ? TRIMFILE_CHR : 0,
		trim == TRIM_EXT ? props->color : "",
		trim == TRIM_EXT ? ext_name : "",

		*fc_str ? fc_str : "",
		prop_fields.inode == 1 ? ino_s : "",
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
