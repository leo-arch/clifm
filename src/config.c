/* config.c -- functions to define, create, and set configuration files */

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
#include <limits.h>
#include <stdio.h>
#include <readline/readline.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "aux.h"
#include "colors.h"
#include "config.h"
#include "exec.h"
#include "init.h"
#include "listing.h"
#include "mime.h"
#include "misc.h"
#include "navigation.h"

/* Regenerate the configuration file and create a back up of the old
 * one */
int
regen_config(void)
{
	int config_found = 1;
	struct stat config_attrib;

	if (stat(CONFIG_FILE, &config_attrib) == -1) {
		puts(_("No configuration file found"));
		config_found = 0;
	}

	if (config_found) {
		time_t rawtime = time(NULL);
		struct tm *t = localtime(&rawtime);

		char date[18];
		strftime(date, 18, "%Y%m%d@%H:%M:%S", t);

		char *bk = (char *)xnmalloc(strlen(CONFIG_FILE) + strlen(date) + 2, sizeof(char));
		sprintf(bk, "%s.%s", CONFIG_FILE, date);

		char *cmd[] = {"mv", CONFIG_FILE, bk, NULL};

		if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS) {
			free(bk);
			return EXIT_FAILURE;
		}

		printf(_("Old configuration file stored as '%s'\n"), bk);
		free(bk);
	}

	if (create_config(CONFIG_FILE) != EXIT_SUCCESS)
		return EXIT_FAILURE;

	printf(_("New configuration file written to '%s'\n"), CONFIG_FILE);

	reload_config();

	return EXIT_SUCCESS;
}

/* Edit the config file, either via the mime function or via the first
 * passed argument (Ex: 'edit nano'). The 'gen' option regenerates
 * the configuration file and creates a back up of the old one. */
int
edit_function(char **comm)
{
	if (xargs.stealth_mode == 1) {
		printf(_("%s: Access to configuration files is not allowed in "
			 "stealth mode\n"), PROGRAM_NAME);
		return EXIT_SUCCESS;
	}

	if (comm[1] && strcmp(comm[1], "gen") == 0)
		return regen_config();

	if (!config_ok) {
		fprintf(stderr, _("%s: Cannot access the configuration file\n"),
		    PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	/* Get modification time of the config file before opening it */
	struct stat file_attrib;

	/* If, for some reason (like someone erasing the file while the
	 * program is running) clifmrc doesn't exist, call init_config()
	 * to recreate the configuration file. Then run 'stat' again to
	 * reread the attributes of the file */
	if (stat(CONFIG_FILE, &file_attrib) == -1) {
		create_config(CONFIG_FILE);
		stat(CONFIG_FILE, &file_attrib);
	}

	time_t mtime_bfr = (time_t)file_attrib.st_mtime;

	int ret = EXIT_SUCCESS;

	/* If there is an argument... */
	if (comm[1]) {
		char *cmd[] = {comm[1], CONFIG_FILE, NULL};
		ret = launch_execve(cmd, FOREGROUND, E_NOSTDERR);
	}

	/* If no application has been passed as 2nd argument */
	else {
		if (!(flags & FILE_CMD_OK)) {
			fprintf(stderr, _("%s: file: Command not found. Try "
					"'edit APPLICATION'\n"), PROGRAM_NAME);
			ret = EXIT_FAILURE;
		}

		else {
			char *cmd[] = {"mime", CONFIG_FILE, NULL};
			ret = mime_open(cmd);
		}
	}

	if (ret != EXIT_SUCCESS)
		return EXIT_FAILURE;

	/* Get modification time after opening the config file */
	stat(CONFIG_FILE, &file_attrib);
	/* If modification times differ, the file was modified after being
	 * opened */

	if (mtime_bfr != (time_t)file_attrib.st_mtime) {
		/* Reload configuration only if the config file was modified */

		reload_config();
		welcome_message = 0;

		if (cd_lists_on_the_fly) {
			free_dirlist();
			ret = list_dir();
		}
	}

	return ret;
}

void
set_env(void)
{
	if (xargs.stealth_mode == 1)
		return;

	/* Set a few environment variables, mostly useful to run custom
	 * scripts via the actions function */
	/* CLIFM env variable is set to one when CliFM is running, so that
	 * external programs can determine if they were spawned by CliFM */
	setenv("CLIFM", "1", 1);
	setenv("CLIFM_PROFILE", alt_profile ? alt_profile : "default", 1);

	if (SEL_FILE)
		setenv("CLIFM_SELFILE", SEL_FILE, 1);
}

/* Define the file for the Selection Box */
void
set_sel_file(void)
{
	if (SEL_FILE) {
		free(SEL_FILE);
		SEL_FILE = (char *)NULL;
	}

	if (!CONFIG_DIR)
		return;

	size_t config_len = strlen(CONFIG_DIR);

	if (!share_selbox) {
		/* Private selection box is stored in the profile
		 * directory */
		SEL_FILE = (char *)xnmalloc(config_len + 9, sizeof(char));

		sprintf(SEL_FILE, "%s/selbox", CONFIG_DIR);
	}

	else {
		/* Common selection box is stored in the general
		 * configuration directory */
		SEL_FILE = (char *)xnmalloc(config_len + 17, sizeof(char));
		sprintf(SEL_FILE, "%s/.config/%s/selbox", user.home, PNL);
	}

	return;
}

int
create_kbinds_file(void)
{
	if (!config_ok)
		return EXIT_FAILURE;

	struct stat file_attrib;

	if (stat(KBINDS_FILE, &file_attrib) != -1)
		return EXIT_SUCCESS;

	FILE *fp = fopen(KBINDS_FILE, "w");

	if (!fp) {
		_err('w', PRINT_PROMPT, "%s: '%s': %s\n", PROGRAM_NAME, KBINDS_FILE,
		    strerror(errno));
		return EXIT_FAILURE;
	}

	fprintf(fp, "# %s keybindings file\n\n\
# Use the 'kbgen' plugin (compile it first: gcc -o kbgen kbgen.c) to \n\
# find out the escape code for the key o key sequence you want. Use \n\
# either octal, hexadecimal codes or symbols.\n\
# Ex: For Alt-/ (in rxvt terminals) 'kbgen' will print the following \n\
# lines:\n\
# Hex  | Oct | Symbol\n\
# ---- | ---- | ------\n\
# \\x1b | \\033 | ESC (\\e)\n\
# \\x2f | \\057 | /\n\
# In this case, the keybinding, if using symbols, is: \"\\e/:function\"\n\
# In case you prefer the hex codes it would be: \\x1b\\x2f:function.\n\
# GNU emacs escape sequences are also allowed (ex: \"\\M-a\", Alt-a\n\
# in most keyboards, or \"\\C-r\" for Ctrl-r).\n\
# Some codes, especially those involving keys like Ctrl or the arrow\n\
# keys, vary depending on the terminal emulator and the system settings.\n\
# These keybindings should be set up thus on a per terminal basis.\n\
# You can also consult the terminfo database via the infocmp command.\n\
# See terminfo(5) and infocmp(1).\n\
\n\
# Alt-j\n\
previous-dir:\\M-j\n\
# Shift-left (rxvt)\n\
previous-dir2:\\e[d\n\
# Shift-left (xterm)\n\
previous-dir3:\\e[2D\n\
# Shift-left (others)\n\
previous-dir4:\\e[1;2D\n\
\n\
# Alt-k\n\
next-dir:\\M-k\n\
# Shift-right (rxvt)\n\
next-dir2:\\e[c\n\
# Shift-right (xterm)\n\
next-dir3:\\e[2C\n\
# Shift-right (others)\n\
next-dir4:\\e[1;2C\n\
first-dir:\\C-\\M-j\n\
last-dir:\\C-\\M-k\n\
\n\
# Alt-u\n\
parent-dir:\\M-u\n\
# Shift-up (rxvt)\n\
parent-dir2:\\e[a\n\
# Shift-up (xterm)\n\
parent-dir3:\\e[2A\n\
# Shift-up (others)\n\
parent-dir4:\\e[1;2A\n\
\n\
# Alt-e\n\
home-dir:\\M-e\n\
# Home key (rxvt)\n\
home-dir2:\\e[7~\n\
# Home key (xterm)\n\
home-dir3:\\e[H\n\
home-dir4:\n\
\n\
# Alt-r\n\
root-dir:\\M-r\n\
# Alt-/ (rxvt)\n\
root-dir2:\\e/\n\
#root-dir3:\n\
\n\
pinned-dir:\\M-p\n\
\n\
# Help\n\
# F1-3\n\
show-manpage:\\eOP\n\
show-cmds:\\eOQ\n\
show-kbinds:\\eOR\n\
\n\
new-instance:\\C-x\n\
previous-profile:\\C-\\M-o\n\
next-profile:\\C-\\M-p\n\
archive-sel:\\C-\\M-a\n\
rename-sel:\\C-\\M-r\n\
remove-sel:\\C-\\M-d\n\
trash-sel:\\C-\\M-t\n\
untrash-all:\\C-\\M-u\n\
paste-sel:\\C-\\M-v\n\
move-sel:\\C-\\M-n\n\
export-sel:\\C-\\M-e\n\
open-sel:\\C-\\M-g\n\
bookmark-sel:\\C-\\M-b\n\
refresh-screen:\\C-r\n\
clear-line:\\M-c\n\
clear-msgs:\\M-t\n\
show-dirhist:\\M-h\n\
toggle-hidden:\\M-i\n\
toggle-hidden2:\\M-.\n\
toggle-light:\\M-y\n\
toggle-long:\\M-l\n\
sort-previous:\\M-z\n\
sort-next:\\M-x\n\
bookmarks:\\M-b\n\
select-all:\\M-a\n\
deselect-all:\\M-d\n\
mountpoints:\\M-m\n\
folders-first:\\M-f\n\
selbox:\\M-s\n\
lock:\\M-o\n\
# F6-12\n\
open-mime:\\e[17~\n\
open-jump-db:\\e[18~\n\
edit-color-scheme:\\e[19~\n\
open-keybinds:\\e[20~\n\
open-config:\\e[21~\n\
open-bookmarks:\\e[23~\n\
quit:\\e[24~\n\n\
# Plugins\n\
# 1) Make sure your plugin is in the plugins directory (or use any of the\n\
# plugins in there)\n\
# 2) Link pluginx to your plugin using the 'actions edit' command. Ex:\n\
\"plugin1=myplugin.sh\"\n\
# 3) Set a keybinding here for pluginx. Ex: \"plugin1:\\M-7\"\n\n\
#plugin1:\n\
#plugin2:\n\
#plugin3:\n\
#plugin4:\n",
	    PROGRAM_NAME);

	fclose(fp);

	return EXIT_SUCCESS;
}

void
create_tmp_files(void)
{
	size_t pnl_len = strlen(PNL);

	/* #### CHECK THE TMP DIR #### */

	/* If the temporary directory doesn't exist, create it. I create
	 * the parent directory (/tmp/clifm) with 1777 permissions (world
	 * writable with the sticky bit set), so that every user is able
	 * to create files in here, but only the file's owner can remove
	 * or modify them */

	size_t user_len = strlen(user.name);
	TMP_DIR = (char *)xnmalloc(pnl_len + user_len + 7, sizeof(char));
	snprintf(TMP_DIR, pnl_len + 6, "/tmp/%s", PNL);

	struct stat file_attrib;

	if (stat(TMP_DIR, &file_attrib) == -1) {

		char *md_cmd[] = {"mkdir", "-pm1777", TMP_DIR, NULL};

		if (launch_execve(md_cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS) {
			_err('e', PRINT_PROMPT, _("%s: '%s': Error creating temporary "
					"directory\n"), PROGRAM_NAME, TMP_DIR);
		}
	}

	/* Once the parent directory exists, create the user's directory to
	 * store the list of selected files:
	 * TMP_DIR/clifm/username/.selbox_PROFILE. I use here very
	 * restrictive permissions (700), since only the corresponding user
	 * must be able to read and/or modify this list */

	snprintf(TMP_DIR, pnl_len + user_len + 7, "/tmp/%s/%s", PNL, user.name);

	if (stat(TMP_DIR, &file_attrib) == -1) {

		char *md_cmd2[] = {"mkdir", "-pm700", TMP_DIR, NULL};

		if (launch_execve(md_cmd2, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS) {
			selfile_ok = 0;
			_err('e', PRINT_PROMPT, _("%s: '%s': Error creating temporary "
					"directory\n"), PROGRAM_NAME, TMP_DIR);
		}
	}

	/* If the directory exists, check it is writable */
	else if (access(TMP_DIR, W_OK) == -1) {

		if (!SEL_FILE) {
			selfile_ok = 0;
			_err('w', PRINT_PROMPT, "%s: '%s': Directory not writable. Selected "
				"files will be lost after program exit\n",
			    PROGRAM_NAME, TMP_DIR);
		}
	}

	/* If the config directory isn't available, define an alternative
	 * selection file in /tmp */
	if (!SEL_FILE && xargs.stealth_mode != 1) {

		size_t tmp_dir_len = strlen(TMP_DIR);

		if (!share_selbox) {
			size_t prof_len = 0;

			if (alt_profile)
				prof_len = strlen(alt_profile);
			else
				prof_len = 7; /* Lenght of "default" */

			SEL_FILE = (char *)xnmalloc(tmp_dir_len + prof_len + 9,
			    sizeof(char));
			sprintf(SEL_FILE, "%s/selbox_%s", TMP_DIR,
			    (alt_profile) ? alt_profile : "default");
		}

		else {
			SEL_FILE = (char *)xnmalloc(tmp_dir_len + 8, sizeof(char));
			sprintf(SEL_FILE, "%s/selbox", TMP_DIR);
		}

		_err('w', PRINT_PROMPT, _("%s: '%s': Using a temporary directory for "
					  "the Selection Box. Selected files won't be persistent accros "
					  "reboots"), PROGRAM_NAME, TMP_DIR);
	}
}

void
edit_xresources(void)
{
	if (xargs.stealth_mode == 1)
		return;

	/* Check if ~/.Xresources exists and eightBitInput is set to
	 * false. If not, create the file and set the corresponding
	 * value */
	char xresources[PATH_MAX] = "";
	sprintf(xresources, "%s/.Xresources", user.home);

	FILE *xresources_fp = fopen(xresources, "a+");

	if (!xresources_fp) {
		_err('e', PRINT_PROMPT, "%s: fopen: '%s': %s\n",
		    PROGRAM_NAME, xresources, strerror(errno));
		return;
	}

	/* Since I'm looking for very specific lines, which are
	 * fixed lines far below MAX_LINE, I don't care to get
	 * any of the remaining lines truncated */
#if __FreeBSD__
	fseek(xresources_fp, 0, SEEK_SET);
#endif
	char line[256] = "";
	int eight_bit = 0, cursor = 0, function = 0;

	while (fgets(line, (int)sizeof(line), xresources_fp)) {

		if (strncmp(line, "XTerm*eightBitInput: false",
			26) == 0)
			eight_bit = 1;

		else if (strncmp(line, "XTerm*modifyCursorKeys: 1",
			     25) == 0)
			cursor = 1;

		else if (strncmp(line, "XTerm*modifyFunctionKeys: 1",
			     27) == 0)
			function = 1;
	}

	if (!eight_bit || !cursor || !function) {
		/* Set the file position indicator at the end of
		 * the file */
		fseek(xresources_fp, 0L, SEEK_END);

		if (!eight_bit)
			fputs("\nXTerm*eightBitInput: false\n", xresources_fp);

		if (!cursor)
			fputs("\nXTerm*modifyCursorKeys: 1\n", xresources_fp);

		if (!function)
			fputs("\nXTerm*modifyFunctionKeys: 1\n", xresources_fp);

		char *xrdb_path = get_cmd_path("xrdb");

		if (xrdb_path) {
			char *res_file = (char *)xnmalloc(user.home_len + 13, sizeof(char));
			sprintf(res_file, "%s/.Xresources", user.home);
			char *cmd[] = {"xrdb", "merge", res_file, NULL};

			launch_execve(cmd, FOREGROUND, E_NOFLAG);
			free(res_file);
		}

		_err('w', PRINT_PROMPT, _("%s: Restart your %s for changes to "
					  "~/.Xresources to take effect. Otherwise, %s keybindings "
					  "might not work as expected.\n"), PROGRAM_NAME,
					  xrdb_path ? _("terminal") : _("X session"), PROGRAM_NAME);

		if (xrdb_path)
			free(xrdb_path);
	}

	fclose(xresources_fp);

	return;
}

void
define_config_file_names(void)
{
	size_t pnl_len = strlen(PNL);

	/* If $XDG_CONFIG_HOME is set, use it for the config file.
	 * Else, fall back to $HOME/.config */
	char *xdg_config_home = getenv("XDG_CONFIG_HOME");

	if (xdg_config_home) {
		size_t xdg_config_home_len = strlen(xdg_config_home);

		CONFIG_DIR_GRAL = (char *)xnmalloc(xdg_config_home_len + pnl_len + 2, sizeof(char));
		sprintf(CONFIG_DIR_GRAL, "%s/%s", xdg_config_home, PNL);

		xdg_config_home = (char *)NULL;
	}

	else {
		CONFIG_DIR_GRAL = (char *)xnmalloc(user.home_len + pnl_len + 11, sizeof(char));
		sprintf(CONFIG_DIR_GRAL, "%s/.config/%s", user.home, PNL);
	}

	size_t config_gral_len = strlen(CONFIG_DIR_GRAL);

	/* alt_profile will not be NULL whenever the -P option is used
	 * to run the program under an alternative profile */
	if (alt_profile) {
		CONFIG_DIR = (char *)xnmalloc(config_gral_len + strlen(alt_profile) + 11, sizeof(char));
		sprintf(CONFIG_DIR, "%s/profiles/%s", CONFIG_DIR_GRAL, alt_profile);
	}

	else {
		CONFIG_DIR = (char *)xnmalloc(config_gral_len + 18, sizeof(char));
		sprintf(CONFIG_DIR, "%s/profiles/default", CONFIG_DIR_GRAL);
	}

	if (alt_kbinds_file) {
		KBINDS_FILE = savestring(alt_kbinds_file, strlen(alt_kbinds_file));
		free(alt_kbinds_file);
		alt_kbinds_file = (char *)NULL;
	}

	else {
		/* Keybindings per user, not per profile */
		KBINDS_FILE = (char *)xnmalloc(config_gral_len + 13, sizeof(char));
		sprintf(KBINDS_FILE, "%s/keybindings", CONFIG_DIR_GRAL);
	}

	COLORS_DIR = (char *)xnmalloc(config_gral_len + 8, sizeof(char));
	sprintf(COLORS_DIR, "%s/colors", CONFIG_DIR_GRAL);

	PLUGINS_DIR = (char *)xnmalloc(config_gral_len + 9, sizeof(char));
	sprintf(PLUGINS_DIR, "%s/plugins", CONFIG_DIR_GRAL);

	TRASH_DIR = (char *)xnmalloc(user.home_len + 20, sizeof(char));
	sprintf(TRASH_DIR, "%s/.local/share/Trash", user.home);

	size_t trash_len = strlen(TRASH_DIR);

	TRASH_FILES_DIR = (char *)xnmalloc(trash_len + 7, sizeof(char));
	sprintf(TRASH_FILES_DIR, "%s/files", TRASH_DIR);

	TRASH_INFO_DIR = (char *)xnmalloc(trash_len + 6, sizeof(char));
	sprintf(TRASH_INFO_DIR, "%s/info", TRASH_DIR);

	size_t config_len = strlen(CONFIG_DIR);

	DIRHIST_FILE = (char *)xnmalloc(config_len + 13, sizeof(char));
	sprintf(DIRHIST_FILE, "%s/dirhist.cfm", CONFIG_DIR);

	if (!alt_bm_file) {
		BM_FILE = (char *)xnmalloc(config_len + 15, sizeof(char));
		sprintf(BM_FILE, "%s/bookmarks.cfm", CONFIG_DIR);
	}

	else {
		BM_FILE = savestring(alt_bm_file, strlen(alt_bm_file));
		free(alt_bm_file);
		alt_bm_file = (char *)NULL;
	}

	LOG_FILE = (char *)xnmalloc(config_len + 9, sizeof(char));
	sprintf(LOG_FILE, "%s/log.cfm", CONFIG_DIR);

	HIST_FILE = (char *)xnmalloc(config_len + 13, sizeof(char));
	sprintf(HIST_FILE, "%s/history.cfm", CONFIG_DIR);

	if (!alt_config_file) {
		CONFIG_FILE = (char *)xnmalloc(config_len + pnl_len + 4, sizeof(char));
		sprintf(CONFIG_FILE, "%s/%src", CONFIG_DIR, PNL);
	}

	else {
		CONFIG_FILE = savestring(alt_config_file, strlen(alt_config_file));
		free(alt_config_file);
		alt_config_file = (char *)NULL;
	}

	PROFILE_FILE = (char *)xnmalloc(config_len + 13, sizeof(char));
	sprintf(PROFILE_FILE, "%s/profile.cfm", CONFIG_DIR);

	MSG_LOG_FILE = (char *)xnmalloc(config_len + 14, sizeof(char));
	sprintf(MSG_LOG_FILE, "%s/messages.cfm", CONFIG_DIR);

	MIME_FILE = (char *)xnmalloc(config_len + 14, sizeof(char));
	sprintf(MIME_FILE, "%s/mimelist.cfm", CONFIG_DIR);

	ACTIONS_FILE = (char *)xnmalloc(config_len + 13, sizeof(char));
	sprintf(ACTIONS_FILE, "%s/actions.cfm", CONFIG_DIR);

	return;
}

void
copy_plugins(void)
{
	if (!CONFIG_DIR_GRAL)
		return;

	char usr_share_plugins_dir[] = "/usr/share/clifm/plugins";

	/* Make sure the system pĺugins dir exists and is not empty */
	if (count_dir(usr_share_plugins_dir) <= 2)
		return;

	char *cp_comm[] = {"cp", "-r", usr_share_plugins_dir,
	    CONFIG_DIR_GRAL, NULL};
	launch_execve(cp_comm, FOREGROUND, E_NOFLAG);
}

int
create_config(const char *file)
{
	FILE *config_fp = fopen(file, "w");

	if (!config_fp) {
		fprintf(stderr, "%s: fopen: %s: %s\n", PROGRAM_NAME, file, strerror(errno));
		return EXIT_FAILURE;
	}

	/* Do not translate anything in the config file */
	fprintf(config_fp,

	    "\t\t###########################################\n\
\t\t#                  CLIFM                  #\n\
\t\t#  The anti-eye-candy, KISS file manager  #\n\
\t\t###########################################\n\n"

	    "# This is the configuration file for CliFM\n\n"

	    "# Color schemes are stored in the colors directory. By default,\n\
# the 'default' color scheme is used. Visit %s\n\
# to get a few more\n\
ColorScheme=%s\n\n"

	    "# The amount of files contained by a directory is informed next\n\
# to the directory name. However, this feature might slow things down when,\n\
# for example, listing files on a remote server. The filescounter can be\n\
# disabled here, via the --no-files-counter option, or using the 'fc'\n\
# command while in the program itself.\n\
FilesCounter=%s\n\n"

	    "# The character used to construct the line dividing the list of files and\n\
# the prompt. DividingLineChar accepts both literal characters (in single\n\
# quotes) and decimal numbers.\n\
DividingLineChar='%c'\n\n"

	    "# If set to true, print a map of the current position in the directory\n\
# history list, showing previous, current, and next entries\n\
DirhistMap=%s\n\n"

	    "# Use a regex expression to exclude file names when listing files.\n\
# Example: .*~$ to exclude backup files (ending with ~). Do not quote\n\
# the regular expression\n\
Filter=\n\n"

	    "# Set the default copy command. Available options are: 0 = cp,\n\
# 1 = advcp, and 2 = wcp. Both 1 and 2 add a progress bar to cp.\n\
cpCmd=%d\n\n"

	    "# Set the default move command. Available options are: 0 = mv,\n\
# and 1 = advmv. 1 adds a progress bar to mv.\n\
mvCmd=%d\n\n"

	    "# The prompt line is built using string literals and/or one or more of\n\
# the following escape sequences:\n"
	    "# \\xnn: The character whose hexadecimal code is nn.\n\
# \\e: Escape character\n\
# \\h: The hostname, up to the first dot\n\
# \\u: The username\n\
# \\H: The full hostname\n\
# \\n: A newline character\n\
# \\r: A carriage return\n\
# \\a: A bell character\n\
# \\d: The date, in abbrevieted form (ex: 'Tue May 26')\n\
# \\s: The name of the shell (everything after the last slash) currently used\n\
# by CliFM\n\
# \\S: The number of the current workspace\n\
# \\l: Print an 'L' if running in light mode\n\
# \\P: Current profile name\n\
# \\t: The time, in 24-hour HH:MM:SS format\n\
# \\T: The time, in 12-hour HH:MM:SS format\n\
# \\@: The time, in 12-hour am/pm format\n\
# \\A: The time, in 24-hour HH:MM format\n\
# \\w: The full current working directory, with $HOME abbreviated with a tilde\n\
# \\W: The basename of $PWD, with $HOME abbreviated with a tilde\n\
# \\p: A mix of the two above, it abbreviates the current working directory \n\
# only if longer than PathMax (a value defined in the configuration file).\n\
# \\z: Exit code of the last executed command. :) if success and :( in case of\n\
# error\n\
# \\$ '#', if the effective user ID is 0, and '$' otherwise\n\
# \\nnn: The character whose ASCII code is the octal value nnn\n\
# \\\\: A backslash\n\
# \\[: Begin a sequence of non-printing characters. This is mostly used to\n\
# add color to the prompt line\n\
# \\]: End a sequence of non-printing characters\n\n"

	    "Prompt=\"%s\"\n\n",

	    COLORS_REPO,
		DEF_COLOR_SCHEME,
		DEF_FILES_COUNTER == 1 ? "true" : "false",
		DEF_DIV_LINE_CHAR,
		DEF_DIRHIST_MAP == 1 ? "true" : "false",
		DEF_CP_CMD,
		DEF_MV_CMD,
	    DEFAULT_PROMPT);

	fprintf(config_fp,
	    "# MaxPath is only used for the /p option of the prompt: the current working\n\
# directory will be abbreviated to its basename (everything after last slash)\n\
# whenever the current path is longer than MaxPath.\n\
MaxPath=%d\n\n"

	    "WelcomeMessage=%s\n\
SplashScreen=%s\n\
ShowHiddenFiles=%s\n\
LongViewMode=%s\n\
LogCmds=%s\n\n"

	    "# Minimum length at which a file name can be trimmed in long view mode\n\
# (including ELN length and spaces)\n\
MinFilenameTrim=%d\n\n"

	    "# When a directory rank in the jump database is below MinJumpRank, it\n\
# will be forgotten\n\
MinJumpRank=%d\n\n"

	    "# When the sum of all ranks in the jump database reaches MaxJumpTotalRank,\n\
# all ranks will be reduced 10%%, and those falling below MinJumpRank will\n\
# be deleted\n\
MaxJumpTotalRank=%d\n\n"

	    "# Should CliFM be allowed to run external, shell commands?\n\
ExternalCommands=%s\n\n"

	    "# Write the last visited directory to $XDG_CONFIG_HOME/clifm/.last to be\n\
# later accessed by the corresponding shell function at program exit.\n\
# To enable this feature consult the manpage.\n\
CdOnQuit=%s\n\n"

	    "# If set to true, a command name that is the name of a directory or a\n\
# file is executed as if it were the argument to the the 'cd' or the \n\
# 'open' commands respectivelly: 'cd DIR' works the same as just 'DIR'\n\
# and 'open FILE' works the same as just 'FILE'.\n\
Autocd=%s\n\
AutoOpen=%s\n\n"

	    "# If set to true, expand bookmark names into the corresponding bookmark\n\
# path: if the bookmark is \"name=/path\", \"name\" will be interpreted\n\
# as /path. TAB completion is also available for bookmark names.\n\
ExpandBookmarks=%s\n\n"

	    "# In light mode, extra file type checks (except those provided by\n\
# the d_type field of the dirent structure (see readdir(3))\n\
# are disabled to speed up the listing process. stat(3) and access(3)\n\
# are not executed at all, so that we cannot know in advance if a file\n\
# is readable by the current user, if it is executable, SUID, SGID, if a\n\
# symlink is broken, and so on. The file extension check is ignored as\n\
# well, so that the color per extension feature is disabled.\n\
LightMode=%s\n\n",

		DEF_MAX_PATH,
		DEF_WELCOME_MESSAGE == 1 ? "true" : "false",
		DEF_SPLASH_SCREEN == 1 ? "true" : "false",
		DEF_SHOW_HIDDEN == 1 ? "true" : "false",
		DEF_LONG_VIEW == 1 ? "true" : "false",
		DEF_LOGS_ENABLED == 1 ? "true" : "false",
		DEF_MIN_NAME_TRIM,
		DEF_MIN_JUMP_RANK,
		DEF_MAX_JUMP_TOTAL_RANK,
		DEF_EXT_CMD_OK == 1 ? "true" : "false",
		DEF_CD_ON_QUIT == 1 ? "true" : "false",
		DEF_AUTOCD == 1 ? "true" : "false",
		DEF_AUTO_OPEN == 1 ? "true" : "false",
		DEF_EXPAND_BOOKMARKS == 1 ? "true" : "false",
		DEF_LIGHT_MODE == 1 ? "true" : "false"
		);

	fprintf(config_fp,
	    "# If running with colors, append directory indicator and files counter\n\
# to directories. If running without colors (via the --no-colors option),\n\
# append file type indicator at the end of file names: '/' for directories,\n\
# '@' for symbolic links, '=' for sockets, '|' for FIFO/pipes, '*'\n\
# for for executable files, and '?' for unknown file types. Bear in mind\n\
# that when running in light mode the check for executable files won't be\n\
# performed, and thereby no inidicator will be added to executable files.\n\
Classify=%s\n\n"

	    "# Should the Selection Box be shared among different profiles?\n\
ShareSelbox=%s\n\n"

	    "# Choose the resource opener to open files with their default associated\n\
# application. If not set, 'lira', CLiFM's built-in opener, is used.\n\
Opener=\n\n"

	    "# Set the shell to be used when running external commands. Defaults to the\n\
# user's shell as specified in '/etc/passwd'.\n\
SystemShell=\n\n"

	    "# Only used when opening a directory via a new CliFM instance (with the 'x'\n\
# command), this option specifies the command to be used to launch a\n\
# terminal emulator to run CliFM on it.\n\
TerminalCmd='%s'\n\n"

	    "# Choose sorting method: 0 = none, 1 = name, 2 = size, 3 = atime\n\
# 4 = btime (ctime if not available), 5 = ctime, 6 = mtime, 7 = version\n\
# (name if note available) 8 = extension, 9 = inode, 10 = owner, 11 = group\n\
# NOTE: the 'version' method is not available on FreeBSD\n\
Sort=%d\n\
# By default, CliFM sorts files from less to more (ex: from 'a' to 'z' if\n\
# using the \"name\" method). To invert this ordering, set SortReverse to\n\
# true (you can also use the --sort-reverse option or the 'st' command)\n\
SortReverse=%s\n\n"

	    "Tips=%s\n\
ListFoldersFirst=%s\n\
CdListsAutomatically=%s\n\
CaseSensitiveList=%s\n\
CaseSensitiveDirJump=%s\n\
CaseSensitivePathComp=%s\n\
Unicode=%s\n\
Pager=%s\n\
MaxHistory=%d\n\
MaxDirhist=%d\n\
MaxLog=%d\n\
DiskUsage=%s\n\n"

	    "# If set to true, clear the screen before listing files\n\
ClearScreen=%s\n\n"

	    "# If not specified, StartingPath defaults to the current working\n\
# directory.\n\
StartingPath=\n\n"

	    "# If set to true, start CliFM in the last visited directory (and in the\n\
# last used workspace). This option overrides StartingPath.\n\
RestoreLastPath=%s\n\n"

	    "# If set to true, the 'r' command executes 'trash' instead of 'rm' to\n\
# prevent accidental deletions.\n\
TrashAsRm=%s\n\n"

	    "# Set readline editing mode: 0 for vi and 1 for emacs (default).\n\
RlEditMode=%d\n\n"

	    "#END OF OPTIONS\n\n",

		DEF_CLASSIFY == 1 ? "true" : "false",
		DEF_SHARE_SELBOX == 1 ? "true" : "false",
		DEFAULT_TERM_CMD,
		DEF_SORT,
		DEF_SORT_REVERSE == 1 ? "true" : "false",
		DEF_TIPS == 1 ? "true" : "false",
		DEF_LIST_FOLDERS_FIRST == 1 ? "true" : "false",
		DEF_CD_LISTS_ON_THE_FLY == 1 ? "true" : "false",
		DEF_CASE_SENSITIVE == 1 ? "true" : "false",
		DEF_CASE_SENS_DIRJUMP == 1 ? "true" : "false",
		DEF_CASE_SENS_PATH_COMP == 1 ? "true" : "false",
		DEF_UNICODE == 1 ? "true" : "false",
		DEF_PAGER == 1 ? "true" : "false",
		DEF_MAX_HIST,
		DEF_MAX_DIRHIST,
		DEF_MAX_LOG,
		DEF_DISK_USAGE == 1 ? "true" : "false",
		DEF_CLEAR_SCREEN == 1 ? "true" : "false",
		DEF_RESTORE_LAST_PATH == 1 ? "true" : "false",
		DEF_TRASRM == 1 ? "true" : "false",
		DEF_RL_EDIT_MODE
		);

	fputs(

	    "#ALIASES\n\
#alias ls='ls --color=auto -A'\n\n"

	    "#PROMPT COMMANDS\n\n"
	    "# Write below the commands you want to be executed before the prompt.\n\
# Ex:\n\
#/usr/share/clifm/plugins/git_status.sh\n\
#date | awk '{print $1\", \"$2,$3\", \"$4}'\n\n"
	    "#END OF PROMPT COMMANDS\n\n",
	    config_fp);

	fclose(config_fp);

	return EXIT_SUCCESS;
}

void
create_def_cscheme(void)
{
	if (!COLORS_DIR)
		return;

	char *cscheme_file = (char *)xnmalloc(strlen(COLORS_DIR) + 13, sizeof(char));

	sprintf(cscheme_file, "%s/default.cfm", COLORS_DIR);

	/* If the file already exists, do nothing */
	struct stat attr;

	if (stat(cscheme_file, &attr) != -1) {
		free(cscheme_file);
		return;
	}

	FILE *fp = fopen(cscheme_file, "w+");

	if (!fp) {
		_err('w', PRINT_PROMPT, "%s: Error creating default color scheme "
				"file\n", PROGRAM_NAME);
		free(cscheme_file);
		return;
	}

	fprintf(fp, "# CliFM default color scheme\n\n\
# FiletypeColors defines the color used for file types when listing files, \n\
# just as InterfaceColors defines colors for CliFM interface. Both make\n\
# use of the same format used by the LS_COLORS environment variable. Thus,\n\
# \"di=01;34\" means that (non-empty) directories will be listed in bold blue.\n\
# Color codes are traditional ANSI escape sequences less the escape char and\n\
# the final 'm'. 8 bit, 256 colors, and RGB colors are supported.\n\
# A detailed explanation of all these codes can be found in the manpage.\n\n"

		    "FiletypeColors=\"%s\"\n\n"

		    "InterfaceColors=\"%s\"\n\n"

		    "# Same as FiletypeColors, but for file extensions. The format is always\n\
# *.EXT=COLOR\n"
		    "ExtColors=\"%s\"\n\n"

		    "DirIconsColor=\"00;33\"\n",
	    DEF_FILE_COLORS,
	    DEF_IFACE_COLORS,
	    DEF_EXT_COLORS);

	fclose(fp);

	free(cscheme_file);

	return;
}

void
create_config_files(void)
{
	struct stat file_attrib;

			/* #############################
			 * #        TRASH DIRS         #
			 * ############################# */

	if (stat(TRASH_DIR, &file_attrib) == -1) {
		char *trash_files = (char *)NULL;
		trash_files = (char *)xnmalloc(strlen(TRASH_DIR) + 7, sizeof(char));

		sprintf(trash_files, "%s/files", TRASH_DIR);
		char *trash_info = (char *)NULL;
		trash_info = (char *)xnmalloc(strlen(TRASH_DIR) + 6, sizeof(char));

		sprintf(trash_info, "%s/info", TRASH_DIR);
		char *cmd[] = {"mkdir", "-p", trash_files, trash_info, NULL};

		int ret = launch_execve(cmd, FOREGROUND, E_NOFLAG);
		free(trash_files);
		free(trash_info);

		if (ret != EXIT_SUCCESS) {
			trash_ok = 0;
			_err('w', PRINT_PROMPT, _("%s: mkdir: '%s': Error creating trash "
						  "directory. Trash function disabled\n"),
			    PROGRAM_NAME, TRASH_DIR);
		}
	}

	/* If it exists, check it is writable */
	else if (access(TRASH_DIR, W_OK) == -1) {
		trash_ok = 0;
		_err('w', PRINT_PROMPT, _("%s: '%s': Directory not writable. "
					  "Trash function disabled\n"),
		    PROGRAM_NAME, TRASH_DIR);
	}

				/* ####################
				 * #    CONFIG DIR    #
				 * #################### */

	/* If the config directory doesn't exist, create it */
	/* Use the GNU mkdir to let it handle parent directories */
	if (stat(CONFIG_DIR, &file_attrib) == -1) {
		char *tmp_cmd[] = {"mkdir", "-p", CONFIG_DIR, NULL};

		if (launch_execve(tmp_cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS) {

			config_ok = 0;

			_err('e', PRINT_PROMPT, _("%s: mkdir: '%s': Error creating "
						  "configuration directory. Bookmarks, commands logs, and "
						  "command history are disabled. Program messages won't be "
						  "persistent. Using default options\n"),
			    PROGRAM_NAME, CONFIG_DIR);

			return;
		}
	}

	/* If it exists, check it is writable */
	else if (access(CONFIG_DIR, W_OK) == -1) {

		config_ok = 0;

		_err('e', PRINT_PROMPT, _("%s: '%s': Directory not writable. Bookmarks, "
					  "commands logs, and commands history are disabled. Program messages "
					  "won't be persistent. Using default options\n"),
		    PROGRAM_NAME,
		    CONFIG_DIR);

		return;
	}

				/* #####################
				 * #    CONFIG FILE    #
				 * #####################*/

	if (stat(CONFIG_FILE, &file_attrib) == -1) {

		if (create_config(CONFIG_FILE) == EXIT_SUCCESS)
			config_ok = 1;

		else
			config_ok = 0;
	}

	if (!config_ok)
		return;

				/* ######################
				 * #    PROFILE FILE    #
				 * ###################### */

	if (stat(PROFILE_FILE, &file_attrib) == -1) {

		FILE *profile_fp = fopen(PROFILE_FILE, "w");

		if (!profile_fp) {
			_err('e', PRINT_PROMPT, "%s: fopen: '%s': %s\n", PROGRAM_NAME,
			    PROFILE_FILE, strerror(errno));
		}

		else {
			fprintf(profile_fp, _("#%s profile\n\
#Write here the commands you want to be executed at startup\n\
#Ex:\n#echo -e \"%s, the anti-eye-candy/KISS file manager\"\n"),
			    PROGRAM_NAME, PROGRAM_NAME);
			fclose(profile_fp);
		}
	}

				/* #####################
				 * #    COLORS DIR     #
				 * ##################### */

	if (stat(COLORS_DIR, &file_attrib) == -1) {

		char *cmd[] = {"mkdir", COLORS_DIR, NULL};

		if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS) {
			_err('w', PRINT_PROMPT, _("%s: mkdir: Error creating colors "
				"directory. Using the default color scheme\n"),
			    PROGRAM_NAME);
		}
	}

	/* Generate the default color scheme file */
	create_def_cscheme();

				/* #####################
				 * #      PLUGINS      #
				 * #####################*/

	if (stat(PLUGINS_DIR, &file_attrib) == -1) {
		char *cmd[] = {"mkdir", PLUGINS_DIR, NULL};

		if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
			_err('e', PRINT_PROMPT, _("%s: mkdir: Error creating plugins "
						  "directory. The actions function is disabled\n"),
			    PROGRAM_NAME);
		else
			copy_plugins();
	}

				/* #####################
				 * #    ACTIONS FILE   #
				 * #####################*/

	if (stat(ACTIONS_FILE, &file_attrib) == -1) {
		FILE *actions_fp = fopen(ACTIONS_FILE, "w");

		if (!actions_fp) {
			_err('e', PRINT_PROMPT, "%s: '%s': %s\n", PROGRAM_NAME,
			    ACTIONS_FILE, strerror(errno));
		}

		else {
			fprintf(actions_fp, "######################\n"
					    "# %s actions file #\n"
					    "######################\n\n"
					    "# Define here your custom actions. Actions are "
					    "custom command names\n"
					    "# binded to a shell script located in "
					    "$XDG_CONFIG_HOME/clifm/PROFILE/scripts.\n"
					    "# Actions can be executed directly from "
					    "%s command line, as if they\n"
					    "# were any other command, and the associated "
					    "script will be executed\n"
					    "# instead. All parameters passed to the action "
					    "command will be passed\n"
					    "# to the corresponding plugin as well.\n\n"
					    "i=img_viewer.sh\n"
					    "kbgen=kbgen\n"
					    "vid=vid_viewer.sh\n"
					    "ptot=pdf_viewer.sh\n"
					    "music=music_player.sh\n"
					    "update=update.sh\n"
					    "wall=wallpaper_setter.sh\n"
					    "dragon=dragondrop.sh\n"
					    "bn=batch_create.sh\n"
					    "+=finder.sh\n"
					    "++=jumper.sh\n"
					    "-=fzfnav.sh\n"
					    "*=fzfsel.sh\n"
					    "**=fzfdesel.sh\n"
					    "h=fzfhist.sh\n"
					    "//=rgfind.sh\n"
					    "ih=ihelp.sh\n",
			    PROGRAM_NAME, PROGRAM_NAME);

			fclose(actions_fp);
		}
	}

				/* #####################
				 * #     MIME FILE     #
				 * #####################*/

	if (stat(MIME_FILE, &file_attrib) == 0)
		return;

	char sys_mimelist[] = "/usr/share/clifm/mimelist.cfm";

	_err('n', PRINT_PROMPT, _("%s created a new MIME list file (%s) "
			"It is recommended to edit this file (entering 'mm edit' or "
			"pressing F6) to add the programs you use and remove those "
			"you don't. This will make the process of opening files "
			"faster and smoother\n"
			"The default MIME list file, covering the most common file type "
			"associations, can be found in %s.\n"),
			PROGRAM_NAME, MIME_FILE, sys_mimelist);

	/* Try importing MIME associations from the system, and in
	 * case nothing can be imported use the default mimelist.cfm file */
	if (mime_import(MIME_FILE) == EXIT_SUCCESS)
		return;

	FILE *mime_fp = fopen(MIME_FILE, "w");

	if (!mime_fp) {
		_err('e', PRINT_PROMPT, "%s: fopen: '%s': %s\n",
			PROGRAM_NAME, MIME_FILE, strerror(errno));
		return;
	}

	if (stat(sys_mimelist, &file_attrib) == -1) {
		_err('e', PRINT_PROMPT, "%s: %s: %s\n", PROGRAM_NAME,
			sys_mimelist, strerror(errno));
		fclose(mime_fp);
		return;
	}

	char *cmd[] = {"cp", "-f", sys_mimelist, MIME_FILE, NULL};
	launch_execve(cmd, FOREGROUND, E_NOFLAG);

	fclose(mime_fp);
}

int
create_bm_file(void)
{
	if (!BM_FILE)
		return EXIT_FAILURE;

	struct stat file_attrib;

	if (stat(BM_FILE, &file_attrib) == -1) {
		FILE *fp = fopen(BM_FILE, "w+");

		if (!fp) {
			_err('e', PRINT_PROMPT, "bookmarks: '%s': %s\n", BM_FILE,
			    strerror(errno));
			return EXIT_FAILURE;
		}

		else {
			fprintf(fp, "### This is %s bookmarks file ###\n\n"
				    "# Empty and commented lines are ommited\n"
				    "# The bookmarks syntax is: [shortcut]name:path\n"
				    "# Example:\n"
				    "[c]clifm:%s\n",
			    PROGRAM_NAME, CONFIG_DIR ? CONFIG_DIR : "path/to/file");
			fclose(fp);
		}
	}

	return EXIT_SUCCESS;
}

void
read_config(void)
{
	int ret = -1;

	FILE *config_fp = fopen(CONFIG_FILE, "r");

	if (!config_fp) {
		_err('e', PRINT_PROMPT, _("%s: fopen: '%s': %s. Using default values.\n"),
		    PROGRAM_NAME, CONFIG_FILE, strerror(errno));
		return;
	}

	if (xargs.rl_vi_mode == 1)
		rl_vi_editing_mode(1, 0);

	div_line_char = UNSET;
#define MAX_BOOL 6
	/* starting path(14) + PATH_MAX + \n(1)*/
	char line[PATH_MAX + 15];

	while (fgets(line, (int)sizeof(line), config_fp)) {

		if (*line == '\n' || (*line == '#' && line[1] != 'E'))
			continue;

		if (*line == '#' && strncmp(line, "#END OF OPTIONS", 15) == 0)
			break;

		/* Check for the xargs.splash flag. If -1, it was
		 * not set via command line, so that it must be
		 * set here */
		else if (xargs.splash == UNSET && *line == 'S'
		&& strncmp(line, "SplashScreen=", 13) == 0) {

			char opt_str[MAX_BOOL] = ""; /* false (5) + 1 */
			ret = sscanf(line, "SplashScreen=%5s\n", opt_str);
			/* According to cppcheck: "sscanf() without field
			 * width limits can crash with huge input data".
			 * Field width limits = %5s */

			if (ret == -1)
				continue;

			if (strncmp(opt_str, "true", 4) == 0)
				splash_screen = 1;

			else if (strncmp(opt_str, "false", 5) == 0)
				splash_screen = 0;
		}

		else if (xargs.case_sens_dirjump == UNSET && *line == 'C'
		&& strncmp(line, "CaseSensitiveDirJump=", 21) == 0) {

			char opt_str[MAX_BOOL] = ""; /* false (5) + 1 */
			ret = sscanf(line, "CaseSensitiveDirJump=%5s\n", opt_str);

			if (ret == -1)
				continue;

			if (strncmp(opt_str, "true", 4) == 0)
				case_sens_dirjump = 1;

			else if (strncmp(opt_str, "false", 5) == 0)
				case_sens_dirjump = 0;
		}

		else if (xargs.case_sens_path_comp == UNSET && *line == 'C'
		&& strncmp(line, "CaseSensitivePathComp=", 22) == 0) {

			char opt_str[MAX_BOOL] = ""; /* false (5) + 1 */
			ret = sscanf(line, "CaseSensitivePathComp=%5s\n", opt_str);

			if (ret == -1)
				continue;

			if (strncmp(opt_str, "true", 4) == 0)
				case_sens_path_comp = 1;

			else if (strncmp(opt_str, "false", 5) == 0)
				case_sens_path_comp = 0;
		}

		else if (!filter && *line == 'F' && strncmp(line, "Filter=", 7) == 0) {

			char *opt_str = strchr(line, '=');

			if (!opt_str)
				continue;

			size_t len = strlen(opt_str);
			if (opt_str[len - 1] == '\n')
				opt_str[len - 1] = '\0';

			if (!*(++opt_str))
				continue;

			filter = savestring(opt_str, len);
		}

		else if (!usr_cscheme && *line == 'C' && strncmp(line, "ColorScheme=", 12) == 0) {

			char *opt_str = (char *)NULL;
			opt_str = strchr(line, '=');

			if (!opt_str)
				continue;

			size_t len = strlen(opt_str);
			if (opt_str[len - 1] == '\n')
				opt_str[len - 1] = '\0';

			if (!*(++opt_str))
				continue;

			usr_cscheme = savestring(opt_str, len);
		}

		else if (xargs.light == UNSET && *line == 'L'
		&& strncmp(line, "LightMode=", 10) == 0) {

			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "LightMode=%5s\n", opt_str);

			if (ret == -1)
				continue;

			if (strncmp(opt_str, "true", 4) == 0)
				light_mode = 1;

			else if (strncmp(opt_str, "false", 5) == 0)
				light_mode = 0;
		}

		else if (xargs.trasrm == UNSET && *line == 'T'
		&& strncmp(line, "TrashAsRm=", 10) == 0) {

			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "TrashAsRm=%5s\n", opt_str);

			if (ret == -1)
				continue;

			if (strncmp(opt_str, "true", 4) == 0)
				tr_as_rm = 1;

			else if (strncmp(opt_str, "false", 5) == 0)
				tr_as_rm = 0;
		}

		else if (xargs.cd_on_quit == UNSET && *line == 'C'
		&& strncmp(line, "CdOnQuit=", 9) == 0) {

			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "CdOnQuit=%5s\n", opt_str);

			if (ret == -1)
				continue;

			if (strncmp(opt_str, "true", 4) == 0)
				cd_on_quit = 1;

			else if (strncmp(opt_str, "false", 5) == 0)
				cd_on_quit = 0;
		}

		else if (xargs.expand_bookmarks == UNSET && *line == 'E'
		&& strncmp(line, "ExpandBookmarks=", 16) == 0) {

			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "ExpandBookmarks=%5s\n",
			    opt_str);
			if (ret == -1)
				continue;

			if (strncmp(opt_str, "true", 4) == 0)
				expand_bookmarks = 1;

			else if (strncmp(opt_str, "false", 5) == 0)
				expand_bookmarks = 0;
		}

		else if (xargs.restore_last_path == UNSET && *line == 'R'
		&& strncmp(line, "RestoreLastPath=", 16) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "RestoreLastPath=%5s\n",
			    opt_str);
			if (ret == -1)
				continue;

			if (strncmp(opt_str, "true", 4) == 0)
				restore_last_path = 1;

			else if (strncmp(opt_str, "false", 5) == 0)
				restore_last_path = 0;
		}

		else if (!opener && *line == 'O' && strncmp(line, "Opener=", 7) == 0) {

			char *opt_str = (char *)NULL;
			opt_str = straft(line, '=');

			if (!opt_str)
				continue;

			char *tmp = remove_quotes(opt_str);

			if (!tmp) {
				free(opt_str);
				continue;
			}

			opener = savestring(tmp, strlen(tmp));
			free(opt_str);
		}

		else if (xargs.tips == UNSET && *line == 'T' && strncmp(line, "Tips=", 5) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "Tips=%5s\n", opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "false", 5) == 0)
				tips = 0;
			else if (strncmp(opt_str, "true", 4) == 0)
				tips = 1;
		}

		else if (xargs.disk_usage == UNSET && *line == 'D'
		&& strncmp(line, "DiskUsage=", 10) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "DiskUsage=%5s\n", opt_str);

			if (ret == -1)
				continue;

			if (strncmp(opt_str, "true", 4) == 0)
				disk_usage = 1;

			else if (strncmp(opt_str, "false", 5) == 0)
				disk_usage = 0;
		}

		else if (xargs.autocd == UNSET && *line == 'A'
		&& strncmp(line, "Autocd=", 7) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "Autocd=%5s\n", opt_str);

			if (ret == -1)
				continue;

			if (strncmp(opt_str, "true", 4) == 0)
				autocd = 1;

			else if (strncmp(opt_str, "false", 5) == 0)
				autocd = 0;
		}

		else if (xargs.auto_open == UNSET && *line == 'A'
		&& strncmp(line, "AutoOpen=", 9) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "AutoOpen=%5s\n", opt_str);

			if (ret == -1)
				continue;

			if (strncmp(opt_str, "true", 4) == 0)
				auto_open = 1;

			else if (strncmp(opt_str, "false", 5) == 0)
				auto_open = 0;
		}

		else if (xargs.dirmap == UNSET && *line == 'D'
		&& strncmp(line, "DirhistMap=", 11) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "DirhistMap=%5s\n", opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				dirhist_map = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				dirhist_map = 0;
		}

		else if (xargs.classify == UNSET && *line == 'C'
		&& strncmp(line, "Classify=", 9) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "Classify=%5s\n", opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				classify = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				classify = 0;
		}

		else if (xargs.share_selbox == UNSET && *line == 'S'
		&& strncmp(line, "ShareSelbox=", 12) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "ShareSelbox=%5s\n", opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				share_selbox = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				share_selbox = 0;
		}

		else if (*line == 'M' && strncmp(line, "MinJumpRank=", 12) == 0) {
			int opt_num = 0;
			ret = sscanf(line, "MinJumpRank=%d\n", &opt_num);
			if (ret == -1 || opt_num < INT_MIN || opt_num > INT_MAX)
				continue;
			min_jump_rank = opt_num;
		}

		else if (*line == 'M' && strncmp(line, "MaxJumpTotalRank=", 17) == 0) {
			int opt_num = 0;
			ret = sscanf(line, "MaxJumpTotalRank=%d\n", &opt_num);
			if (ret == -1 || opt_num < INT_MIN || opt_num > INT_MAX)
				continue;
			max_jump_total_rank = opt_num;
		}

		else if (xargs.sort == UNSET && *line == 'S' && strncmp(line, "Sort=", 5) == 0) {
			int opt_num = 0;
			ret = sscanf(line, "Sort=%d\n", &opt_num);
			if (ret == -1)
				continue;
			if (opt_num >= 0 && opt_num <= SORT_TYPES)
				sort = opt_num;
			else /* default (sort by name) */
				sort = DEF_SORT;
		}

		else if (*line == 'M' && strncmp(line, "MinFilenameTrim=", 16) == 0) {
			int opt_num = 0;
			ret = sscanf(line, "MinFilenameTrim=%d\n", &opt_num);
			if (ret == -1)
				continue;
			if (opt_num > 0)
				min_name_trim = opt_num;
			else /* default */
				min_name_trim = DEF_MIN_NAME_TRIM;
		}

		else if (*line == 'c' && strncmp(line, "cpCmd=", 6) == 0) {
			int opt_num = 0;
			ret = sscanf(line, "cpCmd=%d\n", &opt_num);
			if (ret == -1)
				continue;
			if (opt_num >= 0 && opt_num <= 2)
				cp_cmd = opt_num;
			else /* default (sort by name) */
				cp_cmd = DEF_CP_CMD;
		}

		else if (*line == 'm' && strncmp(line, "mvCmd=", 6) == 0) {
			int opt_num = 0;
			ret = sscanf(line, "mvCmd=%d\n", &opt_num);
			if (ret == -1)
				continue;
			if (opt_num == 0 || opt_num == 1)
				mv_cmd = opt_num;
			else /* default (sort by name) */
				mv_cmd = DEF_MV_CMD;
		}

		else if (*line == 'R' && strncmp(line, "RlEditMode=0", 12) == 0) {
			rl_vi_editing_mode(1, 0);
			/* By default, readline uses emacs editing
			 * mode */
		}

		else if (xargs.max_dirhist == UNSET && *line == 'M'
		&& strncmp(line, "MaxDirhist=", 11) == 0) {
			int opt_num = 0;
			ret = sscanf(line, "MaxDirhist=%d\n", &opt_num);
			if (ret == -1)
				continue;
			if (opt_num >= 0)
				max_dirhist = opt_num;
			else /* default */
				max_dirhist = DEF_MAX_DIRHIST;
		}

		else if (xargs.sort_reverse == UNSET && *line == 'S'
		&& strncmp(line, "SortReverse=", 12) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "SortReverse=%5s\n", opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				sort_reverse = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				sort_reverse = 0;
		}

		else if (xargs.files_counter == UNSET && *line == 'F'
		&& strncmp(line, "FilesCounter=", 13) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "FilesCounter=%5s\n", opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				files_counter = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				files_counter = 0;
		}

		else if (xargs.welcome_message == UNSET && *line == 'W'
		&& strncmp(line, "WelcomeMessage=", 15) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "WelcomeMessage=%5s\n",
			    opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				welcome_message = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				welcome_message = 0;
		}

		else if (xargs.clear_screen == UNSET && *line == 'C'
		&& strncmp(line, "ClearScreen=", 12) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "ClearScreen=%5s\n",
			    opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				clear_screen = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				clear_screen = 0;
		}

		else if (xargs.hidden == UNSET && *line == 'S'
		&& strncmp(line, "ShowHiddenFiles=", 16) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "ShowHiddenFiles=%5s\n",
			    opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				show_hidden = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				show_hidden = 0;
		}

		else if (xargs.longview == UNSET && *line == 'L'
		&& strncmp(line, "LongViewMode=", 13) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "LongViewMode=%5s\n",
			    opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				long_view = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				long_view = 0;
		}

		else if (xargs.ext == UNSET && *line == 'E'
		&& strncmp(line, "ExternalCommands=", 17) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "ExternalCommands=%5s\n",
			    opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				ext_cmd_ok = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				ext_cmd_ok = 0;
		}

		else if (xargs.logs == UNSET && *line == 'L'
		&& strncmp(line, "LogCmds=", 8) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "LogCmds=%5s\n", opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				logs_enabled = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				logs_enabled = 0;
		}

		else if (*line == 'S' && strncmp(line, "SystemShell=", 12) == 0) {
			free(user.shell);
			user.shell = (char *)NULL;
			char *opt_str = straft(line, '=');
			if (!opt_str)
				continue;

			char *tmp = remove_quotes(opt_str);
			if (!tmp) {
				free(opt_str);
				continue;
			}

			if (*tmp == '/') {
				if (access(tmp, F_OK | X_OK) != 0) {
					free(opt_str);
					continue;
				}

				user.shell = savestring(tmp, strlen(tmp));
			}

			else {
				char *shell_path = get_cmd_path(tmp);

				if (!shell_path) {
					free(opt_str);
					continue;
				}

				user.shell = savestring(shell_path, strlen(shell_path));
				free(shell_path);
			}

			free(opt_str);
		}

		else if (*line == 'T' && strncmp(line, "TerminalCmd=", 12) == 0) {

			if (term) {
				free(term);
				term = (char *)NULL;
			}

			char *opt_str = straft(line, '=');
			if (!opt_str)
				continue;

			char *tmp = remove_quotes(opt_str);
			if (!tmp) {
				free(opt_str);
				continue;
			}

			term = savestring(tmp, strlen(tmp));
			free(opt_str);
		}

		else if (xargs.ffirst == UNSET && *line == 'L'
		&& strncmp(line, "ListFoldersFirst=", 17) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "ListFoldersFirst=%5s\n",
			    opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				list_folders_first = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				list_folders_first = 0;
		}

		else if (xargs.cd_list_auto == UNSET && *line == 'C'
		&& strncmp(line, "CdListsAutomatically=", 21) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "CdListsAutomatically=%5s\n",
			    opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				cd_lists_on_the_fly = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				cd_lists_on_the_fly = 0;
		}

		else if (xargs.sensitive == UNSET && *line == 'C'
		&& strncmp(line, "CaseSensitiveList=", 18) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "CaseSensitiveList=%5s\n",
			    opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				case_sensitive = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				case_sensitive = 0;
		}

		else if (xargs.unicode == UNSET && *line == 'U'
		&& strncmp(line, "Unicode=", 8) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "Unicode=%5s\n", opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				unicode = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				unicode = 0;
		}

		else if (xargs.pager == UNSET && *line == 'P'
		&& strncmp(line, "Pager=", 6) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "Pager=%5s\n", opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				pager = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				pager = 0;
		}

		else if (*line == 'P' && strncmp(line, "Prompt=", 7) == 0) {
			if (encoded_prompt)
				free(encoded_prompt);
			encoded_prompt = straft(line, '=');
		}

		else if (xargs.max_path == UNSET && *line == 'M'
		&& strncmp(line, "MaxPath=", 8) == 0) {
			int opt_num = 0;
			sscanf(line, "MaxPath=%d\n", &opt_num);
			if (opt_num <= 0)
				continue;
			max_path = opt_num;
		}

		else if (*line == 'D' && strncmp(line, "DividingLineChar=", 17) == 0) {
			/* Accepts both chars and decimal integers */
			char opt_c = -1;
			sscanf(line, "DividingLineChar='%c'", &opt_c);
			if (opt_c == -1) {
				int num = -1;
				sscanf(line, "DividingLineChar=%d", &num);
				if (num == -1)
					div_line_char = DEF_DIV_LINE_CHAR;
				else
					div_line_char = (char)num;
			} else
				div_line_char = opt_c;
		}

		else if (*line == 'M' && strncmp(line, "MaxHistory=", 11) == 0) {
			int opt_num = 0;
			sscanf(line, "MaxHistory=%d\n", &opt_num);
			if (opt_num <= 0)
				continue;
			max_hist = opt_num;
		}

		else if (*line == 'M' && strncmp(line, "MaxLog=", 7) == 0) {
			int opt_num = 0;
			sscanf(line, "MaxLog=%d\n", &opt_num);
			if (opt_num <= 0)
				continue;
			max_log = opt_num;
		}

		else if (xargs.path == UNSET && cur_ws == UNSET && *line == 'S'
		&& strncmp(line, "StartingPath=", 13) == 0) {
			char *opt_str = straft(line, '=');
			if (!opt_str)
				continue;

			char *tmp = remove_quotes(opt_str);
			if (!tmp) {
				free(opt_str);
				continue;
			}

			/* If starting path is not NULL, and exists,
			 * and is a directory, and the user has
			 * appropriate permissions, set path to starting
			 * path. If any of these conditions is false,
			 * path will be set to default, that is, CWD */
			if (xchdir(tmp, SET_TITLE) == 0) {
				free(ws[cur_ws].path);
				ws[cur_ws].path = savestring(tmp, strlen(tmp));
			} else {
				_err('w', PRINT_PROMPT, _("%s: '%s': %s. "
							  "Using the current working directory "
							  "as starting path\n"),
				    PROGRAM_NAME,
				    tmp, strerror(errno));
			}
			free(opt_str);
		}
	}

	fclose(config_fp);

	if (filter) {
		ret = regcomp(&regex_exp, filter, REG_NOSUB | REG_EXTENDED);

		if (ret != EXIT_SUCCESS) {
			_err('w', PRINT_PROMPT, _("%s: '%s': Invalid regular "
				  "expression\n"), PROGRAM_NAME, filter);
			free(filter);
			filter = (char *)NULL;
			regfree(&regex_exp);
		}
	}

	return;
}

/* Set up CliFM directories and config files. Load the user's
 * configuration from clifmrc */
void
init_config(void)
{
	if (xargs.stealth_mode == 1) {

		_err(0, PRINT_PROMPT, _("%s: Running in stealth mode: trash, persistent "
					"selection and directory history, just as bookmarks, logs and "
					"configuration files, are disabled.\n"),
		    PROGRAM_NAME);

		config_ok = 0;

		return;
	}

	/* Store a pointer to the current LS_COLORS value to be used by
	 * external commands */
	ls_colors_bk = getenv("LS_COLORS");

	if (!home_ok)
		return;

	define_config_file_names();

	create_config_files();

	if (config_ok)
		read_config();

	/* "XTerm*eightBitInput: false" must be set in HOME/.Xresources
	 * to make some keybindings like Alt+letter work correctly in
	 * xterm-like terminal emulators */
	/* However, there is no need to do this if using the linux console,
	 * since we are not in a graphical environment */
	if (xargs.stealth_mode != 1 && (flags & GUI)
	&& strncmp(getenv("TERM"), "xterm", 5) == 0)
		edit_xresources();
}

int
reload_config(void)
{
	/* Free everything */
	free(CONFIG_DIR_GRAL);
	free(CONFIG_DIR);
	free(TRASH_DIR);
	free(TRASH_FILES_DIR);
	free(TRASH_INFO_DIR);
	CONFIG_DIR = TRASH_DIR = TRASH_FILES_DIR = (char *)NULL;
	TRASH_INFO_DIR = (char *)NULL;

	free(BM_FILE);
	free(LOG_FILE);
	free(HIST_FILE);
	free(DIRHIST_FILE);
	BM_FILE = LOG_FILE = HIST_FILE = DIRHIST_FILE = (char *)NULL;

	free(CONFIG_FILE);
	free(PROFILE_FILE);
	free(MSG_LOG_FILE);
	CONFIG_FILE = PROFILE_FILE = MSG_LOG_FILE = (char *)NULL;

	free(MIME_FILE);
	free(PLUGINS_DIR);
	free(ACTIONS_FILE);
	free(KBINDS_FILE);
	MIME_FILE = PLUGINS_DIR = ACTIONS_FILE = KBINDS_FILE = (char *)NULL;

	free(COLORS_DIR);
	free(TMP_DIR);
	free(SEL_FILE);
	TMP_DIR = COLORS_DIR = SEL_FILE = (char *)NULL;

	if (filter) {
		regfree(&regex_exp);
		free(filter);
		filter = (char *)NULL;
	}

	if (opener) {
		free(opener);
		opener = (char *)NULL;
	}

	if (encoded_prompt) {
		free(encoded_prompt);
		encoded_prompt = (char *)NULL;
	}

	if (term) {
		free(term);
		term = (char *)NULL;
	}

	free(user.shell);
	user.shell = (char *)NULL;

	/* Reset all variables */
	splash_screen = welcome_message = ext_cmd_ok = pager = UNSET;
	show_hidden = clear_screen = list_folders_first = long_view = UNSET;
	unicode = case_sensitive = cd_lists_on_the_fly = share_selbox = UNSET;
	autocd = auto_open = restore_last_path = dirhist_map = UNSET;
	disk_usage = tips = logs_enabled = sort = files_counter = UNSET;
	light_mode = classify = cd_on_quit = columned = tr_as_rm = UNSET;
	no_eln = min_name_trim = case_sens_dirjump = case_sens_path_comp = UNSET;
	min_jump_rank = max_jump_total_rank = UNSET;

	shell_terminal = no_log = internal_cmd = recur_perm_error_flag = 0;
	is_sel = sel_is_last = print_msg = kbind_busy = dequoted = 0;
	mime_match = sort_switch = sort_reverse = kbind_busy = 0;
	shell_terminal = no_log = internal_cmd = dequoted = 0;
	shell_is_interactive = recur_perm_error_flag = mime_match = 0;
	recur_perm_error_flag = is_sel = sel_is_last = print_msg = 0;

	pmsg = nomsg;

	home_ok = config_ok = trash_ok = selfile_ok = 1;

	/* Set up config files and options */
	init_config();

	/* If some option was not set, set it to the default value*/
	check_options();

	set_sel_file();

	create_tmp_files();

	set_colors(usr_cscheme ? usr_cscheme : "default", 1);

	free(usr_cscheme);
	usr_cscheme = (char *)NULL;

	/* If some option was set via command line, keep that value
	 * for any profile */
	if (xargs.case_sens_dirjump != UNSET)
		case_sens_dirjump = xargs.case_sens_dirjump;
	if (xargs.case_sens_path_comp != UNSET)
		case_sens_path_comp = xargs.case_sens_path_comp;
	if (xargs.noeln != UNSET)
		no_eln = xargs.noeln;
	if (xargs.trasrm != UNSET)
		tr_as_rm = xargs.trasrm;
	if (xargs.no_colors != UNSET)
		colorize = xargs.no_colors;
	if (xargs.no_columns != UNSET)
		columned = xargs.no_columns;
	if (xargs.cd_on_quit != UNSET)
		cd_on_quit = xargs.cd_on_quit;
	if (xargs.ext != UNSET)
		ext_cmd_ok = xargs.ext;
	if (xargs.splash != UNSET)
		splash_screen = xargs.splash;
	if (xargs.light != UNSET)
		light_mode = xargs.light;
	if (xargs.sort != UNSET)
		sort = xargs.sort;
	if (xargs.hidden != UNSET)
		show_hidden = xargs.hidden;
	if (xargs.longview != UNSET)
		long_view = xargs.longview;
	if (xargs.ffirst != UNSET)
		list_folders_first = xargs.ffirst;
	if (xargs.cd_list_auto != UNSET)
		cd_lists_on_the_fly = xargs.cd_list_auto;
	if (xargs.sensitive != UNSET)
		case_sensitive = xargs.sensitive;
	if (xargs.unicode != UNSET)
		unicode = xargs.unicode;
	if (xargs.pager != UNSET)
		pager = xargs.pager;
	if (xargs.dirmap != UNSET)
		dirhist_map = xargs.dirmap;
	if (xargs.autocd != UNSET)
		autocd = xargs.autocd;
	if (xargs.auto_open != UNSET)
		auto_open = xargs.auto_open;
	if (xargs.restore_last_path != UNSET)
		restore_last_path = xargs.restore_last_path;
	if (xargs.tips != UNSET)
		tips = xargs.tips;
	if (xargs.disk_usage != UNSET)
		disk_usage = xargs.disk_usage;
	if (xargs.classify != UNSET)
		classify = xargs.classify;
	if (xargs.share_selbox != UNSET)
		share_selbox = xargs.share_selbox;
	if (xargs.max_dirhist != UNSET)
		max_dirhist = xargs.max_dirhist;
	if (xargs.sort_reverse != UNSET)
		sort_reverse = xargs.sort_reverse;
	if (xargs.files_counter != UNSET)
		files_counter = xargs.files_counter;
	if (xargs.welcome_message != UNSET)
		welcome_message = xargs.welcome_message;
	if (xargs.clear_screen != UNSET)
		clear_screen = xargs.clear_screen;
	if (xargs.logs != UNSET)
		logs_enabled = xargs.logs;
	if (xargs.max_path != UNSET)
		max_path = xargs.max_path;
	if (xargs.expand_bookmarks != UNSET)
		expand_bookmarks = xargs.expand_bookmarks;
	if (xargs.only_dirs != UNSET)
		only_dirs = xargs.only_dirs;
	if (xargs.tips != UNSET)
		tips = xargs.tips;
	if (xargs.icons != UNSET)
		icons = xargs.icons;

	/* Free the aliases and prompt_cmds arrays to be allocated again */
	int i = dirhist_total_index;

	while (--i >= 0)
		free(old_pwd[i]);

	free(old_pwd);

	old_pwd = (char **)NULL;

	if (jump_db) {
		for (i = 0; jump_db[i].path; i++)
			free(jump_db[i].path);

		free(jump_db);
		jump_db = (struct jump_t *)NULL;
	}

	jump_n = 0;

	i = (int)aliases_n;
	while (--i >= 0)
		free(aliases[i]);

	i = (int)prompt_cmds_n;
	while (--i >= 0)
		free(prompt_cmds[i]);

	aliases_n = prompt_cmds_n = dirhist_total_index = 0;

	get_aliases();

	get_prompt_cmds();

	load_dirhist();

	load_jumpdb();

	/* Set the current poistion of the dirhist index to the last
	 * entry */
	dirhist_cur_index = dirhist_total_index - 1;

	set_env();

	return EXIT_SUCCESS;
}
