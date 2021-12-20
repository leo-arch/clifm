/* settings.h - User settings default values */

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

#ifndef SETTINGS_H
#define SETTINGS_H

/* User settings */

/* Macros for ELN padding */
#define NOPAD 			0
#define ZEROPAD 		1
#define LEFTSPACEPAD 	2
#define RIGHTSPACEPAD 	3

/* Macros for listing_mode */
#define VERTLIST 0 /* ls-like listing mode */
#define HORLIST 1

/* Default color definitions */
#define DEF_FILE_COLORS "di=01;34:nd=01;31:ed=00;34:ne=00;31:fi=0:\
ef=00;33:nf=00;31:ln=01;36:mh=30;46:or=00;36:pi=00;35:\
so=01;35:bd=01;33:cd=1:su=37;41:sg=30;43:ca=30;41:tw=30;42:\
ow=34;42:st=37;44:ex=01;32:ee=00;32:no=00;31;47:uf=34;47:"

#define DEF_IFACE_COLORS "el=01;33:mi=01;36:dl=01;34:tx=0:df=0:\
dc=00;02;34:wc=01;36:dh=00;36:li=01;32:si=01;34:ti=01;33:em=01;31:wm=01;33:\
nm=01;32:bm=01;36:sb=02;33:sc=02;31:sh=02;35:sf=02;04;36:sp=02;31:sx=02;32:\
hb=00;36:hc=02;31:hd=00;36:he=00;36:hn=00;35:hp=02;36:hq=00;33:\
hr=00;31:hs=00;32:hv=00;32:ts=04;35:wp=02;31:"

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

/* Filetypes */
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

/* Interface */
#define DEF_EL_C "\x1b[01;33m"
#define DEF_MI_C "\x1b[01;36m"
#define DEF_DL_C "\x1b[00;34m"
#define DEF_DC_C "\x1b[0m"
#define DEF_WC_C "\x1b[01;36m"
#define DEF_DH_C "\x1b[00;36m"
#define DEF_TS_C "\x1b[04;35m"
#define DEF_WP_C "\x1b[02;31m"
#define DEF_TT_C "\x1b[02;01;36m"

#define DEF_DF_C "\x1b[0m"

#define DEF_TX_C "\x1b[0m"
#define DEF_LI_C "\001\x1b[01;32m\002"
#define DEF_LI_CB "\x1b[01;32m"
#define DEF_TI_C "\001\x1b[01;33m\002"
#define DEF_EM_C "\001\x1b[01;31m\002"
#define DEF_WM_C "\001\x1b[01;33m\002"
#define DEF_NM_C "\001\x1b[01;32m\002"
#define DEF_SI_C "\001\x1b[01;34m\002"

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
#define DEF_SB_C "\x1b[02;33m"
#define DEF_SH_C "\x1b[02;35m"
#define DEF_SF_C "\x1b[02;04;36m"
#define DEF_SC_C "\x1b[02;31m"
#define DEF_SX_C "\x1b[02;32m"
#define DEF_SP_C "\x1b[02;31m"

/* Highlight */
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

#define DEF_DIR_ICO_C "\x1b[00;33m"

#define MNT_UDEVIL 	0
#define MNT_UDISKS2 1

/* Default options */
#define DEF_AUTOCD 1
#define DEF_AUTOJUMP 0
#define DEF_AUTO_OPEN 1
#define DEF_AUTOLS 1
#define DEF_CASE_SENS_LIST 0
#define DEF_CASE_SENS_DIRJUMP 0
#define DEF_CASE_SENS_PATH_COMP 0
#define DEF_CASE_SENS_SEARCH 0
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
#define DEF_ELNPAD LEFTSPACEPAD
#define DEF_EXPAND_BOOKMARKS 0
#define DEF_EXT_CMD_OK 1
#define DEF_FILES_COUNTER 1
#define DEF_FOLLOW_SYMLINKS 1
#define DEF_FZFTAB 0
#define DEF_FZF_WIN_HEIHGT 80 /* Max win percentage taken by FZF */
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
#define DEF_WARNING_PROMPT 1
#define SUG_STRATS 7

/* These options should work with FZF 0.17.5 or later */
#define DEF_FZFTAB_OPTIONS "--color=16,prompt:6,fg+:-1,pointer:2,\
hl:5,hl+:5,gutter:-1 --bind tab:accept,right:accept,\
left:abort --inline-info --layout=reverse-list"

#define MAX_WS 8

#define DEFAULT_PROMPT "\\[\\e[0m\\][\\[\\e[0;36m\\]\\S\\[\\e[0m\\]]\\l \
\\A \\u:\\H \\[\\e[00;36m\\]\\w\\n\\[\\e[0m\\]<\\z\\[\\e[0m\\]\
>\\[\\e[0;34m\\] \\$\\[\\e[0m\\] "

#define DEF_WPROMPT_STR "(!) > "

#define DEFAULT_TERM_CMD "xterm -e"

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

#endif /* SETTINGS_H */
