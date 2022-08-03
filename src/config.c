/* config.c -- functions to define, create, and set configuration files */

/*
 * This file is part of CliFM
 * 
 * Copyright (C) 2016-2022, L. Abramovich <johndoe.arch@outlook.com>
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
#include "checks.h"
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

#ifndef _NO_FZF
/* Determine input and output files to be used by the fuzzy finder (either fzf or fzy)
 * Let's do this even if fzftab is not enabled at startup, because this feature
 * can be enabled in place editing the config file */
void
set_finder_paths(void)
{
	char *p = xargs.stealth_mode == 1 ? P_tmpdir : tmp_dir;
	snprintf(finder_in_file, sizeof(finder_in_file), "%s/%s.finder.in", p, PNL);
	snprintf(finder_out_file, sizeof(finder_out_file), "%s/%s.finder.out", p, PNL);
}
#endif /* _NO_FZF */

/* Regenerate the configuration file and create a back up of the old
 * one */
static int
regen_config(void)
{
	int config_found = 1;
	struct stat attr;

	if (stat(config_file, &attr) == -1) {
		puts(_("No configuration file found"));
		config_found = 0;
	}

	if (config_found == 1) {
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
 * the configuration file and creates a back up of the old one */
int
edit_function(char **comm)
{
	if (xargs.stealth_mode == 1) {
		printf("%s: %s\n", PROGRAM_NAME, STEALTH_DISABLED);
		return EXIT_SUCCESS;
	}

	if (comm[1] && IS_HELP(comm[1])) {
		printf("%s\n", EDIT_USAGE);
		return EXIT_SUCCESS;
	}

	if (comm[1] && *comm[1] == 'r' && strcmp(comm[1], "reset") == 0)
		return regen_config();

	if (config_ok == 0) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, _("%s: Cannot access the configuration file\n"),
			PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	/* Get modification time of the config file before opening it */
	struct stat attr;

	/* If, for some reason (like someone erasing the file while the program
	 * is running) clifmrc doesn't exist, recreate the configuration file.
	 * Then run 'stat' again to reread the attributes of the file */
	if (stat(config_file, &attr) == -1) {
		create_config(config_file);
		stat(config_file, &attr);
	}

	time_t mtime_bfr = (time_t)attr.st_mtime;
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
	stat(config_file, &attr);
	/* If modification times differ, the file was modified after being opened */
	if (mtime_bfr != (time_t)attr.st_mtime) {
		/* Reload configuration only if the config file was modified */
		reload_config();
		welcome_message = 0;

		if (autols == 1) {
			free_dirlist();
			ret = list_dir();
		}
		print_reload_msg(_(CONFIG_FILE_UPDATED));
	}

	return ret;
}

/* Find the plugins-helper file and set CLIFM_PLUGINS_HELPER accordingly
 * This envionment variable will be used by plugins. Returns zero on
 * success or one on error */
static int
setenv_plugins_helper(void)
{
	if (getenv("CLIFM_PLUGINS_HELPER"))
		return EXIT_SUCCESS;

	struct stat attr;
	if (plugins_dir && *plugins_dir) {
		char _path[PATH_MAX];
		snprintf(_path, sizeof(_path), "%s/plugins-helper", plugins_dir);

	//	struct stat attr;
		if (stat(_path, &attr) != -1 && setenv("CLIFM_PLUGINS_HELPER", _path, 1) == 0)
			return EXIT_SUCCESS;
	}

	const char *_paths[] = {
#ifndef __HAIKU__
		"/usr/share/clifm/plugins/plugins-helper",
		"/usr/local/share/clifm/plugins/plugins-helper",
#else
		"/boot/system/non-packaged/data/clifm/plugins/plugins-helper",
		"/boot/system/data/clifm/plugins/plugins-helper",
#endif
		NULL};

	size_t i;
	for (i = 0; _paths[i]; i++) {
		if (stat(_paths[i], &attr) != -1
		&& setenv("CLIFM_PLUGINS_HELPER", _paths[i], 1) == 0)
			return EXIT_SUCCESS;
	}

	return EXIT_FAILURE;
}

/* Set a few environment variables, mostly useful to run custom scripts
 * via the actions function */
void
set_env(void)
{
	if (xargs.stealth_mode == 1)
		return;

	setenv("CLIFM", config_dir ? config_dir : "1", 1);
	setenv("CLIFM_PROFILE", alt_profile ? alt_profile : "default", 1);

	if (sel_file)
		setenv("CLIFM_SELFILE", sel_file, 1);

	setenv_plugins_helper();
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

	if (!share_selbox) {
		/* Private selection box is stored in the profile directory */
/*		sel_file = (char *)xnmalloc(config_dir_len + 12, sizeof(char));
		sprintf(sel_file, "%s/selbox.cfm", config_dir); */
		sel_file = (char *)xnmalloc(config_dir_len + 14, sizeof(char));
		sprintf(sel_file, "%s/selbox.clifm", config_dir);
	} else {
		/* Common selection box is stored in the general configuration directory */
/*		sel_file = (char *)xnmalloc(config_dir_len + 21, sizeof(char));
		sprintf(sel_file, "%s/.config/%s/selbox.cfm", user.home, PNL); */
		sel_file = (char *)xnmalloc(config_dir_len + 23, sizeof(char));
		sprintf(sel_file, "%s/.config/%s/selbox.clifm", user.home, PNL);
	}

	return;
}

int
create_kbinds_file(void)
{
	if (config_ok == 0 || !kbinds_file)
		return EXIT_FAILURE;

	struct stat attr;
	/* If the file already exists, do nothing */
	if (stat(kbinds_file, &attr) == EXIT_SUCCESS)
		return EXIT_SUCCESS;

	/* If not, try to import it from DATADIR */
	if (data_dir) {
		char sys_file[PATH_MAX];
//		snprintf(sys_file, PATH_MAX - 1, "%s/%s/keybindings.cfm", data_dir, PNL);
		snprintf(sys_file, PATH_MAX - 1, "%s/%s/keybindings.clifm", data_dir, PNL);
		if (stat(sys_file, &attr) == EXIT_SUCCESS) {
			char *cmd[] = {"cp", sys_file, kbinds_file, NULL};
			if (launch_execve(cmd, FOREGROUND, E_NOFLAG) == EXIT_SUCCESS)
				return EXIT_SUCCESS;
		}
	}

	/* Else, create it */
	FILE *fp = fopen(kbinds_file, "w");
	if (!fp) {
		_err('w', PRINT_PROMPT, "%s: '%s': %s\n", PROGRAM_NAME, kbinds_file, strerror(errno));
		return EXIT_FAILURE;
	}

	fprintf(fp, "# Keybindings file for %s\n\n\
# Emacs style key escapes are the simplest way of setting your \n\
# keybindings. For example, use \"action:\\C-t\" to bind the action name \n\
# 'action' to Ctrl-t \n\
# Note: available action names are defined below \n\n\
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
#home-dir2:\\e[7~\n\
# Home key (xterm)\n\
#home-dir3:\\e[H\n\
# Home key (Emacs term)\n\
#home-dir4:\\e[1~\n\
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
show-manpage2:\\e[11~\n\
show-cmds:\\eOQ\n\
show-cmds2:\\e[12~\n\
show-kbinds:\\eOR\n\
show-kbinds2:\\e[13~\n\n\
archive-sel:\\C-\\M-a\n\
bookmark-sel:\\C-\\M-b\n\
bookmarks:\\M-b\n\
clear-line:\\M-c\n\
clear-msgs:\\M-t\n\
create-file:\\M-n\n\
deselect-all:\\M-d\n\
export-sel:\\C-\\M-e\n\
dirs-first:\\M-g\n\
lock:\\M-o\n\
mountpoints:\\M-m\n\
move-sel:\\C-\\M-n\n\
new-instance:\\C-x\n\
next-profile:\\C-\\M-p\n\
only-dirs:\\M-,\n\
open-sel:\\C-\\M-g\n\
paste-sel:\\C-\\M-v\n\
prepend-sudo:\\M-v\n\
previous-profile:\\C-\\M-o\n\
rename-sel:\\C-\\M-r\n\
remove-sel:\\C-\\M-d\n\
refresh-screen:\\C-r\n\
selbox:\\M-s\n\
select-all:\\M-a\n\
show-dirhist:\\M-h\n\
sort-previous:\\M-z\n\
sort-next:\\M-x\n\
toggle-hidden:\\M-i\n\
toggle-hidden2:\\M-.\n\
toggle-light:\\M-y\n\
toggle-long:\\M-l\n\
toggle-max-name-len:\\C-\\M-l\n\
toggle-disk-usage:\\C-\\M-i\n\
toggle-virtualdir-full-paths:\\M-w\n\
trash-sel:\\C-\\M-t\n\
untrash-all:\\C-\\M-u\n\n\
# F6-12\n\
open-mime:\\e[17~\n\
open-jump-db:\\e[18~\n\
edit-color-scheme:\\e[19~\n\
open-keybinds:\\e[20~\n\
open-config:\\e[21~\n\
open-bookmarks:\\e[23~\n\
quit:\\e[24~\n\
\n\
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
	if (!data_dir || !src_filename || !dest
	|| !*data_dir || !*src_filename || !*dest)
		return EXIT_FAILURE;

	struct stat attr;
	char sys_file[PATH_MAX];
	snprintf(sys_file, PATH_MAX - 1, "%s/%s/%s", data_dir, PNL, src_filename);
	if (stat(sys_file, &attr) == -1)
		return EXIT_FAILURE;

	char *cmd[] = {"cp", sys_file, dest, NULL};
	int ret = launch_execve(cmd, FOREGROUND, E_NOFLAG);
	if (ret == EXIT_SUCCESS)
		return EXIT_SUCCESS;

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
//	if (import_from_data_dir("actions.cfm", file) == EXIT_SUCCESS)
	if (import_from_data_dir("actions.clifm", file) == EXIT_SUCCESS)
		return EXIT_SUCCESS;

	/* Else, create it */
	int fd;
	FILE *fp = open_fstream_w(file, &fd);
	if (!fp) {
		_err('e', PRINT_PROMPT, "%s: %s: %s\n", PROGRAM_NAME, file, strerror(errno));
		return EXIT_FAILURE;
	}

	fprintf(fp, "######################\n"
		"# Actions file for %s #\n"
		"######################\n\n"
		"# Define here your custom actions. Actions are "
		"custom command names\n"
		"# bound to an executable file located either in "
		"DATADIR/clifm/plugins\n"
		"# (usually /usr/local/share/clifm/plugins) or in "
		"$XDG_CONFIG_HOME/clifm/plugins.\n"
		"# Actions can be executed directly from "
		"%s command line, as if they\n"
		"# were any other command, and the associated "
		"file will be executed\n"
		"# instead. All parameters passed to the action "
		"command will be passed\n"
		"# to the corresponding plugin as well.\n\n"
		"+=finder.sh\n"
		"++=jumper.sh\n"
		"-=fzfnav.sh\n"
		"*=fzfsel.sh\n"
		"**=fzfdesel.sh\n"
		"//=rgfind.sh\n"
		"_=fzcd.sh\n"
		"bn=batch_create.sh\n"
		"cr=cprm.sh\n"
		"da=disk_analyzer.sh\n"
		"dh=fzfdirhist.sh\n"
		"dr=dragondrop.sh\n"
		"fdups=fdups.sh\n"
		"gg=pager.sh\n"
		"h=fzfhist.sh\n"
		"i=img_viewer.sh\n"
		"ih=ihelp.sh\n"
		"kbgen=kbgen\n"
		"music=music_player.sh\n"
		"ptot=pdf_viewer.sh\n"
		"rrm=recur_rm.sh\n"
		"update=update.sh\n"
		"vid=vid_viewer.sh\n"
		"vt=virtualize.sh\n"
		"wall=wallpaper_setter.sh\n",
	    PROGRAM_NAME, PROGRAM_NAME);

	close_fstream(fp, fd);
	return EXIT_SUCCESS;
}

void
create_tmp_files(void)
{
	if (xargs.stealth_mode == 1)
		return;

	size_t pnl_len = strlen(PNL);

	/* #### CHECK THE TMP DIR #### */

	/* If the temporary directory doesn't exist, create it. Let's create the
	 * parent directory (/tmp/clifm) with 1777 permissions (world writable
	 * with the sticky bit set), so that every user is able to create files
	 * in here, but only the file's owner can remove or modify them */
	size_t user_len = user.name ? strlen(user.name) : 7; /* 7: length of "unknown" */
	tmp_dir = (char *)xnmalloc(P_tmpdir_len + pnl_len + user_len + 3, sizeof(char));
	if (P_tmpdir_len > 0 && P_tmpdir[P_tmpdir_len - 1] == '/')
		sprintf(tmp_dir, "%s%s", P_tmpdir, PNL); /* On OpenBSD we get "/tmp/" */
	else
		sprintf(tmp_dir, "%s/%s", P_tmpdir, PNL);
	/* P_tmpdir is defined in stdio.h and it's value is usually /tmp
	 * If not defined, it will be defined as "/tmp" */

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
	 * store the list of selected files: TMP_DIR/clifm/username/.selbox_PROFILE.
	 * We use here very restrictive permissions (700), since only the corresponding
	 * user must be able to read and/or modify this list */
	sprintf(tmp_dir, "%s/%s/%s", P_tmpdir, PNL, user.name ? user.name : "unknown");
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
				"files will be lost after program exit\n", PROGRAM_NAME, tmp_dir);
		}
	}

	/* sel_file should has been set before by set_sel_file(). If not set,
	 * we do not have access to the config dir */
	if (sel_file)
		return;

	/*"We will write a temporary selfile in /tmp. Check if this latter is available */
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

/*		sel_file = (char *)xnmalloc(P_tmpdir_len + prof_len + 13, sizeof(char));
		sprintf(sel_file, "%s/selbox_%s.cfm", P_tmpdir, */
		sel_file = (char *)xnmalloc(P_tmpdir_len + prof_len + 15, sizeof(char));
		sprintf(sel_file, "%s/selbox_%s.clifm", P_tmpdir,
		    (alt_profile) ? alt_profile : "default");
	} else {
/*		sel_file = (char *)xnmalloc(P_tmpdir_len + 12, sizeof(char));
		sprintf(sel_file, "%s/selbox.cfm", P_tmpdir); */
		sel_file = (char *)xnmalloc(P_tmpdir_len + 14, sizeof(char));
		sprintf(sel_file, "%s/selbox.clifm", P_tmpdir);
	}

	_err('w', PRINT_PROMPT, _("%s: %s: Using a temporary directory for "
		"the Selection Box. Selected files won't be persistent across "
		"reboots\n"), PROGRAM_NAME, tmp_dir);
}

/* THIS IS TEMPORAL CODE!
 * It is intended to rename config files from .cfm to .clifm, and should
 * be removed once this transition has been made (2 releases?) */

#if !defined(_NO_RENAME_CONFIG)
#include <dirent.h>
static void
rename_cfm_files(char *dir)
{
	struct dirent **_files = (struct dirent **)NULL;
	int n = scandir(dir, &_files, NULL, NULL);

	if (n == -1)
		return;

	size_t i;
	for (i = 0; i < (size_t)n; i++) {
		if (SELFORPARENT(_files[i]->d_name)) {
			free(_files[i]);
			continue;
		}

		char *p = strrchr(_files[i]->d_name, '.');
		if (!p || !*(p + 1) || strcmp(p, ".cfm") != 0) {
			free(_files[i]);
			continue;
		}

		*p = '\0';
		char src[PATH_MAX], dst[PATH_MAX];
		snprintf(src, sizeof(src), "%s/%s.cfm", dir, _files[i]->d_name);
		snprintf(dst, sizeof(dst), "%s/%s.clifm", dir, _files[i]->d_name);

//		printf("%s renamed as %s\n", src, dst);
		if (rename(src, dst) == -1)
			_err('e', PRINT_PROMPT, "%s: %s: %s\n", PROGRAM_NAME, src, strerror(errno));
		free(_files[i]);
	}

	free(_files);
}

static void
rename_profile_files(char *dir)
{
	char p[PATH_MAX];
	snprintf(p, sizeof(p), "%s/profiles", dir);
	struct stat a;
	if (stat(p, &a) == -1)
		return;

	struct dirent **_files = (struct dirent **)NULL;
	int n = scandir(p, &_files, NULL, NULL);

	if (n == -1)
		return;

	size_t i;
	for (i = 0; i < (size_t)n; i++) {
		if (SELFORPARENT(_files[i]->d_name)) {
			free(_files[i]);
			continue;
		}

		char tmp[PATH_MAX * 2];
		snprintf(tmp, sizeof(tmp), "%s/%s", p, _files[i]->d_name);

#if !defined(_DIRENT_HAVE_D_TYPE)
		if (stat(tmp, &a) == -1) {
			free(_files[i]);
			continue;
		}
		if (S_ISDIR(a.st_mode))
#else
		if (_files[i]->d_type == DT_DIR)
#endif /* !_DIRENT_HAVE_D_TYPE */
			rename_cfm_files(tmp);

		free(_files[i]);
	}

	free(_files);
}

static void
rename_color_files(char *dir)
{
	char p[PATH_MAX];
	snprintf(p, sizeof(p), "%s/colors", dir);

	struct stat a;
	if (stat(p, &a) != -1)
		rename_cfm_files(p);
}

static void
rename_config_files(char *dir)
{
	rename_cfm_files(dir);
	rename_color_files(dir);
	rename_profile_files(dir);
}

#include "readline.h"
static int
get_user_answer(const char *_msg)
{
	char *answer = (char *)NULL;
	while (!answer) {
		answer = rl_no_hist(_msg);
		if (!answer)
			continue;

		if (!*answer || answer[1]) {
			free(answer);
			answer = (char *)NULL;
			continue;
		}

		switch(*answer) {
		case 'y': /* fallthrough */
		case 'Y': free(answer); return 1;
		case 'n': /* fallthrough */
		case 'N': free(answer); return 0;
		default: free(answer); answer = (char *)NULL; continue;
		}
	}

	return 0; /* Never reached */
}

static void
check_cfm_files(void)
{
	/* Let's suppose that, if keybindings.cfm doesn't exist, the
	 * transition has been already made (or this is a fresh installation) */
	struct stat a;
	char q[PATH_MAX + 17];
	snprintf(q, sizeof(q), "%s/keybindings.cfm", config_dir_gral);
	if (stat(q, &a) == -1)
		return;

	fprintf(stderr, "##################\n"
	"# IMPORTANT NOTE #\n"
	"##################\n\n"
	"%s transitioned from .cfm to .clifm file extension for its "
	"configuration files.\n"
	"This has been done to avoid conflicts with the ColdFusion Markup Language file "
	"extension (cfm).\n"
	"Your config files, including color schemes, will be automatically renamed, "
	"that's all: nothing will be lost.\n"
	"Note that if you cancel this operation (by pressing 'n'), new .clifm config files will be "
	"created and the\nold .cfm ones will be kept, but ignored (in which case you "
	"should manually remove/rename them to silence this warning).\n\n"
	"If you prefer to perform a manual transition, please follow these steps:\n"
	"1. Press 'n' (clifm will start as usual, using the new configuration files)\n"
	"2. cd into the appropriate directories (those containing .cfm files, i.e, clifm, "
	"clifm/colors, and /clifm/profiles)\n"
	"3. Bulk rename .cfm files as .clifm files: 'br *.cfm' (or, file by "
	"file: 'm FILE.cfm FILE.clifm')\n"
	"4. Restart clifm\n",
	_PROGRAM_NAME);

	putchar('\n');
	if (get_user_answer("Rename configuration files automatically? [y/n] ") != 1)
		return;

	rename_config_files(config_dir_gral);
	_err('n', PRINT_PROMPT, "%s: Configuration files changed from .cfm to .clifm "
		"file extension\n", PROGRAM_NAME);
}
#endif /* !_NO_RENAME_CONFIG */

static void
define_config_file_names(void)
{
	size_t pnl_len = strlen(PNL);

	if (alt_config_dir) {
		config_dir_gral = savestring(alt_config_dir, strlen(alt_config_dir));
		free(alt_config_dir);
		alt_config_dir = (char *)NULL;
	} else {
		/* If $XDG_CONFIG_HOME is set, use it for the config file.
		 * Else, fall back to $HOME/.config */
		char *xdg_config_home = getenv("XDG_CONFIG_HOME");
		if (xdg_config_home) {
			size_t xdg_config_home_len = strlen(xdg_config_home);
			config_dir_gral = (char *)xnmalloc(xdg_config_home_len + pnl_len + 2, sizeof(char));
			sprintf(config_dir_gral, "%s/%s", xdg_config_home, PNL);
			xdg_config_home = (char *)NULL;
		} else {
			config_dir_gral = (char *)xnmalloc(user.home_len + pnl_len + 11, sizeof(char));
			sprintf(config_dir_gral, "%s/.config/%s", user.home, PNL);
		}
	}

#if !defined(_NO_RENAME_CONFIG)
	check_cfm_files();
#endif /* !_NO_RENAME_CONFIG */

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

	config_dir_len = strlen(config_dir);

	tags_dir = (char *)xnmalloc(config_dir_len + 6, sizeof(char));
	sprintf(tags_dir, "%s/tags", config_dir);

	if (alt_kbinds_file) {
		kbinds_file = savestring(alt_kbinds_file, strlen(alt_kbinds_file));
		free(alt_kbinds_file);
		alt_kbinds_file = (char *)NULL;
	} else {
		/* Keybindings per user, not per profile */
/*		kbinds_file = (char *)xnmalloc(config_gral_len + 17, sizeof(char));
		sprintf(kbinds_file, "%s/keybindings.cfm", config_dir_gral); */
		kbinds_file = (char *)xnmalloc(config_gral_len + 19, sizeof(char));
		sprintf(kbinds_file, "%s/keybindings.clifm", config_dir_gral);
	}

	colors_dir = (char *)xnmalloc(config_gral_len + 8, sizeof(char));
	sprintf(colors_dir, "%s/colors", config_dir_gral);

	plugins_dir = (char *)xnmalloc(config_gral_len + 9, sizeof(char));
	sprintf(plugins_dir, "%s/plugins", config_dir_gral);
/*
#ifndef _NO_TRASH
	trash_dir = (char *)xnmalloc(user.home_len + 20, sizeof(char));
	sprintf(trash_dir, "%s/.local/share/Trash", user.home);

	size_t trash_len = strlen(trash_dir);

	trash_files_dir = (char *)xnmalloc(trash_len + 7, sizeof(char));
	sprintf(trash_files_dir, "%s/files", trash_dir);

	trash_info_dir = (char *)xnmalloc(trash_len + 6, sizeof(char));
	sprintf(trash_info_dir, "%s/info", trash_dir);
#endif */

/*	dirhist_file = (char *)xnmalloc(config_dir_len + 13, sizeof(char));
	sprintf(dirhist_file, "%s/dirhist.cfm", config_dir); */
	dirhist_file = (char *)xnmalloc(config_dir_len + 15, sizeof(char));
	sprintf(dirhist_file, "%s/dirhist.clifm", config_dir);

	if (!alt_bm_file) {
/*		bm_file = (char *)xnmalloc(config_dir_len + 15, sizeof(char));
		sprintf(bm_file, "%s/bookmarks.cfm", config_dir); */
		bm_file = (char *)xnmalloc(config_dir_len + 17, sizeof(char));
		sprintf(bm_file, "%s/bookmarks.clifm", config_dir);
	} else {
		bm_file = savestring(alt_bm_file, strlen(alt_bm_file));
		free(alt_bm_file);
		alt_bm_file = (char *)NULL;
	}

/*	log_file = (char *)xnmalloc(config_dir_len + 9, sizeof(char));
	sprintf(log_file, "%s/log.cfm", config_dir); */
	log_file = (char *)xnmalloc(config_dir_len + 11, sizeof(char));
	sprintf(log_file, "%s/log.clifm", config_dir);

/*	hist_file = (char *)xnmalloc(config_dir_len + 13, sizeof(char));
	sprintf(hist_file, "%s/history.cfm", config_dir); */
	hist_file = (char *)xnmalloc(config_dir_len + 15, sizeof(char));
	sprintf(hist_file, "%s/history.clifm", config_dir);

	if (!alt_config_file) {
		config_file = (char *)xnmalloc(config_dir_len + pnl_len + 4, sizeof(char));
		sprintf(config_file, "%s/%src", config_dir, PNL);
	} else {
		config_file = savestring(alt_config_file, strlen(alt_config_file));
		free(alt_config_file);
		alt_config_file = (char *)NULL;
	}

/*	profile_file = (char *)xnmalloc(config_dir_len + 13, sizeof(char));
	sprintf(profile_file, "%s/profile.cfm", config_dir); */
	profile_file = (char *)xnmalloc(config_dir_len + 15, sizeof(char));
	sprintf(profile_file, "%s/profile.clifm", config_dir);

/*	mime_file = (char *)xnmalloc(config_dir_len + 14, sizeof(char));
	sprintf(mime_file, "%s/mimelist.cfm", config_dir); */
	mime_file = (char *)xnmalloc(config_dir_len + 16, sizeof(char));
	sprintf(mime_file, "%s/mimelist.clifm", config_dir);

/*	actions_file = (char *)xnmalloc(config_dir_len + 13, sizeof(char));
	sprintf(actions_file, "%s/actions.cfm", config_dir); */
	actions_file = (char *)xnmalloc(config_dir_len + 15, sizeof(char));
	sprintf(actions_file, "%s/actions.clifm", config_dir);

/*	remotes_file = (char *)xnmalloc(config_dir_len + 10, sizeof(char));
	sprintf(remotes_file, "%s/nets.cfm", config_dir); */
	remotes_file = (char *)xnmalloc(config_dir_len + 12, sizeof(char));
	sprintf(remotes_file, "%s/nets.clifm", config_dir);

	return;
}

static int
import_rl_file(void)
{
	if (!data_dir || !config_dir_gral)
		return EXIT_FAILURE;

	char tmp[PATH_MAX];
//	sprintf(tmp, "%s/readline.cfm", config_dir_gral);
	sprintf(tmp, "%s/readline.clifm", config_dir_gral);
	struct stat attr;
	if (lstat(tmp, &attr) == 0)
		return EXIT_SUCCESS;

	char rl_file[PATH_MAX];
//	snprintf(rl_file, PATH_MAX - 1, "%s/%s/readline.cfm", data_dir, PNL);
	snprintf(rl_file, PATH_MAX - 1, "%s/%s/readline.clifm", data_dir, PNL);
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
	/* First, try to import it from DATADIR */
	char src_filename[NAME_MAX];
	snprintf(src_filename, NAME_MAX, "%src", PNL);
	if (import_from_data_dir(src_filename, file) == EXIT_SUCCESS)
		return EXIT_SUCCESS;

	/* If not found, create it */
	int fd;
	FILE *config_fp = open_fstream_w(file, &fd);

	if (!config_fp) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "%s: fopen: %s: %s\n", PROGRAM_NAME,
			file, strerror(errno));
		return EXIT_FAILURE;
	}

	/* Do not translate anything in the config file */
	fprintf(config_fp,

"\t\t###########################################\n\
\t\t#                  CLIFM                  #\n\
\t\t#      The command line file manager      #\n\
\t\t###########################################\n\n"

	    "# This is the configuration file for CliFM\n\n"

	    "# Color schemes (or just themes) are stored in the colors directory.\n\
# Available themes: base16, default, dracula, dracula-vivid, gruvbox,\n\
# jellybeans-vivid, light, molokai, nocolor, nord, one-dark, solarized, zenburn\n\
# Visit %s to get some extra themes\n\
ColorScheme=%s\n\n"

	    "# The amount of files contained by a directory is informed next\n\
# to the directory name. However, this feature might slow things down when,\n\
# for example, listing files on a remote server. The filescounter can be\n\
# disabled here, via the --no-files-counter option, or using the 'fc'\n\
# command while in the program itself.\n\
FilesCounter=%s\n\n"

		"# How to list files: 0 = vertically (like ls(1) would), 1 = horizontally\n\
ListingMode=%d\n\n"

		"# List files automatically after changing current directory\n\
AutoLs=%s\n\n"

		"# Send errors, warnings, and notices to the notification daemon?\n\
DesktopNotifications=%s\n\n"

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
mvCmd=%d\n\n",

	    COLORS_REPO,
		DEF_COLOR_SCHEME,
		DEF_FILES_COUNTER == 1 ? "true" : "false",
		DEF_LISTING_MODE,
		DEF_AUTOLS == 1 ? "true" : "false",
		DEF_DESKTOP_NOTIFICATIONS == 1 ? "true" : "false",
		DEF_DIRHIST_MAP == 1 ? "true" : "false",
		DEF_CP_CMD,
		DEF_MV_CMD);

	fprintf(config_fp,
		"# TAB completion mode: 'standard', 'fzf', 'fzy', or 'smenu'. Defaults to\n\
# 'fzf' if the binary is found in PATH. Otherwise, the standard mode is used\n\
TabCompletionMode=\n\n"

	    "# MaxPath is only used for the /p option of the prompt: the current working\n\
# directory will be abbreviated to its basename (everything after last slash)\n\
# whenever the current path is longer than MaxPath.\n\
MaxPath=%d\n\n"

	    "WelcomeMessage=%s\n\n\
# Print %s's logo screen at startup\n\
SplashScreen=%s\n\n\
ShowHiddenFiles=%s\n\n\
# List file properties next to file names instead of just file names\n\
LongViewMode=%s\n\
# Properties fields to be printed in long view mode\n\
# d = inode number\n\
# p|n = permissions: either symbolic (p) or numeric/octal (n)\n\
# i = user/group IDs (numeric)\n\
# a|c|m = either last (a)ccess, (m)odification or status (c)hange time\n\
# s = size\n\
# A single dash \"-\" disables all fields\n\
PropFields=\"%s\"\n\
# Print files apparent size instead of actual device usage (Linux only)\n\
ApparentSize=%s\n\
# If running in long view, print directories full size (including contents)\n\
FullDirSize=%s\n\n\
# Log errors and warnings\n\
Logs=%s\n\
# Keep a record of external commands and internal commands able to\n\
# modify the files system (e.g. 'r', 'c', 'm', and so on). Logs must be set to true\n\
LogCmds=%s\n\n"

	    "# Minimum length at which a file name can be trimmed in long view mode\n\
# (including ELN length and spaces). When running in long mode, this setting\n\
# overrides MaxFilenameLen\n\
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

		"# We have three search strategies: 0 = glob-only, 1 = regex-only,\n\
# and 2 = glob-regex\n\
SearchStrategy=%d\n\n"

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
		PROGRAM_NAME,
		DEF_SPLASH_SCREEN == 1 ? "true" : "false",
		DEF_SHOW_HIDDEN == 1 ? "true" : "false",
		DEF_LONG_VIEW == 1 ? "true" : "false",
		DEF_PROP_FIELDS,
		DEF_APPARENT_SIZE == 1 ? "true" : "false",
		DEF_FULL_DIR_SIZE == 1 ? "true" : "false",
		DEF_LOGS_ENABLED == 1 ? "true" : "false",
		DEF_LOG_CMDS == 1 ? "true" : "false",
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
		DEF_SEARCH_STRATEGY,
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
# application. If not set, 'lira', CliFM's built-in opener, is used.\n\
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

		"# A comma separated list of workspace names in the form NUM=NAME\n\
# Example: \"1=MAIN,2=EXTRA,3=GIT,4=WORK\" or \"1=α,2=β,3=γ,4=δ\"\n\
WorkspaceNames=\"\"\n\n"

	    "# Print a usage tip at startup\n\
Tips=%s\n\n\
ListDirsFirst=%s\n\n\
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
# MAXFILENAMELEN will be truncated at MAXFILENAMELEN using a tilde.\n\
# Set it to -1 (or empty) to remove this limit.\n\
# When running in long mode, this setting is overriden by MinFilenameTrim\n\
MaxFilenameLen=%d\n\n"

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
		DEF_LIST_DIRS_FIRST == 1 ? "true" : "false",
		DEF_CASE_SENS_LIST == 1 ? "true" : "false",
		DEF_CASE_SENS_DIRJUMP == 1 ? "true" : "false",
		DEF_CASE_SENS_PATH_COMP == 1 ? "true" : "false",
		DEF_CASE_SENS_SEARCH == 1 ? "true" : "false",
		DEF_UNICODE == 1 ? "true" : "false",
		DEF_PAGER == 1 ? "true" : "false",
		DEF_MAX_NAME_LEN,
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
#autocmd ~/important !printf \"Keep your fingers outta here!\\n\" && read -n1\n\
#autocmd ~/Downloads !/usr/share/clifm/plugins/fzfnav.sh\n\n",
	    config_fp);

	close_fstream(config_fp, fd);
	return EXIT_SUCCESS;
}

static void
create_def_cscheme(void)
{
	if (!colors_dir || !*colors_dir)
		return;

/*	char *cscheme_file = (char *)xnmalloc(strlen(colors_dir) + 13, sizeof(char));
	sprintf(cscheme_file, "%s/default.cfm", colors_dir); */
	char *cscheme_file = (char *)xnmalloc(strlen(colors_dir) + 15, sizeof(char));
	sprintf(cscheme_file, "%s/default.clifm", colors_dir);

	/* If the file already exists, do nothing */
	struct stat attr;
	if (stat(cscheme_file, &attr) != -1) {
		free(cscheme_file);
		return;
	}

	/* Try to import it from data dir */
	if (import_color_scheme("default") == EXIT_SUCCESS) {
		free(cscheme_file);
		return;
	}

	/* If cannot be imported either, create it with default values */
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
# the final 'm'. 8 bit, 256 colors, RGB, and hex (#rrggbb) colors are supported.\n\
# A detailed explanation of all these codes can be found in the manpage.\n\n"

			"FiletypeColors=\"%s\"\n\n"

			"InterfaceColors=\"%s\"\n\n"

			"# Same as FiletypeColors, but for file extensions. The format is always\n\
# *.EXT=COLOR\n"

			"ExtColors=\"%s\"\n\n"
			"DirIconColor=\"00;33\"\n\n"
			"DividingLine=\"%s\"\n\n"

			"If set to 'true', automatically print notifications at the left\n\
of the prompt. If set to 'false', let the prompt string handle these notifications\n\
itself via escape codes. See the manpage for more information\n"
			"Notifications=\"%s\"\n\n"
			"Prompt=\"%s\"\n\n"

			"An alternative prompt to warn the user about invalid command names\n"
			"EnableWarningPrompt=\"%s\"\n\n"
			"WarningPrompt=\"%s\"\n\n"
			"FzfTabOptions=\"%s\"\n\n",

		PROGRAM_NAME,
		DEF_FILE_COLORS,
		DEF_IFACE_COLORS,
		DEF_EXT_COLORS,
		DEF_DIV_LINE,
		DEF_PROMPT_NOTIF == 1 ? "true" : "false",
		DEFAULT_PROMPT,
		DEF_WARNING_PROMPT == 1 ? "true" : "false",
		DEF_WPROMPT_STR,
		DEF_FZFTAB_OPTIONS);

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

	/* If the file already exists, do nothing */
	if (stat(remotes_file, &attr) == EXIT_SUCCESS)
		return EXIT_SUCCESS;

	/* Let's try to copy the file from DATADIR */
	char sys_remotes[PATH_MAX];
//	snprintf(sys_remotes, PATH_MAX - 1, "%s/%s/nets.cfm", data_dir, PNL);
	snprintf(sys_remotes, PATH_MAX - 1, "%s/%s/nets.clifm", data_dir, PNL);

	if (stat(sys_remotes, &attr) == EXIT_SUCCESS) {
		char *cmd[] = {"cp", "-f", sys_remotes, remotes_file, NULL};
		if (launch_execve(cmd, FOREGROUND, E_NOFLAG) == EXIT_SUCCESS)
			return EXIT_SUCCESS;
	}

	/* If not in DATADIR, let's create a minimal file here */
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
				 * #     TAGS DIR      #
				 * #####################*/

	if (stat(tags_dir, &attr) == -1 && xmkdir(tags_dir, S_IRWXU) == EXIT_FAILURE)
		_err('w', PRINT_PROMPT, _("%s: %s: Error creating tags directory. "
			"Tag function disabled\n"),	PROGRAM_NAME, tags_dir);

				/* #####################
				 * #    CONFIG FILE    #
				 * #####################*/

	if (stat(config_file, &attr) == -1)
		config_ok = create_config(config_file) == EXIT_SUCCESS ? 1 : 0;

	if (config_ok == 0)
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
			fprintf(profile_fp, _("# This is %s's profile file\n#\n"
				"# Write here the commands you want to be executed at startup\n"
				"# Ex:\n#echo \"%s, the command line file manager\"; read -r\n#\n"
				"# Uncommented, non-empty lines are executed line by line. If you\n"
				"# want a multi-line command, just write a script for it:\n"
				"#sh /path/to/my/script.sh\n"),
				_PROGRAM_NAME, _PROGRAM_NAME);
			fclose(profile_fp);
		}
	}

				/* #####################
				 * #    COLORS DIR     #
				 * ##################### */

	if (stat(colors_dir, &attr) == -1
	&& xmkdir(colors_dir, S_IRWXU) == EXIT_FAILURE)
		_err('w', PRINT_PROMPT, _("%s: mkdir: Error creating colors "
			"directory. Using the default color scheme\n"), PROGRAM_NAME);

	/* Generate the default color scheme file */
	create_def_cscheme();

				/* #####################
				 * #      PLUGINS      #
				 * #####################*/

	if (stat(plugins_dir, &attr) == -1
	&& xmkdir(plugins_dir, S_IRWXU) == EXIT_FAILURE)
		_err('e', PRINT_PROMPT, _("%s: mkdir: Error creating plugins "
			"directory. The actions function is disabled\n"), PROGRAM_NAME);

	import_rl_file();
	create_actions_file(actions_file);
	create_mime_file(mime_file, 0);
	create_remotes_file();
}

static int
create_mime_file_anew(char *file)
{
	int fd;
	FILE *fp = open_fstream_w(file, &fd);
	if (!fp) {
		_err('e', PRINT_PROMPT, "%s: %s: %s\n", PROGRAM_NAME, file, strerror(errno));
		return EXIT_FAILURE;
	}

	fprintf(fp, "                  ###################################\n\
                  #   Configuration file for Lira   #\n\
                  #     CliFM's resource opener     #\n\
                  ###################################\n\
\n\
# Commented and blank lines are omitted\n\
\n\
# The below settings cover the most common filetypes\n\
# It is recommended to edit this file placing your prefered applications\n\
# at the beginning of the apps list to speed up the opening process\n\
\n\
# The file is read top to bottom and left to right; the first existent\n\
# application found will be used\n\
\n\
# Applications defined here are NOT desktop files, but commands (arguments\n\
# could be used as well). Write you own handmade scripts to open specific\n\
# files if necessary. Ex: X:^text/.*:~/scripts/my_cool_script.sh\n\
\n\
# Use 'X' to specify a GUI environment and '!X' for non-GUI environments,\n\
# like the kernel built-in console or a remote SSH session.\n\
\n\
# Use 'N' to match file names instead of MIME types.\n\
\n\
# Regular expressions are allowed for both file types and file names.\n\
\n\
# Use the %%f placeholder to specify the position of the file name to be\n\
# opened in the command. Example:\n\
# 'mpv %%f --terminal=no' -> 'mpv FILE --terminal=no'\n\
# If %%f is not specified, the file name will be added to the end of the\n\
# command. Ex: 'mpv --terminal=no' -> 'mpv --terminal=no FILE'\n\
\n\
# Running the opening application in the background:\n\
# For GUI applications:\n\
#    APP %%f &\n\
# For terminal applications:\n\
#    TERM -e APP %%f &\n\
# Replace 'TERM' and 'APP' by the corresponding values. The -e option\n\
# might vary depending on the terminal emulator used (TERM)\n\
\n\
# Note on graphical applications: If the opening application is already running\n\
# the file might be opened in a tab, and CliFM won't wait for the file to be\n\
# closed (because the process already returned). To avoid this, instruct the\n\
# application to run a new instance: Ex: geany -i, gedit -s, kate -n,\n\
# pluma --new-window, and so on.\n\
\n\
# To silence STDERR and/or STDOUT use !E and !O respectivelly (they could\n\
# be used together). Examples:\n\
# Silence STDERR only and run in the foreground:\n\
#    mpv %%f !E\n\
# Silence both (STDERR and STDOUT) and run in the background:\n\
#    mpv %%f !EO &\n\
# or\n\
#    mpv %%f !E !O &\n\
\n\
# Environment variables could be used as well. Example:\n\
# X:text/plain=$EDITOR %%f &;$VISUAL;nano;vi\n\
\n\
###########################\n\
#  File names/extensions  #\n\
###########################\n\
\n\
# Match a full file name\n\
#X:N:some_filename=cmd\n\
\n\
# Match all file names starting with 'str'\n\
#X:N:^str.*=cmd\n\
\n\
# Match files with extension 'ext'\n\
#X:N:.*\\.ext$=cmd\n\
\n\
X:N:.*\\.djvu$=djview;zathura;evince;atril\n\
X:N:.*\\.epub$=mupdf;zathura;ebook-viewer\n\
X:N:.*\\.mobi$=ebook-viewer\n\
X:N:.*\\.(cbr|cbz)$=zathura\n\
X:N:(.*\\.clifm$|clifmrc)=$EDITOR;$VISUAL;kak;micro;nvim;vim;vi;mg;emacs;ed;nano;mili;leafpad;mousepad;featherpad;gedit;kate;pluma\n\
!X:N:(.*\\.clifm$|clifmrc)=$EDITOR;$VISUAL;kak;micro;nvim;vim;vi;mg;emacs;ed;nano\n\
\n\n");

	fprintf(fp, "##################\n\
#   MIME types   #\n\
##################\n\
\n\
# Directories - only for the open-with (ow) command and the --open command\n\
# line switch\n\
# In graphical environments directories will be opened in a new window\n\
X:inode/directory=xterm -e clifm %%f &;xterm -e vifm %%f &;pcmanfm %%f &;thunar %%f &;xterm -e ncdu %%f &\n\
!X:inode/directory=vifm;ranger;nnn;ncdu\n\
\n\
# Web content\n\
X:^text/html$=$BROWSER;surf;vimprobable;vimprobable2;qutebrowser;dwb;jumanji;luakit;uzbl;uzbl-tabbed;uzbl-browser;uzbl-core;iceweasel;midori;opera;firefox;seamonkey;brave;chromium-browser;chromium;google-chrome;epiphany;konqueror;elinks;links2;links;lynx;w3m\n\
!X:^text/html$=$BROWSER;elinks;links2;links;lynx;w3m\n\
\n\
# Text\n\
#X:^text/x-(c|shellscript|perl|script.python|makefile|fortran|java-source|javascript|pascal)$=geany\n\
X:(^text/.*|application/json|inode/x-empty)=$EDITOR;$VISUAL;kak;micro;dte;nvim;vim;vi;mg;emacs;ed;nano;mili;leafpad;mousepad;featherpad;nedit;kate;gedit;pluma;io.elementary.code;liri-text;xed;atom;nota;gobby;kwrite;xedit\n\
!X:(^text/.*|application/json|inode/x-empty)=$EDITOR;$VISUAL;kak;micro;dte;nvim;vim;vi;mg;emacs;ed;nano\n\
\n\
# Office documents\n\
X:^application/.*(open|office)document.*=libreoffice;soffice;ooffice\n\
\n\
# Archives\n\
# Note: 'ad' is CliFM's built-in archives utility (based on atool). Remove it if you\n\
# prefer another application\n\
X:^application/(zip|gzip|zstd|x-7z-compressed|x-xz|x-bzip*|x-tar|x-iso9660-image)=ad;xarchiver %%f &;lxqt-archiver %%f &;ark %%f &\n\
!X:^application/(zip|gzip|zstd|x-7z-compressed|x-xz|x-bzip*|x-tar|x-iso9660-image)=ad\n\
\n\
# PDF\n\
X:.*/pdf$=mupdf;sioyek;llpp;lpdf;zathura;mupdf-x11;apvlv;xpdf;evince;atril;okular;epdfview;qpdfview\n\
\n\
# Images\n\
X:^image/gif$=animate;pqiv;sxiv -a;nsxiv -a\n\
X:^image/.*=fim;display;sxiv;nsxiv;pqiv;gpicview;qview;qimgv;inkscape;mirage;ristretto;eog;eom;xviewer;viewnior;nomacs;geeqie;gwenview;gthumb;gimp\n\
!X:^image/*=fim;img2txt;cacaview;fbi;fbv\n\
\n\
# Video and audio\n\
X:^video/.*=ffplay;mplayer;mplayer2;mpv;vlc;gmplayer;smplayer;celluloid;qmplayer2;haruna;totem\n\
X:^audio/.*=ffplay -nodisp -autoexit;mplayer;mplayer2;mpv;vlc;gmplayer;smplayer;totem\n\
\n\
# Fonts\n\
X:^font/.*=fontforge;fontpreview\n\
\n\
# Torrent:\n\
X:application/x-bittorrent=rtorrent;transimission-gtk;transmission-qt;deluge-gtk;ktorrent\n\
\n\
# Fallback to another resource opener as last resource\n\
.*=xdg-open;mimeo;mimeopen -n;whippet -m;open;linopen;\n");
	close_fstream(fp, fd);

	return EXIT_SUCCESS;
}

static void
print_mime_file_msg(char *file)
{
	int _free = 0;
	char *f = home_tilde(file, &_free);

	_err('n', PRINT_PROMPT, _("%sNOTE%s: %s created a new MIME list file (%s). "
		"It is recommended to edit this file (entering 'mm edit' or "
		"pressing F6) to add the programs you use and remove those "
		"you don't. This will make the process of opening files "
		"faster and smoother\n"), BOLD, NC, PROGRAM_NAME, f ? f : file);

	if (f != file)
		free(f);
}

int
create_mime_file(char *file, int new_prof)
{
	if (!file || !*file)
		return EXIT_FAILURE;

	struct stat attr;
	if (stat(file, &attr) == EXIT_SUCCESS)
		return EXIT_SUCCESS;

//	int ret = import_from_data_dir("mimelist.cfm", file);
	int ret = import_from_data_dir("mimelist.clifm", file);
	if (ret != EXIT_SUCCESS)
		ret = create_mime_file_anew(file);

	if (new_prof == 0 && ret == EXIT_SUCCESS)
		print_mime_file_msg(file);
	return ret;
}

int
create_bm_file(void)
{
	if (!bm_file)
		return EXIT_FAILURE;

	struct stat attr;
	if (stat(bm_file, &attr) != -1)
		return EXIT_SUCCESS;

	FILE *fp = fopen(bm_file, "w+");
	if (!fp) {
		_err('e', PRINT_PROMPT, "bookmarks: '%s': %s\n", bm_file, strerror(errno));
		return EXIT_FAILURE;
	}

	fprintf(fp, "### This is the bookmarks file for %s ###\n\n"
		    "# Empty and commented lines are ommited\n"
		    "# The bookmarks syntax is: [shortcut]name:path\n"
		    "# Example:\n"
		    "[c]clifm:%s\n",
	    PROGRAM_NAME, config_dir ? config_dir : "path/to/file");
	fclose(fp);

	return EXIT_SUCCESS;
}

#ifndef CLIFM_SUCKLESS
static char *
get_line_value(char *line)
{
	char *opt = strchr(line, '=');
	if (!opt || !*opt || !*(++opt) )
		return (char *)NULL;

	return remove_quotes(opt);
}

/* Get boolean value from LINE and set VAR accordingly */
static inline int
set_config_bool_value(const char *line, int *var)
{
	char *p = strchr(line, '=');
	if (!p || !*(++p))
		return (-1);

	if (*p == 't' && strncmp(p, "true", 4) == 0) {
		*var = 1;
	} else {
		if (*p == 'f' && strncmp(p, "false", 5) == 0)
			*var = 0;
	}

	return EXIT_SUCCESS;
}

static inline int
_set_colorscheme(const char *line)
{
	char *p = strchr(line, '=');
	if (!p || !*(++p))
		return (-1);

	size_t l = strlen(p);
	if (p[l - 1] == '\n') {
		p[l - 1] = '\0';
		l--;
	}

	free(usr_cscheme);
	usr_cscheme = savestring(p, l);

	return EXIT_SUCCESS;
}

void
set_div_line(const char *line)
{
	char *opt = strchr(line, '=');
	if (!opt || !*opt || !*(++opt)) {
		*div_line = *DEF_DIV_LINE;
		return;
	}

	char *tmp = remove_quotes(opt);
	if (!tmp) {
		*div_line = '\0';
		return;
	}

	xstrsncpy(div_line, tmp, NAME_MAX);
}

static inline int
_set_filter(const char *line)
{
	char *p = strchr(line, '=');
	if (!p || !*(++p))
		return (-1);

	char *q = remove_quotes(p);
	if (!q)
		return (-1);

	size_t l = strlen(q);
	if (q[l - 1] == '\n') {
		q[l - 1] = '\0';
		l--;
	}

	if (*q == '!') {
		filter_rev = 1;
		q++;
		l--;
	} else {
		filter_rev = 0;
	}

	free(_filter);
	_filter = savestring(q, l);

	return EXIT_SUCCESS;
}

static inline void
set_listing_mode(const char *line)
{
	char *p = strchr(line, '=');
	if (!p || !*p || !*(++p))
		goto END;

	int n = atoi(p);
	if (n == INT_MIN)
		goto END;

	switch(n) {
	case VERTLIST: listing_mode = VERTLIST; return;
	case HORLIST: listing_mode = HORLIST; return;
	default: break;
	}

END:
	listing_mode = DEF_LISTING_MODE;
}

static void
set_search_strategy(char *line)
{
	char *p = strchr(line, '=');
	if (!p || !*p || !*(++p))
		return;
	switch(*p) {
	case '0': search_strategy = GLOB_ONLY; break;
	case '1': search_strategy = REGEX_ONLY; break;
	case '2': search_strategy = GLOB_REGEX; break;
	default: break;
	}
}

static inline int
set_max_filename_len(const char *line)
{
	char *p = strchr(line, '=');
	if (!p || !*p || !*(++p))
		return (max_name_len = UNSET);

	int n = atoi(p);
	if (n <= 0)
		return (max_name_len = UNSET);

	max_name_len = n;
	return EXIT_SUCCESS;
}

static void
free_workspaces_names(void)
{
	if (workspaces) {
		int i = MAX_WS;
		while (--i >= 0) {
			free(workspaces[i].name);
			workspaces[i].name = (char *)NULL;
		}
	}

	return;
}

/* Get workspaces names from the WorkspacesNames line in the configuration
 * file and store them in the workspaces array */
void
set_workspaces_names(char *line)
{
	if (!line || !*line)
		return;

	char *e = strchr(line, '=');
	if (!e || !*(++e))
		return;

	char *t = remove_quotes(e);
	if (!t || !*t)
		return;

	char *p = (char *)NULL;
	while (*t) {
		p = strchr(t, ',');
		if (p)
			*p = '\0';
		else
			p = t;

		e = strchr(t, '=');
		if (!e || !*(e + 1))
			goto CONT;

		*e = '\0';
		if (!is_number(t))
			goto CONT;

		int a = atoi(t);
		if (a <= 0 || a > MAX_WS)
			goto CONT;

		free(workspaces[a - 1].name);
		workspaces[a - 1].name = savestring(e + 1, strlen(e + 1));

CONT:
		t = p + 1;
		continue;
	}

	return;
}

static void
read_config(void)
{
	int fd;
	FILE *config_fp = open_fstream_r(config_file, &fd);
	if (!config_fp) {
		_err('e', PRINT_PROMPT, _("%s: fopen: '%s': %s. Using default values.\n"),
		    PROGRAM_NAME, config_file, strerror(errno));
		return;
	}

	free_workspaces_names();

	if (xargs.rl_vi_mode == 1)
		rl_vi_editing_mode(1, 0);

	int ret = -1;
	max_name_len = DEF_MAX_NAME_LEN;
	*div_line = *DEF_DIV_LINE;
	char line[PATH_MAX + 15];

	*prop_fields_str = '\0';

	while (fgets(line, (int)sizeof(line), config_fp)) {
		if (*line == '\n' || *line == '#')
			continue;

		else if (xargs.apparent_size == UNSET && *line == 'A'
		&& strncmp(line, "ApparentSize=", 13) == 0) {
			if (set_config_bool_value(line, &apparent_size) == -1)
				continue;
		}

		else if (*line == 'a' && strncmp(line, "autocmd ", 8) == 0)
			parse_autocmd_line(line + 8);

		else if (xargs.autocd == UNSET && *line == 'A'
		&& strncmp(line, "Autocd=", 7) == 0) {
			if (set_config_bool_value(line, &autocd) == -1)
				continue;
		}

		else if (xargs.autols == UNSET && *line == 'A'
		&& strncmp(line, "AutoLs=", 7) == 0) {
			if (set_config_bool_value(line, &autols) == -1)
				continue;
		}

		else if (xargs.auto_open == UNSET && *line == 'A'
		&& strncmp(line, "AutoOpen=", 9) == 0) {
			if (set_config_bool_value(line, &auto_open) == -1)
				continue;
		}

#ifndef _NO_SUGGESTIONS
		else if (xargs.suggestions == UNSET && *line == 'A'
		&& strncmp(line, "AutoSuggestions=", 16) == 0) {
			if (set_config_bool_value(line, &suggestions) == -1)
				continue;
		}
#endif

		else if (xargs.case_sens_dirjump == UNSET && *line == 'C'
		&& strncmp(line, "CaseSensitiveDirJump=", 21) == 0) {
			if (set_config_bool_value(line, &case_sens_dirjump) == -1)
				continue;
		}

		else if (*line == 'C'
		&& strncmp(line, "CaseSensitiveSearch=", 20) == 0) {
			if (set_config_bool_value(line, &case_sens_search) == -1)
				continue;
		}

		else if (xargs.sensitive == UNSET && *line == 'C'
		&& strncmp(line, "CaseSensitiveList=", 18) == 0) {
			if (set_config_bool_value(line, &case_sensitive) == -1)
				continue;
		}

		else if (xargs.case_sens_path_comp == UNSET && *line == 'C'
		&& strncmp(line, "CaseSensitivePathComp=", 22) == 0) {
			if (set_config_bool_value(line, &case_sens_path_comp) == -1)
				continue;
		}

		else if (xargs.cd_on_quit == UNSET && *line == 'C'
		&& strncmp(line, "CdOnQuit=", 9) == 0) {
			if (set_config_bool_value(line, &cd_on_quit) == -1)
				continue;
		}

		else if (xargs.classify == UNSET && *line == 'C'
		&& strncmp(line, "Classify=", 9) == 0) {
			if (set_config_bool_value(line, &classify) == -1)
				continue;
		}

		else if (xargs.clear_screen == UNSET && *line == 'C'
		&& strncmp(line, "ClearScreen=", 12) == 0) {
			if (set_config_bool_value(line, &clear_screen) == -1)
				continue;
		}

		else if (!usr_cscheme && *line == 'C'
		&& strncmp(line, "ColorScheme=", 12) == 0) {
			if (_set_colorscheme(line) == -1)
				continue;
		}

		else if (*line == 'c' && strncmp(line, "cpCmd=", 6) == 0) {
			int opt_num = 0;
			ret = sscanf(line, "cpCmd=%d\n", &opt_num);
			if (ret == -1)
				continue;
			if (opt_num >= 0 && opt_num <= CP_CMD_AVAILABLE)
				cp_cmd = opt_num;
			else
				cp_cmd = DEF_CP_CMD;
		}

		else if (xargs.desktop_notifications == UNSET && *line == 'D'
		&& strncmp(line, "DesktopNotifications=", 21) == 0) {
			if (set_config_bool_value(line, &desktop_notifications) == -1)
				continue;
		}

		else if (xargs.dirmap == UNSET && *line == 'D'
		&& strncmp(line, "DirhistMap=", 11) == 0) {
			if (set_config_bool_value(line, &dirhist_map) == -1)
				continue;
		}

		else if (xargs.disk_usage == UNSET && *line == 'D'
		&& strncmp(line, "DiskUsage=", 10) == 0) {
			if (set_config_bool_value(line, &disk_usage) == -1)
				continue;
		}

		else if (*line == 'D' && strncmp(line, "DividingLine", 12) == 0) {
			set_div_line(line);
		}

		else if (xargs.expand_bookmarks == UNSET && *line == 'E'
		&& strncmp(line, "ExpandBookmarks=", 16) == 0) {
			if (set_config_bool_value(line, &expand_bookmarks) == -1)
				continue;
		}

		else if (xargs.ext == UNSET && *line == 'E'
		&& strncmp(line, "ExternalCommands=", 17) == 0) {
			if (set_config_bool_value(line, &ext_cmd_ok) == -1)
				continue;
		}

		else if (xargs.files_counter == UNSET && *line == 'F'
		&& strncmp(line, "FilesCounter=", 13) == 0) {
			if (set_config_bool_value(line, &files_counter) == -1)
				continue;
		}

		else if (!_filter && *line == 'F' && strncmp(line, "Filter=", 7) == 0) {
			if (_set_filter(line) == -1)
				continue;
		}

#ifndef _NO_HIGHLIGHT
		else if (xargs.highlight == UNSET && *line == 'S'
		&& strncmp(line, "SyntaxHighlighting=", 19) == 0) {
			if (set_config_bool_value(line, &highlight) == -1)
				continue;
		}
#endif /* !_NO_HIGHLIGHT */

#ifndef _NO_ICONS
		else if (xargs.icons == UNSET && *line == 'I'
		&& strncmp(line, "Icons=", 6) == 0) {
			if (set_config_bool_value(line, &icons) == -1)
				continue;
		}
#endif /* !_NO_ICONS */

		else if (xargs.light == UNSET && *line == 'L'
		&& strncmp(line, "LightMode=", 10) == 0) {
			if (set_config_bool_value(line, &light_mode) == -1)
				continue;
		}

		else if (xargs.dirs_first == UNSET && *line == 'L'
		&& strncmp(line, "ListDirsFirst=", 17) == 0) {
			if (set_config_bool_value(line, &list_dirs_first) == -1)
				continue;
		}

		else if (xargs.horizontal_list == UNSET && *line == 'L'
		&& strncmp(line, "ListingMode=", 12) == 0) {
			set_listing_mode(line);
		}

		else if (xargs.longview == UNSET && *line == 'L'
		&& strncmp(line, "LongViewMode=", 13) == 0) {
			if (set_config_bool_value(line, &long_view) == -1)
				continue;
		}

		else if (xargs.full_dir_size == UNSET && *line == 'F'
		&& strncmp(line, "FullDirSize=", 12) == 0) {
			if (set_config_bool_value(line, &full_dir_size) == -1)
				continue;
		}

		else if (xargs.logs == UNSET && *line == 'L'
		&& strncmp(line, "Logs=", 5) == 0) {
			if (set_config_bool_value(line, &logs_enabled) == -1)
				continue;
		}

		else if (*line == 'L' && strncmp(line, "LogCmds=", 8) == 0) {
			if (set_config_bool_value(line, &log_cmds) == -1)
				continue;
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
			if (set_max_filename_len(line) == -1)
				continue;
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
			if (ret == -1)
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
			if (ret == -1)
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
			free(opener);
			opener = savestring(tmp, strlen(tmp));
		}

		else if (xargs.pager == UNSET && *line == 'P'
		&& strncmp(line, "Pager=", 6) == 0) {
			if (set_config_bool_value(line, &pager) == -1)
				continue;
		}

		else if (xargs.printsel == UNSET && *line == 'P'
		&& strncmp(line, "PrintSelfiles=", 14) == 0) {
			if (set_config_bool_value(line, &print_selfiles) == -1)
				continue;
		}

		else if (*line == 'P' && strncmp(line, "Prompt=", 7) == 0) {
			free(encoded_prompt);
			encoded_prompt = (char *)NULL;
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
				prompt_notif = 1;
			else if (strncmp(opt_str, "custom", 6) == 0)
				prompt_notif = 0;
			else
				prompt_notif = DEF_PROMPT_NOTIF;
		}

		else if (*line == 'P' && strncmp(line, "PropFields=", 11) == 0) {
			char *tmp = get_line_value(line);
			if (!tmp)
				continue;
			xstrsncpy(prop_fields_str, tmp, PROP_FIELDS_SIZE);
			set_prop_fields(prop_fields_str);
		}

		else if (xargs.restore_last_path == UNSET && *line == 'R'
		&& strncmp(line, "RestoreLastPath=", 16) == 0) {
			if (set_config_bool_value(line, &restore_last_path) == -1)
				continue;
		}

		else if (*line == 'R' && strncmp(line, "RlEditMode=0", 12) == 0) {
			rl_vi_editing_mode(1, 0); /* Readline defaults to emacs */
		}

		else if (*line == 'S' && strncmp(line, "SearchStrategy=", 15) == 0) {
			set_search_strategy(line);
		}

		else if (xargs.share_selbox == UNSET && *line == 'S'
		&& strncmp(line, "ShareSelbox=", 12) == 0) {
			if (set_config_bool_value(line, &share_selbox) == -1)
				continue;
		}

		else if (xargs.hidden == UNSET && *line == 'S'
		&& strncmp(line, "ShowHiddenFiles=", 16) == 0) {
			if (set_config_bool_value(line, &show_hidden) == -1)
				continue;
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
			if (set_config_bool_value(line, &sort_reverse) == -1)
				continue;
		}

		else if (xargs.splash == UNSET && *line == 'S'
		&& strncmp(line, "SplashScreen=", 13) == 0) {
			if (set_config_bool_value(line, &splash_screen) == -1)
				continue;
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
				free(workspaces[cur_ws].path);
				workspaces[cur_ws].path = savestring(tmp, strlen(tmp));
			} else {
				_err('w', PRINT_PROMPT, _("%s: chdir: %s: %s. Using the "
					"current working directory as starting path\n"),
					PROGRAM_NAME, tmp, strerror(errno));
			}
		}

#ifndef _NO_SUGGESTIONS
		else if (*line == 'S' && strncmp(line, "SuggestFiletypeColor=", 21) == 0) {
			if (set_config_bool_value(line, &suggest_filetype_color) == -1)
				continue;
		}

		else if (*line == 'S'
		&& strncmp(line, "SuggestionStrategy=", 19) == 0) {
			char opt_str[SUG_STRATS + 1] = "";
			ret = sscanf(line, "SuggestionStrategy=%7s\n", opt_str);
			if (ret == -1)
				continue;
			int fail = 0;
			size_t s;
			for (s = 0; opt_str[s]; s++) {
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
			free(suggestion_strategy);
			suggestion_strategy = savestring(opt_str, strlen(opt_str));
		}
#endif /* !_NO_SUGGESTIONS */

#ifndef _NO_FZF
		else if (xargs.fzftab == UNSET && xargs.fzytab == UNSET && xargs.smenutab == UNSET
		&& *line == 'T' && strncmp(line, "TabCompletionMode=", 18) == 0) {
			char opt_str[9] = "";
			ret = sscanf(line, "TabCompletionMode=%8s\n", opt_str);
			if (ret == -1)
				continue;
			if (strncmp(opt_str, "standard", 8) == 0) {
				fzftab = 0; tabmode = STD_TAB;
			} else if (strncmp(opt_str, "fzf", 3) == 0) {
				fzftab = 1; tabmode = FZF_TAB;
			} else if (strncmp(opt_str, "fzy", 3) == 0) {
				fzftab = 1; tabmode = FZY_TAB;
			} else if (strncmp(opt_str, "smenu", 5) == 0) {
				fzftab = 1; tabmode = SMENU_TAB;
			}
		}
#endif /* !_NO_FZF */

		else if (*line == 'F' && strncmp(line, "FzfTabOptions=", 14) == 0) {
			char *tmp = get_line_value(line);
			if (!tmp)
				continue;
			if (*tmp == 'n' && strcmp(tmp, "none") == 0) {
				fzftab_options = (char *)xnmalloc(1, sizeof(char));
				*fzftab_options = '\0';
			} else {
				free(fzftab_options);
				fzftab_options = savestring(tmp, strlen(tmp));
				if (strstr(fzftab_options, "--height"))
					fzf_height_set = 1;
			}
		}

		else if (*line == 'T' && strncmp(line, "TerminalCmd=", 12) == 0) {
			char *opt = strchr(line, '=');
			if (!opt || !*opt || !*(++opt))
				continue;

			char *tmp = remove_quotes(opt);
			if (!tmp)
				continue;

			free(term);
			term = savestring(tmp, strlen(tmp));
		}

		else if (xargs.tips == UNSET && *line == 'T' && strncmp(line, "Tips=", 5) == 0) {
			if (set_config_bool_value(line, &tips) == -1)
				continue;
		}

#ifndef _NO_TRASH
		else if (xargs.trasrm == UNSET && *line == 'T'
		&& strncmp(line, "TrashAsRm=", 10) == 0) {
			if (set_config_bool_value(line, &tr_as_rm) == -1)
				continue;
		}
#endif

		else if (xargs.unicode == UNSET && *line == 'U'
		&& strncmp(line, "Unicode=", 8) == 0) {
			if (set_config_bool_value(line, &unicode) == -1)
				continue;
		}

		else if (xargs.warning_prompt == UNSET && *line == 'W'
		&& strncmp(line, "WarningPrompt=", 14) == 0) {
			if (set_config_bool_value(line, &warning_prompt) == -1)
				continue;
		}

		else if (*line == 'W'
		&& strncmp(line, "WarningPromptStr=", 17) == 0) {
			char *tmp = get_line_value(line);
			if (!tmp)
				continue;
			free(wprompt_str);
			wprompt_str = savestring(tmp, strlen(tmp));
		}

		else if (xargs.welcome_message == UNSET && *line == 'W'
			&& strncmp(line, "WelcomeMessage=", 15) == 0) {
			if (set_config_bool_value(line, &welcome_message) == -1)
				continue;
		}

		else {
			if (*line == 'W' && strncmp(line, "WorkspaceNames=", 15) == 0)
				set_workspaces_names(line);
		}
	}

	close_fstream(config_fp, fd);

	if (xargs.disk_usage_analyzer == 1) {
		sort = STSIZE;
		long_view = full_dir_size = 1;
		list_dirs_first = welcome_message = 0;
	}

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
#endif /* CLIFM_SUCKLESS */

static void
check_colors(void)
{
	if (getenv("CLIFM_NO_COLOR") || getenv("NO_COLOR")) {
		colorize = 0;
	} else {
		if (colorize == UNSET) {
			if (xargs.colorize == UNSET)
				colorize = DEF_COLORS;
			else
				colorize = xargs.colorize;
		}
	}

	if (colorize == 1) {
		unsetenv("CLIFM_COLORLESS");
		set_colors(usr_cscheme ? usr_cscheme : "default", 1);
		return;
	}

	if (xargs.stealth_mode == 0)
		setenv("CLIFM_COLORLESS", "1", 1);

	reset_filetype_colors();
	reset_iface_colors();
	unset_suggestions_color();
}

#ifndef _NO_FZF
/* Just check if --height is specified in FZF_DEFAULT_OPTS */
static int
get_fzf_win_height()
{
	char *p = getenv("FZF_DEFAULT_OPTS");
	if (!p || !*p)
		return 0;

	char *q = strstr(p, "--height");
	if (!q)
		return 0;

	return 1;
}
#endif /* !_NO_FZF */

#ifndef _NO_TRASH
static void
create_trash_dirs(void)
{
	struct stat attr;
	if (stat(trash_dir, &attr) == -1) {
		if (xargs.stealth_mode == 1) {
			_err('w', PRINT_PROMPT, _("%s: %s: %s. Trash function disabled. "
				"If needed, create the directory manually and restart %s.\n"),
				PROGRAM_NAME, trash_dir, strerror(errno), PROGRAM_NAME);
			trash_ok = 0;
			return;
		}

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
			_err('w', PRINT_PROMPT, _("%s: mkdir: %s: Error creating trash "
				"directory. Trash function disabled\n"), PROGRAM_NAME, trash_dir);
		}
	}

	/* If it exists, check it is writable */
	else if (access(trash_dir, W_OK) == -1) {
		trash_ok = 0;
		_err('w', PRINT_PROMPT, _("%s: %s: Directory not writable. "
				"Trash function disabled\n"), PROGRAM_NAME, trash_dir);
	}
}

static void
set_trash_dirs(void)
{
	if (!user.home) {
		trash_ok = 0;
		return;
	}

	trash_dir = (char *)xnmalloc(user.home_len + 20, sizeof(char));
	sprintf(trash_dir, "%s/.local/share/Trash", user.home);

	size_t trash_len = strlen(trash_dir);

	trash_files_dir = (char *)xnmalloc(trash_len + 7, sizeof(char));
	sprintf(trash_files_dir, "%s/files", trash_dir);

	trash_info_dir = (char *)xnmalloc(trash_len + 6, sizeof(char));
	sprintf(trash_info_dir, "%s/info", trash_dir);

	create_trash_dirs();
}
#endif /* _NO_TRASH */

/* Set up CliFM directories and config files. Load the user's
 * configuration from clifmrc */
void
init_config(void)
{
#ifndef _NO_TRASH
	set_trash_dirs();
#endif /* _NO_TRASH */
	if (xargs.stealth_mode == 1) {
		_err(ERR_NO_LOG, PRINT_PROMPT, _("%s: Running in stealth mode: "
			"persistent selection, bookmarks, jump database and directory history, "
			"just as plugins, logs and configuration files, are disabled.\n"), PROGRAM_NAME);
		config_ok = 0;
		check_colors();
		return;
	}

	if (home_ok == 0) {
		check_colors();
		return;
	}

	define_config_file_names();
	create_config_files();
#ifndef CLIFM_SUCKLESS
	cschemes_n = get_colorschemes();

	if (config_ok == 1)
		read_config();
#else
	xstrsncpy(div_line, DEF_DIV_LINE, sizeof(div_line));
#endif /* CLIFM_SUCKLESS */

	check_colors();

#ifndef _NO_FZF
	/* If FZF win height was not defined in the config file,
	 * check whether it is present in FZF_DEFAULT_OPTS */
	if (fzftab && fzf_height_set == 0)
		fzf_height_set = get_fzf_win_height();
#endif

	char *t = getenv("TERM");
	if (xargs.list_and_quit != 1 && t && *t == 'x' && strncmp(t, "xterm", 5) == 0)
		/* If running Xterm, instruct it to send an escape code (27) for
		 * Meta (Alt) key sequences. Otherwise, Alt keybindings won't work */
		fputs(META_SENDS_ESC, stdout); /* metaSendsEscape = true */
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
	config_file = profile_file = (char *)NULL;

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
	free(suggestion_strategy);
	suggestion_buf = suggestion_strategy = (char *)NULL;
#endif

	free(fzftab_options);
	free(tags_dir);
	free(wprompt_str);
	fzftab_options = tags_dir = wprompt_str = (char *)NULL;

	free_autocmds();
	free_tags();
	free_remotes(0);

	if (_filter) {
		regfree(&regex_exp);
		free(_filter);
		_filter = (char *)NULL;
	}

	free(opener);
	free(encoded_prompt);
/*	free(right_prompt); */
	free(term);
	opener = encoded_prompt /*= right_prompt*/ = term = (char *)NULL;

	int i = (int)cschemes_n;
	while (--i >= 0)
		free(color_schemes[i]);
	free(color_schemes);
	color_schemes = (char **)NULL;
	cschemes_n = 0;
	free(usr_cscheme);
	usr_cscheme = (char *)NULL;
	cur_cscheme = (char *)NULL;

	free(user.shell);
	user.shell = (char *)NULL;

	/* Reset all variables */
	apparent_size = UNSET;
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
	cp_cmd = UNSET;
	dirhist_map = UNSET;
	disk_usage = UNSET;
	ext_cmd_ok = UNSET;
	files_counter = UNSET;
	follow_symlinks = UNSET;
	full_dir_size = UNSET;
#ifndef _NO_FZF
	fzftab = UNSET;
#endif
#ifndef _NO_HIGHLIGHT
	highlight = UNSET;
#endif
	hist_status = UNSET;
#ifndef _NO_ICONS
	icons = UNSET;
#endif
	int_vars = UNSET;
	light_mode = UNSET;
	list_dirs_first = UNSET;
	listing_mode = UNSET;
	log_cmds = UNSET;
	logs_enabled = UNSET;
	long_view = UNSET;

	max_dirhist = UNSET;
	max_files = UNSET;
	max_hist = UNSET;
	max_name_len = DEF_MAX_NAME_LEN;
	max_jump_total_rank = UNSET;
	max_log = UNSET;
	max_path = UNSET;
	max_printselfiles = UNSET;

	mv_cmd = UNSET;
	no_eln = UNSET;
	only_dirs = UNSET;
	pager = UNSET;
	print_removed_files = UNSET;
	print_selfiles = UNSET;
	prompt_offset = UNSET;
	prompt_notif = UNSET;
	restore_last_path = UNSET;
	search_strategy = UNSET;
	share_selbox = UNSET;
	show_hidden = UNSET;
	sort = UNSET;
	splash_screen = UNSET;
	tips = UNSET;
	unicode = UNSET;
	warning_prompt = UNSET;
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

	if (xargs.dirs_first != UNSET)
		list_dirs_first = xargs.dirs_first;

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
}

#ifndef _NO_FZF
static void
update_finder_binaries_status(void)
{
	char *p = (char *)NULL;
	if (!(finder_flags & FZF_BIN_OK)) {
		if ((p = get_cmd_path("fzf"))) {
			free(p);
			finder_flags |= FZF_BIN_OK;
		}
	}

	if (!(finder_flags & FZY_BIN_OK)) {
		if ((p = get_cmd_path("fzy"))) {
			free(p);
			finder_flags |= FZY_BIN_OK;
		}
	}

	if (!(finder_flags & SMENU_BIN_OK)) {
		if ((p = get_cmd_path("smenu"))) {
			free(p);
			finder_flags |= SMENU_BIN_OK;
		}
	}
}
#endif /* !_NO_FZF */

int
reload_config(void)
{
#ifndef _NO_FZF
	enum tab_mode tabmode_bk = tabmode;
#endif /* !_NO_FZF */
	reset_variables();

	/* Set up config files and options */
	init_config();

	/* If some option was not set, set it to the default value*/
	check_options();
	set_sel_file();
	create_tmp_files();

	/* If some option was set via command line, keep that value for any profile */
	check_cmd_line_options();

#ifndef _NO_FZF
	if (tabmode_bk != tabmode)
		update_finder_binaries_status();
	check_completion_mode();
#endif /* !_NO_FZF */

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
	load_tags();
	load_remotes();

	/* Set the current poistion of the dirhist index to the last entry */
	dirhist_cur_index = dirhist_total_index - 1;

	dir_changed = 1;
	set_env();
	return EXIT_SUCCESS;
}
