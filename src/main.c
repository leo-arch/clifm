/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* main.c -- Main file for Clifm */

#include "helpers.h"

#include <errno.h>
#include <langinfo.h> /* nl_langinfo() */
#include <locale.h>   /* setlocale */
#include <string.h>   /* strcmp, strerror */
#include <unistd.h>   /* isatty, gethostname, STDIN_FILENO */

#include "args.h"
#include "aux.h"
#include "checks.h"
#include "config.h"
#include "exec.h"
#include "history.h"
#include "init.h"
#include "jump.h"
#include "keybinds.h"
#include "listing.h"
#include "mimetypes.h" /* load_user_mimetypes() */
#include "misc.h"
#ifndef _NO_PROFILES
# include "profiles.h"
#endif /* !_NO_PROFILES */
#include "prompt.h"
#include "properties.h" /* do_stat_and_exit() */
#include "readline.h"
#include "remotes.h"
#ifdef SECURITY_PARANOID
# include "sanitize.h"
#endif /* SECURITY_PARANOID */
#include "term.h" /* set_term_title() */

/* Globals */

struct usrvar_t *usr_var = NULL;
struct actions_t *usr_actions = NULL;
struct ws_t *workspaces = NULL;
struct kbinds_t *kbinds = NULL;
struct jump_t *jump_db = NULL;
struct bookmarks_t *bookmarks = NULL;
struct fileinfo *file_info = NULL;
struct remote_t *remotes = NULL;
struct alias_t *aliases = NULL;
struct user_t user = {0};
/* Store device and inode number of selected files */
struct devino_t *sel_devino = NULL;
#ifndef _NO_SUGGESTIONS
struct suggestions_t suggestion = {0};
#endif /* !_NO_SUGGESTIONS */
struct stats_t stats = {0};
struct autocmds_t *autocmds = NULL;
struct opts_t opts = {0};
struct opts_t workspace_opts[MAX_WS];
struct sel_t *sel_elements = NULL;
struct prompts_t *prompts = NULL;
struct history_t *history = NULL;
struct msgs_t msgs = {0};
struct props_t prop_fields = {0};
struct termcaps_t term_caps = {0};
struct filter_t filter = {0};
struct config_t conf = {0};
struct shades_t date_shades = {0};
struct shades_t size_shades = {0};
struct paths_t *paths = NULL;
struct ext_t *ext_colors = NULL;
#ifdef LINUX_FSINFO
struct ext_mnt_t *ext_mnt = NULL;
#endif /* LINUX_FSINFO */
struct groups_t *sys_users = NULL;
struct groups_t *sys_groups = NULL;
struct dircmds_t dir_cmds = {UNSET, 0};
struct pmsgs_t *messages = NULL;
struct mime_t *user_mimetypes = NULL;

#ifndef _NO_MAGIC
magic_t g_magic_mime_type_cookie = NULL;
magic_t g_magic_text_desc_cookie = NULL;
#endif /* !_NO_MAGIC */

const struct sort_t sort_methods[] = {
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
    {"blocks", 12, 0},
    {"links", 13, 0},
    {"type", 14, 0},
    {NULL, 15, 0},
};

/* pmsg holds the current program message type */
enum prog_msg pmsg = NOMSG;
enum comp_type cur_comp_type = TCMP_NONE;
enum tab_mode tabmode = STD_TAB;

struct param_t xargs = {0};
unsigned short term_cols;
double last_cmd_time = 0;

/* Bit flag holders */
int
	flags = 0,
	bin_flags = 0,
	search_flags = 0;

int
	date_shades_old_style = 0,
	size_shades_old_style = 0;

pid_t own_pid = 0;
time_t props_now = 0;

unsigned short
	term_cols = 0,
	term_lines = 0;

regex_t regex_exp;
regex_t regex_hist;
regex_t regex_dirhist;

/* Internal status flags */
int
	alt_prompt = 0,
	argc_bk = 0,
	autocmd_set = 0,
	bg_proc = 0,
	cmdhist_flag = 0,
	config_ok = 1,
	cur_ws = UNSET,
	curcol = 0,
	dequoted = 0,
	dir_changed = 0,
	dirhist_cur_index = 0,
	dirhist_total_index = 0,
	exit_code = 0,

	fzftab = UNSET,
	fzf_ext_border = UNSET,
	fzf_border_type = UNSET,
	fzf_height_value = 0,
	fzf_preview_border_type = 0,

	hist_status = UNSET,
	home_ok = 1,
	internal_cmd = 0,
	is_sel = 0,
	is_cdpath = 0,
	jump_total_rank = 0,
	kbind_busy = 0,
	nesting_level = 0,
	no_log = 0,
	open_in_foreground = 0,
	prev_ws = UNSET,
	print_msg = 0,
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
#endif /* !_NO_TRASH */
	virtual_dir = 0,
	wrong_cmd = 0;

filesn_t g_files_num = 0;

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
	jump_n = 0,
	kbinds_n = 0,
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
size_t *name_icon_hashes = (size_t *)0;
size_t *dir_icon_hashes = (size_t *)0;
size_t *ext_icon_hashes = (size_t *)0;
#ifndef OLD_ICON_LOOKUP
size_t *ext_table = (size_t *)0;
#endif
#endif /* !_NO_ICONS */

char
	cur_prompt_name[NAME_MAX + 1] = "",
	div_line[NAME_MAX + 1],
	hostname[HOST_NAME_MAX + 1],
	fz_match[PATH_MAX + 1],
	prop_fields_str[PROP_FIELDS_SIZE + 1] = "",
	invalid_time_str[MAX_TIME_STR] = "",

#ifdef RUN_CMD
	*cmd_line_cmd = NULL,
#endif /* RUN_CMD */

	*actions_file = NULL,
	*alt_bm_file = NULL,
	*alt_config_dir = NULL,
	*alt_trash_dir = NULL,
	*alt_config_file = NULL,
	*alt_kbinds_file = NULL,
	*alt_mimelist_file = NULL,
	*alt_preview_file = NULL,
	*alt_profile = NULL,
	*bm_file = NULL,
	*cmds_log_file = NULL,
	*colors_dir = NULL,
	*config_dir = NULL,
	*config_dir_gral = NULL,
	*config_file = NULL,
	*cur_color = NULL,
	*cur_tag = NULL,
	*data_dir = NULL,
	*cur_cscheme = NULL,
	*dirhist_file = NULL,
	*file_cmd_path = NULL,
	*hist_file = NULL,
	*jump_suggestion = NULL,
	*kbinds_file = NULL,
	*last_cmd = NULL,
	*mime_file = NULL,
	*msgs_log_file = NULL,
	*pinned_dir = NULL,
	*plugins_dir = NULL,
	*plugins_helper_file = NULL,
	*profile_file = NULL,
	*prompts_file = NULL,
	*quote_chars = NULL,
	*rl_callback_handler_input = NULL,
	*remotes_file = NULL,
	*sel_file = NULL,
	*smenutab_options_env = NULL,
	*stdin_tmp_dir = NULL,
	*sudo_cmd = NULL,
#ifndef _NO_SUGGESTIONS
	*suggestion_buf = NULL,
#endif /* !_NO_SUGGESTIONS */
	*sys_shell = NULL,
	*tags_dir = NULL,
	*templates_dir = NULL,
	*thumbnails_dir = NULL,
	*tmp_rootdir = NULL,
	*tmp_dir = NULL,
#ifndef _NO_TRASH
	*trash_dir = NULL,
	*trash_files_dir = NULL,
	*trash_info_dir = NULL,
#endif /* !_NO_TRASH */

	**argv_bk = NULL,
	**bin_commands = NULL,
	**cdpaths = NULL,
	**color_schemes = NULL,
	**file_templates = NULL,
	**old_pwd = NULL,
	**profile_names = NULL,
	**prompt_cmds = NULL,
	**tags = NULL;

/* Colors */
char
	/* File types */
	bd_c[MAX_COLOR], /* Block device */
	bk_c[MAX_COLOR], /* Backup/temp files */
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
#ifdef SOLARIS_DOORS
	oo_c[MAX_COLOR], /* Solaris door/port */
#endif /* SOLARIS_DOORS */
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
	ac_c[MAX_COLOR + 2], /* Autocmd indicator */
	df_c[MAX_COLOR], /* Default color */
	dl_c[MAX_COLOR], /* Dividing line index */
	el_c[MAX_COLOR], /* ELN */
	fc_c[MAX_COLOR], /* File counter */
	lc_c[MAX_COLOR], /* Symlink character (ColorLinkAsTarget only) */
	mi_c[MAX_COLOR], /* Misc indicators */
	ts_c[MAX_COLOR], /* Tab completion suffix */
	wc_c[MAX_COLOR], /* Welcome message color */
	wp_c[MAX_COLOR], /* Warning prompt */
	tt_c[MAX_COLOR], /* Character to mark truncated filenames */

	/* Suggestions */
	sb_c[MAX_COLOR], /* Auto-suggestions: shell builtins */
	sc_c[MAX_COLOR], /* Auto-suggestions: external commands */
	sd_c[MAX_COLOR], /* Auto-suggestions: internal commands description */
	sh_c[MAX_COLOR], /* Auto-suggestions: history */
	sf_c[MAX_COLOR], /* Auto-suggestions: filenames */
	sx_c[MAX_COLOR], /* Auto-suggestions: internal commands and params */
	sp_c[MAX_COLOR], /* Auto-suggestions: suggestions pointer */
	sz_c[MAX_COLOR], /* Auto-suggestions: filenames (fuzzy) */

#ifndef _NO_ICONS
	dir_ico_c[MAX_COLOR], /* Directories icon color */
#endif /* !_NO_ICONS */

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
	hw_c[MAX_COLOR], /* Backslash (aka whack) */

	db_c[MAX_COLOR],  /* File allocated blocks */
	dd_c[MAX_COLOR],  /* Date (fixed color: no shading) */
	de_c[MAX_COLOR],  /* Inode number */
	dg_c[MAX_COLOR],  /* Group ID */
	dk_c[MAX_COLOR],  /* Number of links */
	dn_c[MAX_COLOR],  /* dash (none) */
	do_c[MAX_COLOR],  /* Octal perms */
	dp_c[MAX_COLOR],  /* Special files (SUID, SGID, etc) */
	dr_c[MAX_COLOR],  /* Read */
	dt_c[MAX_COLOR],  /* Timestamp mark */
	du_c[MAX_COLOR],  /* User ID */
	dw_c[MAX_COLOR],  /* Write */
	dxd_c[MAX_COLOR], /* Execute (dirs) */
	dxr_c[MAX_COLOR], /* Execute (reg files) */
	dz_c[MAX_COLOR],  /* Size (dirs) */

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
	li_cb[MAX_COLOR], /* Sel indicator color (for the file list) */
	nm_c[MAX_COLOR + 2], /* Notice msg color */
	wm_c[MAX_COLOR + 2], /* Warning msg color */
	ro_c[MAX_COLOR + 2], /* read-only indicator color */
	si_c[MAX_COLOR + 2], /* stealth-mode indicator color */
	ti_c[MAX_COLOR + 2], /* Trash indicator color */
	tx_c[MAX_COLOR + 2], /* Text color */
	xs_c[MAX_COLOR + 2], /* Exit code: success */
	xs_cb[MAX_COLOR],    /* Exit code: success (Unicode success indicator) */
	xf_c[MAX_COLOR + 2], /* Exit code: failure */
	xf_cb[MAX_COLOR],    /* Exit code: failure (dir read) */

	tmp_color[MAX_COLOR + 2], /* A temp buffer to store color codes */
	dim_c[] = "\x1b[2m";

/* A buffer to store filenames to be displayed (wide string) */
char name_buf[NAME_BUF_SIZE * sizeof(wchar_t)];

/* A list of all internal commands, including command name, name length,
 * and parameter type (as a bit flag). Used by is_internal_cmd(), in checks.c. */
const struct cmdslist_t internal_cmds[] = {
	{",", 1, NO_PARAM, 0},
	{"?", 1, NO_PARAM, 0},
	{"help", 4, NO_PARAM, 0},
	{"ac", 2, PARAM_FNAME, 0},
	{"ad", 2, PARAM_FNAME, 0},
	{"acd", 3, PARAM_STR, 0},
	{"auto", 4, PARAM_STR, 0},
	{"autocd", 6, PARAM_STR, 0},
	{"actions", 7, PARAM_STR, 0},
	{"alias", 5, PARAM_FNAME, 0}, // diff (old 0110, new 1110) // 'alias import' takes filenames
	{"ao", 2, PARAM_STR, 0},
	{"auto-open", 9, PARAM_STR, 0},
	{"b", 1, PARAM_STR, 0},
	{"back", 4, PARAM_STR, 0},
	{"bb", 2, PARAM_FNAME, 0},
	{"bleach", 6, PARAM_FNAME, 0},
	{"bd", 2, PARAM_STR, 0},
	{"dh", 2, PARAM_FNAME, 0}, // diff (old 0110, new 1110)
	{"bl", 2, PARAM_FNAME, 0},
	{"bm", 2, PARAM_FNAME, 0},
	{"bookmarks", 9, PARAM_FNAME, 0},
	{"br", 2, PARAM_FNAME, 0},
	{"bulk", 4, PARAM_FNAME, 0},
	{"c", 1, PARAM_FNAME, 0}, //"cp",
	{"colors", 6, NO_PARAM, 0},
	{"cd", 2, PARAM_FNAME, 0},
	{"cl", 2, PARAM_STR, 0},
	{"columns", 7, PARAM_STR, 0},
	{"cmd", 3, NO_PARAM, 0},
	{"commands", 8, NO_PARAM, 0},
	{"cs", 2, PARAM_STR, 0},
	{"colorschemes", 12, PARAM_STR, 0},
	{"cwd", 3, NO_PARAM, 0}, // deprecate this one, just as 'path'
	{"d", 1, PARAM_FNAME, 0},
	{"dup", 3, PARAM_FNAME, 0},
	{"ds", 2, PARAM_FNAME, 0}, // diff (old 0110, new 1110)
	{"desel", 5, PARAM_FNAME, 0}, // diff (old 0110, new 1110)
	{"config", 6, PARAM_STR, 0},
	{"exp", 3, PARAM_FNAME, 0},
	{"export", 6, PARAM_FNAME | PARAM_STR, 0}, // diff (old 1101, new 1111)
	{"ext", 3, PARAM_STR, 0},
	{"f", 1, PARAM_STR, 0},
	{"forth", 5, PARAM_STR, 0},
	{"fc", 2, PARAM_STR, 0},
	{"ff", 2, PARAM_STR, 0},
	{"dirs-first", 10, PARAM_STR, 0},
	{"ft", 2, PARAM_STR, 0},
	{"filter", 6, PARAM_STR, 0},
	{"fz", 2, PARAM_STR, 0},
	{"hh", 2, PARAM_STR, 0},
	{"hf", 2, PARAM_STR, 0},
	{"hidden", 6, PARAM_STR, 0},
	{"history", 7, PARAM_STR, 0},
	{"icons", 5, PARAM_STR, 0},
	{"j", 1, PARAM_STR, 0},
	{"jump", 4, PARAM_STR, 0},
	{"je", 2, PARAM_STR, 0},
	{"jc", 2, PARAM_STR, 0}, // diff (old 1101, new 0101)
	{"jl", 2, PARAM_STR, 0},
	{"jp", 2, PARAM_STR, 0}, // diff (old 1101, new 0101)
	{"k", 1, NO_PARAM, 0},
	{"kk", 2, NO_PARAM, 0},
	{"kb", 2, PARAM_STR, 0},
	{"keybinds", 8, PARAM_STR, 0},
	{"l", 1, PARAM_FNAME, 0},
	{"le", 2, PARAM_FNAME, 0},
	{"lm", 2, PARAM_STR, 0},
	{"log", 3, PARAM_STR, 0},
	{"ll", 2, PARAM_STR, 0},
	{"lv", 2, PARAM_STR, 0},
	{"m", 1, PARAM_FNAME, 0},
	{"md", 2, PARAM_FNAME, 0}, // diff (old 0110, 1110)
	{"media", 5, NO_PARAM, 0},
	{"mf", 2, PARAM_NUM, 0},
	{"mm", 2, PARAM_FNAME, 0},
	{"mime", 4, PARAM_FNAME, 0},
	{"mp", 2, NO_PARAM, 0},
	{"mountpoints", 11, NO_PARAM, 0},
	{"msg", 3, PARAM_STR, 0},
	{"messages", 8, PARAM_STR, 0},
	{"n", 1, PARAM_FNAME, 0},
	{"new", 3, PARAM_FNAME, 0},
	{"net", 3, PARAM_STR, 0},
	{"o", 1, PARAM_FNAME, 0},
	{"oc", 2, PARAM_FNAME, 0},
	{"open", 4, PARAM_FNAME, 0},
	{"ow", 2, PARAM_FNAME, 0}, // diff (old 0110, new 1110)
	{"opener", 6, PARAM_STR, 0},
	{"p", 1, PARAM_FNAME, 0},
	{"pc", 2, PARAM_FNAME, 0},
	{"pp", 2, PARAM_FNAME, 0},
	{"pr", 2, PARAM_FNAME, 0},
	{"prop", 4, PARAM_FNAME, 0},
	{"path", 4, NO_PARAM, 0}, // deprecate this one, just as 'cwd'
	{"paste", 5, PARAM_FNAME, 0},
	{"pf", 2, PARAM_STR, 0},
	{"prof", 4, PARAM_STR, 0},
	{"profile", 7, PARAM_STR, 0},
	{"pg", 2, PARAM_STR, 0},
	{"pager", 5, PARAM_STR, 0},
	{"pin", 3, PARAM_FNAME, 0},
	{"unpin", 5, NO_PARAM, 0},
	{"prompt", 6, PARAM_STR, 0},
	{"pwd", 3, PARAM_STR, 0},
	{"q", 1, NO_PARAM, 0},
	{"quit", 4, NO_PARAM, 0},
	{"exit", 4, NO_PARAM, 0},
	{"r", 1, PARAM_FNAME, 0}, //"rm",
	{"rf", 2, NO_PARAM, 0},
	{"refresh", 7, NO_PARAM, 0},
	{"rl", 2, NO_PARAM, 0},
	{"reload", 6, NO_PARAM, 0},
	{"rr", 2, PARAM_FNAME, 0},
	{"s", 1, PARAM_FNAME, 0},
	{"sel", 3, PARAM_FNAME, 0},
	{"sb", 2, NO_PARAM, 0},
	{"selbox", 6, NO_PARAM, 0},
	{"st", 2, PARAM_NUM, 0},
	{"sort", 4, PARAM_NUM, 0},
	{"stats", 5, NO_PARAM, 0},
	{"t", 1, PARAM_FNAME, 0},
	{"tr", 2, PARAM_FNAME, 0},
	{"trash", 5, PARAM_FNAME, 0},
	{"tag", 3, PARAM_FNAME, 0},
	{"ta", 2, PARAM_FNAME, 0},
	{"td", 2, PARAM_STR, 0},
	{"tl", 2, PARAM_STR, 0}, // diff (old 0110, new 0101)
	{"tm", 2, PARAM_STR, 0},
	{"tn", 2, PARAM_STR, 0},
	{"tu", 2, PARAM_STR, 0},
	{"ty", 2, PARAM_STR, 0},
	{"te", 2, PARAM_FNAME, 0},
	{"tips", 4, NO_PARAM, 0},
	{"u", 1, PARAM_STR, 0},
	{"undel", 5, PARAM_STR, 0},
	{"unset", 5, PARAM_STR, 0},
	{"untrash", 7, PARAM_STR, 0},
	{"umask", 5, PARAM_STR, 0},
	{"vv", 2, PARAM_FNAME, 0},
	{"ver", 3, NO_PARAM, 0},
	{"version", 7, NO_PARAM, 0},
	{"view", 4, PARAM_STR, 0},
	{"ws", 2, PARAM_NUM, 0},
	{"x", 1, PARAM_FNAME, 0}, // diff (old 0110, new 1110)
	{"X", 1, PARAM_FNAME, 0}, // diff (old 0110, new 1110)
	{NULL, 0, 0, 0}
};

size_t internal_cmds_n = 0;

/* A list of internal commands and fixed parameters for the auto-suggestions
 * system. */
const struct nameslist_t param_str[] = {
	{"actions edit", 12},
	{"actions list", 12},
	{"auto unset", 10},
	{"auto list", 9},
	{"auto none", 9},
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
	{"cs check-ext", 12},
	{"colorschemes check-ext", 22},
	{"colorschemes edit", 17},
	{"cs edit", 7},
	{"colorschemes preview", 20},
	{"cs preview", 10},
	{"desel all", 9},
	{"ds all", 6},
	{"config", 6},
	{"config edit", 11},
	{"config dump", 11},
	{"config reload", 13},
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
	{"help image-previews", 19},
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
	{"hf first", 8},
	{"hf last", 7},
	{"hf status", 9},
	{"hh on", 5},
	{"hh off", 6},
	{"hh status", 9},
	{"hh last", 7},
	{"hh status", 9},
	{"hidden on", 9},
	{"hidden off", 10},
	{"hidden status", 13},
	{"hidden last", 11},
	{"hidden first", 12},
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
	{"kb bind", 7},
	{"keybinds bind", 13},
	{"kb conflict", 11},
	{"keybinds conflict", 17},
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
	{"pg once", 7},
	{"pager once", 10},
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
	{"s --invert", 10},
	{"sel --invert", 12},
	{"st none", 7},
	{"st name", 7},
	{"st size", 7},
	{"st blocks", 9},
	{"st links", 8},
	{"st atime", 8},
	{"st btime", 8},
	{"st ctime", 8},
	{"st mtime", 8},
	{"st owner", 8},
	{"st group", 8},
	{"st extension", 12},
	{"st inode", 8},
	{"st version", 10},
	{"st type", 7},
	{"sort none", 9},
	{"sort name", 9},
	{"sort blocks", 11},
	{"sort size", 9},
	{"sort links", 10},
	{"sort atime", 10},
	{"sort btime", 10},
	{"sort ctime", 10},
	{"sort mtime", 10},
	{"sort owner", 10},
	{"sort group", 10},
	{"sort extension", 14},
	{"sort inode", 10},
	{"sort version", 12},
	{"sort type", 9},
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
	{"view purge", 10},
	{NULL, 0}
};

const struct nameslist_t kb_cmds[] = {
	{"archive-sel", 11},
	{"bookmarks", 9},
	{"cmd-hist", 8},
	{"clear-line", 10},
	{"clear-msgs", 10},
	{"create-file", 11},
	{"deselect-all", 12},
	{"dirs-first", 10},
	{"edit-color-scheme", 17},
	{"export-sel", 10},
	{"home-dir", 8},
	{"invert-selection", 16},
	{"launch-view", 11},
	{"lock", 4},
	{"mountpoints", 11},
	{"move-sel", 8},
	{"new-instance", 12},
	{"next-dir", 8},
	{"next-profile", 12},
	{"only-dirs", 9},
	{"open-bookmarks", 14},
	{"open-config", 11},
	{"open-jump-db", 12},
	{"open-keybinds", 13},
	{"open-mime", 9},
	{"open-preview", 12},
	{"open-sel", 8},
	{"parent-dir", 10},
	{"pinned-dir", 10},
	{"plugin1", 7},
	{"plugin2", 7},
	{"plugin3", 7},
	{"plugin4", 7},
	{"plugin5", 7},
	{"plugin6", 7},
	{"plugin7", 7},
	{"plugin8", 7},
	{"plugin9", 7},
	{"plugin10", 8},
	{"plugin11", 8},
	{"plugin12", 8},
	{"plugin13", 8},
	{"plugin14", 8},
	{"plugin15", 8},
	{"plugin16", 8},
	{"paste-sel", 10},
	{"prepend-sudo", 12},
	{"previous-dir", 12},
	{"previous-profile", 16},
	{"quit", 4},
	{"refresh-screen", 14},
	{"remove-sel", 10},
	{"rename-sel", 10},
	{"root-dir", 8},
	{"run-pager", 9},
	{"selbox", 6},
	{"select-all", 10},
	{"show-dirhist", 12},
	{"sort-previous", 13},
	{"sort-next", 9},
	{"show-manpage", 12},
	{"show-cmds", 9},
	{"show-kbinds", 11},
	{"toggle-disk-usage", 17},
	{"toggle-follow-links-long", 24},
	{"toggle-hidden", 13},
	{"toggle-light", 12},
	{"toggle-long", 11},
	{"toggle-max-name-len", 19},
	{"toggle-virtualdir-full-paths", 28},
	{"trash-sel", 9},
	{"untrash-sel", 11},
	{"workspace1", 10},
	{"workspace2", 10},
	{"workspace3", 10},
	{"workspace4", 10},
	{"workspace5", 10},
	{"workspace6", 10},
	{"workspace7", 10},
	{"workspace8", 10},
	{NULL, 0}
};

#if defined(LINUX_INOTIFY)
int inotify_fd = UNSET, inotify_wd = UNSET;
int watch = UNSET;
unsigned int INOTIFY_MASK =
	IN_CREATE | IN_DELETE | IN_DELETE_SELF | IN_MOVE | IN_MOVE_SELF
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 15)
	| IN_DONT_FOLLOW | IN_ONLYDIR
# endif /* LINUX >= 2.6.15 */
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
	| IN_EXCL_UNLINK
# endif /* LINUX >= 2.6.36 */
	;
#elif defined(BSD_KQUEUE)
int kq, event_fd = UNSET;
int watch = UNSET;
struct kevent events_to_monitor[NUM_EVENT_FDS];
unsigned int KQUEUE_FFLAGS = NOTE_DELETE | NOTE_EXTEND | NOTE_LINK
	| NOTE_RENAME | NOTE_REVOKE | NOTE_WRITE;
struct timespec timeout;
#elif defined(GENERIC_FS_MONITOR)
time_t curdir_mtime = 0;
#endif /* LINUX_INOTIFY */

#ifdef RUN_CMD
# ifndef _NO_TRASH
static inline void
init_trash(void)
{
	if (trash_ok == 1) {
		const filesn_t ret = count_dir(trash_files_dir, NO_CPOP);
		trash_n = ret <= 2 ? 0 : (size_t)ret - 2;
	}
}
# endif /* !_NO_TRASH */

/* Run the command passed via --cmd and exit */
static void
run_and_exit(void)
{
# ifndef _NO_TRASH
	init_trash();
# endif /* !_NO_TRASH */

	/* 1) Parse input string. */
	char **cmd = parse_input_str(cmd_line_cmd);
	if (!cmd)
		exit(EXIT_FAILURE);

	/* 2) Execute input string. */
	char **alias_cmd = check_for_alias(cmd);
	if (alias_cmd) {
		/* If an alias is found, check_for_alias() frees CMD and returns
		 * ALIAS_CMD in its place to be executed by exec_cmd_tm(). */
		exec_cmd_tm(alias_cmd);

		for (size_t i = 0; alias_cmd[i]; i++)
			free(alias_cmd[i]);
		free(alias_cmd);
		exit(exit_code);
	}

	if (!(flags & FAILED_ALIAS))
		exec_cmd_tm(cmd);
	flags &= ~FAILED_ALIAS;

	for (size_t i = 0; cmd[i]; i++)
		free(cmd[i]);
	free(cmd);

	UNHIDE_CURSOR;
	exit(exit_code);
}
#endif /* RUN_CMD */

/* This is the main structure of a basic shell (a REPL)
	 1 - Grab user input
	 2 - Parse user input
	 3 - Execute command
	 4 - Grab user input again
	 See https://brennan.io/2015/01/16/write-a-shell-in-c/
*/
__attribute__ ((noreturn))
static void
run_main_loop(void)
{
#ifdef RUN_CMD
	if (cmd_line_cmd != NULL)
		run_and_exit(); /* No return */
#endif /* RUN_CMD */

	/* 1) Infinite loop to keep the program running. */
	while (1) {
		/* 2) Grab the input string from the command prompt. */
		char *input = prompt(PROMPT_SHOW, PROMPT_SCREEN_REFRESH);
		if (!input)
			continue;

		/* 3) Parse the input string. */
		char **cmd = parse_input_str(input);
		free(input);

		if (!cmd)
			continue;

		/* 4) Execute the command. */
		char **alias_cmd = check_for_alias(cmd);
		if (alias_cmd) {
			/* If an alias is found, check_for_alias() frees CMD and returns
			 * ALIAS_CMD in its place to be executed by exec_cmd_tm(). */
			exec_cmd_tm(alias_cmd);

			for (size_t i = 0; alias_cmd[i]; i++)
				free(alias_cmd[i]);
			free(alias_cmd);
			continue;
		}

		if (!(flags & FAILED_ALIAS))
			exec_cmd_tm(cmd);
		flags &= ~FAILED_ALIAS;

		for (size_t i = 0; i <= args_n; i++)
			free(cmd[i]);
		free(cmd);
	}
}

static inline void
print_root_indicator(void)
{
#ifndef __HAIKU__
	if (user.uid == 0)
		err(ERR_NO_LOG, PRINT_PROMPT, _("%s%s%s Running as root%s\n"),
			mi_c, SET_MSG_PTR, xf_cb, df_c);
#else
	/* No need for this warning on Haiku: it runs as root by default. */
	return;
#endif /* !__HAIKU__ */
}

static inline void
list_files(void)
{
#ifdef RUN_CMD
	if (cmd_line_cmd != NULL)
		return;
#endif /* RUN_CMD */

	if (conf.autols == 1 && isatty(STDIN_FILENO)) {
		if (xargs.list_and_quit == 1)
			goto LIST;

#ifdef LINUX_INOTIFY
		/* Initialize inotify */
		inotify_fd = inotify_init1(IN_NONBLOCK);
		if (inotify_fd < 0) {
			err('w', PRINT_PROMPT, "%s: inotify: %s\n",
				PROGRAM_NAME, strerror(errno));
		}
#elif defined(BSD_KQUEUE)
		kq = kqueue();
		if (kq < 0) {
			err('w', PRINT_PROMPT, "%s: kqueue: %s\n",
				PROGRAM_NAME, strerror(errno));
		}
#endif /* LINUX_INOTIFY */

LIST:
		if (conf.colorize == 1 && xargs.eln_use_workspace_color == 1)
			set_eln_color();

		list_dir();
	}
}

static inline void
print_splash_screen(void)
{
	if (conf.splash_screen == 1) {
		splash();
		conf.splash_screen = 0;
		CLEAR;
	}
}

static inline void
check_working_directory(void)
{
	if (workspaces == NULL || !workspaces[cur_ws].path
	|| !*workspaces[cur_ws].path) {
		err(0, NOPRINT_PROMPT, _("%s: Fatal error! Failure "
			"retrieving the current working directory\n"), PROGRAM_NAME);
		exit(EXIT_FAILURE);
	}
}

static inline void
get_hostname(void)
{
	if (gethostname(hostname, sizeof(hostname)) == -1) {
		hostname[0] = UNKNOWN_CHR;
		hostname[1] = '\0';
		err('e', PRINT_PROMPT, _("%s: Error getting hostname\n"),
			PROGRAM_NAME);
	}
}

/* Initialize the file filter struct */
static inline void
init_filter(void)
{
	filter.str = NULL;
	filter.rev = 0;
	filter.env = 0;
	filter.type = FILTER_NONE;
}

/* Initialize the msgs struct */
static inline void
init_msgs(void)
{
	msgs.error = msgs.notice = msgs.warning = 0;
}

static inline void
set_locale(void)
{
	/* Use the locale specified by the environment */
	setlocale(LC_ALL, "");

	/* Check whether we have a UTF-8 encoding. */
    char *cs = nl_langinfo(CODESET);
#ifndef _BE_POSIX
    if (cs && strncasecmp(cs, "UTF", 3) == 0
#else
    if (cs && strncmp(cs, "UTF", 3) == 0
#endif /* !_BE_POSIX*/
    && (cs[3] == '8' || (cs[3] == '-' && cs[4] == '8')))
		return;

	err('w', PRINT_PROMPT, _("%s: Locale is not UTF-8. To avoid "
		"encoding issues, set a UTF-8 locale. For example: "
		"'export LANG=es_AR.UTF-8'.\n"), PROGRAM_NAME);
}

static inline void
check_gui(void)
{
	/* Running on a graphical environment? */
#if !defined(__HAIKU__) && !defined(__CYGWIN__)
	if (getenv("DISPLAY") || getenv("WAYLAND_DISPLAY"))
#endif /* !__HAIKU__ && !__CYGWIN__ */
	{
		flags |= GUI;
	}
}

#ifdef SECURITY_PARANOID
static void
set_security_paranoid_mode(void)
{
# if SECURITY_PARANOID <= 0
	return;
# elif SECURITY_PARANOID == 1
	if (xargs.secure_env != 1 && xargs.secure_env_full != 1) {
		xsecure_env(SECURE_ENV_IMPORT);
		xargs.secure_env = 1;
	}
	xargs.secure_cmds = 1;
# else
	if (xargs.secure_env_full != 1)
		xsecure_env(SECURE_ENV_FULL);
	xargs.secure_cmds = xargs.secure_env_full = 1;
	xargs.secure_env = UNSET;
#  if SECURITY_PARANOID > 2
	xargs.stealth_mode = 1;
#  endif /* SECURITY_PARANOID > 2 */
# endif /* SECURITY_PARANOID <= 0 */
}
#endif /* SECURITY_PARANOID */

#ifdef HAVE_PLEDGE
static inline void
set_pledge(void)
{
	if (pledge("stdio rpath wpath cpath dpath tmppath fattr "
	"chown flock getpw tty proc exec", NULL) == -1) {
		fprintf(stderr, "%s: pledge: %s\n", PROGRAM_NAME, strerror(errno));
		exit(errno);
	}
}
#endif /* HAVE_PLEDGE */

__attribute__ ((noreturn))
static void
list_files_and_quit(void)
{
	if (conf.light_mode == 1) {
		if (conf.long_view == 1 && prop_fields.time == PROP_TIME_BIRTH) {
			fprintf(stderr, _("%s: PropFields: 'b': Birth time is not "
				"allowed in light mode\n"), PROGRAM_NAME);
			exit(EXIT_FAILURE);
		}
		const int s = conf.sort;
		if (s != SNONE && s != SNAME && s != SEXT && s != SVER && s != SINO) {
			fprintf(stderr, _("%s: Invalid sort value: only none, "
				"name, extension, version, and inode are allowed in "
				"light mode\n"), PROGRAM_NAME);
			exit(EXIT_FAILURE);
		}
	}

	if (xargs.full_dir_size == 1)
		tmp_dir = savestring(P_tmpdir, P_tmpdir_len);

	list_files();
	exit(EXIT_SUCCESS); /* Never reached. */
}

				/**
				 * #############################
				 * #           MAIN            #
				 * #############################
				 * */

/* 1. Initialize stuff.
 * 2. Run the main program loop. */
int
main(int argc, char *argv[])
{
#ifdef HAVE_PLEDGE
	set_pledge();
#endif /* HAVE_PLEDGE */

	/* Quite unlikely to happen, but one never knows. See
	 * https://lwn.net/SubscriberLink/882799/cb8f313c57c6d8a6/
	 * and
	 * https://stackoverflow.com/questions/49817316/can-argc-be-zero-on-a-posix-system */
	if (argc == 0) {
		fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(EINVAL));
		exit(EINVAL);
	}

	/* Make sure all initialization is made with restrictive permissions. */
	const mode_t old_mask = umask(0077); /* flawfinder: ignore */

	/* # 1. INITIALIZE EVERYTHING WE NEED # */

	init_conf_struct();
	init_filter();
	init_msgs();
#ifndef _NO_ICONS
	init_icons_hashes();
#endif /* !_NO_ICONS */

	set_locale();

	/* Store external arguments to be able to rerun parse_cmdline_args()
	 * in case the user edits the config file, in which case the program
	 * must rerun init_config(), get_aliases(), get_prompt_cmds(), and
	 * then parse_cmdline_args(). */
	backup_argv(argc, argv);

	atexit(free_stuff); /* free_stuff does some cleaning. */

	user = get_user_data();
	get_home();

	check_gui();

	P_tmpdir_len = sizeof(P_tmpdir) - 1;
	init_workspaces();

	/* Set all external arguments flags to uninitialized state. */
	unset_xargs();

	/* Manage external arguments.
	 * Command line arguments will override initialization values (init_config). */
	if (argc > 1)
		parse_cmdline_args(argc, argv);
	/* parse_cmdline_args is executed before init_config() because, if
	 * specified (-P option), it sets the value of alt_profile, which
	 * is then checked by init_config(). */
#ifdef SECURITY_PARANOID
	set_security_paranoid_mode();
#endif /* SECURITY_PARANOID */

	check_term(); /* Let's check terminal capabilities. */

	/* Get paths from PATH environment variable. These paths will be
	 * used later by get_path_programs (for the autocomplete function)
	 * and get_cmd_path(). */
	path_n = get_path_env(1);
	cdpath_n = get_cdpath();

	check_env_filter();
	get_data_dir();

	/* Initialize program paths and files, set options from the config
	 * file, if they were not already set via external arguments, and
	 * load sel elements, if any. All these configurations are made
	 * per user basis. */
	init_config();
	check_options();

	if (xargs.stat > 0) /* Running with --stat(-full). Print and exit. */
		do_stat_and_exit(xargs.stat == FULL_STAT ? 1 : 0);

	if (xargs.list_and_quit == 1)
		list_files_and_quit(); /* No return. */

	set_sel_file();
	create_tmp_files();
	load_actions();
	get_aliases();

	/* Get the list of available programs in PATH to be used by the
	 * custom TAB-completion function (tab_complete(), in tabcomp.c). */
	get_path_programs();

	/* Check third-party programs availability: finders (fzf, fnf, smenu),
	 * udevil, and udisks2. */
	check_third_party_cmds();
#ifndef _NO_FZF
	check_completion_mode();
#endif /* _NO_FZF */

	/* Initialize gettext() for translations. */
#ifndef _NO_GETTEXT
	init_gettext();
#endif /* !_NO_GETTEXT */

	fputs(df_c, stdout);
	fflush(stdout);

	print_root_indicator();

	load_remotes();
	automount_remotes();
	print_splash_screen();
	set_start_path();
	check_working_directory();
	set_term_title(workspaces[cur_ws].path);
	exec_profile();
	load_dirhist();
	add_to_dirhist(workspaces[cur_ws].path);
	get_sel_files();

	/* Start listing as soon as possible to speed up startup time. */
	list_files();

	shell = get_sys_shell();
	create_kbinds_file();
	load_file_templates();
	load_bookmarks();
	load_keybinds();
	load_tags();
	load_jumpdb();
	if (!jump_db || xargs.path == 1)
		add_to_jumpdb(workspaces[cur_ws].path);

	init_shell();
	initialize_readline();
	get_prompt_cmds();
	get_hostname();
	set_env(0);

	if (config_ok == 1)
		init_history();

	/* Store history in an array to be able to manipulate it. */
	get_history();

#ifndef _NO_PROFILES
	get_profile_names();
#endif /* !_NO_PROFILES */

	load_pinned_dir();
	init_workspaces_opts();
	load_user_mimetypes();

	/* Restore user umask (if not set via Umask in the config file) */
	if (conf.umask_set != 1)
		umask(old_mask); /* flawfinder: ignore */

	/* # 2. MAIN PROGRAM LOOP # */
	run_main_loop();
}
