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

#pragma once

#if defined(__linux__) && !defined(_BE_POSIX)
#define _GNU_SOURCE
#else
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
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

/* Setting GLOB_BRACE to ZERO which disables support for GLOB_BRACE if not available on current platform */
#ifndef GLOB_BRACE
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
#define PROG_DESC "The command line file manager"
#ifndef __HAIKU__
#define CLEAR if (write(STDOUT_FILENO, "\033c", 2) <= 0) {}
#else
#define CLEAR fputs("\x1b[H\x1b[2J", stdout);
#endif
#define VERSION "1.3"
#define AUTHOR "L. Abramovich"
#define CONTACT "johndoe.arch@outlook.com"
#define WEBSITE "https://github.com/leo-arch/clifm"
#define DATE "Oct 3, 2021"
#define LICENSE "GPL2+"

/* Options flags */
#define FOLDERS_FIRST (1 << 1)
#define HELP (1 << 2)
#define HIDDEN (1 << 3)
#define ON_THE_FLY (1 << 4)
#define SPLASH (1 << 5)
#define CASE_SENS (1 << 6)
#define START_PATH (1 << 7)
#define PRINT_VERSION (1 << 8)
#define ALT_PROFILE (1 << 9)

/* File ownership flags */
#define R_USR (1 << 1)
#define X_USR (1 << 2)
#define R_GRP (1 << 3)
#define X_GRP (1 << 4)
#define R_OTH (1 << 5)
#define X_OTH (1 << 6)

/* Internal flags */
#define ROOT_USR (1 << 10)
#define EXT_HELP (1 << 11)
//#define FILE_CMD_OK (1 << 12)
#define GUI (1 << 13)
#define IS_USRVAR_DEF (1 << 14)

/* Used by log_msg() to know wether to tell prompt() to print messages or
 * not */
#define PRINT_PROMPT 1
#define NOPRINT_PROMPT 0

/* Macros for xchdir (for setting term title or not) */
#define SET_TITLE 1
#define NO_TITLE 0

/* Macros for cd_function */
#define CD_PRINT_ERROR 1
#define CD_NO_PRINT_ERROR 0

/* Macros for the count_dir function. CPOP tells the function to only
 * check if a given directory is populated (it has at least 3 files) */
#define CPOP 1
#define NO_CPOP 0

/* Error codes, used by the launch_exec functions */
#define EXNULLERR 79
#define EXFORKERR 81
#define EXCRASHERR 82

#define BACKGROUND 1
#define FOREGROUND 0

/* A few colors */
#define GRAY "\x1b[1;30m"
#define _RED "\x1b[1;31m"
#define _GREEN "\x1b[0;32m"
#define D_CYAN "\x1b[0;36m"
#define BOLD "\x1b[1m"
#define NC "\x1b[0m"

#define COLORS_REPO "https://github.com/leo-arch/clifm-colors"

/* Colors for the prompt: */
/* \001 and \002 tell readline that color codes between them are
 * non-printing chars. This is specially useful for the prompt, i.e.,
 * when passing color codes to readline */
#define RL_NC "\001\x1b[0m\002"

/* Default color definitions */
#define DEF_LS_COLORS "di=01;34:fi=0:ln=01;36:mh=30;46:or=00;36:\
pi=00;35:so=01;35:bd=01;33:cd=1:su=37;41:sg=30;43:st=37;44:\
tw=30;42:ow=34;42:ex=01;32:no=31;47"

#define DEF_FILE_COLORS "di=01;34:nd=01;31:ed=00;34:ne=00;31:fi=0:\
ef=00;33:nf=00;31:ln=01;36:mh=30;46:or=00;36:pi=00;35:\
so=01;35:bd=01;33:cd=1:su=37;41:sg=30;43:ca=30;41:tw=30;42:\
ow=34;42:st=37;44:ex=01;32:ee=00;32:no=00;31;47:uf=34;47:"

#define DEF_IFACE_COLORS "el=01;33:mi=01;36:dl=01;34:tx=0:df=0:\
dc=0:wc=01;36:dh=00;36:li=01;32:si=01;34:ti=01;33:em=01;31:wm=01;33:\
nm=01;32:bm=01;36:sh=02;35:sf=04;36;sc=02;31:sx=02;32:"

#define DEF_EXT_COLORS "*.tar=01;31:*.tgz=01;31:*.arc=01;31:\
*.arj=01;31:*.taz=01;31:*.lha=01;31:*.lz4=01;31:*.lzh=01;31:\
*.lzma=01;31:*.tlz=01;31:*.txz=01;31:*.tzo=01;31:*.t7z=01;31:\
*.zip=01;31:*.z=01;31:*.dz=01;31:*.gz=01;31:*.lrz=01;31:*.lz=01;31:\
*.lzo=01;31:*.xz=01;31:*.zst=01;31:*.tzst=01;31:*.bz2=01;31:\
*.bz=01;31:*.tbz=01;31:*.tbz2=01;31:*.tz=01;31:*.deb=01;31:\
*.rpm=01;31:*.jar=01;31:*.war=01;31:*.ear=01;31:*.sar=01;31:\
*.rar=01;31:*.alz=01;31:*.ace=01;31:*.zoo=01;31:*.cpio=01;31:\
*.7z=01;31:*.rz=01;31:*.cab=01;31:*.wim=01;31:*.swm=01;31:\
*.dwm=01;31:*.esd=01;31:*.jpg=01;35:*.jpeg=01;35:*.mjpg=01;35:\
*.mjpeg=01;35:*.gif=01;35:*.bmp=01;35:*.pbm=01;35:*.pgm=01;35:\
*.ppm=01;35:*.tga=01;35:*.xbm=01;35:*.xpm=01;35:*.tif=01;35:\
*.tiff=01;35:*.png=01;35:*.svg=01;35:*.svgz=01;35:*.mng=01;35:\
*.pcx=01;35:*.mov=01;35:*.mpg=01;35:*.mpeg=01;35:*.m2v=01;35:\
*.mkv=01;35:*.webm=01;35:*.webp=01;35:*.ogm=01;35:*.mp4=01;35:\
*.m4v=01;35:*.mp4v=01;35:*.vob=01;35:*.qt=01;35:*.nuv=01;35:\
*.wmv=01;35:*.asf=01;35:*.rm=01;35:*.rmvb=01;35:*.flc=01;35:\
*.avi=01;35:*.fli=01;35:*.flv=01;35:*.gl=01;35:*.dl=01;35:\
*.xcf=01;35:*.xwd=01;35:*.yuv=01;35:*.cgm=01;35:*.emf=01;35:\
*.ogv=01;35:*.ogx=01;35:*.aac=00;36:*.au=00;36:*.flac=00;36:\
*.m4a=00;36:*.mid=00;36:*.midi=00;36:*.mka=00;36:*.mp3=00;36:\
*.mpc=00;36:*.ogg=00;36:*.ra=00;36:*.wav=00;36:*.oga=00;36:\
*.opus=00;36:*.spx=00;36:*.xspf=00;36:"

#define DEF_DI_C "\x1b[01;34m"
#define DEF_ND_C "\x1b[01;31m"
#define DEF_ED_C "\x1b[00;34m"
#define DEF_NE_C "\x1b[00;31m"
#define DEF_FI_C "\x1b[0m"
#define DEF_EF_C "\x1b[00;33m"
#define DEF_NF_C "\x1b[00;31m"
#define DEF_LN_C "\x1b[01;36m"
#define DEF_OR_C "\x1b[00;36m"
#define DEF_PI_C "\x1b[00;35m"
#define DEF_SO_C "\x1b[01;35m"
#define DEF_BD_C "\x1b[01;33m"
#define DEF_CD_C "\x1b[1m"
#define DEF_SU_C "\x1b[37;41m"
#define DEF_SG_C "\x1b[30;43m"
#define DEF_ST_C "\x1b[37;44m"
#define DEF_TW_C "\x1b[30;42m"
#define DEF_OW_C "\x1b[34;42m"
#define DEF_EX_C "\x1b[01;32m"
#define DEF_EE_C "\x1b[00;32m"
#define DEF_CA_C "\x1b[30;41m"
#define DEF_NO_C "\x1b[31;47m"
#define DEF_UF_C "\x1b[34;47m"
#define DEF_MH_C "\x1b[30;46m"
#define DEF_BM_C "\x1b[01;36m"

#define DEF_EL_C "\x1b[01;33m"
#define DEF_MI_C "\x1b[01;36m"
#define DEF_DL_C "\x1b[01;34m"
#define DEF_DC_C "\x1b[0m"
#define DEF_WC_C "\x1b[01;36m"
#define DEF_DH_C "\x1b[00;36m"

#define DEF_DF_C "\x1b[0m"

#define DEF_TX_C "\001\x1b[0m\002"
#define DEF_LI_C "\001\x1b[01;32m\002"
#define DEF_TI_C "\001\x1b[01;33m\002"
#define DEF_EM_C "\001\x1b[01;31m\002"
#define DEF_WM_C "\001\x1b[01;33m\002"
#define DEF_NM_C "\001\x1b[01;32m\002"
#define DEF_SI_C "\001\x1b[01;34m\002"

#define DEF_SH_C "\x1b[02;35m"
#define DEF_SF_C "\x1b[02;04;36m"
#define DEF_SC_C "\x1b[02;31m"
#define DEF_SX_C "\x1b[02;32m"

#define DEF_HB_C "\x1b[00;36m"
#define DEF_HC_C "\x1b[02;31m"
#define DEF_HD_C "\x1b[00;36m"
#define DEF_HE_C "\x1b[00;36m"
#define DEF_HN_C "\x1b[00;35m"
#define DEF_HP_C "\x1b[02;36m"
#define DEF_HQ_C "\x1b[00;33m"
#define DEF_HR_C "\x1b[00;31m"
#define DEF_HS_C "\x1b[00;32m"
#define DEF_HV_C "\x1b[00;32m"
#define DEF_HW_C "\x1b[00;31m"

#define DLFC "\x1b[0K" /* Delete line from cursor */
#define CNL "\x1b[1E" /* Move the cursor to beginning next line*/
#define DLFC_LEN 4 /* Length of the above escape codes */

#define DEF_DIR_ICO_C "\x1b[00;33m"

#define UNSET -1

/* Macros for the cp and mv cmds */
#define CP_CP 0
#define CP_ADVCP 1
#define CP_WCP 2
#define MV_MV 0
#define MV_ADVMV 1

/* Sort macros */
#define SNONE 0
#define SNAME 1
#define SSIZE 2
#define SATIME 3
#define SBTIME 4
#define SCTIME 5
#define SMTIME 6
#define SVER 7
#define SEXT 8
#define SINO 9
#define SOWN 10
#define SGRP 11
#define SORT_TYPES 11

/* Default options */
#define DEF_AUTOCD 1
#define DEF_AUTOJUMP 0
#define DEF_AUTO_OPEN 1
#define DEF_CASE_SENSITIVE 0
#define DEF_CASE_SENS_DIRJUMP 0
#define DEF_CASE_SENS_PATH_COMP 0
#define DEF_CD_LISTS_ON_THE_FLY 1
#define DEF_CD_ON_QUIT 0
#define DEF_CHECK_CAP 1
#define DEF_CHECK_EXT 1
#define DEF_CLASSIFY 1
#define DEF_CLEAR_SCREEN 1
#define DEF_COLOR_SCHEME "default"
#define DEF_COLORS 1
#define DEF_CONTROL_D_EXITS 0
#define DEF_CP_CMD CP_CP
#define DEF_CUR_WS 0
#define DEF_CWD_IN_TITLE 0
#define DEF_DIRHIST_MAP 0
#define DEF_DISK_USAGE 0
#define DEF_DIV_LINE_CHAR '-'
#define DEF_EXPAND_BOOKMARKS 0
#define DEF_EXT_CMD_OK 1
#define DEF_FILES_COUNTER 1
#define DEF_FOLLOW_SYMLINKS 1
#define DEF_HIGHLIGHT 0
#define DEF_ICONS 0
#define DEF_INT_VARS 0
#define DEF_LIGHT_MODE 0
#define DEF_LIST_FOLDERS_FIRST 1
#define DEF_LOGS_ENABLED 0
#define DEF_LONG_VIEW 0
#define DEF_MAX_DIRHIST 100
#define DEF_MAX_FILES UNSET
#define DEF_MAX_HIST 1000
#define DEF_MAX_JUMP_TOTAL_RANK 100000
#define DEF_MAX_LOG 500
#define DEF_MAX_PATH 40
#define DEF_MAXPRINTSEL 0
#define DEF_MIN_JUMP_RANK 10
#define DEF_MIN_NAME_TRIM 20
#define DEF_MV_CMD MV_MV
#define DEF_NOELN 0
#define DEF_ONLY_DIRS 0
#define DEF_PAGER 0
#define DEF_PRINTSEL 0
#define DEF_RESTORE_LAST_PATH 1
#define DEF_RL_EDIT_MODE 1
#define DEF_SHARE_SELBOX 0
#define DEF_SHOW_HIDDEN 0
#define DEF_SHOW_BACKUP_FILES 1
#define DEF_SORT SNAME
#define DEF_SORT_REVERSE 0
#define DEF_SPLASH_SCREEN 0
#ifdef __OpenBSD__
#define DEF_SUDO_CMD "doas"
#else
#define DEF_SUDO_CMD "sudo"
#endif
#define DEF_SUG_FILETYPE_COLOR 0
#define DEF_SUG_STRATEGY "ehfjbac"
#define DEF_SUGGESTIONS 1
#define DEF_TIPS 1
#define DEF_TRASRM 0
#define DEF_UNICODE 1
#define DEF_WELCOME_MESSAGE 1
#define SUG_STRATS 7

#define MAX_WS 8

#define DEFAULT_PROMPT "\\[\\e[0m\\][\\[\\e[0;36m\\]\\S\\[\\e[0m\\]]\\l \
\\A \\u:\\H \\[\\e[00;36m\\]\\w\\n\\[\\e[0m\\]<\\z\\[\\e[0m\\]>\\[\\e[0;34m\\] \
\\$\\[\\e[0m\\] "

#define DEFAULT_TERM_CMD "xterm -e"

/* Macros for the colors_list function */
#define NO_ELN 0
#define NO_NEWLINE 0
#define NO_PAD 0
#define PRINT_NEWLINE 1

/* A few key macros used by the auto-suggestions system */
#define _ESC 27
#define _TAB 9
#define BS 8
#define DELETE 127
#define ENTER 13
/* #define OP_BRACKET 91
#define UC_O 79
#define SPACE 32 */

/* Macros to specify suggestions type */
#define NO_SUG 0
#define HIST_SUG 1
#define FILE_SUG 2
#define CMD_SUG 3
#define INT_CMD 4
#define COMP_SUG 5
#define BOOKMARK_SUG 6
#define ALIAS_SUG 7
#define ELN_SUG 8
#define FIRST_WORD 9
#define JCMD_SUG 10
#define JCMD_SUG_NOACD 11 /* No auto-cd */
#define VAR_SUG 12

/* 46 == \x1b[00;38;02;000;000;000;00;48;02;000;000;000m\0 (24bit, RGB
 * true color format including foreground and background colors, the SGR
 * (Select Graphic Rendition) parameter, and, of course, the terminating
 * null byte. */
#define MAX_COLOR 46

/* Macros to control file descriptors in exec functions */
#define E_NOFLAG 0
#define E_NOSTDIN (1 << 1)
#define E_NOSTDOUT (1 << 2)
#define E_NOSTDERR (1 << 3)
#define E_MUTE (E_NOSTDOUT | E_NOSTDERR)

/* Macros for the clear_suggestion function */
#define CS_FREEBUF 1
#define CS_KEEPBUF 0

/* Macros for the xmagic function */
#define MIME_TYPE 1
#define TEXT_DESC 0

/* Max length of the properties string in long view mode */
#define MAX_PROP_STR 55

/* Macros for the prompt style */
#define DEF_PROMPT_STYLE 0
#define CUSTOM_PROMPT_STYLE 1

/* Macros for the dirjump function */
#define SUG_JUMP 0
#define NO_SUG_JUMP 1

/* Macros for the rl_highlight function */
#define SET_COLOR 1
#define INFORM_COLOR 0

#define FALLBACK_SHELL "/bin/sh"

#ifdef __APPLE__
#define FALLBACK_OPENER "/usr/bin/open"
#elif defined __CYGWIN__
#define FALLBACK_OPENER "cygstart"
#elif __HAIKU__
#define FALLBACK_OPENER "open"
#else
#define FALLBACK_OPENER "xdg-open"
#endif

#define TMP_FILENAME ".clifmXXXXXX"

#ifndef P_tmpdir
#define P_tmpdir "/tmp"
#endif

#define itoa xitoa /* itoa does not exist in some OS's */

#ifndef _NO_GETTEXT
#define _(String) gettext(String)
#else
#define _(String) String
#endif /* _GETTEXT */

#define strlen(s) xstrnlen(s)

#define ENTRY_N 64

#define UNUSED(x) (void)x /* Just silence the compiler's warning */
#define TOUPPER(ch) (((ch) >= 'a' && (ch) <= 'z') ? ((ch) - 'a' + 'A') : (ch))
#define DIGINUM(n) (((n) < 10) ? 1 : ((n) < 100)      ? 2 \
				 : ((n) < 1000)	      ? 3 \
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
#define BASENAME_BONUS 300
#define BOOKMARK_BONUS 500
#define PINNED_BONUS 1000
#define WORKSPACE_BONUS 300
/* Last directory access */
#define JHOUR(n) ((n) *= 4) /* Within last hour */
#define JDAY(n) ((n) *= 2)  /* Within last day */
#define JWEEK(n) ((n) / 2)  /* Within last week */
#define JOLDER(n) ((n) / 4) /* More than a week */

#if defined(__HAIKU__)
# define DT_UNKNOWN	0
# define DT_FIFO 1
# define DT_CHR 2
# define DT_DIR 4
# define DT_BLK 6
# define DT_REG 8
# define DT_LNK 10
# define DT_SOCK 12
#endif

#define DT_NONE 14

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
	int padding;
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

/* Struct to specify which parameters have been set from the command
 * line, to avoid overriding them with init_config(). While no command
 * line parameter will be overriden, the user still can modifiy on the
 * fly (editing the config file) any option not specified in the command
 * line */
struct param {
	int auto_open;
	int autocd;
	int autojump;
	int bm_file;
	int case_sens_dirjump;
	int case_sens_path_comp;
	int cd_list_auto;
	int clear_screen;
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
	int hidden;
#ifndef _NO_HIGHLIGHT
	int highlight;
#endif
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
	int no_colors;
	int no_columns;
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
	NOMSG = 0,
	ERROR = 1,
	WARNING = 2,
	NOTICE = 4
};

/* pmsg holds the current program message type */
extern enum prog_msg pmsg;

/* Enumeration for the dirjump function options */
enum jump {
	NONE = 0,
	JPARENT = 1,
	JCHILD = 2,
	JORDER = 4,
	JLIST = 8
};

enum comp_type {
	TCMP_BOOKMARK = 0,
	TCMP_CMD = 1,
	TCMP_CSCHEME = 2,
	TCMP_ELN = 3,
	TCMP_JUMP = 4,
	TCMP_NET = 5,
	TCMP_NONE = 6,
	TCMP_PATH = 7,
	TCMP_PROF = 8,
	TCMP_SORT = 9
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
    autojump,
    bg_proc,
    case_sensitive,
    case_sens_dirjump,
    case_sens_path_comp,
    cd_lists_on_the_fly,
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
    dirhist_map,
    disk_usage,
    expand_bookmarks,
    ext_cmd_ok,
    files_counter,
    filter_rev,
	follow_symlinks,
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
    logs_enabled,
    long_view,
    mime_match,
    min_name_trim,
    mv_cmd,
    no_eln,
    no_log,
    only_dirs,
    pager,
    print_msg,
    print_selfiles,
    prompt_style,
    recur_perm_error_flag,
    restore_last_path,
    sel_is_last,
    selfile_ok,
    share_selbox,
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
    welcome_message;

#ifndef _NO_HIGHLIGHT
extern int wrong_cmd;
extern int wrong_cmd_line;
#endif

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
    usrvar_n;

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
    *filter,
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
    *tmp_dir,
#ifndef _NO_TRASH
    *trash_dir,
    *trash_files_dir,
    *trash_info_dir,
#endif
    *usr_cscheme,
    *user_home,

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
    bd_c[MAX_COLOR],	     /* Block device */
    ca_c[MAX_COLOR],	     /* Cap file */
    cd_c[MAX_COLOR],	     /* Char device */
    ee_c[MAX_COLOR],	     /* Empty executable */
    ex_c[MAX_COLOR],	     /* Executable */
    ef_c[MAX_COLOR],	     /* Empty reg file */
    ed_c[MAX_COLOR],	     /* Empty dir */
    fi_c[MAX_COLOR],	     /* Reg file */
    di_c[MAX_COLOR],	     /* Directory */
    ln_c[MAX_COLOR],	     /* Symlink */
    mh_c[MAX_COLOR],	     /* Multi-hardlink file */
    nd_c[MAX_COLOR],	     /* No read directory */
    ne_c[MAX_COLOR],	     /* No read empty dir */
    nf_c[MAX_COLOR],	     /* No read file */
    no_c[MAX_COLOR],	     /* Unknown */
    or_c[MAX_COLOR],	     /* Broken symlink */
    ow_c[MAX_COLOR],	     /* Other writable */
    pi_c[MAX_COLOR],	     /* FIFO, pipe */
    sg_c[MAX_COLOR],	     /* SGID file */
    so_c[MAX_COLOR],	     /* Socket */
    st_c[MAX_COLOR],	     /* Sticky (not ow)*/
    su_c[MAX_COLOR],	     /* SUID file */
    tw_c[MAX_COLOR],	     /* Sticky other writable */
    uf_c[MAX_COLOR],	     /* Non-'stat'able file */

	/* Interface */
    bm_c[MAX_COLOR],	     /* Bookmarked directory */
    dc_c[MAX_COLOR],	     /* Files counter color */
    df_c[MAX_COLOR],	     /* Default color */
    dh_c[MAX_COLOR],	     /* Dirhist index color */
    dl_c[MAX_COLOR],	     /* Dividing line color */
    el_c[MAX_COLOR],	     /* ELN color */
    mi_c[MAX_COLOR],	     /* Misc indicators color */

	/* Suggestions */
	sc_c[MAX_COLOR],	     /* Auto-suggestions: external commands */
	sf_c[MAX_COLOR],	     /* Auto-suggestions: filenames */
	sh_c[MAX_COLOR],	     /* Auto-suggestions: history */
	sx_c[MAX_COLOR],	     /* Auto-suggestions: internal commands and params */
    wc_c[MAX_COLOR],	     /* Welcome message color */

#ifndef _NO_ICONS
    dir_ico_c[MAX_COLOR], /* Directories icon color */
#endif

	/* Syntax highlighting */
	hb_c[MAX_COLOR],		/* Brackets () [] {} */
	hc_c[MAX_COLOR],		/* Comments */
	hd_c[MAX_COLOR],		/* Paths (slashes) */
	he_c[MAX_COLOR],		/* Expansion operators: * ~ */
	hn_c[MAX_COLOR],		/* Numbers */
	hp_c[MAX_COLOR],		/* Parameters: - */
	hq_c[MAX_COLOR],		/* Quoted strings */
	hr_c[MAX_COLOR],		/* Redirection > */
	hs_c[MAX_COLOR],		/* Process separators | & ; */
	hv_c[MAX_COLOR],		/* Variables $ */
	hw_c[MAX_COLOR],		/* Wrong, non-existent command name $ */

    /* Colors used in the prompt, so that \001 and \002 needs to
	 * be added. This is why MAX_COLOR + 2 */
    em_c[MAX_COLOR + 2], /* Error msg color */
    li_c[MAX_COLOR + 2], /* Sel indicator color */
    nm_c[MAX_COLOR + 2], /* Notice msg color */
    ti_c[MAX_COLOR + 2], /* Trash indicator color */
    tx_c[MAX_COLOR + 2], /* Text color */
    si_c[MAX_COLOR + 2], /* stealth indicator color */
    wm_c[MAX_COLOR + 2]; /* Warning msg color */
