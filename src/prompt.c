/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* prompt.c -- functions controlling the appearance and behavior of the prompt */

/* The decode_prompt function is taken from Bash (1.14.7), licensed under
 * GPL-2.0-or-later, and modified to fit our needs. */

#include "helpers.h"

#include <string.h>
#if !defined(__HAIKU__) && !defined(__OpenBSD__) && !defined(__ANDROID__)
# include <wordexp.h>
#endif /* !__HAIKU__ && !__OpenBSD__ && !__ANDROID__ */
#include <readline/readline.h>
#include <readline/history.h> /* history_expand() */
#include <errno.h>

#include "aux.h"
#include "checks.h" /* is_number() */
#include "colors.h" /* update_warning_prompt_text_color() */
#include "file_operations.h"
#include "history.h"
#include "init.h"
#include "listing.h"
#include "messages.h"
#include "misc.h"
#include "navigation.h"
#include "prompt.h"
#include "properties.h" /* get_file_perms() */
#include "sanitize.h"
#include "sort.h" /* num_to_sort_name() */
#include "spawn.h"
#ifndef _NO_SUGGESTIONS
# include "suggestions.h"
#endif /* !_NO_SUGGESTIONS */

#if defined(__HAIKU__) || defined(__OpenBSD__) || defined(__ANDROID__)
# define NO_WORDEXP
#else
# define MAX_PMOD_PATHS 8
static size_t p_mod_paths_n = 0;

struct p_mod_paths_t {
	char path[PATH_MAX];
	char *name;
};

static struct p_mod_paths_t p_mod_paths[MAX_PMOD_PATHS];
#endif /* __HAIKU__ || __OpenBSD__ || __ANDROID__ */

int g_prompt_ignore_empty_line = 0;

static char *
gen_time(const int c)
{
	const time_t rawtime = time(NULL);
	if (rawtime == (time_t)-1)
		return NULL;

	struct tm tm;
	if (!localtime_r(&rawtime, &tm)) {
		char *tmp = savestring(UNKNOWN_STR, sizeof(UNKNOWN_STR) - 1);
		return tmp;
	}

	char buf[64] = "";
	size_t len = 0;

	switch (c) {
	case 't': len = strftime(buf, sizeof(buf), "%H:%M:%S", &tm); break;
	case 'T': len = strftime(buf, sizeof(buf), "%I:%M:%S", &tm); break;
	case 'A': len = strftime(buf, sizeof(buf), "%H:%M", &tm); break;
	case '@': len = strftime(buf, sizeof(buf), "%I:%M:%S %p", &tm); break;
	case 'd': len = strftime(buf, sizeof(buf), "%a %b %d", &tm); break;
	default: break;
	}

	if (len == 0)
		return NULL;

	return savestring(buf, len);
}

static char *
gen_rl_vi_mode(const int alloc)
{
	if (rl_editing_mode == RL_EMACS_MODE
	/*"Ignore if running in vi mode and notifications are enabled: the
	 * notification will be printed by the prompt itself. */
	|| (alloc == 1 && prompt_notif == 1)) {
		if (alloc == 0)
			return "";
		char *tmp = xnmalloc(1, sizeof(char));
		*tmp = '\0';
		return tmp;
	}

	Keymap keymap = rl_get_keymap();
	if (keymap == vi_insertion_keymap) {
		return alloc == 1
			? savestring(RL_VI_INS_MODESTR, RL_VI_INS_MODESTR_LEN)
			: RL_VI_INS_MODESTR;
	}

	return alloc == 1 ? savestring(RL_VI_CMD_MODESTR, RL_VI_CMD_MODESTR_LEN)
		: RL_VI_CMD_MODESTR;
}

static char *
get_dir_basename(const char *str)
{
	/* If not root dir (/), get last path component */
	const char *ret = (*str == '/' && !str[1])
		? NULL : strrchr(str, '/');

	if (!ret || !ret[1])
		return savestring(str, strlen(str));

	return savestring(ret + 1, strlen(ret + 1));
}

static char *
reduce_path(const char *str)
{
	char *temp = NULL;
	const size_t slen = strlen(str);

	if (slen > (size_t)conf.prompt_p_max_path) {
		const char *ret = strrchr(str, '/');
		if (!ret || !ret[1])
			temp = savestring(str, slen);
		else
			temp = savestring(ret + 1, (size_t)(str + slen - ret - 1));
	} else {
		temp = savestring(str, slen);
	}

	return temp;
}

static size_t
copy_char(char *buf, const char *s)
{
	const int bytes = IS_UTF8_CHAR(*s) ? utf8_bytes((unsigned char)*s) : 1;

	if (bytes == 1) {
		*buf = *s;
		return 1;
	}

	int c = 0;
	while (c < bytes)
		buf[c++] = *s++;

	return (size_t)c;
}

static char *
reduce_path_fish(char *str)
{
	if ((*str == '~' || *str == '/') && !str[1])
		return savestring(*str == '~' ? "~" : "/", 1);

	if (conf.prompt_f_dir_len == 0)
		return savestring(str, strlen(str));

	char *s = str;
	size_t total_comps = (*s == '~');

	while (*s) {
		if (*s == '/')
			total_comps++;
		s++;
	}

	if (conf.prompt_f_full_len_dirs > 1
	&& total_comps > (size_t)conf.prompt_f_full_len_dirs - 1)
		total_comps -= (size_t)conf.prompt_f_full_len_dirs - 1;

	size_t slen = (size_t)(s - str);
	s = str;

	char *buf = xnmalloc(slen + 1, sizeof(char));
	size_t i = 0;
	size_t cur_comps = 0;

	if (*s == '~') {
		buf[i++] = *s;
		cur_comps++;
	}

	while (*s) {
		if (*s != '/') {
			s++;
			continue;
		}

		buf[i++] = '/';
		cur_comps++;
		s++;

		if (cur_comps >= total_comps) {
			xstrsncpy(buf + i, s, slen - i + 1);
			break;
		}

		if (*s == '.')
			buf[i++] = *s++;

		if (conf.prompt_f_dir_len == 1) {
			const size_t bytes = copy_char(buf + i, s);
			i += bytes;
			s += bytes;
			continue;
		}

		size_t q = 0;
		while (*s && *s != '/' && q < (size_t)conf.prompt_f_dir_len) {
			const size_t bytes = copy_char(buf + i, s);
			i += bytes;
			s += bytes;
			q++;
		}
	}

	return buf;
}

static char *
gen_pwd(const int c)
{
	if (!workspaces[cur_ws].path)
		return NULL;

	char *temp = NULL;
	char *tmp_path = NULL;
	int free_tmp_path = 0;

	if (user.home
	&& strncmp(workspaces[cur_ws].path, user.home, user.home_len) == 0)
		tmp_path = home_tilde(workspaces[cur_ws].path, &free_tmp_path);

	if (!tmp_path)
		tmp_path = workspaces[cur_ws].path;

	if (c == 'W')
		temp = get_dir_basename(tmp_path);
	else if (c == 'p')
		temp = reduce_path(tmp_path);
	else if (c == 'f')
		temp = reduce_path_fish(tmp_path);
	else /* If c == 'w' */
		temp = savestring(tmp_path, strlen(tmp_path));

	if (free_tmp_path == 1)
		free(tmp_path);

	return temp;
}

static char *
gen_workspace(void)
{
	/* An int (or workspace name) + workspaces color + NUL byte */
	char s[NAME_MAX + sizeof(ws1_c) + 1];
	const char *cl = df_c;

	if (conf.colorize == 1) {
		switch (cur_ws + 1) {
		case 1: cl = *ws1_c ? ws1_c : DEF_WS1_C; break;
		case 2: cl = *ws2_c ? ws2_c : DEF_WS2_C; break;
		case 3: cl = *ws3_c ? ws3_c : DEF_WS3_C; break;
		case 4: cl = *ws4_c ? ws4_c : DEF_WS4_C; break;
		case 5: cl = *ws5_c ? ws5_c : DEF_WS5_C; break;
		case 6: cl = *ws6_c ? ws6_c : DEF_WS6_C; break;
		case 7: cl = *ws7_c ? ws7_c : DEF_WS7_C; break;
		case 8: cl = *ws8_c ? ws8_c : DEF_WS8_C; break;
		default: break;
		}
	}

	int len = 0;
	if (workspaces[cur_ws].name)
		len = snprintf(s, sizeof(s), "%s%s", cl, workspaces[cur_ws].name);
	else
		len = snprintf(s, sizeof(s), "%s%d", cl, cur_ws + 1);

	if (len <= 0)
		return NULL;

	return savestring(s, (size_t)len);
}

static char *
gen_exit_status(void)
{
	const size_t code_len = (size_t)DIGINUM(exit_code);
	const size_t len = code_len + 3 + ((size_t)MAX_COLOR * 2);

	char *temp = xnmalloc(len, sizeof(char));
	snprintf(temp, len, "%s%d\001%s\002",
		(exit_code == 0) ? (conf.colorize == 1 ? xs_c : "")
		: (conf.colorize == 1 ? xf_c : ""), exit_code, df_c);

	return temp;
}

static char *
gen_escape_char(void)
{
	char *temp = xnmalloc(2, sizeof(char));
	*temp = '\033';
	temp[1] = '\0';

	return temp;
}

static char *
gen_octal(char *line, const int c, char **end)
{
	char octal_string[4];
	xstrsncpy(octal_string, line, sizeof(octal_string));

	int n = octal2int(octal_string);
#ifdef CHAR_MAX
	/* Max octal number: 0177 (char is signed) or 0377 (char is unsigned). */
	if (n > CHAR_MAX)
		n = CHAR_MAX;
#else
	if (n > 127)
		n = 127;
#endif /* CHAR_MAX */

	char *temp = xnmalloc(3, sizeof(char));

	if (n == CTLESC || n == CTLNUL) {
		temp[0] = CTLESC;
		temp[1] = (char)n;
		temp[2] = '\0';
		if (end) *end = line + 2;
	} else if (n == -1) { /* Error */
		temp[0] = '\\';
		temp[1] = (const char)c;
		temp[2] = '\0';
	} else {
		temp[0] = (char)n;
		temp[1] = '\0';
		if (end) *end = line + 2;
	}

	return temp;
}

static char *
gen_profile(void)
{
	if (!alt_profile)
		return savestring("default", 7);

	return savestring(alt_profile, strlen(alt_profile));
}

static char *
gen_user_name(void)
{
	if (!user.name)
		return savestring(UNKNOWN_STR, sizeof(UNKNOWN_STR) - 1);

	return savestring(user.name, strlen(user.name));
}

static char *
gen_sort_name(void)
{
	const char *name = num_to_sort_name(conf.sort, 1);
	if (!name)
		return NULL;

	return strdup(name);
}

static char *
gen_hostname(const int c)
{
	/* Using strnlen() here avoids a Redhat hardened compilation warning. */
	char *temp = savestring(hostname, strnlen(hostname, sizeof(hostname)));
	if (c != 'h')
		return temp;

	char *ret = strchr(temp, '.');
	if (ret)
		*ret = '\0';

	return temp;
}

static char *
gen_user_flag(void)
{
	char *temp = xnmalloc(2, sizeof(char));
	*temp = user.uid == 0 ? ROOT_USR_CHAR : NON_ROOT_USR_CHAR;
	temp[1] = '\0';

	return temp;
}

static char *
gen_mode(void)
{
	char *temp = xnmalloc(2, sizeof(char));
	*temp = conf.light_mode == 1 ? LIGHT_MODE_CHAR : '\0';
	temp[1] = '\0';

	return temp;
}

static char *
gen_misc(const int c)
{
	char *temp = xnmalloc(2, sizeof(char));

	if (c == 'n')
		*temp = '\n';
	else if (c == 'r')
		*temp = '\r';
	else
		*temp = '\a';

	temp[1] = '\0';

	return temp;
}

static char *
gen_non_print_sequence(const int c)
{
	char *temp = xnmalloc(2, sizeof(char));
	*temp = c == '[' ? RL_PROMPT_START_IGNORE : RL_PROMPT_END_IGNORE;
	temp[1] = '\0';

	return temp;
}

static char *
gen_shell_name(void)
{
	if (user.shell && user.shell_basename)
		return savestring(user.shell_basename, strlen(user.shell_basename));

	return savestring("unknown", 7);
}

/* Append the string STR to the buffer BUF, whose current length is BUF_LEN. */
static void
add_string(char **str, char **buf, size_t *buf_len)
{
	if (!*str)
		return;

	const size_t orig_buf_len = *buf_len;
	*buf_len += strlen(*str);

	const size_t l = *buf_len + 1;
	if (!*buf) {
		*buf = xnmalloc(l, sizeof(char));
		*(*buf) = '\0';
	} else {
		*buf = xnrealloc(*buf, l, sizeof(char));
	}

	xstrncat(*buf, orig_buf_len, *str, l);
	free(*str);
	*str = NULL;
}

#ifndef NO_WORDEXP
static void
reset_ifs(const char *value)
{
	if (value)
		setenv("IFS", value, 1);
	else
		unsetenv("IFS");
}

static void
substitute_cmd(char *cmd, char **buf, size_t *buf_len, char **cmd_end)
{
	if (!cmd || !buf || !buf_len)
		return;

	char *p = strchr(cmd, ')');
	if (!p)
		return; /* No trailing parenthesis */

	/* Extract the command to be executed */
	const char c = p[1];
	p[1] = '\0';

	char *old_value = xgetenv("IFS", 1);
	setenv("IFS", "", 1);

	wordexp_t wordbuf;
	const int ret = wordexp(cmd, &wordbuf, 0);

	p[1] = c;

	/* Set cmd_end to the trailing parenthesis to continue processing. */
	if (cmd_end)
		*cmd_end = p;

	reset_ifs(old_value);
	free(old_value);

	if (ret != 0)
		return;

	if (wordbuf.we_wordc == 0) {
		wordfree(&wordbuf);
		return;
	}

	for (size_t j = 0; j < wordbuf.we_wordc; j++) {
		const size_t cur_buf_len = *buf_len;

		*buf_len += strlen(wordbuf.we_wordv[j]);
		if (!*buf) {
			*buf = xnmalloc(*buf_len + 1, sizeof(char));
			*(*buf) = '\0';
		} else {
			*buf = xnrealloc(*buf, *buf_len + 1, sizeof(char));
		}

		xstrncat(*buf, cur_buf_len, wordbuf.we_wordv[j], *buf_len + 1);
	}

	wordfree(&wordbuf);
}
#endif /* !NO_WORDEXP */

static char *
gen_emergency_prompt(void)
{
	static int f = 0;
	if (f == 0) {
		f = 1;
		xerror("%s: %s\n", PROGRAM_NAME, EMERGENCY_PROMPT_MSG);
	}

	return savestring(EMERGENCY_PROMPT, sizeof(EMERGENCY_PROMPT) - 1);
}

static char *
gen_stats_str(const int flag)
{
	size_t val = 0;

	switch (flag) {
	case STATS_BLK: val = stats.block_dev; break;
	case STATS_BROKEN_L: val = stats.broken_link; break;
	case STATS_CAP: val = stats.caps; break;
	case STATS_CHR: val = stats.char_dev; break;
	case STATS_DIR: val = stats.dir; break;
#ifdef SOLARIS_DOORS
	case STATS_DOOR: val = stats.door; break;
	case STATS_PORT: val = stats.port; break;
#endif /* SOLARIS_DOORS */
	case STATS_EXE: val = stats.exec; break;
	case STATS_EXTENDED: val = stats.extended; break;
	case STATS_FIFO: val = stats.fifo; break;
	case STATS_HIDDEN: val = stats.hidden; break;
	case STATS_LNK: val = stats.link; break;
	case STATS_MULTI_L: val = stats.multi_link; break;
	case STATS_OTHER_W: val = stats.other_writable; break;
	case STATS_REG: val = stats.reg; break;
	case STATS_SUID: val = stats.suid; break;
	case STATS_SGID: val = stats.sgid; break;
	case STATS_SOCK: val = stats.socket; break;
	case STATS_STICKY: val = stats.sticky; break;
	case STATS_UNKNOWN: val = stats.unknown; break;
	case STATS_UNSTAT: val = stats.unstat; break;
	case STATS_NON_DIR:
		val = stats.reg + stats.block_dev + stats.char_dev
			+ stats.socket + stats.fifo;
		break;
	default: break;
	}

	char *p = NULL;
	if (val != 0) {
		p = xnmalloc(MAX_INT_STR, sizeof(char));
		snprintf(p, MAX_INT_STR, "%zu", val);
	} else {
		p = xnmalloc(2, sizeof(char));
		*p = '-';
		p[1] = '\0';
	}

	return p;
}

static size_t
count_autocmd_matches(void)
{
	if (autocmds_n == 0)
		return 0;

	size_t c = 0;
	for (size_t i = 0; i < autocmds_n; i++)
		c += (autocmds[i].match == 1);

	return c;
}

static char *
gen_notification(const int flag)
{
	const size_t len = MAX_INT_STR + 2;

	char *p = xnmalloc(len, sizeof(char));
	*p = '\0';

	switch (flag) {
	case NOTIF_AUTOCMD:
		if (count_autocmd_matches() > 0)
			{ *p = 'A'; p[1] = '\0'; }
		break;
	case NOTIF_ERROR:
		if (msgs.error > 0)
			snprintf(p, len, "E%zu", msgs.error);
		break;
	case NOTIF_NOTICE:
		if (msgs.notice > 0)
			snprintf(p, len, "N%zu", msgs.notice);
		break;
	case NOTIF_WARNING:
		if (msgs.warning > 0)
			snprintf(p, len, "W%zu", msgs.warning);
		break;
	case NOTIF_ROOT:
		if (user.uid == 0)
			{ *p = 'R'; p[1] = '\0'; }
		break;
	case NOTIF_SEL:
		if (sel_n > 0)
			snprintf(p, len, "%c%zu", SELFILE_CHR, sel_n);
		break;
	case NOTIF_TRASH:
		if (trash_n > 0)
			snprintf(p, len, "T%zu", trash_n);
		break;
	default: break;
	}

	return p;
}

static char *
gen_nesting_level(const int mode)
{
	char *p = NULL;

	if (mode == 'i') {
		p = xnmalloc(MAX_INT_STR, sizeof(char));
		snprintf(p, MAX_INT_STR, "%d", nesting_level);
		return p;
	}

	/* I == full mode (nothing if first level) */
	if (nesting_level <= 1) {
		p = xnmalloc(1, sizeof(char));
		*p = '\0';
		return p;
	}

	const size_t len = MAX_INT_STR + 3;
	p = xnmalloc(len, sizeof(char));
	snprintf(p, len, "(%d)", nesting_level);

	return p;
}

static const char *
get_color_attribute(const char *line)
{
	if (!line || !line[0] || line[1] != ':')
		return NULL;

	switch (line[0]) {
	case 'b': return "1;"; /* Bold */
	case 'd': return "2;"; /* Dim */
	case 'i': return "3;"; /* Italic */
	case 'n': return "0;"; /* Normal/reset */
	case 'r': return "7;"; /* Reverse */
	case 's': return "9;"; /* Strikethrough */
	case 'u': return "4;"; /* Underline */
	case 'B': /* fallthrough */
	case 'D': return "22;"; /* Disable bold/dim: normal intensity */
	case 'I': return "23;"; /* Disable italic */
	case 'R': return "27;"; /* Disable reverse */
	case 'S': return "29;"; /* Disable strikethrough */
	case 'U': return "24;"; /* Disable underline */
	case 'K': return "49;"; /* Disable background (terminal default) */
	case 'N': return "39;"; /* Disable foreground (terminal default) */
	default: return NULL;
	}
}

/* Return 1 if the string S is valid hex color, or 0 otherwise. */
static int
is_valid_hex(const char *s)
{
	if (!s || !*s)
		return 0;

	size_t i;
	for (i = 0; s[i]; i++) {
		if (!IS_HEX_DIGIT(s[i]))
			return 0;
	}

	return (i == 3 || i == 6);
}

/* Convert a color notation ("%{color}") into an SGR escape sequence.
 * On success returns a malloc'd NUL-terminated string containing the
 * escape sequence (caller must free it), and sets COLOR_END (if not NULL)
 * to point at the closing '}' of the color token. On error, NULL is returned
 * and COLOR_END is left unmodified. */
char *
gen_color(char *color_begin, char **color_end)
{
	if (!color_begin || !*color_begin)
		return NULL;

	char *l = color_begin;
	if (*l != '%' || l[1] != '{' || !l[2])
		return NULL;

	l += 2; /* L is now "color}" */

	const int bg = (l[0] == 'k' && l[1] == ':' && l[2]);
	const char *attr = bg == 0 ? get_color_attribute(l) : NULL;
	if (bg == 1 || attr)
		l += 2; /* Remove background/attribute prefix ("x:") */

	/* Is color bright? */
	const int br = (l[0] == 'b' && l[1] == 'r' && l[2]);
	if (br == 1)
		l += 2;

	/* Disable attribute? */
	const int attr_off = (l[0] == 'n' && l[1] == 'o' && l[2]);
	if (attr_off == 1)
		l += 2;

	char *p = strchr(l, '}');
	if (!p)
		return NULL;

	*p = '\0'; /* Remove trailing '}': now we have "color" */

#define C_START RL_PROMPT_START_IGNORE
#define C_END   RL_PROMPT_END_IGNORE
#define C_ESC   0x1b
#define C_LEN   25 /* 25 == max color length (rgb) */

#define GEN_COLOR(s1, s2) (snprintf(temp, C_LEN, "%c%c[%s%sm%c", C_START, \
	C_ESC, attr ? attr : "", bg == 1 ? (s1) : (s2), C_END));
#define GEN_ATTR(s) (snprintf(temp, C_LEN, "%c%c[%s%sm%c", C_START, C_ESC, \
	attr ? attr : "", (s), C_END))

	char *temp = xnmalloc(C_LEN, sizeof(char));
	int n = -1;

	/* 'bold' and 'blue' are used more often than 'black': check them first. */
	if (l[0] == 'b' && strcmp(l, "bold") == 0) {
		GEN_ATTR(attr_off ? "22" : "1");
	} else if (l[0] == 'b' && strcmp(l, "blue") == 0) {
		GEN_COLOR(br ? "104" : "44", br ? "94" : "34");
	} else if (l[0] == 'b' && strcmp(l, "black") == 0) {
		GEN_COLOR(br ? "100" : "40", br ? "90" : "30");
	/* 'reset' is used more often than 'red' and 'reverse': check it first. */
	} else if (l[0] == 'r' && strcmp(l, "reset") == 0) {
		GEN_ATTR("0");
	} else if (l[0] == 'r' && strcmp(l, "red") == 0) {
		GEN_COLOR(br ? "101" : "41", br ? "91" : "31");
	} else if (l[0] == 'r' && strcmp(l, "reverse") == 0) {
		GEN_ATTR(attr_off ? "27" : "7");
	} else if (l[0] == 'g' && strcmp(l, "green") == 0) {
		GEN_COLOR(br ? "102" : "42", br ? "92" : "32");
	} else if (l[0] == 'y' && strcmp(l, "yellow") == 0) {
		GEN_COLOR(br ? "103" : "43", br ? "93" : "33");
	} else if (l[0] == 'm' && strcmp(l, "magenta") == 0) {
		GEN_COLOR(br ? "105" : "45", br ? "95" : "35");
	} else if (l[0] == 'c' && strcmp(l, "cyan") == 0) {
		GEN_COLOR(br ? "106" : "46", br ? "96" : "36");
	} else if (l[0] == 'w' && strcmp(l, "white") == 0) {
		GEN_COLOR(br ? "107" : "47", br ? "97" : "37");
	} else if (l[0] == 'd' && strcmp(l, "dim") == 0) {
		GEN_ATTR(attr_off ? "22" : "2");
	} else if (l[0] == 'i' && strcmp(l, "italic") == 0) {
		GEN_ATTR(attr_off ? "23" : "3");
	} else if (l[0] == 'u' && strcmp(l, "underline") == 0) {
		GEN_ATTR(attr_off ? "24" : "4");
	} else if (l[0] == 's' && strcmp(l, "strike") == 0) {
		GEN_ATTR(attr_off ? "29" : "9");
	} else if (l[0] == 'f' && l[1] == 'g' && strcmp(l + 2, "reset") == 0) {
		GEN_ATTR("39");
	} else if (l[0] == 'b' && l[1] == 'g' && strcmp(l + 2, "reset") == 0) {
		GEN_ATTR("49");
	} else if (IS_DIGIT(l[0]) && (!l[1] || (is_number(l + 1)
	&& (n = xatoi(l)) <= 255))) {
		if (!l[1])
			n = l[0] - '0';
		snprintf(temp, C_LEN, "%c%c[%s%s;5;%dm%c",
			C_START, C_ESC, attr ? attr : "",
			bg == 1 ? "48" : "38", n, C_END);
	} else if (l[0] == '#' && is_valid_hex(l + 1)) {
		/* Fallback values in case get_rgb() returns prematurely (error) */
		int a = -1, r = 100, g = 100, b = 100;
		get_rgb(l + 1, &a, &r, &g, &b);
		snprintf(temp, C_LEN, "%c%c[%s%s;2;%d;%d;%dm%c",
			C_START, C_ESC, attr ? attr : "",
			bg == 1 ? "48" : "38", r, g, b, C_END);
	} else {
		*p = '}'; /* Restore the trailing '}' */
		free(temp);
		return NULL;
	}

	*p = '}'; /* Restore the trailing '}' */
	if (color_end)
		*color_end = p; /* Set to the end of the color notation */
	return temp;

#undef GEN_COLOR
#undef GEN_ATTR
#undef C_START
#undef C_END
#undef C_ESC
#undef C_LEN
}

#ifndef NO_WORDEXP
static char *
check_mod_paths_cache(const char *name)
{
	size_t i = 0;

	static int init = 0;
	if (init == 0) { /* Initialize the cache */
		init = 1;
		for (i = 0; i < MAX_PMOD_PATHS; i++) {
			p_mod_paths[i].path[0] = '\0';
			p_mod_paths[i].name = NULL;
		}
	}

	for (i = 0; i < MAX_PMOD_PATHS && p_mod_paths[i].name; i++) {
		if (*name == *p_mod_paths[i].name
		&& strcmp(name + 1, p_mod_paths[i].name + 1) == 0)
			return &p_mod_paths[i].path[0];
	}

	return NULL;
}

static void
cache_pmod_path(const char *mod_path)
{
	if (!mod_path || !*mod_path || p_mod_paths_n == MAX_PMOD_PATHS)
		return;

	xstrsncpy(p_mod_paths[p_mod_paths_n].path, mod_path, strlen(mod_path) + 1);

	char *p = strrchr(p_mod_paths[p_mod_paths_n].path, '/');
	if (p && p[1])
		p_mod_paths[p_mod_paths_n].name = p + 1;

	p_mod_paths_n++;
}

static char *
get_prompt_module_path(const char *name)
{
	char *cached_path = check_mod_paths_cache(name);
	if (cached_path)
		return cached_path;

	struct stat a;
	static char m_path[PATH_MAX];

	if (plugins_dir && *plugins_dir) {
		snprintf(m_path, sizeof(m_path), "%s/%s", plugins_dir, name);
		if (stat(m_path, &a) != -1) {
			cache_pmod_path(m_path);
			return &m_path[0];
		}
	}

	if (data_dir && *data_dir) {
		snprintf(m_path, sizeof(m_path), "%s/%s/plugins/%s",
			data_dir, PROGRAM_NAME, name);
		if (stat(m_path, &a) != -1) {
			cache_pmod_path(m_path);
			return &m_path[0];
		}
	}

	return NULL;
}

static void
run_prompt_module(char *module, char **buf, size_t *buf_len, char **end)
{
	if (!module || !buf || !buf_len)
		return;

	char *p = strchr(module, '}');
	if (!p)
		return;

	*p = '\0';

	const char *p_path = get_prompt_module_path(module + 1);
	if (p_path) {
		char cmd[PATH_MAX + 4];
		snprintf(cmd, sizeof(cmd), "$(%s)", p_path);
		substitute_cmd(cmd, buf, buf_len, NULL);
	}

	*p = '}';

	if (end)
		*end = p;
}
#endif /* !NO_WORDEXP */

static char *
gen_last_cmd_time(void)
{
	if (last_cmd_time < (double)conf.prompt_b_min)
		return NULL;

	const int precision = conf.prompt_b_precision;
	const int len = snprintf(NULL, 0, "%.*f", precision, last_cmd_time);
	if (len < 0)
		return NULL;

	char *temp = xnmalloc((size_t)len + 1, sizeof(char));
	snprintf(temp, (size_t)len + 1, "%.*f", precision, last_cmd_time);

	return temp;
}

static char *
gen_cwd_perms(void)
{
	struct stat a;
	if (!workspaces || !workspaces[cur_ws].path
	|| stat(workspaces[cur_ws].path, &a) == -1)
		return savestring(UNKNOWN_STR, sizeof(UNKNOWN_STR) - 1);

	char *buf = xnmalloc(5, sizeof(char));
	snprintf(buf, 5, "%04o", a.st_mode & 07777);

	return buf;
}

/* Decode the prompt string LINE, as taken from the configuration file,
 * expanding escape sequences, prompt color notation, prompt modules, and
 * performning command substitution. Returns a pointer to the expanded prompt
 * (must be free'd by the caller) or NULL on error. */
char *
decode_prompt(char *line)
{
	if (!line)
		return NULL;

	char *tmp = NULL;
	char *buf = NULL;
	char *ptr = NULL;
	size_t buf_len = 0;
	int c;

	while ((c = (int)*line++)) {
		/* Color notation: "%{color}" */
		if (c == '%' && *line == '{' && line[1]) {
			char *color_end = NULL;
			char *color_begin = line - 1; /* Points to '%' */
			tmp = gen_color(color_begin, &color_end);
			if (!tmp) {
				tmp = savestring("%", 1);
				color_end = color_begin;
			}

			add_string(&tmp, &buf, &buf_len);
			line = color_end + 1;
		}

		/* We have an escape char */
		else if (c == '\\') {
			c = (int)*line; /* c holds the character next to the backslash */
			switch (c) {
			/* File statistics */
			case 'B': tmp = gen_stats_str(STATS_BLK); break;
			case 'C': tmp = gen_stats_str(STATS_CHR); break;
			case 'D': tmp = gen_stats_str(STATS_DIR); break;
			case 'E': tmp = gen_stats_str(STATS_EXTENDED); break;
			case 'F': tmp = gen_stats_str(STATS_FIFO); break;
			case 'G': tmp = gen_stats_str(STATS_SGID); break;
			case 'K': tmp = gen_stats_str(STATS_SOCK); break;
			case 'L': tmp = gen_stats_str(STATS_LNK); break;
			case 'M': tmp = gen_stats_str(STATS_MULTI_L); break;
			case 'o': tmp = gen_stats_str(STATS_BROKEN_L); break;
			case 'O': tmp = gen_stats_str(STATS_OTHER_W); break;
			case 'Q': tmp = gen_stats_str(STATS_NON_DIR); break;
			case 'R': tmp = gen_stats_str(STATS_REG); break;
			case 'U': tmp = gen_stats_str(STATS_SUID); break;
			case 'x': tmp = gen_stats_str(STATS_CAP); break;
			case 'X': tmp = gen_stats_str(STATS_EXE); break;
			case '.': tmp = gen_stats_str(STATS_HIDDEN); break;
			case '"': tmp = gen_stats_str(STATS_STICKY); break;
			case '?': tmp = gen_stats_str(STATS_UNKNOWN); break;
			case '!': tmp = gen_stats_str(STATS_UNSTAT); break;
#ifdef SOLARIS_DOORS
			case '>': tmp = gen_stats_str(STATS_DOOR); break;
			case '<': tmp = gen_stats_str(STATS_PORT); break;
#endif /* SOLARIS_DOORS */

			case '*': tmp = gen_notification(NOTIF_SEL); break;
			case '%': tmp = gen_notification(NOTIF_TRASH); break;
			case '#': tmp = gen_notification(NOTIF_ROOT); break;
			case ')': tmp = gen_notification(NOTIF_WARNING); break;
			case '(': tmp = gen_notification(NOTIF_ERROR); break;
			case '=': tmp = gen_notification(NOTIF_NOTICE); break;

			case 'v': tmp = gen_rl_vi_mode(1); break;
			case 'y': tmp = gen_notification(NOTIF_AUTOCMD); break;

			case 'z': /* Exit status of last executed command */
				tmp = gen_exit_status(); break;

			case 'e': /* Escape char */
				tmp = gen_escape_char(); break;

			case 'j': tmp = gen_cwd_perms(); break;

			case '0': /* fallthrough */ /* Octal char */
			case '1': /* fallthrough */
			case '2': /* fallthrough */
			case '3': /* fallthrough */
			case '4': /* fallthrough */
			case '5': /* fallthrough */
			case '6': /* fallthrough */
			case '7':
				ptr = NULL;
				tmp = gen_octal(line, c, &ptr);
				if (ptr) line = ptr;
				break;

			case 'c': /* Program name */
				tmp = savestring(PROGRAM_NAME, sizeof(PROGRAM_NAME) - 1);
				break;

			case 'b': /* Elapsed time of the last executed command */
				tmp = gen_last_cmd_time(); break;

			case 'P': /* Current profile name */
				tmp = gen_profile(); break;

			case 't': /* fallthrough */ /* Time: 24-hour HH:MM:SS format */
			case 'T': /* fallthrough */ /* 12-hour HH:MM:SS format */
			case 'A': /* fallthrough */ /* 24-hour HH:MM format */
			case '@': /* fallthrough */ /* 12-hour HH:MM:SS am/pm format */
			case 'd': /* Date: abrev_weak_day, abrev_month_day month_num */
				tmp = gen_time(c); break;

			case 'u': /* User name */
				tmp = gen_user_name(); break;

			case 'g':
				tmp = gen_sort_name(); break;

			case 'h': /* fallthrough */ /* Hostname up to first '.' */
			case 'H': /* Full hostname */
				tmp = gen_hostname(c); break;

			case 'i': /* fallthrough */ /* Nest level (number only) */
			case 'I': /* Nest level (full format) */
				tmp = gen_nesting_level(c); break;

			case 's': /* Shell name (after last slash)*/
				tmp = gen_shell_name(); break;

			case 'S': /* Current workspace */
				tmp = gen_workspace(); break;

			case 'l': /* Current mode */
				tmp = gen_mode(); break;

			case 'p': /* fallthrough */ /* Abbreviated if longer than PathMax */
			case 'f': /* fallthrough */ /* Abbreviated, fish-like */
			case 'w': /* fallthrough */ /* Full PWD */
			case 'W': /* Short PWD */
				tmp = gen_pwd(c); break;

			case '$': /* '$' or '#' for normal and root user */
				tmp = gen_user_flag();	break;

			case 'a': /* fallthrough */ /* Bell character */
			case 'r': /* fallthrough */ /* Carriage return */
			case 'n': /* fallthrough */ /* New line char */
				tmp = gen_misc(c); break;

			case '[': /* fallthrough */ /* Begin a sequence of non-printing characters */
			case ']': /* End the sequence */
				tmp = gen_non_print_sequence(c); break;

			case '\\': /* Literal backslash */
				tmp = savestring("\\", 1); break;

			default: /* Unknown sequence: copy it verbatim */
				tmp = savestring("\\ ", 2);
				tmp[1] = (char)c;
				break;
			}

			add_string(&tmp, &buf, &buf_len);
			line++;
		}

		/* If not an escape code, check for command substitution, and if not,
		 * just add whatever char is there. */
		else {
			/* Remove non-escaped quotes */
			if (c == '\'' || c == '"')
				continue;

#ifndef NO_WORDEXP
			/* Command substitution: "$(cmd)" */
			if (c == '$' && *line == '(') {
				char *cmd_begin = line - 1;
				char *cmd_end = NULL;
				substitute_cmd(cmd_begin, &buf, &buf_len, &cmd_end);
				if (cmd_end)
					/* Line points now after the trailing parenthesis */
					line = cmd_end + 1;
				continue;
			}

			/* Prompt module: "${module}" */
			if (c == '$' && *line == '{') {
				char *end = NULL;
				run_prompt_module(line, &buf, &buf_len, &end);
				if (end)
					/* Line points now after the trailing bracket */
					line = end + 1;
				continue;
			}
#endif /* !NO_WORDEXP */

			const size_t new_len = buf_len + 2;
			buf = xnrealloc(buf, new_len, sizeof(char));
			buf[buf_len++] = (char)c;

			buf[buf_len] = '\0';
		}
	}

	/* Remove trailing new line char, if any */
	if (buf && buf_len > 0 && buf[buf_len - 1] == '\n')
		buf[buf_len - 1] = '\0';

	/* Emergency prompt, just in case something went wrong */
	if (!buf)
		buf = gen_emergency_prompt();

	return buf;
}

/* Make sure CWD exists; if not, go up to the parent, and so on */
static void
check_cwd(void)
{
	int cwd_change = 0;

	while (xchdir(workspaces[cur_ws].path, SET_TITLE) != FUNC_SUCCESS) {
		char *ret = strrchr(workspaces[cur_ws].path, '/');
		if (ret && ret != workspaces[cur_ws].path) {
			*ret = '\0';
			cwd_change = 1;
		} else {
			break;
		}
	}

	if (cwd_change == 1 && conf.autols == 1)
		refresh_screen();
}

/* Remove trailing slashes from DIR. */
static void
remove_trailing_slashes(char *dir)
{
	if (!dir || !*dir)
		return;

	const size_t dir_len = strlen(dir);
	for (size_t i = dir_len - 1; dir[i] && i > 0; i--) {
		if (dir[i] != '/')
			return;
		dir[i] = '\0';
	}
}

static void
print_user_message(void)
{
	if (!conf.welcome_message_str || !*conf.welcome_message_str)
		return;

	char *s = conf.welcome_message_str;

	if (!strchr(s, '\\')) {
		printf("%s%s%s\n", wc_c, s, df_c);
		return;
	}

	char *tmp = NULL;

	fputs(wc_c, stdout);

	while (*s) {
		if (*s == '\\' && s[1]) {
			s++;
			char *end = NULL;
			if (*s >= '0' && *s <= '7' && (tmp = gen_octal(s, *s, &end))) { /* NOLINT */
				fputs(tmp, stdout);
				free(tmp);
				if (end) s = end;
				s++;
			} else if (*s == 'e' && (tmp = gen_escape_char())) {
				fputs(tmp, stdout);
				free(tmp);
				s++;
			} else if (*s == 'n') {
				putchar('\n');
				s++;
			} else {
				putchar(*s);
				s++;
			}
		} else {
			putchar(*s);
			s++;
		}
	}

	putchar('\n');
	fputs(df_c, stdout);
}

static void
print_welcome_msg(void)
{
	static int message_shown = 0;

	if (message_shown == 1 || conf.welcome_message == 0)
		return;

	if (conf.welcome_message_str != NULL)
		print_user_message();
	else
		printf("%s%s\n%s", wc_c, DEF_WELCOME_MESSAGE_STR, df_c);

	printf("%s\n", _(HELP_MESSAGE));

	message_shown = 1;
}

static void
print_tips_func(void)
{
	if (conf.tips == 0)
		return;

	static int first_run = 1;
	if (first_run == 1) {
		print_tips(0);
		first_run = 0;
	}
}

static void
run_prompt_cmds(void)
{
	if (conf.ext_cmd_ok == 0 || prompt_cmds_n == 0)
		return;

	const int tflags = flags;
	flags &= ~DELAYED_REFRESH;

	for (size_t i = 0; i < prompt_cmds_n; i++) {
		if (xargs.secure_cmds == 0
		|| sanitize_cmd(prompt_cmds[i], SNT_PROMPT) == FUNC_SUCCESS)
			launch_execl(prompt_cmds[i]);
	}

	flags = tflags;
}

#ifndef _NO_TRASH
static void
update_trash_indicator(void)
{
	static time_t trash_files_dir_mtime = 0;

	if (trash_ok == 0)
		return;

	struct stat a;
	if (stat(trash_files_dir, &a) == -1)
		return;

	if (trash_files_dir_mtime == a.st_mtime)
		return;

	trash_files_dir_mtime = a.st_mtime;

	const filesn_t n = count_dir(trash_files_dir, NO_CPOP);
	trash_n = n <= 2 ? 0 : (size_t)n - 2;
}
#endif /* !_NO_TRASH */

static void
setenv_prompt(const size_t ac_matches)
{
	if (prompt_notif == 1)
		return;

	/* Set environment variables with clifm state information
	 * (sel files, trash, stealth mode, messages, workspace, and last exit
	 * code) to be handled by the prompt itself. */
	setenv("CLIFM_STAT_SEL", xitoa((long long)sel_n), 1);
#ifndef _NO_TRASH
	setenv("CLIFM_STAT_TRASH", xitoa((long long)trash_n), 1);
#endif /* !_NO_TRASH */
	setenv("CLIFM_STAT_AUTOCMD", ac_matches > 0 ? "1" : "0", 1);
	setenv("CLIFM_STAT_ERROR_MSGS", xitoa((long long)msgs.error), 1);
	setenv("CLIFM_STAT_WARNING_MSGS", xitoa((long long)msgs.warning), 1);
	setenv("CLIFM_STAT_NOTICE_MSGS", xitoa((long long)msgs.notice), 1);
	setenv("CLIFM_STAT_WS", xitoa((long long)cur_ws + 1), 1);
	setenv("CLIFM_STAT_EXIT", xitoa((long long)exit_code), 1);
	setenv("CLIFM_STAT_ROOT", user.uid == 0 ? "1" : "0", 1);
	setenv("CLIFM_STAT_STEALTH", (xargs.stealth_mode == 1) ? "1" : "0", 1);
}

static size_t
set_prompt_length(const size_t decoded_prompt_len, const size_t ac_matches)
{
	size_t len = 0;

	if (prompt_notif == 1) {
		len = (size_t)(decoded_prompt_len
		+ (ac_matches > 0 ? N_IND : 0)
		+ (xargs.stealth_mode == 1 ? STEALTH_IND_SIZE : 0)
		+ (user.uid == 0 ? ROOT_IND_SIZE : 0)
		+ (conf.readonly == 1 ? RDONLY_IND_SIZE : 0)
		+ (sel_n > 0 ? N_IND : 0)
		+ (trash_n > 0 ? N_IND : 0)
		+ (msgs.error > 0 ? N_IND : 0)
		+ (msgs.warning > 0 ? N_IND : 0)
		+ (msgs.notice > 0 ? N_IND : 0)
		+ 6 + sizeof(tx_c) + 1 + 2);
	} else {
		len = (size_t)(decoded_prompt_len + 6 + sizeof(tx_c) + 1);
	}

	return len;
}

static char *
construct_prompt(const char *decoded_prompt, const size_t ac_matches)
{
	const char *rl_vi_mode = NULL;
	/* Construct indicators: MSGS (ERR, WARN, and NOTICE), SEL, and TRASH */
	char err_ind[N_IND], warn_ind[N_IND], notice_ind[N_IND],
		trash_ind[N_IND], sel_ind[N_IND], acmd_ind[N_IND];
	*err_ind = *warn_ind = *notice_ind = *trash_ind = *sel_ind = '\0';
	*acmd_ind = '\0';

	if (prompt_notif == 1) {
		if (rl_editing_mode == RL_VI_MODE)
			rl_vi_mode = gen_rl_vi_mode(0);
		if (ac_matches > 0)
			snprintf(acmd_ind, N_IND, "%sA%s", ac_c, RL_NC);
		if (msgs.error > 0)
			snprintf(err_ind, N_IND, "%sE%zu%s", em_c, msgs.error, RL_NC);
		if (msgs.warning > 0)
			snprintf(warn_ind, N_IND, "%sW%zu%s", wm_c, msgs.warning, RL_NC);
		if (msgs.notice > 0)
			snprintf(notice_ind, N_IND, "%sN%zu%s", nm_c, msgs.notice, RL_NC);
		if (trash_n > 0)
			snprintf(trash_ind, N_IND, "%sT%zu%s",
				ti_c, trash_n, RL_NC);
		if (sel_n > 0)
			snprintf(sel_ind, N_IND, "%s%c%zu%s", li_c, SELFILE_CHR,
				sel_n, RL_NC);
	}

	const size_t prompt_len = (rl_vi_mode ? strlen(rl_vi_mode) : 0) +
		set_prompt_length(strlen(decoded_prompt), ac_matches);
	char *the_prompt = xnmalloc(prompt_len, sizeof(char));

	if (prompt_notif == 1) {
		snprintf(the_prompt, prompt_len,
			"%s%s%s%s%s%s%s%s%s%s%s%s%s%s\001%s\002",
			rl_vi_mode ? rl_vi_mode : "",
			acmd_ind,
			(user.uid == 0) ? (conf.colorize == 1
				? ROOT_IND : ROOT_IND_NO_COLOR) : "",
			(conf.readonly == 1) ? ro_c : "",
			(conf.readonly == 1) ? RDONLY_IND : "",
			err_ind, warn_ind, notice_ind,
			(xargs.stealth_mode == 1) ? si_c : "",
			(xargs.stealth_mode == 1) ? STEALTH_IND : "",
			trash_ind, sel_ind,
			decoded_prompt, RL_NC, tx_c);
	} else {
		snprintf(the_prompt, prompt_len, "%s%s\001%s\002",
			decoded_prompt, RL_NC, tx_c);
	}

	return the_prompt;
}

static void
print_prompt_messages(void)
{
	for (size_t i = 0; i < msgs_n; i++) {
		if (messages[i].read == 1)
			continue;

		fputs(messages[i].text, stderr);
		messages[i].read = 1;
	}

	print_msg = 0;
}

static void
initialize_prompt_data(const int prompt_flag, size_t *ac_matches)
{
	check_cwd();
	remove_trailing_slashes(workspaces[cur_ws].path);
	print_welcome_msg();
	print_tips_func();

	/* If autols is disabled, and since terminal dimensions are gathered
	 * in list_dir() via get_term_size(), let's get terminal dimensions
	 * here. We need them to print suggestions. */
	if (conf.autols == 0 && conf.suggestions == 1)
		get_term_size();

	/* Set foreground color to default. */
	fputs(df_c, stdout);
	fflush(stdout);

	/* If just updating the prompt, there's no need to run prompt commands. */
	if (prompt_flag != PROMPT_UPDATE)
		run_prompt_cmds();

#ifndef _NO_TRASH
	update_trash_indicator();
#endif /* !_NO_TRASH */
	get_sel_files();

	*ac_matches = conf.autocmd_msg == AUTOCMD_MSG_PROMPT
		? count_autocmd_matches() : 0;
	setenv_prompt(*ac_matches);

	args_n = 0;
	curhistindex = current_hist_n;
#ifndef _NO_SUGGESTIONS
	if (wrong_cmd == 1) {
		rl_delete_text(0, rl_end);
		rl_point = rl_end = 0;
		recover_from_wrong_cmd();
	}
#endif /* !_NO_SUGGESTIONS */

	if (print_msg == 1 && msgs_n > 0)
		print_prompt_messages();
}

static void
log_and_record(char *input)
{
	if (conf.log_cmds == 1) {
		free(last_cmd);
		last_cmd = savestring(input, strlen(input));
		log_cmd();
	}

	if (record_cmd(input) == 1)
		add_to_cmdhist(input);
}

/* UTF-8 version of get_prompt_len(). */
static int
get_rprompt_len_utf8(char *rprompt)
{
	if (!rprompt || !*rprompt)
		return 0;

	char *nl = strchr(rprompt, '\n');
	if (nl)
		*nl = '\0';

	static wchar_t buf[NAME_MAX];
	*buf = L'\0';
	if (mbstowcs(buf, rprompt, NAME_MAX) == (size_t)-1)
		return 0;

	size_t i = 0;
	int len = 0;

	while (buf[i]) {
		if (buf[i] == '\x1b' && buf[i + 1] == '[') {
			const wchar_t *tmp = wcschr(buf + i + 1, L'm');
			if (tmp)
				i += (size_t)(tmp - (buf + i) + 1);
		} else if (buf[i] == 001) {
			const wchar_t *tmp = wcschr(buf + i, L'\002');
			if (tmp)
				i += (size_t)(tmp - (buf + i) + 1);
		} else {
			len += wcwidth(buf[i++]);
		}
	}

	return len;
}

/* Return the printable length of the string RPROMPT. Zero is returned on error. */
static int
get_rprompt_len(char *rprompt)
{
	char *p = rprompt;
	while (*p) {
		if (IS_UTF8_CHAR(*p))
			return get_rprompt_len_utf8(rprompt);
		p++;
	}

	p = rprompt;
	int len = 0;

	while (*p) {
		if (*p == '\n') {
			*p = '\0';
			break;
		} else if (*p == '\x1b' && p[1] == '[') {
			char *tmp = strchr(p, 'm');
			if (tmp)
				p = tmp + 1;
		} else if (*p == 001) {
			char *tmp = strchr(p, '\002');
			if (tmp)
				p = tmp + 1;
		} else {
			len++;
			p++;
		}
	}

	return len;
}

static void
print_right_prompt(void)
{
	char *rprompt = decode_prompt(conf.rprompt_str);
	const int len = rprompt ? get_rprompt_len(rprompt) : 0;
	if (len <= 0 || len >= term_cols) {
		free(rprompt);
		return;
	}

	MOVE_CURSOR_RIGHT(term_cols);
	MOVE_CURSOR_LEFT(len);
	fputs(rprompt, stdout);
	MOVE_CURSOR_LEFT(term_cols);
	free(rprompt);
}

/* Some commands take '!' as parameter modifier: quick search, 'filter',
 * and 'sel', in which case history expansion must not be performed.
 * Return 1 if we have one of these commands or 0 otherwise. */
static int
exclude_from_history(const char *s)
{
	if (*s == '/'
	|| (*s == 's' && (s[1] == ' ' || strncmp(s + 1, "el ", 3) == 0))
	|| (*s == 'f' && ((s[1] == 't' && s[2] == ' ')
	|| strncmp(s + 1, "ilter ", 6) == 0))
	|| (*s == 'd' && s[1] == 'h' && s[2] == ' ')
	|| (*s == 'b' && s[1] == ' '))
		return 1;

	return 0;
}

/* Replace history expressions ("!*") in the string INPUT by the corresponding
 * history entry. */
static int
expand_history(char **input)
{
	/* history_expansion_char defaults to '!' */
	const char *hist_c = strchr(*input, history_expansion_char);
	if (!hist_c || (hist_c != *input && *(hist_c - 1) != ' ')
	|| exclude_from_history(*input) == 1)
		return FUNC_SUCCESS;

	char *exp_input = NULL;
	const int ret = history_expand(*input, &exp_input);

	if (ret == -1) { /* Error in expansion */
		/* From history_expand: If an error occurred in expansion (-1), then
		 * exp_input contains a descriptive error message. */
		xerror("%s: %s\n", PROGRAM_NAME, exp_input ? exp_input : UNKNOWN_STR);
		free(*input);
		free(exp_input);
		return FUNC_FAILURE;
	}

	if (ret == 0) { /* No expansion took place */
		free(exp_input);
		return FUNC_SUCCESS;
	}

	printf("%s\n", exp_input);
	free(*input);

	if (ret == 2) { /* Display but do not execute the expanded command (:p) */
		free(exp_input);
		return (-1);
	}

	/* (ret == 1) Display and execute */
	*input = exp_input;

	return FUNC_SUCCESS;
}

static char *
handle_empty_line(const int screen_refresh)
{
	if (conf.autols == 1 && ((flags & DELAYED_REFRESH)
	|| xargs.refresh_on_empty_line == 1)
	&& screen_refresh == PROMPT_SCREEN_REFRESH
	&& g_prompt_ignore_empty_line == 0)
		refresh_screen();

	g_prompt_ignore_empty_line = 0;
	flags &= ~DELAYED_REFRESH;
	return NULL;
}

/* Print the prompt and return the string entered by the user, to be
 * parsed later by parse_input_str() */
char *
prompt(const int prompt_flag, const int screen_refresh)
{
	size_t ac_matches = 0;
	initialize_prompt_data(prompt_flag, &ac_matches);
	/* Generate the prompt string using the prompt line in the config
	 * file (stored in encoded_prompt at startup). */
	char *decoded_prompt = decode_prompt(conf.encoded_prompt);
	char *the_prompt = construct_prompt(decoded_prompt
		? decoded_prompt : EMERGENCY_PROMPT, ac_matches);
	free(decoded_prompt);

	if (conf.rprompt_str && *conf.rprompt_str
	&& conf.prompt_is_multiline == 1 && term_caps.suggestions == 1)
		print_right_prompt();

	if (prompt_flag == PROMPT_UPDATE || prompt_flag == PROMPT_UPDATE_RUN_CMDS) {
		rl_set_prompt(the_prompt);
		free(the_prompt);
		return NULL;
	}

	/* Tell my_rl_getc() (readline.c) to recalculate the length
	 * of the last prompt line, needed to calculate the finder's offset
	 * and the current cursor column. This length might vary if the
	 * prompt contains dynamic values. */
	prompt_offset = UNSET;

	UNHIDE_CURSOR;

	/* Print the prompt and get user input */
	char *input = readline(the_prompt);
	free(the_prompt);

	if (!input || !*input || rl_end == 0) {
		free(input);
		return handle_empty_line(screen_refresh);
	}

	g_prompt_ignore_empty_line = 0;
	flags &= ~DELAYED_REFRESH;

	if (expand_history(&input) != FUNC_SUCCESS)
		return NULL;

	log_and_record(input);
	return input;
}

static int
list_prompts(void)
{
	if (prompts_n == 0) {
		puts(_("prompt: No extra prompts found. Using the default prompt."));
		return FUNC_SUCCESS;
	}

	const char *ptr = SET_MISC_PTR;
	for (size_t i = 0; i < prompts_n; i++) {
		if (!prompts[i].name)
			continue;

		if (*cur_prompt_name == *prompts[i].name
		&& strcmp(cur_prompt_name, prompts[i].name) == 0)
			printf("%s%s%s %s\n", mi_c, ptr, df_c, prompts[i].name);
		else
			printf("  %s\n", prompts[i].name);
	}

	return FUNC_SUCCESS;
}

static int
switch_prompt(const size_t n)
{
	free(conf.encoded_prompt);
	free(conf.wprompt_str);
	free(conf.rprompt_str);
	conf.encoded_prompt = conf.wprompt_str = conf.rprompt_str = NULL;

	if (prompts[n].regular)
		conf.encoded_prompt = savestring(prompts[n].regular, strlen(prompts[n].regular));

	if (prompts[n].warning)
		conf.wprompt_str = savestring(prompts[n].warning, strlen(prompts[n].warning));

	if (prompts[n].right) {
		conf.rprompt_str = savestring(prompts[n].right, strlen(prompts[n].right));
		conf.prompt_is_multiline = prompts[n].multiline;
	}

	prompt_notif = prompts[n].notifications;
	set_prompt_options();

	if (xargs.warning_prompt == 0)
		return FUNC_SUCCESS;

	conf.warning_prompt = prompts[n].warning_prompt_enabled;

	update_warning_prompt_text_color();

	return FUNC_SUCCESS;
}

static int
set_prompt(char *name)
{
	if (!name || !*name)
		return FUNC_FAILURE;

	if (prompts_n == 0) {
		xerror("%s\n", _("prompt: No extra prompts defined. Using the "
			"default prompt"));
		return FUNC_FAILURE;
	}

	char *p = unescape_str(name);
	if (!p) {
		xerror(_("prompt: %s: Error unescaping string\n"), name);
		return FUNC_FAILURE;
	}

	for (size_t i = prompts_n; i-- > 0;) {
		if (*p != *prompts[i].name || strcmp(p, prompts[i].name) != 0)
			continue;
		free(p);
		xstrsncpy(cur_prompt_name, prompts[i].name, sizeof(cur_prompt_name));
		return switch_prompt(i);
	}

	xerror(_("prompt: %s: No such prompt\n"), p);
	free(p);
	return FUNC_FAILURE;
}

static int
set_default_prompt(void)
{
	free(conf.encoded_prompt);
	conf.encoded_prompt = savestring(DEFAULT_PROMPT, sizeof(DEFAULT_PROMPT) - 1);
	free(conf.wprompt_str);
	conf.wprompt_str = savestring(DEF_WPROMPT_STR, sizeof(DEF_WPROMPT_STR) - 1);
	*cur_prompt_name = '\0';
	prompt_notif = DEF_PROMPT_NOTIF;
	return FUNC_SUCCESS;
}

/* Read environment variables controlling options for the '\b', '\f', and
 * '\p' prompt escape codes, and set the appropriate values. */
void
set_prompt_options(void)
{
	const char *val = NULL;
	int n = 0;

	const char *np = conf.encoded_prompt; /* Normal/Regular prompt */
	const char *wp = conf.wprompt_str;    /* Warning prompt */
	const char *rp = conf.rprompt_str;    /* Right prompt */

#define CHECK_PROMPT_OPT(s) ((np && *np && strstr(np, (s)) != NULL) \
	|| (wp && *wp && strstr(wp, (s)) != NULL)                       \
	|| (rp && *rp && strstr(rp, (s)) != NULL))

	const int b_is_set = CHECK_PROMPT_OPT("\\b");
	const int f_is_set = CHECK_PROMPT_OPT("\\f");
	const int p_is_set = CHECK_PROMPT_OPT("\\p");

#undef CHECK_PROMPT_OPT

	conf.prompt_b_is_set = b_is_set;

	if (f_is_set == 1) {
		val = getenv("CLIFM_PROMPT_F_DIR_LEN");
		if (val && is_number(val) && (n = xatoi(val)) > 0 && n < INT_MAX)
			conf.prompt_f_dir_len = n;

		val = getenv("CLIFM_PROMPT_F_FULL_LEN_DIRS");
		if (val && is_number(val) && (n = xatoi(val)) > 0 && n < INT_MAX)
			conf.prompt_f_full_len_dirs = n;
	}

	if (b_is_set == 1) {
		val = getenv("CLIFM_PROMPT_B_PRECISION");
		if (val && IS_DIGIT(*val) && !val[1])
			conf.prompt_b_precision = *val - '0';

		val = getenv("CLIFM_PROMPT_B_MIN");
		if (val && is_number(val) && (n = xatoi(val)) < INT_MAX)
			conf.prompt_b_min = n;
	}

	if (conf.prompt_p_max_path == UNSET && p_is_set == 1) {
		val = getenv("CLIFM_PROMPT_P_MAX_PATH");
		if (val && is_number(val) && (n = xatoi(val)) > 0 && n < INT_MAX)
			conf.prompt_p_max_path = n;
	}
}

static int
edit_prompts_file(char *app)
{
	if (xargs.stealth_mode == 1) {
		printf("%s: prompt: %s\n", PROGRAM_NAME, STEALTH_DISABLED);
		return FUNC_SUCCESS;
	}

	if (!prompts_file || !*prompts_file) {
		xerror("%s\n", _("prompt: Prompts file not found"));
		return FUNC_FAILURE;
	}

	struct stat a;
	if (stat(prompts_file, &a) == -1) {
		xerror("prompt: '%s': %s\n", prompts_file, strerror(errno));
		return errno;
	}

	const time_t old_time = a.st_mtime;

	int ret = open_config_file(app, prompts_file);
	if (ret != FUNC_SUCCESS)
		return ret;

	if (stat(prompts_file, &a) == -1) {
		xerror("prompt: '%s': %s\n", prompts_file, strerror(errno));
		return errno;
	}

	if (old_time == a.st_mtime)
		return FUNC_SUCCESS;

	if (conf.autols == 1)
		reload_dirlist();

	ret = load_prompts();
	print_reload_msg(NULL, NULL, _("File modified. Prompts reloaded.\n"));

	if (*cur_prompt_name)
		set_prompt(cur_prompt_name);

	return ret;
}

int
prompt_function(char **args)
{
	if (!args[0] || !*args[0] || (*args[0] == 'l'
	&& strcmp(args[0], "list") == 0))
		return list_prompts();

	if (IS_HELP(args[0])) {
		puts(PROMPT_USAGE);
		return FUNC_SUCCESS;
	}

	if (*args[0] == 'u' && strcmp(args[0], "unset") == 0)
		return set_default_prompt();

	if (*args[0] == 'e' && strcmp(args[0], "edit") == 0)
		return edit_prompts_file(args[1]);

	if (*args[0] == 'r' && strcmp(args[0], "reload") == 0) {
		const int ret = load_prompts();
		if (ret == FUNC_SUCCESS) {
			printf(_("%s: Prompts successfully reloaded\n"), PROGRAM_NAME);
			return FUNC_SUCCESS;
		}
		return ret;
	}

	if (*args[0] == 's' && strcmp(args[0], "set") == 0)
		return set_prompt(args[1]);

	return set_prompt(args[0]);
}
