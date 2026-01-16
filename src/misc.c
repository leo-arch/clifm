/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* misc.c -- functions that do not fit in any other file */

/* The err function is based on littlstar's asprintf implementation
 * (https://github.com/littlstar/asprintf.c/blob/master/asprintf.c),
 * licensed under MIT.
 * All changes are licensed under GPL-2.0-or-later. */

#include "helpers.h"

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h> /* tcsetattr */

#ifdef __sun
# include <sys/termios.h> /* TIOCGWINSZ */
#endif /* __sun */

#if defined(__NetBSD__) || defined(__FreeBSD__)
# include <sys/param.h>
# include <sys/sysctl.h>
#endif /* __NetBSD__ || __FreeBSD__ */

#ifdef __OpenBSD__
typedef char *rl_cpvfunc_t;
typedef void rl_macro_print_func_t (const char *, const char *, int, const char *);
# include <ereadline/readline/readline.h>
#else
# include <readline/readline.h>
#endif /* __OpenBSD__ */

#include <time.h>
#include <unistd.h>
#ifdef LINUX_INOTIFY
# include <sys/inotify.h>
#endif /* LINUX_INOTIFY */

#include "aux.h"
#include "autocmds.h" /* update_autocmd_opts() */
#include "bookmarks.h"
#include "checks.h"
#include "file_operations.h"
#include "history.h"
#include "init.h"
#include "jump.h"
#include "listing.h"
#include "messages.h"
#include "navigation.h"
#include "readline.h"
#include "remotes.h"
#include "spawn.h"

char *
gen_diff_str(const int diff)
{
	if (diff == 1)
		return "\x1b[1C";
	if (diff == 2)
		return "\x1b[2C";
	if (diff == 3)
		return "\x1b[3C";

	static char diff_str[MAX_INT_STR + 4];
	snprintf(diff_str, sizeof(diff_str), "\x1b[%dC", diff);

	return diff_str;
}

int
is_blank_name(const char *s)
{
	if (!s || !*s)
		return 1;

	int blank = 1;

	while (*s) {
		if (*s != ' ' && *s != '\n' && *s != '\t') {
			blank = 0;
			break;
		}
		s++;
	}

	return blank;
}

/* Prompt for a new name using MSG as prompt.
 * If OLD_NAME is not NULL, it will be used as template for the new name.
 * If the entered name is quoted, it will be returned verbatim (without quotes),
 * and QUOTED will be set to 1 (the caller should not perform expansions on
 * this name). */
char *
get_newname(const char *msg, char *old_name, int *quoted)
{
	rl_nohist = 1;
	alt_prompt = FILES_PROMPT;
	int poffset_bk = prompt_offset;
	prompt_offset = 3;

	char *n = (old_name && *old_name) ? unescape_str(old_name, 0) : (char *)NULL;
	char *input = secondary_prompt((msg && *msg) ? msg : "> ",
		n ? n : (char *)NULL);
	free(n);

	char *new_name = (char *)NULL;
	if (!input)
		goto END;

	char *p = remove_quotes(input);
	if (!p || !*p) /* Input was "" (empty string) */
		goto END;

	if (p != input) {
		/* Quoted: copy input verbatim (without quotes) */
		*quoted = 1;
		new_name = savestring(p, strlen(p));
		goto END;
	}

	char *deq = unescape_str(p, 0);
	char *tmp = deq ? deq : p;

	size_t len = strlen(tmp);
	int i = len > 1 ? (int)len - 1 : 0;
	while (tmp[i] == ' ') {
		tmp[i] = '\0';
		i--;
	}

	new_name = savestring(tmp, (size_t)i + 1);
	free(deq);

END:
	free(input);
	alt_prompt = rl_nohist = 0;
	prompt_offset = poffset_bk;

	return new_name;
}

/* Set ELN color according to the current workspace. */
void
set_eln_color(void)
{
	char *cl = (char *)NULL;

	switch (cur_ws) {
	case 0: cl = ws1_c; break;
	case 1: cl = ws2_c; break;
	case 2: cl = ws3_c; break;
	case 3: cl = ws4_c; break;
	case 4: cl = ws5_c; break;
	case 5: cl = ws6_c; break;
	case 6: cl = ws7_c; break;
	case 7: cl = ws8_c; break;
	default: break;
	}

	if (!cl || !*cl) {
		xstrsncpy(el_c, term_caps.color >= 256
			? DEF_EL_C256 : DEF_EL_C, sizeof(el_c));
		return;
	}

	/* Remove leading and trailing control characters (\001 and \002). */
	size_t cl_len = 0;
	if (*cl == 001) {
		cl++;
		cl_len = strlen(cl);
		if (cl_len > 0 && cl[cl_len - 1] == 002)
			cl[cl_len - 1] = '\0';
	}

	xstrsncpy(el_c, cl, sizeof(el_c));

	if (cl_len > 0)
		cl[cl_len - 1] = 002;
}

/* Custom POSIX implementation of GNU asprintf() modified to log program
 * messages.
 *
 * MSG_TYPE is one of: 'e', 'f', 'w', 'n', zero (meaning this
 * latter that no message mark (E, W, or N) will be added to the prompt).
 * If MSG_TYPE is 'n' the message is not not logged.
 * 'f' means that the message must be printed forcefully, even if identical
 * to the previous one, without printing any message mark.
 * MSG_TYPE also accepts ERR_NO_LOG (-1) and ERR_NO_STORE (-2) as values:
 * ERR_NO_LOG: Print the message but do not log it.
 * ERR_NO_STORE: Log but do not store the message in the messages array.
 *
 * PROMPT_FLAG tells whether to print the message immediately before the next
 * prompt or rather in place.
 *
 * This function guarantees not to modify the value of errno, usually passed
 * as part of the format string to print error messages.
 * */
__attribute__((__format__(__printf__, 3, 0)))
/* We use __attribute__ here to silence clang warning: "format string is
 * not a string literal" */
int
err(const int msg_type, const int prompt_flag, const char *format, ...)
{
	const int saved_errno = errno;
	va_list arglist;

	va_start(arglist, format);
	int size = vsnprintf((char *)NULL, 0, format, arglist);
	va_end(arglist);

	if (size < 0)
		goto ERROR;

	const size_t n = (size_t)size + 1;
	char *buf = xnmalloc(n, sizeof(char));
	va_start(arglist, format);
	size = vsnprintf(buf, n, format, arglist);
	va_end(arglist);

	if (size < 0 || !buf || !*buf)
		{free(buf); goto ERROR;}

	/* If the new message is the same as the last message, skip it. */
	if (msgs_n > 0 && msg_type != 'f' && *messages[msgs_n - 1].text == *buf
	&& strcmp(messages[msgs_n - 1].text, buf) == 0)
		{free(buf); goto ERROR;}

	if (msg_type >= 'e') {
		switch (msg_type) {
		case 'e': pmsg = ERROR; msgs.error++; break;
		case 'w': pmsg = WARNING; msgs.warning++; break;
		case 'n': pmsg = NOTICE; msgs.notice++; break;
		default:  pmsg = NOMSG; break;
		}
	}

	int logme = msg_type == ERR_NO_LOG ? 0 : (msg_type == 'n' ? -1 : 1);
	int add_to_msgs_list = 1;

	if (msg_type == ERR_NO_STORE) {
		add_to_msgs_list = 0;
		logme = 1;
/*		if (prompt_flag == NOPRINT_PROMPT && term_caps.unicode == 1) {
			// Invoked as xerror()
			printf("%s%s%s ", xf_c, ERROR_PTR_STR_U, df_c);
			fflush(stdout);
		} */
	}

	log_msg(buf, prompt_flag, logme, add_to_msgs_list);

	free(buf);
	errno = saved_errno;
	return FUNC_SUCCESS;

ERROR:
	errno = saved_errno;
	return FUNC_FAILURE;
}

/* Print format string MSG, as "> MSG" (colored), if autols is on, or just
 * as "MSG" if off.
 * Use PTR as pointer, or ">" if PTR is NULL.
 * Use COLOR as pointer color, or mi_c if COLOR is NULL.
 * This function is used to inform the user about changes that require a
 * a file list reload (either upon files or interface modifications). */
__attribute__((__format__(__printf__, 3, 4)))
int
print_reload_msg(const char *ptr, const char *color, const char *msg, ...)
{
	va_list arglist;

	va_start(arglist, msg);
	const int size = vsnprintf((char *)NULL, 0, msg, arglist);
	va_end(arglist);

	if (size < 0)
		return FUNC_FAILURE;

	if (conf.autols == 1)
		printf("%s%s%s ", color ? color : mi_c,
			ptr ? ptr : SET_MSG_PTR, df_c);

	char *buf = xnmalloc((size_t)size + 1, sizeof(char));

	va_start(arglist, msg);
	vsnprintf(buf, (size_t)size + 1, msg, arglist);
	va_end(arglist);

	fputs(buf, stdout);
	free(buf);

	return FUNC_SUCCESS;
}

void
set_filter_type(const char c)
{
	if (c == '=')
		filter.type = FILTER_FILE_TYPE;
	else if (c == '@')
		filter.type = FILTER_MIME_TYPE; /* UNIMPLEMENTED */
	else
		filter.type = FILTER_FILE_NAME;
}

static int
unset_filter(void)
{
	if (!filter.str) {
		puts(_("ft: No filter set"));
		return FUNC_SUCCESS;
	}

	free(filter.str);
	filter.str = (char *)NULL;
	filter.rev = 0;
	filter.type = FILTER_NONE;
	regfree(&regex_exp);

	if (conf.autols == 1)
		reload_dirlist();

	print_reload_msg(NULL, NULL, _("Filter unset\n"));
	return FUNC_SUCCESS;
}

static int
validate_file_type_filter(void)
{
	if (!filter.str || !*filter.str || *filter.str != '='
	|| !filter.str[1] || filter.str[2])
		return FUNC_FAILURE;

	const char c = filter.str[1];
	if (c == 'b' || c == 'c' || c == 'd' || c == 'f'
#ifdef SOLARIS_DOORS
	|| c == 'l' || c == 'p' || c == 's' || c == 'O' || c == 'P')
#else
	|| c == 'l' || c == 'p' || c == 's')
#endif /* SOLARIS_DOORS */
		return FUNC_SUCCESS;

	if (conf.light_mode == 1)
		return FUNC_FAILURE;

	if (c == 'g' || c == 'h' || c == 'o' || c == 't'
	|| c == 'u' || c == 'x' || c == 'D' || c == 'F' || c == 'L')
		return FUNC_SUCCESS;

	return FUNC_FAILURE;
}

static int
compile_filter(void)
{
	if (filter.type == FILTER_FILE_NAME) {
		const int ret =
			regcomp(&regex_exp, filter.str, REG_NOSUB | REG_EXTENDED);
		if (ret != FUNC_SUCCESS) {
			xregerror("ft", filter.str, ret, regex_exp, 0);
			regfree(&regex_exp);
			goto ERR;
		}
	} else if (filter.type == FILTER_FILE_TYPE) {
		if (validate_file_type_filter() != FUNC_SUCCESS) {
			xerror("%s\n", _("ft: Invalid file type filter"));
			goto ERR;
		}
	} else {
		xerror("%s\n", _("ft: Invalid filter"));
		goto ERR;
	}

	if (conf.autols == 1)
		reload_dirlist();

	return FUNC_SUCCESS;

ERR:
	free(filter.str);
	filter.str = (char *)NULL;
	filter.type = FILTER_NONE;
	return FUNC_FAILURE;
}

int
filter_function(char *arg)
{
	if (!arg) {
		printf(_("Current filter: %c%s\n"), filter.rev == 1 ? '!' : 0,
			filter.str ? filter.str : _("none"));
		return FUNC_SUCCESS;
	}

	if (IS_HELP(arg)) {
		puts(_(FILTER_USAGE));
		return FUNC_SUCCESS;
	}

	if (*arg == 'u' && strcmp(arg, "unset") == 0) {
		const int ret = unset_filter();
		update_autocmd_opts(AC_FILTER);
		return ret;
	}

	free(filter.str);
	regfree(&regex_exp);

	if (*arg == '!') {
		filter.rev = 1;
		arg++;
	} else {
		filter.rev = 0;
	}

	char *p = arg;
	if (*arg == '\'' || *arg == '"') {
		p = remove_quotes(arg);
		if (!p) {
			xerror("%s\n", _("ft: Error removing quotes: Filter unset"));
			return FUNC_FAILURE;
		}
	}

	set_filter_type(*p);
	filter.str = savestring(p, strlen(p));
	update_autocmd_opts(AC_FILTER);

	return compile_filter();
}

/* Check whether the conditions to run the new_instance function are
 * fulfilled */
static int
check_new_instance_init_conditions(void)
{
	if (!(flags & GUI)) {
		xerror(_("%s: Function only available for graphical "
			"environments\n"), PROGRAM_NAME);
		return FUNC_FAILURE;
	}

	if (!conf.term || !*conf.term) {
		xerror(_("%s: Default terminal not set. Use the "
			"configuration file (F10) to set it.\n"), PROGRAM_NAME);
		return FUNC_FAILURE;
	}

	/* Check command existence. */
	char *s = strchr(conf.term, ' ');
	if (s)
		*s = '\0';

	if (is_cmd_in_path(conf.term) == 0) {
		xerror("%s: %s: %s\n", PROGRAM_NAME, conf.term, NOTFOUND_MSG);
		if (s)
			*s = ' ';
		return E_NOTFOUND;
	}

	if (s)
		*s = ' ';

	return FUNC_SUCCESS;
}

/* Just check if DIR exists and it is a directory. */
static int
check_dir(char **dir)
{
	int ret = FUNC_SUCCESS;
	struct stat attr;
	if (stat(*dir, &attr) == -1) {
		xerror("%s: '%s': %s\n", PROGRAM_NAME, *dir, strerror(errno));
		return errno;
	}

	if (!S_ISDIR(attr.st_mode)) {
		xerror(_("%s: '%s': Not a directory\n"), PROGRAM_NAME, *dir);
		return ENOTDIR;
	}

	return ret;
}

/* Construct absolute path for DIR */
static char *
get_path_dir(char **dir)
{
	char *path_dir = (char *)NULL;

	if (*(*dir) != '/') {
		const size_t len = strlen(workspaces[cur_ws].path) + strlen(*dir) + 2;
		path_dir = xnmalloc(len, sizeof(char));
		snprintf(path_dir, len, "%s/%s", workspaces[cur_ws].path, *dir);
		free(*dir);
		*dir = (char *)NULL;
	} else {
		path_dir = *dir;
	}

	return path_dir;
}

/* Get command to be executed by the new_instance function, only if
 * CONF.TERM (global) contains spaces. Otherwise, new_instance will try
 * "CONF.TERM clifm". */
static char **
get_cmd(char *dir, char *_sudo, char *self, const int sudo)
{
	if (!conf.term || !strchr(conf.term, ' '))
		return (char **)NULL;

	char **tmp_term = get_substr(conf.term, ' ', 0);
	if (!tmp_term)
		return (char **)NULL;

	int i;
	for (i = 0; tmp_term[i]; i++);

	int num = i;
	char **cmd = xnmalloc((size_t)i + (sudo == 1 ? 4 : 3), sizeof(char *));

	for (i = 0; tmp_term[i]; i++) {
		cmd[i] = savestring(tmp_term[i], strlen(tmp_term[i]));
		free(tmp_term[i]);
	}
	free(tmp_term);

	i = num - 1;
	int plus = 1;

	size_t len = 0;
	if (sudo == 1) {
		len = strlen(self);
		cmd[i + plus] = xnmalloc(len + 1, sizeof(char));
		xstrsncpy(cmd[i + plus], _sudo, len + 1);
		plus++;
	}

	len = strlen(self);
	cmd[i + plus] = xnmalloc(len + 1, sizeof(char));
	xstrsncpy(cmd[i + plus], self, len + 1);
	plus++;
	len = strlen(dir);
	cmd[i + plus] = xnmalloc(len + 1, sizeof(char));
	xstrsncpy(cmd[i + plus], dir, len + 1);
	plus++;
	cmd[i + plus] = (char *)NULL;

	return cmd;
}

/* Print the command CMD and ask the user for confirmation.
 * Returns 1 if yes or 0 if no. */
int
confirm_sudo_cmd(char **cmd)
{
	if (!cmd)
		return 0;

	size_t i;
	for (i = 0; cmd[i]; i++) {
		fputs(cmd[i], stdout);
		putchar(' ');
	}
	if (i > 0)
		putchar('\n');

	return rl_get_y_or_n(_("Run command?"), 0);
}

/* Launch a new instance using CMD. If CMD is NULL, try "CONF.TERM clifm".
 * Returns the exit status of the executed command. */
static int
launch_new_instance_cmd(char ***cmd, char **self, char **sudo_prog,
	char **dir, int sudo)
{
	int ret = 0;
#if defined(__HAIKU__)
	sudo = 0;
#endif /* __HAIKU__ */

	if (*cmd) {
		ret = (sudo == 0 || confirm_sudo_cmd(*cmd) == 1)
			? launch_execv(*cmd, BACKGROUND, E_SETSID)
			: FUNC_SUCCESS;
		for (size_t i = 0; (*cmd)[i]; i++)
			free((*cmd)[i]);
		free(*cmd);
	} else {
		if (sudo == 1) {
			char *tcmd[] = {conf.term, *sudo_prog, *self, *dir, NULL};
			ret = (confirm_sudo_cmd(tcmd) == 1)
				? launch_execv(tcmd, BACKGROUND, E_SETSID)
				: FUNC_SUCCESS;
		} else {
			char *tcmd[] = {conf.term, *self, *dir, NULL};
			ret = launch_execv(tcmd, BACKGROUND, E_SETSID);
		}
	}

	free(*sudo_prog);
	free(*self);
	free(*dir);

	return ret;
}

/* After the last line of new_instance */
// cppcheck-suppress syntaxError

/* Open DIR in a new instance of the program (using TERM, set in the config
 * file, as terminal emulator). */
int
new_instance(char *dir, int sudo)
{
	int ret = check_new_instance_init_conditions();
	if (ret != FUNC_SUCCESS)
		return ret;

	if (!dir)
		return EINVAL;

	/* Do not run with sudo if already root */
	if (user.uid == 0)
		sudo = 0;

	char *sudo_prog = (char *)NULL;
#ifndef __HAIKU__
	if (sudo == 1 && !(sudo_prog = get_sudo_path()))
		return errno;
#endif /* !__HAIKU__ */

	char *deq_dir = unescape_str(dir, 0);
	if (!deq_dir) {
		free(sudo_prog);
		xerror(_("%s: '%s': Error unescaping filename\n"), PROGRAM_NAME, dir);
		return FUNC_FAILURE;
	}

	char *self = get_cmd_path(PROGRAM_NAME);
	if (!self) {
		free(sudo_prog); free(deq_dir);
		xerror("%s: %s: %s\n", PROGRAM_NAME, PROGRAM_NAME, strerror(errno));
		return errno;
	}

	ret = check_dir(&deq_dir);
	if (ret != FUNC_SUCCESS) {
		free(deq_dir); free(self); free(sudo_prog);
		return ret;
	}

	char *path_dir = get_path_dir(&deq_dir);
	char **cmd = get_cmd(path_dir, sudo_prog, self, sudo);
	return launch_new_instance_cmd(&cmd, &self, &sudo_prog, &path_dir, sudo);
}

/* Import (copy to main config file) aliases from the file named FILE.
 * Returns 0 on success or >0 on error. */
int
alias_import(char *file)
{
	if (xargs.stealth_mode == 1) {
		printf("%s: alias: %s\n", PROGRAM_NAME, STEALTH_DISABLED);
		return FUNC_SUCCESS;
	}

	if (!file)
		return FUNC_FAILURE;

	char *npath = normalize_path(file, strlen(file));
	if (!npath) {
		xerror(_("alias: '%s': Error normalizing filename\n"), file);
		return FUNC_FAILURE;
	}

	char rfile[PATH_MAX + 1]; *rfile = '\0';
	xstrsncpy(rfile, npath, sizeof(rfile));
	free(npath);

	/* Open the file to import aliases from. */
	int fd;
	FILE *fp = open_fread(rfile, &fd);
	if (!fp) {
		xerror("alias: '%s': %s\n", rfile, strerror(errno));
		return errno;
	}

	/* Open clifm's config file as well. */
	FILE *config_fp = open_fappend(config_file);
	if (!config_fp) {
		xerror("alias: '%s': %s\n", config_file, strerror(errno));
		fclose(fp);
		return errno;
	}

	size_t line_size = 0, i;
	char *line = (char *)NULL;
	size_t alias_found = 0, alias_imported = 0;
	int first = 1;

	while (getline(&line, &line_size, fp) > 0) {
		if (*line != 'a' || strncmp(line, "alias ", 6) != 0)
			continue;

		alias_found++;

		/* If alias name conflicts with some internal command, skip it. */
		char *alias_name = strbtw(line, ' ', '=');
		if (!alias_name)
			continue;

		if (is_internal_cmd(alias_name, ALL_CMDS, 1, 1)) {
			xerror(_("'%s': Alias conflicts with internal "
				"command\n"), alias_name);
			free(alias_name);
			continue;
		}

		char *p = line + 6;
		/* p points now to the beginning of the alias name
		 * (because "alias " == 6). */

		char *tmp = strchr(p, '=');
		if (!tmp || !tmp[1] || (tmp[1] != '\'' && tmp[1] != '"')) {
			free(alias_name);
			continue;
		}

		*tmp = '\0';
		/* If the alias name already exists, skip it too. */
		int exists = 0;
		for (i = 0; i < aliases_n; i++) {
			if (*p == *aliases[i].name && strcmp(aliases[i].name, p) == 0) {
				exists = 1;
				break;
			}
		}

		*tmp = '=';

		if (exists == 0) {
			if (first == 1) {
				first = 0;
				fputs("\n\n", config_fp);
			}

			alias_imported++;

			/* Write the new alias into CliFM's config file */
			fputs(line, config_fp);
		} else {
			xerror(_("'%s': Alias already exists\n"), alias_name);
		}

		free(alias_name);
	}

	free(line);
	fclose(fp);
	fclose(config_fp);

	/* No alias was found in FILE */
	if (alias_found == 0) {
		xerror(_("alias: No alias found in '%s'\n"), rfile);
		return FUNC_FAILURE;
	}

	/* Aliases were found in FILE, but none was imported (either because
	 * they conflicted with internal commands or the alias already
	 * existed). */
	if (alias_imported == 0) {
		xerror(_("alias: No alias imported\n"));
		return FUNC_FAILURE;
	}

	/* If some alias was found and imported, print the corresponding
	 * message and update the aliases array. */
	printf(_("alias: %zu alias(es) imported\n"), alias_imported);

	/* Add new aliases to the internal list of aliases. */
	get_aliases();

	/* Add new aliases to the commands list for tab completion. */
	if (bin_commands) {
		for (i = 0; bin_commands[i]; i++)
			free(bin_commands[i]);
		free(bin_commands);
		bin_commands = (char **)NULL;
	}
	get_path_programs();

	return FUNC_SUCCESS;
}

char *
parse_usrvar_value(const char *str, const char c)
{
	if (c == '\0' || !str)
		return (char *)NULL;

	/* Get whatever comes after c */
	char *tmp = strchr(str, c);
	if (!tmp || !*(++tmp))
		return (char *)NULL;

	/* Remove leading quotes */
	if (*tmp == '"' || *tmp == '\'')
		tmp++;

	/* Remove trailing spaces, tabs, new line chars, and quotes */
	const size_t tmp_len = strlen(tmp);

	for (size_t i = tmp_len - 1; tmp[i] && i > 0; i--) {
		if (tmp[i] != ' ' && tmp[i] != '\t' && tmp[i] != '"' && tmp[i] != '\''
		&& tmp[i] != '\n')
			break;
		else
			tmp[i] = '\0';
	}

	if (!*tmp)
		return (char *)NULL;

	char *buf = savestring(tmp, strlen(tmp));
	return buf;
}

int
create_usr_var(const char *str)
{
	if (!str || !*str)
		return FUNC_FAILURE;

	char *p = strchr(str, '=');
	if (!p || p == str)
		return FUNC_FAILURE;

	*p = '\0';
	const size_t len = (size_t)(p - str);
	char *name = xnmalloc(len + 1, sizeof(char));
	xstrsncpy(name, str, len + 1);
	*p = '=';

	char *value = parse_usrvar_value(str, '=');

	if (!value) {
		free(name);
		xerror(_("%s: Error getting variable value\n"), PROGRAM_NAME);
		return FUNC_FAILURE;
	}

	usr_var = xnrealloc(usr_var, (size_t)(usrvar_n + 2), sizeof(struct usrvar_t));
	usr_var[usrvar_n].name = savestring(name, strlen(name));
	usr_var[usrvar_n++].value = savestring(value, strlen(value));

	usr_var[usrvar_n].name = (char *)NULL;
	usr_var[usrvar_n].value = (char *)NULL;

	free(name);
	free(value);
	return FUNC_SUCCESS;
}

/*
// Resize the autocmds array leaving only temporary autocommands.
static void
keep_temp_autocmds(void)
{
	size_t i;
	size_t c = 0;
	struct autocmds_t *a = (struct autocmds_t *)NULL;

	for (i = 0; i < autocmds_n; i++) {
		if (autocmds[i].temp == 1) {
			a = xnrealloc(a, c + 2, sizeof(struct autocmds_t));
			memmove(a + c, autocmds + i, sizeof(struct autocmds_t));
			c++;
			a[c] = (struct autocmds_t){0};
		}

		free(autocmds[i].pattern);
		free(autocmds[i].cmd);
		free(autocmds[i].filter.str);
		autocmds[i] = (struct autocmds_t){0};
	}

	autocmds_n = c;
	free(autocmds);
	autocmds = a;
//	autocmds = xnrealloc(autocmds, autocmds_n, sizeof(struct autocmds_t));
} */

void
free_autocmds(const int keep_temp)
{
	UNUSED(keep_temp);
/*	size_t i = autocmds_n;

	if (keep_temp == 1) {
		for (; i-- > 0;) {
			if (autocmds[i].temp == 1) {
				keep_temp_autocmds();
				return;
			}
		}
	} */

	for (size_t i = autocmds_n; i-- > 0;) {
		free(autocmds[i].pattern);
		free(autocmds[i].cmd);
		free(autocmds[i].filter.str);
		autocmds[i].color_scheme = (char *)NULL;
	}

	free(autocmds);
	autocmds = (struct autocmds_t *)NULL;
	autocmds_n = 0;
	autocmd_set = 0;
}

void
free_tags(void)
{
	for (size_t i = tags_n; i-- > 0;)
		free(tags[i]);
	free(tags);
	tags = (char **)NULL;
	tags_n = 0;
}

int
free_remotes(const int exit)
{
	if (exit)
		autounmount_remotes();

	for (size_t i = 0; i < remotes_n; i++) {
		free(remotes[i].name);
		free(remotes[i].desc);
		free(remotes[i].mountpoint);
		free(remotes[i].mount_cmd);
		free(remotes[i].unmount_cmd);
	}
	free(remotes);
	remotes_n = 0;

	return FUNC_SUCCESS;
}

/* Load both regular and warning prompt, if enabled, from the prompt name NAME.
 * Return FUNC_SUCCESS if found or FUNC_FAILURE if not. */
int
expand_prompt_name(char *name)
{
	if (!name || !*name || prompts_n == 0)
		return FUNC_FAILURE;

	char *p = remove_quotes(name);
	if (!p || !*p || strchr(p, '\\')) /* Exclude prompt codes. */
		return FUNC_FAILURE;

	for (size_t i = prompts_n; i-- > 0;) {
		if (*p != *prompts[i].name || strcmp(p, prompts[i].name) != 0)
			continue;

		if (prompts[i].regular) {
			free(conf.encoded_prompt);
			conf.encoded_prompt = savestring(prompts[i].regular,
				strlen(prompts[i].regular));
		}

		if (prompts[i].warning) {
			free(conf.wprompt_str);
			conf.wprompt_str = savestring(prompts[i].warning,
				strlen(prompts[i].warning));
		}

		if (prompts[i].right) {
			free(conf.rprompt_str);
			conf.rprompt_str = savestring(prompts[i].right,
				strlen(prompts[i].right));
			if (prompts[i].regular)
				conf.prompt_is_multiline =
					strstr(prompts[i].regular, "\\n") ? 1 : 0;
		}

		prompt_notif = prompts[i].notifications;

		if (xargs.warning_prompt != 0)
			conf.warning_prompt = prompts[i].warning_prompt_enabled;

		xstrsncpy(cur_prompt_name, prompts[i].name, sizeof(cur_prompt_name));

		return FUNC_SUCCESS;
	}

	return FUNC_FAILURE;
}

void
free_prompts(void)
{
	for (size_t i = prompts_n; i-- > 0;) {
		free(prompts[i].name);
		free(prompts[i].regular);
		free(prompts[i].warning);
		free(prompts[i].right);
	}
	free(prompts);
	prompts = (struct prompts_t *)NULL;
	prompts_n = 0;
}

static void
remove_virtual_dir(void)
{
	struct stat a;
	if (stdin_tmp_dir && stat(stdin_tmp_dir, &a) != -1) {
		xchmod(stdin_tmp_dir, "0700", 1);

		char *rm_cmd[] = {"rm", "-r", "--", stdin_tmp_dir, NULL};
		const int ret = launch_execv(rm_cmd, FOREGROUND, E_NOFLAG);
		if (ret != FUNC_SUCCESS)
			exit_code = ret;
		free(stdin_tmp_dir);
	}

	unsetenv("CLIFM_VIRTUAL_DIR");
}

/*
#if defined(__clang__)
// Free the storage associated with MAP
static void
xrl_discard_keymap(Keymap map)
{
	if (map == 0)
		return;

	int i;
	for (i = 0; i < KEYMAP_SIZE; i++) {
		switch (map[i].type) {
		case ISFUNC: break;

		case ISKMAP:
			// GCC (but not clang) complains about this if compiled with -pedantic
			// See discussion here: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=83584
			xrl_discard_keymap((Keymap)map[i].function);
			break;

		case ISMACR:
			// GCC (not clang) complains about this one too
			free((char *)map[i].function);
			break;
		}
	}
}
#endif // __clang__ */

void
free_workspaces_filters(void)
{
	for (size_t i = 0; i < MAX_WS; i++) {
		free(workspace_opts[i].filter.str);
		workspace_opts[i].filter.str = (char *)NULL;
		workspace_opts[i].filter.rev = 0;
		workspace_opts[i].filter.type = FILTER_NONE;
	}
}

void
save_last_path(char *last_path_tmp)
{
	if (config_ok == 0 || !config_dir || !config_dir_gral)
		return;

	char *last_path = xnmalloc(config_dir_len + 7, sizeof(char));
	snprintf(last_path, config_dir_len + 7, "%s/.last", config_dir);

	int fd = 0;
	FILE *last_fp = open_fwrite(last_path, &fd);
	if (!last_fp) {
		xerror(_("%s: Error saving last visited directory: %s\n"),
			PROGRAM_NAME, strerror(errno));
		free(last_path);
		return;
	}

	for (int i = 0; i < MAX_WS; i++) {
		if (!workspaces[i].path)
			continue;

		if (cur_ws == i)
			fprintf(last_fp, "*%d:%s\n", i, workspaces[i].path);
		else
			fprintf(last_fp, "%d:%s\n", i, workspaces[i].path);
	}

	fclose(last_fp);

	/* Last visited path is stored in the profile directory, but the
	 * cd-on-quit script cannot know what our profile was. So, let's
	 * symlink the .last file in our profile directory to the clifm
	 * gral config dir, so that the script knows where to look for. */
	if (conf.cd_on_quit == 1 && last_path_tmp
	&& symlinkat(last_path, XAT_FDCWD, last_path_tmp) == -1) {
		xerror(_("%s: cd-on-quit: Cannot create symbolic link '%s': %s\n"),
			PROGRAM_NAME, last_path_tmp, strerror(errno));
	}

	free(last_path);
}

/* Store last visited directory for the restore-last-path and the
 * cd-on-quit functions. Current workspace/path will be marked with an
 * asterisk. It will be read at startup by get_last_path() and at exit by
 * cd_on_quit.sh, if enabled. */
static void
handle_last_path(void)
{
	if (!config_dir_gral) /* NULL if running with --open or --preview */
		return;

	/* The cd_on_quit.sh function changes the shell current directory to the
	 * directory specifcied in ~/.config/clifm/.last if this file is found (it
	 * is actually a symlink to ~/.config/clifm/profiles/PROFILE/.last).
	 * Remove the file to prevent the function from changing the directory
	 * if cd-on-quit is disabled. If necessary, it will be recreated by}
	 * save_last_path() below. */
	const size_t len = strlen(config_dir_gral) + 7;
	char *last_path_tmp = xnmalloc(len, sizeof(char));
	snprintf(last_path_tmp, len, "%s/.last", config_dir_gral);

	struct stat a;
	if (lstat(last_path_tmp, &a) != -1
	&& unlinkat(XAT_FDCWD, last_path_tmp, 0) == -1)
		xerror("unlink: '%s': %s\n", last_path_tmp, strerror(errno));

	if (conf.restore_last_path == 1 || conf.cd_on_quit == 1)
		save_last_path(last_path_tmp);

	free(last_path_tmp);
}

static void
free_file_templates(void)
{
	if (!file_templates)
		return;

	/* Max file_templates is filesn_t (ssize_t), <= size_t. */
	for (size_t i = 0; file_templates[i]; i++)
		free(file_templates[i]);

	free(file_templates);
	file_templates = (char **)NULL;
}

/* This function is called by atexit() to clear whatever is there at exit
 * time and avoid thus memory leaks */
void
free_stuff(void)
{
	size_t i = 0;

	free(alt_config_dir);
	free(alt_trash_dir);
	free(alt_config_file);
	free(alt_bm_file);
	free(alt_kbinds_file);
	free(alt_mimelist_file);
	free(alt_preview_file);
	free(alt_profile);

	if (user_mimetypes) {
		for (i = 0; user_mimetypes[i].mimetype; i++) {
			free(user_mimetypes[i].ext);
			free(user_mimetypes[i].mimetype);
		}
		free(user_mimetypes);
	}

	if (sys_users) {
		for (i = 0; sys_users[i].name; i++)
			free(sys_users[i].name);
		free(sys_users);
	}
	if (sys_groups) {
		for (i = 0; sys_groups[i].name; i++)
			free(sys_groups[i].name);
		free(sys_groups);
	}

#ifdef LINUX_FSINFO
	if (ext_mnt) {
		for (i = 0; ext_mnt[i].mnt_point; i++)
			free(ext_mnt[i].mnt_point);
		free(ext_mnt);
	}
#endif /* LINUX_FSINFO */

#ifdef RUN_CMD
	free(cmd_line_cmd);
#endif /* RUN_CMD */

#if !defined(_NO_ICONS)
	free(name_icon_hashes);
	free(dir_icon_hashes);
	free(ext_icon_hashes);
#ifndef OLD_ICON_LOOKUP
	free(ext_table);
#endif
#endif /* !_NO_ICONS */

	free(conf.time_str);
	free(conf.ptime_str);
	free(conf.priority_sort_char);

#ifdef LINUX_INOTIFY
	/* Shutdown inotify */
	if (inotify_wd >= 0)
		inotify_rm_watch(inotify_fd, inotify_wd);
	if (inotify_fd != UNSET)
		close(inotify_fd);
#elif defined(BSD_KQUEUE)
	if (event_fd >= 0)
		close(event_fd);
	if (kq != UNSET)
		close(kq);
#endif /* LINUX_INOTIFY */

	free_prompts();
	free(prompts_file);
	free_autocmds(0);
	free_tags();
	free_remotes(1);
	free_file_templates();

	if (xargs.stealth_mode != 1)
		save_jumpdb();

	handle_last_path();

	free_bookmarks();
	free(conf.encoded_prompt);
	free_dirlist();
	free(conf.opener);
	free(conf.rprompt_str);
	free(conf.wprompt_str);
	free(conf.fzftab_options);
	free(conf.welcome_message_str);

	remove_virtual_dir();

	for (i = cschemes_n; i-- > 0;)
		free(color_schemes[i]);
	free(color_schemes);
	free(conf.usr_cscheme);

	if (jump_db) {
		for (i = jump_n; i-- > 0;)
			free(jump_db[i].path);
		free(jump_db);
	}

	free(pinned_dir);

//	ADD FILTER TYPE CHECK!
	if (filter.str) {
		regfree(&regex_exp);
		free(filter.str);
	}

	if (conf.histignore_regex) {
		regfree(&regex_hist);
		free(conf.histignore_regex);
	}

	if (conf.dirhistignore_regex) {
		regfree(&regex_dirhist);
		free(conf.dirhistignore_regex);
	}

	free_workspaces_filters();

	if (profile_names) {
		for (i = 0; profile_names[i]; i++)
			free(profile_names[i]);
		free(profile_names);
	}

	if (sel_n > 0) {
		for (i = sel_n; i-- > 0;)
			free(sel_elements[i].name);
		free(sel_elements);
	}
	free(sel_devino);

	if (bin_commands) {
		for (i = path_progsn; i-- > 0;)
			free(bin_commands[i]);
		free(bin_commands);
	}

	if (paths) {
		for (i = path_n; i-- > 0;)
			free(paths[i].path);
		free(paths);
	}

	if (cdpaths) {
		for (i = cdpath_n; i-- > 0;)
			free(cdpaths[i]);
		free(cdpaths);
	}

	if (history) {
		for (i = current_hist_n; i-- > 0;)
			free(history[i].cmd);
		free(history);
	}

	if (dirhist_total_index) {
		i = dirhist_total_index > 0 ? (size_t)dirhist_total_index : 0;
		for (; i-- > 0;)
			free(old_pwd[i]);
		free(old_pwd);
	}

	for (i = aliases_n; i-- > 0;) {
		free(aliases[i].name);
		free(aliases[i].cmd);
	}
	free(aliases);

	for (i = kbinds_n; i-- > 0;) {
		free(kbinds[i].function);
		free(kbinds[i].key);
	}
	free(kbinds);

	for (i = usrvar_n; i-- > 0;) {
		free(usr_var[i].name);
		free(usr_var[i].value);
	}
	free(usr_var);

	for (i = actions_n; i-- > 0;) {
		free(usr_actions[i].name);
		free(usr_actions[i].value);
	}
	free(usr_actions);

	for (i = prompt_cmds_n; i-- > 0;)
		free(prompt_cmds[i]);
	free(prompt_cmds);

	if (msgs_n > 0) {
		for (i = msgs_n; i-- > 0;)
			free(messages[i].text);
		free(messages);
	}

	if (ext_colors_n) {
		for (i = ext_colors_n; i-- > 0;) {
			free(ext_colors[i].name);
			free(ext_colors[i].value);
		}
		free(ext_colors);
	}

	if (workspaces && workspaces[0].path) {
		for (i = MAX_WS; i-- > 0;) {
			free(workspaces[i].path);
			free(workspaces[i].name);
		}
		free(workspaces);
	}

	free(actions_file);
	free(bm_file);
	free(data_dir);
	free(colors_dir);
	free(config_dir_gral);
	free(config_dir);
	free(config_file);
	free(dirhist_file);
	free(hist_file);
	free(kbinds_file);
	free(msgs_log_file);
	free(cmds_log_file);
	free(mime_file);
	free(plugins_dir);
	free(plugins_helper_file);
	free(profile_file);
	free(remotes_file);

#ifndef _NO_SUGGESTIONS
	free(suggestion_buf);
	free(conf.suggestion_strategy);
#endif /* !_NO_SUGGESTIONS */

	free(sel_file);
	free(templates_dir);
	free(thumbnails_dir);
	free(tmp_rootdir);
	free(tmp_dir);
	free(user.name);
	free(user.home);
	free(user.shell);
	free(user.shell_basename);

	free(user.groups);

#ifndef _NO_TRASH
	free(trash_dir);
	free(trash_files_dir);
	free(trash_info_dir);
#endif /* !_NO_TRASH */
	free(tags_dir);
	free(conf.term);
	free(quote_chars);

	rl_clear_history();
	rl_free_undo_list();
	rl_clear_pending_input();

/*
#if defined(__clang__)
	Keymap km = rl_get_keymap();
	xrl_discard_keymap(km);
#endif // __clang__ */

#ifdef CLIFM_TEST_INPUT
	if (rl_instream)
		fclose(rl_instream);
#endif /* CLIFM_TEST_INPUT */

	/* Restore the color of the running terminal */
	if (conf.colorize == 1 && xargs.list_and_quit != 1)
		RESTORE_COLOR;

	if (xargs.kitty_keys == 1)
		UNSET_KITTY_KEYS;
}

/* Dynamically set MaxFilenameLen based on the current number of terminal
 * columns. */
static void
set_max_filename_len_auto(void)
{
	if (conf.max_name_len_auto == UNSET || conf.max_name_len == UNSET)
		return;

	if (conf.columned == 0 && conf.long_view == 0) {
		/* Displaying in a single column: Do not truncate names. */
		conf.max_name_len = term_cols;
		return;
	}

	const int n = (int)(term_cols * conf.max_name_len_auto / 100);
	conf.max_name_len = n < conf.min_name_trunc ? conf.min_name_trunc : n;
}

/* Get current terminal dimensions and store them in TERM_COLS and
 * TERM_LINES (globals). These values will be updated upon SIGWINCH.
 * In case of error, we fallback to 80x24. */
void
get_term_size(void)
{
	struct winsize w;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1) {
		term_cols = DEFAULT_WIN_COLS;
		term_lines = DEFAULT_WIN_ROWS;
	} else {
		term_cols =  w.ws_col > 0 ? w.ws_col : DEFAULT_WIN_COLS;
		term_lines = w.ws_row > 0 ? w.ws_row : DEFAULT_WIN_ROWS;
	}

	if (xargs.secure_env == 1 || xargs.secure_env_full == 1) {
		set_max_filename_len_auto();
		return;
	}

	char *env = getenv("CLIFM_COLUMNS");
	int value = env ? atoi(env) : -1;
	if (value > 0 && value <= USHRT_MAX)
		term_cols = (unsigned short)value;

	env = getenv("CLIFM_LINES");
	value = env ? atoi(env) : -1;
	if (value > 0 && value <= USHRT_MAX)
		term_lines = (unsigned short)value;

	set_max_filename_len_auto();
}

static int
create_virtual_dir(const int user_provided)
{
	if (!stdin_tmp_dir || !*stdin_tmp_dir) {
		if (user_provided == 1) {
			err('e', PRINT_PROMPT, _("%s: Empty buffer for virtual "
				"directory name. Trying with default value\n"), PROGRAM_NAME);
		} else {
			err('e', PRINT_PROMPT, _("%s: Empty buffer for virtual "
				"directory name\n"), PROGRAM_NAME);
		}
		return FUNC_FAILURE;
	}

	char *cmd[] = {"mkdir", "-p", "--", stdin_tmp_dir, NULL};
	int ret = 0;
	if ((ret = launch_execv(cmd, FOREGROUND, E_MUTE)) != FUNC_SUCCESS) {
		char *errmsg = (ret == E_NOTFOUND ? NOTFOUND_MSG
			: (ret == E_NOEXEC ? NOEXEC_MSG : (char *)NULL));

		if (user_provided == 1) {
			err('e', PRINT_PROMPT, _("%s: mkdir: '%s': %s. Trying with "
				"default value\n"), PROGRAM_NAME, stdin_tmp_dir,
				errmsg ? errmsg : strerror(ret));
		} else {
			err('e', PRINT_PROMPT, "%s: mkdir: '%s': %s\n",
				PROGRAM_NAME, stdin_tmp_dir,
				errmsg ? errmsg : strerror(ret));
		}
		return ret;
	}

	return FUNC_SUCCESS;
}

static char *
construct_name(char *file, const size_t flen)
{
	char *name = (char *)NULL;

	/* Should we construct destiny file as full path or using only the
	 * last path component (the file's basename)? */
	if (xargs.virtual_dir_full_paths != 1) {
		const char *p = strrchr(file, '/');
		if (!p || !*(++p))
			name = savestring(file, flen);
		else
			name = savestring(p, strlen(p));
	} else {
		name = replace_slashes(file, ':');
	}

	if (!name || !*name) {
		free(name);
		err('w', PRINT_PROMPT, _("%s: '%s': Error constructing "
			"filename\n"), PROGRAM_NAME, file);
		return (char *)NULL;
	}

	/* Prohibited names */
	if (*name == '/' && !name[1]) {
		name = xnrealloc(name, 5, sizeof(char));
		xstrsncpy(name, "root", 5);
	}

	else if (*name == '.' && !name[1]) {
		name = xnrealloc(name, 5, sizeof(char));
		xstrsncpy(name, "self", 5);
	}

	else {
		if (*name == '.' && name[1] == '.' && !name[2]) {
			name = xnrealloc(name, 7, sizeof(char));
			xstrsncpy(name, "parent", 7);
		}
	}

	return name;
}

static size_t
gen_symlink(char *file, const char *cwd)
{
	if (SELFORPARENT(file))
		return 0;

	struct stat attr;
	if (lstat(file, &attr) == -1) {
		/* "~" fails here. No need to check in construct_name(). */
		err('w', PRINT_PROMPT, "%s: '%s': %s\n",
			PROGRAM_NAME, file, strerror(errno));
		return 0;
	}

	/* Construct source and destiny files. */

	/* symlink(3) doesn't like filenames ending with slash. */
	size_t file_len = strlen(file);
	if (file_len > 1 && file[file_len - 1] == '/') {
		file[file_len - 1] = '\0';
		file_len--;
	}

	char source[PATH_MAX + 1];
	if (*file != '/')
		snprintf(source, sizeof(source), "%s/%s", cwd, file);
	else
		xstrsncpy(source, file, sizeof(source));

	char *name = construct_name(file, file_len);
	if (!name)
		return 0;

	char dest[PATH_MAX + 32];
	snprintf(dest, sizeof(dest), "%s/%s", stdin_tmp_dir, name);

	size_t suffix = 0;

	while (1) {
		errno = 0;
		if (symlink(source, dest) == 0)
			break;

		if (errno != EEXIST) {
			err('w', PRINT_PROMPT, _("%s: Cannot create symbolic "
				"link '%s': %s\n"), PROGRAM_NAME, dest, strerror(errno));
			free(name);
			return 0;
		}

		suffix++;
		if (suffix > MAX_FILE_CREATION_TRIES) {
			err('w', PRINT_PROMPT, _("%s: Cannot create symbolic "
				"link to '%s': max attempts (%d) reached\n"),
				PROGRAM_NAME, stdin_tmp_dir, name, MAX_FILE_CREATION_TRIES);
			free(name);
			return 0;
		}

		snprintf(dest, sizeof(dest), "%s/%s-%zu", stdin_tmp_dir,
			name, suffix);
	}

	free(name);
	return 1;
}

int
handle_stdin(void)
{
	/* If files are passed via stdin, we need to disable restore-
	 * last-path in order to correctly understand relative paths. */
	conf.restore_last_path = 0;
	/* In light mode, stat(2) is not executed, so that we cannot dereference
	 * symlinks created in virtual directories. */
	if (conf.light_mode == 1)
		err('n', PRINT_PROMPT, _("%s: Light mode is not supported "
			"in virtual directories\n"), PROGRAM_NAME);
	conf.light_mode = 0;

	int exit_status = FUNC_SUCCESS;

	/* Max input size: 512 * (512 * 1024)
	 * 512 chunks of 524288 bytes (512KiB) each
	 * == (65535 * PATH_MAX)
	 * == 262MiB of data ((65535 * PATH_MAX) / 1024). */

	const size_t chunk = (size_t)512 * 1024;
	const size_t max_chunks = 512;
	size_t chunks_n = 1;
	size_t total_len = 0;
	ssize_t input_len = 0;

	/* Initial buffer allocation == 1 chunk */
	char *buf = xnmalloc(chunk, sizeof(char));

	while (chunks_n < max_chunks) {
		input_len = read(STDIN_FILENO, buf + total_len, chunk); /* flawfinder: ignore */

		/* Error */
		if (input_len < 0) {
			free(buf);
			return FUNC_FAILURE;
		}

		/* Nothing else to be read */
		if (input_len == 0)
			break;

		total_len += (size_t)input_len;
		chunks_n++;

		/* Append a new chunk of memory to the buffer */
		buf = xnrealloc(buf, chunks_n + 1, chunk);
	}

	if (total_len == 0) {
		free(buf);
		fprintf(stderr, _("%s: No entries\n"), PROGRAM_NAME);
		exit(EXIT_FAILURE);
	}

	/* Null terminate the input buffer */
	buf[total_len] = '\0';

	/* Create tmp dir to store links to files */
	char *suffix = (char *)NULL;

	if (!stdin_tmp_dir || (exit_status = create_virtual_dir(1)) != FUNC_SUCCESS) {
		free(stdin_tmp_dir);

		suffix = gen_rand_str(RAND_SUFFIX_LEN);
		char *temp = tmp_dir ? tmp_dir : P_tmpdir;
		const size_t tmp_len = strlen(temp) + RAND_SUFFIX_LEN + 7;
		stdin_tmp_dir = xnmalloc(tmp_len, sizeof(char));
		snprintf(stdin_tmp_dir, tmp_len, "%s/vdir.%s", temp,
			suffix ? suffix : "nTmp0B9&54");
		free(suffix);

		if ((exit_status = create_virtual_dir(0)) != FUNC_SUCCESS)
			goto FREE_N_EXIT;
	}

	if (xargs.stealth_mode != 1)
		setenv("CLIFM_VIRTUAL_DIR", stdin_tmp_dir, 1);

	/* Get CWD: we need it to prepend it to relative paths. */
	char t[PATH_MAX + 1] = "";
	char *cwd = get_cwd(t, sizeof(t), 0);

	if (!cwd || !*cwd) {
		exit_status = errno;
		goto FREE_N_EXIT;
	}

	/* Get substrings from buf */
	char *p = buf, *q = buf;
	size_t links_counter = 0;

	while (*p) {
		if (!*p || *p == '\n') {
			*p = '\0';

			/* Create symlinks (in tmp dir) to each valid file in the buffer */
			links_counter += gen_symlink(q, cwd);
			q = p + 1;
		}

		p++;
	}

	if (links_counter == 0) { /* No symlink was created. Exit */
		dup2(STDOUT_FILENO, STDIN_FILENO);
		xerror(_("%s: Empty filenames buffer. Nothing to do\n"), PROGRAM_NAME);
		if (getenv("CLIFM_VT_RUNNING"))
			press_any_key_to_continue(0);

		free(buf);
		exit(FUNC_FAILURE);
	}

	/* Make the virtual dir read only */
	xchmod(stdin_tmp_dir, "0500", 1);

	/* chdir to tmp dir and update path var */
	if (xchdir(stdin_tmp_dir, SET_TITLE) == -1) {
		exit_status = errno;
		xerror("cd: '%s': %s\n", stdin_tmp_dir, strerror(errno));

		xchmod(stdin_tmp_dir, "0700", 1);

		char *rm_cmd[] = {"rm", "-r", "--", stdin_tmp_dir, NULL};
		const int ret = launch_execv(rm_cmd, FOREGROUND, E_NOFLAG);
		if (ret != FUNC_SUCCESS)
			exit_status = ret;

		free(cwd);
		goto FREE_N_EXIT;
	}

	free(workspaces[cur_ws].path);
	workspaces[cur_ws].path = savestring(stdin_tmp_dir, strlen(stdin_tmp_dir));

FREE_N_EXIT:
	free(buf);

	/* Go back to tty */
	dup2(STDOUT_FILENO, STDIN_FILENO);

	if (conf.autols == 1) {
		reload_dirlist();
		add_to_dirhist(workspaces[cur_ws].path);
	}

	return exit_status;
}

/* Save pinned directory into a file. */
static int
save_pinned_dir(void)
{
	if (!pinned_dir || config_ok == 0)
		return FUNC_FAILURE;

	char *pin_file = xnmalloc(config_dir_len + 7, sizeof(char));
	snprintf(pin_file, config_dir_len + 7, "%s/.pin", config_dir);

	int fd = 0;
	FILE *fp = open_fwrite(pin_file, &fd);
	if (!fp) {
		xerror(_("pin: Error saving pinned directory: %s\n"), strerror(errno));
	} else {
		fprintf(fp, "%s", pinned_dir);
		fclose(fp);
	}

	free(pin_file);

	return FUNC_SUCCESS;
}

int
pin_directory(char *dir)
{
	if (!dir || !*dir)
		return FUNC_FAILURE;

	char *d = unescape_str(dir, 0);
	if (!d)
		return FUNC_FAILURE;

	struct stat a;
	if (lstat(d, &a) == -1) {
		xerror("pin: '%s': %s\n", d, strerror(errno));
		free(d);
		return FUNC_FAILURE;
	}

	if (pinned_dir) {
		free(pinned_dir);
		pinned_dir = (char *)NULL;
	}

	const size_t dir_len = strlen(d);

	/* If absolute path */
	if (*dir == '/') {
		pinned_dir = savestring(d, dir_len);
	} else { /* If relative path */
		if (strcmp(workspaces[cur_ws].path, "/") == 0) {
			pinned_dir = xnmalloc(dir_len + 2, sizeof(char));
			snprintf(pinned_dir, dir_len + 2, "/%s", d);
		} else {
			const size_t plen = dir_len + strlen(workspaces[cur_ws].path) + 2;
			pinned_dir = xnmalloc(plen, sizeof(char));
			snprintf(pinned_dir, plen, "%s/%s", workspaces[cur_ws].path, d);
		}
	}

	if (xargs.stealth_mode == 1 || save_pinned_dir() == FUNC_SUCCESS) {
		printf(_("pin: Succesfully pinned '%s'\n"), d);
		free(d);
		return FUNC_SUCCESS;
	} else {
		free(d);
		free(pinned_dir);
		pinned_dir = (char *)NULL;
		return FUNC_FAILURE;
	}
}

int
unpin_dir(void)
{
	if (!pinned_dir) {
		puts(_("unpin: No pinned file"));
		return FUNC_SUCCESS;
	}

	if (config_dir && xargs.stealth_mode != 1) {
		int cmd_error = 0;
		char *pin_file = xnmalloc(config_dir_len + 7, sizeof(char));
		snprintf(pin_file, config_dir_len + 7, "%s/.pin", config_dir);
		if (unlinkat(XAT_FDCWD, pin_file, 0) == -1) {
			xerror("pin: '%s': %s\n", pin_file, strerror(errno));
			cmd_error = 1;
		}

		free(pin_file);
		if (cmd_error == 1)
			return FUNC_FAILURE;
	}

	printf(_("unpin: Succesfully unpinned '%s'\n"), pinned_dir);

	free(pinned_dir);
	pinned_dir = (char *)NULL;
	return FUNC_SUCCESS;
}

void
version_function(const int full)
{
	char *posix = "";
	char *legacy = "";
	char *suckless = "";
	char *paranoid = "";

#ifdef _BE_POSIX
	posix = "-POSIX";
#endif /* _BE_POSIX */

#ifdef CLIFM_LEGACY
	legacy = "-LEGACY";
#endif /* CLIFM_LEGACY */

#ifdef CLIFM_SUCKLESS
	suckless = "-SUCKLESS";
#endif /* CLIFM_SUCKLESS */

#ifdef SECURITY_PARANOID
# if SECURITY_PARANOID > 0
	paranoid = "-PARANOID";
# endif
#endif /* SECURITY_PARANOID */

	if (full == 1) { /* Running from within clifm (as 'ver')*/
		printf(_("%s %s%s%s%s%s (%s)\n%s\nLicense %s\nWritten by %s\n"),
			PROGRAM_NAME, VERSION, posix, legacy, suckless, paranoid,
			DATE, CONTACT, LICENSE, AUTHOR);
	} else { /* Running as --version (or -v) */
		printf("%s%s%s%s%s\n", VERSION, posix, legacy, suckless, paranoid);
		exit(EXIT_SUCCESS);
	}
}

void
splash(void)
{
	const char *reg_cyan = conf.colorize == 1 ? "\x1b[0;36m" : "";
	printf("\n%s%s\n\n%s%s\t\t       %s%s\n           %s\n",
		reg_cyan, ASCII_LOGO_BIG, df_c, BOLD, df_c,
		PROGRAM_NAME_UPPERCASE, PROGRAM_DESC);

	HIDE_CURSOR;
	if (conf.splash_screen) {
		printf("\n            ");
		fflush(stdout);
		press_any_key_to_continue(0);
	} else {
		putchar('\n');
	}

	UNHIDE_CURSOR;
}

void
bonus_function(void)
{
	char *phrases[] = {
		"\"Vamos Boca Juniors Carajo!\" (La mitad + 1)",
		"\"Hey! Look behind you! A three-headed monkey! (G. Threepweed)",
		"\"Free as in free speech, not as in free beer\" (R. M. S)",
		"\"Nothing great has been made in the world without passion\" (G. W. F. Hegel)",
		"\"Simplicity is the ultimate sophistication\" (Leo Da Vinci)",
		"\"Yo vendí semillas de alambre de púa, al contado, y me lo agradecieron\" (Marquitos, 9 Reinas)",
		"\"I'm so happy, because today I've found my friends, they're in my head\" (K. D. Cobain)",
		"\"The best code is written with the delete key\" (Someone, somewhere, sometime)",
		"\"I'm selling these fine leather jackets\" (Indy)",
		"\"If you've been feeling increasingly stupid lately, you're not alone\" (Zak McKracken)",
		"\"I pray to God to make me free of God\" (Meister Eckhart)",
		"¡Truco y quiero retruco mierda!",
		"\"The are no facts, only interpretations\" (F. Nietzsche)",
		"\"This is a lie\" (The liar paradox)",
		"\"There are two ways to write error-free programs; only the third one works\" (Alan J. Perlis)",
		"The man who sold the world was later sold by the big G",
		"A programmer is always one year older than themself",
		"A smartphone is anything but smart",
		"And he did it: he killed the man who killed him",
		">++('>",
		":(){:|:&};:",
		"Keep it simple, stupid",
		"If ain't broken, brake it",
		"\"I only know that I know nothing\" (Socrates)",
		"(Learned) Ignorance is the true outcome of wisdom (Nicholas " /* NOLINT */
		"of Cusa)",
		"True intelligence is about questions, not answers",
		"Humanity is just an arrow released towards God",
		"Buzz is right: infinity is our only and ultimate aim",
		"That stain will never ever be erased (La 12)",
		"\"Una obra de arte no se termina, se abandona\" (L. J. Guerrero)",
		"At the beginning, software was hardware; but today hardware is "
		"being absorbed by software",
		"\"Juremos con gloria morir\"",
		"\"Given enough eyeballs, all bugs are shallow.\" (E. Raymond)",
		"\"We're gonna need a bigger boat.\" (Caleb)",
		"\"Ein Verletzter, Alarm, Alarm!\"",
		"\"There is not knowledge that is not power\"",
		"idkfa",
		"This is the second best file manager I've ever seen!",
		"Winners don't use TUIs",
		"\"La inmortalidad es baladí\" (J. L. Borges)",
		"\"Computer updated [...] Establish communications, priority alpha\"",
		"\"Step one: find plans, step two: save world, step three: get out of my house!\" (Dr. Fred)",
		"\"Leave my loneliness unbroken!, quit the bust above my door! Quoth the raven: Nevermore.\" (E. A. Poe)",
		NULL};

	const size_t num = (sizeof(phrases) / sizeof(phrases[0])) - 1;
#ifndef HAVE_ARC4RANDOM
	srandom((unsigned int)time(NULL));
	puts(phrases[random() % (int)num]);
#else
	puts(phrases[arc4random_uniform((uint32_t)num)]);
#endif /* !HAVE_ARC4RANDOM */
}
