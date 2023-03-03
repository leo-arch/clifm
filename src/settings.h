/* settings.h - User settings default values */

/*
 * This file is part of CliFM
 *
 * Copyright (C) 2016-2023, L. Abramovich <johndoe.arch@outlook.com>
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

/* User settings */

/* Default color definitions */
#define DEF_FILE_COLORS "bd=01;33:ca=30;41:cd=01:di=01;34:ed=02;34:\
ee=32:ef=02:ex=01;32:fi=0:ln=01;36:mh=30;46:nd=04;01;31:ne=02;04;31:nf=02;04;31:\
no=31;47:or=02;04;36:ow=34;42:pi=35:so=01;35:su=37;41:sg=30;43:st=37;44:\
tw=30;42:uf=02;04;37:"

#define DEF_IFACE_COLORS "bm=01;36:dd=:df=0:dg=:dl=02;37:\
dn=00;02;37:do=:dp=:dr=:dw=:dxd=:dxr=:dz=:\
el=00;36:em=01;31:fc=00;02;37:hb=00;36:hc=02;31:\
hd=00;36:he=00;36:hn=0:hp=00;36:hq=00;33:hr=00;31:hs=00;32:hv=00;32:\
li=01;32:mi=01;36:nm=01;32:sb=00;02;33:sc=02;36:sd=00;02;37:\
sf=04;02;36:sh=02;35:si=01;34:sp=02;31:sx=02;32:sz=00;02;04;35:\
ti=01;36:ts=04;35:tt=02;01;36:tx=0:\
wc=01;36:wm=01;33:wp=00;02;31:ws1=34:ws2=31:ws3=33:ws4=32:ws5=36:ws6=36:ws7=36:ws8=36:\
xf=01;31:xs=00;32:"

/* Definition order: archive, image, video, sound, document, code, uninmportant */
#define DEF_EXT_COLORS "*.tar=01;31:*.tgz=01;31:*.arc=01;31:*.arj=01;31:*.taz=01;31:*.lha=01;31:\
*.lz4=01;31:*.lzh=01;31:*.lzma=01;31:*.tlz=01;31:*.txz=01;31:*.tzo=01;31:\
*.t7z=01;31:*.zip=01;31:*.z=01;31:*.dz=01;31:*.gz=01;31:*.lrz=01;31:*.lz=01;31:\
*.lzo=01;31:*.xz=01;31:*.zst=01;31:*.tzst=01;31:*.bz2=01;31:*.bz=01;31:\
*.tbz=01;31:*.tbz2=01;31:*.tz=01;31:*.deb=01;31:*.rpm=01;31:*.jar=01;31:\
*.war=01;31:*.ear=01;31:*.sar=01;31:*.rar=01;31:*.alz=01;31:*.ace=01;31:\
*.zoo=01;31:*.cpio=01;31:*.7z=01;31:*.rz=01;31:*.cab=01;31:*.wim=01;31:\
*.swm=01;31:*.dwm=01;31:*.esd=01;31:*.apk=01;31:\
*.avif=35:*.jpg=35:*.jpeg=35:*.jxl=35:*.mjpg=35:*.mjpeg=35:*.gif=35:*.bmp=35:\
*.xbm=35:*.xpm=35:*.png=35:*.svg=35:*.svgz=35:*.pcx=35:*.pbm=35:*.pgm=35:\
*.ppm=35:*.tga=35:*.tif=35:*.tiff=35:*.mng=35:\
*.mov=01;35:*.mpg=01;35:*.mpeg=01;35:*.m2v=01;35:*.mkv=01;35:*.webm=01;35:\
*.webp=01;35:*.ogm=01;35:*.mp4=01;35:*.m4v=01;35:*.mp4v=01;35:*.vob=01;35:\
*.qt=01;35:*.nuv=01;35:*.wmv=01;35:*.asf=01;35:*.rm=01;35:*.rmvb=01;35:\
*.flc=01;35:*.avi=01;35:*.fli=01;35:*.flv=01;35:*.gl=01;35:*.dl=01;35:\
*.xcf=01;35:*.xwd=01;35:*.yuv=01;35:*.cgm=01;35:*.emf=01;35:*.ogv=01;35:\
*.ogx=01;35:*.mov=01;35:*.mpg=01;35:*.mpeg=01;35:*.m2v=01;35:*.mkv=01;35:\
*.webm=01;35:*.webp=01;35:*.ogm=01;35:*.ogv=01;35:*.ogx=01;35:*.mp4=01;35:\
*.m4v=01;35:*.mp4v=01;35:*.vob=01;35:*.qt=01;35:*.nuv=01;35:*.wmv=01;35:\
*.flc=01;35:*.avi=01;35:*.fli=01;35:*.flv=01;35:*.asf=01;35:*.rm=01;35:\
*.rmvb=01;35:*.gl=01;35:*.dl=01;35:*.xcf=01;35:*.xwd=01;35:*.yuv=01;35:\
*.cgm=01;35:*.emf=01;35:\
*.aac=36:*.au=36:*.m4a=36:*.mid=36:*.midi=36:*.mp3=36:*.mka=36:*.ogg=36:\
*.opus=36:*.spx=36:*.wma=36:*.wv=36:*.wav=36:*.flac=36:*.aif=36:\
*.pdf=01:*.djvu=01:*.epub=01:*.mobi=01:*.cbr=01:*.cbz=01:*.ps=01:*.sxw=01:\
*.doc=01:*.docx=01:*.xls=01:*.xlsx=01:*.xlr=01:*.sxi=01:*.ppt=01:*.pptx=01:\
*.odt=01:*.ods=01:*.odp=01:*.rtf=01:\
*.c=01;33:*.c++=01;33:*.h=01;33:*.cc=01;33:*.cpp=01;33:*.h=01;33:*.h++=01;33:\
*.hh=01;33:*.go=01;33:*.java=01;33:*.js=01;33:*.lua=01;33:*.rb=01;33:*.rs=01;33:\
*.kt=01;33:*.kts=01;33:*.hs=01;33:*.pl=01;33:*.vb=01;33:*.html=01;33:*.htm=01;33:\
*.shtml=01;33:xhtml=01;33:*.xml=01;33:*.php=01;33:*.tex=01;33:*.ltx=01;33:\
*.md=01;33:\
*.cache=02;37:*.tmp=02;37:*.temp=02;37:*.log=02;37:*.bak=02;37:*.bk=02;37:\
*.in=02;37:*.out=02;37:*.part=02;37:*.aux=02;37:*.old=02;37:*.orig=02;37:\
*.rej=02;37:*.swp=02;37:*.pid=02;37:"

/* Filetypes */
#define DEF_BD_C  "\x1b[01;33m" /* Block device */
#define DEF_CA_C  "\x1b[30;41m" /* File with capabilities */
#define DEF_CD_C  "\x1b[00;01m" /* Character device */
#define DEF_DI_C  "\x1b[01;34m" /* Dir */
#define DEF_ED_C  "\x1b[02;34m" /* Empty dir */
#define DEF_EE_C  "\x1b[00;32m" /* Empty executable file */
#define DEF_EF_C  "\x1b[00;02m" /* Empty regular file */
#define DEF_EX_C  "\x1b[01;32m" /* Executable file */
#define DEF_FI_C  "\x1b[0m"     /* Regular file */
#define DEF_LN_C  "\x1b[01;36m" /* Symbolic link */
#define DEF_MH_C  "\x1b[30;46m" /* Multi-link */
#define DEF_ND_C  "\x1b[01;31m" /* Unaccessible dir */
#define DEF_NE_C  "\x1b[02;31m" /* Empty dir no read permission */
#define DEF_NF_C  "\x1b[02;31m" /* Unaccessible file */
#define DEF_NO_C  "\x1b[00;31;47m" /* Unknown file type */
#define DEF_OR_C  "\x1b[02;36m" /* Orphaned/broken symlink */
#define DEF_OW_C  "\x1b[34;42m" /* Other-writable */
#define DEF_PI_C  "\x1b[00;35m" /* FIFO/pipe */
#define DEF_SG_C  "\x1b[30;43m" /* SGID file */
#define DEF_SO_C  "\x1b[01;35m" /* Socket */
#define DEF_ST_C  "\x1b[37;44m" /* Sticky bit set */
#define DEF_SU_C  "\x1b[37;41m" /* SUID file */
#define DEF_TW_C  "\x1b[30;42m" /* Sticky and other-writable */
#define DEF_UF_C  "\x1b[02;04;37m" /* Un'stat'able file */

/* Interface */
#define DEF_BM_C  "\x1b[01;36m" /* Bookmarked dirs in bookmarks screen */
#define DEF_DF_C  "\x1b[0m" /* Reset attributes: default terminal color */
#define DEF_DL_C  "\x1b[02;37m" /* Dividing line */
#define DEF_EL_C  "\x1b[00;36m" /* ELN's */
#define DEF_EM_C  "\001\x1b[01;31m\002" /* Error msg indicator */
#define DEF_FC_C  "\x1b[00;02;37m" /* Files counter */
#define DEF_LI_C  "\001\x1b[01;32m\002" /* Sel files indicator (prompt) */
#define DEF_LI_CB "\x1b[01;32m" /* Sel files indicator (files list) */
#define DEF_MI_C  "\x1b[01;36m" /* Misc */
#define DEF_NM_C  "\001\x1b[01;32m\002" /* Notice msg indicator */
#define DEF_SI_C  "\001\x1b[01;34m\002" /* Stealth mode indicator */
#define DEF_TI_C  "\001\x1b[01;36m\002" /* Trash indicator */
#define DEF_TX_C  "\x1b[0m" /* Input text */
#define DEF_WC_C  "\x1b[01;36m" /* Welcome message */
#define DEF_WM_C  "\001\x1b[01;33m\002" /* Warning msg indicator */

#define DEF_TS_C  "\x1b[04;35m" /* Matching prefix for TAB completed possible entries */
#define DEF_TT_C  "\x1b[01;02;36m" /* Tilde for trimmed file names */
#define DEF_WP_C  "\x1b[00;02;31m" /* Warning prompt input text */
#define DEF_XS_C  "\001\x1b[32m\002" /* Exit code: success */
#define DEF_XF_C  "\001\x1b[01;31m\002" /* Exit code: failure */

/* Workspaces */
#define DEF_WS1_C "\001\x1b[34m\002"
#define DEF_WS2_C "\001\x1b[31m\002"
#define DEF_WS3_C "\001\x1b[33m\002"
#define DEF_WS4_C "\001\x1b[32m\002"
#define DEF_WS5_C "\001\x1b[36m\002"
#define DEF_WS6_C "\001\x1b[36m\002"
#define DEF_WS7_C "\001\x1b[36m\002"
#define DEF_WS8_C "\001\x1b[36m\002"

/* Suggestions */
#define DEF_SB_C "\x1b[02;33m"    /* Shell built-ins */
#define DEF_SC_C "\x1b[02;36m"    /* Aliases and binaries in PATH */
#define DEF_SD_C "\x1b[02;37m"    /* Internal commands description */
#define DEF_SH_C "\x1b[02;35m"    /* Commands history */
#define DEF_SF_C "\x1b[02;04;36m" /* ELN's, bookmark, file, and directory names */
#define DEF_SP_C "\x1b[02;31m"    /* Suggestions pointer (12 > filename) */
#define DEF_SX_C "\x1b[02;32m"    /* Internal commands and parameters */
#define DEF_SZ_C "\x1b[02;04;35m" /* File names (fuzzy) */

/* Highlight */
#define DEF_HB_C "\x1b[00;36m" /* Parenthesis, Brackets ( {[()]} ) */
#define DEF_HC_C "\x1b[02;37m" /* Comments (#comment) */
#define DEF_HD_C "\x1b[00;36m" /* Slashes (for paths) */
#define DEF_HE_C "\x1b[00;36m" /* Expansion operators (* ~) */
//#define DEF_HN_C "\x1b[00;35m" /* Numbers (including ELN's) */
#define DEF_HN_C "\x1b[0m" /* Numbers (including ELN's) */
#define DEF_HP_C "\x1b[00;36m" /* Parameters (e.g. -h --help) */
#define DEF_HQ_C "\x1b[00;33m" /* Quotes (single and double) */
#define DEF_HR_C "\x1b[00;31m" /* Redirection operator (>) */
#define DEF_HS_C "\x1b[00;32m" /* Process separators (; & |)*/
#define DEF_HV_C "\x1b[00;32m" /* Variables ($VAR) */

/* Colors for the properties and long/detail view functions */
#define DEF_DD_C     "\x1b[00;36m" /* Modification date */
#define DEF_DG_C     "\x1b[00;33m" /* UID, GID */
#define DEF_DG_C256  "\x1b[0;38;5;214m"
#define DEF_DN_C     "\x1b[00;02;37m" /* Dash (no attribute) */
#define DEF_DO_C     "\x1b[00;36m" /* Perms in octal */
#define DEF_DO_C256  "\x1b[0;38;5;50m"
#define DEF_DP_C     "\x1b[00;36m" /* SUID, SGID, Sticky */
#define DEF_DP_C256  "\x1b[0;38;5;50m"
#define DEF_DR_C     "\x1b[00;33m" /* Read perm */
#define DEF_DR_C256  "\x1b[0;38;5;227m"
#define DEF_DW_C     "\x1b[00;31m" /* Write perm */
#define DEF_DW_C256  "\x1b[0;38;5;196m"
#define DEF_DXD_C    "\x1b[00;32m" /* Execute perm (dirs) */
#define DEF_DXD_C256 "\x1b[0;38;5;40m"
#define DEF_DXR_C    "\x1b[00;36m" /* Execute perm (reg files) */
#define DEF_DXR_C256 "\x1b[0;38;5;50m"
#define DEF_DZ_C     "\x1b[00;32m" /* Size (only for dirs) */

/* Default color shades and attributes for date field in file properties */
#define DEF_DATE_SHADES_256 "2,196-2,231,253,250,247,244"
#define DEF_DATE_SHADES_8   "1,31-2,36-1,36,36-2"

/* Default color shades and attributes for size field in file properties */
#define DEF_SIZE_SHADES_256 "2,196-2,40,112,227,214,202"
#define DEF_SIZE_SHADES_8   "1,31-2,32,33,31"

/* This one is purely internal: cur_color is set to this value when
 * entering the warning prompt, so that that we can check cur_color
 * anytime to know whether we are in the warning prompt or not */
//#define DEF_HW_C "\x1b[00;31m"
//#define DEF_HW_C DEF_WP_C

#define DEF_DIR_ICO_C "\x1b[00;33m"

/* Characters used to classify files when running colorless and classify
 * is enabled */
#define EXEC_CHR    '*'
#define FIFO_CHR    '|'
#define DIR_CHR     '/'
#define LINK_CHR    '@'
#define BRK_LNK_CHR '!'
#define SOCK_CHR    '='
#define BLK_CHR     '+'
#define CHR_CHR     '-'
#define UNKNOWN_CHR '?'

#define SELFILE_CHR  '*'
#define TRIMFILE_CHR '~'

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
#define DEF_CMD_DESC_SUG 1
#define DEF_COLOR_SCHEME "default"
#define DEF_COLOR_SCHEME_256 "default-256"
#define DEF_COLORS 1
//#define DEF_CONTROL_D_EXITS 1
#define DEF_CP_CMD CP_CP
#define DEF_CUR_WS 0
#define DEF_CWD_IN_TITLE 0
#define DEF_DESKTOP_NOTIFICATIONS 0
#define DEF_DIRHIST_MAP 0
#define DEF_DISK_USAGE 0
#define DEF_DIV_LINE "-"
#define DEF_ELN_USE_WORKSPACE_COLOR 0
//#define DEF_EXPAND_BOOKMARKS 0
#define DEF_EXT_CMD_OK 1
#define DEF_FILES_COUNTER 1
#define DEF_FOLLOW_SYMLINKS 1
#define DEF_FULL_DIR_SIZE 0
#define DEF_FUZZY_MATCH 0
#define DEF_FUZZY_MATCH_ALGO 2 /* 1 or 2. 2 is Unicode aware, but slower than 1 */
#define DEF_FZF_WIN_HEIGHT 40 /* Max win percentage taken by FZF */
#define DEF_FZF_PREVIEW 1
#define DEF_HIGHLIGHT 1
#define DEF_HIST_STATUS 1
#define DEF_ICONS 0
#define DEF_INT_VARS 0
#define DEF_LIGHT_MODE 0
#define DEF_LIST_DIRS_FIRST 1
#define DEF_LISTING_MODE VERTLIST
#define DEF_LOGS_ENABLED 0
#define DEF_LOG_CMDS 0
#define DEF_LONG_VIEW 0
#define DEF_MAX_DIRHIST 100
#define DEF_MAX_FILES UNSET
#define DEF_MAX_NAME_LEN 20
#define DEF_MAX_HIST 1000
#define DEF_MAX_JUMP_TOTAL_RANK 100000
#define DEF_MAX_LOG 1000
#define DEF_MAX_PATH 40
#define DEF_MAX_PRINTSEL 0
#define DEF_MIN_JUMP_RANK 10
#define DEF_MIN_NAME_TRIM 20
#define DEF_MOUNT_CMD MNT_UDEVIL
#define DEF_MV_CMD MV_MV
#define DEF_NOELN 0
#define DEF_ONLY_DIRS 0
#define DEF_PAGER 0
#define DEF_PRINTSEL 0
#define DEF_PRINT_REMOVED_FILES 1
#define DEF_PRIVATE_WS_SETTINGS 0
#define DEF_PROP_FIELDS "fpims" /* Files counter, permissions, owner/grp Ids, mod Time, Size (human readable) */
#define DEF_PURGE_JUMPDB 0
#define DEF_REFRESH_ON_EMPTY_LINE 1
#define DEF_REFRESH_ON_RESIZE 1
#define DEF_RELATIVE_TIME 0
#define DEF_RESTORE_LAST_PATH 1
#define DEF_RL_EDIT_MODE 1
#define DEF_SECURE_CMDS 0
#define DEF_SECURE_ENV 0
#define DEF_SECURE_ENV_FULL 0
#define DEF_SHARE_SELBOX 0
#define DEF_SHOW_HIDDEN 0
#define DEF_SHOW_BACKUP_FILES 1
#define DEF_SI 0 /* If 1, use sizes in powers of 1000 instead of 1024 */
/* Available sort options:
 * SNONE, SNAME, STSIZE, SATIME, SBTIME, SCTIME, SMTIME
 * SVER, SEXT, SINO, SOWN, SGRP */
#define DEF_SORT SVER
#define DEF_SORT_REVERSE 0
#define DEF_SPLASH_SCREEN 0
#ifdef __OpenBSD__
# define DEF_SUDO_CMD "doas"
#else
# define DEF_SUDO_CMD "sudo"
#endif
#define DEF_SUG_FILETYPE_COLOR 0
#define DEF_SUG_STRATEGY "ehfj-ac"
#define DEF_SUGGESTIONS 1
#define DEF_TIME_STYLE_RECENT "%b %e %H:%M" /* Timestamps in long view mode */
#define DEF_TIME_STYLE_OLDER  "%b %e  %Y"
#define DEF_TIME_STYLE_LONG   "%a %b %d %T %Y %z" /* Used by history and trash */
#define DEF_TIPS 1
#define DEF_TOGGLE_WORKSPACES 1
#define DEF_TRASRM 0
#define DEF_UNICODE 1
#define DEF_WELCOME_MESSAGE 1

/* This expands to "CliFM > The command line file manager" */
#define DEF_WELCOME_MESSAGE_STR _PROGRAM_NAME " > " PROGRAM_DESC

/* We have three search strategies: GLOB_ONLY, REGEX_ONLY, GLOB_REGEX */
#define DEF_SEARCH_STRATEGY GLOB_REGEX

/* Four bell styles: BELL_NONE, BELL_AUDIBLE, BELL_VISIBLE, BELL_FLASH */
#define DEF_BELL_STYLE     BELL_VISIBLE
#define VISIBLE_BELL_DELAY 30 /* Milliseconds */

/* cp, mv, and rm commands */
#define _DEF_CP_CMD "cp -iRp"
#define _DEF_CP_CMD_FORCE "cp -Rp"
#define _DEF_ADVCP_CMD "advcp -giRp"
#define _DEF_ADVCP_CMD_FORCE "advcp -gRp"
#define _DEF_WCP_CMD "wcp"
#define _DEF_RSYNC_CMD "rsync -avP"

#define _DEF_MV_CMD "mv -i"
#define _DEF_MV_CMD_FORCE "mv"
#define _DEF_ADVMV_CMD "advmv -gi"
#define _DEF_ADVMV_CMD_FORCE "advmv -g"

#define DEF_RM_FORCE 0

#define SUG_STRATS 7

#define MAX_WS 8

#define MIN_SCREEN_WIDTH 20
#define MIN_SCREEN_HEIGHT 5

#define DEF_PROMPT_NOTIF 1
#define DEFAULT_PROMPT "\\[\\e[0m\\][\\S\\[\\e[0m\\]]\\l \
\\A \\u:\\H \\[\\e[00;36m\\]\\w\\n\\[\\e[0m\\]<\\z\\[\\e[0m\\]\
>\\[\\e[00;34m\\] \\$\\[\\e[0m\\] "
#define DEFAULT_PROMPT_NO_COLOR "[\\S]\\l \\A \\u:\\H \\w\\n<\\z> \\$ "

#define DEF_WARNING_PROMPT 1
#define DEF_WPROMPT_STR "\\[\\e[00;02;31m\\](!) > "
#define DEF_WPROMPT_STR_NO_COLOR "(!) > "

/* These options, except --preview-window=border-left (fzf 0.27), work with at
 * least fzf 0.18.0 (Mar 31, 2018) */
#define DEF_FZFTAB_OPTIONS "--color=16,prompt:6,fg+:-1,pointer:4,\
hl:2,hl+:2,gutter:-1,marker:2,border:7:dim --bind tab:accept,right:accept,\
left:abort,alt-p:toggle-preview,change:top,alt-up:preview-page-up,\
alt-down:preview-page-down --inline-info --layout=reverse-list \
--preview-window=wrap,border-left"
// --preview-window=noborder (0.19, Nov 15, 2019)

#define DEF_FZFTAB_OPTIONS_NO_COLOR "--color=bw --bind tab:accept,\
right:accept,left:abort,alt-p:toggle-preview,change:top,alt-up:preview-page-up,\
alt-down:preview-page-down --inline-info --layout=reverse-list \
--preview-window=wrap,border-left"

#define DEF_SMENU_OPTIONS "-a t:2,b b:4 c:r ct:2,r sf:6,r st:5,r mt:5,b"

/* Should we add __APPLE__ here too? */
#if defined(__HAIKU__)
# define DEF_TERM_CMD "Terminal"
#else
# define DEF_TERM_CMD "xterm -e"
#endif

#define FALLBACK_SHELL "/bin/sh"

#if defined(__APPLE__)
# define FALLBACK_OPENER "/usr/bin/open"
#elif defined(__CYGWIN__)
# define FALLBACK_OPENER "cygstart"
#elif defined(__HAIKU__)
# define FALLBACK_OPENER "open"
#else
# define FALLBACK_OPENER "xdg-open"
#endif

#endif /* SETTINGS_H */
