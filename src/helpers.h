/* helpers.h -- main header file */

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

#ifndef HELPERS_H
#define HELPERS_H

#if defined(__linux__) && !defined(_BE_POSIX)
#define _GNU_SOURCE
#else
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE /* wcwidth() */
#if defined(__FreeBSD__)
#define __XSI_VISIBLE 700
#define __BSD_VISIBLE 1
#endif
#ifdef __NetBSD__
#define _NETBSD_SOURCE
#endif
#ifdef __OpenBSD__
#define _BSD_SOURCE
#endif
#endif

/* Setting GLOB_BRACE to ZERO disables support for GLOB_BRACE if not
 * available on current platform */
#if !defined(__TINYC__) && !defined(GLOB_BRACE)
#define GLOB_BRACE 0
#endif

/* Support large files on ARM or 32-bit machines */
#if defined(__arm__) || defined(__i386__)
#define _FILE_OFFSET_BITS 64
#endif

#include <libintl.h>
#include <regex.h>
#include <stddef.h>
#include <stdlib.h>

#if defined(__linux__)
#include <linux/version.h>
#include <sys/inotify.h>
#include <sys/types.h>
#define LINUX_INOTIFY
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#define BSD_KQUEUE
#endif /* __linux__ */

#include "init.h"
#include "strings.h"
#include "messages.h"
#include "settings.h"

#if __TINYC__
void *__dso_handle;
#endif

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 64
#endif

#ifndef NAME_MAX
#define NAME_MAX 255
#endif

/* _GNU_SOURCE is only defined if __linux__ is defined and _BE_POSIX
 * is not defined */
#ifdef _GNU_SOURCE
#if (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 28))
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#define _STATX
#endif /* LINUX_VERSION (4.11) */
#endif /* __GLIBC__ */
#endif /* _GNU_SOURCE */

/* Because capability.h is deprecated in BSD */
#if __linux__
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
#define _LINUX_CAP
#endif /* LINUX_VERSION (2.6.24)*/
#endif /* __linux__ */

/* Define our own boolean type
#undef bool
#define bool signed char
#undef TRUE
#undef FALSE
#define TRUE 1
#define FALSE 0 */

/* Event handling */
#ifdef LINUX_INOTIFY
#define NUM_EVENT_SLOTS 32 /* Make room for 32 events */
#define EVENT_SIZE (sizeof(struct inotify_event))
#define EVENT_BUF_LEN (EVENT_SIZE * NUM_EVENT_SLOTS)
extern int inotify_fd, inotify_wd;
extern unsigned int INOTIFY_MASK;
#elif defined(BSD_KQUEUE)
#define NUM_EVENT_SLOTS 10
#define NUM_EVENT_FDS 10
extern int kq, event_fd;
extern struct kevent events_to_monitor[];
extern unsigned int KQUEUE_FFLAGS;
extern struct timespec timeout;
#endif /* LINUX_INOTIFY */
extern int watch;

#define PROGRAM_NAME "CliFM"
#define PNL "clifm" /* Program name lowercase */
#define PROGRAM_DESC "The command line file manager"
#ifndef __HAIKU__
#define CLEAR if (write(STDOUT_FILENO, "\033c", 2) <= 0) {}
#else
#define CLEAR fputs("\x1b[H\x1b[2J", stdout);
#endif
#define VERSION "1.3"
#define AUTHOR "L. Abramovich"
#define CONTACT "johndoe.arch@outlook.com"
#define WEBSITE "https://github.com/leo-arch/clifm"
#define DATE "Dec 5, 2021"
#define LICENSE "GPL2+"

/* Options flags */
#define FOLDERS_FIRST (1 << 1)
#define HELP          (1 << 2)
#define HIDDEN        (1 << 3)
#define AUTOLS        (1 << 4)
#define SPLASH        (1 << 5)
#define CASE_SENS     (1 << 6)
#define START_PATH    (1 << 7)
#define PRINT_VERSION (1 << 8)
#define ALT_PROFILE   (1 << 9)

/* File ownership flags */
#define R_USR (1 << 1)
#define X_USR (1 << 2)
#define R_GRP (1 << 3)
#define X_GRP (1 << 4)
#define R_OTH (1 << 5)
#define X_OTH (1 << 6)

/* Internal flags */
#define ROOT_USR      (1 << 10)
#define EXT_HELP      (1 << 11)
//#define FILE_CMD_OK (1 << 12)
#define GUI           (1 << 13)
#define IS_USRVAR_DEF (1 << 14)

/* Used by log_msg() to know wether to tell prompt() to print messages or
 * not */
#define PRINT_PROMPT   1
#define NOPRINT_PROMPT 0

/* Macros for xchdir (for setting term title or not) */
#define SET_TITLE 1
#define NO_TITLE  0

/* Macros for cd_function */
#define CD_PRINT_ERROR    1
#define CD_NO_PRINT_ERROR 0

/* Macros for the count_dir function. CPOP tells the function to only
 * check if a given directory is populated (it has at least 3 files) */
#define CPOP    1
#define NO_CPOP 0

/* Error codes, used by the launch_exec functions */
#define EXNULLERR  79
#define EXFORKERR  81
#define EXCRASHERR 82

#define BACKGROUND 1
#define FOREGROUND 0

/* A few colors */
#define GRAY   "\x1b[1;30m"
#define _RED   "\x1b[1;31m"
#define _GREEN "\x1b[0;32m"
#define D_CYAN "\x1b[0;36m"
#define BOLD   "\x1b[1m"
#define NC     "\x1b[0m"

#define COLORS_REPO "https://github.com/leo-arch/clifm-colors"

/* Colors for the prompt: */
/* \001 and \002 tell readline that color codes between them are
 * non-printing chars. This is specially useful for the prompt, i.e.,
 * when passing color codes to readline */
#define RL_NC    "\001\x1b[0m\002"
#define DLFC     "\x1b[0K"         /* Delete line from cursor */
#define CNL      "\x1b[1E"         /* Move the cursor to beginning next line*/
#define DLFC_LEN 4                 /* Length of the above escape codes */

#define UNSET -1

/* Macros for the cp and mv cmds */
#define CP_CP    0
#define CP_ADVCP 1
#define CP_WCP   2
#define MV_MV    0
#define MV_ADVMV 1

/* Sort macros */
#define SNONE      0
#define SNAME      1
#define SSIZE      2
#define SATIME     3
#define SBTIME     4
#define SCTIME     5
#define SMTIME     6
#define SVER       7
#define SEXT       8
#define SINO       9
#define SOWN       10
#define SGRP       11
#define SORT_TYPES 11

/* Macros for the colors_list function */
#define NO_ELN        0
#define NO_NEWLINE    0
#define NO_PAD        0
#define PRINT_NEWLINE 1

/* A few key macros used by the auto-suggestions system */
#define _ESC   27
#define _TAB   9
#define BS     8
#define DELETE 127
#define ENTER  13
/* #define OP_BRACKET 91
#define UC_O 79
#define SPACE 32 */

/* Macros to specify suggestions type */
#define NO_SUG         0
#define HIST_SUG       1
#define FILE_SUG       2
#define CMD_SUG        3
#define INT_CMD        4
#define COMP_SUG       5
#define BOOKMARK_SUG   6
#define ALIAS_SUG      7
#define ELN_SUG        8
#define FIRST_WORD     9
#define JCMD_SUG       10
#define JCMD_SUG_NOACD 11 /* No auto-cd */
#define VAR_SUG	       12
#define SEL_SUG	       13

/* 46 == \x1b[00;38;02;000;000;000;00;48;02;000;000;000m\0 (24bit, RGB
 * true color format including foreground and background colors, the SGR
 * (Select Graphic Rendition) parameter, and, of course, the terminating
 * null byte. */
#define MAX_COLOR 46

/* Macros to control file descriptors in exec functions */
#define E_NOFLAG   0
#define E_NOSTDIN  (1 << 1)
#define E_NOSTDOUT (1 << 2)
#define E_NOSTDERR (1 << 3)
#define E_MUTE     (E_NOSTDOUT | E_NOSTDERR)

/* Macros for ELN padding */
#define NOPAD         0
#define ZEROPAD       1
#define LEFTSPACEPAD  2
#define RIGHTSPACEPAD 3

/* Macros for the clear_suggestion function */
#define CS_FREEBUF 1
#define CS_KEEPBUF 0

/* Macros for the xmagic function */
#define MIME_TYPE 1
#define TEXT_DESC 0

/* Max length of the properties string in long view mode */
#define MAX_PROP_STR 55

/* Macros for the prompt style */
#define DEF_PROMPT_STYLE    0
#define CUSTOM_PROMPT_STYLE 1

/* Macros for the dirjump function */
#define SUG_JUMP    0
#define NO_SUG_JUMP 1

/* Macros for the media_menu function */
#define MEDIA_LIST 	0
#define MEDIA_MOUNT	1

/* Macros for the rl_highlight function */
#define SET_COLOR    1
#define INFORM_COLOR 0

#define MB_LEN_MAX 16

#define TMP_FILENAME ".clifmXXXXXX"

#ifndef P_tmpdir
#define P_tmpdir "/tmp"
#endif

#define itoa xitoa /* itoa does not exist in some OS's */
//#define atoi xatoi

#ifndef _NO_GETTEXT
#define _(String) gettext(String)
#else
#define _(String) String
#endif /* _GETTEXT */

#define strlen(s) xstrnlen(s)

#define ENTRY_N 64

#define UNUSED(x) (void)x /* Just silence the compiler's warning */
#define TOUPPER(ch) (((ch) >= 'a' && (ch) <= 'z') ? ((ch) - 'a' + 'A') : (ch))
#define DIGINUM(n) (((n) < 10) ? 1 \
		: ((n) < 100)        ? 2 \
		: ((n) < 1000)       ? 3 \
		: ((n) < 10000)      ? 4 \
		: ((n) < 100000)     ? 5 \
		: ((n) < 1000000)    ? 6 \
		: ((n) < 10000000)   ? 7 \
		: ((n) < 100000000)  ? 8 \
		: ((n) < 1000000000) ? 9 \
				      : 10)
#define _ISDIGIT(n) ((unsigned int)(n) - '0' <= 9)
#define _ISALPHA(n) ((unsigned int)(n) >= 'a' && (unsigned int)(n) <= 'z')
#define SELFORPARENT(n) (*(n) == '.' && (!(n)[1] || ((n)[1] == '.' && !(n)[2])))

/* dirjump macros for calculating directories rank extra points */
#define BASENAME_BONUS 	300
#define BOOKMARK_BONUS 	500
#define PINNED_BONUS    1000
#define WORKSPACE_BONUS 300
/* Last directory access */
#define JHOUR(n)  ((n) *= 4) /* Within last hour */
#define JDAY(n)   ((n) *= 2) /* Within last day */
#define JWEEK(n)  ((n) / 2)  /* Within last week */
#define JOLDER(n) ((n) / 4)  /* More than a week */

#if defined(__HAIKU__)
# define DT_UNKNOWN 0
# define DT_FIFO    1
# define DT_CHR     2
# define DT_DIR     4
# define DT_BLK     6
# define DT_REG     8
# define DT_LNK     10
# define DT_SOCK    12
#endif

#define DT_NONE     14

/* Macros for the get_sys_shell function */
#define SHELL_NONE 0
#define SHELL_BASH 1
#define SHELL_DASH 2
#define SHELL_FISH 3
#define SHELL_KSH  4
#define SHELL_TCSH 5
#define SHELL_ZSH  6

#define BELL_NONE          0
#define BELL_AUDIBLE       1
#define BELL_VISIBLE       2
#define VISIBLE_BELL_DELAY 30
#define DEF_BELL_STYLE     BELL_VISIBLE

				/** #########################
				 *  #    GLOBAL VARIABLES   #
				 *  ######################### */

/* Struct to store information about current user */
struct user_t {
	char *home;
	char *name;
	char *shell;
	size_t home_len;
	uid_t uid;
	gid_t gid;
};

extern struct user_t user;

/* Struct to store user defined variables */
struct usrvar_t {
	char *name;
	char *value;
};

extern struct usrvar_t *usr_var;

/* Struct to store user defined actions */
struct actions_t {
	char *name;
	char *value;
};

extern struct actions_t *usr_actions;

/* Workspaces information */
struct ws_t {
	char *path;
	int num;
	int pad;
};

extern struct ws_t *ws;

/* Struct to store user defined keybindings */
struct kbinds_t {
	char *function;
	char *key;
};

extern struct kbinds_t *kbinds;

/* Struct to store the dirjump database values */
struct jump_t {
	char *path;
	int keep;
	int rank;
	size_t visits;
	time_t first_visit;
	time_t last_visit;
};

extern struct jump_t *jump_db;

/* Struct to store bookmarks */
struct bookmarks_t {
	char *shortcut;
	char *name;
	char *path;
};

extern struct bookmarks_t *bookmarks;

struct alias_t {
	char *name;
	char *cmd;
};

extern struct alias_t *aliases;

/* Struct to store file information */
struct fileinfo {
	char *color;
	char *ext_color;
#ifndef _NO_ICONS
	char *icon;
	char *icon_color;
#endif
	char *name;
	int dir;
	int eln_n;
	int exec;
	int filesn; /* Number of files in subdir */
	int ruser; /* User read permission for dir */
	int symlink;
	int sel;
	int pad;
	size_t len;
	mode_t mode; /* Store st_mode (for long view mode) */
	mode_t type; /* Store d_type value */
	ino_t inode;
	off_t size;
	uid_t uid;
	gid_t gid;
	nlink_t linkn;
	time_t ltime; /* For long view mode */
	time_t time;
};

extern struct fileinfo *file_info;

struct devino_t {
	dev_t dev;
	ino_t ino;
	char pad1;
	char pad2;
	char pad3;
	char pad4;
	char pad5;
	char pad6;
	char pad7;
	char mark;
};

extern struct devino_t *sel_devino;

struct autocmds_t {
	char *pattern;
	char *color_scheme;
	char *cmd;
	int long_view;
	int light_mode;
	int files_counter;
	int max_files;
	int max_name_len;
	int show_hidden;
	int sort;
	int sort_reverse;
	int pager;
	int pad;
};

extern struct autocmds_t *autocmds;

struct opts_t {
	char *color_scheme;
	int long_view;
	int light_mode;
	int files_counter;
	int max_files;
	int max_name_len;
	int show_hidden;
	int sort;
	int sort_reverse;
	int pager;
	int pad;
};

extern struct opts_t opts;

/* Struct to specify which parameters have been set from the command
 * line, to avoid overriding them with init_config(). While no command
 * line parameter will be overriden, the user still can modifiy on the
 * fly (editing the config file) any option not specified in the command
 * line */
struct param {
	int auto_open;
	int autocd;
	int autojump;
	int autols;
	int bm_file;
	int case_sens_dirjump;
	int case_sens_path_comp;
	int clear_screen;
	int colorize;
	int columns;
	int config;
	int cwd_in_title;
	int dirmap;
	int disk_usage;
	int cd_on_quit;
	int check_cap;
	int check_ext;
	int classify;
	int color_scheme;
	int control_d_exits;
	int expand_bookmarks;
	int ext;
	int ffirst;
	int files_counter;
	int follow_symlinks;
#ifndef _NO_FZF
	int fzftab;
#endif
	int hidden;
#ifndef _NO_HIGHLIGHT
	int highlight;
#endif
	int horizontal_list;
#ifndef _NO_ICONS
	int icons;
#endif
	int icons_use_file_color;
	int int_vars;
	int list_and_quit;
	int light;
	int logs;
	int longview;
	int max_dirhist;
	int max_files;
	int max_path;
	int mount_cmd;
	int no_dirjump;
	int noeln;
	int only_dirs;
	int pager;
	int path;
	int printsel;
	int restore_last_path;
	int rl_vi_mode;
	int sensitive;
	int share_selbox;
	int sort;
	int sort_reverse;
	int splash;
//	int stderr;
	int stealth_mode;
#ifndef _NO_SUGGESTIONS
	int suggestions;
#endif
	int tips;
#ifndef _NO_TRASH
	int trasrm;
#endif
	int unicode;
	int welcome_message;
	int warning_prompt;
};

extern struct param xargs;

/* Struct to store remotes information */
struct remote_t {
	char *desc;
	char *name;
	char *mount_cmd;
	char *mountpoint;
	char *unmount_cmd;
	int auto_mount;
	int auto_unmount;
	int mounted;
	int padding;
};

extern struct remote_t *remotes;

struct suggestions_t {
	int filetype;
    int printed;
	int type;
    int offset;
	char *color;
	size_t full_line_len;
	size_t nlines;
};

extern struct suggestions_t suggestion;

/* A list of possible program messages. Each value tells the prompt what
 * to do with error messages: either to print an E, W, or N char at the
 * beginning of the prompt, or nothing (nomsg) */
enum prog_msg {
	NOMSG = 	0,
	ERROR = 	1,
	WARNING = 	2,
	NOTICE = 	4
};

/* pmsg holds the current program message type */
extern enum prog_msg pmsg;

/* Enumeration for the dirjump function options */
enum jump {
	NONE = 		0,
	JPARENT =	1,
	JCHILD =	2,
	JORDER =	4,
	JLIST =		8
};

enum comp_type {
	TCMP_BOOKMARK =	0,
	TCMP_CMD =		1,
	TCMP_CSCHEME = 	2,
	TCMP_DESEL = 	3,
	TCMP_ELN =		4,
	TCMP_HIST = 	5,
	TCMP_JUMP = 	6,
	TCMP_NET =		7,
	TCMP_NONE = 	8,
	TCMP_OPENWITH = 9,
	TCMP_PATH = 	10,
	TCMP_PROF = 	11,
	TCMP_RANGES = 	12,
	TCMP_SEL =		13,
	TCMP_SORT = 	14,
	TCMP_TRASHDEL=	15,
	TCMP_UNTRASH=	16
};

extern enum comp_type cur_comp_type;

extern struct termios orig_termios;

extern int
	curcol,
	currow,
	flags;

extern int
	auto_open,
	autocd,
	autocmd_set,
	autojump,
	autols,
	bell,
	bg_proc,
	case_sensitive,
	case_sens_dirjump,
	case_sens_path_comp,
	case_sens_search,
	cd_on_quit,
	check_cap,
	check_ext,
	classify,
	clear_screen,
	cmdhist_flag,
	colorize,
	columned,
	config_ok,
	control_d_exits,
	copy_n_rename,
	cp_cmd,
	cur_ws,
	dequoted,
	dir_changed, /* flag to know is dir was changed: used by autocmds */
	dirhist_map,
	disk_usage,
	elnpad,
	expand_bookmarks,
	ext_cmd_ok,
	files_counter,
	filter_rev,
	follow_symlinks,
#ifndef _NO_FZF
	fzftab,
#endif
#ifndef _NO_HIGHLIGHT
	highlight,
#endif
	home_ok,
#ifndef _NO_ICONS
	icons,
#endif
	int_vars,
	internal_cmd,
	is_sel,
	kb_shortcut,
	kbind_busy,
	light_mode,
	list_folders_first,
	listing_mode,
	logs_enabled,
	long_view,
	max_name_len,
	mime_match,
	min_name_trim,
	mv_cmd,
	no_eln,
	no_log,
	only_dirs,
	open_in_foreground, /* Override mimelist file: used by mime_open */
	pager,
	print_msg,
	print_selfiles,
	prompt_offset,
	prompt_style,
	recur_perm_error_flag,
	restore_last_path,
	rl_last_word_start,
	rl_nohist,
	rl_notab,
	sel_is_last,
	selfile_ok,
	share_selbox,
	shell,
	shell_terminal,
	show_hidden,
	sort,
	sort_reverse,
	sort_switch,
	splash_screen,
	suggest_filetype_color,
	suggestions,
	switch_cscheme,
	tips,
#ifndef _NO_TRASH
	tr_as_rm,
	trash_ok,
#endif
	unicode,
	warning_prompt,
	welcome_message,
	_xrename,
	xrename; /* We're running a secondary prompt for the rename function */

//#ifndef _NO_HIGHLIGHT
extern int wrong_cmd;
extern int wrong_cmd_line;
//#endif

extern int
	argc_bk,
	dirhist_cur_index,
	dirhist_total_index,
	exit_code,
	jump_total_rank,
	max_dirhist,
	max_files,
	max_hist,
	max_jump_total_rank,
	max_log,
	max_path,
	max_printselfiles,
	min_jump_rank,
	trash_n,
	shell_is_interactive,
	*eln_as_file;

extern unsigned short term_cols, term_rows;

extern size_t
	actions_n,
	aliases_n,
	args_n,
	autocmds_n,
	bm_n,
	cdpath_n,
	cschemes_n,
	current_hist_n,
	curhistindex,
	eln_as_file_n,
	ext_colors_n,
	files,
	jump_n,
	kbinds_n,
	longest,
	msgs_n,
	P_tmpdir_len,
	path_n,
	path_progsn,
	prompt_cmds_n,
	remotes_n,
	sel_n,
	tab_offset,
	user_home_len,
	usrvar_n,
	nwords;

extern struct termios shell_tmodes;
extern off_t total_sel_size;
extern pid_t own_pid;

extern char
	div_line_char[NAME_MAX],
	hostname[HOST_NAME_MAX],

	*actions_file,
	*alt_config_dir,
	*alt_bm_file,
	*alt_config_file,
	*alt_kbinds_file,
	*alt_profile,
	*bm_file,
	*colors_dir,
	*config_dir,
	*config_dir_gral,
	*config_file,
	*cur_color,
	*data_dir,
	*cur_cscheme,
	*dirhist_file,
	*encoded_prompt,
	*file_cmd_path,
	*_filter,
	*fzftab_options,
	*hist_file,
	*kbinds_file,
	*jump_suggestion,
	*last_cmd,
	*log_file,
	*ls_colors_bk,
	*mime_file,
	*msg_log_file,
	*opener,
	*pinned_dir,
	*plugins_dir,
	*profile_file,
	*qc,
	*remotes_file,
	*sel_file,
	*stdin_tmp_dir,
#ifndef _NO_SUGGESTIONS
	*suggestion_buf,
	*suggestion_strategy,
#endif
	*term,
//	*term_bgcolor,
	*tmp_dir,
#ifndef _NO_TRASH
	*trash_dir,
	*trash_files_dir,
	*trash_info_dir,
#endif
	*usr_cscheme,
	*user_home,
	*wprompt_str,

	**argv_bk,
	**bin_commands,
	**bookmark_names,
	**cdpaths,
	**color_schemes,
	**ext_colors,
	**history,
	**messages,
	**old_pwd,
	**paths,
	**profile_names,
	**prompt_cmds,
	**sel_elements;

extern const char
	*internal_cmds[],
	*param_str[];

extern regex_t regex_exp;
extern size_t *ext_colors_len;
extern char **environ;

/* To store all the 39 color variables I use, with 46 bytes each, I need
 * a total of 1,8Kb. It's not much but it could be less if I'd use
 * dynamically allocated arrays for them (which, on the other side,
 * would make the whole thing slower and more tedious) */

/* Colors */
extern char
	/* File types */
	bd_c[MAX_COLOR],	/* Block device */
	ca_c[MAX_COLOR],	/* Cap file */
	cd_c[MAX_COLOR],	/* Char device */
	ee_c[MAX_COLOR],	/* Empty executable */
	ex_c[MAX_COLOR],	/* Executable */
	ef_c[MAX_COLOR],	/* Empty reg file */
	ed_c[MAX_COLOR],	/* Empty dir */
	fi_c[MAX_COLOR],	/* Reg file */
	di_c[MAX_COLOR],	/* Directory */
	ln_c[MAX_COLOR],	/* Symlink */
	mh_c[MAX_COLOR],	/* Multi-hardlink file */
	nd_c[MAX_COLOR],	/* No read directory */
	ne_c[MAX_COLOR],	/* No read empty dir */
	nf_c[MAX_COLOR],	/* No read file */
	no_c[MAX_COLOR],	/* Unknown */
	or_c[MAX_COLOR],	/* Broken symlink */
	ow_c[MAX_COLOR],	/* Other writable */
	pi_c[MAX_COLOR],	/* FIFO, pipe */
	sg_c[MAX_COLOR],	/* SGID file */
	so_c[MAX_COLOR],	/* Socket */
	st_c[MAX_COLOR],	/* Sticky (not ow)*/
	su_c[MAX_COLOR],	/* SUID file */
	tw_c[MAX_COLOR],	/* Sticky other writable */
	uf_c[MAX_COLOR],	/* Non-'stat'able file */

	/* Interface */
	bm_c[MAX_COLOR],	/* Bookmarked directory */
	dc_c[MAX_COLOR],	/* Files counter */
	df_c[MAX_COLOR],	/* Default color */
	dh_c[MAX_COLOR],	/* Dirhist index */
	dl_c[MAX_COLOR],	/* Dividing line */
	el_c[MAX_COLOR],	/* ELN color */
	mi_c[MAX_COLOR],	/* Misc indicators */
	ts_c[MAX_COLOR],	/* TAB completion suffix */
	wc_c[MAX_COLOR],	/* Welcome message */
	wp_c[MAX_COLOR],	/* Warning prompt */

	/* Suggestions */
	sb_c[MAX_COLOR],	/* Auto-suggestions: shell builtins */
	sc_c[MAX_COLOR],	/* Auto-suggestions: external commands */
	sf_c[MAX_COLOR],	/* Auto-suggestions: filenames */
	sh_c[MAX_COLOR],	/* Auto-suggestions: history */
	sx_c[MAX_COLOR],	/* Auto-suggestions: internal commands and params */
	sp_c[MAX_COLOR],	/* Auto-suggestions: suggestions pointer */

#ifndef _NO_ICONS
    dir_ico_c[MAX_COLOR], /* Directories icon color */
#endif

	/* Syntax highlighting */
	hb_c[MAX_COLOR],	/* Brackets () [] {} */
	hc_c[MAX_COLOR],	/* Comments */
	hd_c[MAX_COLOR],	/* Paths (slashes) */
	he_c[MAX_COLOR],	/* Expansion operators: * ~ */
	hn_c[MAX_COLOR],	/* Numbers */
	hp_c[MAX_COLOR],	/* Parameters: - */
	hq_c[MAX_COLOR],	/* Quoted strings */
	hr_c[MAX_COLOR],	/* Redirection > */
	hs_c[MAX_COLOR],	/* Process separators | & ; */
	hv_c[MAX_COLOR],	/* Variables $ */
	hw_c[MAX_COLOR],	/* Wrong, non-existent command name $ */

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
	ti_c[MAX_COLOR + 2], /* Trash indicator color */
	tx_c[MAX_COLOR + 2], /* Text color */
	si_c[MAX_COLOR + 2], /* stealth indicator color */
	wm_c[MAX_COLOR + 2]; /* Warning msg color */

#endif /* HELPERS_H */
