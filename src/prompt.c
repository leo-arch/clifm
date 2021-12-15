/* prompt.c -- functions controlling the appearance and behaviour of the prompt */

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

#include <stdio.h>
#include <string.h>
#if !defined(__HAIKU__) && !defined(__OpenBSD__)
#include <wordexp.h>
#endif
#include <readline/readline.h>

#include "aux.h"
#include "exec.h"
#include "history.h"
#include "init.h"
#include "listing.h"
#include "misc.h"
#include "navigation.h"
#include "prompt.h"
#ifndef _NO_TRASH
#include "trash.h"
#else
#include <time.h>
#endif

/* Decode the prompt string (encoded_prompt global variable) taken from
 * the configuration file. Based on the decode_prompt_string function
 * found in an old bash release (1.14.7). */
static char *
decode_prompt(const char *line)
{
	if (!line)
		return (char *)NULL;

#define CTLESC '\001'
#define CTLNUL '\177'

	char *temp = (char *)NULL, *result = (char *)NULL;
	size_t result_len = 0;
	int c;

	while ((c = *line++)) {
		/* We have an escape char */
		if (c == '\\') {
			/* Now move on to the next char */
			c = *line;
			switch (c) {

			case 'z': /* Exit status of last executed command */
				temp = (char *)xnmalloc(strlen(xitoa(exit_code)) + 12 + MAX_COLOR,
						sizeof(char));
				sprintf(temp, "\001%s\002%d\001%s\002",
						(exit_code == 0) ? (colorize ? _GREEN : "")
						: (colorize ? _RED : ""), exit_code, df_c);
				goto add_string;
/*				temp = (char *)xnmalloc(3, sizeof(char));
				temp[0] = ':';
				temp[1] = (exit_code) ? '(' : ')';
				temp[2] = '\0';
				goto add_string; */

/*			case 'x': // Hex numbers
			{
				// Go back one char, so that we have "\x ... n", which
				// is what the get_hex_num() requires
				line--;
				// get_hex_num returns an array on integers corresponding
				// to the hex codes found in line up to the fisrt non-hex
				// expression
				int *hex = get_hex_num(line);
				int n = 0, i = 0, j;
				// Count how many hex expressions were found
				while (hex[n++] != -1);
				n--;
				// 2 + n == CTLEST + 0x00 + amount of hex numbers
				temp = xnmalloc(2 + (size_t)n, sizeof(char));
				// Construct the line: "\001hex1hex2...n0x00"
				temp[0] = CTLESC;
				for (j = 1; j < (1 + n); j++)
					temp[j] = (char)hex[i++];
				temp[1 + n] = '\0';
				// Set the line pointer after the first non-hex
				// expression to continue processing
				line += (i * 4);
				c = 0;
				free(hex);
				goto add_string;
			} */

			case 'e': /* Escape char */
				temp = xnmalloc(2, sizeof(char));
				line++;
				/* 27 (dec) == 033 (octal) == 0x1b (hex) == \e */
				temp[0] = '\033';
				temp[1] = '\0';
				c = 0;
				goto add_string;

			case '0': /* Octal char */
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7': {
				char octal_string[4];
				int n;

				xstrsncpy(octal_string, line, 3);
				octal_string[3] = '\0';

				n = read_octal(octal_string);
				temp = xnmalloc(3, sizeof(char));

				if (n == CTLESC || n == CTLNUL) {
					line += 3;
					temp[0] = CTLESC;
					temp[1] = (char)n;
					temp[2] = '\0';
				} else if (n == -1) {
					temp[0] = '\\';
					temp[1] = '\0';
				} else {
					line += 3;
					temp[0] = (char)n;
					temp[1] = '\0';
				}

				c = 0;
				goto add_string;
			}

			case 'c': /* Program name */
				temp = savestring(PNL, strlen(PNL));
				goto add_string;

			case 'P': /* Current profile name */
				if (!alt_profile)
					temp = savestring("default", 7);
				else
					temp = savestring(alt_profile, strlen(alt_profile));
				goto add_string;

			case 't': /* Time: 24-hour HH:MM:SS format */
			case 'T': /* 12-hour HH:MM:SS format */
			case 'A': /* 24-hour HH:MM format */
			case '@': /* 12-hour HH:MM:SS am/pm format */
			case 'd': /* Date: abrev_weak_day, abrev_month_day month_num */
			{
				time_t rawtime = time(NULL);
				struct tm tm;
				localtime_r(&rawtime, &tm);
				if (c == 't') {
					char time[9] = "";
					strftime(time, sizeof(time), "%H:%M:%S", &tm);
					temp = savestring(time, sizeof(time));
				} else if (c == 'T') {
					char time[9] = "";
					strftime(time, sizeof(time), "%I:%M:%S", &tm);
					temp = savestring(time, sizeof(time));
				} else if (c == 'A') {
					char time[6] = "";
					strftime(time, sizeof(time), "%H:%M", &tm);
					temp = savestring(time, sizeof(time));
				} else if (c == '@') {
					char time[12] = "";
					strftime(time, sizeof(time), "%I:%M:%S %p", &tm);
					temp = savestring(time, sizeof(time));
				} else { /* c == 'd' */
					char time[12] = "";
					strftime(time, sizeof(time), "%a %b %d", &tm);
					temp = savestring(time, sizeof(time));
				}
				goto add_string;
			}

			case 'u': /* User name */
				if (!user.name)
					temp = savestring("?", 1);
				else
					temp = savestring(user.name, strlen(user.name));
				goto add_string;

			case 'h': /* Hostname up to first '.' */
			case 'H': /* Full hostname */
				temp = savestring(hostname, strlen(hostname));
				if (c == 'h') {
					int ret = strcntchr(hostname, '.');
					if (ret != -1)
						temp[ret] = '\0';
				}
				goto add_string;

			case 's': /* Shell name (after last slash)*/
			{
				if (!user.shell) {
					line++;
					break;
				}
				char *shell_name = strrchr(user.shell, '/');
				temp = savestring(shell_name + 1, strlen(shell_name) - 1);
				goto add_string;
			}

			case 'S': { /* Current workspace */
#define __WS_STR_LEN sizeof(int) + 6 + (MAX_COLOR + 2) * 2
				char s[__WS_STR_LEN];
				char *cl = (char *)NULL;
				switch(cur_ws + 1) {
				case 1: cl = ws1_c; break;
				case 2: cl = ws2_c; break;
				case 3: cl = ws3_c; break;
				case 4: cl = ws4_c; break;
				case 5: cl = ws5_c; break;
				case 6: cl = ws6_c; break;
				case 7: cl = ws7_c; break;
				case 8: cl = ws8_c; break;
				}
				snprintf(s, __WS_STR_LEN, "%s%d\001%s\002", cl, cur_ws + 1, df_c);
				temp = savestring(s, strlen(s));
				goto add_string;
			}

			case 'l': { /* Current mode */
				char s[2];
				s[0] = (light_mode ? 'L' : '\0');
				s[1] = '\0';
				temp = savestring(s, 1);
				goto add_string;
			}

			case 'p':
			case 'w': /* Full PWD */
			case 'W': /* Short PWD */
			{
				if (!ws[cur_ws].path) {
					line++;
					break;
				}

				/* Reduce HOME to "~" */
				int free_tmp_path = 0;
				char *tmp_path = (char *)NULL;
				if (strncmp(ws[cur_ws].path, user.home,
					user.home_len) == 0)
					tmp_path = home_tilde(ws[cur_ws].path);
				if (!tmp_path)
					tmp_path = ws[cur_ws].path;
				else
					free_tmp_path = 1;

				if (c == 'W') {
					char *ret = (char *)NULL;
					/* If not root dir (/), get last dir name */
					if (!(*tmp_path == '/' && !*(tmp_path + 1)))
						ret = strrchr(tmp_path, '/');

					if (!ret)
						temp = savestring(tmp_path, strlen(tmp_path));
					else
						temp = savestring(ret + 1, strlen(ret) - 1);
				}

				/* Reduce path only if longer than max_path */
				else if (c == 'p') {
					if (strlen(tmp_path) > (size_t)max_path) {
						char *ret = (char *)NULL;
						ret = strrchr(tmp_path, '/');
						if (!ret)
							temp = savestring(tmp_path, strlen(tmp_path));
						else
							temp = savestring(ret + 1, strlen(ret) - 1);
					} else {
						temp = savestring(tmp_path, strlen(tmp_path));
					}
				} else { /* If c == 'w' */
					temp = savestring(tmp_path, strlen(tmp_path));
				}

				if (free_tmp_path)
					free(tmp_path);

				goto add_string;
			}

			case '$': /* '$' or '#' for normal and root user */
				if ((flags & ROOT_USR))
					temp = savestring("#", 1);
				else
					temp = savestring("$", 1);
				goto add_string;

			case 'a': /* Bell character */
			case 'r': /* Carriage return */
			case 'n': /* New line char */
				temp = savestring(" ", 1);
				if (c == 'n')
					temp[0] = '\n';
				else if (c == 'r')
					temp[0] = '\r';
				else
					temp[0] = '\a';
				goto add_string;

			case '[': /* Begin a sequence of non-printing characters.
			Mostly used to add color sequences. Ex: \[\033[1;34m\] */
			case ']': /* End the sequence */
				temp = xnmalloc(2, sizeof(char));
				temp[0] = (c == '[') ? RL_PROMPT_START_IGNORE
						     : RL_PROMPT_END_IGNORE;
				temp[1] = '\0';
				goto add_string;

			case '\\': /* Literal backslash */
				temp = savestring("\\", 1);
				goto add_string;

			default:
				temp = savestring("\\ ", 2);
				temp[1] = (char)c;

			add_string:
				if (!temp)
					break;
				if (c)
					line++;
				result_len += strlen(temp);
				if (!result)
					result = (char *)xcalloc(result_len + 1, sizeof(char));
				else
					result = (char *)xrealloc(result, (result_len + 1) * sizeof(char));
				strcat(result, temp);
				free(temp);
				break;
			}
		}

		/* If not escape code, check for command substitution, and if not,
		 * just add whatever char is there */
		else {
			/* Remove non-escaped quotes */
			if (c == '\'' || c == '"')
				continue;

#if !defined(__HAIKU__) && !defined(__OpenBSD__)
			/* Command substitution */
			if (c == '$' && *line == '(') {
				/* Look for the ending parenthesis */
				int tmp = strcntchr(line, ')');
				if (tmp == -1)
					continue;

				/* Copy the cmd to be substituted and pass it to wordexp */
				char *tmp_str = (char *)xnmalloc(strlen(line) + 2, sizeof(char));
				sprintf(tmp_str, "$%s", line);

				tmp_str[tmp + 2] = '\0';
				line += tmp + 1;

				const char *old_value = getenv("IFS");
				setenv("IFS", "", 1);

				wordexp_t wordbuf;
				if (wordexp(tmp_str, &wordbuf, 0) != EXIT_SUCCESS) {
					free(tmp_str);
					if (old_value)
						setenv("IFS", old_value, 1);
					else
						unsetenv("IFS");
					continue;
				}

				if (old_value)
					setenv("IFS", old_value, 1);
				else
					unsetenv("IFS");

				free(tmp_str);

				if (wordbuf.we_wordc) {
					size_t j;
					for (j = 0; j < wordbuf.we_wordc; j++) {

						size_t word_len = strlen(wordbuf.we_wordv[j]);
						result_len += word_len;

						if (!result)
							result = (char *)xcalloc(result_len + 2, sizeof(char));
						else
							result = (char *)xrealloc(result, (result_len + 2)
													* sizeof(char));
						strcat(result, wordbuf.we_wordv[j]);

						/* If not the last word in cmd output, add an space */
/*						if (j < wordbuf.we_wordc - 1) {
							result[result_len++] = ' ';
							result[result_len] = '\0';
						} */
					}
				}

				wordfree(&wordbuf);
				continue;
			}
#endif /* __HAIKU__ && __OpenBSD__ */

			result = (char *)xrealloc(result, (result_len + 2) * sizeof(char));
			result[result_len++] = (char)c;
			result[result_len] = '\0';
		}
	}

	/* Remove trailing new line char, if any */
	if (result && result[result_len - 1] == '\n')
		result[result_len - 1] = '\0';

	return result;
}

/* Print the prompt and return the string entered by the user (to be
 * parsed later by parse_input_str()) */
char *
prompt(void)
{
	/* Make sure CWD exists; if not, go up to the parent, and so
	 * on */
	while (xchdir(ws[cur_ws].path, SET_TITLE) != EXIT_SUCCESS) {
		char *ret = strrchr(ws[cur_ws].path, '/');
		if (ret && ret != ws[cur_ws].path)
			*ret = '\0';
		else
			break;
	}

	/* Remove all final slash(es) from path, if any */
	size_t path_len = strlen(ws[cur_ws].path), i;

	for (i = path_len - 1; ws[cur_ws].path[i] && i > 0; i--) {
		if (ws[cur_ws].path[i] != '/')
			break;
		else
			ws[cur_ws].path[i] = '\0';
	}

	if (welcome_message) {
		printf("%s%s > %s\n%s%s\n", wc_c, PROGRAM_NAME, _(PROGRAM_DESC),
				df_c, _(HELP_MESSAGE));
		welcome_message = 0;
	}

	/* Print the tip of the day (only for the first run) */
	if (tips) {
		static int first_run = 1;
		if (first_run) {
			print_tips(0);
			first_run = 0;
		}
	}

	fputs(df_c, stdout);
	fflush(stdout);
	/* Execute prompt commands, if any, and only if external commands
	 * are allowed */
	if (ext_cmd_ok && prompt_cmds_n > 0) {
		for (i = 0; i < prompt_cmds_n; i++)
			launch_execle(prompt_cmds[i]);
	}
#ifndef _NO_TRASH
	/* Update trash and sel file indicator on every prompt call */
	if (trash_ok) {
		trash_n = count_dir(trash_files_dir, NO_CPOP);
		if (trash_n <= 2)
			trash_n = 0;
	}
#endif
	get_sel_files();

	/* Messages are categorized in three groups: errors, warnings, and
	 * notices. The kind of message should be specified by the function
	 * printing the message itself via a global enum: pmsg, with the
	 * following values: NOMSG, ERROR, WARNING, and NOTICE. */
	char msg_str[MAX_COLOR + 1 + 16] = "";

	if (msgs_n) {
		/* Errors take precedence over warnings, and warnings over
		 * notices. That is to say, if there is an error message AND a
		 * warning message, the prompt will always display the error
		 * message sign: a red 'E'. */
		switch (pmsg) {
		case NOMSG:	break;
		case ERROR:	sprintf(msg_str, "%sE%s", em_c, RL_NC); break;
		case WARNING: sprintf(msg_str, "%sW%s", wm_c, RL_NC); break;
		case NOTICE: sprintf(msg_str, "%sN%s", nm_c, RL_NC); break;
		default: break;
		}
	}

	/* Generate the prompt string */
	if (prompt_style == CUSTOM_PROMPT_STYLE) {
		/* Set environment variables with CliFM state information
		 * (sel files, trash, stealth mode, messages) to be handled by
		 * the prompt itself */
		char t[32];
		sprintf(t, "%d", (int)sel_n);
		setenv("CLIFM_STAT_SEL", t, 1);
		sprintf(t, "%d", (int)trash_n);
		setenv("CLIFM_STAT_TRASH", t, 1);
		sprintf(t, "%d", (msgs_n && pmsg) ? (int)msgs_n : 0);
		setenv("CLIFM_STAT_MSG", t, 1);
		sprintf(t, "%d", cur_ws + 1);
		setenv("CLIFM_STAT_WS", t, 1);
		sprintf(t, "%d", exit_code);
		setenv("CLIFM_STAT_EXIT", t, 1);
		setenv("CLIFM_STAT_ROOT", (flags & ROOT_USR) ? "1" : "0", 1);
		setenv("CLIFM_STAT_STEALTH", (xargs.stealth_mode == 1) ? "1" : "0", 1);
	}

	/* First, grab and decode the prompt line of the config file (stored
	 * in encoded_prompt at startup) */
	char *decoded_prompt = decode_prompt(encoded_prompt);

	/* Emergency prompt, just in case decode_prompt fails */
	if (!decoded_prompt) {
		fprintf(stderr, _("%s: Error decoding prompt line. Using an "
				"emergency prompt\n"), PROGRAM_NAME);
		decoded_prompt = savestring("\001\x1b[0m\002> ", 8);
	}

	size_t decoded_prompt_len;
	decoded_prompt_len = strlen(decoded_prompt);

	size_t prompt_length = 0;

	if (prompt_style == DEF_PROMPT_STYLE) {
		prompt_length = (size_t)(decoded_prompt_len
		+ (xargs.stealth_mode == 1 ? 16 : 0) + ((flags & ROOT_USR) ? 16 : 0)
		+ (sel_n ? 16 : 0) + (trash_n ? 16 : 0) + ((msgs_n && pmsg) ? 16 : 0)
		+ 6 + sizeof(tx_c) + 1 + 2);
	} else {
		prompt_length = (size_t)(decoded_prompt_len + 6 + sizeof(tx_c) + 1);
	}

	/* 16 = color_b({red,green,yellow}_b)+letter (sel, trash, msg)+RL_NC;
	 * 6 = RL_NC
	 * 1 = null terminating char
	 * 2 = \001 and \002 for tx_c */

	char *the_prompt = (char *)xnmalloc(prompt_length, sizeof(char));

	if (prompt_style == DEF_PROMPT_STYLE) {
		snprintf(the_prompt, prompt_length, "%s%s%s%s%s%s%s%s%s%s\001%s\002",
			(flags & ROOT_USR) ? "\001\x1b[1;31mR\x1b[0m\002" : "",
			(msgs_n && pmsg) ? msg_str : "", (xargs.stealth_mode == 1)
			? si_c : "", (xargs.stealth_mode == 1) ? "S\001\x1b[0m\002"
			: "", (trash_n) ? ti_c : "", (trash_n) ? "T\001\x1b[0m\002" : "",
			(sel_n) ? li_c : "", (sel_n) ? "*\001\x1b[0m\002" : "",
			decoded_prompt, RL_NC, tx_c);
	} else {
		snprintf(the_prompt, prompt_length, "%s%s\001%s\002", decoded_prompt,
			RL_NC, tx_c);
	}

	free(decoded_prompt);

	/* Print error messages, if any. 'print_errors' is set to true by
	 * log_msg() with the PRINT_PROMPT flag. If NOPRINT_PROMPT is
	 * passed instead, 'print_msg' will be false and the message will
	 * be printed in place by log_msg() itself, without waiting for
	 * the next prompt */
	if (print_msg && msgs_n) {
		fputs(messages[msgs_n - 1], stderr);
		print_msg = 0; /* Print messages only once */
	}

	args_n = 0;

	curhistindex = current_hist_n;
	/* Restore foreground color */
	fputs(df_c, stdout);
	/* Print the prompt and get user input */
	char *input = (char *)NULL;
	input = readline(the_prompt);
	free(the_prompt);

	if (!input)
		return (char *)NULL;

	if (!*input) {
		free(input);
		input = (char *)NULL;
		return (char *)NULL;
	}

	/* Keep a literal copy of the last entered command to compose the
	 * commands log, if needed and enabled */
	if (logs_enabled) {
		if (last_cmd)
			free(last_cmd);
		last_cmd = savestring(input, strlen(input));
	}

	/* Do not record empty lines, exit, history commands, consecutively
	 * equal inputs, or lines starting with space */
	if (record_cmd(input))
		add_to_cmdhist(input);

	return input;
}
