/* listing.c -- functions controlling what is listed on the screen */

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

#include "helpers.h"

#include <stdio.h>
#include <sys/statvfs.h>
#include <unistd.h> /* open(2), readlinkat(2) */
#include <errno.h>
#include <string.h>
#if defined(__OpenBSD__)
# include <strings.h>
# include <inttypes.h> /* uintmax_t */
#endif /* __OpenBSD__ */

#if defined(LINUX_FILE_CAPS)
# include <sys/capability.h>
#endif /* LINUX_FILE_CAPS */

#if defined(LINUX_FILE_XATTRS)
# include <sys/xattr.h>
#endif /* LINUX_FILE_XATTRS */

#if defined(LINUX_FSINFO) || defined(HAVE_STATFS)
# include "fsinfo.h"
#endif /* LINUX_FSINFO || HAVE_STATFS */

#if defined(LIST_SPEED_TEST)
# include <time.h>
#endif /* LIST_SPEED_TEST */

#if defined(LINUX_FSINFO)
# include <sys/sysmacros.h> /* major() macro */
#endif /* LINUX_FSINFO */

#if defined(TOURBIN_QSORT)
# include "qsort.h"
# define ENTLESS(i, j) (entrycmp(file_info + (i), file_info + (j)) < 0)
# define ENTSWAP(i, j) (swap_ent((i), (j)))
# define ENTSORT(file_info, n, entrycmp) QSORT((n), ENTLESS, ENTSWAP)
#else
# define ENTSORT(file_info, n, entrycmp) qsort((file_info), (n), sizeof(*(file_info)), (entrycmp))
#endif /* TOURBIN_QSORT */

/* Check for temporary files:
 * 1. "*~"   Gral purpose temp files (mostly used by text editors)
 * 2. "#*#"  Emacs auto-save temp files
 * 3. ".~*#" Libreoffice lock files
 * 4. "~$*"  Office temp files
 * NOTE: MC seems to take only "*~" and "#*" as temp files. */
#define IS_TEMP_FILE(n, l) ( (l) > 0               \
	&& ((n)[(l) - 1] == '~'                        \
	|| ((*(n) == '#' || (*(n) == '.'               \
		&& (n)[1] == '~')) && (n)[(l) - 1] == '#') \
	|| (*(n) == '~' && (n)[1] == '$') ) )

#define IS_EXEC(s) (((s)->st_mode & S_IXUSR)               \
	|| ((s)->st_mode & S_IXGRP) || ((s)->st_mode & S_IXOTH))

#include "autocmds.h"
#include "aux.h"
#include "checks.h"
#include "colors.h"
#include "dothidden.h" /* load_dothidden, check_dothidden, free_dothidden */
#ifndef _NO_ICONS
# include "icons.h"
#endif /* !_NO_ICONS */
#include "messages.h"
#include "misc.h"
#include "properties.h" /* print_analysis_stats() */
#include "long_view.h"  /* print_entry_props() */
#include "sanitize.h"
#include "sort.h"
#include "spawn.h"
#include "xdu.h"        /* dir_size() */

/* In case we want to try some faster printf implementation */
/*#if defined(_PALAND_PRINTF)
# include "printf.h"
# define xprintf printf_
#else
# define xprintf printf
#endif // _PALAND_PRINTF */
#define xprintf printf

/* Macros for run_dir_cmd function */
#define AUTOCMD_DIR_IN  0
#define AUTOCMD_DIR_OUT 1
#define AUTOCMD_DIR_IN_FILE  ".cfm.in"
#define AUTOCMD_DIR_OUT_FILE ".cfm.out"

#define ENTRY_N 64

/* Information about the longest file name in the curent list of files. */
struct longest_t {
	size_t fc_len;   /* Length of the files counter (if a directory). */
	size_t name_len; /* Length of the longest name. */
};

static struct longest_t longest;

struct checks_t {
	int autocmd_files;
	int birthtime;
	int classify;
	int files_counter;
	int filter_name;
	int filter_type;
	int icons_use_file_color;
	int id_names;
	int lnk_char;
	int min_name_trim;
	int scanning;
	int time_follows_sort;
	int xattr;
};

static struct checks_t checks;

static int pager_bk = 0;
static int dir_out = 0;
static int pager_quit = 0;
static int pager_help = 0;
static int long_view_bk = UNSET;

/* Struct to store information about trimmed file names. */
struct wtrim_t {
	char *wname; /* Address to store file name with replaced control chars */
	int type; /* Truncation type: with or without file extension. */
	int diff; /* */
};

/* A version of the loop-unswitching optimization: move loop-invariant
 * conditions out of the loop to reduce the amount of conditions in each
 * loop pass.
 * In this case, instead of checking multiple loop-invariant conditions
 * in each pass of the main files listing loop (in list_dir()), we check
 * only one (i.e. the appropriate member of the checks struct).
 * The most important here is checks.lnk_char: instead of checking 4
 * conditions per listed file, we check only one. Thus, if we have 100 files,
 * this saves 300 condition checks! */
static void
init_checks_struct(void)
{
	checks.autocmd_files = (conf.read_autocmd_files == 1 && dir_changed == 1);
	checks.birthtime = (conf.sort == SBTIME || (conf.long_view == 1
		&& prop_fields.time == PROP_TIME_BIRTH));
	checks.classify = (conf.long_view == 0 && conf.classify == 1);

	checks.files_counter = (conf.files_counter == 1
		&& ((conf.long_view == 1 && prop_fields.counter == 1)
		|| (conf.long_view == 0 && conf.classify == 1)));

	checks.filter_name = (filter.str && filter.type == FILTER_FILE_NAME);
	checks.filter_type = (filter.str && filter.type == FILTER_FILE_TYPE);

	checks.icons_use_file_color =
#ifndef _NO_ICONS
		(xargs.icons_use_file_color == 1 && conf.icons == 1);
#else
		0;
#endif /* !_NO_ICONS */

	checks.id_names = (prop_fields.ids == PROP_ID_NAME
		&& (conf.long_view == 1 || conf.sort == SOWN || conf.sort == SGRP));
	checks.lnk_char = (conf.color_lnk_as_target == 1 && conf.follow_symlinks == 1
		&& conf.icons == 0 && conf.light_mode == 0);
	checks.min_name_trim = (conf.long_view == 1 && conf.max_name_len != UNSET
		&& conf.min_name_trim > conf.max_name_len);
	checks.scanning = (xargs.disk_usage_analyzer == 1
		|| (conf.long_view == 1 && conf.full_dir_size == 1));
	checks.time_follows_sort = (conf.time_follows_sort == 1
		&& conf.sort >= SATIME && conf.sort <= SMTIME);
	checks.xattr = (conf.long_view == 1 && prop_fields.xattr == 1);
}

#if !defined(_NO_ICONS)
static void
set_icon_names_hashes(void)
{
	int i = (int)(sizeof(icon_filenames) / sizeof(struct icons_t));
	name_icons_hashes = xnmalloc((size_t)i + 1, sizeof(size_t));

	while (--i >= 0)
		name_icons_hashes[i] = hashme(icon_filenames[i].name, 0);
}

static void
set_dir_names_hashes(void)
{
	int i = (int)(sizeof(icon_dirnames) / sizeof(struct icons_t));
	dir_icons_hashes = xnmalloc((size_t)i + 1, sizeof(size_t));

	while (--i >= 0)
		dir_icons_hashes[i] = hashme(icon_dirnames[i].name, 0);
}

static void
set_ext_names_hashes(void)
{
	int i = (int)(sizeof(icon_ext) / sizeof(struct icons_t));
	ext_icons_hashes = xnmalloc((size_t)i + 1,  sizeof(size_t));

	while (--i >= 0)
		ext_icons_hashes[i] = hashme(icon_ext[i].name, 0);
}

void
init_icons_hashes(void)
{
	set_icon_names_hashes();
	set_dir_names_hashes();
	set_ext_names_hashes();
}
#endif /* !_NO_ICONS */

#if defined(TOURBIN_QSORT)
static inline void
swap_ent(const size_t id1, const size_t id2)
{
	struct fileinfo _dent, *pdent1 = &file_info[id1], *pdent2 =  &file_info[id2];

	*(&_dent) = *pdent1;
	*pdent1 = *pdent2;
	*pdent2 = *(&_dent);
}
#endif /* TOURBIN_QSORT */

/* Return 1 if NAME contains at least one UTF8/control character, or 0
 * otherwise. BYTES is updated to the number of bytes needed to read the
 * entire name (excluding the terminating NUL char).
 *
 * This check is performed over file names to be listed. If the file name is
 * not UTF8, we get its visible length from BYTES, instead of running
 * wc_xstrlen(). This gives us a little performance improvement: 3% faster
 * over 100,000 files. */
static uint8_t
is_utf8_name(const char *name, size_t *bytes)
{
	uint8_t is_utf8 = 0;
	const char *start = name;

	while (*name) {
#if !defined(CHAR_MIN) || CHAR_MIN >= 0 /* char is unsigned (PowerPC/ARM) */
		if (*name >= 0xC0 || *name < ' ' || *name == 127)
#else /* char is signed (X86) */
		/* If UTF-8 char, the first byte is >= 0xC0, whose decimal
		 * value is 192, which is bigger than CHAR_MAX if char is signed,
		 * becoming thus a negative value. In this way, the above three-steps
		 * check can be written using only two checks. */
		if (*name < ' ' || *name == 127)
#endif /* CHAR_MIN >= 0 */
			is_utf8 = 1;

		name++;
	}

	*bytes = (size_t)(name - start);
	return is_utf8;
}

/* Return the amount of ASCII/UTF-8 characters in the string S. */
static size_t
count_utf8_chars(const char *s)
{
	size_t count = 0;

	for (; *s; s++)
		count += ((*s & 0xc0) != 0x80);

	return count;
}

/* Set the color of the dividing line: DL, is the color code is set,
 * or the color of the current workspace if not */
static void
set_div_line_color(void)
{
	if (*dl_c) {
		fputs(dl_c, stdout);
		return;
	}

	const char *def_color = term_caps.color >= 256 ? DEF_DL_C256 : DEF_DL_C;

	/* If div line color isn't set, use the current workspace color. */
	switch (cur_ws) {
	case 0: fputs(*ws1_c ? ws1_c : def_color, stdout); break;
	case 1: fputs(*ws2_c ? ws2_c : def_color, stdout); break;
	case 2: fputs(*ws3_c ? ws3_c : def_color, stdout); break;
	case 3: fputs(*ws4_c ? ws4_c : def_color, stdout); break;
	case 4: fputs(*ws5_c ? ws5_c : def_color, stdout); break;
	case 5: fputs(*ws6_c ? ws6_c : def_color, stdout); break;
	case 6: fputs(*ws7_c ? ws7_c : def_color, stdout); break;
	case 7: fputs(*ws8_c ? ws8_c : def_color, stdout); break;
	default: fputs(def_color, stdout); break;
	}
}

/* Print the line dividing files and prompt using DIV_LINE.
 * If DIV_LINE is unset, draw a line using box-drawing characters.
 * If it contains exactly one character, print DIV_LINE up to the
 * right screen edge.
 * If it contains more than one character, print exactly the
 * content of DIV_LINE.
 * If it is "0", print an empty line. */
static void
print_div_line(void)
{
#ifdef RUN_CMD
	if (cmd_line_cmd)
		return;
#endif /* RUN_CMD */

	if (conf.colorize == 1)
		set_div_line_color();

	if (!*div_line) {
		fputs("\x1b(0m", stdout);
		int i;
		for (i = 0; i < (int)term_cols - 2; i++)
			putchar('q');
		fputs("\x1b(0j\x1b(B", stdout);
		putchar('\n');
	} else if (*div_line == '0' && !div_line[1]) {
		putchar('\n');
	} else {
		const size_t c = count_utf8_chars(div_line);
		if (c > 1) {
			puts(div_line);
		} else {
			/* Extend DIV_LINE to the end of the screen - 1.
			 * We substract 1 to prevent an extra empty line after the
			 * dividing line on some terminals (e.g. cons25). */
			const size_t len = !div_line[1] ? 1 : wc_xstrlen(div_line);
			int i = c > 0 ? (int)(term_cols / len) : 0;
			for (; i > 1; i--)
				fputs(div_line, stdout);
			putchar('\n');
		}
	}

	fputs(df_c, stdout);
	fflush(stdout);
}

#ifdef LINUX_FSINFO
static char *
get_devname(const char *file)
{
	struct stat b;
	if (stat(file, &b) == -1)
		return DEV_NO_NAME;

	if (major(b.st_dev) == 0)
		return get_dev_name_mntent(file);

	return get_dev_name(b.st_dev);
}
#endif /* LINUX_FSINFO */

/* Print free/total space for the file system to which the current directory
 * belongs, plus device name and file system type name if available. */
static void
print_disk_usage(void)
{
	if (!workspaces || !workspaces[cur_ws].path || !*workspaces[cur_ws].path)
		return;

	struct statvfs a;
	if (statvfs(workspaces[cur_ws].path, &a) != FUNC_SUCCESS) {
		err('w', PRINT_PROMPT, "statvfs: %s\n", strerror(errno));
		return;
	}

	const off_t free_s = (off_t)a.f_bavail * (off_t)a.f_frsize;
	const off_t total = (off_t)a.f_blocks * (off_t)a.f_frsize;
/*	if (total == 0) return; // This is what MC does */

	char *p_free_space = construct_human_size(free_s);
	char *free_space = savestring(p_free_space, strlen(p_free_space));
	char *size = construct_human_size(total);

	const int free_percentage = (int)((free_s * 100) / (total > 0 ? total : 1));

	char *devname = (char *)NULL;
	char *fstype = (char *)NULL;

#ifdef _BE_POSIX
	fstype = DEV_NO_NAME;
	devname = DEV_NO_NAME;
#elif defined(__NetBSD__)
	fstype = a.f_fstypename;
	devname = a.f_mntfromname;
#elif defined(__sun)
	fstype = a.f_basetype;
	devname = DEV_NO_NAME;
#elif defined(LINUX_FSINFO)
	int remote = 0;
	fstype = get_fs_type_name(workspaces[cur_ws].path, &remote);
	devname = get_devname(workspaces[cur_ws].path);
#elif defined(HAVE_STATFS)
	get_dev_info(workspaces[cur_ws].path, &devname, &fstype);
#else
	fstype = DEV_NO_NAME;
	devname = DEV_NO_NAME;
#endif /* _BE_POSIX */

	print_reload_msg(_("%s/%s (%d%% free) %s %s\n"),
		free_space ? free_space : "?", size ? size : "?", free_percentage,
		fstype, devname);

	free(free_space);
}

static void
print_sel_files(const unsigned short t_rows)
{
	int limit = conf.max_printselfiles;

	if (conf.max_printselfiles == 0) {
		/* Never take more than half terminal height */
		limit = (t_rows / 2) - 4;
		/* 4 = 2 div lines, 2 prompt lines */
		if (limit <= 0)
			limit = 1;
	}

	if (limit > (int)sel_n)
		limit = (int)sel_n;

	int i;
	for (i = 0; i < (conf.max_printselfiles != UNSET ? limit : (int)sel_n)
	&& sel_elements[i].name; i++) {
		char *p = abbreviate_file_name(sel_elements[i].name);
		if (!p)
			continue;
		colors_list(p, 0, NO_PAD, PRINT_NEWLINE);
		if (p != sel_elements[i].name)
			free(p);
	}

	if (conf.max_printselfiles != UNSET && limit < (int)sel_n)
		printf("... (%d/%zu)\n", i, sel_n);

	print_div_line();
}

static void
print_dirhist_map(void)
{
	size_t i;
	for (i = 0; i < (size_t)dirhist_total_index; i++) {
		if (i != (size_t)dirhist_cur_index)
			continue;

		if (i > 0 && old_pwd[i - 1])
			printf("%zu %s\n", i, old_pwd[i - 1]);

		printf("%zu %s%s%s\n", i + 1, mi_c, old_pwd[i], df_c);

		if (i + 1 < (size_t)dirhist_total_index && old_pwd[i + 1])
			printf("%zu %s\n", i + 2, old_pwd[i + 1]);

		break;
	}
}

#ifndef _NO_ICONS
/* Set the icon field to the corresponding icon for the file file_info[N].name */
static int
get_name_icon(const filesn_t n)
{
	if (!file_info[n].name)
		return 0;

	const size_t nhash = hashme(file_info[n].name, 0);

	/* This division will be replaced by a constant integer at compile
	 * time, so that it won't even be executed at runtime. */
	int i = (int)(sizeof(icon_filenames) / sizeof(struct icons_t));
	while (--i >= 0) {
		if (nhash != name_icons_hashes[i])
			continue;
		file_info[n].icon = icon_filenames[i].icon;
		file_info[n].icon_color = icon_filenames[i].color;
		return 1;
	}

	return 0;
}

/* Set the icon field to the corresponding icon for the directory
 * file_info[N].name. If not found, set the default icon. */
static void
get_dir_icon(const filesn_t n)
{
	/* Default values for directories */
	file_info[n].icon = DEF_DIR_ICON;
	file_info[n].icon_color = DEF_DIR_ICON_COLOR;

	if (!file_info[n].name)
		return;

	const size_t dhash = hashme(file_info[n].name, 0);

	int i = (int)(sizeof(icon_dirnames) / sizeof(struct icons_t));
	while (--i >= 0) {
		if (dhash != dir_icons_hashes[i])
			continue;
		file_info[n].icon = icon_dirnames[i].icon;
		file_info[n].icon_color = icon_dirnames[i].color;
		break;
	}
}

/* Set the icon field to the corresponding icon for EXT. If not found,
 * set the default icon. */
static void
get_ext_icon(const char *restrict ext, const filesn_t n)
{
	if (!file_info[n].icon) {
		file_info[n].icon = DEF_FILE_ICON;
		file_info[n].icon_color = DEF_FILE_ICON_COLOR;
	}

	if (!ext)
		return;

	ext++;

	const size_t ehash = hashme(ext, 0);

	int i = (int)(sizeof(icon_ext) / sizeof(struct icons_t));
	while (--i >= 0) {
		if (ehash != ext_icons_hashes[i])
			continue;

		file_info[n].icon = icon_ext[i].icon;
		file_info[n].icon_color = icon_ext[i].color;
		break;
	}
}
#endif /* _NO_ICONS */

static void
print_cdpath(void)
{
	if (workspaces && workspaces[cur_ws].path
	&& *workspaces[cur_ws].path)
		print_reload_msg("cdpath: %s\n", workspaces[cur_ws].path);

	is_cdpath = 0;
}

/* Restore the origianl value of long-view (taken from LONG_VIEW_BK)
 * after switching mode for the pager (according to PagerView). */
static void
restore_pager_view(void)
{
	if (long_view_bk != UNSET) {
		conf.long_view = long_view_bk;
		long_view_bk = UNSET;
	}
}

/* If running the pager, set long-view according to the value of PagerView in
 * the config file. the original value of long-view will be restored after
 * list_dir() by restore_pager_view() according to the value of LONG_VIEW_BK. */
static void
set_pager_view(void)
{
	if (conf.pager == 1 && conf.pager_view != PAGER_AUTO) {
		long_view_bk = conf.long_view;
		conf.long_view = conf.pager_view == PAGER_LONG;
	}
}

static void
print_dir_cmds(void)
{
	if (!history || dir_cmds.first_cmd_in_dir > (int)current_hist_n)
		return;

	int i = dir_cmds.first_cmd_in_dir - (dir_cmds.first_cmd_in_dir > 0 ? 1 : 0);
	for (; history[i].cmd; i++)
		printf("%s>%s %s\n", bk_c, df_c, history[i].cmd);
}

static int
post_listing(DIR *dir, const int reset_pager, const filesn_t excluded_files)
{
	restore_pager_view();

	if (dir && closedir(dir) == -1)
		return FUNC_FAILURE;

/* Let plugins and external programs running in clifm know whether
 * we have changed the current directory (last command) or not. */
//	setenv("CLIFM_CHPWD", dir_changed == 1 ? "1" : "0", 1);

	if (dir_changed == 1)
		dir_cmds.first_cmd_in_dir = UNSET;

	dir_changed = 0;

	if (xargs.list_and_quit == 1)
		exit(exit_code);

	if (conf.pager_once == 0) {
		if (reset_pager == 1 && (conf.pager < 2 || files < (filesn_t)conf.pager))
			conf.pager = pager_bk;
	} else {
		conf.pager_once = 0;
		conf.pager = 0;
	}

	if (pager_quit == 0 && conf.max_files != UNSET
	&& files > (filesn_t)conf.max_files)
		printf("... (%d/%jd)\n", conf.max_files, (intmax_t)files);

	print_div_line();

	if (conf.dirhist_map == 1) { /* Print current, previous, and next entries */
		print_dirhist_map();
		print_div_line();
	}

	if (conf.disk_usage == 1)
		print_disk_usage();

	if (is_cdpath == 1)
		print_cdpath();

	if (sort_switch == 1) {
		print_reload_msg(_("Sorted by "));
		print_sort_method();
	}

	if (switch_cscheme == 1)
		printf(_("Color scheme %s->%s %s\n"), mi_c, df_c, cur_cscheme);

	if (conf.print_selfiles == 1 && sel_n > 0)
		print_sel_files(term_lines);

	if (virtual_dir == 1)
		print_reload_msg(_("Virtual directory\n"));

	if (excluded_files > 0)
		print_reload_msg(_("Showing %jd/%jd files\n"),
			(intmax_t)files, (intmax_t)(files + excluded_files));

	if (conf.print_dir_cmds == 1 && dir_cmds.first_cmd_in_dir != UNSET)
		print_dir_cmds();

	return FUNC_SUCCESS;
}

/* A basic pager for directories containing large amount of files.
 * What's missing? It only goes downwards. To go backwards, use the
 * terminal scrollback function */
static int
run_pager(const int columns_n, int *reset_pager, filesn_t *i, size_t *counter)
{
	fputs(PAGER_LABEL, stdout);

	switch (xgetchar()) {
	/* Advance one line at a time */
	case 66: /* fallthrough */ /* Down arrow */
	case 10: /* fallthrough */ /* Enter (LF) */
	case 13: /* fallthrough */ /* Enter (CR) */
	case ' ':
		break;

	/* Advance one page at a time */
	case 126: /* Page Down */
		*counter = 0;
		break;

	/* h: Print pager help */
	case '?': /* fallthrough */
	case 'h': {
		CLEAR;

		fputs(_(PAGER_HELP), stdout);
		int l = (int)term_lines - 6;
		MOVE_CURSOR_DOWN(l);
		fputs(PAGER_LABEL, stdout);

		xgetchar();
		CLEAR;

		pager_help = conf.long_view == 0;

		if (columns_n == -1) { /* Long view */
			*i = 0;
		} else { /* Normal view */
			if (conf.listing_mode == HORLIST)
				*i = 0;
			else
				return (-2);
		}

		*counter = 0;
		if (*i < 0)
			*i = 0;
	} break;

	/* Stop paging (and set a flag to reenable the pager later) */
	case 'c': /* fallthrough */
	case 'p': /* fallthrough */
	case 'Q':
		if (conf.pager_view == PAGER_AUTO
		|| (conf.pager_view == PAGER_SHORT && long_view_bk == 0)
		|| (conf.pager_view == PAGER_LONG && long_view_bk == 1)) {
			pager_bk = conf.pager; conf.pager = 0; *reset_pager = 1;
			break;
		}
		/* fallthrough */

	case 'q':
		pager_bk = conf.pager; conf.pager = 0; *reset_pager = 1;
		putchar('\r');
		ERASE_TO_RIGHT;
		if (conf.long_view == 0 && conf.columned == 1
		&& conf.max_name_len != UNSET)
			MOVE_CURSOR_UP(1);
		return (-3);

	/* If another key is pressed, go back one position.
	 * Otherwise, some file names won't be listed.*/
	default:
		putchar('\r');
		ERASE_TO_RIGHT;
		return (-1);
	}

	putchar('\r');
	ERASE_TO_RIGHT;
	return 0;
}

static void
set_events_checker(void)
{
	if (xargs.list_and_quit == 1)
		return;

#if defined(LINUX_INOTIFY)
	reset_inotify();

#elif defined(BSD_KQUEUE)
	if (event_fd >= 0) {
		close(event_fd);
		event_fd = -1;
		watch = 0;
	}

# if defined(O_EVTONLY)
	event_fd = open(workspaces[cur_ws].path, O_EVTONLY);
# else
	event_fd = open(workspaces[cur_ws].path, O_RDONLY);
# endif /* O_EVTONLY */
	if (event_fd >= 0) {
		/* Prepare for events */
		EV_SET(&events_to_monitor[0], (uintptr_t)event_fd, EVFILT_VNODE,
			EV_ADD | EV_CLEAR, KQUEUE_FFLAGS, 0, workspaces[cur_ws].path);
		watch = 1;
		/* Register events */
		kevent(kq, events_to_monitor, NUM_EVENT_SLOTS, NULL, 0, NULL);
	}

#elif defined(GENERIC_FS_MONITOR)
	struct stat a;
	if (stat(workspaces[cur_ws].path, &a) != -1)
		curdir_mtime = a.st_mtime;
	else
		curdir_mtime = 0;
#endif /* LINUX_INOTIFY */
}

static void
get_longest_filename(const filesn_t n, const size_t pad)
{
	const filesn_t c_max_files = (filesn_t)conf.max_files;
	filesn_t i = (conf.max_files != UNSET && c_max_files < n) ? c_max_files : n;
	filesn_t longest_index = -1;

	/* Cast only once here instead of hundreds of times in the while loop. */
	const size_t c_max_name_len = (size_t)conf.max_name_len;
	const size_t c_min_name_trim = (size_t)conf.min_name_trim;

	while (--i >= 0) {
		size_t total_len = 0;
		file_info[i].eln_n = conf.no_eln == 1 ? -1 : DIGINUM(i + 1);

		size_t file_len = file_info[i].len;
		if (file_len == 0) {
			/* Invalid chars found. Reconstruct and recalculate length. */
			char *wname = replace_invalid_chars(file_info[i].name);
			file_len = wname ? wc_xstrlen(wname) : 0;
			free(wname);
		}

		/* In long view, we won't trim file names below MIN_NAME_TRIM. */
		const size_t max =
			checks.min_name_trim == 1 ? c_min_name_trim : c_max_name_len;
		if (file_len > max)
			file_len = max;

		total_len = pad + 1 + file_len;

		if (checks.classify == 1) {
			if (file_info[i].dir)
				total_len++;

			if (file_info[i].filesn > 0 && conf.files_counter == 1)
				total_len += DIGINUM(file_info[i].filesn);

			if (file_info[i].dir == 0 && conf.colorize == 0) {
				switch (file_info[i].type) {
				case DT_REG:
					if (file_info[i].exec == 1)
						total_len++;
					break;
				case DT_BLK:  /* fallthrough */
				case DT_CHR:  /* fallthrough */
#ifdef SOLARIS_DOORS
				case DT_DOOR: /* fallthrough */
				case DT_PORT: /* fallthrough */
#endif /* SOLARIS_DOORS */
				case DT_LNK:  /* fallthrough */
				case DT_SOCK: /* fallthrough */
				case DT_FIFO: /* fallthrough */
				case DT_UNKNOWN: total_len++; break;
				default: break;
				}
			}
		}

		if (total_len > longest.name_len) {
			longest_index = i;
			if (conf.listing_mode == VERTLIST || conf.max_files == UNSET
			|| i < c_max_files)
				longest.name_len = total_len;
		}
	}

	if (conf.long_view == 0 && conf.icons == 1 && conf.columned == 1)
		longest.name_len += ICON_LEN;

	/* LONGEST.FC_LEN stores the amount of digits taken by the files counter of
	 * the longest file name, provided it is a directory.
	 * We use this to trim file names up to MAX_NAME_LEN + LONGEST.FC_LEN, so
	 * that we can make use of the space taken by the files counter.
	 * Example:
	 *    longest_dirname/13
	 *    very_long_file_na~
	 * instead of
	 *    longest_dirname/13
	 *    very_long_file_~
	 * */

	longest.fc_len = 0;
	if (conf.max_name_len != UNSET && longest_index != -1
	&& file_info[longest_index].dir == 1
	&& file_info[longest_index].filesn > 0 && conf.files_counter == 1) {
		/* We add 1 here to count the slash between the dir name
		 * and the files counter too. However, in doing this the space
		 * between the trimmed file name and the next column is too
		 * tight (only one). By not adding it we get an extra space
		 * to relax a bit the space between columns. */
/*		longest.fc_len = DIGINUM(file_info[longest_index].filesn) + 1; */
		longest.fc_len = DIGINUM(file_info[longest_index].filesn);
		const size_t t = pad + c_max_name_len + 1 + longest.fc_len;
		if (t > longest.name_len)
			longest.fc_len -= t - longest.name_len;
		if ((int)longest.fc_len < 0)
			longest.fc_len = 0;
	}
}

/* Set a few extra properties needed for long view mode */
static void
set_long_attribs(const filesn_t n, const struct stat *a)
{
	if (conf.light_mode == 1) {
		switch (prop_fields.time) {
		case PROP_TIME_ACCESS: file_info[n].ltime = a->st_atime; break;
		case PROP_TIME_CHANGE: file_info[n].ltime = a->st_ctime; break;
		case PROP_TIME_MOD: file_info[n].ltime = a->st_mtime; break; /* NOLINT */
		case PROP_TIME_BIRTH:
#ifdef ST_BTIME_LIGHT
			file_info[n].ltime = a->ST_BTIME.tv_sec; break;
#else
			file_info[n].ltime = a->st_mtime; break;
#endif /* ST_BTIME_LIGHT */
		default: file_info[n].ltime = a->st_mtime; break; /* NOLINT */
		}

		file_info[n].blocks = a->st_blocks;
		file_info[n].linkn = a->st_nlink;
		file_info[n].mode = a->st_mode;
		file_info[n].uid = a->st_uid;
		file_info[n].gid = a->st_gid;
	}

	if (conf.full_dir_size == 1 && file_info[n].dir == 1
	&& file_info[n].type == DT_DIR) {
		file_info[n].size = dir_size(file_info[n].name, 1,
			&file_info[n].du_status);
	} else {
		file_info[n].size = FILE_SIZE_PTR(a);
	}
}

/* Return a pointer to the indicator char color and update IND_CHR to the
 * corresponding indicator character for the file whose index is INDEX. */
static inline char *
get_ind_char(const filesn_t index, int *ind_chr)
{
	if (file_info[index].symlink == 1 && checks.lnk_char == 1) {
		*ind_chr = file_info[index].sel == 1 ? SELFILE_CHR : LINK_CHR;
		return lc_c;
	} else {
		*ind_chr = file_info[index].sel == 1 ? SELFILE_CHR : ' ';
		return file_info[index].sel == 1 ? li_cb : "";
	}
}

/* Return a struct maxes_t with the following information: the longest files
 * counter, user ID, group ID, file size, inode, and file links in the
 * current list of files.
 * This information is required to build the properties line for each entry
 * based on each field's length. */
static struct maxes_t
compute_maxes(void)
{
	struct maxes_t maxes = {0};
	filesn_t i = xargs.max_files > 0 ? (filesn_t)xargs.max_files
		: (conf.max_files > 0 ? conf.max_files : files);

	while (--i >= 0) {
		int t = 0;
		if (file_info[i].dir == 1 && conf.files_counter == 1) {
			t = DIGINUM_BIG(file_info[i].filesn);
			if (t > maxes.files_counter)
				maxes.files_counter = t;
		}

		if (prop_fields.size == PROP_SIZE_BYTES) {
			t = DIGINUM_BIG(file_info[i].size);
			if (t > maxes.size)
				maxes.size = t;
		} else if (prop_fields.size == PROP_SIZE_HUMAN) {
			t = (int)file_info[i].human_size.len;
			if (t > maxes.size)
				maxes.size = t;
		}

		if (prop_fields.ids == PROP_ID_NUM) {
			const int u = DIGINUM(file_info[i].uid);
			const int g = DIGINUM(file_info[i].gid);
			if (g > maxes.id_group)
				maxes.id_group = g;
			if (u > maxes.id_user)
				maxes.id_user = u;
		} else if (prop_fields.ids == PROP_ID_NAME) {
			const int g = file_info[i].gid_i.name
				? (int)file_info[i].gid_i.namlen : DIGINUM(file_info[i].gid);
			if (g > maxes.id_group)
				maxes.id_group = g;

			const int u = file_info[i].uid_i.name
				? (int)file_info[i].uid_i.namlen : DIGINUM(file_info[i].uid);
			if (u > maxes.id_user)
				maxes.id_user = u;
		}

		if (prop_fields.inode == 1) {
			t = DIGINUM(file_info[i].inode);
			if (t > maxes.inode)
				maxes.inode = t;
		}

		if (prop_fields.links == 1) {
			t = DIGINUM((unsigned int)file_info[i].linkn);
			if (t > maxes.links)
				maxes.links = t;
		}

		if (prop_fields.blocks == 1) {
			t = DIGINUM_BIG(file_info[i].blocks);
			if (t > maxes.blocks)
				maxes.blocks = t;
		}
	}


	if (conf.full_dir_size != 1 || prop_fields.size == PROP_SIZE_HUMAN)
		return maxes;

	/* If at least one directory size length equals the maxmimum size lenght
	 * in the current directory, and we have a du(1) error for this directory,
	 * we need to make room for the du error char (!). */
	i = files;
	while (--i >= 0) {
		if (file_info[i].du_status == 0)
			continue;

		const int t = prop_fields.size == PROP_SIZE_BYTES
			? DIGINUM_BIG(file_info[i].size)
			: (int)file_info[i].human_size.len;

		if (t == maxes.size) {
			maxes.size++;
			break;
		}
	}

	return maxes;
}

static void
print_long_mode(size_t *counter, int *reset_pager, const int pad,
	int have_xattr)
{
	struct maxes_t maxes = compute_maxes();

	if (prop_fields.xattr == 0)
		have_xattr = 0;

	/* Available space (term cols) to print the file name. */
	int space_left = (int)term_cols - (prop_fields.len + have_xattr
		+ maxes.files_counter + maxes.size + maxes.links + maxes.inode
		+ maxes.id_user + (prop_fields.no_group == 0 ? maxes.id_group : 0)
		+ maxes.blocks + (conf.icons == 1 ? ICON_LEN : 0));

	if (space_left < conf.min_name_trim)
		space_left = conf.min_name_trim;

	if (conf.min_name_trim != UNSET && longest.name_len > (size_t)space_left)
		longest.name_len = (size_t)space_left;

	if (longest.name_len < (size_t)space_left)
		space_left = (int)longest.name_len;

	maxes.name = space_left + (conf.icons == 1 ? ICON_LEN : 0);
	pager_quit = pager_help = 0;

	filesn_t i, k = files;
	for (i = 0; i < k; i++) {
		if (conf.max_files != UNSET && i == conf.max_files)
			break;

		if (conf.pager == 1 || (*reset_pager == 0 && conf.pager > 1
		&& files >= (filesn_t)conf.pager)) {
			int ret = 0;
			if (*counter > (size_t)(term_lines - 2))
				ret = run_pager(-1, reset_pager, &i, counter);
			if (ret == -3) {
				pager_quit = 1;
				break;
			}

			if (ret == -1 || ret == -2) {
				--i;
				if (ret == -2)
					*counter = 0;
				continue;
			}
			++(*counter);
		}

		int ind_chr = 0;
		const char *ind_chr_color = get_ind_char(i, &ind_chr);

		if (conf.no_eln == 0) {
			printf("%s%*jd%s%s%c%s", el_c, pad, (intmax_t)i + 1, df_c,
				ind_chr_color, ind_chr, df_c);
		} else {
			printf("%s%c%s", ind_chr_color, ind_chr, df_c);
		}

		/* Print the remaining part of the entry. */
		print_entry_props(&file_info[i], &maxes, have_xattr);
	}

	if (pager_quit == 1)
		printf("... (%zd/%zd)\n", i, files);
}

static size_t
get_columns(void)
{
	/* LONGEST.NAME_LEN is size_t: it will never be less than zero. */
	size_t n = (size_t)term_cols / (longest.name_len + 1);
	/* +1 for the space between file names. */

	/* If LONGEST.NAME_LEN is bigger than terminal columns, N will zero.
	 * To avoid this: */
	if (n < 1)
		n = 1;

	/* If we have only three files, we don't want four columns */
	if (n > (size_t)files)
		n = (size_t)files > 0 ? (size_t)files : 1;

	return n;
}

static size_t
get_ext_info(const filesn_t i, int *trim_type)
{
	if (!file_info[i].ext_name) {
		char *dot = strrchr(file_info[i].name, '.');
		if (!dot || dot == file_info[i].name || !dot[1])
			return 0;

		file_info[i].ext_name = dot;
	}

	*trim_type = TRIM_EXT;

	size_t ext_len = 0;
	size_t bytes = 0;
	const char *e = file_info[i].ext_name;

	if (file_info[i].utf8 == 0) {
		/* Extension names are usually short. Let's call strlen(2) only
		 * when it is longer than 6 (including the initial dot). */
		ext_len = !e[1] ? 1
			: !e[2] ? 2
			: !e[3] ? 3
			: !e[4] ? 4
			: !e[5] ? 5
			: !e[6] ? 6
			: strlen(e);
	} else if (is_utf8_name(e, &bytes) == 0) {
		ext_len = bytes;
	} else {
		ext_len = wc_xstrlen(e);
	}

	if ((int)ext_len >= conf.max_name_len - 1 || (int)ext_len <= 0) {
		ext_len = 0;
		*trim_type = TRIM_NO_EXT;
	}

	return ext_len;
}

/* Construct the file name to be displayed.
 * The file name is trimmed if longer than MAX_NAMELEN (if conf.max_name_len
 * is set). */
static char *
construct_filename(const filesn_t i, struct wtrim_t *wtrim,
	const int max_namelen)
{
	/* When quitting the help screen of the pager, the list of files is
	 * reprinted from the first entry. Now, when printing the list for the
	 * first time the LEN field of the file_info struct is set to the displayed
	 * length, which, if the name is trimmed, is the same as MAX_NAME_LEN
	 * (causing thus subsequent prints of the list to never trim long file
	 * names, because the value is already <= MAX_NAME_LEN).
	 * Because of this, we need to recalculate the length of the current entry
	 * whenever we're coming from the pager help screen, in order to know
	 * whether we should trim the name or not. */
	size_t namelen = pager_help == 1
		? (file_info[i].utf8 == 1 ? wc_xstrlen(file_info[i].name)
		: file_info[i].bytes) : file_info[i].len;

	/* file_info[i].len is zero whenever an invalid character was found in
	 * the file name. Let's recalculate the name length. */
	if (namelen == 0) {
		wtrim->wname = replace_invalid_chars(file_info[i].name);
		namelen = file_info[i].len = wc_xstrlen(wtrim->wname);
	}

	char *name = wtrim->wname ? wtrim->wname : file_info[i].name;

	if (files <= 1 || conf.max_name_len == UNSET || conf.long_view != 0
	|| (int)namelen <= max_namelen)
		return name;

	/* Let's trim the file name (at MAX_NAMELEN, in this example, 11).
	 * A. If no file extension, just trim at 11:  "long_filen~"
	 * B. If we have an extension, keep it:       "long_f~.ext"
	 *
	 * This is the place to implement an alternative trimming procedure, say,
	 * to trim names at the middle, like MC does: "long_~ename"
	 * or, if we have an extension:               "long_~e.ext"
	 *
	 * We only need a function similar to get_ext_info(), say, get_suffix_info()
	 * wtrim->trim should be set to TRIM_EXT
	 * ext_len to the length of the suffix (either "ename" or "e.ext", i.e. 5)
	 * file_info[i].ext_name should be a pointer to the beggining of SUFFIX
	 * in file_info[i].name (this might impact on some later operation!) */

	wtrim->type = TRIM_NO_EXT;
	size_t ext_len = get_ext_info(i, &wtrim->type);

	const int trim_len = max_namelen - 1 - (int)ext_len;

	if (file_info[i].utf8 == 1) {
		if (wtrim->wname)
			xstrsncpy(name_buf, wtrim->wname, sizeof(name_buf));
		else /* memcpy is faster: use it whenever possible. */
			memcpy(name_buf, file_info[i].name, file_info[i].bytes + 1);

		wtrim->diff = u8truncstr(name_buf, (size_t)trim_len);
	} else {
		/* If not UTF-8, let's avoid u8truncstr(). It's a bit faster this way. */
		const char c = name[trim_len];
		name[trim_len] = '\0';
		mbstowcs((wchar_t *)name_buf, name, NAME_BUF_SIZE);
		name[trim_len] = c;
	}

	file_info[i].len = (size_t)max_namelen;

	/* At this point we have truncated name. "~" and ".ext" will be appended
	 * later by one of the print_entry functions. */

	return name_buf;
}

static void
print_entry_color(int *ind_char, const filesn_t i, const int pad,
	const int max_namelen)
{
	*ind_char = 0;
	const char *end_color = file_info[i].dir == 1 ? fc_c : df_c;

	struct wtrim_t wtrim = (struct wtrim_t){0};
	const char *n = construct_filename(i, &wtrim, max_namelen);

	const char *trim_diff = wtrim.diff > 0 ? gen_diff_str(wtrim.diff) : "";

	int ind_chr = 0;
	const char *ind_chr_color = get_ind_char(i, &ind_chr);

#ifndef _NO_ICONS
	if (conf.icons == 1) {
		if (conf.no_eln == 1) {
			if (wtrim.type > 0) {
				xprintf("%s%c%s%s%s %s%ls%s\x1b[0m%s%c\x1b[0m%s%s%s",
					ind_chr_color, ind_chr, df_c,
					file_info[i].icon_color, file_info[i].icon,
					file_info[i].color, (wchar_t *)n, trim_diff,
					tt_c, TRIMFILE_CHR,
					wtrim.type == TRIM_EXT ? file_info[i].color : "",
					wtrim.type == TRIM_EXT ? file_info[i].ext_name : "",
					end_color);
			} else {
				xprintf("%s%c%s%s%s %s%s%s",
					ind_chr_color, ind_chr, df_c,
					file_info[i].icon_color, file_info[i].icon,
					file_info[i].color, n, end_color);
			}
		} else {
			if (wtrim.type > 0) {
				xprintf("%s%*jd%s%s%c%s%s%s %s%ls%s\x1b[0m%s%c\x1b[0m%s%s%s",
					el_c, pad, (intmax_t)i + 1, df_c, ind_chr_color, ind_chr,
					df_c, file_info[i].icon_color, file_info[i].icon,
					file_info[i].color, (wchar_t *)n, trim_diff,
					tt_c, TRIMFILE_CHR,
					wtrim.type == TRIM_EXT ? file_info[i].color : "",
					wtrim.type == TRIM_EXT ? file_info[i].ext_name : "",
					end_color);
			} else {
				xprintf("%s%*jd%s%s%c%s%s%s %s%s%s", el_c, pad, (intmax_t)i + 1,
					df_c, ind_chr_color, ind_chr, df_c,
					file_info[i].icon_color, file_info[i].icon,
					file_info[i].color, n, end_color);
			}
		}
	} else
#endif /* !_NO_ICONS */
	{
		if (conf.no_eln == 1) {
			if (wtrim.type > 0) {
				xprintf("%s%c%s%s%ls%s\x1b[0m%s%c\x1b[0m%s%s%s", ind_chr_color,
					(sel_n > 0 || conf.color_lnk_as_target == 1) ? ind_chr : 0,
					df_c, file_info[i].color, (wchar_t *)n, trim_diff,
					tt_c, TRIMFILE_CHR,
					wtrim.type == TRIM_EXT ? file_info[i].color : "",
					wtrim.type == TRIM_EXT ? file_info[i].ext_name : "",
					end_color);
			} else {
				xprintf("%s%c%s%s%s%s", ind_chr_color,
					(sel_n > 0 || conf.color_lnk_as_target == 1) ? ind_chr : 0,
					df_c, file_info[i].color, n, end_color);
			}
		} else {
			if (wtrim.type > 0) {
				xprintf("%s%*jd%s%s%c%s%s%ls%s\x1b[0m%s%c\x1b[0m%s%s%s",
					el_c, pad, (intmax_t)i + 1, df_c, ind_chr_color,
					ind_chr,
					df_c, file_info[i].color, (wchar_t *)n,
					trim_diff, tt_c, TRIMFILE_CHR,
					wtrim.type == TRIM_EXT ? file_info[i].color : "",
					wtrim.type == TRIM_EXT ? file_info[i].ext_name : "",
					end_color);
			} else {
				xprintf("%s%*jd%s%s%c%s%s%s%s", el_c, pad, (intmax_t)i + 1,
					df_c, ind_chr_color, ind_chr, df_c,
					file_info[i].color, n, end_color);
			}
		}
	}

	if (conf.classify == 1) {
		/* Append directory indicator and files counter */
		switch (file_info[i].type) {
		case DT_DIR: /* fallthrough */
		case DT_LNK:
			if (file_info[i].dir == 1)
				putchar(DIR_CHR);
			if (file_info[i].filesn > 0 && conf.files_counter == 1)
				fputs(xitoa(file_info[i].filesn), stdout);
			break;
		default: break;
		}
	}

	if (end_color == fc_c)
		fputs(df_c, stdout);

	free(wtrim.wname);
}

static void
print_entry_nocolor(int *ind_char, const filesn_t i, const int pad,
	const int max_namelen)
{
	struct wtrim_t wtrim = (struct wtrim_t){0};
	const char *n = construct_filename(i, &wtrim, max_namelen);

	const char *trim_diff = wtrim.diff > 0 ? gen_diff_str(wtrim.diff) : "";

#ifndef _NO_ICONS
	if (conf.icons == 1) {
		if (conf.no_eln == 1) {
			if (wtrim.type > 0) {
				xprintf("%c%s %ls%s%c%s", file_info[i].sel ? SELFILE_CHR : ' ',
					file_info[i].icon, (wchar_t *)n, trim_diff, TRIMFILE_CHR,
					wtrim.type == TRIM_EXT ? file_info[i].ext_name : "");
			} else {
				xprintf("%c%s %s", file_info[i].sel ? SELFILE_CHR : ' ',
				file_info[i].icon, n);
			}
		} else {
			if (wtrim.type > 0) {
				xprintf("%s%*jd%s%c%s %ls%s%c%s", el_c, pad, (intmax_t)i + 1,
					df_c, file_info[i].sel ? SELFILE_CHR : ' ',
					file_info[i].icon, (wchar_t *)n, trim_diff, TRIMFILE_CHR,
					wtrim.type == TRIM_EXT ? file_info[i].ext_name : "");
			} else {
				xprintf("%s%*jd%s%c%s %s", el_c, pad, (intmax_t)i + 1, df_c,
					file_info[i].sel ? SELFILE_CHR : ' ', file_info[i].icon, n);
			}
		}
	} else
#endif /* _NO_ICONS */
	{
		if (conf.no_eln == 1) {
			if (wtrim.type > 0) {
				xprintf("%c%ls%s%c%s", file_info[i].sel ? SELFILE_CHR : ' ',
					(wchar_t *)n, trim_diff, TRIMFILE_CHR,
					wtrim.type == TRIM_EXT ? file_info[i].ext_name : "");
			} else {
				xprintf("%c%s", file_info[i].sel ? SELFILE_CHR : ' ', n);
			}
		} else {
			if (wtrim.type > 0) {
				xprintf("%s%*jd%s%c%ls%s%c%s", el_c, pad, (intmax_t)i + 1, df_c,
					file_info[i].sel ? SELFILE_CHR : ' ', (wchar_t *)n,
					trim_diff, TRIMFILE_CHR, wtrim.type == TRIM_EXT
					? file_info[i].ext_name : "");
			} else {
				xprintf("%s%*jd%s%c%s", el_c, pad, (intmax_t)i + 1, df_c,
					file_info[i].sel ? SELFILE_CHR : ' ', n);
			}
		}
	}

	if (conf.classify == 1) {
		/* Append file type indicator */
		switch (file_info[i].type) {
		case DT_DIR:
			*ind_char = 0;
			putchar(DIR_CHR);
			if (file_info[i].filesn > 0 && conf.files_counter == 1)
				fputs(xitoa(file_info[i].filesn), stdout);
			break;

		case DT_LNK:
			if (file_info[i].color == or_c) {
				putchar(BRK_LNK_CHR);
			} else if (file_info[i].dir) {
				*ind_char = 0;
				putchar(DIR_CHR);
				if (file_info[i].filesn > 0 && conf.files_counter == 1)
					fputs(xitoa(file_info[i].filesn), stdout);
			} else {
				putchar(LINK_CHR);
			}
			break;

		case DT_REG:
			if (file_info[i].exec == 1)
				putchar(EXEC_CHR);
			else
				*ind_char = 0;
			break;

		case DT_BLK: putchar(BLK_CHR); break;
		case DT_CHR: putchar(CHR_CHR); break;
#ifdef SOLARIS_DOORS
		case DT_DOOR: putchar(DOOR_CHR); break;
//		case DT_PORT: break;
#endif /* SOLARIS_DOORS */
		case DT_FIFO: putchar(FIFO_CHR); break;
		case DT_SOCK: putchar(SOCK_CHR); break;
#ifdef S_IFWHT
		case DT_WHT: putchar(WHT_CHR); break;
#endif /* S_IFWHT */
		case DT_UNKNOWN: putchar(UNK_CHR); break;
		default: *ind_char = 0;
		}
	}

	free(wtrim.wname);
}

static void
print_entry_color_light(int *ind_char, const filesn_t i,
	const int pad, const int max_namelen)
{
	*ind_char = 0;
	const char *end_color = file_info[i].dir == 1 ? fc_c : df_c;

	struct wtrim_t wtrim = (struct wtrim_t){0};
	const char *n = construct_filename(i, &wtrim, max_namelen);

	const char *trim_diff = wtrim.diff > 0 ? gen_diff_str(wtrim.diff) : "";

#ifndef _NO_ICONS
	if (conf.icons == 1) {
		if (xargs.icons_use_file_color == 1)
			file_info[i].icon_color = file_info[i].color;

		if (conf.no_eln == 1) {
			if (wtrim.type > 0) {
				xprintf("%s%s %s%ls%s\x1b[0m%s%c\x1b[0m%s%s%s",
					file_info[i].icon_color, file_info[i].icon,
					file_info[i].color, (wchar_t *)n, trim_diff,
					tt_c, TRIMFILE_CHR,
					wtrim.type == TRIM_EXT ? file_info[i].color : "",
					wtrim.type == TRIM_EXT ? file_info[i].ext_name : "",
					end_color);
			} else {
				xprintf("%s%s %s%s%s", file_info[i].icon_color,
					file_info[i].icon, file_info[i].color, n, end_color);
			}
		} else {
			if (wtrim.type > 0) {
				xprintf("%s%*jd%s %s%s %s%ls%s\x1b[0m%s%c\x1b[0m%s%s%s",
					el_c, pad, (intmax_t)i + 1, df_c, file_info[i].icon_color,
					file_info[i].icon, file_info[i].color, (wchar_t *)n,
					trim_diff, tt_c, TRIMFILE_CHR,
					wtrim.type == TRIM_EXT ? file_info[i].color : "",
					wtrim.type == TRIM_EXT ? file_info[i].ext_name : "",
					end_color);
			} else {
				xprintf("%s%*jd%s %s%s %s%s%s", el_c, pad, (intmax_t)i + 1,
					df_c, file_info[i].icon_color, file_info[i].icon,
					file_info[i].color, n, end_color);
			}
		}
	} else
#endif /* _NO_ICONS */
	{
		if (conf.no_eln == 1) {
			if (wtrim.type > 0) {
				xprintf("%s%ls%s\x1b[0m%s%c\x1b[0m%s%s%s",
					file_info[i].color, (wchar_t *)n,
					trim_diff, tt_c, TRIMFILE_CHR,
					wtrim.type == TRIM_EXT ? file_info[i].color : "",
					wtrim.type == TRIM_EXT ? file_info[i].ext_name : "",
					end_color);
			} else {
				xprintf("%s%s%s", file_info[i].color, n, end_color);
			}
		} else {
			if (wtrim.type > 0) {
				xprintf("%s%*jd%s %s%ls%s\x1b[0m%s%c\x1b[0m%s%s%s",
					el_c, pad, (intmax_t)i + 1, df_c, file_info[i].color,
					(wchar_t *)n, trim_diff, tt_c, TRIMFILE_CHR,
					wtrim.type == TRIM_EXT ? file_info[i].color : "",
					wtrim.type == TRIM_EXT ? file_info[i].ext_name : "",
					end_color);
			} else {
				xprintf("%s%*jd%s %s%s%s", el_c, pad, (intmax_t)i + 1, df_c,
					file_info[i].color, n, end_color);
			}
		}
	}

	if (file_info[i].dir == 1 && conf.classify == 1) {
		putchar(DIR_CHR);
		if (file_info[i].filesn > 0 && conf.files_counter == 1)
			fputs(xitoa(file_info[i].filesn), stdout);
	}

	if (end_color == fc_c)
		fputs(df_c, stdout);

	free(wtrim.wname);
}

static void
print_entry_nocolor_light(int *ind_char, const filesn_t i,
	const int pad, const int max_namelen)
{
	struct wtrim_t wtrim = (struct wtrim_t){0};
	const char *n = construct_filename(i, &wtrim, max_namelen);

	const char *trim_diff = wtrim.diff > 0 ? gen_diff_str(wtrim.diff) : "";

#ifndef _NO_ICONS
	if (conf.icons == 1) {
		if (conf.no_eln == 1) {
			if (wtrim.type > 0) {
				xprintf("%s %ls%s%c%s", file_info[i].icon, (wchar_t *)n,
					trim_diff, TRIMFILE_CHR,
					wtrim.type == TRIM_EXT ? file_info[i].ext_name : "");
			} else {
				xprintf("%s %s", file_info[i].icon, n);
			}
		} else {
			if (wtrim.type > 0) {
				xprintf("%s%*jd%s %s %ls%s%c%s", el_c, pad, (intmax_t)i + 1,
					df_c, file_info[i].icon, (wchar_t *)n, trim_diff,
					TRIMFILE_CHR, wtrim.type == TRIM_EXT
					? file_info[i].ext_name : "");
			} else {
				xprintf("%s%*jd%s %s %s", el_c, pad, (intmax_t)i + 1, df_c,
					file_info[i].icon, n);
			}
		}
	} else
#endif /* _NO_ICONS */
	{
		if (conf.no_eln == 1) {
			if (wtrim.type > 0) {
				xprintf("%ls%s%c%s", (wchar_t *)n, trim_diff, TRIMFILE_CHR,
					wtrim.type == TRIM_EXT ? file_info[i].ext_name : "");
			} else {
				fputs(file_info[i].name, stdout);
			}
		} else {
			if (wtrim.type > 0) {
				xprintf("%s%*jd%s %ls%s%c%s", el_c, pad, (intmax_t)i + 1, df_c,
					(wchar_t *)n, trim_diff, TRIMFILE_CHR,
					wtrim.type == TRIM_EXT ? file_info[i].ext_name : "");
			} else {
				xprintf("%s%*jd%s %s", el_c, pad, (intmax_t)i + 1, df_c, n);
			}
		}
	}

	if (conf.classify == 1) {
		switch (file_info[i].type) {
		case DT_DIR:
			*ind_char = 0;
			putchar(DIR_CHR);
			if (file_info[i].filesn > 0 && conf.files_counter == 1)
				fputs(xitoa(file_info[i].filesn), stdout);
			break;

		case DT_BLK: putchar(BLK_CHR); break;
		case DT_CHR: putchar(CHR_CHR); break;
#ifdef SOLARIS_DOORS
		case DT_DOOR: putchar(DOOR_CHR); break;
//		case DT_DOOR: break;
#endif /* SOLARIS_DOORS */
		case DT_FIFO: putchar(FIFO_CHR); break;
		case DT_LNK: putchar(LINK_CHR); break;
		case DT_SOCK: putchar(SOCK_CHR); break;
#ifdef S_IFWHT
		case DT_WHT: putchar(WHT_CHR); break;
#endif /* S_IFWHT */
		case DT_UNKNOWN: putchar(UNKNOWN_CHR); break;
		default: *ind_char = 0; break;
		}
	}

	free(wtrim.wname);
}

/* Right pad the current file name (adding spaces) to equate the longest
 * file name length. */
static void
pad_filename(const int ind_char, const filesn_t i, const int pad,
	const int termcap_move_right)
{
	int cur_len = 0;

#ifndef _NO_ICONS
	cur_len = pad + 1 + (conf.icons == 1 ? ICON_LEN : 0) + (int)file_info[i].len
		+ (ind_char ? 1 : 0);
#else
	cur_len = pad + 1 + (int)file_info[i].len + (ind_char ? 1 : 0);
#endif /* !_NO_ICONS */

	if (file_info[i].dir == 1 && conf.classify == 1) {
		++cur_len;
		if (file_info[i].filesn > 0 && conf.files_counter == 1
		&& file_info[i].ruser == 1)
			cur_len += DIGINUM((int)file_info[i].filesn);
	}

	const int diff = (int)longest.name_len - cur_len;
	if (termcap_move_right == 0) {
		int j = diff + 1;
		while (--j >= 0)
			putchar(' ');
	} else {
		MOVE_CURSOR_RIGHT(diff + 1);
	}
}

/* List files horizontally:
 * 1 AAA	2 AAB	3 AAC
 * 4 AAD	5 AAE	6 AAF */
static void
list_files_horizontal(size_t *counter, int *reset_pager,
	const int pad, const size_t columns_n)
{
	const filesn_t nn = (conf.max_files != UNSET
		&& (filesn_t)conf.max_files < files) ? (filesn_t)conf.max_files : files;

	void (*print_entry_function)(int *, const filesn_t, const int, const int);
	if (conf.colorize == 1)
		print_entry_function = conf.light_mode == 1
			? print_entry_color_light : print_entry_color;
	else
		print_entry_function = conf.light_mode == 1
			? print_entry_nocolor_light : print_entry_nocolor;

	const int termcap_move_right = (xargs.list_and_quit == 1
		|| term_caps.suggestions == 0) ? 0 : 1;

	const int int_longest_fc_len = (int)longest.fc_len;
	size_t cur_cols = 0;
	filesn_t i;
	int last_column = 0;
	int blc = last_column;

	pager_quit = pager_help = 0;

	for (i = 0; i < nn; i++) {
		/* If current entry is in the last column, we need to print a new line char */
		size_t bcur_cols = cur_cols;
		if (++cur_cols == columns_n) {
			cur_cols = 0;
			last_column = 1;
		} else {
			last_column = 0;
		}

		int ind_char = (conf.classify != 0);

				/* ##########################
				 * #  MAS: A SIMPLE PAGER   #
				 * ########################## */

		if (conf.pager == 1 || (*reset_pager == 0 && conf.pager > 1
		&& files >= (filesn_t)conf.pager)) {
			/* Run the pager only once all columns and rows fitting in
			 * the screen are filled with the corresponding file names */
			int ret = 0;
			filesn_t bi = i;
			if (blc && *counter > columns_n * ((size_t)term_lines - 2))
				ret = run_pager((int)columns_n, reset_pager, &i, counter);

			if (ret == -3) {
				pager_quit = 1;
				goto END;
			}

			if (ret == -1) {
				i = bi ? bi - 1 : bi;
				cur_cols = bcur_cols;
				last_column = blc;
				continue;
			}
			(*counter)++;
		}

		blc = last_column;

			/* #################################
			 * #    PRINT THE CURRENT ENTRY    #
			 * ################################# */

		const int fc = file_info[i].dir != 1 ? int_longest_fc_len : 0;
		/* Displayed file name will be trimmed to MAX_NAME_LEN. */
		const int max_namelen = conf.max_name_len + fc;

		file_info[i].eln_n = conf.no_eln == 1 ? -1 : DIGINUM(i + 1);

		print_entry_function(&ind_char, i, pad, max_namelen);

		if (last_column == 0)
			pad_filename(ind_char, i, pad, termcap_move_right);
		else
			putchar('\n');
	}

END:
	if (last_column == 0)
		putchar('\n');
	if (pager_quit == 1)
		printf("... (%zd/%zd)\n", i, files);
}

/* List files vertically, like ls(1) would
 * 1 AAA	3 AAC	5 AAE
 * 2 AAB	4 AAD	6 AAF */
static void
list_files_vertical(size_t *counter, int *reset_pager,
	const int pad, const size_t columns_n)
{
	/* Total amount of files to be listed. */
	const filesn_t nn = (conf.max_files != UNSET
		&& (filesn_t)conf.max_files < files) ? (filesn_t)conf.max_files : files;

	/* How many lines (rows) do we need to print NN files? */
	/* Division/modulo is slow, true. But the compiler will make a much
	 * better job than us at optimizing this code. */
	/* COLUMNS_N is guarranteed to be >0 by get_columns() */
	filesn_t rows = nn / (filesn_t)columns_n;
	if (nn % (filesn_t)columns_n > 0)
		++rows;

	int last_column = 0;
	/* The previous value of LAST_COLUMN. We need this value to run the pager. */
	int blc = last_column;

	void (*print_entry_function)(int *, const filesn_t, const int, const int);
	if (conf.colorize == 1)
		print_entry_function = conf.light_mode == 1
			? print_entry_color_light : print_entry_color;
	else
		print_entry_function = conf.light_mode == 1
			? print_entry_nocolor_light : print_entry_nocolor;

	const int termcap_move_right = (xargs.list_and_quit == 1
		|| term_caps.suggestions == 0) ? 0 : 1;

	const int int_longest_fc_len = (int)longest.fc_len;
	size_t cur_cols = 0;
	size_t cc = columns_n; // Current column number
	filesn_t x = 0; // Index of the file to be actually printed
	filesn_t xx = 0; // Current line number
	filesn_t i = 0; // Index of the current entry being analyzed

	pager_quit = pager_help = 0;

	for ( ; ; i++) {
		/* Copy current values to restore them if necessary: done to
		 * skip the first two chars of arrow keys : \x1b [ */
		filesn_t bxx = xx, bx = x; // Copies of X and XX
		size_t bcc = cc; // Copy of CC
		if (cc == columns_n) {
			x = xx;
			++xx;
			cc = 0;
		} else {
			x += rows;
		}
		++cc;

		if (xx > rows)
			break;

		size_t bcur_cols = cur_cols;
		/* If current entry is in the last column, print a new line char */
		if (++cur_cols == columns_n) {
			cur_cols = 0;
			last_column = 1;
		} else {
			last_column = 0;
		}

		int ind_char = (conf.classify != 0);

		if (x >= nn || !file_info[x].name) {
			if (last_column == 1)
				/* Last column is empty. E.g.:
				 * 1 file  3 file3  5 file5
				 * 2 file2 4 file4  HERE
				 * ... */
				putchar('\n');
			continue;
		}

				/* ##########################
				 * #  MAS: A SIMPLE PAGER   #
				 * ########################## */

		if (conf.pager == 1 || (*reset_pager == 0 && conf.pager > 1
		&& files >= (filesn_t)conf.pager)) {
			int ret = 0;
			filesn_t bi = i;
			/* Run the pager only once all columns and rows fitting in
			 * the screen are filled with the corresponding file names */
			if (blc && *counter > columns_n * ((size_t)term_lines - 2))
				ret = run_pager((int)columns_n, reset_pager, &x, counter);

			if (ret == -3) {
				pager_quit = 1;
				goto END;
			}

			if (ret == -1) {
				/* Restore previous values */
				i = bi ? bi - 1: bi;
				x = bx;
				xx = bxx;
				cc = bcc;
				cur_cols = bcur_cols;
				continue;
			} else {
				if (ret == -2) {
					i = x = xx = last_column = blc = 0;
					*counter = cur_cols = 0;
					cc = columns_n;
					continue;
				}
			}
			++(*counter);
		}

		blc = last_column;

			/* #################################
			 * #    PRINT THE CURRENT ENTRY    #
			 * ################################# */

		const int fc = file_info[x].dir != 1 ? int_longest_fc_len : 0;
		/* Displayed file name will be trimmed to MAX_NAMELEN. */
		const int max_namelen = conf.max_name_len + fc;

		file_info[x].eln_n = conf.no_eln == 1 ? -1 : DIGINUM(x + 1);

		print_entry_function(&ind_char, x, pad, max_namelen);

		if (last_column == 0)
			pad_filename(ind_char, x, pad, termcap_move_right);
		else
			/* Last column is populated. Example:
			 * 1 file  3 file3  5 file5HERE
			 * 2 file2 4 file4  6 file6HERE
			 * ... */
			putchar('\n');
	}

END:
	if (last_column == 0)
		putchar('\n');
	if (pager_quit == 1)
		printf("... (%zd/%zd)\n", i, files);
}

/* Execute commands in either AUTOCMD_DIR_IN_FILE or AUTOCMD_DIR_OUT_FILE files.
 * MODE (either AUTOCMD_DIR_IN or AUTOCMD_DIR_OUT) tells the function
 * whether to check for AUTOCMD_DIR_IN_FILE or AUTOCMD_DIR_OUT_FILE files.
 * Used by the autocommands function. */
static void
run_dir_cmd(const int mode)
{
	char path[PATH_MAX + 1];

	if (mode == AUTOCMD_DIR_IN) {
		snprintf(path, sizeof(path), "%s/%s",
			workspaces[cur_ws].path, AUTOCMD_DIR_IN_FILE);
	} else { /* AUTOCMD_DIR_OUT */
		if (dirhist_cur_index <= 0 || !old_pwd
		|| !old_pwd[dirhist_cur_index - 1])
			return;
		snprintf(path, sizeof(path), "%s/%s",
			old_pwd[dirhist_cur_index - 1], AUTOCMD_DIR_OUT_FILE);
	}

	int fd = 0;
	FILE *fp;
	struct stat a;

	/* Non-regular files, empty regular files, or bigger than PATH_MAX bytes,
	 * are rejected. */
	if (lstat(path, &a) == -1 || !S_ISREG(a.st_mode)
	|| a.st_size == 0 || a.st_size > PATH_MAX
	|| !(fp = open_fread(path, &fd)))
		return;

	char buf[PATH_MAX + 1];
	*buf = '\0';
	char *ret = fgets(buf, sizeof(buf), fp);
	size_t buf_len = *buf ? strnlen(buf, sizeof(buf)) : 0;
	if (buf_len > 0 && buf[buf_len - 1] == '\n') {
		buf[buf_len - 1] = '\0';
		buf_len--;
	}

	if (!ret || buf_len == 0 || memchr(buf, '\0', buf_len)) {
		/* Empty line, or it contains a NUL byte (probably binary): reject it. */
		fclose(fp);
		return;
	}

	fclose(fp);

	if (xargs.secure_cmds == 0
	|| sanitize_cmd(buf, SNT_AUTOCMD) == FUNC_SUCCESS)
		launch_execl(buf);
}

/* Check if the file named S is either cfm.in or cfm.out.
 * We already know it starts with a dot. */
static void
check_autocmd_file(const char *s)
{
	if (s[0] == 'c' && s[1] == 'f' && s[2] == 'm' && s[3] == '.') {
		if (s[4] == 'o' && s[5] == 'u' && s[6] == 't' && !s[7]) {
			dir_out = 1;
		} else {
			if (s[4] == 'i' && s[5] == 'n' && !s[6])
				run_dir_cmd(AUTOCMD_DIR_IN);
		}
	}
}

/* If the file at offset I is largest than SIZE, store information about this
 * file in NAME and COLOR, and update SIZE to the size of the new largest file.
 * Also, TOTAL is increased with the size of the file under analysis. */
static void
get_largest_file_info(const filesn_t i, off_t *size, char **name,
	char **color, off_t *total)
{
	/* Only directories and regular files should be counted. */
	if (file_info[i].type != DT_DIR && file_info[i].type != DT_REG
	&& (file_info[i].type != DT_LNK || conf.apparent_size != 1))
		return;

	if (file_info[i].size > *size) {
		*size = file_info[i].size;
		*name = file_info[i].name;
		*color = file_info[i].color;
	}

	/* Do not recount hardlinks in the same directory. */
	if (file_info[i].linkn > 1 && i > 0) {
		filesn_t j = i;
		while (--j >= 0) {
			if (file_info[i].inode == file_info[j].inode)
				return;
		}
	}

	*total += file_info[i].size;
}

static int
exclude_file_type_light(const unsigned char type)
{
	if (!*(filter.str + 1))
		return FUNC_FAILURE;

	int match = 0;

	switch (*(filter.str + 1)) {
	case 'd': if (type == DT_DIR)  match = 1; break;
	case 'f': if (type == DT_REG)  match = 1; break;
	case 'l': if (type == DT_LNK)  match = 1; break;
	case 's': if (type == DT_SOCK) match = 1; break;
	case 'c': if (type == DT_CHR)  match = 1; break;
	case 'b': if (type == DT_BLK)  match = 1; break;
	case 'p': if (type == DT_FIFO) match = 1; break;
#ifdef SOLARIS_DOORS
	case 'D': if (type == DT_DOOR) match = 1; break;
	case 'P': if (type == DT_PORT) match = 1; break;
#endif /* SOLARIS_DOORS */
	default: return FUNC_FAILURE;
	}

	if (match == 1)
		return filter.rev == 1 ? FUNC_SUCCESS : FUNC_FAILURE;
	else
		return filter.rev == 1 ? FUNC_FAILURE : FUNC_SUCCESS;
}

/* Returns FUNC_SUCCESS if the file with mode MODE and LINKS number
 * of links must be excluded from the files list, or FUNC_FAILURE. */
static int
exclude_file_type(const mode_t mode, const nlink_t links)
{
/* ADD: C = Files with capabilities */

	if (!*(filter.str + 1))
		return FUNC_FAILURE;

	int match = 0;

	switch (*(filter.str + 1)) {
	case 'b': if (S_ISBLK(mode))  match = 1; break;
	case 'd': if (S_ISDIR(mode))  match = 1; break;
#ifdef SOLARIS_DOORS
	case 'D': if (S_ISDOOR(mode)) match = 1; break;
	case 'P': if (S_ISPORT(mode)) match = 1; break;
#endif /* SOLARIS_DOORS */
	case 'c': if (S_ISCHR(mode))  match = 1; break;
	case 'f': if (S_ISREG(mode))  match = 1; break;
	case 'l': if (S_ISLNK(mode))  match = 1; break;
	case 'p': if (S_ISFIFO(mode)) match = 1; break;
	case 's': if (S_ISSOCK(mode)) match = 1; break;

	case 'g': if (mode & 02000) match = 1; break; /* SGID */
	case 'h': if (links > 1 && !S_ISDIR(mode)) match = 1; break;
	case 'o': if (mode & 00002) match = 1; break; /* Other writable */
	case 't': if (mode & 01000) match = 1; break; /* Sticky */
	case 'u': if (mode & 04000) match = 1; break; /* SUID */
	case 'x': /* Executable */
		if (S_ISREG(mode) && ((mode & 00100) || (mode & 00010)
		|| (mode & 00001)))
			match = 1;
		break;
	default: return FUNC_FAILURE;
	}

	if (match == 1)
		return filter.rev == 1 ? FUNC_SUCCESS : FUNC_FAILURE;
	else
		return filter.rev == 1 ? FUNC_FAILURE : FUNC_SUCCESS;
}

/* Initialize the file_info struct, mostly in case stat fails. */
static inline void
init_fileinfo(const filesn_t n)
{
	file_info[n] = (struct fileinfo){0};
	file_info[n].color = df_c;
#ifdef _NO_ICONS
	file_info[n].icon = (char *)NULL;
	file_info[n].icon_color = df_c;
#else
	file_info[n].icon = DEF_FILE_ICON;
	file_info[n].icon_color = DEF_FILE_ICON_COLOR;
#endif /* _NO_ICONS */
	file_info[n].human_size.str[0] = '\0';
	file_info[n].human_size.len = 0;
	file_info[n].human_size.unit = 0;
	file_info[n].linkn = 1;
	file_info[n].ruser = 1;
	file_info[n].size =  1;
}

static inline void
get_id_names(const filesn_t n)
{
	size_t i;

	if (sys_users) {
		for (i = 0; sys_users[i].name; i++) {
			if (file_info[n].uid != sys_users[i].id)
				continue;
			file_info[n].uid_i.name = sys_users[i].name;
			file_info[n].uid_i.namlen = sys_users[i].namlen;
		}
	}

	if (sys_groups) {
		for (i = 0; sys_groups[i].name; i++) {
			if (file_info[n].gid != sys_groups[i].id)
				continue;
			file_info[n].gid_i.name = sys_groups[i].name;
			file_info[n].gid_i.namlen = sys_groups[i].namlen;
		}
	}
}

/* Construct human readable sizes for all files in the current directory
 * and store them in the human_size struct field of the file_info struct. The
 * length of each human size is stored in the human_size.len field of the
 * same struct. */
static void
construct_human_sizes(void)
{
	const off_t ibase = xargs.si == 1 ? 1000 : 1024;
	const float base = (float)ibase;
	/* R: Ronnabyte, Q: Quettabyte. It's highly unlikely to have files of
	 * such huge sizes (and even less) in the near future, but anyway... */
	static const char *const u = "BKMGTPEZYRQ";
	static float mult_factor = 0;
	if (mult_factor == 0)
		mult_factor = 1.0f / base;

	filesn_t i = files;
	while (--i >= 0) {
		if (file_info[i].size < ibase) {
			const int ret = snprintf(file_info[i].human_size.str,
				MAX_HUMAN_SIZE, "%jd", (intmax_t)file_info[i].size);
			file_info[i].human_size.len = ret > 0 ? (size_t)ret : 0;
			file_info[i].human_size.unit = 'B';
			continue;
		}

		size_t n = 0;
		float s = (float)file_info[i].size;

		while (s >= base) {
			s = s * mult_factor; /* == (s = s / base), but faster */
			++n;
		}

		const int x = (int)s;
		/* If (s == 0 || s - (float)x == 0), then S has no reminder (zero).
		 * We don't want to print the reminder when it is zero. */
		const int ret =
			snprintf(file_info[i].human_size.str, MAX_HUMAN_SIZE, "%.*f",
				(s == 0.00f || s - (float)x == 0.00f) ? 0 : 2,
				(double)s);

		file_info[i].human_size.len = ret > 0 ? (size_t)ret : 0;
		/* Let's follow du(1) in using 'k' (lowercase) instead of 'K'
		 * (uppercase) when using powers of 1000 (--si). */
		file_info[i].human_size.unit = (xargs.si == 1 && u[n] == 'K')
			? 'k' : u[n];
	}
}

#define LIST_SCANNING_MSG "Scanning... "
static void
print_scanning_message(void)
{
	UNHIDE_CURSOR;
	fputs(LIST_SCANNING_MSG, stdout);
	fflush(stdout);
	if (xargs.list_and_quit != 1)
		HIDE_CURSOR;
}

static void
print_scanned_file(const char *name)
{
	printf("\r\x1b[%zuC\x1b[0K%s%s%s/", sizeof(LIST_SCANNING_MSG) - 1,
		di_c, name, df_c);
	fflush(stdout);
}

static void
erase_scanning_message(void)
{
	fputs("\r\x1b[0K", stdout);
	fflush(stdout);
}

/* List files in the current working directory (global variable 'path').
 * Unlike list_dir(), however, this function uses no color and runs
 * neither stat() nor count_dir(), which makes it quite faster. Return
 * zero on success and one on error */
static int
list_dir_light(void)
{
#ifdef LIST_SPEED_TEST
	clock_t start = clock();
#endif /* LIST_SPEED_TEST */

	struct dothidden_t *hidden_list =
		(conf.read_dothidden == 1 && conf.show_hidden == 0)
		? load_dothidden() : NULL;

	DIR *dir;
	struct dirent *ent;
	int reset_pager = 0;
	filesn_t excluded_files = 0;
	int close_dir = 1;
	int have_xattr = 0;

	/* Let's store information about the largest file in the list for the
	 * disk usage analyzer mode. */
	off_t largest_name_size = 0, total_size = 0;
	char *largest_name = (char *)NULL, *largest_color = (char *)NULL;

	if ((dir = opendir(workspaces[cur_ws].path)) == NULL) {
		xerror("%s: %s: %s\n", PROGRAM_NAME, workspaces[cur_ws].path,
			strerror(errno));
		close_dir = 0;
		goto END;
	}

#ifdef POSIX_FADV_SEQUENTIAL
	const int fd = dirfd(dir);
	posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
#endif /* POSIX_FADV_SEQUENTIAL */

	set_events_checker();

	errno = 0;
	longest.name_len = 0;
	filesn_t n = 0, count = 0;
	size_t total_dents = 0;

	file_info = xnmalloc(ENTRY_N + 2, sizeof(struct fileinfo));

	while ((ent = readdir(dir))) {
		const char *ename = ent->d_name;
		/* Skip self and parent directories */
		if (SELFORPARENT(ename))
			continue;

		/* Check .cfm.in and .cfm.out files for the autocommands function */
		if (checks.autocmd_files == 1 && *ename == '.')
			check_autocmd_file(ename + 1);

		/* Skip files according to a regex filter */
		if (checks.filter_name == 1) {
			if (regexec(&regex_exp, ename, 0, NULL, 0) == FUNC_SUCCESS) {
				if (filter.rev == 1) {
					excluded_files++;
					continue;
				}
			} else if (filter.rev == 0) {
				excluded_files++;
				continue;
			}
		}

		if (*ename == '.') {
			stats.hidden++;
			if (conf.show_hidden == 0) {
				excluded_files++;
				continue;
			}
		}

		if (hidden_list	&& check_dothidden(ename, &hidden_list) == 1) {
			stats.hidden++;
			excluded_files++;
			continue;
		}

#ifndef _DIRENT_HAVE_D_TYPE
		struct stat attr;
		if (lstat(ename, &attr) == -1)
			continue;
		if (conf.only_dirs == 1 && !S_ISDIR(attr.st_mode))
#else
		if (conf.only_dirs == 1 && ent->d_type != DT_DIR)
#endif /* !_DIRENT_HAVE_D_TYPE */
		{
			excluded_files++;
			continue;
		}

		/* Filter files according to file type */
		if (checks.filter_type == 1
#ifndef _DIRENT_HAVE_D_TYPE
		&& exclude_file_type_light((unsigned char)get_dt(attr.st_mode))
		== FUNC_SUCCESS)
#else
		&& exclude_file_type_light(ent->d_type) == FUNC_SUCCESS)
#endif /* !_DIRENT_HAVE_D_TYPE */
		{
			excluded_files++;
			continue;
		}

		if (count > ENTRY_N) {
			count = 0;
			total_dents = (size_t)n + ENTRY_N;
			file_info = xnrealloc(file_info, total_dents + 2,
				sizeof(struct fileinfo));
		}

		init_fileinfo(n);

		file_info[n].utf8 = is_utf8_name(ename, &file_info[n].bytes);
		file_info[n].name = xnmalloc(file_info[n].bytes + 1, sizeof(char));
		memcpy(file_info[n].name, ename, file_info[n].bytes + 1);
		file_info[n].len = (file_info[n].utf8 == 0)
			? file_info[n].bytes : wc_xstrlen(ename);

		/* ################  */
#ifndef _DIRENT_HAVE_D_TYPE
		file_info[n].type = get_dt(attr.st_mode);
#else
		/* If type is unknown, we might be facing a file system not
		 * supporting d_type, for example, loop devices. In this case,
		 * try falling back to lstat(2). */
		if (ent->d_type == DT_UNKNOWN) {
			struct stat a;
			if (lstat(ename, &a) == -1)
				continue;
			file_info[n].type = get_dt(a.st_mode);
		} else {
			file_info[n].type = ent->d_type;
		}
#endif /* !_DIRENT_HAVE_D_TYPE */

		file_info[n].dir = (file_info[n].type == DT_DIR);
		file_info[n].symlink = (file_info[n].type == DT_LNK);
		file_info[n].inode = ent->d_ino;

		if (checks.scanning == 1 && file_info[n].dir == 1)
			print_scanned_file(file_info[n].name);

		switch (file_info[n].type) {
		case DT_DIR:
#ifndef _NO_ICONS
			if (conf.icons == 1) {
				file_info[n].icon = DEF_DIR_ICON;
				file_info[n].icon_color = DEF_DIR_ICON_COLOR;
				/* If set from the color scheme file */
				if (*dir_ico_c)
					file_info[n].icon_color = dir_ico_c;
			}
#endif /* !_NO_ICONS */

			stats.dir++;
			if (conf.files_counter == 1)
				file_info[n].filesn = count_dir(ename, NO_CPOP) - 2;
			else
				file_info[n].filesn = 1;

			if (file_info[n].filesn > 0) {
				file_info[n].color = di_c;
			} else if (file_info[n].filesn == 0) {
				file_info[n].color = ed_c;
			} else {
				file_info[n].color = nd_c;
#ifndef _NO_ICONS
				file_info[n].icon = ICON_LOCK;
				file_info[n].icon_color = YELLOW;
#endif /* !_NO_ICONS */
			}

			break;

		case DT_LNK:
#ifndef _NO_ICONS
			file_info[n].icon = ICON_LINK;
#endif /* !_NO_ICONS */
			file_info[n].color = ln_c;
			stats.link++;
			break;

		case DT_REG: file_info[n].color = fi_c; stats.reg++; break;
		case DT_SOCK: file_info[n].color = so_c; stats.socket++; break;
		case DT_FIFO: file_info[n].color = pi_c; stats.fifo++; break;
		case DT_BLK: file_info[n].color = bd_c; stats.block_dev++; break;
		case DT_CHR: file_info[n].color = cd_c; stats.char_dev++; break;
#ifndef _BE_POSIX
# ifdef SOLARIS_DOORS
		case DT_DOOR: file_info[n].color = oo_c; stats.door++; break;
		case DT_PORT: file_info[n].color = oo_c; stats.port++; break;
# endif /* SOLARIS_DOORS */
# ifdef S_ARCH1
		case DT_ARCH1: file_info[n].color = fi_c; stats.arch1++; break;
		case DT_ARCH2: file_info[n].color = fi_c; stats.arch2++; break;
# endif /* S_ARCH1 */
# ifdef S_IFWHT
		case DT_WHT: file_info[n].color = fi_c; stats.whiteout++; break;
# endif /* DT_WHT */
#endif /* !_BE_POSIX */
		case DT_UNKNOWN: file_info[n].color = no_c; stats.unknown++; break;
		default: file_info[n].color = df_c; break;
		}

#ifndef _NO_ICONS
		if (checks.icons_use_file_color == 1)
			file_info[n].icon_color = file_info[n].color;
#endif /* !_NO_ICONS */

		if (conf.long_view == 1) {
#ifndef _DIRENT_HAVE_D_TYPE
			set_long_attribs(n, &attr);
#else
			struct stat a;
			if (lstat(file_info[n].name, &a) != -1)
				set_long_attribs(n, &a);
			else
				file_info[n].stat_err = 1;
#endif /* !_DIRENT_HAVE_D_TYPE */
			if (prop_fields.ids == PROP_ID_NAME && file_info[n].stat_err == 0)
				get_id_names(n);
		}

		if (xargs.disk_usage_analyzer == 1) {
			get_largest_file_info(n, &largest_name_size, &largest_name,
				&largest_color, &total_size);
		}

		n++;
		if (n > FILESN_MAX - 1) {
			err('w', PRINT_PROMPT, _("%s: Integer overflow detected "
				"(showing only %jd files)\n"), PROGRAM_NAME, (intmax_t)n);
			break;
		}

		count++;
	}

	file_info[n].name = (char *)NULL;
	files = n;

	if (hidden_list)
		free_dothidden(&hidden_list);

	if (checks.scanning == 1)
		erase_scanning_message();

	if (n == 0) {
		printf("%s. ..%s\n", di_c, df_c);
		free(file_info);
		goto END;
	}

	const int pad = (conf.max_files != UNSET && files > (filesn_t)conf.max_files)
		? DIGINUM(conf.max_files) : DIGINUM(files);

	if (conf.sort != SNONE)
		ENTSORT(file_info, (size_t)n, entrycmp);

	size_t counter = 0;
	size_t columns_n = 1;

	/* Get the longest file name */
	if (conf.columned == 1 || conf.long_view == 1)
		get_longest_filename(n, (size_t)pad);

				/* ########################
				 * #    LONG VIEW MODE    #
				 * ######################## */

	if (conf.long_view == 1) {
		if (prop_fields.size == PROP_SIZE_HUMAN)
			construct_human_sizes();
		print_long_mode(&counter, &reset_pager, pad, have_xattr);
		goto END;
	}

				/* ########################
				 * #   NORMAL VIEW MODE   #
				 * ######################## */

	/* Get possible amount of columns for the dirlist screen */
	columns_n = conf.columned == 0 ? 1 : get_columns();

	if (conf.listing_mode == VERTLIST) /* ls(1)-like listing */
		list_files_vertical(&counter, &reset_pager, pad, columns_n);
	else
		list_files_horizontal(&counter, &reset_pager, pad, columns_n);

END:
	exit_code =
		post_listing(close_dir == 1 ? dir : NULL, reset_pager, excluded_files);

#ifndef ST_BTIME_LIGHT
	if (conf.long_view == 1 && prop_fields.time == PROP_TIME_BIRTH)
		print_reload_msg("Long view: Birth time not available in light "
			"mode. Using %smodification time%s.\n", BOLD, NC);
#endif /* !ST_BTIME_LIGHT */

	if (xargs.disk_usage_analyzer == 1 && conf.long_view == 1
	&& conf.full_dir_size == 1) {
		print_analysis_stats(total_size, largest_name_size,
			largest_color, largest_name);
	}

#ifdef LIST_SPEED_TEST
	clock_t end = clock();
	printf("list_dir time: %f\n", (double)(end - start) / CLOCKS_PER_SEC);
#endif /* LIST_SPEED_TEST */

	return exit_code;
}

/* Check whether the file in the device DEV with inode INO is selected.
 * Used to mark selected files in the files list. */
static int
check_seltag(const dev_t dev, const ino_t ino, const nlink_t links,
	const filesn_t index)
{
	if (sel_n == 0 || !sel_devino)
		return 0;

	filesn_t j = (filesn_t)sel_n;
	while (--j >= 0) {
		if (sel_devino[j].dev != dev || sel_devino[j].ino != ino)
			continue;
		/* Only check hardlinks in case of regular files */
		if (file_info[index].type != DT_DIR && links > 1) {
			const char *p = strrchr(sel_elements[j].name, '/');
			if (!p || !*(++p))
				continue;
			if (*p == *file_info[index].name
			&& strcmp(p, file_info[index].name) == 0)
				return 1;
		} else {
			return 1;
		}
	}

	return 0;
}

/* Get the color of a link target NAME, whose file attributes are ATTR,
 * and write the result into the file_info array at index I. */
static inline void
get_link_target_color(const char *name, const struct stat *attr,
	const filesn_t i)
{
	switch (attr->st_mode & S_IFMT) {
	case S_IFSOCK: file_info[i].color = so_c; break;
	case S_IFIFO:  file_info[i].color = pi_c; break;
	case S_IFBLK:  file_info[i].color = bd_c; break;
	case S_IFCHR:  file_info[i].color = cd_c; break;
#ifndef _BE_POSIX
# ifdef SOLARIS_DOORS
	case S_IFDOOR: file_info[i].color = oo_c; break;
	case S_IFPORT: file_info[i].color = oo_c; break;
# endif /* SOLARIS_DOORS */
# ifdef S_ARCH1
	case S_ARCH1: file_info[i].color = fi_c; break;
	case S_ARCH2: file_info[i].color = fi_c; break;
# endif /* S_ARCH1 */
# ifdef S_IFWHT
	case S_IFWHT: file_info[i].color = fi_c; break;
# endif /* S_IFWHT */
#endif /* !_BE_POSIX */
	case S_IFREG: {
		size_t clen = 0;
		char *color = get_regfile_color(name, attr, &clen);

		if (!color) {
			file_info[i].color = fi_c;
			return;
		}

		if (clen > 0) { /* We have an extension color */
			file_info[i].ext_color = savestring(color, clen);
			file_info[i].color = file_info[i].ext_color;
		} else {
			file_info[i].color = color;
		}
		}
		break;

	default: file_info[i].color = df_c; break;
	}
}

static inline void
check_extra_file_types(mode_t *mode, const struct stat *a)
{
	/* If all the below macros are originally undefined, they all expand to
	 * zero, in which case S is never used. Let's avoid a compiler warning. */
	UNUSED(a);

	if (S_TYPEISMQ(a))
		*mode = DT_MQ;
	else if (S_TYPEISSEM(a))
		*mode = DT_SEM;
	else if (S_TYPEISSHM(a))
		*mode = DT_SHM;
	else if (S_TYPEISTMO(a))
		*mode = DT_TPO;
}

static inline void
load_file_gral_info(const struct stat *a, const filesn_t n)
{
	switch (a->st_mode & S_IFMT) {
	case S_IFREG: file_info[n].type = DT_REG; stats.reg++; break;
	case S_IFDIR: file_info[n].type = DT_DIR; stats.dir++; break;
	case S_IFLNK: file_info[n].type = DT_LNK; stats.link++; break;
	case S_IFIFO: file_info[n].type = DT_FIFO; stats.fifo++; break;
	case S_IFSOCK: file_info[n].type = DT_SOCK; stats.socket++; break;
	case S_IFBLK: file_info[n].type = DT_BLK; stats.block_dev++; break;
	case S_IFCHR: file_info[n].type = DT_CHR; stats.char_dev++; break;
#ifndef _BE_POSIX
# ifdef SOLARIS_DOORS
	case S_IFDOOR: file_info[n].type = DT_DOOR; stats.door++; break;
	case S_IFPORT: file_info[n].type = DT_PORT; stats.port++; break;
# endif /* SOLARIS_DOORS */
# ifdef S_ARCH1
	case S_ARCH1: file_info[n].type = DT_ARCH1; stats.arch1++; break;
	case S_ARCH2: file_info[n].type = DT_ARCH2; stats.arch2++; break;
# endif /* S_ARCH1 */
# ifdef S_IFWHT
	case S_IFWHT: file_info[n].type = DT_WHT; stats.whiteout++; break;
# endif /* S_IFWHT */
#endif /* !_BE_POSIX */
	default: file_info[n].type = DT_UNKNOWN; stats.unknown++; break;
	}

	check_extra_file_types(&file_info[n].type, a);

	file_info[n].blocks = a->st_blocks;
	file_info[n].inode = a->st_ino;
	file_info[n].linkn = a->st_nlink;
	file_info[n].mode = a->st_mode;
	file_info[n].sel = check_seltag(a->st_dev, a->st_ino, a->st_nlink, n);
	file_info[n].size = FILE_TYPE_NON_ZERO_SIZE(a->st_mode) ? FILE_SIZE(*a) : 0;
	file_info[n].uid = a->st_uid;
	file_info[n].gid = a->st_gid;

	if (checks.id_names == 1)
		get_id_names(n);

#if defined(LINUX_FILE_XATTRS)
	if (file_info[n].type != DT_LNK
	&& (checks.xattr == 1 || conf.check_cap == 1)
	&& listxattr(file_info[n].name, NULL, 0) > 0)
		file_info[n].xattr = 1;
#endif /* LINUX_FILE_XATTRS */

	time_t btime = (time_t)-1;
	if (checks.birthtime == 1) {
#if defined(ST_BTIME)
# ifdef LINUX_STATX
		struct statx attx;
		if (statx(AT_FDCWD, file_info[n].name, AT_SYMLINK_NOFOLLOW,
		STATX_BTIME, &attx) == 0 && (attx.stx_mask & STATX_BTIME))
			btime = attx.ST_BTIME.tv_sec;
# elif defined(__sun)
		struct timespec birthtim = get_birthtime(file_info[n].name);
		btime = birthtim.tv_sec;
# else
		btime = a->ST_BTIME.tv_sec;
# endif /* LINUX_STATX */
#endif /* ST_BTIME */
	}

	if (conf.long_view == 1) {
		if (checks.time_follows_sort == 1) {
			switch (conf.sort) {
			case SATIME: file_info[n].ltime = a->st_atime; break;
			case SBTIME: file_info[n].ltime = btime; break;
			case SCTIME: file_info[n].ltime = a->st_ctime; break;
			case SMTIME: /* fallthrough */
			default: file_info[n].ltime = a->st_mtime; break;
			}
		} else {
			switch (prop_fields.time) {
			case PROP_TIME_ACCESS: file_info[n].ltime = a->st_atime; break;
			case PROP_TIME_CHANGE: file_info[n].ltime = a->st_ctime; break;
			case PROP_TIME_MOD: file_info[n].ltime = a->st_mtime; break;
			case PROP_TIME_BIRTH: file_info[n].ltime = btime; break;
			default: file_info[n].ltime = a->st_mtime; break;
			}
		}
	}

	switch (conf.sort) {
	case SATIME: file_info[n].time = a->st_atime; break;
	case SBTIME: file_info[n].time = btime; break;
	case SCTIME: file_info[n].time = a->st_ctime; break;
	case SMTIME: file_info[n].time = a->st_mtime; break;
	default: file_info[n].time = 0; break;
	}
}

static inline void
load_dir_info(const struct stat *a, const filesn_t n)
{
#ifndef _NO_ICONS
	if (conf.icons == 1) {
		get_dir_icon(n);

		if (*dir_ico_c)	/* If set from the color scheme file */
			file_info[n].icon_color = dir_ico_c;
	}
#endif /* !_NO_ICONS */

	const int daccess = (a &&
		check_file_access(a->st_mode, a->st_uid, a->st_gid) == 1);

	file_info[n].filesn = (checks.files_counter == 1
		? (count_dir(file_info[n].name, NO_CPOP) - 2) : 1);

	if (daccess == 0 || file_info[n].filesn < 0) {
		file_info[n].color = nd_c;
#ifndef _NO_ICONS
		file_info[n].icon = ICON_LOCK;
		file_info[n].icon_color = YELLOW;
#endif /* !_NO_ICONS */
	} else {
		file_info[n].color = a ? ((a->st_mode & S_ISVTX)
			? ((a->st_mode & S_IWOTH) ? tw_c : st_c)
			: ((a->st_mode & S_IWOTH) ? ow_c
			: (file_info[n].filesn == 0 ? ed_c : di_c)))
			: df_c;
	}

	/* Let's gather some file statistics based on the file type color */
	if (file_info[n].color == tw_c) {
		stats.other_writable++;
		stats.sticky++;
	} else if (file_info[n].color == ow_c) {
		stats.other_writable++;
	} else
		if (file_info[n].color == st_c) {
			stats.sticky++;
	}
}

static inline void
load_link_info(const int fd, const filesn_t n)
{
#ifndef _NO_ICONS
	file_info[n].icon = ICON_LINK;
#endif // !_NO_ICONS

	if (conf.follow_symlinks == 0) {
		file_info[n].color = ln_c;
		return;
	}

	struct stat a;
	if (fstatat(fd, file_info[n].name, &a, 0) == -1) {
		file_info[n].color = or_c;
		file_info[n].xattr = 0;
		stats.broken_link++;
		return;
	}

	/* We only need the symlink target name provided the target
	 * is not a directory, because get_link_target_color() will
	 * check the file name extension. get_dir_color() only needs
	 * this name to run count_dir(), but we have already executed
	 * this function. */
	static char tmp[PATH_MAX + 1]; *tmp = '\0';
	const ssize_t ret =
		(conf.color_lnk_as_target == 1 && !S_ISDIR(a.st_mode))
		? readlinkat(XAT_FDCWD, file_info[n].name, tmp, sizeof(tmp) - 1)
		: 0;
	if (ret > 0)
		tmp[ret] = '\0';

	const char *lname = *tmp ? tmp : file_info[n].name;

	if (S_ISDIR(a.st_mode)) {
		file_info[n].dir = 1;
		file_info[n].filesn = conf.files_counter == 1
			? (count_dir(file_info[n].name, NO_CPOP) - 2) : 1;

		const filesn_t dfiles = (conf.files_counter == 1)
			? (file_info[n].filesn == 2 ? 3
			: file_info[n].filesn) : 3; /* 3 == populated */

		/* DFILES is negative only if count_dir() failed, which in
		 * this case only means EACCESS error. */
		file_info[n].color = conf.color_lnk_as_target == 1
			? ((dfiles < 0 || check_file_access(a.st_mode,
			a.st_uid, a.st_gid) == 0) ? nd_c
			: get_dir_color(lname, a.st_mode, a.st_nlink,
			dfiles)) : ln_c;
	} else {
		if (conf.color_lnk_as_target == 1)
			get_link_target_color(lname, &a, n);
		else
			file_info[n].color = ln_c;
	}
}

static inline void
load_regfile_info(const struct stat *a, const filesn_t n)
{
#ifdef LINUX_FILE_CAPS
	cap_t cap;
#endif /* !LINUX_FILE_CAPS */

	/* Do not perform the access check if the user is root. */
	if (user.uid != 0 && a
	&& check_file_access(a->st_mode, a->st_uid, a->st_gid) == 0) {
#ifndef _NO_ICONS
		file_info[n].icon = ICON_LOCK;
		file_info[n].icon_color = YELLOW;
#endif /* !_NO_ICONS */
		file_info[n].color = nf_c;
	} else if (a && (a->st_mode & S_ISUID)) {
		file_info[n].exec = 1;
		stats.exec++;
		stats.suid++;
		file_info[n].color = su_c;
	} else if (a && (a->st_mode & S_ISGID)) {
		file_info[n].exec = 1;
		stats.exec++;
		stats.sgid++;
		file_info[n].color = sg_c;
	}

#ifdef LINUX_FILE_CAPS
	/* Capabilities are stored by the system as extended attributes.
	 * No xattrs, no caps. */
	else if (file_info[n].xattr == 1
	&& (cap = cap_get_file(file_info[n].name))) {
		file_info[n].color = ca_c;
		stats.caps++;
		cap_free(cap);
		if (a && IS_EXEC(a)) {
			file_info[n].exec = 1;
			stats.exec++;
		}
	}
#endif /* LINUX_FILE_CAPS */

	else if (a && IS_EXEC(a)) {
		file_info[n].exec = 1;
		stats.exec++;
		file_info[n].color = file_info[n].size == 0 ? ee_c : ex_c;
	} else if (file_info[n].linkn > 1) { /* Multi-hardlink */
		file_info[n].color = mh_c;
		stats.multi_link++;
	} else if (file_info[n].size == 0) {
		file_info[n].color = ef_c;
	} else { /* Regular file */
		file_info[n].color = fi_c;
	}

#ifndef _NO_ICONS
	if (file_info[n].exec == 1)
		file_info[n].icon = ICON_EXEC;
#endif /* !_NO_ICONS */

	/* Try temp and extension color only provided the file is a non-empty
	 * regular file. */
	const int override_color = (file_info[n].color == fi_c);

	if (override_color == 1
	&& IS_TEMP_FILE(file_info[n].name, file_info[n].bytes)) {
		file_info[n].color = bk_c;
		return;
	}

#ifndef _NO_ICONS
	/* The icons check precedence order is this:
	 * 1. filename or filename.extension
	 * 2. extension
	 * 3. file type */
	/* Check icons for specific file names */
	const int name_icon_found = (conf.icons == 1)
		? get_name_icon(n) : 0;
#endif /* !_NO_ICONS */

	/* Check file extension */
	char *ext = (override_color == 1 && conf.check_ext == 1)
		? strrchr(file_info[n].name, '.') : (char *)NULL;

	if (!ext || ext == file_info[n].name || !*(ext + 1))
		return;

	file_info[n].ext_name = ext;
#ifndef _NO_ICONS
	if (name_icon_found == 0 && conf.icons == 1)
		get_ext_icon(ext, n);
#endif /* !_NO_ICONS */

	size_t color_len = 0;
	const char *extcolor = get_ext_color(ext, &color_len);
	if (!extcolor)
		return;

	char *t = xnmalloc(color_len + 4, sizeof(char));
	*t = '\x1b'; t[1] = '[';
	memcpy(t + 2, extcolor, color_len);
	t[color_len + 2] = 'm';
	t[color_len + 3] = '\0';
	file_info[n].ext_color = file_info[n].color = t;
}

static int
vt_stat(const int fd, char *restrict path, struct stat *attr)
{
	static char buf[PATH_MAX + 1];
	*buf = '\0';

	if (xreadlink(fd, path, buf, sizeof(buf)) == -1)
		return (-1);

	if (!*buf || fstatat(fd, buf, attr, AT_SYMLINK_NOFOLLOW) == -1)
		return (-1);

	return 0;
}

/* List files in the current working directory. Uses file type colors
 * and columns. Return 0 on success or 1 on error. */
int
list_dir(void)
{
#ifdef LIST_SPEED_TEST
	clock_t start = clock();
#endif /* LIST_SPEED_TEST */

	if (conf.clear_screen > 0)
		{ CLEAR; fflush(stdout); }

	/* Hide the cursor to minimize flickering: it will be unhidden immediately
	 * before printing the next prompt (prompt.c) */
	if (xargs.list_and_quit != 1)
		HIDE_CURSOR;

	if (autocmds_n > 0 && dir_changed == 1) {
		if (autocmd_set == 1)
			revert_autocmd_opts();
		check_autocmds();
	}

	if (dir_changed == 1 && dir_out == 1) {
		run_dir_cmd(AUTOCMD_DIR_OUT);
		dir_out = 0;
	}

	if (conf.clear_screen > 0) {
		/* For some reason we need to clear the screen twice to prevent
		 * a garbage first line when scrolling up */
		CLEAR; fflush(stdout);
	}

	set_pager_view();
	get_term_size();

	virtual_dir =
		(stdin_tmp_dir && strcmp(stdin_tmp_dir, workspaces[cur_ws].path) == 0);

	/* Reset the stats struct */
	stats = (struct stats_t){0};
	init_checks_struct();

	if (checks.scanning == 1)
		print_scanning_message();

	if (conf.long_view == 1)
		props_now = time(NULL);

	if (conf.light_mode == 1)
		return list_dir_light();

	struct dothidden_t *hidden_list =
		(conf.read_dothidden == 1 && conf.show_hidden == 0)
		? load_dothidden() : NULL;

	DIR *dir;
	struct dirent *ent;
	struct stat attr;
	int reset_pager = 0;
	filesn_t excluded_files = 0;
	int close_dir = 1;
	int have_xattr = 0;

	/* Let's store information about the largest file in the list for the
	 * disk usage analyzer mode. */
	off_t largest_name_size = 0, total_size = 0;
	char *largest_name = (char *)NULL, *largest_color = (char *)NULL;

	if ((dir = opendir(workspaces[cur_ws].path)) == NULL) {
		xerror("%s: %s: %s\n", PROGRAM_NAME, workspaces[cur_ws].path,
			strerror(errno));
		close_dir = 0;
		goto END;
	}

	set_events_checker();

	const int fd = dirfd(dir);

#ifdef POSIX_FADV_SEQUENTIAL
	/* A hint to the kernel to optimize current dir for reading */
	posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
#endif /* POSIX_FADV_SEQUENTIAL */

		/* ##########################################
		 * #    GATHER AND STORE FILE INFORMATION   #
		 * ########################################## */

	errno = 0;
	longest.name_len = 0;
	filesn_t n = 0, count = 0;
	size_t total_dents = 0;

	file_info = xnmalloc(ENTRY_N + 2, sizeof(struct fileinfo));

	const int stat_flag =
		(conf.follow_symlinks == 1 && conf.long_view == 1
		&& conf.follow_symlinks_long == 1) ? 0 : AT_SYMLINK_NOFOLLOW;

	while ((ent = readdir(dir))) {
		const char *ename = ent->d_name;
		/* Skip self and parent directories */
		if (SELFORPARENT(ename))
			continue;

		/* Check .cfm.in and .cfm.out files for the autocommands function */
		if (checks.autocmd_files == 1 && *ename == '.')
			check_autocmd_file(ename + 1);

		/* Filter files according to a regex filter */
		if (checks.filter_name == 1) {
			if (regexec(&regex_exp, ename, 0, NULL, 0) == FUNC_SUCCESS) {
				if (filter.rev == 1) {
					excluded_files++;
					continue;
				}
			} else if (filter.rev == 0) {
				excluded_files++;
				continue;
			}
		}

		if (*ename == '.') {
			stats.hidden++;
			if (conf.show_hidden == 0) {
				excluded_files++;
				continue;
			}
		}

		if (hidden_list	&& check_dothidden(ename, &hidden_list) == 1) {
			stats.hidden++;
			excluded_files++;
			continue;
		}

		init_fileinfo(n);

		const int stat_ok =
			((virtual_dir == 1 ? vt_stat(fd, ent->d_name, &attr)
			: fstatat(fd, ename, &attr, stat_flag)) == 0);

		if (stat_ok == 0) {
			if (virtual_dir == 1)
				continue;
			stats.unstat++;
			file_info[n].stat_err = 1;
		}

		/* Filter files according to file type */
		if (checks.filter_type == 1 && stat_ok == 1
		&& exclude_file_type(attr.st_mode, attr.st_nlink) == FUNC_SUCCESS) {
			/* Decrease counters: the file won't be displayed */
			if (*ename == '.' && stats.hidden > 0)
				stats.hidden--;
			if (stat_ok == 0 && stats.unstat > 0)
				stats.unstat--;
			excluded_files++;
			continue;
		}

		if (conf.only_dirs == 1 && stat_ok == 1 && !S_ISDIR(attr.st_mode)
		&& (conf.follow_symlinks == 0 || !S_ISLNK(attr.st_mode)
		|| get_link_ref(ename) != S_IFDIR)) {
			excluded_files++;
			continue;
		}

		if (count > ENTRY_N) {
			count = 0;
			total_dents = (size_t)n + ENTRY_N;
			file_info = xnrealloc(file_info, total_dents + 2,
				sizeof(struct fileinfo));
		}

		/* Both is_utf8_name() and wc_xstrlen() calculate the number of
		 * columns needed to display the current file name on the screen
		 * (the former for ASCII names, where 1 char = 1 byte = 1 column, and
		 * the latter for UTF-8 names, i.e. containing at least one non-ASCII
		 * character).
		 * Now, since is_utf8_name() is ~8 times faster than wc_xstrlen()
		 * (10,000 entries, optimization O3), we only run wc_xstrlen() in
		 * case of an UTF-8 name.
		 * However, since is_utf8_name() will be executed anyway, this ends
		 * up being actually slower whenever the current directory contains
		 * more UTF-8 than ASCII names. The assumption here is that ASCII
		 * names are far more common than UTF-8 names. */
		file_info[n].utf8 = is_utf8_name(ename, &file_info[n].bytes);

		file_info[n].name = xnmalloc(file_info[n].bytes + 1, sizeof(char));
		memcpy(file_info[n].name, ename, file_info[n].bytes + 1);

		/* Columns needed to display file name */
		file_info[n].len = file_info[n].utf8 == 0
			? file_info[n].bytes : wc_xstrlen(ename);

		if (stat_ok == 1) {
			load_file_gral_info(&attr, n);
			if (file_info[n].xattr == 1)
				have_xattr = 1;
		} else {
			file_info[n].type = DT_UNKNOWN;
			stats.unknown++;
		}

		file_info[n].dir = (file_info[n].type == DT_DIR);
		file_info[n].symlink = (file_info[n].type == DT_LNK);

		switch (file_info[n].type) {
		case DT_DIR: load_dir_info(stat_ok == 1 ? &attr : NULL, n); break;
		case DT_LNK: load_link_info(fd, n); break;
		case DT_REG: load_regfile_info(stat_ok == 1 ? &attr : NULL, n); break;

		/* For the time being, we have no specific colors for DT_ARCH1,
		 * DT_ARCH2, and DT_WHT. */
		case DT_SOCK: file_info[n].color = so_c; break;
		case DT_FIFO: file_info[n].color = pi_c; break;
		case DT_BLK: file_info[n].color = bd_c; break;
		case DT_CHR: file_info[n].color = cd_c; break;
#ifdef SOLARIS_DOORS
		case DT_DOOR: file_info[n].color = oo_c; break;
		case DT_PORT: file_info[n].color = oo_c; break;
#endif /* SOLARIS_DOORS */
		case DT_UNKNOWN: file_info[n].color = no_c; break;
		default: file_info[n].color = df_c; break;
		}

		if (checks.scanning == 1 && file_info[n].dir == 1)
			print_scanned_file(file_info[n].name);

#ifndef _NO_ICONS
		if (checks.icons_use_file_color == 1)
			file_info[n].icon_color = file_info[n].color;
#endif /* !_NO_ICONS */
		if (conf.long_view == 1 && stat_ok == 1)
			set_long_attribs(n, &attr);

		if (xargs.disk_usage_analyzer == 1) {
			get_largest_file_info(n, &largest_name_size, &largest_name,
				&largest_color, &total_size);
		}

		n++;
		if (n > FILESN_MAX - 1) {
			err('w', PRINT_PROMPT, _("%s: Integer overflow detected "
				"(showing only %jd files)\n"), PROGRAM_NAME, (intmax_t)n);
			break;
		}

		count++;
	}

	if (hidden_list)
		free_dothidden(&hidden_list);

	/* Since we allocate memory by chunks, we might have allocated more
	 * than needed. Reallocate only actually used memory.
	 * NOTE: Haven't been able to measure any memory usage difference
	 * running this code. */
/*	filesn_t tdents = total_dents > 0 ? (filesn_t)total_dents : ENTRY_N + 2;
	if (tdents > n)
		file_info =
			xnrealloc(file_info, (size_t)n + 1, sizeof(struct fileinfo)); */

	file_info[n].name = (char *)NULL;
	files = n;

	if (checks.scanning == 1)
		erase_scanning_message();

	if (n == 0) {
		printf("%s. ..%s\n", di_c, df_c);
		free(file_info);
		goto END;
	}

	const int pad = (conf.max_files != UNSET && files > (filesn_t)conf.max_files)
		? DIGINUM(conf.max_files) : DIGINUM(files);

		/* #############################################
		 * #    SORT FILES ACCORDING TO SORT METHOD    #
		 * ############################################# */

	if (conf.sort != SNONE)
		ENTSORT(file_info, (size_t)n, entrycmp);

		/* ##########################################
		 * #    GET INFO TO PRINT COLUMNED OUTPUT   #
		 * ########################################## */

	size_t counter = 0;
	size_t columns_n = 1;

	/* Get the longest file name */
	if (conf.columned == 1 || conf.long_view == 1)
		get_longest_filename(n, (size_t)pad);

				/* ########################
				 * #    LONG VIEW MODE    #
				 * ######################## */

	if (conf.long_view == 1) {
		if (prop_fields.size == PROP_SIZE_HUMAN)
			construct_human_sizes();
		print_long_mode(&counter, &reset_pager, pad, have_xattr);
		goto END;
	}

				/* ########################
				 * #   NORMAL VIEW MODE   #
				 * ######################## */

	/* Get amount of columns needed to print files in CWD  */
	columns_n = conf.columned == 0 ? 1 : get_columns();

	if (conf.listing_mode == VERTLIST) /* ls(1) like listing */
		list_files_vertical(&counter, &reset_pager, pad, columns_n);
	else
		list_files_horizontal(&counter, &reset_pager, pad, columns_n);

				/* #########################
				 * #   POST LISTING STUFF  #
				 * ######################### */

END:
	exit_code =
		post_listing(close_dir == 1 ? dir : NULL, reset_pager, excluded_files);

	if (xargs.disk_usage_analyzer == 1 && conf.long_view == 1
	&& conf.full_dir_size == 1) {
		print_analysis_stats(total_size, largest_name_size,
			largest_color, largest_name);
	}

#ifdef LIST_SPEED_TEST
	clock_t end = clock();
	printf("list_dir time: %f\n", (double)(end - start) / CLOCKS_PER_SEC);
#endif /* LIST_SPEED_TEST */

	return exit_code;
}

void
free_dirlist(void)
{
	if (!file_info || files == 0)
		return;

	filesn_t i = files;
	while (--i >= 0) {
		free(file_info[i].name);
		free(file_info[i].ext_color);
	}

	free(file_info);
	file_info = (struct fileinfo *)NULL;
}

void
reload_dirlist(void)
{
#ifdef RUN_CMD
	if (cmd_line_cmd)
		return;
#endif /* RUN_CMD */

	free_dirlist();
	const int bk = exit_code;
	list_dir();
	exit_code = bk;
}

void
refresh_screen(void)
{
	if (conf.autols == 0) {
		CLEAR;
		return;
	}

	int bk = conf.clear_screen;
	conf.clear_screen = 1;
	reload_dirlist();
	conf.clear_screen = bk;
}
