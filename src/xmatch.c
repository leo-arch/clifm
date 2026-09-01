/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* xmatch.c */

#include "helpers.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "aux.h"      /* get_dt() */
#include "colors.h"   /* get_entry_color() */
#ifndef _NO_ICONS
# include "listing.h" /* get_file_icon_and_color() */
#endif
#include "mem.h"      /* xnrealloc() */
#include "misc.h"     /* xerror() */
#include "sort.h"     /* entrycmp() */
#include "xmatch.h"

static int
mt_is_utf8_name(const char *filename, size_t *len, size_t *ext_index)
{
	static const uint8_t utf8_chars[256] = {
		/* 0x00 - 0x1F (Control chars) */
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 0x00 - 0x0F */
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 0x10 - 0x1F */
		/* 0x20 - 0x7E (ASCII chars) */
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x20 - 0x2F */
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x30 - 0x3F */
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x40 - 0x4F */
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x50 - 0x5F */
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x60 - 0x6F */
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,	 /* 0x70 - 0x7E */
		1, /* 0x7F (DEL) */
		/* 0x80 - 0xFF (non-ASCII chars) */
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 0x80-0x8F */
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 0x90-0x9F */
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 0xA0-0xAF */
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 0xB0-0xBF */
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 0xC0-0xCF */
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 0xD0-0xDF */
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 0xE0-0xEF */
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1  /* 0xF0-0xFF */
	};

	const unsigned char *name = (const unsigned char *)filename;
	const unsigned char *start = (const unsigned char *)filename;
	const unsigned char *ext = NULL;

	int utf8 = 0;
	while (*name) {
		if (utf8_chars[*name]) {
			utf8 = 1;
		} else {
			if (*name == '.')
				ext = name;
		}
		name++;
	}

	if (len) *len = (size_t)(name - start);
	if (ext && ext != start && ext[1] && ext_index)
		*ext_index = (size_t)(ext - start);
	return utf8;
}

static struct fileinfo
fill_st_info(const char *fpath, const char *basename, const struct stat *a)
{
	struct fileinfo f = {0};

	if (!fpath || !basename || !a)
		return f;

	const char *name = basename ? basename : fpath;

	size_t ext_index = 0;
	f.utf8 = mt_is_utf8_name(name, &f.bytes, &ext_index);
	f.len = f.utf8 == 1 ? wc_xstrlen(name) : f.bytes;
	f.name = strdup(name);
	f.ext_name = f.name + ext_index;
	f.dir = S_ISDIR(a->st_mode);
	f.symlink = S_ISLNK(a->st_mode);
	f.size = a->st_size;
	f.type = get_dt(a->st_mode);
	f.mode = a->st_mode;
	f.inode = a->st_ino;
	f.linkn = a->st_nlink;
	f.blocks = a->st_blocks;
	f.gid = a->st_gid;
	f.uid = a->st_uid;
	f.rdev = a->st_rdev;

	const char *color = get_entry_color(fpath, a);
	f.color = color ? strdup(color) : NULL;

#ifndef _NO_ICONS
	if (conf.icons == 1) {
		f.icon = (char *)get_file_icon_and_color(fpath, a, f.color,
			(const char **)&f.icon_color);
	}
#endif
	if (conf.file_counter == 1 && f.dir == 1) {
		const filesn_t n = count_dir(fpath, NO_CPOP);
		f.filesn = n > 2 ? n - 2 : 0;
	}

	switch (conf.sort) {
	case SATIME: f.time = a->st_atime; break;
/*	case SBTIME: f.time = birth_time; break; */
	case SCTIME: f.time = a->st_ctime; break;
	case SMTIME: f.time = a->st_mtime; break;
	default: f.time = 0; break;
	}

/*	f.user_access = check_file_access(a->st_mode, a->st_uid, a->st_gid);
	f.exec = (f.color == su_c || f.color == sg_c || f.color == ex_c); */

	return f;
}

static int
xglob_cwd(const char *pattern, xglob_t *out, const int gl_flags,
	const mode_t file_type)
{
	int invert = 0;
	if (*pattern == '!') {
		invert = 1;
		pattern++;
	}

	for (filesn_t i = 0; i < g_files_num; i++) {
		if (file_type != 0 && file_type != file_info[i].type)
			continue;

		const int ret = fnmatch(pattern, file_info[i].name, gl_flags);
		if (ret == 0) {
			if (invert == 1)
				continue;
		} else {
			if (invert == 0)
				continue;
		}

		if (out->gl_matches == out->cap) {
			const size_t newcap = out->cap ? out->cap * 2 : 32;
			out->gl_finfo = xnrealloc(out->gl_finfo, newcap + 1,
				sizeof(struct fileinfo));
			out->cap = newcap;
		}

		out->gl_finfo[out->gl_matches] = file_info[i];
		out->gl_finfo[out->gl_matches++].eln = i + 1;
		out->gl_finfo[out->gl_matches] = (struct fileinfo){0};
	}

	out->free_names = 0;
	if (out->gl_matches == 0)
		return (-1);

	return 0;
}

static void
build_gl_flags(int *gl_flags)
{
	if (*gl_flags == -1) {
		*gl_flags = FNM_PATHNAME|FNM_EXTMATCH;
		if (conf.show_hidden == 0)
			*gl_flags |= FNM_PERIOD;
		if (conf.ignore_case == 1)
			*gl_flags |= FNM_CASEFOLD;
	}
}

int
xglob(const char *dirpath, const char *pattern, xglob_t *out,
	int gl_flags, const mode_t file_type, const int do_stat,
	const int store_basename)
{
	build_gl_flags(&gl_flags);

	if ((!dirpath || (*dirpath == '.' && !dirpath[1])) && conf.autols == 1)
		return xglob_cwd(pattern, out, gl_flags, file_type);

	DIR *dp = opendir(dirpath);
	if (!dp) {
		xerror("%s: '%s': %s\n", PROGRAM_NAME, dirpath, strerror(errno));
		return (-1);
	}

	int invert = 0;
	if (*pattern == '!') {
		invert = 1;
		pattern++;
	}

	struct dirent *ent;
	char fpath[PATH_MAX + 1];
	const size_t dlen = strlen(dirpath);
	const int bytes = snprintf(fpath, sizeof(fpath), "%s%s", dirpath,
		(dlen > 1 && dirpath[dlen - 1] == '/') ? "" : "/");
	const size_t dirpath_len = bytes > 0 ? (size_t)bytes : 0;

	while ((ent = readdir(dp)) != NULL) {
		const char *name = ent->d_name;
		if (SELFORPARENT(name))
			continue;

		if (*name == '.' && conf.show_hidden == 0)
			continue;

		xstrsncpy(fpath + dirpath_len, name, sizeof(fpath) - dirpath_len);
		struct stat a;
		int ret = 0;

		if (do_stat == 0) {
#ifndef _DIRENT_HAVE_D_TYPE
			if (file_type != 0
			&& (lstat(fpath, &a) == -1 || file_type != get_dt(a.st_mode)))
#else
			if (file_type != 0 && file_type != ent->d_type)
#endif
				continue;
		} else {
			ret = lstat(fpath, &a);
			if (file_type != 0
			&& (ret == -1 || file_type != get_dt(a.st_mode)))
				continue;
		}

		ret = fnmatch(pattern, name, gl_flags);
		if (ret == 0) {
			if (invert == 1)
				continue;
		} else {
			if (invert == 0)
				continue;
		}

		if (out->gl_matches == out->cap) {
			const size_t newcap = out->cap ? out->cap * 2 : 32;
			out->gl_finfo = xnrealloc(out->gl_finfo, newcap + 1,
				sizeof(struct fileinfo));
			out->cap = newcap;
		}

		const char *basename = store_basename ? name : NULL;
		if (do_stat == 1) {
			out->gl_finfo[out->gl_matches++] = fill_st_info(fpath, basename, &a);
		} else {
			out->gl_finfo[out->gl_matches] = (struct fileinfo){0};
			out->gl_finfo[out->gl_matches++].name =
				strdup(basename ? basename : fpath);
		}
		out->gl_finfo[out->gl_matches] = (struct fileinfo){0};
	}

	closedir(dp);
	out->free_names = 1;
	if (out->gl_matches == 0)
		return (-1);

	qsort(out->gl_finfo, out->gl_matches, sizeof(out->gl_finfo[0]), entrycmp);

	return 0;
}

void
xglobfree(xglob_t *g)
{
	if (!g)
		return;

	if (g->free_names == 1) {
		for (size_t i = 0; i < g->gl_matches; i++) {
			free(g->gl_finfo[i].name);
			free(g->gl_finfo[i].color);
		}
	}
	free(g->gl_finfo);
	g = NULL;
}

static int
xregex_cwd(const char *pattern, xregex_t *out, const int re_flags,
	const mode_t file_type)
{
	int invert = 0;
	if (*pattern == '!') {
		invert = 1;
		pattern++;
	}

	regex_t regex;
	if (regcomp(&regex, pattern, re_flags) != 0) {
		xerror(_("%s: %s: Invalid regular expression\n"), PROGRAM_NAME, pattern);
		regfree(&regex);
		return (-1);
	}

	for (filesn_t i = 0; i < g_files_num; i++) {
		if (file_type != 0 && file_type != file_info[i].type)
			continue;

		const int ret = regexec(&regex, file_info[i].name, 0, NULL, 0);
		if (ret == 0) {
			if (invert == 1)
				continue;
		} else {
			if (invert == 0)
				continue;
		}

		if (out->re_matches == out->cap) {
			const size_t newcap = out->cap ? out->cap * 2 : 32;
			out->re_finfo = xnrealloc(out->re_finfo, newcap + 1,
				sizeof(struct fileinfo));
			out->cap = newcap;
		}

		out->re_finfo[out->re_matches] = file_info[i];
		out->re_finfo[out->re_matches++].eln = i + 1;
		out->re_finfo[out->re_matches] = (struct fileinfo){0};
	}

	out->free_names = 0;
	regfree(&regex);
	if (out->re_matches == 0)
		return (-1);

	return 0;
}

static void
build_re_flags(int *re_flags)
{
	if (*re_flags == -1) {
		*re_flags = conf.ignore_case == 0 ? (REG_NOSUB | REG_EXTENDED)
			: (REG_NOSUB | REG_EXTENDED | REG_ICASE);
	}
}

int
xregex(const char *dirpath, const char *pattern, xregex_t *out,
	int re_flags, const mode_t file_type, const int do_stat,
	const int store_basename)
{
	build_re_flags(&re_flags);

	if ((!dirpath || (*dirpath == '.' && !dirpath[1])) && conf.autols == 1)
		return xregex_cwd(pattern, out, re_flags, file_type);

	DIR *dp = opendir(dirpath);
	if (!dp) {
		xerror("%s: '%s': %s\n", PROGRAM_NAME, dirpath, strerror(errno));
		return (-1);
	}

	int invert = 0;
	if (*pattern == '!') {
		invert = 1;
		pattern++;
	}

	regex_t regex;
	if (regcomp(&regex, pattern, re_flags) != 0) {
		xerror(_("%s: %s: Invalid regular expression\n"), PROGRAM_NAME, pattern);
		regfree(&regex);
		return (-1);
	}

	struct dirent *ent;
	char fpath[PATH_MAX + 1];
	const size_t dlen = strlen(dirpath);
	const int bytes = snprintf(fpath, sizeof(fpath), "%s%s", dirpath,
		(dlen > 1 && dirpath[dlen - 1] == '/') ? "" : "/");
	const size_t dirpath_len = bytes > 0 ? (size_t)bytes : 0;

	while ((ent = readdir(dp)) != NULL) {
		const char *name = ent->d_name;
		if (SELFORPARENT(name))
			continue;

		if (*name == '.' && conf.show_hidden == 0)
			continue;

		xstrsncpy(fpath + dirpath_len, name, sizeof(fpath) - dirpath_len);
		struct stat a;
		int ret = 0;

		if (do_stat == 0) {
#ifndef _DIRENT_HAVE_D_TYPE
			if (file_type != 0
			&& (lstat(fpath, &a) == -1 || file_type != get_dt(a.st_mode)))
#else
			if (file_type != 0 && file_type != ent->d_type)
#endif
				continue;
		} else {
			ret = lstat(fpath, &a);
			if (file_type != 0
			&& (ret == -1 || file_type != get_dt(a.st_mode)))
				continue;
		}

		ret = regexec(&regex, name, 0, NULL, 0);
		if (ret == 0) {
			if (invert == 1)
				continue;
		} else {
			if (invert == 0)
				continue;
		}

		if (out->re_matches == out->cap) {
			const size_t newcap = out->cap ? out->cap * 2 : 32;
			out->re_finfo = xnrealloc(out->re_finfo, newcap + 1,
				sizeof(struct fileinfo));
			out->cap = newcap;
		}

		const char *basename = store_basename ? name : NULL;
		if (do_stat == 1) {
			out->re_finfo[out->re_matches++] = fill_st_info(fpath, basename, &a);
		} else {
			out->re_finfo[out->re_matches] = (struct fileinfo){0};
			out->re_finfo[out->re_matches++].name =
				strdup(basename ? basename : fpath);
		}
		out->re_finfo[out->re_matches] = (struct fileinfo){0};
	}

	closedir(dp);
	regfree(&regex);
	out->free_names = 1;
	if (out->re_matches == 0)
		return (-1);

	qsort(out->re_finfo, out->re_matches, sizeof(out->re_finfo[0]), entrycmp);

	return 0;
}

void
xregfree(xregex_t *r)
{
	if (!r)
		return;

	if (r->free_names == 1) {
		for (size_t i = 0; i < r->re_matches; i++) {
			free(r->re_finfo[i].name);
			free(r->re_finfo[i].color);
		}
	}
	free(r->re_finfo);
	r = NULL;
}
