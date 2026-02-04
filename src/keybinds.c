/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* keybinds.c -- handle keybindings */

#include "helpers.h"

#ifdef __OpenBSD__
typedef char *rl_cpvfunc_t;
typedef void rl_macro_print_func_t (const char *, const char *, int, const char *);
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
#include "autocmds.h" /* update_autocmd_opts() */
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
#include "sort.h" /* QSFUNC macro for qsort(3) */
#include "spawn.h"
#include "tabcomp.h" /* tab_complete() */
#include "translate_key.h" /* translate_key() */

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

static int
append_str(char *buf, const int buf_size, size_t *len, const char *str)
{
	const size_t length = strlen(str);
	if (length >= (size_t)buf_size - *len)
		return (-1); /* Buffer overflow */

	xstrsncpy(buf + *len, str, (size_t)buf_size - *len);
	*len += length;

	return 0;
}

#define KBUF_SIZE 256 /* This should be enough to handle most keybindings. */
#define END_KEYSEQ_CHAR ','

static const char *
translate_emacs_style_keyseq(const char *key)
{
	if (!key || !*key || *key != '\\')
		return NULL;

	if (key[1] == 'e' && (key[2] == CSI_INTRODUCER || key[2] == SS3_INTRODUCER))
		return NULL;

	static char buf[KBUF_SIZE] = {0};
	size_t buf_len = 0;

	while (*key) {
		if (*key == '\\') {
			if (key[1] == 'e' || (key[1] == 'M' && key[2] ==  '-')) {
				if (append_str(buf, KBUF_SIZE, &buf_len, "Alt+") == -1)
					return NULL;
				/* If "\M-" we want to advance 3 bytes, not 2. */
				key += 2 + (key[1] ==  'M');
				if (!*key) /* Incomplete sequence: Alt without modified key. */
					return NULL;
				continue;
			}

			if (key[1] == 'C' && key[2] == '-') {
				if (append_str(buf, KBUF_SIZE, &buf_len, "Ctrl+") == -1)
					return NULL;
				key += 3;
				if (!*key) /* Incomplete sequence: Ctrl without modified key. */
					return NULL;
				continue;
			}

			// Additional escape sequences:
			// https://www.gnu.org/software/bash/manual/html_node/Readline-Init-File-Syntax.html

			/* Unrecognized escape sequence */
			return NULL;
		}

		if (buf_len >= sizeof(buf) - 2
		/* No keybinding starts with a non-modifier key. Skip it. */
		|| buf_len == 0)
			return NULL;

		/* Let's try to skip non-keyboard related escape sequences:
		 * CSI, OSC, DCS, APC, and PM escape sequences, plus
		 * character set switching sequences (e.g. "\e(A". */
		if ((*key == '[' || *key == ']' || *key == 'P' || *key == '_'
		|| *key == '^' || *key == '(' || *key == ')')
		&& key[1] && key[1] != '\\')
			return NULL;

		/* Append single character to the buffer. */
		buf[buf_len++] = *key++;

		/* A character that is not a modifier key marks the end of the
		 * key sequence. Append an ',' in this case, provided it is
		 * not the end of the string. */
		if (*key)
			buf[buf_len++] = END_KEYSEQ_CHAR;
	}

	buf[buf_len] = '\0';

	if (*buf == 'A' && strncmp(buf, "Alt+Ctrl+", 9) == 0)
		memcpy(buf, "Ctrl+Alt+", 9);

	return *buf ? buf : NULL;
}
#undef END_KEYSEQ_CHAR

/* Translate the raw escape code KEY (sent by the terminal upon a key press)
 * into human-readable format.
 * Return the translation, if found, or the original sequence (KEY) otherwise. */
static const char *
xtranslate_key(const char *key)
{
	if (!key || !*key)
		return NULL;

	if (*key == '-' && !key[1])
		return _("not bound");

	const char *t = (strstr(key, "C-") || strstr(key, "M-"))
		? translate_emacs_style_keyseq(key) : NULL;
	if (t)
		return t;

	const char *key_ptr = key;

	static char buf[KBUF_SIZE] = "";
	int c = 0;
	while (*key_ptr) {
		if (*key_ptr == '\\' && key_ptr[1] == 'e') {
			buf[c++] = KEY_ESC;
			key_ptr += 2;
		} else {
			buf[c++] = *key_ptr++;
		}
	}
	buf[c] = '\0';

	char *translation = translate_key(buf, TK_TERM_GENERIC);
	if (translation) {
		xstrsncpy(buf, translation, sizeof(buf));
		free(translation);
		return buf;
	}

	return key;
}

static int
backup_and_create_kbinds_file(void)
{
	char *backup = gen_backup_file(kbinds_file, 1);
	if (!backup)
		return FUNC_FAILURE;

	if (renameat(XAT_FDCWD, kbinds_file, XAT_FDCWD, backup) != 0) {
		xerror(_("kb: Cannot rename '%s' to '%s': %s\n"),
			kbinds_file, backup, strerror(errno));
		free(backup);
		return FUNC_FAILURE;
	}

	char *abbrev = abbreviate_file_name(backup);
	printf(_("Old keybindings file saved as '%s'\n"),
		abbrev ? abbrev : backup);
	free(backup);
	free(abbrev);

	return create_kbinds_file();
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

	if (stat(kbinds_file, &attr) == -1)
		exit_status = create_kbinds_file();
	else
		exit_status = backup_and_create_kbinds_file();

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

/* FUNC_NAME is not NULL when invokd by 'kb conflict' (in which case we're
 * check a readline key sequence). Otherwise, if invoked by 'kb bind',
 * FUNC_NAME isn't set.
 * We display different messages depending on the invoking command.
 * Returns the number of conflicts found. */
static int
check_clifm_kb(const char *kb, const char *func_name)
{
	int conflicts = 0;

	for (size_t i = 0; i < kbinds_n; i++) {
		if (!kbinds[i].key || strcmp(kb, kbinds[i].key) != 0)
			continue;

		if (func_name != NULL) {
			fprintf(stderr, _("kb: '%s' conflicts with '%s' (readline)\n"),
				kbinds[i].function ? kbinds[i].function : "unnamed", func_name);
		} else {
			const char *func = kbinds[i].function
				? kbinds[i].function : "unnamed";
			const char *t = xtranslate_key(kbinds[i].key);

			fprintf(stderr, _("kb: %s: Key already in use by '%s'.\n"),
				t ? t : kbinds[i].key, func);
		}

		conflicts++;
	}

	return conflicts;
}

/* Check all readline key sequences against the key sequence KB, if not NULL
 * (this is the case when validating a key entered via 'kb bind').
 * Otherwise the check is made against all clifm key sequences (i.e., when
 * the invoking command is 'kb conflict').
 * Returns the number of conflicts found. */
static int
check_rl_kbinds(const char *kb)
{
	char *name = NULL;
	char **names = (char **)rl_funmap_names();
	int conflicts = 0;

	if (!names)
		return FUNC_SUCCESS;

	for (size_t i = 0; (name = names[i]); i++) {
		rl_command_func_t *function = rl_named_function(name);
		char **keys = rl_invoking_keyseqs(function);
		if (!keys)
			continue;

		for (size_t j = 0; keys[j]; j++) {
			if (kb == NULL) {
				conflicts += check_clifm_kb(keys[j], name);
			} else {
				if (strcmp(kb, keys[j]) == 0) {
					const char *t = xtranslate_key(kb);
					fprintf(stderr, _("kb: %s: Key already in use by '%s' "
						"(readline)\n"), t ? t : kb, name);
					conflicts++;
				}
			}
			free(keys[j]);
		}
		free(keys);
	}

	free(names);

	return conflicts;
}

static int
check_kbinds_conflict(void)
{
	if (kbinds_n == 0) {
		puts(_("kb: No keybindings defined"));
		return FUNC_SUCCESS;
	}

	int ret = FUNC_SUCCESS;
	for (size_t i = 0; i < kbinds_n; i++) {
		for (size_t j = i + 1; j < kbinds_n; j++) {
			if (strcmp(kbinds[i].key, kbinds[j].key) == 0) {
				fprintf(stderr, _("kb: '%s' conflicts with '%s'\n"),
					kbinds[i].function, kbinds[j].function);
				ret = FUNC_FAILURE;
			}
		}
	}

	if (check_rl_kbinds(NULL) > 0)
		ret = FUNC_FAILURE;

	return ret;
}

/* Retrieve the key sequence associated to FUNCTION */
static char *
find_key(const char *function)
{
	if (kbinds_n == 0)
		return NULL;

	for (size_t i = kbinds_n; i-- > 0;) {
		if (*function != *kbinds[i].function)
			continue;
		if (strcmp(function, kbinds[i].function) == 0)
			return kbinds[i].key;
	}

	return NULL;
}

/* Read a key sequence from STDIN and return its value (memory to hold this
 * value is allocated via malloc(2)). */
static char *
get_new_keybind(void)
{
	char buf[64];
	size_t len = 0;
	int ret = 0;
	int ch = 0;
	int prev = 0;

	buf[0] = '\0';

	putchar(':');
	fflush(stdout);

	if (enable_raw_mode(STDIN_FILENO) == -1) {
		UNHIDE_CURSOR;
		return NULL;
	}

	while (1) {
		int result = (int)read(STDIN_FILENO, &ch, sizeof(unsigned char)); /* flawfinder: ignore */
		if (result <= 0 || len >= sizeof(buf) - 1) {
			buf[len] = '\0';
			break;
		}

		if (result != sizeof(unsigned char))
			continue;

		const unsigned char c = (unsigned char)ch;

		if (prev != KEY_ESC) {
			if (c == KEY_ENTER)
				break;

			if (c == CTRL('D')) {
				buf[0] = '\0';
				break;
			}

			if (c == CTRL('C')) {
				putchar('\r');
				MOVE_CURSOR_RIGHT(1);
				ERASE_TO_RIGHT;
				fflush(stdout);
				buf[0] = '\0';
				len = 0;
				continue;
			}
		}

		const size_t remaining = sizeof(buf) - len - 1;

		if (c == KEY_ESC) {
			ret = snprintf(buf + len, remaining, "\\e");
		} else if (isprint(c) && c != ' ') {
			ret = snprintf(buf + len, remaining, "%c", c);
		} else if (c <= 31) {
			ret = snprintf(buf + len, remaining, "\\C-%c", c + '@' - 'A' + 'a');
		} else {
			ret = snprintf(buf + len, remaining, "\\x%x", c);
		}

		prev = ch;
		if (ret < 0 || (size_t)ret >= remaining)
			continue;

		/* Kitty keyboard protocol */
		if (*buf && strstr(buf, "\\e[100;5u")) { /* Ctrl+d */
			MOVE_CURSOR_LEFT(8);
			ERASE_TO_RIGHT;
			fflush(stdout);
			buf[0] = '\0';
			break;
		}

		if (*buf && strstr(buf, "\\e[99;5u")) { /* Ctrl+c */
			putchar('\r');
			MOVE_CURSOR_RIGHT(1);
			ERASE_TO_RIGHT;
			fflush(stdout);
			buf[0] = '\0';
			len = 0;
			continue;
		}

		if (*buf) {
			fputs(buf + len, stdout);
			fflush(stdout);
		}

		len += (size_t)ret;
	}

	disable_raw_mode(STDIN_FILENO);
	putchar('\n');

	if (!*buf)
		return NULL;

	return savestring(buf, strlen(buf));
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

	const int tmp_fd = mkstemp(tmp_name);
	if (tmp_fd == -1) {
		xerror(_("kb: Error creating temporary file: %s\n"), strerror(errno));
		goto ERROR;
	}

	FILE *tmp_fp = fdopen(tmp_fd, "w");
	if (!tmp_fp) {
		xerror(_("kb: Cannot open temporary file: %s\n"), strerror(errno));
		unlinkat(XAT_FDCWD, tmp_name, 0);
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
		if (renameat(XAT_FDCWD, tmp_name, XAT_FDCWD, kbinds_file) == -1)
			xerror(_("kb: Cannot rename '%s' to '%s': %s\n"),
				tmp_name, kbinds_file, strerror(errno));
	} else {
		unlinkat(XAT_FDCWD, tmp_name, 0);
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
	const size_t len = strlen(func_name);

	for (size_t i = 0; kb_cmds[i].name; i++) {
		if (len == kb_cmds[i].len && *kb_cmds[i].name == *func_name
		&& strcmp(kb_cmds[i].name, func_name) == 0)
			return FUNC_SUCCESS;
	}

	return FUNC_FAILURE;
}

/* Check the key sequence KB against both clifm and readline key sequences.
 * Return the number of conflicts found. */
static int
check_kb_conflicts(const char *kb)
{
	if (!strchr(kb, '\\') && strcmp(kb, "-") != 0) {
		fprintf(stderr, _("kb: Invalid keybinding\n"));
		return FUNC_FAILURE;
	}

	if (*kb == '-' && !kb[1])
		return FUNC_SUCCESS;

	int conflicts = 0;
	conflicts += check_clifm_kb(kb, NULL);
	conflicts += check_rl_kbinds(kb);

	return conflicts;
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

	const char *func_key = find_key(func_name);
	const char *translated_key = func_key ? xtranslate_key(func_key) : "unset";
	/* translated_key can only be NULL if func_key is not NULL. */
	const char *cur_key = translated_key ? translated_key : func_key;

	printf(_("Enter a keybinding for %s%s%s (current: %s%s%s)\n"),
		BOLD, func_name, df_c, BOLD, cur_key, df_c);
	puts(_("(Enter:accept, Ctrl+d:abort, Ctrl+c:clear-line)"));
	puts(_("To unset the function enter '-'"));

	char *kb = get_new_keybind();
	if (kb == NULL)
		return FUNC_SUCCESS;

	const int unset_key = (*kb == '-' && !kb[1]);
	if (unset_key == 0 && check_kb_conflicts(kb) == 0) {
		/* If any conflict was found (check_kb_conflicts() is greater than
		 * zero), the function already displayed the keybinding translation. */
		const char *translation = xtranslate_key(kb);
		printf(_("New key: %s\n"), translation ? translation : kb);
	}

	const char *msg = unset_key == 1 ? _("Unset this function?")
		: _("Bind function to this new key?");

	if (rl_get_y_or_n(msg, 0) == 0) {
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

static int
list_kbinds(void)
{
	if (kbinds_n == 0) {
		puts(_("kb: No keybindings defined\n"));
		return FUNC_SUCCESS;
	}

	size_t flen = 0;
	for (size_t i = 0; i < kbinds_n; i++) {
		const size_t l = kbinds[i].function ? strlen(kbinds[i].function) : 0;
		if (l > flen)
			flen = l;
	}

	for (size_t i = 0; i < kbinds_n; i++) {
		if (!kbinds[i].key || !kbinds[i].function)
			continue;

		const char *translation = xtranslate_key(kbinds[i].key);
		printf("%-*s (%s)\n", (int)flen, kbinds[i].function,
			translation ? translation : kbinds[i].key);
	}

	return FUNC_SUCCESS;
}

/* Print the list of readline functions and associated keys. */
static int
list_rl_kbinds(void)
{
	char *name = NULL;
	char **names = (char **)rl_funmap_names();

	if (!names)
		return FUNC_SUCCESS;

	size_t flen = 0;
	for (size_t i = 0; (name = names[i]); i++) {
		rl_command_func_t *function = rl_named_function(name);
		char **keys = rl_invoking_keyseqs(function);
		if (!keys)
			continue;

		for (size_t j = 0; keys[j]; j++)
			free(keys[j]);
		free(keys);

		const size_t l = strlen(name);
		if (l > flen)
			flen = l;
	}

	char prev[KBUF_SIZE] = "";

	for (size_t i = 0; (name = names[i]); i++) {
		if ((*name == 's' && strcmp(name, "self-insert") == 0)
		|| (*name == 'd' && strcmp(name, "do-lowercase-version") == 0))
			continue;

		rl_command_func_t *function = rl_named_function(name);
		char **keys = rl_invoking_keyseqs(function);
		if (!keys)
			continue;

		printf("%-*s ", (int)flen, name);

		for (size_t j = 0; keys[j]; j++) {
			const char *t = xtranslate_key(keys[j]);

			/* Skip consecutive duplicates. */
			if (t && *prev == *t && strcmp(prev, t) == 0) {
				free(keys[j]);
				continue;
			}

			printf("(%s) ", t ? t : keys[j]);

			if (t)
				xstrsncpy(prev, t, sizeof(prev));

			free(keys[j]);
		}

		putchar('\n');
		free(keys);
	}

	free(names);

	puts(_("\nNote: Bear in mind that clifm's keybindings take precedence "
		"over readline's.\nTo modify readline's keybindings edit "
		"~/.config/clifm/readline.clifm"));

	return FUNC_SUCCESS;
}
#undef KBUF_SIZE

int
kbinds_function(char **args)
{
	if (!args)
		return FUNC_FAILURE;

	if (!args[1] || strcmp(args[1], "list") == 0)
		return list_kbinds();

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

	if (*args[1] == 'r' && strcmp(args[1], "readline") == 0)
		return list_rl_kbinds();

	fprintf(stderr, "%s\n", _(KB_USAGE));
	return FUNC_FAILURE;
}

/* Store keybinds from the keybinds file in a struct. */
int
load_keybinds(void)
{
	if (config_ok == 0 || !kbinds_file)
		return FUNC_FAILURE;

	/* Free the keybinds struct array */
	if (kbinds_n > 0) {
		for (size_t i = kbinds_n; i-- > 0;) {
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
	char *line = NULL;
	ssize_t line_len = 0;

	while ((line_len = getline(&line, &line_size, fp)) > 0) {
		if (!line || !*line || *line == '#' || *line == '\n')
			continue;

		if (line[line_len - 1] == '\n')
			line[line_len - 1] = '\0';

		char *tmp = NULL;
		tmp = strchr(line, ':');
		if (!tmp || !*(tmp + 1))
			continue;

		/* Now copy left and right values of each keybinding to the
		 * kbinds struct. */
		kbinds = xnrealloc(kbinds, kbinds_n + 1, sizeof(struct kbinds_t));
		kbinds[kbinds_n].key = savestring(tmp + 1, strlen(tmp + 1));

		*tmp = '\0';

		kbinds[kbinds_n++].function = savestring(line, strlen(line));
	}

	fclose(fp);
	free(line);

	if (kbinds_n > 1)
		qsort(kbinds, kbinds_n, sizeof(*kbinds), (QSFUNC *)compare_strings);

	return FUNC_SUCCESS;
}

/* This call to prompt() just updates the prompt in case it was modified by
 * a keybinding, for example, chdir, files selection, trash, and so on.
 * The screen is not refreshed in any way. */
static void
rl_update_prompt(void)
{
	if (rl_line_buffer) {
		memset(rl_line_buffer, '\0', (size_t)rl_end);
		rl_point = rl_end = 0;
	}

	prompt(exec_prompt_cmds ? PROMPT_UPDATE_RUN_CMDS : PROMPT_UPDATE,
		PROMPT_NO_SCREEN_REFRESH);
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
	char *input = prompt(PROMPT_SHOW, PROMPT_NO_SCREEN_REFRESH);
	free(input);
}
#endif /* __HAIKU__ || !_NO_PROFILES */

static void
xrl_update_prompt(void)
{
#ifdef __HAIKU__
	rl_update_prompt_old();
#else
	rl_update_prompt();
#endif /* __HAIKU__ */
}

/* Run any command recognized by Clifm via a keybind. Example:
 * keybind_exec_cmd("sel *") */
int
keybind_exec_cmd(char *str)
{
	const size_t old_args = args_n;
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

		for (size_t i = args_n + 1; i-- > 0;)
			free(cmd[i]);
		free(cmd);

		xrl_update_prompt();
	}

	args_n = old_args;
	return exit_status;
}

static int
run_kb_cmd(char *cmd, const int ignore_empty_line)
{
	if (!cmd || !*cmd)
		return FUNC_FAILURE;

	if (kbind_busy == 1)
		return FUNC_SUCCESS;

#ifndef _NO_SUGGESTIONS
	if (conf.colorize == 1 && wrong_cmd == 1)
		recover_from_wrong_cmd();
#endif /* _NO_SUGGESTIONS */

	const int exit_code_bk = exit_code;

	keybind_exec_cmd(cmd);
	rl_reset_line_state();

	if (exit_code != exit_code_bk)
		/* The exit code was changed by the executed command. Force the
		 * input taking function (my_rl_getc) to update the value of
		 * prompt_offset to correctly calculate the cursor position. */
		prompt_offset = UNSET;

	g_prompt_ignore_empty_line = ignore_empty_line;
	return FUNC_SUCCESS;
}

int
rl_toggle_max_filename_len(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1)
		return FUNC_SUCCESS;

	static int mnl_bk = 0, flag = 0;

	if (flag == 0 || conf.trunc_names == 0) {
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

	update_autocmd_opts(AC_MAX_NAME_LEN);

	if (conf.autols == 1) {
		if (conf.clear_screen == 0)
			putchar('\n');
		reload_dirlist();
	}

	if (conf.max_name_len == UNSET) {
		print_reload_msg(NULL, NULL, _("Max name length unset\n"));
	} else {
		char auto_perc[MAX_INT_STR + 5] = "";
		if (conf.max_name_len_auto != UNSET)
			snprintf(auto_perc, sizeof(auto_perc), " (%d%%)", conf.max_name_len_auto);
		print_reload_msg(NULL, NULL, _("Max name length set to %d%s\n"),
			conf.max_name_len, auto_perc);
	}

	xrl_reset_line_state();
	return FUNC_SUCCESS;
}

/* Prepend authentication program name (typically sudo or doas) to the current
 * input string. */
static int
rl_prepend_sudo(int count, int key)
{
	UNUSED(count); UNUSED(key);

	if (kbind_busy == 1)
		return FUNC_SUCCESS;

#ifndef _NO_SUGGESTIONS
	if (suggestion.printed && suggestion_buf) {
		clear_suggestion(CS_FREEBUF);
		fputs(df_c, stdout);
	}
#endif /* !_NO_SUGGESTIONS */

	int free_s = 1;
	size_t len = 0;
	char *t = sudo_cmd;
	char *s = NULL;

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

	char *c = NULL;
	if (conf.highlight == 1 && conf.colorize == 1
	&& cur_color && cur_color != tx_c) {
		c = cur_color;
		fputs(tx_c, stdout);
	}

	const int p = rl_point;
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
		const int r = rl_point;
		rl_point = rl_end;
		clear_suggestion(CS_FREEBUF);
		rl_point = r;
	}
#endif /* !_NO_SUGGESTIONS */

#ifndef _NO_HIGHLIGHT
	if (conf.highlight == 1) {
		const int r = rl_point;
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
	return run_kb_cmd("n", 1);
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

		/* We only need to redisplay first suggested word if it contains
		 * a highlighting char and it is not preceded by a space */
		int redisplay = 0;
		if (accept_first_word == 1) {
			for (size_t i = 0; t[i]; i++) {
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
		for (size_t i = 0; t[i]; i++) {
			rl_highlight(t, i, SET_COLOR);
			if ((signed char)t[i] < 0) {
				q[l++] = t[i];
				if ((signed char)t[i + 1] >= 0) {
					q[l] = '\0';
					l = 0;
					rl_insert_text(q);
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
			const size_t slen = strlen(suggestion_buf);
			*s = _s ? _s : ' ';
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

	if (wrong_cmd == 0 && cur_color != hq_c) {
		cur_color = tx_c;
		fputs(tx_c, stdout);
	}

	/* Only accept the current suggestion if the cursor is at the end
	 * of the line typed so far */
	if (!conf.suggestions || rl_point != rl_end || !suggestion_buf
	|| suggestion.type == CMD_DESC_SUG) {
		if (rl_point < rl_end) {
			/* Just move the cursor forward one character */
			const int mlen = mblen(rl_line_buffer + rl_point, MB_CUR_MAX);
			rl_point += mlen;
		}
		return FUNC_SUCCESS;
	}

	/* If accepting the first suggested word, accept only up to next
	 * word delimiter. */
	char *s = NULL, truncated_char = 0;
	int truncated = 0, accept_first_word_last = 0;
	if (accept_first_word == 1) {
		char *p = suggestion_buf + (rl_point - suggestion.offset);
		/* Skip leading spaces */
		while (*p == ' ')
			p++;

		/* Skip all consecutive word delimiters from the beginning of the
		 * suggestion (P), except for slash and space. */
		while ((s = strpbrk(p, WORD_DELIMITERS)) == p && *s != '/' && *s != ' ')
			p++;
		if (s && s != p && *(s - 1) == ' ')
			s = strpbrk(p, WORD_DELIMITERS);

		while (s && IS_UTF8_CHAR(*s))
			s = strpbrk(s + 1, WORD_DELIMITERS);

		if (s && s[1]) { /* Truncate suggestion after word delimiter */
			if (*s == '/')
				++s;
			truncated_char = *s;
			*s = '\0';
			truncated = 1;
		} else { /* Last word: No word delimiter */
			const size_t len = strlen(suggestion_buf);
			if (len > 0 && suggestion_buf[len - 1] != '/'
			&& suggestion_buf[len - 1] != ' ')
				suggestion.type = NO_SUG;
			accept_first_word_last = 1;
			s = NULL;
		}
	}

	const int bypass_alias =
		(rl_line_buffer && *rl_line_buffer == '\\' && rl_line_buffer[1]);

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
		char *tmp = NULL;
		size_t isquote = 0, backslash = 0;
		for (size_t i = 0; suggestion_buf[i]; i++) {
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
			} else {
				if (*suggestion_buf != '/' || suggestion_buf[1]) {
					fputs(hd_c, stdout);
					rl_insert_text("/");
					rl_redisplay();
					fputs(df_c, stdout);
				}
			}
		} else {
			if (suggestion.filetype != DT_DIR
			&& suggestion.type != BOOKMARK_SUG && suggestion.type != BACKDIR_SUG)
				rl_stuff_char(' ');
		}
		suggestion.type = NO_SUG;
		}
		break;

	case FIRST_WORD:
		my_insert_text(suggestion_buf, s, truncated_char); break;

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
		} else {
			if (suggestion.type == WS_PREFIX_SUG
			|| suggestion.type == WS_NUM_PREFIX_SUG)
				{ prefix[0] = 'w'; prefix[1] = ':'; prefix[2] = '\0'; }
		}

		rl_insert_text(prefix);
		char *p = (suggestion.type != BM_PREFIX_SUG
			&& suggestion.type != WS_PREFIX_SUG)
			? escape_str(suggestion_buf) : NULL;

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

	/* Move the cursor to the end of the line. */
	rl_point = rl_end;
	if (accept_first_word == 0) {
		suggestion.printed = 0;
		free(suggestion_buf);
		suggestion_buf = NULL;
	} else {
		if (s && truncated == 1)
			/* Reinsert the char we removed to print only the first word. */
			*s = truncated_char;
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
	 * bookmark and alias names. */
	const int t = suggestion.type;
	if (t != ELN_SUG && t != BOOKMARK_SUG && t != ALIAS_SUG && t != JCMD_SUG
	&& t != FUZZY_FILENAME && t != CMD_DESC_SUG
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
	return run_kb_cmd("rf", 0);
}

static int
rl_dir_parent(int count, int key)
{
	UNUSED(count); UNUSED(key);
	/* If already root dir, do nothing */
	if (*workspaces[cur_ws].path == '/' && !workspaces[cur_ws].path[1])
		return FUNC_SUCCESS;

	exec_prompt_cmds = 1;
	return run_kb_cmd("cd ..", 0);
}

static int
rl_dir_root(int count, int key)
{
	UNUSED(count); UNUSED(key);
	/* If already root dir, do nothing */
	if (*workspaces[cur_ws].path == '/' && !workspaces[cur_ws].path[1])
		return FUNC_SUCCESS;

	return run_kb_cmd("cd /", 0);
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
	return run_kb_cmd("cd", 0);
}

static int
rl_dir_previous(int count, int key)
{
	UNUSED(count); UNUSED(key);
	/* If already at the beginning of dir hist, do nothing */
	if (dirhist_cur_index == 0)
		return FUNC_SUCCESS;

	exec_prompt_cmds = 1;
	return run_kb_cmd("b", 0);
}

static int
rl_dir_next(int count, int key)
{
	UNUSED(count); UNUSED(key);

	/* If already at the end of dir hist, do nothing */
	if (dirhist_cur_index + 1 == dirhist_total_index)
		return FUNC_SUCCESS;

	exec_prompt_cmds = 1;
	return run_kb_cmd("f", 0);
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

	conf.long_view = !conf.long_view;

	update_autocmd_opts(AC_LONG_VIEW);

	if (conf.autols == 1) {
		if (conf.clear_screen == 0)
		/* Without this putchar(), the first entries of the directories
		 * list are printed in the prompt line. */
			putchar('\n');
		reload_dirlist();
	}

	print_reload_msg(NULL, NULL, _("Long view: %s\n"),
		conf.long_view == 1 ? _("on") : _("off"));
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

	conf.follow_symlinks_long = !conf.follow_symlinks_long;

	if (conf.autols == 1) {
		if (conf.clear_screen == 0)
		/* Without this putchar(), the first entries of the directories
		 * list are printed in the prompt line. */
			putchar('\n');
		reload_dirlist();
	}

	print_reload_msg(NULL, NULL, _("Follow links: %s\n"),
		conf.follow_symlinks_long == 1 ? _("on") : _("off"));
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

	conf.list_dirs_first = !conf.list_dirs_first;

	if (conf.autols == 1) {
		if (conf.clear_screen == 0)
			putchar('\n');
		reload_dirlist();
	}

	print_reload_msg(NULL, NULL, _("Directories first: %s\n"),
		conf.list_dirs_first ? _("on") : _("off"));
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

	conf.light_mode = !conf.light_mode;
	update_autocmd_opts(AC_LIGHT_MODE);

	if (conf.autols == 1) {
		if (conf.clear_screen == 0)
			putchar('\n');
		reload_dirlist();
	}

	print_reload_msg(NULL, NULL, _("Light mode: %s\n"),
		conf.light_mode == 1 ? _("on") : _("off"));
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

	static int hidden_bk = 0;
	if (conf.show_hidden != 0) {
		hidden_bk = conf.show_hidden;
	} else {
		if (hidden_bk == 0)
			hidden_bk = 1;
	}
	conf.show_hidden = conf.show_hidden > 0 ? 0 : hidden_bk;

	update_autocmd_opts(AC_SHOW_HIDDEN);

	if (conf.autols == 1) {
		if (conf.clear_screen == 0)
			putchar('\n');
		reload_dirlist();
	}

	print_reload_msg(NULL, NULL, _("Hidden files: %s\n"),
		(conf.show_hidden > 0) ? _("on") : _("off"));

	xrl_reset_line_state();
	return FUNC_SUCCESS;
}

static int
rl_open_config(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_kb_cmd("config", 0);
}

static int
rl_open_keybinds(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_kb_cmd("kb edit", 0);
}

static int
rl_open_cscheme(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_kb_cmd("cs edit", 0);
}

static int
rl_open_bm_file(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_kb_cmd("bm edit", 0);
}

static int
rl_open_jump_db(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_kb_cmd("je", 0);
}

static int
rl_open_preview(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (!config_dir || kbind_busy == 1)
		return FUNC_FAILURE;

	char *file = xnmalloc(config_dir_len + 15, sizeof(char));
	snprintf(file, config_dir_len + 15, "%s/preview.clifm", config_dir);

	const int ret = open_file(file);
	free(file);
	rl_on_new_line();
	return ret;
}

static int
rl_open_mime(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_kb_cmd("mm edit", 0);
}

static int
rl_mountpoints(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_kb_cmd("mp", 1);
}

static int
rl_select_all(int count, int key)
{
	UNUSED(count); UNUSED(key);
	exec_prompt_cmds = 1;
	return run_kb_cmd("s ^", 0);
}

static int
rl_deselect_all(int count, int key)
{
	UNUSED(count); UNUSED(key);
	exec_prompt_cmds = 1;
	return run_kb_cmd("ds *", 0);
}

static int
rl_invert_selection(int count, int key)
{
	UNUSED(count); UNUSED(key);
	exec_prompt_cmds = 1;
	return run_kb_cmd("s --invert", 0);
}

static int
rl_bookmarks(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_kb_cmd("bm", 1);
}

static int
rl_selbox(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_kb_cmd("ds", 1);
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

	xrl_update_prompt();
	update_autocmd_opts(AC_SORT);

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
		conf.sort = SORT_TYPES;

	if (conf.autols == 1) {
		sort_switch = 1;
		if (conf.clear_screen == 0)
			putchar('\n');
		reload_dirlist();
		sort_switch = 0;
	}


	xrl_update_prompt();
	update_autocmd_opts(AC_SORT);

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

	printf(_("\n%s: No selected files\n"), func);
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

	fputs(_("\nReady to archive/compress selected files."), stdout);
	return run_kb_cmd("ac sel", 1);
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
	g_prompt_ignore_empty_line = 1;
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

	fputs(_("\nReady to export selected filenames"), stdout);
	return run_kb_cmd("exp sel", 0);
}

static int
rl_move_sel(int count, int key)
{
	UNUSED(count); UNUSED(key);

	if (sel_n == 0)
		return handle_no_sel("m");

	exec_prompt_cmds = 1;
	return run_kb_cmd("m sel", 0);
}

static int
rl_rename_sel(int count, int key)
{
	UNUSED(count); UNUSED(key);

	if (sel_n == 0)
		return handle_no_sel("br");

	exec_prompt_cmds = 1;
	return run_kb_cmd("br sel", 0);
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
	puts("\n");

	/* Reset terminal attributes before exiting. */
	rl_deprep_terminal();
	exit(FUNC_SUCCESS);
}

#ifndef _NO_PROFILES
/* Get current profile and total number of profiles and store this info
 * in pointers CUR and TOTAL. */
static void
get_cur_prof(int *cur, int *total)
{
	if (!profile_names)
		return;

	for (int i = 0; profile_names[i]; i++) {
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

	if (cur_prof == -1 || !profile_names[cur_prof]
	|| total_profs <= 1)
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
	return run_kb_cmd("dh", 0);
}

static int
rl_new_instance(int count, int key)
{
	UNUSED(count); UNUSED(key);
	exec_prompt_cmds = 1;
	return run_kb_cmd("x .", 0);
}

static int
rl_clear_msgs(int count, int key)
{
	UNUSED(count); UNUSED(key);
	exec_prompt_cmds = 1;
	return run_kb_cmd("msg clear", 0);
}

static int
rl_trash_sel(int count, int key)
{
	UNUSED(count); UNUSED(key);

	if (sel_n == 0)
		return handle_no_sel("trash");

	exec_prompt_cmds = 1;
	return run_kb_cmd("t sel", 1);
}

static int
rl_untrash_all(int count, int key)
{
	UNUSED(count); UNUSED(key);
	exec_prompt_cmds = 1;
	return run_kb_cmd("u *", 0);
}

static int
rl_open_sel(int count, int key)
{
	UNUSED(count); UNUSED(key);

	if (sel_n == 0)
		return handle_no_sel("open");

	char cmd[PATH_MAX + 4];
	snprintf(cmd, sizeof(cmd), "o %s", sel_elements[sel_n - 1].name);

	return run_kb_cmd(cmd, 0);
}

static int
run_man_cmd(char *str)
{
	char *mp = NULL;
	const char *p = getenv("MANPAGER");
	if (p) {
		const size_t len = strlen(p);
		mp = xnmalloc(len + 1, sizeof(char *));
		xstrsncpy(mp, p, len + 1);
		unsetenv("MANPAGER");
	}

	const int ret = launch_execl(str) != FUNC_SUCCESS;

	if (mp) {
		setenv("MANPAGER", mp, 1);
		free(mp);
	}

	if (ret != 0) /* Restore prompt in case of failure. */
		rl_reset_line_state();

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
	const int ret = run_man_cmd(cmd);
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
	const int ret = run_man_cmd(cmd);
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
	if (launch_execv(cmd, FOREGROUND, E_NOFLAG) != FUNC_SUCCESS) {
		rl_reset_line_state();
		return FUNC_FAILURE;
	}
	return FUNC_SUCCESS;
}

static int
rl_dir_pinned(int count, int key)
{
	UNUSED(count); UNUSED(key);

	if (pinned_dir == NULL) {
		printf(_("\n%s: No pinned file\n"), PROGRAM_NAME);
		rl_reset_line_state();
		return FUNC_SUCCESS;
	}

	return run_kb_cmd(",", 0);
}

/* Switch to workspace N. */
static int
rl_switch_workspace(const int n)
{
	if (kbind_busy == 1)
		return FUNC_SUCCESS;

	if (rl_line_buffer && *rl_line_buffer)
		rl_delete_text(0, rl_end);

	char t[MAX_INT_STR + 4];
	exec_prompt_cmds = 1;

	if (cur_ws == (n - 1)) {
		/* If the user attempts to switch to the same workspace they're
		 * currently in, switch rather to the previous workspace. */
		if (prev_ws != cur_ws) {
			snprintf(t, sizeof(t), "ws %d", prev_ws + 1);
			return run_kb_cmd(t, 0);
		}

		return FUNC_SUCCESS;
	}

	snprintf(t, sizeof(t), "ws %d", n);
	return run_kb_cmd(t, 0);
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
rl_ws5(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return rl_switch_workspace(5);
}

static int
rl_ws6(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return rl_switch_workspace(6);
}

static int
rl_ws7(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return rl_switch_workspace(7);
}

static int
rl_ws8(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return rl_switch_workspace(8);
}


static int
run_plugin(const int num)
{
	if (kbind_busy == 1)
		return FUNC_SUCCESS;

	if (rl_line_buffer && *rl_line_buffer)
		setenv("CLIFM_LINE", rl_line_buffer, 1);

	char cmd[MAX_INT_STR + 7];
	snprintf(cmd, sizeof(cmd), "plugin%d", num);
	const int ret = run_kb_cmd(cmd, 0);

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
	return run_kb_cmd("view", 0);
}

static int
rl_toggle_only_dirs(int count, int key)
{
	UNUSED(count); UNUSED(key);

	if (kbind_busy == 1)
		return FUNC_SUCCESS;

	conf.only_dirs = !conf.only_dirs;
	update_autocmd_opts(AC_ONLY_DIRS);

	const int exit_status = exit_code;
	if (conf.autols == 1) {
		if (conf.clear_screen == 0)
			putchar('\n');
		reload_dirlist();
	}

	print_reload_msg(NULL, NULL, _("Only directories: %s\n"),
		conf.only_dirs > 0 ? _("on") : _("off"));
	xrl_reset_line_state();
	return exit_status;
}

#ifndef _NO_HIGHLIGHT
static void
print_highlight_string(char *s, const int insert_point)
{
	if (!s || !*s)
		return;

	rl_delete_text(insert_point, rl_end);
	rl_point = rl_end = insert_point;
	fputs(tx_c, stdout);
	cur_color = tx_c;

	char q[PATH_MAX + 1];
	for (size_t i = 0, l = 0; s[i]; i++) {
		rl_highlight(s, i, SET_COLOR);

		if ((signed char)s[i] < 0) {
			q[l++] = s[i];

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
print_cmdhist_line(const size_t n, const int beg_line)
{
#ifndef _NO_SUGGESTIONS
	if (wrong_cmd == 1)
		recover_from_wrong_cmd();
#endif /* !_NO_SUGGESTIONS */

	curhistindex = n;

	HIDE_CURSOR;
	const int rl_point_bk = rl_point;

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
	size_t p = curhistindex;
	cmdhist_flag = 1;

	if (key == 65) { /* Up arrow key */
		if (p == 0) return FUNC_FAILURE;
		p--;
	} else { /* Down arrow key */
		if (rl_end == 0)
			return FUNC_SUCCESS;
		if (++p >= current_hist_n) {
			rl_replace_line("", 1);
			curhistindex++;
			return FUNC_SUCCESS;
		}
	}

	if (!history[p].cmd)
		return FUNC_FAILURE;

	curhistindex = p;

	return print_cmdhist_line(p, 1);
}

static inline int
handle_cmdhist_middle(int key)
{
	int found = 0;
	const size_t s_rl_point = rl_point < 0 ? 0 : (size_t)rl_point;
	size_t p = curhistindex;

	if (key == 65) { /* Up arrow key */
		if (p == 0) return FUNC_FAILURE;
		p--;

		while (history[p].cmd) {
			if (strncmp(rl_line_buffer, history[p].cmd, s_rl_point) == 0
			&& strcmp(rl_line_buffer, history[p].cmd) != 0) {
				found = 1; break;
			}
			if (p == 0) break;
			p--;
		}
	} else { /* Down arrow key */
		if (++p >= current_hist_n) return FUNC_FAILURE;

		while (history[p].cmd) {
			if (strncmp(rl_line_buffer, history[p].cmd, s_rl_point) == 0
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
		suggestion_buf = NULL;
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

	if (kbind_busy == 1)
		return FUNC_SUCCESS;

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

	const int exit_status = exit_code;
	if (conf.autols == 1) {
		if (conf.clear_screen == 0)
			putchar('\n');
		reload_dirlist();
	}

	print_reload_msg(NULL, NULL, _("Disk usage analyzer: %s\n"),
		xargs.disk_usage_analyzer == 1 ? _("on") : _("off"));
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

	char *end_buf = NULL;
	if (rl_point < rl_end) { /* Somewhere before the end of the line */
		end_buf = rl_copy_text(rl_point, rl_end);
		rl_delete_text(rl_point, rl_end);
	}

	char *b = rl_line_buffer;

	if (b[rl_point - 1] == '/' || b[rl_point - 1] == ' ') {
		rl_point--;
		b[rl_point] = '\0';
		rl_end--;
	}

	int n = 0;
	char *p = xstrrpbrk(b, WORD_DELIMITERS);
	if (p)
		n = (int)(p - b) + (p[1] ? 1 : 0);

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
	xargs.virtual_dir_full_paths = !xargs.virtual_dir_full_paths;

	filesn_t i = g_files_num;
	while (--i >= 0) {
		char *rp = xrealpath(file_info[i].name, NULL);
		if (!rp) continue;

		char *p = NULL;
		if (xargs.virtual_dir_full_paths != 1) {
			if ((p = strrchr(rp, '/')) && p[1])
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
	print_reload_msg(NULL, NULL, _("Switched to %s names\n"),
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

	return run_kb_cmd("pg", 0);
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

static int
rl_toggle_vi_mode(int count, int key)
{
	UNUSED(count); UNUSED(key);

	Keymap keymap = rl_get_keymap();
	if (keymap == vi_insertion_keymap) {
		keymap = rl_get_keymap_by_name("emacs-standard");
		rl_set_keymap(keymap);
		rl_editing_mode = RL_EMACS_MODE;
	} else if (keymap == emacs_standard_keymap) {
		keymap = rl_get_keymap_by_name("vi-insert");
		rl_set_keymap(keymap);
		rl_editing_mode = RL_VI_MODE;
	} else {
		return FUNC_SUCCESS;
	}

	const size_t n = rl_prompt ? count_chars(rl_prompt, '\n') : 0;
	if (n > 0 && n <= INT_MAX)
		MOVE_CURSOR_UP((int)n);
	putchar('\r');
	ERASE_TO_RIGHT_AND_BELOW;
	fflush(stdout);
	xrl_reset_line_state();
	rl_update_prompt();

	return FUNC_SUCCESS;
}

/* Used to disable keybindings. */
static int
do_nothing(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return FUNC_SUCCESS;
}

/* Hold kebinding names and associated functions. */
struct keyfuncs_t {
	const char *name;
	int (*func)(int, int);
};

/* Return a pointer to the function associated to the keybinding name NAME. */
static int (*get_function(struct keyfuncs_t *s, const char *name))(int, int)
{
	while (s->name) {
		if (*name == *s->name && strncmp(name, s->name, strlen(s->name)) == 0)
			return s->func;
		s++;
	}

	return NULL;
}

static void
set_keybinds_from_file(void)
{
	struct keyfuncs_t keys[] = {
		{"show-manpage", rl_manpage}, {"show-cmds", rl_cmds_help},
		{"show-kbinds", rl_kbinds_help}, {"parent-dir", rl_dir_parent},
		{"previous-dir", rl_dir_previous}, {"next-dir", rl_dir_next},
		{"home-dir", rl_dir_home}, {"root-dir", rl_dir_root},
		{"workspace1", rl_ws1}, {"workspace2", rl_ws2},
		{"workspace3", rl_ws3}, {"workspace4", rl_ws4},
		{"workspace5", rl_ws5}, {"workspace6", rl_ws6},
		{"workspace7", rl_ws7}, {"workspace8", rl_ws8},

		{"create-file", rl_create_file}, {"archive-sel", rl_archive_sel},
		{"open-sel", rl_open_sel}, {"export-sel", rl_export_sel},
		{"move-sel", rl_move_sel}, {"rename-sel", rl_rename_sel},
		{"remove-sel", rl_remove_sel}, {"trash-sel", rl_trash_sel},
		{"untrash-all", rl_untrash_all}, {"paste-sel", rl_paste_sel},
		{"copy-sel", rl_paste_sel}, {"select-all", rl_select_all},
		{"deselect-all", rl_deselect_all},

		{"open-mime", rl_open_mime}, {"open-jump-db", rl_open_jump_db},
		{"open-preview", rl_open_preview}, {"open-config", rl_open_config},
		{"edit-color-scheme", rl_open_cscheme},
		{"open-keybinds", rl_open_keybinds}, {"open-bookmarks", rl_open_bm_file},

		{"toggle-virtualdir-full-paths", rl_toggle_virtualdir_full_paths},
		{"clear-msgs", rl_clear_msgs},
#ifndef _NO_PROFILES
		{"next-profile", rl_profile_next},
		{"previous-profile", rl_profile_previous},
#endif // _NO_PROFILES
		{"quit", rl_quit}, {"lock", rl_lock}, {"refresh-screen", rl_refresh},
		{"clear-line", rl_clear_line}, {"toggle-hidden", rl_toggle_hidden_files},
		{"toggle-long", rl_toggle_long_view},
		{"toggle-follow-links-long", rl_toggle_follow_link_long},
		{"toggle-light", rl_toggle_light_mode},
		{"invert-selection", rl_invert_selection},
		{"dirs-first", rl_toggle_dirs_first},
		{"sort-previous", rl_sort_previous}, {"sort-next", rl_sort_next},
		{"only-dirs", rl_toggle_only_dirs},
		{"run-pager", rl_run_pager},

		{"launch-view", rl_launch_view}, {"new-instance", rl_new_instance},
		{"show-dirhist", rl_dirhist}, {"bookmarks", rl_bookmarks},
		{"mountpoints", rl_mountpoints}, {"selbox", rl_selbox},
		{"prepend-sudo", rl_prepend_sudo},
		{"toggle-disk-usage", rl_toggle_disk_usage},
		{"toggle-max-name-len", rl_toggle_max_filename_len},
		{"cmd-hist", rl_cmdhist_tab},

		{"plugin1", rl_plugin1}, {"plugin2", rl_plugin2},
		{"plugin3", rl_plugin3}, {"plugin4", rl_plugin4},
		{"plugin5", rl_plugin5}, {"plugin6", rl_plugin6},
		{"plugin7", rl_plugin7}, {"plugin8", rl_plugin8},
		{"plugin9", rl_plugin9}, {"plugin10", rl_plugin10},
		{"plugin11", rl_plugin11}, {"plugin12", rl_plugin12},
		{"plugin13", rl_plugin13}, {"plugin14", rl_plugin14},
		{"plugin15", rl_plugin15}, {"plugin16", rl_plugin16},
		{NULL, NULL}
	};

	for (size_t i = 0; i < kbinds_n; i++)
		rl_bind_keyseq(kbinds[i].key, get_function(keys, kbinds[i].function));

	const char *vi_mode_keyseq = find_key("toggle-vi-mode");
	if (vi_mode_keyseq) {
		rl_bind_keyseq(vi_mode_keyseq, rl_toggle_vi_mode);
		Keymap keymap = rl_get_keymap_by_name("vi-insert");
		if (keymap)
			rl_bind_keyseq_in_map(vi_mode_keyseq, rl_toggle_vi_mode, keymap);
	}
}

static void
set_default_keybinds(void)
{
	struct keyfuncs_t keys[] = {
		/* Help */
		{"\\eOP", rl_manpage}, {"\\e[11~", rl_manpage},
		{"\\eOQ", rl_cmds_help}, {"\\e[12~", rl_cmds_help},
		{"\\eOR", rl_kbinds_help}, {"\\e[13~", rl_kbinds_help},
		/* Navigation */
		{"\\M-u", rl_dir_parent}, {"\\e[a", rl_dir_parent},
		{"\\e[2A", rl_dir_parent}, {"\\e[1;2A", rl_dir_parent},
		{"\\M-j", rl_dir_previous}, {"\\e[d", rl_dir_previous},
		{"\\e[2D", rl_dir_previous}, {"\\e[1;2D", rl_dir_previous},
		{"\\M-k", rl_dir_next}, {"\\e[c", rl_dir_next},
		{"\\e[2C", rl_dir_next}, {"\\e[1;2C", rl_dir_next},
		{"\\M-e", rl_dir_home}, {"\\e[1~", rl_dir_home},
		{"\\e[7~", rl_dir_home}, {"\\e[H", rl_dir_home},
		{"\\M-r", rl_dir_root}, {"\\e/", rl_dir_root},
		{"\\M-p", rl_dir_pinned},
		{"\\M-1", rl_ws1}, {"\\M-2", rl_ws2},
		{"\\M-3", rl_ws3}, {"\\M-4", rl_ws4},
		{"\\M-5", rl_ws5}, {"\\M-6", rl_ws6},
		{"\\M-7", rl_ws7}, {"\\M-8", rl_ws8},
		/* Operations on files */
		{"\\M-n", rl_create_file}, {"\\C-\\M-a", rl_archive_sel},
		{"\\C-\\M-e", rl_export_sel}, {"\\C-\\M-r", rl_rename_sel},
		{"\\C-\\M-d", rl_remove_sel}, {"\\C-\\M-t", rl_trash_sel},
		{"\\C-\\M-v", rl_paste_sel}, {"\\M-a", rl_select_all},
		{"\\M-d", rl_deselect_all}, {"\\e[Z", rl_invert_selection},
		{"\\M-v", rl_prepend_sudo},
		/* Config files */
		{"\\e[17~", rl_open_mime}, {"\\e[18~", rl_open_preview},
		{"\\e[19~", rl_open_cscheme}, {"\\e[20~", rl_open_keybinds},
		{"\\e[21~", rl_open_config}, {"\\e[23~", rl_open_bm_file},
		/* Settings */
		{"\\M-w", rl_toggle_virtualdir_full_paths},
		{"\\M-t", rl_clear_msgs}, {"\\M-o", rl_lock},
		{"\\C-r", rl_refresh}, {"\\M-c", rl_clear_line},
		{"\\M-i", rl_toggle_hidden_files}, {"\\M-.", rl_toggle_hidden_files},
		{"\\M-l", rl_toggle_long_view}, {"\\M-+", rl_toggle_follow_link_long},
		{"\\M-y", rl_toggle_light_mode}, {"\\M-g", rl_toggle_dirs_first},
		{"\\M-z", rl_sort_previous}, {"\\M-x", rl_sort_next},
		{"\\M-,", rl_toggle_only_dirs}, {"\\M-0", rl_run_pager},
		/* Misc */
		{"\\M--", rl_launch_view}, {"\\C-\\M-x", rl_new_instance},
		{"\\M-h", rl_dirhist}, {"\\M-b", rl_bookmarks},
		{"\\M-m", rl_mountpoints}, {"\\M-s", rl_selbox},
		{"\\C-\\M-l", rl_toggle_max_filename_len},
		{"\\C-\\M-i", rl_toggle_disk_usage}, {"\\e[24~", rl_quit},
/*		{"\\C-\\M-g", rl_open_sel}, // Disabled by default
		{"\\C-\\M-n", rl_move_sel}, // Disabled by default
		{"\\C-\\M-u", rl_untrash_all}, // Disabled by default
		{"\\e[18~", rl_open_jump_db}, // Disabled by default */
		{NULL, NULL}
	};

	for (size_t i = 0; keys[i].name; i++)
		rl_bind_keyseq(keys[i].name, keys[i].func);

	rl_bind_keyseq("\\C-\\M-j", rl_toggle_vi_mode);
	Keymap keymap = rl_get_keymap_by_name("vi-insert");
	if (keymap)
		rl_bind_keyseq_in_map("\\C-\\M-j", rl_toggle_vi_mode, keymap);
}

static void
set_hardcoded_keybinds(void)
{
	struct keyfuncs_t keys[] = {
		{"\\M-*", do_nothing}, {"\x1b[42;3u", do_nothing},
#ifndef __HAIKU__
		{"\\C-l", rl_refresh}, {"\x1b[108;5u", rl_refresh},
		{"\\C-p", rl_cmdhist}, {"\\C-n", rl_cmdhist},
#endif /* !__HAIKU__ */
		{"\x1b[A", rl_cmdhist}, {"\x1b[B", rl_cmdhist},
		{"\\M-q", rl_del_last_word}, {"\x1b[113;3u", rl_del_last_word},
#ifndef _NO_SUGGESTIONS
# ifndef __HAIKU__
		{"\x1b[102;5u", rl_accept_suggestion}, {"\\C-f", rl_accept_suggestion},
		{"\x1b[C", rl_accept_suggestion}, {"\x1bOC", rl_accept_suggestion},
		{"\x1b[102;3u", rl_accept_first_word}, {"\\M-f", rl_accept_first_word},
		{"\x1b[3C", rl_accept_first_word}, {"\x1b\x1b[C", rl_accept_first_word},
		{"\x1b[1;3C", rl_accept_first_word},
# else
		{"\x1bOC", rl_accept_suggestion}, {"\\C-f", rl_accept_first_word},
# endif /* !__HAIKU__ */
#endif /* !_NO_SUGGESTIONS */
		{NULL, NULL}
	};

	for (size_t i = 0; keys[i].name; i++)
		rl_bind_keyseq(keys[i].name, keys[i].func);

	rl_bind_key('\t', rl_tab_comp);
}

/* Disable readline keybindings conflicting with clifm's.
 * This function is called before reading the readline config file (by
 * default ~/.inputrc), so that the user can rebind them using any of config
 * files (either readline.clifm or keybindings.clifm). */
void
disable_rl_conflicting_kbinds(void)
{
	const char *const keys[] = {"\\x1b\\xd", "\\C-x(", "\\C-x\\C-u",
		"\\C-x\\C-x", "\\C-x\\C-g", "\\C-x\\C-?","\\C-x\\C-r", "\\C-xe",
		"\\C-x", "\\C-q", "\\C-d", "\\C-]", "\\e\\C-]", "\\e\\C-i",
		"\\e\\", "\\e\\e", "\\eb", "\\e.", "\\et", "\\ey", "\\e-",
		"\\eu", "\\M-5", "\\M-6", "\\M-7", "\\M-8", "\\M-9", NULL};

	for (size_t i = 0; keys[i]; i++)
		rl_bind_keyseq(keys[i], do_nothing);
}

void
readline_kbinds(void)
{
	if (kbinds_file)
		set_keybinds_from_file();
	else
		set_default_keybinds();

	set_hardcoded_keybinds();
}
