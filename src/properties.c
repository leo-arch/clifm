/* properties.c -- functions to get files properties */

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

/* These four functions: get_color_size, get_color_size256, get_color_age,
 * and get_color_age256 are based on https://github.com/leahneukirchen/lr
 * (licenced MIT) and modified to fit our needs.
 * All changes are licensed under GPL-2.0-or-later. */

/* get_birthtime() was taken from GNU stat.c, licensed under GPL-3.0-or-later:
 * All changes are licensed under GPL-2.0-or-later. */

#include "helpers.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <grp.h>
#include <pwd.h>

#if defined(__linux__) || defined(__CYGWIN__)
# include <sys/sysmacros.h> /* minor(), major() */
#elif defined(__sun)
//# if defined(ST_BTIME) /* Undefined if compiled with _NO_SUN_BIRTHTIME */
//#  include <attr.h> /* getattrat, nvlist_lookup_uint64_array, nvlist_free */
//# endif /* ST_BTIME */
# include <sys/mkdev.h> /* minor(), major() */
/* For BSD systems, we need sys/types.h, already included in helpers.h */
#endif /* __linux__ || __CYGWIN__ */

#include <readline/tilde.h>

#ifdef LINUX_FILE_ATTRS
# include <sys/ioctl.h>  /* ioctl(2) */
# ifdef __TINYC__
#  undef SYNC_FILE_RANGE_WRITE_AND_WAIT /* Silence redefinition error */
# endif /* __TINYC__ */
# include <linux/fs.h>   /* FS_IOC_GETFLAGS */
# include "properties.h" /* XFS_?????_FL flags */
#endif /* LINUX_FILE_ATTRS */

#ifdef LINUX_FILE_XATTRS
# include <sys/xattr.h>
#endif /* LINUX_FILE_XATTRS */

#ifdef LINUX_FILE_CAPS
# include <sys/capability.h>
#endif /* LINUX_FILE_CAPS */

/* Do we have BSD file flags support? */
#ifndef _BE_POSIX
# if defined(__FreeBSD__) || (defined(__NetBSD__) && !defined(_NO_NETBSD_FFLAGS)) \
|| defined(__OpenBSD__) || defined(__DragonFly__) || defined(__APPLE__)
#  define HAVE_BSD_FFLAGS
#  ifdef __NetBSD__
#   include <util.h> /* flags_to_string() */
#   define FLAGSTOSTR_FUNC(f) flags_to_string((f), "-")
#  else
#   define FLAGSTOSTR_FUNC(f) fflagstostr((f)) /* Provided by unistd.h */
#  endif /* __NetBSD__ */
# endif /* BSD */
#endif /* !_BE_POSIX */

#ifndef _BE_POSIX
# ifdef __linux__
#  if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,46)
#   define HAVE_ACL
#   include <sys/acl.h> /* acl_get_file(), acl_get_entry() */
#   include <acl/libacl.h> /* acl_extended_file_nofollow(), acl_to_any_text() */
#  endif /* Linux >= 2.5.46 */
# endif /* __linux__ */
/*# if defined(__NetBSD__)
#  if __NetBSD_Prereq__(9,99,63)
#   include <sys/acl.h>
#   define HAVE_ACL
#  endif // NetBSD >= 9.99.63
# elif !defined(__HAIKU__) && !defined(__OpenBSD__) && !defined(__sun) \
&& !defined(__DragonFly__)
#  include <sys/acl.h>
#  if defined(__linux__)
#   include <acl/libacl.h>
#  endif // __linux__
#  define HAVE_ACL
# endif // __NetBSD__ */
#endif /* !_BE_POSIX */

#include "aux.h"
#include "checks.h"
#include "colors.h"
#include "messages.h"
#include "misc.h"
#include "properties.h"
#include "readline.h" /* Required by the 'pc' command */

/* A few macros for nano-second precision.
 * Used to print timestamps with the 'p/pp' command. */
#ifndef CLIFM_LEGACY
# define NANO_SEC_MAX 999999999
# if defined(__NetBSD__) || defined(__APPLE__)
#  define ATIMNSEC st_atimespec.tv_nsec
#  define CTIMNSEC st_ctimespec.tv_nsec
#  define MTIMNSEC st_mtimespec.tv_nsec
# else
#  define ATIMNSEC st_atim.tv_nsec
#  define CTIMNSEC st_ctim.tv_nsec
#  define MTIMNSEC st_mtim.tv_nsec
# endif /* __NetBSD__ || __APPLE__ */
#else
/* Let's use any valid value: it won't be used anyway */
#  define ATIMNSEC st_atime
#  define CTIMNSEC st_ctime
#  define MTIMNSEC st_mtime
#endif /* CLIFM_LEGACY */

#ifndef major /* Not defined in Haiku */
# define major(x) ((x >> 8) & 0x7F)
#endif /* major */
#ifndef minor /* Not defined in Haiku */
# define minor(x) (x & 0xFF)
#endif /* minor */

/* Macros to calculate relative timestamps */
#define RT_SECOND 1
#define RT_MINUTE (60  * RT_SECOND)
#define RT_HOUR   (60  * RT_MINUTE)
#define RT_DAY    (24  * RT_HOUR)
#define RT_WEEK   (7   * RT_DAY)
#define RT_MONTH  (30  * RT_DAY)
#define RT_YEAR   (365 * RT_DAY)

/* These macros define the max length for each properties field (long view).
 * These lengths are construed based on how each field is built (i,.e. displayed).
 * We first construct and store (in the stack, to avoid expensive heap
 * allocation) the appropeiate value, and then print them all
 * (print_entry_props()). */

/* 14 colors + 15 single chars + NUL byte */
#define PERM_STR_LEN ((MAX_COLOR * 14) + 16) /* construct_file_perms() */

#define TIME_STR_LEN (MAX_TIME_STR + (MAX_COLOR * 2) + 2) /* construct_timestamp() */

/* construct_human_size() returns a string of at most MAX_HUMAN_SIZE chars (helpers.h) */
#define SIZE_STR_LEN (MAX_HUMAN_SIZE + (MAX_COLOR * 3) + 1) /* construct_file_size() */

/* Let's suppose that IDs go up to 999 billions (12 digits)
 * * 2 IDs + pad (at most 12 more digits) == 36
 * 39 == 36 + colon + space + NUL byte */
//#define ID_STR_LEN   ((MAX_COLOR * 2) + 39) /* construct_id_field() */
#define ID_STR_LEN   ((MAX_COLOR * 2) + (NAME_MAX * 2) + 2 + 4)

/* Max inode number able to hold: 999 billions! Padding could be as long
 * as max inode lenght - 1 */
#define INO_STR_LEN  ((MAX_COLOR * 2) + ((12 + 1) * 2) + 4)

#define FC_STR_LEN   ((MAX_COLOR * 2) + 32) /* construct_files_counter() */

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
	char pad1;
	char pad2;
	char pad3;
	int  pad4;
};

/*
#if defined(__sun) && defined(ST_BTIME)
static struct timespec
get_birthtime(const char *filename)
{
	struct timespec ts;
	ts.tv_sec = ts.tv_nsec = (time_t)-1;
	nvlist_t *response;

	if (getattrat(XAT_FDCWD, XATTR_VIEW_READWRITE, filename, &response) != 0)
		return ts;

	uint64_t *val;
	uint_t n;

	if (nvlist_lookup_uint64_array(response, A_CRTIME, &val, &n) == 0
	&& n >= 2 && val[0] <= LONG_MAX && val[1] < 1000000000 * 2 // for leap seconds) {
		ts.tv_sec = (time_t)val[0];
		ts.tv_nsec = (time_t)val[1];
	}

	nvlist_free(response);

	return ts;
}
#endif // __sun && ST_BTIME */

#if defined(LINUX_FILE_ATTRS)
/* Print file attributes as lsattr(1) would.
 * Bits order as listed by lsattr(1): suSDiadAcEjItTeCxFNPVm
 * See https://git.kernel.org/pub/scm/fs/ext2/e2fsprogs.git/tree/lib/e2p/pf.c */
static int
print_file_attrs(const int aflags)
{
	if (aflags == -1) {
		puts(_("Unavailable"));
		return FUNC_FAILURE;
	}

	const char c = '-';
	char bits[23];

	bits[0] =  (aflags & XFS_SECRM_FL) ? 's' : c;
	bits[1] =  (aflags & XFS_UNRM_FL) ? 'u' : c;
	bits[2] =  (aflags & XFS_SYNC_FL) ? 'S' : c;
	bits[3] =  (aflags & XFS_DIRSYNC_FL) ? 'D' : c;
	bits[4] =  (aflags & XFS_IMMUTABLE_FL) ? 'i' : c;
	bits[5] =  (aflags & XFS_APPEND_FL) ? 'a' : c;
	bits[6] =  (aflags & XFS_NODUMP_FL) ? 'd' : c;
	bits[7] =  (aflags & XFS_NOATIME_FL) ? 'A' : c;
	bits[8] =  (aflags & XFS_COMPR_FL) ? 'c' : c;
	bits[9] =  (aflags & XFS_ENCRYPT_FL) ? 'E' : c;
	bits[10] = (aflags & XFS_JOURNAL_DATA_FL) ? 'j' : c;
	bits[11] = (aflags & XFS_INDEX_FL) ? 'I' : c;
	bits[12] = (aflags & XFS_NOTAIL_FL) ? 't' : c;
	bits[13] = (aflags & XFS_TOPDIR_FL) ? 'T' : c;
	bits[14] = (aflags & XFS_EXTENT_FL) ? 'e' : c;
	bits[15] = (aflags & XFS_NOCOW_FL) ? 'C' : c;
	bits[16] = (aflags & XFS_DAX_FL) ? 'x' : c;
	bits[17] = (aflags & XFS_CASEFOLD_FL) ? 'F' : c;
	bits[18] = (aflags & XFS_INLINE_DATA_FL) ? 'N' : c;
	bits[19] = (aflags & XFS_PROJINHERIT_FL) ? 'P' : c;
	bits[20] = (aflags & XFS_VERITY_FL) ? 'V' : c;
	bits[21] = (aflags & XFS_NOCOMP_FL) ? 'm' : c;
	bits[22] = '\0';

	puts(bits);

	return FUNC_SUCCESS;
}

static int
get_file_attrs(const char *file)
{
# if !defined(FS_IOC_GETFLAGS)
	UNUSED(file);
	return (-1);
# else
	int attr;

	const int fd = open(file, O_RDONLY);
	if (fd == -1)
		return (-1);

	const int ret = ioctl(fd, FS_IOC_GETFLAGS, &attr);
	close(fd);

	return (ret == -1 ? -1 : attr);
# endif /* !FS_IOC_GETFLAGS */
}
#endif /* LINUX_FILE_ATTRS */

static char *
get_link_color(const char *name)
{
	struct stat a;
	char *color = no_c;

	if (lstat(name, &a) == -1)
		return color;

	if (S_ISDIR(a.st_mode)) {
		if (check_file_access(a.st_mode, a.st_uid, a.st_gid) == 1)
			color = get_dir_color(name, a.st_mode, a.st_nlink, -1);
		else
			color = nd_c;
	} else {
		switch (a.st_mode & S_IFMT) {
		case S_IFLNK:  color = stat(name, &a) == -1 ? or_c : ln_c; break;
		case S_IFSOCK: color = so_c; break;
		case S_IFIFO:  color = pi_c; break;
		case S_IFBLK:  color = bd_c; break;
		case S_IFCHR:  color = cd_c; break;
#ifdef SOLARIS_DOORS
		case S_IFDOOR: color = oo_c; break;
		case S_IFPORT: color = oo_c; break;
#endif /* SOLARIS_DOORS */
		case S_IFREG: {
			size_t ext = 0;
			color = get_regfile_color(name, &a, &ext);
			}
			break;
		default: color = df_c; break;
		}
	}

	return color;
}

static off_t
get_total_size(char *filename, int *status)
{
	off_t total_size = 0;

	char _path[PATH_MAX + 1]; *_path = '\0';
	snprintf(_path, sizeof(_path), "%s/", filename);

	if (term_caps.suggestions == 0) {
		fputs("Scanning... ", stdout);
		fflush(stdout);
#ifdef USE_XDU
		total_size = dir_size(*_path ? _path : filename, 1, status);
#else
		total_size = dir_size(*_path ? _path : filename,
			(bin_flags & (GNU_DU_BIN_DU | GNU_DU_BIN_GDU)) ? 1 : 0, status);
#endif /* USE_XDU */
		fputs("\r           \r", stdout);
		fputs(_("Total size: \t"), stdout);
	} else {
		fputs(_("Total size: \t"), stdout);
		HIDE_CURSOR;
		fputs("Scanning...", stdout);
		fflush(stdout);
#ifdef USE_XDU
		total_size = dir_size(*_path ? _path : filename, 1, status);
#else
		total_size = dir_size(*_path ? _path : filename,
			(bin_flags & (GNU_DU_BIN_DU | GNU_DU_BIN_GDU)) ? 1 : 0, status);
#endif /* USE_XDU */
		MOVE_CURSOR_LEFT(11);
		ERASE_TO_RIGHT;
		UNHIDE_CURSOR;
		fflush(stdout);
	}

	return total_size;
}

/* Returns a struct perms_t with the symbolic value and color for each
 * properties field of a file with mode MODE */
static struct perms_t
get_file_perms(const mode_t mode)
{
	struct perms_t p = {0};

	p.ur = p.uw = p.ux = '-';
	p.gr = p.gw = p.gx = '-';
	p.or = p.ow = p.ox = '-';

	p.cur = p.cuw = p.cux = dn_c;
	p.cgr = p.cgw = p.cgx = dn_c;
	p.cor = p.cow = p.cox = dn_c;

	const mode_t val = (mode & (mode_t)~S_IFMT);
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

	if (conf.colorize == 0) {
		p.cur = p.cuw = p.cux = df_c;
		p.cgr = p.cgw = p.cgx = df_c;
		p.cor = p.cow = p.cox = df_c;
	}

	return p;
}

/* Returns FUNC_SUCCESS if the permissions string S (in octal notation)
 * is a valid permissions string, or FUNC_FAILURE otherwise. */
static int
validate_octal_perms(const char *s, const size_t l)
{
	if (l > 4 || l < 3) {
		xerror(_("pc: %s digits. Either 3 or 4 are "
			"expected\n"), l > 4 ? _("Too many") : _("Too few"));
		return FUNC_FAILURE;
	}

	size_t i;
	for (i = 0; s[i]; i++) {
		if (s[i] < '0' || s[i] > '7') {
			xerror(_("pc: '%c': Invalid digit. Values in the range 0-7 "
				"are expected for each field\n"), s[i]);
			return FUNC_FAILURE;
		}
	}

	return FUNC_SUCCESS;
}

/* Validate each field of a symbolic permissions string.
 * Returns FUNC_SUCCESS on success and FUNC_FAILURE on error. */
static int
validate_symbolic_perms(const char *s)
{
	size_t i;
	for (i = 0; i < 9; i++) {
		switch (i) {
		case 0: /* fallthrough */
		case 3: /* fallthrough */
		case 6:
			if (s[i] != '-' && s[i] != 'r') {
				xerror(_("pc: Invalid character in field %zu: "
					"%s-r%s are expected\n"), i + 1, BOLD, NC);
				return FUNC_FAILURE;
			}
			break;

		case 1: /* fallthrough */
		case 4: /* fallthrough */
		case 7:
			if (s[i] != '-' && s[i] != 'w') {
				xerror(_("pc: Invalid character in field %zu: "
					"%s-w%s are expected\n"), i + 1, BOLD, NC);
				return FUNC_FAILURE;
			}
			break;

		case 2: /* fallthrough */
		case 5:
			if (s[i] != '-' && s[i] != 'x' && TOUPPER(s[i]) != 'S') {
				xerror(_("pc: Invalid character in field %zu: "
					"%s-xsS%s are expected\n"), i + 1, BOLD, NC);
				return FUNC_FAILURE;
			}
			break;

		case 8:
			if (s[i] != '-' && s[i] != 'x' && TOUPPER(s[i]) != 'T') {
				xerror(_("pc: Invalid character in field %zu: "
					"%s-xtT%s are expected\n"), i + 1, BOLD, NC);
				return FUNC_FAILURE;
			}
			break;

		default: return FUNC_FAILURE;
		}
	}

	return FUNC_SUCCESS;
}

/* Returns FUNC_SUCCESS if the permissions string S is a valid permissions
 * string, or FUNC_FAILURE otherwise. */
static int
validate_new_perms(const char *s)
{
	const size_t l = strlen(s);
	if (*s >= '0' && *s <= '9')
		return validate_octal_perms(s, l);

	if (l != 9) {
		xerror(_("pc: %s characters: 9 are expected\n"),
			l < 9 ? _("Too few") : _("Too many"));
		return FUNC_FAILURE;
	}

	return validate_symbolic_perms(s);
}

/* Convert permissions in symbolic notation given by S into octal notation
 * and return it as a string. */
static char *
perm2octal(const char *s)
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

	char *p = xnmalloc(64, sizeof(char));
	snprintf(p, 64, "%d%d%d%d", a, b, c, d);

	return p;
}

/* Ask the user to edit permissions given by STR and return the edited string.
 * If DIFF is set to 0, we are editing a single file or multiple files with the
 * same set of permissions. Otherwise, we have multiple files with different
 * sets of permissions. */
static char *
get_new_perms(const char *str, const int diff)
{
	const int poffset_bk = prompt_offset;
	prompt_offset = 3;
	xrename = 2; /* Disable TAB completion for this prompt */
	rl_nohist = 1;

	if (diff == 1) {
		printf(_("%sFiles with different sets of permissions\n"
			"Only shared permission bits are set in the template\n"), tx_c);
	}
	printf(_("%sEdit file permissions (Ctrl-d to quit)\n"
		"Both symbolic and numeric notation are supported\n"), tx_c);
	char m[(MAX_COLOR * 2) + 7];
	snprintf(m, sizeof(m), "\001%s\002>\001%s\002 ", mi_c, tx_c);

	char *new_perms = secondary_prompt(m, str);

	xrename = rl_nohist = 0;
	prompt_offset = poffset_bk;

	if (diff == 0 && new_perms && *str == *new_perms
	&& strcmp(str, new_perms) == 0) {
		fputs(_("pc: Nothing to do\n"), stderr);
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
	struct perms_t p = {0};

	p.ur = p.gr = p.or = 'r';
	p.uw = p.gw = p.ow = 'w';
	p.ux = p.gx = p.ox = 'x';
	int suid = 1, sgid = 1, sticky = 1;

	int i, stat_ready = 0;
	for (i = 0; s[i]; i++) {
		if (stat(s[i], &a) == -1)
			continue;
		if (stat_ready == 1 && a.st_mode != b.st_mode)
			*diff = 1;

		const mode_t val = (a.st_mode & (mode_t)~S_IFMT);
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

		stat_ready = 1;
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
	char *ptr = xnmalloc(10, sizeof(char));
	*diff = 0;

	if (s[1]) { /* Multiple files */
		struct perms_t p = get_common_perms(s, diff);
		snprintf(ptr, 10, "%c%c%c%c%c%c%c%c%c",
			p.ur, p.uw, p.ux, p.gr, p.gw, p.gx, p.or, p.ow, p.ox);
		return ptr;
	}

	/* Single file */
	struct stat a;
	if (stat(s[0], &a) == -1) {
		xerror("stat: '%s': %s\n", s[0], strerror(errno));
		free(ptr);
		return (char *)NULL;
	}

	struct perms_t p = get_file_perms(a.st_mode);
	snprintf(ptr, 10, "%c%c%c%c%c%c%c%c%c",
		p.ur, p.uw, p.ux, p.gr, p.gw, p.gx, p.or, p.ow, p.ox);

	return ptr;
}

/* Interactively change permissions of files passed via ARGS. */
int
set_file_perms(char **args)
{
	if (!args || !args[1] || IS_HELP(args[1])) {
		puts(PC_USAGE);
		return FUNC_SUCCESS;
	}

	size_t i;
	for (i = 1; args[i]; i++) {
		if (!strchr(args[i], '\\'))
			continue;
		char *t = unescape_str(args[i], 0);
		if (t) {
			free(args[i]);
			args[i] = t;
		}
	}

	int diff = 0; /* Either a single file o multiple files with same perms */
	char *pstr = get_perm_str(args + 1, &diff);
	if (!pstr)
		return errno;

	char *new_perms = get_new_perms(pstr, diff);
	free(pstr);

	if (!new_perms)
		return FUNC_SUCCESS;

	if (validate_new_perms(new_perms) != FUNC_SUCCESS) {
		free(new_perms);
		return FUNC_FAILURE;
	}

	char *octal_str = IS_DIGIT(*new_perms) ? new_perms : perm2octal(new_perms);

	int ret = FUNC_SUCCESS;
	size_t n = 0;
	const mode_t mode = (mode_t)strtol(octal_str, 0, 8);
	for (i = 1; args[i]; i++) {
		if (fchmodat(XAT_FDCWD, args[i], mode, 0) == FUNC_SUCCESS) {
			n++;
		} else {
			xerror(_("pc: Changing permissions of '%s': %s\n"),
				args[i], strerror(errno));
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

static char *
get_new_ownership(const char *str, const int diff)
{
	const int poffset_bk = prompt_offset;
	prompt_offset = 3;
	xrename = 3; /* Allow completion only for user and group names */
	rl_nohist = 1;

	if (diff == 1) {
		printf(_("%sFiles with different owners\n"
			"Only common owners are set in the template\n"), tx_c);
	}
	printf(_("%sEdit file ownership (Ctrl-d to quit)\n"
		"Both ID numbers and names are supported\n"), tx_c);
	char m[(MAX_COLOR * 2) + 7];
	snprintf(m, sizeof(m), "\001%s\002>\001%s\002 ", mi_c, tx_c);

	char *new_own = secondary_prompt(m, str);

	xrename = rl_nohist = 0;
	prompt_offset = poffset_bk;

	if (diff == 0 && new_own && *str == *new_own && strcmp(str, new_own) == 0) {
		fputs(_("oc: Nothing to do\n"), stderr);
		free(new_own);
		new_own = (char *)NULL;
	}

	return new_own;
}

static char *
get_common_ownership(char **args, int *exit_status, int *diff)
{
	if (!args || !args[0])
		return (char *)NULL;

	*exit_status = FUNC_SUCCESS;
	struct stat a;
	if (stat(args[0], &a) == -1) {
		xerror("oc: '%s': %s\n", args[0], strerror(errno));
		*exit_status = errno;
		return (char *)NULL;
	}

	int common_uid = 1, common_gid = 1;
	struct stat b;
	size_t i;

	for (i = 1; args[i]; i++) {
		if (stat(args[i], &b) == -1) {
			xerror("oc: '%s': %s\n", args[i], strerror(errno));
			*exit_status = errno;
			return (char *)NULL;
		}
		if (b.st_uid != a.st_uid) {
			common_uid = 0;
			*diff = 1;
		}
		if (b.st_gid != a.st_gid) {
			common_gid = 0;
			*diff = 1;
		}
		if (common_gid == 0 && common_uid == 0)
			return savestring(":", 1);
	}

	struct passwd *owner = getpwuid(a.st_uid);
	struct group *group = getgrgid(a.st_gid);

	const size_t owner_len = (common_uid > 0 && owner && owner->pw_name)
		? wc_xstrlen(owner->pw_name) : 0;
	const size_t group_len = (common_gid > 0 && group && group->gr_name)
		? wc_xstrlen(group->gr_name) : 0;

	if (owner_len + group_len == 0)
		return (char *)NULL;

	const size_t len = owner_len + group_len + 2;
	char *p = xnmalloc(len, sizeof(char));
	snprintf(p, len, "%s%c%s", owner_len > 0 ? owner->pw_name : "",
		group_len > 0 ? ':' : 0,
		group_len > 0 ? group->gr_name : "");

	return p;
}

int
set_file_owner(char **args)
{
	if (!args || !args[1] || IS_HELP(args[1])) {
		puts(OC_USAGE);
		return FUNC_SUCCESS;
	}

	int exit_status = FUNC_SUCCESS;
	int diff = 0;
	char *own = get_common_ownership(args + 1, &exit_status, &diff);

	if (!own)
		return exit_status;

	/* Neither owners nor primary groups are common */
	if (*own == ':' && !*(own + 1))
		*own = '\0';

	char *new_own = get_new_ownership(own, diff);
	free(own);

	if (!new_own || !*new_own) {
		free(new_own);
		return FUNC_SUCCESS;
	}

	char *new_group = (strchr(new_own, ':'));
	if (new_group) {
		*new_group = '\0';
		if (!*(new_group + 1))
			new_group = (char *)NULL;
	}

	struct passwd *owner = (struct passwd *)NULL;
	struct group *group = (struct group *)NULL;

	/* Validate new ownership */
	if (*new_own) { /* NEW_OWN is null in case of ":group" or ":gid" */
		if (is_number(new_own))
			owner = getpwuid((uid_t)atoi(new_own));
		else
			owner = getpwnam(new_own);

		if (!owner || !owner->pw_name) {
			xerror(_("oc: '%s': Invalid user\n"), new_own);
			free(new_own);
			return FUNC_FAILURE;
		}
	}

	if (new_group && *(++new_group)) {
		if (is_number(new_group))
			group = getgrgid((gid_t)atoi(new_group));
		else
			group = getgrnam(new_group);

		if (!group || !group->gr_name) {
			xerror(_("oc: '%s': Invalid group\n"), new_group);
			free(new_own);
			return FUNC_FAILURE;
		}
	}

	/* Change ownership */
	struct stat a;
	size_t new_o = 0, new_g = 0, i;

	for (i = 1; args[i]; i++) {
		if (stat(args[i], &a) == -1) {
			xerror("%s: %s\n", args[i], strerror(errno));
			free(new_own);
			return errno;
		}

		if (fchownat(XAT_FDCWD, args[i],
		*new_own ? owner->pw_uid : a.st_uid,
		new_group ? group->gr_gid : a.st_gid,
		0) == -1) {
			xerror("chown: '%s': %s\n", args[i], strerror(errno));
			exit_status = errno;
			continue;
		}

		if (*new_own && owner->pw_uid != a.st_uid) {
			printf(_("%s->%s %s: User set to %d (%s%s%s)\n"),
				mi_c, NC, args[i], owner->pw_uid, BOLD, owner->pw_name, NC);
			new_o++;
		}

		if (new_group && group->gr_gid != a.st_gid) {
			printf(_("%s->%s %s: Primary group set to %d (%s%s%s)\n"),
				mi_c, NC, args[i], group->gr_gid, BOLD, group->gr_name, NC);
			new_g++;
		}
	}

	if (new_o + new_g == 0) {
		if (exit_status == 0)
			puts(_("oc: Nothing to do"));
	} else {
		printf(_("New ownership set for %zu file(s)\n"), new_o + new_g);
	}

	free(new_own);
	return exit_status;
}

/* Get color shade based on file size (true colors version). */
static void
get_color_size_truecolor(const off_t s, char *str, const size_t len)
{
	const long long base = xargs.si == 1 ? 1000 : 1024;
	uint8_t n = 0;

	if      (s <                base) n = 1; /* Bytes */
	else if (s <           base*base) n = 2; /* Kb */
	else if (s <      base*base*base) n = 3; /* Mb */
	else if (s < base*base*base*base) n = 4; /* Gb */
	else				              n = 5; /* Larger */

	snprintf(str, len, "\x1b[0;%d;38;2;%d;%d;%dm",
		size_shades.shades[n].attr,
		size_shades.shades[n].R,
		size_shades.shades[n].G,
		size_shades.shades[n].B);
}

/* Get color shade based on file time (true colors version). */
static void
get_color_age_truecolor(const time_t t, char *str, const size_t len)
{
	const time_t age = props_now - t;
	uint8_t n;

	if      (age <             0LL) n = 0;
	else if (age <=        60*60LL) n = 1; /* One hour or less */
	else if (age <=     24*60*60LL) n = 2; /* One day or less */
	else if (age <=   7*24*60*60LL) n = 3; /* One weak or less */
	else if (age <= 4*7*24*60*60LL) n = 4; /* One month or less */
	else                            n = 5; /* Older */

	snprintf(str, len, "\x1b[0;%d;38;2;%d;%d;%dm",
		date_shades.shades[n].attr,
		date_shades.shades[n].R,
		date_shades.shades[n].G,
		date_shades.shades[n].B);
}

/* Get color shade based on file size (256 colors version). */
static void
get_color_size256(const off_t s, char *str, const size_t len)
{
	const long long base = xargs.si == 1 ? 1000 : 1024;
	uint8_t n = 0;

	if      (s <                base) n = 1; /* Bytes */
	else if (s <           base*base) n = 2; /* Kb */
	else if (s <      base*base*base) n = 3; /* Mb */
	else if (s < base*base*base*base) n = 4; /* Gb */
	else				              n = 5; /* Larger */

	snprintf(str, len, "\x1b[0;%d;38;5;%dm",
		size_shades.shades[n].attr,
		size_shades.shades[n].R);
}

/* Get color shade based on file time (256 colors version). */
static void
get_color_age256(const time_t t, char *str, const size_t len)
{
	/* PROPS_NOW is global. Calculated before by list_dir() and when
	 * running the 'p' command */
	const time_t age = props_now - t;
	uint8_t n;

	if      (age <             0LL) n = 0;
	else if (age <=        60*60LL) n = 1; /* One hour or less */
	else if (age <=     24*60*60LL) n = 2; /* One day or less */
	else if (age <=   7*24*60*60LL) n = 3; /* One weak or less */
	else if (age <= 4*7*24*60*60LL) n = 4; /* One month or less */
	else                            n = 5; /* Older */

	snprintf(str, len, "\x1b[0;%d;38;5;%dm",
		date_shades.shades[n].attr,
		date_shades.shades[n].R);
}

static void
get_color_size8(const off_t s, char *str, const size_t len)
{
	const long long base = xargs.si == 1 ? 1000 : 1024;
	uint8_t n;

	if      (s <      base*base) n = 1; /* Byte and Kb */
	else if (s < base*base*base) n = 2; /* Mb */
	else		                 n = 3; /* Larger */

	snprintf(str, len, "\x1b[0;%d;%dm",
		size_shades.shades[n].attr,
		size_shades.shades[n].R);
}

static void
get_color_age8(const time_t t, char *str, const size_t len)
{
	const time_t age = props_now - t;
	uint8_t n;

	if      (age <         0LL) n = 0;
	else if (age <=    60*60LL) n = 1; /* One hour or less */
	else if (age <= 24*60*60LL) n = 2; /* One day or less */
	else                        n = 3; /* Older */

	snprintf(str, len, "\x1b[0;%d;%dm",
		date_shades.shades[n].attr,
		date_shades.shades[n].R);
}

/* Get color shade (based on size) for the file whose size is S.
 * Store color in STR, whose len is LEN. */
void
get_color_size(const off_t s, char *str, const size_t len)
{
	switch (size_shades.type) {
	case SHADE_TYPE_8COLORS: get_color_size8(s, str, len); break;
	case SHADE_TYPE_256COLORS: get_color_size256(s, str, len); break;
	case SHADE_TYPE_TRUECOLOR: get_color_size_truecolor(s, str, len); break;
	default: break;
	}
}

/* Get color shade (based on time) for the file whose time is T.
 * Store color in STR, whose len is LEN. */
static void
get_color_age(const time_t t, char *str, const size_t len)
{
	switch (date_shades.type) {
	case SHADE_TYPE_8COLORS: get_color_age8(t, str, len); break;
	case SHADE_TYPE_256COLORS: get_color_age256(t, str, len); break;
	case SHADE_TYPE_TRUECOLOR: get_color_age_truecolor(t, str, len); break;
	default: break;
	}
}

#if defined(LINUX_FILE_XATTRS)
static int
xattr_val_is_printable(const char *val, const size_t len)
{
	size_t i;
	for (i = 0; i < len; i++)
		if (val[len] < ' ') /* Control char (== non-printable) */
			return 0;

	return 1;
}

static int
print_extended_attributes(char *s, const mode_t mode, const int xattr)
{
	if (xattr == 0 || S_ISLNK(mode)) {
		puts(S_ISLNK(mode) ? _("Unavailable") : _("None"));
		return FUNC_SUCCESS;
	}

	ssize_t buflen = 0, keylen = 0, vallen = 0;
	char *buf = (char *)NULL, *key = (char *)NULL, *val = (char *)NULL;

	/* Determine the length of the buffer needed */
	buflen = listxattr(s, NULL, 0);
	if (buflen == -1) {
		printf("error: %s\n", strerror(errno));
		return FUNC_FAILURE;
	}

	if (buflen == 0) {
		puts(_("None"));
		return FUNC_SUCCESS;
	}

	/* Allocate the buffer */
	buf = xnmalloc((size_t)buflen, sizeof(char));

	/* Copy the list of attribute keys to the buffer */
	buflen = listxattr(s, buf, (size_t)buflen);
	if (buflen == -1) {
		xerror("%s\n", strerror(errno));
		free(buf);
		return FUNC_FAILURE;
	}

	/* Loop over the list of zero terminated strings with the
	 * attribute keys. Use the remaining buffer length to determine
	 * the end of the list. */
	key = buf;
	size_t count = 0;
	while (buflen > 0) {

		/* Output attribute key */
		if (count == 0)
			printf("%s: ", key);
		else
			printf("                %s: ", key);
		count++;

		/* Determine length of the value */
		vallen = getxattr(s, key, NULL, 0);
		if (vallen == -1) {
			printf("%s\n", strerror(errno));
		} else if (vallen > 0) {
			/* Output attribute value */
			val = xnmalloc((size_t)vallen + 1, sizeof(char));

			vallen = getxattr(s, key, val, (size_t)vallen);
			if (vallen == -1) {
				printf("%s\n", strerror(errno));
			} else {
				val[vallen] = '\0';
				if (xattr_val_is_printable(val, (size_t)vallen) == 1)
					printf("%s\n", val);
				else
					putchar('\n');
			}

			free(val);
		} else { /* vallen == 0 */
			puts(_("<no value>"));
		}

		/* Forward to next attribute key */
		keylen = (ssize_t)strlen(key) + 1;
		buflen -= keylen;
		key += keylen;
	}

	free(buf);
	return FUNC_SUCCESS;
}
#endif /* LINUX_FILE_XATTRS */

static char *
get_file_type_and_color(const char *filename, const struct stat *attr,
	char *file_type, char **ctype)
{
	char *color = fi_c;

	switch (attr->st_mode & S_IFMT) {
	case S_IFREG:
		*file_type = REG_PCHR;
		if (conf.colorize == 1) {
			size_t ext = 0;
			color = get_regfile_color(filename, attr, &ext);
		}
		break;

	case S_IFDIR:
		*file_type = DIR_PCHR;
		*ctype = di_c;
		if (conf.colorize == 0)
			break;

		if (check_file_access(attr->st_mode, attr->st_uid, attr->st_gid) == 0)
			color = nd_c;
		else
			color = get_dir_color(filename, attr->st_mode, attr->st_nlink, -1);
		break;

	case S_IFLNK: {
		*file_type = LNK_PCHR;
		struct stat a;
		*ctype = ln_c;
		color = stat(filename, &a) == -1 ? or_c : ln_c;
		break;
	}

	case S_IFIFO:  *file_type = FIFO_PCHR; color = *ctype = pi_c; break;
	case S_IFSOCK: *file_type = SOCK_PCHR; color = *ctype = so_c; break;
	case S_IFBLK:  *file_type = BLKDEV_PCHR; color = *ctype = bd_c; break;
	case S_IFCHR:  *file_type = CHARDEV_PCHR; color = *ctype = cd_c; break;
#ifndef _BE_POSIX
# ifdef S_ARCH1
	case S_ARCH1:  *file_type = ARCH1_PCHR; color = *ctype = fi_c; break;
	case S_ARCH2:  *file_type = ARCH2_PCHR; color = *ctype = fi_c; break;
# endif /* S_ARCH1 */
# ifdef SOLARIS_DOORS
	case S_IFDOOR: *file_type = DOOR_PCHR; color = *ctype = oo_c; break;
	case S_IFPORT: *file_type = PORT_PCHR; color = *ctype = oo_c; break;
# endif /* SOLARIS_DOORS */
# ifdef S_IFWHT
	case S_IFWHT: *file_type = WHT_PCHR; color = *ctype = fi_c; break;
# endif /* S_IFWHT */
#endif /* !_BE_POSIX */
	default:       *file_type = UNK_PCHR; color = no_c; break;
	}

	if (conf.colorize == 0)
		color = *ctype = df_c;

	return color;
}

static void
print_file_perms(const struct stat *attr, const char file_type_char,
	const char *file_type_char_color, const int xattr)
{
	char tmp_file_type_char_color[MAX_COLOR + 1];
	xstrsncpy(tmp_file_type_char_color, file_type_char_color,
		sizeof(tmp_file_type_char_color));
	if (xargs.no_bold != 1)
		remove_bold_attr(tmp_file_type_char_color);

	struct perms_t perms = get_file_perms(attr->st_mode);
	printf(_("(%s%04o%s)%s%c%s/%s%c%s%c%s%c%s.%s%c%s%c%s%c%s.%s%c%s%c%s%c%s%s "
		"Links: %s%zu%s "),
		do_c, attr->st_mode & 07777, df_c,
		tmp_file_type_char_color, file_type_char, dn_c,
		perms.cur, perms.ur, perms.cuw, perms.uw, perms.cux, perms.ux, dn_c,
		perms.cgr, perms.gr, perms.cgw, perms.gw, perms.cgx, perms.gx, dn_c,
		perms.cor, perms.or, perms.cow, perms.ow, perms.cox, perms.ox, df_c,
		xattr == 1 ? "@" : "",
		BOLD, (size_t)attr->st_nlink, df_c);
}

static void
print_file_name(char *filename, const char *color, const int follow_link,
	const mode_t mode, char *link_target)
{
	char *wname = wc_xstrlen(filename) == 0
		? replace_invalid_chars(filename) : (char *)NULL;

	if (follow_link == 1) { /* 'pp' command */
		if (link_target && *link_target) {
			char *tmp = abbreviate_file_name(link_target);
			printf(_("\tName: %s%s%s <- %s%s%s\n"), color,
				tmp ? tmp : link_target, df_c, ln_c,
				wname ? wname : filename, df_c);
			if (tmp != link_target)
				free(tmp);
		} else {
			printf(_("\tName: %s%s%s\n"), color,
				wname ? wname : filename, df_c);
		}

		free(wname);
		return;
	}

	/* 'p' command */
	if (!S_ISLNK(mode)) {
		printf(_("\tName: %s%s%s\n"), color, wname ? wname : filename, df_c);
		free(wname);
		return;
	}

	char target[PATH_MAX + 1];
	const ssize_t tlen =
		xreadlink(XAT_FDCWD, filename, target, sizeof(target));

	struct stat a;
	if (tlen != -1 && *target && lstat(target, &a) != -1) {
		char *link_color = get_link_color(target);
		char *name = abbreviate_file_name(target);

		printf(_("\tName: %s%s%s -> %s%s%s\n"), ln_c, wname ? wname
			: filename, df_c, link_color, name ? name : target, df_c);

		if (name != target)
			free(name);

	} else { /* Broken link */
		if (tlen != -1 && *target) {
			printf(_("\tName: %s%s%s -> %s%s%s (broken link)\n"), or_c,
				wname ? wname : filename, df_c, uf_c, target, df_c);
		} else {
			printf(_("\tName: %s%s%s -> ???\n"), or_c, wname ? wname
				: filename, df_c);
		}
	}

	free(wname);
}

#ifdef LINUX_FILE_CAPS
static void
print_capabilities(const char *filename, const int xattr)
{
	cap_t cap = xattr == 1 ? cap_get_file(filename) : NULL;
	if (!cap) {
		puts(_("None"));
		return;
	}

	char *str = cap_to_text(cap, NULL);
	if (str) {
		printf("%s\n", str);
		cap_free(str);
	} else {
		printf("%s\n", strerror(errno));
	}

	cap_free(cap);
}
#endif /* LINUX_FILE_CAPS */

#if defined(HAVE_ACL) && defined(__linux__)
static void
list_acl(const acl_t acl, int *found, const acl_type_t type)
{
	acl_entry_t entry;
	int entryid;
	int num = 0;
	int f = *found;

	for (entryid = ACL_FIRST_ENTRY; ; entryid = ACL_NEXT_ENTRY) {
		if (acl_get_entry(acl, entryid, &entry) != 1)
			break;

		/* We don't care about base ACL entries (owner, group, and others),
		 * that is, entries 0, 1, and 2. */
		if (num == 3) {
			/* acl_to_any_text() is Linux-specific */
			char *val = acl_to_any_text(acl,
				type == ACL_TYPE_DEFAULT ? "default:" : (char *)NULL,
				',', TEXT_ABBREVIATE);

			if (val) {
				printf("%s%s\n", f > 0 ? "\t\t" : "", val);
				f++;
				acl_free(val);
			}

			break;
		}

		num++;
	}

	*found = f;
}
#endif /* HAVE_ACL && __linux__ */

#ifdef HAVE_ACL
/* Print ACLs for FILE, whose mode is MODE.
 * If FILE is a directory, default ACLs are checked besides access ACLs. */
static void
print_file_acl(char *file, const mode_t mode, const int xattr)
{
#ifndef __linux__
	UNUSED(file); UNUSED(mode);
	puts(_("Unavailable"));
	return;
#else
	if (S_ISLNK(mode)) {
		puts(_("Unavailable"));
		return;
	}

	acl_t acl = (acl_t)NULL;
	int found = 0;

	if (!file || !*file || xattr == 0)
		goto END;

	acl = acl_get_file(file, ACL_TYPE_ACCESS);
	if (!acl)
		goto END;

	if ((found = acl_valid(acl)) == -1)
		goto END;

	list_acl(acl, &found, ACL_TYPE_ACCESS);

	if (!S_ISDIR(mode))
		goto END; /* Only directories have default ACLs */

	acl_free(acl);
	acl = acl_get_file(file, ACL_TYPE_DEFAULT);
	if (!acl)
		goto END;

	list_acl(acl, &found, ACL_TYPE_DEFAULT);

END:
	if (found <= 0)
		puts(found == 0 ? _("None") : _("Invalid ACL"));

	acl_free(acl);
#endif /* !__linux__ */
}
#endif /* HAVE_ACL */

static void
print_file_details(char *filename, const struct stat *attr, const char file_type,
	const int file_perm, const int xattr)
{
#if !defined(LINUX_FILE_ATTRS) && !defined(LINUX_FILE_XATTRS) \
&& !defined(HAVE_ACL) && !defined(LINUX_FILE_CAPS)
	UNUSED(filename);
#endif
#if !defined(LINUX_FILE_XATTRS) && !defined(HAVE_ACL) \
&& !defined(LINUX_FILE_CAPS)
	UNUSED(xattr);
#endif

	struct passwd *owner = getpwuid(attr->st_uid);
	struct group *group = getgrgid(attr->st_gid);

	const char *id_color =
		conf.colorize == 0 ? "" : (file_perm == 1 ? dg_c : BOLD);
	const char *cend = conf.colorize == 1 ? df_c : "";

	if (conf.colorize == 1)
		fputs(BOLD, stdout);

	switch (file_type) {
	case REG_PCHR: fputs(_("Regular file"), stdout); break;
	case DIR_PCHR: fputs(_("Directory"), stdout); break;
	case LNK_PCHR: fputs(_("Symbolic link"), stdout); break;
	case FIFO_PCHR: fputs(_("Fifo"), stdout); break;
	case SOCK_PCHR: fputs(_("Socket"), stdout); break;
	case BLKDEV_PCHR: fputs(_("Block special file"), stdout); break;
	case CHARDEV_PCHR: fputs(_("Character special file"), stdout); break;
#ifndef _BE_POSIX
# ifdef S_ARCH1
	case ARCH1_PCHR: fputs(_("Archive state 1"), stdout); break;
	case ARCH2_PCHR: fputs(_("Archive state 2"), stdout); break;
# endif /* S_ARCH1 */
# ifdef SOLARIS_DOORS
	case DOOR_PCHR: fputs(_("Door"), stdout); break;
	case PORT_PCHR: fputs(_("Port"), stdout); break;
# endif /* SOLARIS_DOORS */
# ifdef S_IFWHT
	case WHT_PCHR: fputs(_("Whiteout"), stdout); break;
# endif /* S_IFWHT */
#endif /* !_BE_POSIX */
	default: break;
	}

	if (conf.colorize == 1)
		fputs(cend, stdout);

	printf(_("\tBlocks: %s%jd%s"), BOLD, (intmax_t)attr->st_blocks, cend);
	printf(_("  Block size: %s%d%s"), BOLD, S_BLKSIZE, cend);
	printf(_("  IO Block: %s%jd%s\n"), BOLD, (intmax_t)attr->st_blksize, cend);

	printf(_("Device: %s%ju,%ju%s"), BOLD, (uintmax_t)major(attr->st_dev),
		(uintmax_t)minor(attr->st_dev), cend);
	printf(_("\tInode: %s%ju%s"), BOLD, (uintmax_t)attr->st_ino, cend);

	printf(_("  Uid: %s%u (%s)%s"), id_color, attr->st_uid, !owner
		? _("UNKNOWN") : owner->pw_name, cend);
	printf(_("  Gid: %s%u (%s)%s"), id_color, attr->st_gid, !group
		? _("UNKNOWN") : group->gr_name, cend);

	if (S_ISCHR(attr->st_mode) || S_ISBLK(attr->st_mode)) {
		printf(_("  Device type: %s%ju,%ju%s\n"), BOLD,
			(uintmax_t)major(attr->st_rdev), (uintmax_t)minor(attr->st_rdev),
			cend);
	} else {
		putchar('\n');
	}

#if defined(LINUX_FILE_ATTRS)
	fputs(_("Attributes: \t"), stdout);
	if (S_ISDIR(attr->st_mode) || S_ISREG(attr->st_mode))
		print_file_attrs(get_file_attrs(filename));
	else
		puts(_("Unavailable"));

#elif defined(HAVE_BSD_FFLAGS)
	fputs(_("Flags: \t\t"), stdout);
	char *fflags = (!S_ISDIR(attr->st_mode) && !S_ISLNK(attr->st_mode))
		? FLAGSTOSTR_FUNC(attr->st_flags) : (char *)NULL;
	printf("%s\n", (!fflags || !*fflags) ? "-" : fflags);
	free(fflags);
#endif /* LINUX_FILE_ATTRS */

#if defined(LINUX_FILE_XATTRS)
	fputs(_("Xattributes:\t"), stdout);
	print_extended_attributes(filename, attr->st_mode, xattr);
#endif /* LINUX_FILE_XATTRS */

#if defined(HAVE_ACL)
	fputs(_("ACL-extended:\t"), stdout);
	print_file_acl(filename, attr->st_mode, xattr);
#endif /* HAVE_ACL */

#if defined(LINUX_FILE_CAPS)
	fputs(_("Capabilities:\t"), stdout);
	if (S_ISREG(attr->st_mode))
		print_capabilities(filename, xattr);
	else
		puts(_("Unavailable"));
#endif /* LINUX_FILE_CAPS */
}

/* Return a pointer to the beginning of the %N modifier in the format
 * string FMT. If not found, NULL is returned. */
static char *
has_nsec_modifier(char *fmt)
{
	if (!fmt || !*fmt)
		return (char *)NULL;

	while (*fmt) {
		if (*fmt == '%' && *(fmt + 1) == 'N')
			return fmt;
		++fmt;
	}

	return (char *)NULL;
}

static void
gen_user_time_str(char *buf, size_t buf_size, struct tm *t, const size_t nsec)
{
	char *ptr = has_nsec_modifier(conf.ptime_str);
	if (!ptr) {
		/* GCC (not clang) complains about format being not a string literal.
		 * Let's silence this warning until we find a better approach. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
		strftime(buf, buf_size, conf.ptime_str, t);
#pragma GCC diagnostic pop
		return;
	}

	/* Let's insert nano-seconds in the time format string supplied by the
	 * user (conf.ptime_str). Note that this is not a full-blown expansion,
	 * but rather a hack, and as such it's limited: only the first appearance
	 * of %N (PTR) in the format string will be replaced by the corresponding
	 * nano-seconds. */

	size_t len = 0;
	*ptr = '\0';
	if (ptr != conf.ptime_str)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
		len = strftime(buf, buf_size, conf.ptime_str, t);
#pragma GCC diagnostic pop

	*ptr = '%';
	if (len == 0 && ptr != conf.ptime_str) /* Error or exhausted space in BUF. */
		return;

	len += (size_t)snprintf(buf + len, buf_size - len, "%09zu", nsec);
	if (len >= buf_size) /* Error or exhausted space in BUF. */
		return;

	ptr += 2; /* Move past the %N modifier in format string */
	if (!*ptr)
		return;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
	strftime(buf + len, buf_size - len, ptr, t);
#pragma GCC diagnostic pop

	return;
}

/* Write into BUF, whose size is SIZE, the timestamp TIM, with nanoseconds NSEC,
 * in human readable format.
 * NSEC must be a tv_nsec member of a timespec struct, which, according to the
 * GNU docs (https://www.gnu.org/software/libc/manual/html_node/Time-Types.html),
 * is guarranted to be 0-999999999, so that it should fit into a size_t type.
 * See also https://en.cppreference.com/w/c/chrono/timespec
 * NOTE: coreutils stat.c uses an int to hold a tv_nsec value. */
static void
xgen_time_str(char *buf, const size_t buf_size, const time_t tim,
	const size_t nsec)
{
	if (buf_size == 0)
		return;

	struct tm t;
#ifndef CLIFM_LEGACY
	if (nsec > NANO_SEC_MAX || tim < 0 || !localtime_r(&tim, &t))
#else
	UNUSED(nsec);
	if (tim < 0 || !localtime_r(&tim, &t))
#endif /* CLIFM_LEGACY */
		goto END;

	*buf = '\0';

	if (conf.ptime_str) { /* User-defined time format */
		gen_user_time_str(buf, buf_size, &t, nsec);
		return;
	}

	/* Default value */
	size_t len = strftime(buf, buf_size, "%F %T", &t);
	if (len == 0) /* Error or exhausted space in BUF. */
		return;

#ifndef CLIFM_LEGACY
	len += (size_t)snprintf(buf + len, buf_size - len, ".%09zu ", nsec);
#else
	*(buf + len) = ' ';
	len++;
#endif /* CLIFM_LEGACY */
	if (len >= buf_size) /* Error or exhausted space in BUF. */
		return;

	strftime(buf + len, buf_size - len, "%z", &t);
	return;

END:
	if (buf_size > 1) {
		*buf = '-';
		buf[1] = '\0';
	} else {
		*buf = '\0';
	}
}

static void
print_timestamps(char *filename, const struct stat *attr)
{
#ifndef LINUX_STATX
	UNUSED(filename);
#endif /* !LINUX_STATX */

	const char *cdate = conf.colorize == 1 ? dd_c : "";
	const char *cend = conf.colorize == 1 ? df_c : "";

	char access_time[MAX_TIME_STR];
	char change_time[MAX_TIME_STR];
	char mod_time[MAX_TIME_STR];

	xgen_time_str(access_time, sizeof(access_time), attr->st_atime,
		(size_t)attr->ATIMNSEC);
	xgen_time_str(change_time, sizeof(change_time), attr->st_ctime,
		(size_t)attr->CTIMNSEC);
	xgen_time_str(mod_time, sizeof(mod_time), attr->st_mtime,
		(size_t)attr->MTIMNSEC);

	const char *cadate = cdate;
	const char *ccdate = cdate;
	const char *cmdate = cdate;

	char atf[MAX_SHADE_LEN], mtf[MAX_SHADE_LEN], ctf[MAX_SHADE_LEN];
	*atf = *mtf = *ctf = '\0';
	if (conf.colorize == 1 && !*dd_c) {
		props_now = time(NULL);
		get_color_age(attr->st_atime, atf, sizeof(atf));
		cadate = atf;
		get_color_age(attr->st_mtime, mtf, sizeof(mtf));
		cmdate = mtf;
		get_color_age(attr->st_ctime, ctf, sizeof(ctf));
		ccdate = ctf;
	}

	printf(_("Access: \t%s%s%s\n"), cadate, access_time, cend);
	printf(_("Modify: \t%s%s%s\n"), cmdate, mod_time, cend);
	printf(_("Change: \t%s%s%s\n"), ccdate, change_time, cend);

#ifdef ST_BTIME
	const char *cbdate = cdate;
	char btf[MAX_SHADE_LEN];
	*btf = '\0';
	char creation_time[MAX_TIME_STR];
	time_t bt = 0;

# ifdef LINUX_STATX
	struct statx attrx;
	const int ret = statx(AT_FDCWD, filename, AT_SYMLINK_NOFOLLOW,
		STATX_BTIME, &attrx);
	if (ret == 0 && attrx.stx_mask & STATX_BTIME) {
		xgen_time_str(creation_time, sizeof(creation_time),
			attrx.ST_BTIME.tv_sec, (size_t)attrx.ST_BTIME.tv_nsec);
		bt = attrx.ST_BTIME.tv_sec;
	} else {
		/* Birthtime is not available */
		*creation_time = '-';
		creation_time[1] = '\0';
	}

# elif defined(__sun)
	struct timespec birthtim = get_birthtime(filename);
	if (birthtim.tv_sec == (time_t)-1) { /* Error */
		*creation_time = '-';
		creation_time[1] = '\0';
	} else {
		xgen_time_str(creation_time, sizeof(creation_time), birthtim.tv_sec,
			(size_t)birthtim.tv_nsec);
		bt = birthtim.tv_sec;
	}

# else
	xgen_time_str(creation_time, sizeof(creation_time), attr->ST_BTIME.tv_sec,
		(size_t)attr->ST_BTIME.tv_nsec);
	bt = attr->ST_BTIME.tv_sec;
# endif /* LINUX_STATX */

	if (conf.colorize == 1 && !*dd_c) {
		get_color_age(bt, btf, sizeof(btf));
		cbdate = btf;
	}

	printf(_("Birth: \t\t%s%s%s\n"), cbdate, creation_time, cend);
#elif !defined(_BE_POSIX) /* Birthtime not supported */
	printf(_("Birth: \t\t%s-%s\n"), dn_c, cend);
#endif /* ST_BTIME */
}

static void
print_file_size(char *filename, const struct stat *attr, const int file_perm,
	const int full_dirsize)
{
	const off_t size =
		(FILE_TYPE_NON_ZERO_SIZE(attr->st_mode) || S_TYPEISSHM(attr)
		|| S_TYPEISTMO(attr)) ? FILE_SIZE_PTR(attr) : 0;

	char *size_unit = construct_human_size(size);
	char *csize = dz_c;
	char *cend = conf.colorize == 1 ? df_c : "";

	char sf[MAX_SHADE_LEN];
	*sf = '\0';
	if (conf.colorize == 1 && !*dz_c && !S_ISDIR(attr->st_mode)) {
		get_color_size(size, sf, sizeof(sf));
		csize = sf;
	}

	if (!S_ISDIR(attr->st_mode)) {
		printf(_("Size: \t\t%s%s%s"), csize, size_unit ? size_unit : "?", cend);

		const int bigger_than_bytes = size > (xargs.si == 1 ? 1000 : 1024);

		if (bigger_than_bytes == 1)
			printf(" / %s%jdB%s", csize, (intmax_t)size, cend);

		printf(" (%s%s%s)\n", conf.apparent_size == 1 ? _("apparent")
			: _("disk usage"), (xargs.si == 1 && bigger_than_bytes == 1)
			? ",si" : "", (S_ISREG(attr->st_mode)
			&& (intmax_t)(attr->st_blocks * S_BLKSIZE) < (intmax_t)attr->st_size)
			? ",sparse" : "");

		return;
	}

	if (full_dirsize == 0) /* We're running 'p', not 'pp'. */
		return;

	int du_status = 0;
	const off_t total_size = file_perm == 1
		? get_total_size(filename, &du_status) : (-2);

	if (total_size < 0) {
		if (total_size == -2) /* No access */
			printf(_("Total size: \t%s-%s\n"), dn_c, cend);
		else /* get_total_size returned error (-1) */
			puts("?");
		return;
	}

	const int size_mult_factor = xargs.si == 1 ? 1000 : 1024;

	if (!*dz_c) {
		get_color_size(total_size, sf, sizeof(sf));
		csize = sf;
	}

	char *human_size = construct_human_size(total_size);
	if (!human_size) {
		puts("?");
		return;
	}

#ifndef USE_XDU
	if (bin_flags & (GNU_DU_BIN_DU | GNU_DU_BIN_GDU)) {
#endif // USE_XDU
		char err[sizeof(xf_c) + 6]; *err = '\0';
		if (du_status != 0)
			snprintf(err, sizeof(err), "%s%c%s", xf_c, DU_ERR_CHAR, NC);

		printf("%s%s%s%s ", err, csize, human_size, cend);

		if (total_size > size_mult_factor)
			printf("/ %s%jdB%s ", csize, (intmax_t)total_size, cend);

		printf("(%s%s)\n", conf.apparent_size == 1 ? _("apparent")
			: _("disk usage"), xargs.si == 1 ? ",si" : "");
#ifndef USE_XDU
	} else {
		printf("%s%s%s\n", csize, human_size, cend);
	}
#endif // USE_XDU
}

static int
err_no_file(const char *filename, const char *target, const int errnum)
{
	/* If stat_filename, we're running with --stat(-full): err with program
	 * name. */
	char *errname = stat_filename ? PROGRAM_NAME : "prop";

	if (*target) {
		xerror(_("%s: %s %s->%s %s: Broken symbolic link\n"),
			errname, filename, mi_c, df_c);
	} else {
		xerror("%s: '%s': %s\n", errname, filename, strerror(errnum));
	}

	return errnum;
}

/* Retrieve information for the file named FILENAME in a stat(1)-like fashion.
 * If FOLLOW_LINK is set to 1, in which case we're running the 'pp' command
 * instead of just 'p', symbolic links are followed and directories size is
 * calculated recursively. */
static int
do_stat(char *filename, const int follow_link)
{
	if (!filename || !*filename)
		return FUNC_FAILURE;

	/* Remove leading "./" */
	if (*filename == '.' && *(filename + 1) == '/' && *(filename + 2))
		filename += 2;

	/* Check file existence. */
	struct stat attr;
	int ret = lstat(filename, &attr);

	char link_target[PATH_MAX + 1]; *link_target = '\0';
	if (follow_link == 1 && ret != -1 && S_ISLNK(attr.st_mode)) {
		/* pp: In case of a symlink we want both the symlink name (FILENAME)
		 * and the target name (LINK_TARGET): the Name field in the output
		 * will be printed as follows: "Name: target <- link_name". */
		const ssize_t tlen = xreadlink(XAT_FDCWD, filename, link_target,
			sizeof(link_target));
		if (tlen != -1)
			ret = lstat(link_target, &attr);
		else
			ret = -1;
	}

	if (ret == -1)
		return err_no_file(filename, link_target, errno);

	char file_type = 0; /* File type char indicator. */
	char *ctype = dn_c; /* Color for file type char. */

	const int file_perm =
		check_file_access(attr.st_mode, attr.st_uid, attr.st_gid);

	char *color = get_file_type_and_color(*link_target ? link_target : filename,
		&attr, &file_type, &ctype);

#if defined(LINUX_FILE_XATTRS)
	const int xattr =
		llistxattr(*link_target ? link_target : filename, NULL, 0) > 0;
#else
	const int xattr = 0;
#endif /* LINUX_FILE_XATTRS */

	print_file_perms(&attr, file_type, ctype, xattr);
	print_file_name(filename, color, follow_link, attr.st_mode, link_target);
	print_file_details(filename, &attr, file_type, file_perm, xattr);
	print_timestamps(*link_target ? link_target : filename, &attr);
	print_file_size(filename, &attr, file_perm, follow_link);

	return FUNC_SUCCESS;
}

/* Print file properties (in a stat(1) fashion) for all files passed via ARGS. */
int
properties_function(char **args, const int follow_link)
{
	if (!args)
		return FUNC_FAILURE;

	size_t i;
	int exit_status = FUNC_SUCCESS;

	for (i = 0; args[i]; i++) {
		if (strchr(args[i], '\\')) {
			char *deq_file = unescape_str(args[i], 0);
			if (!deq_file) {
				xerror(_("p: '%s': Cannot unescape file name\n"), args[i]);
				exit_status = FUNC_FAILURE;
				continue;
			}

			free(args[i]);
			args[i] = deq_file;
		}

		if (do_stat(args[i], follow_link) != 0)
			exit_status = FUNC_FAILURE;
	}

	return exit_status;
}

/* Print properties of the file specified by the global variable STAT_FILENAME
 * and exit.
 * If FULL_STAT is set to 1, run with 'pp'. Otherwise use 'p' instead.
 * Used when running with either --stat or --stat-full. */
__attribute__ ((noreturn))
void
do_stat_and_exit(const int full_stat)
{
	/* This function is called very early from main(), immediately after loading
	 * settings, so that we need to set up a few things needed to run the
	 * properties function (do_stat). Namely:
	 * 1. du(1) binary (or xdu).
	 * 2. tmp dir (if not xdu).
	 * 3. Current directory. */
#ifndef USE_XDU
# if defined(HAVE_GNU_DU)
	bin_flags |= GNU_DU_BIN_DU;
# elif !defined(_BE_POSIX)
	char *p = (char *)NULL;
	if ((p = get_cmd_path("gdu")) != NULL)
		{ free(p); bin_flags |= GNU_DU_BIN_GDU; }
# endif /* HAVE_GNU_DU */

	if (!tmp_dir)
		tmp_dir = savestring(P_tmpdir, P_tmpdir_len);
#endif /* !USE_XDU */

	fputs(df_c, stdout);
	cur_ws = 0;
	char tmp[PATH_MAX + 1] = "";
	char *cwd = get_cwd(tmp, sizeof(tmp), 0);
	if (cwd)
		workspaces[cur_ws].path = savestring(cwd, strlen(cwd));

	char *norm_path = *stat_filename == '~'
		? tilde_expand(stat_filename) : (char *)NULL;

	const int ret = do_stat(norm_path ? norm_path : stat_filename, full_stat);

	free(norm_path);
	free(stat_filename);
	exit(ret);
}

/* #################################################################
 * The following functions handle file properties in long view mode
 * #################################################################*/

static char *
get_ext_info_long(const char *name, const size_t name_len, int *trim,
	size_t *ext_len)
{
	char *ext_name = (char *)NULL;

	char *e = strrchr(name, '.');
	if (e && e != name && *(e + 1)) {
		ext_name = e;
		*trim = TRIM_EXT;
		if (conf.unicode == 0)
			*ext_len = name_len - (size_t)(ext_name - name);
		else
			*ext_len = wc_xstrlen(ext_name);
	}

	if ((int)*ext_len >= conf.max_name_len || (int)*ext_len <= 0) {
		*ext_len = 0;
		*trim = TRIM_NO_EXT;
	}

	return ext_name;
}

/* Calculate the relative time of AGE, which is the difference between
 * NOW and the corresponding file time. */
static void
calc_relative_time(const time_t age, char *s)
{
	if (age < 0L) /* Future (AGE, however, is guaranteed to be positive) */
		snprintf(s, MAX_TIME_STR, " -     ");
	else if (age < RT_MINUTE)
		snprintf(s, MAX_TIME_STR, "%*ju  sec", 2, (uintmax_t)age);
	else if (age < RT_HOUR)
		snprintf(s, MAX_TIME_STR, "%*ju  min", 2, (uintmax_t)(age / RT_MINUTE));
	else if (age < RT_DAY)
		snprintf(s, MAX_TIME_STR, "%*ju hour", 2, (uintmax_t)(age / RT_HOUR));
	else if (age < RT_WEEK)
		snprintf(s, MAX_TIME_STR, "%*ju  day", 2, (uintmax_t)(age / RT_DAY));
	else if (age < RT_MONTH) {
		/* RT_MONTH is 30 days. But since Feb has only 28, we get 4 weeks
		 * in some cases, which is weird. Always make 4 weeks into 1 month */
		const long long n = age / RT_WEEK;
		if (n == 4)
			snprintf(s, MAX_TIME_STR, " 1  mon");
		else
			snprintf(s, MAX_TIME_STR, "%*ju week", 2, (uintmax_t)n);
	}
	else if (age < RT_YEAR) {
		const long long n = age / RT_MONTH;
		if (n == 12)
			snprintf(s, MAX_TIME_STR, " 1 year");
		else
			snprintf(s, MAX_TIME_STR, "%*ju  mon", 2, (uintmax_t)n);
	}
	else {
		snprintf(s, MAX_TIME_STR, "%*ju year", 2, (uintmax_t)(age / RT_YEAR));
	}
}

static void
construct_and_print_filename(const struct fileinfo *props,
	const int max_namelen)
{
	/* If file name length is greater than max, truncate it to max (later a
	 * tilde (~) will be appended to let the user know the file name was
	 * truncated). */
	static char tname[(NAME_MAX + 1) * sizeof(wchar_t)];
	int trim = 0;

	/* Handle file names with embedded control characters */
	size_t plen = props->len;
	char *wname = (char *)NULL;
	if (props->len == 0) {
		wname = replace_invalid_chars(props->name);
		plen = wc_xstrlen(wname);
	}

	const filesn_t n = (max_files > UNSET && files > (filesn_t)max_files)
		? (filesn_t)max_files : files;

	size_t cur_len = (size_t)DIGINUM(n) + 1 + plen;
	if (conf.icons == 1)
		cur_len += 3;

	int diff = 0;
	char *ext_name = (char *)NULL;
	if (cur_len > (size_t)max_namelen) {
		int rest = (int)cur_len - max_namelen;
		trim = TRIM_NO_EXT;
		size_t ext_len = 0;
		ext_name = get_ext_info_long(props->name, plen, &trim, &ext_len);

		if (wname)
			xstrsncpy(tname, wname, sizeof(tname));
		else
			memcpy(tname, props->name, props->bytes + 1);

		int trim_point = (int)plen - rest - 1 - (int)ext_len;
		if (trim_point <= 0) {
			trim_point = (int)plen - rest - 1;
			trim = TRIM_NO_EXT;
		}

		diff = u8truncstr(tname, (size_t)trim_point);

		cur_len -= (size_t)rest;
	}

	/* Calculate pad for each file name */
	int pad = max_namelen - (int)cur_len;
	if (pad < 0)
		pad = 0;

	if (!trim || !conf.unicode)
		mbstowcs((wchar_t *)tname, wname ? wname : props->name, NAME_MAX + 1);

	free(wname);

	char *trim_diff = diff > 0 ? gen_diff_str(diff) : "";

	char trim_s[2] = {0};
	*trim_s = trim > 0 ? TRIMFILE_CHR : 0;

	printf("%s%s%s%s%s%ls%s%s%-*s%s\x1b[0m%s%s\x1b[0m%s%s%s  ",
		(conf.colorize == 1 && conf.icons == 1) ? props->icon_color : "",
		conf.icons == 1 ? props->icon : "", conf.icons == 1 ? " " : "", df_c,

		conf.colorize == 1 ? props->color : "",
		(wchar_t *)tname, trim_diff,
		conf.light_mode == 1 ? "\x1b[0m" : df_c, pad, "", df_c,
		trim ? tt_c : "", trim_s,
		trim == TRIM_EXT ? props->color : "",
		trim == TRIM_EXT ? ext_name : "",
		trim == TRIM_EXT ? df_c : "");
}

static int
construct_file_size(const struct fileinfo *props, char *size_str,
	const int size_max)
{
	int file_perm = check_file_access(props->mode, props->uid, props->gid);

	if (prop_fields.size < 1) {
		*size_str = '\0';
		return file_perm;
	}

	if (S_ISCHR(props->mode) || S_ISBLK(props->mode)
	|| (file_perm == 0 && props->dir == 1 && conf.full_dir_size == 1)) {
		snprintf(size_str, SIZE_STR_LEN, "%s%*s%s", dn_c, size_max, "-", df_c);
		return file_perm;
	}

	const off_t size = (FILE_TYPE_NON_ZERO_SIZE(props->mode)
		|| props->type == DT_SHM || props->type == DT_TPO)
		? props->size : 0;

	/* Let's construct the color for the current file size */
	const char *csize = props->dir == 1 ? dz_c : df_c;
	char sf[MAX_SHADE_LEN];
	if (conf.colorize == 1) {
		if (!*dz_c) {
			get_color_size(size, sf, sizeof(sf));
			csize = sf;
		}
	}

	if (prop_fields.size != PROP_SIZE_HUMAN) {
		snprintf(size_str, SIZE_STR_LEN, "%s%*jd%s%c", csize,
			size_max, (intmax_t)size, df_c,
			props->du_status != 0 ? DU_ERR_CHAR : 0);
		return file_perm;
	}

	char err[sizeof(xf_c) + 6]; *err = '\0';
	if (props->dir == 1 && conf.full_dir_size == 1
	&& props->du_status != 0)
		snprintf(err, sizeof(err), "%s%c%s", xf_c, DU_ERR_CHAR, NC);

	snprintf(size_str, SIZE_STR_LEN, "%s%s%*s%s",
		err, csize,
		/* In case of du error, substract from the padding (size_max) the
		 * space used by the error character ('!'). */
		(*err && size_max > 0) ? size_max - 1 : size_max,
		props->human_size ? props->human_size : "?", df_c);

	return file_perm;
}

static void
construct_file_perms(const mode_t mode, char *perm_str, const char file_type,
	const char *ctype)
{
	char tmp_ctype[MAX_COLOR + 1];
	xstrsncpy(tmp_ctype, ctype, sizeof(tmp_ctype));
	if (xargs.no_bold != 1)
		remove_bold_attr(tmp_ctype);

	if (prop_fields.perm == PERM_SYMBOLIC) {
		struct perms_t perms = get_file_perms(mode);
		snprintf(perm_str, PERM_STR_LEN,
			"%s%c%s/%s%c%s%c%s%c%s.%s%c%s%c%s%c%s.%s%c%s%c%s%c%s",
			tmp_ctype, file_type, dn_c,
			perms.cur, perms.ur, perms.cuw, perms.uw, perms.cux, perms.ux, dn_c,
			perms.cgr, perms.gr, perms.cgw, perms.gw, perms.cgx, perms.gx, dn_c,
			perms.cor, perms.or, perms.cow, perms.ow, perms.cox, perms.ox, df_c);

	} else if (prop_fields.perm == PERM_NUMERIC) {
		snprintf(perm_str, PERM_STR_LEN, "%s%04o%s", do_c, mode & 07777, df_c);

	} else {
		*perm_str = '\0';
	}
}

static void
construct_timestamp(char *time_str, const time_t ltime)
{
	if (prop_fields.time == 0) {
		*time_str = '\0';
		return;
	}

	/* Let's construct the color for the current timestamp. */
	char *cdate = dd_c;
	char df[MAX_SHADE_LEN];
	if (conf.colorize == 1 && !*dd_c) {
		get_color_age(ltime, df, sizeof(df));
		cdate = df;
	}

	char file_time[MAX_TIME_STR];
	struct tm t;

	if (ltime >= 0 && localtime_r(&ltime, &t)) {
		/* PROPS_NOW (global) is set by list_dir(), in listing.c before
		 * calling print_entry_props(), which calls this function. */
		const time_t age = props_now - ltime;
		/* AGE is negative if file time is in the future. */

		if (conf.relative_time == 1) {
			calc_relative_time(age < 0 ? age - (age * 2) : age, file_time);
		} else {
			/* If not user defined, let's mimic ls(1) behavior: a file is
			 * considered recent if it is within the past six months. */
			const int recent = age >= 0 && age < 14515200LL;
			/* 14515200 == 6*4*7*24*60*60 == six months */
			const char *tfmt = conf.time_str ? conf.time_str :
				(recent ? DEF_TIME_STYLE_RECENT : DEF_TIME_STYLE_OLDER);

			/* GCC (not clang) complains about tfmt being not a string
			 * literal. Let's silence this warning until we find a better
			 * approach. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
			strftime(file_time, sizeof(file_time), tfmt, &t);
#pragma GCC diagnostic pop
		}
	} else {
		/* INVALID_TIME_STR (global) is generated at startup by
		 * check_time_str(), in init.c. */
		xstrsncpy(file_time, invalid_time_str, sizeof(file_time));
	}

	snprintf(time_str, TIME_STR_LEN, "%s%s%s", cdate, *file_time
		? file_time : "?", df_c);
}

static void
construct_id_field(const struct fileinfo *props, char *id_str,
	const char *id_color, const struct maxes_t *maxes)
{
	if (prop_fields.ids == 0) {
		*id_str = '\0';
		return;
	}

	const char *dim = conf.colorize == 0 ? "" : "\x1b[2m";

	if (prop_fields.ids == PROP_ID_NUM) {
		snprintf(id_str, ID_STR_LEN, "%s%-*u %s%-*u%s", id_color,
			maxes->id_user, props->uid, dim, maxes->id_group,
			props->gid, df_c);
	} else { /* PROPS_ID_NAME */
		snprintf(id_str, ID_STR_LEN, "%s%-*s %s%-*s%s", id_color,
			maxes->id_user, props->uid_i.name, dim,
			maxes->id_group, props->gid_i.name, df_c);
	}
}

static void
construct_files_counter(const struct fileinfo *props, char *fc_str,
	const int fc_max)
{
	/* FC_MAX is zero if there are no subdirs in the current dir */
	if (conf.files_counter == 0 || fc_max == 0 || prop_fields.counter == 0) {
		*fc_str = '\0';
		return;
	}

	if (props->dir == 1 && props->filesn > 0) {
		snprintf(fc_str, FC_STR_LEN, "%s%*d%s", fc_c, (int)fc_max,
			(int)props->filesn, df_c);
	} else {
		snprintf(fc_str, FC_STR_LEN, "%s%*c%s", dn_c, (int)fc_max,
			'-', df_c);
	}
}

static void
set_file_type_and_color(const mode_t mode, char *type, char **color)
{
	switch (mode & S_IFMT) {
	case S_IFREG:  *type = REG_PCHR; break;
	case S_IFDIR:  *type = DIR_PCHR; *color = di_c; break;
	case S_IFLNK:  *type = LNK_PCHR; *color = ln_c; break;
	case S_IFIFO:  *type = FIFO_PCHR; *color = pi_c; break;
	case S_IFSOCK: *type = SOCK_PCHR; *color = so_c; break;
	case S_IFBLK:  *type = BLKDEV_PCHR; *color = bd_c; break;
	case S_IFCHR:  *type = CHARDEV_PCHR; *color = cd_c; break;
#ifndef _BE_POSIX
# ifdef S_ARCH1
	case S_ARCH1:  *type = ARCH1_PCHR; *color = fi_c; break;
	case S_ARCH2:  *type = ARCH2_PCHR; *color = fi_c; break;
# endif /* S_ARCH1 */
# ifdef SOLARIS_DOORS
	case S_IFDOOR: *type = DOOR_PCHR; *color = oo_c; break;
	case S_IFPORT: *type = PORT_PCHR; *color = oo_c; break;
# endif /* SOLARIS_DOORS */
# ifdef S_IFWHT
	case S_IFWHT:  *type = WHT_PCHR; *color = fi_c; break;
# endif /* S_IFWHT */
#endif /* !_BE_POSIX */
	default:       *type = UNK_PCHR; break;
	}

	if (conf.colorize == 0)
		*color = df_c;
}

/* Compose the properties line for the current file name.
 * This function is called by list_dir(), in listing.c, for each file name
 * in the current directory when running in long view mode (after
 * printing the corresponding ELN). */
int
print_entry_props(const struct fileinfo *props, const struct maxes_t *maxes,
	const int have_xattr)
{
	char file_type = 0; /* File type indicator */
	char *ctype = dn_c; /* Color for file type indicator */

	set_file_type_and_color(props->mode, &file_type, &ctype);

	/* Let's compose each properties field individually to be able to
	 * print only the desired ones. This is specified via the PropFields
	 * option in the config file. */

	construct_and_print_filename(props, maxes->name);

	char perm_str[PERM_STR_LEN];
	construct_file_perms(props->mode, perm_str, file_type, ctype);

	char time_str[TIME_STR_LEN];
	construct_timestamp(time_str, props->ltime);

	/* size_str is either file size or "major,minor" IDs in case of special
	 * files (char and block devices). */
	char size_str[SIZE_STR_LEN];
	const int file_perm = construct_file_size(props, size_str, maxes->size);

	const char *id_color = (file_perm == 1 && conf.colorize == 1) ? dg_c : df_c;
	char id_str[ID_STR_LEN];
	construct_id_field(props, id_str, id_color, maxes);

	char ino_str[INO_STR_LEN];
	*ino_str = '\0';
	if (prop_fields.inode == 1) {
		snprintf(ino_str, INO_STR_LEN, "\x1b[0m%s%*ju%s", tx_c,
			(int)maxes->inode, (uintmax_t)props->inode, df_c);
	}

	char fc_str[FC_STR_LEN];
	construct_files_counter(props, fc_str, maxes->files_counter);

	/* We only need a single character to print the xattributes indicator,
	 * which would be normally printed like this:
	 * printf("%c", x ? 'x' : 0);
	 * However, some terminals, like 'cons25', print the 0 above graphically,
	 * as a space, which is not what we want here. To fix this, let's
	 * construct this char as a string. */
	char xattr_str[2] = {0};
	*xattr_str = have_xattr == 1 ? (props->xattr == 1 ? XATTR_CHAR : ' ') : 0;

	/* Print stuff */
	for (size_t i = 0; prop_fields_str[i] && i < PROP_FIELDS_SIZE; i++) {
		int print_space = prop_fields_str[i + 1] ? 1 : 0;
		switch(prop_fields_str[i]) {
		case 'f': fputs(fc_str, stdout); break;
		case 'd': if (*ino_str) fputs(ino_str, stdout); break;
		case 'p': /* fallthrough */
		case 'n': fputs(perm_str, stdout);
			if (*xattr_str) fputs(xattr_str, stdout);
			break;
		case 'i': /* fallthrough */
		case 'I': fputs(id_str, stdout); break;
		case 'a': /* fallthrough */
		case 'b': /* fallthrough */
		case 'm': /* fallthrough */
		case 'c': fputs(time_str, stdout); break;
		case 's': /* fallthrough */
		case 'S': fputs(size_str, stdout); break;
		default: print_space = 0; break;
		}
		if (print_space == 1)
			putchar(' ');
	}
	putchar('\n');

	return FUNC_SUCCESS;
}

/* Print final stats for the disk usage analyzer mode: total and largest file. */
void
print_analysis_stats(const off_t total, const off_t largest,
	const char *color, const char *name)
{
	char *t = (char *)NULL;
	char *l = (char *)NULL;

	if (prop_fields.size == PROP_SIZE_HUMAN) {
		const char *p_total = construct_human_size(total);
		t = savestring(p_total, strlen(p_total));
		const char *p_largest = construct_human_size(largest);
		l = savestring(p_largest, strlen(p_largest));
	} else {
		t = xnmalloc(32, sizeof(char));
		l = xnmalloc(32, sizeof(char));
		snprintf(t, 32, "%ju", (uintmax_t)total);
		snprintf(l, 32, "%ju", (uintmax_t)largest);
	}

	char *tsize = BOLD_GREEN, *lsize = BOLD_GREEN;

	char ts[MAX_SHADE_LEN], ls[MAX_SHADE_LEN];
	if (term_caps.color > 0 && !*dz_c) {
		get_color_size(total, ts, sizeof(ts));
		tsize = ts;
		get_color_size(largest, ls, sizeof(ls));
		lsize = ls;
	}

	printf(_("Total size:   %s%s%s\n"
		"Largest file: %s%s%s %c%s%s%s%c\n"),
		conf.colorize == 1 ? tsize : "" , t ? t : "?",
		conf.colorize == 1 ? tx_c : "",
		conf.colorize == 1 ? lsize : "" , l ? l : "?",
		conf.colorize == 1 ? tx_c : "",
		name ? '[' : 0,
		(conf.colorize == 1 && color) ? color : "",
		name ? name : "", tx_c,
		name ? ']' : 0);

	free(t);
	free(l);
}
