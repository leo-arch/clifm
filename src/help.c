/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* help.c -- home of clifm's help system */

#include "helpers.h"

#include <errno.h>  /* errno */
#include <string.h>
#include <unistd.h> /* unlink */

#include "aux.h"      /* is_cmd_in_path */
#include "listing.h"  /* reload_dirlist */
#include "messages.h" /* USAGE macros */
#include "misc.h"     /* xerror */
#include "spawn.h"    /* launch_exec */
#include "strings.h"  /* savestring */

/* Instead of recreating here the commands description, just jump to the
 * corresponding section in the manpage. */
int
list_commands(void)
{
	char cmd[PATH_MAX];
	snprintf(cmd, sizeof(cmd), "export PAGER=\"less -p '^[0-9]+\\.[[:space:]]COMMANDS'\"; man %s\n",
		PROGRAM_NAME);
	if (launch_execl(cmd) != FUNC_SUCCESS)
		return FUNC_FAILURE;

	return FUNC_SUCCESS;
}

/* Print either all tips (if ALL == 1) or just a random one (ALL == 0) */
void
print_tips(const int all)
{
	const char *TIPS[] = {
#ifndef _BE_POSIX
		"Customize file-opening applications with 'mm edit' or F6",
		"Get MIME information for a file with 'mm info'",
		"Manage default associated applications with the 'mime' command",
		"Customize preview applications with 'view edit' or F7",
		"List mountpoints: 'mp' or Alt+m",
		"Compress files with 'ac' and decompress with 'ad': e.g., 'ac sel' or 'ad file.zip'",
		"Disallow the use of shell commands with the -x option: 'clifm -x'",
		"Don't you like ELNs? Disable them with the -e command-line switch",
		"Disable file previews (fzf mode) with --no-fzfpreview",
# ifdef __linux__
		"Manage removable devices with the 'media' command",
# endif /* __linux__ */
#endif /* _BE_POSIX */
#ifndef _BE_POSIX
		"Create a new profile: 'pf add PROFILE' or 'clifm -P PROFILE'",
		"Enable incognito (stealth) mode: -S/--stealth-mode",
		"Run in read-only mode with --readonly",
		"Use --secure-env and --secure-cmds for secure operation in untrusted environments",
		"Run in disk-usage-analyzer mode using the -t command-line switch",
		"Use fuzzy suggestions: e.g., 'dwn > Downloads'. Enable with --fuzzy-matching or FuzzyMatching in the configuration file",
		"Miss LS_COLORS? Run with --lscolors",
#else
		"Create a new profile: 'pf add PROFILE' or 'clifm -p PROFILE'",
		"Enable incognito (stealth) mode with the -s command-line switch",
		"Running in an untrusted environment? Try the -x, -X, and -Y flags",
		"Run in disk-usage-analyzer mode using the -u flag",
		"Use fuzzy suggestions: e.g., 'dwn > Downloads'. Enable with the -m flag (or FuzzyMatching in the configuration file)",
#endif /* _BE_POSIX */
		"Clear the screen: 'rf', '.', Enter (on empty line), or Ctrl+l",
		"Try the autocd and auto-open functions: run 'FILE' instead "
		"of 'cd FILE' or 'open FILE'",
		"Do not forget to take a look at the manpage",
		"Need more speed? Try the light mode ('lm' or Alt+y)",
		"The Selection Box is shared among different instances of Clifm",
		"Select files with the 's': 's FILE...'",
		"Use wildcards and regular expressions to select files: e.g., "
		"'s *.c' or 's .*\\.c$'",
		"Operate on selected files: e.g., 'p sel' or 'p s:'",
		"List selected files: 'sb' or s:<TAB>",
		"Use ELNs and 'sel' with shell commands, like 'ls -ld 1 sel'",
		"Press TAB to automatically expand an ELN. E.g., 's 2<TAB>' -> 's FILENAME'",
		"Use ranges (ELN-ELN) to easily move multiple files. E.g., 'm 3-12 dir/'",
		"Trash files with a simple 't FILE'",
		"Too many listed files? Run the pager: 'pg' or Alt+0",
		"Toggle the long view: 'll' or Alt+l",
		"Search for files with the slash command: e.g., '/*.png'",
		"The search function supports regular expressions: e.g., '/^c'",
		"Add a new bookmark: 'bm add FILENAME BM_NAME'",
		"Use c, l, m, md, and r instead of cp, ln, mv, mkdir, and rm",
		"Access a remote filesystem with the 'net' command",
		"Navigate the directory history with Alt+j and Alt+k "
		"(also Shift+Left and Shift+Right)",
		"Run a new instance of Clifm: 'x DIR'",
		"Send a command to the system shell: e.g., ';ls -l *'",
		"Run the last executed command: '!!'",
		"Access the command history: '!<TAB>'",
		"Exclude commands from history using the HistIgnore option in the configuration file (F10)",
		"Access the directory history list: 'dh <TAB>'",
		"List previous search patterns: '/*<TAB>'",
		"Import aliases from file: 'alias import FILE'",
		"List available aliases: 'alias'",
		"Create aliases to easily run your preferred commands (F10)",
		"Get a brief description of Clifm commands: 'cmd<TAB>'",
		"Preview the current color scheme: 'cs preview'",
		"Toggle show-hidden-files: 'hh' or Alt+.",
		"Toggle follow-links (long view only): 'k' or Alt++",
		"Change to the root directory: Alt+r",
		"Change to the home directory: Alt+e (or 'cd')",
		"Edit the current color scheme file: F8 (or 'cs edit')",
		"Edit the keybindings file: F9 (or 'kb edit')",
		"Edit the main configuration file: F10 (or 'config')",
		"Edit the bookmarks file: F11 (or 'bm edit')",
		"Edit the MIME list file: F6 (or 'mm edit')",
		"Set the starting path: e.g., 'clifm ~/media'",
		"Open files and directories with the 'o' command: e.g., 'o 12'",
		"Open a file or directory by just entering its ELN or name (auto-open/autocd)",
		"Bypass the file opener by specifying an application: e.g., '12 leafpad'",
		"Open a file in the background: e.g., '24&'",
		"Create a custom prompt by editing the prompts file ('prompt edit')",
		"Customize your color scheme: 'cs edit' or F8",
		"Launch the bookmark manager: 'bm' or Alt+b",
		"Quickly list bookmarks: 'b:<TAB>'",
		"Change to a bookmark: 'bm NAME' or 'b:NAME'",
		"Chain commands with ';' and '&&': e.g., 's 2 7-10; r sel'",
		"Switch profiles: 'pf set PROFILE'",
		"Delete a profile: 'pf del PROFILE'",
		"Rename a profile: 'pf rename PROFILE'",
		"Print file properties with 'p FILE'",
		"Deselect all files: 'ds *' or Alt+d",
		"Select all files in the current directory: 's *' or Alt+a",
		"Jump to the Selection Box: 'sb' or Alt+s",
		"Selectively restore trashed files with the 'u' command",
		"Empty the trash can: 't empty'",
		"Toggle list-directories-first: 'ff' or Alt+g",
		"Toggle the file counter: 'fc'",
		"Take a look at the splash screen with the 'splash' command",
		"Try the 'bonus' command for some fun",
		"Launch the default system shell in the current directory: ':' or ';'",
		"Cycle through file sort orders: Alt+z and Alt+x",
		"Reverse the file sort order: 'st rev'",
		"Rename multiple files at once: e.g., 'br *.txt'",
		"Don't need any more tips? Disable them in the configuration file",
		"Need root privileges? Launch a new instance of Clifm as root "
		"with the 'X' command (note the uppercase)",
		"Create a fresh configuration file: 'config reset'",
		"Edit symbolic links with 'ln edit' (or 'le')",
		"Customize keyboard shortcuts with the 'kb bind' command",
		"Display previous and next visited directories with the "
		"DirhistMap option in the configuration file (F10)",
		"Pin a file with the 'pin' command and use it with the "
		"period keyword (,): e.g., 'pin DIR' and then 'cd ,'",
		"Switch color schemes with the 'cs' command",
		"Try the 'j' command to quickly jump to a visited directory",
		"Switch workspaces by pressing Alt+[1–8]",
		"Use the 'ws' command to list available workspaces",
		"List available plugins with the 'actions' command",
		"No space is required: e.g., 'p12' instead of 'p 12'",
		"Negate a search pattern with the exclamation mark : e.g., 's !*.pdf'",
		"Enable the TrashAsRm option to send removed files to the trash can",
		"Create files and directories with the 'n' command: e.g., 'n file dir/'",
		"Add prompt commands using the 'promptcmd' keyword: 'config' (F10)",
		"Need git integration? Consult the manpage",
		"Accept a suggestion with the Right arrow key",
		"Accept the first suggested word with Alt+f or Alt+Right",
		"Use 'c sel' to copy selected files to the current directory",
		"Delete the last entered word with Alt+q",
		"Check ELN ranges with Tab: e.g., '1-12<TAB>'",
		"Operate on specific selected files: e.g., 'p sel<TAB>' or 'p s:<TAB>'",
		"Use the 'ow' command to open a file with a specific application",
		"Limit the number of listed files with the 'mf' command",
		"Limit filename length for listed files with the MaxFilenameLen "
		"option in the configuration file (F10)",
		"Use the 'm' command to interactively rename a file: e.g., 'm 12'",
		"Set options per directory with autocommands. Try 'help autocommands'",
		"Sanitize non-ASCII filenames using the 'bleach' command",
		"Get help for internal commands using -h/--help: 'p -h'",
		"Enable icons with 'icons on' (or --icons in the command line)",
		"Quickly change to a parent directory with the 'bd' command",
		"Use 'stats' to print statistics on files in the current directory",
		"Customize the warning prompt by setting WarningPrompt in the prompts file ('prompt edit')",
		"Create multiple symbolic links at once using the 'bl' command",
		"Organize your files using tags. Try 'tag --help'",
		"Remove files in bulk using a text editor with 'rr'",
		"Send files to a remote location with the 'cr' command",
		"Switch prompts with 'prompt NAME' (or 'prompt set <TAB>')",
		"Press Alt+Tab to toggle the disk-usage-analyzer mode",
		"Press Ctrl+Alt+l to toggle max-filename-length",
		"Wildcards can be expanded with the Tab key: e.g., 's *.c<TAB>'",
		"Try help topics: 'help <TAB>'",
		"List Clifm commands (and a brief description): 'cmd<TAB>'",
		"List symlinks in the current directory: '=l<TAB>'. Try 'help file-filters' for more information",
		"Use PropFields in the configuration file to customize long-view fields",
		"Preview files in the current directory with the 'view' command (requires fzf)",
		"Press Alt+- to launch the file previewer (requires fzf)",
		"Interactively select files (requires fzf, fnf, or smenu): e.g., 's /dir/*<TAB>'",
		"Change file permissions/ownership with the 'pc' and 'oc' commands respectively",
		"Set a custom shell to run external commands: e.g., 'CLIFM_SHELL=/bin/dash clifm'",
		"Print all tips: 'tips'",
		"Create files from a template. Run 'n --help' for details.",
		"Press 'z<TAB>' to get the list of built-in command aliases",
		NULL};

	const size_t tipsn = (sizeof(TIPS) / sizeof(TIPS[0])) - 1;

	if (all == 1) {
		const int l = DIGINUM(tipsn);
		for (size_t i = 0; i < tipsn; i++) {
			printf("%s%sTIP %*zu%s: %s\n",
				conf.colorize == 1 ? df_c : "", conf.colorize == 1 ? BOLD : "",
				l, i, conf.colorize == 1 ? df_c : "", TIPS[i]);
		}
		return;
	}

#ifndef HAVE_ARC4RANDOM
	srandom((unsigned int)time(NULL));
	const long tip_num = random() % (int)tipsn;
#else
	const uint32_t tip_num = arc4random_uniform((uint32_t)tipsn);
#endif /* !HAVE_ARC4RANDOM */

	printf("%s%sTIP%s: %s\n", conf.colorize == 1 ? df_c : "",
		conf.colorize == 1 ? BOLD : "", conf.colorize == 1
		? df_c : "", TIPS[tip_num]);
}

/* Retrieve pager path, first from PAGER, then try less(1), and finally
 * more(1). If none is found return NULL. */
static char *
get_pager(void)
{
	char *p = getenv("PAGER");
	if (p) {
		char *s = strchr(p, ' ');
		if (s)
			*s = '\0';
		return strdup(p);
	}

	(void)is_cmd_in_path("less", &p);
	if (p)
		return p;

	(void)is_cmd_in_path("more", &p);
	if (p)
		return p;

	return NULL;
}

/* Help topics */
static void
print_more_info(void)
{
	puts(_("For more information consult the manpage and/or the Wiki:\n"
		"https://github.com/leo-arch/clifm/wiki"));
}

static int
print_archives_topic(void)
{
	puts(ARCHIVE_USAGE); return FUNC_SUCCESS;
}

static int
print_autocmds_topic(void)
{
	puts(AUTOCMDS_USAGE);
	putchar('\n');
	print_more_info();
	return FUNC_SUCCESS;
}

static int
print_basics_topic(void)
{
	printf(_("Run '?' or 'help' to get started with %s\n"), PROGRAM_NAME);
	return FUNC_SUCCESS;
}

static int
print_bookmarks_topic(void)
{
	puts(BOOKMARKS_USAGE); return FUNC_SUCCESS;
}

static int
print_commands_topic(void)
{
	printf("%s%s", CLIFM_COMMANDS_HEADER, CLIFM_COMMANDS); return FUNC_SUCCESS;
}

static int
print_desktop_notifications_topic(void)
{
	puts(DESKTOP_NOTIFICATIONS_USAGE); return FUNC_SUCCESS;
}

static int
print_dir_jumper_topic(void)
{
	puts(JUMP_USAGE); return FUNC_SUCCESS;
}

static int
print_file_tags_topic(void)
{
	puts(TAG_USAGE); return FUNC_SUCCESS;
}

static int
print_file_attributes_topic(void)
{
	puts(FILE_DETAILS);
	putchar('\n');
	puts(FILE_SIZE_USAGE);
	putchar('\n');
	puts(FILTER_USAGE);
	return FUNC_SUCCESS;
}

static int
print_file_filters_topic(void)
{
	puts(FILTER_USAGE); return FUNC_SUCCESS;
}

static int
print_file_previews_topic(const int image_prevs)
{
	puts(image_prevs == 1 ? IMAGE_PREVIEWS : FILE_PREVIEWS);
	putchar('\n');
	print_more_info();
	return FUNC_SUCCESS;
}

static int
print_navigation_topic(void)
{
	puts(_("Run '?' and consult the NAVIGATION section"));
	return FUNC_SUCCESS;
}

static int
print_plugins_topic(void)
{
	puts(ACTIONS_USAGE);
	putchar('\n');
	print_more_info();
	return FUNC_SUCCESS;
}

static int
print_profiles_topic(void)
{
	puts(PROFILES_USAGE); return FUNC_SUCCESS;
}

static int
print_remotes_topic(void)
{
	puts(NET_USAGE); return FUNC_SUCCESS;
}

static int
print_resource_opener_topic(void)
{
	puts(MIME_USAGE); return FUNC_SUCCESS;
}

static int
print_security_topic(void)
{
	puts(SECURITY_USAGE);
	putchar('\n');
	print_more_info();
	return FUNC_SUCCESS;
}

static int
print_selection_topic(void)
{
	puts(SEL_USAGE); return FUNC_SUCCESS;
}

static int
print_search_topic(void)
{
	puts(SEARCH_USAGE); return FUNC_SUCCESS;
}

static int
print_theming_topic(void)
{
	puts(_("Take a look at the 'colorschemes', 'prompt', and 'config' commands"));
	print_more_info();
	return FUNC_SUCCESS;
}

static int
print_trash_topic(void)
{
	puts(TRASH_USAGE); return FUNC_SUCCESS;
}

static int
run_help_topic(const char *topic)
{
	if (IS_HELP(topic)) {
		puts(HELP_USAGE);
		return FUNC_SUCCESS;
	}

	if (*topic == 'a' && strcmp(topic, "archives") == 0)
		return print_archives_topic();
	if (*topic == 'a' && strcmp(topic, "autocommands") == 0)
		return print_autocmds_topic();
	if (*topic == 'b' && strcmp(topic, "basics") == 0)
		return print_basics_topic();
	if (*topic == 'b' && strcmp(topic, "bookmarks") == 0)
		return print_bookmarks_topic();
	if (*topic == 'c' && strcmp(topic, "commands") == 0)
		return print_commands_topic();
	if (*topic == 'd' && strcmp(topic, "desktop-notifications") == 0)
		return print_desktop_notifications_topic();
	if (*topic == 'd' && strcmp(topic, "dir-jumper") == 0)
		return print_dir_jumper_topic();
	if (*topic == 'f' && strcmp(topic, "file-details") == 0)
		return print_file_attributes_topic();
	if (*topic == 'f' && strcmp(topic, "file-filters") == 0)
		return print_file_filters_topic();
	if (*topic == 'f' && strcmp(topic, "file-previews") == 0)
		return print_file_previews_topic(0);
	if (*topic == 'i' && strcmp(topic, "image-previews") == 0)
		return print_file_previews_topic(1);
	if (*topic == 'f' && strcmp(topic, "file-tags") == 0)
		return print_file_tags_topic();
	if (*topic == 'n' && strcmp(topic, "navigation") == 0)
		return print_navigation_topic();
	if (*topic == 'p' && strcmp(topic, "plugins") == 0)
		return print_plugins_topic();
	if (*topic == 'p' && strcmp(topic, "profiles") == 0)
		return print_profiles_topic();
	if (*topic == 'r' && strcmp(topic, "remotes") == 0)
		return print_remotes_topic();
	if (*topic == 'r' && strcmp(topic, "resource-opener") == 0)
		return print_resource_opener_topic();
	if (*topic == 's' && strcmp(topic, "security") == 0)
		return print_security_topic();
	if (*topic == 's' && strcmp(topic, "selection") == 0)
		return print_selection_topic();
	if (*topic == 's' && strcmp(topic, "search") == 0)
		return print_search_topic();
	if (*topic == 't' && strcmp(topic, "theming") == 0)
		return print_theming_topic();
	if (*topic == 't' && strcmp(topic, "trash") == 0)
		return print_trash_topic();

	xerror("%s: help: '%s': No such help topic\n", PROGRAM_NAME, topic);
	return FUNC_FAILURE;
}

int
quick_help(const char *topic)
{
	if (topic && *topic)
		return run_help_topic(topic);

	char *pager_app = NULL;
	if (xargs.stealth_mode == 1 || !(pager_app = get_pager())) {
		printf("%s                                %s\n\n%s\n\n%s",
			ASCII_LOGO, PROGRAM_NAME_UPPERCASE, QUICK_HELP_HEADER,
			QUICK_HELP_NAVIGATION);
		printf("\n\n%s\n\n%s\n", QUICK_HELP_BASIC_OPERATIONS, QUICK_HELP_MISC);
		return FUNC_SUCCESS;
	}

	char tmp_file[PATH_MAX + 1];
	snprintf(tmp_file, sizeof(tmp_file), "%s/%s", xargs.stealth_mode == 1
		? P_tmpdir : tmp_dir, TMP_FILENAME);

	int fd = mkstemp(tmp_file);
	if (fd == -1) {
		xerror("%s: Error creating temporary file '%s': %s\n",
			PROGRAM_NAME, tmp_file, strerror(errno));
		free(pager_app);
		return FUNC_FAILURE;
	}

	FILE *fp = fdopen(fd, "w");
	if (!fp) {
		xerror("%s: '%s': %s\n", PROGRAM_NAME, tmp_file, strerror(errno));
		free(pager_app);
		if (unlinkat(XAT_FDCWD, tmp_file, 0) == -1)
			xerror("%s: '%s': %s\n", PROGRAM_NAME, tmp_file, strerror(errno));
		close(fd);
		return FUNC_FAILURE;
	}

	fprintf(fp, "%s                                %s\n\n%s\n\n%s",
		ASCII_LOGO, PROGRAM_NAME_UPPERCASE, QUICK_HELP_HEADER,
		QUICK_HELP_NAVIGATION);
	fprintf(fp, "\n\n%s\n\n%s", QUICK_HELP_BASIC_OPERATIONS, QUICK_HELP_MISC);

	fclose(fp);

	int ret = 0;
	char *s = strrchr(pager_app, '/');
	const char *p = (s && *(++s)) ? s : pager_app;

	if (*p == 'l' && strcmp(p, "less") == 0) {
		const char *cmd[] =
			{pager_app, "-FIRXP?e\\(END\\):CLIFM", tmp_file, NULL};
		ret = launch_execv(cmd, FOREGROUND, E_NOFLAG);
	} else {
		const char *cmd[] = {pager_app, tmp_file, NULL};
		ret = launch_execv(cmd, FOREGROUND, E_NOFLAG);
	}

	free(pager_app);

	if (unlinkat(XAT_FDCWD, tmp_file, 0) == -1)
		err('w', PRINT_PROMPT, "help: '%s': %s\n", tmp_file, strerror(errno));

	if (ret != FUNC_SUCCESS)
		return ret;

	if (conf.autols == 1)
		reload_dirlist();

	return FUNC_SUCCESS;
}

__attribute__ ((noreturn))
void
help_function(void)
{
	fputs(NC, stdout);
	printf("%s\n", ASCII_LOGO);
	printf(_("%s %s (%s), by %s\n"), PROGRAM_NAME, VERSION,
		DATE, AUTHOR);
#ifdef _BE_POSIX
	printf("\nUSAGE: %s %s\n%s\n", PROGRAM_NAME, GRAL_USAGE,
		_(OPTIONS_LIST));
#else
	printf("\nUSAGE: %s %s\n%s%s%s", PROGRAM_NAME, GRAL_USAGE,
		_(SHORT_OPTIONS), _(LONG_OPTIONS_A), _(LONG_OPTIONS_B));
#endif /* _BE_POSIX */

	puts("\nBUILT-IN COMMANDS:\n");
	puts(_(CLIFM_COMMANDS_HEADER));
	puts(_(CLIFM_COMMANDS));
	puts(_(CLIFM_KEYBOARD_SHORTCUTS));
	puts(_(HELP_END_NOTE));
	exit(FUNC_SUCCESS);
}
