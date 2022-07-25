/* history.c -- functions for the history system */

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
#include "file_operations.h"

/* Print available logs */
static int
print_logs(void)
{
	FILE *log_fp = fopen(log_file, "r");
	if (!log_fp) {
		_err(0, NOPRINT_PROMPT, "log: %s: %s\n", log_file, strerror(errno));
		return EXIT_FAILURE;
	}

	size_t line_size = 0;
	char *line_buff = (char *)NULL;

	while (getline(&line_buff, &line_size, log_fp) > 0)
		fputs(line_buff, stdout);

	free(line_buff);
	fclose(log_fp);
	return EXIT_SUCCESS;
}

/* Log CMD into LOG_FILE (global) */
int
log_function(char **cmd)
{
	if (xargs.stealth_mode == 1)
		return EXIT_SUCCESS;

	/* If cmd logs are disabled, allow only "log" commands */
	if (log_cmds == 0) {
		if (cmd && cmd[0] && strcmp(cmd[0], "log") != 0)
			return EXIT_SUCCESS;
	}

	if (config_ok == 0 || !log_file)
		return EXIT_FAILURE;

	int clear_log = 0;

	/* If the command was just 'log' */
	if (cmd && cmd[0] && *cmd[0] == 'l' && strcmp(cmd[0], "log") == 0 && !cmd[1])
		return print_logs();

	else if (cmd && cmd[0] && *cmd[0] == 'l' && strcmp(cmd[0], "log") == 0
	&& cmd[1]) {
		if (*cmd[1] == 'c' && strcmp(cmd[1], "clear") == 0) {
			clear_log = 1;
		} else if (*cmd[1] == 's' && strcmp(cmd[1], "status") == 0) {
			printf(_("Logs %s\n"), (logs_enabled == 1) ? _("enabled") : _("disabled"));
			return EXIT_SUCCESS;
		} else if (*cmd[1] == 'o' && strcmp(cmd[1], "on") == 0) {
			if (logs_enabled == 1) {
				puts(_("Logs already enabled"));
			} else {
				logs_enabled = 1;
				puts(_("Logs successfully enabled"));
			}
			return EXIT_SUCCESS;
		} else if (*cmd[1] == 'o' && strcmp(cmd[1], "off") == 0) {
			/* If logs were already disabled, just exit. Otherwise, log
			 * the "log off" command */
			if (logs_enabled == 0) {
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
		if (log_cmds == 0) {
			/* When cmd logs are disabled, "log clear" and "log off" are
			 * the only commands that can reach this code */
			if (clear_log == 1) {
				last_cmd = (char *)xnmalloc(10, sizeof(char));
				strcpy(last_cmd, "log clear");
			} else {
				last_cmd = (char *)xnmalloc(8, sizeof(char));
				strcpy(last_cmd, "log off");
			}
		} else {
		/* last_cmd should never be NULL if logs are enabled (this
		 * variable is set immediately after taking valid user input
		 * in the prompt function). However ... */
			last_cmd = (char *)xnmalloc(23, sizeof(char));
			strcpy(last_cmd, _("Error getting command!"));
		}
	}

	char *date = get_date();

	size_t log_len = strlen(date)
		+ (workspaces[cur_ws].path ? strlen(workspaces[cur_ws].path) : 2)
		+ strlen(last_cmd) + 8;

	char *full_log = (char *)xnmalloc(log_len, sizeof(char));
	snprintf(full_log, log_len, "c:[%s] %s:%s\n", date,
		workspaces[cur_ws].path ? workspaces[cur_ws].path : "?", last_cmd);

	free(date);
	free(last_cmd);
	last_cmd = (char *)NULL;

	/* Write the log into LOG_FILE */
	FILE *log_fp;

	if (clear_log == 0) /* Append the log to the existing logs */
		log_fp = fopen(log_file, "a");
	else /* Overwrite the log file leaving only the 'log clear' command */
		log_fp = fopen(log_file, "w+");

	if (!log_fp) {
		_err('e', PRINT_PROMPT, "log: %s: %s\n", log_file, strerror(errno));
		free(full_log);
		return EXIT_FAILURE;
	}

	fputs(full_log, log_fp);
	free(full_log);
	fclose(log_fp);

	return EXIT_SUCCESS;
}

/* Write _MSG into the log file: [date] _MSG */
static void
write_msg_into_logfile(const char *_msg)
{
	FILE *msg_fp = fopen(log_file, "a");
	if (!msg_fp) {
		/* Do not log this error: We might enter into an infinite loop
		 * trying to access a file that cannot be accessed. Just warn the user
		 * and print the error to STDERR */
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, log_file, strerror(errno));
		fputs("Press any key to continue... ", stdout);
		xgetchar();
		putchar('\n');
		return;
	}

	char *date = get_date();
	fprintf(msg_fp, "m:[%s] %s", date, _msg);
	fclose(msg_fp);
	free(date);
}

/* Let's send a desktop notification */
static void
send_desktop_notification(char *msg)
{
/*	if (!msg || !*msg || !(flags & GUI) || desktop_noti != 1) */
	if (!msg || !*msg)
		return;

	char type[12];
	*type = '\0';
	switch(pmsg) {
#if defined(__HAIKU__)
	case ERROR: snprintf(type, sizeof(type), "error"); break;
	case WARNING: snprintf(type, sizeof(type), "important"); break;
	case NOTICE: snprintf(type, sizeof(type), "information"); break;
	default: snprintf(type, sizeof(type), "information"); break;
#elif defined(__APPLE__)
	case ERROR: snprintf(type, sizeof(type), "Error"); break;
	case WARNING: snprintf(type, sizeof(type), "Warning"); break;
	case NOTICE: snprintf(type, sizeof(type), "Notice"); break;
	default: snprintf(type, sizeof(type), "Notice"); break;
#else
	case ERROR: snprintf(type, sizeof(type), "critical"); break;
	case WARNING: snprintf(type, sizeof(type), "normal"); break;
	case NOTICE: snprintf(type, sizeof(type), "low"); break;
	default: snprintf(type, sizeof(type), "low"); break;
#endif
	}

	size_t mlen = strlen(msg);
	if (mlen > 0 && msg[mlen - 1] == '\n') {
		msg[mlen - 1] = '\0';
		mlen--;
		if (mlen == 0)
			return;
	}

	/* Some messages are written in the form PROGRAM_NAME: MSG. We only
	 * want the MSG part */
	char name[NAME_MAX];
	snprintf(name, sizeof(name), "%s: ", PROGRAM_NAME);
	char *p = msg;
	size_t nlen = strlen(name);
	int ret = strncmp(p, name, nlen);
	if (ret == 0) {
		if (mlen <= nlen)
			return;
		p = msg + nlen;
	}

#if defined(__HAIKU__)
	char *cmd[] = {"notify", "--type", type, "--title", PROGRAM_NAME, p, NULL};
	ret = launch_execve(cmd, FOREGROUND, E_MUTE);
#elif defined (__APPLE__)
	size_t msg_len = strlen(msg) + strlen(type) + strlen(PROGRAM_NAME) + 60;
	char *_msg = (char *)xnmalloc(msg_len, sizeof(char));
	snprintf(_msg, msg_len, "'display notification \"%s\" subtitle \"%s\" with title \"%s\"'",
		msg, type, PROGRAM_NAME);
	char *cmd[] = {"osascript", "-e", _msg, NULL};
	ret = launch_execve(cmd, FOREGROUND, E_MUTE);
	free(_msg);
#else
	char *cmd[] = {"notify-send", "-u", type, PROGRAM_NAME, p, NULL};
	ret = launch_execve(cmd, FOREGROUND, E_MUTE);
#endif

	if (ret == EXIT_SUCCESS)
		return;

	/* Error: warn and print the original message */
	fprintf(stderr, "%s: Notification daemon error: %s\n"
		"Disable desktop notifications (run 'help desktop-notifications' "
		"for details) or %s to silence this "
		"warning (original message printed below)\n", PROGRAM_NAME,
		strerror(ret), ret == ENOENT ? "install a notification daemon"
		: "fix this error (consult your daemon's documentation)");
	fprintf(stderr, "%s\n", msg);
}

/* Handle the error message MSG.
 *
 * If ADD_TO_MSGS_LIST is 1, store MSG into the messages array: MSG will be
 * accessible to the user via the 'msg' command
 *
 * If PRINT_PROMPT is 1, either raise a flag to tell the next prompt to print
 * the message itself, or, if desktop notifications are enabled and LOGME is
 * not zero (ERR_NO_LOG), send the notification to the notification daemon
 * NOTE: if not zero, LOGME could be either 1 (error/warning) or -1 (notice)
 *
 * If PRINT_PROMPT is not 1, MSG is printed directly here
 *
 * Finally, if logs are enabled and LOGME is 1, write the message into the log
 * file as follows: "[date] msg", where 'date' is YYYY-MM-DDTHH:MM:SS */
void
log_msg(char *_msg, const int print_prompt, const int logme, const int add_to_msgs_list)
{
	if (!_msg)
		return;

	size_t msg_len = strlen(_msg);
	if (msg_len == 0)
		return;

	if (add_to_msgs_list == 1) {
		msgs_n++;
		messages = (char **)xrealloc(messages, (size_t)(msgs_n + 1) * sizeof(char *));
		messages[msgs_n - 1] = savestring(_msg, msg_len);
		messages[msgs_n] = (char *)NULL;
	}

	if (print_prompt == 1) {
		if (desktop_notifications == 1 && logme != 0)
			send_desktop_notification(_msg);
		else
			print_msg = 1;
	} else {
		fputs(_msg, stderr);
	}

	if (xargs.stealth_mode == 1 || config_ok == 0 || !log_file || !*log_file
	|| logme != 1 || logs_enabled == 0)
		return;

	write_msg_into_logfile(_msg);
}

int
save_dirhist(void)
{
	if (!dirhist_file)
		return EXIT_FAILURE;

	if (!old_pwd || !old_pwd[0])
		return EXIT_SUCCESS;

	FILE *fp = fopen(dirhist_file, "w");
	if (!fp) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, _("history: Could not save "
			"directory history: %s\n"), strerror(errno));
		return EXIT_FAILURE;
	}

	int i;
	for (i = 0; i < dirhist_total_index; i++) {
		/* Exclude invalid entries */
		if (!old_pwd[i] || *old_pwd[i] == _ESC)
			continue;
		fprintf(fp, "%s\n", old_pwd[i]);
	}

	fclose(fp);
	return EXIT_SUCCESS;
}

/* Add DIR_PATH to visited directory history (old_pwd) */
void
add_to_dirhist(const char *dir_path)
{
	/* If already at the end of dirhist, add new entry */
	if (dirhist_cur_index + 1 >= dirhist_total_index) {
		/* Do not add anything if new path equals last entry in
		 * directory history */
		if ((dirhist_total_index - 1) >= 0 && old_pwd[dirhist_total_index - 1]
		&& *(dir_path + 1) == *(old_pwd[dirhist_total_index - 1] + 1)
		&& strcmp(dir_path, old_pwd[dirhist_total_index - 1]) == 0)
			return;

		old_pwd = (char **)xrealloc(old_pwd,
		    (size_t)(dirhist_total_index + 2) * sizeof(char *));

		dirhist_cur_index = dirhist_total_index;
		old_pwd[dirhist_total_index] = savestring(dir_path, strlen(dir_path));
		dirhist_total_index++;
		old_pwd[dirhist_total_index] = (char *)NULL;
	}

	/* I not at the end of dirhist, add previous AND new entry */
	else {
		old_pwd = (char **)xrealloc(old_pwd,
		    (size_t)(dirhist_total_index + 3) * sizeof(char *));

		old_pwd[dirhist_total_index] = savestring(
		    old_pwd[dirhist_cur_index],
		    strlen(old_pwd[dirhist_cur_index]));
		dirhist_total_index++;

		dirhist_cur_index = dirhist_total_index;
		old_pwd[dirhist_total_index] = savestring(dir_path, strlen(dir_path));
		dirhist_total_index++;

		old_pwd[dirhist_total_index] = (char *)NULL;
	}
}

static int
reload_history(char **args)
{
	clear_history();
	read_history(hist_file);
	history_truncate_file(hist_file, max_hist);

	/* Update the history array */
	int ret = get_history();

	if (log_function(args) != EXIT_SUCCESS)
		return EXIT_FAILURE;

	return ret;
}

static int
edit_history(char **args)
{
	struct stat attr;
	if (stat(hist_file, &attr) == -1) {
		int err = errno;
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "history: %s: %s\n", hist_file, strerror(errno));
		return err;
	}
	time_t mtime_bfr = (time_t)attr.st_mtime;

	int ret = EXIT_SUCCESS;

	/* If we have an opening application (2nd argument) */
	if (args[2]) {
		char *cmd[] = {args[2], hist_file, NULL};
		ret = launch_execve(cmd, FOREGROUND, E_NOSTDERR);
	} else {
		open_in_foreground = 1;
		ret = open_file(hist_file);
		open_in_foreground = 0;
	}

	if (ret != EXIT_SUCCESS)
		return ret;

	/* Get modification time after opening the config file */
	stat(hist_file, &attr);
	/* If modification times differ, the file was modified after being
	 * opened */
	if (mtime_bfr != (time_t)attr.st_mtime) {
		ret = reload_history(args);
		print_reload_msg("File modified. History entries reloaded\n");
		return ret;
	}

	return EXIT_SUCCESS;
}

static int
__clear_history(char **args)
{
	/* Let's overwrite whatever was there */
	FILE *hist_fp = fopen(hist_file, "w+");
	if (!hist_fp) {
		_err(0, NOPRINT_PROMPT, "history: %s: %s\n", hist_file, strerror(errno));
		return EXIT_FAILURE;
	}

	/* Do not create an empty file */
	fprintf(hist_fp, "%s %s\n", args[0], args[1]);
	fclose(hist_fp);

	/* Reset readline history */
	return reload_history(args);
}

static int
print_history_list(void)
{
	int n = DIGINUM(current_hist_n);
	size_t i;
	for (i = 0; i < current_hist_n; i++)
		printf(" %s%-*zu%s %s\n", el_c, n, i + 1, df_c, history[i].cmd);

	return EXIT_SUCCESS;
}

static int
print_last_items(char *str)
{
	int num = atoi(str);

	if (num < 0 || num > (int)current_hist_n)
		num = (int)current_hist_n;

	int n = DIGINUM(current_hist_n);
	size_t i;
	for (i = current_hist_n - (size_t)num; i < current_hist_n; i++)
		printf(" %s%-*zu%s %s\n", el_c, n, i + 1, df_c, history[i].cmd);

	return EXIT_SUCCESS;
}

static int
print_hist_status(void)
{
	printf(_("History is %s\n"), hist_status == 1 ? "enabled" : "disabled");
	return EXIT_SUCCESS;
}

static int
toggle_history(const char *arg)
{
	if (!arg || !*arg)
		return EXIT_FAILURE;

	switch(*arg) {
	case 'o':
		hist_status = (*(arg + 1) == 'n' ? 1 : 0);
		return print_hist_status();
	case 's': return print_hist_status();
	default: puts(_(HISTORY_USAGE)); return EXIT_FAILURE;
	}
}

int
history_function(char **comm)
{
	if (xargs.stealth_mode == 1) {
		printf(_("%s: history: %s\n"), PROGRAM_NAME, STEALTH_DISABLED);
		return EXIT_SUCCESS;
	}

	if (config_ok == 0) {
		fprintf(stderr, _("%s: History function disabled\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	/* If no arguments, print the history list */
	if (!comm[1])
		return print_history_list();

	if (*comm[1] == 'e' && strcmp(comm[1], "edit") == 0)
		return edit_history(comm);

	if (*comm[1] == 'c' && strcmp(comm[1], "clear") == 0)
		return __clear_history(comm);

	/* If 'history -n', print the last -n elements */
	if (*comm[1] == '-' && is_number(comm[1] + 1))
		return print_last_items(comm[1] + 1);

	if ((*comm[1] == 'o' || *comm[1] == 's') && (strcmp(comm[1], "on") == 0
	|| strcmp(comm[1], "off") == 0 || strcmp(comm[1], "status") == 0))
		return toggle_history(comm[1]);

	/* None of the above */
	puts(_(HISTORY_USAGE));
	return EXIT_SUCCESS;
}

static int
exec_hist_cmd(char **cmd)
{
	int i, exit_status = EXIT_SUCCESS;

	char **alias_cmd = check_for_alias(cmd);
	if (alias_cmd) {
		/* If an alias is found, check_for_alias frees CMD and returns
		 * alias_cmd in its place to be executed by exec_cmd() */
		if (exec_cmd(alias_cmd) != 0)
			exit_status = EXIT_FAILURE;

		for (i = 0; alias_cmd[i]; i++)
			free(alias_cmd[i]);
		free(alias_cmd);
		alias_cmd = (char **)NULL;
	} else {
		if (exec_cmd(cmd) != 0)
			exit_status = EXIT_FAILURE;

		for (i = 0; cmd[i]; i++)
			free(cmd[i]);
		free(cmd);
	}

	return exit_status;
}

/* Run history command number CMD */
static int
run_hist_num(const char *cmd)
{
	size_t old_args = args_n;
	int num = atoi(cmd);

	if (num <= 0 || num > (int)current_hist_n) {
		fprintf(stderr, _("history: !%s: event not found\n"), cmd);
		return EXIT_FAILURE;
	}

	if (record_cmd(history[num - 1].cmd))
		add_to_cmdhist(history[num - 1].cmd);

	char **cmd_hist = parse_input_str(history[num - 1].cmd);
	if (!cmd_hist) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, _("history: Error parsing history command\n"));
		return EXIT_FAILURE;
	}

	int exit_status = exec_hist_cmd(cmd_hist);
	args_n = old_args;
	return exit_status;
}

/* Run the last command in history */
static int
run_last_hist_cmd(void)
{
	size_t old_args = args_n;
	if (record_cmd(history[current_hist_n - 1].cmd))
		add_to_cmdhist(history[current_hist_n - 1].cmd);

	char **cmd_hist = parse_input_str(history[current_hist_n - 1].cmd);
	if (!cmd_hist) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, _("history: Error parsing history command\n"));
		return EXIT_FAILURE;
	}

	int exit_status = exec_hist_cmd(cmd_hist);
	args_n = old_args;
	return exit_status;
}

/* Run history command number "last - N" */
static int
run_last_lessn_hist_cmd(const char *cmd)
{
	size_t old_args = args_n;
	int acmd = atoi(cmd + 1);

	if (!is_number(cmd + 1) || acmd <= 0 || acmd > (int)current_hist_n - 1) {
		fprintf(stderr, _("history: !%s: Event not found\n"), cmd);
		return EXIT_FAILURE;
	}

	size_t n = current_hist_n - (size_t)acmd - 1;

	if (record_cmd(history[n].cmd))
		add_to_cmdhist(history[n].cmd);

	char **cmd_hist = parse_input_str(history[n].cmd);
	if (cmd_hist) {
		int exit_status = exec_hist_cmd(cmd_hist);
		args_n = old_args;
		return exit_status;
	}

	_err(ERR_NO_STORE, NOPRINT_PROMPT, _("history: Error parsing history command\n"));
	return EXIT_FAILURE;
}

/* Run history command matching CMD */
static int
run_hist_string(const char *cmd)
{
	size_t i, len = strlen(cmd), old_args = args_n;

	for (i = 0; history[i].cmd; i++) {
		if (*cmd != *history[i].cmd || strncmp(cmd, history[i].cmd, len) != 0)
			continue;

		char **cmd_hist = parse_input_str(history[i].cmd);
		if (!cmd_hist)
			continue;

		int exit_status = exec_hist_cmd(cmd_hist);
		args_n = old_args;
		return exit_status;
	}

	fprintf(stderr, _("history: !%s: Event not found\n"), cmd);
	return EXIT_FAILURE;
}

/* Takes as argument the history cmd less the first exclamation mark.
 * Example: if exec_cmd() gets "!-10" it pass to this function "-10",
 * that is, comm + 1 */
int
run_history_cmd(const char *cmd)
{
	/* If "!n" */
	if (is_number(cmd))
		return run_hist_num(cmd);

	/* If "!!", execute the last command */
	if (*cmd == '!' && !cmd[1])
		return run_last_hist_cmd();

	/* If "!-n" */
	if (*cmd == '-' && *(cmd + 1) && is_number(cmd + 1))
		return run_last_lessn_hist_cmd(cmd);

	/* If !STRING */
	if ((*cmd >= 'a' && *cmd <= 'z') || (*cmd >= 'A' && *cmd <= 'Z'))
		return run_hist_string(cmd);

	puts(_(HISTEXEC_USAGE));
	return EXIT_SUCCESS;
}

int
get_history(void)
{
	if (!config_ok || !hist_file) return EXIT_FAILURE;

	if (current_hist_n == 0) { /* Coming from main() */
		history = (struct history_t *)xcalloc(1, sizeof(struct history_t));
	} else { /* Only true when comming from 'history clear' */
		size_t i;
		for (i = 0; history[i].cmd; i++)
			free(history[i].cmd);
		history = (struct history_t *)xrealloc(history, 1 * sizeof(struct history_t));
		current_hist_n = 0;
	}

	FILE *hist_fp = fopen(hist_file, "r");
	if (!hist_fp) {
		_err('e', PRINT_PROMPT, "history: %s: %s\n", hist_file, strerror(errno));
		return EXIT_FAILURE;
	}

	size_t line_size = 0;
	char *line_buff = (char *)NULL;
	ssize_t line_len = 0;

	while ((line_len = getline(&line_buff, &line_size, hist_fp)) > 0) {
		line_buff[line_len - 1] = '\0';
		history = (struct history_t *)xrealloc(history, (current_hist_n + 2) * sizeof(struct history_t));
		history[current_hist_n].cmd = savestring(line_buff, (size_t)line_len);
		history[current_hist_n].len = (size_t)line_len;
		current_hist_n++;
	}

	curhistindex = current_hist_n ? current_hist_n - 1 : 0;
	history[current_hist_n].cmd = (char *)NULL;
	history[current_hist_n].len = 0;
	free(line_buff);
	fclose(hist_fp);
	return EXIT_SUCCESS;
}

void
add_to_cmdhist(char *cmd)
{
	if (!cmd)
		return;

	/* Remove trailing spaces from CMD */
	size_t cmd_len = strlen(cmd);
	int i = (int)cmd_len;
	while (--i >= 0 && cmd[i] == ' ') {
		cmd[i] = '\0';
		cmd_len--;
	}
	if (cmd_len == 0)
		return;

	/* For readline */
	add_history(cmd);

	if (config_ok == 1 && hist_status == 1 && hist_file)
		append_history(1, hist_file);

	/* For us */
	/* Add the new input to the history array */
	history = (struct history_t *)xrealloc(history, (size_t)(current_hist_n + 2) * sizeof(struct history_t));
	history[current_hist_n].cmd = savestring(cmd, cmd_len);
	history[current_hist_n].len = cmd_len;
	current_hist_n++;
	history[current_hist_n].cmd = (char *)NULL;
	history[current_hist_n].len = 0;
}

/* Returns 1 if INPUT should be stored in history and 0 if not */
int
record_cmd(char *input)
{
	/* NULL input, self or parent (. ..) and commands starting with space */
	if (!input || !*input || SELFORPARENT(input) || *input == ' ')
		return 0;

	/* Blank lines */
	size_t blank = 1;
	char *p = input;

	while (*p) {
		if (*p > ' ') {
			blank = 0;
			break;
		}
		p++;
	}

	if (blank == 1)
		return 0;

	/* Rewind the pointer to the beginning of the input line */
	p = input;

	size_t len = strlen(p), amp_rm = 0;
	if (len > 0 && p[len - 1] == '&') {
		p[len - 1] = '\0';
		amp_rm = 1;
	}

	/* Do not record single ELN's */
	if (*p > '0' && *p <= '9' && is_number(p)) {
		if (amp_rm == 1)
			p[len - 1] = '&';
		return 0;
	}

	if (amp_rm == 1)
		p[len - 1] = '&';

	switch(*p) {
	case '.': /* . */
		if (!*(p + 1))
			return 0;
		break;

	/* Do not record the history command itself */
	case 'h':
		if (*(p + 1) == 'i' && strcmp(p, "history") == 0)
			return 0;
		break;

	case 'r': /* rf command */
		if (*(p + 1) == 'f' && !*(p + 2))
			return 0;
		break;

	/* Do not record exit commands */
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

	default: break;
	}

	/* History */
	if (*p == '!' && (_ISDIGIT(*(p + 1)) || (*(p + 1) == '-'
	&& _ISDIGIT(*(p + 2))) || ((*(p + 1) == '!') && *(p + 2) == '\0')))
		return 0;

	/* Consequtively equal commands in history */
	if (history && history[current_hist_n - 1].cmd
	&& *p == *history[current_hist_n - 1].cmd
	&& strcmp(p, history[current_hist_n - 1].cmd) == 0)
		return 0;

	return 1;
}
