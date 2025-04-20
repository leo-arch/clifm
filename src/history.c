/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2016-2025, L. Abramovich <leo.clifm@outlook.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
*/

/* history.c -- functions for the history system */

#include "helpers.h"

#include <errno.h>
#include <string.h>
#include <time.h>
#include <readline/history.h>

#include "aux.h"
#include "checks.h"
#include "file_operations.h"
#include "history.h"
#include "init.h"
#include "messages.h"
#include "misc.h"
#include "readline.h" /* rl_get_y_or_n */
#include "spawn.h"

/* Return a string with the current date.
 * Used to compose log entries. */
static char *
get_date(void)
{
	const time_t rawtime = time(NULL);
	struct tm t;
	if (!localtime_r(&rawtime, &t))
		return (char *)NULL;

	const size_t date_max = MAX_TIME_STR;
	char *date = xnmalloc(date_max + 1, sizeof(char));
	*date = '\0';

	strftime(date, date_max, "%Y-%m-%dT%T%z", &t);
	return date;
}

/* Print available logs, for messages, if FLAG is MSG_LOGS, or for
 * commands otherwise. */
int
print_logs(const int flag)
{
	char *file = flag == MSG_LOGS ? msgs_log_file : cmds_log_file;

	FILE *log_fp = fopen(file, "r");
	if (!log_fp) {
		err(0, NOPRINT_PROMPT, "log: '%s': %s\n", file, strerror(errno));
		return FUNC_FAILURE;
	}

	size_t line_size = 0;
	char *line_buff = (char *)NULL;

	while (getline(&line_buff, &line_size, log_fp) > 0)
		fputs(line_buff, stdout);

	free(line_buff);
	fclose(log_fp);
	return FUNC_SUCCESS;
}

static int
gen_file(char *file)
{
	int fd = 0;
	FILE *fp = open_fwrite(file, &fd);

	if (!fp)
		return FUNC_FAILURE;

	fclose(fp);
	return FUNC_SUCCESS;
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
		return FUNC_SUCCESS;

	if (remove(file) == -1) {
		xerror("log: '%s': %s\n", file, strerror(errno));
		return errno;
	}

	const int ret = gen_file(file);
	if (ret != FUNC_SUCCESS)
		return FUNC_FAILURE;

	free(last_cmd);
	last_cmd = savestring(flag == MSG_LOGS
		? "log msg clear" : "log cmd clear", 13);
	const int bk = conf.log_cmds;
	conf.log_cmds = 1;
	log_cmd();
	conf.log_cmds = bk;

	return FUNC_SUCCESS;
}

/* Log LAST_CMD (global) into LOG_FILE */
int
log_cmd(void)
{
	if (xargs.stealth_mode == 1 || !last_cmd || conf.log_cmds == 0) {
		free(last_cmd);
		last_cmd = (char *)NULL;
		return FUNC_SUCCESS;
	}

	if (config_ok == 0 || !cmds_log_file)
		return FUNC_FAILURE;

	/* Construct the log line */
	char *date = get_date();

	const size_t log_len = strlen(date ? date : "unknown")
		+ (workspaces[cur_ws].path ? strlen(workspaces[cur_ws].path) : 2)
		+ strlen(last_cmd) + 6;

	char *full_log = xnmalloc(log_len, sizeof(char));
	snprintf(full_log, log_len, "[%s] %s:%s\n", date ? date : "unknown",
		workspaces[cur_ws].path ? workspaces[cur_ws].path : "?", last_cmd);

	free(date);
	free(last_cmd);
	last_cmd = (char *)NULL;

	/* Write the log into LOG_FILE */
	FILE *log_fp = open_fappend(cmds_log_file);
	if (!log_fp) {
		err('e', PRINT_PROMPT, "log: '%s': %s\n", cmds_log_file, strerror(errno));
		free(full_log);
		return FUNC_FAILURE;
	}

	fputs(full_log, log_fp);
	free(full_log);
	fclose(log_fp);

	return FUNC_SUCCESS;
}

/* Write _MSG into the log file: [date] _MSG */
static void
write_msg_into_logfile(const char *msg_str)
{
	if (!msg_str || !*msg_str || !msgs_log_file || !*msgs_log_file)
		return;

	FILE *fp = open_fappend(msgs_log_file);
	if (!fp) {
		/* Do not log this error: We might enter into an infinite loop
		 * trying to access a file that cannot be accessed. Just warn the user
		 * and print the error to STDERR. */
		fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME,
			msgs_log_file, strerror(errno));
		press_any_key_to_continue(0);
		return;
	}

	char *date = get_date();
	fprintf(fp, "[%s] %s", date ? date : "unknown", msg_str);
	fclose(fp);
	free(date);
}

static void
send_kitty_notification(const char *msg)
{
	static int kitty_msg_id = 1;
	const int urgency = pmsg == ERROR ? 2 : (pmsg == WARNING ? 1 : 0);

	printf("\x1b]99;i=%d:d=0:p=title;%s\x1b\\",
		kitty_msg_id, PROGRAM_NAME);
	printf("\x1b]99;i=%d:d=1:n=file-manager:f=%s:u=%d:p=body;%s\x1b\\",
		kitty_msg_id, PROGRAM_NAME, urgency, msg);

	fflush(stdout);

	if (kitty_msg_id < INT_MAX)
		kitty_msg_id++;
}

/* Let's send a desktop notification */
static void
send_desktop_notification(char *msg)
{
	if (!msg || !*msg || *msg == '\n')
		return;

	size_t mlen = strlen(msg);
	if (msg[mlen - 1] == '\n') {
		msg[mlen - 1] = '\0';
		mlen--;
	}

	/* Some messages are written in the form "PROGRAM_NAME: MSG". We only
	 * want the MSG part. */
	int ret = 0;
	const size_t s = sizeof(PROGRAM_NAME) - 1;
	char *p = msg;
	if (strncmp(msg, PROGRAM_NAME, s) == 0
	&& msg[s] == ':' && msg[s + 1] == ' ') {
		p += s + 2;
		if (!*p)
			return;
	}

	if (conf.desktop_notifications == DESKTOP_NOTIF_KITTY) {
		send_kitty_notification(p);
		return;
	}

	char type[12];
	*type = '\0';

	switch (pmsg) {
#if defined(__HAIKU__)
	case ERROR: snprintf(type, sizeof(type), "error"); break;
	case WARNING: snprintf(type, sizeof(type), "important"); break;
	case NOTICE: /* fallthrough */
	default: snprintf(type, sizeof(type), "information"); break;
#elif defined(__APPLE__)
	case ERROR: snprintf(type, sizeof(type), "Error"); break;
	case WARNING: snprintf(type, sizeof(type), "Warning"); break;
	case NOTICE: /* fallthrough */
	default: snprintf(type, sizeof(type), "Notice"); break;
#else
	case ERROR: snprintf(type, sizeof(type), "critical"); break;
	case WARNING: snprintf(type, sizeof(type), "normal"); break;
	case NOTICE: /* fallthrough */
	default: snprintf(type, sizeof(type), "low"); break;
#endif /* __HAIKU__ */
	}

#if defined(__HAIKU__)
	char *cmd[] = {"notify", "--type", type, "--title", PROGRAM_NAME, p, NULL};
	ret = launch_execv(cmd, FOREGROUND, E_MUTE);
#elif defined(__APPLE__)
	size_t msg_len = strlen(msg) + strlen(type)
		+ (sizeof(PROGRAM_NAME) - 1) + 60;
	char *tmp_msg = xnmalloc(msg_len, sizeof(char));
	snprintf(tmp_msg, msg_len,
		"'display notification \"%s\" subtitle \"%s\" with title \"%s\"'",
		msg, type, PROGRAM_NAME);
	char *cmd[] = {"osascript", "-e", tmp_msg, NULL};
	ret = launch_execv(cmd, FOREGROUND, E_MUTE);
	free(tmp_msg);
#else
	char *cmd[] = {"notify-send", "-u", type, PROGRAM_NAME, p, NULL};
	ret = launch_execv(cmd, FOREGROUND, E_MUTE);
#endif /* __HAIKU__ */

	if (ret == FUNC_SUCCESS)
		return;

	/* Error: warn and print the original message */
	xerror(_("%s: Notification daemon error: %s\n"
		"Disable desktop notifications (run 'help desktop-notifications' "
		"for details) or %s to silence this "
		"warning (original message printed below)\n"),
		PROGRAM_NAME, strerror(ret),
		ret == ENOENT ? _("install a notification daemon")
		: _("fix the error (consult your daemon's documentation)"));
	xerror("%s\n", msg);
}

/* Handle the error message MSG.
 *
 * If ADD_TO_MSGS_LIST is 1, store MSG into the messages array: MSG will be
 * accessible to the user via the 'msg' command.
 *
 * If PRINT_PROMPT is 1, either raise a flag to tell the next prompt to print
 * the message itself, or, if desktop notifications are enabled and LOGME is
 * not zero (ERR_NO_LOG), send the notification to the notification daemon.
 * NOTE: if not zero, LOGME could be either 1 (error/warning) or -1 (notice).
 *
 * If PRINT_PROMPT is not 1, MSG is printed directly here.
 *
 * Finally, if logs are enabled and LOGME is 1, write the message into the log
 * file as follows: "m:[date] msg", where 'date' is YYYY-MM-DDTHH:MM:SS.
 * */
void
log_msg(char *msg_str, const int print_prompt, const int logme,
	const int add_to_msgs_list)
{
	if (!msg_str)
		return;

	const size_t msg_len = strlen(msg_str);
	if (msg_len == 0)
		return;

	if (add_to_msgs_list == 1) {
		msgs_n++;
		messages = xnrealloc(messages, (size_t)(msgs_n + 1),
			sizeof(struct pmsgs_t));
		messages[msgs_n - 1].text = savestring(msg_str, msg_len);
		messages[msgs_n - 1].read = 0;
		messages[msgs_n].text = (char *)NULL;
		messages[msgs_n].read = 0;
	}

	if (print_prompt == 1) {
		if (conf.desktop_notifications > 0 && logme != 0)
			send_desktop_notification(msg_str);
		else
			print_msg = 1;
	} else {
		fputs(msg_str, stderr);
	}

	if (xargs.stealth_mode == 1 || config_ok == 0 || !msgs_log_file
	|| !*msgs_log_file || logme != 1 || conf.log_msgs == 0)
		return;

	write_msg_into_logfile(msg_str);
}

static void
append_to_dirhist_file(const char *dir_path)
{
	if (!dirhist_file || !dir_path || !*dir_path || xargs.stealth_mode == 1)
		return;

	FILE *fp = open_fappend(dirhist_file);
	if (!fp) {
		xerror(_("%s: '%s': Error saving directory entry: %s\n"),
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

		old_pwd = xnrealloc(old_pwd, (size_t)dirhist_total_index + 2,
			sizeof(char *));

		dirhist_cur_index = dirhist_total_index;
		old_pwd[dirhist_total_index] = savestring(dir_path, strlen(dir_path));

		append_to_dirhist_file(dir_path);

		dirhist_total_index++;
		old_pwd[dirhist_total_index] = (char *)NULL;
	}

	/* If not at the end of dirhist, add previous AND new entry */
	else {
		old_pwd = xnrealloc(old_pwd, (size_t)dirhist_total_index + 3,
			sizeof(char *));

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
	const int ret = get_history();

	return ret;
}

static int
edit_history(char **args)
{
	struct stat attr;
	if (stat(hist_file, &attr) == -1) {
		xerror("history: '%s': %s\n", hist_file, strerror(errno));
		return errno;
	}

	const time_t mtime_bfr = attr.st_mtime;

	int ret = open_config_file(args[2], hist_file);
	if (ret != FUNC_SUCCESS)
		return ret;

	/* Get modification time after opening the config file. */
	if (stat(hist_file, &attr) == -1) {
		xerror("history: '%s': %s\n", hist_file, strerror(errno));
		return errno;
	}

	/* If modification times differ, the file was modified after being
	 * opened. */
	if (mtime_bfr != attr.st_mtime) {
		ret = reload_history();
		print_reload_msg(NULL, NULL, _("File modified. History entries reloaded\n"));
		return ret;
	}

	return FUNC_SUCCESS;
}

static int
clear_history_func(char **args)
{
	if (rl_get_y_or_n(_("Clear history?"), conf.default_answer.remove) == 0)
		return FUNC_SUCCESS;

	/* Let's overwrite whatever was there. */
	int fd = 0;
	FILE *hist_fp = open_fwrite(hist_file, &fd);
	if (!hist_fp) {
		err(0, NOPRINT_PROMPT, "history: '%s': %s\n", hist_file, strerror(errno));
		return FUNC_FAILURE;
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
			printf(" %s%-*zu%s %s%s%s %s\n", el_c, n, i + 1, df_c,
				conf.colorize == 1 ? "\x1b[0;2m" : "",
				tdate, "\x1b[0m", history[i].cmd);
		} else {
			printf(" %s%-*zu%s %s\n", el_c, n, i + 1, df_c, history[i].cmd);
		}
	}

	return FUNC_SUCCESS;
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

	return FUNC_SUCCESS;
}

static int
print_hist_status(void)
{
	printf(_("History is %s\n"), hist_status == 1 ? "enabled" : "disabled");
	return FUNC_SUCCESS;
}

static int
toggle_history(const char *arg)
{
	if (!arg || !*arg)
		return FUNC_FAILURE;

	switch (*arg) {
	case 'o':
		hist_status = (arg[1] == 'n' ? 1 : 0);
		return print_hist_status();
	case 's': return print_hist_status();
	default: puts(_(HISTORY_USAGE)); return FUNC_FAILURE;
	}
}

int
history_function(char **args)
{
	if (xargs.stealth_mode == 1) {
		printf(_("%s: history: %s\n"), PROGRAM_NAME, STEALTH_DISABLED);
		return FUNC_SUCCESS;
	}

	if (config_ok == 0) {
		xerror(_("%s: History function disabled\n"), PROGRAM_NAME);
		return FUNC_FAILURE;
	}

	/* If no arguments, print the history list */
	if (!args[1] || (strcmp(args[1], "show-time") == 0 && !args[2]))
		return print_history_list(args[1] ? HIST_TIME : NO_HIST_TIME);

	if (*args[1] == 'e' && strcmp(args[1], "edit") == 0)
		return edit_history(args);

	if (*args[1] == 'c' && strcmp(args[1], "clear") == 0)
		return clear_history_func(args);

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
	return FUNC_SUCCESS;
}

int
get_history(void)
{
	if (config_ok == 0 || !hist_file) return FUNC_FAILURE;

	if (current_hist_n == 0) { /* Coming from main() */
		history = xcalloc(1, sizeof(struct history_t));
	} else { /* Only true when comming from 'history clear' */
		size_t i;
		for (i = 0; history[i].cmd; i++)
			free(history[i].cmd);
		history = xnrealloc(history, 1, sizeof(struct history_t));
		current_hist_n = 0;
	}

	FILE *hist_fp = fopen(hist_file, "r");
	if (!hist_fp) {
		err('e', PRINT_PROMPT, "history: '%s': %s\n", hist_file, strerror(errno));
		return FUNC_FAILURE;
	}

	size_t line_size = 0;
	char *line_buff = (char *)NULL;
	ssize_t line_len = 0;
	time_t tdate = -1;

	while ((line_len = getline(&line_buff, &line_size, hist_fp)) > 0) {
		line_buff[line_len - 1] = '\0';
		if (!*line_buff)
			continue;

		/* Store the command timestamp and continue: the next line is
		 * the cmd itself. */
		if (*line_buff == history_comment_char && *(line_buff + 1)
		&& is_number(line_buff + 1)) {
			int d = atoi(line_buff + 1);
			tdate = d == INT_MIN ? -1 : (time_t)d;
			continue;
		}

		history = xnrealloc(history, current_hist_n + 2,
			sizeof(struct history_t));
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

	return FUNC_SUCCESS;
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
	const time_t tdate = time(NULL);
	history = xnrealloc(history, (size_t)(current_hist_n + 2),
		sizeof(struct history_t));
	history[current_hist_n].cmd = savestring(cmd, cmd_len);
	history[current_hist_n].len = cmd_len;
	history[current_hist_n].date = tdate;
	current_hist_n++;
	history[current_hist_n].cmd = (char *)NULL;
	history[current_hist_n].len = 0;
	history[current_hist_n].date = -1;
}

/* Returns 1 if INPUT should be saved on history or 0 if not. */
int
record_cmd(char *input)
{
	if (!input || !*input)
		return 0;

	dir_cmds.last_cmd_ignored = 0;

	/* Ignore entries matching HistIgnore */
	if (conf.histignore_regex && *conf.histignore_regex
	&& regexec(&regex_hist, input, 0, NULL, 0) == FUNC_SUCCESS) {
		dir_cmds.last_cmd_ignored = 1;
		return 0;
	}

	/* Consequtively equal commands in history */
	if (history && current_hist_n > 0 && history[current_hist_n - 1].cmd
	&& *input == *history[current_hist_n - 1].cmd
	&& strcmp(input, history[current_hist_n - 1].cmd) == 0) {
		/* Update timestamp */
		history[current_hist_n - 1].date = time(NULL);
		return 0;
	}

	return 1;
}
