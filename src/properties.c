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

/* properties.c -- Home of the p/pp, pc, oc, and stats commands */

/* These four functions: get_color_size, get_color_size256, get_color_age,
 * and get_color_age256 are based on https://github.com/leahneukirchen/lr
 * (licenced MIT) and modified to fit our needs.
 * All changes are licensed under GPL-2.0-or-later. */

/* get_birthtime() was taken from GNU stat.c, licensed under GPL-3.0-or-later:
 * All changes are licensed under GPL-2.0-or-later. */

#include "helpers.h"

#include <errno.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <grp.h>
#include <pwd.h>

#if defined(__linux__) || defined(__CYGWIN__)
# include <sys/sysmacros.h> /* minor(), major() */
#elif defined(__sun)
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
#endif /* !_BE_POSIX */

#include "aux.h"
#include "checks.h" /* check_file_access(), is_number() */
#include "colors.h" /* get_dir_color(), get_regfile_color() */
#include "messages.h"
# include "mime.h"  /* xmagic() */
#include "misc.h"
#include "properties.h"
#include "readline.h"   /* Required by the 'pc' command */
#include "xdu.h" /* dir_info(), dir_size() */

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

#if defined(LINUX_FILE_ATTRS)
/* Print file attributes as lsattr(1) would.
 * Bits order as listed by lsattr(1): suSDiadAcEjItTeCxFNPVm
 * See https://git.kernel.org/pub/scm/fs/ext2/e2fsprogs.git/tree/lib/e2p/pf.c */
static int
print_file_attrs(const int aflags)
{
	if (aflags == -1) {
		puts(_("unavailable"));
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
		color = get_dir_color(name, &a, -1);
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

static struct perms_t
set_invalid_file_perms(void)
{
	struct perms_t p = {0};

	p.ur = p.uw = p.ux = UNKNOWN_CHR;
	p.gr = p.gw = p.gx = UNKNOWN_CHR;
	p.or = p.ow = p.ox = UNKNOWN_CHR;

	p.cur = p.cuw = p.cux = df_c;
	p.cgr = p.cgw = p.cgx = df_c;
	p.cor = p.cow = p.cox = df_c;

	return p;
}

/* Returns a struct perms_t with the symbolic value and color for each
 * properties field of a file with mode MODE */
struct perms_t
get_file_perms(const mode_t mode)
{
	struct perms_t p = {0};

	if (mode == 0) /* stat(2) err */
		return set_invalid_file_perms();

	p.cur = p.cuw = p.cux = dn_c;
	p.cgr = p.cgw = p.cgx = dn_c;
	p.cor = p.cow = p.cox = dn_c;

	p.ur = p.uw = p.ux = '-';
	p.gr = p.gw = p.gx = '-';
	p.or = p.ow = p.ox = '-';

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
	if (IS_DIGIT(*s))
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
	alt_prompt = PERMISSIONS_PROMPT;
	rl_nohist = 1;

	if (diff == 1) {
		printf(_("%sFiles with different sets of permissions\n"
			"Only shared permission bits are set in the template\n"), tx_c);
	}
	printf(_("%sEdit file permissions (Ctrl+d to quit)\n"
		"Both symbolic and numeric notation are supported\n"), tx_c);
	char m[(MAX_COLOR * 2) + 7];
	snprintf(m, sizeof(m), "\001%s\002>\001%s\002 ", mi_c, tx_c);

	char *new_perms = secondary_prompt(m, str);

	alt_prompt = rl_nohist = 0;
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
	alt_prompt = OWNERSHIP_PROMPT;
	rl_nohist = 1;

	if (diff == 1) {
		printf(_("%sFiles with different owners\n"
			"Only common owners are set in the template\n"), tx_c);
	}
	printf(_("%sEdit file ownership (Ctrl+d to quit)\n"
		"Both ID numbers and names are supported\n"), tx_c);
	char m[(MAX_COLOR * 2) + 7];
	snprintf(m, sizeof(m), "\001%s\002>\001%s\002 ", mi_c, tx_c);

	char *new_own = secondary_prompt(m, str);

	alt_prompt = rl_nohist = 0;
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
			continue;
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
	if (*own == ':' && !own[1])
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
		if (!new_group[1])
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
			xerror("stat: '%s': %s\n", args[i], strerror(errno));
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
			printf(_("%s%s%s %s: User set to %d (%s%s%s)\n"),
				mi_c, SET_MSG_PTR, NC, args[i], owner->pw_uid,
				BOLD, owner->pw_name, NC);
			new_o++;
		}

		if (new_group && group->gr_gid != a.st_gid) {
			printf(_("%s%s%s %s: Primary group set to %d (%s%s%s)\n"),
				mi_c, SET_MSG_PTR, NC, args[i], group->gr_gid, BOLD,
				group->gr_name, NC);
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

/* Get color shade based on file size. */
void
get_color_size(const off_t s, char *str, const size_t len)
{
	const long long base = xargs.si == 1 ? 1000 : 1024;
	int n = 0;

	/* Keep compatibility with old-style config, which only accepted 3 shades
	 * for 8 color shades type. This check should be removed in the future. */
	if (size_shades_old_style == 1) {
		if      (s <      base*base) n = 1; /* Byte and Kb */
		else if (s < base*base*base) n = 2; /* Mb */
		else		                 n = 3; /* Larger */
	} else {
		if      (s <                base) n = 1; /* Bytes */
		else if (s <           base*base) n = 2; /* Kb */
		else if (s <      base*base*base) n = 3; /* Mb */
		else if (s < base*base*base*base) n = 4; /* Gb */
		else				              n = 5; /* Larger */
	}

	switch (size_shades.type) {
	case SHADE_TYPE_8COLORS:
		snprintf(str, len, "\x1b[0;%d;%dm",
			size_shades.shades[n].attr,
			size_shades.shades[n].R);
		break;

	case SHADE_TYPE_256COLORS:
		snprintf(str, len, "\x1b[0;%d;38;5;%dm",
			size_shades.shades[n].attr,
			size_shades.shades[n].R);
		break;

	case SHADE_TYPE_TRUECOLOR:
		snprintf(str, len, "\x1b[0;%d;38;2;%d;%d;%dm",
			size_shades.shades[n].attr,
			size_shades.shades[n].R,
			size_shades.shades[n].G,
			size_shades.shades[n].B);
		break;

	default: break;
	}
}

/* Get color shade based on file time. */
void
get_color_age(const time_t t, char *str, const size_t len)
{
	const time_t age = props_now - t;
	int n;

	/* Keep compatibility with old-style config, which only accepted 3 shades
	 * for 8 color shades type. This check should be removed in the future. */
	if (date_shades_old_style == 1) {
		if      (age <         0LL) n = 0;
		else if (age <=    60LL*60) n = 1; /* One hour or less */
		else if (age <= 24LL*60*60) n = 2; /* One day or less */
		else                        n = 3; /* Older */
	} else {
		if      (age <             0LL) n = 0;
		else if (age <=        60LL*60) n = 1; /* One hour or less */
		else if (age <=     24LL*60*60) n = 2; /* One day or less */
		else if (age <=   7LL*24*60*60) n = 3; /* One weak or less */
		else if (age <= 4LL*7*24*60*60) n = 4; /* One month or less */
		else                            n = 5; /* Older */
	}

	switch (date_shades.type) {
	case SHADE_TYPE_8COLORS:
		snprintf(str, len, "\x1b[0;%d;%dm",
			date_shades.shades[n].attr,
			date_shades.shades[n].R);
		break;

	case SHADE_TYPE_256COLORS:
		snprintf(str, len, "\x1b[0;%d;38;5;%dm",
			date_shades.shades[n].attr,
			date_shades.shades[n].R);
		break;

	case SHADE_TYPE_TRUECOLOR:
		snprintf(str, len, "\x1b[0;%d;38;2;%d;%d;%dm",
			date_shades.shades[n].attr,
			date_shades.shades[n].R,
			date_shades.shades[n].G,
			date_shades.shades[n].B);
		break;

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
		puts(S_ISLNK(mode) ? _("unavailable") : _("none"));
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
		puts(_("none"));
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

		color = get_dir_color(filename, attr, -1);
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
		xattr == 1 ? XATTR_STR : "",
		BOLD, (size_t)attr->st_nlink, df_c);
}

static void
print_filename(char *filename, const char *color, const int follow_link,
	const mode_t mode, char *link_target)
{
	char *wname = wc_xstrlen(filename) == 0
		? replace_invalid_chars(filename) : (char *)NULL;

	char name[(NAME_MAX * sizeof(wchar_t)) + 3]; *name = '\0';
	if (detect_space(wname ? wname : filename) == 1)
		snprintf(name, sizeof(name), "'%s'", wname ? wname : filename);

	char *n = *name ? name : (wname ? wname : filename);

	if (follow_link == 1) { /* 'pp' command */
		if (link_target && *link_target) {
			char t[PATH_MAX * sizeof(wchar_t) + 3]; *t = '\0';
			if (detect_space(link_target) == 1)
				snprintf(t, sizeof(t), "'%s'", link_target);
			printf(_("\tName: %s%s%s %s%s%s %s%s%s\n"), color,
				*t ? t : link_target, df_c, dn_c,
				term_caps.unicode == 1 ? MSG_PTR_STR_LEFT_U : MSG_PTR_STR_LEFT,
				df_c, ln_c, n, df_c);
		} else {
			printf(_("\tName: %s%s%s\n"), color, n, df_c);
		}

		free(wname);
		return;
	}

	/* 'p' command */
	if (!S_ISLNK(mode)) {
		printf(_("\tName: %s%s%s\n"), color, n, df_c);
		free(wname);
		return;
	}

	char target[PATH_MAX + 1];
	const ssize_t tlen =
		xreadlink(XAT_FDCWD, filename, target, sizeof(target));

	char t[PATH_MAX * sizeof(wchar_t) + 3]; *t = '\0';
	if (tlen != -1 && *target && detect_space(target) == 1)
		snprintf(t, sizeof(t), "'%s'", target);

	struct stat a;
	if (tlen != -1 && *target && lstat(target, &a) != -1) {
		char *link_color = get_link_color(target);
		printf(_("\tName: %s%s%s %s%s%s %s%s%s\n"), ln_c, n, df_c,
			dn_c, SET_MSG_PTR, df_c, link_color, *t ? t : target, df_c);

	} else { /* Broken link */
		if (tlen != -1 && *target) {
			printf(_("\tName: %s%s%s %s%s%s %s%s%s (broken link)\n"), or_c,
				n, df_c, dn_c, SET_MSG_PTR, df_c, uf_c, *t ? t : target, df_c);
		} else {
			printf(_("\tName: %s%s%s %s%s ???%s\n"), or_c, n, df_c,
				dn_c, SET_MSG_PTR, df_c);
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
		puts(_("none"));
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
list_acl(acl_t acl, int *found, const acl_type_t type)
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
	puts(_("unavailable"));
	return;
#else
	if (S_ISLNK(mode)) {
		puts(_("unavailable"));
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
		puts(found == 0 ? _("none") : _("invalid ACL"));

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

	const char *gid_color =
		conf.colorize == 0 ? "" : (file_perm == 1 ? dg_c : BOLD);
	const char *uid_color =
		conf.colorize == 0 ? "" : (file_perm == 1 ? du_c : BOLD);
	const char *cend = conf.colorize == 1 ? df_c : "";

	if (conf.colorize == 1)
		fputs(BOLD, stdout);

	switch (file_type) {
	case REG_PCHR: fputs(_("Regular file"), stdout); break;
	case DIR_PCHR: fputs(_("Directory"), stdout); break;
	case LNK_PCHR: fputs(_("Symbolic link"), stdout); break;
	case FIFO_PCHR: fputs(_("Fifo    "), stdout); break;
	case SOCK_PCHR: fputs(_("Socket  "), stdout); break;
	case BLKDEV_PCHR: fputs(_("Block special file"), stdout); break;
	case CHARDEV_PCHR: fputs(_("Character special file"), stdout); break;
#ifndef _BE_POSIX
# ifdef S_ARCH1
	case ARCH1_PCHR: fputs(_("Archive state 1"), stdout); break;
	case ARCH2_PCHR: fputs(_("Archive state 2"), stdout); break;
# endif /* S_ARCH1 */
# ifdef SOLARIS_DOORS
	case DOOR_PCHR: fputs(_("Door    "), stdout); break;
	case PORT_PCHR: fputs(_("Port    "), stdout); break;
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

	printf(_("  Uid: %s%u (%s)%s"), uid_color, attr->st_uid, !owner
		? _("UNKNOWN") : owner->pw_name, cend);
	printf(_("  Gid: %s%u (%s)%s"), gid_color, attr->st_gid, !group
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
		puts(_("unavailable"));

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
		puts(_("unavailable"));
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
		if (*fmt == '%' && fmt[1] == 'N')
			return fmt;
		++fmt;
	}

	return (char *)NULL;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
static void
gen_user_time_str(char *buf, size_t buf_size, struct tm *t, const size_t nsec)
{
	char *ptr = has_nsec_modifier(conf.ptime_str);
	if (!ptr) {
		/* GCC (not clang) complains about format being not a string literal.
		 * Let's silence this warning until we find a better approach. */
		strftime(buf, buf_size, conf.ptime_str, t);
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
		len = strftime(buf, buf_size, conf.ptime_str, t);

	*ptr = '%';
	if (len == 0 && ptr != conf.ptime_str) /* Error or exhausted space in BUF. */
		return;

	len += (size_t)snprintf(buf + len, buf_size - len, "%09zu", nsec);
	if (len >= buf_size) /* Error or exhausted space in BUF. */
		return;

	ptr += 2; /* Move past the %N modifier in format string. */
	if (!*ptr)
		return;

	strftime(buf + len, buf_size - len, ptr, t);
}
#pragma GCC diagnostic pop

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

	if (conf.ptime_str) { /* User-defined time format. */
		gen_user_time_str(buf, buf_size, &t, nsec);
		return;
	}

	/* Default value. */
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
#if !defined(ST_BTIME) || (!defined(LINUX_STATX) && !defined(__sun))
	UNUSED(filename);
#endif /* !ST_BIME || (!LINUX_STAT && !__sun) */

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
		/* Birthtime is not available. */
		*creation_time = '-';
		creation_time[1] = '\0';
	}

# elif defined(__sun)
	struct timespec birthtim = get_birthtime(filename);
	if (birthtim.tv_sec == (time_t)-1) { /* Error. */
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
#elif !defined(_BE_POSIX) /* Birthtime is not supported. */
	printf(_("Birth: \t\t%s-%s\n"), dn_c, cend);
#endif /* ST_BTIME */
}

static int
err_no_file(const char *filename, const char *target, const int errnum)
{
	char *errname = xargs.stat > 0 ? PROGRAM_NAME : "prop";

	if (*target) {
		xerror(_("%s: %s %s%s%s %s: Broken symbolic link\n"),
			errname, filename, mi_c, SET_MSG_PTR, df_c, target);
	} else {
		xerror("%s: '%s': %s\n", errname, filename, strerror(errnum));
	}

	return errnum;
}

static void
print_file_mime(const char *name)
{
	fputs("MIME type:\t", stdout);
	char *n = xmagic(name, MIME_TYPE);
	if (n) {
		printf("%s\n", n);
		free(n);
	} else {
		printf("%s%c%s\n", dn_c, UNKNOWN_CHR, df_c);
	}
}

#ifdef USE_DU1
static off_t
get_total_size(char *filename, int *status)
{
	off_t total_size = 0;

	char _path[PATH_MAX + 1]; *_path = '\0';
	snprintf(_path, sizeof(_path), "%s/", filename);

	fputs(_("Total size: \t"), stdout);

#define SCANNING_MSG "Scanning..."
	if (term_caps.suggestions == 1) {
		HIDE_CURSOR;
		fputs(dn_c, stdout);
		fputs(SCANNING_MSG, stdout);
		fflush(stdout);
	}
	fflush(stdout);

	total_size = dir_size(*_path ? _path : filename, 1, status);

	if (term_caps.suggestions == 1) {
		MOVE_CURSOR_LEFT((int)sizeof(SCANNING_MSG) - 1);
		ERASE_TO_RIGHT;
		UNHIDE_CURSOR;
		fputs(df_c, stdout);
		fflush(stdout);
	}

	return total_size;
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
			printf(" / %s%jd B%s", csize, (intmax_t)size, cend);

		const int is_sparse = (S_ISREG(attr->st_mode) &&
			(intmax_t)(attr->st_blocks * S_BLKSIZE) < (intmax_t)attr->st_size
			 && attr->st_blocks > 0);

		printf(" (%s%s%s)\n", conf.apparent_size == 1 ? _("apparent")
			: _("disk usage"), (xargs.si == 1 && bigger_than_bytes == 1)
			? ",si" : "", is_sparse == 1 ? ",sparse" : "");

		return;
	}

	if (full_dirsize == 0) /* We're running 'p', not 'pp'. */
		return;

	int du_status = 0;
	const off_t total_size = file_perm == 1
		? get_total_size(filename, &du_status) : (-2);

	if (total_size < 0) {
		if (total_size == -2) /* No access. */
			printf(_("Total size: \t%s%c%s\n"), dn_c, UNKNOWN_CHR, cend);
		else /* get_total_size returned error (-1). */
			puts(UNKNOWN_STR);
		return;
	}

	const int size_mult_factor = xargs.si == 1 ? 1000 : 1024;

	if (!*dz_c) {
		get_color_size(total_size, sf, sizeof(sf));
		csize = sf;
	}

	char *human_size = construct_human_size(total_size);
	if (!human_size) {
		puts(UNKNOWN_STR);
		return;
	}

	if (bin_flags & (GNU_DU_BIN_DU | GNU_DU_BIN_GDU)) {
		char err[sizeof(xf_cb) + 6]; *err = '\0';
		if (du_status != 0)
			snprintf(err, sizeof(err), "%s%c%s", xf_cb, DU_ERR_CHAR, NC);

		printf("%s%s%s%s ", err, csize, human_size, cend);

		if (total_size > size_mult_factor)
			printf("/ %s%jd B%s ", csize, (intmax_t)total_size, cend);

		printf("(%s%s)\n", conf.apparent_size == 1 ? _("apparent")
			: _("disk usage"), xargs.si == 1 ? ",si" : "");
	} else {
		printf("%s%s%s\n", csize, human_size, cend);
	}
}

static void
print_dir_items(const char *dir, const int file_perm)
{
	fputs("Items:\t\t", stdout);

	if (file_perm == 0) {
		printf("%s%c%s\n", dn_c, UNKNOWN_CHR, NC);
		return;
	}

	struct dir_info_t info = {0};

#define COUNTING_MSG "Counting..."
	if (term_caps.suggestions == 1) {
		HIDE_CURSOR;
		fputs(dn_c, stdout);
		fputs(COUNTING_MSG, stdout);
	}
	fflush(stdout);

	dir_info(dir, 1, &info);

	if (term_caps.suggestions == 1) {
		MOVE_CURSOR_LEFT((int)sizeof(COUNTING_MSG) - 1);
		ERASE_TO_RIGHT;
		UNHIDE_CURSOR;
		fputs(df_c, stdout);
		fflush(stdout);
	}

	char read_err[5 + (MAX_COLOR * 2)]; *read_err = '\0';
	if (info.status != 0)
		snprintf(read_err, sizeof(read_err), "%s%c%s", xf_cb, DU_ERR_CHAR, df_c);

	printf("%s%s%llu%s (%s%llu%s %s, %s%llu%s %s, %s%llu%s %s)\n",
		read_err, BOLD, info.dirs + info.files + info.links, df_c,
		BOLD, info.dirs, df_c, info.dirs == 1 ? _("directory") : _("directories"),
		BOLD, info.files, df_c, info.files == 1 ? _("file") : _("files"),
		BOLD, info.links, df_c, info.links == 1 ? _("link") : _("links"));
}

#else /* !USE_DU1 */
static void
print_size(const struct stat *attr, const int apparent)
{
	const off_t size =
		(FILE_TYPE_NON_ZERO_SIZE(attr->st_mode) || S_TYPEISSHM(attr)
		|| S_TYPEISTMO(attr))
			? (apparent == 1 ? attr->st_size : attr->st_blocks * S_BLKSIZE)
			: 0;

	char *size_unit = construct_human_size(size);
	char *csize = dz_c;
	char *cend = conf.colorize == 1 ? df_c : "";

	char sf[MAX_SHADE_LEN];
	*sf = '\0';
	if (conf.colorize == 1 && !*dz_c) {
		get_color_size(size, sf, sizeof(sf));
		csize = sf;
	}

	printf("%s%s%s", csize, size_unit ? size_unit : "?", cend);

	const int bigger_than_bytes = size > (xargs.si == 1 ? 1000 : 1024);

	const int is_sparse = (S_ISREG(attr->st_mode) && attr->st_blocks > 0
		&& (intmax_t)(attr->st_blocks * S_BLKSIZE) < (intmax_t)attr->st_size);

	if (bigger_than_bytes == 1)
		printf(" / %s%jd B%s", csize, (intmax_t)size, cend);

	printf(" (%s%s%s)\n", apparent == 1 ? _("apparent")
		: _("on disk"), (xargs.si == 1 && bigger_than_bytes == 1)
		? ",si" : "", is_sparse == 1 ? ",sparse" : "");
}

static void
print_file_size(const struct stat *attr)
{
	fputs(_("Size:\t\t"), stdout);
	print_size(attr, 1); /* Apparent size */
	fputs("\t\t", stdout);
	print_size(attr, 0); /* Physical size */
}

static void
print_dir_size(const off_t dir_size, const int apparent, const char *read_err)
{
	char *cend = conf.colorize == 1 ? df_c : "";
	char *size_color = dz_c;
	char sf[MAX_SHADE_LEN]; *sf = '\0';
	if (conf.colorize == 1 && !*dz_c) {
		get_color_size(dir_size, sf, sizeof(sf));
		size_color = sf;
	}

	char *size = construct_human_size(dir_size);
	if (size) {
		printf("%s%s%s%s ", read_err, size_color, size, cend);

		const int size_mult_factor = xargs.si == 1 ? 1000 : 1024;
		if (dir_size > size_mult_factor)
			printf("/ %s%jd B%s ", size_color, (intmax_t)dir_size, cend);

		printf("(%s%s)\n", apparent == 1 ? _("apparent")
			: _("on disk"), xargs.si == 1 ? ",si" : "");
	} else {
		puts(UNKNOWN_STR);
	}
}

static void
print_dir_info(const char *dir, const int file_perm)
{
	if (file_perm == 0) {
		printf(_("Total size: \t%s%c%s\n"), dn_c, UNKNOWN_CHR, NC);
		printf(_("Items:\t\t%s%c%s\n"), dn_c, UNKNOWN_CHR, NC);
		return;
	}

	struct dir_info_t info = {0};

	fputs(_("Total size:\t"), stdout);

#define SCANNING_MSG "Scanning..."
	if (term_caps.suggestions == 1) {
		HIDE_CURSOR;
		fputs(dn_c, stdout);
		fputs(SCANNING_MSG, stdout);
	}
	fflush(stdout);

	dir_info(dir, 1, &info);

	if (term_caps.suggestions == 1) {
		MOVE_CURSOR_LEFT((int)sizeof(SCANNING_MSG) - 1);
		ERASE_TO_RIGHT;
		UNHIDE_CURSOR;
		fputs(df_c, stdout);
		fflush(stdout);
	}
#undef SCANNING_MSG

	char read_err[5 + (MAX_COLOR * 2)]; *read_err = '\0';
	if (info.status != 0)
		snprintf(read_err, sizeof(read_err), "%s%c%s", xf_cb, DU_ERR_CHAR, df_c);

	print_dir_size(info.size, 1, read_err); /* Apparent */
	fputs("\t\t", stdout);
	print_dir_size(info.blocks * S_BLKSIZE, 0, read_err); /* Physical */

	printf(_("Items:\t\t%s%s%llu%s (%s%llu%s %s, %s%llu%s %s, %s%llu%s %s)\n"),
		read_err, BOLD, info.dirs + info.files + info.links, df_c,
		BOLD, info.dirs, df_c, info.dirs == 1 ? _("directory") : _("directories"),
		BOLD, info.files, df_c, info.files == 1 ? _("file") : _("files"),
		BOLD, info.links, df_c, info.links == 1 ? _("link") : _("links"));
}
#endif /* USE_DU1 */

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
	if (*filename == '.' && filename[1] == '/' && filename[2])
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
#ifndef __CYGWIN__
		check_file_access(attr.st_mode, attr.st_uid, attr.st_gid);
#else
		/* check_file_access() checks Unix-style permissions, which does
		 * not make much sense on Windows filesystems (particularly, on
		 * system directories). */
		1;
#endif /* !__CYGWIN__ */

	char *color = get_file_type_and_color(*link_target ? link_target : filename,
		&attr, &file_type, &ctype);

#if defined(LINUX_FILE_XATTRS)
	const int xattr =
		llistxattr(*link_target ? link_target : filename, NULL, 0) > 0;
#else
	const int xattr = 0;
#endif /* LINUX_FILE_XATTRS */

	print_file_perms(&attr, file_type, ctype, xattr);
	print_filename(filename, color, follow_link, attr.st_mode, link_target);
	print_file_details(filename, &attr, file_type, file_perm, xattr);
	print_file_mime(*link_target ? link_target : filename);
	print_timestamps(*link_target ? link_target : filename, &attr);

#ifdef USE_DU1
	print_file_size(filename, &attr, file_perm, follow_link);
	if (S_ISDIR(attr.st_mode) && follow_link == 1)
		print_dir_items(*link_target ? link_target : filename, file_perm);
#else
	if (S_ISDIR(attr.st_mode)) {
		if (follow_link == 1)
			print_dir_info(*link_target ? link_target : filename, file_perm);
	} else {
		print_file_size(&attr);
	}
#endif /* USE_DU1 */

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
				xerror(_("p: '%s': Cannot unescape filename\n"), args[i]);
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

/* Print properties of the files passed from the command line.
 * If FULL_STAT is set to 1, run with 'pp'. Otherwise use 'p' instead.
 * Used when running with either --stat or --stat-full. */
__attribute__ ((noreturn))
void
do_stat_and_exit(const int full_stat)
{
	/* This function is called very early from main(), immediately after loading
	 * settings, so that we need to set up a few things required to run the
	 * properties function (do_stat). Namely:
	 * 1. du(1) binary (or xdu).
	 * 2. tmp dir (if not xdu).
	 * 3. Current directory. */
#ifdef USE_DU1
# if defined(HAVE_GNU_DU)
	bin_flags |= GNU_DU_BIN_DU;
# elif !defined(_BE_POSIX)
	if (is_cmd_in_path("gdu") == 1)
		bin_flags |= GNU_DU_BIN_GDU;
# endif /* HAVE_GNU_DU */

	if (!tmp_dir)
		tmp_dir = savestring(P_tmpdir, P_tmpdir_len);
#endif /* USE_DU1 */

	fputs(df_c, stdout);
	cur_ws = 0;
	char tmp[PATH_MAX + 1] = "";
	char *cwd = get_cwd(tmp, sizeof(tmp), 0);
	if (cwd)
		workspaces[cur_ws].path = savestring(cwd, strlen(cwd));

	int status = 0;
	int start = -1;
	int i;

	for (i = 0; i < argc_bk; i++) {
		if (argv_bk[i] && strncmp(argv_bk[i], "--stat", 6) == 0
		&& argv_bk[i + 1] && *argv_bk[i + 1]) {
			start = i + 1;
			break;
		}
	}

	if (start <= 0) {
		fprintf(stderr, _("%s: '--stat': Option requires an argument\n"
			"Try '%s --help' for more information.\n"),
			PROGRAM_NAME, PROGRAM_NAME);
		exit(EXIT_FAILURE);
	}

	for (i = start; i < argc_bk; i++) {
		char *norm_path = *argv_bk[i] == '~'
			? tilde_expand(argv_bk[i]) : (char *)NULL;

		 const int ret =
			do_stat(norm_path ? norm_path : argv_bk[i], full_stat);
		if (ret != 0)
			status = ret;

		free(norm_path);
	}

	exit(status);
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
		if (conf.sort != STSIZE) {
			const char *p_largest = construct_human_size(largest);
			l = savestring(p_largest, strlen(p_largest));
		}
	} else {
		t = xnmalloc(MAX_INT_STR, sizeof(char));
		snprintf(t, MAX_INT_STR, "%ju", (uintmax_t)total);
		if (conf.sort != STSIZE) {
			l = xnmalloc(MAX_INT_STR, sizeof(char));
			snprintf(l, MAX_INT_STR, "%ju", (uintmax_t)largest);
		}
	}

	char *tsize = dz_c, *lsize = dz_c;

	char ts[MAX_SHADE_LEN], ls[MAX_SHADE_LEN];
	if (!*dz_c) {
		get_color_size(total, ts, sizeof(ts));
		tsize = ts;
		if (conf.sort != STSIZE) {
			get_color_size(largest, ls, sizeof(ls));
			lsize = ls;
		}
	}

	printf(_("Total size: %s%s%s%s\n"), conf.sort != STSIZE ? "  " : "",
		conf.colorize == 1 ? tsize : "" , t ? t : UNKNOWN_STR,
		conf.colorize == 1 ? tx_c : "");

	if (conf.sort != STSIZE) {
		printf(_("Largest file: %s%s%s %s%s%s%s%s\n"),
			conf.colorize == 1 ? lsize : "" , l ? l : UNKNOWN_STR,
			conf.colorize == 1 ? tx_c : "",
			name ? "[" : "",
			(conf.colorize == 1 && color) ? color : "",
			name ? name : "", tx_c,
			name ? "]" : "");
	}

	free(t);
	free(l);
}
