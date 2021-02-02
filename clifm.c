
			/** ########################################
			 *  #			    CliFM			       #
			 *  # The anti-eye-candy/KISS file manager #
			 *  ######################################## */

/*
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


#if defined(__linux__) && !defined(_BE_POSIX)
#  define _GNU_SOURCE
#else
#  define _POSIX_C_SOURCE 200809L
#  define _DEFAULT_SOURCE
#  if __FreeBSD__
#    define __XSI_VISIBLE 1
#    define __BSD_VISIBLE 1
#  endif
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
#include <stdio.h> /* (f)printf, s(n)printf, scanf, fopen, fclose, remove,
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
#include <readline/readline.h>
/* Readline: This function allows the user to move back and forth with
 * the arrow keys in the prompt. I've tried scanf, getchar, getline,
 * fscanf, fgets, and none of them does the trick. Besides, readline
 * provides TAB completion and history. Btw, readline is what Bash uses
 * for its prompt */
#include <libintl.h> /* gettext */

#include <limits.h>

#if __linux__
#  include <sys/capability.h> /* cap_get_file. NOTE: This header exists
in FreeBSD, but is deprecated */
#  include <linux/limits.h> /* PATH_MAX (4096), NAME_MAX (255) macros */
#  include <linux/version.h> /* LINUX_VERSION_CODE && KERNEL_VERSION
								macros */
#  include <linux/fs.h> /* FS_IOC_GETFLAGS, S_IMMUTABLE_FL macros */
#elif __FreeBSD__
/*#  include <strings.h> Enables strcasecmp() */
#endif

#include <uchar.h> /* char32_t and char16_t types */
/* #include <bsd/string.h> // strlcpy, strlcat */
/* #include <sys/types.h> */

#include "clifm.h" /* A few custom functions */


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
//#define CLEAR write(STDOUT_FILENO, "\033c", 3);
#define CLEAR write(STDOUT_FILENO, "\x1b[2J\x1b[3J\x1b[H", 11);
/* #define CLEAR write(STDOUT_FILENO, "\033[2J\033[H", 7); */
#define VERSION "0.27.3"
#define AUTHOR "L. Abramovich"
#define CONTACT "johndoe.arch@outlook.com"
#define WEBSITE "https://github.com/leo-arch/clifm"
#define DATE "February 1, 2021"
#define LICENSE "GPL2+"

/* Define flags for program options and internal use */
/* Variable to hold all the flags (int == 4 bytes == 32 bits == 32 flags).
 * In case more flags are needed, use a long double variable
 * (16 bytes == 128 flags) and even several of these */
static int flags;
/* Options flags: None of these are really useful. Just testing */
#define FOLDERS_FIRST 	(1 << 1) /* 4 dec, 0x04 hex, 00000100 binary */
#define HELP			(1 << 2) /* and so on... */
#define HIDDEN 			(1 << 3)
#define ON_THE_FLY 		(1 << 4)
#define SPLASH 			(1 << 5)
#define CASE_SENS 		(1 << 6)
#define START_PATH 		(1 << 7)
#define PRINT_VERSION	(1 << 8)
#define ALT_PROFILE		(1 << 9)

/* Internal flags */
#define ROOT_USR		(1 << 10)
#define EXT_HELP		(1 << 11)
#define FILE_CMD_OK		(1 << 12)
#define GRAPHICAL		(1 << 13)
#define IS_USRVAR_DEF	(1 << 14) /* 18 dec, 0x12 hex, 00010010 binary */

/* Used by log_msg() to know wether to tell prompt() to print messages or
 * not */
#define PRINT_PROMPT 1
#define NOPRINT_PROMPT 0

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
#define blue "\x1b[1;34m"
#define green "\x1b[1;32m"
#define gray "\x1b[1;30m"
#define white "\x1b[1;37m"
#define red "\x1b[1;31m"
#define cyan "\x1b[1;36m"
#define d_cyan "\x1b[0;36m"
#define NC "\x1b[0m"
#define bold "\x1b[1m"

/* Colors for the prompt: */
/* \001 and \002 tell readline that color codes between them are
 * non-printing chars. This is specially useful for the prompt, i.e.,
 * when passing color codes to readline */
#define red_b "\001\x1b[1;31m\002" /* error message indicator */
#define green_b "\001\x1b[1;32m\002" /* sel, notice message indicator */
#define yellow_b "\001\x1b[0;33m\002" /* trash, warning message indicator */
#define NC_b "\001\x1b[0m\002"
/* NOTE: Do not use \001 prefix and \002 suffix for colors list: they
 * produce a displaying problem in lxterminal (though not in aterm and
 * xterm). */

/* Replace some functions by my custom (faster, I think: NO!!)
 * implementations. */
/*#define strlen xstrlen // All calls to strlen will be replaced by a
 * call to xstrlen */
#define strcpy xstrcpy
#define strncpy xstrncpy
#define strcmp xstrcmp
#define strncmp xstrncmp
/* #define atoi xatoi */
/*#define alphasort xalphasort */
#define _(String) gettext (String)


				/** ##########################
				 * #  FUNCTIONS PROTOTYPES  #
				 * ##########################*/

/* Initialization */
void init_shell(void);
void init_config(void);
void exec_profile(void);
void external_arguments(int argc, char **argv);
void get_aliases(void);
void get_prompt_cmds(void);
void check_log_file_size(char *log_file);
void get_path_programs(void);
size_t get_path_env(void);
int create_config(char *file);
int set_shell(char *str);

/* Memory */
char *xnmalloc(size_t nmemb, size_t size);
void *xcalloc(size_t nmemb, size_t size);
void *xrealloc(void *ptr, size_t size);
void free_stuff(void);

/* Exec */
int launch_execve(char **cmd, int bg);
int launch_execle(const char *cmd);
int run_in_foreground(pid_t pid);
void run_in_background(pid_t pid);
char **check_for_alias(char **comm);
int exec_cmd(char **comm);

/* Signals */
void signal_handler(int sig_num);
void set_signals_to_ignore(void);
void set_signals_to_default(void);

/* Readline */
char **my_rl_completion(const char *text, int start, int end);
char *my_rl_quote(char *text, int m_t, char *qp);
int quote_detector(char *line, int index);
int is_quote_char(char c);
char *filenames_generator(const char *text, int state);
char *bin_cmd_generator(const char *text, int state);
void readline_kbinds(void);
int readline_kbind_action(int count, int key);
char *rl_no_hist(const char *prompt);
char *bookmarks_generator(const char *text, int state);
int initialize_readline(void);
char *my_rl_path_completion(const char *text, int state);
void keybind_exec_cmd(char *str);

/* Scandir filters */
int folder_select(const struct dirent *entry);
int file_select(const struct dirent *entry);
int skip_implied_dot(const struct dirent *entry);
int skip_nonexec(const struct dirent *entry);

/* Parsing */
char *home_tilde(const char *new_path);
char **parse_input_str(char *str);
int brace_expansion(const char *str);
int *expand_range(char *str, int listdir);
char **split_str(char *str);
int is_internal(const char *cmd);
char *escape_str(char *str);
int is_internal_c(const char *cmd);
char **get_substr(char *str, const char ifs);
char *savestring(const char *str, size_t size);
char *dequote_str(char *text, int m_t);
size_t u8_xstrlen(const char *str);

/* Listing */
int list_dir(void);
int list_dir_light(void);

/* Prompt */
char *prompt(void);
char *decode_prompt(char *line);

/* Navigation */
int back_function(char **comm);
int forth_function(char **comm);
int cd_function(char *new_path);
int open_function(char **cmd);

/* Files handling */
int copy_function(char **comm);
int run_and_refresh(char **comm);
int search_function(char **comm);
int new_instance(char *dir);
int list_mountpoints(void);
int alias_import(char *file);
void exec_chained_cmds(char *cmd);
int edit_function(char **comm);

/* Selection */
int save_sel(void);
int sel_function(char **comm);
void show_sel_files(void);
int deselect(char **comm);
int get_sel_files(void);

/* Bookmarks */
int bookmarks_function(char **cmd);
char **get_bookmarks(char *bookmarks_file);
char **bm_prompt(void);
int open_bookmark(char **cmd);
int get_bm_names(void);
int bookmark_del(char *name);
int bookmark_add(char *file);

/* Trash */
int trash_function(char **comm);
int trash_element(const char *suffix, struct tm *tm, char *file);
int wx_parent_check(char *file);
int remove_from_trash(void);
int untrash_function(char **comm);
int untrash_element(char *file);
int trash_clear(void);
int recur_perm_check(const char *dirname);

/* Mime */
int mime_open(char **args);
int mime_import(char *file);
int mime_edit(char **args);
char *get_mime(char *file);
char *get_app(char *mime, char *ext);

/* Profile */
int profile_function(char ** comm);
int profile_add(char *prof);
int profile_del(char *prof);
int profile_set(char *prof);
int get_profile_names(void);
char *profiles_generator(const char *text, int state);

/* History */
int history_function(char **comm);
int run_history_cmd(const char *cmd);
void surf_hist(char **comm);
int get_history(void);
void add_to_dirhist(const char *dir_path);
int record_cmd(char *input);
void add_to_cmdhist(char *cmd);

/* Sorting functions */
int sort_function(char **arg);
void print_sort_method(void);
#if !defined __FreeBSD__ && !defined _BE_POSIX
int m_versionsort(const struct dirent **a, const struct dirent **b);
#endif
int m_alphasort(const struct dirent **a, const struct dirent **b);
int alphasort_insensitive(const struct dirent **a,
						  const struct dirent **b);
int xalphasort(const struct dirent **a, const struct dirent **b);
int size_sort(const struct dirent **a, const struct dirent **b);
int atime_sort(const struct dirent **a, const struct dirent **b);
int btime_sort(const struct dirent **a, const struct dirent **b);
int ctime_sort(const struct dirent **a, const struct dirent **b);
int mtime_sort(const struct dirent **a, const struct dirent **b);

/* Archiving and compression */
int bulk_rename(char **args);
int archiver(char **args, char mode);
int zstandard(char *in_file, char *out_file, char mode, char op);
int is_compressed(char *file, int test_iso);
int check_iso(char *file);
int handle_iso(char *file);
int create_iso(char *file, char *out_file);

/* Logs */
int log_function(char **comm);
void log_msg(char *msg, int print);
int _err(int msg_type, int prompt, const char *format, ...);

/* Remote connections: Still under development */
int remote_ssh(char *address, char *options);
int remote_smb(char *address, char *options);
int remote_ftp(char *address, char *options);

/* Properties */
int properties_function(char **comm);
int get_properties(char *filename, int _long, int max,
				   size_t filename_len);

/* User variables */
char *parse_usrvar_value(const char *str, const char c);
int create_usr_var(char *str);

/* Printing functions */
void splash(void);
void version_function(void);
void bonus_function(void);
void colors_list(const char *entry, const int i, const int pad,
				 const int new_line);
int hidden_function(char **comm);
void help_function(void);
void color_codes(void);
int list_commands(void);
void free_software(void);
void print_tips(int all);

/* Checks */
char *get_cmd_path(const char *cmd);
size_t count_dir(const char *dir_path);
off_t dir_size(char *dir);
void file_cmd_check(void);
int get_max_long_view(void);
int is_color_code(char *str);
int get_link_ref(const char *link);
int *get_hex_num(char *str);

/* Colors */
void set_default_options(void);
void set_colors (void);


				/** ##########################
				 * #    GLOBAL VARIABLES    # 
				 * ##########################*/

/* Without this variable, TCC complains that __dso_handle is an
 * undefined symbol and won't compile */
#if __TINYC__
void* __dso_handle;
#endif

/* Struct to store user defined variables */
struct usrvar_t
{
	char *name;
	char *value;
};

/* Struct to store file information */
struct fileinfo
{
	size_t len;
	size_t filesn; /* Number of files in subdir */
	int exists;
	int brokenlink;
	int linkdir;
	int exec;
	mode_t type;
	off_t size;
	int ruser; /* User read permission for dir */
};

struct fileinfo *file_info = (struct fileinfo *)NULL;

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
	int cdauto;
	int ext;
	int ffirst;
	int sensitive;
	int unicode;
	int pager;
	int path;
	int light;
	int sort;
};

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

/* pmsg holds the current program message type */
enum prog_msg pmsg = nomsg;

/* Always initialize variables, to NULL if string, to zero if int;
 * otherwise they may contain garbage, and an access to them may result
 * in a crash or some invalid data being read. However, non-initialized
 * variables are automatically initialized by the compiler */

short splash_screen = -1, welcome_message = -1, ext_cmd_ok = -1,
	show_hidden = -1, clear_screen = -1, shell_terminal = 0, pager = -1,
	no_log = 0, internal_cmd = 0, list_folders_first = -1,
	case_sensitive = -1, cd_lists_on_the_fly = -1, share_selbox = -1,
	recur_perm_error_flag = 0, is_sel = 0, sel_is_last = 0, print_msg = 0,
	long_view = -1, kbind_busy = 0, unicode = -1, dequoted = 0,
	home_ok = 1, config_ok = 1, trash_ok = 1, selfile_ok = 1, tips = -1,
	mime_match = 0, logs_enabled = -1, sort = -1, files_counter = -1,
	light_mode = -1, dir_indicator = -1, classify = -1, sort_switch = 0,
	sort_reverse = 0, autocd = -1, auto_open = -1;
	/* -1 means non-initialized or unset. Once initialized, these variables
	 * are always either zero or one */
/*	sel_no_sel=0 */
/* A short int accepts values from -32,768 to 32,767, and since all the
 * above variables will take -1, 0, and 1 as values, a short int is more
 * than enough. Now, since short takes 2 bytes of memory, using int for
 * these variables would be a waste of 2 bytes (since an int takes 4 bytes).
 * I can even use char (or signed char), which takes only 1 byte (8 bits)
 * and accepts negative numbers (-128 -> +127).
* NOTE: If the value passed to a variable is bigger than what the variable
* can hold, the result of a comparison with this variable will always be
* true */

int max_hist = -1, max_log = -1, dirhist_total_index = 0,
	dirhist_cur_index = 0, argc_bk = 0, max_path = -1, exit_code = 0,
	shell_is_interactive = 0, cont_bt = 0, sort_types = 7;

unsigned short term_cols = 0;

size_t user_home_len = 0, args_n = 0, sel_n = 0, trash_n = 0, msgs_n = 0,
	   prompt_cmds_n = 0, path_n = 0, current_hist_n = 0, usrvar_n = 0,
	   aliases_n = 0, longest = 0, files = 0, eln_len = 0;

struct termios shell_tmodes;
off_t total_sel_size = 0;
pid_t own_pid = 0;
struct usrvar_t *usr_var = (struct usrvar_t *)NULL;
struct param xargs;
char *user = (char *)NULL, *path = (char *)NULL,
	**sel_elements = (char **)NULL, *qc = (char *)NULL,
	*sel_file_user = (char *)NULL, **paths = (char **)NULL,
	**bin_commands = (char **)NULL, **history = (char **)NULL,
	*file_cmd_path = (char *)NULL, **braces = (char **)NULL,
	*alt_profile = (char *)NULL, **prompt_cmds = (char **)NULL,
	**aliases = (char **)NULL, **argv_bk = (char **)NULL,
	*user_home = (char *)NULL, **messages = (char **)NULL,
	*msg = (char *)NULL, *CONFIG_DIR = (char *)NULL,
	*CONFIG_FILE = (char *)NULL, *BM_FILE = (char *)NULL,
	hostname[HOST_NAME_MAX] = "", *LOG_FILE = (char *)NULL,
	*LOG_FILE_TMP = (char *)NULL, *HIST_FILE = (char *)NULL,
	*MSG_LOG_FILE = (char *)NULL, *PROFILE_FILE = (char *)NULL,
	*TRASH_DIR = (char *)NULL, *TRASH_FILES_DIR = (char *)NULL,
	*TRASH_INFO_DIR = (char *)NULL, *sys_shell = (char *)NULL,
	**dirlist = (char **)NULL, **bookmark_names = (char **)NULL,
	*ls_colors_bk = (char *)NULL, *MIME_FILE = (char *)NULL,
	**profile_names = (char **)NULL, *encoded_prompt = (char *)NULL,
	*last_cmd = (char *)NULL, *term = (char *)NULL,
	*TMP_DIR = (char *)NULL, div_line_char = -1, *opener = (char *)NULL,
	**old_pwd = (char **)NULL;

	/* This is not a comprehensive list of commands. It only lists
	 * commands long version for TAB completion */
const char *INTERNAL_CMDS[] = { "alias", "open", "prop", "back", "forth",
		"move", "paste", "sel", "selbox", "desel", "refresh",
		"edit", "history", "hidden", "path", "help", "commands",
		"colors", "version", "splash", "folders first", "opener",
		"exit", "quit", "pager", "trash", "undel", "messages",
		"mountpoints", "bookmarks", "log", "untrash", "unicode",
		"profile", "shell", "mime", "sort", "tips", "autocd",
		"auto-open", NULL };

#define DEFAULT_PROMPT "\\A \\u:\\H \\[\\e[00;36m\\]\\w\\n\\[\\e[0m\\]\
\\z\\[\\e[0;34m\\] \\$\\[\\e[0m\\] "

#define DEFAULT_TERM_CMD "xterm -e"

#define FALLBACK_SHELL "/bin/sh"

#define MAX_COLOR 46
/* 46 == \x1b[00;38;02;000;000;000;00;48;02;000;000;000m\0 (24bit, RGB
 * true color format including foreground and background colors, the SGR
 * (Select Graphic Rendition) parameter, and, of course, the terminating
 * null byte.

 * To store all the 29 color variables I use, with 46 bytes each, I need
 * a total of 1,3Kb. It's not much but it could be less if I'd use
 * dynamically allocated arrays for them */

/* Some interface colors */
char text_color[MAX_COLOR + 2] = "", eln_color[MAX_COLOR] = "",
	 default_color[MAX_COLOR] = "", dir_count_color[MAX_COLOR] = "",
	 div_line_color[MAX_COLOR] = "", welcome_msg_color[MAX_COLOR] = "";
/* text_color is used in the command line, and readline needs to know
 * that color codes are not printable chars. For this we need to add
 * "\001" at the beginning of the color code and "\002" at the end. We
 * need thereby 2 more bytes */

/* Filetype colors */
char di_c[MAX_COLOR] = "", /* Directory */
	nd_c[MAX_COLOR] = "", /* No read directory */
	ed_c[MAX_COLOR] = "", /* Empty dir */
	ne_c[MAX_COLOR] = "", /* No read empty dir */
	fi_c[MAX_COLOR] = "", /* Reg file */
	ef_c[MAX_COLOR] = "", /* Empty reg file */
	nf_c[MAX_COLOR] = "", /* No read file */
	ln_c[MAX_COLOR] = "",	/* Symlink */
	or_c[MAX_COLOR] = "", /* Broken symlink */
	pi_c[MAX_COLOR] = "", /* FIFO, pipe */
	so_c[MAX_COLOR] = "", /* Socket */
	bd_c[MAX_COLOR] = "", /* Block device */
	cd_c[MAX_COLOR] = "",	/* Char device */
	su_c[MAX_COLOR] = "", /* SUID file */
	sg_c[MAX_COLOR] = "", /* SGID file */
	tw_c[MAX_COLOR] = "", /* Sticky other writable */
	st_c[MAX_COLOR] = "", /* Sticky (not ow)*/
	ow_c[MAX_COLOR] = "", /* Other writable */
	ex_c[MAX_COLOR] = "", /* Executable */
	ee_c[MAX_COLOR] = "", /* Empty executable */
	ca_c[MAX_COLOR] = "", /* Cap file */
	no_c[MAX_COLOR] = ""; /* Unknown */


				/**
				 * #############################
				 * #           MAIN            #
				 * #############################
				 * */

int
main(int argc, char **argv)
{

	/* #########################################################
	 * #			0) INITIAL CONDITIONS					   #
	 * #########################################################*/

	/* Though this program might perfectly work on other architectures,
	 * I just didn't test anything beyond x86 and ARM */
	#if !defined __x86_64__ && !defined __i386__ && !defined __ARM_ARCH
		fprintf(stderr, "Unsupported CPU architecture\n");
		exit(EXIT_FAILURE);
	#endif /* __x86_64__ */

	/* Though this program might perfectly work on other OSes, especially
	 * Unices, I just didn't make any test */
	#if !defined __linux__  && !defined linux && !defined __linux \
	&& !defined __gnu_linux__ && !defined __FreeBSD__
		fprintf(stderr, "%s: Unsupported operating system\n",
				PROGRAM_NAME);
		exit(EXIT_FAILURE);
	#endif

	/* What about unices like BSD, Android and even MacOS?
	* unix || __unix || __unix__
	* __NetBSD__, __OpenBSD__, __bdsi__, __DragonFly__
	* __APPLE__ && __MACH__
	* sun || __sun
	* __ANDROID__
	* __CYGWIN__
	* MSDOS, _MSDOS, __MSDOS__, __DOS__, _WIN16, _WIN32, _WIN64 */

/*	#ifndef __GLIBC__
		fprintf(stderr, "%s: GNU C libraries not found\n", PROGRAM_NAME);
		exit(EXIT_FAILURE);
	#endif */

/*	printf("Linux: %d\n", LINUX_VERSION_CODE);
	printf("GLIBC: %d.%d\n", __GLIBC__, __GLIBC_MINOR__);

	#ifndef _GNU_SOURCE
		puts("Not GNU source");
		printf("POSIX: %ld\n", _POSIX_C_SOURCE);
	#endif

	# ifndef _POSIX_C_SOURCE
		puts("Not POSIX: Using GNU extensions");
	#endif */


				/* #################################
				 * #     1) INITIALIZATION		   #
				 * #  Basic config and variables   #
				 * #################################*/

	/* If running the program locally, that is, not from a path in PATH,
	 * remove the leading "./" to get the correct program invocation
	 * name */
	if (*argv[0] == '.' && *(argv[0] + 1) == '/')
		argv[0] += 2;

	/* Use the locale specified by the environment. By default (ANSI C),
	 * all programs start in the standard 'C' (== 'POSIX') locale. This
	 * function makes the program portable to all locales (See man (3)
	 * setlocale). It affects characters handling functions like
	 * toupper(), tolower(), alphasort() (since it uses strcoll()), and
	 * strcoll() itself, among others. Here, it's important to correctly
	 * sort filenames according to the rules specified by the current
	 * locale. If the function fails, the default locale ("C") is not
	 * changed */
	setlocale(LC_ALL, "");
	/* To query the current locale, use NULL as second argument to
	 * setlocale(): printf("Locale: %s\n", setlocale(LC_CTYPE, NULL)); */

	/* If the locale isn't English, set 'unicode' to true to correctly
	 * list (sort and padding) filenames containing non-7bit ASCII chars
	 * like accents, tildes, umlauts, non-latin letters, and so on */
	if (strncmp(getenv("LANG"), "en", 2) != 0)
		unicode = 1;

	/* Initialize gettext() for translations */
	bindtextdomain("clifm", "/usr/share/locale");
	textdomain("clifm");

	/* Store external arguments to be able to rerun external_arguments()
	 * in case the user edits the config file, in which case the program
	 * must rerun init_config(), get_aliases(), get_prompt_cmds(), and
	 * then external_arguments() */
	argc_bk = argc;
	argv_bk = (char **)xnmalloc((size_t)argc, sizeof(char *));
	register size_t i = 0;
	for (i = 0; i < (size_t)argc; i++) {
		argv_bk[i] = (char *)xnmalloc(strlen(argv[i]) + 1, sizeof(char));
		strcpy(argv_bk[i], argv[i]);
	}

	/* Register the function to be called at normal exit, either via
	 * exit() or main's return. The registered function will not be
	 * executed when abnormally exiting the program, eg., via the KILL
	 * signal */
	atexit(free_stuff);

	/* Get user's home directory */
	user_home = get_user_home();
	if (!user_home || access(user_home, W_OK) == -1) {
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
	}
	else
		user_home_len = strlen(user_home);

	/* Get user name */
	user = get_user();

	if (!user) {
		user = (char *)xnmalloc(4, sizeof(char));
		strcpy(user, "???\0");
		_err('w', PRINT_PROMPT, _("%s: Error getting username\n"), 
			 PROGRAM_NAME);
	}

	if (geteuid() == 0)
		flags |= ROOT_USR;

	/* Running in X? */
	#if __linux__
	if (getenv("DISPLAY") != NULL
	&& strncmp(getenv("TERM"), "linux", 5) != 0)
	#else
	if (getenv("DISPLAY") != NULL)
	#endif
		flags |= GRAPHICAL;

	/* ##### READLINE ##### */

	initialize_readline();
	/* Copy the list of quote chars to a global variable to be used
	 * later by some of the program functions like split_str(),
	 * my_rl_quote(), is_quote_char(), and my_rl_dequote() */
	qc = (char *)xcalloc(strlen(rl_filename_quote_characters) + 1,
						 sizeof(char));
	strcpy(qc, rl_filename_quote_characters);

	/* Get paths from PATH environment variable. These paths will be
	 * used later by get_path_programs (for the autocomplete function)
	 * and get_cmd_path() */
	path_n = (size_t)get_path_env();

	/* Manage external arguments, but only if any: argc == 1 equates to
	 * no argument, since this '1' is just the program invokation name.
	 * External arguments will override initialization values
	 * (init_config) */

	/* Set all external arguments flags to uninitialized state */
	xargs.splash = xargs.hidden = xargs.longview = xargs.ext = -1;
	xargs.ffirst = xargs.sensitive = xargs.unicode = xargs.pager = -1;
	xargs.path = xargs.cdauto = xargs.light = xargs.sort = -1;

	if (argc > 1)
		external_arguments(argc, argv);
		/* external_arguments is executed before init_config because, if
		 * specified (-P option), it sets the value of alt_profile, which
		 * is then checked by init_config */

	/* Initialize program paths and files, set options from the config
	 * file, if they were not already set via external arguments, and
	 * load sel elements, if any. All these configurations are made per
	 * user basis */
	init_config();

	if (!config_ok)
		set_default_options();

	/* Check whether we have a working shell */
	if (access(sys_shell, X_OK) == -1) {
		_err('w', PRINT_PROMPT, _("%s: %s: System shell not found. "
			 "Please edit the configuration file to specify a working "
			 "shell.\n"), PROGRAM_NAME, sys_shell);
	}

	get_aliases();
	get_prompt_cmds();

	get_sel_files();

	if (trash_ok) {
		trash_n = count_dir(TRASH_FILES_DIR);
		if (trash_n <= 2)
			trash_n = 0;
	}

	/* Get hostname */
	if (gethostname(hostname, sizeof(hostname)) == -1) {
		strcpy(hostname, "???");
		_err('w', PRINT_PROMPT, _("%s: Error getting hostname\n"),
			 PROGRAM_NAME);
	}

	/* Initialize the shell */
	init_shell();

	exec_profile();

	if (config_ok) {

		/* Limit the log files size */
		check_log_file_size(LOG_FILE);
		check_log_file_size(MSG_LOG_FILE);

		/* Get history */
		struct stat file_attrib;
		if (stat(HIST_FILE, &file_attrib) == 0
		&& file_attrib.st_size != 0) {
		/* If the size condition is not included, and in case of a zero
		 * size file, read_history() gives malloc errors */
			/* Recover history from the history file */
			read_history(HIST_FILE); /* This line adds more leaks to
			readline */
			/* Limit the size of the history file to max_hist lines */
			history_truncate_file(HIST_FILE, max_hist);
		}
		else { /* If the history file doesn't exist, create it */
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
	file_cmd_check();

	if (splash_screen) {
		splash();
		splash_screen = 0;
		CLEAR;
	}

	/* If path was not set (neither in the config file nor via command
	 * line, that is, as external argument), set the default (CWD), and
	 * if CWD is not set, use the user's home directory, and if the home
	 * cannot be found either, try the root directory, and if there's no
	 * access to the root dir either, exit. 
	 * Bear in mind that if you launch CliFM through a terminal emulator, 
	 * say xterm (xterm -e clifm), xterm will run a shell, say bash, and 
	 * the shell will read its config file. Now, if this config file
	 * changes the CWD, this will be the CWD for CliFM */
	if (!path) {
		char cwd[PATH_MAX] = "";
		getcwd(cwd, sizeof(cwd));
		/* getenv() returns an address, so that pwd is this address and
		 * *pwd is its value */
		if (!*cwd || strlen(cwd) == 0) {
			if (user_home) {
				path = (char *)xcalloc(strlen(user_home)
									   + 1, sizeof(char));
				strcpy(path, user_home);
			}
			else {
				if (access("/", R_OK|X_OK) == -1) {
					fprintf(stderr, "%s: '/': %s\n", PROGRAM_NAME,
							strerror(errno));
					exit(EXIT_FAILURE);
				}
				else {
					path = (char *)xnmalloc(2, sizeof(char));
					strcpy(path, "/\0");
				}
			}
		}
		else {
			path = (char*)xcalloc(strlen(cwd) + 1, sizeof(char));
			strcpy(path, cwd);
		}
	}

	/* Make path the CWD */
	/* If chdir(path) fails, set path to cwd, list files and print the
	 * error message. If no access to CWD either, exit */
	if (chdir(path) == -1) {

		_err('e', PRINT_PROMPT, "%s: chdir: '%s': %s\n", PROGRAM_NAME,
			 path, strerror(errno));

		char cwd[PATH_MAX] = "";
		if (getcwd(cwd, sizeof(cwd)) == NULL) {
			_err(0, NOPRINT_PROMPT, _("%s: Fatal error! Failed retrieving "
				 "current working directory\n"), PROGRAM_NAME);
			exit(EXIT_FAILURE);
		}

		if (path)
			free(path);
		path = (char *)xcalloc(strlen(cwd) + 1, sizeof(char));
		strcpy(path, cwd);
		add_to_dirhist(path);
		if (cd_lists_on_the_fly)
			list_dir();
	}
	else {
		add_to_dirhist(path);
		if (cd_lists_on_the_fly)
			list_dir();
	}

	/* Get the list of available applications in PATH to be used by my
	 * custom TAB-completion function */
	get_path_programs();

	get_bm_names();

	get_profile_names();


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
	while (1) { /* Or: for (;;) */

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
			exec_cmd(cmd); /* Execute command */
			for (i = 0; i <= args_n; i++)
				free(cmd[i]);
			free(cmd);
			cmd = (char **)NULL;
		}
	}

	return exit_code; /* Never reached */
}


			/** #################################
			 * #     FUNCTIONS DEFINITIONS     #
			 * ################################# */

int
create_iso(char *in_file, char *out_file)
{
	int exit_status = EXIT_SUCCESS;
	struct stat file_attrib;

	if (lstat(in_file, &file_attrib) == -1) {
		fprintf(stderr, "archiver: '%s': %s\n", in_file,
				strerror(errno));
		return EXIT_FAILURE;
	}

	/* If IN_FILE is a directory */
	if ((file_attrib.st_mode & S_IFMT) == S_IFDIR) {
		char *cmd[] = { "mkisofs", "-R", "-o", out_file, in_file,
						NULL };

		if (launch_execve(cmd, FOREGROUND) != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;
	}

	/* If IN_FILE is a block device */
	else if ((file_attrib.st_mode & S_IFMT) == S_IFBLK) {

		char *if_option = (char *)xnmalloc(strlen(in_file) + 4,
										   sizeof(char));
		sprintf(if_option, "if=%s", in_file);

		char *of_option = (char *)xnmalloc(strlen(out_file) + 4,
										   sizeof(char));
		sprintf(of_option, "of=%s", out_file);

		char *cmd[] = { "sudo", "dd", if_option, of_option, "bs=64k",
 						"conv=noerror,sync", "status=progress",
 						NULL };

		if (launch_execve(cmd, FOREGROUND) != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;

		free(if_option);
		free(of_option);
	}

	else {
		fprintf(stderr, "archiver: '%s': Invalid file format\nFile "
				"should be either a directory or a block device\n",
				in_file);
		return EXIT_FAILURE;
	}

	return exit_status;
}

int
handle_iso(char *file)
{
	int exit_status = EXIT_SUCCESS;

	/* Use 7z to
	 * list (l)
	 * extract (e)
	 * extrat to dir (x -oDIR FILE)
	 * test (t) */

	printf(_("%s[e]%sxtract %s[E]%sxtract-to-dir %s[l]%sist "
		  "%s[t]%stest %s[m]%sount %s[q]%suit\n"), bold, NC, bold,
		  NC, bold, NC, bold, NC, bold, NC, bold, NC);

	char *operation = (char *)NULL;
	char sel_op = 0;

	while (!operation) {
		operation = rl_no_hist(_("Operation: "));

		if (!operation)
			continue;

		if (operation && (!operation[0] || operation[1] != '\0')) {
			free(operation);
			operation = (char *)NULL;
			continue;
		}

		switch(*operation) {
			case 'e':
			case 'E':
			case 'l':
			case 'm':
			case 't':
				sel_op = *operation;
				free(operation);
			break;

			case 'q':
				free(operation);
				return EXIT_SUCCESS;

			default:
				free(operation);
				operation = (char *)NULL;
			break;
		}

		if (sel_op)
			break;
	}

	char *ret = strchr(file, '\\');
	if (ret) {
		char *deq_file = dequote_str(file, 0);
		if (deq_file) {
			strcpy(file, deq_file);
			free(deq_file);
		}
		ret = (char *)NULL;
	}

	switch(sel_op) {

		/* ########## EXTRACT #######*/
		case 'e': {
			/* 7z x -oDIR FILE (use FILE as DIR) */
			char *o_option = (char *)xnmalloc(strlen(file) + 7,
										sizeof(char));
			sprintf(o_option, "-o%s.dir", file);

			/* Construct and execute cmd */
			char *cmd[] = { "7z", "x", o_option, file, NULL };
			if (launch_execve(cmd, FOREGROUND) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;

			free(o_option);
		}
		break;

		/* ########## EXTRACT TO DIR ####### */
		case 'E': {
			/* 7z x -oDIR FILE (ask for DIR) */
			char *ext_path = (char *)NULL;

			while (!ext_path) {
				ext_path = rl_no_hist(_("Extraction path: "));

				if (!ext_path)
					continue;

				if (ext_path && !*ext_path) {
					free(ext_path);
					ext_path = (char *)NULL;
					continue;
				}
			}

			char *o_option = (char *)xnmalloc(strlen(ext_path) + 3,
										sizeof(char));
			sprintf(o_option, "-o%s", ext_path);

			/* Construct and execute cmd */
			char *cmd[] = { "7z", "x", o_option, file, NULL };

			if (launch_execve(cmd, FOREGROUND) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;

			free(ext_path);
			free(o_option);
			ext_path = (char *)NULL;
		}
		break;

		/* ########## LIST ####### */
		case 'l': {
			/* 7z l FILE */
			char *cmd[] = { "7z", "l", file, NULL };

			if (launch_execve(cmd, FOREGROUND) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;
			}
		break;

		/* ########## MOUNT ####### */

		case 'm': {
			/* Create mountpoint */
			char *mountpoint = (char *)NULL;
			mountpoint = (char *)xnmalloc(strlen(CONFIG_DIR)
										  + strlen(file) + 9,
										  sizeof(char));

			sprintf(mountpoint, "%s/mounts/%s", CONFIG_DIR, file);

			char *dir_cmd[] = { "mkdir", "-pm700", mountpoint, NULL };

			if (launch_execve(dir_cmd, FOREGROUND) != EXIT_SUCCESS) {
				free(mountpoint);
				return EXIT_FAILURE;
			}

			/* Construct and execute cmd */
			char *cmd[] = { "sudo", "mount", "-o", "loop", file,
							mountpoint, NULL };

			if (launch_execve(cmd, FOREGROUND) != EXIT_SUCCESS) {
				free(mountpoint);
				return EXIT_FAILURE;
			}

			/* List content of mountpoint */
			if (chdir(mountpoint) == -1) {
				fprintf(stderr, "archiver: %s: %s\n", mountpoint,
						strerror(errno));
				free(mountpoint);
				return EXIT_FAILURE;
			}

			free(path);
			path = (char *)xcalloc(strlen(mountpoint) + 1,
								   sizeof(char));
			strcpy(path, mountpoint);

			if (cd_lists_on_the_fly) {
				while(files--)
					free(dirlist[files]);
				if (list_dir() != EXIT_SUCCESS)
					exit_status = EXIT_FAILURE;
				add_to_dirhist(path);
			}
			else
				printf("%s: Successfully mounted on %s\n", file,
					   mountpoint);

			free(mountpoint);
		}
		break;

		/* ########## TEST #######*/
		case 't': {
			/* 7z t FILE */
			char *cmd[] = { "7z", "t", file, NULL };

			if (launch_execve(cmd, FOREGROUND) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;
			}
		break;
	}

	return exit_status;
}

int
check_iso(char *file)
/* Run the 'file' command on FILE and look for "ISO 9660" and
 * string in its output. Returns zero if found, one if not, and -1
 * in case of error */
{
	if (!file || !*file) {
		fputs(_("Error opening temporary file\n"), stderr);
		return -1;
	}

	char ISO_TMP_FILE[PATH_MAX] = "";
	sprintf(ISO_TMP_FILE, "%s/archiver_tmp", TMP_DIR);

	if (access(ISO_TMP_FILE, F_OK) == 0)
		remove(ISO_TMP_FILE);

	FILE *file_fp = fopen(ISO_TMP_FILE, "w");

	if (!file_fp) {
		fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME,
				ISO_TMP_FILE, strerror(errno));
		return -1;
	}

	FILE *file_fp_err = fopen("/dev/null", "w");

	if (!file_fp_err) {
		fprintf(stderr, "%s: '/dev/null': %s\n", PROGRAM_NAME,
				strerror(errno));
		fclose(file_fp);
		return -1;
	}

	int stdout_bk = dup(STDOUT_FILENO); /* Store original stdout */
	int stderr_bk = dup(STDERR_FILENO); /* Store original stderr */

	/* Redirect stdout to the desired file */
	if (dup2(fileno(file_fp), STDOUT_FILENO) == -1 ) {
		fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
		fclose(file_fp);
		fclose(file_fp_err);
		return -1;
	}

	/* Redirect stderr to /dev/null */
	if (dup2(fileno(file_fp_err), STDERR_FILENO) == -1) {
		fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
		fclose(file_fp);
		fclose(file_fp_err);
		return -1;
	}

	fclose(file_fp);
	fclose(file_fp_err);

	char *cmd[] = { "file", "-b", file, NULL };
	int retval = launch_execve(cmd, FOREGROUND);

	dup2(stdout_bk, STDOUT_FILENO); /* Restore original stdout */
	dup2(stderr_bk, STDERR_FILENO); /* Restore original stderr */
	close(stdout_bk);
	close(stderr_bk);

	if (retval != EXIT_SUCCESS)
		return -1;

	int is_iso = 0;
	if (access(ISO_TMP_FILE, F_OK) == 0) {

		file_fp = fopen(ISO_TMP_FILE, "r");

		if (file_fp) {
			char line[255] = "";
			fgets(line, sizeof(line), file_fp);
			char *ret = strstr(line, "ISO 9660");
			if (ret)
				is_iso = 1;
			fclose(file_fp);
		}
		remove(ISO_TMP_FILE);
	}

	if (is_iso)
		return EXIT_SUCCESS;

	return EXIT_FAILURE;
}

void
print_tips(int all)
/* Print either all tips (if ALL == 1) or just a random one (ALL == 0) */
{
	const char *TIPS[] = {
		"Try the autocd and auto-open functions: run 'FILE' instead "
		"of 'open FILE' or 'cd FILE'",
		"Add a new entry to the mimelist file with 'mm edit'",
		"Do not forget to take a look at the manpage",
		"If need more speed, try the light mode (A-y)",
		"The Selection Box is shared among different instances of CliFM",
		"Select files here and there with the 's' command",
		"Use wildcards with the 's' command: 's *.c'",
		"ELN's and the 'sel' keyword work for shell commands as well: "
		"'file 1 sel'",
		"Press TAB to automatically expand an ELN: 'o 2' -> TAB -> "
		"'o FILENAME'",
		"Easily copy everything in CWD into another directory: 's * "
		"&& c sel ELN/DIR'",
		"Use ranges (ELN-ELN) to easily move multiple files: 'm 3-12 "
		"ELN/DIR'",
		"Trash files with a simple 't ELN'",
		"Get mime information for a file: 'mm info ELN'",
		"If too many files are listed, try enabling the pager ('pg on')",
		"Once in the pager, go backwards pressing the keyboard shortcut "
		"provided by your terminal emulator",
		"Press 'q' to stop the pager",
		"Press 'A-l' to switch to long view mode",
		"Search for files using the slash command: '/*.png'",
		"Add a new bookmark by just entering 'bm ELN/FILE'",
		"Use c, l, m, md, and r instead of cp, ln, mv, mkdir, and rm",
		"Access a remote file system using the 'net' command",
		"Manage default associated applications with the 'mime' command",
		"Go back and forth in the directory history with 'A-j' and 'A-k'",
		"Open a new instance of CliFM with the 'x' command: 'x ELN/DIR'",
		"Send a command directly to the system shell with ';CMD'",
		"Run the last executed command by just running '!!'",
		"Import aliases from file using 'alias import FILE'",
		"List available aliases by running 'alias'",
		"Open and edit the configuration file with 'edit'",
		"Find a description for each CLiFM command by running 'cmd'",
		"Print the currently used color codes list by entering 'cc'",
		"Press 'A-i' to toggle hidden files on/off",
		"List mountpoints by pressing 'A-m'",
		"Allow the use of shell commands with the -x option: 'clifm -x'",
		"Go to the root directory by just pressing 'A-r'",
		"Go to the home directory by just pressing 'A-e'",
		"Press 'F10' to open and edit the configuration file",
		"Customize the starting using the -p option: 'clifm -p PATH'",
		"Use the 'o' command to open files and directories: 'o 12'",
		"Bypass the resource opener specifying an application: 'o 12 "
		"leafpad'",
		"Open a file and send it to the background running 'o 24 &'",
		"Create a custom prompt editing the configuration file",
		"Customize color codes using the configuration file",
		"Open the bookmarks manager by just pressing 'A-b'",
		"Chain commands using ; and &&: 's 2 7-10; r sel'",
		"Add emojis to the prompt by copying them to the Prompt line "
		"in the configuration file",
		"Create a new profile running 'pf add PROFILE' or 'clifm -P "
		"PROFILE'",
		"Switch profiles using 'pf set PROFILE'",
		"Delete a profile using 'pf del PROFILE'",
		"Copy selected files into CWD by just running 'v sel'",
		"Use 'p ELN' to print file properties for ELN",
		"Deselect all selected files by pressing 'A-d'",
		"Select all files in CWD by pressing 'A-a'",
		"Jump to the Selection Box by pressing 'A-s'",
		"Restore trashed files using the 'u' command",
		"Empty the trash bin running 't clear'",
		"Press A-f to toggle list-folders-first on/off",
		"Use the 'fc' command to disable the files counter",
		"Take a look at the splash screen with the 'splash' command",
		"Have some fun trying the 'bonus' command",
		"Launch the default system shell in CWD using ':' or ';'",
		"Use 'A-z' and 'A-x' to switch sorting methods",
		"Reverse sorting order using the 'rev' option: 'st rev'",
		"Compress and decompress files using the 'ac' and 'ad' "
		"commands respectivelly",
		"Rename multiple files at once with the bulk rename function: "
		"'br *.txt'",
		NULL
	};

	size_t tipsn = (sizeof(TIPS) / sizeof(TIPS[0])) - 1;

	if (all) {
		size_t i;
		for (i = 0; i < tipsn; i++)
			printf("%sTIP %zu%s: %s\n", bold, i, NC, TIPS[i]);

		return;
	}

	srand(time(NULL));
	printf("%sTIP%s: %s\n", bold, NC, TIPS[rand() % tipsn]);
}

int
is_compressed(char *file, int test_iso)
/* Run the 'file' command on FILE and look for "archive" and
 * "compressed" strings in its output. Returns zero if compressed,
 * one if not, and -1 in case of error.
 * test_iso is used to determine if ISO files should be checked as
 * well: this is the case when called from open_function() or
 * mime_open(), since both need to check compressed and ISOs as
 * well (and there is no need to run two functions (is_compressed and
 * check_iso), when we can run just one) */
{
	if (!file || !*file) {
		fputs(_("Error opening temporary file\n"), stderr);
		return -1;
	}

	char ARCHIVER_TMP_FILE[PATH_MAX] = "";
	sprintf(ARCHIVER_TMP_FILE, "%s/archiver_tmp", TMP_DIR);

	if (access(ARCHIVER_TMP_FILE, F_OK) == 0)
		remove(ARCHIVER_TMP_FILE);

	FILE *file_fp = fopen(ARCHIVER_TMP_FILE, "w");

	if (!file_fp) {
		fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME,
				ARCHIVER_TMP_FILE, strerror(errno));
		return -1;
	}

	FILE *file_fp_err = fopen("/dev/null", "w");

	if (!file_fp_err) {
		fprintf(stderr, "%s: '/dev/null': %s\n", PROGRAM_NAME,
				strerror(errno));
		fclose(file_fp);
		return -1;
	}

	int stdout_bk = dup(STDOUT_FILENO); /* Store original stdout */
	int stderr_bk = dup(STDERR_FILENO); /* Store original stderr */

	/* Redirect stdout to the desired file */
	if (dup2(fileno(file_fp), STDOUT_FILENO) == -1 ) {
		fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
		fclose(file_fp);
		fclose(file_fp_err);
		return -1;
	}

	/* Redirect stderr to /dev/null */
	if (dup2(fileno(file_fp_err), STDERR_FILENO) == -1) {
		fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
		fclose(file_fp);
		fclose(file_fp_err);
		return -1;
	}

	fclose(file_fp);
	fclose(file_fp_err);

	char *cmd[] = { "file", "-b", file, NULL };
	int retval = launch_execve(cmd, FOREGROUND);

	dup2(stdout_bk, STDOUT_FILENO); /* Restore original stdout */
	dup2(stderr_bk, STDERR_FILENO); /* Restore original stderr */
	close(stdout_bk);
	close(stderr_bk);

	if (retval != EXIT_SUCCESS)
		return -1;

	int compressed = 0;
	if (access(ARCHIVER_TMP_FILE, F_OK) == 0) {

		file_fp = fopen(ARCHIVER_TMP_FILE, "r");

		if (file_fp) {
			char line[255] = "";
			fgets(line, sizeof(line), file_fp);
			char *ret = strstr(line, "archive");
			if (ret)
				compressed = 1;
			else {
				ret = strstr(line, "compressed");
				if (ret)
					compressed = 1;
				else if (test_iso) {
					ret = strstr(line, "ISO 9660");
					if (ret)
						compressed = 1;
				}
			}

			fclose(file_fp);
		}

		remove(ARCHIVER_TMP_FILE);
	}

	if (compressed)
		return EXIT_SUCCESS;

	return EXIT_FAILURE;
}

int
zstandard(char *in_file, char *out_file, char mode, char op)
/* If MODE is 'c', compress IN_FILE producing a zstandard compressed
 * file named OUT_FILE. If MODE is 'd', extract, test or get
 * information about IN_FILE. OP is used only for the 'd' mode: it
 * tells if we have one or multiple file. Returns zero on success and
 * one on error */
{
	int exit_status = EXIT_SUCCESS;

	char *deq_file = dequote_str(in_file, 0);

	if (!deq_file) {
		fprintf(stderr, _("archiver: '%s': Error dequoting file\n"),
				in_file);
		return EXIT_FAILURE;
	}

	if (mode == 'c') {

		if (out_file) {
			char *cmd[] = { "zstd", "-zo", out_file, deq_file, NULL };
			if (launch_execve(cmd, FOREGROUND) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;
		}

		else {
			char *cmd[] = { "zstd", "-z", deq_file, NULL };
			if (launch_execve(cmd, FOREGROUND) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;
		}

		free(deq_file);
		return exit_status;
	}

	/* mode == 'd' */

	/* op is non-zero when multiple files, including at least one
	 * zst file, are passed to the archiver function */
	if (op != 0) {
		char option[3] = "";

		switch(op) {
			case 'e': strcpy(option, "-d"); break;
			case 't': strcpy(option, "-t"); break;
			case 'i': strcpy(option, "-l"); break;
		}

		char *cmd[] = { "zstd", option, deq_file, NULL };

		exit_status = launch_execve(cmd, FOREGROUND);

		free(deq_file);

		if (exit_status != EXIT_SUCCESS)
			return EXIT_FAILURE;

		return EXIT_SUCCESS;
	}


	printf(_("%s[e]%sxtract %s[t]%sest %s[i]%snfo %s[q]%suit\n"),
		   bold, NC, bold, NC, bold, NC, bold, NC);

	char *operation = (char *)NULL;

	while (!operation) {
		operation = rl_no_hist(_("Operation: "));

		if (!operation)
			continue;

		if (operation && (!operation[0] || operation[1] != '\0')) {
			free(operation);
			operation = (char *)NULL;
			continue;
		}

		switch(*operation) {
			case 'e': {
				char *cmd[] = { "zstd", "-d", deq_file, NULL };
				if (launch_execve(cmd, FOREGROUND) != EXIT_SUCCESS)
					exit_status = EXIT_FAILURE;
			}
			break;

			case 't': {
				char *cmd[] = { "zstd", "-t", deq_file, NULL };
				if (launch_execve(cmd, FOREGROUND) != EXIT_SUCCESS)
					exit_status = EXIT_FAILURE;
			}
			break;

			case 'i': {
				char *cmd[] = { "zstd", "-l", deq_file, NULL };
				if (launch_execve(cmd, FOREGROUND) != EXIT_SUCCESS)
					exit_status = EXIT_FAILURE;
			}
			break;

			case 'q':
				free(operation);
				free(deq_file);
				return EXIT_SUCCESS;

			default:
				free(operation);
				operation = (char *)NULL;
			break;
		}
	}

	free(operation);
	free(deq_file);

	return exit_status;
}

int
archiver(char **args, char mode)
/* Handle archives and/or compressed files (ARGS) according to MODE:
 * 'c' for archiving/compression, and 'd' for dearchiving/decompression
 * (including listing, extracting, repacking, and mounting). Returns
 * zero on success and one on error. Depends on 'zstd' for Zdtandard
 * files 'atool' and 'archivemount' for the remaining types. */
{
	size_t i;
	int exit_status = EXIT_SUCCESS;

	if (!args[1])
		return EXIT_FAILURE;

	if (mode == 'c') {

			/* ##################################
			 * #        1 - COMPRESSION		    #
			 * ##################################*/

		/* Get archive name/type */

		puts(_("Use extension to specify archive/compression type.\n"
			   "Defaults to .tar.gz"));
		char *name = (char *)NULL;
		while (!name) {
			name = rl_no_hist(_("Filename ('q' to quit): "));

			if (!name)
				continue;

			if (!*name) {
				free(name);
				name = (char *)NULL;
				continue;
			}

			if (*name == 'q' && name[1] == 0x00) {
				free(name);
				return EXIT_SUCCESS;
			}
		}

				/* ##########################
				 * #		ZSTANDARD		#
				 * ########################## */

		char *ret = strrchr(name, '.');
		if (strcmp(ret, ".zst") == 0) {

			/* Multiple files */
			if (args[2]) {

				printf(_("\n%sNOTE%s: Zstandard does not support "
					   "compression of multiple files into one single "
					   "compressed file. Files will be compressed rather "
					   "into multiple compressed files using original "
					   "filenames\n"), bold, NC);

				for (i = 1; args[i]; i++) {
					if (zstandard(args[i], NULL, 'c', 0)
					!= EXIT_SUCCESS)
						exit_status = EXIT_FAILURE;
				}
			}

			/* Only one file */
			else
				exit_status = zstandard(args[1], name, 'c', 0);

			free(name);

			return exit_status;
		}


				/* ##########################
				 * #		ISO 9660		#
				 * ########################## */

		if (strcmp(ret, ".iso") == 0) {
			exit_status = create_iso(args[1], name);
			free(name);
			return exit_status;
		}


				/* ##########################
				 * #		  OTHERS		#
				 * ########################## */


		/* Escape the string, if needed */
		char *esc_name = escape_str(name);
		free(name);

		if (!esc_name) {
			fprintf(stderr, _("archiver: '%s': Error escaping "
					"string\n"), name);
			return EXIT_FAILURE;
		}

		/* Construct the command */
		char *cmd = (char *)NULL;
		char *ext_ok = strchr(esc_name, '.');
		size_t cmd_len = strlen(esc_name) + 10 + ((!ext_ok) ? 8 : 0);

		cmd = (char *)xcalloc(cmd_len, sizeof(char));

		/* If name has no extension, add the default */
		sprintf(cmd, "atool -a %s%s", esc_name, (!ext_ok)
									  ? ".tar.gz" : "");

		for (i = 1; args[i]; i++) {
			cmd_len += strlen(args[i]) + 1;
			cmd = (char *)xrealloc(cmd, (cmd_len + 1) * sizeof(char));
			strcat(cmd, " ");
			strcat(cmd, args[i]);
		}

		if (launch_execle(cmd) != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;

		free(cmd);
		free(esc_name);

		return exit_status;
	}


	/* mode == 'd' */

			/* ##################################
			 * #      2 - DECOMPRESSION		    #
			 * ##################################*/

	/* Exit if at least one non-compressed file is found */
	for (i = 1; args[i]; i++) {
		char *deq = (char *)NULL;

		if (strchr(args[i], '\\'))
			deq = dequote_str(args[i], 0);

		if (is_compressed((deq) ? deq : args[i], 1) != 0) {
			fprintf(stderr, _("archiver: '%s': Not an "
					"archive/compressed file\n"), args[i]);
			if (deq)
				free(deq);

			return EXIT_FAILURE;
		}

		if (deq)
			free(deq);
	}


				/* ##########################
				 * #		ISO 9660		#
				 * ########################## */

	char *ret = strrchr(args[1], '.');
	if ((ret && strcmp(ret, ".iso") == 0)
	|| check_iso(args[1]) == 0) {
		return handle_iso(args[1]);
	}

				/* ##########################
				 * #		ZSTANDARD		#
				 * ########################## */

	/* Check if we have at least one Zstandard file */

	int zst_index = -1;
	size_t files_num = 0;

	for (i = 1; args[i]; i++) {
		files_num++;
		if (args[i][strlen(args[i]) -1] == 't') {
			char *retval = strrchr(args[i], '.');
			if (retval) {
				if (strcmp(retval, ".zst") == 0)
					zst_index = i;
			}
		}
	}

	if (zst_index != -1) {

		/* Multiple files */
		if (files_num > 1) {

			printf(_("%sNOTE%s: Using Zstandard\n"), bold, NC);
			printf(_("%s[e]%sxtract %s[t]%sest %s[i]%snfo %s[q]%suit\n"),
				   bold, NC, bold, NC, bold, NC, bold, NC);

			char *operation = (char *)NULL;
			char sel_op = 0;
			while(!operation) {
				operation = rl_no_hist(_("Operation: "));

				if (!operation)
					continue;

				if (operation && (!operation[0]
				|| operation[1] != '\0')) {
					free(operation);
					operation = (char *)NULL;
					continue;
				}

				switch(*operation) {
					case 'e':
					case 't':
					case 'i':
						sel_op = *operation;
					break;

					case 'q':
						free(operation);
						return EXIT_SUCCESS;

					default:
						free(operation);
						operation = (char *)NULL;
					break;
				}
			}

			for (i = 1; args[i]; i++) {
				if (zstandard(args[i], NULL, 'd', sel_op)
				!= EXIT_SUCCESS)
					exit_status = EXIT_FAILURE;
			}

			free(operation);
			return exit_status;
		}

		/* Just one file */
		else {
			if (zstandard(args[zst_index], NULL, 'd', 0) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;

			return exit_status;
		}
	}


				/* ##########################
				 * #		  OTHERS		#
				 * ########################## */

	/* 1) Get operation to be performed
	 * ################################ */

	printf(_("%s[e]%sxtract %s[E]%sxtract-to-dir %s[l]%sist "
		  "%s[m]%sount %s[r]%sepack %s[q]%suit\n"), bold, NC, bold,
		  NC, bold, NC, bold, NC, bold, NC, bold, NC);

	char *operation = (char *)NULL;
	char sel_op = 0;

	while (!operation) {
		operation = rl_no_hist(_("Operation: "));

		if (!operation)
			continue;

		if (operation && (!operation[0] || operation[1] != '\0')) {
			free(operation);
			operation = (char *)NULL;
			continue;
		}

		switch(*operation) {
			case 'e':
			case 'E':
			case 'l':
			case 'm':
			case 'r':
				sel_op = *operation;
				free(operation);
			break;

			case 'q':
				free(operation);
				return EXIT_SUCCESS;

			default:
				free(operation);
				operation = (char *)NULL;
			break;
		}

		if (sel_op)
			break;
	}

	/* 2) Prepare files based on operation
	 * #################################### */

	char *dec_files = (char *)NULL;

	switch(sel_op) {
		case 'e':
		case 'r': {

			/* Store all filenames into one single variable */
			size_t len = 1;
			dec_files = (char *)xnmalloc(len, sizeof(char));
			*dec_files = 0x00;

			for (i = 1; args[i]; i++) {

				len += strlen(args[i]) + 1;
				dec_files = (char *)xrealloc(dec_files, (len + 1)
											 * sizeof(char));
				strcat(dec_files, " ");
				strcat(dec_files, args[i]);
			}
		}
		break;

		case 'E':
		case 'l':
		case 'm': {

			/* These operation won't be executed via the system shell,
			 * so that we need to deescape files if necessary */
			for (i = 1; args[i]; i++) {

				 if (strchr(args[i], '\\')) {
					char *deq_name = dequote_str(args[i], 0);

					if (!deq_name) {
						fprintf(stderr, _("archiver: '%s': Error "
								"dequoting filename\n"), args[i]);
						return EXIT_FAILURE;
					}

					strcpy(args[i], deq_name);
					free(deq_name);
					deq_name = (char *)NULL;
				}
			}
		}
		break;
	}

	/* 3) Construct and run the corresponding commands
	 * ############################################### */

	switch(sel_op) {

			/* ########## EXTRACT ############## */

		case 'e': {
			char *cmd = (char *)NULL;
			cmd = (char *)xnmalloc(strlen(dec_files) + 13,
								   sizeof(char));

			sprintf(cmd, "atool -x -e %s", dec_files);

			if (launch_execle(cmd) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;

			free(cmd);
			free(dec_files);
		}
		break;

		/* ########## EXTRACT TO DIR ############## */

		case 'E':
			for (i = 1; args[i]; i++) {

				/* Ask for extraction path */
				printf(_("%sFile%s: %s\n"), bold, NC, args[i]);

				char *ext_path = (char *)NULL;

				while (!ext_path) {
					ext_path = rl_no_hist(_("Extraction path: "));

					if (!ext_path)
						continue;

					if (ext_path && !*ext_path) {
						free(ext_path);
						ext_path = (char *)NULL;
						continue;
					}
				}

				/* Construct and execute cmd */
				char *cmd[] = { "atool", "-X", ext_path, args[i],
								NULL };

				if (launch_execve(cmd, FOREGROUND) != EXIT_SUCCESS)
					exit_status = EXIT_FAILURE;

				free(ext_path);
				ext_path = (char *)NULL;
			}
		break;

			/* ########## LIST ############## */

		case 'l':
			for (i = 1; args[i]; i++) {

				printf(_("%s%sFile%s: %s\n"), (i > 1) ? "\n" : "",
					   bold, NC, args[i]);

				char *cmd[] = { "atool", "-l", args[i], NULL };

				if (launch_execve(cmd, FOREGROUND) != EXIT_SUCCESS)
					exit_status = EXIT_FAILURE;
			}
		break;

			/* ########## MOUNT ############## */

		case 'm':
			for (i = 1; args[i]; i++) {

				/* Create mountpoint */
				char *mountpoint = (char *)NULL;
				mountpoint = (char *)xnmalloc(strlen(CONFIG_DIR)
									+ strlen(args[i]) + 9,
									sizeof(char));

				sprintf(mountpoint, "%s/mounts/%s", CONFIG_DIR,
						args[i]);

				char *dir_cmd[] = { "mkdir", "-pm700", mountpoint,
									NULL };

				if (launch_execve(dir_cmd, FOREGROUND) != EXIT_SUCCESS) {
					free(mountpoint);
					return EXIT_FAILURE;
				}

				/* Construct and execute cmd */
				char *cmd[] = { "archivemount", args[i], mountpoint,
								NULL };

				if (launch_execve(cmd, FOREGROUND) != EXIT_SUCCESS) {
					free(mountpoint);
					continue;
				}

				/* List content of mountpoint if there is only
				 * one archive */
				if (files_num > 1) {
					printf(_("%s%s%s: Succesfully mounted "
							"on %s\n"), bold, args[i], NC, mountpoint);
					free(mountpoint);
					continue;
				}

				if (chdir(mountpoint) == -1) {
					fprintf(stderr, "archiver: %s: %s\n", mountpoint,
							strerror(errno));
					free(mountpoint);
					return EXIT_FAILURE;
				}

				free(path);
				path = (char *)xcalloc(strlen(mountpoint) + 1,
									   sizeof(char));
				strcpy(path, mountpoint);
				free(mountpoint);

				if (cd_lists_on_the_fly) {
					while(files--)
						free(dirlist[files]);
					if (list_dir() != EXIT_SUCCESS)
						exit_status = EXIT_FAILURE;
					add_to_dirhist(path);
				}
			}
		break;

			/* ########## REPACK ############## */

		case 'r': {
			/* Ask for new archive/compression format */
			puts(_("Enter 'q' to quit"));

			char *format = (char *)NULL;
			while (!format) {
				format = rl_no_hist(_("New format "
									"(Ex: .tar.xz): "));
				if (!format)
					continue;

				if (!*format || (*format != '.' && *format != 'q')) {
					free(format);
					format = (char *)NULL;
					continue;
				}

				if (*format == 'q' && format[1] == 0x00) {
					free(format);
					free(dec_files);
					return EXIT_SUCCESS;
				}
			}

			/* Construct and execute cmd */
			char *cmd = (char *)NULL;
			cmd = (char *)xnmalloc(strlen(format) + strlen(dec_files)
								   + 16, sizeof(char));
			sprintf(cmd, "arepack -F %s -e %s", format, dec_files);

			if (launch_execle(cmd) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;

			free(format);
			free(dec_files);
			free(cmd);
		}
		break;
	}

	return exit_status;
}

int
bulk_rename(char **args)
/* Rename a bulk of files (ARGS) at once. Takes files to be renamed
 * as arguments, and returns zero on success and one on error. The
 * procedude is quite simple: filenames to be renamed are copied into
 * a temporary file, which is opened via the mime function and shown
 * to the user to modify it. Once the filenames have been modified and
 * saved, modifications are printed on the screen and the user is
 * asked whether to perform the actual bulk renaming (via mv) or not.
 * I took this bulk rename method, just because it is quite simple and
 * KISS, from the fff filemanager. So, thanks fff! */
{
	if (!args[1])
		return EXIT_FAILURE;

	int exit_status = EXIT_SUCCESS;

	char BULK_FILE[PATH_MAX] = "";
	sprintf(BULK_FILE, "%s/.bulk_rename", TMP_DIR);

	FILE *bulk_fp;

	bulk_fp = fopen(BULK_FILE, "w+");
	if (!bulk_fp) {
		_err('e', PRINT_PROMPT, "bulk: '%s': %s\n", BULK_FILE,
			 strerror(errno));
		return EXIT_FAILURE;
	}

	size_t i, arg_total = 0;

	/* Copy all files to be renamed to the bulk file */
	for (i = 1; args[i]; i++) {

		/* Dequote filename, if necessary */
		if (strchr(args[i], '\\')) {
			char *deq_file = dequote_str(args[i], 0);
			if (!deq_file) {
				fprintf(stderr, _("bulk: '%s': Error dequoting "
						"filename\n"), args[i]);
				continue;
			}
			strcpy(args[i], deq_file);
			free(deq_file);
		}

		fprintf(bulk_fp, "%s\n", args[i]);
	}

	arg_total = i;

	fclose(bulk_fp);

	/* Store the last modification time of the bulk file. This time
	 * will be later compared to the modification time of the same
	 * file after shown to the user */
	struct stat file_attrib;
	stat(BULK_FILE, &file_attrib);
	time_t mtime_bfr = file_attrib.st_mtime;

	/* The application opening the bulk file could print some stuff to
	 * stderr. Silence it */
	FILE *fp_out = fopen("/dev/null", "w");
	/* Store stderr current value */
	int stderr_bk = dup(STDERR_FILENO);

	/* Redirect stderr to /dev/null */
	dup2(fileno(fp_out), STDERR_FILENO);

	fclose(fp_out);

	/* Open the bulk file via the mime function */
	char *cmd[] = { "mm", BULK_FILE, NULL };
	mime_open(cmd);

	/* Restore stderr to previous value */
	dup2(stderr_bk, STDERR_FILENO);
	close(stderr_bk);

	/* Compare the new modification time to the stored one: if they
	 * match, nothing was modified */
	stat(BULK_FILE, &file_attrib);
	if (mtime_bfr == file_attrib.st_mtime) {
		puts(_("bulk: Nothing to do"));
		if (remove(BULK_FILE) == -1) {
			_err('e', PRINT_PROMPT, "%s: '%s': %s\n", PROGRAM_NAME,
				 BULK_FILE, strerror(errno));
			exit_status = EXIT_FAILURE;
		}
		return exit_status;
	}

	bulk_fp = fopen(BULK_FILE, "r");
	if (!bulk_fp) {
		_err('e', PRINT_PROMPT, "bulk: '%s': %s\n", BULK_FILE, 
			 strerror(errno));
		return EXIT_FAILURE;
	}

	/* Go back to the beginning of the bulk file */
	fseek(bulk_fp, 0, SEEK_SET);

	/* Make sure there are as many lines in the bulk file as files
	 * to be renamed */
	size_t file_total = 1;
	char tmp_line[256];
	while (fgets(tmp_line, sizeof(tmp_line), bulk_fp)) {
		file_total++;
	}

	if (arg_total != file_total) {
		fputs(_("bulk: Line mismatch in rename file\n"), stderr);
		fclose(bulk_fp);
		if (remove(BULK_FILE) == -1)
			_err('e', PRINT_PROMPT, "%s: '%s': %s\n", PROGRAM_NAME,
				 BULK_FILE, strerror(errno));
		return EXIT_FAILURE;
	}

	/* Go back to the beginning of the bulk file, again */
	fseek(bulk_fp, 0L, SEEK_SET);

	size_t line_size = 0;
	char *line = (char *)NULL;
	ssize_t line_len = 0;
	int modified = 0;

	i = 1;

	/* Print what would be done */
	while ((line_len = getline(&line, &line_size, bulk_fp)) > 0) {
		if (line[line_len - 1] == '\n')
			line[line_len - 1] = 0x00;
		if (strcmp(args[i], line) != 0) {
			printf("%s %s->%s %s\n", args[i], cyan, NC, line);
			modified++;
		}
		i++;
	}

	/* If no filename was modified */
	if (!modified) {
		puts(_("bulk: Nothing to do"));
		if (remove(BULK_FILE) == -1) {
			_err('e', PRINT_PROMPT, "%s: '%s': %s\n", PROGRAM_NAME,
				 BULK_FILE, strerror(errno));
			exit_status = EXIT_FAILURE;
		}
		free(line);
		fclose(bulk_fp);
		return exit_status;
	}

	/* Ask the user for confirmation */
	char *answer = (char *)NULL;
	while (!answer) {
		answer = readline(_("Continue? [y/N] "));

		if (strlen(answer) > 1) {
			free(answer);
			answer = (char *)NULL;
			continue;
		}

		switch(*answer) {
			case 'y':
			case 'Y': break;

			case 'n':
			case 'N':
			case '\0':
				free(answer);
				free(line);
				fclose(bulk_fp);
				return EXIT_SUCCESS;

			default:
				free(answer);
				answer = (char *)NULL;
				break;
		}
	}

	free(answer);

	/* Once again */
	fseek(bulk_fp, 0L, SEEK_SET);

	i = 1;

	/* Compose the mv commands and execute them */
	while ((line_len = getline(&line, &line_size, bulk_fp)) > 0) {

		if (line[line_len - 1] == '\n')
			line[line_len - 1] = 0x00;

		if (strcmp(args[i], line) != 0) {
			char *tmp_cmd[] = { "mv", args[i], line, NULL };

			if (launch_execve(tmp_cmd, FOREGROUND) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;
		}

		i++;
	}

	free(line);

	fclose(bulk_fp);

	if (remove(BULK_FILE) == -1) {
		_err('e', PRINT_PROMPT, "%s: '%s': %s\n", PROGRAM_NAME,
			 BULK_FILE, strerror(errno));
		exit_status = EXIT_FAILURE;
	}

	if (cd_lists_on_the_fly) {
		while (files--)
			free(dirlist[files]);
		if (list_dir() != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;
	}

	return exit_status;
}

void
print_sort_method(void)
{
	printf(_("%s->%s Sorted by: "), cyan, NC);
	switch(sort) {
		case 0: puts(_("none")); break;
		case 1: printf(_("name %s\n"), (sort_reverse) ? "[rev]" : "");
			break;
		case 2: printf(_("size %s\n"), (sort_reverse) ? "[rev]" : "");
			break;
		case 3: printf(_("atime %s\n"), (sort_reverse) ? "[rev]" : "");
			break;
		case 4:
		#if defined(HAVE_ST_BIRTHTIME) \
			|| defined(__BSD_VISIBLE) || defined(_STATX)
				printf(_("btime %s\n"), (sort_reverse) ? "[rev]" : "");
			#else
				printf(_("btime (not available: using 'ctime') %s\n"),
					   (sort_reverse) ? "[rev]" : "");
			#endif
			break;
		case 5: printf(_("ctime %s\n"), (sort_reverse) ? "[rev]" : "");
			break;
		case 6: printf(_("mtime %s\n"), (sort_reverse) ? "[rev]" : "");
			break;
		#if __FreeBSD__ || _BE_POSIX
		case 7: printf(_("version (not available: using 'name') %s\n"),
					   (sort_reverse) ? "[rev]" : "");
		#else
		case 7: printf(_("version %s\n"), (sort_reverse) ? "[rev]" : "");
		#endif
			break;
	}
}

int
sort_function(char **arg)
{
	int exit_status = EXIT_FAILURE;

	/* No argument: Just print current sorting method */
	if (!arg[1]) {
		printf(_("Sorting method: "));
		switch(sort) {
			case 0:
				printf(_("none %s\n"), (sort_reverse) ? "[rev]": "");
				break;
			case 1:
				printf(_("name %s\n"), (sort_reverse) ? "[rev]": "");
				break;
			case 2:
				printf(_("size %s\n"), (sort_reverse) ? "[rev]": "");
				break;
			case 3:
				printf(_("atime %s\n"), (sort_reverse) ? "[rev]": "");
				break;
			case 4:
			#if defined(HAVE_ST_BIRTHTIME) \
			|| defined(__BSD_VISIBLE) || defined(_STATX)
				printf(_("btime %s\n"), (sort_reverse) ? "[rev]": "");
			#else
				printf(_("ctime %s\n"), (sort_reverse) ? "[rev]": "");
			#endif
				break;
			case 5:
				printf(_("ctime %s\n"), (sort_reverse) ? "[rev]": "");
				break;
			case 6:
				printf(_("mtime %s\n"), (sort_reverse) ? "[rev]": "");
				break;
			case 7:
			#if __FreeBSD__ || _BE_POSIX
				printf(_("name %s\n"), (sort_reverse) ? "[rev]": "");
			#else
				printf(_("version %s\n"), (sort_reverse) ? "[rev]": "");
			#endif
				break;
		}
		return EXIT_SUCCESS;
	}

	/* Argument is alphanumerical string */
	else if (!is_number(arg[1])) {
		if (strcmp(arg[1], "rev") == 0) {

			if (sort_reverse)
				sort_reverse = 0;
			else
				sort_reverse = 1;

			if (cd_lists_on_the_fly) {
				/* sort_switch just tells list_dir() to print a line
				 * with the current sorting method at the end of the
				 * files list */
				sort_switch = 1;
				while (files--)
					free(dirlist[files]);
				exit_status = list_dir();
				sort_switch = 0;
			}

			return exit_status;
		}

		/* If arg1 is not a number and is not "rev", the fputs()
		 * above is executed */
	}

	/* Argument is a number */
	else {
		int int_arg = atoi(arg[1]);

		if (int_arg >= 0 && int_arg <= sort_types) {
			sort = int_arg;

			if (arg[2] && strcmp(arg[2], "rev") == 0) {
				if (sort_reverse)
					sort_reverse = 0;
				else
					sort_reverse = 1;
			}

			if (cd_lists_on_the_fly) {
				sort_switch = 1;
				while (files--)
					free(dirlist[files]);
				exit_status = list_dir();
				sort_switch = 0;
			}

			return exit_status;
		}
	}

	/* If arg1 is a number but is not in the range 0-sort_types,
	 * error */

	fputs(_("Usage: st [METHOD] [rev]\nMETHOD: 0 = none, "
			"1 = name, 2 = size, 3 = atime, 4 = btime, "
		    "5 = ctime, 6 = mtime, 7 = version\n"), stderr);

	return EXIT_FAILURE;
}

int
xalphasort(const struct dirent **a, const struct dirent **b)
/* Same as alphasort, but is uses strcmp instead of sctroll, which is
 * slower. However, bear in mind that, unlike strcmp(), strcoll() is locale
 * aware. Use only with C and english locales */
{
	int ret = 0;

	/* The if statements prevent strcmp from running in every
	 * call to the function (it will be called only if the first
	 * character of the two strings is the same), which makes the
	 * function faster */
	if ((*a)->d_name[0] > (*b)->d_name[0])
		ret = 1;

	else if ((*a)->d_name[0] < (*b)->d_name[0])
		ret = -1;

	else
		ret = strcmp((*a)->d_name, (*b)->d_name);

	if (!sort_reverse)
		return ret;

	/* If sort_reverse, return the opposite value */
	return (ret - (ret * 2));
}

int
atime_sort(const struct dirent **a, const struct dirent **b)
/* Sort files by last access time */
{
	int ret = 0;
	struct stat atta, attb;

	if (lstat((*a)->d_name, &atta) == -1) {
		fprintf(stderr, _("stat: cannot access '%s': %s\n"),
				(*a)->d_name, strerror(errno));
		return 0;
	}

	if (lstat((*b)->d_name, &attb) == -1) {
		fprintf(stderr, _("stat: cannot access '%s': %s\n"),
				(*b)->d_name, strerror(errno));
		return 0;
	}

	if (atta.st_atim.tv_sec > attb.st_atim.tv_sec)
		ret = 1;

	else if (atta.st_atim.tv_sec < attb.st_atim.tv_sec)
		ret = -1;

	else
		return 0;

	if (!sort_reverse)
		return ret;

	return (ret - (ret * 2));
}

int
btime_sort(const struct dirent **a, const struct dirent **b)
/* Sort files by birthtime */
{
	int ret = 0;

	#if defined(HAVE_ST_BIRTHTIME) || defined(__BSD_VISIBLE)
		struct stat atta, attb;

		if (lstat((*a)->d_name, &atta) == -1) {
			fprintf(stderr, _("stat: cannot access '%s': %s\n"),
					(*a)->d_name, strerror(errno));
			return 0;
		}

		if (lstat((*b)->d_name, &attb) == -1) {
			fprintf(stderr, _("stat: cannot access '%s': %s\n"),
					(*b)->d_name, strerror(errno));
			return 0;
		}

		if (atta.st_birthtime > attb.st_birthtime)
			ret = 1;

		else if (atta.st_birthtime < attb.st_birthtime)
			ret = -1;

		else
			return 0;

	#elif defined(_STATX)
		struct statx atta, attb;

		if (statx(AT_FDCWD, (*a)->d_name, AT_SYMLINK_NOFOLLOW,
		STATX_BTIME, &atta) == -1) {
			fprintf(stderr, _("stat: cannot access '%s': %s\n"),
					(*a)->d_name, strerror(errno));
			return 0;
		}

		if (statx(AT_FDCWD, (*b)->d_name, AT_SYMLINK_NOFOLLOW,
		STATX_BTIME, &attb) == -1) {
			fprintf(stderr, _("stat: cannot access '%s': %s\n"),
					(*b)->d_name, strerror(errno));
			return 0;
		}

		if (atta.stx_btime.tv_sec > attb.stx_btime.tv_sec)
			ret = 1;

		else if (atta.stx_btime.tv_sec < attb.stx_btime.tv_sec)
			ret = -1;

		else
			return 0;

	#else
		return 0;

	#endif

	if (!sort_reverse)
		return ret;

	return (ret - (ret * 2));
}

int
ctime_sort(const struct dirent **a, const struct dirent **b)
/* Sort files by last status change time */
{
	int ret = 0;
	struct stat atta, attb;

	if (lstat((*a)->d_name, &atta) == -1) {
		fprintf(stderr, _("stat: cannot access '%s': %s\n"),
				(*a)->d_name, strerror(errno));
		return 0;
	}

	if (lstat((*b)->d_name, &attb) == -1) {
		fprintf(stderr, _("stat: cannot access '%s': %s\n"),
				(*b)->d_name, strerror(errno));
		return 0;
	}
	if (atta.st_ctim.tv_sec > attb.st_ctim.tv_sec)
		ret = 1;

	else if (atta.st_ctim.tv_sec < attb.st_ctim.tv_sec)
		ret = -1;

	else
		return 0;

	if (!sort_reverse)
		return ret;

	return (ret - (ret * 2));
}

int
mtime_sort(const struct dirent **a, const struct dirent **b)
/* Sort files by last modification time */
{
	int ret = 0;
	struct stat atta, attb;

	if (lstat((*a)->d_name, &atta) == -1) {
		fprintf(stderr, _("stat: cannot access '%s': %s\n"),
				(*a)->d_name, strerror(errno));
		return 0;
	}

	if (lstat((*b)->d_name, &attb) == -1) {
		fprintf(stderr, _("stat: cannot access '%s': %s\n"),
				(*b)->d_name, strerror(errno));
		return 0;
	}
	if (atta.st_mtim.tv_sec > attb.st_mtim.tv_sec)
		ret = 1;

	else if (atta.st_mtim.tv_sec < attb.st_mtim.tv_sec)
		ret = -1;

	else
		return 0;

	if (!sort_reverse)
		return ret;

	return (ret - (ret * 2));
}

int
size_sort(const struct dirent **a, const struct dirent **b)
/* Sort files by size */
{
	int ret = 0;
	struct stat atta, attb;

	if (lstat((*a)->d_name, &atta) == -1) {
		fprintf(stderr, _("stat: cannot access '%s': %s\n"),
				(*a)->d_name, strerror(errno));
		return 0;
	}

	if (lstat((*b)->d_name, &attb) == -1) {
		fprintf(stderr, _("stat: cannot access '%s': %s\n"),
				(*b)->d_name, strerror(errno));
		return 0;
	}

	if (atta.st_size > attb.st_size)
		ret = 1;

	else if (atta.st_size < attb.st_size)
		ret = -1;

	else
		return 0;

	if (!sort_reverse)
		return ret;

	return (ret - (ret * 2));
}

int
alphasort_insensitive(const struct dirent **a, const struct dirent **b)
/* This is a modification of the alphasort function that makes it case 
 * insensitive. It also sorts without taking the initial dot of hidden 
 * files into account. Note that strcasecmp() isn't locale aware. Use
 * only with C and english locales */
{
	int ret = strcasecmp(((*a)->d_name[0] == '.') ? (*a)->d_name + 1 : 
						(*a)->d_name, ((*b)->d_name[0] == '.') ? 
						(*b)->d_name + 1 : (*b)->d_name);

	if (!sort_reverse)
		return ret;

	return (ret - (ret * 2));
}

int
m_alphasort(const struct dirent **a, const struct dirent **b)
/* Just a reverse sorting capable alphasort */
{
	int ret = strcoll((*a)->d_name, (*b)->d_name);

	if (!sort_reverse)
		return ret;

	return (ret - (ret * 2));
}

/* NOTE: strverscmp() is a GNU extension, so that it will be available
 * neither on FreeBSD nor when compiling with _BE_POSIX */
#if !defined __FreeBSD__ && !defined _BE_POSIX
int
m_versionsort(const struct dirent **a, const struct dirent **b)
/* Just a reverse sorting capable versionsort. */
{
	int ret = strverscmp((*a)->d_name, (*b)->d_name);

	if (!sort_reverse)
		return ret;

	return (ret - (ret * 2));
}
#endif

int
remote_ftp(char *address, char *options)
{
	#if __FreeBSD__
	fprintf(stderr, _("%s: FTP is not yet supported on FreeBSD\n"),
			PROGRAM_NAME);
	return EXIT_FAILURE;
	#endif

	if (!address || !*address)
		return EXIT_FAILURE;

	char *tmp_addr = (char *)xnmalloc(strlen(address) + 1, sizeof(char));
	strcpy(tmp_addr, address);

	char *p = tmp_addr;
	while (*p) {
		if (*p == '/')
			*p = '_';
		p++;
	}

	char *rmountpoint = (char *)xnmalloc(strlen(TMP_DIR)
										 + strlen(tmp_addr)
										 + 9, sizeof(char));
	sprintf(rmountpoint, "%s/remote/%s", TMP_DIR, tmp_addr);
	free(tmp_addr);

	struct stat file_attrib;
	if (stat(rmountpoint, &file_attrib) == -1) {
		char *mkdir_cmd[] = { "mkdir", "-p", rmountpoint, NULL };
		if (launch_execve(mkdir_cmd, FOREGROUND) != EXIT_SUCCESS) {
			fprintf(stderr, _("%s: '%s': Cannot create mountpoint\n"),
					PROGRAM_NAME, rmountpoint);
			free(rmountpoint);
			return EXIT_FAILURE;
		}
	}

	else if (count_dir(rmountpoint) > 2) {
		fprintf(stderr, _("%s: '%s': Mounpoint not empty\n"),
				PROGRAM_NAME, rmountpoint);
		free(rmountpoint);
		return EXIT_FAILURE;
	}

	/* CurlFTPFS does not require sudo */
	char *cmd[] = { "curlftpfs", address, rmountpoint, (options) ? "-o"
					: NULL, (options) ? options: NULL, NULL };
	int error_code = launch_execve(cmd, FOREGROUND);

	if (error_code) {
		free(rmountpoint);
		return EXIT_FAILURE;
	}

	if (chdir(rmountpoint) != 0) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, rmountpoint,
				strerror(errno));
		free(rmountpoint);
		return EXIT_FAILURE;
	}

	free(path);
	path = (char *)xnmalloc(strlen(rmountpoint) + 1, sizeof(char));
	strcpy(path, rmountpoint);

	free(rmountpoint);

	if (cd_lists_on_the_fly) {
		while (files--)
			free(dirlist[files]);
		if (list_dir() != 0)
			error_code = 1;
	}

	return error_code;

}

int
remote_smb(char *address, char *options)
{
	#if __FreeBSD__
	fprintf(stderr, _("%s: SMB is not yet supported on FreeBSD\n"),
			PROGRAM_NAME);
	return EXIT_FAILURE;
	#endif

	/* smb://[USER@]HOST[/SERVICE][/REMOTE-DIR] */

	if (!address || !*address)
		return EXIT_FAILURE;

	int free_address = 0;
	char *ruser = (char *)NULL, *raddress = (char *)NULL;
	char *tmp = strchr(address, '@');
	if (tmp) {
		*tmp = 0x00;
		ruser = (char *)xnmalloc(strlen(address) + 1, sizeof(char));
		strcpy(ruser, address);

		raddress = (char *)xnmalloc(strlen(tmp + 1) + 1, sizeof(char));
		strcpy(raddress, tmp + 1);
		free_address = 1;
	}
	else
		raddress = address;

	char *addr_tmp = (char *)xnmalloc(strlen(raddress)
									  + 3, sizeof(char));
	sprintf(addr_tmp, "//%s", raddress);

	char *p = raddress;
	while (*p) {
		if (*p == '/')
			*p = '_';
		p++;
	}

	char *rmountpoint = (char *)xnmalloc(strlen(raddress)
										 + strlen(TMP_DIR)
										 + 9, sizeof(char));
	sprintf(rmountpoint, "%s/remote/%s", TMP_DIR, raddress);

	int free_options = 0;
	char *roptions = (char *)NULL;
	if (ruser) {
		roptions = (char *)xnmalloc(strlen(ruser) + strlen(options)
									+ 11, sizeof(char));
		sprintf(roptions, "username=%s,%s", ruser, options);
		free_options = 1;
	}
	else
		roptions = options;

	/* Create the mountpoint, if it doesn't exist */
	struct stat file_attrib;
	if (stat(rmountpoint, &file_attrib) == -1) {
		char *mkdir_cmd[] = { "mkdir", "-p", rmountpoint, NULL };
		if (launch_execve(mkdir_cmd, FOREGROUND) != EXIT_SUCCESS) {
			if (free_options)
				free(roptions);
			if (ruser)
				free(ruser);
			if (free_address)
				free(raddress);
			free(rmountpoint);
			free(addr_tmp);
			fprintf(stderr, _("%s: '%s': Cannot create mountpoint\n"),
					PROGRAM_NAME, rmountpoint);
			return EXIT_FAILURE;
		}
	}

	/* If the mountpoint already exists, check if it is empty */
	else if (count_dir(rmountpoint) > 2) {
		fprintf(stderr, _("%s: '%s': Mountpoint not empty\n"),
				PROGRAM_NAME, rmountpoint);
		if (free_options)
			free(roptions);
		if (ruser)
			free(ruser);
		if (free_address)
			free(raddress);
		free(rmountpoint);
		free(addr_tmp);
		return EXIT_FAILURE;
	}

	int error_code = 1;
	/* Create and execute the SMB command */
	if (!(flags & ROOT_USR)) {
		char *cmd[] = { "sudo", "-u", "root", "mount.cifs", addr_tmp,
						rmountpoint, (roptions) ? "-o" : NULL,
						(roptions) ? roptions : NULL, NULL };
		error_code = launch_execve(cmd, FOREGROUND);
	}
	else {
		char *cmd[] = { "mount.cifs", addr_tmp, rmountpoint, (roptions)
						? "-o" : NULL, (roptions) ? roptions
						: NULL, NULL };
		error_code = launch_execve(cmd, FOREGROUND);
	}

	if (free_options)
		free(roptions);
	if (ruser)
		free(ruser);
	if (free_address)
		free(raddress);
	free(addr_tmp);	

	if (error_code) {
		free(rmountpoint);
		return EXIT_FAILURE;
	}

	/* If successfully mounted, chdir into mountpoint */
	if (chdir(rmountpoint) != 0) {
		fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, rmountpoint,
				strerror(errno));
		free(rmountpoint);
		return EXIT_FAILURE;
	}

	free(path);
	path = (char *)xnmalloc(strlen(rmountpoint) + 1, sizeof(char));
	strcpy(path, rmountpoint);

	free(rmountpoint);

	if (cd_lists_on_the_fly) {
		while (files--)
			free(dirlist[files]);
		if (list_dir() != 0)
			error_code = 1;
	}

	return error_code;
}

int
remote_ssh(char *address, char *options)
{
	#if __FreeBSD__
	fprintf(stderr, _("%s: SFTP is not yet supported on FreeBSD"),
			PROGRAM_NAME);
	return EXIT_FAILURE;
	#endif

	if (!config_ok)
		return EXIT_FAILURE;

/*	char *sshfs_path = get_cmd_path("sshfs");
	if (!sshfs_path) {
		fprintf(stderr, _("%s: sshfs: Program not found.\n"),
				PROGRAM_NAME);
		return EXIT_FAILURE;
	} */

	if(!address || !*address)
		return EXIT_FAILURE;

	/* Create mountpoint */
	char *rname = (char *)xnmalloc(strlen(address) + 1, sizeof(char));
	strcpy(rname, address);

	/* Replace all slashes in address by underscore to construct the
	 * mounpoint name */
	char *p = rname;
	while (*p) {
		if (*p == '/')
			*p = '_';
		p++;
	}

	char *rmountpoint = (char *)xnmalloc(strlen(TMP_DIR) + strlen(rname)
										 + 9, sizeof(char));
	sprintf(rmountpoint, "%s/remote/%s", TMP_DIR, rname);
	free(rname);

	/* If the mountpoint doesn't exist, create it */
	struct stat file_attrib;
	if (stat(rmountpoint, &file_attrib) == -1) {
		char *mkdir_cmd[] = { "mkdir", "-p", rmountpoint, NULL };

		if (launch_execve(mkdir_cmd, FOREGROUND) != EXIT_SUCCESS) {
			fprintf(stderr, _("%s: '%s': Cannot create mountpoint\n"), 
					PROGRAM_NAME, rmountpoint);
			free(rmountpoint);
			return EXIT_FAILURE;
		}
	}

	/* If it exists, make sure it is not populated */
	else if (count_dir(rmountpoint) > 2) {
		fprintf(stderr, _("%s: '%s': Mounpoint not empty\n"), 
				PROGRAM_NAME, rmountpoint);
		free(rmountpoint);
		return EXIT_FAILURE;
	}

	/* Construct the command */

	int error_code = 1;

	if ((flags & ROOT_USR)) {
		char *cmd[] = { "sshfs", address, rmountpoint, (options) ? "-o"
						: NULL, (options) ? options: NULL, NULL };
		error_code = launch_execve(cmd, FOREGROUND);
	}
	else {
		char *cmd[] = { "sudo", "sshfs", address, rmountpoint, "-o",
						"allow_other", (options) ? "-o" : NULL,
						(options) ? options : NULL, NULL};
		error_code = launch_execve(cmd, FOREGROUND);
	}

	if (error_code != EXIT_SUCCESS) {
		free(rmountpoint);
		return EXIT_FAILURE;
	}

	/* If successfully mounted, chdir into mountpoint */
	if (chdir(rmountpoint) != 0) {
		fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, rmountpoint,
				strerror(errno));
		free(rmountpoint);
		return EXIT_FAILURE;
	}

	free(path);
	path = (char *)xnmalloc(strlen(rmountpoint) + 1, sizeof(char));
	strcpy(path, rmountpoint);
	free(rmountpoint);

	if (cd_lists_on_the_fly) {
		while (files--)
			free(dirlist[files]);
		if (list_dir() != 0)
			error_code = 1;
	}

	return error_code;
}


int
create_config(char *file)
{
	FILE *config_fp = fopen(file, "w");

	if (!config_fp) {
		fprintf(stderr, "%s: fopen: '%s': %s\n", PROGRAM_NAME, 
				file, strerror(errno));
		return EXIT_FAILURE;
	}

	/* Do not translate anything in the config file */
	fprintf(config_fp,

"\t\t###########################################\n\
\t\t#                  CLIFM                  #\n\
\t\t#  The anti-eye-candy, KISS file manager  #\n\
\t\t###########################################\n\n"

"# This is the configuration file for CLiFM\n\n"

"# FiletypeColors define the color used for filetypes when listing files. It \n\
# uses the same format used by the LS_COLORS environment variable. Thus, \n\
# \"di=01;34\" means that (non-empty) directories will be listed in bold blue.\n\
# Color codes are traditional ANSI escape sequences less the escape char and \n\
# the final 'm'. 8 bit, 256 colors, and RGB colors are supported.\n"

"FiletypeColors=\"di=01;34:nd=01;31:ed=00;34:ne=00;31:fi=00;39:\
ef=00;33:nf=00;31:ln=01;36:or=00;36:pi=33;40:so=01;35:bd=01;33:\
cd=01;37:su=37;41:sg=30;43:ca=30;41:tw=30;42:ow=34;42:st=37;44:\
ex=01;32:ee=00;32:no=00;31;47\"\n\n"

"# All the color lines below use the same color codes as FiletypeColors.\n"
"# TextColor specifies the color of the text typed in the command line.\n"
"TextColor=00;39;49\n\n"

"ELNColor=01;33\n\n"

"# DefaultColor is the color used when no color is specified, neither by the \n\
# user nor by CliFM itself, for a given ouput.\n\
DefaultColor=00;39;49\n\
DirCounterColor=00;39;49\n\
DividingLineColor=00;34\n\
WelcomeMessageColor=01;36\n\n"

"# By default, the amount of files contained by a directory is informed next\n\
# to the directory name. However, this feature might slow things down when, \n\
# for example, listing files on a remote server. The filescounter can be \n\
# disabled here or using the 'fc' command while in the program itself.\n\
FilesCounter=true\n\n"

"# DividingLineChar accepts both literal characters (in single quotes) and \n\
# decimal numbers.\n\
DividingLineChar='='\n\n"

"# The prompt line is build using string literals and/or the following escape\n\
# sequences:\n"
"# \\xnn: The character whose hexadecimal code is nn.\n\
# \\e: Escape character\n\
# \\h: The hostname, up to the first dot\n\
# \\u: The username\n\
# \\H: The full hostname\n\
# \\n: A newline character\n\
# \\r: A carriage return\n\
# \\a: A bell character\n\
# \\d: The date, in abbrevieted form (ex: 'Tue May 26')\n\
# \\s: The name of the shell (everything after the last slash) currently used\n\
# by CliFM\n\
# \\t: The time, in 24-hour HH:MM:SS format\n\
# \\T: The time, in 12-hour HH:MM:SS format\n\
# \\@: The time, in 12-hour am/pm format\n\
# \\A: The time, in 24-hour HH:MM format\n\
# \\w: The full current working directory, with $HOME abbreviated with a tilde\n\
# \\W: The basename of $PWD, with $HOME abbreviated with a tilde\n\
# \\p: A mix of the two above, it abbreviates the current working directory \n\
# only if longer than PathMax (a value defined in the configuration file).\n\
# \\z: Exit code of the last executed command. :) if success and :( in case of \n\
# error\n\
# \\$ '#', if the effective user ID is 0, and '$' otherwise\n\
# \\nnn: The character whose ASCII code is the octal value nnn\n\
# \\\\: A backslash\n\
# \\[: Begin a sequence of non-printing characters. This is mostly used to \n\
# add color to the prompt line\n\
# \\]: End a sequence of non-printing characters\n\n"

"Prompt=\"%s\"\n\n",

			DEFAULT_PROMPT);

	fprintf(config_fp,
"# MaxPath is only used for the /p option of the prompt: the current working \n\
# directory will be abbreviated to its basename (everything after last slash)\n\
# whenever the current path is longer than MaxPath.\n\
MaxPath=40\n\n"

"WelcomeMessage=true\n\
SplashScreen=false\n\
ShowHiddenFiles=true\n\
LongViewMode=false\n\
ExternalCommands=false\n\
LogCmds=false\n\n"

"# If set to true, a command name that is the name of a directory or a\n\
# file is executed as if it were the argument to the the 'cd' or the \n\
# 'open' commands respectivelly.\n\
Autocd=true\n\
AutoOpen=true\n\n"

"# In light mode, colors and filetype checks (except the directory check,\n\
# which is enabled by default) are disabled to speed up the listing\n\
# process.\n\
LightMode=false\n\n"

"# The following three options are only valid when running in light mode:\n\
# Perform a directory check appending a slash at the end of directory\n\
# names.\n\
DirIndicator=true\n"
"# Append filetype indicator at the end of filenames: '/' for directories,\n\
# '@' for symbolic links, '=' for sockets, and '|' for FIFO/pipes. This\n\
# option implies DirIndicator.\n\
Classify=false\n"
"# Same as Classify, but append '*' to executable files as well. This\n\
# option implies Classify, but is slower, since access(3) needs to be\n\
# called for every regular file.\n\
ClassifyExec=false\n\n"

"# Should the Selection Box be shared among different profiles?\n\
ShareSelbox=false\n\n"

"# Choose the resource opener to open files with their default associated\n\
# application. If not set, CLiFM built-in opener is used\n\
Opener=\n\n"

"# Set the shell to be used when running external commands. Defaults to the \n\
# user's shell as is specified in '/etc/passwd'.\n\
SystemShell=\n\n"

"# Only used when opening a directory via a new CliFM instance (with the 'x' \n\
# command), this option specifies the command to be used to launch a \n\
# terminal emulator to run CliFM on it.\n\
TerminalCmd='%s'\n\n"

"# Choose sorting method: 0 = none, 1 = name, 2 = size, 3 = atime\n\
# 4 = btime (ctime if not available), 5 = ctime, 6 = mtime, 7 = version\n\
# NOTE: the 'version' method is not available on FreeBSD\n\
Sort=1\n\
# By default, CliFM sorts files from less to more (ex: from 'a' to 'z' if\n\
# using the \"name\" method). To invert this ordering, set SortReverse to\n\
# true\n\
SortReverse=false\n\n"

"Tips=true\n\
ListFoldersFirst=true\n\
CdListsAutomatically=true\n\
CaseSensitiveList=false\n\
Unicode=false\n\
Pager=false\n\
MaxHistory=500\n\
MaxLog=1000\n\
ClearScreen=false\n\n"

"# If not specified, StartingPath defaults to the current working directory.\n\
StartingPath=\n\n"
"#END OF OPTIONS\n\n",

			DEFAULT_TERM_CMD);

	fputs(

"#ALIASES\n\
#alias ls='ls --color=auto -A'\n\n"

"#PROMPT COMMANDS\n\n"
"# Write below the commands you want to be executed before the prompt.\n\
# Ex:\n\
#date | awk '{print $1\", \"$2,$3\", \"$4}'\n\n"
"#END OF PROMPT COMMANDS\n\n", config_fp);

	fclose(config_fp);

	return EXIT_SUCCESS;
}

int
new_instance(char *dir)
/* Open DIR in a new instance of the program (using TERM, set in the config 
 * file, as terminal emulator) */
{
	if (!term) {
		fprintf(stderr, _("%s: Default terminal not set. Use the "
				"configuration file to set one\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	if (!(flags & GRAPHICAL)) {
		fprintf(stderr, _("%s: Function only available for graphical "
				"environments\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	/* Get absolute path of executable name of itself */
	char *self = realpath("/proc/self/exe", NULL);
	if (!self) {
		fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
		return EXIT_FAILURE;
	}

	if (!dir) {
		free(self);
		return EXIT_FAILURE;
	}

	char *deq_dir = dequote_str(dir, 0);
	if (!deq_dir) {
		fprintf(stderr, _("%s: '%s': Error dequoting filename\n"),
				PROGRAM_NAME, dir);
		free(self);
		return EXIT_FAILURE;
	}

	struct stat file_attrib;
	if (stat(deq_dir, &file_attrib) == -1) {
		fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, deq_dir, 
				strerror(errno));
		free(self);
		free(deq_dir);
		return EXIT_FAILURE;
	}

	if ((file_attrib.st_mode & S_IFMT) != S_IFDIR) {
		fprintf(stderr, _("%s: '%s': Not a directory\n"),
				PROGRAM_NAME, deq_dir);
		free(self);
		free(deq_dir);
		return EXIT_FAILURE;
	}

	char *path_dir = (char *)NULL;

	if (*deq_dir != '/') {
		path_dir = (char *)xnmalloc(strlen(path) + strlen(deq_dir)
									+ 2, sizeof(char));
		sprintf(path_dir, "%s/%s", path, deq_dir);
	}
	else
		path_dir = deq_dir;

/*	char *cmd = (char *)xnmalloc(strlen(term) + strlen(self) 
								 + strlen(path_dir) + 13, sizeof(char));
	sprintf(cmd, "%s %s -p \"%s\" &", term, self, path_dir); 

	int ret = launch_execle(cmd);
	free(cmd); */

 	char **tmp_term = (char **)NULL, **tmp_cmd = (char **)NULL;
	if (strcntchr(term, 0x20) != -1) {
		tmp_term = get_substr(term, 0x20);
		if (tmp_term) {
			size_t i;

			for (i = 0; tmp_term[i]; i++);

			size_t num = i;
			tmp_cmd = (char **)xrealloc(tmp_cmd, (i + 4)
										* sizeof(char *));
			for (i = 0; tmp_term[i]; i++) {
				tmp_cmd[i] = (char *)xnmalloc(strlen(tmp_term[i])
											  + 1, sizeof(char));
				strcpy(tmp_cmd[i], tmp_term[i]);
				free(tmp_term[i]);
			}
			free(tmp_term);

			i = num - 1;
			tmp_cmd[i + 1] = (char *)xnmalloc(strlen(self) + 1,
											  sizeof(char));
			strcpy(tmp_cmd[i + 1], self);
			tmp_cmd[i + 2] = (char *)xnmalloc(3, sizeof(char));
			strcpy(tmp_cmd[i + 2], "-p\0");
			tmp_cmd[i + 3] = (char *)xnmalloc(strlen(path_dir)
											  + 1, sizeof(char));
			strcpy(tmp_cmd[i + 3], path_dir);
			tmp_cmd[i + 4] = (char *)NULL;
		}
	}

	int ret = -1;

	if (tmp_cmd) {
		ret = launch_execve(tmp_cmd, BACKGROUND);
		for (size_t i = 0; tmp_cmd[i]; i++)
			free(tmp_cmd[i]);
		free(tmp_cmd);
	}

	else {
		fprintf(stderr, _("%s: No option specified for '%s'\n"
				"Trying '%s -e %s -p %s'\n"), PROGRAM_NAME, term,
				term, self, path);

		char *cmd[] = { term, "-e", self, "-p", path_dir, NULL };
		ret = launch_execve(cmd, BACKGROUND);
	}

	if (*deq_dir != '/')
		free(path_dir);
	free(deq_dir);
	free(self);

	if (ret != EXIT_SUCCESS)
		fprintf(stderr, _("%s: Error lauching new instance\n"), 
				PROGRAM_NAME);

	return ret;
}

void
add_to_cmdhist(char *cmd)
{
	if (!cmd)
		return;

	/* For readline */
	add_history(cmd);
	if (config_ok)
		append_history(1, HIST_FILE);

	/* For us */
	/* Add the new input to the history array */
	size_t cmd_len = strlen(cmd);
	history = (char **)xrealloc(history, (size_t)(current_hist_n + 1) 
								* sizeof(char *));
	history[current_hist_n] = (char *)xcalloc(cmd_len + 1,
											  sizeof(char));
	strcpy(history[current_hist_n++], cmd);
}

int
record_cmd(char *input)
/* Returns 1 if INPUT should be stored in history and 0 if not */
{
	/* NULL input */
	if (!input)
		return 0;

	/* Blank lines */
	unsigned short blank = 1;
	char *p = input;
	while (*p) {
		if (*p > 0x20) {
			blank = 0;
			break;
		}
		p++;
	}

	if (blank)
		return 0;

	/* Rewind the pointer to the beginning of the input line */
	p = input;

	/* Commands starting with space */
	if (*p == 0x20)
		return 0;

	/* Exit commands */
	switch (*p) {
	case 'q':
		if (*(p + 1) == 0x00 || strcmp(p, "quit") == 0)
			return 0;
		break;
	case 'e':
		if (strcmp(p, "exit") == 0)
			return 0;
		break;
	case 'z':
		if (*(p + 1) == 'z' && *(p + 2) == 0x00)
			return 0;
		break;
	case 's':
		if (strcmp(p, "salir") == 0)
			return 0;
		break;
	case 'c':
		if (strcmp(p, "chau") == 0)
			return 0;
		break;
	default: break;
	}

	/* History */
	if (*p == '!' && (isdigit(*(p + 1))
	|| (*(p + 1) == '-' && isdigit(*(p + 2)))
	|| ((*(p + 1) == '!') && *(p + 2) == 0x00)))
		return 0;

	/* Consequtively equal commands in history */
	if (history && history[current_hist_n - 1] 
	&& (strcmp(p, history[current_hist_n - 1]) == 0))
		return 0;

	return 1;
}

int
alias_import(char *file)
{
	if (!file)
		return EXIT_FAILURE;

	char rfile[PATH_MAX] = "";
	rfile[0] = 0x00;

/*	if (*file == '~' && *(file + 1) == '/') { */
	if (*file == '~') {
		char *file_exp = tilde_expand(file);
		if (!file_exp) {
			fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, file, 
					strerror(errno));
			return EXIT_FAILURE;
		}
		realpath(file_exp, rfile);
		free(file_exp);
	}
	else
		realpath(file, rfile);

	if (rfile[0] == 0x00) {
		fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, file,
				strerror(errno));
		return EXIT_FAILURE;
	}

	if (access(rfile, F_OK|R_OK) != 0) {
		fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, rfile,
				strerror(errno));
		return EXIT_FAILURE;
	}

	/* Open the file to import aliases from */
	FILE *fp = fopen(rfile, "r");
	if (!fp) {
		fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, rfile,
				strerror(errno));
		return EXIT_FAILURE;
	}

	/* Open CLiFM config file as well */
	FILE *config_fp = fopen(CONFIG_FILE, "a");
	if (!config_fp) {
		fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME,
				CONFIG_FILE, strerror(errno));
		fclose(fp);
		return EXIT_FAILURE;
	}

	size_t line_size = 0, i;
	char *line = (char *)NULL;
	ssize_t line_len = 0;
	size_t alias_found = 0, alias_imported = 0;
	int first = 1;

	while ((line_len = getline(&line, &line_size, fp)) > 0) {
		if (strncmp(line, "alias ", 6) == 0) {
			alias_found++;

			/* If alias name conflicts with some internal command,
			 * skip it */
			char *alias_name = strbtw(line, ' ', '=');
			if (!alias_name)
				continue;

			if (is_internal_c(alias_name)) {
				fprintf(stderr, _("'%s': Alias conflicts with "
						"internal command\n"), alias_name);
				free(alias_name);
				continue;
			}

			char *p = line + 6; /* p points now to the beginning of the 
			alias name (because "alias " == 6) */

			/* Only accept single quoted aliases commands */
			char *tmp = strchr(p, '=');
			if (!tmp)
				continue; 

			if (*(++tmp) != '\'') {
				free(alias_name);
				continue;
			}

			/* If alias already exists, skip it too */
			int exists = 0;
			for (i = 0; i < aliases_n; i++) {
				int alias_len = strcntchr(aliases[i], '=');
				if (alias_len != -1 
				&& strncmp(aliases[i], p, (size_t)alias_len + 1) == 0) {
					exists = 1;
					break;
				}
			}

			if (!exists) {
				if (first) {
					first = 0;
					fputs("\n\n", config_fp);
				}

				alias_imported++;

				/* Write the new alias into CLiFM config file */
				fputs(line, config_fp);
			}
			else
				fprintf(stderr, _("'%s': Alias already exists\n"),
						alias_name);

			free(alias_name);
		}
	}

	free(line);

	fclose(fp);
	fclose(config_fp);

	/* No alias was found in FILE */
	if (alias_found == 0) {
		fprintf(stderr, _("%s: '%s': No alias found\n"), PROGRAM_NAME,
				rfile);
		return EXIT_FAILURE;
	}

	/* Aliases were found in FILE, but none was imported (either because
	 * they conflicted with internal commands or the alias already
	 * existed) */
	else if (alias_imported == 0) {
		fprintf(stderr, _("%s: No alias imported\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	/* If some alias was found and imported, print the corresponding
	 * message and update the aliases array */
	if (alias_imported > 1)
		printf(_("%s: %zu aliases were successfully imported\n"),
				PROGRAM_NAME, alias_imported);
	else
		printf(_("%s: 1 alias was successfully imported\n"),
			   PROGRAM_NAME);

	/* Add new aliases to the internal list of aliases */
	get_aliases();

	/* Add new aliases to the commands list for TAB completion */
	if (bin_commands) {
		for (i = 0; bin_commands[i]; i++)
			free(bin_commands[i]);
		free(bin_commands);
		bin_commands = (char  **)NULL;
	}

	get_path_programs();

	return EXIT_SUCCESS;
}

int *
get_hex_num(char *str)
/* Given this value: \xA0\xA1\xA1, return an array of integers with
 * the integer values for A0, A1, and A2 respectivelly */
{
	size_t i = 0; 
	int *hex_n = (int *)calloc(3, sizeof(int));

	while (*str) {
		if (*str == '\\') {
			if (*(str + 1) == 'x') {
				str += 2;
				char *tmp = calloc(3, sizeof(char));
				strncpy(tmp, str, 2);
				if (i >= 3)
					hex_n = xrealloc(hex_n, (i + 1) * sizeof(int *));
				hex_n[i++] = hex2int(tmp);
				free(tmp);
			}
			else
				break;
		}
		str++;
	}

	hex_n = xrealloc(hex_n, (i + 1) * sizeof(int));
	hex_n[i] = -1; /* -1 marks the end of the int array */

	return hex_n;
}

char *
decode_prompt(char *line)
/* Decode the prompt string (encoded_prompt global variable) taken from
 * the configuration file. Based on the decode_prompt_string function
 * found in an old bash release (1.14.7). */
{
	if(!line)
		return (char *)NULL;

	#define CTLESC '\001'
	#define CTLNUL '\177'

	char *temp = (char *)NULL, *result = (char *)NULL;
	size_t result_len = 0;
	int c;

	while((c = *line++)) {
		/* We have a escape char */
		if (c == '\\') {

			/* Now move on to the next char */
			c = *line;

			switch (c) {

			case 'z': /* Exit status of last executed command */
				temp = (char *)xnmalloc(3, sizeof(char));
				temp[0] = ':';
				temp[1] = (exit_code) ? '(' : ')';
				temp[2] = 0x00;
				goto add_string;

			case 'x': /* Hex numbers */
			{
				/* Go back one char, so that we have "\x ... n", which
				 * is what the get_hex_num() requires */
				line--;
				/* get_hex_num returns an array on integers corresponding
				 * to the hex codes found in line up to the fisrt non-hex 
				 * expression */
				int *hex = get_hex_num(line);
				int n = 0, i = 0, j;
				/* Count how many hex expressions were found */
				while (hex[n++] != -1);
				n--;
				/* 2 + n == CTLEST + 0x00 + amount of hex numbers*/
				temp = xnmalloc(2 + (size_t)n, sizeof(char));
				/* Construct the line: "\001hex1hex2...n0x00"*/
				temp[0] = CTLESC;
				for (j = 1; j < (1 + n); j++)
					temp[j] = (char)hex[i++];
				temp[1 + n] = 0x00;
				/* Set the line pointer after the first non-hex
				 * expression to continue processing */
				line += (i * 4);
				c = 0;
				free(hex);
				goto add_string;
			}

			case 'e': /* Escape char */
				temp = xnmalloc(3, sizeof(char));
				line ++;
				temp[0] = CTLESC;
				/* 27 (dec) == 033 (octal) == 0x1b (hex) == \e */
				temp[1] = 27;
				temp[2] = 0x00;
				c = 0;
				goto add_string;

			case '0': /* Octal char */
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			{
				char octal_string[4];
				int n;

				strncpy(octal_string, line, 3);
				octal_string[3] = 0x00;

				n = read_octal(octal_string);
				temp = xnmalloc(3, sizeof(char));

				if (n == CTLESC || n == CTLNUL) {
					line += 3;
					temp[0] = CTLESC;
					temp[1] = (char)n;
					temp[2] = 0x00;
				}
				else if (n == -1) {
					temp[0] = '\\';
					temp[1] = 0x00;
				}
				else {
					line += 3;
					temp[0] = (char)n;
					temp[1] = 0x00;
				}

				c = 0;
				goto add_string;
			}

			case 'c': /* Program name */
				temp = savestring(PNL, strlen(PNL));
				goto add_string;

			case 't': /* Time: 24-hour HH:MM:SS format */
			case 'T': /* 12-hour HH:MM:SS format */
			case 'A': /* 24-hour HH:MM format */
			case '@': /* 12-hour HH:MM:SS am/pm format */
			case 'd': /* Date: abrev_weak_day, abrev_month_day month_num */
			{
				time_t rawtime = time(NULL);
				struct tm *tm = localtime(&rawtime);
				if (c == 't') {
					char time[9] = "";
					strftime(time, sizeof(time), "%H:%M:%S", tm);
					temp = savestring(time, sizeof(time));
				}
				else if (c == 'T') {
					char time[9] = "";
					strftime(time, sizeof(time), "%I:%M:%S", tm);
					temp = savestring(time, sizeof(time));
				}
				else if (c == 'A') {
					char time[6] = "";
					strftime(time, sizeof(time), "%H:%M", tm);
					temp = savestring(time, sizeof(time));
				}
				else if (c == '@') {
					char time[12] = "";
					strftime(time, sizeof(time), "%I:%M:%S %p", tm);
					temp = savestring(time, sizeof(time));
				}
				else { /* c == 'd' */
					char time[12] = "";
					strftime(time, sizeof(time), "%a %b %d", tm);
					temp = savestring(time, sizeof(time));
				}
				goto add_string;
			}

			case 'u': /* User name */
				temp = savestring(user, strlen(user));
				goto add_string;

			case 'h': /* Hostname up to first '.' */
			case 'H': /* Full hostname */
				temp = savestring(hostname, strlen(hostname));
				if (c == 'h') {
					int ret = strcntchr(hostname, '.');
					if (ret != -1) {
						temp[ret] = 0x00;
					}
				}
				goto add_string;

			case 's': /* Shell name (after last slash)*/
				{
				if (!sys_shell) {
					line++;
					break;
				}
				char *shell_name = strrchr(sys_shell, '/');
				temp = savestring(shell_name + 1,
								  strlen(shell_name) - 1);
				goto add_string;
				}

			case 'p':
			case 'w': /* Full PWD */
			case 'W': /* Short PWD */
				{
				if (!path) {
					line++;
					break;
				}

				/* Reduce HOME to "~" */
				int free_tmp_path = 0;
				char *tmp_path = (char *)NULL;
				if (strncmp(path, user_home, user_home_len) == 0)
					tmp_path = home_tilde(path);
				if (!tmp_path) {
					tmp_path = path;
				}
				else
					free_tmp_path = 1;

				if (c == 'W') {
					char *ret = (char *)NULL;
					/* If not root dir (/), get last dir name */
					if (!(*tmp_path == '/' && !*(tmp_path + 1)))
						ret = strrchr(tmp_path, '/');

					if (!ret)
						temp = savestring(tmp_path, strlen(tmp_path));
					else
						temp = savestring(ret + 1, strlen(ret) - 1);
				}

				/* Reduce path only if longer than max_path */
				else if (c == 'p') {
					if (strlen(tmp_path) > (size_t)max_path) {
						char *ret = (char *)NULL;
						ret = strrchr(tmp_path, '/');
						if (!ret)
							temp = savestring(tmp_path,
											  strlen(tmp_path));
						else
							temp = savestring(ret + 1, strlen(ret) - 1);
					}
					else
						temp = savestring(tmp_path, strlen(tmp_path));
				}

				else /* If c == 'w' */
					temp = savestring(tmp_path, strlen(tmp_path));

				if (free_tmp_path)
					free(tmp_path);

				goto add_string;
				}

			case '$': /* '$' or '#' for normal and root user */
				if ((flags & ROOT_USR))
					temp = savestring("#", 1);
				else
					temp = savestring("$", 1);
				goto add_string;

			case 'a': /* Bell character */
			case 'r': /* Carriage return */
			case 'n': /* New line char */
				temp = savestring(" ", 1);
				if (c == 'n')
					temp[0] = '\n';
				else if (c == 'r')
					temp[0] = '\r';
				else
					temp[0] = '\a';
				goto add_string;

			case '[': /* Begin a sequence of non-printing characters.
			Mostly used to add color sequences. Ex: \[\033[1;34m\] */
			case ']': /* End the sequence */
				temp = xnmalloc(3, sizeof(char));
				temp[0] = '\001';
				temp[1] = (c == '[') ? RL_PROMPT_START_IGNORE
						  : RL_PROMPT_END_IGNORE;
				temp[2] = 0x00;
				goto add_string;

			case '\\': /* Literal backslash */
				temp = savestring ("\\", 1);
				goto add_string;

			default:
				temp = savestring("\\ ", 2);
				temp[1] = (char)c;

			add_string:
				if (c)
					line++;
				result_len += strlen(temp);
				if (!result)
					result = (char *)xcalloc(result_len + 1,
											 sizeof(char));
				else
					result = (char *)xrealloc(result, (result_len + 1)
											  * sizeof(char));
				strcat(result, temp);
				free(temp);
				break;
			}
		}

		/* If not escape code, just add whatever char is there */
		else {
			/* Remove non-escaped quotes */
			if (c == '\'' || c == '"')
				continue;
			result = (char *)xrealloc(result, (result_len + 2)
									  * sizeof(char));
			result[result_len++] = (char)c;
			result[result_len] = 0x00;
		}
	}

	/* Remove trailing new line char, if any */
	if (result[result_len - 1] == '\n')
		result[result_len - 1] = 0x00;

	return result;
}

int
profile_function(char **comm)
{
	int exit_status = EXIT_SUCCESS;

	if (comm[1]) {
		if (strcmp(comm[1], "--help") == 0)
			puts(_("Usage: pf, prof, profile [ls, list] [set, add, "
				   "del PROFILE]"));

		/* List profiles */
		else if (comm[1] && (strcmp(comm[1], "ls") == 0
		|| strcmp(comm[1], "list") == 0)) {
			size_t i;
			for (i = 0; profile_names[i]; i++)
				printf("%s\n", profile_names[i]);
		}

		/* Create a new profile */
		else if (strcmp(comm[1], "add") == 0)
			if (comm[2]) {
				exit_status = profile_add(comm[2]);
			}
			else {
				fputs("Usage: pf, prof, profile add PROFILE\n", stderr);
				exit_status = EXIT_FAILURE;
			}

		/* Delete a profile */
		else if (strcmp(comm[1], "del") == 0)
			if (comm[2])
				exit_status = profile_del(comm[2]);
			else {
				fputs("Usage: pf, prof, profile del PROFILE\n", stderr);
				exit_status = EXIT_FAILURE;
		}

		/* Switch to another profile */
		else if (strcmp(comm[1], "set") == 0) {
			if (comm[2])
				exit_status = profile_set(comm[2]);
			else {
				fputs("Usage: pf, prof, profile set PROFILE\n", stderr);
				exit_status = EXIT_FAILURE;
			}
		}

		/* None of the above == error */
		else {
			fputs(_("Usage: pf, prof, profile [set, add, del PROFILE]\n"), 
				  stderr);
			exit_status = EXIT_FAILURE;
		}
	}

	/* If only "pr" print the current profile name */
	else if (!alt_profile)
		printf("%s: profile: default\n", PROGRAM_NAME);
	else
		printf("%s: profile: '%s'\n", PROGRAM_NAME, alt_profile);

	return exit_status;
}

int
profile_add(char *prof)
{
	if (!prof)
		return EXIT_FAILURE;

	int found = 0;
	size_t i;
	for (i = 0; profile_names[i]; i++) {
		if (strcmp(prof, profile_names[i]) == 0) {
			found = 1;
			break;
		}
	}

	if (found) {
		fprintf(stderr, _("%s: '%s': Profile already exists\n"),
				PROGRAM_NAME, prof);
		return EXIT_FAILURE;
	}

	if (!home_ok) {
		fprintf(stderr, _("%s: '%s': Error creating profile: Home "
		"directory not found\n"), PROGRAM_NAME, prof);
		return EXIT_FAILURE;
	}

	size_t pnl_len = strlen(PNL);
	/* ### GENERATE PROGRAM'S CONFIG DIRECTORY NAME ### */
	char *NCONFIG_DIR = (char *)xcalloc(user_home_len + pnl_len 
										+ strlen(prof) + 11,
										sizeof(char));
	sprintf(NCONFIG_DIR, "%s/.config/%s/%s", user_home, PNL, prof);

	/* #### CREATE THE CONFIG DIR #### */
	char *tmp_cmd[] = { "mkdir", "-p", NCONFIG_DIR, NULL }; 
	int ret = launch_execve(tmp_cmd, FOREGROUND);
	if (ret != EXIT_SUCCESS) {
		fprintf(stderr, _("%s: mkdir: '%s': Error creating "
				"configuration directory\n"), PROGRAM_NAME,
				NCONFIG_DIR);
		free(NCONFIG_DIR);
		return EXIT_FAILURE;
	}

	/* If the config dir is fine, generate config file names */
	int error_code = 0;
	size_t config_len = strlen(NCONFIG_DIR);

	char *NCONFIG_FILE = (char *)xcalloc(config_len + pnl_len + 4, 
										 sizeof(char));
	sprintf(NCONFIG_FILE, "%s/%src", NCONFIG_DIR, PNL);	
	char *NHIST_FILE = (char *)xcalloc(config_len + 13, sizeof(char));
	sprintf(NHIST_FILE, "%s/history.cfm", NCONFIG_DIR);
	char *NPROFILE_FILE = (char *)xcalloc(config_len + pnl_len + 10, 
										  sizeof(char));
	sprintf(NPROFILE_FILE, "%s/%s_profile", NCONFIG_DIR, PNL);
	char *NMIME_FILE = (char *)xcalloc(config_len + 14, sizeof(char));
	sprintf(NMIME_FILE, "%s/mimelist.cfm", NCONFIG_DIR);

/*	char *NMSG_LOG_FILE = (char *)xcalloc(config_len + 14, sizeof(char));
	sprintf(NMSG_LOG_FILE, "%s/messages.cfm", NCONFIG_DIR);	
	char *NBM_FILE = (char *)xcalloc(config_len + 15, sizeof(char));
	sprintf(NBM_FILE, "%s/bookmarks.cfm", NCONFIG_DIR);
	char *NLOG_FILE = (char *)xcalloc(config_len + 9, sizeof(char));
	sprintf(NLOG_FILE, "%s/log.cfm", NCONFIG_DIR);
	char *NLOG_FILE_TMP = (char *)xcalloc(config_len + 13, sizeof(char));
	sprintf(NLOG_FILE_TMP, "%s/log_tmp.cfm", NCONFIG_DIR); */

	/* Create config files */

	/* #### CREATE THE HISTORY FILE #### */
	FILE *hist_fp = fopen(NHIST_FILE, "w+");
	if (!hist_fp) {
		fprintf(stderr, "%s: fopen: '%s': %s\n", PROGRAM_NAME,
				NHIST_FILE, strerror(errno));
		error_code = 1;
	}
	else {
		/* To avoid malloc errors in read_history(), do not create
		 * an empty file */
		fputs("edit\n", hist_fp);
		fclose(hist_fp);
	}

	/* #### CREATE THE MIME CONFIG FILE #### */
	/* Try importing MIME associations from the system, and in case
	 * nothing can be imported, create an empty MIME associations
	 * file */
	ret = mime_import(NMIME_FILE);
	if (ret != 0) {
		FILE *mime_fp = fopen(NMIME_FILE, "w");
		if (!mime_fp) {
			fprintf(stderr, "%s: fopen: '%s': %s\n", PROGRAM_NAME,
					NMIME_FILE, strerror(errno));
			error_code = 1;
		}
		else {
			if ((flags & GRAPHICAL))
				fputs("text/plain=gedit;kate;pluma;mousepad;"
					  "leafpad;nano;vim;vi;emacs;ed\n"
					  "*.cfm=gedit;kate;pluma;mousepad;leafpad;"
					  "nano;vim;vi;emacs;ed\n", mime_fp);
			else
				fputs("text/plain=nano;vim;vi;emacs\n"
					  "*.cfm=nano;vim;vi;emacs;ed\n", mime_fp);
			fclose(mime_fp);
		}
	}

	/* #### CREATE THE PROFILE FILE #### */
	FILE *profile_fp = fopen(NPROFILE_FILE, "w");
	if (!profile_fp) {
		fprintf(stderr, _("%s: Error creating the profile file\n"),
				PROGRAM_NAME);
		error_code = 1;
	}
	else {
		fprintf(profile_fp, _("#%s profile\n"
				"#Write here the commands you want to be executed at "
				"startup\n#Ex:\n#echo -e \"%s, the anti-eye-candy/KISS "
				"file manager\"\n"), PROGRAM_NAME, PROGRAM_NAME);
		fclose(profile_fp);
	}

	/* #### CREATE THE CONFIG FILE #### */
		error_code = create_config(NCONFIG_FILE);

	/* Free stuff */

	free(NCONFIG_DIR);
	free(NCONFIG_FILE);
/*	free(NBM_FILE);
	free(NLOG_FILE);
	free(NMSG_LOG_FILE);
	free(NLOG_FILE_TMP); */
	free(NHIST_FILE);
	free(NPROFILE_FILE);
	free(NMIME_FILE);

	if (error_code == EXIT_SUCCESS) {
		printf(_("%s: '%s': Profile succesfully created\n"),
			   PROGRAM_NAME, prof);
		for (i = 0; profile_names[i]; i++)
			free(profile_names[i]);
		get_profile_names();
	}
	else
		fprintf(stderr, _("%s: '%s': Error creating profile\n"),
				PROGRAM_NAME, prof);

	return error_code;
}

int
profile_del(char *prof)
{
	if (!prof)
		return EXIT_FAILURE;

	/* Check if prof is a valid profile */
	int found = 0;
	size_t i;
	for (i = 0; profile_names[i]; i++) {
		if (strcmp(prof, profile_names[i]) == 0) {
			found = 1;
			break;
		}
	}

	if (!found) {
		fprintf(stderr, _("%s: %s: No such profile\n"),
				PROGRAM_NAME, prof);
		return EXIT_FAILURE;
	}

	char *tmp = (char *)xcalloc(user_home_len + strlen(PNL)
								+ strlen(prof) + 11, sizeof(char));
	sprintf(tmp, "%s/.config/%s/%s", user_home, PNL, prof);

	char *cmd[] = { "rm", "-r", tmp, NULL };
	int ret = launch_execve(cmd, FOREGROUND);
	free(tmp);

	if (ret == EXIT_SUCCESS) {
		printf(_("%s: '%s': Profile successfully removed\n"),
				PROGRAM_NAME, prof);
		for (i = 0; profile_names[i]; i++)
			free(profile_names[i]);
		get_profile_names();
		return EXIT_SUCCESS;
	}

	fprintf(stderr, _("%s: '%s': Error removing profile\n"),
			PROGRAM_NAME, prof);
	return EXIT_FAILURE;
}

int
profile_set(char *prof)
/* Switch to another profile */
{
	if (!prof)
		return EXIT_FAILURE;

	/* Check if prof is a valid profile */
	int found = 0;
	size_t i;
	for (i = 0; profile_names[i]; i++) {
		if (strcmp(prof, profile_names[i]) == 0) {
			found = 1;
			break;
		}
	}
	if (!found) {
		fprintf(stderr, _("%s: '%s': No such profile\nTo add a new "
				"profile enter 'pf add PROFILE'\n"), PROGRAM_NAME, prof);
		return EXIT_FAILURE;
	}

	/* If changing to the current profile, do nothing */
	if ((strcmp(prof, "default") == 0 && !alt_profile)
	|| (alt_profile && strcmp(prof, alt_profile) == 0)) {
		printf("%s: '%s' is the current profile\n", PROGRAM_NAME, prof);
		return EXIT_SUCCESS;
	}

	/* Reset everything */
	free(CONFIG_DIR);
	CONFIG_DIR = (char *)NULL;
	free(TRASH_DIR);
	TRASH_DIR = (char *)NULL;
	free(TRASH_FILES_DIR);
	TRASH_FILES_DIR = (char *)NULL;
	free(TRASH_INFO_DIR);
	TRASH_INFO_DIR = (char *)NULL;
	free(BM_FILE);
	BM_FILE = (char *)NULL;
	free(LOG_FILE);
	LOG_FILE = (char *)NULL;
	free(LOG_FILE_TMP);
	LOG_FILE_TMP = (char *)NULL;
	free(HIST_FILE);
	HIST_FILE = (char *)NULL;
	free(CONFIG_FILE);
	CONFIG_FILE = (char *)NULL;
	free(PROFILE_FILE);
	PROFILE_FILE = (char *)NULL;
	free(MSG_LOG_FILE);
	MSG_LOG_FILE = (char *)NULL;
	free(MIME_FILE);
	MIME_FILE = (char *)NULL;

	if (opener) {
		free(opener);
		opener = (char *)NULL;
	}

	free(TMP_DIR);
	TMP_DIR = (char *)NULL;
	free(sel_file_user);
	sel_file_user = (char *)NULL;

	if (alt_profile) {
		free(alt_profile);
		alt_profile = (char *)NULL;
	}

	if (encoded_prompt) {
		free(encoded_prompt);
		encoded_prompt = (char *)NULL;
	}

	splash_screen = welcome_message = ext_cmd_ok = show_hidden = -1;
	clear_screen = pager = list_folders_first = long_view = -1;
	case_sensitive = cd_lists_on_the_fly = unicode = -1;

	shell_terminal = no_log = internal_cmd = dequoted = 0;
	shell_is_interactive = recur_perm_error_flag = mime_match = 0;
	recur_perm_error_flag = is_sel = sel_is_last = print_msg = 0;
	kbind_busy = 0;
	cont_bt = 0;
	pmsg = nomsg;

	home_ok = config_ok = trash_ok = selfile_ok = 1;

	/* Set the new profile value */
	/* Default profile == (alt_profile == NULL) */
	if (strcmp(prof, "default") != 0) {
		alt_profile = (char *)xcalloc(strlen(prof) + 1, sizeof(char));
		strcpy(alt_profile, prof);
	}

	/* Rerun external_arguments */
/*	if (argc_bk > 1)
		external_arguments(argc_bk, argv_bk); */
	/* If command line arguments are re-processed, and if the user
	 * used the profile option (-P), the new profile won't be set,
	 * because it will be overriden by the command line value */

	/* Set up config files and variables */
	init_config();

	if (!config_ok)
		set_default_options();

	/* Check whether we have a working shell */
	if (access(sys_shell, X_OK) == -1) {
		_err('w', PRINT_PROMPT, _("%s: %s: System shell not found. Please "
			 "edit the configuration file to specify a working shell.\n"),
			 PROGRAM_NAME, sys_shell);
	}

	for (i = 0; i < usrvar_n; i++) {
		free(usr_var[i].name);
		free(usr_var[i].value);
	}
	usrvar_n = 0;

	for (i = 0; i < aliases_n; i++)
		free(aliases[i]);
	free(aliases);
	aliases = (char **)NULL;
	for (i = 0; i < prompt_cmds_n; i++)
		free(prompt_cmds[i]);
	free(prompt_cmds);
	prompt_cmds = (char **)NULL;
	aliases_n = prompt_cmds_n = 0; /* Reset counters */
	get_aliases();
	get_prompt_cmds();

	exec_profile();

	if (msgs_n) {
		for (i = 0; i < (size_t)msgs_n; i++)
			free(messages[i]);
	}
	msgs_n = 0;

	if (config_ok) {
		/* Limit the log files size */
		check_log_file_size(LOG_FILE);
		check_log_file_size(MSG_LOG_FILE);

		/* Reset history */
		if (access(HIST_FILE, F_OK|W_OK) == 0) {
			clear_history(); /* This is for readline */
			read_history(HIST_FILE);
			history_truncate_file(HIST_FILE, max_hist);
		}
		else {
			FILE *hist_fp = fopen(HIST_FILE, "w");
			if (hist_fp) {
				fputs("edit\n", hist_fp);
				fclose(hist_fp);
			}
			else {
				_err('w', PRINT_PROMPT, _("%s: Error opening the "
					 "history file\n"),	PROGRAM_NAME);
			}
		}
		get_history(); /* This is only for us */
	}

	get_bm_names();

	if (cd_lists_on_the_fly) {
		while (files--) free(dirlist[files]);
		list_dir();
	}

	return EXIT_SUCCESS;
}

int mime_open(char **args)
/* Open a file according to the application associated to its MIME type
 * or extension. It also accepts the 'info' and 'edit' arguments, the
 * former providing MIME info about the corresponding file and the
 * latter opening the MIME list file */
{
	/* Check arguments */
	if (!args[1]) {
		puts(_("Usage: mm, mime [info ELN/FILENAME] [edit]"));
		return EXIT_FAILURE;
	}

	/* Check the existence of the 'file' command. */
	char *file_path_tmp = (char *)NULL;
	if ((file_path_tmp = get_cmd_path("file")) == NULL) {
		fprintf(stderr, _("%s: 'file' command not found\n"),
				PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	free(file_path_tmp);
	file_path_tmp = (char *)NULL;

	char *file_path = (char *)NULL, *deq_file = (char *)NULL;
	int info = 0, file_index = 0;

	if (strcmp(args[1], "edit") == 0) {	
		return mime_edit(args);
	}

	else if (strcmp(args[1], "info") == 0) {
		if (!args[2]) {
			fputs(_("Usage: mm, mime info FILENAME\n"), stderr);
			return EXIT_FAILURE;
		}
		if (strcntchr(args[2], '\\') != -1) {
			deq_file = dequote_str(args[2], 0);
			file_path = realpath(deq_file, NULL);
			free(deq_file);
			deq_file = (char *)NULL;
		}
		else
			file_path = realpath(args[2], NULL);

		if (!file_path) {
			fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, args[2],
					(is_number(args[2]) == 1) ? "No such ELN"
					: strerror(errno));
			return EXIT_FAILURE;
		}

		if (access(file_path, R_OK) == -1) {
			fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, file_path,
					strerror(errno));
			free(file_path);
			return EXIT_FAILURE;
		}

		info = 1;
		file_index = 2;
	}

	else {
		if (strcntchr(args[1], '\\') != -1) {
			deq_file = dequote_str(args[1], 0);
			file_path = realpath(deq_file, NULL);
			free(deq_file);
			deq_file = (char *)NULL;
		}
		else
			file_path = realpath(args[1], NULL);

		if (access(file_path, R_OK) == -1) {
			fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, file_path, 
					strerror(errno));
			free(file_path);
			/* Since this function is called by open_function, and since
			 * this latter prints an error message itself whenever the
			 * exit code of mime_open is EXIT_FAILURE, and since we
			 * don't want that message in this case, return -1 instead
			 * to prevent that message from being printed */
			return -1;
		}

		file_index = 1;
	}

	if (!file_path) {
		fprintf(stderr, "'%s': %s\n", args[file_index], strerror(errno));
		return EXIT_FAILURE;
	}

	/* Get file's mime-type */
	char *mime = get_mime(file_path);
	if (!mime) {
		fprintf(stderr, _("%s: Error getting mime-type\n"),
				PROGRAM_NAME);
		free(file_path);
		return EXIT_FAILURE;
	}

	if (info)
		printf(_("MIME type: %s\n"), mime);

	/* Get file extension, if any */
	char *ext = (char *)NULL;
	char *filename = strrchr(file_path, '/');
	if (filename) {
		filename++; /* Remove leading slash */
		if (*filename == '.')
			filename++; /* Remove leading dot if hidden */
		char *ext_tmp = strrchr(filename, '.');
		if (ext_tmp) {
			ext_tmp++; /* Remove dot from extension */
			ext = (char *)xcalloc(strlen(ext_tmp) + 1, sizeof(char));
			strcpy(ext, ext_tmp);
			ext_tmp = (char *)NULL;
		}
		filename = (char *)NULL;
	}

	if (info) {
		if (ext)
			printf(_("Extension: %s\n"), ext);
		else
			puts(_("Extension: None"));
	}

	/* Get default application for MIME or extension */
	char *app = get_app(mime, ext);

	if (!app) {

		if (info)
			fputs(_("Associated application: None\n"), stderr);

		else {

			/* If an archive/compressed file, run archiver() */
			if (is_compressed(file_path, 1) == 0) {

				char *tmp_cmd[] = { "ad", file_path, NULL };
				int exit_status = archiver(tmp_cmd, 'd');
				free(file_path);
				free(mime);
				if (ext)
					free(ext);
				return exit_status;
			}

			else
				fprintf(stderr, _("%s: No associated application found\n"), 
						PROGRAM_NAME);
		}

		free(file_path);
		free(mime);

		if (ext)
			free(ext);

		return EXIT_FAILURE;
	}

	if (info) {
		/* In case of "cmd args" print only cmd */
		int ret = strcntchr(app, 0x20);
		if (ret != -1)
			app[ret] = 0x00;
		printf(_("Associated application: %s (%s)\n"), app, 
			   (mime_match) ? "MIME" : "ext");
		free(file_path);
		free(mime);
		free(app);
		if (ext)
			free(ext);
		return EXIT_SUCCESS;
	}

	free(mime);
	mime = (char *)NULL;
	if (ext) {
		free(ext);
		ext = (char *)NULL;
	}

	/* If not info, open the file with the associated application */

	/* Get number of arguments to check for final ampersand */
	int args_num = 0;
	for (args_num = 0; args[args_num]; args_num++);

	/* Construct the command and run it */
	char *cmd[] = { (opener) ? opener : app, file_path, NULL };
	int ret = launch_execve(cmd, strcmp(args[args_num - 1], "&") == 0
							? BACKGROUND : FOREGROUND);

	free(file_path);
	free(app);

	return ret;
}

int
mime_import(char *file)
/* Import MIME definitions from the system into FILE. This function will
 * only be executed if the MIME file is not found or when creating a new
 * profile. Returns zero if some association is found in the system
 * 'mimeapps.list' files, or one in case of error or no association
 * found */
{

	/* Open the internal MIME file */
	FILE *mime_fp = fopen(file, "w");
	if (!mime_fp)
		return EXIT_FAILURE;

	/* If not in X, just specify a few basic associations to make sure
	 * that at least 'mm edit' will work ('vi' should be installed in
	 * almost any Unix computer) */
	if (!(flags & GRAPHICAL)) {
		fputs("text/plain=nano;vim;vi;emacs;ed\n" 
			  "*.cfm=nano;vim;vi;emacs;ed\n", mime_fp);
		fclose(mime_fp);
		return EXIT_SUCCESS;
	}

	if (!user_home) {
		fclose(mime_fp);
		return EXIT_FAILURE;
	}

	/* Create a list of possible paths for the 'mimeapps.list' file */
	size_t home_len = strlen(user_home);
	char *config_path = (char *)NULL, *local_path = (char *)NULL;
	config_path = (char *)xcalloc(home_len + 23, sizeof(char));
	local_path = (char *)xcalloc(home_len + 41, sizeof(char));
	sprintf(config_path, "%s/.config/mimeapps.list", user_home);
	sprintf(local_path, "%s/.local/share/applications/mimeapps.list",
			user_home);

	char *mime_paths[] = { config_path, local_path,
						   "/usr/local/share/applications/mimeapps.list",
						   "/usr/share/applications/mimeapps.list",
						   "/etc/xdg/mimeapps.list",
						   NULL };

	/* Check each mimeapps.list file and store its associations into
	 * FILE */
	size_t i;
	for (i = 0; mime_paths[i]; i++) {

		FILE *sys_mime_fp = fopen(mime_paths[i], "r");
		if (!sys_mime_fp)
			continue;

		size_t line_size = 0;
		char *line = (char *)NULL;
		ssize_t line_len = 0;

		/* Only store associations in the "Default Applications" section */
		int da_found = 0;
		while ((line_len = getline(&line, &line_size,
		sys_mime_fp)) > 0) {
			if (!da_found && strncmp(line, "[Default Applications]",
			22) == 0) {
				da_found = 1;
				continue;
			}
			if (da_found) {
				if (*line == '[')
					break;
				if (*line == '#' || *line == '\n')
					continue;
				int index = strcntchr(line, '.');
				if (index != -1)
					line[index] = 0x00;
				fprintf(mime_fp, "%s\n", line);
			}
		}

		free(line);
		line = (char *)NULL;

		fclose(sys_mime_fp);
	}

	/* Make sure there is an entry for text/plain and *.cfm files, so
	 * that at least 'mm edit' will work. Gedit, kate, pluma, mousepad,
	 * and leafpad, are the default text editors of Gnome, KDE, Mate,
	 * XFCE, and LXDE respectivelly */
	fputs("text/plain=gedit;kate;pluma;mousepad;leafpad;nano;vim;"
		  "vi;emacs;ed\n"
		  "*.cfm=gedit;kate;pluma;mousepad;leafpad;nano;vim;vi;"
		  "emacs;ed\n", mime_fp);

	fclose(mime_fp);

	free(config_path);
	free(local_path);

	return EXIT_SUCCESS;
}

int
mime_edit(char **args)
{
	if (!args[2]) {
		char *cmd[] = { "mime", MIME_FILE, NULL };

		if (mime_open(cmd) != 0) {
			fputs(_("Try 'mm, mime edit APPLICATION'\n"), stderr);
			return EXIT_FAILURE;
		}

		return EXIT_SUCCESS;
	}

	else {
		char *cmd[] = { args[2], MIME_FILE, NULL };
		if (launch_execve(cmd, FOREGROUND) != EXIT_SUCCESS)
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}
}

char *
get_app(char *mime, char *ext)
/* Get application associated to a given MIME filetype or file extension.
 * Returns the first matching line in the MIME file or NULL if none is
 * found */
{
	if (!mime)
		return (char *)NULL;

	FILE *defs_fp = fopen(MIME_FILE, "r");

	if (!defs_fp) {
		fprintf(stderr, _("%s: '%s': Error opening file\n"),
				PROGRAM_NAME, MIME_FILE);
		return (char *)NULL;
	}

	int found = 0, cmd_ok = 0;
	size_t line_size = 0, mime_len = strlen(mime);
	char *line = (char *)NULL, *app = (char *)NULL;
	ssize_t line_len = 0;

	char *mime_tmp = (char *)xnmalloc(mime_len + 2, sizeof(char));
	sprintf(mime_tmp, "%s=", mime);

	size_t ext_len = 0;
	char *ext_tmp = (char *)NULL;
	if (ext) {
		ext_len = strlen(ext);
		ext_tmp = (char *)xcalloc(ext_len + 2, sizeof(char));
		sprintf(ext_tmp, "%s=", ext);
	}

	while ((line_len = getline(&line, &line_size, defs_fp)) > 0) {
		found = mime_match = 0; /* Global variable to tell mime_open()
		if the application is associated to the file's extension or MIME
		type */
		if (*line == '#' || *line == '[' || *line == '\n')
			continue;

		if (ext) {
			if (*line == '*') {
				if (strncmp(line + 2, ext_tmp, ext_len + 1) == 0)
					found = 1;
			}
		}

		if (strncmp(line, mime_tmp, mime_len + 1) == 0)
				found = mime_match = 1;

		if (found) {
			/* Get associated applications */
			char *tmp = strrchr(line, '=');
			if (!tmp)
				continue;

			tmp++; /* We don't want the '=' char */

			size_t tmp_len = strlen(tmp);
			app = (char *)xrealloc(app, (tmp_len + 1) * sizeof(char));
			size_t app_len = 0;
			while (*tmp) {
				app_len = 0;
				/* Split the appplications line into substrings, if
				 * any */
				while (*tmp != 0x00 && *tmp != ';' && *tmp != '\n' 
				&& *tmp != '\'' && *tmp != '"')
					app[app_len++] = *(tmp++);

				while (*tmp == 0x20) /* Remove leading spaces */
					tmp++;
				if (app_len) {
					app[app_len] = 0x00;
					/* Check each application existence */
					char *file_path = (char *)NULL;
					/* If app contains spaces, the command to check is
					 * the string before the first space */
					int ret = strcntchr(app, 0x20);
					if (ret != -1) {
						char *app_tmp = (char *)xcalloc(app_len + 1, 
														sizeof(char));
						strcpy(app_tmp, app);
						app_tmp[ret] = 0x00;
						file_path = get_cmd_path(app_tmp);
						free(app_tmp);
					}
					else
						file_path = get_cmd_path(app);
					if (file_path) {
						/* If the app exists, break the loops and
						 * return it */
						free(file_path);
						file_path = (char *)NULL;
						cmd_ok = 1;
					}
					else
						continue;
				}

				if (cmd_ok)
					break;
				tmp++;
			}

			if (cmd_ok)
				break;
		}
	}

	free(mime_tmp);
	free(ext_tmp);

	free(line);
	fclose(defs_fp);

	if (found) {
		if (app)
			return app;
	}
	else {
		if (app)
			free(app);
	}

	return (char *)NULL;
}

char *
get_mime(char *file)
{
	if (!file || !*file) {
		fputs(_("Error opening temporary file\n"), stderr);
		return (char *)NULL;
	}

	char MIME_TMP_FILE[PATH_MAX] = "";
	sprintf(MIME_TMP_FILE, "%s/mime_tmp", TMP_DIR);

	if (access(MIME_TMP_FILE, F_OK) == 0)
		remove(MIME_TMP_FILE);

	FILE *file_fp = fopen(MIME_TMP_FILE, "w");

	if (!file_fp) {
		fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME,
				MIME_TMP_FILE, strerror(errno));
		return (char *)NULL;
	}

	FILE *file_fp_err = fopen("/dev/null", "w");

	if (!file_fp_err) {
		fprintf(stderr, "%s: '/dev/null': %s\n", PROGRAM_NAME,
				strerror(errno));
		fclose(file_fp);
		return (char *)NULL;
	}

	int stdout_bk = dup(STDOUT_FILENO); /* Store original stdout */
	int stderr_bk = dup(STDERR_FILENO); /* Store original stderr */

	/* Redirect stdout to the desired file */
	if (dup2(fileno(file_fp), STDOUT_FILENO) == -1 ) {
		fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
		fclose(file_fp);
		fclose(file_fp_err);
		return (char *)NULL;
	}

	/* Redirect stderr to /dev/null */
	if (dup2(fileno(file_fp_err), STDERR_FILENO) == -1) {
		fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
		fclose(file_fp);
		fclose(file_fp_err);
		return (char *)NULL;
	}

	fclose(file_fp);
	fclose(file_fp_err);

	char *cmd[] = { "file", "--mime-type", file, NULL };
	int ret = launch_execve(cmd, FOREGROUND);

	dup2(stdout_bk, STDOUT_FILENO); /* Restore original stdout */
	dup2(stderr_bk, STDERR_FILENO); /* Restore original stderr */
	close(stdout_bk);
	close(stderr_bk);

	if (ret != EXIT_SUCCESS)
		return (char *)NULL;

	char *mime_type = (char *)NULL;

	if (access(MIME_TMP_FILE, F_OK) == 0) {
		file_fp = fopen(MIME_TMP_FILE, "r");
		if (file_fp) {
			char line[255] = "";
			fgets(line, sizeof(line), file_fp);
			char *tmp = strrchr(line, 0x20);
			if (tmp) {
				size_t len = strlen(tmp);
				if (tmp[len - 1] == '\n')
					tmp[len - 1] = 0x00;
				mime_type = (char *)xnmalloc(strlen(tmp), sizeof(char));
				strcpy(mime_type, tmp + 1);
			}
			fclose(file_fp);
		}
		remove(MIME_TMP_FILE);
	}

	if (mime_type)
		return mime_type;

	return (char *)NULL;
}

int
_err(int msg_type, int prompt, const char *format, ...)
/* Custom POSIX implementation of GNU asprintf() modified to log program 
 * messages. MSG_TYPE is one of: 'e', 'w', 'n', or zero (meaning this
 * latter that no message mark (E, W, or N) will be added to the prompt).
 * PROMPT tells whether to print the message immediately before the
 * prompt or rather in place. Based on littlstar's xasprintf
 * implementation:
 * https://github.com/littlstar/asprintf.c/blob/master/asprintf.c*/
{
	va_list arglist, tmp_list;
	int size = 0;

	va_start(arglist, format);
	va_copy(tmp_list, arglist);
	size = vsnprintf((char *)NULL, 0, format, tmp_list);
	va_end(tmp_list);

	if (size < 0) {
		va_end(arglist);
		return EXIT_FAILURE;
	}

	char *buf = (char *)xcalloc((size_t)size + 1, sizeof(char));

	vsprintf(buf, format, arglist);
	va_end(arglist);

	/* If the new message is the same as the last message, skip it */
	if (msgs_n && strcmp(messages[msgs_n - 1], buf) == 0) {
		free(buf);
		return EXIT_SUCCESS;
	}

	if (buf) {
		if (msg_type) {
			switch (msg_type) {
			case 'e': pmsg = error; break;
			case 'w': pmsg = warning; break;
			case 'n': pmsg = notice; break;
			default: pmsg = nomsg;
			}
		}

		log_msg(buf, (prompt) ? PRINT_PROMPT : NOPRINT_PROMPT);
		free(buf);

		return EXIT_SUCCESS;
	}

	return EXIT_FAILURE;
}

int
initialize_readline(void)
{
	/* #### INITIALIZE READLINE (what a hard beast to tackle!!) #### */

	 /* Enable tab auto-completion for commands (in PATH) in case of
	  * first entered string (if autocd and/or auto-open are enabled, check
	  * for paths as well). The second and later entered strings will
	  * be autocompleted with paths instead, just like in Bash, or with
	  * listed filenames, in case of ELN's. I use a custom completion
	  * function to add command and ELN completion, since readline's
	  * internal completer only performs path completion */

	/* Define a function for path completion.
	 * NULL means to use filename_entry_function (), the default
	 * filename completer. */
	rl_completion_entry_function = my_rl_path_completion;

	/* Pointer to alternative function to create matches.
	 * Function is called with TEXT, START, and END.
	 * START and END are indices in RL_LINE_BUFFER saying what the
	 * boundaries of TEXT are.
	 * If this function exists and returns NULL then call the value of
	 * rl_completion_entry_function to try to match, otherwise use the
	 * array of strings returned. */
	rl_attempted_completion_function = my_rl_completion;
	rl_ignore_completion_duplicates = 1;

	/* I'm using here a custom quoting function. If not specified,
	 * readline uses the default internal function. */
	rl_filename_quoting_function = my_rl_quote;

	/* Tell readline what char to use for quoting. This is only the
	 * readline internal quoting function, and for custom ones, like the
	 * one I use above. However, custom quoting functions, though they
	 * need to define their own quoting chars, won't be called at all
	 * if this variable isn't set. */
	rl_completer_quote_characters = "\"'";
	rl_completer_word_break_characters = " ";

	/* Whenever readline finds any of the following chars, it will call
	 * the quoting function */
	rl_filename_quote_characters = " \t\n\"\\'`@$><=,;|&{[()]}?!*^";
	/* According to readline documentation, the following string is
	 * the default and the one used by Bash: " \t\n\"\\'`@$><=;|&{(" */

	/* Executed immediately before calling the completer function, it
	 * tells readline if a space char, which is a word break character
	 * (see the above rl_completer_word_break_characters variable) is
	 * quoted or not. If it is, readline then passes the whole string
	 * to the completer function (ex: "user\ file"), and if not, only
	 * wathever it found after the space char (ex: "file") 
	 * Thanks to George Brocklehurst for pointing out this function:
	 * https://thoughtbot.com/blog/tab-completion-in-gnu-readline*/
	rl_char_is_quoted_p = quote_detector;

	/* This function is executed inmediately before path completion. So,
	 * if the string to be completed is, for instance, "user\ file" (see
	 * the above comment), this function should return the dequoted
	 * string so it won't conflict with system filenames: you want
	 * "user file", because "user\ file" does not exist, and, in this
	 * latter case, readline won't find any matches */
	rl_filename_dequoting_function = dequote_str;

	/* Initialize the keyboard bindings function */
	readline_kbinds();

	return EXIT_SUCCESS;
}

int
get_link_ref(const char *link)
/* Return the filetype of the file pointed to by LINK, or -1 in case of 
 * error. Possible return values: 
	S_IFDIR: 40000 (octal) / 16384 (decimal, integer)
	S_IFREG: 100000 / 32768
	S_IFLNK: 120000 / 40960
	S_IFSOCK: 140000 / 49152
	S_IFBLK: 60000 / 24576
	S_IFCHR: 20000 / 8192
	S_IFIFO: 10000 / 4096
* See the inode manpage */
{
	if (!link)
		return (-1);

	char *linkname = realpath(link, (char *)NULL);
	if (linkname) {
		struct stat file_attrib;
		stat(linkname, &file_attrib);
		free(linkname);
		return (file_attrib.st_mode & S_IFMT);
	}

	return (-1);
}

int
is_internal_c(const char *cmd)
/* Check cmd against a list of internal commands. Used by
 * parse_input_str() when running chained commands */
{
	const char *int_cmds[] = { "o", "open", "cd", "p", "pr", "prop",
					"t", "tr", "trash", "s", "sel", "rf", "refresh",
					"c", "cp", "m", "mv", "bm", "bookmarks", "b",
					"back", "f", "forth", "bh", "fh", "u", "undel",
					"untrash", "s", "sel", "sb", "selbox", "ds",
					"desel", "rm", "mkdir", "ln", "unlink", "touch",
					"r", "md", "l", "p", "pr", "prop", "pf", "prof",
					"profile", "mp", "mountpoints", "ext", "pg",
					"pager", "uc", "unicode", "folders", "ff", "log",
					"msg", "messages", "alias", "shell", "edit",
					"history", "hf", "hidden", "path", "cwd", "splash",
					"ver", "version", "?", "help", "cmd", "commands",
					"colors", "cc", "fs", "mm", "mime", "x", "n",
					"net", "lm", "st", "sort", "fc", "tips", "br",
					"bulk", "opener", "ac", "ad", "acd", "autocd",
					"ao", "auto-open", NULL };

	short found = 0;
	size_t i;
	for (i = 0; int_cmds[i]; i++) {
		if (strcmp(cmd, int_cmds[i]) == 0) {
			found = 1;
			break;
		}
	}

	if (found)
		return 1;

	/* Check for the search and history functions as well */
	else if ((cmd[0] == '/' && access(cmd, F_OK) != 0) 
	|| (cmd[0] == '!' && (isdigit(cmd[1]) 
	|| (cmd[1] == '-' && isdigit(cmd[2])) || cmd[1] == '!')))
		return 1;

	return 0;
}

void
exec_chained_cmds(char *cmd)
/* Execute chained commands (cmd1;cmd2 and/or cmd1 && cmd2). The function
 * is called by parse_input_str() if some non-quoted double ampersand or 
 * semicolon is found in the input string AND at least one of these 
 * chained commands is internal */
{
	if (!cmd)
		return;

	size_t i = 0, cmd_len = strlen(cmd);
	for (i = 0; i < cmd_len; i++) {
		char *str = (char *)NULL;
		size_t len = 0, cond_exec = 0;

		/* Get command */
		str = (char *)xcalloc(strlen(cmd) + 1, sizeof(char));
		while (cmd[i] && cmd[i] != '&' && cmd[i] != ';') {
			str[len++] = cmd[i++];
		}

		/* Should we execute conditionally? */
		if (cmd[i] == '&')
			cond_exec = 1;

		/* Execute the command */
		if (str) {
			char **tmp_cmd = parse_input_str(str);
			free(str);
			if (tmp_cmd) {
				int error_code = 0;
				size_t j;
				char **alias_cmd = check_for_alias(tmp_cmd);
				if (alias_cmd) {
					if (exec_cmd(alias_cmd) != 0)
						error_code = 1;
					for (j = 0; alias_cmd[j]; j++)
						free(alias_cmd[j]);
					free(alias_cmd);
				}
				else {
					if (exec_cmd(tmp_cmd) != 0)
						error_code = 1;
					for (j = 0; j <= args_n; j++)
						free(tmp_cmd[j]);
					free(tmp_cmd);
				}
				/* Do not continue if the execution was condtional and
				 * the previous command failed */
				if (cond_exec && error_code)
					break;
			}
		}
	}
}

int
set_shell(char *str)
/* Set STR as the program current shell */
{
	if (!str || !*str)
		return EXIT_FAILURE;

	/* IF no slash in STR, check PATH env variable for a file named STR
	 * and get its full path*/
	char *full_path = (char *)NULL;
	if (strcntchr(str, '/') == -1)
		full_path = get_cmd_path(str);

	char *tmp = (char *)NULL;
	if (full_path)
		tmp = full_path;
	else
		tmp = str;

	if (access(tmp, X_OK) == -1) {
		fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, tmp,
				strerror(errno));
		return EXIT_FAILURE;
	}

	if (sys_shell)
		free(sys_shell);

	sys_shell = (char *)xnmalloc(strlen(tmp) + 1, sizeof(char));
	strcpy(sys_shell, tmp);
	printf(_("Successfully set '%s' as %s default shell\n"), sys_shell, 
		   PROGRAM_NAME);

	if (full_path)
		free(full_path);

	return EXIT_SUCCESS;
}

char **
get_substr(char *str, const char ifs)
/* Get all substrings from STR using IFS as substring separator, and,
 * if there is a range, expand it. Returns an array containing all
 * substrings in STR plus expandes ranges, or NULL if: STR is NULL or
 * empty, STR contains only IFS(s), or in case of memory allocation
 * error */
{
	if (!str || *str == 0x00)
		return (char **)NULL;

	/* ############## SPLIT THE STRING #######################*/

	char **substr = (char **)NULL;
	void *p = (char *)NULL;
	size_t str_len = strlen(str);
	size_t length = 0, substr_n = 0;

	char *buf = (char *)xnmalloc(str_len + 1, sizeof(char));

	while (*str) {
		while (*str != ifs && *str != 0x00 && length < (str_len + 1))
			buf[length++] = *(str++);
		if (length) {
			buf[length] = 0x00;
			p = (char *)realloc(substr, (substr_n + 1) * sizeof(char *));
			if (!p) {
				/* Free whatever was allocated so far */
				size_t i;
				for (i = 0; i < substr_n; i++)
					free(substr[i]);
				free(substr);
				return (char **)NULL;
			}
			substr = (char **)p;
			p = (char *)calloc(length + 1, sizeof(char));
			if (!p) {
				size_t i;
				for (i = 0; i < substr_n; i++)
					free(substr[i]);
				free(substr);
				return (char **)NULL;
			}
			substr[substr_n] = p;
			p = (char *)NULL;
			strncpy(substr[substr_n++], buf, length);
			length = 0;
		}
		else
			str++;
	}

	free(buf);

	if (!substr_n)
		return (char **)NULL;

	size_t i = 0, j = 0;
	p = (char *)realloc(substr, (substr_n + 1) * sizeof(char *));
	if (!p) {
		for (i = 0; i < substr_n; i++)
			free(substr[i]);
		free(substr);
		substr = (char **)NULL;
		return (char **)NULL;
	}

	substr = (char **)p;
	p = (char *)NULL;
	substr[substr_n] = (char *)NULL;

	/* ################### EXPAND RANGES ######################*/

	int afirst = 0, asecond = 0, ranges_ok = 0;

	for (i = 0; substr[i]; i++) {
		/* Check if substr is a valid range */
		ranges_ok = 0;
		/* If range, get both extremes of it */
		for (j = 1; substr[i][j]; j++) {
			if (substr[i][j] == '-') {
				/* Get strings before and after the dash */
				char *first = strbfr(substr[i], '-');
				if (!first)
					break;
				char *second = straft(substr[i], '-');
				if (!second) {
					free(first);
					break;
				}
				/* Make sure it is a valid range */
				if (is_number(first) && is_number(second)) {
					afirst = atoi(first), asecond = atoi(second);
					if (asecond <= afirst) {
						free(first);
						free(second);
						break;
					}
					/* We have a valid range */
					ranges_ok = 1;
					free(first);
					free(second);
				}
				else {
					free(first);
					free(second);
					break;
				}
			}
		}

		if (!ranges_ok)
			continue;

		/* If a valid range */
		size_t k = 0, next = 0;
		char **rbuf = (char **)NULL;
		rbuf = (char **)xcalloc((substr_n + (size_t)(asecond - afirst)
								+ 1), sizeof(char *));
		/* Copy everything before the range expression
		 * into the buffer */
		for (j = 0; j < i; j++) {
			rbuf[k] = (char *)xcalloc(strlen(substr[j])+1,
									  sizeof(char));
			strcpy(rbuf[k++], substr[j]);
		}
		/* Copy the expanded range into the buffer */
		for (j = (size_t)afirst; j <= (size_t)asecond; j++) {
			rbuf[k] = (char *)xcalloc(digits_in_num((int)j) + 1,
									  sizeof(char));
			sprintf(rbuf[k++], "%zu", j);
		}
		/* Copy everything after the range expression into
		 * the buffer, if anything */
		if (substr[i+1]) {
			next = k;
			for (j = (i + 1); substr[j]; j++) {
				rbuf[k] = (char *)xcalloc(strlen(substr[j]) + 1,
										  sizeof(char));
				strcpy(rbuf[k++], substr[j]);
			}
		}
		else /* If there's nothing after last range, there's no next
		either */
			next = 0;

		/* Repopulate the original array with the expanded range and
		 * remaining strings */
		substr_n = k;
		for (j = 0; substr[j]; j++)
			free(substr[j]);
		substr = (char **)xrealloc(substr, (substr_n + 1)
								   * sizeof(char *));
		for (j = 0;j < substr_n; j++) {
			substr[j] = (char *)xcalloc(strlen(rbuf[j]) + 1,
										sizeof(char));
			strcpy(substr[j], rbuf[j]);
			free(rbuf[j]);
		}
		free(rbuf);

		substr[j] = (char *)NULL;

		/* Proceede only if there's something after the last range */
		if (next)
			i = next;
		else
			break;
	}

	/* ############## REMOVE DUPLICATES ###############*/

	char **dstr = (char **)NULL;
	size_t len = 0, d;
	for (i = 0;i < substr_n; i++) {
		short duplicate = 0;

		for (d = (i + 1); d < substr_n; d++) {
			if (strcmp(substr[i], substr[d]) == 0) {
				duplicate = 1;
				break;
			}
		}
		if (duplicate) {
			free(substr[i]);
			continue;
		}
		dstr = (char **)xrealloc(dstr, (len+1) * sizeof(char *));
		dstr[len] = (char *)xcalloc(strlen(substr[i])
									+ 1, sizeof(char));
		strcpy(dstr[len++], substr[i]);
		free(substr[i]);
	}
	free(substr);

	dstr = (char **)xrealloc(dstr, (len + 1) * sizeof(char *));
	dstr[len] = (char *)NULL;

	return dstr;
}

int
is_color_code(char *str)
/* Check if STR has the format of a color code string (a number or a
 * semicolon list (max 12 fields) of numbers of at most 3 digits each).
 * Returns 1 if true and 0 if false. */
{
	if(!str || !*str)
		return 0;

	size_t digits = 0, semicolon = 0;

	while(*str) {

		if (*str >= 0x30 && *str <= 0x39)
			digits++;

		else if (*str == ';') {
			if (*(str + 1) == ';') /* Consecutive semicolons */
				return 0;
			digits = 0;
			semicolon++;
		}

		else if (*str == '\n');

		else /* Neither digit nor semicolon */
			return 0;

		str++;
	}

	/* No digits at all, ending semicolon, more than eleven fields, or
	 * more than three consecutive digits */
	if (!digits || digits > 3 || semicolon > 11)
		return 0;

	/* At this point, we have a semicolon separated string of digits (3
	 * consecutive max) with at most 12 fields. The only thing not
	 * validated here are numbers themselves */

	return 1;
}

void
set_colors(void)
/* Open the config file, get values for filetype colors and copy these
 * values into the corresponnding filetype variable. If some value is
 * not found, or if it's a wrong value, the default is set. */
{
	char *dircolors = (char *)NULL;

	/* Get the colors line from the config file */
	FILE *fp_colors = fopen(CONFIG_FILE, "r");
	if (fp_colors) {
		char *line = (char *)NULL;
		ssize_t line_len = 0;
		size_t line_size = 0;

		while ((line_len = getline(&line, &line_size, fp_colors)) > 0) {
			if (strncmp(line, "FiletypeColors=", 15) == 0) {
				char *opt_str = straft(line, '=');
				if (!opt_str)
					continue;
				size_t str_len = strlen(opt_str);
				dircolors = (char *)xcalloc(str_len + 1, sizeof(char));
				if (opt_str[str_len-1] == '\n')
					opt_str[str_len-1] = 0x00;
				strcpy(dircolors, opt_str);
				free(opt_str);
				opt_str = (char *)NULL;
				break;
			}
		}

		free(line);
		line = (char *)NULL;

		fclose(fp_colors);
	}

	if (dircolors) {

		/* Set the LS_COLORS environment variable to use CliFM own
		 * colors. In this way, files listed for TAB completion will
		 * use CliFM colors instead of system colors */

		/* Strip CLiFM custom filetypes (nd, ne, nf, ed, ef, ee, and ca), 
		 * from dircolors to construct a valid value for LS_COLORS */
		size_t i = 0, buflen = 0, linec_len = strlen(dircolors);
		char *ls_buf = (char *)NULL;
		ls_buf = (char *)xcalloc(linec_len + 1, sizeof(char));
		while (dircolors[i]) {

			int rem = 0;
			if ((int)i < (int)(linec_len - 3) && (
			(dircolors[i] == 'n' && (dircolors[i+1] == 'd'
			|| dircolors[i+1] == 'e' || dircolors[i+1] == 'f'))
			|| (dircolors[i] == 'e' && (dircolors[i+1] == 'd'
			|| dircolors[i+1] == 'f' || dircolors[i+1] == 'e'))
			|| (dircolors[i] == 'c' && dircolors[i+1] == 'a') )
			&& dircolors[i+2] == '=') {
				rem = 1;
				for (i += 3; dircolors[i] && dircolors[i] != ':'; i++);
			}

			if (dircolors[i] && !rem && dircolors[i] != '"'
			&& dircolors[i] != '\'')
				ls_buf[buflen++] = dircolors[i];
			i++;
		}

		if (ls_buf) {
			ls_buf[buflen] = 0x00;
			if (setenv("LS_COLORS", ls_buf, 1) == -1)
				fprintf(stderr, _("%s: Error registering environment "
						          "colors\n"), PROGRAM_NAME);
			free(ls_buf);
			ls_buf = (char *)NULL;
		}

		/* Split the colors line into substrings (one per color) */
		char *p = dircolors, *buf = (char *)NULL, **colors = (char **)NULL;
		size_t len = 0, words = 0;
		while(*p) {
			switch (*p) {
			case '\'':
			case '"':
				p++;
				break;
			case ':':
				buf[len] = 0x00;
				colors = (char **)xrealloc(colors, (words + 1)
										   * sizeof(char *));
				colors[words] = (char *)xcalloc(len + 1, sizeof(char));
				strcpy(colors[words++], buf);
				memset(buf, 0x00, len);
				len = 0;
				p++;
				break;
			default:
				buf = (char *)xrealloc(buf, (len + 2) * sizeof(char));
				buf[len++] = *(p++);
				break;
			}
		}
		p = (char *)NULL;
		free(dircolors);
		dircolors = (char *)NULL;

		if (len) {
			buf[len] = 0x00;
			colors = (char **)xrealloc(colors, (words + 1)
									   * sizeof(char *));
			colors[words] = (char *)xcalloc(len + 1, sizeof(char));
			strcpy(colors[words++], buf);
		}

		if (buf) {
			free(buf);
			buf = (char *)NULL;
		}

		if (colors) {
			colors = (char **)xrealloc(colors, (words + 1)
									   * sizeof(char *));
			colors[words] = (char *)NULL;
		}

		/* Set the color variables */
		for (i = 0; colors[i]; i++) {
			if (strncmp(colors[i], "di=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					/* zero the corresponding variable as a flag for
					 * the check after this for loop and to prepare the
					 * variable to hold the default color */
					memset(di_c, 0x00, MAX_COLOR);
				else
					snprintf(di_c, MAX_COLOR - 1, "\x1b[%sm", colors[i]
							 + 3);
			else if (strncmp(colors[i], "nd=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					memset(nd_c, 0x00, MAX_COLOR);
				else
					snprintf(nd_c, MAX_COLOR - 1, "\x1b[%sm", colors[i]
							 + 3);
			else if (strncmp(colors[i], "ed=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					memset(ed_c, 0x00, MAX_COLOR);
				else
					snprintf(ed_c, MAX_COLOR - 1, "\x1b[%sm", colors[i]
							 + 3);
			else if (strncmp(colors[i], "ne=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					memset(ne_c, 0x00, MAX_COLOR);
				else
					snprintf(ne_c, MAX_COLOR - 1, "\x1b[%sm", colors[i]
							 + 3);
			else if (strncmp(colors[i], "fi=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					memset(fi_c, 0x00, MAX_COLOR);
				else
					snprintf(fi_c, MAX_COLOR - 1, "\x1b[%sm", colors[i]
							 + 3);
			else if (strncmp(colors[i], "ef=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					memset(ef_c, 0x00, MAX_COLOR);
				else
					snprintf(ef_c, MAX_COLOR - 1, "\x1b[%sm", colors[i]
							 + 3);
			else if (strncmp(colors[i], "nf=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					memset(nf_c, 0x00, MAX_COLOR);
				else
					snprintf(nf_c, MAX_COLOR - 1, "\x1b[%sm", colors[i]
							 + 3);
			else if (strncmp(colors[i], "ln=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					memset(ln_c, 0x00, MAX_COLOR);
				else
					snprintf(ln_c, MAX_COLOR - 1, "\x1b[%sm", colors[i]
							 + 3);
			else if (strncmp(colors[i], "or=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					memset(or_c, 0x00, MAX_COLOR);
				else
					snprintf(or_c, MAX_COLOR - 1, "\x1b[%sm", colors[i]
							 + 3);
			else if (strncmp(colors[i], "ex=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					memset(ex_c, 0x00, MAX_COLOR);
				else
					snprintf(ex_c, MAX_COLOR-1, "\x1b[%sm", colors[i]
							 + 3);
			else if (strncmp(colors[i], "ee=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					memset(ee_c, 0x00, MAX_COLOR);
				else
					snprintf(ee_c, MAX_COLOR - 1, "\x1b[%sm", colors[i]
							 + 3);
			else if (strncmp(colors[i], "bd=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					memset(bd_c, 0x00, MAX_COLOR);
				else
					snprintf(bd_c, MAX_COLOR - 1, "\x1b[%sm", colors[i]
							 + 3);
			else if (strncmp(colors[i], "cd=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					memset(cd_c, 0x00, MAX_COLOR);
				else
					snprintf(cd_c, MAX_COLOR - 1, "\x1b[%sm", colors[i]
							 + 3);
			else if (strncmp(colors[i], "pi=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					memset(pi_c, 0x00, MAX_COLOR);
				else
					snprintf(pi_c, MAX_COLOR - 1, "\x1b[%sm", colors[i]
							 + 3);
			else if (strncmp(colors[i], "so=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					memset(so_c, 0x00, MAX_COLOR);
				else
					snprintf(so_c, MAX_COLOR-1, "\x1b[%sm", colors[i]
							 + 3);
			else if (strncmp(colors[i], "su=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					memset(su_c, 0x00, MAX_COLOR);
				else
					snprintf(su_c, MAX_COLOR - 1, "\x1b[%sm", colors[i]
							 + 3);
			else if (strncmp(colors[i], "sg=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					memset(sg_c, 0x00, MAX_COLOR);
				else
					snprintf(sg_c, MAX_COLOR - 1, "\x1b[%sm", colors[i]
							 + 3);
			else if (strncmp(colors[i], "tw=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					memset(tw_c, 0x00, MAX_COLOR);
				else
					snprintf(tw_c, MAX_COLOR - 1, "\x1b[%sm", colors[i]
							 + 3);
			else if (strncmp(colors[i], "st=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					memset(st_c, 0x00, MAX_COLOR);
				else
					snprintf(st_c, MAX_COLOR - 1, "\x1b[%sm", colors[i]
							 + 3);
			else if (strncmp(colors[i], "ow=", 3) == 0)
				if (!is_color_code(colors[i]+3))
					memset(ow_c, 0x00, MAX_COLOR);
				else
					snprintf(ow_c, MAX_COLOR - 1, "\x1b[%sm", colors[i]
							 + 3);
			else if (strncmp(colors[i], "ca=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					memset(ca_c, 0x00, MAX_COLOR);
				else
					snprintf(ca_c, MAX_COLOR - 1, "\x1b[%sm", colors[i]
							 + 3);
			else if (strncmp(colors[i], "no=", 3) == 0) {
				if (!is_color_code(colors[i] + 3))
					memset(no_c, 0x00, MAX_COLOR);
				else
					snprintf(no_c, MAX_COLOR - 1, "\x1b[%sm", colors[i]
							 + 3);
			}
			free(colors[i]);
		}
		free(colors);
		colors = (char **)NULL;
	}

	/* If not dircolors */
	else {
		/* Set the LS_COLORS environment variable with default values */
		char lsc[] = "di=01;34:fi=00;97:ln=01;36:or=00;36:pi=00;35:"
				     "so=01;35:bd=01;33:cd=01;37:su=37;41:sg=30;43:"
				     "st=37;44:tw=30;42:ow=34;42:ex=01;32:no=31;47";
		if (setenv("LS_COLORS", lsc, 1) == -1)
			fprintf(stderr, _("%s: Error registering environment colors\n"),
				 	PROGRAM_NAME);
	}

	/* If some color was not set or it was a wrong color code, set the
	 * default */
	if (!*di_c)
		sprintf(di_c, "\x1b[01;34m");
	if (!*nd_c)
		sprintf(nd_c, "\x1b[01;31m");
	if (!*ed_c)
		sprintf(ed_c, "\x1b[00;34m");
	if (!*ne_c)
		sprintf(ne_c, "\x1b[00;31m");
	if (!*fi_c)
		sprintf(fi_c, "\x1b[00;97m");
	if (!*ef_c)
		sprintf(ef_c, "\x1b[00;33m");
	if (!*nf_c)
		sprintf(nf_c, "\x1b[00;31m");
	if (!*ln_c)
		sprintf(ln_c, "\x1b[01;36m");
	if (!*or_c)
		sprintf(or_c, "\x1b[00;36m");
	if (!*pi_c)
		sprintf(pi_c, "\x1b[00;35m");
	if (!*so_c)
		sprintf(so_c, "\x1b[01;35m");
	if (!*bd_c)
		sprintf(bd_c, "\x1b[01;33m");
	if (!*cd_c)
		sprintf(cd_c, "\x1b[01;37m");
	if (!*su_c)
		sprintf(su_c, "\x1b[37;41m");
	if (!*sg_c)
		sprintf(sg_c, "\x1b[30;43m");
	if (!*st_c)
		sprintf(st_c, "\x1b[37;44m");
	if (!*tw_c)
		sprintf(tw_c, "\x1b[30;42m");
	if (!*ow_c)
		sprintf(ow_c, "\x1b[34;42m");
	if (!*ex_c)
		sprintf(ex_c, "\x1b[01;32m");
	if (!*ee_c)
		sprintf(ee_c, "\x1b[00;32m");
	if (!*ca_c)
		sprintf(ca_c, "\x1b[30;41m");
	if (!*no_c)
		sprintf(no_c, "\x1b[31;47m");
}

void
set_default_options(void)
/* Set the default options. Used when the config file isn't accessible */
{
	splash_screen = 0;
	welcome_message = 1;
	show_hidden = 1;
	sort = 1;
	files_counter = 1;
	long_view = 0;
	ext_cmd_ok = 0;
	pager = 0;
	max_hist = 500;
	max_log = 1000;
	clear_screen = 0;
	list_folders_first = 1;
	cd_lists_on_the_fly = 1;
	case_sensitive = 0;
	unicode = 0;
	max_path = 40;
	logs_enabled = 0;
	div_line_char = '=';
	light_mode = 0;
	dir_indicator = 1;
	classify = 0;
	share_selbox = 0;
	sort = 1;
	sort_reverse = 0;
	tips = 1;
	autocd = 1;
	auto_open = 1;

	strcpy(text_color, "\001\x1b[00;39m\002");
	strcpy(eln_color, "\x1b[01;33m");
	strcpy(dir_count_color, "\x1b[00;97m");
	strcpy(default_color, "\x1b[00;39;49m");
	strcpy(div_line_color, "\x1b[00;34m");
	strcpy(welcome_msg_color, "\x1b[01;36m");
	sprintf(di_c, "\x1b[01;34m");
	sprintf(nd_c, "\x1b[01;31m");
	sprintf(ed_c, "\x1b[00;34m");
	sprintf(ne_c, "\x1b[00;31m");
	sprintf(fi_c, "\x1b[00;97m");
	sprintf(ef_c, "\x1b[00;33m");
	sprintf(nf_c, "\x1b[00;31m");
	sprintf(ln_c, "\x1b[01;36m");
	sprintf(or_c, "\x1b[00;36m");
	sprintf(pi_c, "\x1b[00;35m");
	sprintf(so_c, "\x1b[01;35m");
	sprintf(bd_c, "\x1b[01;33m");
	sprintf(cd_c, "\x1b[01;37m");
	sprintf(su_c, "\x1b[37;41m");
	sprintf(sg_c, "\x1b[30;43m");
	sprintf(st_c, "\x1b[37;44m");
	sprintf(tw_c, "\x1b[30;42m");
	sprintf(ow_c, "\x1b[34;42m");
	sprintf(ex_c, "\x1b[01;32m");
	sprintf(ee_c, "\x1b[00;32m");
	sprintf(ca_c, "\x1b[30;41m");
	sprintf(no_c, "\x1b[31;47m");

	/* Set the LS_COLORS environment variable */
	char lsc[] = "di=01;34:fi=00;97:ln=01;36:or=00;36:pi=00;35:"
			     "so=01;35:bd=01;33:cd=01;37:su=37;41:sg=30;43:"
			     "st=37;44:tw=30;42:ow=34;42:ex=01;32:no=31;47";
	if (setenv("LS_COLORS", lsc, 1) == -1) 
		fprintf(stderr, _("%s: Error registering environment colors\n"), 
				PROGRAM_NAME);

	if (sys_shell) {
		free(sys_shell);
		sys_shell = (char *)NULL;
	}

	sys_shell = get_sys_shell();

	if (!sys_shell) {
		sys_shell = (char *)xcalloc(strlen(FALLBACK_SHELL) + 1,
									sizeof(char));
		strcpy(sys_shell, FALLBACK_SHELL);
	}

	if (term)
		free(term);

	term = (char *)xcalloc(strlen(DEFAULT_TERM_CMD) + 1, sizeof(char));
	strcpy(term, DEFAULT_TERM_CMD);

	if (encoded_prompt)
		free(encoded_prompt);

	encoded_prompt = (char *)xcalloc(strlen(DEFAULT_PROMPT) + 1,
									 sizeof(char));
	strcpy(encoded_prompt, DEFAULT_PROMPT);
}

char *
escape_str(char *str)
/* Take a string and returns the same string escaped. If nothing to be
 * escaped, the original string is returned */
{
	if (!str)
		return (char *)NULL;

	size_t len = 0;
	char *buf = (char *)NULL;

	buf = (char *)xcalloc(strlen(str) * 2 + 1, sizeof(char));

	while(*str) {
		if (is_quote_char(*str))
			buf[len++] = '\\';
		buf[len++] = *(str++);
	}

	if (buf)
		return buf;

	return (char *)NULL;
}

int
is_internal(const char *cmd)
/* Check cmd against a list of internal commands. Used by parse_input_str()
 * to know if it should perform additional expansions. Only commands
 * dealing with filenames should be here */
{
	const char *int_cmds[] = { "o", "open", "cd", "p", "pr", "prop", "t",
							   "tr", "trash", "s", "sel", "mm", "mime",
							   "bm", "bookmarks", "br", "bulk", "ac",
							   "ad", NULL };
	short found = 0;
	size_t i;

	for (i = 0; int_cmds[i]; i++) {
		if (strcmp(cmd, int_cmds[i]) == 0) {
			found = 1;
			break;
		}
	}

	if (found) /* Check for the search function as well */
		return 1;
	else if (cmd[0] == '/' && access(cmd, F_OK) != 0)
		return 1;

	return 0;
}

int
quote_detector(char *line, int index)
/* Used by readline to check if a char in the string being completed is
 * quoted or not */
{
	if (index > 0 && line[index - 1] == '\\'
	&& !quote_detector(line, index - 1))
		return 1;

	return 0;
}

int
is_quote_char(char c)
/* Simply check a single chartacter (c) against the quoting characters
 * list defined in the qc global array (which takes its values from
 * rl_filename_quote_characters */
{
	if (c == 0x00 || !qc)
		return -1;

	char *p = qc;

	while (*p) {
		if (c == *p)
			return 1;
		p++;
	}

	return 0;
}

char **
split_str(char *str)
/* This function takes a string as argument and split it into substrings 
 * taking tab, new line char, and space as word delimiters, except when
 * they are preceded by a quote char (single or double quotes), in which
 * case eveything after the first quote char is taken as one single string.
 * It also allows escaping space to prevent word spliting. It returns an
 * array of splitted strings (without leading and terminating spaces) or
 * NULL if str is NULL or if no substring was found, i.e., if str contains
 * only spaces. */
{
	if (!str)
		return (char **)NULL;

	size_t buf_len = 0, words = 0, str_len = 0;
	char *buf = (char *)NULL;
	buf = (char *)xcalloc(1, sizeof(char));;
	short quote = 0;
	char **substr = (char **)NULL;

	while (*str) {
		switch (*str) {
		case '\'':
		case '"':
			/* If the quote is escaped, copy it into the buffer */
			if (str_len && *(str - 1) == '\\') {
				buf = (char *)xrealloc(buf, (buf_len + 1)
									   * sizeof(char *));
				buf[buf_len++] = *str;
				break;
			}

			/* If not escaped, move on to the next char */
			quote = *str;
			str++;

			/* Copy into the buffer whatever is after the first quote
			 * up to the last quote or NULL */
			while (*str && *str != quote) {
				if (is_quote_char(*str)) {
					buf = (char *)xrealloc(buf, (buf_len + 1)
										   * sizeof(char *));
					buf[buf_len++] = '\\';
				}
				buf = (char *)xrealloc(buf, (buf_len + 1)
									   * sizeof(char *));
				buf[buf_len++] = *(str++);
			}

			/* The above while breaks with NULL or quote, so that if
			 * *str is a null byte there was not terminating quote */
			if (!*str) {
				fprintf(stderr, _("%s: Missing '%c'\n"), PROGRAM_NAME,
						quote);

				/* Free the current buffer and whatever was already
				 * allocated */
				free(buf);
				buf = (char *)NULL;
				size_t i;
				for (i = 0;i < words; i++)
					free(substr[i]);
				free(substr);

				return (char **)NULL;
			}
			break;

		/* TAB, new line char, and space are taken as word breaking
		 * characters */
		case '\t':
		case '\n':
		case 0x20:

			/* If escaped, just copy it into the buffer */
			if (str_len && *(str - 1) == '\\') {
				buf = (char *)xrealloc(buf, (buf_len + 1)
									   * sizeof(char *));
				buf[buf_len++] = *str;
			}

			/* If not escaped, break the string */
			else {
				/* Add a terminating null byte to the buffer, and, if
				 * not empty, dump the buffer into the substrings
				 * array */
				buf[buf_len] = 0x00;
				if (buf_len > 0) {
					substr = (char **)xrealloc(substr, (words + 1)
											   * sizeof(char *));
					substr[words] = (char *)xcalloc(buf_len + 1,
													sizeof(char));
					strcpy(substr[words], buf);
					words++;
				}

				/* Clear te buffer to get a new string */
				memset(buf, 0x00, buf_len);
				buf_len = 0;
			}
			break;

		/* If neither a quote nor a breaking word char, just dump it
		 * into the buffer */
		default:
			buf = (char *)xrealloc(buf, (buf_len + 1) * sizeof(char *));
			buf[buf_len++] = *str;
			break;
		}
		str++;
		str_len++;
	}

	/* The while loop stops when the null byte is reached, so that the
	 * last substring is not printed, but still stored in the buffer.
	 * Therefore, we need to add it, if not empty, to our subtrings
	 * array */
	buf[buf_len] = 0x00;
	if (buf_len > 0) {
		if (!words)
			substr = (char **)xcalloc(words + 1, sizeof(char *));
		else
			substr = (char **)xrealloc(substr, (words + 1)
									   * sizeof(char *));
		substr[words] = (char *)xcalloc(buf_len + 1, sizeof(char));
		strcpy(substr[words], buf);
		words++;
	}

	free(buf);
	buf = (char *)NULL;

	if (words) {
		/* Add a final null string to the array */
		substr = (char **)xrealloc(substr, (words + 1)
								   * sizeof(char *));
		substr[words] = (char *)NULL;
		args_n = words - 1;
		return substr;
	}
	else {
		args_n = 0; /* Just in case, but I think it's not needed */
		return (char **)NULL;
	}
}

size_t
u8_xstrlen(const char *str)
/* An strlen implementation able to handle unicode characters. Taken from:
* https://stackoverflow.com/questions/5117393/number-of-character-cells-used-by-string
* Explanation: strlen() counts bytes, not chars. Now, since ASCII chars
* take each 1 byte, the amount of bytes equals the amount of chars.
* However, non-ASCII or wide chars are multibyte chars, that is, one char
* takes more than 1 byte, and this is why strlen() does not work as
* expected for this kind of chars: a 6 chars string might take 12 or
* more bytes */
{
	size_t len = 0;
	cont_bt = 0;

	while (*(str++))
		if ((*str & 0xc0) != 0x80)
		/* Do not count continuation bytes (used by multibyte, that is,
		 * wide or non-ASCII characters) */
			len++;
		else
			cont_bt++;

	return len;
}

int
open_function(char **cmd)
{
	if (!cmd)
		return EXIT_FAILURE;

	if (strchr(cmd[1], '\\')) {
		char *deq_path = (char *)NULL;
		deq_path = dequote_str(cmd[1], 0);

		if (!deq_path) {
			fprintf(stderr, _("%s: '%s': Error dequoting filename\n"),
					PROGRAM_NAME, cmd[1]);
			return EXIT_FAILURE;
		}

		strcpy(cmd[1], deq_path);
		free(deq_path);
	}

	/* Check file existence */
	struct stat file_attrib;

	if (stat(cmd[1], &file_attrib) == -1) {
		fprintf(stderr, "%s: open: '%s': %s\n", PROGRAM_NAME, cmd[1],
				strerror(errno));
		return EXIT_FAILURE;
	}

	/* Check file type: only directories, symlinks, and regular files
	 * will be opened */

	char no_open_file = 1, file_type[128] = "";
		 /* Reserve a good amount of bytes for filetype: it cannot be
		  * known beforehand how many bytes the TRANSLATED string will
		  * need */

	switch((file_attrib.st_mode & S_IFMT)) {
	case S_IFBLK:
		/* Store filetype to compose and print the error message, if 
		 * necessary */
		strcpy(file_type, _("block device"));
		break;

	case S_IFCHR:
		strcpy(file_type, _("character device"));
		break;

	case S_IFSOCK:
		strcpy(file_type, _("socket"));
		break;

	case S_IFIFO:
		strcpy(file_type, _("FIFO/pipe"));
		break;

	case S_IFDIR:
		return cd_function(cmd[1]);

	case S_IFREG:

		/* If an archive/compressed file, call archiver() */
		if (is_compressed(cmd[1], 1) == 0) {
			char *tmp_cmd[] = { "ad", cmd[1], NULL };
			return archiver(tmp_cmd, 'd');
		}

		no_open_file = 0;
		break;

	default:
		strcpy(file_type, _("unknown file type"));
		break;
	}

	/* If neither directory nor regular file nor symlink (to directory
	 * or regular file), print the corresponding error message and
	 * exit */
	if (no_open_file) {
		fprintf(stderr, _("%s: '%s' (%s): Cannot open file. Try "
				"'APPLICATION FILENAME'.\n"), PROGRAM_NAME,
				cmd[1], file_type);
		return EXIT_FAILURE;
	}

	/* At this point we know the file to be openend is either a regular
	 * file or a symlink to a regular file. So, just open the file */

	if (!cmd[2] || strcmp(cmd[2], "&") == 0) {

		if (!(flags & FILE_CMD_OK)) {
			fprintf(stderr, _("%s: 'file' command not found. Specify an "
							  "application to open the file\nUsage: "
							  "open ELN/FILENAME [APPLICATION]\n"), 
							  PROGRAM_NAME);
			return EXIT_FAILURE;
		}

		else {
			int ret = mime_open(cmd);

			/* The return value of mime_open could be zero
			 * (EXIT_SUCCESS), if success, one (EXIT_FAILURE) if error
			 * (in which case the following error message should be
			 * printed), and -1 if no access permission, in which case
			 * no error message should be printed, since the
			 * corresponding message is printed by mime_open itself */
			if (ret == EXIT_FAILURE) {
				fputs("Add a new entry to the mimelist file ('mime "
					  "edit') or run 'open FILE APPLICATION'\n",
					  stderr);
				return EXIT_FAILURE;
			}

			return EXIT_SUCCESS;
		}
	}

	/* If some application was specified to open the file */
	char *tmp_cmd[] = { cmd[2], cmd[1], NULL };

	int ret = launch_execve(tmp_cmd, (cmd[args_n]
							&& strcmp(cmd[args_n], "&") == 0)
							? BACKGROUND : FOREGROUND);

	if (ret != EXIT_SUCCESS)
		return EXIT_FAILURE;
	else
		return EXIT_SUCCESS;
}

int
cd_function(char *new_path)
/* Change CliFM working directory to NEW_PATH */
{
	int dequoted_p = 0;
	char *deq_path = (char *)NULL;
	char buf[PATH_MAX] = ""; /* Temporarily store new_path */

	/* dequote new_path, if necessary */
	if (strcntchr(new_path, '\\') != -1) {
		deq_path = dequote_str(new_path, 0);
		dequoted_p = 1; /* Only in this case deq_path should be freed */
	}
	else {
		deq_path = new_path;
		new_path = (char *)NULL;
	}

	/* If no argument, change to home */
	if (!deq_path || deq_path[0] == 0x00) {

		if (dequoted_p) {
			free(deq_path);
			deq_path = (char *)NULL;
		}

		if (user_home) {
			int ret = chdir(user_home);
			if (ret != 0) {
				fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME,
						user_home, strerror(errno));
				return EXIT_FAILURE;
			}
			else
				strncpy(buf, user_home, PATH_MAX);
		}
		else {
			fprintf(stderr, _("%s: Home directory not found\n"),
					PROGRAM_NAME);
			return EXIT_FAILURE;
		}
	}

	/* If we have some argument, resolve it with realpath(), cd into
	 * the resolved path, and set the path variable to this latter */
	else {

		char *real_path = realpath(deq_path, NULL);

		if (!real_path) {
			fprintf(stderr, "%s: cd: '%s': %s\n", PROGRAM_NAME,
					deq_path, strerror(errno));
			if (dequoted_p)
				free(deq_path);
			return EXIT_FAILURE;
		}

		/* Execute permission is enough to access a directory, but not
		 * to list its content; for this we need read permission as well.
		 * So, without the read permission check, chdir() below will be
		 * successfull, but CliFM will be nonetheless unable to list the
		 * content of the directory */
		if (access(real_path, R_OK) != 0) {
			fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME,
					real_path, strerror(errno));
			if (dequoted_p)
				free(deq_path);
			free(real_path);
			return EXIT_FAILURE;
		}

		int ret = chdir(real_path);
		if (ret != 0) {
			fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME,
					real_path, strerror(errno));
			if (dequoted_p)
				free(deq_path);
			free(real_path);
			return EXIT_FAILURE;
		}
		else
			strncpy(buf, real_path, PATH_MAX);
		free(real_path);
	}

	if (dequoted_p)
		free(deq_path);

	/* If chdir() was successful */
	free(path);
	path = (char *)xcalloc(strlen(buf) + 1, sizeof(char));
	strcpy(path, buf);
	add_to_dirhist(path);

	if (cd_lists_on_the_fly) {
		while (files--)
			free(dirlist[files]);
		list_dir();
	}

	return EXIT_SUCCESS;
}

int
back_function(char **comm)
/* Go back one element in dir hist */
{
	if (!comm)
		return EXIT_FAILURE;

	if (comm[1]) {
		if (strcmp(comm[1], "--help") == 0)
			puts(_("Usage: back, b [h, hist] [clear] [!ELN]"));
		else
			surf_hist(comm);
		return EXIT_SUCCESS;
	}
	/* If just 'back', with no arguments */
	/* If first path in current dirhist was reached, do nothing */
	if (dirhist_cur_index <= 0) {
/*		dirhist_cur_index=dirhist_total_index; */
		return EXIT_SUCCESS;
	}

	int ret = chdir(old_pwd[dirhist_cur_index - 1]);
	if (ret == 0) {
		free(path);
		path = (char *)xcalloc(strlen(old_pwd[dirhist_cur_index - 1])
							   + 1, sizeof(char));
		strcpy(path, old_pwd[--dirhist_cur_index]);
		if (cd_lists_on_the_fly) {
			while (files--) free(dirlist[files]);
			list_dir();
		}
	}
	else
		fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, 
				old_pwd[dirhist_cur_index - 1], strerror(errno));

	return EXIT_SUCCESS;
}

int
forth_function(char **comm)
/* Go forth one element in dir hist */
{
	if (!comm)
		return EXIT_FAILURE;

	if (comm[1]) {
		if (strcmp(comm[1], "--help") == 0)
			puts(_("Usage: forth, f [h, hist] [clear] [!ELN]"));
		else
			surf_hist(comm);
		return EXIT_SUCCESS;
	}

	/* If just 'forth', with no arguments */
	/* If last path in dirhist was reached, do nothing */
	if (dirhist_cur_index + 1 >= dirhist_total_index)
		return EXIT_SUCCESS;
	dirhist_cur_index++;

	int ret = chdir(old_pwd[dirhist_cur_index]);
	if (ret == 0) {
		free(path);
		path = (char *)xcalloc(strlen(old_pwd[dirhist_cur_index])
							   + 1, sizeof(char));
		strcpy(path, old_pwd[dirhist_cur_index]);
		if (cd_lists_on_the_fly) {
			while (files--)
				free(dirlist[files]);
			list_dir();
		}
	}
	else
		fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, 
				old_pwd[dirhist_cur_index], strerror(errno));

	return EXIT_SUCCESS;
}

int
list_mountpoints(void)
/* List available mountpoints and chdir into one of them */
{
	if (access("/proc/mounts", F_OK) != 0) {
		fprintf(stderr, "%s: mp: '/proc/mounts': %s\n",
				PROGRAM_NAME, strerror(errno));
		return EXIT_FAILURE;
	}

	FILE *mp_fp = fopen("/proc/mounts", "r");

	if (!mp_fp) {
		fprintf(stderr, "%s: mp: fopen: '/proc/mounts': %s\n",
				PROGRAM_NAME, strerror(errno));
		return EXIT_FAILURE;
	}

	printf(_("%sMountpoints%s\n\n"), white, NC);

	char **mountpoints = (char **)NULL;
	size_t mp_n = 0; 
	int exit_status = EXIT_SUCCESS;

	size_t line_size = 0;
	char *line = (char *)NULL;
	ssize_t line_len = 0;

	while ((line_len = getline(&line, &line_size, mp_fp)) > 0) {
		/* Do not list all mountpoints, but only those corresponding
		 * to a block device (/dev) */
		if (strncmp(line, "/dev/", 5) == 0) {
			char *str = (char *)NULL;
			size_t counter = 0;
			/* use strtok() to split LINE into tokens using space as
			 * IFS */
			str = strtok(line, " ");
			size_t dev_len = strlen(str);
			char *device = (char *)xnmalloc(dev_len + 1, sizeof(char));
			strcpy(device, str);
			/* Print only the first two fileds of each /proc/mounts
			 * line */
			while (str && counter < 2) {
				if (counter == 1) { /* 1 == second field */
					printf("%s%zu%s %s%s%s (%s)\n", eln_color, mp_n + 1,
						   NC, (access(str, R_OK|X_OK) == 0)
						   ? blue : red, str, default_color,
						   device);
					/* Store the second field (mountpoint) into an
					 * array */
					mountpoints = (char **)xrealloc(mountpoints, 
										   (mp_n + 1) * sizeof(char *));
					mountpoints[mp_n] = (char *)xcalloc(strlen(str)
												+ 1, sizeof(char));
					strcpy(mountpoints[mp_n++], str);
				}
				str = strtok(NULL, " ,");
				counter++;
			}

			free(device);
		}
	}

	free(line);
	line = (char *)NULL;
	fclose(mp_fp);

	/* This should never happen: There should always be a mountpoint,
	 * at least "/" */
	if (mp_n <= 0) {
		fputs(_("mp: There are no available mountpoints\n"), stdout);
		return EXIT_SUCCESS;
	}

	puts("");
	/* Ask the user and chdir into the selected mountpoint */
	char *input = (char *)NULL;
	while (!input)
		input = rl_no_hist(_("Choose a mountpoint ('q' to quit): "));

	if (!(*input == 'q' && *(input + 1) == 0x00)) {
		int atoi_num = atoi(input);
		if (atoi_num > 0 && atoi_num <= (int)mp_n) {
			int ret = chdir(mountpoints[atoi_num - 1]);
			if (ret == 0) {
				free(path);
				path = (char *)xcalloc(strlen(mountpoints[atoi_num - 1]) 
									   + 1, sizeof(char));
				strcpy(path, mountpoints[atoi_num - 1]);
				add_to_dirhist(path);
				if (cd_lists_on_the_fly) {
					while (files--)
						free(dirlist[files]);
					list_dir();
				}
			}
			else {
				fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, 
						mountpoints[atoi_num - 1], strerror(errno));
				exit_status = EXIT_FAILURE;
			}
		}
		else {
			printf(_("mp: '%s': Invalid mountpoint\n"), input);
			exit_status = EXIT_FAILURE;
		}
	}

	/* Free stuff and exit */
	if (input)
		free(input);

	size_t i;
	for (i = 0; i < (size_t)mp_n; i++)
		free(mountpoints[i]);

	free(mountpoints);

	return exit_status;
}

int
get_max_long_view(void)
/* Returns the max length a filename can have in long view mode */
{
	/* 70 is approx the length of the properties string (less filename).
	 * So, the amount of current terminal columns less 70 should be the
	 * space left to print the filename, that is, the max filename
	 * length */
	int max = (term_cols - 70);

	/* Do not allow max to be less than 20 (this is a more or less
	 * arbitrary value), specially because the result of the above
	 * operation could be negative */
	if (max < 20) max = 20;

	if ((int)longest < max)
		max = (int)longest;
	/* Should I specify a max length for trimmed filenames ?
	if (max > 50) max=50; */

	return max;
}

void
log_msg(char *_msg, int print)
/* Handle the error message 'msg'. Store 'msg' in an array of error
 * messages, write it into an error log file, and print it immediately
 * (if print is zero (NOPRINT_PROMPT) or tell the next prompt, if print
 * is one to do it (PRINT_PROMPT)). Messages wrote to the error log file
 * have the following format:
 * "[date] msg", where 'date' is YYYY-MM-DDTHH:MM:SS */
{
	if (!_msg)
		return;

	size_t msg_len = strlen(_msg);
	if (msg_len == 0)
		return;

	/* Store messages (for current session only) in an array, so that
	 * the user can check them via the 'msg' command */
	msgs_n++;
	messages = (char **)xrealloc(messages, (size_t)(msgs_n + 1)
								 * sizeof(char *));
	messages[msgs_n - 1] = (char *)xcalloc(msg_len + 1, sizeof(char));
	strcpy(messages[msgs_n - 1], _msg);
	messages[msgs_n] = (char *)NULL;

	if (print) /* PRINT_PROMPT */
		/* The next prompt will take care of printing the message */
		print_msg = 1;
	else /* NOPRINT_PROMPT */
		/* Print the message directly here */
		fputs(_msg, stderr);

	/* If the config dir cannot be found or if msg log file isn't set
	 * yet... This will happen if an error occurs before running
	 * init_config(), for example, if the user's home cannot be found */
	if (!config_ok || !MSG_LOG_FILE || *MSG_LOG_FILE == 0x00)
		return;

	FILE *msg_fp = fopen(MSG_LOG_FILE, "a");
	if (!msg_fp) {
		/* Do not log this error: We might incur in an infinite loop
		 * trying to access a file that cannot be accessed */
		fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, MSG_LOG_FILE, 
				strerror(errno));
		fputs("Press any key to continue... ", stdout);
		xgetchar(); puts("");
	}
	else {
		/* Write message to messages file: [date] msg */
		time_t rawtime = time(NULL);
		struct tm *tm = localtime(&rawtime);
		char date[64] = "";
		strftime(date, sizeof(date), "%b %d %H:%M:%S %Y", tm);
		fprintf(msg_fp, "[%d-%d-%dT%d:%d:%d] ", tm->tm_year + 1900, 
				tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, 
				tm->tm_sec);
		fputs(_msg, msg_fp);
		fclose(msg_fp);
	}
}

int *
expand_range(char *str, int listdir)
/* Expand a range of numbers given by str. It will expand the range
 * provided that both extremes are numbers, bigger than zero, equal or
 * smaller than the amount of files currently listed on the screen, and
 * the second (right) extreme is bigger than the first (left). Returns
 * an array of int's with the expanded range or NULL if one of the
 * above conditions is not met */
{
	if (strcntchr(str, '-') == -1) 
		return (int *)NULL;

	char *first = (char *)NULL;
	first = strbfr(str, '-');

	if (!first) 
		return (int *)NULL;

	if (!is_number(first)) {
		free(first);
		return (int *)NULL;
	}

	char *second = (char *)NULL;
	second = straft(str, '-');

	if (!second) {
		free(first);
		return (int *)NULL;
	}

	if (!is_number(second)) {
		free(first);
		free(second);
		return (int *)NULL;
	}

	int afirst = atoi(first), asecond = atoi(second);
	free(first);
	free(second);

	if (listdir) {
		if (afirst <= 0 || afirst > (int)files || asecond <= 0 
		|| asecond > (int)files || afirst >= asecond)
			return (int *)NULL;
	}
	else
		if (afirst >= asecond)
			return (int *)NULL;

	int *buf = (int *)NULL;
	buf = (int *)xcalloc((size_t)(asecond - afirst) + 2, sizeof(int));

	size_t i, j = 0;
	for (i = (size_t)afirst; i <= (size_t)asecond; i++)
		buf[j++] = (int)i;

	return buf;
}

int
recur_perm_check(const char *dirname)
/* Recursively check directory permissions (write and execute). Returns
 * zero if OK, and one if at least one subdirectory does not have
 * write/execute permissions */
{
	DIR *dir;
	struct dirent *entry;

	if (!(dir = opendir(dirname)))
		return EXIT_FAILURE;

	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_type == DT_DIR) {
			char dirpath[PATH_MAX] = "";
			if (strcmp(entry->d_name, ".") == 0 
					|| strcmp(entry->d_name, "..") == 0)
				continue;
			snprintf(dirpath, PATH_MAX, "%s/%s", dirname, entry->d_name);
			if (access(dirpath, W_OK|X_OK) != 0) {
				/* recur_perm_error_flag needs to be a global variable.
				  * Otherwise, since this function calls itself
				  * recursivelly, the flag would be reset upon every
				  * new call, without preserving the error code, which
				  * is what the flag is aimed to do. On the other side,
				  * if I use a local static variable for this flag, it
				  * will never drop the error value, and all subsequent
				  * calls to the function will allways return error
				  * (even if there's no actual error) */
				recur_perm_error_flag = 1;
				fprintf(stderr, _("'%s': Permission denied\n"), dirpath);
			}
			recur_perm_check(dirpath);
		}
	}
	closedir(dir);

	if (recur_perm_error_flag)
		return EXIT_FAILURE;
	else 
		return EXIT_SUCCESS;
}

int
wx_parent_check(char *file)
/* Check whether the current user has enough permissions (write, execute)
 * to modify the contents of the parent directory of 'file'. 'file' needs
 * to be an absolute path. Returns zero if yes and one if no. Useful to
 * know if a file can be removed from or copied into the parent. In case
 * FILE is a directory, the function checks all its subdirectories for
 * appropriate permissions, including the immutable bit */
{
	struct stat file_attrib;
	short exit_status = -1; 
	int ret = -1;
	size_t file_len = strlen(file);
	if (file[file_len - 1] == '/')
		file[file_len - 1] = 0x00;
	if (lstat(file, &file_attrib) == -1) {
		fprintf(stderr, _("'%s': No such file or directory\n"), file);
		return EXIT_FAILURE;
	}

	char *parent = strbfrlst(file, '/');
	if (!parent) {
		/* strbfrlst() will return NULL if file's parent is root (/),
		 * simply because in this case there's nothing before the last
		 * slash. So, check if file's parent dir is root */
		if (file[0] == '/' && strcntchr(file + 1, '/') == -1) {
			parent = (char *)xnmalloc(2, sizeof(char));
			parent[0] = '/';
			parent[1] = 0x00;
		}
		else {
			fprintf(stderr, _("%s: '%s': Error getting parent "
					"directory\n"), PROGRAM_NAME, file);
			return EXIT_FAILURE;
		}
	}

	switch (file_attrib.st_mode & S_IFMT) {

	/* DIRECTORY */
	case S_IFDIR:
		ret = check_immutable_bit(file);
		if (ret == -1) {
			/* Error message is printed by check_immutable_bit() itself */
			exit_status = EXIT_FAILURE;
		}
		else if (ret == 1) {
			fprintf(stderr, _("'%s': Directory is immutable\n"), file);
			exit_status = EXIT_FAILURE;
		}
		/* Check the parent for appropriate permissions */
		else if (access(parent, W_OK|X_OK) == 0) {
			size_t files_n = count_dir(parent);
			if (files_n > 2) {
				/* I manually check here subdir because recur_perm_check() 
				 * will only check the contents of subdir, but not subdir 
				 * itself */
				/* If the parent is ok and not empty, check subdir */
				if (access(file, W_OK|X_OK) == 0) {
					/* If subdir is ok and not empty, recusivelly check 
					 * subdir */
					files_n = count_dir(file);
					if (files_n > 2) {
						/* Reset the recur_perm_check() error flag. See 
						 * the note in the function block. */
						recur_perm_error_flag = 0;
						if (recur_perm_check(file) == 0) {
							exit_status = EXIT_SUCCESS;
						}
						else
							/* recur_perm_check itself will print the 
							 * error messages */
							exit_status = EXIT_FAILURE;
					}
					else /* Subdir is ok and empty */
						exit_status = EXIT_SUCCESS;
				}
				else { /* No permission for subdir */
					fprintf(stderr, _("'%s': Permission denied\n"),
							file);
					exit_status = EXIT_FAILURE;
				}
			}
			else
				exit_status = EXIT_SUCCESS;
		}
		else { /* No permission for parent */
			fprintf(stderr, _("'%s': Permission denied\n"), parent);
			exit_status = EXIT_FAILURE;
		}
		break;

	/* REGULAR FILE */
	case S_IFREG:
		ret=check_immutable_bit(file);
		if (ret == -1) {
			/* Error message is printed by check_immutable_bit()
			 * itself */
			exit_status = EXIT_FAILURE;
		}
		else if (ret == 1) {
			fprintf(stderr, _("'%s': File is immutable\n"), file);
			exit_status = EXIT_FAILURE;
		}
		else if (parent) {
			if (access(parent, W_OK|X_OK) == 0)
				exit_status = EXIT_SUCCESS;
			else {
				fprintf(stderr, _("'%s': Permission denied\n"), parent);
				exit_status = EXIT_FAILURE;
			}
		}
		break;

	/* SYMLINK, SOCKET, AND FIFO PIPE */
	case S_IFSOCK:
	case S_IFIFO:
	case S_IFLNK:
		/* Symlinks, sockets and pipes do not support immutable bit */
		if (parent) {
			if (access(parent, W_OK|X_OK) == 0)
				exit_status = EXIT_SUCCESS;
			else {
				fprintf(stderr, _("'%s': Permission denied\n"), parent);
				exit_status = EXIT_FAILURE;
			}
		}
		break;

	/* DO NOT TRASH BLOCK AND CHAR DEVICES */
	default:
		fprintf(stderr, _("%s: trash: '%s' (%s): Unsupported file type\n"), 
				PROGRAM_NAME, file, ((file_attrib.st_mode & S_IFMT) == S_IFBLK)
				? "Block device" : ((file_attrib.st_mode & S_IFMT) == S_IFCHR)
				? "Character device" : "Unknown filetype");
		exit_status = EXIT_FAILURE;
		break;
	}

	if (parent)
		free(parent);

	return exit_status;
}

int
trash_element(const char *suffix, struct tm *tm, char *file)
{
	/* Check file's existence */
	struct stat file_attrib;

	if (lstat(file, &file_attrib) == -1) {
		fprintf(stderr, "%s: trash: '%s': %s\n", PROGRAM_NAME, file, 
				strerror(errno));
		return EXIT_FAILURE;
	}

	/* Check whether the user has enough permissions to remove file */
	/* If relative path */
	char full_path[PATH_MAX] = "";

	if (*file != '/') {
		/* Construct absolute path for file */
		snprintf(full_path, PATH_MAX, "%s/%s", path, file);
		if (wx_parent_check(full_path) != 0)
			return EXIT_FAILURE;
	}

	/* If absolute path */
	else if (wx_parent_check(file) != 0)
		return EXIT_FAILURE;

	int ret = -1;

	/* Create the trashed file name: orig_filename.suffix, where SUFFIX is
	 * current date and time */
	char *filename = (char *)NULL;

	if (*file != '/') /* If relative path */
		filename = straftlst(full_path, '/');
	else /* If absolute path */
		filename = straftlst(file, '/');

	if (!filename) {
		fprintf(stderr, _("%s: trash: '%s': Error getting filename\n"), 
				PROGRAM_NAME, file);
		return EXIT_FAILURE;
	}
	/* If the length of the trashed file name (orig_filename.suffix) is 
	 * longer than NAME_MAX (255), trim the original filename, so that
	 * (original_filename_len + 1 (dot) + suffix_len) won't be longer
	 * than NAME_MAX */
	size_t filename_len = strlen(filename), suffix_len = strlen(suffix);
	int size = (int)(filename_len + suffix_len + 1) - NAME_MAX;

	if (size > 0) {
		/* If SIZE is a positive value, that is, the trashed file name 
		 * exceeds NAME_MAX by SIZE bytes, reduce the original file name 
		 * SIZE bytes. Terminate the original file name (FILENAME) with
		 * a tilde (~), to let the user know it is trimmed */
		filename[filename_len - (size_t)size - 1] = '~';
		filename[filename_len - (size_t)size] = 0x00;
	}

											/* 2 = dot + null byte */
	size_t file_suffix_len = filename_len + suffix_len + 2;
	char *file_suffix = (char *)xnmalloc(file_suffix_len, sizeof(char));
	/* No need for memset. sprintf adds the terminating null byte by
	 * itself */
	sprintf(file_suffix, "%s.%s", filename, suffix);

	/* Copy the original file into the trash files directory */
	char *dest = (char *)NULL;
	dest = (char *)xnmalloc(strlen(TRASH_FILES_DIR)
							+ strlen(file_suffix) + 2,
							sizeof(char));
	sprintf(dest, "%s/%s", TRASH_FILES_DIR, file_suffix);

	char *tmp_cmd[] = { "cp", "-a", file, dest, NULL };

	free(filename);

	ret = launch_execve(tmp_cmd, FOREGROUND);
	free(dest);
	dest = (char *)NULL;

	if (ret != EXIT_SUCCESS) {
		fprintf(stderr, _("%s: trash: '%s': Failed copying file to "
				"Trash\n"), PROGRAM_NAME, file);
		free(file_suffix);
		return EXIT_FAILURE;
	}

	/* Generate the info file */
	size_t info_file_len = strlen(TRASH_INFO_DIR)
								  + strlen(file_suffix)
								  + 12;
	char *info_file = (char *)xnmalloc(info_file_len, sizeof(char));
	sprintf(info_file, "%s/%s.trashinfo", TRASH_INFO_DIR, file_suffix);

	FILE *info_fp = fopen(info_file, "w");
	if (!info_fp) { /* If error creating the info file */
		fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, info_file, 
				strerror(errno));
		/* Remove the trash file */
		char *trash_file = (char *)NULL;
		trash_file = (char *)xnmalloc(strlen(TRASH_FILES_DIR) 
									  + strlen(file_suffix) + 2,
									  sizeof(char));
		sprintf(trash_file, "%s/%s", TRASH_FILES_DIR, file_suffix);
		char *tmp_cmd2[] = { "rm", "-r", trash_file, NULL };
		ret = launch_execve (tmp_cmd2, FOREGROUND);
		free(trash_file);
		if (ret != EXIT_SUCCESS)
			fprintf(stderr, _("%s: trash: '%s/%s': Failed removing trash "
					"file\nTry removing it manually\n"), PROGRAM_NAME, 
					TRASH_FILES_DIR, file_suffix);
		free(file_suffix);
		free(info_file);
		return EXIT_FAILURE;
	}
	else { /* If info file was generated successfully */
		/* Encode path to URL format (RF 2396) */
		char *url_str = (char *)NULL;

		if (*file != '/')
			url_str = url_encode(full_path);
		else
			url_str = url_encode(file);

		if (!url_str) {
			fprintf(stderr, _("%s: trash: '%s': Failed encoding path\n"), 
					PROGRAM_NAME, file);
			fclose(info_fp);

			free(info_file);
			free(file_suffix);
			return EXIT_FAILURE;
		}

		/* Write trashed file information into the info file */
		fprintf(info_fp, 
				"[Trash Info]\nPath=%s\nDeletionDate=%d%d%dT%d:%d:%d\n", 
				url_str, tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, 
				tm->tm_hour, tm->tm_min, tm->tm_sec);
		fclose(info_fp);
		free(url_str);
		url_str = (char *)NULL;
	}

	/* Remove the file to be trashed */
	char *tmp_cmd3[] = { "rm", "-r", file, NULL };
	ret = launch_execve(tmp_cmd3, FOREGROUND);
	/* If remove fails, remove trash and info files */
	if (ret != EXIT_SUCCESS) {
		fprintf(stderr, _("%s: trash: '%s': Failed removing file\n"), 
				PROGRAM_NAME, file);
		char *trash_file = (char *)NULL;
		trash_file = (char *)xnmalloc(strlen(TRASH_FILES_DIR) 
									 + strlen(file_suffix) + 2,
									 sizeof(char));
		sprintf(trash_file, "%s/%s", TRASH_FILES_DIR, file_suffix);

		char *tmp_cmd4[] = { "rm", "-r", trash_file, info_file, NULL };
		ret = launch_execve(tmp_cmd4, FOREGROUND);
		free(trash_file);

		if (ret != EXIT_SUCCESS) {
			fprintf(stderr, _("%s: trash: Failed removing temporary "
							  "files from Trash.\nTry removing them "
							  "manually\n"), PROGRAM_NAME);
			free(file_suffix);
			free(info_file);
			return EXIT_FAILURE;
		}
	}

	free(info_file);
	free(file_suffix);
	return EXIT_SUCCESS;
}

int
remove_from_trash(void)
{
	/* List trashed files */
	/* Change CWD to the trash directory. Otherwise, scandir() will fail */
	if (chdir(TRASH_FILES_DIR) == -1) {
		_err(0, NOPRINT_PROMPT, "%s: trash: '%s': %s\n", PROGRAM_NAME, 
			 TRASH_FILES_DIR, strerror(errno));
		return EXIT_FAILURE;
	}

	size_t i = 0;
	struct dirent **trash_files = (struct dirent **)NULL;
	int files_n = scandir(TRASH_FILES_DIR, &trash_files,
						  skip_implied_dot, (unicode) ? alphasort
						  : (case_sensitive) ? xalphasort
						  : alphasort_insensitive);

	if (files_n) {
		printf(_("%sTrashed files%s%s\n\n"), white, NC, default_color);
		for (i = 0; i < (size_t)files_n; i++)
			colors_list(trash_files[i]->d_name, (int)i + 1, 0, 1);
	}
	else {
		puts(_("trash: There are no trashed files"));
		if (chdir(path) == -1) { /* Restore CWD and return */
			_err(0, NOPRINT_PROMPT, "%s: trash: '%s': %s\n",
				 PROGRAM_NAME, path, strerror(errno));
		}
		return EXIT_SUCCESS;
	}

	/* Restore CWD and continue */
	if (chdir(path) == -1) {
		_err(0, NOPRINT_PROMPT, "%s: trash: '%s': %s\n", PROGRAM_NAME, 
			 path, strerror(errno));
		return EXIT_FAILURE;
	}

	/* Get user input */
	printf(_("\n%s%sEnter 'q' to quit.\n"), NC, default_color);
	char *line = (char *)NULL, **rm_elements = (char **)NULL;

	while (!line)
		line = rl_no_hist(_("Element(s) to be removed "
							"(ex: 1 2-6, or *): "));

	rm_elements = get_substr(line, 0x20);
	free(line);

	if (!rm_elements)
		return EXIT_FAILURE;

	/* Remove files */
	char rm_file[PATH_MAX] = "", rm_info[PATH_MAX] = "";
	int ret = -1, exit_status = EXIT_SUCCESS;

	/* First check for exit, wildcard, and non-number args */
	for (i = 0; rm_elements[i]; i++) {

		/* Quit */
		if (strcmp(rm_elements[i], "q") == 0) {
			size_t j;

			for (j = 0; rm_elements[j]; j++)
				free(rm_elements[j]);
			free(rm_elements);

			for (j = 0; j < (size_t)files_n; j++)
				free(trash_files[j]);
			free(trash_files);

			return exit_status;
		}

		/* Asterisk */
		else if (strcmp(rm_elements[i], "*") == 0) {
			size_t j;
			for (j = 0; j < (size_t)files_n; j++) {

				snprintf(rm_file, PATH_MAX, "%s/%s", TRASH_FILES_DIR,
						 trash_files[j]->d_name);
				snprintf(rm_info, PATH_MAX, "%s/%s.trashinfo",
						 TRASH_INFO_DIR, trash_files[j]->d_name);
				char *tmp_cmd[] = { "rm", "-r", rm_file, rm_info, NULL };
				ret = launch_execve(tmp_cmd, FOREGROUND);

				if (ret != EXIT_SUCCESS) {
					fprintf(stderr, _("%s: trash: Error trashing %s\n"), 
							 PROGRAM_NAME, trash_files[j]->d_name);
					exit_status = EXIT_FAILURE;
				}

				free(trash_files[j]);
			}

			free(trash_files);

			for (j = 0; rm_elements[j]; j++)
				free(rm_elements[j]);	
			free(rm_elements);

			return exit_status;
		}

		else if (!is_number(rm_elements[i])) {

			fprintf(stderr, _("%s: trash: '%s': Invalid ELN\n"),
					PROGRAM_NAME, rm_elements[i]);
			exit_status = EXIT_FAILURE;

			size_t j;

			for (j = 0; rm_elements[j]; j++)
				free(rm_elements[j]);
			free(rm_elements);

			for (j = 0; j < (size_t)files_n; j++)
				free(trash_files[j]);
			free(trash_files);

			return exit_status;
		}
	}

	/* If all args are numbers, and neither 'q' nor wildcard */
	int rm_num;
	for (i = 0; rm_elements[i]; i++) {

		rm_num = atoi(rm_elements[i]);
		if (rm_num <= 0 || rm_num > files_n) {
			fprintf(stderr, _("%s: trash: '%d': Invalid ELN\n"),
					PROGRAM_NAME, rm_num);
			free(rm_elements[i]);
			exit_status = EXIT_FAILURE;
			continue;
		}

		snprintf(rm_file, PATH_MAX, "%s/%s", TRASH_FILES_DIR, 
				 trash_files[rm_num - 1]->d_name);
		snprintf(rm_info, PATH_MAX, "%s/%s.trashinfo", TRASH_INFO_DIR, 
				 trash_files[rm_num - 1]->d_name);
		char *tmp_cmd2[] = { "rm", "-r", rm_file, rm_info, NULL };
		ret = launch_execve(tmp_cmd2, FOREGROUND);

		if (ret != EXIT_SUCCESS) {
			fprintf(stderr, _("%s: trash: Error trashing %s\n"),
					PROGRAM_NAME, trash_files[rm_num - 1]->d_name);
			exit_status = EXIT_FAILURE;
		}

		free(rm_elements[i]);
	}

	free(rm_elements);

	for (i = 0; i < (size_t)files_n; i++)
		free(trash_files[i]);
	free(trash_files);

	return exit_status;
}

int
untrash_element(char *file)
{
	if (!file)
		return EXIT_FAILURE;

	char undel_file[PATH_MAX] = "", undel_info[PATH_MAX] = "";
	snprintf(undel_file, PATH_MAX, "%s/%s", TRASH_FILES_DIR, file);
	snprintf(undel_info, PATH_MAX, "%s/%s.trashinfo", TRASH_INFO_DIR,
			 file);

	FILE *info_fp;
	info_fp = fopen(undel_info, "r");
	if (info_fp) {
		char *orig_path = (char *)NULL;
		/* The max length for line is Path=(5) + PATH_MAX + \n(1) */
		char line[PATH_MAX + 6];
		memset(line, 0x00, PATH_MAX + 6);
		while (fgets(line, sizeof(line), info_fp))
			if (strncmp(line, "Path=", 5) == 0)
				orig_path = straft(line, '=');
		fclose (info_fp);

		/* If original path is NULL or empty, return error */
		if (!orig_path)
			return EXIT_FAILURE;

/*		if (strcmp(orig_path, "") == 0) { */
		if (*orig_path == 0x00) {
			free(orig_path);
			return EXIT_FAILURE;
		}

		/* Remove new line char from original path, if any */
		size_t orig_path_len = strlen(orig_path);
		if (orig_path[orig_path_len - 1] == '\n')
			orig_path[orig_path_len - 1] = 0x00;

		/* Decode original path's URL format */
		char *url_decoded = url_decode(orig_path);
		if (!url_decoded) {
			fprintf(stderr, _("%s: undel: '%s': Failed decoding path\n"), 
					PROGRAM_NAME, orig_path);
			free(orig_path);
			return EXIT_FAILURE;
		}

		free(orig_path);
		orig_path = (char *)NULL;

		/* Check existence and permissions of parent directory */
		char *parent = (char *)NULL;
		parent = strbfrlst(url_decoded, '/');

		if (!parent) {
			/* strbfrlst() returns NULL is file's parent is root (simply
			 * because there's nothing before last slash in this case).
			 * So, check if file's parent is root. Else returns */
			if (url_decoded[0] == '/'
			&& strcntchr(url_decoded + 1, '/') == -1) {
				parent = (char *)xnmalloc(2, sizeof(char));
				parent[0] = '/';
				parent[1] = 0x00;
			}
			else {
				free(url_decoded);
				return EXIT_FAILURE;
			}
		}

		if (access(parent, F_OK) != 0) {
			fprintf(stderr, _("%s: undel: '%s': No such file or "
					"directory\n"), PROGRAM_NAME, parent);
			free(parent);
			free(url_decoded);
			return EXIT_FAILURE;
		}

		if (access(parent, X_OK|W_OK) != 0) {
			fprintf(stderr, _("%s: undel: '%s': Permission denied\n"), 
					PROGRAM_NAME, parent);
			free(parent);
			free(url_decoded);
			return EXIT_FAILURE;
		}

		free(parent);

		char *tmp_cmd[] = { "cp", "-a", undel_file, url_decoded, NULL };

		int ret = -1;
		ret = launch_execve(tmp_cmd, FOREGROUND);
		free(url_decoded);

		if (ret == EXIT_SUCCESS) {
			char *tmp_cmd2[] = { "rm", "-r", undel_file, undel_info,
								  NULL };
			ret = launch_execve(tmp_cmd2, FOREGROUND);
			if (ret != EXIT_SUCCESS) {
				fprintf(stderr, _("%s: undel: '%s': Failed removing "
								  "info file\n"), PROGRAM_NAME,
								  undel_info);
				return EXIT_FAILURE;
			}
			else
				return EXIT_SUCCESS;
		}
		else {
			fprintf(stderr, _("%s: undel: '%s': Failed restoring trashed "
							  "file\n"), PROGRAM_NAME, undel_file);
			return EXIT_FAILURE;
		}
	}

	else { /* !info_fp */
		fprintf(stderr, _("%s: undel: Info file for '%s' not found. "
						  "Try restoring the file manually\n"),
						  PROGRAM_NAME, file);
		return EXIT_FAILURE;
	}

	return EXIT_FAILURE; /* Never reached */
}

int
untrash_function(char **comm)
{
	if (!comm)
		return EXIT_FAILURE;

	if (!trash_ok) {
		fprintf(stderr, _("%s: Trash function disabled\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	/* Change CWD to the trash directory to make scandir() work */
	if (chdir(TRASH_FILES_DIR) == -1) {
		_err(0, NOPRINT_PROMPT, "%s: undel: '%s': %s\n", PROGRAM_NAME, 
			 TRASH_FILES_DIR, strerror(errno));
		return EXIT_FAILURE;
	}

	/* Get trashed files */
	struct dirent **trash_files = (struct dirent **)NULL;
	int trash_files_n = scandir(TRASH_FILES_DIR, &trash_files,
								skip_implied_dot, (unicode) ? alphasort
								: (case_sensitive) ? xalphasort
								: alphasort_insensitive);
	if (trash_files_n <= 0) {
		puts(_("trash: There are no trashed files"));
		if (chdir(path) == -1) {
			_err(0, NOPRINT_PROMPT, "%s: undel: '%s': %s\n",
				 PROGRAM_NAME, path, strerror(errno));
			return EXIT_FAILURE;
		}
		return EXIT_SUCCESS;
	}

	int exit_status = EXIT_SUCCESS;
	/* if "undel all" (or "u a" or "u *") */
	if (comm[1] && (strcmp(comm[1], "*") == 0
	|| strcmp(comm[1], "a") == 0 || strcmp(comm[1], "all") == 0)) {
		size_t j;
		for (j = 0; j < (size_t)trash_files_n; j++) {
			if (untrash_element(trash_files[j]->d_name) != 0)
				exit_status = EXIT_FAILURE;
			free(trash_files[j]);
		}
		free(trash_files);

		if (chdir(path) == -1) {
			_err(0, NOPRINT_PROMPT, "%s: undel: '%s': %s\n",
				 PROGRAM_NAME, path, strerror(errno));
			return EXIT_FAILURE;
		}

		return exit_status;
	}

	/* List trashed files */
	printf(_("%sTrashed files%s%s\n\n"), white, NC, default_color);
	size_t i;
	for (i = 0; i < (size_t)trash_files_n; i++)
		colors_list(trash_files[i]->d_name, (int)i + 1, 0, 1);

	/* Go back to previous path */
	if (chdir(path) == -1) {
		_err(0, NOPRINT_PROMPT, "%s: undel: '%s': %s\n", PROGRAM_NAME,
			 path, strerror(errno));
		return EXIT_FAILURE;
	}

	/* Get user input */
	printf(_("\n%s%sEnter 'q' to quit.\n"), NC, default_color);
	int undel_n = 0;
	char *line = (char *)NULL, **undel_elements = (char **)NULL;
	while (!line)
		line = rl_no_hist(_("Element(s) to be undeleted "
						  "(ex: 1 2-6, or *): "));

	undel_elements = get_substr(line, 0x20);
	free(line);
	line = (char *)NULL;
	if (undel_elements)
		for (i = 0; undel_elements[i]; i++)
			undel_n++;
	else
		return EXIT_FAILURE;

	/* First check for quit, *, and non-number args */
	short free_and_return = 0;
	for(i = 0; i < (size_t)undel_n; i++) {
		if (strcmp(undel_elements[i], "q") == 0)
			free_and_return = 1;
		else if (strcmp(undel_elements[i], "*") == 0) {
			size_t j;
			for (j = 0; j < (size_t)trash_files_n; j++)
				if (untrash_element(trash_files[j]->d_name) != 0)
					exit_status = EXIT_FAILURE;
			free_and_return = 1;
		}
		else if (!is_number(undel_elements[i])) {
			fprintf(stderr, _("undel: '%s': Invalid ELN\n"), 
					undel_elements[i]);
			exit_status = EXIT_FAILURE;
			free_and_return = 1;
		}
	} 
	/* Free and return if any of the above conditions is true */
	if (free_and_return) {
		size_t j = 0;
		for (j = 0; j < (size_t)undel_n; j++)
			free(undel_elements[j]);
		free(undel_elements);
		for (j = 0; j < (size_t)trash_files_n; j++)
			free(trash_files[j]);
		free(trash_files);
		return exit_status;
	}

	/* Undelete trashed files */
	int undel_num;
	for(i = 0; i < (size_t)undel_n; i++) {
		undel_num = atoi(undel_elements[i]);
		if (undel_num <= 0 || undel_num > trash_files_n) {
			fprintf(stderr, _("%s: undel: '%d': Invalid ELN\n"),
					PROGRAM_NAME, undel_num);
			free(undel_elements[i]);
			continue;
		}

		/* If valid ELN */
		if (untrash_element(trash_files[undel_num - 1]->d_name) != 0)
			exit_status = EXIT_FAILURE;
		free(undel_elements[i]);
	}
	free(undel_elements);

	/* Free trashed files list */
	for (i = 0; i < (size_t)trash_files_n; i++)
		free(trash_files[i]);
	free(trash_files);

	/* If some trashed file still remains, reload the undel screen */
	trash_n = count_dir(TRASH_FILES_DIR);
	if (trash_n <= 2)
		trash_n = 0;
	if (trash_n)
		untrash_function(comm);

	return exit_status;
}

int
trash_clear(void)
{
	struct dirent **trash_files = (struct dirent **)NULL;
	int files_n = -1, exit_status = EXIT_SUCCESS;

	if (chdir(TRASH_FILES_DIR) == -1) {
		_err(0, NOPRINT_PROMPT, "%s: trash: '%s': %s\n", PROGRAM_NAME, 
			 TRASH_FILES_DIR, strerror(errno));
		return EXIT_FAILURE;
	}

	files_n = scandir(TRASH_FILES_DIR, &trash_files, skip_implied_dot, 
					  xalphasort);

	if (!files_n) {
		puts(_("trash: There are no trashed files"));
		return EXIT_SUCCESS;
	}

	size_t i;
	for (i = 0; i < (size_t)files_n; i++) {
		size_t info_file_len = strlen(trash_files[i]->d_name) + 11;
		char *info_file = (char *)xnmalloc(info_file_len, sizeof(char));
		sprintf(info_file, "%s.trashinfo", trash_files[i]->d_name);

		char *file1 = (char *)NULL;
		file1 = (char *)xnmalloc(strlen(TRASH_FILES_DIR) +
					    strlen(trash_files[i]->d_name) + 2, sizeof(char));
		sprintf(file1, "%s/%s", TRASH_FILES_DIR, trash_files[i]->d_name);

		char *file2 = (char *)NULL;
		file2 = (char *)xnmalloc(strlen(TRASH_INFO_DIR) +
					    strlen(info_file) + 2, sizeof(char));
		sprintf(file2, "%s/%s", TRASH_INFO_DIR, info_file);

		char *tmp_cmd[] = { "rm", "-r", file1, file2, NULL };
		int ret = launch_execve(tmp_cmd, FOREGROUND);
		free(file1);
		free(file2);

		if (ret != EXIT_SUCCESS) {
			fprintf(stderr, _("%s: trash: '%s': Error removing "
					"trashed file\n"), PROGRAM_NAME,
					trash_files[i]->d_name);
			exit_status = EXIT_FAILURE;
			/* If there is at least one error, return error */
		}

		free(info_file);
		free(trash_files[i]);
	}
	free(trash_files);

	if (chdir(path) == -1) {
		_err(0, NOPRINT_PROMPT, "%s: trash: '%s': %s\n", PROGRAM_NAME,
			 path, strerror(errno));
		return EXIT_FAILURE;
	}

	return exit_status;
}

int
trash_function (char **comm)
{
	/* Create trash dirs, if necessary */
/*	struct stat file_attrib;
	if (stat (TRASH_DIR, &file_attrib) == -1) {
		char *trash_files = NULL;
		trash_files = xcalloc(strlen(TRASH_DIR) + 7, sizeof(char));
		sprintf(trash_files, "%s/files", TRASH_DIR);
		char *trash_info=NULL;
		trash_info = xcalloc(strlen(TRASH_DIR) + 6, sizeof(char));
		sprintf(trash_info, "%s/info", TRASH_DIR);
		char *cmd[] = { "mkdir", "-p", trash_files, trash_info, NULL };
		int ret = launch_execve (cmd, FOREGROUND);
		free(trash_files);
		free(trash_info);
		if (ret != EXIT_SUCCESS) {
			_err(0, NOPRINT_PROMPT, _("%s: mkdir: '%s': Error creating "
			 	 "trash directory\n"), PROGRAM_NAME, TRASH_DIR);
			return;
		}
	} */

	if (!comm)
		return EXIT_FAILURE;

	if (!trash_ok) {
		fprintf(stderr, _("%s: Trash function disabled\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	/* List trashed files ('tr' or 'tr ls') */
	if (!comm[1] || strcmp(comm[1], "ls") == 0 
			|| strcmp(comm[1], "list") == 0) {
		/* List files in the Trash/files dir */

		if (chdir(TRASH_FILES_DIR) == -1) {
			_err(0, NOPRINT_PROMPT, "%s: trash: '%s': %s\n",
				 PROGRAM_NAME, TRASH_FILES_DIR, strerror(errno));
			return EXIT_FAILURE;
		}

		struct dirent **trash_files = (struct dirent **)NULL;
		int files_n = scandir(TRASH_FILES_DIR, &trash_files,
							  skip_implied_dot, (unicode) ? alphasort : 
							  (case_sensitive) ? xalphasort : 
							  alphasort_insensitive);
		if (files_n) {
			size_t i;
			for (i = 0; i < (size_t)files_n; i++) {
				colors_list(trash_files[i]->d_name, (int)i + 1, 0, 1);
				free(trash_files[i]);
			}
			free(trash_files);
		}
		else
			puts(_("trash: There are no trashed files"));

		if (chdir(path) == -1) {
			_err(0, NOPRINT_PROMPT, "%s: trash: '%s': %s\n",
				 PROGRAM_NAME, path, strerror(errno));
			return EXIT_FAILURE;
		}
		else
			return EXIT_SUCCESS;
	}

	else {
		/* Create suffix from current date and time to create unique 
		 * filenames for trashed files */
		int exit_status = EXIT_SUCCESS;
		time_t rawtime = time(NULL);
		struct tm *tm = localtime(&rawtime);
		char date[64] = "";
		strftime(date, sizeof(date), "%b %d %H:%M:%S %Y", tm);
		char suffix[68] = "";
		snprintf(suffix, 67, "%d%d%d%d%d%d", tm->tm_year + 1900,
				tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min,
				tm->tm_sec);

		/* Remove file(s) from Trash */
		if (strcmp(comm[1], "del") == 0 || strcmp(comm[1], "rm") == 0)
			exit_status = remove_from_trash();

		else if (strcmp(comm[1], "clear") == 0)
			trash_clear();

		else {
			/* Trash files passed as arguments */
			size_t i;
			for (i = 1; comm[i]; i++) {
				char *deq_file = dequote_str(comm[i], 0);
				char tmp_comm[PATH_MAX] = "";
				if (deq_file[0] == '/') /* If absolute path */
					strcpy(tmp_comm, deq_file);
				else { /* If relative path, add path to check against 
					TRASH_DIR */
					snprintf(tmp_comm, PATH_MAX, "%s/%s", path,
							 deq_file);
				}

				/* Some filters: you cannot trash wathever you want */
				/* Do not trash any of the parent directories of TRASH_DIR,
				 * that is, /, /home, ~/, ~/.local, ~/.local/share */
				if (strncmp(tmp_comm, TRASH_DIR, strlen(tmp_comm)) == 0) {
					fprintf(stderr, _("trash: Cannot trash '%s'\n"), 
							tmp_comm);
					exit_status = EXIT_FAILURE;
					free(deq_file);
					continue;
				}
				/* Do no trash TRASH_DIR itself nor anything inside it,
				 * that is, already trashed files */
				else if (strncmp(tmp_comm, TRASH_DIR, 
						 strlen(TRASH_DIR)) == 0) {
					puts(_("trash: Use 'trash del' to remove trashed "
							 "files"));
					exit_status = EXIT_FAILURE;
					free(deq_file);
					continue;
				}
				struct stat file_attrib;
				if (lstat(deq_file, &file_attrib) == -1) {
					fprintf(stderr, _("trash: '%s': %s\n"), deq_file, 
							strerror(errno));
					exit_status = EXIT_FAILURE;
					free(deq_file);
					continue;
				}
				/* Do not trash block or character devices */
				else {
					if ((file_attrib.st_mode & S_IFMT) == S_IFBLK) {
						fprintf(stderr, _("trash: '%s': Cannot trash a "
								"block device\n"), deq_file);
						exit_status = EXIT_FAILURE;
						free(deq_file);
						continue;
					}
					else if ((file_attrib.st_mode & S_IFMT) == S_IFCHR) {
						fprintf(stderr, _("trash: '%s': Cannot trash a "
								"character device\n"), deq_file);
						exit_status = EXIT_FAILURE;
						free(deq_file);
						continue;
					}
				}

				/* Once here, everything is fine: trash the file */
				exit_status = trash_element(suffix, tm, deq_file);
				/* The trash_element() function will take care of
				 * printing error messages, if any */
				free(deq_file);
			}
		}

		return exit_status;
	}
}

void
add_to_dirhist(const char *dir_path)
/* Add new path (DIR_PATH) to visited directory history (old_pwd) */
{
	/* Do not add anything if new path equals last entry in directory
	 * history.Just update the current dihist index to reflect the path
	 * change */
	if (dirhist_total_index > 0) {
		if (strcmp(dir_path, old_pwd[dirhist_total_index - 1]) == 0) {
			int i;
			for (i = (dirhist_total_index - 1); i >= 0; i--) {
				if (strcmp(old_pwd[i], dir_path) == 0) {
					dirhist_cur_index = i;
					break;
				}
			}
			return;
		}
	}

	old_pwd = (char **)xrealloc(old_pwd, (size_t)(dirhist_total_index + 1) 
								* sizeof(char *));
	old_pwd[dirhist_total_index] = (char *)xcalloc(strlen(dir_path) + 1, 
												   sizeof(char));
	dirhist_cur_index = dirhist_total_index;
	strcpy(old_pwd[dirhist_total_index++], dir_path);
}

void
readline_kbinds(void)
{
/*	rl_command_func_t readline_kbind_action; */
/* To get the keyseq value for a given key do this in an Xterm terminal:
 * C-v and then press the key (or the key combination). So, for example, 
 * C-v, C-right arrow gives "[[1;5C", which here should be written like
 * this:
 * "\\x1b[1;5C" */

/* These keybindings work on all the terminals I tried: the linux built-in
 * console, aterm, urxvt, xterm, lxterminal, xfce4-terminal, gnome-terminal,
 * terminator, and st (with some patches, however, they might stop working
 * in st) */

	rl_bind_keyseq("\\x1b[H", readline_kbind_action); /* HOME: 72 */
	rl_bind_keyseq("\\x1b[1;3A", readline_kbind_action); /* key: 65 */
	rl_bind_keyseq("\\x1b[1;3B", readline_kbind_action); /* key: 66 */
	rl_bind_keyseq("\\x1b[1;3C", readline_kbind_action); /* key: 67 */
	rl_bind_keyseq("\\x1b[1;3D", readline_kbind_action); /* key: 68 */
	rl_bind_keyseq("\\M-c", readline_kbind_action); /* key: 99 */
	rl_bind_keyseq("\\M-u", readline_kbind_action); /* key: 117 */
	rl_bind_keyseq("\\M-j", readline_kbind_action); /* key: 106 */
	rl_bind_keyseq("\\M-h", readline_kbind_action); /* key: 104 */
	rl_bind_keyseq("\\M-k", readline_kbind_action); /* key: 107 */
	rl_bind_keyseq("\\x1b[21~", readline_kbind_action); /* F10: 126 */
	rl_bind_keyseq("\\M-e", readline_kbind_action); /* key: 101 */
	rl_bind_keyseq("\\M-i", readline_kbind_action); /* key: 105 */
	rl_bind_keyseq("\\M-f", readline_kbind_action); /* key: 102 */
	rl_bind_keyseq("\\M-b", readline_kbind_action); /* key: 98 */
	rl_bind_keyseq("\\M-l", readline_kbind_action); /* key: 108 */
	rl_bind_keyseq("\\M-m", readline_kbind_action); /* key: 109 */
	rl_bind_keyseq("\\M-a", readline_kbind_action); /* key: 97 */
	rl_bind_keyseq("\\M-d", readline_kbind_action); /* key: 100 */
	rl_bind_keyseq("\\M-r", readline_kbind_action); /* key: 114 */
	rl_bind_keyseq("\\M-s", readline_kbind_action); /* key: 115 */
	rl_bind_keyseq("\\M-x", readline_kbind_action); /* key: 120 */
	rl_bind_keyseq("\\M-y", readline_kbind_action); /* key: 121 */
	rl_bind_keyseq("\\M-z", readline_kbind_action); /* key: 122 */
	rl_bind_keyseq("\\C-r", readline_kbind_action); /* key: 18 */

/*
 * Do not use arrow keys for keybindings: there are no standard codes for 
 * them: what works for a certain terminal might not work for another

	rl_bind_keyseq("\\x1b[1;5A", readline_kbind_action); // C-up: 65
	rl_bind_keyseq("\\x1b[1;5B", readline_kbind_action); // C-down: 66
	rl_bind_keyseq("\\x1b[1;5C", readline_kbind_action); // C-right: 67
	rl_bind_keyseq("\\x1b[1;5D", readline_kbind_action); // C-left: 68
	rl_bind_keyseq("\\x1b[H", readline_kbind_action); // HOME: 72
	rl_bind_keyseq("\\x1b[21~", readline_kbind_action); // F10: 126
*/

/*	rl_bind_keyseq("\\C-a", readline_kbind_action); //key: 1
	rl_bind_keyseq("\\C-b", readline_kbind_action); //key: 2
	rl_bind_keyseq("\\C-c", readline_kbind_action); //key: 3 Doesn't work
	rl_bind_keyseq("\\C-d", readline_kbind_action); //key: 4 Doesn't work
	rl_bind_keyseq("\\C-e", readline_kbind_action); //key: 5
	rl_bind_keyseq("\\C-f", readline_kbind_action); //key: 6
	rl_bind_keyseq("\\C-g", readline_kbind_action); //key: 7
	rl_bind_keyseq("\\C-h", readline_kbind_action); //key: 8
	rl_bind_keyseq("\\C-i", readline_kbind_action); //key: 9
	rl_bind_keyseq("\\C-j", readline_kbind_action); //key: 10
	rl_bind_keyseq("\\C-k", readline_kbind_action); //key: 11
	rl_bind_keyseq("\\C-l", readline_kbind_action); //key: 12
	rl_bind_keyseq("\\C-m", readline_kbind_action); //key: 13
	rl_bind_keyseq("\\C-n", readline_kbind_action); //key: 14
	rl_bind_keyseq("\\C-o", readline_kbind_action); //key: 15
	rl_bind_keyseq("\\C-p", readline_kbind_action); //key: 16
	rl_bind_keyseq("\\C-q", readline_kbind_action); //key: 17
	rl_bind_keyseq("\\C-r", readline_kbind_action); //key: 18
	rl_bind_keyseq("\\C-s", readline_kbind_action); //key: 19 Doesn't work
	NOTE: C-s blocks the terminal. It's a feature of the OS called 
	Software Flow Control (XON/XOFF flow control). To reenable it simply 
	press C-q.
	rl_bind_keyseq("\\C-t", readline_kbind_action); //key: 20
	rl_bind_keyseq("\\C-u", readline_kbind_action); //key: 21 Doesn't work
	rl_bind_keyseq("\\C-v", readline_kbind_action); //key: 22 Doesn't work
	rl_bind_keyseq("\\C-w", readline_kbind_action); //key: 23 Doesn't work
	rl_bind_keyseq("\\C-x", readline_kbind_action); //key: 24
	rl_bind_keyseq("\\C-y", readline_kbind_action); //key: 25
	rl_bind_keyseq("\\C-z", readline_kbind_action); //key: 26 Doesn't work

	NOTE: \\M-a means Meta-a, that is, Alt-a
	rl_bind_keyseq("\\M-a", readline_kbind_action); //key: 97
	rl_bind_keyseq("\\M-b", readline_kbind_action); //key: 98
	rl_bind_keyseq("\\M-c", readline_kbind_action); //key: 99
	rl_bind_keyseq("\\M-d", readline_kbind_action); //key: 100
	rl_bind_keyseq("\\M-e", readline_kbind_action); //key: 101
	rl_bind_keyseq("\\M-f", readline_kbind_action); //key: 102
	rl_bind_keyseq("\\M-g", readline_kbind_action); //key: 103
	rl_bind_keyseq("\\M-h", readline_kbind_action); //key: 104
	rl_bind_keyseq("\\M-i", readline_kbind_action); //key: 105
	rl_bind_keyseq("\\M-j", readline_kbind_action); //key: 106
	rl_bind_keyseq("\\M-k", readline_kbind_action); //key: 107
	rl_bind_keyseq("\\M-l", readline_kbind_action); //key: 108
	rl_bind_keyseq("\\M-m", readline_kbind_action); //key: 109
	rl_bind_keyseq("\\M-n", readline_kbind_action); //key: 110 
	rl_bind_keyseq("\\M-o", readline_kbind_action); //key: 111
	rl_bind_keyseq("\\M-p", readline_kbind_action); //key: 112
	rl_bind_keyseq("\\M-q", readline_kbind_action); //key: 113
	rl_bind_keyseq("\\M-r", readline_kbind_action); //key: 114
	rl_bind_keyseq("\\M-s", readline_kbind_action); //key: 115
	rl_bind_keyseq("\\M-t", readline_kbind_action); //key: 116
	rl_bind_keyseq("\\M-u", readline_kbind_action); //key: 117
	rl_bind_keyseq("\\M-v", readline_kbind_action); //key: 118
	rl_bind_keyseq("\\M-w", readline_kbind_action); //key: 119
	rl_bind_keyseq("\\M-x", readline_kbind_action); //key: 120
	rl_bind_keyseq("\\M-y", readline_kbind_action); //key: 121
	rl_bind_keyseq("\\M-z", readline_kbind_action); //key: 122
*/
}

void
keybind_exec_cmd(char *str)
/* Runs any command recognized by CliFM via a keybind. Example:
 * keybind_exec_cmd("sel *") */
{
	size_t old_args = args_n;
	args_n = 0;

	char **cmd = parse_input_str(str);
	puts("");

	if (cmd) {

		exec_cmd(cmd);

		/* While in the bookmarks or mountpoints screen, the kbind_busy 
		 * flag will be set to 1 and no keybinding will work. Once the 
		 * corresponding function exited, set the kbind_busy flag to zero, 
		 * so that keybindings work again */
		if (kbind_busy)
			kbind_busy = 0;

		size_t i;
		for (i = 0; i <= args_n; i++)
			free(cmd[i]);
		free(cmd);

		/* This call to prompt() just updates the prompt in case it was 
		 * modified, for example, in case of chdir, files selection, and 
		 * so on */
		char *buf = prompt();
		free(buf);
	}

	args_n = old_args;
}

int
readline_kbind_action(int count, int key) {
	/* Prevent Valgrind from complaining about unused variable */
	if (count) {}

/*	printf("Key: %d\n", key); */

	/* Disable all keybindings while in the bookmarks or mountpoints
	 * screen */
	if (kbind_busy)
		return 0;

	switch (key) {

	/* C-r: Refresh the screen */
	case 18:
		CLEAR;
		keybind_exec_cmd("rf");
		break;

	/* A-a: Select all files in CWD */
	case 97:
		keybind_exec_cmd("sel .* *");
		break;

	/* A-b: Run the bookmarks manager */
	case 98:
		/* Call the function only if it is not already running */
		if (!kbind_busy) {
/*				if (!config_ok) {
				fprintf(stderr, "%s: Trash disabled\n", PROGRAM_NAME);
				return 0;
			} */
			kbind_busy=1;
			keybind_exec_cmd("bm");
		}
		else
			return 0;
		break;

	/* A-c: Clear the current line (== C-a, C-k). Very handy, 
	 * since C-c is currently disabled */
	case 99:

		/* 1) Clear text typed so far (\x1b[2K) and move cursor to the
		 * beginning of the current line (\r) */
		write(STDOUT_FILENO, "\x1b[2K\r", 5);

		/* 2) Clear the readline buffer */
		rl_delete_text(0, rl_end);
		rl_end = rl_point = 0;

/*		puts("");
		rl_replace_line("", 0); */

		break;

	/* A-d: Deselect all selected files */
	case 100:
		keybind_exec_cmd("ds a");
		break;

	/* Change CWD to the HOME directory */
	case 72: /* HOME */
	case 101: /* A-e */
		/* If already in the home dir, do nothing */
		if (strcmp(path, user_home) == 0)
			return 0;
		keybind_exec_cmd("cd");
		break;

	/* A-f: Toggle folders first on/off */
	case 102:
		/* If status == 0 set it to 1. In this way, the next time
		 * this function is called it will not be true, and the else
		 * clause will be executed instead */
		if (list_folders_first)
			list_folders_first = 0;
		else
			list_folders_first = 1;

		if (cd_lists_on_the_fly) {
			CLEAR;
			while (files--)
				free(dirlist[files]);
			/* Without this puts(), the first entries of the directories
			 * list are printed in the prompt line */
			puts("");
			list_dir();
		}

		break;

	/* A-i: Toggle hidden files on/off */
	case 105:
		if (show_hidden)
			show_hidden = 0;
		else
			show_hidden = 1;

		if (cd_lists_on_the_fly) {
			CLEAR;
			while (files--)
				free(dirlist[files]);
			puts("");
			list_dir();
		}

		break;

	/* Change CWD to PREVIOUS directoy in history */
	case 68: /* A-Left */
	case 66: /* A-Down */
	case 106: 	/* A-j: */
		/* If already at the beginning of dir hist, do nothing */
		if (dirhist_cur_index == 0)
			return 0;
		keybind_exec_cmd("back");
		break;

	/* Change CWD to NEXT directory in history */
	case 67: /* A-Right */
	case 107: /* A-k */
		/* If already at the end of dir hist, do nothing */
		if (dirhist_cur_index + 1 == dirhist_total_index)
			return 0;
		keybind_exec_cmd("forth");
		break;

	/* A-l: Toggle long view mode on/off */
	case 108:
		if (long_view)
			long_view = 0;
		else
			long_view = 1;
		CLEAR;
		keybind_exec_cmd("rf");
		break;

	/* A-m: List available mountpoints */
	case 109:
		/* Call the function only if it's not already running */
		if (!kbind_busy) {
			kbind_busy = 1;
			keybind_exec_cmd("mp");
		}
		else
			return 0;
		break;

	/* A-r: Change CWD to the ROOT directory */
	case 114:
		/* If already in the root dir, do nothing */
		if (strcmp(path, "/") == 0)
			return 0;
		keybind_exec_cmd("cd /");
		break;

	/* A-s: Launch the Selection Box*/
	case 115:
		keybind_exec_cmd("sb");
		break;

	/* Change CWD to PARENT directory */
	case 65: /* A-Up */
	case 117: /* A-u */
		/* If already root dir, do nothing */
		if (strcmp(path, "/") == 0)
			return 0;
		keybind_exec_cmd("cd ..");
		break;

	/* A-y: Toggle light mode on/off */
	case 121:
		if (light_mode)
			light_mode = 0;
		else
			light_mode = 1;

		if (cd_lists_on_the_fly) {
			CLEAR;
			while (files--)
				free(dirlist[files]);
			puts("");
			list_dir();
		}

		break;

	/* A-z: Change to previous sorting method */
	case 122:
		sort--;
		if (sort < 0)
			sort = sort_types;

		if (cd_lists_on_the_fly) {
			CLEAR;
			sort_switch = 1;
			while (files--)
				free(dirlist[files]);
			puts("");
			list_dir();
			sort_switch = 0;
		}

		break;

	/* A-x: Change to next sorting method */
	case 120:
		sort++;
		if (sort > sort_types)
			sort = 0;

		if (cd_lists_on_the_fly) {
			CLEAR;
			sort_switch = 1;
			while (files--)
				free(dirlist[files]);
			puts("");
			list_dir();
			sort_switch = 0;
		}

		break;

	/* F10: Open the config file */
	case 126:
		keybind_exec_cmd("edit");
		break;

	default: break;
	}

	rl_on_new_line();

	return 0;
}

char *
parse_usrvar_value(const char *str, const char c)
{
	if (c == 0x00 || !str) 
		return (char *)NULL;

	/* Get whatever comes after c */
	char *tmp = (char *)NULL;
	tmp = strchr(str, c);

	/* Since we don't want c in our string, move on to the next char */
	tmp++;

	/* If nothing remains */
	if (!tmp)
		return (char *)NULL;

	/* Remove leading quotes */
	if ( *tmp == '"' || *tmp == '\'' )
		tmp++;

	/* Remove trailing spaces, tabs, new line chars, and quotes */
	size_t tmp_len = strlen(tmp), i;
	for (i = tmp_len - 1; tmp[i] && i > 0; i--) {
		if (tmp[i] != 0x20 && tmp[i] != '\t' && tmp[i] != '"'
		&& tmp[i] != '\'' && tmp[i] != '\n')
			break;
		else
			tmp[i] = 0x00;
	}

	if (!tmp)
		return (char *)NULL;

	/* Copy the result string into a buffer and return it */
	char *buf = (char *)NULL;
	buf = (char *)xnmalloc((strlen(tmp)) + 1, sizeof(char));
	strcpy(buf, tmp);
	tmp=(char *)NULL;

	if (buf)
		return buf;

	return (char *)NULL;
}

int
create_usr_var(char *str)
{
	char *name = strbfr(str, '=');
	char *value = parse_usrvar_value(str, '=');

	if (!name) {
		if (value)
			free(value);
		fprintf(stderr, _("%s: Error getting variable name\n"),
				PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	if (!value) {
		free(name);
		fprintf(stderr, _("%s: Error getting variable value\n"),
				PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	usr_var = xrealloc(usr_var, (size_t)(usrvar_n + 1)
					   * sizeof(struct usrvar_t));
	usr_var[usrvar_n].name = xcalloc(strlen(name) + 1, sizeof(char));
	usr_var[usrvar_n].value = xcalloc(strlen(value) + 1, sizeof(char));

	strcpy(usr_var[usrvar_n].name, name);
	strcpy(usr_var[usrvar_n++].value, value);

	free(name);
	free(value);

	return EXIT_SUCCESS;
}

void
free_stuff(void)
/* This function is called by atexit() to clear whatever is there at exit
 * time and avoid thus memory leaks */
{
	size_t i = 0;

	free(TMP_DIR);

	if (file_info)
		free(file_info);

	if (opener)
		free(opener);

	if (encoded_prompt)
		free(encoded_prompt);

	if (bookmark_names) {
		for (i = 0; bookmark_names[i]; i++)
			free(bookmark_names[i]);
		free(bookmark_names);
	}

	if (profile_names) {
		for (i = 0; profile_names[i]; i++)
			free(profile_names[i]);
		free(profile_names);
	}

	if (alt_profile)
		free(alt_profile);

	while (files--)
		free(dirlist[files]);
	free(dirlist);

	if (sel_n > 0) {
		for (i = 0; i < sel_n; i++)
			free(sel_elements[i]);
		free(sel_elements);
	}

	if (bin_commands) {
		for (i = 0; bin_commands[i]; i++)
			free(bin_commands[i]);
		free(bin_commands);
	}

	if (paths) {
		for (i = 0; i < path_n; i++)
			free(paths[i]);
		free(paths);
	}

	if (history) {
		for (i = 0; i < current_hist_n; i++)
			free(history[i]);
		free(history);
	}

	if (argv_bk) {
		for (i = 0; i < (size_t)argc_bk; i++)
			free(argv_bk[i]);
		free(argv_bk);
	}

	if (dirhist_total_index) {
		for (i = 0; i < (size_t)dirhist_total_index; i++)
			free(old_pwd[i]);
		free(old_pwd);
	}

	for (i = 0; i < usrvar_n; i++) {
		free(usr_var[i].name);
		free(usr_var[i].value);
	}
	free(usr_var);

	for (i = 0; i < aliases_n; i++)
		free(aliases[i]);
	free(aliases);

	for (i = 0; i < prompt_cmds_n; i++)
		free(prompt_cmds[i]);
	free(prompt_cmds);

	if (flags & FILE_CMD_OK)
		free(file_cmd_path);

	if (msgs_n) {
		for (i = 0; i < (size_t)msgs_n; i++)
			free(messages[i]);
		free(messages);
	}

	free(user);
	free(user_home);
	free(sys_shell);
	free(path);
	free(CONFIG_DIR);
	free(TRASH_DIR);
	free(TRASH_FILES_DIR);
	free(TRASH_INFO_DIR);
	free(BM_FILE);
	free(LOG_FILE);
	free(LOG_FILE_TMP);
	free(HIST_FILE);
	free(CONFIG_FILE);
	free(PROFILE_FILE);
	free(MSG_LOG_FILE);
	free(sel_file_user);
	free(MIME_FILE);

	/* Restore the foreground color of the running terminal */
	fputs(NC, stdout);
}

char *
savestring(const char *str, size_t size)
{
	if (!str)
		return (char *)NULL;

	char *ptr = (char *)NULL;
	ptr = (char *)xcalloc(size + 1, sizeof(char));
	strcpy(ptr, str);

	return ptr;
}

void *
xrealloc(void *ptr, size_t size)
/*
Usage example:
	char **str = NULL; 
	int i;
	str = xcalloc(1, sizeof(char *));
	for (i = 0; i < 10; i++)
		str = xrealloc(str, (i + 1) * sizeof(char));
*/
{
	if (size == 0)
		++size;

	void *new_ptr = realloc(ptr, size);

	if (!new_ptr) {
		new_ptr = realloc(ptr, size);
		if (!new_ptr) {
			_err(0, NOPRINT_PROMPT, _("%s: %s failed to allocate "
				 "%zu bytes\n"), PROGRAM_NAME, __func__, size);
			exit(EXIT_FAILURE);
		}
	}

	return new_ptr;
}

void *
xcalloc(size_t nmemb, size_t size)
/*
Usage example:
	char **str = NULL;
	str = xcalloc(1, sizeof(char *)); //for arrays allocation

	int i;
	for (i = 0; str; i++) //to allocate elements of the array
		str[i] = xcalloc(7, sizeof(char)); 

	char *str = NULL;
	str = xcalloc(20, sizeof(char)); //for strings allocation
*/
{
	if (nmemb == 0) ++nmemb;
	if (size == 0) ++size;

	void *new_ptr = calloc(nmemb, size);

	if (!new_ptr) {
		new_ptr = calloc(nmemb, size);
		if (!new_ptr) {
			_err(0, NOPRINT_PROMPT, _("%s: %s failed to allocate "
				 "%zu bytes\n"), PROGRAM_NAME, __func__, nmemb * size);
			exit(EXIT_FAILURE);
		}
	}

	return new_ptr;
}

char *
xnmalloc(size_t nmemb, size_t size)
{
	if (nmemb == 0) ++nmemb;
	if (size == 0) ++size;

	char *new_ptr = (char *)malloc(nmemb * size);

	if (!new_ptr) {
		_err(0, NOPRINT_PROMPT, _("%s: %s failed to allocate %zu bytes\n"), 
			 PROGRAM_NAME, __func__, nmemb*size);
		exit(EXIT_FAILURE);
	}

	return new_ptr;
}

void
file_cmd_check(void)
/* Check if the 'file' command is available: it is needed by the mime
 * function */
{
	file_cmd_path = get_cmd_path("file");
	if (!file_cmd_path) {
		flags &= ~FILE_CMD_OK;
		_err('n', PRINT_PROMPT, _("%s: 'file' command not found. "
			 "Specify an application when opening files. Ex: 'o 12 nano' "
			 "or just 'nano 12'\n"), PROGRAM_NAME);
	}
	else
		flags |= FILE_CMD_OK;
}

void
set_signals_to_ignore(void)
{
/*	signal(SIGINT, signal_handler); C-c */
	signal(SIGINT, SIG_IGN); /* C-c */
	signal(SIGQUIT, SIG_IGN); /* C-\ */
	signal(SIGTSTP, SIG_IGN); /* C-z */
/*	signal(SIGTERM, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN); */
/* Why the heck did I disabled SIGCHLD?
 * SIGCHLD is the signal sent to the parent process when the child
 * terminates, for example, when using fork() to create a new process
 * (child). Without this signal, waitpid (or wait) always fails
 * (-1 error) and you cannot get to know the status of the child.
 * (SIGCHLD, SIG_IGN); */
}

void
set_signals_to_default (void)
{
	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGTSTP, SIG_DFL);
/*	signal(SIGTTIN, SIG_DFL);
	signal(SIGTTOU, SIG_DFL);
	signal(SIGCHLD, SIG_DFL); */
}

void
init_shell(void)
/* Keep track of attributes of the shell. Make sure the shell is running
 * interactively as the foreground job before proceeding.
 * Taken from:
 * https://www.gnu.org/software/libc/manual/html_node/Initializing-the-Shell.html#Initializing-the-Shell
 * */
{
	/* Check wether we are running interactively */
	shell_terminal = STDIN_FILENO;
	shell_is_interactive = isatty(shell_terminal);
	/* isatty() returns 1 if the file descriptor, here STDIN_FILENO,
	 * outputs to a terminal (is interactive). Otherwise, it returns
	 * zero */

	if (shell_is_interactive) {
		/* Loop until we are in the foreground */
		while (tcgetpgrp(shell_terminal) != (own_pid = getpgrp()))
			kill (- own_pid, SIGTTIN);

		/* Ignore interactive and job-control signals */
		set_signals_to_ignore();

		/* Put ourselves in our own process group */
		own_pid = get_own_pid();

		if (flags & ROOT_USR) {
			/* Make the shell pgid (process group id) equal to its pid */
			/* Without the setpgid line below, the program cannot be run
			 * with sudo, but it can be run nonetheless by the root user */
			if (setpgid(own_pid, own_pid) < 0) {
				_err(0, NOPRINT_PROMPT, "%s: setpgid: %s\n", PROGRAM_NAME,
					 strerror(errno));
				exit(EXIT_FAILURE);
			}
		}

		/* Grab control of the terminal */
		tcsetpgrp(shell_terminal, own_pid);

		/* Save default terminal attributes for shell */
		tcgetattr(shell_terminal, &shell_tmodes);
	}
}

/*
void signal_handler(int sig_num)
//handle signals catched by the signal function. In this case, the
//function just prompts a new line. If trap signals are not enabled,
//kill the current foreground process (pid)
{
//	if (sig_num == SIGINT) {
//		printf("Content of rl_line_buffer: %s\n", rl_line_buffer);
//		if (is_number(rl_line_buffer) && atoi(rl_line_buffer) > 0 &&
//			atoi(rl_line_buffer) < files) {
//			printf("%s\n", dirlist[atoi(rl_line_buffer)-1]->d_name);
//		}
//		memset(rl_line_buffer, 0x00, strlen(rl_line_buffer));
//		rl_reset_line_state();
//		rl_on_new_line();
//	}
	//if Ctrl-C and pid is not ClifM's pid, kill the current foreground 
	//process AND its childs (-pid).
//	if (sig_num == SIGINT && pid != own_pid) {
//		if (kill(-pid, SIGTERM) != -1)
//			printf("CliFM: %d: Terminated\n", pid);
//		else
//			perror("CliFM: kill");
//	}
} */

void
get_path_programs(void)
/* Get the list of files in PATH, plus CliFM internal commands, and send
 * them into an array to be read by my readline custom auto-complete
 * function (my_rl_completion) */
{
	struct dirent ***commands_bin = (struct dirent ***)xnmalloc(path_n,
												sizeof(struct dirent));
	size_t i, j, l = 0, total_cmd = 0;
	int *cmd_n = (int *)xnmalloc(path_n, sizeof(int));

	for (i = 0; i < path_n; i++) {

		if (!paths[i] || *paths[i] == 0x00 || chdir(paths[i]) == -1) {
			cmd_n[i] = 0;
			continue;
		}

		cmd_n[i] = scandir(paths[i], &commands_bin[i], skip_nonexec,
						   xalphasort);
		/* If paths[i] directory does not exist, scandir returns -1.
		 * Fedora, for example, adds $HOME/bin and $HOME/.local/bin to
		 * PATH disregarding if they exist or not. If paths[i] dir is
		 * empty do not use it either */
		if (cmd_n[i] > 0)
			total_cmd += (size_t)cmd_n[i];
	}

	chdir(path);

	/* Add internal commands */
	/* Get amount of internal cmds (elements in INTERNAL_CMDS array) */
	size_t internal_cmd_n = (sizeof(INTERNAL_CMDS) /
						  sizeof(INTERNAL_CMDS[0])) - 1;
	bin_commands = (char **)xcalloc(total_cmd + internal_cmd_n +
									aliases_n + 2,
									sizeof(char *));
	for (i = 0; i < internal_cmd_n; i++) {
		bin_commands[l] = (char *)xnmalloc(strlen(INTERNAL_CMDS[i]) + 1,
								   sizeof(char));
		strcpy(bin_commands[l++], INTERNAL_CMDS[i]);
	}

	/* Add commands in PATH */
	for (i = 0; i < path_n; i++) {

		if (cmd_n[i] <= 0)
			continue;

		for (j = 0; j < (size_t)cmd_n[i]; j++) {
			bin_commands[l] = (char *)xnmalloc(
									strlen(commands_bin[i][j]->d_name)
									+ 1, sizeof(char));
			strcpy(bin_commands[l++], commands_bin[i][j]->d_name);
			free(commands_bin[i][j]);
		}
		free(commands_bin[i]);
	}

	free(commands_bin);
	free(cmd_n);

	/* Add aliases too */
	if (aliases_n == 0)
		return;

	for (i = 0; i < aliases_n; i++) {
		int index = strcntchr(aliases[i], '=');
		if (index != -1) {
			bin_commands[l] = (char *)xnmalloc((size_t)index + 1,
											   sizeof(char));
			strncpy(bin_commands[l++], aliases[i], (size_t)index);
		}
	}

/*	bin_commands[l] = (char *)NULL; */
}

char *
my_rl_quote(char *text, int m_t, char *qp)
/* Performs bash-style filename quoting for readline (put a backslash
 * before any char listed in rl_filename_quote_characters.
 * Modified version of:
 * https://utcc.utoronto.ca/~cks/space/blog/programming/ReadlineQuotingExample*/
{
	/* NOTE: m_t and qp arguments are not used here, but are required by
	 * rl_filename_quoting_function */

	/* 
	 * How it works: P and R are pointers to the same memory location 
	 * initialized (calloced) twice as big as the line that needs to be 
	 * quoted (in case all chars in the line need to be quoted); TP is a 
	 * pointer to TEXT, which contains the string to be quoted. We move 
	 * through TP to find all chars that need to be quoted ("a's" becomes 
	 * "a\'s", for example). At this point we cannot return P, since this 
	 * pointer is at the end of the string, so that we return R instead, 
	 * which is at the beginning of the same string pointed to by P. 
	 * */
	char *r = (char *)NULL, *p = (char *)NULL, *tp = (char *)NULL;

	size_t text_len = strlen(text);
	/* Worst case: every character of text needs to be escaped. In this 
	 * case we need 2x text's bytes plus the NULL byte. */
	p = (char *)xcalloc((text_len * 2) + 1, sizeof(char));
	r = p;

	if (r == NULL)
		return (char *)NULL;

	/* Escape whatever char that needs to be escaped */
	for (tp = text; *tp; tp++) {
		if (is_quote_char(*tp))
			*p++ = '\\';
		*p++ = *tp;
	}

	/* Add a final null byte to the string */
	*p++ = 0x00;

	return r;
}

char *
dequote_str(char *text, int m_t)
/* This function simply deescapes whatever escaped chars it founds in
 * TEXT, so that readline can compare it to system filenames when
 * completing paths. Returns a string containing text without escape
 * sequences */
{
	if (!text)
		return (char *)NULL;

	/* At most, we need as many bytes as text (in case no escape sequence
	 * is found)*/
	char *buf = NULL;
	buf = (char *)xcalloc(strlen(text) + 1, sizeof(char));
	size_t len = 0;

	while(*text) {

		switch (*text) {

			case '\\':
				if (*(text + 1) && is_quote_char(*(text + 1)))
					buf[len++] = *(++text);
				else
					buf[len++] = *text;
			break;

			default:
				buf[len++] = *text;
			break;
		}

		text++;
	}

	buf[len] = 0x00;

	return buf;
}

char *
my_rl_path_completion(const char *text, int state)
/* This is the filename_completion_function() function of an old Bash 
 * release (1.14.7) modified to fit CliFM needs */
{
	/* state is zero before completion, and 1 ... n after getting
	 * possible completions. Example: 
	 * cd Do[TAB] -> state 0
	 * cuments/ -> state 1
	 * wnloads/ -> state 2 
	 * */

	/* Dequote string to be completed (text), if necessary */
	static char *tmp_text = (char *)NULL;

	if (strcntchr(text, '\\') != -1) {
		char *p = savestring(text, strlen(text));
		tmp_text = dequote_str(p, 0);
		free(p);
		p = (char *)NULL;
		if (!tmp_text)
			return (char *)NULL;
	}

	int rl_complete_with_tilde_expansion = 0;
	/* ~/Doc -> /home/user/Doc */

	static DIR *directory;
	static char *filename = (char *)NULL;
	static char *dirname = (char *)NULL;
	static char *users_dirname = (char *)NULL;
	static size_t filename_len;
	static int match, ret;
	struct dirent *entry = (struct dirent *)NULL;
	static int exec = 0, exec_path = 0;
	static char *dir_tmp = (char *)NULL;
	static char tmp[PATH_MAX] = "";

	/* If we don't have any state, then do some initialization. */
	if (!state) {
		char *temp;

		if (dirname)
			free(dirname);
		if (filename)
			free(filename);
		if (users_dirname)
			free(users_dirname);

						/* tmp_text is true whenver text was dequoted */
		size_t text_len = strlen((tmp_text) ? tmp_text : text);
		if (text_len)
			filename = savestring((tmp_text) ? tmp_text : text, text_len);
		else
			filename = savestring("", 1);
		if (!*text)
			text = ".";
		if (text_len)
			dirname = savestring((tmp_text) ? tmp_text : text, text_len);
		else
			dirname = savestring("", 1);

		if (dirname[0] == '.' && dirname[1] == '/')
			exec = 1;
		else
			exec = 0;

		/* Get everything after last slash */
		temp = strrchr(dirname, '/');

		if (temp) {
			strcpy(filename, ++temp);
			*temp = 0x00;
		}
		else
			strcpy(dirname, ".");

		/* We aren't done yet.  We also support the "~user" syntax. */

		/* Save the version of the directory that the user typed. */
		size_t dirname_len = strlen(dirname);

		users_dirname = savestring(dirname, dirname_len);
/*		{ */
			char *temp_dirname;
			int replace_dirname;

			temp_dirname = tilde_expand(dirname);
			free(dirname);
			dirname = temp_dirname;

			replace_dirname = 0;
			if (rl_directory_completion_hook)
				replace_dirname = (*rl_directory_completion_hook) (&dirname);
			if (replace_dirname) {
				free(users_dirname);
				users_dirname = savestring(dirname, dirname_len);
			}
/*		} */
		directory = opendir(dirname);
		filename_len = strlen(filename);

		rl_filename_completion_desired = 1;


    }

    if (tmp_text) {
		free(tmp_text);
		tmp_text = (char *)NULL;
	}

	/* Now that we have some state, we can read the directory. If we found
	 * a match among files in dir, break the loop and print the match */

	match = 0;

	size_t dirname_len = strlen(dirname);

	/* This block is used only in case of "/path/./" to remove the
	 * ending "./" from dirname and to be able to perform thus the
	 * executable check via access() */
	exec_path = 0;
	if (dirname_len > 2) {
		if (dirname[dirname_len - 3] == '/'
		&& dirname[dirname_len - 2] == '.'
		&& dirname[dirname_len - 1] == '/') {
			dir_tmp = savestring(dirname, dirname_len);
			if (dir_tmp) {
				dir_tmp[dirname_len - 2] = 0x00;
				exec_path = 1;
			}
		}
	}

	/* ############### COMPLETION FILTER ################## */
	/*         This is the heart of the function           */
 
	while (directory && (entry = readdir(directory))) {
		/* If the user entered nothing before TAB (ex: "cd [TAB]") */
		if (!filename_len) {
			/* Exclude "." and ".." as possible completions */
			if ((strcmp(entry->d_name, ".") != 0)
			&& (strcmp(entry->d_name, "..") != 0)) {

				/* If 'cd', match only dirs or symlinks to dir */
				if (strncmp(rl_line_buffer, "cd ", 3) == 0) {
					ret = -1;
					switch (entry->d_type) {
					case DT_LNK:
						if (dirname[0] == '.' && !dirname[1])
							ret = get_link_ref(entry->d_name);
						else {
							snprintf(tmp, PATH_MAX, "%s%s", dirname,
									 entry->d_name);
							ret = get_link_ref(tmp);
						}
						if (ret == S_IFDIR)
							match = 1;
						break;

					case DT_DIR:
						match = 1;
						break;

					default:
						break;
					}
				}

				/* If 'open', allow only reg files, dirs, and symlinks */
				else if (strncmp(rl_line_buffer, "o ", 2) == 0
				|| strncmp(rl_line_buffer, "open ", 5) == 0) {
					ret = -1;
					switch (entry->d_type) {

					case DT_LNK:
						if (dirname[0] == '.' && !dirname[1])
							ret = get_link_ref(entry->d_name);
						else {
							snprintf(tmp, PATH_MAX, "%s%s", dirname,
									 entry->d_name);
							ret = get_link_ref(tmp);
						}
						if (ret == S_IFDIR || ret == S_IFREG)
							match = 1;
						break;

					case DT_REG:
					case DT_DIR:
						match = 1;
						break;

					default:
						break;
					}
				}

				/* If 'trash', allow only reg files, dirs, symlinks, pipes
				 * and sockets. You should not trash a block or a character
				 * device */
				else if (strncmp(rl_line_buffer, "t ", 2) == 0
				|| strncmp(rl_line_buffer, "tr ", 2) == 0
				|| strncmp(rl_line_buffer, "trash ", 6) == 0) {
					if (entry->d_type != DT_BLK && entry->d_type != DT_CHR)
						match = 1;
				}

				/* If "./", list only executable regular files */
				else if (exec) {
					if (entry->d_type == DT_REG 
					&& access(entry->d_name, X_OK) == 0)
						match = 1;
				}

				/* If "/path/./", list only executable regular files */
				else if (exec_path) {
					if (entry->d_type == DT_REG) {
						/* dir_tmp is dirname less "./", already
						 * allocated before the while loop */
						snprintf(tmp, PATH_MAX, "%s%s", dir_tmp,
								 entry->d_name);
						if (access(tmp, X_OK) == 0)
							match = 1;
					}
				}

				/* No filter for everything else. Just print whatever is 
				 * there */
				else
					match = 1;
			}
		}

		/* If there is at least one char to complete (ex: "cd .[TAB]") */
		else {
			/* Check if possible completion match up to the length of
			 * filename. */
			if ((entry->d_name[0] == filename[0])
			&& ((strlen(entry->d_name)) >= filename_len)
			&& (strncmp(filename, entry->d_name, filename_len) == 0)) {

				if (strncmp(rl_line_buffer, "cd ", 3) == 0) {
					ret = -1;
					switch (entry->d_type) {
					case DT_LNK:
						if (dirname[0] == '.' && !dirname[1])
							ret = get_link_ref(entry->d_name);
						else {
							snprintf(tmp, PATH_MAX, "%s%s", dirname,
									 entry->d_name);
							ret = get_link_ref(tmp);
						}
						if (ret == S_IFDIR)
							match = 1;
						break;

					case DT_DIR:
						match = 1;
						break;

					default:
						break;
					}
				}

				else if (strncmp(rl_line_buffer, "o ", 2) == 0
				|| strncmp(rl_line_buffer, "open ", 5) == 0) {
					ret = -1;
					switch (entry->d_type) {
					case DT_REG:
					case DT_DIR:
						match = 1;
						break;

					case DT_LNK:
						if (dirname[0] == '.' && !dirname [1]) {
							ret = get_link_ref(entry->d_name);
						}
						else {
							snprintf(tmp, PATH_MAX, "%s%s", dirname,
									 entry->d_name);
							ret = get_link_ref(tmp);
						}
						if (ret == S_IFDIR || ret == S_IFREG)
							match = 1;
						break;

					default:
						break;
					}
				}

				else if (strncmp(rl_line_buffer, "t ", 2) == 0
				|| strncmp(rl_line_buffer, "tr ", 3) == 0
				|| strncmp(rl_line_buffer, "trash ", 6) == 0) {
					if (entry->d_type != DT_BLK
					&& entry->d_type != DT_CHR)
						match = 1;
				}

				else if (exec) {
					if (entry->d_type == DT_REG
					&& access(entry->d_name, X_OK) == 0)
						match = 1;
				}

				else if (exec_path) {
					if (entry->d_type == DT_REG) {
						snprintf(tmp, PATH_MAX, "%s%s", dir_tmp,
								 entry->d_name);
						if (access(tmp, X_OK) == 0)
							match = 1;
					}
				}

				else
					match = 1;
			}
		}

		if (match)
			break;
	}

	if (dir_tmp) { /* == exec_path */
		free(dir_tmp);
		dir_tmp = (char *)NULL;
	}

	/* readdir() returns NULL on reaching the end of directory stream.
	 * So that if entry is NULL, we have no matches */

	if (!entry) { /* == !match */
		if (directory) {
			closedir(directory);
			directory = (DIR *)NULL;
		}
		if (dirname) {
			free(dirname);
			dirname = (char *)NULL;
		}
		if (filename) {
			free(filename);
			filename = (char *)NULL;
		}
		if (users_dirname) {
			free(users_dirname);
			users_dirname = (char *)NULL;
		}

		return (char *)NULL;
	}

	/* We have a match */
	else {
		char *temp = (char *)NULL;

		/* dirname && (strcmp(dirname, ".") != 0) */
		if (dirname && (dirname[0] != '.' || dirname[1])) {
			if (rl_complete_with_tilde_expansion
			&& *users_dirname == '~') {
				size_t dirlen = strlen(dirname);
				temp = (char *)xcalloc(dirlen + strlen(entry->d_name)
									   + 2, sizeof(char));
				strcpy(temp, dirname);
				/* Canonicalization cuts off any final slash present.
				 * We need to add it back. */
				if (dirname[dirlen - 1] != '/') {
					temp[dirlen] = '/';
					temp[dirlen + 1] = 0x00;
				}
			}
			else {
				  temp = (char *)xcalloc(strlen(users_dirname) + 
										 strlen(entry->d_name) 
										 + 1, sizeof(char));
				  strcpy(temp, users_dirname);
			}
			strcat(temp, entry->d_name);
		}
		else
			temp = savestring(entry->d_name, strlen(entry->d_name));

		return (temp);
	}
}

char **
my_rl_completion(const char *text, int start, int end)
{
	char **matches = (char **)NULL;

	if (start == 0) { /* Only for the first word entered in the prompt */

		/* Commands completion */
		if (end == 0) { /* If text is empty, do nothing */
			/* Prevent readline from attempting path completion if 
			* rl_completion matches returns NULL */
			rl_attempted_completion_over = 1;
			return (char **)NULL;
		}

		/* If autocd or auto-open, try to expand ELN's first */
		if ((autocd || auto_open) && *text >= 0x31 && *text <= 0x39) {

			int num_text = atoi(text);

			if (is_number(text) && num_text > 0
			&& num_text <= (int)files)
				matches = rl_completion_matches(text,
						  &filenames_generator);
		}

		/* If not ELN, complete with command names */
		else
			matches = rl_completion_matches(text, &bin_cmd_generator);
	}

	/* Second word or more */
	else {

				/* #### ELN EXPANSION ### */

		/* Perform this check only if the first char of the string to be 
		 * completed is a number in order to prevent an unnecessary call
		 * to atoi */
		if (*text >= 0x31 && *text <= 0x39) {

			int num_text = atoi(text);

			if (is_number(text) && num_text > 0
			&& num_text <= (int)files)
				matches = rl_completion_matches(text,
						  &filenames_generator);
		}
				/* ### BOOKMARKS COMPLETION ### */

		else if (*rl_line_buffer == 'b' 
		/* Why to compare the first char of the line buffer? Simply to
		 * prevent unnecessary calls to strncmp(). For instance, if the
		 * user typed "cd ", there is no need to compare the line
		 * against "bm" or "bookmarks" */
		&& (strncmp(rl_line_buffer, "bm ", 3) == 0
		|| strncmp(rl_line_buffer, "bookmarks ", 10) == 0)) {
			rl_attempted_completion_over = 1;
			matches = rl_completion_matches(text, &bookmarks_generator);
		}

				/* ### PROFILES COMPLETION ### */

		else if (*rl_line_buffer == 'p'
		&& (strncmp(rl_line_buffer, "pf set ", 7) == 0
		|| strncmp(rl_line_buffer, "profile set ", 12) == 0
		|| strncmp(rl_line_buffer, "pf del ", 7) == 0
		|| strncmp(rl_line_buffer, "profile del ", 12) == 0)) {
			rl_attempted_completion_over = 1;
			matches = rl_completion_matches(text, &profiles_generator);
		}
	}

				/* ### PATH COMPLETION ### */

	/* If not first word and not a number, readline will attempt 
	 * path completion instead via my custom my_rl_path_completion() */
	return matches;
}

int
get_profile_names(void)
{
	char *pf_dir = (char *)xcalloc(user_home_len + strlen(PNL) + 10,
								   sizeof(char));
	sprintf(pf_dir, "%s/.config/%s", user_home, PNL);

	struct dirent **profs = (struct dirent **)NULL;
	int files_n = scandir(pf_dir, &profs, NULL, xalphasort);
	free(pf_dir);
	pf_dir = (char *)NULL;

	if (files_n == -1)
		return EXIT_FAILURE;

	size_t i, pf_n = 0;
	for (i = 0; i < (size_t)files_n; i++) {
		if (profs[i]->d_type == DT_DIR
		/* Discard ".", "..", and hidden dirs */
		&& strncmp(profs[i]->d_name, ".", 1) != 0) {
			profile_names = (char **)xrealloc(profile_names, (pf_n + 1)
											  * sizeof(char *));
			profile_names[pf_n] = (char *)xcalloc(strlen(profs[i]->d_name)
											+ 1, sizeof(char));
			strcpy(profile_names[pf_n++], profs[i]->d_name);
		}
		free(profs[i]);
	}
	free(profs);
	profs = (struct dirent **)NULL;

	profile_names = (char **)xrealloc(profile_names, (pf_n + 1)
									  * sizeof(char *));
	profile_names[pf_n] = (char *)NULL;

	return EXIT_SUCCESS;
}

char *
profiles_generator(const char *text, int state)
/* Used by profiles completion */
{
	static int i;
	static size_t len;
	char *name;

	if (!state) {
		i = 0;
		len = strlen(text);
	} /* The state variable is zero only the first time the function is
	called, and a non-zero positive in later calls. This means that i
	and len will be necessarilly initialized the first time */

	/* Look for profiles in profile_names for a match */
	while ((name = profile_names[i++]) != NULL) {
		if (strncmp(name, text, len) == 0)
			return strdup(name);
	}

	return (char *)NULL;
}

int
get_bm_names(void)
/* Get bookmark names for bookmarks TAB completion */
{
	if (!config_ok)
		return EXIT_FAILURE;

	FILE *fp;

	/* If the bookmarks file doesn't exist, create it */
	struct stat file_attrib;
	if (stat(BM_FILE, &file_attrib) == -1) {
		fp = fopen(BM_FILE, "w+");
		if (!fp) {
			_err('e', PRINT_PROMPT, "bookmarks: '%s': %s\n", BM_FILE,
				 strerror(errno));
			return EXIT_FAILURE;
		}
		else {
			fprintf(fp, "### This is %s bookmarks file ###\n\n"
					"# Empty and commented lines are ommited\n"
					"# The bookmarks syntax is: [hotkey]name:path\n"
					"# Example:\n"
					"# [t]test:/path/to/test\n", PROGRAM_NAME);
			fclose(fp);
		}
	}

	fp = fopen(BM_FILE, "r");
	if (!fp) {
		_err('e', PRINT_PROMPT, "%s: '%s': Error reading the "
			 "bookmarks file\n", PROGRAM_NAME, BM_FILE);
		return EXIT_FAILURE;
	}

	if (bookmark_names) {
		size_t i;
		for (i = 0; bookmark_names[i]; i++)
			free(bookmark_names[i]);
	}

	size_t line_size = 0, bm_n = 0;
	char *line = (char *)NULL;
	ssize_t line_len = 0;

	while ((line_len = getline(&line, &line_size, fp)) > 0) {
		if (line[0] == '#')
			continue;
		/* Full bookmark line: "[sc]name:path". So, let's first get
		 * everything before ':' */
		int ret = strcntchr(line, ':');
		if (ret != -1) {
			line[ret] = 0x00;
			/* Now we have at most "[sc]name" (or just "name" if no
			 * shortcut) */
			char *name = (char *)NULL;
			ret = strcntchr(line, ']');
			if (ret != -1)
				/* Now name is everthing after ']', that is, "name" */
				name = line + ret + 1;
			else /* If no shortcut, name is line in its entirety */
				name = line;
			if (!name || *name == 0x00)
				continue;
			bookmark_names = (char **)xrealloc(bookmark_names,
											   (bm_n + 1) 
											   * sizeof(char *));
			bookmark_names[bm_n] = (char *)xcalloc(strlen(name)
												   + 1, sizeof(char));
			strcpy(bookmark_names[bm_n++], name);
		}
	}

	bookmark_names = (char **)xrealloc(bookmark_names, (bm_n + 1)
									   * sizeof(char *));
	bookmark_names[bm_n] = (char *)NULL;

	free(line);

	fclose(fp);

	return EXIT_SUCCESS;
}

char *
bookmarks_generator(const char *text, int state)
/* Used by bookmarks completion */
{
	static int i;
	static size_t len;
	char *name;

	if (!state) {
		i = 0;
		len = strlen(text);
	}

	/* Look for bookmarks in bookmark_names for a match */
	while ((name = bookmark_names[i++]) != NULL) {
		if (strncmp(name, text, len) == 0)
			return strdup(name);
	}

	return (char *)NULL;
}

char *
filenames_generator(const char *text, int state)
/* Used by ELN expansion */
{
	static size_t i;
	char *name;
	rl_filename_completion_desired=1;
	/* According to the GNU readline documention: "If it is set to a
	 * non-zero value, directory names have a slash appended and
	 * Readline attempts to quote completed filenames if they contain
	 * any embedded word break characters." To make the quoting part
	 * work I had to specify a custom quoting function (my_rl_quote) */
	if (!state) /* state is zero only the first time readline is
	executed */
		i=0;

	int num_text = atoi(text);

	/* Check list of currently displayed files for a match */
	while (i < files && (name = dirlist[i++]) != NULL)
		if (strcmp(name, dirlist[num_text-1]) == 0)
			return strdup(name);

	return (char *)NULL;
}

char *
bin_cmd_generator(const char *text, int state)
/* Used by commands completion */
{
	static int i;
	static size_t len;
	char *name;

	if (!state) {
		i = 0;
		len = strlen(text);
	}

	while ((name = bin_commands[i++]) != NULL) {
		if (strncmp(name, text, len) == 0)
			return strdup(name);
	}

	return (char *)NULL;
}

size_t
get_path_env(void)
/* Store all paths in the PATH environment variable into a globally
 * declared array (paths) */
{
	size_t i = 0;

	/* Get the value of the PATH env variable */
	char *path_tmp = (char *)NULL;

	#if __linux__
	for (i = 0; __environ[i]; i++) {
		if (strncmp(__environ[i], "PATH", 4) == 0) {
			path_tmp = straft(__environ[i], '=');
			break;
		}
	}

	#else
	path_tmp = (char *)xcalloc(strlen(getenv("PATH")) + 1, sizeof(char));
	strcpy(path_tmp, getenv("PATH"));
	#endif

	if (!path_tmp)
		return 0;

	/* Get each path in PATH */
	size_t path_num = 0, length = 0;
	for (i = 0; path_tmp[i]; i++) {

		/* Store path in PATH in a tmp buffer */
		char buf[PATH_MAX] = "";

		while (path_tmp[i] && path_tmp[i] != ':')
			buf[length++] = path_tmp[i++];

		/* Make room in paths for a new path */
		paths = (char **)xrealloc(paths, (path_num + 1) * sizeof(char *));

		/* Allocate space (buf length) for the new path */
		paths[path_num] = (char *)xcalloc(length + 1, sizeof(char));

		/* Dump the buffer into the global paths array */
		strcpy(paths[path_num], buf);

		path_num++;
		length = 0;
		if (!path_tmp[i])
			break;
	}

	free(path_tmp);

	return path_num;
}

void
init_config(void)
/* Set up CliFM directories and config files. Load the user's 
 * configuration from clifmrc */
{
	struct stat file_attrib;
	size_t pnl_len = strlen(PNL);

	/* Store a pointer to the current LS_COLORS value to be used by
	 * external commands */
	ls_colors_bk = getenv("LS_COLORS");

	if (home_ok) {
		/* Set up program's directories and files (always per user) */

		/* If $XDG_CONFIG_HOME is set, use it for the config file.
		 * Else, fall back to $HOME/.config */
		char *xdg_config_home = getenv("XDG_CONFIG_HOME");

		/* alt_profile will not be NULL whenever the -P option is used
		 * to run the program under an alternative profile */
		if (alt_profile) {

			if (xdg_config_home) {
				CONFIG_DIR = (char *)xcalloc(strlen(xdg_config_home)
									 + pnl_len + strlen(alt_profile)
									 + 3, sizeof(char));
				sprintf(CONFIG_DIR, "%s/%s/%s", xdg_config_home, PNL,
						alt_profile);
			}

			else {
				CONFIG_DIR = (char *)xcalloc(user_home_len + pnl_len
										+ 11 + strlen(alt_profile),
										sizeof(char));
				sprintf(CONFIG_DIR, "%s/.config/%s/%s", user_home, PNL, 
						alt_profile);
			}
		}

		/* Use the default profile */
		else {

			if (xdg_config_home) {
				CONFIG_DIR = (char *)xcalloc(strlen(xdg_config_home)
									 + pnl_len + 10, sizeof(char));
				sprintf(CONFIG_DIR, "%s/%s/default", xdg_config_home,
						PNL);
			}

			else {
				CONFIG_DIR = (char *)xcalloc(user_home_len + pnl_len
											 + 18, sizeof(char));
				sprintf(CONFIG_DIR, "%s/.config/%s/default", user_home,
						PNL);
			}
		}

		xdg_config_home = (char *)NULL;

		TRASH_DIR = (char *)xcalloc(user_home_len + 20, sizeof(char));
		sprintf(TRASH_DIR, "%s/.local/share/Trash", user_home);
		size_t trash_len = strlen(TRASH_DIR);
		TRASH_FILES_DIR = (char *)xcalloc(trash_len + 7, sizeof(char));
		sprintf(TRASH_FILES_DIR, "%s/files", TRASH_DIR);
		TRASH_INFO_DIR = (char *)xcalloc(trash_len + 6, sizeof(char));
		sprintf(TRASH_INFO_DIR, "%s/info", TRASH_DIR);
		size_t config_len = strlen(CONFIG_DIR);
		BM_FILE = (char *)xcalloc(config_len + 15, sizeof(char));
		sprintf(BM_FILE, "%s/bookmarks.cfm", CONFIG_DIR);
		LOG_FILE = (char *)xcalloc(config_len + 9, sizeof(char));
		sprintf(LOG_FILE, "%s/log.cfm", CONFIG_DIR);
		LOG_FILE_TMP = (char *)xcalloc(config_len + 13, sizeof(char));
		sprintf(LOG_FILE_TMP, "%s/log_tmp.cfm", CONFIG_DIR);
		HIST_FILE = (char *)xcalloc(config_len + 13, sizeof(char));
		sprintf(HIST_FILE, "%s/history.cfm", CONFIG_DIR);
		CONFIG_FILE = (char *)xcalloc(config_len + pnl_len + 4,
									  sizeof(char));
		sprintf(CONFIG_FILE, "%s/%src", CONFIG_DIR, PNL);
		PROFILE_FILE = (char *)xcalloc(config_len + pnl_len + 10,
							   sizeof(char));
		sprintf(PROFILE_FILE, "%s/%s_profile", CONFIG_DIR, PNL);
		MSG_LOG_FILE = (char *)xcalloc(config_len + 14, sizeof(char));
		sprintf(MSG_LOG_FILE, "%s/messages.cfm", CONFIG_DIR);
		MIME_FILE = (char *)xcalloc(config_len + 14, sizeof(char));
		sprintf(MIME_FILE, "%s/mimelist.cfm", CONFIG_DIR);

		int ret = -1;

		/* #### CHECK THE TRASH DIR #### */
		/* Create trash dirs, if necessary */

		if (stat(TRASH_DIR, &file_attrib) == -1) {
			char *trash_files = (char *)NULL;
			trash_files = (char *)xcalloc(strlen(TRASH_DIR) + 7,
										  sizeof(char));
			sprintf(trash_files, "%s/files", TRASH_DIR);
			char *trash_info = (char *)NULL;
			trash_info = (char *)xcalloc(strlen(TRASH_DIR) + 6,
										 sizeof(char));
			sprintf(trash_info, "%s/info", TRASH_DIR);
			char *cmd[] = { "mkdir", "-p", trash_files, trash_info,
							NULL };
			ret = launch_execve(cmd, FOREGROUND);
			free(trash_files);
			free(trash_info);
			if (ret != EXIT_SUCCESS) {
				trash_ok = 0;
				_err('w', PRINT_PROMPT, _("%s: mkdir: '%s': Error "
					 "creating trash directory. Trash function "
					 "disabled\n"), PROGRAM_NAME, TRASH_DIR);
			}
		}

		/* If it exists, check it is writable */
		else if (access (TRASH_DIR, W_OK) == -1) {
			trash_ok = 0;
			_err('w', PRINT_PROMPT, _("%s: '%s': Directory not writable. "
				 "Trash function disabled\n"), PROGRAM_NAME, TRASH_DIR);
		}

		/* #### CHECK THE CONFIG DIR #### */
		/* If the config directory doesn't exist, create it */
		/* Use the GNU mkdir to let it handle parent directories */
		if (stat(CONFIG_DIR, &file_attrib) == -1) {
			char *tmp_cmd[] = { "mkdir", "-p", CONFIG_DIR, NULL };
			ret = launch_execve(tmp_cmd, FOREGROUND);
			if (ret != EXIT_SUCCESS) {
				config_ok = 0;
				_err('e', PRINT_PROMPT, _("%s: mkdir: '%s': Error "
					 "creating configuration directory. Bookmarks, "
					 "commands logs, and command history are disabled. "
					 "Program messages won't be persistent. Using "
					 "default options\n"), PROGRAM_NAME, CONFIG_DIR);
			}
		}

		/* If it exists, check it is writable */
		else if (access(CONFIG_DIR, W_OK) == -1) {
			config_ok = 0;
			_err('e', PRINT_PROMPT, _("%s: '%s': Directory not writable. "
				 "Bookmarks, commands logs, and commands history are "
				 "disabled. Program messages won't be persistent. "
				 "Using default options\n"), PROGRAM_NAME, CONFIG_DIR);
		}

		/* #### CHECK THE MIME CONFIG FILE #### */
		/* Open the mime file or create it, if it doesn't exist */
		if (config_ok && stat(MIME_FILE, &file_attrib) == -1) {

			_err('n', PRINT_PROMPT, _("%s created a new MIME list file "
				 "(%s). It is recommended to edit this file (typing "
				 "'mm edit' or however you want) to add the programs "
				 "you use and remove those you don't. This will make "
				 "the process of opening files faster and smoother\n"),
				 PROGRAM_NAME, MIME_FILE);


			/* Try importing MIME associations from the system, and in
			 * case nothing can be imported, create an empty MIME list
			 * file */
			ret = mime_import(MIME_FILE);
			if (ret != 0) {
				FILE *mime_fp = fopen(MIME_FILE, "w");
				if (!mime_fp) {
					_err('e', PRINT_PROMPT, "%s: fopen: '%s': %s\n", 
						 PROGRAM_NAME, MIME_FILE, strerror(errno));
				}
				else {
					if (!(flags & GRAPHICAL))
						fputs("text/plain=nano;vim;vi;emacs;ed\n"
							  "*.cfm=nano;vim;vi;emacs;ed\n", mime_fp);
					else
						fputs("text/plain=gedit;kate;pluma;mousepad;"
							  "leafpad;nano;vim;vi;emacs;ed\n"
							  "*.cfm=gedit;kate;pluma;mousepad;leafpad;"
							  "nano;vim;vi;emacs;ed\n", mime_fp);
					fclose(mime_fp);
				}
			}
		}

		/* #### CHECK THE PROFILE FILE #### */
		/* Open the profile file or create it, if it doesn't exist */
		if (config_ok && stat(PROFILE_FILE, &file_attrib) == -1) {
			FILE *profile_fp = fopen(PROFILE_FILE, "w");
			if (!profile_fp) {
				_err('e', PRINT_PROMPT, "%s: fopen: '%s': %s\n",
					 PROGRAM_NAME, PROFILE_FILE, strerror(errno));
			}
			else {
				fprintf(profile_fp, _("#%s profile\n\
#Write here the commands you want to be executed at startup\n\
#Ex:\n#echo -e \"%s, the anti-eye-candy/KISS file manager\"\n"), 
						PROGRAM_NAME, PROGRAM_NAME);
				fclose(profile_fp);
			}
		}

		/* #### CHECK THE CONFIG FILE #### */
		/* Open the config file or create it, if it doesn't exist */
		if (config_ok && stat(CONFIG_FILE, &file_attrib) == -1) {
			ret = create_config(CONFIG_FILE);
			if (ret == EXIT_SUCCESS)
				config_ok = 1;
			else
				config_ok = 0;
		}

				/* #### READ THE CONFIG FILE ##### */
		if (config_ok) {

			set_colors();

			short text_color_set = -1, eln_color_set = -1, 
				  default_color_set = -1, dir_count_color_set = -1, 
				  div_line_color_set = -1, welcome_msg_color_set = -1;

			FILE *config_fp;
			config_fp = fopen(CONFIG_FILE, "r");
			if (!config_fp) {
				_err('e', PRINT_PROMPT, _("%s: fopen: '%s': %s. Using "
					 "default values.\n"), PROGRAM_NAME, CONFIG_FILE,
					 strerror(errno));
			}
			else {
				div_line_char = -1;
				#define MAX_BOOL 6
				/* starting path(14) + PATH_MAX + \n(1)*/
				char line[PATH_MAX + 15];
				while (fgets(line, sizeof(line), config_fp)) {
					if (strncmp(line, "#END OF OPTIONS", 15) == 0)
						break;

					/* Check for the xargs.splash flag. If -1, it was
					 * not set via command line, so that it must be
					 * set here */
					else if (xargs.splash == -1 
					&& strncmp(line, "SplashScreen=", 13) == 0) {
						char opt_str[MAX_BOOL] = ""; /* false (5) + 1 */
						ret = sscanf(line, "SplashScreen=%5s\n", opt_str);
						/* According to cppcheck: "sscanf() without field 
						 * width limits can crash with huge input data". 
						 * Field width limits = %5s */
						if (ret == -1)
							continue;
						if (strncmp(opt_str, "true", 4) == 0)
							splash_screen = 1;
						else /* False and default */
							splash_screen = 0;
					}

					else if (xargs.light == -1 
					&& strncmp(line, "LightMode=", 10) == 0) {
						char opt_str[MAX_BOOL] = "";
						ret = sscanf(line, "LightMode=%5s\n", opt_str);
						if (ret == -1)
							continue;
						if (strncmp(opt_str, "true", 4) == 0)
							light_mode = 1;
						else /* False and default */
							light_mode = 0;
					}

					else if (strncmp(line, "Opener=", 7) == 0) {
						char *opt_str = (char *)NULL;
						opt_str = straft(line, '=');
						if (!opt_str)
							continue;

						char *tmp = remove_quotes(opt_str);
						if (!tmp) {
							free(opt_str);
							continue;
						}

						opener = (char *)xcalloc(strlen(tmp) + 1,
										sizeof(char));
						strcpy(opener, tmp);
						free(opt_str);
					}

					else if (strncmp(line, "Tips=", 5) == 0) {
						char opt_str[MAX_BOOL] = "";
						ret = sscanf(line, "Tips=%5s\n", opt_str);
						if (ret == -1)
							continue;
						if (strncmp(opt_str, "false", 5) == 0)
							tips = 0;
						else /* True and default */
							tips = 1;
					}

					else if (strncmp(line, "Autocd=", 7) == 0) {
						char opt_str[MAX_BOOL] = "";
						ret = sscanf(line, "Autocd=%5s\n", opt_str);
						if (ret == -1)
							continue;
						if (strncmp(opt_str, "false", 5) == 0)
							autocd = 0;
						else /* True and default */
							autocd = 1;
					}

					else if (strncmp(line, "AutoOpen=", 9) == 0) {
						char opt_str[MAX_BOOL] = "";
						ret = sscanf(line, "AutoOpen=%5s\n", opt_str);
						if (ret == -1)
							continue;
						if (strncmp(opt_str, "false", 5) == 0)
							auto_open = 0;
						else /* True and default */
							auto_open = 1;
					}

					else if (strncmp(line, "DirIndicator=", 13) == 0) {
						char opt_str[MAX_BOOL] = "";
						ret = sscanf(line, "DirIndicator=%5s\n", opt_str);
						if (ret == -1)
							continue;
						if (strncmp(opt_str, "false", 5) == 0)
							dir_indicator = 0;
						else /* True and default */
							dir_indicator = 1;
					}

					else if (strncmp(line, "Classify=", 9) == 0) {
						char opt_str[MAX_BOOL] = "";
						ret = sscanf(line, "Classify=%5s\n", opt_str);
						if (ret == -1)
							continue;
						if (strncmp(opt_str, "true", 4) == 0)
							classify = 1;
						else /* False and default */
							classify = 0;
					}

					else if (strncmp(line, "ClassifyExec=", 13) == 0) {
						char opt_str[MAX_BOOL] = "";
						ret = sscanf(line, "ClassifyExec=%5s\n", opt_str);
						if (ret == -1)
							continue;
						if (strncmp(opt_str, "true", 4) == 0)
							classify = 2;
					}

					else if (strncmp(line, "ShareSelbox=", 12) == 0) {
						char opt_str[MAX_BOOL] = "";
						ret = sscanf(line, "ShareSelbox=%5s\n", opt_str);
						if (ret == -1)
							continue;
						if (strncmp(opt_str, "true", 4) == 0)
							share_selbox = 1;
						else /* False and default */
							share_selbox = 0;
					}

					else if (xargs.sort == -1
					&& strncmp(line, "Sort=", 5) == 0) {
						int opt_num = 0;
						ret = sscanf(line, "Sort=%d\n", &opt_num);
						if (ret == -1)
							continue;
						if (opt_num >= 0 && opt_num <= sort_types)
							sort = opt_num;
						else /* default (sort by name) */
							sort = 1;
					}

					else if (strncmp(line, "SortReverse=", 12) == 0) {
						char opt_str[MAX_BOOL] = "";
						ret = sscanf(line, "SortReverse=%5s\n", opt_str);
						if (ret == -1)
							continue;
						if (strncmp(opt_str, "false", 5) == 0)
							sort_reverse = 0;
						else /* True and default */
							sort_reverse = 1;
					}

					else if (strncmp(line, "FilesCounter=", 13) == 0) {
						char opt_str[MAX_BOOL] = "";
						ret = sscanf(line, "FilesCounter=%5s\n", opt_str);
						if (ret == -1)
							continue;
						if (strncmp(opt_str, "false", 5) == 0)
							files_counter = 0;
						else /* True and default */
							files_counter = 1;
					}

					else if (strncmp(line, "WelcomeMessage=",
					15) == 0) {
						char opt_str[MAX_BOOL] = "";
						ret = sscanf(line, "WelcomeMessage=%5s\n",
									 opt_str);
						if (ret == -1)
							continue;
						if (strncmp(opt_str, "false", 5) == 0)
							welcome_message = 0;
						else /* True and default */
							welcome_message = 1;
					}

					else if (strncmp(line, "ClearScreen=", 12) == 0) {
						char opt_str[MAX_BOOL] = "";
						ret = sscanf(line, "ClearScreen=%5s\n",
									 opt_str);
						if (ret == -1)
							continue;
						if (strncmp(opt_str, "true", 4) == 0)
							clear_screen = 1;
						else /* False and default */
							clear_screen = 0;
					}

					else if (xargs.hidden == -1 
					&& strncmp(line, "ShowHiddenFiles=", 16) == 0) {
						char opt_str[MAX_BOOL] = "";
						ret = sscanf(line, "ShowHiddenFiles=%5s\n",
									 opt_str);
						if (ret == -1)
							continue;
						if (strncmp(opt_str, "false", 5) == 0)
							show_hidden = 0;
						else /* True and default */
							show_hidden = 1;
					}

					else if (xargs.longview == -1 
					&& strncmp(line, "LongViewMode=", 13) == 0) {
						char opt_str[MAX_BOOL] = "";
						ret = sscanf(line, "LongViewMode=%5s\n",
									 opt_str);
						if (ret == -1)
							continue;
						if (strncmp (opt_str, "true", 4) == 0)
							long_view = 1;
						else /* False and default */
							long_view = 0;
					}

					else if (xargs.ext == -1 
					&& strncmp(line, "ExternalCommands=", 17) == 0) {
						char opt_str[MAX_BOOL] = "";
						ret = sscanf(line, "ExternalCommands=%5s\n",
									 opt_str);
						if (ret == -1)
							continue;
						if (strncmp(opt_str, "true", 4) == 0)
							ext_cmd_ok = 1;
						else /* False and default */
							ext_cmd_ok = 0;
					}

					else if (strncmp(line, "LogCmds=", 8) == 0) {
						char opt_str[MAX_BOOL] = "";
						ret = sscanf(line, "LogCmds=%5s\n", opt_str);
						if (ret == -1)
							continue;
						if (strncmp(opt_str, "true", 4) == 0)
							logs_enabled = 1;
						else /* False and default */
							logs_enabled = 0;
					}

					else if (strncmp(line, "SystemShell=", 12) == 0) {
						if (sys_shell) {
							free(sys_shell);
							sys_shell = (char *)NULL;
						}
						char *opt_str = straft(line, '=');
						if (!opt_str)
							continue;

						char *tmp = remove_quotes(opt_str);
						if (!tmp) {
							free(opt_str);
							continue;
						}

						if (*tmp == '/') {
							if (access(tmp, F_OK|X_OK) != 0) {
								free(opt_str);
								continue;
							}
							sys_shell = (char *)xnmalloc(strlen(tmp)
													+ 1, sizeof(char));
							strcpy(sys_shell, tmp);
						}

						else {
							char *shell_path = get_cmd_path(tmp);
							if (!shell_path) {
								free(opt_str);
								continue;
							}
							sys_shell = (char *)xnmalloc(
												strlen(shell_path)
												+ 1, sizeof(char));
							strcpy(sys_shell, shell_path);
							free(shell_path);
						}

						free(opt_str);
					}

					else if (strncmp(line, "TerminalCmd=", 12) == 0) {

						if (term) {
							free(term);
							term = (char *)NULL;
						}

						char *opt_str = straft(line, '=');
						if (!opt_str)
							continue;

						char *tmp = remove_quotes(opt_str);
						if (!tmp) {
							free(opt_str);
							continue;
						}

						term = (char *)xnmalloc(strlen(tmp) + 1,
												sizeof(char));
						strcpy(term, tmp);
						free(opt_str);
					}

					else if (xargs.ffirst == -1 
					&& strncmp(line, "ListFoldersFirst=", 17) == 0) {
						char opt_str[MAX_BOOL] = "";
						ret = sscanf(line, "ListFoldersFirst=%5s\n",
									 opt_str);
						if (ret == -1)
							continue;
						if (strncmp(opt_str, "false", 5) == 0)
							list_folders_first = 0;
						else /* True and default */
							list_folders_first = 1;
					}

					else if (xargs.cdauto == -1 
					&& strncmp(line, "CdListsAutomatically=",
					21) == 0) {
						char opt_str[MAX_BOOL] = "";
						ret = sscanf(line, "CdListsAutomatically=%5s\n", 
									 opt_str);
						if (ret == -1)
							continue;
						if (strncmp(opt_str, "false", 5) == 0)
							cd_lists_on_the_fly = 0;
						else /* True and default */
							cd_lists_on_the_fly = 1;
					}

					else if (xargs.sensitive == -1 
					&& strncmp(line, "CaseSensitiveList=", 18) == 0) {
						char opt_str[MAX_BOOL] = "";
						ret = sscanf(line, "CaseSensitiveList=%5s\n", 
								   opt_str);
						if (ret == -1)
							continue;
						if (strncmp(opt_str, "true", 4) == 0)
							case_sensitive = 1;
						else /* False and default */
							case_sensitive = 0;
					}

					else if (xargs.unicode == -1 
					&& strncmp(line, "Unicode=", 8) == 0) {
						char opt_str[MAX_BOOL] = "";
						ret = sscanf(line, "Unicode=%5s\n", opt_str);
						if (ret == -1)
							continue;
						if (strncmp(opt_str, "true", 4) == 0)
							unicode = 1;
						else /* False and default */
							unicode = 0;
					}

					else if (xargs.pager == -1 
					&& strncmp(line, "Pager=", 6) == 0) {
						char opt_str[MAX_BOOL] = "";
						ret = sscanf(line, "Pager=%5s\n", opt_str);
						if (ret == -1)
							continue;
						if (strncmp(opt_str, "true", 4) == 0)
							pager = 1;
						else /* False and default */
							pager = 0;
					}

					else if (strncmp(line, "Prompt=", 7) == 0) {
						if (encoded_prompt)
							free(encoded_prompt);
						encoded_prompt = straft(line, '=');
					}

					else if (strncmp(line, "MaxPath=", 8) == 0) {
						int opt_num = 0;
						sscanf(line, "MaxPath=%d\n", &opt_num);
						if (opt_num <= 0)
							continue;
						max_path = opt_num;
					}

					else if (strncmp(line, "TextColor=", 10) == 0) {
						char *opt_str = (char *)NULL;
						opt_str = straft(line, '=');
						if (!opt_str)
							continue;

						char *tmp = remove_quotes(opt_str);
						if (!tmp) {
							free(opt_str);
							continue;
						}

						if (!is_color_code(tmp)) {
							free(opt_str);
							continue;
						}

						text_color_set = 1;
						snprintf(text_color, sizeof(text_color), 
								 "\001\x1b[%sm\002", tmp);
						free(opt_str);
					}

					else if (strncmp(line, "ELNColor=", 9) == 0) {
						char *opt_str = (char *)NULL;
						opt_str = straft(line, '=');
						if (!opt_str)
							continue;

						char *tmp = remove_quotes(opt_str);
						if (!tmp) {
							free(opt_str);
							continue;
						}

						if (!is_color_code(tmp)) {
							free(opt_str);
							continue;
						}

						eln_color_set = 1;
						snprintf(eln_color, MAX_COLOR, "\x1b[%sm", tmp);
						free(opt_str);
					}

					else if (strncmp(line, "DefaultColor=", 13) == 0) {
						char *opt_str = (char *)NULL;
						opt_str = straft(line, '=');
						if (!opt_str)
							continue;

						char *tmp = remove_quotes(opt_str);
						if (!tmp) {
							free(opt_str);
							continue;
						}

						if (!is_color_code(tmp)) {
							free(opt_str);
							continue;
						}

						default_color_set = 1;
						snprintf(default_color, MAX_COLOR, "\x1b[%sm",
								 tmp);
						free(opt_str);
					}

					else if (strncmp(line, "DirCounterColor=",
					16) == 0) {
						char *opt_str = (char *)NULL;
						opt_str = straft(line, '=');
						if (!opt_str)
							continue;

						char *tmp = remove_quotes(opt_str);
						if (!tmp) {
							free(opt_str);
							continue;
						}

						if (!is_color_code(tmp)) {
							free(opt_str);
							continue;
						}

						dir_count_color_set = 1;
						snprintf(dir_count_color, MAX_COLOR, "\x1b[%sm",
								 tmp);
						free(opt_str);
					}

					else if (strncmp(line, "WelcomeMessageColor=",
					20) == 0) {
						char *opt_str = (char *)NULL;
						opt_str=straft(line, '=');
						if (!opt_str)
							continue;

						char *tmp = remove_quotes(opt_str);
						if (!tmp) {
							free(opt_str);
							continue;
						}

						if (!is_color_code(tmp)) {
							free(opt_str);
							continue;
						}

						welcome_msg_color_set = 1;
						snprintf(welcome_msg_color, MAX_COLOR,
								 "\x1b[%sm", tmp);
						free(opt_str);
					}

					else if (strncmp(line, "DividingLineColor=",
					18) == 0) {
						char *opt_str = (char *)NULL;
						opt_str = straft(line, '=');
						if (!opt_str)
							continue;

						char *tmp = remove_quotes(opt_str);
						if (!tmp) {
							free(opt_str);
							continue;
						}

						if (!is_color_code(tmp)) {
							free(opt_str);
							continue;
						}

						div_line_color_set = 1;
						snprintf(div_line_color, MAX_COLOR,
								 "\x1b[%sm", tmp);
						free(opt_str);
					}
					else if (strncmp(line, "DividingLineChar=",
					17) == 0) {
						/* Accepts both chars and decimal integers */
						char opt_c = -1;
						sscanf(line, "DividingLineChar='%c'", &opt_c);
						if (opt_c == -1) {
							int num = -1;
							sscanf(line, "DividingLineChar=%d", &num);
							if (num == -1)
								div_line_char = '=';
							else
								div_line_char = (char)num;
						}
						else
							div_line_char = opt_c;
					}

					else if (strncmp(line, "MaxHistory=", 11) == 0) {
						int opt_num = 0;
						sscanf(line, "MaxHistory=%d\n", &opt_num);
						if (opt_num <= 0)
							continue;
						max_hist = opt_num;
					}

					else if (strncmp(line, "MaxLog=", 7) == 0) {
						int opt_num = 0;
						sscanf (line, "MaxLog=%d\n", &opt_num);
						if (opt_num <= 0)
							continue;
						max_log = opt_num;
					}

					else if (xargs.path == -1 && !path 
					&& strncmp(line, "StartingPath=", 13) == 0) {
						char *opt_str = straft(line, '=');
						if (!opt_str)
							continue;

						char *tmp = remove_quotes(opt_str);
						if (!tmp) {
							free(opt_str);
							continue;
						}

						/* If starting path is not NULL, and exists,
						 * and is a directory, and the user has
						 * appropriate permissions, set path to starting
						 * path. If any of these conditions is false,
						 * path will be set to default, that is, CWD */
						if (chdir(tmp) == 0) {
							free(path);
							path = (char *)xnmalloc(strlen(tmp) + 1, 
												    sizeof(char));
							strcpy(path, tmp);
						}
						else {
							_err('w', PRINT_PROMPT, _("%s: '%s': %s. "
								 "Using the current working directory "
								 "as starting path\n"), PROGRAM_NAME,
								 tmp, strerror(errno));
						}
						free(opt_str);
					}
				}

				fclose(config_fp);
			}

			/* If some option was not set, neither via command line nor
			 * via the config file, or if this latter could not be read
			 * for any reason, set the defaults */
			/* -1 means not set */
			if (auto_open == -1) auto_open = 1;
			if (autocd == -1) autocd = 1;
			if (light_mode == -1) light_mode = 0;
			if (dir_indicator == -1) dir_indicator = 1;
			if (classify == -1) classify = 0;
			if (share_selbox == -1) share_selbox = 0;
			if (splash_screen == -1) splash_screen = 0;
			if (welcome_message == -1) welcome_message = 1;
			if (show_hidden == -1) show_hidden = 1;
			if (sort == -1) sort = 1;
			if (sort_reverse == -1) sort_reverse = 0;
			if (tips == -1) tips = 1;
			if (files_counter == -1) files_counter = 1;
			if (long_view == -1) long_view = 0;
			if (ext_cmd_ok == -1) ext_cmd_ok = 0;
			if (max_path == -1) max_path = 40;
			if (logs_enabled == -1) logs_enabled = 0;
			if (pager == -1) pager = 0;
			if (max_hist == -1) max_hist = 500;
			if (max_log == -1) max_log = 1000;
			if (clear_screen == -1) clear_screen = 0;
			if (list_folders_first == -1) list_folders_first = 1;
			if (cd_lists_on_the_fly == -1) cd_lists_on_the_fly = 1;	
			if (case_sensitive == -1) case_sensitive = 0;
			if (unicode == -1) unicode = 0;
			if (text_color_set == -1)
				strcpy(text_color, "\001\x1b[00;39;49m\002");
			if (eln_color_set == -1)
				strcpy(eln_color, "\x1b[01;33m");
			if (default_color_set == -1)
				strcpy(default_color, "\x1b[00;39;49m");
			if (dir_count_color_set == -1)
				strcpy(dir_count_color, "\x1b[00;97m");
			if (div_line_char == -1)
				div_line_char = '=';
			if (div_line_color_set == -1)
				strcpy(div_line_color, "\x1b[00;34m");
			if (welcome_msg_color_set == -1)
				strcpy(welcome_msg_color, "\x1b[01;36m");
			if (!sys_shell) {
				/* Get user's default shell */
				sys_shell = get_sys_shell();
				if (!sys_shell) {
					/* Fallback to FALLBACK_SHELL */
					sys_shell = (char *)xcalloc(strlen(FALLBACK_SHELL)
												+ 1, sizeof(char));
					strcpy(sys_shell, FALLBACK_SHELL);
				}
			}
			if (!encoded_prompt) {
				encoded_prompt = (char *)xcalloc(strlen(DEFAULT_PROMPT)
												 + 1, sizeof(char));
				strcpy(encoded_prompt, DEFAULT_PROMPT);
			}
			if (!term) {
				term = (char *)xcalloc(strlen(DEFAULT_TERM_CMD) + 1, 
									   sizeof(char));
				strcpy(term, DEFAULT_TERM_CMD);
			}
		}

		/* If we reset all these values, a) the user will be able to
		 * modify them while in the program via the edit command.
		 * However, this means that b) any edit, to whichever option,
		 * will override the values for ALL options, including those
		 * set in the command line. Example: if the user pass the -A
		 * option to show hidden files (while it is disabled in the
		 * config file), and she edits the config file to, say, change
		 * the prompt, hidden files will be disabled, which is not what
		 * the user wanted */

		/* "XTerm*eightBitInput: false" must be set in HOME/.Xresources
		 * to make some keybindings like Alt+letter work correctly in
		 * xterm-like terminal emulators */
		/* However, there is no need to do this if using the linux console, 
		 * since we are not in a graphical environment */
		if (flags & GRAPHICAL) {
			/* Check ~/.Xresources exists and eightBitInput is set to
			 * false. If not, create the file and set the corresponding
			 * value */
			char xresources[PATH_MAX] = "";
			sprintf(xresources, "%s/.Xresources", user_home);
			FILE *xresources_fp = fopen(xresources, "a+");
			if (xresources_fp) {
				/* Since I'm looking for a very specific line, which is
				 * a fixed line far below MAX_LINE, I don't care to get
				 * any of the remaining lines truncated */
				#if __FreeBSD__
					fseek(xresources_fp, 0, SEEK_SET);
				#endif
				char line[256] = "";
				int eight_bit_ok = 0;
				while (fgets(line, sizeof(line), xresources_fp))
					if (strncmp(line, "XTerm*eightBitInput: false",
					26) == 0)
						eight_bit_ok = 1;
				if (!eight_bit_ok) {
					/* Set the file position indicator at the end of
					 * the file */
					fseek(xresources_fp, 0L, SEEK_END);
					fputs("\nXTerm*eightBitInput: false\n",
						  xresources_fp);
					char *xrdb_path = get_cmd_path("xrdb");
					if (xrdb_path) {
						char *res_file = (char *)xnmalloc(
													strlen(user_home) 
													+ 13, sizeof(char));
						sprintf(res_file, "%s/.Xresources", user_home); 
						char *cmd[] = { "xrdb", "merge", res_file,
										NULL };
						launch_execve(cmd, FOREGROUND);
						free(res_file);
					}

					_err('w', PRINT_PROMPT, _("%s: Restart your %s for "
						 "changes to ~/.Xresources to take effect. "
						 "Otherwise, %s keybindings might not work as "
						 "expected.\n"), PROGRAM_NAME, (xrdb_path) 
						 ? _("terminal") : _("X session"),
						 PROGRAM_NAME);

					if (xrdb_path)
						free(xrdb_path);
				}

				fclose(xresources_fp);
			}
			else {
				_err('e', PRINT_PROMPT, "%s: fopen: '%s': %s\n",
					 PROGRAM_NAME, xresources, strerror(errno));
			}
		}
	}

	/* ################## END OF IF HOME ########################### */

	/* #### CHECK THE TMP DIR #### */

	/* If the temporary directory doesn't exist, create it. I create
	 * the parent directory (/tmp/clifm) with 1777 permissions (world
	 * writable with the sticky bit set), so that every user is able
	 * to create files in here, but only the file's owner can remove
	 * or modify them */

	TMP_DIR = (char *)xnmalloc(strlen(user) + pnl_len + 7, sizeof(char));
	sprintf(TMP_DIR, "/tmp/%s", PNL);

	if (stat(TMP_DIR, &file_attrib) == -1) {
/*		if (mkdir(TMP_DIR, 1777) == -1) { */
		char *tmp_cmd2[] = { "mkdir", "-pm1777", TMP_DIR, NULL };
		int ret = launch_execve(tmp_cmd2, FOREGROUND);
		if (ret != EXIT_SUCCESS) {
			_err('e', PRINT_PROMPT, "%s: mkdir: '%s': %s\n",
				 PROGRAM_NAME, TMP_DIR, strerror(errno));
		}
	}

	/* Once the parent directory exists, create the user's directory to 
	 * store the list of selected files: 
	 * TMP_DIR/clifm/username/.selbox_PROFILE. I use here very
	 * restrictive permissions (700), since only the corresponding user
	 * must be able to read and/or modify this list */

	sprintf(TMP_DIR, "/tmp/%s/%s", PNL, user);

	if (stat(TMP_DIR, &file_attrib) == -1) {
/*		if (mkdir(TMP_DIR, 1777) == -1) { */
		char *tmp_cmd3[] = { "mkdir", "-pm700", TMP_DIR, NULL };
		int ret = launch_execve(tmp_cmd3, FOREGROUND);
		if (ret != EXIT_SUCCESS) {
			selfile_ok = 0;
			_err('e', PRINT_PROMPT, "%s: mkdir: '%s': %s\n",
				 PROGRAM_NAME, TMP_DIR, strerror(errno));
		}
	}

	/* If the directory exists, check it is writable */
	else if (access(TMP_DIR, W_OK) == -1) {
		selfile_ok = 0;
		_err('e', PRINT_PROMPT, "%s: '%s': Directory not writable. "
			 "Selected files won't be persistent\n", PROGRAM_NAME,
			 TMP_DIR);
	}

	if (selfile_ok) {

		size_t tmp_dir_len = strlen(TMP_DIR);

		/* Define the user's sel file. There will be one per
		 * user-profile (/tmp/clifm/username/.selbox_PROFILE) */

		if (sel_file_user)
			free(sel_file_user);

		if (!share_selbox) {
			size_t prof_len = 0;

			if (alt_profile)
				prof_len = strlen(alt_profile);
			else
				prof_len = 7; /* Lenght of "default" */

			sel_file_user = (char *)xcalloc(tmp_dir_len + prof_len
											+ 10, sizeof(char));
			sprintf(sel_file_user, "%s/.selbox_%s", TMP_DIR,
					(alt_profile) ? alt_profile : "default");
		}
		else {
			sel_file_user = (char *)xcalloc(tmp_dir_len + 9,
											sizeof(char));
			sprintf(sel_file_user, "%s/.selbox", TMP_DIR);
		}
	}
}

void
exec_profile(void)
{
	if (!config_ok)
		return;

	struct stat file_attrib;
	if (stat(PROFILE_FILE, &file_attrib) == -1)
		return;

	FILE *fp = fopen(PROFILE_FILE, "r");
	if (!fp)
		return;

	size_t line_size = 0;
	char *line = (char *)NULL;
	ssize_t line_len = 0;

	while ((line_len = getline(&line, &line_size, fp)) > 0) {

		/* Skip empty and commented lines */
		if (*line == 0x00 || *line == '\n' || *line == '#')
			continue;

		/* Remove trailing new line char */
		if (line[line_len - 1] == '\n')
			line[line_len - 1] = 0x00;

		if (strcntchr(line, '=') != -1 && !isdigit(*line))
			create_usr_var(line);

		/* Parse line and execute it */
		else if (strlen(line) != 0) {
			args_n = 0;
			char **cmds = parse_input_str(line);
			if (cmds) {
				no_log = 1;
				exec_cmd(cmds);
				no_log = 0;
				size_t i;
				for (i = 0; i <= args_n; i++)
					free(cmds[i]);
				free(cmds);
				cmds = (char **)NULL;
			}
			args_n = 0;
		}
	}
	free(line);
	fclose(fp);
}

char *
get_cmd_path(const char *cmd)
/* Get the path of a given command from the PATH environment variable.
 * It basically does the same as the 'which' Unix command */
{
	char *cmd_path = (char *)NULL;
	size_t i;

	cmd_path = (char *)xnmalloc(PATH_MAX + 1, sizeof(char));

	for (i = 0; i < path_n; i++) { /* Get each path from PATH */
		/* Append cmd to each path and check if it exists and is
		 * executable */
		snprintf(cmd_path, PATH_MAX, "%s/%s", paths[i], cmd);
		if (access(cmd_path, X_OK) == 0)
			return cmd_path;
	}

	/* If cmd was not found */
	free(cmd_path);

	return (char *)NULL;
}

void
external_arguments(int argc, char **argv)
/* Evaluate external arguments, if any, and change initial variables to
 * its corresponding value */
{
/* Link long (--option) and short options (-o) for the getopt_long
 * function */
	/* Disable automatic error messages to be able to handle them
	 * myself via the '?' case in the switch */
	opterr=optind = 0;
	static struct option longopts[] = {
		{"no-hidden", no_argument, 0, 'a'},
		{"show-hidden", no_argument, 0, 'A'},
		{"no-folders-first", no_argument, 0, 'f'},
		{"folders-first", no_argument, 0, 'F'},
		{"pager", no_argument, 0, 'g'},
		{"no-pager", no_argument, 0, 'G'},
		{"help", no_argument, 0, 'h'},
		{"no-case-sensitive", no_argument, 0, 'i'},
		{"case-sensitive", no_argument, 0, 'I'},
		{"no-long-view", no_argument, 0, 'l'},
		{"long-view", no_argument, 0, 'L'},
		{"no-list-on-the-fly", no_argument, 0, 'o'},
		{"list-on-the-fly", no_argument, 0, 'O'},
		{"path", required_argument, 0, 'p'},
		{"profile", required_argument, 0, 'P'},
		{"unicode", no_argument, 0, 'U'},
		{"no-unicode", no_argument, 0, 'u'},
		{"version", no_argument, 0, 'v'},
		{"splash", no_argument, 0, 's'},
		{"ext-cmds", no_argument, 0, 'x'},
		{"light", no_argument, 0, 'y'},
		{"sort", required_argument, 0, 'z'},
		{0, 0, 0, 0}
	};

	/* Set all external arguments flags to uninitialized state */
	xargs.splash = xargs.hidden = xargs.longview = xargs.ext = -1;
	xargs.ffirst = xargs.sensitive = xargs.unicode = xargs.pager = -1;
	xargs.path = xargs.cdauto = xargs.light = xargs.sort = -1;

	int optc;
	/* Variables to store arguments to options (-p and -P) */
	char *path_value = (char *)NULL, *alt_profile_value = (char *)NULL;

	while ((optc = getopt_long(argc, argv, "+aAfFgGhiIlLoOp:P:sUuvxyz:",
							   longopts, (int *)0)) != EOF) {
		/* ':' and '::' in the short options string means 'required' and 
		 * 'optional argument' respectivelly. Thus, 'p' and 'P' require
		 * an argument here. The plus char (+) tells getopt to stop
		 * processing at the first non-option (and non-argument) */
		switch (optc) {

		case 'a':
			flags &= ~HIDDEN; /* Remove HIDDEN from 'flags' */
			show_hidden = 0;
			xargs.hidden = 0;
			break;

		case 'A':
			flags |= HIDDEN; /* Add HIDDEN to 'flags' */
			show_hidden = 1;
			xargs.hidden = 1;
			break;

		case 'f':
			flags &= ~FOLDERS_FIRST;
			list_folders_first = 0;
			xargs.ffirst = 0;
			break;

		case 'F':
			flags |= FOLDERS_FIRST;
			list_folders_first = 1;
			xargs.ffirst = 1;
			break;

		case 'g':
			pager = 1;
			xargs.pager = 1;
			break;

		case 'G':
			pager = 0;
			xargs.pager = 0;
			break;

		case 'h':
			flags |= HELP;
			/* Do not display "Press any key to continue" */
			flags |= EXT_HELP;
			help_function();
			exit(EXIT_SUCCESS);

		case 'i':
			flags &= ~CASE_SENS;
			case_sensitive = 0;
			xargs.sensitive = 0;
			break;

		case 'I':
			flags |= CASE_SENS;
			case_sensitive = 1;
			xargs.sensitive = 1;
			break;

		case 'l':
			long_view = 0;
			xargs.longview = 0;
			break;

		case 'L':
			long_view = 1;
			xargs.longview = 1;
			break;

		case 'o':
			flags &= ~ON_THE_FLY;
			cd_lists_on_the_fly = 0;
			xargs.cdauto = 0;
			break;

		case 'O':
			flags |= ON_THE_FLY;
			cd_lists_on_the_fly = 1;
			xargs.cdauto = 1;
			break;

		case 'p':
			flags |= START_PATH;
			path_value = optarg;
			xargs.path = 1;
			break;

		case 'P':
			flags |= ALT_PROFILE;
			alt_profile_value = optarg;
			break;

		case 's':
			flags |= SPLASH;
			splash_screen = 1;
			xargs.splash = 1;
			break;

		case 'u':
			unicode = 0;
			xargs.unicode = 0;
			break;

		case 'U':
			unicode = 1;
			xargs.unicode = 1;
			break;

		case 'v':
			flags |= PRINT_VERSION;
			version_function();
			exit(EXIT_SUCCESS);

		case 'x':
			ext_cmd_ok = 1;
			xargs.ext = 1;
			break;

		case 'y':
			light_mode = 1;
			xargs.light = 1;
			break;

		case 'z':
			{
				int arg = atoi(optarg);
				if (!is_number(optarg))
					sort = 1;
				else if (arg < 0 || arg > sort_types)
					sort = 1;
				else
					sort = arg;
				xargs.sort = 1;
			}
			break;

		case '?': /* If some unrecognized option was found... */
			/* Options that require an argument */
			if (optopt == 'p')
				fprintf(stderr, _("%s: option requires an argument -- "
						"'%c'\nTry '%s --help' for more information.\n"), 
						PROGRAM_NAME, optopt, PNL);
			/* If unknown option is printable... */
			else if (isprint(optopt))
				fprintf(stderr, _("%s: invalid option -- '%c'\nUsage: %s "
						"[-aAfFgGhiIlLoOsuUvx] [-p path]\nTry '%s --help' "
						"for more information.\n"), PROGRAM_NAME, optopt,
						PNL, PNL);
			else fprintf(stderr, _("%s: unknown option character '\\%x'\n"), 
						 PROGRAM_NAME, (unsigned int)optopt);
			exit(EXIT_FAILURE);

		default: break;
		}
	}

	if ((flags & START_PATH) && path_value) {
		char *path_exp = (char *)NULL;
		if (*path_value == '~') {
			path_exp = tilde_expand(path_value);
			path_value = path_exp;
		}
		if (chdir(path_value) == 0) {
			if (path)
				free(path);
			path = (char *)xcalloc(strlen(path_value) + 1, sizeof(char));
			strcpy(path, path_value);
		}
		else { /* Error changing directory */
			_err('w', PRINT_PROMPT, "%s: '%s': %s\n", PROGRAM_NAME,
				 path_value, strerror(errno));
		}
		if (path_exp)
			free(path_exp);
	}

	if ((flags & ALT_PROFILE) && alt_profile_value) {
		if (alt_profile)
			free(alt_profile);

		alt_profile = (char *)xcalloc(strlen(alt_profile_value) + 1, 
									  sizeof(char));
		strcpy(alt_profile, alt_profile_value);
	}
}

char *
home_tilde(const char *new_path)
/* Reduce "HOME" to tilde ("~"). The new_path variable is always either 
 * "HOME" or "HOME/file", that's why there's no need to check for
 * "/file" */
{
	if (!home_ok)
		return (char *)NULL;

	char *path_tilde = (char *)NULL;

	/* If path == HOME */
	if (strcmp(new_path, user_home) == 0) {
		path_tilde = (char *)xcalloc(2, sizeof(char));
		path_tilde[0] = '~';
	}

	/* If path == HOME/file */
	else {
		path_tilde = (char *)xcalloc(strlen(
							 new_path + user_home_len + 1)
							 + 3, sizeof(char));
		sprintf(path_tilde, "~/%s", new_path + user_home_len + 1);
	}

	return path_tilde;
}

int
brace_expansion(const char *str)
{
	size_t str_len = strlen(str);
	size_t i = 0, j = 0, k = 0, l = 0;
	int initial_brace = 0, closing_brace = -1;
	char *braces_root = (char *)NULL, *braces_end = (char *)NULL;

	for (i = 0; i < str_len; i++) {
		if (str[i] == '{') {
			initial_brace = (int)i;
			braces_root = (char *)xcalloc(i + 2, sizeof(char));

			/* If any, store the string before the beginning of the
			 * braces */
			for (j = 0; j < i; j++)
				braces_root[j] = str[j];
		}

		/* If any, store the string after the closing brace */
		if (initial_brace && str[i] == '}') {
			closing_brace = (int)i;
			if ((str_len - 1) - (size_t)closing_brace > 0) {
				braces_end = (char *)xcalloc(str_len -
											 (size_t)closing_brace, 
											 sizeof(char));
				for (k = (size_t)closing_brace + 1; str[k] != 0x00; k++)
					braces_end[l++] = str[k];
			}
		break;
		}
	}

	if (closing_brace == -1) {
		if (braces_root) {
			free(braces_root);
			braces_root = (char *)NULL;
		}
		return 0;
	}

	k=0;
	size_t braces_n = 0;

	braces = (char **)xnmalloc(braces_n + 1, sizeof(char *));

	for (i = j + 1; i < (size_t)closing_brace; i++) {
		char *brace_tmp = (char *)xnmalloc((size_t)
										   (closing_brace -
										   initial_brace),
										   sizeof(char));

		while (str[i] != '}' && str[i] != ',' && str[i] != 0x00)
			brace_tmp[k++] = str[i++];

		brace_tmp[k] = 0x00;

		if (braces_end) {
			braces[braces_n] = (char *)xcalloc(strlen(braces_root) 
									+ strlen(brace_tmp)
									+ strlen(braces_end) + 1,
									sizeof(char));
			sprintf(braces[braces_n], "%s%s%s", braces_root,
					brace_tmp, braces_end);
		}
		else {
			braces[braces_n] = (char *)xcalloc(strlen(braces_root)
										+ strlen(brace_tmp) + 1,
										sizeof(char));
			sprintf(braces[braces_n], "%s%s", braces_root, brace_tmp);
		}

		if (str[i] == ',')
			braces = (char **)xrealloc(braces, (++braces_n + 1)
									   * sizeof(char *));

		k = 0;

		free(brace_tmp);
	}

	if (braces_root)
		free(braces_root);

	if (braces_end)
		free(braces_end);

	return (int)braces_n + 1;
}

char **
parse_input_str(char *str)
/* 
 * This function is one of the keys of CliFM. It will perform a series of 
 * actions:
 * 1) Take the string stored by readline and get its substrings without
 * spaces. 
 * 2) In case of user defined variable (var=value), it will pass the
 * whole string to exec_cmd(), which will take care of storing the
 * variable;
 * 3) If the input string begins with ';' or ':' the whole string is
 * send to exec_cmd(), where it will be directly executed by the system
 * shell (via launch_execle()) to prevent all of the expansions made
 * here.
 * 4) The following expansions (especific to CLiFM) are performed here:
 * ELN's, "sel" keyword, and ranges of numbers (ELN's). For CliFM
 * internal commands, braces, tilde, and wildarcards expansions are
 * performed here as well. These expansions are the most import part
 * of this function.
 */

/* NOTE: Though filenames could consist of everything except of slash
 * and null characters, POSIX.1 recommends restricting filenames to
 * consist of the following characters: letters (a-z, A-Z), numbers
 * (0-9), period (.), dash (-), and underscore ( _ ).

 * NOTE 2: There is no any need to pass anything to this function, since
 * the input string I need here is already in the readline buffer. So,
 * instead of taking the buffer from a function parameter (str) I could
 * simply use rl_line_buffer. However, since I use this function to
 * parse other strings, like history lines, I need to keep the str
 * argument */

{
	register size_t i = 0;

		   /* ########################################
		    * #    0) CHECK FOR SPECIAL FUNCTIONS    # 
		    * ########################################*/

	int chaining = 0, cond_cmd = 0, send_shell = 0;

					/* ###########################
					 * #  0.a) RUN AS EXTERNAL   # 
					 * ###########################*/

	/* If invoking a command via ';' or ':' set the send_shell flag to
	 * true and send the whole string to exec_cmd(), in which case no
	 * expansion is made: the command is send to the system shell as
	 * is. */
	if (*str == ';' || *str == ':')
		send_shell = 1;

	if (!send_shell) {
		for (i = 0; str[i]; i++) {

				/* ##################################
				 * #   0.b) CONDITIONAL EXECUTION   # 
				 * ##################################*/

			/* Check for chained commands (cmd1;cmd2) */
			if (!chaining && str[i] == ';' && i > 0
			&& str[i - 1] != '\\')
				chaining = 1;

			/* Check for conditional execution (cmd1 && cmd 2)*/
			if (!cond_cmd && str[i] == '&' && i > 0
			&& str[i - 1] != '\\' && str[i + 1] && str[i + 1] == '&')
				cond_cmd = 1;

				/* ##################################
				 * #   0.c) USER DEFINED VARIABLE   # 
				 * ##################################*/

			/* If user defined variable send the whole string to
			 * exec_cmd(), which will take care of storing the
			 * variable. */
			if (!(flags & IS_USRVAR_DEF) && str[i] == '=' && i > 0 
			&& str[i - 1] != '\\' && str[0] != '=') {
				/* Remove leading spaces. This: '   a="test"' should be
				 * taken as a valid variable declaration */
				char *p = str;
				while (*p == 0x20 || *p == '\t')
					p++;

				/* If first non-space is a number, it's not a variable
				 * name */
				if (!isdigit(*p)) {
					int space_found = 0;
					/* If there are no spaces before '=', take it as a
					 * variable. This check is done in order to avoid
					 * taking as a variable things like:
					 * 'ls -color=auto' */
					while (*p != '=') {
						if (*(p++) == 0x20)
							space_found = 1;
					}

					if (!space_found)
						flags |= IS_USRVAR_DEF;
				}

				p = (char *)NULL;
			}
		}
	}

	/* If chained commands, check each of them. If at least one of them
	 * is internal, take care of the job (the system shell does not know
	 * our internal commands and therefore cannot execute them); else,
	 * if no internal command is found, let it to the system shell */
	if (chaining || cond_cmd) {

		/* User defined variables are always internal, so that there is
		 * no need to check whatever else is in the command string */
		if (flags & IS_USRVAR_DEF) {
			exec_chained_cmds(str);
			return (char **)NULL;
		}

		register size_t j = 0; 
		size_t str_len = strlen(str), len = 0, internal_ok = 0;
		char *buf = (char *)NULL;

		/* Get each word (cmd) in STR */
		buf = (char *)xcalloc(str_len + 1, sizeof(char));
		for (j = 0; j < str_len; j++) {

			while (str[j] && str[j] != 0x20 && str[j] != ';'
			&& str[j] != '&') {
				buf[len++] = str[j++];
			}

			if (strcmp(buf, "&&") != 0) {
				if (is_internal_c(buf)) {
					internal_ok = 1;
					break;
				}
			}

			memset(buf, 0x00, len);
			len = 0;
		}

		free(buf);
		buf = (char *)NULL;

		if (internal_ok) {
			exec_chained_cmds(str);
			return (char **)NULL;
		}
	}

	if (flags & IS_USRVAR_DEF || send_shell) {
		/* Remove leading spaces, again */
		char *p = str;
		while (*p == 0x20 || *p == '\t')
			p++;

		args_n=0;

		char **cmd = (char **)NULL;
		cmd = (char **)xnmalloc(2, sizeof(char *));
		cmd[0] = (char *)xcalloc(strlen(p) + 1, sizeof(char));
		strcpy(cmd[0], p);
		cmd[1] = (char *)NULL;

		p = (char *)NULL;

		return cmd;
		/* If ";cmd" or ":cmd" the whole input line will be send to
		 * exec_cmd() and will be executed by the system shell via
		 * execle(). Since we don't run split_str() here, dequoting
		 * and deescaping is performed directly by the system shell */
	}

		/* ################################################
		 * #     1) SPLIT INPUT STRING INTO SUBSTRINGS    # 
		 * ################################################*/

	/* split_str() returns an array of strings without leading,
	 * terminating and double spaces. */
	char **substr = split_str(str);

	/* NOTE: isspace() not only checks for space, but also for new line, 
	 * carriage return, vertical and horizontal TAB. Be careful when
	 * replacing this function. */

	if (!substr)
		return (char **)NULL;

				/* ##############################
				 * #   2) BUILTIN EXPANSIONS    #
				 * ############################## 

	 * Ranges, sel, ELN, and internal variables. 
	 * These expansions are specific to CliFM. To be able to use them
	 * even with external commands, they must be expanded here, before
	 * sending the input string, in case the command is external, to
	 * the system shell */

	is_sel = 0, sel_is_last = 0;

	size_t int_array_max = 10, ranges_ok = 0;
	int *range_array = (int *)xnmalloc(int_array_max, sizeof(int));

	for (i = 0; i <= args_n; i++) {

		size_t substr_len = strlen(substr[i]);

		/* Check for ranges */
		register size_t j = 0;
		for (j = 0; substr[i][j]; j++) {
			/* If some alphabetic char, besides '-', is found in the
			 * string, we have no range */
			if (substr[i][j] != '-' && (substr[i][j] < 48
			|| substr[i][j] > 57)) {
/*				if (ranges_ok)
					free(range_array);
				ranges_ok = 0; */
				break;
			}
			/* If a range is found, store its index */
			if (j > 0 && j < substr_len && substr[i][j] == '-' && 
			isdigit(substr[i][j - 1]) && isdigit(substr[i][j + 1]))
				if (ranges_ok < int_array_max)
					range_array[ranges_ok++] = (int)i;
		}

		/* Expand 'sel' only as an argument, not as command */
		if (i > 0 && strcmp(substr[i], "sel") == 0)
			is_sel = (short)i;
	}

			/* ####################################
			 * #       2.a) RANGES EXPANSION      # 
			 * ####################################*/

	 /* Expand expressions like "1-3" to "1 2 3" if all the numbers in
	  * the range correspond to an ELN */

	if (ranges_ok) {
		size_t old_ranges_n = 0;
		register size_t r = 0;

		for (r = 0; r < ranges_ok; r++) {
			size_t ranges_n = 0;
			int *ranges = expand_range(substr[range_array[r] + 
									   (int)old_ranges_n], 1);
			if (ranges) {
				register size_t j = 0;

				for (ranges_n = 0; ranges[ranges_n]; ranges_n++);
				char **ranges_cmd = (char **)NULL;
				ranges_cmd = (char **)xcalloc(args_n + ranges_n + 2, 
											  sizeof(char *));
				for (i = 0; i < (size_t)range_array[r] + old_ranges_n; i++) {
					ranges_cmd[j] = (char *)xcalloc(strlen(substr[i]) + 1, 
													sizeof(char));
					strcpy(ranges_cmd[j++], substr[i]);
				}

				for (i = 0; i < ranges_n; i++) {
					ranges_cmd[j] = (char *)xcalloc(digits_in_num(ranges[i])
													+ 1, sizeof(int));
					sprintf(ranges_cmd[j++], "%d", ranges[i]);
				}

				for (i = (size_t)range_array[r] + old_ranges_n + 1; 
				i <= args_n; i++) {
					ranges_cmd[j] = (char *)xcalloc(strlen(substr[i]) + 1, 
													sizeof(char));
					strcpy(ranges_cmd[j++], substr[i]);
				}
				ranges_cmd[j] = NULL;
				free(ranges);

				for (i = 0; i <= args_n; i++) 
					free(substr[i]);

				substr = (char **)xrealloc(substr, (args_n + ranges_n + 2)
										   * sizeof(char *));

				for (i = 0; i < j; i++) {
					substr[i] = (char *)xcalloc(strlen(ranges_cmd[i])
												+ 1, sizeof(char));
					strcpy(substr[i], ranges_cmd[i]);
					free(ranges_cmd[i]);
				}
				free(ranges_cmd);
				args_n = j - 1;
			}
			old_ranges_n += (ranges_n - 1);
		}
	}

	free(range_array);

				/* ##########################
				 * #   2.b) SEL EXPANSION   # 
				 * ##########################*/

	if (is_sel) {

		if ((size_t)is_sel == args_n)
			sel_is_last = 1;

		if (sel_n) {
			register size_t j = 0;
			char **sel_array = (char **)NULL;
			sel_array = (char **)xcalloc(args_n + sel_n + 2,
										 sizeof(char *));

			for (i = 0; i < (size_t)is_sel; i++) {
				sel_array[j] = (char *)xcalloc(strlen(substr[i]) + 1,
											   sizeof(char));
				strcpy(sel_array[j++], substr[i]);
			}

			for (i = 0; i < sel_n; i++) {
				/* Escape selected filenames and copy them into tmp
				 * array */
				char *esc_str = escape_str(sel_elements[i]);
				if (esc_str) {
					sel_array[j] = (char *)xcalloc(strlen(esc_str) + 1, 
												   sizeof(char));
					strcpy(sel_array[j++], esc_str);
					free(esc_str);
					esc_str = (char *)NULL;
				}
				else {
					fprintf(stderr, _("%s: %s: Error quoting filename\n"), 
							PROGRAM_NAME, sel_elements[j]);
					/* Free elements selected thus far and and all the
					 * input substrings */
					register size_t k = 0;
					for (k = 0; k < j; k++)
						free(sel_array[k]);
					free(sel_array);
					for (k = 0; k <= args_n; k++)
						free(substr[k]);
					free(substr);
					return (char **)NULL;
				}
			}

			for (i = (size_t)is_sel + 1; i <= args_n; i++) {
				sel_array[j] = (char *)xcalloc(strlen(substr[i]) + 1, 
											   sizeof(char));
				strcpy(sel_array[j++], substr[i]);
			}

			for (i = 0; i <= args_n; i++)
				free(substr[i]);

			substr = (char **)xrealloc(substr, (args_n + sel_n + 2) 
									   * sizeof(char *));

			for (i = 0; i < j; i++) {
				substr[i] = (char *)xcalloc(strlen(sel_array[i]) + 1,
										    sizeof(char));
				strcpy(substr[i], sel_array[i]);
				free(sel_array[i]);
			}
			free(sel_array);
			substr[i] = (char *)NULL;
			args_n = j - 1;
		}
		else {
			/* 'sel' is an argument, but there are no selected files. */ 
			fprintf(stderr, _("%s: There are no selected files\n"), 
					PROGRAM_NAME);
			register size_t j = 0;

			for (j = 0; j <= args_n; j++)
				free(substr[j]);
			free(substr);

			return (char **)NULL;
		}
	}

	for (i = 0; i <= args_n; i++) {

				/* ##########################
				 * #   2.c) ELN EXPANSION   # 
				 * ##########################*/

		/* If autocd is set to false, i must be bigger than zero because
		 * the first string in comm_array, the command name, should NOT
		 * be expanded, but only arguments. Otherwise, if the expanded
		 * ELN happens to be a program name as well, this program will
		 * be executed, and this, for sure, is to be avoided */

		/* The 'sort' command take digits as arguments. So, do not expand
		 * ELN's in this case */
		if (strcmp(substr[0], "st") != 0
		&& strcmp(substr[0], "sort") != 0) {
			if (is_number(substr[i])) {

				/* Expand first word only if autocd is set to true */
				if (i == 0 && !autocd && !auto_open)
					continue;

				int num = atoi(substr[i]);
				/* Expand numbers only if there is a corresponding ELN */

				if (num > 0 && num <= (int)files) {
					/* Replace the ELN by the corresponding escaped
					 * filename */
					char *esc_str = escape_str(dirlist[num - 1]);
					if (esc_str) {
						substr[i] = (char *)xrealloc(substr[i], 
												(strlen(esc_str) + 1) * 
												 sizeof(char));
						strcpy(substr[i], esc_str);
						free(esc_str);
						esc_str = (char *)NULL;
					}
					else {
						fprintf(stderr, _("%s: %s: Error quoting "
								"filename\n"), PROGRAM_NAME,
								dirlist[num-1]);
						/* Free whatever was allocated thus far */
						size_t j;
						for (j = 0; j <= args_n; j++)
							free(substr[j]);
						free(substr);
						return (char **)NULL;
					}
				}
			}
		}

		/* #############################################
		 * #   2.d) USER DEFINED VARIABLES EXPANSION   # 
		 * #############################################*/

		if (substr[i][0] == '$' && substr[i][1] != '(') {
			char *var_name = straft(substr[i], '$');
			if (var_name) {
				size_t j;

				for (j = 0; j < usrvar_n; j++) {
					if (strcmp(var_name, usr_var[j].name) == 0) {
						substr[i] = (char *)xrealloc(substr[i], 
							    (strlen(usr_var[j].value) + 1)
							    * sizeof(char));
						strcpy(substr[i], usr_var[j].value);
					}
				}
				free(var_name);
				var_name = (char *)NULL;
			}
			else {
				fprintf(stderr, _("%s: %s: Error getting variable name\n"), 
						PROGRAM_NAME, substr[i]);
				size_t j;

				for (j = 0; j <= args_n; j++)
					free(substr[j]);
				free(substr);

				return (char **)NULL;
			}
		}
	}

	/* #### 3) NULL TERMINATE THE INPUT STRING ARRAY #### */
	substr = (char **)xrealloc(substr, sizeof(char *) * (args_n + 2));	
	substr[args_n + 1] = (char *)NULL;

	/* If the command is internal, go on for more expansions; else, just
	 * return the input string array */

	if(!is_internal(substr[0]))
		return substr;

	/* #############################################################
	 * #			   ONLY FOR INTERNAL COMMANDS 				   # 
	 * #############################################################*/

	/* Some functions of CliFM are purely internal, that is, they are not
	 * wrappers of a shell command and do not call the system shell at all.
	 * For this reason, some expansions normally made by the system shell
	 * must be made here (in the lobby [got it?]) in order to be able to 
	 * understand these expansions at all. These functions are properties,
	 * selection, and trash.
	 *  */

	int *glob_array = (int *)xnmalloc(int_array_max, sizeof(int)); 
	int *braces_array = (int *)xnmalloc(int_array_max, sizeof(int)); 
	size_t glob_n = 0, braces_n = 0;

	for (i = 0; substr[i]; i++) {

		/* Do not perform any of the expansions below for selected
		 * elements: they are full path filenames that, as such, do not
		 * need any expansion */
		if (is_sel) { /* is_sel is true only for the current input and if
			there was some "sel" keyword in it */
			/* Strings between is_sel and sel_n are selected filenames */
			if (i >= (size_t)is_sel && i <= sel_n)
				continue;
		}

		/* The search function admits a path as second argument. So, if
		 * the command is search, perform the expansions only for the
		 * first parameter, if any. */
		if (substr[0][0] == '/' && i != 1)
			continue;

				/* ############################
				 * #    4) TILDE EXPANSION    # 
				 * ###########################*/

		 /* (replace "~" by "/home/user") */
/*		if (strncmp (substr[i], "~", 1) == 0) { */
		if (*substr[i] == '~') {
			/* tilde_expand() is provided by the readline lib */
			char *exp_path = tilde_expand(substr[i]);
			if (exp_path) {
				substr[i] = (char *)xrealloc(substr[i], 
									 (strlen(exp_path) + 1) * sizeof(char));
				strcpy(substr[i], exp_path);
			}
			free(exp_path);
			exp_path = (char *)NULL;
		}

		register size_t j = 0;
		for (j = 0; substr[i][j]; j++) {

			/* If a brace is found, store its index */
			if (substr[i][j] == '{' && substr[i][j + 1] != 0x00
			&& substr[i][j + 1] != '}' && substr[i][j - 1] != 0x00
			&& substr[i][j - 1] != '\\') {
				size_t k;

				for (k = j + 1; substr[i][k] && substr[i][k]!='}'; k++) {
					if (substr[i][k] == ',' && substr[i][k - 1] != '\\'
					&& braces_n < int_array_max) {
						braces_array[braces_n++] = (int)i;
						break;
					}
				}
			}

			/* Check for glob chars */
			if (substr[i][j] == '*' || substr[i][j] == '?' 
			|| (substr[i][j] == '[' && substr[i][j + 1] != 0x20))
			/* Strings containing these characters are taken as wildacard 
			 * patterns and are expanded by the glob function. See man (7) 
			 * glob */
				if (glob_n < int_array_max)
					glob_array[glob_n++] = (int)i;
		}
	}

				/* ###########################
				 * #    5) GLOB EXPANSION    # 
				 * ###########################*/

	/* Do not expand if command is deselect or untrash, just to allow the
	 * use of "*" for both commands: "ds *" and "u *" */
	if (glob_n && strcmp(substr[0], "ds") != 0 
	&& strcmp(substr[0], "desel") != 0 
	&& strcmp(substr[0], "u") != 0
	&& strcmp(substr[0], "undel") != 0 
	&& strcmp(substr[0], "untrash") != 0) {
	 /*	1) Expand glob
		2) Create a new array, say comm_array_glob, large enough to store
		   the expanded glob and the remaining (non-glob) arguments 
		   (args_n+gl_pathc)
		3) Copy into this array everything before the glob 
		   (i=0;i<glob_char;i++)
		4) Copy the expanded elements (if none, copy the original element, 
		   comm_array[glob_char])
		5) Copy the remaining elements (i=glob_char+1;i<=args_n;i++)
		6) Free the old comm_array and fill it with comm_array_glob 
	  */
		size_t old_pathc = 0;
		/* glob_array stores the index of the globbed strings. However,
		 * once the first expansion is done, the index of the next globbed
		 * string has changed. To recover the next globbed string, and
		 * more precisely, its index, we only need to add the amount of
		 * files matched by the previous instances of glob(). Example:
		 * if original indexes were 2 and 4, once 2 is expanded 4 stores
		 * now some of the files expanded in 2. But if we add to 4 the
		 * amount of files expanded in 2 (gl_pathc), we get now the
		 * original globbed string pointed by 4.
		*/
		register size_t g = 0;
		for (g = 0; g < (size_t)glob_n; g++){
			glob_t globbuf;
			glob(substr[glob_array[g] + (int)old_pathc], 0, NULL, &globbuf);

			if (globbuf.gl_pathc) {
				register size_t j = 0;
				char **glob_cmd = (char **)NULL;
				glob_cmd = (char **)xcalloc(args_n + globbuf.gl_pathc + 1, 
											sizeof(char *));

				for (i = 0; i < ((size_t)glob_array[g] + old_pathc); i++) {
					glob_cmd[j] = (char *)xcalloc(strlen(substr[i]) + 1, 
												  sizeof(char));
					strcpy(glob_cmd[j++], substr[i]);
				}

				for (i = 0; i < globbuf.gl_pathc; i++) {
					/* Escape the globbed filename and copy it*/
					char *esc_str = escape_str(globbuf.gl_pathv[i]);
					if (esc_str) {
						glob_cmd[j] = (char *)xcalloc(strlen(esc_str)
													+ 1, sizeof(char));
						strcpy(glob_cmd[j++], esc_str);
						free(esc_str);
					}
					else {
						fprintf(stderr, _("%s: %s: Error quoting "
										  "filename\n"), PROGRAM_NAME,
										  globbuf.gl_pathv[i]);
						register size_t k = 0;
						for (k = 0; k < j; k++)
							free(glob_cmd[k]);
						free(glob_cmd);
						glob_cmd = (char **)NULL;
						for (k = 0; k <= args_n; k++)
							free(substr[k]);
						free(substr);
						return (char **)NULL;
					}
				}

				for (i = (size_t)glob_array[g] + old_pathc + 1;
				i <= args_n; i++) {
					glob_cmd[j] = (char *)xcalloc(strlen(substr[i])
												  + 1, sizeof(char));
					strcpy(glob_cmd[j++], substr[i]);
				}
				glob_cmd[j] = (char *)NULL;

				for (i = 0; i <= args_n; i++) 
					free(substr[i]);
				substr = (char **)xrealloc(substr, 
										(args_n+globbuf.gl_pathc + 1)
										* sizeof(char *));

				for (i = 0;i < j; i++) {
					substr[i] = (char *)xcalloc(strlen(glob_cmd[i])
												+ 1, sizeof(char));
					strcpy(substr[i], glob_cmd[i]);
					free(glob_cmd[i]);
				}
				args_n = j-1;
				free(glob_cmd);
			}
			old_pathc += (globbuf.gl_pathc-1);
			globfree(&globbuf);
		}
	}

	free(glob_array);

				/* #############################
				 * #    6) BRACES EXPANSION    # 
				 * #############################*/

	if (braces_n) { /* If there is some braced parameter... */
		/* We already know the indexes of braced strings
		 * (braces_array[]) */
		int old_braces_arg = 0;
		register size_t b = 0;
		for (b = 0; b < braces_n; b++) {
			/* Expand the braced parameter and store it into a new
			 * array */
			int braced_args = brace_expansion(substr[braces_array[b]
											  + old_braces_arg]);
			/* Now we also know how many elements the expanded braced 
			 * parameter has */
			if (braced_args) {
				/* Create an array large enough to store parameters
				 * plus expanded braces */
				char **comm_array_braces = (char **)NULL;
				comm_array_braces = (char **)xcalloc(args_n + 
												(size_t)braced_args, 
												sizeof(char *));
				/* First, add to the new array the paramenters coming
				 * before braces */

				for (i = 0; i < (size_t)(braces_array[b] + old_braces_arg); 
				i++) {
					comm_array_braces[i] = (char *)xcalloc(
												strlen(substr[i])
												 + 1, sizeof(char));
					strcpy(comm_array_braces[i], substr[i]);
				}

				/* Now, add the expanded braces to the same array */
				register size_t j = 0;

				for (j = 0; j < (size_t)braced_args; j++) {
					/* Escape each filename and copy it */
					char *esc_str = escape_str(braces[j]);
					if (esc_str) {
						comm_array_braces[i] = (char *)xcalloc(
												strlen(esc_str)
												+ 1, sizeof(char));
						strcpy(comm_array_braces[i++], esc_str);
						free(esc_str);
					}
					else {
						fprintf(stderr, _("%s: %s: Error quoting "
								"filename\n"), PROGRAM_NAME,
								braces[j]);
						register size_t k = 0;

						for (k = 0; k < braces_n; k++)
							free(braces[k]);
						free(braces);
						braces = (char **)NULL;

						for (k = 0;k < i; k++)
							free(comm_array_braces[k]);
						free(comm_array_braces);
						comm_array_braces = (char **)NULL;

						for (k = 0; k < args_n; k++)
							free(substr[k]);
						free(substr);

						return (char **)NULL;
					}
					free(braces[j]);
				}
				free(braces);
				braces = (char **)NULL;
				/* Finally, add, if any, those parameters coming after
				 * the braces */

				for (j = (size_t)(braces_array[b] + old_braces_arg) + 1;
				j <= args_n; j++) {
					comm_array_braces[i] = (char *)xcalloc(
												    strlen(substr[j])
													+ 1, sizeof(char));
					strcpy(comm_array_braces[i++], substr[j]);
				}

				/* Now, free the old comm_array and copy to it our new
				 * array containing all the parameters, including the
				 * expanded braces */
				for (j = 0; j <= args_n; j++)
					free(substr[j]);
				substr = (char **)xrealloc(substr, (args_n +
										   (size_t)braced_args + 1) *
										   sizeof(char *));

				args_n = i - 1;

				for (j = 0; j < i; j++) {
					substr[j] = (char *)xcalloc(strlen(
										comm_array_braces[j]) + 1,
										sizeof(char));
					strcpy(substr[j], comm_array_braces[j]);
					free(comm_array_braces[j]);
				}

				old_braces_arg += (braced_args - 1);
				free(comm_array_braces);
			}
		}
	}

	free(braces_array);

	/* #### 7) NULL TERMINATE THE INPUT STRING ARRAY (again) #### */
	substr = (char **)xrealloc(substr, (args_n + 2) * sizeof(char *));
	substr[args_n + 1] = (char *)NULL;

	return substr;
}

char *
rl_no_hist(const char *prompt)
{
	stifle_history(0); /* Prevent readline from using the history
	setting */
	char *input = readline(prompt);
	unstifle_history(); /* Reenable history */
	read_history(HIST_FILE); /* Reload history lines from file */

	if (input) {

		/* Make sure input isn't empty string */
		if (!*input) {
			free(input);
			return (char *)NULL;
		}

		/* Check we have some non-blank char */
		int no_blank = 0;
		char *p = input;
		while (*p) {
			if (*p != 0x20 && *p != '\n' && *p != '\t') {
				no_blank = 1;
				break;
			}
			p++;
		}

		if (!no_blank) {
			free(input);
			return (char *)NULL;
		}

		return input;
	}

	return (char *)NULL;
}

int
get_sel_files(void)
/* Get elements currently in the Selection Box, if any. */
{
	if (!selfile_ok)
		return EXIT_FAILURE;

	/* First, clear the sel array, in case it was already used */
	if (sel_n > 0) {
		size_t i;
		for (i = 0; i < sel_n; i++)
			free(sel_elements[i]);
	}
/*	free(sel_elements); */
	sel_n = 0;
	/* Open the tmp sel file and load its contents into the sel array */
	FILE *sel_fp = fopen(sel_file_user, "r");
/*	sel_elements=xcalloc(1, sizeof(char *)); */
	if (sel_fp) {
		/* Since this file contains only paths, I can be sure no line
		 * length will be larger than PATH_MAX */
		char sel_line[PATH_MAX] = "";
		while (fgets(sel_line, sizeof(sel_line), sel_fp)) {
			size_t line_len = strlen(sel_line);
			sel_line[line_len - 1] = 0x00;
			sel_elements = (char **)xrealloc(sel_elements, (sel_n + 1)
											 * sizeof(char *));
			sel_elements[sel_n] = (char *)xcalloc(line_len+1,
												  sizeof(char));
			strcpy(sel_elements[sel_n++], sel_line);
		}
		fclose(sel_fp);
		return EXIT_SUCCESS;
	}
	else
		return EXIT_FAILURE;
}

char *
prompt(void)
/* Print the prompt and return the string entered by the user (to be
 * parsed later by parse_input_str()) */
{
	/* Remove all final slash(es) from path, if any */
	size_t path_len = strlen(path), i;
	for (i = path_len - 1; path[i] && i > 0; i--) {
		if (path[i] != '/')
			break;
		else
			path[i] = 0x00;
	}

	if (welcome_message) {
		printf(_("%sCliFM, the anti-eye-candy, KISS file manager%s\n"
			   "%sEnter 'help' or '?' for instructions.%s\n"), 
			   welcome_msg_color, NC, default_color, NC);
		welcome_message = 0;
	}

	/* Print the tip of the day (only on first run) */
	if (tips) {
		static int first_run = 1;
		if (first_run) {
			print_tips(0);
			first_run = 0;
		}
	}

	/* Execute prompt commands, if any, and only if external commands
	 * are allowed */
	if (ext_cmd_ok && prompt_cmds_n > 0) 
		for (i = 0; i < prompt_cmds_n; i++)
			launch_execle(prompt_cmds[i]);

	/* Update trash and sel file indicator on every prompt call */
	if (trash_ok) {
		trash_n = (size_t)count_dir(TRASH_FILES_DIR);
		if (trash_n <= 2)
			trash_n = 0;
	}

	get_sel_files();

	/* Messages are categorized in three groups: errors, warnings, and
	 * notices. The kind of message should be specified by the function
	 * printing the message itself via a global enum: pmsg, with the
	 * following values: nomsg, error, warning, and notice. */
	char msg_str[17] = "";
	/* 11 == length of color_b + letter + NC_b + null ??? */
	if (msgs_n) {
		/* Errors take precedence over warnings, and warnings over
		 * notices. That is to say, if there is an error message AND a
		 * warning message, the prompt will always display the error
		 * message sign: a red 'E'. */
		switch (pmsg) {
		case nomsg: break;
		case error: sprintf(msg_str, "%sE%s", red_b, NC_b); break;
		case warning: sprintf(msg_str, "%sW%s", yellow_b, NC_b); break;
		case notice: sprintf(msg_str, "%sN%s", green_b, NC_b); break;
		default: break;
		}
	}

	/* Generate the prompt string */

	/* First, grab and decode the prompt line of the config file (stored
	 * in encoded_prompt at startup) */
	char *decoded_prompt = decode_prompt(encoded_prompt);

	/* Emergency prompt, just in case decode_prompt fails */
	if (!decoded_prompt) {
		fprintf(stderr, _("%s: Error decoding prompt line. Using an "
				"emergency prompt\n"), PROGRAM_NAME);
		decoded_prompt = (char *)xnmalloc(9, sizeof(char));
		sprintf(decoded_prompt, "\001\x1b[0m\002> ");
	}

	size_t decoded_prompt_len = strlen(decoded_prompt);

	size_t prompt_length = (size_t)(decoded_prompt_len
		+ (sel_n ? 16 : 0) + (trash_n ? 16 : 0) + ((msgs_n && pmsg)
		? 16: 0) + 6 + sizeof(text_color) + 1);

	/* 16 = color_b({red,green,yellow}_b)+letter (sel, trash, msg)+NC_b; 
	 * 6 = NC_b
	 * 1 = null terminating char */

	char *the_prompt = (char *)xnmalloc(prompt_length, sizeof(char));

	snprintf(the_prompt, prompt_length, "%s%s%s%s%s%s%s%s", 
		(msgs_n && pmsg) ? msg_str : "", (trash_n) ? yellow_b : "", 
		(trash_n) ? "T\001\x1b[0m\002" : "", (sel_n) ? green_b : "", 
		(sel_n) ? "*\001\x1b[0m\002" : "", decoded_prompt, NC_b,
		text_color);

	free(decoded_prompt);

	/* Print error messages, if any. 'print_errors' is set to true by 
	 * log_msg() with the PRINT_PROMPT flag. If NOPRINT_PROMPT is
	 * passed instead, 'print_msg' will be false and the message will
	 * be printed in place by log_msg() itself, without waiting for
	 * the next prompt */
	if (print_msg) {
		for (i = 0; i < (size_t)msgs_n; i++)
			fputs(messages[i], stderr);
		print_msg = 0; /* Print messages only once */
	}

	args_n = 0;

	/* Print the prompt and get user input */
	char *input = (char *)NULL;
	input = readline(the_prompt);

	free(the_prompt);

	if (!input)
	/* Same as 'input == NULL': input is a pointer poiting to no
	 * memory address whatsover */
		return (char *)NULL;

	if (!*input) {
	/* input is not NULL, but a pointer poiting to a memory address
	 * whose first byte is the null byte (\0). In other words, it is
	 * an empty string */
		free(input);
		input = (char *)NULL;
		return (char *)NULL;
	}

	/* Keep a literal copy of the last entered command to compose the
	 * commands log, if needed and enabled */
	if (logs_enabled) {
		if (last_cmd)
			free(last_cmd);
		last_cmd = (char *)xnmalloc(strlen(input) + 1, sizeof(char));
		strcpy(last_cmd, input);
	}

	/* Do not record empty lines, exit, history commands, consecutively 
	 * equal inputs, or lines starting with space */
	if (record_cmd(input))
		add_to_cmdhist(input);

	return input;
}

/*
 * int count_dir(const char *dir_path) //Scandir version
// Count the amount of elements contained in a directory. Receives the
// path to the directory as argument.
{
	int files_n=0;
	struct dirent **dirlist_n=NULL;
	files_n=scandir(dir_path, &dirlist_n, NULL, alphasort);
	if (files_n >= 0) { //this includes implied . and ..
		for (int i=0;i<files_n;i++) free(dirlist_n[i]);
		free(dirlist_n);
	}
	else if (files_n == -1 && errno == ENOMEM) {
		fprintf(stderr, "CliFM: scandir: Out of memory!\n");
		exit(EXIT_FAILURE);
	}
	return files_n;
}*/

size_t
count_dir(const char *dir_path) /* Readdir version */
{
	struct stat file_attrib;

	if (lstat (dir_path, &file_attrib) == -1)
		return 0;

	size_t file_count = 0;
	DIR *dir_p;
	struct dirent *entry;

	if ((dir_p = opendir(dir_path)) == NULL) {
		if (errno == ENOMEM)
			exit(EXIT_FAILURE);
		else
			return -1;
	}

	while ((entry = readdir(dir_p))) 
		file_count++;
	closedir(dir_p);

	return file_count;
}

/*
int count_dir(const char *dir_path) //Getdents version
{
	#define BUF_SIZE 1024
	struct linux_dirent {
		long           	 d_ino;
		off_t          	 d_off;
		unsigned short  d_reclen;
		char           	 d_name[];
	};
	int fd;
	char buf[BUF_SIZE];
	struct linux_dirent *d;
	int bpos;

	fd=open(dir_path, O_RDONLY | O_DIRECTORY);
	if (fd == -1) { perror("open"); return -1; }

	size_t files_n=0;
//	int nread;
	for ( ; ; ) {
		int nread=syscall(SYS_getdents, fd, buf, BUF_SIZE);
		if (nread == -1) { perror("syscall"); return -1; }
		if (nread == 0) break;
		for (bpos=0; bpos < nread;) {
			d=(struct linux_dirent *)(buf + bpos);
			files_n++;
			bpos+=d->d_reclen;
		}
	}
	close(fd);
	return files_n;
}*/

int
skip_nonexec(const struct dirent *entry)
{
	if (access(entry->d_name, R_OK) == -1)
		return 0;

	return 1;
}

int
skip_implied_dot(const struct dirent *entry)
{
	/* In case a directory isn't reacheable, like a failed
	 * mountpoint... */
	struct stat file_attrib;
	if (lstat(entry->d_name, &file_attrib) == -1) {
		fprintf(stderr, _("stat: cannot access '%s': %s\n"),
				entry->d_name, strerror(errno));
		return 0;
	}
	if (strcmp(entry->d_name, ".") !=0
	&& strcmp(entry->d_name, "..") !=0) {
		if (show_hidden == 0) { /* Do not show hidden files (hidden == 0) */
			if (entry->d_name[0] == '.')
				return 0;
			else
				return 1;
		}
		else
			return 1;
	}
	else
		return 0;
}

int
folder_select(const struct dirent *entry)
{
	if (entry->d_type == DT_DIR) {
		if (strcmp(entry->d_name, ".") != 0 
		&& strcmp(entry->d_name, "..") != 0) {
			/* Do not print hidden files (hidden == 0) */
			if (show_hidden == 0) {
				if (entry->d_name[0] == '.')
					return 0;
				else
					return 1;
			}
			else
				return 1;
		}
		else
			return 0;
	}
	else
		return 0;

	return 0; /* Never reached */
}

int
file_select(const struct dirent *entry)
{
	if (entry->d_type != DT_DIR) {
		if (show_hidden == 0) { /* Do not show hidden files */
			if (entry->d_name[0] == '.')
				return 0;
			else
				return 1;
		}
		else
			return 1;
	}
	else
		return 0;

	return 0; /* Never reached */
}

void
colors_list(const char *entry, const int i, const int pad, 
			const int new_line)
/* Print ENTRY using color codes and I as ELN, right padding ENTRY PAD
 * chars and terminating ENTRY with or without a new line char (NEW_LINE
 * 1 or 0 respectivelly) */
{
	size_t i_digits = digits_in_num(i);
							/* Num (i) + space + null byte */
	char *index = (char *)xnmalloc(i_digits + 2, sizeof(char));
	if (i > 0) /* When listing files in CWD */
		sprintf(index, "%d ", i);
	else if (i == -1) /* ELN for entry could not be found */
		sprintf(index, "? ");
	/* When listing files NOT in CWD (called from search_function() and
	 * first argument is a path: "/search_str /path") 'i' is zero. In
	 * this case, no index should be shown at all */
	else
		index[0] = 0x00;

	struct stat file_attrib;
	int ret = 0;
	ret = lstat(entry, &file_attrib);

	if (ret == -1) {
		fprintf(stderr, "%s%-*s: %s%s", index, pad, entry,
				strerror(errno), new_line ? "\n" : "");
		return;
	}

	char *linkname = (char *)NULL;
	#ifdef _LINUX_CAP
	cap_t cap;
	#endif
	switch (file_attrib.st_mode & S_IFMT) {
	case S_IFREG:
/*		if (!(file_attrib.st_mode & S_IRUSR)) */
		if (access(entry, R_OK) == -1)
			printf("%s%s%s%s%s%s%s%-*s", eln_color, index, NC, nf_c,
				   entry, NC, new_line ? "\n" : "", pad, "");
		else if (file_attrib.st_mode & S_ISUID) /* set uid file */
			printf("%s%s%s%s%s%s%s%-*s", eln_color, index, NC, su_c,
				   entry, NC, new_line ? "\n" : "", pad, "");
		else if (file_attrib.st_mode & S_ISGID) /* set gid file */
			printf("%s%s%s%s%s%s%s%-*s", eln_color, index, NC, sg_c,
				   entry, NC, new_line ? "\n" : "", pad, "");
		else {
			#ifdef _LINUX_CAP
			cap = cap_get_file(entry);
			if (cap) {
				printf("%s%s%s%s%s%s%s%-*s", eln_color, index, NC, ca_c,
					   entry, NC, new_line ? "\n" : "", pad, "");
				cap_free(cap);
			}
			else if (file_attrib.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)) {
			#else
			if (file_attrib.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)) {
			#endif
				if (file_attrib.st_size == 0)
					printf("%s%s%s%s%s%s%s%-*s", eln_color, index, NC,
						   ee_c, entry, NC, new_line ? "\n" : "",
						   pad, "");
				else 
					printf("%s%s%s%s%s%s%s%-*s", eln_color, index, NC,
						   ex_c, entry, NC, new_line ? "\n" : "",
						   pad, "");
			}
			else if (file_attrib.st_size == 0)
				printf("%s%s%s%s%s%s%s%-*s", eln_color, index, NC,
					   ef_c, entry, NC, new_line ? "\n" : "",
					   pad, "");
			else
				printf("%s%s%s%s%s%s%s%-*s", eln_color, index, NC,
					   fi_c, entry, NC, new_line ? "\n" : "",
					   pad, "");
		}
			break;

	case S_IFDIR:
		if (access(entry, R_OK|X_OK) != 0)
			printf("%s%s%s%s%s%s%s%-*s", eln_color, index, NC, nd_c, 
				   entry, NC, new_line ? "\n" : "", pad, "");
		else {
			int is_oth_w = 0;
			if (file_attrib.st_mode & S_IWOTH) is_oth_w = 1;
			size_t files_dir = count_dir(entry);
			if (files_dir == 2 || files_dir == 0) { /* If folder is
				empty, it contains only "." and ".." (2 elements). If
				not mounted (ex: /media/usb) the result will be zero. */
				/* If sticky bit dir: green bg. */
				printf("%s%s%s%s%s%s%s%-*s", eln_color, index, NC, 
					   (file_attrib.st_mode & S_ISVTX) ? ((is_oth_w) ? 
					   tw_c : st_c) : ((is_oth_w) ? 
					   ow_c : ed_c), entry, NC, 
					   new_line ? "\n" : "", pad, "");
			}
			else
				printf("%s%s%s%s%s%s%s%-*s", eln_color, index, NC,
					   (file_attrib.st_mode & S_ISVTX) ? ((is_oth_w) ? 
					   tw_c : st_c) : ((is_oth_w) ? 
					   ow_c : di_c), entry, NC, 
					   new_line ? "\n" : "", pad, "");
	}
		break;

	case S_IFLNK:
		linkname = realpath(entry, (char *)NULL);
		if (linkname) {
			printf("%s%s%s%s%s%s%s%-*s", eln_color, index, NC, ln_c, 
				   entry, NC, new_line ? "\n" : "", pad, "");
			free(linkname);
		}
		else printf("%s%s%s%s%s%s%s%-*s", eln_color, index, NC, or_c, 
					entry, NC, new_line ? "\n" : "", pad, "");
		break;

	case S_IFIFO: printf("%s%s%s%s%s%s%s%-*s", eln_color, index, NC,
						 pi_c, entry, NC, new_line ? "\n" : "",
						 pad, ""); break;

	case S_IFBLK: printf("%s%s%s%s%s%s%s%-*s", eln_color, index, NC,
						 bd_c, entry, NC, new_line ? "\n" : "",
						 pad, ""); break;

	case S_IFCHR: printf("%s%s%s%s%s%s%s%-*s", eln_color, index, NC,
						 cd_c, entry, NC, new_line ? "\n" : "",
						 pad, ""); break;

	case S_IFSOCK: printf("%s%s%s%s%s%s%s%-*s", eln_color, index, NC,
						  so_c, entry, NC, new_line ? "\n" : "",
						  pad, ""); break;

	/* In case all of the above conditions are false... */
	default: printf("%s%s%s%s%s%s%s%-*s", eln_color, index, NC, no_c, 
				    entry, NC, new_line ? "\n" : "", pad, "");
	}

	free(index);
}

int
list_dir(void)
/* List files in the current working directory (global variable 'path'). 
 * Uses filetype colors and columns. Return zero on success or one on
 * error */
{
/*	clock_t start = clock(); */

	/* The global variable 'path' SHOULD be set before calling this
	 * function */

	/* "!path" means that the pointer 'path' points to no memory address 
	 * (NULL), while "*path == 0x00" means that the first byte of 
	 * the memory block pointed to by the pointer 'path' is a null char */
	if (!path || *path == 0x00) {
		fprintf(stderr, _("%s: Path is NULL or empty\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	if (file_info) {
		free(file_info);
		file_info = (struct fileinfo *)NULL;
	}

	if (light_mode)
		return list_dir_light();

	files = 0; /* Reset the files counter */

	/* CPU Registers are faster than memory to access, so the variables 
	 * which are most frequently used in a C program can be put in
	 * registers using 'register' keyword. For this reason, the unary
	 * operator "&", cannot be used with registers: they are not memory
	 * addresses. Only for local variables. The most common use for
	 * registers are counters. However the use fo the 'register' keyword
	 * is just a suggestion made to the compiler, which can perfectly
	 * reject it. Most modern compilers decide themselves what should
	 * be put into registers and what not */
	register int i = 0;

	struct dirent **list = (struct dirent **)NULL;
	int total = -1;

	/* Get the list of files in CWD according to sorting method
	 * 0 = none, 1 = name, 2 = size, 3 = atime, 4 = btime,
	 * 5 = ctime, 6 = mtime, 7 = version. Reverse sorting is handled
	 * by the sorting fuctions themselves */
	switch(sort) {
		case 0:
			total = scandir(path, &list, skip_implied_dot, NULL);
		break;

		case 1:
			total = scandir(path, &list, skip_implied_dot, (unicode)
							? m_alphasort : (case_sensitive)
							? xalphasort : alphasort_insensitive);
		break;

		case 2:
			total = scandir(path, &list, skip_implied_dot, size_sort);
		break;

		case 3:
			total = scandir(path, &list, skip_implied_dot, atime_sort);
		break;

		case 4:
		#if defined(HAVE_ST_BIRTHTIME) || defined(__BSD_VISIBLE) \
		|| defined(_STATX)
			total = scandir(path, &list, skip_implied_dot, btime_sort);
		#else
			total = scandir(path, &list, skip_implied_dot, ctime_sort);
		#endif
		break;

		case 5:
			total = scandir(path, &list, skip_implied_dot, ctime_sort);
		break;

		case 6:
			total = scandir(path, &list, skip_implied_dot, mtime_sort);
		break;

		case 7:
		#if __FreeBSD__ || _BE_POSIX
			total = scandir(path, &list, skip_implied_dot, m_alphasort);
		#else
			total = scandir(path, &list, skip_implied_dot, m_versionsort);
		#endif
		break;
	}

	if (total == -1) {
		_err('e', PRINT_PROMPT, "%s: scandir: '%s': %s\n",
			 PROGRAM_NAME, path, strerror(errno));
		if (errno == ENOMEM)
			exit(EXIT_FAILURE);
		else
			return EXIT_FAILURE;
	}

	/* Struct to store information about each file, so that we don't
	 * need to run stat() and strlen() again later, perhaps hundreds
	 * of times */

	if (total > 0)
		file_info = (struct fileinfo *)xnmalloc(
					(size_t)total + 1, sizeof(struct fileinfo));

	if (list_folders_first) {

		/* Store indices of dirs and files into different int arrays,
		 * counting the number of elements for each array too. Symlinks
		 * to directories are counted as directories. */
		int *tmp_files = (int *)xnmalloc((size_t)total + 1, sizeof(int)); 
		int *tmp_dirs = (int *)xnmalloc((size_t)total + 1, sizeof(int));
		size_t filesn = 0, dirsn = 0;

		for (i = 0; i < total; i++) {

			switch (list[i]->d_type) {
				case DT_DIR:
					tmp_dirs[dirsn++] = i;
					break;

				case DT_LNK:
					if (get_link_ref(list[i]->d_name) == S_IFDIR)
						tmp_dirs[dirsn++] = i;
					else
						tmp_files[filesn++] = i;
					break;

				default:
					tmp_files[filesn++] = i;
					break;
			}
		}

		/* Allocate enough space to store all dirs and file names in 
		 * the dirlist array */
		if (!filesn && !dirsn)
			dirlist = (char **)xrealloc(dirlist, sizeof(char *));
		else
			dirlist = (char **)xrealloc(dirlist, (filesn + dirsn + 1) 
										* sizeof(char *));

		/* First copy dir names into the dirlist array */
		for (i = 0; i < (int)dirsn; i++) {
			file_info[i].len = (unicode)
					? u8_xstrlen(list[tmp_dirs[i]]->d_name)
					: strlen(list[tmp_dirs[i]]->d_name);
			/* Store the filename length here, so that we don't need to
			 * run strlen() again later on the same file */
			dirlist[i] = (char *)xnmalloc(file_info[i].len + 1,
							(cont_bt) ? sizeof(char32_t)
							: sizeof(char));
			strcpy(dirlist[i], list[tmp_dirs[i]]->d_name);
		}

		/* Now copy file names */
		register int j;
		for (j = 0; j < (int)filesn; j++) {
			file_info[i].len = (unicode)
				  ? u8_xstrlen(list[tmp_files[j]]->d_name)
				  : strlen(list[tmp_files[j]]->d_name);
			/* cont_bt value is set by u8_xstrlen() */
			dirlist[i] = (char *)xnmalloc(file_info[i].len + 1,
							(cont_bt) ? sizeof(char32_t)
							: sizeof(char));
			strcpy(dirlist[i++], list[tmp_files[j]]->d_name);
		}

		free(tmp_files);
		free(tmp_dirs);
		dirlist[i] = (char *)NULL;

		/* This global variable keeps a record of the amounf of files
		 * in the CWD */
		files = (size_t)i;
	}

	/* If no list_folders_first */
	else {

		files = (size_t)total;

		dirlist = (char **)xrealloc(dirlist, (size_t)(files + 1)
									* sizeof(char *));

		for (i = 0; i < (int)files; i++) {
			file_info[i].len = (unicode) ? u8_xstrlen(list[i]->d_name)
				  : strlen(list[i]->d_name);
			dirlist[i] = xnmalloc(file_info[i].len + 1, (cont_bt)
								? sizeof(char32_t) : sizeof(char));
			strcpy(dirlist[i], list[i]->d_name);
		}

		dirlist[files] = (char *)NULL;
	}

	if (files == 0) {
		if (clear_screen)
			CLEAR;
		printf("%s. ..%s\n", blue, NC);
		free(file_info);
		return EXIT_SUCCESS;
	}

	/* Get the longest element and a few more things about file
	 * information */
	longest = 0; /* Global */
	struct stat file_attrib;
	for (i = (int)files; i--;) {
		int ret = lstat(dirlist[i], &file_attrib);

		if (ret == -1) {
			file_info[i].exists = 0;
			continue;
		}

		file_info[i].exists = 1;
		file_info[i].type = file_attrib.st_mode;
		file_info[i].size = file_attrib.st_size;
		file_info[i].linkdir = -1;

		/* file_name_width contains: ELN's amount of digits + one space 
		 * between ELN and filename + filename length. Ex: '12 name'
		 * contains 7 chars */
		size_t file_name_width = digits_in_num(i + 1) + 1
								 + file_info[i].len;

		/* If the file is a non-empty directory or a symlink to a
		 * non-emtpy directory, and the user has access permision to
		 * it, add to file_name_width the number of digits of the
		 * amount of files this directory contains (ex: 123 (files)
		 * contains 3 digits) + 2 for space and slash between the
		 * directory name and the amount of files it contains. Ex:
		 * '12 name /45' contains 11 chars */

		switch((file_info[i].type & S_IFMT)) {

		case S_IFDIR:
		case S_IFLNK:
			{
			char *linkname = (char *)NULL;

			/* In case of symlink, check if it points to a directory */
			if ((file_info[i].type & S_IFMT) == S_IFLNK) {
				linkname = realpath(dirlist[i], (char *)NULL);

				if (linkname) {
					file_info[i].brokenlink = 0;
					struct stat link_attrib;
					stat(linkname, &link_attrib);

					if ((link_attrib.st_mode & S_IFMT) != S_IFDIR) {
						free(linkname);
						linkname = (char *)NULL;
						file_info[i].linkdir = 0;
					}
					else /* We have a symlink to a valid directory */
						file_info[i].linkdir = 1;
				}
				else {
					file_info[i].linkdir = 0;
					file_info[i].brokenlink = 1;
				}
			}

			/* If dir counter is disabled and/or the file is a symlink
			 * to a non-directory */
			if (!files_counter || file_info[i].linkdir == 0) {
				/* All dirs will be printed as having read access and
				 * being not empty, no matter if they are empty or not or
				 * if the user has read access or not. Disabling the
				 * dir counter could be useful when listing files on a
				 * remote server (or fi the hardware is too old), where
				 * the listing process could become really slow */
				file_info[i].ruser = 1;

				if (file_info[i].linkdir == 0)
					file_info[i].filesn = 0;
				else
					file_info[i].filesn = 3;
			}

			/* Dir counter is enabled, and the file is either a
			 * directory or a symlink to a directory */
			else {
				/* linkname is not null only if the file is a symlink
				 * to an existent directory */
				int retval = count_dir((linkname) ? linkname
									: dirlist[i]);
				if (retval != -1) {

					file_info[i].filesn = retval;
					file_info[i].ruser = 1;

					if (file_info[i].filesn > 2)
						/* Add dir counter lenght (" /num") to
						 * file_name_with */
						file_name_width += 
							digits_in_num((int)file_info[i].filesn)
							+ 2;
				}
				else
					file_info[i].ruser = 0;
			}

			if (linkname)
				free(linkname);

			}
			break;
		}

		if (file_name_width > longest)
			longest = file_name_width;
	}

	/* Get terminal current amount of rows and columns */
	struct winsize w;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
	/* ws_col and ws_row are both unsigned short int according to 
	 * /bits/ioctl-types.h */
	term_cols = w.ws_col; /* This one is global */
	unsigned short term_rows = w.ws_row;

	short reset_pager = 0;
	int c;

	/* Long view mode */
	if (long_view) {
		register size_t counter = 0;

		int max = get_max_long_view(), eln_len_cur = 0, eln_len_bk = 0;

		eln_len = digits_in_num((int)files); /* This is max ELN length */
		eln_len_bk = eln_len;

		for (i = 0; i < (int)files; i++) {

			/* Correct padding, if necessary. Without this correction,
			 * padding for files with different ELN lengths differs.
			 * Ex:
			 * 1) filename (prop)
			 * 12) filename (prop)
			 * But it should be:
			 * 1) filename  (prop)
			 * 12) filename (prop)
			 * */
			eln_len_cur = digits_in_num(i + 1);
			if (eln_len_bk > eln_len_cur)
				eln_len = eln_len_bk - (eln_len_bk - eln_len_cur);
			else
				eln_len = eln_len_bk;

			if (pager) {
				/*	Check terminal amount of rows: if as many filenames
				 * as the amount of available terminal rows has been
				 * printed, stop */

				 /* NOTE: No need to disable keybinds, since every
				  * keystroke is filtered here */
				if ((int)counter > (int)(term_rows - 2)) {
					switch(c = xgetchar()) {
					/* Advance one line at a time */
					case 66: /* Down arrow */
					case 10: /* Enter */
					case 32: /* Space */
						break;
					/* Advance one page at a time */
					case 126: counter = 0; /* Page Down */
						break;
					/* Stop paging (and set a flag to reenable the pager 
					 * later) */
					case 99: /* 'c' */
					case 112: /* 'p' */
					case 113: pager = 0, reset_pager = 1; /* 'q' */
						break;
					/* If another key is pressed, go back one position. 
					 * Otherwise, some filenames won't be listed */
					default: i--; continue;
						break;
					}
				}
				counter++;
			}

			/* Print ELN. The remaining part of the line will be printed
			 * by get_properties() */
			printf("%s%d%s ", eln_color, i + 1, NC);

			get_properties(dirlist[i], (int)long_view, max,
						   file_info[i].len);
		}

		if (reset_pager)
			pager = 1;

		free(file_info);

		/* Print a dividing line between the files list and the
		 * prompt */
		fputs(div_line_color, stdout);
		for (i = term_cols; i--; )
			putchar(div_line_char);
		printf("%s%s", NC, default_color);

		if (sort_switch)
			print_sort_method();

		while (total--)
			free(list[total]);
		free(list);

		return EXIT_SUCCESS;
	}

	/* Normal view mode */

	/* Get possible amount of columns for the dirlist screen */
	size_t columns_n = (size_t)term_cols / (longest + 1); /* +1 for the
	space between file names */

	/* If longest is bigger than terminal columns, columns_n will be 
	 * negative or zero. To avoid this: */
	if (columns_n < 1)
		columns_n = 1;

	/* If we have only three files, we don't want four columns */
	if (columns_n > files)
		columns_n = files;

	if (clear_screen)
		CLEAR;

	short last_column = 0; /* c, reset_pager=0; */
	register size_t counter = 0;

	/* Now we can do the listing */
	for (i = 0; i < (int)files; i++) {

		if (file_info[i].exists == 0)
			continue;

		/* A basic pager for directories containing large amount of
		 * files. What's missing? It only goes downwards. To go
		 * backwards, use the terminal scrollback function */
		if (pager) {
			/* Run the pager only once all columns and rows fitting in
			 * the screen are filled with the corresponding filenames */
			/*		Check rows					Check columns */
			if ((counter / columns_n) > (size_t)(term_rows - 2) 
			&& last_column) {
/*				printf("--Mas--"); */
				switch (c = xgetchar()) {
				/* Advance one line at a time */
				case 66: /* Down arrow */
				case 10: /* Enter */
				case 32: /* Space */
					break;
				/* Advance one page at a time */
				case 126: counter = 0; /* Page Down */
					break;
				/* Stop paging (and set a flag to reenable the pager 
				 * later) */
				case 99:  /* 'c' */
				case 112: /* 'p' */
				case 113: pager = 0, reset_pager = 1; /* 'q' */
					break;
				/* If another key is pressed, go back one position. 
				 * Otherwise, some filenames won't be listed.*/
				default: i--; continue;
					break;
				}
/*				printf("\r\r\r\r\r\r\r\r"); */
			}
			counter++;
		}

		int is_dir = 0;

		#ifdef _LINUX_CAP
		cap_t cap;
		#endif

		if (((size_t)i + 1) % columns_n == 0)
			last_column = 1;
		else
			last_column = 0;

		switch (file_info[i].type & S_IFMT) {
		case S_IFDIR:
			if (!file_info[i].ruser)
				printf("%s%d%s %s%s%s%s", eln_color, i + 1, NC, nd_c, 
					   dirlist[i], NC, (last_column) ? "\n" : "");
			else {
				int is_oth_w = 0;
				if (file_info[i].type & S_IWOTH) is_oth_w = 1;
				/* If folder is empty, it contains only "." and ".." 
				 * (2 elements). If not mounted (ex: /media/usb) the result 
				 * will be zero */
				if (file_info[i].filesn == 2 || file_info[i].filesn == 0) {
					/* If sticky bit dir */
					printf("%s%d%s %s%s%s%s", eln_color, i + 1, NC,
						 (file_info[i].type & S_ISVTX) ? ((is_oth_w) ? 
						 tw_c : st_c) : ((is_oth_w) 
						 ? ow_c : ed_c), dirlist[i], 
						 NC, (last_column) ? "\n" : "");
				}
				else {
					if (files_counter) {
						printf("%s%d%s %s%s%s%s /%zu%s%s", eln_color,
							 i + 1, NC, (file_info[i].type & S_ISVTX)
							 ? ((is_oth_w) ? tw_c : st_c) : ((is_oth_w) 
							 ? ow_c : di_c), dirlist[i], NC,
							 dir_count_color, file_info[i].filesn - 2, 
							 NC, (last_column) ? "\n" : "");
						is_dir = 1;
					}
					else {
						printf("%s%d%s %s%s%s%s", eln_color, i + 1, 
							 NC, (file_info[i].type & S_ISVTX)
							 ? ((is_oth_w) ? tw_c : st_c) : ((is_oth_w) 
							 ? ow_c : di_c), dirlist[i], 
							 NC, (last_column) ? "\n" : "");
					}
				}
			}
			break;

		case S_IFIFO:
			printf("%s%d%s %s%s%s%s", eln_color, i + 1, NC, pi_c, 
					dirlist[i], NC, (last_column) ? "\n" : "");
			break;

		case S_IFLNK:
			{
				if (!file_info[i].brokenlink) {
					if (files_counter && file_info[i].filesn > 2) {
						/* Symlink to dir */
						is_dir = 1;
						printf("%s%d%s %s%s%s%s /%zu%s%s", eln_color,
							i + 1, NC, ln_c, dirlist[i], NC,
							dir_count_color, file_info[i].filesn -2,
							NC, (last_column) ? "\n" : "");
					}
					else /* Symlink to file */
						printf("%s%d%s %s%s%s%s", eln_color, i + 1, NC,
								ln_c, dirlist[i], NC, (last_column)
								? "\n" : "");
				}
				else /* Oops, broken symlink */
					printf("%s%d%s %s%s%s%s", eln_color, i + 1, NC, or_c, 
							dirlist[i], NC, (last_column) ? "\n" : "");
			}
			break;

		case S_IFBLK:
				printf("%s%d%s %s%s%s%s", eln_color, i + 1, NC, bd_c, 
					    dirlist[i], NC, (last_column) ? "\n" : "");
			break;

		case S_IFCHR:
			printf("%s%d%s %s%s%s%s", eln_color, i + 1, NC, cd_c, 
					dirlist[i], NC, (last_column) ? "\n" : "");
			break;

		case S_IFSOCK:
			printf("%s%d%s %s%s%s%s", eln_color, i + 1, NC, so_c, 
				    dirlist[i], NC, (last_column) ? "\n" : "");
			break;

		case S_IFREG:
			/* WARNING: S_IRUSR refers to owner, not current user */
/*			if (!(file_info[i].type & S_IRUSR)) */
			if (access(dirlist[i], R_OK) == -1)
				printf("%s%d%s %s%s%s%s", eln_color, i + 1, NC, nf_c, 
						dirlist[i], NC, (last_column) ? "\n" : "");
			else if (file_info[i].type & S_ISUID) /* set uid file */
				printf("%s%d%s %s%s%s%s", eln_color, i + 1, NC, su_c, 
						dirlist[i], NC, (last_column) ? "\n" : "");
			else if (file_info[i].type & S_ISGID) /* set gid file */
				printf("%s%d%s %s%s%s%s", eln_color, i + 1, NC, sg_c, 
					dirlist[i], NC, (last_column) ? "\n" : "");
			#ifdef _LINUX_CAP
			else if ((cap = cap_get_file(dirlist[i]))) {
				printf("%s%d%s %s%s%s%s", eln_color, i + 1, NC, ca_c, 
						dirlist[i], NC, (last_column) ? "\n" : "");

				cap_free(cap);
			}
			#endif
			else if (file_info[i].type & (S_IXUSR|S_IXGRP|S_IXOTH))
				if (file_info[i].size == 0)
					printf("%s%d%s %s%s%s%s", eln_color, i + 1, NC, ee_c, 
							dirlist[i], NC, (last_column) 
							? "\n" : "");
				else
					printf("%s%d%s %s%s%s%s", eln_color, i + 1, NC, ex_c, 
							dirlist[i], NC, (last_column) 
							? "\n" : "");
			else if (file_info[i].size == 0)
				printf("%s%d%s %s%s%s%s", eln_color, i + 1, NC, ef_c, 
						dirlist[i], NC, (last_column) ? "\n" : "");
			else
				printf("%s%d%s %s%s%s%s", eln_color, i + 1, NC, fi_c, 
						dirlist[i], NC, (last_column) ? "\n" : "");
			break;
			/* In case all of the above conditions are false, we have an
			 * unknown file type */
		default: 
			printf("%s%d%s %s%s%s%s", eln_color, i + 1, NC, no_c, 
					dirlist[i], NC, (last_column) ? "\n" : "");
		}

		if (!last_column) {
			/* Get the difference between the length of longest and the 
			 * current element */
			size_t diff = longest - (digits_in_num(i + 1) + 1 + 
						  file_info[i].len);

			if (is_dir) { /* If a directory, make room for displaying the 
				* amount of files it contains */
				/* Get the amount of digits in the number of files
				 * contained by the listed directory */
				size_t dig_num = digits_in_num(
										(int)file_info[i].filesn - 2);
				/* The amount of digits plus 2 chars for " /" */
				diff -= (dig_num + 2);
			}

			/* Print the spaces needed to equate the length of the
			 * lines */
			/* +1 is for the space between filenames */
			register int j;
			for (j = (int)diff + 1; j--;)
				putchar(' ');
		}
	}

/*	free(file_info); */

	/* If the pager was disabled during listing (by pressing 'c', 'p' or
	 * 'q'), reenable it */
	if (reset_pager)
		pager = 1;

	/* If the last listed element was modulo (in the last column), don't
	 * print anything, since it already has a new line char at the end.
	 * Otherwise, if not modulo (not in the last column), print a new
	 * line, for it has none */
	 if (!last_column)
		putchar('\n');

	/* Print a dividing line between the files list and the prompt */
	fputs(div_line_color, stdout);
	for (i = term_cols; i--; )
		putchar(div_line_char);
	printf("%s%s", NC, default_color);

	fflush(stdout);

	/* If changing sorting method, inform the user about the current
	 * method */
	if (sort_switch)
		print_sort_method();

	/* Free the scandir array */
	/* Whatever time it takes to free this array, it will be faster to
	 * do it after listing files than before (at least theoretically) */
	while (total--)
		free(list[total]);
	free(list);

/*	clock_t end = clock();
	printf("list_dir time: %f\n", (double)(end-start)/CLOCKS_PER_SEC); */

	return EXIT_SUCCESS;
}

int
list_dir_light(void)
/* List files in the current working directory (global variable 'path'). 
 * Unlike list_dir(), however, this function uses no color and runs
 * neither stat() nor count_dir(), which makes it quite faster. Return
 * zero on success or one on error */
{
/*	clock_t start = clock(); */

	files = 0; /* Reset the files counter */

	register int i = 0;

	struct dirent **list = (struct dirent **)NULL;
	int total = -1;

	/* Get the list of files in CWD according to sorting method
	 * 0 = none, 1 = name, 2 = size, 3 = atime, 4 = btime,
	 * 5 = ctime, 6 = mtime, 7 = version */
	switch(sort) {
		case 0:
			total = scandir(path, &list, skip_implied_dot, NULL);
		break;

		case 1:
			total = scandir(path, &list, skip_implied_dot, (unicode)
							? m_alphasort : (case_sensitive)
							? xalphasort : alphasort_insensitive);
		break;

		case 2:
			total = scandir(path, &list, skip_implied_dot, size_sort);
		break;

		case 3:
			total = scandir(path, &list, skip_implied_dot, atime_sort);
		break;

		case 4:
		#if defined(HAVE_ST_BIRTHTIME) || defined(__BSD_VISIBLE) \
		|| defined(_STATX)
			total = scandir(path, &list, skip_implied_dot, btime_sort);
		#else
			total = scandir(path, &list, skip_implied_dot, ctime_sort);
		#endif
		break;

		case 5:
			total = scandir(path, &list, skip_implied_dot, ctime_sort);
		break;

		case 6:
			total = scandir(path, &list, skip_implied_dot, mtime_sort);
		break;

		case 7:
		# if __FreeBSD__ || _BE_POSIX
			total = scandir(path, &list, skip_implied_dot, m_alphasort);
		#else
			total = scandir(path, &list, skip_implied_dot, m_versionsort);
		#endif
		break;
	}

	if (total == -1) {
		_err('e', PRINT_PROMPT, "%s: scandir: '%s': %s\n",
			 PROGRAM_NAME, path, strerror(errno));
		if (errno == ENOMEM)
			exit(EXIT_FAILURE);
		else
			return EXIT_FAILURE;
	}

	file_info = (struct fileinfo *)xnmalloc(
					(size_t)total + 1, sizeof(struct fileinfo));

	files = (size_t)total;

	if (files == 0) {
		if (clear_screen)
			CLEAR;
		puts(". ..\n");
		return EXIT_SUCCESS;
	}

	if (list_folders_first) {

		/* Store indices of dirs and files into different int arrays,
		 * counting the number of elements for each array too. */
		int *tmp_files = (int *)xnmalloc((size_t)total + 1, sizeof(int)); 
		int *tmp_dirs = (int *)xnmalloc((size_t)total + 1, sizeof(int));
		size_t filesn = 0, dirsn = 0;

		for (i = 0; i < total; i++) {
			switch (list[i]->d_type) {
				case DT_DIR:
					tmp_dirs[dirsn++] = i;
					break;

				default:
					tmp_files[filesn++] = i;
					break;
			}
		}

		/* Allocate enough space to store all dirs and file names in 
		 * the dirlist array */
		if (!filesn && !dirsn)
			dirlist = (char **)xrealloc(dirlist, sizeof(char *));
		else
			dirlist = (char **)xrealloc(dirlist, (filesn + dirsn + 1) 
										* sizeof(char *));

		/* First copy dir names into the dirlist array */
		for (i = 0; i < (int)dirsn; i++) {
			/* Store the filename length (and filetype) here, so that
			 * we don't need to run strlen() again later on the same
			 * file */
			file_info[i].len = (unicode)
					? u8_xstrlen(list[tmp_dirs[i]]->d_name)
					: strlen(list[tmp_dirs[i]]->d_name);
			file_info[i].type = list[tmp_dirs[i]]->d_type;
			/* cont_bt value is set by u8_xstrlen() */
			dirlist[i] = (char *)xnmalloc(file_info[i].len + 1,
							(cont_bt) ? sizeof(char32_t)
							: sizeof(char));
			strcpy(dirlist[i], list[tmp_dirs[i]]->d_name);
		}

		/* Now copy file names */
		register int j;
		for (j = 0; j < (int)filesn; j++) {
			file_info[i].len = (unicode)
					? u8_xstrlen(list[tmp_files[j]]->d_name)
					: strlen(list[tmp_files[j]]->d_name);
			file_info[i].type = list[tmp_files[j]]->d_type;
			dirlist[i] = (char *)xnmalloc(file_info[i].len + 1,
								(cont_bt) ? sizeof(char32_t)
								: sizeof(char));
			strcpy(dirlist[i++], list[tmp_files[j]]->d_name);
		}

		free(tmp_files);
		free(tmp_dirs);
		dirlist[i] = (char *)NULL;

		/* This global variable keeps a record of the amounf of files
		 * in the CWD */
		files = (size_t)i;
	}

	/* If no list_folders_first */
	else {
		dirlist = (char **)xrealloc(dirlist, (size_t)(files + 1)
										* sizeof(char *));

		for (i = 0; i < (int)files; i++) {
			file_info[i].len = (unicode) ? u8_xstrlen(list[i]->d_name)
							   : strlen(list[i]->d_name);
			file_info[i].type = list[i]->d_type;
			dirlist[i] = xnmalloc(file_info[i].len + 1, (cont_bt)
								  ? sizeof(char32_t) : sizeof(char));
			strcpy(dirlist[i], list[i]->d_name);
		}
	}

	dirlist[files] = (char *)NULL;

	/* Get the longest filename */
	longest = 0; /* Global */

	for (i = (int)files; i--;) {

		file_info[i].linkdir = -1;

		size_t file_name_width = digits_in_num(i + 1) + 1
								 + file_info[i].len;

		if (classify) {
			/* Increase filename width in one to include the ending
			 * classification char ( one of *@/=| ) */
			switch (file_info[i].type) {
				case DT_DIR:
				case DT_LNK:
				case DT_FIFO:
				case DT_SOCK:
					file_name_width++;
					break;

				case DT_REG:
					/* classify is greater than 1 only if the
					 * ClassifyExec option is enabled, in which case
					 * executable files must be classified as well */
					if (classify > 1 && access(dirlist[i], X_OK) == 0) {
						file_info[i].exec = 1;
						file_name_width++;
					}
					else
						file_info[i].exec = 0;
					break;
			}
		}

		else if (dir_indicator && file_info[i].type == DT_DIR)
			file_name_width++;

		/* Else, there is no filetype indicator at all */

		if (file_name_width > longest)
			longest = file_name_width;
	}

	/* Get terminal current amount of rows and columns */
	struct winsize w;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
	term_cols = w.ws_col; /* This one is global */
	unsigned short term_rows = w.ws_row;

	short reset_pager = 0;
	int c;

	/* Long view mode */
	if (long_view) {
		register size_t counter = 0;

		int max = get_max_long_view(), eln_len_cur = 0, eln_len_bk = 0;

		eln_len = digits_in_num((int)files); /* This is max ELN length */
		eln_len_bk = eln_len;

		for (i = 0; i < (int)files; i++) {

			/* Correct padding, if necessary. Without this correction,
			 * padding for files with different ELN lengths differs.
			 * Ex:
			 * 1) filename (prop)
			 * 12) filename (prop)
			 * But it should be:
			 * 1) filename  (prop)
			 * 12) filename (prop)
			 * */
			eln_len_cur = digits_in_num(i + 1);
			if (eln_len_bk > eln_len_cur)
				eln_len = eln_len_bk - (eln_len_bk - eln_len_cur);
			else
				eln_len = eln_len_bk;

			if (pager) {
				/*	Check terminal amount of rows: if as many filenames
				 * as the amount of available terminal rows has been
				 * printed, stop */

				 /* NOTE: No need to disable keybinds, since every
				  * keystroke is filtered here */
				if ((int)counter > (int)(term_rows - 2)) {
					switch(c = xgetchar()) {
					/* Advance one line at a time */
					case 66: /* Down arrow */
					case 10: /* Enter */
					case 32: /* Space */
						break;
					/* Advance one page at a time */
					case 126: counter = 0; /* Page Down */
						break;
					/* Stop paging (and set a flag to reenable the pager 
					 * later) */
					case 99: /* 'c' */
					case 112: /* 'p' */
					case 113: pager = 0, reset_pager = 1; /* 'q' */
						break;
					/* If another key is pressed, go back one position. 
					 * Otherwise, some filenames won't be listed */
					default: i--; continue;
						break;
					}
				}
				counter++;
			}

			/* Print ELN. The remaining part of the line will be printed
			 * by get_properties() */
			printf("%s%d%s ", eln_color, i + 1, NC);

			get_properties(dirlist[i], (int)long_view, max,
						   file_info[i].len);
		}

		if (reset_pager)
			pager = 1;

		free(file_info);

		/* Print a dividing line between the files list and the
		 * prompt */
		fputs(div_line_color, stdout);
		for (i = term_cols; i--; )
			putchar(div_line_char);
		printf("%s%s", NC, default_color);

		if (sort_switch)
			print_sort_method();

		while (total--)
			free(list[total]);
		free(list);

		return EXIT_SUCCESS;
	}

	/* Normal view mode */

	/* Get possible amount of columns for the dirlist screen */
	size_t columns_n = (size_t)term_cols / (longest + 1); /* +1 for the
	space between file names */

	/* If longest is bigger than terminal columns, columns_n will be 
	 * negative or zero. To avoid this: */
	if (columns_n < 1)
		columns_n = 1;

	/* If we have only three files, we don't want four columns */
	if (columns_n > files)
		columns_n = files;

	if (clear_screen)
		CLEAR;

	short last_column = 0; /* c, reset_pager=0; */
	register size_t counter = 0;

	/* Now we can do the listing */
	for (i = 0; i < (int)files; i++) {

		if (pager) {

			if ((counter / columns_n) > (size_t)(term_rows - 2) 
			&& last_column) {

				switch (c = xgetchar()) {

				/* Advance one line at a time */
				case 66: /* Down arrow */
				case 10: /* Enter */
				case 32: /* Space */
					break;

				/* Advance one page at a time */
				case 126: counter = 0; /* Page Down */
					break;
				/* Stop paging (and set a flag to reenable the pager 
				 * later) */
				case 99:  /* 'c' */
				case 112: /* 'p' */
				case 113: pager = 0, reset_pager = 1; /* 'q' */
					break;

				/* If another key is pressed, go back one position. 
				 * Otherwise, some filenames won't be listed.*/
				default: i--; continue;
					break;
				}
/*				printf("\r\r\r\r\r\r\r\r"); */
			}
			counter++;
		}

		if (((size_t)i + 1) % columns_n == 0)
			last_column = 1;
		else
			last_column = 0;

		/* Print the corresponding entry */
		if (!dir_indicator && !classify)
			/* No filetype indicator at all */
			printf("%s%d%s %s%s", eln_color, i + 1, NC, dirlist[i],
				   (last_column) ? "\n" : "");

		else if (classify) {

			switch(file_info[i].type) {
				case DT_DIR:
					printf("%s%d%s %s/%s", eln_color, i + 1, NC,
							dirlist[i], (last_column) ? "\n" : "");
				break;

				case DT_LNK:
					printf("%s%d%s %s@%s", eln_color, i + 1, NC,
							dirlist[i], (last_column) ? "\n" : "");
				break;

				case DT_FIFO:
					printf("%s%d%s %s|%s", eln_color, i + 1, NC,
							dirlist[i], (last_column) ? "\n" : "");
				break;

				case DT_SOCK:
					printf("%s%d%s %s=%s", eln_color, i + 1, NC,
							dirlist[i], (last_column) ? "\n" : "");
				break;

				case DT_REG:
					if (file_info[i].exec)
						printf("%s%d%s %s*%s", eln_color, i + 1, NC,
						 		dirlist[i], (last_column) ? "\n" : "");
					else
						printf("%s%d%s %s%s", eln_color, i + 1, NC,
								dirlist[i], (last_column) ? "\n" : "");
				break;

				default:
						printf("%s%d%s %s%s", eln_color, i + 1, NC,
							   dirlist[i], (last_column) ? "\n" : "");
				break;
			}
		} 

		else { /* Only dirs */
			if (file_info[i].type == DT_DIR)
				printf("%s%d%s %s/%s", eln_color, i + 1, NC, dirlist[i],
					   (last_column) ? "\n" : "");
			else
				printf("%s%d%s %s%s", eln_color, i + 1, NC, dirlist[i],
					   (last_column) ? "\n" : "");
		}

		if (!last_column) {
			/* Get the difference between the length of longest and the 
			 * current element */
			size_t diff = longest - (digits_in_num(i + 1) + 1 + 
						  file_info[i].len);

			/* Print the spaces needed to equate the length of the
			 * lines */
			register int j;
			for (j = (int)diff; j--;)
				putchar(' ');

			/* A final space is only needed if no filetype indicator
			 * has been added to the filename */
			if (classify) {
				switch (file_info[i].type) {
					case DT_DIR:
					case DT_LNK:
					case DT_FIFO:
					case DT_SOCK:
						break;

					case DT_REG:
						if (!file_info[i].exec)
							putchar(' ');
						break;

					default:
						putchar(' ');
						break;
				}
			}

			else if (dir_indicator) {
				if (file_info[i].type != DT_DIR)
					putchar(' ');
			}
			else
				putchar(' ');
		}
	}

/*	free(file_info); */

	/* If the pager was disabled during listing (by pressing 'c', 'p' or
	 * 'q'), reenable it */
	if (reset_pager)
		pager = 1;

	/* If the last listed element was modulo (in the last column), don't
	 * print anything, since it already has a new line char at the end.
	 * Otherwise, if not modulo (not in the last column), print a new
	 * line, for it has none */
	 if (!last_column)
		putchar('\n');

	/* Print a dividing line between the files list and the prompt */
	fputs(div_line_color, stdout);
	for (i = term_cols; i--; )
		putchar(div_line_char);
	printf("%s%s", NC, default_color);

	fflush(stdout);

	/* If changing sorting method, inform the user about the current
	 * method */
	if (sort_switch)
		print_sort_method();

	/* Free the scandir array */
	while (total--)
		free(list[total]);
	free(list);

/*	clock_t end = clock();
	printf("list_dir time: %f\n", (double)(end-start)/CLOCKS_PER_SEC); */

	return EXIT_SUCCESS;
}

void
get_prompt_cmds(void)
{
	if (!config_ok)
		return;

	FILE *config_file_fp;
	config_file_fp = fopen(CONFIG_FILE, "r");
	if (!config_file_fp) {
		_err('e', PRINT_PROMPT, "%s: prompt: '%s': %s\n",
			 PROGRAM_NAME, CONFIG_FILE, strerror(errno));
		return;
	}

	if (prompt_cmds_n) {
		size_t i;
		for (i = 0; i < prompt_cmds_n; i++)
			free(prompt_cmds[i]);
		free(prompt_cmds);
		prompt_cmds = (char **)NULL;
		prompt_cmds_n = 0;
	}

	int prompt_line_found = 0;
	char *line = (char *)NULL;
	size_t line_size = 0;
	ssize_t line_len = 0;

	while ((line_len = getline(&line, &line_size,
	config_file_fp)) > 0) {

		if (prompt_line_found) {
			if (strncmp(line, "#END OF PROMPT", 14) == 0)
				break;
			if (*line != '#') {
				prompt_cmds = (char **)xrealloc(prompt_cmds,
										(prompt_cmds_n + 1)
										* sizeof(char *));
				prompt_cmds[prompt_cmds_n] = (char *)xcalloc(
											  strlen(line) +1,
											  sizeof(char));
				strcpy(prompt_cmds[prompt_cmds_n++], line);
			}
		}

		else if (strncmp(line, "#PROMPT", 7) == 0) 
			prompt_line_found = 1;
	}

	free(line);

	fclose(config_file_fp);
}

void
get_aliases(void)
{
	if (!config_ok)
		return;

	FILE *config_file_fp;
	config_file_fp = fopen(CONFIG_FILE, "r");
	if (!config_file_fp) {
		_err('e', PRINT_PROMPT, "%s: alias: '%s': %s\n",
			 PROGRAM_NAME, CONFIG_FILE, strerror(errno));
		return;
	}

	if (aliases_n) {
		size_t i;
		for (i = 0; i < aliases_n; i++)
			free(aliases[i]);
		free(aliases);
		aliases = (char **)NULL;
		aliases_n = 0;
	}


	char *line = (char *)NULL;
	size_t line_size = 0;
	ssize_t line_len = 0;

	while ((line_len = getline(&line, &line_size,
	config_file_fp)) > 0) {

		if (strncmp(line, "alias ", 6) == 0) {
			char *alias_line = strchr(line, ' ');
			if (alias_line) {
				alias_line++;
				aliases = (char **)xrealloc(aliases, (aliases_n + 1)
											* sizeof(char *));
				aliases[aliases_n] = (char *)xcalloc(
												strlen(alias_line)
												+ 1, sizeof(char));
				strcpy(aliases[aliases_n++], alias_line);
			}
		}
	}

	free(line);

	fclose(config_file_fp);
}

char **
check_for_alias(char **comm)
/* Returns the parsed aliased command in an array of string, if
 * matching alias is found, or NULL if not. */
{
	if (aliases_n == 0 || !aliases)
		return (char **)NULL;

	char *aliased_cmd = (char *)NULL;
	size_t comm_len = strlen(comm[0]);
	char *comm_tmp = (char *)xcalloc(comm_len + 2, sizeof(char));
	/* Look for this string: "command=", in the aliases file */
	snprintf(comm_tmp, comm_len + 2, "%s=", comm[0]);

	register size_t i;
	for (i = 0; i < aliases_n; i++) {

		/* If a match is found */
		if (strncmp(aliases[i], comm_tmp, comm_len + 1) == 0) {

			/* Get the aliased command */
			aliased_cmd = strbtw(aliases[i], '\'', '\'');

			if (!aliased_cmd) {
				free(comm_tmp);
				return (char **)NULL;
			}

			if (*aliased_cmd == 0x00) { /* zero length */
				free(comm_tmp);
				free(aliased_cmd);
				return (char **)NULL;
			}

			args_n = 0; /* Reset args_n to be used by parse_input_str() */

			/* Parse the aliased cmd */
			char **alias_comm = parse_input_str(aliased_cmd);
			free(aliased_cmd);

			if (!alias_comm) {
				args_n = 0;
				fprintf(stderr, _("%s: Error parsing aliased command\n"), 
						PROGRAM_NAME);
				free(comm_tmp);
				return (char **)NULL;
			}

			register size_t j;

			/* Add input parameters, if any, to alias_comm */
			if (comm[1]) {
				for (j = 1; comm[j]; j++) {
					alias_comm = (char **)xrealloc(alias_comm, (++args_n
												+ 1) * sizeof(char *));
					alias_comm[args_n] = (char *)xcalloc(strlen(comm[j])
												+ 1, sizeof(char));
					strcpy(alias_comm[args_n], comm[j]);
				}
			}

			/* Add a terminating NULL string */
			alias_comm = (char **)xrealloc(alias_comm, (args_n + 2)
										   * sizeof(char *));
			alias_comm[args_n + 1] = (char *)NULL;

			/* Free original command */
			for (j = 0; comm[j]; j++) 
				free(comm[j]);
			free(comm);	

			free(comm_tmp);

			return alias_comm;
		}
	}

	free(comm_tmp);

	return (char **)NULL;
}

int
exec_cmd(char **comm)
/* Take the command entered by the user, already splitted into substrings
 * by parse_input_str(), and call the corresponding function. Return zero
 * in case of success and one in case of error */
{
/*	if (!comm || *comm[0] == 0x00)
		return EXIT_FAILURE; */

	fputs(default_color, stdout);

	/* Exit flag. exit_code is zero (sucess) by default. In case of error
	 * in any of the functions below, it will be set to one (failure).
	 * This will be the value returned by this function. Used by the \z
	 * escape code in the prompt to print the exit status of the last
	 * executed command */
	exit_code = EXIT_SUCCESS;

	/* If a user defined variable */
	if (flags & IS_USRVAR_DEF) {
		flags &= ~IS_USRVAR_DEF;

		exit_code = create_usr_var(comm[0]);
		return exit_code;
	}

	if (comm[0][0] == ';' || comm[0][0] == ':') {
		if (!comm[0][1]) {
			/* If just ":" or ";", launch the default shell */
			char *cmd[] = { sys_shell, NULL };
			if (launch_execve(cmd, FOREGROUND) != EXIT_SUCCESS)
				exit_code = EXIT_FAILURE;
			return exit_code;
		}
		/* If double semi colon or colon (or ";:" or ":;") */
		else if (comm[0][1] == ';' || comm[0][1] == ':') {
			fprintf(stderr, _("%s: '%s': Syntax error\n"),
					PROGRAM_NAME, comm[0]);
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}
	}

				/* ###############################
				 * #	 AUTOCD & AUTOOPEN (1)   #
				 * ############################### */

	if (autocd || auto_open) {
		/* Expand tilde */
		if (*comm[0] == '~') {
			char *exp_path = tilde_expand(comm[0]);
			if (exp_path) {
				comm[0] = (char *)xrealloc(comm[0],	(strlen(exp_path)
										   + 1) * sizeof(char));
				strcpy(comm[0], exp_path);
				free(exp_path);
			}
		}

		/* Deescape the string */
		if (strchr(comm[0], '\\')) {
			char *deq_str = dequote_str(comm[0], 0);
			if (deq_str) {
				strcpy(comm[0], deq_str);
				free(deq_str);
			}
		}
	}

	/* Only autocd or auto-open here if not absolute path, and if there
	 * is no second argument or if second argument is "&" */
	if (*comm[0] != '/' && (autocd || auto_open)
	&& (!comm[1] || (*comm[1] == '&' && comm[1][1] == '\0'))) {

		char *tmp = comm[0];
		size_t i, tmp_len = strlen(tmp);

		if (tmp[tmp_len - 1] == '/')
			tmp[tmp_len - 1] = 0x00;

		for (i = files; i--;) {

			if (*tmp != *dirlist[i])
				continue;

			if (strcmp(tmp, dirlist[i]) != 0)
				continue;

			/* In light mode, stat() is never called, so that we
			 * don't have the stat macros (S_IFDIR and company), but
			 * only those provided by scandir (DT_DIR and company) */
			if (autocd && (((light_mode) ? file_info[i].type
			: file_info[i].type & S_IFMT) == ((light_mode)
			? DT_DIR : S_IFDIR) || file_info[i].linkdir == 1)) {
				exit_code = cd_function(tmp);
				return exit_code;
			}

			else if (auto_open && (((light_mode) ? file_info[i].type
			: file_info[i].type & S_IFMT) == ((light_mode)
			? DT_REG : S_IFREG) || ((light_mode) ? file_info[i].type
			: file_info[i].type & S_IFMT) == ((light_mode)
			? DT_LNK : S_IFLNK))) {
				char *cmd[] = { "open", comm[0],
								(comm[1]) ? comm[1] : NULL,
								NULL }; 
				exit_code = open_function(cmd);
				return exit_code;
			}

			else
				break;
		}
	}


	/* The more often a function is used, the more on top should it be
	 * in this if...else..if chain. It will be found faster this way. */

	/* ####################################################
	 * #				 BUILTIN COMMANDS 				  #
	 * ####################################################*/


	/*          ############### CD ##################     */
	if (strcmp(comm[0], "cd") == 0) {
		if (strcmp(comm[1], "--help") == 0)
			puts(_("Usage: cd [ELN/DIR]"));

		else if (strncmp(comm[1], "sftp://", 7) == 0)
			exit_code = remote_ssh(comm[1] + 7, (comm[2]) ? comm[2]
								   : NULL);
		else if (strncmp(comm[1], "smb://", 6) == 0)
			exit_code = remote_smb(comm[1] + 6, (comm[2]) ? comm[2]
								   : NULL);
		else if (strncmp(comm[1], "ftp://", 6) == 0)
			exit_code = remote_ftp(comm[1] + 6, (comm[2]) ? comm[2]
								   : NULL);
		else
			exit_code = cd_function(comm[1]);
	}

	/*         ############### OPEN ##################     */
	else if (strcmp(comm[0], "o") == 0
	|| strcmp(comm[0], "open") == 0) {
		if (!comm[1]) {
			puts(_("Usage: o, open ELN/FILE [APPLICATION]"));
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}
		else if (strcmp(comm[1], "--help") == 0)
			puts(_("Usage: o, open ELN/FILE [APPLICATION]"));
		else if (strncmp(comm[1], "sftp://", 7) == 0)
			exit_code = remote_ssh(comm[1] + 7, (comm[2]) ? comm[2]
								   : NULL);
		else if (strncmp(comm[1], "smb://", 6) == 0)
			exit_code = remote_smb(comm[1] + 6, (comm[2]) ? comm[2]
								   : NULL);
		else if (strncmp(comm[1], "ftp://", 6) == 0)
			exit_code = remote_ftp(comm[1] + 6, (comm[2]) ? comm[2]
								   : NULL);
		else
			exit_code = open_function(comm);
	}

	else if (strcmp(comm[0], "rf") == 0
	|| strcmp(comm[0], "refresh") == 0) {
		if (cd_lists_on_the_fly) {
			while (files--)
				free(dirlist[files]);
			exit_code = list_dir();
		}
	}

	/*         ############### BOOKMARKS ##################     */
	else if (strcmp(comm[0], "bm") == 0
	|| strcmp(comm[0], "bookmarks") == 0) {
		if (comm[1] && strcmp(comm[1], "--help") == 0) {
			puts(_("Usage: bm, bookmarks [a, add FILE] [d, del] [edit]"));
			return EXIT_SUCCESS;
		}
		/* Disable keyboard shortcuts. Otherwise, the function will still
		 * be waiting for input while the screen have been taken by 
		 * another function */
		kbind_busy = 1;
		/* Disable TAB completion while in Bookmarks */
		rl_attempted_completion_function = NULL;
		exit_code = bookmarks_function(comm);
		/* Reenable TAB completion */
		rl_attempted_completion_function = my_rl_completion;
		/* Reenable keyboard shortcuts */
		kbind_busy = 0;
	}

	/*       ############### BACK AND FORTH ##################     */
	else if (strcmp(comm[0], "b") == 0 || strcmp(comm[0], "back") == 0)
		exit_code = back_function (comm);

	else if (strcmp(comm[0], "f") == 0 || strcmp(comm[0], "forth") == 0)
		exit_code = forth_function (comm);

	else if (strcmp(comm[0], "bh") == 0
	|| strcmp(comm[0], "fh") == 0) {
		int i;
		for (i = 0; i < dirhist_total_index; i++) {
			if (i == dirhist_cur_index)
				printf("%d %s%s%s%s\n", i + 1, green, old_pwd[i], NC, 
						default_color);
			else 
				printf("%d %s\n", i + 1, old_pwd[i]);
		}
	}

	/*     ############### COPY AND MOVE ##################     */
	else if (strcmp(comm[0], "cp") == 0 || strcmp(comm[0], "paste") == 0 
	|| strcmp(comm[0], "mv") == 0 || strcmp(comm[0], "c") == 0 
	|| strcmp(comm[0], "m") == 0 || strcmp(comm[0], "v") == 0) {
		if (strcmp(comm[0], "c") == 0) {
			comm[0] = (char *)xrealloc(comm[0], 3 * sizeof(char *));
			strncpy(comm[0], "cp", 3);
		}
		else if (strcmp(comm[0], "m") == 0) {
			comm[0] = (char *)xrealloc(comm[0], 3 * sizeof(char *));
			strncpy(comm[0], "mv", 3);
		}
		else if (strcmp(comm[0], "v") == 0) {
			comm[0] = (char *)xrealloc(comm[0], 6 * sizeof(char *));
			strncpy(comm[0], "paste", 6);
		}
		kbind_busy = 1;
		exit_code = copy_function(comm);
		kbind_busy = 0;
	}

	/*         ############### TRASH ##################     */	
	else if (strcmp(comm[0], "tr") == 0 || strcmp(comm[0], "t") == 0 
	|| strcmp(comm[0], "trash") == 0) {
		if (comm[1] && strcmp(comm[1], "--help") == 0) {
			puts(_("Usage: t, tr, trash [ELN/FILE ... n] "
				 "[ls, list] [clear] [del, rm]"));
			return EXIT_SUCCESS;
		}

		exit_code = trash_function(comm);

		if (is_sel) { /* If 'tr sel', deselect everything */
			size_t i;

			for (i = 0; i < sel_n; i++)
				free(sel_elements[i]);

			sel_n = 0;

			if (save_sel() != 0)
				exit_code = EXIT_FAILURE;
		}
	}

	else if (strcmp(comm[0], "undel") == 0 || strcmp(comm[0], "u") == 0
	|| strcmp(comm[0], "untrash") == 0) {
		if (comm[1] && strcmp(comm[1], "--help") == 0) {
			puts(_("Usage: u, undel, untrash [*, a, all]"));
			return EXIT_SUCCESS;
		}

		kbind_busy = 1;
		rl_attempted_completion_function = NULL;
		exit_code = untrash_function(comm);
		rl_attempted_completion_function = my_rl_completion;
		kbind_busy = 0;
	}

	/*         ############### SELECTION ##################     */
	else if (strcmp(comm[0], "s") == 0 || strcmp(comm[0], "sel") == 0) {
		if (!comm[1]) {
			puts(_("Usage: s, sel ELN ELN-ELN FILE ... n"));
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}
		else if (strcmp(comm[1], "--help") == 0) {
			puts(_("Usage: s, sel ELN ELN-ELN FILE ... n"));
			return EXIT_SUCCESS;
		}

		exit_code = sel_function(comm);
	}

	else if (strcmp(comm[0], "sb") == 0
	|| strcmp(comm[0], "selbox") == 0)
		show_sel_files();

	else if (strcmp(comm[0], "ds") == 0
	|| strcmp(comm[0], "desel") == 0) {
		if (comm[1] && strcmp(comm[1], "--help") == 0) {
			puts(_("Usage: desel, ds [*, a, all]"));
			return EXIT_SUCCESS;
		}

		kbind_busy = 1;
		rl_attempted_completion_function = NULL;
		exit_code = deselect(comm);
		rl_attempted_completion_function = my_rl_completion;
		kbind_busy = 0;
	}

	/*  ############### SOME SHELL CMD WRAPPERS ##################  */
	else if (strcmp(comm[0], "rm") == 0 || strcmp(comm[0], "mkdir") == 0 
	|| strcmp(comm[0], "touch") == 0 || strcmp(comm[0], "ln") == 0
	|| strcmp(comm[0], "chmod") == 0 || strcmp(comm[0], "unlink") == 0
	|| strcmp(comm[0], "r") == 0 || strcmp(comm[0], "l") == 0
	|| strcmp(comm[0], "md") == 0) {

		if (strcmp(comm[0], "l") == 0) {
			comm[0] = (char *)xrealloc(comm[0], 3 * sizeof(char *));
			strncpy(comm[0], "ln", 3);
		}
		else if (strcmp(comm[0], "r") == 0) {
			comm[0] = (char *)xrealloc(comm[0], 3 * sizeof(char *));
			strncpy(comm[0], "rm", 3);
		}
		else if (strcmp(comm[0], "md") == 0) {
			comm[0] = (char *)xrealloc(comm[0], 6 * sizeof(char *));
			strncpy(comm[0], "mkdir", 6);
		}

		kbind_busy = 1;
		exit_code = run_and_refresh(comm);
		kbind_busy = 0;
	}

	/*    ############### PROPERTIES ##################     */
	else if (strcmp(comm[0], "pr") == 0 || strcmp(comm[0], "prop") == 0 
	|| strcmp(comm[0], "p") == 0) {
		if (!comm[1]) {
			fputs(_("Usage: pr [ELN/FILE ... n] [a, all] [s, size]\n"),
				  stderr);
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}
		else if (strcmp(comm[1], "--help") == 0) {
			puts(_("Usage: pr [ELN/FILE ... n] [a, all] [s, size]"));
			return EXIT_SUCCESS;
		}
		exit_code = properties_function(comm);
	}

	/*     ############### SEARCH ##################     */	
	else if (comm[0][0] == '/' && access(comm[0], F_OK) != 0)
								/* If not absolute path */ 
		exit_code = search_function(comm);

	/*      ############### HISTORY ##################     */
	/* If '!number' or '!-number' or '!!' */
	else if (comm[0][0] == '!' && (isdigit(comm[0][1]) 
	|| (comm[0][1] == '-' && isdigit(comm[0][2])) || comm[0][1] == '!'))
		exit_code = run_history_cmd(comm[0] + 1);

	/*    ############### BULK RENAME ##################     */
	else if (strcmp(comm[0], "br") == 0
	|| strcmp(comm[0], "bulk") == 0) {
		if (!comm[1]) {
			fputs(_("Usage: br, bulk ELN/FILE ...\n"), stderr);
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}
		if (strcmp(comm[1], "--help") == 0) {
			puts(_("Usage: br, bulk ELN/FILE ...\n"));
			return EXIT_SUCCESS;
		}
		exit_code = bulk_rename(comm);
	}

	/*      ################ SORT ##################     */
	else if (strcmp(comm[0], "st") == 0
	|| strcmp(comm[0], "sort") == 0) {
		if (comm[1] && strcmp(comm[1], "--help") == 0) {
			puts(_("Usage: st [METHOD] [rev]\nMETHOD: 0 = none, "
				   "1 = name, 2 = size, 3 = atime, 4 = btime, "
				   "5 = ctime, 6 = mtime, 7 = version"));
		    return EXIT_SUCCESS;
		}
		exit_code = sort_function(comm);
	}

	/*   ################ ARCHIVER ##################     */
	else if (strcmp(comm[0], "ac") == 0
	|| strcmp(comm[0], "ad") == 0) {
		if (!comm[1] || strcmp(comm[1], "--help") == 0) {
			puts(_("Usage: ac, ad ELN/FILE ..."));
			return EXIT_SUCCESS;
		}

		if (comm[0][1] == 'c')
			exit_code = archiver(comm, 'c');
		else
			exit_code = archiver(comm, 'd');

		return exit_code;
	}

	/* ##################################################
	 * #			     MINOR FUNCTIONS 				#
	 * ##################################################*/

	else if (strcmp(comm[0], "opener") == 0) {
		if (!comm[1]) {
			printf("opener: %s\n", (opener) ? opener : "mime (built-in)");
			return EXIT_SUCCESS;
		}
		if (comm[1] && strcmp(comm[1], "--help") == 0) {
			puts(_("Usage: opener APPLICATION\n"));
			return EXIT_SUCCESS;
		}
		if (opener) {
			free(opener);
			opener = (char *)NULL;
		}
		if (strcmp(comm[1], "default") != 0) {
			opener = (char *)xcalloc(strlen(comm[1]) + 1, sizeof(char));
			strcpy(opener, comm[1]);
		}
		printf(_("opener: Opener set to '%s'\n"), (opener) ? opener
			   : "mime (built-in)");
	}

					/* #### TIPS #### */
	else if (strcmp(comm[0], "tips") == 0)
		print_tips(1);

					/* #### LIGHT MODE #### */
	else if (strcmp(comm[0], "lm") == 0) {
		if (comm[1]) {
			if (strcmp(comm[1], "on") == 0) {
				light_mode = 1;
				puts(_("Light mode is on"));
			}
			else if (strcmp(comm[1], "off") == 0) {
				light_mode = 0;
				puts(_("Light mode is off"));
			}
			else {
				puts(_("Usage: lm [on, off]"));
				exit_code = EXIT_FAILURE;
			}
		}
		else {
			fputs(_("Usage: lm [on, off]\n"), stderr);
			exit_code = EXIT_FAILURE;
		}
	}

					/* #### NEW INSTANCE #### */
	else if (strcmp(comm[0], "x") == 0) {
		if (comm[1])
			exit_code = new_instance(comm[1]);
		else {
			fputs("Usage: x DIR\n", stderr);
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}
	}

						/* #### NET #### */
	else if (strcmp(comm[0], "n") == 0 || strcmp(comm[0], "net") == 0) {
		if (!comm[1]) {
			puts(_("Usage: n, net [sftp, smb, ftp]://ADDRESS [OPTIONS]"));
			return EXIT_SUCCESS;
		}
		if (strncmp(comm[1], "sftp://", 7) == 0)
			exit_code = remote_ssh(comm[1] + 7, (comm[2]) ? comm[2]
								   : NULL);
		else if (strncmp(comm[1], "smb://", 6) == 0)
			exit_code = remote_smb(comm[1] + 6, (comm[2]) ? comm[2]
								   : NULL);
		else if (strncmp(comm[1], "ftp://", 6) == 0)
			exit_code = remote_ftp(comm[1] + 6, (comm[2]) ? comm[2]
								   : NULL);
		else {
			fputs(_("Usage: n, net [sftp, smb, ftp]://ADDRESS [OPTIONS]\n"),
				  stderr);
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}
	}

						/* #### MIME #### */
	else if (strcmp(comm[0], "mm") == 0 || strcmp(comm[0], "mime") == 0) {
		exit_code = mime_open(comm);
	}

	else if (strcmp(comm[0], "ls") == 0 && !cd_lists_on_the_fly) {
		while (files--)
			free(dirlist[files]);
		exit_code = list_dir();

		if (get_sel_files() != 0)
			exit_code = EXIT_FAILURE;
	}

					/* #### PROFILE #### */
	else if (strcmp(comm[0], "pf") == 0 || strcmp(comm[0], "prof") == 0 
	|| strcmp(comm[0], "profile") == 0)
		exit_code = profile_function(comm);

					/* #### MOUNTPOINTS #### */
	else if (strcmp(comm[0], "mp") == 0 
	|| (strcmp(comm[0], "mountpoints") == 0)) {
		if (comm[1] && strcmp(comm[1], "--help") == 0)
			puts(_("Usage: mountpoints, mp"));
		else {
			kbind_busy = 1;
			rl_attempted_completion_function = NULL;
			exit_code = list_mountpoints();
			rl_attempted_completion_function = my_rl_completion;
			kbind_busy = 0;
		}
	}

					/* #### EXT #### */
	else if (strcmp(comm[0], "ext") == 0) {
		if (!comm[1]) {
			puts(_("Usage: ext [on, off, status]"));
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}
		else if (strcmp(comm[1], "--help") == 0) 
			puts(_("Usage: ext [on, off, status]"));

		else {
			if (strcmp(comm[1], "status") == 0)
				printf(_("%s: External commands %s\n"), PROGRAM_NAME, 
					    (ext_cmd_ok) ? _("enabled") : _("disabled"));
			else if (strcmp(comm[1], "on") == 0) {
				ext_cmd_ok = 1;
				printf(_("%s: External commands enabled\n"),
					   PROGRAM_NAME);
			}
			else if (strcmp(comm[1], "off") == 0) {
				ext_cmd_ok = 0;
				printf(_("%s: External commands disabled\n"),
					   PROGRAM_NAME);
			}
			else {
				fputs(_("Usage: ext [on, off, status]\n"), stderr);
				exit_code = EXIT_FAILURE;
			}
		}
	}

					/* #### PAGER #### */
	else if (strcmp(comm[0], "pg") == 0
	|| strcmp(comm[0], "pager") == 0) {
		if (!comm[1]) {
			puts(_("Usage: pager, pg [on, off, status]"));
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}
		else if (strcmp(comm[1], "--help") == 0) 
			puts(_("Usage: pager, pg [on, off, status]"));

		else {
			if (strcmp(comm[1], "status") == 0)
				printf(_("%s: Pager %s\n"), PROGRAM_NAME, 
					   (pager) ? _("enabled") : _("disabled"));
			else if (strcmp(comm[1], "on") == 0) {
				pager = 1;
				printf(_("%s: Pager enabled\n"), PROGRAM_NAME);
			}
			else if (strcmp(comm[1], "off") == 0) {
				pager = 0;
				printf(_("%s: Pager disabled\n"), PROGRAM_NAME);
			}
			else {
				fputs(_("Usage: pager, pg [on, off, status]\n"),
					  stderr);
				exit_code = EXIT_FAILURE;
			}
		}
	}

					/* #### DIR COUNTER #### */
	else if (strcmp(comm[0], "fc") == 0
	|| strcmp(comm[0], "filescounter") == 0) {
		if (!comm[1]) {
			fputs(_("Usage: fc, filescounter [on, off, status]"), stderr);
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}

		if (strcmp(comm[1], "on") == 0) {
			files_counter = 1;
			puts(_("Filescounter is enabled"));
			return EXIT_SUCCESS;
		}

		if (strcmp(comm[1], "off") == 0) {
			files_counter = 0;
			puts(_("Filescounter is disabled"));
			return EXIT_SUCCESS;
		}

		if (strcmp(comm[1], "status") == 0) {
			if (files_counter)
				puts(_("Filescounter is enabled"));
			else
				puts(_("Filescounter is disabled"));
			return EXIT_SUCCESS;
		}
		else {
			fputs(_("Usage: fc, filescounter [on, off, status]\n"), stderr);
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}
	}

				    /* #### UNICODE #### */
	else if (strcmp(comm[0], "uc") == 0
	|| strcmp(comm[0], "unicode") == 0) {
		if (!comm[1]) {
			fputs(_("Usage: unicode, uc [on, off, status]"), stderr);
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}
		else if (strcmp(comm[1], "--help") == 0)
			puts(_("Usage: unicode, uc [on, off, status]"));

		else {
			if (strcmp(comm[1], "status") == 0)
				printf(_("%s: Unicode %s\n"), PROGRAM_NAME, 
					    (unicode) ? _("enabled") : _("disabled"));
			else if (strcmp(comm[1], "on") == 0) {
				unicode = 1;
				printf(_("%s: Unicode enabled\n"), PROGRAM_NAME);
			}
			else if (strcmp(comm[1], "off") == 0) {
				unicode = 0;
				printf(_("%s: Unicode disabled\n"), PROGRAM_NAME);
			}
			else {
				fputs(_("Usage: unicode, uc [on, off, status]\n"),
					  stderr);
				exit_code = EXIT_FAILURE;
			}
		}
	}

				/* #### FOLDERS FIRST #### */
	else if ((strcmp(comm[0], "folders") == 0 
	&& strcmp(comm[1], "first") == 0) || strcmp(comm[0], "ff") == 0) {
		if (cd_lists_on_the_fly == 0) 
			return EXIT_SUCCESS;
		int n = 0;
		if (strcmp(comm[0], "ff") == 0)
			n = 1;
		else
			n = 2;
		if (comm[n]) {
			if (strcmp(comm[n], "--help") == 0) {
				puts(_("Usage: folders first, ff [on, off, status]"));
				return EXIT_SUCCESS;
			}
			int status = list_folders_first;
			if (strcmp(comm[n], "status") == 0)
				printf(_("%s: Folders first %s\n"), PROGRAM_NAME, 
					     (list_folders_first) ? _("enabled")
					     : _("disabled"));
			else if (strcmp(comm[n], "on") == 0)
				list_folders_first = 1;
			else if (strcmp(comm[n], "off") == 0)
				list_folders_first = 0;
			else {
				fputs(_("Usage: folders first, ff [on, off, status]\n"), 
					    stderr);
				exit_code = EXIT_FAILURE;
				return EXIT_FAILURE;
			}

			if (list_folders_first != status) {
				if (cd_lists_on_the_fly) {
					while (files--)
						free(dirlist[files]);
					exit_code = list_dir();
				}
			}
		}
		else {
			fputs(_("Usage: folders first, ff [on, off, status]\n"),
					stderr);
			exit_code = EXIT_FAILURE;
		}
	}

				/* #### LOG #### */
	else if (strcmp(comm[0], "log") == 0) {
		if (comm[1] && strcmp(comm[1], "--help") == 0) {
			puts(_("Usage: log [clear]"));
			return EXIT_SUCCESS;
		}

		/* I make this check here, and not in the function itself,
		 * because this function is also called by other instances of
		 * the program where no message should be printed */
		if (!config_ok) {
			fprintf(stderr, _("%s: Log function disabled\n"),
							  PROGRAM_NAME);
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}

		exit_code = log_function(comm);
	}

				/* #### MESSAGES #### */
	else if (strcmp(comm[0], "msg") == 0
	|| strcmp(comm[0], "messages") == 0) {
		if (comm[1] && strcmp(comm[1], "--help") == 0) {
			puts(_("Usage: messages, msg [clear]"));
			return EXIT_SUCCESS;
		}
		if (comm[1] && strcmp(comm[1], "clear") == 0) {
			if (!msgs_n) {
				printf(_("%s: There are no messages\n"), PROGRAM_NAME);
				return EXIT_SUCCESS;
			}
			size_t i;
			for (i = 0; i < (size_t)msgs_n; i++)
				free(messages[i]);
			msgs_n = 0;
			pmsg = nomsg;
		}
		else {
			if (msgs_n) {
				size_t i;
				for (i = 0; i < (size_t)msgs_n; i++)
					printf("%s", messages[i]);
			}
			else
				printf(_("%s: There are no messages\n"), PROGRAM_NAME);
		}
	}

				/* #### ALIASES #### */
	else if (strcmp(comm[0], "alias") == 0) {
		if (comm[1]) {
			if (strcmp(comm[1], "--help") == 0) {
				puts(_("Usage: alias [import FILE]"));
				return EXIT_SUCCESS;
			}
			else if (strcmp(comm[1], "import") == 0) {
				if (!comm[2]) {
					fprintf(stderr, _("Usage: alias import FILE\n"));
					exit_code = EXIT_FAILURE;
					return EXIT_FAILURE;
				}
				exit_code = alias_import(comm[2]);
				return exit_code;
			}
		}

		if (aliases_n) {
			size_t i;
			for (i = 0; i < aliases_n; i++)
				printf("%s", aliases[i]);
		}
	}

				/* #### SHELL #### */
	else if (strcmp(comm[0], "shell") == 0) {
		if (!comm[1]) {
			if (sys_shell)
				printf("%s: shell: %s\n", PROGRAM_NAME, sys_shell);
			else
				printf("%s: shell: unknown\n", PROGRAM_NAME);
		}
		else if (strcmp(comm[1], "--help") == 0)
			puts(_("Usage: shell [SHELL]"));
		else
			exit_code = set_shell(comm[1]);
	}

				/* #### EDIT #### */
	else if (strcmp(comm[0], "edit") == 0)
		exit_code = edit_function(comm);

				/* #### HISTORY #### */
	else if (strcmp(comm[0], "history") == 0)
		exit_code = history_function(comm);

			  /* #### HIDDEN FILES #### */
	else if (strcmp(comm[0], "hf") == 0 
	|| strcmp(comm[0], "hidden") == 0) {
		if (!comm[1]) {
			fputs(_("Usage: hidden, hf [on, off, status]\n"), stderr); 
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}
		else if (strcmp(comm[1], "--help") == 0) {
			/* The same message is in hidden_function(), and printed
			 * whenever an invalid argument is entered */
			puts(_("Usage: hidden, hf [on, off, status]")); 
			return EXIT_SUCCESS;
		}
		else
			exit_code = hidden_function(comm);
	}

					/* #### AUTOCD #### */
	else if (strcmp(comm[0], "acd") == 0
	|| strcmp(comm[0], "autocd") == 0) {

		if (!comm[1]) {
			fputs(_("Usage: acd, autocd [on, off, status]\n"), stderr);
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}

		if (strcmp(comm[1], "on") == 0) {
			autocd = 1;
			printf(_("%s: autocd is enabled\n"), PROGRAM_NAME);
		}
		else if (strcmp(comm[1], "off") == 0) {
			autocd = 0;
			printf(_("%s: autocd is disabled\n"), PROGRAM_NAME);
		}
		else if (strcmp(comm[1], "status") == 0) {
			if (autocd)
				printf(_("%s: autocd is enabled\n"), PROGRAM_NAME);
			else
				printf(_("%s: autocd is disabled\n"), PROGRAM_NAME);
		}
		else if (strcmp(comm[1], "--help") == 0)
			puts(_("Usage: acd, autocd [on, off, status]"));
		else {
			fputs(_("Usage: acd, autocd [on, off, status]\n"), stderr);
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}
	}

					/* #### AUTOOPEN #### */
	else if (strcmp(comm[0], "ao") == 0
	|| strcmp(comm[0], "auto-open") == 0) {

		if (!comm[1]) {
			fputs(_("Usage: ao, auto-open [on, off, status]\n"), stderr);
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}

		if (strcmp(comm[1], "on") == 0) {
			auto_open = 1;
			printf(_("%s: auto-open is enabled\n"), PROGRAM_NAME);
		}
		else if (strcmp(comm[1], "off") == 0) {
			auto_open = 0;
			printf(_("%s: auto-open is disabled\n"), PROGRAM_NAME);
		}
		else if (strcmp(comm[1], "status") == 0) {
			if (auto_open)
				printf(_("%s: auto-open is enabled\n"), PROGRAM_NAME);
			else
				printf(_("%s: auto-open is disabled\n"), PROGRAM_NAME);
		}
		else if (strcmp(comm[1], "--help") == 0)
			puts(_("Usage: ao, auto-open [on, off, status]"));
		else {
			fputs(_("Usage: ao, auto-open [on, off, status]\n"), stderr);
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}
	}

					/* #### COMMANDS #### */
	else if (strcmp(comm[0], "cmd") == 0
	|| strcmp(comm[0], "commands") == 0) 
		exit_code = list_commands();

			  /* #### AND THESE ONES TOO #### */
	/* These functions just print stuff, so that the value of exit_code
	 * is always zero, that is to say, success */
	else if (strcmp(comm[0], "path") == 0 || strcmp(comm[0], "cwd") == 0) 
		printf("%s\n", path);

	else if (strcmp(comm[0], "help") == 0 || strcmp(comm[0], "?") == 0)
		help_function();

	else if (strcmp(comm[0], "colors") == 0
	|| strcmp(comm[0], "cc") == 0)
		color_codes();

	else if (strcmp(comm[0], "ver") == 0
	|| strcmp(comm[0], "version") == 0)
		version_function();

	else if (strcmp(comm[0], "fs") == 0)
		free_software();

	else if (strcmp(comm[0], "bonus") == 0)
		bonus_function();

	else if (strcmp(comm[0], "splash") == 0)
		splash();

					/* #### QUIT #### */
	else if (strcmp(comm[0], "q") == 0 || strcmp(comm[0], "quit") == 0 
	|| strcmp(comm[0], "exit") == 0 || strcmp(comm[0], "zz") == 0 
	|| strcmp(comm[0], "salir") == 0 || strcmp(comm[0], "chau") == 0) {

		/* Free everything and exit */
		size_t i;
		for (i = 0; i <= args_n; i++)
			free(comm[i]);
		free(comm);

		exit(exit_code);
	}


	else {

				/* ###############################
				 * #	 AUTOCD & AUTOOPEN (2)   #
				 * ############################### */

		struct stat file_attrib;
		if (stat(comm[0], &file_attrib) == 0) {
			if ((file_attrib.st_mode & S_IFMT) == S_IFDIR) {

				if (autocd) {
					exit_code = cd_function(comm[0]);
					return exit_code;
				}

				fprintf(stderr, _("%s: '%s': Is a directory\n"), 
						PROGRAM_NAME, comm[0]);
				exit_code = EXIT_FAILURE;
				return EXIT_FAILURE;
			}

			else if (auto_open && (file_attrib.st_mode & S_IFMT)
			== S_IFREG) {
				/* Make sure we have not an executable file */
				if (!(file_attrib.st_mode
				& (S_IXUSR|S_IXGRP|S_IXOTH))) {

					char *cmd[] = { "open", comm[0], (comm[1])
									? comm[1] : NULL, NULL };
					exit_code = open_function(cmd);
					return exit_code;
				}
			}
		}

	/* ####################################################
	 * #				EXTERNAL/SHELL COMMANDS			  #
	 * ####################################################*/


		/* LOG EXTERNAL COMMANDS
		* 'no_log' will be true when running profile or prompt commands */
		if (!no_log)
			exit_code = log_function(comm);

		/* PREVENT UNGRACEFUL EXIT */
		/* Prevent the user from killing the program via the 'kill',
		 * 'pkill' or 'killall' commands, from within CliFM itself.
		 * Otherwise, the program will be forcefully terminated without
		 * freeing allocated memory */
		if (strcmp(comm[0], "kill") == 0 
		|| strcmp(comm[0], "killall") == 0 
		|| strcmp(comm[0], "pkill") == 0) {
			size_t i;
			for (i = 1; i <= args_n; i++) {
				if ((strcmp(comm[0], "kill") == 0 
				&& atoi(comm[i]) == (int)own_pid) 
				|| ((strcmp(comm[0], "killall") == 0 
				|| strcmp(comm[0], "pkill") == 0) 
				&& strcmp(comm[i], argv_bk[0]) == 0)) {
					fprintf(stderr, _("%s: To gracefully quit enter"
									  "'quit'\n"), PROGRAM_NAME);
					exit_code = EXIT_FAILURE;
					return EXIT_FAILURE;
				}
			}
		}

		/* CHECK WHETHER EXTERNAL COMMANDS ARE ALLOWED */
		if (!ext_cmd_ok) {
			fprintf(stderr, _("%s: External commands are not allowed. "
					"Run 'ext on' to enable them.\n"), PROGRAM_NAME);
			exit_code = EXIT_FAILURE;
			return EXIT_FAILURE;
		}

		/*
		 * By making precede the command by a colon or a semicolon, the
		 * user can BYPASS CliFM parsing, expansions, and checks to be
		 * executed DIRECTLY by the system shell (execle). For example:
		 * if the amount of files listed on the screen (ELN's) is larger
		 * or equal than 644 and the user tries to issue this command:
		 * "chmod 644 filename", CLIFM will take 644 to be an ELN, and
		 * will thereby try to expand it into the corresponding filename,
		 * which is not what the user wants. To prevent this, simply run
		 * the command as follows: ";chmod 644 filename" */

		if (comm[0][0] == ':' || comm[0][0] == ';') {
			/* Remove the colon from the beginning of the first argument,
			 * that is, move the pointer to the next (second) position */
			char *comm_tmp = comm[0] + 1;
			/* If string == ":" or ";" */
			if (!comm_tmp || comm_tmp[0] == 0x00) {
				fprintf(stderr, _("%s: '%c': Syntax error\n"),
						PROGRAM_NAME, comm[0][0]);
				exit_code = EXIT_FAILURE;
				return EXIT_FAILURE;
			}
			else
				strcpy(comm[0], comm_tmp);
		}

		/* #### RUN THE EXTERNAL COMMAND #### */

		/* Store the command and each argument into a single array to be 
		 * executed by execle() using the system shell (/bin/sh -c) */
		char *ext_cmd = (char *)NULL;
		size_t ext_cmd_len = strlen(comm[0]);
		ext_cmd = (char *)xcalloc(ext_cmd_len + 1, sizeof(char));
		strcpy(ext_cmd, comm[0]);

		register size_t i;
		if (args_n) { /* This will be false in case of ";cmd" or ":cmd" */
			for (i = 1; i <= args_n; i++) {
				ext_cmd_len += strlen(comm[i]) + 1;
				ext_cmd = (char *)xrealloc(ext_cmd, (ext_cmd_len + 1)
										   * sizeof(char));
				strcat(ext_cmd, " ");
				strcat(ext_cmd, comm[i]);
			}
		}

		/* Since we modified LS_COLORS, store its current value and unset
		 * it. Some shell commands use LS_COLORS to display their outputs 
		 * ("ls -l", for example, use the "no" value to print file
		 * properties). So, we unset it to prevent wrong color output
		 * for external commands. The disadvantage of this procedure is
		 * that if the user uses a customized LS_COLORS, unsetting it
		 * set its value to default, and the customization is lost. */

		#if __FreeBSD__
		char *my_ls_colors = (char *)NULL, *p = (char *)NULL;
		/* For some reason, when running on FreeBSD Valgrind complains
		 * about overlapping source and destiny in setenv() if I just
		 * copy the address returned by getenv() instead of the string
		 * itself. Not sure why, but this makes the error go away */
		p = getenv("LS_COLORS");
		my_ls_colors = (char *)xcalloc(strlen(p) + 1, sizeof(char *));
		strcpy(my_ls_colors, p);
		p = (char *)NULL;

		#else
		static char *my_ls_colors = (char *)NULL;
		my_ls_colors = getenv("LS_COLORS");
		#endif

		if (ls_colors_bk && *ls_colors_bk != 0x00)
			setenv("LS_COLORS", ls_colors_bk, 1);
		else
			unsetenv("LS_COLORS");

		if (launch_execle(ext_cmd) != EXIT_SUCCESS)
			exit_code = EXIT_FAILURE;
		free(ext_cmd);

		/* Restore LS_COLORS value to use CliFM colors */
		setenv("LS_COLORS", my_ls_colors, 1);

		#if __FreeBSD__
		free(my_ls_colors);
		#endif

 		/* Reload the list of available commands in PATH for TAB completion.
 		 * Why? If this list is not updated, whenever some new program is 
 		 * installed, renamed, or removed from some of the pathsin PATH
 		 * while in CliFM, this latter needs to be restarted in order 
 		 * to be able to recognize the new program for TAB completion */

		if (bin_commands) {
			for (i = 0; bin_commands[i]; i++)
				free(bin_commands[i]);
			free(bin_commands);
			bin_commands = (char  **)NULL;
		}

		if (paths)
			for (i = 0; i < path_n; i++)
				free(paths[i]);

		path_n = (size_t)get_path_env();
		get_path_programs();
	}

	return exit_code;
}

void
surf_hist(char **comm)
{
	if (strcmp(comm[1], "h") == 0 || strcmp(comm[1], "hist") == 0) {
		/* Show the list of already visited directories */
		int i;
		for (i = 0; i < dirhist_total_index; i++) {
			if (i == dirhist_cur_index)
				printf("%d %s%s%s%s\n", i + 1, green, old_pwd[i], NC, 
					   default_color);
			else
				printf("%d %s\n", i + 1, old_pwd[i]);
		}
	}
	else if (strcmp(comm[1], "clear") == 0) {
		int i;
		for (i = 0; i < dirhist_total_index; i++)
			free(old_pwd[i]);
		dirhist_cur_index = dirhist_total_index = 0;
		add_to_dirhist(path);
	}
	else if (comm[1][0] == '!' && is_number (comm[1] + 1) == 1) {
		/* Go the the specified directory (first arg) in the directory
		 * history list */
		int atoi_comm = atoi(comm[1] + 1);
		if (atoi_comm > 0 && atoi_comm <= dirhist_total_index) {
			int ret = chdir(old_pwd[atoi_comm - 1]);
			if (ret == 0) {
				free(path);
				path = (char *)xcalloc(strlen(old_pwd[atoi_comm - 1])
									   + 1, sizeof(char));
				strcpy(path, old_pwd[atoi_comm - 1]);
/*				add_to_dirhist(path); */
				dirhist_cur_index = atoi_comm - 1;
				if (cd_lists_on_the_fly) {
					while (files--) free(dirlist[files]);
						list_dir();
				}
			}
			else
				fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME,
						old_pwd[atoi_comm - 1], strerror(errno));
		}
		else 
			fprintf(stderr, _("history: '%d': No such ELN\n"), 
					atoi(comm[1] + 1));
	}
	else 
		fputs(_("history: Usage: b/f [hist] [clear] [!ELN]\n"), stderr);

	return;
}

int
launch_execle(const char *cmd)
/* Execute a command using the system shell (/bin/sh), which takes care
 * of special functions such as pipes and stream redirection, and special
 * chars like wildcards, quotes, and escape sequences. Use only when the
 * shell is needed; otherwise, launch_execve() should be used instead. */
{
	/* Error codes
	#define EXNULLERR 79
	#define EXEXECERR 80
	#define EXFORKERR 81
	#define EXCRASHERR 82
	#define EXWAITERR 83
	*/
	if (!cmd)
		return EXNULLERR;

	/* Reenable SIGCHLD, in case it was disabled. Otherwise, waitpid won't
	 * be able to catch error codes coming from the child */
	signal (SIGCHLD, SIG_DFL);

	int status;
	/* Create a new process via fork() */
	pid_t pid = fork();
	if (pid < 0) {
		fprintf(stderr, "%s: fork: %s\n", PROGRAM_NAME, strerror(errno));
		return EXFORKERR;
	}
	/* Run the command via execle */
	else if (pid == 0) {
		/* Reenable signals only for the child, in case they were
		 * disabled for the parent */
		signal (SIGHUP, SIG_DFL);
		signal (SIGINT, SIG_DFL);
		signal (SIGQUIT, SIG_DFL);
		signal (SIGTERM, SIG_DFL);

		/* Get shell base name */
		char *name = strrchr(sys_shell, '/');

		/* This is basically what the system() function does: running
		 * a command via the system shell */
		execl(sys_shell, name ? name + 1 : sys_shell, "-c", cmd, NULL);
		fprintf(stderr, "%s: '%s': execle: %s\n", PROGRAM_NAME, sys_shell, 
				strerror(errno));
		exit(errno);
	}
	/* Get command status */
	else if (pid > 0) {
		/* The parent process calls waitpid() on the child */
		if (waitpid(pid, &status, 0) > 0) {
			if (WIFEXITED(status) && !WEXITSTATUS(status)) {
				/* The program terminated normally and executed
				 * successfully */
				return EXIT_SUCCESS;
			}
			else if (WIFEXITED(status) && WEXITSTATUS(status)) {
				/* Either "command not found" (WEXITSTATUS(status) == 127), 
				 * "permission denied" (not executable) (WEXITSTATUS(status) == 
				 * 126) or the program terminated normally, but returned a 
				 * non-zero status. These exit codes will be handled by the 
				 * system shell itself, since we're using here execle() */
				return WEXITSTATUS(status);
			}
			else {
				/* The program didn't terminate normally */
				return EXCRASHERR;
			}
		}
		else {
			/* Waitpid() failed */
			fprintf(stderr, "%s: waitpid: %s\n", PROGRAM_NAME, 
					strerror(errno));
			return errno;
		}
	}

	/* Never reached */
	return EXIT_FAILURE;
}

int
launch_execve(char **cmd, int bg)
/* Execute a command and return the corresponding exit status. The exit
 * status could be: zero, if everything went fine, or a non-zero value
 * in case of error. The function takes as first arguement an array of
 * strings containing the command name to be executed and its arguments
 * (cmd), and an integer (bg) specifying if the command should be
 * backgrounded (1) or not (0) */
{
	/* Error codes
	#define EXNULLERR 79
	#define EXEXECERR 80
	#define EXFORKERR 81
	#define EXCRASHERR 82
	#define EXWAITERR 83 
	*/
	/* Standard exit codes in UNIX/Linux go from 0 to 255, where 0 means 
	 * success and a non-zero value means error. This is why I should use 
	 * negative values (or > 255) so that my own exit codes won't be
	 * confused with some standard exit code. However, the Linux
	 * Documentation Project (tldp) recommends the use of codes 64-113
	 * for user-defined exit codes. See:
	 * http://www.tldp.org/LDP/abs/html/exitcodes.html
	 * and
	 * https://www.gnu.org/software/libc/manual/html_node/Exit-Status.html 
	*/

	if (!cmd)
		return EXNULLERR;

	/* Reenable SIGCHLD, in case it was disabled. Otherwise, waitpid
	 * won't be able to catch error codes coming from the child. */
	signal(SIGCHLD, SIG_DFL);

	/* Create a new process via fork() */
	pid_t pid = fork();
	if (pid < 0) {
		fprintf(stderr, "%s: fork: %s\n", PROGRAM_NAME, strerror(errno));
		return errno;
	}
	/* Run the command via execvp */
	else if (pid == 0) {
		if (!bg) {
			/* If the program runs in the foreground, reenable signals
			 * only for the child, in case they were disabled for the
			 * parent */
			signal(SIGHUP, SIG_DFL);
			signal(SIGINT, SIG_DFL);
			signal(SIGQUIT, SIG_DFL);
			signal(SIGTERM, SIG_DFL);
		}
		/* If backgrounded, detach it from the parent */
/*		else {
			pid_t sid = setsid();
			signal(SIGHUP, SIG_IGN);
		} */

		execvp(cmd[0], cmd);
		/* This will only be reached if execvp() fails, because the exec
		 * family of functions returns only on error */
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, cmd[0], 
				strerror(errno));
		exit(errno);
	}
	/* Get the command status */
	else { /* pid > 0 */
		if (bg) {
			run_in_background(pid);
			return EXIT_SUCCESS;
		}
		else
			return run_in_foreground(pid);
	}

	/* Never reached */
	return EXIT_FAILURE;
}

int
run_in_foreground (pid_t pid)
{
	int status = 0;

	/* The parent process calls waitpid() on the child */
	if (waitpid(pid, &status, 0) > 0) {
		if (WIFEXITED(status) && !WEXITSTATUS(status)) {
			/* The program terminated normally and executed successfully 
			 * (WEXITSTATUS(status) == 0) */
			return EXIT_SUCCESS;
		}
		else if (WIFEXITED(status) && WEXITSTATUS(status)) {
			/* Program terminated normally, but returned a 
			 * non-zero status. Error codes should be printed by the 
			 * program itself */
			return WEXITSTATUS(status);
		}
		else {
			/* The program didn't terminate normally. In this case too,
			 * error codes should be printed by the program */
			return EXCRASHERR;
		}
	}
	else {
		/* waitpid() failed */
		fprintf(stderr, "%s: waitpid: %s\n", PROGRAM_NAME,
				strerror(errno));
		return errno;
	}

	return EXIT_FAILURE; /* Never reached */
}

void
run_in_background(pid_t pid)
{
	int status = 0;
	/* Keep it in the background */
	waitpid(pid, &status, WNOHANG); /* or: kill(pid, SIGCONT); */
}

int
save_sel(void)
/* Save selected elements into a tmp file. Returns 1 if success and 0
 * if error. This function allows the user to work with multiple
 * instances of the program: he/she can select some files in the
 * first instance and then execute a second one to operate on those
 * files as he/she whises. */
{
	if (!selfile_ok)
		return EXIT_FAILURE;

	if (sel_n == 0) {
		if (remove(sel_file_user) == -1) {
			fprintf(stderr, "%s: sel: '%s': %s\n", PROGRAM_NAME, 
					sel_file_user, strerror(errno));
			return EXIT_FAILURE;
		}
		else
			return EXIT_SUCCESS;
	}

	FILE *sel_fp = fopen(sel_file_user, "w");

	if (!sel_fp) {
		_err(0, NOPRINT_PROMPT, "%s: sel: '%s': %s\n", PROGRAM_NAME, 
			 sel_file_user, strerror(errno));
		return EXIT_FAILURE;
	}

	size_t i;
	for (i = 0; i < sel_n; i++) {
		fputs(sel_elements[i], sel_fp);
		fputc('\n', sel_fp);
	}

	fclose(sel_fp);

	return EXIT_SUCCESS;
}

int
sel_function (char **comm)
{
	if (!comm)
		return EXIT_FAILURE;

	char *sel_tmp = (char *)NULL;
	size_t i = 0, j = 0;
	int exists = 0, new_sel = 0, exit_status = EXIT_SUCCESS;

	for (i = 1; i <= args_n; i++) {
		/* If string is "*" or ".*", the wildcards expansion function
		 * found no match, in which case the string must be skipped.
		 * Exclude self and parent directories (. and ..) as well */
		if (strcmp(comm[i], "*") == 0 || strcmp(comm[i], ".*") == 0
		|| strcmp(comm[i], ".") == 0 || strcmp(comm[i], "..") == 0)
			continue;

		char *deq_file = dequote_str(comm[i], 0);

		if (!deq_file) {
			for (j = 0; j < sel_n; j++)
				free(sel_elements[j]);
			return EXIT_FAILURE;
		}

		size_t file_len = strlen(deq_file);

		/* Remove final slash from directories. No need to check if file
		 * is a directory, since in Linux only directories can contain a
		 * slash in its name */
		if (deq_file[file_len - 1] == '/')
			deq_file[file_len - 1] = 0x00;

		if (strcmp(deq_file, ".") == 0 || strcmp(deq_file, "..") == 0) {
			free(deq_file);
			continue;
		}

		/* If a filename in CWD... */
		int sel_is_filename = 0, sel_is_relative_path = 0;
		for (j = 0; j < files; j++) {
			if (strcmp(dirlist[j], deq_file) == 0) {
				sel_is_filename = 1;
				break;
			}
		}

		if (!sel_is_filename) {

			/* If a path (contains a slash)... */
			if (strcntchr(deq_file, '/') != -1) {
				if (deq_file[0] != '/') /* If relative path */
					sel_is_relative_path = 1;
				struct stat file_attrib;
				if (stat(deq_file, &file_attrib) != 0) {
					fprintf(stderr, "%s: sel: '%s': %s\n", PROGRAM_NAME, 
							deq_file, strerror(errno));
					free(deq_file);
					/* Return error when at least one error occur */
					exit_status = EXIT_FAILURE;
					continue;
				}
			}

			/* If neither a filename in CWD nor a path... */
			else {
				fprintf(stderr, _("%s: sel: '%s': No such %s\n"), 
						PROGRAM_NAME, deq_file, 
						(is_number(deq_file)) ? "ELN"
						: "file or directory");
				free(deq_file);
				exit_status = EXIT_FAILURE;
				continue;
			}
		}

		if (sel_is_filename || sel_is_relative_path) { 
			/* Add path to filename or relative path */
			sel_tmp = (char *)xcalloc(strlen(path) + strlen(deq_file)
									  + 2, sizeof(char));
			sprintf(sel_tmp, "%s/%s", path, deq_file);
		}

		else { /* If absolute path... */
			sel_tmp = (char *)xcalloc(strlen(deq_file) + 1,
									  sizeof(char));
			strcpy(sel_tmp, deq_file);
		}

		free(deq_file);
		deq_file = (char *)NULL;

		/* Check if the selected element is already in the selection 
		 * box */
		exists = 0; 

		for (j = 0; j < sel_n; j++) {
			if (strcmp(sel_elements[j], sel_tmp) == 0) {
				exists = 1;
				break;
			}
		}

		if (!exists) {
			sel_elements = (char **)xrealloc(sel_elements, (sel_n + 1)
											 * sizeof(char *));
			sel_elements[sel_n] = (char *)xcalloc(strlen(sel_tmp) + 1,
												  sizeof(char));
			strcpy(sel_elements[sel_n++], sel_tmp);
			new_sel++;
		}

		else fprintf(stderr, _("%s: sel: '%s': Already selected\n"), 
					 PROGRAM_NAME, sel_tmp);
		free(sel_tmp);
		sel_tmp = (char *)NULL;
		continue;
	}

	if (!selfile_ok)
		return EXIT_FAILURE;

	if (!new_sel)
		return exit_status;

	/* At this point, we know there are new selected files and that the
	 * selection file is OK. So, write new selections into the selection 
	 * file */
	if (save_sel() == 0) { /* If selected files were successfully written
		to sel file */

		/* Get size of total sel files */
		struct stat sel_attrib;
		for (i = 0; i < sel_n; i++) {
			if (lstat(sel_elements[i], &sel_attrib) != -1) {
				if ((sel_attrib.st_mode & S_IFMT) == S_IFDIR)
					total_sel_size += dir_size(sel_elements[i]);
				else
					total_sel_size += sel_attrib.st_size;
			}
		}

		/* Print entries */
		if (sel_n > 10)
			printf(_("%zu elements are now in the Selection Box\n"),
					 sel_n);

		else if (sel_n > 0) {
			printf(_("%zu selected %s:\n\n"), sel_n, (sel_n == 1)
					? _("element") : _("elements"));

			for (i = 0; i < sel_n; i++)
				colors_list(sel_elements[i], (int)i + 1, 0, 1);
		}

		/* Print total size */
		char *human_size = get_size_unit(total_sel_size);

		if (sel_n > 10)
			printf("Total size: %s\n", human_size);

		else if (sel_n > 0)
			printf("\n%sTotal size%s: %s\n", white, NC, human_size);

		free(human_size);

		return exit_status;
	}

	else {
		if (sel_n > 0) { /* In case of error, remove sel files from
			memory */
			for (i = 0; i < sel_n; i++)
				free(sel_elements[i]);
			sel_n = 0;
		}
/*		save_sel(); */
		return EXIT_FAILURE;
	}
}

void
show_sel_files(void)
{
	if (clear_screen)
		CLEAR;

	printf(_("%sSelection Box%s%s\n"), white, NC, default_color);

	short reset_pager = 0;

	if (sel_n == 0)
		puts(_("Empty"));

	else {
		puts("");
		struct winsize w;
		ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
		int c;
		size_t counter = 0;
		unsigned short term_rows = w.ws_row;
		term_rows -= 2;
		size_t i;

		for(i = 0; i < sel_n; i++) {
/*			if (pager && counter > (term_rows-2)) { */

			if (pager && counter > (size_t)term_rows) {
				switch(c = xgetchar()) {
				/* Advance one line at a time */
				case 66: /* Down arrow */
				case 10: /* Enter */
				case 32: /* Space */
					break;
				/* Advance one page at a time */
				case 126: counter=0; /* Page Down */
					break;
				/* Stop paging (and set a flag to reenable the pager 
				 * later) */
				case 99: /* 'c' */
				case 112: /* 'p' */
				case 113: pager=0, reset_pager=1; /* 'q' */
					break;
				/* If another key is pressed, go back one position. 
				 * Otherwise, some filenames won't be listed.*/
				default: i--; continue;
					break;
				}
			}
			counter++;
			colors_list(sel_elements[i], (int)i + 1, 0, 1);
		}

		char *human_size = get_size_unit(total_sel_size);
		printf("\n%sTotal size%s: %s\n", white, NC, human_size);
		free(human_size);
	}

	if (reset_pager)
		pager = 1;
}

int
deselect(char **comm)
{
	if (!comm)
		return EXIT_FAILURE;

	if (comm[1] && (strcmp(comm[1], "*") == 0
	|| strcmp(comm[1], "a") == 0 || strcmp(comm[1], "all") == 0)) {
		if (sel_n > 0) {
			size_t i;
			for (i = 0; i < sel_n; i++)
				free(sel_elements[i]);
			sel_n = total_sel_size = 0;
			if (save_sel() != 0)
				return EXIT_FAILURE;
			else
				return EXIT_SUCCESS;
		}
		else {
			puts(_("desel: There are no selected files"));
			return EXIT_SUCCESS;
		}
	}

	register size_t i; 

	if (clear_screen)
		CLEAR;

	printf(_("%sSelection Box%s%s\n"), white, NC, default_color);

	if (sel_n == 0) {
		puts(_("Empty"));
		return EXIT_SUCCESS;
	}

	puts("");

	for (i = 0; i < sel_n; i++)
		colors_list(sel_elements[i], (int)i + 1, 0, 1);

	char *human_size = get_size_unit(total_sel_size);
	printf("\n%sTotal size%s: %s\n", white, NC, human_size);
	free(human_size);

	printf(_("\n%s%sEnter 'q' to quit.\n"), NC, default_color);
	size_t desel_n = 0;
	char *line = NULL, **desel_elements = (char **)NULL;

	while (!line)
		line = rl_no_hist(_("Element(s) to be deselected "
							"(ex: 1 2-6, or *): "));

	desel_elements = get_substr(line, ' ');
	free(line);

	if (!desel_elements)
		return EXIT_FAILURE;

	for (i = 0; desel_elements[i]; i++)
		desel_n++;

	for (i = 0; i < desel_n; i++) { /* Validation */

		/* If not a number */
		if (!is_number(desel_elements[i])) {

			if (strcmp(desel_elements[i], "q") == 0) {
				for (i = 0; i < desel_n; i++)
					free(desel_elements[i]);
				free(desel_elements);
				return EXIT_SUCCESS;
			}
			
			else if (strcmp(desel_elements[i], "*") == 0) {
				/* Clear the sel array */
				for (i = 0; i < sel_n; i++)
					free(sel_elements[i]);
				sel_n = total_sel_size = 0;
				for (i = 0; i < desel_n; i++)
					free(desel_elements[i]);
				int exit_status = EXIT_SUCCESS;
				if (save_sel() != 0)
					exit_status = EXIT_FAILURE;
				free(desel_elements);
				if (cd_lists_on_the_fly) {
					while (files--)
						free(dirlist[files]);
					if (list_dir() != 0)
						exit_status = EXIT_FAILURE;
				}
				return exit_status;
			}

			else {
				printf(_("desel: '%s': Invalid element\n"),
						desel_elements[i]);
				size_t j;
				for (j = 0; j < desel_n; j++)
					free(desel_elements[j]);
				free(desel_elements);
				return EXIT_FAILURE;
			}
		}

		/* If a number, check it's a valid ELN */
		else {
			int atoi_desel = atoi(desel_elements[i]);
			if (atoi_desel == 0 || (size_t)atoi_desel > sel_n) {
				printf(_("desel: '%s': Invalid ELN\n"),
						 desel_elements[i]);
				size_t j;
				for (j = 0; j < desel_n; j++)
					free(desel_elements[j]);
				free(desel_elements);
				return EXIT_FAILURE;
			}
		}
	}

	/* If a valid ELN and not asterisk... */
	/* Store the full path of all the elements to be deselected in a new
	 * array (desel_path). I need to do this because after the first
	 * rearragement of the sel array, that is, after the removal of the
	 * first element, the index of the next elements changed, and cannot
	 * thereby be found by their index. The only way to find them is to
	 * compare string by string */
	char **desel_path = (char **)NULL;
	desel_path = (char **)xnmalloc(desel_n, sizeof(char *));

	for (i = 0; i < desel_n; i++) {
		int desel_int = atoi(desel_elements[i]);
		desel_path[i] = (char *)xnmalloc(
								strlen(sel_elements[desel_int - 1]) 
								+ 1, sizeof(char));
		strcpy(desel_path[i], sel_elements[desel_int - 1]);
	}

	/* Search the sel array for the path of the element to deselect and
	 * store its index */
	struct stat desel_attrib;
	for (i = 0; i < desel_n; i++) {
		size_t j, k, desel_index = 0;
		for (k = 0; k < sel_n; k++) {
			if (strcmp(sel_elements[k], desel_path[i]) == 0) {

				/* Sustract size from total size */
				if (lstat(sel_elements[k], &desel_attrib) != -1) {
					if ((desel_attrib.st_mode & S_IFMT) == S_IFDIR)
						total_sel_size -= dir_size(sel_elements[k]);
					else
						total_sel_size -= desel_attrib.st_size;
				}

				desel_index = k;
				break;
			}
		}

		/* Once the index was found, rearrange the sel array removing the 
		 * deselected element (actually, moving each string after it to
		 * the previous position) */
		for (j = desel_index; j < (sel_n - 1); j++) {
			sel_elements[j] = (char *)xrealloc(sel_elements[j], 
										(strlen(sel_elements[j + 1]) + 1)
										* sizeof(char));
			strcpy(sel_elements[j], sel_elements[j + 1]);
		}
	}

	/* Free the last DESEL_N elements from the old sel array. They won't
	 * be used anymore, for they contain the same value as the last
	 * non-deselected element due to the above array rearrangement */
	for (i = 1; i <= desel_n; i++)
		if ((int)(sel_n - i) >= 0 && sel_elements[sel_n - i])
			free(sel_elements[sel_n - i]);

	/* Reallocate the sel array according to the new size */
	sel_n = (sel_n - desel_n);

	if ((int)sel_n < 0)
		sel_n = total_sel_size = 0;

	if (sel_n)
		sel_elements = (char **)xrealloc(sel_elements,
						sel_n * sizeof(char *));

	/* Deallocate local arrays */
	for (i = 0; i < desel_n; i++) {
		free(desel_path[i]);
		free(desel_elements[i]);
	}
	free(desel_path);
	free(desel_elements);

	if (args_n > 0) {
		for (i = 1; i <= args_n; i++)
			free(comm[i]);
		comm = (char **)xrealloc(comm, 1 * sizeof(char *));
		args_n = 0;
	}

	int exit_status = EXIT_SUCCESS;

	if (save_sel() != 0)
		exit_status = EXIT_FAILURE;

	/* If there is still some selected file, reload the desel screen */
	if (sel_n)
		if (deselect(comm) != 0)
			exit_status = EXIT_FAILURE;

	return exit_status;
}

int
run_and_refresh(char **comm)
/* Run a command via execle() and refresh the screen in case of success */
{
	if (!comm)
		return EXIT_FAILURE;

	log_function(comm);

	size_t i = 0, total_len = 0;
	for (i = 0; i <= args_n; i++)
		total_len += strlen(comm[i]);

	char *tmp_cmd = (char *)NULL;
	tmp_cmd = (char *)xcalloc(total_len + (i + 1) + 1, sizeof(char));
	for (i = 0; i <= args_n; i++) {
		strcat(tmp_cmd, comm[i]);
		strcat(tmp_cmd, " ");
	}

	int ret = launch_execle(tmp_cmd);
	free(tmp_cmd);
	tmp_cmd = (char *)NULL;
	if (ret == EXIT_SUCCESS) {
		/* If 'rm sel' and command is successful, deselect everything */
		if (is_sel && strcmp(comm[0], "rm") == 0) {
			for (i = 0; i < sel_n; i++)
				free(sel_elements[i]);
			sel_n = 0;
			save_sel();
		}
		/* When should the screen actually be refreshed: 
		* 1) Creation or removal of file (in current files list)
		* 2) The contents of a directory (in current files list) changed */
		if (cd_lists_on_the_fly && strcmp(comm[1], "--help") != 0 
		&& strcmp(comm[1], "--version") != 0) {
			while (files--) free(dirlist[files]);
			list_dir();
		}
		return EXIT_SUCCESS;
	}
	else
		return EXIT_FAILURE;
	/* Error messages will be printed by launch_execve() itself */
}

int
search_function(char **comm)
/* List matching filenames in the specified directory */
/* This function just works, but its logic is crap */
{
	if (!comm || !comm[0])
		return EXIT_FAILURE;

	/* If search string (comm[0]) is "/search", comm[0]+1 returns
	 * "search" */
	char *search_str = comm[0] + 1, *deq_dir = (char *)NULL, 
		 *search_path = (char *)NULL;
	mode_t file_type = 0;
	struct stat file_attrib;

	/* If there are two arguments, the one starting with '-' is the
	 * filetype and the other is the path */
	if (comm[1] && comm[2]) {
		if (comm[1][0] == '-') {
			file_type = (mode_t)comm[1][1];
			search_path = comm[2];
		}
		else if (comm[2][0] == '-') {
			file_type = (mode_t)comm[2][1];
			search_path = comm[1];
		}
		else
			search_path = comm[1];
	}

	/* If just one argument, '-' indicates filetype. Else, we have a
	 * path */
	else if (comm[1]) {
		if (comm[1][0] == '-')
			file_type = (mode_t)comm[1][1];
		else
			search_path = comm[1];
	}
	/* If no arguments, search_path will be NULL and file_type zero */

	/* Convert filetype into a macro that can be decoded by stat() */
	if (file_type) {
		switch (file_type) {
		case 'd': file_type = S_IFDIR; break;
		case 'r': file_type = S_IFREG; break;
		case 'l': file_type = S_IFLNK; break;
		case 's': file_type = S_IFSOCK; break;
		case 'f': file_type = S_IFIFO; break;
		case 'b': file_type = S_IFBLK; break;
		case 'c': file_type = S_IFCHR; break;
		default:
			fprintf(stderr, "%s: '%c': Unrecognized filetype\n",
					PROGRAM_NAME, (char)file_type);
			return EXIT_FAILURE;
		}
	}

	struct winsize w;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
	/* ws_col and ws_row are both unsigned short int according to 
	 * /bits/ioctl-types.h */
	unsigned short tcols = w.ws_col; /* This one is global */

	/* We will store here pointers to file names to be printed */
	char **pfiles = (char **)NULL;
	size_t found = 0;

				/* #############################
				 * #         WILDACRDS         #
				 * #############################*/

	if (strcntchr(search_str, '*') != -1
	|| strcntchr(search_str, '?') != -1
	|| strcntchr(search_str, '[') != -1) {

		/* If we have a path ("/search_str /path"), chdir into it, since
		 * glob() works on CWD */
		if (search_path && *search_path != 0x00) {
			deq_dir = dequote_str(search_path, 0);

			if (!deq_dir) {
				fprintf(stderr, _("%s: %s: Error dequoting filename\n"),
						PROGRAM_NAME, comm[1]);
				return EXIT_FAILURE;
			}

			if (chdir(deq_dir) == -1) {
				fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, deq_dir,
						strerror(errno));
				free(deq_dir);
				return EXIT_FAILURE;
			}
		}

		/* Get globbed files */
		glob_t globbed_files;
		int ret = glob(search_str, 0, NULL, &globbed_files);
		if (ret == 0) {

			int last_column = 0;
			size_t len = 0, flongest = 0, columns_n = 0;
			pfiles = (char **)xnmalloc(globbed_files.gl_pathc + 1, 
									   sizeof(char *));
			size_t *files_len = (size_t *)xnmalloc(globbed_files.gl_pathc
												+ 1, sizeof(size_t));

			/* If we have a path */
			if (deq_dir) {
				size_t i;
				for (i = 0; globbed_files.gl_pathv[i]; i++) {
					if (strcmp(globbed_files.gl_pathv[i], ".") == 0
					|| strcmp(globbed_files.gl_pathv[i], "..") == 0)
						continue;
					if (file_type) {
						/* Simply skip all files not matching file_type */
						if (lstat(globbed_files.gl_pathv[i], 
								  &file_attrib) == -1)
							continue;
						if ((file_attrib.st_mode & S_IFMT) != file_type)
							continue;
					}
					/* Store pointer to maching filename in array of 
					 * pointers */
					pfiles[found] = globbed_files.gl_pathv[i];

					/* Get the longest filename in the list */
					len = (unicode) ? u8_xstrlen(pfiles[found])
						  : strlen(pfiles[found]);
					files_len[found++] = len;
					if (len > flongest)
						flongest = len;
				}

				/* Print the result */
				if (found) {

					if (flongest <= 0 || flongest > tcols)
						columns_n = 1;
					else
						columns_n = (size_t)tcols / (flongest + 1);

					if (columns_n > found)
						columns_n = found;

					for (i = 0; i < found; i++) {
						if ((i + 1) % columns_n == 0)
							last_column = 1;
						else
							last_column = 0;
						colors_list(pfiles[i], 0, (last_column 
									|| i == found - 1) ? 0 : 
									(int)(flongest - files_len[i]) + 1, 
									(last_column || i == found - 1)
									? 1 : 0);
						/* Second argument to colors_list() is:
						 * 0: Do not print any ELN 
						 * Positive number: Print positive number as ELN
						 * -1: Print "?" instead of an ELN */
					}
				}
			}

			/* If no path was specified */
			else {
				size_t i, j;
				int *index = (int *)xnmalloc(globbed_files.gl_pathc + 1, 
											 sizeof(int));
				for (i = 0; globbed_files.gl_pathv[i]; i++) {
					if (strcmp(globbed_files.gl_pathv[i], ".") == 0
					|| strcmp(globbed_files.gl_pathv[i], "..") == 0)
						continue;
					if (file_type) {
						if (lstat(globbed_files.gl_pathv[i], 
						&file_attrib) == -1)
							continue;
						if ((file_attrib.st_mode & S_IFMT) != file_type)
							continue;
					}
					pfiles[found] = globbed_files.gl_pathv[i];

					/* In case 'index' is not found in the next for loop,
					 * that is, if the globbed file is not found in the
					 * current dir list, 'index' value would be that of
					 * the previous file if 'index' is not set to zero
					 * in each for iteration */
					index[found] = -1;
					for (j = 0; j < files; j++) {
						if (strcmp(globbed_files.gl_pathv[i],
						dirlist[j]) == 0) {
							index[found] = (int)j;
							break;
						}
					}

					size_t elnn = (index[found] != -1 ) ? 
								  digits_in_num(index[found] + 1) : 1;
					len = ((unicode) ? u8_xstrlen(pfiles[found]) 
						  : strlen(pfiles[found])) + elnn + 1;
					/* len == ELN + space + filename */
					files_len[found] = len;
					if (len > flongest)
						flongest = len;
					found++;
				}

				if (found) {

					if (flongest <= 0 || flongest > tcols)
						columns_n = 1;
					else
						columns_n = tcols / (flongest + 1);

					if ((size_t)columns_n > found)
						columns_n = found;

					for (i = 0; i < found; i++) {
						if ((i + 1) % columns_n == 0)
							last_column = 1;
						else
							last_column = 0;

						colors_list(pfiles[i], (index[i] != -1) 
									? index[i] + 1 : -1, (last_column
									|| i == found - 1) ? 0
									: (int)(flongest - files_len[i]) + 1, 
									(last_column || i == found - 1)
									? 1 : 0);
					}
				}

				free(index);
			}

			free(files_len);
			free(pfiles);

			if (!found) 
				printf(_("%s: No matches found\n"), PROGRAM_NAME);
		}
		else 
			printf(_("%s: No matches found\n"), PROGRAM_NAME);

		globfree(&globbed_files);

		/* Go back to the directory we came from */
		if (deq_dir) {
			free(deq_dir);

			if (chdir(path) == -1) {
				fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME,
						path, strerror(errno));
				return EXIT_FAILURE;
			}

			return EXIT_SUCCESS;
		}
	}

				/* #####################
				 * #    NO WILDCARDS   #
				 * #####################*/

	else {
		int last_column = 0;
		size_t len = 0, flongest = 0, i = 0, columns_n = 0;

		/* If /search_str /path */
		if (search_path && *search_path != 0x00) {
			deq_dir = dequote_str(search_path, 0);
			if (!deq_dir) {
				fprintf(stderr, _("%s: %s: Error dequoting filename\n"), 
						PROGRAM_NAME, comm[1]);
				return EXIT_FAILURE;
			}

			struct dirent **search_list;
			int search_files = 0;

			search_files = scandir(deq_dir, &search_list, NULL, alphasort);
			if (search_files == -1) {
				fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, deq_dir, 
						strerror(errno));
				free(deq_dir);
				return EXIT_FAILURE;
			}

			if (chdir(deq_dir) == -1) {
				fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, deq_dir, 
						strerror(errno));

				for (i = 0; i < (size_t)search_files; i++)
					free(search_list[i]);
				free(search_list);
				free(deq_dir);

				return EXIT_FAILURE;
			}

			pfiles = (char **)xnmalloc((size_t)search_files + 1,
									   sizeof(char *));
			size_t *files_len = (size_t *)xnmalloc((size_t)search_files
												   + 1, sizeof(size_t)); 

			for (i = 0; i < (size_t)search_files; i++) {
				if (strstr(search_list[i]->d_name, search_str)) {
					if (file_type) {

						if (lstat(search_list[i]->d_name,
						&file_attrib) == -1)
							continue;

						if ((file_attrib.st_mode & S_IFMT) == file_type) {
							pfiles[found] = search_list[i]->d_name;
							len = (unicode)
							? u8_xstrlen(search_list[i]->d_name) 
							: strlen(search_list[i]->d_name);

							files_len[found++] = len;
							if (len > flongest)
								flongest = len;
						}
					}
					else {
						pfiles[found] = search_list[i]->d_name;
						len = (unicode)
							? u8_xstrlen(search_list[i]->d_name) 
							: strlen(search_list[i]->d_name);
						files_len[found++] = len;
						if (len > flongest)
							flongest = len;
					}
				}
			}

			if (found) {

					if (flongest <= 0 || flongest > tcols)
						columns_n = 1;
					else
						columns_n = (size_t)tcols / (flongest + 1);

					if (columns_n > found)
						columns_n = found;

				for (i = 0; i < found; i++) {

					if ((i + 1) % columns_n == 0)
						last_column = 1;
					else
						last_column = 0;

					colors_list(pfiles[i], 0, (last_column
								|| i == found - 1) ? 0
								: (int)(flongest - files_len[i]) + 1, 
								(last_column || i == found - 1)
								? 1 : 0);
				}
			}

			for (i = 0; i < (size_t)search_files; i++)
				free(search_list[i]);
			free(search_list);

			free(files_len);
			free(pfiles);

			if (chdir(path) == -1) {
				fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME,
						path, strerror(errno));

				free(deq_dir);

				return EXIT_FAILURE;
			}
		}

		/* If /search_str */
		else {
			int *index = (int *)xnmalloc(files + 1, sizeof(int));
			pfiles = (char **)xnmalloc(files + 1, sizeof(char *));
			size_t *files_len = (size_t *)xnmalloc(files + 1,
											sizeof(size_t));

			for (i = 0; i < files; i++) {
				/* strstr finds substr in STR, as if STR where
				 * "*substr*" */
				if (strstr(dirlist[i], search_str)) {
					if (file_type) {
						if (lstat(dirlist[i], &file_attrib) == -1)
							continue;
						if ((file_attrib.st_mode & S_IFMT) == file_type) {
							index[found] = (int)i;
							pfiles[found] = dirlist[i];

							len = ((unicode) ? u8_xstrlen(pfiles[found]) 
								  : strlen(pfiles[found])) 
								  + (size_t)((index[found] != -1) 
								  ? digits_in_num(index[found]) + 1 : 2);
							files_len[found] = len;
							if (len > flongest) {
							flongest = len;
							}

							found++;
						}
					}
					else {

						index[found] = (int)i;
						pfiles[found] = dirlist[i];

						len = ((unicode) ? u8_xstrlen(pfiles[found]) 
							  : strlen(pfiles[found])) + 
							  (size_t)((index[found] != -1) 
							  ? digits_in_num(index[found]) + 1 : 2);
						files_len[found] = len;
						if (len > flongest) {
						flongest = len;
						}

						found++;
					}
				}
			}

			if (found) {

					if (flongest <= 0 || flongest > tcols)
						columns_n = 1;
					else
						columns_n = tcols / (flongest + 1);

					if ((size_t)columns_n > found)
						columns_n = found;

				for (i = 0; i < found; i++) {

					if ((i + 1) % columns_n == 0)
						last_column = 1;
					else
						last_column = 0;

					colors_list(pfiles[i], (index[i] != -1) 
								? index[i] + 1 : -1, (last_column 
								|| i == found - 1) ? 0 
								: (int)(flongest - files_len[i]) + 1, 
								(last_column || i == found - 1) ? 1 : 0);
				}
			}

			free(index);
			free(files_len);
			free(pfiles);
		}

		if (!found)
			printf(_("%s: No matches found\n"), PROGRAM_NAME);
	}

	if (deq_dir)
		free(deq_dir);

	if (!found)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

int
copy_function(char **comm)
{
	if (strcmp(comm[0], "paste") == 0)
		strncpy(comm[0], "cp\0", 3);

	log_function(comm);

	/* #####If SEL###### */
	if (is_sel) {
		char *tmp_cmd = (char *)NULL;
		size_t total_len = 0, i = 0;
		for (i = 0; i <= args_n; i++) {
			total_len += strlen(comm[i]);
		}
		tmp_cmd = (char *)xcalloc(total_len + (i + 1) + 2, sizeof(char));
		for (i = 0; i <= args_n; i++) {
			strcat(tmp_cmd, comm[i]);
			strcat(tmp_cmd, " ");
		}
		if (sel_is_last)
			strcat(tmp_cmd, ".");

		int ret = 0;
		ret = launch_execle(tmp_cmd);
		free(tmp_cmd);

		if (ret == EXIT_SUCCESS) {
			/* If 'mv sel' and command is successful deselect everything,
			 * since sel files are note there anymore */
			if (strcmp(comm[0], "mv") == 0) {
				for (i = 0; i < sel_n; i++)
					free(sel_elements[i]);
				sel_n = 0;
				save_sel();
			}
			if (cd_lists_on_the_fly) {
				while (files--) free(dirlist[files]);
				list_dir();
			}
			return EXIT_SUCCESS;
		}
		else
			return EXIT_FAILURE;
	}

	/* ####If NOT SEL#######... */
	return run_and_refresh(comm);
}

char **
get_bookmarks(char *bookmarks_file)
/* Get bookmarks from BOOKMARKS_FILE, store them in the bookmarks array
 * and the amount of bookmarks in the global variable bm_n, used later
 * by bookmarks_function() */
{
	size_t bm_n = 0;
	char **bookmarks = (char **)NULL;
	FILE *bm_fp;
	bm_fp = fopen(bookmarks_file, "r");
	if (!bm_fp) {
		_err(0, NOPRINT_PROMPT, "%s: bookmarks: %s: %s\n", PROGRAM_NAME, 
			 bookmarks_file, strerror(errno));
		return (char **)NULL;
	}

	/* Get bookmarks from the bookmarks file */

	char *line = (char *)NULL;
	size_t line_size = 0;
	ssize_t line_len = 0;
	while ((line_len = getline(&line, &line_size, bm_fp)) > 0) {
		/* If a comment or empty line */
		if (*line == '#' || *line == '\n')
			continue;
		/* Since bookmarks are shortcuts to paths, there must be at least
		 * one slash */
		int slash = 0;
		size_t i;
		for (i = 0; i < (size_t)line_len; i++) {
			if (line[i] == '/') {
				slash = 1;
				break;
			}
		}
		if (!slash)
			continue;
		/* Allocate memory to store the bookmark */
		bookmarks = (char **)xrealloc(bookmarks, (bm_n + 1)
									  * sizeof(char *));
		bookmarks[bm_n] = (char *)xcalloc((size_t)line_len + 1,
										  sizeof(char));
		/* Remove terminating new line char and copy the entire line */
		if (line[line_len - 1] == '\n')
			line[line_len - 1] = 0x00;
		strcpy(bookmarks[bm_n], line);
		bm_n++;
	}
	free(line);
	fclose(bm_fp);

	bookmarks = (char **)xrealloc(bookmarks, (bm_n + 1) * sizeof(char *));
	bookmarks[bm_n] = (char *)NULL;	

	return bookmarks;
}

char **
bm_prompt(void)
{
	char *bm_sel = (char *)NULL;
	printf("%s%s\nEnter 'e' to edit your bookmarks or 'q' to quit.\n",
			NC_b, default_color);

	while (!bm_sel)
		bm_sel = rl_no_hist(_("Choose a bookmark: "));

	char **comm_bm = get_substr(bm_sel, ' ');
	free(bm_sel);

	return comm_bm;
}

int
bookmark_del(char *name)
{
	FILE *bm_fp = NULL;
	bm_fp = fopen(BM_FILE, "r");
	if (!bm_fp)
		return EXIT_FAILURE;

	size_t i = 0;

	/* Get bookmarks from file */
	size_t line_size = 0;
	char *line = (char *)NULL, **bms = (char **)NULL;
	size_t bmn = 0;
	ssize_t line_len = 0;
	while ((line_len = getline(&line, &line_size, bm_fp)) > 0) {
		if (*line == '#' || *line == '\n')
			continue;
	int slash = 0;
		for (i = 0; i < (size_t)line_len; i++) {
			if (line[i] == '/') {
				slash = 1;
				break;
			}
		}
		if (!slash)
			continue;
		if (line[line_len - 1] == '\n')
			line[line_len - 1] = 0x00;
		bms = (char **)xrealloc(bms, (bmn + 1) * sizeof(char *));
		bms[bmn] = (char *)xcalloc((size_t)line_len + 1, sizeof(char));
		strcpy(bms[bmn++], line);
	}
	free(line);
	line = (char *)NULL;

	if (!bmn) {
		puts(_("bookmarks: There are no bookmarks"));
		fclose(bm_fp);
		return EXIT_SUCCESS;
	}

	char **del_elements = (char **)NULL;
	int cmd_line = -1; 
	/* This variable let us know two things: a) bookmark name was
	 * specified in command line; b) the index of this name in the
	 * bookmarks array. It is initialized as -1 since the index name
	 * could be zero */
	if (name) {
		for (i = 0; i < bmn; i++) {
			char *bm_name = strbtw(bms[i], ']', ':');
			if (!bm_name)
				continue;
			if (strcmp(name, bm_name) == 0) {
				free(bm_name);
				cmd_line = (int)i;
				break;
			}
			free(bm_name);
		}
	}

	/* If a valid bookmark name was passed in command line, copy the 
	 * corresponding bookmark index (plus 1, as if it were typed in the 
	 * bookmarks screen) to the del_elements array */
	if (cmd_line != -1) {
		del_elements = (char **)xnmalloc(2, sizeof(char *));
		del_elements[0] = (char *)xnmalloc(digits_in_num(cmd_line + 1) 
										   + 1, sizeof(char));
		sprintf(del_elements[0], "%d", cmd_line + 1);
		del_elements[1] = (char *)NULL;
	}

	/* If bookmark name was passed but it is not a valid bookmark */
	else if (name) {
		fprintf(stderr, "bookmarks: '%s': No such bookmark\n", name);
		for (i = 0; i < bmn; i++)
			free(bms[i]);
		free(bms);
		fclose(bm_fp);
		return EXIT_FAILURE;
	}

	/* If not name, list bookmarks and get user input */
	else {
		printf("%sBookmarks%s\n\n", white, NC);
		for (i = 0; i < bmn; i++)
			printf("%s%zu %s%s%s\n", eln_color, i + 1, d_cyan, bms[i],
				   NC);

		/* Get user input */
		printf(_("\n%s%sEnter 'q' to quit.\n"), NC, default_color);
		char *input = (char *)NULL;
		while (!input)
			input = rl_no_hist("Bookmark(s) to be deleted "
							   "(ex: 1 2-6, or *): ");

		del_elements = get_substr(input, ' ');
		free(input);
		input = (char *)NULL;
		if (!del_elements) {
			for (i = 0; i < bmn; i++)
				free(bms[i]);
			free(bms);
			fclose(bm_fp);
			fprintf(stderr, _("bookmarks: Error parsing input\n"));
			return EXIT_FAILURE;
		}
	}

	/* We have input */
	/* If quit */
	/* I inspect all substrings entered by the user for "q" before any
	 * other value to prevent some accidental deletion, like "1 q", or 
	 * worst, "* q" */
	for (i = 0; del_elements[i]; i++) {
		int quit = 0;
		if (strcmp(del_elements[i], "q") == 0)
			quit = 1;
		else if (is_number(del_elements[i]) 
		&& (atoi(del_elements[i]) <= 0
		|| atoi(del_elements[i]) > (int)bmn)) {
			fprintf(stderr, "bookmarks: '%s': No such bookmark\n", 
					del_elements[i]);
			quit = 1;
		}
		if (quit) {
			for (i = 0; i < bmn; i++)
				free(bms[i]);
			free(bms);
			for (i = 0; del_elements[i]; i++)
				free(del_elements[i]);
			free(del_elements);
			fclose(bm_fp);
			return EXIT_SUCCESS;
		}
	}

	/* If "*", simply remove the bookmarks file */
	/* If there is some "*" in the input line (like "1 5 6-9 *"), it
	 * makes no sense to remove singles bookmarks: Just delete all of
	 * them at once */
	for (i = 0; del_elements[i]; i++) {
		if (strcmp(del_elements[i], "*") == 0) {
			/* Create a backup copy of the bookmarks file, just in case */
			char *bk_file = (char *)NULL;
			bk_file = (char *)xcalloc(strlen(CONFIG_DIR) + 14,
									  sizeof(char));
			sprintf(bk_file, "%s/bookmarks.bk", CONFIG_DIR);
			char *tmp_cmd[] = { "cp", BM_FILE, bk_file, NULL };
			int ret = launch_execve(tmp_cmd, FOREGROUND);
			/* Remove the bookmarks file, free stuff, and exit */
			if (ret == EXIT_SUCCESS) {
				remove(BM_FILE);
				printf(_("bookmarks: All bookmarks were deleted\n "
						 "However, a backup copy was created (%s)\n"),
						  bk_file);
				free(bk_file);
				bk_file = (char *)NULL;
			}
			else
				printf(_("bookmarks: Error creating backup file. No "
						 "bookmark was deleted\n"));
			for (i = 0; i < bmn; i++)
				free(bms[i]);
			free(bms);
			for (i = 0; del_elements[i]; i++)
				free(del_elements[i]);
			free(del_elements);
			fclose(bm_fp);

			/* Update bookmark names for TAB completion */
			get_bm_names();

			/* If the argument "*" was specified in command line */
			if (cmd_line != -1)
				fputs("All bookmarks succesfully removed\n", stdout);

			return EXIT_SUCCESS;
		}
	}

	/* Remove single bookmarks */
	/* Open a temporary file */
	char *tmp_file = (char *)NULL;
	tmp_file = (char *)xcalloc(strlen(CONFIG_DIR) + 8, sizeof(char));
	sprintf(tmp_file, "%s/bm_tmp", CONFIG_DIR);

	FILE *tmp_fp = fopen(tmp_file, "w+");
	if (!tmp_fp) {
		for (i = 0; i < bmn; i++)
			free(bms[i]);
		free(bms);
		fclose(bm_fp);
		fprintf(stderr, _("bookmarks: Error creating temporary file\n"));
		return EXIT_FAILURE;
	}

	/* Go back to the beginning of the bookmarks file */
	fseek(bm_fp, 0, SEEK_SET);

	/* Dump into the tmp file everything except bookmarks marked for 
	 * deletion */

	line_len = 0;
	line_size = 0;
	char *lineb = (char *)NULL;
	while ((line_len = getline(&lineb, &line_size, bm_fp)) > 0) {
		if (lineb[line_len - 1] == '\n')
			lineb[line_len - 1] = 0x00;
		int bm_found = 0;
		size_t j;
		for (j = 0; del_elements[j]; j++) {
			if (!is_number(del_elements[j]))
				continue;
			if (strcmp(bms[atoi(del_elements[j]) - 1], lineb) == 0)
				bm_found = 1;
		}
		if (bm_found)
			continue;
		fprintf(tmp_fp, "%s\n", lineb);
	}
	free(lineb);
	lineb = (char *)NULL;

	/* Free stuff */
	for (i = 0; del_elements[i]; i++)
		free(del_elements[i]);
	free(del_elements);

	for (i = 0; i < bmn; i++)
		free(bms[i]);
	free(bms);

	/* Close files */
	fclose(bm_fp);
	fclose(tmp_fp);

	/* Remove the old bookmarks file and make the tmp file the new 
	 * bookmarks file*/
	remove(BM_FILE);
	rename(tmp_file, BM_FILE);
	free(tmp_file);
	tmp_file = (char *)NULL;

	/* Update bookmark names for TAB completion */
	get_bm_names();

	/* If the bookmark to be removed was specified in command line */
	if (cmd_line != -1)
		printf("Successfully removed '%s'\n", name);

	return EXIT_SUCCESS;
}

int
bookmark_add(char *file)
{
	if (!file)
		return EXIT_FAILURE;

	int mod_file = 0;
	/* If not absolute path, prepend current path to file */
	if (*file != '/') {
		char *tmp_file = (char *)NULL;
		tmp_file = (char *)xnmalloc((strlen(path) + strlen(file) + 2), 
								    sizeof(char));
		sprintf(tmp_file, "%s/%s", path, file);
		file = tmp_file;
		tmp_file = (char *)NULL;
		mod_file = 1;
	}

	/* Check if FILE is an available path */

	FILE *bm_fp = fopen(BM_FILE, "r");	
	if (!bm_fp) {
		fprintf(stderr, _("bookmarks: Error opening the bookmarks file\n"));
		if (mod_file) {
			free(file);
		}
		return EXIT_FAILURE;
	}
	int dup = 0;
	char **bms = (char **)NULL;
	size_t line_size = 0, i, bmn = 0;
	char *line = (char *)NULL;
	ssize_t line_len = 0;
	while ((line_len = getline(&line, &line_size, bm_fp)) > 0) {
		if (*line == '#')
			continue;

		char *tmp_line = (char *)NULL;
		tmp_line = strchr (line, '/');
		if (tmp_line) {
			size_t tmp_line_len = strlen(tmp_line);
			if (tmp_line_len && tmp_line[tmp_line_len - 1] == '\n')
				tmp_line[tmp_line_len - 1] = 0x00;
			if (strcmp(tmp_line, file) == 0) {
				fprintf(stderr, _("bookmarks: '%s': Path already "
								   "bookmarked\n"), file);
				dup = 1;
				break;
			}
			tmp_line = (char *)NULL;
		}
		/* Store lines: used later to check hotkeys */
		bms = (char **)xrealloc(bms, (bmn + 1) * sizeof(char *));
		bms[bmn] = (char *)xcalloc(strlen(line) + 1, sizeof(char));
		strcpy(bms[bmn++], line);

	}

	free(line);
	line = (char *)NULL;
	fclose(bm_fp);

	if (dup) {
		for (i = 0; i < bmn; i++)
			free(bms[i]);
		free(bms);
		if (mod_file)
			free(file);
		return EXIT_FAILURE;
	}

	/* If path is available */

	char *name = (char *)NULL, *hk = (char *)NULL, *tmp = (char *)NULL;

	/* Ask for data to construct the bookmark line. Both values could be
	 * NULL */
	puts("Bookmark line example: [sc]name:path");
	hk = rl_no_hist("Shortcut: ");

	/* Check if hotkey is available */
	if (hk) {
		char *tmp_line = (char *)NULL;
		for (i = 0; i < bmn; i++) {
			tmp_line = strbtw(bms[i], '[', ']');
			if (tmp_line) {
				if (strcmp(hk, tmp_line) == 0) {
					fprintf(stderr, _("bookmarks: '%s': This shortcut is "
								 	  "already in use\n"), hk);

					dup = 1;
					free(tmp_line);
					break;
				}
				free(tmp_line);
			}
		}
	}

	if (dup) {
		if (hk)
			free(hk);
		for (i = 0; i < bmn; i++)
			free(bms[i]);
		free(bms);
		if (mod_file)
			free(file);
		return EXIT_FAILURE;
	}

	name = rl_no_hist("Name: ");

	if (name) {
		/* Check name is not duplicated */

		char *tmp_line = (char *)NULL;
		for (i = 0; i < bmn; i++) {
			tmp_line = strbtw(bms[i], ']', ':');
			if (tmp_line) {
				if (strcmp(name, tmp_line) == 0) {
					fprintf(stderr, _("bookmarks: '%s': This name is "
								 	  "already in use\n"), name);

					dup = 1;
					free(tmp_line);
					break;
				}
				free(tmp_line);
			}
		}

		if (dup) {
			free(name);
			if (hk)
				free(hk);
			for (i = 0; i < bmn; i++)
				free(bms[i]);
			free(bms);
			if (mod_file)
				free(file);
			return EXIT_FAILURE;
		}

		/* Generate the bookmark line */
		if (hk) { /* name AND hk */
			tmp = (char *)xcalloc(strlen(hk) + strlen(name)
								  + strlen(file) + 5, sizeof(char));
			sprintf(tmp, "[%s]%s:%s\n", hk, name, file);
			free(hk);
		}
		else { /* Only name */
			tmp = (char *)xcalloc(strlen(name) + strlen(file) + 3, 
								  sizeof(char));
			sprintf(tmp, "%s:%s\n", name, file);
		}
		free(name);
		name = (char *)NULL;
	}

	else if (hk) { /* Only hk */
		tmp = (char *)xcalloc(strlen(hk) + strlen(file) + 4,
							  sizeof(char));
		sprintf(tmp, "[%s]%s\n", hk, file);
		free(hk);
		hk = (char *)NULL;
	}

	else { /* Neither shortcut nor name: only path */
		tmp = (char *)xnmalloc(strlen(file) + 2, sizeof(char));
		sprintf(tmp, "%s\n", file);
	}

	for (i = 0; i < bmn; i++)
		free(bms[i]);
	free(bms);
	bms = (char **)NULL;

	if (!tmp) {
		fprintf(stderr, "bookmarks: Error generating the bookmark line\n");
		return EXIT_FAILURE;
	}

	/* Once we have the bookmark line, write it to the bookmarks file */

	bm_fp = fopen(BM_FILE, "a+");
	if (!bm_fp) {
		fprintf(stderr, _("bookmarks: Error opening the bookmarks file\n"));
		free(tmp);
		return EXIT_FAILURE;
	}

	if (mod_file)
		free(file);

	if (fseek(bm_fp, 0L, SEEK_END) == -1) {
		fprintf(stderr, _("bookmarks: Error opening the bookmarks file\n"));
		free(tmp);
		fclose(bm_fp);
		return EXIT_FAILURE;
	}

	/* Everything is fine: add the new bookmark to the bookmarks file */
	fprintf(bm_fp, "%s", tmp);
	fclose(bm_fp);
	printf(_("File succesfully bookmarked\n"));
	free(tmp);

	/* Update bookmark names for TAB completion */
	get_bm_names();

	return EXIT_SUCCESS;
}

int
open_bookmark(char **cmd)
{
	/* Get bookmark lines from the bookmarks file */
	char **bookmarks = get_bookmarks(BM_FILE);

	/* Get the amount of available bookmarks */
	size_t bm_n = 0;
	for (bm_n = 0; bookmarks[bm_n]; bm_n++);

	/* If no bookmarks */
	if (bm_n == 0 && !cmd[1]) {
		free(bookmarks);
		bookmarks = (char **)NULL;
		printf(_("Bookmarks: There are no bookmarks\nEnter 'bm edit' to "
				 "edit the bookmarks file or 'bm add PATH' to add a new "
				 "bookmark\n"));
		return EXIT_SUCCESS;
	}


	/* If there are bookmarks... */

	/* Store shortcut, name, and path of each bookmark into different
	 * arrays but linked by the array index */

	char **bm_paths = (char **)NULL, **hot_keys = (char **)NULL, 
		 **bm_names = (char **)NULL;
	bm_paths = (char **)xnmalloc(bm_n, sizeof(char *));
	hot_keys = (char **)xnmalloc(bm_n, sizeof(char *));
	bm_names = (char **)xnmalloc(bm_n, sizeof(char *));

	register size_t i;

	for (i = 0; i < bm_n; i++) {

		/* Get paths */
		int ret = strcntchr(bookmarks[i], '/');
		if (ret != -1) {
			/* If there is some slash in the shortcut or in the name
			 * string, the bookmark path will be wrong. FIX! Or just
			 * disallow the use of slashes for bookmark names and
			 * shortcuts */
			bm_paths[i] = (char *)xnmalloc(strlen(bookmarks[i] + ret)
										   + 1, sizeof(char));
			strcpy(bm_paths[i], bookmarks[i] + ret);
		}
		else
			bm_paths[i] = (char *)NULL;

		/* Get shortcuts */
		char *str_b = strbtw(bookmarks[i], '[', ']');
		if (str_b) {
			hot_keys[i] = (char *)xnmalloc(strlen(str_b) + 1,
										   sizeof(char));
			strcpy(hot_keys[i], str_b);
			free(str_b);
			str_b = (char *)NULL;
		}
		else 
			hot_keys[i] = (char *)NULL;

		/* Get names */
		char *str_name = strbtw(bookmarks[i], ']', ':');
		if (str_name) {
			bm_names[i] = (char *)xnmalloc(strlen(str_name) + 1,
										   sizeof(char));
			strcpy(bm_names[i], str_name);
			free(str_name);
		}
		else {
			str_name = strbfr(bookmarks[i], ':');
			if (str_name) {
				bm_names[i] = (char *)xnmalloc(strlen(str_name)
											   + 1, sizeof(char));
				strcpy(bm_names[i], str_name);
				free(str_name);
			}
			else
				bm_names[i] = (char *)NULL;
		}
	}

	/* Once we have all bookmarks data loaded, there are two alternatives:
	 * either to display the bookmarks screen and let the use enter the
	 * bookmark she wants (that is, in case the command is just "bm"), or
	 * the user entered "bm argument", in which case we just need to 
	 * execute the bookmarks function with the corresponding argument */
 
	struct stat file_attrib;
	char **arg = (char **)NULL;
	int error_code = 0;

	if (!cmd[1]) { /* Just "bm" */
		if (clear_screen)
			CLEAR;
		printf(_("%sBookmarks Manager%s\n\n"), white, NC);

		/* Print bookmarks taking into account the existence of shortcut,
		 * name, and path for each bookmark */
		for (i = 0; i < bm_n; i++) {
			int path_ok = stat(bm_paths[i], &file_attrib);
			if (hot_keys[i]) {
				if (bm_names[i]) {
					if (path_ok == 0) {
						/* Shortcut, name, and path OK */
						/* Directory or symlink to directory */
						if ((file_attrib.st_mode & S_IFMT) == S_IFDIR)
							printf("%s%zu %s[%s]%s %s%s%s\n", eln_color,
									i + 1, white, hot_keys[i], NC, cyan, 
									bm_names[i], NC);
						else /* No directory */
							printf("%s%zu %s[%s]%s %s%s%s\n", eln_color,
									i + 1, white, hot_keys[i], NC,
									default_color, bm_names[i], NC);

					}
					else /* Invalid path */
						printf("%s%zu [%s] %s%s\n", gray, i + 1,
								hot_keys[i], bm_names[i], NC);
				}
				else {
					if (path_ok == 0) { /* Shortcut and path OK */
						if ((file_attrib.st_mode & S_IFMT) == S_IFDIR)
							printf("%s%zu %s[%s]%s %s%s%s\n", eln_color,
									i + 1, white, hot_keys[i], NC, cyan, 
									bm_paths[i], NC);
						else
							printf("%s%zu %s[%s]%s %s%s%s\n", eln_color,
									i + 1, white, hot_keys[i], NC,
									default_color, bm_paths[i], NC);
					}
					else
						printf("%s%zu [%s] %s%s\n", gray, i + 1,
								hot_keys[i], bm_paths[i], NC);
				}
			}
			else {
				if (bm_names[i]) {
					if (path_ok == 0) { /* Name and path OK */
						if ((file_attrib.st_mode & S_IFMT) == S_IFDIR)
							printf("%s%zu %s%s%s\n", eln_color, i + 1,
									cyan, bm_names[i], NC);
						else
							printf("%s%zu %s%s%s%s\n", eln_color, i + 1,
									NC, default_color, bm_names[i], NC);
					}
					else
						printf("%s%zu %s%s\n", gray, i + 1, bm_names[i],
								NC);
				}
				else {
					if (path_ok == 0) { /* Only path OK */
						if ((file_attrib.st_mode & S_IFMT) == S_IFDIR)
							printf("%s%zu %s%s%s\n", eln_color, i + 1,
									cyan, bm_paths[i], NC);
						else
							printf("%s%zu %s%s%s%s\n", eln_color, i + 1,
									NC, default_color, bm_paths[i], NC);
					}
					else
						printf("%s%zu %s%s\n", gray, i + 1, bm_paths[i],
								NC);
				}
			}
		}

		/* User selection. Display the prompt */
		arg = bm_prompt();
		if (!arg) {
			error_code = 1;
			goto free_and_exit;
		}
	}

	if (!arg) { /* arg is NULL in case of "bm argument" */
		size_t len = 0;
		for (i = 1; cmd[i]; i++) {
			arg = (char **)xrealloc(arg, (len + 1) * sizeof(char *));
			arg[len] = (char *)xnmalloc(strlen(cmd[i]) + 1, sizeof(char));
			strcpy(arg[len++], cmd[i]);
		}
		arg = (char **)xrealloc(arg, (len + 1) * sizeof(char *));
		arg[len] = (char *)NULL;
	}

	char *tmp_path = (char *)NULL;
	int reload_bm = 0, go_dirlist = 0;

	/* Case "edit" */
	if (strcmp(arg[0], "e") == 0 || strcmp(arg[0], "edit") == 0) {
		if (!arg[1]) { /* If no application specified */
			if (!(flags & FILE_CMD_OK)) {
				fprintf(stderr, _("Bookmarks: 'file' command not found."
								  "Try 'e APPLICATION'\n"));
				error_code = 1;
			}
			else {
				char *tmp_cmd[] = { "mime", BM_FILE, NULL };
				int ret = mime_open(tmp_cmd);
				if (ret != 0) {
					fprintf(stderr, _("Bookmarks: Try 'e APPLICATION'\n"));
					error_code = 1;
				}
				else
					reload_bm = 1;
			}
		}
		/* If application was specified */
		else {
			char *tmp_cmd[] = { arg[1], BM_FILE, NULL };
			int ret = -1;
			if ((ret = launch_execve(tmp_cmd, FOREGROUND)) != EXIT_SUCCESS)
				error_code = 1;
			else
				reload_bm = 1;
		}
		goto free_and_exit;
	}

	/* Case "quit" */
	else if (strcmp(arg[0], "q") == 0 || strcmp(arg[0], "quit") == 0)
		goto free_and_exit;

	/* Get the corresponding bookmark path */
	if (is_number(arg[0])) {
		int eln = atoi(arg[0]);
		if (eln <= 0 || (size_t)eln > bm_n) {
			fprintf(stderr, _("Bookmarks: '%d': No such ELN\n"), eln);
			error_code = 1;
			goto free_and_exit;
		}
		else
			tmp_path = bm_paths[eln - 1];
	}
	else { /* If string, check shortcuts and names */
		for (i = 0; i < bm_n; i++) {
			if (strcmp(arg[0], hot_keys[i]) == 0
			|| strcmp(arg[0], bm_names[i]) == 0) {
				tmp_path = bm_paths[i];
				break;
			}
		}
	}

	if (!tmp_path) {
		fprintf(stderr, _("Bookmarks: '%s': No such bookmark\n"),
						  arg[0]);
		error_code = 1;
		goto free_and_exit;
	}

	/* Now we have the corresponding bookmark path */
	/* Check path existence */
	if (stat(tmp_path, &file_attrib) == -1) {
		fprintf(stderr, "Bookmarks: '%s': %s\n", tmp_path,
				strerror(errno));
		error_code = 1;
		goto free_and_exit;
	}

	/* If a directory */
	if ((file_attrib.st_mode & S_IFMT) == S_IFDIR) {
		int ret = chdir(tmp_path);
		if (ret == 0) {
			free(path);
			path = (char *)xnmalloc(strlen(tmp_path) + 1, sizeof(char));
			strcpy(path, tmp_path);
			add_to_dirhist(path);
			go_dirlist = 1;
		}
		else {
			error_code = 1;
			fprintf(stderr, "Bookmarks: '%s': %s\n", tmp_path,
					strerror(errno));
		}
		goto free_and_exit;
	}

	/* If a regular file */
	else if ((file_attrib.st_mode & S_IFMT) == S_IFREG) {
		if (arg[1]) {
			char *tmp_cmd[] = { arg[1], tmp_path, NULL };
			int ret = -1;
			if ((ret = launch_execve(tmp_cmd, FOREGROUND)) != EXIT_SUCCESS)
				error_code = 1;
		}
		else {
			if (!(flags & FILE_CMD_OK)) {
				fprintf(stderr, _("Bookmarks: 'file' command not found. "
								  "Try 'bookmark APPLICATION'\n"));
				error_code = 1;
			}
			else {
				char *tmp_cmd[] = { "mime", tmp_path, NULL };
				int ret = mime_open(tmp_cmd);
				if (ret != 0)
					error_code = 1;
			}
		}
		goto free_and_exit;
	}

	/* If neither directory nor regular file */
	else {
		fprintf(stderr, _("Bookmarks: '%s': Cannot open file\n"),
				tmp_path);
		error_code = 1;
		goto free_and_exit;
	}

	free_and_exit:
		for (i = 0; arg[i]; i++)
			free(arg[i]);
		free(arg);
		arg = (char **)NULL;

		for (i = 0; i < bm_n; i++) {
			free(bookmarks[i]);
			free(bm_paths[i]);
			free(bm_names[i]);
			free(hot_keys[i]);
		}
		free(bookmarks);
		free(bm_paths);
		free(bm_names);
		free(hot_keys);

		if (reload_bm && !cmd[1])
			bookmarks_function(cmd);

		if (go_dirlist && cd_lists_on_the_fly) {
			while (files--)
				free(dirlist[files]);
			list_dir();
		}

		if (error_code)
			return EXIT_FAILURE;
		else
			return EXIT_SUCCESS;
}

int
bookmarks_function(char **cmd)
{
	if (!config_ok) {
		fprintf(stderr, _("Bookmarks function disabled\n"));
		return EXIT_FAILURE;
	}

	/* If the bookmarks file doesn't exist, create it. NOTE: This file
	 * should be created at startup (by get_bm_names()), but we check
	 * it again here just in case it was meanwhile deleted for some
	 * reason */
	struct stat file_attrib;
	if (stat(BM_FILE, &file_attrib) == -1) {
		FILE *fp;
		fp = fopen(BM_FILE, "w+");
		if (!fp) {
			_err(0, NOPRINT_PROMPT, "bookmarks: '%s': %s\n", BM_FILE, 
				 strerror(errno));
			return EXIT_FAILURE;
		}
		else {
			fprintf(fp, "### This is %s bookmarks file ###\n\n"
					"# Empty and commented lines are ommited\n"
					"# The bookmarks syntax is: [hotkey]name:path\n"
					"# Example:\n"
					"# [t]test:/path/to/test\n", PROGRAM_NAME);
			fclose(fp);
		}
	}

	/* Check arguments */
	if (cmd[1]) {
		/* Add a bookmark */
		if (strcmp(cmd[1], "a") == 0 || strcmp(cmd[1], "add") == 0) {

			if (!cmd[2]) {
				printf(_("Usage: bookmarks, bm [a, add PATH]\n"));
				return EXIT_SUCCESS;
			}

			if (access(cmd[2], F_OK) != 0) {
				fprintf(stderr, "bookmarks: %s: %s\n", cmd[2],
						strerror(errno));
				return EXIT_FAILURE;
			}

			return bookmark_add(cmd[2]);
		}
		/* Delete bookmarks */
		else if (strcmp(cmd[1], "d") == 0
		|| strcmp(cmd[1], "del") == 0) {
			if (cmd[2])
				return bookmark_del(cmd[2]);
			else
				return bookmark_del(NULL);
		}
	}

	/* If no arguments or "bm [edit] [shortcut, name]" */
	return open_bookmark(cmd);
}

off_t
dir_size(char *dir)
{
	#define DU_TMP_FILE "/tmp/.du_size"

	if (!dir)
		return -1;

	FILE *du_fp = fopen(DU_TMP_FILE, "w");
	FILE *du_fp_err = fopen("/dev/null", "w");
	int stdout_bk = dup(STDOUT_FILENO); /* Save original stdout */
	int stderr_bk = dup(STDERR_FILENO); /* Save original stderr */
	dup2(fileno(du_fp), STDOUT_FILENO); /* Redirect stdout to the desired
	file */	
	dup2(fileno(du_fp_err), STDERR_FILENO); /* Redirect stderr to
	/dev/null */
	fclose(du_fp);
	fclose(du_fp_err);

/*	char *cmd = (char *)NULL;
	cmd = (char *)xcalloc(strlen(dir) + 10, sizeof(char));
	sprintf(cmd, "du -sh '%s'", dir);
	//int ret = launch_execle(cmd);
	launch_execle(cmd);
	free(cmd); */

	char *cmd[] = { "du", "--block-size=1", "-s", dir, NULL };
	launch_execve(cmd, FOREGROUND);

	dup2(stdout_bk, STDOUT_FILENO); /* Restore original stdout */
	dup2(stderr_bk, STDERR_FILENO); /* Restore original stderr */
	close(stdout_bk);
	close(stderr_bk);

/*	if (ret != 0) {
		puts("???");
		return;
	} */

	off_t retval = -1;

	if (access(DU_TMP_FILE, F_OK) == 0) {

		du_fp = fopen(DU_TMP_FILE, "r");

		if (du_fp) {
			/* I only need here the first field of the line, which is a
			 * file size and could only take a few bytes, so that 32
			 * bytes is more than enough */
			char line[32] = "";
			fgets(line, sizeof(line), du_fp);

			char *file_size = strbfr(line, '\t');

			if (file_size) {
				retval = (off_t)atoi(file_size);
				free(file_size);
			}

			fclose(du_fp);
		}

		remove(DU_TMP_FILE);
	}

	return retval;
}

int
properties_function(char **comm)
{
	if(!comm)
		return EXIT_FAILURE;

	int exit_status = EXIT_SUCCESS;

	/* Same thing as long view mode */
	if (comm[1] && (strcmp(comm[1], "all") == 0
	|| strcmp(comm[1], "a") == 0)) {

		short status = long_view;
		long_view = 1;
		int max, i;

		max = get_max_long_view();

		for (i = 0; i < (int)files; i++) {
			printf("%s%d%s ", eln_color, i + 1, NC);
			if (get_properties(dirlist[i], (int)long_view, max, 
							   (unicode) ? u8_xstrlen(dirlist[i]) 
							   : strlen(dirlist[i])) != 0)
				exit_status = EXIT_FAILURE;
		}

		long_view = status;

		return exit_status;
	}

	/* List files by size */
	register int i;
	if (comm[1] && (strcmp(comm[1], "size") == 0
	|| strcmp(comm[1], "s") == 0)) {

		struct stat file_attrib;
		/* Get largest filename to format the output */
		size_t largest = 0;

		for (i = (int)files; i--; ) {
			size_t file_len = (unicode) ? u8_xstrlen(dirlist[i])
							  : strlen(dirlist[i]);
			if (file_len > largest)
				largest = file_len + 1;
		}

		for (i = 0; i < (int)files; i++) {

			/* Get the amount of continuation bytes for each filename.
			 * This is necessary to properly format the output in case
			 * of non-ASCII strings */
			(unicode) ? u8_xstrlen(dirlist[i]) : strlen(dirlist[i]);

			lstat(dirlist[i], &file_attrib);
			char *size = get_size_unit(file_attrib.st_size);
			/* Print: 	filename	ELN		Padding		no new line */
			colors_list(dirlist[i], i + 1, (int)largest + cont_bt, 0);
			printf("%s%s%s\n", NC, default_color, (size) ? size : "??");

			if (size)
				free(size);
		}

		return EXIT_SUCCESS;
	}

	/* If "pr file file..." */
	for (i = 1; i <= (int)args_n; i++) {

		char *deq_file = dequote_str(comm[i], 0);

		if (!deq_file) {
			fprintf(stderr, _("%s: '%s': Error dequoting filename\n"), 
					PROGRAM_NAME, comm[i]);
			exit_status = EXIT_FAILURE;
			continue;
		}

		if (get_properties(deq_file, 0, 0, 0) != 0)
			exit_status = EXIT_FAILURE;
		free(deq_file);
	}

	return exit_status;
}

int
get_properties (char *filename, int _long, int max, size_t filename_len)
{
	if (!filename || !*filename)
		return EXIT_FAILURE;

	size_t len = strlen(filename);
	if (filename[len - 1] == '/')
		filename[len - 1] = 0x00;

	struct stat file_attrib;
	/* Check file existence */
	if (lstat(filename, &file_attrib) == -1) {
		fprintf(stderr, "%s: pr: '%s': %s\n", PROGRAM_NAME, filename, 
				strerror(errno));
		return EXIT_FAILURE;
	}
	/* Get file size */
	char *size_type = get_size_unit(file_attrib.st_size);

	/* Get file type (and color): */
	int sticky = 0;
	char file_type = 0, color[MAX_COLOR]= "";
	char *linkname = (char *)NULL;
	switch (file_attrib.st_mode & S_IFMT) {
	case S_IFREG:
		file_type='-';
/*		if (!(file_attrib.st_mode & S_IRUSR)) strcpy(color, nf_c); */
		if (access(filename, R_OK) == -1)
			strcpy(color, nf_c);
		else if (file_attrib.st_mode & S_ISUID)
			strcpy(color, su_c);
		else if (file_attrib.st_mode & S_ISGID)
			strcpy(color, sg_c);
		else {
			#ifdef _LINUX_CAP
			cap_t cap = cap_get_file(filename);
			if (cap) {
				strcpy(color, ca_c);
				cap_free(cap);
			}
			else if (file_attrib.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)) {
			#else
			if (file_attrib.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)) {
			#endif
				if (file_attrib.st_size == 0) strcpy(color, ee_c);
				else strcpy(color, ex_c);
			}
			else if (file_attrib.st_size == 0) strcpy(color, ef_c);
			else strcpy(color, fi_c);
		}
		break;

	case S_IFDIR:
		file_type='d';
		if (access(filename, R_OK|X_OK) != 0)
			strcpy(color, nd_c);
		else {
			int is_oth_w = 0;
			if (file_attrib.st_mode & S_ISVTX) sticky = 1;
			if (file_attrib.st_mode & S_IWOTH) is_oth_w = 1;
			size_t files_dir = count_dir(filename);
			strcpy(color, (sticky) ? ((is_oth_w) ? tw_c : 
				st_c) : ((is_oth_w) ? ow_c : 
				((files_dir == 2 || files_dir == 0) ? ed_c : di_c)));
		}
		break;

	case S_IFLNK:
		file_type = 'l';
		linkname = realpath(filename, (char *)NULL);
		if (linkname)
			strcpy(color, ln_c);
		else
			strcpy(color, or_c);
		break;
	case S_IFSOCK: file_type = 's'; strcpy(color, so_c); break;
	case S_IFBLK: file_type = 'b'; strcpy(color, bd_c); break;
	case S_IFCHR: file_type = 'c'; strcpy(color, cd_c); break;
	case S_IFIFO: file_type = 'p'; strcpy(color, pi_c); break;
	default: file_type = '?'; strcpy(color, no_c);
	}

	/* Get file permissions */
	char read_usr = '-', write_usr = '-', exec_usr = '-', 
		 read_grp = '-', write_grp = '-', exec_grp = '-',
		 read_others = '-', write_others = '-', exec_others = '-';

	mode_t val=(file_attrib.st_mode & ~S_IFMT);
	if (val & S_IRUSR) read_usr = 'r';
	if (val & S_IWUSR) write_usr = 'w'; 
	if (val & S_IXUSR) exec_usr = 'x';

	if (val & S_IRGRP) read_grp = 'r';
	if (val & S_IWGRP) write_grp = 'w';
	if (val & S_IXGRP) exec_grp = 'x';

	if (val & S_IROTH) read_others = 'r';
	if (val & S_IWOTH) write_others = 'w';
	if (val & S_IXOTH) exec_others = 'x';

	if (file_attrib.st_mode & S_ISUID) 
		(val & S_IXUSR) ? (exec_usr = 's') : (exec_usr = 'S');
	if (file_attrib.st_mode & S_ISGID) 
		(val & S_IXGRP) ? (exec_grp = 's') : (exec_grp = 'S');

	/* Get number of links to the file */
	nlink_t link_n = file_attrib.st_nlink;

	/* Get modification time */
	time_t time = file_attrib.st_mtim.tv_sec;
	struct tm *tm = localtime(&time);
	char mod_time[128] = "";
	if (time)
		/* Store formatted (and localized) date-time string into
		 * mod_time */
		strftime(mod_time, sizeof(mod_time), "%b %d %H:%M:%S %Y", tm);
	else
		mod_time[0] = '-';

	/* Get owner and group names */
	uid_t owner_id = file_attrib.st_uid; /* owner ID */
	gid_t group_id = file_attrib.st_gid; /* group ID */
	struct group *group;
	struct passwd *owner;
	group = getgrgid(group_id);
	owner = getpwuid(owner_id); 

	/* Print file properties for long view mode */
	if (_long) {

		/*	If filename length is greater than max, truncate it
		 * (max-1 = '~') to let the user know the filename isn't
		 * complete */
		 /* The value of max (global) is (or should be) already
		  * calculated by get_max_long_view() before calling this
		  * function */
		char trim_filename[NAME_MAX] = "";
		short trim = 0;
		if (filename_len > (size_t)max) {
			trim = 1;
			strcpy(trim_filename, filename);
			trim_filename[(max + cont_bt) - 1] = '~';
			trim_filename[max + cont_bt] = 0x00;
			filename_len = (max + cont_bt);
		}

		/* Calculate pad for each file */
		int pad;

		/* Long files are trimmed */
/*		if (longest > (size_t)max)
			pad = (max + cont_bt) - ((!trim) ? (int)filename_len
				  : (max + cont_bt));

		else */
			/* No trimmed files */
			pad = (int)(longest - (eln_len + 1 + filename_len));

		if (pad < 0)
			pad = 0;

		printf("%s%s%s%-*s%s (%04o) %c/%c%c%c/%c%c%c/%c%c%c%s %s %s %s %s\n", 
				(light_mode) ? "" : color, (!trim) ? filename
				: trim_filename, (light_mode) ? "" : NC,
				pad, "", default_color, file_attrib.st_mode & 07777,
				file_type,
				read_usr, write_usr, exec_usr, 
				read_grp, write_grp, exec_grp,
				read_others, write_others, (sticky) ? 't' : exec_others,
				is_acl(filename) ? "+" : "",
				(!owner) ? _("unknown") : owner->pw_name, 
				(!group) ? _("unknown") : group->gr_name,
				(size_type) ? size_type : "??", 
				(mod_time[0] != 0x00) ? mod_time : "??");
		if (linkname)
			free(linkname);

		if (size_type)
			free(size_type);

		return EXIT_SUCCESS;
	}

	/* Print file properties for normal mode */
	printf("(%04o)%c/%c%c%c/%c%c%c/%c%c%c%s %zu %s %s %s %s ", 
							file_attrib.st_mode & 07777, file_type, 
							read_usr, write_usr, exec_usr, read_grp, 
							write_grp, exec_grp, read_others, write_others, 
							(sticky) ? 't' : exec_others, 
							is_acl(filename) ? "+" : "", link_n, 
							(!owner) ? _("unknown") : owner->pw_name, 
							(!group) ? _("unknown") : group->gr_name, 
							(size_type) ? size_type : "??", 
							(mod_time[0] != 0x00) ? mod_time : "??");

	if (file_type && file_type != 'l')
		printf("%s%s%s%s\n", color, filename, NC, default_color);

	else if (linkname) {
		printf("%s%s%s%s -> %s\n", color, filename, NC, default_color, 
			   linkname);
		free(linkname);
	}

	else { /* Broken link */
		char link[PATH_MAX] = "";
		ssize_t ret = readlink(filename, link, PATH_MAX);
		if (ret) {
			printf("%s%s%s%s -> %s (broken link)\n", color, filename, NC, 
				   default_color, link);
		}
		else
			printf("%s%s%s%s -> ???\n", color, filename, NC,
				   default_color);
	}

	/* Stat information */

	/* Last access time */
	time = file_attrib.st_atim.tv_sec;
	tm = localtime(&time);
	char access_time[128] = "";
	if (time)
		/* Store formatted (and localized) date-time string into
		 * access_time */
		strftime(access_time, sizeof(access_time), "%b %d %H:%M:%S %Y", tm);
	else
		access_time[0] = '-';

	/* Last properties change time */
	time = file_attrib.st_ctim.tv_sec;
	tm = localtime(&time);
	char change_time[128] = "";
	if (time)
		strftime(change_time, sizeof(change_time), "%b %d %H:%M:%S %Y",
				 tm);
	else
		change_time[0] = '-';

	/* Get creation (birth) time */
	#if defined(HAVE_ST_BIRTHTIME) || defined(__BSD_VISIBLE)
		time = file_attrib.st_birthtime;
		tm = localtime(&time);
		char creation_time[128] = "";
		if (!time)
			creation_time[0] = '-';
		else
			strftime(creation_time, sizeof(creation_time),
					 "%b %d %H:%M:%S %Y", tm);
	#elif defined(_STATX)
		struct statx xfile_attrib;
		statx(AT_FDCWD, filename, AT_SYMLINK_NOFOLLOW, STATX_BTIME, 
			  &xfile_attrib);
		time = xfile_attrib.stx_btime.tv_sec;
		tm = localtime(&time);
		char creation_time[128] = "";
		if (!time)
			creation_time[0] = '-';
		else
			strftime(creation_time, sizeof(creation_time),
					 "%b %d %H:%M:%S %Y", tm);
	#endif

	switch (file_type) {
		case 'd': printf(_("Directory")); break;
		case 's': printf(_("Socket")); break;
		case 'l': printf(_("Symbolic link")); break;
		case 'b': printf(_("Block special file")); break;
		case 'c': printf(_("Character special file")); break;
		case 'p': printf(_("Fifo")); break;
		case '-': printf(_("Regular file")); break;
		default: break;
	}

	printf(_("\tBlocks: %ld"), file_attrib.st_blocks);
	printf(_("\tIO Block: %ld"), file_attrib.st_blksize);
	printf(_("\tInode: %zu\n"), file_attrib.st_ino);
	printf(_("Device: %zu"), file_attrib.st_dev);
	printf(_("\tUid: %u (%s)"), file_attrib.st_uid, (!owner)
			? _("unknown") : owner->pw_name);
	printf(_("\tGid: %u (%s)\n"), file_attrib.st_gid, (!group)
			? _("unknown") : group->gr_name);

	/* Print file timestamps */
	printf(_("Access: \t%s\n"), access_time);
	printf(_("Modify: \t%s\n"), mod_time);
	printf(_("Change: \t%s\n"), change_time);

	#if defined(HAVE_ST_BIRTHTIME) || defined(__BSD_VISIBLE) \
	|| defined(_STATX)
		printf(_("Birth: \t\t%s\n"), creation_time);
	#endif

	/* Print file size */
	if ((file_attrib.st_mode & S_IFMT) == S_IFDIR) {
		fputs(_("Total size: \t"), stdout);
		off_t total_size = dir_size(filename);
		if (total_size != -1) {
			char *human_size = get_size_unit(total_size);
			if (human_size) {
				printf("%s\n", human_size);
				free(human_size);
			}
			else
				puts("???");
		}
		else
			puts("???");
	}

	else
		printf(_("Size: \t\t%s\n"), (size_type) ? size_type : "??");

	if (size_type)
		free(size_type);

	return EXIT_SUCCESS;
}

int
hidden_function(char **comm)
{
	int exit_status = EXIT_SUCCESS;

	if (strcmp(comm[1], "status") == 0)
		printf(_("%s: Hidden files %s\n"), PROGRAM_NAME, 
			   (show_hidden) ? _("enabled") : _("disabled"));

	else if (strcmp(comm[1], "off") == 0) {
		if (show_hidden == 1) {
			show_hidden = 0;
			if (cd_lists_on_the_fly) {
				while (files--)
					free(dirlist[files]);
				exit_status = list_dir();
			}
		}
	}

	else if (strcmp(comm[1], "on") == 0) {
		if (show_hidden == 0) {
			show_hidden = 1;
			if (cd_lists_on_the_fly) {
				while (files--)
					free(dirlist[files]);
				exit_status = list_dir();
			}
		}
	}

	else
		fputs(_("Usage: hidden, hf [on, off, status]\n"), stderr);

	return exit_status;
}

int
log_function(char **comm)
/* Log 'comm' into LOG_FILE */
{
	/* If cmd logs are disabled, allow only "log" commands */
	if (!logs_enabled) {
		if (strcmp(comm[0], "log") != 0)
			return EXIT_SUCCESS;
	}

	if (!config_ok)
		return EXIT_FAILURE;

	int clear_log = 0;

	/* If the command was just 'log' */
	if (strcmp(comm[0], "log") == 0 && !comm[1]) {
		FILE *log_fp;
		log_fp = fopen(LOG_FILE, "r");
		if (!log_fp) {
			_err(0, NOPRINT_PROMPT, "%s: log: '%s': %s\n",
				 PROGRAM_NAME, LOG_FILE, strerror(errno));
			return EXIT_FAILURE;
		}
		else {
			size_t line_size = 0;
			char *line_buff = (char *)NULL;
			ssize_t line_len = 0;

			while ((line_len = getline(&line_buff, &line_size,
			log_fp)) > 0)
				fputs(line_buff, stdout);

			free(line_buff);

			fclose(log_fp);
			return EXIT_SUCCESS;
		}
	}

	else if (strcmp(comm[0], "log") == 0 && comm[1]) {
		if (strcmp(comm[1], "clear") == 0)
			clear_log = 1;
		else if (strcmp(comm[1], "status") == 0) {
			printf(_("Logs %s\n"), (logs_enabled) ? _("enabled")
				   : _("disabled"));
			return EXIT_SUCCESS;
		}
		else if (strcmp(comm[1], "on") == 0) {
			if (logs_enabled)
				puts(_("Logs already enabled"));
			else {
				logs_enabled = 1;
				puts(_("Logs successfully enabled"));
			}
			return EXIT_SUCCESS;
		}
		else if (strcmp(comm[1], "off") == 0) {
			/* If logs were already disabled, just exit. Otherwise, log
			 * the "log off" command */
			if (!logs_enabled) {
				puts(_("Logs already disabled"));
				return EXIT_SUCCESS;
			}
			else {
				puts(_("Logs succesfully disabled"));
				logs_enabled = 0;
			}
		}
	}

	/* Construct the log line */

	if (!last_cmd) {
		if (!logs_enabled) {
			/* When cmd logs are disabled, "log clear" and "log off" are 
			 * the only commands that can reach this code */
			if (clear_log) {
				last_cmd = (char *)xnmalloc(10, sizeof(char));
				strcpy(last_cmd, "log clear\0");
			}
			else {
				last_cmd = (char *)xnmalloc(8, sizeof(char));
				strcpy(last_cmd, "log off\0");
			}
		}
		/* last_cmd should never be NULL if logs are enabled (this
		 * variable is set immediately after taking valid user input
		 * in the prompt function). However ... */
		else {
			last_cmd = (char *)xnmalloc(23, sizeof(char));
			strcpy(last_cmd, "Error getting command!\0");
		}
	}

	char *date = get_date();
	size_t log_len = strlen(date) + strlen(path) + strlen(last_cmd) + 6;
	char *full_log = (char *)xnmalloc(log_len, sizeof(char));
	snprintf(full_log, log_len, "[%s] %s:%s\n", date, path, last_cmd);
	free(date);

	free(last_cmd);
	last_cmd = (char *)NULL;

	/* Write the log into LOG_FILE */
	FILE *log_fp;
	/* If not 'log clear', append the log to the existing logs */
	if (!clear_log)
		log_fp = fopen(LOG_FILE, "a");
	/* Else, overwrite the log file leaving only the 'log clear'
	 * command */
	else
		log_fp = fopen(LOG_FILE, "w+");

	if (!log_fp) {
		_err('e', PRINT_PROMPT, "%s: log: '%s': %s\n", PROGRAM_NAME,
			 LOG_FILE, strerror(errno));
		free(full_log);
		return EXIT_FAILURE;
	}
	else { /* If LOG_FILE was correctly opened, write the log */
		fputs(full_log, log_fp);
		free(full_log);
		fclose(log_fp);
		return EXIT_SUCCESS;
	}
}

void
check_log_file_size(char *log_file)
/* Keep only the last 'max_log' records in LOG_FILE */
{
	/* Create the file, if it doesn't exist */
	struct stat file_attrib;
	FILE *log_fp = (FILE *)NULL;
	if (stat(log_file, &file_attrib) == -1) {
		log_fp = fopen(log_file, "w");
		if (!log_fp) {
			_err(0, NOPRINT_PROMPT, "%s: '%s': %s\n", PROGRAM_NAME,
				 log_file, strerror(errno));
		}
		else
			fclose(log_fp);
		return; /* Return anyway, for, being a new empty file, there's
		no need to truncate it */
	}

	/* Truncate the file, if needed */
	log_fp = fopen (log_file, "r");
	if (log_fp) {
		int logs_num = 0, c;

		/* Count newline chars to get amount of lines in file */
		while ((c = fgetc(log_fp)) != EOF) {
			if (c == '\n')
				logs_num++;
		}

		if (logs_num > max_log) {
			/* Go back to the beginning of the log file */
			fseek(log_fp, 0, SEEK_SET);
			/* Create a temp file to store only newest logs */
			FILE *log_fp_tmp = fopen(LOG_FILE_TMP, "w+");
			if (!log_fp_tmp) {
				perror("log");
				fclose(log_fp);
				return;
			}
			int i = 1;
			size_t line_size = 0;
			char *line_buff = (char *)NULL;
			ssize_t line_len = 0;
			while ((line_len = getline(&line_buff, &line_size,
			log_fp)) > 0)
				/* Delete oldest entries */
				if (i++ >= logs_num - (max_log - 1))
					fprintf(log_fp_tmp, "%s", line_buff);
			free(line_buff);
			fclose(log_fp_tmp);
			fclose(log_fp);
			remove(log_file);
			rename(LOG_FILE_TMP, log_file);
		}
	}
	else
		_err(0, NOPRINT_PROMPT, "%s: log: %s: %s\n", PROGRAM_NAME,
			 log_file, strerror(errno));
}

int
get_history(void)
{
	if (!config_ok)
		return EXIT_FAILURE;

	if (current_hist_n == 0) /* Coming from main() */
		history = (char **)xcalloc(1, sizeof(char *));

	else { /* Only true when comming from 'history clear' */
		size_t i;
		for (i = 0; i < current_hist_n; i++)
			free(history[i]);
		history = (char **)xrealloc(history, 1 * sizeof(char *));
		current_hist_n = 0;
	}

	FILE *hist_fp = fopen(HIST_FILE, "r");

	if (!hist_fp) {
		_err('e', PRINT_PROMPT, "%s: history: '%s': %s\n",
			 PROGRAM_NAME, HIST_FILE,  strerror(errno));
		return EXIT_FAILURE;
	}

	size_t line_size = 0;
	char *line_buff = (char *)NULL;
	ssize_t line_len = 0;

	while ((line_len = getline(&line_buff, &line_size, hist_fp)) > 0) {
		line_buff[line_len - 1] = 0x00;
		history = (char **)xrealloc(history, (current_hist_n + 1)
									* sizeof(char *));
		history[current_hist_n] = (char *)xcalloc((size_t)line_len + 1, 
												  sizeof(char));
		strcpy(history[current_hist_n++], line_buff);
	}

	free(line_buff);
	fclose (hist_fp);

	return EXIT_SUCCESS;
}

int
history_function(char **comm)
{
	if (!config_ok) {
		fprintf(stderr, _("%s: History function disabled\n"),
				PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	/* If no arguments, print the history list */
	if (args_n == 0) {
		size_t i;
		for (i = 0; i < current_hist_n; i++)
			printf("%zu %s\n", i + 1, history[i]);
		return EXIT_SUCCESS;
	}

	/* If 'history clear', guess what, clear the history list! */
	if (args_n == 1 && strcmp(comm[1], "clear") == 0) {
		FILE *hist_fp = fopen(HIST_FILE, "w+");

		if (!hist_fp) {
			_err(0, NOPRINT_PROMPT, "%s: history: %s: %s\n",
				 PROGRAM_NAME, HIST_FILE, strerror(errno));
			return EXIT_FAILURE;
		}

		/* Do not create an empty file */
		fprintf(hist_fp, "%s %s\n", comm[0], comm[1]);
		fclose(hist_fp);

		/* Update the history array */
		int exit_status = EXIT_SUCCESS;
		if (get_history() != 0)
			exit_status = EXIT_FAILURE;
		if (log_function(comm) != 0)
			exit_code = EXIT_FAILURE;
		return exit_status;
	}

	/* If 'history -n', print the last -n elements */
	if (args_n == 1 && comm[1][0] == '-' && is_number(comm[1]+1)) {
		int num = atoi(comm[1] + 1);
		if (num < 0 || num > (int)current_hist_n) {
			num = (int)current_hist_n;
		}

		size_t i;
		for (i = current_hist_n - (size_t)num; i < current_hist_n; i++)
			printf("%zu %s\n", i + 1, history[i]);
		return EXIT_SUCCESS;
	}

	/* None of the above */
	puts(_("Usage: history [clear] [-n]"));

	return EXIT_SUCCESS;
}

int
run_history_cmd(const char *cmd)
/* Takes as argument the history cmd less the first exclamation mark. 
 * Example: if exec_cmd() gets "!-10" it pass to this function "-10",
 * that is, comm + 1 */
{
	/* If "!n" */
	int exit_status = EXIT_SUCCESS;
	size_t i;

	if (is_number(cmd)) {
		int num = atoi(cmd);
		if (num > 0 && num < (int)current_hist_n) {
			size_t old_args = args_n;
			if (record_cmd(history[num - 1]))
				add_to_cmdhist(history[num - 1]);
			char **cmd_hist = parse_input_str(history[num - 1]);
			if (cmd_hist) {

				char **alias_cmd = check_for_alias(cmd_hist);
				if (alias_cmd) {
					/* If an alias is found, the function frees cmd_hist
					 * and returns alias_cmd in its place to be executed
					 * by exec_cmd() */
					if (exec_cmd(alias_cmd) != 0)
						exit_status = EXIT_FAILURE;
					for (i = 0; alias_cmd[i]; i++)
						free(alias_cmd[i]);
					free(alias_cmd);
					alias_cmd = (char **)NULL;
				}

				else {
					if (exec_cmd(cmd_hist) != 0)
						exit_status = EXIT_FAILURE;
					for (i = 0; cmd_hist[i]; i++)
						free(cmd_hist[i]);
					free(cmd_hist);
				}

				args_n = old_args;

				return exit_status;
			}
			fprintf(stderr, _("%s: Error parsing history command\n"), 
					PROGRAM_NAME);
			return EXIT_FAILURE;
		}
		else
			fprintf(stderr, _("%s: !%d: event not found\n"), 
					PROGRAM_NAME, num);
		return EXIT_FAILURE;
	}

	/* If "!!", execute the last command */
	else if (strcmp(cmd, "!") == 0) {
		size_t old_args = args_n;
		if (record_cmd(history[current_hist_n - 1]))
			add_to_cmdhist(history[current_hist_n - 1]);
		char **cmd_hist = parse_input_str(history[current_hist_n - 1]);
		if (cmd_hist) {

			char **alias_cmd = check_for_alias(cmd_hist);
			if (alias_cmd) {
				if (exec_cmd(alias_cmd) != 0)
					exit_status = EXIT_FAILURE;
				for (i = 0; alias_cmd[i]; i++)
					free(alias_cmd[i]);
				free(alias_cmd);
				alias_cmd = (char **)NULL;
			}

			else {
				if (exec_cmd(cmd_hist) != 0)
					exit_status = EXIT_FAILURE;
				for (i = 0; cmd_hist[i]; i++)
					free(cmd_hist[i]);
				free(cmd_hist);
			}

			args_n = old_args;
			return exit_status;
		}
		fprintf(stderr, _("%s: Error parsing history command\n"), 
				PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	/* If "!-n" */
	else if (cmd[0] == '-') {
		/* If not number or zero or bigger than max... */
		int acmd = atoi(cmd + 1);
		if (!is_number(cmd + 1) || acmd == 0 
		|| acmd > (int)current_hist_n - 1) {
			fprintf(stderr, _("%s: !%s: event not found\n"), 
					PROGRAM_NAME, cmd);
			return EXIT_FAILURE;
		}
		size_t old_args = args_n;
		char **cmd_hist = parse_input_str(history[current_hist_n
										  - (size_t)acmd - 1]);
		if (cmd_hist) {

			char **alias_cmd = check_for_alias(cmd_hist);
			if (alias_cmd) {
				if (exec_cmd(alias_cmd) != 0)
					exit_status = EXIT_FAILURE;
				for (i = 0; alias_cmd[i]; i++)
					free(alias_cmd[i]);
				free(alias_cmd);
				alias_cmd = (char **)NULL;
			}

			else {
				if (exec_cmd(cmd_hist) != 0)
					exit_status = EXIT_FAILURE;
				for (i = 0; cmd_hist[i]; i++)
					free(cmd_hist[i]);
				free(cmd_hist);
			}

			if (record_cmd(history[current_hist_n - (size_t)acmd - 1]))
				add_to_cmdhist(history[current_hist_n - (size_t)acmd - 1]);

			args_n = old_args;
			return exit_status;
		}

		if (record_cmd(history[current_hist_n - (size_t)acmd - 1]))
			add_to_cmdhist(history[current_hist_n - (size_t)acmd - 1]);

		fprintf(stderr, _("%s: Error parsing history command\n"), 
				PROGRAM_NAME);
		return EXIT_FAILURE;
	}
	else {
		printf(_("Usage:\n\
!!: Execute the last command.\n\
!n: Execute the command number 'n' in the history list.\n\
!-n: Execute the last-n command in the history list.\n"));
		return EXIT_SUCCESS;
	}
}

int
edit_function (char **comm)
/* Edit the config file, either via my mime function or via the first
 * passed argument (Ex: 'edit nano') */
{
	if (!config_ok) {
		fprintf(stderr, _("%s: Cannot access the configuration file\n"), 
				PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	/* Get modification time of the config file before opening it */
	struct stat file_attrib;

	/* If, for some reason (like someone erasing the file while the
	 * program is running) clifmrc doesn't exist, call init_config()
	 * to recreate the configuration file. Then run 'stat' again to
	 * reread the attributes of the file */ 
	if (stat(CONFIG_FILE, &file_attrib) == -1) {
		free(CONFIG_DIR);
		free(TRASH_DIR);
		free(TRASH_FILES_DIR);
		free(TRASH_INFO_DIR);
		CONFIG_DIR = TRASH_DIR = TRASH_FILES_DIR = (char *)NULL;
		TRASH_INFO_DIR = (char *)NULL;

		free(BM_FILE);
		free(LOG_FILE);
		free(LOG_FILE_TMP);
		free(HIST_FILE);
		BM_FILE = LOG_FILE = LOG_FILE_TMP = HIST_FILE = (char *)NULL;

		free(CONFIG_FILE);
		free(PROFILE_FILE);
		free(MSG_LOG_FILE);
		free(sel_file_user);
		CONFIG_FILE = PROFILE_FILE = MSG_LOG_FILE = (char *)NULL;
		sel_file_user = (char *)NULL;

		free(MIME_FILE);
		MIME_FILE = (char *)NULL;

		if (encoded_prompt) {
			free(encoded_prompt);
			encoded_prompt = (char *)NULL;
		}

		if (sys_shell) {
			free(sys_shell);
			sys_shell = (char *)NULL;
		}

		if (term) {
			free(term);
			term = (char *)NULL;
		}

		if (opener) {
			free(opener);
			opener = (char *)NULL;
		}

		free(TMP_DIR);
		TMP_DIR = (char *)NULL;

		/* Rerun external_arguments */
		if (argc_bk > 1)
			external_arguments(argc_bk, argv_bk);
/*		initialize_readline(); */
		init_config();
		stat(CONFIG_FILE, &file_attrib);
	}

	time_t mtime_bfr = file_attrib.st_mtime;

	if (comm[1]) { /* If there is an argument... */
		char *cmd[] = { comm[1], CONFIG_FILE, NULL };
		int ret = launch_execve(cmd, FOREGROUND);
		if (ret != EXIT_SUCCESS)
			return EXIT_FAILURE;
	}

	/* If no application has been passed as 2nd argument */
	else {
		if (!(flags & FILE_CMD_OK)) {
			fprintf(stderr, _("%s: 'file' command not found. Try "
					"'edit APPLICATION'\n"), PROGRAM_NAME);
			return EXIT_FAILURE;
		}
		else {
			char *cmd[] = { "mime", CONFIG_FILE, NULL };
			int ret = mime_open(cmd);
			if (ret != 0)
				return EXIT_FAILURE;
		}
	}

	/* Get modification time after opening the config file */
	stat(CONFIG_FILE, &file_attrib);
	/* If modification times differ, the file was modified after being 
	 * opened */
	if (mtime_bfr != file_attrib.st_mtime) {
		/* Reload configuration only if the config file was modified */

		free(CONFIG_DIR);
		free(TRASH_DIR);
		free(TRASH_FILES_DIR);
		free(TRASH_INFO_DIR);
		CONFIG_DIR = TRASH_DIR = TRASH_FILES_DIR = (char *)NULL;
		TRASH_INFO_DIR = (char *)NULL;

		free(BM_FILE);
		free(LOG_FILE);
		free(LOG_FILE_TMP);
		free(HIST_FILE);
		BM_FILE = LOG_FILE = LOG_FILE_TMP = HIST_FILE = (char *)NULL;

		free(CONFIG_FILE);
		free(PROFILE_FILE);
		free(MSG_LOG_FILE);
		CONFIG_FILE = PROFILE_FILE = MSG_LOG_FILE = (char *)NULL;

		free(MIME_FILE);
		free(sel_file_user);
		MIME_FILE = sel_file_user = (char *)NULL;


		if (encoded_prompt) {
			free(encoded_prompt);
			encoded_prompt = (char *)NULL;
		}

		if (sys_shell) {
			free(sys_shell);
			sys_shell = (char *)NULL;
		}

		if (term) {
			free(term);
			term = (char *)NULL;
		}

		if (opener) {
			free(opener);
			opener = (char *)NULL;
		}

		free(TMP_DIR);
		TMP_DIR = (char *)NULL;

		if (argc_bk > 1)
			external_arguments(argc_bk, argv_bk);

		init_config();

		/* Free the aliases and prompt_cmds arrays to be allocated again */
		size_t i = 0;
		for (i = 0; i < aliases_n; i++)
			free(aliases[i]);
		for (i = 0; i < prompt_cmds_n; i++)
			free(prompt_cmds[i]);
		aliases_n = prompt_cmds_n = 0; /* Reset counters */
		get_aliases();
		get_prompt_cmds();

		if (cd_lists_on_the_fly) {
			while (files--) free(dirlist[files]);
			list_dir();
		}
	}

	return EXIT_SUCCESS;
}

void
color_codes (void)
/* List color codes for file types used by the program */
{
	if (light_mode) {
		printf("%s: Currently running in light mode (no colors)\n",
			   PROGRAM_NAME);
		return;
	}

	printf(_("%s file name%s%s: Directory with no read permission (nd)\n"), 
		   nd_c, NC, default_color);
	printf(_("%s file name%s%s: File with no read permission (nf)\n"), 
		   nf_c, NC, default_color);
	printf(_("%s file name%s%s: Directory* (di)\n"), di_c, NC,
		   default_color);
	printf(_("%s file name%s%s: EMPTY directory (ed)\n"), ed_c, NC, 
		   default_color);
	printf(_("%s file name%s%s: EMPTY directory with no read "
			 "permission (ne)\n"), ne_c, NC, default_color);
	printf(_("%s file name%s%s: Executable file (ex)\n"), ex_c, NC, 
		   default_color);
	printf(_("%s file name%s%s: Empty executable file (ee)\n"), ee_c, NC, 
		   default_color);
	printf(_("%s file name%s%s: Block special file (bd)\n"), bd_c, NC, 
		   default_color);
	printf(_("%s file name%s%s: Symbolic link* (ln)\n"), ln_c, NC, 
		   default_color);
	printf(_("%s file name%s%s: Broken symbolic link (or)\n"), or_c, NC, 
		   default_color);
	printf(_("%s file name%s%s: Socket file (so)\n"), so_c, NC, 
		   default_color);
	printf(_("%s file name%s%s: Pipe or FIFO special file (pi)\n"), pi_c,
		   NC, default_color);
	printf(_("%s file name%s%s: Character special file (cd)\n"), cd_c, NC, 
		   default_color);
	printf(_("%s file name%s%s: Regular file (fi)\n"), fi_c, NC, 
		   default_color);
	printf(_("%s file name%s%s: Empty (zero-lenght) file (ef)\n"), ef_c,
		   NC, default_color);
	printf(_(" %s%sfile name%s%s: SUID file (su)\n"), NC, su_c, NC, 
		   default_color);
	printf(_(" %s%sfile name%s%s: SGID file (sg)\n"), NC, sg_c, NC, 
		   default_color);
	printf(_(" %s%sfile name%s%s: File with capabilities (ca)\n"), NC, ca_c, 
		   NC, default_color);
	printf(_(" %s%sfile name%s%s: Sticky and NOT other-writable "
			 "directory* (st)\n"),  NC, st_c, NC, default_color);
	printf(_(" %s%sfile name%s%s: Sticky and other-writable "
			 "directory* (tw)\n"),  NC, tw_c, NC, default_color);
	printf(_(" %s%sfile name%s%s: Other-writable and NOT sticky "
			 "directory* (ow)\n"),  NC, ow_c, NC, default_color);
	printf(_(" %s%sfile name%s%s: Unknown file type (no)\n"), NC, no_c, 
		   NC, default_color);
	printf(_("\n*The slash followed by a number (/xx) after directories "
			 "or symbolic links to directories indicates the amount of "
			 "files contained by the corresponding directory, excluding "
			 "self (.) and parent (..) directories.\n"));
	printf(_("\nThe value in parentheses is the code that must be used "
			 "modify the color of the corresponding filetype in the "
			 "configuration file (in the \"FiletypeColors\" line), "
			 "using the same ANSI style color format used by dircolors. "
			 "By default, %s uses only 8 colors, but you can use 256 and "
			 "RGB colors as well.\n\n"), PROGRAM_NAME);
}

int
list_commands (void)
/* Instead of recreating here the commands description, just jump to the
 * corresponding section in the manpage */
{
	char *cmd[] = { "man", "-P", "less -p ^COMMANDS", PNL, NULL };
	if (launch_execve(cmd, FOREGROUND) != EXIT_SUCCESS)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

void
help_function (void)
{
/*	if (clear_screen)
		CLEAR; */

	printf(_("%s %s (%s), by %s\n"), PROGRAM_NAME, VERSION, DATE, AUTHOR);

	printf(_("\nUSAGE: %s [-aAfFgGhiIlLoOsuUvx] [-p PATH] [-P PROFILE]\n\
\n -a, --no-hidden\t\t do not show hidden files\
\n -A, --show-hidden\t\t show hidden files (default)\
\n -f, --no-folders-first\t\t do not list folders first\
\n -F, --folders-first\t\t list folders first (default)\
\n -g, --pager\t\t\t enable the pager\
\n -G, --no-pager\t\t\t disable the pager (default)\
\n -h, --help\t\t\t show this help and exit\
\n -i, --no-case-sensitive\t no case-sensitive files listing (default)\
\n -I, --case-sensitive\t\t case-sensitive files listing\
\n -l, --no-long-view\t\t disable long view mode (default)\
\n -L, --long-view\t\t enable long view mode\
\n -o, --no-list-on-the-fly\t 'cd' works as the shell 'cd' command\
\n -O, --list-on-the-fly\t\t 'cd' lists files on the fly (default)\
\n -p, --path PATH\t\t use PATH as %s starting path\
\n -P, --profile PROFILE\t\t use (or create) PROFILE as profile\
\n -s, --splash \t\t\t enable the splash screen\
\n -u, --no-unicode \t\t disable unicode\
\n -U, --unicode \t\t\t enable unicode to correctly list filenames \
containing accents, tildes, umlauts, non-latin letters, etc. This option \
is enabled by default for non-english locales\
\n -v, --version\t\t\t show version details and exit\
\n -x, --ext-cmds\t\t\t allow the use of external commands\
\n -y, --light-mode\t\t enable the light mode\
\n -z, --sort METHOD\t\t sort files by METHOD, where METHOD could \
be: 0 = none, 1 = name, 2 = size, 3 = atime, \
4 = btime, 5 = ctime, 6 = mtime, 7 = version\n"), PNL, 
		PROGRAM_NAME);

	puts(_("\nBUILT-IN COMMANDS:\n\n\
 ELN/FILE/DIR (auto-open and autocd)\n\
 /* [DIR]\n\
 bm, bookmarks [a, add PATH] [d, del] [edit] [SHORTCUT or NAME]\n\
 o, open [ELN/FILE] [APPLICATION]\n\
 cd [ELN/DIR]\n\
 s, sel [ELN ELN-ELN FILE ... n]\n\
 sb, selbox\n\
 ds, desel [*, a, all]\n\
 t, tr, trash [ELN/FILE ... n] [ls, list] [clear] [del, rm]\n\
 u, undel, untrash [*, a, all]\n\
 b, back [h, hist] [clear] [!ELN]\n\
 f, forth [h, hist] [clear] [!ELN]\n\
 c, l, m, md, r\n\
 p, pr, prop [ELN/FILE ... n] [s, size] [a, all]\n\
 mm, mime [info ELN/FILE] [edit]\n\
 ;[CMD], :[CMD]\n\
 mp, mountpoints\n\
 v, paste [sel] [DESTINY]\n\
 pf, prof, profile [ls, list] [set, add, del PROFILE]\n\
 br, bulk ELN/FILE ...\n\
 ac, ad ELN/FILE ...\n\
 shell [SHELL]\n\
 st, sort [METHOD] [rev]\n\
 opener [default] [APPLICATION]\n\
 msg, messages [clear]\n\
 log [clear]\n\
 history [clear] [-n]\n\
 edit [APPLICATION]\n\
 alias [import FILE]\n\
 splash\n\
 tips\n\
 path, cwd\n\
 cmd, commands\n\
 lm [on, off]\n\
 rf, refresh\n\
 cc, colors\n\
 acd, autocd [on, off, status]\n\
 ao, auto-open [on, off, status]\n\
 hf, hidden [on, off, status]\n\
 ff, folders first [on, off, status]\n\
 fc, filescounter [on, off, status]\n\
 pg, pager [on, off, status]\n\
 uc, unicode [on, off, status]\n\
 ext [on, off, status]\n\
 ver, version\n\
 fs\n\
 q, quit, exit, zz\n"));

	puts(_("Run 'cmd' or consult the manpage for more information about "
		   "each of these commands.\n"));

	puts(_("KEYBOARD SHORTCUTS:\n\n\
 A-c:	Clear the current command line buffer\n\
 A-f:	Toggle list-folders-first on/off\n\
 C-r:	Refresh the screen\n\
 A-l:	Toggle long view mode on/off\n\
 A-m:	Open the mountpoints screen\n\
 A-b:	Launch the Bookmarks Manager\n\
 A-i:	Toggle hidden files on/off\n\
 A-s:	Print currently selected files\n\
 A-a:	Select all files in the current working directory\n\
 A-d:	Deselect all selected files\n\
 A-r:	Change to the root directory\n\
 A-e:	Change to the home directory\n\
 A-u:	Change to the parent directory\n\
 A-j:	Change to the previous directory in the directory history \
list\n\
 A-k:	Change to the next directory in the directory history list\n\
 A-y:	Toggle light mode on/off\n\
 A-z:	Change to previous sorting method\n\
 A-x:	Change to next sorting method\n\
 F10:	Open the configuration file\n\n\
NOTE: Depending on the terminal emulator being used, some of these \
keybindings, like A-e, A-f, and F10, might conflict with some of the \
terminal keybindings.\n"));

	puts(_("Color codes: Run the 'colors' or 'cc' command to see the list "
		   "of currently used color codes.\n"));

	puts(_("The configuration and profile files allow you to customize "
		   "colors, define some prompt commands and aliases, and more. "
		   "For a full description consult the manpage."));
}

void
free_software (void)
{
	puts(_("Excerpt from 'What is Free Software?', by Richard Stallman. \
Source: https://www.gnu.org/philosophy/free-sw.html\n \
\n\"'Free software' means software that respects users' freedom and \
community. Roughly, it means that the users have the freedom to run, \
copy, distribute, study, change and improve the software. Thus, 'free \
software' is a matter of liberty, not price. To understand the concept, \
you should think of 'free' as in 'free speech', not as in 'free beer'. \
We sometimes call it 'libre software', borrowing the French or Spanish \
word for 'free' as in freedom, to show we do not mean the software is \
gratis.\n\
\nWe campaign for these freedoms because everyone deserves them. With \
these freedoms, the users (both individually and collectively) control \
the program and what it does for them. When users don't control the \
program, we call it a 'nonfree' or proprietary program. The nonfree \
program controls the users, and the developer controls the program; \
this makes the program an instrument of unjust power. \n\
\nA program is free software if the program's users have the four \
essential freedoms:\n\n\
- The freedom to run the program as you wish, for any purpose \
(freedom 0).\n\
- The freedom to study how the program works, and change it so it does \
your computing as you wish (freedom 1). Access to the source code is a \
precondition for this.\n\
- The freedom to redistribute copies so you can help your neighbor \
(freedom 2).\n\
- The freedom to distribute copies of your modified versions to others \
(freedom 3). By doing this you can give the whole community a chance to \
benefit from your changes. Access to the source code is a precondition \
for this. \n\
\nA program is free software if it gives users adequately all of these \
freedoms. Otherwise, it is nonfree. While we can distinguish various \
nonfree distribution schemes in terms of how far they fall short of \
being free, we consider them all equally unethical (...)\""));
}

void
version_function(void)
{
	printf(_("%s %s (%s), by %s\nContact: %s\nWebsite: "
		   "%s\nLicense: %s\n"), PROGRAM_NAME, VERSION, DATE,
		   AUTHOR, CONTACT, WEBSITE, LICENSE);
}

void
splash(void)
{
	printf("\n%s                         xux\n"
	"       :xuiiiinu:.......u@@@u........:xunninnu;\n"
	"    .xi#@@@@@@@@@n......x@@@l.......x#@@@@@@@@@:...........:;unnnu;\n"
	"  .:i@@@@lnx;x#@@i.......l@@@u.....x#@@lu;:;;..;;nnll#llnnl#@@@@@@#u.\n"
	"  .i@@@i:......::........;#@@#:....i@@@x......;@@@@@@@@@@@@@#iuul@@@n.\n"
	"  ;@@@#:..........:nin:...n@@@n....n@@@nunlll;;@@@@i;:xl@@@l:...:l@@@u.\n"
	"  ;#@@l...........x@@@l...;@@@#:...u@@@@@@@@@n:i@@@n....i@@@n....;#@@#;.\n"
	"  .l@@@;...........l@@@x...i@@@u...x@@@@iux;:..;#@@@x...:#@@@;....n@@@l.\n"
	"  .i@@@x...........u@@@i...;@@@l....l@@@;.......u@@@#:...;nin:.....l@@@u.\n"
	"  .n@@@i:..........:l@@@n...xnnx....u@@@i........i@@@i.............x@@@#:\n"
	"   :l@@@i...........:#@@@;..........:@@@@x.......:l@@@u.............n@@@n.\n"
	"    :l@@@i;.......unni@@@#:.:xnlli;..;@@@#:.......:l@@u.............:#@@n.\n"
	"     ;l@@@@#lnuxxi@@@i#@@@##@@@@@#;...xlln.         :.                ;:.\n"
	"      :xil@@@@@@@@@@l:u@@@@##lnx;.\n"
	"         .:xuuuunnu;...;ux;.", d_cyan);

	printf(_("\n\t\t   %sThe anti-eye-candy/KISS file manager\n%s"),
		   white, NC);

	if (splash_screen) {
		printf(_("\n\t\t\tPress any key to continue... "));
		xgetchar(); puts("");
	}
	else
		puts("");
}

void
bonus_function(void)
{
	char *phrases[] = {
		"\"Vamos Boca Juniors Carajo!\" (La mitad + 1)",
		"\"Hey! Look behind you! A three-headed monkey! (G. Threepweed)",
		"\"Free as in free speech, not as in free beer\" (R. M. S)",
		"\"Nothing great has been made in the world without passion\" (G. W. F. Hegel)",
		"\"Simplicity is the ultimate sophistication (Leo Da Vinci)",
		"\"Yo vendí semillas de alambre de púa, al contado, y me lo agradecieron\" (Marquitos, 9 Reinas)",
		"\"I'm so happy, because today I've found my friends, they're in my head\" (K. D. Cobain)",
		"\"The best code is written with the delete key (Someone, somewhere, sometime)",
		"\"I'm selling these fine leather jackets (Indy)",
		"\"I pray to God to make me free of God\" (Meister Eckhart)",
		"¡Truco y quiero retruco mierda!",
		"The only truth is that there is no truth",
		"\"This is a lie\" (The liar paradox)",
		"\"There are two ways to write error-free programs; only the third one works\" (Alan J. Perlis)",
		"The man who sold the world was later sold by the big G",
		"A programmer is always one year older than herself",
		"A smartphone is anything but smart",
		"And he did it: he killed the one who killed him",
		">++('>",
		":(){:|:&};:",
		"Keep it simple, stupid",
		NULL
		};

	size_t num = (sizeof(phrases) / sizeof(phrases[0])) - 1;

	srand(time(NULL));
	printf("%s\n", phrases[rand() % num]);
}
