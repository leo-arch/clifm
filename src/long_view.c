/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* long_view.c -- Construct entries in long view mode */

#include "helpers.h"

#include <string.h> /* memcpy(3), strrchr(3) */
#include <time.h>   /* strftime(3) */

#include "aux.h"    /* xitoa() */
#include "checks.h" /* check_file_access() */
#include "colors.h" /* remove_bold_attr() */
#include "long_view.h" /* macros */
#include "misc.h"   /* gen_diff_str() */
#include "properties.h" /* get_color_age, get_color_size, get_file_perms */

/* Remaining space in the properties string buffer. */
static size_t buf_rem_space = 0;

/* Precomputed colors without the bold attribute for the file type field
 * in the permissions string. */
static char bd_nb[MAX_COLOR];
static char cd_nb[MAX_COLOR];
static char df_nb[MAX_COLOR];
static char di_nb[MAX_COLOR];
static char dn_nb[MAX_COLOR];
static char fi_nb[MAX_COLOR];
static char ln_nb[MAX_COLOR];
#ifdef SOLARIS_DOORS
static char oo_nb[MAX_COLOR];
#endif /* SOLARIS_DOORS */
static char pi_nb[MAX_COLOR];
static char so_nb[MAX_COLOR];

static char *
get_ext_info_long(const struct fileinfo *props, const size_t name_len,
	int *trunc, size_t *ext_len)
{
	/* At this point, TRUNC is set to TRUNC_NO_EXT and EXT_LEN to zero. */
	if (!props->ext_name)
		return NULL;

	if (props->utf8 == 0)
		*ext_len = name_len - (size_t)(props->ext_name - props->name);
	else
		*ext_len = wc_xstrlen(props->ext_name);

	if ((int)*ext_len >= conf.max_name_len || (int)*ext_len <= 0)
		*ext_len = 0;
	else
		*trunc = TRUNC_EXT;

	return props->ext_name;
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
	/* If the filename length is greater than MAX_NAMELEN, truncate it to
	 * MAX_NAMELEN (later a tilde (~) will be appended to let the user know
	 * the filename was truncated). */
	int trunc = 0;

	/* Handle filenames with embedded control characters. */
	size_t plen = props->len;
	char *wname = NULL;
	if (plen == 0) {
		wname = replace_invalid_chars(props->name);
		plen = wc_xstrlen(wname);
	}

	const filesn_t n = (conf.max_files > UNSET
		&& g_files_num > (filesn_t)conf.max_files)
		? (filesn_t)conf.max_files : g_files_num;

	size_t cur_len = (conf.no_eln == 0 ? (size_t)DIGINUM(n) : 0) + 1 + plen
		+ (conf.icons == 1 ? (size_t)ICON_LEN : 0);

	int diff = 0;
	char *name = wname ? wname : props->name;
	const char *ext_name = NULL;
	char truncated_char = 0;
	int trunc_point = 0;

	if (cur_len > (size_t)max_namelen) {
		const int rest = (int)cur_len - max_namelen;
		trunc = TRUNC_NO_EXT;
		size_t ext_len = 0;
		ext_name = get_ext_info_long(props, plen, &trunc, &ext_len);

		trunc_point = (int)plen - rest - 1 - (int)ext_len;
		if (trunc_point <= 0) {
			trunc_point = (int)plen - rest - 1;
			trunc = TRUNC_NO_EXT;
		}

		if (props->utf8 == 1) {
			mbstowcs(g_wcs_name_buf, name, NAME_BUF_SIZE);
			diff = wctruncstr(g_wcs_name_buf, (size_t)trunc_point);
		} else {
			truncated_char = name[trunc_point];
			name[trunc_point] = '\0';
		}

		cur_len -= (size_t)rest;
	}

	/* Calculate pad for each filename */
	int pad = max_namelen - (int)cur_len;
	if (pad < 0)
		pad = 0;

	if (trunc == 0) {
		printf("%s%s%s%s%s%s%s%-*s%s  ",
			(conf.colorize == 1 && conf.icons == 1) ? props->icon_color : "",
			conf.icons == 1 ? props->icon : "", conf.icons == 1 ? " " : "",
			df_c, conf.colorize == 1 ? props->color : "", name,
			conf.light_mode == 1 ? "\x1b[0m" : df_c, pad, "", df_c);

		free(wname);
		return;
	}

	const char *trunc_diff = diff > 0 ? gen_diff_str(diff) : "";
	static char trunc_s[2] = {0};
	trunc_s[0] = TRUNC_FILE_CHR;

	/* I hate using two almost identical pieces of code, but this allows us
	 * to use mbstowcs only for UTF-8 names and printing non-UTF8 directly,
	 * without conversion (which makes the whole thing faster).
	 * NOTE: The only difference between the two printf calls is the filename:
	 * %ls (pointer to wchar_t string) or %s (pointer to char string). Any
	 * modification must be applied to both calls to keep output consistent. */
	if (props->utf8 == 1) {
		printf("%s%s%s%s%s%ls%s%s%-*s%s\x1b[0m%s%s\x1b[0m%s%s%s  ",
			(conf.colorize == 1 && conf.icons == 1) ? props->icon_color : "",
			conf.icons == 1 ? props->icon : "", conf.icons == 1 ? " " : "", df_c,
			conf.colorize == 1 ? props->color : "",
			g_wcs_name_buf, trunc_diff,
			conf.light_mode == 1 ? "\x1b[0m" : df_c, pad, "", df_c,
			trunc > 0 ? tt_c : "", trunc_s,
			trunc == TRUNC_EXT ? props->color : "",
			trunc == TRUNC_EXT ? ext_name : "",
			trunc == TRUNC_EXT ? df_c : "");
	} else {
		printf("%s%s%s%s%s%s%s%s%-*s%s\x1b[0m%s%s\x1b[0m%s%s%s  ",
			(conf.colorize == 1 && conf.icons == 1) ? props->icon_color : "",
			conf.icons == 1 ? props->icon : "", conf.icons == 1 ? " " : "", df_c,
			conf.colorize == 1 ? props->color : "",
			name, trunc_diff,
			conf.light_mode == 1 ? "\x1b[0m" : df_c, pad, "", df_c,
			trunc > 0 ? tt_c : "", trunc_s,
			trunc == TRUNC_EXT ? props->color : "",
			trunc == TRUNC_EXT ? ext_name : "",
			trunc == TRUNC_EXT ? df_c : "");

		/* Reinsert the character removed when truncating the string. */
		name[trunc_point] = truncated_char;
	}

	free(wname);
}

static size_t
gen_size(const struct fileinfo *props, char *size_str,
	const int size_max, const int file_perm)
{
	if (prop_fields.size == 0) {
		*size_str = '\0';
		return 0;
	}

	int bytes = 0;

	if (props->stat_err == 1) {
		bytes = snprintf(size_str, buf_rem_space, "%*s", size_max
			+ (prop_fields.size == PROP_SIZE_HUMAN), UNKNOWN_STR);
		return bytes > 0 ? (size_t)bytes : 0;
	}

	const int no_dir_access =
		(file_perm == 0 && props->dir == 1 && conf.full_dir_size == 1);
	if (S_ISCHR(props->mode) || S_ISBLK(props->mode) || no_dir_access == 1) {
		bytes = snprintf(size_str, buf_rem_space, "%s%*c%s", dn_c, size_max
			+ (prop_fields.size == PROP_SIZE_HUMAN),
			no_dir_access == 1 ? UNKNOWN_CHR : '-', df_c);
		return bytes > 0 ? (size_t)bytes : 0;
	}

	const off_t size = (FILE_TYPE_NON_ZERO_SIZE(props->mode)
		|| props->type == DT_SHM || props->type == DT_TPO)
		? props->size : 0;

	/* Let's construct the color for the current file size */
	const char *csize = dz_c;
	static char sf[MAX_SHADE_LEN];
	if (!*dz_c && conf.colorize == 1) {
		get_color_size(size, sf, sizeof(sf));
		csize = sf;
	}

	if (prop_fields.size != PROP_SIZE_HUMAN) {
		char err_char[2] = {0};
		err_char[0] = props->du_status != 0 ? DU_ERR_CHAR : 0;
		bytes = snprintf(size_str, buf_rem_space, "%s%*jd%s%s", csize,
			(props->du_status != 0 && size_max > 0) ? size_max - 1 : size_max,
			(intmax_t)size, df_c, err_char);
		return bytes > 0 ? (size_t)bytes : 0;
	}

	const int du_err = (props->du_status != 0 && props->dir == 1
		&& conf.full_dir_size == 1);
	const char *unit_color = conf.colorize == 1
		? (du_err == 1 ? xf_cb : dim_c)
		: ((du_err == 1 && xargs.no_bold != 1) ? "\x1b[1m" : "");

	bytes = snprintf(size_str, buf_rem_space, "%s%*s%s%c\x1b[0m%s",
		csize, size_max,
		*props->human_size.str ? props->human_size.str : UNKNOWN_STR,
		unit_color, props->human_size.unit, df_c);

	return bytes > 0 ? (size_t)bytes : 0;
}

static size_t
gen_perms(const mode_t mode, char *perm_str, const char file_type,
	const char *ctype)
{
	int bytes = 0;

	if (prop_fields.perm == PERM_SYMBOLIC) {
		const struct perms_t perms = get_file_perms(mode);
		bytes = snprintf(perm_str, buf_rem_space,
			"%s%c%s/%s%c%s%c%s%c%s.%s%c%s%c%s%c%s.%s%c%s%c%s%c%s",
			ctype, file_type, dn_c,
			perms.cur, perms.ur, perms.cuw, perms.uw, perms.cux, perms.ux, dn_c,
			perms.cgr, perms.gr, perms.cgw, perms.gw, perms.cgx, perms.gx, dn_c,
			perms.cor, perms.or, perms.cow, perms.ow, perms.cox, perms.ox, df_c);

	} else /* PERM_NUMERIC */ {
		bytes = snprintf(perm_str, PERM_STR_LEN,
			"%s%04o%s", do_c, mode & 07777, df_c);
	}

	return bytes > 0 ? (size_t)bytes : 0;
}

static const char *
get_time_char(void)
{
	const char *time_char = NULL;

	if (conf.time_follows_sort == 1) {
		switch (conf.sort) {
		case SATIME: time_char = conf.relative_time == 1 ? "A" : "a"; break;
		case SBTIME: time_char = conf.relative_time == 1 ? "B" : "b"; break;
		case SCTIME: time_char = conf.relative_time == 1 ? "C" : "c"; break;
		case SMTIME: time_char = conf.relative_time == 1 ? "M" : "m"; break;
		default: break;
		}
	}

	if (!time_char) {
		switch (prop_fields.time) {
		case PROP_TIME_ACCESS:
			time_char = conf.relative_time == 1 ? "A" : "a"; break;
		case PROP_TIME_BIRTH:
			time_char = conf.relative_time == 1 ? "B" : "b"; break;
		case PROP_TIME_CHANGE:
			time_char = conf.relative_time == 1 ? "C" : "c"; break;
		case PROP_TIME_MOD:
			time_char = conf.relative_time == 1 ? "M" : "m"; break;
		default: time_char = conf.relative_time == 1 ? "M" : "m"; break;
		}
	}

#ifndef ST_BTIME_LIGHT
	if (conf.light_mode == 1 && (*time_char == 'B' || *time_char == 'b'))
		time_char = conf.relative_time == 1 ? "M" : "m";
#endif /* !ST_BTIME_LIGHT */

	return time_char;
}

/* GCC (not clang) complains about tfmt being not a string literal. Let's
 * silence this warning until we find a better approach. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
static size_t
gen_time(char *time_str, const struct fileinfo *props)
{
	const time_t t = props->ltime;

	/* Let's construct the color for the current timestamp. */
	const char *cdate = dd_c;
	static char df[MAX_SHADE_LEN];
	if (conf.colorize == 1 && !*dd_c) {
		get_color_age(t, df, sizeof(df));
		cdate = df;
	}

	static char file_time[MAX_TIME_STR];
	struct tm tm;

	if (props->stat_err == 1) {
		/* Let' use the same string we use for invalid times, but
		 * replace '-' by '?'. */
		xstrsncpy(file_time, invalid_time_str, sizeof(file_time));
		const int index = conf.relative_time == 1 ? 1 : 0;
		file_time[index] = UNKNOWN_CHR;
		cdate = df_c;
	} else if (t >= 0 && t != (time_t)-1) {
		/* PROPS_NOW (global) is set by list_dir(), in listing.c before
		 * calling print_entry_props(), which calls this function. */
		const time_t age = props_now - t;
		/* AGE is negative if file time is in the future. */

		if (conf.relative_time == 1) {
			calc_relative_time(age < 0 ? -age : age, file_time);
		} else if (localtime_r(&t, &tm)) {
			/* If not user defined, let's mimic ls(1) behavior: a file is
			 * considered recent if it is within the past six months. */
			const int recent = age >= 0 && age < 14515200LL;
			/* 14515200 == 6*4*7*24*60*60 == six months */
			const char *tfmt = conf.time_str ? conf.time_str :
				(recent ? DEF_TIME_STYLE_RECENT : DEF_TIME_STYLE_OLDER);

			strftime(file_time, sizeof(file_time), tfmt, &tm);
		} else {
			xstrsncpy(file_time, invalid_time_str, sizeof(file_time));
		}
	} else {
		/* INVALID_TIME_STR (global) is generated at startup by
		 * check_time_str(), in init.c. */
		xstrsncpy(file_time, invalid_time_str, sizeof(file_time));
	}

	const char *time_char = conf.timestamp_mark == 1 ? get_time_char() : "";
	const int bytes = snprintf(time_str, buf_rem_space, "%s%s%s%s%s", cdate,
		*file_time ? file_time : UNKNOWN_STR, dt_c, time_char, df_c);

	return bytes > 0 ? (size_t)bytes : 0;
}
#pragma GCC diagnostic pop

static size_t
gen_id(const struct fileinfo *props, char *id_str,
	const struct maxes_t *maxes, const int file_perm)
{
	int bytes = 0;
	const char *uid_color =
		(file_perm == 1 && conf.colorize == 1) ? du_c : df_c;

#define USER_NAME props->uid_i.name ? props->uid_i.name \
		: (props->stat_err == 1 ? UNKNOWN_STR : xitoa(props->uid))
#define GROUP_NAME props->gid_i.name ? props->gid_i.name \
		: (props->stat_err == 1 ? UNKNOWN_STR : xitoa(props->gid))

	if (prop_fields.no_group == 1) {
		if (prop_fields.ids == PROP_ID_NUM) {
			if (props->stat_err == 1) {
				bytes = snprintf(id_str, buf_rem_space, "%s%*s%s", uid_color,
					maxes->id_user, UNKNOWN_STR, df_c);
			} else {
				bytes = snprintf(id_str, buf_rem_space, "%s%*u%s", uid_color,
					maxes->id_user, props->uid, df_c);
			}
		} else { /* PROPS_ID_NAME */
			bytes = snprintf(id_str, buf_rem_space, "%s%-*s%s", uid_color,
				maxes->id_user, USER_NAME, df_c);
		}

		return bytes > 0 ? (size_t)bytes : 0;
	}

	const char *gid_color = conf.colorize == 0 ? "" :
		(file_perm == 1 ? dg_c : dim_c);

	if (prop_fields.ids == PROP_ID_NUM) {
		if (props->stat_err == 1) {
			bytes = snprintf(id_str, buf_rem_space, "%s%*c %*c",
				df_c, maxes->id_user, UNKNOWN_CHR,
				maxes->id_group, UNKNOWN_CHR);
		} else {
			bytes = snprintf(id_str, buf_rem_space, "%s%*u %s%*u%s",
				uid_color, maxes->id_user, props->uid,
				gid_color, maxes->id_group, props->gid, df_c);
		}
	} else { /* PROPS_ID_NAME */
		bytes = snprintf(id_str, buf_rem_space, "%s%-*s %s%-*s%s",
			uid_color, maxes->id_user, USER_NAME,
			props->stat_err == 1 ? "" : gid_color,
			maxes->id_group, GROUP_NAME, df_c);
	}
#undef USER_NAME
#undef GROUP_NAME
	return bytes > 0 ? (size_t)bytes : 0;
}

static size_t
gen_filecounter(const struct fileinfo *props, char *fc_str,
	const int max)
{
	int bytes = 0;

	if (props->filesn > 0) {
		bytes = snprintf(fc_str, buf_rem_space, "%s%*jd%s", fc_c, max,
			(intmax_t)props->filesn, df_c);
	} else {
		bytes = snprintf(fc_str, buf_rem_space, "%s%*c%s", dn_c, max,
			props->filesn < 0 ? UNKNOWN_CHR /* Dir with no read permission */
			: (props->dir == 1 ? '0' : '-'), df_c);
	}

	return bytes > 0 ? (size_t)bytes : 0;
}

static size_t
gen_inode(const struct fileinfo *props, char *ino_str, const int max)
{
	int bytes = 0;

	if (props->stat_err == 1) {
		bytes = snprintf(ino_str, buf_rem_space,
			"\x1b[0m%*s%s", max, UNKNOWN_STR, df_c);
	} else {
		bytes = snprintf(ino_str, buf_rem_space, "\x1b[0m%s%*ju%s", de_c,
			max, (uintmax_t)props->inode, df_c);
	}

	return bytes > 0 ? (size_t)bytes : 0;
}

static size_t
gen_links(const struct fileinfo *props, char *links_str, const int max)
{
	int bytes = 0;

	if (props->stat_err == 1) {
		bytes = snprintf(links_str, buf_rem_space, "\x1b[0m%*s%s",
			max, UNKNOWN_STR, df_c);
	} else {
		bytes = snprintf(links_str, buf_rem_space, "\x1b[0m%s%s%*ju%s",
			dk_c, props->linkn > 1 ? BOLD : "", max,
			(uintmax_t)props->linkn, df_c);
	}

	return bytes > 0 ? (size_t)bytes : 0;
}

static size_t
gen_blocks(const struct fileinfo *props, char *blk_str, const int max)
{
	int bytes = 0;

	if (props->stat_err == 1) {
		bytes = snprintf(blk_str, buf_rem_space,
			"\x1b[0m%*s%s", max, UNKNOWN_STR, df_c);
	} else {
		bytes = snprintf(blk_str, buf_rem_space, "\x1b[0m%s%*jd%s",
			db_c, max, (intmax_t)props->blocks, df_c);
	}

	return bytes > 0 ? (size_t)bytes : 0;
}

static void
set_no_bold_colors(void)
{
	xstrsncpy(bd_nb, bd_c, sizeof(bd_nb)); remove_bold_attr(bd_nb);
	xstrsncpy(cd_nb, cd_c, sizeof(cd_nb)); remove_bold_attr(cd_nb);
	xstrsncpy(df_nb, df_c, sizeof(df_nb)); remove_bold_attr(df_nb);
	xstrsncpy(di_nb, di_c, sizeof(di_nb)); remove_bold_attr(di_nb);
	xstrsncpy(dn_nb, dn_c, sizeof(dn_nb)); remove_bold_attr(dn_nb);
	xstrsncpy(fi_nb, fi_c, sizeof(fi_nb)); remove_bold_attr(fi_nb);
	xstrsncpy(ln_nb, ln_c, sizeof(ln_nb)); remove_bold_attr(ln_nb);
#ifdef SOLARIS_DOORS
	xstrsncpy(oo_nb, oo_c, sizeof(oo_nb)); remove_bold_attr(oo_nb);
#endif /* SOLARIS_DOORS */
	xstrsncpy(pi_nb, pi_c, sizeof(pi_nb)); remove_bold_attr(pi_nb);
	xstrsncpy(so_nb, so_c, sizeof(so_nb)); remove_bold_attr(so_nb);
}

static char
set_file_type_and_color(const struct fileinfo *props, char **color)
{
	/* Precompute file type colors without the bold attribute for the
	 * file type field in the permissions string. Let's do this only once,
	 * and each time the color scheme is switched. */
	static const char *cscheme_bk = NULL;
	if (!cscheme_bk || cscheme_bk != cur_cscheme) {
		set_no_bold_colors();
		cscheme_bk = cur_cscheme;
	}

	/* In case we want to keep the symlink identification char (l) when
	 * displaying info about a link target (follow-symlinks).
	 * Note: nnn keeps it, vifm doesn't. I think vifm's approach is better:
	 * if we use the target identification char, the user can easily see
	 * what kind of file the link points to (and this is useful). But if we
	 * keep the symlink char, no only that info is lost, but we are also
	 * providing duplicate data: the user already knows the file is a symlink,
	 * either by the file name color, its icon, or the link character printed
	 * before the name (if color-links-as-target is enabled). */
/*	if (props->symlink == 1) {
		*color = conf.colorize == 1 ? ln_nb : df_nb;
		return LNK_PCHR;
	} */

	char type = 0;

	switch (props->mode & S_IFMT) {
	case S_IFREG:  type = REG_PCHR; *color = dn_nb; break;
	case S_IFDIR:  type = DIR_PCHR; *color = di_nb; break;
	case S_IFLNK:  type = LNK_PCHR; *color = ln_nb; break;
	case S_IFIFO:  type = FIFO_PCHR; *color = pi_nb; break;
	case S_IFSOCK: type = SOCK_PCHR; *color = so_nb; break;
	case S_IFBLK:  type = BLKDEV_PCHR; *color = bd_nb; break;
	case S_IFCHR:  type = CHARDEV_PCHR; *color = cd_nb; break;
#ifndef _BE_POSIX
# ifdef S_ARCH1
	case S_ARCH1:  type = ARCH1_PCHR; *color = fi_nb; break;
	case S_ARCH2:  type = ARCH2_PCHR; *color = fi_nb; break;
# endif /* S_ARCH1 */
# ifdef SOLARIS_DOORS
	case S_IFDOOR: type = DOOR_PCHR; *color = oo_nb; break;
	case S_IFPORT: type = PORT_PCHR; *color = oo_nb; break;
# endif /* SOLARIS_DOORS */
# ifdef S_IFWHT
	case S_IFWHT:  type = WHT_PCHR; *color = fi_nb; break;
# endif /* S_IFWHT */
#endif /* !_BE_POSIX */
	default:       type = UNK_PCHR; *color = dn_nb; break;
	}

	if (conf.colorize == 0)
		*color = df_nb;

	return type;
}

/* Compose the properties line for the current filename.
 * This function is called by list_dir(), in listing.c, for each filename
 * in the current directory when running in long view mode (after
 * printing the corresponding ELN). */
int
print_entry_props(const struct fileinfo *props, const struct maxes_t *maxes,
	const int have_xattr)
{
	static char buf[MAX_PROP_STR + 1]; /* Store generated fields */
	size_t len = 0; /* Bytes written into buf so far. */

	char *ctype = NULL; /* Color for the file type indicator */
	const char file_type = set_file_type_and_color(props, &ctype);
	const int file_perm = conf.light_mode == 1
		? check_file_access(props->mode, props->uid, props->gid)
		: (props->stat_err != 1 && props->user_access != 0);
	const char xattr_char =
		have_xattr == 1 ? (props->xattr == 1 ? XATTR_CHAR : ' ') : 0;
	const size_t prop_fields_gap = (size_t)conf.prop_fields_gap;
	const int file_counter =
		(conf.file_counter != 0 && maxes->file_counter != 0);

	construct_and_print_filename(props, maxes->name);

	/* Let's print fields according to the value of PropFields in the
	 * config file (prop_fields_str). */
	for (size_t i = 0; i < PROP_FIELDS_SIZE && prop_fields_str[i]; i++) {
		if (len >= sizeof(buf) - 1)
			break;

		buf_rem_space = sizeof(buf) - len;

		switch (prop_fields_str[i]) {
		case 'B': len += gen_blocks(props, buf + len, maxes->blocks); break;
		case 'f':
			if (file_counter == 1)
				len += gen_filecounter(props, buf + len, maxes->file_counter);
			break;
		case 'd': len += gen_inode(props, buf + len, maxes->inode); break;
		case 'p': /* fallthrough */
		case 'n':
			len += gen_perms(props->mode, buf + len, file_type, ctype);
			if (xattr_char != 0) buf[len++] = xattr_char;
			break;
		case 'i': /* fallthrough */
		case 'I': len += gen_id(props, buf + len, maxes, file_perm); break;
		case 'l': len += gen_links(props, buf + len, maxes->links); break;
		case 'a': /* fallthrough */
		case 'b': /* fallthrough */
		case 'm': /* fallthrough */
		case 'c': len += gen_time(buf + len, props); break;
		case 's': /* fallthrough */
		case 'S':
			len += gen_size(props, buf + len, maxes->size, file_perm); break;
		default: continue; /* Unknown option character. Skip it. */
		}

		/* If not the last field, add some space to separate the current
		 * field from the next one. */
		const int last_field = prop_fields_str[i + 1] == '\0';
		if (last_field == 1 || (sizeof(buf) - len) <= prop_fields_gap)
			break;

		buf[len++] = ' ';
		if (conf.prop_fields_gap > 1) /* PropFieldsGap is at most 2. */
			buf[len++] = ' ';
	}

	buf[len] = '\0';
	fputs(buf, stdout);
	putchar('\n');

	return FUNC_SUCCESS;
}
