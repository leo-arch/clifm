/* history.c -- functions for the history system */

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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <readline/history.h>
#include <limits.h> /* INT_MIN */

#include "aux.h"
#include "checks.h"
#include "exec.h"
#include "history.h"
#include "init.h"
#include "misc.h"
#include "messages.h"
#include "file_operations.h"

#ifndef _NO_SPLIT_LOG
////// TEMPORAL CODE
/* Split the old log file: messages go into the message logs file, and commands
 * into the command logs file. */
void
split_old_log_file(void)
{
	/* 1) Create the old file path */
	char *new_file = msgs_log_file ? msgs_log_file : cmds_log_file;
	if (!new_file)
		return;

	char *p = strrchr(new_file, '/');
	if (!p)
		return;

	*p = '\0';
	char old_file[PATH_MAX];
	snprintf(old_file, sizeof(old_file), "%s/log.clifm", new_file);
	*p = '/';

	/* 2) Open the 3 files */
	FILE *old = fopen(old_file, "r");
	if (!old)
		return;

	FILE *msg = fopen(msgs_log_file, "a");
	if (!msg) {
		fclose(old);
		return;
	}

	FILE *cmd = fopen(cmds_log_file, "a");
	if (!cmd) {
		fclose(msg);
		fclose(old);
		return;
	}

	/* 3) Read the old file and move lines to the new ones accordingly */
	size_t line_size = 0;
	char *line = (char *)NULL;

	while (getline(&line, &line_size, old) > 0) {
		if (!line || !*line || (*line != 'c' && *line != 'm')
		|| line[1] != ':' || !line[2])
			continue;

		fprintf(*line == 'm' ? msg : cmd, "%s", line + 2);
	}

	free(line);
	fclose(old);
	fclose(msg);
	fclose(cmd);

	/* 4) Remove the old file */
	remove(old_file);
}
////////////////////
#endif // _NO_SPLIT_LOG

/* Return a string with the current date.
 * Used to compose log entries. */
static char *
get_date(void)
{
	time_t rawtime = time(NULL);
	struct tm t;
	if (!localtime_r(&rawtime, &t))
		return (char *)NULL;

	size_t date_max = MAX_TIME_STR;
	char *date = (char *)xnmalloc(date_max + 1, sizeof(char));
	*date = '\0';

	strftime(date, date_max, "%Y-%m-%dT%T%z", &t);
	return date;
}

/* Print available logs, for messages, if MSG is 1, or for commands otherwise. */
int
print_logs(const int flag)
{
	char *file = flag == MSG_LOGS ? msgs_log_file : cmds_log_file;

	FILE *log_fp = fopen(file, "r");
	if (!log_fp) {
		_err(0, NOPRINT_PROMPT, "log: %s: %s\n", file, strerror(errno));
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

static int
gen_file(char *file)
{
	int fd = 0;
	FILE *fp = (FILE *)NULL;

	fp = open_fstream_w(file, &fd);
	if (!fp)
		return EXIT_FAILURE;

	close_fstream(fp, fd);
	return EXIT_SUCCESS;
}

/* Clear logs (the message logs if FLAG is CLR_MSG_LOGS, or the command logs
 * otherwise).
 * Delete the file, recreate it, and write the last command ("log msg/cmd clear")
 * into the command logs file. */
int
clear_logs(const int flag)
{
	char *file = flag == MSG_LOGS ? msgs_log_file : cmds_log_file;

	if (!file || !*file)
		return EXIT_SUCCESS;

	if (remove(file) == -1) {
		xerror("log: %s: %s\n", file, strerror(errno));
		return errno;
	}

	int ret = gen_file(file);
	if (ret != EXIT_SUCCESS)
		return EXIT_FAILURE;

	free(last_cmd);
	last_cmd = savestring(flag == MSG_LOGS
		? "log msg clear" : "log cmd clear", 13);
	int bk = conf.log_cmds;
	conf.log_cmds = 1;
	log_cmd();
	conf.log_cmds = bk;

	return EXIT_SUCCESS;
}

/* Log LAST_CMD (global) into LOG_FILE */
int
log_cmd(void)
{
	if (xargs.stealth_mode == 1 || !last_cmd || conf.log_cmds == 0) {
		free(last_cmd);
		last_cmd = (char *)NULL;
		return EXIT_SUCCESS;
	}

	if (config_ok == 0 || !cmds_log_file)
		return EXIT_FAILURE;

	/* Construct the log line */
	char *date = get_date();

	size_t log_len = strlen(date ? date : "unknown")
		+ (workspaces[cur_ws].path ? strlen(workspaces[cur_ws].path) : 2)
		+ strlen(last_cmd) + 6;

	char *full_log = (char *)xnmalloc(log_len, sizeof(char));
	snprintf(full_log, log_len, "[%s] %s:%s\n", date ? date : "unknown",
		workspaces[cur_ws].path ? workspaces[cur_ws].path : "?", last_cmd);

	free(date);
	free(last_cmd);
	last_cmd = (char *)NULL;

	/* Write the log into LOG_FILE */
	FILE *log_fp;

	log_fp = fopen(cmds_log_file, "a");
	if (!log_fp) {
		_err('e', PRINT_PROMPT, "log: %s: %s\n", cmds_log_file, strerror(errno));
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
	FILE *msg_fp = fopen(msgs_log_file, "a");
	if (!msg_fp) {
		/* Do not log this error: We might enter into an infinite loop
		 * trying to access a file that cannot be accessed. Just warn the user
		 * and print the error to STDERR */
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME,
			msgs_log_file, strerror(errno));
		fputs("Press any key to continue... ", stdout);
		xgetchar();
		putchar('\n');
		return;
	}

	char *date = get_date();
	fprintf(msg_fp, "[%s] %s", date ? date : "unknown", _msg);
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
	size_t nlen = strnlen(name, sizeof(name));
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
	snprintf(_msg, msg_len,
		"'display notification \"%s\" subtitle \"%s\" with title \"%s\"'",
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
 * file as follows: "m:[date] msg", where 'date' is YYYY-MM-DDTHH:MM:SS */
void
log_msg(char *_msg, const int print_prompt, const int logme,
	const int add_to_msgs_list)
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
		if (conf.desktop_notifications == 1 && logme != 0)
			send_desktop_notification(_msg);
		else
			print_msg = 1;
	} else {
		fputs(_msg, stderr);
	}

	if (xargs.stealth_mode == 1 || config_ok == 0 || !msgs_log_file
	|| !*msgs_log_file || logme != 1 || conf.log_msgs == 0)
		return;

	write_msg_into_logfile(_msg);
}

/*
int
save_dirhist(void)
{
	if (!dirhist_file)
		return EXIT_FAILURE;

	if (!old_pwd || !old_pwd[0])
		return EXIT_SUCCESS;

	FILE *fp = fopen(dirhist_file, "w");
	if (!fp) {
		xerror(_("%s: Error saving directory history: %s\n"),
			PROGRAM_NAME, strerror(errno));
		return EXIT_FAILURE;
	}

	// Let's keep only the last MaxDirhist entries
	int i, n = dirhist_total_index <= conf.max_dirhist ? 0
		: dirhist_total_index - conf.max_dirhist;

	for (i = n; i < dirhist_total_index; i++) {
		// Exclude invalid/consecutive equal entries
		if (!old_pwd[i] || *old_pwd[i] == _ESC || (i > 0 && old_pwd[i - 1]
		&& strcmp(old_pwd[i - 1], old_pwd[i]) == 0))
			continue;

		fprintf(fp, "%s\n", old_pwd[i]);
	}

	fclose(fp);
	return EXIT_SUCCESS;
} */

static void
append_to_dirhist_file(const char *dir_path)
{
	if (!dirhist_file || !dir_path || !*dir_path || xargs.stealth_mode == 1)
		return;

	FILE *fp = fopen(dirhist_file, "a");
	if (!fp) {
		xerror(_("%s: %s: Error saving directory entry: %s\n"),
			PROGRAM_NAME, dir_path, strerror(errno));
		return;
	}

	fprintf(fp, "%s\n", dir_path);
	fclose(fp);
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

		append_to_dirhist_file(dir_path);

		dirhist_total_index++;
		old_pwd[dirhist_total_index] = (char *)NULL;
	}

	/* If not at the end of dirhist, add previous AND new entry */
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
reload_history(void)
{
	clear_history();
	read_history(hist_file);
	history_truncate_file(hist_file, conf.max_hist);

	/* Update the history array */
	int ret = get_history();

	return ret;
}

static int
edit_history(char **args)
{
	struct stat attr;
	if (stat(hist_file, &attr) == -1) {
		int err = errno;
		xerror("history: %s: %s\n", hist_file, strerror(errno));
		return err;
	}
	time_t mtime_bfr = (time_t)attr.st_mtime;

	int ret = EXIT_SUCCESS;

	/* If we have an opening application (2nd argument) */
	if (args[2]) {
		char *cmd[] = {args[2], hist_file, NULL};
		ret = launch_execve(cmd, FOREGROUND, E_NOFLAG);
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
//		ret = reload_history(args);
		ret = reload_history();
		print_reload_msg(_("File modified. History entries reloaded\n"));
		return ret;
	}

	return EXIT_SUCCESS;
}

static int
_clear_history(char **args)
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
	return reload_history();
}

static int
print_history_list(const int timestamp)
{
	int n = DIGINUM(current_hist_n);
	size_t i;
	for (i = 0; i < current_hist_n; i++) {
		if (timestamp == 1 && history[i].date != -1) {
			char tdate[MAX_TIME_STR];
			gen_time_str(tdate, sizeof(tdate), history[i].date);
			printf(" %s# %s%s\n", "\x1b[0;2m", tdate, "\x1b[0m");
		}
		printf(" %s%-*zu%s %s\n", el_c, n, i + 1, df_c, history[i].cmd);
	}

	return EXIT_SUCCESS;
}

static int
print_last_items(char *str, const int timestamp)
{
	int num = atoi(str);

	if (num < 0 || num > (int)current_hist_n)
		num = (int)current_hist_n;

	int n = DIGINUM(current_hist_n);
	size_t i;
	for (i = current_hist_n - (size_t)num; i < current_hist_n; i++) {
		if (timestamp == 1 && history[i].date != -1) {
			char tdate[MAX_TIME_STR];
			gen_time_str(tdate, sizeof(tdate), history[i].date);
			printf(" %s# %s%s\n", "\x1b[0;2m", tdate, "\x1b[0m");
		}
		printf(" %s%-*zu%s %s\n", el_c, n, i + 1, df_c, history[i].cmd);
	}

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
history_function(char **args)
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
	if (!args[1] || (strcmp(args[1], "show-time") == 0 && !args[2]))
		return print_history_list(args[1] ? HIST_TIME : NO_HIST_TIME);

	if (*args[1] == 'e' && strcmp(args[1], "edit") == 0)
		return edit_history(args);

	if (*args[1] == 'c' && strcmp(args[1], "clear") == 0)
		return _clear_history(args);

	/* If "history -n [show-time]", print the last -n elements */
	if (*args[1] == '-' && is_number(args[1] + 1))
		return print_last_items(args[1] + 1, (args[2]
			&& strcmp(args[2], "show-time") == 0) ? HIST_TIME : NO_HIST_TIME);

	/* If "history show-time -n" */
	if ((*args[1] == 's' && strcmp(args[1], "show-time") == 0)
	&& args[2] && *args[2] == '-' && is_number(args[2] + 1))
		return print_last_items(args[2] + 1, HIST_TIME);

	if ((*args[1] == 'o' || *args[1] == 's') && (strcmp(args[1], "on") == 0
	|| strcmp(args[1], "off") == 0 || strcmp(args[1], "status") == 0))
		return toggle_history(args[1]);

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
		if ((flags & FAILED_ALIAS) || exec_cmd(cmd) != 0)
			exit_status = EXIT_FAILURE;

		flags &= ~FAILED_ALIAS;
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
		xerror("%s\n", _("history: Error parsing history command"));
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
		xerror("%s\n", _("history: Error parsing history command"));
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

	xerror("%s\n", _("history: Error parsing history command"));
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
	if (config_ok == 0 || !hist_file) return EXIT_FAILURE;

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
	time_t tdate = -1;

	while ((line_len = getline(&line_buff, &line_size, hist_fp)) > 0) {
		line_buff[line_len - 1] = '\0';
		if (!*line_buff)
			continue;

		/* Store the command timestamp and continue: the next line is the cmd itself */
		if (*line_buff == history_comment_char && *(line_buff + 1)
		&& is_number(line_buff + 1)) {
//			if (conf.hist_timestamp == 0)
//				continue;
			int d = atoi(line_buff + 1);
			tdate = d == INT_MIN ? -1 : (time_t)d;
			continue;
		}

		history = (struct history_t *)xrealloc(history,
				(current_hist_n + 2) * sizeof(struct history_t));
		history[current_hist_n].cmd = savestring(line_buff, (size_t)line_len);
		history[current_hist_n].len = (size_t)line_len;
		history[current_hist_n].date = tdate;
		tdate = -1;
		current_hist_n++;
	}

	curhistindex = current_hist_n ? current_hist_n - 1 : 0;
	history[current_hist_n].cmd = (char *)NULL;
	history[current_hist_n].len = 0;
	history[current_hist_n].date = -1;
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
	time_t tdate = time(NULL);
	history = (struct history_t *)xrealloc(history,
			(size_t)(current_hist_n + 2) * sizeof(struct history_t));
	history[current_hist_n].cmd = savestring(cmd, cmd_len);
	history[current_hist_n].len = cmd_len;
	history[current_hist_n].date = tdate;
	current_hist_n++;
	history[current_hist_n].cmd = (char *)NULL;
	history[current_hist_n].len = 0;
	history[current_hist_n].date = -1;
}

/* Returns 1 if INPUT should be stored in history and 0 if not */
int
record_cmd(char *input)
{
	/* NULL input, self or parent (. ..), commands starting with space,
	 * and input generating a BAEJ suggestion */
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
	case '.': /* Self, parent, and fastback */
		if (!*(p + 1) || (*(p + 1) == '.' && (!*(p + 2) || *(p + 2) == '.') ) )
			return 0;
		break;

	/* Do not record the history command itself */
	case 'h':
		if (*(p + 1) == 'i' && strcmp(p + 2, "story") == 0)
			return 0;
		break;

	case 'r': /* rf command */
		if (*(p + 1) == 'f' && !*(p + 2))
			return 0;
		break;

	/* Do not record exit commands */
	case 'q':
		if (!*(p + 1) || strcmp(p + 1, "uit") == 0)
			return 0;
		break;

	case 'Q':
		if (!*(p + 1))
			return 0;
		break;

	case 'e':
		if (*(p + 1) == 'x' && strcmp(p + 2, "it") == 0)
			return 0;
		break;

	default: break;
	}

	/* History */
	if (*p == '!' && (IS_DIGIT(*(p + 1)) || (*(p + 1) == '-'
	&& IS_DIGIT(*(p + 2))) || ((*(p + 1) == '!') && *(p + 2) == '\0')))
		return 0;

	/* Consequtively equal commands in history */
	if (history && current_hist_n > 0 && history[current_hist_n - 1].cmd
	&& *p == *history[current_hist_n - 1].cmd
	&& strcmp(p, history[current_hist_n - 1].cmd) == 0) {
		/* Update timestamp */
		history[current_hist_n - 1].date = time(NULL);
		return 0;
	}

	return 1;
}
