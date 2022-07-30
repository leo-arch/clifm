
			/*  ########################################
			 *  #               CliFM                  #
			 *  # 	  The command line file manager    #
			 *  ######################################## */

/* GPL2+ License 
 * Copyright (C) 2016-2022, L. Abramovich <johndoe.arch@outlook.com>
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
 *
 */

#include "helpers.h"

#include <errno.h>
#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "aux.h"
#include "checks.h"
#include "colors.h"
#include "config.h"
#include "exec.h"
#include "history.h"
#include "init.h"
#include "jump.h"
#include "keybinds.h"
#include "listing.h"
#include "misc.h"
#include "navigation.h"
#include "profiles.h"
#include "prompt.h"
#include "readline.h"
#include "strings.h"
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
struct sel_t *sel_elements = (struct sel_t *)NULL;
struct prompts_t *prompts = (struct prompts_t *)NULL;
struct history_t *history = (struct history_t *)NULL;
struct msgs_t msgs;
struct props_t prop_fields;

struct sort_t __sorts[] = {
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

int curcol = 0,
	currow = 0,
	flags = 0,
	finder_flags = 0,
	search_flags = 0;

struct termios
	orig_termios,
	shell_tmodes;

pid_t own_pid = 0;

unsigned short
	term_cols = 0,
	term_rows = 0;

regex_t regex_exp;
size_t *ext_colors_len = (size_t *)NULL;

int
	apparent_size = UNSET,
	auto_open = UNSET,
	autocd = UNSET,
	autocmd_set = 0,
	autojump = UNSET,
	autols = UNSET,
	bell = UNSET,
	bg_proc = 0,
	case_sens_dirjump = UNSET,
	case_sens_path_comp = UNSET,
	case_sensitive = UNSET, /* Case sensitive file listing */
	case_sens_search = UNSET,
	cd_on_quit = UNSET,
	check_cap = UNSET,
	check_ext = UNSET,
	classify = UNSET,
	clear_screen = UNSET,
	cmdhist_flag = 0,
	colorize = UNSET,
	columned = UNSET,
	config_ok = 1,
	control_d_exits = 0,
	cp_cmd = UNSET,
	cur_ws = UNSET,
	dequoted = 0,

	desktop_notifications = UNSET,

	dir_changed = 0,
	dirhist_map = UNSET,
	disk_usage = UNSET,
	expand_bookmarks = UNSET,
	ext_cmd_ok = UNSET,
	files_counter = UNSET,
	filter_rev = 0,
	follow_symlinks = UNSET,
	full_dir_size = UNSET,
	fzftab = UNSET,
	fzf_height_set = 0,
	fzf_open_with = 0,
	highlight = UNSET,
	hist_status = UNSET,
	home_ok = 1,
#ifndef _NO_ICONS
	icons = UNSET,
#endif
	int_vars = UNSET,
	internal_cmd = 0,
	is_sel = 0,
	kb_shortcut = 0,
	kbind_busy = 0,
	light_mode = UNSET,
	list_dirs_first = UNSET,
	listing_mode = UNSET,
	log_cmds = UNSET,
	logs_enabled = UNSET,
	long_view = UNSET,
	max_name_len = DEF_MAX_NAME_LEN,
	mime_match = 0,
	min_name_trim = UNSET,
	mv_cmd = UNSET,
	no_eln = UNSET,
	no_log = 0,
	only_dirs = UNSET,
	open_in_foreground = 0,
	pager = UNSET,
	tips = UNSET,
	prev_ws = UNSET,
	print_msg = 0,
	print_selfiles = UNSET,
	print_removed_files = UNSET,
	prompt_offset = UNSET,
	prompt_notif = UNSET,
	recur_perm_error_flag = 0,
	restore_last_path = UNSET,
	rl_last_word_start = 0,
	rl_nohist = 0,
	rl_notab = 0,
	search_strategy = UNSET,
	sel_is_last = 0,
	selfile_ok = 1,
	share_selbox = UNSET,
	shell = SHELL_NONE,
	shell_terminal = 0,
	show_hidden = UNSET,
	sort = UNSET,
	sort_reverse = 0,
	sort_switch = 0,
	splash_screen = UNSET,
	suggestions = UNSET,
	suggest_filetype_color = UNSET,
	switch_cscheme = 0,
#ifndef _NO_TRASH
	tr_as_rm = UNSET,
	trash_ok = 1,
#endif
	unicode = UNSET,
	warning_prompt = UNSET,
	welcome_message = UNSET,
	_xrename = 0,
	xrename = 0;

int wrong_cmd = 0;
/*int wrong_cmd_line = 0; */

int
	argc_bk = 0,
	dirhist_cur_index = 0,
	dir_out = 0,
	exit_code = 0,
	dirhist_total_index = 0,
	jump_total_rank = 0,
	max_dirhist = UNSET,
	max_files = UNSET,
	max_hist = UNSET,
	min_jump_rank = UNSET,
	max_jump_total_rank = UNSET,
	max_log = UNSET,
	max_path = UNSET,
	max_printselfiles = UNSET,
	shell_is_interactive = 0;

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
	nwords = 0,
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
	zombies = 0;

char
	cur_prompt_name[NAME_MAX] = "",
	div_line[NAME_MAX],
	hostname[HOST_NAME_MAX],
#ifndef _NO_FZF
	finder_in_file[PATH_MAX],
	finder_out_file[PATH_MAX],
#endif /* _NO_FZF */
	_fmatch[PATH_MAX],
	prop_fields_str[PROP_FIELDS_SIZE] = "",

	*actions_file = (char *)NULL,
	*alt_bm_file = (char *)NULL,
	*alt_config_dir = (char *)NULL,
	*alt_config_file = (char *)NULL,
	*alt_kbinds_file = (char *)NULL,
	*alt_profile = (char *)NULL,
	*bm_file = (char *)NULL,
	*colors_dir = (char *)NULL,
	*config_dir = (char *)NULL,
	*config_dir_gral = (char *)NULL,
	*config_file = (char *)NULL,
	*cur_color = (char *)NULL,
	*cur_tag = (char *)NULL,
	*data_dir = (char *)NULL,
	*cur_cscheme = (char *)NULL,
	*dirhist_file = (char *)NULL,
	*encoded_prompt = (char *)NULL,
	*file_cmd_path = (char *)NULL,
	*_filter = (char *)NULL,
	*fzftab_options = (char *)NULL,
	*hist_file = (char *)NULL,
	*jump_suggestion = (char *)NULL,
	*kbinds_file = (char *)NULL,
	*last_cmd = (char *)NULL,
	*log_file = (char *)NULL,
	*mime_file = (char *)NULL,
	*opener = (char *)NULL,
	*pinned_dir = (char *)NULL,
	*plugins_dir = (char *)NULL,
	*profile_file = (char *)NULL,
	*prompts_file = (char *)NULL,
	*quote_chars = (char *)NULL,
	*remotes_file = (char *)NULL,
/*	*right_prompt = (char *)NULL, */
	*sel_file = (char *)NULL,
	*smenutab_options_env = (char *)NULL,
	*stdin_tmp_dir = (char *)NULL,
#ifndef _NO_SUGGESTIONS
	*suggestion_buf = (char *)NULL,
	*suggestion_strategy = (char *)NULL,
#endif
	*sys_shell = (char *)NULL,
	*tags_dir = (char *)NULL,
	*term = (char *)NULL,
	*tmp_dir = (char *)NULL,
#ifndef _NO_TRASH
	*trash_dir = (char *)NULL,
	*trash_files_dir = (char *)NULL,
	*trash_info_dir = (char *)NULL,
#endif
	*usr_cscheme = (char *)NULL,
	*wprompt_str = (char *)NULL,

	**argv_bk = (char **)NULL,
	**bin_commands = (char **)NULL,
	**bookmark_names = (char **)NULL,
	**cdpaths = (char **)NULL,
	**color_schemes = (char **)NULL,
	**ext_colors = (char **)NULL,
	**messages = (char **)NULL,
	**old_pwd = (char **)NULL,
	**paths = (char **)NULL,
	**profile_names = (char **)NULL,
	**prompt_cmds = (char **)NULL,
	**tags = (char **)NULL;

/* A list of internal commands, with short and long formats. We use two
 * more lists of commands: one of commands dealing with file names
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
	{"bh", 2},
	{"fh", 2},
	{"bl", 2},
	{"bm", 2},
	{"bookmarks", 9},
	{"br", 2},
	{"bulk", 4},
	{"c", 1}, //"cp",
//	{"cc", 2}, /* Remove to avoid conflicts with /bin/cc */
	{"colors", 6},
	{"cd", 2},
	{"cl", 2},
	{"columns", 7},
	{"cmd", 3},
	{"commands", 8},
	{"cs", 2},
	{"colorschemes", 12},
	{"d", 1},
	{"dup", 3},
	{"ds", 2},
	{"desel", 5},
	{"edit", 4},
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
	{"history", 7},
	{"hf", 2},
	{"hidden", 6},
	{"icons", 5},
	{"jump", 4},
	{"je", 2},
	{"jc", 2},
	{"jp", 2},
	{"jo", 2},
	{"kb", 2},
	{"keybinds", 8},
	{"l", 1},
	{"le", 2}, //"ln",
	{"lm", 2},
	{"log", 3},
	{"m", 1}, //"mv",
	{"md", 2},
//	{"mkdir", },
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
	{"open", 4},
	{"ow", 2},
	{"opener", 6},
	{"p", 1},
	{"pp", 2},
	{"pr", 2},
	{"prop", 4},
	{"path", 4},
	{"cwd", 3},
	{"paste", 5},
	{"pf", 2},
	{"prof", 4},
	{"profile", 7},
	{"pg", 2},
	{"pager", 5},
	{"pin", 3},
	{"unpin", 5},
	{"prompt", 6},
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
//	"touch",
	{"u", 1},
	{"undel", 5},
	{"untrash", 7},
	{"uc", 2},
	{"unicode", 7},
	{"unlink", 6},
	{"v", 1},
	{"vv", 2},
	{"ver", 3},
	{"version", 7},
	{"ws", 2},
	{"x", 1},
	{"X", 1},
	{NULL, 0}
};
size_t internal_cmds_n = 0;

/* A list of internal commands and fixed parameters for the auto-suggestions system */
const struct cmdslist_t param_str[] = {
	{"actions edit", 12},
	{"autocd on", 9},
	{"acd on", 6},
	{"autocd off", 10},
	{"acd off", 7},
	{"autocd status", 13},
	{"acd status", 10},
	{"alias import", 12},
	{"alias ls", 8},
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
	{"desel all", 9},
	{"ds all", 6},
	{"cs edit", 7},
	{"colorscheme edit", 16},
	{"edit", 4},
	{"edit reset", 10},
	{"ext on", 6},
	{"ext off", 7},
	{"ext status", 10},
	{"f hist", 6},
	{"f clear", 7},
	{"forth hist", 10},
	{"forth clear", 11},
	{"fc on", 5},
	{"filescounter on", 15},
	{"fc off", 6},
	{"filescounter off", 16},
	{"fc status", 9},
	{"filescounter status", 19},
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
	{"help desktop-notifications", 26},
	{"help dir-jumper", 15},
	{"help file-details", 17},
	{"help file-tags", 14},
	{"help navigation", 15},
	{"help plugins", 12},
	{"help remotes", 12},
	{"help resource-opener", 20},
	{"help selection", 14},
	{"help search", 11},
	{"help theming", 12},
	{"help trash", 10},
	{"hf on", 5},
	{"hf off", 6},
	{"hf status", 9},
	{"hidden on", 9},
	{"hidden off", 10},
	{"hidden status", 13},
	{"history clear", 13},
	{"history edit", 12},
	{"history on", 10},
	{"history off", 11},
	{"history status", 14},
	{"icons on", 8},
	{"icons off", 9},
	{"kb edit", 7},
	{"keybinds edit", 13},
	{"kb reset", 8},
	{"keybinds reset", 14},
	{"kb readline", 11},
	{"keybinds readline", 17},
	{"l edit", 6},
	{"lm on", 5},
	{"lm off", 6},
	{"log clear", 9},
	{"mf unset", 8},
	{"mm info", 7},
	{"mm edit", 7},
	{"mm import", 9},
	{"mime info", 9},
	{"mime edit", 9},
	{"mime import", 11},
	{"msg clear", 9},
	{"messages clear", 14},
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
	{"profile set", 11},
	{"profile add", 11},
	{"profile del", 11},
	{"profile list", 12},
	{"prompt edit", 11},
	{"prompt list", 11},
	{"prompt reload", 13},
	{"prompt unset", 12},
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
	{"tr list", 7},
	{"tr clear", 8},
	{"tr empty", 8},
	{"tr del", 6},
	{"trash list", 10},
	{"trash clear", 11},
	{"trash empty", 11},
	{"trash del", 9},
	{"tag del", 7},
	{"tag rm", 6},
	{"tag new", 7},
	{"tag merge", 9},
	{"tag rename", 10},
	{"tag mv", 6},
	{"tag untag", 9},
	{"u all", 5},
	{"undel all", 9},
	{"untrash all", 11},
	{"uc on", 5},
	{"unicode on", 10},
	{"uc off", 6},
	{"unicode off", 11},
	{"uc status", 9},
	{"unicode status", 14},
	{NULL, 0}
};

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
	ne_c[MAX_COLOR], /* No read empty dir */
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
	mi_c[MAX_COLOR], /* Misc indicators */
	ts_c[MAX_COLOR], /* TAB completion suffix */
	wc_c[MAX_COLOR], /* Welcome message color */
	wp_c[MAX_COLOR], /* Warning prompt */
	tt_c[MAX_COLOR], /* Tilde for trimmed files */

	/* Suggestions */
	sb_c[MAX_COLOR], /* Auto-suggestions: shell builtins */
	sc_c[MAX_COLOR], /* Auto-suggestions: external commands */
	sh_c[MAX_COLOR], /* Auto-suggestions: history */
	sf_c[MAX_COLOR], /* Auto-suggestions: filenames */
	sx_c[MAX_COLOR], /* Auto-suggestions: internal commands and params */
	sp_c[MAX_COLOR], /* Auto-suggestions: suggestions pointer */

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

/* This is the main structure of any basic shell
	 1 - Infinite loop
	 2 - Grab user input
	 3 - Parse user input
	 4 - Execute command
	 See https://brennan.io/2015/01/16/write-a-shell-in-c/
	 */
static inline void
run_main_loop(void)
{
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
			/* If an alias is found, check_for_alias() frees CMD
			 * and returns ALIAS_CMD in its place to be executed by
			 * exec_cmd() */
			exec_cmd(alias_cmd);

			for (i = 0; alias_cmd[i]; i++)
				free(alias_cmd[i]);
			free(alias_cmd);
		} else {
			exec_cmd(cmd);
			i = (int)args_n + 1;
			while (--i >= 0)
				free(cmd[i]);
			free(cmd);
		}
	}
}

static inline void
check_cpu_os(void)
{
	/* Though this program might perfectly work on other architectures,
	 * I just didn't test anything beyond x86 and ARM */
#if !defined(__x86_64__) && !defined(__i386__) && !defined(__ARM_ARCH)
	fprintf(stderr, _("%s: Unsupported CPU architecture\n"), PROGRAM_NAME);
	exit(EXIT_FAILURE);
#endif

#if !defined(__linux__) && !defined(__FreeBSD__) && !defined(__NetBSD__) \
&& !defined(__OpenBSD__) && !defined(__HAIKU__) && !defined(__APPLE__)
	fprintf(stderr, _("%s: Unsupported operating system\n"), PROGRAM_NAME);
	exit(EXIT_FAILURE);
#endif
}

static inline void
set_root_indicator(void)
{
	if (flags & ROOT_USR) {
		_err(ERR_NO_LOG, PRINT_PROMPT, _("%s->%s Running as root%s\n"),
			colorize == 1 ? mi_c : "", colorize == 1 ? _RED : "", colorize == 1 ? df_c : "");
	}
}

static inline void
__list()
{
	if (autols == 1 && isatty(STDIN_FILENO)) {
#ifdef LINUX_INOTIFY
		/* Initialize inotify */
		inotify_fd = inotify_init1(IN_NONBLOCK);
		if (inotify_fd < 0) {
			_err('w', PRINT_PROMPT, "%s: inotify: %s\n", PROGRAM_NAME, strerror(errno));
		}
#elif defined(BSD_KQUEUE)
		kq = kqueue();
		if (kq < 0) {
			_err('w', PRINT_PROMPT, "%s: kqueue: %s\n", PROGRAM_NAME, strerror(errno));
		}
#endif

		if (colorize == 1 && xargs.eln_use_workspace_color == 1)
			set_eln_color();

		list_dir();
	}
}

static inline void
_splash(void)
{
	if (splash_screen) {
		splash();
		splash_screen = 0;
		CLEAR;
	}
}

/* Set terminal window title */
static inline void
_set_term_title(void)
{
	if (!(flags & GUI) || xargs.list_and_quit == 1)
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
static inline void
check_working_shell(void)
{
	if (access(user.shell, X_OK) == -1) {
		_err('w', PRINT_PROMPT, _("%s: %s: System shell not found. "
				"Please edit the configuration file to specify a working "
				"shell.\n"), PROGRAM_NAME, user.shell);
	}
}

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
		_err('e', PRINT_PROMPT, _("%s: Error getting hostname\n"), PROGRAM_NAME);
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

	msgs.error = msgs.notice = msgs.warning = 0;
/*	init_file_flags(); */
	check_cpu_os(); /* Running on supported CPU and operating system? */
	check_term(); /* Running on a supported terminal */

	/* # 1. INITIALIZE EVERYTHING WE NEED # */

	/* If running the program locally, that is, not from a path in PATH,
	 * remove the leading "./" to get the correct program invocation
	 * name */
	if (*argv[0] == '.' && *(argv[0] + 1) == '/' && *(argv[0] + 2))
		argv[0] += 2;

	/* Use the locale specified by the environment */
	setlocale(LC_ALL, "");

	unicode = DEF_UNICODE;

	/* Store external arguments to be able to rerun external_arguments()
	 * in case the user edits the config file, in which case the program
	 * must rerun init_config(), get_aliases(), get_prompt_cmds(), and
	 * then external_arguments() */
	backup_argv(argc, argv);

	/* free_stuff does some cleaning */
	atexit(free_stuff);

	user = get_user();
	get_home();

	if (geteuid() == 0)
		flags |= ROOT_USR;

	/* Running in a graphical environment? */
	if (getenv("DISPLAY") || getenv("WAYLAND_DISPLAY"))
		flags |= GUI;

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
	set_env();
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

	/* Check third-party programs availability: finders (fzf, fzy, smenu), udevil, and udisks2 */
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
	__list();

	shell = get_sys_shell();
	create_kbinds_file();
	load_bookmarks();
	load_keybinds();
	load_tags();
	load_jumpdb();
	if (!jump_db || xargs.path == 1)
		add_to_jumpdb(workspaces[cur_ws].path);

//	int vanilla_readline = 1;
//	if (vanilla_readline != 1)
	initialize_readline();
	/*Trim the directory history file if necessary */
	check_file_size(dirhist_file, max_dirhist);
	check_working_shell();
	get_prompt_cmds();

#ifndef _NO_TRASH
	init_trash();
#endif

	get_hostname();
	init_shell();

	if (config_ok == 1)
		init_history();

	/* Store history into an array to be able to manipulate it */
	get_history();

	get_profile_names();
	load_pinned_dir();
	load_prompts();

	/* # 2. MAIN PROGRAM LOOP # */
	run_main_loop();

	return exit_code; /* Never reached */
}
