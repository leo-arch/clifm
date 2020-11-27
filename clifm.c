
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

/* Compile as follows:

 * On Linux
 * $ gcc -O3 -march=native -s -fstack-protector-strong -lreadline -lcap -lacl -o 
 * clifm clifm.c
 * To be fully POSIX-2008 compliant pass the _BE_POSIX option to the compiler,
 * that is, -D_BE_POSIX

 * On FreeBSD:
 * gcc -O3 -march=native -s -fstack-protector-strong -lreadline -lintl -o
 * clifm clifm.c
 * NOTE: I still didn't checked the acl library on FreeBSD. It works 
 * without the need of -lacl.

 * You can also use tcc instead of gcc.
 *  */

/* Notes about compilation:
 * Do not forget to link the readline and cap libraries with the lreadline and 
 * lcap flags respectivelly, since we'll be using these libraries:
 * $ gcc -lreadline -lcap -o clifm clifm.c
 * It's strongly recommended to optimize your program adding these flags: 
 * -O2 and -march=native. You could also try -O3 instead of -O2, which is 
 * supposed to produce an even faster executable.

 * NOTE: The last test showed that compiling with -O2 makes the program
 * crash when moving selected directories into another directory (Valgrind
 * complains about "client switching stacks"). I'm not sure what it was,
 * but after compiling with -O3 the problem was gone.

 * You can also protect your program from stack buffer overflow exploits by 
 *  adding this flag: -fstack-protector (-protection, +performance), 
 * -fstack-protector-strong, or -fsatck-protector-all (+protection, 
 * -performance). NOTE: Since 2014, Arch Linux packages are compiled with 
 * -fstack-protector-strong. For debugging purposes, you can also add some 
 * debugging flags (optimization is not necessary here):
 * $ gcc -g -Wall -Wextra -Wstack-protector -Wpedantic -lreadline -lcap -o 
 * clifm clifm.c
 * Note: -g adds debugging symbols (readable by Valgrind and GDB, for example) 
 * to the code, but it makes the executable two or three times bigger.
 * When debugging isn't necessary anymore, simply remove all the debugging 
 * flags. The final compilation should not include warning and error flags at 
 * all, but only optimization and protection flags, plus readline and cap 
 * libraries:
 *
 * $ gcc -O3 -march=native -s -fstack-protector-strong -lreadline -lcap -o 
 * clifm clifm.c
 *
 * NOTE: In Debian Bullseye (gcc 10.2.0) I got undefined reference error when
 * compiling using the above command. The solution was simply to alter the
 * order of the parameters: gcc -o clifm clifm.c -O3 -march=native -s
 * -fstack-protector-strong -lreadline -lcap -lacl
 * 
 * The -s option generates a stripped (and thereby smaller, if not faster) 
 * binary (by removing debugging symbols)
 * tcc (the Tiny C Compiler) can also be used by simply replacing "gcc"
 * by "tcc" in the command above. The global variable __dso_handle is
 * used below to prevent tcc from complaing about the __dso_handle
 * symbol being undefined.
 */

/*
* DEPENDENCIES: 'glibc', 'ncurses', 'libcap', 'readline', 'coreutils' 
* (providing basic programs such as rm, cp, mkdir, etc). For Archers: all 
* these deps are part of the 'core' repo, and 'glibc' is also part of the 
* 'base' metapackage. In Debian three packages must be installed: 'libcap-dev'
* , 'libreadline-dev', and 'libacl1-dev'. In Fedora/redhat: 'libcap-devel', 
* 'readline-devel', and 'libacl-devel'. In Slackware: 'readline', 'acl', 
* 'libcap', and 'libtermcap' (and then compile adding -ltermcap to gcc).

* Getting dependencies:
* $ ldd /path/to/program
* $ objdump -p /path/to/program | grep NEEDED
* $ readelf -d /path/to/program | grep 'NEEDED'
* $ lsof -p pid_running_program | grep mem
* $ cat /proc/pid_running_program/maps
* # pmap pid_running_program
* Once you get the library, get its full path with 'find', and find which 
* package owns that file with 'pacman -Qo full_path'
* OPTIONAL DEPENDENCIES: 'du' (to check dir sizes), and 'file', to check files
* MIME types and open the file with the associated appplication (via my mime
* function).
* */

/* On code optimization see:
 *  https://www.codeproject.com/Articles/6154/Writing-Efficient-C-and-C-Code-Optimization */

/* To see the complete list of predefined macros run:
$ gcc -E -dM somefile.c 
or: $ echo "" | gcc -E -dM -c -
of course you can grep it to find, say, linux' macros, as here. */


/* NOTES:
 * I've tried pthread to run some procedures in parallel, which is supposed to 
   boost the program speed. However, the serialized (non-threaded) version was 
   always faster.
 * Use decrementing instead of incrementing loops whenever it's possible.
 * Make sure all strings are null terminated.
 * Most time-consuming functions according to gprof: get_path_programs (211 
   miliseconds per call), get_history (112), bookmarks_function (96), 
   open_function (85.88), list_dir (85.61), check_log_file (75), prompt (28.26), 
   exec_prompt_cmds (27.99), and exec_cmd (27.54). Among them, those most 
   called are: exec_cmd (32), exec_prompt_cmds (16), prompt (16), list_dir (11), 
   and open_function (5). get_path_programs() and get_history(), though the 
   most time-consuming by far, are called only once at the beginning of the 
   program. The most crucial function to be optimized is still list_dir(). 
 * When testing some code performance, always do it outside Valgrind and 
   optmimizing it via the compiler. What is slow in Valgrind, might be fast 
   outside of it, and what is faster without optimization, might not be so 
   when optimized.
*/

/* UNICODE: The whole thing about UNICODE is related non-ASCII 
 * character sets, like Chinese, Greek, Russian, and so on. If I do not 
 * pretend to make CliFM compatible with these non-latin languages, there is
 * no need to deal with UNICODE at all.
 * Whereas the original ASCII table (7-bit, 128 bytes) was meant for English 
 * only (no accents nor diacritics marks at all), the 8-bit (256 bytes) or 
 * "extended" ASCII table supports, thanks to the extra bit, virtually all 
 * languages based on the latin alphabet like French, Spanish, German, 
 * Portuguese, and so on.
 * If you create a file named "ñañó" (it makes use of the extended ASCII 
 * charset), it depends on the terminal emulator you use to correctly display 
 * the string. Xterm and Aterm, for example, fail, whereas the Linux console, 
 * urxvt and lxterminal don't.

 * I've made a few tests with some Russian and Greek file names: when using 
 * terminal emulators supporting UNICODE, filenames are displayed correctly,
 * but, as it was expected, columned output fails because of not being able
 * to correctly get the amount of characters for each of these filenames. 
 * This is mainly because of how strlen() works and how non-ASCII chars are
 * construed: strlen() counts bytes, not chars. Now, since ASCII chars take 
 * each 1 byte, the amount of bytes equals the amount of chars. However, 
 * non-ASCII chars are multibyte chars, that is, one char takes more than 1 
 * byte, and this is why strlen() does not work as expected for this kind of 
 * chars. 
 * To fix this issue I should replace the two calls to strlen() in list_dir() 
 * by a custom implementation able to correctly get the length of non-ASCII 
 * (UTF-8) strings: u8_xstrlen(). Take a look at get_properties() as well 
 * (DONE), just as to any function depending on strlen() to print file names 
 * (DONE too, I think) */

/* A note about my trash implementation: CliFM trash system can perfectly read 
 * and undelete files trashed by another trash implementation (I've tried 
 * Thunar via gvfs), but not the other way round: Thunar sees files trashed 
 * by CliFM, but it cannot undelete them (it says that it cannot determine the 
 * original path for the trashed file, though the corresponding trashinfo file 
 * exists and is correctly written). I think Thunar is doing something else 
 * besides the trashinfo file when trashing files. */

/*
 ###### SOME ADVICES TO KEEP IN MIND #####

 ** Only global variables are automatically initialized by the compiler; local 
    variables must be initialized by the programer. Check there are no 
    uninitialized local variables.
 ** THE STACK (FIXED ARRAYS) IS MUCH MORE FASTER THAN THE HEAP (DYNAMIC 
	ARRAYS)!! If a variable is only localy used, allocate it in the stack.
 ** Take a look at the capabilities() function.
 ** When the string is initialized as NULL, there's no need for a calloc 
	before the reallocation.
 ** It seems that strlcpy (and family) is safer than strcpy (and family). On 
	these functions see its manpage and https://www.sudo.ws/todd/papers/strlcpy.html. 
    Whereas strcpy might produce a buffer overflow (if dest is bigger than 
    dest), strncpy will not NULL terminate the string if dest does not contain 
    a NULL char among the n specified bytes in strncpy. Now, this will produce 
    a segfault if, for example, we use strlen, which looks for a NULL 
    terminated byte. strlcpy, by contrast, adds a single null char at the end 
    of the string copied into dest. On the other side, strncpy will zero fill 
    the remaining bytes of dest whenever src is shorter than dest, and this is 
    a performance issue. Let's say we have this code: 
    * 'strncpy(dest, src, 100)', and that src length is 50 bytes. In this case, 
	strncpy will copy the 50 bytes of src into dest, plus 50 more zeroes to 
	reach 100 bytes. Bycontrast, strlcpy doesn't do this:  
	See both strncpy and strlcpy manpage.
	NOTE: to use these functions: 1 - include the header bsd/string.h and 
	2 - link the bsd library at compile time as follows: $ gcc -lbsd ...
 ** Take a look at the output of cppcheck --enable=all clifm.c
 ** A switch statement, when possible, is faster than an if...else chain.
 ** It seems that stat() is faster than access() when it comes to check files existence. See:
	https://stackoverflow.com/questions/12774207/fastest-way-to-check-if-a-file-exist-using-standard-c-c11-c
 ** Perhaps it would be nice to add a vault protected by a password (see 
	passwd.c)
 ** Find a way to remove all unused alias files in /tmp.
 ** Take a look at the ouput of gcc When compiled with the following flags:
	(SOLVED) a) -Wformat-overflow=2: I get a couple of errors coming from 
	get_properties(). 
	(SOLVED) b) -Wformat=2 in a couple of minor functions.
	(SOLVED) c) -Wformat-security= there are a couple of fprintf that do not 
	specify the string format, which could be a security whole.
	(SOLVED) d) -Wformat-truncation=2: some printf outputs may be truncated 
	due to wrong sizes.
	(SOLVED) e) -Wformat-y2k: The strftime manpage says we don't need to worry 
	about these warnings. It recommends indeed to use -Wno-format-y2k instead 
	to avoid these warnings.
	f) -Wstrict-overflow=5
	g) (SOLVED) -Wshadow (duplicated var names)
	(SOLVED) h) -Wunsafe-loop-optimizations
	i) -Wconversion: many warnings (incorrect use of data types)
 ** Check different kinds of C (traditional, ISO, ANSI, etc.) Check also POSIX 
    specifications.
 ** Take a look at gcc optimization options (-Ox). The default option is -O0 
    (disabled). It's recommended by the GNU guys themselves to use -O2 instead 
    of -O3. In fact, -O2 is the default optimization level for releases of GNU 
    packages. See: 
    https://stackoverflow.com/questions/1778538/how-many-gcc-optimization-levels-are-there
	See also the -march and -mtune flags. To see gcc default compiling options 
	type: $ gcc -Q --help=target | grep 'march\|mtune='.
 ** Some optimization tips: 1 - For large chains of decisions 
	(if...else...else) it may be faster to use SWITCH. 2 - DECREMENTING for 
	LOOPS are faster than incrementing ones. Ex: (i=10;i!=0;i--) is faster 
	than its more traditional form: (i=0;i<10;i++). This is so because it is 
	faster to evaluate 'i!=0' than 'i<10'. In the former, the processor says: 
	"is i non-zero?, whereas in the latter it has to perform an extra step: 
	"Subtract i from 10. Is the result non-zero? 3 - I should use UNSIGNED INT 
	instead of int if I know the value will never be negative (some says this 
	is not worth it); 4 - When it comes to single chars (say 'c') it's faster 
	to declare it as int than as char. See: 
	https://www.codeproject.com/articles/6154/writing-efficient-c-and-c-code-optimization
 ** Check ALL the exec commands for: 1 - terminal control; 2 - signal handling.
 ** Check size_t vs int in case of numeric variables. Calloc, malloc, strlen, 
    among others, dealt with size_t variables; In general, use size_t for 
    sizes. It is also recommended to use this variable type for array indexes. 
    Finally, if you compare or asign a size_t variable to another variable, 
    this latter should also be defined as size_t (or casted as unisgned 
    [ex: (unsigned)var]). 
    Note: for printing a size_t (or pid_t) variable, use %ld (long double).
    Note2: size_t does not take negative values.
 ** Check the type of value returned by functions. Ex: strlen returns a 
	size_t value, so that in var=strlen(foo), var should be size_t.
 ** Check that, whenever a function returns, the variables it malloc(ed) are
	freed.
 ** Scalability/stress testing: Make a hard test of the program: big files 
    handling, large amount of files, heavy load/usage, etc. 
 ** Destructive testing: Try running wrong commands, MAKE IT FAIL! Otherwise, 
	it will never leave the beta state.
 ** The __func__ variable returns the name of the current function. It could 
	be useful to compose error logs.
 ** Take a look at the OFM (Orthodox File Manager) specification:
	http://www.softpanorama.org/OFM/Standards/ofm_standard1999.shtml
	The Midnight Commander could be a good source of ideas.
 ** When using strncpy(dest, n, src), pay attention to this: if there is no 
	null byte among the 'n' bytes of 'src', the string placed in 'dest' won't 
	be null terminated. To avoid this problem, always initialize 'dest' as 
	follows: 
		dest=calloc(strlen(src)+1, sizeof(char))
	In this way, 'dest' always have a terminating null byte.
 ** Pay attention to the construction of error messages: they should be enough
	informative to allow the user/developer to track the origin of the error.
	This line, for example, is vague: 
	  fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
	Use __func__ or some string to help identifying the problem:
	  fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, __func__, strerror(errno));
	If a file or path is involved, add it to the message as well. If possible,
	follow this pattern: program: function: sub-function: file/path: message
 ** Check all the 'n' functions, like strncpy or snprintf, for buffer limit 
	being correctly set. The whole point of the 'n' option is to protect the 
	buffer against being overflowed.
      char buf[MAX_STR]="";
      snprintf(buf, strlen(str), "%s", str);
    This code is wrong for, though you might assume that str will never be 
    larger than MAX_STR, you cannot be sure. The right way would be rather:
      snprintf(buf, MAX_STR, "%s", str);
    In this way, no matter how long is STR, at most MAX_STR bytes will be 
    copied into buf, and thus no buffer overflow is possible.
 ** Take a look at the GNU C Coding Style: 
	"https://developer.gnome.org/programming-guidelines/stable/c-coding-style.html.en"
 ** When possible, use the 'register' keyword for counters: registers are 
	even faster than RAM. However, it's up to the compiler to implement the
	suggestion. 
 ** When compiling with -ansi (and -Wpedantic) I get a bunch of two kinds of 
	errors: 1) mixed declaration and code (int i=0); b) length of string literal
	is larger than 509 bytes (printf("blablabla......"));
 ** ALWAYS use the appropriate data types. For example, DO NOT use char for a
	flag just in order to save a few bytes. This might work in x86 machines, but
	it will probably fail in ARM machines, for instance.
 ** PORTABILITY. From more to less portable: standard (c89-90), POSIX, GNU 
	extensions. Stick to the standard as much as possible. For example, 
	asprintf() and execvpe() are GNU extensions, statx() is Linux specific 
	and getline() is POSIX since 2008. To test the program for portability, 
	define one of the following, compile and check compilation errors and 
	warnings:
	_POSIX_C_SOURCE 200112L
	_POSIX_C_SOURCE 200809L
	_GNU_SOURCE
	You can also check the manpage for each function, specially the "Conform to" 
	or "Attributes" section.
 ** When should freed pointers be set to NULL? When this pointer might be reused
	later. Ex: in this block "char *ptr ... free(ptr); ptr = NULL; return" 
	setting the pointer to NULL makes no sense, since the function finishes,
	discarding all local variables, after using the pointer without never
	using it again.
 ** When testing keep GCC warning flags enabled: Wall, Wpedantic, Wextra, 
	Wstack-protector, Wshadow, Wconversion
 ** Be careful when substracting from an unsigned int, like size_t, since the
	result will always be positive, which is not the expected result: give two
	size_t values, say, 0 and 1, substracting the second form the first results
	in UINT_MAX (a very large positive value), and not -1. Casting to int seems
	a good solution. Example: (int)(a - b);
*/

/** 

							###############
							#  TODO LIST  #
							###############
*/
/*
 ** I could use the _err function to print some tip at startup, like MC does.
 ** Take a look at the 'ls' source and look for columned listing and strlen
	for UTF-8 strings.
 **	Take a look at the FreeDesktop specification for MIME apps:
	https://specifications.freedesktop.org/mime-apps-spec/mime-apps-spec-1.0.1.html
 ** The color of files listed for possible completions depends on readline,
	which takes the color values from LS_COLORS. I can modify this variable
	to use CliFM color scheme, but I cannot make it use colors for filetypes
	it doesn't recognize, like empty regular files, files with capabilities
	and so on.
 ** When changing filetype colors on the fly, that is, editing the config
	file while running CliFM, readline colors won't get updated until
	next start. I guess I should find a way to reinitialize readline without
	restarting the program.
 ** Add a help option for each internal command. Make sure this help system is
	consistent accross all commands: if you call help via the --help option, 
	do it this way for all commands. Try to unify the usage and description
	notice so that the same text is used for both the command help and the
	general help. I guess I should move the help for each command to a 
	separate function.
 ** Customizable keybindings would be a great feature!
 ** The screen refresh check is really weak: 
	1) The screen is refreshed only for a few commands (rm, mkdir, mv, cp, and 
	   ln), but there are for sure more commands able to modify the filesystem. 
	   Even if not, the user could always use a custom script.
	2) External commands never refresh the screen.
	3) The screen is refreshed if the executed command was successfull. 
		However, if the CWD is $HOME, for example, and the command is 
	   'touch /media/test/file', the screen should not be refreshed.
	In other words, I refresh the screen on a per command basis.
	When should the screen actually be refreshed:
	a) Whenever a file is added or removed from the current list of files
	b) Whenever the amount of files in a directory (in the current list of 
	   files) is decremented or incremented.
	In both cases, what command was executed does not matter, that is, the 
	ideal check should not be made on a per command basis. The first case is 
	easy to solve: count files before and after the command execution, and then 
	compare the results (not bullet proof, however: if file1 is removed and 
	file2 is created with just one command, the amount of files remains the 
	same, and, nonetheless, the screen should be refreshed). The second case, 
	however, is quite harder/expensive to solve than the first one.
 ** When jumping from one directory to another with the 'b/f !ELN' command,
	for example from 1 to 4 in the directories list, 'back' does not go back
	to 1, but to 3. It feels ugly and unintuitive.
 ** The glob, range, and braces expansion functions will expand a fixed amount 
	of non-expanded strings (10). I should use a dynamic array of integers 
	instead. However, 10 should be more than enough: this is not a shell!
 ** glob() can do brace expansion!!
 ** Check all functions for error, for example, chdir().
 ** run_in_foreground() returns now the exit status of the child. Use this 
    whenever needed.
 ** There's a function for tilde expansion called wordexp(). Currently using
	tilde_expand(), provided by the readline library.
 ** The list-on-the-fly check is not elegant at all! It's spread all around 
	the code. I'm not sure if there's an alternative to this: I have to 
	prevent listdir() from running, and it is this function itself which is 
	spread all around the code. I cannot make the on-the-fly test inside the 
	dirlist function itself, since the function is preceded by free_dirlist (), 
    which frees the dirlist array to be filled again by listdir(). 
    So, this freeing must also be prevented, and thus, the check must be 
    previous to the call to listdir().
 ** Set a limit, say 100, to how much directories will the back function 
	recall.
 ** When creating a variable, make sure the new variable name doesn't exist. 
	If it does, replace the old position of the variable in the array of user 
    variables by the new one in order not to unnecessarily take more memory 
    for it.
 ** Add an unset function to remove user defined variables. The entire array 
	of user variables should be reordered every time a vairable is unset.
 **	Add support for wildcards and nested braces to the BRACE EXPANSION function, 
    like in Bash. 

###################################
 * (DONE) Allow get_properties() to recognize the existence of ACL properties,
	like 'ls -l' does via the plus sign.
 * (DONE) TMP_DIR should not be /tmp/clifm, since this is common to all users, 
	in which case network mounpoint could be overwritten. It should be rather
	/tmp/clifm/user.
 * (DONE) Add an option to disable the dir counter, since this feature could
	slow things down too much when listing files on a remote server.
 * (DONE) Add network support for SSH, SMB, and FTP mounts. Use sshfs, 
	mount.cifs, and curlftpfs, for SSH, SMB, and FTP respectivelly.
 * (DONE) Add an option to disable files sorting.
 * (DONE) Write a better color code check (is_color()).
 * (DONE) Be careful with variable length arrays (VLA), since they might smash 
	the stack. Compile with the -Wvla option to list VLA's. A VLA is like this:
	"size_t len = strlen(str); char array[len];" Though VLA's are allowed by
	C99, they are not in C90 (in C11 they are optional). 
	See "https://nullprogram.com/blog/2019/10/27/"
 * (DONE) Write one function to create the config file and use it for both
	init_config() and profile_add()
 * (DONE) Comment the config file to explain the meaning of each option and 
	value, something like the SSH and Samba config files.
 * (DONE) Add to "bm del" the possibility to specify the bookmark name to be
	deleted directly from the command line ("bm del name").
 * (DONE) By default, the commands log function should be disabled. Add an 
	option to the config file (DisableCmdLogs) and a check before calling 
	log_function() (or at the beginning of the function itself) and before 
	storing the last command in the prompt.
 * (DONE) Find a way to log the literal command entered by the user, and not 
	the expanded version. I guess a global variable, say last_cmd, will do 
	the job.
 * (DONE) Remove user from the logs line: it's redundant.
 * (DONE) 'trash del' still do not accepts ranges! Fix it.
 * (DONE) Inlcude all the xasprintf block into a new function, say _err(), so
	that only one line of code is needed to handle error logs and messages
 * (DONE) At the end of prompt() I have 7 strcmp. Try to reduce all this to 
	just one operation. I think I should put all this into a function, say, 
    int record_cmd(char *input). Instead of directly comparing strings with 
    strcmp(), I should first compare the first char of both strings, and then 
    use strcmp only if they match.
 * (DONE) Add aliases for commands TAB completion (get_path_programs)
 * (DONE) Use CamelCase for option names in the config file.
 * (DONE) Add prompt customization. Take a look at the Bash code.
 * (DONE) Bash implementations of xmalloc and xrealloc return a char 
	pointer (char *) and each call to these functions is casted to the 
	corresponding variable type. Ex: char *var; var=(char *)xcalloc(1, sizeof(char)). Though a void
	pointer might work, casting is done to ensure portability, for example,
	in the case of ARM architectures.
 * (DONE) Add file extension support to the mime function (*.ext=app).
 * (DONE) If the MIME file doesn't exist, do not create an empty one, but try to
	import the values from the 'mimeapps.list' file.
 * (DONE) Replace the xdg_open_check to check now for the 'file' command and 
	set the corresponding flag. Recall to check this flag every time the mime 
	function is called. 
 * (DONE) Add the possibility to open files in the background when no 
	application has been passed (ex: o xx &). DONE, but now the pid shown is 
	that of xdg-open, so that when the user tries to kill that pid only 
	xdg-open will be killed. SOLVED: Not using xdg-open anymore, but my 
	own resource opener: mime.
 * (DONE) Check compatibility with BSD Unices. Working fine in FreeBSD.
 * (DONE) Test the program in an ARM machine, like the Raspberry Pi. Done and
	working, at least by now.
 * (DONE) The logic of bookmarks and copy functions is crap! Rewrite it.
 * (DONE) Add TAB completion for bookmarks.
 * (DONE) Files listed for TAB completion use system defined colors instead 
	of CliFM own colors. I only need to set the LS_COLORS environment 
	variable using CliFM colors.
 * (DONE) Do not allow the user to trash some stupid thing like a block or a 
	character device.
 * (DONE) Bookmarks: I don't think "hotkey" is the right word. "Shortcut" 
	looks better.
 * (DONE) Allow the bookmarks function to add or remove bookmarks from the
	command line, something like "bm add PATH".
 * (DONE) Conditional and sequential commands cannot be mixed: chained cmds 
	are either sequential or conditional. This cannot be done: 
	cmd1 && cmd2 ; cmd3. SOLUTION: Mix both in one function and make a test 
	for ';' or "&&" to decide whether to execute each separated command 
	conditionally or sequentially.
 * (DONE) Add support for commands conditional execution (cmd1 && cmd2). I 
	should modify exec_cmd(), and each function called by it, to return a 
	value. It's not hard but time consuming. For the rest, it's basically the 
	same as sequential execution.
 * (DONE) Add support for commands sequential execution (cmd1;cmd2).
 * (DONE) Check for the system shell. If it doesn't exist, print a warning 
	message.
 * (NOT NEEDED) A pager for displaying help would be great. Not needed 
	anymore: the current help page is now quite short, and the complete 
	version have been transfered to the manpage.
 * (DONE) Add ranges to deselect and undel functions. get_substr() is only 
	used by these two functions, so that I can modify it to return the 
	substrings including the expanded ranges.
 * (DONE) Stop TAB completion when in bookmarks, mountpoints, undel or desel 
	functions.
 * (DONE) Add an argument, -P, to use an alternative profile.
 * (DONE) Allow the use of ANSI color codes for prompt, ELN's and text color, 
	just as I did with filetypes. The only issue here is that when I use some 
	background color for the prompt, this same color is somehow transferred 
	to the command line (text) background color (after running list_dir()).
	SOLUTION: added a printf() at the beginning of prompt(). Not sure why,
	but it works.
 * (DONE) Add the possibility to customize filetypes colors, just like 'ls' 
	does via the LS_COLORS environment variable. Add ELN color customization
	too.
 * (DONE) I should not use any config file if there's no home, only the 
	defaults. I should use a global flag for this. The same goes for
	config dir and config file. A flag for tmp dir, sel file and trash dir 
	is needed as well. Write a function to load default options. Prevent 
	functions from trying to access what is not there printing a 
	nice message.
 * (DONE) I'm stupidly wasting resources when calling calloc:
	This: "p=calloc(4, sizeof(char))" allocates memory for four chars (1 byte
	each), while this "p=calloc(4, sizeof(char *))" allocates memory for four
	POINTERS to char (8 bytes each!!!!) When fixing this, make sure to add 1
	to each strlen(str) in calls to calloc.
	Proper allocation of strings array:
		p=calloc(strings_num+1, sizeof(char *));
		p[i]=calloc(len+1, sizeof(char));
	Running the corrected and the non-corrected version side by side, the 
	former takes between 100 and 500 kb less. Nice!!
 * (DONE) Replace the dirlist dirent struct by a simple array of strings. Each
	member of a dirent struct takes 280 bytes, while most filenames take 
	between 20 and 30 bytes. If you have 100 files listed, the difference in
	memory footprint is significant (for a program like this that is intended
	to be as lightweight as possible), more than 250 bytes per file: 
	2,5Kb total).
 * (DONE) If some new program is installed while in CliFM, this latter needs
	to be restarted in order to recognize the new program for TAB completion.
	SOLUTION: I just added an update function after running an external 
	program.
 * (DONE) Rewrite 'ds *'. Since wildcards are now expanded before exec_cmd(), 
	that is, before reaching the deselect function, what it gets instead is a 
	bunch of selected filenames. WORKAROUND: I replaced 'ds *' by 'ds a'.
 * (DONE) Replace single and double quotes for completion and use instead
	bash-style quoting via escape char: Not "'file name'", but "file\ name".
 * (DONE) Handle filenames with spaces and special chars in TAB expansion, 
	both for ELN's and filenames.
 * (DONE) The "clean" option (taken by history, log, trash, and msg commands) 
	should be reanamed to "clear". Besides, back and forths commands already 
	use "clear".
 * (DONE) Add ELN auto-expansion. Great!!!!
 * (DONE) Fix file sizes (from MB to MiB)
 * (UNNEEDED) When refreshing the screen, there's no need to reload the whole 
	array of elements. Instead, I should simply print those elements again, 
	without wasting resources in reloading them as if the array were empty. 
	Perhaps I could use some flag to do this. Or perhaps it's a better idea 
	to keep it as it is: if something was modified in the current list of 
	files, I want the refresh function to print the updated data.
 * (DONE) The properties function prints "linkname: No such file or directory"
	in case of a broken symlink, which is vague: the link does exists; what 
	does not is the linked file. So, It would be better to write the error
	message as follows: "linkname -> linked_filename: No such file..."
	Currently properties for the broken symlink are shown, plus a text next to
	the filename: "broken symlink".
 * (DONE) Get rid of the get_bm_n() function. It's redundat, since 
	get_bookmarks() can do what it does (to get the amount of bookmarks) via a 
	global variable (bm_n) without the need to open the bookmarks file once 
	again.
 * (DONE) Use gettext to make the program tanslatable to any language
 * (DONE) I'm not sure whether to allow the user to open/execute non-regular
	files like socket, character, block, etc. Since xdg-open allows it, I 
	guess I should do it as well. However, the user still can run 
	"application filename", for example, "leafpad socket_file". Add this note
	to the "Cannot open file" message.
 * (DONE) Add an "all" option to the undel function to undelete all trashed
	files (just like the "ds all" command).
 * (DONE) Make the undel function reload the undel screen after undeleting if 
	some trashed file still remains.
 * (DONE) Add to the properties function the ability list properties for more
	than one file: "pr 1 2-5 10".
 * (DONE) Add to the search function the ability to search for files in any
	directory specified by the user: "/str /path".
 * (DONE) Rewrite open_function(). Write a separate function for 'cd'.
 * (DONE) Remove the -c argument (run command and exit): it's a remaining of
	the old shell version of this program.
 * (DONE) The errors system can display not only errors, but also warnings, 
	like in the case of modifiyng ~/.Xresources, and any kind of message. 
	Replace the red "E" in the prompt by three different colors and letters: 
	red "E" for errors, yellow "W" for warnings, and green "N" for simple 
	notifications, hints, and so on. I should also modify the log_errors() 
	function and other variables to reflect this change.
 * (DONE) Add a pager for the long view mode
 * (DONE) Add a long view mode (or info mode) for files listing (basically 
	similar to the output of 'ls -l'): filename permissions owner group size 
	mod_time. Add a keybind (A-l), an argument (-l and -L), and an option
	(Long view mode) in the config file.
 * (DONE) Add keybindings for some basic operations: u-j-h-k for filesystem
	navigation (go up to parent dir, navigate the directory history list, back
	and forth), bookmarks manager, files selection (sel *), chdir to home and
	to root.
 * (DONE) Construct the Path line for trashinfo files as in URL's (as 
	recommended by the freedesktop specification). Example:
	/path/to/file%20with%20spaces, where 20 is the hex value for space.
 * (DONE) Keep code and comments up to 79 columns at most. For GNU advices on 
	writing C code see: 
	https://www.gnu.org/prep/standards/html_node/Writing-C.html#Writing-C
 * (DONE) Check error messages in the trash function: both trash_element() and 
	trash_function() print error messages, and that's redundant.
 * (DONE) Replace "CLiFM" and "clifm" (only present in config files and help) 
	by macros (PROGRAM_NAME, PNL). 
 * (DONE) Add a system to log, store, and display error messages. a) Error 
	messages should be displayed immediately before the prompt; b) the user 
	should be able to check errors from within CliFM itself; c) a prompt 
	inidicator could be useful; d) log errors to a file; e) do not log trivial 
	errors.
 * (DONE) Let the user choose the color of the command line text.
 * (DONE) Add bold colors to the list of colors the user may choose for the 
	prompt and the command line text.
 * (DONE) Remove move_function(), it's not necessary.
 * (DONE) The jobs function doesn't make much sense for this program, and 
	besides is badly implemented. It's just a remain of the old shell version. 
	Remove it.
 * (DONE) Automatically deselect files ONLY in case of trash, remove, or move,
    simply because in these cases selected files are not there anymore. 
    Otherwise, let the user deselect whatever/whenever he/she wants: multiple 
    operations on selected files might be useful. For example: 'ls -l sel', 
    'cp sel dir1', 'cp sel dir2'. It's stupid to deselect files after a simple 
    'ls sel'.
 * (DONE) Add error check for chdir(). Since this is a file manager, where 
    directory changing is crucial, the chdir() function must be heavily 
    checked against errors.
 * (DONE) Add AUTOMATIC SEL EXPANSION: a) It would allow the use of sel for 
	any command, and b) I wouldn't need to deal with the sel argument inside 
	functions. NOTE: Do not expand sel if it is the first input string 
	(args_n == 0), that is, the sel command, but only as argument (args_n > 0).
    'mv sel' should be written as 'mv sel .', for example. As a workaround I 
    could check the last argument passed to 'mv', and, if not a directory, add 
    a new argument pointing to CWD, that is, ".".
 * (DONE) Add trash functions to help.
 * (DONE) Remove exec_prompt_cmds(): it's a waste of resources to call such a 
    little function, which besides is used only once. Run the code in place 
    instead. Function calls are expensive, in the best zero cost, but to be 
    sure it will not be faster than the code it runs running without the 
    function call. So, do not use functions to do this kind of little things. 
    Simply do the little thing instead of calling a function to do it. On 
	functions cost see: 
	https://mortoray.com/2010/10/12/the-cost-a-function-call/
 * (DONE) ELN expansion shouldn't be a separated function: integrate it into 
    parse_input_str().
 * (DONE) Add a little trash indicator to the prompt.
 * (DONE) Remove from the sel function all the code related to wildcards, ELN's 
    and ranges.
 * (DONE) Add ranges to remove_function. Done indirectly by adding automatic
    ranges expansion. Now not only rm, but every command can use ranges.
 * (DONE) Add automatic ELN ranges expansion.
 * (DONE) Replace system() by launch_execle().
 * (DONE) The brace expansion function will only expand the last braced string in 
    the user input. Do what I did in the glob expansion function.
 * (DONE) Remove the backup function. Replaced by trash.
 * (DONE) run_glob_cmd() is now obsolete. Remove it.
 * (DONE) Rewrite copy and remove functions: a) ELN and wildcard checks aren't 
    necessary anymore; b) There is no need to check for the command existence; 
    c) Only take care of the 'sel' argument and let everything else to the 
    corresponding command.  
 * (SOLVED) When logging a command, how to know whether an argument, which is a 
    number, corresponds to an ELN or is just a number (Ex: 
    'echo 0 > /tmp/updates' or 'chmod 755 some_file')? Since automatic ELN 
    expansion, this problem is disolved.
 * (DONE) Add a glob expansion function to automatically expand wildcards in 
	the command line. Added directly in parse_input_str().
 * (DONE) Check directories permissions recursivelly in wx_parent_check(), 
	critical for the trash function.
 * (DONE) Check for the immutable bit in both directories and regular files in
    wx_parent_check ().
 * (DONE) Add a trash function. 
	See "freedesktop.org/wiki/Specifications/trash-spec".
    Trash location: '$HOME/.local/share/Trash/{files, info}'
    Functions: list, delete (remove from trash), undelete (restore to original 
    location)
	Preserve original properties (use 'cp -ra')
	Allow to trash files with same name and location (eg: file(1), file(2))
 * (DONE) Do not make the automatic ELN expansion an optional feature. In this \
	way I can get rid of all the checks for ELN's in functions.
 * (DONE) Add some kind of pager to list directories with large amount of 
	files.
 * (DONE) The touch() and mkdir() functions aren't necessary at all, execept 
	for the fact that they, unlike the GNU versions, refresh the screen. I 
	could call the GNU versions and then refresh the screen if they where 
    successfull (use launch_execve()!!!).
 * (DONE) Add a function to automatically expand ELN's. It would be nice to 
	allow the user to turn this feature on and off, since it might conflict 
	with some external commands like 'chmod'.
 * (DONE) Use const for functions parameters if these parameters won't be 
	modified by the function. 'const' means read-only, so that the function 
	reads its content only once, and do not need to re-read it at every access. 
	It's faster therefore than a read-write paramenter. The compiler itself 
	will complain if a function tries to write into a read-only variable.
 * (DONE) Add a forward function as a complement to the back() function.
 * (DONE) Try to use launch_execle() for external commands as well.
 * (DONE) Replace execve by execvp: execvp, unlike execve, doesn't requiere 
	the first argument to be the full command path, since the function itself 
	checks the PATH environment variable.
 * (DONE) Remove references to the search_mark variable, unused since long 
	time ago.
 * (DONE) I'm using more than 15 calls to printf to print the splash screen. 
	What a waste! Reduce them to just one.
 * (DONE) Re-write the alphasort_insensitive function using pointers, to get 
	rid of the leading dot of hidden files, and strcasecmp() for a case 
	insensitive comparison. In this way, the function is twice faster than 
	before (specially when big amount of files are involved)!!
 * (DONE) Write a custom alphasort function using strcmp(), instead of 
	strcoll(), used by alphasort, to compare dir names. Why? 
	Simply because strcmp() is faster than strcoll().
 * (DONE) There are two unused functions: str_ends_with, and strbfrlst. 
	Remove them.
 * (DONE) Try a custom and faster implementation of atoi (say, xatoi) and 
	rewrite digits_in_num to make it faster. Both functions are called by 
	list_dir().
 * (DONE) Remove the call to lstat() from both folder and file_select 
	functions and replace it by a simple check for entry->dt_type == DT_DIR. 
	This data is provided by the own scandir() function. lstat() simply adds 
	an extra and unnecesary step.
 * (DONE) /tmp is being accessed twice for each command line input, once to 
	check for aliases and once to run prompt cmds. I guess this could be 
	slowing things down. I should try storing both aliases and prompt cmds 
	into two arrays of strings at the beginning of the program. Rewrite the 
	get_aliases_n_prompt_cmds function to use global variables instead of tmp 
	files to store this information. Result: whereas there's seems to be no 
	change int the prompt function speed, the check_for_alias function is now 
	almost 6 times faster. However, the whole program takes now 100k aprox of 
	memory more than before. But, it's still below the 5Mb, 4.6 indeed.
 * (DONE) User defined variables only works for the CLiFM shell. Make it work 
	for the system shell as well, it's a nice feature.
 * (DONE) Get rid of the DEFAULT_PATH macro (it gets the user's home dir). It 
	calls two functions (getpwuid and getuid) every time it is called, being 
	thus unnecessarily expensive. Just run these functions once at the 
	beginning and store the user's home dir into a dynamic array.
 * (UNNEEDED) It could be a good idea to add a puts call in every function to 
	track when, and how many times functions are called. Just use gprof to 
	profile the program! It does exactly this.
 * (DONE) Let folders-first, show-hidden, and list-on-the-fly as default. Add 
	command line options to enable AND to disable these features.
 * (DONE) Rewrite the count_dir function using getdents instead of scandir or 
	readdir. Getdents is 4 times faster than scandir and almost 2 times faster 
	than readdir. However, It FAILS when trying to access a shared mount point. 
	Going back to the readdir version.
 * (DONE) According to gprof, the most used function is by far 
	alphasort_insensitive. So, this is a perfect place for code optimization. 
	Replace tolower() by some custom code and rewrite the loops to make them 
	decrease instead of increase.
 * (DONE) Add an explanation of the history command (!) to help.
 * (DONE) Make the 'back' function remember more paths. It only goes back once. 
	'old_pwd' (directory history recollection) is a FIXED array: it always 
	takes 4096 bytes!, even if the current value takes only 20 bytes. Even 
	more, it should be an ARRAY of strings containing ALL visited directories 
	(and not just the last one). I added some subfunctions (like clear, hist 
	and ELN) to the main 'back' function. I added this function to the 
	bookmarks function as well.
 * (DONE) Find a way to conveniently handle realloc and calloc errors (if 
	possible, not exiting the program). I wrote custom functions to do this: 
	xcalloc() and xrealloc(), replacing every call to malloc/calloc and 
	realloc by xcalloc and xrealloc respectivelly.
 * (DONE) Get rid of the comm_for_matches function and reform the logic of the 
	search function. When doing a quick search, simply filter the current array 
	of elements and display the matching ones, if any. In this way, I don't 
	need to do anything special to operate on those elements, and the 
	comm_for_matches function would be just obsolete.
 * (DONE) Use the glob function to select files when using wildcards.
 * (DONE) Add ranges to the selection function.
 * (DONE) log_function should log filenames instead of ELN whenever an ELN is 
	part of the cmd. 
 * (DONE) Add an explanation of profile and prompt commands to help.
 * (DONE) Allow the user to define permanent custom variables in the profile 
	file.
 * (DONE) Allow the user to create some custom variables. Ex: 
	'dir="/home/user"'; 'cd $dir'. $var should by expanded by parse_input(), 
	just as I did with '~/'. I should use an array of structs to store the 
	variable name and its value. 
 * (DONE) Save prompt commands into RAM, just like I did with aliases. See 
	get_aliases()
 * (DONE) Add a profile file to allow the user to run custom commands when 
	starting CliFM or before the prompt.
 * (DONE) Do not allow 'cd' to open files.
 * (DONE) Add some option to allow the user to choose to run external commands 
	either via system() or via exec(). This can be done now in three different 
	ways: 1) config file; 2) beginning the cmd by a semicolon 
	(ex: ;pacman -Rns $(pacman -Qqtd)); 3) the 'sys' cmd.
 * (DONE) Replace allowed_cmd (in exec_cmd) by a simple return.
 * (DONE) Every input argument takes 4096 bytes, no matter if it has only 1 
	byte. SOLUTION: First dump arguments into a buffer big enough, say 
	PATH_MAX, and just then copy the content of this buffer into an array 
	(comm_array), allocating only what is necessary to store that buffer.
 * (DONE) Replace the if...else chain in colors_list by a switch. Remove the 
	two last arguments.
 * Take a look at str(r)char(), which seems to do the same thing as my 
	strcntchar(). NO: both strchar and strrchar return a pointer to the 
	specified char in string, but not its index number. I could use them 
	nonetheless whenever I don't need this index.
 * (DONE) Use getopt_long function to parse external arguments.
 * (DONE) Update colors in properties_function and colors_list.
 * (DONE) Add color for 'file with capability' (???) (\x1b[0;30;41). Some 
	files with capabilities: /bin/{rsh,rlogin,rcp,ping}
 * (DONE) Add different color (green bg, like 'ls') for sticky directories.
 * (DONE) Replace all calls to calloc and realloc by my custom functions 
	XCALLOC and XREALLOC. In case of (re)allocation failure, my functions quit 
	the program gracefully, unlike calloc and and realloc, which make the 
	program crash. Make sure to replace as well the argument of sizeof by the 
	type of data (char * for calloc and char ** for realloc).
 * (DONE) Add brace expansion.
 * (DONE) Replace fixed paths (/bin/{mv, cp, rm}) in copy_function by 
	get_cmd_path
 * (DONE) ls should act as refresh only when cd doesn't list automatically.
 * (DONE) Remove 'Allow sudo' & allowed_cmd: doesn't make sense anymore.
 * (DONE) Check that 'du' exists in dir_size(). Same with 'cp' and 'mv' in 
	copy_function().
 * (DONE) Allow prompt color customization.
 * (DONE) Add the possibility not to list folders content automatically when 
	running cd, just as in any shell.
 * (DONE) Add ranges to the selection function.
 * (DONE) Do not limit the maximum number of input arguments (max_args=100). 
	Use realloc instead in parse_input_string() to allocate only as many 
	arguments as entered;
 * (DONE) Remove strlen from loops. in 'i=0;i<strlen(str);i++' strlen is 
	called as many times as chars contained in str. To avoid this waste of 
	resources, write the loop as this: size_t str_len=strlen(str); 
	for (i=0;i<str_len;i++). In this way, strlen is called just once. Another 
	possibility: size_t str_len; for (i=0; str_len=strlen(str);i++)
	See: http://www.cprogramming.com/tips/tip/dont-use-strlen-in-a-loop-condition
 * (DONE) Replace get_linkref() by realpath().
 * (DONE) Add command names to listed jobs.
 * (DONE) Add internal commands (ex: open, sel, history, etc) to the 
	autocomplete list.
 * (DONE) Enhace the history function by allowing the user to choose and 
	execute some history line. Ex: if 'ls' is the 3rd line in history, the 
	user could type '!3' to run the 'ls' command.
 * (DONE) Change the way of listing files in list_dir(): use the d_type field 
	of the dirent struct to find out the type of the listed file instead of 
	running a separate command (lstat). By doing this I save one or two 
	commands for EACH listed file, which is a lot.
 * (DONE) Rewrite the logic of the program main loop: do an infinite loop 
	(while (1)) in main() containing the three basic steps of a shell: 
	1 - prompt and input; 2 - parse input; 3 - execute command.
 * (DONE) The exec family functions do not do globbing: something like 'rm *' 
	won't work. Use the glob() function defined in glob.h.
 * (DONE) - When running a command with sudo (or a backgrounded command) and 
	then Ctrl+c I get two prompts in the same line. SOLUTION: reenable signals 
	for external commands. Otherwise, you'll get a new prompt but the command 
	will be left running.
 * (DONE) Replace foreground running processes (wait(NULL)) by 
	run_in_foreground().
 * (DONE) Add commands autocompletion, not just filenames. SOLUTION: wrote a 
	custom autocomplete function for readline.
 * (DONE) Get environment variables, especially PATH to run external commands. 
	PATH is also needed for commands autocompletion. 
 * (DONE) Add an alias function. Use bash syntax and append aliases to clifmrc.
 * (DONE) Change list color for suid files (red bg, white fg) and sgid files 
	(bold_yellow bg, black fg)
 * Check sprintf and strcpy calls for buffer overflow. Replace by snprintf and 
	strncpy (also strcat by strncat).
 * (NO) Get rid of calls to system() (for "rm -r", "cp") and replace them by 
	pure C code. I've already replaced 'ls -hl' (plus some info from 'stat' 
	command) and 'clear' by my own code. On cp, take a look at: 
	http://linuxandc.com/program-implement-linux-cp-copy-command/
	DO NOT replace these commands; it's much safer to call them than to 
	(unnecessarily) re-write them. The latter would have only educational 
	purposes.
 * (UNNEEDED) Replace rl_no_hist by: 
	answer=getchar(); while ((getchar() != '\n')) {}
	This, however, is not necessary at all.
 * (DONE) Allow Bash aliases. SOLUTION: implement a custom aliases system.
 * (DONE) Cppcheck gives error on realloc. An explanation could be: "It's a 
	bad idea to assign the results of realloc to the same old pointer, because 
	if the reallocation fails, 0 will be assigned to p and you will lose your 
	pointer to the old memory, which will in turn leak." 
	(http://stackoverflow.com/questions/11974719/realloc-error-in-c) Solution 
	in this same site. See also: 
	http://www.mantidproject.org/How_to_fix_CppCheck_Errors
 * (DONE) Replace malloc by calloc, since this latter, unlike malloc, 
    automatically initialize all of the allocated blocks to zero, so that 
	there's no need for memset. It seems that calloc is better than 
	malloc/memset in that it is at least twice faster (or 100x faster!) and 
	uses less memory. 
    See: https://vorpus.org/blog/why-does-calloc-exist/
 * (DONE) Commands fail when file contains single quotes (') SOLUTION: replace 
    single by double quotes when passing filenames as arguments. However, if 
    the file contains double quotes in its name, it will fail anyway. I should 
    test all files for single and double: if contains single, use double, if 
    double, use single instead. What if it contains both, single and double??!!
    Check only in case of ELN's in cp and rm (both in case of sel or not)
 * (DONE) Find a way to make alphasort() in scandir() non-case-sensitive. 
    Solution: write a custom alphasort function using tolower().
 * (DONE) Allow the user to choose whether he/she wants to clear the screen 
	after every screen change/refresh or not. Added to the configuration file.
 * (DONE) Smoke testing: minimal attempts to operate the software, designed to 
    determine whether there are any basic problems that will prevent it from 
    working at all.
 * (DONE) Limit the log file size (in line numbers), say, 1000 by default. 
	Allow change it in the config file.
 * (DONE) Add better sync between different instances of the program regarding 
    selected files. SOLUTION: call get_sel_files() from the prompt 
    function, so that sel files will be updated everytime a new prompt is 
    called, even if the modification was made in a different instance of the 
    program. The trick here is that selected files are stored, via the 
    save_sel function, in ram (a file in /tmp), so that this data is 
    independent from the private variables of different instances of the 
    program, which take their value from the tmp file. NOTE: I'm getting an 
    error in Valgrind coming from get_sel_files() every time I call 
    it from the prompt function. Fixed by freeing the sel_elements array at 
    the beginning of get_sel_files().
 * 20 - When running the 'pr' command, I get an error from Valgrind whenever 
	the GID of a file is UNKNOWN. Do "pr ~/scripts/test/file_bk". Problematic 
	line (3621): group = getgrgid(group_id); It seems to be a library 
	(/usr/lib/libc-2.25.so) error, not mine.

*/
/**
							################
							#  KNOWN BUGS  #
							################
*/

/*
 ** 1 - Piping doesn't work as it should. Try this command: 
    'pacman -Sii systemd systemd-libs | grep "Required By"'. Now run the same 
    command outside CliFM. The output in CliFM is reduced to only the first 
    line of each "Required by" line. This makes running shell commands on
    CliFM unreliable.
 ** 2 - Cannot use slash in bookmark shortcuts or names (whatever from slash 
	onwards is taken as the bookmark's path, which in this case is wrong).
 ** 3 - When TAB completing bookmarks, if there is a file named as one of the
	possible bookmark names in the CWD, this bookmark name will be printed in
	the color corresponding to the filetype of the file in the CWD.

###########################################
 * (SOLVED) When searching for files not in CWD (/string dir), we get a 
	corrupted ELN (it prints garbage). I just needed to set the 'index' string 
	to NULL in colors_list() whenever no ELN was found.
 * (SOLVED) In FreeBSD, the line "XTerm*eightBitInput" is always added to
	.Xresources file, no matter what. SOLUTION: Rewind the file to the beginning
	before doing any check.
 * (SOLVED) chdir() succees if the user has at least execute permission on the
	directory. However, since CliFM wants to list files in dir as well, we also 
	need read permission. Add this check to cd_function()!!
 * (SOLVED) Path completion when double dot is involved stopped working (since
	version 0.17.2). Ex: 'c file ../..[TAB]'
 * (SOLVED) The no list folders first code is broken.
 * (SOLVED) Trash is not working in Free-BSD: its 'cp' implementation does not 
	allow the use of -r and -a simultaneously. So, replace -ra by -a if using 
	Free-BSD. NOTE: -ra is redundant; -a already implies -R, which is the same
	as -r. So, just use -a for both Linux and Free-BSD.
 * (SOLVED) Some files are displaying the wrong birthtime. Whenever there is
	no birthtime, statx returns 0, which strftime() translates to dec 31,
	1969. SOLUTION: Check birthtime for zero.
 * (SOLVED) Filenames background color continues to the end of line in search 
	and properties functions. The problem was that colors_list inserted the pad 
	before the filename, when it should be inserted after it.
 * (SOLVED) The exec check in list_dir checks the executable bit only for the
	owner of the file, and not for group and others. Change S_IXUSR by 
	(S_IXUSR|S_IXGRP|S_IXOTH).
 * (SOLVED) History does not execute aliases.
 * (SOLVED) I'm having Valgrind errors when listing unicode filenames. SOLUTION:
	Whenever finale names have at least one unicode char (in which case cont_bt
	is true), use char32_t (4 bytes) instead of simply char (1 byte) to allocate 
	memory to store the filename (in list_dir).
 * (SOLVED) Enter this: "p /etc/pacman.conf" and then "cd". The user_home 
	variable is now 'in/bash' and the user is root!!! SOLUTION: The pointer
	returned by get_user(), which makes use of getpwuid() is overriden by the
	properties function, for it uses getpwuid as well. So, make get_user() 
	return a string, and not a pointer.
 * (SOLVED) Ctrl-c kills backgrounded jobs. SOLUTION: Use launch_execle() for
	the open function and let the system shell handle backgrounded jobs.
 * (SOLVED) The program won't compile if statx is not found, that is, whenever
	the linux kernel is not 4.11 or greater and glibc version is not 2.28 or
	greater. SOLUTION: Use some #define to check both kernel and glib versions,
	and use stat() instead of statx() whenever the conditions are not met.
 * (SOLVED) Error when opening '/dev/fd'. It's a symlink to a directory. The 
	dir gets opened, but I get an error when listing one of its files: a 
	broken link named '4'. The error comes from list_dir(), from the longest 
	filename check. Here lstat() returns -1 complaining that the file 
	doesn't exist, but it is there nonetheless. SOLUTION: Skip "non-existent"
	files both in the longest filename check and when listing files. 
	NOTE: I'm still not sure why scandir() shows this file but lstat() says 
	it doesn't exist.
 * (SOLVED) TAB completion still not working with cd and symlinks.
 * (SOLVED) "cd [TAB]" shows only dirs, but "cd .[TAB]" shows all files.
 * (SOLVED) TAB path completion not working with local executables: "./file" 
 * (SOLVED) Select some files, say 5. Then pass this to the deselect 
	function: '1-3 1 1'. All the 5 files are deselected, and it shouldn't. 
	SOLUTION: Remove duplicates directly from get_substr().
 * (SOLVED) If the user's home cannot be found, CliFM will attempt to create 
	its config files in CWD/clifm_emergency. Now, if CWD is not writable, no 
	option will be set, and CliFM will become very unstable. SOLUTION: In 
	this case, just prevents the program from writing or reading anything and 
	simply set the defaults.
 * (SOLVED) The atexit() function does not actually exits the program, but 
	just registers a function to be called at normal process termination, 
	either calling exit() or via return from program's main(). Call atexit() 
	once in main and replace remaining calls for a simple exit(code). 
	Otherwise, if using atexit() instead of exit(), the program won't 
	terminate!!
 * 	(SOLVED) I cannot create this file: "abc{1,2,3}". When I type 
	"touch abc\{1\,2\,3\}", everything is dequoted by split_str(), 
	interpreted as a valid braces expression by parse_input_str(), and then
	send to the touch command, who creates three files: abc1, abc2, and abc3.
	The problem is step two: parse_input_str() should not take the string
	as a braces expression. The same happens with, for example: "touch
	\*". NOTE: I wrote a very ugly workaround that partially works (check
	the dequoted global variable). FIX!!! SOLUTION: Got rid of the distinction
	between internal and external commands: parse_input_str() is not
	dequoting strings anymore; it's always done now by the system shell,
	except for functions like sel and prop where the shell is not involved:
	I dequote strings here, but locally, in the function itself. 
 * (SOLVED) Whenever I use the system shell to run commands (execle()), this 
	shell tries to deescape and dequote what was already deescaped and 
	dequoted by CliFM itself. So, for instance, if the original	string was 
	"'user\\123'", CliFM sends it to the system shell already dequoted and 
	deescaped: "user\123". It works perfect for execve(), because it does not 
	call any shell, but just executes the command with the given parameters 
	literally. However, launch_execle() calls the system shell, which tries 
	to dequote and deescape the string again, resuting in "user123", which is 
	a non-existent filename. If I want, and I want, to keep some features 
	like pipes and stream redirection, I need the system shell. But if I use 
	the system shell, the quoting and escaping thing gets broken. 
	SOLUTION: Keep launch_execle() for all external commands; add a check
	for external commands in parse_input_str(), and, if the command is 
	external, tell split_str() not to dequote anything (let the system shell
	do it). In case of ELN's and sel keyword, in which case filenames won't
	be quoted, always check if the command to be run is external, and in that
	case, return the quoted filename.
 * (SOLVED) When launch_execve() tries to run a non-existent command, it 
	displays the error message fine, but after that I have to exit CliFM 
	one more time per failed command. SOLUTION: After running exec() via 
	fork() use exit() and not return in case of error.
 * (SOLVED) Readline TAB completion stops working when it gets to a quoted 
	directory name, for example: "/home/user/dir\ name/". After this point,
	there's no more TAB completion. SOLUTION: Wrote two functions: one for
	rl_char_is_quoted_p (to tell readline that "dir\ name" is just one name), 
	and another one for rl_filename_dequoting_function (to remove the 
	backslash from the filename, so that it can be correctly compared to
	available system filenames).
 * (SOLVED) When TAB expanding the same ELN more than once, and if that ELN
	corresponds to a directory, one slash is added to the expanded string
	each time, resulting in something like: "path////". Of course, this 
	shouldn't happen.
 * (SOLVED) GETT RID OF MAX_LINE COMPLETELY!! It doesn't exist and could 
	only break things. No more MAX_LINE anymore!
 * (SOLVED) I'm using MAX_LINE (255) length for getting lines from files with 
	fgets(). A log line, for example, could perfectly contain a PATH_MAX (4096) 
	path, plus misc stuff, so that MAX_LINE is clearly not enough. I should 
	try the GNU getline() instead, which takes care itself of allocating 
	enough bytes to hold the entire line. Done. I keep fgets only when I known 
	which is the max valid length for each line of the file.
 * (SOLVED) The log gets screwed whenever it is reconstructed deleting oldest
	entries. SOLUTION: Use getline to get log lines.
 * (SOLVED) Log lines are truncated. Problem: The function to get the 
	command's full length was wrong.
 * (SOLVED) TMP_DIR will be created by the first user who launched the
	program, so that remaining users won't be able to write in here, that is
	to say, they won't be able to select any file, since TMP_DIR is not
	accessible. SOLUTION: Create TMP_DIR with 1777 permissions, that is, 
	world writable and with the sticky bit set.
 * (SOLVED) Enter this: '[[ $(whoami) ]] && echo Yes || echo No', and the
	program crashes. The problem was that parse_input_string() was taken '[' 
	as part of a glob pattern. SOLUTION: Simply add a new check to 
	parse_input_string() to avoid taking '[' followed by a space as a glob 
	pattern. 
 * (SOLVED) When wrongly removing selected directories ("r sel" instead of
	"r -r sel"), nothing is removed, but files are deselected anyway.
	SOLUTION: Simply check rm exit status before deselecting files.
 * (SOLVED) If a path in PATH does not exist, the program will crash when
	attempting to autocomplete commands.
 * (SOLVED) Run a really long command, say 'chmod +x 1-150', and Bang! Stack 
	smash! Problem: The buffer for the log function was not big enough.
 * (SOLVED) Error when getting total size for directory names containing 
	spaces in get_properties() function. SOLUTION: Quote the filename!!
 * (SOLVED) Non 7bit-ASCII filenames are not correctly sorted when listing 
	files. SOLUTION: When unicode is set true do not use xalphasort() nor
	alphasort_insensitive(), since they use strcmp() and strcasecmp(), being
	neither of them locale aware; use alphasort() instead, which uses 
	strcoll().
 * (SOLVED) The glob, range, and braces expansion functions expand only up to 
	10 non-expanded strings: Beyond this, expect a crash. SOLUTION: Though the 
	ideal solution would be to make all these arrays dynamic, I just added a
	check to add values only up to 10.
 * (SOLVED) Go to the root directory (/) and then to any directory in there. 
	The resulting path is "//dir".
 * (SOLVED) Memory leak when deselecting several files at once.
 * (SOLVED) Check undel and deselect functions: Wrong output and errors when 
	input string is empty or contains only spaces. Rewrite the whole user 
	input block.
 * (SOLVED) Possible null pointer dereference (path_tmp) in get_path_env(). 
	Same in prompt() with path_tilde and short_path (using cppcheck).
 * (SOLVED) In the bookmarks prompt enter only spaces: CRASH!!
 * (SOLVED) "Not a directory error" when listing directories contining a socket
	file. SOLUTION: "file_attrib.st_mode & S_IFDIR" takes sockets as dirs. Use
	"(file_attrib.st_mode & S_IFMT) == S_IFDIR" instead whenever it is needed 
	to check if a file is a directory.
 * (SOLVED) When listing files by size ("pr size"), only directories and 
	regular files are printed according to their color codes: everything else
	is just white, that is, wrongly printed as regular files. Modify the
	colors_list() function to make it fit properties_function(), adding to it
	arguemnts for padding and new line. 
 * (SOLVED) Wrong formatting when listing filenames containing non-7-bit 
	ASCII chars (both in normal and long view mode). This is because printf() 
	does not correctly count chars in this kind of strings.
 * (SOLVED) When trashing a file, if the trash file name length is at least
	NAME_MAX-suffix+1, adding the suffix to it will produce a filename 
	longer than NAME_MAX (255).
 * (SOLVED) Run 'cd /dev/sda1' and CRASH!!
 * (SOLVED) If not cd_list_on_the_fly and the user runs 'open dir', the 
	contents of DIR are listed on the fly anyway.
 * (SOLVED) Edit the config file while in the program and change the value of
	Starting path: list_dir(), called by init_config(), fails miserabily. 
	SOLUTION: chdir to new path before calling list_dir().
 * (SOLVED) Memory leak at the end of get_properties(): memory allocated by
	get_user() is reallocated without previously calling free().
 * (SOLVED) Keybindings do not work the same for different terminal emulators.
	Ctrl-arrow keys do not work at all for aterm, while they work
	for xterm-like terminal emulators depending on the readline configuration
	(~/.inputrc). I should find a way to do this disregarding the terminal
	emulator in use. SOLUTION: Do not use arrow keys or Control key for 
	keybindings; use instead some vi-like ones like A-{u,h,j,k}, etc.
	In this way, my keybindings work for the Linux built-in terminal, xvt
	terminal emulators like aterm and urxvt, and xterm-like terminals.
 * (SOLVED) Launch the Bookmarks Manager and just press Enter at the prompt:
	CRASH!! Solution: check for empty input in bm_prompt().
 * (SOLVED) When running 'history clean' the history list is only actually
	cleaned after a restart, and not inmmediately after the command. SOLUTION:
	added a check to get_history() to free the history array if there are
	elements in the history list.
 * (SOLVED) Memory error with Valgrind when running a history command with 
	more than 1 argument. SOLUTION: Backup the args_n variable and restore it
	after running the history command. Otherwise, the comm array, which 
	depends on args_n, won't be correctly freed.
 * (SOLVED) When using the 'b/f !ELN' command the CWD is not updated in the
	history list: it still shows the previous path as current (highlighted 
	in green).
 * (SOLVED) If the user tries to trash some directory in the TRASH_DIR path, 
	say, /, /home, ~/, ~/.local or ~/.local/share, and if he/she has 
	appropriate permissions, the trashed directory will be lost! Why? Suppose
	the user tries to trash its home dir: the home dir will be copied into the
	trash directory, then the home dir will be removed, and, because the trash
	dir is in the home dir, the trash dir will be also removed, and thus the
	entire home dir is lost. PREVENT THE USER FROM TRASHING ANY OF THESE 
	DIRECTORIES!!
 * (SOLVED) Cannot trash files (passed as absolute path) whose parent dir
	is root (/). SOLUTION: strbfrlst() returns NULL, when checking file's 
	parent, whenever FILE is "/file", simply because there's nothing before
	the first slash. In this case, manually check whether FILE's parent is the 
	root directory, and if true, set parent to "/".
 * (SOLVED) Failure undeleting files with spaces. The Path line for trashinfo 
	files must be construed as in URL's. Example: path/to/file%20with%20spaces. 
	See the freedesktop specification.
 * (SOLVED) Cannot trash files containing spaces. Solution: quote strings 
	passed as arguments to 'cp' and 'rm' commands. Ex: cp -ra '%s' ...
 * (SOLVED) Commands run via run_and_refresh() do not recognize quoted strings,
	that is, strings containing spaces. Check every substring in 
	run_and_refresh() and quote it if it contains spaces.
 * (SOLVED) I use asprintf to allocate memory for error messages (err_msg). If 
	this allocation fails (if (!err_msg)) the error message won't be printed.
 * (SOLVED) Beyond PATH_MAX length, external commands will fail to execute. 
	Solution: dinamically increase the size of the ext_cmd array in exec_cmd()
	instead of using a fixed size (PATH_MAX).
 * (SOLVED) Cannot trash symlinks with the 'tr sel' command. Solution: changed
	stat() by lstat() in trash_element().
 * (SOLVED) If get_user_home() fails (in main()), list_dir() will crash. 
	Solution: If get_user_home() fails, set home as 'cwd/clifm_emergency' (all
	config dirs and files will be created here), and if getcwd() also fails, 
	exit. Currently, if no user home is found, no config file is created and
	all options are set to default.
 * (SOLVED) The 'b ELN' function is broken because of automatic ELN expansion. 
	ELN does not refer here to files listed on the screen, but to visited 
	directories. Solution: modified surf_hist() to use "!ELN" instead of just 
	"ELN", so that the parameter won't be expanded by parse_input_str().
 * (SOLVED) Valgrind complains about a write error when doing 'cd scripts' 
	(symlink), or directly cd /symlink/reference. The problem was in list_dir().
	I was not reserving enough space when callocing filenames. I changed 
	str_len+1 to str_len+2. This bug is already present in older versions of 
	the program, at least from 0.9.4-12b onwards.
 * (SOLVED) If a file is manually removed from the trash dir, the trash 
	indicator won't be updated until either 'tr' or 'undel' are run. To solve 
	this check for trashed files in the prompt itself. The same happens to 
	the sel files indicator: if the selection box is modified from another 
	CliFM instance, the indicator wont be updated till a screen refresh (rf).
 * (SOLVED) Error when trying to trash a write-protected non-empty directory.
	Solution: the permissions check function was not checking subdir itself,
	but only parent and subdir contents (that is, level 0 and 2, but not 1).
 * (SOLVED) Write an empty alias (alias test=''), run the alias and CRASH!!
 * (SOLVED) Error in Valgrind when trashing selected files, and more 
	specifically in the save_sel() function, anytime some file fails to be 
	trashed. The problem was that I was not properly emptying the array of 
	selected elements when trashing.
 * (SOLVED) BACKUP doesn't work as it should when files are removed using 
	wildcards. This isn't a problem anymore since wildcards are automatically 
	expanded before reaching any function. Backup was replaced by trash anyway.
 * (SOLVED) Since backed up files are copied into /tmp, whenever big files, or
	a great amount of files are removed, the capacity of /tmp could be easily 
	exceeded producing thereby errors, like running out of memory! Backup was 
	replaced by trash, which does not copy files into /tpm, but into the local
	filesystem: if there is not enough remaining space, 'cp' will tell.
 * (SOLVED) After displaying a command help (cp, mv, rm, touch, mkdir) listdir is
	reloaded, and it shouldn't.
 * (SOLVED) Run 'rm --help' and CRASH!!
 * (SOLVED) Path highlighting in the back hist function fails when path starts 
	with tilde (~).
 * (SOLVED) The program crashes whenever the CWD doesn't exist anymore. The 
	problem was that the files counter was set to -1 in case of a non-existing 
	CWD. Setting it to 0 at the beginning of list_dir() solved the problem.
 * (SOLVED) If $HOME/.config doesn't exist, expect a crash. The problem is 
	this: mkdir() doesn't create parent directories. Use the GNU mkdir instead.
 * (SOLVED) Since the last versions, run_in_the_foreground() makes the program 
    exit. The problem was tcsetpgrp().
 * (SOLVED) If rm makes a question regarding the file to be removed, this 
	question won't be seen on the screen (I guess the same happens with cp and 
	mv, at least). The problem is check_cmd_stderr(). The solution was to write 
	an exec function (launch_execle()), able to handle errors.
 * (SOLVED) Do "cd path/to/dir/\*" and CRASH!
 * (SOLVED) Specify a file (not a dir) as destiny of the 'paste sel' command, 
	and CRASH!!! 
 * (SOLVED) This command: "sel /usr/local/sbin/\*" doesn't work, for it produces 
   these wrong paths: "/home/leo//usr/local/sbin/{filenames}". It only happens
   when using the wildcard.
 * (UNNEDED) 'sel *' doesn't select hidden files. This is not a bug, but the 
	default behaviour of Unix shells. To select everything, simply do a 'sel *' 
	followed by a 'sel .*'
 * (SOLVED) 'sel .*' selects "." and ".." besides hidden files. DANGEROUS!!
 * (SOLVED) Every time the screen is refreshed by some keyboard shortcut (C-f, 
	C-h, or C-r), the first entries of the files list appear in the prompt 
	line. Solution: add a puts("") before calling list_dir().
 * (SOLVED) Select some files, go to the deselect screen and then type "*". 
	The program crashes. Problem: my xatoi() returns decimal values for 
	alphabetical chars. Solution: add a conditional statement to check for 
	these chars.
 * (SOLVED) Enter "slaby", press Enter, and bang!, the program crashes. Problem 
	was: I'm not sure. Perhaps it only happens when using the CLiFM shell.
 * (SOLVED) When not listing files automatically, the bookmarks function 
	doesn't update the prompt. Solved by simply adding a 'chdir(path)' in case 
	of no list-on-the-fly.
 * (SOLVED) There are duplicated highlighted lines in the back hist function 
	whenever there are two or more lines pointing to the same path. Problem: 
	wrong condition for highlighting lines.
 * (SOLVED) When running as root, the program cannot be called as argument for 
	a terminal. See the setpgid lines in init_shell(). I'm not sure why, but 
	this doesn't happen anymore.
 * (SOLVED) Wrongly listing non-empty files as empty. Check /proc/pid/cmdline. 
	Stat's st_size is not accurate enough. Not my problem; the buil-in 'stat' 
	command has the same issue, if it's an issue at all.
 * (SOLVED) Errors in Valgrind when: 1 - search for a pattern; 2 - select an 
	element; 3 - search for the first pattern again. I guess this bug was 
	fixed when the search function was rewritten.
 * (SOLVED) Weird: when opening ~/.xinitrc as 'o(pen) ELN/filename' it doesn't 
	remain in the foreground. It only happens with THIS file. Not a problem of 
	my program, it happens as well in other terminals. It seems to be a 
	problem of xdg-open: it doesn't happen when using leafpad for example.
 * (SOLVED) The log_function is logging profile and prompt commands instead of 
	just external commands. SOLUTION: add a 'no_log' flag to tell exec_cmd() 
	not to log cmds when they come from the profile or prompt functions.
 * (SOLVED) 'ls' stopped working. Problem: the '=' case of the switch 
	statement in parse_input_str(). The condition to evaluate user variables 
	should be more restricted. SOLUTION: check for spaces before '=' in order 
	to avoid taking as variables things like: 'ls -color=auto'.
 * (SOLVED) Search for a file (/x) and then enter this: 
	'cp 1 ~/scripts/test/empty_folder/': ERRORS!!!
 * (SOLVED) Errors in folder_select and file_select when trying to access to 
	a failed mountpoint. SOLUTION: add an existence check at the beginning of 
	theses functions.
 * (SOLVED) When running pacman via system() this error came up from within 
	pacman itself: "call to waitpid failed". SOLUTION: do not run system() by 
	itself, but execle("/bin/sh"...), which is what system() basically does. 
 * (SOLVED) 'ls -a | grep file > stream' doesn't work.
 * (SOLVED) 'ls | grep scripts && mirage' fails and produces an error.
 * (SOLVED) Concatenated commands don't take background operator (&).
 * (SOLVED) Take a look at the end of dir_list(): there is no difference 
	between >1.000 and >10.000
 * (SOLVED) get_sel_files() is being executed with every new prompt. SOLUTION: 
	move the function to refresh().
 * (SOLVED) Error: ~/scripts/test $ lsblk -i > empty_folder/stream
 * (SOLVED) 'path' variable is a FIXED array: it always takes 4096 bytes!, 
	even if the current value takes only 20 bytes.
 * (SOLVED) When running an alias quotes don't work. SOLUTION: first parse the 
	aliased command (with parse_input_str) and then add entered parameters, if 
	any, to the parsed aliased command.
 * (SOLVED) Aliases work only for external commands. SOLUTION: relocate 
	run_alias_cmd (renamed now as check_for_alias) in the main loop of main, 
	before exec_cmd, and remove it from this latter. Inside check_for_alias, 
	call exec_cmd instead of execve().
 * (SOLVED) Sometimes the number of files contained in a directory is wrongly 
	shown. Ex: instead of usb (empty) I get 'usb /-2'!!
 * (SOLVED) xdg-open is not installed by default in every Linux system. Fix it. 
	Perhaps, xdg-open should be installed as a requirement of CliFM. SOLUTION: 
	Added a check for xdg-open at startup, so that it won't be called if it's 
	not installed. xdg-open is not used anymore, but the same thing goes now
	for the 'file' command, used by my mime function.
 * (SOLVED) When it comes to optical drives (CD-ROM) files color is always 
	default, i.e., its file type isn't recognized. It doesn't happen with USB 
	drives though. The problem seems to be scandir's st_type. the stat function 
	instead doesn't have this problem. So, whenever st_type is DT_UNKNOWN 
	(default case of the switch statement in list_dir), call a modified version 
	of the colors_list function (which works with stat) and tell it whether it 
	has to add a newline character at the end or not by means of a new boolean 
	argument: newline
 * (SOLVED) 'cd ~/' doesn't work as it should. Instead of changing the path to 
	the home dir, it says: 'No such file or directory'.
 * (SOLVED) If you edit the config file, options will be reloaded according to 
	this file, overwritting any external paramater passed to the program. 
	SOLUTION: copy the values of argc and argv to rerun external_arguments 
	everytime the user modifies the config file.
 * (SOLVED) Sometimes columns are wrong (see ~/.thumbnails/normal). It would 
	be nice to produce a better columned output in general, just like the 'ls' 
	command does.
 * (SOLVED) When running a globbing command (ex: leafpad world*) cannot 
	SIG_INT.
 * (SOLVED) When I type 'cd/bin' (with no spaces) a wrong message is shown: 
	"CliFM: cd/bin: Permission denied".
 * (SOLVED) Error when entering incomplete quotation marks (ex: "test, or 
	'test).
 * (SOLVED) Error when entering a directory as first input word (ex: '/').
 * (SOLVED) Error when typing '     |', or any weird char like 
	{,  (in parse_input_string)
 * (SOLVED) This: '!"#%#$&/%&(&/(%#$' produces the same error. Simply type any 
	shit and watch caos go! This: '(/' says 'Permision denied' (from external 
	command)
 * (SOLVED) Nested double dots (ex: cd ../..) are read as a simple double dot 
	(cd ..)
 * (SOLVED) The init_shell function prevents CliFM from being called as 
	argument for a terminal, ex: 'xterm -e /path/to/clifm'. It can be executed 
	though in a running terminal by typing the corresponding command: 
	'$ clifm'. However, if I remove init_shell, other problems come out. 
	SOLUTION: remove the setpgid lines from init_shell().
 * (SOLVED) Fails when running interactive terminal programs. SOLUTION: use 
	tcsetpgrp to allow the executed program to control the terminal. Then give 
	control back to the main program. PROBLEM: Now I can't send a SIGINT to 
	the process. SOLUTION: set SIG_INT to default action (SIG_DFL) before 
	executing the program, and set it to ignore (SIG_IGN) after the program 
	has been executed (both in the child).
 * (SOLVED) Aliases don't work when called with wildcards.
 * (SOLVED) When a file is opened via xdg-open (no application specified) in 
	the foreground, a SIG_INT signal (Ctrl-c) will not kill the opening 
	application, but rather xdg-open only. SOLUTION: when executing the 
	process, make its pid equal to its pgid (setpgid(0, 0)), so that kill, 
	called with a negative pid: "kill(-pid, SIGTERM)", will kill every process 
	corresponding to that pgid.
 * (SOLVED) When: 1 - run background process and leave it running; 2 - run 
	foreground process; 3 - send a SIG_INT signal (Ctrl-c) to the foreground 
	process, this latter will get terminated, but the shell keeps waiting for 
	the backgound process to be terminated. SOLUTION: replace wait(NULL) by 
	waitpid(pid, NULL, 0).
 * (SOLVED) When: 1 - run process in background; 2 - kill that process; 
	3 - run a process in the foreground, this latter is sent to the background. 
	Ex: 'leafpad &'; close leafpad; 'leafpad'. Solution: use the init_shell 
	function provided by the GNU. See:
	https://www.gnu.org/software/libc/manual/html_node/Initializing-the-Shell.html#Initializing-the-Shell
 * (SOLVED) Valgrind error in color_list() when listing or searching in 
	/usr/bin or some large directory, like /usr/lib. It only happens on the 
	first search. SOLUTION: memory reserved for allocated variables in 
	search_function was not big enough. Added 1.
 * (UNNEEDED) Readline has some minor memory leaks. It think it's the function 
    itself. Look for an alternative or some way to fix it. However, all these 
    readline leaks ("still reachable" according to what Valgrind says) are 
    "not real leaks," and therefore not dangerous at all, as "real leaks" are. 
    See: 
    http://www.w3lc.com/2012/02/analysis-of-valgrind-still-reachable.html
	Furthermore, this is not really a bug, not even a leak of MY programm, but 
	at most an issue of the readline library.
 * (SOLVED) When doing 'pr' in a root folder (ex: /dev/sda3) the prompt changes 
	to a root prompt!!! The problem is the getpwuid function in get_properties. 
	SOLUTION: A very dirty workaround is to run get_user() at the end of the 
	get_properties function. However, I stil don't know why nor how this 
	happens. I suspect of some memory override issue.
 * (SOLVED) Columns go bad when socket files are present (cd ~/.dropbox). 
	SOLUTION: there was an extra new line character when printing socket files 
	in list_dir().
 * (SOLVED) mv and cp dont work when either source or destiny file  contains 
	spaces, for it takes the last one as destiny and the previous one as 
	source. I should solve this before any command, that is, when parsing 
	arguments, in order not to take care of this issue with every command and 
	function. SOLVED: still need some improvement though. a) it will take 
	everything after the first quote (single or double) as one argument when 
	there is no closing quote; b) when there's a quote within an argument, it 
	will be omitted. Note: Bash admits neither incomplete quotes nor quotes 
	within strings for filenames. So, I don't have to do it either.
 * (SOLVED) Socket files are listed as if they were directories (cd /tmp). 
	SOLUTION: exclude socket files from folder_select() and include them in 
	file_select().
 * (SOLVED) Whenever you try to open a socket file ('o ELN') CliFM complains: 
	"Error listing files". After this, the program will crash on the next 
	command you run. SOLUTION: when opening directories, make sure it is not a 
	socket file by adding a negative condition: 
	!S_ISSOCKET(file_attirb.st_mode).
 * (SOLVED) When showing the properties of a socket file there is break in the 
	second line. SOLUTION: there was a new line char in "printf(Socket)" in 
	'properties_function'.
 * (SOLVED) Empty executable files (green) are shown in 'pr' function as empty 
	files (yellow). SOLUTION: It was explicitly said to print it yellow in 
	properties_function. Changed to green.
 * (SOLVED) When creating a new file, it will override without asking any 
	existing file with the same name (this is what the touch built-in command 
	do by default). However, I added a check for this case, for this could 
	lead to unintentional lost of data.
 * (SOLVED) The program may be killed from within by the kill, pkill or 
	killall commands. If this happens, malloced variables won't be freed. 
	SOLUTION: get both the invocation name and the pid of CliFM; then check 
	whether 'kill' is targeted to pid or killall (or pkill) to invocation name 
	and prevent them from running.   
 * (SOLVED) It's possible for a user to become root with the 'su' command. 
	When this happens from within the program, CliFM commands won't work 
	anymore, for the prompt will become an ordinary shell: basically, the user 
	is outside the program from within the program itself. However, the user 
	can still come back to the CliFM prompt by simply exiting the root shell. 
	SOLUTION: Not a problem. 
 * (SOLVED) The user can run any shell whithin the program, for instance, 
	'exec bash'. SOLUTION: Not a problem.
 * (SOLVED) Valgrind error when editing bookmarks.
*/

/* For the time being, CliFM hardly goes beyond the 5 Mb! Could I make it even 
 * more lightweight? Readline by itself takes 2.2Mb, that is, almost half of 
 * the program memory footprint. */

/* On C functions definitions consult its corresponding manpage (ex: man 3 
 * printf) or see:
http://pubs.opengroup.org/onlinepubs/9699919799/nframe.html */


				/** ############################### 
					#	 THE PROGRAM BEGINS		  # 
					############################### */	

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
/* The only Linux-specific function I use, and the only function requiring
 * _GNU_SOURCE, is statx(), and only to get files creation (birth) date in the
 * properties function. 
 * However, the statx() is conditionally added at compile time, so that, if
 * _BE_POSIX is defined (pass the -D_BE_POSIX option to the compiler), the 
 * program will just ommit the call to the function, being thus completely 
 * POSIX-2008 compliant. */

/* DEFAULT_SOURCE enables strcasecmp() and realpath() functions, 
and DT_DIR (and company) and S_ISVTX macros */

/* FreeBSD*/
/* Up tp now, two features are disabled in FreeBSD: file capabilities and 
 * immutable bit checks */

/* The following C libraries are located in /usr/include */
#include <stdio.h> /* (f)printf, s(n)printf, scanf, fopen, fclose, remove, 
					fgetc, fputc, perror, rename, sscanf, getline */
#include <string.h> /* str(n)cpy, str(n)cat, str(n)cmp, strlen, strstr, memset */
#include <stdlib.h> /* getenv, malloc, calloc, free, atoi, realpath, 
					EXIT_FAILURE and EXIT_SUCCESS macros */
#include <stdarg.h> /* va_list */
#include <dirent.h> /* scandir */
#include <unistd.h> /* sleep, readlink, chdir, symlink, access, exec, isatty, 
					* setpgid, getpid, getuid, gethostname, tcsetpgrp, 
					* tcgetattr, __environ, STDIN_FILENO, STDOUT_FILENO, 
					* and STDERR_FILENO macros */
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
/* Readline: This function allows the user to move back and forth with the 
 * arrow keys in the prompt. I've tried scanf, getchar, getline, fscanf, fgets, 
 * and none of them does the trick. Besides, readline provides TAB completion 
 * and history. Btw, readline is what Bash uses for its prompt */
#include <libintl.h> /* gettext */

#include <limits.h>

#if __linux__
#  include <sys/capability.h> /* cap_get_file. NOTE: This header exists
in FreeBSD, but is deprecated */
#  include <linux/limits.h> /* PATH_MAX (4096), NAME_MAX (255) macros */
#  include <linux/version.h> /* LINUX_VERSION_CODE && KERNEL_VERSION macros */
#  include <linux/fs.h> /* FS_IOC_GETFLAGS, S_IMMUTABLE_FL macros */
#elif __FreeBSD__
/*#  include <strings.h> Enables strcasecmp() */
#endif

#include <uchar.h> /* char32_t and char16_t types */
/* #include <bsd/string.h> // strlcpy, strlcat */
/* #include "clifm.h" */
/* #include <sys/types.h> */

#ifndef EXIT_SUCCESS
# define EXIT_SUCCESS 0
#endif

#ifndef EXIT_FAILURE
#  define EXIT_FAILURE 1
#endif

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
#define CLEAR puts("\x1b[c")
/* #define CLEAR write(STDOUT_FILENO, "\ec", 3) */
#define VERSION "0.21.1"
#define AUTHOR "L. Abramovich"
#define CONTACT "johndoe.arch@outlook.com"
#define DATE "November 27, 2020"

/* Define flags for program options and internal use */
/* Variable to hold all the flags (int == 4 bytes == 32 bits == 32 flags). In
 * case more flags are needed, use a long double variable (16 bytes == 128 
 * flags) and even several of these */
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

/* Colors for the prompt: */
/* \001 and \002 tell readline that color codes between them are non-printing 
 * chars. This is specially useful for the prompt, i.e., when passing color 
 * codes to readline */
#define red_b "\001\x1b[1;31m\002" /* error message indicator */
#define green_b "\001\x1b[1;32m\002" /* sel, notice message indicator */
#define yellow_b "\001\x1b[0;33m\002" /* trash, warning message indicator */
#define NC_b "\001\x1b[0m\002"
/* NOTE: Do not use \001 prefix and \002 suffix for colors list: they produce 
 * a displaying problem in lxterminal (though not in aterm and xterm). */

/* Replace some functions by my custom (faster, I think: NO!!) 
 * implementations. */
/*#define strlen xstrlen // All calls to strlen will be replaced by a call to 
xstrlen */
#define strcpy xstrcpy
#define strncpy xstrncpy 
#define strcmp xstrcmp  
#define strncmp xstrncmp
/* #define atoi xatoi */
/*#define alphasort xalphasort */
#define _(String) gettext (String)


				/* ######################
				 * #  CUSTOM FUNCTIONS  # 
				 * ######################*/

/* They all work independently of this program and its variables */

int
check_immutable_bit(char *file)
/* Check a file's immutable bit. Returns 1 if true, zero if false, and
 * -1 in case of error */
{
	#if !defined(FS_IOC_GETFLAGS) || !defined(FS_IMMUTABLE_FL)
	return 0;
	#else

	int attr, fd, immut_flag = -1;

	fd = open(file, O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "'%s': %s\n", file, strerror(errno));
		return -1;
	}

	ioctl(fd, FS_IOC_GETFLAGS, &attr);
	if (attr & FS_IMMUTABLE_FL)
		immut_flag = 1;
	else
		immut_flag = 0;
	close(fd);

	if (immut_flag) 
		return 1;
	else 
		return 0;

	#endif /* !defined(FS_IOC_GETFLAGS) || !defined(FS_IMMUTABLE_FL) */
}

int
xgetchar(void)
/* Unlike getchar(), gets key pressed immediately, without the need to wait
 * for new line (Enter)
 * Taken from: 
 * https://stackoverflow.com/questions/12710582/how-can-i-capture-a-key-stroke-immediately-in-linux */
{
	struct termios oldt, newt;
	int ch;

	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~(ICANON|ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	ch = getchar();
	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

	return ch;
}

int
xstrcmp(const char *str1, const char *str2)
{
/* Check for null. This check is done neither by strcmp nor by strncmp. I 
* use 256 for error code since it does not represent any ASCII code (the 
* extended version goes up to 255) */
	if (!str1 || !str2)
		return 256;

	while (*str1) {
		if (*str1 != *str2)
			return (*str1 - *str2);
		str1++;
		str2++;
	}
	if (*str2)
		return (0 - *str2);

	return 0;
	
/*	for (; *str1 == *str2; str1++, str2++)
		if (*str1 == 0x00)
			return 0;
	return *str1 - *str2; */
}

int
xstrncmp(const char *str1, const char *str2, size_t n)
{
	if (!str1 || !str2)
		return 256;

	size_t counter = 0;
	while (*str1 && counter < n) {
		if (*str1 != *str2)
			return (*str1 - *str2);
		str1++;
		str2++;
		counter++;
	}

	if (counter == n)
		return 0;
	if (*str2)
		return (0 - *str2);

	return 0;
}

char *
xstrcpy(char *buf, const char *str)
{
	if (!str)
		return (char *)NULL;

	while (*str) {
		*buf = *str;
		buf++;
		str++;
	}

	*buf = 0x00;

/*	while ((*buf++ = *str++)); */

	return buf;
}

char *
xstrncpy(char *buf, const char *str, size_t n)
{
	if (!str)
		return (char *)NULL;

	size_t counter = 0;
	while (*str && counter < n) {
		*buf = *str;
		buf++;
		str++;
		counter++;
	}

	*buf = 0x00;

/*	size_t counter = 0;
	while ((*buf++ = *str++) && counter++ < n); */

	return buf;
}

/*
size_t
xstrlen(const char *str)
{
	size_t len = 0;

	while (*(str++))
		len++;

	return len;
} */

/*
int
xatoi(const char *str)
// 2 times faster than atoi. Cannot handle negative number (See xnatoi below)
{
	int ret = 0; 
	
	while (*str) {
		if (*str < 0x30 || *str > 0x39)
			return ret;
		ret = (ret * 10) + (*str - '0');
		str++;
	}

	if (ret > INT_MAX)
		return INT_MAX;
	
	if (ret < INT_MIN)
		return INT_MIN;

	return ret;
}

int
xnatoi(const char *str)
// 2 times faster than atoi. The commented lines make xatoi able to handle 
// negative values
{
	int ret = 0, neg = 0;

	while (*str) {
		if (*str < 0x30 || *str > 0x39)
			return ret;
		ret = (ret * 10) + (*str - '0');
		str++;
	}

	if (neg)
		ret = ret - (ret * 2);

	if (ret > INT_MAX)
		return INT_MAX;
	
	if (ret < INT_MIN)
		return INT_MIN;

	return ret;
} */

pid_t
get_own_pid(void)
{
	pid_t pid;

	/* Get the process id */
	pid = getpid();

	if (pid < 0)
		return 0;
	else
		return pid;
}

char *
get_user(void)
/* Returns a pointer to a new string containing the current user's name, or
 * NULL if not found */
{
	struct passwd *pw;
	uid_t uid = 0;
	
	uid = geteuid();

	/* Get a passwd struct for current user ID. An alternative is
	 * to use setpwent(), getpwent(), and endpwent() */
	pw = getpwuid(uid);
/*
	printf("Username: %s\n", user_info->pw_name);
	printf("Password: %s\n", user_info->pw_passwd);
	printf("UID: %d\n", user_info->pw_uid);
	printf("GID: %d\n", user_info->pw_gid);
	printf("Gecos: %s\n", user_info->pw_gecos);
	printf("Home: %s\n", user_info->pw_dir);
	printf("Shell: %s\n", user_info->pw_shell);
*/

	if (!pw)
		return (char *)NULL;

	/* Why we don't just return a pointer to the field of the passwd struct
	 * we need? Because this struct will be overwritten by subsequent calls to
	 * getpwuid(), for example, in the properties function, in which case
	 * our pointer will point to a wrong string. So, to avoid this, we just
	 * copy the string we need into a new variable. The same applies to the
	 * following functions, get_user_home() and get_sys_shell() */
	char *p = (char *)NULL;
	p = (char *)malloc((strlen(pw->pw_name) + 1) * sizeof(char));

	if (!p)
		return (char *)NULL;
	
	char *username = p;
	p = (char *)NULL;

	strcpy(username, pw->pw_name);
	
	return username;
}

char *
get_user_home(void)
/* Returns a pointer to a string containing the user's home directory, or NULL
 * if not found */
{
	struct passwd *pw;
	
	pw = getpwuid(getuid());

	if (!pw)
		return NULL;

	char *p = (char *)NULL;
	p = (char *)malloc((strlen(pw->pw_dir) + 1) * sizeof(char));

	if (!p)
		return (char *)NULL;
	
	char *home = p;
	p = (char *)NULL;

	strcpy(home, pw->pw_dir);

	return home;
}

char *
get_sys_shell(void)
/* Returns a pointer to a string containing the user's default shell or NULL 
 * if not found */
{
	struct passwd *pw;

	pw = getpwuid(getuid());

	if (!pw)
		return (char *)NULL;

	char *p = (char *)NULL;
	p = (char *)malloc((strlen(pw->pw_shell) + 1) * sizeof(char));

	if (!p)
		return (char *)NULL;
	
	char *shell = p;
	p = (char *)NULL;

	strcpy(shell, pw->pw_shell);
	
	return shell;
}

int
is_number(const char *str)
/* Check whether a given string contains only digits. Returns 1 if true and 0 
 * if false. Does not work with negative numbers */
{
	for (; *str ; str++)
		if (*str < 0x30 || *str > 0x39)
			return 0;

	return 1;
}

size_t
digits_in_num(int num) {
/* Return the amount of digits in a given number */
	size_t count = 0; /* VERSION 2: neither printf nor any function call at all */

	while (num != 0) {
		num /= 10; /* n = n/10 */
		++count;
	}

	return count;
}

int
xalphasort(const struct dirent **a, const struct dirent **b)
/* Same as alphasort, but is uses strcmp instead of sctroll, which is 
 * slower. However, bear in mind that, unlike strcmp(), strcoll() is locale 
 * aware. Use only with C and english locales */
{
	/* The if statements prevent strcmp from running in every 
	 * call to the function (it will be called only if the first 
	 * character of the two strings is the same), which makes the 
	 * function faster */
	if ((*a)->d_name[0] > (*b)->d_name[0])
		return 1;

	else if ((*a)->d_name[0] < (*b)->d_name[0])
		return -1;

	else
		return strcmp((*a)->d_name, (*b)->d_name);
}

int
alphasort_insensitive(const struct dirent **a, const struct dirent **b)
/* This is a modification of the alphasort function that makes it case 
 * insensitive. It also sorts without taking the initial dot of hidden 
 * files into account. Note that strcasecmp() isn't locale aware. Use
 * only with C and english locales */
{
	return strcasecmp(((*a)->d_name[0] == '.') ? (*a)->d_name+1 : 
					(*a)->d_name, ((*b)->d_name[0] == '.') ? 
					(*b)->d_name + 1 : (*b)->d_name);
}

int
strcntchr(const char *str, const char c)
/* Returns the index of the first appearance of c in str, if any, and -1 if c 
 * was not found or if no str. NOTE: Same thing as strchr(), except that 
 * returns an index, not a pointer */
{
	if (!str)
		return -1;
	
	register int i = 0;

	while (*str) {
		if (*str == c)
			return i;
		i++;
		str++;
	}

	return -1;
}

char *
straft(char *str, const char c)
/* Returns the string after the first appearance of a given char, or returns 
 * NULL if C is not found in STR or C is the last char in STR. */
{
	if (!str || !*str || !c)
		return (char *)NULL;
	
	char *p = str, *q = (char *)NULL;

	while (*p) {
		if (*p == c) {
			q = p;
			break;
		}
		p++;
	}
	
	/* If C was not found or there is nothing after C */
	if (!q || !*(q + 1))
		return (char *)NULL;

	char *buf = (char *)malloc(strlen(q));

	if (!buf)
		return (char *)NULL;

	strcpy(buf, q + 1);

	return buf;
}

char *
straftlst(char *str, const char c)
/* Returns the string after the last appearance of a given char, or NULL if no 
 * match */
{
	if (!str || !*str || !c)
		return (char *)NULL;
	
	char *p = str, *q = (char *)NULL;
	
	while (*p) {
		if (*p == c)
			q = p;
		p++;
	}
	
	if (!q || !*(q + 1))
		return (char *)NULL;
	
	char *buf = (char *)malloc(strlen(q));
	
	if (!buf)
		return (char *)NULL;

	strcpy(buf, q + 1);
	
	return buf;
}

char *
strbfr(char *str, const char c)
/* Returns the substring in str before the first appearance of c. If not 
 * found, or C is the first char in STR, returns NULL */
{
	if (!str || !*str || !c)
		return (char *)NULL;

	char *p = str, *q = (char *)NULL;
	while (*p) {
		if (*p == c) {
			q = p; /* q is now a pointer to C */
			break;
		}
		p++;
	}
	
	/* C was not found or it was the first char in STR */
	if (!q || q == str)
		return (char *)NULL;

	*q = 0x00; 
	/* Now C (because q points to C) is the null byte and STR ends in C, which 
	 * is what we want */

	char *buf = (char *)malloc((size_t)(q - str + 1));

	if (!buf) { /* Memory allocation error */
		/* Give back to C its original value, so that STR is not modified in 
		 * the process */
		*q = c;
		return (char *)NULL;
	}

	strcpy(buf, str);
	
	*q = c;
	
	return buf;
}

char *
strbfrlst(char *str, const char c)
/* Get substring in STR before the last appearance of C. Returns substring 
 * if C is found and NULL if not (or if C was the first char in STR). */
{
	if (!str || !*str || !c)
		return (char *)NULL;

	char *p = str, *q = (char *)NULL;

	while (*p) {
		if (*p == c)
			q = p;
		p++;
	}
	
	if (!q || q == str)
		return (char *)NULL;
	
	*q = 0x00;

	char *buf = (char *)malloc((size_t)(q - str + 1));

	if (!buf) {
		*q = c;
		return (char *)NULL;
	}

	strcpy(buf, str);
	
	*q = c;
	
	return buf;
}

char *
strbtw(char *str, const char a, const char b)
/* Returns the string between first ocurrence of A and the first ocurrence of B 
 * in STR, or NULL if: there is nothing between A and B, or A and/or B are not 
 * found */
{
	if (!str || !*str || !a || !b)
		return (char *)NULL;

	char *p = str, *pa = (char *)NULL, *pb = (char *)NULL;

	while (*p) {
		if (!pa) {
			if (*p == a)
				pa = p;
		}
		else if (*p == b) {
			pb = p;
			break;
		}
		p++;
	}
	
	if (!pb)
		return (char *)NULL;
		
	*pb = 0x00;
	
	char *buf = (char *)malloc((size_t)(pb - pa));
	
	if (!buf) {
		*pb = b;
		return (char *)NULL;
	}
	
	strcpy(buf, pa + 1);
	
	*pb = b;
	
	return buf;
}

/* The following four functions (from_hex, to_hex, url_encode, and
 * url_decode) were taken from "http://www.geekhideout.com/urlcode.shtml"
 * and modified to comform to RFC 2395, as recommended by the freedesktop
 * trash specification */
char
from_hex(char c)
/* Converts a hex char to its integer value */
{
	return isdigit(c) ? c - '0' : tolower(c) - 'a' + 10;
}

char
to_hex(char c)
/* Converts an integer value to its hex form*/
{
	static char hex[] = "0123456789ABCDEF";
	return hex[c & 15];
}

char *
url_encode(char *str)
/* Returns a url-encoded version of str */
{
	if (!str || *str == 0x00)
		return (char *)NULL;

	char *p;
	p = (char *)calloc((strlen(str) * 3) + 1, sizeof(char));
	/* The max lenght of our buffer is 3 times the length of STR plus 1 extra 
	 * byte for the null byte terminator: each char in STR will be, if encoded, 
	 * %XX (3 chars) */
	if (!p)
		return (char *)NULL;

	char *buf;
	buf = p;
	p = (char *)NULL;

	/* Copies of STR and BUF pointers to be able
	* to increase and/or decrease them without loosing the original memory 
	* location */
	char *pstr, *pbuf; 
	pstr = str;
	pbuf = buf;
	
	for (; *pstr; pstr++) {
		if (isalnum (*pstr) || *pstr == '-' || *pstr == '_' 
					|| *pstr == '.' || *pstr == '~' || *pstr == '/')
			/* Do not encode the above chars */
			*pbuf++ = *pstr;
		else {
			/* Encode char to URL format. Example: space char to %20 */
			*pbuf++ = '%';
			*pbuf++ = to_hex(*pstr >> 4); /* Right shift operation */
			*pbuf++ = to_hex(*pstr & 15); /* Bitwise AND operation */
		}
	}
	
	return buf;
}

char *
url_decode(char *str)
/* Returns a url-decoded version of str */
{
	if (!str || str[0] == 0x00)
		return (char *)NULL;

	char *p = (char *)NULL;
	p = (char *)calloc(strlen(str) + 1, sizeof(char));
	/* The decoded string will be at most as long as the encoded string */

	if (!p)
		return (char *)NULL;

	char *buf;
	buf = p;
	p = (char *)NULL;
	
	char *pstr, *pbuf;
	pstr = str; 
	pbuf = buf;
	for ( ; *pstr; pstr++) {
		if (*pstr == '%') {
			if (pstr[1] && pstr[2]) {
				/* Decode URL code. Example: %20 to space char */  
				/* Left shift and bitwise OR operations */  
				*pbuf++ = from_hex(pstr[1]) << 4 | from_hex(pstr[2]);
				pstr += 2;
			}
		}
		else
			*pbuf++ = *pstr;
	}

	return buf;
}

char *
get_date (void)
{
	time_t rawtime = time(NULL);
	struct tm *tm = localtime(&rawtime);
	size_t date_max = 128;

	char *p = (char *)calloc(date_max + 1, sizeof(char)), *date;
	if (p) {
		date = p;
		p = (char *)NULL;
	}
	else
		return (char *)NULL;

	strftime(date, date_max, "%a, %b %d, %Y, %T", tm);

	return date;
}

char *
get_size_unit(off_t file_size)
/* Convert FILE_SIZE to human readeable form */
{
	size_t max_size_type_len = 11;
	/* Max size type length == 10 == "1024.1KiB\0",
	 * or 11 == "1023 bytes\0" */
	char *size_type = calloc(max_size_type_len + 1, sizeof(char));
	short units_n = 0;
	float size = (float)file_size;

	while (size > 1024) {
		size = size/1024;
		++units_n;
	}

	/* If bytes */
	if (!units_n)
		snprintf(size_type, max_size_type_len, "%.0f bytes", (double)size);
	else {
		static char units[] = { 'b', 'K', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y' };		
		snprintf(size_type, max_size_type_len, "%.1f%ciB", (double)size, 
				 units[units_n]);
	}

	return size_type;
}

int
read_octal(char *str)
/* Convert octal string into integer. 
 * Taken from: https://www.geeksforgeeks.org/program-octal-decimal-conversion/ 
 * Used by decode_prompt() to make things like this work: \033[1;34m */
{
	if (!str)
		return -1;
	
	int n = atoi(str);
	int num = n; 
	int dec_value = 0; 

	/* Initializing base value to 1, i.e 8^0 */
	int base = 1; 

	int temp = num; 
	while (temp) { 

		/* Extracting last digit */
		int last_digit = temp % 10; 
		temp = temp / 10; 

		/* Multiplying last digit with appropriate 
		 * base value and adding it to dec_value */
		dec_value += last_digit * base; 

		base = base * 8; 
	}

	return dec_value; 
}

int
hex2int(char *str)
{
	int i, n[2];
	for (i = 1; i >= 0; i--) {
		if (str[i] >= '0' && str[i] <= '9')
			n[i] = str[i] - 0x30;
		else {
			switch(str[i]) {
			case 'A': case 'a': n[i] = 10; break;
			case 'B': case 'b': n[i] = 11; break;
			case 'C': case 'c': n[i] = 12; break;
			case 'D': case 'd': n[i] = 13; break;
			case 'E': case 'e': n[i] = 14; break;
			case 'F': case 'f': n[i] = 15; break;
			default: break;
			}
		}
	}

	return ((n[0] * 16) + n[1]);
}

char *
remove_quotes(char *str)
/* Removes end of line char and quotes (single and double) from STR. Returns a
 * pointer to the modified STR if the result is non-blank or NULL */
{
	if (!str || !*str)
		return (char *)NULL;
	
	char *p = str;
	size_t len = strlen(p);
	
	if (len > 0 && p[len - 1] == '\n') {
		p[len - 1] = 0x00;
		len--;
	}
	
	if (len > 0 && (p[len - 1] == '\'' || p[len - 1] == '"'))
		p[len - 1] = 0x00;

	if (*p == '\'' || *p == '"')
		p++;
	
	if (!*p)
		return (char *)NULL;
	
	char *q = p;
	int blank = 1;

	while(*q) {
		if (*q != 0x20 && *q != '\n' && *q != '\t') {
			blank = 0;
			break;
		}
		q++;
	}
	
	if (!blank)
		return p;
	
	return (char *)NULL;
}

int
is_acl(char *file)
/* Return 1 if FILE has some ACL property and zero if none
 * See: https://man7.org/tlpi/code/online/diff/acl/acl_view.c.html */
{
	if (!file || !*file)
		return 0;

	acl_t acl;
	int entryid, num = 0;
	acl = acl_get_file(file, ACL_TYPE_ACCESS);

	if (acl) {
		acl_entry_t entry;

		for (entryid = ACL_FIRST_ENTRY; ; entryid = ACL_NEXT_ENTRY) {
			if (acl_get_entry(acl, entryid, &entry) != 1)
				break;
			num++;
		}

		acl_free(acl);

		if (num > 3)
			/* We have something else besides owner, group, and others, that is,
			 * we have at least one ACL property */
			return 1;
		else
			return 0;
	}

	else /* Error */
		/* fprintf(stderr, "%s\n", strerror(errno)); */
		return 0;

	return 0;
}


				/* ##########################
				 * #  FUNCTIONS PROTOTYPES  # 
				 * ##########################*/

void signal_handler(int sig_num);
char *xnmalloc(size_t nmemb, size_t size);
void *xcalloc(size_t nmemb, size_t size);
void *xrealloc(void *ptr, size_t size);
void free_stuff(void);
void set_signals_to_ignore(void);
void set_signals_to_default(void);
void init_shell(void);
void file_cmd_check(void);
void splash(void);
char **my_rl_completion(const char *text, int start, int end);
char *my_rl_quote(char *text, int m_t, char *qp);
char *dequote_str(char *text, int m_t);
int quote_detector(char *line, int index);
int is_quote_char(char c);
char *filenames_generator(const char *text, int state);
char *bin_cmd_generator(const char *text, int state);
void get_path_programs(void);
size_t get_path_env(void);
int get_sel_files(void);
void init_config(void);
void exec_profile(void);
void external_arguments(int argc, char **argv);
void get_aliases(void);
void get_prompt_cmds(void);
void check_log_file_size(char *log_file);
char **check_for_alias(char **comm);
int get_history(void);
void add_to_dirhist(const char *dir_path);
char *home_tilde(const char *new_path);
char **parse_input_str(char *str);
int list_dir(void);
char *prompt(void);
int exec_cmd(char **comm);
int launch_execve(char **cmd, int bg);
int launch_execle(const char *cmd);
char *get_cmd_path(const char *cmd);
int brace_expansion(const char *str);
int run_in_foreground(pid_t pid);
void run_in_background(pid_t pid);
int copy_function(char **comm);
int run_and_refresh(char **comm);
int properties_function(char **comm);
int get_properties(char *filename, int _long, int max, size_t filename_len);
int save_sel(void);
int sel_function(char **comm);
void show_sel_files(void);
int deselect(char **comm);
int search_function(char **comm);
int bookmarks_function(char **cmd);
char **get_bookmarks(char *bookmarks_file);
char **bm_prompt(void);
size_t count_dir(const char *dir_path);
char *rl_no_hist(const char *prompt);
int log_function(char **comm);
int history_function(char **comm);
int run_history_cmd(const char *cmd);
int edit_function(char **comm);
void colors_list(const char *entry, const int i, const int pad, 
				 const int new_line);
int hidden_function(char **comm);
void help_function(void);
void color_codes(void);
void list_commands(void);
int folder_select(const struct dirent *entry);
int file_select(const struct dirent *entry);
int skip_implied_dot(const struct dirent *entry);
void version_function(void);
void bonus_function(void);
char *parse_usrvar_value(const char *str, const char c);
int create_usr_var(char *str);
void readline_kbinds(void);
int readline_kbind_action(int count, int key);
void surf_hist(char **comm);
int trash_function(char **comm);
int trash_element(const char *suffix, struct tm *tm, char *file);
int wx_parent_check(char *file);
int remove_from_trash(void);
int untrash_function(char **comm);
int untrash_element(char *file);
int trash_clear(void);
int recur_perm_check(const char *dirname);
int *expand_range(char *str, int listdir);
void log_msg(char *msg, int print);
void keybind_exec_cmd(char *str);
int get_max_long_view(void);
int list_mountpoints(void);
int back_function(char **comm);
int forth_function(char **comm);
int cd_function(char *new_path);
int open_function(char **cmd);
size_t u8_xstrlen(const char *str);
void print_license(void);
void free_sotware(void);
char **split_str(char *str);
int is_internal(const char *cmd);
char *escape_str(char *str);
void set_default_options(void);
void set_colors (void);
int is_color_code(char *str);
char **get_substr(char *str, const char ifs);
int set_shell(char *str);
void exec_chained_cmds(char *cmd);
int is_internal_c(const char *cmd);
int open_bookmark(char **cmd);
int get_bm_names(void);
char *bookmarks_generator(const char *text, int state);
int initialize_readline(void);
int bookmark_del(char *name);
int bookmark_add(char *file);
char *savestring(const char *str, size_t size);
char *my_rl_path_completion(const char *text, int state);
int get_link_ref(const char *link);
int mime_open(char **args);
int mime_import(char *file);
int mime_edit(char **args);
char *get_mime(char *file);
char *get_app(char *mime, char *ext);
int profile_function(char ** comm);
int profile_add(char *prof);
int profile_del(char *prof);
int profile_set(char *prof);
int get_profile_names(void);
char *profiles_generator(const char *text, int state);
char *decode_prompt(char *line);
int alias_import(char *file);
int record_cmd(char *input);
void add_to_cmdhist(char *cmd);
int _err(int msg_type, int prompt, const char *format, ...);
int new_instance(char *dir);
int *get_hex_num(char *str);
int create_config(char *file);

int remote_ssh(char *address, char *options);
int remote_smb(char *address, char *options);
int remote_ftp(char *address, char *options);

/* Some notes on memory:
* If a variable is declared OUTSIDE of a function, it is typically considered 
* "GLOBAL," meaning that any function can access it. Global variables are 
* stored in a special area in the program's memory called the DATA SEGMENT. 
* This memory is in use as long as the program is running. The DATA SEGMENT 
* is where global and static variables (if non-zero initialized) are stored.
* Variables initialized to zero or uninitialized are stored in the BSS.
* The STACK is used to store FIXED variables that are used only INSIDE a 
* function (LOCAL). Stack memory is temporary. Once a function finishes 
* running, all of the memory for its variables is freed, unless the variable 
* was declared as static, in which case it's stored in the data segment.
* The HEAP is where dynamic variables are stored. Everything going through 
* malloc, realloc, and free goes to the heap.
* 
* The STACK is MUCH MORE FASTER THAN THE HEAP, THOUGH DINAMYCALLY ALLOCATED 
* ARRAYS GENERALLY MAKE THE PROGRAM MUCH MORE LEIGHTWEIGHT IN TERMS OF MEMORY 
* FOOTPRINT. 
* Ex: 
	char str[4096]="test"; //4096 bytes
* obviously takes more memory than this:
 	char *str=calloc(strlen("test"), sizeof(char *)); //5 bytes
 * However, the second procedure (0.000079 seconds) is dozens of times slower 
 * than the second one (0.000001 seconds!!). The results are almost the same 
 * even without strlen, that is, as follows: 
 * char *str=calloc(5, sizeof(char *)). 
 * Replacing calloc by malloc doesn't seem to make any difference either.

high address
    _______________
							| cmd-line args and env variables
	_______________
	4 - STACK*				| fixed local variables (not declared as static)
							  MUCH MORE FASTER THAN THE HEAP!!!
	---------------
	
	
	---------------
	3 - HEAP**				| dynamically allocated variables (the home of 
	* 						  malloc, realloc, and free)
	_______________
	3 - BBS 				| uninitialized or initialized to zero
    _______________
	2 - DATA SEGMENT        | global (static or not) and local static 
	* 						  non-zero-initialized variables. This mem is 
	* 						  freed only once the program exited.
	_______________
	1 - TEXT (CODE) SEGMENT	|
low address 

*The stack grows in direction to the heap.
** The heap grows in direction to the stack.
* Note: if the heap and the stack met, you're out of memory.

* You can use the 'size' command to see the size of the text, data, and bbs 
* segments of your program. Ex: $ size clifm

* See: https://www.geeksforgeeks.org/memory-layout-of-c-program/

Data types sizes:
CHAR: 1 byte (8 bits) (ascii chars: -128 to 127 or 0 to 255 if unsigned).
	char str[length] takes 1*length bytes.
SHORT: 2 bytes (16 bits) (-32,768 to 32,767).
INT: 2 (for old 16 bits PC's) or 4 bytes (32 bits). if 4 bytes: -2,147,483,648 
to 2,147,483,647 (if signed). 
	'int var[5]', for example, takes 4*5 bytes.
LONG: 8 bytes (64 bits) (-2,147,483,648 to 2,147,483,647, if signed)
LONG LONG: 8 bytes
FLOAT: 4 bytes (1.2E-38 to 3.4E+38. Six decimal places)
DOUBLE: 8 bytes (2.3E-308 to 1.7E+308. 15 decimal places).
LONG DOUBLE: 16 bytes

* See: https://www.tutorialspoint.com/cprogramming/c_data_types.htm

POINTERS: Ex: char *p; pointers, unlike common variables which stores data, 
* store memory addresses, which in 32 bits machines take 4 bytes (0xffff3d98) 
* and in 64 bits machines 8 bytes (0xffffffffd989ed4f). So, while 'char p' 
* takes only 1 byte, 'char *p' takes 4/8 bytes. Since pointers store only 
* memory addresses, the type of pointer (char, int, float, etc.) has nothing 
* to do with its length; it's always 4/8 bytes. 
* NOTE: in 16 bits machines a pointer size is 2 bytes.

* Note: unsigned data types may contain more positive values that its signed 
* counterpart, but at the price of not being able to store negative values. 
* size_t is just an alias for unsigned int.
* See: http://www.geeksforgeeks.org/memory-layout-of-c-program/

* Garbage Collection (automatic memory handling) in C
* Though C is designed to manually handle memory, there is a garbarge 
* collector (gc) available (The Boehm-Demers-Weiser GC Library, or the WDW 
* library) to automatically handle memory, much like Java, Python, or Bash 
* does: just add the gc/gc.h library and then use the GC_malloc and GC_realloc 
* macros instead of malloc/calloc and realloc. There is no need to call 
* free(). In order to replace all calls to malloc/calloc and realloc by their 
* corresponing GC functions, add the following define lines:

#define malloc(x) GC_malloc(x)
#define calloc(n,x) GC_malloc((n)*(x))
#define realloc(p,x) GC_realloc((p),(x))
#define free(x) (x) = NULL

* NOTE: The above are called macros. The macro name is replaced by its 
* definition all along the source code by the pre-processor even before 
* compilation. If we have this macro:
#define MAX 10
* the compiler will not see this: 'a=50+MAX', but this: 'a=50+10'.

*/

				/* ##########################
				 * #    GLOBAL VARIABLES    # 
				 * ##########################*/

/* These variables, unlike local or automatic variables, are accessible to any 
 * part of the program and hold their values throughout the entire lifetime of 
 * the program. If not initialized, they're initialized automatically by the 
 * compiler in the following way:
 int 		0
 char 		'\0' or 0x00 (hex)
 float	 	0
 double 	0
 pointer 	NULL

* Local variables are only accesible from within the function they were 
* declared, and loses their value once the function ended. The same goes 
* for block variables, only that these variables only work within the 
* block in which they were declared. Example: if (x) int a=1; the variable 
* 'a' will only be visible within the if block. Parameters passed to a 
* function are also considered local to that function.

* Global, local, and block are the different SCOPES a variable can have.
* See: https://www.geeksforgeeks.org/scope-rules-in-c/
* On why to reduce variables scope as much as possible see: 
* https://refactoring.com/catalog/reduceScopeOfVariable.html

* When initializing a char string, make sure you add the NULL terminator or 
* leave room, 1 byte, so that it can be added automatically. Example: 
* char [6]="hello\0", or char[6]="hello", or char[]="hello". Do not initialize 
* it like this: char [5]="hello", for this will be a non-NULL-terminated 
* string.
*/

/* Without this variable, TCC complains that __dso_handle is an undefined 
 * symbol and won't compile */
#if __TINYC__
void* __dso_handle;
#endif

/* Struct to store user defined variables */
struct usrvar_t {
	char *name;
	char *value;
};

/* Struct to store file information */
struct fileinfo
{
	size_t len;
	size_t filesn; /* Number of files in subdir */
	int exists;
	mode_t type;
	off_t size;
	int ruser; /* User read permission for dir */
};

/* Struct to specify which parameters have been set from the command line,
 * to avoid overriding them with init_config(). While no command line parameter
 * will be overriden, the user still can modifiy on the fly (editing the config
 * file) any option not specified in the command line */
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
};

/* A list of possible program messages. Each value tells the prompt what to do
 * with error messages: either to print an E, W, or N char at the beginning of 
 * the prompt, or nothing (nomsg) */
enum prog_msg {
	nomsg = 0,
	error = 1,
	warning = 2,
	notice = 4
};

/* pmsg holds the current program message type */
enum prog_msg pmsg = nomsg;

/* Always initialize variables, to NULL if string, to zero if int; otherwise 
 * they may contain garbage, and an access to them may result in a crash or 
 * some invalid data being read. However, non-initialized variables are 
 * automatically initialized by the compiler */

short splash_screen = -1, welcome_message = -1, ext_cmd_ok = -1,
	show_hidden = -1, clear_screen = -1, shell_terminal = 0, pager = -1, 
	no_log = 0, internal_cmd = 0, list_folders_first = -1, case_sensitive = -1, 
	cd_lists_on_the_fly = -1, recur_perm_error_flag = 0, is_sel = 0, 
	sel_is_last = 0, print_msg = 0, long_view = -1, kbind_busy = 0, 
	unicode = -1, dequoted = 0, home_ok = 1, config_ok = 1, trash_ok = 1, 
	selfile_ok = 1, mime_match = 0, logs_enabled = -1, sort = -1,
	dir_counter = -1;
	/* -1 means non-initialized or unset. Once initialized, these variables
	 * are always either zero or one */
/*	sel_no_sel=0 */
/* A short int accepts values from -32,768 to 32,767, and since all the 
 * above variables will take -1, 0, and 1 as values, a short int is more 
 * than enough. Now, since short takes 2 bytes of memory, using int for these 
 * variables would be a waste of 2 bytes (since an int takes 4 bytes). I can 
 * even use char (or signed char), which takes only 1 byte (8 bits) and 
 * accepts negative numbers (-128 -> +127).
* NOTE: If the value passed to a variable is bigger than what the variable can 
* hold, the result of a comparison with this variable will always be true */

int max_hist = -1, max_log = -1, dirhist_total_index = 0, 
	dirhist_cur_index = 0, argc_bk = 0, max_path = -1, exit_code = 0, 
	shell_is_interactive = 0, cont_bt = 0;

unsigned short term_cols = 0;

size_t user_home_len = 0, args_n = 0, sel_n = 0, trash_n = 0, msgs_n = 0, 
	   prompt_cmds_n = 0, path_n = 0, current_hist_n = 0, usrvar_n = 0, 
	   aliases_n = 0, longest = 0, files = 0, eln_len = 0;

struct termios shell_tmodes;
pid_t own_pid = 0;
struct usrvar_t *usr_var = (struct usrvar_t *)NULL;
struct param xargs;
char *user = (char *)NULL, *path = (char *)NULL, **old_pwd = (char **)NULL, 
	**sel_elements = (char **)NULL, *qc = (char *)NULL,	
	*sel_file_user = (char *)NULL, **paths = (char **)NULL, 
	**bin_commands = (char **)NULL, **history = (char **)NULL, 
	*file_cmd_path = (char *)NULL, **braces = (char **)NULL,
	*alt_profile = (char *)NULL, **prompt_cmds = (char **)NULL, 
	**aliases = (char **)NULL, **argv_bk = (char **)NULL, 
	*user_home = (char *)NULL, **messages = (char **)NULL, 
	*msg = (char *)NULL, *CONFIG_DIR = (char *)NULL, div_line_char = -1,
	*CONFIG_FILE = (char *)NULL, *BM_FILE = (char *)NULL, 
	hostname[HOST_NAME_MAX] = "", *LOG_FILE = (char *)NULL, 
	*LOG_FILE_TMP = (char *)NULL, *HIST_FILE = (char *)NULL, 
	*MSG_LOG_FILE = (char *)NULL, *PROFILE_FILE = (char *)NULL, 
	*TRASH_DIR = (char *)NULL, *TRASH_FILES_DIR = (char *)NULL, 
	*TRASH_INFO_DIR = (char *)NULL, *sys_shell = (char *)NULL, 
	**dirlist = (char **)NULL, **bookmark_names = (char **)NULL,
	*ls_colors_bk = (char *)NULL, *MIME_FILE = (char *)NULL,
	**profile_names = (char **)NULL, *encoded_prompt = (char *)NULL,
	*last_cmd = (char *)NULL, *term = (char *)NULL, *TMP_DIR = (char *)NULL;

	/* This is not a comprehensive list of commands. It only lists commands
	 * long version for TAB completion */
const char *INTERNAL_CMDS[] = { "alias", "open", "prop", "back", "forth",
		"move", "paste", "sel", "selbox", "desel", "refresh", 
		"edit", "history", "hidden", "path", "help", "commands", 
		"colors", "version", "license", "splash", "folders first", 
		"exit", "quit", "pager", "trash", "undel", "messages", 
		"mountpoints", "bookmarks", "log", "untrash", "unicode", 
		"profile", "shell", "mime", NULL };

#define DEFAULT_PROMPT "\\A \\u:\\H \\[\\e[00;36m\\]\\w\\n\\[\\e[0m\\]\
\\z\\[\\e[0;34m\\] \\$\\[\\e[0m\\] "

#define DEFAULT_TERM_CMD "xterm -e"

#define FALLBACK_SHELL "/bin/sh"

#define MAX_COLOR 46
/* 46 == \x1b[00;38;02;000;000;000;00;48;02;000;000;000m\0 (24bit, RGB true 
 * color format including foreground and background colors, the SGR (Select 
 * Graphic Rendition) parameter, and, of course, the terminating null byte.

 * To store all the 29 color variables I use, with 46 bytes each, I need a 
 * total of 1,3Kb. It's not much but it could be less if I'd use dynamically 
 * allocated arrays for them */

/* Some interface colors */
char text_color[MAX_COLOR + 2] = "", eln_color[MAX_COLOR] = "", 
	 default_color[MAX_COLOR] = "", dir_count_color[MAX_COLOR] = "", 
	 div_line_color[MAX_COLOR] = "", welcome_msg_color[MAX_COLOR] = "";
/* text_color is used in the command line, and readline needs to know that 
 * color codes are not printable chars. For this we need to add "\001" at 
 * the beginning of the color code and "\002" at the end. We need thereby 2 
 * more bytes */

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
	* __FreeBSD___, __NetBSD__, __OpenBSD__, __bdsi__, __DragonFly__
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
	 * remove the leading "./" to get the correct program invocation name */
	if (*argv[0] == '.' && *(argv[0] + 1) == '/')
		argv[0] += 2;

	/* Use the locale specified by the environment. By default (ANSI C), 
	 * all programs start in the standard 'C' (== 'POSIX') locale. This 
	 * function makes the program portable to all locales (See man (3) 
	 * setlocale). It affects characters handling functions like toupper(),
	 * tolower(), alphasort() (since it uses strcoll()), and strcoll() itself,
	 * among others. Here, it's important to correctly sort filenames 
	 * according to the rules specified by the current locale. If the function 
	 * fails, the default locale ("C") is not changed */
	setlocale(LC_ALL, "");
	/* To query the current locale, use NULL as second argument to 
	 * setlocale(): printf("Locale: %s\n", setlocale(LC_CTYPE, NULL)); */

	/* If the locale isn't English, set 'unicode' to true to correctly list 
	 * (sort and padding) filenames containing non-7bit ASCII chars like 
	 * accents, tildes, umlauts, non-latin letters, and so on */
	if (strncmp(getenv("LANG"), "en", 2) != 0)
		unicode = 1;

	/* Initialize gettext() for translations */
	bindtextdomain("clifm", "/usr/share/locale");
	textdomain("clifm");

	/* Store external arguments to be able to rerun external_arguments() in 
	 * case the user edits the config file, in which case the program must 
	 * rerun init_config(), get_aliases(), get_prompt_cmds(), and then 
	 * external_arguments() */
	argc_bk = argc;
	argv_bk = (char **)xnmalloc((size_t)argc, sizeof(char *));
	register size_t i = 0;
	for (i = 0; i < (size_t)argc; i++) {
		argv_bk[i] = (char *)xnmalloc(strlen(argv[i]) + 1, sizeof(char));
		strcpy(argv_bk[i], argv[i]);
	}

	/* Register the function to be called at normal exit, either via exit()
	 * or main's return. The registered function will not be executed when
	 * abnormally exiting the program, eg., via the KILL signal */
	atexit(free_stuff);
	
	/* Get user's home directory */
	user_home = get_user_home();
	if (!user_home || access(user_home, W_OK) == -1) {
		/* If no user's home, or if it's not writable, there won't be any 
		 * config nor trash directory. These flags are used to prevent 
		 * functions from trying to access any of these directories */
		home_ok = config_ok = trash_ok = 0;
		/* Print message: trash, bookmarks, command logs, commands history and 
		 * program messages won't be stored */
		_err('e', PRINT_PROMPT, _("%s: Cannot access the home directory. Trash, "
			 "bookmarks, commands logs, and commands history are disabled. "
			 "Program messages and selected files won't be persistent. Using "
			 "default options\n"), PROGRAM_NAME);
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

	/* Get paths from PATH environment variable. These paths will be used 
	 * later by get_path_programs (for the autocomplete function) and 
	 * get_cmd_path() */
	path_n = (size_t)get_path_env();

	/* Manage external arguments, but only if any: argc == 1 equates to no 
	 * argument, since this '1' is just the program invokation name. External 
	 * arguments will override initialization values (init_config) */
	if (argc > 1)
		external_arguments(argc, argv);
		/* external_arguments is executed before init_config because, if
		 * specified (-P option), it sets the value of alt_profile, which
		 * is then checked by init_config */

	/* Initialize program paths and files, set options from the config file, 
	 * if they were not already set via external arguments, and load sel 
	 * elements, if any. All these configurations are made per 
	 * user basis */
	init_config();

	if (!config_ok)
		set_default_options();

	/* Check whether we have a working shell */
	if (access(sys_shell, X_OK) == -1) {
		_err('w', PRINT_PROMPT, _("%s: %s: System shell not found. Please edit "
			 "the configuration file to specify a working shell.\n"),
			 PROGRAM_NAME, sys_shell);
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

	/* Get the list of available applications in PATH to be used by my
	 * custom autocomplete function */
	get_path_programs();
	
	exec_profile();	
	
	if (config_ok) {

		/* Limit the log files size */
		check_log_file_size(LOG_FILE);
		check_log_file_size(MSG_LOG_FILE);
	
		/* Get history */
		struct stat file_attrib;
		if (stat(HIST_FILE, &file_attrib) == 0 
		&& file_attrib.st_size != 0) {
		/* If the size condition is not included, and in case of a zero size 
		 * file, read_history() gives malloc errors */
			/* Recover history from the history file */
			read_history(HIST_FILE); /* This line adds more leaks to 
			readline */
			/* Limit the size of the history file to max_hist lines */
			history_truncate_file(HIST_FILE, max_hist);
		}
		else { /* If the history file doesn't exist, create it */
			FILE *hist_fp = fopen(HIST_FILE, "w+");
			if (!hist_fp) {
				_err('w', PRINT_PROMPT, "%s: fopen: '%s': %s\n", PROGRAM_NAME, 
					 HIST_FILE, strerror(errno));
			}
			else {
				/* To avoid malloc errors in read_history(), do not create 
				 * an empty file */
				fputs("edit\n", hist_fp);
				/* There is no need to run read_history() here, since the 
				 * history file is still empty */
				fclose(hist_fp);
			}
		}
	}
	
	/* Store history into an array to be able to manipulate it */
	get_history();

	/* Check if the 'file' command is available */
	file_cmd_check();

	/* ##### READLINE ##### */

	initialize_readline();
	/* Copy the list of quote chars to a global variable to be used
	 * later by some of the program functions like split_str(), my_rl_quote(), 
	 * is_quote_char(), and my_rl_dequote() */
	qc = (char *)xcalloc(strlen(rl_filename_quote_characters) + 1, 
						 sizeof(char));
	strcpy(qc, rl_filename_quote_characters);

	if (splash_screen) {
		splash();
		CLEAR;
	}

	/* If path was not set (neither in the config file nor via command 
	 * line, that is, as external argument), set the default (CWD), and
	 * if CWD is not set, use the user's home directory, and if the home
	 * cannot be found either, try the root directory, and if there's no
	 * access to the root dir either, exit. 
	 * Bear in mind that if you launch CliFM through a terminal emulator, 
	 * say xterm (xterm -e clifm), xterm will run a shell, say bash, and 
	 * the shell will read its config file. Now, if this config file changes 
	 * the CWD, this will the CWD for CliFM */
	if (!path) {
		char cwd[PATH_MAX] = "";
		getcwd(cwd, sizeof(cwd));
		/* getenv() returns an address, so that pwd is this address and
		 * *pwd is its value */
		if (!*cwd || strlen(cwd) == 0) {
			if (user_home) {
				path = (char *)xcalloc(strlen(user_home) + 1, sizeof(char));
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
	/* If chdir(path) fails, set path to cwd, list files and print the error 
	 * message. If no access to CWD either, exit */
	if (chdir(path) == -1) {

		_err('e', PRINT_PROMPT, "%s: chdir: '%s': %s\n", PROGRAM_NAME, path, 
			 strerror(errno));

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
	while (1) { /* Or: for (;;) --> Infinite loop to keep the program 
		running */
		char *input = prompt(); /* Get input string from the prompt */
		if (input) {
			char **cmd = parse_input_str(input); /* Parse input string */
			free(input);
			input = (char *)NULL;
			if (cmd) {
				char **alias_cmd = check_for_alias(cmd);
				if (alias_cmd) {
					/* If an alias is found, the function frees cmd and 
					 * returns alias_cmd in its place to be executed by 
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
		}
	}
	
	return EXIT_SUCCESS; /* Never reached */
}

/* ###FUNCTIONS DEFINITIONS### */

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

	char *rmountpoint = (char *)xnmalloc(strlen(TMP_DIR) + strlen(tmp_addr) + 9,
										 sizeof(char));
	sprintf(rmountpoint, "%s/remote/%s", TMP_DIR, tmp_addr);
	free(tmp_addr);

	struct stat file_attrib;
	if (stat(rmountpoint, &file_attrib) == -1) {
		char *mkdir_cmd[] = { "mkdir", "-p", rmountpoint, NULL };
		if (launch_execve(mkdir_cmd, FOREGROUND) != 0) {
			fprintf(stderr, _("%s: '%s': Cannot create mountpoint\n"),
					PROGRAM_NAME, rmountpoint);
			free(rmountpoint);
			return EXIT_FAILURE;
		}
	}

	else if (count_dir(rmountpoint) > 2) {
		fprintf(stderr, _("%s: '%s': Mounpoint is not empty\n"), PROGRAM_NAME,
				rmountpoint);
		free(rmountpoint);
		return EXIT_FAILURE;
	}
	
	/* CurlFTPFS does not require sudo */
	char *cmd[] = { "curlftpfs", address, rmountpoint, (options) ? "-o" : NULL,
					(options) ? options: NULL, NULL };
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

	char *addr_tmp = (char *)xnmalloc(strlen(raddress) + 3, sizeof(char));
	sprintf(addr_tmp, "//%s", raddress);

	char *p = raddress;
	while (*p) {
		if (*p == '/')
			*p = '_';
		p++;
	}

	char *rmountpoint = (char *)xnmalloc(strlen(raddress) + strlen(TMP_DIR)
										 + 9, sizeof(char));
	sprintf(rmountpoint, "%s/remote/%s", TMP_DIR, raddress);
	
	int free_options = 0;
	char *roptions = (char *)NULL;
	if (ruser) {
		roptions = (char *)xnmalloc(strlen(ruser) + strlen(options) + 11,
									sizeof(char));
		sprintf(roptions, "username=%s,%s", ruser, options);
		free_options = 1;
	}
	else
		roptions = options;

	/* Create the mountpoint, if it doesn't exist */
	struct stat file_attrib;
	if (stat(rmountpoint, &file_attrib) == -1) {
		char *mkdir_cmd[] = { "mkdir", "-p", rmountpoint, NULL };
		if (launch_execve(mkdir_cmd, FOREGROUND) != 0) {
			if (free_options)
				free(roptions);
			if (ruser)
				free(ruser);
			if (free_address)
				free(raddress);
			free(rmountpoint);
			free(addr_tmp);
			fprintf(stderr, "%s: '%s': Cannot create mountpoint\n",
					PROGRAM_NAME, rmountpoint);
			return EXIT_FAILURE;
		}
	}

	/* If the mountpoint already exists, check it is empty */
	else if (count_dir(rmountpoint) > 2) {
		fprintf(stderr, "%s: '%s': Mountpoint is not empty\n", 
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
		char *cmd[] = { "mount.cifs", addr_tmp, rmountpoint, (roptions) ? "-o"
						: NULL, (roptions) ? roptions : NULL, NULL };
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
	fprintf(stderr, _("%s: SFTP is not yet supported on FreeBSD"), PROGRAM_NAME);
	return EXIT_FAILURE;
	#endif
	
	if (!config_ok)
		return EXIT_FAILURE;

/*	char *sshfs_path = get_cmd_path("sshfs");
	if (!sshfs_path) {
		fprintf(stderr, _("%s: sshfs: Program not found.\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	} */

	if(!address || !*address)
		return EXIT_FAILURE;

	/* Create mountpoint */
	char *rname = (char *)xnmalloc(strlen(address) + 1, sizeof(char));
	strcpy(rname, address);

	/* Replace all slashes in address by underscore to construct the mounpoint
	 * name */
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

		if (launch_execve(mkdir_cmd, FOREGROUND) != 0) {
			fprintf(stderr, "%s: '%s': Cannot create mountpoint\n", 
					PROGRAM_NAME, rmountpoint);
			free(rmountpoint);
			return EXIT_FAILURE;
		}
	}

	/* If it exists, make sure it is not populated */
	else if (count_dir(rmountpoint) > 2) {
		fprintf(stderr, "%s: '%s': Mounpoint is not empty\n", 
				PROGRAM_NAME, rmountpoint);
		free(rmountpoint);
		return EXIT_FAILURE;
	}
	
	/* Construct the command */
	
	int error_code = 1;

	if ((flags & ROOT_USR)) {
		char *cmd[] = { "sshfs", address, rmountpoint, (options) ? "-o" : NULL, 
						(options) ? options: NULL, NULL };
		error_code = launch_execve(cmd, FOREGROUND);
	}
	else {
		char *cmd[] = { "sudo", "sshfs", address, rmountpoint, "-o", 
						"allow_other", (options) ? "-o" : NULL, (options) ? 
						options : NULL, NULL};
		error_code = launch_execve(cmd, FOREGROUND);
	}
	
	if (error_code != 0) {
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

	else {
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
# for example, listing files on a remote server. The dircounter can be \n\
# disabled here or using the 'dc' command while in the program itself.\n\
DirCounter=true\n\n"

"# DividingLineChar accepts both literal characters (in single quotes) and \n\
# decimal numbers.\n\
DividingLineChar='='\n\n"

"# The prompt line is build using string literals and/or the following escape\n\
# sequences:\n"
"# \\xnn: The character whose hexadecimal code is nn.\n\
# \\e: Escape character\n\
# \\h: The hostname, up to the first ‘.’\n\
# \\u: The username\n\
# \\H: The full hostname\n\
# \\n: A newline character\n\
# \\r: A carriage return\n\
# \\a: A bell character\n\
# \\d: The date, in abbrevieted form (ex: “Tue May 26”)\n\
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

"Prompt=\"%s\"\n\n"

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

"# Set the shell to be used when running external commands. Defaults to the \n\
# user's shell as is specified in '/etc/passwd'.\n\
SystemShell=\n\n"

"# Only used when opening a directory via a new CliFM instance (with the 'x' \n\
# command), this option specifies the command to be used to launch a \n\
# terminal emulator to run CliFM on it.\n\
TerminalCmd='%s'\n\n"

"SortList=true\n\
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

			DEFAULT_PROMPT, DEFAULT_TERM_CMD);

		fputs(

"#ALIASES\n\
#alias ls='ls --color=auto -A'\n\n"

"#PROMPT COMMANDS\n\n"
"# Write below the commands you want to be executed before the prompt.\n\
# Ex:\n\
#date | awk '{print $1\", \"$2,$3\", \"$4}'\n\n"
"#END OF PROMPT COMMANDS\n\n", config_fp);

		fclose(config_fp);
	}
	
	return EXIT_SUCCESS;
}

int
new_instance(char *dir)
/* Open DIR in a new instance of the program (using TERM, set in the config 
 * file, as terminal emulator) */
{
	if (!term) {
		fprintf(stderr, _("%s: Default terminal not set. Use the configuration "
				"file to set one\n"), PROGRAM_NAME);
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
		fprintf(stderr, _("%s: '%s': Error dequoting filename\n"), PROGRAM_NAME,
				dir);
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
		fprintf(stderr, _("%s: '%s': Not a directory\n"), PROGRAM_NAME, 
				deq_dir);
		free(self);
		free(deq_dir);
		return EXIT_FAILURE;
	}

	char *path_dir = (char *)NULL;

	if (*deq_dir != '/') {
		path_dir = (char *)xnmalloc(strlen(path) + strlen(deq_dir) + 2, 
									sizeof(char));
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
			tmp_cmd = (char **)xrealloc(tmp_cmd, (i + 4) * sizeof(char *));
			for (i = 0; tmp_term[i]; i++) {
				tmp_cmd[i] = (char *)xnmalloc(strlen(tmp_term[i]) + 1, 
											  sizeof(char));
				strcpy(tmp_cmd[i], tmp_term[i]);
				free(tmp_term[i]);
			}
			free(tmp_term);

			i = num - 1;
			tmp_cmd[i + 1] = (char *)xnmalloc(strlen(self) + 1, sizeof(char));
			strcpy(tmp_cmd[i + 1], self);
			tmp_cmd[i + 2] = (char *)xnmalloc(3, sizeof(char));
			strcpy(tmp_cmd[i + 2], "-p\0");
			tmp_cmd[i + 3] = (char *)xnmalloc(strlen(path_dir) + 1, 
											  sizeof(char));
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
		char *cmd[] = { term, "-e", self, "-p", path_dir, NULL };
		ret = launch_execve(cmd, BACKGROUND);
	}

	if (*deq_dir != '/')
		free(path_dir);
	free(deq_dir);
	free(self);

	if (ret != 0)
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
	history[current_hist_n] = (char *)xcalloc(cmd_len + 1, sizeof(char));
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

	if (*file == '~' && *(file + 1) == '/') {
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
		fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, file, strerror(errno));
		return EXIT_FAILURE;
	}
	
	if (access(rfile, F_OK|R_OK) != 0) {
		fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, rfile, strerror(errno));
		return EXIT_FAILURE;
	}
	
	FILE *fp = fopen(rfile, "r");
	if (!fp) {
		fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, rfile, strerror(errno));
		return EXIT_FAILURE;
	}
	
	FILE *config_fp = fopen(CONFIG_FILE, "a");
	if (!config_fp) {
		fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, CONFIG_FILE, 
				strerror(errno));
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

			/* If alias name conflicts with some internal command, skip it */
			char *alias_name = strbtw(line, ' ', '=');
			if (!alias_name)
				continue;

			if (is_internal_c(alias_name)) {
				fprintf(stderr, _("'%s': Alias conflicts with internal " 
						"command\n"), alias_name);
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
				fputs(line, config_fp);
			}
			else
				fprintf(stderr, _("'%s': Alias already exists\n"), alias_name);
			
			free(alias_name);
		}
	}

	free(line);

	fclose(fp);
	fclose(config_fp);
	
	/* No alias was found in FILE */
	if (alias_found == 0) {
		fprintf(stderr, _("%s: '%s': No alias found\n"), PROGRAM_NAME, rfile);
		return EXIT_FAILURE;
	}

	/* Aliases were found in FILE, but none was imported (either because they
	 * conflicted with internal commands or the alias already existed) */
	else if (alias_imported == 0) {
		fprintf(stderr, _("%s: No alias imported\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	/* If some alias was found and imported, print the corresponding message 
	 * and update the aliases array */
	if (alias_imported > 1)
		printf(_("%s: %zu aliases were successfully imported\n"), PROGRAM_NAME, 
			   alias_imported);
	else
		printf(_("%s: 1 alias was successfully imported\n"), PROGRAM_NAME);

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
/* Given this value: \xA0\xA1\xA1, return an array of integers with the 
 * integer values for A0, A1, and A2 respectivelly */
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
/* Decode the prompt string (encoded_prompt global variable) taken from the 
 * configuration file. Based on the decode_prompt_string function found in an 
 * old bash release (1.14.7). */
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
				/* Go back one char, so that we have "\x ... n", which is what
				 * the get_hex_num() requires */
				line--;
				/* get_hex_num returns an array on integers corresponding to
				 * the hex codes found in line up to the fisrt non-hex 
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
				/* Set the line pointer after the first non-hex expression to
				 * continue processing */
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
				temp = savestring(shell_name + 1, strlen(shell_name) - 1);
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
							temp = savestring(tmp_path, strlen(tmp_path));
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

			case '[': /* Begin a sequence of non-printing characters. Mostly
			used to add color sequences. Ex: \[\033[1;34m\] */
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
					result = (char *)xcalloc(result_len + 1, sizeof(char));
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
			result = (char *)xrealloc(result, (result_len + 2) * sizeof(char));
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
			puts(_("Usage: pf, prof, profile [set, add, del PROFILE]"));
		
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
		fprintf(stderr, _("%s: '%s': Profile already exists\n"), PROGRAM_NAME, 
				prof);
		return EXIT_FAILURE;
	}
	
	if (!home_ok) {
		fprintf(stderr, _("%s: '%s': Error creating profile: Home directory "
		"not found\n"), PROGRAM_NAME, prof);
		return EXIT_FAILURE;
	}
	
	size_t pnl_len = strlen(PNL);
	/* ### GENERATE PROGRAM'S CONFIG DIRECTORY NAME ### */
	char *NCONFIG_DIR = (char *)xcalloc(user_home_len + pnl_len 
										+ strlen(prof) + 11, sizeof(char));
	sprintf(NCONFIG_DIR, "%s/.config/%s/%s", user_home, PNL, prof);

	/* #### CREATE THE CONFIG DIR #### */
	char *tmp_cmd[] = { "mkdir", "-p", NCONFIG_DIR, NULL }; 
	int ret = launch_execve(tmp_cmd, FOREGROUND);
	if (ret != 0) {
		fprintf(stderr, _("%s: mkdir: '%s': Error creating configuration "
				"directory\n"), PROGRAM_NAME, NCONFIG_DIR);
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
					  "*.cfm=gedit;kate;pluma;mousepad;leafpad;nano;vim;"
					  "vi;emacs;ed\n", mime_fp);
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
				"startup\n#Ex:\n#echo -e \"%s, the anti-eye-candy/KISS file"
				"manager\"\n"), PROGRAM_NAME, PROGRAM_NAME);
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
		printf(_("%s: '%s': Profile succesfully created\n"), PROGRAM_NAME, 
			   prof);
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
		fprintf(stderr, _("%s: %s: No such profile\n"), PROGRAM_NAME, prof);
		return EXIT_FAILURE;
	}
	
	char *tmp = (char *)xcalloc(user_home_len + strlen(PNL) + strlen(prof)
								+ 11, sizeof(char));
	sprintf(tmp, "%s/.config/%s/%s", user_home, PNL, prof);
	
	char *cmd[] = { "rm", "-r", tmp, NULL };
	int ret = launch_execve(cmd, FOREGROUND);
	free(tmp);

	if (ret == 0) {
		printf(_("%s: '%s': Profile successfully removed\n"), PROGRAM_NAME,
			   prof);
		for (i = 0; profile_names[i]; i++)
			free(profile_names[i]);
		get_profile_names();
		return EXIT_SUCCESS;
	}
	
	fprintf(stderr, _("%s: '%s': Error removing profile\n"), PROGRAM_NAME,
			prof);
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
		fprintf(stderr, _("%s: '%s': No such profile\nTo add a new profile "
				"enter 'pf add PROFILE'\n"), PROGRAM_NAME, prof);
		return EXIT_FAILURE;
	}
	
	/* If changing to the current profile, do nothing */
	if ((strcmp(prof, "default") == 0 && !alt_profile)
	|| (alt_profile && strcmp(prof, alt_profile) == 0))
		return EXIT_SUCCESS;
	
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
	if (argc_bk > 1)
		external_arguments(argc_bk, argv_bk);
	
	/* Set up config files and variables */
	init_config();
	
	if (!config_ok)
		set_default_options();

	/* Check whether we have a working shell */
	if (access(sys_shell, X_OK) == -1) {
		_err('w', PRINT_PROMPT, _("%s: %s: System shell not found. Please edit "
			 "the configuration file to specify a working shell.\n"),
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
				_err('w', PRINT_PROMPT, _("%s: Error opening the history "
					 "file\n"),	PROGRAM_NAME);
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
/* Open a file according to the application associated to its MIME type or
 * extension. It also accepts the 'info' and 'edit' arguments, the former
 * providing MIME info about the corresponding file and the latter opening
 * the MIME list file */
{
	/* Check arguments */
	if (!args[1]) {
		puts(_("Usage: mm, mime [info ELN/FILENAME] [edit]"));
		return EXIT_SUCCESS;
	}

	/* Check the existence of the 'file' command. */
	char *file_path_tmp = (char *)NULL;
	if ((file_path_tmp = get_cmd_path("file")) == NULL) {
		fprintf(stderr, _("%s: 'file' command not found\n"), PROGRAM_NAME);
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
			/* Since this function is called by open_function, and since this
			 * latter prints an error message itself whenever the exit code
			 * of mime_open is EXIT_FAILURE, and since we don't want that
			 * message in this case, return -1 instead to prevent that
			 * message from being printed */
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
		fprintf(stderr, _("%s: Error getting mime-type\n"), PROGRAM_NAME);
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
		else
			fprintf(stderr, _("%s: No associated application found\n"), 
					PROGRAM_NAME);
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
	char *cmd[] = { app, file_path, NULL };
	int ret = launch_execve(cmd, strcmp(args[args_num - 1], "&") == 0 
							? BACKGROUND : FOREGROUND);
	
	free(file_path);
	free(app);

	return ret;
}

int
mime_import(char *file)
/* Import MIME definitions from the system into FILE. This function will only 
 * be executed if the MIME file is not found or when creating a new profile. 
 * Returns zero if some association is found in the system 'mimeapps.list' 
 * files, or one in case of error or no association found */
{

	/* Open the internal MIME file */
	FILE *mime_fp = fopen(file, "w");
	if (!mime_fp)
		return EXIT_FAILURE;
		
	/* If not in X, just specify a few basic associations to make sure that
	 * at least 'mm edit' will work ('vi' should be installed in almost any 
	 * Unix computer) */
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
	
	/* Check each mimeapps.list file and store its associations into FILE */
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
		while ((line_len = getline(&line, &line_size, sys_mime_fp)) > 0) {
			if (!da_found && strncmp(line, "[Default Applications]", 22) == 0) {
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
	
	/* Make sure there is an entry for text/plain and *.cfm files, so that at 
	 * least 'mm edit' will work. Gedit, kate, pluma, mousepad, and leafpad, 
	 * are the default text editors of Gnome, KDE, Mate, XFCE, and LXDE 
	 * respectivelly */
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
		return launch_execve(cmd, FOREGROUND);
	}
}

char *
get_app(char *mime, char *ext)
/* Get application associated to a given MIME filetype or file extension.
 * Returns the first matching line in the MIME file or NULL if none is found */
{
	if (!mime)
		return (char *)NULL;
	
	FILE *defs_fp = fopen(MIME_FILE, "r");

	if (!defs_fp) {
		fprintf(stderr, _("%s: '%s': Error opening file\n"), PROGRAM_NAME,
				MIME_FILE);
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
		found = mime_match = 0; /* Global variable to tell mime_open() if the 
		application is associated to the file's extension or MIME type*/
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
				/* Split the appplications line into substrings, if any */
				while (*tmp != 0x00 && *tmp != ';' && *tmp != '\n' 
				&& *tmp != '\'' && *tmp != '"')
					app[app_len++] = *(tmp++);
				
				while (*tmp == 0x20) /* Remove leading spaces */
					tmp++;
				if (app_len) {
					app[app_len] = 0x00;
					/* Check each application existence */
					char *file_path = (char *)NULL;
					/* If app contains spaces, the command to check is the
					 * string before the first space */
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
						/* If the app exists, break the loops and return it */
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
		fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, MIME_TMP_FILE, 
				strerror(errno));
		return (char *)NULL;
	}

	FILE *file_fp_err = fopen("/dev/null", "w");

	if (!file_fp_err) {
		fprintf(stderr, "%s: '/dev/null': %s\n", PROGRAM_NAME, strerror(errno));
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

	if (ret != 0)
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
 * messages. MSG_TYPE is one of: 'e', 'w', 'n', or zero (meaning this latter 
 * that no message mark (E, W, or N) will be added to the prompt). PROMPT tells 
 * whether to print the message immediately before the prompt or rather in place.
 * Based on littlstar's xasprintf implementation:
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
	/* ###### INITIALIZE READLINE (what a hard beast to tackle!!) #### */

	 /* Enable tab auto-completion for commands (in PATH) in case of first 
	 * entered string. The second and later entered string will be 
	 * autocompleted with paths instead, just like in Bash, or with
	 * listed filenames, in case of ELN's. I use a custom completion
	 * function to add command and ELN completion, since readline's 
	 * internal completer only performs path completion */

	/* Define a function for path completion.
	 * NULL means to use filename_entry_function (), the default filename
	 * completer. */
	rl_completion_entry_function = my_rl_path_completion;

	/* Pointer to alternative function to create matches.
	 * Function is called with TEXT, START, and END.
	 * START and END are indices in RL_LINE_BUFFER saying what the boundaries
	 * of TEXT are.
	 * If this function exists and returns NULL then call the value of
	 * rl_completion_entry_function to try to match, otherwise use the
	 * array of strings returned. */
	rl_attempted_completion_function = my_rl_completion;
	rl_ignore_completion_duplicates = 1;
	/* I'm using here a custom quoting function. If not specified, readline
	 * uses the default internal function. */
	rl_filename_quoting_function = my_rl_quote;
	/* Tell readline what char to use for quoting. This is only the the
	 * readline internal quoting function, and for custom ones, like the one 
	 * I use above. However, custom quoting functions, though they need to
	 * define their own quoting chars, won't be called at all if this
	 * variable isn't set. */
	rl_completer_quote_characters = "\"'";
	rl_completer_word_break_characters = " ";
	/* Whenever readline finds any of the following chars, it will call the
	 * quoting function */
	rl_filename_quote_characters = " \t\n\"\\'`@$><=,;|&{[()]}?!*^";
	/* According to readline documentation, the following string is
	 * the default and the one used by Bash: " \t\n\"\\'`@$><=;|&{(" */
	
	/* Executed immediately before calling the completer function, it tells
	 * readline if a space char, which is a word break character (see the 
	 * above rl_completer_word_break_characters variable) is quoted or not. 
	 * If it is, readline then passes the whole string to the completer 
	 * function (ex: "user\ file"), and if not, only wathever it found after 
	 * the space char (ex: "file") 
	 * Thanks to George Brocklehurst for pointing out this function:
	 * https://thoughtbot.com/blog/tab-completion-in-gnu-readline*/
	rl_char_is_quoted_p = quote_detector;
	/* This function is executed inmediately before path completion. So,
	 * if the string to be completed is, for instance, "user\ file" (see the 
	 * above comment), this function should return the dequoted string so
	 * it won't conflict with system filenames: you want "user file",
	 * because "user\ file" does not exist, and, in this latter case,
	 * readline won't find any matches */
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
/* Check cmd against a list of internal commands. Used by parse_input_str()
 * when running chained commands */
{
	const char *int_cmds[] = { "o", "open", "cd", "p", "pr", "prop", "t", "tr", 
						 "trash", "s", "sel", "rf", "refresh", "c", "cp",
					     "m", "mv", "bm", "bookmarks", "b", "back",
					     "f", "forth", "bh", "fh", "u", "undel", "untrash",
					     "s", "sel", "sb", "selbox", "ds", "desel", "rm",
					     "mkdir", "ln", "unlink", "touch", "r", "md", "l",
					     "p", "pr", "prop", "pf", "prof", "profile",
					     "mp", "mountpoints", "ext", "pg", "pager", "uc",
					     "unicode", "folders", "ff", "log", "msg", "messages",
					     "alias", "shell", "edit", "history", "hf", "hidden",
					     "path", "cwd", "splash", "ver", "version", "?",
					     "help", "cmd", "commands", "colors", "license",
					     "fs", "mm", "mime", "x", "n", "net", NULL };
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
/* Execute chained commands (cmd1;cmd2 and/or cmd1 && cmd2). The function is 
 * called by parse_input_str() if some non-quoted double ampersand or 
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
		fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, tmp, strerror(errno));
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
/* Get all substrings from STR using IFS as substring separator, and, if there
 * is a range, expand it. Returns an array containing all substrings in STR 
 * plus expandes ranges, or NULL if: STR is NULL or empty, STR contains only 
 * IFS(s), or in case of memory allocation error */
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
		while (*str != ifs && *str != 0x00 && length < sizeof(buf))
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
		else /* If there's nothing after last range, there's no next either */
			next = 0;

		/* Repopulate the original array with the expanded range and
		 * remaining strings */
		substr_n = k;
		for (j = 0; substr[j]; j++)
			free(substr[j]);
		substr = (char **)xrealloc(substr, (substr_n + 1) * sizeof(char *));
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
		dstr[len] = (char *)xcalloc(strlen(substr[i]) + 1, sizeof(char));
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
/* Check if STR has the format of a color code string (a number or a semicolon
 * list (max 12 fields) of numbers of at most 3 digits each). Returns 1 if true 
 * and 0 if false. */
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
	
	/* No digits at all, ending semicolon, more than eleven fields, or more 
	 * than three consecutive digits */
	if (!digits || digits > 3 || semicolon > 11)
		return 0;
		
	/* At this point, we have a semicolon separated string of digits (3
	 * consecutive max) with at most 12 fields. The only thing not validated 
	 * here are numbers themselves */

	return 1;
}

void
set_colors(void)
/* Open the config file, get values for filetype colors and copy these values
 * into the corresponnding filetype variable. If some value is not found, or 
 * if it's a wrong value, the default is set. */
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
		 * colors. In this way, files listed for TAB completion will use
		 * CliFM colors instead of system colors */
		 
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
				colors = (char **)xrealloc(colors, (words + 1) * sizeof(char *));
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
			colors = (char **)xrealloc(colors, (words + 1) * sizeof(char *));
			colors[words] = (char *)xcalloc(len + 1, sizeof(char));
			strcpy(colors[words++], buf);
		}

		if (buf) {
			free(buf);
			buf = (char *)NULL;
		}

		if (colors) {
			colors = (char **)xrealloc(colors, (words + 1) * sizeof(char *));
			colors[words] = (char *)NULL;
		}

		/* Set the color variables */
		for (i = 0; colors[i]; i++) {
			if (strncmp(colors[i], "di=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					/* zero the corresponding variable as a flag for the
					 * check after this for loop and to prepare the variable
					 * to hold the default color */
					memset(di_c, 0x00, MAX_COLOR);
				else
					snprintf(di_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);
			else if (strncmp(colors[i], "nd=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					memset(nd_c, 0x00, MAX_COLOR);
				else
					snprintf(nd_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);
			else if (strncmp(colors[i], "ed=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					memset(ed_c, 0x00, MAX_COLOR);
				else
					snprintf(ed_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);
			else if (strncmp(colors[i], "ne=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					memset(ne_c, 0x00, MAX_COLOR);
				else
					snprintf(ne_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);
			else if (strncmp(colors[i], "fi=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					memset(fi_c, 0x00, MAX_COLOR);
				else
					snprintf(fi_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);
			else if (strncmp(colors[i], "ef=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					memset(ef_c, 0x00, MAX_COLOR);
				else
					snprintf(ef_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);
			else if (strncmp(colors[i], "nf=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					memset(nf_c, 0x00, MAX_COLOR);
				else
					snprintf(nf_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);
			else if (strncmp(colors[i], "ln=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					memset(ln_c, 0x00, MAX_COLOR);
				else
					snprintf(ln_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);
			else if (strncmp(colors[i], "or=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					memset(or_c, 0x00, MAX_COLOR);
				else
					snprintf(or_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);
			else if (strncmp(colors[i], "ex=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					memset(ex_c, 0x00, MAX_COLOR);
				else
					snprintf(ex_c, MAX_COLOR-1, "\x1b[%sm", colors[i]+3);
			else if (strncmp(colors[i], "ee=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					memset(ee_c, 0x00, MAX_COLOR);
				else
					snprintf(ee_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);
			else if (strncmp(colors[i], "bd=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					memset(bd_c, 0x00, MAX_COLOR);
				else
					snprintf(bd_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);
			else if (strncmp(colors[i], "cd=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					memset(cd_c, 0x00, MAX_COLOR);
				else
					snprintf(cd_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);
			else if (strncmp(colors[i], "pi=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					memset(pi_c, 0x00, MAX_COLOR);
				else
					snprintf(pi_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);
			else if (strncmp(colors[i], "so=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					memset(so_c, 0x00, MAX_COLOR);
				else
					snprintf(so_c, MAX_COLOR-1, "\x1b[%sm", colors[i] + 3);
			else if (strncmp(colors[i], "su=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					memset(su_c, 0x00, MAX_COLOR);
				else
					snprintf(su_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);
			else if (strncmp(colors[i], "sg=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					memset(sg_c, 0x00, MAX_COLOR);
				else
					snprintf(sg_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);
			else if (strncmp(colors[i], "tw=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					memset(tw_c, 0x00, MAX_COLOR);
				else
					snprintf(tw_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);
			else if (strncmp(colors[i], "st=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					memset(st_c, 0x00, MAX_COLOR);
				else
					snprintf(st_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);
			else if (strncmp(colors[i], "ow=", 3) == 0)
				if (!is_color_code(colors[i]+3))
					memset(ow_c, 0x00, MAX_COLOR);
				else
					snprintf(ow_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);
			else if (strncmp(colors[i], "ca=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					memset(ca_c, 0x00, MAX_COLOR);
				else
					snprintf(ca_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);
			else if (strncmp(colors[i], "no=", 3) == 0) {
				if (!is_color_code(colors[i] + 3))
					memset(no_c, 0x00, MAX_COLOR);
				else
					snprintf(no_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);
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
	dir_counter = 1;
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
		sys_shell = (char *)xcalloc(strlen(FALLBACK_SHELL) + 1, sizeof(char));
		strcpy(sys_shell, FALLBACK_SHELL);
	}

	if (term)
		free(term);

	term = (char *)xcalloc(strlen(DEFAULT_TERM_CMD) + 1, sizeof(char));
	strcpy(term, DEFAULT_TERM_CMD);
	
	if (encoded_prompt)
		free(encoded_prompt);

	encoded_prompt = (char *)xcalloc(strlen(DEFAULT_PROMPT) + 1, sizeof(char));
	strcpy(encoded_prompt, DEFAULT_PROMPT);
}

char *
escape_str(char *str)
/* Take a string, supposedly a path or a filename, and returns the same
 * string escaped */
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
 * to know if it should perform additional expansions */
{
	const char *int_cmds[] = { "o", "open", "cd", "p", "pr", "prop", "t", "tr", 
							   "trash", "s", "sel", "mm", "mime", NULL };
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
	if (index > 0 && line[index-1] == '\\' 
	&& !quote_detector(line, index-1))
		return 1;

	return 0;
}

int
is_quote_char(char c)
/* Simply check a single chartacter (c) against the quoting characters list 
 * defined in the qc global array (which takes its values from 
 * rl_filename_quote_characters */
{
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
 * taking tab, new line char, and space as word delimiters, except when they
 * are preceded by a quote char (single or double quotes), in which case 
 * eveything after the first quote char is taken as one single string. It 
 * also allows escaping space to prevent word spliting. It returns an array 
 * of splitted strings (without leading and terminating spaces) or NULL if 
 * str is NULL or if no substring was found, i.e., if str contains only 
 * spaces. */
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
			if (str_len && *(str - 1) == '\\') {
				buf = (char *)xrealloc(buf, (buf_len + 1) * sizeof(char *));
				buf[buf_len++] = *str;
				break;
			}
			quote = *str;
			str++;
			while (*str && *str != quote) {
				if (is_quote_char(*str)) {
					buf = (char *)xrealloc(buf, (buf_len + 1) * sizeof(char *));
					buf[buf_len++] ='\\';
				}
				buf = (char *)xrealloc(buf, (buf_len + 1) * sizeof(char *));
				buf[buf_len++] = *(str++);
			}
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

		case '\t': /* TAB, new line char, and space are taken as word
		breaking characters */
		case '\n':
		case 0x20:
			if (str_len && *(str - 1) == '\\') {
				buf = (char *)xrealloc(buf, (buf_len + 1) * sizeof(char *));
				buf[buf_len++] = *str;
			}
			else {
				/* Add a terminating null byte */
				buf[buf_len] = 0x00;
				if (buf_len > 0) {
					substr = (char **)xrealloc(substr, (words + 1) 
											   * sizeof(char *));
					substr[words] = (char *)xcalloc( buf_len + 1, sizeof(char));
					strcpy(substr[words], buf);
					words++;
				}
				/* Clear te buffer to get a new string */
				memset(buf, 0x00, buf_len);
				buf_len = 0;
			}
			break;

		default:
			buf = (char *)xrealloc(buf, (buf_len + 1) * sizeof(char *));
			buf[buf_len++] = *str;
			break;
		}
		str++;
		str_len++;
	}
	
	/* The while loop stops when the null byte is reached, so that the last
	 * substring is not printed, but still stored in the buffer. Therefore,
	 * we need to add it, if not empty, to our subtrings array */
	buf[buf_len] = 0x00;
	if (buf_len > 0) {
		if (!words)
			substr = (char **)xcalloc(words + 1, sizeof(char *));
		else
			substr = (char **)xrealloc(substr, (words + 1) * sizeof(char *));
		substr[words] = (char *)xcalloc(buf_len + 1, sizeof(char));
		strcpy(substr[words], buf);
		words++;
	}

	free(buf);
	buf = (char *)NULL;

	if (words) {
		/* Add a final null string to the array */
		substr = (char **)xrealloc(substr, (words + 1) * sizeof(char *));
		substr[words] = (char *)NULL;
		args_n = words - 1;
		return substr;
	}
	else {
		args_n = 0; /* Just in case, but I think it's not needed */
		return (char **)NULL;
	}
}

void
print_license(void)
{
	time_t rawtime = time(NULL);
	struct tm *tm = localtime(&rawtime);

	printf(_("%s, 2017-%d, %s\n\n"
			 "This program is free software; you can redistribute it "
			 "and/or modify it under the terms of the GNU General Public "
			 "License (version 2 or later) as published by the Free Software "
			 "Foundation.\n\nThis program is distributed in the hope that it "
			 "will be useful, but WITHOUT ANY WARRANTY; without even the "
			 "implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR "
			 "PURPOSE. See the GNU General Public License for more details.\n\n"
			 "You should have received a copy of the GNU General Public License "
			 "along with this program. If not, write to the Free Software "
			 "Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA "
			 "02110-1301, USA, or visit <http://www.gnu.org/licenses/>\n\n"
			 "NOTE: For more information about the meaning of 'free software' "
			 "run 'fs'.\n"), PROGRAM_NAME, tm->tm_year + 1900, AUTHOR);
}


size_t
u8_xstrlen(const char *str)
/* An strlen implementation able to handle unicode characters. Taken from: 
* https://stackoverflow.com/questions/5117393/number-of-character-cells-used-by-string
* Explanation: strlen() counts bytes, not chars. Now, since ASCII chars take 
* each 1 byte, the amount of bytes equals the amount of chars. However, 
* non-ASCII or wide chars are multibyte chars, that is, one char takes more than 
* 1 byte, and this is why strlen() does not work as expected for this kind of 
* chars: a 6 chars string might take 12 or more bytes */
{
	size_t len = 0;
	cont_bt = 0;

	while (*(str++))
		if ((*str & 0xc0) != 0x80) /* Do not count continuation bytes (used 
		* by multibyte, that is, wide or non-ASCII characters) */
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

	char *deq_path = dequote_str(cmd[1], 0);

	if (!deq_path) {
		fprintf(stderr, _("%s: '%s': Error dequoting filename\n"), 
				PROGRAM_NAME, cmd[1]);
		return EXIT_FAILURE;
	}
	
	/* Check file existence */
	struct stat file_attrib;

	if (lstat(deq_path, &file_attrib) == -1) {
		fprintf(stderr, "%s: open: '%s': %s\n", PROGRAM_NAME, deq_path, 
				strerror(errno));
		free(deq_path);
		return EXIT_FAILURE;
	}
	
	/* Check file type: only directories, symlinks, and regular files will
	 * be opened */

	char *linkname = (char *)NULL, file_tmp[PATH_MAX] = "", is_link = 0, 
		  no_open_file = 1, file_type[128] = "";
		 /* Reserve a good amount of bytes for filetype: it cannot be known
		  * beforehand how many bytes the TRANSLATED string will need */

	switch (file_attrib.st_mode & S_IFMT) {
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
		free(deq_path);
		return (cd_function(cmd[1]));

	/* If a symlink, find out whether it is a symlink to dir or to file */
	case S_IFLNK:
		linkname = realpath(deq_path, NULL);
		if (!linkname) {
			if (errno == ENOENT)
				fprintf(stderr, _("%s: open: '%s': Broken link\n"), 
							PROGRAM_NAME, deq_path);
			else
				fprintf(stderr, "%s: open: '%s': %s\n", PROGRAM_NAME,
						deq_path, strerror(errno));

			free(deq_path);
			return EXIT_FAILURE;
		}
		stat(linkname, &file_attrib);
		switch (file_attrib.st_mode & S_IFMT) {
		/* Realpath() will never return a symlink, but an absolute 
		 * path, so that there is no need to check for symlinks */
		case S_IFBLK:
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
			free(deq_path);
			int ret = cd_function(linkname);
			free(linkname);
			return ret;
		case S_IFREG:
		/* Set the no_open_file flag to false, since regular files) will 
		 * be opened */
			no_open_file = 0;
			strncpy(file_tmp, linkname, PATH_MAX);
			break;
		default:
			strcpy(file_type, _("unknown file type"));
				break;
		}
		break;

	case S_IFREG:
		no_open_file = 0;
		break;
	
	default:
		strcpy(file_type, _("unknown file type"));
		break;
	}

	/* If neither directory nor regular file nor symlink (to directory or 
	 * regular file), print the corresponding error message and exit */
	if (no_open_file) {
		if (linkname) {
			fprintf(stderr, _("%s: %s -> '%s' (%s): Cannot open file. Try "
							  "'APPLICATION FILENAME'.\n"), PROGRAM_NAME, 
							  deq_path, linkname, file_type);
			free(linkname);
			linkname = (char *)NULL;
		}
		else
			fprintf(stderr, _("%s: '%s' (%s): Cannot open file. Try "
					"'APPLICATION FILENAME'.\n"), PROGRAM_NAME, deq_path, 
					file_type);
		free(deq_path);
		return EXIT_FAILURE;
	}
	
	if (linkname) {
		free(linkname);
		linkname = (char *)NULL;
	}

	/* At this point we know the file to be openend is either a regular file 
	 * or a symlink to a regular file. So, just open the file */

	if (!cmd[2] || strcmp(cmd[2], "&") == 0) {
		free(deq_path);
		if (!(flags & FILE_CMD_OK)) {
			fprintf(stderr, _("%s: 'file' command not found. Specify an "
							  "application to open the file\nUsage: "
							  "open ELN/FILENAME [APPLICATION]\n"), 
							  PROGRAM_NAME);
			return EXIT_FAILURE;
		}
		else {
			int ret = mime_open(cmd);
			/* The return value of mime_open could be zero (EXIT_SUCCESS), if 
			 * success, one (EXIT_FAILURE) if error (in which case the following 
			 * error message should be printed), and -1 if no access permission, 
			 * in which case no error message should be printed, since 
			 * the corresponding message is printed by mime_open itself */
			if (ret == EXIT_FAILURE) {
				fputs("Try 'open FILE APPLICATION'\n", stderr);
				return EXIT_FAILURE;
			}
			return EXIT_SUCCESS;
		}
	}

	/* If some application was specified to open the file */
	char *tmp_cmd[] = { cmd[2], (is_link) ? file_tmp : deq_path, NULL };

	int ret = launch_execve(tmp_cmd, (cmd[args_n] 
							&& strcmp(cmd[args_n], "&") == 0) ? BACKGROUND 
							: FOREGROUND);

	free(deq_path);

	if (ret != 0)
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
				fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, user_home, 
						strerror(errno));				
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
	
	/* If we have some argument, resolve it with realpath(), cd into the
	 * resolved path, and set the path variable to this latter */
	else {
		
		char *real_path = realpath(deq_path, NULL);
		
		if (!real_path) {
			fprintf(stderr, "%s: cd: '%s': %s\n", PROGRAM_NAME, deq_path,
					strerror(errno));
			if (dequoted_p)
				free(deq_path);
			return EXIT_FAILURE;
		}
		
		/* Execute permission is enough to access a directory, but not to
		 * list its content; for this we need read permission as well. So,
		 * without the read permission check, chdir() below will be successfull,
		 * but CliFM will be nonetheless unable to list the content of the
		 * directory */
		if (access(real_path, R_OK) != 0) {
			fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, real_path, 
					strerror(errno));
			if (dequoted_p)
				free(deq_path);
			free(real_path);
			return EXIT_FAILURE;			
		}

		int ret = chdir(real_path);
		if (ret != 0) {
			fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, real_path, 
					strerror(errno));
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
		path = (char *)xcalloc(strlen(old_pwd[dirhist_cur_index - 1]) + 1, 
							   sizeof(char));
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
		path = (char *)xcalloc(strlen(old_pwd[dirhist_cur_index]) + 1, 
							   sizeof(char));
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
		fprintf(stderr, "%s: mp: '/proc/mounts': %s\n", PROGRAM_NAME, 
				strerror(errno));
		return EXIT_FAILURE;
	}
	
	FILE *mp_fp = fopen("/proc/mounts", "r");
	if (mp_fp) {
		printf(_("%sMountpoints%s\n\n"), white, NC);
		/* The line variable should be able to store the device name,
		 * the mount point (PATH_MAX) and mount options. PATH_MAX*2 
		 * should be more than enough */
		char **mountpoints = (char **)NULL;
		size_t mp_n = 0; 
		int exit_status = 0;

		size_t line_size = 0;
		char *line = (char *)NULL;
		ssize_t line_len = 0;
		
		while ((line_len = getline(&line, &line_size, mp_fp)) > 0) {
			/* Do not list all mountpoints, but only those corresponding to 
			 * a block device (/dev) */
			if (strncmp (line, "/dev/", 5) == 0) {
				char *str = (char *)NULL;
				size_t counter = 0;
				/* use strtok() to split LINE into tokens using space as
				 * IFS */
				str = strtok(line, " ");
				size_t dev_len = strlen(str);
				char *device = (char *)xnmalloc(dev_len + 1, sizeof(char));
				strcpy(device, str);
				/* Print only the first two fileds of each /proc/mounts line */
				while (str && counter < 2) {
					if (counter == 1) { /* 1 == second field */
						printf("%s%ld%s %s%s%s (%s)\n", eln_color, mp_n + 1, NC, 
							   (access(str, R_OK|X_OK) == 0) ? blue : red, 
							   str, default_color, device);
						/* Store the second field (mountpoint) into an
						 * array */
						mountpoints = (char **)xrealloc(mountpoints, 
											   (mp_n + 1) * sizeof(char *));
						mountpoints[mp_n] = (char *)xcalloc(strlen(str) + 1, 
												    sizeof(char));					
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
					exit_status = 1;
				}
			}
			else {
				printf(_("mp: '%s': Invalid mountpoint\n"), input);
				exit_status = 1;
			}
		}
		
		/* Free stuff and exit */
		if (input)
			free(input);

		size_t i;
		for (i = 0; i < (size_t)mp_n; i++) {
			free(mountpoints[i]);
		}

		free(mountpoints);

		return exit_status;
	}

	else { /* If fopen failed */
		fprintf(stderr, "%s: mp: fopen: '/proc/mounts': %s\n", PROGRAM_NAME, 
				strerror(errno));
		return EXIT_FAILURE;
	}
}

int
get_max_long_view(void)
/* Returns the max length a filename can have in long view mode */
{
	/* 70 is approx the length of the properties string (less filename). So, 
	 * the amount of current terminal columns less 70 should be the space 
	 * left to print the filename, that is, the max filename length */
	int max = (term_cols - 70);

	/* Do not allow max to be less than 20 (this is a more or less arbitrary
	 * value), specially because the result of the above operation could be 
	 * negative */
	if (max < 20) max = 20;

	if ((int)longest < max)
		max = (int)longest;
	/* Should I specify a max length for trimmed filenames ?
	if (max > 50) max=50; */

	return max;
}

void
log_msg(char *_msg, int print)
/* Handle the error message 'msg'. Store 'msg' in an array of error messages, 
 * write it into an error log file, and print it immediately (if print is 
 * zero (NOPRINT_PROMPT) or tell the next prompt, if print is one to do it 
 * (PRINT_PROMPT)). Messages wrote to the error log file have the following 
 * format: "[date] msg", where 'date' is YYYY-MM-DDTHH:MM:SS */
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
	
	/* If the config dir cannot be found or if msg log file isn't set yet... 
	 * This will happen if an error occurs before running init_config(), for 
	 * example, if the user's home cannot be found */
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
		strftime(date, sizeof(date), "%c", tm);
		fprintf(msg_fp, "[%d-%d-%dT%d:%d:%d] ", tm->tm_year + 1900, 
				tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, 
				tm->tm_sec);
		fputs(_msg, msg_fp);
		fclose(msg_fp);
	}
}

int *
expand_range(char *str, int listdir)
/* Expand a range of numbers given by str. It will expand the range provided 
 * that both extremes are numbers, bigger than zero, equal or smaller than the 
 * amount of files currently listed on the screen, and the second (right) 
 * extreme is bigger than the first (left). Returns an array of int's with the 
 * expanded range or NULL if one of the above conditions is not met */
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
/* Recursively check directory permissions (write and execute). Returns zero 
 * if OK, and one if at least one subdirectory does not have write/execute 
 * permissions */
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
				  * Otherwise, since this function calls itself recursivelly,
				  * the flag would be reset upon every new call, without
				  * preserving the error code, which is what the flag is
				  * aimed to do. On the other side, if I use a local static 
				  * variable for this flag, it will never drop the error 
				  * value, and all subsequent calls to the function will 
				  * allways return error (even if there's no actual error) */
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
/* Check whether the current user has enough permissions (write, execute) to 
 * modify the contents of the parent directory of 'file'. 'file' needs to be 
 * an absolute path. Returns zero if yes and one if no. Useful to know if a 
 * file can be removed from or copied into the parent. In case FILE is a 
 * directory, the function checks all its subdirectories for appropriate 
 * permissions, including the immutable bit */
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
		/* strbfrlst() will return NULL if file's parent is root (/), simply
		 * because in this case there's nothing before the last slash. 
		 * So, check if file's parent dir is root */
		if (file[0] == '/' && strcntchr(file + 1, '/') == -1) {
			parent = (char *)xnmalloc(2, sizeof(char));
			parent[0] = '/';
			parent[1] = 0x00;
		}
		else {
			fprintf(stderr, _("%s: '%s': Error getting parent directory\n"), 
					PROGRAM_NAME, file);
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
				 * itself*/
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
					fprintf(stderr, _("'%s': Permission denied\n"), file);
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
			/* Error message is printed by check_immutable_bit() itself */
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
	 * (original_filename_len + 1 (dot) + suffix_len) won't be longer than
	 * NAME_MAX */
	size_t filename_len = strlen(filename), suffix_len = strlen(suffix);
	int size = (int)(filename_len + suffix_len + 1) - NAME_MAX;

	if (size > 0) {
		/* If SIZE is a positive value, that is, the trashed file name 
		 * exceeds NAME_MAX by SIZE bytes, reduce the original file name 
		 * SIZE bytes. Terminate the original file name (FILENAME) with a
		 * tilde (~), to let the user know it is trimmed */
		filename[filename_len - (size_t)size - 1] = '~';
		filename[filename_len - (size_t)size] = 0x00;
	}

											/* 2 = dot + null byte */
	size_t file_suffix_len = filename_len + suffix_len + 2;
	char *file_suffix = (char *)xnmalloc(file_suffix_len, sizeof(char));
	/* No need for memset. sprintf adds the terminating null byte by itself */
	sprintf(file_suffix, "%s.%s", filename, suffix);

	/* Copy the original file into the trash files directory */
	char *dest = (char *)NULL;
	dest = (char *)xnmalloc(strlen(TRASH_FILES_DIR) + strlen(file_suffix) + 2, 
					       sizeof(char));
	sprintf(dest, "%s/%s", TRASH_FILES_DIR, file_suffix);

	char *tmp_cmd[] = { "cp", "-a", file, dest, NULL };

	free(filename);

	ret = launch_execve(tmp_cmd, FOREGROUND);
	free(dest);
	dest = (char *)NULL;

	if (ret != 0) {
		fprintf(stderr, _("%s: trash: '%s': Failed copying file to Trash\n"), 
				PROGRAM_NAME, file);
		free(file_suffix);
		return EXIT_FAILURE;
	}

	/* Generate the info file */
	size_t info_file_len = strlen(TRASH_INFO_DIR) + strlen(file_suffix) + 12;
	char *info_file = (char *)xnmalloc(info_file_len, sizeof(char));
	sprintf(info_file, "%s/%s.trashinfo", TRASH_INFO_DIR, file_suffix);

	FILE *info_fp = fopen(info_file, "w");
	if (!info_fp) { /* If error creating the info file */
		fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, info_file, 
				strerror(errno));
		/* Remove the trash file */
		char *trash_file = (char *)NULL;
		trash_file = (char *)xnmalloc(strlen(TRASH_FILES_DIR) 
									  + strlen(file_suffix) + 2, sizeof(char));
		sprintf(trash_file, "%s/%s", TRASH_FILES_DIR, file_suffix);
		char *tmp_cmd2[] = { "rm", "-r", trash_file, NULL };
		ret = launch_execve (tmp_cmd2, FOREGROUND);
		free(trash_file);
		if (ret != 0)
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
	if (ret != 0) {
		fprintf(stderr, _("%s: trash: '%s': Failed removing file\n"), 
				PROGRAM_NAME, file);
		char *trash_file = (char *)NULL;
		trash_file = (char *)xnmalloc(strlen(TRASH_FILES_DIR) 
									 + strlen(file_suffix) + 2, sizeof(char));
		sprintf(trash_file, "%s/%s", TRASH_FILES_DIR, file_suffix);

		char *tmp_cmd4[] = { "rm", "-r", trash_file, info_file, NULL };
		ret = launch_execve(tmp_cmd4, FOREGROUND);
		free(trash_file);

		if (ret != 0) {
			fprintf(stderr, _("%s: trash: Failed removing temporary files "
							  "from Trash.\nTry removing them manually\n"), 
							  PROGRAM_NAME);
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
	int files_n = scandir(TRASH_FILES_DIR, &trash_files, skip_implied_dot, 
						  (unicode) ? alphasort : (case_sensitive) ? xalphasort 
						  : alphasort_insensitive);
	
	if (files_n) {
		printf(_("%sTrashed files%s%s\n\n"), white, NC, default_color);
		for (i = 0; i < (size_t)files_n; i++)
			colors_list(trash_files[i]->d_name, (int)i + 1, 0, 1);
	}
	else {
		puts(_("trash: There are no trashed files"));
		if (chdir(path) == -1) { /* Restore CWD and return */
			_err(0, NOPRINT_PROMPT, "%s: trash: '%s': %s\n", PROGRAM_NAME, 
				 path, strerror(errno));
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
		line = rl_no_hist(_("Element(s) to be removed (ex: 1 2-6, or *): "));

	rm_elements = get_substr(line, 0x20);
	free(line);

	if (!rm_elements)
		return EXIT_FAILURE;

	/* Remove files */
	char rm_file[PATH_MAX] = "", rm_info[PATH_MAX] = "";
	int ret = -1, exit_status = 0;
	
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
				snprintf(rm_info, PATH_MAX, "%s/%s.trashinfo", TRASH_INFO_DIR, 
						 trash_files[j]->d_name);
				char *tmp_cmd[] = { "rm", "-r", rm_file, rm_info, NULL };
				ret = launch_execve(tmp_cmd, FOREGROUND);
				
				if (ret != 0) {
					fprintf(stderr, _("%s: trash: Error trashing %s\n"), 
							 PROGRAM_NAME, trash_files[j]->d_name);
					exit_status = 1;
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

			fprintf(stderr, _("%s: trash: '%s': Invalid ELN\n"), PROGRAM_NAME, 
					rm_elements[i]);
			exit_status = 1;

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
			fprintf(stderr, _("%s: trash: '%d': Invalid ELN\n"), PROGRAM_NAME, 
					rm_num);
			free(rm_elements[i]);
			exit_status = 1;
			continue;
		}

		snprintf(rm_file, PATH_MAX, "%s/%s", TRASH_FILES_DIR, 
				 trash_files[rm_num - 1]->d_name);
		snprintf(rm_info, PATH_MAX, "%s/%s.trashinfo", TRASH_INFO_DIR, 
				 trash_files[rm_num - 1]->d_name);
		char *tmp_cmd2[] = { "rm", "-r", rm_file, rm_info, NULL };
		ret = launch_execve(tmp_cmd2, FOREGROUND);
		
		if (ret != 0) {
			fprintf(stderr, _("%s: trash: Error trashing %s\n"), PROGRAM_NAME, 
					trash_files[rm_num - 1]->d_name);
			exit_status = 1;
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
	snprintf(undel_info, PATH_MAX, "%s/%s.trashinfo", TRASH_INFO_DIR, file);

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
			 * because there's nothing before last slash in this case). So, 
			 * check if file's parent is root. Else returns */
			if (url_decoded[0] == '/' && strcntchr(url_decoded + 1, '/') == -1) {
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
			fprintf(stderr, _("%s: undel: '%s': No such file or directory\n"), 
					PROGRAM_NAME, parent);
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

		if (ret == 0) {
			char *tmp_cmd2[] = { "rm", "-r", undel_file, undel_info, NULL };
			ret = launch_execve(tmp_cmd2, FOREGROUND);
			if (ret != 0) {
				fprintf(stderr, _("%s: undel: '%s': Failed removing info "
								  "file\n"), PROGRAM_NAME, undel_info);
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
						  "Try restoring the file manually\n"), PROGRAM_NAME, 
						  file);
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
	int trash_files_n = scandir(TRASH_FILES_DIR, &trash_files, skip_implied_dot, 
							    (unicode) ? alphasort : (case_sensitive) 
							     ? xalphasort : alphasort_insensitive);
	if (trash_files_n <= 0) {
		puts(_("trash: There are no trashed files"));
		if (chdir(path) == -1) {
			_err(0, NOPRINT_PROMPT, "%s: undel: '%s': %s\n", PROGRAM_NAME, path, 
				 strerror(errno));
			return EXIT_FAILURE;
		}
		return EXIT_SUCCESS;
	}

	int exit_status = 0;
	/* if "undel all" (or "u a" or "u *") */
	if (comm[1] && (strcmp(comm[1], "*") == 0|| strcmp(comm[1], "a") == 0 
	|| strcmp(comm[1], "all") == 0)) {
		size_t j;
		for (j = 0; j < (size_t)trash_files_n; j++) {
			if (untrash_element(trash_files[j]->d_name) != 0)
				exit_status = 1;
			free(trash_files[j]);
		}
		free(trash_files);

		if (chdir(path) == -1) {
			_err(0, NOPRINT_PROMPT, "%s: undel: '%s': %s\n", PROGRAM_NAME, path, 
				 strerror(errno));
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
		_err(0, NOPRINT_PROMPT, "%s: undel: '%s': %s\n", PROGRAM_NAME, path, 
			 strerror(errno));
		return EXIT_FAILURE;
	}

	/* Get user input */
	printf(_("\n%s%sEnter 'q' to quit.\n"), NC, default_color);
	int undel_n = 0;
	char *line = (char *)NULL, **undel_elements = (char **)NULL;
	while (!line)
		line = rl_no_hist(_("Element(s) to be undeleted (ex: 1 2-6, or *): "));

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
					exit_status = 1;
			free_and_return = 1;
		}
		else if (!is_number(undel_elements[i])) {
			fprintf(stderr, _("undel: '%s': Invalid ELN\n"), 
					undel_elements[i]);
			exit_status = 1;
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
			fprintf(stderr, _("%s: undel: '%d': Invalid ELN\n"), PROGRAM_NAME, 
					undel_num);
			free(undel_elements[i]);
			continue;
		}
		
		/* If valid ELN */
		if (untrash_element(trash_files[undel_num - 1]->d_name) != 0)
			exit_status = 1;
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
	int files_n = -1, exit_status = 0;

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

		if (ret != 0) {
			fprintf(stderr, _("%s: trash: '%s': Error removing trashed file\n"), 
					PROGRAM_NAME, trash_files[i]->d_name);
			exit_status = 1; /* If there is at least one error, return error */
		}
		
		free(info_file);
		free(trash_files[i]);
	}
	free(trash_files);

	if (chdir(path) == -1) {
		_err(0, NOPRINT_PROMPT, "%s: trash: '%s': %s\n", PROGRAM_NAME, path, 
			 strerror(errno));
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
		if (ret != 0) {
			_err(0, NOPRINT_PROMPT, _("%s: mkdir: '%s': Error creating trash "
				 "directory\n"), PROGRAM_NAME, TRASH_DIR);
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
			_err(0, NOPRINT_PROMPT, "%s: trash: '%s': %s\n", PROGRAM_NAME, 
				 TRASH_FILES_DIR, strerror(errno));
			return EXIT_FAILURE;
		}
		
		struct dirent **trash_files = (struct dirent **)NULL;
		int files_n = scandir(TRASH_FILES_DIR, &trash_files, skip_implied_dot, 
							  (unicode) ? alphasort : 
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
			_err(0, NOPRINT_PROMPT, "%s: trash: '%s': %s\n", PROGRAM_NAME, path, 
				 strerror(errno));
			return EXIT_FAILURE;
		}
		else
			return EXIT_SUCCESS;
	}

	else {
		/* Create suffix from current date and time to create unique 
		 * filenames for trashed files */
		int exit_status = 0;
		time_t rawtime = time(NULL);
		struct tm *tm = localtime(&rawtime);
		char date[64] = "";
		strftime(date, sizeof(date), "%c", tm);
		char suffix[68] = "";
		snprintf(suffix, 67, "%d%d%d%d%d%d", tm->tm_year + 1900, tm->tm_mon + 1, 
				tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
		
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
					snprintf(tmp_comm, PATH_MAX, "%s/%s", path, deq_file);
				}

				/* Some filters: you cannot trash wathever you want */
				/* Do not trash any of the parent directories of TRASH_DIR,
				 * that is, /, /home, ~/, ~/.local, ~/.local/share */
				if (strncmp(tmp_comm, TRASH_DIR, strlen(tmp_comm)) == 0) {
					fprintf(stderr, _("trash: Cannot trash '%s'\n"), 
							tmp_comm);
					exit_status = 1;
					free(deq_file);
					continue;
				}
				/* Do no trash TRASH_DIR itself nor anything inside it, that 
				 * is, already trashed files */
				else if (strncmp(tmp_comm, TRASH_DIR, 
						 strlen(TRASH_DIR)) == 0) {
					puts(_("trash: Use 'trash del' to remove trashed "
							 "files"));
					exit_status = 1;
					free(deq_file);
					continue;
				}
				struct stat file_attrib;
				if (lstat(deq_file, &file_attrib) == -1) {
					fprintf(stderr, _("trash: '%s': %s\n"), deq_file, 
							strerror(errno));
					exit_status = 1;
					free(deq_file);
					continue;
				}
				/* Do not trash block or character devices */
				else {
					if ((file_attrib.st_mode & S_IFMT) == S_IFBLK) {
						fprintf(stderr, _("trash: '%s': Cannot trash a "
								"block device\n"), deq_file);
						exit_status = 1;
						free(deq_file);
						continue;
					}
					else if ((file_attrib.st_mode & S_IFMT) == S_IFCHR) {
						fprintf(stderr, _("trash: '%s': Cannot trash a "
								"character device\n"), deq_file);
						exit_status = 1;
						free(deq_file);
						continue;
					}
				}
				
				/* Once here, everything is fine: trash the file */
				exit_status = trash_element(suffix, tm, deq_file);
				/* The trash_element() function will take care of printing
				 * error messages, if any */
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
	/* Do not add anything if new path equals last entry in directory history.
	 * Just update the current dihist index to reflect the path change */
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
 * C-v, C-right arrow gives "[[1;5C", which here should be written like this:
 * "\\x1b[1;5C" */

/* These keybindings work on all the terminals I tried: the linux built-in
 * console, aterm, urxvt, xterm, lxterminal, xfce4-terminal, gnome-terminal,
 * terminator, and st (with some patches, however, they might stop working in
 * st) */

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
	int status = 0;
	static short long_status = 0;

	/* Disable all keybindings while in the bookmarks or mountpoints screen */
	if (kbind_busy)
		return 0;

	switch (key) {

	/* C-r: Refresh the screen */
	case 18:
		keybind_exec_cmd("rf");
		break;

	/* A-a: Select all files in CWD */
	case 97:
		keybind_exec_cmd("sel * .*");
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

	/* A-c: Clear the current command line (== C-a, C-k). Very handy, 
	 * since C-c is currently disabled */
	case 99:
		puts("");
		rl_replace_line("", 0);
/*		puts("");
		rl_delete_text(0, rl_end);
		rl_end = rl_point = 0;
		rl_on_new_line(); */
		break;

	/* A-d: Deselect all selected files */		
	case 100:
		keybind_exec_cmd("ds a");
		break;

	/* A-e: Change CWD to the HOME directory */
	case 101:
		/* If already in the home dir, do nothing */
		if (strcmp(path, user_home) == 0)
			return 0;
		keybind_exec_cmd("cd");
		break;
		
	/* A-f: Toggle folders first on/off */
	case 102:
		status = list_folders_first;
		if (list_folders_first)
			list_folders_first = 0;
		else
			list_folders_first = 1;
		
		if (status != list_folders_first) {
			while (files--)
				free(dirlist[files]);
			/* Without this puts(), the first entries of the directories
			 * list are printed in the prompt line */
			puts("");
			list_dir();
		}
		break;

	/* A-h: Change CWD to PREVIOUS directoy in history */
	case 104: /* Same as C-Down arrow (66) */
		/* If at the beginning of dir hist, do nothing */
		if (dirhist_cur_index == 0)
			return 0;
		keybind_exec_cmd("back");
		break;
	
	/* A-i: Toggle hidden files on/off */
	case 105:
		status = show_hidden;
		if (show_hidden)
			show_hidden = 0;
		else
			show_hidden = 1;
		
		if (status != show_hidden) {
			while (files--)
				free(dirlist[files]);
			puts("");
			list_dir();
		}
		break;

	/* A-j: Change CWD to PREVIOUS directoy in history */
	case 106: 
		/* If already at the beginning of dir hist, do nothing */
		if (dirhist_cur_index == 0)
			return 0;
		keybind_exec_cmd("back");
		break;
		
	/* A-k: Change CWD to NEXT directory in history */
	case 107:
		/* If already at the end of dir hist, do nothing */
		if (dirhist_cur_index + 1 == dirhist_total_index)
			return 0;
		keybind_exec_cmd("forth");
		break;

	/* A-l: Toggle long view mode on/off */
	case 108:
		if(!long_status)
			/* If status == 0 set it to 1. In this way, the next time
			 * this function is called it will not be true, and the else
			 * clause will be executed instead */
			long_view=long_status = 1;
		else
			long_view = long_status = 0;
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

	/* A-u: Change CWD to PARENT directory */
	case 117:
		/* If already root dir, do nothing */
		if (strcmp(path, "/") == 0)
			return 0;
		keybind_exec_cmd("cd ..");
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

	size_t str_len = strlen(str), i;

	for (i = 0; i < str_len; i++) {
		if (str[i] == c) {
			size_t index = i + 1, j = 0;
			if (i == (str_len - 1))
				return NULL;
			char *buf = (char *)NULL;
			buf = (char *)xnmalloc((str_len - index) + 1, sizeof(char));	
			for (i = index; i < str_len; i++) {
				if (str[i] != '"' && str[i] != '\'' && str[i] != '\\' 
				&& str[i] != 0x00) {
					buf[j++] = str[i];
				}
			}
			buf[j] = 0x00;
			return buf;
		}
	}
	
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
		fprintf(stderr, _("%s: Error getting variable name\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}
	
	if (!value) {
		free(name);
		fprintf(stderr, _("%s: Error getting variable value\n"), PROGRAM_NAME);
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
free_stuff (void)
{
	size_t i = 0;

	free(TMP_DIR);
	
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
			_err(0, NOPRINT_PROMPT, _("%s: %s failed to allocate %zu bytes\n"), 
				 PROGRAM_NAME, __func__, size);
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
			_err(0, NOPRINT_PROMPT, _("%s: %s failed to allocate %zu bytes\n"), 
				 PROGRAM_NAME, __func__, nmemb * size);
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
{
	file_cmd_path = get_cmd_path("file");
	if (!file_cmd_path) {
		flags &= ~FILE_CMD_OK;
		_err('n', PRINT_PROMPT, _("%s: 'file' command not found. Specify an "
			 "application when opening files. Ex: 'o 12 nano' or just 'nano "
			 "12'\n"), PROGRAM_NAME);
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
 * SIGCHLD is the signal sent to the parent process when the child terminates, 
 * for example, when using fork() to create a new process (child). Without 
 * this signal, waitpid (or wait) always fails (-1 error) and you cannot get 
 * to know the status of the child.
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
	/* isatty() returns 1 if the file descriptor, here STDIN_FILENO, outputs 
	 * to a terminal (is interactive). Otherwise, it returns zero */
	
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
			/* Without the setpgid line below, the program cannot be run with 
			* sudo, but it can be run nonetheless by the root user */
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
//handle signals catched by the signal function. In this case, the function 
//just prompts a new line. If trap signals are not enabled, kill the current 
//foreground process (pid)
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
/* Get the list of files in PATH, plus CliFM internal commands, and send them 
 * into an array to be read by my readline custom auto-complete function 
 * (my_rl_completion) */
{
	struct dirent ***commands_bin = (struct dirent ***)xnmalloc(path_n, 
													sizeof(struct dirent));
	size_t i, j, k, l = 0, total_cmd = 0;
	int *cmd_n = (int *)xnmalloc(path_n, sizeof(int));

	for (k = 0; k < path_n; k++) {
		cmd_n[k] = scandir(paths[k], &commands_bin[k], NULL, xalphasort);
		/* If paths[k] directory does not exist, scandir returns -1. Fedora,
		 * for example, adds $HOME/bin and $HOME/.local/bin to PATH disregarding
		 * if they exist or not. If paths[k] dir is empty do not use it 
		 * either */
		if (cmd_n[k] > 0)
			total_cmd += (size_t)cmd_n[k];
	}

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
		if (cmd_n[i] > 0) {
			for (j = 0; j < (size_t)cmd_n[i]; j++) {
				bin_commands[l] = (char *)xnmalloc(
											strlen(commands_bin[i][j]->d_name) 
											+ 1, sizeof(char));
				strcpy(bin_commands[l++], commands_bin[i][j]->d_name);
				free(commands_bin[i][j]);
			}
			free(commands_bin[i]);
		}
	}
	
	free(commands_bin);
	free(cmd_n);
	
	/* Add aliases too */
	if (aliases_n == 0)
		return;
	
	for (i = 0; i < aliases_n; i++) {
		int index = strcntchr(aliases[i], '=');
		if (index != -1) {
			bin_commands[l] = (char *)xnmalloc((size_t)index + 1, sizeof(char));
			strncpy(bin_commands[l++], aliases[i], (size_t)index);
		}
	}

/*	bin_commands[l] = (char *)NULL; */
}

char *
my_rl_quote(char *text, int m_t, char *qp)
/* Performs bash-style filename quoting for readline (put a backslash before
 * any char listed in rl_filename_quote_characters.
 * Modified version of:
 * https://utcc.utoronto.ca/~cks/space/blog/programming/ReadlineQuotingExample*/
{
	/* NOTE: m_t and qp arguments are not used here, but are required by
	 * rl_filename_quoting_function */

	/* 
	 * How it works: p and r are pointers to the same memory location 
	 * initialized (calloced) twice as big as the line that needs to be 
	 * quoted (in case all chars in the line need to be quoted); tp is a 
	 * pointer to text, which contains the string to be quoted. We move 
	 * through tp to find all chars that need to be quoted ("a's" becomes 
	 * "a\'s", for example). At this point we cannot return p, since this 
	 * pointer is at the end of the string, so that we return r instead, 
	 * which is at the beginning of the same string pointed by p. 
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
/* This function simply deescapes whatever escaped chars it founds in text,
 * so that readline can compare it to system filenames when completing paths. 
 * Returns a string containing text without escape sequences */
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
	/* state is zero before completion, and 1 ... n after getting possible
	 * completions. Example: 
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

	/* This block is used only in case of "/path/./" to remove the ending "./"
	 * from dirname and to be able to perform thus the executable check via
	 * access() */
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
					if (entry->d_type != DT_BLK && entry->d_type != DT_CHR)
						match = 1;
				}
				
				else if (exec) {
					if (entry->d_type == DT_REG
					&& access(entry->d_name, X_OK) == 0)
						match = 1;
				}

				else if (exec_path) {
					if (entry->d_type == DT_REG) {
						snprintf(tmp, PATH_MAX, "%s%s", dir_tmp, entry->d_name);
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

	/* readdir() returns NULL on reaching the end of directory stream. So
	 * that if entry is NULL, we have no matches */

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
			if (rl_complete_with_tilde_expansion && *users_dirname == '~') {
				size_t dirlen = strlen(dirname);
				temp = (char *)xcalloc(dirlen + strlen(entry->d_name) + 2, 
									   sizeof(char));
				strcpy(temp, dirname);
				/* Canonicalization cuts off any final slash present.  We 
				 * need to add it back. */
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
		matches = rl_completion_matches(text, &bin_cmd_generator);
	}
	else {
		 
				/* #### ELN EXPANSION ### */
		
		/* Perform this check only if the first char of the string to be 
		 * completed is a number in order to prevent an unnecessary call to 
		 * atoi */
		if (*text >= 0x31 && *text <= 0x39) {
			int num_text = atoi(text);
			if (is_number(text) && num_text > 0 && num_text <= (int)files)
					matches = rl_completion_matches(text, &filenames_generator);
		}
				/* ### BOOKMARKS COMPLETION ### */
		
		else if (*rl_line_buffer == 'b' 
		/* Why to compare the first char of the line buffer? Simply to
		 * prevent unnecessary calls to strncmp(). For instance, if the user
		 * typed "cd ", there is no need to compare the line against "bm"
		 * or "bookmarks" */
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
			profile_names = (char **)xrealloc(profile_names, (pf_n + 1) * 
									 sizeof(char *));
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
	called, and a non-zero positive in later calls. This means that i and len 
	will be necessarilly initialized the first time */

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
			fputs("#Example: [t]test:/path/to/test\n", fp);
			fclose(fp);
		}
	}

	fp = fopen(BM_FILE, "r");
	if (!fp) {
		_err('e', PRINT_PROMPT, "%s: '%s': Error reading the bookmarks file\n", 
			 PROGRAM_NAME, BM_FILE);
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
		/* Full bookmark line: "[sc]name:path". So, let's first get everything
		 * before ':' */
		int ret = strcntchr(line, ':');
		if (ret != -1) {
			line[ret] = 0x00;
			/* Now we have at most "[sc]name" (or just "name" if no shortcut) */
			char *name = (char *)NULL;
			ret = strcntchr(line, ']');
			if (ret != -1)
				/* Now name is everthing after ']', that is, "name" */
				name = line + ret + 1;
			else /* If no shortcut, name is line in its entirety */
				name = line;
			if (!name || *name == 0x00)
				continue;
			bookmark_names = (char **)xrealloc(bookmark_names, (bm_n + 1) 
											   * sizeof(char *));
			bookmark_names[bm_n] = (char *)xcalloc(strlen(name) + 1, 
												   sizeof(char));
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
	/* According to the GNU readline documention: "If it is set to a non-zero 
	 * value, directory names have a slash appended and Readline attempts to 
	 * quote completed filenames if they contain any embedded word break 
	 * characters." To make the quoting part work I had to specify a custom
	 * quoting function (my_rl_quote) */
	if (!state) /* state is zero only the first time readline is executed */
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
/* Store all paths in the PATH environment variable into a globally declared 
 * array (paths) */
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

		/* alt_profile will not be NULL whenever the -P option is used
		 * to run the program under an alternative profile */
		if (alt_profile) {
			CONFIG_DIR = (char *)xcalloc(user_home_len + pnl_len + 10 
									+ strlen(alt_profile) + 1, sizeof(char));
			sprintf(CONFIG_DIR, "%s/.config/%s/%s", user_home, PNL, 
					alt_profile);
		}
		else {
			CONFIG_DIR = (char *)xcalloc(user_home_len + pnl_len + 17 + 1, 
										 sizeof(char));
			sprintf(CONFIG_DIR, "%s/.config/%s/default", user_home, PNL);
		}
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
		CONFIG_FILE = (char *)xcalloc(config_len + pnl_len + 4, sizeof(char));
		sprintf(CONFIG_FILE, "%s/%src", CONFIG_DIR, PNL);
		PROFILE_FILE = (char *)xcalloc(config_len + pnl_len + 10, 
							   sizeof(char));
		sprintf(PROFILE_FILE, "%s/%s_profile", CONFIG_DIR, PNL);
		MSG_LOG_FILE = (char *)xcalloc(config_len + 14, sizeof(char));
		sprintf(MSG_LOG_FILE, "%s/messages.cfm", CONFIG_DIR);
		MIME_FILE = (char *)xcalloc(config_len + 14, sizeof(char));
		sprintf(MIME_FILE, "%s/mimelist.cfm", CONFIG_DIR);

		/* #### CHECK THE TRASH DIR #### */
		/* Create trash dirs, if necessary */
		int ret = -1;
		
		if (stat(TRASH_DIR, &file_attrib) == -1) {
			char *trash_files = (char *)NULL;
			trash_files = (char *)xcalloc(strlen(TRASH_DIR) + 7, sizeof(char));
			sprintf(trash_files, "%s/files", TRASH_DIR);
			char *trash_info = (char *)NULL;
			trash_info = (char *)xcalloc(strlen(TRASH_DIR) + 6, sizeof(char));
			sprintf(trash_info, "%s/info", TRASH_DIR);		
			char *cmd[] = { "mkdir", "-p", trash_files, trash_info, NULL };
			ret = launch_execve(cmd, FOREGROUND);
			free(trash_files);
			free(trash_info);
			if (ret != 0) {
				trash_ok = 0;
				_err('w', PRINT_PROMPT, _("%s: mkdir: '%s': Error creating "
					 "trash directory. Trash function disabled\n"), 
					 PROGRAM_NAME, TRASH_DIR);
			}
		}
		
		/* If it exists, check it is writable */
		else if (access (TRASH_DIR, W_OK) == -1) {
			trash_ok = 0;
			_err('w', PRINT_PROMPT, _("%s: '%s': Directory not writable. Trash "
				 "function disabled\n"), PROGRAM_NAME, TRASH_DIR);
		}

		/* #### CHECK THE CONFIG DIR #### */
		/* If the config directory doesn't exist, create it */
		/* Use the GNU mkdir to let it handle parent directories */
		if (stat(CONFIG_DIR, &file_attrib) == -1) {
			char *tmp_cmd[] = { "mkdir", "-p", CONFIG_DIR, NULL }; 
			ret = launch_execve(tmp_cmd, FOREGROUND);
			if (ret != 0) {
				config_ok = 0;
				_err('e', PRINT_PROMPT, _("%s: mkdir: '%s': Error creating "
					 "configuration directory. Bookmarks, commands logs, and "
					 "command history are disabled. Program messages won't be "
					 "persistent. Using default options\n"), PROGRAM_NAME, 
					 CONFIG_DIR);
			}
		}
		
		/* If it exists, check it is writable */
		else if (access(CONFIG_DIR, W_OK) == -1) {
			config_ok = 0;
			_err('e', PRINT_PROMPT, _("%s: '%s': Directory not writable. "
				 "Bookmarks, commands logs, and commands history are disabled. "
				 "Program messages won't be persistent. Using default "
				 "options\n"), PROGRAM_NAME, CONFIG_DIR);
		}

		/* #### CHECK THE MIME CONFIG FILE #### */
		/* Open the mime file or create it, if it doesn't exist */
		if (config_ok && stat(MIME_FILE, &file_attrib) == -1) {

			_err('n', PRINT_PROMPT, _("%s created a new MIME list file (%s). It "
				 "is recommended to edit this file (typing 'mm edit' or "
				 "however you want) to add the programs you use and remove "
				 "those you don't. This will make the process of opening files "
				 "faster and smoother\n"), PROGRAM_NAME, MIME_FILE);


			/* Try importing MIME associations from the system, and in case 
			 * nothing can be imported, create an empty MIME list file */
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
						fputs("text/plain=gedit;kate;pluma;mousepad;leafpad;"
							  "nano;vim;vi;emacs;ed\n"
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
				_err('e', PRINT_PROMPT, "%s: fopen: '%s': %s\n", PROGRAM_NAME, 
					 PROFILE_FILE, strerror(errno));
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
				_err('e', PRINT_PROMPT, _("%s: fopen: '%s': %s. Using default "
					 "values.\n"), PROGRAM_NAME, CONFIG_FILE, strerror(errno));
			}
			else {
				div_line_char = -1;
				#define MAX_BOOL 6
				/* starting path(14) + PATH_MAX + \n(1)*/
				char line[PATH_MAX + 15];
				while (fgets(line, sizeof(line), config_fp)) {
					if (strncmp(line, "#END OF OPTIONS", 15) == 0)
						break;

					/* Check for the xargs.splash flag. If -1, it was not
					 * set via command line, so that it must be set here */
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

					else if (strncmp(line, "SortList=", 9) == 0) {
						char opt_str[MAX_BOOL] = "";
						ret = sscanf(line, "SortList=%5s\n", opt_str);
						if (ret == -1)
							continue;
						if (strncmp(opt_str, "false", 5) == 0)
							sort = 0;
						else /* True and default */
							sort = 1;
					}
					else if (strncmp(line, "DirCounter=", 11) == 0) {
						char opt_str[MAX_BOOL] = "";
						ret = sscanf(line, "DirCounter=%5s\n", opt_str);
						if (ret == -1)
							continue;
						if (strncmp(opt_str, "false", 5) == 0)
							dir_counter = 0;
						else /* True and default */
							dir_counter = 1;
					}
					else if (strncmp(line, "WelcomeMessage=", 15) == 0) {
						char opt_str[MAX_BOOL] = "";
						ret = sscanf(line, "WelcomeMessage=%5s\n", opt_str);
						if (ret == -1)
							continue;
						if (strncmp(opt_str, "false", 5) == 0)
							welcome_message = 0;
						else /* True and default */
							welcome_message = 1;
					}

					else if (strncmp(line, "ClearScreen=", 12) == 0) {
						char opt_str[MAX_BOOL] = "";
						ret = sscanf(line, "ClearScreen=%5s\n", opt_str);
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
						ret = sscanf(line, "ShowHiddenFiles=%5s\n", opt_str);
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
						ret = sscanf(line, "LongViewMode=%5s\n", opt_str);
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
						ret = sscanf(line, "ExternalCommands=%5s\n", opt_str);
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
							sys_shell = (char *)xnmalloc(strlen(tmp) + 1, 
														 sizeof(char));
							strcpy(sys_shell, tmp);
						}
						
						else {
							char *shell_path = get_cmd_path(tmp);
							if (!shell_path) {
								free(opt_str);
								continue;
							}
							sys_shell = (char *)xnmalloc(strlen(shell_path) + 1, 
														 sizeof(char));
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

						term = (char *)xnmalloc(strlen(tmp) + 1, sizeof(char));
						strcpy(term, tmp);
						free(opt_str);
					}

					else if (xargs.ffirst == -1 
					&& strncmp(line, "ListFoldersFirst=", 17) == 0) {
						char opt_str[MAX_BOOL] = "";
						ret = sscanf(line, "ListFoldersFirst=%5s\n", opt_str);
						if (ret == -1)
							continue;
						if (strncmp(opt_str, "false", 5) == 0)
							list_folders_first = 0;
						else /* True and default */
							list_folders_first = 1;
					}

					else if (xargs.cdauto == -1 
					&& strncmp(line, "CdListsAutomatically=", 21) == 0) {
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
						snprintf(default_color, MAX_COLOR, "\x1b[%sm", tmp);
						free(opt_str);
					}

					else if (strncmp(line, "DirCounterColor=", 16) == 0) {
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
						snprintf(dir_count_color, MAX_COLOR, "\x1b[%sm", tmp);
						free(opt_str);
					}

					else if (strncmp(line, "WelcomeMessageColor=", 20) == 0) {
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
						snprintf(welcome_msg_color, MAX_COLOR, "\x1b[%sm", 
								 tmp);
						free(opt_str);
					}

					else if (strncmp(line, "DividingLineColor=", 18) == 0) {
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
						snprintf(div_line_color, MAX_COLOR, "\x1b[%sm", tmp);
						free(opt_str);
					}
					else if (strncmp(line, "DividingLineChar=", 17) == 0) {
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
						
						/* If starting path is not NULL, and exists, and 
						 * is a directory, and the user has appropriate 
						 * permissions, set path to starting path. If any of 
						 * these conditions is false, path will be set to 
						 * default, that is, CWD */
						if (chdir(tmp) == 0) {
							free(path);
							path = (char *)xnmalloc(strlen(tmp) + 1, 
												    sizeof(char));
							strcpy(path, tmp);
						}
						else {
							_err('w', PRINT_PROMPT, _("%s: '%s': %s. Using "
								 "the current working directory as starting "
								 "path\n"), PROGRAM_NAME, tmp, strerror(errno));
						}
						free(opt_str);
					}
				}

				fclose(config_fp);
			}

			/* If some option was not set, neither via command line nor via 
			 * the config file, or if this latter could not be read for any 
			 * reason, set the defaults */
			if (splash_screen == -1) splash_screen = 0; /* -1 means not set */
			if (welcome_message == -1) welcome_message = 1;
			if (show_hidden == -1) show_hidden = 1;
			if (sort == -1) sort = 1;
			if (dir_counter == -1) dir_counter = 1;
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
					sys_shell = (char *)xcalloc(strlen(FALLBACK_SHELL), 
												sizeof(char));
					strcpy(sys_shell, FALLBACK_SHELL);
				}
			}
			if (!encoded_prompt) {
				encoded_prompt = (char *)xcalloc(strlen(DEFAULT_PROMPT) + 1, 
												 sizeof(char));
				strcpy(encoded_prompt, DEFAULT_PROMPT);
			}
			if (!term) {
				term = (char *)xcalloc(strlen(DEFAULT_TERM_CMD) + 1, 
									   sizeof(char));
				strcpy(term, DEFAULT_TERM_CMD);
			}
		}

		/* If we reset all these values, a) the user will be able to modify 
		 * them while in the program via the edit command. However, this means
		 * that b) any edit, to whichever option, will override the values for 
		 * ALL options, including those set in the command line. Example: if the
		 * user pass the -A option to show hidden files (while it is disabled
		 * in the config file), and she edits the config file to, say, change
		 * the prompt, hidden files will be disabled, which is not what the 
		 * user wanted */

/*		xargs.splash = xargs.hidden = xargs.longview = xargs.ext = -1;
		xargs.ffirst = xargs.sensitive = xargs.unicode = xargs.pager = -1;
		xargs.path = xargs.cdauto = -1; */

		
		/* "XTerm*eightBitInput: false" must be set in HOME/.Xresources to 
		 * make some keybindings like Alt+letter work correctly in xterm-like 
		 * terminal emulators */
		/* However, there is no need to do this if using the linux console, 
		 * since we are not in a graphical environment */
		if (flags & GRAPHICAL) {
			/* Check ~/.Xresources exists and eightBitInput is set to false. 
			 * If not, create the file and set the corresponding value */
			char xresources[PATH_MAX] = "";
			sprintf(xresources, "%s/.Xresources", user_home);
			FILE *xresources_fp = fopen(xresources, "a+");
			if (xresources_fp) {
				/* Since I'm looking for a very specific line, which is a 
				 * fixed line far below MAX_LINE, I don't care to get any 
				 * of the remaining lines truncated */
				#if __FreeBSD__
					fseek(xresources_fp, 0, SEEK_SET);
				#endif
				char line[256] = "";
				int eight_bit_ok = 0;
				while (fgets(line, sizeof(line), xresources_fp))
					if (strncmp(line, "XTerm*eightBitInput: false", 26) == 0)
						eight_bit_ok = 1;
				if (!eight_bit_ok) {
					/* Set the file position indicator at the end of the 
					 * file */
					fseek(xresources_fp, 0L, SEEK_END);
					fputs("\nXTerm*eightBitInput: false\n", xresources_fp);
					char *xrdb_path = get_cmd_path("xrdb");
					if (xrdb_path) {
						char *res_file = (char *)xnmalloc(strlen(user_home) 
														+ 13, sizeof(char));
						sprintf(res_file, "%s/.Xresources", user_home); 
						char *cmd[] = { "xrdb", "merge", res_file, NULL };
						launch_execve(cmd, FOREGROUND);
						free(res_file);
					}

					_err('w', PRINT_PROMPT, _("%s: Restart your %s for "
						 "changes to ~/.Xresources to take effect. "
						 "Otherwise, %s keybindings might not work as "
						 "expected.\n"), PROGRAM_NAME, (xrdb_path) 
						 ? _("terminal") : _("X session"), PROGRAM_NAME);
						
					if (xrdb_path)
						free(xrdb_path);
				}

				fclose(xresources_fp);
			}
			else {
				_err('e', PRINT_PROMPT, "%s: fopen: '%s': %s\n", PROGRAM_NAME, 
					 xresources, strerror(errno));
			}
		}
	}
	
	/* ################## END OF IF HOME ########################### */

	/* #### CHECK THE TMP DIR #### */
	/* If the temporary directory doesn't exist, create it. I Use 1777 
	 * permissions, that is, world writable, but with sticky bit set:
	 * everyone can create files in here, but only the file's owner can remove
	 * or modify the file. I store here the list of selected files per user
	 * (TMP_DIR/sel_file_username), so that all users need to be able to
	 * create files here, but only the user who created it is able
	 * to delete it or modify it */

	TMP_DIR = (char *)xnmalloc(strlen(user) + pnl_len + 7, sizeof(char));
	sprintf(TMP_DIR, "/tmp/%s/%s", PNL, user);

	if (stat(TMP_DIR, &file_attrib) == -1) {
/*		if (mkdir(TMP_DIR, 1777) == -1) { */
		char *tmp_cmd2[] = { "mkdir", "-pm1777", TMP_DIR, NULL };
		int ret = launch_execve(tmp_cmd2, FOREGROUND);
		if (ret != 0) {
			selfile_ok = 0;
			_err('e', PRINT_PROMPT, "%s: mkdir: '%s': %s\n", PROGRAM_NAME, 
				 TMP_DIR, strerror(errno));
		}
	}
	
	/* If the directory exists, check it is writable */
	else if (access(TMP_DIR, W_OK) == -1) {
		selfile_ok = 0;
		_err('e', PRINT_PROMPT, "%s: '%s': Directory not writable. Selected "
			 "files won't be persistent\n", PROGRAM_NAME, TMP_DIR);
	}

	if (selfile_ok) {
		/* Define the user's sel file. There will be one per user 
		 * (/tmp/clifm/sel_file_username) */
		size_t user_len = strlen(user);
		size_t tmp_dir_len = strlen(TMP_DIR);
		sel_file_user = (char *)xnmalloc(tmp_dir_len + pnl_len + user_len + 8, 
										sizeof(char));
		sprintf(sel_file_user, "%s/.%s_sel_%s", TMP_DIR, PNL, user);
	}
}

void
exec_profile(void)
{
	if (!config_ok)
		return;

	struct stat file_attrib;
	if (stat(PROFILE_FILE, &file_attrib) == 0) {
		FILE *fp = fopen(PROFILE_FILE, "r");
		if (fp) {

			size_t line_size = 0;
			char *line = (char *)NULL;
			ssize_t line_len = 0;
			while ((line_len = getline(&line, &line_size, fp)) > 0) {
				if (strcntchr(line, '=') != -1 && !isdigit (line[0])) {
					create_usr_var(line);
				}
				if (strlen(line) != 0 && line[0] != '#') {
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
	}
}

char *
get_cmd_path(const char *cmd)
/* Get the path of a given command from the PATH environment variable. It 
 * basically does the same as the 'which' Unix command */
{
	char *cmd_path = (char *)NULL;
	size_t i;
	
	cmd_path = (char *)xnmalloc(PATH_MAX + 1, sizeof(char));
	
	for (i = 0; i < path_n; i++) { /* Get the path from PATH env variable */
		snprintf(cmd_path, PATH_MAX, "%s/%s", paths[i], cmd);
		if (access(cmd_path, X_OK) == 0)
			return cmd_path;
	}
	
	/* If path was not found */
	free(cmd_path);
	
	return (char *)NULL;
}

void
external_arguments(int argc, char **argv)
/* Evaluate external arguments, if any, and change initial variables to its 
* corresponding value */
{
/* Link long (--option) and short options (-o) for the getopt_long function */
	/* Disable automatic error messages to be able to handle them myself via 
	* the '?' case in the switch */
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
		{0, 0, 0, 0}
	};

	/* Set all external arguments flags to uninitialized state */
	xargs.splash = xargs.hidden = xargs.longview = xargs.ext = -1;
	xargs.ffirst = xargs.sensitive = xargs.unicode = xargs.pager = -1;
	xargs.path = xargs.cdauto = -1;

	int optc;
	/* Variables to store arguments to options (-p and -P) */
	char *path_value = (char *)NULL, *alt_profile_value = (char *)NULL;

	while ((optc = getopt_long(argc, argv, "+aAfFgGhiIlLoOp:P:sUuvx", longopts, 
		(int *)0)) != EOF) {
		/* ':' and '::' in the short options string means 'required' and 
		 * 'optional argument' respectivelly. Thus, 'p' and 'P' require an 
		 * argument here. The plus char (+) tells getopt to stop processing 
		 * at the first non-option (and non-argument) */
		switch (optc) {

		case 'a':
			flags &= ~HIDDEN; /* Remove HIDDEN from 'flags' */
			show_hidden = 0;
			xargs.hidden = 0;
			break;

		case 'A':
			flags |= HIDDEN; /* Add flag HIDDEN to 'flags' */
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
			print_license();
			exit(EXIT_SUCCESS);

		case 'x':
			ext_cmd_ok = 1;
			xargs.ext = 1;
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
						"[-aAfFgGhiIlLoOsuUvx] [-p path]\nTry '%s --help' for "
						"more information.\n"), PROGRAM_NAME, optopt, PNL, PNL);
			else fprintf(stderr, _("%s: unknown option character '\\%x'\n"), 
						 PROGRAM_NAME, (unsigned int)optopt);
			exit(EXIT_FAILURE);
		
		default: break;
		}
	}

	if ((flags & START_PATH) && path_value) {
		if (chdir(path_value) == 0) {
			if (path)
				free(path);
			path = (char *)xcalloc(strlen(path_value) + 1, sizeof(char));
			strcpy(path, path_value);
		}
		else { /* Error changing directory */
			_err('w', PRINT_PROMPT, "%s: '%s': %s\n", PROGRAM_NAME, path_value, 
				 strerror(errno));
		}
	}
	
	if ((flags & ALT_PROFILE) && alt_profile_value) {
		alt_profile = (char *)xcalloc(strlen(alt_profile_value) + 1, 
									  sizeof(char));
		strcpy(alt_profile, alt_profile_value);
	}
}

char *
home_tilde (const char *new_path)
/* Reduce "HOME" to tilde ("~"). The new_path variable is always either 
 * "HOME" or "HOME/file", that's why there's no need to check for "/file" */
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
		path_tilde = (char *)xcalloc(strlen(new_path + user_home_len + 1) + 3, 
						     sizeof(char));
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

			/* If any, store the string before the beginning of the braces */
			for (j = 0; j < i; j++)
				braces_root[j] = str[j];
		}

		/* If any, store the string after the closing brace */
		if (initial_brace && str[i] == '}') {
			closing_brace = (int)i;
			if ((str_len - 1) - (size_t)closing_brace > 0) {
				braces_end = (char *)xcalloc(str_len - (size_t)closing_brace, 
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
										   (closing_brace - initial_brace),
										   sizeof(char));
		
		while (str[i] != '}' && str[i] != ',' && str[i] != 0x00)
			brace_tmp[k++] = str[i++];
		
		brace_tmp[k] = 0x00;
		
		if (braces_end) {
			braces[braces_n] = (char *)xcalloc(strlen(braces_root) 
									+ strlen(brace_tmp)
									+ strlen(braces_end) + 1, sizeof(char));
			sprintf(braces[braces_n], "%s%s%s", braces_root, brace_tmp, 
					 braces_end);
		}
		else {
			braces[braces_n] = (char *)xcalloc(strlen(braces_root)
										+ strlen(brace_tmp) + 1, sizeof(char));
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
parse_input_str (char *str)
/* 
 * This function is one of the keys of CliFM. It will perform a series of 
 * actions:
 * 1) Take the string stored by readline and get its substrings without spaces. 
 * 2) In case of user defined variable (var=value), it will pass the whole 
 * string to exec_cmd(), which will take care of storing the variable;
 * 3) If the input string begins with ';' or ':' the whole string is send to 
 * exec_cmd(), where it will be directly executed by the system shell 
 * (via launch_execle()) to prevent all of the expansions made here.
 * 4) The following expansions (especific to CLiFM) are performed here: ELN's, 
 * "sel" keyword, and ranges of numbers (ELN's). For CliFM internal commands, 
 * braces, tilde, and wildarcards expansions are performed here as well.
 * These expansions are the most import part of this function.
 */

/* NOTE: Though filenames could consist of everything except of slash and null 
 * characters, POSIX.1 recommends restricting filenames to consist of the 
 * following characters: letters (a-z, A-Z), numbers (0-9), period (.), 
 * dash (-), and underscore ( _ ).

 * NOTE 2: There is no any need to pass anything to this function, since the
 * input string I need here is already in the readline buffer. So, instead of
 * taking the buffer from a function parameter (str) I could simply use 
 * rl_line_buffer. However, since I use this function to parse other strings, 
 * like history lines, I need to keep the str argument */

{
	register size_t i = 0;

		   /* ########################################
		    * #    0) CHECK FOR SPECIAL FUNCTIONS    # 
		    * ########################################*/

	int chaining = 0, cond_cmd = 0, send_shell = 0;

					/* ###########################
					 * #  0.a) RUN AS EXTERNAL   # 
					 * ###########################*/

	/* If invoking a command via ';' or ':' set the send_shell flag to tru and 
	 * send the whole string to exec_cmd(), in which case no expansion is made: 
	 * the command is literally send to the system shell. */
	if (*str == ';' || *str == ':')
		send_shell = 1;

	if (!send_shell) {
		for (i = 0; str[i]; i++) {
				
				/* ##################################
				 * #   0.b) CONDITIONAL EXECUTION   # 
				 * ##################################*/

			/* Check for chained commands (cmd1;cmd2) */
			if (!chaining && str[i] == ';' && i > 0 && str[i - 1] != '\\')
				chaining = 1;
				
			/* Check for conditional execution (cmd1 && cmd 2)*/
			if (!cond_cmd && str[i] == '&' && i > 0 && str[i - 1] != '\\'
			&& str[i + 1] && str[i + 1] == '&')
				cond_cmd = 1;

				/* ##################################
				 * #   0.c) USER DEFINED VARIABLE   # 
				 * ##################################*/
				 			
			/* If user defined variable send the whole string to exec_cmd(), 
			 * which will take care of storing the variable. */
			if (!(flags & IS_USRVAR_DEF) && str[i] == '=' && i > 0 
			&& str[i - 1] != '\\' && str[0] != '=') {
				/* Remove leading spaces. This: '   a="test"' should be taken
				 * as a valid variable declaration */
				char *p = str;
				while (*p == 0x20 || *p == '\t')
					p++;

				/* If first non-space is a number, it's not a variable name */
				if (!isdigit(*p)) {
					int space_found = 0;
					/* If there are no spaces before '=', take it as a variable. 
					* This check is done in order to avoid taking as a variable 
					* things like: 'ls -color=auto'*/
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

	/* If chained commands, check each of them. If at least one of them is
	 * internal, take care of the job (the system shell does not know our 
	 * internal commands and therefore cannot execute them); else, if no 
	 * internal command is found, let it to the system shell */
	if (chaining || cond_cmd) {
		
		/* User defined variables are always internal, so that there is no
		 * need to check whatever else is in the command string */
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
		/* Remove leading sapces, again */
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
		/* If ";cmd" or ":cmd" the whole input line will be send to exec_cmd()
		* and will be executed by the system shell via execle(). Since we 
		* don't run split_str() here, dequoting and deescaping is performed 
		* directly by the system shell */
	}

		/* ################################################
		 * #     1) SPLIT INPUT STRING INTO SUBSTRINGS    # 
		 * ################################################*/

	/* split_str() returns an array of strings without leading, terminating
	 * and double spaces. */
	char **substr = split_str(str);

	/* NOTE: isspace() not only checks for space, but also for new line, 
	 * carriage return, vertical and horizontal TAB. Be careful when replacing 
	 * this function. */

	if (!substr)
		return (char **)NULL;

				/* ##############################
				 * #   2) BUILTIN EXPANSIONS    #
				 * ############################## 

	 * Ranges, sel, ELN, and internal variables. 
	 * These expansions are specific to CliFM. To be able to use them even
	 * with external commands, they must be expanded here, before sending
	 * the input string, in case the command is external, to the system
	 * shell */

	is_sel = 0, sel_is_last = 0;

	size_t int_array_max = 10, ranges_ok = 0;
	int *range_array = (int *)xnmalloc(int_array_max, sizeof(int));

	for (i = 0; i <= args_n; i++) {
		
		size_t substr_len = strlen(substr[i]);
		
		/* Check for ranges */
		register size_t j = 0;
		for (j = 0; substr[i][j]; j++) {
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

	 /* Expand expressions like "1-3" to "1 2 3" if all the numbers in the
	 * range correspond to an ELN */

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
				
				substr = (char **)xrealloc(substr, (args_n + ranges_n + 2) * 
										   sizeof(char *));
				
				for (i = 0; i < j; i++) {
					substr[i] = (char *)xcalloc(strlen(ranges_cmd[i]) + 1, 
												sizeof(char));
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
			sel_array = (char **)xcalloc(args_n + sel_n + 2, sizeof(char *));
			
			for (i = 0; i < (size_t)is_sel; i++) {
				sel_array[j] = (char *)xcalloc(strlen(substr[i]) + 1, 
											   sizeof(char));
				strcpy(sel_array[j++], substr[i]);
			}
			
			for (i = 0; i < sel_n; i++) {
				/* Escape selected filenames and copy them into tmp array */
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
					/* Free elements selected thus far and and all the input
					 * substrings */
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

		/* i must be bigger than zero because the first string in comm_array, 
		 * the command name, should NOT be expanded, but only arguments. 
		 * Otherwise, if the expanded ELN happens to be a program name as 
		 * well, this program will be executed, and this, for sure, is to be 
		 * avoided */

		if (i > 0 && is_number(substr[i])) {
			int num = atoi(substr[i]);
			/* Expand numbers only if there is a corresponding ELN */

			if (num > 0 && num <= (int)files) {
				/* Replace the ELN by the corresponding escaped filename */
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
					fprintf(stderr, _("%s: %s: Error quoting filename\n"), 
							PROGRAM_NAME, dirlist[num-1]);
					/* Free whatever was allocated thus far */
					size_t j;
					for (j = 0; j <= args_n; j++)
						free(substr[j]);
					free(substr);
					return (char **)NULL;
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
							    (strlen(usr_var[j].value) + 1) * sizeof(char));
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

		/* Do not perform any of the expansions below for selected elements: 
		 * they are full path filenames that, as such, do not need any 
		 * expansion */
		if (is_sel) { /* is_sel is true only for the current input and if
			there was some "sel" keyword in it */
			/* Strings between is_sel and sel_n are selected filenames */
			if (i >= (size_t)is_sel && i <= sel_n)
				continue;
		}
		
		/* The search function admits a path as second argument. So, if the
		 * command is search, perform the expansions only for the first
		 * parameter, if any.
		 *  */
		if (substr[0][0] == '/' && i != 1)
			continue;

				/* ############################
				 * #    4) TILDE EXPANSION    # 
				 * ###########################*/

		 /* (replace "~/" by "/home/user") */
		if (strncmp (substr[i], "~/", 2) == 0) {
			/* tilde_expansion() is provided by the readline lib */
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
		2) Create a new array, say comm_array_glob, large enough to store the 
		   expanded glob and the remaining (non-glob) arguments 
		   (args_n+gl_pathc)
		3) Copy into this array everything before the glob 
		   (i=0;i<glob_char;i++)
		4) Copy the expanded elements (if none, copy the original element, 
		   comm_array[glob_char])
		5) Copy the remaining elements (i=glob_char+1;i<=args_n;i++)
		6) Free the old comm_array and fill it with comm_array_glob 
	  */
		size_t old_pathc = 0;
		/* glob_array stores the index of the globbed strings. However, once 
		 * the first expansion is done, the index of the next globbed string 
		 * has changed. To recover the next globbed string, and more precisely, 
		 * its index, we only need to add the amount of files matched by the 
		 * previous instances of glob(). Example: if original indexes were 2 
		 * and 4, once 2 is expanded 4 stores now some of the files expanded 
		 * in 2. But if we add to 4 the amount of files expanded in 2 
		 * (gl_pathc), we get now the original globbed string pointed by 4.
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
						glob_cmd[j] = (char *)xcalloc(strlen(esc_str) + 1,
													  sizeof(char));
						strcpy(glob_cmd[j++], esc_str);
						free(esc_str);
					}
					else {
						fprintf(stderr, _("%s: %s: Error quoting filename\n"), 
								PROGRAM_NAME, globbuf.gl_pathv[i]);
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
				
				for (i = (size_t)glob_array[g] + old_pathc + 1; i <= args_n; 
				i++) {
					glob_cmd[j] = (char *)xcalloc(strlen(substr[i]) + 1, 
												  sizeof(char));
					strcpy(glob_cmd[j++], substr[i]);
				}
				glob_cmd[j] = (char *)NULL;

				for (i = 0; i <= args_n; i++) 
					free(substr[i]);
				substr = (char **)xrealloc(substr, 
										(args_n+globbuf.gl_pathc + 1)
										* sizeof(char *));
				
				for (i = 0;i < j; i++) {
					substr[i] = (char *)xcalloc(strlen(glob_cmd[i]) + 1, 
												sizeof(char));
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
		/* We already know the indexes of braced strings (braces_array[]) */
		int old_braces_arg = 0;
		register size_t b = 0;
		for (b = 0; b < braces_n; b++) {
			/* Expand the braced parameter and store it into a new array */
			int braced_args = brace_expansion(substr[braces_array[b]
											  + old_braces_arg]);
			/* Now we also know how many elements the expanded braced 
			 * parameter has */
			if (braced_args) {
				/* Create an array large enough to store parameters plus 
				 * expanded braces */
				char **comm_array_braces = (char **)NULL;
				comm_array_braces = (char **)xcalloc(args_n + 
													 (size_t)braced_args, 
													 sizeof(char *));
				/* First, add to the new array the paramenters coming before 
				 * braces */
				
				for (i = 0; i < (size_t)(braces_array[b] + old_braces_arg); 
				i++) {
					comm_array_braces[i] = (char *)xcalloc(strlen(substr[i])
														+ 1, sizeof(char));
					strcpy(comm_array_braces[i], substr[i]);
				}

				/* Now, add the expanded braces to the same array */
				register size_t j = 0;
				
				for (j = 0; j < (size_t)braced_args; j++) {
					/* Escape each filename and copy it */
					char *esc_str = escape_str(braces[j]);
					if (esc_str) {
						comm_array_braces[i] = (char *)xcalloc(strlen(esc_str)
														+ 1, sizeof(char));
						strcpy(comm_array_braces[i++], esc_str);
						free(esc_str);
					}
					else {
						fprintf(stderr, _("%s: %s: Error quoting filename\n"), 
								PROGRAM_NAME, braces[j]);
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
				/* Finally, add, if any, those parameters coming after the 
				 * braces */
				
				for (j = (size_t)(braces_array[b] + old_braces_arg) + 1;
				j <= args_n; j++) {
					comm_array_braces[i] = (char *)xcalloc(strlen(substr[j])
													+ 1, sizeof(char));
					strcpy(comm_array_braces[i++], substr[j]);				
				}
				/* Now, free the old comm_array and copy to it our new array 
				 * containing all the parameters, including the expanded 
				 * braces */
				for (j = 0; j <= args_n; j++)
					free(substr[j]);
				substr = (char **)xrealloc(substr, (args_n + 
										   (size_t)braced_args + 1) * 
										   sizeof(char *));			
				
				args_n = i - 1;
				
				for (j = 0; j < i; j++) {
					substr[j] = (char *)xcalloc(strlen(comm_array_braces[j])+1, 
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
	stifle_history(0); /* Prevent readline from using the history setting */
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
		/* Since this file contains only paths, I can be sure no line length 
		 * will larger than PATH_MAX*/
		char sel_line[PATH_MAX] = "";
		while (fgets(sel_line, sizeof(sel_line), sel_fp)) {
			size_t line_len = strlen(sel_line);
			sel_line[line_len - 1] = 0x00;
			sel_elements = (char **)xrealloc(sel_elements, (sel_n + 1)
											 * sizeof(char *));
			sel_elements[sel_n] = (char *)xcalloc(line_len+1, sizeof(char));
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
/* Print the prompt and return the string entered by the user (to be parsed 
 * later by parse_input_str()) */
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
/*	static int first_run = 1;
	if (first_run) {
//		srand(time(NULL));
//		printf("TIP: %s\n", tips[rand() % tipsn]);
		puts(_("TIP: Use a short alias to access remote file systems. "
				"Example: alias ssh_work='some long command'"));	
		first_run = 0;
	} */

	/* Execute prompt commands, if any, and only if external commands are 
	 * allowed */
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

	/* Messages are categorized in three groups: errors, warnings, and notices.
	 * The kind of message should be specified by the function printing
	 * the message itself via a global enum: pmsg, with the following values:
	 * nomsg, error, warning, and notice. */
	char msg_str[17] = ""; /* 11 == length of color_b + letter + NC_b + null */
	if (msgs_n) {
		/* Errors take precedence over warnings, and warnings over notices.
		 * That is to say, if there is an error message AND a warning message,
		 * the prompt will always display the error message sign: a red 'E'. */
		switch (pmsg) {
		case nomsg: break;
		case error: sprintf(msg_str, "%sE%s", red_b, NC_b); break;
		case warning: sprintf(msg_str, "%sW%s", yellow_b, NC_b); break;
		case notice: sprintf(msg_str, "%sN%s", green_b, NC_b); break;
		default: break;
		}
	}

	/* Generate the prompt string */

	/* First, grab and decode the prompt line of the config file (stored in
	 * encoded_prompt at startup) */
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
		+ (sel_n ? 16 : 0) + (trash_n ? 16 : 0) + ((msgs_n && pmsg) ? 16: 0) 
		+ 6 + sizeof(text_color) + 1);

	/* 16 = color_b ({red,green,yellow}_b) + letter (sel, trash, msg) + NC_b; 
	 * 6 = NC_b
	 * 1 = null terminating char */

	char *the_prompt = (char *)xnmalloc(prompt_length, sizeof(char));
	
	snprintf(the_prompt, prompt_length, "%s%s%s%s%s%s%s%s", 
		(msgs_n && pmsg) ? msg_str : "", (trash_n) ? yellow_b : "", 
		(trash_n) ? "T\001\x1b[0m\002" : "", (sel_n) ? green_b : "", 
		(sel_n) ? "*\001\x1b[0m\002" : "", decoded_prompt, NC_b, text_color);

	free(decoded_prompt);

	/* Print error messages, if any. 'print_errors' is set to true by 
	 * log_msg() with the PRINT_PROMPT flag. If NOPRINT_PROMPT is passed
	 * instead, 'print_msg' will be false and the message will be
	 * printed in place by log_msg() itself, without waiting for the next 
	 * prompt */
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
	
	/* Enable commands history only if the input line is not void */
	if (input) {

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
	
	return (char *)NULL;
}

/*
 * int count_dir(const char *dir_path) //Scandir version
// Count the amount of elements contained in a directory. Receives the path to 
// the directory as argument.
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
		_err('e', PRINT_PROMPT, "%s: opendir: '%s': %s\n", PROGRAM_NAME, 
			 dir_path, strerror(errno));

		if (errno == ENOMEM)
			exit(EXIT_FAILURE);
		else
			return 0;
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
skip_implied_dot(const struct dirent *entry)
{
	/* In case a directory isn't reacheable, like a failed mountpoint... */
	struct stat file_attrib;
	if (lstat(entry->d_name, &file_attrib) == -1) {
		fprintf(stderr, _("stat: cannot access '%s': %s\n"), entry->d_name, 
				 strerror(errno));
		return 0;
	}
	if (strcmp(entry->d_name, ".") !=0 && strcmp(entry->d_name, "..") !=0) {
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
/* Print ENTRY using color codes and I as ELN, right padding ENTRY PAD chars
 * and terminating ENTRY with or without a new line char (NEW_LINE 1 or 0
 * respectivelly) */
{
	size_t i_digits = digits_in_num(i);
							/* Num (i) + space + null byte */
	char *index = (char *)xnmalloc(i_digits + 2, sizeof(char));
	if (i > 0) /* When listing files in CWD */
		sprintf(index, "%d ", i);
	else if (i == -1) /* ELN for entry could not be found */
		sprintf(index, "? ");
	/* When listing files NOT in CWD (called from search_function() and first
	 * argument is a path: "/search_str /path") 'i' is zero. In this case, no 
	 * index should be shown at all */
	else
		index[0] = 0x00;

	struct stat file_attrib;
	int ret = 0;
	ret = lstat(entry, &file_attrib);

	if (ret == -1) {
		fprintf(stderr, "%s%-*s: %s%s", index, pad, entry, strerror(errno), 
			    new_line ? "\n" : "");
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
					printf("%s%s%s%s%s%s%s%-*s", eln_color, index, NC, ee_c, 
						   entry, NC, new_line ? "\n" : "", pad, "");
				else 
					printf("%s%s%s%s%s%s%s%-*s", eln_color, index, NC, ex_c, 
						   entry, NC, new_line ? "\n" : "", pad, "");
			}
			else if (file_attrib.st_size == 0)
				printf("%s%s%s%s%s%s%s%-*s", eln_color, index, NC, ef_c, 
					   entry, NC, new_line ? "\n" : "", pad, "");
			else
				printf("%s%s%s%s%s%s%s%-*s", eln_color, index, NC, fi_c, 
					   entry, NC, new_line ? "\n" : "", pad, "");
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
			if (files_dir == 2 || files_dir == 0) { /* If folder is empty, it 
				contains only "." and ".." (2 elements). If not mounted 
				(ex: /media/usb) the result will be zero.*/
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

	case S_IFIFO: printf("%s%s%s%s%s%s%s%-*s", eln_color, index, NC, pi_c, 
						 entry, NC, new_line ? "\n" : "", pad, ""); break;

	case S_IFBLK: printf("%s%s%s%s%s%s%s%-*s", eln_color, index, NC, bd_c, 
						 entry, NC, new_line ? "\n" : "", pad, ""); break;

	case S_IFCHR: printf("%s%s%s%s%s%s%s%-*s", eln_color, index, NC, cd_c, 
						 entry, NC, new_line ? "\n" : "", pad, ""); break;

	case S_IFSOCK: printf("%s%s%s%s%s%s%s%-*s", eln_color, index, NC, so_c, 
						  entry, NC, new_line ? "\n" : "", pad, ""); break;

	/* In case all of the above conditions are false... */
	default: printf("%s%s%s%s%s%s%s%-*s", eln_color, index, NC, no_c, 
				    entry, NC, new_line ? "\n" : "", pad, "");
	}
	
	free(index);
}

int
list_dir(void)
/* List files in the current working directory (global variable 'path'). 
 * Uses filetype colors and columns. Return zero if success or one on error */
{
/*	clock_t start = clock(); */

	/* The global variable 'path' SHOULD be set before calling this
	 * function */

	/* "!path" means that the pointer 'path' points to no memory address 
	 * (NULL), while "*path == 0x00 means" means that the first byte of 
	 * the memory block pointed to by the pointer 'path' is a null char */
	if (!path || *path == 0x00) {
		fprintf(stderr, _("%s: Path is NULL or empty\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}
	
	files = 0; /* Reset the files counter */

	/* CPU Registers are faster than memory to access, so the variables 
	 * which are most frequently used in a C program can be put in registers 
	 * using 'register' keyword. For this reason, the unary operator "&",
	 * cannot be used with registers: they are not memory addresses. Only for 
	 * local variables. The most common use for registers are counters. 
	 * However the use fo the 'register' keyword is just a suggestion made
	 * to the compiler, which can perfectly reject it. Most modern compilers
	 * decide themselves what should be put into registers and what not */
	register int i = 0;

	/* Get the list of files in CWD */
	struct dirent **list = (struct dirent **)NULL;
	int total = scandir(path, &list, skip_implied_dot, (sort) ? ((unicode) 
						? alphasort : (case_sensitive) ? xalphasort : 
						alphasort_insensitive) : NULL);

	if (total == -1) {
		_err('e', PRINT_PROMPT, "%s: scandir: '%s': %s\n", PROGRAM_NAME, 
			 path, strerror(errno));
		if (errno == ENOMEM)
			exit(EXIT_FAILURE);
		else
			return EXIT_FAILURE;
	}

	/* Struct to store information about each file, so that we don't need
	 * to run stat() and strlen() again later, perhaps hundreds of times */
	struct fileinfo *file_info = (struct fileinfo *)xnmalloc(
									(size_t)total + 1, sizeof(struct fileinfo));

	if (list_folders_first) {

		/* Store indices of dirs and files into different int arrays, counting
		 * the number of elements for each array too */
		int *tmp_files = (int *)xnmalloc((size_t)total + 1, sizeof(int)); 
		int *tmp_dirs = (int *)xnmalloc((size_t)total + 1, sizeof(int));
		size_t filesn = 0, dirsn = 0;
		
		for (i = 0; i < total; i++) {
			if (list[i]->d_type == DT_DIR)
				tmp_dirs[dirsn++] = i;
			else
				tmp_files[filesn++] = i;
		}
		
		/* Allocate enough space to store all dirs and file names in 
		 * the dirlist array */
		if (!filesn && !dirsn)
			dirlist = (char **)xrealloc(dirlist, sizeof(char *));
		else
			dirlist = (char **)xrealloc(dirlist, (filesn + dirsn + 1) 
										* sizeof(char *));

		/* First copy dir names into the dirlist array */
		size_t len;
		for (i = 0; i < (int)dirsn; i++) {
			len = (unicode) ? u8_xstrlen(list[tmp_dirs[i]]->d_name)
				  : strlen(list[tmp_dirs[i]]->d_name);
			/* Store the filename length here, so that we don't need to run
			 * strlen() again later on the same file */
			file_info[i].len = len;
			dirlist[i] = (char *)xnmalloc(len + 1, (cont_bt) ? sizeof(char32_t) 
										  : sizeof(char));
			strcpy(dirlist[i], list[tmp_dirs[i]]->d_name);
		}
		
		/* Now copy file names */
		register int j;
		for (j = 0; j < (int)filesn; j++) {
			len = (unicode) ? u8_xstrlen(list[tmp_files[j]]->d_name)
				  : strlen(list[tmp_files[j]]->d_name);
			file_info[i].len = len;
			dirlist[i] = (char *)xnmalloc(len + 1, (cont_bt) ? sizeof(char32_t)
										  : sizeof(char));
			strcpy(dirlist[i++], list[tmp_files[j]]->d_name);
		}
		
		free(tmp_files);
		free(tmp_dirs);
		dirlist[i] = (char *)NULL;

		/* This global variable keeps a record of the amounf of files in the
		 * CWD */
		files = (size_t)i;
	}
	
	/* If no list_folders_first */
	else {

		files = (size_t)total;

		dirlist = (char **)xrealloc(dirlist, (size_t)(files + 1)
									* sizeof(char *));

		size_t len;
		for (i = 0; i < (int)files; i++) {
			len = strlen(list[i]->d_name);
			file_info[i].len = len;
			dirlist[i] = xnmalloc(len + 1, sizeof(char));
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

	/* Get the longest element */
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

		/* file_name_width contains: ELN's amount of digits + one space 
		 * between ELN and filename + filename length. Ex: '12 name' contains 
		 * 7 chars */
		size_t file_name_width = digits_in_num(i + 1) + 1 + file_info[i].len;

		/* If the file is a non-empty directory and the user has access 
		 * permision to it, add to file_name_width the number of digits of the 
		 * amount of files this directory contains (ex: 123 (files) contains 3 
		 * digits) + 2 for space and slash between the directory name and the 
		 * amount of files it contains. Ex: '12 name /45' contains 11 chars */
		if ((file_info[i].type & S_IFMT) == S_IFDIR) {
			if (!dir_counter) {
				/* All dirs will be printed as having read access and being 
				 * not empty (bold blue by default), no matter if they're empty 
				 * or not or if the user has read access or not. Otherwise, the
				 * listing process could be really slow, for example, when 
				 * listing files on a remote server */
				file_info[i].ruser = 1;
				file_info[i].filesn = 3;
			}
			else {
				if (access(dirlist[i], R_OK) == 0) {
					file_info[i].filesn = count_dir(dirlist[i]);
					file_info[i].ruser = 1;
					if (file_info[i].filesn > 2)
						file_name_width += 
							digits_in_num((int)file_info[i].filesn) + 2;
				}
				else
					file_info[i].ruser = 0;
			}
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

		int max = get_max_long_view();
		
		eln_len = digits_in_num((int)files);

		for (i = 0; i < (int)files; i++) {

			if (pager) {
				/*	Check terminal amount of rows: if as many filenames as 
				 * the amount of available terminal rows has been printed, 
				 * stop */
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

			printf("%s%d%s ", eln_color, i + 1, NC);

			get_properties(dirlist[i], (int)long_view, max, file_info[i].len);
		}

		if (reset_pager)
			pager = 1;

		free(file_info);
		
		while (total--)
			free(list[total]);
		free(list);
		
		return EXIT_SUCCESS;
	}
	
	/* Normal view mode */
	
	/* Get possible amount of columns for the dirlist screen */
	size_t columns_n = (size_t)term_cols / (longest + 1); /* +1 for the space 
	between file names */
	
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

		/* A basic pager for directories containing large amount of files
		* What's missing? It only goes downwards. To go backwards, use the 
		* terminal scrollback function */
		if (pager) {
			/* Run the pager only once all columns and rows fitting in the 
			 * screen are filled with the corresponding filenames */
			/*			Check rows					Check columns */
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
					if (dir_counter) {
						printf("%s%d%s %s%s%s%s /%zu%s%s", eln_color, i + 1, 
							 NC, (file_info[i].type & S_ISVTX) ? ((is_oth_w) ? 
							 tw_c : st_c) : ((is_oth_w) 
							 ? ow_c : di_c), dirlist[i], 
							 NC, dir_count_color, file_info[i].filesn - 2, 
							 NC, (last_column) ? "\n" : "");
						is_dir = 1;
					}
					else {
						printf("%s%d%s %s%s%s%s", eln_color, i + 1, 
							 NC, (file_info[i].type & S_ISVTX) ? ((is_oth_w) ? 
							 tw_c : st_c) : ((is_oth_w) 
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
				char *linkname = realpath(dirlist[i], (char *)NULL);
				if (linkname) {
					printf("%s%d%s %s%s%s%s", eln_color, i + 1, NC, ln_c, 
							dirlist[i], NC, (last_column) ? "\n" : "");
					free(linkname);
				}
				else
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
				/* Get the amount of digits in the number of files contained
				 * by the listed directory */
				size_t dig_num = digits_in_num((int)file_info[i].filesn - 2);
				/* The amount of digits plus 2 chars for " /" */
				diff -= (dig_num + 2);
			}

			/* Print the spaces needed to equate the length of the lines */
			/* +1 is for the space between filenames */
			register int j;
			for (j = (int)diff + 1; j--;)
				putchar(' ');
		}
	}
	
	free(file_info);
	
	/* If the pager was disabled during listing (by pressing 'c', 'p' or 'q'),
	reenable it */
	if (reset_pager)
		pager = 1;

	/* If the last listed element was modulo (in the last column), don't print 
	 * anything, since it already has a new line char at the end. Otherwise, 
	 * if not modulo (not in the last column), print a new line, for it has 
	 * none */
	 if (!last_column)
		putchar('\n');

	/* Print a dividing line between the files list and the prompt */
	fputs(div_line_color, stdout);
	for (i = term_cols; i--; )
		putchar(div_line_char);
	printf("%s%s", NC, default_color);

	fflush(stdout);

	/* Free the scandir array */
	/* Whatever time it takes to free this array, it will be faster to do it 
	 * after listing files than before (at least theoretically) */
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
		_err('e', PRINT_PROMPT, "%s: prompt: '%s': %s\n", PROGRAM_NAME, 
						CONFIG_FILE, strerror(errno));
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

	while ((line_len = getline(&line, &line_size, config_file_fp)) > 0) {
		
		if (prompt_line_found) {
			if (strncmp(line, "#END OF PROMPT", 14) == 0)
				break;
			if (*line != '#') {
				prompt_cmds = (char **)xrealloc(prompt_cmds, (prompt_cmds_n + 1) 
												* sizeof(char *));
				prompt_cmds[prompt_cmds_n] = (char *)xcalloc(strlen(line) + 1, 
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
		_err('e', PRINT_PROMPT, "%s: alias: '%s': %s\n", PROGRAM_NAME, 
			 CONFIG_FILE, strerror(errno));
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

	while ((line_len = getline(&line, &line_size, config_file_fp)) > 0) {
		
		if (strncmp(line, "alias ", 6) == 0) {
			char *alias_line = strchr(line, ' ');	
			if (alias_line) {
				alias_line++;
				aliases = (char **)xrealloc(aliases, (aliases_n + 1)
											* sizeof(char *));
				aliases[aliases_n] = (char *)xcalloc(strlen(alias_line) + 1, 
													 sizeof(char));
				strcpy(aliases[aliases_n++], alias_line);
			}
		}
	}
	
	free(line);

	fclose(config_file_fp);
}

char **
check_for_alias(char **comm)
/* Returns the parsed aliased command in an array of string, if matching 
 * alias is found, or NULL if not. */
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
					alias_comm = (char **)xrealloc(alias_comm, (++args_n + 1) * 
												   sizeof(char *));
					alias_comm[args_n] = (char *)xcalloc(strlen(comm[j]) + 1, 
														 sizeof(char));
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
/* Take the command entered by the user, already splitted into substrings by
 * parse_input_str(), and call the corresponding function. Return zero in 
 * case of success and one in case of error */
{
/*	if (!comm || *comm[0] == 0x00)
		return EXIT_FAILURE; */

	fputs(default_color, stdout);

	/* Exit flag. exit_code is zero (sucess) by default. In case of error
	 * in any of the functions below, it will be set to one (failure).
	 * This will be the value returned by this function. Used by the \z
	 * escape code in the prompt to print the exit status of the last executed
	 * command */
	exit_code = 0;

	/* If a user defined variable */
	if (flags & IS_USRVAR_DEF) {
		flags &= ~IS_USRVAR_DEF;
		
		exit_code = create_usr_var(comm[0]);
		return exit_code;
	}

	/* If double semi colon or colon (or ";:" or ":;") */
	if (comm[0][0] == ';' || comm[0][0] == ':') {
		if (comm[0][1] == ';' || comm[0][1] == ':') {
			fprintf(stderr, _("%s: '%.2s': Syntax error\n"), PROGRAM_NAME, 
					comm[0]);
			exit_code = 1;
			return EXIT_FAILURE;
		}
	}

	/* The more often a function is used, the more on top should it be in this 
	 * if...else..if chain. It will be found faster this way. */

	/* ####################################################   
	 * #				 BUILTIN COMMANDS 				  #     
	 * ####################################################*/	


	/*          ############### CD ##################     */
	if (strcmp(comm[0], "cd") == 0) {
		if (strcmp(comm[1], "--help") == 0)
			puts(_("Usage: cd [ELN/DIR]"));

		else if (strncmp(comm[1], "sftp://", 7) == 0)
			exit_code = remote_ssh(comm[1] + 7, (comm[2]) ? comm[2] : NULL);
		else if (strncmp(comm[1], "smb://", 6) == 0)
			exit_code = remote_smb(comm[1] + 6, (comm[2]) ? comm[2] : NULL);
		else if (strncmp(comm[1], "ftp://", 6) == 0) 
			exit_code = remote_ftp(comm[1] + 6, (comm[2]) ? comm[2] : NULL);
		else
			exit_code = cd_function(comm[1]);
	}

	/*         ############### OPEN ##################     */
	else if (strcmp(comm[0], "o") == 0	|| strcmp(comm[0], "open") == 0) {
		if (!comm[1] || strcmp(comm[1], "--help") == 0)
			puts(_("Usage: o, open ELN/FILE [APPLICATION]"));
		else if (strncmp(comm[1], "sftp://", 7) == 0)
			exit_code = remote_ssh(comm[1] + 7, (comm[2]) ? comm[2] : NULL);
		else if (strncmp(comm[1], "smb://", 6) == 0)
			exit_code = remote_smb(comm[1] + 6, (comm[2]) ? comm[2] : NULL);
		else if (strncmp(comm[1], "ftp://", 6) == 0)
			exit_code = remote_ftp(comm[1] + 6, (comm[2]) ? comm[2] : NULL);
		else
			exit_code = open_function(comm);
	}
	
	else if (strcmp(comm[0], "rf") == 0 || strcmp(comm[0], "refresh") == 0) {
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
	
	else if (strcmp(comm[0], "bh") == 0 || strcmp(comm[0], "fh") == 0) {
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
				exit_code = 1;
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
		if (!comm[1] || strcmp(comm[1], "--help") == 0) {
			puts(_("Usage: s, sel ELN ELN-ELN FILE ... n"));
			return EXIT_SUCCESS;
		}
		exit_code = sel_function(comm);
	}
	
	else if (strcmp(comm[0], "sb") == 0 || strcmp(comm[0], "selbox") == 0)
		show_sel_files();
	
	else if (strcmp(comm[0], "ds") == 0 || strcmp(comm[0], "desel") == 0) {
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
	
	/*  ############### SOME SHELL CMDS WRAPPERS ##################  */
	else if (strcmp(comm[0], "rm") == 0 || strcmp(comm[0], "mkdir") == 0 
	|| strcmp(comm[0], "touch") == 0 || strcmp(comm[0], "ln") == 0 
	|| strcmp(comm[0], "unlink") == 0 || strcmp(comm[0], "r") == 0
	|| strcmp(comm[0], "l") == 0 || strcmp(comm[0], "md") == 0) {
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
		if (!comm[1] || strcmp(comm[1], "--help") == 0) {
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
		exit_code = run_history_cmd(comm[0]+1);
	

	/*     ############### MINOR FUNCTIONS ##################     */

					/* #### NEW INSTANCE #### */
	else if (strcmp(comm[0], "x") == 0) {
		if (comm[1])
			exit_code = new_instance(comm[1]);
		else
			puts("Usage: x DIR\n");
	}
	
						/* #### NET #### */
	else if (strcmp(comm[0], "n") == 0 || strcmp(comm[0], "net") == 0) {
		if (!comm[1]) {
			puts(_("Usage: n, net [sftp, smb, ftp]://ADDRESS [OPTIONS]"));
			return EXIT_SUCCESS;
		}
		if (strncmp(comm[1], "sftp://", 7) == 0)
			exit_code = remote_ssh(comm[1] + 7, (comm[2]) ? comm[2] : NULL);
		else if (strncmp(comm[1], "smb://", 6) == 0)
			exit_code = remote_smb(comm[1] + 6, (comm[2]) ? comm[2] : NULL);
		else if (strncmp(comm[1], "ftp://", 6) == 0)
			exit_code = remote_ftp(comm[1] + 6, (comm[2]) ? comm[2] : NULL);
		else {
			fputs(_("Usage: n, net [sftp, smb, ftp]://ADDRESS [OPTIONS]\n"),
				  stderr);
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
			exit_code = 1;
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
		if (!comm[1] || strcmp(comm[1], "--help") == 0) 
			puts(_("Usage: ext [on, off, status]"));					
		else {
			if (strcmp(comm[1], "status") == 0)
				printf(_("%s: External commands %s\n"), PROGRAM_NAME, 
					    (ext_cmd_ok) ? _("enabled") : _("disabled"));
			else if (strcmp(comm[1], "on") == 0) {
				ext_cmd_ok = 1;
				printf(_("%s: External commands enabled\n"), PROGRAM_NAME);
			}
			else if (strcmp(comm[1], "off") == 0) {
				ext_cmd_ok = 0;
				printf(_("%s: External commands disabled\n"), PROGRAM_NAME);
			}
			else {
				fputs(_("Usage: ext [on, off, status]\n"), stderr);
				exit_code = 1;
			}
		}
	}
	
				/* #### PAGER #### */
	else if (strcmp(comm[0], "pg") == 0 || strcmp(comm[0], "pager") == 0) {
		if (!comm[1] || strcmp(comm[1], "--help") == 0) 
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
				fputs(_("Usage: pager, pg [on, off, status]\n"), stderr);
				exit_code = 1;
			}
		}
	}

					/* #### DIR COUNTER #### */
	else if (strcmp(comm[0], "dc") == 0 || strcmp(comm[0], "dircounter") == 0) {
		if (!comm[1]) {
			puts("Usage: dc, dircounter [on, off, status]");
			return EXIT_SUCCESS;
		}
		
		if (strcmp(comm[1], "on") == 0) {
			if (dir_counter == 1) {
				puts("Dircounter is already enabled");
				return EXIT_SUCCESS;
			}
			dir_counter = 1;
		}
		else if (strcmp(comm[1], "off") == 0) {
			if (dir_counter == 0) {
				puts("Dircounter is already disabled");
				return EXIT_SUCCESS;
			}
			dir_counter = 0;
		}
		else if (strcmp(comm[1], "status") == 0) {
			if (dir_counter)
				puts("Dircounter is enabled");
			else
				puts("Dircounter is disabled");
		}
		else {
			fputs("Usage: dc, dircounter [on, off, status]\n", stderr);
			return EXIT_FAILURE;
		}
	}

				    /* #### UNICODE #### */
	else if (strcmp(comm[0], "uc") == 0 || strcmp(comm[0], "unicode") == 0) {
		if (!comm[1] || strcmp(comm[1], "--help") == 0)
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
				fputs(_("Usage: unicode, uc [on, off, status]\n"), stderr);
				exit_code = 1;
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
					     (list_folders_first) ? _("enabled") : _("disabled"));
			else if (strcmp(comm[n], "on") == 0)
				list_folders_first = 1;
			else if (strcmp(comm[n], "off") == 0)
				list_folders_first = 0;
			else {
				fputs(_("Usage: folders first, ff [on, off, status]\n"), 
					    stderr);
				exit_code = 1;
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
			fputs(_("Usage: folders first, ff [on, off, status]\n"), stderr);
			exit_code = 1;
		}
	}
	
				/* #### LOG #### */
	else if (strcmp(comm[0], "log") == 0) {
		if (comm[1] && strcmp(comm[1], "--help") == 0) {
			puts("Usage: log [clear]");
			return EXIT_SUCCESS;
		}

		/* I make this check here, and not in the function itself, because
		 * this function is also called by other instances of the program
		 * where no message should be printed */
		if (!config_ok) {
			fprintf(stderr, _("%s: Log function disabled\n"), PROGRAM_NAME);
			exit_code = 1;
			return EXIT_FAILURE;
		}
		
		exit_code = log_function(comm);
	}
	
				/* #### MESSAGES #### */
	else if (strcmp(comm[0], "msg") == 0 || strcmp(comm[0], "messages") == 0) {
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
				puts(_("Usage: alias"));
				return EXIT_SUCCESS;
			}
			else if (strcmp(comm[1], "import") == 0) {
				if (!comm[2]) {
					fprintf(stderr, _("Usage: alias import FILE\n"));
					exit_code = 1;
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
		if (!comm[1] || strcmp(comm[1], "--help") == 0) {
			/* The same message is in hidden_function(), and printed
			 * whenever an invalid argument is entered */
			puts(_("Usage: hidden, hf [on, off, status]")); 
			return EXIT_SUCCESS;
		}
		else
			exit_code = hidden_function(comm);
	}

			  /* #### ANS THESE ONES TOO #### */
	/* These functions just print stuff, so that the value of exit_code is 
	 * always zero, that is to say, success */
	else if (strcmp(comm[0], "path") == 0 || strcmp(comm[0], "cwd") == 0) 
		printf("%s\n", path);
	
	else if (strcmp(comm[0], "help") == 0 || strcmp(comm[0], "?") == 0)
		help_function();
	
	else if (strcmp(comm[0], "cmd") == 0 || strcmp(comm[0], "commands") == 0) 
		list_commands();
	
	else if (strcmp(comm[0], "colors") == 0)
		color_codes();
	
	else if (strcmp(comm[0], "ver") == 0 || strcmp(comm[0], "version") == 0)
		version_function();
	
	else if (strcmp(comm[0], "license") == 0)
		print_license();
	
	else if (strcmp(comm[0], "fs") == 0)
		free_sotware();
	
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

		exit(EXIT_SUCCESS);
	}


	/* ####################################################   
	 * #				 EXTERNAL COMMANDS 				  #     
	 * ####################################################*/	
	else {
		/* IF NOT A COMMAND, BUT A DIRECTORY... */
		if (*comm[0] == '/') {
			struct stat file_attrib;
			if (lstat(comm[0], &file_attrib) == 0) {
				if ((file_attrib.st_mode & S_IFMT) == S_IFDIR ) {
					fprintf(stderr, _("%s: '%s': Is a directory\n"), 
							PROGRAM_NAME, comm[0]);
					exit_code = 1;
					return EXIT_FAILURE;
				}
			}
		}

		/* LOG EXTERNAL COMMANDS 
		* 'no_log' will be true when running profile or prompt commands */
		if (!no_log)
			exit_code = log_function(comm);
		
		/* PREVENT UNGRACEFUL EXIT */
		/* Prevent the user from killing the program via the 'kill', 'pkill' 
		 * or 'killall' commands, from within CliFM itself. Otherwise, the 
		 * program will be forcefully terminated without freeing allocated 
		 * memory */
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
					fprintf(stderr, _("%s: To gracefully quit enter 'quit'\n"), 
							PROGRAM_NAME);
					exit_code = 1;
					return exit_code;
				}
			}
		}
		
		/* CHECK WHETHER EXTERNAL COMMANDS ARE ALLOWED */
		if (!ext_cmd_ok) {
			fprintf(stderr, _("%s: External commands are not allowed. "
					"Run 'ext on' to enable them.\n"), PROGRAM_NAME);
			exit_code = 1;
			return EXIT_FAILURE;
		}
		
		/*
		 * By making precede the command by a colon or a semicolon, the user
		 * can BYPASS CliFM parsing, expansions, and checks to be executed 
		 * DIRECTLY by the system shell (execle). For example: if the amount 
		 * of files listed on the screen (ELN's) is larger or equal than 644 
		 * and the user tries to issue this command: "chmod 644 filename", 
		 * CLIFM will take 644 to be an ELN, and will thereby try to expand it 
		 * into the corresponding filename, which is not what the user wants. 
		 * To prevent this, simply run the command as follows: 
		 * ";chmod 644 filename" */

		if (comm[0][0] == ':' || comm[0][0] == ';') {
			/* Remove the colon from the beginning of the first argument,
			 * that is, move the pointer to the next (second) position */
			char *comm_tmp = comm[0] + 1;
			/* If string == ":" or ";" */
			if (!comm_tmp || comm_tmp[0] == 0x00) {
				fprintf(stderr, _("%s: '%c': Syntax error\n"), PROGRAM_NAME, 
						 comm[0][0]);
				exit_code = 1;
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

		/* Since we modified LS_COLORS, store its current value and unset it. 
		 * Some shell commands use LS_COLORS to display their outputs 
		 * ("ls -l", for example, use the "no" value to print file properties). 
		 * So, we unset it to prevent wrong color output for external commands. 
		 * The disadvantage of this procedure is that if the user uses a 
		 * customized LS_COLORS, unsetting it set its value to default, and the
		 * customization is lost. */
		static char *my_ls_colors = (char *)NULL;
		my_ls_colors = getenv("LS_COLORS");
		if (ls_colors_bk && *ls_colors_bk != 0x00)
			setenv("LS_COLORS", ls_colors_bk, 1);
		else
			unsetenv("LS_COLORS");
		
		exit_code = launch_execle(ext_cmd);
		free(ext_cmd);

		/* Restore LS_COLORS value to use CliFM colors */
		setenv("LS_COLORS", my_ls_colors, 1);

 		/* Reload the list of available commands in PATH for TAB completion.
 		 * Why? If this list is not updated, whenever some new program is 
 		 * installed or simply a new executable is added to some of the paths 
 		 * in PATH while in CliFM, this latter needs to be restarted in order 
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
		/* Go the the specified directory (first arg) in the directory history 
		 * list */
		int atoi_comm = atoi(comm[1] + 1);
		if (atoi_comm > 0 && atoi_comm <= dirhist_total_index) {
			int ret = chdir(old_pwd[atoi_comm - 1]);
			if (ret == 0) {
				free(path);
				path = (char *)xcalloc(strlen(old_pwd[atoi_comm - 1]) + 1, 
									   sizeof(char));
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
/* Execute a command using the system shell (/bin/sh), which takes care of
 * special functions such as pipes and stream redirection, and special chars
 * like wildcards, quotes, and escape sequences. Use only when the shell is
 * needed; otherwise, launch_execve() should be used instead. */
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

	/* Reenable SIGCHLD, in case it was disabled. Otherwise, waitpid won't be 
	 * able to catch error codes coming from the child */
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
		/* Reenable signals only for the child, in case they were disabled for
		* the parent */
		signal (SIGHUP, SIG_DFL);
		signal (SIGINT, SIG_DFL);
		signal (SIGQUIT, SIG_DFL);
		signal (SIGTERM, SIG_DFL);

		/* Get shell base name */
		char *name = strrchr(sys_shell, '/');

		/* This is basically what the system() function does: running a command
		 * via the system shell */
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
				/* The program terminated normally and executed successfully */
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
/* Execute a command and return the corresponding exit status. The exit status
* could be: zero, if everything went fine, or a non-zero value in case of 
* error. The function takes as first arguement an array of strings containing 
* the command name to be executed and its arguments (cmd), and an integer (bg)
* specifying if the command should be backgrounded (1) or not (0) */
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
	 * negative values (or > 255) so that my own exit codes won't be confused 
	 * with some standard exit code. However, the Linux Documentation Project 
	 * (tldp) recommends the use of codes 64-113 for user-defined exit codes.
	 * See: http://www.tldp.org/LDP/abs/html/exitcodes.html and
	 * https://www.gnu.org/software/libc/manual/html_node/Exit-Status.html 
	*/
	
	if (!cmd)
		return EXNULLERR;

	/* Reenable SIGCHLD, in case it was disabled. Otherwise, waitpid won't be 
	 * able to catch error codes coming from the child. */
	signal(SIGCHLD, SIG_DFL);

	/* Create a new process via fork() */
	pid_t pid = fork();
	if (pid < 0) {
		fprintf(stderr, "%s: fork: %s\n", PROGRAM_NAME, strerror(errno));
		return errno;
	}
	/* Run the command via execvp */
	else if (pid == 0) {
		/* Reenable signals only for the child, in case they were disabled for
		the parent */
		signal(SIGHUP, SIG_DFL);
		signal(SIGINT, SIG_DFL);
		signal(SIGQUIT, SIG_DFL);
		signal(SIGTERM, SIG_DFL);
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
		fprintf(stderr, "%s: waitpid: %s\n", PROGRAM_NAME, strerror(errno));
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
/* Save selected elements into a tmp file. Returns 1 if success and 0 if error. 
 * This function allows the user to work with multiple instances of the 
 * program: he/she can select some files in the first instance and then 
 * execute a second one to operate on those files as he/she whises. */
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
	int exists = 0, new_sel = 0, exit_status = 0;

	for (i = 1; i <= args_n; i++) {
		char *deq_file = dequote_str(comm[i], 0);
		if (!deq_file) {
			for (j = 0; j < sel_n; j++)
				free(sel_elements[j]);
			return EXIT_FAILURE;
		}
		size_t file_len = strlen(deq_file);
		/* Remove final slash from directories. No need to check if file is
		 * a directory, since in Linux only directories can contain a
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
					exit_status = 1;
					continue;
				}
			}
			else { /* If neither a filename in CWD nor a path... */
				fprintf(stderr, _("%s: sel: '%s': No such %s\n"), 
						PROGRAM_NAME, deq_file, 
						(is_number(deq_file)) ? "ELN" : "file or directory");
				free(deq_file);
				exit_status = 1;
				continue;
			}
		}
		if (sel_is_filename || sel_is_relative_path) { 
			/* Add path to filename or relative path */
			sel_tmp = (char *)xcalloc(strlen(path) + strlen(deq_file) + 2, 
									  sizeof(char));
			sprintf(sel_tmp, "%s/%s", path, deq_file);
		}
		else { /* If absolute path... */
			sel_tmp = (char *)xcalloc(strlen(deq_file) + 1, sizeof(char));		
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
			sel_elements = (char **)xrealloc(sel_elements, (sel_n + 1) * 
											 sizeof(char *));
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
	 * file*/
	if (save_sel() == 0) { /* If selected files were successfully written to
		sel file */
		if (sel_n > 10)
			printf(_("%zu elements are now in the Selection Box\n"), sel_n);
		else if (sel_n > 0) {
			printf(_("%zu selected %s:\n"), sel_n, (sel_n == 1) ? _("element") : 
				_("elements"));
			for (i = 0; i < sel_n; i++)
				printf("  %s\n", sel_elements[i]);
		}
		return exit_status;
	}
	else {
		if (sel_n > 0) { /* In case of error, remove sel files from memory */
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
	}
	if (reset_pager)
		pager = 1;
}

int
deselect(char **comm)
{
	if (!comm)
		return EXIT_FAILURE;

	if (comm[1] && (strcmp(comm[1], "*") == 0 || strcmp(comm[1], "a") == 0 
	|| strcmp(comm[1], "all") == 0)) {
		if (sel_n > 0) {
			size_t i;
			for (i = 0; i < sel_n; i++)
				free(sel_elements[i]);
			sel_n = 0;
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

	printf(_("\n%s%sEnter 'q' to quit.\n"), NC, default_color);
	size_t desel_n = 0;
	char *line = NULL, **desel_elements = (char **)NULL;

	while (!line)
		line = rl_no_hist(_("Element(s) to be deselected (ex: 1 2-6, or *): "));

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
				sel_n = 0;
				for (i = 0; i < desel_n; i++)
					free(desel_elements[i]);
				int exit_status = 0;
				if (save_sel() != 0)
					exit_status = 1;
				free(desel_elements);
				if (cd_lists_on_the_fly) {
					while (files--)
						free(dirlist[files]);
					if (list_dir() != 0)
						exit_status = 1;
				}
				return exit_status;
			}
			else {
				printf(_("desel: '%s': Invalid element\n"), desel_elements[i]);
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
				printf(_("desel: '%s': Invalid ELN\n"), desel_elements[i]);
				size_t j;
				for (j = 0; j < desel_n; j++)
					free(desel_elements[j]);
				free(desel_elements);
				return EXIT_FAILURE;				
			}
		}
	}

	/* If a valid ELN and not asterisk... */
	/* Store the full path of all the elements to be deselected in a new array 
	 * (desel_path). I need to do this because after the first rearragement
	 * of the sel array, that is, after the removal of the first element, the
	 * index of the next elements changed, and cannot thereby be found
	 * by their index. The only way to find them is to compare string by 
	 * string */
	char **desel_path = (char **)NULL;
	desel_path = (char **)xnmalloc(desel_n, sizeof(char *));

	for (i = 0; i < desel_n; i++) {
		int desel_int = atoi(desel_elements[i]);
		desel_path[i] = (char *)xnmalloc(strlen(sel_elements[desel_int - 1]) 
										 + 1, sizeof(char));
		strcpy(desel_path[i], sel_elements[desel_int - 1]);
	}

	/* Search the sel array for the path of the element to deselect and store 
	its index */
	for (i = 0; i < desel_n; i++) {
		size_t j, k, desel_index = 0;
		for (k = 0; k < sel_n; k++) {
			if (strcmp(sel_elements[k], desel_path[i]) == 0) {
				desel_index = k;
				break;
			}
		}
		
		/* Once the index was found, rearrange the sel array removing the 
		 * deselected element (actually, moving each string after it to the 
		 * previous position) */
		for (j = desel_index; j < (sel_n - 1); j++) {
			sel_elements[j] = (char *)xrealloc(sel_elements[j], 
										(strlen(sel_elements[j + 1]) + 1)
										* sizeof(char));
			strcpy(sel_elements[j], sel_elements[j + 1]);
		}
	}

	/* Free the last DESEL_N elements from the old sel array. They won't be 
	 * used anymore, for they contain the same value as the last non-deselected 
	 * element due to the above array rearrangement */
	for (i = 1; i <= desel_n; i++)
		if ((int)(sel_n - i) >= 0 && sel_elements[sel_n - i])
			free(sel_elements[sel_n - i]);

	/* Reallocate the sel array according to the new size */
	sel_n = (sel_n - desel_n);

	if ((int)sel_n < 0)
		sel_n = 0;

	if (sel_n)
		sel_elements = (char **)xrealloc(sel_elements, sel_n * sizeof(char *));

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

	int exit_status = 0;

	if (save_sel() != 0)
		exit_status = 1;
	
	/* If there is still some selected file, reload the sel screen */
	if (sel_n)
		if (deselect(comm) != 0)
			exit_status = 1;

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
	if (ret == 0) {
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

	/* If search string (comm[0]) is "/search", comm[0]+1 returns "search" */
	char *search_str = comm[0] + 1, *deq_dir = (char *)NULL, 
		 *search_path = (char *)NULL;
	mode_t file_type = 0;
	struct stat file_attrib;
	
	/* If there are two arguments, the one starting with '-' is the filetype 
	 * and the other is the path */
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

	/* If just one argument, '-' indicates filetype. Else, we have a path */
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
			fprintf(stderr, "%s: '%c': Unrecognized filetype\n", PROGRAM_NAME,
					file_type);
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
			size_t *files_len = (size_t *)xnmalloc(globbed_files.gl_pathc + 1,
													sizeof(size_t));

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
									(last_column || i == found - 1) ? 1 : 0);
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

					/* In case 'index' is not found in the next for loop, that 
					 * is, if the globbed file is not found in the current dir 
					 * list, 'index' value would be that of the previous file 
					 * if 'index' is not set to zero in each for iteration */
					index[found] = -1;
					for (j = 0; j < files; j++) {
						if (strcmp(globbed_files.gl_pathv[i], dirlist[j]) == 0) {
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
									(last_column || i == found - 1) ? 1 : 0);
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
				fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, path, 
						strerror(errno));
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
			
			pfiles = (char **)xnmalloc((size_t)search_files + 1, sizeof(char *));
			size_t *files_len = (size_t *)xnmalloc((size_t)search_files + 1, 
												   sizeof(size_t)); 

			for (i = 0; i < (size_t)search_files; i++) {
				if (strstr(search_list[i]->d_name, search_str)) {
					if (file_type) {

						if (lstat(search_list[i]->d_name, &file_attrib) == -1)
							continue;

						if ((file_attrib.st_mode & S_IFMT) == file_type) {
							pfiles[found] = search_list[i]->d_name;
							len = (unicode) ? u8_xstrlen(search_list[i]->d_name) 
								  : strlen(search_list[i]->d_name);
							files_len[found++] = len;
							if (len > flongest)
								flongest = len;
						}
					}
					else {
						pfiles[found] = search_list[i]->d_name;
						len = (unicode) ? u8_xstrlen(search_list[i]->d_name) 
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
					
					colors_list(pfiles[i], 0, (last_column || i == found - 1) 
								? 0 : (int)(flongest - files_len[i]) + 1, 
								(last_column || i == found - 1) ? 1 : 0);
				}
			}
			
			for (i = 0; i < (size_t)search_files; i++)
				free(search_list[i]);
			free(search_list);
			
			free(files_len);
			free(pfiles);

			if (chdir(path) == -1) {
				fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, path, 
						strerror(errno));

				free(deq_dir);

				return EXIT_FAILURE;
			}
		}
		
		/* If /search_str */
		else {
			int *index = (int *)xnmalloc(files + 1, sizeof(int));
			pfiles = (char **)xnmalloc(files + 1, sizeof(char *));
			size_t *files_len = (size_t *)xnmalloc(files + 1, sizeof(size_t));

			for (i = 0; i < files; i++) {
				/* strstr finds substr in STR, as if STR where "*substr*" */
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
		
		if (ret == 0) {
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
/* Get bookmarks from BOOKMARKS_FILE, store them in the bookmarks array and
 * the amount of bookmarks in the global variable bm_n, used later by 
 * bookmarks_function() */
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
		bookmarks = (char **)xrealloc(bookmarks, (bm_n + 1) * sizeof(char *));
		bookmarks[bm_n] = (char *)xcalloc((size_t)line_len + 1, sizeof(char));
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
	printf("%s%s\nEnter 'e' to edit your bookmarks or 'q' to quit.\n", NC_b, 
		   default_color);

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
	/* This variable let us know two things: a) bookmark name was specified 
	 * in command line; b) the index of this name in the bookmarks array. It is
	 * initialized as -1 since the index name could be zero */
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
			printf("%s%zu %s%s%s\n", eln_color, i + 1, d_cyan, bms[i], NC);

		/* Get user input */
		printf(_("\n%s%sEnter 'q' to quit.\n"), NC, default_color);
		char *input = (char *)NULL;
		while (!input)
			input = rl_no_hist("Bookmark(s) to be deleted (ex: 1 2-6, or *): ");

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
		&& (atoi(del_elements[i]) <= 0 || atoi(del_elements[i]) > (int)bmn)) {
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
	/* If there is some "*" in the input line (like "1 5 6-9 *"), it makes 
	 * no sense to remove singles bookmarks: Just delete all of them at 
	 * once */
	for (i = 0; del_elements[i]; i++) {
		if (strcmp(del_elements[i], "*") == 0) {
			/* Create a backup copy of the bookmarks file, just in case */
			char *bk_file = (char *)NULL;
			bk_file = (char *)xcalloc(strlen(CONFIG_DIR) + 14, sizeof(char));
			sprintf(bk_file, "%s/bookmarks.bk", CONFIG_DIR);
			char *tmp_cmd[] = { "cp", BM_FILE, bk_file, NULL };
			int ret = launch_execve(tmp_cmd, FOREGROUND);
			/* Remove the bookmarks file, free stuff, and exit */
			if (ret == 0) {
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
			tmp = (char *)xcalloc(strlen(hk) + strlen(name) + strlen(file) + 5, 
								  sizeof(char));
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
		tmp = (char *)xcalloc(strlen(hk) + strlen(file) + 4, sizeof(char));
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
		printf(_("Bookmarks: There are no bookmarks\nEnter 'bm edit' to edit "
				 "the bookmarks file or 'bm add PATH' to add a new "
				 "bookmark\n"));
		return EXIT_SUCCESS;
	}


	/* If there are bookmarks... */

	/* Store shortcut, name, and path of each bookmark into different arrays 
	 * but linked by the array index */

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
			/* If there is some slash in the shortcut or in the name string, 
			 * the bookmark path will be wrong. FIX! Or just disallow the use
			 * of slashes for bookmark names and shortcuts */
			bm_paths[i] = (char *)xnmalloc(strlen(bookmarks[i] + ret) + 1,
										  sizeof(char));
			strcpy(bm_paths[i], bookmarks[i] + ret);
		}
		else
			bm_paths[i] = (char *)NULL;

		/* Get shortcuts */
		char *str_b = strbtw(bookmarks[i], '[', ']');
		if (str_b) {
			hot_keys[i] = (char *)xnmalloc(strlen(str_b) + 1, sizeof(char));
			strcpy(hot_keys[i], str_b);
			free(str_b);
			str_b = (char *)NULL;
		}
		else 
			hot_keys[i] = (char *)NULL;

		/* Get names */
		char *str_name = strbtw(bookmarks[i], ']', ':');
		if (str_name) {
			bm_names[i] = (char *)xnmalloc(strlen(str_name) + 1, sizeof(char));
			strcpy(bm_names[i], str_name);
			free(str_name);
		}
		else {
			str_name = strbfr(bookmarks[i], ':');
			if (str_name) {
				bm_names[i] = (char *)xnmalloc(strlen(str_name) + 1, 
											  sizeof(char));
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

		/* Print bookmarks taking into account the existence of shortcut, name,
		 * and path for each bookmark */
		for (i = 0; i < bm_n; i++) {
			int path_ok = stat(bm_paths[i], &file_attrib);
			if (hot_keys[i]) {
				if (bm_names[i]) {
					if (path_ok == 0) {
						/* Shortcut, name, and path OK */
						/* Directory or symlink to directory */
						if ((file_attrib.st_mode & S_IFMT) == S_IFDIR)
							printf("%s%zu %s[%s]%s %s%s%s\n", eln_color, i + 1, 
									white, hot_keys[i], NC, cyan, 
									bm_names[i], NC);
						else /* No directory */
							printf("%s%zu %s[%s]%s %s%s%s\n", eln_color, i + 1, 
									white, hot_keys[i], NC, default_color, 
									bm_names[i], NC);					
					}
					else /* Invalid path */
						printf("%s%zu [%s] %s%s\n", gray, i + 1, hot_keys[i], 
								bm_names[i], NC);
				}
				else {
					if (path_ok == 0) { /* Shortcut and path OK */
						if ((file_attrib.st_mode & S_IFMT) == S_IFDIR)
							printf("%s%zu %s[%s]%s %s%s%s\n", eln_color, i + 1, 
									white, hot_keys[i], NC, cyan, 
									bm_paths[i], NC);
						else
							printf("%s%zu %s[%s]%s %s%s%s\n", eln_color, i + 1, 
									white, hot_keys[i], NC, default_color, 
									bm_paths[i], NC);		
					}
					else
						printf("%s%zu [%s] %s%s\n", gray, i + 1, hot_keys[i], 
								bm_paths[i], NC);
				}
			}
			else {
				if (bm_names[i]) {
					if (path_ok == 0) { /* Name and path OK */
						if ((file_attrib.st_mode & S_IFMT) == S_IFDIR)
							printf("%s%zu %s%s%s\n", eln_color, i + 1, cyan, 
									bm_names[i], NC);
						else
							printf("%s%zu %s%s%s%s\n", eln_color, i + 1, NC, 
									default_color, bm_names[i], NC);				
					}
					else
						printf("%s%zu %s%s\n", gray, i + 1, bm_names[i], NC);
				}
				else {
					if (path_ok == 0) { /* Only path OK */
						if ((file_attrib.st_mode & S_IFMT) == S_IFDIR)
							printf("%s%zu %s%s%s\n", eln_color, i + 1, cyan, 
									bm_paths[i], NC);
						else
							printf("%s%zu %s%s%s%s\n", eln_color, i + 1, NC, 
									default_color, bm_paths[i], NC);	
					}
					else
						printf("%s%zu %s%s\n", gray, i + 1, bm_paths[i], NC);
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
				fprintf(stderr, _("Bookmarks: 'file' command not found. Try "
								  "'e APPLICATION'\n"));
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
			if ((ret = launch_execve(tmp_cmd, FOREGROUND)) != 0)
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
		fprintf(stderr, _("Bookmarks: '%s': No such bookmark\n"), arg[0]);
		error_code = 1;
		goto free_and_exit;
	}

	/* Now we have the corresponding bookmark path */
	/* Check path existence */
	if (stat(tmp_path, &file_attrib) == -1) {
		fprintf(stderr, "Bookmarks: '%s': %s\n", tmp_path, strerror(errno));
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
			if ((ret = launch_execve(tmp_cmd, FOREGROUND)) != 0)
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
	 * should be created at startup (by get_bm_names()), but we check it
	 * again here just in case it was meanwhile deleted for some reason */
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
			fprintf(fp, "#Example: [t]test:/path/to/test\n");
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
		else if (strcmp(cmd[1], "d") == 0 || strcmp(cmd[1], "del") == 0) {
			if (cmd[2])
				return bookmark_del(cmd[2]);
			else
				return bookmark_del(NULL);
		}
	}
	
	/* If no arguments or "bm [edit] [shortcut, name]" */
	return open_bookmark(cmd);
}

void
dir_size(char *dir)
{
	#ifndef DU_TMP_FILE
		#define DU_TMP_FILE "/tmp/.du_size"
	#endif

	if (!dir)
		return;

	FILE *du_fp = fopen(DU_TMP_FILE, "w");
	FILE *du_fp_err = fopen("/dev/null", "w");
	int stdout_bk = dup(STDOUT_FILENO); /* Save original stdout */
	int stderr_bk = dup(STDERR_FILENO); /* Save original stderr */
	dup2(fileno(du_fp), STDOUT_FILENO); /* Redirect stdout to the desired file */	
	dup2(fileno(du_fp_err), STDERR_FILENO); /* Redirect stderr to /dev/null */
	fclose(du_fp);
	fclose(du_fp_err);

/*	char *cmd = (char *)NULL;
	cmd = (char *)xcalloc(strlen(dir) + 10, sizeof(char));
	sprintf(cmd, "du -sh '%s'", dir);
	//int ret = launch_execle(cmd);
	launch_execle(cmd);
	free(cmd); */

	char *cmd[] = { "du", "-sh", dir, NULL };
	launch_execve(cmd, FOREGROUND);
	
	dup2(stdout_bk, STDOUT_FILENO); /* Restore original stdout */
	dup2(stderr_bk, STDERR_FILENO); /* Restore original stderr */
	close(stdout_bk);
	close(stderr_bk);

/*	if (ret != 0) {
		puts("???");
		return;
	} */

	if (access(DU_TMP_FILE, F_OK) == 0) {
		du_fp = fopen(DU_TMP_FILE, "r");
		if (du_fp) {
			/* I only need here the first field of the line, which is a file
			 * size and could only take a few bytes, so that 32 bytes is
			 * more than enough */
			char line[32] = "";
			fgets(line, sizeof(line), du_fp);
			
			char *file_size = strbfr(line, '\t');
			
			if (file_size)
				printf("%siB\n", file_size);
			else 
				puts("???");
			
			free(file_size);
			fclose(du_fp);
		}
		else
			puts(_("???"));
		remove(DU_TMP_FILE);
	}
	else 
		puts(_("???"));
}

int
properties_function(char **comm)
{
	if(!comm)
		return EXIT_FAILURE;

	int exit_status = 0;

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
				exit_status = 1;
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

			/* Get the amount of continuation bytes for each filename. This
			 * is necessary to properly format the output in case of non-ASCII
			 * strings */
			(unicode) ? u8_xstrlen(dirlist[i]) : strlen(dirlist[i]);

			lstat(dirlist[i], &file_attrib);
			char *size = get_size_unit(file_attrib.st_size);
			/* Print: 		filename		ELN		Padding		no new line*/
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
			exit_status = 1;
			continue;
		}

		if (get_properties(deq_file, 0, 0, 0) != 0)
			exit_status = 1;
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
		/* Store formatted (and localized) date-time string into mod_time */
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

/*		size_t filename_len = ((unicode) ? u8_xstrlen(filename) : 
								strlen(filename)); */
								
		/*	If filename length is greater than max, truncate it (max-1 = '~')
		 * to let the user know the filename isn't complete */
		 /* The value of max (global) is (or should be) already calculated by 
		  * get_max_long_view() before calling this function */
		char trim_filename[NAME_MAX] = "";
		short trim = 0;
		if (filename_len > (size_t)max) {
			trim = 1;
			strcpy(trim_filename, filename);
			trim_filename[(max + cont_bt) - 1] = '~';
			trim_filename[max + cont_bt] = 0x00;
		}
		
		/* Calculate pad for each file (not perfect, but almost) */
		int pad;

		if (longest > (size_t)max) /* Long files are trimmed */
			pad = (max + cont_bt) - ((!trim) ? (int)filename_len
				  : (max + cont_bt));

		else /* No trimmed files */
			pad = (int)(longest - (eln_len + 1 + filename_len));
			
		if (pad < 0)
			pad = 0;
		
		printf("%s%s%s%-*s%s (%04o) %c/%c%c%c/%c%c%c/%c%c%c%s %s %s %s %s\n", 
				color, (!trim) ? filename : trim_filename, NC, pad, "", 
				default_color, file_attrib.st_mode & 07777,
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
			printf("%s%s%s%s -> ???\n", color, filename, NC, default_color);
	}
	
	/* Stat information */

	/* Last access time */
	time = file_attrib.st_atim.tv_sec;
	tm = localtime(&time);
	char access_time[128] = "";
	if (time)
		/* Store formatted (and localized) date-time string into access_time */
		strftime(access_time, sizeof(access_time), "%b %d %H:%M:%S %Y", tm);
	else
		access_time[0] = '-';

	/* Last properties change time */
	time = file_attrib.st_ctim.tv_sec;
	tm = localtime(&time);
	char change_time[128] = "";
	if (time)
		strftime(change_time, sizeof(change_time), "%b %d %H:%M:%S %Y", tm);
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
			strftime(creation_time, sizeof(creation_time), "%b %d %H:%M:%S %Y", 
					 tm);
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
			strftime(creation_time, sizeof(creation_time), "%b %d %H:%M:%S %Y", 
					 tm);
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
	printf(_("\tUid: %u (%s)"), file_attrib.st_uid, (!owner) ? _("unknown") 
		    : owner->pw_name);
	printf(_("\tGid: %u (%s)\n"), file_attrib.st_gid, (!group) ? _("unknown") 
		   : group->gr_name);

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
		dir_size(filename);
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
	int exit_status = 0;

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
			_err(0, NOPRINT_PROMPT, "%s: log: '%s': %s\n", PROGRAM_NAME, 
				 LOG_FILE, strerror(errno));
			return EXIT_FAILURE;
		}
		else {
			size_t line_size = 0;
			char *line_buff = (char *)NULL;
			ssize_t line_len = 0;
			
			while ((line_len = getline(&line_buff, &line_size, log_fp)) > 0)
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
			/* If logs were already disabled, just exit. Otherwise, log the
			 * "log off" command */
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
		/* last_cmd should never be NULL if logs are enabled (this variable is
		 * set immediately after taking valid user input in the prompt function). 
		 * However ... */
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
	/* Else, overwrite the log file leaving only the 'log clear' command */
	else 
		log_fp = fopen(LOG_FILE, "w+");

	if (!log_fp) {
		_err('e', PRINT_PROMPT, "%s: log: '%s': %s\n", PROGRAM_NAME, LOG_FILE,
			 strerror(errno));
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
			_err(0, NOPRINT_PROMPT, "%s: '%s': %s\n", PROGRAM_NAME, log_file, 
				 strerror(errno));
		}
		else
			fclose(log_fp);
		return; /* Return anyway, for, being a new empty file, there's no need
		to truncate it */
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
			while ((line_len = getline(&line_buff, &line_size, log_fp)) > 0)
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
		_err(0, NOPRINT_PROMPT, "%s: log: %s: %s\n", PROGRAM_NAME, log_file, 
			 strerror(errno));
}

int
get_history(void)
{
	if (!config_ok)
		return EXIT_FAILURE;

	FILE *hist_fp = fopen(HIST_FILE, "r");
	if (current_hist_n == 0) { /* Coming from main() */
		history = (char **)xcalloc(1, sizeof(char *));
	}
	else { /* Only true when comming from 'history clear' */
		size_t i;
		for (i = 0; i < current_hist_n; i++)
			free(history[i]);
		history = (char **)xrealloc(history, 1 * sizeof(char *));
		current_hist_n = 0;
	}
	
	if (hist_fp) {

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
	
	else {
		_err('e', PRINT_PROMPT, "%s: history: '%s': %s\n", PROGRAM_NAME, 
			 HIST_FILE,  strerror(errno));
		return EXIT_FAILURE;
	}
}

int
history_function(char **comm)
{
	if (!config_ok) {
		fprintf(stderr, _("%s: History function disabled\n"), PROGRAM_NAME);
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
			_err(0, NOPRINT_PROMPT, "%s: history: %s: %s\n", PROGRAM_NAME, 
				 HIST_FILE, strerror(errno));
			return EXIT_FAILURE;
		}

		/* Do not create an empty file */
		fprintf(hist_fp, "%s %s\n", comm[0], comm[1]);
		fclose(hist_fp);

		/* Update the history array */
		int exit_status = 0;
		if (get_history() != 0)
			exit_status = 1;
		if (log_function(comm) != 0)
			exit_code = 1;
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
 * Example: if exec_cmd() gets "!-10" it pass to this function "-10", that 
 * is, comm+1 */
{
	/* If "!n" */
	int exit_status = 0;
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
					/* If an alias is found, the function frees cmd_hist and 
					 * returns alias_cmd in its place to be executed by 
					 * exec_cmd() */
					if (exec_cmd(alias_cmd) != 0)
						exit_status = 1;
					for (i = 0; alias_cmd[i]; i++)
						free(alias_cmd[i]);
					free(alias_cmd);
					alias_cmd = (char **)NULL;
				}

				else {
					if (exec_cmd(cmd_hist) != 0)
						exit_status = 1;
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
					exit_status = 1;
				for (i = 0; alias_cmd[i]; i++)
					free(alias_cmd[i]);
				free(alias_cmd);
				alias_cmd = (char **)NULL;
			}			

			else {
				if (exec_cmd(cmd_hist) != 0)
					exit_status = 1;
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
		char **cmd_hist = parse_input_str(history[current_hist_n - (size_t)acmd 
												  - 1]);
		if (cmd_hist) {
			
			char **alias_cmd = check_for_alias(cmd_hist);
			if (alias_cmd) {
				if (exec_cmd(alias_cmd) != 0)
					exit_status = 1;
				for (i = 0; alias_cmd[i]; i++)
					free(alias_cmd[i]);
				free(alias_cmd);
				alias_cmd = (char **)NULL;
			}

			else {
				if (exec_cmd(cmd_hist) != 0)
					exit_status = 1;
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
/* Edit the config file, either via my mime function or via the first passed 
 * argument (Ex: 'edit nano') */
{
	if (!config_ok) {
		fprintf(stderr, _("%s: Cannot access the configuration file\n"), 
				PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	/* Get modification time of the config file before opening it */
	struct stat file_attrib;

	/* If, for some reason (like someone erasing the file while the program 
	 * is running) clifmrc doesn't exist, call init_config() to recreate the 
	 * configuration file. Then run 'stat' again to reread the attributes of 
	 * the file */ 
	if (stat(CONFIG_FILE, &file_attrib) == -1) {
		free(CONFIG_DIR);
		free(TRASH_DIR);
		free(TRASH_FILES_DIR);
		free(TRASH_INFO_DIR);
		CONFIG_DIR = TRASH_DIR = TRASH_FILES_DIR = TRASH_INFO_DIR = (char *)NULL;
		
		free(BM_FILE);
		free(LOG_FILE);
		free(LOG_FILE_TMP);
		free(HIST_FILE);
		BM_FILE = LOG_FILE = LOG_FILE_TMP = HIST_FILE = (char *)NULL;
		
		free(CONFIG_FILE);
		free(PROFILE_FILE);
		free(MSG_LOG_FILE);
		free(sel_file_user);
		CONFIG_FILE = PROFILE_FILE = MSG_LOG_FILE = sel_file_user = (char *)NULL;

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
		if (ret != 0)
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
		CONFIG_DIR = TRASH_DIR = TRASH_FILES_DIR = TRASH_INFO_DIR = (char *)NULL;
		
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
	printf(_("%s file name%s%s: Directory with no read permission (nd)\n"), 
		   nd_c, NC, default_color);
	printf(_("%s file name%s%s: File with no read permission (nf)\n"), 
		   nf_c, NC, default_color);
	printf(_("%s file name%s%s: Directory* (di)\n"), di_c, NC, default_color);
	printf(_("%s file name%s%s: EMPTY directory (ed)\n"), ed_c, NC, 
		   default_color);
	printf(_("%s file name%s%s: EMPTY directory with no read permission \
(ne)\n"), ne_c, NC, default_color);
	printf(_("%s file name%s%s: Executable file (ex)\n"), ex_c, NC, 
		   default_color);
	printf(_("%s file name%s%s: Empty executable file (ee)\n"), ee_c, NC, 
		   default_color);
	printf(_("%s file name%s%s: Block special file (bd)\n"), bd_c, NC, 
		   default_color);	
	printf(_("%s file name%s%s: Symbolic link (ln)\n"), ln_c, NC, 
		   default_color);	
	printf(_("%s file name%s%s: Broken symbolic link (or)\n"), or_c, NC, 
		   default_color);
	printf(_("%s file name%s%s: Socket file (so)\n"), so_c, NC, 
		   default_color);
	printf(_("%s file name%s%s: Pipe or FIFO special file (pi)\n"), pi_c, NC, 
		   default_color);
	printf(_("%s file name%s%s: Character special file (cd)\n"), cd_c, NC, 
		   default_color);
	printf(_("%s file name%s%s: Regular file (fi)\n"), fi_c, NC, 
		   default_color);
	printf(_("%s file name%s%s: Empty (zero-lenght) file (ef)\n"), ef_c, NC, 
		   default_color);
	printf(_(" %s%sfile name%s%s: SUID file (su)\n"), NC, su_c, NC, 
		   default_color);
	printf(_(" %s%sfile name%s%s: SGID file (sg)\n"), NC, sg_c, NC, 
		   default_color);
	printf(_(" %s%sfile name%s%s: File with capabilities (ca)\n"), NC, ca_c, 
		   NC, default_color);
	printf(_(" %s%sfile name%s%s: Sticky and NOT other-writable directory* \
(st)\n"),  NC, st_c, NC, default_color);
	printf(_(" %s%sfile name%s%s: Sticky and other-writable directory* \
(tw)\n"),  NC, tw_c, NC, default_color);
	printf(_(" %s%sfile name%s%s: Other-writable and NOT sticky directory* \
(ow)\n"),  NC, ow_c, NC, default_color);
	printf(_(" %s%sfile name%s%s: Unknown file type (no)\n"), NC, no_c, 
		   NC, default_color);
	printf(_("\n*The slash followed by a number (/xx) after directory names \
indicates the amount of files contained by the corresponding directory.\n"));
	printf(_("\nThe value in parentheses is the code to use to modify \
the color of the corresponding filetype in the configuration file \
(in the \"FiletypeColors\" line), using the same ANSI style color format \
used by dircolors. By default, %s uses only 8 colors, but you can \
use 256 and RGB colors as well.\n\n"), PROGRAM_NAME);
}

void
list_commands (void)
{
	printf(_("\nNOTE: ELN = Element List Number. Example: In \
the line \"12 openbox\", 12 is the ELN corresponding to the 'openbox' \
file.\n"));

	/* ### SEARCH ### */
	printf(_("\n%s/%s%s* [-filetype] [DIR]: This is the quick search function. \
Just type '/' followed by the string you are looking for (you can use \
wildcards), and %s will list all matches in the current working directory. \
To search for files in any other directory, specify the directory name as \
another argument. This argument (DIR) could be an absolute path, a relative \
path, or an ELN. It is also possible to further filter the results of the \
search by filetype, specifying it as follows: -d, -r, -l, -f, -s, -b, -c \
(directory, regular file, symlink, FIFO/pipe, socket, block device, and \
character device respectivelly). Example: '/*x -d /Documents' will list all \
directories in the directory \"Documents\" ending with 'x'\n"), 
		   white, NC, default_color, PROGRAM_NAME);

	/* ### BOOKMARKS ### */
	printf(_("\n%sbm, bookmarks%s%s [a, add PATH] [d, del] [edit] [shortcut, \
name]: With no argument, open the bookmarks menu. Here you can change the \
current directory to that specified by the corresponding bookmark by just \
typing its ELN, its shortcut, or its name. In this screen you can also add, \
remove or edit your bookmarks by simply typing 'e' to edit the bookmarks file. \
To add or remove a bookmark directly from the command line, you can us the 'a' \
and 'd' arguments as follows: \"bm a PATH\" or \"bm d\" respectively. You can \
also open a bookmark by typing 'bm shortcut' or 'bm name' (in which case TAB \
completion is available).\n"), white, NC, default_color);

	/* ### OPEN ### */
	printf(_("\n%so, open%s%s [ELN/FILE] [APPLICATION]: Open \
either a directory or a file. For example: 'o 12' or 'o filename'. By default, \
the 'open' function will open files with the default application associated to \
them (via the built-in resource opener (mime)). However, if you want to open \
a file with a different application, just add the application name as second \
argument, e.g. 'o 12 leafpad'. If you want to run the program in the \
background, simply add the ampersand character (&): 'o 12 &'. When it comes \
to directories, 'open' works just like the 'cd' command. Remote file systems \
are also supported: just enter 'o ADDRESS [OPTIONS]'. For more information \
about this syntax see the description of 'net' command below.\n"), white, NC,
		   default_color);

	/* ### CD ### */
	printf(_("\n%scd%s%s [ELN/DIR]: When used with no argument, it changes the \
current directory to the default directory (HOME). Otherwise, 'cd' changes \
the current directory to the one specified by the first argument. You can use \
either ELN's or a string to indicate the directory you want. Ex: 'cd 12' or \
'cd ~/media'. Unlike the shell 'cd' command, %s's built-in 'cd' function does \
not only changes the current directory, but also lists its content \
(provided the option \"cd lists automatically\" is enabled, which is the \
default) according to a comprehensive list of color codes. By default, the \
output of 'cd' is much like this shell command: 'cd DIR && ls -A --color=auto \
--group-directories-first'. Remote file systems are also supported. Just \
enter 'cd ADDRESS [OPTIONS]'. For more information about this syntax see the \
description of the 'net' command below.\n"), white, NC, default_color, 
		   PROGRAM_NAME);

	/* ### SELECTION ### */
	printf(_("\n%ss, sel%s%s [ELN ELN-ELN FILE ... n]: Send one or \
multiple elements to the Selection Box. 'Sel' accepts individual elements, \
range of elements, say 1-6, filenames and paths, just as wildcards. Ex: sel \
1 4-10 file* filename /path/to/filename\n"), white, NC, default_color);

	/* ### SELBOX ### */
	printf(_("\n%ssb, selbox%s%s: Show the elements contained in the \
Selection Box.\n"), white, NC, default_color);

	/* ### DESELECT ### */
	printf(_("\n%sds, desel%s%s [*, a, all]: Deselect one or more selected \
elements. You can also deselect all selected elements by typing 'ds *', \
'ds a' or 'ds all'.\n"), white, 
		   NC, default_color);

	/* ### TRASH ### */
	printf(_("\n%st, tr, trash%s%s  [ELN, FILE ... n] [ls, list] [clear] \
[del, rm]: With no argument (or by passing the 'ls' option), it prints a list \
of currently trashed files. The 'clear' option removes all files from the \
trash can, while the 'del' option lists trashed files allowing you to remove \
one or more of them. The trash directory is $XDG_DATA_HOME/Trash, that is, \
'~/.local/share/Trash'. Since this trash system follows the Freedesktop \
specification, it is able to handle files trashed by different Trash \
implementations.\n"), white, NC, default_color);

	/* ### UNTRASH ### */
	printf(_("\n%su, undel, untrash%s%s [*, a, all]: Print a list of currently \
trashed files allowing you to choose one or more of these files to be \
undeleted, that is to say, restored to their original location. You can also \
undelete all trashed files at once using the 'all' argument.\n"), 
		   white, NC, default_color);

	/* ### BACK ### */
	printf(_("\n%sb, back%s%s [h, hist] [clear] [!ELN]: Unlike 'cd ..', which \
will send you to the parent directory of the current directory, this command \
(with no argument) will send you back to the previously visited directory. %s \
keeps a record of all the visited directories. You can see this list by typing \
'b hist', 'b h' or 'bh', and you can access any element in this list by simply \
passing the corresponding ELN in this list (preceded by the exclamation mark) \
to the 'back' command. Example:\n\
	[user@hostname] ~ $ bh\n\
	1 /home/user\n\
	2 /etc\n\
	3 /proc\n\
	[user@hostname] ~ $ b !3\n\
	[user@hostname] /proc $ \n\
  Note: The line printed in green indicates the current position of the back \
function in the directory history list.\nFinally, you can also clear this \
history list by typing 'b clear'.\n"), white, NC, default_color, PROGRAM_NAME);

	/* ### FORTH ### */
	printf(_("\n%sf, forth%s%s [h, hist] [clear] [!ELN]: It works just like \
the back function, but it goes forward in the history record. Of course, you \
can use 'f hist', 'f h', 'fh', and 'f !ELN'\n"), white, NC, default_color);

	/* ### SHORTS ### */
	printf(_("\n%sc, l, m, md, r%s%s: short for 'cp', 'ln', 'mv', 'mkdir', and \
'rm' shell commands respectivelly.\n"), white, NC, default_color);

	/* ### PROPERTIES ### */
	printf(_("\n%sp, pr, prop%s%s [ELN/FILE ... n] [a, all] [s, size]: Print \
file properties for FILE. Use 'all' to list the properties of all files in \
the current working directory, and 'size' to list them size.\n"), 
		   white, NC, default_color);

	/* ### NET ### */
	printf(_("\n%sn, net%s%s ADDRESS [OPTIONS]: Access a remote file system, \
either via the SSH, the FTP, or the SMB protocol. To access a machine via SSH, \
use the following URI scheme: sftp://[USER@]HOST:[/REMOTE-DIR] [PORT]. Bracketed \
elements are optional. If no user is specified, %s will attempt to \
login using the current local username. If no port is specified, the default \
port (22, if SSH) will be used. To access the remote file system via SMB, this \
scheme should be used instead: smb://[USER@]HOST[/SHARE][/REMOTE-DIR] [OPTIONS]. \
The URI scheme for FTP connections is this: ftp://HOST[:PORT] [OPTIONS]. Network \
resources are mounted in '%s/remote_dir', replacing all slashes in remote_dir \
by underscores. To disconnect from the remote machine, use 'fusermount3 -u' for \
SSH, 'fusermount -u' for FTP, and 'umount' for SMB, followd by the corresponding \
mountpoint. Examples: \n\n\t\
net sftp://john@192.168.0.25:Documents/misc port=2222\n\t\
fusermount3 -u %s/remote/john@192.168.0.25:Documents_misc\n\n\t\
net smb://192.168.1.34/share username=laura,password=my_pass\n\t\
umount %s/remote/192.168.0.34_share\n\n\t\
net ftp://192.168.0.123:2121 user=me:mypass\n\t\
fusermount -u %s/remote/192.168.0.123:2121\n"), white, NC, default_color, 
		   PROGRAM_NAME, TMP_DIR, TMP_DIR, TMP_DIR, TMP_DIR);
/* NOTE: fusermount3 is Linux-specific. On FreeBSD 'umount' should be used */

	/* ### MIME ### */
	printf(_("\n%smm, mime%s%s [info ELN/FILE] [edit]: This is %s's \
built-in resource opener. The 'info' option prints the MIME information \
about ELN/FILE: its MIME type, its extension, and the application \
associated to FILENAME. The 'edit' option allows you to edit and customize the \
MIME list file. So, if a file has no default associated application, first get \
its MIME info (you can use its file extension as well) and then add a value for \
it to the MIME list. Each value in the MIME list file has this format: \
'mime_type=application args;application args; ... n' or '*.ext=application args;application args; ... n'. Example: 'plain/text=nano' \
or '*.c=geany -p;leafpad;nano'. %s will use the first matching line in the \
list, and if at least one of the specified applications exists, this one will \
used to open associated files. In case none of the specified applications \
exists, the next matching line will be checked. If the MIME file is not found, \
%s will try to import MIME definitions from the default locations for the \
'mimeapps.list' file as specified by the Freedesktop specification.\n"), 
		   white, NC, default_color, PROGRAM_NAME, PROGRAM_NAME, PROGRAM_NAME);

	/* ### NEW INSTANCE ### */	
	printf(_("\n%sx%s%s DIR: Open DIR in a new instance of %s using the value \
of TerminalCmd (from the configuration file) as terminal emulator. If this \
value is not set, 'xterm' will be used as fallback terminal emulator. This \
function is only available for graphical environments.\n"), 
		   white, NC, default_color, PROGRAM_NAME);

	/* ### EXTERNAL COMMANDS ### */
	printf(_("\n%s;%s%scmd, %s:%s%scmd: Skip all %s expansions and send the \
input string (cmd) as it is to the system shell.\n"), white, NC, default_color, 
		   white, NC, default_color, PROGRAM_NAME);

	/* ### MOUNTPOINTS ### */
	printf(_("\n%smp, mountpoints%s%s: List available mountpoints and change \
the current working directory into the selected mountpoint.\n"), white, NC, 
		   default_color);

	/* ### PASTE ### */
	printf(_("\n%sv, paste%s%s [sel] [DESTINY]: The 'paste sel' command will \
copy the currently selected elements, if any, into the current working \
directory. To copy these elements into another directory, just tell 'paste' \
where to copy these files. Ex: paste sel /path/to/directory\n"), 
		   white, NC, default_color);

	/* ### PROFILE ### */
	printf(_("\n%spf, prof, profile%s%s [set, add, del PROFILE] [edit]: With \
no argument, print the currently used profile. Use the 'set', 'add', and 'del' \
arguments to switch, add or remove a profile\n"), white, NC, default_color);

	/* ### SHELL ### */
	printf(_("\n%sshell%s%s [SHELL]: Print the current default shell for \
%s or set SHELL as the new default shell.\n"), white, NC, default_color, 
		   PROGRAM_NAME);

	/* ### LOG ### */
	printf(_("\n%slog%s%s [clear] [on, off, status]: With no arguments, it \
prints the contents of the log file. If 'clear' is passed as argument, all the \
logs will be deleted. 'on', 'off', and 'status' enable, disable, and check the \
status of the log function for the current session.\n"), white, NC, 
		   default_color);

	/* ### MESSAGES ### */
	printf(_("\n%smsg, messages%s%s [clear]: With no arguments, prints the \
list of messages in the current session. The 'clear' option tells %s to \
empty the messages list.\n"), white, NC, default_color, PROGRAM_NAME);

	/* ### HISTORY ### */
	printf(_("\n%shistory%s%s [clear] [-n]: With no arguments, it shows the \
history list. If 'clear' is passed as argument, it will delete all the entries \
in the history file. Finally, '-n' tells the history command to list only the \
last 'n' commands in the history list.\n"), white, NC, default_color);
	printf(_("You can use the exclamation character (!) to perform some \
history commands:\n\
  !!: Execute the last command.\n\
  !n: Execute the command number 'n' in the history list.\n\
  !-n: Execute the last-n command in the history list.\n"));

	/* ### EDIT ### */
	printf(_("\n%sedit%s%s [EDITOR]: Edit the configuration file. If \
specified, use EDITOR, if available.\n"), white, NC, default_color);

	/* ### ALIAS ### */
	printf(_("\n%salias [import FILE]%s%s: With no arguments, prints a list of \
avaialble aliases, if any. To write a new alias simpy enter 'edit' to open the \
configuration file and add a line like this one: alias name='command args ...' \
To import aliases from a file, provided it contains aliases in the specified \
form, use the 'import' option. Aliases conflicting with some of the internal \
commands won't be imported.\n"), white, NC, default_color);

	/* ### SPLASH ### */
	printf(_("\n%ssplash%s%s: Show the splash screen.\n"), white, NC, default_color);

	/* ### PATH ### */
	printf(_("\n%spath, cwd%s%s: Print the current working directory.\n"), 
		   white, NC, default_color);

	/* ### REFRESH ### */
	printf(_("\n%srf, refresh%s%s: Refresh the screen.\n"), white, NC, 
		   default_color);

	/* ### COLORS ### */
	printf(_("\n%scolors%s%s: Show the color codes used in the elements \
list.\n"), white, NC, default_color);

	/* ### COMMANDS ### */
	printf(_("\n%scmd, commands%s%s: Show this list of commands.\n"), white, 
		   NC, default_color);

	/* ### DIR COUNTER ### */
	printf(_("\n%shdc, dircounter%s%s [on, off, status]: Toggle the dircounter \
on/off.\n"), white, NC, default_color);

	/* ### HIDDEN FILES ### */
	printf(_("\n%shf, hidden%s%s [on, off, status]: Toggle hidden files \
on/off.\n"), white, NC, default_color);

	/* ### FOLDERS FIRST ### */
	printf(_("\n%sff, folders first%s%s [on, off, status]: Toggle list folders \
first on/off.\n"), white, NC, default_color);

	/* ### PAGER ### */
	printf(_("\n%spg, pager%s%s [on, off, status]: Toggle the pager on/off.\n"), 
			white, NC, default_color);

	/* ### UNICODE ### */
	printf(_("\n%suc, unicode%s%s [on, off, status]: Toggle unicode on/off.\n"), 
			white, NC, default_color);

	/* ### EXT ### */
	printf(_("\n%sext%s%s [on, off, status]: Toggle external commands \
on/off.\n"), white, NC, default_color);

	/* ### VERSION ### */
	printf(_("\n%sver, version%s%s: Show %s version details.\n"), white, NC, 
			default_color, PROGRAM_NAME);

	/* ### LICENSE ### */
	printf(_("\n%slicense%s%s: Display the license notice.\n"), white, NC, 
			default_color);

	/* ### FREE SOFTWARE ### */
	printf(_("\n%sfs%s%s: Print an extract from 'What is Free Software?', \
written by Richard Stallman.\n"), white, NC, 
			default_color);

	/* ### QUIT ### */
	printf(_("\n%sq, quit, exit, zz%s%s: Safely quit %s.\n"), white, NC, 
			default_color, PROGRAM_NAME);
}

void
help_function (void)
{
	if (clear_screen)
		CLEAR;

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
\n -U, --unicode \t\t\t enable unicode to correctly list filenames containing \
accents, tildes, umlauts, non-latin letters, etc. This option is enabled by \
default for non-english locales\
\n -v, --version\t\t\t show version details and exit\
\n -x, --ext-cmds\t\t\t allow the use of external commands\n"), PNL, 
		PROGRAM_NAME);

	puts(_("\nBUILT-IN COMMANDS:\n\n\
 /* [DIR]\n\
 bm, bookmarks [a, add PATH] [d, del] [edit] [shortcut, name]\n\
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
 mm, mime [info ELN/FILENAME] [edit]\n\
 ;cmd, :cmd\n\
 mp, mountpoints\n\
 v, paste [sel] [DESTINY]\n\
 pf, prof, profile [set, add, del PROFILE]\n\
 shell [SHELL]\n\
 msg, messages [clear]\n\
 log [clear]\n\
 history [clear] [-n]\n\
 edit [APPLICATION]\n\
 alias\n\
 splash\n\
 path, cwd\n\
 rf, refresh\n\
 colors\n\
 commands\n\
 hf, hidden [on, off, status]\n\
 ff, folders first [on, off, status]\n\
 pg, pager [on, off, status]\n\
 uc, unicode [on, off, status]\n\
 ext [on, off, status]\n\
 ver, version\n\
 license\n\
 fs\n\
 q, quit, exit, zz\n"));

	puts(_("Enter 'commands' to find out more about each of these \
commands.\n"));

	puts(_("KEYBOARD SHORTCUTS:\n\n\
 A-c:	Clear the current command line buffer\n\
 A-f:	Toggle list-folders-first on/off\n\
 C-r:	Refresh the screen\n\
 A-l:	Toggle long view mode on/off\n\
 A-m:	List mountpoints\n\
 A-b:	Launch the Bookmarks Manager\n\
 A-i:	Toggle hidden files on/off\n\
 A-s:	Open the Selection Box\n\
 A-a:	Select all files in the current working directory\n\
 A-d:	Deselect all selected files\n\
 A-r:	Change to the root directory\n\
 A-e:	Change to the home directory\n\
 A-u:	Cahnge up to the parent directory of the current working directory\n\
 A-j:	Change to the previous directory in the directory history \
list\n\
 A-k:	Change to the next directory in the directory history list\n\
 F10:	Open the configuration file\n\n\
NOTE: Depending on the terminal emulator being used, some of these \
keybindings, like A-e, A-f, and F10, might conflict with some of the \
terminal keybindings.\n"));

	puts(_("Color codes: Run the 'colors' command to see the list \
of currently used color codes.\n"));

	puts(_("The configuration and profile files allow you to customize \
colors, define some prompt commands and aliases, and more. For a full \
description consult the man page."));
}

void
free_sotware (void)
{
	puts(_("Excerpt from 'What is Free Software?', by Richard Stallman. \
Source: https://www.gnu.org/philosophy/free-sw.html\n \
\n\"'Free software' means software that respects users' freedom and \
community. Roughly, it means that the users have the freedom to run, copy, \
distribute, study, change and improve the software. Thus, 'free software' is \
a matter of liberty, not price. To understand the concept, you should think \
of 'free' as in 'free speech', not as in 'free beer'. We sometimes call \
it 'libre software', borrowing the French or Spanish word for 'free' \
as in freedom, to show we do not mean the software is gratis.\n\
\nWe campaign for these freedoms because everyone deserves them. With these \
freedoms, the users (both individually and collectively) control the program \
and what it does for them. When users don't control the program, we call it \
a 'nonfree' or proprietary program. The nonfree program controls the users, \
and the developer controls the program; this makes the program an instrument \
of unjust power. \n\
\nA program is free software if the program's users have the four \
essential freedoms:\n\n\
- The freedom to run the program as you wish, for any purpose (freedom 0).\n\
- The freedom to study how the program works, and change it so it does your \
computing as you wish (freedom 1). Access to the source code is a \
precondition for this.\n\
- The freedom to redistribute copies so you can help your neighbor (freedom 2).\n\
- The freedom to distribute copies of your modified versions to others \
(freedom 3). By doing this you can give the whole community a chance to \
benefit from your changes. Access to the source code is a precondition for \
this. \n\
\nA program is free software if it gives users adequately all of these \
freedoms. Otherwise, it is nonfree. While we can distinguish various nonfree \
distribution schemes in terms of how far they fall short of being free, we \
consider them all equally unethical (...)\""));
}

void
version_function(void)
{
	printf(_("%s %s (%s), by %s\nContact: %s\n"), PROGRAM_NAME, VERSION, DATE, 
		   AUTHOR, CONTACT);
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
	printf(_("\n                     The anti-eye-candy/KISS file manager\n%s"), NC);
	printf(_("\n                       Press any key to continue... "));
	xgetchar(); puts("");
}

void
bonus_function (void)
{
	static short state = 0;
	if (state > 13)
		state = 0;
	switch (state) {
	case 0:
		puts("\"Vamos Boca Juniors Carajo!\" (La mitad + 1)");
		break;

	case 1:
		puts("\"Hey! Look behind you! A three-headed monkey!\" "
			 "(G. Threepweed)");
		break;

	case 2:
		puts("\"Free as in free speech, not as in free beer\" (R. M. S)");
		break;

	case 3:
		puts("\"Nothing great has been made in the world without "
			 "passion\" (G. W. F. Hegel)");
		break;

	case 4:
		puts("\"Simplicity is the ultimate sophistication\" "
			 "(Leo Da Vinci)");
		break;

	case 5:
		puts("\"Yo vendí semillas de alambre de púa, al contado, y "
			 "me lo agradecieron\" (Marquitos, 9 Reinas)");
		break;

	case 6:
		puts("\"I'm so happy, because today I've found my friends, "
			 "they're in my head\" (K. D. Cobain)");
		break;

	case 7:
		puts("\"The best code is written with the delete key\" "
			 "(Someone, somewhere, sometime)");
		break;

	case 8:
		puts("\"I'm selling these fine leather jackets\" (Indy)");
		break;

	case 9:
		puts("\"I pray to God to make me free of God\" (Meister Eckhart)");
		break;

	case 10:
		puts("¡Truco y quiero retruco mierda!");
		break;

	case 11:
		puts("The only truth is that there is no truth");
		break;

	case 12:
		puts("\"This is a lie\" (The liar paradox)");
		break;
	case 13:
		puts("\"There are two ways to write error-free programs; only the "
			 "third one works\" (Alan J. Perlis)");
		break;
	default: break;
	}
	state++;
}
