/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* mouse.c -- mouse input handling */

#include "helpers.h"

#include <errno.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>

#ifdef __OpenBSD__
typedef char *rl_cpvfunc_t;
typedef void rl_macro_print_func_t (const char *, const char *, int, const char *);
# include <ereadline/readline/readline.h>
#else
# include <readline/readline.h>
#endif /* __OpenBSD__ */

#include "mouse.h"
#include "mem.h"
#ifndef _NO_SUGGESTIONS
# include "suggestions.h"
#endif /* !_NO_SUGGESTIONS */
#include "term.h"

/* Keep these short to avoid noticeable delays for non-mouse ESC sequences. */
#define MOUSE_PARSE_TIMEOUT_FIRST_MS 8
#define MOUSE_PARSE_TIMEOUT_NEXT_MS  4
#define MOUSE_DBLCLICK_MAX_MS 350
#define MOUSE_PENDING_BUF_SIZE 64

struct scroll_item {
	filesn_t index;
	char *token;
};

static int apply_single_click_insert(char *arg);

static unsigned char mouse_pending_buf[MOUSE_PENDING_BUF_SIZE];
static size_t mouse_pending_len = 0;
static size_t mouse_pending_pos = 0;

static filesn_t mouse_armed_index = (filesn_t)-1;
static struct timespec mouse_armed_click = {0};
static char *mouse_armed_arg = NULL;

static int mouse_press_active = 0;
static int mouse_press_x = 0;
static int mouse_press_y = 0;

/* Number of non-list lines printed after the file list and before the prompt. */
static int mouse_post_listing_lines = 0;

static inline int
is_blank_char(const char c)
{
	return c == ' ' || c == '\t';
}

static int
has_command_prefix(const char *line)
{
	if (!line || !*line)
		return 0;

	while (*line && is_blank_char(*line))
		line++;

	return *line ? 1 : 0;
}

void
mouse_reset_layout_state(void)
{
	mouse_post_listing_lines = 0;
}

void
mouse_set_post_listing_lines(const int lines)
{
	mouse_post_listing_lines = lines > 0 ? lines : 0;
}

void
mouse_add_post_listing_lines(const int lines)
{
	if (lines <= 0)
		return;

	mouse_post_listing_lines += lines;
}

static void
clear_armed_click(void)
{
	mouse_armed_index = (filesn_t)-1;
	mouse_armed_click = (struct timespec){0};
	free(mouse_armed_arg);
	mouse_armed_arg = NULL;
}

void
enable_mouse_if_interactive(void)
{
	if (conf.mouse_support == 0) {
		clear_armed_click();

		if (mouse_enabled == 1) {
			UNSET_MOUSE_TRACKING;
			fflush(stdout);
			mouse_enabled = 0;
		}
		return;
	}

	if (mouse_enabled == 0 && xargs.list_and_quit != 1
	&& xargs.open != 1 && xargs.preview != 1
	&& isatty(STDIN_FILENO) == 1 && isatty(STDOUT_FILENO) == 1) {
		SET_MOUSE_TRACKING;
		fflush(stdout);
		mouse_enabled = 1;
	}
}

static void
queue_pending_byte(const unsigned char c)
{
	if (mouse_pending_len >= MOUSE_PENDING_BUF_SIZE)
		return;
	mouse_pending_buf[mouse_pending_len++] = c;
}

static void
queue_pending_bytes(const unsigned char *buf, const size_t len)
{
	for (size_t i = 0; i < len; i++)
		queue_pending_byte(buf[i]);
}

int
mouse_pop_pending_byte(unsigned char *c)
{
	if (!c || mouse_pending_pos >= mouse_pending_len)
		return 0;

	*c = mouse_pending_buf[mouse_pending_pos++];
	if (mouse_pending_pos >= mouse_pending_len)
		mouse_pending_pos = mouse_pending_len = 0;

	return 1;
}

static int
read_byte_with_timeout(const int fd, unsigned char *c, const int timeout_ms)
{
	if (!c)
		return 0;

	struct pollfd pfd = {.fd = fd, .events = POLLIN, .revents = 0};
	int pret;
	do {
		pret = poll(&pfd, 1, timeout_ms);
	} while (pret < 0 && errno == EINTR);

	if (pret <= 0 || (pfd.revents & POLLIN) == 0)
		return 0;

	return read(fd, c, 1) == 1; // flawfinder: ignore
}

static inline long
get_elapsed_ms(const struct timespec *from, const struct timespec *to)
{
	return (to->tv_sec - from->tv_sec) * 1000L
		+ (to->tv_nsec - from->tv_nsec) / 1000000L;
}

static void
count_rendered_lines(const char *s, int *lines, int *col, const int cols,
	const int skip_rl_ignore)
{
	if (!s || !*s || !lines || !col || cols <= 0)
		return;

	int in_ignore = 0;
	for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
		if (skip_rl_ignore == 1) {
			if (*p == RL_PROMPT_START_IGNORE) {
				in_ignore = 1;
				continue;
			}
			if (in_ignore == 1) {
				if (*p == RL_PROMPT_END_IGNORE)
					in_ignore = 0;
				continue;
			}
		}

		if (*p == '\n') {
			(*lines)++;
			*col = 0;
			continue;
		}

		if (*p == '\r') {
			*col = 0;
			continue;
		}

		(*col)++;
		if (*col >= cols) {
			(*lines)++;
			*col = 0;
		}
	}
}

static int
get_prompt_lines(void)
{
	const int cols = (int)term_cols > 0 ? (int)term_cols : 80;
	int lines = 1;
	int col = 0;

	count_rendered_lines(rl_prompt, &lines, &col, cols, 1);
	count_rendered_lines(rl_line_buffer, &lines, &col, cols, 0);

	return lines > 0 ? lines : 1;
}

int
mouse_pending_single_wait_ms(void)
{
	if (!mouse_armed_arg || mouse_armed_click.tv_sec <= 0)
		return -1;

	struct timespec now = {0};
	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
		return 0;

	long rem = MOUSE_DBLCLICK_MAX_MS
		- get_elapsed_ms(&mouse_armed_click, &now);
	if (rem < 0)
		rem = 0;

	return (int)rem;
}

int
mouse_commit_pending_single_click(void)
{
	if (!mouse_armed_arg)
		return MOUSE_SEQ_CONSUMED;

	char *arg = mouse_armed_arg;
	mouse_armed_arg = NULL;
	mouse_armed_index = (filesn_t)-1;
	mouse_armed_click = (struct timespec){0};

	const int ret = apply_single_click_insert(arg);
	free(arg);
	return ret;
}

static int
get_listing_rows_total(void)
{
	if (!file_info || g_files_num == 0)
		return 0;

	int max_row = -1;
	filesn_t i = g_files_num;
	while (--i >= 0) {
		if (!file_info[i].name || file_info[i].mouse_row < 0)
			continue;
		if (file_info[i].mouse_row > max_row)
			max_row = file_info[i].mouse_row;
	}

	return max_row >= 0 ? max_row + 1 : 0;
}

static filesn_t
get_clicked_file_by_coord(const int x, const int y)
{
	if (!file_info || g_files_num == 0 || x <= 0 || y <= 0)
		return (filesn_t)-1;

	const int rows_total = get_listing_rows_total();
	if (rows_total <= 0)
		return (filesn_t)-1;

	const int prompt_lines = get_prompt_lines();
	int first_visible = rows_total + mouse_post_listing_lines + prompt_lines
		- (int)term_lines;
	if (first_visible < 0)
		first_visible = 0;

	filesn_t i = g_files_num;
	while (--i >= 0) {
		if (!file_info[i].name || file_info[i].mouse_col_start <= 0
		|| file_info[i].mouse_col_end < file_info[i].mouse_col_start)
			continue;

		const int row = file_info[i].mouse_row - first_visible + 1;
		if (row != y)
			continue;

		if (x >= file_info[i].mouse_col_start
		&& x <= file_info[i].mouse_col_end)
			return i;
	}

	return (filesn_t)-1;
}

static char *
append_dir_slash(char *arg, const filesn_t index)
{
	if (!arg || conf.mouse_dir_trailing_slash == 0
	|| !file_info || index < 0 || !file_info[index].name
	|| file_info[index].dir == 0)
		return arg;

	const size_t len = strlen(arg);
	if (len == 0 || arg[len - 1] == '/')
		return arg;

	char *tmp = xnmalloc(len + 2, sizeof(char));
	snprintf(tmp, len + 2, "%s/", arg);
	free(arg);
	return tmp;
}

static char *
escape_clicked_name(const filesn_t index)
{
	if (index < 0 || !file_info || !file_info[index].name)
		return NULL;

	const char *name = file_info[index].name;
	if (file_info[index].type == DT_REG
	&& conf.quoting_style != QUOTING_STYLE_BACKSLASH) {
		const char quote_char = conf.quoting_style
			== QUOTING_STYLE_DOUBLE_QUOTES ? '"' : '\'';
		/* Do not wrap with quotes if this would produce a broken token. */
		if (strchr(name, quote_char) == NULL && strchr(name, '\\') == NULL) {
			char *quoted = quote_str(name);
			if (quoted)
				return quoted;
		}
	}

	return append_dir_slash(escape_str(name), index);
}

static void
clear_suggestions_if_any(void)
{
#ifndef _NO_SUGGESTIONS
	if (suggestion_buf)
		clear_suggestion(CS_FREEBUF);
#endif /* !_NO_SUGGESTIONS */
}

static int
apply_single_click_insert(char *arg)
{
	if (!arg || !*arg)
		return MOUSE_SEQ_CONSUMED;

	clear_suggestions_if_any();

	if (has_command_prefix(rl_line_buffer) == 1) {
		const char *line = rl_line_buffer;
		const size_t line_len = strlen(line);
		const int need_space = (line_len == 0
			|| is_blank_char(line[line_len - 1])) ? 0 : 1;
		const size_t new_len = line_len + (size_t)need_space
			+ strlen(arg) + 1;
		char *new_line = xnmalloc(new_len, sizeof(char));
		snprintf(new_line, new_len, "%s%s%s", line,
			need_space == 1 ? " " : "", arg);
		rl_replace_line(new_line, 1);
		free(new_line);
	} else {
		rl_replace_line(arg, 1);
	}

	rl_point = rl_end;
	rl_redisplay();
	cmdhist_flag = 1;
	return MOUSE_SEQ_CONSUMED;
}

static int
apply_double_click_open(const filesn_t index, char *arg)
{
	size_t cmd_len = strlen(arg) + 6;
	char *cmd = xnmalloc(cmd_len, sizeof(char));
	if (file_info[index].dir == 1)
		snprintf(cmd, cmd_len, "cd %s", arg);
	else
		snprintf(cmd, cmd_len, "open %s", arg);

	rl_replace_line(cmd, 1);
	rl_point = rl_end;
	rl_redisplay();
	cmdhist_flag = 1;

	free(cmd);
	clear_armed_click();
	return MOUSE_SEQ_ACCEPT_LINE;
}

static int
handle_mouse_left_click(const int x, const int y)
{
	const filesn_t index = get_clicked_file_by_coord(x, y);
	if (index == (filesn_t)-1)
		return MOUSE_SEQ_CONSUMED;

	char *arg = escape_clicked_name(index);
	if (!arg || !*arg) {
		free(arg);
		return MOUSE_SEQ_CONSUMED;
	}

	struct timespec now = {0};
	const int clock_ok = (clock_gettime(CLOCK_MONOTONIC, &now) == 0);

	const int is_double_click = (conf.mouse_open_on_double_click == 1
		&& clock_ok == 1
		&& mouse_armed_index == index
		&& mouse_armed_click.tv_sec > 0
		&& get_elapsed_ms(&mouse_armed_click, &now) <= MOUSE_DBLCLICK_MAX_MS);

	if (is_double_click == 1) {
		clear_armed_click();
		clear_suggestions_if_any();
		const int ret = apply_double_click_open(index, arg);
		free(arg);
		return ret;
	}

	/* A new click cancels pending double-click detection for a previous target.
	 * If a deferred single-click insertion exists, commit it first. */
	if (mouse_armed_arg)
		mouse_commit_pending_single_click();
	else if (mouse_armed_index != (filesn_t)-1)
		clear_armed_click();

	if (conf.mouse_open_on_double_click == 1 && clock_ok == 1) {
		mouse_armed_index = index;
		mouse_armed_click = now;

		if (conf.mouse_insert_on_single_click == 1) {
			mouse_armed_arg = arg;
			return MOUSE_SEQ_CONSUMED;
		}

		free(arg);
		return MOUSE_SEQ_CONSUMED;
	}

	int ret = MOUSE_SEQ_CONSUMED;
	if (conf.mouse_insert_on_single_click == 1)
		ret = apply_single_click_insert(arg);

	free(arg);
	return ret;
}

static int
handle_mouse_scroll_history(const int dir)
{
	if ((dir != -1 && dir != 1) || !history || current_hist_n == 0)
		return MOUSE_SEQ_CONSUMED;

	clear_suggestions_if_any();

	cmdhist_flag = 1;
	size_t p = curhistindex;
	if (p > current_hist_n)
		p = current_hist_n;

	if (dir < 0) { /* Wheel up: previous (older) history entry. */
		if (p == 0)
			return MOUSE_SEQ_CONSUMED;
		p--;
	} else { /* Wheel down: next (newer) history entry. */
		if (p >= current_hist_n) {
			curhistindex = current_hist_n;
			return MOUSE_SEQ_CONSUMED;
		}
		p++;
		if (p >= current_hist_n) {
			rl_replace_line("", 1);
			rl_point = rl_end;
			rl_redisplay();
			curhistindex = current_hist_n;
			return MOUSE_SEQ_CONSUMED;
		}
	}

	if (!history[p].cmd)
		return MOUSE_SEQ_CONSUMED;

	rl_replace_line(history[p].cmd, 1);
	rl_point = rl_end;
	rl_redisplay();
	curhistindex = p;
	return MOUSE_SEQ_CONSUMED;
}

static int
parse_command_bounds(const char *line, size_t *cmd_start, size_t *cmd_end)
{
	if (!line || !cmd_start || !cmd_end)
		return 0;

	size_t i = 0;
	while (line[i] && is_blank_char(line[i]))
		i++;
	if (!line[i])
		return 0;

	size_t j = i;
	while (line[j] && !is_blank_char(line[j]))
		j++;

	*cmd_start = i;
	*cmd_end = j;
	return 1;
}

static inline int
cmd_eq(const char *cmd, const size_t len, const char *name)
{
	return cmd && name && len == strlen(name)
		&& strncmp(cmd, name, len) == 0;
}

static int
get_scroll_target(const char *line)
{
	if (conf.mouse_scroll_mode != MOUSE_SCROLL_AUTO)
		return conf.mouse_scroll_mode;

	if (has_command_prefix(line) == 0)
		return MOUSE_SCROLL_HISTORY;

	size_t cmd_start = 0, cmd_end = 0;
	if (parse_command_bounds(line, &cmd_start, &cmd_end) == 0)
		return MOUSE_SCROLL_HISTORY;

	const size_t len = cmd_end - cmd_start;
	const char *cmd = line + cmd_start;

	/* Directory-centric commands. */
	if (cmd_eq(cmd, len, "cd")
	|| cmd_eq(cmd, len, "ls"))
		return MOUSE_SCROLL_DIRS;

	/* File-centric commands. */
	if (cmd_eq(cmd, len, "cat")
	|| cmd_eq(cmd, len, "less")
	|| cmd_eq(cmd, len, "more")
	|| cmd_eq(cmd, len, "head")
	|| cmd_eq(cmd, len, "tail")
	|| cmd_eq(cmd, len, "bat")
	|| cmd_eq(cmd, len, "vim")
	|| cmd_eq(cmd, len, "vi")
	|| cmd_eq(cmd, len, "nvim")
	|| cmd_eq(cmd, len, "nano")
	|| cmd_eq(cmd, len, "emacs")
	|| cmd_eq(cmd, len, "view"))
		return MOUSE_SCROLL_FILES;

	/* Everything else: cycle all visible entries. */
	return MOUSE_SCROLL_ALL;
}

static char *
build_scroll_token(const filesn_t index)
{
	if (index == (filesn_t)-1)
		return savestring("..", 2);
	return escape_clicked_name(index);
}

static struct scroll_item *
build_scroll_items(const int mode, size_t *nitems)
{
	if (!nitems)
		return NULL;
	*nitems = 0;

	if (!file_info || g_files_num == 0)
		return NULL;

	size_t count = 0;
	if (mode == MOUSE_SCROLL_DIRS)
		count = 1; /* ".." */

	filesn_t i = g_files_num;
	while (--i >= 0) {
		if (!file_info[i].name)
			continue;
		if (mode == MOUSE_SCROLL_DIRS && file_info[i].dir == 0)
			continue;
		if (mode == MOUSE_SCROLL_FILES && file_info[i].dir == 1)
			continue;
		count++;
	}

	if (count == 0)
		return NULL;

	struct scroll_item *items = xnmalloc(count, sizeof(struct scroll_item));
	size_t p = 0;

	if (mode == MOUSE_SCROLL_DIRS) {
		items[p].index = (filesn_t)-1;
		items[p].token = build_scroll_token((filesn_t)-1);
		if (!items[p].token)
			items[p].token = savestring("..", 2);
		p++;
	}

	for (i = 0; i < g_files_num; i++) {
		if (!file_info[i].name)
			continue;
		if (mode == MOUSE_SCROLL_DIRS && file_info[i].dir == 0)
			continue;
		if (mode == MOUSE_SCROLL_FILES && file_info[i].dir == 1)
			continue;

		items[p].index = i;
		items[p].token = build_scroll_token(i);
		if (!items[p].token)
			items[p].token = savestring(file_info[i].name, strlen(file_info[i].name));
		p++;
	}

	*nitems = p;
	return items;
}

static void
free_scroll_items(struct scroll_item *items, const size_t nitems)
{
	if (!items)
		return;
	for (size_t i = 0; i < nitems; i++)
		free(items[i].token);
	free(items);
}

static char *
get_current_arg_str(const char *line, const size_t cmd_end)
{
	if (!line || !line[cmd_end])
		return NULL;

	size_t p = cmd_end;
	while (line[p] && is_blank_char(line[p]))
		p++;
	if (!line[p])
		return NULL;

	size_t end = strlen(line);
	while (end > p && is_blank_char(line[end - 1]))
		end--;

	if (end <= p)
		return NULL;

	const size_t len = end - p;
	char *arg = xnmalloc(len + 1, sizeof(char));
	memcpy(arg, line + p, len);
	arg[len] = '\0';
	return arg;
}

static int
replace_scrolled_arg(const char *line, const size_t cmd_end, const char *token)
{
	if (!line || !token || cmd_end == 0)
		return MOUSE_SEQ_CONSUMED;

	const size_t new_len = cmd_end + 1 + strlen(token) + 1;
	char *new_line = xnmalloc(new_len, sizeof(char));
	snprintf(new_line, new_len, "%.*s %s", (int)cmd_end, line, token);
	rl_replace_line(new_line, 1);
	rl_point = rl_end;
	rl_redisplay();
	cmdhist_flag = 1;
	free(new_line);
	return MOUSE_SEQ_CONSUMED;
}

static int
handle_mouse_scroll_entries(const int dir, const int mode)
{
	if (dir != -1 && dir != 1)
		return MOUSE_SEQ_CONSUMED;

	const char *line = rl_line_buffer ? rl_line_buffer : "";
	size_t cmd_start = 0, cmd_end = 0;
	if (parse_command_bounds(line, &cmd_start, &cmd_end) == 0)
		return MOUSE_SEQ_CONSUMED;

	size_t nitems = 0;
	struct scroll_item *items = build_scroll_items(mode, &nitems);
	if (!items || nitems == 0) {
		free_scroll_items(items, nitems);
		return MOUSE_SEQ_CONSUMED;
	}

	clear_suggestions_if_any();

	char *cur_arg = get_current_arg_str(line, cmd_end);
	ssize_t pos = -1;
	if (cur_arg && *cur_arg) {
		for (size_t i = 0; i < nitems; i++) {
			if (!items[i].token)
				continue;
			if (*cur_arg == *items[i].token
			&& strcmp(cur_arg, items[i].token) == 0) {
				pos = (ssize_t)i;
				break;
			}
		}
	}

	if (pos == -1)
		pos = (dir < 0) ? (ssize_t)nitems - 1 : 0;
	else if (dir < 0 && pos > 0)
		pos--;
	else if (dir > 0 && (size_t)pos + 1 < nitems)
		pos++;

	const int ret = replace_scrolled_arg(line, cmd_end, items[(size_t)pos].token);
	free(cur_arg);
	free_scroll_items(items, nitems);
	return ret;
}

static int
handle_mouse_scroll(const int x, const int y, const int dir)
{
	UNUSED(x); UNUSED(y);

	if (mouse_armed_arg)
		mouse_commit_pending_single_click();
	else if (mouse_armed_index != (filesn_t)-1)
		clear_armed_click();

	const int target = get_scroll_target(rl_line_buffer);

	switch (target) {
	case MOUSE_SCROLL_HISTORY:
		return handle_mouse_scroll_history(dir);
	case MOUSE_SCROLL_DIRS:
		return handle_mouse_scroll_entries(dir, MOUSE_SCROLL_DIRS);
	case MOUSE_SCROLL_FILES:
		return handle_mouse_scroll_entries(dir, MOUSE_SCROLL_FILES);
	case MOUSE_SCROLL_ALL:
		return handle_mouse_scroll_entries(dir, MOUSE_SCROLL_ALL);
	case MOUSE_SCROLL_AUTO: /* fallthrough */
	default:
		return handle_mouse_scroll_history(dir);
	}
}

int
mouse_handle_escape(FILE *stream)
{
	if (mouse_enabled == 0 || conf.mouse_support == 0)
		return MOUSE_SEQ_NOT_MOUSE;

	const int fd = fileno(stream);
	unsigned char seq[64];
	size_t len = 0;
	unsigned char c = 0;

	if (!read_byte_with_timeout(fd, &c, MOUSE_PARSE_TIMEOUT_FIRST_MS))
		return MOUSE_SEQ_NOT_MOUSE;

	if (c != '[') {
		queue_pending_byte(c);
		return MOUSE_SEQ_NOT_MOUSE;
	}
	seq[len++] = c;

	if (!read_byte_with_timeout(fd, &c, MOUSE_PARSE_TIMEOUT_NEXT_MS)) {
		queue_pending_bytes(seq, len);
		return MOUSE_SEQ_NOT_MOUSE;
	}

	if (c != '<') {
		queue_pending_bytes(seq, len);
		queue_pending_byte(c);
		return MOUSE_SEQ_NOT_MOUSE;
	}
	seq[len++] = c;

	while (len < sizeof(seq) - 1) {
		if (!read_byte_with_timeout(fd, &c, MOUSE_PARSE_TIMEOUT_NEXT_MS)) {
			queue_pending_bytes(seq, len);
			return MOUSE_SEQ_NOT_MOUSE;
		}
		seq[len++] = c;
		if (c == 'M' || c == 'm')
			break;
	}

	if (len == sizeof(seq) - 1
	&& seq[len - 1] != 'M' && seq[len - 1] != 'm') {
		queue_pending_bytes(seq, len);
		return MOUSE_SEQ_NOT_MOUSE;
	}

	if (len < 5 || (seq[len - 1] != 'M' && seq[len - 1] != 'm'))
		return MOUSE_SEQ_CONSUMED;

	seq[len] = '\0';

	int button = 0;
	int x = 0;
	int y = 0;
	char suffix = '\0';

	if (sscanf((const char *)seq, "[<%d;%d;%d%c", &button, &x, &y, &suffix) != 4)
		return MOUSE_SEQ_CONSUMED;

	/* Mouse wheel: button 64 (up), 65 (down). */
	if (suffix == 'M' && (button & 64) != 0 && (button & 32) == 0) {
		const int dir = (button & 1) ? 1 : -1;
		return handle_mouse_scroll(x, y, dir);
	}

	/* Only plain left-button events. */
	if ((button & 3) != 0 || (button & 32) != 0 || (button & 64) != 0)
		return MOUSE_SEQ_CONSUMED;

	if (suffix == 'M') { /* Press */
		mouse_press_active = 1;
		mouse_press_x = x;
		mouse_press_y = y;
		return MOUSE_SEQ_CONSUMED;
	}

	if (suffix == 'm') { /* Release */
		if (mouse_press_active == 0)
			return MOUSE_SEQ_CONSUMED;

		const int click = (mouse_press_x == x && mouse_press_y == y);
		mouse_press_active = 0;
		if (click == 0)
			return MOUSE_SEQ_CONSUMED;

		return handle_mouse_left_click(x, y);
	}

	return MOUSE_SEQ_CONSUMED;
}
