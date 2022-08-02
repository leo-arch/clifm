/* listing.c -- functions controlling what is listed on the screen */

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

#include <stdio.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef __linux__
# include <sys/capability.h>
#endif
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#ifdef __OpenBSD__
# include <strings.h>
#endif

#include <glob.h>

#ifdef _LIST_SPEED
# include <time.h>
#endif

#ifdef TOURBIN_QSORT
# include "qsort.h"
# define ENTLESS(i, j) (entrycmp(file_info + (i), file_info + (j)) < 0)
# define ENTSWAP(i, j) (swap_ent((i), (j)))
# define ENTSORT(file_info, n, entrycmp) QSORT((n), ENTLESS, ENTSWAP)
#else
# define ENTSORT(file_info, n, entrycmp) qsort((file_info), (n), sizeof(*(file_info)), (entrycmp))
#endif

#include "aux.h"
#include "colors.h"
#include "misc.h"
#include "properties.h"
#include "sort.h"
#include "strings.h"
#include "checks.h"
#include "exec.h"
#include "autocmds.h"

#ifndef _NO_ICONS
# include "icons.h"
#endif

#ifdef _PALAND_PRINTF
# include "printf.h"
#define xprintf printf_
#else
# define xprintf printf
#endif

#include <readline/readline.h>

/* Macros for run_dir_cmd function */
#define DIR_IN 0
#define DIR_OUT 1
#define DIR_IN_NAME ".cfm.in"
#define DIR_OUT_NAME ".cfm.out"

/* Struct to store information about trimmed file names. Used only when
 * Unicode is disabled */
struct trim_t {
	int state;
	int a; /* trimmed char */
	int b; /* char next to trimmed char */
	int pad;
	size_t len; /* Lenght of file name before trimming */
};

struct trim_t trim;

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
	switch(cur_ws) {
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
	if (colorize == 1)
		set_div_line_color();

	if (!*div_line) { /* Let's draw the line with bow drawing chars */
		fputs("\x1b(0m", stdout);
		int k = 0;
		for (; k < (int)term_cols - 2; k++)
			putchar('q');
		fputs("\x1b(0j\x1b(B", stdout);
	} else if (*div_line == '0' && !div_line[1]) {
		/* No line */
		putchar('\n');
	} else {
		/* Custom line */
		size_t len = wc_xstrlen(div_line);
		if (len <= 2) {
			/* Extend DIV_LINE to the end of the screen */
			int i;
			for (i = (int)(term_cols / len); i; i--)
				fputs(div_line, stdout);
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

	char *free_space = get_size_unit((off_t)(stat.f_frsize * stat.f_bavail));
	char *size = get_size_unit((off_t)(stat.f_blocks * stat.f_frsize));

	print_reload_msg("%s/%s free\n",
		free_space ? free_space : "?", size ? size : "?");

	free(free_space);
	free(size);

	return;
}

static void
_print_selfiles(unsigned short t_rows)
{
	int limit = max_printselfiles;

	if (max_printselfiles == 0) {
		/* Never take more than half terminal height */
		limit = (t_rows / 2) - 4;
		/* 4 = 2 div lines, 2 prompt lines */
		if (limit <= 0)
			limit = 1;
	}

	if (limit > (int)sel_n)
		limit = (int)sel_n;

	int i;
	for (i = 0; i < (max_printselfiles != UNSET ? limit : (int)sel_n); i++)
		colors_list(sel_elements[i].name, 0, NO_PAD, PRINT_NEWLINE);

	if (max_printselfiles != UNSET && limit < (int)sel_n)
		printf("%d/%zu\n", i, sel_n);

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
get_name_icon(const char *file, int n)
{
	if (!file)
		return 0;

	int i = (int)(sizeof(icon_filenames) / sizeof(struct icons_t));
	while (--i >= 0) {
		if (TOUPPER(*file) == TOUPPER(*icon_filenames[i].name)
		&& strcasecmp(file, icon_filenames[i].name) == 0) {
			file_info[n].icon = icon_filenames[i].icon;
			file_info[n].icon_color = icon_filenames[i].color;
			return 1;
		}
	}

	return 0;
}

/* Set the icon field to the corresponding icon for DIR. If not found,
 * set the default icon */
static void
get_dir_icon(const char *dir, int n)
{
	/* Default values for directories */
	file_info[n].icon = DEF_DIR_ICON;
	file_info[n].icon_color = DEF_DIR_ICON_COLOR;

	if (!dir)
		return;

	int i = (int)(sizeof(icon_dirnames) / sizeof(struct icons_t));
	while (--i >= 0) {
		if (TOUPPER(*dir) == TOUPPER(*icon_dirnames[i].name)
		&& strcasecmp(dir, icon_dirnames[i].name) == 0) {
			file_info[n].icon = icon_dirnames[i].icon;
			file_info[n].icon_color = icon_dirnames[i].color;
			break;
		}
	}
}

/* Set the icon field to the corresponding icon for EXT. If not found,
 * set the default icon */
static void
get_ext_icon(const char *restrict ext, int n)
{
	if (!file_info[n].icon) {
		file_info[n].icon = DEF_FILE_ICON;
		file_info[n].icon_color = DEF_FILE_ICON_COLOR;
	}

	if (!ext)
		return;

	ext++;

	int i = (int)(sizeof(icon_ext) / sizeof(struct icons_t));
	while (--i >= 0) {
		/* Tolower */
		char c = (*ext >= 'A' && *ext <= 'Z') ? (char)(*ext - 'A' + 'a') : *ext;
		if (c == *icon_ext[i].name && strcasecmp(ext, icon_ext[i].name) == 0) {
			file_info[n].icon = icon_ext[i].icon;
			file_info[n].icon_color = icon_ext[i].color;
			break;
		}
	}
}
#endif /* _NO_ICONS */

static int
post_listing(DIR *dir, const int close_dir, const int reset_pager)
{
	if (xargs.list_and_quit != 1)
		fputs(UNHIDE_CURSOR, stdout);
	if (close_dir && closedir(dir) == -1)
		return EXIT_FAILURE;

	dir_changed = 0;

	if (xargs.list_and_quit == 1)
		exit(exit_code);
	if (reset_pager)
		pager = 1;

	if (max_files != UNSET && (int)files > max_files)
		printf("%d/%zu\n", max_files, files);

	print_div_line();

	if (dirhist_map) { /* Print current, previous, and next entries */
		print_dirhist_map();
		print_div_line();
	}

	if (disk_usage)
		print_disk_usage();
	if (sort_switch) {
		print_reload_msg(_("Sorted by "));
		print_sort_method();
	}

	if (switch_cscheme)
		printf(_("Color scheme %s->%s %s\n"), mi_c, df_c, cur_cscheme);

	if (print_selfiles && sel_n > 0)
		_print_selfiles(term_rows);

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
		int l = (int)term_rows - 5;
		MOVE_CURSOR_DOWN(l);
//		printf("\x1b[%dB", l);
		fputs(PAGER_LABEL, stdout);

		xgetchar();
		CLEAR;

		if (columns_n == -1) { /* Long view */
			*i = 0;
		} else { /* Normal view */
			if (listing_mode == HORLIST)
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
		pager = 0, *reset_pager = 1;
		break;

	/* If another key is pressed, go back one position.
	 * Otherwise, some file names won't be listed.*/
	default:
		fputs("\r\x1b[K\x1b[3J", stdout);
		return (-1);
	}

	fputs("\r\x1b[K\x1b[3J", stdout);
	return 0;
}

static void
set_events_checker(void)
{
#ifdef LINUX_INOTIFY
	reset_inotify();

#elif defined(BSD_KQUEUE)
	if (event_fd >= 0) {
		close(event_fd);
		event_fd = -1;
		watch = 0;
	}
#if defined(O_EVTONLY)
	event_fd = open(workspaces[cur_ws].path, O_EVTONLY);
#else
	event_fd = open(workspaces[cur_ws].path, O_RDONLY);
#endif
	if (event_fd >= 0) {
		/* Prepare for events */
		EV_SET(&events_to_monitor[0], event_fd, EVFILT_VNODE,
				EV_ADD | EV_CLEAR, KQUEUE_FFLAGS, 0, workspaces[cur_ws].path);
		watch = 1;
		/* Register events */
		kevent(kq, events_to_monitor, NUM_EVENT_SLOTS, NULL, 0, NULL);

	}
#endif /* LINUX_INOTIFY */
}

static void
get_longest_filename(const int n, const int pad)
{
	int i = (max_files != UNSET && max_files < n) ? max_files : n;

	while (--i >= 0) {
		size_t total_len = 0;
		file_info[i].eln_n = no_eln ? -1 : DIGINUM(i + 1);

		size_t blen = file_info[i].len;
		if (file_info[i].len > (size_t)max_name_len)
			file_info[i].len = (size_t)max_name_len;

		if (long_view && min_name_trim != UNSET
		&& file_info[i].len < (size_t)min_name_trim)
			file_info[i].len = (size_t)min_name_trim;

		total_len = (size_t)pad + 1 + file_info[i].len;

		file_info[i].len = blen;

		if (!long_view && classify) {
			if (file_info[i].dir)
				total_len++;

			if (file_info[i].filesn > 0 && files_counter)
				total_len += DIGINUM(file_info[i].filesn);

			if (!file_info[i].dir && !colorize) {
				switch (file_info[i].type) {
				case DT_REG:
					if (file_info[i].exec)
						total_len++;
					break;
				case DT_LNK:  /* fallthrough */
				case DT_SOCK: /* fallthrough */
				case DT_FIFO: /* fallthrough */
				case DT_UNKNOWN: total_len++; break;
				default: break;
				}
			}
		}

		if (total_len > longest) {
			if (listing_mode == VERTLIST) {
				longest = total_len;
			} else {
				if (max_files == UNSET) {
					longest = total_len;
				} else {
					if (i < max_files)
						longest = total_len;
				}
			}
		}
	}
#ifndef _NO_ICONS
	if (icons && !long_view && columned)
		longest += 3;
#endif
}

/* Set a few extra properties needed for long view mode */
static void
set_long_attribs(const int n, const struct stat *attr)
{
	file_info[n].uid = attr->st_uid;
	file_info[n].gid = attr->st_gid;
	file_info[n].mode = attr->st_mode;
	file_info[n].rdev = attr->st_rdev;

	switch(prop_fields.time) {
	case PROP_TIME_ACCESS: file_info[n].ltime = (time_t)attr->st_atime; break;
	case PROP_TIME_CHANGE: file_info[n].ltime = (time_t)attr->st_ctime; break;
	case PROP_TIME_MOD: file_info[n].ltime = (time_t)attr->st_mtime; break;
	default: file_info[n].ltime = (time_t)attr->st_mtime; break;
	}

	if (full_dir_size == 1 && file_info[n].dir == 1) {
		char name[PATH_MAX]; *name = '\0';
		if (file_info[n].type == DT_LNK) /* Symlink to directory */
			snprintf(name, sizeof(name), "%s/", file_info[n].name);
		file_info[n].size = dir_size(*name ? name : file_info[n].name);
		if (file_info[n].type == DT_DIR && attr->st_nlink == 2 && file_info[n].size == 4)
			file_info[n].size = 0; /* Empty directory */
	} else {
		file_info[n].size = FILE_SIZE_PTR;
	}
}

static void
print_long_mode(size_t *counter, int *reset_pager, const int pad, size_t ug_max, const size_t ino_max)
{
	struct stat lattr;
	int space_left = (int)term_cols - MAX_PROP_STR;

	if (space_left < min_name_trim)
		space_left = min_name_trim;

	if ((int)longest < space_left)
		space_left = (int)longest;

	int i, k = (int)files;
	for (i = 0; i < k; i++) {
		if (max_files != UNSET && i == max_files)
			break;
		if (lstat(file_info[i].name, &lattr) == -1)
			continue;

		if (pager) {
			int ret = 0;
			if (*counter > (size_t)(term_rows - 2))
				ret = run_pager(-1, reset_pager, &i, counter);
			if (ret == -1 || ret == -2) {
				--i;
				if (ret == -2)
					*counter = 0;
				continue;
			}
			(*counter)++;
		}

		if (!no_eln) /* Print ELN */
			printf("%s%*d%s%s%c%s", el_c, pad, i + 1, df_c,
				li_cb, file_info[i].sel ? SELFILE_CHR : ' ', df_c);
		/* Print the remaining part of the entry */
		print_entry_props(&file_info[i], (size_t)space_left, ug_max, ino_max);
	}
}

static void
get_columns(size_t *n)
{
	*n = (size_t)term_cols / (longest + 1); /* +1 for the
	space between file names */

	/* If longest is bigger than terminal columns, columns_n will
	 * be negative or zero. To avoid this: */
	if (*n < 1)
		*n = 1;

	/* If we have only three files, we don't want four columns */
	if (*n > files)
		*n = files;
}

static void
print_entry_color(int *ind_char, const int i, const int pad)
{
	*ind_char = 0;
	char *end_color = df_c;
	if (file_info[i].dir)
		end_color = fc_c;

	char *wname = (char *)NULL;
	/* wc_xstrlen returns 0 if a non-printable char was found in the file name */
	if (file_info[i].len == 0) {
		wname = truncate_wname(file_info[i].name);
		file_info[i].len = wc_xstrlen(wname);
	}

	char *n = wname ? wname : file_info[i].name;
	int _trim = 0, diff = 0;
	char tname[NAME_MAX * sizeof(wchar_t)];
	if (unicode && max_name_len != UNSET && !long_view
	&& (int)file_info[i].len > max_name_len) {
		_trim = 1;
		xstrsncpy(tname, wname ? wname : file_info[i].name,
				(NAME_MAX * sizeof(wchar_t)) - 1);
		diff = u8truncstr(tname, (size_t)max_name_len - 1);
		file_info[i].len = (size_t)max_name_len;
		n = tname;
	}

	char trim_diff[8];
	*trim_diff = '\0';
	if (diff)
		snprintf(trim_diff, 8, "\x1b[%dC", diff);

#ifndef _NO_ICONS
	if (icons) {
		if (no_eln) {
			if (_trim) {
				xprintf("%s%c%s%s%s %s%ls%s\x1b[0m%s%c\x1b[0m%s",
					li_cb, file_info[i].sel ? SELFILE_CHR : ' ', df_c,
					file_info[i].icon_color, file_info[i].icon,
					file_info[i].color, (wchar_t *)n, trim_diff,
					tt_c, TRIMFILE_CHR, end_color);
			} else {
				xprintf("%s%c%s%s%s %s%s%s",
					li_cb, file_info[i].sel ? SELFILE_CHR : ' ', df_c,
					file_info[i].icon_color, file_info[i].icon,
					file_info[i].color, n, end_color);
			}
		} else {
			if (_trim) {
				xprintf("%s%*d%s%s%c%s%s%s %s%ls%s\x1b[0m%s%c\x1b[0m%s",
					el_c, pad, i + 1, df_c, li_cb, file_info[i].sel ? SELFILE_CHR : ' ',
					df_c, file_info[i].icon_color, file_info[i].icon,
					file_info[i].color, (wchar_t *)n, trim_diff,
					tt_c, TRIMFILE_CHR, end_color);
			} else {
				xprintf("%s%*d%s%s%c%s%s%s %s%s%s", el_c, pad, i + 1, df_c,
					li_cb, file_info[i].sel ? SELFILE_CHR : ' ', df_c,
					file_info[i].icon_color, file_info[i].icon,
					file_info[i].color, n, end_color);
			}
		}
	} else
#endif
	{
		if (no_eln) {
			if (_trim) {
				xprintf("%s%c%s%s%ls%s\x1b[0m%s%c\x1b[0m%s",
					li_cb, sel_n ? (file_info[i].sel ? SELFILE_CHR : ' ') : 0, df_c,
					file_info[i].color, (wchar_t *)n, trim_diff,
					tt_c, TRIMFILE_CHR, end_color);
			} else {
				xprintf("%s%c%s%s%s%s",
					li_cb, sel_n ? (file_info[i].sel ? SELFILE_CHR : ' ') : 0, df_c,
					file_info[i].color, n, end_color);
			}
		} else {
			if (_trim) {
				xprintf("%s%*d%s%s%c%s%s%ls%s\x1b[0m%s%c\x1b[0m%s",
					el_c, pad, i + 1, df_c, li_cb, file_info[i].sel ? SELFILE_CHR : ' ',
					df_c, file_info[i].color, (wchar_t *)n,
					trim_diff, tt_c, TRIMFILE_CHR, end_color);
			} else {
				xprintf("%s%*d%s%s%c%s%s%s%s", el_c, pad, i + 1, df_c,
					li_cb, file_info[i].sel ? SELFILE_CHR : ' ', df_c,
					file_info[i].color, n, end_color);
			}
		}
	}

	if (classify) {
		/* Append directory indicator and files counter */
		switch (file_info[i].type) {
		case DT_DIR:
			putchar(DIR_CHR);
			if (file_info[i].filesn > 0 && files_counter)
				fputs(xitoa(file_info[i].filesn), stdout);
			break;

		case DT_LNK:
			if (file_info[i].dir)
				putchar(DIR_CHR);
			if (file_info[i].filesn > 0 && files_counter)
				fputs(xitoa(file_info[i].filesn), stdout);
			break;
		}
	}

	if (end_color == fc_c)
		fputs(df_c, stdout);

	free(wname);
}

static void
print_entry_nocolor(int *ind_char, const int i, const int pad)
{
	char *wname = (char *)NULL;
	/* wc_strlen returns 0 if a non-printable char was found in the file name */
	if (file_info[i].len == 0) {
		wname = truncate_wname(file_info[i].name);
		file_info[i].len = wc_xstrlen(wname);
	}

	char *n = wname ? wname : file_info[i].name;
	int _trim = 0, diff = 0;
	char tname[NAME_MAX * sizeof(wchar_t)];
	if (unicode && max_name_len != UNSET && !long_view
	&& (int)file_info[i].len > max_name_len) {
		_trim = 1;
		xstrsncpy(tname, wname ? wname : file_info[i].name,
			(NAME_MAX * sizeof(wchar_t)) - 1);
		diff = u8truncstr(tname, (size_t)max_name_len - 1);
		file_info[i].len = (size_t)max_name_len;
		n = tname;
	}

	char trim_diff[8];
	*trim_diff = '\0';
	if (diff)
		snprintf(trim_diff, 8, "\x1b[%dC", diff);

#ifndef _NO_ICONS
	if (icons) {
		if (no_eln) {
			if (_trim) {
				xprintf("%c%s %ls%s%c", file_info[i].sel ? SELFILE_CHR : ' ',
					file_info[i].icon, (wchar_t *)n, trim_diff, TRIMFILE_CHR);
			} else {
				xprintf("%c%s %s", file_info[i].sel ? SELFILE_CHR : ' ',
				file_info[i].icon, n);
			}
		} else {
			if (_trim) {
				xprintf("%s%*d%s%c%s %ls%s%c", el_c, pad, i + 1, df_c,
					file_info[i].sel ? SELFILE_CHR : ' ', file_info[i].icon,
					(wchar_t *)n, trim_diff, TRIMFILE_CHR);
			} else {
				xprintf("%s%*d%s%c%s %s", el_c, pad, i + 1, df_c,
					file_info[i].sel ? SELFILE_CHR : ' ', file_info[i].icon, n);
			}
		}
	} else
#endif /* _NO_ICONS */
	{
		if (no_eln) {
			if (_trim) {
				xprintf("%c%ls%s%c", file_info[i].sel ? SELFILE_CHR : ' ',
					(wchar_t *)n, trim_diff, TRIMFILE_CHR);
			} else {
				xprintf("%c%s", file_info[i].sel ? SELFILE_CHR : ' ', n);
			}
		} else {
			if (_trim) {
				xprintf("%s%*d%s%c%ls%s%c", el_c, pad, i + 1, df_c,
					file_info[i].sel ? SELFILE_CHR : ' ', (wchar_t *)n,
					trim_diff, TRIMFILE_CHR);
			} else {
				xprintf("%s%*d%s%c%s", el_c, pad, i + 1, df_c,
					file_info[i].sel ? SELFILE_CHR : ' ', n);
			}
		}
	}

	if (classify) {
		/* Append file type indicator */
		switch (file_info[i].type) {
		case DT_DIR:
			*ind_char = 0;
			putchar(DIR_CHR);
			if (file_info[i].filesn > 0 && files_counter)
				fputs(xitoa(file_info[i].filesn), stdout);
			break;

		case DT_LNK:
			if (file_info[i].dir) {
				*ind_char = 0;
				putchar(DIR_CHR);
				if (file_info[i].filesn > 0 && files_counter)
					fputs(xitoa(file_info[i].filesn), stdout);
			} else {
				putchar(LINK_CHR);
			}
			break;

		case DT_REG:
			if (file_info[i].exec)
				putchar(EXEC_CHR);
			else
				*ind_char = 0;
			break;

		case DT_FIFO: putchar(FIFO_CHR); break;
		case DT_SOCK: putchar(SOCK_CHR); break;
		case DT_UNKNOWN: putchar(UNKNOWN_CHR); break;
		default: *ind_char = 0;
		}
	}

	free(wname);
}

/* Pad the current file name to equate the longest file name length */
static void
pad_filename(int *ind_char, const int i, const int pad)
{
	int cur_len = 0;

#ifndef _NO_ICONS
	cur_len = pad + 1 + (icons ? 3 : 0) + (int)file_info[i].len + (*ind_char ? 1 : 0);
#else
	cur_len = pad + 1 + (int)file_info[i].len + (*ind_char ? 1 : 0);
#endif

	if (file_info[i].dir && classify) {
		cur_len++;
		if (file_info[i].filesn > 0 && files_counter && file_info[i].ruser)
			cur_len += DIGINUM((int)file_info[i].filesn);
	}

	int diff = (int)longest - cur_len;
	if (xargs.list_and_quit == 1) {
		int j = diff + 1;
		while(--j >= 0)
			putchar(' ');
	} else {
		MOVE_CURSOR_RIGHT(diff + 1);
	}
}

static void
print_entry_color_light(int *ind_char, const int i, const int pad)
{
	*ind_char = 0;
	char *end_color = df_c;
	if (file_info[i].dir)
		end_color = fc_c;

	char *wname = (char *)NULL;
	/* wc_strlen returns 0 if a non-printable char was found in the file name */
	if (file_info[i].len == 0) {
		wname = truncate_wname(file_info[i].name);
		file_info[i].len = wc_xstrlen(wname);
	}

	char *n = wname ? wname : file_info[i].name;

	int _trim = 0, diff = 0;
	char tname[NAME_MAX * sizeof(wchar_t)];
	if (unicode && max_name_len != UNSET && !long_view
	&& (int)file_info[i].len > max_name_len) {
		_trim = 1;
		xstrsncpy(tname, wname ? wname : file_info[i].name,
				(NAME_MAX * sizeof(wchar_t)) - 1);
		diff = u8truncstr(tname, (size_t)max_name_len - 1);
		file_info[i].len = (size_t)max_name_len;
		n = tname;
	}

	char trim_diff[8];
	*trim_diff = '\0';
	if (diff)
		snprintf(trim_diff, 8, "\x1b[%dC", diff);

#ifndef _NO_ICONS
	if (icons) {
		if (xargs.icons_use_file_color == 1)
			file_info[i].icon_color = file_info[i].color;

		if (no_eln) {
			if (_trim) {
				xprintf("%s%s %s%ls%s\x1b[0m%s%c\x1b[0m%s",
					file_info[i].icon_color, file_info[i].icon,
					file_info[i].color, (wchar_t *)n, trim_diff,
					tt_c, TRIMFILE_CHR, end_color);
			} else {
				xprintf("%s%s %s%s%s", file_info[i].icon_color,
					file_info[i].icon, file_info[i].color, n, end_color);
			}
		} else {
			if (_trim) {
				xprintf("%s%*d%s %s%s %s%ls%s\x1b[0m%s%c\x1b[0m%s",
					el_c, pad, i + 1, df_c,	file_info[i].icon_color,
					file_info[i].icon, file_info[i].color, (wchar_t *)n,
					trim_diff, tt_c, TRIMFILE_CHR, end_color);
			} else {
				xprintf("%s%*d%s %s%s %s%s%s", el_c, pad, i + 1, df_c,
					file_info[i].icon_color, file_info[i].icon,
					file_info[i].color, n, end_color);
			}
		}
	} else
#endif /* _NO_ICONS */
	{
		if (no_eln) {
			if (_trim) {
				xprintf("%s%ls%s\x1b[0m%s%c\x1b[0m%s",
					file_info[i].color, (wchar_t *)n,
					trim_diff, tt_c, TRIMFILE_CHR, end_color);
			} else {
				xprintf("%s%s%s", file_info[i].color, n, end_color);
			}
		} else {
			if (_trim) {
				xprintf("%s%*d%s %s%ls%s\x1b[0m%s%c\x1b[0m%s",
					el_c, pad, i + 1, df_c,	file_info[i].color, (wchar_t *)n,
					trim_diff, tt_c, TRIMFILE_CHR, end_color);
			} else {
				xprintf("%s%*d%s %s%s%s", el_c, pad, i + 1, df_c,
					file_info[i].color, n, end_color);
			}
		}
	}

	if (file_info[i].dir && classify) {
		putchar(DIR_CHR);
		if (file_info[i].filesn > 0 && files_counter)
			fputs(xitoa(file_info[i].filesn), stdout);
	}

	if (end_color == fc_c)
		fputs(df_c, stdout);

	free(wname);
}

static void
print_entry_nocolor_light(int *ind_char, const int i, const int pad)
{
	char *wname = (char *)NULL;
	/* wc_strlen returns 0 if a non-printable char was found in the file name */
	if (file_info[i].len == 0) {
		wname = truncate_wname(file_info[i].name);
		file_info[i].len = wc_xstrlen(wname);
	}

	char *n = wname ? wname : file_info[i].name;
	int _trim = 0, diff = 0;
	char tname[NAME_MAX * sizeof(wchar_t)];
	if (unicode && max_name_len != UNSET && !long_view
	&& (int)file_info[i].len > max_name_len) {
		_trim = 1;
		xstrsncpy(tname, wname ? wname : file_info[i].name,
				(NAME_MAX * sizeof(wchar_t)) - 1);
		diff = u8truncstr(tname, (size_t)max_name_len - 1);
		file_info[i].len = (size_t)max_name_len;
		n = tname;
	}

	char trim_diff[8];
	*trim_diff = '\0';
	if (diff)
		snprintf(trim_diff, 8, "\x1b[%dC", diff);

#ifndef _NO_ICONS
	if (icons) {
		if (no_eln)
			if (_trim) {
				xprintf("%s %ls%s%c", file_info[i].icon, (wchar_t *)n,
					trim_diff, TRIMFILE_CHR);
			} else {
				xprintf("%s %s", file_info[i].icon, n);
			}
		else {
			if (_trim) {
				xprintf("%s%*d%s %s %ls%s%c", el_c, pad, i + 1, df_c,
					file_info[i].icon, (wchar_t *)n, trim_diff, TRIMFILE_CHR);
			} else {
				xprintf("%s%*d%s %s %s", el_c, pad, i + 1, df_c,
					file_info[i].icon, n);
			}
		}
	} else
#endif /* _NO_ICONS */
	{
		if (no_eln)
			if (_trim) {
				printf("%ls%s%c", (wchar_t *)file_info[i].name,
					trim_diff, TRIMFILE_CHR);
			} else {
				fputs(file_info[i].name, stdout);
			}
		else {
			if (_trim) {
				xprintf("%s%*d%s %ls%s%c", el_c, pad, i + 1, df_c,
					(wchar_t *)n, trim_diff, TRIMFILE_CHR);
			} else {
				xprintf("%s%*d%s %s", el_c, pad, i + 1, df_c, n);
			}
		}
	}

	if (classify) {
		switch (file_info[i].type) {
		case DT_DIR:
			*ind_char = 0;
			putchar(DIR_CHR);
			if (file_info[i].filesn > 0 && files_counter)
				fputs(xitoa(file_info[i].filesn), stdout);
			break;

		case DT_FIFO: putchar(FIFO_CHR); break;
		case DT_LNK: putchar(LINK_CHR); break;
		case DT_SOCK: putchar(SOCK_CHR); break;
		case DT_UNKNOWN: putchar(UNKNOWN_CHR); break;
		default: *ind_char = 0; break;
		}
	}

	free(wname);
}

/* Add spaces needed to equate the longest file name length */
static void
pad_filename_light(int *ind_char, const int i, const int pad)
{
	int cur_len = 0;
#ifndef _NO_ICONS
	cur_len = pad + 1 + (icons ? 3 : 0)	+ (int)file_info[i].len + (*ind_char ? 1 : 0);
#else
	cur_len = pad + 1 + (int)file_info[i].len + (*ind_char ? 1 : 0);
#endif

	if (classify) {
		if (file_info[i].dir)
			cur_len++;
		if (file_info[i].filesn > 0 && files_counter
		&& file_info[i].ruser)
			cur_len += DIGINUM((int)file_info[i].filesn);
	}

	int diff = (int)longest - cur_len;
	if (xargs.list_and_quit == 1) {
		int j = diff + 1;
		while(--j >= 0)
			putchar(' ');
	} else {
		MOVE_CURSOR_RIGHT(diff + 1);
	}
}

/* Trim and untrim file names when current file name length exceeds
 * man file name length. Only used when Unicode is disabled */
static void
trim_filename(int i)
{
	trim.state = 1;
	trim.a = file_info[i].name[max_name_len - 1];
	trim.b = file_info[i].name[max_name_len];
	trim.len = file_info[i].len;
	file_info[i].name[max_name_len - 1] = TRIMFILE_CHR;
	file_info[i].name[max_name_len] = '\0';
	file_info[i].len = (size_t)max_name_len;
}

static void
untrim_filename(int i)
{
	file_info[i].len = trim.len;
	file_info[i].name[max_name_len - 1] = (char)trim.a;
	file_info[i].name[max_name_len] = (char)trim.b;
	trim.state = trim.a = trim.b = 0;
	trim.len = 0;
}

/* List files horizontally:
 * 1 AAA	2 AAB	3 AAC
 * 4 AAD	5 AAE	6 AAF */
static void
list_files_horizontal(size_t *counter, int *reset_pager, const int pad,
		const size_t columns_n)
{
	int nn = (max_files != UNSET && max_files < (int)files) ? max_files : (int)files;

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
		if (!classify)
			ind_char = 0;

				/* ##########################
				 * #  MAS: A SIMPLE PAGER   #
				 * ########################## */

		if (pager) {
			/* Run the pager only once all columns and rows fitting in
			 * the screen are filled with the corresponding file names */
			int ret = 0, bi = i;
			if (blc && *counter > columns_n * ((size_t)term_rows - 2))
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

		/* Trim file name to MAX_NAME_LEN */
		if (!unicode && max_name_len != UNSET && !long_view
		&& (int)file_info[i].len > max_name_len)
			trim_filename(i);

		file_info[i].eln_n = no_eln ? -1 : DIGINUM(i + 1);

		if (colorize) {
			if (light_mode)
				print_entry_color_light(&ind_char, i, pad);
			else
				print_entry_color(&ind_char, i, pad);
		} else {
			if (light_mode)
				print_entry_nocolor_light(&ind_char, i, pad);
			else
				print_entry_nocolor(&ind_char, i, pad);
		}

		if (!last_column) {
			if (light_mode)
				pad_filename_light(&ind_char, i, pad);
			else
				pad_filename(&ind_char, i, pad);
		} else {
			putchar('\n');
		}

		if (!unicode && trim.state == 1)
			untrim_filename(i);
	}

	if (!last_column)
		putchar('\n');
}

/* List files vertically, like ls(1) would
 * 1 AAA	3 AAC	5 AAE
 * 2 AAB	4 AAD	6 AAF */
static void
list_files_vertical(size_t *counter, int *reset_pager, const int pad,
		const size_t columns_n)
{
	int nn = (max_files != UNSET && max_files < (int)files) ? max_files : (int)files;

	int rows = nn / (int)columns_n;
	if (nn % (int)columns_n > 0)
		rows++;

	int last_column = 0;
	/* The previous value of LAST_COLUMN. We need this value to run the pager */
	int blc = last_column;

	size_t cur_cols = 0, cc = columns_n;
	int x = 0, xx = 0, i = 0;
	for ( ; ; i++) {
		/* Copy current values to restore them if necessary: done to
		 * skip the first two chars of arrow keys : \x1b [ */
		int bxx = xx, bx = x;
		size_t bcc = cc;
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
		if (!classify)
			ind_char = 0;

		if (x >= nn || !file_info[x].name) {
			if (last_column)
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

		if (pager) {
			int ret = 0, bi = i;
			/* Run the pager only once all columns and rows fitting in
			 * the screen are filled with the corresponding file names */
			if (blc && *counter > columns_n * ((size_t)term_rows - 2))
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

		/* Trim file name to MAX_NAME_LEN */
		if (!unicode && max_name_len != UNSET && !long_view
		&& (int)file_info[x].len > max_name_len)
			trim_filename(x);

		file_info[x].eln_n = no_eln ? -1 : DIGINUM(x + 1);

		if (colorize) {
			if (light_mode)
				print_entry_color_light(&ind_char, x, pad);
			else
				print_entry_color(&ind_char, x, pad);
		} else {
			if (light_mode)
				print_entry_nocolor_light(&ind_char, x, pad);
			else
				print_entry_nocolor(&ind_char, x, pad);
		}

		if (!last_column) {
			if (light_mode)
				pad_filename_light(&ind_char, x, pad);
			else
				pad_filename(&ind_char, x, pad);
		} else {
			/* Last column is populated. Ex:
			 * 1 file  3 file3  5 file5HERE
			 * 2 file2 4 file4  6 file6HERE
			 * ... */
			putchar('\n');
		}

		if (!unicode && trim.state == 1)
			untrim_filename(x);
	}

	if (!last_column)
		putchar('\n');
}

/* Execute commands in either DIR_IN_NAME or DIR_OUT_NAME files
 * MODE (either DIR_IN or DIR_OUT) tells the function whether to check
 * for DIR_IN_NAME or DIR_OUT_NAME files
 * Used by the autocommands function */
static void
run_dir_cmd(const int mode)
{
	FILE *fp = (FILE *)NULL;
	char path[PATH_MAX];

	if (mode == DIR_IN) {
		snprintf(path, PATH_MAX - 1, "%s/%s", workspaces[cur_ws].path, DIR_IN_NAME);
	} else {
		if (mode == DIR_OUT) {
			if (dirhist_cur_index <= 0 || !old_pwd[dirhist_cur_index - 1])
				return;
			snprintf(path, PATH_MAX - 1, "%s/%s", old_pwd[dirhist_cur_index - 1], DIR_OUT_NAME);
		}
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
		launch_execle(buf);
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
get_largest(size_t i, off_t *size, char **name, char **color, off_t *total)
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

static void
print_analysis_stats(off_t total, off_t largest, char *color, char *name)
{
	char *t = get_size_unit(total);
	char *l = get_size_unit(largest);

	printf(_("Total size:   %s%s%s\n"
		"Largest file: %s%s%s %c%s%s%s%c\n"),
		colorize ? _BGREEN : "" , t ? t : "?",
		colorize ? NC : "",
		colorize ? _BGREEN : "" , l ? l : "?",
		colorize ? NC : "",
		name ? '[' : 0,
		(colorize && color) ? color : "",
		name ? name : "", df_c,
		name ? ']' : 0);

	free(t);
	free(l);
}

/* Return the lenght of the longest UID:GID string for files listed in long view mode */
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

/* List files in the current working directory (global variable 'path').
 * Unlike list_dir(), however, this function uses no color and runs
 * neither stat() nor count_dir(), which makes it quite faster. Return
 * zero on success and one on error */
static int
list_dir_light(void)
{
#ifdef _LIST_SPEED
	clock_t start = clock();
#endif

	int virtual_dir = 0;
	if (stdin_tmp_dir && strcmp(stdin_tmp_dir, workspaces[cur_ws].path) == 0)
		virtual_dir = 1;

	DIR *dir;
	struct dirent *ent;
	int reset_pager = 0;
	int close_dir = 1;
	int excluded_files = 0;

	/* A few variables for the disk usage analyzer mode */
	off_t largest_size = 0, total_size = 0;
	char *largest_name = (char *)NULL, *largest_color = (char *)NULL;

	if ((dir = opendir(workspaces[cur_ws].path)) == NULL) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "%s: %s: %s\n", PROGRAM_NAME, workspaces[cur_ws].path,
		    strerror(errno));
		close_dir = 0;
		goto END;
	}

	set_events_checker();

	errno = 0;
	longest = 0;
	register unsigned int n = 0;
	unsigned int total_dents = 0, count = 0;

	file_info = (struct fileinfo *)xnmalloc(ENTRY_N + 2, sizeof(struct fileinfo));

	while ((ent = readdir(dir))) {
		char *ename = ent->d_name;
		/* Skip self and parent directories */
		if (SELFORPARENT(ename))
			continue;

		/* Check .cfm.in and .cfm.out files for the autocommands function */
		if (dir_changed)
			check_autocmd_file(ename);

		/* Skip files according to FILTER */
		if (_filter) {
			if (regexec(&regex_exp, ename, 0, NULL, 0) == EXIT_SUCCESS) {
				if (filter_rev) {
					excluded_files++;
					continue;
				}
			} else if (!filter_rev) {
				excluded_files++;
				continue;
			}
		}

		if (!show_hidden && *ename == '.')
			continue;
#ifndef _DIRENT_HAVE_D_TYPE
		struct stat attr;
		if (lstat(ename, &attr) == -1)
			continue;
		if (only_dirs && !S_ISDIR(attr.st_mode))
#else
		if (only_dirs && ent->d_type != DT_DIR)
#endif /* !_DIRENT_HAVE_D_TYPE */
			continue;

		if (count > ENTRY_N) {
			count = 0;
			total_dents = n + ENTRY_N;
			file_info = xrealloc(file_info, (total_dents + 2) * sizeof(struct fileinfo));
		}

		file_info[n].name = (char *)xnmalloc(NAME_MAX + 1, sizeof(char));
		if (!unicode) {
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
		file_info[n].exec = 0;
		file_info[n].ruser = 1;
		file_info[n].filesn = 0;
		file_info[n].time = 0;
		file_info[n].sel = 0;
#ifndef _NO_ICONS
		file_info[n].icon = DEF_FILE_ICON;
		file_info[n].icon_color = DEF_FILE_ICON_COLOR;
#endif
		switch (file_info[n].type) {

		case DT_DIR:
#ifndef _NO_ICONS
			if (icons) {
				file_info[n].icon = DEF_DIR_ICON;
				file_info[n].icon_color = DEF_DIR_ICON_COLOR;
//				get_dir_icon(file_info[n].name, (int)n);
				/* If set from the color scheme file */
				if (*dir_ico_c)
					file_info[n].icon_color = dir_ico_c;
			}
#endif

			if (files_counter == 1)
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
#endif
			}

			break;

		case DT_LNK:
#ifndef _NO_ICONS
			file_info[n].icon = ICON_LINK;
#endif
			file_info[n].color = ln_c;
			break;

		case DT_REG: file_info[n].color = fi_c; break;
		case DT_SOCK: file_info[n].color = so_c; break;
		case DT_FIFO: file_info[n].color = pi_c; break;
		case DT_BLK: file_info[n].color = bd_c; break;
		case DT_CHR: file_info[n].color = cd_c; break;
		case DT_UNKNOWN: file_info[n].color = uf_c; break;
		default: file_info[n].color = df_c; break;
		}

#ifndef _NO_ICONS
		if (xargs.icons_use_file_color == 1 && icons)
			file_info[n].icon_color = file_info[n].color;
#endif

		if (long_view) {
			struct stat _attr;
			if (lstat(file_info[n].name, &_attr) != -1)
				set_long_attribs((int)n, &_attr);
		}

		if (xargs.disk_usage_analyzer == 1)
			get_largest(n, &largest_size, &largest_name, &largest_color, &total_size);

		n++;
		count++;
	}

	if (xargs.disk_usage_analyzer == 1) {
		/* Erase the "Retrieveing file sizes" message */
		ERASE_FULL_LINE;
		SET_CURSOR(1, 1);
	}

	file_info[n].name = (char *)NULL;
	files = (size_t)n;

	if (n == 0) {
		printf("%s. ..%s\n", colorize ? di_c : df_c, df_c);
		free(file_info);
		goto END;
	}

	int pad = max_files != UNSET ? DIGINUM(max_files) : DIGINUM(files + 1);

	if (sort)
		ENTSORT(file_info, n, entrycmp);

	size_t counter = 0;
	size_t columns_n = 1;

	/* Get the longest file name */
	if (columned || long_view) {
		int nn = (int)n;
		get_longest_filename(nn, pad);
	}

				/* ########################
				 * #    LONG VIEW MODE    #
				 * ######################## */

	if (long_view) {
		print_long_mode(&counter, &reset_pager, pad,
			prop_fields.ids == 1 ? get_max_ug_str() : 0,
			prop_fields.inode == 1 ? get_longest_inode() : 0);
		goto END;
	}

				/* ########################
				 * #   NORMAL VIEW MODE   #
				 * ######################## */

	/* Get possible amount of columns for the dirlist screen */
	if (!columned)
		columns_n = 1;
	else
		get_columns(&columns_n);

	if (listing_mode == VERTLIST) /* ls(1) like listing */
		list_files_vertical(&counter, &reset_pager, pad, columns_n);
	else
		list_files_horizontal(&counter, &reset_pager, pad, columns_n);

END:
	exit_code = post_listing(dir, close_dir, reset_pager);
	if (virtual_dir == 1)
		print_reload_msg("Virtual directory\n");
	if (excluded_files > 0)
		printf(_("Excluded files: %d\n"), excluded_files);

	if (xargs.disk_usage_analyzer == 1 && long_view && full_dir_size)
		print_analysis_stats(total_size, largest_size, largest_color, largest_name);

#ifdef _LIST_SPEED
	clock_t end = clock();
	printf("list_dir time: %f\n", (double)(end-start)/CLOCKS_PER_SEC);
#endif

	return exit_code;
}

/* Check whether the file in the device DEV with inode INO is selected.
 * Used to mark selected files in the files list */
static inline int
check_seltag(const dev_t dev, const ino_t ino, const nlink_t links, const size_t index)
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
	file_info[n].pad = 0;
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
}

/* List files in the current working directory. Uses file type colors
 * and columns. Return zero on success or one on error */
int
list_dir(void)
{
#ifdef _LIST_SPEED
	clock_t start = clock();
#endif

	if (dir_changed && autocmds_n) {
		if (autocmd_set)
			revert_autocmd_opts();
		check_autocmds();
	}

	if (dir_changed && dir_out) {
		run_dir_cmd(DIR_OUT);
		dir_out = 0;
	}

	if (clear_screen == 1)
		CLEAR;

	if (xargs.disk_usage_analyzer == 1
	|| (long_view == 1 && full_dir_size == 1)) {
		fputs(_("Retrieving file sizes. Please wait... "), stdout);
		fflush(stdout);
	}

	if (!unicode) {
		trim.state = trim.a = trim.b = 0;
		trim.len = 0;
	}

	if (xargs.list_and_quit != 1)
		fputs(HIDE_CURSOR, stdout);
	reset_stats();
	get_term_size();

	if (light_mode)
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

	/* A few variables for the disk usage analyzer mode */
	off_t largest_size = 0, total_size = 0;
	char *largest_name = (char *)NULL, *largest_color = (char *)NULL;

	if ((dir = opendir(workspaces[cur_ws].path)) == NULL) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "%s: %s: %s\n", PROGRAM_NAME,
			workspaces[cur_ws].path, strerror(errno));
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
	register unsigned int n = 0;
	unsigned int total_dents = 0, count = 0;

	file_info = (struct fileinfo *)xnmalloc(ENTRY_N + 2, sizeof(struct fileinfo));

	while ((ent = readdir(dir))) {
		char *ename = ent->d_name;
		/* Skip self and parent directories */
		if (SELFORPARENT(ename))
			continue;

		/* Check .cfm.in and .cfm.out files for the autocommands function */
		if (dir_changed)
			check_autocmd_file(ename);

		/* Filter files according to _FILTER */
		if (_filter) {
			if (regexec(&regex_exp, ename, 0, NULL, 0) == EXIT_SUCCESS) {
				if (filter_rev) {
					excluded_files++;
					continue;
				}
			} else if (!filter_rev) {
				excluded_files++;
				continue;
			}
		}

		if (*ename == '.')
			stats.hidden++;

		if (!show_hidden && *ename == '.')
			continue;

		init_fileinfo(n);

		int stat_ok = 1;
		if (fstatat(fd, ename, &attr, virtual_dir == 1 ? 0 : AT_SYMLINK_NOFOLLOW) == -1) {
			stat_ok = 0;
			stats.unstat++;
		}

#ifdef _DIRENT_HAVE_D_TYPE
		if (only_dirs == 1 && ent->d_type != DT_DIR
		&& (ent->d_type != DT_LNK || get_link_ref(ename) != S_IFDIR))
#else
		if (stat_ok == 1 && only_dirs == 1 && !S_ISDIR(attr.st_mode)
		&& (!S_ISLNK(attr.st_mode) || get_link_ref(ename) != S_IFDIR))
#endif
			continue;

		if (count > ENTRY_N) {
			count = 0;
			total_dents = n + ENTRY_N;
			file_info = xrealloc(file_info, (total_dents + 2) * sizeof(struct fileinfo));
		}

		file_info[n].name = (char *)xnmalloc(NAME_MAX + 1, sizeof(char));
		if (!unicode) {
			file_info[n].len = xstrsncpy(file_info[n].name, ename, NAME_MAX);
		} else {
			xstrsncpy(file_info[n].name, ename, NAME_MAX);
			file_info[n].len = wc_xstrlen(ename);
		}

		if (stat_ok) {
			switch (attr.st_mode & S_IFMT) {
			case S_IFBLK: file_info[n].type = DT_BLK; stats.block_dev++; break;
			case S_IFCHR: file_info[n].type = DT_CHR; stats.char_dev++; break;
			case S_IFDIR: file_info[n].type = DT_DIR; stats.dir++; break;
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

			if (long_view == 1) {
				switch(prop_fields.time) {
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

		switch (sort) {
		case SATIME:
			file_info[n].time = stat_ok ? (time_t)attr.st_atime : 0;
			break;
#if defined(HAVE_ST_BIRTHTIME) || defined(__BSD_VISIBLE)
		case SBTIME:
# ifdef __OpenBSD__
			file_info[n].time = stat_ok ? (time_t)attr.__st_birthtim.tv_sec : 0;
# else
			file_info[n].time = stat_ok ? (time_t)attr.st_birthtime : 0;
# endif /* HAVE_ST_BIRTHTIME || __BSD_VISIBLE */
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
#endif /* _STATX */
		case SCTIME: file_info[n].time = stat_ok ? (time_t)attr.st_ctime : 0; break;
		case SMTIME: file_info[n].time = stat_ok ? (time_t)attr.st_mtime : 0; break;
		default: file_info[n].time = 0; break;
		}

		switch (file_info[n].type) {

		case DT_DIR:
#ifndef _NO_ICONS
			if (icons) {
				get_dir_icon(file_info[n].name, (int)n);

				/* If set from the color scheme file */
				if (*dir_ico_c)
					file_info[n].icon_color = dir_ico_c;
			}
#endif /* _NO_ICONS */
			if (files_counter) {
				file_info[n].filesn = count_dir(ename, NO_CPOP) - 2;
			} else {
				if (stat_ok && check_file_access(&attr) == 0)
					file_info[n].filesn = -1;
				else
					file_info[n].filesn = 1;
			}
			if (file_info[n].filesn > 0) { /* S_ISVTX*/
				file_info[n].color = stat_ok ? ((attr.st_mode & 01000)
						? ((attr.st_mode & 00002) ? tw_c : st_c)
						: ((attr.st_mode & 00002) ? ow_c : di_c))
						: df_c;
				/* S_ISWOTH*/
			} else if (file_info[n].filesn == 0) {
				file_info[n].color = stat_ok ? ((attr.st_mode & 01000)
						? ((attr.st_mode & 00002) ? tw_c : st_c)
						: ((attr.st_mode & 00002) ? ow_c : ed_c))
						: df_c;
			} else {
				file_info[n].color = nd_c;
#ifndef _NO_ICONS
				file_info[n].icon = ICON_LOCK;
				file_info[n].icon_color = YELLOW;
#endif
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

			break;

		case DT_LNK: {
#ifndef _NO_ICONS
			file_info[n].icon = ICON_LINK;
#endif
			if (!follow_symlinks) {
				file_info[n].color = ln_c;
				break;
			}
			struct stat attrl;
			if (fstatat(fd, ename, &attrl, 0) == -1) {
				file_info[n].color = or_c;
				stats.broken_link++;
			} else {
				if (S_ISDIR(attrl.st_mode)) {
					file_info[n].dir = 1;
					files_counter
					    ? (file_info[n].filesn = (count_dir(ename, NO_CPOP) - 2))
					    : (file_info[n].filesn = 0);
				}
				file_info[n].color = ln_c;
			}
			}
			break;

		case DT_REG: {
#ifdef _LINUX_CAP
			cap_t cap;
#endif
			/* Do not perform the access check if the user is root */
			if (!(flags & ROOT_USR)	&& stat_ok && check_file_access(&attr) == 0) {
#ifndef _NO_ICONS
				file_info[n].icon = ICON_LOCK;
				file_info[n].icon_color = YELLOW;
#endif
				file_info[n].color = nf_c;
			} else if (stat_ok && (attr.st_mode & 04000)) { /* SUID */
				file_info[n].exec = 1;
				stats.exec++; stats.suid++;
				file_info[n].color = su_c;
#ifndef _NO_ICONS
				file_info[n].icon = ICON_EXEC;
#endif
			} else if (stat_ok && (attr.st_mode & 02000)) { /* SGID */
				file_info[n].exec = 1;
				stats.exec++; stats.sgid++;
				file_info[n].color = sg_c;
#ifndef _NO_ICONS
				file_info[n].icon = ICON_EXEC;
#endif
			}

#ifdef _LINUX_CAP
			else if (check_cap && (cap = cap_get_file(ename))) {
				file_info[n].color = ca_c;
				stats.caps++;
				cap_free(cap);
			}
#endif

			else if (stat_ok && ((attr.st_mode & 00100) /* Exec */
			|| (attr.st_mode & 00010) || (attr.st_mode & 00001))) {
				file_info[n].exec = 1;
				stats.exec++;
#ifndef _NO_ICONS
				file_info[n].icon = ICON_EXEC;
#endif
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
			if (icons == 1)
				name_icon_found = get_name_icon(file_info[n].name, (int)n);
#endif

			/* Check file extension */
			char *ext = (char *)NULL;
			if (check_ext == 1)
				ext = strrchr(file_info[n].name, '.');
			/* Make sure not to take a hidden file for a file extension */
			if (!ext || ext == file_info[n].name)
				break;
#ifndef _NO_ICONS
			if (icons == 1 && name_icon_found == 0)
				get_ext_icon(ext, (int)n);
#endif
			char *extcolor = get_ext_color(ext);
			if (!extcolor)
				break;

			file_info[n].ext_color = (char *)xnmalloc(strlen(extcolor) + 4, sizeof(char));
			sprintf(file_info[n].ext_color, "\x1b[%sm", extcolor);
			file_info[n].color = file_info[n].ext_color;
			extcolor = (char *)NULL;
		} /* End of DT_REG block */
		break;

		case DT_SOCK: file_info[n].color = so_c; break;
		case DT_FIFO: file_info[n].color = pi_c; break;
		case DT_BLK: file_info[n].color = bd_c; break;
		case DT_CHR: file_info[n].color = cd_c; break;
		case DT_UNKNOWN: file_info[n].color = uf_c; break;
		default: file_info[n].color = df_c; break;
		}

#ifndef _NO_ICONS
		if (xargs.icons_use_file_color == 1 && icons)
			file_info[n].icon_color = file_info[n].color;
#endif
		if (long_view && stat_ok)
			set_long_attribs((int)n, &attr);

		if (xargs.disk_usage_analyzer == 1)
			get_largest(n, &largest_size, &largest_name, &largest_color, &total_size);

		n++;
		count++;
	}

	if (xargs.disk_usage_analyzer == 1 || (long_view == 1 && full_dir_size == 1))
		/* Erase the "Retrieveing file sizes" message */
		fputs("\x1b[2K\x1b[1G", stdout);

	file_info[n].name = (char *)NULL;
	files = n;

	if (n == 0) {
		printf("%s. ..%s\n", colorize ? di_c : df_c, df_c);
		free(file_info);
		goto END;
	}

	int pad = max_files != UNSET ? DIGINUM(max_files) : DIGINUM(files + 1);

		/* #############################################
		 * #    SORT FILES ACCORDING TO SORT METHOD    #
		 * ############################################# */

	if (sort)
		ENTSORT(file_info, n, entrycmp);

		/* ##########################################
		 * #    GET INFO TO PRINT COLUMNED OUTPUT   #
		 * ########################################## */

	size_t counter = 0;
	size_t columns_n = 1;

	/* Get the longest file name */
	if (columned || long_view) {
		int nn = (int)n;
		get_longest_filename(nn, pad);
	}

				/* ########################
				 * #    LONG VIEW MODE    #
				 * ######################## */

	if (long_view) {
		print_long_mode(&counter, &reset_pager, pad,
			prop_fields.ids == 1 ? get_max_ug_str() : 0,
			prop_fields.inode == 1 ? get_longest_inode() : 0);
		goto END;
	}

				/* ########################
				 * #   NORMAL VIEW MODE   #
				 * ######################## */

	/* Get amount of columns needed to print files in CWD  */
	if (!columned)
		columns_n = 1;
	else
		get_columns(&columns_n);

	if (listing_mode == VERTLIST) /* ls(1) like listing */
		list_files_vertical(&counter, &reset_pager, pad, columns_n);
	else
		list_files_horizontal(&counter, &reset_pager, pad, columns_n);

				/* #########################
				 * #   POST LISTING STUFF  #
				 * ######################### */

END:
	exit_code = post_listing(dir, close_dir, reset_pager);
	if (virtual_dir == 1)
		print_reload_msg("Virtual directory\n");
	if (excluded_files > 0)
		printf(_("Excluded files: %d\n"), excluded_files);

	if (xargs.disk_usage_analyzer == 1 && long_view && full_dir_size)
		print_analysis_stats(total_size, largest_size, largest_color, largest_name);

#ifdef _LIST_SPEED
	clock_t end = clock();
	printf("list_dir time: %f\n", (double)(end-start) / CLOCKS_PER_SEC);
#endif

	return exit_code;
}

void
free_dirlist(void)
{
	if (!file_info || !files)
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
	free_dirlist();
	int bk = exit_code;
	list_dir();
	exit_code = bk;
}
