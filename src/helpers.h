#pragma once

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

#ifndef PATH_MAX
# define PATH_MAX 4096
#endif

#ifndef HOST_NAME_MAX
# define HOST_NAME_MAX 64
#endif

#ifndef NAME_MAX
# define NAME_MAX 255
#endif

#define PROGRAM_NAME "CliFM"
#define PNL "clifm" // Program name lowercase

/* If no formatting, puts (or write) is faster than printf */
/* #define CLEAR puts("\x1b[c") "\x1b[2J\x1b[1;1H"*/
#define CLEAR write(STDOUT_FILENO, "\033c", 3);
/* #define CLEAR puts("\x1b[2J\x1b[1;1H");
* #define CLEAR write(STDOUT_FILENO, "\x1b[2J\x1b[3J\x1b[H", 11);
* #define CLEAR puts("\x1b[1;1H\x1b[2J");
* #define CLEAR puts("\x1b[H\x1b[J");
* #define CLEAR write(STDOUT_FILENO, "\x1b[2J\x1b[H", 7); */

#define VERSION "1.0"
#define AUTHOR "L. Abramovich"
#define CONTACT "johndoe.arch@outlook.com"
#define WEBSITE "https://github.com/leo-arch/clifm"
#define DATE "April 10, 2021"
#define LICENSE "GPL2+"

/* Define flags for program options and internal use */
/* Variable to hold all the flags (int == 4 bytes == 32 bits == 32 flags).
 * In case more flags are needed, use a long double variable
 * (16 bytes == 128 flags) and even several of these */
static int flags;
/* Options flags: None of these are really useful. Just testing */
#define FOLDERS_FIRST   (1 << 1) /* 4 dec, 0x04 hex, 00000100 binary */
#define HELP            (1 << 2) /* and so on... */
#define HIDDEN          (1 << 3)
#define ON_THE_FLY      (1 << 4)
#define SPLASH          (1 << 5)
#define CASE_SENS       (1 << 6)
#define START_PATH      (1 << 7)
#define PRINT_VERSION   (1 << 8)
#define ALT_PROFILE     (1 << 9)

/* Internal flags */
#define ROOT_USR        (1 << 10)
#define EXT_HELP        (1 << 11)
#define FILE_CMD_OK     (1 << 12)
#define GUI             (1 << 13)
#define IS_USRVAR_DEF   (1 << 14) /* 18 dec, 0x12 hex, 00010010 binary */

/* Used by log_msg() to know wether to tell prompt() to print messages or
 * not */
#define PRINT_PROMPT 1
#define NOPRINT_PROMPT 0

/* Macros for xchdir (for setting term title or not) */
#define SET_TITLE 1
#define NO_TITLE 0

/* Error codes, to be used by launch_exec functions */
#define EXNULLERR 79
#define EXFORKERR 81
#define EXCRASHERR 82

#define BACKGROUND 1
#define FOREGROUND 0

/* ###COLORS### */
/* These are just a fixed color stuff in the interface. Remaining colors
 * are customizable and set via the config file */

/* \x1b: hex value for escape char (alternative: ^[), dec == 27
 * \033: octal value for escape char
 * \e is non-standard */
#define gray "\x1b[1;30m"
#define white "\x1b[1;37m"
#define cyan "\x1b[1;36m"
#define d_cyan "\x1b[0;36m"
#define bold "\x1b[1m"
#define NB "\x1b[49m"

#define COLORS_REPO "https://github.com/leo-arch/clifm-colors"

/* Colors for the prompt: */
/* \001 and \002 tell readline that color codes between them are
 * non-printing chars. This is specially useful for the prompt, i.e.,
 * when passing color codes to readline */
#define NC_b "\001\x1b[0m\002"
#define NB_b "\001\x1b[49m\002"

/* Default colors */
#define DEF_LS_COLORS "di=01;34:fi=00;37:ln=01;36:mh=30;46:or=00;36:\
pi=00;35:so=01;35:bd=01;33:cd=01;37:su=37;41:sg=30;43:st=37;44:\
tw=30;42:ow=34;42:ex=01;32:no=31;47"

#define DEF_FILE_COLORS "di=01;34:nd=01;31:ed=00;34:ne=00;31:fi=00;37:\
ef=00;33:nf=00;31:ln=01;36:mh=30;46:or=00;36:pi=00;35:\
so=01;35:bd=01;33:cd=01;37:su=37;41:sg=30;43:ca=30;41:tw=30;42:\
ow=34;42:st=37;44:ex=01;32:ee=00;32:no=00;31;47:uf=34;47:"

#define DEF_IFACE_COLORS "el=01;33:mi=01;36:dl=01;34:tx=00;37:df=00;37:\
dc=00;37:wc=01;36:dh=00;36:li=01;32:si=01;34:ti=01;33:em=01;31:wm=01;33:\
nm=01;32:bm=01;36:"

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
#define DEF_FI_C "\x1b[00;37m"
#define DEF_EF_C "\x1b[00;33m"
#define DEF_NF_C "\x1b[00;31m"
#define DEF_LN_C "\x1b[01;36m"
#define DEF_OR_C "\x1b[00;36m"
#define DEF_PI_C "\x1b[00;35m"
#define DEF_SO_C "\x1b[01;35m"
#define DEF_BD_C "\x1b[01;33m"
#define DEF_CD_C "\x1b[01;37m"
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
#define DEF_DF_C "\x1b[00;37m"
#define DEF_DC_C "\x1b[00;37m"
#define DEF_WC_C "\x1b[01;36m"
#define DEF_DH_C "\x1b[00;36m"

#define DEF_TX_C "\001\x1b[00;37m\002"
#define DEF_LI_C "\001\x1b[01;32m\002"
#define DEF_TI_C "\001\x1b[01;33m\002"
#define DEF_EM_C "\001\x1b[01;31m\002"
#define DEF_WM_C "\001\x1b[01;33m\002"
#define DEF_NM_C "\001\x1b[01;32m\002"
#define DEF_SI_C "\001\x1b[01;34m\002"

#define DEF_DIR_ICO_C "\x1b[00;33m"

// Default options
#define DEF_SPLASH_SCREEN 0
#define DEF_WELCOME_MESSAGE 1
#define DEF_SHOW_HIDDEN 0
#define DEF_SORT 1
#define DEF_FILES_COUNTER 1
#define DEF_LONG_VIEW 0
#define DEF_EXT_CMD_OK 0
#define DEF_PAGER 0
#define DEF_MAX_HIST 1000
#define DEF_MAX_DIRHIST 100
#define DEF_MAX_LOG 500
#define DEF_CLEAR_SCREEN 1
#define DEF_LIST_FOLDERS_FIRST 1
#define DEF_CD_LISTS_ON_THE_FLY 1
#define DEF_CASE_SENSITIVE 0
#define DEF_UNICODE 0
#define DEF_MAX_PATH 40
#define DEF_LOGS_ENABLED 0
#define DEF_DIV_LINE_CHAR '='
#define DEF_LIGHT_MODE 0
#define DEF_CLASSIFY 1
#define DEF_SHARE_SELBOX 0
#define DEF_SORT 1
#define DEF_SORT_REVERSE 0
#define DEF_TIPS 1
#define DEF_AUTOCD 1
#define DEF_AUTO_OPEN 1
#define DEF_DIRHIST_MAP 0
#define DEF_DISK_USAGE 0
#define DEF_RESTORE_LAST_PATH 0
#define DEF_EXPAND_BOOKMARKS 0
#define DEF_ONLY_DIRS 0
#define DEF_CD_ON_QUIT 0
#define DEF_COLORS 1
#define MAX_WS 8
#define DEF_CUR_WS 0
#define DEF_TRASRM 0
#define DEF_NOELN 0
#define DEF_MIN_NAME_TRIM 20
#define DEF_CASE_SENS_DIRJUMP 1
#define DEF_CASE_SENS_PATH_COMP 1
#define DEF_MIN_JUMP_RANK 10
#define DEF_MAX_JUMP_TOTAL_RANK 100000
#define DEF_CWD_IN_TITLE 0
#define UNSET -1
#define DEF_MAX_FILES UNSET

// Macros for the cp and mv cmds
#define CP_CP 0
#define CP_ADVCP 1
#define CP_WCP 2
#define DEF_CP_CMD CP_CP
#define MV_MV 0
#define MV_ADVMV 1
#define DEF_MV_CMD MV_MV

// Macros for the colors_list function
#define NO_ELN 0
#define NO_PAD 0
#define PRINT_NEWLINE 1
#define NO_NEWLINE 0

// Sort macros
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

// Macros to control file descriptors in exec functions
#define E_NOFLAG    0
#define E_NOSTDIN   (1 << 1)
#define E_NOSTDOUT  (1 << 2)
#define E_NOSTDERR  (1 << 3)
#define E_MUTE      (E_NOSTDOUT | E_NOSTDERR)

// Max length of the properties string in long view mode
#define MAX_PROP_STR 55

#define DEFAULT_PROMPT "\\[\\e[0;37m\\][\\[\\e[0;36m\\]\\S\\[\\e[0;37m\\]]\\l \
\\A \\u:\\H \\[\\e[00;36m\\]\\w\\n\\[\\e[0;37m\\]\\z\\[\\e[0;34m\\] \
\\$\\[\\e[0m\\] "

#define GRAL_USAGE "[-aAefFgGhiIlLmoOsSuUvxy] [-b FILE] [-c FILE] \
[-k FILE] [-p PATH] [-P PROFILE] [-z METHOD]"

#define DEFAULT_TERM_CMD "xterm -e"

#define FALLBACK_SHELL "/bin/sh"
#define FALLBACK_OPENER "xdg-open"

#ifndef __FreeBSD__
#  define strlen xstrlen
#endif
#define strcpy xstrcpy
#define strncpy xstrncpy
#define strcmp xstrcmp
#define strncmp xstrncmp
#define _(String) gettext (String)

#define ENTRY_N 64

#define TOUPPER(ch) (((ch) >= 'a' && (ch) <= 'z') ? ((ch) - 'a' + 'A') : (ch))
#define DIGINUM(n) (((n) < 10) ? 1 : ((n) < 100) ? 2 : ((n) < 1000) ? 3 : ((n) < 10000) ? 4 : ((n) < 100000) ? 5 : ((n) < 1000000) ? 6 : ((n) < 10000000) ? 7 : ((n) < 100000000) ? 8 : ((n) < 1000000000) ? 9 : 10)
#define _ISDIGIT(n) ((unsigned int)(n) - '0' <= 9)
#define _ISALPHA(n) ((unsigned int)(n) >= 'a' && (unsigned int)(n) <= 'z')
#define SELFORPARENT(n) (*(n) == '.' && (!(n)[1] || ((n)[1] == '.' && !(n)[2])))

// dirjump macros for calculating directories rank extra points
#define BASENAME_BONUS 300 
#define BOOKMARK_BONUS 500 
#define PINNED_BONUS 1000 
#define WORKSPACE_BONUS 300 // Last directory access 
#define JHOUR(n) ((n) *= 4) // Within last hour
#define JDAY(n) ((n) *= 2)  // Within last day
#define JWEEK(n) ((n) / 2)  // Within last week
#define JOLDER(n) ((n) / 4) // More than a week
