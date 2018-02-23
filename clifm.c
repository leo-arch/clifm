//kliss(fm) (cli+kiss+bliss)?

/*NOTE:
Though this program was never intended to be a shell, it might be easily used as such:
1 - Add the full path to ClifM to /etc/shells.
2 - Change the user's default shell:
	$ chsh -s /path/to/clifm
3 - Exit and login again.
* You can also do this for the root user.
* However, it cannot be a true shell insofar as it depends, for running commands, on the shell
* pointed by /bin/sh (in my case Bash). To avoid this I should replace all calls to system()
* by the exec() family of functions.
* A shell is "a command-line interpreter that reads user input and execute commands." (Stevens, 
* R., Advanced Programming in the Unix Environment, Adison-West, 2013 (3rd ed.), p. 3)
* On how to write a basic shell see:
* http://linuxgazette.net/111/ramankutty.html
* https://brennan.io/2015/01/16/write-a-shell-in-c/
* http://users.cs.cf.ac.uk/Dave.Marshall/C/node22.html
* Something a bit more complex (with job control) could be found in the GNU site: 
* https://www.gnu.org/software/libc/manual/html_node/Implementing-a-Shell.html

* On a comparison of the features of different Linux shells see: 
* https://www.packtpub.com/books/content/linux-shell-scripting
*/

/*To see the complete list of predefined macros run:
$ gcc -E -dM somefile.c 
or: $ echo "" | gcc -E -dM -c -
of course you can grep it to find, say, linux' macros, as here.*/

//#define _POSIX_C_SOURCE 200809L 
/*"if you define _GNU_SOURCE, then defining either _POSIX_SOURCE or _POSIX_C_SOURCE as well 
has no effect". If I define this macro without defining _GNU_SOURCE macro as well, the macros
'S_IFDIR' and the others couldn't be used.*/
#define _GNU_SOURCE //I only need this macro to enable program_invocation_short_name variable
#define PROGRAM_NAME "CliFM"
#ifndef __linux__ //&& !defined(linux) && !defined(__linux) && !defined(__gnu_linux__)
//#ifndef __linux__
	//while 'linux' is deprecated, '__linux__' is recommended
	fprintf(stderr, "%s: This program runs on Linux only\n", PROGRAM_NAME); //Get a real OS man!
	exit(EXIT_FAILURE);
#endif
#ifndef PATH_MAX
	#define PATH_MAX 4096
#endif
#ifndef HOST_NAME_MAX
	#define HOST_NAME_MAX 64
#endif

/* Taken from procps-ng c.h
#ifndef TRUE
# define TRUE 1
#endif

#ifndef FALSE
# define FALSE 0
#endif
*/

#include <stdio.h> //(f)printf, s(n)printf, scanf, fopen, fclose, remove, fgetc, fputc, perror,
				   //rename, sscanf
#include <string.h> //str(n)cpy, str(n)cat, str(n)cmp, strstr, memset
#include <stdlib.h> //getenv, malloc, calloc, free, atoi, realpath, EXIT_FAILURE and 
					//EXIT_SUCCESS macros
#include <dirent.h> //scandir
#include <unistd.h> //sleep, readlink, chdir, symlink, access, exec, isatty, setpgid, getpid
					//gethostname, tcsetpgrp, tcgetattr
#include <sys/stat.h> //stat, lstat, mkdir
//#include <sys/types.h>
#include <sys/wait.h> //waitpid, wait
#include <sys/ioctl.h> //ioctl
#include <time.h> //localtime, strftime, clock (to time functions or commands)
#include <grp.h> //getgrgid
#include <signal.h> //trap signals
#include <ctype.h> //isspace, isdigit, tolower
#include <pwd.h> //getcwd, getpid, geteuid, getpwuid
#include <linux/limits.h> //PATH_MAX, NAME_MAX
#include <readline/history.h> //for commands history: add_history(buf);
#include <readline/readline.h> //readline
#include <glob.h> //glob
#include <termios.h> //struct termios
#include <locale.h> //setlocale
#include <errno.h>
#include <sys/capability.h> //cap_get_file
#include <getopt.h> //getopt_long
//#include <bsd/string.h> //strlcpy, strlcat
//#include "clifm.h"

#define DEFAULT_PATH getpwuid(getuid())->pw_dir //get user's home
#define CONF_PATHS_MAX 128
#define MAX_LINE 256
#define STDERR_FILE "/tmp/clifm/.clifm_cmd_stderr"
#define TMP_DIR "/tmp/clifm"
#define CLEAR printf("\033c")
#define COMM_FILE "/tmp/clifm/.clifm_comm"
#define VERSION "0.9.40.5 (fm/shell version)"
#define AUTHOR "L. Abramovich"
#define CONTACT "johndoe.arch@outlook.com"
#define DATE "Jan 3, 2018"
#define WELCOME_MSG "Welcome to CLiFM, the anti-eye-candy/KISS file manager!"

//Define flags for program options and internal use
static int flags;
//options flags
#define BACKUP_OK 		(1 << 1)
#define COMM 			(1 << 2)
#define FOLDERS_FIRST 	(1 << 3)
#define HELP			(1 << 4)
#define HIDDEN 			(1 << 5)
#define ON_THE_FLY 		(1 << 6)
#define SPLASH 			(1 << 7)
#define CASE_SENS 		(1 << 8)
#define START_PATH 		(1 << 9)
#define PRINT_VERSION	(1 << 10)
//internal flags
#define IS_PIPE			(1 << 11)
#define STREAM_REDIR	(1 << 12)
#define CONC_CMD		(1 << 13)
#define ROOT_USR		(1 << 14)
#define EXT_HELP		(1 << 15)
#define XDG_OPEN_OK		(1 << 16)
#define WINDOWED		(1 << 17)
#define IS_USRVAR_DEF	(1 << 18)

/*readline: this function allows the user to move backward and forward with the arrow keys in 
 * the prompt. I've tried scanf, getchar, getline, fscanf, fgets, and none of them does the 
 * trick. Besides, readline provides TAB completion and history. Btw, readline is what Bash 
 * uses for its prompt.

 * To compile the program link first the readline library with the lreadline flag: 
 * $ gcc -g -O2 -mtune=native -Wall -Wextra -pedantic -fstack-protector -lreadline -o clifm clifm.c
 * Note: -g is only for debugging (it adds debugging symbols to the code making the executable 
 * thus 2 o 3 times bigger). When debugging it's no necessary anymore; simply remove this flag.
 * Note2: I guess that the final compilation should not include warning and error flags at all,
 * but only optimization flags (ex: -O2 and -mtune=native), plus lreadline and lcap libraries.
*/

/* NOTE: I've tried pthread to run some procedures in parallel, which is supposed to boost the
program speed. However, the serialized (non-threaded) version was always faster.*/

//###TODO LIST###
/*
 ** The glob function can do brace expansion!!
 ** When logging a command, how to know whether an argument, which is a number, corresponds
 ++ to an ELN or is just a number (Ex: 'echo 0 > /tmp/updates' or 'chmod 755 some_file')?
 ** When refreshing the screen, there's no need to reload the whole array of elements. Instead,
 ++ I should simply print those elements again, wihtout wasting resources in reloading them as 
 ++ if the array were empty. Perhaps I could use some flag to do this.
 ** Type "slaby", press Enter, and bang!, the program crashes. 
 ** When creating a variable, make sure the new variable name doesn't exist. If it does, 
 ++ replace the old position of the variable in the array of user variables by the new one in
 ++ order not to unnecessarily take more memory for it.
 ** Add an unset function to remove user defined variables. The entire array of user variables
 ++ should be reordered every time a vairable is unset.
 ** Add "conditional execution". Ex: 'a && b' will execute 'b' only if 'a' was succesfull. On 
 ++ the contrary, 'a || b' will execute 'b' only if 'a' was NOT succesfull. To know whether a 
 ++ command (and not the exec function that executed it) was succesfull, make use of the 
 ++ WIFEXITED and WIFEXITSTATUS macros (with waitpid()). I think I should redefine exec_cmd() 
 ++ to make it return some exit value (say, EXIT_SUCCESS or EXIT_FAILURE), which implies to 
 ++ redefine all of the functions called by exec_cmd to do the same thing (or not: perhaps I 
 ++ could simply return a zero when it comes to internal commands). In case of an external
 ++ command, exec_cmds should return the value WIFEXITSTATUS. See test.c
 ** Replace execve by execvp: execvp, unlike execve, doesn't requiere the first argument to be 
 ++ the full command path, since the function itself checks the PATH environment variable.
 ** Add an explanation of the history command (!) to help.
 ** Better format the output of 'pr size' and 'pr all'.
 ** Use flags for sys_shell, sys_shell_status, move_mark, and search_mark variables.
 ** Replace all the beginning of parse_pipes() by get_substr().
 ** Replace "CLiFM" by PROGRAM_NAME
 ** When piping, redirecting a stream or concatenating commands, aliases are not read.
 ** The __func__ variable returns the name of the current function. It could be useful to 
 ++ compose the error logs.
 ** Try to emulate bash i/o redirection (ex: ls &>/dev/null). See: http://www.tldp.org/LDP/abs/html/io-redirection.html
 ** At the end of prompt() I have 7 strcmp. Try to reduce all this to just one operation.
 ** THE STACK (FIXED ARRAYS) IS MUCH MORE FASTER THAN THE HEAP (DYNAMIC ARRAYS)!! If a variable
 ++ is only localy used, allocate it in the stack.
 ** Log errors!! Perhaps I should write a function different from log_function (which only log
 ** commands) named something like log_error();
 ** Take a look at the capabilities() function.
 ** Add some keybinds, for instance, C-c to paste (see test3.c). Other posibilities: folders 
 ++ first, case sensitive listing, hidden files, etc.
 ** If rm makes a question regarding the file to be removed, this question won't be seen on the
 ++ screen (I guess the same happens with cp and mv, at least). The problem is check_cmd_stderr().

 ** Possible null pointer dereference (path_tmp) in get_path_env(). Same in prompt() with 
 ++ path_tilde and short_path (using cppcheck).

 ** When the string is initialized as NULL, there's no need for a calloc before the 
 ++ reallocation.
 
 ** It seems that strlcpy (and family) is safer than strncpy (and family). On these functions
 ++ see its manpage and https://www.sudo.ws/todd/papers/strlcpy.html. The main difference seems
 ++ to be that whereas strncpy zero-fills the entire length of the destination, strlcpy only
 ++ adds a single null char at the end of the string copied into destination, which implies a 
 ++ performance difference.
 ++ NOTE: to use these functions: 1 - include the header bsd/string.h and 2 - link the bsd 
 ++ library at compile time as follows: $ gcc -lbsd ...
 ** PIPES added and working fine; but still cannot pipe INTERNAL commands 
 ++ (ex: pr 12 | grep size). In fact, the program chashes when trying to do it!!!
 ** It could be a good idea to add a puts call in every function to track when, and how many 
 ++ times functions are called.
 ** Find a way to conveniently handle realloc and calloc errors (if possible, not exiting the
 ++ program)

 ** Take a look at the output of cppcheck --enable=all clifm_test7.c
 **	Add support for wildcards and nested braces to the BRACE EXPANSION function, like in Bash.
 ** (DONE, very basically though) Add STREAM REDIRECTION: cat file > /path/to/file. CHECK!!!
 ** Add ranges to remove_function.
 ** Stream redirection does not refresh the screen whe it should, namely, whenever the modified
 ++ file/dir is listed on the screen.

 ** A switch statement is faster than an if...else chain.
 ** It seems that stat() is faster than access() when it comes to check files existence. See:
 ++ https://stackoverflow.com/questions/12774207/fastest-way-to-check-if-a-file-exist-using-standard-c-c11-c
 ** 'old_pwd' is a FIXED array: it always takes 4096 bytes!, even if the current value takes 
 ++ only 20 bytes.
 ** Check for functions that need to redirect the stderr to a file (those which somehow need 
 ++ to refresh the screen), just like I did with cp/mv/rm.
 ** Perhaps it would be nice to add a vault protected by a password (see passwd.c)
 ** Find a way to remove all unused alias files in /tmp.
 ** Take a look at the ouput of gcc When compiled with the following flags:
 ++ a) -Wformat-overflow=2: I get a couple of errors coming from get_properties(). 
 ++ b) -Wformat=2 in a couple of minor functions.
 ++ c) -Wformat-security= there are a couple of fprintf that do not specify the string format, 
 ++ which could be a security whole.
 ++ d) -Wformat-truncation=2: some printf outputs may be truncated due to wrong sizes.
 ++ e) -Wformat-y2k
 ++ f) -Wstrict-overflow=5
 ++ g) -Wshadow
 ++ h) -Wunsafe-loop-optimizations
 ++ i) -Wconversion
 ** Check different kinds of C (traditional, ISO, ANSI, etc.) Check also POSIX specifications.
 ** Take a look at gcc optimization options (-Ox). The default option is -O0 (disabled). It's
 ++ recommended by the GNU guys themselves to use -O2 instead of -O3. In fact, -O2 is the default 
 ++ optimization level for releases of GNU packages. 
 ++ See: https://stackoverflow.com/questions/1778538/how-many-gcc-optimization-levels-are-there
 ++ See also the -march and -mtune flags. To see gcc default compiling options type: 
 ++ $ gcc -Q --help=target | grep 'march\|mtune='.
 ** Some optimization tips: 1 - For large change of decisions (if...else...else) it may be 
 ++ faster to use SWITCH. 2 - DECREMENTING for LOOPS are faster than incrementing ones. Ex: 
 ++ (i=10;i!=0;i--) is faster than its more traditional form: (i=0;i<10;i++). This is so 
 ++ because it is faster to evaluate 'i!=0' than 'i<10'. In the former, the processor says: 
 ++ "is i non-zero?, whereas in the latter it has to perform an extra step: "Subtract i from 10. 
 ++ Is the result non-zero? 3 - We should use UNSIGNED INT instead of int if we know the value 
 ++ will never be negative (some says this doesn't worth it); 4 - When it comes to single chars 
 ++ (say 'c') it's faster to declare it as int than as char. See: 
 ++ https://www.codeproject.com/articles/6154/writing-efficient-c-and-c-code-optimization
 ** Revise open_function.
 ** The logic of bookmarks and copy functions is crap! Rewrite it.
 ** 2- Add the possibility to open files in the background when no application has been passed 
 ++ (ex: o xx &). DONE, but now the pid shown is that of xdg-open, so that when the user tries to
 ++ kill that pid only xdg-open will be killed. Find a way to kill xdg-open AND its child, OR
 ++ make it show not xdg-open's pid, but the program's instead. Drawback: xdg-open would 
 ++ remain alive.
 ** 3 - Touch and mkdir functions aren't necessary at all, execept for the fact that they, unlike
 ++ the built-in commands, refresh the screen. I could call the built-in commands and then
 ++ refresh the screen if they where successfull (if execve is not -1).
 ** 4 - Check ALL the exec commands for: 1 - terminal control; 2 - signal handling.
 ** 5 - I'm not sure about setlocale() in main.
 ** 8 - CliFM isn't able to interpret shell scripts. You cannot do soemthing like this:
 ++ #!/bin/clifm
 ++ echo $PATH   
 ** 9 - Background processes number (bg_proc[100]) should not be limited to 100. In fact, it should
 ++ not be a static, but a dynamic array, in order to be able to modified its number of 
 ++ elements.
 ** 10 - Replace foreground running processes (wait(NULL)) by run_in_foreground()
 ** Replace system() by the exec() family. Done only for external commands, rm, cd_glob_path, 
 ++ aliases, and edit_function.
 ** 11 - Check size_t vs int in case of numeric variables. Calloc, malloc, strlen, among others,
 ++ dealt with size_t variables; In general, use size_t for sizes. It is also recommended to 
 ++ use this variable type for array indexes. Finally, if you compare or asign a size_t variable
 ++ to another variable, this latter should also be defined as size_t (or casted as unisgned 
 ++ [ex: (unsigned)var]). Note: for printing a size_t (or pid_t) variable, use %ld (long double).
 ++ Note2: size_t do not take negative values.
 ** 12 - Check the type of value returned by functions. Ex: strlen returns a size_t value, so that
 ++ in var=strlen(foo), var should be size_t.
 ** 13 - Check that, whenever a function returns, the variables it malloc(ed) are freed.
 ** 14 - Use remove() to delete files and empty directories whose name contains no spaces.
 ** 16 - Error handling: use perror(), defined in stdio.h, and EXIT_FAILURE.
 ** 17 - Be careful when removing symlinks. I remove them with 'rm -r', but perhaps I should 
 ++ use the C unlink() function instead, for rm -r might remove those files pointed by the 
 ++ symlink.
 ** 18 - Make the 'back' function remember more paths. It only goes back once.
 ** 19 - Scalability/stress testing: Make a hard test of the program: big files handling, large 
 ++ amount of files, heavy load/usage, etc. 
 ** 20 - Destructive testing: Try running wrong commands, MAKE IT FAIL! Otherwise, it will never 
 ++ leave the beta state.
 ** 21 - Add a trash function.

 * (DONE) Get rid of the comm_for_matches function and reform the logic of the search function. When 
 + doing a quick search, simply filter the current array of elements and display the matching 
 + ones, if any. In this way, I don't need to do anything special to operate on those elements, 
 + and the comm_for_matches function would be just obsolete.
 * (DONE) Use the glob function to select files when using wildcards.
 * (DONE) Add ranges to the selection function.
 * (DONE) log_function should log filenames instead of ELN whenever an ELN is part of the cmd. 
 * (DONE) My custom function for user defined Variables should only be active when running the 
 + CliFM shell.
 * (DONE) What I called 'commands concatenation' is not 'a && b', but rather 'a; b'. Replace 
 * '&&' by ';'.
 * (DONE) Add an explanation of profile and prompt commands in help.
 * (DONE) Allow the user to define permanent custom variables in the profile file.
 * (DONE) Allow the user to create some custom variables. Ex: 'dir="/home/leo"'; 'cd $dir'. 
 + $var should by expanded by parse_input(), just as I did with '~/'. I should use an array of
 + structs to store the variable name and its value. 
 * (DONE) Save prompt commands into RAM, just like I did with aliases. See get_aliases()
 * (DONE) Add a profile file to allow the user to run custom commands when starting CliFM or
 + before the prompt.
 * (DONE) Do not allow 'cd' to open files.
 * (DONE) Add some option to allow the user to choose to run external commands either via 
 + system() or via exec(). This can be done now in three different ways: 1) config file; 2) 
 + beginning the cmd by a semicolon (ex: ;pacman -Rns $(pacman -Qqtd)); 3) the 'sys' cmd.
 * (DONE) Replace allowed_cmd (in exec_cmd) by a simple return.
 * (DONE) Add commands concatenation. Ex: $ cd /home && ls | grep leo, or: $ leafpad && mirage. 
 +	Executes the right-hand command of && only if the previous one succeeded.
 * (DONE) Every input argument takes 4096 bytes, no matter if it has only 1 byte. SOLUTION:
 + First dump arguments into a buffer big enough, say PATH_MAX, and just then copy the
 + content of this buffer into an array (comm_array), allocating only what is necessary
 + to store that buffer.
 * (DONE) Replace the if...else chain in colors_list by a switch. Remove the two last arguments.
 * Take a look at str(r)char(), which seems to do the same thing as my strcntchar(). NO: both
 + strchar and strrchar return a pointer to the specified char in string, but not its index
 + number. I could use them nonetheless whenever I don't need this index.
 * (DONE) Use getopt_long function to parse external arguments.
 * (DONE) Update colors in properties_function and colors_list.
 * (DONE) Add color for 'file with capability' (???) (\033[0;30;41). Some files with capabilities:
 + /bin/{rsh,rlogin,rcp,ping}
 * (DONE) Add different color (green bg, like 'ls') for sticky directories.
 * (DONE) Replace all calls to calloc and realloc by my custom functions XCALLOC and XREALLOC. 
 + In case of (re)allocation failure, my functions quit the program gracefully, unlike calloc and
 + and realloc, which make the program crash. Make sure to replace as well the argument of 
 + sizeof by the type of data (char * for calloc and char ** for realloc).
 * (DONE) Add brace expansion.
 * (DONE) Replace fixed paths (/bin/{mv, cp, rm}) in copy_function by get_cmd_path
 * (DONE) ls should act as refresh only when cd doesn't list automatically.
 * (DONE) Remove 'Allow sudo' & allowed_cmd: doesn't make sense anymore.
 * (DONE) Check that 'du' exists in dir_size(). Same with 'cp' and 'mv' in copy_function().
 * (DONE) Allow prompt color customization.
 * (DONE) Add the possibility not to list folders content automatically when running cd, just 
 + as in any shell.
 * (DONE) Add ranges to the selection function.
 * (DONE) ADD PIPES: exec() is great, but, at the moment, it's not possible to PIPE commands. On 
 + pipes see: http://crasseux.com/books/ctutorial/Programming-with-pipes.html, and specially
 + https://stackoverflow.com/questions/17630247/coding-multiple-pipe-in-c, from where I took
 + the basic code for multiple pipes.
 * (DONE) Do not limit the maximum number of input arguments (max_args=100). Use realloc instead 
 + in parse_input_string() to allocate only as many arguments as entered; 
 * (DONE) Remove strlen from loops. in 'i=0;i<strlen(str);i++' strlen is called as many time 
 + as chars contained in str. To void this waste of resources, write the loop as this:
 + size_t str_len=strlen(str); for (i=0;i<str_len;i++). In this way, strlen is called just
 + once. Another possibility: size_t str_len; for (i=0; str_len=strlen(str);i++)
 * See: http://www.cprogramming.com/tips/tip/dont-use-strlen-in-a-loop-condition
 * (DONE) Replace get_linkref() by realpath().
 * (DONE) Add command names to listed jobs.
 * (DONE) Add internal commands (ex: open, sel, history, etc) to the autocomplete list.
 * (DONE) Enhace the history function by allowing the user to choose and execute some history 
 + line. Ex: if 'ls' is the 3rd line in history, the user could type '!3' to run the 'ls' 
 + command.
 * (DONE) Change the way of listing files in list_dir(): use the d_type field of the dirent
 + struct to find out the type of the listed file instead of running a separate command 
 + (lstat). By doing this we save one or two commands for EACH listed file, which is a lot.
 * (DONE) Rewrite the logic of the program main loop: do an infinite loop (while (1)) in 
 + main() containing the three basic steps of a shell: 1 - prompt and input; 2 - parse input; 
 + 3 - execute command.
 * (DONE) The exec family functions do not do globbing: something like 'rm *' won't work. Use 
 + the glob() function defined in glob.h.
 * (DONE) - When running a command with sudo (or a backgrounded command) and then Ctrl+c I get two 
 + prompts in the same line. SOLUTION: reenable signals for external commands. Otherwise, you'll 
 + get a new prompt but the command will be left running.
 * - (DONE) Add commands autocompletion, not just filenames. SOLUTION: wrote a custom autocomplete
 + function for readline.
 * (DONE) Get environment variables, especially PATH to run external commands. PATH is also
 + needed for commands autocompletion. 
 * (DONE) Add an alias function. Use bash syntax and append aliases to clifmrc.
 * (DONE) Change list color for suid files (red bg, white fg) and sgid files (bold_yellow bg 
 + black fg)
 * Check sprintf and strcpy calls for buffer overflow. Replace by snprintf and strncpy (also 
 + strcat by strncat).
 * (NO) - Get rid of calls to system() (for "rm -r", "cp") and replace them by pure C code. I've 
 + already replaced 'ls -hl' (plus some info from 'stat' command) and 'clear' by my own code.
 + On cp, take a look at: http://linuxandc.com/program-implement-linux-cp-copy-command/
 + DO NOT replace these commands; it's much more safe to call them than to (unnecessarily)
 + re-write them. The latter would have only educational purposes.
 * (UNNEEDED) - Replace rl_no_hist by: answer=getchar(); while ((getchar() != '\n')) {}. This, however,
 + is not necessary at all.
 * (DONE) - Allow Bash aliases. SOLUTION: implement a custom aliases system.
 * (DONE) - Cppcheck gives error on realloc. An explanation could be: "It's a bad idea to assign 
 + the results of realloc to the same old pointer, because if the reallocation fails, 0 will 
 + be assigned to p and you will lose your pointer to the old memory, which will in turn leak."
 + (http://stackoverflow.com/questions/11974719/realloc-error-in-c) Solution in this same site.
 + See also: http://www.mantidproject.org/How_to_fix_CppCheck_Errors

 * (DONE) - Replace malloc by calloc, since this latter, unlike malloc, automatically initializes
 + all of the allocated blocks to zero, so that there's no need for memset. It seems that 
 + calloc is better than malloc/memset in that it is at least twice faster (or 100x faster) 
 + and uses less memory. See: https://vorpus.org/blog/why-does-calloc-exist/
 * (DONE) - Commands fail when file contains single quotes (') SOLUTION: replace single by double 
 + quotes when passing filenames as arguments. However, if the file contains double quotes in
 + its name, it will fail anyway. I should test all files for single and double: if contains
 + single, use double, if double, use single instead. What if it contains both, single and 
 + double??!! Check only in case of ELN's in cp and rm (both in case of sel or not)
 * (DONE) - Find a way to make alphasort() in scandir() non-case-sensitive. Solution: write a
 + custom alphasort function using tolower().
 * (DONE) - Allow the user to choose whether he/she wants to clear the screen after every
 + screen change/refresh or not. Added to the configuration file.
 * (DONE) - Smoke testing: minimal attempts to operate the software, designed to determine 
 + whether there are any basic problems that will prevent it from working at all.
 * (DONE) - Limit the log file size (in line numbers), say, 1000 by default. Allow change it
 + in the config file.
 * (DONE) - Add better sync between different instances of the program regarding selected 
 + files. SOLUTION: call the get_sel_files function from the prompt function, so that sel files 
 + will be updated everytime a new prompt is called, even if the modification was made in a 
 + different instance of the program. The trick here is that selected files are stored, via 
 + the save_sel function, in ram (a file in /tmp), so that this data is independent from the 
 + private variables of different instances of the program, which take their value from the 
 + tmp file. NOTE: I'm getting an error in Valgrind coming from the get_sel_files function 
 + every time I call it from the prompt function. Fixed by freeing the sel_elements array
 + at the beginning of get_sel_files function.
 * 20 - When running the 'pr' command, I get an error from Valgrind whenever the GID of a file 
 + is UNKNOWN. Do "pr ~/scripts/test/file_bk". Problematic line (3621): group=getgrgid(group_id);
 + It seems to be a library (/usr/lib/libc-2.25.so) error, not mine.

###KNOWN BUGS###
 ** When running as root, the program cannot be called as argument for a terminal. See the
 ++ setpgid lines in init_shell().
 ** 1 - Wrongly listing non-empty files as empty. Check /proc/pid/cmdline. Stat's st_size is not
 ++ accurate enough. NOTE: the buil-in stat command has the same problem.
 ** 2 - Errors in Valgrind when: 1 - search for a pattern; 2 - select an element; 3 - search 
 ** for the first pattern again.
 ** 3 - BACKUP doesn't work as it should when files are removed using wildcards.
 ** 4 - Since removed files are copied into /tmp, whenever big files, or a great amount of 
 ++ files are removed, the capacity of /tmp could be easily exceeded producing thereby errors, 
 ++ like running out of memory!


 * (SOLVED) The log_function is logging profile and prompt commands instead of just external
 + commands. SOLUTION: add a 'no_log' flag to tell exec_cmd() not to log cmds when they come 
 + from the profile or prompt functions.
 * (SOLVED) 'ls' stopped working. Problem: the '=' case of the switch statement in 
 + parse_input_str(). The condition to evaluate user variables should be more restricted.
 + SOLUTION: check for spaces before '=' is order to avoid taking as variables things like:
 + 'ls -color=auto'.
 * (SOLVED) Search for a file (/x) and then type this: 'cp 1 ~/scripts/test/empty_folder/': 
 + ERRORS!!!
 * (SOLVED) Errors in folder_select and file_select when trying to access to a failed 
 + mountpoint. SOLUTION: add an existence check at the beginning of theses functions.
 * (SOLVED) When running pacman via system() this error came up from within pacman itself: 
 + "call to waitpid failed". SOLUTION: do not run system by itself, but execle("/bin/sh"...), 
 + which is what system() basically does. 
 * (SOLVED) 'ls -a | grep file > stream' doesn't work.
 * (SOLVED) 'ls | grep scripts && mirage' fails and produces an error.
 * (SOLVED) Concatenated commands doesn't take background operator (&).
 *(SOLVED) Take a look at the end of dir_list(): there is no difference between >1.000 and 
 + >10.000
 * (SOLVED) get_sel_files() is being executed with every new prompt. SOLUTION: move the function
 + to refresh().
 * (SOLVED) Error: ~/scripts/test $ lsblk -i > empty_folder/stream
 * (SOLVED) 'path' variable is a FIXED array: it always takes 4096 bytes!, even if the current 
 + value takes only 20 bytes.
 * (SOLVED) When running an alias quotes don't work. SOLUTION: first parse the aliased command 
 + (with parse_input_str) and then add entered parameters, if any, to the parsed aliased command.
 * (SOLVED) Aliases work only for external commands. SOLUTION: relocate run_alias_cmd (renamed
 + now as check_for_alias) in the main loop of main, before exec_cmd, and remove it from
 + this latter. Inside check_for_alias, call exec_cmd instead of execve().
 * (SOLVED) Sometimes the number of files contained in a directory is wrongly shown. Ex: 
 + instead of usb (empty) I get 'usb /-2'!!
 * (SOLVED) xdg-open is not installed by default in every Linux system. Fix it. Perhaps, xdg-open should
 + be installed as a requirement of CliFM. SOLUTION: Added a check for xdg-open at startup, so
 + that it won't be called if it's not installed.
 * (SOLVED) When it comes to optical drives (CD-ROM) files color is always default, i.e., its
 + file type isn't recognized. It doesn't happen with USB drives though. The problem seems to
 + be scandir's st_type. the stat fucntion instead doesn't have this problem. So, whenever 
 + st_type is DT_UNKNOWN (default case of the switch statement in list_dir), call a modified 
 + version of the colors_list function (which works with stat) and tell it whether it has to 
 + add a newline character at the end or not by means of a new boolean argument: newline
 * (SOLVED) 'cd ~/' doesn't work as it should. Instead of changing the path to the home dir, 
 + it says: 'No such file or directory'.
 * (SOLVED) If you edit the config file, options will be reloaded according to this file, overwritting
 + any external paramater passed to the program. SOLUTION: copy the values of argc and argv to
 + rerun external_arguments everytime the user modifies the config file.
 * (SOLVED) Sometimes columns are wrong (see ~/.thumbnails/normal). It would be nice to produce
 + a better columned output in general, just like the 'ls' command does.
 * (SOLVED) When running a globbing command (ex: leafpad world*) cannot SIG_INT.
 * (SOLVED) When I type 'cd/bin' (with no spaces) a wrong message is shown: "CliFM: cd/bin: 
 + Permission denied".
 * (SOLVED) Error when entering incomplete quotation marks (ex: "test, or 'test).
 * (SOLVED) Error when entering a directory as first input word (ex: '/').
 * (SOLVED) Error when typing '     |', or any weird char like {,  (in parse_input_string)
 * (SOLVED) This: '!"#%#$&/%&(&/(%#$' produces the same error. Simply type any shit and watch caos go!
 + This: '(/' says 'Permision denied' (from external command) 
 * (SOLVED) Nested double dots (ex: cd ../..) are read as a simple double dot (cd ..)
 * (SOLVED) The init_shell function prevents CliFM from being called as argument for a 
 + terminal, ex: 'xterm -e /path/to/clifm'. It can be executed though in a running terminal
 + by typing the corresponding command: '$ clifm'. However, if I remove init_shell, other
 + problems come out. SOLUTION: remove the setpgid lines from init_shell().
 * (SOLVED) Fails when running interactive terminal programs. SOLUTION: use tcsetpgrp to allow 
 + the executed program to control the terminal. Then give control back to the main program.
 + PROBLEM: Now I can't send a SIGINT to the process. SOLUTION: set SIG_INT to default action
 + (SIG_DFL) before executing the program, and set it to ignore (SIG_IGN) after the program has
 + been executed (both in the child)
 * (SOLVED) Aliases don't work when called with wildcards.
 * (SOLVED) When a file is opened via xdg-open (no application specified) in the foreground, 
 + a SIG_INT signal (Ctrl-c) will not kill the opening application, but rather xdg-open only.
 + SOLUTION: when executing the process, make its pid equal to its pgid (setpgid(0, 0)), so 
 + that kill, called with a negative pid: "kill(-pid, SIGTERM)", will kill every process 
 + corresponding to that pgid.
 * (SOLVED) When: 1 - run background process and leave it running; 2 - run foreground process;
 + 3 - send a SIG_INT signal (Ctrl-c) to the foreground process, this latter will get terminated,
 + but the shell keeps waiting for the backgound process to be terminated. SOLUTION: replace
 + wait(NULL) by waitpid(pid, NULL, 0)
 * (SOLVED) When: 1 - run process in background; 2 - kill that process; 3 - run a process in 
 + the foreground, this latter is sent to the background. Ex: 'leafpad &'; close leafpad; 
 + 'leafpad'. Solution: use the init_shell function provided by the GNU. See:
 + https://www.gnu.org/software/libc/manual/html_node/Initializing-the-Shell.html#Initializing-the-Shell
 * Weird: when opening ~/.xinitrc as 'o(pen) ELN/filename' it doesn't remain in the foreground.
 * It ONLY happens with this file. Only a very minor bug.
 * (SOLVED) Valgrind error in color_list() when listing or searching in /usr/bin or some large 
 + directory, like /usr/lib. It only happens on the first search. SOLUTION: memory reserved 
 + for allocated variables in search_function was not big enough. Added 1.
 * (UNNEEDED) Readline has some minor memory leaks. It think it's the function itself. Look for an 
 * alternative or some way to fix it. However, all these readline leaks ("still reachable" 
 * according to what Valgrind says) are "not real leaks," and therefore not dangerous at all, 
 * as "real leaks" are. See: http://www.w3lc.com/2012/02/analysis-of-valgrind-still-reachable.html
 * Furthermore, this is not really a bug, not even a leak of MY programm, but at most an issue 
 * of the readline library.
 * (SOLVED) When doing 'pr' in a root folder (ex: /dev/sda3) the prompt changes to a root prompt!!!
 * The problem is the getpwuid function in get_properties. SOLUTION: A very dirty workaround is 
 * to run get_user() at the end of the get_properties function. However, I stil don't know why 
 * nor how this happens. I suspect of some memory override issue.
 * (SOLVED) Columns go bad when socket files are present (cd ~/.dropbox). SOLUTION: there 
 * was an extra new line character when printing socket files in list_dir().
 * (SOLVED) mv and cp dont work when either source or destiny file  contains spaces, for it 
 * takes the last one as destiny and the previous one as source. I should solve this before 
 * any command, that is, when parsing arguments, in order not to take care of this issue with 
 * every command and function. SOLVED: still need some improvement though. a) it will take 
 * everything after the first quote (single or double) as one argument when there is no closing 
 * quote; b) when there's a quote within an argument, it will be omitted. Note: Bash admits 
 * neither incomplete quotes nor quotes within strings for filenames. So, I don't have to do 
 * it either.
 * (SOLVED) Socket files are listed as if they were directories (cd /tmp). SOLUTION:
 * exclude socket files from folder_select() and include them in file_select().
 * (SOLVED) Whenever you try to open a socket file ('o ELN') CliFM complains: "Error 
 * listing files". After this, the program will crash on the next command you run.
 * SOLUTION: when opening directories, make sure it is not a socket file by adding a negative
 * condition: !S_ISSOCKET(file_attirb.st_mode).
 * (SOLVED) When showing the properties of a socket file there is break in the second line.
 * SOLUTION: there was a new line char in "printf(Socket)" in 'properties_function'.
 * (SOLVED) Empty executable files (green) are shown in 'pr' function as empty files (yellow).
 * SOLUTION: It was explicitly said to print it yellow in properties_function. Changed to green.
 * (SOLVED) When creating a new file, it will override without asking any existing file 
 * with the same name (this is what the touch built-in command do by default). However, I added
 * a check for this case, for this could lead to unintentional lost of data.
 * (SOLVED) The program may be killed from within by the kill, pkill or killall commands. 
 * If this happens, malloced variables won't be freed. SOLUTION: get both the invocation name 
 * and the pid of CliFM; then check whether 'kill' is targeted to pid or killall (or pkill) to 
 * invocation name and prevent them from running.   
 * (SOLVED) It's possible for a user to become root with the 'su' command. When this 
 * happens from within the program, CliFM commands won't work anymore, for the prompt will 
 * become an ordinary shell: basically, the user is outside the program from within the 
 * program itself. However, the user can still come back to the CliFM prompt by simply exiting
 * the root shell. SOLUTION: Not a problem. 
 * (SOLVED) The user can run any shell whithin the program, for instance, 'exec bash'.
 * SOLUTION: Not a problem.
 * (SOLVED) Valgrind error when editing bookmarks.
*/

/*For the time being, CliFM hardly goes beyond the 5 Mb! Could I make it even more 
lightweight? Readline by itself takes 2.2Mb, that is, almost half of the program 
memory footprint.*/

/* On C functions definitions consult its corresponding manpage (ex: man 3 printf) or see:
http://pubs.opengroup.org/onlinepubs/9699919799/nframe.html*/

/*###CUSTOM FUNCTIONS### 
They all work independently of this program and its variables.*/

/*int get_max_pid()
{
	#define LINE_MAX 256
	FILE *fp_pid;
	char line[LINE_MAX]="";
	fp_pid=fopen("/proc/sys/kernel/pid_max", "r");
	if (fp_pid != NULL) {
		fgets(line, LINE_MAX, fp_pid);
		if (line != NULL) {
			return atoi(line);
		}
	}
	else 
		return 0;
}*/

pid_t get_own_pid(void)
{
	pid_t pid;
	//get the process id
	if ((pid=getpid()) < 0)
		return 0;
	else
		return pid;
}

int is_number(char *string)
//Check whether a given string contains only digits. Returns 1 if true and 0 if false.
{
	size_t str_len=strlen(string), i;
	for (i=0;i<str_len;i++)
		if (!(isdigit(string[i])))
			return 0;
	return 1;
}

int digits_in_str(char *string)
//Return the amount of digits in a given string
{
	size_t str_len=strlen(string);
	int digits=0;
	for (int i=0;(unsigned)i<str_len;i++) {
		if (isdigit(string[i]))
			digits++;
	}
	return digits;
}

int digits_in_num (int num) {
//Return the amount of digits of a given number
	return snprintf(NULL, 0, "%d", num)-(num < 0);
}

int alphasort_insensitive(const struct dirent **a, const struct dirent **b) {
/*This is a modification of the alphasort function that makes it case insensitive. It also
sorts without taking the initial dot of hidden files into account.*/
	char string1[NAME_MAX]="", string2[NAME_MAX]="";
	size_t i;
	/*Type size_t must be big enough to store the size of any possible object. Unsigned int 
	doesn't have to satisfy that condition. For example in 64 bit systems int and unsigned 
	int may be 32 bit wide, but size_t must be big enough to store numbers bigger than 4G.
	limits.h defines max int size as 2147483647. Big enough, true, but beyond this value
	you'll surely encouter some problems.*/
	for (i=0;(*a)->d_name[i];i++) {
		if ((*a)->d_name[0] == '.') //do not take initial dot into account.
			string1[i]=tolower((*a)->d_name[i+1]); //set every char to lower case.
		else
			string1[i]=tolower((*a)->d_name[i]);
	}
	for (i=0;(*b)->d_name[i];i++) {
		if ((*b)->d_name[0] == '.')
			string2[i]=tolower((*b)->d_name[i+1]);
		else
			string2[i]=tolower((*b)->d_name[i]);
	}
	return strcmp(string1, string2);
}

char *handle_spaces(char *str)
/*Remove leading, terminating and double spaces from str. Returns NULL if str: a) is NULL;
b) contains only spaces; c) is empty*/
{
	if (!str) return NULL; //if NULL
	size_t str_len=strlen(str);
	if (str_len == 0) return NULL; //if empty
	char buf[str_len];
	memset(buf, '\0', str_len);
	unsigned length=0;
	for (unsigned i=0;i<str_len;i++) {
		if (!isspace(str[i]) && str[i] != '\0') {
			buf[length++]=str[i];
			int index=i;
			for (unsigned j=index+1;j<str_len;j++) {
				if (!isspace(str[j]) || (isspace(str[j]) && !isspace(str[j+1]) && 
															   str[j+1] != '\0'))
					buf[length++]=str[j];
			}
			break;
		}
	}
	if (length) { //if buf isn't empty
		char *f_str=calloc(length+1, sizeof(char *));
		strncpy(f_str, buf, length);
		return f_str;
	}
	else return NULL; //if only spaces
}

//Take a look at strrchar, which seems to do the same thing
short strcntchr (char *str, char c)
//returns the index of the last ocurrence of given char in a string. 
//False (no matches) == -1
{
	size_t str_len=strlen(str);
	for (int i=(unsigned)str_len;i>=0;i--) {
		if (str[i] == c)
			return i;
	}
	return -1;
}

int str_ends_with(char* str, const char* suffix)
/*returns 1 if str ends with suffix (ex: returns 1 if "clifm.cfm", ends with ".cfm"), and zero 
if not.*/
{
	size_t strLen=strlen(str);
	size_t suffixLen=strlen(suffix);
	if (suffixLen <= strLen) {
		return strncmp(str+strLen-suffixLen, suffix, suffixLen) == 0;
	}
	return 0;
}

char *straft(char *str, char c)
/*returns the string after the first appearance of a given char. If not found, 
returns NULL.*/
{
	size_t str_len=strlen(str);
	for (int i=0;(unsigned)i<str_len;i++) {
		if (str[i] == c) {
			unsigned index=i, j=0;
			if (index == str_len-1) return NULL; //if 'c' is last char of str
			char *f_str=calloc((str_len-index), sizeof(char *));
			for (j=0;j<(str_len-index);j++)
				f_str[j]=str[++i];
			f_str[j]='\0';
			return f_str;
		}
	}
	return NULL; //'c' was not found
}
/*
char *xstraft(char *str, const char c)
//50% faster than straft!
{
	if (!str) return NULL;
	unsigned index=-1;
	for (unsigned i=0;str[i];i++) {
		if (str[i] == c) {
			index=i;
			break;
		}
	}
	if ((int)index == -1 || !(str+(index+1))) 
		return NULL;
	return str+(index+1);
}
*/

char *straftlst(char *str, char c)
//returns the string after the last appearance of a given char, NULL if no matches
{
	size_t str_len=strlen(str);
	for (int i=str_len;i>=0;i--) {
		if (str[i] == c) {
			unsigned index=i, k=0;
			if (index == str_len-1) return NULL;
			char *f_str=calloc((str_len-index)+1, sizeof(char *));
			for (unsigned j=index+1;j<str_len;j++) {
				f_str[k++]=str[j];
			}
			f_str[k]='\0';
			return f_str;
		}
	}
	return NULL;
}

char *strbfr(char *str, char c)
/*returns the string in *string before the first appearance of a given char. If not found, 
returns NULL. NOTE: add a thrid argument to let the user choose whether the given char is
the first or the last in the string.*/
{
	size_t str_len=strlen(str);
	for (unsigned i=0;i<str_len;i++) {
		if (str[i] == c) {
			if (i == 0) return NULL;
			unsigned index=i, j=0;
			char *f_str=calloc(index+1, sizeof(char *));
			for (i=0;i<index;i++)
				f_str[j++]=str[i];
			f_str[j]='\0';
			return f_str;
		}
	}
	return NULL;
}

char *strbfrlst(char *str, char c)
//returns the string in *string before the last appearance of a given char. If not found, 
//returns NULL. NOTE: add a thrid argument to let the user choose whether the given char is
//the first or the last in the string.
{
	size_t str_len=strlen(str);
	for (unsigned i=str_len;(int)i>=0;i--) {
		if (str[i] == c) {
			if (i == 0) return NULL;
			unsigned index=i, j=0, k=0;
			char *f_str=calloc(index+1, sizeof(char *));
			for (j=0;j<index;j++)
				f_str[j]=str[k++];
			f_str[j]='\0';
			return f_str;
		}
	}
	return NULL;
}

char *strbtw(char *str, char a, char b)
/*return the string in 'str' between two given chars ('a' and 'b'). If not found, returns NULL. 
Receives a string and the two chars to get the string between them.*/
{
	size_t str_len=strlen(str);
	if (str_len == 0) return NULL;
	int ch_from=-1;
	for (int i=0;(unsigned)i<str_len;i++) {
		if (str[i] == a) {
			ch_from=i;
			break;
		}
	}
	if (ch_from == -1) return NULL;
	int ch_to=-1;
	for (int i=(int)str_len-1;i>ch_from;i--) {
		if (str[i] == b) {
			ch_to=i;
			break;
		}
	}
	if (ch_to == -1) return NULL;
	int j=0;
	char *f_str=calloc((ch_to-ch_from)+1, sizeof(char *));
	for(int i=ch_from+1;i<ch_to;i++)
		f_str[j++]=str[i];
	f_str[j]='\0';
	return f_str;
}

char **get_substr(char *str, const char IFS)
{
	if (!str) return NULL;
	char **substr=NULL;
	char buf[1024]="";
	size_t str_len=strlen(str);
	if (str_len == 0) return NULL;
	unsigned length=0, substr_n=0;
	for (unsigned i=0;i<str_len;i++) {
		while (str[i] != IFS && str[i] != '\0' && length < sizeof(buf))
			buf[length++]=str[i++];
		if (length) {
			buf[length]='\0';
			substr=realloc(substr, sizeof(char **)*(substr_n+1));
			substr[substr_n]=calloc(length, sizeof(char *));
			strncpy(substr[substr_n++], buf, length);
			length=0;
		}
	}
	if (!substr_n) return NULL;
	substr=realloc(substr, sizeof(char **)*(substr_n+1));
	substr[substr_n]=NULL;
	return substr;
}

char *get_user(void)
{
	register struct passwd *pw;
	register uid_t uid;
	uid=geteuid();
	pw=getpwuid(uid);
	if (pw)
		return pw->pw_name;
	else
		return NULL;
}

char *get_date(void)
{
	time_t rawtime=time(NULL);
	struct tm *tm=localtime(&rawtime);
	char *date=calloc(64, sizeof(char *));
	strftime(date, sizeof(date), "%c", tm);
	char *months[]={"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", 
						"Sep", "Oct", "Nov", "Dec"};
	char *weekday[]={"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
	snprintf(date, 64, "%s, %s %d, %d, %d:%d:%d\n", weekday[tm->tm_wday], months[tm->tm_mon], 
	tm->tm_mday, tm->tm_year+1900, tm->tm_hour, tm->tm_min, tm->tm_sec);
	date[strlen(date)-1]='\0';
	return date;	
}

char *get_proc_name(pid_t pid)
//Get the name of a process from its pid
{
//	#ifndef LINE_MAX
//		#define LINE_MAX 256
//	#endif
	FILE *fp;
	char proc_file[24]="";
	char *line=calloc(MAX_LINE, sizeof(char *));
	snprintf(proc_file, 24, "/proc/%d/comm", pid);
	/*/proc/pid/comm is where 'ps' gets the command name*/
	fp=fopen(proc_file, "r");
	if (fp == NULL) {
		free(line);
		return NULL;
	}
	else
		fgets(line, MAX_LINE, fp);
	fclose(fp);
	//remove new line char, if any
	for (int i=0;line[i];i++) {
		if (line[i] == '\n') {
			line[i]='\0';
			break;
		}
	}
	return line;
}

char *get_file_size(off_t file_size)
//Gets st_size data from 'stat' and convert it to human readeable size. 
{
	char *size_type=calloc(20, sizeof(char *));
//	char units[]={ 'b', 'K', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y' };
	short units_n=0;
	float size=file_size;
	while (size > 1000) {
		size=size/1000;
		++units_n;
	}
	//if bytes
	if (!units_n) sprintf(size_type, "%.0f bytes", size);
	else {
		char units[]={ 'b', 'K', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y' };		
		sprintf(size_type, "%.1f%ciB", size, units[units_n]);
	}
	return size_type;
}

char *cd_glob_path(char *path)
//Return the first match for a given path using wildcards, or NULL if no match.
{
	char *result=NULL;
	glob_t globbed_files;
	int ret=glob(path, 0, NULL, &globbed_files);
	if (ret == 0) {
		result=calloc(strlen(globbed_files.gl_pathv[0]), sizeof(char *));
		strncpy(result, globbed_files.gl_pathv[0], strlen(globbed_files.gl_pathv[0]));
		globfree(&globbed_files);
		return result;
	}
	else {
		globfree(&globbed_files);		
		return NULL;
	}
}

//###FUNCTIONS PROTOTYPES###

void *xcalloc(void *ptr, size_t nmemb, size_t size);
void *xrealloc(void *ptr, size_t size);
void free_stuff(void);
//void get_my_name(char *argv_zero);
void set_signals_to_ignore(void);
void set_signals_to_default(void);
void init_shell(void);
void xdg_open_check(void);
//void signal_handler(int sig_num);
void splash(void);
char **my_rl_completion(const char *text, int start, int end);
char *bin_cmd_generator(const char *text, int state);
void get_path_programs(void);
//int copy_envp(char **envp);
int get_path_env(void);
void get_sel_files(void);
void init_config(void);
void exec_profile(void);
void external_arguments(int argc, char **argv);
void get_aliases_n_prompt_cmds(void);
void check_log_file_size(void);
char **check_for_alias(char **comm);
void get_history(void);
char *home_tilde(char *path);
char **parse_input_str(char *str);
void list_dir(void);
char *prompt(void);
void exec_prompt_cmds(void);
void exec_cmd(char **comm);
void launch_execv(int is_background, char **comm);
char ***parse_pipes(char *string);
char *get_cmd_path(char *cmd);
int brace_expansion(char *str);
void update_path(char *new_path);
void open_function(char **comm);
void open_element(int eln, char **comm);
void run_in_foreground(pid_t pid);
void run_in_background(pid_t pid);
int run_glob_cmd(int options_n, int is_background, char **args, char *path);
char *check_cmd_stderr(char *cmd, char **args);
void copy_function(char **comm);
void move_function(char **comm);
void remove_function(char **comm);
//void remove_dir(char *);
void symlink_function(char **comm);
void chown_chmod(char **comm);
void mkdir_function(char **comm);
void touch_function(char **comm);
void properties_function(char **comm);
void get_properties(char *filename);
int save_sel(void);
void sel_function(char **comm);
void show_sel_files(void);
void deselect(char **comm);
void search_function(char **comm);
//void comm_for_matches(char **comm);
void bookmarks_function(void);
char **bm_prompt(void);
int count_dir(char *dir_path);
void free_dirlist(void);
char *rl_no_hist(char *prompt);
void log_function(char **comm);
void history_function(char **comm);
void run_history_cmd(char *comm);
void backup_function(char **comm, char *file);
void edit_function(char **comm);
void colors_list(char *entry, int i);
void concatenate_cmds(char *cmd);
void stream_redirection(char *string);
void exec_pipes(char ***cmd); 
void list_jobs(void);
void hidden_function(char **comm);
void help_function(void);
void color_codes(void);
void list_commands(void);
int folder_select(const struct dirent *entry);
int file_select(const struct dirent *entry);
int skip_implied_dot(const struct dirent *entry);
void version_function(void);
void license(void);
void bonus_function(void);
void show_aliases(void);
char *parse_usrvar_value(char *str, char c);
void create_usr_var(char *str);
void free_usrvars(void);
void readline_kbinds();
int readline_kbind_action (int count, int key);

/* Some notes on memory:
If a variable is declared OUTSIDE of a function, it is typically considered "GLOBAL," 
meaning that any function can access it. Global variables are stored in a special area in 
*the program's memory called the DATA SEGMENT. This memory is in use for as long as the 
program is running.
The DATA SEGMENT is where global and static variables (if non-zero initialized) are stored.
Variables initialized to zero or uninitialized are stored in the BSS.
The STACK is used to store FIXED variables that are used only INSIDE of a function (LOCAL). 
Stack memory is temporary. Once a function finishes running, all of the memory for its 
variables is freed.
The HEAP is where dynamic variables are stored. 
The STACK is MUCH MORE FASTER THAN THE HEAP, THOUGH 
DINAMYCALLY ALLOCATED ARRAYS GENERALLY MAKE THE PROGRAM MUCH MORE LEIGHTWEIGHT IN TERMS
OF MEMORY FOOTPRINT. Ex: 
	char str[4096]="test"; //4096 bytes
obviously takes more memory than this:
 	char *str=calloc(strlen("test"), sizeof(char *)); //5 bytes
However, the first procedure (0.000079 seconds) is dozens of times faster than the second 
one (0.000001 seconds!!). The results are almost the same even without strlen, that is, as
follows: char *str=calloc(5, sizeof(char *)); Replacing calloc by malloc doesn't seem to 
make any difference either.

high address
    _______________
							| cmd-line args and env variables
	_______________
	4 - STACK*				| fixed local variables (not declared as static)
							  MUCH MORE FASTER THAN THE HEAP!!!
	---------------
	
	
	---------------
	3 - HEAP**				| dynamically allocated variables (the home of malloc, realloc, and free)
	_______________
	3 - BBS 				| uninitialized or initialized to zero
    _______________
	2 - DATA SEGMENT        | global (static or not) and local static non-zero-initialized 
							  variables. This mem is freed only once the program exited.
	_______________
	1 - TEXT (CODE) SEGMENT	|
low address 

*The stack grows in direction to the heap.
** The heap grows in direction to the stack.
Note: if the heap and the stack met, you're out of memory.

Data types sizes:
CHAR: 1 byte (8 bits) (ascii chars: -128 to 127 or 0 to 255 if unsigned).
	char str[length] takes 1xlength bytes.
SHORT: 2 bytes (16 bits) (-32,768 to 32,767).
INT: 2 (for old PC's) or 4 bytes (32 bits). if 4 bytes: -2,147,483,648 to 2,147,483,647 
	(if signed). 
	'int var[5]', for example, takes 4x5 bytes.
LONG: 8 bytes (64 bits) (-2,147,483,648 to 2,147,483,647, if signed)
LONG LONG: 8 bytes
FLOAT: 4 bytes (1.2E-38 to 3.4E+38. Six decimal places)
DOUBLE: 8 bytes (2.3E-308 to 1.7E+308. 15 decimal places).
LONG DOUBLE: 16 bytes (128 bits)

Note: unsigned data types may contain more positive values that its signed counterpart, but at
the price of not being able to store negative values.
See: http://www.geeksforgeeks.org/memory-layout-of-c-program/

*/

//###GLOBAL VARIABLES####
//Struct to store user defined variables
struct usrvar_t {
	char *name;
	char *value;
};
/*Always initialize variables, to NULL if string, to zero if int; otherwise they may contain
garbage, and an access to them may result in a crash or some invalid data being read. However,
non-initialized variables are automatically initialized by the compiler*/
char splash_screen=-1, welcome_message=-1, backup=-1, search_mark=0, move_mark=0, 
	show_hidden=-1, clear_screen=-1, internal_cmd_n=27, shell_terminal=0, no_log=0, 
	shell_is_interactive=0, glob_cmd=0, list_folders_first=-1, case_sensitive=-1, 
	cd_lists_on_the_fly=-1, prompt_color_set=-1, sys_shell=-1, sys_shell_status=0;
/*A short int accepts values from -32,768 to 32,767, and since all these variables will take 
-1, 0, and 1 as values, a short int is more than enough. Now, since short takes 2 bytes of 
memory, using int for these variables would be a waste of 2 bytes (since an int takes 4 bytes).
I can even use char (or signed char), which takes only 1 byte (8 bits) and accepts negative 
numbers (-128 -> +127).
NOTE: if the value passed to a variable is bigger than what the variable can hold, the result 
of a comparison with this variable will always be true.*/
int files=0, args_n=0, sel_n=0, max_hist=-1, max_log=-1, pipes_index=0, path_n=0, 
	current_hist_n=0, bg_proc_n=0, argc_bk=0, usrvar_n=0; //current amount of user defined 
	//variables; //-1 means non-initialized
size_t def_path_len=0;
struct termios shell_tmodes;
pid_t bg_proc[100]={ 0 }, own_pid=0;
struct dirent **dirlist=NULL;
struct usrvar_t *usr_var=NULL;
char *user=NULL, *path=NULL, old_pwd[PATH_MAX]="", **sel_elements=NULL, sel_file_user[64]="", 
	**paths=NULL, **bin_commands=NULL, **history=NULL, *xdg_open_path=NULL, **braces=NULL, 
	prompt_color[19]="", **pipes=NULL, **argv_bk=NULL; //**my_envp=NULL, *my_invocation_name=NULL,
char CONFIG_DIR[CONF_PATHS_MAX]="", CONFIG_FILE[CONF_PATHS_MAX]="", BM_FILE[CONF_PATHS_MAX]="", 
	 hostname[HOST_NAME_MAX]="", LOG_FILE[CONF_PATHS_MAX]="", LOG_FILE_TMP[CONF_PATHS_MAX]="", HIST_FILE[CONF_PATHS_MAX]="", 
	 BK_DIR[CONF_PATHS_MAX]="", ALIASES_FILE[CONF_PATHS_MAX]="", PROFILE_FILE[CONF_PATHS_MAX]="", 
	 PROMPT_FILE[CONF_PATHS_MAX]="", *INTERNAL_CMDS[27]={ "alias", "open", "prop", "pr", "back", 
		 "move", "paste", "sel", "selbox", "desel", "link", "refresh", "backup", "edit", 
		 "history", "hidden", "path", "help", "commands", "colors", "version", "license", 
		 "splash", "folders first", "jobs", "exit", "quit" };
/*According to useradd manpage, the max lenght of a username is 32. So, "/home/" (6) + 32
+ "/.config/clifm/bookmarks.cfm" (31) = 66. This is then the max length I need for config folders 
and files. So, 128 (CONF_PATHS_MAX) is more than enough, and perhaps too much.*/

//###COLORS###
/* \001 and \002 tell the program that color codes are non-printing chars. This is specially
useful for the prompt, i.e., when passing color codes to readline.*/

#define default_color "" //default terminal foreground color
#define blue "\033[1;34m"
#define d_blue "\033[0;34m"
#define green "\033[1;32m"
#define d_green "\033[0;32m"
#define gray "\033[1;30m"
//#define char d_gray "\033[0;30m"
#define white "\033[1;37m"
//#define char d_white "\033[0;37m"
#define yellow "\033[1;33m"
#define d_yellow "\033[0;33m"
#define red "\033[1;31m"
#define d_red "\033[0;31m"
#define cyan "\033[1;36m"
#define d_cyan "\033[0;36m"
#define magenta "\033[1;35m"
#define d_magenta "\033[0;35m"
#define bg_red_fg_white "\033[0;37;41m"
#define bg_red_fg_black "\033[0;30;41m"
#define bg_yellow_fg_black "\033[0;30;43m"
#define bg_cyan_fg_black "\033[0;46;30m"
#define bg_white_fg_red "\033[0;47;31m"
#define bg_green_fg_red "\033[0;31;42m"
#define bg_green_fg_bold_red "\033[1;31;42m"
#define bg_green_fg_black "\033[0;30;42m"
#define bg_green_fg_blue "\033[0;34;42m"
#define bg_blue_fg_white "\033[0;37;44m"
#define bg_blue_fg_red "\033[0;31;44m"
#define NC "\033[0m"
//colors for the prompt:
#define green_b "\001\033[1;32m\002"
//#define d_red_b "\001\033[0;31m\002"
//#define d_cyan_b "\001\033[0;36m\002"
#define NC_b "\001\033[0m\002"
//NOTE: do not use \001 prefix and \002 suffix for colors list: they produce a displaying
//problem in lxterminal (though not in aterm and xterm).

//###MAIN####
int main(int argc, char **argv)
{
	//###BASIC CONFIG AND VARIABLES######
	/* Save external arguments to be able to rerun external_arguments() in case the user edits
	the config file, in which case the program must rerun init_config(), get_aliases_n_prompt_cmds(),
	and then external_arguments()*/
	argc_bk=argc;
	argv_bk=xcalloc(argv_bk, (unsigned)argc, sizeof(char **));
	for (unsigned i=0;i<(unsigned)argc;i++) {
		argv_bk[i]=xcalloc(argv_bk[i], strlen(argv[i])+1, sizeof(char *));
		strcpy(argv_bk[i], argv[i]);
	}
	def_path_len=strlen(DEFAULT_PATH);
//	setlocale(LC_ALL, "");
	setlocale(LC_CTYPE, ""); /*use the locales specified by the environment. By default 
	(ANSI C), all programs start in the standard 'C' (== 'POSIX') locale. This function is
	supposed to make the program portable to all locales.*/ 
	//Running in X?
	if (getenv("DISPLAY") != NULL && strncmp(getenv("TERM"), "linux", 5) != 0)
		flags |= WINDOWED;
	/*get program invocation name. Used to prevent the user from killing the program with 
	killall*/
//	get_my_name(argv[0]);
	//get environment values
//	env_n=copy_envp(envp); //the varible **__environ already does this
	/*get paths from PATH environment variable. These paths will be used later by 
	get_path_programs (for the autocomplete function) and get_cmd_path()*/
	path_n=get_path_env();
	//get original working directory
//	getcwd(ORIGINAL_PWD, sizeof(ORIGINAL_PWD));
	//get user and host names
	if ((user=get_user()) == NULL) {
		user=xcalloc(user, 4, sizeof(char *));
		strcpy(user, "???");
	}
	if (strcmp(user, "root") == 0) flags |= ROOT_USR;	
	if (gethostname(hostname, sizeof(hostname)) == -1)
		strncpy(hostname, "???", 4);
	path=xcalloc(path, def_path_len+1, sizeof(char *));
	strcpy(path, DEFAULT_PATH); //set initial path to default path
	strncpy(old_pwd, path, strlen(path));
	init_shell();
	/*Initialize program paths and files, set options from the config file, and load sel 
	elements, if any. All these configurations are made per user basis.*/
	init_config();
	get_aliases_n_prompt_cmds();
	get_sel_files();
	get_path_programs(); /*get the list of available applications in PATH to be used by my 
	custom autocomplete function*/
	/*Manage external arguments, but only if any: argc == 1 equates to no argument, since this 
	'1' is just the progrma invokation name. External arguments will override initialization 
	values (init_config).*/
	if (argc > 1) external_arguments(argc, argv);
	exec_profile();	
	//limit the log size
	check_log_file_size();
	//Get history
	struct stat file_attrib;
	if (stat(HIST_FILE, &file_attrib) == 0 && file_attrib.st_size != 0) {
		//recover history from the history file.
		read_history(HIST_FILE); //this line adds more leaks to readline.
		//limit the size of the history file to max_hist lines.
		history_truncate_file(HIST_FILE, max_hist);
	} /*if the size condition is not included, and in case of a zero size file, the read_history
	function gives malloc errors.*/
	else {	//if the history file doesn't exist, create it.
		FILE *hist_fp=fopen(HIST_FILE, "w+");
		if (hist_fp == NULL)
			fprintf(stderr, "%s: history: %s\n", PROGRAM_NAME, strerror(errno));
		fprintf(hist_fp, "edit\n"); //Because of the above, do not create an empty file.
		fclose(hist_fp);
	}
	//store history into an array to be able to manipulate it
	get_history();
	//check whether xdg-open is available 
	xdg_open_check();
	/*enable tab auto-completion for commands (in PATH) in case of first entered word. The 
	second entered word will be autocompleted with filenames instead, just as in Bash.*/
	rl_attempted_completion_function=my_rl_completion;
	if (splash_screen) {
		splash();
		CLEAR;
	}
	if (cd_lists_on_the_fly) list_dir();
	else chdir(path);
	readline_kbinds();
	
	//###MAIN PROGRAM LOOP####
	while (1) { //or: for (;;)
		char *input=prompt(); //get input string form the prompt
		if (input) {
			char **comm=parse_input_str(input); //parse input string
			free(input);
			if (comm) {
				char **alias_comm=check_for_alias(comm);
				if (alias_comm) { /*if an alias is found, the function frees comm and returns
					alias_comm in its place to be executed by exec_cmd*/
					exec_cmd(alias_comm);
					for (int i=0;alias_comm[i];i++) free(alias_comm[i]);
					free(alias_comm);
				}
				else {
					if (!glob_cmd) { 
						exec_cmd(comm); //execute command
						for (int i=0;i<=args_n;i++) free(comm[i]);
						free(comm);
					}
				}
			}
		}
	}
	return 0; //never reached
}

//###FUNCTIONS DEFINITIONS###

void readline_kbinds(void)
{
	rl_command_func_t readline_kbind_action;
	rl_bind_keyseq("\\C-f", readline_kbind_action); //key: 6
	rl_bind_keyseq("\\C-h", readline_kbind_action); //key: 8
	rl_bind_keyseq("\\C-y", readline_kbind_action); //key: 25
	rl_bind_keyseq("\\C-r", readline_kbind_action); //key: 18
	rl_bind_keyseq("\\C-v", readline_kbind_action); //key: 18
	rl_bind_keyseq("\\C-p", readline_kbind_action); //key: 18
//	rl_bind_keyseq("\\C-e", readline_kbind_action); //key: 5
//	rl_bind_keyseq("\\C-s", readline_kbind_action); //key:
//	rl_bind_keyseq("\\C-c", readline_kbind_action); //Doesn't work
}

int readline_kbind_action (int count, int key) {
	if (count) {} //prevent Valgrind from complaining about unused variable
	printf("Key: %d\n", key);
	int status=0;
	switch (key) {
/*		case 5: //C-e
			free_stuff();
			exit(EXIT_SUCCESS);
			break;
*/
		case 6: //C-f
			status=list_folders_first;
			if (list_folders_first) list_folders_first=0;
			else list_folders_first=1;
			if (status != list_folders_first) {
				free_dirlist();
				list_dir();
			}
			break;
		case 8: //C-h
			status=show_hidden;
			if (show_hidden) show_hidden=0;
			else show_hidden=1;
			if (status != show_hidden) {
				free_dirlist();
				list_dir();
			}
			break;
		case 18: //C-r
			search_mark=0;
			free_dirlist();
			list_dir();
			get_sel_files();
			break;
		case 25: //C-y
			status=sys_shell_status;
			if (sys_shell_status) sys_shell_status=sys_shell=0;
			else sys_shell=sys_shell_status=1;
			if (status !=sys_shell_status) {
				char *input=prompt();
				free(input);
			}
			break;
	}
	rl_on_new_line();
	return 0;
}

char *parse_usrvar_value(char *str, char c)
{
	if (c == '\0' || !str) return NULL;
	size_t str_len=strlen(str);
	for (unsigned i=0;i<str_len;i++) {
		if (str[i] == c) {
			unsigned index=i+1, j=0;
			if (i == str_len-1) return NULL;
			char *f_str=NULL;
			f_str=xcalloc(f_str, (str_len-index)+1, sizeof(char *));	
			for (i=index;i<str_len;i++) {
				if (str[i] != '"' && str[i] != '\'' && str[i] != '\\' && str[i] != '\0') {
					f_str[j++]=str[i];
				}
			}
			f_str[j]='\0';
			return f_str;
		}
	}
	return NULL;
}

void create_usr_var(char *str)
{
	char *name=strbfr(str, '=');
	char *value=parse_usrvar_value(str, '=');
	if (!name) {
		if (value) free(value);
		fprintf(stderr, "%s: Error getting variable name\n", PROGRAM_NAME);
		return;
	}
	if (!value) {
		free(name);
		fprintf(stderr, "%s: Error getting variable value\n", PROGRAM_NAME);
		return;
	}
	usr_var=xrealloc(usr_var, (usrvar_n+1)*sizeof(struct usrvar_t));
	usr_var[usrvar_n].name=xcalloc(usr_var[usrvar_n].name, strlen(name)+1, sizeof(char *));
	usr_var[usrvar_n].value=xcalloc(usr_var[usrvar_n].value, strlen(value)+1, sizeof(char *));
	strcpy(usr_var[usrvar_n].name, name);
	strcpy(usr_var[usrvar_n++].value, value);
	free(name);
	free(value);
}

void free_usrvars(void)
{
	for (unsigned i=0;(int)i<usrvar_n;i++) {
		free(usr_var[i].name);
		free(usr_var[i].value);
	}
	free(usr_var);
}

void free_stuff(void)
{
	free_dirlist();
	free(dirlist);
	if (sel_n > 0) {
		for (int i=0;i<sel_n;i++) free(sel_elements[i]);
		free(sel_elements);
	}
	if (bin_commands) {
		for (int i=0;bin_commands[i]!=NULL;i++) free(bin_commands[i]);
		free(bin_commands);
	}
	if (history) {
		for (int i=0;i<current_hist_n;i++) free(history[i]);
		free(history);
	}
	if (paths) {
		for (int i=0;i<path_n;i++) free(paths[i]);
		free(paths);
	}
	if (argv_bk) {
		for (int i=0;i<argc_bk;i++) free(argv_bk[i]);
		free(argv_bk);
	}
	free_usrvars();
/*	
	if (my_envp) {
		for (int i=0;i<env_n;i++)
			free(my_envp[i]);
		free(my_envp);
	}
*/
//	if (my_invocation_name) free(my_invocation_name);
	if (flags & XDG_OPEN_OK) free(xdg_open_path);
	remove(ALIASES_FILE);
	remove(PROMPT_FILE);

//	printf("%s", NC); //restore the foreground color of the running terminal.
}

void *xrealloc(void *ptr, size_t size)
/*
Usage example:
	char **str=NULL; 
	str=xcalloc(str, 1, sizeof(char *));
	for (int i=0;i<10;i++)
		str=xrealloc(str, sizeof(char **)*(i+1));
*/
{
	if (size == 0) ++size;
	void *new_ptr=realloc(ptr, size);
	if (new_ptr == NULL) {
		printf("xrealloc: size: %ld (%p)\n", (long)size, ptr);
		new_ptr=realloc(ptr, size);
		if (new_ptr == NULL) {
			ptr=NULL;
			fprintf(stderr, "%s: %s failed to allocate %zu bytes\n", PROGRAM_NAME, __func__, 
																					  size);
			atexit(free_stuff);
		}
	}
		ptr=new_ptr;
		new_ptr=NULL;
		return ptr;
}

void *xcalloc(void *ptr, size_t nmemb, size_t size)
/*
Usage example:
	char **str=NULL;
	str=xcalloc(str, 1, sizeof(char *)); //for arrays allocation

	for (int i=0;str;i++) //to allocate elements of the array
		str[i]=xcalloc(str[i], 7, sizeof(char *)); 

	char *str=NULL;
	str=xcalloc(str, 20, sizeof(char *)); //for strings allocation
*/
{
	if (nmemb == 0) ++nmemb;
	if (size == 0) ++size;
	void *new_ptr=calloc(nmemb, size);
	if (new_ptr == NULL) {
		new_ptr=calloc(nmemb, size);
		if (new_ptr == NULL) {
			fprintf(stderr, "%s: %s failed to allocate %zu bytes\n", PROGRAM_NAME, __func__, 
																				nmemb*size);
			atexit(free_stuff);
		}
	}
	ptr=new_ptr;
	new_ptr=NULL;
	return ptr;
}

void update_path(char *new_path)
{
	free(path);
	path=xcalloc(path, strlen(new_path)+1, sizeof(char *));
	strcpy(path, new_path);
}

/*
void get_my_name(char *argv_zero)
{
	my_invocation_name=straftlst(argv_zero, '/');
	if (!my_invocation_name) { //if there's no slash in argv[0]...
		my_invocation_name=xcalloc(my_invocation_name, strlen(argv_zero), sizeof(char *));
		strcpy(my_invocation_name, argv_zero);
	}
}*/

void xdg_open_check(void)
{
	xdg_open_path=get_cmd_path("xdg-open");
	if (xdg_open_path == NULL) flags &= ~XDG_OPEN_OK;
	else flags |= XDG_OPEN_OK;
}

void set_signals_to_ignore(void)
{
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);
}

void set_signals_to_default(void)
{
	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGTSTP, SIG_DFL);
	signal(SIGTTIN, SIG_DFL);
	signal(SIGTTOU, SIG_DFL);
	signal(SIGCHLD, SIG_DFL);
}

void init_shell(void)
/* Keep track of attributes of the shell. Make sure the shell is running interactively as 
the foreground job before proceeding.*/
{ //REPLACED shell_pgid by own_pid
	//See if we are running interactively.
	shell_terminal=STDIN_FILENO;
	shell_is_interactive=isatty(shell_terminal); //isatty returns 1 if the file descriptor, 
	//here STDIN_FILENO, outputs to a terminal. Otherwise, it returns zero. 
	if (shell_is_interactive) {
		//Loop until we are in the foreground.
		while (tcgetpgrp(shell_terminal) != (own_pid=getpgrp()))
			kill(- own_pid, SIGTTIN);
		//Ignore interactive and job-control signals.
		set_signals_to_ignore();
		//Put ourselves in our own process group.
		own_pid=get_own_pid();
		//make the shell gpid (group id) equal to its pid
		/*The lines below prevent CliFM from being called as argument for a terminal, ex:
		xterm -e /path/to/clifm. However, without them input freezes when running as root*/
//		if (strncmp(user, "root", 4) == 0) {
		if (flags & ROOT_USR) {
			if (setpgid(own_pid, own_pid) < 0) {
				fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
				atexit(free_stuff);
			}
		}
		//Grab control of the terminal.
		tcsetpgrp(shell_terminal, own_pid);
		//Save default terminal attributes for shell.
		tcgetattr(shell_terminal, &shell_tmodes);
	}
}

/*void signal_handler(int sig_num)
//handle signals catched by the signal function. In this case, the function just prompts a 
//new line. If trap signals are not enabled, kill the current foreground process (pid)
{
	//if Ctrl-C and pid is not ClifM's pid, kill the current foreground process AND its childs
	//(-pid).
	if (sig_num == SIGINT && pid != own_pid) {
		if (kill(-pid, SIGTERM) != -1)
			printf("CliFM: %d: Terminated\n", pid);
		else
			perror("CliFM: kill");
	}
}*/

void get_path_programs(void)
/*Get the list of files in PATH and send them into an array to be read by my readline 
custom auto-complete function (my_rl_completion)*/
{
	struct dirent **commands_bin[path_n];
	int	i, j, l=0, cmd_n[path_n], total_cmd=0;
	for (int k=0;k<path_n;k++) {
		cmd_n[k]=scandir(paths[k], &commands_bin[k], NULL, alphasort);
		total_cmd+=cmd_n[k];
	}
	//add internal commands
	bin_commands=xcalloc(bin_commands, total_cmd+internal_cmd_n+2, sizeof(char **));
	for (i=0;i<internal_cmd_n;i++) {
		bin_commands[l]=xcalloc(bin_commands[l], strlen(INTERNAL_CMDS[i])+3, sizeof(char *));
		strcpy(bin_commands[l++], INTERNAL_CMDS[i]);		
	}
	//add commands in PATH
	for (i=0;i<path_n;i++) {
 		for (j=0;j<cmd_n[i];j++) {
			bin_commands[l]=xcalloc(bin_commands[l], strlen(commands_bin[i][j]->d_name)+3, 
																		  sizeof(char *));
			strcpy(bin_commands[l++], commands_bin[i][j]->d_name);
			free(commands_bin[i][j]);
		}
		free(commands_bin[i]);
	}
}

char **my_rl_completion(const char *text, int start, int end)
{
	char **matches=NULL;
	if (end) {} //this line only prevents a Valgrind warning about unused variables
	if (start == 0) { //only for the first word entered in the path
//		rl_attempted_completion_over=1;
		matches=rl_completion_matches(text, &bin_cmd_generator);
	}
	return matches;
}

char *bin_cmd_generator(const char *text, int state)
{
	static int i, len;
	char *name;
	if (!state) {
		i=0;
		len=strlen(text);
	}
	//look for files in PATH for a match
	while ((name=bin_commands[i++])!=NULL) {
		if (strncmp(name, text, len) == 0)
			return strdup(name);
	}
	return NULL;
}

/*
//The variable 'char **__environ', contained in unistd.h does exactly the same thing. If you
// define _GNU_SOURCE at the beginning of the code, you can use char **environ instead.
int copy_envp(char **envp)
//copy the environment variable (envp) to a globaly declared array (my_envp) so that 
//environment variables will be accessible from any part of the program
{
	int i;
	for(i=0;envp[i]!=NULL;i++) {
		my_envp=xrealloc(my_envp, sizeof(char **)*(i+1));
		my_envp[i]=xcalloc(my_envp[i], (strlen(envp[i])+1), sizeof(char *));
		strncpy(my_envp[i], envp[i], strlen(envp[i]));
	}
	//add the final NULL required by execve
	my_envp=xrealloc(my_envp, sizeof(char **)*(i+1));
	my_envp[i]=NULL;	
	return i; //return the amount of environment variables
}*/

int get_path_env(void)
//send all paths of PATH environment variable to a globally declared array (paths)
{
	unsigned i=0;
	char *path_tmp=NULL;
	for (i=0;__environ[i]!=NULL;i++) {
		if (strncmp(__environ[i], "PATH", 4) == 0) {
			path_tmp=straft(__environ[i], '=');
			break;
		}
	}
	if (!path_tmp) return 0;
	int path_n=0;// length=0;
	for (i=0;path_tmp[i];i++) {
		paths=xrealloc(paths, sizeof(char **)*(path_n+1));
		paths[path_n]=xcalloc(paths[path_n], 256, sizeof(char *)); //should be PATH_MAX
		int length=0;
		while (path_tmp[i] && path_tmp[i]!=':') {
			paths[path_n][length++]=path_tmp[i++];
		}
		path_n++;
	}
	free(path_tmp);
	return path_n;
}

void splash(void)
{
	printf("\n%s                         xux\n", d_cyan);
	printf("       :xuiiiinu:.......u@@@u........:xunninnu;\n");
	printf("    .xi#@@@@@@@@@n......x@@@l.......x#@@@@@@@@@:...........:;unnnu;\n");
	printf("  .:i@@@@lnx;x#@@i.......l@@@u.....x#@@lu;:;;..;;nnll#llnnl#@@@@@@#u.\n");
	printf("  .i@@@i:......::........;#@@#:....i@@@x......;@@@@@@@@@@@@@#iuul@@@n.\n");
	printf("  ;@@@#:..........:nin:...n@@@n....n@@@nunlll;;@@@@i;:xl@@@l:...:l@@@u.\n");
	printf("  ;#@@l...........x@@@l...;@@@#:...u@@@@@@@@@n:i@@@n....i@@@n....;#@@#;.\n");
	printf("  .l@@@;...........l@@@x...i@@@u...x@@@@iux;:..;#@@@x...:#@@@;....n@@@l.\n");
	printf("  .i@@@x...........u@@@i...;@@@l....l@@@;.......u@@@#:...;nin:.....l@@@u.\n");
	printf("  .n@@@i:..........:l@@@n...xnnx....u@@@i........i@@@i.............x@@@#:\n");
	printf("   :l@@@i...........:#@@@;..........:@@@@x.......:l@@@u.............n@@@n.\n");
	printf("    :l@@@i;.......unni@@@#:.:xnlli;..;@@@#:.......:l@@u.............:#@@n.\n");
	printf("     ;l@@@@#lnuxxi@@@i#@@@##@@@@@#;...xlln.         :.                ;:.\n");
	printf("      :xil@@@@@@@@@@l:u@@@@##lnx;.\n");
	printf("         .:xuuuunnu;...;ux;.\n");
	printf("\n                     The anti-eye-candy/KISS file manager\n%s", NC);
	printf("\n                       Press Enter key to continue... ");
	getchar();
}

void init_config(void)
//set up CliFM directories and config files. Load the user's configuration from clifmrc
{
	//set up program's directories and files (always per user).
	snprintf(CONFIG_DIR, def_path_len+15, "%s/.config/clifm", DEFAULT_PATH);
	size_t conf_dir_len=strlen(CONFIG_DIR);
	snprintf(BM_FILE, conf_dir_len+15, "%s/bookmarks.cfm", CONFIG_DIR);
	snprintf(LOG_FILE, conf_dir_len+9, "%s/log.cfm", CONFIG_DIR);
	snprintf(LOG_FILE_TMP, conf_dir_len+13, "%s/log_tmp.cfm", CONFIG_DIR);
	snprintf(HIST_FILE, conf_dir_len+13, "%s/history.cfm", CONFIG_DIR);
	snprintf(CONFIG_FILE, conf_dir_len+9, "%s/clifmrc", CONFIG_DIR);	
	snprintf(PROFILE_FILE, conf_dir_len+16, "%s/clifm_profile", CONFIG_DIR);
	size_t user_len=strlen(user);
	size_t tmp_dir_len=strlen(TMP_DIR);
	snprintf(BK_DIR, tmp_dir_len+user_len+11, "%s/clifm_bk_%s", TMP_DIR, user);
	snprintf(ALIASES_FILE, tmp_dir_len+user_len+25, "%s/.clifm_alias_%s_%d", TMP_DIR, user, get_own_pid());
	snprintf(PROMPT_FILE, tmp_dir_len+user_len+21, "%s/.clifm_prompt_cmds_%s", TMP_DIR, user);

	//Define the user's sel file. There will be one per user (/tmp/clifm/sel_file_username).
	snprintf(sel_file_user, strlen(TMP_DIR)+13+user_len, "%s/.clifm_sel_%s", TMP_DIR, user);

	//if the config directory doesn't exist, create it.
	struct stat file_attrib;
	if (stat(CONFIG_DIR, &file_attrib) == -1) {
		if (mkdir(CONFIG_DIR, S_IRWXU) == -1) {
			perror("mkdir");
			return;
		}
	}

	//if the temporary directory doesn't exist, create it.
	if (stat(TMP_DIR, &file_attrib) == -1) {
		if (mkdir(TMP_DIR, S_IRWXU) == -1) {
			perror("mkdir");
			return;
		}
	}

	//open the profile file or create it, if it doesn't exist.
	if (stat(PROFILE_FILE, &file_attrib) == -1) { //if file doesn't exist, create it.	
		FILE *profile_fp=fopen(PROFILE_FILE, "w");
		if (!profile_fp) {
			fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
			return;
		}
		else {
			fprintf(profile_fp, "#%s profile\n", PROGRAM_NAME);
			fprintf(profile_fp, "#Write here the commands you want to be executed at startup\n");			
			fprintf(profile_fp, "#Ex:\n#echo -e \"%s\"\n", WELCOME_MSG);
			fclose(profile_fp);
		}
	}

	//open the config file or create it, if it doesn't exist.
	FILE *config_fp;
	if (stat(CONFIG_FILE, &file_attrib) == -1) { //if file doesn't exist, create it.
		config_fp=fopen(CONFIG_FILE, "w");
		if (config_fp == NULL) {
			fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
			return;
		}
		else {
			fprintf(config_fp, "%s configuration file\n", PROGRAM_NAME);
			fprintf(config_fp, "########################\n\n");						
			fprintf(config_fp, "Splash screen=false\n");
			fprintf(config_fp, "Welcome message=true\n");
			fprintf(config_fp, "Use system shell=true\n");
			fprintf(config_fp, "Show hidden files=false\n");
			fprintf(config_fp, "Backup deleted files=false\n");
			fprintf(config_fp, "List folders first=false\n");
			fprintf(config_fp, "cd lists automatically=false\n");
			fprintf(config_fp, "Case sensitive list=false\n");
			fprintf(config_fp, "Prompt color=6\n");
			fprintf(config_fp, "#0: black; 1: red; 2: green; 3: yellow; 4: blue;\n");
			fprintf(config_fp, "#5: magenta; 6: cyan; 7: white; 8: default terminal color\n");
			fprintf(config_fp, "Max history=500\n");
			fprintf(config_fp, "Max log=1000\n");
			fprintf(config_fp, "Clear screen=false\n");
			fprintf(config_fp, "Starting path=default\n");
			fprintf(config_fp, "#Default starting path is HOME\n");
			fprintf(config_fp, "#END OF OPTIONS\n\n");
			fprintf(config_fp, "\n###Aliases###\nalias ls='ls --color=auto -A'\n");
			fprintf(config_fp, "\n#PROMPT\n");
			fprintf(config_fp, "#Write below the commands you want to be executed before the prompt \
\n#Ex: \
\n#;date | awk '{print $1\", \"$2,$3\", \"$4}'\n\n#END OF PROMPT");
			fclose(config_fp);
		}
	}

	//Read the config file
	if ((config_fp=fopen(CONFIG_FILE, "r")) != NULL) {
		char line[MAX_LINE]="";
		while (fgets(line, sizeof(line), config_fp)) {
			if (strncmp(line, "#END OF OPTIONS", 15) == 0) break;
			else if (strncmp(line, "Splash screen=", 14) == 0) {
				char opt_str[MAX_LINE+1]="";
				int ret=sscanf(line, "Splash screen=%5s\n", opt_str);
				/*According to cppcheck: "sscanf() without field width limits can crash with 
				huge input data". Field width limits = %5s*/
				if (ret == -1) continue;
				if (strncmp(opt_str, "true", 4) == 0)
					splash_screen=1;
				else if (strncmp(opt_str, "false", 5) == 0)
					splash_screen=0;
				else //Default value (if option was found but value neither true nor false)
					splash_screen=0;
			}
			else if (strncmp(line, "Welcome message=", 16) == 0) {
				char opt_str[MAX_LINE+1]="";
				int ret=sscanf(line, "Welcome message=%5s\n", opt_str);
				if (ret == -1) continue;
				if (strncmp(opt_str, "true", 4) == 0)
					welcome_message=1;
				else if (strncmp(opt_str, "false", 5) == 0)
					welcome_message=0;
				else
					welcome_message=1;
			}
			else if (strncmp(line, "Use system shell=", 17) == 0) {
				char opt_str[MAX_LINE+1]="";
				int ret=sscanf(line, "Use system shell=%5s\n", opt_str);
				if (ret == -1) continue;
				if (strncmp(opt_str, "true", 4) == 0)
					sys_shell=sys_shell_status=1;
				else if (strncmp(opt_str, "false", 5) == 0)
					sys_shell=sys_shell_status=0;
				else sys_shell=sys_shell_status=0;
			}
			else if (strncmp(line, "Clear screen=", 13) == 0) {
				char opt_str[MAX_LINE+1]="";
				int ret=sscanf(line, "Clear screen=%5s\n", opt_str);
				if (ret == -1) continue;
				if (strncmp(opt_str, "true", 4) == 0)
					clear_screen=1;
				else if (strncmp(opt_str, "false", 5) == 0)
					clear_screen=0;
				else
					clear_screen=0;
			}
			else if (strncmp(line, "Show hidden files=", 18) == 0) {
				char opt_str[MAX_LINE+1]="";
				int ret=sscanf(line, "Show hidden files=%5s\n", opt_str);
				if (ret == -1) continue;
				if (strncmp(opt_str, "true", 4) == 0)
					show_hidden=1;
				else if (strncmp(opt_str, "false", 5) == 0)
					show_hidden=0;
				else
					show_hidden=0;
			}
			else if (strncmp(line, "List folders first=", 19) == 0) {
				char opt_str[MAX_LINE+1]="";
				int ret=sscanf(line, "List folders first=%5s\n", opt_str);
				if (ret == -1) continue;
				if (strncmp(opt_str, "true",4) == 0)
					list_folders_first=1;
				else if (strncmp(opt_str, "false", 5) == 0)
					list_folders_first=0;
				else
					list_folders_first=0;
			}
			else if (strncmp(line, "cd lists automatically=", 23) == 0) {
				char opt_str[MAX_LINE+1]="";
				int ret=sscanf(line, "cd lists automatically=%5s\n", opt_str);
				if (ret == -1) continue;
				if (strncmp(opt_str, "true", 4) == 0)
					cd_lists_on_the_fly=1;
				else if (strncmp(opt_str, "false", 5) == 0)
					cd_lists_on_the_fly=0;
				else
					cd_lists_on_the_fly=0;
			}
			else if (strncmp(line, "Case sensitive list=", 20) == 0) {
				char opt_str[MAX_LINE+1]="";
				int ret=sscanf(line, "Case sensitive list=%5s\n", opt_str);
				if (ret == -1) continue;
				if (strncmp(opt_str, "true", 4) == 0)
					case_sensitive=1;
				else if (strncmp(opt_str, "false", 5) == 0)
					case_sensitive=0;
				else
					case_sensitive=0;
			}
			else if (strncmp(line, "Prompt color=", 13) == 0) {
				int opt_num=0;
				sscanf(line, "Prompt color=%d\n", &opt_num);
				if (opt_num <= 0) continue;
				prompt_color_set=1;
				switch (opt_num) {
					case 0: strcpy(prompt_color, "\001\033[0;30m\002"); break;
					case 1: strcpy(prompt_color, "\001\033[0;31m\002"); break;
					case 2: strcpy(prompt_color, "\001\033[0;32m\002"); break;
					case 3: strcpy(prompt_color, "\001\033[0;33m\002"); break;
					case 4: strcpy(prompt_color, "\001\033[0;34m\002"); break;
					case 5: strcpy(prompt_color, "\001\033[0;35m\002");	break;
					case 6: strcpy(prompt_color, "\001\033[0;36m\002");	break;
					case 7: strcpy(prompt_color, "\001\033[0;37m\002");	break;
					case 8: strcpy(prompt_color, "\001\033[0m\002"); break;
					default: strcpy(prompt_color, "\001\033[0;36m\002");
				}
			}
			else if (strncmp(line, "Backup deleted files=", 21) == 0) {
				char opt_str[MAX_LINE+1]="";
				int ret=sscanf(line, "Backup deleted files=%5s\n", opt_str);
				if (ret == -1) continue;
				if (strncmp(opt_str, "true", 4) == 0)
					backup=1;
				else if (strncmp(opt_str, "false", 5) == 0)
					backup=0;
				else
					backup=0;
			}
			else if (strncmp(line, "Max history=", 12) == 0) {
				int opt_num=0;
				sscanf(line, "Max history=%d\n", &opt_num);
				if (opt_num <= 0) continue;
				max_hist=opt_num;
			}
			else if (strncmp(line, "Max log=", 8) == 0) {
				int opt_num=0;
				sscanf(line, "Max log=%d\n", &opt_num);
				if (opt_num <= 0) continue;
				max_log=opt_num;
			}
			else if (strncmp(line, "Starting path=", 14) == 0) {
				char opt_str[MAX_LINE+1]="";
				int ret=sscanf(line, "Starting path=%4096s\n", opt_str);				
				if (ret == -1) continue;
				if (strncmp(opt_str, "default", 7) != 0)
					if (stat(opt_str, &file_attrib) == 0)
						if (file_attrib.st_mode & S_IFDIR)
							if (access(opt_str, R_OK|X_OK) == 0)
								update_path(opt_str);
/*							}
							else { 
								CLEAR;
								fflush(stdout);
								fprintf(stderr, "CliFM: %s: Access denied: starting path set \
to default\n", opt_str);
								printf("Press Enter key to continue... ");
								getchar();
							}
						}
						else {
							CLEAR;
							fflush(stdout);
							fprintf(stderr, "CliFM: %s: Is not a directory: starting path set \
to default\n", opt_str);
							printf("Press Enter key to continue... ");
							getchar();
						}
					}
					else {
						CLEAR;
						fflush(stdout);
						fprintf(stderr, "CliFM: %s: No such file or directory: starting path \
set to default\n", opt_str);
						printf("Press Enter key to continue... ");
						getchar();
					}
				}*/
			}
		}
		fclose(config_fp);
	}
	else fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));

	//if, for some reason, some option was not found in the config file, set the defaults.
	if (splash_screen == -1) splash_screen=0; //-1 means not set
	if (welcome_message == -1) welcome_message=1;
	if (sys_shell == -1) sys_shell=sys_shell_status=0;
	if (show_hidden == -1) show_hidden=0;
	if (backup == -1) backup=0;
	if (max_hist == -1) max_hist=500;
	if (max_log == -1) max_log=1000;
	if (clear_screen == -1) clear_screen=0;
	if (list_folders_first == -1) list_folders_first=0;
	if (cd_lists_on_the_fly == -1) cd_lists_on_the_fly=0;	
	if (case_sensitive == -1) case_sensitive=0;
	if (prompt_color_set == -1) 
		strcpy(prompt_color, "\001\033[0;36m\002");
}

void exec_profile(void)
{
	struct stat file_attrib;
	if (stat(PROFILE_FILE, &file_attrib) == 0) {
		FILE *fp=fopen(PROFILE_FILE, "r");
		if (fp) {
			char line[MAX_LINE]="";
			while(fgets(line, sizeof(line), fp)) {
				if (strcntchr(line, '=') != -1 && !isdigit(line[0])) {
					if (!sys_shell)
						create_usr_var(line);
					else continue;
				}
				if (strlen(line) != 0 && line[0] != '#') {
					args_n=0;
					char **cmds=parse_input_str(line);
					if (cmds) {
						no_log=1;
						exec_cmd(cmds);
						no_log=0;
						for (unsigned i=0;i<=(unsigned)args_n;i++)
							free(cmds[i]);
						free(cmds);
					}
					args_n=0;
				}
			}
			fclose(fp);
		}
	}
}

char *get_cmd_path(char *cmd)
/*get the path of a given command from the PATH environment variable. It basically does the
same as the 'command' built-in Linux command*/
{
	char *cmd_path=NULL;
	cmd_path=xcalloc(cmd_path, PATH_MAX, sizeof(char *));
	for (int i=0;i<path_n;i++) { //get the path from PATH env variable
		snprintf(cmd_path, PATH_MAX, "%s/%s", paths[i], cmd);
		if (access(cmd_path, F_OK) == 0) return cmd_path;
	}
	free(cmd_path);
	return NULL;
}

void external_arguments(int argc, char **argv)
//evaluate external arguments, if any, and change initial variables to its corresponding value.
{
//Link long (--option) and short options (-o) for the getopt_long function
	opterr=optind=0; /*disable automatic error messages to be able to handle them myself via the '?'
	case in the switch*/
	static struct option longopts[]={
		{"hidden", no_argument, 0, 'A'},
		{"backup", no_argument, 0, 'b'},
		{"command", required_argument, 0, 'c'},
		{"folders-first", no_argument, 0, 'f'},
		{"help", no_argument, 0, 'h'},
		{"case-sensitive", no_argument, 0, 's'},
		{"list-on-the-fly", no_argument, 0, 'l'},
		{"starting-path", required_argument, 0, 'p'},
		{"system", no_argument, 0, 'S'},
		{"version", no_argument, 0, 'v'},
		{"splash", no_argument, 0, 'x'},
		{0, 0, 0, 0}
	};
	int optc;
	char *cmd_value=NULL, *path_value=NULL; //store arguments to options -c and -p
	while ((optc=getopt_long(argc, argv, "+Abc:fhlp:sSvx", longopts, (int *)0)) != EOF) {
		/*':' and '::' is the short options string means required argument and optional argument
		respectivelly. The plus char (+) tells getopt to stop processing at the first 
		non-option*/
		switch (optc) {
			case 'A':
				flags |= HIDDEN;
				show_hidden=1;
				break;
			case 'b':
				flags |= BACKUP_OK; //add flag BACKUP_OK to 'flags'
				backup=1;
				break;
			case 'c':
				flags |= COMM;
				cmd_value=optarg;
				break;
			case 'f':
				flags |= FOLDERS_FIRST;
				list_folders_first=1;
				break;
			case 'h':
				flags |= HELP;
				flags |= EXT_HELP; //do not display "Press Enter to continue"
				help_function();
				exit(EXIT_SUCCESS);
			case 'l':
				flags |= ON_THE_FLY;
				cd_lists_on_the_fly=1;
				break;
			case 'p':
				flags |= START_PATH;
				path_value=optarg;
				break;
			case 's':
				flags |= CASE_SENS;
				case_sensitive=1;
				break;
			case 'S':
				sys_shell=sys_shell_status=1;
				break;
			case 'v':
				flags |= PRINT_VERSION;
				version_function(); license();
				exit(EXIT_SUCCESS);
			case 'x':
				flags |= SPLASH;
				splash_screen=1;
				break;
			case '?': //if some unrecognized option is found...
				if (optopt == 'c' || optopt == 'p') //options that require an argument
					fprintf(stderr, "%s: option -%c requires an argument\n", PROGRAM_NAME, 
																				  optopt);
				else if (isprint(optopt)) //if unknown option is printable...
					fprintf(stderr, "%s: invalid option -- '%c'\nUsage: clifm [-AbfhlsSv] \
[-c command] [-p path]\nTry 'clifm --help' for more information.\n", PROGRAM_NAME, optopt);
				else fprintf (stderr, "%s: unknown option character '\\x%x'\n", PROGRAM_NAME, 
																					 optopt);
				exit(EXIT_FAILURE);
		}
	}
	if (flags & COMM) {
		char **cmd_args=get_substr(cmd_value, ' ');
/*		char *cmd_path=NULL;
		if (strcntchr(cmd_args[0], '/') == -1)
			cmd_path=get_cmd_path(cmd_args[0]);
		else {
			cmd_path=xcalloc(cmd_path, strlen(cmd_args[0]), sizeof(char *));
			strcpy(cmd_path, cmd_args[0]);
		}
		if (!cmd_path) {
			fprintf(stderr, "CliFM: %s: Command not found\n", cmd_args[0]);
			for (unsigned i=0;cmd_args[i];i++) free(cmd_args[i]);
			free(cmd_args);
			exit(EXIT_FAILURE);
		}
*/
		pid_t pid=fork();
		if (pid == 0) {
			set_signals_to_default();
			if (execvp(cmd_args[0], cmd_args) == -1) {
				fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, cmd_args[0], strerror(errno));
				exit(EXIT_FAILURE);
			}
			set_signals_to_ignore();
		}
		else waitpid(pid, NULL, 0);
		for (unsigned i=0;cmd_args[i];i++) free(cmd_args[i]);
		free(cmd_args);
//		free(cmd_path);
		exit(EXIT_SUCCESS);
	}
	if (flags & START_PATH) {
		struct stat file_attrib;
		if (stat(path_value, &file_attrib) == 0) {
			//if a directory...
			if (file_attrib.st_mode & S_IFDIR) {
				//if the directory has appropriate permissions (read, execute)...
				if (access(path_value, R_OK|X_OK) == 0)
					//overwrite the starting path of the config file.
					update_path(path_value);
				else {
					fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, path_value, strerror(errno));
					exit(EXIT_FAILURE);
				}
			}
			else {
				fprintf(stderr, "%s: %s: Not a directory\n", PROGRAM_NAME, path_value);
				exit(EXIT_FAILURE);
			}
		}
		//if non-existent path...
		else if (strcntchr(path_value, '/') != -1) {
			fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, path_value, strerror(errno));
			exit(EXIT_FAILURE);
		}
	}
}

char *home_tilde(char *path)
//Replace "/home/user" by "~/".
{
	//if path != home dir...
	char *path_tilde=NULL;	
	size_t path_len=strlen(path);
	if (strncmp(path, DEFAULT_PATH, path_len) != 0) {
		char *str_aft=NULL;
		str_aft=xcalloc(str_aft, path_len, sizeof(char *));
		int j=0;
		for (unsigned i=def_path_len+1;i<path_len;i++)
			str_aft[j++]=path[i];
		path_tilde=xcalloc(path_tilde, path_len, sizeof(char *));		
		snprintf(path_tilde, path_len, "~/%s", str_aft);
		free(str_aft);
	}
	else {
		path_tilde=xcalloc(path_tilde, 2, sizeof(char *));		
		strcpy(path_tilde, "~");
	}
	return path_tilde;
}

int brace_expansion(char *str)
{
	size_t str_len=strlen(str);
	int j=0, k=0, l=0, initial_brace=0, closing_brace=-1;
	char *braces_root=NULL, *braces_end=NULL;
	for (int i=0;i<(int)str_len;i++) {
		if (str[i] == '{') {
			initial_brace=i;
			braces_root=xcalloc(braces_root, i+1, sizeof(char *));			
			for (j=0;j<i;j++) //if any, store the string before the beginning of the braces
				braces_root[j]=str[j];
		}
		//if any, store the string after the closing brace
		if (initial_brace && str[i] == '}') {
			closing_brace=i;
			if ((str_len-1)-closing_brace > 0) {
				braces_end=xcalloc(braces_end, (str_len-1)-closing_brace, sizeof(char *));
				for (k=closing_brace+1;str[k]!='\0';k++)
					braces_end[l++]=str[k];
			}
		break;
		}
	}
	if (closing_brace == -1) {
		if (braces_root) free(braces_root);
		return 0;
	}
	k=0;
	int braces_n=0;
	braces=xcalloc(braces, braces_n+1, sizeof(char *));
	for (int i=(int)j+1;i<closing_brace;i++) {
		char brace_tmp[closing_brace-initial_brace];
		memset(brace_tmp, '\0', closing_brace-initial_brace+1);
		while (str[i] != '}' && str[i] != ',' && str[i] != '\0')
			brace_tmp[k++]=str[i++];
		if (braces_end) {
			braces[braces_n]=xcalloc(braces[braces_n], strlen(braces_root)+strlen(brace_tmp)+
														 strlen(braces_end), sizeof(char *));
			sprintf(braces[braces_n], "%s%s%s", braces_root, brace_tmp, braces_end);
		}
		else {
			braces[braces_n]=xcalloc(braces[braces_n], strlen(braces_root)+strlen(brace_tmp), 
																			 sizeof(char *));
			sprintf(braces[braces_n], "%s%s", braces_root, brace_tmp);
		}
		if (str[i] == ',')
			braces=xrealloc(braces, (++braces_n+1)*sizeof(char **));
		k=0;
	}
//	braces=realloc(braces, (++braces_n+1)*sizeof(*braces));
//	braces[braces_n]=NULL;
	if (braces_root) free(braces_root);
	if (braces_end) free(braces_end);
	return braces_n+1;
}

char **parse_input_str(char *str)
/*take the string stored by readline and get its substrings without spaces. It accepts
both single and double quotes, just as '\' for file names containing spaces (stores the 
quoted file name in one single variable). In case of pipe or stream redirection, it will
return the original string without modification to be parsed later by the corresponding 
function*/
/*NOTE: though filenames could consist of everything except of slash and null characters, 
POSIX.1 recommends restricting filenames to consist of the following characters: letters (a-z,
A-Z), numbers (0-9), period (.), dash (-), and underscore ( _ ).*/
{
	char *string_b=handle_spaces(str);
	if (!string_b) return NULL; /*handle_spaces() removes leading, terminating, and double 
	spaces from str. It will return NULL if str: a) contains only spaces; b) is NULL; 
	c) is empty*/
	size_t str_len=strlen(string_b); /*run strlen just once in order not to do it once and 
	again, possibily hundreds of times, in a loop*/
	char **comm_array=NULL;
	/*if pipes or stream redirection... just return the whole string to be parsed by the 
	pipes or stream_redir function*/
	unsigned i=0;
	/*if invoking the system shell via ';' send the whole string to exec_cmd to be executed by 
	system()*/
	if (string_b[0] == ';') sys_shell=1;
	short space_found=0;
	for (i=0;i<str_len;i++) {
		switch(string_b[i]) {
			case ';':
				if (i != 0) flags |= CONC_CMD; 
				break;
			case '|': flags |= IS_PIPE; break;
			case '>': flags |= STREAM_REDIR; break;
			case '=':
				if (!sys_shell) {
					for (unsigned j=0;j<i;j++) {
						/*If there are no spaces before '=', take it as a variable. This check is
						done in order to avoid taking as a variable things like: 'ls -color=auto'*/
						if (isspace(string_b[j])) space_found=1;
					}
					if (!space_found && !isdigit(string_b[0])) flags |= IS_USRVAR_DEF;
				}
				break;
		}
	}
	if ((flags & (CONC_CMD|IS_PIPE|STREAM_REDIR|IS_USRVAR_DEF)) || (sys_shell && string_b[0] == ';')) {
		comm_array=xcalloc(comm_array, 1, sizeof(char **));
		comm_array[args_n]=xcalloc(comm_array[args_n], str_len+1, sizeof(char *));
		strncpy(comm_array[args_n], string_b, str_len); //args_n is here always zero
		free(string_b);
		return comm_array;
	}
	//if neither null string nor pipe nor stream redirection...
	//Check for simple or double quotes
	unsigned length=0;
	char buf[PATH_MAX]="";
	int braces_index=-1;
	for (i=0;i<=str_len;i++) {
		if (string_b[i] == '"' || string_b[i] == '\'') {
			unsigned first_quote_index=i;
			char quotes;
			if (string_b[i] == '"') quotes='"';
			else quotes='\'';
			unsigned j;
			for (j=i+1;string_b[j]!=quotes && string_b[j]!='\0' && length < sizeof(buf);j++) {
			//the last condition (length < sizeof(buff)) is aimed to prevent buffer overflow.
				buf[length++]=string_b[j];
				i=j+1;
			}
			if (string_b[j] != quotes) { //if no ending quotation mark...
				fprintf(stderr, "%s: Missing %s '%c'\n", PROGRAM_NAME, (first_quote_index == 
					str_len-1 && str_len-1 != 0) ? "initial" : "ending", quotes);
				free(string_b);
				if (comm_array) { //in case something has been already dumped into comm_array
					/*Do not take the last argument into account (i<args_n, and not i<=args_m), 
					for this argument will not be copied precisely because some quote is 
					missing*/
					for (int i=0;i<args_n;i++) free(comm_array[i]);
					free(comm_array);
				}
				return NULL;
			}
		}
		/*if '\' and next is space, dump the space into the buffer and increment string_b 
		index. At the next loop, index will be located at the char next to last space
			0 1 2 3 4 5 6
			t e s t \   x
			        ^
		We're now at index 4. Copy string_b[index 5] (string_b[++i]) and the for loop will
		increment the index again, so that on the next loop we will be at index 6. 
		*/
		//Check for backslashes to deal with filenames containing spaces
		else if (string_b[i] == '\\' && isspace(string_b[i+1]))
			buf[length++]=string_b[++i];
		//if not space, not null, and the buffer is not full, dump it into the buffer.
		/*NOTE: isspace() checks for white-space chars: space, \t, \n\, \v, and \f. So, null
		does not count as space.*/
		else if (!(isspace(string_b[i])) && string_b[i] != '\0' && (unsigned)length < sizeof(buf)) {
			buf[length++]=string_b[i];
			//store the index of the braced parameter, if any
			if (string_b[i] == '{') braces_index=args_n;
		}
		//if space or null or buffer is full...
		//and next one is not space and not end of the string ...
		else if (string_b[i] == '\0' || (isspace(string_b[i]) && !(isspace(string_b[i+1])))) { //&& string_b[i+1] != '\0') {
			comm_array=xrealloc(comm_array, sizeof(char **)*(args_n+1));
			comm_array[args_n]=xcalloc(comm_array[args_n], length+1, sizeof(char));
			strncpy(comm_array[args_n], buf, length);
			memset(buf, '\0', length); //clean the buffer
			length=0; //reset length for the next argument
			if (string_b[i] == '\0') break; //end of input
			else args_n++;
		}
	}
	free(string_b);
	if (braces_index != -1) { //if there is some braced parameter...
		//(we already know the index of the braced parameter (braces_index)
		//Expand the braced parameter and store it into a new array
		int braced_args=brace_expansion(comm_array[braces_index]);
		//Now we also know how many elements the expanded braced parameter has.
		if (braced_args) {
			int i=0;
			//create an array large enough to store parameters plus expanded braces
			char **comm_array_braces=NULL;
			comm_array_braces=xcalloc(comm_array_braces, args_n+braced_args, sizeof(char *));
			//First, add to the new array, the paramenters coming before braces
			for (i=0;i<braces_index;i++) {
				comm_array_braces[i]=xcalloc(comm_array_braces[i], strlen(comm_array[i]), 
																	     sizeof(char *));
				strcpy(comm_array_braces[i], comm_array[i]);
			}
			//Now, add the expanded braces to the same array
			for (int j=0;j<braced_args;j++) {
				comm_array_braces[i]=xcalloc(comm_array_braces[i], strlen(braces[j]), 
																	 sizeof(char *));
				strcpy(comm_array_braces[i++], braces[j]);
				free(braces[j]);
			}
			free(braces);
			//finally,add, if any, those parameters coming after the braces
			for (int j=braces_index+1;j<=args_n;j++) {
				comm_array_braces[i]=xcalloc(comm_array_braces[i], strlen(comm_array[j]), 
																		 sizeof(char *));
				strcpy(comm_array_braces[i++], comm_array[j]);				
			}
			/*Now, free the old comm_array and copy to it our new array containing all the
			parameters, including the expanded braces*/
			for (int j=0;j<=args_n;j++)
				free(comm_array[j]);
			comm_array=xrealloc(comm_array, (args_n+braced_args+1)*sizeof(char **));			
			args_n=i-1;
			for (int j=0;j<i;j++) {
				comm_array[j]=xcalloc(comm_array[j], strlen(comm_array_braces[j]), sizeof(char *));
				strcpy(comm_array[j], comm_array_braces[j]);
				free(comm_array_braces[j]);
			}
			free(comm_array_braces);
		}
	}
	//Add a terminating NULL string to the array
	comm_array=xrealloc(comm_array, sizeof(char **)*(args_n+2));	
	comm_array[args_n+1]=NULL;

	//this should go in a different function
	for (i=0;i<=(unsigned)args_n;i++) { //replace "~/" by "/home/user"
		if (strncmp(comm_array[i], "~/", 2) == 0) {
			char *path_no_tilde=straft(comm_array[i], '/');
			if (path_no_tilde) {
				comm_array[i]=xrealloc(comm_array[i], (def_path_len+
								   strlen(path_no_tilde)+2)*sizeof(char *));
				sprintf(comm_array[i], "%s/%s", DEFAULT_PATH, path_no_tilde);
			}
			else {
				comm_array[i]=xrealloc(comm_array[i], (def_path_len+2)*sizeof(char *));
				sprintf(comm_array[i], "%s/", DEFAULT_PATH);
			}
			free(path_no_tilde);
		}
		//expand user defined variables
		if (!sys_shell && comm_array[i][0] == '$') {
			char *var_name=straft(comm_array[i], '$');
			if (var_name) {
				for (unsigned j=0;(int)j<usrvar_n;j++) {
					if (strcmp(var_name, usr_var[j].name) == 0) {
						comm_array[i]=xrealloc(comm_array[i], (strlen(usr_var[j].value)+1)*sizeof(char *));
						strcpy(comm_array[i], usr_var[j].value);
					}
				}
				free(var_name);
			}
		}
	}
	return comm_array;
}

char *rl_no_hist(char *prompt)
{
	stifle_history(0); //prevent readline from using the history setting
	char *input=readline(prompt);
	unstifle_history(); //reenable history
	read_history(HIST_FILE); //reload the history lines.
	return input;
}

void get_sel_files(void)
//get elements currently in the Selection Box, if any.
{
	//first, clear the arrays, in case they were already used
	if (sel_n > 0)
		for (int i=0;i<sel_n;i++)
			free(sel_elements[i]);
	free(sel_elements);
	sel_n=0;
	FILE *sel_fp=fopen(sel_file_user, "r");
	sel_elements=xcalloc(sel_elements, 1, sizeof(char *));
	if (sel_fp != NULL) {
//		size_t line_len;
		char sel_line[PATH_MAX]="";
		while (fgets(sel_line, sizeof(sel_line), sel_fp)) {
			size_t line_len=strlen(sel_line);
			sel_line[line_len-1]='\0';
			sel_elements=xrealloc(sel_elements, sizeof(char **)*(sel_n+1));
			sel_elements[sel_n]=xcalloc(sel_elements[sel_n], line_len, sizeof(char *));
			strcpy(sel_elements[sel_n++], sel_line);
		}
		fclose(sel_fp);
	}
}

void exec_prompt_cmds(void)
{
/*	static char **cmds=NULL;
	if (cmds) {
		exec_cmd(cmds);
		return;
	}
*/
	struct stat file_attrib;
	if (stat(PROMPT_FILE, &file_attrib) == 0) {
		FILE *fp=fopen(PROMPT_FILE, "r");
		if (fp) {
			char line[MAX_LINE]="";
			while(fgets(line, sizeof(line), fp)) {
				if (strncmp(line, "#END OF PROMPT", 14) == 0) break;
				if (strlen(line) != 0 && line[0] != '#') {
					args_n=0;
					char **cmds=parse_input_str(line);
					if (cmds) {
						no_log=1;
						exec_cmd(cmds);
						no_log=0;
						for (unsigned i=0;i<=(unsigned)args_n;i++)
							free(cmds[i]);
						free(cmds);
					}
					args_n=0;
				}
			}
			fclose(fp);
		}
	}
}

char *prompt(void)
/*print the prompt and return the string entered by the user (to be parsed later by 
parse_input_string())*/
{	
	if (welcome_message) {
		printf("%s%s%s\n", magenta, WELCOME_MSG, NC);
		printf("%sType '%shelp%s%s' or '%s?%s%s' for instructions.%s\n", default_color, white, NC, 
														   default_color, white, NC, default_color, NC);
		welcome_message=0;
	}
	exec_prompt_cmds();
	short max_prompt_path=40, home=0, path_too_long=0;
	char *input=NULL, *path_tilde=NULL, *short_path=NULL, cwd[PATH_MAX]="";
	args_n=0; //reset value
	if (strncmp(path, DEFAULT_PATH, def_path_len) == 0) {
		home=1;
		if ((path_tilde=home_tilde(path)) == NULL) { //reduce "/home/user/..." to "~/..."
			path_tilde=xcalloc(path_tilde, 4, sizeof(char *));
			strcpy(path_tilde, "???");
		}
	}
	if (strlen(path) > (unsigned)max_prompt_path) {
		path_too_long=1;
		if ((short_path=straftlst(path, '/')) == NULL) {
			short_path=xcalloc(short_path, 4, sizeof(char *));
			strcpy(short_path, "???");
		}
	}
	if (getcwd(cwd, sizeof(cwd)));
	else strcpy(cwd, "???");
	static char first_time=1, user_len=0, hostname_len=0; /*calculate the length of these 
	variables just once. In Bash, even if the user changes his/her user or hostname, the 
	prompt will not be updated til' the next login*/
	if (first_time) {
		user_len=(char)strlen(user);
		hostname_len=(char)strlen(hostname);
		first_time=0;
	}
	short prompt_length=((path_too_long) ? strlen(short_path)+1 : (home) ? strlen(path_tilde)+1 : 
							 strlen(cwd)+1)+33+7+2+((sel_n) ? 19 : 0)+user_len+hostname_len+1;
	char shell_prompt[prompt_length];
	//add 1 to strlen, since this function doesn't count the terminating null byte
	/*33=length of prompt_color + length of NC_b; 7=chars in the prompt: '[]', '@', '$' plus 
	3 spaces; 2=':[C/S]' (C for CliFM shell, and S for system shell); 19=length of green_b 
	+ '*'; 1=space for the null char written at the end of the string by sprintf*/
	memset(shell_prompt, '\0', prompt_length);
	snprintf(shell_prompt, prompt_length, "%s%s%s[%s@%s:%c] %s $%s%s ", (sel_n) ? green_b 
	   : "", (sel_n) ? "*" : "", prompt_color, user, hostname, (sys_shell) ? 'S' : 'C', 
	   (path_too_long) ? short_path : (home) ? path_tilde : cwd, NC_b, default_color);
	if (home) free(path_tilde);
	if (path_too_long) free(short_path);
	input=readline(shell_prompt);
	if (input == NULL) return NULL;
	//enable commands history only if the input line is not void
	if (input && *input) {

		//do not record exit, history commands, or consecutively equal inputs
//		int exit_c=check_syn_cmd(exit_cmds, input);
//		if (!exit_c && input[0] != '!' && (strcmp(input, history[current_hist_n-1]) != 0)) {

		if (strcmp(input, "q") != 0 && strcmp(input, "quit") != 0 
		&& strcmp(input, "exit") != 0 && strcmp(input, "zz") != 0
		&& strcmp(input, "salir") != 0 && strcmp(input, "chau") != 0
		&& input[0] != '!' && (strcmp(input, history[current_hist_n-1]) != 0)) {
			add_history(input);
			append_history(1, HIST_FILE);
			//add the new input to the history array
			history=xrealloc(history, sizeof(char **)*(current_hist_n+1));
			history[current_hist_n]=xcalloc(history[current_hist_n], strlen(input)+1, 
																   sizeof(char *));
			strcpy(history[current_hist_n++], input);
		}
	}
	return input;
}

/*int count_dir(char *dir_path)
//Count the amount of elements contained in a directory. Receives the path to the directory as
//argument.
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
		atexit(free_stuff);
	}
	return files_n;
}*/

int count_dir(char *dir_path) //No-scandir version
{
	struct stat file_attrib;
	if (lstat(dir_path, &file_attrib) == -1) return 0;
	int file_count=0;
	DIR *dir_p;
	struct dirent *entry;
	if ((dir_p=opendir(dir_path)) == NULL && errno == ENOMEM) {
		fprintf(stderr, "%s: opendir: Out of memory!\n", PROGRAM_NAME);
		atexit(free_stuff);
	}
	while ((entry=readdir(dir_p))) file_count++;
	closedir(dir_p);
	return file_count;
}

int skip_implied_dot (const struct dirent *entry)
{
	struct stat file_attrib;
	if (lstat(entry->d_name, &file_attrib) == -1) {
		fprintf(stderr, "stat: cannot access '%s': %s\n", entry->d_name, strerror(errno));
		return 0;
	}
	if (strcmp(entry->d_name, ".") !=0 && strcmp(entry->d_name, "..") !=0) {
		if (show_hidden == 0) { //do not show hidden files (hidden == 0)
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

int folder_select (const struct dirent *entry)
{
	struct stat file_attrib;
	//In case a directory isn't reacheable, like a failed mountpoint...
	if (lstat(entry->d_name, &file_attrib) == -1) {
		fprintf(stderr, "stat: cannot access '%s': %s\n", entry->d_name, strerror(errno));
		return 0;
	}
	if (file_attrib.st_mode & S_IFDIR && !S_ISSOCK(file_attrib.st_mode)) {
		if (strcmp(entry->d_name, ".") !=0 && strcmp(entry->d_name, "..") !=0) {
			if (show_hidden == 0) { //do not show hidden files (hidden == 0)
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
}

int file_select (const struct dirent *entry)
{
	struct stat file_attrib;
	if (lstat(entry->d_name, &file_attrib) == -1) {
//		fprintf(stderr, "stat_c: cannot access '%s': %s\n", entry->d_name, strerror(errno));
		return 0;
	}
	if (!(file_attrib.st_mode & S_IFDIR) || S_ISSOCK(file_attrib.st_mode)) {
		if (show_hidden == 0) { //do not show hidden files
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

void colors_list(char *entry, int i)
{
	struct stat file_attrib;
	int ret=0;
	ret=lstat(entry, &file_attrib);
	if (ret == -1) {
		fprintf(stderr, "%s: stat: %s\n", PROGRAM_NAME, strerror(errno));
		return;
	}
	char *linkname=NULL;
	cap_t cap;
	switch (file_attrib.st_mode & S_IFMT) {
		case S_IFREG:
			if (!(file_attrib.st_mode & S_IRUSR))
				printf("%s%d %s%s%s\n", yellow, i+1, d_red, entry, NC);
			else if (file_attrib.st_mode & S_ISUID) //set uid file
				printf("%s%d %s%s%s\n", yellow, i+1, bg_red_fg_white, entry, NC);
			else if (file_attrib.st_mode & S_ISGID) //set gid file
				printf("%s%d %s%s%s\n", yellow, i+1, bg_yellow_fg_black, entry, NC);
			else {
				cap=cap_get_file(entry);
				if (cap) {
					printf("%s%d %s%s%s\n", yellow, i+1, bg_red_fg_black, entry, NC);					
					cap_free(cap);
				}
				else if (file_attrib.st_mode & S_IXUSR) {
					if (file_attrib.st_size == 0)
						printf("%s%d %s%s%s\n", yellow, i+1, d_green, entry, NC);
					else printf("%s%d %s%s%s\n", yellow, i+1, green, entry, NC);
				}
				else if (file_attrib.st_size == 0)
					printf("%s%d %s%s%s\n", yellow, i+1, d_yellow, entry, NC);
				else
					printf("%s%d%s %s%s%s\n", yellow, i+1, NC, default_color, entry, NC);
			}
		break;
		case S_IFDIR:
			if (access(entry, R_OK|X_OK) != 0)
				printf("%s%d %s%s%s\n", yellow, i+1, red, entry, NC);
			else {
				int is_oth_w=0;
				if (file_attrib.st_mode & S_IWOTH) is_oth_w=1;
				int files_dir=count_dir(entry);
				if (files_dir == 2 || files_dir == 0) { /*if folder is empty, it contains only "." 
				and ".." (2 elements). If not mounted (ex: /media/usb) the result will be zero.*/
					//if sticky bit dir: green bg.
					printf("%s%d %s%s%s\n", yellow, i+1, (file_attrib.st_mode & S_ISVTX) ? 
						((is_oth_w) ? bg_green_fg_blue : bg_blue_fg_white) : ((is_oth_w) ? 
												  bg_green_fg_black : d_blue), entry, NC);
				}
				else
					printf("%s%d %s%s%s\n", yellow, i+1, (file_attrib.st_mode & S_ISVTX) ? 
						((is_oth_w) ? bg_green_fg_blue : bg_blue_fg_white) : ((is_oth_w) ? 
												  bg_green_fg_black : blue), entry, NC);
			}
		break;
		case S_IFLNK:
			linkname=realpath(entry, NULL);
			if (linkname) {
				printf("%s%d %s%s%s\n", yellow, i+1, cyan, entry, NC);
				free(linkname);
			}
			else printf("%s%d %s%s%s\n", yellow, i+1, d_cyan, entry, NC);
		break;
		case S_IFIFO: printf("%s%d %s%s%s\n", yellow, i+1, d_magenta, entry, NC); break;
		case S_IFBLK: printf("%s%d %s%s%s\n", yellow, i+1, yellow, entry, NC); break;
		case S_IFCHR: printf("%s%d %s%s%s\n", yellow, i+1, white, entry, NC); break;
		case S_IFSOCK: printf("%s%d %s%s%s\n", yellow, i+1, magenta, entry, NC); break;
		//in case all of the above conditions are false...
		default: printf("%s%d %s%s%s\n", yellow, i+1, bg_white_fg_red, entry, NC);
	}
}

void free_dirlist()
{
	while (files--)
		free(dirlist[files]);
}

void list_dir()
//to get a list of 'ls' colors run: 
//$ dircolors --print-database
{
//	clock_t start=clock();
	if (chdir(path) == -1) { /*this command changes the program's path, so that functions 
		will be aware of the current path, making thus unnecesary to pass the absolute path 
		to them. File names or relative paths are enough.*/
		fprintf(stderr, "%s: %s: %s", PROGRAM_NAME, path, strerror(errno));
		return;
	}
	int i=0;
	struct stat file_attrib;
	if (path[strlen(path)-1] == '/' && strcmp(path, "/") != 0) //remove final slash from path, if any.
		path[strlen(path)-1]='\0';
	files=0; //reset the files counter
	if (list_folders_first) {
		int files_files=0, files_folders=0;
		struct dirent **dirlist_folders, **dirlist_files;
		files_folders=scandir(path, &dirlist_folders, folder_select, (case_sensitive) ? 
													alphasort : alphasort_insensitive);
		if (files_folders == -1) {
			if (errno == ENOMEM) {
				fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
				atexit(free_stuff); //exit the program after running the clean function
			}
			else {
				fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
				return;
			}
		}
		files_files=scandir(path, &dirlist_files, file_select, (case_sensitive) ?
											  alphasort : alphasort_insensitive);
		if (files_files == -1) {
			if (errno == ENOMEM) {
				fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
				atexit(free_stuff);
			}
			else {
				fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
				return;
			}
		}
		//if empty folder... Note: realloc(ptr, 0) acts like free
		if (!files_folders && !files_files) dirlist=xrealloc(dirlist, sizeof(struct dirent **));
		else if (files_folders > 0) {
			if (files_files > 0) //if files and folders
				dirlist=xrealloc(dirlist, (sizeof(struct dirent **)*(files_folders+files_files)));
			else //if only folders
				dirlist=xrealloc(dirlist, sizeof(struct dirent **)*(files_folders));
		}
		else if (files_files > 0) //if only files
			dirlist=xrealloc(dirlist, sizeof(struct dirent **)*(files_files));
//		else //if neither files nor folders
//			dirlist=xrealloc(dirlist, sizeof(struct dirent **));
		if (files_folders > 0) {
			for(i=0;i<files_folders;i++) {
				dirlist[files]=xcalloc(dirlist[files], strlen(dirlist_folders[i]->d_name)+4, 
																			sizeof(char *));
				/*copy not only the file name (d_name), but also its file type (d_type), since this
				latter will be used to classify files according to its type.*/
				strcpy(dirlist[files++]->d_name, dirlist_folders[i]->d_name);
//				dirlist[files++]->d_type=dirlist_folders[i]->d_type;
				free(dirlist_folders[i]);
			}
			free(dirlist_folders);
		}
		if (files_files > 0) {
			for(i=0;i<files_files;i++) {
				dirlist[files]=xcalloc(dirlist[files], strlen(dirlist_files[i]->d_name)+4, 
																		  sizeof(char *));
				strcpy(dirlist[files++]->d_name, dirlist_files[i]->d_name);
//				dirlist[files++]->d_type=dirlist_files[i]->d_type;
				free(dirlist_files[i]);
			}
			free(dirlist_files);
		}
	}
	else { //if no list_folders_first
		struct dirent **files_list;
		int files_num;
		if ((files_num=scandir(path, &files_list, skip_implied_dot, (case_sensitive) ? 
										  alphasort : alphasort_insensitive)) == -1) {
			if (errno == ENOMEM) {
				fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
				atexit(free_stuff);
			}
			else {
				fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
				return;
			}
		}
		dirlist=xrealloc(dirlist, sizeof(struct dirent **)*files_num);
		for (int i=0;i<files_num;i++) {
			dirlist[files]=xcalloc(dirlist[files], strlen(files_list[i]->d_name)+4, 
																   sizeof(char *));
			strcpy(dirlist[files++]->d_name, files_list[i]->d_name);
//			dirlist[files++]->d_type=files_list[i]->d_type;
			free(files_list[i]);			
		}
		free(files_list);
	}
	if (files == 0) {
		if (clear_screen) CLEAR;
		printf("Empty directory\n");
		return;
	}
/*	if (files < 0) { //This will not happen here: error was already handled above
		fprintf(stderr, "CLiFM: Error listing files\n");
		return;
	}*/
	int longest=0, files_num=0;
	//get the longest element
	for(i=0;i<files;i++) {
		lstat(dirlist[i]->d_name, &file_attrib);
		/* file_name_width contains: ELN's number of digits + 1 space between ELN and file 
		name + the length of the filename itself. Ex: '12 name' contains 7 chars*/
		int file_name_width=digits_in_num(i+1)+1+strlen(dirlist[i]->d_name);
		/*if the file is a non-empty directory and the user has access permision to it, add to
		file_name_width the number of digits of the amount of files this directory contains
		(ex: 123 (files) contains 3 digits) + 2 for space and slash between the directory 
		name and the amount of files it contains. Ex: '12 name /45' contains 11 chars.*/
		if ((file_attrib.st_mode & S_IFDIR) && access(dirlist[i]->d_name, R_OK|X_OK) == 0)
			if ((files_num=count_dir(dirlist[i]->d_name)) > 2)
				file_name_width+=digits_in_num(files_num)+2;
		if (file_name_width > longest) {
			longest=file_name_width;
		}
	}
	//get terminal current amount of columns 
	struct winsize w;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
	int term_cols=w.ws_col;
	//get possible amount of columns for the dirlist screen
	int columns_n=term_cols/(longest+1); //+1 for the space between file names
	/*if longest is bigger than terminal columns, columns_n will be negative or zero. 
	To avoid this:*/
	if (columns_n < 1) columns_n=1;
	if (clear_screen) CLEAR;
	int last_column=0;
	for (i=0;i<files;i++) {
		int is_dir=0, files_dir=0;
		char *linkname=NULL;
		cap_t cap;
		if ((i+1)%columns_n == 0) last_column=1;
		else last_column=0;
		lstat(dirlist[i]->d_name, &file_attrib);
		switch (file_attrib.st_mode & S_IFMT) {
			case S_IFDIR:
				if (access(dirlist[i]->d_name, R_OK|X_OK) != 0)
					printf("%s%d %s%s%s%s", yellow, i+1, red, dirlist[i]->d_name, NC, 
														  (last_column) ? "\n" : "");
				else {
					int is_oth_w=0;
					if (file_attrib.st_mode & S_IWOTH) is_oth_w=1;
					files_dir=count_dir(dirlist[i]->d_name);
					if (files_dir == 2 || files_dir == 0) { /*if folder is empty, it contains only "." 
					and ".." (2 elements). If not mounted (ex: /media/usb) the result will be zero.*/
						//if sticky bit dir: green bg.
						printf("%s%d %s%s%s%s", yellow, i+1, (file_attrib.st_mode & S_ISVTX) ? 
						((is_oth_w) ? bg_green_fg_blue : bg_blue_fg_white) : ((is_oth_w) ? 
						  bg_green_fg_black : d_blue), dirlist[i]->d_name, NC, (last_column) ? 
																				 "\n" : "");
					}
					else {
						printf("%s%d %s%s%s%s /%d%s%s", yellow, i+1, (file_attrib.st_mode & S_ISVTX) ? 
						  ((is_oth_w) ? bg_green_fg_blue : bg_blue_fg_white) : ((is_oth_w) ? 
						    bg_green_fg_black : blue), dirlist[i]->d_name, NC, default_color, 
												 files_dir-2, NC, (last_column) ? "\n" : "");
						is_dir=1;
					}
				}
			break;
			case S_IFIFO:
				printf("%s%d %s%s%s%s", yellow, i+1, d_magenta, dirlist[i]->d_name, NC, 
															(last_column) ? "\n" : "");
			break;
			case S_IFLNK:
				linkname=realpath(dirlist[i]->d_name, NULL);
				if (linkname) {
					printf("%s%d %s%s%s%s", yellow, i+1, cyan, dirlist[i]->d_name, NC, 
														   (last_column) ? "\n" : "");
					free(linkname);
				}
				else
					printf("%s%d %s%s%s%s", yellow, i+1, d_cyan, dirlist[i]->d_name, NC, 
															 (last_column) ? "\n" : "");
			break;
			case S_IFBLK:
				printf("%s%d %s%s%s%s", yellow, i+1, yellow, dirlist[i]->d_name, NC, 
														 (last_column) ? "\n" : "");
			break;
			case S_IFCHR:
				printf("%s%d %s%s%s%s", yellow, i+1, white, dirlist[i]->d_name, NC, 
														(last_column) ? "\n" : "");
			break;
			case S_IFSOCK:
				printf("%s%d %s%s%s%s", yellow, i+1, magenta, dirlist[i]->d_name, NC, 
														  (last_column) ? "\n" : "");
			break;
			case S_IFREG:
				cap=cap_get_file(dirlist[i]->d_name);
				if (!(file_attrib.st_mode & S_IRUSR))
					printf("%s%d %s%s%s%s", yellow, i+1, d_red, dirlist[i]->d_name, NC, 
															(last_column) ? "\n" : "");
				else if (file_attrib.st_mode & S_ISUID) //set uid file
					printf("%s%d %s%s%s%s", yellow, i+1, bg_red_fg_white, dirlist[i]->d_name, NC, 
																	  (last_column) ? "\n" : "");
				else if (file_attrib.st_mode & S_ISGID) //set gid file
					printf("%s%d %s%s%s%s", yellow, i+1, bg_yellow_fg_black, dirlist[i]->d_name, 
															  NC, (last_column) ? "\n" : "");
				else if (cap) {
						printf("%s%d %s%s%s%s", yellow, i+1, "\033[0;30;41m", dirlist[i]->d_name, NC, 
																  (last_column) ? "\n" : "");				
					cap_free(cap);
				}
				else if (file_attrib.st_mode & S_IXUSR)
					if (file_attrib.st_size == 0)
						printf("%s%d %s%s%s%s", yellow, i+1, d_green, dirlist[i]->d_name, NC, 
																  (last_column) ? "\n" : "");
					else
						printf("%s%d %s%s%s%s", yellow, i+1, green, dirlist[i]->d_name, NC, 
																(last_column) ? "\n" : "");
				else if (file_attrib.st_size == 0)
					printf("%s%d %s%s%s%s", yellow, i+1, d_yellow, dirlist[i]->d_name, NC, 
															   (last_column) ? "\n" : "");
				else
					printf("%s%d%s %s%s%s%s", yellow, i+1, NC, default_color, dirlist[i]->d_name, 
																  NC, (last_column) ? "\n" : "");
			break;
			//in case all of the above conditions are false...
			default: 
				printf("%s%d %s%s%s%s", yellow, i+1, bg_white_fg_red, dirlist[i]->d_name, NC, 
															 (last_column) ? "\n" : "");
		}
		if (!last_column) {
			//get the difference between the length of longest and the current elements
			int diff=longest-(digits_in_num(i+1)+1+strlen(dirlist[i]->d_name));
			if (is_dir) { //if a directory, make room for displaying the amount of files
				//it contains.
				int dig_num=digits_in_num(files_dir-2); //get the amount of digits of files_dir.
				diff=diff-(dig_num+2); //the amount of digits plus 2 chars for " /".
			}
			//print the spaces needded to equate the length of the lines
			for (int j=diff+1;j;j--) //+1 is for the space between file names
				printf(" ");
		}
	}
	/*if the last listed element was modulo (in the last column), don't print anything, since 
	it already has a new line char at the end. Otherwise, if not modulo (not in the last column), 
	print a new line, for it has none.*/
	printf("%s", (last_column) ? "" : "\n");
	//print a dividing line between the files list and the prompt
	for (i=term_cols;i;i--)
		printf("%s=", d_blue);
	printf("%s%s", NC, default_color);
	fflush(stdout);

//	clock_t end=clock();
//	printf("list_dir time: %f\n", (double)(end-start)/CLOCKS_PER_SEC);
}

void get_aliases_n_prompt_cmds(void)
//Copy aliases found in clifmrc to a tmp file: /tmp/.clifm_aliases
{
	remove(PROMPT_FILE);
	remove(ALIASES_FILE);
	FILE *config_file_fp;
	FILE *aliases_fp;
	FILE *prompt_fp;
	config_file_fp=fopen(CONFIG_FILE, "r");
	if (config_file_fp == NULL) {
		fputs("alias: Error opening file", stderr);
		return;
	}
	aliases_fp=fopen(ALIASES_FILE, "a");
	if (aliases_fp == NULL) {
		fputs("alias: Error opening file", stderr);
		fclose(config_file_fp);
		return;
	}
	prompt_fp=fopen(PROMPT_FILE, "a");
	if (prompt_fp == NULL) {
		fputs("prompt: Error opening file", stderr);
		fclose(config_file_fp);	
		fclose(aliases_fp);
		return;
	}	
	char line[MAX_LINE]="";
	int prompt_line_found=0;
	while (fgets(line, sizeof(line), config_file_fp)) {
		if (strncmp(line, "alias", 5) == 0) {
			char *alias_line=straft(line, ' ');	
			fprintf(aliases_fp, alias_line);
			free(alias_line);
		}
		else if (prompt_line_found) {
			fprintf(prompt_fp, line);
		}
		else if (strncmp(line, "#PROMPT", 7) == 0) 
			prompt_line_found=1;
	}
	fclose(config_file_fp);
	fclose(aliases_fp);
	fclose(prompt_fp);
}

char **check_for_alias(char **comm)
/*if a matching alias is found, execute the corresponding command, if it exists. Returns 1 if
matching alias is found, zero if not.*/
{
	glob_cmd=0;
	FILE *aliases_fp;
	aliases_fp=fopen(ALIASES_FILE, "r");
	if (aliases_fp == NULL) {
		fprintf(stderr, "%s: Error reading aliases\n", PROGRAM_NAME);
		return NULL;
	}
	char *aliased_cmd=NULL;
	char line[MAX_LINE]="", comm_tmp[MAX_LINE]="";
	snprintf(comm_tmp, MAX_LINE, "%s=", comm[0]); /*look for this string: "command=" in the aliases 
	file*/
	size_t comm_len=strlen(comm[0]);
	while (fgets(line, sizeof(line), aliases_fp)) {
		if (strncmp(line, comm_tmp, comm_len+1) == 0) {
			aliased_cmd=strbtw(line, '\'', '\''); //get the aliased command
			char *command_tmp=NULL;
			command_tmp=xcalloc(command_tmp, PATH_MAX, sizeof(char *));
			strncat(command_tmp, aliased_cmd, PATH_MAX); 
			free(aliased_cmd);
			args_n=0; //reset args_n to be used by parse_input_str()
			//parse the aliased cmd
			char **alias_comm=parse_input_str(command_tmp);
			free(command_tmp);
			//add input parameters, if any, to alias_comm
			for (int i=1;comm[i];i++) {
				alias_comm=xrealloc(alias_comm, sizeof(char **)*(++args_n+1));
				alias_comm[args_n]=xcalloc(alias_comm[args_n], strlen(comm[i])+1, sizeof(char *));
				strcpy(alias_comm[args_n], comm[i]);
			}
			//add a final null
			alias_comm=xrealloc(alias_comm, sizeof(char **)*(args_n+2));			
			alias_comm[args_n+1]=NULL;
			//free comm
			for (int i=0;comm[i];i++) free(comm[i]);
			free(comm);			
			int wildcard_index=-1, options_n=0;
			for (int i=0;alias_comm[i]!=NULL;i++) {
				if (alias_comm[i][0] == '-')
					options_n++;
				else if (strcntchr(alias_comm[i], '*') != -1 || strcntchr(alias_comm[i], '?') != -1)
					wildcard_index=i;
			}
			if (wildcard_index != -1) { //if there are wildcards...
				run_glob_cmd(options_n, 0, alias_comm, alias_comm[wildcard_index]);
				for (int i=0;alias_comm[i];i++) free(alias_comm[i]);
				free(alias_comm);
				glob_cmd=1;
//				fclose(aliases_fp);
				return NULL;
			}
			else { //if no wildcards...
				fclose(aliases_fp);
				return alias_comm;
			}
		}
	}
	fclose(aliases_fp);
	return NULL;
}

void show_aliases(void)
{
	FILE *aliases_fp;
	aliases_fp=fopen(ALIASES_FILE, "r");
	if (aliases_fp == NULL) {
		fprintf(stderr, "%s: alias: Error getting aliases\n", PROGRAM_NAME);
		return;
	}
	char line[MAX_LINE]="";
	while(fgets(line, sizeof(line), aliases_fp)) {
		printf("%s", line);
	}
	fclose(aliases_fp);
}

int run_glob_cmd(int options_n, int is_background, char **args, char *path)
{
	glob_t globbed_files;
	globbed_files.gl_offs=options_n+1; /*number of slots reserved by GLOB_DOOFFS at the 
	beginning of globbed_files.gl_pathv*/ 
	int ret=glob(path, GLOB_DOOFFS, NULL, &globbed_files);
	if (ret == 0) {
		globbed_files.gl_pathv[0]=args[0];
		for (int i=1;i<=options_n;i++)
			globbed_files.gl_pathv[i]=args[i];
		pid_t pid_glob=fork();
		if (pid_glob == 0) {
			set_signals_to_default();
			execvp(args[0], &globbed_files.gl_pathv[0]);
			set_signals_to_ignore();
		}
		else {
			if (is_background)
				run_in_background(pid_glob);
			else
				run_in_foreground(pid_glob);
		}
		globfree(&globbed_files);
		return 1;
	}
	else {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, args[0], (ret == GLOB_ABORTED) 
						? "Read error" : (ret == GLOB_NOMATCH) ? "No matches found" : 
											(ret == GLOB_NOSPACE) ? "Out of memory" : 
																  "Unknown problem");
		globfree(&globbed_files);
		return 0;
	}
}

char ***parse_pipes(char *string)
{
	//###get amount of cmds and length of every one of them, including spaces####
	size_t str_len=strlen(string);
	unsigned i=0, j=0, k=0, l=0;
	for (i=0;i<=str_len;i++) {
		if (string[i] == '|' || string[i] == '\0') { 
			pipes=xrealloc(pipes, sizeof(char **)*(++l+1));
			pipes[pipes_index]=xcalloc(pipes[pipes_index], 2, sizeof(char *));
			sprintf(pipes[pipes_index++], "%d", (int)(i-j));
			j=i+1;
		}
	}
	//####Allocate memory for the piped cmds####
	char **piped_cmds=NULL;
	piped_cmds=xcalloc(piped_cmds, pipes_index+1, sizeof(char **));
	unsigned not_space=0;
	for (i=0;i<(unsigned)pipes_index;i++) {
		piped_cmds[i]=xcalloc(piped_cmds[i], atoi(pipes[i]), sizeof(char *));
		free(pipes[i]);
	}
	//store cmds into an two-dimensional array of strings (removing leading and trailing 
	//spaces)
	j=0;
	for (i=0;i<=str_len;i++) {
		while (string[i]!='|' && string[i]!='\0')
			//if space and no non-space char has been found yet, do nothing
			if (isspace(string[i]) && not_space == 0) 
				i++;
			else { //if non-space or space when non-space has been already found, add it
				piped_cmds[j][k++]=string[i++];
				not_space=1; //first non-space char found
			}
		//to remove final spaces, traverse the array from end to start and. When first non-
		//space char is found, make NULL the index before it.
		for (int l=strlen(piped_cmds[j])-1;l>=0;l--) {
			if (!isspace(piped_cmds[j][l])) {
				piped_cmds[j][l+1]='\0';
				break;
			}
		}
		j++;
		k=not_space=0;
	}

//	Replace all of the above by this:
//	char **piped_cmds=get_substr(string, '|');

	/*now split each command (in piped_cmds) into its different parameters in a three-dimensional
	array*/
	char ***cmds_array=NULL;
	cmds_array=xcalloc(cmds_array, pipes_index+1, sizeof(char ***));
	for (i=0;piped_cmds[i];i++) {
		args_n=0;
		cmds_array[i]=parse_input_str(piped_cmds[i]);
		free(piped_cmds[i]); //free the two-dimensional array
	}
	free(piped_cmds);
	return cmds_array; //return the three-dimensional array
}

void exec_pipes(char ***cmd)
//Basic code taken from: https://stackoverflow.com/questions/17630247/coding-multiple-pipe-in-c
{
/*	short internal=0;
	//first, check whether there is some internal command
	for (int i=0;i<pipes_index;i++) {
		for (int j=0;j<internal_cmd_n;j++) {
			if (strcmp(cmd[i][0], INTERNAL_CMDS[j]) == 0) {
				internal=1;
				break;
			}
		}
		if (internal) break;
	}*/
//	if (!internal) { //check the existence of external (shell) commands
		for (int i=0;i<pipes_index;i++) { //if some of them doesn't exist...
			char *cmd_path=NULL;
			if ((cmd_path=get_cmd_path(cmd[i][0])) == NULL) {
				fprintf(stderr, "%s: %s: Command not found\n", PROGRAM_NAME, cmd[i][0]);
				free(cmd_path);
				return;
			}
			free(cmd_path);
		}
//	}
	int p[2];
	pid_t pid=0;
	//###INTERNAL####
/*	if (internal) {
		printf("Internal pipes not yet implemented\n");
		return;
	}
		int stdout_bk=dup(STDOUT_FILENO); //save original stdout
		if (pipe(p) == -1) { //create the pipe
			perror("pipe");
			return;
		}
		pid=fork(); //create a fork, a parent, and a child
		if (pid == -1) {
			perror("fork");
			return;
		}
		if (pid == 0) { //CHILD
			close(p[1]); //close one extrem of the pipe
			p[0]=dup2(p[0], STDIN_FILENO); //open the other as STDIN
			set_signals_to_default();
			if (execvp(cmd[1][0], cmd[1]) == -1) {
				perror("CliFM");
				return;
			}
			set_signals_to_ignore();
			return;
		}
		else { //PARENT
			close(p[0]); //close the extrem of the pipe opened in the child as STDIN
			p[1]=dup2(p[1], STDOUT_FILENO); //open the other extreme as STDOUT
			is_pipe=0;
			exec_cmd(cmd[0]);
		}
		dup2(stdout_bk, STDOUT_FILENO); //restore original stdout			
		return;
	}*/

	//###EXTERNAL####
	//At this point all piped command are external and existent
	int fd_in=0;
	while (*cmd != NULL) {
		pipe(p);
		if ((pid=fork()) == -1) {
			fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
			return;
		}
		else if (pid == 0) {
			//STDIN_FILENO==0; STDOUT_FILENO==1; STDERR_FILENO==2
			dup2(fd_in, STDIN_FILENO); //change the input according to the old one 
			if (*(cmd+1) != NULL)
				dup2(p[1], STDOUT_FILENO);
			close(p[0]);
			set_signals_to_default();
			if (execvp((*cmd)[0], *cmd) == -1) {
				fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
				close(p[1]);
				return;
			}
			set_signals_to_ignore();
		}
		else {
			waitpid(pid, NULL, 0);
			close(p[1]);
			fd_in=p[0]; //save the input for the next command
			cmd++;
		}
	}
}

void stream_redirection(char *string)
{
	flags &= ~STREAM_REDIR; //disable the stream redirectio flag
	int override=1;
	if (string[0] == '>' || string[strlen(string)-1] == '>') {
		fprintf(stderr, "%s: syntax error near unexpected token 'newline'\n", PROGRAM_NAME);
		return;
	}
	//parse the second part of the stream (the path)
	char *path_tmp=straft(string, '>');
	if (path_tmp == NULL) return; //Error message!!
	char *path=handle_spaces(path_tmp);
	free(path_tmp);
	//parse the first part (the command) and check if it exists
	char *cmd_tmp=strbfr(string, '>');
	args_n=0;
	char **cmd=parse_input_str(cmd_tmp);
	free(cmd_tmp);

	//store current stdout value
	int stdout_bk=dup(STDOUT_FILENO);
	if (stdout_bk == -1) {
		fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
		for (int i=0;i<=args_n;i++) free(cmd[i]);
		free(cmd);
		return;
	}
	//Redirect stdout to file
	FILE *stdout_fp;
	if ((stdout_fp=fopen(path, (override) ? "w" : "a")) == NULL)
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, path, strerror(errno));
	else {
		if (dup2(fileno(stdout_fp), STDOUT_FILENO) == -1) {
			fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
		}
		//If new stdout ok, execute the command
		else { //execute the command
			fclose(stdout_fp);
			exec_cmd(cmd);
			//restore the original stdout
			if (dup2(stdout_bk, STDOUT_FILENO) == -1)
				fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
			close(stdout_bk);
		}
	}
	//Free everything
	free(path);
	for (int i=0;i<=args_n;i++) free(cmd[i]);
	free(cmd);
	args_n=0;
}

void concatenate_cmds(char *cmd)
{
	//Get number of concatenated commands
	size_t cmd_len=strlen(cmd);
	unsigned i=0;
	short conc_n=0;
	for (i=0;i<cmd_len;i++)
		if (cmd[i] == ';')
			conc_n++;

	//Split commands into separated strings
	char **conc_cmds=NULL;
	conc_cmds=xcalloc(conc_cmds, conc_n+1, sizeof(char **));
	unsigned cmds_n=0, length=0;
	char buf[PATH_MAX]="";
	for (i=0;i<cmd_len;i++) {
		if (cmd[i] == ';') continue;
		else if (!cmd[i]) break;
		while (cmd[i] != ';' && cmd[i] != '\0') {
			buf[length++]=cmd[i++];
		}
		if (length) {
			conc_cmds[cmds_n]=xcalloc(conc_cmds[cmds_n], length+1, sizeof(char *));
			strncpy(conc_cmds[cmds_n++], buf, length);
			length=0;
		}
	}
	
	//Parse each command string, save it into a three-dimensional array and execute it
	char ***cmds_array=NULL;
	int args[cmds_n];
	cmds_array=xcalloc(cmds_array, cmds_n+1, sizeof(char ***));		
	for (i=0;i<cmds_n;i++) {
		args_n=0;
		cmds_array[i]=parse_input_str(conc_cmds[i]);
		free(conc_cmds[i]); //free the two-dimensional array
		args[i]=args_n; //number of args of this cmd
		exec_cmd(cmds_array[i]);
		args_n=0;
	}
	free(conc_cmds);

	//Free everything
	unsigned j=0;
	for (i=0;i<cmds_n;i++) {
		for (j=0;j<=(unsigned)args[i];j++) {
			free(cmds_array[i][j]);
		}
		free(cmds_array[i]);
	}
	free(cmds_array);
}

void exec_cmd(char **comm)
//Take the command entered by the user and call the corresponding function.
{
	//Process commands
	//process pipes, stream redir and cmds concatenation if !sys_shell
	if (!sys_shell) {
		if (comm[0][0] == '&' || strcmp(comm[0], "&") == 0) {
			fprintf(stderr, "%s: Syntax error near unexpected token '&'\n", PROGRAM_NAME);
			return;
		}
		if (flags & IS_USRVAR_DEF) {
			flags &= ~IS_USRVAR_DEF;
			create_usr_var(comm[0]);
			return;
		}
		if (flags & CONC_CMD) {
			flags &= ~CONC_CMD; //Remove the flag
			concatenate_cmds(comm[0]);
			return;
		}
		if (flags & STREAM_REDIR) {
			flags &= ~STREAM_REDIR;
			stream_redirection(comm[0]);
			return;
		}
		if (flags & IS_PIPE) {
			flags &= ~IS_PIPE;
			//if pipe char is first or last char...
			if (comm[0][0] == '|' || strcmp(comm[args_n], "|") == 0 || 
						comm[args_n][strlen(comm[args_n])-1] == '|') {
				printf("%s: syntax error near unexpected token '|'\n", PROGRAM_NAME);
				pipes_index=0;
				return;
			}
			/*split the input string (containing pipe chars '|') into a three-dimensional array
			containing commands and parameters*/ 
			char ***cmds_array=parse_pipes(comm[0]);
			exec_pipes(cmds_array);
			//free cmds_array
			for (int i=0;i<pipes_index;i++) {
				for (int j=0;cmds_array[i][j]!=NULL;j++) {
					free(cmds_array[i][j]);
				}
				free(cmds_array[i]);
			}
			free(cmds_array);
			pipes_index=args_n=0; /*when coming back to main, only comm[0], which contains
			the string entered at the prompt, will be freed*/
			return;
		}
	}
	else if (comm[0][1] == ';') {
		fprintf(stderr, "%s: syntax error near unexpected token ';;'\n", PROGRAM_NAME);
		return;
	}
	if (flags & IS_PIPE) flags &= ~IS_PIPE, pipes_index=0;
	if (flags & CONC_CMD) flags &= ~CONC_CMD;	
	if (flags & STREAM_REDIR) flags &= ~STREAM_REDIR;
	
	if (strcmp(comm[0], "o") == 0 || strcmp(comm[0], "cd") == 0 
								  || strcmp(comm[0], "open") == 0)
		open_function(comm);
	else if (strcmp(comm[0], "b") == 0 || strcmp(comm[0], "back") == 0) {
		if (access(old_pwd, F_OK) == 0) {
			update_path(old_pwd); //no need for strncpy, since both variables are PATH_MAX
			free_dirlist();
			list_dir();
		}
		else fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, old_pwd, strerror(errno));
	}
	else if (strcmp(comm[0], "mv") == 0 || strcmp(comm[0], "move") == 0) move_function(comm);
	else if (strcmp(comm[0], "cp") == 0 || strcmp(comm[0], "paste") == 0) copy_function(comm);
	else if (strcmp(comm[0], "rm") == 0) remove_function(comm);
	else if (strcmp(comm[0], "sel") == 0) sel_function(comm);
	else if (strcmp(comm[0], "sb") == 0 || strcmp(comm[0], "selbox") == 0)
		show_sel_files();
	else if (strcmp(comm[0], "ds") == 0 || strcmp(comm[0], "desel") == 0)
		deselect(comm);
	else if (strcmp(comm[0], "mkdir") == 0) mkdir_function(comm);
	else if (strcmp(comm[0], "touch") == 0) touch_function(comm);
	else if (strcmp(comm[0], "chown") == 0 || strcmp(comm[0], "chmod") == 0)
		chown_chmod(comm);
	else if (strcmp(comm[0], "ln") == 0 || strcmp(comm[0], "link") == 0)
		symlink_function(comm);
	else if (strcmp(comm[0], "pr") == 0 || strcmp(comm[0], "prop") == 0 
									    || strcmp(comm[0], "stat") == 0)
		properties_function(comm);
	else if (strcmp(comm[0], "bm") == 0) bookmarks_function();
	else if (comm[0][0] == '/' && access(comm[0], F_OK) != 0) //if not absolute path 
		search_function(comm);
	//if: '!number' or '!-number' or '!!'
	else if (comm[0][0] == '!') {
		if (isdigit(comm[0][1]) || comm[0][1] == '-' || comm[0][1] == '!')
			run_history_cmd(comm[0]);
		else return;
	}
	else if (strcmp(comm[0], "ls") == 0 && !cd_lists_on_the_fly) {
//		puts("here");
		search_mark=0;
		free_dirlist();
		list_dir();
		get_sel_files();
	}
	else if (strcmp(comm[0], "rf") == 0 || strcmp(comm[0], "refresh") == 0) {
		search_mark=0;
		free_dirlist();
		list_dir();
		get_sel_files();
	}
	else if ((strcmp(comm[0], "folders") == 0 && strcmp(comm[1], "first") == 0) ||
			strcmp(comm[0], "ff") == 0) {
		int n=0;
		if (strcmp(comm[0], "ff") == 0) n=1;
		else n=2;
		if (comm[n]) {
			int status=list_folders_first;
			if (strcmp(comm[n], "on") == 0) list_folders_first=1;
			else if (strcmp(comm[n], "off") == 0) list_folders_first=0;
			else {
				fprintf(stderr, "Usage: folers first [on/off]\n");
				return;			
			}
			if (list_folders_first != status) {
				search_mark=0;
				free_dirlist();
				list_dir();
			}
		}
		else {
			fprintf(stderr, "Usage: folers first [on/off]\n");
		}
	}
	else if (strcmp(comm[0], "sys") == 0) {
		if (comm[1]) {
			if (strcmp(comm[1], "on") == 0) {
				sys_shell=sys_shell_status=1;
			}
			else if (strcmp(comm[1], "off") == 0) {
				sys_shell=sys_shell_status=0;
			}
			else if (strcmp(comm[1], "status") == 0) {
				if (sys_shell_status) puts("System shell");
				else printf("%s shell\n", PROGRAM_NAME);
			}
			else fprintf(stderr, "Usage: sys [on/off/status]\n");
		}
		else fprintf(stderr, "Usage: sys [on/off/status]\n");
	}
	else if (strcmp(comm[0], "log") == 0) log_function(comm);
	else if (strcmp(comm[0], "bk") == 0 || strcmp(comm[0], "backup") == 0)
		backup_function(comm, NULL);
	else if (strcmp(comm[0], "alias") == 0) show_aliases();
	else if (strncmp(comm[0], "edit", 4) == 0) edit_function(comm);
	else if (strcmp(comm[0], "history") == 0) history_function(comm);
	else if (strcmp(comm[0], "hidden") == 0) hidden_function(comm);
	else if (strcmp(comm[0], "path") == 0) printf("%s\n", path);
	else if (strcmp(comm[0], "help") == 0 || strcmp(comm[0], "?") == 0)
		help_function();
	else if (strcmp(comm[0], "cmd") == 0 || strcmp(comm[0], "commands") == 0) 
		list_commands();
	else if (strcmp(comm[0], "colors") == 0) color_codes();
	else if (strcmp(comm[0], "version") == 0 || strcmp(comm[0], "v") == 0)
		version_function();
	else if (strcmp(comm[0], "license") == 0) license();
	else if (strcmp(comm[0], "bonus") == 0 || strcmp(comm[0], "boca") == 0)
		bonus_function();
	else if (strcmp(comm[0], "splash") == 0) {
		splash();
		search_mark=0;
		free_dirlist();
		list_dir();
	}
	else if (strncmp(comm[0], "jobs", 4) == 0) list_jobs();
	else if (strcmp(comm[0], "q") == 0 || strcmp(comm[0], "quit") == 0 
			|| strcmp(comm[0], "exit") == 0 || strcmp(comm[0], "zz") == 0 || 
			strcmp(comm[0], "salir") == 0 || strcmp(comm[0], "chau") == 0) {
		//#####free everything#####
		free_stuff();
		for (int i=0;i<args_n+1;i++)
			free(comm[i]);
		free(comm);
		exit(EXIT_SUCCESS);
	}
	
	else { //EXTERNAL COMMANDS
		//if not a command, but a directory...
		if (comm[0][0] == '/') {
			struct stat file_attrib;
			if (lstat(comm[0], &file_attrib) == 0) {
				if (file_attrib.st_mode & S_IFDIR) {
					fprintf(stderr, "%s: %s: Is a directory\n", PROGRAM_NAME, comm[0]);
					return;
				}
			}
		}
		/*Allow the user to run a CliFM command (ex: cd, touch, mkdir, chown, etc.) in its 
		external form with only making precede the cmd by a colon.*/
		if (comm[0][0] == ':') { //remove the colon from the beginning of the first argument.
			char *comm_tmp=straft(comm[0], ':');
			if (comm_tmp) {
				strcpy(comm[0], comm_tmp);
				free(comm_tmp);
			}
			else {
				fprintf(stderr, "%s: Syntax error near unexpected token ':'\n", PROGRAM_NAME);			
				return;
			}
		}
		//Log external commands: 'no_log' will be true when running profile or prompt commands.
		if (!no_log) log_function(comm);
		/*If running the CLiFM shell, allow the user to run a cmd via the system shell 
		(system() == execl("/bin/sh"...)) by making it precede by a semicolon.*/
		/*Prevent the user from killing the program by the 'kill', 'pkill' or 'killall' commands.
		Otherwise, the program will be forcefully terminated without freeing malloced memory.*/
		if (strncmp(comm[0], "kill", 3) == 0 || strncmp(comm[0], "killall", 6) == 0 || 
											  strncmp(comm[0], "pkill", 5) == 0) {
			for (int i=1;i<=args_n;i++) {
				if ((strcmp(comm[0], "kill") == 0 && atoi(comm[i]) == get_own_pid()) || 
				 ((strcmp(comm[0], "killall") == 0 || strcmp(comm[0], "pkill") == 0) && 
								strcmp(comm[i], program_invocation_short_name) == 0)) {
						fprintf(stderr, "%s: To exit the program type 'quit'", PROGRAM_NAME);
						return;
				}
			}
		}

//		####SYSTEM VERSION#####
		if (sys_shell) {
			if (comm[0][0] == ';') {
				//Remove the semicolon, if any, from the beginning of the first arg.
				char *comm_tmp=straft(comm[0], ';');
				if (comm_tmp) {
					strcpy(comm[0], comm_tmp);
					free(comm_tmp);
				}
				else { //if string is ";"
					fprintf(stderr, "%s: Syntax error near unexpected token ';'\n", PROGRAM_NAME);
					sys_shell=sys_shell_status; //restore the original value of sys_shell
				return;
				}
			}
			char *ext_cmd=NULL;
			ext_cmd=xcalloc(ext_cmd, PATH_MAX, sizeof(char *));
			strncpy(ext_cmd, comm[0], PATH_MAX);
			if (args_n) {
				for (unsigned i=1;i<=(unsigned)args_n;i++) {
					strncat(ext_cmd, " ", PATH_MAX);
					strncat(ext_cmd, comm[i], PATH_MAX);
				}
			}
			pid_t pid=fork();
			if (pid < 0) {
				fprintf(stderr, "%s: fork: %s\n", PROGRAM_NAME, strerror(errno));
				return;
			}
			if (pid == 0) {
				set_signals_to_default();
				//this is basically what system() does:
				if (execle("/bin/sh", "sh", "-c", ext_cmd, NULL, __environ) == -1) {
					fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
					free(ext_cmd);
					return;
				}
				set_signals_to_ignore();
			}
			else {
				waitpid(pid, NULL, 0);
			}
			free(ext_cmd);
			sys_shell=sys_shell_status;
			return;
		}

//		#####EXEC VERSION#######
		//First of all, check the background char
		int is_background=0;
//		char *comm_tmp=NULL;
		if (strncmp(comm[args_n], "&", 1) == 0 && args_n != 0) {
			is_background=1;
			free(comm[args_n]);
			comm[args_n]=NULL;
		}
		else {
			size_t last_arg_size=strlen(comm[args_n]);
			if (comm[args_n][last_arg_size-1] == '&') {
				comm[args_n][last_arg_size-1]='\0';
				is_background=1;
			}
		}
		//Now, check that command is valid
/*
		if (strncmp(comm[0], "/", 1) == 0 && access(comm[0], F_OK) == 0) { 
			//If absolute path and exists...
			comm_tmp=xcalloc(comm_tmp, strlen(comm[0])+1, sizeof(char *));
			strcpy(comm_tmp, comm[0]);
		}
		else if (strcntchr(comm[0], '/') != -1 && access(comm[0], F_OK) == 0) { 
			//If relative path, and it exists, add it to current path (this includes 
			//"./prog_name")
			size_t path_len=strlen(path);
			size_t comm_len=strlen(comm[0]);
			comm_tmp=xcalloc(comm_tmp, comm_len+path_len+1, sizeof(char *));
			snprintf(comm_tmp, path_len+2, "%s/", path);				
			strncat(comm_tmp, comm[0], comm_len+path_len+2);
		}
		else //If only a filename, add the corresponding path, if any
			comm_tmp=get_cmd_path(comm[0]);
		//Now, if command doesn't exist...
		if (comm_tmp == NULL) {
			fprintf(stderr, "CliFM: %s: Command not found\n", comm[0]);
			free(comm_tmp);
			return;
		}
		*/
		//Look for wildcards
		int wildcard_index=-1, options_n=0;
		for (int i=0;comm[i]!=NULL;i++) {
			if (strcntchr(comm[i], '*') != -1 || strcntchr(comm[i], '?') != -1) {
				wildcard_index=i;
			}
			else if (comm[i][0] == '-')
				options_n++;
		}
		if (wildcard_index != -1) { //if there are wildcards...
			run_glob_cmd(options_n, is_background, comm, comm[wildcard_index]);
			return;
		}
		//If no wildcards...
//		if (access(comm_tmp, X_OK) == 0) //and command is executable...
			launch_execv(is_background, comm);
//		else
//			fprintf(stderr, "CliFM: %s: Permission denied\n", comm[0]);						
//		free(comm_tmp);
//		###END OF EXEC VERSION####
	}
	return;
}

void launch_execv(int is_background, char **comm)
{
	pid_t pid=fork();
	if (pid < 0) {
		fprintf(stderr, "%s: fork: %s\n", PROGRAM_NAME, strerror(errno));
		return;
	}
	if (pid >= 0) { //if fork was successfull (fork returns -1 if it fails)
		if (pid == 0) { //child process
			set_signals_to_default();
			setpgid(0, 0);
			if (execvp(comm[0], comm) == -1) {
				fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, comm[0], strerror(errno));
				exit(EXIT_FAILURE);
			}
			set_signals_to_ignore(); //block signals again
		}
		else { //parent process
			if (is_background)
				run_in_background(pid);
			else //if foreground...
				run_in_foreground(pid);
		}
	}
	else
		fprintf(stderr, "%s: Error running %s\n", PROGRAM_NAME, comm[0]);
}

void run_in_foreground(pid_t pid)
{
	//keep the process in the foreground
	tcsetpgrp(0, pid); /*allow the executed program to control the terminal. Otherwise,
	interactive (terminal based) applications like scripts, nano, or sudo will not work.*/
//	waitpid(pid, NULL, 0);
	
	int status;
	while (waitpid(pid, &status, 0) == -1) {
		if (errno != EINTR) { //Error other than EINTR
			status=-1;
			break; //So exit loop
		}
	}

	tcsetpgrp(shell_terminal, own_pid); //give back terminal control to the main program
}

void run_in_background(pid_t pid)
{
	/*every time a process is backgrounded, print the background list process number and 
	its pid*/
	int child_status=0;
	printf("[%d] %d\n", bg_proc_n+1, pid);
	//save the backgrounded pid into an array
	bg_proc[bg_proc_n]=pid;
	//keep it in the background
	waitpid(bg_proc[bg_proc_n++], &child_status, WNOHANG); //or: kill(pid, SIGCONT);
}

void list_jobs(void)
{
	if (bg_proc_n) {
		int alive_procs=0;
		for (int i=0;i<bg_proc_n;i++) {
			if (kill(bg_proc[i], 0) == 0) { //if process is alive...
				char *proc_name=get_proc_name(bg_proc[i]);
				printf("%d: %s\n", bg_proc[i], (proc_name != NULL) ? proc_name : 
																	 "UNKNOWN");
				if (proc_name != NULL)
					free(proc_name);
				alive_procs++;
			}
		}
		if (!alive_procs)
			printf("%s: jobs: No background process running\n", PROGRAM_NAME);				
	}
	else
		printf("%s: jobs: No background process running\n", PROGRAM_NAME);
}

void open_function(char **comm)
{
//	strncpy(old_pwd, path, strlen(path));
	//if no arguments or first argument is "."
	if (args_n == 0  || strcmp(comm[1], ".") == 0) {
		//go back to home only if command is 'cd', not 'open'
		if (strncmp(comm[0], "o", 1) == 0 || strncmp(comm[0], "open", 4) == 0) {
			puts("Usage: open ELN/filename [application]");
			return;
		}
		strncpy(old_pwd, path, strlen(path));
		update_path(DEFAULT_PATH);
		if (search_mark)
			search_mark=0;
		if (cd_lists_on_the_fly || strcmp(comm[0], "o") == 0 ||
						   strcmp(comm[0], "open") == 0) {
			free_dirlist();
			list_dir();
		}
		else chdir(path);
		return;
	}
	else {
		//check whether argument is string or number
		if (is_number(comm[1])) { //if number...
			int eln=(int)atoi(comm[1]); // convert char (comm[1]) into int (eln)
			if (eln == 0 || eln > files) { //if not valid...
				fprintf(stderr, "%s: %s: %d: No such ELN\n", PROGRAM_NAME, 
				(strcmp(comm[0], "o") == 0 || strcmp(comm[0], "open") == 0) 
													 ? "open" : "cd", eln);
				return;
			}
			//if a valid ELN, open it
			else open_element(eln, comm);
		}
		//if string...
		else {
			//if "cd .."
			if (strncmp(comm[1], "..", 2) == 0 ) {
				if (strcmp(comm[0], "cd") != 0) {
					fprintf(stderr, "Usage: open ELN/filename [application]\n");
					return;
				}
				char *path_tmp=NULL;
				if ((path_tmp=realpath(comm[1], NULL))) {
					update_path(path_tmp);
					free(path_tmp);
				}
				else {
					fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, comm[1], strerror(errno));
					return;
				}
				if (search_mark) search_mark=0;
				if (cd_lists_on_the_fly || strcmp(comm[0], "o") == 0 ||
										strcmp(comm[0], "open") == 0) {
					free_dirlist();
					list_dir();
				}
				else chdir(path);
			}
			else { //if a string and not ".."
				//if contains wildcards...
				if (strcntchr(comm[1], '*') != -1 || strcntchr(comm[1], '?') != -1) {
					char *destiny=cd_glob_path(comm[1]);
					if (destiny != NULL) {
						strncpy(old_pwd, path, strlen(path));
						strncat(path, "/", 1);
						strncat(path, destiny, strlen(destiny));
						free(destiny);
						if (search_mark)
							search_mark=0;
						if (cd_lists_on_the_fly || strcmp(comm[0], "o") == 0 ||
										   strcmp(comm[0], "open") == 0) {
							free_dirlist();
							list_dir();
						}
						else chdir(path);
						return;
					}
					else {
						free(destiny);
						fprintf(stderr, "%s: %s: No such file or directory\n", PROGRAM_NAME, 
								(strcmp(comm[0], "o") == 0 || strcmp(comm[0], "open") == 0) 
								? "open" : "cd");
						return;
					}
				}
				//if no wildcards...
				//remove final slash from comm[1], if any
				if (comm[1][strlen(comm[1])-1] == '/' && strcmp(comm[1], "/") != 0)
					comm[1][strlen(comm[1])-1]='\0';
				if (chdir(comm[1]) == -1) {
					if (errno == ENOTDIR) //if a file
						open_element(-1, comm);
					else {
						fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, comm[1], strerror(errno));
						return;
					}
				}
				else { //if directory exists...
					//if absolute path
					if (comm[1][0] == '/') update_path(comm[1]);
					else {
						char *path_tmp=NULL;
						path_tmp=xcalloc(path_tmp, strlen(path)+strlen(comm[1])+1, sizeof(char *));
						sprintf(path_tmp, "%s%s%s", path, (strcmp(path, "/") == 0) ? "" : 
																		   "/", comm[1]);
						update_path(path_tmp);
						free(path_tmp);
					}
					if (cd_lists_on_the_fly || strcmp(comm[0], "o") == 0 ||
											strcmp(comm[0], "open") == 0) {
						free_dirlist();
						list_dir();
					}	
				}
			}
		}
	}
	return;
}

void open_element(int eln, char **comm)
{
	int link_file=0;
	char *path_tmp=NULL;
	struct stat file_attrib;
	if (eln != -1) { //-1 means that comm[1] is a path to a file (neither an ELN nor path to dir)
		path_tmp=xcalloc(path_tmp, strlen(path)+strlen(dirlist[eln-1]->d_name)+2, sizeof(char *));
		sprintf(path_tmp, "%s/%s", path, dirlist[eln-1]->d_name);
		/*I need to add path 'cause path_tmp will be copied to path, which is the variable used
		by dir_list, which, unlike built-in C function like access(), knows nothing about
		system path (provided by chdir())*/
	}
	else {
		path_tmp=xcalloc(path_tmp, strlen(comm[1])+1, sizeof(char *));
		strcpy(path_tmp, comm[1]);
	}
	lstat(path_tmp, &file_attrib);
	char *linkname=NULL;
	//if folder...
	if (file_attrib.st_mode & S_IFDIR && !S_ISSOCK(file_attrib.st_mode)) {
		//For some reason, socket files are taken as directories, which makes CliFM crash. To
		//avoid this make sure that, when opening a dir, it is not a socket. 
		if (access(path_tmp, R_OK|X_OK) == 0) {
//			free_dirlist();
			int files_tmp=count_dir(path_tmp);
			if (files_tmp >= 0) {
				strncpy(old_pwd, path, strlen(path));
				update_path(path_tmp);
				free(path_tmp);
				if (search_mark)
					search_mark=0;
				if (cd_lists_on_the_fly || strcmp(comm[0], "o") == 0 ||
								   strcmp(comm[0], "open") == 0) {
					free_dirlist();
					files=files_tmp;
					list_dir();
				}
				else chdir(path);
				return;
			}
			else {
				fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
				free(path_tmp);
				return;
			}
		}
		else {
			fprintf(stderr, "%s: %s: %s: %s\n", PROGRAM_NAME, (strcmp(comm[0], "o") ==0 || 
				strcmp(comm[0], "open") == 0) ? "open" : "cd", dirlist[eln-1]->d_name, 
				strerror(errno));
			free(path_tmp);
			return;
		}
	}
	//if a symlink...
	else if (S_ISLNK(file_attrib.st_mode)) {
//		char buf[PATH_MAX]="";
		linkname=realpath(path_tmp, NULL);
		if (linkname == NULL) {
			fprintf(stderr, "%s: open: %s: Broken symbolic link\n", PROGRAM_NAME, 
														 dirlist[eln-1]->d_name);
			free(path_tmp);
			return;
		}
		stat(linkname, &file_attrib);
		//if symlink to a directory...
		if ((file_attrib.st_mode & S_IFDIR)  && !S_ISSOCK(file_attrib.st_mode)) {
			strncpy(old_pwd, path, strlen(path));
			update_path(linkname);
			free(path_tmp);
			if (search_mark)
				search_mark=0;
			if (cd_lists_on_the_fly || strcmp(comm[0], "o") == 0 ||
							   strcmp(comm[0], "open") == 0) {
				free_dirlist();
				list_dir();
			}
			else chdir(path);
			free(linkname);
			return;
		}
		else //if symlink to a file...
			link_file=1;
	}
	if (strcmp(comm[0], "cd") == 0) {
		fprintf(stderr, "%s: cd: %s: Not a directory\n", PROGRAM_NAME, path_tmp);
		free(path_tmp);
		return;
	}
	if (access(path_tmp, R_OK) == 0) {
		if (link_file) {
			strcpy(path_tmp, linkname);
		}
		free(linkname);		
		if (args_n == 1 || (args_n == 2 && strcmp(comm[2], "&") == 0)) { //if no opening application has been passed...
			if (!(flags & XDG_OPEN_OK)) { //if xdg-open doesn't exist...
				fprintf(stderr, "%s: Install xdg-utils or specify an application to open \
the file\nUsage: open [filename, ELN] [application]\n", PROGRAM_NAME);
				free(path_tmp);
				return;
			}
			//if xdg-open is available...
			if (flags & WINDOWED) {
				pid_t pid_open=fork();
				if (pid_open == 0) {
					set_signals_to_default();
					setpgid(0, 0);
					execle(xdg_open_path, "xdg-open", path_tmp, NULL, __environ);
					set_signals_to_ignore();
				}
				else {
					if (comm[2] != NULL && strcmp(comm[2], "&") == 0)
						run_in_background(pid_open);
					else
						run_in_foreground(pid_open);
				}
			}
			else {
				fprintf(stderr, "%s: An application must be specified\nUsage: open [filename/ELN] \
[application]\n", PROGRAM_NAME);
				free(path_tmp);
				return;
			}
		}
		else if (args_n >= 2) { //if application has been passed (as 2nd arg)...
			char *cmd_path;
			if ((cmd_path=get_cmd_path(comm[2])) != NULL) { /*if application exists, store its 
				path*/
				pid_t pid_open=fork();
				//if ampersand '&' (backgrounded)...
				if (comm[args_n] != NULL && strcmp(comm[args_n], "&") == 0) {
					if (pid_open == 0) {
						setpgid(0 ,0);
						execle(cmd_path, comm[2], path_tmp, NULL, __environ);
					}
					else
						run_in_background(pid_open);
				}
				else { //if no ampersand...  
					if (pid_open == 0) {
						set_signals_to_default();
						setpgid(0, 0);
						execle(cmd_path, comm[2], path_tmp, NULL, __environ);
						set_signals_to_ignore();
					}
					else
						run_in_foreground(pid_open);
				}
				free(cmd_path);
			}
			else {
				fprintf(stderr, "%s: %s: Application not found\n", (strcmp(comm[0], "o") ==0 || 
									   strcmp(comm[0], "open") == 0) ? "open" : "cd", comm[2]);
			}
		}
	}
	else {
		fprintf(stderr, "%s: %s: %s: Permission denied\n", PROGRAM_NAME, (strcmp(comm[0], "o") ==0 || 
			   strcmp(comm[0], "open") == 0) ? "open" : "cd", (eln != -1) ? 
										 dirlist[eln-1]->d_name : comm[1]);
	}
	free(path_tmp);
}

int save_sel(void)
//save selected elements into a tmp file. Returns 1 if success and 0 if error. This function
//allows the user to work with multiple instances of the program: he/she can select some files
//in the first instance and then execute a second one to operate on those files as he/she 
//whises.
{
	if (sel_n == 0) {
		remove(sel_file_user);
		return 0;
	}
	else {
		FILE *sel_fp=fopen(sel_file_user, "w");
		if (sel_fp == NULL) {
			perror("sel");
			return 0;
		}
		for (int i=0;i<sel_n;i++) {		
			fprintf(sel_fp, sel_elements[i]);
			fprintf(sel_fp, "\n");
		}
		fclose(sel_fp);
		return 1;
	}
}

void sel_function(char **comm)
{
	if (args_n == 0) {
		fprintf(stderr, "Usage: sel [ELN ELN-ELN filename path... n]\n");
		return;
	}
	char *sel_tmp=NULL, *sel_path=NULL;
	int i=0, j=0, k=0, exists=0;

	for (i=1;i<=args_n;i++) {

		//#####WILDCARDS######
		if (strcntchr(comm[i], '*') != -1 || strcntchr(comm[i], '?') != -1) {
			size_t path_len=strlen(path);
			glob_t globbed_files;
			int root_path=0;
			int ret=glob(comm[i], 0, NULL, &globbed_files);
			if (ret == 0) { //if there are matches...
				if (strcmp(path, "/") == 0) root_path=1;
				for (j=0; globbed_files.gl_pathv[j];j++) {
					//Append path to matching file
					sel_path=xcalloc(sel_path, (path_len+strlen(globbed_files.gl_pathv[j])+2), 
																			  sizeof(char *));
					sprintf(sel_path, "%s/%s", (root_path) ? "" : path, 
											globbed_files.gl_pathv[j]);
					for (k=0;k<sel_n;k++) {
						if (strcmp(sel_elements[k], sel_path) == 0) {
							exists=1;
							break;
						}
					}
					if (!exists) { //if the element isn't already selected...
						sel_elements=xrealloc(sel_elements, sizeof(char **)*(sel_n+1));
						sel_elements[sel_n]=xcalloc(sel_elements[sel_n], strlen(sel_path)+1, sizeof(char *));
						strcpy(sel_elements[sel_n++], sel_path);
					}
					free(sel_path);
				}
			}
			else fprintf(stderr, "%s: %s: No Matches found\n", PROGRAM_NAME, comm[i]);
			globfree(&globbed_files);
		}

		//#####ELN#####
		else if (is_number(comm[i]) || (strcntchr(comm[i], '-') != -1 && 
			(unsigned)digits_in_str(comm[i]) == strlen(comm[i])-1)) {

			//#####RANGE########
			if (strcntchr(comm[i], '-') != -1) {
				char *range_start=strbfr(comm[i], '-');
				char *range_end=straft(comm[i], '-');
				if (range_start && range_end) {
					if (is_number(range_start) && is_number(range_end)) {
						if (atoi(range_start) == 0 || atoi(range_start) > files ||
								  atoi(range_end) == 0 || atoi(range_end) > files) {
							free(range_start);
							free(range_end);
							fprintf(stderr, "sel: %s: Wrong range\n",comm[i]);
							continue;
						}
						for (int j=atoi(range_start)-1;j<atoi(range_end);j++) {
							sel_tmp=xcalloc(sel_tmp, strlen(path)+strlen(dirlist[j]->d_name)+2, 
																	sizeof(char *));
							sprintf(sel_tmp, "%s%s%s", path, (strcmp(path, "/") == 0) ? 
														 "" : "/", dirlist[j]->d_name);
							for (int k=0;k<sel_n;k++) {
								if (strcmp(sel_elements[k], sel_tmp) == 0) {
									exists=1;
									break;
								}
							}
							if (!exists) {
								sel_elements=xrealloc(sel_elements, sizeof(char **)*(sel_n+1));
								sel_elements[sel_n]=xcalloc(sel_elements[sel_n], strlen(sel_tmp), sizeof(char *));
								strcpy(sel_elements[sel_n++], sel_tmp);
							}
							else fprintf(stderr, "sel: %s: Already selected\n", sel_tmp);
							free(sel_tmp);
							exists=0;
						}
						free(range_start);
						free(range_end);
					}
					else fprintf(stderr, "sel: %s: Wrong range\n", comm[i]);
				}
				else fprintf(stderr, "sel: Error\n");
				continue;
			}

			//#####SINGLE NUMBER####
			if (atoi(comm[i]) == 0 || atoi(comm[i]) > files) {
				fprintf(stderr, "sel: %s: No such ELN\n", comm[i]);
				continue;
			}
			sel_tmp=xcalloc(sel_tmp, strlen(path)+strlen(dirlist[atoi(comm[i])-1]->d_name)+2, sizeof(char *));
			strcpy(sel_tmp, path);
			if (strcmp(path, "/") !=0)
				strcat(sel_tmp, "/");
			strcat(sel_tmp, dirlist[atoi(comm[i])-1]->d_name);
			for (j=0;j<sel_n;j++) {
				if (strcmp(sel_elements[j], sel_tmp) == 0) {
					exists=1;
					break;
				}
			}
			if (!exists) {
				sel_elements=xrealloc(sel_elements, sizeof(char **)*(sel_n+1));
				sel_elements[sel_n]=xcalloc(sel_elements[sel_n], strlen(sel_tmp), sizeof(char *));
				strcpy(sel_elements[sel_n++], sel_tmp);
			}
			else fprintf(stderr, "sel: %s: Already selected\n", sel_tmp);
			free(sel_tmp);
			continue;
		}
		
		//######STRING#######
		else {
			//if a filename in the current directory...
			int sel_is_filename=0, sel_is_relative_path=0;
			for (j=0;j<files;j++)
				if (strcmp(dirlist[j]->d_name, comm[i]) == 0)
					sel_is_filename=1;
			if (!sel_is_filename) {
				//if a path (contains a slash)...
				if (strcntchr(comm[i], '/') != -1) {
					if (comm[i][0] != '/') //if relative path (path/to/file)
						sel_is_relative_path=1;
					struct stat file_attrib;
					if (stat(comm[i], &file_attrib) != 0) {
						perror("sel");
						continue;
					}
				}
				else { //if string, but neither a filename nor a path...
					fprintf(stderr, "sel: %s: No such file or directory\n", comm[i]);
					continue;
				}
			}
			if (sel_is_filename || sel_is_relative_path) { //add path to filename or relative path
				sel_tmp=xcalloc(sel_tmp, strlen(path)+strlen(comm[i])+2, sizeof(char *));
				sprintf(sel_tmp, "%s/%s", path, comm[i]);
			}
			else { //if full path (/path/to/file)...
				sel_tmp=xcalloc(sel_tmp, strlen(comm[i])+1, sizeof(char *));		
				strcpy(sel_tmp, comm[i]);
			}
			for (j=0;j<sel_n;j++) { //check whether the selected element is already in the 
				//selection box
				if (strcmp(sel_elements[j], sel_tmp) == 0) {
					exists=1;
					break;
				}
			}
			if (!exists) {
				sel_elements=xrealloc(sel_elements, sizeof(char **)*(sel_n+1));
				sel_elements[sel_n]=xcalloc(sel_elements[sel_n], strlen(sel_tmp)+1, sizeof(char *));
				strcpy(sel_elements[sel_n++], sel_tmp);
			}
			else fprintf(stderr, "sel: %s: Already selected\n", sel_tmp);
			free(sel_tmp);
			continue;
		}

	}
	save_sel();
	if (sel_n > 10)
		printf("%d elements are now in the Selection Box\n", sel_n);
	else if (sel_n > 0) {
		printf("%d selected %s:\n", sel_n, (sel_n == 1) ? "element" : "elements");
		for (int i=0;i<sel_n;i++)
			printf("  %s\n", sel_elements[i]);
	}
}

void show_sel_files(void)
{
	if (clear_screen)
		CLEAR;
	printf("%sSelection Box%s%s\n\n", white, NC, default_color);
	if (sel_n == 0)
		printf("Empty\n");
	else {
		for(int i=0;i<sel_n;i++) {
			colors_list(sel_elements[i], i);
		}
	}
	printf("\n%s%sPress Enter key to continue... ", NC, default_color);
	getchar();
	free_dirlist();
	list_dir();
}

void deselect (char **comm)
{
	if (args_n == 1 && strcmp(comm[1], "*") == 0) { //if asterisk is passed as first argument...
		if (sel_n > 0) {
			for (int i=0;i<sel_n;i++)
				free(sel_elements[i]);
			sel_n=0;
			save_sel();
			return;
		}
		else {
			puts("There are no selected elements.");
			return;
		}
	}
	int i, desel_n=0, c=0, length=0; 
	if (clear_screen)
		CLEAR;
	printf("%sSelection Box%s%s\n\n", white, NC, default_color);
	if (sel_n == 0) {
		printf("Empty\n\nPress Enter key to continue... ");
		getchar();
		free_dirlist();
		list_dir();
		return;
	}
	for (i=0;i<sel_n;i++)
		colors_list(sel_elements[i], i);
	printf("\n%s%sEnter 'q' to quit.\n", NC, default_color);
	printf("Elements to be deselected (ex: 1 2 6, or *)? ");
	char **desel_elements=NULL;
	desel_elements=xcalloc(desel_elements, sel_n, sizeof(char **));
	while (c != '\n') { //exit loop when enter is pressed.
	desel_elements[desel_n]=xcalloc(desel_elements[desel_n], 4, sizeof(char *)); //4 = 4 digits num (max 9,999)
		if (desel_n == sel_n) //do not allow more deselections than selections are.
			break;
		while (!isspace(c=getchar())) { 
			desel_elements[desel_n][length++]=c;
		}
		desel_elements[desel_n++][length]='\0'; //add end of string char to the last position
		length=0;
	}
	desel_elements=xrealloc(desel_elements, sizeof(char **)*desel_n);
	for (i=0;i<desel_n;i++) { //validation
		if (atoi(desel_elements[i]) == 0 || atoi(desel_elements[i]) > sel_n) 
		{
			if (strcmp(desel_elements[i], "q") == 0) {
				for (i=0;i<desel_n;i++)
					free(desel_elements[i]);
				free(desel_elements);
				free_dirlist();
				list_dir();
				return;
			}
			else if (strcmp(desel_elements[i], "*") == 0) {
				//clean the sel array
				for (i=0;i<sel_n;i++)
					free(sel_elements[i]);
				sel_n=0;
				for (i=0;i<desel_n;i++)
					free(desel_elements[i]);
				save_sel();
				free(desel_elements);
				free_dirlist();
				list_dir();
				return;
			}
			else {
				printf("%s is not a valid element.\n", desel_elements[i]);
				for (i=0;i<desel_n;i++)
					free(desel_elements[i]);
				free(desel_elements);
				return;
			}
		}
	}
	//if a valid ELN and not asterisk...
	//store the full path of all the elements to be deselected in a new array (desel_path)
	char **desel_path=NULL;
	desel_path=xcalloc(desel_path, desel_n, sizeof(char **));
	for (i=0;i<desel_n;i++) {
		int desel_int=atoi(desel_elements[i]);
		desel_path[i]=xcalloc(desel_path[i], strlen(sel_elements[desel_int-1]), sizeof(char *));
		strcpy(desel_path[i], sel_elements[desel_int-1]);
	}
	//saerch the sel array for the path of the element to deselect and store its index
	for (i=0;i<desel_n;i++) {
		int desel_index=0;
		for (int k=0;k<sel_n;k++) {
			if (strcmp(sel_elements[k], desel_path[i]) == 0) {
				desel_index=k; 
				break;
			}
		}
		//once the index was found, rearrange the sel array removing the deselected element
		for (int j=desel_index;j<sel_n-1;j++)
			strcpy(sel_elements[j], sel_elements[j+1]);
	}
	/*free the last element of the old sel array. It won't be used anymore, for it contains
	the same value as the preceding element thanks to the above array rearrangement.*/
	free(sel_elements[sel_n-1]);
	//reallocate the sel array according to the new size.
	sel_n=sel_n-desel_n;
	if (sel_n) sel_elements=xrealloc(sel_elements, sizeof(char **)*sel_n);
	//deallocate local arrays
	for (i=0;i<desel_n;i++) {
		free(desel_path[i]);
		free(desel_elements[i]);
	}
	free(desel_path);
	free(desel_elements);
	if (args_n > 0) {
		for (i=1;i<=args_n;i++)
			free(comm[i]);
		comm=xrealloc(comm, sizeof(char **)*1);
		args_n=0;
	}
	save_sel();
	deselect(comm);
}

void touch_function(char **comm)
{
	if (args_n > 0) {
		for (int i=1;i<=args_n;i++) {
			for (int j=0;j<files;j++) //check whether file exists.
				if (strcmp(dirlist[j]->d_name, comm[i]) == 0) {
					puts("touch: File already exists");
					char rl_question[60];
					sprintf(rl_question, "%s%sDo you want to overwrite it? (y/n) ", NC_b, 
																			  default_color);
					/*This function gets input from readline disabling history and reenables 
					it after getting the input line.*/
					char *answer=rl_no_hist(rl_question);	
					if (strcmp(answer, "y") == 0 || strcmp(answer, "Y") == 0); //Go on.
					else if (strcmp(answer, "n") == 0 || strcmp(answer, "N") == 0)
						return;
					else {
						puts("touch: Wrong answer");
						return;
					}
				}
			FILE *fp;
			fp=fopen(comm[i], "w+");
			if (fp == NULL) {
				perror("touch");
//				printf("Press Enter key to continue... ");
//				getchar();
				return;
			}
			else
				fclose(fp);
		}
		log_function(comm);
		free_dirlist();
		list_dir();
	}
	else
		puts("Usage: touch filename(s)");
}

void mkdir_function(char **comm)
{
	if (args_n > 0) {
		for (int i=1;i<=args_n;i++) {			
			//create directory with 755 permissions
			if (mkdir(comm[i], S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH) == -1) {
				perror("mkdir");
//				printf("Press Enter key to continue... ");
//				getchar();
				return;
			}
		}
		log_function(comm);
		free_dirlist();
		list_dir();
	}
	else
		puts("Usage: mkdir dirname(s)");
}

void remove_function(char **comm)
{
	//if no arguments and no selected files...
	if (!args_n && !sel_n) {
		puts("Usage: rm [ELN, filename, path]");
		return;
	}
	//check command existence (just once)
	static char rm_path[PATH_MAX]="";
/*	if (strcmp(rm_path, "not found") == 0) {
		fprintf(stderr, "CliFM: 'rm' command not found\n");
		return;
	}*/
	if (rm_path[0] == '\0') {
		char *rm_path_tmp=NULL;
		if ((rm_path_tmp=get_cmd_path("rm")) != NULL) {
			strncpy(rm_path, rm_path_tmp, strlen(rm_path_tmp));
			free(rm_path_tmp);
		}
		else {
			strncpy(rm_path, "not found", 10);
			fprintf(stderr, "%s: 'rm': command not found\n", PROGRAM_NAME);
			return;
		}
	}
	//if command exists...
	//if there are selected elements and no arguments were passed (or first arg is sel), delete them.
	if ((args_n && strcmp(comm[1], "sel") == 0) || (!args_n && sel_n > 0)) {
		if (!sel_n) {
			fprintf(stderr, "rm: No selected files");
			return;
		}
		int i=0; //answer=0;
		puts("Elements to be deleted:");
		for (i=0;i<sel_n;i++)
			colors_list(sel_elements[i], i);
		char rl_question[50];
		sprintf(rl_question, "%s%sDo you wish to delete %s? (y/n) ", NC_b, default_color, 
													(sel_n > 1) ? "them" : "it");
		//This function gets input from readline disabling history and reenables it after getting 
		//the input line.
		char *answer=rl_no_hist(rl_question);	
		if (strcmp(answer, "y") == 0 || strcmp(answer, "Y") == 0) {
			struct stat file_attrib;
			int sel_tmp=sel_n;
			for (i=0;i<sel_tmp; i++) {
				lstat(sel_elements[i], &file_attrib);
				pid_t pid_rm=fork();
				if (pid_rm == 0) {
					set_signals_to_default();
					if ((file_attrib.st_mode & S_IFDIR) || S_ISLNK(file_attrib.st_mode))
						execle(rm_path, "rm", "-r", sel_elements[i], NULL, __environ);
					else
						execle(rm_path, "rm", sel_elements[i], NULL, __environ);
					set_signals_to_ignore();
				}
				else
					waitpid(pid_rm, NULL, 0);
				//if backups enabled and not in the backups folder (or: do not back up backups).				
				if (backup == 1 && strcmp(path, BK_DIR) != 0)
					backup_function(comm, sel_elements[i]);
				/*One problem: if rm asks the user whether he/she wants to remove a write-protected
				file, and if the user answers "no", the selected element will be freed, and
				thereby deselected anyway. But this shouldn't be so.*/
				free(sel_elements[i]);
				sel_n=sel_n-1;
			}
			log_function(comm);
			free(answer);
			save_sel();
			return;
		}
		else if (strcmp(answer, "n") == 0 || strcmp(answer, "N") == 0) {
			free(answer);
			return;
		}
		else {
			fprintf(stderr, "rm: Invalid answer");
			free(answer);
			return;
		}
	}
	//if not sel...
	int files_index=args_n+1;
	if (args_n > 0) {
		int glob=0, options_n=0;
		char **comm_tmp=NULL; //a tmp comm array to be used with exec()
		comm_tmp=xcalloc(comm_tmp, (unsigned)args_n+2, sizeof(char **));
		for (int i=0;i==0 || comm[i][0]=='-';i++) { //copy comm and arguments to the tmp array
			comm_tmp[i]=xcalloc(comm_tmp[i], strlen(comm[i]), sizeof(char *));
			strncpy(comm_tmp[i], comm[i], strlen(comm[i]));
		}
		for (int i=1;i<=args_n;i++) {
			if (comm[i][0] != '-') { //if no "-", here files begin
				files_index=i;
				break;
			}
			else //if an option...
				options_n++; //this variable will be passed to run_glob_cmd, if necessary
		}
		for (int i=files_index;i<=args_n;i++) {
			glob=0;
			if (is_number(comm[i])) {
				int eln=atoi(comm[i]);
				if (eln == 0 || eln > files) {
					fprintf(stderr, "%s: %d: No such ELN\n", PROGRAM_NAME, eln);
					if (i == args_n) {
						for (int i=0;i<=args_n;i++) {
							free(comm_tmp[i]);
						}
						free(comm_tmp);
						return;
					}
					else continue;
				}
				comm_tmp[i]=xcalloc(comm_tmp[i], strlen(dirlist[eln-1]->d_name), sizeof(char *));
				strncpy(comm_tmp[i], dirlist[eln-1]->d_name, strlen(dirlist[eln-1]->d_name));
				if (backup == 1 && strcmp(path, BK_DIR) != 0) {
					char *full_path=NULL;
					full_path=xcalloc(full_path, strlen(path)+strlen(dirlist[eln-1]->d_name)+1, 
																	  sizeof(char *));
					sprintf(full_path, "%s/%s", path, dirlist[eln-1]->d_name);
					backup_function(comm, full_path);
					free(full_path);
				}
			}
			else { //if string...
				if (strcntchr(comm[i], '*') != -1 || strcntchr(comm[i], '?') != -1) {
					glob=1;
					run_glob_cmd(options_n, 0, comm_tmp, comm[i]);
				}
				else {
					comm_tmp[i]=calloc(strlen(comm[i]), sizeof(comm_tmp));
					strncpy(comm_tmp[i], comm[i], strlen(comm[i]));
				}
				if (backup ==1 && strcmp(path, BK_DIR) != 0) {
					char *full_path=NULL;
					full_path=xcalloc(full_path, strlen(path)+strlen(comm[i])+2, sizeof(char *));
					sprintf(full_path, "%s/%s", path, comm[i]);
					backup_function(comm, full_path);
					free(full_path);	
				}
			}
		}
		if (!glob) {
			free(comm_tmp[args_n+1]);
			comm_tmp[args_n+1]=NULL; //last argument passed to exec must be NULL.
			log_function(comm);
			char *cmd_ret=check_cmd_stderr(rm_path, comm_tmp);
			if (cmd_ret) {
				if (strlen(cmd_ret) == 0) { //if zero, stderr was empty, and thereby the cmd succeded
					free_dirlist();
					list_dir();
				}
				else
					fprintf(stderr, "%s", cmd_ret);
				free(cmd_ret);
			}
			else
				fprintf(stderr, "Error\n");
			for (int i=0;i<=args_n;i++) {
				free(comm_tmp[i]);
			}
			free(comm_tmp);
			return;
		}
		//if glob... free everything
		for (int i=0;i<=args_n;i++) {
			free(comm_tmp[i]);
		}
		free(comm_tmp);
		free_dirlist();
		list_dir();
	}
}

void symlink_function(char **comm)
{
	if (args_n == 0) {
		fprintf(stderr, "Usage: ln ELN/filename path/to/symlink");
		return;
	}
	else if (args_n == 1) {
		fprintf(stderr, "link: Argument missing\nUsage: ln [ELN, filename] path/to/symlink");
		return;
	}
	else {
		int i=0, src_exists=0;//digit=1, 
		struct stat file_attrib;
		char *source=NULL, *destiny=NULL, *ln_tmp=NULL;
		ln_tmp=xcalloc(ln_tmp, PATH_MAX, sizeof(char *));
/*		size_t str_len=strlen(comm[1]);
		for (i=0;(unsigned)i<str_len;i++) {
			if (!(isdigit(comm[1][i]))) {
				digit=0;
				break;
			}
		}*/
		if (is_number(comm[1])) {
			if (atoi(comm[1]) == 0 || atoi(comm[1]) > files) {
				fprintf(stderr, "link: No such ELN\n");
				free(ln_tmp);
				return;
			}
			else {
				snprintf(ln_tmp, PATH_MAX, "%s/%s", path, dirlist[atoi(comm[1])-1]->d_name);
				source=xcalloc(source, strlen(ln_tmp), sizeof(char *));
				strcpy(source, ln_tmp);
				free(ln_tmp);
				src_exists=1;
			}
		}
		else { //if string
			//If first arg is string it will be valid no matter the file or dir exists
			//+or not, for the user is allowed to create a symlink to a non-existent location.
			//Allow the user to symlink a selected file into the current folder ('ln sel filename', or 'ln filename')
			if (strcmp(comm[1], "sel") == 0)
				if (sel_n == 0) {
					fputs("link: No selected elements", stderr);
					free(ln_tmp);
					return;
				}
				else if (sel_n > 1) {
					fputs("link: There are more than one selected element", stderr);
					free(ln_tmp);
					return;
				}
				else {
					source=xcalloc(source, strlen(sel_elements[0]), sizeof(char *));
					strcpy(source, sel_elements[0]);
					src_exists=1;
					free(ln_tmp);
				}
			else if (strcntchr(comm[1], '/') != -1) {
				if (lstat(comm[1], &file_attrib) == 0)
					src_exists=1; //this variable, when zero, will warn the user whenever 
					//he/she attempts to create a symlink to a non-existent location.
				source=xcalloc(source, strlen(comm[1]), sizeof(char *));
				strcpy(source, comm[1]);
				free(ln_tmp);
			}
			else {
				int index=0;
				for (i=0;i<files;i++) {
					if (strcmp(dirlist[i]->d_name, comm[1]) == 0) {
						src_exists=1;
						index=i;
						break;
					}
				}
				if (src_exists)
					snprintf(ln_tmp, PATH_MAX, "%s/%s", path, dirlist[index]->d_name);
				else
					sprintf(ln_tmp, "%s/%s", path, comm[1]);				
				source=xcalloc(source, strlen(ln_tmp), sizeof(char *));
				strcpy(source, ln_tmp);
				free(ln_tmp);
			}
		}
		if (!src_exists) { //warn the user that source doesn't exist.
			char rl_question[128]="";
			snprintf(rl_question, 128, "The symlink points to a non-existent location.\n\
Do you want to create it anyway? (y/n) ");
			char *answer=rl_no_hist(rl_question);
			if (strcmp(answer, "y") == 0 || strcmp(answer, "Y") == 0);
			else if (strcmp(answer, "n") == 0 || strcmp(answer, "N") == 0) {
				free(answer);
				free(source);
				return;
			}
			else {
				fputs("CLiFM: link: Invalid answer", stderr);
				free(answer);
				free(source);
				return;
			}
			free(answer);
		}
		//validate now the second argument...
		if (strcntchr(comm[2], '/') != -1) {
			if (lstat(comm[2], &file_attrib) == 0) {
				fputs("link: Destiny already exists", stderr);
				free(source);
				return;
			}
			else {
				destiny=xcalloc(destiny, strlen(comm[2]), sizeof(char *));
				strcpy(destiny, comm[2]);
			}
		}
		else {
			char *dest_tmp=NULL;
			dest_tmp=xcalloc(dest_tmp, PATH_MAX, sizeof(char *));
			snprintf(dest_tmp, PATH_MAX, "%s/%s", path, comm[2]);
			destiny=xcalloc(destiny, strlen(dest_tmp), sizeof(char *));
			strcpy(destiny, dest_tmp);
			free(dest_tmp);
		}
		if (symlink(source, destiny) == -1)
			perror("link");
		else {
			free_dirlist();
			list_dir(); 
		}
		log_function(comm);
		free(source);
		free(destiny);
	}
}

void search_function(char **comm)
{
	if (!comm || !comm[0]) return;
	char *search_str=comm[0]+1;
	if (strcntchr(search_str, '*') != -1 || strcntchr(search_str, '?') != -1) {
		glob_t globbed_files;
		int ret=glob(search_str, 0, NULL, &globbed_files);
		unsigned index=0;
		if (ret == 0) {
			for (unsigned i=0; globbed_files.gl_pathv[i];i++) {
				for (unsigned j=0;j<(unsigned)files;j++)
					if (strcmp(globbed_files.gl_pathv[i], dirlist[j]->d_name) == 0)
						index=j;
				colors_list(globbed_files.gl_pathv[i], (index) ? index : 0);
			}
		}
		else fprintf(stderr, "%s: No matches found\n", PROGRAM_NAME);
		globfree(&globbed_files);		
	}
	else {
		int found=0;
		for (unsigned i=0;i<(unsigned)files;i++)
			if (strstr(dirlist[i]->d_name, search_str)) {
				colors_list(dirlist[i]->d_name, i);
				found=1;
			}
		if (!found)
			fprintf(stderr, "%s: No matches found\n",PROGRAM_NAME);
	}
}

char *check_cmd_stderr(char *cmd, char **args)
/*Execute a given command redirecting its stderr to a temp file. Finally return the stderr 
string or NULL in case of failure. NOTE: if temp file is empty, the command succeded.*/
{
	FILE *cmd_err_fp=fopen(STDERR_FILE, "w");
	int stderr_bk=dup(STDERR_FILENO); //save original stderr
	dup2(fileno(cmd_err_fp), STDERR_FILENO); //redirect stderr to the tmp file
	fclose(cmd_err_fp);
	//exec the corresponding command with execve
	pid_t cmd_pid=fork();
	if (cmd_pid == 0) {
		set_signals_to_default();
		setpgid(0, 0);
		execve(cmd, args, __environ);
		set_signals_to_ignore();
	}
	else run_in_foreground(cmd_pid);
	dup2(stderr_bk, STDERR_FILENO); //restore original stderr
	close(stderr_bk);
	//Now read the tmp file
	if (access(STDERR_FILE, F_OK) == 0) {
		FILE *cmd_fp=fopen(STDERR_FILE, "r");
		char *line=NULL;
		line=xcalloc(line, MAX_LINE, sizeof(char *));
		//50 is an arbitrary number!
		char *line_ret=NULL;
		line_ret=xcalloc(line_ret, MAX_LINE*50, sizeof(char *));
		if (cmd_fp != NULL) {
			while (fgets(line, sizeof(line), cmd_fp)) {
				strcat(line_ret, line);
			}
			fclose(cmd_fp);
			remove(STDERR_FILE);
			free(line);
			return line_ret;
		}
		else { 
			free(line);
			free(line_ret);
			return NULL;
		}
	}
	else return NULL;
}

void copy_function(char **comm)
{
	/*Set cp and mv paths only once by running get_cmd_path and storing its result in a 
	static variable*/
	static char *cp_path=NULL;
	static char *mv_path=NULL;
	if (!move_mark) {
		if (!cp_path) {
			char *cp_path_tmp=NULL;
			if ((cp_path_tmp=get_cmd_path("cp")) != NULL) {
				size_t cp_path_tmp_len=strlen(cp_path_tmp);
				cp_path=xcalloc(cp_path, cp_path_tmp_len+1, sizeof(char *));
				strncpy(cp_path, cp_path_tmp, cp_path_tmp_len);
				free(cp_path_tmp);
			}
			else if (!move_mark) { //if command is cp
				fprintf(stderr, "%s: 'cp' command not found\n", PROGRAM_NAME);
				return;
			}
		}
	}
	else {
		if (!mv_path) {
			char *mv_path_tmp=NULL;
			if ((mv_path_tmp=get_cmd_path("mv")) != NULL) {
				size_t mv_path_tmp_len=strlen(mv_path_tmp);
				mv_path=xcalloc(mv_path, mv_path_tmp_len+1, sizeof(char *));
				strncpy(mv_path, mv_path_tmp, mv_path_tmp_len);
				free(mv_path_tmp);
			}
			else if (move_mark) { //if command is mv
				fprintf(stderr, "%s: 'mv' command not found\n", PROGRAM_NAME);
				return;
			}
		}
	}
	//####If the corresponding command exists...
	
	//#####If there are SELECTED FILES#######
	//Possible cases: 1 - paste (mv or cp); 2 - paste sel; 3 - paste sel destiny
	if (args_n == 0 || (args_n <= 2 && strcmp(comm[1], "sel") == 0)) {
		if (sel_n > 0) {
			int dest_given=0;
			char *dest_path=NULL;
			struct stat file_attrib;
			if (args_n >= 2) {
				int dest_is_num=0;
				if (is_number(comm[2])) dest_is_num=1;
				if (dest_is_num) {
					if (atoi(comm[2]) == 0 || atoi(comm[2]) > files) {
						fprintf(stderr, "%s: No such ELN\n", comm[0]);
						return;
					}
					else { //if a valid ELN...
						if (access(dirlist[atoi(comm[2])-1]->d_name, 
											R_OK|W_OK|X_OK) == -1) {
							fprintf(stderr, "%s: %s: Permission denied\n", comm[0], 
											   dirlist[atoi(comm[2])-1]->d_name);
							return;
						}
						lstat(dirlist[atoi(comm[2])-1]->d_name, &file_attrib);
						if ((file_attrib.st_mode & S_IFDIR) && !S_ISSOCK(file_attrib.st_mode)) {
							dest_path=xcalloc(dest_path, strlen(dirlist[atoi(comm[2])-1]->d_name)+1, sizeof(char *));
							strcpy(dest_path, dirlist[atoi(comm[2])-1]->d_name);
							dest_given=1;
						}
						else {
							fprintf(stderr, "%s: '%s' is not a directory\n", comm[0], 
												   dirlist[atoi(comm[2])-1]->d_name);
							return;
						}
					}
				}
				else { //if string...
					if (access(comm[2], R_OK|W_OK|X_OK) == -1) {
						fprintf(stderr, "%s: %s: Permission denied\n", comm[0], 
										   dirlist[atoi(comm[2])-1]->d_name);
						return;
					}
					if (lstat(comm[2], &file_attrib) == 0) {
						if ((file_attrib.st_mode & S_IFDIR) && !S_ISSOCK(file_attrib.st_mode)) {
							dest_path=xcalloc(dest_path, strlen(comm[2])+1, sizeof(char *));
							strcpy(dest_path, comm[2]);
							dest_given=1;
						}
						else {
							fprintf(stderr, "%s: %s: Not a directory\n", comm[0], comm[2]);
							return;
						}
					}
					else {
						fprintf(stderr, "%s: %s: Doesn't exist\n", comm[0], comm[2]);
						return;
					}
				}
			}
			int comm_fail=0, sel_tmp=sel_n;
			//Once validated, execute the corresponding command
			for (int i=0;i<sel_tmp;i++) {				
				lstat(sel_elements[i], &file_attrib);
				pid_t pid_copy=fork();
				if (pid_copy == 0) {
					set_signals_to_default();
					//if directory or symlink. Cp demands the -r option	to copy symlinks
					if ((file_attrib.st_mode & S_IFDIR) || S_ISLNK(file_attrib.st_mode)) {
						if (move_mark) {
							if (execle(mv_path, "mv", sel_elements[i], (dest_given) ? 
							dest_path: ".", NULL, __environ) == -1)
								comm_fail=1;
						}
						else {
							if (execle(cp_path, "cp", "-r", sel_elements[i], (dest_given) 
							? dest_path : ".", NULL, __environ) == -1)
								comm_fail=1;
						}
					}
					//if a file...
					else {
						if (execle((move_mark) ? mv_path : cp_path, (move_mark) ? "mv" : 
							"cp", sel_elements[i], (dest_given) ? dest_path : ".", NULL,
							__environ) == -1)
								comm_fail=1;
					}
					set_signals_to_ignore();
				}
				else waitpid(pid_copy, NULL, 0);
				if (!comm_fail) {
					sel_n=sel_n-1;
					free(sel_elements[i]); //clean the sel array
					log_function(comm);
				}
			}
			if (dest_given) free(dest_path);
			save_sel();
			if (!comm_fail) { /*No errors. In case of error, don't refresh the files list 
			in order not to reduce the error's visibility.*/ 	
				free_dirlist();
				list_dir();
			}
			if (move_mark) move_mark=0;			
			return;
		}
		else {
			fprintf(stderr, "%s: No selected files\n", comm[0]);
			return;
		}
	}
	
	//####If NOT SEL#######...
	char *source=NULL, *destiny=NULL;
	for (int i=args_n-1;i<=args_n;i++) { //check two last arguments: source and destiny
		if (is_number(comm[i])) {
			if (atoi(comm[i]) != 0 && !(atoi(comm[i]) > files)) { //if valid ELN...
				if (i == args_n-1) { //source
					source=xcalloc(source, strlen(dirlist[atoi(comm[i])-1]->d_name)+1, sizeof(char *));					
					strcpy(source, dirlist[atoi(comm[i])-1]->d_name);
				}
				else if (i == args_n) { //destiny
					destiny=xcalloc(destiny, strlen(dirlist[atoi(comm[i])-1]->d_name)+1, sizeof(char *));					
					strcpy(destiny, dirlist[atoi(comm[i])-1]->d_name);
				}
			}
			else {
				fprintf(stderr, "%s: %s: No such ELN\n", (move_mark) ? "mv": "cp", comm[i]);
				if (source) free(source); //or: if (i == args_n)
				return;
			}
		}
		else { //if string, just send the two last arguments (source and destiny) tp cp/mv.
			if (i == args_n-1) {
				source=xcalloc(source, strlen(comm[i])+1, sizeof(char *));
				strncpy(source, comm[i], strlen(comm[i]));
			}
			else if (i == args_n) {
				destiny=xcalloc(destiny, strlen(comm[i])+1, sizeof(char *));
				strncpy(destiny, comm[i], strlen(comm[i]));
			}
		}
	}

	//copy command and arguments into an array to be used by execve 
	char **cmd_array=NULL;
	cmd_array=xcalloc(cmd_array, args_n+2, sizeof(char **));
	for (int i=0;i<args_n-1;i++) { //copy command and flags, if any
		cmd_array[i]=xcalloc(cmd_array[i], strlen(comm[i])+1, sizeof(char *));
		strcpy(cmd_array[i], comm[i]);
	}
	//copy source and destiny
	cmd_array[args_n-1]=xcalloc(cmd_array[args_n-1], strlen(source)+1, sizeof(char *));
	strcpy(cmd_array[args_n-1], source);
	cmd_array[args_n]=xcalloc(cmd_array[args_n], strlen(destiny)+1, sizeof(char *));
	strcpy(cmd_array[args_n], destiny);
	cmd_array[args_n+1]=NULL; //add a NULL last element

	//run the cmd and return stderr
	char *cmd_ret=check_cmd_stderr((move_mark) ? mv_path : cp_path, cmd_array);
	if (cmd_ret != NULL) {
		if (strlen(cmd_ret) == 0) { //if zero, stderr was empty, and thereby the cmd succeded
			free_dirlist();
			list_dir();
		}
		else
			fprintf(stderr, "%s\n", cmd_ret);
		free(cmd_ret);
	}
	else
		fprintf(stderr, "%s: Error\n", (move_mark) ? "mv" : "cp");	
	//free everything
	for (int i=0;cmd_array[i];i++) free(cmd_array[i]);
	free(cmd_array);
	free(source);
	free(destiny);
	if (move_mark) move_mark=0;
	return;
}

void move_function(char **comm)
{
	move_mark=1;
	copy_function(comm);
}

int get_bm_n(char *bookmarks_file)
{
	FILE *bm_fp=fopen(bookmarks_file, "r");
	char line[MAX_LINE]="";
	int bm_n=0;
	//get the amount of bookmarks from the bookmarks file
	while (fgets(line, sizeof(line), bm_fp))
		if (line[0] != '#') bm_n++;
	fclose(bm_fp);
	return bm_n;
}

char **get_bookmarks(char *bookmarks_file, int bm_num)
{
	char **bookmarks=NULL;
	bookmarks=xcalloc(bookmarks, bm_num, sizeof(char **));
	FILE *bm_fp;
	char line[MAX_LINE]="";
	bm_fp=fopen(bookmarks_file, "r");
	int ret_nl, bm_n=0;
	//get bookmarks for the bookmarks file
	while (fgets(line, sizeof(line), bm_fp)) {
		if (line[0] != '#') {
			bookmarks[bm_n]=xcalloc(bookmarks[bm_n], strlen(line), sizeof(char *));
			//store the entire line
			strcpy(bookmarks[bm_n], line);
			ret_nl=strcntchr(bookmarks[bm_n], '\n');
			if (ret_nl != -1)
				bookmarks[bm_n][ret_nl]='\0';
			bm_n++;
		}
	}
	fclose(bm_fp);
	return bookmarks;
}

char **bm_prompt(void)
{
	char *rl_bm_prompt=NULL;
	rl_bm_prompt=xcalloc(rl_bm_prompt, strlen(NC_b)+strlen(default_color)+38, sizeof(char *));
	sprintf(rl_bm_prompt, "%s%s\nChoose a bookmark ([e]dit, [q]uit): ", NC_b, default_color);
	char *bm_sel=rl_no_hist(rl_bm_prompt);
	free(rl_bm_prompt);
	char **comm_bm=parse_input_str(bm_sel);
	free(bm_sel);
	return comm_bm;	
}

void bookmarks_function()
{
	if (clear_screen)
		CLEAR;
	printf("%s%s Bookmarks Manager%s\n\n", white, PROGRAM_NAME, NC);
	struct stat file_attrib;
	//if the bookmarks file doesn't exist, create it
	if (stat(BM_FILE, &file_attrib) == -1) {
		FILE *fp;
		fp=fopen(BM_FILE, "w+");
		if (fp == NULL) perror("bookmarks");
		else fprintf(fp, "#Example: [t]test:/path/to/test\n");
		fclose(fp);
	}
	int bm_n=get_bm_n(BM_FILE); //get the amount of bookmarks
	//if no bookmarks 
	if (bm_n == 0) {
		char rl_question[128];
		sprintf(rl_question, "%s%sThere are no bookmarks.\nDo you want to edit the bookmarks file? (y/n) ", NC_b, default_color);
		char *answer=rl_no_hist(rl_question);
		if (strcmp(answer, "n") == 0 || strcmp(answer, "N") == 0) {
			free(answer);
			free_dirlist();
			list_dir();
			return;
		}
		else if (strcmp(answer, "y") == 0 || strcmp(answer, "Y") == 0) {
			free(answer);
			char edit_comm[150];
			snprintf(edit_comm, 150, "xdg-open '%s'", BM_FILE);
			system(edit_comm); //replace by execve
			bookmarks_function();
			return;
		}
		else {
			fputs("bm: Wrong answer", stderr);
			free(answer);
			free_dirlist();
			list_dir();
			return;
		}	
	}
	//if there are bookmarks...
	char **bookmarks=get_bookmarks(BM_FILE, bm_n); //get bookmarks
	char **bm_paths=NULL;
	bm_paths=xcalloc(bm_paths, bm_n, sizeof(char **));
	char **hot_keys=NULL;
	hot_keys=xcalloc(hot_keys, bm_n, sizeof(char **));
	char **bm_names=NULL;
	bm_names=xcalloc(bm_names, bm_n, sizeof(char **));
	int i;
	//get paths
	for(i=0;i<bm_n;i++) {
		char *str;
		str=straft(bookmarks[i], ':');
		if (str != NULL) {
			bm_paths[i]=xcalloc(bm_paths[i], strlen(str), sizeof(char *));
			strcpy(bm_paths[i], str);
		}
		else {
			str=straft(bookmarks[i], ']');
			if (str != NULL) {
				bm_paths[i]=xcalloc(bm_paths[i], strlen(str), sizeof(char *));
				strcpy(bm_paths[i], str);
			}
			else {
				bm_paths[i]=xcalloc(bm_paths[i], strlen(bookmarks[i]), sizeof(char *));
				strcpy(bm_paths[i], bookmarks[i]);
			}
		}
		free(str);	
	}
	//get hot_keys
	for (i=0;i<bm_n;i++) {
		char *str_b=strbtw(bookmarks[i], '[', ']');
		if (str_b != NULL) {
			hot_keys[i]=xcalloc(hot_keys[i], strlen(str_b), sizeof(char *));
			strcpy(hot_keys[i], str_b);
		}
		else hot_keys[i]=NULL;
		free(str_b);
	}
	//get bm_names
	for (i=0;i<bm_n;i++) {
		char *str_name=strbtw(bookmarks[i], ']', ':');
		if (str_name != NULL) {
			bm_names[i]=xcalloc(bm_names[i], strlen(str_name), sizeof(char *));
			strcpy(bm_names[i], str_name);
		}
		else {
			str_name=strbfr(bookmarks[i], ':');
			if (str_name != NULL) {
				bm_names[i]=xcalloc(bm_names[i], strlen(str_name), sizeof(char *));
				strcpy(bm_names[i], str_name);
			}	
			else bm_names[i]=NULL;
		}
		free(str_name);
	}
	//display results
	for (i=0;i<bm_n;i++) {
		int path_ok=stat(bm_paths[i], &file_attrib);
		if (hot_keys[i] != NULL) {
			if (bm_names[i] != NULL) {
				if (path_ok == 0) {
					if (file_attrib.st_mode & S_IFDIR)
						printf("%s%d %s[%s]%s %s%s%s\n", yellow, i+1, white, hot_keys[i], NC, cyan, bm_names[i], NC);
					else
						printf("%s%d %s[%s]%s %s%s%s\n", yellow, i+1, white, hot_keys[i], NC, default_color, bm_names[i], NC);					
				}
				else
					printf("%s%d [%s] %s%s\n", gray, i+1, hot_keys[i], bm_names[i], NC);
			}
			else {
				if (path_ok == 0) {
					if (file_attrib.st_mode & S_IFDIR)
						printf("%s%d %s[%s]%s %s%s%s\n", yellow, i+1, white, hot_keys[i], NC, cyan, bm_paths[i], NC);
					else
						printf("%s%d %s[%s]%s %s%s%s\n", yellow, i+1, white, hot_keys[i], NC, default_color, bm_paths[i], NC);		
				}
				else
					printf("%s%d [%s] %s%s\n", gray, i+1, hot_keys[i], bm_paths[i], NC);
			}
		}
		else {
			if (bm_names[i] != NULL) {
				if (path_ok == 0) {
					if (file_attrib.st_mode & S_IFDIR)
						printf("%s%d %s%s%s\n", yellow, i+1, cyan, bm_names[i], NC);
					else
						printf("%s%d %s%s%s%s\n", yellow, i+1, NC, default_color, bm_names[i], NC);				
				}
				else
					printf("%s%d %s%s\n", gray, i+1, bm_names[i], NC);
			}
			else { 
				if (path_ok == 0) {
					if (file_attrib.st_mode & S_IFDIR)
						printf("%s%d %s%s%s\n", yellow, i+1, cyan, bm_paths[i], NC);
					else
						printf("%s%d %s%s%s%s\n", yellow, i+1, NC, default_color, bm_paths[i], NC);	
				}
				else
					printf("%s%d %s%s\n", gray, i+1, bm_paths[i], NC);
			}
		}
	}
	//display prompt
	int args_n_old=args_n, reload_bm=0, go_dirlist=0; //, go_back_prompt=0;
	args_n=0;
	char **comm_bm=bm_prompt();
	//user selection
	if (!is_number(comm_bm[0])) { //if string...
		int valid_hk=0, eln=0;
		//if no application has been specified and xdg-open doesn't exist...
		if (strcmp(comm_bm[0], "e") == 0) {
			if (args_n == 0 && !(flags & XDG_OPEN_OK))
				fprintf(stderr, "%s: xdg-utils not installed. Try 'e application'\n", PROGRAM_NAME);
			else {
				if (args_n > 0) {
					char *bm_comm_path=NULL;
					if ((bm_comm_path=get_cmd_path(comm_bm[1])) != NULL) {
						pid_t pid_edit_bm=fork();
						if (pid_edit_bm == 0) {
							set_signals_to_default();
							execle(bm_comm_path, comm_bm[1], BM_FILE, NULL, __environ);
							set_signals_to_ignore();
						}
						else waitpid(pid_edit_bm, NULL, 0);
						reload_bm=1;
						free(bm_comm_path);
					}
					else fprintf(stderr, "%s: %s: Application not found\n", PROGRAM_NAME, 
																			 comm_bm[1]);
				}
				else {
					pid_t pid_edit_bm=fork();
					if (pid_edit_bm == 0) {
						set_signals_to_default();
						execle(xdg_open_path, "xdg-open", BM_FILE, NULL, __environ);
						set_signals_to_ignore();
					}
					else waitpid(pid_edit_bm, NULL, 0);
					reload_bm=1;
				}
			}
		}
		else if (strcmp(comm_bm[0], "q") == 0) {
			go_dirlist=1;
		}
		else { //if neither 'e' nor 'q'...
			for (i=0;i<bm_n;i++) {
				if (hot_keys[i] != NULL) {
					if (strcmp(hot_keys[i], comm_bm[0]) == 0) {
						valid_hk=1;
						eln=i;
						break;
					}
				}
			}
			if (!valid_hk) {
				if (is_number(comm_bm[0]))
					fprintf(stderr, "bm: %d: No such bookmark\n", bm_n);
				else fprintf(stderr, "bm: %s: No such bookmark\n", comm_bm[0]);
//				go_back_prompt=1;
				}
			else { //if a valid hotkey...
				if (stat(bm_paths[eln], &file_attrib) == 0) {
					if (file_attrib.st_mode & S_IFDIR) {
						update_path(bm_paths[eln]);
						go_dirlist=1;
					}
					else { //if not a directory...
						if (args_n == 0) { //if no opening application has been passed...
							if (flags & XDG_OPEN_OK) {
								pid_t pid_bm=fork();
								if (pid_bm == 0) {
									set_signals_to_default();
									execle(xdg_open_path, "xdg-open", bm_paths[eln], NULL, __environ);
									set_signals_to_ignore();
								}
								else waitpid(pid_bm, NULL, 0);
							}
							else fprintf(stderr, "%s: xdg-utils not installed. Try 'hotkey application'\n", PROGRAM_NAME);
						}
						else { //if application has been passed (as 2nd arg)...
							char *cmd_path=NULL;
							if ((cmd_path=get_cmd_path(comm_bm[1])) != NULL) { //check if application exists
								pid_t pid_bm=fork();
								if (pid_bm == 0) {
									set_signals_to_default();
									execle(cmd_path, comm_bm[1], bm_paths[eln], NULL, __environ);
									set_signals_to_ignore();
								}
								else
									waitpid(pid_bm, NULL, 0);
							}
							else {
								fprintf(stderr, "bm: %s: Application not found\n", comm_bm[1]);
							}
							free(cmd_path);
						}
					}
				}
				else {
					fputs("bm: No such bookmark", stderr);
				}
			}
		}
	}
	else { //if digit
		if (atoi(comm_bm[0]) != 0 && !(atoi(comm_bm[0]) > bm_n)) {
			stat(bm_paths[atoi(comm_bm[0])-1], &file_attrib);
			if (file_attrib.st_mode & S_IFDIR) { //if a directory...
				update_path(bm_paths[atoi(comm_bm[0])-1]);
				go_dirlist=1;
			}
			else { //if a file...
				if (args_n == 0) { //if no opening application has been passed...
					if (!(flags & XDG_OPEN_OK))
						fprintf(stderr, "%s: xdg-utils not installed. Try 'ELN/hot-key application'\n", PROGRAM_NAME);
					else {
						pid_t pid_bm=fork();
						if (pid_bm == 0) {
							set_signals_to_default();
							execle(xdg_open_path, "xdg-open", bm_paths[atoi(comm_bm[0])-1], NULL, __environ);
							set_signals_to_ignore();
						}
						else waitpid(pid_bm, NULL, 0);
					}
				}
				else { //if application has been passed (as 2nd arg)...
					//check if application exists
					char *cmd_path=NULL;
					if ((cmd_path=get_cmd_path(comm_bm[1])) != NULL) {
						pid_t pid_bm=fork();
						if (pid_bm == 0) {
							set_signals_to_default();
							execle(cmd_path, comm_bm[1], bm_paths[atoi(comm_bm[0])-1], NULL, __environ);
							set_signals_to_ignore();
						}
						else waitpid(pid_bm, NULL, 0);
					}
					else {
						fprintf(stderr, "bm: %s: Application not found\n", comm_bm[1]);
//						puts("Press Enter key to continue... ");
//						getchar();
//						bookmarks_function();
					}
					free(cmd_path);
				}
			}
		}
		else {
			fprintf(stderr, "bm: %s: No such bookmark\n", comm_bm[0]);
		}
	}
/*	if (go_back_prompt) {
		for (int i=0;i<=args_n;i++)
			free(comm_bm[i]);
		free(comm_bm);
		bm_prompt();
	}
	else {*/
		//FREE EVERYTHING!
		for (int i=0;i<=args_n;i++)
			free(comm_bm[i]);
		free(comm_bm);
		for (i=0;i<bm_n;i++) {
			free(bookmarks[i]);
			free(bm_paths[i]);
			free(bm_names[i]);
			free(hot_keys[i]);
		}
		free(bookmarks);
		free(bm_paths);
		free(bm_names);
		free(hot_keys);
		args_n=args_n_old;
		if (reload_bm) bookmarks_function();
		if (go_dirlist) {
			free_dirlist();
			list_dir();
		}
//	}	
}

void dir_size(char *dir)
{
	#ifndef DU_TMP_FILE
		#define DU_TMP_FILE "/tmp/.du_size"
	#endif
	/* Check for 'du' existence just once (the first time the function is called) by storing
	the result in a static variable (whose value will survive the function).*/
	static char du_path[PATH_MAX]="";
	if (du_path[0] == '\0') {
		char *du_path_tmp=NULL;
		if ((du_path_tmp=get_cmd_path("du")) != NULL) {
			strncpy(du_path, du_path_tmp, strlen(du_path_tmp));
			free(du_path_tmp);
		}
		else strcpy(du_path, "du not found");
	}
	FILE *du_fp=fopen(DU_TMP_FILE, "w");
	FILE *du_fp_err=fopen("/dev/null", "w");
	int stdout_bk=dup(STDOUT_FILENO); //save original stdout
	int stderr_bk=dup(STDERR_FILENO); //save original stderr
	dup2(fileno(du_fp), STDOUT_FILENO); //redirect stdout to the desired file	
	dup2(fileno(du_fp_err), STDERR_FILENO); //redirect stderr to /dev/null
	fclose(du_fp);
	fclose(du_fp_err);
	//the same can be done with STDIN_FILENO
	pid_t pid_du=fork();
	if (pid_du == 0)
		//write command output to new stdout (file) and error to /dev/null
		execle(du_path, "du", "-s", "--si", dir, NULL, __environ);
	else waitpid(pid_du, NULL, 0);
	dup2(stdout_bk, STDOUT_FILENO); //restore original stdout
	dup2(stderr_bk, STDERR_FILENO); //restore original stderr
	close(stdout_bk);
	close(stderr_bk);
	if (access(DU_TMP_FILE, F_OK) == 0) {
		FILE *du_fp=fopen(DU_TMP_FILE, "r");
		if (du_fp != NULL) {
			char line[MAX_LINE]="";
			fgets(line, sizeof(line), du_fp);
			char *file_size=NULL;
			if ((file_size=strbfr(line, '\t')) != NULL)
				printf("%s\n", file_size);
			else printf("strbfr: error\n");
			free(file_size);
			fclose(du_fp);
		}
		else puts("unknown");
		remove(DU_TMP_FILE);
	}
	else puts("unknown");
}

void properties_function(char **comm)
{
	if (args_n == 0) {
		puts("Usage: pr [all] [size] ELN/filename");
		return;
	}
	if (args_n == 1 && strcmp(comm[1], "all") == 0) {
		for (int i=0;i<files;i++) {
			get_properties(dirlist[i]->d_name);
			printf("\n");
		}
		return;
	}
	if (args_n == 1 && strcmp(comm[1], "size") == 0) { //list files by size
		struct stat file_attrib;
		for (int i=0;i<files;i++) {
			lstat(dirlist[i]->d_name, &file_attrib);
			if (file_attrib.st_mode & S_IFDIR) {
				char *dir=get_file_size(file_attrib.st_size);
				printf("%s%-2d%s %-10.15s%s%s%10s\n", yellow, i+1, blue, dirlist[i]->d_name, 
																	NC, default_color, dir);
				free(dir);
//				dir_size(dirlist[i]->d_name);
			}
			else {
				char *file=get_file_size(file_attrib.st_size);
				printf("%s%-2d%s%s %-10.15s%10s\n", yellow, i+1, NC, default_color, dirlist[i]->d_name, 
																						file);
				free(file);
			}
		}
		return;
	}
	if (is_number(comm[1])) {
		if (atoi(comm[1]) == 0 || atoi(comm[1]) > files) {
			fputs("pr: No such ELN\n", stderr);
			return;
		}
		else { //if valid ELN...
			get_properties(dirlist[atoi(comm[1])-1]->d_name);
			return;
		}
	}
	else { //if string...
		if (access(comm[1], F_OK) == 0) get_properties(comm[1]);
		else {
			fprintf(stderr, "pr: %s: %s\n", comm[1], strerror(errno));
			return;
		}
	}
}

void get_properties(char *filename)
//this is my humble version of 'ls -hl' command plus most of the info from 'stat' command.
{
	struct stat file_attrib;
	//check file existence (though this isn't necessary in this program)
	if (lstat(filename, &file_attrib) != 0) {
		perror("pr");
		return;
	}
	//get file size
	char *size_type=get_file_size(file_attrib.st_size);

	//get file type (and color):
	int sticky=0;
	char file_type=0, color[15]="";
	char *linkname=NULL;
	switch(file_attrib.st_mode & S_IFMT) {
		case S_IFREG:
			file_type='-';
			if (!(file_attrib.st_mode & S_IRUSR)) strcpy(color, d_red);
			else if (file_attrib.st_mode & S_ISUID)
				strcpy(color, bg_red_fg_white);
			else if (file_attrib.st_mode & S_ISGID)
				strcpy(color, bg_yellow_fg_black);
			else {
				cap_t cap=cap_get_file(filename);
				if (cap) {
					strcpy(color, bg_red_fg_black);
					cap_free(cap);
				}
				else if (file_attrib.st_mode & S_IXUSR) {
					if (file_attrib.st_size == 0) strcpy(color, d_green);
					else strcpy(color, green);
				}
				else if (file_attrib.st_size == 0) strcpy(color, d_yellow);
				else strcpy(color, default_color);
			}
		break;
		case S_IFDIR:
			file_type='d';
			if (access(filename, R_OK|X_OK) != 0)
				strcpy(color, red);
			else {
				int is_oth_w=0;
				if (file_attrib.st_mode & S_ISVTX) sticky=1;
				if (file_attrib.st_mode & S_IWOTH) is_oth_w=1;
				int files_dir=count_dir(filename);
				strcpy(color, (sticky) ? ((is_oth_w) ? bg_green_fg_blue : bg_blue_fg_white) 
				  : ((is_oth_w) ? bg_green_fg_black : ((files_dir == 2 || files_dir == 0) ? 
																		  d_blue : blue)));
			}
		break;
		case S_IFLNK:
			file_type='l';
			linkname=realpath(filename, NULL);
			if (linkname) {
				strcpy(color, cyan);
			}
			else strcpy(color, d_cyan);
		break;
		case S_IFSOCK: file_type='s'; strcpy(color, magenta); break;
		case S_IFBLK: file_type='b'; strcpy(color, yellow); break;
		case S_IFCHR: file_type='c'; strcpy(color, white); break;
		case S_IFIFO: file_type='p'; strcpy(color, d_magenta); break;
		default: file_type='?'; strcpy(color, default_color);
	}
	
	//get file permissions
	//OWNER:
	char read_usr='-', write_usr='-', exec_usr='-';
	short read_usr_n=0, write_usr_n=0, exec_usr_n=0;
	char read_grp='-', write_grp='-', exec_grp='-';
	short read_grp_n=0, write_grp_n=0, exec_grp_n=0;
	char read_others='-', write_others='-', exec_others='-';
	short read_others_n=0, write_others_n=0, exec_others_n=0;

	mode_t val=(file_attrib.st_mode & ~S_IFMT);
	if (val & S_IRUSR) { read_usr='r'; read_usr_n=4; }
	if (val & S_IWUSR) { write_usr='w'; write_usr_n=2; } 
	if (val & S_IXUSR) { exec_usr='x'; exec_usr_n=1; }

	if (val & S_IRGRP) { read_grp='r'; read_grp_n=4; }
	if (val & S_IWGRP) { write_grp='w'; write_grp_n=2; }
	if (val & S_IXGRP) { exec_grp='x'; exec_grp_n=1; }

	if (val & S_IROTH) { read_others='r'; read_others_n=4; }
	if (val & S_IWOTH) { write_others='w'; write_others_n=2; }
	if (val & S_IXOTH) { exec_others='x'; exec_others_n=1; }

	if (file_attrib.st_mode & S_ISUID) (exec_usr_n == 1) ? (exec_usr='s') : (exec_usr='S');
	if (file_attrib.st_mode & S_ISGID) (exec_grp_n == 1) ? (exec_grp='s') : (exec_grp='S');

	//get number of links to the file
	nlink_t link_n=file_attrib.st_nlink;

	//get modification time
	time_t mod_time_t=file_attrib.st_mtime;
	struct tm *tm=localtime(&mod_time_t);
	char *mod_time=NULL;
	mod_time=xcalloc(mod_time, 64, sizeof(char *));
	strftime(mod_time, sizeof(mod_time), "%c", tm);
	char *months[]={"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", 
						"Sep", "Oct", "Nov", "Dec"};
	sprintf(mod_time, "%s %d %d:%d:%d %d", months[tm->tm_mon], tm->tm_mday, tm->tm_hour, 
											  tm->tm_min, tm->tm_sec, tm->tm_year+1900);

	//get owner and group names
	uid_t owner_id=file_attrib.st_uid; //owner ID
	gid_t group_id=file_attrib.st_gid; //group ID	
	struct group *group;
	struct passwd *owner;
	group=getgrgid(group_id);
	owner=getpwuid(owner_id); //for some reason this function changes the value of the user 
	//variable(????) Very dirty workaround: run get_user() at the end of the function.
	
	//Print the results
	printf("(%d%d%d)%c/%c%c%c/%c%c%c/%c%c%c %ld %s %s %s %s ", 
	read_usr_n+write_usr_n+exec_usr_n, read_grp_n+write_grp_n+exec_grp_n,
	read_others_n+write_others_n+exec_others_n, file_type, 
												  read_usr, write_usr, exec_usr, 
												  read_grp, write_grp, exec_grp,
												  read_others, write_others, (sticky) ? 't' 
												  : exec_others, link_n, 
												  (!owner) ? "unknown" : owner->pw_name, 
												  (!group) ? "unknown" : group->gr_name, 
												  (size_type) ? size_type : "??", 
												  (mod_time) ? mod_time : "??");
	if (file_type != 'l') printf("%s%s\033[0m\n", color, filename);
	else {
		printf("%s%s\033[0m -> %s\n", color, filename, (linkname) ? linkname : "??");
		if (linkname) free(linkname);
	}
	//stat information
	if (file_type == 'd') write(STDOUT_FILENO, "Directory", 9);
	else if (file_type == 's') write(STDOUT_FILENO, "Socket", 6);
	else if (file_type == 'l') write(STDOUT_FILENO, "Symbolic link", 13);
	else if (file_type == 'b') write(STDOUT_FILENO, "Block special file", 18);
	else if (file_type == 'c') write(STDOUT_FILENO, "Character special file", 22);
	else if (file_type == 'p') write(STDOUT_FILENO, "Fifo", 4);
	else if (file_type == '-') {
		if (file_attrib.st_size == 0) write(STDOUT_FILENO, "Empty regular file", 18);
		else write(STDOUT_FILENO, "Regular file", 12);
	}
	printf("\tBlocks: %ld", file_attrib.st_blocks);
	printf("\tIO Block: %ld", file_attrib.st_blksize);
	printf("\tInode: %ld\n", file_attrib.st_ino);
	//Octal and decimal value for Device
	printf("Device: %lxh/%ldd", file_attrib.st_dev, file_attrib.st_dev);
	printf("\tUid: %d (%s)", file_attrib.st_uid, (!owner) ? "unknown" : owner->pw_name);
	printf("\tGid: %d (%s)\n", file_attrib.st_gid, (!group) ? "unknown": group->gr_name);	
	if (file_attrib.st_mode & S_IFDIR) { printf("Total size: "); dir_size(filename); }
	else printf("Size: %s\n", (size_type) ? size_type : "??");
	if (size_type) free(size_type);
	free(mod_time);
	get_user();
}

void chown_chmod (char **comm)
//Note: there are native C chown() and chmod() functions
{
	if (args_n == 0) {
		printf("Usage: %s [args] [ELN, filename]\n", comm[0]);
		return;
	}
	int i=0;
	char *ext_comm=NULL, *target_file=NULL;
	if (is_number(comm[args_n])) {
		if (atoi(comm[args_n]) == 0 || atoi(comm[args_n]) > files) {
			printf("%s: No such ELN", comm[0]);
			return;
		}
		else {
			target_file=xcalloc(target_file, strlen(path)+strlen(dirlist[atoi(comm[args_n])-1]->d_name)+1, 
																	   sizeof(char *));
			sprintf(target_file, "%s/%s", path, dirlist[atoi(comm[args_n])-1]->d_name);
		}
	}
	else { //if string
		if (strcntchr(comm[args_n], '/') != -1) { //if a path
			struct stat file_attrib;
			if (lstat(comm[args_n], &file_attrib) == 0) {
				target_file=xcalloc(target_file, strlen(comm[args_n]), sizeof(char *));
				strcpy(target_file, comm[args_n]);
			}
			else {
				printf("%s: Invalid file", comm[0]);
				return;				
			}				
		}
		else { //if not a path
			int elem_list=0;
			for (i=0;i<files;i++) {
				if (strcmp(dirlist[i]->d_name, comm[args_n]) == 0) {
					target_file=xcalloc(target_file, strlen(path)+strlen(comm[args_n])+1, 
													  sizeof(char *));				
					sprintf(target_file, "%s/%s", path, comm[args_n]);
					elem_list=1;
					break;
				}
			}
			if (!elem_list) {
				fprintf(stderr, "%s: Invalid file", comm[0]);
				return;
			}
		}
	}
	ext_comm=xcalloc(ext_comm, PATH_MAX, sizeof(char *));
	if (strcmp(comm[0], "chmod") == 0)
		strcpy(ext_comm, "chmod");
	else
		strcpy(ext_comm, "chown");
	for (i=1;i<args_n;i++) {
		strncat(ext_comm, " ", PATH_MAX);
		strncat(ext_comm, comm[i], PATH_MAX);
	}
	strncat(ext_comm, " ", PATH_MAX);
	strncat(ext_comm, target_file, PATH_MAX);
	system(ext_comm);
	log_function(comm);
	free(target_file);
	free(ext_comm);
}

void hidden_function(char **comm)
{
	if (args_n == 0) { puts("Usage: hidden [on, off]"); return; }	
	if (strcmp(comm[1], "off") == 0) {
		if (show_hidden == 1) {
			show_hidden=0;
			free_dirlist();
			list_dir();
		}
	}
	else if (strcmp(comm[1], "on") == 0) {
		if (show_hidden == 0) {
			show_hidden=1;
			free_dirlist();
			list_dir();
		}
	}
	else fputs("Invalid argument", stderr);		
}

void log_function(char **comm)
{
	int clean_log=0;
	if (strcmp(comm[0], "log") == 0 && args_n == 0) {
		FILE *log_fp;
		log_fp=fopen(LOG_FILE, "r");
		if (log_fp == NULL) {
			perror("log");
			return;
		}
		else {
			char line[MAX_LINE]="";	
			while (fgets(line, sizeof(line), log_fp))
				printf("%s", line);
			fclose(log_fp);
			return;
		}
	}
	else if (strcmp(comm[0], "log") == 0 && args_n == 1) {
		if (strcmp(comm[1], "clean") == 0) {
			clean_log=1;
		}
	}
	char full_comm[PATH_MAX]="";
	strncpy(full_comm, comm[0], PATH_MAX);
	for (int i=1;i<=args_n;i++) {
		strncat(full_comm, " ", PATH_MAX);
//		if (is_number(comm[i]))
//			strncat(full_comm, dirlist[atoi(comm[i])-1]->d_name, PATH_MAX);
//		else
			strncat(full_comm, comm[i], PATH_MAX);
	}
	char *date=get_date();
	char full_log[PATH_MAX]="";
	snprintf(full_log, PATH_MAX, "[%s] %s:%s:%s\n", date, user, path, full_comm);
	free(date);
	FILE *log_fp;
	if (!clean_log) log_fp=fopen(LOG_FILE, "a");
	//else, overwrite the log file leaving only the 'log clean' command.
	else log_fp=fopen(LOG_FILE, "w+");
	fprintf(log_fp, full_log);
	fclose(log_fp);
}

void check_log_file_size(void) 
{
	FILE *log_fp=fopen(LOG_FILE, "r");
	if (log_fp != NULL) {
		char line[MAX_LINE]="";
		int logs_num=0;
		while (fgets(line, sizeof(line), log_fp))
			logs_num++;
		fclose(log_fp);
		if (logs_num > max_log) {
			FILE *log_fp=fopen(LOG_FILE, "r");
			if (log_fp == NULL) {
				perror("history");
				return;
			}
			FILE *log_fp_tmp=fopen(LOG_FILE_TMP, "w+");
			if (log_fp_tmp == NULL) {
				perror("history");
				fclose(log_fp);
				return;
			}
			int i=1;
			while (fgets(line, sizeof(line), log_fp)) {
				if (i++ >= logs_num-(max_log-1)) { //delete old entries
					line[strlen(line)-1]='\0';
					fprintf(log_fp_tmp, line);
					fprintf(log_fp_tmp, "\n");
				}
			}
			fclose(log_fp_tmp);
			fclose(log_fp);
			remove(LOG_FILE);
			rename(LOG_FILE_TMP, LOG_FILE);
		}
	}
	else perror("history");
}

void backup_function(char **comm, char *file)
{
	if (args_n > 0) {
		if (strcmp(comm[1], "on") == 0) {
			backup=1;
			puts("bk: Backups enabled");
			return;
		}
		else if (strcmp(comm[1], "off") == 0) {
			backup=0;
			puts("bk: Backups disabled");
			return;
		}
	}
	if (!backup) { //if backup is disabled...
		if (strcmp(comm[0], "bk") == 0 || strcmp(comm[0], "backup") == 0)
			puts("bk: Backup function disabled");
		return;
	}
	struct stat file_attrib;
	if (stat(BK_DIR, &file_attrib) != 0) //if backup folder doesn't exist, create it.
		mkdir(BK_DIR, S_IRWXU);
	if (file != NULL) { //if a path has been passed to the function...
		//get date for the backup file extension
		time_t rawtime=time(NULL);
		struct tm *tm=localtime(&rawtime);
		char *date=NULL;
		date=xcalloc(date, 64, sizeof(char *));
		strftime(date, sizeof(date), "%c", tm);
		char *months[]={"jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", 
						"sep", "oct", "nov", "dec"};
		snprintf(date, 64, "%s_%d_%d:%d:%d\n", months[tm->tm_mon], tm->tm_mday, tm->tm_hour, 
															   tm->tm_min, tm->tm_sec);
		date[strlen(date)-1]='\0';
		//get the filename from the path to be backed up
		char *bk_filename=straftlst(file, '/');
		char *ext_comm=NULL;
		ext_comm=xcalloc(ext_comm, strlen(file)+strlen(date)+strlen(bk_filename)+strlen(BK_DIR)+25, 
																		    sizeof(char *));
		lstat(file, &file_attrib);
		//if directory or symlink...
		if (file_attrib.st_mode & S_IFDIR || S_ISLNK(file_attrib.st_mode))
			sprintf(ext_comm, "cp -r %s %s/%s.%s & 2>/dev/null", file, BK_DIR, bk_filename, 
																				   date);
		else
			sprintf(ext_comm, "cp %s %s/%s.%s & 2>/dev/null", file, BK_DIR, bk_filename, date);
		system(ext_comm);
		free(bk_filename);
		free(ext_comm);
		free(date);
	}
	else if (args_n == 0) { //if no path has been passed to ethe function...
		update_path(BK_DIR);
		free_dirlist();
		list_dir();
	}
	else puts("Usage: bk");
}

void get_history(void)
{
	FILE *hist_fp=fopen(HIST_FILE, "r");
	if (current_hist_n == 0)
		history=xcalloc(history, 1, sizeof(char **));
	if (hist_fp != NULL) {
		char line[MAX_LINE]="";
		while (fgets(line, sizeof(line), hist_fp)) {
			line[strlen(line)-1]='\0';
			history=xrealloc(history, (current_hist_n+1)*sizeof(char **));
			history[current_hist_n]=xcalloc(history[current_hist_n], strlen(line), sizeof(char *));
			strncpy(history[current_hist_n++], line, strlen(line));
		}
		fclose(hist_fp);
	}
	else perror("history");
}

void history_function(char **comm)
{
	if (args_n == 0) {
		for (int i=0;i<current_hist_n;i++)
			printf("%d %s\n", i+1, history[i]);
	}
	else if (args_n == 1 && strcmp(comm[1], "clean") == 0) {
		FILE *hist_fp=fopen(HIST_FILE, "w+");
		fprintf(hist_fp, "history clean\n");
		fclose(hist_fp);
		get_history();
		log_function(comm);
	}
	else if (args_n == 1 && is_number(comm[1]) != -1) {		
		for (int i=current_hist_n-atoi(comm[1])-1;i<current_hist_n;i++)
			printf("%d %s\n", i+1, history[i]);
	}
	else puts("Usage: history [clean]");
}

void run_history_cmd(char *comm)
{
	int usage=0;
	if (comm[1] != '\0') {
		char *history_num=straft(comm, '!');
		if (history_num != NULL) {
			if (is_number(history_num) == 1) {
				if (atoi(history_num) > 0 && atoi(history_num) < current_hist_n) {
					char **cmd_hist=parse_input_str(history[atoi(history_num)-1]);
					exec_cmd(cmd_hist);
					for (int i=0;i<=args_n;i++)
						free(cmd_hist[i]);
					free(cmd_hist);
				}
				else
					fprintf(stderr, "%s: !%d: event not found\n", PROGRAM_NAME, atoi(history_num));
			}
			else if (strcmp(comm, "!!") == 0) { //if "!!" execute the last command
				char **cmd_hist=parse_input_str(history[current_hist_n-1]);
				exec_cmd(cmd_hist);
				for (int i=0;i<=args_n;i++)
					free(cmd_hist[i]);
				free(cmd_hist);
			}
			else if (comm[1] == '-') {
				char *hist_num=straft(comm, '-');
				if (hist_num != NULL) {
					if (is_number(hist_num) == -1 || //if not a number...
					(atoi(hist_num) == 0 || atoi(hist_num) > current_hist_n-1)) {
						//or if zero or bigger than max...
						fprintf(stderr, "%s: !%d: event not found\n", PROGRAM_NAME, atoi(hist_num));
						free(hist_num);
						free(history_num);
						return;
					}
					char **cmd_hist=parse_input_str(history[current_hist_n-atoi(hist_num)]);
					exec_cmd(cmd_hist);
					free(hist_num);
					for (int i=0;i<=args_n;i++)
						free(cmd_hist[i]);
					free(cmd_hist);
				}
				else
					usage=1;
			}
			else {
				usage=1;
			}
			free(history_num);
		}
		else
			fprintf(stderr, "history: Error\n");
	}
	else
		usage=1;
	if (usage)
		printf("Usage:\n\
!!: Expand to the last command.\n\
!n: Expand to command with history number \"n\".\n\
!-n: Expand to command that was \"n\" number of commands before the last command in history.\n");
}

void edit_function(char **comm)
{
	//get modification time of the config file before opening it
	struct stat file_attrib;
	/*If, for some reason (like someone erasing the file while the program is running) clifmrc
	doesn't exist, call init_config to recreate the configuration file. Then run 'stat' again
	to reread the attributes of the file*/ 
	if (stat(CONFIG_FILE, &file_attrib) == -1) { 
		init_config();
		stat(CONFIG_FILE, &file_attrib);
	}
	time_t mtime_bfr=file_attrib.st_mtime;
	char *cmd_path=NULL;
	if (args_n > 0) { //if an argument is found...
		if ((cmd_path=get_cmd_path(comm[1])) == NULL) { //and not a valid program...
			fprintf(stderr, "%s: %s: Command not found\n", PROGRAM_NAME, comm[1]);
			return;
		}
	}
	if (args_n == 0) { //if no application has been passed as 2nd argument...
		//and if xdg-open is not installed...
		if (!(flags & XDG_OPEN_OK)) {
			fprintf(stderr, "%s: xdg-utils not installed. Try 'edit application_name'\n", PROGRAM_NAME);
			return;
		}
	}
	//either a valid program has been passed or xdg-open exists
	pid_t pid_edit=fork();
	if (pid_edit == 0) {
		set_signals_to_default();
		//if application has been passed
		if (args_n > 0)	execle(cmd_path, comm[1], CONFIG_FILE, NULL, __environ);		
		//no application passed and xdg-open exists
		else execle(xdg_open_path, "xdg-open", CONFIG_FILE, NULL, __environ);
		set_signals_to_ignore();
	}
	else
		waitpid(pid_edit, NULL, 0);
	if (args_n > 0)	free(cmd_path);
	//get modification time after opening it
	stat(CONFIG_FILE, &file_attrib);
	//if modification times differ, the file was modified after being opened.
	if (mtime_bfr != file_attrib.st_mtime) {	
		//reload configuration only if the config file was modified.
		init_config();
		get_aliases_n_prompt_cmds();
		/*Rerun external_arguments. Otherwise, external arguments will be overwritten by 
		init_config().*/
		if (argc_bk > 1) external_arguments(argc_bk, argv_bk);
		free_dirlist();
		list_dir();
	}
}

void color_codes(void)
{
	printf("%s file name%s%s: Directory with no read permission\n", red, NC, default_color);
	printf("%s file name%s%s: File with no read permission\n", d_red, NC, default_color);
	printf("%s file name%s%s: Directory*\n", blue, NC, default_color);
	printf("%s file name%s%s: EMPTY directory\n", d_blue, NC, default_color);
	printf("%s file name%s%s: Executable file\n", green, NC, default_color);
	printf("%s file name%s%s: Empty executable file\n", d_green, NC, default_color);
	printf("%s file name%s%s: Block special file\n", yellow, NC, default_color);	
	printf("%s file name%s%s: Empty (zero-lenght) file\n", d_yellow, NC, default_color);
	printf("%s file name%s%s: Symbolic link\n", cyan, NC, default_color);	
	printf("%s file name%s%s: Broken symbolic link\n", d_cyan, NC, default_color);
	printf("%s file name%s%s: Socket file\n", magenta, NC, default_color);
	printf("%s file name%s%s: Pipe or FIFO special file\n", d_magenta, NC, default_color);
	printf("%s file name%s%s: Character special file\n", white, NC, default_color);
	printf("%s file name%s%s: Regular file (terminal default foreground color)\n", 
											   default_color, NC, default_color);
	printf(" %s%sfile name%s%s: SUID file\n", NC, bg_red_fg_white, NC, 
																	default_color);
	printf(" %s%sfile name%s%s: SGID file\n", NC, bg_yellow_fg_black, NC, 
																		  default_color);
	printf(" %s%sfile name%s%s: Sticky and NOT other-writable directory*\n", NC, bg_blue_fg_white, NC, 
																			  default_color);
	printf(" %s%sfile name%s%s: Sticky and other-writable directory*\n", NC, bg_green_fg_blue, NC, 
																			  default_color);
	printf(" %s%sfile name%s%s: Other-writable and NOT sticky directory*\n", NC, bg_green_fg_black, NC, 
																			  default_color);
	printf(" %s%sfile name%s%s: Unknown file type\n", NC, bg_white_fg_red, NC, 
																			  default_color);
	printf("\n*The slash followed by a number (/xx) after directory names indicates \
the amount of files contained by the corresponding directory.\n\n");
}

void list_commands(void)
{
	printf("%s  cmd, commands%s%s: show this list of commands.\n", yellow, NC, default_color);
	printf("%s  /%s%s*: This is the quick search function. Just type '/' followed by the string \
you are looking for (you can use asterisks as wildcards), and %s will list all the matches in the \
current folder.\n", yellow, NC, default_color, PROGRAM_NAME);
	printf("%s  bm, bookmarks%s%s: open the bookmarks menu. Here you can add, remove or edit your \
bookmarks to your liking, or simply cd into the desired bookmark by entering either its ELN \
or its hotkey.\n", yellow, NC, default_color);
	printf("%s  o, open%s%s ELN (or path or filename) [application name]: open either a folder, \
or a file. For example: 'o 12' or 'o filename'. By default, the 'open' function will open \
files with the default application associated to them. However, if you want to open a file \
with a different application, just add the application name as a second argument, e.g. \
'o 12 leafpad'.\n", yellow, NC, default_color);
	printf("%s  cd%s%s ELN (or path): change the current directory to that inidicated by the first \
argument. You can also use ELN's to indicate the folder to change to. Ex: cd 12, or cd ~/media. \
Unlike the built-in cd command, %s's cd function will not only change the current directory, \
but will also show its content.\n", yellow, NC, default_color, PROGRAM_NAME);
	printf("%s  b, back%s%s: Unlike 'cd ..', which will send you to the parent directory of the \
current directory, this comand will send you back to the previously visited directory.\n", yellow, NC, default_color);
	printf("%s  pr, prop, stat%s%s ELN (or path or filename): display the properties of the selected element.\n", yellow, NC, default_color);
	printf("%s  mkdir%s%s dirname(s): Create one or more directories called dirname(s). \
Ex: mkdir dir1 dir2 \"file test\"\n", yellow, NC, default_color);
	printf("%s  touch%s%s filename(s): Create one or more empty regular files named as filename(s). \
Ex: touch file1 file2 \"file test\"\n", yellow, NC, default_color);
	printf("%s  ln, link%s%s [sel or ELN] [link_name]: create a simbolic link. The source element could \
be either a selected element, in which case you has to simply type 'sel' as first argument, or \
an element listed in the screen, in which case you simply has to specify its ELN as first \
argument. The second argument is always a file name for the symlink. Ex: link sel \
symlink_name.\n", yellow, NC, default_color);
	printf("%s  s, sel%s%s ELN ELN-ELN filename path... n: send one or multiple elements to the \
Selection Box. 'Sel' accepts individual elements, range of elements (NOT YET!), say 1-6, \
filenames and paths, just as '*' as wildcard. Ex: sel 1 4-10 file* filename /path/to/filename\n", 
    yellow, NC, default_color);
	printf("%s  sb, selbox%s%s: show the elements contained in the Selection Box.\n", yellow, NC, default_color);
	printf("%s  ds, desel%s%s: deselect one or more selected elements. You can also deselect all \
selected elements by typing 'ds *'.\n", yellow, NC, default_color);
	printf("%s  rm%s%s [options] [sel] file(s): if 'sel' is passed as first argument, 'rm' will \
delete those elements currently selected, if any. Otherwise, when one or more filenames are \
passed to rm, it will delete only those elements. You can use filenames and paths as always to \
indicate the element(s) to be removed, but also ELN's.\n", yellow, NC, default_color);
	printf("%s  cp%s%s [options] [sel] source destiny: this command works just as the built-in \
'cp' command, with the difference that you can use ELN's to indicate both source and destiny \
files. So, cp 1 2 will copy the first listed element into the second. If 'sel' is passed as \
the only argument, 'cp' will copy all the selected elements into the current directory. However, \
if you specify a destiny path as second argument (i.e. cp sel ~/misc), the selected files will \
be copied into that path. You can also use an ELN to indicate where to copy those selected \
files. Ex: cp sel 3\n", yellow, NC, default_color);
	printf("%s  paste%s%s [sel] [destiny]: when no arguments are passed, 'paste' will copy the \
currently selected elements, if any, into the current directory. If you want to copy those \
elements into some other directory, you have to tell 'paste' where to copy those elements. Ex: \
paste sel /path/to/directory\n", yellow, NC, default_color);
	printf("%s  mv, move%s%s [options] [sel] source destiny: idem cp.\n", yellow, NC, default_color);
	printf("%s  chown%s%s: run the built-in 'chown' command, with the exception that you can use ELN's \
instead of filenames. Ex: chown -r user 12\n", yellow, NC, default_color);
	printf("%s  chmod%s%s: idem chown.\n", yellow, NC, default_color);
	printf("%s  bk, backup%s%s [on off]: Toggle on/off the backup function. With no arguments \
it shows the content of the backup directory. THIS FUNCTION IS STILL EXPERIMENTAL: USE IT AT \
YOUR OWN RISK!\n", yellow, NC, default_color);
	printf("%s  log%s%s [clean]: with no arguments, it shows the log file. If clean is passed as \
argument, it will delete all the logs.\n", yellow, NC, default_color);
	printf("%s  history%s%s [clean]: with no arguments, it shows the history list. If clean is passed \
as argument, it will delete all the entries in the history file.\n", yellow, NC, default_color);
	printf("%s  edit%s%s: edit the configuration file.\n", yellow, NC, default_color);
/*	printf("%sdevman, devs%s%s: manage your storage devices. Once in the devices screen, the possible \
commands are: [m]ount ELN [mountpoint], [u]nmount ELN, [q]uit. Ex: 'i 5', 'm 5 ~/misc', 'u 5', \
'q'.\n", d_yellow, NC, default_color);
	printf("%scrypt%s%s ELN, or filename: encrypt a file and protect it with a password. Crypt requires \
GPG to be installed.\n", d_yellow, NC, default_color);
	printf("%sdecrypt%s%s ELN, or filename: decrypt a file encrypted with the crypt command. Just as crypt, \
decrypt depends on GPG as well.\n", d_yellow, NC, default_color);*/
	printf("%s  alias%s%s: Show aliases, if any. To write a new alias simpy type 'edit' to open\
the configuration file and add a line like this: alias alias_name='command_name args...'\n", yellow, NC, default_color);
	printf("%s  sys%s%s [on/off/status]: Toggle on/off the system shell or check which shell is \
currently in use via the 'status' argument.\n", yellow, NC, default_color);
	printf("%s  splash%s%s: show the splash screen.\n", yellow, NC, default_color);
	printf("%s  path%s%s: show the entire current path.\n", yellow, NC, default_color);
/*	printf("%sx, term%s%s: start a new terminal (xterm by default) in the current path and in a \
different window.\n", d_yellow, NC, default_color);*/
	printf("%s  rf, refresh%s%s: refresh the screen.\n", yellow, NC, default_color);
	printf("%s  colors%s%s: show the color codes of the elements list.\n", yellow, NC, default_color);
	printf("%s  hidden%s%s [on off]: toggle hidden files on/off.\n", yellow, NC, default_color);
	printf("%s  v, ver, version%s%s: show %s version details.\n", yellow, NC, default_color, PROGRAM_NAME);
	printf("%s  license%s%s: display the license notice.\n", yellow, NC, default_color);
	printf("%s  q, quit, exit, zz%s%s: safely quit %s.\n", yellow, NC, default_color, PROGRAM_NAME);
	printf("%s  \nKeyboard shortcuts%s%s:\n\
%s  C-f%s: Toggle list-folders-first on/off\n\
%s  C-h%s: Toggle hidden-files on/off\n\
%s  C-h%s: Toggle system-shell on/off\n\
%s  C-r%s: Refresh the screen\n", white, NC, default_color, white, NC, white, NC, white, NC, \
white, NC);
}

void help_function (void)
{
	if (clear_screen)
		CLEAR;
	printf("%s%s %s%s %s(%s), by %s\n\n", cyan, PROGRAM_NAME, VERSION, NC, default_color, 
																		   DATE, AUTHOR);
	printf("%s is a completely text-based file manager able to perform all the basic \
operations you may expect from any other FM. Because inspired in Arch Linux and its KISS \
principle, it is fundamentally aimed to be lightweight, fast, and simple. On Arch's notion of \
simplcity see: https://wiki.archlinux.org/index.php/Arch_Linux#Simplicity\n", PROGRAM_NAME);
	printf("\nYou can also use %s as a shell, which, just as most Linux shells, includes \
the following features:\
\n  1) History function\
\n  2) TAB completion\
\n  3) Wildcards expansion\
\n  4) Braces expansion\
\n  5) Pipes (under development)\
\n  6) Aliases\
\n  7) Commands concatenation\
\n  8) Stream redirection (under development)\n", PROGRAM_NAME);
	printf("\nUsage: clifm [-AbfhlsSv] [-c command] [-p path]\n\
\n -A, --hidden\t\t\t show hidden files\
\n -b, --backup\t\t\t enable backup of deleted files\
\n -c, --command command_name\t execute a command via CliFM's shell and exit\
\n -f, --folders-first\t\t list folders first\
\n -h, --help\t\t\t show this help and exit\
\n -l, --on-the-fly\t\t 'cd' lists files on the fly\
\n -p, --path /starting/path\t tell CliFM which path do you want to begin from\
\n -s, --case-sensitive\t\t case-sensitive files listing\
\n -S, --system\t\t\t use the system shell\
\n -v, --version\t\t\t show version details and exit\n");
	printf("\n%sColor codes:%s%s\n\n", white, NC, default_color);
	color_codes();
	printf("%sCommands:\n%sNote: ELN = Element List Number\n%s", white, NC, default_color);
	list_commands();
	printf("\n%sNotes%s%s:\nBesides the above listed commands, you can also run every \
built-in Linux command or some other application by simply typing the command name and the \
corresponding arguments, if any.", white, NC, default_color);
	printf("\n\nIn case you want to run an external command whose name conflicts with some of the \
CliFM commands, say, 'rm', 'cp', 'mv', or 'ln', you can tell CliFM that you want to run the \
external command by simply typing the command name preceded by a colon. Ex: ':rm [args]'.");
	printf("\n\nWhen dealing with filenames containing spaces, you can use both single and \
double quotes (ex: \"this file\" or 'this file') plus escape sequences (ex: this\\ file).");
	printf("\n\nBy default, %s will start in your home folder. However, you can always \
specify a different path by passing it as an argument. Ex: clifm -p /home/user/misc. You can \
also permanently set up the starting path in the %s configuration file.\n\n", PROGRAM_NAME, PROGRAM_NAME);
	printf("You are able to choose whether to run external commands via the system shell \
or via CliFM own shell in four different ways: \n  1) Editing the config file\n  2) By making \
a semicolon preceed the command, this latter will be executed by the system shell \
(ex: ';cat .xinitrc')\n  3) Turning on/off the system shell via the 'sys' command\n  4) Via \
the -s, --system flag\n\n");
	printf("%sConfiguration file%s%s: ~/.config/clifm/clifmrc\n", white, NC, default_color);
	printf("Here you can permanently set up %s options, add aliases and some prompt \
commands (which will be executed imediately before each new prompt line).\n\n", PROGRAM_NAME);
	printf("%sProfile file%s%s: ~/.config/clifm/clifm_profile\n", white, NC, default_color);
	printf("In this file you can add those commands you want to be executed at startup. You \
can also define here some custom variables. Ex: 'dir=\"/path/to/folder\"'. This variable may \
be used as a shorcut to that folder, for instance: 'cd $dir'. Custom variables could also be \
temporarily defined by defining them via the command prompt: Ex: user@hostname ~ $ var=\"This \
is a test\". Temporary variables will be removed at program exit.\n\n");
	printf("%sLog file%s%s: ~/.config/clifm/log.cfm\n", white, NC, default_color);
	printf("The file contains a series of fields separated by a colon in the following way: \
'date:user:current_dir:command. All commands executed as external will be logged.\n\n");
	printf("%sBackup folder%s%s: /tmp/clifm_bk_username\n", white, NC, default_color);
	printf("Provided the backup function is enabled, all deleted elements will be temporarily \
stored in this folder.\n\n");
	printf("A bold green asterisk at the beginning of the prompt indicates that there are \
elements in the Selection Box.\n\nIn the prompt you'll also see, following the hostname and \
after a colon, a capital letter. This letter tells whether you are using the system shell ('S') \
or CliFM's shell ('C'). A prompt example: \n  %s*%s[user@hostname:C] ~ $%s \nThis prompt lets \
you know (letting aside the familiarly known information about username, hostname, and current \
working directory) that: 1) there are files in the Selection Box; and 2) you are using CLiFM's \
shell.\n", green, d_cyan, NC);
	if (flags & EXT_HELP) { //if called as external argument...
		return;
	}
	printf("\nPress Enter key to exit help...");
	getchar();
	free_dirlist();
	list_dir();
}

void version_function()
{
	printf("%s %s (%s), by %s\nContact: %s\n", PROGRAM_NAME, VERSION, DATE, AUTHOR, CONTACT);
}

void license  ()
{
printf("Copyright (c) 2017, L. Abramovich\n\n\
This program is free software; you can redistribute it and/or modify \
it under the terms of the GNU General Public License (version 2) as \
published by the Free Software Foundation.\n\n\
%s is distributed in the hope that it will be useful, but WITHOUT \
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or \
FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License \
for more details.\n\n\
You should have received a copy of the GNU General Public License \
along with this program. If not, see <http://www.gnu.org/licenses/>.\n", PROGRAM_NAME);
}
void bonus_function()
{
	printf("%sVamos %sBoca %sJuniors %sCarajo%s! %s*%s*%s*\n", blue, yellow, blue, yellow, 
																blue, blue, yellow, blue);
}
