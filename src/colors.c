/* colors.c -- functions to control interface colors */

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
#include <sys/types.h>
#ifdef __linux__
#include <sys/capability.h>
#endif
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>

#include "aux.h"
#include "checks.h"
#include "colors.h"
#include "listing.h"
#include "mime.h"
#include "misc.h"
#include "messages.h"
#include "file_operations.h"
#include "exec.h"
#include "config.h" /* set_div_line() */

#define RL_PRINTABLE    1
#define RL_NO_PRINTABLE 0 /* Add non-printing flags (\001 and \002)*/

/* Max amount of custom color variables in the color scheme file */
#define MAX_DEFS 64

#ifndef CLIFM_SUCKLESS
/* A struct to hold color variables */
struct defs_t {
	char *name;
	char *value;
};

struct defs_t *defs;
size_t defs_n = 0;
#endif /* CLIFM_SUCKLESS */

/* Retrieve the color corresponding to dir FILENAME with mode MODE
 * If LINKS > 2, we know the directory is populated, so that there's no need
 * to run count_dir() */
char *
get_dir_color(const char *filename, const mode_t mode, const nlink_t links)
{
	char *color = (char *)NULL;
	int sticky = 0;
	int is_oth_w = 0;
	if (mode & S_ISVTX)
		sticky = 1;

	if (mode & S_IWOTH)
		is_oth_w = 1;

	int files_dir = links > 2 ? (int)links : count_dir(filename, CPOP);

	color = sticky ? (is_oth_w ? tw_c : st_c) : is_oth_w ? ow_c
		   : ((files_dir == 2 || files_dir == 0) ? ed_c : di_c);

	return color;
}

char *
get_file_color(const char *filename, const struct stat *attr)
{
	char *color = (char *)NULL;

#ifdef _LINUX_CAP
	cap_t cap;
#else
	UNUSED(filename);
#endif
	if (attr->st_mode & 04000) { /* SUID */
		color = su_c;
	} else if (attr->st_mode & 02000) { /* SGID */
		color = sg_c;
	}
#ifdef _LINUX_CAP
	else if (check_cap && (cap = cap_get_file(filename))) {
		color = ca_c;
		cap_free(cap);
	}
#endif
	else if ((attr->st_mode & 00100) /* Exec */
	|| (attr->st_mode & 00010) || (attr->st_mode & 00001)) {
		color = FILE_SIZE_PTR == 0 ? ee_c : ex_c;
	} else if (FILE_SIZE_PTR == 0) {
		color = ef_c;
	} else if (attr->st_nlink > 1) { /* Multi-hardlink */
		color = mh_c;
	} else { /* Regular file */
		color = fi_c;
	}

	return color;
}

/* Validate a hex color code string with this format: RRGGBB-[1-9] */
static int
is_hex_color(const char *str)
{
	size_t c = 0;

	while (*str) {
		c++;
		if (c == 7 && *str == '-') {
			if (!*(str + 1))
				return 0;
			return (*(str + 1) >= '0' && *(str + 1) <= '9');
		}
		if ( !( (*str >= '0' && *str <= '9') || (*str >= 'a' && *str <= 'f')
		|| (*str >= 'A' && *str <= 'F') ) )
			return 0;
		str++;
	}

	if (c != 6)
		return 0;

	return 1;
}

/* Check if STR has the format of a color code string (a number or a
 * semicolon list (max 12 fields) of numbers of at most 3 digits each).
 * Hex color codes (#RRGGBB) are also validated
 * Returns 1 if true and 0 if false. */
static int
is_color_code(const char *str)
{
	if (!str || !*str)
		return 0;

	if (*str == '#')
		return is_hex_color(str + 1);

	size_t digits = 0, semicolon = 0;

	while (*str) {
		if (*str >= '0' && *str <= '9') {
			digits++;
		} else if (*str == ';') {
			if (*(str + 1) == ';') /* Consecutive semicolons */
				return 0;
			digits = 0;
			semicolon++;
		} else {
			if (*str != '\n') /* Neither digit nor semicolon */
				return 0;
		}
		str++;
	}

	/* No digits at all, ending semicolon, more than eleven fields, or
	 * more than three consecutive digits */
	if (!digits || digits > 3 || semicolon > 11)
		return 0;

	/* At this point, we have a semicolon separated string of digits (3
	 * consecutive max) with at most 12 fields. The only thing not
	 * validated here are numbers themselves */
	return 1;
}

#ifndef CLIFM_SUCKLESS
/* If STR is a valid color variable name, return the value of this variable */
static char *
check_defs(char *str)
{
	if (defs_n == 0 || !str || !*str)
		return (char *)NULL;

	char *val = (char *)NULL;
	int i = (int)defs_n;

	while (--i >= 0) {
		if (!defs[i].name || !defs[i].value || !*defs[i].name || !*defs[i].value)
			continue;
		if (*defs[i].name == *str && strcmp(defs[i].name, str) == 0
		&& is_color_code(defs[i].value) == 1) {
			val = defs[i].value;
			return val;
		}
	}

	return val;
}

/* Free custom color variables set from the color scheme file */
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
	defs = (struct defs_t *)NULL;
}
#endif /* CLIFM_SUCKLESS */

/* Returns a pointer to the corresponding color code for EXT, if any */
char *
get_ext_color(char *ext)
{
	if (!ext || !*ext || !*(ext + 1) || ext_colors_n == 0)
		return (char *)NULL;

	ext++;

	int i = (int)ext_colors_n;
	while (--i >= 0) {
		if (!ext_colors[i] || !*ext_colors[i] || !ext_colors[i][1] || !ext_colors[i][2])
			continue;

		char *p = ext, *q = ext_colors[i];
		q += 2; /* +2 because stored extensions have this form: *.ext */

		size_t match = 1;
		while (*p) {
			if (*p != *q) {
				match = 0;
				break;
			}
			p++;
			q++;
		}

		if (!match || *q != '=')
			continue;

		q++;
		return q ? q : (char *)NULL;
	}

	return (char *)NULL;
}

#ifndef CLIFM_SUCKLESS
/* Strip color lines from the config file (FiletypeColors, if mode is
 * 't', and ExtColors, if mode is 'x') returning the same string
 * containing only allowed characters */
static char *
strip_color_line(const char *str, char mode)
{
	if (!str || !*str) return (char *)NULL;

	char *buf = (char *)xnmalloc(strlen(str) + 1, sizeof(char));
	size_t len = 0;

	switch (mode) {
	case 't': /* di=01;31: */
		while (*str) {
			if ((*str >= '0' && *str <= '9') || (*str >= 'a' && *str <= 'z')
			|| (*str >= 'A' && *str <= 'Z')
			|| *str == '=' || *str == ';' || *str == ':'
			|| *str == '#' || *str == '-')
				{buf[len] = *str; len++;}
			str++;
		}
		break;

	case 'x': /* *.tar=01;31: */
		while (*str) {
			if ((*str >= '0' && *str <= '9') || (*str >= 'a' && *str <= 'z')
			|| (*str >= 'A' && *str <= 'Z') || *str == '*' || *str == '.'
			|| *str == '=' || *str == ';' || *str == ':'
			|| *str == '#' || *str == '-')
				{buf[len] = *str; len++;}
			str++;
		}
		break;
	default: break;
	}

	if (!len || !*buf)
		{free(buf); return (char *)NULL;}

	buf[len] = '\0';
	return buf;
}
#endif /* CLIFM_SUCKLESS */

void
reset_filetype_colors(void)
{
	*nd_c = '\0';
	*nf_c = '\0';
	*di_c = '\0';
	*ed_c = '\0';
	*ne_c = '\0';
	*ex_c = '\0';
	*ee_c = '\0';
	*bd_c = '\0';
	*ln_c = '\0';
	*mh_c = '\0';
	*or_c = '\0';
	*so_c = '\0';
	*pi_c = '\0';
	*cd_c = '\0';
	*fi_c = '\0';
	*ef_c = '\0';
	*su_c = '\0';
	*sg_c = '\0';
	*ca_c = '\0';
	*st_c = '\0';
	*tw_c = '\0';
	*ow_c = '\0';
	*no_c = '\0';
	*uf_c = '\0';
}

void
reset_iface_colors(void)
{
	*hb_c = '\0';
	*hc_c = '\0';
	*hd_c = '\0';
	*he_c = '\0';
	*hn_c = '\0';
	*hp_c = '\0';
	*hq_c = '\0';
	*hr_c = '\0';
	*hs_c = '\0';
	*hv_c = '\0';
	*hw_c = '\0';

	*sh_c = '\0';
	*sf_c = '\0';
	*sc_c = '\0';
	*sx_c = '\0';
	*sp_c = '\0';

	*bm_c = '\0';
	*dl_c = '\0';
	*el_c = '\0';
	*mi_c = '\0';
	*tx_c = '\0';
	*df_c = '\0';
	*fc_c = '\0';
	*wc_c = '\0';
	*li_c = '\0';
	*li_cb = '\0';
	*ti_c = '\0';
	*em_c = '\0';
	*wm_c = '\0';
	*nm_c = '\0';
	*si_c = '\0';
	*ts_c = '\0';
	*wp_c = '\0';
	*tt_c = '\0';
	*xs_c = '\0';
	*xf_c = '\0';

	*ws1_c = '\0';
	*ws2_c = '\0';
	*ws3_c = '\0';
	*ws4_c = '\0';
	*ws5_c = '\0';
	*ws6_c = '\0';
	*ws7_c = '\0';
	*ws8_c = '\0';

	*dr_c = '\0';
	*dw_c = '\0';
	*dxd_c = '\0';
	*dxr_c = '\0';
	*dg_c = '\0';
	*dd_c = '\0';
	*dz_c = '\0';
	*do_c = '\0';
	*dp_c = '\0';
	*dn_c = '\0';
}

/* Disable colors for suggestions */
void
unset_suggestions_color(void)
{
	strcpy(sb_c, SUG_NO_COLOR); /* shell built-ins */
	strcpy(sc_c, SUG_NO_COLOR); /* external commands */
	strcpy(sf_c, SUG_NO_COLOR); /* filenames */
	strcpy(sh_c, SUG_NO_COLOR); /* history */
	strcpy(sp_c, SUG_NO_COLOR); /* suggestions pointer */
	strcpy(sx_c, SUG_NO_COLOR); /* internal commands and params */
}

/* Import the color scheme NAME from DATADIR (usually /usr/local/share)
 * Return zero on success or one on failure */
int
import_color_scheme(const char *name)
{
	if (!data_dir || !*data_dir || !colors_dir || !*colors_dir
	|| !name || !*name)
		return EXIT_FAILURE;

	char dfile[PATH_MAX];
//	snprintf(dfile, PATH_MAX - 1, "%s/%s/colors/%s.cfm", data_dir, PNL, name);
	snprintf(dfile, PATH_MAX - 1, "%s/%s/colors/%s.clifm", data_dir, PNL, name);

	struct stat attr;
	if (stat(dfile, &attr) == -1)
		return EXIT_FAILURE;

	char *cmd[] = {"cp", dfile, colors_dir, NULL};
	if (launch_execve(cmd, FOREGROUND, E_NOFLAG) == EXIT_SUCCESS)
		return EXIT_SUCCESS;

	return EXIT_FAILURE;
}

#ifndef CLIFM_SUCKLESS
static int
print_cur_colorscheme(void)
{
	if (!cschemes_n) {
		printf(_("%s: No color schemes found\n"), PROGRAM_NAME);
		return EXIT_SUCCESS;
	}
	size_t i;
	for (i = 0; color_schemes[i]; i++) {
		if (cur_cscheme == color_schemes[i])
			printf("%s%s%s\n", mi_c, color_schemes[i], df_c);
		else
			printf("%s\n", color_schemes[i]);
	}

	return EXIT_SUCCESS;
}

/* Edit the current color scheme file
 * If the file is not in the local colors dir, try to copy it from DATADIR
 * into the local dir to avoid permission issues */
static int
edit_colorscheme(char *app)
{
	if (!colors_dir) {
		fprintf(stderr, _("%s: No color scheme found\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	if (!cur_cscheme) {
		fprintf(stderr, _("%s: Current color scheme is unknown\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	struct stat attr;
	char file[PATH_MAX];

//	snprintf(file, PATH_MAX - 1, "%s/%s.cfm", colors_dir, cur_cscheme); /* NOLINT */
	snprintf(file, PATH_MAX - 1, "%s/%s.clifm", colors_dir, cur_cscheme); /* NOLINT */
	if (stat(file, &attr) == -1 && import_color_scheme(cur_cscheme) != EXIT_SUCCESS) {
		fprintf(stderr, _("%s: %s: No such color scheme\n"), PROGRAM_NAME, cur_cscheme);
		return EXIT_FAILURE;
	}

	stat(file, &attr);
	time_t mtime_bfr = (time_t)attr.st_mtime;

	int ret = EXIT_FAILURE;
	char *app_path = (char *)NULL;
	if (app && *app && (app_path = get_cmd_path(app)) != NULL) {
		char *cmd[] = {app_path, file, NULL};
		if (launch_execve(cmd, FOREGROUND, E_NOSTDERR) == EXIT_SUCCESS)
			ret = EXIT_SUCCESS;
		free(app_path);
	} else {
		open_in_foreground = 1;
		ret = open_file(file);
		open_in_foreground = 0;
	}

	if (ret != EXIT_FAILURE) {
		stat(file, &attr);
		if (mtime_bfr != (time_t)attr.st_mtime
		&& set_colors(cur_cscheme, 0) == EXIT_SUCCESS && autols == 1)
			reload_dirlist();
	}

	return ret;
}

static int
set_colorscheme(char *arg)
{
	size_t i, cs_found = 0;
	for (i = 0; color_schemes[i]; i++) {
		if (*arg != *color_schemes[i]
		|| strcmp(arg, color_schemes[i]) != 0)
			continue;
		cs_found = 1;

		if (set_colors(arg, 0) != EXIT_SUCCESS)
			continue;
		cur_cscheme = color_schemes[i];

		switch_cscheme = 1;
		if (autols == 1)
			reload_dirlist();
		switch_cscheme = 0;

		return EXIT_SUCCESS;
	}

	if (cs_found == 0)
		fprintf(stderr, _("%s: No such color scheme\n"), PROGRAM_NAME);

	return EXIT_FAILURE;
}
#endif /* CLIFM_SUCKLESS */

int
cschemes_function(char **args)
{
#ifdef CLIFM_SUCKLESS
	UNUSED(args);
	printf("%s: colors: %s. Edit settings.h in the source code "
		"and recompile\n", PROGRAM_NAME, NOT_AVAILABLE);
	return EXIT_FAILURE;
#else
	if (xargs.stealth_mode == 1) {
		fprintf(stderr, _("%s: colors: %s\nTIP: To change the "
			"current color scheme use the following environment "
			"variables: CLIFM_FILE_COLORS, CLIFM_IFACE_COLORS, "
			"and CLIFM_EXT_COLORS\n"), PROGRAM_NAME, STEALTH_DISABLED);
		return EXIT_FAILURE;
	}

	if (colorize == 0) {
		printf("%s: Colors are disabled\n", PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	if (!args) return EXIT_FAILURE;

	if (!args[1]) return print_cur_colorscheme();

	if (IS_HELP(args[1])) { puts(_(CS_USAGE)); return EXIT_SUCCESS;	}

	if (*args[1] == 'e' && (!args[1][1] || strcmp(args[1], "edit") == 0))
		return edit_colorscheme(args[2]);

	if (*args[1] == 'n' && (!args[1][1] || strcmp(args[1], "name") == 0)) {
		printf(_("%s: Current color scheme: %s\n"), PROGRAM_NAME,
		    cur_cscheme ? cur_cscheme : "?");
		return EXIT_SUCCESS;
	}

	return set_colorscheme(args[1]);
#endif /* CLIFM_SUCKLESS */
}

/* Set color variable VAR (static global) to _COLOR + OFFSET
 * If not printable, add non-printing char flags (\001 and \002) */
static void
set_color(char *_color, int offset, char var[], int flag)
{
#ifndef CLIFM_SUCKLESS
	char *color_code = (char *)NULL;
	if (is_color_code(_color + offset) == 0
	&& (color_code = check_defs(_color + offset)) == NULL)
#else
	if (is_color_code(_color + offset) == 0)
#endif /* CLIFM_SUCKLESS */
	{
		/* A null color string will be set to the default value by
		 * set_default_colors function */
		*var = '\0';
		return;
	}

#ifndef CLIFM_SUCKLESS
	char *p = color_code ? color_code : (_color + offset);
#else
	char *p = _color + offset;
#endif /* CLIFM_SUCKLESS */

	char *s = (char *)NULL;
	if (*p == '#') {
		if (!(s = hex2rgb(p))) {
			*var = '\0';
			return;
		}
	}

	if (flag == RL_NO_PRINTABLE)
		snprintf(var, MAX_COLOR + 2, "\001\x1b[%sm\002", s ? s : p); /* NOLINT */
	else
		snprintf(var, MAX_COLOR - 1, "\x1b[0;%sm", s ? s : p); /* NOLINT */
}

static void
set_filetype_colors(char **colors, const size_t words)
{
	int i = (int)words;
	while (--i >= 0) {
		if (*colors[i] == 'd' && strncmp(colors[i], "di=", 3) == 0)
			set_color(colors[i], 3, di_c, RL_PRINTABLE);

		else if (*colors[i] == 'n' && strncmp(colors[i], "nd=", 3) == 0)
			set_color(colors[i], 3, nd_c, RL_PRINTABLE);

		else if (*colors[i] == 'e' && strncmp(colors[i], "ed=", 3) == 0)
			set_color(colors[i], 3, ed_c, RL_PRINTABLE);

		else if (*colors[i] == 'n' && strncmp(colors[i], "ne=", 3) == 0)
			set_color(colors[i], 3, ne_c, RL_PRINTABLE);

		else if (*colors[i] == 'f' && strncmp(colors[i], "fi=", 3) == 0)
			set_color(colors[i], 3, fi_c, RL_PRINTABLE);

		else if (*colors[i] == 'e' && strncmp(colors[i], "ef=", 3) == 0)
			set_color(colors[i], 3, ef_c, RL_PRINTABLE);

		else if (*colors[i] == 'n' && strncmp(colors[i], "nf=", 3) == 0)
			set_color(colors[i], 3, nf_c, RL_PRINTABLE);

		else if (*colors[i] == 'l' && strncmp(colors[i], "ln=", 3) == 0)
			set_color(colors[i], 3, ln_c, RL_PRINTABLE);

		else if (*colors[i] == 'o' && strncmp(colors[i], "or=", 3) == 0)
			set_color(colors[i], 3, or_c, RL_PRINTABLE);

		else if (*colors[i] == 'e' && strncmp(colors[i], "ex=", 3) == 0)
			set_color(colors[i], 3, ex_c, RL_PRINTABLE);

		else if (*colors[i] == 'e' && strncmp(colors[i], "ee=", 3) == 0)
			set_color(colors[i], 3, ee_c, RL_PRINTABLE);

		else if (*colors[i] == 'b' && strncmp(colors[i], "bd=", 3) == 0)
			set_color(colors[i], 3, bd_c, RL_PRINTABLE);

		else if (*colors[i] == 'c' && strncmp(colors[i], "cd=", 3) == 0)
			set_color(colors[i], 3, cd_c, RL_PRINTABLE);

		else if (*colors[i] == 'p' && strncmp(colors[i], "pi=", 3) == 0)
			set_color(colors[i], 3, pi_c, RL_PRINTABLE);

		else if (*colors[i] == 's' && strncmp(colors[i], "so=", 3) == 0)
			set_color(colors[i], 3, so_c, RL_PRINTABLE);

		else if (*colors[i] == 's' && strncmp(colors[i], "su=", 3) == 0)
			set_color(colors[i], 3, su_c, RL_PRINTABLE);

		else if (*colors[i] == 's' && strncmp(colors[i], "sg=", 3) == 0)
			set_color(colors[i], 3, sg_c, RL_PRINTABLE);

		else if (*colors[i] == 't' && strncmp(colors[i], "tw=", 3) == 0)
			set_color(colors[i], 3, tw_c, RL_PRINTABLE);

		else if (*colors[i] == 's' && strncmp(colors[i], "st=", 3) == 0)
			set_color(colors[i], 3, st_c, RL_PRINTABLE);

		else if (*colors[i] == 'o' && strncmp(colors[i], "ow=", 3) == 0)
			set_color(colors[i], 3, ow_c, RL_PRINTABLE);

		else if (*colors[i] == 'c' && strncmp(colors[i], "ca=", 3) == 0)
			set_color(colors[i], 3, ca_c, RL_PRINTABLE);

		else if (*colors[i] == 'n' && strncmp(colors[i], "no=", 3) == 0)
			set_color(colors[i], 3, no_c, RL_PRINTABLE);

		else if (*colors[i] == 'm' && strncmp(colors[i], "mh=", 3) == 0)
			set_color(colors[i], 3, mh_c, RL_PRINTABLE);

		else if (*colors[i] == 'u' && strncmp(colors[i], "uf=", 3) == 0)
			set_color(colors[i], 3, uf_c, RL_PRINTABLE);

		free(colors[i]);
	}
}

static void
set_iface_colors(char **colors, const size_t words)
{
	int i = (int)words;
	while (--i >= 0) {
		if (*colors[i] == 't' && strncmp(colors[i], "tx=", 3) == 0)
			set_color(colors[i], 3, tx_c, RL_PRINTABLE);

		else if (*colors[i] == 't' && strncmp(colors[i], "tt=", 3) == 0)
			set_color(colors[i], 3, tt_c, RL_PRINTABLE);

		else if (*colors[i] == 'w' && strncmp(colors[i], "ws1=", 4) == 0)
			set_color(colors[i], 4, ws1_c, RL_NO_PRINTABLE);

		else if (*colors[i] == 'w' && strncmp(colors[i], "ws2=", 4) == 0)
			set_color(colors[i], 4, ws2_c, RL_NO_PRINTABLE);

		else if (*colors[i] == 'w' && strncmp(colors[i], "ws3=", 4) == 0)
			set_color(colors[i], 4, ws3_c, RL_NO_PRINTABLE);

		else if (*colors[i] == 'w' && strncmp(colors[i], "ws4=", 4) == 0)
			set_color(colors[i], 4, ws4_c, RL_NO_PRINTABLE);

		else if (*colors[i] == 'w' && strncmp(colors[i], "ws5=", 4) == 0)
			set_color(colors[i], 4, ws5_c, RL_NO_PRINTABLE);

		else if (*colors[i] == 'w' && strncmp(colors[i], "ws6=", 4) == 0)
			set_color(colors[i], 4, ws6_c, RL_NO_PRINTABLE);

		else if (*colors[i] == 'w' && strncmp(colors[i], "ws7=", 4) == 0)
			set_color(colors[i], 4, ws7_c, RL_NO_PRINTABLE);

		else if (*colors[i] == 'w' && strncmp(colors[i], "ws8=", 4) == 0)
			set_color(colors[i], 4, ws8_c, RL_NO_PRINTABLE);

		else if (*colors[i] == 'h' && strncmp(colors[i], "hb=", 3) == 0)
			set_color(colors[i], 3, hb_c, RL_PRINTABLE);

		else if (*colors[i] == 'h' && strncmp(colors[i], "hc=", 3) == 0)
			set_color(colors[i], 3, hc_c, RL_PRINTABLE);

		else if (*colors[i] == 'h' && strncmp(colors[i], "hd=", 3) == 0)
			set_color(colors[i], 3, hd_c, RL_PRINTABLE);

		else if (*colors[i] == 'h' && strncmp(colors[i], "he=", 3) == 0)
			set_color(colors[i], 3, he_c, RL_PRINTABLE);

		else if (*colors[i] == 'h' && strncmp(colors[i], "hn=", 3) == 0)
			set_color(colors[i], 3, hn_c, RL_PRINTABLE);

		else if (*colors[i] == 'h' && strncmp(colors[i], "hp=", 3) == 0)
			set_color(colors[i], 3, hp_c, RL_PRINTABLE);

		else if (*colors[i] == 'h' && strncmp(colors[i], "hq=", 3) == 0)
			set_color(colors[i], 3, hq_c, RL_PRINTABLE);

		else if (*colors[i] == 'h' && strncmp(colors[i], "hr=", 3) == 0)
			set_color(colors[i], 3, hr_c, RL_PRINTABLE);

		else if (*colors[i] == 'h' && strncmp(colors[i], "hs=", 3) == 0)
			set_color(colors[i], 3, hs_c, RL_PRINTABLE);

		else if (*colors[i] == 'h' && strncmp(colors[i], "hv=", 3) == 0)
			set_color(colors[i], 3, hv_c, RL_PRINTABLE);

/*		else if (*colors[i] == 'h' && strncmp(colors[i], "hw=", 3) == 0)
			set_color(colors[i], 3, hw_c, RL_PRINTABLE); */

		else if (*colors[i] == 's' && strncmp(colors[i], "sb=", 3) == 0)
			set_color(colors[i], 3, sb_c, RL_PRINTABLE);

		else if (*colors[i] == 's' && strncmp(colors[i], "sc=", 3) == 0)
			set_color(colors[i], 3, sc_c, RL_PRINTABLE);

		else if (*colors[i] == 's' && strncmp(colors[i], "sh=", 3) == 0)
			set_color(colors[i], 3, sh_c, RL_PRINTABLE);

		else if (*colors[i] == 's' && strncmp(colors[i], "sf=", 3) == 0)
			set_color(colors[i], 3, sf_c, RL_PRINTABLE);

		else if (*colors[i] == 's' && strncmp(colors[i], "sp=", 3) == 0)
			set_color(colors[i], 3, sp_c, RL_PRINTABLE);

		else if (*colors[i] == 's' && strncmp(colors[i], "sx=", 3) == 0)
			set_color(colors[i], 3, sx_c, RL_PRINTABLE);

		else if (*colors[i] == 'b' && strncmp(colors[i], "bm=", 3) == 0)
			set_color(colors[i], 3, bm_c, RL_PRINTABLE);

		else if (*colors[i] == 'l' && strncmp(colors[i], "li=", 3) == 0) {
			set_color(colors[i], 3, li_c, RL_NO_PRINTABLE);
			set_color(colors[i], 3, li_cb, RL_PRINTABLE);
		}

		else if (*colors[i] == 't' && strncmp(colors[i], "ti=", 3) == 0)
			set_color(colors[i], 3, ti_c, RL_NO_PRINTABLE);

		else if (*colors[i] == 'e' && strncmp(colors[i], "em=", 3) == 0)
			set_color(colors[i], 3, em_c, RL_NO_PRINTABLE);

		else if (*colors[i] == 'w' && strncmp(colors[i], "wm=", 3) == 0)
			set_color(colors[i], 3, wm_c, RL_NO_PRINTABLE);

		else if (*colors[i] == 'n' && strncmp(colors[i], "nm=", 3) == 0)
			set_color(colors[i], 3, nm_c, RL_NO_PRINTABLE);

		else if (*colors[i] == 's' && strncmp(colors[i], "si=", 3) == 0)
			set_color(colors[i], 3, si_c, RL_NO_PRINTABLE);

		else if (*colors[i] == 'e' && strncmp(colors[i], "el=", 3) == 0)
			set_color(colors[i], 3, el_c, RL_PRINTABLE);

		else if (*colors[i] == 'm' && strncmp(colors[i], "mi=", 3) == 0)
			set_color(colors[i], 3, mi_c, RL_PRINTABLE);

		else if (*colors[i] == 'd' && strncmp(colors[i], "dl=", 3) == 0)
			set_color(colors[i], 3, dl_c, RL_PRINTABLE);

		else if (*colors[i] == 'd' && strncmp(colors[i], "df=", 3) == 0)
			set_color(colors[i], 3, df_c, RL_PRINTABLE);

		else if (*colors[i] == 'f' && strncmp(colors[i], "fc=", 3) == 0)
			set_color(colors[i], 3, fc_c, RL_PRINTABLE);

		else if (*colors[i] == 'w' && strncmp(colors[i], "wc=", 3) == 0)
			set_color(colors[i], 3, wc_c, RL_PRINTABLE);

		else if (*colors[i] == 't' && strncmp(colors[i], "ts=", 3) == 0)
			set_color(colors[i], 3, ts_c, RL_PRINTABLE);

		else if (*colors[i] == 'w' && strncmp(colors[i], "wp=", 3) == 0)
			set_color(colors[i], 3, wp_c, RL_PRINTABLE);

		else if (*colors[i] == 'd' && strncmp(colors[i], "dr=", 3) == 0)
			set_color(colors[i], 3, dr_c, RL_PRINTABLE);

		else if (*colors[i] == 'd' && strncmp(colors[i], "dw=", 3) == 0)
			set_color(colors[i], 3, dw_c, RL_PRINTABLE);

		else if (*colors[i] == 'd' && strncmp(colors[i], "dxd=", 4) == 0)
			set_color(colors[i], 4, dxd_c, RL_PRINTABLE);

		else if (*colors[i] == 'd' && strncmp(colors[i], "dxr=", 4) == 0)
			set_color(colors[i], 4, dxr_c, RL_PRINTABLE);

		else if (*colors[i] == 'd' && strncmp(colors[i], "dg=", 3) == 0)
			set_color(colors[i], 3, dg_c, RL_PRINTABLE);

		else if (*colors[i] == 'd' && strncmp(colors[i], "dd=", 3) == 0)
			set_color(colors[i], 3, dd_c, RL_PRINTABLE);

		else if (*colors[i] == 'd' && strncmp(colors[i], "dz=", 3) == 0)
			set_color(colors[i], 3, dz_c, RL_PRINTABLE);

		else if (*colors[i] == 'd' && strncmp(colors[i], "do=", 3) == 0)
			set_color(colors[i], 3, do_c, RL_PRINTABLE);

		else if (*colors[i] == 'd' && strncmp(colors[i], "dp=", 3) == 0)
			set_color(colors[i], 3, dp_c, RL_PRINTABLE);

		else if (*colors[i] == 'd' && strncmp(colors[i], "dn=", 3) == 0)
			set_color(colors[i], 3, dn_c, RL_PRINTABLE);

		else if (*colors[i] == 'x' && strncmp(colors[i], "xs=", 3) == 0)
			set_color(colors[i], 3, xs_c, RL_NO_PRINTABLE);

		else if (*colors[i] == 'x' && strncmp(colors[i], "xf=", 3) == 0)
			set_color(colors[i], 3, xf_c, RL_NO_PRINTABLE);

		free(colors[i]);
	}
}

void
set_default_colors(void)
{
	if (!*hb_c) strcpy(hb_c, DEF_HB_C);
	if (!*hc_c) strcpy(hc_c, DEF_HC_C);
	if (!*hd_c) strcpy(hd_c, DEF_HD_C);
	if (!*he_c) strcpy(he_c, DEF_HE_C);
	if (!*hn_c) strcpy(hn_c, DEF_HN_C);
	if (!*hp_c) strcpy(hp_c, DEF_HP_C);
	if (!*hq_c) strcpy(hq_c, DEF_HQ_C);
	if (!*hr_c) strcpy(hr_c, DEF_HR_C);
	if (!*hs_c) strcpy(hs_c, DEF_HS_C);
	if (!*hv_c) strcpy(hv_c, DEF_HV_C);
	if (!*hw_c) strcpy(hw_c, DEF_HW_C);
	if (!*tt_c) strcpy(tt_c, DEF_TT_C);

	if (!*sb_c) strcpy(sb_c, DEF_SB_C);
	if (!*sc_c) strcpy(sc_c, DEF_SC_C);
	if (!*sh_c) strcpy(sh_c, DEF_SH_C);
	if (!*sf_c) strcpy(sf_c, DEF_SF_C);
	if (!*sx_c) strcpy(sx_c, DEF_SX_C);
	if (!*sp_c) strcpy(sp_c, DEF_SP_C);

	if (!*el_c) strcpy(el_c, DEF_EL_C);
	if (!*mi_c) strcpy(mi_c, DEF_MI_C);
	/* If unset from the config file, use current workspace color */
	if (!*dl_c && config_ok == 0) strcpy(dl_c, DEF_DL_C);

	if (!*df_c) strcpy(df_c, DEF_DF_C);
	if (!*fc_c) strcpy(fc_c, DEF_FC_C);
	if (!*wc_c) strcpy(wc_c, DEF_WC_C);
	if (!*tx_c) strcpy(tx_c, DEF_TX_C);
	if (!*li_c) strcpy(li_c, DEF_LI_C);
	if (!*li_cb) strcpy(li_cb, DEF_LI_CB);
	if (!*ti_c) strcpy(ti_c, DEF_TI_C);
	if (!*em_c) strcpy(em_c, DEF_EM_C);
	if (!*wm_c) strcpy(wm_c, DEF_WM_C);
	if (!*nm_c) strcpy(nm_c, DEF_NM_C);
	if (!*si_c) strcpy(si_c, DEF_SI_C);
	if (!*bm_c) strcpy(bm_c, DEF_BM_C);
	if (!*ts_c) strcpy(ts_c, DEF_TS_C);
	if (!*wp_c) strcpy(wp_c, DEF_WP_C);
	if (!*xs_c) strcpy(xs_c, DEF_XS_C);
	if (!*xf_c) strcpy(xf_c, DEF_XF_C);

	if (!*ws1_c) strcpy(ws1_c, DEF_WS1_C);
	if (!*ws2_c) strcpy(ws2_c, DEF_WS2_C);
	if (!*ws3_c) strcpy(ws3_c, DEF_WS3_C);
	if (!*ws4_c) strcpy(ws4_c, DEF_WS4_C);
	if (!*ws5_c) strcpy(ws5_c, DEF_WS5_C);
	if (!*ws6_c) strcpy(ws6_c, DEF_WS6_C);
	if (!*ws7_c) strcpy(ws7_c, DEF_WS7_C);
	if (!*ws8_c) strcpy(ws8_c, DEF_WS8_C);

	if (!*di_c) strcpy(di_c, DEF_DI_C);
	if (!*nd_c) strcpy(nd_c, DEF_ND_C);
	if (!*ed_c) strcpy(ed_c, DEF_ED_C);
	if (!*ne_c) strcpy(ne_c, DEF_NE_C);
	if (!*fi_c) strcpy(fi_c, DEF_FI_C);
	if (!*ef_c) strcpy(ef_c, DEF_EF_C);
	if (!*nf_c) strcpy(nf_c, DEF_NF_C);
	if (!*ln_c) strcpy(ln_c, DEF_LN_C);
	if (!*or_c) strcpy(or_c, DEF_OR_C);
	if (!*pi_c) strcpy(pi_c, DEF_PI_C);
	if (!*so_c) strcpy(so_c, DEF_SO_C);
	if (!*bd_c) strcpy(bd_c, DEF_BD_C);
	if (!*cd_c) strcpy(cd_c, DEF_CD_C);
	if (!*su_c) strcpy(su_c, DEF_SU_C);
	if (!*sg_c) strcpy(sg_c, DEF_SG_C);
	if (!*st_c) strcpy(st_c, DEF_ST_C);
	if (!*tw_c) strcpy(tw_c, DEF_TW_C);
	if (!*ow_c) strcpy(ow_c, DEF_OW_C);
	if (!*ex_c) strcpy(ex_c, DEF_EX_C);
	if (!*ee_c) strcpy(ee_c, DEF_EE_C);
	if (!*ca_c) strcpy(ca_c, DEF_CA_C);
	if (!*no_c) strcpy(no_c, DEF_NO_C);
	if (!*uf_c) strcpy(uf_c, DEF_UF_C);
	if (!*mh_c) strcpy(mh_c, DEF_MH_C);
#ifndef _NO_ICONS
	if (!*dir_ico_c) strcpy(dir_ico_c, DEF_DIR_ICO_C);
#endif

	if (!*dr_c) strcpy(dr_c, DEF_DR_C);
	if (!*dw_c) strcpy(dw_c, DEF_DW_C);
	if (!*dxd_c) strcpy(dxd_c, DEF_DXD_C);
	if (!*dxr_c) strcpy(dxr_c, DEF_DXR_C);
	if (!*dg_c) strcpy(dg_c, DEF_DG_C);
	if (!*dd_c) strcpy(dd_c, DEF_DD_C);
	if (!*dz_c) strcpy(dz_c, DEF_DZ_C);
	if (!*do_c) strcpy(do_c, DEF_DO_C);
	if (!*dp_c) strcpy(dp_c, DEF_DP_C);
	if (!*dn_c) strcpy(dn_c, DEF_DN_C);
}

static void
free_extension_colors(void)
{
	int i = (int)ext_colors_n;
	while (--i >= 0)
		free(ext_colors[i]);
	free(ext_colors);
	ext_colors = (char **)NULL;
	free(ext_colors_len);
	ext_colors_n = 0;
}

/* Set a pointer to the current color scheme */
static int
get_cur_colorscheme(const char *colorscheme)
{
	if (!colorscheme)
		return EXIT_FAILURE;

	char *def_cscheme = (char *)NULL;
	int i = (int)cschemes_n;
	while (--i >= 0) {
		if (*colorscheme == *color_schemes[i]
		&& strcmp(colorscheme, color_schemes[i]) == 0) {
			cur_cscheme = color_schemes[i];
			break;
		}

		if (*color_schemes[i] == 'd'
		&& strcmp(color_schemes[i], "default") == 0)
			def_cscheme = color_schemes[i];
	}

	if (!cur_cscheme) {
		_err('w', PRINT_PROMPT, _("%s: colors: %s: No such color scheme. "
			"Falling back to default\n"), PROGRAM_NAME, colorscheme);

		if (def_cscheme)
			cur_cscheme = def_cscheme;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

/* Try to retrieve colors from the environment */
static void
get_colors_from_env(char **file, char **ext, char **iface)
{
	char *env_filecolors = getenv("CLIFM_FILE_COLORS");
	char *env_extcolors = getenv("CLIFM_EXT_COLORS");
	char *env_ifacecolors = getenv("CLIFM_IFACE_COLORS");

	if (env_filecolors)
		*file = savestring(env_filecolors, strlen(env_filecolors));

	if (env_extcolors)
		*ext = savestring(env_extcolors, strlen(env_extcolors));

	if (env_ifacecolors)
		*iface = savestring(env_ifacecolors, strlen(env_ifacecolors));
}

#ifndef CLIFM_SUCKLESS
/* Store color variable defined in STR into the global defs struct */
static void
store_definition(char *str)
{
	if (!str || !*str || defs_n > MAX_DEFS)
		return;

	char *name = strchr(str, ' ');
	if (!name || !*(++name))
		return;

	char *value = strchr(name, '=');
	if (!value || !*(value + 1) || value == name)
		return;

	*value = '\0';
	value++;

	defs[defs_n].name = savestring(name, (size_t)(value - name - 1));

	size_t val_len;
	char *s = strchr(value, ' ');
	if (s) {
		*s = '\0';
		val_len = (size_t)(s - value);
	} else {
		val_len = strlen(value);
		if (val_len > 0 && value[val_len - 1] == '\n') {
			value[val_len - 1] = '\0';
			val_len--;
		}
	}

	defs[defs_n].value = savestring(value, val_len);
	defs_n++;
}

/* Initialize the defs_t struct */
static void
init_defs(void)
{
	int n = MAX_DEFS;
	while(--n >= 0) {
		defs[n].name = (char *)NULL;
		defs[n].value = (char *)NULL;
	}
}

/* Get color lines from the configuration file */
static int
get_colors_from_file(const char *colorscheme, char **filecolors,
                     char **extcolors, char **ifacecolors, const int env)
{
	/* Allocate some memory for custom color variables */
	defs = (struct defs_t *)xnmalloc(MAX_DEFS + 1, sizeof(struct defs_t));
	defs_n = 0;
	init_defs();

	char colorscheme_file[PATH_MAX];
	*colorscheme_file = '\0';
	if (config_ok == 1 && colors_dir) {
//		snprintf(colorscheme_file, PATH_MAX - 1, "%s/%s.cfm", colors_dir, /* NOLINT */
		snprintf(colorscheme_file, PATH_MAX - 1, "%s/%s.clifm", colors_dir, /* NOLINT */
			colorscheme ? colorscheme : "default");
	}

	/* If not in local dir, check system data dir as well */
	struct stat attr;
	if (data_dir && (!*colorscheme_file || stat(colorscheme_file, &attr) == -1)) {
//		snprintf(colorscheme_file, PATH_MAX - 1, "%s/%s/colors/%s.cfm", /* NOLINT */
		snprintf(colorscheme_file, PATH_MAX - 1, "%s/%s/colors/%s.clifm", /* NOLINT */
			data_dir, PNL, colorscheme ? colorscheme : "default");
	}

	FILE *fp_colors = fopen(colorscheme_file, "r");
	if (!fp_colors) {
		if (!env) {
			_err(ERR_NO_STORE, NOPRINT_PROMPT, "%s: colors: %s: %s\n", PROGRAM_NAME,
				colorscheme_file, strerror(errno));
			return EXIT_FAILURE;
		} else {
			_err('w', PRINT_PROMPT, _("%s: colors: %s: No such color scheme. "
				"Falling back to the default one\n"), PROGRAM_NAME, colorscheme);
			return EXIT_SUCCESS;
		}
	}

	/* If called from the color scheme function, reset all color values before proceeding */
	if (!env) {
		reset_filetype_colors();
		reset_iface_colors();
	}

	char *line = (char *)NULL;
	size_t line_size = 0;
	ssize_t line_len = 0;

	while ((line_len = getline(&line, &line_size, fp_colors)) > 0) {
		if (*line == '#' || *line == '\n')
			continue;

		if (*line == 'd' && strncmp(line, "define ", 7) == 0) {
			store_definition(line);
		}

		else if (*line == 'P' && strncmp(line, "Prompt=", 7) == 0) {
			char *p = strchr(line, '=');
			if (!p || !*p || !*(++p))
				continue;
			if (expand_prompt_name(p) != EXIT_SUCCESS) {
				free(encoded_prompt);
				encoded_prompt = savestring(p, strlen(p));
			}
			continue;
		}

		/* THIS OPTION IS DEPRECATED */
		else if (*line == 'P' && strncmp(line, "PromptStyle=", 12) == 0) {
			char *p = strchr(line, '=');
			if (!p || !*(++p))
				continue;
			if (*p == 'd' && strncmp(p, "default", 7) == 0)
				prompt_notif = 1;
			else if (*p == 'c' && strncmp(p, "custom", 6) == 0)
				prompt_notif = 0;
			else
				prompt_notif = DEF_PROMPT_NOTIF;
		}

		/* The following values override those set via the Prompt line
		 * (provided it was set to a valid prompt name, as defined in the
		 * prompts file)*/
		else if (*line == 'N' && strncmp(line, "Notifications=", 14) == 0) {
			char *p = strchr(line, '=');
			if (!p || !*(++p))
				continue;
			if (*p == 't' && strncmp(p, "true", 4) == 0)
				prompt_notif = 1;
			else if (*p == 'f' && strncmp(p, "false", 5) == 0)
				prompt_notif = 0;
			else
				prompt_notif = DEF_PROMPT_NOTIF;
		}

/*		else if (*line == 'R' && strncmp(line, "RightPrompt=", 12) == 0) {
			char *p = strchr(line, '=');
			if (!p || !*p || !*(++p))
				continue;
			char *q = remove_quotes(p);
			if (!q)
				continue;
			free(right_prompt);
			right_prompt = savestring(q, strlen(q));
		} */

		else if (xargs.warning_prompt == UNSET && *line == 'E'
		&& strncmp(line, "EnableWarningPrompt=", 20) == 0) {
			char *p = strchr(line, '=');
			if (!p || !*(++p))
				continue;
			if (*p == 't' && strncmp(p, "true", 4) == 0)
				warning_prompt = 1;
			else if (*p == 'f' && strncmp(p, "false", 5) == 0)
				warning_prompt = 0;
			else
				warning_prompt = DEF_WARNING_PROMPT;
		}

		else if (*line == 'W' && strncmp(line, "WarningPrompt=", 14) == 0) {
			char *p = strchr(line, '=');
			if (!p || !*p || !*(++p))
				continue;
			char *q = remove_quotes(p);
			if (!q)
				continue;
			free(wprompt_str);
			wprompt_str = savestring(q, strlen(q));
		}
#ifndef _NO_FZF
		else if (*line == 'F' && strncmp(line, "FzfTabOptions=", 14) == 0) {
			char *p = strchr(line, '=');
			if (!p || !*p || !*(++p))
				continue;
			char *q = remove_quotes(p);
			if (!q)
				continue;
			free(fzftab_options);
			fzftab_options = savestring(q, strlen(q));
			fzf_height_set = 0;
			if (strstr(fzftab_options, "--height"))
				fzf_height_set = 1;
		}
#endif /* _NO_FZF */

		else if (*line == 'D' && strncmp(line, "DividingLine", 12) == 0) {
			set_div_line(line);
		}

		/* Interface colors */
		else if (!*ifacecolors && *line == 'I'
		&& strncmp(line, "InterfaceColors=", 16) == 0) {
			char *opt_str = strchr(line, '=');
			if (!opt_str)
				continue;

			opt_str++;
			char *color_line = strip_color_line(opt_str, 't');
			if (!color_line)
				continue;

			*ifacecolors = savestring(color_line, strlen(color_line));
			free(color_line);
		}

		/* Filetype colors */
		else if (!*filecolors && *line == 'F'
		&& strncmp(line, "FiletypeColors=", 15) == 0) {
			char *opt_str = strchr(line, '=');
			if (!opt_str)
				continue;

			opt_str++;

			char *color_line = strip_color_line(opt_str, 't');
			if (!color_line)
				continue;

			*filecolors = savestring(color_line, strlen(color_line));
			free(color_line);
		}

		/* File extension colors */
		else if (!*extcolors && *line == 'E' && strncmp(line, "ExtColors=", 10) == 0) {
			char *opt_str = strchr(line, '=');
			if (!opt_str || !*(++opt_str))
				continue;

			if (*opt_str == '\'' || *opt_str == '"')
				if (!*(++opt_str))
					continue;

			ssize_t len = line_len - (opt_str - line);
			if (len && opt_str[len - 1] == '\n') {
				opt_str[len - 1] = '\0';
				--len;
			}
			if (len && (opt_str[len - 1] == '\'' || opt_str[len - 1] == '"')) {
				opt_str[len - 1] = '\0';
				--len;
			}
			*extcolors = savestring(opt_str, (size_t)len);
		}

#ifndef _NO_ICONS
		/* Directory icon color */
		else if (*line == 'D' && strncmp(line, "DirIconColor=", 13) == 0) {
			char *opt_str = strchr(line, '=');
			if (!opt_str || !*(++opt_str))
				continue;

			if ((*opt_str == '\'' || *opt_str == '"') && !*(++opt_str))
				continue;

			if (line[line_len - 1] == '\n') {
				line[line_len - 1] = '\0';
				--line_len;
			}

			if (line[line_len - 1] == '\'' || line[line_len - 1] == '"')
				line[line_len - 1] = '\0';

			char *c = (char *)NULL;

			if (is_color_code(opt_str) == 0 && (c = check_defs(opt_str)) == NULL)
				continue;

			sprintf(dir_ico_c, "\x1b[%sm", c ? c : opt_str);
		}
#endif /* !_NO_ICONS */
	}

	free(line);
	fclose(fp_colors);

	return EXIT_SUCCESS;
}
#endif /* CLIFM_SUCKLESS */

/* Check if LINE contains a valid color code, and store it into the
 * ext_colors global array
 * If LINE contains a color variable, expand it, check it, and store it */
static int
store_extension_line(char *line, size_t len)
{
	char *q = strchr(line, '=');
	if (!q || !*(q + 1) || q == line)
		return EXIT_FAILURE;

	*q = '\0';
#ifndef CLIFM_SUCKLESS
	char *c = (char *)NULL;
	if (is_color_code(q + 1) == 0 && (c = check_defs(q + 1)) == NULL)
#else
	if (is_color_code(q + 1) == 0)
#endif /* CLIFM_SUCKLESS */
		return EXIT_FAILURE;

	ext_colors = (char **)xrealloc(ext_colors,
	             (ext_colors_n + 1) * sizeof(char *));
#ifndef CLIFM_SUCKLESS
	if (c) {
		size_t clen = (size_t)(q - line) + strlen(c) + 3;
		ext_colors[ext_colors_n] = (char *)xnmalloc(clen + 1, sizeof(char));
		sprintf(ext_colors[ext_colors_n], "%s=0;%s", line, c);
	} else
#endif /* CLIFM_SUCKLESS */
	{
		ext_colors[ext_colors_n] = (char *)xnmalloc(len + 3, sizeof(char));
		sprintf(ext_colors[ext_colors_n], "%s=0;%s", line, q + 1);
	}

	*q = '=';
	ext_colors_n++;

	return EXIT_SUCCESS;
}

static void
split_extensions_colors(char *extcolors)
{
	char *p = extcolors, *buf = (char *)NULL;
	size_t len = 0;
	int eol = 0;

	if (ext_colors_n)
		free_extension_colors();

	while (!eol) {
		switch (*p) {

		case '\0': /* fallthrough */
		case '\n': /* fallthrough */
		case ':':
			if (!buf || !*buf) {
				eol = 1;
				break;
			}
			buf[len] = '\0';
			if (store_extension_line(buf, len) == EXIT_SUCCESS)
				*buf = '\0';
//			else {
			if (!*p)
				eol = 1;
			len = 0;
			p++;
//			}
			break;

		default:
			buf = (char *)xrealloc(buf, (len + 2) * sizeof(char));
			buf[len] = *p;
			len++;
			p++;
			break;
		}
	}

	p = (char *)NULL;

	free(buf);

	if (ext_colors) {
		ext_colors = (char **)xrealloc(ext_colors, (ext_colors_n + 1) * sizeof(char *));
		ext_colors[ext_colors_n] = (char *)NULL;
	}

	/* Make sure we have valid color codes and store the length
	 * of each stored extension: this length will be used later
	 * when listing files */
	ext_colors_len = (size_t *)xnmalloc(ext_colors_n, sizeof(size_t));

	int i = (int)ext_colors_n;
	while (--i >= 0) {
		char *ret = strrchr(ext_colors[i], '=');
		if (!ret || !*(++ret) || !is_color_code(ret)) {
			*ext_colors[i] = '\0';
			ext_colors_len[i] = 0;
			continue;
		}

		size_t j, ext_len = 0;
		for (j = 2; ext_colors[i][j] && ext_colors[i][j] != '='; j++)
			ext_len++;
		ext_colors_len[i] = ext_len;
	}
}

static void
split_iface_colors(char *ifacecolors)
{
	char *p = ifacecolors, *buf = (char *)NULL,
	     **colors = (char **)NULL;
	size_t len = 0, words = 0;
	int eol = 0;

	while (!eol) {
		switch (*p) {

		case '\0': /* fallthrough */
		case '\n': /* fallthrough */
		case ':':
			if (!buf)
				break;
			buf[len] = '\0';
			colors = (char **)xrealloc(colors, (words + 1) * sizeof(char *));
			colors[words] = savestring(buf, len);
			words++;
			*buf = '\0';

			if (!*p)
				eol = 1;

			len = 0;
			p++;
			break;

		default:
			buf = (char *)xrealloc(buf, (len + 2) * sizeof(char));
			buf[len] = *p;
			len++;
			p++;
			break;
		}
	}

	p = (char *)NULL;

	free(buf);

	if (colors) {
		colors = (char **)xrealloc(colors, (words + 1) * sizeof(char *));
		colors[words] = (char *)NULL;
	}

	set_iface_colors(colors, words);
	free(colors);
}

static void
split_filetype_colors(char *filecolors)
{
	/* Split the colors line into substrings (one per color) */
	char *p = filecolors, *buf = (char *)NULL, **colors = (char **)NULL;
	size_t len = 0, words = 0;
	int eol = 0;

	while (!eol) {
		switch (*p) {

		case '\0': /* fallthrough */
		case '\n': /* fallthrough */
		case ':':
			if (!buf)
				break;
			buf[len] = '\0';
			colors = (char **)xrealloc(colors, (words + 1) * sizeof(char *));
			colors[words] = savestring(buf, len);
			words++;
			*buf = '\0';

			if (!*p)
				eol = 1;

			len = 0;
			p++;
			break;

		default:
			buf = (char *)xrealloc(buf, (len + 2) * sizeof(char));
			buf[len] = *p;
			len++;
			p++;
			break;
		}
	}

	p = (char *)NULL;

	free(buf);

	if (colors) {
		colors = (char **)xrealloc(colors, (words + 1) * sizeof(char *));
		colors[words] = (char *)NULL;
	}

	/* Set the file type color variables */
	set_filetype_colors(colors, words);
	free(colors);
}

/* Open the config file, get values for file type and extension colors
 * and copy these values into the corresponding variable. If some value
 * is not found, or if it's a wrong value, the default is set. */
int
set_colors(const char *colorscheme, const int env)
{
	char *filecolors = (char *)NULL,
		 *extcolors = (char *)NULL,
	     *ifacecolors = (char *)NULL;

#ifndef _NO_ICONS
	*dir_ico_c = '\0';
#endif

	int ret = EXIT_SUCCESS;
	if (colorscheme && *colorscheme && color_schemes)
		ret = get_cur_colorscheme(colorscheme);

	/* env is true only when the function is called from main() */
	if (ret == EXIT_SUCCESS && env)
		get_colors_from_env(&filecolors, &extcolors, &ifacecolors);

#ifndef CLIFM_SUCKLESS
	if (ret == EXIT_SUCCESS && xargs.stealth_mode != 1
	&& (!filecolors || !extcolors || !ifacecolors)) {
		/* Get color lines, for both file types and extensions, from
		 * COLORSCHEME file */
		if (get_colors_from_file(colorscheme, &filecolors,
		&extcolors, &ifacecolors, env) == EXIT_FAILURE) {
			clear_defs();
			return EXIT_FAILURE;
		}
	}
#endif /* CLIFM_SUCKLESS */
	/* Split the color lines into substrings (one per color) */

	if (!extcolors) {
		/* Unload current extension colors */
		if (ext_colors_n)
			free_extension_colors();
	} else {
		split_extensions_colors(extcolors);
		free(extcolors);
	}

	if (!ifacecolors) {
		reset_iface_colors();
	} else {
		split_iface_colors(ifacecolors);
		free(ifacecolors);
	}

	if (!filecolors) {
		reset_filetype_colors();
	} else {
		split_filetype_colors(filecolors);
		free(filecolors);
	}
#ifndef CLIFM_SUCKLESS
	clear_defs();
#endif

	/* If some color is unset or it's a wrong color code, set the default */
	set_default_colors();

	return EXIT_SUCCESS;
}

/* Print ENTRY using color codes and ELN as ELN, right padding PAD
 * chars and terminating ENTRY with or without a new line char (NEW_LINE
 * 1 or 0 respectivelly)
 * ELN could be:
 * > 0: The ELN of a file in CWD
 * -1: Error getting ELN
 * 0: ELN should not be printed, for example, when listing files not in CWD */
void
colors_list(char *ent, const int eln, const int pad, const int new_line)
{
	char index[sizeof(int) + 8];
	*index = '\0';

	if (eln > 0)
		sprintf(index, "%d ", eln); /* NOLINT */
	else if (eln == -1)
		sprintf(index, "? "); /* NOLINT */
	else
		index[0] = '\0';

	struct stat attr;
	char *p = ent, *q = ent, t[PATH_MAX];
	char *eln_color = *index == '?' ? mi_c : el_c;

	if (*q == '~') {
		if (!*(q + 1) || (*(q + 1) == '/' && !*(q + 2)))
			xstrsncpy(t, user.home, PATH_MAX - 1);
		else
			snprintf(t, PATH_MAX, "%s/%s", user.home, q + 2);
		p = t;
	}

	size_t len = strlen(p);
	int rem_slash = 0;
	/* Remove the ending slash: lstat() won't take a symlink to dir as
	 * a symlink (but as a dir), if the file name ends with a slash */
	if (len > 1 && p[len - 1] == '/') {
		p[len - 1] = '\0';
		rem_slash = 1;
	}

	int ret = lstat(p, &attr);
	if (rem_slash)
		p[len - 1] = '/';

	char *wname = (char *)NULL;
	size_t wlen = wc_xstrlen(ent);
	if (wlen == 0)
		wname = truncate_wname(ent);

	if (ret == -1) {
		fprintf(stderr, "%s%s%s%s%-*s%s%s", eln_color, index, df_c,
		    uf_c, pad, wname ? wname : ent, df_c, new_line ? "\n" : "");
		free(wname);
		return;
	}

	char *color = fi_c, ext_color[MAX_COLOR];

	switch (attr.st_mode & S_IFMT) {
	case S_IFREG:
		if (light_mode == 1 || colorize == 0) {
			color = fi_c;
		} else if (check_file_access(&attr) == 0) {
			color = nf_c;
		} else {
			color = get_file_color(ent, &attr);
			if (light_mode == 1 || color != fi_c)
				break;
			char *ext = check_ext == 1 ? strrchr(ent, '.') : (char *)NULL;
			if (!ext) break;
			char *extcolor = get_ext_color(ext);
			ext = (char *)NULL;
			if (!extcolor) break;

			snprintf(ext_color, MAX_COLOR, "\x1b[%sm", extcolor); // NOLINT
			color = ext_color;
			extcolor = (char *)NULL;
		}
		break;

	case S_IFDIR:
		if (light_mode == 1 || colorize == 0)
			color = di_c;
		else if (check_file_access(&attr) == 0)
			color = nd_c;
		else
			color = get_dir_color(ent, attr.st_mode, attr.st_nlink);
		break;

	case S_IFLNK: {
		char *linkname = realpath(ent, NULL);
		color = linkname ? ln_c : or_c;
		free(linkname);
		}
		break;

	case S_IFIFO: color = pi_c; break;
	case S_IFBLK: color = bd_c; break;
	case S_IFCHR: color = cd_c; break;
	case S_IFSOCK: color = so_c; break;
	default: color = no_c; break;
	}

	char *name = wname ? wname : ent;
	char *tmp = (flags & IN_SELBOX_SCREEN) ? abbreviate_file_name(name) : name;

	printf("%s%s%s%s%s%s%s%-*s", eln_color, index, df_c, color,
		tmp + tab_offset, df_c, new_line ? "\n" : "", pad, "");
	free(wname);

	if ((flags & IN_SELBOX_SCREEN) && tmp != name)
		free(tmp);
}

#ifndef CLIFM_SUCKLESS
size_t
get_colorschemes(void)
{
	struct stat attr;
	int schemes_total = 0;
	struct dirent *ent;
	DIR *dir_p;
	size_t i = 0;

	if (colors_dir && stat(colors_dir, &attr) == EXIT_SUCCESS) {
		schemes_total = count_dir(colors_dir, NO_CPOP) - 2;
		if (schemes_total) {
			if (!(dir_p = opendir(colors_dir))) {
				_err('e', PRINT_PROMPT, "opendir: %s: %s\n", colors_dir, strerror(errno));
				return 0;
			}

			color_schemes = (char **)xrealloc(color_schemes,
							((size_t)schemes_total + 2) * sizeof(char *));

			while ((ent = readdir(dir_p)) != NULL) {
				/* Skipp . and .. */
				char *name = ent->d_name;
				if (*name == '.' && (!name[1] || (name[1] == '.' && !name[2])))
					continue;

				char *ret = strchr(name, '.');
				/* If the file contains not dot, or if its extension is not
				 * .clifm, or if it's just a hidden file named ".clifm", skip it */
//				if (!ret || strcmp(ret, ".cfm") != 0 || ret == name)
				if (!ret || strcmp(ret, ".clifm") != 0 || ret == name)
					continue;

				*ret = '\0';
				color_schemes[i] = savestring(name, strlen(name));
				i++;
			}

			closedir(dir_p);
			color_schemes[i] = (char *)NULL;
		}
	}

	if (!data_dir)
		return i;

	char sys_colors_dir[PATH_MAX];
	snprintf(sys_colors_dir, PATH_MAX - 1, "%s/%s/colors", data_dir, PNL); /* NOLINT */

	if (stat(sys_colors_dir, &attr) == -1)
		return i;

	int total_tmp = schemes_total;
	schemes_total += (count_dir(sys_colors_dir, NO_CPOP) - 2);

	if (schemes_total <= total_tmp)
		return i;

	if (!(dir_p = opendir(sys_colors_dir))) {
		_err('e', PRINT_PROMPT, "opendir: %s: %s\n", sys_colors_dir, strerror(errno));
		return i;
	}

	color_schemes = (char **)xrealloc(color_schemes,
					((size_t)schemes_total + 2) * sizeof(char *));

	size_t i_tmp = i;

	while ((ent = readdir(dir_p)) != NULL) {
		/* Skipp . and .. */
		char *name = ent->d_name;
		if (*name == '.' && (!name[1] || (name[1] == '.' && !name[2])))
			continue;

		char *ret = strchr(name, '.');
		/* If the file contains not dot, or if its extension is not
		 * .clifm, or if it's just a hidden file named ".clifm", skip it */
//		if (!ret || ret == name || strcmp(ret, ".cfm") != 0)
		if (!ret || ret == name || strcmp(ret, ".clifm") != 0)
			continue;

		*ret = '\0';

		size_t j;
		int dup = 0;
		for (j = 0; j < i_tmp; j++) {
			if (*color_schemes[j] == *name && strcmp(name, color_schemes[j]) == 0) {
				dup = 1;
				break;
			}
		}

		if (dup == 1)
			continue;

		color_schemes[i] = savestring(name, strlen(name));
		i++;
	}

	closedir(dir_p);
	color_schemes[i] = (char *)NULL;
	return i;
}
#endif /* CLIFM_SUCKLESS */

static void
print_color_blocks(void)
{
	printf(NO_LINE_WRAP); /* Disable line wrap */

	int pad = (term_cols - 24) / 2;
	printf("\x1b[%dC\x1b[0;40m   \x1b[0m\x1b[0;41m   \x1b[0m\x1b[0;42m   "
		"\x1b[0m\x1b[0;43m   \x1b[0m\x1b[0;44m   \x1b[0m\x1b[0;45m   "
		"\x1b[0m\x1b[0;46m   \x1b[0m\x1b[0;47m   \x1b[0m\n"
		"\x1b[%dC\x1b[0m\x1b[0;100m   \x1b[0m\x1b[0;101m   "
		"\x1b[0m\x1b[0;102m   \x1b[0m\x1b[0;103m   \x1b[0m\x1b[0;104m   "
		"\x1b[0m\x1b[0;105m   \x1b[0m\x1b[0;106m   \x1b[0m\x1b[0;107m   "
		"\x1b[0m\n", pad, pad);

	printf(LINE_WRAP); /* Reenable line wrap */
}

/* List color codes for file types used by the program */
void
color_codes(void)
{
	if (colorize == 0) {
		printf(_("%s: Currently running without colors\n"), PROGRAM_NAME);
		return;
	}

	if (ext_colors_n > 0)
		printf(_("%sFile type colors%s\n\n"), BOLD, df_c);
	printf(_(" %sfile name%s: nd: Directory with no read permission\n"),
	    nd_c, df_c);
	printf(_(" %sfile name%s: nf: File with no read permission\n"),
	    nf_c, df_c);
	printf(_(" %sfile name%s: di: Directory*\n"), di_c, df_c);
	printf(_(" %sfile name%s: ed: EMPTY directory\n"), ed_c, df_c);
	printf(_(" %sfile name%s: ne: EMPTY directory with no read "
		 "permission\n"),
	    ne_c, df_c);
	printf(_(" %sfile name%s: ex: Executable file\n"), ex_c, df_c);
	printf(_(" %sfile name%s: ee: Empty executable file\n"), ee_c, df_c);
	printf(_(" %sfile name%s: bd: Block special file\n"), bd_c, df_c);
	printf(_(" %sfile name%s: ln: Symbolic link*\n"), ln_c, df_c);
	printf(_(" %sfile name%s: or: Broken symbolic link\n"), or_c, df_c);
	printf(_(" %sfile name%s: mh: Multi-hardlink\n"), mh_c, df_c);
	printf(_(" %sfile name%s: so: Socket file\n"), so_c, df_c);
	printf(_(" %sfile name%s: pi: Pipe or FIFO special file\n"), pi_c, df_c);
	printf(_(" %sfile name%s: cd: Character special file\n"), cd_c, df_c);
	printf(_(" %sfile name%s: fi: Regular file\n"), fi_c, df_c);
	printf(_(" %sfile name%s: ef: Empty (zero-lenght) file\n"), ef_c, df_c);
	printf(_(" %sfile name%s: su: SUID file\n"), su_c, df_c);
	printf(_(" %sfile name%s: sg: SGID file\n"), sg_c, df_c);
	printf(_(" %sfile name%s: ca: File with capabilities\n"), ca_c, df_c);
	printf(_(" %sfile name%s: st: Sticky and NOT other-writable "
		 "directory*\n"),
	    st_c, df_c);
	printf(_(" %sfile name%s: tw: Sticky and other-writable "
		 "directory*\n"),
	    tw_c, df_c);
	printf(_(" %sfile name%s: ow: Other-writable and NOT sticky "
		 "directory*\n"),
	    ow_c, df_c);
	printf(_(" %sfile name%s: no: Unknown file type\n"), no_c, df_c);
	printf(_(" %sfile name%s: uf: Unaccessible (non-stat'able) file\n"), uf_c, df_c);

	printf(_("\n*The slash followed by a number (/xx) after directories "
		 "or symbolic links to directories indicates the amount of "
		 "files contained by the corresponding directory, excluding "
		 "self (.) and parent (..) directories.\n"));
	printf(_("\nThe second field in this list is the code that is to be used "
		 "to modify the color of the corresponding file type in the "
		 "color scheme file (in the \"FiletypeColors\" line), "
		 "using the same ANSI style color format used by dircolors. "
		 "By default, %s uses only 8/16 colors, but you can use 256 "
		 "and RGB/true colors as well.\n\n"), PROGRAM_NAME);

	if (ext_colors_n > 0) {
		size_t i, j;
		printf(_("%sExtension colors%s\n\n"), BOLD, df_c);
		for (i = 0; i < ext_colors_n; i++) {
			char *ret = strrchr(ext_colors[i], '=');
			if (!ret)
				continue;
			printf(" \x1b[%sm", ret + 1);
			for (j = 0; ext_colors[i][j] != '='; j++)
				putchar(ext_colors[i][j]);
			puts(NC);
		}
		putchar('\n');
	}

	print_color_blocks();
}
