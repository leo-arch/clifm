/* tabcomp.c -- functions for TAB completion */

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
#include <unistd.h>
#include <sys/stat.h>

#ifdef __OpenBSD__
typedef char *rl_cpvfunc_t;
#include <ereadline/readline/readline.h>
#else
#include <readline/readline.h>
#endif
#include <errno.h>
#include <fcntl.h>

#include "exec.h"
#include "aux.h"
#include "misc.h"
#include "checks.h"
#include "strings.h"
#include "colors.h"
#include "navigation.h"
#include "readline.h"

#ifndef _NO_HIGHLIGHT
#include "highlight.h"
#endif

#ifndef _NO_SUGGESTIONS
#include "suggestions.h"
#endif

#define PUTX(c) \
	if (CTRL_CHAR(c)) { \
          putc('^', rl_outstream); \
          putc(UNCTRL(c), rl_outstream); \
	} else if (c == RUBOUT) { \
		putc('^', rl_outstream); \
		putc('?', rl_outstream); \
	} else \
		putc(c, rl_outstream)

/* Return the character which best describes FILENAME.
`@' for symbolic links
`/' for directories
`*' for executables
`=' for sockets */
static int
stat_char(char *filename)
{
	struct stat attr;
	int r;

#if defined(S_ISLNK)
	r = lstat(filename, &attr);
#else
	r = stat(filename, &attr);
#endif

	if (r == -1)
		return 0;

	int c = 0;
	if (S_ISDIR(attr.st_mode)) {
		c = '/';
#if defined(S_ISLNK)
	} else if (S_ISLNK(attr.st_mode)) {
		c = '@';
#endif /* S_ISLNK */
#if defined(S_ISSOCK)
	} else if (S_ISSOCK(attr.st_mode)) {
		c = '=';
#endif /* S_ISSOCK */
	} else if (S_ISREG(attr.st_mode)) {
		if (access(filename, X_OK) == 0)
			c = '*';
#if defined(S_ISFIFO)
	} else {
		if (S_ISFIFO(attr.st_mode))
			c = '|';
#endif
	}

	return c;
}

/* The user must press "y" or "n". Non-zero return means "y" pressed. */
static int
get_y_or_n(void)
{
	for (;;) {
		int c = fgetc(stdin);
		if (c == 'y' || c == 'Y' || c == ' ')
			return (1);
		 if (c == 'n' || c == 'N' || c == RUBOUT || c == EOF) {
			putchar('\n');
			return (0);
		}
		if (c == ABORT_CHAR) /* defined by readline as CTRL('G') */
			rl_abort(0, 0);
		rl_ding();
	}
}

static int
print_filename(char *to_print, char *full_pathname)
{
	char *s;

	if (colorize == 1 && (cur_comp_type == TCMP_PATH || cur_comp_type == TCMP_SEL
	|| cur_comp_type == TCMP_DESEL || cur_comp_type == TCMP_RANGES)) {
		colors_list(to_print, NO_ELN, NO_PAD, NO_NEWLINE);
	} else {
		for (s = to_print + tab_offset; *s; s++) {
			PUTX(*s);
		}
	}

	if (rl_filename_completion_desired && colorize == 0) {
		if (cur_comp_type == TCMP_CMD) {
			putc('*', rl_outstream);
			return 1;
		}
      /* If to_print != full_pathname, to_print is the basename of the
	 path passed. In this case, we try to expand the directory
	 name before checking for the stat character */
		int extension_char = 0;
		if (to_print != full_pathname) {
			/* Terminate the directory name */
			char c = to_print[-1];
			to_print[-1] = '\0';

			s = tilde_expand(full_pathname);
			if (rl_directory_completion_hook)
				(*rl_directory_completion_hook) (&s);

			size_t slen = strlen(s);
			size_t tlen = strlen(to_print);
			char *new_full_pathname = (char *)xnmalloc(slen + tlen + 2, sizeof(char));
			strcpy(new_full_pathname, s);
			new_full_pathname[slen] = '/';
			strcpy(new_full_pathname + slen + 1, to_print);

			extension_char = stat_char(new_full_pathname);

			free(new_full_pathname);
			to_print[-1] = c;
		} else {
			s = tilde_expand(full_pathname);
			extension_char = stat_char(s);
		}

		free(s);
		if (extension_char)
			putc(extension_char, rl_outstream);
		return (extension_char != 0);
	} else {
		return 0;
	}
}

/* Return the portion of PATHNAME that should be output when listing
   possible completions. If we are hacking filename completion, we
   are only interested in the basename, the portion following the
   final slash. Otherwise, we return what we were passed. */
static char *
printable_part(char *pathname)
{
	char *temp = (char *)NULL;

	if (rl_filename_completion_desired)
		temp = strrchr(pathname, '/');

	if (!temp)
		return (pathname);
	else
		return (++temp);
}

/* Find the first occurrence in STRING1 of any character from STRING2.
   Return a pointer to the character in STRING1. */
static char *
rl_strpbrk(char *s1, char *s2)
{
	register char *scan;

	for (; *s1; s1++) {
		for (scan = s2; *scan; scan++) {
			if (*s1 == *scan) {
				return (s1);
			}
		}
	}
	return (char *)NULL;
}

void
reinsert_slashes(char *str)
{
	if (!str || !*str)
		return;

	char *p = str;
	while (*p) {
		if (*p == ':')
			*p = '/';
		p++;
	}
}

#ifndef _NO_FZF

static char *
fzftab_color(char *filename, const struct stat *attr)
{
	if (!colorize)
		return df_c;

	switch(attr->st_mode & S_IFMT) {
	case S_IFDIR:
		if (!check_file_access(attr))
			return nd_c;
		return get_dir_color(filename, attr->st_mode, attr->st_nlink);
	case S_IFREG:
		if (!check_file_access(attr))
			return nf_c;
		char *ext_cl = (char *)NULL;
		char *ext = check_ext == 1 ? strrchr(filename, '.') : (char *)NULL;
		if (ext && ext != filename)
			ext_cl = get_ext_color(ext);
		if (ext_cl)
			return ext_cl;
		return get_file_color(filename, attr);
	case S_IFSOCK: return so_c;
	case S_IFIFO: return pi_c;
	case S_IFBLK: return bd_c;
	case S_IFCHR: return cd_c;
	case S_IFLNK: return ln_c;
	default: return uf_c;
	}
}

static char *
get_entry_color(char **matches, const size_t i)
{
	if (colorize == 0)
		return (char *)NULL;

	struct stat attr;

	/* Normalize URI file scheme */
	char *dir = matches[i];
	size_t dlen = strlen(dir);
	if (dlen > FILE_URI_PREFIX_LEN && IS_FILE_URI(dir))
		dir += FILE_URI_PREFIX_LEN;

	/* Absolute path (/FILE) or file in CWD (./FILE) */
	if ( (*dir == '/' || (*dir == '.' && *(dir + 1) == '/') ) && (cur_comp_type == TCMP_PATH
	|| cur_comp_type == TCMP_SEL || cur_comp_type == TCMP_DESEL
	|| cur_comp_type == TCMP_BM_PATHS) ) {
		if (lstat(dir, &attr) != -1)
			return fzftab_color(dir, &attr);
		return uf_c;
	}

	/* Tilde */
	if (*dir == '~' && (cur_comp_type == TCMP_PATH || cur_comp_type == TCMP_BM_PATHS
	|| cur_comp_type == TCMP_SEL || cur_comp_type == TCMP_DESEL) ) {
		char *exp_path = tilde_expand(matches[i]);
		if (exp_path) {
			char tmp_path[PATH_MAX + 1];
			xstrsncpy(tmp_path, exp_path, PATH_MAX);
			free(exp_path);
			if (lstat(tmp_path, &attr) != -1)
				return fzftab_color(tmp_path, &attr);
			return uf_c;
		}
	}

	if (cur_comp_type == TCMP_PATH || cur_comp_type == TCMP_RANGES) {
		char tmp_path[PATH_MAX];
		snprintf(tmp_path, sizeof(tmp_path), "%s/%s", workspaces[cur_ws].path, dir);
		if (lstat(tmp_path, &attr) != -1)
			return fzftab_color(tmp_path, &attr);
		return uf_c;
	}

	if (cur_comp_type == TCMP_CMD && is_internal_c(dir))
		return hv_c;

	return df_c;
}

static void
write_completion(char *buf, const size_t *offset, int *exit_status, const int multi)
{
	/* Remove ending new line char */
	char *n = strchr(buf, '\n');
	if (n)
		*n = '\0';

	if (cur_comp_type == TCMP_GLOB) {
		size_t blen = strlen(buf);
		if (blen > 0 && buf[blen - 1] == '/')
			buf[blen - 1] = '\0';
		if (*rl_line_buffer == '/' && rl_end > 0
		&& !strchr(rl_line_buffer + 1, '/') && !strchr(rl_line_buffer + 1, ' ')) {
			rl_delete_text(0, rl_end);
			rl_end = rl_point = 0;
		}
	}

	if (cur_comp_type == TCMP_ENVIRON || cur_comp_type == TCMP_USERS)
		/* Skip the leading dollar sign (env vars) and tilde (users) */
		buf++;

	if (cur_comp_type == TCMP_PATH && multi == 0) {
		char *esc_buf = escape_str(buf);
		if (esc_buf) {
			rl_insert_text(esc_buf + *offset);
			free(esc_buf);
		} else {
			rl_insert_text(buf + *offset);
		}
	} else if (cur_comp_type == TCMP_FILE_TYPES_OPTS) {
		rl_insert_text(buf);
		return;
	} else {
		if (autocd == 0 && cur_comp_type == TCMP_JUMP)
			rl_insert_text("cd ");
		rl_insert_text(buf + *offset);
	}

	/* Append slash for dirs and space for non-dirs */
	char *pp = rl_line_buffer;
	char *ss = (char *)NULL;
	if (pp) {
		while (*pp) {
			if (pp == rl_line_buffer) {
				pp++;
				continue;
			}

			if (*pp == ' ' && *(pp - 1) != '\\' && *(pp + 1) != ' ')
				ss = pp + 1;

			pp++;
		}
	}

	if (!ss || !*ss)
		ss = rl_line_buffer;
	if (!ss)
		return;

	char deq_str[PATH_MAX];
	*deq_str = '\0';
	/* Clang static analysis complains that tmp[4] (deq_str[4]) is a garbage
	 * value. Initialize only this exact value to get rid of the warning */
	deq_str[4] = '\0';
	if (strchr(ss, '\\')) {
		size_t i = 0;
		char *b = ss;
		while (*b && i < (PATH_MAX - 1)) {
			if (*b != '\\') {
				deq_str[i] = *b;
				i++;
			}
			b++;
		}
		deq_str[i] = '\0';
	}

	char _path[PATH_MAX + NAME_MAX];
	*_path = '\0';
	char *tmp = *deq_str ? deq_str : ss;

	size_t dlen = strlen(tmp), is_file_uri = 0;
	if (*tmp == 'f' && *(tmp + 1) == 'i' && dlen > FILE_URI_PREFIX_LEN
	&& IS_FILE_URI(tmp))
		is_file_uri = 1;

	if (is_file_uri == 0 && *tmp != '/' && *tmp != '.' && *tmp != '~') {
		if (*(workspaces[cur_ws].path + 1))
			snprintf(_path, PATH_MAX + NAME_MAX, "%s/%s", workspaces[cur_ws].path, tmp);
		else /* Root directory */
			snprintf(_path, PATH_MAX + NAME_MAX, "/%s", tmp);
	}

	char *spath = *_path ? _path : tmp;
	char *epath = (char *)NULL;
	if (*spath == '~')
		epath = tilde_expand(spath);
	else {
		if (*spath == '.') {
			xchdir(workspaces[cur_ws].path, NO_TITLE);
			epath = realpath(spath, NULL);
			/* No need to change back to CWD. Done here */
			*exit_status = -1;
		}
	}

	if (epath)
		spath = epath;

	char *d = spath;
	if (is_file_uri == 1)
		d += FILE_URI_PREFIX_LEN;

	struct stat attr;
	if (stat(d, &attr) != -1 && S_ISDIR(attr.st_mode)) {
		/* If not the root directory, append a slash */
		if ((*d != '/' || *(d + 1) || cur_comp_type == TCMP_USERS))
			rl_insert_text("/");
	} else {
		if (rl_end == rl_point && cur_comp_type != TCMP_OPENWITH
		&& cur_comp_type != TCMP_TAGS_T && cur_comp_type != TCMP_FILE_TYPES_OPTS)
			rl_stuff_char(' ');
	}

	free(epath);
}

/* Get word after last non-escaped slash */
static char *
get_last_word(char *str)
{
	char *sl = str;
	char *d = (char *)NULL;
	while (*sl) {
		if (sl == str) {
			if (*sl == '/')
				d = sl;
		} else {
			if (*sl == '/' && *(sl - 1) != '\\')
				d = sl;
		}
		sl++;
	}

	if (!d) {
		return str;
	} else {
		if (*d == '/')
			return d + 1;
		return d;
	}
}

static int
run_finder(const size_t *height, const int *offset, const char *lw, const int multi)
{
	/* If height was not set in FZF_DEFAULT_OPTS nor in the config
	 * file, let's define it ourselves */
	char height_str[sizeof(size_t) + 21];
	*height_str = '\0';
	if (fzf_height_set == 0)
		snprintf(height_str, sizeof(height_str), "--height=%zu", *height);

	char cmd[(PATH_MAX * 2) + NAME_MAX];
	if (tabmode == FZY_TAB) {
		snprintf(cmd, sizeof(cmd), "fzy "
				"--read-null --pad=%d --query=\"%s\" --reverse "
				"--tab-accepts --right-accepts --left-aborts "
				"--lines=%zu %s %s < %s > %s",
				*offset, lw ? lw : "", *height,
				colorize == 0 ? "--no-color" : "",
				multi ? "--multi" : "",
				finder_in_file, finder_out_file);
	} else if (tabmode == SMENU_TAB) {
		snprintf(cmd, sizeof(cmd), "smenu %s "
				"-t -d -n%zu -limits l:%d -W$'\n' %s < %s > %s",
				smenutab_options_env ? smenutab_options_env : DEF_SMENU_OPTIONS,
				*height, PATH_MAX, multi ? "-P$'\n'" : "",
				finder_in_file, finder_out_file);
	} else {
		snprintf(cmd, sizeof(cmd), "fzf %s "
				"%s --margin=0,0,0,%d "
				"%s --read0 --ansi "
				"--query=\"%s\" %s %s "
				"< %s > %s",
				fzftab_options,
				*height_str ? height_str : "", *offset,
				case_sens_path_comp ? "+i" : "-i",
				lw ? lw : "", colorize == 0 ? "--no-color" : "",
				multi ? "--multi --bind tab:toggle+down,ctrl-s:select-all,\
ctrl-d:deselect-all,ctrl-t:toggle-all" : "",
				finder_in_file, finder_out_file);
	}

/*		snprintf(cmd, sizeof(cmd), "sk " // skim
				"%s --margin=0,0,0,%d --color=16 "
				"--read0 --ansi --inline-info "
				"--layout=reverse-list --query=\"%s\" %s %s "
				"< %s > %s",
				*height_str ? height_str : "", *offset,
				lw ? lw : "", colorize == 0 ? "--no-color" : "",
				multi ? "--multi --bind tab:toggle+down" : "",
				finder_in_file, finder_out_file); */

	int dr = (flags & DELAYED_REFRESH) ? 1 : 0;
	flags &= ~DELAYED_REFRESH;
	int ret = launch_execle(cmd);
	if (dr == 1) flags |= DELAYED_REFRESH;
	return ret;
}

/* Set FZF window's max height. No more than MAX HEIGHT entries will
 * be listed at once. */
static inline size_t
set_fzf_max_win_height(void)
{
	return (size_t)(DEF_FZF_WIN_HEIGHT * term_rows / 100);
}

/* FILENAME is just a symlink name from the tags dir.
 * Let's get the target path */
static char *
get_tagged_file_target(char *filename)
{
	if (!filename || !*filename)
		return (char *)NULL;

	char dir[PATH_MAX];
	char *p = (char *)NULL;
	if (strchr(filename, '\\'))
		p = dequote_str(filename, 0);

	snprintf(dir, PATH_MAX, "%s/%s/%s", tags_dir, cur_tag, p ? p : filename);
	free(p);

	char *rpath = realpath(dir, NULL);
	char *s = rpath ? rpath : filename;
	int free_tmp = 0;
	char *q = home_tilde(s, &free_tmp);
	if (q && free_tmp == 1)
		free(s);

	return q ? q : s;
}

static char *
print_no_finder_file(void)
{
	_err('e', PRINT_PROMPT, "%s: %s: %s\n", PROGRAM_NAME,
		finder_out_file, strerror(errno));
	return (char *)NULL;
}

/* If we are completing a path whose last component is a glob expression,
 * return the selected match for this expression (STR) preceded by
 * the initial portion of the path (everything before the glob expression):
 * INITIAL_PATH. We need to do this because, in case of PATH/GLOB, glob(3)
 * does not return the full path, but only the expanded glob expression
 * Ex (underscore is an asterisk):
 * downloads/_.pdf<TAB> -> downloads/file.pdf
 * _.pdf<TAB> -> file.pdf */
static char *
get_glob_file_target(char *str, char *initial_path)
{
	if (!str || !*str)
		return (char *)NULL;

	if (*str == '/' || !initial_path)
		return str;

	char *p = (char *)xnmalloc(strlen(initial_path) + strlen(str) + 1, sizeof(char));
	sprintf(p, "%s%s", initial_path, str);

	return p;
}

/* Given PATH/GLOB (last word of the line buffer) return PATH/, in which case
 * the return value should be freed by the caller. If not PATH/GLOB, return NULL
 * Ex (underscore is an asterisk):
 * documents/misc/_.c -> documents/misc/ */
static char *
get_initial_path(void)
{
	char *lp = rl_line_buffer ? strrchr(rl_line_buffer, ' ') : (char *)NULL;
	char *ls = (lp && *(++lp)) ? strrchr(lp, '/') : (char *)NULL;

	if (!ls || !*(ls + 1) || check_glob_char(ls, GLOB_ONLY) == 0)
		ls = (char *)NULL;

	if (!ls)
		return (char *)NULL;

	++ls;
	char bk = *ls;
	*ls = '\0';
	char *p = savestring(lp, strlen(lp));
	*ls = bk;
	return p;
}

/* Recover finder (fzf/fzy) output from FINDER_OUT_FILE file
 * Return this output (reformated if needed) or NULL in case of error */
static char *
get_finder_output(const int multi)
{
	FILE *fp = fopen(finder_out_file, "r");
	if (!fp)
		return print_no_finder_file();

	char *buf = (char *)xnmalloc(1, sizeof(char)), *line = (char *)NULL;
	*buf = '\0';
	size_t bsize = 0, line_size = 0;
	ssize_t line_len = 0;
	char *initial_path = (cur_comp_type == TCMP_GLOB) ? get_initial_path() : (char *)NULL;

	while ((line_len = getline(&line, &line_size, fp)) > 0) {
		if (line[line_len - 1] == '\n')
			line[--line_len] = '\0';

		char *q = line;
		if (multi == 1) {
			char *s = line;
			if (cur_comp_type == TCMP_GLOB)
				s = get_glob_file_target(line, initial_path);
			else if (cur_comp_type == TCMP_TAGS_F && tags_dir && cur_tag)
				s = get_tagged_file_target(line);
			q = escape_str(s);
			if (s != line)
				free(s);
			if (!q)
				continue;
		}

		/* We don't want to quote the initial tilde */
		char *r = q;
		if (*r == '\\' && *(r + 1) == '~')
			r++;

		size_t qlen = (r != line) ? strlen(r) : (size_t)line_len;
		bsize += qlen + 3;
		buf = (char *)xrealloc(buf, bsize * sizeof(char));
		strcat(buf, r);

		if (multi == 1) {
			size_t l = strlen(buf);
			buf[l] = ' ';
			buf[l + 1] = '\0';
			free(q);
		}
	}

	free(initial_path);
	free(line);
	fclose(fp);
	unlink(finder_out_file);

	if (*buf == '\0') {
		free(buf);
		buf = (char *)NULL;
	}

	return buf;
}

static void
write_comp_to_file(char *entry, const char *color, FILE **fp)
{
	char *c = (char *)NULL, tmp[MAX_COLOR + 4];
	if (cur_comp_type == TCMP_TAGS_F || cur_comp_type == TCMP_GLOB
	|| cur_comp_type == TCMP_FILE_TYPES_FILES) {
		size_t len = strlen(entry);
		if (len > 1 && entry[len - 1] == '/')
			entry[len - 1] = '\0';
		char *p = (char *)NULL;
		if (*entry == '~')
			p = tilde_expand(entry);
		struct stat a;
		if (lstat(p ? p : entry, &a) != -1) {
			c = fzftab_color(entry, &a);
			if (*c != _ESC) {
				snprintf(tmp, sizeof(tmp), "\x1b[%sm", c);
				c = tmp;
			}
		}
		free(p);
	}

	if (wc_xstrlen(entry) == 0) {
		char *wname = truncate_wname(entry);
		if (tabmode == SMENU_TAB)
			fprintf(*fp, "%s%c", wname ? wname : entry, '\n');
		else
			fprintf(*fp, "%s%c", wname ? wname : entry, '\0');
		free(wname);
		return;
	}

	if (tabmode == SMENU_TAB)
		fprintf(*fp, "%s%s%s%c", c ? c : color, entry, NC, '\n');
	else
		fprintf(*fp, "%s%s%s%c", c ? c : color, entry, NC, '\0');
}

/* Store possible completions (MATCHES) in FINDER_IN_FILE to pass them to the finder,
 * either FZF or FZY
 * Return the number of stored matches */
static size_t
store_completions(char **matches, FILE *fp)
{
	int no_file_comp = 0;
	if (cur_comp_type == TCMP_TAGS_S || cur_comp_type == TCMP_TAGS_U
	|| cur_comp_type == TCMP_SORT || cur_comp_type == TCMP_BOOKMARK
	|| cur_comp_type == TCMP_CSCHEME || cur_comp_type == TCMP_NET
	|| cur_comp_type == TCMP_PROF || cur_comp_type == TCMP_PROMPTS)
		no_file_comp = 1; /* We're not completing file names */

	size_t i;
	char *_path = (char *)NULL;
	for (i = 1; matches[i]; i++) {
		if (!matches[i] || !*matches[i] || SELFORPARENT(matches[i]))
			continue;
		char *color = df_c, *entry = matches[i];

		if (cur_comp_type == TCMP_BACKDIR) {
			color = di_c;
		} else if (cur_comp_type == TCMP_TAGS_T) {
			color = mi_c;
			if (*(entry + 2))
				entry += 2;
		} else if (cur_comp_type == TCMP_TAGS_C) {
			color = mi_c;
			if (*(entry + 1))
				entry += 1;
		} else if (no_file_comp == 1) {
			color = mi_c;
		} else if (cur_comp_type != TCMP_HIST && cur_comp_type != TCMP_JUMP
		&& cur_comp_type != TCMP_TAGS_F && cur_comp_type != TCMP_FILE_TYPES_OPTS) {
			char *cl = get_entry_color(matches, i);
			char ext_cl[MAX_COLOR + 5];
			*ext_cl = '\0';
			/* If color does not start with escape, then we have a color
			 * for a file extension. In this case, we need to properly
			 * construct the color code */
			if (cl && *cl != _ESC)
				snprintf(ext_cl, MAX_COLOR + 4, "\x1b[%sm", cl);

			color = *ext_cl ? ext_cl : (cl ? cl : "");

			if (cur_comp_type != TCMP_SEL && cur_comp_type != TCMP_DESEL
			&& cur_comp_type != TCMP_BM_PATHS
			&& cur_comp_type != TCMP_OPENWITH && cur_comp_type != TCMP_BACKDIR) {
				_path = strrchr(matches[i], '/');
				entry = (_path && *(++_path)) ? _path : matches[i];
			}
		}

		if (*entry)
			write_comp_to_file(entry, color, &fp);
	}

	return i;
}

static char *
get_query_str(int *fzf_offset)
{
	char *query = (char *)NULL;

	switch(cur_comp_type) {
	case TCMP_DESEL: {
		char *sp = strrchr(rl_line_buffer, ' ');
		if (!sp || !*(sp++))
			return (char *)NULL;
		query = sp;
		*fzf_offset = prompt_offset + (int)(sp - rl_line_buffer) - 3;
		}
		break;

	case TCMP_HIST:
		/* Skip the leading ! char of the input string */
		query = rl_line_buffer + 1;
		*fzf_offset = 1 + prompt_offset - 3;
		break;

	case TCMP_JUMP: {
		char *sp = strchr(rl_line_buffer, ' ');
		if (sp && *(++sp)) {
			query = sp;
			if (*(rl_line_buffer + 1) == ' ')
				/* The command is "j" */
				*fzf_offset = 2 + prompt_offset - 3;
			else
				/* The command is "jump" */
				*fzf_offset = 5 + prompt_offset - 3;
		}
		}
		break;

	default: break;
	}

	return query;
}

/* Calculate the length of the matching prefix to insert into the line
 * buffer only the non-matched part of the string returned by FZF */
static size_t
calculate_prefix_len(char *str)
{
	size_t prefix_len = 0, len = strlen(str);

	if (len == 0 || str[len - 1] == '/')
		return prefix_len;

	char *q = strrchr(str, '/');
	if (q) {
		size_t qlen = strlen(q);
		if (cur_comp_type == TCMP_PATH) {
			/* Add backslashes to the len of the match: every quoted char
			 * will be escaped later by write_completion(), so that backslashes
			 * should be counted as well to get the right offset */
			size_t c = 0;
			int x = (int)qlen;
			while (--x >= 0) {
				if (is_quote_char(q[x]))
					c++;
			}
			prefix_len = qlen - 1 + c;
		} else {
			prefix_len = qlen + 1;
		}
	} else { /* We have just a name, no slash */
		if (cur_comp_type == TCMP_PATH) {
			size_t c = 0;
			int x = (int)len;
			while (--x >= 0) {
				if (is_quote_char(str[x]))
					c++;
			}
			prefix_len = len + c;
		} else if (cur_comp_type == TCMP_TAGS_T && len >= 2) {
			prefix_len = len - 2;
		} else if (cur_comp_type == TCMP_TAGS_C) {
			prefix_len = len - 1;
		} else {
			prefix_len = len;
		}
	}

	return prefix_len;
}

/* Let's define whether we have a case which allows multi-selection
 * Returns 1 if true or zero if false */
static int
is_multi_sel(void)
{
	enum comp_type t = cur_comp_type;

	if (t == TCMP_SEL || t == TCMP_DESEL || t == TCMP_RANGES
	|| t == TCMP_TRASHDEL || t == TCMP_UNTRASH || t == TCMP_TAGS_F
	|| t == TCMP_TAGS_U || (flags & MULTI_SEL))
		return 1;

/*	if (t == TCMP_BOOKMARK && (strncmp(rl_line_buffer, "bm del ", 7) == 0
	|| strncmp(rl_line_buffer, "bookmarks del ", 14) == 0))
		return 1; */

	char *l = rl_line_buffer;
	char *lws = strrchr(rl_line_buffer, ' ');

	/* Do not allow multi-sel if we have a path, only file names */
	if (t == TCMP_PATH && *l != '/' && (!lws || !strchr(lws, '/'))) {
		if (
		/* Select */
		(*l == 's' && (l[1] == ' ' || strncmp(l, "sel ", 4) == 0))
		/* Trash */
		|| (*l == 't' && (l[1] == ' ' || (l[1] == 't' && l[2] == ' ')
		|| strncmp(l, "trash ", 6) == 0))
		/* ac and ad */
		|| (*l == 'a' && ((l[1] == 'c' || l[1] == 'd') && l[2] == ' '))
		/* bb, bl, and br */
		|| (*l == 'b' && ((l[1] == 'b' || l[1] == 'l' || l[1] == 'r') && l[2] == ' '))
		/* r */
		|| (*l == 'r' && l[1] == ' ')
		/* d/dup */
		|| (*l == 'd' && (l[1] == ' ' || strncmp(l, "dup ", 4) == 0))
		/* Properties */
		|| (*l == 'p' && (l[1] == ' ' || ((l[1] == 'r' && l[2] == ' ')
		|| strncmp (l, "prop ", 5) == 0)))
		/* te */
		|| (*l == 't' && l[1] == 'e' && l[2] == ' ') ) {
			return 1;
		} else {
			return 0;
		}
	} else {
		return 0;
	}
}

/* Clean the input buffer in case the user cancelled the completion pressing ESC */
/* If all possible completions share a common prefix, this prefix is
 * automatically appended to the query string. However, the user
 * cancelled here the completion (pressing ESC), so that we need to
 * remove this prefix by reinserting the original query string */
static int
clean_rl_buffer(const char *text)
{
	if (!text || !*text)
		return EXIT_FAILURE;

	/* If the previous char is not space, then a common prefix was appended:
	 * remove it */
	if ((rl_end && rl_line_buffer[rl_end - 1] != ' ')
	|| (rl_end >= 2 && rl_line_buffer[rl_end - 2] == '\\')) {
		/* Find the last non-escaped space */
		int i = rl_end, sp = -1;
		while (--i >= 0) {
			if (rl_line_buffer[i] == ' ' && i > 0
			&& rl_line_buffer[i - 1] != '\\') {
				sp = i;
				break;
			}
		}

		if (sp != -1) {
			/* If found, remove the content of the input line starting
			 * exactly one char after the last space */
			rl_delete_text(sp + 1, rl_end);
			rl_point = rl_end = sp + 1;
		} else { /* No space: delete the entire line */
			rl_delete_text(0, rl_end);
			rl_point = rl_end = 0;
		}
		ERASE_TO_RIGHT;
	}

	rl_insert_text(text);

	return EXIT_FAILURE;
}

/* Display possible completions using the corresponding finder. If one of these
 * possible completions is selected, insert it into the current line buffer.
 *
 * What is ORIGINAL_QUERY and why we need it?
 * MATCHES[0] is supposed to hold the common prefix among all possible
 * completions. This common prefix should be the same as the query string when
 * performing regular matches. But it might not be the same as the
 * original query string when performing fuzzy match: then, we need a copy of
 * this original query string (ORIGINAL_QUERY) to later be passed to FZF
 * Example:
 * Query string: '.f'
 * Returned matches (fuzzy):
 *   .file
 *   .this_file
 *   .beef
 * Common suffix: '.' (not '.f')
 * */
static int
finder_tabcomp(char **matches, const char *text, char *original_query)
{
	FILE *fp = fopen(finder_in_file, "w");
	if (!fp) {
		_err('e', PRINT_PROMPT, "%s: %s: %s\n", PROGRAM_NAME,
			finder_in_file, strerror(errno));
		return EXIT_FAILURE;
	}

	int exit_status = EXIT_SUCCESS;

	/* Store possible completions in FINDER_IN_FILE to pass them to FZF */
	size_t i = store_completions(matches, fp);

	fclose(fp);

	/* Set a pointer to the last word (either space or slash) in the
	 * input buffer. We use this to highlight the matching prefix in FZF */
	char *lw = get_last_word(original_query ? original_query : matches[0]);

	/* If not already defined (environment or config file), calculate the
	 * height of the FZF window based on the amount of entries. This
	 * specifies how many entries will be displayed at once */
	size_t height = 0;

	if (fzf_height_set == 0 || tabmode == FZY_TAB) {
		size_t max_height = set_fzf_max_win_height();
		if (i + 1 > max_height)
			height = max_height;
		else
			height = i;
	}

	/* Calculate the offset (left padding) of the FZF window based on
	 * cursor position and current query string */
	int max_finder_offset = term_cols > 20 ? term_cols - 20 : 0;
	int finder_offset = (rl_point + prompt_offset < max_finder_offset)
			? (rl_point + prompt_offset - 4) : 0;

// TESTING FZF OFFSET!
	if (text && xargs.fuzzy_match == 1)
		/* text is not NULL whenever a common prefix was added, replacing
		 * the original query string */
		finder_offset -= (int)(strlen(matches[0]) - strlen(text));
// TESTING FZF OFFSET!

	if (!lw) {
		finder_offset++;
	} else {
		size_t lw_len = strlen(lw);
		if (lw_len > 1) {
			finder_offset -= (int)(lw_len - 1);
			if (finder_offset < 0)
				finder_offset = 0;
		}
	}

	char *query = (char *)NULL;
	/* In case of a range, the sel keyword, or a full tag expression,
	 * the query string is just empty */
	if (cur_comp_type != TCMP_RANGES && cur_comp_type != TCMP_SEL
	&& cur_comp_type != TCMP_BM_PATHS
	&& cur_comp_type != TCMP_TAGS_F && cur_comp_type != TCMP_GLOB
	&& cur_comp_type != TCMP_FILE_TYPES_OPTS && cur_comp_type != TCMP_FILE_TYPES_FILES) {
		query = get_query_str(&finder_offset);
		if (!query) {
			if (cur_comp_type == TCMP_TAGS_T)
				query = lw ? lw + 2 : (char *)NULL;
			else if (cur_comp_type == TCMP_TAGS_C)
				query = lw ? lw + 1 : (char *)NULL;
			else
				query = lw ? lw : (char *)NULL;
		}
		if (!query || !*query) /* Last char is space */
			finder_offset++;
	}

	if (cur_comp_type == TCMP_TAGS_F) {
		if (rl_end && rl_line_buffer[rl_end - 1] == ' ')
			/* Coming from untag ('tu :TAG ') */
			finder_offset++;
		else { /* Coming from tag expression ('t:FULL_TAG') */
			char *sp = strrchr(rl_line_buffer,  ' ');
			finder_offset = prompt_offset + (int)(sp - rl_line_buffer) - 2;
		}
	} else if (cur_comp_type == TCMP_FILE_TYPES_OPTS) {
		finder_offset++;
	} else if (cur_comp_type == TCMP_FILE_TYPES_FILES) {
		char *sp = strrchr(rl_line_buffer,  ' ');
		if (sp) /* Expression is second or more word: "text =FILE_TYPE" */
			finder_offset = prompt_offset + (int)(sp - rl_line_buffer) - 1;
		else /* Expression is first word: "=FILE_TYPE" */
			finder_offset = prompt_offset - 2;
	} else if (cur_comp_type == TCMP_SEL || cur_comp_type == TCMP_RANGES
	|| cur_comp_type == TCMP_BM_PATHS) {
		char *sp = strrchr(rl_line_buffer, ' ');
		finder_offset = prompt_offset + (int)(sp - rl_line_buffer) - 2;
	} else if (cur_comp_type == TCMP_TAGS_C) {
		char *sp = strrchr(rl_line_buffer,  ' ');
		finder_offset = prompt_offset + (int)(sp - rl_line_buffer) - 1;
	} else if (cur_comp_type == TCMP_TAGS_T) {
		char *sp = strrchr(rl_line_buffer,  ' ');
		finder_offset = prompt_offset + (int)(sp - rl_line_buffer);
	} else if (cur_comp_type == TCMP_GLOB) {
		char *sl = strrchr(rl_line_buffer, '/');
		char *sp = strrchr(rl_line_buffer, ' ');
		if (!sl) {
			if (sp)
				finder_offset = prompt_offset + (int)(sp - rl_line_buffer) - 2;
		} else {
			if (sp && sp > sl)
				finder_offset = prompt_offset + (int)(sp - rl_line_buffer) - 2;
			else
				finder_offset = prompt_offset + (int)(sl - rl_line_buffer) - 2;
		}
	}

	if (finder_offset < 0)
		finder_offset = 0;

	/* TAB completion cases allowing multiple selection */
	int multi = is_multi_sel();

	/* Run FZF and store the ouput into the FINDER_OUT_FILE file */
	int ret = run_finder(&height, &finder_offset, query, multi);
	unlink(finder_in_file);

	/* Calculate currently used lines to go back to the correct cursor
	 * position after quitting FZF */
	int lines = 1, total_line_len = 0;
	total_line_len = rl_end + prompt_offset;
	/* PROMPT_OFFSET (the space used by the prompt in the current line)
	 * is calculated the first time we print the prompt (in my_rl_getc
	 * (readline.c)) */

	if (total_line_len > term_cols) {
		lines = total_line_len / term_cols;
		int rem = (int)total_line_len % term_cols;
		if (rem > 0)
			lines++;
	}

	MOVE_CURSOR_UP(lines);

	/* No results (the user pressed ESC) */
	if (ret != EXIT_SUCCESS) {
		unlink(finder_out_file);
		return clean_rl_buffer(text);
	}

	char *buf = get_finder_output(multi);
	if (!buf)
		return EXIT_FAILURE;

	/* Calculate the length of the matching prefix to insert into the
	 * line buffer only the non-matched part of the string returned by FZF */
	size_t prefix_len = calculate_prefix_len(matches[0]);

	if (cur_comp_type == TCMP_OPENWITH) {
		/* Interpret the corresponding cmd line in the mimelist file
		 * and replace the input string by the interpreted line */
		char *sp = (char *)NULL;
		if (rl_line_buffer) {
			sp = strchr(rl_line_buffer, ' ');
			if (!sp || !*(sp++)) {
				free(buf);
				return EXIT_FAILURE;
			}
		} else {
			free(buf);
			return EXIT_FAILURE;
		}

		char *t = sp;
		while (*t) {
			if (t != sp && *t == ' ' && *(t - 1) != '\\') {
				*t = '\0';
				break;
			}
			++t;
		}

		size_t splen = (size_t)(t - sp);
		if (sp[splen - 1] == '/') {
			splen--;
			sp[splen] = '\0';
		}

		rl_delete_text(0, rl_end);
		rl_point = rl_end = 0;
		prefix_len = 0;

		t = strchr(buf, '%');
		if (t && *(t + 1) == 'f') {
			char *ss = replace_substr(buf, "%f", sp);
			if (ss) {
				buf = (char *)xrealloc(buf, (strlen(ss) + 1) * sizeof(char));
				strcpy(buf, ss);
				free(ss);
			}
		} else {
			size_t blen = strlen(buf);
			if (buf[blen - 1] == '\n') {
				blen--;
				buf[blen] = '\0';
			}
			splen = strlen(sp);
			buf = (char *)xrealloc(buf, (blen + splen + 2) * sizeof(char));
			buf[blen] = ' ';
			buf[blen + 1] = '\0';
			xstrsncpy(buf + blen + 1, sp, splen);
		}

	} else if (cur_comp_type == TCMP_DESEL) {
		prefix_len = strlen(query ? query : (lw ? lw : ""));

	} else if (cur_comp_type == TCMP_HIST || cur_comp_type == TCMP_JUMP) {
		rl_delete_text(0, rl_end);
		rl_point = rl_end = 0;
		prefix_len = 0;

	} else if (cur_comp_type == TCMP_RANGES || cur_comp_type == TCMP_SEL
	|| cur_comp_type == TCMP_TAGS_F || cur_comp_type == TCMP_GLOB
	|| cur_comp_type == TCMP_BM_PATHS) {
		char *s = strrchr(rl_line_buffer, ' ');
		if (s) {
			rl_point = (int)(s - rl_line_buffer + 1);
			rl_delete_text(rl_point, rl_end);
			rl_end = rl_point;
			prefix_len = 0;
		}

	} else if (cur_comp_type == TCMP_FILE_TYPES_FILES) {
		char *s = strrchr(rl_line_buffer, ' ');
		rl_point = !s ? 0 : (int)(s - rl_line_buffer + 1);
		rl_delete_text(rl_point, rl_end);
		rl_end = rl_point;
		prefix_len = 0;

	} else if (cur_comp_type == TCMP_USERS) {
		size_t l = strlen(buf);
		char *p = savestring(buf, l);
		buf = (char *)xrealloc(buf, (l + 2) * sizeof(char));
		sprintf(buf, "~%s", p);
		free(p);
	} else {
		if ((case_sens_path_comp == 0 || xargs.fuzzy_match == 1) && query) {
			/* Honor case insensitive completion/fuzzy matches */
			if (strncmp(matches[0], buf, prefix_len) != 0) {
				int bk = rl_point;
				rl_delete_text(bk - (int)prefix_len, rl_end);
				rl_point = rl_end = bk - (int)prefix_len;
				prefix_len = 0;
			}
		}
	}

	if (buf && *buf) {
		/* Some buffer clean up: remove new line char and ending spaces */
		size_t blen = strlen(buf);
		int j = (int)blen;
		if (buf[j - 1] == '\n') {
			j--;
			buf[j] = '\0';
		}
		while (--j >= 0 && buf[j] == ' ')
			buf[j] = '\0';

		char *q = (char *)NULL;
		if (cur_comp_type != TCMP_OPENWITH && cur_comp_type != TCMP_PATH
		&& cur_comp_type != TCMP_HIST && !multi) {
			q = escape_str(buf);
			if (!q) {
				free(buf);
				return EXIT_FAILURE;
			}
		} else {
			q = savestring(buf, blen);
		}

		fzf_open_with = 1;
		write_completion(q, &prefix_len, &exit_status, multi);
		free(q);
	}

	free(buf);

#ifndef _NO_SUGGESTIONS
	if (suggestions && nwords == 1 && wrong_cmd == 1) {
		fputs(NC, stdout);
		fflush(stdout);
		rl_restore_prompt();
		wrong_cmd = 0;
	}
#endif

	return exit_status;
}
#endif /* !_NO_FZF */

/* Simple comparison routine for qsort()ing strings */
static int
compare_strings (s1, s2)
  char **s1, **s2;
{
	int ret;

	ret = **s1 - **s2;
	if (ret == 0)
		ret = strcmp(*s1, *s2);

	return ret;
}

/* Complete the word at or before point.
   WHAT_TO_DO says what to do with the completion.
   `?' means list the possible completions.
   TAB means do standard completion.
   `*' means insert all of the possible completions.
   `!' means to do standard completion, and list all possible completions
   if there is more than one. */
/* This function is taken from an old bash release (1.14.7) and modified
 * to fit our needs */
int
tab_complete(int what_to_do)
{
	if (rl_notab)
		return EXIT_SUCCESS;

	if (*rl_line_buffer == '#' || cur_color == hc_c) {
		/* No completion at all if comment */
#ifndef _NO_SUGGESTIONS
		if (suggestion.printed)
			clear_suggestion(CS_FREEBUF);
#endif /* _NO_SUGGESTIONS */
		return EXIT_SUCCESS;
	}

	rl_compentry_func_t *our_func = rl_completion_entry_function;

	/* Only the completion entry function can change these */
	rl_filename_completion_desired = 0;
	rl_filename_quoting_desired = 1;
	rl_sort_completion_matches = 1;

	int end = rl_point, delimiter = 0;
	char quote_char = '\0';

	/* We now look backwards for the start of a filename/variable word. */
	if (rl_point) {
		int scan = 0;

		if (rl_completer_quote_characters) {
		/* We have a list of characters which can be used in pairs to
		 * quote substrings for the completer. Try to find the start
		 * of an unclosed quoted substring. */
		/* FOUND_QUOTE is set so we know what kind of quotes we found. */
			int pass_next; //found_quote = 0;
			for (scan = pass_next = 0; scan < end; scan++) {
				if (pass_next) {
					pass_next = 0;
					continue;
				}

				if (rl_line_buffer[scan] == '\\') {
					pass_next = 1;
					continue;
				}

				if (quote_char != '\0') {
				/* Ignore everything until the matching close quote char. */
					if (rl_line_buffer[scan] == quote_char) {
					/* Found matching close. Abandon this substring. */
						quote_char = '\0';
						rl_point = end;
					}
				} else if (strchr(rl_completer_quote_characters,
						rl_line_buffer[scan])) {
					/* Found start of a quoted substring. */
					quote_char = rl_line_buffer[scan];
					rl_point = scan + 1;
				}
			}
		}

		if (rl_point == end && quote_char == '\0') {
		/* We didn't find an unclosed quoted substring upon which to do
		 * completion, so use the word break characters to find the
		 * substring on which to complete. */
			while (--rl_point) {
				scan = rl_line_buffer[rl_point];

				if (strchr(rl_completer_word_break_characters, scan) == 0
				|| (scan == ' '	&& rl_point
				&& rl_line_buffer[rl_point - 1] == '\\'))
					continue;

				/* Convoluted code, but it avoids an n^2 algorithm with
				 * calls to char_is_quoted. */
				break;
			}
		}

		/* If we are at an unquoted word break, then advance past it. */
		scan = rl_line_buffer[rl_point];
		if (strchr(rl_completer_word_break_characters, scan)) {
		/* If the character that caused the word break was a quoting
		 * character, then remember it as the delimiter. */
			if (strchr("\"'", scan) && (end - rl_point) > 1)
				delimiter = scan;

		/* If the character isn't needed to determine something special
		 * about what kind of completion to perform, then advance past it. */
			if (!rl_special_prefixes || strchr(rl_special_prefixes, scan) == 0)
				rl_point++;
		}
	}

	int start = rl_point;
	rl_point = end;
	char *text = rl_copy_text(start, end);
	char **matches = (char **)NULL;

	/* At this point, we know we have an open quote if quote_char != '\0'. */

	/* If the user wants to TRY to complete, but then wants to give
	* up and use the default completion function, they set the
	* variable rl_attempted_completion_function. */
	if (rl_attempted_completion_function) {
		matches = (*rl_attempted_completion_function) (text, start, end);
		if (matches || rl_attempted_completion_over) {
			rl_attempted_completion_over = 0;
			our_func = (rl_compentry_func_t *)NULL;
			goto AFTER_USUAL_COMPLETION;
		}
	}

	matches = rl_completion_matches(text, our_func);

AFTER_USUAL_COMPLETION:

	if (!matches || !matches[0]) {
		rl_ring_bell();
		free(text);
		return EXIT_FAILURE;
	}

#ifndef _NO_FZF
	/* Common prefix for multiple matches is appended to the input query.
	 * Let's rise a flag to know if we should reinsert the original query
	 * in case the user cancels the completion (pressing ESC) */
	int common_prefix_added = 0;
	if (fzftab == 1 && matches[1] && strcmp(matches[0], text) != 0)
		common_prefix_added = 1;
#endif /* _NO_FZF */

	register size_t i;
	int should_quote;

	/* It seems to me that in all the cases we handle we would like
	 * to ignore duplicate possiblilities. Scan for the text to
	 * insert being identical to the other completions. */
	if (rl_ignore_completion_duplicates == 1) {
		char *lowest_common;
		size_t j;
		size_t newlen = 0;
		char dead_slot;
		char **temp_array;

		if (cur_comp_type == TCMP_HIST) {// || cur_comp_type == TCMP_EXT_OPTS) {
			/* Sort the array without matches[0]: we need it to stay in
			 * place no matter what */
			for (i = 0; matches[i]; i++);
			if (i > 0)
				qsort(matches + 1, i - 1, sizeof(char *), compare_strings);
		}

		/* Remember the lowest common denominator, for it may be unique */
		lowest_common = savestring(matches[0], strlen(matches[0]));

		for (i = 0; matches[i + 1]; i++) {
			if (strcmp(matches[i], matches[i + 1]) == 0) {
				free(matches[i]);
				matches[i] = (char *)&dead_slot;
			} else {
				newlen++;
			}
		}

		/* We have marked all the dead slots with (char *)&dead_slot
		 * Copy all the non-dead entries into a new array */
		temp_array = (char **)xnmalloc(3 + newlen, sizeof (char *));
		for (i = j = 1; matches[i]; i++) {
			if (matches[i] != (char *)&dead_slot) {
				temp_array[j] = matches[i];
				j++;
			}
		}
		temp_array[j] = (char *)NULL;

		if (matches[0] != (char *)&dead_slot)
			free(matches[0]);
		free(matches);

		matches = temp_array;

		/* Place the lowest common denominator back in [0]. */
		matches[0] = lowest_common;

		/* If there is one string left, and it is identical to the lowest
		 * common denominator (LCD), then the LCD is the string to insert */
		if (j == 2 && strcmp(matches[0], matches[1]) == 0) {
			free(matches[1]);
			matches[1] = (char *)NULL;
		}
	}

	switch (what_to_do) {
	case '!':
		/* If we are matching filenames, then here is our chance to
		 * do clever processing by re-examining the list. Call the
		 * ignore function with the array as a parameter. It can
		 * munge the array, deleting matches as it desires. */
		if (rl_ignore_some_completions_function
		&& our_func == rl_completion_entry_function) {
			(void)(*rl_ignore_some_completions_function)(matches);
			if (matches == 0 || matches[0] == 0) {
				if (matches)
					free(matches);
				free(text);
				rl_ding();
				return 0;
			}
		}

		/* If we are doing completion on quoted substrings, and any matches
		 * contain any of the completer_word_break_characters, then
		 * automatically prepend the substring with a quote character
		 * (just pick the first one from the list of such) if it does not
		 * already begin with a quote string. FIXME: Need to remove any such
		 * automatically inserted quote character when it no longer is necessary,
		 * such as if we change the string we are completing on and the new
		 * set of matches don't require a quoted substring. */
		char *replacement = matches[0];
		should_quote = matches[0] && rl_completer_quote_characters &&
		rl_filename_completion_desired && rl_filename_quoting_desired;

		if (should_quote)
			should_quote = should_quote && !quote_char;

		if (should_quote) {
			int do_replace;

			do_replace = NO_MATCH;

			/* If there is a single match, see if we need to quote it.
			This also checks whether the common prefix of several
			matches needs to be quoted.  If the common prefix should
			not be checked, add !matches[1] to the if clause. */
			should_quote = rl_strpbrk(matches[0], quote_chars) != 0;

			if (should_quote)
				do_replace = matches[1] ? MULT_MATCH : SINGLE_MATCH;

			if (do_replace != NO_MATCH) {
				/* Found an embedded word break character in a potential
				 match, so we need to prepend a quote character if we
				 are replacing the completion string. */
				replacement = escape_str(matches[0]);

				/* escape_str escapes the leading tilde, but we don't
				 * want it here. Remove it */
/*				if (cur_comp_type == TCMP_PATH && *matches[0] == '~') {
					char *tmp = strdup(replacement + 1);
					free(replacement);
					replacement = tmp;
				} */
			}
		}

		if (replacement && cur_comp_type != TCMP_HIST
		&& cur_comp_type != TCMP_FILE_TYPES_OPTS
		&& (cur_comp_type != TCMP_FILE_TYPES_FILES || !matches[1])
		&& (cur_comp_type != TCMP_GLOB || !matches[1])
		&& cur_comp_type != TCMP_JUMP && cur_comp_type != TCMP_RANGES
		&& (cur_comp_type != TCMP_SEL || fzftab != 1 || sel_n == 1)

		&& (cur_comp_type != TCMP_BM_PATHS || !matches[1])

		&& (cur_comp_type != TCMP_TAGS_F || !matches[1])) {
			enum comp_type c = cur_comp_type;
			if ((c == TCMP_SEL || c == TCMP_DESEL || c == TCMP_NET

			|| c == TCMP_BM_PATHS

			|| c == TCMP_TAGS_C || c == TCMP_TAGS_S || c == TCMP_TAGS_T
			|| c == TCMP_TAGS_U || c == TCMP_BOOKMARK || c == TCMP_GLOB
			|| c == TCMP_PROMPTS) && !strchr(replacement, '\\')) {
				char *r = escape_str(replacement);
				if (!r) {
					if (replacement != matches[0])
						free(replacement);
					break;
				}
				if (replacement != matches[0])
					free(replacement);
				replacement = r;
			}

			rl_begin_undo_group();
			rl_delete_text(start, rl_point);
			rl_point = start;
#ifndef _NO_HIGHLIGHT
			if (highlight && !wrong_cmd) {
				size_t k, l = 0;
				size_t _start = (*replacement == '\\' && *(replacement + 1) == '~') ? 1 : 0;
				char *cc = cur_color;
				fputs(HIDE_CURSOR, stdout);
				char t[PATH_MAX];
				for (k = _start; replacement[k]; k++) {
					rl_highlight(replacement, k, SET_COLOR);
					if (replacement[k] < 0) {
						t[l] = replacement[k];
						l++;
						if (replacement[k + 1] >= 0) {
							t[l] = '\0';
							l = 0;
							rl_insert_text(t);
							rl_redisplay();
						}
						continue;
					}
					t[0] = (char)replacement[k];
					t[1] = '\0';
					rl_insert_text(t);
					rl_redisplay();
				}
				fputs(UNHIDE_CURSOR, stdout);
				cur_color = cc;
				if (cur_color)
					fputs(cur_color, stdout);
			} else {
				char *q = (*replacement == '\\' && *(replacement + 1) == '~')
						? replacement + 1 : replacement;
				rl_insert_text(q);
				rl_redisplay();
			}
#else
			char *q = (*replacement == '\\' && *(replacement + 1) == '~')
					? replacement + 1 : replacement;
			rl_insert_text(q);
			rl_redisplay();
#endif /* !_NO_HIGHLIGHT */
			rl_end_undo_group();
		}

		if (replacement != matches[0])
			free(replacement);

/*		if (cur_comp_type == TCMP_FILE_TYPES_FILES) {
			char *s = strrchr(rl_line_buffer, ' ');
			rl_point = !s ? 0 : (int)(s - rl_line_buffer + 1);
			rl_delete_text(rl_point, rl_end);
			rl_end = rl_point;
			rl_redisplay();
		} */

		/* If there are more matches, ring the bell to indicate. If this was
		 * the only match, and we are hacking files, check the file to see if
		 * it was a directory. If so, add a '/' to the name.  If not, and we
		 * are at the end of the line, then add a space. */
		if (matches[1]) {
			if (what_to_do == '!') {
				goto DISPLAY_MATCHES;		/* XXX */
			} else {
				if (rl_editing_mode != 0) /* vi_mode */
					rl_ding();	/* There are other matches remaining. */
			}
		} else {
			char temp_string[4];
			int temp_string_index = 0;

			if (quote_char) {
				temp_string[temp_string_index] = quote_char;
				temp_string_index++;
			}

			if (fzftab != 1 || cur_comp_type != TCMP_TAGS_T) {
				temp_string[temp_string_index] = (char)(delimiter ? delimiter : ' ');
				temp_string_index++;
			}
			temp_string[temp_string_index] = '\0';

			if (rl_filename_completion_desired) {
				struct stat finfo;
				char *filename = tilde_expand(matches[0]);

				char *d = filename;
				if (*filename == 'f' && filename[1] == 'i') {
					size_t flen = strlen(filename);
					if (flen > FILE_URI_PREFIX_LEN && IS_FILE_URI(filename))
						d = filename + FILE_URI_PREFIX_LEN;
				}

				if ((stat(d, &finfo) == 0) && S_ISDIR(finfo.st_mode)) {
					if (rl_line_buffer[rl_point] != '/') {
#ifndef _NO_HIGHLIGHT
						if (highlight && !wrong_cmd) {
							char *cc = cur_color;
							fputs(hd_c, stdout);
							rl_insert_text("/");
							rl_redisplay();
							fputs(cc, stdout);
						} else {
							rl_insert_text("/");
						}
#else
						rl_insert_text("/");
#endif /* !_NO_HIGHLIGHT */
					}
				} else {
					if (rl_point == rl_end)
						rl_insert_text(temp_string);
				}
				free(filename);
			} else {
				if (rl_point == rl_end)
					rl_insert_text(temp_string);
			}
		}
	break;

	case '?': {
		int len = 0, count = 0, limit = 0, max = 0;
		int j = 0, k = 0, l = 0;

		/* Handle simple case first. Just one match */
		if (!matches[1]) {
			char *temp;
			temp = printable_part(matches[0]);
			rl_crlf();
			print_filename(temp, matches[0]);
			rl_crlf();
			goto RESTART;
		}

		/* There is more than one match. Find out how many there are,
		and find out what the maximum printed length of a single entry
		is. */

DISPLAY_MATCHES:
#ifndef _NO_FZF
		if (fzftab != 1) {
#endif /* !_NO_FZF */
		{
			max = 0;
			for (i = 1; matches[i]; i++) {
				char *temp;
				size_t name_length;

				temp = printable_part(matches[i]);
				name_length = strlen(temp);

				if ((int)name_length > max)
				  max = (int)name_length;
			}

			len = (int)i - 1;

			/* If there are multiple items, ask the user if she
			   really wants to see them all */
			if (len >= rl_completion_query_items) {
				putchar('\n');
#ifndef _NO_HIGHLIGHT
				if (highlight && cur_color != tx_c && !wrong_cmd) {
					cur_color = tx_c;
					fputs(tx_c, stdout);
				}
#endif /* !_NO_HIGHLIGHT */
				fprintf(rl_outstream, "Display all %d possibilities? (y or n) ", len);
				fflush(rl_outstream);
				if (!get_y_or_n())
					goto RESTART;
			}

			/* How many items of MAX length can we fit in the screen window? */
			max += 2;
			limit = term_cols / max;
			if (limit != 1 && (limit * max == term_cols))
				limit--;

			if (limit == 0)
			  limit = 1;

			/* How many iterations of the printing loop? */
			count = (len + (limit - 1)) / limit;

		}
#ifndef _NO_FZF
		}
#endif /* !_NO_FZF */

		putchar('\n');
#ifndef _NO_HIGHLIGHT
		if (highlight && cur_color != tx_c && !wrong_cmd) {
			cur_color = tx_c;
			fputs(tx_c, stdout);
		}
#endif /* !_NO_HIGHLIGHT */
		char *qq = (char *)NULL, *ptr = (char *)NULL;
		if (cur_comp_type != TCMP_PATH)
			goto CALC_OFFSET;

		if (*matches[0] == '~') {
			char *exp_path = tilde_expand(matches[0]);
			if (exp_path) {
				xchdir(exp_path, NO_TITLE);
				free(exp_path);
			}
		} else {
			char *dir = matches[0];
			size_t dlen = strlen(dir);
			if (dlen > FILE_URI_PREFIX_LEN && IS_FILE_URI(dir))
				dir += FILE_URI_PREFIX_LEN;
			char *p = strrchr(dir, '/');
			if (!p)
				goto CALC_OFFSET;

			if (p == dir) {
				if (*(p + 1)) {
					char pp = *(p + 1);
					*(p + 1) = '\0';
					xchdir(dir, NO_TITLE);
					*(p + 1) = pp;
				} else {
					/* We have the root dir */
					xchdir(dir, NO_TITLE);
				}
			} else {
				*p = '\0';
				xchdir(dir, NO_TITLE);
				*p = '/';
			}
		}

CALC_OFFSET:
#ifndef _NO_FZF
		if (fzftab == 1) {
			char *t = text ? text : (char *)NULL;
			if (finder_tabcomp(matches, common_prefix_added == 1 ? t : NULL,
			xargs.fuzzy_match == 1 ? t : NULL) == -1)
				goto RESTART;
			goto RESET_PATH;
		}
#endif /* !_NO_FZF */

		ptr = matches[0];
		/* Skip leading backslashes */
		while (*ptr) {
			if (*ptr != '\\')
				break;
			ptr++;
		}

/*		qq = strrchr(ptr, '/');
		if (qq) { */
		qq = (cur_comp_type == TCMP_DESEL || cur_comp_type == TCMP_SEL)
			? ptr : strrchr(ptr, '/');
		if (qq && qq != ptr) {
			if (*(++qq)) {
				tab_offset = strlen(qq);
			} else {
				if (cur_comp_type == TCMP_DESEL) {
					tab_offset = strlen(matches[0]);
					qq = matches[0];
				}
			}
		} else {
			tab_offset = strlen(ptr);
		}

		if (cur_comp_type == TCMP_RANGES || cur_comp_type == TCMP_BACKDIR
		|| cur_comp_type == TCMP_FILE_TYPES_FILES || cur_comp_type == TCMP_FILE_TYPES_OPTS
		|| cur_comp_type == TCMP_BM_PATHS)
			tab_offset = 0;

		for (i = 1; i <= (size_t)count; i++) {
			if (i >= term_rows) {
				/* A little pager */
				fputs("\x1b[7;97m--Mas--\x1b[0;49m", stdout);
				int c = 0;
				while ((c = xgetchar()) == _ESC);
				if (c == 'q') {
					/* Delete the --Mas-- label */
					fputs("\x1b[7D\x1b[7X\x1b[1A\n", stdout);
					break;
				}
				fputs("\x1b[7D\x1b[0K", stdout);
			}

			l = (int)i;
			for (j = 0; j < limit; j++) {
				if (l > len || !matches[l] || !*matches[l]) {
					break;
				} else {
					if (tab_offset) {
						/* Print the matching part of the match */
						printf("\x1b[0m%s%s\x1b[0m%s", ts_c, qq ? qq : matches[0],
						(cur_comp_type == TCMP_CMD) ? (colorize
						? ex_c : "") : fc_c);
					}

					/* Now print the non-matching part of the match */
					char *temp;
					int printed_length;
					temp = printable_part(matches[l]);
					printed_length = (int)strlen(temp);
					printed_length += print_filename(temp, matches[l]);

					if (j + 1 < limit) {
						for (k = 0; k < max - printed_length; k++)
							putc(' ', rl_outstream);
					}
				}
				l += count;
			}
			putchar('\n');
		}
		tab_offset = 0;

		if (!wrong_cmd && colorize && cur_comp_type == TCMP_CMD)
			fputs(tx_c, stdout);
		rl_reset_line_state();

#ifndef _NO_FZF
RESET_PATH:
#endif /* !_NO_FZF */
		if (cur_comp_type == TCMP_PATH)
			xchdir(workspaces[cur_ws].path, NO_TITLE);

RESTART:
		rl_on_new_line();
#ifndef _NO_HIGHLIGHT
		if (highlight == 1 && wrong_cmd == 0) {
			int bk = rl_point;
/*			rl_point = 0;
			recolorize_line();
			rl_point = bk; */
			fputs(HIDE_CURSOR, stdout);
			char *ss = rl_copy_text(0, rl_end);
			rl_delete_text(0, rl_end);
			rl_redisplay();
			rl_point = rl_end = 0;

			l = 0;
			char t[PATH_MAX];
			for (k = 0; ss[k]; k++) {
				rl_highlight(ss, (size_t)k, SET_COLOR);

				if (ss[k] < 0) {
					t[l] = ss[k];
					l++;
					if (ss[k + 1] >= 0) {
						t[l] = '\0';
						l = 0;
						rl_insert_text(t);
						rl_redisplay();
					}
					continue;
				}

				t[0] = (char)ss[k];
				t[1] = '\0';
				rl_insert_text(t);
				rl_redisplay();
			}
			fputs(UNHIDE_CURSOR, stdout);
//			rl_point = rl_end = bk;
			rl_point = bk;
			free(ss);
		}
#endif /* !_NO_HIGHLIGHT */
		}
		break;

	default:
		fprintf(stderr, "\r\nreadline: bad value for what_to_do in rl_complete\n");
		abort();
		break;
	}

	for (i = 0; matches[i]; i++)
		free(matches[i]);
	free(matches);
	free(text);

	return EXIT_SUCCESS;
}
