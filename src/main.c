
			/*  ########################################
			 *  #               CliFM                  #
			 *  # 	The KISS/non-curses file manager   #
			 *  ######################################## */

/* GPL2+ License 
 * Copyright (C) 2016-2021, L. Abramovich <johndoe.arch@outlook.com>
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
struct ws_t *ws = (struct ws_t *)NULL;
struct kbinds_t *kbinds = (struct kbinds_t *)NULL;
struct jump_t *jump_db = (struct jump_t *)NULL;
struct bookmarks_t *bookmarks = (struct bookmarks_t *)NULL;
struct fileinfo *file_info = (struct fileinfo *)NULL;
struct remote_t *remotes = (struct remote_t *)NULL;
struct suggestions_t suggestion;

/* pmsg holds the current program message type */
enum prog_msg pmsg = nomsg;
struct param xargs;
unsigned short term_cols;
int flags;
struct termios shell_tmodes;
off_t total_sel_size = 0;
pid_t own_pid = 0;
unsigned short term_cols = 0;
regex_t regex_exp;
size_t *ext_colors_len = (size_t *)NULL;

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
    unicode = UNSET,
    min_name_trim = UNSET,
    case_sens_dirjump = UNSET,
    case_sens_path_comp = UNSET,
	print_selfiles = UNSET,
    suggestions = UNSET,

    no_log = 0,
    internal_cmd = 0,
    shell_terminal = 0,
    print_msg = 0,
    recur_perm_error_flag = 0,
    is_sel = 0,
    sel_is_last = 0,
    kbind_busy = 0,
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
    selfile_ok = 1,
	filter_rev = 0,
	bg_proc = 0;

int
    max_hist = UNSET,
    max_log = UNSET,
    max_dirhist = UNSET,
    max_path = UNSET,
    max_files = UNSET,
    min_jump_rank = UNSET,
    max_jump_total_rank = UNSET,
	max_printselfiles = UNSET,
    visible_prompt_len = UNSET,

    dirhist_cur_index = 0,
    argc_bk = 0,
    exit_code = 0,
    shell_is_interactive = 0,
    dirhist_total_index = 0,
    trash_n = 0,
    jump_total_rank = 0,
    *eln_as_file = (int *)0;

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
    path_progsn = 0,
    remotes_n = 0;

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
    *alt_config_dir = (char *)NULL,
    *alt_config_file = (char *)NULL,
    *alt_kbinds_file = (char *)NULL,
    *alt_profile = (char *)NULL,
    *BM_FILE = (char *)NULL,
    *COLORS_DIR = (char *)NULL,
    *CONFIG_DIR = (char *)NULL,
    *CONFIG_DIR_GRAL = (char *)NULL,
    *CONFIG_FILE = (char *)NULL,
    *DATA_DIR = (char *)NULL,
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
    *REMOTES_FILE = (char *)NULL,
    *SEL_FILE = (char *)NULL,
    *STDIN_TMP_DIR = (char *)NULL,
    *sys_shell = (char *)NULL,
    *term = (char *)NULL,
    *TMP_DIR = (char *)NULL,
    *TRASH_DIR = (char *)NULL,
    *TRASH_FILES_DIR = (char *)NULL,
    *TRASH_INFO_DIR = (char *)NULL,
    *usr_cscheme = (char *)NULL,
	*suggestion_buf = (char *)NULL,
    *user_home = (char *)NULL;

/* This is not a comprehensive list of commands. It only lists
 * commands long version for TAB completion */
const char *INTERNAL_CMDS[] = {
    "actions",
    "alias",
    "auto-open",
    "autocd",
    "back",
    "bookmarks",
    "colors",
    "colorschemes",
    "columns",
    "commands",
    "desel",
	"dup",
    "edit",
    "exit",
    "export",
    "filter",
    "folders-first",
    "forth",
    "help",
    "hidden",
    "history",
    "icons",
    "jump",
    "keybinds",
    "log",
    "messages",
    "mime",
    "mountpoints",
    "move",
	"new",
    "open",
    "opener",
    "pager",
    "paste",
    "path",
    "pin",
    "profile",
    "prop",
    "quit",
    "refresh",
    "reload",
    "sel",
    "selbox",
    "shell",
    "sort",
    "splash",
    "tips",
    "trash",
    "undel",
    "unicode",
    "unpin",
    "untrash",
    "version",
    NULL};

const char *PARAM_STR[] = {
	"actions edit",
	"autocd on",
	"acd on",
	"autocd off",
	"acd off",
	"autocd status",
	"acd status",
	"alias import",
	"ao on",
	"auto-open on",
	"ao off",
	"auto-open off",
	"ao status",
	"auto-open status",
	"b hist",
	"b clear",
	"back hist",
	"back clear",
	"bm add",
	"bm del",
	"bm edit",
	"bookmarks add",
	"bookmarks del",
	"bookmarks edit",
	"cs edit",
	"colorscheme edit",
	"ext on",
	"ext off",
	"ext status",
	"f hist",
	"f clear",
	"forth hist",
	"forth clear",
	"fc on",
	"filescounter on",
	"fc off",
	"filescounter off",
	"fc status",
	"filescounter status",
	"ff on",
	"folders-first on",
	"ff off",
	"folders-first off",
	"ff status",
	"folders-first status",
	"ft unset",
	"filter unset",
	"hf on",
	"hf off",
	"hf status",
	"hidden on",
	"hidden off",
	"hidden status",
	"history clear",
	"icons on",
	"icons off",
	"kb edit",
	"keybinds edit",
	"l edit",
	"lm on",
	"lm off",
	"log clear",
	"mm info",
	"mm edit",
	"mm import",
	"mime info",
	"mime edit",
	"mime import",
	"msg clear",
	"messages clear",
	"net edit",
	"net mount",
	"net unmount",
	"pg on",
	"pager on",
	"pg off",
	"pager off",
	"pg status",
	"pager status",
	"pf set",
	"pf add",
	"pf del",
	"profile set",
	"profile add",
	"profile del",
	"st rev",
	"sort rev",
	"t list",
	"t clear",
	"t del",
	"t sel",
	"tr list",
	"tr clear",
	"tr del",
	"tr sel",
	"trash list",
	"trash clear",
	"trash del",
	"trash sel",
	"u all",
	"undel all",
	"untrash all",
	"uc on",
	"unicode on",
	"uc off",
	"unicode off",
	"uc status",
	"unicode status",
	NULL};

/* To store all the 39 color variables I use, with 46 bytes each, I need
 * a total of 1,8Kb. It's not much but it could be less if I'd use
 * dynamically allocated arrays for them (which, on the other side,
 * would make the whole thing slower and more tedious) */

/* Colors (file type and interface) */
char di_c[MAX_COLOR], /* Directory */
    nd_c[MAX_COLOR],  /* No read directory */
    ed_c[MAX_COLOR],  /* Empty dir */
    ne_c[MAX_COLOR],  /* No read empty dir */
    fi_c[MAX_COLOR],  /* Reg file */
    ef_c[MAX_COLOR],  /* Empty reg file */
    nf_c[MAX_COLOR],  /* No read file */
    ln_c[MAX_COLOR],  /* Symlink */
    or_c[MAX_COLOR],  /* Broken symlink */
    pi_c[MAX_COLOR],  /* FIFO, pipe */
    so_c[MAX_COLOR],  /* Socket */
    bd_c[MAX_COLOR],  /* Block device */
    cd_c[MAX_COLOR],  /* Char device */
    su_c[MAX_COLOR],  /* SUID file */
    sg_c[MAX_COLOR],  /* SGID file */
    tw_c[MAX_COLOR],  /* Sticky other writable */
    st_c[MAX_COLOR],  /* Sticky (not ow)*/
    ow_c[MAX_COLOR],  /* Other writable */
    ex_c[MAX_COLOR],  /* Executable */
    ee_c[MAX_COLOR],  /* Empty executable */
    ca_c[MAX_COLOR],  /* Cap file */
    no_c[MAX_COLOR],  /* Unknown */
    uf_c[MAX_COLOR],  /* Non-'stat'able file */
    mh_c[MAX_COLOR],  /* Multi-hardlink file */

    bm_c[MAX_COLOR], /* Bookmarked directory */
    el_c[MAX_COLOR], /* ELN color */
    mi_c[MAX_COLOR], /* Misc indicators color */
    df_c[MAX_COLOR], /* Default color */
    dc_c[MAX_COLOR], /* Files counter color */
    wc_c[MAX_COLOR], /* Welcome message color */
    dh_c[MAX_COLOR], /* Dirhist index color */
    dl_c[MAX_COLOR], /* Dividing line index color */
    sh_c[MAX_COLOR], /* Auto-suggestions: history */
    sf_c[MAX_COLOR], /* Auto-suggestions: filenames */
    sc_c[MAX_COLOR], /* Auto-suggestions: external commands */
    sx_c[MAX_COLOR], /* Auto-suggestions: internal commands and params */

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

			/**
				 * #############################
				 * #           MAIN            #
				 * #############################
				 * */

int
main(int argc, char *argv[])
{
	/* Though this program might perfectly work on other architectures,
	 * I just didn't test anything beyond x86 and ARM */
#if !defined __x86_64__ && !defined __i386__ && !defined __ARM_ARCH
	fprintf(stderr, "%s: Unsupported CPU architecture\n", PROGRAM_NAME);
	exit(EXIT_FAILURE);
#endif

#if !defined(__linux__) && !defined(__FreeBSD__) \
&& !defined(__NetBSD__) && !defined(__OpenBSD__) && !defined(__HAIKU__)
	fprintf(stderr, _("%s: Unsupported operating system\n"), PROGRAM_NAME);
	exit(EXIT_FAILURE);
#endif

	/* If running the program locally, that is, not from a path in PATH,
	 * remove the leading "./" to get the correct program invocation
	 * name */
	if (*argv[0] == '.' && *(argv[0] + 1) == '/')
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

	if (geteuid() == 0) {
		flags |= ROOT_USR;
		_err(0, PRINT_PROMPT, _("%s%s: %sRunning as root\n"),
		    bold, PROGRAM_NAME, red);
	}

	/* Running in a graphical environment? */
#if __linux__
	if (getenv("DISPLAY") != NULL && strncmp(getenv("TERM"), "linux", 5) != 0)
#else
	if (getenv("DISPLAY") != NULL)
#endif
		flags |= GUI;

	/* Get paths from PATH environment variable. These paths will be
	 * used later by get_path_programs (for the autocomplete function)
	 * and get_cmd_path() */
	path_n = (size_t)get_path_env();

	init_workspaces();

	/* Set all external arguments flags to uninitialized state */
	unset_xargs();

	/* Manage external arguments, but only if any: argc == 1 equates to
	 * no argument, since this '1' is just the program invokation name.
	 * External arguments will override initialization values
	 * (init_config) */
	if (argc > 1)
		external_arguments(argc, argv);
	/* external_arguments is executed before init_config because, if
	 * specified (-P option), it sets the value of alt_profile, which
	 * is then checked by init_config */

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
	load_actions();
	get_aliases();

	/* Get the list of available applications in PATH to be used by my
	 * custom TAB-completion function */
	get_path_programs();

	/* Initialize gettext() for translations */
	init_gettext();

	cschemes_n = get_colorschemes();
	set_colors(usr_cscheme ? usr_cscheme : "default", 1);

	free(usr_cscheme);
	usr_cscheme = (char *)NULL;

	load_remotes();
	automount_remotes();

	if (splash_screen) {
		splash();
		splash_screen = 0;
		CLEAR;
	}

	set_start_path();

	/* Set terminal window title */
	if (flags & GUI) {
		if (xargs.cwd_in_title == 0) {
			printf("\033]2;%s\007", PROGRAM_NAME);
			fflush(stdout);
		} else {
			set_term_title(ws[cur_ws].path);
		}
	}

	exec_profile();
	load_dirhist();
	add_to_dirhist(ws[cur_ws].path);
	get_sel_files();

	/* Start listing as soon as possible to speed up startup time */
	if (cd_lists_on_the_fly && isatty(STDIN_FILENO))
		list_dir();

	create_kbinds_file();
	load_bookmarks();
	load_keybinds();
	load_jumpdb();
	if (!jump_db || xargs.path == 1)
		add_to_jumpdb(ws[cur_ws].path);

	initialize_readline();

	/*Trim the directory history file if necessary */
	check_file_size(DIRHIST_FILE, max_dirhist);

	/* Check whether we have a working shell */
	if (access(user.shell, X_OK) == -1) {
		_err('w', PRINT_PROMPT, _("%s: %s: System shell not found. "
				"Please edit the configuration file to specify a working "
				"shell.\n"), PROGRAM_NAME, user.shell);
	}

	get_prompt_cmds();

	if (trash_ok) {
		trash_n = count_dir(TRASH_FILES_DIR, NO_CPOP);
		if (trash_n <= 2)
			trash_n = 0;
	}

	if (gethostname(hostname, sizeof(hostname)) == -1) {
		hostname[0] = '?';
		hostname[1] = '\0';
		_err('e', PRINT_PROMPT, _("%s: Error getting hostname\n"), PROGRAM_NAME);
	}

	init_shell();

	if (config_ok)
		init_history();

	/* Store history into an array to be able to manipulate it */
	get_history();

	/* Check if the 'file' command is available: we need it for Lira */
	if (!opener)
		file_cmd_check();

	get_profile_names();
	load_pinned_dir();
	set_env();

				/* ###########################
				 * #   2) MAIN PROGRAM LOOP  #
				 * ########################### */

	/* This is the main structure of any basic shell
		 1 - Infinite loop
		 2 - Grab user input
		 3 - Parse user input
		 4 - Execute command
		 See https://brennan.io/2015/01/16/write-a-shell-in-c/
		 */

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
			/* If an alias is found, check_for_alias() frees cmd
			 * and returns alias_cmd in its place to be executed by
			 * exec_cmd() */
			exec_cmd(alias_cmd);

			for (i = 0; alias_cmd[i]; i++)
				free(alias_cmd[i]);

			free(alias_cmd);
			alias_cmd = (char **)NULL;
		} else {
			exec_cmd(cmd);

			i = (int)args_n + 1;
			while (--i >= 0)
				free(cmd[i]);

			free(cmd);
			cmd = (char **)NULL;
		}
	}

	return exit_code; /* Never reached */
}
