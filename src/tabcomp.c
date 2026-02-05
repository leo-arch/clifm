/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* tabcomp.c -- functions for tab completion */

/* The following functions are taken from Bash (1.14.7), licensed GPL-1.0-or-later,
 * and modified as required:
 * stat_char
 * get_y_or_n
 * print_filename
 * printable_part
 * rl_strpbrk
 * compare_strings
 * tab_complete
 * All changes are licensed GPL-2.0-or-later. */

#include "helpers.h"

#include <unistd.h>

#ifdef __OpenBSD__
typedef char *rl_cpvfunc_t;
typedef void rl_macro_print_func_t (const char *, const char *, int, const char *);
# include <ereadline/readline/readline.h>
#else
# include <readline/readline.h>
#endif /* __OpenBSD__ */

#include <errno.h>

#include "aux.h"
#include "checks.h"
#include "colors.h"
#ifndef _NO_HIGHLIGHT
# include "highlight.h"
#endif /* !_NO_HIGHLIGHT */
#include "misc.h"
#include "navigation.h"
#include "readline.h"
#include "selection.h"
#include "sort.h"
#include "spawn.h"
#include "strings.h" /* quote_str() */
#ifndef _NO_SUGGESTIONS
# include "suggestions.h"
#endif /* !_NO_SUGGESTIONS */

#ifndef _NO_FZF
# define SHOW_PREVIEWS(c) ((c) == TCMP_PATH || (c) == TCMP_SEL \
|| (c) == TCMP_RANGES || (c) == TCMP_DESEL || (c) == TCMP_JUMP \
|| (c) == TCMP_TAGS_F || (c) == TCMP_GLOB || (c) == TCMP_FILE_TYPES_FILES \
|| (c) == TCMP_BM_PATHS || (c) == TCMP_UNTRASH || (c) == TCMP_TRASHDEL)

static char finder_in_file[PATH_MAX + 1];
static char finder_out_file[PATH_MAX + 1];

/* We need to know the longest entry (if previewing files) to correctly
 * calculate the width of the preview window. */
static size_t longest_prev_entry;
#endif /* _NO_FZF */

/* The following three functions are used to get current cursor position
 * (both vertical and horizontal), needed by tab completion in fzf mode
 * with previews enabled */

/* Return the character which best describes FILENAME.
`@' for symbolic links
`/' for directories
`*' for executables
`=' for sockets */
static int
stat_char(const char *filename)
{
	struct stat attr;
	int r;

#ifdef S_ISLNK
	r = lstat(filename, &attr);
#else
	r = stat(filename, &attr);
#endif /* S_ISLNK */

	if (r == -1)
		return 0;

	int c = 0;
	if (S_ISDIR(attr.st_mode)) {
		c = DIR_CHR;
#ifdef S_ISLNK
	} else if (S_ISLNK(attr.st_mode)) {
		c = LINK_CHR;
#endif /* S_ISLNK */
#ifdef S_ISSOCK
	} else if (S_ISSOCK(attr.st_mode)) {
		c = SOCK_CHR;
#endif /* S_ISSOCK */
	} else if (S_ISREG(attr.st_mode)) {
		if (access(filename, X_OK) == 0)
			c = EXEC_CHR;
	} else if (S_ISBLK(attr.st_mode)) {
		c = BLK_CHR;
	} else if (S_ISCHR(attr.st_mode)) {
		c = CHR_CHR;
#ifdef SOLARIS_DOORS
	} else if (S_ISDOOR(attr.st_mode)) {
		c = DOOR_CHR;
#endif /* SOLARIS_DOORS */
#ifdef S_ISWHT
	} else if (S_ISWHT(attr.st_mode)) {
		c = WHT_CHR;
#endif /* S_IFWHT */
#ifdef S_ISFIFO
	} else {
		if (S_ISFIFO(attr.st_mode))
			c = FIFO_CHR;
#endif /* S_ISFIFO */
	}

	return c;
}

/* The user must press "y" or "n". Non-zero return means "y" pressed. */
static int
get_y_or_n(void)
{
	for (;;) {
		const int c = fgetc(stdin); /* Flawfinder: ignore */
		if (c == 'y' || c == 'Y' || c == ' ')
			return (1);
		if (c == 'n' || c == 'N' || c == RUBOUT || c == EOF) {
			putchar('\n');
			return (0);
		}
		if (c == ABORT_CHAR) /* Defined by readline as CTRL('G') */
			rl_abort(0, 0);
		rl_ding();
	}
}

static int
print_filename(char *to_print, char *full_pathname)
{
	char *s;
	const enum comp_type t = cur_comp_type;

	if (conf.colorize == 1 && (t == TCMP_PATH || t == TCMP_SEL
	|| t == TCMP_DESEL || t == TCMP_RANGES || t == TCMP_TAGS_F
	|| t == TCMP_FILE_TYPES_FILES || t == TCMP_MIME_LIST
	|| t == TCMP_BM_PATHS || t == TCMP_GLOB || t == TCMP_UNTRASH
	|| t == TCMP_TRASHDEL || t == TCMP_FILE_TEMPLATES)) {
		colors_list(to_print, NO_ELN, NO_PAD, NO_NEWLINE);
	} else {
		for (s = to_print + tab_offset; *s; s++)
			putchar(*s);
	}

	if (rl_filename_completion_desired && conf.colorize == 0) {
		if (t == TCMP_CMD) {
			putc('*', rl_outstream);
			return 1;
		}

		/* If to_print != full_pathname, to_print is the basename of the
		 * path passed. In this case, we try to expand the directory
		 * name before checking for the stat character. */
		int extension_char = 0;
		if (to_print != full_pathname) {
			/* Terminate the directory name */
			const char c = to_print[-1];
			to_print[-1] = '\0';

			s = tilde_expand(full_pathname);
			if (rl_directory_completion_hook)
				(*rl_directory_completion_hook)(&s);

			const size_t slen = strlen(s);
			const size_t tlen = strlen(to_print);
			char *new_full_pathname = xnmalloc(slen + tlen + 2, sizeof(char));
			snprintf(new_full_pathname, slen + tlen + 2, "%s/%s", s, to_print);

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
 * possible completions. If we are hacking filename completion, we
 * are only interested in the basename, the portion following the
 * final slash. Otherwise, we return what we were passed. */
static char *
printable_part(char *pathname)
{
	char *temp = NULL;

	if (rl_filename_completion_desired)
		temp = strrchr(pathname, '/');

	if (!temp)
		return (pathname);
	else
		return (++temp);
}

/* Find the first occurrence in STRING1 of any character from STRING2.
 * Return a pointer to the character in STRING1. */
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
	return NULL;
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
fzftab_color(const char *filename, const struct stat *attr)
{
	if (conf.colorize == 0)
		return df_c;

	struct stat a;

	switch (attr->st_mode & S_IFMT) {
	case S_IFDIR:
		return get_dir_color(filename, attr, -1);

	case S_IFREG: {
		if (conf.light_mode == 1)
			return fi_c;

		if (*nf_c
		&& check_file_access(attr->st_mode, attr->st_uid, attr->st_gid) == 0)
			return nf_c;

		char *cl = get_file_color(filename, attr);
		if (conf.check_ext == 0 || cl != fi_c)
			return (cl ? cl : fi_c);

		char *ext_cl = NULL;
		const char *ext = strrchr(filename, '.');
		if (ext && ext != filename)
			ext_cl = get_ext_color(ext, NULL);

		return ext_cl ? ext_cl : (cl ? cl : fi_c);
		}

	case S_IFSOCK: return so_c;
	case S_IFIFO: return pi_c;
	case S_IFBLK: return bd_c;
	case S_IFCHR: return cd_c;
	case S_IFLNK: return conf.light_mode == 1 ? ln_c
		: (stat(filename, &a) == -1 ? or_c : ln_c);
	default: return uf_c;
	}
}

static char *
get_comp_entry_color(char *entry, const char *norm_prefix)
{
	if (conf.colorize == 0)
		return NULL;

	char vt_file[PATH_MAX + 1]; *vt_file = '\0';
	if (virtual_dir == 1 && is_file_in_cwd(entry))
		xreadlink(XAT_FDCWD, entry, vt_file, sizeof(vt_file));

	struct stat attr;

	/* Normalize URI file scheme */
	const size_t len = strlen(entry);
	if (IS_FILE_URI(entry, len))
		entry += FILE_URI_PREFIX_LEN;

	if (norm_prefix && !*vt_file) {
		const char *s = strrchr(entry, '/');
		char tmp[PATH_MAX + 1];
		snprintf(tmp, sizeof(tmp), "%s/%s", norm_prefix,
			(s && *(++s)) ? s : entry);
		if (lstat(tmp, &attr) != -1)
			return fzftab_color(tmp, &attr);
		return uf_c;
	}

	const enum comp_type t = cur_comp_type;
	/* Absolute path (/FILE) or file in CWD (./FILE) */
	if ( (*entry == '/' || (*entry == '.' && entry[1] == '/') )
	&& (t == TCMP_PATH || t == TCMP_SEL || t == TCMP_DESEL
	|| t == TCMP_BM_PATHS || t == TCMP_DIRHIST || t == TCMP_JUMP) ) {
		char *f = *vt_file ? vt_file : entry;
		if (lstat(f, &attr) != -1)
			return fzftab_color(f, &attr);
		return uf_c;
	}

	/* Tilde */
	if (*entry == '~' && (t == TCMP_PATH || t == TCMP_TAGS_F
	|| t == TCMP_BM_PATHS || t == TCMP_SEL || t == TCMP_DESEL) ) {
		char *exp_path = tilde_expand(entry);
		if (exp_path) {
			char *color = uf_c;
			if (lstat(exp_path, &attr) != -1)
				color = fzftab_color(exp_path, &attr);
			free(exp_path);
			return color;
		}
	}

	if (t == TCMP_PATH || t == TCMP_RANGES) {
		if (*vt_file) {
			if (lstat(vt_file, &attr) != -1)
				return fzftab_color(vt_file, &attr);
		} else {
			char tmp[PATH_MAX + 1];
			snprintf(tmp, sizeof(tmp), "%s/%s", workspaces[cur_ws].path, entry);
			if (lstat(tmp, &attr) != -1)
				return fzftab_color(tmp, &attr);
		}

		return uf_c;
	}

	if (t == TCMP_UNTRASH || t == TCMP_TRASHDEL || t == TCMP_GLOB
	|| t == TCMP_TAGS_F || t == TCMP_FILE_TYPES_FILES) {
		char *f = (t == TCMP_GLOB && *vt_file) ? vt_file : entry;

		char p[PATH_MAX + 2]; *p = '\0';
		if (t == TCMP_GLOB && *f != '/') {
			snprintf(p, sizeof(p), "%s/%s", workspaces[cur_ws].path, f);
			f = p;
		}

		return (lstat(f, &attr) != -1) ? fzftab_color(f, &attr) : uf_c;
	}

	if (t == TCMP_FILE_TEMPLATES && templates_dir) {
		char tmp[PATH_MAX + 1];
		snprintf(tmp, sizeof(tmp), "%s/%s", templates_dir, entry);
		if (lstat(tmp, &attr) != -1)
			return fzftab_color(tmp, &attr);
	}

	if (t == TCMP_CMD && is_internal_cmd(entry, ALL_CMDS, 1, 1))
		return hv_c;

	return df_c;
}

/* Return a pointer to the word after the last non-escaped space in the input
 * buffer. If no space is found, apointer to the entire command line
 * (rl_line_buffer) is returned. */
static char *
get_last_input_word(void)
{
	if (!rl_line_buffer)
		return NULL;

	char *lb = rl_line_buffer + 1;
	char *lastword = rl_line_buffer;

	while (*lb) {
		if (*lb == ' ' && *(lb - 1) != '\\' && lb[1] != ' ')
			lastword = lb + 1;
		lb++;
	}

	return lastword;
}

/* Store the unescaped string STR in the buffer BUF (only up to MAX bytes).
 * Returns the number of copied bytes. */
static size_t
unescape_word(const char *str, char *buf, const size_t max)
{
	size_t i = 0;
	const char *p = str;

	while (*p && i < max) {
		if (*p != '\\')
			buf[i++] = *p;
		p++;
	}

	buf[i] = '\0';
	return i;
}

/* Append slash for dirs and space for non-dirs to the current completion. */
static void
append_ending_char(const enum comp_type ct)
{
	/* We only want the portion of the line before the cursor position. */
	const char cur_char = rl_line_buffer[rl_point];
	rl_line_buffer[rl_point] = '\0';

	char *lastword = get_last_input_word();
	if (!lastword)
		return;

	char deq_str[PATH_MAX];
	*deq_str = '\0';
	/* Clang static analysis complains that tmp[4] (deq_str[4]) is a garbage
	 * value. Initialize only this exact value to get rid of the warning. */
	deq_str[4] = '\0';
	size_t deq_str_len = 0;

	if (strchr(lastword, '\\'))
		deq_str_len = unescape_word(lastword, deq_str, sizeof(deq_str) - 1);

	char *tmp = *deq_str ? deq_str : lastword;

	const size_t len = tmp == deq_str ? deq_str_len : strlen(tmp);
	size_t is_file_uri = 0;

	if (*tmp == 'f' && tmp[1] == 'i' && IS_FILE_URI(tmp, len))
		is_file_uri = 1;

	char *name = tmp;
	char *p = is_file_uri == 0 ? normalize_path(tmp, len) : NULL;
	if (p)
		name = p;

	if (is_file_uri == 1)
		name += FILE_URI_PREFIX_LEN;

	struct stat attr;
	if (stat(name, &attr) != -1 && S_ISDIR(attr.st_mode)) {
		/* If not the root directory, append a slash. */
		if ((*name != '/' || name[1] || ct == TCMP_USERS))
			rl_insert_text("/");
	} else {
		if (rl_end == rl_point && ct != TCMP_OPENWITH && ct != TCMP_TAGS_T
		&& ct != TCMP_FILE_TYPES_OPTS && ct != TCMP_MIME_LIST)
			rl_stuff_char(' ');
	}

	/* Restore the character we removed to truncate the line at cursor position. */
	rl_line_buffer[rl_point] = cur_char;

	if (name == p)
		free(p);
}

/* Write the text BUF, at offset OFFSET, into STDOUT. */
static void
write_completion(char *buf, const size_t offset, const int multi)
{
	if (cur_comp_type == TCMP_TAGS_F)
		/* Needed in case the replacement string is shorter than the query
		 * string. Tagged files (TCMP_TAGS_F) is a possible case. We might
		 * need to consider other completion types as well. */
		ERASE_TO_RIGHT;

	const enum comp_type t = cur_comp_type;
	/* Remove ending new line char */
	char *n = strchr(buf, '\n');
	if (n)
		*n = '\0';

	if (t == TCMP_GLOB) {
		const size_t blen = strlen(buf);
		if (blen > 0 && buf[blen - 1] == '/')
			buf[blen - 1] = '\0';
		if (rl_line_buffer && *rl_line_buffer == '/' && rl_end > 0
		&& !strchr(rl_line_buffer + 1, '/')
		&& !strchr(rl_line_buffer + 1, ' ')) {
			rl_delete_text(0, rl_end);
			rl_end = rl_point = 0;
		}
	}

	if (t == TCMP_ENVIRON || t == TCMP_USERS) {
		/* Skip the leading dollar sign (env vars) and tilde (users) */
		rl_insert_text(buf + 1 + offset);

	} else if (t == TCMP_PATH && multi == 0) {
		char *esc_buf = escape_str(buf);
		if (esc_buf) {
			rl_insert_text(esc_buf + offset);
			free(esc_buf);
		} else {
			rl_insert_text(buf + offset);
		}

	} else if (t == TCMP_FILE_TYPES_OPTS || t == TCMP_MIME_LIST
	|| t == TCMP_BOOKMARK || t == TCMP_WORKSPACES || t == TCMP_NET
	|| t == TCMP_CSCHEME || t == TCMP_PROMPTS || t == TCMP_HIST
	|| t == TCMP_BACKDIR || t == TCMP_PROF || t == TCMP_BM_PREFIX
	|| t == TCMP_TAGS_T) {
		rl_insert_text(buf + offset);
		return;

	} else if (t == TCMP_OWNERSHIP) {
		rl_insert_text(buf + offset);
		if (rl_line_buffer && !strchr(rl_line_buffer, ':'))
			rl_stuff_char(':');
		return;

	} else {
		rl_insert_text(buf + offset);
	}

	append_ending_char(t);
}

/* Return a pointer to the beginning of the word right after the last
 * non-escaped slash in STR, or STR if none is found. */
static char *
get_last_word(char *str, const int original_query)
{
	char *ptr = str;
	char *word = NULL;

	while (*ptr) {
		if (ptr == str) {
			if (*ptr == '/')
				word = ptr;
		} else {
			if (*ptr == '/' && *(ptr - 1) != '\\')
				word = ptr;
		}
		ptr++;
	}

	char *w = !word ? str : (*word == '/' ? word + 1 : word);

	/* Remove ending backslash to avoid finder (fzf) error: no ending '"' */
	if (original_query == 1 && w) {
		char *bs = strrchr(w, '\\');
		if (bs && !bs[1])
			*bs = '\0';
	}

	return w;
}

/* We have an alternative preview file (set either via --shotgun-file,
 * --config-dir, or --profile).
 * Let's write the filename into an environment variable
 * (CLIFM_ALT_PREVIEW_FILE), so that the clifm instance invoked by fzf
 * can handle it. */
static void
setenv_fzf_alt_preview_file(void)
{
	static char buf[PATH_MAX + 1] = "";

	if (!*buf) {
		if (alt_preview_file && *alt_preview_file)
			snprintf(buf, sizeof(buf), "%s", alt_preview_file);
		else if (config_dir && *config_dir)
			snprintf(buf, sizeof(buf), "%s/preview.clifm", config_dir);
		else
			return;
	}

	setenv("CLIFM_ALT_PREVIEW_FILE", buf, 1);
}

static void
fix_x_y_values(int *x, int *y)
{
	switch (fzf_ext_border) {
	case FZF_BORDER_ROUNDED:   /* fallthrough */
	case FZF_BORDER_SHARP:     /* fallthrough */
	case FZF_BORDER_BOLD:      /* fallthrough */
	case FZF_BORDER_DOUBLE:    /* fallthrough */
	case FZF_BORDER_BLOCK:     /* fallthrough */
	case FZF_BORDER_THINBLOCK: /* fallthrough */
	case FZF_BORDER_HORIZ:     /* fallthrough */
	case FZF_BORDER_TOP: (*x)--; (*y)++; break;
	case FZF_BORDER_RIGHT: /* fallthrough */
	case FZF_BORDER_VERT: (*x)--; break;
/*	case FZF_BORDER_BOTTOM: // No coordinates correction required
	case FZF_BORDER_NONE:
	case FZF_BORDER_LEFT: */
	default: break;
	}
}

static void
set_fzf_env_vars(const int height)
{
	int col = 0;
	int line = 0;

	if (!(flags & PREVIEWER) && term_caps.req_cur_pos == 1) {
		get_cursor_position(&col, &line);
		if (line + height - 1 > term_lines)
			line -= ((line + height - 1) - term_lines);
	}

	/* Let's correct image coordinates on the screen based on the preview
	 * window style. */
	int x = term_cols;
	int y = line;

	switch (fzf_preview_border_type) {
	case FZF_BORDER_BOTTOM:    /* fallthrough */
	case FZF_BORDER_NONE:      /* fallthrough */
	case FZF_BORDER_LEFT:      /* fallthrough */
	case FZF_BORDER_RIGHT: break;
	case FZF_BORDER_TOP:       /* fallthrough */
	case FZF_BORDER_HORIZ: y += (flags & PREVIEWER) ? 2 : 1; break;
	case FZF_BORDER_BLOCK:     /* fallthrough */
	case FZF_BORDER_BOLD:      /* fallthrough */
	case FZF_BORDER_DOUBLE:    /* fallthrough */
	case FZF_BORDER_ROUNDED:   /* fallthrough */
	case FZF_BORDER_THINBLOCK: /* fallthrough */
	case FZF_BORDER_SHARP: y += (flags & PREVIEWER) ? 2 : 1; x -= 2; break;
	case FZF_BORDER_VERT: x -= 2; break;
	default: break;
	}

	if (flags & UEBERZUG_IMG_PREV)
		fix_x_y_values(&x, &y);

	char p[MAX_INT_STR];
	snprintf(p, sizeof(p), "%d", y > 0 ? y - 1 : 0);
	setenv("CLIFM_FZF_LINE", p, 1);
	snprintf(p, sizeof(p), "%d", x > 0 ? x : 0);
	setenv("CLIFM_TERM_COLUMNS", p, 1);
	snprintf(p, sizeof(p), "%d", term_lines);
	setenv("CLIFM_TERM_LINES", p, 1);

	if (thumbnails_dir && *thumbnails_dir) {
		setenv("CLIFM_THUMBNAILS_DIR", thumbnails_dir, 1);
		setenv("CLIFM_THUMBINFO_FILE", THUMBNAILS_INFO_FILE, 1);
	}

	if (conf.preview_max_size != UNSET) {
		snprintf(p, sizeof(p), "%d", conf.preview_max_size);
		setenv("CLIFM_PREVIEW_MAX_SIZE", p, 1);
	}

	if (flags & ALT_PREVIEW_FILE)
		setenv_fzf_alt_preview_file();
}

static void
clear_fzf(void)
{
	clear_term_img();
	unsetenv("CLIFM_FZF_LINE");
	unsetenv("CLIFM_TERM_COLUMNS");
	unsetenv("CLIFM_TERM_LINES");
	unsetenv("CLIFM_THUMBNAILS_DIR");
	unsetenv("CLIFM_THUMBINFO_FILE");
	if (conf.preview_max_size != UNSET)
		unsetenv("CLIFM_PREVIEW_MAX_SIZE");
	if (flags & ALT_PREVIEW_FILE)
		unsetenv("CLIFM_ALT_PREVIEW_FILE");
}

/* Calculate the available space for the fzf preview window based on
 * the main window width, terminal columns, and longest entry (including the
 * border type).
 * Return (size_t)-1 if the space is less than 50% of total space. */
static size_t
get_preview_win_width(const int offset)
{
	size_t w = 0;
	const size_t l = longest_prev_entry + 8 + (fzf_border_type <= 0 ? 0
		/* fzf_border_type == 1 (right or left): this takes 2 extra columns.
		 * fzf_border_type == 2 (rounded, sharp, bold, block, thinblock,
		 * double, or vertical): this takes 4 extra columns. */
		: (fzf_border_type == 1 ? 2 : 4));
	const int total_win_width = term_cols - offset;
	const size_t s_total_win_width = total_win_width < 0 ? 0
		: (size_t)total_win_width;

	if (l < s_total_win_width)
		w = s_total_win_width - l;

	if (w > s_total_win_width / 2)
		return w;

	return (size_t)-1;
}

/* Change to the trash directory so that we can generate file previews.
 * Only for the 'u' and 't del' commands. */
static int
cd_trashdir(const int prev)
{
#ifndef _NO_TRASH
	return (prev == 1 && (cur_comp_type == TCMP_UNTRASH
	|| cur_comp_type == TCMP_TRASHDEL) && trash_files_dir
	&& *trash_files_dir && trash_n > 0 && workspaces
	&& cur_ws >= 0 && workspaces[cur_ws].path
	&& xchdir(trash_files_dir, NO_TITLE) == 0);
#else
	UNUSED(prev);
	return 0;
#endif /* !_NO_TRASH */
}

static void
warn_fzf_error(void)
{
	fprintf(stderr, _("%s: Fzf failed. Check the FzfTabOptions "
		"line by running 'cs edit'\n(some option may not "
		"be supported by your fzf version).\n"), PROGRAM_NAME);
	press_any_key_to_continue(0);
	if (term_caps.suggestions == 1) {
		MOVE_CURSOR_UP(3);
		ERASE_TO_RIGHT_AND_BELOW;
		MOVE_CURSOR_DOWN(1);
	}
}

static int
run_finder(const size_t height, const int offset, const char *lw,
	const int multi)
{
	int prev = (conf.fzf_preview > 0 && SHOW_PREVIEWS(cur_comp_type) == 1)
		? FZF_INTERNAL_PREVIEWER : 0;

	const int restore_cwd = cd_trashdir(prev);
	const int prev_hidden = conf.fzf_preview == 2 ? 1 : 0;

	if (conf.fzf_preview == FZF_EXTERNAL_PREVIEWER)
		prev = FZF_EXTERNAL_PREVIEWER;

	/* Some shells, like xonsh (the only one to my knowledge), have problems
	 * parsing the command constructed below. Let's force launch_execl() to
	 * use "/bin/sh" to avoid this issue. */
	char *shell_bk = user.shell;
	user.shell = NULL;

	char height_str[10 + MAX_INT_STR];
	snprintf(height_str, sizeof(height_str), "--height=%zu", height);

	char cmd[(PATH_MAX * 2) + (NAME_MAX * 2)];

	if (tabmode == FNF_TAB) {
		snprintf(cmd, sizeof(cmd), "fnf "
			"--read-null --pad=%d --query='%s' --reverse "
			"--tab-accepts --right-accepts --left-aborts "
			"--lines=%zu %s %s < %s > %s",
			offset, lw ? lw : "", height,
			conf.colorize == 0 ? "--no-color" : "",
			multi == 1 ? "--multi" : "",
			finder_in_file, finder_out_file);

	} else if (tabmode == SMENU_TAB) {
		snprintf(cmd, sizeof(cmd), "smenu %s "
			"-t -d -n%zu -limits l:%d -W$'\n' %s < %s > %s",
			smenutab_options_env ? smenutab_options_env : DEF_SMENU_OPTIONS,
			height, PATH_MAX, multi == 1 ? "-P$'\n'" : "",
			finder_in_file, finder_out_file);

	} else { /* FZF */
		/* All fixed parameters are compatible with at least fzf 0.18.0 (Mar 31, 2019) */
		char prev_opts[18 + MAX_INT_STR];
		*prev_opts = '\0';
		const char prev_str[] = "--preview \"clifm --preview {}\"";

		if (prev > 0) { /* Either internal of external previewer */
			set_fzf_env_vars((int)height);
			const size_t s = get_preview_win_width(offset);
			if (s != (size_t)-1)
				snprintf(prev_opts, sizeof(prev_opts), "--preview-window=%zu", s);
		}

		snprintf(cmd, sizeof(cmd), "fzf %s %s "
			"%s --margin=0,0,0,%d "
			"%s --read0 --ansi "
			"--query='%s' %s %s %s %s %s "
			"< %s > %s",
			conf.fzftab_options,
			term_caps.unicode == 0 ? "--no-unicode" : "",
			*height_str ? height_str : "", offset,
			conf.case_sens_path_comp == 1 ? "+i" : "-i",
			lw ? lw : "", conf.colorize == 0 ? "--color=bw" : "",
			multi == 1 ? "--multi --bind tab:toggle+down,ctrl-s:select-all,\
ctrl-d:deselect-all,ctrl-t:toggle-all" : "",
			prev == FZF_INTERNAL_PREVIEWER ? prev_str : "",
			(prev == FZF_INTERNAL_PREVIEWER && prev_hidden == 1)
				? "--preview-window=hidden --bind alt-p:toggle-preview" : "",
			*prev_opts ? prev_opts : "",
			finder_in_file, finder_out_file);

		/* Skim is a nice alternative, but it currently (0.10.4) fails
		 * clearing the screen when --height is set, which makes it unusable
		 * for us. The issue has been reported, but there was no response.
		 * See https://github.com/lotabout/skim/issues/494
		 * As a workaround, run with --no-clear-start */
/*		snprintf(cmd, sizeof(cmd), "sk %s " // skim
			"%s %s --margin=0,0,0,%d "
			"--read0 --ansi "
			"--query=\"%s\" %s %s %s %s %s "
			"< %s > %s",
			conf.fzftab_options,
			*height_str ? height_str : "",
			*height_str ? "--no-clear-start" : "", offset,
			lw ? lw : "", conf.colorize == 0 ? "--no-color" : "",
			multi == 1 ? "--multi --bind tab:toggle+down,ctrl-s:select-all,\
ctrl-d:deselect-all,ctrl-t:toggle-all" : "",
			prev == 1 ? prev_str : "",
			(prev == 1 && prev_hidden == 1)
				? "--preview-window=hidden --bind alt-p:toggle-preview" : "",
			*prev_opts ? prev_opts : "",
			finder_in_file, finder_out_file); */
	}

	const int dr = (flags & DELAYED_REFRESH) ? 1 : 0;
	flags &= ~DELAYED_REFRESH;
	const mode_t old_mask = umask(0077); /* flawfinder: ignore */
	const int ret = launch_execl(cmd);
	umask(old_mask); /* flawfinder: ignore */

	if (restore_cwd == 1)
		xchdir(workspaces[cur_ws].path, NO_TITLE);

	/* Restore the user's shell to its original value. */
	user.shell = shell_bk;

	if (prev == FZF_INTERNAL_PREVIEWER)
		clear_fzf();
	if (dr == 1)
		flags |= DELAYED_REFRESH;
	if (ret == 2 && tabmode == FZF_TAB)
		warn_fzf_error();

	return ret;
}

/* Set FZF window's max height. No more than MAX HEIGHT entries will
 * be listed at once. */
static inline size_t
set_fzf_max_win_height(void)
{
	/* On some terminals, like lxterminal, urxvt, and vte, the number
	 * of terminal lines is not properly detected when first running
	 * the finder. So, let's update this value. */
	static int first_run = 0;
	if (first_run == 0) {
		get_term_size();
		first_run = 1;
	}

	return (size_t)(DEF_FZF_WIN_HEIGHT * term_lines / 100);
}

/* FILENAME is just a symlink name from the tags dir.
 * Let's get the target path. */
static char *
get_tagged_file_target(char *filename)
{
	if (!filename || !*filename)
		return NULL;

	char dir[PATH_MAX];
	char *p = NULL;
	if (strchr(filename, '\\'))
		p = unescape_str(filename, 0);

	snprintf(dir, sizeof(dir), "%s/%s/%s", tags_dir, cur_tag, p ? p : filename);
	free(p);

	char *rpath = xrealpath(dir, NULL);
	char *s = rpath ? rpath : filename;
	int free_tmp = 0;
	char *q = home_tilde(s, &free_tmp);
	if (q && free_tmp == 1 && s == rpath)
		free(s);

	return q ? q : s;
}

static char *
print_no_finder_file(void)
{
	err('e', PRINT_PROMPT, "%s: %s: %s\n", PROGRAM_NAME,
		finder_out_file, strerror(errno));
	return NULL;
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
get_glob_file_target(char *str, const char *initial_path)
{
	if (!str || !*str)
		return NULL;

	if (*str == '/' || !initial_path)
		return str;

	const size_t len = strlen(initial_path) + strlen(str) + 1;
	char *p = xnmalloc(len, sizeof(char));
	snprintf(p, len, "%s%s", initial_path, str);

	return p;
}

/* Recover finder (fzf/fnf/smenu) output from FINDER_OUT_FILE file.
 * Return this output (reformatted if needed) or NULL in case of error.
 * FINDER_OUT_FILE is removed immediately after use.  */
static char *
get_finder_output(const int multi, char *base)
{
	int fd = 0;
	FILE *fp = open_fread(finder_out_file, &fd);
	if (!fp) {
		unlinkat(XAT_FDCWD, finder_out_file, 0);
		return print_no_finder_file();
	}

	char *buf = xnmalloc(1, sizeof(char));
	*buf = '\0';
	const char *initial_path =
		(cur_comp_type == TCMP_GLOB) ? base : NULL;

	char *line = NULL;
	size_t bsize = 0, line_size = 0;
	ssize_t line_len = 0;

	while ((line_len = getline(&line, &line_size, fp)) > 0) {
		if (line[line_len - 1] == '\n') {
			line_len--;
			line[line_len] = '\0';
		}

		if (cur_comp_type == TCMP_FILE_TYPES_OPTS && *line && line[1]) {
			line[1] = '\0';
			line_len = 1;
		}

		if (cur_comp_type == TCMP_CMD_DESC && *line) {
			char *p = strchr(line, ' ');
			if (p) {
				*p = '\0';
				line_len = (ssize_t)strlen(line);
			}
		}

		char *q = line;
		if (multi == 1) {
			char *s = line;
			if ((flags & PREVIEWER) && workspaces[cur_ws].path) {
				char f[PATH_MAX];
				snprintf(f, sizeof(f), "%s/%s", workspaces[cur_ws].path, s);
				select_file(f);
				continue;
			} else if (cur_comp_type == TCMP_GLOB) {
				s = get_glob_file_target(line, initial_path);
			} else if (cur_comp_type == TCMP_TAGS_F && tags_dir && cur_tag) {
				s = get_tagged_file_target(line);
			} else if (cur_comp_type == TCMP_BM_PREFIX) {
				const size_t len = strlen(line) + 3;
				s = xnmalloc(len, sizeof(char));
				snprintf(s, len, "b:%s", line);
			} else if (cur_comp_type == TCMP_TAGS_T) {
				const size_t len = strlen(line) + 3;
				s = xnmalloc(len, sizeof(char));
				snprintf(s, len, "t:%s", line);
			}

			q = escape_str(s);
			if (s != line)
				free(s);
			if (!q)
				continue;
		}

		/* We don't want to quote the initial tilde */
		const char *r = q;
		if (*r == '\\' && *(r + 1) == '~')
			r++;

		const size_t qlen = (r != line) ? strlen(r) : (size_t)line_len;
		bsize += qlen + 3;
		buf = xnrealloc(buf, bsize, sizeof(char));
		xstrncat(buf, strlen(buf), r, bsize);

		if (multi == 1) {
			const size_t l = strlen(buf);
			buf[l] = ' ';
			buf[l + 1] = '\0';
			free(q);
		}
	}

	free(line);
	unlinkat(XAT_FDCWD, finder_out_file, 0);
	fclose(fp);

	if (*buf == '\0') {
		free(buf);
		buf = NULL;
	}

	return buf;
}

/* Return a normalized (absolute) path for the query string PREFIX.
 * E.g., "./b<TAB>" -> /parent/dir
 * The partially typed basename, here 'b', is excluded, since it will be added
 * later using the list of matches passed to store_completions(). */
static char *
normalize_prefix(char *prefix)
{
	char *s = strrchr(prefix, '/');
	if (s && s != prefix && s[1])
		*s = '\0';

	char *norm_prefix = normalize_path(prefix, strlen(prefix));

	if (s)
		*s = '/';

	return norm_prefix;
}

/* Store possible completions (MATCHES) in FINDER_IN_FILE to pass them to the
 * finder, either FZF or FNF.
 * Return the number of stored matches. */
static size_t
store_completions(char **matches)
{
	int fd = 0;
	FILE *fp = open_fwrite(finder_in_file, &fd);
	if (!fp) {
		err('e', PRINT_PROMPT, "%s: %s: %s\n", PROGRAM_NAME,
			finder_in_file, strerror(errno));
		return (size_t)-1;
	}

	const enum comp_type ct = cur_comp_type;

	const int no_file_comp = (ct == TCMP_TAGS_S || ct == TCMP_TAGS_U
		|| ct == TCMP_SORT || ct == TCMP_BOOKMARK || ct == TCMP_CSCHEME
		|| ct == TCMP_NET || ct == TCMP_PROF || ct == TCMP_PROMPTS
		|| ct == TCMP_BM_PREFIX || ct == TCMP_WS_PREFIX
		|| ct == TCMP_WORKSPACES);
			/* We're not completing filenames. */

	char *norm_prefix = NULL;
	/* "./_", "../_", and "_/.._" */
	if (ct == TCMP_PATH && ((*matches[0] == '.' && (matches[0][1] == '/'
	|| (matches[0][1] == '.' && matches[0][2] == '/')))
	|| strstr(matches[0], "/..")))
		norm_prefix = normalize_prefix(matches[0]);

	size_t i;
	/* 'view' cmd with only one match: matches[0]. */
	const size_t start = ((flags & PREVIEWER) && !matches[1]) ? 0 : 1;
	const int prev = (conf.fzf_preview > 0 && SHOW_PREVIEWS(ct) == 1);
	const char end_char = tabmode == SMENU_TAB ? '\n' : '\0';
	longest_prev_entry = 0;

#ifndef _NO_TRASH
	/* Change to the trash dir so we can correctly get trashed files color. */
	if (conf.colorize == 1 && (ct == TCMP_TRASHDEL || ct == TCMP_UNTRASH)
	&& trash_files_dir)
		xchdir(trash_files_dir, NO_TITLE);
#endif /* _NO_TRASH */

	for (i = start; matches[i]; i++) {
		if (!matches[i] || !*matches[i] || SELFORPARENT(matches[i]))
			continue;

		const char *color = df_c;
		char *entry = matches[i];

		if (prev == 1) {
			const int get_base_name = ((ct == TCMP_PATH || ct == TCMP_GLOB)
				&& !(flags & PREVIEWER));
			const char *p = get_base_name == 1 ? strrchr(entry, '/') : NULL;
			const size_t len = strlen((p && p[1]) ? p + 1 : entry);
			if (len > longest_prev_entry)
				longest_prev_entry = len;
		}

		if (ct == TCMP_BACKDIR) {
			color = di_c;
		} else if (ct == TCMP_TAGS_T || ct == TCMP_BM_PREFIX) {
			color = mi_c;
			if (entry[2])
				entry += 2;
		} else if (ct == TCMP_TAGS_C) {
			color = mi_c;
			if (entry[1])
				entry += 1;
		} else if (no_file_comp == 1) {
			color = mi_c;

		} else if (ct != TCMP_HIST && ct != TCMP_FILE_TYPES_OPTS
		&& ct != TCMP_MIME_LIST && ct != TCMP_CMD_DESC) {
			char *cl = get_comp_entry_color(entry, norm_prefix);
			*tmp_color = '\0';

			/* If color does not start with escape, then we have a color
			 * for a file extension. In this case, we need to properly
			 * construct the color code. */
			if (cl && *cl != KEY_ESC)
				snprintf(tmp_color, sizeof(tmp_color), "\x1b[%sm", cl);

			color = *tmp_color ? tmp_color : (cl ? cl : "");

			if (ct != TCMP_SEL && ct != TCMP_DESEL && ct != TCMP_BM_PATHS
			&& ct != TCMP_DIRHIST && ct != TCMP_OPENWITH
			&& ct != TCMP_JUMP && ct != TCMP_TAGS_F) {
				char *ptr = strrchr(entry, '/');
				entry = (ptr && *(++ptr)) ? ptr : entry;
			}
		}

		if (*entry)
			fprintf(fp, "%s%s%s%c", color, entry, NC, end_char);
	}

#ifndef _NO_TRASH
	/* We changed to the trash dir. Change back to the current dir. */
	if (conf.colorize == 1 && (ct == TCMP_TRASHDEL || ct == TCMP_UNTRASH)
	&& workspaces && workspaces[cur_ws].path)
		xchdir(workspaces[cur_ws].path, NO_TITLE);
#endif /* _NO_TRASH */

	fclose(fp);
	free(norm_prefix);
	return i;
}

static char *
get_query_str(char *lw)
{
	char *query = NULL;
	char *lb = rl_line_buffer;
	char *tmp = NULL;

	switch (cur_comp_type) {
	/* These completions take an empty query string */
	case TCMP_RANGES:           /* fallthrough */
	case TCMP_SEL:              /* fallthrough */
	case TCMP_BM_PATHS:         /* fallthrough */
	case TCMP_GLOB:             /* fallthrough */
	case TCMP_CMD_DESC:         /* fallthrough */
	case TCMP_FILE_TYPES_OPTS:  /* fallthrough */
	case TCMP_FILE_TYPES_FILES: /* fallthrough */
	case TCMP_MIME_LIST:        /* fallthrough */
	case TCMP_TAGS_F:
		break;

	case TCMP_TAGS_T: /* fallthrough */
	case TCMP_WS_PREFIX: /* fallthrough */
	case TCMP_BM_PREFIX:
		query = (lw && *lw && lw[1] && lw[2]) ? lw + 2 : NULL;
		break;

	case TCMP_FILE_TEMPLATES: /* fallthrough */
	case TCMP_DESEL:
		tmp = lb ? strrchr(lb, cur_comp_type == TCMP_DESEL ? ' ' : '@')
			: NULL;
		query = (!tmp || !*(tmp++)) ? NULL : tmp;
		break;

	case TCMP_TAGS_C:
		query = (lw && *lw && lw[1]) ? lw + 1 : NULL;
		break;

	case TCMP_DIRHIST:
		if (lb && *lb && lb[1] && lb[2] && lb[3])
			query = lb + 3;
		break;

	case TCMP_OWNERSHIP:
		tmp = lb ? strchr(lb, ':') : NULL;
		query = !tmp ? lb : ((*tmp && tmp[1]) ? tmp + 1 : NULL);
		break;

	case TCMP_HIST:
		if (lb && *lb == '/' && lb[1] == '*') { /* Search history */
			query = lb + 1;
		} else { /* Commands history */
			tmp = lb ? strrchr(lb, '!') : NULL;
			query = (!tmp || !*(tmp++)) ? NULL : tmp;
		}
		break;

	case TCMP_JUMP:
		if (lb && *lb == 'j' && lb[1] == ' ') {
			query = lb[2] ? lb + 2 : NULL;
		} else {
			tmp = lb ? strstr(lb, "j ") : NULL;
			query = (tmp && *tmp && tmp[1] && tmp[2])
				? tmp + 2 : NULL;
		}
		break;

	default: query = lw; break;
	}

	return query;
}

/* Return the number of characters that need to be quoted/escaped in the
 * string STR, whose length is LEN. */
static size_t
count_quote_chars(const char *str, const size_t len)
{
	size_t n = 0;

	for (size_t i = len; i-- > 0;) {
		if (is_quote_char(str[i]))
			n++;
	}

	return n;
}

/* Calculate the length of the matching prefix to insert into the line
 * buffer only the non-matched part of the string returned by the finder. */
static size_t
calculate_prefix_len(const char *str, const char *query, const char *lw)
{
	const enum comp_type ct = cur_comp_type;

	if (ct == TCMP_FILE_TYPES_OPTS || ct == TCMP_SEL || ct == TCMP_RANGES
	|| ct == TCMP_TAGS_T || ct == TCMP_BM_PREFIX || ct == TCMP_HIST
	|| ct == TCMP_JUMP || ct == TCMP_BM_PATHS || ct == TCMP_TAGS_F
	|| ct == TCMP_GLOB || ct == TCMP_DIRHIST
	|| ct == TCMP_FILE_TYPES_FILES || ct == TCMP_CMD_DESC)
		/* None of these completions produces a partial match (prefix) */
		return 0;

	if (ct == TCMP_FILE_TEMPLATES)
		/* Insert the entire match (to honor case insensitive query) */
		return 0;

	size_t prefix_len = 0;
	const size_t len = strlen(str);

	if (len == 0 || str[len - 1] == '/') {
		if (ct != TCMP_DESEL)
			return 0;
	}

	if (ct == TCMP_DESEL)
		return strlen(query ? query : (lw ? lw : ""));

	if (ct == TCMP_OWNERSHIP) {
		char *p = rl_line_buffer ? strchr(rl_line_buffer, ':') : NULL;
		if (p)
			return (*(++p) ? wc_xstrlen(p) : 0);

		return (rl_line_buffer ? wc_xstrlen(rl_line_buffer) : 0);
	}

	const char *q = strrchr(str, '/');
	if (q) {
		const size_t qlen = strlen(q);
		if (ct == TCMP_PATH) {
			/* Add backslashes to the length of the match: every quoted char
			 * will be escaped later by write_completion(), so that backslashes
			 * should be counted as well to get the right offset. */
			prefix_len = qlen - 1 + count_quote_chars(q, qlen);
		} else {
			prefix_len = qlen + 1;
		}
	} else { /* We have just a name, no slash. */
		if (ct == TCMP_PATH || ct == TCMP_WORKSPACES || ct == TCMP_CSCHEME
		|| ct == TCMP_NET || ct == TCMP_PROMPTS || ct == TCMP_BOOKMARK) {
			prefix_len = len + count_quote_chars(str, len);
		} else if (ct == TCMP_TAGS_C) {
			prefix_len = len - 1;
		} else {
			prefix_len = len;
		}
	}

	return prefix_len;
}

/* Let's define whether we have a case which allows multi-selection
 * Returns 1 if true or zero if false. */
static int
is_multi_sel(void)
{
	const enum comp_type t = cur_comp_type;

	if (t == TCMP_SEL || t == TCMP_DESEL || t == TCMP_RANGES
	|| t == TCMP_TRASHDEL || t == TCMP_UNTRASH || t == TCMP_TAGS_F
	|| t == TCMP_TAGS_U || (flags & MULTI_SEL))
		return 1;

	if (!rl_line_buffer)
		return 0;

	const char *l = rl_line_buffer;
	const char *lws = get_last_chr(rl_line_buffer, ' ', rl_point);

	/* Do not allow multi-sel if we have a path, only filenames */
	if (t == TCMP_PATH && *l != '/' && (!lws || !strchr(lws, '/'))) {
		if (
		/* Select */
		(*l == 's' && (l[1] == ' ' || strncmp(l, "sel ", 4) == 0))
		/* Trash */
		|| (*l == 't' && (l[1] == ' ' || strncmp(l, "trash ", 6) == 0))
		/* ac and ad */
		|| (*l == 'a' && ((l[1] == 'c' || l[1] == 'd') && l[2] == ' '))
		/* bb, bl, and br */
		|| (*l == 'b' && ((l[1] == 'b' || l[1] == 'l' || l[1] == 'r') && l[2] == ' '))
		/* r */
		|| (*l == 'r' && l[1] == ' ')
		/* d/dup */
		|| (*l == 'd' && (l[1] == ' ' || strncmp(l, "dup ", 4) == 0))
		/* Properties */
		|| (*l == 'p' && (l[1] == ' ' || strncmp (l, "prop ", 5) == 0))
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

/* Clean the input buffer in case the user cancelled the completion pressing ESC.
 * If all possible completions share a common prefix, this prefix is
 * automatically appended to the query string. However, the user cancelled
 * here the completion (pressing ESC), so that we need to remove this prefix
 * by reinserting the original query string. */
static int
clean_rl_buffer(const char *text)
{
	if (!text || !*text)
		return FUNC_FAILURE;

	if (rl_point != rl_end)
		return FUNC_SUCCESS;

	const char *lb = rl_line_buffer;
	/* If the previous char is not space, then a common prefix was appended:
	 * remove it. */
	if ((rl_end > 0 && lb && lb[rl_end - 1] != ' ')
	|| (rl_end >= 2 && lb && lb[rl_end - 2] == '\\')) {
		/* Find the last non-escaped space. */
		int i = rl_end, sp = -1;
		while (--i >= 0) {
			if (lb[i] == ' ' && i > 0 && lb[i - 1] != '\\') {
				sp = i;
				break;
			}
		}

		if (sp != -1) {
			/* If found, remove the content of the input line starting
			 * exactly one char after the last space. */
			rl_delete_text(sp + 1, rl_end);
			rl_point = rl_end = sp + 1;
		} else { /* No space: delete the entire line. */
			rl_delete_text(0, rl_end);
			rl_point = rl_end = 0;
		}

		ERASE_TO_RIGHT_AND_BELOW;
	}

	rl_insert_text(text);

	return FUNC_FAILURE;
}

/* Calculate the offset (left padding) of the finder's window based on
 * the cursor position, the current query string, and the completion type. */
static int
get_finder_offset(const char *query, const char *text, char **matches,
	const char *lw, size_t *height, int *total_line_len)
{
	const enum comp_type ct = cur_comp_type;
	char *lb = rl_line_buffer;
	int finder_offset = 0;

	 /* We don't want to place the finder's window too much to the right,
	 * making its contents unreadable: let's make sure we have at least
	 * 20 chars (40 if previews are enabled) for the finder's window. */
	const int fspace = (tabmode == FZF_TAB && conf.fzf_preview == 1
		&& SHOW_PREVIEWS(ct) == 1) ? 40 : 20;

	/* If showing previews, let's reserve at least a quarter of the
	 * terminal height. */
	const int min_prev_height = term_lines / 4;
	if (fspace == 40 && (int)*height < min_prev_height
	&& min_prev_height > 0 && fzf_height_value == 0)
		/* fspace == 40: We're previewing files. */
		*height = (size_t)min_prev_height;

	const int max_finder_offset = term_cols > fspace ? term_cols - fspace : 0;

	int diff = 0;
	if (rl_end > rl_point)
		diff = (int)wc_xstrlen(lb + rl_point);

	const int cur_line_len = lb ? (int)wc_xstrlen(lb) - diff : 0;
	*total_line_len = cur_line_len + prompt_offset;

	int last_line_len = cur_line_len;
	while (last_line_len > term_cols)
		last_line_len -= term_cols;

	finder_offset = last_line_len + prompt_offset < max_finder_offset
		? (last_line_len + prompt_offset - 4) : 0;
	/* PROMPT_OFFSET (the space used by the prompt in the current line)
	 * is calculated the first time we print the prompt (in my_rl_getc()
	 * (readline.c)). */

	if (text && conf.fuzzy_match == 1 && ct != TCMP_TAGS_F
	&& ct != TCMP_DIRHIST) {
		/* TEXT is not NULL whenever a common prefix was added, replacing
		 * the original query string. */
		const size_t bslashes = count_chars(text, '\\');
		finder_offset -=
			(int)(wc_xstrlen(matches[0]) - wc_xstrlen(text) + bslashes);
	}

	if (!lw) {
		finder_offset++;
	} else {
		const size_t lw_len = wc_xstrlen(lw);
		if (lw_len > 1) {
			finder_offset -= (int)(lw_len - 1);
			if (finder_offset < 0)
				finder_offset = 0;
		}
	}

	if (ct == TCMP_OWNERSHIP && query) {
		if (lb && query == lb)
			finder_offset = (int)wc_xstrlen(lb) - 3;
		else
			finder_offset = lb ? (int)(query - lb) : 0;
	}

	else if ((ct == TCMP_DESEL || ct == TCMP_FILE_TEMPLATES) && query) {
		finder_offset = prompt_offset + (int)(query - lb) - 3;
		if (!*query && finder_offset > 0)
			finder_offset--;
	}

	else if (ct == TCMP_HIST) {
		if (lb && *lb == '/' && lb[1] == '*') { /* Search history */
			finder_offset = 1 + prompt_offset - ((query && *query) ? 3 : 4);
		} else { /* Commands history */
			const char *sp = lb ? get_last_chr(lb, '!', rl_point) : NULL;
			finder_offset = prompt_offset + (sp ? (int)(sp - lb)
				- ((query && *query) ? 2 : 3) : -1);
		}
	}

	else if (ct == TCMP_JUMP) {
		if (query) {
			if (lb && *lb == 'j' && lb[1] == ' ') {
				/* The command is "j" */
				finder_offset = prompt_offset - 1;
			} else {
				/* The command is "jump" */
				finder_offset = prompt_offset + 2;
			}
		} else {
			finder_offset = prompt_offset + ((lb && *lb) ?
				(lb[1] == ' ' ? -2 : 1) : -2);
		}
	}

	else if (ct == TCMP_TAGS_F) {
		if (rl_end > 0 && lb && *lb == 't' && lb[1] == 'u'
		&& lb[rl_end - 1] == ' ') {
			/* Coming from untag ('tu :TAG ') */
			finder_offset++;
		} else { /* Coming from tag expression ('t:FULL_TAG') */
			const char *sp = lb ? get_last_chr(lb, ' ', rl_point) : NULL;
			finder_offset = prompt_offset + (sp ? (int)(sp - lb): -1);
		}
	}

	else if (ct == TCMP_DIRHIST) {
		finder_offset = prompt_offset;
	}

	else if (ct == TCMP_FILE_TYPES_OPTS || ct == TCMP_MIME_LIST) {
		finder_offset++;
	}

	else if (ct == TCMP_FILE_TYPES_FILES) {
		const char *sp = lb ? get_last_chr(lb, ' ', rl_point) : NULL;
		if (sp) /* Expression is second or more word: "text =FILE_TYPE" */
			finder_offset = prompt_offset + (int)(sp - lb) - 1;
		else /* Expression is first word: "=FILE_TYPE" */
			finder_offset = prompt_offset - 2;
	}

	else if (ct == TCMP_SEL || ct == TCMP_RANGES) {
		const char *sp = lb ? get_last_chr(lb, ' ', rl_point) : NULL;
		finder_offset = prompt_offset + (sp ? (int)(sp - lb) - 2 : -(rl_end + 1));
	}

	else if (ct == TCMP_BM_PATHS) {
		const char *sp = lb ? get_last_chr(lb, ' ', rl_point) : NULL;
		finder_offset = prompt_offset + (sp ? (int)(sp - lb) - 2 : -3);
	}

	else if (ct == TCMP_TAGS_C) {
		const char *sp = lb ? strrchr(lb, ' ') : NULL;
		finder_offset = prompt_offset + (sp ? (int)(sp - lb) - 1 : 0);
	}

	else if (ct == TCMP_BM_PREFIX || ct == TCMP_TAGS_T) {
		const char *sp = lb ? get_last_chr(lb, ' ', rl_point) : NULL;
		finder_offset = prompt_offset + (sp ? (int)(sp - lb): -1);
	}

	else if (ct == TCMP_GLOB) {
		const char *sl = lb ? get_last_chr(lb, '/', rl_point) : NULL;
		const char *sp = lb ? get_last_chr(lb, ' ', rl_point) : NULL;
		if (!sl) {
			if (sp)
				finder_offset = prompt_offset + (int)(sp - lb) - 2;
			else /* Neither space nor slash == first word */
				finder_offset = prompt_offset - 3;
		} else {
			if (sp && sp > sl)
				finder_offset = prompt_offset + (int)(sp - lb) - 2;
			else
				finder_offset = prompt_offset + (int)(sl - lb) - 2;
		}
	}

	if ((!query || !*query) && ct != TCMP_RANGES && ct != TCMP_SEL
	&& ct != TCMP_BM_PATHS && ct != TCMP_BM_PREFIX && ct != TCMP_GLOB
	&& ct != TCMP_CMD_DESC && ct != TCMP_FILE_TYPES_OPTS
	&& ct != TCMP_FILE_TYPES_FILES && ct != TCMP_MIME_LIST
	&& ct != TCMP_TAGS_F && ct != TCMP_TAGS_T && ct != TCMP_TAGS_C
	&& ct != TCMP_DIRHIST && ct != TCMP_WS_PREFIX)
		finder_offset++; /* Last char is space */

	while (finder_offset > term_cols)
		finder_offset -= term_cols;

	if (finder_offset > max_finder_offset || finder_offset < 0)
		finder_offset = 0;

	if (finder_offset == 0) {
		/* Not enough space to align the window with the last word. Let's
		 * try to align it with the prompt. If not enough space either, send
		 * the window to the leftmost side of the screen. */
		finder_offset = prompt_offset >= 3 ? prompt_offset - 3 : prompt_offset;
		if (finder_offset > max_finder_offset)
			finder_offset = 0;
	}

	return finder_offset;
}

/* Depending on the completion type, either the typed text (input buffer), or
 * the text to be inserted, needs to be modified. Let's do this here. */
static void
do_some_cleanup(char **buf, char **matches, const char *query,
	size_t *prefix_len)
{
	const enum comp_type ct = cur_comp_type;
	char *lb = rl_line_buffer;

	/* TCMP_HIST may be either search or command history */
	const int cmd_hist = (ct == TCMP_HIST && rl_line_buffer
		&& (*lb != '/' || lb[1] != '*'));

	if (rl_point < rl_end && ct != TCMP_PATH && ct != TCMP_CMD) {
		const char *s = lb ? get_last_chr(lb, ' ', rl_point) : NULL;
		const int start = s ? (int)(s - lb + 1) : 0;
		rl_delete_text(start, rl_point);
		rl_point = start;
	}

	else if (ct == TCMP_OPENWITH && strchr(*buf, ' ')) {
		/* We have multiple words ("CMD ARG..."): quote the string. */
		const size_t len = strlen(*buf) + 3;
		char *tmp = xnmalloc(len, sizeof(char));
		snprintf(tmp, len, "\"%s\"", *buf);
		free(*buf);
		*buf = tmp;
	}

	else if (ct == TCMP_HIST && cmd_hist == 0) {
		rl_delete_text(0, rl_end);
		rl_point = rl_end = 0;
	}

	else if (ct == TCMP_JUMP) {
		if (*lb == 'j' && lb[1] == ' ' && lb[2]) {
			rl_delete_text(2, rl_end);
			rl_point = rl_end = 2;
		} else {
			const char *tmp = strstr(lb, "j ");
			if (tmp && tmp[1] && tmp[2]) {
				rl_point = (int)(tmp - lb + 2);
				rl_delete_text(rl_point, rl_end);
				rl_end = rl_point;
			}
		}
	}

	else if (cmd_hist == 1 || ct == TCMP_RANGES || ct == TCMP_SEL
	|| ct == TCMP_TAGS_F || ct == TCMP_GLOB
	|| ct == TCMP_BM_PATHS || ct == TCMP_BM_PREFIX
	|| ct == TCMP_TAGS_T || ct == TCMP_DIRHIST) {
		const char *s = lb ? get_last_chr(lb,
			(ct == TCMP_GLOB && words_num == 1)
			? '/' : ' ', rl_end) : NULL;
		if (s) {
			rl_point = (int)(s - lb + 1);
			rl_delete_text(rl_point, rl_end);
			rl_end = rl_point;
		/* Completions allowed for the first word. */
		} else if (cmd_hist == 1 || ct == TCMP_BM_PATHS || ct == TCMP_TAGS_F
		|| ct == TCMP_BM_PREFIX || ct == TCMP_TAGS_T || ct == TCMP_RANGES
		|| ct == TCMP_SEL || ct == TCMP_DIRHIST || ct == TCMP_GLOB) {
			rl_delete_text(0, rl_end);
			rl_end = rl_point = 0;
		}
	}

	else if (ct == TCMP_FILE_TYPES_FILES || ct == TCMP_CMD_DESC
	|| ct == TCMP_FILE_TEMPLATES) {
		const char *s = lb ? get_last_chr(lb,
			ct == TCMP_FILE_TEMPLATES ? '@' : ' ', rl_end) : NULL;
		rl_point = !s ? 0 : (int)(s - lb + 1);
		rl_delete_text(rl_point, rl_end);
		rl_end = rl_point;
		ERASE_TO_RIGHT;
		fflush(stdout);
	}

	else if (ct == TCMP_USERS) {
		const size_t l = strlen(*buf);
		char *p = savestring(*buf, l);
		*buf = xnrealloc(*buf, (l + 2), sizeof(char));
		snprintf(*buf, l + 2, "~%s", p);
		free(p);
	}

	else {
		/* Honor case insensitive completion/fuzzy matches. */
		if ((conf.case_sens_path_comp == 0 || conf.fuzzy_match == 1)
		&& query && strncmp(matches[0], *buf, *prefix_len) != 0) {
			const int bk = rl_point;
			rl_delete_text(bk - (int)*prefix_len, rl_end);
			rl_point = rl_end = bk - (int)*prefix_len;
			*prefix_len = 0;
		}
	}
}

static int
do_completion(char *buf, const size_t prefix_len, const int multi)
{
	/* Some further buffer clean up: remove new line char and ending spaces. */
	const size_t blen = strlen(buf);
	int j = blen > INT_MAX ? INT_MAX : (int)blen;

	if (j > 0 && buf[j - 1] == '\n') {
		j--;
		buf[j] = '\0';
	}

	while (--j >= 0 && buf[j] == ' ')
		buf[j] = '\0';

	char *p = NULL;
	if (cur_comp_type != TCMP_OPENWITH && cur_comp_type != TCMP_PATH
	&& cur_comp_type != TCMP_HIST && !multi) {
		p = escape_str(buf);
		if (!p)
			return FUNC_FAILURE;
	} else {
		p = savestring(buf, blen);
	}

	write_completion(p, prefix_len, multi);
	free(p);

	return FUNC_SUCCESS;
}

/* Calculate currently used lines to go back to the correct cursor
 * position after quitting the finder. */
static void
move_cursor_up(const int total_line_len)
{
	int lines = 1;

	if (total_line_len > term_cols && term_cols > 0) {
		lines = total_line_len / term_cols;
		const int rem = (int)total_line_len % term_cols;
		if (rem > 0)
			lines++;
	}

	MOVE_CURSOR_UP(lines);
}

/* Determine input and output files to be used by the fuzzy finder (either
 * fzf or fnf).
 * Let's do this even if fzftab is not enabled at startup, because this feature
 * can be enabled in place by editing the config file.
 *
 * NOTE: Why do the random file extensions have different lengths? If
 * compiled in POSIX mode, gen_rand_ext() uses srandom(3) and random(3) to
 * generate the string, but since both extensions are generated at the same
 * time, they happen to be the same, resulting in both filenames being
 * identical. As a workaround, we use different lengths for both extensions.
 *
 * These files are created immediately after this function returns by
 * store_completions() with permissions 600 (in a directory to which only the
 * current user has read/write access). These files are then immediately read
 * by the finder application, and deleted as soon as this latter returns. */
static void
set_finder_paths(void)
{
	const int sm = (xargs.stealth_mode == 1);
	const char *p = sm ? P_tmpdir : tmp_dir;

	char *rand_ext = gen_rand_str(RAND_SUFFIX_LEN + (sm ? 6 : 0));
	snprintf(finder_in_file, sizeof(finder_in_file), "%s/.temp%s",
		p, rand_ext ? rand_ext : "a3_2yu!d43");
	free(rand_ext);

	rand_ext = gen_rand_str(RAND_SUFFIX_LEN + (sm ? 10 : 4));
	snprintf(finder_out_file, sizeof(finder_out_file), "%s/.temp%s",
		p, rand_ext ? rand_ext : "0rNkds7++@");
	free(rand_ext);
}

/* Display possible completions using the corresponding finder. If one of these
 * possible completions is selected, insert it into the current line buffer.
 *
 * What is ORIGINAL_QUERY and why do we need it?
 * MATCHES[0] is supposed to hold the common prefix among all possible
 * completions. This common prefix should be the same as the query string when
 * performing regular matches. But it might not be the same as the
 * original query string when performing fuzzy match: then, we need a copy of
 * this original query string (ORIGINAL_QUERY) to later be passed to FZF.
 * Example:
 * Query string: '.f'
 * Returned matches (fuzzy):
 *   .file
 *   .this_file
 *   .beef
 * Common preffix: '.' (not '.f')
 * */
static int
finder_tabcomp(char **matches, const char *text, char *original_query)
{
	/* Set random filenames for both FINDER_IN_FILE and FINDER_OUT_FILE. */
	set_finder_paths();

	/* Store possible completions in FINDER_IN_FILE to pass them to the finder. */
	const size_t num_matches = store_completions(matches);
	if (num_matches == (size_t)-1)
		return FUNC_FAILURE;

	/* Set a pointer to the last word in the query string. We use this to
	 * highlight the matching prefix in the list of matches. */
	char *lw = get_last_word(original_query ? original_query : matches[0],
		original_query ? 1 : 0);

	char *query = get_query_str(lw);

	/* If not already defined (environment or config file), calculate the
	 * height of the finder's window based on the number of entries. This
	 * specifies how many entries will be displayed at once. */
	size_t height = 0;

	if (fzf_height_value > 0) {
		/* Height was set either in FZF_DEFAULT_OPTS or in the color scheme
		 * file. Let's respect this value. */
		height = (size_t)fzf_height_value;
	} else {
		const size_t max_height = set_fzf_max_win_height();
		height = (num_matches + 1 > max_height) ? max_height : num_matches;
	}

	int total_line_len = 0;
	int finder_offset = get_finder_offset(query, text, matches, lw,
		&height, &total_line_len);

	/* Tab completion cases allowing multi-selection. */
	int multi = is_multi_sel();

	char *q = query;
	if (flags & PREVIEWER) {
		height = term_lines;
		finder_offset = 0;
		multi = 1;
		q = NULL;
	}

	char *deq = q ? (strchr(q, '\\') ? unescape_str(q, 0) : q) : NULL;

	/* Run the finder application and store the ouput in FINDER_OUT_FILE. */
	const int ret = run_finder(height, finder_offset, deq, multi);

	if (deq && deq != q)
		free(deq);

	unlinkat(XAT_FDCWD, finder_in_file, 0);

	if (!(flags & PREVIEWER))
		move_cursor_up(total_line_len);

	/* No results (the user pressed ESC or the Left arrow key). */
	if (ret != FUNC_SUCCESS) {
		unlinkat(XAT_FDCWD, finder_out_file, 0);
		return clean_rl_buffer(text);
	}

	/* Retrieve finder's output from FINDER_OUT_FILE. */
	char *buf = get_finder_output(multi, matches[0]);
	if (!buf)
		return FUNC_FAILURE;

	/* Calculate the length of the matching prefix to insert into the
	 * line buffer only the non-matched part of the string returned by the
	 * finder. */
	size_t prefix_len = calculate_prefix_len(matches[0], query, lw);

	do_some_cleanup(&buf, matches, query, &prefix_len);

	/* Let's insert the selected match(es): BUF. */
	if (buf && *buf && do_completion(buf, prefix_len, multi) == FUNC_FAILURE) {
		free(buf);
		return FUNC_FAILURE;
	}

	free(buf);

#ifndef _NO_SUGGESTIONS
	if (conf.suggestions && words_num == 1 && wrong_cmd == 1) {
		fputs(NC, stdout);
		fflush(stdout);
		rl_restore_prompt();
		wrong_cmd = 0;
	}
#endif /* !_NO_SUGGESTIONS */

	return FUNC_SUCCESS;
}
#endif /* !_NO_FZF */

static char *
gen_quoted_str(const char *str)
{
	struct stat a;
	if (conf.quoting_style == QUOTING_STYLE_BACKSLASH
	|| cur_comp_type != TCMP_ELN
	|| (stat(str, &a) != -1 && S_ISDIR(a.st_mode)))
		return escape_str(str);

	return quote_str(str);
}

/* Complete the word at or before point.
   WHAT_TO_DO says what to do with the completion.
   `?' means list the possible completions.
   TAB means do standard completion.
   `*' means insert all of the possible completions.
   `!' means to do standard completion, and list all possible completions
   if there is more than one. */
int
tab_complete(const int what_to_do)
{
	if (rl_notab == 1)
		return FUNC_SUCCESS;

	if (*rl_line_buffer == '#' || cur_color == hc_c) {
		/* No completion at all if comment */
#ifndef _NO_SUGGESTIONS
		if (suggestion.printed)
			clear_suggestion(CS_FREEBUF);
#endif /* _NO_SUGGESTIONS */
		return FUNC_SUCCESS;
	}

	rl_compentry_func_t *our_func = rl_completion_entry_function;

	/* Only the completion entry function can change these. */
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
				scan = (int)rl_line_buffer[rl_point];

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
		scan = (int)rl_line_buffer[rl_point];
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

	int directory_changed = 0;
	int start = rl_point;
	rl_point = end;
	char *text = rl_copy_text(start, end);
	char **matches = NULL;

	/* At this point, we know we have an open quote if quote_char != '\0'. */

	/* If the user wants to TRY to complete, but then wants to give
	* up and use the default completion function, they set the
	* variable rl_attempted_completion_function. */
	if (rl_attempted_completion_function) {
		matches = (*rl_attempted_completion_function) (text, start, end);
		if (matches || rl_attempted_completion_over) {
			rl_attempted_completion_over = 0;
			our_func = NULL;
			goto AFTER_USUAL_COMPLETION;
		}
	}

	matches = rl_completion_matches(text, our_func);

AFTER_USUAL_COMPLETION:

	if (!matches || !matches[0]) {
		rl_ring_bell();
		free(text);
		return FUNC_FAILURE;
	}

#ifndef _NO_FZF
	/* Common prefix for multiple matches is appended to the input query.
	 * Let's rise a flag to know if we should reinsert the original query
	 * in case the user cancels the completion (pressing ESC). */
	const int common_prefix_added =
		(fzftab == 1 && matches[1] && strcmp(matches[0], text) != 0);
//		(fzftab == 1 && matches[1] && strlen(matches[0]) > strlen(text));
#endif /* _NO_FZF */

	size_t i;
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

		if (cur_comp_type == TCMP_HIST) {
			/* Sort the array without matches[0]: we need it to stay in
			 * place no matter what. */
			for (i = 0; matches[i]; i++);
			if (i > 0)
				qsort(matches + 1, i - 1, sizeof(char *),
					(QSFUNC *)compare_strings);
		}

		/* Remember the lowest common denominator: it may be unique. */
		lowest_common = savestring(matches[0], strlen(matches[0]));

		for (i = 0; matches[i + 1]; i++) {
			if (strcmp(matches[i], matches[i + 1]) == 0) {
				free(matches[i]);
				matches[i] = &dead_slot;
			} else {
				newlen++;
			}
		}

		/* We have marked all the dead slots with (char *)&dead_slot
		 * Copy all the non-dead entries into a new array. */
		temp_array = xnmalloc(3 + newlen, sizeof (char *));
		for (i = j = 1; matches[i]; i++) {
			if (matches[i] != &dead_slot)
				temp_array[j++] = matches[i];
		}
		temp_array[j] = NULL;

		if (matches[0] != &dead_slot)
			free(matches[0]);
		free(matches);

		matches = temp_array;

		/* Place the lowest common denominator back in [0]. */
		matches[0] = lowest_common;

		/* If there is one string left, and it is identical to the lowest
		 * common denominator (LCD), then the LCD is the string to insert. */
		if (j == 2 && strcmp(matches[0], matches[1]) == 0) {
			free(matches[1]);
			matches[1] = NULL;
		}
	}

	int len = 0, count = 0, limit = 0, max = 0;

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

//////////
		/* WORKAROUND: If 'ds' and the replacement string needs to be
		 * quoted, the completion does not work as expected. */
		if (cur_comp_type == TCMP_DESEL && matches[0]
		&& rl_strpbrk(matches[0], quote_chars))
			replacement = NULL;
//////////

		if (should_quote)
			should_quote = (should_quote && !quote_char);

		if (should_quote) {
			int do_replace = NO_MATCH;

			/* If there is a single match, see if we need to quote it.
			This also checks whether the common prefix of several
			matches needs to be quoted.  If the common prefix should
			not be checked, add !matches[1] to the if clause. */
			should_quote = (rl_strpbrk(matches[0], quote_chars) != 0);

			if (should_quote)
				do_replace = matches[1] ? MULT_MATCH : SINGLE_MATCH;

			if (do_replace != NO_MATCH) {
				/* Found an embedded word break character in a potential
				 match, so we need to prepend a quote character if we
				 are replacing the completion string. */
				replacement = gen_quoted_str(matches[0]);
			}
		}

		/* Handle replacement string */
		if (replacement && (cur_comp_type != TCMP_HIST || !matches[1])
		&& cur_comp_type != TCMP_FILE_TYPES_OPTS
		&& cur_comp_type != TCMP_MIME_LIST
		&& (cur_comp_type != TCMP_FILE_TEMPLATES || !matches[1])
		&& (cur_comp_type != TCMP_FILE_TYPES_FILES || !matches[1])
		&& (cur_comp_type != TCMP_GLOB || !matches[1])
		&& cur_comp_type != TCMP_JUMP && cur_comp_type != TCMP_RANGES
		&& cur_comp_type != TCMP_SEL
		&& cur_comp_type != TCMP_CMD_DESC
		&& cur_comp_type != TCMP_OWNERSHIP
		&& cur_comp_type != TCMP_DIRHIST
		&& (cur_comp_type != TCMP_WS_PREFIX || !matches[1])

		&& (cur_comp_type != TCMP_BM_PATHS || !matches[1])

		&& (cur_comp_type != TCMP_TAGS_F || !matches[1])) {

			enum comp_type c = cur_comp_type;
			if ((c == TCMP_DESEL || c == TCMP_NET
			|| c == TCMP_BM_PATHS || c == TCMP_PROF || c == TCMP_FILE_TEMPLATES
			|| c == TCMP_TAGS_C || c == TCMP_TAGS_S || c == TCMP_TAGS_T
			|| c == TCMP_TAGS_U || c == TCMP_BOOKMARK || c == TCMP_GLOB
			|| c == TCMP_PROMPTS || c == TCMP_CSCHEME || c == TCMP_WORKSPACES
			|| c == TCMP_BM_PREFIX) && !strchr(replacement, '\\')) {
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

			/* Let's keep the backslash, used to bypass alias names. */
			if (c == TCMP_CMD && text && *text == '\\' && *(text + 1))
				start++;
			else if (c == TCMP_WS_PREFIX)
				start += 2;
			else if (c == TCMP_FILE_TEMPLATES) {
				const char *p = strrchr(text, '@');
				if (p)
					start = rl_point - (int)strlen(p + 1);
			}

			rl_begin_undo_group();
			rl_delete_text(start, rl_point);
			rl_point = start;

#ifndef _NO_HIGHLIGHT
			if (conf.highlight && !wrong_cmd) {
				size_t k, l = 0;
				size_t _start = (*replacement == '\\'
					&& *(replacement + 1) == '~') ? 1 : 0;
				char *cc = cur_color;
				HIDE_CURSOR;
				fputs(tx_c, stdout);
				char t[PATH_MAX];
				for (k = _start; replacement[k]; k++) {
					rl_highlight(replacement, k, SET_COLOR);
					if ((signed char)replacement[k] < 0) {
						t[l++] = replacement[k];

						if ((signed char)replacement[k + 1] >= 0) {
							t[l] = '\0';
							l = 0;
							rl_insert_text(t);
							rl_redisplay();
						}
						continue;
					}
					t[0] = replacement[k];
					t[1] = '\0';
					rl_insert_text(t);

					/* WORKAROUND: If we are not at the end of the line,
					 * redisplay only up to the cursor position, to prevent
					 * whatever is after it from being printed using the
					 * last printed color.
					 * Drawback: there will be no color after the cursor
					 * position (no color however is better than a wrong color). */
					if (!replacement[k + 1] && rl_point < rl_end
					&& cur_color != tx_c) {
						int _end = rl_end;
						rl_end = rl_point;
						rl_redisplay();
						rl_end = _end;
						fputs(tx_c, stdout);
						fflush(stdout);
					}

					rl_redisplay();

					if (cur_color == hv_c || cur_color == hq_c
					|| cur_color == hp_c) {
						fputs(cur_color, stdout);
						fflush(stdout);
					}
				}

				UNHIDE_CURSOR;
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

		/* If there are more matches, ring the bell to indicate. If this was
		 * the only match, and we are hacking files, check the file to see if
		 * it was a directory. If so, add a '/' to the name.  If not, and we
		 * are at the end of the line, then add a space. */
		if (matches[1]) {
			if (what_to_do == '!') {
				goto DISPLAY_MATCHES;
			} else {
				if (rl_editing_mode != 0) /* vi_mode */
					rl_ding();	/* There are other matches remaining. */
			}
		} else { /* Just one match */
			if (cur_comp_type == TCMP_TAGS_T || cur_comp_type == TCMP_BOOKMARK
			|| cur_comp_type == TCMP_PROMPTS || cur_comp_type == TCMP_NET
			|| cur_comp_type == TCMP_CSCHEME || cur_comp_type == TCMP_WORKSPACES
			|| cur_comp_type == TCMP_HIST || cur_comp_type == TCMP_BACKDIR
			|| cur_comp_type == TCMP_PROF || cur_comp_type == TCMP_BM_PREFIX)
				break;

			/* Let's append an ending character to the inserted match. */
			if (cur_comp_type == TCMP_OWNERSHIP) {
				const char *sc = rl_line_buffer
					? strchr(rl_line_buffer, ':') : NULL;
				size_t l = wc_xstrlen(sc ? sc + 1
					: (rl_line_buffer ? rl_line_buffer : ""));
				rl_insert_text(matches[0] + l);
				if (!sc)
					rl_stuff_char(':');
				break;
			}

			char temp_string[4];
			int temp_string_index = 0;

			if (quote_char)
				temp_string[temp_string_index++] = quote_char;

			temp_string[temp_string_index++] = (char)(delimiter ? delimiter : ' ');

			temp_string[temp_string_index] = '\0';

			if (rl_filename_completion_desired) {
				struct stat finfo;
				char *filename = matches[0]
					? normalize_path(matches[0], strlen(matches[0]))
					: NULL;

				char *d = filename;
				if (filename && *filename == 'f' && filename[1] == 'i') {
					size_t flen = strlen(filename);
					if (IS_FILE_URI(filename, flen))
						d = filename + FILE_URI_PREFIX_LEN;
				}

				if (d && stat(d, &finfo) == 0 && S_ISDIR(finfo.st_mode)) {
					if (rl_line_buffer[rl_point] != '/') {
#ifndef _NO_HIGHLIGHT
						if (conf.highlight && !wrong_cmd) {
							const char *cc = cur_color;

							fputs(hd_c, stdout);
							rl_insert_text("/");

					/* WORKAROUND: If we are not at the end of the line,
					 * redisplay only up to the cursor position, to prevent
					 * whatever is after it from being printed using the
					 * last printed color.
					 * Drawback: there will be no color after the cursor
					 * position (no color however is better than a wrong color). */
							if (rl_point < rl_end) {
								int _end = rl_end;
								rl_end = rl_point;
								fputs(tx_c, stdout);
								fflush(stdout);
								rl_redisplay();
								rl_end = _end;
							} else {
								rl_redisplay();
							}

							fputs(rl_point < rl_end ? tx_c
								: (cc ? cc : ""), stdout);
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
		int j = 0, k = 0, l = 0;

		if (flags & PREVIEWER)
			goto CALC_OFFSET;

		/* Handle simple case first. Just one match. */
		if (!matches[1]) {
			char *temp;
			temp = printable_part(matches[0]);
			rl_crlf();
			print_filename(temp, matches[0]);
			rl_crlf();
			goto RESTART;
		}

		/* There is more than one match. Find out how many there are, and
		 * find out what the maximum printed length of a single entry is. */

DISPLAY_MATCHES:
#ifndef _NO_FZF
		if (fzftab != 1)
#endif /* !_NO_FZF */
		{
			max = 0;
			for (i = 1; matches[i]; i++) {
				const char *temp = printable_part(matches[i]);
				const size_t name_length = wc_xstrlen(temp);

				if ((int)name_length > max)
					max = (int)name_length;
			}

			len = (int)i - 1;

			/* If there are multiple items, ask the user if they really
			 * wants to see them all. */
			if (len >= rl_completion_query_items) {
				putchar('\n');
#ifndef _NO_HIGHLIGHT
				if (conf.highlight && cur_color != tx_c && !wrong_cmd) {
					cur_color = tx_c;
					fputs(tx_c, stdout);
				}
#endif /* !_NO_HIGHLIGHT */
				fprintf(rl_outstream, "Display all %d possibilities? "
					"[y/n] ", len);
				fflush(rl_outstream);
				if (!get_y_or_n())
					goto RESTART;
			}

			/* How many items of MAX length can we fit in the screen window? */
			max += 2;
			limit = term_cols / max;
			if (limit != 1 && (limit * max == term_cols))
				limit--;

			if (limit <= 0)
			  limit = 1;

			/* How many iterations of the printing loop? */
			count = (len + (limit - 1)) / limit;
		}

		putchar('\n');
#ifndef _NO_HIGHLIGHT
		if (conf.highlight && cur_color != tx_c && !wrong_cmd) {
			cur_color = tx_c;
			fputs(tx_c, stdout);
		}
#endif /* !_NO_HIGHLIGHT */
		if (cur_comp_type != TCMP_PATH && cur_comp_type != TCMP_GLOB)
			goto CALC_OFFSET;

		/* If /path/to/dir/<TAB> or /path/to/dir/GLOB<TAB>, let's temporarily
		 * change to /path/to/dir, so that the finder knows where we are. */
		if (*matches[0] == '~') {
			char *exp_path = tilde_expand(matches[0]);
			if (exp_path) {
				xchdir(exp_path, NO_TITLE);
				free(exp_path);
				directory_changed = 1;
			}
		} else {
			char *dir = matches[0];

			size_t dir_len = strlen(dir);
			if (IS_FILE_URI(dir, dir_len))
				dir += FILE_URI_PREFIX_LEN;

			char *last_slash = strrchr(dir, '/');
			if (!last_slash)
				goto CALC_OFFSET;

			char *norm_dir = NULL;
			/* MATCHES[0] SHOULD BE DIR!! */
			if (strstr(matches[0], "..")) {
				norm_dir = normalize_path(matches[0], strlen(matches[0]));
				if (norm_dir) {
					size_t norm_dir_len = strlen(norm_dir) + 2;
					char *ptr = xnmalloc(norm_dir_len, sizeof(char *));
					snprintf(ptr, norm_dir_len, "%s/", norm_dir);
					free(norm_dir);
					norm_dir = ptr;
				}
			}

			dir = norm_dir ? norm_dir : matches[0];
			directory_changed = 1;

			if (last_slash == dir) {
				if (last_slash[1]) {
					char c = last_slash[1];
					*(last_slash + 1) = '\0';
					xchdir(dir, NO_TITLE);
					*(last_slash + 1) = c;
				} else {
					/* We have the root dir */
					xchdir(dir, NO_TITLE);
				}
			} else {
				*last_slash = '\0';
				xchdir(dir, NO_TITLE);
				*last_slash = '/';
			}

			if (dir != matches[0])
				free(dir);
		}

CALC_OFFSET:
#ifndef _NO_FZF
		/* Alternative tab completion: fzf, fnf, smenu. */
		if (fzftab == 1) {
			char *t = text ? text : NULL;
			if (finder_tabcomp(matches, common_prefix_added == 1 ? t : NULL,
			conf.fuzzy_match == 1 ? t : NULL) == -1)
				goto RESTART;
			goto RESET_PATH;
		}
#else
		if (1) {} /* This just silences a warning. */
#endif /* !_NO_FZF */

		/* Standard tab completion. */
		char *ptr = matches[0];
		/* Skip leading backslashes. */
		while (*ptr) {
			if (*ptr != '\\')
				break;
			ptr++;
		}

		const char *qq =
			(cur_comp_type == TCMP_DESEL || cur_comp_type == TCMP_SEL
			|| cur_comp_type == TCMP_HIST) ? ptr : strrchr(ptr, '/');

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
			int add = (cur_comp_type == TCMP_PATH && *ptr == '/') ? 1 : 0;
			tab_offset = strlen(ptr + add);
		}

		if (cur_comp_type == TCMP_OWNERSHIP && *ptr == ':' && !ptr[1]) {
			ptr = NULL;
			tab_offset = 0;
		}

		if (cur_comp_type == TCMP_RANGES || cur_comp_type == TCMP_BACKDIR
		|| cur_comp_type == TCMP_FILE_TYPES_FILES
		|| cur_comp_type == TCMP_FILE_TYPES_OPTS
		|| cur_comp_type == TCMP_BM_PATHS || cur_comp_type == TCMP_MIME_LIST
		|| cur_comp_type == TCMP_CMD_DESC || cur_comp_type == TCMP_SEL
		|| cur_comp_type == TCMP_FILE_TEMPLATES
		|| cur_comp_type == TCMP_DIRHIST
		|| (tabmode == STD_TAB && (cur_comp_type == TCMP_JUMP
		|| cur_comp_type == TCMP_TAGS_F) ) )
			/* We don't want to highlight the matching part. */
			tab_offset = 0;

		if (cur_comp_type == TCMP_HIST && ptr && *ptr == '!' && tab_offset > 0) {
			if (conf.fuzzy_match == 1)
				tab_offset = 0;
			else
				tab_offset--;
		}

#ifndef _NO_TRASH
		/* If printing trashed files, let's change to the trash dir
		 * to allow files colorization. */
		if ((cur_comp_type == TCMP_UNTRASH || cur_comp_type == TCMP_TRASHDEL)
		&& conf.colorize == 1 && trash_files_dir) {
			directory_changed = 1;
			xchdir(trash_files_dir, NO_TITLE);
		}
#endif /* _NO_TRASH */

		ERASE_TO_RIGHT_AND_BELOW;

		for (i = 1; i <= (size_t)count; i++) {
			if (i >= term_lines) {
				/* A little pager */
				fputs("\x1b[7m--Mas--\x1b[0m", stdout);
				char c = 0;
				while ((c = xgetchar()) == KEY_ESC);
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
						/* Print the matching part of the match. */
						printf("\x1b[0m%s%s\x1b[0m%s",
							ts_c, qq ? qq : matches[0],
							(cur_comp_type == TCMP_CMD) ? (conf.colorize
							? ex_c : "") : fc_c);
					}

					/* Now print the non-matching part of the match. */
					char *temp = printable_part(matches[l]);
					int printed_length = (int)wc_xstrlen(temp);
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

		if (!wrong_cmd && conf.colorize && cur_comp_type == TCMP_CMD)
			fputs(tx_c, stdout);
		rl_reset_line_state();

#ifndef _NO_FZF
RESET_PATH:
#endif /* !_NO_FZF */

RESTART:
		flags &= ~STATE_COMPLETING;
		if (directory_changed == 1 && workspaces && workspaces[cur_ws].path)
			xchdir(workspaces[cur_ws].path, NO_TITLE);

		rl_on_new_line();
#ifndef _NO_HIGHLIGHT
		if (conf.highlight == 1 && wrong_cmd == 0) {
			int bk = rl_point;
			HIDE_CURSOR;
			char *ss = rl_copy_text(0, rl_end);
			rl_delete_text(0, rl_end);
			rl_redisplay();
			rl_point = rl_end = 0;

			l = 0;
			char t[PATH_MAX];
			for (k = 0; ss[k]; k++) {
				rl_highlight(ss, (size_t)k, SET_COLOR);

				if ((signed char)ss[k] < 0) {
					t[l++] = ss[k];

					if ((signed char)ss[k + 1] >= 0) {
						t[l] = '\0';
						l = 0;
						rl_insert_text(t);
						rl_redisplay();
					}
					continue;
				}

				t[0] = ss[k];
				t[1] = '\0';
				rl_insert_text(t);
				rl_redisplay();
			}
			UNHIDE_CURSOR;
			rl_point = bk;
			free(ss);
		}
#endif /* !_NO_HIGHLIGHT */
		}
		break;

	default:
		xerror("\r\nreadline: %c: Bad value for what_to_do "
			"in tab_complete\n", what_to_do);
		exit(FUNC_FAILURE);
	}

	for (i = 0; matches[i]; i++)
		free(matches[i]);
	free(matches);
	free(text);

	return FUNC_SUCCESS;
}
