/* keybinds.c -- functions keybindings configuration */

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

#ifdef __OpenBSD__
typedef char *rl_cpvfunc_t;
# include <ereadline/readline/readline.h>
#else
# include <readline/readline.h>
#endif /* __OpenBSD__ */

#ifdef __TINYC__
/* Silence a tcc warning. We don't use CTRL anyway */
# undef CTRL
#endif /* __TINYC */

#include <termios.h>
#include <unistd.h>

#ifdef __NetBSD__
# include <string.h>
#endif /* __NetBSD__ */

#include <errno.h>

#include "aux.h"
#include "config.h"
#include "exec.h"
#include "keybinds.h"
#include "listing.h"
#include "misc.h"
#include "profiles.h"
#include "prompt.h"
#include "messages.h"
#include "readline.h"
#include "file_operations.h"

#ifndef _NO_SUGGESTIONS
# include "suggestions.h"
#endif /* !_NO_SUGGESTIONS */

#ifndef _NO_HIGHLIGHT
# include "highlight.h"
#endif /* !_NO_HIGHLIGHT */

#include "strings.h" /* quote_str() */

/* Let's use these word delimiters to print first suggested word
 * and to delete last typed word */
#define WORD_DELIMITERS " /.-_=,:;@+*&$#<>%~|({[]})¿?¡!"

#ifndef _NO_SUGGESTIONS
static int accept_first_word = 0;
#endif /* !_NO_SUGGESTIONS */

/* This is just an ugly workaround. You've been warned!
 * Long story short: prompt commands are executed after SOME keybindings, but
 * not after others. I'm not sure why, and this irregularity is what is wrong
 * in the first place.
 * In the second case, when prompt commands are not executed, we sometimes do
 * want to run them (mostly when changing directories): in this case, we set
 * exec_prompt_cmds to 1. */
static int exec_prompt_cmds = 0;

static void
xrl_reset_line_state(void)
{
	UNHIDE_CURSOR;
	rl_reset_line_state();
}

int
kbinds_reset(void)
{
	int exit_status = EXIT_SUCCESS;
	struct stat file_attrib;

	if (stat(kbinds_file, &file_attrib) == -1) {
		exit_status = create_kbinds_file();
	} else {
		char *cmd[] = {"rm", "--", kbinds_file, NULL};
		if (launch_execv(cmd, FOREGROUND, E_NOFLAG) == EXIT_SUCCESS)
			exit_status = create_kbinds_file();
		else
			exit_status = EXIT_FAILURE;
	}

	if (exit_status == EXIT_SUCCESS)
		err('n', PRINT_PROMPT, _("kb: Restart %s for changes "
			"to take effect\n"), PROGRAM_NAME);

	return exit_status;
}

static int
kbinds_edit(char *app)
{
	if (xargs.stealth_mode == 1) {
		printf("%s: kb: %s\n", PROGRAM_NAME, STEALTH_DISABLED);
		return EXIT_SUCCESS;
	}

	if (!kbinds_file)
		return EXIT_FAILURE;

	struct stat attr;
	if (stat(kbinds_file, &attr) == -1) {
		create_kbinds_file();
		stat(kbinds_file, &attr);
	}

	time_t mtime_bfr = (time_t)attr.st_mtime;

	int ret = EXIT_SUCCESS;
	if (app && *app) {
		char *cmd[] = {app, kbinds_file, NULL};
		ret = launch_execv(cmd, FOREGROUND, E_NOFLAG);
	} else {
		open_in_foreground = 1;
		ret = open_file(kbinds_file);
		open_in_foreground = 0;
	}

	if (ret != EXIT_SUCCESS)
		return ret;

	stat(kbinds_file, &attr);
	if (mtime_bfr == (time_t)attr.st_mtime)
		return EXIT_SUCCESS;

	err('n', PRINT_PROMPT, _("kb: Restart %s for changes to "
			"take effect\n"), PROGRAM_NAME);
	return EXIT_SUCCESS;
}

int
kbinds_function(char **args)
{
	if (!args)
		return EXIT_FAILURE;

	if (!args[1] || strcmp(args[1], "list") == 0) {
		if (kbinds_n == 0) {
			printf(_("%s: kb: No keybindings defined\n"), PROGRAM_NAME);
			return EXIT_SUCCESS;
		}
		size_t i;
		for (i = 0; i < kbinds_n; i++)
			printf("%s: %s\n", kbinds[i].key, kbinds[i].function);

		return EXIT_SUCCESS;
	}

	if (IS_HELP(args[1])) {
		puts(_(KB_USAGE));
		return EXIT_SUCCESS;
	}

	if (*args[1] == 'e' && strcmp(args[1], "edit") == 0)
		return kbinds_edit(args[2] ? args[2] : NULL);

	if (*args[1] == 'r' && strcmp(args[1], "reset") == 0)
		return kbinds_reset();

	if (*args[1] == 'r' && strcmp(args[1], "readline") == 0) {
		rl_function_dumper(1);
		return EXIT_SUCCESS;
	}

	fprintf(stderr, "%s\n", _(KB_USAGE));
	return EXIT_FAILURE;
}

/* Store keybinds from the keybinds file into a struct */
int
load_keybinds(void)
{
	if (config_ok == 0 || !kbinds_file)
		return EXIT_FAILURE;

	/* Free the keybinds struct array */
	if (kbinds_n > 0) {
		int i = (int)kbinds_n;

		while (--i >= 0) {
			free(kbinds[i].function);
			free(kbinds[i].key);
		}

		free(kbinds);
		kbinds = (struct kbinds_t *)xnmalloc(1, sizeof(struct kbinds_t));
		kbinds_n = 0;
	}

	/* Open the keybinds file */
	FILE *fp = fopen(kbinds_file, "r");
	if (!fp)
		return EXIT_FAILURE;

	size_t line_size = 0;
	char *line = (char *)NULL;
	ssize_t line_len = 0;

	while ((line_len = getline(&line, &line_size, fp)) > 0) {
		if (!line || !*line || *line == '#' || *line == '\n')
			continue;

		if (line[line_len - 1] == '\n')
			line[line_len - 1] = '\0';

		char *tmp = (char *)NULL;
		tmp = strchr(line, ':');
		if (!tmp || !*(tmp + 1))
			continue;

		/* Now copy left and right value of each keybind into the
		 * keybinds struct */
		kbinds = xrealloc(kbinds, (kbinds_n + 1) * sizeof(struct kbinds_t));
		kbinds[kbinds_n].key = savestring(tmp + 1, strlen(tmp + 1));

		*tmp = '\0';

		kbinds[kbinds_n].function = savestring(line, strlen(line));
		kbinds_n++;
	}

	fclose(fp);
	free(line);
	return EXIT_SUCCESS;
}

/* This call to prompt() just updates the prompt in case it was modified by
 * a keybinding, for example, chdir, files selection, trash, and so on. */
static void
rl_update_prompt(void)
{
	if (rl_line_buffer) {
		memset(rl_line_buffer, '\0', (size_t)rl_end);
		rl_point = rl_end = 0;
	}

	/* Set this flag to prevent prompt() from refreshing the screen. */
	rl_pending_input = 1;
	/* In UPDATE mode, prompt() always returns NULL. */
	prompt(exec_prompt_cmds ? PROMPT_UPDATE_RUN_CMDS : PROMPT_UPDATE);
	exec_prompt_cmds = 0;
	UNHIDE_CURSOR;
}

/* Old version of rl_update_prompt(). Used only by rl_profile_previous() and
 * rl_profile_next(): if any of these functions use rl_update_prompt() instead,
 * no prompt is printed (not sure why). FIX. */
static void
rl_update_prompt_old(void)
{
	HIDE_CURSOR;
	int b = xargs.refresh_on_empty_line;
	xargs.refresh_on_empty_line = 0;
	char *input = prompt(PROMPT_SHOW);
	free(input);
	xargs.refresh_on_empty_line = b;
}

/* Runs any command recognized by CliFM via a keybind. Example:
 * keybind_exec_cmd("sel *") */
int
keybind_exec_cmd(char *str)
{
	size_t old_args = args_n;
	args_n = 0;

#ifndef _NO_SUGGESTIONS
	if (suggestion.printed == 1 && suggestion_buf)
		clear_suggestion(CS_FREEBUF);
#endif /* !_NO_SUGGESTIONS */

	int exit_status = EXIT_FAILURE;
	char **cmd = parse_input_str(str);
	putchar('\n');

	if (cmd) {
		exit_status = exec_cmd(cmd);

		/* While in the bookmarks or mountpoints screen, the kbind_busy
		 * flag will be set to 1 and no keybinding will work. Once the
		 * corresponding function exited, set the kbind_busy flag to zero,
		 * so that keybindings work again. */
		if (kbind_busy == 1)
			kbind_busy = 0;

		int i = (int)args_n + 1;
		while (--i >= 0)
			free(cmd[i]);
		free(cmd);

#ifdef __HAIKU__
		rl_update_prompt_old();
#else
		rl_update_prompt();
#endif /* __HAIKU__ */
	}

	args_n = old_args;
	return exit_status;
}

static int
run_kb_cmd(char *cmd)
{
	if (!cmd || !*cmd)
		return EXIT_FAILURE;

	if (kbind_busy == 1)
		return EXIT_SUCCESS;

	if (conf.colorize == 1 && wrong_cmd == 1)
		recover_from_wrong_cmd();

	int exit_code_bk = exit_code;

	keybind_exec_cmd(cmd);
	rl_reset_line_state();

	if (exit_code != exit_code_bk)
		/* The exit code was changed by the executed command. Force the
		 * input taking function (my_rl_getc) to update the value of
		 * prompt_offset to correctly calculate the cursor position. */
		prompt_offset = UNSET;

	return EXIT_SUCCESS;
}

/* Retrieve the key sequence associated to FUNCTION */
static char *
find_key(char *function)
{
	if (!kbinds_n)
		return (char *)NULL;

	int n = (int)kbinds_n;
	while (--n >= 0) {
		if (*function != *kbinds[n].function)
			continue;
		if (strcmp(function, kbinds[n].function) == 0)
			return kbinds[n].key;
	}

	return (char *)NULL;
}

int
rl_toggle_max_filename_len(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1)
		return EXIT_SUCCESS;

	static int mnl_bk = 0, flag = 0;

	if (flag == 0 || conf.trim_names == 0) {
		mnl_bk = conf.max_name_len_bk;
		flag = 1;
	}

	if (conf.max_name_len == UNSET) {
		conf.max_name_len = mnl_bk;
		mnl_bk = UNSET;
	} else {
		mnl_bk = conf.max_name_len;
		conf.max_name_len = UNSET;
	}

	if (conf.autols == 1) {
		if (conf.clear_screen == 0)
			putchar('\n');
		reload_dirlist();
	}

	if (conf.max_name_len == UNSET)
		print_reload_msg(_("Max name length unset\n"));
	else
		print_reload_msg(_("Max name length set to %d\n"), conf.max_name_len);

	xrl_reset_line_state();
	return EXIT_SUCCESS;
}

/* Prepend authentication program name (usually sudo or doas) to the current
 * input string. */
static int
rl_prepend_sudo(int count, int key)
{
	UNUSED(count); UNUSED(key);
#ifndef _NO_SUGGESTIONS
	if (suggestion.printed && suggestion_buf) {
		clear_suggestion(CS_FREEBUF);
		fputs(df_c, stdout);
	}
#endif /* !_NO_SUGGESTIONS */

	int free_s = 1;
	size_t len = 0;
	char *t = sudo_cmd;
	char *s = (char *)NULL;

	if (t) {
		len = strlen(t);
		if (len > 0 && t[len - 1] != ' ') {
			s = (char *)xnmalloc(len + 2, sizeof(char));
			snprintf(s, len + 2, "%s ", t);
			len++;
		} else {
			s = t;
			free_s = 0;
		}
	} else {
		len = strlen(DEF_SUDO_CMD) + 1;
		s = (char *)xnmalloc(len + 1, sizeof(char));
		snprintf(s, len + 1, "%s ", DEF_SUDO_CMD);
	}

	char *c = (char *)NULL;
	if (conf.highlight == 1 && conf.colorize == 1
	&& cur_color && cur_color != tx_c) {
		c = cur_color;
		fputs(tx_c, stdout);
	}

	int p = rl_point;
	if (*rl_line_buffer == *s
	&& strncmp(rl_line_buffer, s, len) == 0) {
		rl_delete_text(0, (int)len);
		rl_point = p - (int)len;
	} else {
		rl_point = 0;
		rl_insert_text(s);
		rl_point = p + (int)len;
		if (c)
			rl_redisplay();
	}

	if (c)
		fputs(c, stdout);

	if (free_s)
		free(s);

#ifndef _NO_SUGGESTIONS
	if (suggestion.offset == 0 && suggestion_buf) {
		int r = rl_point;
		rl_point = rl_end;
		clear_suggestion(CS_FREEBUF);
		rl_point = r;
	}
#endif /* !_NO_SUGGESTIONS */

#ifndef _NO_HIGHLIGHT
	if (conf.highlight == 1) {
		int r = rl_point;
		rl_point = 0;
		recolorize_line();
		rl_point = r;
	}
#endif /* !_NO_HIGHLIGHT */

	return EXIT_SUCCESS;
}

static int
rl_create_file(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_kb_cmd("n");
}

#ifndef _NO_SUGGESTIONS
/* Insert the accepted suggestion into the current input line
 * (highlighting words and special chars if syntax highlighting is enabled) */
static void
my_insert_text(char *text, char *s, const char _s)
{
#ifdef _NO_HIGHLIGHT
	UNUSED(s); UNUSED(_s);
#endif /* !_NO_HIGHLIGHT */

	if (!text || !*text)
		return;
	{
	if (wrong_cmd == 1 || cur_color == hq_c)
		goto INSERT_TEXT;

#ifndef _NO_HIGHLIGHT
	if (conf.highlight == 1) {
		/* Hide the cursor to minimize flickering */
		HIDE_CURSOR;
		/* Set text color to default */
		fputs(tx_c, stdout);
		cur_color = tx_c;
		char *t = text;
		size_t i;

		/* We only need to redisplay first suggested word if it contains
		 * a highlighting char and it is not preceded by a space */
		int redisplay = 0;
		if (accept_first_word == 1) {
			for (i = 0; t[i]; i++) {
				if (t[i] >= '0' && t[i] <= '9') {
					if (i == 0 || t[i - 1] != ' ') {
						redisplay = 1;
						break;
					}
				}
				switch (t[i]) {
				case '/': /* fallthrough */
				case '"': /* fallthrough */
				case '\'': /* fallthrough */
				case '&': /* fallthrough */
				case '|': /* fallthrough */
				case ';': /* fallthrough */
				case '>': /* fallthrough */
				case '(': /* fallthrough */
				case '[': /* fallthrough */
				case '{': /* fallthrough */
				case ')': /* fallthrough */
				case ']': /* fallthrough */
				case '}': /* fallthrough */
				case '$': /* fallthrough */
				case '-': /* fallthrough */
				case '~': /* fallthrough */
				case '*': /* fallthrough */
				case '#':
					if (i == 0 || t[i - 1] != ' ')
						redisplay = 1;
					break;
				default: break;
				}
				if (redisplay == 1)
					break;
			}
		}

		char q[PATH_MAX];
		int l = 0;
		for (i = 0; t[i]; i++) {
			rl_highlight(t, i, SET_COLOR);
			if ((signed char)t[i] < 0) {
				q[l] = t[i];
				l++;
				if ((signed char)t[i + 1] >= 0) {
					q[l] = '\0';
					l = 0;
					rl_insert_text(q);
//					rl_redisplay();
				}
				continue;
			}
			q[0] = t[i];
			q[1] = '\0';
			rl_insert_text(q);
			if (accept_first_word == 0 || redisplay == 1)
				rl_redisplay();
		}

		if (s && redisplay == 1) {
			/* 1) rl_redisplay removes the suggestion from the current line
			 * 2) We need rl_redisplay to correctly print highlighting colors
			 * 3) We need to keep the suggestion when accepting only
			 * the first suggested word.
			 * In other words, if we correctly print colors, we lose the
			 * suggestion.
			 * As a workaround, let's reprint the suggestion */
			size_t slen = strlen(suggestion_buf);
			*s = _s ? _s : ' ';
//			print_suggestion(suggestion_buf, slen + 1, suggestion.color);
			print_suggestion(suggestion_buf, slen, suggestion.color);
			*s = '\0';
		}

		UNHIDE_CURSOR;
	} else
#endif /* !_NO_HIGHLIGHT */
INSERT_TEXT:
	{
		rl_insert_text(text);
	}
	}
}

static int
rl_accept_suggestion(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1) {
		/* If not at the end of the typed string, just move the cursor
		 * forward one column */
		if (rl_point < rl_end)
			rl_point++;
		return EXIT_SUCCESS;
	}

	if (!wrong_cmd && cur_color != hq_c) {
		cur_color = tx_c;
		fputs(tx_c, stdout);
	}

	/* Only accept the current suggestion if the cursor is at the end
	 * of the line typed so far */
	if (!conf.suggestions || rl_point != rl_end || !suggestion_buf
	|| suggestion.type == CMD_DESC_SUG) {
		if (rl_point < rl_end) {
			/* Just move the cursor forward one character */
			int mlen = mblen(rl_line_buffer + rl_point, MB_CUR_MAX);
			rl_point += mlen;
		}
		return EXIT_SUCCESS;
	}

	/* If accepting the first suggested word, accept only up to next
	 * word delimiter. */
	char *s = (char *)NULL, _s = 0;
	int trimmed = 0, accept_first_word_last = 0;
	if (accept_first_word == 1) {
		char *p = suggestion_buf + (rl_point - suggestion.offset);
		/* Skip leading spaces */
		while (*p == ' ')
			p++;

		/* Skip all consecutive word delimiters from the beginning of the
		 * suggestion (P), except for slash and space */
		while ((s = strpbrk(p, WORD_DELIMITERS)) == p && *s != '/' && *s != ' ')
			p++;
		if (s && s != p && *(s - 1) == ' ')
			s = strpbrk(p, WORD_DELIMITERS);

		if (s && *(s + 1)) { /* Trim suggestion after word delimiter */
			if (*s == '/')
				++s;
			_s = *s;
			*s = '\0';
			trimmed = 1;
		} else { /* Last word: No word delimiter */
			size_t len = strlen(suggestion_buf);
			if (len > 0 && suggestion_buf[len - 1] != '/'
			&& suggestion_buf[len - 1] != ' ')
				suggestion.type = NO_SUG;
			accept_first_word_last = 1;
			s = (char *)NULL;
		}
	}

	int bypass_alias = 0;
	if (rl_line_buffer && *rl_line_buffer == '\\' && *(rl_line_buffer + 1))
		bypass_alias = 1;

	rl_delete_text(suggestion.offset, rl_end);
	rl_point = suggestion.offset;

	if (conf.highlight == 1 && accept_first_word == 0 && cur_color != hq_c) {
		cur_color = tx_c;
		rl_redisplay();
	}

	if (accept_first_word_last == 1)
		accept_first_word = 0;

	if (accept_first_word == 0 && (flags & BAEJ_SUGGESTION))
		clear_suggestion(CS_KEEPBUF);

	/* Complete according to the suggestion type */
	switch (suggestion.type) {
	case BACKDIR_SUG:    /* fallthrough */
	case JCMD_SUG:       /* fallthrough */
	case BOOKMARK_SUG:   /* fallthrough */
	case COMP_SUG:       /* fallthrough */
	case ELN_SUG:        /* fallthrough */
	case FASTBACK_SUG:   /* fallthrough */
	case FUZZY_FILENAME: /* fallthrough */
	case FILE_SUG: {
		char *tmp = (char *)NULL;
		size_t i, isquote = 0, backslash = 0;
		for (i = 0; suggestion_buf[i]; i++) {
			if (is_quote_char(suggestion_buf[i]))
				isquote = 1;

			if (suggestion_buf[i] == '\\') {
				backslash = 1;
				break;
			}
		}

		if (isquote == 1 && backslash == 0) {
			if (suggestion.type == ELN_SUG && suggestion.filetype == DT_REG
			&& conf.quoting_style != QUOTING_STYLE_BACKSLASH)
				tmp = quote_str(suggestion_buf);
			else
				tmp = escape_str(suggestion_buf);
		}

		my_insert_text(tmp ? tmp : suggestion_buf, NULL, 0);
		free(tmp);

		if (suggestion.type == FASTBACK_SUG) {
			if (conf.highlight == 0) {
				rl_insert_text("/");
			} else if (*suggestion_buf != '/' || suggestion_buf[1]) {
				fputs(hd_c, stdout);
				rl_insert_text("/");
				rl_redisplay();
				fputs(df_c, stdout);
			}
		} else if (suggestion.filetype != DT_DIR
		&& suggestion.type != BOOKMARK_SUG && suggestion.type != BACKDIR_SUG) {
			rl_stuff_char(' ');
		}
		suggestion.type = NO_SUG;
		}
		break;

	case FIRST_WORD:
		my_insert_text(suggestion_buf, s, _s); break;

	case JCMD_SUG_NOACD:
		rl_insert_text("cd ");
		rl_redisplay();
		my_insert_text(suggestion_buf, NULL, 0);
		break;

	case SEL_SUG:      /* fallthrough */
	case HIST_SUG:     /* fallthrough */
	case BM_NAME_SUG:  /* fallthrough */
	case PROMPT_SUG:   /* fallthrough */
	case NET_SUG:      /* fallthrough */
	case CSCHEME_SUG:  /* fallthrough */
	case WS_NAME_SUG:  /* fallthrough */
	case INT_HELP_SUG: /* fallthrough */
	case PROFILE_SUG:  /* fallthrough */
	case DIRHIST_SUG:
		my_insert_text(suggestion_buf, NULL, 0); break;

#ifndef _NO_TAGS
	case TAGT_SUG: /* fallthrough */
	case TAGC_SUG: /* fallthrough */
	case TAGS_SUG: /* fallthrough */
	case BM_PREFIX_SUG: {
		char prefix[3];
		if (suggestion.type == TAGC_SUG) {
			prefix[0] = ':'; prefix[1] = '\0';
		} else if (suggestion.type == TAGT_SUG) {
			prefix[0] = 't'; prefix[1] = ':'; prefix[2] = '\0';
		} else if (suggestion.type == BM_PREFIX_SUG) {
			prefix[0] = 'b'; prefix[1] = ':'; prefix[2] = '\0';
		}

		rl_insert_text(prefix);
		char *p = suggestion.type != BM_PREFIX_SUG
			? escape_str(suggestion_buf) : (char *)NULL;

		my_insert_text(p ? p : suggestion_buf, NULL, 0);

		if (suggestion.type != BM_PREFIX_SUG
		&& (fzftab != 1 || (suggestion.type != TAGT_SUG)))
			rl_stuff_char(' ');
		free(p);
		}
		break;
#endif /* _NO_TAGS */

	case WS_NUM_SUG: /* fallthrough */
	case USER_SUG: {
		char *p = escape_str(suggestion_buf);
		my_insert_text(p ? p : suggestion_buf, NULL, 0);
		if (suggestion.type == USER_SUG)
			rl_stuff_char('/');
		free(p);
		break;
	}

	default:
		if (bypass_alias == 1)
			rl_insert_text("\\");
		my_insert_text(suggestion_buf, NULL, 0);
		rl_stuff_char(' ');
		break;
	}

	/* Move the cursor to the end of the line */
	rl_point = rl_end;
	if (accept_first_word == 0) {
		suggestion.printed = 0;
		free(suggestion_buf);
		suggestion_buf = (char *)NULL;
	} else {
		if (s) {
			/* Reinsert the char we removed to print only the first word */
			if (trimmed == 1)
				*s = _s;
/*			if (slash)
				*s = _s;
			else
				*s = ' '; */
		}
		accept_first_word = 0;
	}

	flags &= ~BAEJ_SUGGESTION;
	return EXIT_SUCCESS;
}

static int
rl_accept_first_word(int count, int key)
{
	if (rl_point < rl_end)
		return rl_forward_word(1, 0);

	/* Accepting the first suggested word is not supported for ELN's,
	 * bookmark and alias names */
	int t = suggestion.type;
	if (t != ELN_SUG && t != BOOKMARK_SUG && t != ALIAS_SUG && t != JCMD_SUG
	&& t != JCMD_SUG_NOACD && t != FUZZY_FILENAME && t != CMD_DESC_SUG
	&& t != BM_NAME_SUG && t != INT_HELP_SUG && t != TAGT_SUG
	&& t != BM_PREFIX_SUG) {
		accept_first_word = 1;
		suggestion.type = FIRST_WORD;
	}
	return rl_accept_suggestion(count, key);
}
#endif /* !_NO_SUGGESTIONS */

static int
rl_refresh(int count, int key)
{
	UNUSED(count); UNUSED(key);
	exec_prompt_cmds = 1;
	return run_kb_cmd("rf");
}

static int
rl_dir_parent(int count, int key)
{
	UNUSED(count); UNUSED(key);
	/* If already root dir, do nothing */
	if (*workspaces[cur_ws].path == '/' && !workspaces[cur_ws].path[1])
		return EXIT_SUCCESS;

	exec_prompt_cmds = 1;
	return run_kb_cmd("cd ..");
}

static int
rl_dir_root(int count, int key)
{
	UNUSED(count); UNUSED(key);
	/* If already root dir, do nothing */
	if (*workspaces[cur_ws].path == '/' && !workspaces[cur_ws].path[1])
		return EXIT_SUCCESS;

	return run_kb_cmd("cd /");
}

static int
rl_dir_home(int count, int key)
{
	UNUSED(count); UNUSED(key);
	/* If already in home, do nothing */
	if (*workspaces[cur_ws].path == *user.home
	&& strcmp(workspaces[cur_ws].path, user.home) == 0)
		return EXIT_SUCCESS;

	exec_prompt_cmds = 1;
	return run_kb_cmd("cd");
}

static int
rl_dir_previous(int count, int key)
{
	UNUSED(count); UNUSED(key);
	/* If already at the beginning of dir hist, do nothing */
	if (dirhist_cur_index == 0)
		return EXIT_SUCCESS;

	exec_prompt_cmds = 1;
	return run_kb_cmd("b");
}

static int
rl_dir_next(int count, int key)
{
	UNUSED(count); UNUSED(key);

	/* If already at the end of dir hist, do nothing */
	if (dirhist_cur_index + 1 == dirhist_total_index)
		return EXIT_SUCCESS;

	exec_prompt_cmds = 1;
	return run_kb_cmd("f");
}

int
rl_toggle_long_view(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1 || xargs.disk_usage_analyzer == 1)
		return EXIT_SUCCESS;

#ifndef _NO_SUGGESTIONS
	if (suggestion.printed && suggestion_buf)
		free_suggestion();
#endif /* !_NO_SUGGESTIONS */

	conf.long_view = conf.long_view == 1 ? 0 : 1;

	if (conf.autols == 1) {
		if (conf.clear_screen == 0)
		/* Without this putchar(), the first entries of the directories
		 * list are printed in the prompt line. */
			putchar('\n');
		reload_dirlist();
	}

	print_reload_msg(_("Long view %s\n"),
		conf.long_view == 1 ? _("enabled") : _("disabled"));
	xrl_reset_line_state();
	return EXIT_SUCCESS;
}

int
rl_toggle_dirs_first(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1)
		return EXIT_SUCCESS;

#ifndef _NO_SUGGESTIONS
	if (suggestion.printed && suggestion_buf)
		free_suggestion();
#endif /* !_NO_SUGGESTIONS */

	conf.list_dirs_first = conf.list_dirs_first ? 0 : 1;

	if (conf.autols == 1) {
		if (conf.clear_screen == 0)
			putchar('\n');
		reload_dirlist();
	}

	print_reload_msg(_("Directories first %s\n"),
		conf.list_dirs_first ? _("enabled") : _("disabled"));
	xrl_reset_line_state();
	return EXIT_SUCCESS;
}

int
rl_toggle_light_mode(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1)
		return EXIT_SUCCESS;

#ifndef _NO_SUGGESTIONS
	if (suggestion.printed && suggestion_buf)
		free_suggestion();
#endif /* !_NO_SUGGESTIONS */

	conf.light_mode = conf.light_mode == 1 ? 0 : 1;

	if (conf.autols == 1) {
		if (conf.clear_screen == 0)
			putchar('\n');
		reload_dirlist();
	}

	print_reload_msg(_("Light mode %s\n"),
		conf.light_mode == 1 ? _("enabled") : _("disabled"));
	xrl_reset_line_state();

	/* RL_DISPATCHING is zero when called from lightmode_function(),
	 * in exec.c. Otherwise, it is called from a keybinding and
	 * rl_update_prompt() must be executed. */
	if (rl_dispatching == 1)
		rl_update_prompt();

	return EXIT_SUCCESS;
}

int
rl_toggle_hidden_files(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1)
		return EXIT_SUCCESS;

#ifndef _NO_SUGGESTIONS
	if (suggestion.printed && suggestion_buf)
		free_suggestion();
#endif /* !_NO_SUGGESTIONS */

	conf.show_hidden = conf.show_hidden == 1 ? 0 : 1;

	if (conf.autols == 1) {
		if (conf.clear_screen == 0)
			putchar('\n');
		reload_dirlist();
	}

	print_reload_msg(_("Hidden files %s\n"),
		conf.show_hidden == 1 ? "enabled" : "disabled");
	xrl_reset_line_state();
	return EXIT_SUCCESS;
}

static int
rl_open_config(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_kb_cmd("config");
}

static int
rl_open_keybinds(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_kb_cmd("kb edit");
}

static int
rl_open_cscheme(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_kb_cmd("cs edit");
}

static int
rl_open_bm_file(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_kb_cmd("bm edit");
}

static int
rl_open_jump_db(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_kb_cmd("je");
}

static int
rl_open_preview(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (!config_dir || kbind_busy == 1)
		return EXIT_FAILURE;

	char *file = (char *)xnmalloc(config_dir_len + 15, sizeof(char));
	snprintf(file, config_dir_len + 15, "%s/preview.clifm", config_dir);

	int ret = open_file(file);
	free(file);
	rl_on_new_line();
	return ret;
}

static int
rl_open_mime(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_kb_cmd("mm edit");
}

static int
rl_mountpoints(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_kb_cmd("mp");
}

static int
rl_select_all(int count, int key)
{
	UNUSED(count); UNUSED(key);
	exec_prompt_cmds = 1;
	return run_kb_cmd("s ^");
}

static int
rl_deselect_all(int count, int key)
{
	UNUSED(count); UNUSED(key);
	exec_prompt_cmds = 1;
	return run_kb_cmd("ds *");
}

static int
rl_bookmarks(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_kb_cmd("bm");
}

static int
rl_selbox(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_kb_cmd("ds");
}

static int
rl_clear_line(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1 && xrename == 0)
		return EXIT_SUCCESS;

	words_num = 0;

#ifndef _NO_HIGHLIGHT
	if (cur_color != tx_c) {
		cur_color = tx_c;
		fputs(cur_color, stdout);
	}
#endif /* !_NO_HIGHLIGHT */

#ifndef _NO_SUGGESTIONS
	if (wrong_cmd) {
		if (!recover_from_wrong_cmd())
			rl_point = 0;
	}
	if (suggestion.nlines > term_lines) {
		rl_on_new_line();
		return EXIT_SUCCESS;
	}

	if (suggestion_buf) {
		clear_suggestion(CS_FREEBUF);
		suggestion.printed = 0;
		suggestion.nlines = 0;
	}
#endif /* !_NO_SUGGESTIONS */
	curhistindex = current_hist_n;
	rl_kill_text(0, rl_end);
	rl_point = rl_end = 0;
	return EXIT_SUCCESS;
}

static int
rl_sort_next(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1)
		return EXIT_SUCCESS;

#ifndef _NO_SUGGESTIONS
	if (suggestion.printed && suggestion_buf)
		free_suggestion();
#endif /* !_NO_SUGGESTIONS */
	conf.sort++;
	if (conf.sort > SORT_TYPES)
		conf.sort = 0;

	if (conf.autols == 1) {
		sort_switch = 1;
		if (conf.clear_screen == 0)
			putchar('\n');
		reload_dirlist();
		sort_switch = 0;
	}

	xrl_reset_line_state();
	return EXIT_SUCCESS;
}

static int
rl_sort_previous(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1)
		return EXIT_SUCCESS;

#ifndef _NO_SUGGESTIONS
	if (suggestion.printed && suggestion_buf)
		free_suggestion();
#endif /* !_NO_SUGGESTIONS */
	conf.sort--;
	if (conf.sort < 0)
		conf.sort = SORT_TYPES;

	if (conf.autols == 1) {
		sort_switch = 1;
		if (conf.clear_screen == 0)
			putchar('\n');
		reload_dirlist();
		sort_switch = 0;
	}

	xrl_reset_line_state();
	return EXIT_SUCCESS;
}

static int
rl_lock(int count, int key)
{
	UNUSED(count); UNUSED(key);
	int ret = EXIT_SUCCESS;
#ifndef _NO_SUGGESTIONS
	if (suggestion.printed && suggestion_buf)
		free_suggestion();
#endif /* !_NO_SUGGESTIONS */
	rl_deprep_terminal();

#if defined(__APPLE__)
	char *cmd[] = {"bashlock", NULL}; /* See https://github.com/acornejo/bashlock */
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) \
|| defined(__DragonFly__)
	char *cmd[] = {"lock", "-p", NULL};
#elif defined(__HAIKU__)
	char *cmd[] = {"peaclock", NULL};
#else
	char *cmd[] = {"vlock", NULL};
#endif /* __APPLE__ */
	ret = launch_execv(cmd, FOREGROUND, E_NOFLAG);

	rl_prep_terminal(0);
	xrl_reset_line_state();

	if (ret != EXIT_SUCCESS)
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
}

static int
handle_no_sel(const char *func)
{
	if (conf.colorize == 1 && wrong_cmd == 1)
		recover_from_wrong_cmd();

#ifndef _NO_SUGGESTIONS
	if (suggestion.printed == 1 && suggestion_buf)
		clear_suggestion(CS_FREEBUF);
#endif /* !_NO_SUGGESTIONS */

	if (rl_end > 0) {
		rl_delete_text(0, rl_end);
		rl_point = rl_end = 0;
	}

	printf(_("\n%s: sel: %s\n"), func, strerror(ENOENT));
	rl_reset_line_state();

	return EXIT_SUCCESS;
}

static int
rl_archive_sel(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1)
		return EXIT_SUCCESS;

	if (sel_n == 0)
		return handle_no_sel("ac");

	return run_kb_cmd("ac sel");
}

static int
rl_remove_sel(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1)
		return EXIT_SUCCESS;

	if (sel_n == 0)
		return handle_no_sel("r");

	rl_deprep_terminal();
	char cmd[] = "r sel";
	keybind_exec_cmd(cmd);
	rl_prep_terminal(0);
	rl_reset_line_state();
	return EXIT_SUCCESS;
}

static int
rl_export_sel(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1)
		return EXIT_SUCCESS;

	if (sel_n == 0)
		return handle_no_sel("exp");

	return run_kb_cmd("exp sel");
}

static int
rl_move_sel(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1)
		return EXIT_SUCCESS;

	if (sel_n == 0)
		return handle_no_sel("m");

	exec_prompt_cmds = 1;
	return run_kb_cmd("m sel");
}

static int
rl_rename_sel(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1)
		return EXIT_SUCCESS;

	if (sel_n == 0)
		return handle_no_sel("br");

	exec_prompt_cmds = 1;
	return run_kb_cmd("br sel");
}

static int
rl_paste_sel(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1)
		return EXIT_SUCCESS;

	if (sel_n == 0)
		return handle_no_sel("c");

	exec_prompt_cmds = 1;
	rl_deprep_terminal();
	char cmd[] = "c sel";
	keybind_exec_cmd(cmd);
	rl_prep_terminal(0);
	rl_reset_line_state();
	return EXIT_SUCCESS;
}

int
rl_quit(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1)
		return EXIT_SUCCESS;

	conf.cd_on_quit = 0;
	puts("\n");

	/* Reset terminal attributes before exiting. Without this line, the program
	 * quits, but terminal input is not printed to STDOUT. */
	tcsetattr(STDIN_FILENO, TCSANOW, &shell_tmodes);
	exit(EXIT_SUCCESS);
}

#ifndef _NO_PROFILES
/* Get current profile and total amount of profiles and store this info
 * in pointers CUR and TOTAL. */
static void
get_cur_prof(int *cur, int *total)
{
	int i;
	for (i = 0; profile_names[i]; i++) {
		(*total)++;

		if (!alt_profile) {
			if (*profile_names[i] == 'd'
			&& strcmp(profile_names[i], "default") == 0)
				*cur = i;
		} else {
			if (*alt_profile == *profile_names[i]
			&& strcmp(alt_profile, profile_names[i]) == 0)
				*cur = i;
		}
	}
}

static int
rl_profile_previous(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1)
		return EXIT_SUCCESS;

#ifndef _NO_SUGGESTIONS
	if (suggestion.printed && suggestion_buf)
		free_suggestion();
#endif /* !_NO_SUGGESTIONS */

	int prev_prof, cur_prof = -1, total_profs = 0;
	get_cur_prof(&cur_prof, &total_profs);

	if (cur_prof == -1 || !profile_names[cur_prof]
	|| total_profs <= 1)
		return EXIT_FAILURE;

	prev_prof = cur_prof - 1;
	total_profs--;

	if (prev_prof < 0 || !profile_names[prev_prof])
		prev_prof = total_profs;

	if (conf.clear_screen) {
		CLEAR;
	} else {
		putchar('\n');
	}

	if (profile_set(profile_names[prev_prof]) == EXIT_SUCCESS)
		rl_update_prompt_old();

	return EXIT_SUCCESS;
}

static int
rl_profile_next(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1)
		return EXIT_SUCCESS;

#ifndef _NO_SUGGESTIONS
	if (suggestion.printed && suggestion_buf)
		free_suggestion();
#endif /* !_NO_SUGGESTIONS */

	int next_prof, cur_prof = -1, total_profs = 0;
	get_cur_prof(&cur_prof, &total_profs);

	if (cur_prof == -1 || !profile_names[cur_prof])
		return EXIT_FAILURE;

	next_prof = cur_prof + 1;
	total_profs--;

	if (next_prof > (int)total_profs || !profile_names[next_prof]
	|| total_profs <= 1)
		next_prof = 0;

	if (conf.clear_screen) {
		CLEAR;
	} else {
		putchar('\n');
	}

	if (profile_set(profile_names[next_prof]) == EXIT_SUCCESS)
		rl_update_prompt_old();

	return EXIT_SUCCESS;
}
#endif /* _NO_PROFILES */

static int
rl_dirhist(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_kb_cmd("bh");
}

static int
rl_new_instance(int count, int key)
{
	UNUSED(count); UNUSED(key);
	exec_prompt_cmds = 1;
	return run_kb_cmd("x .");
}

static int
rl_clear_msgs(int count, int key)
{
	UNUSED(count); UNUSED(key);
	exec_prompt_cmds = 1;
	return run_kb_cmd("msg clear");
}

static int
rl_trash_sel(int count, int key)
{
	UNUSED(count); UNUSED(key);
	exec_prompt_cmds = 1;
	return run_kb_cmd("t sel");
}

static int
rl_untrash_all(int count, int key)
{
	UNUSED(count); UNUSED(key);
	exec_prompt_cmds = 1;
	return run_kb_cmd("u *");
}

static int
rl_open_sel(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1)
		return EXIT_SUCCESS;

	char cmd[PATH_MAX + 3];
	snprintf(cmd, sizeof(cmd), "o %s", (sel_n && sel_elements[sel_n - 1].name)
		? sel_elements[sel_n - 1].name : "sel");

	return run_kb_cmd(cmd);
}

static int
run_man_cmd(char *str)
{
	char *mp = (char *)NULL;
	char *p = getenv("MANPAGER");
	if (p) {
		size_t len = strlen(p);
		mp = (char *)xnmalloc(len + 1, sizeof(char *));
		xstrsncpy(mp, p, len + 1);
		unsetenv("MANPAGER");
	}

	int ret = launch_execl(str) != EXIT_SUCCESS;

	if (mp) {
		setenv("MANPAGER", mp, 1);
		free(mp);
	}

	return ret;
}

static int
rl_kbinds_help(int count, int key)
{
	if (kbind_busy == 1)
		return EXIT_SUCCESS;

	UNUSED(count); UNUSED(key);
#ifndef _NO_SUGGESTIONS
	if (suggestion.printed && suggestion_buf)
		free_suggestion();
#endif /* !_NO_SUGGESTIONS */

	char cmd[PATH_MAX];
	snprintf(cmd, sizeof(cmd),
		"export PAGER=\"less -p ^[0-9]+\\.[[:space:]]KEYBOARD[[:space:]]SHORTCUTS\"; man %s\n",
		PROGRAM_NAME);
	int ret = run_man_cmd(cmd);
	if (!ret)
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
}

static int
rl_cmds_help(int count, int key)
{
	if (kbind_busy == 1)
		return EXIT_SUCCESS;

	UNUSED(count); UNUSED(key);
#ifndef _NO_SUGGESTIONS
	if (suggestion.printed && suggestion_buf)
		free_suggestion();
#endif /* !_NO_SUGGESTIONS */

	char cmd[PATH_MAX];
	snprintf(cmd, sizeof(cmd),
		"export PAGER=\"less -p ^[0-9]+\\.[[:space:]]COMMANDS\"; man %s\n",
		PROGRAM_NAME);
	int ret = run_man_cmd(cmd);
	if (!ret)
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
}

static int
rl_manpage(int count, int key)
{
	if (kbind_busy == 1)
		return EXIT_SUCCESS;

	UNUSED(count); UNUSED(key);
#ifndef _NO_SUGGESTIONS
	if (suggestion.printed && suggestion_buf)
		free_suggestion();
#endif /* !_NO_SUGGESTIONS */
	char *cmd[] = {"man", PROGRAM_NAME, NULL};
	if (launch_execv(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
}

static int
rl_dir_pinned(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (!pinned_dir) {
		printf(_("\n%s: No pinned file\n"), PROGRAM_NAME);
		rl_reset_line_state();
		return EXIT_SUCCESS;
	}

	return run_kb_cmd(",");
}

static int
rl_ws1(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (rl_line_buffer && *rl_line_buffer)
		rl_delete_text(0, rl_end);

	exec_prompt_cmds = 1;
	if (cur_ws == 0) {
		/* If the user attempts to switch to the same workspace they're
		 * currently in, switch rather to the previous workspace. */
		if (prev_ws != cur_ws) {
			char t[16];
			snprintf(t, sizeof(t), "ws %d", prev_ws + 1);
			return run_kb_cmd(t);
		}
		return EXIT_SUCCESS;
	}

	return run_kb_cmd("ws 1");
}

static int
rl_ws2(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (rl_line_buffer && *rl_line_buffer)
		rl_delete_text(0, rl_end);

	exec_prompt_cmds = 1;
	if (cur_ws == 1) {
		if (prev_ws != cur_ws) {
			char t[16];
			snprintf(t, sizeof(t), "ws %d", prev_ws + 1);
			return run_kb_cmd(t);
		}
		return EXIT_SUCCESS;
	}

	return run_kb_cmd("ws 2");
}

static int
rl_ws3(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (rl_line_buffer && *rl_line_buffer)
		rl_delete_text(0, rl_end);

	exec_prompt_cmds = 1;
	if (cur_ws == 2) {
		if (prev_ws != cur_ws) {
			char t[16];
			snprintf(t, sizeof(t), "ws %d", prev_ws + 1);
			return run_kb_cmd(t);
		}
		return EXIT_SUCCESS;
	}

	return run_kb_cmd("ws 3");
}

static int
rl_ws4(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (rl_line_buffer && *rl_line_buffer)
		rl_delete_text(0, rl_end);

	exec_prompt_cmds = 1;
	if (cur_ws == 3) {
		if (prev_ws != cur_ws) {
			char t[16];
			snprintf(t, sizeof(t), "ws %d", prev_ws + 1);
			return run_kb_cmd(t);
		}
		return EXIT_SUCCESS;
	}

	return run_kb_cmd("ws 4");
}

static int
run_plugin(const int num)
{
	if (rl_line_buffer && *rl_line_buffer)
		setenv("CLIFM_LINE", rl_line_buffer, 1);

	char cmd[32 + 7];
	snprintf(cmd, sizeof(cmd), "plugin%d", num);
	int ret = run_kb_cmd(cmd);

	unsetenv("CLIFM_LINE");

	return ret;
}

static int
rl_plugin1(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_plugin(1);
}

static int
rl_plugin2(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_plugin(2);
}

static int
rl_plugin3(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_plugin(3);
}

static int
rl_plugin4(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_plugin(4);
}

static int
rl_launch_view(int count, int key)
{
	UNUSED(count); UNUSED(key);
	exec_prompt_cmds = 1;
	return run_kb_cmd("view");
}

static int
rl_toggle_only_dirs(int count, int key)
{
	UNUSED(count); UNUSED(key);

	if (kbind_busy == 1)
		return EXIT_SUCCESS;

	conf.only_dirs = conf.only_dirs ? 0 : 1;

	int exit_status = exit_code;
	if (conf.autols == 1) {
		if (conf.clear_screen == 0)
			putchar('\n');
		reload_dirlist();
	}

	print_reload_msg(_("Only directories %s\n"), conf.only_dirs
		? _("enabled") : _("disabled"));
	xrl_reset_line_state();
	return exit_status;
}

#ifndef _NO_HIGHLIGHT
static void
print_highlight_string(char *s, const int insert_point)
{
	if (!s || !*s)
		return;

	size_t i, l = 0;

	rl_delete_text(insert_point, rl_end);
	rl_point = rl_end = insert_point;
	fputs(tx_c, stdout);
	cur_color = tx_c;

	char q[PATH_MAX];
	for (i = 0; s[i]; i++) {
		rl_highlight(s, i, SET_COLOR);

		if ((signed char)s[i] < 0) {
			q[l] = s[i];
			l++;

			if ((signed char)s[i + 1] >= 0) {
				q[l] = '\0';
				l = 0;
				rl_insert_text(q);
				rl_redisplay();
			}

			continue;
		}

		q[0] = s[i];
		q[1] = '\0';
		rl_insert_text(q);
		rl_redisplay();
	}
}
#endif /* !_NO_HIGHLIGHT */

static int
print_cmdhist_line(int n, int beg_line)
{
#ifndef _NO_SUGGESTIONS
	if (wrong_cmd == 1)
		recover_from_wrong_cmd();
#endif /* !_NO_SUGGESTIONS */

	curhistindex = (size_t)n;

	HIDE_CURSOR;
	int rl_point_bk = rl_point;

#ifndef _NO_HIGHLIGHT
	if (conf.highlight == 1)
		print_highlight_string(history[n].cmd, 0);
	else
#endif /* !_NO_HIGHLIGHT */
	{
		rl_replace_line(history[n].cmd, 1);
	}

	UNHIDE_CURSOR;
	rl_point = (beg_line == 1) ? rl_end : rl_point_bk;
	cur_color = df_c;
	fputs(df_c, stdout);

	return EXIT_SUCCESS;
}

static inline int
handle_cmdhist_beginning(int key)
{
	int p = (int)curhistindex;
	cmdhist_flag = 1;

	if (key == 65) { /* Up arrow key */
		if (--p < 0)
			return EXIT_FAILURE;
	} else { /* Down arrow key */
		if (rl_end == 0)
			return EXIT_SUCCESS;
		if (++p >= (int)current_hist_n) {
			rl_replace_line("", 1);
			curhistindex++;
			return EXIT_SUCCESS;
		}
	}

	if (!history[p].cmd)
		return EXIT_FAILURE;

	curhistindex = (size_t)p;

	return print_cmdhist_line(p, 1);
}

static inline int
handle_cmdhist_middle(int key)
{
	int found = 0, p = (int)curhistindex;

	if (key == 65) { /* Up arrow key */
		if (--p < 0) return EXIT_FAILURE;

		while (p >= 0 && history[p].cmd) {
			if (strncmp(rl_line_buffer, history[p].cmd, (size_t)rl_point) == 0
			&& strcmp(rl_line_buffer, history[p].cmd) != 0) {
				found = 1; break;
			}
			p--;
		}
	} else { /* Down arrow key */
		if (++p >= (int)current_hist_n)	return EXIT_FAILURE;

		while (history[p].cmd) {
			if (strncmp(rl_line_buffer, history[p].cmd, (size_t)rl_point) == 0
			&& strcmp(rl_line_buffer, history[p].cmd) != 0) {
				found = 1; break;
			}
			p++;
		}
	}

	if (found == 0) {
		rl_ring_bell();
		return EXIT_FAILURE;
	}

	return print_cmdhist_line(p, 0);
}

/* Handle keybinds for the cmds history: UP/C-p and DOWN/C-n */
static int
rl_cmdhist(int count, int key)
{
	UNUSED(count);
	if (rl_nohist == 1)
		return EXIT_SUCCESS;

#ifndef _NO_SUGGESTIONS
	if (suggestion_buf) {
		free(suggestion_buf);
		suggestion_buf = (char *)NULL;
	}
#endif /* !_NO_SUGGESTIONS */

	if (key == 16) /* C-p  */
		key = 65;  /* Up   */
	if (key == 14) /* C-n  */
		key = 66;  /* Down */

	if (key != 65 && key != 66)
		return EXIT_FAILURE;

	/* If the cursor is at the beginning of the line */
	if (rl_point == 0 || cmdhist_flag == 1)
		return handle_cmdhist_beginning(key);

	return handle_cmdhist_middle(key);
}

static int
rl_toggle_disk_usage(int count, int key)
{
	UNUSED(count); UNUSED(key);

	/* Default values */
	static int dsort = DEF_SORT, dlong = DEF_LONG_VIEW,
		ddirsize = DEF_FULL_DIR_SIZE, ddf = DEF_LIST_DIRS_FIRST;

	if (xargs.disk_usage_analyzer == 1) {
		xargs.disk_usage_analyzer = 0;
		conf.sort = dsort;
		conf.long_view = dlong;
		conf.full_dir_size = ddirsize;
		conf.list_dirs_first = ddf;
	} else {
		xargs.disk_usage_analyzer = 1;
		dsort = conf.sort;
		dlong = conf.long_view;
		ddirsize = conf.full_dir_size;
		ddf = conf.list_dirs_first;

		conf.sort = STSIZE;
		conf.long_view = conf.full_dir_size = 1;
		conf.list_dirs_first = 0;
	}

	int exit_status = exit_code;
	if (conf.autols == 1) {
		if (conf.clear_screen == 0)
			putchar('\n');
		reload_dirlist();
	}

	print_reload_msg(_("Disk usage analyzer %s\n"),
		xargs.disk_usage_analyzer == 1 ? _("enabled") : _("disabled"));
	xrl_reset_line_state();
	return exit_status;
}

static int
rl_tab_comp(int count, int key)
{
#ifndef _NO_SUGGESTIONS
	if (suggestion.printed && suggestion_buf)
		clear_suggestion(CS_FREEBUF);
#endif /* !_NO_SUGGESTIONS */
	UNUSED(count); UNUSED(key);

	tab_complete('!');
	return EXIT_SUCCESS;
}

static int
rl_del_last_word(int count, int key)
{
#ifndef _NO_SUGGESTIONS
	if (suggestion.printed && suggestion_buf)
		clear_suggestion(CS_FREEBUF);
#endif /* !_NO_SUGGESTIONS */
	UNUSED(count); UNUSED(key);

	if (rl_point == 0)
		return EXIT_SUCCESS;

	char *end_buf = (char *)NULL;
	if (rl_point < rl_end) { /* Somewhere before the end of the line */
		end_buf = rl_copy_text(rl_point, rl_end);
		rl_delete_text(rl_point, rl_end);
	}

	char *b = rl_line_buffer;

	if (b[rl_point - 1] == '/' || b[rl_point - 1] == ' ') {
		--rl_point;
		b[rl_point] = '\0';
		--rl_end;
	}

	int n = 0;
	char *p = xstrrpbrk(b, WORD_DELIMITERS);
	if (p)
		n = (int)(p - b) + (*(p + 1) ? 1 : 0);

	rl_begin_undo_group();
	rl_delete_text(n, rl_end);
	rl_end_undo_group();
	rl_point = rl_end = n;
	if (end_buf) {
		rl_insert_text(end_buf);
		rl_point = n;
		free(end_buf);
	}
	rl_redisplay();

#ifndef _NO_SUGGESTIONS
	if (conf.suggestions == 1 && n == 0 && wrong_cmd)
		recover_from_wrong_cmd();
#endif /* !_NO_SUGGESTIONS */

	return EXIT_SUCCESS;
}

static int
rl_toggle_virtualdir_full_paths(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (!stdin_tmp_dir || strcmp(stdin_tmp_dir, workspaces[cur_ws].path) != 0)
		return EXIT_SUCCESS;

	xchmod(stdin_tmp_dir, "0700", 1);
	xargs.virtual_dir_full_paths = xargs.virtual_dir_full_paths == 1 ? 0 : 1;

	filesn_t i = files;
	while (--i >= 0) {
		char *rp = realpath(file_info[i].name, NULL);
		if (!rp) continue;

		char *p = (char *)NULL;
		if (xargs.virtual_dir_full_paths != 1) {
			if ((p = strrchr(rp, '/')) && *(p + 1))
				++p;
		} else {
			p = replace_slashes(rp, ':');
		}

		if (!p || !*p) continue;

		if (renameat(XAT_FDCWD, file_info[i].name, XAT_FDCWD, p) == -1)
			err('w', PRINT_PROMPT, "renameat: %s: %s\n",
				file_info[i].name, strerror(errno));

		if (xargs.virtual_dir_full_paths == 1) free(p);
		free(rp);
	}

	xchmod(stdin_tmp_dir, "0500", 1);

	if (conf.clear_screen == 0)
		putchar('\n');

	reload_dirlist();
	print_reload_msg(_("Switched to %s names\n"),
		xargs.virtual_dir_full_paths == 1 ? _("long") : _("short"));
	xrl_reset_line_state();
	return EXIT_SUCCESS;
}

/* Used to disable keybindings. */
static int
do_nothing(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return EXIT_SUCCESS;
}

static void
set_keybinds_from_file(void)
{
	/* Help */
	rl_bind_keyseq(find_key("show-manpage"), rl_manpage);
	rl_bind_keyseq(find_key("show-manpage2"), rl_manpage);
	rl_bind_keyseq(find_key("show-cmds"), rl_cmds_help);
	rl_bind_keyseq(find_key("show-cmds2"), rl_cmds_help);
	rl_bind_keyseq(find_key("show-kbinds"), rl_kbinds_help);
	rl_bind_keyseq(find_key("show-kbinds2"), rl_kbinds_help);

	/* Navigation */
	/* Define multiple keybinds for different terminals:
	 * rxvt, xterm, kernel console. */
	rl_bind_keyseq(find_key("parent-dir"), rl_dir_parent);
	rl_bind_keyseq(find_key("parent-dir2"), rl_dir_parent);
	rl_bind_keyseq(find_key("parent-dir3"), rl_dir_parent);
	rl_bind_keyseq(find_key("parent-dir4"), rl_dir_parent);
	rl_bind_keyseq(find_key("previous-dir"), rl_dir_previous);
	rl_bind_keyseq(find_key("previous-dir2"), rl_dir_previous);
	rl_bind_keyseq(find_key("previous-dir3"), rl_dir_previous);
	rl_bind_keyseq(find_key("previous-dir4"), rl_dir_previous);
	rl_bind_keyseq(find_key("next-dir"), rl_dir_next);
	rl_bind_keyseq(find_key("next-dir2"), rl_dir_next);
	rl_bind_keyseq(find_key("next-dir3"), rl_dir_next);
	rl_bind_keyseq(find_key("next-dir4"), rl_dir_next);
	rl_bind_keyseq(find_key("home-dir"), rl_dir_home);
	rl_bind_keyseq(find_key("home-dir2"), rl_dir_home);
	rl_bind_keyseq(find_key("home-dir3"), rl_dir_home);
	rl_bind_keyseq(find_key("home-dir4"), rl_dir_home);
	rl_bind_keyseq(find_key("root-dir"), rl_dir_root);
	rl_bind_keyseq(find_key("root-dir2"), rl_dir_root);
	rl_bind_keyseq(find_key("root-dir3"), rl_dir_root);
	rl_bind_keyseq(find_key("pinned-dir"), rl_dir_pinned);
	rl_bind_keyseq(find_key("workspace1"), rl_ws1);
	rl_bind_keyseq(find_key("workspace2"), rl_ws2);
	rl_bind_keyseq(find_key("workspace3"), rl_ws3);
	rl_bind_keyseq(find_key("workspace4"), rl_ws4);

	/* Operations on files */
	rl_bind_keyseq(find_key("create-file"), rl_create_file);
	rl_bind_keyseq(find_key("archive-sel"), rl_archive_sel);
	rl_bind_keyseq(find_key("open-sel"), rl_open_sel);
	rl_bind_keyseq(find_key("export-sel"), rl_export_sel);
	rl_bind_keyseq(find_key("move-sel"), rl_move_sel);
	rl_bind_keyseq(find_key("rename-sel"), rl_rename_sel);
	rl_bind_keyseq(find_key("remove-sel"), rl_remove_sel);
	rl_bind_keyseq(find_key("trash-sel"), rl_trash_sel);
	rl_bind_keyseq(find_key("untrash-all"), rl_untrash_all);
	rl_bind_keyseq(find_key("paste-sel"), rl_paste_sel);
	rl_bind_keyseq(find_key("select-all"), rl_select_all);
	rl_bind_keyseq(find_key("deselect-all"), rl_deselect_all);

	/* Config files */
	rl_bind_keyseq(find_key("open-mime"), rl_open_mime);
	rl_bind_keyseq(find_key("open-jump-db"), rl_open_jump_db);
	rl_bind_keyseq(find_key("open-preview"), rl_open_preview);
	rl_bind_keyseq(find_key("edit-color-scheme"), rl_open_cscheme);
	rl_bind_keyseq(find_key("open-config"), rl_open_config);
	rl_bind_keyseq(find_key("open-keybinds"), rl_open_keybinds);
	rl_bind_keyseq(find_key("open-bookmarks"), rl_open_bm_file);

	/* Settings */
	rl_bind_keyseq(find_key("toggle-virtualdir-full-paths"), rl_toggle_virtualdir_full_paths);
	rl_bind_keyseq(find_key("clear-msgs"), rl_clear_msgs);
#ifndef _NO_PROFILES
	rl_bind_keyseq(find_key("next-profile"), rl_profile_next);
	rl_bind_keyseq(find_key("previous-profile"), rl_profile_previous);
#endif /* _NO_PROFILES */
	rl_bind_keyseq(find_key("quit"), rl_quit);
	rl_bind_keyseq(find_key("lock"), rl_lock);
	rl_bind_keyseq(find_key("refresh-screen"), rl_refresh);
	rl_bind_keyseq(find_key("clear-line"), rl_clear_line);
	rl_bind_keyseq(find_key("toggle-hidden"), rl_toggle_hidden_files);
	rl_bind_keyseq(find_key("toggle-hidden2"), rl_toggle_hidden_files);
	rl_bind_keyseq(find_key("toggle-long"), rl_toggle_long_view);
	rl_bind_keyseq(find_key("toggle-light"), rl_toggle_light_mode);
	rl_bind_keyseq(find_key("dirs-first"), rl_toggle_dirs_first);
	rl_bind_keyseq(find_key("sort-previous"), rl_sort_previous);
	rl_bind_keyseq(find_key("sort-next"), rl_sort_next);
	rl_bind_keyseq(find_key("only-dirs"), rl_toggle_only_dirs);

	/* Misc */
	rl_bind_keyseq(find_key("launch-view"), rl_launch_view);
	rl_bind_keyseq(find_key("new-instance"), rl_new_instance);
	rl_bind_keyseq(find_key("show-dirhist"), rl_dirhist);
	rl_bind_keyseq(find_key("bookmarks"), rl_bookmarks);
	rl_bind_keyseq(find_key("mountpoints"), rl_mountpoints);
	rl_bind_keyseq(find_key("selbox"), rl_selbox);
	rl_bind_keyseq(find_key("prepend-sudo"), rl_prepend_sudo);
	rl_bind_keyseq(find_key("toggle-disk-usage"), rl_toggle_disk_usage);
	rl_bind_keyseq(find_key("toggle-max-name-len"), rl_toggle_max_filename_len);
	rl_bind_keyseq(find_key("quit"), rl_quit);

	/* Plugins */
	rl_bind_keyseq(find_key("plugin1"), rl_plugin1);
	rl_bind_keyseq(find_key("plugin2"), rl_plugin2);
	rl_bind_keyseq(find_key("plugin3"), rl_plugin3);
	rl_bind_keyseq(find_key("plugin4"), rl_plugin4);
}

static void
set_default_keybinds(void)
{
	/* Help */
	rl_bind_keyseq("\\eOP", rl_manpage);
	rl_bind_keyseq("\\eOQ", rl_cmds_help);
	rl_bind_keyseq("\\eOR", rl_kbinds_help);
	rl_bind_keyseq("\\e[11~", rl_manpage);
	rl_bind_keyseq("\\e[12~", rl_cmds_help);
	rl_bind_keyseq("\\e[13~", rl_kbinds_help);

	/* Navigation */
	rl_bind_keyseq("\\M-u", rl_dir_parent);
	rl_bind_keyseq("\\e[a", rl_dir_parent);
	rl_bind_keyseq("\\e[2A", rl_dir_parent);
	rl_bind_keyseq("\\e[1;2A", rl_dir_parent);
	rl_bind_keyseq("\\M-j", rl_dir_previous);
	rl_bind_keyseq("\\e[d", rl_dir_previous);
	rl_bind_keyseq("\\e[2D", rl_dir_previous);
	rl_bind_keyseq("\\e[1;2D", rl_dir_previous);
	rl_bind_keyseq("\\M-k", rl_dir_next);
	rl_bind_keyseq("\\e[c", rl_dir_next);
	rl_bind_keyseq("\\e[2C", rl_dir_next);
	rl_bind_keyseq("\\e[1;2C", rl_dir_next);
	rl_bind_keyseq("\\M-e", rl_dir_home);
	rl_bind_keyseq("\\e[1~", rl_dir_home);
	rl_bind_keyseq("\\e[7~", rl_dir_home);
	rl_bind_keyseq("\\e[H", rl_dir_home);
	rl_bind_keyseq("\\M-r", rl_dir_root);
	rl_bind_keyseq("\\e/", rl_dir_root);
	rl_bind_keyseq("\\M-p", rl_dir_pinned);
	rl_bind_keyseq("\\M-1", rl_ws1);
	rl_bind_keyseq("\\M-2", rl_ws2);
	rl_bind_keyseq("\\M-3", rl_ws3);
	rl_bind_keyseq("\\M-4", rl_ws4);

	/* Operations on files */
	rl_bind_keyseq("\\M-n", rl_create_file);
	rl_bind_keyseq("\\C-\\M-a", rl_archive_sel);
	rl_bind_keyseq("\\C-\\M-g", rl_open_sel);
	rl_bind_keyseq("\\C-\\M-e", rl_export_sel);
	rl_bind_keyseq("\\C-\\M-n", rl_move_sel);
	rl_bind_keyseq("\\C-\\M-r", rl_rename_sel);
	rl_bind_keyseq("\\C-\\M-d", rl_remove_sel);
	rl_bind_keyseq("\\C-\\M-t", rl_trash_sel);
	rl_bind_keyseq("\\C-\\M-u", rl_untrash_all);
	rl_bind_keyseq("\\C-\\M-v", rl_paste_sel);
	rl_bind_keyseq("\\M-a", rl_select_all);
	rl_bind_keyseq("\\M-d", rl_deselect_all);
	rl_bind_keyseq("\\M-v", rl_prepend_sudo);

	/* Config files */
	rl_bind_keyseq("\\e[17~", rl_open_mime);
	rl_bind_keyseq("\\e[18~", rl_open_jump_db);
	rl_bind_keyseq("\\e[19~", rl_open_cscheme);
	rl_bind_keyseq("\\e[20~", rl_open_keybinds);
	rl_bind_keyseq("\\e[21~", rl_open_config);
	rl_bind_keyseq("\\e[23~", rl_open_bm_file);

	/* Settings */
	rl_bind_keyseq("\\M-w", rl_toggle_virtualdir_full_paths);
	rl_bind_keyseq("\\M-t", rl_clear_msgs);
	rl_bind_keyseq("\\M-o", rl_lock);
	rl_bind_keyseq("\\C-r", rl_refresh);
	rl_bind_keyseq("\\M-c", rl_clear_line);
	rl_bind_keyseq("\\M-i", rl_toggle_hidden_files);
	rl_bind_keyseq("\\M-.", rl_toggle_hidden_files);
	rl_bind_keyseq("\\M-l", rl_toggle_long_view);
	rl_bind_keyseq("\\M-y", rl_toggle_light_mode);
	rl_bind_keyseq("\\M-g", rl_toggle_dirs_first);
	rl_bind_keyseq("\\M-z", rl_sort_previous);
	rl_bind_keyseq("\\M-x", rl_sort_next);
	rl_bind_keyseq("\\M-,", rl_toggle_only_dirs);

	rl_bind_keyseq("\\M--", rl_launch_view);
	rl_bind_keyseq("\\C-x", rl_new_instance);
	rl_bind_keyseq("\\M-h", rl_dirhist);
	rl_bind_keyseq("\\M-b", rl_bookmarks);
	rl_bind_keyseq("\\M-m", rl_mountpoints);
	rl_bind_keyseq("\\M-s", rl_selbox);

	rl_bind_keyseq("\\C-\\M-l", rl_toggle_max_filename_len);
	rl_bind_keyseq("\\C-\\M-i", rl_toggle_disk_usage);
	rl_bind_keyseq("\\e[24~", rl_quit);
}

static void
set_hardcoded_keybinds(void)
{
#ifndef __HAIKU__
	rl_bind_keyseq("\\C-l", rl_refresh);
	rl_bind_keyseq("\\C-p", rl_cmdhist);
	rl_bind_keyseq("\\C-n", rl_cmdhist);
#endif /* !__HAIKU__ */
	rl_bind_keyseq("\x1b[A", rl_cmdhist);
	rl_bind_keyseq("\x1b[B", rl_cmdhist);

	rl_bind_keyseq("\\M-q", rl_del_last_word);
	rl_bind_key('\t', rl_tab_comp);

#ifndef _NO_SUGGESTIONS
# ifndef __HAIKU__
	rl_bind_keyseq("\\C-f", rl_accept_suggestion);
	rl_bind_keyseq("\x1b[C", rl_accept_suggestion);
	rl_bind_keyseq("\x1bOC", rl_accept_suggestion);

	/* Bind Alt-Right and Alt-f to accept the first suggested word */
/*	rl_bind_key(('f' | 0200), rl_accept_first_word); */ // Alt-f
	rl_bind_keyseq("\x1b\x66", rl_accept_first_word);
	rl_bind_keyseq("\x1b[3C", rl_accept_first_word);
	rl_bind_keyseq("\x1b\x1b[C", rl_accept_first_word);
	rl_bind_keyseq("\x1b[1;3C", rl_accept_first_word);
# else
	rl_bind_keyseq("\x1bOC", rl_accept_suggestion);
	rl_bind_keyseq("\\C-f", rl_accept_first_word);
# endif /* !__HAIKU__ */
#endif /* !_NO_SUGGESTIONS */
}

void
readline_kbinds(void)
{
	/* Disable "Esc + Enter". Otherwise, it switches to vi mode, which is not
	 * intended (for instance, in a secondary prompt).
	 * Disable it here so that the user can rebind it using the config file
	 * (readline.clifm or keybindings.clifm). */
	rl_bind_keyseq("\x1b\xd", do_nothing);

	if (kbinds_file)
		set_keybinds_from_file();
	else
		set_default_keybinds();

	set_hardcoded_keybinds();
}
