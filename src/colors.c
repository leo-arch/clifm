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

/* colors.c -- functions to control interface colors */

#include "helpers.h"

#ifdef __linux__
# include <sys/capability.h>
#endif /* __linux__ */
#include <errno.h>
#include <string.h>
#ifndef CLIFM_SUCKLESS
# include <strings.h> /* str(n)casecmp() */
#endif /* CLIFM_SUCKLESS */

/* Only used to check the readline version */
#ifdef __OpenBSD__
  typedef char *rl_cpvfunc_t;
# include <ereadline/readline/readline.h>
#else
# include <readline/readline.h>
#endif /* __OpenBSD__ */

#include "aux.h"
#include "autocmds.h" /* update_autocmd_opts() */
#include "checks.h"
#include "colors.h"
#include "config.h" /* set_div_line() */
#include "file_operations.h"
#include "listing.h"
#include "messages.h"
#include "misc.h"
#include "prompt.h" /* gen_color() */
#include "properties.h" /* get_color_age(), get_color_size() */
#include "sanitize.h"
#include "spawn.h"

#ifndef CLIFM_SUCKLESS
/* qsort(3) is used only by get_colorschemes(), which is not included
 * if CLIFM_SUCKLESS is defined */
# include "sort.h" /* compare_string() (used by qsort(3)) */
#endif /* !CLIFM_SUCKLESS */

#define ON_LSCOLORS (xargs.lscolors == LS_COLORS_GNU              \
		? _(" (on LS_COLORS)") : (xargs.lscolors == LS_COLORS_BSD \
		? _(" (on LSCOLORS)") : ""))

#ifndef CLIFM_SUCKLESS
/* A struct to hold color variables */
struct colors_t {
	char *name;
	char *value;
	size_t namelen;
};

/* A struct to map color codes to color variables */
struct color_mapping_t {
	const char *prefix; /* Color code name (e.g. "el") */
	char *color;        /* Pointer to the color variable (.e.g. el_c) */
	int prefix_len;
	int printable;      /* Either RL_PRINTABLE or RL_NO_PRINTABLE */
};

static struct colors_t *defs;
static size_t defs_n = 0;

/* Xterm-like color names taken from vifm(1) */
static const struct colors_t color_names[] = {
	{"Black", "38;5;0", 5},	{"Red", "38;5;1", 3}, {"Green", "38;5;2", 5},
	{"Yellow", "38;5;3", 6}, {"Blue", "38;5;4", 4}, {"Magenta", "38;5;5", 7},
	{"Cyan", "38;5;6", 4}, {"White", "38;5;7", 5}, {"LightBlack", "38;5;8", 10},
	{"LightRed", "38;5;9", 8}, {"LightGreen", "38;5;10", 10},
	{"LightYellow", "38;5;11", 11}, {"LightBlue", "38;5;12", 9},
	{"LightMagenta", "38;5;13", 12}, {"LightCyan", "38;5;14", 9},
	{"LightWhite", "38;5;15", 10}, {"Grey0", "38;5;16", 5},
	{"NavyBlue", "38;5;17", 8}, {"DarkBlue", "38;5;18", 8},
	{"Blue3", "38;5;19", 5}, {"Blue3_2", "38;5;20", 7},
	{"Blue1", "38;5;21", 5},  {"DarkGreen", "38;5;22", 9},
	{"DeepSkyBlue4", "38;5;23", 12}, {"DeepSkyBlue4_2", "38;5;24", 14},
	{"DeepSkyBlue4_3", "38;5;25", 14}, {"DodgerBlue3", "38;5;26", 11},
	{"DodgerBlue2", "38;5;27", 11}, {"Green4", "38;5;28", 6},
	{"SpringGreen4", "38;5;29", 12}, {"Turquoise4", "38;5;30", 10},
	{"DeepSkyBlue3", "38;5;31", 12}, {"DeepSkyBlue3_2", "38;5;32", 14},
	{"DodgerBlue1", "38;5;33", 11}, {"Green3", "38;5;34", 6},
	{"SpringGreen3", "38;5;35", 12}, {"DarkCyan", "38;5;36", 8},
	{"LightSeaGreen", "38;5;37", 13}, {"DeepSkyBlue2", "38;5;38", 12},
	{"DeepSkyBlue1", "38;5;39", 12}, {"Green3_2", "38;5;40", 8},
	{"SpringGreen3_2", "38;5;41", 14}, {"SpringGreen2", "38;5;42", 12},
	{"Cyan3", "38;5;43", 5}, {"DarkTurquoise", "38;5;44", 13},
	{"Turquoise2", "38;5;45", 10}, {"Green1", "38;5;46", 6},
	{"SpringGreen2_2", "38;5;47", 14}, {"SpringGreen1", "38;5;48", 12},
	{"MediumSpringGreen", "38;5;49", 17}, {"Cyan2", "38;5;50", 5},
	{"Cyan1", "38;5;51", 5}, {"DarkRed", "38;5;52", 7},
	{"DeepPink4", "38;5;53", 9}, {"Purple4", "38;5;54", 7},
	{"Purple4_2", "38;5;55", 9}, {"Purple3", "38;5;56", 7},
	{"BlueViolet", "38;5;57", 10}, {"Orange4", "38;5;58", 7},
	{"Grey37", "38;5;59", 6}, {"MediumPurple4", "38;5;60", 13},
	{"SlateBlue3", "38;5;61", 10}, {"SlateBlue3_2", "38;5;62", 12},
	{"RoyalBlue1", "38;5;63", 10}, {"Chartreuse4", "38;5;64", 11},
	{"DarkSeaGreen4", "38;5;65", 13}, {"PaleTurquoise4", "38;5;66", 14},
	{"SteelBlue", "38;5;67", 9}, {"SteelBlue3", "38;5;68", 10},
	{"CornflowerBlue", "38;5;69", 14}, {"Chartreuse3", "38;5;70", 11},
	{"DarkSeaGreen4_2", "38;5;71", 15}, {"CadetBlue", "38;5;72", 9},
	{"CadetBlue_2", "38;5;73", 11}, {"SkyBlue3", "38;5;74", 8},
	{"SteelBlue1", "38;5;75", 10}, {"Chartreuse3_2", "38;5;76", 13},
	{"PaleGreen3", "38;5;77", 10}, {"SeaGreen3", "38;5;78", 9},
	{"Aquamarine3", "38;5;79", 11}, {"MediumTurquoise", "38;5;80", 15},
	{"SteelBlue1_2", "38;5;81", 12}, {"Chartreuse2", "38;5;82", 11},
	{"SeaGreen2", "38;5;83", 9}, {"SeaGreen1", "38;5;84", 9},
	{"SeaGreen1_2", "38;5;85", 11}, {"Aquamarine1", "38;5;86", 11},
	{"DarkSlateGray2", "38;5;87", 14}, {"DarkRed_2", "38;5;88", 9},
	{"DeepPink4_2", "38;5;89", 11}, {"DarkMagenta", "38;5;90", 11},
	{"DarkMagenta_2", "38;5;91", 13}, {"DarkViolet", "38;5;92", 10},
	{"Purple", "38;5;93", 6}, {"Orange4_2", "38;5;94", 9},
	{"LightPink4", "38;5;95", 10}, {"Plum4", "38;5;96", 5},
	{"MediumPurple3", "38;5;97", 13}, {"MediumPurple3_2", "38;5;98", 15},
	{"SlateBlue1", "38;5;99", 10}, {"Yellow4", "38;5;100", 7},
	{"Wheat4", "38;5;101", 6}, {"Grey53", "38;5;102", 6},
	{"LightSlateGrey", "38;5;103", 14}, {"MediumPurple", "38;5;104", 12},
	{"LightSlateBlue", "38;5;105", 14}, {"Yellow4_2", "38;5;106", 9},
	{"DarkOliveGreen3", "38;5;107", 15}, {"DarkSeaGreen", "38;5;108", 12},
	{"LightSkyBlue3", "38;5;109", 13}, {"LightSkyBlue3_2", "38;5;110", 15},
	{"SkyBlue2", "38;5;111", 8}, {"Chartreuse2_2", "38;5;112", 13},
	{"DarkOliveGreen3_2", "38;5;113", 17}, {"PaleGreen3_2", "38;5;114", 12},
	{"DarkSeaGreen3", "38;5;115", 13}, {"DarkSlateGray3", "38;5;116", 14},
	{"SkyBlue1", "38;5;117", 8}, {"Chartreuse1", "38;5;118", 11},
	{"LightGreen_2", "38;5;119", 12}, {"LightGreen_3", "38;5;120", 12},
	{"PaleGreen1", "38;5;121", 10}, {"Aquamarine1_2", "38;5;122", 13},
	{"DarkSlateGray1", "38;5;123", 14}, {"Red3", "38;5;124", 4},
	{"DeepPink4_3", "38;5;125", 11}, {"MediumVioletRed", "38;5;126", 15},
	{"Magenta3", "38;5;127", 8}, {"DarkViolet_2", "38;5;128", 12},
	{"Purple_2", "38;5;129", 8}, {"DarkOrange3", "38;5;130", 11},
	{"IndianRed", "38;5;131", 9}, {"HotPink3", "38;5;132", 8},
	{"MediumOrchid3", "38;5;133", 13}, {"MediumOrchid", "38;5;134", 12},
	{"MediumPurple2", "38;5;135", 13}, {"DarkGoldenrod", "38;5;136", 13},
	{"LightSalmon3", "38;5;137", 12}, {"RosyBrown", "38;5;138", 9},
	{"Grey63", "38;5;139", 6}, {"MediumPurple2_2", "38;5;140", 15},
	{"MediumPurple1", "38;5;141", 13}, {"Gold3", "38;5;142", 5},
	{"DarkKhaki", "38;5;143", 9}, {"NavajoWhite3", "38;5;144", 12},
	{"Grey69", "38;5;145", 6}, {"LightSteelBlue3", "38;5;146", 15},
	{"LightSteelBlue", "38;5;147", 14}, {"Yellow3", "38;5;148", 7},
	{"DarkOliveGreen3_3", "38;5;149", 17}, {"DarkSeaGreen3_2", "38;5;150", 15},
	{"DarkSeaGreen2", "38;5;151", 13}, {"LightCyan3", "38;5;152", 10},
	{"LightSkyBlue1", "38;5;153", 13}, {"GreenYellow", "38;5;154", 11},
	{"DarkOliveGreen2", "38;5;155", 15}, {"PaleGreen1_2", "38;5;156", 12},
	{"DarkSeaGreen2_2", "38;5;157", 15}, {"DarkSeaGreen1", "38;5;158", 13},
	{"PaleTurquoise1", "38;5;159", 14}, {"Red3_2", "38;5;160", 6},
	{"DeepPink3", "38;5;161", 9}, {"DeepPink3_2", "38;5;162", 11},
	{"Magenta3_2", "38;5;163", 10}, {"Magenta3_3", "38;5;164", 10},
	{"Magenta2", "38;5;165", 8}, {"DarkOrange3_2", "38;5;166", 13},
	{"IndianRed_2", "38;5;167", 11}, {"HotPink3_2", "38;5;168", 10},
	{"HotPink2", "38;5;169", 8}, {"Orchid", "38;5;170", 6},
	{"MediumOrchid1", "38;5;171", 13}, {"Orange3", "38;5;172", 7},
	{"LightSalmon3_2", "38;5;173", 14}, {"LightPink3", "38;5;174", 10},
	{"Pink3", "38;5;175", 5}, {"Plum3", "38;5;176", 5},
	{"Violet", "38;5;177", 6}, {"Gold3_2", "38;5;178", 7},
	{"LightGoldenrod3", "38;5;179", 15}, {"Tan", "38;5;180", 3},
	{"MistyRose3", "38;5;181", 10}, {"Thistle3", "38;5;182", 8},
	{"Plum2", "38;5;183", 5}, {"Yellow3_2", "38;5;184", 9},
	{"Khaki3", "38;5;185", 6}, {"LightGoldenrod2", "38;5;186", 15},
	{"LightYellow3", "38;5;187", 12}, {"Grey84", "38;5;188", 6},
	{"LightSteelBlue1", "38;5;189", 15}, {"Yellow2", "38;5;190", 7},
	{"DarkOliveGreen1", "38;5;191", 15}, {"DarkOliveGreen1_2", "38;5;192", 17},
	{"DarkSeaGreen1_2", "38;5;193", 15}, {"Honeydew2", "38;5;194", 9},
	{"LightCyan1", "38;5;195", 10}, {"Red1", "38;5;196", 4},
	{"DeepPink2", "38;5;197", 9}, {"DeepPink1", "38;5;198", 9},
	{"DeepPink1_2", "38;5;199", 11}, {"Magenta2_2", "38;5;200", 10},
	{"Magenta1", "38;5;201", 8}, {"OrangeRed1", "38;5;202", 10},
	{"IndianRed1", "38;5;203", 10}, {"IndianRed1_2", "38;5;204", 12},
	{"HotPink", "38;5;205", 7}, {"HotPink_2", "38;5;206", 9},
	{"MediumOrchid1_2", "38;5;207", 15}, {"DarkOrange", "38;5;208", 10},
	{"Salmon1", "38;5;209", 7}, {"LightCoral", "38;5;210", 10},
	{"PaleVioletRed1", "38;5;211", 14}, {"Orchid2", "38;5;212", 7},
	{"Orchid1", "38;5;213", 7}, {"Orange1", "38;5;214", 7},
	{"SandyBrown", "38;5;215", 10}, {"LightSalmon1", "38;5;216", 12},
	{"LightPink1", "38;5;217", 10}, {"Pink1", "38;5;218", 5},
	{"Plum1", "38;5;219", 5}, {"Gold1", "38;5;220", 5},
	{"LightGoldenrod2_2", "38;5;221", 17}, {"LightGoldenrod2_3", "38;5;222", 17},
	{"NavajoWhite1", "38;5;223", 12}, {"MistyRose1", "38;5;224", 10},
	{"Thistle1", "38;5;225", 8}, {"Yellow1", "38;5;226", 7},
	{"LightGoldenrod1", "38;5;227", 15}, {"Khaki1", "38;5;228", 6},
	{"Wheat1", "38;5;229", 6}, {"Cornsilk1", "38;5;230", 9},
	{"Grey100", "38;5;231", 7}, {"Grey3", "38;5;232", 5},
	{"Grey7", "38;5;233", 5}, {"Grey11", "38;5;234", 6},
	{"Grey15", "38;5;235", 6}, {"Grey19", "38;5;236", 6},
	{"Grey23", "38;5;237", 6}, {"Grey27", "38;5;238", 6},
	{"Grey30", "38;5;239", 6}, {"Grey35", "38;5;240", 6},
	{"Grey39", "38;5;241", 6}, {"Grey42", "38;5;242", 6},
	{"Grey46", "38;5;243", 6}, {"Grey50", "38;5;244", 6},
	{"Grey54", "38;5;245", 6}, {"Grey58", "38;5;246", 6},
	{"Grey62", "38;5;247", 6}, {"Grey66", "38;5;248", 6},
	{"Grey70", "38;5;249", 6}, {"Grey74", "38;5;250", 6},
	{"Grey78", "38;5;251", 6}, {"Grey82", "38;5;252", 6},
	{"Grey85", "38;5;253", 6}, {"Grey89", "38;5;254", 6},
	{"Grey93", "38;5;255", 6},
	{NULL, NULL, 0},
};
#endif /* !CLIFM_SUCKLESS */

/* Turn the first or second field of a color code sequence, provided
 * it is either 1 or 01 (bold attribute), into 0 (regular).
 * It returns no value: the change is made in place via a pointer.
 * STR must be a color code with this form: \x1b[xx;xx;xx...
 * It cannot handle the bold attribute beyond the second field.
 * Though this is usually enough, it's far from ideal.
 *
 * This function is used to print properties strings ('p' command
 * and long view mode). It takes the user defined color of the
 * corresponding file type (e.g., dirs) and removes the bold attribute.
 * Also used when running with --no-bold. */
void
remove_bold_attr(char *str)
{
	if (!str || !*str)
		return;

	char *p = str, *q = str;
	size_t c = 0;

	while (1) {
		if (*p == '\\' && *(p + 1) == 'x'
		&& *(p + 2) == '1' && *(p + 3) == 'b') {
			if (*(p + 4)) {
				p += 4;
				q = p;
				continue;
			} else {
				break;
			}
		}

		if (*p == '[') {
			p++;
			q = p;
			continue;
		}

		/* Skip leading "0;" or "00;" */
		if (*p == '0' && (p[1] == ';' || (p[1] == '0' && p[2] == ';'))) {
			p += p[1] == ';' ? 2 : 3;
			q = p;
		}

		if ( (*q == '0' && *(q + 1) == '1' && (*(q + 2) == ';'
		|| *(q + 2) == 'm')) || (*q == '1' && (*(q + 1) == 'm'
		|| *(q + 1) == ';')) ) {
			if (*q == '0')
				*(q + 1) = '0';
			else
				*q = '0';
			break;
		}

		if (*p == ';' && *(p + 1)) {
			q = p + 1;
			c++;
		}

		p++;
		if (!*p || c >= 2)
			break;
	}
}

/* Return the color for the regular file FILENAME, whose attributes are A.
 * IF the color comes from the file extension, IS_EXT is updated to the length
 * of the color code (otherwise, it is set to zero). */
char *
get_regfile_color(const char *filename, const struct stat *a, size_t *is_ext)
{
	*is_ext = 0;
	if (conf.colorize == 0)
		return fi_c;

	if (*nf_c && check_file_access(a->st_mode, a->st_uid, a->st_gid) == 0)
		return nf_c;

	char *color = get_file_color(filename, a);
	if (color != fi_c || conf.check_ext == 0)
		return color ? color : fi_c;

	char *ext = strrchr(filename, '.');
	if (!ext)
		return color ? color : fi_c;

	size_t color_len = 0;
	char *extcolor = get_ext_color(ext, &color_len);
	if (!extcolor || color_len == 0 || color_len + 4 > sizeof(tmp_color))
		return color ? color : fi_c;

	*tmp_color = '\x1b'; tmp_color[1] = '[';
	memcpy(tmp_color + 2, extcolor, color_len);
	tmp_color[color_len + 2] = 'm';
	tmp_color[color_len + 3] = '\0';

	*is_ext = color_len + 3;
	return tmp_color;
}

/* Retrieve the color corresponding to dir FILENAME whose attributes are A.
 * If COUNT > -1, we already know whether the directory is populatoed or not:
 * use this value for FILES_DIR (do not run count_dir()). */
char *
get_dir_color(const char *filename, const struct stat *a,
	const filesn_t count)
{
	const mode_t mode = a->st_mode;
	if (*nd_c && check_file_access(mode, a->st_uid, a->st_gid) == 0)
		return nd_c;

	const int sticky = (mode & S_ISVTX);
	const int is_oth_w = (mode & S_IWOTH);
	const filesn_t links = (filesn_t)a->st_nlink;

	/* Let's find out whether the directory is populated. A positive value
	 * means that it is actually populated (it has at least one file,
	 * not counting self and parent dirs). */
	const filesn_t files_in_dir = count > -1 ? count : (links > 2
		? links : count_dir(filename, CPOP) - 2);

	if (files_in_dir < 0 && *nd_c) /* count_dir() failed. */
		return nd_c;

	return (sticky ? (is_oth_w ? tw_c : st_c) : is_oth_w ? ow_c
		: (files_in_dir == 0 ? ed_c : di_c));
}

char *
get_file_color(const char *filename, const struct stat *a)
{
	const mode_t mode = a->st_mode;

#ifdef LINUX_FILE_CAPS
	cap_t cap;
#else
	UNUSED(filename);
#endif /* LINUX_FILE_CAPS */

	if (mode & S_ISUID) return su_c; /* SUID */
	if (mode & S_ISGID) return sg_c; /* SGID */

#ifdef LINUX_FILE_CAPS
	if (conf.check_cap == 1 && (cap = cap_get_file(filename))) {
		cap_free(cap);
		return ca_c;
	}
#endif /* LINUX_FILE_CAPS */

	if ((mode & S_IXUSR) || (mode & S_IXGRP) || (mode & S_IXOTH)) /* Exec */
		return FILE_SIZE_PTR(a) == 0 ? ee_c : ex_c;

	if (a->st_nlink > 1) return mh_c; /* Multi-hardlink */

	return (FILE_SIZE_PTR(a) == 0) ? ef_c : fi_c;
}

/* Validate a hex color code string with this format: RRGGBB-[1-9] or RGB-[1-9]. */
static int
is_hex_color(const char *str)
{
	if (!str || !*str)
		return 0;

	size_t c = 0;

	while (*str) {
		c++;
		if ((c == 7 || c == 4) && *str == '-') {
			if (!str[1])
				return 0;
			return (str[1] >= '0' && str[1] <= '9');
		}
		if ( !( (*str >= '0' && *str <= '9') || (*str >= 'a' && *str <= 'f')
		|| (*str >= 'A' && *str <= 'F') ) )
			return 0;
		str++;
	}

	if (c != 6 && c != 3)
		return 0;

	return 1;
}

/* Validate a 256 color code string with this format: [0-999]-[0-9]. */
static int
is_256_color(const char *str)
{
	if (!str || !*str)
		return 0;

	const char *s = str;

	size_t c = 0;
	while (s[c]) {
		if (c == 0 && (s[c] < '0' || s[c] > '9'))
			return 0;
		if (c == 1 && ((s[c] < '0' || s[c] > '9') && s[c] != '-'))
			return 0;
		if (c == 2 && ((s[c] < '0' || s[c] > '9') && s[c] != '-'))
			return 0;
		if (c == 3 && ((s[c] < '0' || s[c] > '9') && s[c] != '-'))
			return 0;
		if (c == 4 && (s[c] < '0' || s[c] > '9'))
			return 0;
		if (c == 5)
			return 0;
		c++;
	}

	return 1;
}

/* Check if STR has the format of a color code string (a number or a
 * semicolon list (max 12 fields) of numbers of at most 3 digits each).
 * Hex color codes (#RRGGBB) and 256 colors short (@NUM) are also validated.
 * Returns 1 if true and 0 if false. */
static int
is_color_code(const char *str)
{
	if (!str || !*str)
		return 0;

	if (*str == RGB_COLOR_PREFIX)
		return is_hex_color(str + 1);

	if (*str == COLOR256_PREFIX)
		return is_256_color(str + 1);

	size_t digits = 0, semicolon = 0;

	while (*str) {
		if (*str >= '0' && *str <= '9') {
			digits++;
		} else if (*str == ';') {
			if (str[1] == ';') /* Consecutive semicolons. */
				return 0;
			digits = 0;
			semicolon++;
		} else {
			if (*str != '\n'
			/* Allow styled unerlines. */
			&& !(digits > 0 && *(str - 1) == '4' && *str == ':'
			&& str[1] && str[1] >= '0' && str[1] <= '5'))
				/* Neither digit nor semicolon. */
				return 0;
		}
		str++;
	}

	/* No digits at all, ending semicolon, more than eleven fields, or
	 * more than three consecutive digits. */
	if (!digits || digits > 3 || semicolon > 11)
		return 0;

	/* At this point, we have a semicolon separated string of digits (3
	 * consecutive max) with at most 12 fields. The only thing not
	 * validated here are numbers themselves. */
	return 1;
}

static void
check_rl_version_and_warn(void)
{
	if (rl_readline_version >= 0x0700)
		return;

	err('w', PRINT_PROMPT, _("%s: Escape sequence detected in the "
		"warning prompt string: this might cause a few glichtes in the "
		"prompt due to some bugs in the current readline library (%s). "
		"Please consider removing these escape sequences (via either "
		"'prompt edit' or 'cs edit') or upgrading to a newer version "
		"of the library (>= 7.0 is recommended).\n"),
		PROGRAM_NAME, rl_library_version);
}

/* Same as update_warning_prompt_text_color() but for the new color syntax
 * (%{color}) */
static void
update_warning_prompt_text_color_new_syntax(void)
{
	char *start = strrchr(conf.wprompt_str, '%');
	if (!start || start[1] != '{')
		return;

	start++;
	char *color = gen_color(&start);
	if (!color || !*color) {
		free(color);
		return;
	}

	/* Remove trailing \002 and leading \001 */
	size_t len = strlen(color);
	color[len - 1] = '\0'; /* LEN is guaranteed to be > 0 */
	len--;

	if (len < sizeof(wp_c)) {
		memcpy(wp_c, color + 1, len);
		wp_c[len] = '\0';
	}
	free(color);

	check_rl_version_and_warn();
}

/* Update the wp_c color code to match the last color used in the warning
 * prompt string.
 * NOTE: If we don't do this, the text entered in the warning prompt (wp_c)
 * won't match the warning prompt color. */
void
update_warning_prompt_text_color(void)
{
	if (!conf.wprompt_str || !*conf.wprompt_str)
		return;

	/* Let's look for the last "\e[" */
	char *start = strrchr(conf.wprompt_str, '[');
	if (!start || start - conf.wprompt_str < 2 || *(start - 1) != 'e'
	|| *(start - 2) != '\\' || !IS_DIGIT(*(start + 1))) {
		/* Let's check the new color notation (%{color}) as well */
		update_warning_prompt_text_color_new_syntax();
		return;
	}

	start++;
	char *end = strchr(start, 'm');
	if (!end)
		return;

	*end = '\0';
	if (is_color_code(start)) {
		snprintf(wp_c, sizeof(wp_c), "\x1b[%sm", start);

		check_rl_version_and_warn();
	}

	*end = 'm';
}

#ifndef CLIFM_SUCKLESS
/* If STR is a valid Xterm-like color name, return the value for this name.
 * If an attribute is appended to the name (e.g.: NAME-1), return value for this
 * name plus the corresponding attribute. */
static char *
check_names(const char *str)
{
	char attr = 0;
	char *dash = strchr(str, '-');
	if (dash && *(dash + 1)) {
		*dash = '\0';
		attr = *(dash + 1);
	}

	int found = -1;
	char up_str = (char)TOUPPER(*str);
	size_t len = strlen(str);
	size_t i;

	for (i = 0; color_names[i].name; i++) {
		if (up_str == *color_names[i].name /* All names start with upper case */
		&& color_names[i].namelen == len
		&& strcasecmp(str + 1, color_names[i].name + 1) == 0) {
			found = (int)i;
			break;
		}
	}

	if (found == -1)
		return (char *)NULL;

	if (attr == 0)
		return color_names[found].value;

	snprintf(tmp_color, sizeof(tmp_color), "%c;%s", attr,
		color_names[found].value);
	return tmp_color;
}

/* If STR is a valid color variable name, return the value of this variable. */
static char *
check_defs(const char *str)
{
	if (defs_n == 0 || !str || !*str)
		return (char *)NULL;

	char *val = (char *)NULL;
	int i = (int)defs_n;
	const size_t slen = strlen(str);

	while (--i >= 0) {
		if (!defs[i].name || !defs[i].value || !*defs[i].name
		|| !*defs[i].value)
			continue;

		if (*defs[i].name == *str && slen == defs[i].namelen
		&& strcmp(defs[i].name, str) == 0
		&& is_color_code(defs[i].value) == 1) {
			val = defs[i].value;
			return val;
		}
	}

	if (!val)
		val = check_names(str);

	return val;
}

/* Free custom color variables set from the color scheme file. */
static void
clear_defs(void)
{
	if (defs_n == 0)
		goto END;

	int i = (int)defs_n;
	while (--i >= 0) {
		free(defs[i].name);
		free(defs[i].value);
	}

	defs_n = 0;
END:
	free(defs);
	defs = (struct colors_t *)NULL;
}
#endif /* !CLIFM_SUCKLESS */

static int
bcomp(const void *a, const void *b)
{
	size_t pa = *(size_t *)a;
	struct ext_t *pb = (struct ext_t *)b;

	if (pa < pb->hash) return (-1);
	if (pa > pb->hash) return 1;
    return 0;
}

/* Look for the hash HASH in the hash table for filename extensions.
 * Return a pointer to the corresponding color if found, or NULL.
 * If VAL_LEN isn't NULL, it is updated with the length of the returned value. */
static char *
check_ext_hash(const size_t hash, size_t *val_len)
{
	struct ext_t *ptr = bsearch(&hash, ext_colors, ext_colors_n,
		sizeof(struct ext_t), bcomp);

	if (!ptr || !ptr->value || !*ptr->value)
		return (char *)NULL;

	if (val_len)
		*val_len = ptr->value_len;

	return ptr->value;
}

/* Return the color code associated to the file extension EXT, updating
 * VAL_LEN, if not NULL, to the length of this color code. */
static char *
check_ext_string(const char *ext, size_t *val_len)
{
	/* Hold extension names. NAME_MAX should be enough: no filename should
	 * go beyond NAME_MAX, so it's pretty safe to assume that no file extension
	 * will be larger than this. */
	static char tmp_ext[NAME_MAX];
	char *ptr = tmp_ext;

	int i;
	for (i = 0; i < NAME_MAX && ext[i]; i++)
		tmp_ext[i] = TOLOWER(ext[i]);
	tmp_ext[i] = '\0';

	const size_t len = (size_t)i;

	i = (int)ext_colors_n;
	while (--i >= 0) {
		if (!ext_colors[i].name || !*ext_colors[i].name
		|| ext_colors[i].len != len || *ptr != TOLOWER(*ext_colors[i].name))
			continue;

		char *p = ptr + 1, *q = ext_colors[i].name + 1;

		size_t match = 1;
		while (*p) {
			if (*p != TOLOWER(*q)) {
				match = 0;
				break;
			}
			p++;
			q++;
		}

		if (match == 0 || *q != '\0')
			continue;

		if (val_len)
			*val_len = ext_colors[i].value_len;

		return ext_colors[i].value;
	}

	return (char *)NULL;
}

/* Returns a pointer to the corresponding color code for the file
 * extension EXT (updating VAL_LEN to the length of this code).
 * The hash table is checked first if we have no hash conflicts. Otherwise,
 * a regular string comparison is performed to resolve it. */
char *
get_ext_color(const char *ext, size_t *val_len)
{
	if (!ext || !*ext || !*(++ext) || ext_colors_n == 0)
		return (char *)NULL;

	/* If the hash field at index 0 is set to zero, we have hash conflicts. */
	if (ext_colors[0].hash != 0)
		return check_ext_hash(hashme(ext, 0), val_len);

	return check_ext_string(ext, val_len);
}

#ifndef CLIFM_SUCKLESS
/* Strip color line from the config file (FiletypeColors, if MODE is
 * 't', and ExtColors, if MODE is 'x') returning the same string
 * containing only allowed characters. */
static char *
strip_color_line(const char *str, const size_t str_len)
{
	if (!str || !*str)
		return (char *)NULL;

	char *buf = xnmalloc(str_len + 1, sizeof(char));
	size_t len = 0;

	while (*str) {
		if (IS_ALNUM(*str) || *str == '=' || *str == ';' || *str == ':'
		|| *str == RGB_COLOR_PREFIX || *str == COLOR256_PREFIX
		|| *str == '-' || *str == '_')
			{buf[len] = *str; len++;}
		str++;
	}

	if (!len || !*buf) {
		free(buf);
		return (char *)NULL;
	}

	buf[len] = '\0';
	return buf;
}
#endif /* !CLIFM_SUCKLESS */

void
reset_filetype_colors(void)
{
	*bd_c = '\0'; *bk_c = '\0'; *ca_c = '\0'; *cd_c = '\0';
	*di_c = '\0'; *ed_c = '\0'; *ee_c = '\0'; *ef_c = '\0';
	*ex_c = '\0'; *fi_c = '\0'; *ln_c = '\0'; *mh_c = '\0';
	*nd_c = '\0'; *nf_c = '\0'; *no_c = '\0';
#ifdef SOLARIS_DOORS
	*oo_c = '\0';
#endif /* SOLARIS_DOORS */
	*or_c = '\0'; *ow_c = '\0'; *pi_c = '\0'; *sg_c = '\0';
	*so_c = '\0'; *st_c = '\0'; *su_c = '\0'; *tw_c = '\0';
	*uf_c = '\0';
}

void
reset_iface_colors(void)
{
	*hb_c = '\0'; *hc_c = '\0'; *hd_c = '\0'; *he_c = '\0';
	*hn_c = '\0'; *hp_c = '\0'; *hq_c = '\0'; *hr_c = '\0';
	*hs_c = '\0'; *hv_c = '\0'; *hw_c = '\0';

	*sb_c = '\0'; *sc_c = '\0'; *sd_c = '\0'; *sf_c = '\0';
	*sh_c = '\0'; *sp_c = '\0'; *sx_c = '\0'; *sz_c = '\0';

	*ac_c = '\0'; *df_c = '\0'; *dl_c = '\0'; *el_c = '\0';
	*em_c = '\0'; *fc_c = '\0'; *li_c = '\0'; *li_cb = '\0';
	*mi_c = '\0'; *nm_c = '\0'; *ro_c = '\0'; *si_c = '\0';
	*ti_c = '\0'; *tt_c = '\0'; *ts_c = '\0'; *tx_c = '\0';
	*wc_c = '\0'; *wm_c = '\0'; *wp_c = '\0'; *xf_c = '\0';
	*xf_cb = '\0'; *xs_c = '\0'; *xs_cb = '\0';

	*ws1_c = '\0'; *ws2_c = '\0'; *ws3_c = '\0'; *ws4_c = '\0';
	*ws5_c = '\0'; *ws6_c = '\0'; *ws7_c = '\0'; *ws8_c = '\0';

	*db_c = '\0'; *dd_c = '\0'; *de_c = '\0'; *dg_c = '\0';
	*dk_c = '\0'; *dn_c = '\0'; *do_c = '\0'; *dp_c = '\0';
	*dr_c = '\0'; *dt_c = '\0'; *du_c = '\0'; *dw_c = '\0';
	*dxd_c = '\0'; *dxr_c = '\0'; *dz_c = '\0';
}

/* Import the color scheme NAME from DATADIR (usually /usr/local/share).
 * Return zero on success or one on failure. */
int
import_color_scheme(const char *name)
{
	if (!data_dir || !*data_dir || !colors_dir || !*colors_dir
	|| !name || !*name)
		return FUNC_FAILURE;

	char dfile[PATH_MAX + 1];
	snprintf(dfile, sizeof(dfile), "%s/%s/colors/%s.clifm", data_dir,
		PROGRAM_NAME, name);

	struct stat a;
	if (stat(dfile, &a) == -1 || !S_ISREG(a.st_mode))
		return FUNC_FAILURE;

	char *cmd[] = {"cp", "--", dfile, colors_dir, NULL};
	const mode_t old_mask = umask(0077); /* flawfinder: ignore */
	const int ret = launch_execv(cmd, FOREGROUND, E_NOFLAG);
	umask(old_mask); /* flawfinder: ignore */

	return ret == FUNC_SUCCESS ? ret : FUNC_FAILURE;
}

#ifndef CLIFM_SUCKLESS
static int
list_colorschemes(void)
{
	if (cschemes_n == 0) {
		puts(_("cs: No color scheme found"));
		return FUNC_SUCCESS;
	}

	const char *ptr = SET_MISC_PTR;
	size_t i;
	for (i = 0; color_schemes[i]; i++) {
		if (cur_cscheme == color_schemes[i]) {
			printf("%s%s%s %s%s\n", mi_c, ptr, df_c, color_schemes[i],
				ON_LSCOLORS);
		} else {
			printf("  %s\n", color_schemes[i]);
		}
	}

	return FUNC_SUCCESS;
}

/* Edit the current color scheme file.
 * If the file is not in the local colors dir, try to copy it from DATADIR.
 * into the local dir to avoid permission issues. */
static int
edit_colorscheme(char *app)
{
	if (!colors_dir) {
		xerror("%s\n", _("cs: No color scheme found"));
		return FUNC_FAILURE;
	}

	if (!cur_cscheme) {
		xerror("%s\n", _("cs: Current color scheme is unknown"));
		return FUNC_FAILURE;
	}

	struct stat attr;
	char file[PATH_MAX + 1];

	snprintf(file, sizeof(file), "%s/%s.clifm", colors_dir, cur_cscheme);
	if (stat(file, &attr) == -1
	&& import_color_scheme(cur_cscheme) != FUNC_SUCCESS) {
		xerror(_("cs: '%s': No such color scheme\n"), cur_cscheme);
		return FUNC_FAILURE;
	}

	if (stat(file, &attr) == -1) {
		xerror("cs: '%s': %s\n", file, strerror(errno));
		return errno;
	}

	const time_t mtime_bfr = attr.st_mtime;

	const int ret = open_config_file(app, file);
	if (ret != FUNC_SUCCESS)
		return ret;

	if (stat(file, &attr) == -1) {
		xerror("cs: '%s': %s\n", file, strerror(errno));
		return errno;
	}

	if (mtime_bfr != attr.st_mtime
	&& set_colors(cur_cscheme, 0) == FUNC_SUCCESS) {
		set_fzf_preview_border_type();
		if (conf.autols == 1)
			reload_dirlist();
	}

	return ret;
}

static int
set_colorscheme(char *arg)
{
	if (!arg || !*arg)
		return FUNC_FAILURE;

	char *p = unescape_str(arg, 0);
	char *q = p ? p : arg;

	size_t i, cs_found = 0;
	for (i = 0; color_schemes[i]; i++) {
		if (*q != *color_schemes[i]
		|| strcmp(q, color_schemes[i]) != 0)
			continue;
		cs_found = 1;

		if (set_colors(q, 0) != FUNC_SUCCESS)
			continue;
		cur_cscheme = color_schemes[i];

		switch_cscheme = 1;
		if (conf.autols == 1)
			reload_dirlist();
		switch_cscheme = 0;

		free(p);

		return FUNC_SUCCESS;
	}

	if (cs_found == 0)
		xerror(_("cs: '%s': No such color scheme\n"), p);

	free(p);

	return FUNC_FAILURE;
}
#endif /* !CLIFM_SUCKLESS */

static char *
get_color_scheme_name(void)
{
	if (cur_cscheme && *cur_cscheme)
		return cur_cscheme;

	if (term_caps.color >= 256)
		return _("builtin (256 colors)");

	return _("builtin (8 colors)");
}

static int
print_colors_tip(const int stealth)
{
	xerror(_("%s: %s.\nTIP: To edit the "
		"color scheme use the following environment "
		"variables: CLIFM_FILE_COLORS, CLIFM_IFACE_COLORS, "
		"and CLIFM_EXT_COLORS.\nExample:\n\n"
		"CLIFM_FILE_COLORS=\"di=31:ln=33:\" CLIFM_IFACE_COLORS=\"el=35:fc=34:\" "
		"CLIFM_EXT_COLORS=\"*.c=1;33:*.odt=4;35:\" clifm\n\n"
		"Consult the manpage for more information.\n"),
		PROGRAM_NAME, stealth == 1 ? STEALTH_DISABLED : NOT_AVAILABLE);
	return FUNC_FAILURE;
}

static void
print_ext_conflict(const char *a, const char *b)
{
	if (!a || !b)
		return;

	if (*a == *b && strcmp(a, b) == 0)
		printf(_("'%s' has conflicting definitions\n"), a);
	else
		printf(_("'%s' conflicts with '%s'\n"), a, b);
}

/* Make sure hashes for filename extensions do not conflict.
 * CS_CHECK is 0 when this function is called at startup: if a hash conflict
 * is found, the hash field at index zero (of the ext_colors struct) is set
 * to 0 to indicate that we must use regular string comparison (slower).
 * CS_CHECK is 1 when the function is invoked by the 'cs check-ext' command.
 * It returns FUNC_FAILURE in case of conflicts, or FUNC_SUCCESS otherwise. */
static int
check_ext_color_hash_conflicts(const int cs_check)
{
	size_t i, j, conflicts = 0;

	for (i = 0; i < ext_colors_n; i++) {
		for (j = i + 1; j < ext_colors_n; j++) {
			if (ext_colors[i].hash != ext_colors[j].hash)
				continue;

			if (ext_colors[i].value_len == ext_colors[j].value_len
			&& strcmp(ext_colors[i].value, ext_colors[j].value) == 0)
				/* Two extensions with the same hash, but pointing to the
				 * same color. Most likely a duplicate entry: let it be. */
				continue;

			if (cs_check == 1) {
				print_ext_conflict(ext_colors[i].name, ext_colors[j].name);
				conflicts++;
				continue;
			}

			ext_colors[0].hash = 0;
			err('w', PRINT_PROMPT, _("%s: File extension conflicts "
				"found. Run 'cs check-ext' to see the details.\n"),
				PROGRAM_NAME);
			return FUNC_FAILURE;
		}
	}

	if (cs_check == 0)
		return FUNC_SUCCESS;

	if (conflicts > 0) {
		if (xargs.lscolors != LS_COLORS_GNU)
			puts(_("Run 'cs edit' to fix these conflicts"));
		return FUNC_FAILURE;
	}

	puts(_("cs: No conflicts found"));
	return FUNC_SUCCESS;
}

int
cschemes_function(char **args)
{
#ifdef CLIFM_SUCKLESS
	UNUSED(args);
	print_colors_tip(0);
	fputs(_("\nYou can also edit 'settings.h' in the source code and "
		"recompile.\n"), stderr);
	return FUNC_FAILURE;
#else
	if (!args)
		return FUNC_FAILURE;

	if (args[1] && *args[1] == 'p' && (!args[1][1]
	|| strcmp(args[1], "preview") == 0)) {
		color_codes();
		return FUNC_SUCCESS;
	}

	if (args[1] && *args[1] == 'n' && (!args[1][1]
	|| strcmp(args[1], "name") == 0)) {
		printf(_("Current color scheme: '%s'\n"),
			get_color_scheme_name());
		return FUNC_SUCCESS;
	}

	if (args[1] && *args[1] == 'c' && strcmp(args[1], "check-ext") == 0)
		return check_ext_color_hash_conflicts(1);

	if (args[1] && IS_HELP(args[1])) {
		puts(_(CS_USAGE));
		return FUNC_SUCCESS;
	}

	if (xargs.stealth_mode == 1)
		return print_colors_tip(1);

	if (conf.colorize == 0) {
		printf(_("%s: Colors are disabled\n"), PROGRAM_NAME);
		return FUNC_FAILURE;
	}

	if (!args[1])
		return list_colorschemes();

	if (*args[1] == 'e' && (!args[1][1] || strcmp(args[1], "edit") == 0))
		return edit_colorscheme(args[2]);

	const int ret = set_colorscheme(args[1]);
	update_autocmd_opts(AC_COLOR_SCHEME);
	return ret;
#endif /* CLIFM_SUCKLESS */
}

/* Convert a @NUM color string to the proper ANSI code representation.
 * Return a pointer to the converted string on success or NULL on error. */
static char *
color256_to_ansi(char *s)
{
	if (!s || !*s || !s[1])
		return (char *)NULL;

	int attr = -1;

	char *q = strchr(s + 1, '-');
	if (q) {
		*q = '\0';
		if (IS_DIGIT(q[1]) && !q[2])
			attr = q[1] - '0';
	}

	char *ret = (char *)NULL;
	const int n = atoi(s + 1);

	if (n >= 0 && n <= 255) {
		if (attr == -1) /* No attribute */
			snprintf(tmp_color, sizeof(tmp_color), "38;5;%d", n);
		else
			snprintf(tmp_color, sizeof(tmp_color), "%d;38;5;%d", attr, n);

		ret = tmp_color;
	}

	if (q)
		*q = '-';

	return ret;
}

/* Decode the prefixed color string S to the proper ANSI representation
 * Returns a pointer to the decoded string on success, or NULL or error. */
static char *
decode_color_prefix(char *s)
{
	if (!s || !*s)
		return (char *)NULL;

	if (*s == RGB_COLOR_PREFIX)
		return hex2rgb(s);
	if (*s == COLOR256_PREFIX)
		return color256_to_ansi(s);

	return (char *)NULL;
}

/* Return a pointer to a statically allocated buffer storing the color code
 * S with starting \001 and ending \002 removed. */
static char *
remove_ctrl_chars(char *s)
{
	if (*s != 001)
		return s;

	s++;

	xstrsncpy(tmp_color, s, sizeof(tmp_color));

	const size_t l = strlen(tmp_color);
	if (l > 0 && tmp_color[l - 1] == 002)
		tmp_color[l - 1] = '\0';

	return tmp_color;
}

/* Set color variable VAR (static global) to COLOR.
 * If not printable, add non-printing char flags (\001 and \002). */
static void
set_color(char *color, char var[], const int flag)
{
#ifndef CLIFM_SUCKLESS
	char *def_color = (char *)NULL;
	if (is_color_code(color) == 0
	&& (def_color = check_defs(color)) == NULL)
#else
	if (is_color_code(color) == 0)
#endif /* !CLIFM_SUCKLESS */
	{
		/* A NULL color string will be set to the default value by
		 * set_default_colors function. */
		*var = '\0';
		return;
	}

#ifndef CLIFM_SUCKLESS
	char *p = def_color ? def_color : color;
#else
	char *p = color;
#endif /* !CLIFM_SUCKLESS */

	char *s = (char *)NULL;
	if (IS_COLOR_PREFIX(*p) && !(s = decode_color_prefix(p))) {
		*var = '\0';
		return;
	}

	if (flag == RL_NO_PRINTABLE)
		snprintf(var, MAX_COLOR + 2, "\001\x1b[%sm\002", s ? s : p);
	else
		snprintf(var, MAX_COLOR - 1, "\x1b[0;%sm", s ? s : p);
}

static void
set_filetype_colors(char **colors, const size_t num_colors)
{
	if (!colors || num_colors == 0)
		return;

	const int p = RL_PRINTABLE;
	struct color_mapping_t mappings[] = {
		{"bd", bd_c, 2, p}, {"ca", ca_c, 2, p}, {"cd", cd_c, 2, p},
		{"di", di_c, 2, p}, {"ed", ed_c, 2, p}, {"ee", ee_c, 2, p},
		{"ef", ef_c, 2, p}, {"ex", ex_c, 2, p}, {"fi", fi_c, 2, p},
		{"ln", ln_c, 2, p}, {"mh", mh_c, 2, p}, {"mi", uf_c, 2, p},
		{"nd", nd_c, 2, p}, {"nf", nf_c, 2, p}, {"no", no_c, 2, p},
		{"or", or_c, 2, p}, {"ow", ow_c, 2, p},
#ifdef SOLARIS_DOORS
		{"oo", oo_c, 2, p},
#endif /* SOLARIS_DOORS */
		{"pi", pi_c, 2, p}, {"sg", sg_c, 2, p}, {"so", so_c, 2, p},
		{"st", st_c, 2, p}, {"su", su_c, 2, p}, {"tw", tw_c, 2, p},
		{"uf", uf_c, 2, p},
	};

	const size_t mappings_n = sizeof(mappings) / sizeof(struct color_mapping_t);
	size_t i;

	for (i = 0; i < num_colors; i++) {
		if (!colors[i] || !colors[i][1] || colors[i][2] != '=') {
			free(colors[i]);
			continue;
		}

		size_t j;
		char *color_code = colors[i] + 3;
		for (j = 0; j < mappings_n; j++) {
			if (*colors[i] == *mappings[j].prefix
			&& colors[i][1] == mappings[j].prefix[1]) {
				set_color(color_code, mappings[j].color, mappings[j].printable);
				break;
			}
		}

		free(colors[i]);
	}

	free(colors);
}

static void
set_iface_colors(char **colors, const size_t num_colors)
{
	if (!colors || num_colors == 0)
		return;

	const int y = RL_PRINTABLE;
	const int n = RL_NO_PRINTABLE;

	struct color_mapping_t mappings[] = {
		{"ac", ac_c, 2, n}, {"dxd", dxd_c, 3, y}, {"dxr", dxr_c, 3, y},
		{"db", db_c, 2, y}, {"dd", dd_c, 2, y}, {"de", de_c, 2, y},
		{"df", df_c, 2, y}, {"dg", dg_c, 2, y}, {"dk", dk_c, 2, y},
		{"dl", dl_c, 2, y}, {"dn", dn_c, 2, y}, {"do", do_c, 2, y},
		{"dp", dp_c, 2, y}, {"dr", dr_c, 2, y}, {"dt", dt_c, 2, y},
		{"du", du_c, 2, y}, {"dw", dw_c, 2, y}, {"dz", dz_c, 2, y},
		{"el", el_c, 2, y}, {"em", em_c, 2, n}, {"fc", fc_c, 2, y},
		{"hb", hb_c, 2, y}, {"hc", hc_c, 2, y}, {"hd", hd_c, 2, y},
		{"he", he_c, 2, y}, {"hn", hn_c, 2, y}, {"hp", hp_c, 2, y},
		{"hq", hq_c, 2, y}, {"hr", hr_c, 2, y}, {"hs", hs_c, 2, y},
		{"hv", hv_c, 2, y}, {"hw", hw_c, 2, y}, {"li", li_c, 2, n},
		{"lc", lc_c, 2, y}, {"mi", mi_c, 2, y}, {"nm", nm_c, 2, n},
		{"ro", ro_c, 2, n}, {"sb", sb_c, 2, y}, {"sc", sc_c, 2, y},
		{"sd", sd_c, 2, y}, {"sh", sh_c, 2, y}, {"si", si_c, 2, n},
		{"sf", sf_c, 2, y}, {"sp", sp_c, 2, y}, {"sx", sx_c, 2, y},
		{"sz", sz_c, 2, y}, {"ti", ti_c, 2, n}, {"ts", ts_c, 2, y},
		{"tt", tt_c, 2, y}, {"tx", tx_c, 2, y}, {"wc", wc_c, 2, y},
		{"wm", wm_c, 2, n}, {"wp", wp_c, 2, y}, {"ws1", ws1_c, 3, n},
		{"ws2", ws2_c, 3, n}, {"ws3", ws3_c, 3, n}, {"ws4", ws4_c, 3, n},
		{"ws5", ws5_c, 3, n}, {"ws6", ws6_c, 3, n}, {"ws7", ws7_c, 3, n},
		{"ws8", ws8_c, 3, n}, {"xs", xs_c, 2, n}, {"xf", xf_c, 2, n},
	};

	const size_t mappings_n = sizeof(mappings) / sizeof(struct color_mapping_t);
	size_t i;

	for (i = 0; i < num_colors; i++) {
		size_t j;
		for (j = 0; j < mappings_n; j++) {
			if (*colors[i] == *mappings[j].prefix
			&& strncmp(colors[i], mappings[j].prefix,
				(size_t)mappings[j].prefix_len) == 0
			&& *(colors[i] + mappings[j].prefix_len) == '=') {
				set_color(colors[i] + mappings[j].prefix_len + 1,
					mappings[j].color, mappings[j].printable);
				break;
			}
		}

		free(colors[i]);
	}

	free(colors);

	/* We need a copy of these colors without \001 and \002 escape codes. */
	if (*li_c) {
		const char *p = remove_ctrl_chars(li_c);
		xstrsncpy(li_cb, p, sizeof(li_cb));
	}
	if (*xs_c) {
		const char *p = remove_ctrl_chars(xs_c);
		xstrsncpy(xs_cb, p, sizeof(xs_cb));
	}
	if (*xf_c) {
		const char *p = remove_ctrl_chars(xf_c);
		xstrsncpy(xf_cb, p, sizeof(xf_cb));
	}

}

static void
set_shades(char *line, const int type)
{
	char *l = remove_quotes(line);
	if (!l || !*l)
		return;

	char *str = strtok(l, ",");
	if (!str || !*str)
		return;

	int t = *str - '0';
	if (t < 0 || t > 3)
		return;

	if (type == DATE_SHADES)
		date_shades.type = (uint8_t)t;
	else
		size_shades.type = (uint8_t)t;

	int c = 0;
	while ((str = strtok(NULL, ",")) && c < NUM_SHADES) {
		if (*str == '#') {
			if (!*(str + 1) || t != SHADE_TYPE_TRUECOLOR)
				goto NEXT;

			int a = 0, r = 0, g = 0, b = 0;

			if (get_rgb(str + 1, &a, &r, &g, &b) == -1)
				goto NEXT;

			if (type == DATE_SHADES) {
				date_shades.shades[c].attr = (uint8_t)a;
				date_shades.shades[c].R = (uint8_t)r;
				date_shades.shades[c].G = (uint8_t)g;
				date_shades.shades[c].B = (uint8_t)b;
			} else {
				size_shades.shades[c].attr = (uint8_t)a;
				size_shades.shades[c].R = (uint8_t)r;
				size_shades.shades[c].G = (uint8_t)g;
				size_shades.shades[c].B = (uint8_t)b;
			}

			goto NEXT;
		}

		if (t == SHADE_TYPE_TRUECOLOR)
			goto NEXT;

		char *p = strchr(str, '-');
		uint8_t color_attr = 0;

		if (p) {
			*p = '\0';
			if (p[1] && IS_DIGIT(p[1]) && !p[2])
				color_attr = (uint8_t)(p[1] - '0');
		}

		const int n = atoi(str);
		if (n < 0 || n > 255)
			goto NEXT;

		if (type == DATE_SHADES) {
			date_shades.shades[c].attr = color_attr;
			date_shades.shades[c].R = (uint8_t)n;
		} else {
			size_shades.shades[c].attr = color_attr;
			size_shades.shades[c].R = (uint8_t)n;
		}

NEXT:
		c++;
	}

	/* Handle old-style 8 color shades (only 3 available) */
	if (type == DATE_SHADES)
		date_shades_old_style = (c - 1 == 3);
	else
		size_shades_old_style = (c - 1 == 3);
}

static void
set_default_date_shades(void)
{
	char tmp[NAME_MAX];
	snprintf(tmp, sizeof(tmp), "%s", term_caps.color >= 256
		? DEF_DATE_SHADES_256 : DEF_DATE_SHADES_8);
	set_shades(tmp, DATE_SHADES);
}

static void
set_default_size_shades(void)
{
	char tmp[NAME_MAX];
	snprintf(tmp, sizeof(tmp), "%s", term_caps.color >= 256
		? DEF_SIZE_SHADES_256 : DEF_SIZE_SHADES_8);
	set_shades(tmp, SIZE_SHADES);
}

/* Check if LINE contains a valid color code, and store it in the
 * ext_colors global array.
 * If LINE contains a color variable, expand it, check it, and store it. */
static int
store_extension_line(const char *line)
{
	if (!line || !*line)
		return FUNC_FAILURE;

	/* If --lscolors, make sure all lines have the form "*.ext" */
	if (xargs.lscolors == LS_COLORS_GNU && (*line != '*' || line[1] != '.'
	|| !line[2] || strchr(line + 2, '.')))
		return FUNC_FAILURE;

	/* Remove the leading "*.", if any, from the extension line. */
	if (*line == '*' && *(line + 1) == '.') {
		line += 2;
		if (!*line)
			return FUNC_FAILURE;
	}

	char *q = strchr(line, '=');
	if (!q || !*(q + 1) || q == line)
		return FUNC_FAILURE;

	*q = '\0';

	char *def = (char *)NULL;
#ifndef CLIFM_SUCKLESS
	if (is_color_code(q + 1) == 0 && (def = check_defs(q + 1)) == NULL)
#else
	if (is_color_code(q + 1) == 0)
#endif /* !CLIFM_SUCKLESS */
		return FUNC_FAILURE;

	char *tmp = (def && *def) ? def : q + 1;
	char *code = IS_COLOR_PREFIX(*tmp) ? decode_color_prefix(tmp) : tmp;

	if (!code || !*code)
		return FUNC_FAILURE;

	ext_colors = xnrealloc(ext_colors, ext_colors_n + 2, sizeof(struct ext_t));

	const size_t len = (size_t)(q - line);
	ext_colors[ext_colors_n].len = len;
	ext_colors[ext_colors_n].name = savestring(line, len);
	const size_t elen = strlen(code) + 3;
	ext_colors[ext_colors_n].value = xnmalloc(elen, sizeof(char));
	snprintf(ext_colors[ext_colors_n].value, elen, "0;%s", code);
	ext_colors[ext_colors_n].value_len = elen - 1;
	ext_colors[ext_colors_n].hash = hashme(line, 0);

	if (xargs.no_bold == 1)
		remove_bold_attr(ext_colors[ext_colors_n].value);

	*q = '=';
	ext_colors_n++;

	return FUNC_SUCCESS;
}

static void
free_extension_colors(void)
{
	int i = (int)ext_colors_n;
	while (--i >= 0) {
		free(ext_colors[i].name);
		free(ext_colors[i].value);
	}
	free(ext_colors);
	ext_colors = (struct ext_t *)NULL;
	ext_colors_n = 0;
}

static void
split_extension_colors(char *extcolors)
{
	char *p = extcolors;
	char buf[MAX_COLOR + 3 + NAME_MAX]; /* "*.NAME=COLOR" */
	*buf = '\0';
	size_t len = 0;
	int eol = 0;

	free_extension_colors();

	while (eol == 0) {
		switch (*p) {

		case '\0': /* fallthrough */
		case '\n': /* fallthrough */
		case ':':
			if (!*buf) {
				if (!*p || !*(p + 1))
					eol = 1;
				else
					p++;
				break;
			}

			buf[len] = '\0';
			if (store_extension_line(buf) == FUNC_SUCCESS)
				*buf = '\0';

			if (!*p)
				eol = 1;
			len = 0;
			p++;

			break;

		default:
			if (len >= sizeof(buf)) {
				p++;
				break;
			}

			buf[len] = *p;
			len++;
			p++;
			break;
		}
	}

	p = (char *)NULL;

	if (ext_colors) {
		ext_colors[ext_colors_n].name = (char *)NULL;
		ext_colors[ext_colors_n].value = (char *)NULL;
		ext_colors[ext_colors_n].len = 0;
		ext_colors[ext_colors_n].value_len = 0;

		ext_colors[ext_colors_n].hash = 0;
		check_ext_color_hash_conflicts(0);
	}
}

#define CVAR(s) (term_caps.color >= 256 ? DEF_ ## s ## _C256 : DEF_ ## s ## _C) /* NOLINT */

/* We're running with --lscolors. Let's disable clifm's specific file type
 * colors by just using the closest ones provided by LS_COLORS. */
static void
set_extra_colors(void)
{
	if (*di_c) {
		xstrsncpy(ed_c, di_c, sizeof(ed_c)); /* Empty dir */
		xstrsncpy(nd_c, di_c, sizeof(nd_c)); /* No perm dir */
	} else {
		xstrsncpy(ed_c, CVAR(DI), sizeof(ed_c)); /* NOLINT */
		xstrsncpy(nd_c, CVAR(DI), sizeof(nd_c)); /* NOLINT */
	}

	if (*ex_c)
		xstrsncpy(ee_c, ex_c, sizeof(ee_c)); /* Empty executable */
	else
		xstrsncpy(ee_c, CVAR(EX), sizeof(ee_c)); /* NOLINT */

	if (*fi_c) {
		xstrsncpy(ef_c, fi_c, sizeof(ef_c)); /* Empty file */
		xstrsncpy(nf_c, fi_c, sizeof(nf_c)); /* No perm file */
	} else {
		xstrsncpy(ef_c, CVAR(FI), sizeof(ef_c)); /* NOLINT */
		xstrsncpy(nf_c, CVAR(FI), sizeof(nf_c)); /* NOLINT */
	}
}

static int
hash_sort(const void *a, const void *b)
{
	struct ext_t *pa = (struct ext_t *)a;
	struct ext_t *pb = (struct ext_t *)b;

	if (pa->hash > pb->hash)
		return 1;
	if (pa->hash < pb->hash)
		return -1;
	return 0;
}

void
set_default_colors(void)
{
	if (size_shades.type == SHADE_TYPE_UNSET)
		set_default_size_shades();

	if (date_shades.type == SHADE_TYPE_UNSET)
		set_default_date_shades();

	if (xargs.lscolors > 0)
		set_extra_colors();

	if (!ext_colors)
		split_extension_colors(term_caps.color >= 256
			? DEF_EXT_COLORS_256 : DEF_EXT_COLORS);

	if (ext_colors && ext_colors_n > 0) {
		qsort(ext_colors, ext_colors_n, sizeof(*ext_colors),
			(QSFUNC *)hash_sort);
	}

	/* If a definition for TEMP exists in the color scheme file, BK_C should
	 * have been set to this color in store_defintions(). If not, let's try
	 * with EF_C (empty file color). Otherwise, fallback to the default
	 * color for empty files (DEF_EF_C). */
	if (!*bk_c)
		xstrsncpy(bk_c, *ef_c ? ef_c : CVAR(EF), sizeof(bk_c));

	/* Highlight */
	if (!*hb_c) xstrsncpy(hb_c, CVAR(HB), sizeof(hb_c));
	if (!*hc_c) xstrsncpy(hc_c, CVAR(HC), sizeof(hc_c));
	if (!*hd_c) xstrsncpy(hd_c, CVAR(HD), sizeof(hd_c));
	if (!*he_c) xstrsncpy(he_c, CVAR(HE), sizeof(he_c));
	if (!*hn_c) xstrsncpy(hn_c, CVAR(HN), sizeof(hn_c));
	if (!*hp_c) xstrsncpy(hp_c, CVAR(HP), sizeof(hp_c));
	if (!*hq_c) xstrsncpy(hq_c, CVAR(HQ), sizeof(hq_c));
	if (!*hr_c) xstrsncpy(hr_c, CVAR(HR), sizeof(hr_c));
	if (!*hs_c) xstrsncpy(hs_c, CVAR(HS), sizeof(hs_c));
	if (!*hv_c) xstrsncpy(hv_c, CVAR(HV), sizeof(hv_c));
	if (!*hw_c) xstrsncpy(hw_c, CVAR(HW), sizeof(hw_c));

	/* Suggestions */
	if (!*sb_c) xstrsncpy(sb_c, CVAR(SB), sizeof(sb_c));
	if (!*sc_c) xstrsncpy(sc_c, CVAR(SC), sizeof(sc_c));
	if (!*sd_c) xstrsncpy(sd_c, CVAR(SD), sizeof(sd_c));
	if (!*sh_c) xstrsncpy(sh_c, CVAR(SH), sizeof(sh_c));
	if (!*sf_c) xstrsncpy(sf_c, CVAR(SF), sizeof(sf_c));
	if (!*sx_c) xstrsncpy(sx_c, CVAR(SX), sizeof(sx_c));
	if (!*sp_c) xstrsncpy(sp_c, CVAR(SP), sizeof(sp_c));
	if (!*sz_c) xstrsncpy(sz_c, CVAR(SZ), sizeof(sz_c));

	/* Interface */
	if (!*df_c) xstrsncpy(df_c, CVAR(DF), sizeof(df_c));

	/* If unset from the config file, use current workspace color */
	if (!*dl_c)
#ifndef CLIFM_SUCKLESS
		if (config_ok == 0) xstrsncpy(dl_c, CVAR(DL), sizeof(dl_c));
#else
		xstrsncpy(dl_c, CVAR(DL), sizeof(dl_c));
#endif /* !CLIFM_SUCKLESS */

	if (!*el_c) xstrsncpy(el_c, CVAR(EL), sizeof(el_c));
	if (!*em_c) xstrsncpy(em_c, CVAR(EM), sizeof(em_c));
	if (!*fc_c) xstrsncpy(fc_c, CVAR(FC), sizeof(fc_c));
	if (!*lc_c) xstrsncpy(lc_c, CVAR(LC), sizeof(lc_c));
	if (!*li_c) xstrsncpy(li_c, CVAR(LI), sizeof(li_c));
	if (!*li_cb) xstrsncpy(li_cb, term_caps.color >= 256
		? DEF_LI_CB256: DEF_LI_CB, sizeof(li_cb)); /* NOLINT */
	if (!*mi_c) xstrsncpy(mi_c, CVAR(MI), sizeof(mi_c));
	if (!*nm_c) xstrsncpy(nm_c, CVAR(NM), sizeof(nm_c));
	if (!*ti_c) xstrsncpy(ti_c, CVAR(TI), sizeof(ti_c));
	if (!*tx_c) xstrsncpy(tx_c, CVAR(TX), sizeof(tx_c));
	if (!*wm_c) xstrsncpy(wm_c, CVAR(WM), sizeof(wm_c));
	if (!*ro_c) xstrsncpy(ro_c, CVAR(RO), sizeof(ro_c));
	if (!*si_c) xstrsncpy(si_c, CVAR(SI), sizeof(si_c));
	if (!*ts_c) xstrsncpy(ts_c, CVAR(TS), sizeof(ts_c));
	if (!*tt_c) xstrsncpy(tt_c, CVAR(TT), sizeof(tt_c));
	if (!*wc_c) xstrsncpy(wc_c, CVAR(WC), sizeof(wc_c));
	if (!*wp_c) xstrsncpy(wp_c, CVAR(WP), sizeof(wp_c));
	if (!*ws1_c) xstrsncpy(ws1_c, CVAR(WS1), sizeof(ws1_c));
	if (!*ws2_c) xstrsncpy(ws2_c, CVAR(WS2), sizeof(ws2_c));
	if (!*ws3_c) xstrsncpy(ws3_c, CVAR(WS3), sizeof(ws3_c));
	if (!*ws4_c) xstrsncpy(ws4_c, CVAR(WS4), sizeof(ws4_c));
	if (!*ws5_c) xstrsncpy(ws5_c, CVAR(WS5), sizeof(ws5_c));
	if (!*ws6_c) xstrsncpy(ws6_c, CVAR(WS6), sizeof(ws6_c));
	if (!*ws7_c) xstrsncpy(ws7_c, CVAR(WS7), sizeof(ws7_c));
	if (!*ws8_c) xstrsncpy(ws8_c, CVAR(WS8), sizeof(ws8_c));
	if (!*xs_c) xstrsncpy(xs_c, CVAR(XS), sizeof(xs_c));
	if (!*xs_cb) xstrsncpy(xs_cb, term_caps.color >= 256
		? DEF_XS_CB256 : DEF_XS_CB, sizeof(xs_cb)); /* NOLINT */
	if (!*xf_c) xstrsncpy(xf_c, CVAR(XF), sizeof(xf_c));
	if (!*xf_cb) xstrsncpy(xf_cb, term_caps.color >= 256
		? DEF_XF_CB256 : DEF_XF_CB, sizeof(xf_cb)); /* NOLINT */

	/* File types */
	if (!*bd_c) xstrsncpy(bd_c, CVAR(BD), sizeof(bd_c));
	if (!*ca_c) xstrsncpy(ca_c, CVAR(CA), sizeof(ca_c));
	if (!*cd_c) xstrsncpy(cd_c, CVAR(CD), sizeof(cd_c));
	if (!*di_c) xstrsncpy(di_c, CVAR(DI), sizeof(di_c));
	if (!*ed_c) xstrsncpy(ed_c, CVAR(ED), sizeof(ed_c));
	if (!*ee_c) xstrsncpy(ee_c, CVAR(EE), sizeof(ee_c));
	if (!*ex_c) xstrsncpy(ex_c, CVAR(EX), sizeof(ex_c));
	if (!*fi_c) xstrsncpy(fi_c, CVAR(FI), sizeof(fi_c));
	if (!*ef_c) xstrsncpy(ef_c, CVAR(EF), sizeof(ef_c));
	if (!*ln_c) xstrsncpy(ln_c, CVAR(LN), sizeof(ln_c));
	if (!*mh_c) xstrsncpy(mh_c, CVAR(MH), sizeof(mh_c));
	/* Both 'nd' and 'nf' codes can be unset */
	if (!*no_c) xstrsncpy(no_c, CVAR(NO), sizeof(no_c));
#ifdef SOLARIS_DOORS
	if (!*oo_c) xstrsncpy(oo_c, CVAR(OO), sizeof(oo_c));
#endif /* SOLARIS_DOORS */
	if (!*or_c) xstrsncpy(or_c, CVAR(OR), sizeof(or_c));
	if (!*ow_c) xstrsncpy(ow_c, CVAR(OW), sizeof(ow_c));
	if (!*pi_c) xstrsncpy(pi_c, CVAR(PI), sizeof(pi_c));
	if (!*sg_c) xstrsncpy(sg_c, CVAR(SG), sizeof(sg_c));
	if (!*so_c) xstrsncpy(so_c, CVAR(SO), sizeof(so_c));
	if (!*st_c) xstrsncpy(st_c, CVAR(ST), sizeof(st_c));
	if (!*su_c) xstrsncpy(su_c, CVAR(SU), sizeof(su_c));
	if (!*tw_c) xstrsncpy(tw_c, CVAR(TW), sizeof(tw_c));
	if (!*uf_c) xstrsncpy(uf_c, CVAR(UF), sizeof(uf_c));

	/* Interface */
/*	if (!*dd_c) // Date color unset: let's use shades */
	if (!*ac_c) xstrsncpy(ac_c, CVAR(AC), sizeof(ac_c));
	if (!*db_c) xstrsncpy(db_c, CVAR(DB), sizeof(db_c));
	if (!*de_c) xstrsncpy(de_c, CVAR(DE), sizeof(de_c));
	if (!*dg_c) xstrsncpy(dg_c, *du_c ? CVAR(DG) : CVAR(DU), sizeof(dg_c));
	if (!*dk_c) xstrsncpy(dk_c, CVAR(DK), sizeof(dk_c));
	if (!*dn_c) xstrsncpy(dn_c, CVAR(DN), sizeof(dn_c));
	if (!*do_c) xstrsncpy(do_c, CVAR(DO), sizeof(do_c));
	if (!*dp_c) xstrsncpy(dp_c, CVAR(DP), sizeof(dp_c));
	if (!*dr_c) xstrsncpy(dr_c, CVAR(DR), sizeof(dr_c));
	if (!*dt_c) /* Unset: dim the current color */
		xstrsncpy(dt_c, dim_c, sizeof(dt_c));
	if (!*du_c) {
		/* Before the introduction of the du color code, user IDs were
		 * printed using the dg color code, and group IDs using the same
		 * color but dimmed. If du isn't set, let's keep this old behavior. */
		xstrsncpy(du_c, dg_c, sizeof(du_c));
		xstrsncpy(dg_c, dim_c, sizeof(dg_c));
	}
	if (!*dw_c) xstrsncpy(dw_c, CVAR(DW), sizeof(dw_c));
	if (!*dxd_c) xstrsncpy(dxd_c, CVAR(DXD), sizeof(dxd_c));
	if (!*dxr_c) xstrsncpy(dxr_c, CVAR(DXR), sizeof(dxr_c));
/*	if (!*dz_c) // Size color unset: let's use shades */

#ifndef _NO_ICONS
	if (!*dir_ico_c)
		xstrsncpy(dir_ico_c, CVAR(DIR_ICO), sizeof(dir_ico_c));
#endif /* !_NO_ICONS */
}
#undef CVAR

/* Set a pointer to the current color scheme */
static int
get_cur_colorscheme(const char *colorscheme)
{
	char *def_cscheme = (char *)NULL;
	int i = (int)cschemes_n;

	while (--i >= 0) {
		if (*colorscheme == *color_schemes[i]
		&& strcmp(colorscheme, color_schemes[i]) == 0) {
			cur_cscheme = color_schemes[i];
			break;
		}

		if (strcmp(color_schemes[i], term_caps.color < 256 ? DEF_COLOR_SCHEME
		: DEF_COLOR_SCHEME_256) == 0)
			def_cscheme = color_schemes[i];
	}

	if (!cur_cscheme) {
		err('w', PRINT_PROMPT, _("%s: colors: %s: No such color scheme. "
			"Falling back to default\n"), PROGRAM_NAME, colorscheme);

		if (def_cscheme)
			cur_cscheme = def_cscheme;
		else
			return FUNC_FAILURE;
	}

	return FUNC_SUCCESS;
}

static char *
bsd_to_ansi_color(char color, const int bg)
{
	char c = color;
	int up = 0;

	if (IS_ALPHA_UP(color)) {
		up = 1;
		c = (char)TOLOWER(color);
	}

	/* An uppercase letter for background sets the underline attribute.
	 * We follow here the FreeBSD implementation of ls(1). */

	switch (c) {
	case 'a': return (bg ? (up ? "4;40" : "40") : (up ? "1;30" : "30"));
	case 'b': return (bg ? (up ? "4;41" : "41") : (up ? "1;31" : "31"));
	case 'c': return (bg ? (up ? "4;42" : "42") : (up ? "1;32" : "32"));
	case 'd': return (bg ? (up ? "4;43" : "43") : (up ? "1;33" : "33"));
	case 'e': return (bg ? (up ? "4;44" : "44") : (up ? "1;34" : "34"));
	case 'f': return (bg ? (up ? "4;45" : "45") : (up ? "1;35" : "35"));
	case 'g': return (bg ? (up ? "4;46" : "46") : (up ? "1;36" : "36"));
	case 'h': return (bg ? (up ? "4;47" : "47") : (up ? "1;37" : "37"));
	case 'x': return (bg ? (up ? "4;49" : "49") : "39");
	default:  return (bg ? "49" : "39"); /* Never reached */
	}
}

static char *
set_filetype(const int c)
{
	switch (c) {
	case 0:  return "di";
	case 1:  return "ln";
	case 2:  return "so";
	case 3:  return "pi";
	case 4:  return "ex";
	case 5:  return "bd";
	case 6:  return "cd";
	case 7:  return "su";
	case 8:  return "sg";
	case 9:  return "tw";
	case 10: return "ow";
	default: return "fi"; /* Never reached */
	}
}

/* If the LSCOLORS environment variable is set, convert its value to a valid
 * GNU LS_COLORS format.
 * Returns a pointer to the transformed string, or NULL in case of error.
 * For information about the format used by LSCOLORS consult
 * 'https://www.unix.com/man-page/FreeBSD/1/ls'. */
static char *
set_lscolors_bsd(void)
{
	char *env = getenv("LSCOLORS");
	if (!env)
		return (char *)NULL;

	/* 144 bytes are required to hold the largest possible value for LSCOLORS:
	 * 11 file types, 13 chars max each, plus the terminating NUL char.
	 * However, more often than not these kind hard limits fail: let's use
	 * something a bit bigger. */
	static char buf[256];

	size_t c = 0;
	int f = 0;
	int len = 0;

#define IS_BSD_COLOR(c) (((c) >= 'a' && (c) <= 'h') \
	|| ((c) >= 'A' && (c) <= 'H' ) || (c) == 'x' || (c) == 'X')

	while (env[c] && f < 11) {
		if (!IS_BSD_COLOR(env[c])) {
			c++;
			continue;
		}

		if (!env[c + 1])
			break;

		if (!IS_BSD_COLOR(env[c + 1])) {
			c += 2;
			continue;
		}

		/* At this point, we have a valid "fg" pair. */

		const char *ft = set_filetype(f);
		f++;

		const int n = snprintf(buf + len, sizeof(buf) - (size_t)len,
			"%s=%s;%s:", ft, bsd_to_ansi_color(env[c], 0),
			bsd_to_ansi_color(env[c + 1], 1));

		if (n < 0 || n >= (int)sizeof(buf) - len)
			break;

		len += n;
		c += 2;
	}

#undef IS_BSD_COLOR

	buf[len] = '\0';
	return *buf ? buf : (char *)NULL;
}

/* Inspect LS_COLORS/LSCOLORS variable and assign pointers to ENV_FILECOLORS
 * and ENV_EXTCOLORS accordingly. */
static void
set_lscolors(char **env_filecolors, char **env_extcolors)
{
	static char *ls_colors = (char *)NULL;
	ls_colors = getenv("LS_COLORS");
	if (!ls_colors || !*ls_colors) {
		ls_colors = set_lscolors_bsd();
		if (!ls_colors || !*ls_colors)
			return;
		xargs.lscolors = LS_COLORS_BSD;
	} else {
		xargs.lscolors = LS_COLORS_GNU;
	}

	char *ext_ptr = strchr(ls_colors, '*');
	if (ext_ptr) {
		*env_extcolors = ext_ptr;
		if (ext_ptr > ls_colors && *(ext_ptr - 1))
			*(ext_ptr - 1) = '\0';
	}

	*env_filecolors = ls_colors;
}

/* Try to retrieve colors from the environment. */
static void
get_colors_from_env(char **file, char **ext, char **iface)
{
	char *env_filecolors = (char *)NULL;
	char *env_extcolors = (char *)NULL;

	if (xargs.lscolors > 0) {
		set_lscolors(&env_filecolors, &env_extcolors);
	} else {
		env_filecolors = getenv("CLIFM_FILE_COLORS");
		env_extcolors = getenv("CLIFM_EXT_COLORS");
	}

	char *env_ifacecolors = getenv("CLIFM_IFACE_COLORS");
	char *env_date_shades = getenv("CLIFM_DATE_SHADES");
	char *env_size_shades = getenv("CLIFM_SIZE_SHADES");

	if (env_date_shades && *env_date_shades)
		set_shades(env_date_shades, DATE_SHADES);

	if (env_size_shades && *env_size_shades)
		set_shades(env_size_shades, SIZE_SHADES);

	if (env_filecolors && *env_filecolors)
		*file = strdup(env_filecolors);

	if (env_extcolors && *env_extcolors)
		*ext = strdup(env_extcolors);

	if (env_ifacecolors && *env_ifacecolors)
		*iface = strdup(env_ifacecolors);
}

#ifndef CLIFM_SUCKLESS
/* Store the color variable STR (in the form VAR=VALUE) in the global
 * defs struct. */
static void
store_definition(char *str)
{
	if (!str || !*str || *str == '\n' || defs_n > MAX_DEFS)
		return;

	char *name = str;
	char *value = strchr(name, '=');
	if (!value || !value[1] || value == name)
		return;

	*value = '\0';
	value++;

	defs[defs_n].namelen = (size_t)(value - name - 1);
	defs[defs_n].name = savestring(name, defs[defs_n].namelen);

	size_t val_len;
	char *s = strchr(value, ' ');
	if (s) {
		*s = '\0';
		val_len = (size_t)(s - value);
	} else {
		val_len = strlen(value);
	}

	if (IS_ALPHA_LOW(*value) || IS_ALPHA_UP(*value)) {
		char *ret = check_names(value);
		if (ret) {
			value = ret;
			val_len = strlen(ret);
		}
	}

	defs[defs_n].value = savestring(value, val_len);

	/* If we find a definition for TEMP, let's use this color for backup files
	 * (bk_c color code). */
	if (!*bk_c && *name == 'T' && strcmp(name + 1, "EMP") == 0) {
		char *v = IS_COLOR_PREFIX(*value) ? decode_color_prefix(value) : value;
		if (v && *v)
			snprintf(bk_c, sizeof(bk_c), "\x1b[0;%sm", v);
	}

	defs_n++;
}

/* Initialize the colors_t struct */
static void
init_defs(void)
{
	int n = MAX_DEFS;
	while (--n >= 0) {
		defs[n].name = (char *)NULL;
		defs[n].value = (char *)NULL;
	}
}

static void
set_cs_prompt(char *line)
{
	if (IS_CTRL_CHR(*line))
		return;

	char *p = remove_quotes(line);
	if (!p || !*p)
		return;

	if (expand_prompt_name(p) != FUNC_SUCCESS) {
		free(conf.encoded_prompt);
		conf.encoded_prompt = savestring(p, strlen(p));
	}
}

static void
set_cs_prompt_noti(const char *line)
{
	if (IS_CTRL_CHR(*line))
		return;

	if (*line == 't' && strncmp(line, "true", 4) == 0)
		prompt_notif = 1; /* NOLINT */
	else if (*line == 'f' && strncmp(line, "false", 5) == 0)
		prompt_notif = 0; /* NOLINT */
	else
		prompt_notif = DEF_PROMPT_NOTIF; /* NOLINT */
}

static void
set_cs_enable_warning_prompt(const char *line)
{
	if (IS_CTRL_CHR(*line))
		return;

	if (*line == 't' && strncmp(line, "true", 4) == 0)
		conf.warning_prompt = 1; /* NOLINT */
	else if (*line == 'f' && strncmp(line, "false", 5) == 0)
		conf.warning_prompt = 0; /* NOLINT */
	else
		conf.warning_prompt = DEF_WARNING_PROMPT; /* NOLINT */
}

static void
set_cs_warning_prompt_str(char *line)
{
	if (IS_CTRL_CHR(*line))
		return;

	char *p = remove_quotes(line);
	if (!p)
		return;

	free(conf.wprompt_str);
	conf.wprompt_str = savestring(p, strlen(p));
}

static void
set_cs_right_prompt_str(char *line)
{
	if (IS_CTRL_CHR(*line))
		return;

	char *p = remove_quotes(line);
	if (!p)
		return;

	free(conf.rprompt_str);
	conf.rprompt_str = savestring(p, strlen(p));
	if (conf.encoded_prompt)
		conf.prompt_is_multiline = strstr(conf.encoded_prompt, "\\n") ? 1 : 0;
}

#ifndef _NO_FZF
static void
set_fzf_opts(char *line)
{
	free(conf.fzftab_options);
	conf.fzftab_options = (char *)NULL;

	if (!line) {
		char *p = conf.colorize == 1 ? DEF_FZFTAB_OPTIONS
			: DEF_FZFTAB_OPTIONS_NO_COLOR;
		conf.fzftab_options = savestring(p, strlen(p));
	}

	else if (*line == 'n' && strcmp(line, "none") == 0) {
		conf.fzftab_options = xnmalloc(1, sizeof(char));
		*conf.fzftab_options = '\0';
	}

	else if (sanitize_cmd(line, SNT_BLACKLIST) == FUNC_SUCCESS) {
		conf.fzftab_options = savestring(line, strlen(line));
	}

	else {
		err('w', PRINT_PROMPT, _("%s: FzfTabOptions contains unsafe "
			"characters (<>|;&$`). Falling back to default values.\n"),
			PROGRAM_NAME);
		if (conf.colorize == 1) {
			conf.fzftab_options =
				savestring(DEF_FZFTAB_OPTIONS, strlen(DEF_FZFTAB_OPTIONS));
		} else {
			conf.fzftab_options =
				savestring(DEF_FZFTAB_OPTIONS_NO_COLOR,
				strlen(DEF_FZFTAB_OPTIONS_NO_COLOR));
		}
	}

	if (!conf.fzftab_options)
		return;

	if (strstr(conf.fzftab_options, "--preview "))
		conf.fzf_preview = FZF_EXTERNAL_PREVIEWER;

	char *b = strstr(conf.fzftab_options, "--height");
	if (b)
		fzf_height_value = get_fzf_height(b + (sizeof("--height") - 1));

	b = strstr(conf.fzftab_options, "--border");
	if (b)
		fzf_border_type = get_fzf_border_type(b + (sizeof("--border") - 1));
}

static void
set_cs_fzftabopts(char *line)
{
	if (IS_CTRL_CHR(*line))
		return;

	char *p = remove_quotes(line);
	if (!p || !*p)
		return;

	set_fzf_opts(p);
}
#endif /* !_NO_FZF */

static void
set_cs_colors(char *line, char **colors, const size_t line_len)
{
	if (IS_CTRL_CHR(*line))
		return;

	char *color_line = strip_color_line(line, line_len);
	if (!color_line)
		return;

	*colors = savestring(color_line, strlen(color_line));
	free(color_line);
}

static void
set_cs_extcolors(char *line, char **extcolors, const ssize_t line_len)
{
	char *p = line + 10; /* 10 == "ExtColors=" */
	if (IS_CTRL_CHR(*p) || ((*p == '\'' || *p == '"') && !*(++p)))
		return;

	ssize_t l = line_len - (p - line);

	if (l > 0 && (p[l - 1] == '\'' || p[l - 1] == '"')) {
		p[l - 1] = '\0';
		l--;
	}

	if (!*extcolors) { /* First ExtColors line. */
		*extcolors = savestring(p, (size_t)l);
	} else {
		/* Second ExtColors line or more: append it to the first one. */
		size_t curlen = strlen(*extcolors);
		if (curlen > 0 && (*extcolors)[curlen - 1] == ':') {
			(*extcolors)[curlen - 1] = '\0';
			curlen--;
		}

		if (*p == ':' && *(p + 1) && l > 0) {
			p++;
			l--;
		}

		const size_t total_len = curlen + (size_t)l + 1;
		*extcolors = xnrealloc(*extcolors, total_len + 1, sizeof(char *));

		*(*extcolors + curlen) = ':';
		memcpy(*extcolors + curlen + 1, p, (size_t)l);
		(*extcolors)[total_len] = '\0';
	}
}

#ifndef _NO_ICONS
static void
set_cs_dir_icon_color(char *line, const ssize_t line_len)
{
	char *p = line + 13; /* 13 == "DirIconColor=" */
	if (IS_CTRL_CHR(*p) || ((*p == '\'' || *p == '"') && !*(++p)))
		return;

	if (line[line_len - 1] == '\'' || line[line_len - 1] == '"')
		line[line_len - 1] = '\0';

	char *c = (char *)NULL;
	if (is_color_code(p) == 0 && (c = check_defs(p)) == NULL)
		return;

	snprintf(dir_ico_c, sizeof(dir_ico_c), "\x1b[%sm", c ? c : p);
}
#endif /* !_NO_ICONS */

/* Get color lines from the configuration file */
static int
read_color_scheme_file(const char *colorscheme, char **filecolors,
			char **extcolors, char **ifacecolors, const int env)
{
	/* Allocate some memory for custom color variables */
	defs = xnmalloc(MAX_DEFS + 1, sizeof(struct colors_t));
	defs_n = 0;
	init_defs();

	char colorscheme_file[PATH_MAX + 1];
	*colorscheme_file = '\0';
	if (config_ok == 1 && colors_dir) {
		snprintf(colorscheme_file, sizeof(colorscheme_file), "%s/%s.clifm",
			colors_dir, colorscheme ? colorscheme : "default");
	}

	/* If not in local dir, check system data dir as well */
	struct stat attr;
	if (data_dir && (!*colorscheme_file
	|| stat(colorscheme_file, &attr) == -1)) {
		snprintf(colorscheme_file, sizeof(colorscheme_file),
			"%s/%s/colors/%s.clifm", data_dir, PROGRAM_NAME, colorscheme
			? colorscheme : "default");
	}

	FILE *fp_colors = fopen(colorscheme_file, "r");
	if (!fp_colors) {
		if (!env) {
			xerror("%s: colors: '%s': %s\n", PROGRAM_NAME,
				colorscheme_file, strerror(errno));
			return FUNC_FAILURE;
		} else {
			err('w', PRINT_PROMPT, _("%s: colors: '%s': No such color scheme. "
				"Falling back to default\n"), PROGRAM_NAME, colorscheme);
			return FUNC_SUCCESS;
		}
	}

	/* If called from the color scheme function, reset all color values
	 * before proceeding. */
	if (!env) {
		reset_filetype_colors();
		reset_iface_colors();
	}

	char *line = (char *)NULL;
	size_t line_size = 0;
	ssize_t line_len = 0;

	while ((line_len = getline(&line, &line_size, fp_colors)) > 0) {
		if (SKIP_LINE(*line))
			continue;

		if (line[line_len - 1] == '\n') {
			line[line_len - 1] = '\0';
			line_len--;
		}

		if (*line == 'd' && strncmp(line, "define ", 7) == 0) {
			store_definition(line + 7);
		}

		else if (*line == 'P' && strncmp(line, "Prompt=", 7) == 0) {
			set_cs_prompt(line + 7);
		}

		/* The following values override those set via the Prompt line
		 * (provided it was set to a valid prompt name, as defined in the
		 * prompts file). */
		else if (*line == 'N' && strncmp(line, "Notifications=", 14) == 0) {
			set_cs_prompt_noti(line + 14);
		}

		else if (xargs.warning_prompt == UNSET && *line == 'E'
		&& strncmp(line, "EnableWarningPrompt=", 20) == 0) {
			set_cs_enable_warning_prompt(line + 20);
		}

		else if (*line == 'W' && strncmp(line, "WarningPrompt=", 14) == 0) {
			set_cs_warning_prompt_str(line + 14);
		}

		else if (*line == 'R' && strncmp(line, "RightPrompt=", 12) == 0) {
			set_cs_right_prompt_str(line + 12);
		}

#ifndef _NO_FZF
		else if (*line == 'F' && strncmp(line, "FzfTabOptions=", 14) == 0) {
			set_cs_fzftabopts(line + 14);
		}
#endif /* !_NO_FZF */

		else if (*line == 'D' && strncmp(line, "DividingLine=", 13) == 0) {
			set_div_line(line + 13);
		}

		/* Interface colors */
		else if (!*ifacecolors && *line == 'I'
		&& strncmp(line, "InterfaceColors=", 16) == 0) {
			set_cs_colors(line + 16, ifacecolors, (size_t)line_len - 16);
		}

		/* Filetype colors */
		else if (!*filecolors && *line == 'F'
		&& strncmp(line, "FiletypeColors=", 15) == 0) {
			set_cs_colors(line + 15, filecolors, (size_t)line_len - 15);
		}

		/* File extension colors */
		else if (xargs.lscolors != LS_COLORS_GNU && *line == 'E'
		&& strncmp(line, "ExtColors=", 10) == 0) {
			set_cs_extcolors(line, extcolors, line_len);
		}

#ifndef _NO_ICONS
		/* Directory icon color */
		else if (*line == 'D' && strncmp(line, "DirIconColor=", 13) == 0) {
			set_cs_dir_icon_color(line, line_len);
		}
#endif /* !_NO_ICONS */

		else if (date_shades.type == SHADE_TYPE_UNSET
		&& *line == 'D' && strncmp(line, "DateShades=", 11) == 0) {
			set_shades(line + 11, DATE_SHADES);
		}

		else if (size_shades.type == SHADE_TYPE_UNSET
		&& *line == 'S' && strncmp(line, "SizeShades=", 11) == 0) {
			set_shades(line + 11, SIZE_SHADES);
		}
	}

	free(line);
	fclose(fp_colors);

	return FUNC_SUCCESS;
}
#endif /* !CLIFM_SUCKLESS */

/* Split the colors line LINE and set the corresponding colors
 * according to TYPE (either interface or file type color). */
static void
split_color_line(char *line, const int type)
{
	char *p = line;
	char **colors = (char **)NULL;
	/* MAX_COLOR is not enough when parsing LS_COLORS lines, where we
	 * usually find extension colors ("*.ext=color"). */
	char buf[MAX_COLOR + 3 + NAME_MAX]; *buf = '\0';
	size_t len = 0;
	size_t words = 0;
	int eol = 0;

	while (eol == 0) {
		switch (*p) {

		case '\0': /* fallthrough */
		case '\n': /* fallthrough */
		case ':':
			if (!*buf) {
				if (!*p || !*(p + 1))
					eol = 1;
				else
					p++;
				break;
			}

			buf[len] = '\0';

			colors = xnrealloc(colors, words + 2, sizeof(char *));
			colors[words] = savestring(buf, len);
			words++;
			*buf = '\0';

			if (*p == '\0')
				eol = 1;

			len = 0;
			p++;
			break;

		default:
			if (len >= sizeof(buf)) {
				p++;
				break;
			}

			buf[len] = *p;
			len++;
			p++;
			break;
		}
	}

	p = (char *)NULL;

	if (!colors)
		return;

	colors[words] = (char *)NULL;

	/* Set the color variables.
	 * The colors array is free'd by both of these functions. */
	if (type == SPLIT_FILETYPE_COLORS)
		set_filetype_colors(colors, words);
	else
		set_iface_colors(colors, words);
}

static void
disable_bold(void)
{
	/* File types */
	remove_bold_attr(bd_c); remove_bold_attr(bk_c); remove_bold_attr(ca_c);
	remove_bold_attr(cd_c); remove_bold_attr(di_c); remove_bold_attr(ed_c);
	remove_bold_attr(ee_c); remove_bold_attr(ef_c); remove_bold_attr(ex_c);
	remove_bold_attr(fi_c); remove_bold_attr(ln_c); remove_bold_attr(mh_c);
	remove_bold_attr(nd_c); remove_bold_attr(nf_c); remove_bold_attr(no_c);
#ifdef SOLARIS_DOORS
	remove_bold_attr(oo_c);
#endif /* SOLARIS_DOORS */
	remove_bold_attr(or_c); remove_bold_attr(ow_c); remove_bold_attr(pi_c);
	remove_bold_attr(sg_c); remove_bold_attr(so_c); remove_bold_attr(st_c);
	remove_bold_attr(su_c); remove_bold_attr(tw_c); remove_bold_attr(uf_c);

	/* Interface */
	remove_bold_attr(ac_c); remove_bold_attr(df_c); remove_bold_attr(dl_c);
	remove_bold_attr(el_c); remove_bold_attr(fc_c); remove_bold_attr(lc_c);
	remove_bold_attr(mi_c); remove_bold_attr(ts_c); remove_bold_attr(tt_c);
	remove_bold_attr(wc_c); remove_bold_attr(wp_c);

	/* Suggestions */
	remove_bold_attr(sb_c); remove_bold_attr(sc_c); remove_bold_attr(sd_c);
	remove_bold_attr(sf_c); remove_bold_attr(sh_c); remove_bold_attr(sp_c);
	remove_bold_attr(sx_c); remove_bold_attr(sz_c);

#ifndef _NO_ICONS
	remove_bold_attr(dir_ico_c);
#endif /* !_NO_ICONS */

	/* Syntax highlighting */
	remove_bold_attr(hb_c); remove_bold_attr(hc_c); remove_bold_attr(hd_c);
	remove_bold_attr(he_c); remove_bold_attr(hn_c); remove_bold_attr(hp_c);
	remove_bold_attr(hq_c); remove_bold_attr(hr_c); remove_bold_attr(hs_c);
	remove_bold_attr(hv_c); remove_bold_attr(hw_c);

	/* File properties */
	remove_bold_attr(db_c); remove_bold_attr(dd_c); remove_bold_attr(de_c);
	remove_bold_attr(dg_c); remove_bold_attr(dk_c); remove_bold_attr(dn_c);
	remove_bold_attr(do_c); remove_bold_attr(dp_c); remove_bold_attr(dr_c);
	remove_bold_attr(dt_c); remove_bold_attr(du_c); remove_bold_attr(dw_c);
	remove_bold_attr(dxd_c); remove_bold_attr(dxr_c); remove_bold_attr(dz_c);

	/* Workspaces */
	remove_bold_attr(ws1_c); remove_bold_attr(ws2_c); remove_bold_attr(ws3_c);
	remove_bold_attr(ws4_c); remove_bold_attr(ws5_c); remove_bold_attr(ws6_c);
	remove_bold_attr(ws7_c); remove_bold_attr(ws8_c);

	/* Prompt indicators */
	remove_bold_attr(em_c); remove_bold_attr(li_c); remove_bold_attr(li_cb);
	remove_bold_attr(nm_c); remove_bold_attr(ro_c); remove_bold_attr(si_c);
	remove_bold_attr(ti_c); remove_bold_attr(tx_c); remove_bold_attr(xs_c);
	remove_bold_attr(xs_cb); remove_bold_attr(xf_c); remove_bold_attr(xf_cb);

	remove_bold_attr(wm_c);
}

/* Get color codes values from either the environment or the config file
 * and set colors accordingly. If some value is not found or is a wrong
 * value, the default is set. */
int
set_colors(const char *colorscheme, const int check_env)
{
	char *filecolors = (char *)NULL;
	char *extcolors = (char *)NULL;
	char *ifacecolors = (char *)NULL;

	date_shades.type = SHADE_TYPE_UNSET;
	size_shades.type = SHADE_TYPE_UNSET;

#ifndef _NO_ICONS
	*dir_ico_c = '\0';
#endif /* !_NO_ICONS */

	int ret = FUNC_SUCCESS;
	if (colorscheme && *colorscheme && color_schemes)
		ret = get_cur_colorscheme(colorscheme);

	/* CHECK_ENV is true only when this function is called from
	 * check_colors() (config.c) */
	if (ret == FUNC_SUCCESS && check_env == 1)
		get_colors_from_env(&filecolors, &extcolors, &ifacecolors);

#ifndef CLIFM_SUCKLESS
	if (ret == FUNC_SUCCESS && xargs.stealth_mode != 1
	&& config_ok != 0) {
		if (read_color_scheme_file(cur_cscheme, &filecolors,
		&extcolors, &ifacecolors, check_env) == FUNC_FAILURE) {
			clear_defs();
			return FUNC_FAILURE;
		}
	}
#endif /* CLIFM_SUCKLESS */
	/* Split the color lines into substrings (one per color) */

	if (!extcolors) {
		/* Unload current extension colors */
		if (ext_colors_n > 0)
			free_extension_colors();
	} else {
		split_extension_colors(extcolors);
		free(extcolors);
	}

	if (!ifacecolors) {
		reset_iface_colors();
	} else {
		split_color_line(ifacecolors, SPLIT_INTERFACE_COLORS);
		free(ifacecolors);
	}

	if (!filecolors) {
		reset_filetype_colors();
	} else {
		split_color_line(filecolors, SPLIT_FILETYPE_COLORS);
		free(filecolors);
	}

#ifndef CLIFM_SUCKLESS
	clear_defs();
#endif /* CLIFM_SUCKLESS */

	/* If some color is unset or is a wrong color code, set the default value */
	set_default_colors();
	update_warning_prompt_text_color();

	if (xargs.no_bold == 1)
		disable_bold();

	return FUNC_SUCCESS;
}

/* If completing trashed files (regular only) we need to remove the trash
 * extension in order to correctly determine the file color (according to
 * its actual extension).
 * Remove this extension (setting the initial dot to NULL) and return a
 * pointer to this character, so that we can later reinsert the dot.
 *
 * NOTE: We append a time suffix (via gen_time_suffix()) to the trashed file
 * name in order to make it unique. Now, since other trash implementations do
 * not do this, we need to check the extension name (otherwise, we might end
 * up removing the original file extension).
 * The time suffix is "YYYYMMDDHHMMSS". So we need to check whether we have an
 * extension name of at least 14 digits, being the first one '2' (the time
 * suffix starts by the year, so that it's quite safe to assume the first
 * one will be '2' (at least until the year 3000!)). Not perfect, but it
 * works most of the time. */
char *
remove_trash_ext(char **ent)
{
	if (!(flags & STATE_COMPLETING) || (cur_comp_type != TCMP_UNTRASH
	&& cur_comp_type != TCMP_TRASHDEL))
		return (char *)NULL;

	char *d = strrchr(*ent, '.');
	if (d && d != *ent && d[1] == '2'
	&& strlen(d + 1) == 14 && is_number(d + 1))
		*d = '\0';

	return d;
}

char *
get_entry_color(char *ent, const struct stat *a)
{
	char *color = (char *)NULL;

	switch (a->st_mode & S_IFMT) {
	case S_IFREG: {
		size_t ext = 0;
		char *d = remove_trash_ext(&ent);
		color = get_regfile_color(ent, a, &ext);
		if (d)
			*d = '.';
		}
		break;

	case S_IFDIR:
		color = conf.colorize == 0 ? di_c : get_dir_color(ent, a, -1);
		break;

	case S_IFLNK: {
		if (conf.colorize == 0) {
			color = ln_c;
		} else {
			char *linkname = xrealpath(ent, NULL);
			color = linkname ? ln_c : or_c;
			free(linkname);
		}
		}
		break;

	case S_IFIFO: color = pi_c; break;
	case S_IFBLK: color = bd_c; break;
	case S_IFCHR: color = cd_c; break;
#ifdef SOLARIS_DOORS
	case S_IFPORT: /* fallthrough */
	case S_IFDOOR: color = oo_c; break;
#endif /* SOLARIS_DOORS */
	case S_IFSOCK: color = so_c; break;
	default: color = no_c; break;
	}

	return color;
}

/* Print the entry ENT using color codes and ELN as ELN, right padding PAD
 * chars and terminate ENT with or without a new line char (NEW_LINE
 * 1 or 0 respectively).
 * ELN could be:
 * > 0: The ELN of a file in CWD
 * -1:  Error getting ELN
 * 0:   ELN should not be printed. E.g., when listing files not in CWD */
void
colors_list(char *ent, const int eln, const int pad, const int new_line)
{
	char index[MAX_INT_STR + 1];
	*index = '\0';

	if (eln > 0)
		snprintf(index, sizeof(index), "%d ", eln);
	else if (eln == -1)
		snprintf(index, sizeof(index), "? ");
	else
		index[0] = '\0';

	struct stat attr;
	char *p = ent, *q = ent, t[PATH_MAX + 1];
	char *eln_color = *index == '?' ? mi_c : el_c;

	if (*q == '~') {
		if (!q[1] || (q[1] == '/' && !q[2]))
			xstrsncpy(t, user.home, sizeof(t) - 1);
		else
			snprintf(t, sizeof(t), "%s/%s", user.home, q + 2);
		p = t;
	}

	size_t len = strlen(p);
	int rem_slash = 0;
	/* Remove the ending slash: lstat(3) won't take a symlink to dir as
	 * a symlink (but as a dir), if the filename ends with a slash. */
	if (len > 1 && p[len - 1] == '/') {
		p[len - 1] = '\0';
		rem_slash = 1;
	}

	char vt_file[PATH_MAX + 1]; *vt_file = '\0';
	if (virtual_dir == 1 && is_file_in_cwd(p))
		xreadlink(XAT_FDCWD, p, vt_file, sizeof(vt_file));

	const int ret = lstat(*vt_file ? vt_file : p, &attr);
	if (rem_slash == 1)
		p[len - 1] = '/';

	char *wname = (char *)NULL;
	const size_t wlen = wc_xstrlen(ent);
	if (wlen == 0) /* Invalid chars found. */
		wname = replace_invalid_chars(ent);

	char *color = ret == -1 ? uf_c : get_entry_color(ent, &attr);

	char *name = wname ? wname : ent;
	char *tmp = (flags & IN_SELBOX_SCREEN) ? abbreviate_file_name(name) : name;

	printf("%s%s%s%s%s%s%s%-*s", eln_color, index, df_c, color,
		tmp + tab_offset, df_c, new_line ? "\n" : "", pad, "");
	free(wname);

	if ((flags & IN_SELBOX_SCREEN) && tmp != name)
		free(tmp);
}

#ifndef CLIFM_SUCKLESS
/* Returns 1 if the file named NAME is a valid color scheme name, or 0 otherwise.
 * If true, NAME is truncated to its last dot (the file extension is removed). */
static int
is_valid_colorscheme_name(char *name)
{
	if (SELFORPARENT(name))
		return 0;

	char *ret = strrchr(name, '.');
	if (!ret || ret == name || strcmp(ret, ".clifm") != 0)
		return 0;

	*ret = '\0';

	return 1;
}

/* Returns 1 if the color scheme name NAME already exists in the current
 * list of color schemes, which contains TOTAL entries. */
static int
is_duplicate_colorscheme_name(const char *name, const size_t total)
{
	size_t i;
	for (i = 0; i < total; i++) {
		if (*color_schemes[i] == *name && strcmp(name, color_schemes[i]) == 0)
			return 1;
	}

	return 0;
}

size_t
get_colorschemes(void)
{
	if (color_schemes && cschemes_n > 0)
		return cschemes_n;

	int schemes_total = 0;
	struct dirent *ent;
	DIR *dir_p;
	size_t i = 0;

	if (colors_dir
	&& (schemes_total = (int)count_dir(colors_dir, NO_CPOP) - 2) > 0
	&& (dir_p = opendir(colors_dir)) != NULL) {
		color_schemes = xnrealloc(color_schemes,
			(size_t)schemes_total + 2, sizeof(char *));

		while ((ent = readdir(dir_p)) != NULL && i < (size_t)schemes_total) {
			if (is_valid_colorscheme_name(ent->d_name) == 0)
				continue;

			color_schemes[i] = savestring(ent->d_name, strlen(ent->d_name));
			i++;
		}

		closedir(dir_p);
		color_schemes[i] = (char *)NULL;
	}

	if (!data_dir || !*data_dir)
		goto END;

	if (schemes_total < 0) /* count_dir() failed */
		schemes_total = 0;

	char sys_colors_dir[PATH_MAX + 1];
	snprintf(sys_colors_dir, sizeof(sys_colors_dir), "%s/%s/colors",
		data_dir, PROGRAM_NAME);

	const int n = (int)count_dir(sys_colors_dir, NO_CPOP) - 2;
	if (n <= 0)
		goto END;

	schemes_total += n;

	if (!(dir_p = opendir(sys_colors_dir)))
		goto END;

	color_schemes = xnrealloc(color_schemes,
		(size_t)schemes_total + 2, sizeof(char *));

	const size_t i_tmp = i;

	while ((ent = readdir(dir_p)) != NULL && i < (size_t)schemes_total) {
		if (is_valid_colorscheme_name(ent->d_name) == 0
		|| is_duplicate_colorscheme_name(ent->d_name, i_tmp) == 1)
			continue;

		color_schemes[i] = savestring(ent->d_name, strlen(ent->d_name));
		i++;
	}

	closedir(dir_p);
	color_schemes[i] = (char *)NULL;

END:
	if (color_schemes)
		qsort(color_schemes, i, sizeof(char *), (QSFUNC *)compare_strings);
	return i;
}
#endif /* CLIFM_SUCKLESS */

static size_t
get_longest_ext_name(void)
{
	size_t i;
	size_t l = 0;

	for (i = 0; i < ext_colors_n; i++)
		if (ext_colors[i].len > l)
			l = ext_colors[i].len;

	return l;
}

static int
color_sort(const void *a, const void *b)
{
	struct ext_t *pa = (struct ext_t *)a;
	struct ext_t *pb = (struct ext_t *)b;

	const int ret = strcmp(pa->value, pb->value);
	if (ret != 0)
		return ret;

	return conf.case_sens_list == 1 ? strcmp(pa->name, pb->name)
		: strcasecmp(pa->name, pb->name);
}

static void
print_ext_colors(void)
{
	printf(_("\n%sFile extensions%s\n\n"), BOLD, df_c);

	const int l = (int)get_longest_ext_name() + 2; /* +2 == ".*" */
	int cols = term_cols / (l + 2); /* +2 == 2 ending spaces */
	if (cols <= 0)
		cols = 1;

	/* The ext_colors array is sorted by name hashes (to perform binary
	 * searches for file extension colors). But here we want to group
	 * extensions by color. */
	qsort(ext_colors, ext_colors_n, sizeof(*ext_colors),
		(QSFUNC *)color_sort);

	int n = 1;
	size_t i;

	for (i = 0; i < ext_colors_n; i++) {
		const int pad = l - (int)ext_colors[i].len;
		printf("\x1b[%sm*.%s%s%*s", ext_colors[i].value,
			ext_colors[i].name, NC, pad, "");
		if (n == cols) {
			n = 1;
			putchar('\n');
		} else {
			n++;
		}
	}

	printf("%s\n", df_c);

	qsort(ext_colors, ext_colors_n, sizeof(*ext_colors),
		(QSFUNC *)hash_sort);
}

static void
print_color_blocks(void)
{
	UNSET_LINE_WRAP;

	const int pad = (term_cols - 24) / 2;
	printf("\x1b[%dC\x1b[0;40m   \x1b[0m\x1b[0;41m   \x1b[0m\x1b[0;42m   "
		"\x1b[0m\x1b[0;43m   \x1b[0m\x1b[0;44m   \x1b[0m\x1b[0;45m   "
		"\x1b[0m\x1b[0;46m   \x1b[0m\x1b[0;47m   \x1b[0m\n", pad);

	printf("\x1b[%dC\x1b[0m\x1b[0;100m   \x1b[0m\x1b[0;101m   "
		"\x1b[0m\x1b[0;102m   \x1b[0m\x1b[0;103m   \x1b[0m\x1b[0;104m   "
		"\x1b[0m\x1b[0;105m   \x1b[0m\x1b[0;106m   \x1b[0m\x1b[0;107m   "
		"\x1b[0m\n\n", pad);

	SET_LINE_WRAP;
}

static void
print_file_type_colors(void)
{
	printf(_("%sFile types%s\n\n"), BOLD, df_c);

	printf(_("%sColor%s (di) Directory\n"), di_c, df_c);
	printf(_("%sColor%s (ed) Empty directory\n"), ed_c, df_c);
	if (*nd_c)
		printf(_("%sColor%s (nd) Directory with no read/exec permission\n"),
			nd_c, df_c);
	printf(_("%sColor%s (fi) Regular file\n"), fi_c, df_c);
	printf(_("%sColor%s (ef) Empty file\n"), ef_c, df_c);
	if (*nf_c)
		printf(_("%sColor%s (nf) File with no read permission\n"),
			nf_c, df_c);
	printf(_("%sColor%s (ex) Executable file\n"), ex_c, df_c);
	printf(_("%sColor%s (ee) Empty executable file\n"), ee_c, df_c);
	printf(_("%sColor%s (ln) Symbolic link\n"), ln_c, df_c);
	printf(_("%sColor%s (or) Broken symbolic link\n"), or_c, df_c);
	printf(_("%sColor%s (mh) Multi-hardlink\n"), mh_c, df_c);
	printf(_("%sColor%s (bd) Block device\n"), bd_c, df_c);
	printf(_("%sColor%s (cd) Character device\n"), cd_c, df_c);
	printf(_("%sColor%s (so) Socket file\n"), so_c, df_c);
	printf(_("%sColor%s (pi) Pipe or FIFO special file\n"), pi_c, df_c);
#ifdef SOLARIS_DOORS
	printf(_("%sColor%s (oo) Door/Port file\n"), oo_c, df_c);
#endif // SOLARIS_DOORS
	printf(_("%sColor%s (su) SUID file\n"), su_c, df_c);
	printf(_("%sColor%s (sg) SGID file\n"), sg_c, df_c);
	printf(_("%sColor%s (ca) File with capabilities\n"), ca_c, df_c);
	printf(_("%sColor%s (st) Sticky and NOT other-writable "
		 "directory\n"), st_c, df_c);
	printf(_("%sColor%s (tw) Sticky and other-writable directory\n"),
		tw_c, df_c);
	printf(_("%sColor%s (ow) Other-writable and NOT sticky directory\n"),
	    ow_c, df_c);
	printf(_("%sColor%s (no) Unknown file type\n"), no_c, df_c);
	printf(_("%sColor%s (uf) Unaccessible (non-stat'able) file\n"),
		uf_c, df_c);
}

static void
print_size_shades(void)
{
	fputs(_("      (dz)  Size (unset: using shades)\n              "), stdout);

	char tstr[MAX_SHADE_LEN]; *tstr = '\0';

	get_color_size((off_t)1, tstr, sizeof(tstr));
	printf("%sbytes%s ", tstr, df_c);

	get_color_size((off_t)1024, tstr, sizeof(tstr));
	printf("%sKb%s ", tstr, df_c);

	get_color_size((off_t)1024 * 1024, tstr, sizeof(tstr));
	printf("%sMb%s ", tstr, df_c);

	get_color_size((off_t)1024 * 1024 * 1024, tstr, sizeof(tstr));
	printf("%sGb%s ", tstr, df_c);

	get_color_size((off_t)1024 * 1024 * 1024 * 1024, tstr, sizeof(tstr));
	printf(_("%sbigger%s\n"), tstr, df_c);
}

static void
print_date_shades(const time_t t)
{
	fputs(_("      (dd)  Date (unset: using shades)\n              "), stdout);

	char tstr[MAX_SHADE_LEN]; *tstr = '\0';

	get_color_age(t - (60LL*60), tstr, sizeof(tstr));
	printf(_("%shour%s "), tstr, df_c);

	get_color_age(t - (24LL*60*60), tstr, sizeof(tstr));
	printf(_("%sday%s "), tstr, df_c);

	get_color_age(t - (7LL*24*60*60), tstr, sizeof(tstr));
	printf(_("%sweek%s "), tstr, df_c);

	get_color_age(t - (4LL*7*24*60*60), tstr, sizeof(tstr));
	printf(_("%smonth%s "), tstr, df_c);

	get_color_age(t - (4LL*7*24*60*60+1), tstr, sizeof(tstr));
	printf(_("%solder%s\n"), tstr, df_c);
}

static void
print_date_colors(void)
{
	const time_t t = time(NULL);
	props_now = t;

	if (*dd_c) {
		printf(_("%sColor%s (dd)  Date (e.g. %sJul 9 08:12%s)\n"),
			dd_c, df_c, dd_c, df_c);
	} else {
		print_date_shades(t);
	}

	char tstr[MAX_SHADE_LEN]; *tstr = '\0';
	get_color_age(t - (24LL*60*60), tstr, sizeof(tstr));
	printf(_("%s%sColor%s (dt)  Timestamp mark (e.g. %sMay 25 22:08%sm%s)\n"),
		tstr, dt_c, df_c, tstr, dt_c, df_c);
}

static void
print_prop_colors(void)
{
	printf(_("\n%sProperties / Long view%s\n\n"), BOLD, df_c);

	printf(_("%sColor%s (dr)  Read bit (%sr%s)\n"), dr_c, df_c, dr_c, df_c);
	printf(_("%sColor%s (dw)  Write bit (%sw%s)\n"), dw_c, df_c, dw_c, df_c);
	printf(_("%sColor%s (dxd) Execute bit - directory (%sx%s)\n"),
		dxd_c, df_c, dxd_c, df_c);
	printf(_("%sColor%s (dxr) Execute bit - file (%sx%s)\n"),
		dxr_c, df_c, dxr_c, df_c);
	printf(_("%sColor%s (dp)  SUID/SGID bit (e.g. %ss%s)\n"), dp_c,
		df_c, dp_c, df_c);
	printf(_("%sColor%s (du)  User ID (e.g. %sjane%s)\n"),
		du_c, df_c, du_c, df_c);
	printf(_("%s%sColor%s (dg)  Group ID (e.g. %s%swheel%s)\n"),
		du_c, dg_c, df_c, du_c, dg_c, df_c);

	if (*dz_c) {
		printf(_("%sColor%s (dz)  Size (e.g. %s12.69k%s)\n"),
			dz_c, df_c, dz_c, df_c);
	} else {
		print_size_shades();
	}

	print_date_colors();

	printf(_("%sColor%s (db)  Used blocks (e.g. %s1576%s)\n"),
		db_c, df_c, db_c, df_c);
	printf(_("%sColor%s (dk)  Links number (e.g. %s92%s)\n"),
		dk_c, df_c, dk_c, df_c);
	printf(_("%sColor%s (de)  Inode number (e.g. %s802721%s)\n"),
		de_c, df_c, de_c, df_c);
	printf(_("%sColor%s (do)  Octal permissions (e.g. %s0640%s)\n"),
		do_c, df_c, do_c, df_c);
	printf(_("%sColor%s (dn)  Dot/dash (e.g. %sr%sw%s-.%sr%s--.--%s)\n"),
		dn_c, df_c, dr_c, dw_c, dn_c, dr_c, dn_c, df_c);

}

static void
print_interface_colors(void)
{
	printf(_("\n%sInterface%s\n\n"), BOLD, df_c);

	printf(_("%sColor%s (el) ELN's (e.g. %s12%s filename)\n"),
		el_c, df_c, el_c, df_c);
	printf(_("%sColor%s (fc) File counter (e.g. dir%s/24%s)\n"),
		fc_c, df_c, fc_c, df_c);
	printf(_("%sColor%s (lc) Symbolic link indicator (e.g. %s36%s"
		"%s%s%ssymlink) (%s1%s)\n"), lc_c, df_c, el_c, df_c, lc_c,
		term_caps.unicode == 1 ? LINK_STR_U : LINK_STR, df_c, BOLD, df_c);
	printf(_("%sColor%s (li) Selected file indicator (e.g. %s12%s"
		"%s%s%sfilename)\n"), li_cb, df_c, el_c, df_c, li_cb,
		term_caps.unicode == 1 ? SELFILE_STR_U : SELFILE_STR, df_c);
	printf(_("%sColor%s (tt) Truncated filenames mark (e.g. "
		"filenam%s%c%s.odt)\n"), tt_c, df_c, tt_c, TRUNC_FILE_CHR, df_c);
	printf(_("%sColor%s (dl) Dividing line (e.g. %s------>%s)\n"),
		dl_c, df_c, dl_c, df_c);
	printf(_("%sColor%s (mi) Miscellaneous indicator (%s%s%s) (%s2%s)\n"),
		mi_c, df_c, mi_c, MSG_PTR_STR, df_c, BOLD, df_c);
	printf(_("%sColor%s (ts) Matching completion prefix (e.g. "
		"%sfile%sname) (%s3%s)\n"), ts_c, df_c, ts_c, df_c, BOLD, df_c);
	printf(_("%sColor%s (df) Default color\n"), df_c, df_c);

	printf(_("\n(%s1%s) Used only when ColorLinksAsTarget is "
		"enabled\n"), BOLD, df_c);
	printf(_("(%s2%s) Also used for miscellaneous names (like bookmarks "
		"and color schemes) in tab completion\n"), BOLD, df_c);
	printf(_("(%s3%s) Used only for the standard tab completion "
		"mode\n"), BOLD, df_c);
}

static void
print_workspace_colors(void)
{
	printf(_("\n%sWorkspaces%s\n\n"), BOLD, df_c);

	char *p = remove_ctrl_chars(ws1_c);
	printf(_("%sColor%s (ws1) Workspace [%s1%s]\n"), p, df_c, p, df_c);
	p = remove_ctrl_chars(ws2_c);
	printf(_("%sColor%s (ws2) Workspace [%s2%s]\n"), p, df_c, p, df_c);
	p = remove_ctrl_chars(ws3_c);
	printf(_("%sColor%s (ws3) Workspace [%s3%s]\n"), p, df_c, p, df_c);
	p = remove_ctrl_chars(ws4_c);
	printf(_("%sColor%s (ws4) Workspace [%s4%s]\n"), p, df_c, p, df_c);
	p = remove_ctrl_chars(ws5_c);
	printf(_("%sColor%s (ws5) Workspace [%s5%s]\n"), p, df_c, p, df_c);
	p = remove_ctrl_chars(ws6_c);
	printf(_("%sColor%s (ws6) Workspace [%s6%s]\n"), p, df_c, p, df_c);
	p = remove_ctrl_chars(ws7_c);
	printf(_("%sColor%s (ws7) Workspace [%s7%s]\n"), p, df_c, p, df_c);
	p = remove_ctrl_chars(ws8_c);
	printf(_("%sColor%s (ws8) Workspace [%s8%s]\n"), p, df_c, p, df_c);
}

static void
print_prompt_colors(void)
{
	printf(_("\n%sPrompt%s\n\n"), BOLD, df_c);

	printf(_("%sColor%s (tx) Input text (e.g. \x1b[1m$\x1b[0m "
		"%sls%s %s-l%s %sfilename.zst%s)\n"), tx_c, df_c, tx_c, df_c,
		hp_c, df_c, tx_c, df_c);
	char *p = remove_ctrl_chars(ac_c);
	printf(_("%sColor%s (ac) Autocommand indicator (%sA%s)\n"),
		p, df_c, p, df_c);
	printf(_("%sColor%s (li) Selected files indicator (%s%c%s)\n"),
		li_cb, df_c, li_cb, SELFILE_CHR, df_c);
	p = remove_ctrl_chars(ti_c);
	printf(_("%sColor%s (ti) Trashed files indicator (%sT%s)\n"), p,
		df_c, p, df_c);
	p = remove_ctrl_chars(xs_c);
	printf(_("%sColor%s (xs) Success exit code (<%s0%s>)\n"),
		p, df_c, p, df_c);
	p = remove_ctrl_chars(xf_c);
	printf(_("%sColor%s (xf) Error exit code (e.g. <%s1%s>)\n"),
		p, df_c, p, df_c);
	p = remove_ctrl_chars(nm_c);
	printf(_("%sColor%s (nm) Notice message indicator (%sN%s)\n"),
		p, df_c, p, df_c);
	p = remove_ctrl_chars(wm_c);
	printf(_("%sColor%s (wm) Warning message indicator (%sW%s)\n"),
		p, df_c, p, df_c);
	p = remove_ctrl_chars(em_c);
	printf(_("%sColor%s (em) Error message indicator (%sE%s)\n"),
		p, df_c, p, df_c);
	p = remove_ctrl_chars(ro_c);
	printf(_("%sColor%s (ro) Read-only mode indicator (%sRO%s)\n"),
		p, df_c, p, df_c);
	p = remove_ctrl_chars(si_c);
	printf(_("%sColor%s (si) Stealth mode indicator (%sS%s)\n"),
		p, df_c, p, df_c);
}

static void
print_suggestion_colors(void)
{
#ifndef _NO_SUGGESTIONS
	printf("\n%sSuggestions%s\n\n", BOLD, df_c);
	printf("%sColor%s (sh) History (e.g. sud%so vim clifmrc%s)\n",
		sh_c, df_c, sh_c, df_c);
	printf("%sColor%s (sf) Filenames (e.g. thi%ss_filename%s)\n",
		sf_c, df_c, sf_c, df_c);
	printf("%sColor%s (sz) Filenames (fuzzy) (e.g. dwn %s%c%s "
		"%sDownloads%s)\n", sz_c, df_c, sp_c, SUG_POINTER, df_c,
		sz_c, df_c);
	printf("%sColor%s (sx) Internal command names and parameters "
		"(e.g. boo%skmarks%s)\n", sx_c, df_c, sx_c, df_c);
	printf("%sColor%s (sc) External command names (e.g. lib%sreoffice%s)\n",
		sc_c, df_c, sc_c, df_c);
	printf("%sColor%s (sb) Shell builtin names (e.g. ex%sport%s)\n",
		sb_c, df_c, sb_c, df_c);
	printf("%sColor%s (sd) Internal commands description (e.g. "
		"br %s(batch rename files)%s)\n", sd_c, df_c, sd_c, df_c);
	printf("%sColor%s (sp) Pointer (e.g. %s48%s %s%c%s %sfilename%s)\n",
		sp_c, df_c, hn_c, df_c, sp_c, SUG_POINTER, df_c, sf_c, df_c);
#else
	return;
#endif /* !_NO_SUGGESTIONS */
}

static void
print_highlight_colors(void)
{
#ifndef _NO_HIGHLIGHT
	printf(_("\n%sSyntax highlighting%s\n\n"), BOLD, df_c);

	printf(_("%sColor%s (hb) Brackets: %s(){}[]%s\n"),
		hb_c, df_c, hb_c, df_c);
	printf(_("%sColor%s (hc) Commented out text (e.g. some text "
		"%s#comment%s)\n"), hc_c, df_c, hc_c, df_c);
	printf(_("%sColor%s (hd) Slash (e.g. dir%s/%sfile)\n"),
		hd_c, df_c, hd_c, df_c);
	printf(_("%sColor%s (he) Expansion characters: %s~*%s\n"),
		he_c, df_c, he_c, df_c);
	printf(_("%sColor%s (hn) Number (e.g. pp %s12%s)\n"),
		hn_c, df_c, hn_c, df_c);
	printf(_("%sColor%s (hp) Command parameter (e.g. cmd %s--param%s)\n"),
		hp_c, df_c, hp_c, df_c);
	printf(_("%sColor%s (hq) Quoted text (e.g. %s\"some text\"%s)\n"),
		hq_c, df_c, hq_c, df_c);
	printf(_("%sColor%s (hr) Redirection characters: %s><%s\n"),
		hr_c, df_c, hr_c, df_c);
	printf(_("%sColor%s (hs) Process separator characters: %s|;&%s \n"),
		hs_c, df_c, hs_c, df_c);
	printf(_("%sColor%s (hv) Variable name (e.g. %s$FOO%s)\n"),
		hv_c, df_c, hv_c, df_c);
	printf(_("%sColor%s (hw) Backslash (e.g. sel this%s\\%s file%s\\%s "
		"name)\n"), hw_c, df_c, hw_c, df_c, hw_c, df_c);
#else
	return;
#endif /* !_NO_HIGHLIGHT */
}

static void
print_color_scheme_name(void)
{
	printf(_("%sColor scheme: %s%s%s\n\n"), BOLD, get_color_scheme_name(),
		df_c, ON_LSCOLORS);
}

/* List color codes for file types used by the program. */
void
color_codes(void)
{
	if (conf.colorize == 0) {
		printf(_("%s: Currently running without colors\n"), PROGRAM_NAME);
		return;
	}

	print_color_scheme_name();
	print_file_type_colors();
	print_ext_colors();
	print_prop_colors();
	print_interface_colors();
	print_workspace_colors();
	print_prompt_colors();
	print_highlight_colors();
	print_suggestion_colors();

	puts(_("\nThe bracketed field is the code required to modify the "
		 "color of the corresponding element in the color scheme file.\n\n"));

	print_color_blocks();
}
