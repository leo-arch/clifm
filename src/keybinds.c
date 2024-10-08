/* keybinds.c -- functions keybindings configuration */

/*
 * This file is part of CliFM
 *
 * Copyright (C) 2016-2024, L. Abramovich <leo.clifm@outlook.com>
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

#include <unistd.h>

#ifdef __NetBSD__
# include <string.h>
#endif /* __NetBSD__ */

#include <errno.h>

#if defined(MAC_OS_X_RENAMEAT_SYS_STDIO_H)
# include <sys/stdio.h> /* renameat(2) */
#endif /* MAC_OS_X_RENAMEAT_SYS_STDIO_H */

#include "aux.h"
#include "config.h"
#include "exec.h"
#include "file_operations.h"
#include "keybinds.h"
#include "listing.h"
#include "messages.h"
#include "misc.h"
#include "profiles.h"
#include "prompt.h"
#include "readline.h"
#include "spawn.h"
#include "tabcomp.h" /* tab_complete() */

#ifndef _NO_SUGGESTIONS
# include "suggestions.h"
#endif /* !_NO_SUGGESTIONS */

#ifndef _NO_HIGHLIGHT
# include "highlight.h"
#endif /* !_NO_HIGHLIGHT */

#include "strings.h" /* quote_str() */

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
	int exit_status = FUNC_SUCCESS;
	struct stat attr;

	if (!kbinds_file || !*kbinds_file) {
		xerror(_("kb: No keybindings file found\n"));
		return FUNC_FAILURE;
	}

	if (stat(kbinds_file, &attr) == -1) {
		exit_status = create_kbinds_file();
	} else {
		if (unlink(kbinds_file) == 0) {
			exit_status = create_kbinds_file();
		} else {
			xerror("kb: '%s': %s\n", kbinds_file, strerror(errno));
			exit_status = FUNC_FAILURE;
		}
	}

	if (exit_status == FUNC_SUCCESS)
		err('n', PRINT_PROMPT, _("kb: Restart %s for changes "
			"to take effect\n"), PROGRAM_NAME);

	return exit_status;
}

static int
kbinds_edit(char *app)
{
	if (xargs.stealth_mode == 1) {
		printf("%s: kb: %s\n", PROGRAM_NAME, STEALTH_DISABLED);
		return FUNC_SUCCESS;
	}

	if (!kbinds_file) {
		xerror(_("kb: No keybindings file found\n"));
		return FUNC_FAILURE;
	}

	struct stat attr;
	if (stat(kbinds_file, &attr) == -1) {
		create_kbinds_file();
		stat(kbinds_file, &attr);
	}

	const time_t mtime_bfr = attr.st_mtime;

	const int ret = open_config_file(app, kbinds_file);
	if (ret != FUNC_SUCCESS)
		return ret;

	stat(kbinds_file, &attr);
	if (mtime_bfr == attr.st_mtime)
		return FUNC_SUCCESS;

	err('n', PRINT_PROMPT, _("kb: Restart %s for changes to "
			"take effect\n"), PROGRAM_NAME);
	return FUNC_SUCCESS;
}

/* Check all readline key bindings against all of clifm keybindings.
 * Return FUNC_FAILURE if at least one conflict is found. Otherwise,
 * return FUNC_SUCCESS. */
static int
check_rl_kbinds_all(void)
{
	size_t i;
	char *name = (char *)NULL;
	char **names = (char **)rl_funmap_names();
	int conflict = 0;

	for (i = 0; (name = names[i]); i++) {
		rl_command_func_t *function = rl_named_function(name);
		char **keys = rl_invoking_keyseqs(function);
		if (!keys)
			continue;

		size_t j;
		for (j = 0; keys[j]; j++) {
			size_t k;
			for (k = 0; k < kbinds_n; k++) {
				if (strcmp(kbinds[k].key, keys[j]) == 0) {
					fprintf(stderr, _("kb: '%s' conflicts with '%s' "
						"(readline)\n"), kbinds[k].function, name);
					conflict++;
				}
			}
			free(keys[j]);
		}
		free(keys);
	}

	free(names);

	int ret = conflict > 0 ? FUNC_FAILURE : FUNC_SUCCESS;

	if (ret == FUNC_FAILURE)
		fprintf(stderr, _("Edit '~/.config/clifm/readline.clifm' "
			"to fix the conflicts.\n"));

	return ret;
}

/* Check the key sequence KB against the list of readline's key sequences.
 * Return FUNC_FAILURE in case of conflict, or FUNC_SUCCESS. */
static int
check_rl_kbinds(const char *kb)
{
	size_t i;
	char *name = (char *)NULL;
	char **names = (char **)rl_funmap_names();
	int conflict = 0;

	for (i = 0; (name = names[i]); i++) {
		rl_command_func_t *function = rl_named_function(name);
		char **keys = rl_invoking_keyseqs(function);
		if (!keys)
			continue;

		size_t j;
		for (j = 0; keys[j]; j++) {
			if (conflict == 0 && strcmp(kb, keys[j]) == 0) {
				fprintf(stderr, _("kb: Key already in use by '%s' "
					"(readline)\nEdit '~/.config/clifm/readline.clifm' "
					"to fix the conflict.\nYou can either unset the "
					"key sequence:\n  \"%s\":\nor set the function to a "
					"different key sequence:\n  \"\\e\\C-p\": %s\n"),
					name, kb, name);
				conflict = 1;
			}
			free(keys[j]);
		}
		free(keys);
	}

	free(names);

	return conflict == 1 ? FUNC_FAILURE : FUNC_SUCCESS;
}

static int
check_kbinds_conflict(void)
{
	if (kbinds_n == 0) {
		puts(_("kb: No keybindings defined"));
		return FUNC_SUCCESS;
	}

	int ret = FUNC_SUCCESS;
	size_t i, j;
	for (i = 0; i < kbinds_n; i++) {
		for (j = i + 1; j < kbinds_n; j++) {
			if (strcmp(kbinds[i].key, kbinds[j].key) == 0) {
				fprintf(stderr, _("kb: '%s' conflicts with '%s'\n"),
					kbinds[i].function, kbinds[j].function);
				ret = FUNC_FAILURE;
			}
		}
	}

	if (ret == FUNC_FAILURE)
		puts(_("Run 'kb edit' to fix the conflict"));
	else {
		ret = check_rl_kbinds_all();
		if (ret == FUNC_SUCCESS)
			puts(_("kb: No conflicts found"));
	}

	return ret;
}

/* Retrieve the key sequence associated to FUNCTION */
static char *
find_key(const char *function)
{
	if (kbinds_n == 0)
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

/* Read a key sequence from STDIN a return its value. */
static char *
get_new_keybind(void)
{
	const size_t buf_size = NAME_MAX;
	char *buf = xnmalloc(buf_size + 1, sizeof(char));
	size_t len = 0;
	int ret = 0;
	int result;
	int ch = 0;

	buf[0] = '\0';

	putchar(':');
	fflush(stdout);

	if (enable_raw_mode(STDIN_FILENO) == -1) {
		UNHIDE_CURSOR;
		free(buf);
		return (char *)NULL;
	}

	while (1) {
		result = (int)read(STDIN_FILENO, &ch, sizeof(unsigned char)); /* flawfinder: ignore */
		if (result == 0 || len >= buf_size)
			break;

		if (result != sizeof(unsigned char))
			continue;

		const unsigned char c = (unsigned char)ch;

		if (c == KEY_ENTER)
			break;

		if (c == CTRL('D')) {
			buf[0] = '\0';
			break;
		}

		if (c == CTRL('C')) {
			putchar('\r');
			ERASE_TO_RIGHT;
			putchar(':');
			fflush(stdout);
			buf[0] = '\0';
			len = 0;
			continue;
		}

		if (c == KEY_ESC) {
			ret = snprintf(buf + len, sizeof(buf), "\\e");
		} else if (isprint(c) && c != ' ') {
			ret = snprintf(buf + len, sizeof(buf), "%c", c);
		} else {
			if (c <= 31) {
				ret = snprintf(buf + len, sizeof(buf),
					"\\C-%c", c + '@' - 'A' + 'a');
			} else {
				ret = snprintf(buf + len, sizeof(buf), "\\x%x", c);
			}
		}

		if (ret < 0)
			continue;

		if (*buf) {
			fputs(buf + len, stdout);
			fflush(stdout);
		}

		len += (size_t)ret;
	}

	disable_raw_mode(STDIN_FILENO);
	putchar('\n');

	if (!*buf) {
		free(buf);
		return (char *)NULL;
	}

	return buf;
}

/* Append the key sequence KB associated to the function name FUNC_NAME
 * to the keybingins file. */
static int
append_kb_to_file(const char *func_name, const char *kb)
{
	FILE *fp = open_fappend(kbinds_file);
	if (!fp) {
		xerror(_("kb: Cannot open '%s': %s\n"), kbinds_file, strerror(errno));
		return FUNC_FAILURE;
	}

	fprintf(fp, "\n%s:%s\n", func_name, kb);
	fclose(fp);

	return FUNC_SUCCESS;
}

/* Edit the keybindings file and update the key sequence bound to FUNC_NAME
 * with the key sequence KB. */
static int
rebind_kb(const char *func_name, const char *kb)
{
	int orig_fd = 0;
	FILE *orig_fp = open_fread(kbinds_file, &orig_fd);
	if (!orig_fp) {
		xerror(_("kb: Cannot open '%s': %s\n"), kbinds_file, strerror(errno));
		return FUNC_FAILURE;
	}

	const size_t len = strlen(config_dir_gral) + (sizeof(TMP_FILENAME) - 1) + 2;
	char *tmp_name = xnmalloc(len, sizeof(char));
	snprintf(tmp_name, len, "%s/%s", config_dir_gral, TMP_FILENAME);

	int tmp_fd = mkstemp(tmp_name);
	if (tmp_fd == -1) {
		xerror(_("kb: Error creating temporary file: %s\n"), strerror(errno));
		goto ERROR;
	}

	FILE *tmp_fp = fdopen(tmp_fd, "w");
	if (!tmp_fp) {
		xerror(_("kb: Cannot open temporary file: %s\n"), strerror(errno));
		unlinkat(tmp_fd, tmp_name, 0);
		close(tmp_fd);
		goto ERROR;
	}

	int found = 0;
	const size_t func_len = strlen(func_name);
	char line[NAME_MAX];
	while (fgets(line, sizeof(line), orig_fp) != NULL) {
		if (found == 0 && strncmp(line, func_name, func_len) == 0
		&& line[func_len] == ':') {
			fprintf(tmp_fp, "%s:%s\n", func_name, kb);
			found = 1;
		} else {
			fputs(line, tmp_fp);
		}
	}

	if (found == 1) {
		if (renameat(tmp_fd, tmp_name, orig_fd, kbinds_file) == -1)
			xerror("kb: Cannot rename '%s' to '%s': %s\n",
				tmp_name, kbinds_file, strerror(errno));
	} else {
		unlinkat(tmp_fd, tmp_name, 0);
	}

	fclose(orig_fp);
	fclose(tmp_fp);

	free(tmp_name);

	if (found == 0)
		return append_kb_to_file(func_name, kb);

	return FUNC_SUCCESS;

ERROR:
	free(tmp_name);
	fclose(orig_fp);
	return FUNC_FAILURE;
}

/* Check the key sequence KB against the list of clifm's key sequences.
 * Return FUNC_FAILURE in case of conflict, or FUNC_SUCCESS. */
static int
check_func_name(const char *func_name)
{
	size_t len = strlen(func_name);
	size_t i;
	for (i = 0; kb_cmds[i].name; i++) {
		if (len == kb_cmds[i].len && *kb_cmds[i].name == *func_name
		&& strcmp(kb_cmds[i].name, func_name) == 0)
			return FUNC_SUCCESS;
	}

	return FUNC_FAILURE;
}

/* Check the key sequence KB against both clifm and readline key sequences.
 * Return FUNC_FAILURE in case of conflict, or FUNC_SUCCESS. */
static int
validate_new_kb(const char *kb)
{
	size_t i;
	for (i = 0; i < kbinds_n; i++) {
		if (kbinds[i].key && strcmp(kb, kbinds[i].key) == 0) {
			fprintf(stderr, _("kb: Key already in use by '%s'\n"),
				kbinds[i].function);
			return FUNC_FAILURE;
		}
	}

	if (check_rl_kbinds(kb) == FUNC_FAILURE)
		return FUNC_FAILURE;

	if (!strchr(kb, '\\')) {
		fprintf(stderr, _("kb: Invalid key binding\n"));
		return FUNC_FAILURE;
	}

	return FUNC_SUCCESS;
}

/* Bind the function FUNC_NAME to a new key sequence. */
static int
bind_kb_func(const char *func_name)
{
	if (xargs.stealth_mode == 1) {
		printf("%s: kb: %s\n", PROGRAM_NAME, STEALTH_DISABLED);
		return FUNC_SUCCESS;
	}

	if (!kbinds_file || !*kbinds_file) {
		xerror(_("kb: No keybindings file found\n"));
		return FUNC_FAILURE;
	}

	if (!func_name || !*func_name) {
		puts(KB_USAGE);
		return FUNC_SUCCESS;
	}

	if (check_func_name(func_name) == FUNC_FAILURE) {
		xerror(_("kb: '%s': Invalid function name\nType 'kb bind <TAB>' "
			"to list available function names\n"), func_name);
		return FUNC_FAILURE;
	}

	struct stat a;
	if (stat(kbinds_file, &a) == -1 && create_kbinds_file() == FUNC_FAILURE)
		return FUNC_FAILURE;

	const char *cur_key = find_key(func_name);
	printf(_("Enter a key binding for '%s' (current: %s)\n"),
		func_name, cur_key ? cur_key : "unset");
	puts(_("(Enter:accept, Ctrl-d:abort, Ctrl-c:clear-line)"));
	puts(_("To unset the function enter '-'"));

	char *kb = get_new_keybind();
	if (kb == NULL)
		return FUNC_SUCCESS;

	if (validate_new_kb(kb) == FUNC_FAILURE) {
		free(kb);
		return FUNC_FAILURE;
	}

//	printf(_("Current key: %s\n"), cur_key ? cur_key : "unset");
	printf(_("New key: %s\n"), kb);
	if (rl_get_y_or_n(_("Bind to this new key? [y/n] ")) == 0) {
		free(kb);
		return FUNC_SUCCESS;
	}

	const int ret = rebind_kb(func_name, kb);
	free(kb);

	if (ret == FUNC_SUCCESS) {
		err('n', PRINT_PROMPT, _("kb: Restart %s for changes to "
			"take effect\n"), PROGRAM_NAME);
	}

	return ret;
}

int
kbinds_function(char **args)
{
	if (!args)
		return FUNC_FAILURE;

	if (!args[1] || strcmp(args[1], "list") == 0) {
		if (kbinds_n == 0) {
			puts(_("kb: No keybindings defined\n"));
			return FUNC_SUCCESS;
		}
		size_t i;
		for (i = 0; i < kbinds_n; i++)
			printf("%s: %s\n", kbinds[i].key, kbinds[i].function);

		return FUNC_SUCCESS;
	}

	if (IS_HELP(args[1])) {
		puts(_(KB_USAGE));
		return FUNC_SUCCESS;
	}

	if (*args[1] == 'b' && strcmp(args[1], "bind") == 0)
		return bind_kb_func(args[2]);

	if (*args[1] == 'c' && strcmp(args[1], "conflict") == 0)
		return check_kbinds_conflict();

	if (*args[1] == 'e' && strcmp(args[1], "edit") == 0)
		return kbinds_edit(args[2] ? args[2] : NULL);

	if (*args[1] == 'r' && strcmp(args[1], "reset") == 0)
		return kbinds_reset();

	if (*args[1] == 'r' && strcmp(args[1], "readline") == 0) {
		rl_function_dumper(1);
		return FUNC_SUCCESS;
	}

	fprintf(stderr, "%s\n", _(KB_USAGE));
	return FUNC_FAILURE;
}

/*
static void
copy_key(char *buf, const size_t buf_size, char *ptr, size_t *len)
{
	if (strcasecmp(ptr, "control") == 0 || strcasecmp(ptr, "ctrl") == 0) {
		xstrsncpy(buf + *len, "\\C", buf_size); *len += 2;
	} else if (strcasecmp(ptr, "alt") == 0 || strcasecmp(ptr, "meta") == 0) {
		xstrsncpy(buf + *len, "\\M", buf_size); *len += 2;
	} else if (strcasecmp(ptr, "del") == 0 || strcasecmp(ptr, "rubout") == 0) {
		xstrsncpy(buf + *len, "\\d", buf_size); *len += 2;
	} else if (strcasecmp(ptr, "esc") == 0 || strcasecmp(ptr, "escape") == 0) {
		xstrsncpy(buf + *len, "\\e", buf_size); *len += 2;
	} else if (strcasecmp(ptr, "spc") == 0 || strcasecmp(ptr, "space") == 0) {
		xstrsncpy(buf + *len, "\\ ", buf_size); *len += 2;
	} else if (strcasecmp(ptr, "ret") == 0 || strcasecmp(ptr, "return") == 0) {
		xstrsncpy(buf + *len, "\\r", buf_size); *len += 2;
	} else if (strcasecmp(ptr, "lfd") == 0 || strcasecmp(ptr, "newline") == 0) {
		xstrsncpy(buf + *len, "\\n", buf_size); *len += 2;
	} else if (strcasecmp(ptr, "tab") == 0) {
		xstrsncpy(buf + *len, "\\t", buf_size); *len += 2;
	} else {
		xstrsncpy(buf + *len, ptr, buf_size); *len += strlen(ptr);
	}
}

static char *
save_key(char *key)
{
	if (!key || !*key)
		return (char *)NULL;

	if (*key == '\\')
		return savestring(key, strlen(key));

	char buf[NAME_MAX]; *buf = '\0';
	char *ptr = key;
	char *dash = (char *)NULL;
	size_t len = 0;

	while (len < sizeof(buf) && *ptr && (dash = strchr(ptr, '-')) && dash[1]) {
		*dash = '\0';

		copy_key(buf, sizeof(buf), ptr, &len);

		ptr = dash + 1;
		buf[len] = '-';
		len++;
		*dash = '-';
	}

	if (*ptr)
		copy_key(buf, sizeof(buf), ptr, &len);
	else
		buf[len - (len > 0)] = '\0';

	return savestring(buf, strlen(buf));
} */

/* Store keybinds from the keybinds file into a struct */
int
load_keybinds(void)
{
	if (config_ok == 0 || !kbinds_file)
		return FUNC_FAILURE;

	/* Free the keybinds struct array */
	if (kbinds_n > 0) {
		int i = (int)kbinds_n;

		while (--i >= 0) {
			free(kbinds[i].function);
			free(kbinds[i].key);
		}

		free(kbinds);
		kbinds = xnmalloc(1, sizeof(struct kbinds_t));
		kbinds_n = 0;
	}

	/* Open the keybinds file */
	FILE *fp = fopen(kbinds_file, "r");
	if (!fp)
		return FUNC_FAILURE;

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
		kbinds = xnrealloc(kbinds, kbinds_n + 1, sizeof(struct kbinds_t));
/*		kbinds[kbinds_n].key = save_key(tmp + 1); */
		kbinds[kbinds_n].key = savestring(tmp + 1, strlen(tmp + 1));

		*tmp = '\0';

		kbinds[kbinds_n].function = savestring(line, strlen(line));
		kbinds_n++;
	}

	fclose(fp);
	free(line);
	return FUNC_SUCCESS;
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

#if defined(__HAIKU__) || !defined(_NO_PROFILES)
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
#endif /* __HAIKU__ || !_NO_PROFILES */

/* Runs any command recognized by Clifm via a keybind. Example:
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

	int exit_status = FUNC_FAILURE;
	char **cmd = parse_input_str(str);
	putchar('\n');

	if (cmd) {
		exit_status = exec_cmd_tm(cmd);

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
		return FUNC_FAILURE;

	if (kbind_busy == 1)
		return FUNC_SUCCESS;

#ifndef _NO_SUGGESTIONS
	if (conf.colorize == 1 && wrong_cmd == 1)
		recover_from_wrong_cmd();
#endif /* _NO_SUGGESTIONS */

	int exit_code_bk = exit_code;

	keybind_exec_cmd(cmd);
	rl_reset_line_state();

	if (exit_code != exit_code_bk)
		/* The exit code was changed by the executed command. Force the
		 * input taking function (my_rl_getc) to update the value of
		 * prompt_offset to correctly calculate the cursor position. */
		prompt_offset = UNSET;

	return FUNC_SUCCESS;
}

int
rl_toggle_max_filename_len(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1)
		return FUNC_SUCCESS;

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
	return FUNC_SUCCESS;
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
			s = xnmalloc(len + 2, sizeof(char));
			snprintf(s, len + 2, "%s ", t);
			len++;
		} else {
			s = t;
			free_s = 0;
		}
	} else {
		len = strlen(DEF_SUDO_CMD) + 1;
		s = xnmalloc(len + 1, sizeof(char));
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

	return FUNC_SUCCESS;
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

		char q[PATH_MAX + 1];
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
		return FUNC_SUCCESS;
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
		return FUNC_SUCCESS;
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
	case WS_PREFIX_SUG: /* fallthrough */
	case WS_NUM_PREFIX_SUG:
	case BM_PREFIX_SUG: {
		char prefix[3]; prefix[0] = '\0';
		if (suggestion.type == TAGC_SUG) {
			prefix[0] = ':'; prefix[1] = '\0';
		} else if (suggestion.type == TAGT_SUG) {
			prefix[0] = 't'; prefix[1] = ':'; prefix[2] = '\0';
		} else if (suggestion.type == BM_PREFIX_SUG) {
			prefix[0] = 'b'; prefix[1] = ':'; prefix[2] = '\0';
		} else if (suggestion.type == WS_PREFIX_SUG
		|| suggestion.type == WS_NUM_PREFIX_SUG) {
			prefix[0] = 'w'; prefix[1] = ':'; prefix[2] = '\0';
		}

		rl_insert_text(prefix);
		char *p = (suggestion.type != BM_PREFIX_SUG
			&& suggestion.type != WS_PREFIX_SUG)
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
	return FUNC_SUCCESS;
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
		return FUNC_SUCCESS;

	exec_prompt_cmds = 1;
	return run_kb_cmd("cd ..");
}

static int
rl_dir_root(int count, int key)
{
	UNUSED(count); UNUSED(key);
	/* If already root dir, do nothing */
	if (*workspaces[cur_ws].path == '/' && !workspaces[cur_ws].path[1])
		return FUNC_SUCCESS;

	return run_kb_cmd("cd /");
}

static int
rl_dir_home(int count, int key)
{
	UNUSED(count); UNUSED(key);
	/* If already in home, do nothing */
	if (*workspaces[cur_ws].path == *user.home
	&& strcmp(workspaces[cur_ws].path, user.home) == 0)
		return FUNC_SUCCESS;

	exec_prompt_cmds = 1;
	return run_kb_cmd("cd");
}

static int
rl_dir_previous(int count, int key)
{
	UNUSED(count); UNUSED(key);
	/* If already at the beginning of dir hist, do nothing */
	if (dirhist_cur_index == 0)
		return FUNC_SUCCESS;

	exec_prompt_cmds = 1;
	return run_kb_cmd("b");
}

static int
rl_dir_next(int count, int key)
{
	UNUSED(count); UNUSED(key);

	/* If already at the end of dir hist, do nothing */
	if (dirhist_cur_index + 1 == dirhist_total_index)
		return FUNC_SUCCESS;

	exec_prompt_cmds = 1;
	return run_kb_cmd("f");
}

int
rl_toggle_long_view(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1 || xargs.disk_usage_analyzer == 1)
		return FUNC_SUCCESS;

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
	return FUNC_SUCCESS;
}

int
rl_toggle_follow_link_long(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1 || conf.long_view == 0 || conf.light_mode == 1)
		return FUNC_SUCCESS;

#ifndef _NO_SUGGESTIONS
	if (suggestion.printed && suggestion_buf)
		free_suggestion();
#endif /* !_NO_SUGGESTIONS */

	conf.follow_symlinks_long = conf.follow_symlinks_long == 1 ? 0 : 1;

	if (conf.autols == 1) {
		if (conf.clear_screen == 0)
		/* Without this putchar(), the first entries of the directories
		 * list are printed in the prompt line. */
			putchar('\n');
		reload_dirlist();
	}

	print_reload_msg(_("Follow links %s\n"),
		conf.follow_symlinks_long == 1 ? _("enabled") : _("disabled"));
	xrl_reset_line_state();
	return FUNC_SUCCESS;
}

int
rl_toggle_dirs_first(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1)
		return FUNC_SUCCESS;

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
	return FUNC_SUCCESS;
}

int
rl_toggle_light_mode(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1)
		return FUNC_SUCCESS;

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

	return FUNC_SUCCESS;
}

int
rl_toggle_hidden_files(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1)
		return FUNC_SUCCESS;

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

	if (conf.show_hidden == 1)
		print_reload_msg(_("Showing dotfiles\n"));
	else
		print_reload_msg(_("Hiding dotfiles\n"));

	xrl_reset_line_state();
	return FUNC_SUCCESS;
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
		return FUNC_FAILURE;

	char *file = xnmalloc(config_dir_len + 15, sizeof(char));
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
	if (kbind_busy == 1 && alt_prompt == 0)
		return FUNC_SUCCESS;

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
		return FUNC_SUCCESS;
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
	return FUNC_SUCCESS;
}

static int
rl_sort_next(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1)
		return FUNC_SUCCESS;

#ifndef _NO_SUGGESTIONS
	if (suggestion.printed && suggestion_buf)
		free_suggestion();
#endif /* !_NO_SUGGESTIONS */

#ifndef ST_BTIME
	if (conf.sort + 1 == SBTIME)
		conf.sort++;
#endif /* !ST_BTIME */

	if (conf.light_mode == 1)
		while (!ST_IN_LIGHT_MODE(conf.sort + 1) && conf.sort + 1 <= SORT_TYPES)
			conf.sort++;

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
	return FUNC_SUCCESS;
}

static int
rl_sort_previous(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1)
		return FUNC_SUCCESS;

#ifndef _NO_SUGGESTIONS
	if (suggestion.printed && suggestion_buf)
		free_suggestion();
#endif /* !_NO_SUGGESTIONS */

#ifndef ST_BTIME
	if (conf.sort - 1 == SBTIME)
		conf.sort--;
#endif /* !ST_BTIME */

	if (conf.light_mode == 1)
		while (!ST_IN_LIGHT_MODE(conf.sort - 1) && conf.sort - 1 >= 0)
			conf.sort--;

	conf.sort--;
	if (conf.sort < 0)
		conf.sort = conf.light_mode == 1 ? SINO : SORT_TYPES;

	if (conf.autols == 1) {
		sort_switch = 1;
		if (conf.clear_screen == 0)
			putchar('\n');
		reload_dirlist();
		sort_switch = 0;
	}

	xrl_reset_line_state();
	return FUNC_SUCCESS;
}

static int
rl_lock(int count, int key)
{
	UNUSED(count); UNUSED(key);
	int ret = FUNC_SUCCESS;
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

	if (ret != FUNC_SUCCESS)
		return FUNC_FAILURE;
	return FUNC_SUCCESS;
}

static int
handle_no_sel(const char *func)
{
#ifndef _NO_SUGGESTIONS
	if (conf.colorize == 1 && wrong_cmd == 1)
		recover_from_wrong_cmd();

	if (suggestion.printed == 1 && suggestion_buf)
		clear_suggestion(CS_FREEBUF);
#endif /* !_NO_SUGGESTIONS */

	if (rl_end > 0) {
		rl_delete_text(0, rl_end);
		rl_point = rl_end = 0;
	}

	printf(_("\n%s: sel: %s\n"), func, strerror(ENOENT));
	rl_reset_line_state();

	return FUNC_SUCCESS;
}

static int
rl_archive_sel(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1)
		return FUNC_SUCCESS;

	if (sel_n == 0)
		return handle_no_sel("ac");

	return run_kb_cmd("ac sel");
}

static int
rl_remove_sel(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1)
		return FUNC_SUCCESS;

	if (sel_n == 0)
		return handle_no_sel("r");

	rl_deprep_terminal();
	char cmd[] = "r sel";
	keybind_exec_cmd(cmd);
	rl_prep_terminal(0);
	rl_reset_line_state();
	return FUNC_SUCCESS;
}

static int
rl_export_sel(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1)
		return FUNC_SUCCESS;

	if (sel_n == 0)
		return handle_no_sel("exp");

	return run_kb_cmd("exp sel");
}

static int
rl_move_sel(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1)
		return FUNC_SUCCESS;

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
		return FUNC_SUCCESS;

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
		return FUNC_SUCCESS;

	if (sel_n == 0)
		return handle_no_sel("c");

	exec_prompt_cmds = 1;
	rl_deprep_terminal();
	char cmd[] = "c sel";
	keybind_exec_cmd(cmd);
	rl_prep_terminal(0);
	rl_reset_line_state();
	return FUNC_SUCCESS;
}

int
rl_quit(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1)
		return FUNC_SUCCESS;

//	conf.cd_on_quit = 0;
	puts("\n");

	/* Reset terminal attributes before exiting. */
	rl_deprep_terminal();
	exit(FUNC_SUCCESS);
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
		return FUNC_SUCCESS;

#ifndef _NO_SUGGESTIONS
	if (suggestion.printed && suggestion_buf)
		free_suggestion();
#endif /* !_NO_SUGGESTIONS */

	int prev_prof, cur_prof = -1, total_profs = 0;
	get_cur_prof(&cur_prof, &total_profs);

	if (cur_prof == -1 || !profile_names[cur_prof]
	|| total_profs <= 1)
		return FUNC_FAILURE;

	prev_prof = cur_prof - 1;
	total_profs--;

	if (prev_prof < 0 || !profile_names[prev_prof])
		prev_prof = total_profs;

	if (conf.clear_screen) {
		CLEAR;
	} else {
		putchar('\n');
	}

	profile_set(profile_names[prev_prof]);
	rl_update_prompt_old();

	return FUNC_SUCCESS;
}

static int
rl_profile_next(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1)
		return FUNC_SUCCESS;

#ifndef _NO_SUGGESTIONS
	if (suggestion.printed && suggestion_buf)
		free_suggestion();
#endif /* !_NO_SUGGESTIONS */

	int next_prof, cur_prof = -1, total_profs = 0;
	get_cur_prof(&cur_prof, &total_profs);

	if (cur_prof == -1 || !profile_names[cur_prof])
		return FUNC_FAILURE;

	next_prof = cur_prof + 1;
	total_profs--;

	if (next_prof > total_profs || !profile_names[next_prof]
	|| total_profs <= 1)
		next_prof = 0;

	if (conf.clear_screen) {
		CLEAR;
	} else {
		putchar('\n');
	}

	profile_set(profile_names[next_prof]);
	rl_update_prompt_old();

	return FUNC_SUCCESS;
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
		return FUNC_SUCCESS;

	char cmd[PATH_MAX + 4];
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
		mp = xnmalloc(len + 1, sizeof(char *));
		xstrsncpy(mp, p, len + 1);
		unsetenv("MANPAGER");
	}

	int ret = launch_execl(str) != FUNC_SUCCESS;

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
		return FUNC_SUCCESS;

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
		return FUNC_FAILURE;
	return FUNC_SUCCESS;
}

static int
rl_cmds_help(int count, int key)
{
	if (kbind_busy == 1)
		return FUNC_SUCCESS;

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
		return FUNC_FAILURE;
	return FUNC_SUCCESS;
}

static int
rl_manpage(int count, int key)
{
	if (kbind_busy == 1)
		return FUNC_SUCCESS;

	UNUSED(count); UNUSED(key);
#ifndef _NO_SUGGESTIONS
	if (suggestion.printed && suggestion_buf)
		free_suggestion();
#endif /* !_NO_SUGGESTIONS */
	char *cmd[] = {"man", PROGRAM_NAME, NULL};
	if (launch_execv(cmd, FOREGROUND, E_NOFLAG) != FUNC_SUCCESS)
		return FUNC_FAILURE;
	return FUNC_SUCCESS;
}

static int
rl_dir_pinned(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (!pinned_dir) {
		printf(_("\n%s: No pinned file\n"), PROGRAM_NAME);
		rl_reset_line_state();
		return FUNC_SUCCESS;
	}

	return run_kb_cmd(",");
}

/* Switch to workspace N. */
static int
rl_switch_workspace(const int n)
{
	if (rl_line_buffer && *rl_line_buffer)
		rl_delete_text(0, rl_end);

	char t[16];
	exec_prompt_cmds = 1;

	if (cur_ws == (n - 1)) {
		/* If the user attempts to switch to the same workspace they're
		 * currently in, switch rather to the previous workspace. */
		if (prev_ws != cur_ws) {
			snprintf(t, sizeof(t), "ws %d", prev_ws + 1);
			return run_kb_cmd(t);
		}

		return FUNC_SUCCESS;
	}

	snprintf(t, sizeof(t), "ws %d", n);
	return run_kb_cmd(t);
}

static int
rl_ws1(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return rl_switch_workspace(1);
}

static int
rl_ws2(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return rl_switch_workspace(2);
}

static int
rl_ws3(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return rl_switch_workspace(3);
}

static int
rl_ws4(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return rl_switch_workspace(4);
}

static int
run_plugin(const int num)
{
	if (rl_line_buffer && *rl_line_buffer)
		setenv("CLIFM_LINE", rl_line_buffer, 1);

	char cmd[MAX_INT_STR + 7];
	snprintf(cmd, sizeof(cmd), "plugin%d", num);
	const int ret = run_kb_cmd(cmd);

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
rl_plugin5(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_plugin(5);
}

static int
rl_plugin6(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_plugin(6);
}

static int
rl_plugin7(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_plugin(7);
}

static int
rl_plugin8(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_plugin(8);
}

static int
rl_plugin9(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_plugin(9);
}

static int
rl_plugin10(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_plugin(10);
}

static int
rl_plugin11(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_plugin(11);
}

static int
rl_plugin12(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_plugin(12);
}

static int
rl_plugin13(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_plugin(13);
}

static int
rl_plugin14(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_plugin(14);
}

static int
rl_plugin15(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_plugin(15);
}

static int
rl_plugin16(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_plugin(16);
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
		return FUNC_SUCCESS;

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

	char q[PATH_MAX + 1];
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

	return FUNC_SUCCESS;
}

static inline int
handle_cmdhist_beginning(int key)
{
	int p = (int)curhistindex;
	cmdhist_flag = 1;

	if (key == 65) { /* Up arrow key */
		if (--p < 0)
			return FUNC_FAILURE;
	} else { /* Down arrow key */
		if (rl_end == 0)
			return FUNC_SUCCESS;
		if (++p >= (int)current_hist_n) {
			rl_replace_line("", 1);
			curhistindex++;
			return FUNC_SUCCESS;
		}
	}

	if (!history[p].cmd)
		return FUNC_FAILURE;

	curhistindex = (size_t)p;

	return print_cmdhist_line(p, 1);
}

static inline int
handle_cmdhist_middle(int key)
{
	int found = 0, p = (int)curhistindex;

	if (key == 65) { /* Up arrow key */
		if (--p < 0) return FUNC_FAILURE;

		while (p >= 0 && history[p].cmd) {
			if (strncmp(rl_line_buffer, history[p].cmd, (size_t)rl_point) == 0
			&& strcmp(rl_line_buffer, history[p].cmd) != 0) {
				found = 1; break;
			}
			p--;
		}
	} else { /* Down arrow key */
		if (++p >= (int)current_hist_n)	return FUNC_FAILURE;

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
		return FUNC_FAILURE;
	}

	return print_cmdhist_line(p, 0);
}

/* Handle keybinds for the cmds history: UP/C-p and DOWN/C-n */
static int
rl_cmdhist(int count, int key)
{
	UNUSED(count);
	if (rl_nohist == 1)
		return FUNC_SUCCESS;

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
		return FUNC_FAILURE;

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
	return FUNC_SUCCESS;
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
		return FUNC_SUCCESS;

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

	return FUNC_SUCCESS;
}

static int
rl_toggle_virtualdir_full_paths(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (!stdin_tmp_dir || strcmp(stdin_tmp_dir, workspaces[cur_ws].path) != 0)
		return FUNC_SUCCESS;

	xchmod(stdin_tmp_dir, "0700", 1);
	xargs.virtual_dir_full_paths = xargs.virtual_dir_full_paths == 1 ? 0 : 1;

	filesn_t i = files;
	while (--i >= 0) {
		char *rp = xrealpath(file_info[i].name, NULL);
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
	return FUNC_SUCCESS;
}

static int
rl_run_pager(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1 || conf.pager == 1)
		return FUNC_SUCCESS;

	return run_kb_cmd("pg");
}

/* Run "!<TAB>" to display the command history via finder. */
static int
rl_cmdhist_tab(int count, int key)
{
	UNUSED(count); UNUSED(key);
	rl_insert_text("!");
	rl_point = rl_end;

#ifndef _NO_SUGGESTIONS
	if (suggestion_buf)
		clear_suggestion(CS_FREEBUF);
#endif /* _NO_SUGGESTIONS */

	tab_complete('!');

	if (rl_end > 0 && rl_line_buffer[rl_end - 1] == '!') {
		rl_end--;
		rl_point--;
	}

	return FUNC_SUCCESS;
}

/* Used to disable keybindings. */
static int
do_nothing(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return FUNC_SUCCESS;
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
	rl_bind_keyseq(find_key("toggle-follow-links-long"), rl_toggle_follow_link_long);
	rl_bind_keyseq(find_key("toggle-light"), rl_toggle_light_mode);
	rl_bind_keyseq(find_key("dirs-first"), rl_toggle_dirs_first);
	rl_bind_keyseq(find_key("sort-previous"), rl_sort_previous);
	rl_bind_keyseq(find_key("sort-next"), rl_sort_next);
	rl_bind_keyseq(find_key("only-dirs"), rl_toggle_only_dirs);
	rl_bind_keyseq(find_key("run-pager"), rl_run_pager);

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
	rl_bind_keyseq(find_key("cmd-hist"), rl_cmdhist_tab);
	rl_bind_keyseq(find_key("quit"), rl_quit);

	/* Plugins */
	rl_bind_keyseq(find_key("plugin1"), rl_plugin1);
	rl_bind_keyseq(find_key("plugin2"), rl_plugin2);
	rl_bind_keyseq(find_key("plugin3"), rl_plugin3);
	rl_bind_keyseq(find_key("plugin4"), rl_plugin4);
	rl_bind_keyseq(find_key("plugin5"), rl_plugin5);
	rl_bind_keyseq(find_key("plugin6"), rl_plugin6);
	rl_bind_keyseq(find_key("plugin7"), rl_plugin7);
	rl_bind_keyseq(find_key("plugin8"), rl_plugin8);
	rl_bind_keyseq(find_key("plugin9"), rl_plugin9);
	rl_bind_keyseq(find_key("plugin10"), rl_plugin10);
	rl_bind_keyseq(find_key("plugin11"), rl_plugin11);
	rl_bind_keyseq(find_key("plugin12"), rl_plugin12);
	rl_bind_keyseq(find_key("plugin13"), rl_plugin13);
	rl_bind_keyseq(find_key("plugin14"), rl_plugin14);
	rl_bind_keyseq(find_key("plugin15"), rl_plugin15);
	rl_bind_keyseq(find_key("plugin16"), rl_plugin16);
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
	rl_bind_keyseq("\\M-+", rl_toggle_follow_link_long);
	rl_bind_keyseq("\\M-y", rl_toggle_light_mode);
	rl_bind_keyseq("\\M-g", rl_toggle_dirs_first);
	rl_bind_keyseq("\\M-z", rl_sort_previous);
	rl_bind_keyseq("\\M-x", rl_sort_next);
	rl_bind_keyseq("\\M-,", rl_toggle_only_dirs);
	rl_bind_keyseq("\\M-0", rl_run_pager);

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
	rl_bind_keyseq("\\M-*", do_nothing);

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
