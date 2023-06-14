/*
 * SPDX-FileCopyrightText: 2016-2022 L. Abramovich <leo.clifm@outlook.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

			/*  ########################################
			 *  #               CliFM                  #
			 *  # 	  The command line file manager    #
			 *  ######################################## */

/* Copyright (C) 2016-2023, L. Abramovich <leo.clifm@outlook.com>
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

#include <errno.h>
#include <langinfo.h> /* nl_langinfo() */
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "aux.h"
#include "checks.h"
#include "config.h"
#include "exec.h"
#include "history.h"
#include "init.h"
#include "jump.h"
#include "keybinds.h"
#include "listing.h"
#include "misc.h"
#ifndef _NO_PROFILES
# include "profiles.h"
#endif
#include "prompt.h"
#include "readline.h"
#include "remotes.h"

/* Globals */

struct usrvar_t *usr_var = (struct usrvar_t *)NULL;
struct actions_t *usr_actions = (struct actions_t *)NULL;
struct ws_t *workspaces = (struct ws_t *)NULL;
struct kbinds_t *kbinds = (struct kbinds_t *)NULL;
struct jump_t *jump_db = (struct jump_t *)NULL;
struct bookmarks_t *bookmarks = (struct bookmarks_t *)NULL;
struct fileinfo *file_info = (struct fileinfo *)NULL;
struct remote_t *remotes = (struct remote_t *)NULL;
struct alias_t *aliases = (struct alias_t *)NULL;
struct user_t user;
/* Store device and inode number of selected files */
struct devino_t *sel_devino = (struct devino_t *)NULL;
#ifndef _NO_SUGGESTIONS
struct suggestions_t suggestion;
#endif
struct stats_t stats;
struct autocmds_t *autocmds = (struct autocmds_t *)NULL;
struct opts_t opts;
struct opts_t workspace_opts[MAX_WS];
struct sel_t *sel_elements = (struct sel_t *)NULL;
struct prompts_t *prompts = (struct prompts_t *)NULL;
struct history_t *history = (struct history_t *)NULL;
struct msgs_t msgs;
struct props_t prop_fields;
struct termcaps_t term_caps;
struct filter_t filter;
struct config_t conf;
struct shades_t date_shades;
struct shades_t size_shades;
struct paths_t *paths = (struct paths_t *)NULL;
struct ext_t *ext_colors = (struct ext_t *)NULL;

struct sort_t _sorts[] = {
    {"none", 0, 0},
    {"name", 1, 0},
    {"size", 2, 0},
    {"atime", 3, 0},
    {"btime", 4, 0},
    {"ctime", 5, 0},
    {"mtime", 6, 0},
    {"version", 7, 0},
    {"extension", 8, 0},
    {"inode", 9, 0},
    {"owner", 10, 0},
    {"group", 11, 0},
    {NULL, 12, 0},
};

/* pmsg holds the current program message type */
enum prog_msg pmsg = NOMSG;
enum comp_type cur_comp_type = TCMP_NONE;
enum tab_mode tabmode = STD_TAB;

struct param_t xargs;
unsigned short term_cols;

/* Bit flag holders */
int
	flags = 0,
	bin_flags = 0,
	search_flags = 0;

struct termios
	orig_termios,
	shell_tmodes;

pid_t own_pid = 0;
time_t props_now = 0;

unsigned short
	term_cols = 0,
	term_lines = 0;

regex_t regex_exp;

/* Internal status flags */
int
	argc_bk = 0,
	autocmd_set = 0,
	autojump = UNSET,
	bell = UNSET,
	bg_proc = 0,
	check_cap = UNSET,
	check_ext = UNSET,
	cmdhist_flag = 0,
	config_ok = 1,
	control_d_exits = 0,
	cur_ws = UNSET,
	curcol = 0,
	dequoted = 0,
	dir_changed = 0,
	dir_out = 0,
	dirhist_cur_index = 0,
	dirhist_total_index = 0,
	exit_code = 0,
	follow_symlinks = UNSET,
	fzftab = UNSET,
	fzf_height_set = 0,
//	fzf_open_with = 0,
	fzf_preview_border_type = 0,
	hist_status = UNSET,
	home_ok = 1,
	int_vars = UNSET,
	internal_cmd = 0,
	is_sel = 0,
	jump_total_rank = 0,
	kbind_busy = 0,
	list_dirs_first = UNSET,
	max_files = UNSET,
	mime_match = 0,
	no_log = 0,
	open_in_foreground = 0,
	prev_ws = UNSET,
	print_msg = 0,
	print_selfiles = UNSET,
	print_removed_files = UNSET,
	prompt_offset = UNSET,
	prompt_notif = UNSET,
	recur_perm_error_flag = 0,
	rl_nohist = 0,
	rl_notab = 0,
	sel_is_last = 0,
	selfile_ok = 1,
	shell = SHELL_NONE,
	shell_is_interactive = 0,
	shell_terminal = 0,
	sort_switch = 0,
	switch_cscheme = 0,
#ifndef _NO_TRASH
	trash_ok = 1,
#endif
	wrong_cmd = 0,
	xrename = 0;

size_t
	actions_n = 0,
	aliases_n = 0,
	args_n = 0,
	autocmds_n = 0,
	bm_n = 0,
	cdpath_n = 0,
	config_dir_len = 0,
	cschemes_n = 0,
	current_hist_n = 0,
	curhistindex = 0,
	ext_colors_n = 0,
	files = 0,
	jump_n = 0,
	kbinds_n = 0,
	longest = 0,
	msgs_n = 0,
	P_tmpdir_len = 0,
	path_n = 0,
	path_progsn = 0,
	prompt_cmds_n = 0,
	prompts_n = 0,
	remotes_n = 0,
	sel_n = 0,
	tab_offset = 0,
	tags_n = 0,
	trash_n = 0,
	usrvar_n = 0,
	words_num = 0,
	zombies = 0;

#if !defined(_NO_ICONS)
size_t *name_icons_hashes = (size_t *)0;
size_t *dir_icons_hashes = (size_t *)0;
size_t *ext_icons_hashes = (size_t *)0;
#endif

char
	cur_prompt_name[NAME_MAX + 1] = "",
	div_line[NAME_MAX + 1],
	hostname[HOST_NAME_MAX + 1],
#ifndef _NO_FZF
	finder_in_file[PATH_MAX + 1],
	finder_out_file[PATH_MAX + 1],
#endif /* _NO_FZF */
	_fmatch[PATH_MAX + 1],
	prop_fields_str[PROP_FIELDS_SIZE + 1] = "",
	invalid_time_str[MAX_TIME_STR] = "",

#ifdef RUN_CMD
	*cmd_line_cmd = (char *)NULL,
#endif

	*actions_file = (char *)NULL,
	*alt_bm_file = (char *)NULL,
	*alt_config_dir = (char *)NULL,
	*alt_config_file = (char *)NULL,
	*alt_kbinds_file = (char *)NULL,
	*alt_preview_file = (char *)NULL,
	*alt_profile = (char *)NULL,
	*bin_name = (char *)NULL,
	*bm_file = (char *)NULL,
	*cmds_log_file = (char *)NULL,
	*colors_dir = (char *)NULL,
	*config_dir = (char *)NULL,
	*config_dir_gral = (char *)NULL,
	*config_file = (char *)NULL,
	*cur_color = (char *)NULL,
	*cur_tag = (char *)NULL,
	*data_dir = (char *)NULL,
	*cur_cscheme = (char *)NULL,
	*dirhist_file = (char *)NULL,
	*file_cmd_path = (char *)NULL,
	*hist_file = (char *)NULL,
	*jump_suggestion = (char *)NULL,
	*kbinds_file = (char *)NULL,
	*last_cmd = (char *)NULL,
	*mime_file = (char *)NULL,
	*msgs_log_file = (char *)NULL,
	*pinned_dir = (char *)NULL,
	*plugins_dir = (char *)NULL,
	*profile_file = (char *)NULL,
	*prompts_file = (char *)NULL,
	*quote_chars = (char *)NULL,
	*rl_callback_handler_input = (char *)NULL,
	*remotes_file = (char *)NULL,
/*	*right_prompt = (char *)NULL, */
	*sel_file = (char *)NULL,
	*smenutab_options_env = (char *)NULL,
	*stdin_tmp_dir = (char *)NULL,
	*sudo_cmd = (char *)NULL,
#ifndef _NO_SUGGESTIONS
	*suggestion_buf = (char *)NULL,
#endif
	*sys_shell = (char *)NULL,
	*tags_dir = (char *)NULL,
	*tmp_dir = (char *)NULL,
#ifndef _NO_TRASH
	*trash_dir = (char *)NULL,
	*trash_files_dir = (char *)NULL,
	*trash_info_dir = (char *)NULL,
#endif

	**argv_bk = (char **)NULL,
	**bin_commands = (char **)NULL,
	**cdpaths = (char **)NULL,
	**color_schemes = (char **)NULL,
	**messages = (char **)NULL,
	**old_pwd = (char **)NULL,
	**profile_names = (char **)NULL,
	**prompt_cmds = (char **)NULL,
	**tags = (char **)NULL;

/* Colors */
char
	/* File types */
	bd_c[MAX_COLOR], /* Block device */
	ca_c[MAX_COLOR], /* Cap file */
	cd_c[MAX_COLOR], /* Char device */
	di_c[MAX_COLOR], /* Directory */
	ed_c[MAX_COLOR], /* Empty dir */
	ee_c[MAX_COLOR], /* Empty executable */
	ef_c[MAX_COLOR], /* Empty reg file */
	ex_c[MAX_COLOR], /* Executable */
	fi_c[MAX_COLOR], /* Reg file */
	ln_c[MAX_COLOR], /* Symlink */
	mh_c[MAX_COLOR], /* Multi-hardlink file */
	nd_c[MAX_COLOR], /* No read directory */
	nf_c[MAX_COLOR], /* No read file */
	no_c[MAX_COLOR], /* Unknown */
	or_c[MAX_COLOR], /* Broken symlink */
	ow_c[MAX_COLOR], /* Other writable */
	pi_c[MAX_COLOR], /* FIFO, pipe */
	sg_c[MAX_COLOR], /* SGID file */
	so_c[MAX_COLOR], /* Socket */
	st_c[MAX_COLOR], /* Sticky (not ow)*/
	su_c[MAX_COLOR], /* SUID file */
	tw_c[MAX_COLOR], /* Sticky other writable */
	uf_c[MAX_COLOR], /* Non-'stat'able file */

	/* Interface */
	bm_c[MAX_COLOR], /* Bookmarked directory */
	fc_c[MAX_COLOR], /* Files counter */
	df_c[MAX_COLOR], /* Default color */
	dl_c[MAX_COLOR], /* Dividing line index */
	el_c[MAX_COLOR], /* ELN */
	lc_c[MAX_COLOR], /* Symlink character (ColorLinkAsTarget only) */
	mi_c[MAX_COLOR], /* Misc indicators */
	ts_c[MAX_COLOR], /* TAB completion suffix */
	wc_c[MAX_COLOR], /* Welcome message color */
	wp_c[MAX_COLOR], /* Warning prompt */
	tt_c[MAX_COLOR], /* Tilde for trimmed files */

	/* Suggestions */
	sb_c[MAX_COLOR], /* Auto-suggestions: shell builtins */
	sc_c[MAX_COLOR], /* Auto-suggestions: external commands */
	sd_c[MAX_COLOR], /* Auto-suggestions: internal commands description */
	sh_c[MAX_COLOR], /* Auto-suggestions: history */
	sf_c[MAX_COLOR], /* Auto-suggestions: file names */
	sx_c[MAX_COLOR], /* Auto-suggestions: internal commands and params */
	sp_c[MAX_COLOR], /* Auto-suggestions: suggestions pointer */
	sz_c[MAX_COLOR], /* Auto-suggestions: file names (fuzzy) */

#ifndef _NO_ICONS
	dir_ico_c[MAX_COLOR], /* Directories icon color */
#endif

	/* Syntax highlighting */
	hb_c[MAX_COLOR], /* Brackets: () [] {} */
	hc_c[MAX_COLOR], /* Comments */
	hd_c[MAX_COLOR], /* Paths (slashes) */
	he_c[MAX_COLOR], /* Expansion operators: * ~ */
	hn_c[MAX_COLOR], /* Numbers */
	hp_c[MAX_COLOR], /* Parameters: - */
	hq_c[MAX_COLOR], /* Quoted strings */
	hr_c[MAX_COLOR], /* Redirection: > */
	hs_c[MAX_COLOR], /* Process separators: | & ; */
	hv_c[MAX_COLOR], /* Variables: $ */
	hw_c[MAX_COLOR], /* Wrong, non-existent command name */

	dr_c[MAX_COLOR],  /* Read */
	dw_c[MAX_COLOR],  /* Write */
	dxd_c[MAX_COLOR], /* Execute (dirs) */
	dxr_c[MAX_COLOR], /* Execute (reg files) */
	dg_c[MAX_COLOR],  /* UID, GID */
	dd_c[MAX_COLOR],  /* Date */
	dz_c[MAX_COLOR],  /* Size (dirs) > */
	do_c[MAX_COLOR],  /* Octal representation > */
	dp_c[MAX_COLOR],  /* Special files (SUID, SGID, etc) */
	dn_c[MAX_COLOR],  /* Dash (none) */

	/* Colors used in the prompt, so that \001 and \002 needs to
	 * be added. This is why MAX_COLOR + 2 */
	/* Workspaces */
	ws1_c[MAX_COLOR + 2],
	ws2_c[MAX_COLOR + 2],
	ws3_c[MAX_COLOR + 2],
	ws4_c[MAX_COLOR + 2],
	ws5_c[MAX_COLOR + 2],
	ws6_c[MAX_COLOR + 2],
	ws7_c[MAX_COLOR + 2],
	ws8_c[MAX_COLOR + 2],

	em_c[MAX_COLOR + 2], /* Error msg color */
	li_c[MAX_COLOR + 2], /* Sel indicator color */
	li_cb[MAX_COLOR], /* Sel indicator color (for the files list) */
	nm_c[MAX_COLOR + 2], /* Notice msg color */
	wm_c[MAX_COLOR + 2], /* Warning msg color */
	si_c[MAX_COLOR + 2], /* stealth indicator color */
	ti_c[MAX_COLOR + 2], /* Trash indicator color */
	tx_c[MAX_COLOR + 2], /* Text color */
	xs_c[MAX_COLOR + 2], /* Exit code: success */
	xf_c[MAX_COLOR + 2], /* Exit code: failure */
	tmp_color[MAX_COLOR + 2]; /* A temp buffer to store color codes */

/* A list of all internal commands, with short and long formats.
 * We use two more lists of commands: one of commands dealing with file names
 * (is_internal()), and another one listing commands taking ELN's/numbers
 * as parameters (is_internal_f()), both in checks.c */
const struct cmdslist_t internal_cmds[] = {
	{",", 1},
	{"?", 1},
	{"help", 4},
	{"ac", 2},
	{"ad", 2},
	{"acd", 3},
	{"autocd", 6},
	{"actions", 7},
	{"alias", 5},
	{"ao", 2},
	{"auto-open", 9},
	{"b", 1},
	{"back", 4},
	{"bb", 2},
	{"bleach", 6},
	{"bd", 2},
	{"bh", 2}, // REMOVE AS SOON AS REPLACED BY DH
	{"fh", 2}, // REMOVE AS SOON AS REPLACED BY DH
	{"dh", 2},
	{"bl", 2},
	{"bm", 2},
	{"bookmarks", 9},
	{"br", 2},
	{"bulk", 4},
	{"c", 1}, //"cp",
	{"colors", 6},
	{"cd", 2},
	{"cl", 2},
	{"columns", 7},
	{"cmd", 3},
	{"commands", 8},
	{"cs", 2},
	{"colorschemes", 12},
	{"cwd", 3}, // deprecate this one, just as 'path'
	{"d", 1},
	{"dup", 3},
	{"ds", 2},
	{"desel", 5},
	{"edit", 4}, /* Deprecated */
	{"config", 6},
	{"exp", 3},
	{"export", 6},
	{"ext", 3},
	{"f", 1},
	{"forth", 5},
	{"fc", 2},
	{"ff", 2},
	{"dirs-first", 10},
	{"fs", 2},
	{"ft", 2},
	{"filter", 6},
	{"fz", 2},
	{"hh", 2},
	{"hf", 2},
	{"hidden", 6},
	{"history", 7},
	{"icons", 5},
	{"j", 1},
	{"jump", 4},
	{"je", 2},
	{"jc", 2},
	{"jl", 2},
	{"jp", 2},
//	{"jo", 2},
	{"kb", 2},
	{"keybinds", 8},
	{"l", 1},
	{"le", 2},
	{"lm", 2},
	{"log", 3},
	{"ll", 2},
	{"lv", 2},
	{"m", 1},
	{"md", 2},
	{"media", 5},
	{"mf", 2},
	{"mm", 2},
	{"mime", 4},
	{"mp", 2},
	{"mountpoints", 11},
	{"msg", 3},
	{"messages", 8},
	{"n", 1},
	{"new", 3},
	{"net", 3},
	{"o", 1},
	{"oc", 2},
	{"open", 4},
	{"ow", 2},
	{"opener", 6},
	{"p", 1},
	{"pc", 2},
	{"pp", 2},
	{"pr", 2},
	{"prop", 4},
	{"path", 4}, // deprecate this one, just as 'cwd'
	{"paste", 5},
	{"pf", 2},
	{"prof", 4},
	{"profile", 7},
	{"pg", 2},
	{"pager", 5},
	{"pin", 3},
	{"unpin", 5},
	{"prompt", 6},
	{"pwd", 3},
	{"q", 1},
	{"Q", 1},
	{"quit", 4},
	{"exit", 4},
	{"r", 1}, //"rm",
	{"rf", 2},
	{"refresh", 7},
	{"rl", 2},
	{"reload", 6},
	{"rr", 2},
	{"s", 1},
	{"sel", 3},
	{"sb", 2},
	{"selbox", 6},
	{"splash", 6},
	{"st", 2},
	{"sort", 4},
	{"stats", 5},
	{"t", 1},
	{"tr", 2},
	{"trash", 5},
	{"tag", 3},
	{"ta", 2},
	{"td", 2},
	{"tl", 2},
	{"tm", 2},
	{"tn", 2},
	{"tu", 2},
	{"ty", 2},
	{"te", 2},
	{"tips", 4},
	{"u", 1},
	{"undel", 5},
	{"unset", 5},
	{"untrash", 7},
	{"umask", 5},
	{"v", 1},
	{"vv", 2},
	{"ver", 3},
	{"version", 7},
	{"view", 4},
	{"ws", 2},
	{"x", 1},
	{"X", 1},
	{NULL, 0}
};
size_t internal_cmds_n = 0;

/* A list of internal commands and fixed parameters for the auto-suggestions
 * system */
const struct cmdslist_t param_str[] = {
	{"actions edit", 12},
	{"actions list", 12},
	{"autocd on", 9},
	{"acd on", 6},
	{"autocd off", 10},
	{"acd off", 7},
	{"autocd status", 13},
	{"acd status", 10},
	{"alias import", 12},
	{"alias list", 10},
	{"ao on", 5},
	{"auto-open on", 12},
	{"ao off", 6},
	{"auto-open off", 13},
	{"ao status", 9},
	{"auto-open status", 16},
	{"b hist", 6},
	{"b clear", 7},
	{"back hist", 9},
	{"back clear", 10},
	{"bm add", 6},
	{"bm del", 6},
	{"bm edit", 7},
	{"bookmarks add", 13},
	{"bookmarks del", 13},
	{"bookmarks edit", 14},
	{"c -f", 4},
	{"c --force", 9},
	{"colorschemes edit", 17},
	{"cs edit", 7},
	{"desel all", 9},
	{"ds all", 6},
///////// Deprecated
	{"edit", 4},
	{"edit reset", 10},
	{"edit dump", 9},
/////////
	{"config", 6},
	{"config edit", 11},
	{"config dump", 11},
	{"config reset", 12},
	{"ext on", 6},
	{"ext off", 7},
	{"ext status", 10},
	{"f hist", 6},
	{"f clear", 7},
	{"forth hist", 10},
	{"forth clear", 11},
	{"fc on", 5},
	{"fc off", 6},
	{"fc status", 9},
	{"ff on", 5},
	{"dirs-first on", 13},
	{"ff off", 6},
	{"dirs-first off", 14},
	{"ff status", 9},
	{"dirs-first status", 17},
	{"ft unset", 8},
	{"filter unset", 12},
	{"fz on", 5},
	{"fz off", 6},
	{"help archives", 13},
	{"help autocommands", 17},
	{"help basics", 11},
	{"help bookmarks", 14},
	{"help commands", 13},
	{"help desktop-notifications", 26},
	{"help dir-jumper", 15},
	{"help file-details", 17},
	{"help file-filters", 17},
	{"help file-previews", 18},
	{"help file-tags", 14},
	{"help navigation", 15},
	{"help plugins", 12},
	{"help profiles", 13},
	{"help remotes", 12},
	{"help resource-opener", 20},
	{"help search", 11},
	{"help security", 13},
	{"help selection", 14},
	{"help theming", 12},
	{"help trash", 10},
	{"hf on", 5},
	{"hf off", 6},
	{"hf status", 9},
	{"hh on", 5},
	{"hh off", 6},
	{"hh status", 9},
	{"hidden on", 9},
	{"hidden off", 10},
	{"hidden status", 13},
	{"history clear", 13},
	{"history edit", 12},
	{"history on", 10},
	{"history off", 11},
	{"history status", 14},
	{"history show-time", 17},
	{"icons on", 8},
	{"icons off", 9},
	{"j --purge", 9},
	{"j --edit", 8},
	{"kb edit", 7},
	{"keybinds edit", 13},
	{"kb list", 7},
	{"keybinds list", 13},
	{"kb reset", 8},
	{"keybinds reset", 14},
	{"kb readline", 11},
	{"keybinds readline", 17},
	{"l edit", 6},
	{"lm on", 5},
	{"lm off", 6},
	{"ll on", 5},
	{"ll off", 6},
	{"lv on", 5},
	{"lv off", 6},
	{"log cmd list", 12},
	{"log cmd on", 10},
	{"log cmd off", 11},
	{"log cmd status", 14},
	{"log cmd clear", 13},
	{"log msg list", 12},
	{"log msg on", 10},
	{"log msg off", 11},
	{"log msg status", 14},
	{"log msg clear", 13},
	{"m -f", 4},
	{"m --force", 9},
	{"mf unset", 8},
	{"mm info", 7},
	{"mm edit", 7},
	{"mm import", 9},
	{"mime info", 9},
	{"mime edit", 9},
	{"mime import", 11},
	{"msg clear", 9},
	{"messages clear", 14},
	{"net list", 8},
	{"net edit", 8},
	{"net mount", 9},
	{"net unmount", 11},
	{"opener default", 14},
	{"pg on", 5},
	{"pager on", 8},
	{"pg off", 6},
	{"pager off", 9},
	{"pg status", 9},
	{"pager status", 12},
	{"pf set", 6},
	{"pf add", 6},
	{"pf del", 6},
	{"pf list", 7},
	{"pf rename", 9},
	{"profile set", 11},
	{"profile add", 11},
	{"profile del", 11},
	{"profile list", 12},
	{"profile rename", 14},
	{"prompt edit", 11},
	{"prompt list", 11},
	{"prompt reload", 13},
	{"prompt set", 10},
	{"prompt unset", 12},
	{"r -f", 4},
	{"r --force", 9},
	{"st none", 7},
	{"st name", 7},
	{"st size", 7},
	{"st atime", 8},
	{"st btime", 8},
	{"st ctime", 8},
	{"st mtime", 8},
	{"st owner", 8},
	{"st group", 8},
	{"st extension", 12},
	{"st inode", 8},
	{"st version", 10},
	{"sort none", 9},
	{"sort name", 9},
	{"sort size", 9},
	{"sort atime", 10},
	{"sort btime", 10},
	{"sort ctime", 10},
	{"sort mtime", 10},
	{"sort owner", 10},
	{"sort group", 10},
	{"sort extension", 14},
	{"sort inode", 10},
	{"sort version", 12},
	{"st rev", 6},
	{"sort rev", 8},
	{"t list", 6},
	{"t clear", 7},
	{"t empty", 7},
	{"t del", 5},
	{"trash list", 10},
	{"trash clear", 11},
	{"trash empty", 11},
	{"trash del", 9},
	{"tag add", 7},
	{"tag del", 7},
	{"tag list", 8},
	{"tag list-full", 13},
	{"tag merge", 9},
	{"tag new", 7},
	{"tag rename", 10},
	{"tag untag", 9},
	{"u all", 5},
	{"undel all", 9},
	{"untrash all", 11},
	{"view edit", 9},
	{NULL, 0}
};


#ifdef LINUX_INOTIFY
int inotify_fd = UNSET, inotify_wd = UNSET;
unsigned int INOTIFY_MASK =
	IN_CREATE | IN_DELETE | IN_DELETE_SELF | IN_MOVE | IN_MOVE_SELF
/*#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0)
	| IN_DONT_FOLLOW | IN_EXCL_UNLINK | IN_ONLYDIR | IN_MASK_CREATE;
#else */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 15)
	| IN_DONT_FOLLOW | IN_ONLYDIR
#endif /* LINUX >= 2.6.15 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
	| IN_EXCL_UNLINK
#endif /* LINUX >= 2.6.36 */
	;
#elif defined(BSD_KQUEUE)
int kq, event_fd = UNSET;
struct kevent events_to_monitor[NUM_EVENT_FDS];
unsigned int KQUEUE_FFLAGS = NOTE_DELETE | NOTE_EXTEND| NOTE_LINK
	| NOTE_RENAME | NOTE_REVOKE | NOTE_WRITE;
struct timespec timeout;
#endif
int watch = UNSET;

#ifdef RUN_CMD
// Run the command passed via --cmd and exit
static void
run_and_exit(void)
{
	/* 1) Parse input string */
	int i = 0;
	char **cmd = parse_input_str(cmd_line_cmd);
	if (!cmd)
		exit(EXIT_FAILURE);

	/* 2) Execute input string */
	char **alias_cmd = check_for_alias(cmd);
	if (alias_cmd) {
		/* If an alias is found, check_for_alias() frees CMD and returns
		 * ALIAS_CMD in its place to be executed by exec_cmd() */
		exec_cmd(alias_cmd);

		for (i = 0; alias_cmd[i]; i++)
			free(alias_cmd[i]);
		free(alias_cmd);
		exit(exit_code);
	}

	if (!(flags & FAILED_ALIAS))
		exec_cmd(cmd);
	flags &= ~FAILED_ALIAS;

	for (i = 0; cmd[i]; i++)
		free(cmd[i]);
	free(cmd);

	UNHIDE_CURSOR;
	exit(exit_code);
}
#endif // RUN_CMD

/* This is the main structure of any basic shell (a REPL)
	 1 - Grab user input
	 2 - Parse user input
	 3 - Execute command
	 4 - Grab user input again
	 See https://brennan.io/2015/01/16/write-a-shell-in-c/
*/
static inline void
run_main_loop(void)
{
#ifdef RUN_CMD
	if (cmd_line_cmd)
		run_and_exit();
#endif

	int i;
	/* 1) Infinite loop to keep the program running */
	while (1) {
		/* 2) Grab input string from the prompt */
		char *input = prompt();
		if (!input)
			continue;

		/* 3) Parse input string */
		char **cmd = parse_input_str(input);
		free(input);
		input = (char *)NULL;

		if (!cmd)
			continue;

		/* 4) Execute input string */
		char **alias_cmd = check_for_alias(cmd);
		if (alias_cmd) {
			/* If an alias is found, check_for_alias() frees CMD and returns
			 * ALIAS_CMD in its place to be executed by exec_cmd() */
			exec_cmd(alias_cmd);

			for (i = 0; alias_cmd[i]; i++)
				free(alias_cmd[i]);
			free(alias_cmd);
			continue;
		}

		if (!(flags & FAILED_ALIAS))
			exec_cmd(cmd);
		flags &= ~FAILED_ALIAS;

		i = (int)args_n + 1;
		while (--i >= 0)
			free(cmd[i]);
		free(cmd);
	}
}

static inline void
set_root_indicator(void)
{
	if (user.uid == 0) {
		_err(ERR_NO_LOG, PRINT_PROMPT, _("%s->%s Running as root%s\n"),
			conf.colorize == 1 ? mi_c : "", conf.colorize == 1 ? _RED : "",
			conf.colorize == 1 ? df_c : "");
	}
}

static inline void
_list(void)
{
#ifdef RUN_CMD
	if (cmd_line_cmd)
		return;
#endif

	if (conf.autols == 1 && isatty(STDIN_FILENO)) {
#ifdef LINUX_INOTIFY
		/* Initialize inotify */
		inotify_fd = inotify_init1(IN_NONBLOCK);
		if (inotify_fd < 0) {
			_err('w', PRINT_PROMPT, "%s: inotify: %s\n",
				PROGRAM_NAME, strerror(errno));
		}
#elif defined(BSD_KQUEUE)
		kq = kqueue();
		if (kq < 0) {
			_err('w', PRINT_PROMPT, "%s: kqueue: %s\n",
				PROGRAM_NAME, strerror(errno));
		}
#endif

		if (conf.colorize == 1 && xargs.eln_use_workspace_color == 1)
			set_eln_color();

		list_dir();
	}
}

static inline void
_splash(void)
{
	if (conf.splash_screen) {
		splash();
		conf.splash_screen = 0;
		CLEAR;
	}
}

/* Set terminal window title */
static inline void
_set_term_title(void)
{
	if (!(flags & GUI) || xargs.list_and_quit == 1 || xargs.vt100 == 1)
		return;

	if (xargs.cwd_in_title == 0) {
		printf("\033]2;%s\007", PROGRAM_NAME);
		fflush(stdout);
	} else {
		set_term_title(workspaces[cur_ws].path);
	}
}

static inline void
check_working_directory(void)
{
	if (workspaces == (struct ws_t *)NULL || !workspaces[cur_ws].path
	|| !*workspaces[cur_ws].path) {
		_err(0, NOPRINT_PROMPT, _("%s: Fatal error! Failure "
			"retrieving current working directory\n"), PROGRAM_NAME);
		exit(EXIT_FAILURE);
	}
}

/* Check whether we have a working shell */
/*
static inline void
check_working_shell(void)
{
	if (access(user.shell, X_OK) == -1) {
		_err('w', PRINT_PROMPT, _("%s: %s: System shell not found. "
			"Please edit the configuration file to specify a working "
			"shell.\n"), PROGRAM_NAME, user.shell);
	}
} */

#ifndef _NO_TRASH
static inline void
init_trash(void)
{
	if (trash_ok) {
		trash_n = (size_t)count_dir(trash_files_dir, NO_CPOP);
		if (trash_n <= 2)
			trash_n = 0;
	}
}
#endif /* _NO_TRASH */

static inline void
get_hostname(void)
{
	if (gethostname(hostname, sizeof(hostname)) == -1) {
		hostname[0] = '?';
		hostname[1] = '\0';
		_err('e', PRINT_PROMPT, _("%s: Error getting hostname\n"),
			PROGRAM_NAME);
	}
}

/* Initialize the files filter struct */
static void
init_filter(void)
{
	filter.str = (char *)NULL;
	filter.rev = 0;
	filter.env = 0;
	filter.type = FILTER_NONE;
}

/* Initialize the msgs struct */
static void
init_msgs(void)
{
	msgs.error = msgs.notice = msgs.warning = 0;
}

static void
set_locale(void)
{
	/* Use the locale specified by the environment */
	setlocale(LC_ALL, "");
	if (strcmp(nl_langinfo(CODESET), "UTF-8") != 0) {
		_err('w', PRINT_PROMPT, "%s: Locale is not UTF-8. To avoid "
			"encoding issues you might want to set an UTF-8 locale. Ex: "
			"export LANG=es_AR.UTF-8\n", PROGRAM_NAME);
	}
}

static void
check_gui(void)
{
	/* Running on a graphical environment? */
#if !defined(__HAIKU__) && !defined(__CYGWIN__)
	if (getenv("DISPLAY") || getenv("WAYLAND_DISPLAY"))
#endif
	{
		flags |= GUI;
	}
}

/*
static void
init_file_flags(void)
{
	flags |= CONFIG_OK;
	flags |= HOME_OK;
	flags |= SELFILE_OK;
	flags |= TRASH_OK;
} */

				/**
				 * #############################
				 * #           MAIN            #
				 * #############################
				 * */

/* 1. Initialize stuff
 * 2. Run the main program loop */
int
main(int argc, char *argv[])
{
	/* Quite unlikely to happen, but one never knows. See
	 * https://lwn.net/SubscriberLink/882799/cb8f313c57c6d8a6/
	 * and
	 * https://stackoverflow.com/questions/49817316/can-argc-be-zero-on-a-posix-system*/
	if (argc == 0) {
		fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(EINVAL));
		exit(EINVAL);
	}

	check_term(); /* Let's check terminal capabilities */

	/* # 1. INITIALIZE EVERYTHING WE NEED # */

	init_conf_struct();
	init_filter();
	init_msgs();
#if !defined(_NO_ICONS)
	init_icons_hashes();
#endif
/*	init_file_flags(); */

	set_locale();

	/* Store external arguments to be able to rerun external_arguments()
	 * in case the user edits the config file, in which case the program
	 * must rerun init_config(), get_aliases(), get_prompt_cmds(), and
	 * then external_arguments() */
	backup_argv(argc, argv);

	atexit(free_stuff); /* free_stuff does some cleaning */

	user = get_user_data();
	get_home();

	check_gui();

	P_tmpdir_len = strlen(P_tmpdir);
	init_workspaces();

	/* Set all external arguments flags to uninitialized state */
	unset_xargs();

	/* Manage external arguments
	 * External arguments will override initialization values (init_config) */
	if (argc > 1)
		external_arguments(argc, argv);
	/* external_arguments is executed before init_config because, if
	 * specified (-P option), it sets the value of alt_profile, which
	 * is then checked by init_config */

	/* Get paths from PATH environment variable. These paths will be
	 * used later by get_path_programs (for the autocomplete function)
	 * and get_cmd_path() */
	if (!(flags & PATH_PROGRAMS_ALREADY_LOADED))
		path_n = get_path_env();
	cdpath_n = get_cdpath();

	check_env_filter();
	get_data_dir();

	/* Initialize program paths and files, set options from the config
	 * file, if they were not already set via external arguments, and
	 * load sel elements, if any. All these configurations are made
	 * per user basis */
	init_config();
	check_options();

	set_sel_file();
	create_tmp_files();
#ifndef _NO_FZF
	set_finder_paths();
#endif /* _NO_FZF */
	load_actions();
	get_aliases();

	/* Get the list of available applications in PATH to be used by the
	 * custom TAB-completion function (tab_complete, in tabcomp.c) */
	if (!(flags & PATH_PROGRAMS_ALREADY_LOADED))
		get_path_programs();

	/* Check third-party programs availability: finders (fzf, fnf, smenu),
	 * udevil, and udisks2 */
	check_third_party_cmds();
#ifndef _NO_FZF
	check_completion_mode();
#endif /* _NO_FZF */

	/* Initialize gettext() for translations */
#ifndef _NO_GETTEXT
	init_gettext();
#endif

	fputs(df_c, stdout);
	fflush(stdout);

#ifndef __HAIKU__
	/* No need for this warning on Haiku: it runs as root by default */
	set_root_indicator();
#endif

	load_remotes();
	automount_remotes();
	_splash();
	set_start_path();
	check_working_directory();
	_set_term_title();
	exec_profile();
	load_dirhist();
	add_to_dirhist(workspaces[cur_ws].path);
	get_sel_files();

	/* Start listing as soon as possible to speed up startup time */
	_list();

	shell = get_sys_shell();
	create_kbinds_file();
	load_bookmarks();
	load_keybinds();
	load_tags();
	load_jumpdb();
	if (!jump_db || xargs.path == 1)
		add_to_jumpdb(workspaces[cur_ws].path);

	initialize_readline();
	get_prompt_cmds();

#ifndef _NO_TRASH
	init_trash();
#endif

	get_hostname();
	init_shell();
	set_env();

	if (config_ok == 1)
		init_history();

	/* Store history into an array to be able to manipulate it */
	get_history();

#ifndef _NO_PROFILES
	get_profile_names();
#endif

	load_pinned_dir();
	init_workspaces_opts();

	/* # 2. MAIN PROGRAM LOOP # */
	run_main_loop();

	return exit_code; /* Never reached */
}
