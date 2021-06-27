
			/*  ########################################
			 *  #               CliFM                  #
			 *  # The anti-eye-candy/KISS file manager #
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
#include <readline/history.h>
#include <readline/readline.h>

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

struct usrvar_t *usr_var = (struct usrvar_t *)NULL;
struct actions_t *usr_actions = (struct actions_t *)NULL;
struct ws_t *ws = (struct ws_t *)NULL;
struct kbinds_t *kbinds = (struct kbinds_t *)NULL;
struct jump_t *jump_db = (struct jump_t *)NULL;
struct bookmarks_t *bookmarks = (struct bookmarks_t *)NULL;
struct fileinfo *file_info = (struct fileinfo *)NULL;

/* pmsg holds the current program message type */
enum prog_msg pmsg = nomsg;

struct param xargs;

unsigned short term_cols;

int flags;

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
	filter_rev = 0;

int
    max_hist = UNSET,
    max_log = UNSET,
    max_dirhist = UNSET,
    max_path = UNSET,
    max_files = UNSET,
    min_jump_rank = UNSET,
    max_jump_total_rank = UNSET,
	max_printselfiles = UNSET,

    dirhist_cur_index = 0,
    argc_bk = 0,
    exit_code = 0,
    shell_is_interactive = 0,
    dirhist_total_index = 0,
    trash_n = 0,
    jump_total_rank = 0,
    *eln_as_file = (int *)0;

unsigned short term_cols = 0;

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

struct termios shell_tmodes;
off_t total_sel_size = 0;
pid_t own_pid = 0;

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
    *SEL_FILE = (char *)NULL,
    *STDIN_TMP_DIR = (char *)NULL,
    *sys_shell = (char *)NULL,
    *term = (char *)NULL,
    *TMP_DIR = (char *)NULL,
    *TRASH_DIR = (char *)NULL,
    *TRASH_FILES_DIR = (char *)NULL,
    *TRASH_INFO_DIR = (char *)NULL,
    *usr_cscheme = (char *)NULL,
    *user_home = (char *)NULL;

regex_t regex_exp;

size_t *ext_colors_len = (size_t *)NULL;

/* This is not a comprehensive list of commands. It only lists
 * commands long version for TAB completion */
const char *INTERNAL_CMDS[] = {
	"dup",
	"new",
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
	fprintf(stderr, "Unsupported CPU architecture\n");
	exit(EXIT_FAILURE);
#endif

	/* Though this program might perfectly work on other OSes, especially
	 * Unixes, I just didn't make any test */
#if !defined(__linux__) && !defined(linux) && !defined(__linux) && !defined(__gnu_linux__) && !defined(__FreeBSD__) && !defined(__HAIKU__)
	fprintf(stderr, _("%s: Unsupported operating system\n"),
	    PROGRAM_NAME);
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
	argc_bk = argc;
	argv_bk = (char **)xnmalloc((size_t)argc, sizeof(char *));

	register int i = argc;

	while (--i >= 0)
		argv_bk[i] = savestring(argv[i], strlen(argv[i]));

	atexit(free_stuff); /* free_stuff does some cleaning */

	/* Get user's home directory */
	user = get_user();

	if (access(user.home, W_OK) == -1) {
		/* If no user's home, or if it's not writable, there won't be
		 * any config nor trash directory. These flags are used to
		 * prevent functions from trying to access any of these
		 * directories */
		home_ok = config_ok = trash_ok = 0;
		/* Print message: trash, bookmarks, command logs, commands
		 * history and program messages won't be stored */
		_err('e', PRINT_PROMPT, _("%s: Cannot access the home directory. "
				  "Trash, bookmarks, commands logs, and commands history are "
				  "disabled. Program messages and selected files won't be "
				  "persistent. Using default options\n"), PROGRAM_NAME);
	} else
		user_home_len = strlen(user.home);

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

	/* Manage external arguments, but only if any: argc == 1 equates to
	 * no argument, since this '1' is just the program invokation name.
	 * External arguments will override initialization values
	 * (init_config) */

	ws = (struct ws_t *)xnmalloc(MAX_WS, sizeof(struct ws_t));
	i = MAX_WS;
	while (--i >= 0)
		ws[i].path = (char *)NULL;

	/* Set all external arguments flags to uninitialized state */
	unset_xargs();

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
	char locale_dir[PATH_MAX];
	snprintf(locale_dir, PATH_MAX - 1, "%s/locale", DATA_DIR
			? DATA_DIR : "/usr/share");
	bindtextdomain(PNL, locale_dir);
	textdomain(PNL);

	cschemes_n = get_colorschemes();

	set_colors(usr_cscheme ? usr_cscheme : "default", 1);

	free(usr_cscheme);
	usr_cscheme = (char *)NULL;

	if (splash_screen) {
		splash();
		splash_screen = 0;
		CLEAR;
	}

	/* Last path is overriden by the -p option in the command line */
	if (restore_last_path)
		get_last_path();

	if (cur_ws == UNSET)
		cur_ws = DEF_CUR_WS;

	if (cur_ws > MAX_WS - 1) {
		cur_ws = DEF_CUR_WS;
		_err('w', PRINT_PROMPT, _("%s: %zu: Invalid workspace."
			"\nFalling back to workspace %zu\n"), PROGRAM_NAME,
		    cur_ws, cur_ws + 1);
	}

	/* If path was not set (neither in the config file nor via command
	 * line nor via the RestoreLastPath option), set the default (CWD),
	 * and if CWD is not set, use the user's home directory, and if the
	 * home cannot be found either, try the root directory, and if
	 * there's no access to the root dir either, exit.
	 * Bear in mind that if you launch CliFM through a terminal emulator,
	 * say xterm (xterm -e clifm), xterm will run a shell, say bash, and
	 * the shell will read its config file. Now, if this config file
	 * changes the CWD, this will be the CWD for CliFM */
	if (!ws[cur_ws].path) {
		char cwd[PATH_MAX] = "";

		getcwd(cwd, sizeof(cwd));

		if (!*cwd || strlen(cwd) == 0) {
			if (user_home)
				ws[cur_ws].path = savestring(user_home, strlen(user_home));

			else {
				if (access("/", R_OK | X_OK) == -1) {
					fprintf(stderr, "%s: /: %s\n", PROGRAM_NAME,
					    strerror(errno));
					exit(EXIT_FAILURE);
				}

				else
					ws[cur_ws].path = savestring("/\0", 2);
			}
		}

		else
			ws[cur_ws].path = savestring(cwd, strlen(cwd));
	}

	/* Make path the CWD */
	/* If chdir(path) fails, set path to cwd, list files and print the
	 * error message. If no access to CWD either, exit */
	if (xchdir(ws[cur_ws].path, NO_TITLE) == -1) {

		_err('e', PRINT_PROMPT, "%s: chdir: '%s': %s\n", PROGRAM_NAME,
		    ws[cur_ws].path, strerror(errno));

		char cwd[PATH_MAX] = "";

		if (getcwd(cwd, sizeof(cwd)) == NULL) {

			_err(0, NOPRINT_PROMPT, _("%s: Fatal error! Failed "
					"retrieving current working directory\n"), PROGRAM_NAME);

			exit(EXIT_FAILURE);
		}

		if (ws[cur_ws].path)
			free(ws[cur_ws].path);

		ws[cur_ws].path = savestring(cwd, strlen(cwd));
	}

	/* Set terminal window title */
	if (xargs.cwd_in_title == 0) {
		printf("\033]2;%s\007", PROGRAM_NAME);
		fflush(stdout);
	} else {
		char *tmp = (char *)NULL;

		if (ws[cur_ws].path[1] == 'h')
			tmp = home_tilde(ws[cur_ws].path);

		printf("\033]2;%s - %s\007", PROGRAM_NAME, tmp ? tmp : ws[cur_ws].path);
		fflush(stdout);

		if (tmp)
			free(tmp);
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

	/* Copy the list of quote chars to a global variable to be used
	 * later by some of the program functions like split_str(),
	 * my_rl_quote(), is_quote_char(), and my_rl_dequote() */
	qc = savestring(rl_filename_quote_characters,
	    strlen(rl_filename_quote_characters));

	check_file_size(DIRHIST_FILE, max_dirhist);

	/* Check whether we have a working shell */
	if (access(user.shell, X_OK) == -1) {
		_err('w', PRINT_PROMPT, _("%s: %s: System shell not found. "
				"Please edit the configuration file to specify a working "
				"shell.\n"), PROGRAM_NAME, user.shell);
	}

	get_prompt_cmds();

	if (trash_ok) {
		trash_n = count_dir(TRASH_FILES_DIR);
		if (trash_n <= 2)
			trash_n = 0;
	}

	/* Get hostname */
	if (gethostname(hostname, sizeof(hostname)) == -1) {
		hostname[0] = '?';
		hostname[1] = '\0';
		_err('e', PRINT_PROMPT, _("%s: Error getting hostname\n"), PROGRAM_NAME);
	}

	/* Initialize the shell */
	init_shell();

	if (config_ok) {

		/* Limit the log files size */
		check_file_size(LOG_FILE, max_log);
		check_file_size(MSG_LOG_FILE, max_log);

		/* Get history */
		struct stat file_attrib;

		if (stat(HIST_FILE, &file_attrib) == 0 && file_attrib.st_size != 0) {
			/* If the size condition is not included, and in case of a zero
			 * size file, read_history() produces malloc errors */
			/* Recover history from the history file */
			read_history(HIST_FILE); /* This line adds more leaks to
																	readline */
			/* Limit the size of the history file to max_hist lines */
			history_truncate_file(HIST_FILE, max_hist);
		}

		/* If the history file doesn't exist, create it */
		else {
			FILE *hist_fp = fopen(HIST_FILE, "w+");

			if (!hist_fp) {
				_err('w', PRINT_PROMPT, "%s: fopen: '%s': %s\n",
				    PROGRAM_NAME, HIST_FILE, strerror(errno));
			}

			else {
				/* To avoid malloc errors in read_history(), do not
				 * create an empty file */
				fputs("edit\n", hist_fp);
				/* There is no need to run read_history() here, since
				 * the history file is still empty */
				fclose(hist_fp);
			}
		}
	}

	/* Store history into an array to be able to manipulate it */
	get_history();

	/* Check if the 'file' command is available */
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
		}

		else {
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
