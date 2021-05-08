/* keybinds.c -- functions keybindings configuration */

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

#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>
#include <readline/readline.h>

#include "config.h"
#include "exec.h"
#include "misc.h"
#include "mime.h"
#include "keybinds.h"
#include "aux.h"
#include "prompt.h"
#include "listing.h"
#include "profiles.h"

int
kbinds_reset(void)
{
	int exit_status = EXIT_SUCCESS;
	struct stat file_attrib;

	if (stat(KBINDS_FILE, &file_attrib) == -1)
		exit_status = create_kbinds_file();

	else {
		char *cmd[] = { "rm", KBINDS_FILE, NULL };
		if (launch_execve(cmd, FOREGROUND, E_NOFLAG) == EXIT_SUCCESS)
			exit_status = create_kbinds_file();
		else
			exit_status = EXIT_FAILURE;
	}

	if (exit_status == EXIT_SUCCESS)
		_err('n', PRINT_PROMPT, _("%s: Restart the program for changes "
			 "to take effect\n"), PROGRAM_NAME);

	return exit_status;
}

static int
kbinds_edit(void)
{
	if (xargs.stealth_mode == 1) {
		printf("%s: Access to configuration files is not allowed in "
			   "stealth mode\n", PROGRAM_NAME);
		return EXIT_SUCCESS;
	}

	if (!KBINDS_FILE)
		return EXIT_FAILURE;

	struct stat file_attrib;

	if (stat(KBINDS_FILE, &file_attrib) == -1) {
		create_kbinds_file();
		stat(KBINDS_FILE, &file_attrib);
	}

	time_t mtime_bfr = (time_t)file_attrib.st_mtime;

	char *cmd[] = { "mm", KBINDS_FILE, NULL };
	int ret = mime_open(cmd);

	if (ret != EXIT_SUCCESS)
		return EXIT_FAILURE;

	stat(KBINDS_FILE, &file_attrib);

	if (mtime_bfr == (time_t)file_attrib.st_mtime)
		return EXIT_SUCCESS;

	_err('n', PRINT_PROMPT, _("%s: Restart the program for changes to "
		 "take effect\n"), PROGRAM_NAME);

	return EXIT_SUCCESS;
}

int
kbinds_function(char **args)
{
	if (!args)
		return EXIT_FAILURE;

	if (!args[1]) {
		size_t i;
		for (i = 0; i < kbinds_n; i++) {
			printf("%s: %s\n", kbinds[i].key, kbinds[i].function);
		}

		return EXIT_SUCCESS;
	}

	if (*args[1] == '-' && strcmp(args[1], "--help") == 0) {
		puts(_("Usage: kb, keybinds [edit] [reset]"));
		return EXIT_SUCCESS;
	}

	if (*args[1] == 'e' && strcmp(args[1], "edit") == 0)
		return kbinds_edit();

	if (*args[1] == 'r' && strcmp(args[1], "reset") == 0)
		return kbinds_reset();

	fputs(_("Usage: kb, keybinds [edit] [reset]\n"), stderr);
	return EXIT_FAILURE;
}

void
readline_kbinds(void)
{
/* To get the keyseq value for a given key do this in an Xterm terminal:
 * C-v and then press the key (or the key combination). So, for example,
 * C-v, C-right arrow gives "[[1;5C", which here should be written like
 * this:
 * "\\x1b[1;5C" */

			/* ##############################
			 * #        KEYBINDINGS         #
			 * ##############################*/
	if (KBINDS_FILE) {
		/* Help */
		rl_bind_keyseq(find_key("show-manpage"), rl_manpage);
		rl_bind_keyseq(find_key("show-cmds"), rl_cmds_help);
		rl_bind_keyseq(find_key("show-kbinds"), rl_kbinds_help);

		/* Navigation */
		/* Define multiple keybinds for different terminals:
		 * rxvt, xterm, kernel console */
/*      rl_bind_keyseq("\\M-[D", rl_test); // Left arrow key
		rl_bind_keyseq("\\M-+", rl_test); */
		rl_bind_keyseq(find_key("parent-dir"), rl_parent_dir);
		rl_bind_keyseq(find_key("parent-dir2"), rl_parent_dir);
		rl_bind_keyseq(find_key("parent-dir3"), rl_parent_dir);
		rl_bind_keyseq(find_key("parent-dir4"), rl_parent_dir);
		rl_bind_keyseq(find_key("previous-dir"), rl_previous_dir);
		rl_bind_keyseq(find_key("previous-dir2"), rl_previous_dir);
		rl_bind_keyseq(find_key("previous-dir3"), rl_previous_dir);
		rl_bind_keyseq(find_key("previous-dir4"), rl_previous_dir);
		rl_bind_keyseq(find_key("next-dir"), rl_next_dir);
		rl_bind_keyseq(find_key("next-dir2"), rl_next_dir);
		rl_bind_keyseq(find_key("next-dir3"), rl_next_dir);
		rl_bind_keyseq(find_key("next-dir4"), rl_next_dir);
		rl_bind_keyseq(find_key("home-dir"), rl_home_dir);
		rl_bind_keyseq(find_key("home-dir2"), rl_home_dir);
		rl_bind_keyseq(find_key("home-dir3"), rl_home_dir);
		rl_bind_keyseq(find_key("root-dir"), rl_root_dir);
		rl_bind_keyseq(find_key("root-dir2"), rl_root_dir);
		rl_bind_keyseq(find_key("root-dir3"), rl_root_dir);

		rl_bind_keyseq(find_key("first-dir"), rl_first_dir);
		rl_bind_keyseq(find_key("last-dir"), rl_last_dir);

		rl_bind_keyseq(find_key("pinned-dir"), rl_pinned_dir);
		rl_bind_keyseq(find_key("workspace1"), rl_ws1);
		rl_bind_keyseq(find_key("workspace2"), rl_ws2);
		rl_bind_keyseq(find_key("workspace3"), rl_ws3);
		rl_bind_keyseq(find_key("workspace4"), rl_ws4);

		/* Operations on files */
		rl_bind_keyseq(find_key("bookmark-sel"), rl_bm_sel);
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
		rl_bind_keyseq(find_key("edit-color-scheme"), rl_open_cscheme);
		rl_bind_keyseq(find_key("open-config"), rl_open_config);
		rl_bind_keyseq(find_key("open-keybinds"), rl_open_keybinds);
		rl_bind_keyseq(find_key("open-bookmarks"), rl_open_bm_file);

		/* Settings */
		rl_bind_keyseq(find_key("clear-msgs"), rl_clear_msgs);
		rl_bind_keyseq(find_key("next-profile"), rl_next_profile);
		rl_bind_keyseq(find_key("previous-profile"), rl_previous_profile);
		rl_bind_keyseq(find_key("quit"), rl_quit);
		rl_bind_keyseq(find_key("lock"), rl_lock);
		rl_bind_keyseq(find_key("refresh-screen"), rl_refresh);
		rl_bind_keyseq(find_key("clear-line"), rl_clear_line);
		rl_bind_keyseq(find_key("toggle-hidden"), rl_hidden);
		rl_bind_keyseq(find_key("toggle-hidden2"), rl_hidden);
		rl_bind_keyseq(find_key("toggle-long"), rl_long);
		rl_bind_keyseq(find_key("toggle-light"), rl_light);
		rl_bind_keyseq(find_key("folders-first"), rl_folders_first);
		rl_bind_keyseq(find_key("sort-previous"), rl_sort_previous);
		rl_bind_keyseq(find_key("sort-next"), rl_sort_next);

		rl_bind_keyseq(find_key("new-instance"), rl_new_instance);
		rl_bind_keyseq(find_key("show-dirhist"), rl_dirhist);
		rl_bind_keyseq(find_key("bookmarks"), rl_bookmarks);
		rl_bind_keyseq(find_key("mountpoints"), rl_mountpoints);
		rl_bind_keyseq(find_key("selbox"), rl_selbox);

		/* Plugins */
		rl_bind_keyseq(find_key("plugin1"), rl_plugin1);
		rl_bind_keyseq(find_key("plugin2"), rl_plugin2);
		rl_bind_keyseq(find_key("plugin3"), rl_plugin3);
		rl_bind_keyseq(find_key("plugin4"), rl_plugin4);
	}

	/* If no kbinds file is found, set the defaults */
	else {
		/* Help */
		rl_bind_keyseq("\\eOP", rl_manpage);
		rl_bind_keyseq("\\eOQ", rl_cmds_help);
		rl_bind_keyseq("\\eOR", rl_kbinds_help);

		/* Navigation */
		rl_bind_keyseq("\\M-u", rl_parent_dir);
		rl_bind_keyseq("\\e[a", rl_parent_dir);
		rl_bind_keyseq("\\e[2A", rl_parent_dir);
		rl_bind_keyseq("\\e[1;2A", rl_parent_dir);
		rl_bind_keyseq("\\M-j", rl_previous_dir);
		rl_bind_keyseq("\\e[d", rl_previous_dir);
		rl_bind_keyseq("\\e[2D", rl_previous_dir);
		rl_bind_keyseq("\\e[1;2D", rl_previous_dir);
		rl_bind_keyseq("\\M-k", rl_next_dir);
		rl_bind_keyseq("\\e[c", rl_next_dir);
		rl_bind_keyseq("\\e[2C", rl_next_dir);
		rl_bind_keyseq("\\e[1;2C", rl_next_dir);
		rl_bind_keyseq("\\M-e", rl_home_dir);
		rl_bind_keyseq("\\e[7~", rl_home_dir);
		rl_bind_keyseq("\\e[H", rl_home_dir);
		rl_bind_keyseq("\\M-r", rl_root_dir);
		rl_bind_keyseq("\\e/", rl_root_dir);

		rl_bind_keyseq("\\C-\\M-j", rl_first_dir);
		rl_bind_keyseq("\\C-\\M-k", rl_last_dir);

		/* Operations on files */
		rl_bind_keyseq("\\C-\\M-b", rl_bm_sel);
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

		/* Config files */
		rl_bind_keyseq("\\e[17~", rl_open_mime);
		rl_bind_keyseq("\\e[18~", rl_open_jump_db);
		rl_bind_keyseq("\\e[19~", rl_open_cscheme);
		rl_bind_keyseq("\\e[20~", rl_open_keybinds);
		rl_bind_keyseq("\\e[21~", rl_open_config);
		rl_bind_keyseq("\\e[23~", rl_open_bm_file);

		/* Settings */
		rl_bind_keyseq("\\M-t", rl_clear_msgs);
/*      rl_bind_keyseq("", rl_next_profile);
		rl_bind_keyseq("", rl_previous_profile); */
		rl_bind_keyseq("\\e[24~", rl_quit);
		rl_bind_keyseq("\\M-o", rl_lock);
		rl_bind_keyseq("\\C-r", rl_refresh);
		rl_bind_keyseq("\\M-c", rl_clear_line);
		rl_bind_keyseq("\\M-i", rl_hidden);
		rl_bind_keyseq("\\M-.", rl_hidden);
		rl_bind_keyseq("\\M-l", rl_long);
		rl_bind_keyseq("\\M-y", rl_light);
		rl_bind_keyseq("\\M-f", rl_folders_first);
		rl_bind_keyseq("\\M-z", rl_sort_previous);
		rl_bind_keyseq("\\M-x", rl_sort_next);

		rl_bind_keyseq("\\C-x", rl_new_instance);
		rl_bind_keyseq("\\M-h", rl_dirhist);
		rl_bind_keyseq("\\M-b", rl_bookmarks);
		rl_bind_keyseq("\\M-m", rl_mountpoints);
		rl_bind_keyseq("\\M-s", rl_selbox);
	}
}

int
load_keybinds(void)
/* Store keybinds from the keybinds file into a struct */
{
	if (!config_ok)
		return EXIT_FAILURE;

	/* Free the keybinds struct array */
	if (kbinds_n) {
		int i = (int)kbinds_n;

		while (--i >= 0) {
			free(kbinds[i].function);
			free(kbinds[i].key);
		}

		free(kbinds);

		kbinds = (struct kbinds_t *)xnmalloc(1, sizeof(struct kbinds_t));

		kbinds_n = 0;
	}

	/* Open the keybinds file */
	FILE *fp = fopen(KBINDS_FILE, "r");

	if (!fp)
		return EXIT_FAILURE;

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
		kbinds = xrealloc(kbinds, (kbinds_n + 1) * sizeof(struct kbinds_t));

		kbinds[kbinds_n].key = savestring(tmp + 1, strlen(tmp + 1));

		*tmp = '\0';

		kbinds[kbinds_n++].function = savestring(line, strlen(line));
	}

	free(line);

	return EXIT_SUCCESS;
}

char *
find_key(char *function)
{
	if (!kbinds_n)
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

int
keybind_exec_cmd(char *str)
/* Runs any command recognized by CliFM via a keybind. Example:
 * keybind_exec_cmd("sel *") */
{
	size_t old_args = args_n;
	args_n = 0;

	int exit_status = EXIT_FAILURE;

	char **cmd = parse_input_str(str);
	putchar('\n');

	if (cmd) {

		exit_status = exec_cmd(cmd);

		/* While in the bookmarks or mountpoints screen, the kbind_busy
		 * flag will be set to 1 and no keybinding will work. Once the
		 * corresponding function exited, set the kbind_busy flag to zero,
		 * so that keybindings work again */
		if (kbind_busy)
			kbind_busy = 0;

		int i = (int)args_n + 1;
		while (--i >= 0)
			free(cmd[i]);
		free(cmd);

		/* This call to prompt() just updates the prompt in case it was
		 * modified, for example, in case of chdir, files selection, and
		 * so on */
		char *buf = prompt();
		free(buf);
	}

	args_n = old_args;

	return exit_status;
}

int
rl_refresh(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	if (clear_screen)
		CLEAR;
	keybind_exec_cmd("rf");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_parent_dir(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	/* If already root dir, do nothing */
	if (*ws[cur_ws].path == '/' && !ws[cur_ws].path[1])
		return EXIT_SUCCESS;

	keybind_exec_cmd("cd ..");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_root_dir(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	/* If already root dir, do nothing */
	if (*ws[cur_ws].path == '/' && !ws[cur_ws].path[1])
		return EXIT_SUCCESS;

	keybind_exec_cmd("cd /");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_home_dir(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	/* If already in home, do nothing */
	if (*ws[cur_ws].path == *user_home
	&& strcmp(ws[cur_ws].path, user_home) == 0)
		return EXIT_SUCCESS;

	keybind_exec_cmd("cd");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_next_dir(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	/* If already at the end of dir hist, do nothing */
	if (dirhist_cur_index + 1 == dirhist_total_index)
		return EXIT_SUCCESS;

	keybind_exec_cmd("f");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_first_dir(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	/* If already at the beginning of dir hist, do nothing */
	if (dirhist_cur_index == 0)
		return EXIT_SUCCESS;

	keybind_exec_cmd("b !1");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_last_dir(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	/* If already at the end of dir hist, do nothing */
	if (dirhist_cur_index + 1 == dirhist_total_index)
		return EXIT_SUCCESS;

	char cmd[PATH_MAX + 4];
	sprintf(cmd, "b !%d", dirhist_total_index);
	keybind_exec_cmd(cmd);

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_previous_dir(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	/* If already at the beginning of dir hist, do nothing */
	if (dirhist_cur_index == 0)
		return EXIT_SUCCESS;

	keybind_exec_cmd("b");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_long(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	long_view = long_view ? 0 : 1;

	if (clear_screen)
		CLEAR;

	keybind_exec_cmd("rf");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_folders_first(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	list_folders_first = list_folders_first ? 0 : 1;

	if (cd_lists_on_the_fly) {
		if (clear_screen)
			CLEAR;
		free_dirlist();
		/* Without this putchar(), the first entries of the directories
		 * list are printed in the prompt line */
		putchar('\n');
		list_dir();
	}

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_light(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	light_mode = light_mode ? 0 : 1;

	if (clear_screen)
		CLEAR;

	keybind_exec_cmd("rf");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_hidden(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	show_hidden = show_hidden ? 0 : 1;

	if (cd_lists_on_the_fly) {
		if (clear_screen)
			CLEAR;
		free_dirlist();
		putchar('\n');
		list_dir();
	}

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_open_config(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("edit");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_open_keybinds(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("kb edit");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_open_cscheme(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("cs e");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_open_bm_file(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("bm edit");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_open_jump_db(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("je");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_open_mime(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("mm edit");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_mountpoints(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	/* Call the function only if it's not already running */
	kbind_busy = 1;
	keybind_exec_cmd("mp");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_select_all(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("s ^");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_deselect_all(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("ds *");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_bookmarks(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	kbind_busy = 1;
	keybind_exec_cmd("bm");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_selbox(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("ds");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_clear_line(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	/* 1) Clear text typed so far (\x1b[2K) and move cursor to the
	 * beginning of the current line (\r) */
	write(STDOUT_FILENO, "\x1b[2K\r", 5);

	/* 2) Clear the readline buffer */
	rl_delete_text(0, rl_end);
	rl_end = rl_point = 0;

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_sort_next(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	sort++;
	if (sort > SORT_TYPES)
		sort = 0;

	if (cd_lists_on_the_fly) {
		if (clear_screen)
			CLEAR;
		sort_switch = 1;
		free_dirlist();
		putchar('\n');
		list_dir();
		sort_switch = 0;
	}

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_sort_previous(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	sort--;
	if (sort < 0)
		sort = SORT_TYPES;

	if (cd_lists_on_the_fly) {
		if (clear_screen)
			CLEAR;
		sort_switch = 1;
		free_dirlist();
		putchar('\n');
		list_dir();
		sort_switch = 0;
	}

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_lock(int count, int key)
{
	int ret = EXIT_SUCCESS;

	rl_deprep_terminal();

#if __FreeBSD__
	char *cmd[] = { "lock", NULL };
	ret = launch_execve(cmd, FOREGROUND, E_NOFLAG);
#elif __linux__
	char *cmd[] = { "vlock", NULL };
	ret = launch_execve(cmd, FOREGROUND, E_NOFLAG);
#endif

	rl_prep_terminal(0);
	rl_reset_line_state();

	if (ret != EXIT_SUCCESS)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

int
rl_remove_sel(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	rl_deprep_terminal();

	kb_shortcut = 1;
	keybind_exec_cmd("r sel");
	kb_shortcut = 0;

	rl_prep_terminal(0);
	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_export_sel(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	kb_shortcut = 1;
	keybind_exec_cmd("exp sel");
	kb_shortcut = 0;

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_move_sel(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	kb_shortcut = 1;
	keybind_exec_cmd("m sel");
	kb_shortcut = 0;

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_rename_sel(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	kb_shortcut = 1;
	keybind_exec_cmd("br sel");
	kb_shortcut = 0;

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_paste_sel(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	kb_shortcut = 1;
	keybind_exec_cmd("c sel");
	kb_shortcut = 0;

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_quit(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

//  keybind_exec_cmd("q");

	return EXIT_SUCCESS;
}

int
rl_previous_profile(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	int prev_prof, i, cur_prof = -1, total_profs = 0;
	for (i = 0; profile_names[i]; i++) {
		total_profs++;

		if (!alt_profile) {
			if (*profile_names[i] == 'd'
			&& strcmp(profile_names[i], "default") == 0) {
				cur_prof = i;
			}
		}

		else {
			if (*alt_profile == *profile_names[i]
			&& strcmp(alt_profile, profile_names[i]) == 0) {
				cur_prof = i;
			}
		}
	}

	if (cur_prof == -1 || !profile_names[cur_prof])
		return EXIT_FAILURE;

	prev_prof = cur_prof - 1;
	total_profs--;

	if (prev_prof < 0 || !profile_names[prev_prof])
		prev_prof = total_profs;

	if (clear_screen) {
		CLEAR;
	}
	else
		putchar('\n');

	if (profile_set(profile_names[prev_prof]) == EXIT_SUCCESS) {
		printf(_("%s->%s Switched to profile '%s'\n"), mi_c, df_c,
			   profile_names[prev_prof]);
		char *input = prompt();
		free(input);
	}

	return EXIT_SUCCESS;
}

int
rl_next_profile(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	int next_prof, i, cur_prof = -1, total_profs = 0;
	for (i = 0; profile_names[i]; i++) {
		total_profs++;

		if (!alt_profile) {
			if (*profile_names[i] == 'd'
			&& strcmp(profile_names[i], "default") == 0) {
				cur_prof = i;
			}
		}

		else {
			if (*alt_profile == *profile_names[i]
			&& strcmp(alt_profile, profile_names[i]) == 0) {
				cur_prof = i;
			}
		}
	}

	if (cur_prof == -1 || !profile_names[cur_prof])
		return EXIT_FAILURE;

	next_prof = cur_prof + 1;
	total_profs--;

	if (next_prof > (int)total_profs || !profile_names[next_prof])
		next_prof = 0;

	if (clear_screen) {
		CLEAR;
	}
	else
		putchar('\n');

	if (profile_set(profile_names[next_prof]) == EXIT_SUCCESS) {
		printf(_("%s->%s Switched to profile '%s'\n"), mi_c, df_c,
			   profile_names[next_prof]);
		char *input = prompt();
		free(input);
	}

	return EXIT_SUCCESS;
}

int
rl_dirhist(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("bh");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_archive_sel(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("ac sel");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_new_instance(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("x .");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_clear_msgs(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("msg clear");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_trash_sel(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("t sel");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_untrash_all(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("u *");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_open_sel(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	if (sel_n == 0 || !sel_elements[sel_n - 1]) {
		fprintf(stderr, _("\n%s: No selected files\n"), PROGRAM_NAME);
		rl_reset_line_state();
		return EXIT_FAILURE;
	}

	char cmd[PATH_MAX + 3];
	sprintf(cmd, "o %s", sel_elements[sel_n -  1]);

	keybind_exec_cmd(cmd);

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_bm_sel(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	if (sel_n == 0 || !sel_elements[sel_n - 1]) {
		fprintf(stderr, _("\n%s: No selected files\n"), PROGRAM_NAME);
		rl_reset_line_state();
		return EXIT_FAILURE;
	}

	char cmd[PATH_MAX + 6];
	sprintf(cmd, "bm a %s", sel_elements[sel_n -  1]);

	keybind_exec_cmd(cmd);

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_kbinds_help (int count, int key)
{
	char *cmd[] = { "man", "-P", "less -p ^\"KEYBOARD SHORTCUTS\"", PNL,
					NULL };
	if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

int
rl_cmds_help (int count, int key)
{
	char *cmd[] = { "man", "-P", "less -p ^COMMANDS", PNL, NULL };
	if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

int
rl_manpage (int count, int key)
{
	char *cmd[] = { "man", PNL, NULL };

	if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

int
rl_pinned_dir(int count, int key)
{
	if (!pinned_dir) {
		printf(_("%s: No pinned file\n"), PROGRAM_NAME);
		return EXIT_SUCCESS;
	}

	keybind_exec_cmd(",");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_ws1(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("ws 1");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_ws2(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("ws 2");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_ws3(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("ws 3");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_ws4(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("ws 4");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_plugin1(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("plugin1");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_plugin2(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("plugin2");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_plugin3(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("plugin3");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}

int
rl_plugin4(int count, int key)
{
	if (kbind_busy)
		return EXIT_SUCCESS;

	keybind_exec_cmd("plugin4");

	rl_reset_line_state();

	return EXIT_SUCCESS;
}
