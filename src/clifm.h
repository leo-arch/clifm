
			/* ##################################
			 * #		CLIFM HEADER FILE		#
			 * ##################################
			 * */

#ifndef CLIFM_H
#define CLIFM_H

#if defined(__linux__) && !defined(_BE_POSIX)
#  define _GNU_SOURCE
#else
#  define _POSIX_C_SOURCE 200809L
#  define _DEFAULT_SOURCE
#  if __FreeBSD__
#    define __XSI_VISIBLE 700
#    define __BSD_VISIBLE 1
#  endif
#endif

/* Support large files on ARM or 32-bit machines */
#if defined(__arm__) || defined(__i386__)
#   define _FILE_OFFSET_BITS 64
#endif

/* #define __SIZEOF_WCHAR_T__ 4 */

/* Linux */
/* The only Linux-specific function I use, and the only function
 * requiring _GNU_SOURCE, is statx(), and only to get files creation
 * (birth) date in the properties function.
 * However, the statx() is conditionally added at compile time, so that,
 * if _BE_POSIX is defined (pass the -D_BE_POSIX option to the compiler),
 * the program will just ommit the call to the function, being thus
 * completely POSIX-2008 compliant. */

/* DEFAULT_SOURCE enables strcasecmp() and realpath() functions,
and DT_DIR (and company) and S_ISVTX macros */

/* FreeBSD*/
/* Up tp now, two features are disabled in FreeBSD: file capabilities and
 * immutable bit checks */

/* The following C libraries are located in /usr/include */
#include <stdio.h> /* (f)printf, s(n)printf, scanf, fopen, fclose, unlink,
					fgetc, fputc, perror, rename, sscanf, getline */
#include <string.h> /* str(n)cpy, str(n)cat, str(n)cmp, strlen, strstr,
					memset */
#include <stdlib.h> /* getenv, malloc, calloc, free, atoi, realpath,
					EXIT_FAILURE and EXIT_SUCCESS macros */
#include <stdarg.h> /* va_list */
#include <dirent.h> /* scandir */
#include <unistd.h> /* sleep, readlink, chdir, symlink, access, exec,
					* isatty, setpgid, getpid, getuid, gethostname,
					* tcsetpgrp, tcgetattr, __environ, STDIN_FILENO,
					* STDOUT_FILENO, and STDERR_FILENO macros */
#include <sys/stat.h> /* stat, lstat, mkdir */
#include <sys/wait.h> /* waitpid, wait */
#include <sys/ioctl.h> /* ioctl */
#include <sys/acl.h> /* acl_get_file(), acl_get_entry() */
#include <time.h> /* localtime, strftime, clock (to time functions) */
#include <grp.h> /* getgrgid */
#include <signal.h> /* trap signals */
#include <pwd.h> /* getcwd, getpid, geteuid, getpwuid */
#include <glob.h> /* glob */
#include <termios.h> /* struct termios */
#include <locale.h> /* setlocale */
#include <errno.h>
#include <getopt.h> /* getopt_long */
#include <fcntl.h> /* O_RDONLY, O_DIRECTORY, and AT_* macros */
#include <readline/history.h>
/* Readline: This function allows the user to move back and forth with
 * the arrow keys in the prompt. I've tried scanf, getchar, getline,
 * fscanf, fgets, and none of them does the trick. Besides, readline
 * provides TAB completion and history. Btw, readline is what Bash uses
 * for its prompt */
#include <readline/readline.h>
#include <libintl.h> /* gettext */
#include <limits.h>

#if __linux__
#  include <sys/capability.h> /* cap_get_file. NOTE: This header exists
in FreeBSD, but is deprecated */
#  include <linux/limits.h> /* PATH_MAX (4096), NAME_MAX (255) macros */
#  include <linux/version.h> /* LINUX_VERSION_CODE && KERNEL_VERSION
								macros */
#  include <linux/fs.h> /* FS_IOC_GETFLAGS, S_IMMUTABLE_FL macros */
/*#elif __FreeBSD__
#  include <strings.h> Enables strcasecmp() */
#endif /* __linux__ */

#include <uchar.h> /* char32_t and char16_t types */
/* #include <bsd/string.h> // strlcpy, strlcat */
/* #include <sys/types.h> */

#include <wordexp.h> /* For command substitution */
#include <sys/statvfs.h>
#include <regex.h>
#include <wchar.h>

/* Without this variable, TCC complains that __dso_handle is an
 * undefined symbol and won't compile */
#if __TINYC__
void* __dso_handle;
#endif

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

/* _GNU_SOURCE is only defined if __linux__ is defined and _BE_POSIX
 * is not defined */
#ifdef _GNU_SOURCE
#  if (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 28))
#    if LINUX_VERSION_CODE >= KERNEL_VERSION(4,11,0)
#      define _STATX
#    endif /* LINUX_VERSION (4.11) */
#  endif /* __GLIBC__ */
#endif /* _GNU_SOURCE */

/* Because capability.h is deprecated in BSD */
#if __linux__
#  if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
#    define _LINUX_CAP
#  endif /* LINUX_VERSION (2.6.24)*/
#endif /* __linux__ */

#define PROGRAM_NAME "CliFM"
#define PNL "clifm" /* Program name lowercase */
/* #define TMP_DIR "/tmp/clifm" */
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

/* Default options */
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

/* Macros for the cp and mv cmds */
#define CP_CP 0
#define CP_ADVCP 1
#define CP_WCP 2
#define DEF_CP_CMD CP_CP
#define MV_MV 0
#define MV_ADVMV 1
#define DEF_MV_CMD MV_MV

/* Macros for the colors_list function */
#define NO_ELN 0
#define NO_PAD 0
#define PRINT_NEWLINE 1
#define NO_NEWLINE 0

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

/* 46 == \x1b[00;38;02;000;000;000;00;48;02;000;000;000m\0 (24bit, RGB
 * true color format including foreground and background colors, the SGR
 * (Select Graphic Rendition) parameter, and, of course, the terminating
 * null byte. */
#define MAX_COLOR 46

/* Macros to control file descriptors in exec functions */
#define E_NOFLAG    0
#define E_NOSTDIN   (1 << 1)
#define E_NOSTDOUT  (1 << 2)
#define E_NOSTDERR  (1 << 3)
#define E_MUTE      (E_NOSTDOUT | E_NOSTDERR)

/* Max length of the properties string in long view mode */
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
/* #define atoi xatoi */
/* #define alphasort xalphasort */
#define _(String) gettext (String)

#define ENTRY_N 64

#define TOUPPER(ch) (((ch) >= 'a' && (ch) <= 'z') ? ((ch) - 'a' + 'A') : (ch))
#define DIGINUM(n) (((n) < 10) ? 1 : ((n) < 100) ? 2 : ((n) < 1000) ? 3 : ((n) < 10000) ? 4 : ((n) < 100000) ? 5 : ((n) < 1000000) ? 6 : ((n) < 10000000) ? 7 : ((n) < 100000000) ? 8 : ((n) < 1000000000) ? 9 : 10)
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


				/** #########################
				 *  #    GLOBAL VARIABLES   #
				 *  ######################### */

/* Struct to store user defined variables */
struct usrvar_t
{
	char *name;
	char *value;
};

extern struct usrvar_t *usr_var;

/* Struct to store user defined actions */
struct actions_t
{
	char *name;
	char *value;
};

extern struct actions_t *usr_actions;

/* Workspaces information */
struct ws_t
{
	char *path;
	int num;
};

extern struct ws_t *ws;

/* Struct to store user defined keybindings */
struct kbinds_t
{
	char *function;
	char *key;
};

extern struct kbinds_t *kbinds;

/* Struct to store the dirjump database values */
struct jump_t
{
	char *path;
	int keep;
	int rank;
	size_t visits;
	time_t first_visit;
	time_t last_visit;
};

extern struct jump_t *jump_db;

/* Struct to store bookmarks */
struct bookmarks_t
{
	char *shortcut;
	char *name;
	char *path;
};

extern struct bookmarks_t *bookmarks;

/* Struct to store file information */
struct fileinfo {
	char *name;
	char *color;
	char *icon;
	char *icon_color;
	int eln_n;
	int filesn; /* Number of files in subdir */
	int symlink;
	int dir;
	int exec;
	int ruser; /* User read permission for dir */
	size_t len;
	mode_t type; /* Store d_type value */
	mode_t mode; /* Store st_mode (for long view mode) */
	ino_t inode;
	off_t size;
	uid_t uid;
	gid_t gid;
	nlink_t linkn;
	time_t time;
	time_t ltime; /* For long view mode */
};

extern struct fileinfo *file_info;

/* Struct to specify which parameters have been set from the command
 * line, to avoid overriding them with init_config(). While no command
 * line parameter will be overriden, the user still can modifiy on the
 * fly (editing the config file) any option not specified in the command
 * line */
struct param
{
	int splash;
	int hidden;
	int longview;
	int cd_list_auto;
	int autocd;
	int auto_open;
	int ext;
	int ffirst;
	int sensitive;
	int unicode;
	int pager;
	int path;
	int light;
	int sort;
	int dirmap;
	int config;
	int stealth_mode;
	int restore_last_path;
	int tips;
	int disk_usage;
	int classify;
	int share_selbox;
	int rl_vi_mode;
	int max_dirhist;
	int sort_reverse;
	int files_counter;
	int welcome_message;
	int clear_screen;
	int logs;
	int max_path;
	int bm_file;
	int expand_bookmarks;
	int only_dirs;
	int list_and_quit;
	int color_scheme;
	int cd_on_quit;
	int no_dirjump;
	int icons;
	int icons_use_file_color;
	int no_columns;
	int no_colors;
	int max_files;
	int trasrm;
	int noeln;
	int case_sens_dirjump;
	int case_sens_path_comp;
	int cwd_in_title;
};

extern struct param xargs;

/* A list of possible program messages. Each value tells the prompt what
 * to do with error messages: either to print an E, W, or N char at the
 * beginning of the prompt, or nothing (nomsg) */
enum prog_msg
{
	nomsg = 0,
	error = 1,
	warning = 2,
	notice = 4
};

/* Enumeration for the dirjump function options */
enum jump {
	none = 0,
	jparent = 1,
	jchild = 2,
	jorder = 4,
	jlist = 8
};

/* pmsg holds the current program message type */
extern enum prog_msg pmsg;

extern int flags;

extern short
	splash_screen,
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
	logs_enabled,
	sort,
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

extern int
	max_hist,
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

extern unsigned short term_cols;

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

extern struct termios shell_tmodes;
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

extern regex_t regex_exp;

extern size_t *ext_colors_len;

/* This is not a comprehensive list of commands. It only lists
 * commands long version for TAB completion */
extern const char *INTERNAL_CMDS[];

/* To store all the 39 color variables I use, with 46 bytes each, I need
 * a total of 1,8Kb. It's not much but it could be less if I'd use
 * dynamically allocated arrays for them (which, on the other side,
 * would make the whole thing slower and more tedious) */

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

				/** ##########################
				 *  #  FORWARD DECLARATIONS  #
				 *  ########################## */

/* misc.c */
int _err(int, int, const char *, ...);
int alias_import(char *file);
void bonus_function (void);
int create_usr_var(char *str);
int filter_function(const char *arg);
void free_software (void);
void free_stuff(void);
void handle_stdin(void);
void help_function (void);
int hidden_function(char **comm);
int list_commands (void);
int list_mountpoints(void);
int new_instance(char *dir, int sudo);
char *parse_usrvar_value(const char *str, const char c);
int pin_directory(char *dir);
void print_tips(int all);
void save_last_path(void);
void save_pinned_dir(void);
int set_shell(char *str);
void set_signals_to_ignore(void);
void set_term_title(const char *dir);
void splash (void);
int unpin_dir(void);
void version_function (void);

/* mime.c */
int mime_open(char **args);
int mime_import(char *file);
int mime_edit(char **args);
char *get_app(const char *mime, const char *ext);
char *get_mime(char *file);

/* jump.c */
int add_to_jumpdb(const char *dir);
void save_jumpdb(void);
int dirjump(char **args);
int edit_jumpdb(void);

/* bookmarks.c */
char **bm_prompt(void);
int bookmark_add(char *file);
int bookmark_del(char *name);
int bookmarks_function(char **cmd);
int edit_bookmarks(char *cmd);
int open_bookmark(void);
void free_bookmarks(void);

/* config.c */
int regen_config(void);
int edit_function (char **comm);
void define_config_file_names(void);
int create_config(const char *file);
void create_config_files(void);
void create_def_cscheme(void);
int create_kbinds_file(void);
void init_config(void);
void read_config(void);
int create_bm_file(void);
int reload_config(void);
void copy_plugins(void);
void edit_xresources(void);
void create_tmp_files(void);
void set_sel_file(void);
void set_env(void);

/* profiles.c */
int get_profile_names(void);
int profile_function(char **comm);
int profile_add(const char *prof);
int profile_del(const char *prof);
int profile_set(const char *prof);

/* keybinds.c */
void readline_kbinds(void);
int kbinds_function(char **args);
char *find_key(char *function);
int keybind_exec_cmd(char *str);
int load_keybinds(void);
int rl_refresh(int count, int key);
int rl_parent_dir(int count, int key);
int rl_root_dir(int count, int key);
int rl_home_dir(int count, int key);
int rl_next_dir(int count, int key);
int rl_first_dir(int count, int key);
int rl_last_dir(int count, int key);
int rl_previous_dir(int count, int key);
int rl_long(int count, int key);
int rl_folders_first(int count, int key);
int rl_light(int count, int key);
int rl_hidden(int count, int key);
int rl_open_config(int count, int key);
int rl_open_keybinds(int count, int key);
int rl_open_cscheme(int count, int key);
int rl_open_bm_file(int count, int key);
int rl_open_jump_db(int count, int key);
int rl_open_mime(int count, int key);
int rl_mountpoints(int count, int key);
int rl_select_all(int count, int key);
int rl_deselect_all(int count, int key);
int rl_bookmarks(int count, int key);
int rl_selbox(int count, int key);
int rl_clear_line(int count, int key);
int rl_sort_next(int count, int key);
int rl_sort_previous(int count, int key);
int rl_lock(int count, int key);
int rl_remove_sel(int count, int key);
int rl_export_sel(int count, int key);
int rl_move_sel(int count, int key);
int rl_rename_sel(int count, int key);
int rl_paste_sel(int count, int key);
int rl_quit(int count, int key);
int rl_previous_profile(int count, int key);
int rl_next_profile(int count, int key);
int rl_dirhist(int count, int key);
int rl_archive_sel(int count, int key);
int rl_new_instance(int count, int key);
int rl_clear_msgs(int count, int key);
int rl_trash_sel(int count, int key);
int rl_untrash_all(int count, int key);
int rl_open_sel(int count, int key);
int rl_bm_sel(int count, int key);
int rl_kbinds_help (int count, int key);
int rl_cmds_help (int count, int key);
int rl_manpage (int count, int key);
int rl_pinned_dir(int count, int key);
int rl_ws1(int count, int key);
int rl_ws2(int count, int key);
int rl_ws3(int count, int key);
int rl_ws4(int count, int key);
int rl_plugin1(int count, int key);
int rl_plugin2(int count, int key);
int rl_plugin3(int count, int key);
int rl_plugin4(int count, int key);

/* trash.c */
int trash_clear(void);
int trash_function (char **comm);
int trash_element(const char *suffix, struct tm *tm, char *file);
int untrash_element(char *file);
int untrash_function(char **comm);
int remove_from_trash(void);
int wx_parent_check(char *file);
int recur_perm_check(const char *dirname);

/* colors.c */
int set_colors(const char *colorscheme, int env);
void color_codes (void);
size_t get_colorschemes(void);
int cschemes_function(char **args);
char *strip_color_line(const char *str, char mode);
void free_colors(void);
int is_color_code(const char *str);
void colors_list(const char *ent, const int i, const int pad, const int new_line);
char *get_ext_color(const char *ext);

/* archives.c */
int is_compressed(char *file, int test_iso);
int archiver(char **args, char mode);
int zstandard(char *in_file, char *out_file, char mode, char op);
int check_iso(char *file);
int create_iso(char *in_file, char *out_file);
int handle_iso(char *file);

/* selection.c */
int deselect(char **comm);
int sel_function(char **args);
void show_sel_files(void);
int sel_glob(char *str, const char *sel_path, mode_t filetype);
int sel_regex(char *str, const char *sel_path, mode_t filetype);
int select_file(char *file);
int save_sel(void);

/* readline.c */
int initialize_readline(void);
char *my_rl_path_completion(const char *text, int state);
char **my_rl_completion(const char *text, int start, int end);
char *my_rl_quote(char *text, int mt, char *qp);
char *rl_no_hist(const char *prompt);
int quote_detector(char *line, int index);
char *bin_cmd_generator(const char *text, int state);
char *cschemes_generator(const char *text, int state);
char *filenames_gen_eln(const char *text, int state);
char *filenames_gen_text(const char *text, int state);
char *profiles_generator(const char *text, int state);
char *hist_generator(const char *text, int state);
char *jump_generator(const char *text, int state);
char *jump_entries_generator(const char *text, int state);
char *bookmarks_generator(const char *text, int state);
int is_quote_char(const char c);

/* exec.c */
int exec_cmd(char **comm);
void exec_chained_cmds(char *cmd);
void exec_profile(void);
int run_in_foreground(pid_t pid);
void run_in_background(pid_t pid);
int launch_execve(char **cmd, int bg, int xflags);
int launch_execle(const char *cmd);
int run_and_refresh(char **comm);

/* remotes.c */
int remote_ftp(char *address, char *options);
int remote_smb(char *address, char *options);
int remote_ssh(char *address, char *options);

/* navigation.c */
int back_function(char **comm);
int forth_function(char **comm);
int xchdir(const char *dir, const int set_title);
int cd_function(char *new_path);
int surf_hist(char **comm);
char *fastback(const char *str);
int workspaces(char *str);

/* file_operations.c */
int batch_link(char **args);
char *export(char **filenames, int open);
int bulk_rename(char **args);
int remove_file(char **args);
int copy_function(char **comm);
int edit_link(char *link);
int open_function(char **cmd);
int xchmod(const char *file, mode_t mode);

/* prompt.c */
char *decode_prompt(const char *line);
char *prompt(void);

/* listing.c */
void free_dirlist(void);
int list_dir(void);
int list_dir_light(void);
void get_dir_icon(const char *dir, int n);
void get_ext_icon(const char *restrict ext, int n);
void get_file_icon(const char *file, int n);
void print_disk_usage(void);
void print_div_line(void);
void print_sort_method(void);
void print_dirhist_map(void);

/* search.c */
int search_glob(char **comm, int invert);
int search_regex(char **comm, int invert);

/* sort.c */
int alphasort_insensitive(const struct dirent **a, const struct dirent **b);
int xalphasort(const struct dirent **a, const struct dirent **b);
int sort_function(char **arg);
int entrycmp(const void *a, const void *b);
int namecmp(const char* s1, const char* s2);
int skip_nonexec(const struct dirent *ent);
int skip_files(const struct dirent *ent);

/* actions.c */
int edit_actions(void);
int run_action(char *action, char **args);

/* strings.c */
char **split_str(const char *str);
char *split_fusedcmd(char *str);
char **parse_input_str(char *str);
char *savestring(const char *restrict str, size_t size);
char **get_substr(char *str, const char ifs);
char *dequote_str(char *text, int mt);
char *escape_str(const char *str);
int *expand_range(char *str, int listdir);
char *home_tilde(const char *new_path);
int strcntchr(const char *str, const char c);
char *straft(char *str, const char c);
char *straftlst(char *str, const char c);
char *strbfr(char *str, const char c);
char *strbfrlst(char *str, const char c);
char *strbtw(char *str, const char a, const char b);
char *gen_rand_str(size_t len);
char *xstrcpy(char *buf, const char *restrict str);
size_t xstrlen(const char *restrict s);
int xstrcmp(const char *s1, const char *s2);
int xstrncmp(const char *s1, const char *s2, size_t n);
char *xstrncpy(char *buf, const char *restrict str, size_t n);
size_t xstrsncpy(char *restrict dst, const char *restrict src, size_t n);
size_t wc_xstrlen(const char *restrict str);
int u8truncstr(char *restrict str, size_t n);
size_t u8_xstrlen(const char *restrict str);
char *remove_quotes(char *str);

/* history.c */
int save_dirhist(void);
void add_to_cmdhist(const char *cmd);
int record_cmd(char *input);
int get_history(void);
int history_function(char **comm);
int run_history_cmd(const char *cmd);
void add_to_dirhist(const char *dir_path);
int log_function(char **comm);
void log_msg(char *_msg, int print);

/* init.c */
void check_env_filter(void);
void get_prompt_cmds(void);
void get_aliases(void);
int get_last_path(void);
void check_options(void);
int load_dirhist(void);
size_t get_path_env(void);
void get_prompt_cmds(void);
int get_sel_files(void);
void init_shell(void);
void load_jumpdb(void);
int load_bookmarks(void);
int load_actions(void);
int load_pinned_dir(void);
void get_path_programs(void);
void unset_xargs(void);
void external_arguments(int argc, char **argv);
char *get_date (void);
pid_t get_own_pid(void);
char *get_user(void);
char *get_user_home(void);
char *get_sys_shell(void);

/* checks.c */
void file_cmd_check(void);
void check_file_size(char *log_file, int max);
char **check_for_alias(char **args);
int check_regex(char *str);
int check_immutable_bit(char *file);
int is_bin_cmd(const char *str);
int digit_found(const char *str);
int is_internal(const char *cmd);
int is_internal_c(const char *restrict cmd);
int is_number(const char *restrict str);
int is_acl(char *file);

/* properties.c */
int get_properties(char *filename, int dsize);
int properties_function(char **comm);
int print_entry_props(struct fileinfo *props, size_t max);

/* aux.c */
void *xrealloc(void *ptr, size_t size);
void *xcalloc(size_t nmemb, size_t size);
void *xnmalloc(size_t nmemb, size_t size);

int xgetchar(void);
char from_hex(char c);
char to_hex(char c);
char *url_encode(char *str);
char *url_decode(char *str);
int read_octal(char *str);
int hex2int(char *str);
int get_link_ref(const char *link);
off_t dir_size(char *dir);
char *get_size_unit(off_t size);
char *get_cmd_path(const char *cmd);
int count_dir(const char *dir_path);
int *get_hex_num(const char *str);
char *xitoa(int n);

#endif /* CLIFM_H */
