/* history.c -- functions for the history system */

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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <readline/history.h>

#include "aux.h"
#include "checks.h"
#include "exec.h"
#include "history.h"
#include "init.h"
#include "misc.h"
#include "messages.h"

/* Log COMM into LOG_FILE (global) */
int
log_function(char **comm)
{
	/* If cmd logs are disabled, allow only "log" commands */
	if (!logs_enabled) {
		if (strcmp(comm[0], "log") != 0)
			return EXIT_SUCCESS;
	}

	if (!config_ok)
		return EXIT_FAILURE;

	int clear_log = 0;

	/* If the command was just 'log' */
	if (*comm[0] == 'l' && strcmp(comm[0], "log") == 0 && !comm[1]) {

		FILE *log_fp;
		log_fp = fopen(LOG_FILE, "r");

		if (!log_fp) {
			_err(0, NOPRINT_PROMPT, "%s: log: '%s': %s\n",
			    PROGRAM_NAME, LOG_FILE, strerror(errno));
			return EXIT_FAILURE;
		} else {
			size_t line_size = 0;
			char *line_buff = (char *)NULL;
			ssize_t line_len = 0;

			while ((line_len = getline(&line_buff, &line_size, log_fp)) > 0)
				fputs(line_buff, stdout);

			free(line_buff);

			fclose(log_fp);
			return EXIT_SUCCESS;
		}
	}

	else if (*comm[0] == 'l' && strcmp(comm[0], "log") == 0 && comm[1]) {

		if (*comm[1] == 'c' && strcmp(comm[1], "clear") == 0)
			clear_log = 1;

		else if (*comm[1] == 's' && strcmp(comm[1], "status") == 0) {
			printf(_("Logs %s\n"), (logs_enabled) ? _("enabled")
							      : _("disabled"));
			return EXIT_SUCCESS;
		}

		else if (*comm[1] == 'o' && strcmp(comm[1], "on") == 0) {

			if (logs_enabled) {
				puts(_("Logs already enabled"));
			} else {
				logs_enabled = 1;
				puts(_("Logs successfully enabled"));
			}

			return EXIT_SUCCESS;
		}

		else if (*comm[1] == 'o' && strcmp(comm[1], "off") == 0) {
			/* If logs were already disabled, just exit. Otherwise, log
			 * the "log off" command */
			if (!logs_enabled) {
				puts(_("Logs already disabled"));
				return EXIT_SUCCESS;
			} else {
				puts(_("Logs succesfully disabled"));
				logs_enabled = 0;
			}
		}
	}

	/* Construct the log line */
	if (!last_cmd) {
		if (!logs_enabled) {
			/* When cmd logs are disabled, "log clear" and "log off" are
			 * the only commands that can reach this code */

			if (clear_log) {
				last_cmd = (char *)xnmalloc(10, sizeof(char));
				strcpy(last_cmd, "log clear");
			} else {
				last_cmd = (char *)xnmalloc(8, sizeof(char));
				strcpy(last_cmd, "log off");
			}
		}
		/* last_cmd should never be NULL if logs are enabled (this
		 * variable is set immediately after taking valid user input
		 * in the prompt function). However ... */

		else {
			last_cmd = (char *)xnmalloc(23, sizeof(char));
			strcpy(last_cmd, _("Error getting command!"));
		}
	}

	char *date = get_date();
	size_t log_len = strlen(date) + strlen(ws[cur_ws].path)
					+ strlen(last_cmd) + 6;
	char *full_log = (char *)xnmalloc(log_len, sizeof(char));

	snprintf(full_log, log_len, "[%s] %s:%s\n", date, ws[cur_ws].path, last_cmd);

	free(date);

	free(last_cmd);
	last_cmd = (char *)NULL;

	/* Write the log into LOG_FILE */
	FILE *log_fp;
	/* If not 'log clear', append the log to the existing logs */

	if (!clear_log)
		log_fp = fopen(LOG_FILE, "a");

	/* Else, overwrite the log file leaving only the 'log clear'
	 * command */
	else
		log_fp = fopen(LOG_FILE, "w+");

	if (!log_fp) {
		_err('e', PRINT_PROMPT, "%s: log: '%s': %s\n", PROGRAM_NAME,
		    LOG_FILE, strerror(errno));
		free(full_log);
		return EXIT_FAILURE;
	} else { /* If LOG_FILE was correctly opened, write the log */
		fputs(full_log, log_fp);
		free(full_log);
		fclose(log_fp);

		return EXIT_SUCCESS;
	}
}

/* Handle the error message MSG. Store MSG in an array of error
 * messages, write it into an error log file, and print it immediately
 * (if PRINT is zero (NOPRINT_PROMPT) or tell the next prompt, if PRINT
 * is one to do it (PRINT_PROMPT)). Messages wrote to the error log file
 * have the following format:
 * "[date] msg", where 'date' is YYYY-MM-DDTHH:MM:SS */
void
log_msg(char *_msg, int print)
{
	if (!_msg)
		return;

	size_t msg_len = strlen(_msg);
	if (msg_len == 0)
		return;

	/* Store messages (for current session only) in an array, so that
	 * the user can check them via the 'msg' command */
	msgs_n++;
	messages = (char **)xrealloc(messages, (size_t)(msgs_n + 1) * sizeof(char *));
	messages[msgs_n - 1] = savestring(_msg, msg_len);
	messages[msgs_n] = (char *)NULL;

	if (print) /* PRINT_PROMPT */
		/* The next prompt will take care of printing the message */
		print_msg = 1;
	else /* NOPRINT_PROMPT */
		/* Print the message directly here */
		fputs(_msg, stderr);

	/* If the config dir cannot be found or if msg log file isn't set
	 * yet... This will happen if an error occurs before running
	 * init_config(), for example, if the user's home cannot be found */
	if (!config_ok || !MSG_LOG_FILE || !*MSG_LOG_FILE)
		return;

	FILE *msg_fp = fopen(MSG_LOG_FILE, "a");

	if (!msg_fp) {
		/* Do not log this error: We might incur in an infinite loop
		 * trying to access a file that cannot be accessed */
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, MSG_LOG_FILE,
		    strerror(errno));
		fputs("Press any key to continue... ", stdout);
		xgetchar();
		putchar('\n');
	} else {
		/* Write message to messages file: [date] msg */
		time_t rawtime = time(NULL);
		struct tm tm;
		localtime_r(&rawtime, &tm);
		char date[64] = "";

		strftime(date, sizeof(date), "%b %d %H:%M:%S %Y", &tm);
		fprintf(msg_fp, "[%d-%d-%dT%d:%d:%d] ", tm.tm_year + 1900,
		    tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
		fputs(_msg, msg_fp);
		fclose(msg_fp);
	}
}

int
save_dirhist(void)
{
	if (!DIRHIST_FILE)
		return EXIT_FAILURE;

	if (!old_pwd || !old_pwd[0])
		return EXIT_SUCCESS;

	FILE *fp = fopen(DIRHIST_FILE, "w");

	if (!fp) {
		fprintf(stderr, _("%s: Could not save directory history: %s\n"),
		    PROGRAM_NAME, strerror(errno));
		return EXIT_FAILURE;
	}

	size_t i;

	for (i = 0; old_pwd[i]; i++)
		fprintf(fp, "%s\n", old_pwd[i]);

	fclose(fp);

	return EXIT_SUCCESS;
}

/* Add DIR_PATH to visited directory history (old_pwd) */
void
add_to_dirhist(const char *dir_path)
{
	/*  static size_t end_counter = 11, mid_counter = 11; */

	/* If already at the end of dirhist, add new entry */
	if (dirhist_cur_index + 1 >= dirhist_total_index) {

		/* Do not add anything if new path equals last entry in
		 * directory history */
		if ((dirhist_total_index - 1) >= 0 && old_pwd[dirhist_total_index - 1] && *(dir_path + 1) == *(old_pwd[dirhist_total_index - 1] + 1) && strcmp(dir_path, old_pwd[dirhist_total_index - 1]) == 0)
			return;

		/* Realloc only once per 10 operations */
		/*      if (end_counter > 10) {
			end_counter = 1; */
		/* 20: Realloc dirhist_total + (2 * 10) */
		old_pwd = (char **)xrealloc(old_pwd,
		    (size_t)(dirhist_total_index + 2) * sizeof(char *));
		/*      }

		end_counter++; */

		dirhist_cur_index = dirhist_total_index;
		old_pwd[dirhist_total_index++] = savestring(dir_path, strlen(dir_path));
		old_pwd[dirhist_total_index] = (char *)NULL;
	}

	/* I not at the end of dirhist, add previous AND new entry */
	else {
		/*      if (mid_counter > 10) {
			mid_counter = 1; */
		/* 30: Realloc dirhist_total + (3 * 10) */
		old_pwd = (char **)xrealloc(old_pwd,
		    (size_t)(dirhist_total_index + 3) * sizeof(char *));
		/*      }

		mid_counter++; */

		old_pwd[dirhist_total_index++] = savestring(
		    old_pwd[dirhist_cur_index],
		    strlen(old_pwd[dirhist_cur_index]));

		dirhist_cur_index = dirhist_total_index;
		old_pwd[dirhist_total_index++] = savestring(dir_path, strlen(dir_path));

		old_pwd[dirhist_total_index] = (char *)NULL;
	}
}

int
history_function(char **comm)
{
	if (!config_ok) {
		fprintf(stderr, _("%s: History function disabled\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	/* If no arguments, print the history list */
	if (args_n == 0) {
		size_t i;
		for (i = 0; i < current_hist_n; i++)
			printf("%zu %s\n", i + 1, history[i]);
		return EXIT_SUCCESS;
	}

	/* If 'history clear', guess what, clear the history list! */
	if (args_n == 1 && strcmp(comm[1], "clear") == 0) {
		FILE *hist_fp = fopen(HIST_FILE, "w+");

		if (!hist_fp) {
			_err(0, NOPRINT_PROMPT, "%s: history: %s: %s\n",
			    PROGRAM_NAME, HIST_FILE, strerror(errno));
			return EXIT_FAILURE;
		}

		/* Do not create an empty file */
		fprintf(hist_fp, "%s %s\n", comm[0], comm[1]);
		fclose(hist_fp);

		/* Reset readline history */
		clear_history();
		read_history(HIST_FILE);
		history_truncate_file(HIST_FILE, max_hist);

		/* Update the history array */
		int exit_status = EXIT_SUCCESS;

		if (get_history() != 0)
			exit_status = EXIT_FAILURE;

		if (log_function(comm) != 0)
			exit_code = EXIT_FAILURE;

		return exit_status;
	}

	/* If 'history -n', print the last -n elements */
	if (args_n == 1 && comm[1][0] == '-' && is_number(comm[1] + 1)) {
		int num = atoi(comm[1] + 1);

		if (num < 0 || num > (int)current_hist_n)
			num = (int)current_hist_n;

		size_t i;

		for (i = current_hist_n - (size_t)num; i < current_hist_n; i++)
			printf("%zu %s\n", i + 1, history[i]);

		return EXIT_SUCCESS;
	}

	/* None of the above */
	puts(_(HISTORY_USAGE));

	return EXIT_SUCCESS;
}

/* Takes as argument the history cmd less the first exclamation mark.
 * Example: if exec_cmd() gets "!-10" it pass to this function "-10",
 * that is, comm + 1 */
int
run_history_cmd(const char *cmd)
{
	/* If "!n" */
	int exit_status = EXIT_SUCCESS;
	size_t i;

	if (is_number(cmd)) {
		int num = atoi(cmd);

		if (num <= 0 || num >= (int)current_hist_n) {
			fprintf(stderr, _("%s: !%d: event not found\n"), PROGRAM_NAME, num);
			return EXIT_FAILURE;
		}

		size_t old_args = args_n;

		if (record_cmd(history[num - 1]))
			add_to_cmdhist(history[num - 1]);

		char **cmd_hist = parse_input_str(history[num - 1]);

		if (!cmd_hist) {
			fprintf(stderr, _("%s: Error parsing history command\n"),
				PROGRAM_NAME);
			return EXIT_FAILURE;
		}

		char **alias_cmd = check_for_alias(cmd_hist);

		if (alias_cmd) {
			/* If an alias is found, the function frees cmd_hist
			 * and returns alias_cmd in its place to be executed
			 * by exec_cmd() */

			if (exec_cmd(alias_cmd) != 0)
				exit_status = EXIT_FAILURE;

			for (i = 0; alias_cmd[i]; i++)
				free(alias_cmd[i]);

			free(alias_cmd);

			alias_cmd = (char **)NULL;
		} else {
			if (exec_cmd(cmd_hist) != 0)
				exit_status = EXIT_FAILURE;

			for (i = 0; cmd_hist[i]; i++)
				free(cmd_hist[i]);

			free(cmd_hist);
		}

		args_n = old_args;

		return exit_status;
	}

	/* If "!!", execute the last command */
	if (*cmd == '!' && !cmd[1]) {
		size_t old_args = args_n;

		if (record_cmd(history[current_hist_n - 1]))
			add_to_cmdhist(history[current_hist_n - 1]);

		char **cmd_hist = parse_input_str(history[current_hist_n - 1]);

		if (!cmd_hist) {
			fprintf(stderr, _("%s: Error parsing history command\n"),
				PROGRAM_NAME);
			return EXIT_FAILURE;
		}

		char **alias_cmd = check_for_alias(cmd_hist);

		if (alias_cmd) {

			if (exec_cmd(alias_cmd) != 0)
				exit_status = EXIT_FAILURE;

			for (i = 0; alias_cmd[i]; i++)
				free(alias_cmd[i]);

			free(alias_cmd);

			alias_cmd = (char **)NULL;
		} else {
			if (exec_cmd(cmd_hist) != 0)
				exit_status = EXIT_FAILURE;

			for (i = 0; cmd_hist[i]; i++)
				free(cmd_hist[i]);

			free(cmd_hist);
		}

		args_n = old_args;
		return exit_status;
	}

	/* If "!-n" */
	if (*cmd == '-') {
		/* If not number or zero or bigger than max... */
		int acmd = atoi(cmd + 1);

		if (!is_number(cmd + 1) || acmd == 0 || acmd > (int)current_hist_n - 1) {
			fprintf(stderr, _("%s: !%s: Event not found\n"), PROGRAM_NAME, cmd);
			return EXIT_FAILURE;
		}

		size_t old_args = args_n;
		char **cmd_hist = parse_input_str(history[current_hist_n - (size_t)acmd - 1]);

		if (cmd_hist) {
			char **alias_cmd = check_for_alias(cmd_hist);

			if (alias_cmd) {
				if (exec_cmd(alias_cmd) != 0)
					exit_status = EXIT_FAILURE;

				for (i = 0; alias_cmd[i]; i++)
					free(alias_cmd[i]);

				free(alias_cmd);

				alias_cmd = (char **)NULL;
			} else {
				if (exec_cmd(cmd_hist) != 0)
					exit_status = EXIT_FAILURE;

				for (i = 0; cmd_hist[i]; i++)
					free(cmd_hist[i]);

				free(cmd_hist);
			}

			if (record_cmd(history[current_hist_n - (size_t)acmd - 1]))
				add_to_cmdhist(history[current_hist_n - (size_t)acmd - 1]);

			args_n = old_args;
			return exit_status;
		}

		if (record_cmd(history[current_hist_n - (size_t)acmd - 1]))
			add_to_cmdhist(history[current_hist_n - (size_t)acmd - 1]);

		fprintf(stderr, _("%s: Error parsing history command\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	if ((*cmd >= 'a' && *cmd <= 'z') || (*cmd >= 'A' && *cmd <= 'Z')) {

		size_t len = strlen(cmd),
				old_args = args_n;

		for (i = 0; history[i]; i++) {

			if (*cmd == *history[i] && strncmp(cmd, history[i], len) == 0) {
				char **cmd_hist = parse_input_str(history[i]);

				if (!cmd_hist)
					continue;

				char **alias_cmd = check_for_alias(cmd_hist);

				if (alias_cmd) {

					if (exec_cmd(alias_cmd) != EXIT_SUCCESS)
						exit_status = EXIT_FAILURE;

					for (i = 0; alias_cmd[i]; i++)
						free(alias_cmd[i]);

					free(alias_cmd);

					alias_cmd = (char **)NULL;
				} else {
					if (exec_cmd(cmd_hist) != EXIT_SUCCESS)
						exit_status = EXIT_FAILURE;

					for (i = 0; cmd_hist[i]; i++)
						free(cmd_hist[i]);

					free(cmd_hist);
				}

				args_n = old_args;
				return exit_status;
			}
		}

		fprintf(stderr, _("%s: !%s: Event not found\n"), PROGRAM_NAME, cmd);

		return EXIT_FAILURE;
	}

	puts(_(HISTEXEC_USAGE));
		return EXIT_SUCCESS;
}

int
get_history(void)
{
	if (!config_ok)
		return EXIT_FAILURE;

	if (current_hist_n == 0) /* Coming from main() */
		history = (char **)xcalloc(1, sizeof(char *));

	else { /* Only true when comming from 'history clear' */
		size_t i;

		for (i = 0; history[i]; i++)
			free(history[i]);

		history = (char **)xrealloc(history, 1 * sizeof(char *));
		current_hist_n = 0;
	}

	FILE *hist_fp = fopen(HIST_FILE, "r");

	if (!hist_fp) {
		_err('e', PRINT_PROMPT, "%s: history: '%s': %s\n",
		    PROGRAM_NAME, HIST_FILE, strerror(errno));
		return EXIT_FAILURE;
	}

	size_t line_size = 0;
	char *line_buff = (char *)NULL;
	ssize_t line_len = 0;

	while ((line_len = getline(&line_buff, &line_size, hist_fp)) > 0) {

		line_buff[line_len - 1] = '\0';

		history = (char **)xrealloc(history, (current_hist_n + 2) * sizeof(char *));
		history[current_hist_n++] = savestring(line_buff, (size_t)line_len);
	}

	history[current_hist_n] = (char *)NULL;

	free(line_buff);
	fclose(hist_fp);

	return EXIT_SUCCESS;
}

void
add_to_cmdhist(const char *cmd)
{
	if (!cmd)
		return;

	/* For readline */
	add_history(cmd);

	if (config_ok)
		append_history(1, HIST_FILE);

	/* For us */
	/* Add the new input to the history array */
	size_t cmd_len = strlen(cmd);
	history = (char **)xrealloc(history, (size_t)(current_hist_n + 2) * sizeof(char *));
	history[current_hist_n++] = savestring(cmd, cmd_len);

	history[current_hist_n] = (char *)NULL;
}

/* Returns 1 if INPUT should be stored in history and 0 if not */
int
record_cmd(char *input)
{
	/* NULL input */
	if (!input)
		return 0;

	/* Blank lines */
	unsigned int blank = 1;
	char *p = input;

	while (*p) {
		if (*p > ' ') {
			blank = 0;
			break;
		}
		p++;
	}

	if (blank)
		return 0;

	/* Rewind the pointer to the beginning of the input line */
	p = input;

	/* Commands starting with space */
	if (*p == ' ')
		return 0;

	/* Exit commands */
	switch (*p) {

	case 'q':
		if (*(p + 1) == '\0' || strcmp(p, "quit") == 0)
			return 0;
		break;

	case 'Q':
		if (*(p + 1) == '\0')
			return 0;
		break;

	case 'e':
		if (*(p + 1) == 'x' && strcmp(p, "exit") == 0)
			return 0;
		break;

	case 'z':
		if (*(p + 1) == 'z' && *(p + 2) == '\0')
			return 0;
		break;

	case 's':
		if (*(p + 1) == 'a' && strcmp(p, "salir") == 0)
			return 0;
		break;

	case 'c':
		if (*(p + 1) == 'h' && strcmp(p, "chau") == 0)
			return 0;
		break;

	default:
		break;
	}

	/* History */
	if (*p == '!' && (_ISDIGIT(*(p + 1)) || (*(p + 1) == '-'
	&& _ISDIGIT(*(p + 2))) || ((*(p + 1) == '!') && *(p + 2) == '\0')))
		return 0;

	/* Consequtively equal commands in history */
	if (history && history[current_hist_n - 1]
	&& *p == *history[current_hist_n - 1]
	&& strcmp(p, history[current_hist_n - 1]) == 0)
		return 0;

	return 1;
}
