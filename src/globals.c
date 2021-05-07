#include <sys/types.h>
#include <regex.h>
#include <termios.h>

#include "helpers.h"

/* Always initialize variables, to NULL if string, to zero if int;
 * otherwise they may contain garbage, and an access to them may result
 * in a crash or some invalid data being read. However, non-initialized
 * variables are automatically initialized by the compiler */ 

//
// some structs
//

struct usrvar_t *usr_var = (struct usrvar_t *)NULL;
struct actions_t *usr_actions = (struct actions_t *)NULL;
struct ws_t *ws = (struct ws_t *)NULL;
struct kbinds_t *kbinds = (struct kbinds_t *)NULL;
struct jump_t *jump_db = (struct jump_t *)NULL;
struct bookmarks_t *bookmarks = (struct bookmarks_t *)NULL;
struct fileinfo *file_info = (struct fileinfo *)NULL;
struct param xargs;

struct termios shell_tmodes;

//
// some enums
//
enum prog_msg pmsg = nomsg;

//
// other variables
//
regex_t regex_exp;

short
	splash_screen = UNSET,
	welcome_message = UNSET,
	show_hidden = UNSET,
	clear_screen = UNSET,
	disk_usage = UNSET,
	list_folders_first = UNSET,
	share_selbox = UNSET,
	long_view = UNSET,
	case_sensitive = UNSET,
	cd_lists_on_the_fly = UNSET,
	tips = UNSET,
	logs_enabled = UNSET,
	sort = UNSET,
	classify = UNSET,
	files_counter = UNSET,
	light_mode = UNSET,
	autocd = UNSET,
	auto_open = UNSET,
	dirhist_map = UNSET,
	restore_last_path = UNSET,
	pager = UNSET,
	ext_cmd_ok = UNSET,
	expand_bookmarks = UNSET,
	only_dirs = UNSET,
	cd_on_quit = UNSET,
	columned = UNSET,
	colorize = UNSET,
	cur_ws = UNSET,
	cp_cmd = UNSET,
	mv_cmd = UNSET,
	tr_as_rm = UNSET,
	no_eln = UNSET,
	min_name_trim = UNSET,
	case_sens_dirjump = UNSET,
	case_sens_path_comp = UNSET,

	no_log = 0,
	internal_cmd = 0,
	shell_terminal = 0,
	print_msg = 0,
	recur_perm_error_flag = 0,
	is_sel = 0,
	sel_is_last = 0,
	kbind_busy = 0,
	unicode = UNSET,
	dequoted = 0,
	mime_match = 0,
	sort_reverse = 0,
	sort_switch = 0,
	kb_shortcut = 0,
	switch_cscheme = 0,
	icons = 0,
	copy_n_rename = 0,

	home_ok = 1,
	config_ok = 1,
	trash_ok = 1,
	selfile_ok = 1;

int
	max_hist = UNSET,
	max_log = UNSET,
	max_dirhist = UNSET,
	max_path = UNSET,
	max_files = UNSET,
	min_jump_rank = UNSET,
	max_jump_total_rank = UNSET,

	dirhist_cur_index = 0,
	argc_bk = 0,
	exit_code = 0,
	shell_is_interactive = 0,
	dirhist_total_index = 0,
	trash_n = 0,
	jump_total_rank = 0,
	*eln_as_file = (int *)0;

unsigned short term_cols = 0;
off_t total_sel_size = 0;
pid_t own_pid = 0;

size_t
	user_home_len = 0,
	args_n = 0,
	sel_n = 0,
	msgs_n = 0,
	prompt_cmds_n = 0,
	path_n = 0,
	current_hist_n = 0,
	usrvar_n = 0,
	aliases_n = 0,
	longest = 0,
	files = 0,
	actions_n = 0,
	ext_colors_n = 0,
	kbinds_n = 0,
	eln_as_file_n = 0,
	bm_n = 0,
	cschemes_n = 0,
	jump_n = 0,
	path_progsn = 0;

char
	div_line_char = UNSET,
	hostname[HOST_NAME_MAX],

	**aliases = (char **)NULL,
	**argv_bk = (char **)NULL,
	**bin_commands = (char **)NULL,
	**bookmark_names = (char **)NULL,
	**color_schemes = (char **)NULL,
	**ext_colors = (char **)NULL,
	**history = (char **)NULL,
	**messages = (char **)NULL,
	**old_pwd = (char **)NULL,
	**paths = (char **)NULL,
	**profile_names = (char **)NULL,
	**prompt_cmds = (char **)NULL,
	**sel_elements = (char **)NULL,

	*ACTIONS_FILE = (char *)NULL,
	*alt_bm_file = (char *)NULL,
	*alt_config_file = (char *)NULL,
	*alt_kbinds_file = (char *)NULL,
	*alt_profile = (char *)NULL,
	*BM_FILE = (char *)NULL,
	*COLORS_DIR = (char *)NULL,
	*CONFIG_DIR = (char *)NULL,
	*CONFIG_DIR_GRAL = (char *)NULL,
	*CONFIG_FILE = (char *)NULL,
	*cur_cscheme = (char *)NULL,
	*DIRHIST_FILE = (char *)NULL,
	*encoded_prompt = (char *)NULL,
	*file_cmd_path = (char *)NULL,
	*filter = (char *)NULL,
	*HIST_FILE = (char *)NULL,
	*KBINDS_FILE = (char *)NULL,
	*last_cmd = (char *)NULL,
	*LOG_FILE = (char *)NULL,
	*ls_colors_bk = (char *)NULL,
	*MIME_FILE = (char *)NULL,
	*MSG_LOG_FILE = (char *)NULL,
	*opener = (char *)NULL,
	*pinned_dir = (char *)NULL,
	*PLUGINS_DIR = (char *)NULL,
	*PROFILE_FILE = (char *)NULL,
	*qc = (char *)NULL,
	*SEL_FILE = (char *)NULL,
	*STDIN_TMP_DIR = (char *)NULL,
	*sys_shell = (char *)NULL,
	*term = (char *)NULL,
	*TMP_DIR = (char *)NULL,
	*TRASH_DIR = (char *)NULL,
	*TRASH_FILES_DIR = (char *)NULL,
	*TRASH_INFO_DIR = (char *)NULL,
	*user = (char *)NULL,
	*usr_cscheme = (char *)NULL,
	*user_home = (char *)NULL;

size_t *ext_colors_len = (size_t *)NULL;

/* This is not a comprehensive list of commands. It only lists
 * commands long version for TAB completion */
const char *INTERNAL_CMDS[] = {
	"alias",
	"open",
	"prop",
	"back",
	"forth",
	"move",
	"paste",
	"sel",
	"selbox",
	"desel",
	"refresh",
	"edit",
	"history",
	"hidden",
	"path",
	"help",
	"commands",
	"colors",
	"version",
	"splash",
	"folders-first",
	"opener",
	"exit",
	"quit",
	"pager",
	"trash",
	"undel",
	"messages",
	"mountpoints",
	"bookmarks",
	"log",
	"untrash",
	"unicode",
	"profile",
	"shell",
	"mime",
	"sort",
	"tips",
	"autocd",
	"auto-open",
	"actions",
	"reload",
	"export",
	"keybinds",
	"pin",
	"unpin",
	"colorschemes",
	"jump",
	"icons",
	"columns",
	"filter",
	NULL
};

// Colors (filetype and interface)
char di_c[MAX_COLOR], /* Directory */
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

