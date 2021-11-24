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
#include "file_operations.h"
#include "autocmds.h"

/* Regenerate the configuration file and create a back up of the old
 * one */
static int
regen_config(void)
{
	int config_found = 1;
	struct stat config_attrib;

	if (stat(config_file, &config_attrib) == -1) {
		puts(_("No configuration file found"));
		config_found = 0;
	}

	if (config_found) {
		time_t rawtime = time(NULL);
		struct tm t;
		localtime_r(&rawtime, &t);

		char date[18];
		strftime(date, 18, "%Y%m%d@%H:%M:%S", &t);

		char *bk = (char *)xnmalloc(strlen(config_file) + strlen(date) + 2, sizeof(char));
		sprintf(bk, "%s.%s", config_file, date);

		char *cmd[] = {"mv", config_file, bk, NULL};

		if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS) {
			free(bk);
			return EXIT_FAILURE;
		}

		printf(_("Old configuration file stored as '%s'\n"), bk);
		free(bk);
	}

	if (create_config(config_file) != EXIT_SUCCESS)
		return EXIT_FAILURE;

	printf(_("New configuration file written to '%s'\n"), config_file);
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

	if (comm[1] && *comm[1] == '-' && strcmp(comm[1], "--help") == 0) {
		printf("%s\n", EDIT_USAGE);
		return EXIT_SUCCESS;
	}

	if (comm[1] && *comm[1] == 'r' && strcmp(comm[1], "reset") == 0)
		return regen_config();

	if (!config_ok) {
		fprintf(stderr, _("%s: Cannot access the configuration file\n"),
		    PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	/* Get modification time of the config file before opening it */
	struct stat file_attrib;

	/* If, for some reason (like someone erasing the file while the
	 * program is running) clifmrc doesn't exist, recreate the
	 * configuration file. Then run 'stat' again to reread the attributes
	 * of the file */
	if (stat(config_file, &file_attrib) == -1) {
		create_config(config_file);
		stat(config_file, &file_attrib);
	}

	time_t mtime_bfr = (time_t)file_attrib.st_mtime;
	int ret = EXIT_SUCCESS;

	/* If there is an argument... */
	if (comm[1]) {
		char *cmd[] = {comm[1], config_file, NULL};
		ret = launch_execve(cmd, FOREGROUND, E_NOSTDERR);
	} else {
		/* If no application was passed as 2nd argument */
		open_in_foreground = 1;
		ret = open_file(config_file);
		open_in_foreground = 0;
	}

	if (ret != EXIT_SUCCESS)
		return EXIT_FAILURE;

	/* Get modification time after opening the config file */
	stat(config_file, &file_attrib);
	/* If modification times differ, the file was modified after being
	 * opened */

	if (mtime_bfr != (time_t)file_attrib.st_mtime) {
		/* Reload configuration only if the config file was modified */
		reload_config();
		welcome_message = 0;

		if (autols) {
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
	setenv("CLIFM", config_dir ? config_dir : "1", 1);
	setenv("CLIFM_PROFILE", alt_profile ? alt_profile : "default", 1);

	if (sel_file)
		setenv("CLIFM_SELFILE", sel_file, 1);
}

/* Define the file for the Selection Box */
void
set_sel_file(void)
{
	if (sel_file) {
		free(sel_file);
		sel_file = (char *)NULL;
	}

	if (!config_dir)
		return;

	size_t config_len = strlen(config_dir);

	if (!share_selbox) {
		/* Private selection box is stored in the profile
		 * directory */
		sel_file = (char *)xnmalloc(config_len + 9, sizeof(char));

		sprintf(sel_file, "%s/selbox", config_dir);
	} else {
		/* Common selection box is stored in the general
		 * configuration directory */
		sel_file = (char *)xnmalloc(config_len + 17, sizeof(char));
		sprintf(sel_file, "%s/.config/%s/selbox", user.home, PNL);
	}

	return;
}

int
create_kbinds_file(void)
{
	if (!config_ok)
		return EXIT_FAILURE;

	struct stat attr;
	/* If the file already exists, do nothing */
	if (stat(kbinds_file, &attr) == EXIT_SUCCESS)
		return EXIT_SUCCESS;

	/* If not, try to import it from DATADIR */
	if (data_dir) {
		char sys_file[PATH_MAX];
		snprintf(sys_file, PATH_MAX - 1, "%s/%s/keybindings.cfm", data_dir, PNL);
		if (stat(sys_file, &attr) == EXIT_SUCCESS) {
			char *cmd[] = {"cp", sys_file, kbinds_file, NULL};
			if (launch_execve(cmd, FOREGROUND, E_NOFLAG) == EXIT_SUCCESS)
				return EXIT_SUCCESS;
		}
	}

	/* Else, create it */
	FILE *fp = fopen(kbinds_file, "w");
	if (!fp) {
		_err('w', PRINT_PROMPT, "%s: '%s': %s\n", PROGRAM_NAME, kbinds_file,
		    strerror(errno));
		return EXIT_FAILURE;
	}

	fprintf(fp, "# Keybindings file for %s\n\n\
# Use the 'kbgen' plugin (compile it first: gcc -o kbgen kbgen.c) to \n\
# find out the escape code for the key or key sequence you want. Use \n\
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
# Shift-Left (rxvt)\n\
previous-dir2:\\e[d\n\
# Shift-Left (xterm)\n\
previous-dir3:\\e[2D\n\
# Shift-Left (others)\n\
previous-dir4:\\e[1;2D\n\
\n\
# Alt-k\n\
next-dir:\\M-k\n\
# Shift-right (rxvt)\n\
next-dir2:\\e[c\n\
# Shift-Right (xterm)\n\
next-dir3:\\e[2C\n\
# Shift-Right (others)\n\
next-dir4:\\e[1;2C\n\
first-dir:\\C-\\M-j\n\
last-dir:\\C-\\M-k\n\
\n\
# Alt-u\n\
parent-dir:\\M-u\n\
# Shift-Up (rxvt)\n\
parent-dir2:\\e[a\n\
# Shift-Up (xterm)\n\
parent-dir3:\\e[2A\n\
# Shift-Up (others)\n\
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
workspace1:\\M-1\n\
workspace2:\\M-2\n\
workspace3:\\M-3\n\
workspace4:\\M-4\n\
\n\
# Help\n\
# F1-3\n\
show-manpage:\\eOP\n\
show-cmds:\\eOQ\n\
show-kbinds:\\eOR\n\
\n\
prepend-sudo:\\M-v\n\
create-file:\\M-n\n\
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
folders-first:\\M-g\n\
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
# \"plugin1=myplugin.sh\"\n\
# 3) Set a keybinding here for pluginx. Ex: \"plugin1:\\M-7\"\n\n\
#plugin1:\n\
#plugin2:\n\
#plugin3:\n\
#plugin4:\n",
	    PROGRAM_NAME);

	fclose(fp);
	return EXIT_SUCCESS;
}

static int
import_from_data_dir(char *src_filename, char *dest)
{
	if (!data_dir || !src_filename || !dest)
		return EXIT_FAILURE;

	if (!*data_dir || !*src_filename || !*dest)
		return EXIT_FAILURE;

	struct stat attr;
	char sys_file[PATH_MAX];
	snprintf(sys_file, PATH_MAX - 1, "%s/%s/%s", data_dir, PNL, src_filename);
	if (stat(sys_file, &attr) == EXIT_SUCCESS) {
		char *cmd[] = {"cp", sys_file, dest, NULL};
		if (launch_execve(cmd, FOREGROUND, E_NOFLAG) == EXIT_SUCCESS)
			return EXIT_SUCCESS;
	}

	return EXIT_FAILURE;
}

static int
create_actions_file(char *file)
{
	struct stat attr;
	/* If the file already exists, do nothing */
	if (stat(file, &attr) == EXIT_SUCCESS)
		return EXIT_SUCCESS;

	/* If not, try to import it from DATADIR */
	if (import_from_data_dir("actions.cfm", file) == EXIT_SUCCESS)
		return EXIT_SUCCESS;

	/* Else, create it */
	int fd;
	FILE *fp = open_fstream_w(file, &fd);
	if (!fp) {
		_err('e', PRINT_PROMPT, "%s: '%s': %s\n", PROGRAM_NAME,
		    file, strerror(errno));
		return EXIT_FAILURE;
	}

	fprintf(fp, "######################\n"
		"# Actions file for %s #\n"
		"######################\n\n"
		"# Define here your custom actions. Actions are "
		"custom command names\n"
		"# bound to a executable file located either in "
		"DATADIR/clifm/plugins\n"
		"# (usually /usr/share/clifm/plugins) or in "
		"$XDG_CONFIG_HOME/clifm/plugins.\n"
		"# Actions can be executed directly from "
		"%s command line, as if they\n"
		"# were any other command, and the associated "
		"file will be executed\n"
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
		"dh=fzfdirhist.sh\n"
		"//=rgfind.sh\n"
		"ih=ihelp.sh\n",
	    PROGRAM_NAME, PROGRAM_NAME);

	close_fstream(fp, fd);
	return EXIT_SUCCESS;
}

void
create_tmp_files(void)
{
	if (xargs.stealth_mode == 1)
		return;

	if (!user.name)
		return;

	size_t pnl_len = strlen(PNL);

	/* #### CHECK THE TMP DIR #### */

	/* If the temporary directory doesn't exist, create it. I create
	 * the parent directory (/tmp/clifm) with 1777 permissions (world
	 * writable with the sticky bit set), so that every user is able
	 * to create files in here, but only the file's owner can remove
	 * or modify them */
	size_t user_len = strlen(user.name);
	tmp_dir = (char *)xnmalloc(P_tmpdir_len + pnl_len + user_len + 3, sizeof(char));
	sprintf(tmp_dir, "%s/%s", P_tmpdir, PNL);
	/* P_tmpdir is defined in stdio.h and it's value is usually /tmp */

	int tmp_root_ok = 1;
	struct stat attr;
	/* Create /tmp */
	if (stat(P_tmpdir, &attr) == -1)
		if (xmkdir(P_tmpdir, S_IRWXU | S_IRWXG | S_IRWXO | S_ISVTX) == EXIT_FAILURE)
			tmp_root_ok = 0;
	/* Create /tmp/clifm */
	if (stat(tmp_dir, &attr) == -1)
		xmkdir(tmp_dir, S_IRWXU | S_IRWXG | S_IRWXO | S_ISVTX);

	/* Once the parent directory exists, create the user's directory to
	 * store the list of selected files:
	 * TMP_DIR/clifm/username/.selbox_PROFILE. I use here very
	 * restrictive permissions (700), since only the corresponding user
	 * must be able to read and/or modify this list */
	sprintf(tmp_dir, "%s/%s/%s", P_tmpdir, PNL, user.name);
	if (stat(tmp_dir, &attr) == -1) {
		if (xmkdir(tmp_dir, S_IRWXU) == EXIT_FAILURE) {
			selfile_ok = 0;
			_err('e', PRINT_PROMPT, _("%s: %s: %s\n"), PROGRAM_NAME,
				tmp_dir, strerror(errno));
		}
	}

	/* If the directory exists, check it is writable */
	else if (access(tmp_dir, W_OK) == -1) {
		if (!sel_file) {
			selfile_ok = 0;
			_err('w', PRINT_PROMPT, "%s: %s: Directory not writable. Selected "
				"files will be lost after program exit\n",
			    PROGRAM_NAME, tmp_dir);
		}
	}

	/* sel_file should has been set before by set_sel_file(). If not set,
	 * we do not have access to the config dir */
	if (sel_file)
		return;

	/*"We will write a temporary selfile in /tmp. Check if this latter is
	 * available */
	if (!tmp_root_ok) {
		_err('w', PRINT_PROMPT, "%s: Could not create the selections file.\n"
			"Selected files will be lost after program exit\n",
		    PROGRAM_NAME, tmp_dir);
		return;
	}

	/* If the config directory isn't available, define an alternative
	 * selection file in /tmp (if available) */
	if (!share_selbox) {
		size_t prof_len = 0;

		if (alt_profile)
			prof_len = strlen(alt_profile);
		else
			prof_len = 7; /* Lenght of "default" */

		sel_file = (char *)xnmalloc(P_tmpdir_len + prof_len + 9,
		    sizeof(char));
		sprintf(sel_file, "%s/selbox_%s", P_tmpdir,
		    (alt_profile) ? alt_profile : "default");
	} else {
		sel_file = (char *)xnmalloc(P_tmpdir_len + 8, sizeof(char));
		sprintf(sel_file, "%s/selbox", P_tmpdir);
	}

	_err('w', PRINT_PROMPT, _("%s: '%s': Using a temporary directory for "
		"the Selection Box. Selected files won't be persistent across "
		"reboots"), PROGRAM_NAME, tmp_dir);
}

static void
define_config_file_names(void)
{
	size_t pnl_len = strlen(PNL);

	if (alt_config_dir) {
		config_dir_gral = (char *)xnmalloc(strlen(alt_config_dir) + pnl_len
											+ 2, sizeof(char));
		sprintf(config_dir_gral, "%s/%s", alt_config_dir, PNL);
		free(alt_config_dir);
	} else {
		/* If $XDG_CONFIG_HOME is set, use it for the config file.
		 * Else, fall back to $HOME/.config */
		char *xdg_config_home = getenv("XDG_CONFIG_HOME");
		if (xdg_config_home) {
			size_t xdg_config_home_len = strlen(xdg_config_home);
			config_dir_gral = (char *)xnmalloc(xdg_config_home_len + pnl_len
												+ 2, sizeof(char));
			sprintf(config_dir_gral, "%s/%s", xdg_config_home, PNL);
			xdg_config_home = (char *)NULL;
		} else {
			config_dir_gral = (char *)xnmalloc(user.home_len + pnl_len + 11,
												sizeof(char));
			sprintf(config_dir_gral, "%s/.config/%s", user.home, PNL);
		}
	}

	size_t config_gral_len = strlen(config_dir_gral);

	/* alt_profile will not be NULL whenever the -P option is used
	 * to run the program under an alternative profile */
	if (alt_profile) {
		config_dir = (char *)xnmalloc(config_gral_len + strlen(alt_profile) + 11, sizeof(char));
		sprintf(config_dir, "%s/profiles/%s", config_dir_gral, alt_profile);
	} else {
		config_dir = (char *)xnmalloc(config_gral_len + 18, sizeof(char));
		sprintf(config_dir, "%s/profiles/default", config_dir_gral);
	}

	if (alt_kbinds_file) {
		kbinds_file = savestring(alt_kbinds_file, strlen(alt_kbinds_file));
		free(alt_kbinds_file);
		alt_kbinds_file = (char *)NULL;
	} else {
		/* Keybindings per user, not per profile */
		kbinds_file = (char *)xnmalloc(config_gral_len + 17, sizeof(char));
		sprintf(kbinds_file, "%s/keybindings.cfm", config_dir_gral);
	}

	colors_dir = (char *)xnmalloc(config_gral_len + 8, sizeof(char));
	sprintf(colors_dir, "%s/colors", config_dir_gral);

	plugins_dir = (char *)xnmalloc(config_gral_len + 9, sizeof(char));
	sprintf(plugins_dir, "%s/plugins", config_dir_gral);

#ifndef _NO_TRASH
	trash_dir = (char *)xnmalloc(user.home_len + 20, sizeof(char));
	sprintf(trash_dir, "%s/.local/share/Trash", user.home);

	size_t trash_len = strlen(trash_dir);

	trash_files_dir = (char *)xnmalloc(trash_len + 7, sizeof(char));
	sprintf(trash_files_dir, "%s/files", trash_dir);

	trash_info_dir = (char *)xnmalloc(trash_len + 6, sizeof(char));
	sprintf(trash_info_dir, "%s/info", trash_dir);
#endif

	size_t config_len = strlen(config_dir);

	dirhist_file = (char *)xnmalloc(config_len + 13, sizeof(char));
	sprintf(dirhist_file, "%s/dirhist.cfm", config_dir);

	if (!alt_bm_file) {
		bm_file = (char *)xnmalloc(config_len + 15, sizeof(char));
		sprintf(bm_file, "%s/bookmarks.cfm", config_dir);
	} else {
		bm_file = savestring(alt_bm_file, strlen(alt_bm_file));
		free(alt_bm_file);
		alt_bm_file = (char *)NULL;
	}

	log_file = (char *)xnmalloc(config_len + 9, sizeof(char));
	sprintf(log_file, "%s/log.cfm", config_dir);

	hist_file = (char *)xnmalloc(config_len + 13, sizeof(char));
	sprintf(hist_file, "%s/history.cfm", config_dir);

	if (!alt_config_file) {
		config_file = (char *)xnmalloc(config_len + pnl_len + 4, sizeof(char));
		sprintf(config_file, "%s/%src", config_dir, PNL);
	} else {
		config_file = savestring(alt_config_file, strlen(alt_config_file));
		free(alt_config_file);
		alt_config_file = (char *)NULL;
	}

	profile_file = (char *)xnmalloc(config_len + 13, sizeof(char));
	sprintf(profile_file, "%s/profile.cfm", config_dir);

	msg_log_file = (char *)xnmalloc(config_len + 14, sizeof(char));
	sprintf(msg_log_file, "%s/messages.cfm", config_dir);

	mime_file = (char *)xnmalloc(config_len + 14, sizeof(char));
	sprintf(mime_file, "%s/mimelist.cfm", config_dir);

	actions_file = (char *)xnmalloc(config_len + 13, sizeof(char));
	sprintf(actions_file, "%s/actions.cfm", config_dir);

	remotes_file = (char *)xnmalloc(config_len + 10, sizeof(char));
	sprintf(remotes_file, "%s/nets.cfm", config_dir);

	return;
}

static int
import_rl_file(void)
{
	if (!data_dir || !config_dir_gral)
		return EXIT_FAILURE;

	char tmp[PATH_MAX];
	sprintf(tmp, "%s/readline.cfm", config_dir_gral);
	struct stat attr;
	if (lstat(tmp, &attr) == 0)
		return EXIT_SUCCESS;

	char rl_file[PATH_MAX];
	snprintf(rl_file, PATH_MAX - 1, "%s/%s/readline.cfm", data_dir, PNL);
	if (stat(rl_file, &attr) == EXIT_SUCCESS) {
		char *cmd[] = {"cp", rl_file, config_dir_gral, NULL};
		if (launch_execve(cmd, FOREGROUND, E_NOSTDERR) == EXIT_SUCCESS)
			return EXIT_SUCCESS;
	}

	return EXIT_FAILURE;
}

int
create_config(char *file)
{
//	struct stat attr;

	/* First, try to import it from DATADIR */
	char src_filename[NAME_MAX];
	snprintf(src_filename, NAME_MAX, "%src", PNL);
	if (import_from_data_dir(src_filename, file) == EXIT_SUCCESS)
		return EXIT_SUCCESS;
/*	if (data_dir) {
		char sys_file[PATH_MAX];
		snprintf(sys_file, PATH_MAX - 1, "%s/%s/%src", data_dir, PNL, PNL);
		if (stat(sys_file, &attr) == EXIT_SUCCESS) {
			char *cmd[] = {"cp", sys_file, file, NULL};
			if (launch_execve(cmd, FOREGROUND, E_NOFLAG) == EXIT_SUCCESS)
				return EXIT_SUCCESS;
		}
	} */

	/* If not found, create it */
	int fd;
	FILE *config_fp = open_fstream_w(file, &fd);

	if (!config_fp) {
		fprintf(stderr, "%s: fopen: %s: %s\n", PROGRAM_NAME, file, strerror(errno));
		return EXIT_FAILURE;
	}

	/* Do not translate anything in the config file */
	fprintf(config_fp,

"\t\t###########################################\n\
\t\t#                  CLIFM                  #\n\
\t\t#      The command line file manager      #\n\
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

		"# The character(s) used to construct the line dividing the list of files and\n\
# the prompt. if '0', print just an empty line; if only one char, this char\n\
# is printed reapeatedly to fulfill the screen; if 3 or more chars, only these\n\
# chars (no more) will be printed. Finally, if unset, print a special line\n\
# drawn with bow-drawing characters (not supported by all terminals/consoles)\n\
DividingLineChar='%c'\n\n"

		"# How to list files: 0 = vertically (like ls(1) would), 1 = horizontally\n\
ListingMode=%d\n\n"

		"# List files automatically after changing current directory\n\
AutoLs=%s\n\n"

	    "# If set to true, print a map of the current position in the directory\n\
# history list, showing previous, current, and next entries\n\
DirhistMap=%s\n\n"

		"# Use a regex expression to filter file names when listing files.\n\
# Example: !.*~$ to exclude backup files (ending with ~), or ^\\. to list \n\
# only hidden files. Do not quote the regular expression\n\
Filter=\n\n"

	    "# Set the default copy command. Available options are: 0 = cp,\n\
# 1 = advcp, and 2 = wcp. Both 1 and 2 add a progress bar to cp.\n\
cpCmd=%d\n\n"

	    "# Set the default move command. Available options are: 0 = mv,\n\
# and 1 = advmv. 1 adds a progress bar to mv.\n\
mvCmd=%d\n\n"

	    "# The prompt line is built using string literals and/or one or more of\n\
# the following escape sequences:\n"
	    "# \\e: Escape character\n\
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

	    "Prompt=\"%s\"\n\n"

	    "# If set to 'default', CliFM state information (selected files,\n\
# trashed files, current workspace, messages, and stealth mode) will be printed\n\
# to the left of the prompt. Otherwise, if set to 'custom', this information\n\
# will be stored in environment variables to be handled by the prompt string\n\
# itself. Consult the manpage for more information.\n\
PromptStyle=default\n\n"

		"# A prompt to warn the user about invalid command names\n\
WarningPrompt=%s\n\n"

		"# String to be used by the warning prompt. The color of this prompt\n\
# can be customized using the 'wp' code in the color scheme file\n\
WarningPromptStr=\"%s\"\n\n"

		"# Should we add padding to ELN's. Possible values: 0:none, 1=zeroes\n\
# 2=spaces on the left, and 3=spaces on the right\n\
ELNPad=%d\n\n",

	    COLORS_REPO,
		DEF_COLOR_SCHEME,
		DEF_FILES_COUNTER == 1 ? "true" : "false",
		DEF_DIV_LINE_CHAR,
		DEF_LISTING_MODE,
		DEF_AUTOLS == 1 ? "true" : "false",
		DEF_DIRHIST_MAP == 1 ? "true" : "false",
		DEF_CP_CMD,
		DEF_MV_CMD,
	    DEFAULT_PROMPT,
		DEF_WARN_WRONG_CMD == 1 ? "true" : "false",
	    DEF_WPROMPT_STR,
	    DEF_ELNPAD);

	fprintf(config_fp,

		"# TAB completion mode: either 'standard' (default) or 'fzf'\n\
TabCompletionMode=%s\n\n"

		"# If FZF TAB completion mode is enabled, pass these options to fzf.\n\
# --height, --margin, +i/-i, and --query will be appended to set up the \n\
# completions interface.\n\
FzfTabOptions=%s\n\n"

	    "# MaxPath is only used for the /p option of the prompt: the current working\n\
# directory will be abbreviated to its basename (everything after last slash)\n\
# whenever the current path is longer than MaxPath.\n\
MaxPath=%d\n\n"

	    "WelcomeMessage=%s\n\n\
# Print %s's logo screen at startup\n\
SplashScreen=%s\n\n\
ShowHiddenFiles=%s\n\n\
# List files properties next to file names instead of just file names\n\
LongViewMode=%s\n\n\
# Keep a record of both external commands and internal commands able to\n\
# modify the files system (e.g. 'r', 'c', 'm', and so on)\n\
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

	    "# If set to true, enable auto-suggestions.\n\
AutoSuggestions=%s\n\n"

	    "# The following checks will be performed in the order specified\n\
# by SuggestionStrategy. Available checks:\n\
# a = Aliases names\n\
# b = Bookmarks names\n\
# c = Possible completions\n\
# e = ELN's\n\
# f = File names in current directory\n\
# h = Commands history\n\
# j = Jump database\n\
# Use a dash (-) to skip a check. Ex: 'eahfj-c' to skip the bookmarks check\n\
SuggestionStrategy=%s\n\n"

	    "# If set to true, suggest file names using the corresponding\n\
# file type color (set via the color scheme file).\n\
SuggestFiletypeColor=%s\n\n"

"SyntaxHighlighting=%s\n\n"

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

		DEF_FZFTAB == 1 ? "fzf" : "standard",
		DEF_FZFTAB_OPTIONS,
		DEF_MAX_PATH,
		DEF_WELCOME_MESSAGE == 1 ? "true" : "false",
		PROGRAM_NAME,
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
		DEF_SUGGESTIONS == 1 ? "true" : "false",
		DEF_SUG_STRATEGY,
		DEF_SUG_FILETYPE_COLOR == 1 ? "true" : "false",
		DEF_HIGHLIGHT == 1 ? "true" : "false",
		DEF_EXPAND_BOOKMARKS == 1 ? "true" : "false",
		DEF_LIGHT_MODE == 1 ? "true" : "false"
		);

	fprintf(config_fp,
	    "# If running with colors, append directory indicator\n\
# to directories. If running without colors (via the --no-colors option),\n\
# append file type indicator at the end of file names: '/' for directories,\n\
# '@' for symbolic links, '=' for sockets, '|' for FIFO/pipes, '*'\n\
# for for executable files, and '?' for unknown file types. Bear in mind\n\
# that when running in light mode the check for executable files won't be\n\
# performed, and thereby no indicator will be added to executable files.\n\
Classify=%s\n\n"

	    "# Should the Selection Box be shared among different profiles?\n\
ShareSelbox=%s\n\n"

	    "# Choose the resource opener to open files with their default associated\n\
# application. If not set, 'lira', CLiFM's built-in opener, is used.\n\
Opener=\n\n"

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

	    "# Print a usage tip at startup\n\
Tips=%s\n\n\
ListFoldersFirst=%s\n\n\
# Enable case sensitive listing for files in the current directory\n\
CaseSensitiveList=%s\n\n\
# Enable case sensitive lookup for the directory jumper function (via \n\
# the 'j' command)\n\
CaseSensitiveDirJump=%s\n\n\
# Enable case sensitive completion for file names\n\
CaseSensitivePathComp=%s\n\n\
# Enable case sensitive search\n\
CaseSensitiveSearch=%s\n\n\
Unicode=%s\n\n\
# Enable Mas, the files list pager (executed whenever the list of files\n\
# does not fit in the screen)\n\
Pager=%s\n\n"

	"# Maximum file name length for listed files. Names larger than\n\
# MAXFILENAMELEN will be truncated at MAXFILENAMELEN using a tilde\n\
MaxFilenameLen=\n\n"

	"MaxHistory=%d\n\
MaxDirhist=%d\n\
MaxLog=%d\n\
Icons=%s\n\
DiskUsage=%s\n\n"

		"# If set to true, always print the list of selected files. Since this\n\
# list could become quite extensive, you can limit the number of printed \n\
# entries using the MaxPrintSelfiles option (-1 = no limit, 0 = auto (never\n\
# print more than half terminal height), or any custom value)\n\
PrintSelfiles=%s\n\
MaxPrintSelfiles=%d\n\n"

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
RlEditMode=%d\n\n",

		DEF_CLASSIFY == 1 ? "true" : "false",
		DEF_SHARE_SELBOX == 1 ? "true" : "false",
		DEFAULT_TERM_CMD,
		DEF_SORT,
		DEF_SORT_REVERSE == 1 ? "true" : "false",
		DEF_TIPS == 1 ? "true" : "false",
		DEF_LIST_FOLDERS_FIRST == 1 ? "true" : "false",
		DEF_CASE_SENS_LIST == 1 ? "true" : "false",
		DEF_CASE_SENS_DIRJUMP == 1 ? "true" : "false",
		DEF_CASE_SENS_PATH_COMP == 1 ? "true" : "false",
		DEF_CASE_SENS_SEARCH == 1 ? "true" : "false",
		DEF_UNICODE == 1 ? "true" : "false",
		DEF_PAGER == 1 ? "true" : "false",
		DEF_MAX_HIST,
		DEF_MAX_DIRHIST,
		DEF_MAX_LOG,
		DEF_ICONS == 1 ? "true" : "false",
		DEF_DISK_USAGE == 1 ? "true" : "false",
		DEF_PRINTSEL == 1 ? "true" : "false",
		DEF_MAXPRINTSEL,
		DEF_CLEAR_SCREEN == 1 ? "true" : "false",
		DEF_RESTORE_LAST_PATH == 1 ? "true" : "false",
		DEF_TRASRM == 1 ? "true" : "false",
		DEF_RL_EDIT_MODE
		);

	fputs(

	    "# ALIASES\n\
#alias ls='ls --color=auto -A'\n\n"

	    "# PROMPT COMMANDS\n\
# Write below the commands you want to be executed before the prompt. Ex:\n\
#promptcmd /usr/share/clifm/plugins/git_status.sh\n\
#promptcmd date | awk '{print $1\", \"$2,$3\", \"$4}'\n\n"

		"# AUTOCOMMANDS\n\
# Control CliFM settings on a per directory basis. For more information\n\
# consult the manpage\n\
#autocmd /media/remotes/** lm=1,fc=0\n\
#autocmd ~/important !printf \"Keep your fingers outta here!\n\" && read -n1\n\
#autocmd ~/Downloads !/usr/share/clifm/plugins/fzfnav.sh\n\n",
	    config_fp);

	close_fstream(config_fp, fd);
	return EXIT_SUCCESS;
}

static void
create_def_cscheme(void)
{
	if (!colors_dir)
		return;

	char *cscheme_file = (char *)xnmalloc(strlen(colors_dir) + 13, sizeof(char));
	sprintf(cscheme_file, "%s/default.cfm", colors_dir);

	/* If the file already exists, do nothing */
	struct stat attr;
	if (stat(cscheme_file, &attr) != -1) {
		free(cscheme_file);
		return;
	}

	int fd;
	FILE *fp = open_fstream_w(cscheme_file, &fd);
	if (!fp) {
		_err('w', PRINT_PROMPT, "%s: Error creating default color scheme "
			"file: %s\n", PROGRAM_NAME, strerror(errno));
		free(cscheme_file);
		return;
	}

	fprintf(fp, "# Default color scheme for %s\n\n\
# FiletypeColors defines the color used for file types when listing files,\n\
# just as InterfaceColors defines colors for CliFM's interface and ExtColors\n\
# for file extensions. They all make use of the same format used by the\n\
# LS_COLORS environment variable. Thus, \"di=01;34\" means that (non-empty)\n\
# directories will be listed in bold blue.\n\
# Color codes are traditional ANSI escape sequences less the escape char and\n\
# the final 'm'. 8 bit, 256 colors, and RGB colors are supported.\n\
# A detailed explanation of all these codes can be found in the manpage.\n\n"

		    "FiletypeColors=\"%s\"\n\n"

		    "InterfaceColors=\"%s\"\n\n"

		    "# Same as FiletypeColors, but for file extensions. The format is always\n\
# *.EXT=COLOR\n"
#ifndef _NO_ICONS
		    "ExtColors=\"%s\"\n\n"

		    "DirIconsColor=\"00;33\"\n",
#else
		    "ExtColors=\"%s\"\n\n",
#endif
		PROGRAM_NAME,
	    DEF_FILE_COLORS,
	    DEF_IFACE_COLORS,
	    DEF_EXT_COLORS);

	close_fstream(fp, fd);
	free(cscheme_file);
	return;
}

static int
create_remotes_file(void)
{
	if (!remotes_file || !*remotes_file)
		return EXIT_FAILURE;

	struct stat attr;
	if (stat(remotes_file, &attr) == EXIT_SUCCESS)
		return EXIT_SUCCESS;

	int fd;
	FILE *fp = open_fstream_w(remotes_file, &fd);
	if (!fp) {
		_err('e', PRINT_PROMPT, "%s: '%s': %s\n", PROGRAM_NAME,
		    remotes_file, strerror(errno));
		return EXIT_FAILURE;
	}

	fprintf(fp, "#####################################\n"
		"# Remotes management file for %s #\n"
		"#####################################\n\n"
		"# Blank and commented lines are omitted\n\n"
		"# Example:\n"
		"# A name for this remote. It will be used by the 'net' command\n"
		"# and will be available for TAB completion\n"
		"# [work_smb]\n\n"
		"# Comment=My work samba server\n"
		"# Mountpoint=/home/user/.config/clifm/mounts/work_smb\n\n"
		"# Use %%m as a placeholder for Mountpoint\n"
		"# MountCmd=mount.cifs //WORK_IP/shared %%m -o OPTIONS\n"
		"# UnmountCmd=umount %%m\n\n"
		"# Automatically mount this remote at startup\n"
		"# AutoMount=true\n\n"
		"# Automatically unmount this remote at exit\n"
		"# AutoUnmount=true\n\n", PROGRAM_NAME);

	close_fstream(fp, fd);
	return EXIT_SUCCESS;
}

static void
create_config_files(void)
{
	struct stat attr;

			/* #############################
			 * #        TRASH DIRS         #
			 * ############################# */
#ifndef _NO_TRASH
	if (stat(trash_dir, &attr) == -1) {
		char *trash_files = (char *)NULL;
		trash_files = (char *)xnmalloc(strlen(trash_dir) + 7, sizeof(char));

		sprintf(trash_files, "%s/files", trash_dir);
		char *trash_info = (char *)NULL;
		trash_info = (char *)xnmalloc(strlen(trash_dir) + 6, sizeof(char));

		sprintf(trash_info, "%s/info", trash_dir);
		char *cmd[] = {"mkdir", "-p", trash_files, trash_info, NULL};

		int ret = launch_execve(cmd, FOREGROUND, E_NOFLAG);
		free(trash_files);
		free(trash_info);

		if (ret != EXIT_SUCCESS) {
			trash_ok = 0;
			_err('w', PRINT_PROMPT, _("%s: mkdir: '%s': Error creating trash "
				"directory. Trash function disabled\n"), PROGRAM_NAME, trash_dir);
		}
	}

	/* If it exists, check it is writable */
	else if (access(trash_dir, W_OK) == -1) {
		trash_ok = 0;
		_err('w', PRINT_PROMPT, _("%s: '%s': Directory not writable. "
				"Trash function disabled\n"), PROGRAM_NAME, trash_dir);
	}
#endif
				/* ####################
				 * #    CONFIG DIR    #
				 * #################### */

	/* If the config directory doesn't exist, create it */
	/* Use the mkdir(1) to let it handle parent directories */
	if (stat(config_dir, &attr) == -1) {
		char *tmp_cmd[] = {"mkdir", "-p", config_dir, NULL};
		if (launch_execve(tmp_cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS) {
			config_ok = 0;
			_err('e', PRINT_PROMPT, _("%s: mkdir: '%s': Error creating "
				"configuration directory. Bookmarks, commands logs, and "
				"command history are disabled. Program messages won't be "
				"persistent. Using default options\n"),
			    PROGRAM_NAME, config_dir);
			return;
		}
	}

	/* If it exists, check it is writable */
	else if (access(config_dir, W_OK) == -1) {
		config_ok = 0;
		_err('e', PRINT_PROMPT, _("%s: '%s': Directory not writable. Bookmarks, "
			"commands logs, and commands history are disabled. Program messages "
			"won't be persistent. Using default options\n"),
		    PROGRAM_NAME, config_dir);
		return;
	}

				/* #####################
				 * #    CONFIG FILE    #
				 * #####################*/

	if (stat(config_file, &attr) == -1) {
		if (create_config(config_file) == EXIT_SUCCESS)
			config_ok = 1;
		else
			config_ok = 0;
	}

	if (!config_ok)
		return;

				/* ######################
				 * #    PROFILE FILE    #
				 * ###################### */

	if (stat(profile_file, &attr) == -1) {
		FILE *profile_fp = fopen(profile_file, "w");
		if (!profile_fp) {
			_err('e', PRINT_PROMPT, "%s: fopen: '%s': %s\n", PROGRAM_NAME,
			    profile_file, strerror(errno));
		} else {
			fprintf(profile_fp, _("#%s profile\n\
# Write here the commands you want to be executed at startup\n\
# Ex:\n#echo -e \"%s, the anti-eye-candy/KISS file manager\"\n"),
			    PROGRAM_NAME, PROGRAM_NAME);
			fclose(profile_fp);
		}
	}

				/* #####################
				 * #    COLORS DIR     #
				 * ##################### */

	if (stat(colors_dir, &attr) == -1) {
/*		char *cmd[] = {"mkdir", colors_dir, NULL};
		if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS) { */
		if (xmkdir(colors_dir, S_IRWXU) == EXIT_FAILURE) {
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

	if (stat(plugins_dir, &attr) == -1) {
/*		char *cmd[] = {"mkdir", plugins_dir, NULL};
		if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS) { */
		if (xmkdir(plugins_dir, S_IRWXU) == EXIT_FAILURE) {
			_err('e', PRINT_PROMPT, _("%s: mkdir: Error creating plugins "
				"directory. The actions function is disabled\n"),
			    PROGRAM_NAME);
		}
	}

	import_rl_file();
	create_actions_file(actions_file);
	create_mime_file(mime_file, 0);
	create_remotes_file();
}

int
create_mime_file(char *file, int new_prof)
{
	struct stat attr;
	if (stat(file, &attr) == EXIT_SUCCESS)
		return EXIT_SUCCESS;

	if (!data_dir)
		return EXIT_FAILURE;

	char sys_mimelist[PATH_MAX];
	snprintf(sys_mimelist, PATH_MAX - 1, "%s/%s/mimelist.cfm", data_dir, PNL);

	if (stat(sys_mimelist, &attr) == -1) {
		_err('e', PRINT_PROMPT, "%s: %s: %s\n", PROGRAM_NAME,
			sys_mimelist, strerror(errno));
		return EXIT_FAILURE;
	}

	char *cmd[] = {"cp", "-f", sys_mimelist, file, NULL};
	if (launch_execve(cmd, FOREGROUND, E_NOFLAG) == EXIT_SUCCESS) {
		if (!new_prof) {
			_err('n', PRINT_PROMPT, _("%s created a new MIME list file (%s) "
				"It is recommended to edit this file (entering 'mm edit' or "
				"pressing F6) to add the programs you use and remove those "
				"you don't. This will make the process of opening files "
				"faster and smoother\n"),
				PROGRAM_NAME, file, sys_mimelist);
		}
		return EXIT_SUCCESS;
	}

	return EXIT_FAILURE;
}

int
create_bm_file(void)
{
	if (!bm_file)
		return EXIT_FAILURE;

	struct stat file_attrib;
	if (stat(bm_file, &file_attrib) == -1) {
		FILE *fp = fopen(bm_file, "w+");
		if (!fp) {
			_err('e', PRINT_PROMPT, "bookmarks: '%s': %s\n", bm_file,
			    strerror(errno));
			return EXIT_FAILURE;
		} else {
			fprintf(fp, "### This is the bookmarks file for %s ###\n\n"
				    "# Empty and commented lines are ommited\n"
				    "# The bookmarks syntax is: [shortcut]name:path\n"
				    "# Example:\n"
				    "[c]clifm:%s\n",
			    PROGRAM_NAME, config_dir ? config_dir : "path/to/file");
			fclose(fp);
		}
	}

	return EXIT_SUCCESS;
}

static char *
get_line_value(char *line)
{
	char *opt = strchr(line, '=');
	if (!opt || !*opt || !*(++opt) )
		return (char *)NULL;

	return remove_quotes(opt);
}

static void
read_config(void)
{
	int ret = -1;

	int fd;
	FILE *config_fp = open_fstream_r(config_file, &fd);
	if (!config_fp) {
		_err('e', PRINT_PROMPT, _("%s: fopen: '%s': %s. Using default values.\n"),
		    PROGRAM_NAME, config_file, strerror(errno));
		return;
	}

	if (xargs.rl_vi_mode == 1)
		rl_vi_editing_mode(1, 0);

	max_name_len = UNSET;

	*div_line_char = '\0';
#define MAX_BOOL 6 /* false (5) + 1 */
	/* starting path(14) + PATH_MAX + \n(1)*/
	char line[PATH_MAX + 15];

	while (fgets(line, (int)sizeof(line), config_fp)) {

		if (*line == '\n' || *line == '#')
			continue;
/* 		if (*line == '\n' || (*line == '#' && line[1] != 'E'))
			continue;
		if (*line == '#' && strncmp(line, "#END OF OPTIONS", 15) == 0)
			break; */

		else if (*line == 'a' && strncmp(line, "autocmd ", 8) == 0)
			parse_autocmd_line(line + 8);

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

		else if (xargs.autojump == UNSET && *line == 'A'
		&& strncmp(line, "AutoJump=", 9) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "AutoJump=%5s\n", opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				autojump = autocd = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				autojump = 0;
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

#ifndef _NO_SUGGESTIONS
		else if (xargs.suggestions == UNSET && *line == 'A'
		&& strncmp(line, "AutoSuggestions=", 16) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "AutoSuggestions=%5s\n", opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				suggestions = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				suggestions = 0;
		}
#endif

		else if (xargs.case_sens_dirjump == UNSET && *line == 'C'
		&& strncmp(line, "CaseSensitiveDirJump=", 21) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "CaseSensitiveDirJump=%5s\n", opt_str);

			if (ret == -1)
				continue;

			if (strncmp(opt_str, "true", 4) == 0)
				case_sens_dirjump = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				case_sens_dirjump = 0;
		}

		else if (*line == 'C'
		&& strncmp(line, "CaseSensitiveSearch=", 20) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "CaseSensitiveSearch=%5s\n", opt_str);

			if (ret == -1)
				continue;

			if (strncmp(opt_str, "true", 4) == 0)
				case_sens_search = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				case_sens_search = 0;
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

		else if (xargs.case_sens_path_comp == UNSET && *line == 'C'
		&& strncmp(line, "CaseSensitivePathComp=", 22) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "CaseSensitivePathComp=%5s\n", opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				case_sens_path_comp = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				case_sens_path_comp = 0;
		}

		else if (xargs.autols == UNSET && *line == 'A'
		&& strncmp(line, "AutoLs=", 7) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "AutoLs=%5s\n",
			    opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				autols = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				autols = 0;
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

		else if (!usr_cscheme && *line == 'C' && strncmp(line, "ColorScheme=", 12) == 0) {
			char *opt = strchr(line, '=');
			if (!opt)
				continue;

			size_t len = strlen(opt);
			if (opt[len - 1] == '\n')
				opt[len - 1] = '\0';

			if (!*(++opt))
				continue;

			usr_cscheme = savestring(opt, len);
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

		else if (*line == 'D' && strncmp(line, "DividingLineChar=", 17) == 0) {
			char *opt = strchr(line, '=');
			if (!opt || !*opt || !*(++opt)) {
				div_line_char[0] = '\0';
			} else {
				char *tmp = remove_quotes(opt);
				if (tmp)
					xstrsncpy(div_line_char, tmp, NAME_MAX);
				else
					div_line_char[0] = '\0';
			}
		}

		else if (*line == 'E' && strncmp(line, "ELNPad=", 7) == 0) {
			char *opt = strchr(line, '=');
			if (!opt || !*opt || !*(++opt)) {
				elnpad = DEF_ELNPAD;
			} else {
				int ivalue = atoi(opt);
				switch(ivalue) {
				case NOPAD: elnpad = NOPAD; break;
				case ZEROPAD: elnpad = ZEROPAD; break;
				case LEFTSPACEPAD: elnpad = LEFTSPACEPAD; break;
				case RIGHTSPACEPAD: elnpad = RIGHTSPACEPAD; break;
				default: elnpad = DEF_ELNPAD;
				}
			}
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

		else if (!_filter && *line == 'F' && strncmp(line, "Filter=", 7) == 0) {
			char *opt_str = strchr(line, '=');
			if (!opt_str)
				continue;
			size_t len = strlen(opt_str);
			if (opt_str[len - 1] == '\n')
				opt_str[len - 1] = '\0';

			if (!*(++opt_str))
				continue;

			if (*opt_str == '!') {
				filter_rev = 1;
				opt_str++;
				len--;
			} else {
				filter_rev = 0;
			}

			_filter = savestring(opt_str, len);
		}

#ifndef _NO_HIGHLIGHT
		else if (xargs.highlight == UNSET && *line == 'S'
		&& strncmp(line, "SyntaxHighlighting=", 19) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "SyntaxHighlighting=%5s\n", opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				highlight = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				highlight = 0;
		}
#endif /* !_NO_HIGHLIGHT */

#ifndef _NO_ICONS
		else if (xargs.icons == UNSET && *line == 'I'
		&& strncmp(line, "Icons=", 6) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "Icons=%5s\n", opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				icons = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				icons = 0;
		}
#endif /* !_NO_ICONS */

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

		else if (xargs.horizontal_list == UNSET && *line == 'L'
		&& strncmp(line, "ListingMode=", 12) == 0) {
			char *opt = strchr(line, '=');
			if (!opt || !*opt || !*(++opt)) {
				listing_mode = DEF_LISTING_MODE;
			} else {
				int ivalue = atoi(opt);
				switch(ivalue) {
				case VERTLIST: listing_mode = VERTLIST; break;
				case HORLIST: listing_mode = HORLIST; break;
				default: listing_mode = DEF_LISTING_MODE;
				}
			}
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

		else if (*line == 'M' && strncmp(line, "MaxFilenameLen=", 15) == 0) {
			int opt_num = 0;
			sscanf(line, "MaxFilenameLen=%d\n", &opt_num);
			if (opt_num <= 0)
				continue;
			max_name_len = opt_num;
		}

		else if (*line == 'M' && strncmp(line, "MaxHistory=", 11) == 0) {
			int opt_num = 0;
			sscanf(line, "MaxHistory=%d\n", &opt_num);
			if (opt_num <= 0)
				continue;
			max_hist = opt_num;
		}

		else if (*line == 'M' && strncmp(line, "MaxJumpTotalRank=", 17) == 0) {
			int opt_num = 0;
			ret = sscanf(line, "MaxJumpTotalRank=%d\n", &opt_num);
			if (ret == -1 || opt_num < INT_MIN || opt_num > INT_MAX)
				continue;
			max_jump_total_rank = opt_num;
		}

		else if (*line == 'M' && strncmp(line, "MaxLog=", 7) == 0) {
			int opt_num = 0;
			sscanf(line, "MaxLog=%d\n", &opt_num);
			if (opt_num <= 0)
				continue;
			max_log = opt_num;
		}

		else if (xargs.max_path == UNSET && *line == 'M'
		&& strncmp(line, "MaxPath=", 8) == 0) {
			int opt_num = 0;
			sscanf(line, "MaxPath=%d\n", &opt_num);
			if (opt_num <= 0)
				continue;
			max_path = opt_num;
		}

		else if (*line == 'M' && strncmp(line, "MaxPrintSelfiles=", 17) == 0) {
			int opt_num = 0;
			ret = sscanf(line, "MaxPrintSelfiles=%d\n", &opt_num);
			if (ret == -1)
				continue;
			max_printselfiles = opt_num;
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

		else if (*line == 'M' && strncmp(line, "MinJumpRank=", 12) == 0) {
			int opt_num = 0;
			ret = sscanf(line, "MinJumpRank=%d\n", &opt_num);
			if (ret == -1 || opt_num < INT_MIN || opt_num > INT_MAX)
				continue;
			min_jump_rank = opt_num;
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

		else if (!opener && *line == 'O' && strncmp(line, "Opener=", 7) == 0) {
			char *tmp = get_line_value(line);
			if (!tmp)
				continue;
			opener = savestring(tmp, strlen(tmp));
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

		else if (xargs.printsel == UNSET && *line == 'P'
		&& strncmp(line, "PrintSelfiles=", 14) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "PrintSelfiles=%5s\n", opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				print_selfiles = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				print_selfiles = 0;
		}

		else if (*line == 'P' && strncmp(line, "Prompt=", 7) == 0) {
			if (encoded_prompt)
				free(encoded_prompt);
			char *p = strchr(line, '=');
			if (p && *(++p))
				encoded_prompt = savestring(p, strlen(p));
		}

		else if (*line == 'P' && strncmp(line, "PromptStyle=", 12) == 0) {
			char opt_str[8] = "";
			ret = sscanf(line, "PromptStyle=%7s\n", opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "default", 7) == 0)
				prompt_style = DEF_PROMPT_STYLE;
			else if (strncmp(opt_str, "custom", 6) == 0)
				prompt_style = CUSTOM_PROMPT_STYLE;
			else
				prompt_style = DEF_PROMPT_STYLE;
		}

		else if (xargs.restore_last_path == UNSET && *line == 'R'
		&& strncmp(line, "RestoreLastPath=", 16) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "RestoreLastPath=%5s\n", opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				restore_last_path = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				restore_last_path = 0;
		}

		else if (*line == 'R' && strncmp(line, "RlEditMode=0", 12) == 0) {
			rl_vi_editing_mode(1, 0);
			/* By default, readline uses emacs editing mode */
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

		/* Check for the xargs.splash flag. If -1, it was
		 * not set via command line, so that it must be
		 * set here */
		else if (xargs.splash == UNSET && *line == 'S'
		&& strncmp(line, "SplashScreen=", 13) == 0) {
			char opt_str[MAX_BOOL] = "";
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

		else if (xargs.path == UNSET && cur_ws == UNSET && *line == 'S'
		&& strncmp(line, "StartingPath=", 13) == 0) {
			char *tmp = get_line_value(line);
			if (!tmp)
				continue;

			/* If starting path is not NULL, and exists, and is a
			 * directory, and the user has appropriate permissions,
			 * set path to starting path. If any of these conditions
			 * is false, path will be set to default, that is, CWD */
			if (xchdir(tmp, SET_TITLE) == 0) {
				if (cur_ws < 0)
					cur_ws = 0;
				free(ws[cur_ws].path);
				ws[cur_ws].path = savestring(tmp, strlen(tmp));
			} else {
				_err('w', PRINT_PROMPT, _("%s: '%s': %s. Using the "
					"current working directory as starting path\n"),
					PROGRAM_NAME, tmp, strerror(errno));
			}
		}

#ifndef _NO_SUGGESTIONS
		else if (*line == 'S' && strncmp(line, "SuggestFiletypeColor=", 21) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "SuggestFiletypeColor=%5s\n", opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				suggest_filetype_color = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				suggest_filetype_color = 0;
		}

		else if (*line == 'S'
		&& strncmp(line, "SuggestionStrategy=", 19) == 0) {
			char opt_str[SUG_STRATS + 1] = "";
			ret = sscanf(line, "SuggestionStrategy=%7s\n", opt_str);
			if (ret == -1)
				continue;
			int fail = 0;
			size_t s = 0;
			for (; opt_str[s]; s++) {
				if (opt_str[s] != 'a' && opt_str[s] != 'b'
				&& opt_str[s] != 'c' && opt_str[s] != 'e'
				&& opt_str[s] != 'f' && opt_str[s] != 'h'
				&& opt_str[s] != 'j' && opt_str[s] != '-') {
					fail = 1;
					break;
				}
			}
			if (fail || s != SUG_STRATS)
				continue;
			suggestion_strategy = savestring(opt_str, strlen(opt_str));
		}
#endif /* !_NO_SUGGESTIONS */

		else if (xargs.fzftab == UNSET && *line == 'T'
		&& strncmp(line, "TabCompletionMode=", 18) == 0) {
			char opt_str[9] = "";
			ret = sscanf(line, "TabCompletionMode=%8s\n", opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "standard", 8) == 0)
				fzftab = 0;
			else if (strncmp(opt_str, "fzf", 3) == 0)
				fzftab = 1;
		}

		else if (*line == 'F' && strncmp(line, "FzfTabOptions=", 14) == 0) {
			char *tmp = get_line_value(line);
			if (!tmp)
				continue;
			fzftab_options = savestring(tmp, strlen(tmp));
		}

		else if (*line == 'T' && strncmp(line, "TerminalCmd=", 12) == 0) {
			if (term) {
				free(term);
				term = (char *)NULL;
			}

			char *opt = strchr(line, '=');
			if (!opt || !*opt || !*(++opt))
				continue;

			char *tmp = remove_quotes(opt);
			if (!tmp)
				continue;

			term = savestring(tmp, strlen(tmp));
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

#ifndef _NO_TRASH
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
#endif

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

		else if (xargs.warn_wrong_cmd == UNSET && *line == 'W'
		&& strncmp(line, "WarningPrompt=", 14) == 0) {
			char opt_str[MAX_BOOL] = "";
			ret = sscanf(line, "WarningPrompt=%5s\n", opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "true", 4) == 0)
				warn_wrong_cmd = 1;
			else if (strncmp(opt_str, "false", 5) == 0)
				warn_wrong_cmd = 0;
		}

		else if (*line == 'W' && strncmp(line, "WarningPromptStr=", 17) == 0) {
			char *tmp = get_line_value(line);
			if (!tmp)
				continue;
			wprompt_str = savestring(tmp, strlen(tmp));
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
	}

	close_fstream(config_fp, fd);

	if (_filter) {
		ret = regcomp(&regex_exp, _filter, REG_NOSUB | REG_EXTENDED);
		if (ret != EXIT_SUCCESS) {
			_err('w', PRINT_PROMPT, _("%s: '%s': Invalid regular "
				  "expression\n"), PROGRAM_NAME, _filter);
			free(_filter);
			_filter = (char *)NULL;
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
		_err(0, PRINT_PROMPT, _("%s: Running in stealth mode: trash, "
			"persistent selection and directory history, just as bookmarks, "
			"logs and configuration files, are disabled.\n"),
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

	static int c = 0;
	if (c == 0) {
		c = 1;
		cschemes_n = get_colorschemes();
		set_colors(usr_cscheme ? usr_cscheme : "default", 1);
		free(usr_cscheme);
		usr_cscheme = (char *)NULL;
	}

	if (config_ok)
		read_config();

	if ((flags & GUI) && getenv("XTERM_VERSION")) {
		/* If running Xterm, instruct it to send an escape code (27)
		 * for Meta (Alt) key sequences. Otherwise, Alt keybindings won't
		 * work */
		printf("\x1b[?1036h"); /* metaSendsEscape = true */
/*		printf("\x1b[?1034l"); // eightBitInput = false
		printf("\x1b[>1;1m"); // modifyCursorKeys = 1
		printf("\x1b[>2;1m"); // modifyFunctionKeys = 1
		("\x1b[>1m" and "\x1b[>2m", reset to initial value) */
	}
}

static void
reset_variables(void)
{
	/* Free everything */
	free(config_dir_gral);
	free(config_dir);
	config_dir = config_dir_gral = (char *)NULL;

#ifndef _NO_TRASH
	free(trash_dir);
	free(trash_files_dir);
	free(trash_info_dir);
	trash_dir = trash_files_dir = trash_info_dir = (char *)NULL;
#endif

	free(bm_file);
	free(log_file);
	free(hist_file);
	free(dirhist_file);
	bm_file = log_file = hist_file = dirhist_file = (char *)NULL;

	free(config_file);
	free(profile_file);
	free(msg_log_file);
	config_file = profile_file = msg_log_file = (char *)NULL;

	free(mime_file);
	free(plugins_dir);
	free(actions_file);
	free(kbinds_file);
	mime_file = plugins_dir = actions_file = kbinds_file = (char *)NULL;

	free(colors_dir);
	free(tmp_dir);
	free(sel_file);
	free(remotes_file);
	tmp_dir = colors_dir = sel_file = remotes_file = (char *)NULL;

#ifndef _NO_SUGGESTIONS
	free(suggestion_buf);
	suggestion_buf = (char *)NULL;

	free(suggestion_strategy);
	suggestion_strategy = (char *)NULL;
#endif

	free(fzftab_options);
	fzftab_options = (char *)NULL;

	free(wprompt_str);
	wprompt_str = (char *)NULL;

#ifdef AUTOCMDS_TEST
	free_autocmds();
#endif

	free_remotes(0);

	if (_filter) {
		regfree(&regex_exp);
		free(_filter);
		_filter = (char *)NULL;
	}

	free(opener);
	opener = (char *)NULL;

	free(encoded_prompt);
	encoded_prompt = (char *)NULL;

	free(term);
	term = (char *)NULL;

	free(user.shell);
	user.shell = (char *)NULL;

	/* Reset all variables */
	auto_open = UNSET;
	autocd = UNSET;
	autojump = UNSET;
	autols = UNSET;
	case_sens_dirjump = UNSET;
	case_sens_path_comp = UNSET;
	case_sensitive = UNSET;
	case_sens_search = UNSET;
	cd_on_quit = UNSET;
	check_cap = UNSET;
	check_ext = UNSET;
	classify = UNSET;
	clear_screen = UNSET;
	colorize = UNSET;
	columned = UNSET;
	dirhist_map = UNSET;
	disk_usage = UNSET;
	elnpad = UNSET;
	ext_cmd_ok = UNSET;
	files_counter = UNSET;
	follow_symlinks = UNSET;
#ifndef _NO_FZF
	fzftab = UNSET;
#endif
#ifndef _NO_HIGHLIGHT
	highlight = UNSET;
#endif
#ifndef _NO_ICONS
	icons = UNSET;
#endif
	int_vars = UNSET;
	light_mode = UNSET;
	list_folders_first = UNSET;
	listing_mode = UNSET;
	logs_enabled = UNSET;
	long_view = UNSET;
	max_jump_total_rank = UNSET;
	max_printselfiles = UNSET;
	min_name_trim = UNSET;
	min_jump_rank = UNSET;
	no_eln = UNSET;
	pager = UNSET;
	print_selfiles = UNSET;
	prompt_offset = UNSET;
	prompt_style = UNSET;
	restore_last_path = UNSET;
	share_selbox = UNSET;
	show_hidden = UNSET;
	sort = UNSET;
	splash_screen = UNSET;
	tips = UNSET;
	unicode = UNSET;
	warn_wrong_cmd = UNSET;
	welcome_message = UNSET;

#ifndef _NO_SUGGESTIONS
	suggestions = suggest_filetype_color = UNSET;
#endif

#ifndef _NO_TRASH
	tr_as_rm = UNSET;
	trash_ok = 1;
#endif

	dir_changed = 0;
	dequoted = 0;
	internal_cmd = 0;
	is_sel = 0;
	kbind_busy = 0;
	mime_match = 0;
	no_log = 0;
	print_msg = 0;
	recur_perm_error_flag = 0;
	sel_is_last = 0;
	shell_is_interactive = 0;
	shell_terminal = 0;
	sort_reverse = 0;
	sort_switch = 0;

	config_ok = 1;
	home_ok = 1;
	selfile_ok = 1;

	pmsg = NOMSG;

	return;
}

static void
check_cmd_line_options(void)
{
#ifndef _NO_SUGGESTIONS
	if (xargs.suggestions != UNSET)
		suggestions = xargs.suggestions;
#endif

#ifndef _NO_TRASH
	if (xargs.trasrm != UNSET)
		tr_as_rm = xargs.trasrm;
#endif

#ifndef _NO_ICONS
	if (xargs.icons != UNSET)
		icons = xargs.icons;
#endif

	if (xargs.auto_open != UNSET)
		auto_open = xargs.auto_open;

	if (xargs.autocd != UNSET)
		autocd = xargs.autocd;

	if (xargs.autojump != UNSET)
		autojump = xargs.autojump;
	if (autojump)
		autocd = 1;

	if (xargs.case_sens_dirjump != UNSET)
		case_sens_dirjump = xargs.case_sens_dirjump;

	if (xargs.case_sens_path_comp != UNSET)
		case_sens_path_comp = xargs.case_sens_path_comp;

	if (xargs.autols != UNSET)
		autols = xargs.autols;

	if (xargs.cd_on_quit != UNSET)
		cd_on_quit = xargs.cd_on_quit;

	if (xargs.classify != UNSET)
		classify = xargs.classify;

	if (xargs.clear_screen != UNSET)
		clear_screen = xargs.clear_screen;

	if (xargs.dirmap != UNSET)
		dirhist_map = xargs.dirmap;

	if (xargs.disk_usage != UNSET)
		disk_usage = xargs.disk_usage;

	if (xargs.expand_bookmarks != UNSET)
		expand_bookmarks = xargs.expand_bookmarks;

	if (xargs.ext != UNSET)
		ext_cmd_ok = xargs.ext;

	if (xargs.ffirst != UNSET)
		list_folders_first = xargs.ffirst;

	if (xargs.files_counter != UNSET)
		files_counter = xargs.files_counter;

	if (xargs.hidden != UNSET)
		show_hidden = xargs.hidden;

	if (xargs.light != UNSET)
		light_mode = xargs.light;

	if (xargs.logs != UNSET)
		logs_enabled = xargs.logs;

	if (xargs.longview != UNSET)
		long_view = xargs.longview;

	if (xargs.max_dirhist != UNSET)
		max_dirhist = xargs.max_dirhist;

	if (xargs.max_path != UNSET)
		max_path = xargs.max_path;

	if (xargs.colorize != UNSET)
		colorize = xargs.colorize;

	if (xargs.columns != UNSET)
		columned = xargs.columns;

	if (xargs.noeln != UNSET)
		no_eln = xargs.noeln;

	if (xargs.only_dirs != UNSET)
		only_dirs = xargs.only_dirs;

	if (xargs.pager != UNSET)
		pager = xargs.pager;

	if (xargs.printsel != UNSET)
		print_selfiles = xargs.printsel;

	if (xargs.restore_last_path != UNSET)
		restore_last_path = xargs.restore_last_path;

	if (xargs.sensitive != UNSET)
		case_sensitive = xargs.sensitive;

	if (xargs.share_selbox != UNSET)
		share_selbox = xargs.share_selbox;

	if (xargs.sort != UNSET)
		sort = xargs.sort;

	if (xargs.sort_reverse != UNSET)
		sort_reverse = xargs.sort_reverse;

	if (xargs.splash != UNSET)
		splash_screen = xargs.splash;

	if (xargs.tips != UNSET)
		tips = xargs.tips;

	if (xargs.unicode != UNSET)
		unicode = xargs.unicode;

	if (xargs.welcome_message != UNSET)
		welcome_message = xargs.welcome_message;

	return;
}

int
reload_config(void)
{
	reset_variables();

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
	check_cmd_line_options();

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
	while (--i >= 0) {
		free(aliases[i].name);
		free(aliases[i].cmd);
	}
	free(aliases);
	aliases = (struct alias_t *)NULL;
	aliases_n = 0;

	i = (int)prompt_cmds_n;
	while (--i >= 0)
		free(prompt_cmds[i]);

	dirhist_total_index = 0;
	prompt_cmds_n = 0;

	get_aliases();
	get_prompt_cmds();
	load_dirhist();
	load_jumpdb();
	load_remotes();

	/* Set the current poistion of the dirhist index to the last
	 * entry */
	dirhist_cur_index = dirhist_total_index - 1;

	dir_changed = 1;
	set_env();
	return EXIT_SUCCESS;
}
