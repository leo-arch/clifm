/* listing.c -- functions controlling what is listed on the screen */

/*
 * This file is part of CliFM
 *
 * Copyright (C) 2016-2023, L. Abramovich <leo.clifm@outlook.com>
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
#if defined(BSD_KQUEUE)
# include <unistd.h> /* open(2) */
#endif
#if defined(__linux__)
# include <sys/capability.h>
#endif
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#if defined(__OpenBSD__)
# include <strings.h>
# include <inttypes.h> /* uintmax_t */
#endif

#if defined(_LINUX_XATTR)
# include <sys/xattr.h>
#endif

#include <limits.h> /* INT_MAX */

#if defined(_LIST_SPEED)
# include <time.h>
#endif

#if defined(TOURBIN_QSORT)
# include "qsort.h"
# define ENTLESS(i, j) (entrycmp(file_info + (i), file_info + (j)) < 0)
# define ENTSWAP(i, j) (swap_ent((i), (j)))
# define ENTSORT(file_info, n, entrycmp) QSORT((n), ENTLESS, ENTSWAP)
#else
# define ENTSORT(file_info, n, entrycmp) qsort((file_info), (n), sizeof(*(file_info)), (entrycmp))
#endif /* TOURBIN_QSORT */

#include "aux.h"
#include "colors.h"
#include "messages.h"
#include "misc.h"
#include "properties.h"
#include "sort.h"
#include "checks.h"
#include "exec.h"
#include "autocmds.h"
#include "sanitize.h"

#ifndef _NO_ICONS
# include "icons.h"
#endif

/* In case we want to try some faster printf implementation */
/*#if defined(_PALAND_PRINTF)
# include "printf.h"
# define xprintf printf_
#else
# define xprintf printf
#endif // _PALAND_PRINTF */
#define xprintf printf

/* Macros for run_dir_cmd function */
#define DIR_IN  0
#define DIR_OUT 1
#define DIR_IN_NAME  ".cfm.in"
#define DIR_OUT_NAME ".cfm.out"

/* Amount of digits of the files counter of the longest directory */
static size_t longest_fc = 0;
static int pager_bk = 0;

/* Struct to store information about trimmed file names. Used only when
 * Unicode is disabled */
struct trim_t {
	int state;
	int a; /* trimmed char */
	int b; /* char next to trimmed char */
	int pad;
	size_t len; /* Lenght of file name before trimming */
};

static struct trim_t trim;

/* Struct to store information about file names to be trimmed (Unicode) */
struct wtrim_t {
	char *wname; /* Address to store file name with replaced control chars */
	int type; /* Truncation type: with or without file extension. */
	int diff; /* */
};

/* A buffer to store file names to be displayed */
static char name_buf[NAME_MAX * sizeof(wchar_t)];

#if !defined(_NO_ICONS)
static void
set_icon_names_hashes(void)
{
	int i = (int)(sizeof(icon_filenames) / sizeof(struct icons_t));
	name_icons_hashes = (size_t *)xnmalloc((size_t)i + 1, sizeof(size_t));

	while (--i >= 0)
		name_icons_hashes[i] = hashme(icon_filenames[i].name, 0);
}

static void
set_dir_names_hashes(void)
{
	int i = (int)(sizeof(icon_dirnames) / sizeof(struct icons_t));
	dir_icons_hashes = (size_t *)xnmalloc((size_t)i + 1, sizeof(size_t));

	while (--i >= 0)
		dir_icons_hashes[i] = hashme(icon_dirnames[i].name, 0);
}

static void
set_ext_names_hashes(void)
{
	int i = (int)(sizeof(icon_ext) / sizeof(struct icons_t));
	ext_icons_hashes = (size_t *)xnmalloc((size_t)i + 1,  sizeof(size_t));

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
#endif // !_NO_ICONS

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

/* Set the color of the dividing line: DL, is the color code is set,
 * or the color of the current workspace if not */
static void
set_div_line_color(void)
{
	if (*dl_c) {
		fputs(dl_c, stdout);
		return;
	}

	/* If div line color isn't set, use the current workspace color */
	switch (cur_ws) {
	case 0: fputs(*ws1_c ? ws1_c : DEF_DL_C, stdout); break;
	case 1: fputs(*ws2_c ? ws2_c : DEF_DL_C, stdout); break;
	case 2: fputs(*ws3_c ? ws3_c : DEF_DL_C, stdout); break;
	case 3: fputs(*ws4_c ? ws4_c : DEF_DL_C, stdout); break;
	case 4: fputs(*ws5_c ? ws5_c : DEF_DL_C, stdout); break;
	case 5: fputs(*ws6_c ? ws6_c : DEF_DL_C, stdout); break;
	case 6: fputs(*ws7_c ? ws7_c : DEF_DL_C, stdout); break;
	case 7: fputs(*ws8_c ? ws8_c : DEF_DL_C, stdout); break;
	default: fputs(DEF_DL_C, stdout); break;
	}
}

/* Print the line divinding files and prompt using DIV_LINE. If
 * DIV_LINE takes more than two columns to be printed (ASCII chars
 * take only one, but unicode chars could take two), print exactly the
 * content of DIV_LINE. Otherwise, repeat DIV_LINE to fulfill
 * all terminal columns. If DIV_LINE is '0', print no line at all */
static void
print_div_line(void)
{
#ifdef RUN_CMD
	if (cmd_line_cmd)
		return;
#endif

	if (conf.colorize == 1)
		set_div_line_color();

	if (!*div_line) { /* Let's draw the line with box drawing chars */
		fputs("\x1b(0m", stdout);
		int k;
		for (k = 0; k < (int)term_cols - 2; k++)
			putchar('q');
		fputs("\x1b(0j\x1b(B", stdout);
		putchar('\n');
	} else if (*div_line == '0' && !div_line[1]) {
		/* No line */
		putchar('\n');
	} else {
		/* Custom line */
		size_t len = wc_xstrlen(div_line);
		if (len <= 2) {
			/* Extend DIV_LINE to the end of the screen - 1
			 * We substract 1 to prevent an extra empty line after the
			 * dividing line in some terminals (e.g. cons25) */
			int i;
			for (i = (int)(term_cols / len); i > 1; i--)
				fputs(div_line, stdout);
			putchar('\n');
		} else {
			/* Print DIV_LINE exactly */
			puts(div_line);
		}
	}

	fputs(df_c, stdout);
	fflush(stdout);
}

/* Print free/total space for the file system the current directory belongs to */
static void
print_disk_usage(void)
{
	if (!workspaces || !workspaces[cur_ws].path || !*workspaces[cur_ws].path)
		return;

	struct statvfs stat;
	if (statvfs(workspaces[cur_ws].path, &stat) != EXIT_SUCCESS) {
		_err('w', PRINT_PROMPT, "statvfs: %s\n", strerror(errno));
		return;
	}

	char *free_space = get_size_unit((off_t)(stat.f_bavail * stat.f_frsize));
	char *size = get_size_unit((off_t)(stat.f_blocks * stat.f_frsize));
	int free_percentage = (int)(((stat.f_bavail * stat.f_frsize) * 100)
		/ (stat.f_blocks * stat.f_frsize));

	print_reload_msg(_("%s/%s (%d%% free)\n"),
		free_space ? free_space : "?", size ? size : "?", free_percentage);

	free(free_space);
	free(size);

	return;
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

		printf("%zu %s%s%s\n", i + 1, mi_c,
		    old_pwd[i], df_c);

		if (i + 1 < (size_t)dirhist_total_index && old_pwd[i + 1])
			printf("%zu %s\n", i + 2, old_pwd[i + 1]);

		break;
	}
}

#ifndef _NO_ICONS
/* Set the icon field to the corresponding icon for FILE */
static int
get_name_icon(const char *file, const int n)
{
	if (!file)
		return 0;

	size_t nhash = hashme(file, 0);

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

/* Set the icon field to the corresponding icon for DIR. If not found,
 * set the default icon */
static void
get_dir_icon(const char *dir, const int n)
{
	/* Default values for directories */
	file_info[n].icon = DEF_DIR_ICON;
	file_info[n].icon_color = DEF_DIR_ICON_COLOR;

	if (!dir)
		return;

	size_t dhash = hashme(dir, 0);

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
 * set the default icon */
static void
get_ext_icon(const char *restrict ext, const int n)
{
	if (!file_info[n].icon) {
		file_info[n].icon = DEF_FILE_ICON;
		file_info[n].icon_color = DEF_FILE_ICON_COLOR;
	}

	if (!ext)
		return;

	ext++;

	size_t ehash = hashme(ext, 0);

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

static int
post_listing(DIR *dir, const int close_dir, const int reset_pager)
{
	if (close_dir == 1 && closedir(dir) == -1)
		return EXIT_FAILURE;

/* Let plugins and external programs running in clifm know whether
 * we have changed the current directory (last command) or not. */
//	setenv("CLIFM_CHPWD", dir_changed == 1 ? "1" : "0", 1);

	dir_changed = 0;

	if (xargs.list_and_quit == 1)
		exit(exit_code);

	if (reset_pager == 1 && (conf.pager < 2 || (int)files < conf.pager))
		conf.pager = pager_bk;

	if (max_files != UNSET && (int)files > max_files)
		printf("... (%d/%zu)\n", max_files, files);

	print_div_line();

	if (conf.dirhist_map == 1) { /* Print current, previous, and next entries */
		print_dirhist_map();
		print_div_line();
	}

	if (conf.disk_usage == 1)
		print_disk_usage();

	if (sort_switch == 1) {
		print_reload_msg(_("Sorted by "));
		print_sort_method();
	}

	if (switch_cscheme == 1)
		printf(_("Color scheme %s->%s %s\n"), mi_c, df_c, cur_cscheme);

	if (conf.print_selfiles == 1 && sel_n > 0)
		print_sel_files(term_lines);

	return EXIT_SUCCESS;
}

/* A basic pager for directories containing large amount of files.
 * What's missing? It only goes downwards. To go backwards, use the
 * terminal scrollback function */
static int
run_pager(const int columns_n, int *reset_pager, int *i, size_t *counter)
{
	fputs(PAGER_LABEL, stdout);

	switch (xgetchar()) {

	/* Advance one line at a time */
	case 66: /* fallthrough */ /* Down arrow */
	case 10: /* fallthrough */ /* Enter */
	case 32: /* Space */
		break;

	/* Advance one page at a time */
	case 126: /* Page Down */
		*counter = 0;
		break;

	/* h: Print pager help */
	case 63: /* fallthrough */ /* ? */
	case 104: {
		CLEAR;

		fputs(_(PAGER_HELP), stdout);
		int l = (int)term_lines - 5;
		MOVE_CURSOR_DOWN(l);
		fputs(PAGER_LABEL, stdout);

		xgetchar();
		CLEAR;

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
	case 99:  /* 'c' */
	case 112: /* 'p' */
	case 113: /* 'q' */
		pager_bk = conf.pager; conf.pager = 0; *reset_pager = 1;
		break;

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
# endif
	if (event_fd >= 0) {
		/* Prepare for events */
		EV_SET(&events_to_monitor[0], event_fd, EVFILT_VNODE,
				EV_ADD | EV_CLEAR, KQUEUE_FFLAGS, 0, workspaces[cur_ws].path);
		watch = 1;
		/* Register events */
//		kevent(kq, events_to_monitor, NUM_EVENT_SLOTS, NULL, NUM_EVENT_FDS, NULL);
		kevent(kq, events_to_monitor, NUM_EVENT_SLOTS, NULL, 0, NULL);
	}
#endif /* LINUX_INOTIFY */
}

static void
get_longest_filename(const int n, const int pad)
{
	int i = (max_files != UNSET && max_files < n) ? max_files : n;
	int longest_index = -1;

	while (--i >= 0) {
		size_t total_len = 0;
		file_info[i].eln_n = conf.no_eln == 1 ? -1 : DIGINUM(i + 1);

		size_t file_len = file_info[i].len;
		if (file_len == 0) {
			/* Embedded control chars. Reconstruct and recalculate length. */
			char *wname = replace_ctrl_chars(file_info[i].name);
			file_len = wname ? wc_xstrlen(wname) : 0;
			free(wname);
		}

		if (file_len > (size_t)conf.max_name_len)
			file_len = (size_t)conf.max_name_len;

		total_len = (size_t)pad + 1 + file_len;

		if (conf.long_view == 0 && conf.classify == 1) {
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
#ifdef __sun
				case DT_DOOR: /* fallthrough */
#endif /* __sun */
				case DT_LNK:  /* fallthrough */
				case DT_SOCK: /* fallthrough */
				case DT_FIFO: /* fallthrough */
				case DT_UNKNOWN: total_len++; break;
				default: break;
				}
			}
		}

		if (total_len > longest) {
			longest_index = i;
			if (conf.listing_mode == VERTLIST || max_files == UNSET
			|| i < max_files)
				longest = total_len;
		}
	}

#ifndef _NO_ICONS
	if (conf.icons == 1 && conf.long_view == 0 && conf.columned == 1)
		longest += 3;
#endif /* !_NO_ICONS */

	/* longest_fc stores the amount of digits taken by the files counter of
	 * the longest file name, provided it is a directory
	 * We use this to trim file names up to MAX_NAME_LEN + LONGEST_FC, so
	 * that we can make use the the space used by the files counter
	 * Example:
	 *    longest_dirname/13
	 *    very_long_file_na~
	 * instead of
	 *    longest_dirname/13
	 *    very_long_file_~
	 * */

	longest_fc = 0;
	if (conf.max_name_len != UNSET && file_info[longest_index].dir == 1
	&& file_info[longest_index].filesn > 0 && conf.files_counter == 1) {
		/* We add 1 here to count the slash between the dir name
		 * and the files counter too. However, in doing this the space
		 * between the trimmed file name and the next column is too
		 * tight (only one). By not adding it we get an extra space
		 * to relax a bit the space between columns */
/*		longest_fc = DIGINUM(file_info[longest_index].filesn) + 1; */
		longest_fc = DIGINUM(file_info[longest_index].filesn);
		size_t t = (size_t)pad + (size_t)conf.max_name_len + 1 + longest_fc;
		if (t > longest)
			longest_fc -= t - longest;
		if ((int)longest_fc < 0)
			longest_fc = 0;
	}
}

/* Set a few extra properties needed for long view mode */
static void
set_long_attribs(const int n, const struct stat *attr)
{
	file_info[n].uid = attr->st_uid;
	file_info[n].gid = attr->st_gid;
	file_info[n].mode = attr->st_mode;
	file_info[n].rdev = attr->st_rdev;

	switch (prop_fields.time) {
	case PROP_TIME_ACCESS: file_info[n].ltime = (time_t)attr->st_atime; break;
	case PROP_TIME_CHANGE: file_info[n].ltime = (time_t)attr->st_ctime; break;
	case PROP_TIME_MOD: file_info[n].ltime = (time_t)attr->st_mtime; break;
	default: file_info[n].ltime = (time_t)attr->st_mtime; break;
	}

	if (conf.full_dir_size == 1 && file_info[n].dir == 1) {
		char name[PATH_MAX]; *name = '\0';
		if (file_info[n].type == DT_LNK) /* Symlink to directory */
			snprintf(name, sizeof(name), "%s/", file_info[n].name);

		file_info[n].size = dir_size(*name ? name : file_info[n].name, 0,
			&file_info[n].du_status);
	} else {
		file_info[n].size = FILE_SIZE_PTR;
	}
}

/* Returns the lenght of the largest files counter (directories only)
 * This function is used by the long view function to correctly pad
 * the properties string
 * It returns zero if there are no subdirectories in the current dir */
static size_t
get_max_files_counter(void)
{
	size_t fc_max = 0;
	int i = (int)files;

	while (--i >= 0) {
		if (file_info[i].dir == 0)
			continue;
		size_t t = (size_t)DIGINUM(file_info[i].filesn);
		if (t > fc_max)
			fc_max = t;
	}

	return fc_max;
}

static size_t
get_max_size(void)
{
	size_t size_max = 0;
	int i = (int)files;

	while (--i >= 0) {
		size_t t = (size_t)DIGINUM(file_info[i].size);
		if (t > size_max)
			size_max = t;
	}

	return size_max;
}

/* Return a pointer to the indicator char color and update IND_CHR to the
 * corresponding indicator character for the file whose index is INDEX. */
static inline char *
get_ind_char(const int index, int *ind_chr)
{
	int print_lnk_char = (conf.color_lnk_as_target == 1
		&& file_info[index].symlink == 1 && follow_symlinks == 1
		&& conf.icons == 0 && conf.light_mode == 0);

	*ind_chr = file_info[index].sel == 1 ? SELFILE_CHR
		: (print_lnk_char == 1 ? LINK_CHR : ' ');

	return file_info[index].sel == 1 ? li_cb
		: (print_lnk_char == 1 ? lc_c : "");
}

static void
print_long_mode(size_t *counter, int *reset_pager, const int pad,
	const size_t ug_max, const size_t ino_max, const uint8_t have_xattr)
{
	struct stat lattr;

	size_t fc_max = conf.files_counter == 1 ? get_max_files_counter() : 0;
	size_t size_max = prop_fields.size == PROP_SIZE_BYTES ? get_max_size() : 0;

	/* Available space (term cols) to print the file name */
	int space_left = (int)term_cols - (prop_fields.len + have_xattr
		+ (int)fc_max + (int)size_max + (int)ug_max + (int)ino_max
		+ (conf.icons == 1 ? 3 : 0));

	if (space_left < conf.min_name_trim)
		space_left = conf.min_name_trim;

	if (conf.min_name_trim != UNSET && longest > (size_t)space_left)
		longest = (size_t)space_left;

	if (longest < (size_t)space_left)
		space_left = (int)longest;

	int i, k = (int)files;
	for (i = 0; i < k; i++) {
		if (max_files != UNSET && i == max_files)
			break;
		if (lstat(file_info[i].name, &lattr) == -1)
			continue;

		if (conf.pager == 1 || (*reset_pager == 0 && conf.pager > 1
		&& (int)files >= conf.pager)) {
			int ret = 0;
			if (*counter > (size_t)(term_lines - 2))
				ret = run_pager(-1, reset_pager, &i, counter);
			if (ret == -1 || ret == -2) {
				--i;
				if (ret == -2)
					*counter = 0;
				continue;
			}
			(*counter)++;
		}

		int ind_chr = 0;
		char *ind_chr_color = get_ind_char(i, &ind_chr);

		if (conf.no_eln == 0) {
			printf("%s%*d%s%s%c%s", el_c, pad, i + 1, df_c,
				ind_chr_color, ind_chr, df_c);
		} else {
			printf("%s%c%s", ind_chr_color, ind_chr, df_c);
		}

		struct maxes_t maxes;
		maxes.name = (size_t)space_left + (conf.icons == 1 ? 3 : 0);
		maxes.ids = ug_max;
		maxes.inode = ino_max;
		maxes.files_counter = fc_max;
		maxes.size = size_max;

		/* Print the remaining part of the entry */
		print_entry_props(&file_info[i], &maxes, have_xattr);
	}
}

static size_t
get_columns(void)
{
	size_t n = (size_t)term_cols / (longest + 1); /* +1 for the
	space between file names */

	/* If longest is bigger than terminal columns, columns_n will
	 * be negative or zero. To avoid this: */
	if (n < 1)
		n = 1;

	/* If we have only three files, we don't want four columns */
	if (n > files)
		n = files > 0 ? files : 1;

	return n;
}

static void
get_ext_info(const int i, int *trim_type, size_t *ext_len)
{
	if (file_info[i].ext_name) {
		*trim_type = TRIM_EXT;
		if (conf.unicode == 0) {
			*ext_len = file_info[i].len - (size_t)(file_info[i].ext_name
				- file_info[i].name);
		} else {
			*ext_len = wc_xstrlen(file_info[i].ext_name);
		}

		if ((int)*ext_len >= conf.max_name_len || (int)*ext_len <= 0) {
			*ext_len = 0;
			*trim_type = TRIM_NO_EXT;
		}
		return;
	}

	char *dot = strrchr(file_info[i].name, '.');
	if (!dot || dot == file_info[i].name || !dot[1])
		return;

	file_info[i].ext_name = dot;
	*trim_type = TRIM_EXT;

	if (conf.unicode == 0) {
		*ext_len = file_info[i].len - (size_t)(file_info[i].ext_name
			- file_info[i].name);
	} else {
		*ext_len = wc_xstrlen(file_info[i].ext_name);
	}

	if ((int)*ext_len >= conf.max_name_len || (int)*ext_len <= 0) {
		*ext_len = 0;
		*trim_type = TRIM_NO_EXT;
	}
}

/* Construct the file name to be displayed.
 * The file name is trimmed if longer than MAX_NAMELEN (if conf.max_name_len
 * is set). */
static char *
construct_filename(const int i, struct wtrim_t *wtrim, const int max_namelen)
{
	/* wc_xstrlen returns 0 if a non-printable char was found in the file name */
	if (file_info[i].len == 0) {
		wtrim->wname = replace_ctrl_chars(file_info[i].name);
		file_info[i].len = wc_xstrlen(wtrim->wname);
	}

	char *name = wtrim->wname ? wtrim->wname : file_info[i].name;

	if (files <= 1 || conf.max_name_len == UNSET || conf.long_view != 0
	|| (int)file_info[i].len <= max_namelen)
		return name;

	wtrim->type = TRIM_NO_EXT;
	size_t ext_len = 0;
	get_ext_info(i, &wtrim->type, &ext_len);

	xstrsncpy(name_buf, wtrim->wname ? wtrim->wname
		: file_info[i].name, sizeof(name_buf) - 1);
	wtrim->diff = u8truncstr(name_buf, (size_t)max_namelen - 1 - ext_len);
	file_info[i].len = (size_t)max_namelen;

	return name_buf;
}

static void
print_entry_color(int *ind_char, const int i, const int pad,
	const int max_namelen)
{
	*ind_char = 0;
	char *end_color = file_info[i].dir == 1 ? fc_c : df_c;

	struct wtrim_t wtrim = (struct wtrim_t){0};
	char *n = construct_filename(i, &wtrim, max_namelen);

	char trim_diff[14];
	*trim_diff = '\0';
	if (wtrim.diff > 0)
		snprintf(trim_diff, sizeof(trim_diff), "\x1b[%dC", wtrim.diff);

	int ind_chr = 0;
	char *ind_chr_color = get_ind_char(i, &ind_chr);

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
				xprintf("%s%*d%s%s%c%s%s%s %s%ls%s\x1b[0m%s%c\x1b[0m%s%s%s",
					el_c, pad, i + 1, df_c, ind_chr_color, ind_chr,
					df_c, file_info[i].icon_color, file_info[i].icon,
					file_info[i].color, (wchar_t *)n, trim_diff,
					tt_c, TRIMFILE_CHR,
					wtrim.type == TRIM_EXT ? file_info[i].color : "",
					wtrim.type == TRIM_EXT ? file_info[i].ext_name : "",
					end_color);
			} else {
				xprintf("%s%*d%s%s%c%s%s%s %s%s%s", el_c, pad, i + 1, df_c,
					ind_chr_color, ind_chr, df_c,
					file_info[i].icon_color, file_info[i].icon,
					file_info[i].color, n, end_color);
			}
		}
	} else
#endif
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
				xprintf("%s%*d%s%s%c%s%s%ls%s\x1b[0m%s%c\x1b[0m%s%s%s",
					el_c, pad, i + 1, df_c, ind_chr_color,
					ind_chr,
					df_c, file_info[i].color, (wchar_t *)n,
					trim_diff, tt_c, TRIMFILE_CHR,
					wtrim.type == TRIM_EXT ? file_info[i].color : "",
					wtrim.type == TRIM_EXT ? file_info[i].ext_name : "",
					end_color);
			} else {
				xprintf("%s%*d%s%s%c%s%s%s%s", el_c, pad, i + 1, df_c,
					ind_chr_color, ind_chr, df_c,
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
print_entry_nocolor(int *ind_char, const int i, const int pad,
	const int max_namelen)
{
	struct wtrim_t wtrim = (struct wtrim_t){0};
	char *n = construct_filename(i, &wtrim, max_namelen);

	char trim_diff[14];
	*trim_diff = '\0';
	if (wtrim.diff > 0)
		snprintf(trim_diff, sizeof(trim_diff), "\x1b[%dC", wtrim.diff);

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
				xprintf("%s%*d%s%c%s %ls%s%c%s", el_c, pad, i + 1, df_c,
					file_info[i].sel ? SELFILE_CHR : ' ', file_info[i].icon,
					(wchar_t *)n, trim_diff, TRIMFILE_CHR,
					wtrim.type == TRIM_EXT ? file_info[i].ext_name : "");
			} else {
				xprintf("%s%*d%s%c%s %s", el_c, pad, i + 1, df_c,
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
				xprintf("%s%*d%s%c%ls%s%c%s", el_c, pad, i + 1, df_c,
					file_info[i].sel ? SELFILE_CHR : ' ', (wchar_t *)n,
					trim_diff, TRIMFILE_CHR, wtrim.type == TRIM_EXT
					? file_info[i].ext_name : "");
			} else {
				xprintf("%s%*d%s%c%s", el_c, pad, i + 1, df_c,
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
#ifdef __sun
		case DT_DOOR: putchar(DOOR_CHR); break;
#endif /* __sun */
		case DT_FIFO: putchar(FIFO_CHR); break;
		case DT_SOCK: putchar(SOCK_CHR); break;
		case DT_UNKNOWN: putchar(UNKNOWN_CHR); break;
		default: *ind_char = 0;
		}
	}

	free(wtrim.wname);
}

static void
print_entry_color_light(int *ind_char, const int i, const int pad,
	const int max_namelen)
{
	*ind_char = 0;
	char *end_color = file_info[i].dir == 1 ? fc_c : df_c;

	struct wtrim_t wtrim = (struct wtrim_t){0};
	char *n = construct_filename(i, &wtrim, max_namelen);

	char trim_diff[14];
	*trim_diff = '\0';
	if (wtrim.diff > 0)
		snprintf(trim_diff, sizeof(trim_diff), "\x1b[%dC", wtrim.diff);

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
				xprintf("%s%*d%s %s%s %s%ls%s\x1b[0m%s%c\x1b[0m%s%s%s",
					el_c, pad, i + 1, df_c,	file_info[i].icon_color,
					file_info[i].icon, file_info[i].color, (wchar_t *)n,
					trim_diff, tt_c, TRIMFILE_CHR,
					wtrim.type == TRIM_EXT ? file_info[i].color : "",
					wtrim.type == TRIM_EXT ? file_info[i].ext_name : "",
					end_color);
			} else {
				xprintf("%s%*d%s %s%s %s%s%s", el_c, pad, i + 1, df_c,
					file_info[i].icon_color, file_info[i].icon,
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
				xprintf("%s%*d%s %s%ls%s\x1b[0m%s%c\x1b[0m%s%s%s",
					el_c, pad, i + 1, df_c,	file_info[i].color, (wchar_t *)n,
					trim_diff, tt_c, TRIMFILE_CHR,
					wtrim.type == TRIM_EXT ? file_info[i].color : "",
					wtrim.type == TRIM_EXT ? file_info[i].ext_name : "",
					end_color);
			} else {
				xprintf("%s%*d%s %s%s%s", el_c, pad, i + 1, df_c,
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
print_entry_nocolor_light(int *ind_char, const int i, const int pad,
	const int max_namelen)
{
	struct wtrim_t wtrim = (struct wtrim_t){0};
	char *n = construct_filename(i, &wtrim, max_namelen);

	char trim_diff[14];
	*trim_diff = '\0';
	if (wtrim.diff > 0)
		snprintf(trim_diff, sizeof(trim_diff), "\x1b[%dC", wtrim.diff);

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
				xprintf("%s%*d%s %s %ls%s%c%s", el_c, pad, i + 1, df_c,
					file_info[i].icon, (wchar_t *)n, trim_diff, TRIMFILE_CHR,
					wtrim.type == TRIM_EXT ? file_info[i].ext_name : "");
			} else {
				xprintf("%s%*d%s %s %s", el_c, pad, i + 1, df_c,
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
				xprintf("%s%*d%s %ls%s%c%s", el_c, pad, i + 1, df_c,
					(wchar_t *)n, trim_diff, TRIMFILE_CHR,
					wtrim.type == TRIM_EXT ? file_info[i].ext_name : "");
			} else {
				xprintf("%s%*d%s %s", el_c, pad, i + 1, df_c, n);
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
#ifdef __sun
		case DT_DOOR: putchar(DOOR_CHR); break;
#endif /* __sun */
		case DT_FIFO: putchar(FIFO_CHR); break;
		case DT_LNK: putchar(LINK_CHR); break;
		case DT_SOCK: putchar(SOCK_CHR); break;
		case DT_UNKNOWN: putchar(UNKNOWN_CHR); break;
		default: *ind_char = 0; break;
		}
	}

	free(wtrim.wname);
}

/* Pad the current file name to equate the longest file name length */
static void
pad_filename(const int ind_char, const int i, const int pad,
	const int termcap_move_right)
{
	int cur_len = 0;

#ifndef _NO_ICONS
	cur_len = pad + 1 + (conf.icons == 1 ? 3 : 0) + (int)file_info[i].len
		+ (ind_char == 1 ? 1 : 0);
#else
	cur_len = pad + 1 + (int)file_info[i].len + (*ind_char ? 1 : 0);
#endif /* !_NO_ICONS */

	if (file_info[i].dir == 1 && conf.classify == 1) {
		cur_len++;
		if (file_info[i].filesn > 0 && conf.files_counter == 1
		&& file_info[i].ruser == 1)
			cur_len += DIGINUM((int)file_info[i].filesn);
	}

	int diff = (int)longest - cur_len;
	if (termcap_move_right == 0) {
		int j = diff + 1;
		while(--j >= 0)
			putchar(' ');
	} else {
		MOVE_CURSOR_RIGHT(diff + 1);
	}
}

/* Add spaces needed to equate the longest file name length */
static void
pad_filename_light(const int ind_char, const int i, const int pad,
	const int termcap_move_right)
{
	int cur_len = 0;
#ifndef _NO_ICONS
	cur_len = pad + 1 + (conf.icons == 1 ? 3 : 0)
		+ (int)file_info[i].len + (ind_char == 1 ? 1 : 0);
#else
	cur_len = pad + 1 + (int)file_info[i].len + (*ind_char ? 1 : 0);
#endif /* !_NO_ICONS */

	if (conf.classify == 1) {
		if (file_info[i].dir == 1)
			cur_len++;
		if (file_info[i].filesn > 0 && conf.files_counter == 1
		&& file_info[i].ruser == 1)
			cur_len += DIGINUM((int)file_info[i].filesn);
	}

	int diff = (int)longest - cur_len;

	if (termcap_move_right == 0) {
		int j = diff + 1;
		while(--j >= 0)
			putchar(' ');
	} else {
		MOVE_CURSOR_RIGHT(diff + 1);
	}
}

/* List files horizontally:
 * 1 AAA	2 AAB	3 AAC
 * 4 AAD	5 AAE	6 AAF */
static void
list_files_horizontal(size_t *counter, int *reset_pager, const int pad,
		const size_t columns_n)
{
	int nn = (max_files != UNSET && max_files < (int)files)
		? max_files : (int)files;

	void (*print_entry_function)(int *, const int, const int, const int);
	if (conf.colorize == 1)
		print_entry_function = conf.light_mode == 1
			? print_entry_color_light : print_entry_color;
	else
		print_entry_function = conf.light_mode == 1
			? print_entry_nocolor_light : print_entry_nocolor;

	void (*pad_filename_function)(const int, const int, const int, const int);
	pad_filename_function = conf.light_mode == 1
		? pad_filename_light : pad_filename;

	int termcap_move_right = (xargs.list_and_quit == 1
		|| term_caps.suggestions == 0) ? 0 : 1;

	size_t cur_cols = 0;
	int i, last_column = 0;
	int blc = last_column;
	for (i = 0; i < nn; i++) {
		/* If current entry is in the last column, we need to print a new line char */
		size_t bcur_cols = cur_cols;
		if (++cur_cols == columns_n) {
			cur_cols = 0;
			last_column = 1;
		} else {
			last_column = 0;
		}

		int ind_char = 1;
		if (conf.classify == 0)
			ind_char = 0;

				/* ##########################
				 * #  MAS: A SIMPLE PAGER   #
				 * ########################## */

		if (conf.pager == 1 || (*reset_pager == 0 && conf.pager > 1
		&& (int)files >= conf.pager)) {
			/* Run the pager only once all columns and rows fitting in
			 * the screen are filled with the corresponding file names */
			int ret = 0, bi = i;
			if (blc && *counter > columns_n * ((size_t)term_lines - 2))
				ret = run_pager((int)columns_n, reset_pager, &i, counter);

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

		int fc = file_info[i].dir != 1 ? (int)longest_fc : 0;
		/* Displayed file name will be trimmed to MAX_NAME_LEN. */
		int max_namelen = conf.max_name_len + fc;

		file_info[i].eln_n = conf.no_eln == 1 ? -1 : DIGINUM(i + 1);

		print_entry_function(&ind_char, i, pad, max_namelen);

		if (last_column == 0)
			pad_filename_function(ind_char, i, pad, termcap_move_right);
		else
			putchar('\n');
	}

	if (last_column == 0)
		putchar('\n');
}

/* List files vertically, like ls(1) would
 * 1 AAA	3 AAC	5 AAE
 * 2 AAB	4 AAD	6 AAF */
static void
list_files_vertical(size_t *counter, int *reset_pager, const int pad,
		const size_t columns_n)
{
	int nn = (max_files != UNSET && max_files < (int)files)
		? max_files : (int)files;

	int rows = nn / (int)columns_n;
	if (nn % (int)columns_n > 0)
		rows++;

	int last_column = 0;
	/* The previous value of LAST_COLUMN. We need this value to run the pager */
	int blc = last_column;

	void (*print_entry_function)(int *, const int, const int, const int);
	if (conf.colorize == 1)
		print_entry_function = conf.light_mode == 1
			? print_entry_color_light : print_entry_color;
	else
		print_entry_function = conf.light_mode == 1
			? print_entry_nocolor_light : print_entry_nocolor;

	void (*pad_filename_function)(const int, const int, const int, const int);
	pad_filename_function = conf.light_mode == 1
		? pad_filename_light : pad_filename;

	int termcap_move_right = (xargs.list_and_quit == 1
		|| term_caps.suggestions == 0) ? 0 : 1;

	size_t cur_cols = 0;
	size_t cc = columns_n; // Amount of columns actually printed (per line)
	int x = 0; // Index of the file to be actually printed
	int xx = 0;
	int i = 0; // Index of the current entry being analyzed

	for ( ; ; i++) {
		/* Copy current values to restore them if necessary: done to
		 * skip the first two chars of arrow keys : \x1b [ */
		int bxx = xx, bx = x; // Copies of X and XX
		size_t bcc = cc; // Copy of CC
		if (cc == columns_n) {
			x = xx;
			xx++;
			cc = 0;
		} else {
			x += rows;
		}
		cc++;

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

		int ind_char = 1;
		if (conf.classify == 0)
			ind_char = 0;

		if (x >= nn || !file_info[x].name) {
			if (last_column == 1)
				/* Last column is empty. Ex:
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
		&& (int)files >= conf.pager)) {
			int ret = 0, bi = i;
			/* Run the pager only once all columns and rows fitting in
			 * the screen are filled with the corresponding file names */
			if (blc && *counter > columns_n * ((size_t)term_lines - 2))
				ret = run_pager((int)columns_n, reset_pager, &x, counter);

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
			(*counter)++;
		}

		blc = last_column;

			/* #################################
			 * #    PRINT THE CURRENT ENTRY    #
			 * ################################# */

		int fc = file_info[x].dir != 1 ? (int)longest_fc : 0;
		/* Displayed file name will be trimmed to MAX_NAMELEN. */
		int max_namelen = conf.max_name_len + fc;

		file_info[x].eln_n = conf.no_eln == 1 ? -1 : DIGINUM(x + 1);

		print_entry_function(&ind_char, x, pad, max_namelen);

		if (last_column == 0)
			pad_filename_function(ind_char, x, pad, termcap_move_right);
		else
			/* Last column is populated. Ex:
			 * 1 file  3 file3  5 file5HERE
			 * 2 file2 4 file4  6 file6HERE
			 * ... */
			putchar('\n');
	}

	if (last_column == 0)
		putchar('\n');
}

/* Execute commands in either DIR_IN_NAME or DIR_OUT_NAME files.
 * MODE (either DIR_IN or DIR_OUT) tells the function whether to check
 * for DIR_IN_NAME or DIR_OUT_NAME files.
 * Used by the autocommands function. */
static void
run_dir_cmd(const int mode)
{
	FILE *fp = (FILE *)NULL;
	char path[PATH_MAX];

	if (mode == DIR_IN) {
		snprintf(path, sizeof(path), "%s/%s",
			workspaces[cur_ws].path, DIR_IN_NAME);
	} else { /* DIR_OUT */
		if (dirhist_cur_index <= 0 || !old_pwd[dirhist_cur_index - 1])
			return;
		snprintf(path, sizeof(path), "%s/%s",
			old_pwd[dirhist_cur_index - 1], DIR_OUT_NAME);
	}

	fp = fopen(path, "r");
	if (!fp)
		return;

	char buf[PATH_MAX];
	*buf = '\0';
	char *ret = fgets(buf, sizeof(buf), fp);
	if (!ret) {
		fclose(fp);
		return;
	}

	fclose(fp);

	if (!*buf)
		return;

	if (xargs.secure_cmds == 0
	|| sanitize_cmd(buf, SNT_AUTOCMD) == EXIT_SUCCESS)
		launch_execl(buf);
}

/* Check if S is either .cfm.in or .cfm.out */
static void
check_autocmd_file(const char *s)
{
	if (*s == '.' && s[1] == 'c' && s[2] == 'f' && s[3] == 'm'
	&& s[4] == '.') {
		if (s[5] == 'o' && s[6] == 'u' && s[7] == 't' && !s[8]) {
			dir_out = 1;
		} else {
			if (s[5] == 'i' && s[6] == 'n' && !s[7])
				run_dir_cmd(DIR_IN);
		}
	}
}

static void
get_largest(const size_t i, off_t *size, char **name, char **color, off_t *total)
{
	/* Only directories and regular files should be counted */
	if (file_info[i].type != DT_DIR && file_info[i].type != DT_REG)
		return;

	int d = (file_info[i].type == DT_DIR ? 1024 : 1);
	if (file_info[i].size * d > *size) {
		*size = file_info[i].size * d;
		*name = file_info[i].name;
		*color = file_info[i].color;
	}

	/* Do not recount hardlinks */
	if (file_info[i].linkn > 1) {
		int j = (int)i - 1;
		while (--j >= 0)
			if (file_info[i].inode == file_info[j].inode)
				return;
	}

	*total += file_info[i].size * d;
}

/* Return the lenght of the longest UID:GID string for files listed in
 * long view mode */
static size_t
get_max_ug_str(void)
{
	size_t ug_max = 0;
	int i = (int)files;

	while (--i >= 0) {
		size_t t = (size_t)DIGINUM(file_info[i].uid) + DIGINUM(file_info[i].gid);
		if (t > ug_max)
			ug_max = t;
	}

	return ug_max;
}

static size_t
get_longest_inode(void)
{
	size_t l = 0;

	int i = (int)files;
	while (--i >= 0) {
		size_t n = DIGINUM(file_info[i].inode);
		if (n > l)
			l = n;
	}

	return l;
}

static int
exclude_file_type_light(const unsigned char type)
{
	if (!*(filter.str + 1))
		return EXIT_FAILURE;

	int match = 0;

	switch (*(filter.str + 1)) {
	case 'd': if (type == DT_DIR) match = 1; break;
	case 'f': if (type == DT_REG) match = 1; break;
	case 'l': if (type == DT_LNK) match = 1; break;
	case 's': if (type == DT_SOCK) match = 1; break;
	case 'c': if (type == DT_CHR) match = 1; break;
	case 'b': if (type == DT_BLK) match = 1; break;
	case 'p': if (type == DT_FIFO) match = 1; break;
#ifdef __sun
	case 'D': if (type == DT_DOOR) match = 1; break;
#endif /* __sun */
	default: return EXIT_FAILURE;
	}

	if (match == 1)
		return filter.rev == 1 ? EXIT_SUCCESS : EXIT_FAILURE;
	else
		return filter.rev == 1 ? EXIT_FAILURE : EXIT_SUCCESS;
}

/* Returns EXIT_SUCCESS if the file with mode MODE and LINKS number
 * of links must be excluded from the files list, or EXIT_FAILURE */
static int
exclude_file_type(const mode_t mode, const nlink_t links)
{
/* ADD: C = Files with capabilities */

	if (!*(filter.str + 1))
		return EXIT_FAILURE;

	int match = 0;

	switch (*(filter.str + 1)) {
	case 'b': if (S_ISBLK(mode)) match = 1; break;
	case 'd': if (S_ISDIR(mode)) match = 1; break;
#ifdef __sun
	case 'D': if (S_ISDOOR(mode)) match = 1; break;
#endif /* __sun */
	case 'c': if (S_ISCHR(mode)) match = 1; break;
	case 'f': if (S_ISREG(mode)) match = 1; break;
	case 'l': if (S_ISLNK(mode)) match = 1; break;
	case 'p': if (S_ISFIFO(mode)) match = 1; break;
	case 's': if (S_ISSOCK(mode)) match = 1; break;

	case 'g': if (mode & 02000) match = 1; break; /* SGID */
	case 'h': if (links > 1 && !S_ISDIR(mode)) match = 1; break;
	case 'o': if (mode & 00002) match = 1; break; /* Other writable */
	case 't': if (mode & 01000) match = 1; break; /* Sticky */
	case 'u': if (mode & 04000) match = 1; break; /* SUID */
	case 'x': /* Executable */
		if (S_ISREG(mode) && ((mode & 00100) || (mode & 00010) || (mode & 00001)))
			match = 1;
		break;
	default: return EXIT_FAILURE;
	}

	if (match == 1)
		return filter.rev == 1 ? EXIT_SUCCESS : EXIT_FAILURE;
	else
		return filter.rev == 1 ? EXIT_FAILURE : EXIT_SUCCESS;
}

/* Return 1 if NAME contains at least one UTF8 character or control char, or
 * 0 otherwise.
 * This check is performed over file names to be listed. If the file name is
 * not UTF8, we get its visible length directly from xstrsncpy(), instead of
 * running xstrsncpy() and then wc_xstrlen(). This gives us a little
 * performance improvement: 3% faster over 100000 files. */
static inline uint8_t
is_utf8_name(const char *name)
{
	while (*name) {
		if ((*name & 0xC0) == 0xC0 || *name < ' ')
			return 1;
		name++;
	}

	return 0;
}

/* List files in the current working directory (global variable 'path').
 * Unlike list_dir(), however, this function uses no color and runs
 * neither stat() nor count_dir(), which makes it quite faster. Return
 * zero on success and one on error */
static int
list_dir_light(void)
{
#ifdef _LIST_SPEED
	clock_t start = clock();
#endif /* _LIST_SPEED */

	int virtual_dir = 0;
	if (stdin_tmp_dir && strcmp(stdin_tmp_dir, workspaces[cur_ws].path) == 0)
		virtual_dir = 1;

	DIR *dir;
	struct dirent *ent;
	int reset_pager = 0;
	int close_dir = 1;
	int excluded_files = 0;
	uint8_t have_xattr = 0;

	/* A few variables for the disk usage analyzer mode */
	off_t largest_size = 0, total_size = 0;
	char *largest_name = (char *)NULL, *largest_color = (char *)NULL;

	if ((dir = opendir(workspaces[cur_ws].path)) == NULL) {
		xerror("%s: %s: %s\n", PROGRAM_NAME, workspaces[cur_ws].path,
			strerror(errno));
		close_dir = 0;
		goto END;
	}

	set_events_checker();

	errno = 0;
	longest = 0;
	unsigned int n = 0;
	unsigned int total_dents = 0, count = 0;

	file_info = (struct fileinfo *)xnmalloc(ENTRY_N + 2, sizeof(struct fileinfo));

	while ((ent = readdir(dir))) {
		char *ename = ent->d_name;
		/* Skip self and parent directories */
		if (SELFORPARENT(ename))
			continue;

		/* Check .cfm.in and .cfm.out files for the autocommands function */
		if (dir_changed == 1)
			check_autocmd_file(ename);

		/* Skip files according to a regex filter */
		if (filter.str && filter.type == FILTER_FILE_NAME) {
			if (regexec(&regex_exp, ename, 0, NULL, 0) == EXIT_SUCCESS) {
				if (filter.rev == 1) {
					excluded_files++;
					continue;
				}
			} else if (filter.rev == 0) {
				excluded_files++;
				continue;
			}
		}

		if (conf.show_hidden == 0 && *ename == '.')
			continue;
#ifndef _DIRENT_HAVE_D_TYPE
		struct stat attr;
		if (lstat(ename, &attr) == -1)
			continue;
		if (conf.only_dirs == 1 && !S_ISDIR(attr.st_mode))
#else
		if (conf.only_dirs == 1 && ent->d_type != DT_DIR)
#endif /* !_DIRENT_HAVE_D_TYPE */
			continue;

		/* Filter files according to file type */
		if (filter.str && filter.type == FILTER_FILE_TYPE
#ifndef _DIRENT_HAVE_D_TYPE
		&& exclude_file_type_light((unsigned char)get_dt(attr.st_mode)) == EXIT_SUCCESS) {
#else
		&& exclude_file_type_light(ent->d_type) == EXIT_SUCCESS) {
#endif /* !_DIRENT_HAVE_D_TYPE */
			excluded_files++;
			continue;
		}

		if (count > ENTRY_N) {
			count = 0;
			total_dents = n + ENTRY_N;
			file_info = xrealloc(file_info, (total_dents + 2) * sizeof(struct fileinfo));
		}

//		init_fileinfo(n);

		file_info[n].name = (char *)xnmalloc(NAME_MAX + 1, sizeof(char));
		if (conf.unicode == 0 || is_utf8_name(ename) == 0) {
			file_info[n].len = xstrsncpy(file_info[n].name, ename, NAME_MAX);
		} else {
			xstrsncpy(file_info[n].name, ename, NAME_MAX);
			file_info[n].len = wc_xstrlen(ename);
		}

		/* ################  */
#ifndef _DIRENT_HAVE_D_TYPE
		file_info[n].type = get_dt(attr.st_mode);
#else
		/* If type is unknown, we might be facing a file system not
		 * supporting d_type, for example, loop devices. In this case,
		 * try falling back to stat(3) */
		if (ent->d_type == DT_UNKNOWN) {
			struct stat a;
			if (lstat(ename, &a) == -1)
				continue;
			file_info[n].type = get_dt(a.st_mode);
		} else {
			file_info[n].type = ent->d_type;
		}
#endif /* !_DIRENT_HAVE_D_TYPE */
		file_info[n].dir = (file_info[n].type == DT_DIR) ? 1 : 0;
		file_info[n].symlink = (file_info[n].type == DT_LNK) ? 1 : 0;

		file_info[n].inode = ent->d_ino;
		file_info[n].linkn = 1;
		file_info[n].size = 1;
		file_info[n].color = (char *)NULL;
		file_info[n].ext_color = (char *)NULL;
		file_info[n].ext_name = (char *)NULL;
		file_info[n].exec = 0;
		file_info[n].ruser = 1;
		file_info[n].filesn = 0;
		file_info[n].time = 0;
		file_info[n].sel = 0;
#ifndef _NO_ICONS
		file_info[n].icon = DEF_FILE_ICON;
		file_info[n].icon_color = DEF_FILE_ICON_COLOR;
#else
		file_info[n].icon = (char *)NULL;
		file_info[n].icon_color = df_c;
#endif /* !_NO_ICONS */
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
#ifdef __sun
		case DT_DOOR: file_info[n].color = oo_c; stats.door++; break;
#endif /* __sun */
		case DT_UNKNOWN: file_info[n].color = uf_c; stats.unknown++; break;
		default: file_info[n].color = df_c; break;
		}

#ifndef _NO_ICONS
		if (xargs.icons_use_file_color == 1 && conf.icons == 1)
			file_info[n].icon_color = file_info[n].color;
#endif /* !_NO_ICONS */

		if (conf.long_view == 1) {
			struct stat _attr;
			if (lstat(file_info[n].name, &_attr) != -1)
				set_long_attribs((int)n, &_attr);
		}

		if (xargs.disk_usage_analyzer == 1)
			get_largest(n, &largest_size, &largest_name, &largest_color, &total_size);

		n++;
		if (n > (unsigned int)INT_MAX) {
			n--;
			_err('w', PRINT_PROMPT, _("%s: Integer overflow "
				"detected (showing only %u files)\n"), PROGRAM_NAME, n);
			break;
		}

		count++;
	}

	if (xargs.disk_usage_analyzer == 1)
		putchar('\r');

	file_info[n].name = (char *)NULL;
	files = (size_t)n;

	if (n == 0) {
		printf("%s. ..%s\n", conf.colorize ? di_c : df_c, df_c);
		free(file_info);
		goto END;
	}

	int pad = (max_files != UNSET && (int)files > max_files)
		? DIGINUM(max_files) : DIGINUM(files);

	if (conf.sort != SNONE)
		ENTSORT(file_info, n, entrycmp);

	size_t counter = 0;
	size_t columns_n = 1;

	/* Get the longest file name */
	if (conf.columned == 1 || conf.long_view == 1) {
		int nn = (int)n;
		get_longest_filename(nn, pad);
	}

				/* ########################
				 * #    LONG VIEW MODE    #
				 * ######################## */

	if (conf.long_view == 1) {
		print_long_mode(&counter, &reset_pager, pad,
			prop_fields.ids == 1 ? get_max_ug_str() : 0,
			prop_fields.inode == 1 ? get_longest_inode() : 0, have_xattr);
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
	exit_code = post_listing(dir, close_dir, reset_pager);
	if (virtual_dir == 1)
		print_reload_msg(_("Virtual directory\n"));
	if (excluded_files > 0)
		printf(_("Excluded files: %d\n"), excluded_files);

	if (xargs.disk_usage_analyzer == 1 && conf.long_view == 1
	&& conf.full_dir_size == 1)
		print_analysis_stats(total_size, largest_size, largest_color, largest_name);

#ifdef _LIST_SPEED
	clock_t end = clock();
	printf("list_dir time: %f\n", (double)(end - start) / CLOCKS_PER_SEC);
#endif /* _LIST_SPEED */

	return exit_code;
}

/* Check whether the file in the device DEV with inode INO is selected.
 * Used to mark selected files in the files list */
static inline int
check_seltag(const dev_t dev, const ino_t ino, const nlink_t links,
	const size_t index)
{
	if (sel_n == 0 || !sel_devino)
		return 0;

	int j = (int)sel_n;
	while (--j >= 0) {
		if (sel_devino[j].dev != dev || sel_devino[j].ino != ino)
			continue;
		/* Only check hardlinks in case of regular files */
		if (file_info[index].type != DT_DIR && links > 1) {
			char *p = strrchr(sel_elements[j].name, '/');
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

/* Initialize the file_info struct, mostly in case stat fails */
static inline void
init_fileinfo(const size_t n)
{
	file_info[n].color = df_c;
	file_info[n].ext_color = (char *)NULL;
	file_info[n].ext_name = (char *)NULL;
#ifndef _NO_ICONS
	file_info[n].icon = DEF_FILE_ICON;
	file_info[n].icon_color = DEF_FILE_ICON_COLOR;
#endif
	file_info[n].name = (char *)NULL;
	file_info[n].dir = 0;
	file_info[n].eln_n = 0;
	file_info[n].exec = 0;
	file_info[n].filesn = 0; /* Number of files in subdir */
	file_info[n].ruser = 1; /* User read permission for dir */
	file_info[n].symlink = 0;
	file_info[n].sel = 0;
	file_info[n].len = 0;
	file_info[n].mode = 0; /* Store st_mode (for long view mode) */
	file_info[n].type = 0; /* Store d_type value */
	file_info[n].inode = 0;
	file_info[n].size = 1;
	file_info[n].uid = 0;
	file_info[n].gid = 0;
	file_info[n].linkn = 1;
	file_info[n].ltime = 0; /* For long view mode */
	file_info[n].time = 0;
	file_info[n].xattr = 0;
	file_info[n].du_status = 0;
/*	file_info[n].dev = 0;
	file_info[n].ino = 0; */
}

/* Initialize the stats struct */
static inline void
reset_stats(void)
{
	stats.dir = 0;
	stats.reg = 0;
	stats.exec = 0;
	stats.hidden = 0;
	stats.suid = 0;
	stats.sgid = 0;
	stats.fifo = 0;
	stats.socket = 0;
	stats.block_dev = 0;
	stats.char_dev = 0;
	stats.caps = 0;
	stats.link = 0;
	stats.broken_link = 0;
	stats.multi_link = 0;
	stats.other_writable = 0;
	stats.sticky = 0;
	stats.extended = 0;
	stats.unknown = 0;
	stats.unstat = 0;
#ifdef __sun
	stats.door = 0;
#endif /* __sun */
}

/* Get the color of a link target NAME, whose file attributes are ATTR,
 * and write the result into the file_info array at index I. */
static inline void
get_link_target_color(const char *name, const struct stat *attr, const size_t i)
{
	switch (attr->st_mode & S_IFMT) {
	case S_IFSOCK: file_info[i].color = so_c; break;
	case S_IFIFO:  file_info[i].color = pi_c; break;
	case S_IFBLK:  file_info[i].color = bd_c; break;
	case S_IFCHR:  file_info[i].color = cd_c; break;
#ifdef __sun
	case S_IFDOOR: file_info[i].color = oo_c; break;
#endif /* __sun */
	case S_IFREG: {
		int ext = 0;
		char *color = get_regfile_color(name, attr, &ext);

		if (!color) {
			file_info[i].color = fi_c;
			return;
		}

		if (ext == 1)
			file_info[i].ext_color = savestring(color, strlen(color));

		file_info[i].color = ext == 1 ? file_info[i].ext_color : color;
		}
		break;

	default: file_info[i].color = df_c; break;
	}
}

/* List files in the current working directory. Uses file type colors
 * and columns. Return zero on success or one on error */
int
list_dir(void)
{
#ifdef _LIST_SPEED
	clock_t start = clock();
#endif /* _LIST_SPEED */

	if (conf.clear_screen == 1) {
		CLEAR; fflush(stdout);
	}

	/* Hide the cursor to minimize flickering: it will be unhidden immediately
	 * before printing the next prompt (prompt.c) */
	if (xargs.list_and_quit != 1)
		HIDE_CURSOR;

	if (dir_changed == 1 && autocmds_n > 0) {
		if (autocmd_set == 1)
			revert_autocmd_opts();
		check_autocmds();
	}

	if (dir_changed == 1 && dir_out == 1) {
		run_dir_cmd(DIR_OUT);
		dir_out = 0;
	}

	if (conf.clear_screen == 1) {
		/* For some reason we need to clear the screen twice to prevent
		 * a garbage first line when scrolling up */
		CLEAR; fflush(stdout);
	}

	if (xargs.disk_usage_analyzer == 1
	|| (conf.long_view == 1 && conf.full_dir_size == 1)) {
		UNHIDE_CURSOR;
		fputs(_("Scanning... "), stdout);
		fflush(stdout);
		if (xargs.list_and_quit != 1)
			HIDE_CURSOR;
	}

	if (conf.unicode == 0) {
		trim.state = trim.a = trim.b = 0;
		trim.len = 0;
	}

	reset_stats();
	get_term_size();

	if (conf.long_view == 1)
		props_now = time(NULL);

	if (conf.light_mode == 1)
		return list_dir_light();

	int virtual_dir = 0;
	if (stdin_tmp_dir && strcmp(stdin_tmp_dir, workspaces[cur_ws].path) == 0)
		virtual_dir = 1;

	DIR *dir;
	struct dirent *ent;
	struct stat attr;
	int reset_pager = 0;
	int close_dir = 1;
	int excluded_files = 0;
	uint8_t have_xattr = 0;

	/* A few variables for the disk usage analyzer mode */
	off_t largest_size = 0, total_size = 0;
	char *largest_name = (char *)NULL, *largest_color = (char *)NULL;

	if ((dir = opendir(workspaces[cur_ws].path)) == NULL) {
		xerror("%s: %s: %s\n", PROGRAM_NAME, workspaces[cur_ws].path,
			strerror(errno));
		close_dir = 0;
		goto END;
	}

	set_events_checker();

	int fd = dirfd(dir);

		/* ##########################################
		 * #    GATHER AND STORE FILE INFORMATION   #
		 * ########################################## */

	errno = 0;
	longest = 0;
	unsigned int n = 0;
	unsigned int total_dents = 0, count = 0;

	file_info = (struct fileinfo *)xnmalloc(ENTRY_N + 2, sizeof(struct fileinfo));

	while ((ent = readdir(dir))) {
		char *ename = ent->d_name;
		/* Skip self and parent directories */
		if (SELFORPARENT(ename))
			continue;

		/* Check .cfm.in and .cfm.out files for the autocommands function */
		if (dir_changed == 1)
			check_autocmd_file(ename);

		/* Filter files according to a regex filter */
		if (filter.str && filter.type == FILTER_FILE_NAME) {
			if (regexec(&regex_exp, ename, 0, NULL, 0) == EXIT_SUCCESS) {
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
			if (conf.show_hidden == 0)
				continue;
		}

		init_fileinfo(n);

		int stat_ok = 1;
		if (fstatat(fd, ename, &attr, virtual_dir == 1 ? 0 : AT_SYMLINK_NOFOLLOW) == -1) {
			stat_ok = 0;
			stats.unstat++;
		}

		/* Filter files according to file type */
		if (stat_ok == 1 && filter.str && filter.type == FILTER_FILE_TYPE
		&& exclude_file_type(attr.st_mode, attr.st_nlink) == EXIT_SUCCESS) {
			/* Decrease counters: the file won't be displayed */
			if (*ename == '.' && stats.hidden > 0)
				stats.hidden--;
			if (stat_ok == 0 && stats.unstat > 0)
				stats.unstat--;
			excluded_files++;
			continue;
		}

#if defined(_DIRENT_HAVE_D_TYPE)
		if (conf.only_dirs == 1 && ent->d_type != DT_DIR
		&& (ent->d_type != DT_LNK || get_link_ref(ename) != S_IFDIR))
#else
		if (stat_ok == 1 && conf.only_dirs == 1 && !S_ISDIR(attr.st_mode)
		&& (!S_ISLNK(attr.st_mode) || get_link_ref(ename) != S_IFDIR))
#endif /* _DIRENT_HAVE_D_TYPE */
			continue;

		if (count > ENTRY_N) {
			count = 0;
			total_dents = n + ENTRY_N;
			file_info = xrealloc(file_info, (total_dents + 2) * sizeof(struct fileinfo));
		}

		file_info[n].name = (char *)xnmalloc(NAME_MAX + 1, sizeof(char));

		if (conf.unicode == 0 || is_utf8_name(ename) == 0) {
			file_info[n].len = xstrsncpy(file_info[n].name, ename, NAME_MAX);
		} else {
			xstrsncpy(file_info[n].name, ename, NAME_MAX);
			file_info[n].len = wc_xstrlen(ename);
		}

#ifdef _NO_ICONS
		file_info[n].icon = (char *)NULL;
		file_info[n].icon_color = df_c;
#endif /* _NO_ICONS */

		if (stat_ok == 1) {
			switch (attr.st_mode & S_IFMT) {
			case S_IFBLK: file_info[n].type = DT_BLK; stats.block_dev++; break;
			case S_IFCHR: file_info[n].type = DT_CHR; stats.char_dev++; break;
			case S_IFDIR: file_info[n].type = DT_DIR; stats.dir++; break;
#ifdef __sun
			case S_IFDOOR: file_info[n].type = DT_DOOR; stats.door++; break;
#endif /* __sun */
			case S_IFIFO: file_info[n].type = DT_FIFO; stats.fifo++; break;
			case S_IFLNK: file_info[n].type = DT_LNK; stats.link++; break;
			case S_IFREG: file_info[n].type = DT_REG; stats.reg++; break;
			case S_IFSOCK: file_info[n].type = DT_SOCK; stats.socket++; break;
			default: file_info[n].type = DT_UNKNOWN; stats.unknown++; break;
			}

			file_info[n].sel = check_seltag(attr.st_dev, attr.st_ino, attr.st_nlink, n);
			file_info[n].inode = ent->d_ino;
			file_info[n].linkn = attr.st_nlink;
			file_info[n].size = FILE_SIZE;
			file_info[n].uid = attr.st_uid;
			file_info[n].gid = attr.st_gid;
			file_info[n].mode = attr.st_mode;

			if (conf.long_view == 1) {
#if defined(_LINUX_XATTR)
				if (prop_fields.xattr == 1 && listxattr(ename, NULL, 0)) {
					file_info[n].xattr = 1;
					have_xattr = 1;
				}
#endif /* _LINUX_XATTR */
				switch (prop_fields.time) {
				case PROP_TIME_ACCESS: file_info[n].ltime = (time_t)attr.st_atime; break;
				case PROP_TIME_CHANGE: file_info[n].ltime = (time_t)attr.st_ctime; break;
				case PROP_TIME_MOD: file_info[n].ltime = (time_t)attr.st_mtime; break;
				default: file_info[n].ltime = (time_t)attr.st_mtime; break;
				}
			}
		} else {
			file_info[n].type = DT_UNKNOWN;
			stats.unknown++;
		}
		file_info[n].dir = (file_info[n].type == DT_DIR) ? 1 : 0;
		file_info[n].symlink = (file_info[n].type == DT_LNK) ? 1 : 0;

		switch (conf.sort) {
		case SATIME:
			file_info[n].time = stat_ok ? (time_t)attr.st_atime : 0;
			break;
#if defined(HAVE_ST_BIRTHTIME) || defined(__BSD_VISIBLE)
		case SBTIME:
# if defined(__OpenBSD__)
			file_info[n].time = stat_ok ? (time_t)attr.__st_birthtim.tv_sec : 0;
# elif !defined(__DragonFly__)
			file_info[n].time = stat_ok ? (time_t)attr.st_birthtime : 0;
# endif /* __OpenBSD__ */
			break;
#elif defined(_STATX)
		case SBTIME: {
			struct statx attx;
			if (statx(AT_FDCWD, ename, AT_SYMLINK_NOFOLLOW, STATX_BTIME, &attx) == -1)
				file_info[n].time = 0;
			else
				file_info[n].time = (time_t)attx.stx_btime.tv_sec;
		} break;
#else
		case SBTIME: file_info[n].time = stat_ok ? (time_t)attr.st_ctime : 0; break;
#endif /* HAVE_ST_BIRTHTIME || __BSD_VISIBLE */
		case SCTIME: file_info[n].time = stat_ok ? (time_t)attr.st_ctime : 0; break;
		case SMTIME: file_info[n].time = stat_ok ? (time_t)attr.st_mtime : 0; break;
		default: file_info[n].time = 0; break;
		}

		switch (file_info[n].type) {
		case DT_DIR: {
#ifndef _NO_ICONS
			if (conf.icons == 1) {
				get_dir_icon(file_info[n].name, (int)n);

				/* If set from the color scheme file */
				if (*dir_ico_c)
					file_info[n].icon_color = dir_ico_c;
			}
#endif /* !_NO_ICONS */
			int daccess = (stat_ok == 1 &&
				check_file_access(attr.st_mode, attr.st_uid, attr.st_gid) == 1);

			file_info[n].filesn = conf.files_counter == 1
				? (count_dir(ename, NO_CPOP) - 2) : 1;

			if (daccess == 0 || file_info[n].filesn < 0) {
				file_info[n].color = nd_c;
#ifndef _NO_ICONS
				file_info[n].icon = ICON_LOCK;
				file_info[n].icon_color = YELLOW;
#endif /* !_NO_ICONS */
			} else {
				file_info[n].color = stat_ok == 1 ? ((attr.st_mode & 01000)
					? ((attr.st_mode & 00002) ? tw_c : st_c)
					: ((attr.st_mode & 00002) ? ow_c
					: (file_info[n].filesn == 0 ? ed_c : di_c)))
					: df_c;
			}

			/* Let's gather some file statistics based on the file type color */
			if (file_info[n].color == tw_c) {
				stats.other_writable++; stats.sticky++;
			} else if (file_info[n].color == ow_c) {
				stats.other_writable++;
			} else
				if (file_info[n].color == st_c) {
					stats.sticky++;
			}
			}

			break;

		case DT_LNK: {
#ifndef _NO_ICONS
			file_info[n].icon = ICON_LINK;
#endif /* !_NO_ICONS */
			if (follow_symlinks == 0) {
				file_info[n].color = ln_c;
				break;
			}

			struct stat attrl;
			if (fstatat(fd, ename, &attrl, 0) == -1) {
				file_info[n].color = or_c;
				file_info[n].xattr = 0;
				stats.broken_link++;
				break;
			}

			char tmp[PATH_MAX]; *tmp = '\0';
			char *ret = (char *)NULL;
			if (conf.color_lnk_as_target == 1)
				ret = realpath(ename, tmp);

			char *lname = (ret && *tmp) ? tmp : ename;
			if (S_ISDIR(attrl.st_mode)) {
				file_info[n].dir = 1;
				file_info[n].filesn = conf.files_counter == 1
					? (count_dir(ename, NO_CPOP) - 2) : 1;

				int dfiles = conf.files_counter == 1
					? (file_info[n].filesn == 2 ? 3
					: file_info[n].filesn) : 3; // 3 == populated

				file_info[n].color = conf.color_lnk_as_target == 1
					? (check_file_access(attrl.st_mode, attrl.st_uid,
					attrl.st_gid) == 0 ? nd_c
					: get_dir_color(lname, attrl.st_mode, attrl.st_nlink,
					dfiles)) : ln_c;
			} else {
				if (conf.color_lnk_as_target == 1)
					get_link_target_color(lname, &attrl, n);
				else
					file_info[n].color = ln_c;
			}
			}
			break;

		case DT_REG: {
#ifdef _LINUX_CAP
			cap_t cap;
#endif /* !_LINUX_CAP */
			/* Do not perform the access check if the user is root */
			if (user.uid != 0 && stat_ok == 1
			&& check_file_access(attr.st_mode, attr.st_uid, attr.st_gid) == 0) {
#ifndef _NO_ICONS
				file_info[n].icon = ICON_LOCK;
				file_info[n].icon_color = YELLOW;
#endif /* !_NO_ICONS */
				file_info[n].color = nf_c;
			} else if (stat_ok == 1 && (attr.st_mode & 04000)) { /* SUID */
				file_info[n].exec = 1;
				stats.exec++; stats.suid++;
				file_info[n].color = su_c;
#ifndef _NO_ICONS
				file_info[n].icon = ICON_EXEC;
#endif /* !_NO_ICONS */
			} else if (stat_ok == 1 && (attr.st_mode & 02000)) { /* SGID */
				file_info[n].exec = 1;
				stats.exec++; stats.sgid++;
				file_info[n].color = sg_c;
#ifndef _NO_ICONS
				file_info[n].icon = ICON_EXEC;
#endif /* !_NO_ICONS */
			}

#ifdef _LINUX_CAP
			else if (check_cap == 1 && (cap = cap_get_file(ename))) {
				file_info[n].color = ca_c;
				stats.caps++;
				cap_free(cap);
			}
#endif /* !_LINUX_CAP */

			else if (stat_ok == 1
			&& ((attr.st_mode & 00100) // Exec
			|| (attr.st_mode & 00010) || (attr.st_mode & 00001))) {
/*			&& (((attr.st_mode & 00100) && attr.st_uid == user.uid) // Exec
			|| ((attr.st_mode & 00010) && attr.st_gid == user.gid)
			|| (attr.st_mode & 00001))) { */
				file_info[n].exec = 1;
				stats.exec++;
#ifndef _NO_ICONS
				file_info[n].icon = ICON_EXEC;
#endif /* !_NO_ICONS */
				if (file_info[n].size == 0)
					file_info[n].color = ee_c;
				else
					file_info[n].color = ex_c;
			} else if (file_info[n].size == 0) {
				file_info[n].color = ef_c;
			} else if (file_info[n].linkn > 1) { /* Multi-hardlink */
				file_info[n].color = mh_c;
				stats.multi_link++;
			} else { /* Regular file */
				file_info[n].color = fi_c;
			}

#ifndef _NO_ICONS
			/* The icons check precedence order is this:
			 * 1. filename or filename.extension
			 * 2. extension
			 * 3. file type */
			/* Check icons for specific file names */
			int name_icon_found = 0;
			if (conf.icons == 1)
				name_icon_found = get_name_icon(file_info[n].name, (int)n);
#endif /* !_NO_ICONS */

			/* Check file extension (only if accessible, has no capabilities
			 * and is not executable) */
			char *ext = (char *)NULL;
			if (check_ext == 1 && file_info[n].color != nf_c
			&& file_info[n].color != ca_c && file_info[n].exec != 1)
				ext = strrchr(file_info[n].name, '.');
			/* Make sure not to take a hidden file for a file extension */
			if (!ext || ext == file_info[n].name || !*(ext + 1))
				break;
			else
				file_info[n].ext_name = ext;
#ifndef _NO_ICONS
			if (conf.icons == 1 && name_icon_found == 0)
				get_ext_icon(ext, (int)n);
#endif /* !_NO_ICONS */
			char *extcolor = get_ext_color(ext);
			if (!extcolor)
				break;

			size_t len = strlen(extcolor) + 4;
			file_info[n].ext_color = (char *)xnmalloc(len, sizeof(char));
			snprintf(file_info[n].ext_color, len, "\x1b[%sm", extcolor);
			file_info[n].color = file_info[n].ext_color;
			extcolor = (char *)NULL;
		} /* End of DT_REG block */
		break;

		case DT_SOCK: file_info[n].color = so_c; break;
		case DT_FIFO: file_info[n].color = pi_c; break;
		case DT_BLK: file_info[n].color = bd_c; break;
		case DT_CHR: file_info[n].color = cd_c; break;
#ifdef __sun
		case DT_DOOR: file_info[n].color = oo_c; break;
#endif /* __sun */
		case DT_UNKNOWN: file_info[n].color = uf_c; break;
		default: file_info[n].color = df_c; break;
		}

#ifndef _NO_ICONS
		if (xargs.icons_use_file_color == 1 && conf.icons == 1)
			file_info[n].icon_color = file_info[n].color;
#endif /* !_NO_ICONS */
		if (conf.long_view == 1 && stat_ok == 1)
			set_long_attribs((int)n, &attr);

		if (xargs.disk_usage_analyzer == 1)
			get_largest(n, &largest_size, &largest_name, &largest_color, &total_size);

		n++;
		if (n > (unsigned int)INT_MAX) {
			n--;
			_err('w', PRINT_PROMPT, _("%s: Integer overflow "
				"detected (showing only %u files)\n"), PROGRAM_NAME, n);
			break;
		}

		count++;
	}

/*	unsigned int tdents = total_dents > 0 ? total_dents : ENTRY_N + 2;
	if (tdents > n)
		file_info = xrealloc(file_info, (n + 1) * sizeof(struct fileinfo)); */

	if (xargs.disk_usage_analyzer == 1 || (conf.long_view == 1
	&& conf.full_dir_size == 1))
		putchar('\r'); /* Erase the "Retrieveing file sizes" message */

	file_info[n].name = (char *)NULL;
	files = n;

	if (n == 0) {
		printf("%s. ..%s\n", conf.colorize ? di_c : df_c, df_c);
		free(file_info);
		goto END;
	}

	int pad = (max_files != UNSET && (int)files > max_files)
		? DIGINUM(max_files) : DIGINUM(files);

		/* #############################################
		 * #    SORT FILES ACCORDING TO SORT METHOD    #
		 * ############################################# */

	if (conf.sort != SNONE)
		ENTSORT(file_info, n, entrycmp);

		/* ##########################################
		 * #    GET INFO TO PRINT COLUMNED OUTPUT   #
		 * ########################################## */

	size_t counter = 0;
	size_t columns_n = 1;

	/* Get the longest file name */
	if (conf.columned == 1 || conf.long_view == 1) {
		int nn = (int)n;
		get_longest_filename(nn, pad);
	}

				/* ########################
				 * #    LONG VIEW MODE    #
				 * ######################## */

	if (conf.long_view == 1) {
		print_long_mode(&counter, &reset_pager, pad,
			prop_fields.ids == 1 ? get_max_ug_str() : 0,
			prop_fields.inode == 1 ? get_longest_inode() : 0, have_xattr);
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
	exit_code = post_listing(dir, close_dir, reset_pager);
	if (virtual_dir == 1)
		print_reload_msg(_("Virtual directory\n"));
	if (excluded_files > 0)
		printf(_("Excluded files: %d\n"), excluded_files);

	if (xargs.disk_usage_analyzer == 1 && conf.long_view == 1
	&& conf.full_dir_size == 1)
		print_analysis_stats(total_size, largest_size, largest_color, largest_name);

#ifdef _LIST_SPEED
	clock_t end = clock();
	printf("list_dir time: %f\n", (double)(end - start) / CLOCKS_PER_SEC);
#endif /* _LIST_SPEED */

	return exit_code;
}

void
free_dirlist(void)
{
	if (!file_info || files == 0)
		return;

	int i = (int)files;
	while (--i >= 0) {
		free(file_info[i].name);
		if (file_info[i].ext_color)
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
#endif

	free_dirlist();
	int bk = exit_code;
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
