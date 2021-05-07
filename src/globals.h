#pragma once

#include <sys/types.h>
#include <regex.h>
#include <termios.h>

#include "helpers.h"

//
// some structs
//

extern struct usrvar_t *usr_var;
extern struct actions_t *usr_actions;
extern struct ws_t *ws;
extern struct kbinds_t *kbinds;
extern struct jump_t *jump_db;
extern struct bookmarks_t *bookmarks;
extern struct fileinfo *file_info;
extern struct param xargs;

extern struct termios shell_tmodes;

//
// some enums
//

// pmsg holds the current program message type
static enum prog_msg pmsg = nomsg;

//
// other variables
//
extern regex_t regex_exp;


extern short splash_screen,
	welcome_message,
	show_hidden,
	clear_screen,
	disk_usage,
	list_folders_first,
	share_selbox,
	long_view,
	case_sensitive,
	cd_lists_on_the_fly,
	tips,
	logs_enabled, sort,
	classify,
	files_counter,
	light_mode,
	autocd,
	auto_open,
	dirhist_map,
	restore_last_path,
	pager,
	ext_cmd_ok,
	expand_bookmarks,
	only_dirs,
	cd_on_quit,
	columned,
	colorize,
	cur_ws,
	cp_cmd,
	mv_cmd,
	tr_as_rm,
	no_eln,
	min_name_trim,
	case_sens_dirjump,
	case_sens_path_comp,

	no_log,
	internal_cmd,
	shell_terminal,
	print_msg,
	recur_perm_error_flag,
	is_sel,
	sel_is_last,
	kbind_busy,
	unicode,
	dequoted,
	mime_match,
	sort_reverse,
	sort_switch,
	kb_shortcut,
	switch_cscheme,
	icons,
	copy_n_rename,

	home_ok,
	config_ok,
	trash_ok,
	selfile_ok;

extern int max_hist,
	max_log,
	max_dirhist,
	max_path,
	max_files,
	min_jump_rank,
	max_jump_total_rank,

	dirhist_cur_index,
	argc_bk,
	exit_code,
	shell_is_interactive,
	dirhist_total_index,
	trash_n,
	jump_total_rank,
	*eln_as_file;

extern size_t
	user_home_len,
	args_n,
	sel_n,
	msgs_n,
	prompt_cmds_n,
	path_n,
	current_hist_n,
	usrvar_n,
	aliases_n,
	longest,
	files,
	actions_n,
	ext_colors_n,
	kbinds_n,
	eln_as_file_n,
	bm_n,
	cschemes_n,
	jump_n,
	path_progsn;

extern unsigned short term_cols;
extern off_t total_sel_size;
extern pid_t own_pid;

extern char
	div_line_char,
	hostname[HOST_NAME_MAX],

	**aliases,
	**argv_bk,
	**bin_commands,
	**bookmark_names,
	**color_schemes,
	**ext_colors,
	**history,
	**messages,
	**old_pwd,
	**paths,
	**profile_names,
	**prompt_cmds,
	**sel_elements,

	*ACTIONS_FILE,
	*alt_bm_file,
	*alt_config_file,
	*alt_kbinds_file,
	*alt_profile,
	*BM_FILE,
	*COLORS_DIR,
	*CONFIG_DIR,
	*CONFIG_DIR_GRAL,
	*CONFIG_FILE,
	*cur_cscheme,
	*DIRHIST_FILE,
	*encoded_prompt,
	*file_cmd_path,
	*filter,
	*HIST_FILE,
	*KBINDS_FILE,
	*last_cmd,
	*LOG_FILE,
	*ls_colors_bk,
	*MIME_FILE,
	*MSG_LOG_FILE,
	*opener,
	*pinned_dir,
	*PLUGINS_DIR,
	*PROFILE_FILE,
	*qc,
	*SEL_FILE,
	*STDIN_TMP_DIR,
	*sys_shell,
	*term,
	*TMP_DIR,
	*TRASH_DIR,
	*TRASH_FILES_DIR,
	*TRASH_INFO_DIR,
	*user,
	*usr_cscheme,
	*user_home;

extern size_t *ext_colors_len;

extern const char *INTERNAL_CMDS[52];

/* Colors (filetype and interface) */
extern char di_c[MAX_COLOR], /* Directory */
	nd_c[MAX_COLOR], /* No read directory */
	ed_c[MAX_COLOR], /* Empty dir */
	ne_c[MAX_COLOR], /* No read empty dir */
	fi_c[MAX_COLOR], /* Reg file */
	ef_c[MAX_COLOR], /* Empty reg file */
	nf_c[MAX_COLOR], /* No read file */
	ln_c[MAX_COLOR], /* Symlink */
	or_c[MAX_COLOR], /* Broken symlink */
	pi_c[MAX_COLOR], /* FIFO, pipe */
	so_c[MAX_COLOR], /* Socket */
	bd_c[MAX_COLOR], /* Block device */
	cd_c[MAX_COLOR], /* Char device */
	su_c[MAX_COLOR], /* SUID file */
	sg_c[MAX_COLOR], /* SGID file */
	tw_c[MAX_COLOR], /* Sticky other writable */
	st_c[MAX_COLOR], /* Sticky (not ow)*/
	ow_c[MAX_COLOR], /* Other writable */
	ex_c[MAX_COLOR], /* Executable */
	ee_c[MAX_COLOR], /* Empty executable */
	ca_c[MAX_COLOR], /* Cap file */
	no_c[MAX_COLOR], /* Unknown */
	uf_c[MAX_COLOR], /* Non-'stat'able file */
	mh_c[MAX_COLOR], /* Multi-hardlink file */

	bm_c[MAX_COLOR], /* Bookmarked directory */
	el_c[MAX_COLOR], /* ELN color */
	mi_c[MAX_COLOR], /* Misc indicators color */
	df_c[MAX_COLOR], /* Default color */
	dc_c[MAX_COLOR], /* Files counter color */
	wc_c[MAX_COLOR], /* Welcome message color */
	dh_c[MAX_COLOR], /* Dirhist index color */
	dl_c[MAX_COLOR], /* Dividing line index color */

	/* Colors used in the prompt, so that \001 and \002 needs to
	 * be added. This is why MAX_COLOR + 2 */
	tx_c[MAX_COLOR + 2], /* Text color */
	li_c[MAX_COLOR + 2], /* Sel indicator color */
	ti_c[MAX_COLOR + 2], /* Trash indicator color */
	em_c[MAX_COLOR + 2], /* Error msg color */
	wm_c[MAX_COLOR + 2], /* Warning msg color */
	nm_c[MAX_COLOR + 2], /* Notice msg color */
	si_c[MAX_COLOR + 2], /* stealth indicator color */

	dir_ico_c[MAX_COLOR]; /* Directories icon color */

