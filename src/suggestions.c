/* suggestions.c -- functions to manage the suggestions system */

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

#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#ifdef __OpenBSD__
#include <strings.h>
#endif
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <termios.h>

#ifdef __linux__
#include <sys/capability.h>
#endif

#ifdef __OpenBSD__
typedef char *rl_cpvfunc_t;
#include <ereadline/readline/readline.h>
#else
#include <readline/readline.h>
#endif

#include "suggestions.h"
#include "aux.h"
#include "checks.h"
#include "colors.h"

static int free_color = 0;

/* The following three functions were taken from
 * https://github.com/antirez/linenoise/blob/master/linenoise.c
 * and modified to fir our needs: they are used to get current cursor
 * position (both vertical and horizontal) by the suggestions system */

/* Set the terminal into raw mode. Return 0 on success and -1 on error */
static int
enable_raw_mode(const int fd)
{
	struct termios raw;

	if (!isatty(STDIN_FILENO))
		goto FAIL;

	if (tcgetattr(fd, &orig_termios) == -1)
		goto FAIL;

	raw = orig_termios;  /* modify the original mode */
	/* input modes: no break, no CR to NL, no parity check, no strip char,
	 * * no start/stop output control. */
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	/* output modes - disable post processing */
	raw.c_oflag &= ~(OPOST);
	/* control modes - set 8 bit chars */
	raw.c_cflag |= (CS8);
	/* local modes - choing off, canonical off, no extended functions,
	 * no signal chars (^Z,^C) */
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    /* control chars - set return condition: min number of bytes and timer.
     * We want read to return every single byte, without timeout. */
	raw.c_cc[VMIN] = 1; raw.c_cc[VTIME] = 0; /* 1 byte, no timer */

	/* put terminal in raw mode after flushing */
	if (tcsetattr(fd, TCSAFLUSH, &raw) < 0)
		goto FAIL;

	return 0;

FAIL:
	errno = ENOTTY;
	return -1;
}

static int
disable_raw_mode(const int fd)
{
	if (tcsetattr(fd, TCSAFLUSH, &orig_termios) != -1)
		return EXIT_SUCCESS;
	return EXIT_FAILURE;
}

/* Use the "ESC [6n" escape sequence to query the cursor position (both
 * vertical and horizontal) and store both values into global variables.
 * Return 0 on success and 1 on error */
static int
get_cursor_position(const int ifd, const int ofd)
{
	char buf[32];
	int cols, rows;
	unsigned int i = 0;

	if (enable_raw_mode(ifd) == -1)
		return EXIT_FAILURE;

	/* Report cursor location */
	if (write(ofd, "\x1b[6n", 4) != 4)
		goto FAIL;

	/* Read the response: "ESC [ rows ; cols R" */
	while (i < sizeof(buf) - 1) {
		if (read(ifd, buf + i, 1) != 1)
			break;
		if (buf[i] == 'R')
			break;
		i++;
	}
	buf[i] = '\0';

	/* Parse it */
	if (buf[0] != _ESC || buf[1] != '[')
		goto FAIL;
	if (sscanf(buf + 2, "%d;%d", &rows, &cols) != 2)
		goto FAIL;

	currow = rows;
	curcol = cols;

	disable_raw_mode(ifd);
	return EXIT_SUCCESS;

FAIL:
	disable_raw_mode(ifd);
	return EXIT_FAILURE;
}

/* This function is only used before running a keybind command. We don't
 * want the suggestion buffer after running a keybind */
void
free_suggestion(void)
{
	free(suggestion_buf);
	suggestion_buf = (char *)NULL;
	suggestion.printed = suggestion.lines = 0;
}

void
clear_suggestion(void)
{
	/* Delete everything in the current line starting from the current
	 * cursor position */
	if (write(STDOUT_FILENO, DLFC, DLFC_LEN) <= 0) {}

	if (suggestion.lines > 1) {

		/* Save cursor position */
		get_cursor_position(STDIN_FILENO, STDOUT_FILENO);

		int i = suggestion.lines;
		while (--i > 0) {
			/* Move the cursor to the beginning of the next line */
			if (write(STDOUT_FILENO, "\x1b[1E", 4) <= 0) {}
			/* Delete the line */
			if (write(STDOUT_FILENO, "\x1b[0K", 4) <= 0) {}
		}
		/* Restore cursor position */
		printf("\x1b[%d;%dH", currow, curcol);
		fflush(stdout);
		suggestion.lines = 0;
	}

	suggestion.printed = 0;
}

/* Clear the line, print the suggestion (STR) at OFFSET in COLOR, and
 * move the cursor back to the original position.
 * OFFSET marks the point in STR that is already typed: the suggestion
 * will be printed starting from this point */
static void
print_suggestion(const char *str, size_t offset, const char *color)
{
	if (suggestion.printed)
		clear_suggestion();

	size_t str_len = strlen(str);
	free(suggestion_buf);
	suggestion_buf = (char *)NULL;

	/* Do not print suggestions bigger than what the current terminal
	 * window size can hold */
	if (wc_xstrlen(str) > (term_cols * term_rows) - visible_prompt_len)
		return;

	/* Store the suggestion in a buffer to be used later by the
	 * rl_accept_suggestion function (keybinds.c) */
	suggestion_buf = xnmalloc(str_len + 1, sizeof(char));
	strcpy(suggestion_buf, str);

/*	size_t line_len = strlen(rl_line_buffer); */

	/* Save cursor position in two global variables: currow and curcol */
	get_cursor_position(STDIN_FILENO, STDOUT_FILENO);

	/* Erase everything after the cursor */
	if (write(STDOUT_FILENO, DLFC, DLFC_LEN) <= 0) {}

	/* If not at the end of the line, move the cursor there */
	if (rl_end > rl_point)
		printf("\x1b[%dC", rl_end - rl_point);
	/* rl_end and rl_point are not updated: they do not include
	 * the last typed char. However, since we only care here about
	 * the difference between them, it doesn't matter: the result
	 * is the same (7 - 4 == 6 - 3 == 1) */

	/* Print the suggestion */
	printf("%s%s%s", color, str + (offset - 1), df_c);

	/* Get the amount of lines taken by the suggestion (diff) */
	size_t suggestion_len = wc_xstrlen(str + offset);
	size_t cuc = visible_prompt_len + suggestion.full_line_len;
	size_t cucs = cuc + suggestion_len;
	int clines = 0, slines = 0;

	if (cuc > term_cols)
		clines = cuc / (int)term_cols;
	if (cucs > term_cols)
		slines = cucs / (int)term_cols;

	int diff = slines - clines;

	/* If the module is zero, the cursor is in the last column
	 * of the terminal */
	int cucs_mod = cucs % term_cols;
	int cuc_mod = cuc % term_cols;

	/* Update the row number, if needed */
	/* If the cursor is in the last row, printing a multi-line suggestion
	 * will move the beginning of the current line up the number of
	 * lines taken by the suggestion, so that we need to update the
	 * value to move the cursor back to the correct row (the beginning
	 * of the line) */
	if (diff > term_rows)
		diff = term_rows;

	if (diff > 0 && cuc_mod && cucs_mod && currow == term_rows)
		currow -= diff;

	/* Restore cursor position */
	printf("\x1b[%d;%dH", currow, curcol);

	/* Store the amount of lines taken by the current command line
	 * (including the suggestion's length) to be able to correctly
	 * remove it later (via the clear_suggestion function) */
	suggestion.lines = diff + 1;

	return;
}

/* Used by the check_completions function to get file names color
 * according to file type */
static char *
get_comp_color(const char *filename, const struct stat attr)
{
	char *color = no_c; 

	switch(attr.st_mode & S_IFMT) {
	case S_IFDIR:
		if (light_mode)
			return di_c;
		if (access(filename, R_OK | X_OK) != 0) {
			color = nd_c;
		} else {
			int sticky = 0;
			int is_oth_w = 0;
			if (attr.st_mode & S_ISVTX)
				sticky = 1;

			if (attr.st_mode & S_IWOTH)
				is_oth_w = 1;

			int files_dir = count_dir(filename, CPOP);

			color = sticky ? (is_oth_w ? tw_c : st_c) : is_oth_w ? ow_c
				   : ((files_dir == 2 || files_dir == 0) ? ed_c : di_c);
		}
		break;

	case S_IFREG:
		if (light_mode)
			return fi_c;
		if (access(filename, R_OK) == -1)
			color = nf_c;
		else if (attr.st_mode & S_ISUID)
			color = su_c;
		else if (attr.st_mode & S_ISGID)
			color = sg_c;
		else {
#ifdef _LINUX_CAP
			cap_t cap = cap_get_file(filename);
			if (cap) {
				color = ca_c;
				cap_free(cap);
			} else if (attr.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
#else
			if (attr.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
#endif
				if (attr.st_size == 0)
					color = ee_c;
				else
					color = ex_c;
			} else if (attr.st_size == 0)
				color = ef_c;
			else if (attr.st_nlink > 1)
				color = mh_c;
			else {
				char *ext = strrchr(filename, '.');
				if (ext && ext != filename) {
					char *extcolor = get_ext_color(ext);
					if (extcolor) {
						char *ext_color = (char *)xnmalloc(strlen(extcolor)
											+ 4, sizeof(char));
						sprintf(ext_color, "\x1b[%sm", extcolor);
						color = ext_color;
						free_color = 1;
						extcolor = (char *)NULL;
					} else  {
						color = fi_c;
					}
				} else {
					color = fi_c;
				}
			}
		}
		break;

	case S_IFLNK: {
		if (light_mode)
			return ln_c;
		char *linkname = realpath(filename, (char *)NULL);
		if (linkname)
			color = ln_c;
		else
			color = or_c;
		}
		break;

	case S_IFSOCK: color = so_c; break;
	case S_IFBLK: color = bd_c; break;
	case S_IFCHR: color = cd_c; break;
	case S_IFIFO: color = pi_c; break;
	default: color = no_c; break;
	}

	return color;
}

static int
check_completions(const char *str, const size_t len, const char c)
{
	int printed = 0;
	size_t i;
	char **_matches = rl_completion_matches(str, rl_completion_entry_function);
	struct stat attr;
	suggestion.filetype = DT_REG;
	char *color = (char *)NULL;
	if (suggest_filetype_color)
		color = no_c;
	else
		color = sf_c;

	if (!_matches)
		return printed;

	if (!len)
		goto FREE;

	/* If only one match */
	if (_matches[0] && *_matches[0]	&& strlen(_matches[0]) > len) {
		int append_slash = 0;

		if (lstat(_matches[0], &attr) != -1) {
			if ((attr.st_mode & S_IFMT) == S_IFDIR) {
				append_slash = 1;
				suggestion.filetype = DT_DIR;
			}
			if (suggest_filetype_color)
				color = get_comp_color(_matches[0], attr);
		} else {
			/* We have a partial completion. Set filetype to DT_DIR
			 * so that the rl_accept_suggestion function won't append
			 * a space after the file name */
			suggestion.filetype = DT_DIR;
		}

		char _tmp[NAME_MAX + 2];
		*_tmp = '\0';
		if (append_slash)
			snprintf(_tmp, NAME_MAX + 2, "%s/", _matches[0]);
		char *tmp = escape_str(*_tmp ? _tmp : _matches[0]);

		if (tmp) {
			print_suggestion(tmp, len, color);
			free(tmp);
		} else {
			print_suggestion(_matches[0], len, color);
		}

		if (c != BS)
			suggestion.type = COMP_SUG;

		printed = 1;
	} else {
		/* If multiple matches, suggest the first one */
		if (_matches[1] && *_matches[1]
		&& strlen(_matches[1]) > len) {
			int append_slash = 0;

			if (lstat(_matches[1], &attr) != -1) {
				if ((attr.st_mode & S_IFMT) == S_IFDIR) {
					append_slash = 1;
					suggestion.filetype = DT_DIR;
				}
				if (suggest_filetype_color)
					color = get_comp_color(_matches[1], attr);
			} else {
				suggestion.filetype = DT_DIR;
			}

			char _tmp[NAME_MAX + 2];
			*_tmp = '\0';
			if (append_slash)
				snprintf(_tmp, NAME_MAX + 2, "%s/", _matches[1]);
			char *tmp = escape_str(*_tmp ? _tmp : _matches[1]);

			if (tmp) {
				print_suggestion(tmp, len, color);
				free(tmp);
			} else {
				print_suggestion(_matches[1], len, color);
			}

			if (c != BS)
				suggestion.type = COMP_SUG;

			printed = 1;
		}
	}

FREE:
	for (i = 0; _matches[i]; i++)
		free(_matches[i]);
	free(_matches);

	if (free_color) {
		free(color);
		free_color = 0;
	}

	return printed;
}

static int
check_filenames(const char *str, const size_t len, const char c, const int first_word)
{
	int i = files;
	char *color = (char *)NULL;
	if (suggest_filetype_color)
		color = no_c;
	else
		color = sf_c;

	while (--i >= 0) {
		if (!file_info[i].name || TOUPPER(*str) != TOUPPER(*file_info[i].name))
			continue;
		if (len && (case_sens_path_comp	? strncmp(str, file_info[i].name, len)
		: strncasecmp(str, file_info[i].name, len)) == 0
		&& file_info[i].len > len) {
			if (suggest_filetype_color)
				color = file_info[i].color;
			if (file_info[i].dir) {
				if (first_word && !autocd)
					continue;
				suggestion.filetype = DT_DIR;
				char tmp[NAME_MAX + 2];
				snprintf(tmp, NAME_MAX + 2, "%s/", file_info[i].name);
				char *_tmp = escape_str(tmp);
				if (_tmp) {
					print_suggestion(_tmp, len, color);
					free(_tmp);
				} else {
					print_suggestion(tmp, len, color);
				}
			} else {
				if (first_word && !auto_open)
					continue;
				suggestion.filetype = DT_REG;
				char *tmp = escape_str(file_info[i].name);
				if (tmp) {
					print_suggestion(tmp, len, color);
					free(tmp);
				} else {
					print_suggestion(file_info[i].name, len, color);
				}
			}
			if (c != BS)
				suggestion.type = FILE_SUG;
			return 1;
		}
	}

	return 0;
}

static int
check_history(const char *str, const size_t len)
{
	if (!str || !*str)
		return 0;
	int i = current_hist_n;

	while (--i >= 0) {
		/* Try to suggest only useful entries */
		if (!history[i] || TOUPPER(*str) != TOUPPER(*history[i]))
			continue;
		char *ret = strrchr(history[i], ' ');
		if (!ret) { /* No space */
			if (*history[i] != '/') /* And no absolute path */
				continue;
			else if (!autocd && !auto_open)
				continue;
		} else if (*(++ret)) {
			if (*ret == '&') { /* 'entry &' */
				continue;
			}
		} else { /* Space is last char */
			continue;
		}

		if (len && (case_sens_path_comp ? strncmp(str, history[i], len)
		: strncasecmp(str, history[i], len)) == 0
		&& strlen(history[i]) > len) {
			print_suggestion(history[i], len, sh_c);
			suggestion.type = HIST_SUG;
			return 1;
		}
	}

	return 0;
}

static int
check_cmds(const char *str, const size_t len)
{
	int i = path_progsn;

	while (--i >= 0) {
		if (!bin_commands[i] || *str != *bin_commands[i])
			continue;
		if (len && strncmp(str, bin_commands[i], len) == 0
		&& strlen(bin_commands[i]) > len) {
			if (is_internal_c(bin_commands[i]))
				print_suggestion(bin_commands[i], len, sx_c);
			else if (ext_cmd_ok)
				print_suggestion(bin_commands[i], len, sc_c);
			else
				continue;
			suggestion.type = CMD_SUG;
			return 1;
		}
	}

	return 0;
}

static int
check_jumpdb(const char *str, const size_t len)
{
	int i = jump_n;
	char *color = (char *)NULL;
	if (suggest_filetype_color)
		color = di_c;
	else
		color = sf_c;

	while (--i >= 0) {
		if (!jump_db[i].path || *str != *jump_db[i].path)
			continue;
		if (len && strncmp(str, jump_db[i].path, len) == 0
		&& strlen(jump_db[i].path) > len) {
			print_suggestion(jump_db[i].path, len, color);
			suggestion.type = FILE_SUG;
			return 1;
		}
	}

	return 0;
}

static int
check_int_params(const char *str, const size_t len)
{
	size_t i;
	for (i = 0; PARAM_STR[i]; i++) {
		if (*str != *PARAM_STR[i])
			continue;
		if (len && strncmp(str, PARAM_STR[i], len) == 0
		&& strlen(PARAM_STR[i]) > len) {
			print_suggestion(PARAM_STR[i], len, sx_c);
			suggestion.type = INT_CMD;
			return 1;
		}
	}

	return 0;
}

/* Check for available suggestionse. Returns zero if true, one if not,
 * and -1 if C was inserted before the end of the current line.
 * If a suggestion is found, it will be printed by print_suggestion() */
int
rl_suggestions(const char c)
{
	char *last_word = (char *)NULL;
	char *full_line = (char *)NULL;
	int printed = 0;
	int inserted_c = 0;

		/* ######################################
		 * # 		  1) Filter input			#
		 * ######################################*/

	/* Skip escape sequences, mostly arrow keys */
	if (rl_readline_state & RL_STATE_MOREINPUT) {
		/* Handle history events. If a suggestion has been printed and
		 * a history event is triggered (usually via the Up and Down arrow
		 * keys), the suggestion buffer won't be freed. Let's do it
		 * here */
		if ((c == 'A' || c == 'B') && suggestion_buf) {
			clear_suggestion();
			goto FAIL;
		}
		if (suggestion_buf) {
			printed = 1;
			goto SUCCESS;
		}
		goto FAIL;
	}

	/* Skip backspace, Enter, and TAB keys */
	switch(c) {
		case BS:
			if (suggestion.printed && suggestion_buf)
				clear_suggestion();
			goto FAIL;

		case ENTER:
			goto FAIL;

		case _ESC: /* fallthrough */
		case _TAB:
			if (suggestion.printed)
				printed = 1;
			goto SUCCESS;

		default: break;
	}

		/* ######################################
		 * #	2) Handle last entered char		#
		 * ######################################*/

	/* If not at the end of line, insert C in the current cursor
	 * position. Else, append it to current readline buffer to
	 * correctly find matches: at this point (rl_getc), readline has
	 * not appended this char to rl_line_buffer yet, so that we must
	 * do it manually. Line editing is only allowed for the last word */
	int s = strcntchrlst(rl_line_buffer, ' ');
	if (rl_point != rl_end && rl_point > s && c != _ESC) {
		char text[2];
		text[0] = c;
		text[1] = '\0';
		rl_insert_text(text);
		/* This flag is used to tell my_rl_getc not to append C to the
		 * line buffer (rl_line_buffer), since it was already inserted
		 * here */
		inserted_c = 1;
	}

	size_t buflen = strlen(rl_line_buffer);
	suggestion.full_line_len = buflen + 1;
	char *last_space = strrchr(rl_line_buffer, ' ');
	if (last_space && last_space != rl_line_buffer
	&& *(last_space - 1) == '\\')
		last_space = (char *)NULL;

	/* We need a copy of the complete line */
	full_line = (char *)xnmalloc(buflen + 2, sizeof(char));
	if (inserted_c)
		strcpy(full_line, rl_line_buffer);
	else
		sprintf(full_line, "%s%c", rl_line_buffer, c);

	/* And a copy of the last entered word as well */
	int last_word_offset = 0;

	if (!last_space) {
		last_space = (char *)NULL;
	} else {
		int j = buflen;
		while (--j >= 0) {
			if (rl_line_buffer[j] == ' ')
				break;
		}
		last_word_offset = j + 1;
		buflen = strlen(last_space);
	}

	if (last_space) {
		if (*(++last_space)) {
			buflen = strlen(last_space);
			last_word = (char *)xnmalloc(buflen + 2, sizeof(char));
			if (inserted_c)
				strcpy(last_word, last_space);
			else
				sprintf(last_word, "%s%c", last_space, c);
		} else {
			last_word = (char *)xnmalloc(2, sizeof(char));
			if (inserted_c) {
				*last_word = '\0';
			} else {
				*last_word = c;
				last_word[1] = '\0';
			}
		}
	} else {
		last_word = (char *)xnmalloc(buflen + 2, sizeof(char));
		if (inserted_c)
			strcpy(last_word, rl_line_buffer);
		else
			sprintf(last_word, "%s%c", rl_line_buffer, c);
	}

		/* ######################################
		 * #	  3) Search for suggestions		#
		 * ######################################*/

/*	suggestion.filetype = DT_NONE; */

	/* 3.a) Check already suggested string */
	if (suggestion_buf && suggestion.printed
	&& strncmp(full_line, suggestion_buf, strlen(full_line)) == 0) {
		printed = 1;
		suggestion.offset = 0;
		free(full_line);
		goto SUCCESS;
	}

	/* 3.b) Check CliFM internal parameters */
	/* 3.b.1) Suggest the sel keyword only if not first word */
	char *ret = strchr(full_line, ' ');
	if (ret) {
		size_t len = strlen(last_word);
		if (*last_word == 's' && strncmp(last_word, "sel", len) == 0) {
			print_suggestion("sel", len, sx_c);
			suggestion.offset = last_word_offset;
			suggestion.type = CMD_SUG;
			printed = 1;
			free(full_line);
			goto SUCCESS;
		}
	}

	/* 3.b.2) Check commands fixed parameters */
	if (ret) {
		printed = check_int_params(full_line, strlen(full_line));
		if (printed) {
			suggestion.offset = 0;
			free(full_line);
			goto SUCCESS;
		}
	}

	/* 3.c) Check commands history */
	printed = check_history(full_line, strlen(full_line));
	free(full_line);
	if (printed) {
		suggestion.offset = 0;
		goto SUCCESS;
	}

	/* Do not check dirs and filenames if first word and neither autocd
	 * nor auto-open are enabled */
	if (last_space || autocd || auto_open) {
		/* 2.d) Check file names in CWD */
		printed = check_filenames(last_word, strlen(last_word), c,
					last_space ? 0 : 1);
		if (printed) {
			suggestion.offset = last_word_offset;
			goto SUCCESS;
		}

		/* 3.e) Check the jump database */
		/* We don't care about auto-open here: the jump function
		 * deals with directories only */
		if (last_space || autocd) {
			printed = check_jumpdb(last_word, strlen(last_word));
			if (printed) {
				suggestion.offset = last_word_offset;
				goto SUCCESS;
			}
		}

		/* 3.f) Check possible completions */
		printed = check_completions(last_word, strlen(last_word), c);
		if (printed) {
			suggestion.offset = last_word_offset;
			goto SUCCESS;
		}
	}

	/* 3.g) Check commands in PATH and CliFM internals commands, but
	 * only for the first word */
	if (!last_space)
		printed = check_cmds(last_word, strlen(last_word));
	if (printed) {
		suggestion.offset = 0;
		goto SUCCESS;
	}

		/* ######################################
		 * # 	  4) No suggestion found		#
		 * ######################################*/

	/* Clear current suggestion, if any, only if no escape char is contained
	 * in the current input sequence. This is mainly to avoid erasing
	 * the suggestion if moving thought the text via the arrow keys */
	if (suggestion.printed) {
		if (!strchr(last_word, '\x1b')) {
			clear_suggestion();
			goto FAIL;
		} else {
			/* Go to SUCCESS so that we don't remove the suggestion
			 * buffer */
			printed = 1;
			goto SUCCESS;
		}
	}

SUCCESS:
	if (printed)
		suggestion.printed = 1;
	else
		suggestion.printed = 0;
	free(last_word);
	if (inserted_c)
		return -1;
	return EXIT_SUCCESS;

FAIL:
	suggestion.printed = 0;
	free(last_word);
	free(suggestion_buf);
	suggestion_buf = (char *)NULL;
	if (inserted_c)
		return -1;
	return EXIT_FAILURE;
}
