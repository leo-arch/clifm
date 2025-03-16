/* help.c -- home of clifm's help system */

/*
 * This file is part of Clifm
 * 
 * Copyright (C) 2016-2025, L. Abramovich <leo.clifm@outlook.com>
 * All rights reserved.

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

#include "helpers.h"

#include <errno.h>  /* errno */
#include <string.h>
#include <unistd.h> /* unlink */

#include "aux.h"      /* get_cmd_path */
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
		"Add a new entry to the mimelist file: 'mm edit' or F6",
		"Get mime information for a file. E.g.: 'mm info file'",
		"Manage default associated applications with the 'mime' command",
		"Customize previewing applications: 'view edit' or F7",
		"List mountpoints: 'mp' or Alt+m",
		"Compress and decompress files using the 'ac' and 'ad' " /* NOLINT */
		"commands respectively. E.g.: 'ac sel' or 'ad file.zip'",
		"Disallow the use of shell commands with the -x option: 'clifm -x'",
		"Don't like ELNs? Disable them using the -e command line switch",
		"Disable file previews for tab completion (fzf mode only) via --no-fzfpreview",
# ifdef __linux__
		"Manage removable devices via the 'media' command",
# endif /* __linux__ */
#endif /* _BE_POSIX */
#ifndef _BE_POSIX
		"Create a new profile: 'pf add PROFILE' or 'clifm -P PROFILE'",
		"Run in incognito/private mode (-S, --stealth-mode)",
		"Run in read-only mode via --readonly",
		"Running in an untrusted environment? Try the --secure-env and "
		"--secure-cmds command line switches",
		"Run in disk usage analyzer mode using the -t command line switch",
		"Fuzzy suggestions are supported. E.g.: 'dwn > Downloads'. Enable them via --fuzzy-matching, or FuzzyMatching in the configuration file",
		"Miss LS_COLORS? Run with --lscolors",
#else
		"Create a new profile: 'pf add PROFILE' or 'clifm -p PROFILE'",
		"Run in incognito/private mode via the -s flag",
		"Running in an untrusted environment? Try the -x, -X, and -Y flags",
		"Run in disk usage analyzer mode using the -u flag",
		"Fuzzy suggestions are supported. E.g.: 'dwn > Downloads'. Enable them via the -m flag (or FuzzyMatching in the configuration file)",
#endif /* _BE_POSIX */
		"Clear the screen: 'rf', '.', Enter (on empty line), or Ctrl+l",
		"Try the autocd and auto-open functions: run 'FILE' instead "
		"of 'cd FILE' or 'open FILE'",
		"Do not forget to take a look at the manpage",
		"Need more speed? Try the light mode ('lm' or Alt+y)",
		"The Selection Box is shared among different instances of Clifm",
		"Use the 's' command to select files here and there: 's FILE...'",
		"Use wildcards and regular expressions to select files. E.g.: "
		"'s *.c' or 's .*\\.c$'",
		"Operate on selected files. E.g.: 'p sel' or 'p s:'",
		"List selected files: 'sb' or s:<TAB>",
		"ELNs and the 'sel' keyword work for shell commands as well. E.g.: "
		"'ls -ld 1 sel'",
		"Press TAB to automatically expand an ELN. E.g.: 's 2<TAB>' -> 's FILENAME'",
		"Use ranges (ELN-ELN) to easily move multiple files. E.g.: 'm 3-12 dir/'",
		"Trash files with a simple 't FILE'",
		"Too many files? Run the pager: 'pg' or Alt+0",
		"Switch to long/detail view mode: 'll' or Alt+l",
		"Search for files using the slash command. E.g.: '/*.png'",
		"The search function supports regular expressions. E.g.: '/^c'",
		"Add a new bookmark: 'bm add FILENAME BM_NAME'",
		"Use c, l, m, md, and r instead of cp, ln, mv, mkdir, and rm",
		"Access a remote filesystem using the 'net' command",
		"Move back and forth in the directory history with Alt+j and Alt+k "
		"(also Shift-Left and Shift-Right)",
		"Run a new instance of Clifm: 'x DIR'",
		"Send a command directly to the system shell. E.g.: ';ls -l *'",
		"Run the last executed command: '!!'",
		"Access the command history list: '!<TAB>'",
		"Exclude commands from history using the HistIgnore option in the configuration file (F10)",
		"Access the directory history list: 'dh <TAB>'",
		"List previously used search patterns: '/*<TAB>'",
		"Import aliases from file: 'alias import FILE'",
		"List available aliases: 'alias'",
		"Create aliases to easily run your preferred commands (F10)",
		"Open and edit the configuration file: 'config' or F10",
		"Get a brief description for each Clifm command: 'cmd<TAB>'",
		"Preview the current color scheme: 'cs preview'",
		"Toggle hidden files: 'hh' or Alt+.",
		"Toggle follow-links (long view only): 'k' or Alt++",
		"Change to the root directory: Alt+r",
		"Change to the home directory: Alt+e (or just 'cd')",
		"Open and edit the current color scheme file: F8 (or 'cs edit')",
		"Open and edit the keybindings file: F9 (or 'kb edit')",
		"Open and edit the main configuration file: F10 (or 'config')",
		"Open and edit the bookmarks file: F11 (or 'bm edit')",
		"Open and edit the bookmarks file: F6 (or 'mm edit')",
		"Set the starting path. E.g.: 'clifm ~/media'",
		"Use the 'o' command to open files and directories. E.g.: 'o 12'",
		"Open a file or directory by just entering its ELN or filename",
		"Bypass the file opener by specifying an application. E.g.: '12 leafpad'",
		"Open a file in the background. E.g.: '24&'",
		"Create a custom prompt by editing the prompts file ('prompt edit')",
		"Customize your color scheme: 'cs edit' or F8",
		"Launch the bookmark manager: 'bm' or Alt+b",
		"Quickly list your bookmarks: 'b:<TAB>'",
		"Change to a bookmark: 'bm NAME' or 'b:NAME'",
		"Chain commands using ';' and '&&'. E.g.: 's 2 7-10; r sel'",
		"Add emojis to your prompt by copying them to the prompt line ('prompt edit')",
		"Switch profiles: 'pf set PROFILE'",
		"Delete a profile: 'pf del PROFILE'",
		"Rename a profile: 'pf rename PROFILE'",
		"Use 'p ELN' to print file properties for ELN",
		"Deselect all selected files: 'ds *' or Alt+d",
		"Select all files in the current directory: 's *' or Alt+a",
		"Jump to the Selection Box: 'sb' or Alt+s",
		"Selectively restore trashed files using the 'u' command",
		"Empty the trash can: 't empty'",
		"Toggle list-directories-first: 'ff' or Alt+g",
		"Set the file counter on/off: 'fc'",
		"Take a look at the splash screen with the 'splash' command",
		"Have some fun trying the 'bonus' command",
		"Launch the default system shell in CWD using ':' or ';'",
		"Cycle through file sort orders: Alt+z and Alt+x",
		"Reverse the file sort order: 'st rev'",
		"Rename multiple files at once. E.g.: 'br *.txt'",
		"Need no more tips? Disable this feature in the configuration file",
		"Need root privileges? Launch a new instance of Clifm as root "
		"running the 'X' command (note the uppercase)",
		"Create a fresh configuration file: 'config reset'",
		"Use 'ln edit' (or 'le') to edit symbolic links",
		"Change default keyboard shortcuts: F9 (or 'kb edit')",
		"Keep in sight previous and next visited directories enabling the "
		"DirhistMap option in the configuration file (F10)",
		"Pin a file via the 'pin' command and then use it with the "
		"period keyword (,). E.g.: 'pin DIR' and then 'cd ,'",
		"Switch color schemes using the 'cs' command",
		"Try the 'j' command to quickly jump to any visited directory",
		"Switch workspaces by pressing Alt+[1-4]",
		"Use the 'ws' command to list available workspaces",
		"Take a look at available plugins using the 'actions' command",
		"Space is not required. E.g.: 'p12' instead of 'p 12'",
		"When searching or selecting files, use the exclamation mark "
		"to negate a pattern. E.g.: 's !*.pdf'",
		"Enable the TrashAsRm option to always send removed files to the trash can",
		"Use the 'n' command to create multiple files/directories. E.g.: 'n file dir/'",
		"Add prompt commands via the 'promptcmd' keyword: 'config' (F10)",
		"Need git integration? Consult the manpage",
		"Accept a given suggestion by pressing the Right arrow key",
		"Accept only the first suggested word by pressing Alt+f or Alt+Right",
		"Enter 'c sel' to copy selected files into the current directory",
		"Press Alt+q to delete the last entered word",
		"Check ELN ranges by pressing TAB. E.g.: '1-12<TAB>'",
		"Operate on specific selected files. E.g.: 'p sel<TAB>' or 'p s:<TAB>'",
		"Use the 'ow' command to open a file using a specific application",
		"Limit the number of files listed on the screen via the 'mf' command",
		"Set a maximum filename length for listed files via the MaxFilenameLen "
		"option in the configuration file (F10)",
		"Use the 'm' command to interactively rename a file. E.g.: 'm 12'",
		"Set options on a per-directory basis via the autocommands function. Try 'help autocommands'",
		"Sanitize non-ASCII filenames using the 'bleach' command",
		"Get help for any internal command using the -h or --help parameters: 'p -h'",
		"Enable icons with 'icons on'",
		"Quickly change to any parent directory using the 'bd' command",
		"Use 'stats' to print statistics on files in the current directory",
		"Customize the warning prompt by setting WarningPrompt in the prompts file ('prompt edit')",
		"Create multiple links at once using the 'bl' command",
		"Organize your files using tags. Try 'tag --help'",
		"Remove files in bulk using a text editor with 'rr'",
		"Easily send files to a remote location with the 'cr' command",
		"Quickly switch prompts via 'prompt NAME' (or 'prompt set <TAB>')",
		"Press Alt+Tab to toggle the disk usage analyzer mode",
		"Press Ctrl+Alt+l to toggle max filename length",
		"Wildcards can be expanded with the TAB key. E.g.: 's *.c<TAB>'",
		"Try help topics: 'help <TAB>'",
		"List clifm commands, together with a brief description: 'cmd<TAB>'",
		"List symlinks in the current directory: '=l<TAB>'. Try 'help file-filters' for more information",
		"Use PropFields in the configuration file to customize fields in long view mode",
		"Preview files in the current directory using the 'view' command (requires fzf)",
		"Press Alt+- to launch the file previewer (requires fzf)",
		"Interactively select files (requires fzf, fnf, or smenu). E.g.: 's /dir/*<TAB>'",
		"Change file permissions/ownership using the 'pc' and 'oc' commands respectively",
		"Set a custom shell to run external commands. E.g.: 'CLIFM_SHELL=/bin/dash clifm'",
		"Print all tips: 'tips'",
		"Create files from a template. Run 'n --help' for details.",
		NULL};

	const size_t tipsn = (sizeof(TIPS) / sizeof(TIPS[0])) - 1;

	if (all == 1) {
		size_t i;
		const int l = DIGINUM(tipsn);
		for (i = 0; i < tipsn; i++) {
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
	char *pager_cmd = (char *)NULL;
	char *p = getenv("PAGER");
	if (p) {
		char *s = strchr(p, ' ');
		if (s) *s = '\0';
		pager_cmd = savestring(p, strlen(p));
		return pager_cmd;
	}

	p = get_cmd_path("less");
	if (p) {
		pager_cmd = savestring(p, strlen(p));
		free(p);
		return pager_cmd;
	}

	p = get_cmd_path("more");
	if (p) {
		pager_cmd = savestring(p, strlen(p));
		free(p);
		return pager_cmd;
	}

	return (char *)NULL;
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

	char *pager_app = (char *)NULL;
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
		if (unlinkat(fd, tmp_file, 0) == -1)
			xerror("%s: '%s': %s\n", PROGRAM_NAME, tmp_file, strerror(errno));
		return FUNC_FAILURE;
	}

	fprintf(fp, "%s                                %s\n\n%s\n\n%s",
		ASCII_LOGO, PROGRAM_NAME_UPPERCASE, QUICK_HELP_HEADER,
		QUICK_HELP_NAVIGATION);
	fprintf(fp, "\n\n%s\n\n%s", QUICK_HELP_BASIC_OPERATIONS, QUICK_HELP_MISC);

	fclose(fp);

	int ret = 0;
	char *s = strrchr(pager_app, '/');
	char *p = (s && *(++s)) ? s : pager_app;

	if (*p == 'l' && strcmp(p, "less") == 0) {
		char *cmd[] = {pager_app, "-FIRXP?e\\(END\\):CLIFM", tmp_file, NULL};
		ret = launch_execv(cmd, FOREGROUND, E_NOFLAG);
	} else {
		char *cmd[] = {pager_app, tmp_file, NULL};
		ret = launch_execv(cmd, FOREGROUND, E_NOFLAG);
	}

	free(pager_app);

	if (unlink(tmp_file) == -1)
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
