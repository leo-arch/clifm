/* settings.h - User settings default values */

/*
 * This file is part of CliFM
 *
 * Copyright (C) 2016-2022, L. Abramovich <johndoe.arch@outlook.com>
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

#ifndef SETTINGS_H
#define SETTINGS_H

#define PROGRAM_NAME "CliFM"
#define PNL "clifm" /* Program name lowercase */
#define PROGRAM_DESC "The command line file manager"
#define VERSION "1.4.1"
#define AUTHOR "L. Abramovich"
#define CONTACT "johndoe.arch@outlook.com"
#define WEBSITE "https://github.com/leo-arch/clifm"
#define DATE "Jan 29, 2022"
#define LICENSE "GPL2+"
#define COLORS_REPO "https://github.com/leo-arch/clifm-colors"

/* User settings */

/* Default color definitions */
#define DEF_FILE_COLORS "di=01;34:nd=01;31:ed=02;34:ne=02;31:fi=0:\
ef=02;33:nf=02;31:ln=01;36:mh=30;46:or=02;36:pi=00;35:so=01;35:\
bd=01;33:cd=01:su=37;41:sg=30;43:ca=30;41:tw=30;42:ow=34;42:st=37;44:\
ex=01;32:ee=00;32:no=00;31;47:uf=34;47:"

#define DEF_IFACE_COLORS "el=01;33:mi=01;36:dl=00;34:tx=0:df=0:\
dc=00;02;34:wc=01;36:dh=00;36:li=01;32:si=01;34:ti=01;36:em=01;31:\
wm=01;33:nm=01;32:bm=01;36:sb=02;33:sh=02;35:sf=04;02;36:sc=02;31:\
sx=02;32:sp=02;31:hb=00;36:hc=02;31:hd=00;36:he=00;36:hn=00;35:\
hp=00;36:hq=00;33:hr=00;31:hs=00;32:hv=00;32:tt=02;01;36:ts=04;35:\
ws1=00;34:ws2=0;31:ws3=00;33:ws4=00;32:ws5=00;36:ws6=00;36:ws7=00;36\
:ws8=00;36:"

#define DEF_EXT_COLORS "*.tar=01;31:*.tgz=01;31:*.taz=01;31:*.lha=01;31:\
*.lz4=01;31:*.lzh=01;31:*.lzma=01;31:*.tlz=01;31:*.txz=01;31:*.tzo=01;31:\
*.t7z=01;31:*.zip=01;31:*.z=01;31:*.dz=01;31:*.gz=01;31:*.lrz=01;31:\
*.lz=01;31:*.lzo=01;31:*.xz=01;31:*.zst=01;31:*.tzst=01;31:*.bz2=01;31:\
*.bz=01;31:*.tbz=01;31:*.tbz2=01;31:*.tz=01;31:*.deb=01;31:*.rpm=01;31:\
*.rar=01;31:*.cpio=01;31:*.7z=01;31:*.rz=01;31:*.cab=01;31:*.jpg=01;35:\
*.JPG=01;35:*.jpeg=01;35:*.mjpg=01;35:*.mjpeg=01;35:*.gif=01;35:\
*.GIF=01;35:*.bmp=01;35:*.xbm=01;35:*.xpm=01;35:*.png=01;35:*.PNG=01;35:\
*.svg=01;35:*.pcx=01;35:*.mov=01;35:*.mpg=01;35:*.mpeg=01;35:*.m2v=01;35:\
*.mkv=01;35:*.webm=01;35:*.webp=01;35:*.ogm=01;35:*.mp4=01;35:*.MP4=01;35:\
*.m4v=01;35:*.mp4v=01;35:*.vob=01;35:*.wmv=01;35:*.flc=01;35:*.avi=01;35:\
*.flv=01;35:*.m4a=00;36:*.mid=00;36:*.midi=00;36:*.mp3=00;36:*.MP3=00;36:\
*.ogg=00;36:*.wav=00;36:*.pdf=01;31:*.PDF=01;31:*.doc=35:*.docx=35:\
*.xls=35:*.xlsx=35:*.ppt=35:*.pptx=35:*.odt=35:*.ods=35:*.odp=35:\
*.cache=02;37:*.tmp=02;37:*.temp=02;37:*.log=02;37:*.bak=02;37:*.bk=02;37:\
*.in=02;37:*.out=02;37:*.part=02;37:*.aux=02;37:*.c=00;01:*.c++=00;01:\
*.h=00;01:*.cc=00;01:*.cpp=00;01:*.h=00;01:*.h++=00;01:*.hh=00;01:\
*.go=00;01:*.java=00;01:*.js=00;01:*.lua=00;01:*.rb=00;01:*.rs=00;01:"

/* Filetypes */
#define DEF_DI_C "\x1b[01;34m" /* Dir */
#define DEF_ND_C "\x1b[01;31m" /* Unaccessible dir */
#define DEF_ED_C "\x1b[02;34m" /* Empty dir */
#define DEF_NE_C "\x1b[02;31m" /* Empty dir no read permission */
#define DEF_FI_C "\x1b[0m"     /* Regular file */
#define DEF_EF_C "\x1b[02;33m" /* Empty regular file */
#define DEF_NF_C "\x1b[02;31m" /* Unaccessible file */
#define DEF_LN_C "\x1b[01;36m" /* Symbolic link */
#define DEF_MH_C "\x1b[30;46m" /* Multi-link */
#define DEF_OR_C "\x1b[02;36m" /* Orphaned/broken symlink */
#define DEF_PI_C "\x1b[00;35m" /* FIFO/pipe */
#define DEF_SO_C "\x1b[01;35m" /* Socket */
#define DEF_BD_C "\x1b[01;33m" /* Block device */
#define DEF_CD_C "\x1b[1m"     /* Character device */
#define DEF_SU_C "\x1b[37;41m" /* SUID file */
#define DEF_SG_C "\x1b[30;43m" /* SGID file */
#define DEF_CA_C "\x1b[30;41m" /* File with capabilities */
#define DEF_TW_C "\x1b[30;42m" /* Sticky and other-writable */
#define DEF_OW_C "\x1b[34;42m" /* Other-writable */
#define DEF_ST_C "\x1b[37;44m" /* Sticky bit set */
#define DEF_EX_C "\x1b[01;32m" /* Executable file */
#define DEF_EE_C "\x1b[00;32m" /* Empty executable file */
#define DEF_NO_C "\x1b[00;31;47m" /* Unknown file type */
#define DEF_UF_C "\x1b[34;47m" /* Un'stat'able file */

/* Interface */
#define DEF_BM_C "\x1b[01;36m" /* Bookmarked dirs in bookmarks screen */
#define DEF_DC_C "\x1b[00;02;34m"
#define DEF_DF_C "\x1b[0m" /* Default color */
#define DEF_DH_C "\x1b[00;36m" /* Directory history indicator */
#define DEF_DL_C "\x1b[00;34m" /* Block device */
#define DEF_EL_C "\x1b[01;33m" /* ELN's */
#define DEF_EM_C "\001\x1b[01;31m\002" /* Error msg indicator */
#define DEF_LI_C "\001\x1b[01;32m\002" /* Sel files indicator (prompt) */
#define DEF_LI_CB "\x1b[01;32m" /* Sel files indicator (files list) */
#define DEF_MI_C "\x1b[01;36m" /* Misc */
#define DEF_NM_C "\001\x1b[01;32m\002" /* Notice msg indicator */
#define DEF_SI_C "\001\x1b[01;34m\002" /* Stealth mode indicator */
#define DEF_TI_C "\001\x1b[01;36m\002" /* Trash indicator */
#define DEF_TX_C "\x1b[0m" /* Input text */
#define DEF_WC_C "\x1b[01;36m" /* Welcome message */
#define DEF_WM_C "\001\x1b[01;33m\002" /* Warning msg indicator */

#define DEF_TS_C "\x1b[04;35m" /* Matching prefix for TAB completed possible entries */
#define DEF_TT_C "\x1b[01;02;36m" /* Tilde for trimmed file names */
#define DEF_WP_C "\x1b[02;31m" /* Warning prompt input text */

/* Workspaces */
#define DEF_WS1_C "\001\x1b[0;34m\002"
#define DEF_WS2_C "\001\x1b[0;31m\002"
#define DEF_WS3_C "\001\x1b[0;33m\002"
#define DEF_WS4_C "\001\x1b[0;32m\002"
#define DEF_WS5_C "\001\x1b[0;36m\002"
#define DEF_WS6_C "\001\x1b[0;36m\002"
#define DEF_WS7_C "\001\x1b[0;36m\002"
#define DEF_WS8_C "\001\x1b[0;36m\002"

/* Suggestions */
#define DEF_SB_C "\x1b[02;33m" /* Shell built-ins */
#define DEF_SH_C "\x1b[02;35m" /* Commands history */
#define DEF_SF_C "\x1b[02;04;36m"  /* ELN's, bookmark, file, and directory names */
#define DEF_SC_C "\x1b[02;31m" /* Aliases and binaries in PATH */
#define DEF_SX_C "\x1b[02;32m" /* CliFM internal commands and parameters */
#define DEF_SP_C "\x1b[02;31m" /* Suggestions pointer (12 > filename) */

/* Highlight */
#define DEF_HB_C "\x1b[00;36m" /* Parenthesis, Brackets ( {[()]} ) */
#define DEF_HC_C "\x1b[02;31m" /* Comments (#comment) */
#define DEF_HD_C "\x1b[00;36m" /* Slashes (for paths) */
#define DEF_HE_C "\x1b[00;36m" /* Expansion operators (* ~) */
#define DEF_HN_C "\x1b[00;35m" /* Numbers (including ELN's) */
#define DEF_HP_C "\x1b[00;36m" /* Parameters (e.g. -h --help) */
#define DEF_HQ_C "\x1b[00;33m" /* Quotes (single and double) */
#define DEF_HR_C "\x1b[00;31m" /* Redirection operator (>) */
#define DEF_HS_C "\x1b[00;32m" /* Commands separator (; & |)*/
#define DEF_HV_C "\x1b[00;32m" /* Variables ($VAR) */
/* This one is purely internal: cur_color is set to this value when
 * entering the warning prompt, so that that we can check cur_color
 * anytime to know whether we are in the warning prompt or not */
#define DEF_HW_C "\x1b[00;31m"

#define DEF_DIR_ICO_C "\x1b[00;33m"

/* Colors for file properties */
#define PR_READ     "\x1b[0;33m"
#define PR_WRITE    "\x1b[0;31m"
#define PR_EXEC_DIR "\x1b[0;32m"
#define PR_EXEC_REG "\x1b[0;36m"
#define PR_NONE     "\x1b[0;2;37m"
#define PR_SPECIAL  "\x1b[0;0;35m" /* SUID, GUID, ISVTX */
#define PR_DATE     "\x1b[0;34m"
#define PR_ID       "\x1b[0;33m" /* UID, GID */
#define PR_SIZE     "\x1b[0;32m"
#define PR_SIZE_DIR "\x1b[0;32m"
#define PR_SIZE_REG "\x1b[0m"
#define PR_NUM_VAL  "\x1b[0;36m" /* Properties in octal */

/* Colors for the file type bit of the properties string */
#define PR_DIR  "\x1b[0;34m"
#define PR_REG  "\x1b[0m"
#define PR_LNK  "\x1b[0;36m"
#define PR_CHR  "\x1b[0;32m"
#define PR_BLK  "\x1b[0;32m"
#define PR_FIFO "\x1b[0;35m"
#define PR_SOCK "\x1b[0;35m"

#define MNT_UDEVIL 	0
#define MNT_UDISKS2 1

/* Default options */
#define DEF_APPARENT_SIZE 0
#define DEF_AUTOCD 1
#define DEF_AUTOJUMP 0
#define DEF_AUTO_OPEN 1
#define DEF_AUTOLS 1
#define DEF_CASE_SENS_LIST 0
#define DEF_CASE_SENS_DIRJUMP 0
#define DEF_CASE_SENS_PATH_COMP 0
#define DEF_CASE_SENS_SEARCH 0
#define DEF_CD_ON_QUIT 0
#define DEF_CHECK_CAP 1 /* Check files capabilities */
#define DEF_CHECK_EXT 1 /* Check file names extension (for color) */
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
#define DEF_ELNPAD LEFTSPACEPAD
#define DEF_EXPAND_BOOKMARKS 0
#define DEF_EXT_CMD_OK 1
#define DEF_FILES_COUNTER 1
#define DEF_FOLLOW_SYMLINKS 1
#define DEF_FULL_DIR_SIZE 0
#define DEF_FZFTAB 0
#define DEF_FZF_WIN_HEIGHT 40 /* Max win percentage taken by FZF */
#define DEF_HIGHLIGHT 1
#define DEF_ICONS 0
#define DEF_INT_VARS 0
#define DEF_LIGHT_MODE 0
#define DEF_LIST_FOLDERS_FIRST 1
#define DEF_LISTING_MODE VERTLIST
#define DEF_LOGS_ENABLED 0
#define DEF_LONG_VIEW 0
#define DEF_MAX_DIRHIST 100
#define DEF_MAX_FILES UNSET
#define DEF_MAX_NAME_LEN UNSET
#define DEF_MAX_HIST 1000
#define DEF_MAX_JUMP_TOTAL_RANK 100000
#define DEF_MAX_LOG 500
#define DEF_MAX_PATH 40
#define DEF_MAXPRINTSEL 0
#define DEF_MIN_JUMP_RANK 10
#define DEF_MIN_NAME_TRIM 20
#define DEF_MOUNT_CMD MNT_UDEVIL
#define DEF_MV_CMD MV_MV
#define DEF_NOELN 0
#define DEF_ONLY_DIRS 0
#define DEF_PAGER 0
#define DEF_PRINTSEL 0
#define DEF_PROPS_COLOR 1
#define DEF_RESTORE_LAST_PATH 1
#define DEF_RL_EDIT_MODE 1
#define DEF_SECURE_CMDS 0
#define DEF_SECURE_ENV 0
#define DEF_SECURE_ENV_FULL 0
#define DEF_SHARE_SELBOX 0
#define DEF_SHOW_HIDDEN 0
#define DEF_SHOW_BACKUP_FILES 1
#define DEF_SORT SNAME
#define DEF_SORT_REVERSE 0
#define DEF_SPLASH_SCREEN 0
#ifdef __OpenBSD__
# define DEF_SUDO_CMD "doas"
#else
# define DEF_SUDO_CMD "sudo"
#endif
#define DEF_SUG_FILETYPE_COLOR 0
#define DEF_SUG_STRATEGY "ehfjbac"
#define DEF_SUGGESTIONS 1
#define DEF_TIPS 1
#define DEF_TRASRM 0
#define DEF_UNICODE 1
#define DEF_WELCOME_MESSAGE 1
#define DEF_WARNING_PROMPT 1
#define SUG_STRATS 7

/* These options should work with FZF 0.17.5 or later */
#define DEF_FZFTAB_OPTIONS "--color=16,prompt:6,fg+:-1,pointer:4,\
hl:5,hl+:5,gutter:-1,marker:2 --bind tab:accept,right:accept,\
left:abort --inline-info --layout=reverse-list"

#define MAX_WS 8

#define DEFAULT_PROMPT "\\[\\e[0m\\][\\S\\[\\e[0m\\]]\\l \
\\A \\u:\\H \\[\\e[00;36m\\]\\w\\n\\[\\e[0m\\]<\\z\\[\\e[0m\\]\
>\\[\\e[0;34m\\] \\$\\[\\e[0m\\] "
#define DEFAULT_PROMPT_NO_COLOR "[\\S]\\l \\A \\u:\\H \\w\\n<\\z> \\$ "

#define DEF_WPROMPT_STR "\\[\\e[0m\\]\\[\\e[00;02;31m\\](!) > "
#define DEF_WPROMPT_STR_NO_COLOR "(!) > "

#define DEFAULT_TERM_CMD "xterm -e"
#define FALLBACK_SHELL "/bin/sh"

#ifdef __APPLE__
# define FALLBACK_OPENER "/usr/bin/open"
#elif defined __CYGWIN__
# define FALLBACK_OPENER "cygstart"
#elif __HAIKU__
# define FALLBACK_OPENER "open"
#else
# define FALLBACK_OPENER "xdg-open"
#endif

#endif /* SETTINGS_H */
