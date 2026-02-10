/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* exec.c -- functions controlling the execution of programs */

#include "helpers.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>   /* access() */
#include <sys/wait.h> /* waitpid() */
#include <termios.h>  /* tcgetattr(), tcsetattr() */
#include <time.h>     /* clock_gettime() */

#ifdef __OpenBSD__
typedef char *rl_cpvfunc_t;
typedef void rl_macro_print_func_t (const char *, const char *, int, const char *);
# include <ereadline/readline/readline.h>
#else
# include <readline/readline.h>
#endif /* __OpenBSD__ */

#ifdef __TINYC__
# undef CHAR_MAX /* Silence redefinition error */
#endif /* __TINYC__ */

#include "actions.h"
#ifndef _NO_ARCHIVING
# include "archives.h"
#endif /* !_NO_ARCHIVING */
#include "autocmds.h" /* add_autocmd() */
#include "aux.h"
#include "bookmarks.h"
#include "checks.h"
#include "colors.h"
#include "config.h"
#include "exec.h"
#include "file_operations.h"
#include "fs_events.h" /* check_fs_events */
#include "history.h"
#include "init.h"
#include "jump.h"
#include "keybinds.h"
#include "listing.h"
#include "messages.h"
#ifndef NO_MEDIA_FUNC
# include "media.h"
#endif /* NO_MEDIA_FUNC */
#include "mime.h"
#include "misc.h"
#ifndef _NO_BLEACH
# include "name_cleaner.h"
#endif /* !_NO_BLEACH */
#include "navigation.h"
#ifndef _NO_PROFILES
# include "profiles.h"
#endif /* !_NO_PROFILES */
#include "prompt.h"
#include "properties.h"
#include "readline.h"
#include "remotes.h"
#include "search.h"
#include "selection.h"
#include "sort.h"
#ifndef _NO_TRASH
# include "trash.h"
#endif /* !_NO_TRASH */
#include "sanitize.h"
#include "spawn.h"
#include "tags.h"
#ifndef _NO_LIRA
# include "view.h" /* preview_function */
#endif /* !_NO_LIRA */

#if !defined(__CYGWIN__)
/* Get current modification time for each path in PATH and compare it to
 * the cached time (in paths[n].mtime).
 * Return 1 if at least one timestamp differs, or 0 otherwise. */
static int
check_paths_timestamps(void)
{
	if (path_n == 0)
		return FUNC_SUCCESS;

	struct stat a;
	int status = FUNC_SUCCESS;

	for (size_t i = path_n; i-- > 0;) {
		if (paths[i].path && *paths[i].path && stat(paths[i].path, &a) != -1
		&& a.st_mtime != paths[i].mtime) {
			paths[i].mtime = a.st_mtime;
			status = FUNC_FAILURE;
		}
	}

	return status;
}

/* Reload the list of available commands in PATH for tab completion.
 * Why? If this list is not updated, whenever some new program is
 * installed, renamed, or removed from some of the paths in PATH
 * while in Clifm, this latter needs to be restarted in order
 * to be able to recognize the new program for tab completion. */
static void
reload_binaries(void)
{
	if (check_paths_timestamps() == FUNC_SUCCESS)
		return;

	if (bin_commands) {
		for (size_t j = path_progsn; j-- > 0;)
			free(bin_commands[j]);
		free(bin_commands);
		bin_commands = NULL;
	}

	if (paths) {
		for (size_t j = path_n; j-- > 0;)
			free(paths[j].path);
		free(paths);
	}

	path_n = get_path_env(1);
	get_path_programs();
}
#endif /* !__CYGWIN__ */

/* Little export implementation. What it lacks? Command substitution
 * Export variables in ARG, in the form "VAR=VALUE", to the environment */
static int
export_var_function(char **args)
{
	if (!args || !args[0] || !*args[0]) {
		xerror("%s\n", _("export: A parameter, in the form VAR=VALUE, "
			"is required"));
		return FUNC_FAILURE;
	}

	if (IS_HELP(args[0])) {
		puts(EXPORT_VAR_USAGE);
		return FUNC_SUCCESS;
	}

	int status = FUNC_SUCCESS;
	for (size_t i = 0; args[i]; i++) {
		/* ARG might have been escaped by parse_input_str(), in the command
		 * and parameter substitution block. Let's deescape it. */
		char *ds = unescape_str(args[i]);
		if (!ds) {
			xerror("%s\n", _("export: Error unescaping argument"));
			status = FUNC_FAILURE;
			continue;
		}

		char *p = strchr(ds, '=');
		if (!p || !p[1]) {
			xerror(_("export: %s: Empty assignement\n"), ds);
			free(ds);
			status = FUNC_FAILURE;
			continue;
		}

		errno = 0;

		*p = '\0';
		if (setenv(ds, p + 1, 1) == -1) {
			status = errno;
			xerror("export: %s\n", strerror(errno));
		}
		*p = '=';

		free(ds);
	}

	return status;
}

static char *
construct_shell_cmd(char **args)
{
	if (!args || !args[0])
		return NULL;

	/* If the command starts with either ':' or ';', it has bypassed all clifm
	 * expansions. At this point we don't care about it: skip this char. */
	const int bypass = (*args[0] == ';' || *args[0] == ':');
	size_t i;
	size_t total_len = bg_proc == 1 ? 2 : 0; /* 2 == '&' + NUL byte */
	size_t cmd_len = 0;

	for (i = 0; args[i]; i++)
		total_len += strlen(args[i]) + 1; /* 1 == space */

	char *cmd = xnmalloc(total_len + 1, sizeof(char));

	for (i = 0; args[i]; i++) {
		const char *src = (i == 0 && bypass == 1) ? args[i] + 1 : args[i];
		xstrsncpy(cmd + cmd_len, src, total_len + 1);
		cmd_len += strlen(src) + 1;
		cmd[cmd_len - 1] = ' ';
		cmd[cmd_len] = '\0';
	}

	if (bg_proc == 1) {
		cmd[cmd_len] = '&';
		cmd[cmd_len + 1] = '\0';
	} else {
		cmd[cmd_len - 1] = '\0'; /* Remove trailing space */
	}

	return cmd;
}

static int
check_shell_cmd_conditions(char **args)
{
	/* No command name ends with a slash */
	const size_t len = (args && args[0]) ? strlen(args[0]) : 0;
	if (len > 0 && args[0][len - 1] == '/') {
		xerror("%s: '%s': %s\n", conf.autocd == 1 ? "cd" : "open",
			args[0], strerror(ENOENT));
		return conf.autocd == 1 ? FUNC_FAILURE : E_NOTFOUND;
	}

	if (conf.ext_cmd_ok == 0) {
		xerror(_("%s: External commands are currently disabled. "
			"To enable them, run 'ext on'.\n"), PROGRAM_NAME);
		return FUNC_FAILURE;
	}

	return FUNC_SUCCESS;
}

static int
run_shell_cmd(char **args)
{
	const int ret = check_shell_cmd_conditions(args);
	if (ret != FUNC_SUCCESS)
		return ret;

	char *cmd = construct_shell_cmd(args);

	struct termios orig_termios;
	tcgetattr(STDIN_FILENO, &orig_termios);

	/* Calling the system shell is vulnerable to command injection, true.
	 * But it is the user here who is directly running the command: this
	 * should not be taken as an untrusted source. */
	const int exit_status = launch_execl(cmd);

	tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
	free(cmd);

/* For the time being, this is too slow on Cygwin. */
#if !defined(__CYGWIN__)
	reload_binaries();
#endif /* !__CYGWIN__ */

#if defined(GENERIC_FS_MONITOR)
	const char *p = args[0];
	if (exit_status == FUNC_SUCCESS && ((*p == 'm' && p[1] == 'v' && !p[2])
	|| (*p == 'g' && p[1] == 'm' && p[2] == 'v' && !p[3])))
		if (args[1] && is_file_in_cwd(args[1]) == 1)
			reload_dirlist();
#endif

	return exit_status;
}

/* Free everything and exit the program */
static void
quit_func(char **args, const int exit_status)
{
	if (!args || !args[0])
		return;

	if (args[1] && IS_HELP(args[1])) {
		puts(QUIT_HELP);
		return;
	}

	for (size_t i = args_n + 1; i-- > 0;)
		free(args[i]);
	free(args);

	exit(exit_status);
}

static int
set_max_files(char **args)
{
	if (!args[1]) { /* Inform about the current value */
		if (conf.max_files == UNSET)
			puts(_("Max files: unset"));
		else
			printf(_("Max files: %d\n"), conf.max_files);
		goto SUCCESS;
	}

	if (IS_HELP(args[1])) { puts(_(MF_USAGE)); return FUNC_SUCCESS;	}

	if (*args[1] == 'u' && strcmp(args[1], "unset") == 0) {
		conf.max_files = UNSET;
		if (conf.autols == 1) reload_dirlist();
		print_reload_msg(NULL, NULL, _("Max files unset\n"));
		goto SUCCESS;
	}

	if (*args[1] == '0' && !args[1][1]) {
		conf.max_files = 0;
		if (conf.autols == 1) reload_dirlist();
		print_reload_msg(NULL, NULL, _("Max files set to %d\n"), conf.max_files);
		goto SUCCESS;
	}

	const long inum = strtol(args[1], NULL, 10);
	if (inum == LONG_MAX || inum == LONG_MIN || inum <= 0) {
		xerror(_("%s: %s: Invalid number\n"), PROGRAM_NAME, args[1]);
		return (exit_code = FUNC_FAILURE);
	}

	conf.max_files = (int)inum;
	if (conf.autols == 1) reload_dirlist();
	print_reload_msg(NULL, NULL, _("Max files set to %d\n"), conf.max_files);

SUCCESS:
	update_autocmd_opts(AC_MAX_FILES);
	return FUNC_SUCCESS;
}

static int
dirs_first_function(const char *arg)
{
	if (conf.autols == 0)
		return FUNC_SUCCESS;

	if (!arg)
		return rl_toggle_dirs_first(0, 0);

	if (IS_HELP(arg)) {
		puts(_(FF_USAGE));
		return FUNC_SUCCESS;
	}

	if (*arg == 's' && strcmp(arg, "status") == 0) {
		printf(_("Directories first is %s\n"),
			conf.list_dirs_first == 1 ? _("on") : _("off"));
	} else if (*arg == 'o' && strcmp(arg, "on") == 0) {
		conf.list_dirs_first = 1;
		if (conf.autols == 1) reload_dirlist();
		print_reload_msg(NULL, NULL, _("Directories first: on\n"));
	} else {
		if (*arg == 'o' && strcmp(arg, "off") == 0) {
			conf.list_dirs_first = 0;
			if (conf.autols == 1) reload_dirlist();
			print_reload_msg(NULL, NULL, _("Directories first: off\n"));
		}
	}

	return FUNC_SUCCESS;
}

static int
file_counter_function(const char *arg)
{
	if (!arg) {
		conf.file_counter = !conf.file_counter;
		update_autocmd_opts(AC_FILE_COUNTER);
		if (conf.autols == 1) reload_dirlist();
		print_reload_msg(NULL, NULL, _("File counter: %s\n"),
			conf.file_counter == 1 ? _("on") : _("off"));
		return FUNC_SUCCESS;
	}

	if (IS_HELP(arg)) {
		puts(_(FC_USAGE));
		return FUNC_SUCCESS;
	}

	if (*arg == 'o' && strcmp(arg, "on") == 0) {
		conf.file_counter = 1;
		update_autocmd_opts(AC_FILE_COUNTER);
		if (conf.autols == 1) reload_dirlist();
		print_reload_msg(NULL, NULL, _("File counter: on\n"));
		return FUNC_SUCCESS;
	}

	if (*arg == 'o' && strcmp(arg, "off") == 0) {
		conf.file_counter = 0;
		update_autocmd_opts(AC_FILE_COUNTER);
		if (conf.autols == 1) reload_dirlist();
		print_reload_msg(NULL, NULL, _("File counter: off\n"));
		return FUNC_SUCCESS;
	}

	if (*arg == 's' && strcmp(arg, "status") == 0) {
		if (conf.file_counter == 1)
			puts(_("The file counter is on"));
		else
			puts(_("The file counter is off"));
		return FUNC_SUCCESS;
	}

	fprintf(stderr, "%s\n", _(FC_USAGE));
	return FUNC_FAILURE;
}

static int
pager_function(const char *arg)
{
	if (!arg || (*arg == 'o' && strcmp(arg, "once") == 0)) {
		const int pg_bk = conf.pager;
		conf.pager = 1;
		conf.pager_once = 1;
		if (conf.autols == 1)
			reload_dirlist();
		if (!arg) /* pg (no parameter) */
			conf.pager = pg_bk;
		return FUNC_SUCCESS;
	}

	if (IS_HELP(arg)) {
		puts(_(PAGER_USAGE));
		return FUNC_SUCCESS;
	}

	if (*arg == 's' && strcmp(arg, "status") == 0) {
		switch (conf.pager) {
		case 0: puts(_("The pager is off")); break;
		case 1: puts(_("The pager is on")); break;
		default: printf(_("The pager is set to %d\n"), conf.pager); break;
		}

		return FUNC_SUCCESS;
	}

	if (is_number(arg)) {
		const int n = xatoi(arg);
		if (n == INT_MIN) {
			xerror("%s\n", _("pg: xatoi: Error converting to integer"));
			return FUNC_FAILURE;
		}
		conf.pager = n;
		printf(_("Pager set to %d\n"), n);
		goto SUCCESS;
	}

	if (*arg == 'o' && strcmp(arg, "off") == 0) {
		conf.pager = 0;
		puts(_("Pager: off"));
		goto SUCCESS;
	}

	if (*arg == 'o' && strcmp(arg, "on") == 0) {
		conf.pager = 1;
		if (conf.autols == 1)
			reload_dirlist();
		else
			puts(_("Pager: on"));
		goto SUCCESS;
	}

	fprintf(stderr, "%s\n", _(PAGER_USAGE));
	return FUNC_FAILURE;

SUCCESS:
	update_autocmd_opts(AC_PAGER);
	return FUNC_SUCCESS;
}

static int
ext_cmds_function(const char *arg)
{
	if (!arg || IS_HELP(arg)) {
		puts(_(EXT_USAGE));
		return FUNC_SUCCESS;
	}

	int exit_status = FUNC_SUCCESS;

	if (*arg == 's' && strcmp(arg, "status") == 0) {
		printf(_("External commands are %s\n"),
			conf.ext_cmd_ok ? _("allowed") : _("not allowed"));
	} else if (*arg == 'o' && strcmp(arg, "on") == 0) {
		conf.ext_cmd_ok = 1;
		puts(_("External commands: on"));
	} else if (*arg == 'o' && strcmp(arg, "off") == 0) {
		conf.ext_cmd_ok = 0;
		puts(_("External commands: off"));
	} else {
		fprintf(stderr, "%s\n", _(EXT_USAGE));
		exit_status = FUNC_FAILURE;
	}

	return exit_status;
}

static int
autocd_function(const char *arg)
{
	if (!arg) {
		fprintf(stderr, "%s\n", _(AUTOCD_USAGE));
		return FUNC_FAILURE;
	}

	if (strcmp(arg, "on") == 0) {
		conf.autocd = 1;
		puts(_("Autocd: on"));
	} else if (strcmp(arg, "off") == 0) {
		conf.autocd = 0;
		puts(_("Autocd: off"));
	} else if (strcmp(arg, "status") == 0) {
		printf(_("Autocd is %s\n"), conf.autocd == 1
			? _("on") : _("off"));
	} else if (IS_HELP(arg)) {
		puts(_(AUTOCD_USAGE));
	} else {
		fprintf(stderr, "%s\n", _(AUTOCD_USAGE));
		return FUNC_FAILURE;
	}

	return FUNC_SUCCESS;
}

static int
auto_open_function(const char *arg)
{
	if (!arg) {
		fprintf(stderr, "%s\n", _(AUTO_OPEN_USAGE));
		return FUNC_FAILURE;
	}

	if (strcmp(arg, "on") == 0) {
		conf.auto_open = 1;
		puts(_("Auto-open: on"));
	} else if (strcmp(arg, "off") == 0) {
		conf.auto_open = 0;
		puts(_("Auto-open: off"));
	} else if (strcmp(arg, "status") == 0) {
		printf(_("Auto-open is %s\n"), conf.auto_open == 1
			? _("on") : _("off"));
	} else if (IS_HELP(arg)) {
		puts(_(AUTO_OPEN_USAGE));
	} else {
		fprintf(stderr, "%s\n", _(AUTO_OPEN_USAGE));
		return FUNC_FAILURE;
	}
	return FUNC_SUCCESS;
}

static int
columns_function(const char *arg)
{
	if (!arg) {
		conf.columned = !conf.columned;
		if (conf.autols == 1) reload_dirlist();
		print_reload_msg(NULL, NULL, _("Columns: %s\n"), conf.columned == 1
			? _("on") : _("off"));
		return FUNC_SUCCESS;
	}

	if (IS_HELP(arg)) {
		puts(_(COLUMNS_USAGE));
		return FUNC_SUCCESS;
	}

	if (*arg == 'o' && arg[1] == 'n' && !arg[2]) {
		conf.columned = 1;
		if (conf.autols == 1) reload_dirlist();
		print_reload_msg(NULL, NULL, _("Columns: on\n"));
	} else if (*arg == 'o' && strcmp(arg, "off") == 0) {
		conf.columned = 0;
		if (conf.autols == 1) reload_dirlist();
		print_reload_msg(NULL, NULL, _("Columns: off\n"));
	} else {
		fprintf(stderr, "%s\n", _(COLUMNS_USAGE));
		return FUNC_FAILURE;
	}

	return FUNC_SUCCESS;
}

static int
icons_function(const char *arg)
{
#ifdef _NO_ICONS
	UNUSED(arg);
	xerror(_("%s: icons: %s\n"), PROGRAM_NAME, _(NOT_AVAILABLE));
	return FUNC_SUCCESS;
#else
	if (!arg || !*arg) {
		conf.icons = !conf.icons;
		if (conf.autols == 1) reload_dirlist();
		print_reload_msg(NULL, NULL, _("Icons: %s\n"),
			conf.icons == 1 ? _("on") : _("off"));
		return FUNC_SUCCESS;
	}

	if (IS_HELP(arg)) {
		puts(_(ICONS_USAGE));
		return FUNC_SUCCESS;
	}

	if (*arg == 'o' && arg[1] == 'n' && !arg[2]) {
		conf.icons = 1;
		if (conf.autols == 1) reload_dirlist();
		print_reload_msg(NULL, NULL, _("Icons: on\n"));
		return FUNC_SUCCESS;
	} else if (*arg == 'o' && strcmp(arg, "off") == 0) {
		conf.icons = 0;
		if (conf.autols == 1) reload_dirlist();
		print_reload_msg(NULL, NULL, _("Icons: off\n"));
		return FUNC_SUCCESS;
	} else {
		fprintf(stderr, "%s\n", _(ICONS_USAGE));
		return FUNC_FAILURE;
	}

	return FUNC_SUCCESS;
#endif /* _NO_ICONS */
}

static int
clear_msgs(void)
{
	if (msgs_n == 0) {
		printf(_("%s: No messages\n"), PROGRAM_NAME);
		return FUNC_SUCCESS;
	}

	for (size_t i = 0; i < msgs_n; i++)
		free(messages[i].text);

	if (conf.autols == 1)
		reload_dirlist();
	print_reload_msg(NULL, NULL, _("Messages cleared\n"));
	msgs_n = msgs.error = msgs.warning = msgs.notice = 0;
	pmsg = NOMSG;

	return FUNC_SUCCESS;
}

static int
print_msgs(void)
{
	for (size_t i = 0; i < msgs_n; i++) {
		if (i > 0 && strcmp(messages[i].text, messages[i - 1].text) == 0)
			continue;
		printf("%s", messages[i].text);
	}

	return FUNC_SUCCESS;
}

static int
msgs_function(const char *arg)
{
	if (arg && IS_HELP(arg)) {
		puts(_(MSG_USAGE));
		return FUNC_SUCCESS;
	}

	if (arg && strcmp(arg, "clear") == 0)
		return clear_msgs();

	if (msgs_n > 0)
		return print_msgs();

	printf(_("%s: No messages\n"), PROGRAM_NAME);

	return FUNC_SUCCESS;
}

static int
opener_function(const char *arg)
{
	if (!arg) {
		printf("opener: %s\n", conf.opener ? conf.opener : "lira (built-in)");
		return FUNC_SUCCESS;
	}

	if (IS_HELP(arg)) {
		puts(_(OPENER_USAGE));
		return FUNC_SUCCESS;
	}

	free(conf.opener);
	conf.opener = NULL;

	if (strcmp(arg, "default") != 0 && strcmp(arg, "lira") != 0)
		conf.opener = savestring(arg, strlen(arg));

	printf(_("Opener set to '%s'\n"), conf.opener
		? conf.opener : "lira (built-in)");

	return FUNC_SUCCESS;
}

static int
lightmode_function(const char *arg)
{
	if (!arg)
		return rl_toggle_light_mode(0, 0);

	if (IS_HELP(arg)) {
		puts(LM_USAGE);
		return FUNC_SUCCESS;
	}

	if (*arg == 'o' && strcmp(arg, "on") == 0) {
		conf.light_mode = 1;
		update_autocmd_opts(AC_LIGHT_MODE);
		if (conf.autols == 1) reload_dirlist();
		print_reload_msg(NULL, NULL, _("Light mode: on\n"));
	} else if (*arg == 'o' && strcmp(arg, "off") == 0) {
		conf.light_mode = 0;
		update_autocmd_opts(AC_LIGHT_MODE);
		if (conf.autols == 1) reload_dirlist();
		print_reload_msg(NULL, NULL, _("Light mode: off\n"));
	} else {
		puts(LM_USAGE);
	}

	return FUNC_SUCCESS;
}

static size_t
get_longest_alias_name(void)
{
	size_t l = 0;
	for (size_t i = aliases_n; i-- > 0;) {
		const size_t len = strlen(aliases[i].name);
		if (len > l)
			l = len;
	}

	return l;
}

static int
list_aliases(void)
{
	if (aliases_n == 0) {
		printf(_("%s: No aliases found\n"), PROGRAM_NAME);
		return FUNC_SUCCESS;
	}

	const size_t longest_name_len = get_longest_alias_name();

	for (size_t i = 0; i < aliases_n; i++) {
		printf("%-*s %s%s%s %s\n", (int)longest_name_len,
			aliases[i].name, mi_c, SET_MSG_PTR, df_c, aliases[i].cmd);
	}

	return FUNC_SUCCESS;
}

static int
print_alias(const char *name)
{
	if (!name || !*name)
		return FUNC_FAILURE;

	if (aliases_n == 0) {
		printf(_("%s: No aliases found\n"), PROGRAM_NAME);
		return FUNC_SUCCESS;
	}

	for (size_t i = aliases_n; i-- > 0;) {
		if (aliases[i].name && *name == *aliases[i].name
		&& strcmp(name, aliases[i].name) == 0) {
			printf("alias %s='%s'\n", aliases[i].name,
				aliases[i].cmd ? aliases[i].cmd : "");
			return FUNC_SUCCESS;
		}
	}

	xerror(_("%s: '%s': No such alias\n"), PROGRAM_NAME, name);
	return FUNC_FAILURE;
}

static int
alias_function(char **args)
{
	if (!args[1]) {
		list_aliases();
		return FUNC_SUCCESS;
	}

	if (IS_HELP(args[1])) {
		puts(_(ALIAS_USAGE));
		return FUNC_SUCCESS;
	}

	if (*args[1] == 'i' && strcmp(args[1], "import") == 0) {
		if (!args[2]) {
			puts(_(ALIAS_USAGE));
			return FUNC_SUCCESS;
		}
		return alias_import(args[2]);
	}

	if (*args[1] == 'l' && (strcmp(args[1], "ls") == 0
	|| strcmp(args[1], "list") == 0))
		return list_aliases();

	return print_alias(args[1]);
}

static const char *
gen_hidden_status(void)
{
	switch (conf.show_hidden) {
	case HIDDEN_FALSE: return _("off");
	case HIDDEN_FIRST: return _("on (list first)");
	case HIDDEN_LAST: return _("on (list last)");
	case HIDDEN_TRUE: /* fallthrough */
	default: return _("on");
	}
}

static int
hidden_files_function(const char *arg)
{
	if (!arg)
		return rl_toggle_hidden_files(0, 0);

	if (IS_HELP(arg)) {
		puts(_(HF_USAGE));
		return FUNC_SUCCESS;
	}

	if (strcmp(arg, "status") == 0) {
		printf(_("Show-hidden-files is %s\n"), gen_hidden_status());
	} else if (strcmp(arg, "first") == 0) {
		conf.show_hidden = HIDDEN_FIRST;
		if (conf.autols == 1) reload_dirlist();
		print_reload_msg(NULL, NULL, _("Hidden files: on (list first)\n"));
	} else if (strcmp(arg, "last") == 0) {
		conf.show_hidden = HIDDEN_LAST;
		if (conf.autols == 1) reload_dirlist();
		print_reload_msg(NULL, NULL, _("Hidden files: on (list last)\n"));
	} else if (strcmp(arg, "off") == 0) {
		conf.show_hidden = HIDDEN_FALSE;
		if (conf.autols == 1) reload_dirlist();
		print_reload_msg(NULL, NULL, _("Hidden files: off\n"));
	} else {
		if (strcmp(arg, "on") == 0) {
			conf.show_hidden = HIDDEN_TRUE;
			if (conf.autols == 1) reload_dirlist();
			print_reload_msg(NULL, NULL, _("Hidden files: on\n"));
		}
	}

	update_autocmd_opts(AC_SHOW_HIDDEN);

	return FUNC_SUCCESS;
}

static int
toggle_exec_func(char **args)
{
	if (!args[1] || IS_HELP(args[1]))
		{ puts(_(TE_USAGE)); return FUNC_SUCCESS; }

	int exit_status = FUNC_SUCCESS;
	size_t n = 0;

	for (size_t i = 1; args[i]; i++) {
		struct stat attr;
		if (strchr(args[i], '\\')) {
			char *tmp = unescape_str(args[i]);
			if (tmp) {
				xstrsncpy(args[i], tmp, strlen(tmp) + 1);
				free(tmp);
			}
		}

		if (lstat(args[i], &attr) == -1) {
			xerror("stat: '%s': %s\n", args[i], strerror(errno));
			exit_status = FUNC_FAILURE;
			continue;
		}

		if (toggle_exec(args[i], attr.st_mode) == FUNC_FAILURE)
			exit_status = FUNC_FAILURE;
		else
			n++;
	}

	if (n > 0) {
		if (conf.autols == 1 && exit_status == FUNC_SUCCESS)
			reload_dirlist();

		print_reload_msg(SET_SUCCESS_PTR, xs_cb,
			_("Toggled executable bit on %zu %s\n"),
			n, n > 1 ? _("files") : _("file"));
	}

	return exit_status;
}

static int
pin_function(char *arg)
{
	if (arg) {
		if (IS_HELP(arg)) {
			puts(PIN_USAGE);
			return FUNC_SUCCESS;
		}
		return pin_directory(arg);
	}

	if (pinned_dir)
		printf(_("Pinned file: '%s'\n"), pinned_dir);
	else
		puts(_("pin: No pinned file"));

	return FUNC_SUCCESS;
}

static int
props_function(char **args)
{
	if (!args[1] || IS_HELP(args[1])) {
		fprintf(stderr, "%s\n", _(PROP_USAGE));
		return FUNC_SUCCESS;
	}

	const int full_dirsize = args[0][1] == 'p'; /* Command is 'pp' */
	return properties_function(args + 1, full_dirsize);
}

static int
open_with_function(char **args)
{
#ifndef _NO_LIRA
	if (args[1]) {
		if (IS_HELP(args[1])) {
			puts(_(OW_USAGE));
			return FUNC_SUCCESS;
		}
		return mime_open_with(args[1], args[2] ? args + 2 : NULL);
	}
	puts(_(OW_USAGE));
	return FUNC_SUCCESS;
#else
	UNUSED(args);
	xerror("%s: %s\n", PROGRAM_NAME, _(NOT_AVAILABLE));
	return FUNC_FAILURE;
#endif /* !_NO_LIRA */
}

static int
refresh_function(const int old_exit_code)
{
	refresh_screen();
	return old_exit_code;
}

static int
export_files_function(char **args)
{
	if (args[1] && IS_HELP(args[1])) {
		puts(_(EXPORT_FILES_USAGE));
		return FUNC_SUCCESS;
	}

	char *ret = export_files(args, 1);
	if (ret) {
		printf(_("Filenames exported to '%s'\n"), ret);
		free(ret);
		return FUNC_SUCCESS;
	}

	return FUNC_FAILURE;
}

static int
bookmarks_func(char **args)
{
	if (args[1] && IS_HELP(args[1])) {
		puts(_(BOOKMARKS_USAGE));
		return FUNC_SUCCESS;
	}

	/* Disable keyboard shortcuts. Otherwise, the function will
	 * still be waiting for input while the screen have been taken
	 * by another function. */
	kbind_busy = 1;
	const int exit_status = bookmarks_function(args);
	/* Reenable keyboard shortcuts */
	kbind_busy = 0;

	return exit_status;
}

static int
desel_function(char **args)
{
	if (args[1] && IS_HELP(args[1])) {
		puts(_(DESEL_USAGE));
		return FUNC_SUCCESS;
	}

	kbind_busy = 1;
	rl_attempted_completion_function = NULL;
	const int exit_status = deselect(args);
	rl_attempted_completion_function = my_rl_completion;
	kbind_busy = 0;

	return exit_status;
}

static int
new_instance_function(char **args)
{
	int exit_status = FUNC_SUCCESS;

	if (args[1]) {
		if (IS_HELP(args[1])) {
			puts(_(X_USAGE));
			return FUNC_SUCCESS;
		} else if (*args[0] == 'x') {
			exit_status = new_instance(args[1], 0);
		} else { /* Run as root */
			exit_status = new_instance(args[1], 1);
		}
	} else {
		/* Run new instance in CWD */
		if (*args[0] == 'x')
			exit_status = new_instance(workspaces[cur_ws].path, 0);
		else
			exit_status = new_instance(workspaces[cur_ws].path, 1);
	}

	return exit_status;
}

#ifndef NO_MEDIA_FUNC
/* MODE could be either MEDIA_LIST (mp command) or MEDIA_MOUNT (media command) */
static int
media_function(const char *arg, const int mode)
{
	if (arg && IS_HELP(arg)) {
		if (mode == MEDIA_LIST)
			puts(_(MOUNTPOINTS_USAGE));
		else
			puts(_(MEDIA_USAGE));
		return FUNC_SUCCESS;
	}

	kbind_busy = 1;
	rl_attempted_completion_function = NULL;
	const int exit_status = media_menu(mode);
	rl_attempted_completion_function = my_rl_completion;
	kbind_busy = 0;

	return exit_status;
}
#endif /* NO_MEDIA_FUNC */

static int
chdir_function(char *arg)
{
	if (arg && IS_HELP(arg)) {
		puts(_(CD_USAGE));
		return FUNC_SUCCESS;
	}

	return cd_function(arg, CD_PRINT_ERROR);
}

static int
sort_func(char **args)
{
	if (args[1] && IS_HELP(args[1])) {
		puts(_(SORT_USAGE));
		return FUNC_SUCCESS;
	}

	return sort_function(args);
}

static int
check_pinned_file(char **args)
{
	for (size_t i = args_n + 1; i-- > 0;) {
		if (*args[i] == ',' && !args[i][1]) {
			xerror(_("%s: No pinned file\n"), PROGRAM_NAME);
			return FUNC_FAILURE;
		}
	}

	return FUNC_SUCCESS;
}

static int
check_actions(char **args)
{
	if (actions_n == 0)
		return (-1);

	for (size_t i = actions_n; i-- > 0;) {
		if (*args[0] == *usr_actions[i].name
		&& strcmp(args[0], usr_actions[i].name) == 0) {
			setenv("CLIFM_PLUGIN_NAME", usr_actions[i].name, 1);
			const int ret = run_action(usr_actions[i].value, args);
			unsetenv("CLIFM_PLUGIN_NAME");
			return ret;
		}
	}

	return (-1);
}

static int
launch_shell(const char *arg)
{
	if (!arg[1]) {
		/* If just ":" or ";", launch the default shell */
		char *cmd[] = {user.shell, NULL};
		return launch_execv(cmd, FOREGROUND, E_NOFLAG);
	}

	if (arg[1] == ';' || arg[1] == ':') {
		/* If double semi colon or colon (or ";:" or ":;") */
		xerror(_("%s: '%s': Syntax error\n"), PROGRAM_NAME, arg);
		return FUNC_FAILURE;
	}

	return (-1);
}

static void
expand_and_deescape(char **arg, char **deq_str)
{
	if (*(*arg) == '~') {
		char *exp_path = tilde_expand(*arg);
		if (exp_path) {
			free(*arg);
			*arg = exp_path;
		}
	}

	/* Deescape the string (only if filename) */
	if (strchr(*arg, '\\'))
		*deq_str = unescape_str(*arg);
}

static int
open_file_func(char **args, const filesn_t i)
{
	if (conf.autocd && (file_info[i].type == DT_DIR || file_info[i].dir == 1))
		return cd_function(args[0], CD_PRINT_ERROR);

	if (conf.auto_open && (file_info[i].type == DT_REG
	|| file_info[i].type == DT_LNK)) {
		char *cmd[] = {"open", args[0], args[1], NULL};
		return open_function(cmd);
	}

	return (-1);
}

/* Try to autocd or auto-open dir/file.
 * Only autocd or auto-open here if not absolute path and if there
 * is no second argument or if second argument is "&"
 * If just 'edit' or 'config', do not try to open a file named 'edit' or
 * 'config': always run the 'edit/config' command. */
static int
check_auto_first(char **args)
{
	if (!args || !args[0] || !*args[0])
		return (-1);

	if (*args[0] == '/' || (conf.autocd == 0 && conf.auto_open == 0)
	|| (args[1] && (*args[1] != '&' || args[1][1])))
		return (-1);

	if (!(flags & FIRST_WORD_IS_ELN) && is_internal_cmd(args[0], ALL_CMDS, 1, 1))
		return (-1);

	char *deq_str = NULL;
	if (conf.autocd == 1 || conf.auto_open == 1)
		expand_and_deescape(&args[0], &deq_str);

	char *tmp = deq_str ? deq_str : args[0];
	const size_t len = strlen(tmp);
	int rem_slash = 0;
	if (len > 0 && tmp[len - 1] == '/') {
		rem_slash = 1;
		tmp[len - 1] = '\0';
	}

	if (conf.autocd == 1 && cdpath_n > 0 && !args[1]
	&& cd_function(tmp, CD_NO_PRINT_ERROR) == FUNC_SUCCESS) {
		free(deq_str);
		return FUNC_SUCCESS;
	}

	filesn_t i = g_files_num;
	while (--i >= 0) {
		if (*tmp != *file_info[i].name || strcmp(tmp, file_info[i].name) != 0)
			continue;

		free(deq_str);
		deq_str = NULL;
		const int ret = open_file_func(args, i);
		if (ret != -1)
			return ret;
		break;
	}

	if (rem_slash == 1)
		tmp[len - 1] = '/';

	free(deq_str);
	return (-1);
}

static int
auto_open_file(char **args, char *tmp)
{
	size_t i, n = 0;
	for (i = 0; args[i]; i++);

	char **cmd = xnmalloc(i + 3, sizeof(char *));
	cmd[n++] = "open";
	cmd[n++] = tmp;

	for (i = 1; args[i]; i++)
		cmd[n++] = args[i];

	cmd[n] = NULL;

	args_n++;
	const int ret = open_function(cmd);
	args_n--;
	free(tmp);
	free(cmd);

	return ret;
}

static int
autocd_dir(char *tmp)
{
	int ret = FUNC_SUCCESS;

	if (conf.autocd) {
		ret = cd_function(tmp, CD_PRINT_ERROR);
	} else {
		xerror(_("%s: cd: '%s': Is a directory\n"), PROGRAM_NAME, tmp);
		ret = EISDIR;
	}
	free(tmp);

	return ret;
}

/* Try to autocd or auto-open */
/* If there is a second word (parameter, ARGS[1]) and this word starts
 * with a dash, do not take the first word (ARGS[0]) as a file to be
 * opened, but as a command to be executed. */
static int
check_auto_second(char **args)
{
	if (args[1] && *args[1] == '-')
		return (-1);

	char *tmp = savestring(args[0], strlen(args[0]));
	if (!tmp)
		return (-1);

	if (strchr(tmp, '\\')) {
		char *dstr = unescape_str(tmp);
		if (dstr) {
			free(tmp);
			tmp = dstr;
		}
	}

	if (conf.autocd == 1 && cdpath_n > 0 && !args[1]) {
		const int ret = cd_function(tmp, CD_NO_PRINT_ERROR);
		if (ret == FUNC_SUCCESS) {
			free(tmp);
			return FUNC_SUCCESS;
		}
	}

	struct stat attr;
	if (stat(tmp, &attr) != 0) {
		free(tmp);
		return (-1);
	}

	if (conf.autocd == 1 && S_ISDIR(attr.st_mode) && !args[1])
		return autocd_dir(tmp);

	/* Regular, non-executable file, or exec file not in PATH,
	 * not ./file, and not /path/to/file */
	if (conf.auto_open == 1 && S_ISREG(attr.st_mode)
	&& (!(attr.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
	|| (!is_bin_cmd(tmp) && !(*tmp == '.' && tmp[1] == '/')
	&& *tmp != '/' ) ) )
		return auto_open_file(args, tmp);

	free(tmp);
	return (-1);
}

static int
colors_function(const char *arg)
{
	if (arg && IS_HELP(arg))
		puts(_(COLORS_USAGE));
	else
		color_codes();
	return FUNC_SUCCESS;
}

static int
ls_function(void)
{
	free_dirlist();
	const int ret = list_dir();
	get_sel_files();

	return ret;
}

static int
lira_function(char **args)
{
#ifndef _NO_LIRA
	return mime_open(args);
#else
	UNUSED(args);
	xerror("%s: lira: %s\n", PROGRAM_NAME, _(NOT_AVAILABLE));
	return FUNC_FAILURE;
#endif /* !_NO_LIRA */
}

static int
check_comments(char *name)
{
	const int maybe_comment =
		((*name == '\\' && name[1] == '#') || *name == '#');

	if (maybe_comment == 0)
		return FUNC_FAILURE;

	char *p = (*name == '\\' || strchr(name + 1, '\\'))
		? unescape_str(name) : NULL;
	char * n = p ? p : name;

	/* Skip lines starting with '#' if there is no such filename
	 * in the current directory. This implies that no command starting
	 * with '#' will be executed */
	struct stat a;
	const int ret = lstat(n, &a);
	free(p);

	if (ret == -1)
		return FUNC_SUCCESS;

	if (conf.autocd == 1 && S_ISDIR(a.st_mode))
		return FUNC_FAILURE;

	if (conf.auto_open == 1 && !S_ISDIR(a.st_mode))
		return FUNC_FAILURE;

	return FUNC_SUCCESS;
}

static int
print_stats(void)
{
	if (conf.light_mode == 1)
		puts(_("Running in light mode: Some files statistics are not available\n"));

	char dir_empty[MAX_INT_STR + 10]; *dir_empty = '\0';
	char reg_empty[MAX_INT_STR + 10]; *reg_empty = '\0';
	char lnk_broken[MAX_INT_STR + 10]; *lnk_broken = '\0';
	if (stats.empty_dir > 0)
		snprintf(dir_empty, sizeof(dir_empty), " (%zu empty)", stats.empty_dir);
	if (stats.empty_reg > 0)
		snprintf(reg_empty, sizeof(reg_empty), " (%zu empty)", stats.empty_reg);
	if (stats.broken_link > 0)
		snprintf(lnk_broken, sizeof(lnk_broken),
			" (%zu broken)", stats.broken_link);

	printf(_("Total files:                 %jd\n\
Directories:                 %zu%s\n\
Regular files:               %zu%s\n\
Executable files:            %zu\n\
Hidden files:                %zu\n\
SUID files:                  %zu\n\
SGID files:                  %zu\n\
Files w/capabilities:        %zu\n\
FIFO/pipes:                  %zu\n\
Sockets:                     %zu\n\
Block devices:               %zu\n\
Character devices:           %zu\n\
Symbolic links:              %zu%s\n\
Multi-link files:            %zu\n\
Files w/extended attributes: %zu\n\
Other-writable files:        %zu\n\
Sticky files:                %zu\n\
Unknown file types:          %zu\n\
Inaccessible files:          %zu\n\
"),
	(intmax_t)g_files_num, stats.dir, dir_empty, stats.reg, reg_empty,
	stats.exec, stats.hidden, stats.suid, stats.sgid, stats.caps, stats.fifo,
	stats.socket, stats.block_dev, stats.char_dev, stats.link,
	lnk_broken, stats.multi_link, stats.extended, stats.other_writable,
	stats.sticky, stats.unknown, stats.unstat);

#ifndef _BE_POSIX
# ifdef SOLARIS_DOORS
	printf(_("Doors:                 %zu\n"), stats.door);
	printf(_("Ports:                 %zu\n"), stats.port);
# endif /* SOLARIS_DOORS */
# ifdef S_ARCH1
	printf(_("Archive state 1:       %zu\n"), stats.arch1);
	printf(_("Archive state 2:       %zu\n"), stats.arch2);
# endif /* S_ARCH1 */
# ifdef S_IFWHT
	printf(_("Whiteout:              %zu\n"), stats.whiteout);
# endif /* S_IFWHT */
#endif /* _BE_POSIX */

	return FUNC_SUCCESS;
}

static int
trash_func(char **args, int *t_cont)
{
#ifndef _NO_TRASH
	if (args[1] && IS_HELP(args[1])) {
		puts(_(TRASH_USAGE));
		*t_cont = 0;
		return FUNC_SUCCESS;
	}

	int exit_status = trash_function(args);

	if (is_sel > 0 && sel_n > 0) { /* If 'tr sel', deselect everything */
		for (size_t i = sel_n; i-- > 0;)
			free(sel_elements[i].name);
		sel_n = 0;
		if (save_sel() != 0)
			exit_status = FUNC_FAILURE;
	}

	return exit_status;
#else
	UNUSED(args);
	xerror(_("%s: trash: %s\n"), PROGRAM_NAME, _(NOT_AVAILABLE));
	*t_cont = 0;
	return FUNC_FAILURE;
#endif /* !_NO_TRASH */
}

static int
untrash_func(char **args, int *u_cont)
{
#ifndef _NO_TRASH
	if (args[1] && IS_HELP(args[1])) {
		puts(_(UNTRASH_USAGE));
		*u_cont = 0;
		return FUNC_SUCCESS;
	}

	kbind_busy = 1;
	rl_attempted_completion_function = NULL;
	const int exit_status = untrash_function(args);
	rl_attempted_completion_function = my_rl_completion;
	kbind_busy = 0;

	return exit_status;
#else
	UNUSED(args);
	xerror("%s: trash: %s\n", PROGRAM_NAME, _(NOT_AVAILABLE));
	*u_cont = 0;
	return FUNC_FAILURE;
#endif /* !_NO_TRASH */
}

static int
toggle_full_dir_size(const char *arg)
{
	if (!arg || !*arg || IS_HELP(arg)) {
		puts(_(FZ_USAGE));
		return FUNC_SUCCESS;
	}

	if (*arg != 'o') {
		xerror(_("%s: '%s': Invalid argument. Try 'fz -h'\n"), PROGRAM_NAME, arg);
		return FUNC_FAILURE;
	}

	if (arg[1] == 'n' && !arg[2]) {
		conf.full_dir_size = 1;
		update_autocmd_opts(AC_FULL_DIR_SIZE);
		if (conf.autols == 1) reload_dirlist();
		print_reload_msg(NULL, NULL, _("Full directory size: on\n"));
		return FUNC_SUCCESS;
	}

	if (arg[1] == 'f' && arg[2] == 'f' && !arg[3]) {
		conf.full_dir_size = 0;
		update_autocmd_opts(AC_FULL_DIR_SIZE);
		if (conf.autols == 1) reload_dirlist();
		print_reload_msg(NULL, NULL, _("Full directory size: off\n"));
		return FUNC_SUCCESS;
	}

	xerror(_("%s: '%s': Invalid argument. Try 'fz -h'\n"), PROGRAM_NAME, arg);
	return FUNC_FAILURE;
}

static void
set_cp_cmd(char **cmd, int *cp_force)
{
	const int bk_cp_cmd = conf.cp_cmd;
	if (*cp_force == 1) {
		if (conf.cp_cmd == CP_ADVCP) {
			conf.cp_cmd = CP_ADVCP_FORCE;
		} else {
			if (conf.cp_cmd == CP_CP)
				conf.cp_cmd = CP_CP_FORCE;
		}
	}

	const char *n = NULL;

	switch (conf.cp_cmd) {
	case CP_ADVCP: n = DEFAULT_ADVCP_CMD; break;
	case CP_ADVCP_FORCE: n = DEFAULT_ADVCP_CMD_FORCE; *cp_force = 1; break;
	case CP_WCP: n = DEFAULT_WCP_CMD; break;
	case CP_RSYNC: n = DEFAULT_RSYNC_CMD; break;
	case CP_CP_FORCE: n = DEFAULT_CP_CMD_FORCE; *cp_force = 1; break;
	case CP_CP: /* fallthrough */
	default: n = DEFAULT_CP_CMD; break;
	}

	const size_t len = strlen(n) + 1;
	*cmd = xnrealloc(*cmd, len, sizeof(char));
	xstrsncpy(*cmd, n, len);

	conf.cp_cmd = bk_cp_cmd;
}

static void
set_mv_cmd(char **cmd, int *mv_force)
{
	int bk_mv_cmd = conf.mv_cmd;
	if (*mv_force == 1) {
		if (conf.mv_cmd == MV_ADVMV) {
			conf.mv_cmd = MV_ADVMV_FORCE;
		} else {
			if (conf.mv_cmd == MV_MV)
				conf.mv_cmd = MV_MV_FORCE;
		}
	}

	const char *n = NULL;

	switch (conf.mv_cmd) {
	case MV_ADVMV: n = DEFAULT_ADVMV_CMD; break;
	case MV_ADVMV_FORCE: n = DEFAULT_ADVMV_CMD_FORCE; *mv_force = 1; break;
	case MV_MV_FORCE: n = DEFAULT_MV_CMD_FORCE; *mv_force = 1; break;
	case MV_MV: /* fallthrough */
	default: n = DEFAULT_MV_CMD; break;
	}

	const size_t len = strlen(n) + 1;
	*cmd = xnrealloc(*cmd, len, sizeof(char));
	xstrsncpy(*cmd, n, len);

	conf.mv_cmd = bk_mv_cmd;
}

/* Let's check we have not left any zombie process behind. This happens
 * whenever we run a process in the background via launch_execv. */
static void
check_zombies(void)
{
	int status = 0;
	if (waitpid(-1, &status, WNOHANG) > 0 && zombies > 0)
		zombies--;
}

/* Return 1 if STR is a path, zero otherwise. */
static int
is_path(char *str)
{
	if (!str || !*str)
		return 0;

	if (strchr(str, '\\')) {
		char *p = unescape_str(str);
		if (!p)
			return 0;
		const int ret = access(p, F_OK);
		free(p);
		if (ret != 0)
			return 0;
	} else {
		if (access(str, F_OK) != 0)
			return 0;
	}

	return 1;
}

/* Replace "\S" by "S" in the memory address pointed to by s */
static void
remove_backslash(char **s)
{
	if (!*s || !*(*s))
		return;

	char *p = *s;

	while (*(*s)) {
		*(*s) = *(*s + 1);
		(*s)++;
	}

	*(*s) = '\0';

	*s = p;
}

static int
dirhist_function(char *dir)
{
	if (!dir || !*dir) {
		print_dirhist(NULL);
		return FUNC_SUCCESS;
	}

	if (IS_HELP(dir)) {
		puts(DH_USAGE);
		return FUNC_SUCCESS;
	}

	if (*dir == '!' && is_number(dir + 1)) {
		int n = xatoi(dir + 1);
		if (n <= 0 || n > dirhist_total_index) {
			xerror(_("dh: '%d': No such entry number\n"), n);
			return FUNC_FAILURE;
		}

		n--;
		if (!old_pwd[n] || *old_pwd[n] == KEY_ESC) {
			xerror("%s\n", _("dh: Invalid history entry"));
			return FUNC_FAILURE;
		}

		return cd_function(old_pwd[n], CD_PRINT_ERROR);
	}

	if (*dir != '/' || !strchr(dir + 1, '/')) {
		print_dirhist(dir);
		return FUNC_SUCCESS;
	}

	return cd_function(dir, CD_PRINT_ERROR);
}

static int
long_view_function(const char *arg)
{
	if (!arg)
		return rl_toggle_long_view(0, 0);

	if (IS_HELP(arg) || (strcmp(arg, "on") != 0 && strcmp(arg, "off") != 0)) {
		puts(LL_USAGE);
		return FUNC_SUCCESS;
	}

	conf.long_view = arg[1] == 'n' ? 1 : 0;
	update_autocmd_opts(AC_LONG_VIEW);

	if (conf.autols == 1)
		reload_dirlist();

	print_reload_msg(NULL, NULL, _("Long view: %s\n"), arg[1] == 'n'
		? _("on") : _("off"));

	return FUNC_SUCCESS;
}

/* Remove variables specificied in VAR from the environment */
static int
unset_function(char **var)
{
	if (!var || !var[0]) {
		puts(_("unset: A variable name is required"));
		return FUNC_SUCCESS;
	}

	if (IS_HELP(var[0])) {
		puts(UNSET_USAGE);
		return FUNC_SUCCESS;
	}

	int status = FUNC_SUCCESS;
	for (size_t i = 0; var[i]; i++) {
		if (unsetenv(var[i]) == -1) {
			status = errno;
			xerror("unset: '%s': %s\n", var[i], strerror(errno));
		}
	}

	return status;
}

static int
run_log_cmd(char **args)
{
	if (config_ok == 0) {
		xerror(_("%s: Log function disabled\n"), PROGRAM_NAME);
		return FUNC_FAILURE;
	}

	if (!args[0] || IS_HELP(args[0])) {
		puts(LOG_USAGE);
		return FUNC_SUCCESS;
	}

	if (*args[0] == 'c' && strcmp(args[0], "cmd") == 0) {
		if (!args[1] || strcmp(args[1], "list") == 0)
			return print_logs(CMD_LOGS);

		if (*args[1] == 's' && strcmp(args[1], "status") == 0) {
			printf(_("log: Command logs are %s\n"), (conf.log_cmds == 1)
				? _("enabled") : _("disabled"));
			return FUNC_SUCCESS;
		}

		if (*args[1] == 'o' && strcmp(args[1], "on") == 0) {
			conf.log_cmds = 1;
			puts(_("log: Command logs enabled"));
			return FUNC_SUCCESS;
		}

		if (*args[1] == 'o' && strcmp(args[1], "off") == 0) {
			conf.log_cmds = 0;
			puts(_("log: Command logs disabled"));
			return FUNC_SUCCESS;
		}

		if (*args[1] == 'c' && strcmp(args[1], "clear") == 0) {
			int ret = clear_logs(CMD_LOGS);
			if (ret == FUNC_SUCCESS)
				puts(_("log: Command logs cleared"));
			return ret;
		}
	}

	if (*args[0] == 'm' && strcmp(args[0], "msg") == 0) {
		if (!args[1] || strcmp(args[1], "list") == 0)
			return print_logs(MSG_LOGS);

		if (*args[1] == 's' && strcmp(args[1], "status") == 0) {
			printf(_("log: Message logs are %s\n"), (conf.log_msgs == 1)
				? _("enabled") : _("disabled"));
			return FUNC_SUCCESS;
		}

		if (*args[1] == 'o' && strcmp(args[1], "on") == 0) {
			conf.log_msgs = 1;
			puts(_("log: Message logs enabled"));
			return FUNC_SUCCESS;
		}

		if (*args[1] == 'o' && strcmp(args[1], "off") == 0) {
			conf.log_msgs = 0;
			puts(_("log: Message logs disabled"));
			return FUNC_SUCCESS;
		}

		if (*args[1] == 'c' && strcmp(args[1], "clear") == 0) {
			int ret = clear_logs(MSG_LOGS);
			if (ret == FUNC_SUCCESS)
				puts(_("log: Message logs cleared"));
			return ret;
		}
	}

	fprintf(stderr, "%s\n", LOG_USAGE);
	return FUNC_FAILURE;
}

/* Check the command CMD against a list of commands able to modify the
 * filesystem. Return 1 if CMD is found in the list or zero otherwise. */
static int
is_write_cmd(const char *cmd)
{
	static struct nameslist_t const wcmds[] = {
		/* Internal commands */
		{"ac", 2}, {"ad", 2}, {"bl", 2}, {"bb", 2}, {"bleach", 6},
		{"br", 2}, {"bulk", 4}, {"c", 1}, {"dup", 3}, {"l", 1},
		{"le", 2}, {"m", 1}, {"md", 2}, {"n", 1}, {"new", 3},
		{"oc", 2}, {"paste", 5}, {"pc", 2}, {"r", 1}, {"rr", 2},
		{"t", 1}, {"ta", 2}, {"td", 2}, {"tm", 2}, {"tn", 2},
		{"tu", 2}, {"ty", 2}, {"tag", 3}, {"te", 2}, {"trash", 5},
		{"u", 1}, {"undel", 5}, {"untrash", 7}, {"vv", 2},
		/* Shell commands */
		{"cp", 2}, {"rm", 2}, {"mv", 2}, {"mkdir", 5}, 	{"rmdir", 5},
		{"ln", 2}, {"link", 4}, {"unlink", 6},
#ifdef CHECK_COREUTILS /* Mostly BSD */
		{"gcp", 3}, {"grm", 3}, {"gmv", 3}, {"gmkdir", 6}, {"grmdir", 6},
		{"gln", 3}, {"glink", 5},  {"gunlink", 7},
#endif /* CHECK_COREUTILS */
		{NULL, 0}
	};

	if (!cmd || !*cmd)
		return 0;

	static size_t wcmds_n = 0;
	if (wcmds_n == 0)
		wcmds_n = (sizeof(wcmds) / sizeof(wcmds[0])) - 1;

	const size_t clen = strlen(cmd);

	for (size_t i = wcmds_n; i-- > 0;) {
		if (clen == wcmds[i].len && *cmd == *wcmds[i].name
		&& strcmp(cmd + 1, wcmds[i].name + 1) == 0) {
			xerror(_("%s: %s: Command not allowed in read-only mode\n"),
				PROGRAM_NAME, cmd);
			return 1;
		}
	}

	return 0;
}

static int
toggle_max_filename_len(const char *arg)
{
	if (arg && IS_HELP(arg)) {
		puts(KK_USAGE);
		return FUNC_SUCCESS;
	}

	return rl_toggle_max_filename_len(0, 0);
}

static int
toggle_follow_links(const char *arg)
{
	if (arg && IS_HELP(arg)) {
		puts(K_USAGE);
		return FUNC_SUCCESS;
	}

	if (conf.long_view == 0) {
		puts(_("k: Not in long view"));
		return FUNC_SUCCESS;
	}

	if (conf.light_mode == 1) {
		puts(_("k: Feature not available in light mode"));
		return FUNC_SUCCESS;
	}

	conf.follow_symlinks_long = conf.follow_symlinks_long == 1 ? 0 : 1;

	if (conf.autols == 1)
		reload_dirlist();

	print_reload_msg(NULL, NULL, _("Follow links: %s\n"),
		conf.follow_symlinks_long == 1 ? _("on") : _("off"));

	return FUNC_SUCCESS;
}

/* Handle one of c, m, vv, or paste commands.
 * Returns -1 if we should not continue, or 0 otherwise. */
static int
handle_copy_move_cmds(char ***cmd)
{
	char **args = *cmd;
	if (!args || !args[0])
		return (-1);

	int copy_and_rename = 0;
	int use_force = args[1] ? is_force_param(args[1]) : 0;

	if (args[1] && IS_HELP(args[1])) {
		puts(args[0][1] == 'v' ? _(VV_USAGE) : _(WRAPPERS_USAGE));
		return (-1);
	}

	if (args[0][0] != 'm') { /* Either c, vv, or paste commands. */
		copy_and_rename = (args[0][1] == 'v'); /* vv command. */
		set_cp_cmd(&args[0], &use_force);
	} else { /* The m command. */
		if (sel_is_last == 0 && args[1] && !args[2])
			alt_prompt = FILES_PROMPT; /* Interactive rename. */
		set_mv_cmd(&args[0], &use_force);
    }

	kbind_busy = 1;
	exit_code = cp_mv_file(args, copy_and_rename, use_force);
	kbind_busy = 0;

	return 0;
}

/* Take the command entered by the user, already splitted into substrings
 * by parse_input_str(), and call the corresponding function. Return zero
 * in case of success and one in case of error.

 * Exit status: EXIT_CODE is zero (success) by default. In case of error
 * in any of the functions below, it will be set to one (failure).
 * This will be the value returned by this function. Used by the \z
 * escape code in the prompt to print the exit status of the last
 * executed command. */
static int
exec_cmd(char **args)
{
	if (zombies > 0)
		check_zombies();
	fputs(df_c, stdout);

	if (conf.readonly == 1 && is_write_cmd(
	((*args[0] == 's' && strcmp(args[0] + 1, "udo") == 0)
	|| (*args[0] == 'd' && strcmp(args[0] + 1, "oas") == 0))
	? args[1] : args[0]) == 1)
		return FUNC_FAILURE;

	const int old_exit_code = exit_code;
	exit_code = FUNC_SUCCESS;

	int is_internal_command = 1;

	if (dir_cmds.first_cmd_in_dir == UNSET && dir_cmds.last_cmd_ignored == 0)
		dir_cmds.first_cmd_in_dir = (int)current_hist_n;

	/* Remove backslash in front of command names: used to bypass alias names */
	if (*args[0] == '\\' && *(args[0] + 1))
		remove_backslash(&args[0]);

	/* Skip comments */
	if (check_comments(args[0]) == FUNC_SUCCESS)
		return FUNC_SUCCESS;

	/* Warn when using the ',' keyword and there's no pinned file */
	if (check_pinned_file(args) == FUNC_FAILURE)
		return (exit_code = FUNC_FAILURE);

	/* User defined actions (plugins) */
	if ((exit_code = check_actions(args)) != -1)
		return exit_code;

	/* User defined variables */
	if (flags & IS_USRVAR_DEF) {
		flags &= ~IS_USRVAR_DEF;
		return (exit_code = create_usr_var(args[0]));
	}

	/* Check if we need to send input directly to the system shell. */
	if (*args[0] == ';' || *args[0] == ':')
		if ((exit_code = launch_shell(args[0])) != -1)
			return exit_code;

	/* # AUTOCD & AUTO-OPEN (1) # */
	/* rl_dispatching is set to 1 if coming from a keybind: we have a
	 * command, not a filename. So, skip this check. */
	if (rl_dispatching == 0 && (exit_code = check_auto_first(args)) != -1)
		return exit_code;

	exit_code = FUNC_SUCCESS;

	/* The more often a function is used, the more on top should it be
	 * in this if...else chain. It will be found faster this way. */

	/* ####################################################
	 * #                 BUILT-IN COMMANDS                 #
	 * ####################################################*/

	/*          ############### CD ##################     */
	if (*args[0] == 'c' && args[0][1] == 'd' && !args[0][2])
		return (exit_code = chdir_function(args[1]));

	/*         ############### OPEN ##################     */
	else if (*args[0] == 'o' && (!args[0][1] || strcmp(args[0], "open") == 0))
		return (exit_code = open_function(args));

	/*         ############## BACKDIR ##################     */
	else if (*args[0] == 'b' && args[0][1] == 'd' && !args[0][2])
		return (exit_code = backdir(args[1]));

	/*      ################ OPEN WITH ##################     */
	else if (*args[0] == 'o' && args[0][1] == 'w' && !args[0][2])
		return (exit_code = open_with_function(args));

	/*   ################ DIRECTORY JUMPER ##################     */
	else if (*args[0] == 'j' && (!args[0][1] || ((args[0][1] == 'c'
	|| args[0][1] == 'p' || args[0][1] == 'e'
	|| args[0][1] == 'l') && !args[0][2])))
		return (exit_code = dirjump(args, NO_SUG_JUMP));

	/*       ############### REFRESH ##################     */
	else if (*args[0] == 'r' && ((args[0][1] == 'f' && !args[0][2])
	|| strcmp(args[0], "refresh") == 0))
		return (exit_code = refresh_function(old_exit_code));

	/*     ################# BOOKMARKS ##################     */
	else if (*args[0] == 'b' && ((args[0][1] == 'm' && !args[0][2])
	|| strcmp(args[0], "bookmarks") == 0))
		return (exit_code = bookmarks_func(args));

	/*  ############# BACK, FORTH, and DH (dirhist) ##################     */
	else if (*args[0] == 'b' && (!args[0][1] || strcmp(args[0], "back") == 0))
		return (exit_code = back_function(args));

	else if (*args[0] == 'f' && (!args[0][1] || strcmp(args[0], "forth") == 0))
		return (exit_code = forth_function(args));

	else if (*args[0] == 'd' && args[0][1] == 'h' && !args[0][2])
		return (exit_code = dirhist_function(args[1]));

	/*    ############### BULK REMOVE ##################     */
	else if (*args[0] == 'r' && args[0][1] == 'r' && !args[0][2])
		exit_code = bulk_remove(args[1], args[1] ? args[2] : NULL);

	/*     ################# TAGS ##################     */
	else if (*args[0] == 't'
	&& (((args[0][1] == 'a' || args[0][1] == 'd' || args[0][1] == 'l'
	|| args[0][1] == 'm' || args[0][1] == 'n' || args[0][1] == 'u'
	|| args[0][1] == 'y') && !args[0][2]) || strcmp(args[0], "tag") == 0))
#ifndef _NO_TAGS
		exit_code = tags_function(args);
#else
	{
		xerror("%s: tag: %s\n", PROGRAM_NAME, NOT_AVAILABLE);
		return FUNC_FAILURE;
	}
#endif /* !_NO_TAGS */

	/*     ################# NEW FILE ######################     */
	else if (*args[0] == 'n' && (!args[0][1] || strcmp(args[0], "new") == 0))
		exit_code = create_files(args + 1, 0);

	/*     ############### DUPLICATE FILE ##################     */
	else if (*args[0] == 'd' && (!args[0][1] || strcmp(args[0], "dup") == 0))
		exit_code = dup_file(args);

	/*     ################ REMOVE FILES ###################  */
	else if (*args[0] == 'r' && !args[0][1]) {
		/* This help is only for c, m, and r commands */
		if (args[1] && IS_HELP(args[1])) {
			puts(_(WRAPPERS_USAGE));
			return FUNC_SUCCESS;
		}

		exit_code = remove_files(args);
	}

	/*    ############## CREATE DIRECTORIES ################ */
	else if (*args[0] == 'm' && args[0][1] == 'd' && !args[0][2]) {
		exit_code = create_dirs(args + 1);
	}

	/*     ############### COPY AND MOVE ##################     */
	/* Handle c, m, vv, and paste commands */
	else if (strcmp(args[0], "c") == 0 || strcmp(args[0], "m") == 0
	|| strcmp(args[0], "vv") == 0 || strcmp(args[0], "paste") == 0) {
		if (handle_copy_move_cmds(&args) != 0)
			return FUNC_SUCCESS;
	}

	/*         ############# (UN)TRASH ##################     */
	else if (*args[0] == 't' && (!args[0][1]
	|| strcmp(args[0], "trash") == 0)) {
		int t_cont = 1;
		exit_code = trash_func(args, &t_cont);
		if (t_cont == 0)
			return exit_code;
	}

	else if (*args[0] == 'u' && (!args[0][1] || strcmp(args[0], "undel") == 0
	|| strcmp(args[0], "untrash") == 0)) {
		int u_cont = 1; /* Tells whether to continue or return right here */
		exit_code = untrash_func(args, &u_cont);
		if (u_cont == 0)
			return exit_code;
	}

	/*     ############### SELECTION ##################     */
	else if (*args[0] == 's' && (!args[0][1] || strcmp(args[0], "sel") == 0)) {
		return (exit_code = sel_function(args));
	}

	else if (*args[0] == 's' && (strcmp(args[0], "sb") == 0
	|| strcmp(args[0], "selbox") == 0)) {
		list_selected_files(); return FUNC_SUCCESS;
	}

	else if (*args[0] == 'd' && (strcmp(args[0], "ds") == 0
	|| strcmp(args[0], "desel") == 0))
		return (exit_code = desel_function(args));

	/*   ############# CREATE SYMLINK ###############     */
	else if (*args[0] == 'l' && !args[0][1]) {
		exit_code = symlink_file(args + 1);
		goto CHECK_EVENTS;
	}

	else if (*args[0] == 'l' && args[0][1] == 'e' && !args[0][2]) {
		exit_code = edit_link(args[1]);
		goto CHECK_EVENTS;
	}

	/*    ########### TOGGLE LONG VIEW ##################     */
	else if (*args[0] == 'l' && (args[0][1] == 'v' || args[0][1] == 'l')
	&& !args[0][2])
		return (exit_code = long_view_function(args[1]));

	else if (*args[0] == 'k' && args[0][1] == 'k' && !args[0][2])
		return (exit_code = toggle_max_filename_len(args[1]));

	else if (*args[0] == 'k' && !args[0][1]) {
		return (exit_code = toggle_follow_links(args[1]));
	}

	/*    ############# PREVIEW FILES ##################     */
	else if (*args[0] == 'v' && strcmp(args[0], "view") == 0) {
#ifndef _NO_LIRA
		return (exit_code = preview_function(args + 1));
#else
		fprintf(stderr, "view: %s\n", _(NOT_AVAILABLE));
		return FUNC_FAILURE;
#endif /* !_NO_LIRA */
	}

	/*    ############## TOGGLE EXEC ##################     */
	else if (*args[0] == 't' && args[0][1] == 'e' && !args[0][2])
		return (exit_code = toggle_exec_func(args));

	/*    ########### OWNERSHIP CHANGER ###############     */
	else if (*args[0] == 'o' && args[0][1] == 'c' && !args[0][2])
		return (exit_code = set_file_owner(args));

	/*    ########### PERMISSIONS CHANGER ###############     */
	else if (*args[0] == 'p' && args[0][1] == 'c' && !args[0][2])
		return (exit_code = set_file_perms(args));

	/*    ############### (UN)PIN FILE ##################     */
	else if (*args[0] == 'p' && strcmp(args[0], "pin") == 0)
		return (exit_code = pin_function(args[1]));

	else if (*args[0] == 'u' && strcmp(args[0], "unpin") == 0)
		return (exit_code = unpin_dir());

	/*    ############### PROMPT ####################    */
	else if (*args[0] == 'p' && strcmp(args[0], "prompt") == 0)
		return (exit_code = prompt_function(args + 1));

	/*    ############# PROPERTIES ##################     */
	else if (*args[0] == 'p' && (!args[0][1] || strcmp(args[0], "prop") == 0
	|| strcmp(args[0], "pp") == 0))
		return (exit_code = props_function(args));

	/*     ############### SEARCH ##################     */
	else if ( (*args[0] == '/' && is_path(args[0]) == 0)
	|| (*args[0] == '/' && !args[0][1] && args[1] && IS_HELP(args[1])) )
		return (exit_code = search_function(args));

	/*    ############### BATCH LINK ##################     */
	else if (*args[0] == 'b' && args[0][1] == 'l' && !args[0][2])
		exit_code = batch_link(args + 1);

	/*    ############### BULK RENAME ##################     */
	else if (*args[0] == 'b' && ((args[0][1] == 'r' && !args[0][2])
	|| strcmp(args[0], "bulk") == 0)) {
		size_t renamed = 0;
		exit_code = bulk_rename(args, &renamed, 1);
	}

	/*      ################ SORT ##################     */
	else if (*args[0] == 's' && ((args[0][1] == 't' && !args[0][2])
	|| strcmp(args[0], "sort") == 0))
		return (exit_code = sort_func(args));

	/*    ############ FILENAMES SANITIZER ############## */
	else if (*args[0] == 'b' && ((args[0][1] == 'b' && !args[0][2])
	|| strcmp(args[0], "bleach") == 0)) {
#ifndef _NO_BLEACH
		exit_code = bleach_files(args);
#else
		xerror("%s: bleach: %s\n", PROGRAM_NAME, NOT_AVAILABLE);
		return FUNC_FAILURE;
#endif /* !_NO_BLEACH */
	}

	/*   ################ ARCHIVER ##################     */
	else if (*args[0] == 'a' && ((args[0][1] == 'c' || args[0][1] == 'd')
	&& !args[0][2])) {
#ifndef _NO_ARCHIVING
		if (!args[1] || IS_HELP(args[1])) {
			puts(_(ARCHIVE_USAGE));
			return FUNC_SUCCESS;
		}
                                  /* Either 'c' or 'd' */
		exit_code = archiver(args, args[0][1]);
#else
		xerror("%s: archiver: %s\n", PROGRAM_NAME, _(NOT_AVAILABLE));
		return FUNC_FAILURE;
#endif /* !_NO_ARCHIVING */
	}

	/* ##################################################
	 * #                 MINOR FUNCTIONS                #
	 * ##################################################*/

	else if (*args[0] == 'w' && args[0][1] == 's' && !args[0][2])
		return (exit_code = handle_workspaces(args + 1));

	else if (*args[0] == 's' && strcmp(args[0], "stats") == 0)
		return (exit_code = print_stats());

	else if (*args[0] == 'f' && ((args[0][1] == 't' && !args[0][2])
	|| strcmp(args[0], "filter") == 0))
		return (exit_code = filter_function(args[1]));

	else if (*args[0] == 'a' && strcmp(args[0], "auto") == 0)
		return (exit_code = add_autocmd(args + 1));

	else if (*args[0] == 'f' && args[0][1] == 'z' && !args[0][2])
		return (exit_code = toggle_full_dir_size(args[1]));

	else if (*args[0] == 'c' && ((args[0][1] == 'l' && !args[0][2])
	|| strcmp(args[0], "columns") == 0))
		return (exit_code = columns_function(args[1]));

	else if (*args[0] == 'i' && strcmp(args[0], "icons") == 0)
		return (exit_code = icons_function(args[1]));

	else if (*args[0] == 'c' && ((args[0][1] == 's' && !args[0][2])
	|| strcmp(args[0], "colorschemes") == 0))
		return (exit_code = cschemes_function(args));

	else if (*args[0] == 'k' && ((args[0][1] == 'b' && !args[0][2])
	|| strcmp(args[0], "keybinds") == 0))
		return (exit_code = kbinds_function(args));

	else if (*args[0] == 'e' && strcmp(args[0], "exp") == 0)
		return (exit_code = export_files_function(args));

	else if (*args[0] == 'o' && strcmp(args[0], "opener") == 0)
		return (exit_code = opener_function(args[1]));

	else if (*args[0] == 't' && strcmp(args[0], "tips") == 0)
		{ print_tips(1); return FUNC_SUCCESS; }

	else if (*args[0] == 'a' && strcmp(args[0], "actions") == 0)
		return (exit_code = actions_function(args));

	else if (*args[0] == 'l' && args[0][1] == 'm' && !args[0][2])
		return (exit_code = lightmode_function(args[1]));

	else if (*args[0] == 'r' && ((args[0][1] == 'l' && !args[0][2])
	|| strcmp(args[0], "reload") == 0))
		return (exit_code = config_reload(args[1]));

	/* #### NEW INSTANCE #### */
	else if ((*args[0] == 'x' || *args[0] == 'X') && !args[0][1])
		return (exit_code = new_instance_function(args));

	/* #### NET #### */
	else if (*args[0] == 'n' && (strcmp(args[0], "net") == 0))
		return (exit_code = remotes_function(args));

	/* #### MIME #### */
	else if (*args[0] == 'm' && ((args[0][1] == 'm' && !args[0][2])
	|| strcmp(args[0], "mime") == 0))
		return (exit_code = lira_function(args));

	else if (conf.autols == 0 && *args[0] == 'l' && args[0][1] == 's'
	&& !args[0][2])
		return (exit_code = ls_function());

	/* #### PROFILE #### */
	else if (*args[0] == 'p' && ((args[0][1] == 'f' && !args[0][2])
	|| strcmp(args[0], "profile") == 0))
#ifndef _NO_PROFILES
		return (exit_code = profile_function(args));
#else
		{ xerror("%s: profiles: %s\n", PROGRAM_NAME, NOT_AVAILABLE);
		return FUNC_FAILURE; }
#endif /* !_NO_PROFILES */

	/* #### MOUNTPOINTS #### */
	else if (*args[0] == 'm' && ((args[0][1] == 'p' && !args[0][2])
	|| strcmp(args[0], "mountpoints") == 0)) {
#ifndef NO_MEDIA_FUNC
		return (exit_code = media_function(args[1], MEDIA_LIST));
#else
		fputs(_("mountpoints: Function not available\n"), stderr);
		return FUNC_FAILURE;
#endif /* NO_MEDIA_FUNC */
	}

	/* #### MEDIA #### */
	else if (*args[0] == 'm' && strcmp(args[0], "media") == 0) {
#ifndef NO_MEDIA_FUNC
		return (exit_code = media_function(args[1], MEDIA_MOUNT));
#else
		fputs(_("media: Function not available\n"), stderr);
		return FUNC_FAILURE;
#endif /* NO_MEDIA_FUNC */
	}

	/* #### MAX FILES #### */
	else if (*args[0] == 'm' && args[0][1] == 'f' && !args[0][2])
		return set_max_files(args);

	/* #### EXT #### */
	else if (*args[0] == 'e' && args[0][1] == 'x' && args[0][2] == 't'
	&& !args[0][3])
		return (exit_code = ext_cmds_function(args[1]));

	/* #### PAGER #### */
	else if (*args[0] == 'p' && ((args[0][1] == 'g' && !args[0][2])
	|| strcmp(args[0], "pager") == 0))
		return (exit_code = pager_function(args[1]));

	/* #### FILE COUNTER #### */
	else if (*args[0] == 'f' && ((args[0][1] == 'c' && !args[0][2])
	|| strcmp(args[0], "filecounter") == 0))
		return (exit_code = file_counter_function(args[1]));

	/* #### DIRECTORIES FIRST #### */
	else if ((*args[0] == 'f' && args[0][1] == 'f' && !args[0][2])
	|| (*args[0] == 'd' && strcmp(args[0], "dirs-first") == 0))
		return (exit_code = dirs_first_function(args[1]));

	/* #### LOG #### */
	else if (*args[0] == 'l' && strcmp(args[0], "log") == 0)
		return (exit_code = run_log_cmd(args + 1));

	/* #### MESSAGES #### */
	else if (*args[0] == 'm' && (strcmp(args[0], "msg") == 0
	|| strcmp(args[0], "messages") == 0))
		return (exit_code = msgs_function(args[1]));

	/* #### ALIASES #### */
	else if (*args[0] == 'a' && strcmp(args[0], "alias") == 0)
		return (exit_code = alias_function(args));

	/* #### CONFIG #### */
	else if (*args[0] == 'c' && strcmp(args[0], "config") == 0)
		return (exit_code = config_edit(args));

	/* #### HISTORY #### */
	else if (*args[0] == 'h' && strcmp(args[0], "history") == 0)
		return (exit_code = history_function(args));

	/* #### HIDDEN FILES #### */
	else if (*args[0] == 'h' && (((args[0][1] == 'f' || args[0][1] == 'h')
	&& !args[0][2]) || strcmp(args[0], "hidden") == 0))
		return (exit_code = hidden_files_function(args[1]));

	/* #### AUTOCD #### */
	else if (*args[0] == 'a' && (strcmp(args[0], "acd") == 0
	|| strcmp(args[0], "autocd") == 0))
		return (exit_code = autocd_function(args[1]));

	/* #### AUTO-OPEN #### */
	else if (*args[0] == 'a' && ((args[0][1] == 'o' && !args[0][2])
	|| strcmp(args[0], "auto-open") == 0))
		return (exit_code = auto_open_function(args[1]));

	/* #### COMMANDS #### */
	else if (*args[0] == 'c' && (strcmp(args[0], "cmd") == 0
	|| strcmp(args[0], "commands") == 0))
		return (exit_code = list_commands());

	/* #### PATH, CWD #### */
	else if ((*args[0] == 'p' && (strcmp(args[0], "pwd") == 0
	|| strcmp(args[0], "path") == 0)) || strcmp(args[0], "cwd") == 0) {
		return (exit_code = pwd_function(args[1]));
	}

	/* #### HELP #### */
	else if ((*args[0] == '?' && !args[0][1]) || strcmp(args[0], "help") == 0) {
		return (exit_code = quick_help(args[1]));
	}

	/* #### EXPORT #### */
	else if (*args[0] == 'e' && strcmp(args[0], "export") == 0) {
		return (exit_code = export_var_function(args + 1));
	}

	/* #### UMASK #### */
	else if (*args[0] == 'u' && strcmp(args[0], "umask") == 0) {
		return (exit_code = umask_function(args[1]));
	}

	/* #### UNSET #### */
	else if (*args[0] == 'u' && strcmp(args[0], "unset") == 0) {
		return (exit_code = unset_function(args + 1));
	}

	/* These functions just print stuff, so that the value of exit_code
	 * is always zero, that is to say, success. */
	else if (*args[0] == 'c' && strcmp(args[0], "colors") == 0) {
		colors_function(args[1]); return FUNC_SUCCESS;
	}

	else if (*args[0] == 'v' && (strcmp(args[0], "ver") == 0
	|| strcmp(args[0], "version") == 0)) {
		version_function(1); return FUNC_SUCCESS;
	}

	else if (*args[0] == 'b' && strcmp(args[0], "bonus") == 0) {
		bonus_function(); return FUNC_SUCCESS;
	}

	/* #### QUIT #### */
	else if ((*args[0] == 'q' && (!args[0][1] || strcmp(args[0], "quit") == 0))
	|| (*args[0] == 'e' && strcmp(args[0], "exit") == 0))
		quit_func(args, old_exit_code);

	else {
		/* #  AUTOCD & AUTO-OPEN (2) # */
		if ((exit_code = check_auto_second(args)) != -1)
			return exit_code;

		/* #  EXTERNAL/SHELL COMMANDS # */
		if ((exit_code = run_shell_cmd(args)) == FUNC_FAILURE)
			return FUNC_FAILURE;
		else
			is_internal_command = 0;
	}

CHECK_EVENTS:
	check_fs_events(is_internal_command);

	return exit_code;
}

/* Run the command CMD and, if the '\b' prompt code is used, store execution
 * time in last_cmd_time (global). */
int
exec_cmd_tm(char **cmd)
{
	struct timespec begin, end;
	int reta = -1;

	if (conf.prompt_b_is_set == 1)
		reta = clock_gettime(CLOCK_MONOTONIC, &begin);

	const int ret = exec_cmd(cmd);

	if (conf.prompt_b_is_set == 1) {
		const int retb = clock_gettime(CLOCK_MONOTONIC, &end);

		if (reta == -1 || retb == -1) {
			last_cmd_time = 0.0;
			return ret;
		}

		last_cmd_time =
			(double)(end.tv_sec - begin.tv_sec) +
			(double)(end.tv_nsec - begin.tv_nsec) / 1000000000.0;
	}

	return ret;
}

static void
run_chained_cmd(char **cmd, size_t *err_code)
{
	size_t i;
	char **alias_cmd = check_for_alias(cmd);
	if (alias_cmd) {
		if (exec_cmd_tm(alias_cmd) != 0)
			*err_code = 1;
		for (i = 0; alias_cmd[i]; i++)
			free(alias_cmd[i]);
		free(alias_cmd);
	} else {
		if ((flags & FAILED_ALIAS) || exec_cmd_tm(cmd) != 0)
			*err_code = 1;
		flags &= ~FAILED_ALIAS;
		for (i = 0; i <= args_n; i++)
			free(cmd[i]);
		free(cmd);
	}
}

/* Execute chained commands (cmd1;cmd2 and/or cmd1 && cmd2). The function
 * is called by parse_input_str() if some non-quoted double ampersand or
 * semicolon is found in the input string AND at least one of these
 * chained commands is internal */
void
exec_chained_cmds(char *cmd)
{
	if (!cmd)
		return;

	size_t i = 0;
	const size_t cmd_len = strlen(cmd);

	for (i = 0; i < cmd_len; i++) {
		char *str = NULL;
		size_t len = 0, cond_exec = 0, error_code = 0;

		/* Get command */
		str = xcalloc(strlen(cmd) + 1, sizeof(char));
		while (cmd[i] && cmd[i] != '&' && cmd[i] != ';')
			str[len++] = cmd[i++];

		if (!*str) {
			free(str);
			continue;
		}

		if (cmd[i] == '&')
			cond_exec = 1;

		char **tmp_cmd = parse_input_str((*str == ' ') ? str + 1 : str);
		free(str);

		if (!tmp_cmd)
			continue;

		run_chained_cmd(tmp_cmd, &error_code);

		/* Do not continue if the execution was condtional and
		 * the previous command failed. */
		if (cond_exec == 1 && error_code == 1)
			break;
	}
}

static void
run_profile_line(char *cmd)
{
	if (xargs.secure_cmds == 1
	&& sanitize_cmd(cmd, SNT_PROFILE) != FUNC_SUCCESS)
		return;

	args_n = 0;
	char **cmds = parse_input_str(cmd);
	if (!cmds)
		return;

	no_log = 1;
	exec_cmd(cmds);
	no_log = 0;

	for (size_t i = args_n + 1; i-- > 0;)
		free(cmds[i]);
	free(cmds);

	args_n = 0;
}

void
exec_profile(void)
{
	if (config_ok == 0 || !profile_file)
		return;

	FILE *fp = fopen(profile_file, "r");
	if (!fp)
		return;

	size_t line_size = 0;
	char *line = NULL;
	ssize_t line_len = 0;

	while ((line_len = getline(&line, &line_size, fp)) > 0) {
		if (!*line || *line == '\n' || *line == '#')
			continue;

		if (line[line_len - 1] == '\n')
			line[line_len - 1] = '\0';

		if (conf.int_vars == 1 && strchr(line, '=') && !IS_DIGIT(*line))
			create_usr_var(line);
		else
			run_profile_line(line);
	}

	free(line);
	fclose(fp);
}
