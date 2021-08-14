/* listing.c -- functions controlling what is listed on the screen */

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
#include "icons.h"

#include <stdio.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/capability.h>
#endif
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>

#include "aux.h"
#include "colors.h"
#include "misc.h"
#include "properties.h"
#include "sort.h"

void
print_sort_method(void)
{
	printf(_("%s->%s Sorted by: "), mi_c, df_c);

	switch (sort) {
	case SNONE:
		puts(_("none"));
		break;
	case SNAME:
		printf(_("name %s\n"), (sort_reverse) ? "[rev]" : "");
		break;
	case SSIZE:
		printf(_("size %s\n"), (sort_reverse) ? "[rev]" : "");
		break;
	case SATIME:
		printf(_("atime %s\n"), (sort_reverse) ? "[rev]" : "");
		break;
	case SBTIME:
#if defined(HAVE_ST_BIRTHTIME) || defined(__BSD_VISIBLE) || defined(_STATX)
		printf(_("btime %s\n"), (sort_reverse) ? "[rev]" : "");
#else
		printf(_("btime (not available: using 'ctime') %s\n"),
		    (sort_reverse) ? "[rev]" : "");
#endif
		break;
	case SCTIME:
		printf(_("ctime %s\n"), (sort_reverse) ? "[rev]" : "");
		break;
	case SMTIME:
		printf(_("mtime %s\n"), (sort_reverse) ? "[rev]" : "");
		break;
#if __FreeBSD__ || __NetBSD__ || __OpenBSD__ || _BE_POSIX
	case SVER:
		printf(_("version (not available: using 'name') %s\n"),
		    (sort_reverse) ? "[rev]" : "");
#else
	case SVER:
		printf(_("version %s\n"), (sort_reverse) ? "[rev]" : "");
#endif
		break;
	case SEXT:
		printf(_("extension %s\n"), (sort_reverse) ? "[rev]" : "");
		break;
	case SINO:
		printf(_("inode %s\n"), (sort_reverse) ? "[rev]" : "");
		break;
	case SOWN:
		if (light_mode) {
			printf(_("owner (not available: using 'name') %s\n"),
			    (sort_reverse) ? "[rev]" : "");
		} else {
			printf(_("owner %s\n"), (sort_reverse) ? "[rev]" : "");
		}
		break;
	case SGRP:
		if (light_mode) {
			printf(_("group (not available: using 'name') %s\n"),
			    (sort_reverse) ? "[rev]" : "");
		} else {
			printf(_("group %s\n"), (sort_reverse) ? "[rev]" : "");
		}
		break;
	}
}

/* Print the line divinding files and prompt using DIV_LINE_CHAR. If
 * DIV_LINE_CHAR takes more than two columns to be printed (ASCII chars
 * take only one, but unicode chars could take two), print exactly the
 * content of DIV_LINE_CHAR. Otherwise, repeat DIV_LINE_CHAR to fulfill
 * all terminal columns. */
void
print_div_line(void)
{
	fputs(dl_c, stdout);

	size_t len = wc_xstrlen(div_line_char);
	if (len <= 2) {
		int i;
		for (i = (int)term_cols / len; i--;)
			fputs(div_line_char, stdout);
	} else {
		puts(div_line_char);
	}

	fputs(df_c, stdout);
	fflush(stdout);
}

/* Print free/total space for the filesystem of the current working
 * directory */
void
print_disk_usage(void)
{
	if (!ws || !ws[cur_ws].path || !*ws[cur_ws].path)
		return;

	struct statvfs stat;
	if (statvfs(ws[cur_ws].path, &stat) != EXIT_SUCCESS) {
		_err('w', PRINT_PROMPT, "statvfs: %s\n", strerror(errno));
		return;
	}

	char *free_space = get_size_unit((off_t)(stat.f_frsize * stat.f_bavail));
	char *size = get_size_unit((off_t)(stat.f_blocks * stat.f_frsize));
	printf("%s->%s %s/%s\n", mi_c, df_c, free_space ? free_space : "?",
	    size ? size : "?");

	free(free_space);
	free(size);
	return;
}

void
_print_selfiles(unsigned short term_rows)
{
	int limit = max_printselfiles;

	if (max_printselfiles == 0) {
		/* Never take more than half terminal height */
		limit = (term_rows / 2) - 4;
		/* 4 = 2 div lines, 2 prompt lines */
		if (limit <= 0)
			limit = 1;
	}

	if (limit > sel_n)
		limit = (int)sel_n;

	size_t i;
	for (i = 0; i < (max_printselfiles != UNSET ? limit : sel_n); i++)
		colors_list(sel_elements[i], 0, NO_PAD, PRINT_NEWLINE);

	if (max_printselfiles != UNSET && limit < sel_n)
		printf("%zu/%zu\n", i, sel_n);

	print_div_line();
}

void
print_dirhist_map(void)
{
	size_t i;
	for (i = 0; i < (size_t)dirhist_total_index; i++) {
		if (i != (size_t)dirhist_cur_index)
			continue;

		if (i > 0 && old_pwd[i - 1])
			printf("%zu %s\n", i, old_pwd[i - 1]);

		printf("%zu %s%s%s\n", i + 1, dh_c,
		    old_pwd[i], df_c);

		if (i + 1 < (size_t)dirhist_total_index && old_pwd[i + 1])
			printf("%zu %s\n", i + 2, old_pwd[i + 1]);

		break;
	}
}

/* Set the icon field to the corresponding icon for FILE. If not found,
 * set the default icon */
void
get_file_icon(const char *file, int n)
{
	if (!file)
		return;

	int i = (int)(sizeof(icon_filenames) / sizeof(struct icons_t));
	while (--i >= 0) {
		if (TOUPPER(*file) == TOUPPER(*icon_filenames[i].name)
		&& strcasecmp(file, icon_filenames[i].name) == 0) {
			file_info[n].icon = icon_filenames[i].icon;
			file_info[n].icon_color = icon_filenames[i].color;
			break;
		}
	}
}

/* Set the icon field to the corresponding icon for DIR. If not found,
 * set the default icon */
void
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
void
get_ext_icon(const char *restrict ext, int n)
{
	file_info[n].icon = DEF_FILE_ICON;
	file_info[n].icon_color = DEF_FILE_ICON_COLOR;

	if (!ext)
		return;

	ext++;

	int i = (int)(sizeof(icon_ext) / sizeof(struct icons_t));
	while (--i >= 0) {
		/* Tolower */
		char c = (*ext >= 'A' && *ext <= 'Z')
			     ? (*ext - 'A' + 'a')
			     : *ext;
		if (c == *icon_ext[i].name && strcasecmp(ext, icon_ext[i].name) == 0) {
			file_info[n].icon = icon_ext[i].icon;
			file_info[n].icon_color = icon_ext[i].color;
			break;
		}
	}
}

/* List files in the current working directory (global variable 'path').
 * Unlike list_dir(), however, this function uses no color and runs
 * neither stat() nor count_dir(), which makes it quite faster. Return
 * zero on success or one on error */
int
list_dir_light(void)
{
	/*  clock_t start = clock(); */
	/* Hide the cursor while listing */
	fputs("\x1b[?25l", stdout);

	DIR *dir;
	struct dirent *ent;
	int reset_pager = 0;
	int close_dir = 1;
	/* Get terminal current amount of rows and columns */
	struct winsize w;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
	/* ws_col and ws_row are both unsigned short int according to
	 * /bits/ioctl-types.h */

	/* These two are global */
	term_cols = w.ws_col;
	term_rows = w.ws_row;

	if ((dir = opendir(ws[cur_ws].path)) == NULL) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, ws[cur_ws].path,
		    strerror(errno));
		close_dir = 0;
		goto END;
	}

#ifdef LINUX_INOTIFY
	if (inotify_wd >= 0) {
		inotify_rm_watch(inotify_fd, inotify_wd);
		inotify_wd = -1;
	}
	inotify_wd = inotify_add_watch(inotify_fd, ws[cur_ws].path, INOTIFY_MASK);

#elif defined(BSD_KQUEUE)
	if (event_fd >= 0) {
		close(event_fd);
		event_fd = -1;
	}
#if defined(O_EVTONLY)
	event_fd = open(ws[cur_ws].path, O_EVTONLY);
#else
	event_fd = open(ws[cur_ws].path, O_RDONLY);
#endif
	if (event_fd >= 0) {
		EV_SET(&events_to_monitor[0], event_fd, EVFILT_VNODE,
		       EV_ADD | EV_CLEAR, KQUEUE_FFLAGS, 0, ws[cur_ws].path);
	}
#endif

	errno = 0;
	longest = 0;
	register unsigned int n = 0;
	unsigned int total_dents = 0, count = 0;

	file_info = (struct fileinfo *)xnmalloc(ENTRY_N + 2,
	    sizeof(struct fileinfo));

	while ((ent = readdir(dir))) {
		char *ename = ent->d_name;
		/* Skip self and parent directories */
		if (*ename == '.' && (!ename[1] || (ename[1] == '.' && !ename[2])))
			continue;
		/* Skip files matching FILTER */
		if (filter && regexec(&regex_exp, ename, 0, NULL, 0) == EXIT_SUCCESS)
			continue;
		if (!show_hidden && *ename == '.')
			continue;
#if !defined(_DIRENT_HAVE_D_TYPE)
		struct stat attr;
		if (lstat(ename, &attr) == -1)
			continue;
		if (only_dirs && (attr.st_mode & S_IFMT) != S_IFDIR)
#else
		if (only_dirs && ent->d_type != DT_DIR)
#endif
			continue;

		if (count > ENTRY_N) {
			count = 0;
			total_dents = n + ENTRY_N;
			file_info = xrealloc(file_info, (total_dents + 2) * sizeof(struct fileinfo));
		}

		file_info[n].name = (char *)xnmalloc(NAME_MAX + 1, sizeof(char));

		if (!unicode) {
			file_info[n].len = (xstrsncpy(file_info[n].name, ename,
						NAME_MAX + 1) - 1);
		} else {
			xstrsncpy(file_info[n].name, ename, NAME_MAX + 1);
			file_info[n].len = wc_xstrlen(ename);
		}

		/* ################  */
#if !defined(_DIRENT_HAVE_D_TYPE)
		switch (attr.st_mode & S_IFMT) {
		case S_IFBLK: file_info[n].type = DT_BLK; break;
		case S_IFCHR: file_info[n].type = DT_CHR; break;
		case S_IFDIR: file_info[n].type = DT_DIR; break;
		case S_IFIFO: file_info[n].type = DT_FIFO; break;
		case S_IFLNK: file_info[n].type = DT_LNK; break;
		case S_IFREG: file_info[n].type = DT_REG; break;
		case S_IFSOCK: file_info[n].type = DT_SOCK; break;
		default: file_info[n].type = DT_UNKNOWN; break;
		}
		file_info[n].dir = (file_info[n].type == DT_DIR) ? 1 : 0;
		file_info[n].symlink = (file_info[n].type == DT_LNK) ? 1 : 0;
#else
		file_info[n].dir = (ent->d_type == DT_DIR) ? 1 : 0;
		file_info[n].symlink = (ent->d_type == DT_LNK) ? 1 : 0;
		file_info[n].type = ent->d_type;
#endif
		file_info[n].inode = ent->d_ino;
		file_info[n].linkn = 1;
		file_info[n].size = 1;
		file_info[n].color = (char *)NULL;
		file_info[n].ext_color = (char *)NULL;
		file_info[n].icon = DEF_FILE_ICON;
		file_info[n].icon_color = DEF_FILE_ICON_COLOR;
		file_info[n].exec = 0;
		file_info[n].ruser = 1;
		file_info[n].filesn = 0;
		file_info[n].time = 0;

		switch (file_info[n].type) {

		case DT_DIR:
			if (icons) {
				get_dir_icon(file_info[n].name, (int)n);
				/* If set from the color scheme file */
				if (*dir_ico_c)
					file_info[n].icon_color = dir_ico_c;
			}

			files_counter
			    ? (file_info[n].filesn = (count_dir(ename, NO_CPOP) - 2))
			    : (file_info[n].filesn = 1);

			if (file_info[n].filesn > 0) {
				file_info[n].color = di_c;
			} else if (file_info[n].filesn == 0) {
				file_info[n].color = ed_c;
			} else {
				file_info[n].color = nd_c;
				file_info[n].icon = ICON_LOCK;
				file_info[n].icon_color = YELLOW;
			}

			break;

		case DT_LNK:
			file_info[n].icon = ICON_LINK;
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

		if (xargs.icons_use_file_color == 1 && icons)
			file_info[n].icon_color = file_info[n].color;

		n++;
		count++;
	}

	file_info[n].name = (char *)NULL;
	files = (size_t)n;

	if (n == 0) {
		printf("%s. ..%s\n", colorize ? di_c : df_c, df_c);
		free(file_info);
		goto END;
	}

	if (sort)
		qsort(file_info, n, sizeof(*file_info), entrycmp);

	int c, i;
	register size_t counter = 0;
	size_t columns_n = 1;

	/* Get the longest file name */
	if (columned || long_view) {
		i = (int)n;
		while (--i >= 0) {
			size_t total_len = 0;
			file_info[i].eln_n = no_eln ? -1 : DIGINUM(i + 1);
			total_len = file_info[i].eln_n + 1 + file_info[i].len;

			if (!long_view && classify) {
				if (file_info[i].dir)
					total_len += 2;

				if (file_info[i].filesn > 0 && files_counter)
					total_len += DIGINUM(file_info[i].filesn);

				if (!file_info[i].dir && !colorize) {
					switch (file_info[i].type) {
					case DT_REG:
						if (file_info[i].exec)
							total_len += 1;
						break;
					case DT_LNK:  /* fallthrough */
					case DT_SOCK: /* fallthrough */
					case DT_FIFO: /* fallthrough */
					case DT_UNKNOWN: total_len += 1; break;
					}
				}
			}

			if (total_len > longest)
				longest = total_len;
		}

		if (icons && !long_view && columned)
			longest += 3;
	}

				/* ########################
				 * #    LONG VIEW MODE    #
				 * ######################## */

	if (long_view) {
		struct stat lattr;
		int space_left = (int)term_cols - MAX_PROP_STR;

		if (space_left < min_name_trim)
			space_left = min_name_trim;

		if ((int)longest < space_left)
			space_left = (int)longest;

		int k = (int)files;
		for (i = 0; i < k; i++) {
			if (max_files != UNSET && i == max_files)
				break;
			if (lstat(file_info[i].name, &lattr) == -1)
				continue;

			if (pager) {
				if (counter > (size_t)(term_rows - 2)) {
					fputs("\x1b[7;97m--Mas--\x1b[0;49m", stdout);

					switch (c = xgetchar()) {
					/* Advance one line at a time */
					case 66: /* fallthrough */ /* Down arrow */
					case 10: /* fallthrough */ /* Enter */
					case 32:		   /* Space */
						break;

					/* Advance one page at a time */
					case 126:
						counter = 0; /* Page Down */
						break;

					case 63: /* fallthrough */ /* ? */
					case 104: {		   /* h: Print pager help */
						CLEAR;

						fputs(_("?, h: help\n"
							"Down arrow, Enter, Space: Advance one line\n"
							"Page Down: Advance one page\n"
							"q: Stop pagging\n"), stdout);

						int l = (int)term_rows - 5;
						while (--l >= 0)
							putchar('\n');

						fputs("\x1b[7;97m--Mas--\x1b[0;49m", stdout);

						i -= (term_rows - 1);
						if (i < 0)
							i = 0;

						counter = 0;
						xgetchar();
						CLEAR;
					} break;

					/* Stop paging (and set a flag to reenable the pager
					 * later) */
					case 99:  /* 'c' */
					case 112: /* 'p' */
					case 113:
						pager = 0, reset_pager = 1; /* 'q' */
						break;

					/* If another key is pressed, go back one position.
					 * Otherwise, some file names won't be listed.*/
					default:
						i--;
						fputs("\r\x1b[K\x1b[3J", stdout);
						continue;
					}

					fputs("\r\x1b[K\x1b[3J", stdout);
				}

				counter++;
			}

			file_info[i].uid = lattr.st_uid;
			file_info[i].gid = lattr.st_gid;
			file_info[i].ltime = (time_t)lattr.st_mtim.tv_sec;
			file_info[i].mode = lattr.st_mode;
			file_info[i].size = lattr.st_size;

			/* Print ELN. The remaining part of the line will be
			 * printed by print_entry_props() */
			if (!no_eln)
				printf("%s%d%s ", el_c, i + 1, df_c);

			print_entry_props(&file_info[i], (size_t)space_left);
		}

		goto END;
	}

				/* ########################
				 * #   NORMAL VIEW MODE   #
				 * ######################## */

	int last_column = 0;

	/* Get possible amount of columns for the dirlist screen */
	if (!columned) {
		columns_n = 1;
	} else {
		columns_n = (size_t)term_cols / (longest + 1); /* +1 for the
		space between file names */

		/* If longest is bigger than terminal columns, columns_n will
		 * be negative or zero. To avoid this: */
		if (columns_n < 1)
			columns_n = 1;

		/* If we have only three files, we don't want four columns */
		if (columns_n > (size_t)n)
			columns_n = (size_t)n;
	}

	int nn = (int)n;
	size_t cur_cols = 0;
	for (i = 0; i < nn; i++) {
		if (max_files != UNSET && i == max_files)
			break;

		/* A basic pager for directories containing large amount of
		 * files. What's missing? It only goes downwards. To go
		 * backwards, use the terminal scrollback function */
		if (pager) {
			/* Run the pager only once all columns and rows fitting in
			 * the screen are filled with the corresponding file names */
			if (last_column && counter > columns_n * ((size_t)term_rows - 2)) {
				fputs("\x1b[7;97m--Mas--\x1b[0;49m", stdout);

				switch (c = xgetchar()) {

				/* Advance one line at a time */
				case 66: /* fallthrough */ /* Down arrow */
				case 10: /* fallthrough */ /* Enter */
				case 32:		   /* Space */
					break;

				/* Advance one page at a time */
				case 126:
					counter = 0; /* Page Down */
					break;

				case 63: /* fallthrough */ /* ? */
				case 104: {		   /* h: Print pager help */
					CLEAR;

					fputs(_("?, h: help\n"
						"Down arrow, Enter, Space: Advance one line\n"
						"Page Down: Advance one page\n"
						"q: Stop pagging\n"), stdout);

					int l = (int)term_rows - 5;
					while (--l >= 0)
						putchar('\n');

					fputs("\x1b[7;97m--Mas--\x1b[0;49m", stdout);

					i -= ((term_rows * columns_n) - 1);
					if (i < 0)
						i = 0;

					counter = 0;
					xgetchar();
					CLEAR;
				} break;

				/* Stop paging (and set a flag to reenable the pager
				 * later) */
				case 99:  /* 'c' */
				case 112: /* 'p' */
				case 113:
					pager = 0, reset_pager = 1; /* 'q' */
					break;

				/* If another key is pressed, go back one position.
				 * Otherwise, some file names won't be listed.*/
				default:
					i--;
					fputs("\r\x1b[K\x1b[3J", stdout);
					continue;
				}

				fputs("\r\x1b[K\x1b[3J", stdout);
			}

			counter++;
		}

		if (++cur_cols == columns_n) {
			cur_cols = 0;
			last_column = 1;
		} else {
			last_column = 0;
		}

		file_info[i].eln_n = no_eln ? -1 : DIGINUM(i + 1);
		int ind_char = 1;

		if (!classify)
			ind_char = 0;

		if (colorize) {
			ind_char = 0;
			if (icons) {
				if (xargs.icons_use_file_color == 1)
					file_info[i].icon_color = file_info[i].color;

				if (no_eln) {
					printf("%s%s %s%s%s", file_info[i].icon_color,
					    file_info[i].icon, file_info[i].color,
					    file_info[i].name, df_c);
				} else {
					printf("%s%d%s %s%s %s%s%s", el_c, i + 1, df_c,
					    file_info[i].icon_color, file_info[i].icon,
					    file_info[i].color, file_info[i].name, df_c);
				}
			} else {
				if (no_eln) {
					printf("%s%s%s", file_info[i].color,
					    file_info[i].name, df_c);
				} else {
					printf("%s%d%s %s%s%s", el_c, i + 1, df_c,
					    file_info[i].color, file_info[i].name, df_c);
				}
			}

			if (file_info[i].dir && classify) {
				fputs(" /", stdout);
				if (file_info[i].filesn > 0 && files_counter)
					fputs(xitoa(file_info[i].filesn), stdout);
			}
		}

		/* No color */
		else {
			if (icons) {
				if (no_eln)
					printf("%s %s", file_info[i].icon, file_info[i].name);
				else
					printf("%s%d%s %s %s", el_c, i + 1, df_c,
					    file_info[i].icon, file_info[i].name);
			} else {
				if (no_eln)
					fputs(file_info[i].name, stdout);
				else
					printf("%s%d%s %s", el_c, i + 1, df_c, file_info[i].name);
			}

			if (classify) {
				switch (file_info[i].type) {
				case DT_DIR:
					ind_char = 0;
					fputs(" /", stdout);
					if (file_info[i].filesn > 0 && files_counter)
						fputs(xitoa(file_info[i].filesn), stdout);
					break;

				case DT_FIFO: putchar('|'); break;
				case DT_LNK: putchar('@'); break;
				case DT_SOCK: putchar('='); break;
				case DT_UNKNOWN: putchar('?'); break;
				default: ind_char = 0; break;
				}
			}
		}

		if (!last_column) {
			/* Add spaces needed to equate the longest file name length */
			int cur_len = (int)file_info[i].eln_n + 1 + (icons ? 3 : 0)
						+ (int)file_info[i].len + (ind_char ? 1 : 0);
			if (classify) {
				if (file_info[i].dir)
					cur_len += 2;
				if (file_info[i].filesn > 0 && files_counter
				&& file_info[i].ruser)
					cur_len += DIGINUM((int)file_info[i].filesn);
			}

			int diff = (int)longest - cur_len;
			printf("\x1b[%dC", diff + 1);
/*			register int j;
			for (j = diff + 1; j--;)
				putchar(' '); */
		} else {
			putchar('\n');
		}
	}

	if (!last_column)
		putchar('\n');

END:
	if (close_dir && closedir(dir) == -1) {
		/* Unhide the cursor */
		fputs("\x1b[?25h", stdout);
		return EXIT_FAILURE;
	}

	if (xargs.list_and_quit == 1)
		exit(exit_code);

	if (reset_pager)
		pager = 1;

	if (max_files != UNSET && (int)files > max_files)
		printf("%d/%zu\n", max_files, files);

	/* Print a dividing line between the files list and the
	 * prompt */
	print_div_line();

	if (dirhist_map) {
		/* Print current, previous, and next entries */
		print_dirhist_map();
		print_div_line();
	}

	if (disk_usage)
		print_disk_usage();

	if (sort_switch)
		print_sort_method();

	if (print_selfiles && sel_n > 0)
		_print_selfiles(term_rows);

	/* Unhide the cursor */
	fputs("\x1b[?25h", stdout);

	/*  clock_t end = clock();
	printf("list_dir time: %f\n", (double)(end-start)/CLOCKS_PER_SEC); */

	return EXIT_SUCCESS;
}

/* List files in the current working directory. Uses file type colors
 * and columns. Return zero on success or one on error */
int
list_dir(void)
{
	/*  clock_t start = clock(); */
	if (clear_screen)
		CLEAR;

	if (light_mode)
		return list_dir_light();

	/* Hide the cursor while listing */
	fputs("\x1b[?25l", stdout);

	DIR *dir;
	struct dirent *ent;
	struct stat attr;
	int reset_pager = 0;
	int close_dir = 1;
	/* Get terminal current amount of rows and columns */
	struct winsize w;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
	/* These two are global */
	term_cols = w.ws_col;
	term_rows = w.ws_row;

	if ((dir = opendir(ws[cur_ws].path)) == NULL) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, ws[cur_ws].path,
		    strerror(errno));
		close_dir = 0;
		goto END;
	}

#ifdef LINUX_INOTIFY
	if (inotify_wd >= 0) {
		inotify_rm_watch(inotify_fd, inotify_wd);
		inotify_wd = -1;
		watch = 0;
	}
	inotify_wd = inotify_add_watch(inotify_fd, ws[cur_ws].path, INOTIFY_MASK);
	if (inotify_wd > 0)
		watch = 1;

#elif defined(BSD_KQUEUE)
	if (event_fd >= 0) {
		close(event_fd);
		event_fd = -1;
		watch = 0;
	}
#if defined(O_EVTONLY)
	event_fd = open(ws[cur_ws].path, O_EVTONLY);
#else
	event_fd = open(ws[cur_ws].path, O_RDONLY);
#endif
	if (event_fd >= 0) {
		EV_SET(&events_to_monitor[0], event_fd, EVFILT_VNODE,
				EV_ADD | EV_CLEAR, KQUEUE_FFLAGS, 0, ws[cur_ws].path);
		watch = 1;
	}
#endif

	int fd = dirfd(dir);

		/* ##########################################
		 * #    GATHER AND STORE FILE INFORMATION   #
		 * ########################################## */

	errno = 0;
	longest = 0;
	register unsigned int n = 0;
	unsigned int total_dents = 0, count = 0;

	file_info = (struct fileinfo *)xnmalloc(ENTRY_N + 2,
	    sizeof(struct fileinfo));

	while ((ent = readdir(dir))) {
		char *ename = ent->d_name;
		/* Skip self and parent directories */
		if (*ename == '.' && (!ename[1] || (ename[1] == '.' && !ename[2])))
			continue;

		/* Filter files according to FILTER */
		if (filter) {
			if (regexec(&regex_exp, ename, 0, NULL, 0) == EXIT_SUCCESS) {
				if (filter_rev)
					continue;
			} else if (!filter_rev) {
				continue;
			}
		}

		if (!show_hidden && *ename == '.')
			continue;

		fstatat(fd, ename, &attr, AT_SYMLINK_NOFOLLOW);

#if !defined(_DIRENT_HAVE_D_TYPE)
		if (only_dirs && (attr.st_mode & S_IFMT) == S_IFDIR)
#else
		if (only_dirs && ent->d_type != DT_DIR)
#endif
			continue;

		if (count > ENTRY_N) {
			count = 0;
			total_dents = n + ENTRY_N;
			file_info = xrealloc(file_info, (total_dents + 2) * sizeof(struct fileinfo));
		}

		file_info[n].name = (char *)xnmalloc(NAME_MAX + 1, sizeof(char));

		if (!unicode) {
			file_info[n].len = (xstrsncpy(file_info[n].name, ename,
								NAME_MAX + 1) - 1);
		} else {
			xstrsncpy(file_info[n].name, ename, NAME_MAX + 1);
			file_info[n].len = wc_xstrlen(ename);
		}

		file_info[n].exec = 0;

#if !defined(_DIRENT_HAVE_D_TYPE)
		switch (attr.st_mode & S_IFMT) {
		case S_IFBLK: file_info[n].type = DT_BLK; break;
		case S_IFCHR: file_info[n].type = DT_CHR; break;
		case S_IFDIR: file_info[n].type = DT_DIR; break;
		case S_IFIFO: file_info[n].type = DT_FIFO; break;
		case S_IFLNK: file_info[n].type = DT_LNK; break;
		case S_IFREG: file_info[n].type = DT_REG; break;
		case S_IFSOCK: file_info[n].type = DT_SOCK; break;
		default: file_info[n].type = DT_UNKNOWN; break;
		}
		file_info[n].dir = (file_info[n].type == DT_DIR) ? 1 : 0;
		file_info[n].symlink = (file_info[n].type == DT_LNK) ? 1 : 0;
#else
		file_info[n].dir = (ent->d_type == DT_DIR) ? 1 : 0;
		file_info[n].symlink = (ent->d_type == DT_LNK) ? 1 : 0;
		file_info[n].type = ent->d_type;
#endif

		file_info[n].inode = ent->d_ino;
		file_info[n].linkn = attr.st_nlink;
		file_info[n].size = attr.st_size;

		if (long_view) {
			file_info[n].uid = attr.st_uid;
			file_info[n].gid = attr.st_gid;
			file_info[n].ltime = (time_t)attr.st_mtim.tv_sec;
			file_info[n].mode = attr.st_mode;
		} else if (sort == SOWN || sort == SGRP) {
			file_info[n].uid = attr.st_uid;
			file_info[n].gid = attr.st_gid;
		}

		file_info[n].color = (char *)NULL;
		file_info[n].ext_color = (char *)NULL;

		/* Default icon for all files */
		file_info[n].icon = DEF_FILE_ICON;
		file_info[n].icon_color = DEF_FILE_ICON_COLOR;

		file_info[n].ruser = 1;
		file_info[n].filesn = 0;

		switch (sort) {
		case SATIME:
			file_info[n].time = (time_t)attr.st_atime;
			break;
#if defined(HAVE_ST_BIRTHTIME) || defined(__BSD_VISIBLE)
		case SBTIME:
#ifdef __OpenBSD__
			file_info[n].time = (time_t)attr.__st_birthtim.tv_sec;
#else
			file_info[n].time = (time_t)attr.st_birthtime;
#endif
			break;
#elif defined(_STATX)
		case SBTIME: {
			struct statx attx;
			if (statx(AT_FDCWD, ename, AT_SYMLINK_NOFOLLOW,
			STATX_BTIME, &attx) == -1)
				file_info[n].time = 0;
			else
				file_info[n].time = (time_t)attx.stx_btime.tv_sec;
		} break;
#else
		case SBTIME: file_info[n].time = (time_t)attr.st_ctime; break;
#endif
		case SCTIME: file_info[n].time = (time_t)attr.st_ctime;	break;
		case SMTIME: file_info[n].time = (time_t)attr.st_mtime; break;
		default: file_info[n].time = 0; break;
		}

		switch (file_info[n].type) {

		case DT_DIR:
			if (icons) {
				get_dir_icon(file_info[n].name, (int)n);

				/* If set from the color scheme file */
				if (*dir_ico_c)
					file_info[n].icon_color = dir_ico_c;
			}
			if (files_counter) {
				file_info[n].filesn = count_dir(ename, NO_CPOP) - 2;
			} else {
				if (access(ename, R_OK) == -1)
					file_info[n].filesn = -1;
				else
					file_info[n].filesn = 1;
			}
			if (file_info[n].filesn > 0) { /* S_ISVTX*/
				file_info[n].color = (attr.st_mode & 01000)
							 ? ((attr.st_mode & 00002) ? tw_c : st_c)
							 : ((attr.st_mode & 00002) ? ow_c : di_c);
				/* S_ISWOTH*/
			} else if (file_info[n].filesn == 0) {
				file_info[n].color = (attr.st_mode & 01000)
							 ? ((attr.st_mode & 00002) ? tw_c : st_c)
							 : ((attr.st_mode & 00002) ? ow_c : ed_c);
			} else {
				file_info[n].color = nd_c;
				file_info[n].icon = ICON_LOCK;
				file_info[n].icon_color = YELLOW;
			}

			break;

		case DT_LNK:
			file_info[n].icon = ICON_LINK;
			struct stat attrl;
			if (fstatat(fd, ename, &attrl, 0) == -1) {
				file_info[n].color = or_c;
			} else {
				if (S_ISDIR(attrl.st_mode)) {
					file_info[n].dir = 1;
					files_counter
					    ? (file_info[n].filesn = (count_dir(ename, NO_CPOP) - 2))
					    : (file_info[n].filesn = 0);
				}
				file_info[n].color = ln_c;
			}
			break;

		case DT_REG: {
#ifdef _LINUX_CAP
			cap_t cap;
#endif
			/* Do not perform the access check if the user is root */
			if (!(flags & ROOT_USR)
			&& access(file_info[n].name, F_OK | R_OK) == -1) {
				file_info[n].color = nf_c;
				file_info[n].icon = ICON_LOCK;
				file_info[n].icon_color = YELLOW;
			} else if (attr.st_mode & 04000) { /* SUID */
				file_info[n].exec = 1;
				file_info[n].color = su_c;
				file_info[n].icon = ICON_EXEC;
			} else if (attr.st_mode & 02000) { /* SGID */
				file_info[n].exec = 1;
				file_info[n].color = sg_c;
				file_info[n].icon = ICON_EXEC;
			}

#ifdef _LINUX_CAP
			else if ((cap = cap_get_file(ename))) {
				file_info[n].color = ca_c;
				cap_free(cap);
			}
#endif

			else if ((attr.st_mode & 00100) /* Exec */
				 || (attr.st_mode & 00010) || (attr.st_mode & 00001)) {
				file_info[n].exec = 1;
				file_info[n].icon = ICON_EXEC;
				if (file_info[n].size == 0)
					file_info[n].color = ee_c;
				else
					file_info[n].color = ex_c;
			} else if (file_info[n].size == 0) {
				file_info[n].color = ef_c;
			} else if (file_info[n].linkn > 1) { /* Multi-hardlink */
				file_info[n].color = mh_c;
			} else { /* Regular file */
				file_info[n].color = fi_c;
			}

			/* Check file extension */
			char *ext = strrchr(file_info[n].name, '.');
			/* Make sure not to take a hidden file for a file extension */
			if (ext && ext != file_info[n].name) {
				if (icons)
					get_ext_icon(ext, (int)n);
				/* Check extension color only if some is defined */
				if (ext_colors_n) {
					char *extcolor = get_ext_color(ext);

					if (extcolor) {
						file_info[n].ext_color = (char *)xnmalloc(
									strlen(extcolor) + 4, sizeof(char));
						sprintf(file_info[n].ext_color, "\x1b[%sm",
								extcolor);
						file_info[n].color = file_info[n].ext_color;
						extcolor = (char *)NULL;
					}
				}
			}

			/* No extension. Check icons for specific file names */
			else if (icons)
				get_file_icon(file_info[n].name, (int)n);

		} /* End of DT_REG block */
		break;

		case DT_SOCK: file_info[n].color = so_c; break;
		case DT_FIFO: file_info[n].color = pi_c; break;
		case DT_BLK: file_info[n].color = bd_c; break;
		case DT_CHR: file_info[n].color = cd_c; break;
		case DT_UNKNOWN: file_info[n].color = uf_c; break;
		default: file_info[n].color = df_c; break;
		}

		if (xargs.icons_use_file_color == 1 && icons)
			file_info[n].icon_color = file_info[n].color;

		n++;
		count++;
	}

	file_info[n].name = (char *)NULL;
	files = n;

	if (n == 0) {
		printf("%s. ..%s\n", colorize ? di_c : df_c, df_c);
		free(file_info);
		goto END;
	}

		/* #############################################
		 * #    SORT FILES ACCORDING TO SORT METHOD    #
		 * ############################################# */

	if (sort)
		qsort(file_info, n, sizeof(*file_info), entrycmp);

		/* ##########################################
		 * #    GET INFO TO PRINT COLUMNED OUTPUT   #
		 * ########################################## */

	int c, i;
	register size_t counter = 0;
	size_t columns_n = 1;

	/* Get the longest file name */
	if (columned || long_view) {
		i = (int)n;
		while (--i >= 0) {
			size_t total_len = 0;
			file_info[i].eln_n = no_eln ? -1 : DIGINUM(i + 1);
			total_len = file_info[i].eln_n + 1 + file_info[i].len;

			if (!long_view && classify) {
				if (file_info[i].dir)
					total_len += 2;

				if (file_info[i].filesn > 0 && files_counter)
					total_len += DIGINUM(file_info[i].filesn);

				if (!file_info[i].dir && !colorize) {
					switch (file_info[i].type) {
					case DT_REG:
						if (file_info[i].exec)
							total_len += 1;
						break;
					case DT_LNK:  /* fallthrough */
					case DT_SOCK: /* fallthrough */
					case DT_FIFO: /* fallthrough */
					case DT_UNKNOWN: total_len += 1; break;
					}
				}
			}

			if (total_len > longest)
				longest = total_len;
		}

		if (icons && !long_view && columned)
			longest += 3;
	}

				/* ########################
				 * #    LONG VIEW MODE    #
				 * ######################## */

	if (long_view) {
		int space_left = term_cols - MAX_PROP_STR;
		/* SPACE_LEFT is the max space that should be used to print the
		 * file name (plus space char) */

		/* Do not allow SPACE_LEFT to be less than MIN_NAME_TRIM,
		 * especially because the result of the above operation could
		 * be negative */
		if (space_left < min_name_trim)
			space_left = min_name_trim;

		if ((int)longest < space_left)
			space_left = longest;

		int k = (int)files;
		for (i = 0; i < k; i++) {
			if (max_files != UNSET && i == max_files)
				break;

			if (pager) {
				if (counter > (size_t)(term_rows - 2)) {
					fputs("\x1b[7;97m--Mas--\x1b[0;49m", stdout);

					switch (c = xgetchar()) {
					/* Advance one line at a time */
					case 66: /* fallthrough */ /* Down arrow */
					case 10: /* fallthrough */ /* Enter */
					case 32: /* Space */
						break;

					/* Advance one page at a time */
					case 126:  /* Page Down */
						counter = 0;
						break;

					case 63: /* fallthrough */ /* ? */
					case 104: {		   /* h: Print pager help */
						CLEAR;
						fputs(_("?, h: help\n"
							"Down arrow, Enter, Space: Advance one line\n"
							"Page Down: Advance one page\n"
							"q: Stop pagging\n"), stdout);

						int l = (int)term_rows - 5;
						while (--l >= 0)
							putchar('\n');

						fputs("\x1b[7;97m--Mas--\x1b[0;49m", stdout);

						i -= (term_rows - 1);
						if (i < 0)
							i = 0;

						counter = 0;
						xgetchar();
						CLEAR;
					} break;

					/* Stop paging (and set a flag to reenable the pager
					 * later) */
					case 99:  /* fallthrough */ /* 'c' */
					case 112: /* fallthrough */ /* 'p' */
					case 113:
						pager = 0, reset_pager = 1; /* 'q' */
						break;

					/* If another key is pressed, go back one position.
					 * Otherwise, some file names won't be listed.*/
					default:
						i--;
						fputs("\r\x1b[K\x1b[3J", stdout);
						continue;
					}

					fputs("\r\x1b[K\x1b[3J", stdout);
				}

				counter++;
			}

			/* Print ELN. The remaining part of the line will be
			 * printed by print_entry_props() */
			if (!no_eln)
				printf("%s%d%s ", el_c, i + 1, df_c);

			print_entry_props(&file_info[i], (size_t)space_left);
		}

		goto END;
	}

				/* ########################
				 * #   NORMAL VIEW MODE   #
				 * ######################## */

	int last_column = 0;

	/* Get amount of columns needed to print files in CWD  */
	if (!columned) {
		columns_n = 1;
	} else {
		columns_n = (size_t)term_cols / (longest + 1); /* +1 for the
		space between file names */

		/* If longest is bigger than terminal columns, columns_n will
		 * be negative or zero. To avoid this: */
		if (columns_n < 1)
			columns_n = 1;

		/* If we have only three files, we don't want four columns */
		if (columns_n > (size_t)n)
			columns_n = (size_t)n;
	}

	int nn = (int)n;
	size_t cur_cols = 0;
	for (i = 0; i < nn; i++) {
		if (max_files != UNSET && i == max_files)
			break;

				/* ######################
				 * #   A SIMPLE PAGER   #
				 * ###################### */

		/* A basic pager for directories containing large amount of
		 * files. What's missing? It only goes downwards. To go
		 * backwards, use the terminal scrollback function */
		if (pager) {
			/* Run the pager only once all columns and rows fitting in
			 * the screen are filled with the corresponding file names */
			if (last_column && counter > columns_n * ((size_t)term_rows - 2)) {
				fputs("\x1b[7;97m--Mas--\x1b[0;49m", stdout);

				switch (c = xgetchar()) {
				/* Advance one line at a time */
				case 66: /* fallthrough */ /* Down arrow */
				case 10: /* fallthrough */ /* Enter */
				case 32: /* Space */
					break;

				/* Advance one page at a time */
				case 126: /* Page Down */
					counter = 0; break;

				case 63: /* fallthrough */ /* ? */
				case 104: {		   /* h: Print pager help */
					CLEAR;

					fputs(_("?, h: help\n"
						"Down arrow, Enter, Space: Advance one line\n"
						"Page Down: Advance one page\n"
						"q: Stop pagging\n"), stdout);

					int l = (int)term_rows - 5;
					while (--l >= 0)
						putchar('\n');

					fputs("\x1b[7;97m--Mas--\x1b[0;49m", stdout);

					i -= ((term_rows * columns_n) - 1);

					if (i < 0)
						i = 0;

					counter = 0;
					xgetchar();
					CLEAR;
				} break;

				/* Stop paging (and set a flag to reenable the pager
				 * later) */
				case 99: /* fallthrough */  /* 'c' */
				case 112: /* fallthrough */ /* 'p' */
				case 113:
					pager = 0, reset_pager = 1; /* 'q' */
					break;

				/* If another key is pressed, go back one position.
				 * Otherwise, some file names won't be listed.*/
				default:
					i--;
					fputs("\r\x1b[K\x1b[3J", stdout);
					continue;
				}

				fputs("\r\x1b[K\x1b[3J", stdout);
			}

			counter++;
		}

		/* Determine if current entry is in the last column, in which
		 * case a new line char will be appended */
		if (++cur_cols == columns_n) {
			cur_cols = 0;
			last_column = 1;
		} else {
			last_column = 0;
		}

		file_info[i].eln_n = no_eln ? -1 : DIGINUM(i + 1);
		int ind_char = 1;
		if (!classify)
			ind_char = 0;

			/* #################################
			 * #    PRINT THE CURRENT ENTRY    #
			 * ################################# */

		if (colorize) {
			ind_char = 0;
			if (icons) {
				if (no_eln) {
					printf("%s%s %s%s%s", file_info[i].icon_color,
					    file_info[i].icon, file_info[i].color,
					    file_info[i].name, df_c);
				} else {
					printf("%s%d%s %s%s %s%s%s", el_c, i + 1, df_c,
					    file_info[i].icon_color, file_info[i].icon,
					    file_info[i].color, file_info[i].name, df_c);
				}
			} else {
				if (no_eln) {
					printf("%s%s%s", file_info[i].color,
					    file_info[i].name, df_c);
				} else {
					printf("%s%d%s %s%s%s", el_c, i + 1, df_c,
					    file_info[i].color, file_info[i].name, df_c);
				}
			}

			if (classify) {
				/* Append directory indicator and files counter */
				switch (file_info[i].type) {
				case DT_DIR:
					fputs(" /", stdout);
					if (file_info[i].filesn > 0 && files_counter)
						fputs(xitoa(file_info[i].filesn), stdout);
					break;

				case DT_LNK:
					if (file_info[i].dir)
						fputs(" /", stdout);
					if (file_info[i].filesn > 0 && files_counter)
						fputs(xitoa(file_info[i].filesn), stdout);
					break;
				}
			}
		}

		/* No color */
		else {
			if (icons) {
				if (no_eln)
					printf("%s %s", file_info[i].icon, file_info[i].name);
				else
					printf("%s%d%s %s %s", el_c, i + 1, df_c,
					    file_info[i].icon, file_info[i].name);
			} else {
				if (no_eln) {
					fputs(file_info[i].name, stdout);
				} else {
					printf("%s%d%s %s", el_c, i + 1, df_c, file_info[i].name);
					/*                  fputs(el_c, stdout);
					fputs(xitoa(i + 1), stdout);
					fputs(df_c, stdout);
					putchar(' ');
					fputs(file_info[i].name, stdout); */
				}
			}

			if (classify) {
				/* Append file type indicator */
				switch (file_info[i].type) {
				case DT_DIR:
					ind_char = 0;
					fputs(" /", stdout);
					if (file_info[i].filesn > 0 && files_counter)
						fputs(xitoa(file_info[i].filesn), stdout);
					break;

				case DT_LNK:
					if (file_info[i].dir) {
						ind_char = 0;
						fputs(" /", stdout);
						if (file_info[i].filesn > 0 && files_counter)
							fputs(xitoa(file_info[i].filesn), stdout);
					} else {
						putchar('@');
					}
					break;

				case DT_REG:
					if (file_info[i].exec)
						putchar('*');
					else
						ind_char = 0;
					break;

				case DT_FIFO: putchar('|'); break;
				case DT_SOCK: putchar('='); break;
				case DT_UNKNOWN: putchar('?'); break;
				default: ind_char = 0;
				}
			}
		}

		if (!last_column) {
			/* Pad the current file name to equate the longest file name length */
			int cur_len = (int)file_info[i].eln_n + 1 + (icons ? 3 : 0) + (int)file_info[i].len + (ind_char ? 1 : 0);
			if (file_info[i].dir && classify) {
				cur_len += 2;
				if (file_info[i].filesn > 0 && files_counter && file_info[i].ruser)
					cur_len += DIGINUM((int)file_info[i].filesn);
			}

			int diff = (int)longest - cur_len;
			/* Move the cursor %d columns to the right */
			printf("\x1b[%dC", diff + 1);
/*			register int j;
			for (j = diff + 1; j--;)
				putchar(' '); */
		} else {
			putchar('\n');
		}
	}

	if (!last_column)
		putchar('\n');

				/* #########################
				 * #   POST LISTING STUFF  #
				 * ######################### */

END:
	if (close_dir && closedir(dir) == -1) {
		/* Unhide the cursor */
		fputs("\x1b[?25h", stdout);
		return EXIT_FAILURE;
	}

	if (xargs.list_and_quit == 1)
		exit(exit_code);

	if (reset_pager)
		pager = 1;

	if (max_files != UNSET && (int)files > max_files)
		printf("%d/%zu\n", max_files, files);

	/* Print a dividing line between the files list and the
	 * prompt */
	print_div_line();

	if (dirhist_map) {
		/* Print current, previous, and next entries */
		print_dirhist_map();
		print_div_line();
	}

	if (disk_usage)
		print_disk_usage();

	if (sort_switch)
		print_sort_method();

	if (print_selfiles && sel_n > 0)
		_print_selfiles(term_rows);

	/* Unhide the cursor */
	fputs("\x1b[?25h", stdout);

	/*  clock_t end = clock();
	printf("list_dir time: %f\n", (double)(end-start)/CLOCKS_PER_SEC); */

	return EXIT_SUCCESS;
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
