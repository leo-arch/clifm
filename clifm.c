/** CliFM, the anti-eye-candy/KISS file manager */

/* Compile as follows:
 * $ gcc -O3 -march=native -s -fstack-protector-strong -lreadline -lcap -o 
 * clifm clifm.c
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
 * $ gcc -g -Wall -Wextra -Wstack-protector -pedantic -lreadline -lcap -o 
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
* 'base' metapackage. In Debian two packages must be installed: 'libcap-dev'
* and 'libreadline-dev'. In Fedora/redhat: 'libcap-devel' and 'readline-devel'
* Getting dependencies:
* $ ldd /path/to/program
* $ objdump -p /path/to/program | grep NEEDED
* $ readelf -d /path/to/program | grep 'NEEDED'
* $ lsof -p pid_running_program | grep mem
* $ cat /proc/pid_running_program/maps
* # pmap pid_running_program
* Once you get the library, get its full path with 'find', and find which 
* package owns that file with 'pacman -Qo full_path'
* OPTIONAL DEPENDENCIES: 'du' (to check dir sizes), 'xdg-utils' and 'which' 
* (without 'which' 'xdg-open' will fail to get the default application for 
* files)
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
*/

/** 

							###############
							#  TODO LIST  #
							###############
*/
/*
 ** Bookmarks: I don't think "hotkey" is the right word. "Shortcut" looks
	better.
 ** It would be nice to add TAB completion for bookmarks, untrash and unsel
	functions. Use my_rl_path_completion().
 ** Check compatibility with BSD Unices.
 ** Add a help option for each internal command. Make sure this help system is
	consistent accross all commands: if you call help via the --help option, 
	do it this way for all commands. Try to unify the usage and description
	notice so that the same text is used for both the command help and the
	general help. I guess I should move the help for each command to a 
	separate function.
 ** Customizable keybindings would be a great feature!
 ** Pay attention to the construction of error messages: they should be enough
	informative to allow the user/developer to track the origin of the error.
	This line, for example, is vague: 
	  fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
	Use __func__ or some string to help identifying the problem:
	  fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, __func__, strerror(errno));
	If a file or path is involved, add it to the message as well. If possible,
	follow this pattern: program: function: sub-function: file/path: message
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
 ** At the end of prompt() I have 7 strcmp. Try to reduce all this to just one 
    operation.
 **	Add support for wildcards and nested braces to the BRACE EXPANSION function, 
    like in Bash. 
 ** The logic of bookmarks and copy functions is crap! Rewrite it. DONE for 
	copy.
 ** Add the possibility to open files in the background when no application has 
    been passed (ex: o xx &). DONE, but now the pid shown is that of xdg-open, 
    so that when the user tries to kill that pid only xdg-open will be killed. 
    Find a way to kill xdg-open AND its child, OR make it show not xdg-open's 
    pid, but the program's instead. Drawback: xdg-open would remain alive.

###################################

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
	line (3621): group=getgrgid(group_id); It seems to be a library 
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
    line of each "Required by" line. This makes running shell commands in
    CliFM unreliable.

###########################################

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
 * (SOLVED) Type this: '[[ $(whoami) ]] && echo Yes || echo No', and the
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
 * (SOLVED) Type "slaby", press Enter, and bang!, the program crashes. Problem 
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
 * (SOLVED) Search for a file (/x) and then type this: 
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
	not installed.
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


#ifndef __linux__ 
	fprintf(stderr, "%s: This program runs only on GNU/Linux\n", 
			PROGRAM_NAME);
	exit(EXIT_FAILURE);
#endif

/* && !defined(linux) && !defined(__linux) && !defined(__gnu_linux__) 
while 'linux' is deprecated, '__linux__' is recommended */

/* What about unices like BSD, Android and even MacOS? 
* unix || __unix || __unix__
* __FreeBSD___, __NetBSD__, __OpenBSD__, __bdsi__, __DragonFly__
* __APPLE__ && __MACH__ 
* sun || __sun
* __ANDROID__ 
* __CYGWIN__
* MSDOS, _MSDOS, __MSDOS__, __DOS__, _WIN16, _WIN32, _WIN64 */

/* #define _POSIX_C_SOURCE 200809L  */
/* "if you define _GNU_SOURCE, then defining either _POSIX_SOURCE or 
 * _POSIX_C_SOURCE as well has no effect". If I define this macro without 
 * defining _GNU_SOURCE macro as well, the macros 'S_IFDIR' and the others 
 * cannot be used */
#define _GNU_SOURCE /* I need this macro to enable 
program_invocation_short_name variable and asprintf() */

#ifndef EXIT_SUCCESS
	#define EXIT_SUCCESS 0
#endif

#ifndef EXIT_FAILURE
	#define EXIT_FAILURE 1
#endif

#ifndef PATH_MAX
	#define PATH_MAX 4096
#endif

#ifndef HOST_NAME_MAX
	#define HOST_NAME_MAX 64
#endif

#ifndef NAME_MAX
	#define NAME_MAX 255
#endif

/* The following C libraries are located in /usr/include */
#include <stdio.h> /* (f)printf, s(n)printf, scanf, fopen, fclose, remove, 
					fgetc, fputc, perror, rename, sscanf, getline */
#include <string.h> /* str(n)cpy, str(n)cat, str(n)cmp, strlen, strstr, memset */
#include <stdlib.h> /* getenv, malloc, calloc, free, atoi, realpath, 
					EXIT_FAILURE and EXIT_SUCCESS macros */
#include <dirent.h> /* scandir */
#include <unistd.h> /* sleep, readlink, chdir, symlink, access, exec, isatty, 
					* setpgid, getpid, getuid, gethostname, tcsetpgrp, 
					* tcgetattr, char **__environ, STDIN_FILENO, STDOUT_FILENO, 
					* and STDERR_FILENO macros */
#include <sys/stat.h> /* stat, lstat, mkdir */
#include <sys/wait.h> /* waitpid, wait */
#include <sys/ioctl.h> /* ioctl */
#include <time.h> /* localtime, strftime, clock (to time functions) */
#include <grp.h> /* getgrgid */
#include <signal.h> /* trap signals */
#include <ctype.h> /* isspace, isdigit, tolower */
#include <pwd.h> /* getcwd, getpid, geteuid, getpwuid */
#include <linux/limits.h> /* PATH_MAX (4096), NAME_MAX (255) */
#include <readline/history.h> /* for commands history: add_history(buf); */
#include <readline/readline.h> /* readline */
/* Readline: This function allows the user to move back and forth with the 
 * arrow keys in the prompt. I've tried scanf, getchar, getline, fscanf, fgets, 
 * and none of them does the trick. Besides, readline provides TAB completion 
 * and history. Btw, readline is what Bash uses for its prompt */
#include <glob.h> /* glob */
#include <termios.h> /* struct termios */
#include <locale.h> /* setlocale */
#include <errno.h>
#include <sys/capability.h> /* cap_get_file */
#include <getopt.h> /* getopt_long */
#include <fcntl.h> /* O_RDONLY, O_DIRECTORY, and AT_* macros */
#include <sys/syscall.h> /* SYS_* and __NR_* macros for syscall() */
#include <linux/fs.h> /* FS_IOC_GETFLAGS, S_IMMUTABLE_FL macros */
#include <libintl.h> /* gettext */

/* #include <bsd/string.h> // strlcpy, strlcat */
/* #include "clifm.h" */
/* #include <sys/types.h> */

#define PROGRAM_NAME "CliFM"
#define PNL "clifm" /* Program name lowercase */
/* #define CONF_PATHS_MAX 68
 * According to useradd manpage, the max lenght of a username is 32. So, 
 * "/home/" (6) + 32 + "/.config/clifm/bookmarks.cfm" (28) + terminating 
 * null byte (1) == 67. This is then the max length I need for config dirs 
 * and files. Currently, config dirs and files are dinamically allocated */
#define TMP_DIR "/tmp/clifm"
/* If no formatting, puts (or write) is faster than printf */
#define CLEAR puts("\x1b[c")
/* #define CLEAR write(STDOUT_FILENO, "\ec", 3) */
#define VERSION "0.16.3"
#define AUTHOR "L. Abramovich"
#define CONTACT "johndoe.arch@outlook.com"
#define DATE "June 17, 2020"

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
#define XDG_OPEN_OK		(1 << 12)
#define GRAPHICAL		(1 << 13)
#define IS_USRVAR_DEF	(1 << 14) /* 18 dec, 0x12 hex, 00010010 binary */

/* Used by log_msg() to know wether to tell prompt() to print messages or 
 * not */
#define PRINT_PROMPT 1
#define NOPRINT_PROMPT 0

/* Error codes, to be used by launch_exec functions */
#define EXNULLERR 79
/*#define EXEXECERR 80 */
#define EXFORKERR 81
#define EXCRASHERR 82
/*#define EXWAITERR 83 */

/* ###COLORS### */
/* These are just a fixed color stuff in the interface. Remaining colors 
 * are customizable and set via the config file */

/* \x1b: hex value for escape char (alternative: ^[) 
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
#define red_b "\001\x1b[1;31m\002" /* error log indicator */
#define green_b "\001\x1b[1;32m\002" /* sel indicator */
#define yellow_b "\001\x1b[0;33m\002" /* trash indicator */
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
#define atoi xatoi
/*#define alphasort xalphasort */
#define _(String) gettext (String)
#define statx(a,b,c,d,e) syscall(__NR_statx,(a),(b),(c),(d),(e))


/* ###CUSTOM FUNCTIONS### 
They all work independently of this program and its variables */

int
check_immutable_bit(char *file)
/* Check a file's immutable bit. Returns 1 if true, zero if false, and
 * -1 in case of error */
{
	int attr, fd, immut_flag=-1;
	fd=open(file, O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "'%s': %s\n", file, strerror(errno));
		return -1;
	}
	ioctl(fd, FS_IOC_GETFLAGS, &attr);
	if (attr & FS_IMMUTABLE_FL)
		immut_flag=1;
	else
		immut_flag=0;
	close(fd);
	if (immut_flag) 
		return 1;
	else 
		return 0;
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
	newt=oldt;
	newt.c_lflag &= ~(ICANON|ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	ch=getchar();
	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	return ch;
}

int
xstrcmp(const char *str1, const char *str2)
{
/* Check for null. This check is done neither by strcmp nor by strncmp. I 
* use 256 for error code since it does not represent any ASCII code (the 
* extended version goes up to 255) */
	if (!str1 || !str2) return 256;
	while(*str1) {
		if (*str1 != *str2)
			return *str1-*str2;
		str1++;
		str2++;
	}
	if (*str2) return 0-*str2;
	return 0;
}

int
xstrncmp(const char *str1, const char *str2, size_t n)
{
	if (!str1 || !str2)
		return 256;
	size_t counter=0;
	while(*str1 && counter < n) {
		if (*str1 != *str2)
			return *str1-*str2;
		str1++;
		str2++;
		counter++;
	}
	if (counter == n)
		return 0;
	if (*str2)
		return 0-*str2;
	return 0;
}

char *
xstrcpy(char *buf, const char *str)
{
	if (!str) return NULL;
	while(*str) {
		*buf=*str;
		buf++;
		str++;
	}
	*buf=0x00;
	return buf;
}

char *
xstrncpy(char *buf, const char *str, size_t n)
{
	if (!str) return NULL;
	size_t counter=0;
	while(*str && counter < n) {
		*buf=*str;
		buf++;
		str++;
		counter++;
	}
	*buf=0x00;
	return buf;
}

size_t
xstrlen(const char *str)
{
	size_t len=0;
	while (*(str++)) /* Same as: str[len] != 0x00 */
		len++;
	return len;
}

int
xatoi(const char *str)
/* 2 times faster than atoi. The commented lines make xatoi able to handle 
 * negative values */
{
	int ret=0; /*, neg=0; */
/*	if (*str == '-') { // same as: str[0] == '-'
		str++;
		neg=1;
	}*/
	size_t len=strlen(str);
	for(size_t i=0; i<len; i++) {
		if (str[i] < 0x30 || str[i] > 0x39)
			return ret;
		ret=ret*10+(str[i]-'0');
	}
/*	if (neg) ret=ret-(ret*2); */
	return ret;
}

pid_t
get_own_pid(void)
{
	pid_t pid=0;
	/* Get the process id */
	if ((pid=getpid()) < 0)
		return 0;
	else
		return pid;
}

char *
get_user(void)
{
	register struct passwd *pw;
	register uid_t uid=0;
	uid=geteuid();
	pw=getpwuid(uid);
	if (!pw)
		return NULL;
	size_t user_len=strlen(pw->pw_name);
	char *p=calloc(user_len+1, sizeof(char));
	if (!p)
		return NULL;
	char *buf=p;
	p=NULL;
	strncpy(buf, pw->pw_name, user_len);
	return buf;
}

char *
get_user_home (void)
{
	struct passwd *user_info;
	user_info=getpwuid(getuid());
/*
	printf("Username: %s\n", user_info->pw_name);
	printf("Password: %s\n", user_info->pw_passwd);
	printf("UID: %d\n", user_info->pw_uid);
	printf("GID: %d\n", user_info->pw_gid);
	printf("Gecos: %s\n", user_info->pw_gecos);
	printf("Home: %s\n", user_info->pw_dir);
	printf("Shell: %s\n", user_info->pw_shell);
*/
	if (!user_info)
		return NULL;
	size_t home_len=strlen(user_info->pw_dir);
	char *p=calloc(home_len+1, sizeof(char));
	if (!p)
		return NULL;
	char *home=p;
	p=NULL;
	strncpy(home, user_info->pw_dir, home_len);
	return home;
}

int
is_number(const char *str)
/* Check whether a given string contains only digits. Returns 1 if true and 0 
 * if false. Does not work with negative numbers */
{
/* VERSION 3: NO isdigit, NO strlen, and NO extra variables at all! */
	for (;*str;str++)
		if (*str < 0x30 || *str > 0x39)
			return 0;
	return 1;
}

int
digits_in_num (int num) {
/* Return the amount of digits in a given number */
	int count=0; /* VERSION 2: neither printf nor any function call at all */
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
					(*a)->d_name, ((*b)->d_name[0] == '.') ? (*b)->d_name+1 : 
					(*b)->d_name);
}

int
strcntchr(const char *str, const char c)
/* Returns the index of the first appearance of c in str, if any, and -1 if c 
 * was not found. */
{
/*	size_t str_len=strlen(str);
	for (size_t i=str_len;i--;) { */
	int counter=0;
	while(*str) { /* VERSION 2: NO strlen */
		if (*str == c)
			return counter;
		counter++;
		str++;
	}
	return -1;
}

char *
straft(const char *str, const char c)
/* Returns the string after the first appearance of a given char, or returns 
 * NULL if C is not found in STR or C is the last char in STR. */
{
	char *buf=NULL;
	int counter=0;
	size_t str_len=strlen(str);
	while(*str) {
		if ((str_len-1) == counter)
			/* There's no string after last char */
			return NULL;
		if (*str == c) {
			/* If C is found, copy everything after C into buf */
			char *p=calloc((str_len-counter)+1, sizeof(char));
			if (p) {
				buf=p;
				p=NULL;
				strcpy(buf, str+1);
				return buf;
			}
			else return NULL;
		}
		str++;
		counter++;
	}
	return NULL; /* No matches */
}

char *
straftlst(const char *str, const char c)
/* Returns the string after the last appearance of a given char, NULL if no 
 * matches */
{
	char *f_str=NULL;
	size_t str_len=strlen(str);
	for (size_t i=str_len;i--;) {
		if (str[i] == c) {
			if (i == str_len-1) return NULL; /* There's nothing after C */
			char *p=calloc(str_len-i, sizeof(char));
			if (p) {
				f_str=p;
				p=NULL;
				/* copy STR beginning one char after C */
				strcpy(f_str, str+i+1);
				return f_str;
			}
			else return NULL;
		}
	}
	return NULL;
}

char *
strbfr(char *str, const char c)
/* Returns the substring in str before the first appearance of c. If not found, 
 * or C is the first char in STR, returns NULL */
{
	char *start=str, *f_str=NULL;
	int counter=0;
	while(*str) {
		if (*str == c) {
			if (counter == 0) return NULL; /* There's no substring before the 
			first char */
			char *p=calloc(counter+1, sizeof(char));
			if (p) {
				f_str=p;
				p=NULL;
				strncpy(f_str, start, counter);
				return f_str;
				/*NOTE: f_str is null terminated because calloc allocated and 
				 * zeroed count+1 bytes, whereas strcpy copied the first 
				 * 'counter' bytes of start into f_str. */
			}
			else return NULL;
		}
		counter++;
		str++;
	}
	return NULL;
}

char *
strbfrlst(char *str, char c)
/* Get substring in str before the last appearance of c. Returns substring if
 c is found and NULL if not (or if c was the first char in str). */
{
	/* Get the index in str of the last appearance of c */
	char *buf=str;
	unsigned int index=0, counter=0;
	while(*buf) {
		if (*buf == c)
			index=counter;
		counter++;
		buf++;
	}
	/* If C was not found (or it was the first char in STR (index=0), in 
	 * which case there is nothing before it) */
	if (!index)
		return NULL; 
	/* Else, copy str into buf, replace C by null char, and return buf */
	size_t str_len=strlen(str);
	buf=NULL;
	char *p=calloc(str_len+1, sizeof(char));
	if (!p)
		return NULL;
	buf=p;
	p=NULL;
	strcpy(buf, str);
/*	if (index != str_len-1) // Why? This check shouldn't be here */
		buf[index]=0x00;
	return buf;
}

char *
strbtw(const char *str, const char a, const char b)
/* Returns the substring in 'str' between chars 'a' and 'b'. If not found, 
 * returns NULL. Receives a string and the two chars to get the string between 
 * them */
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
	char *p=calloc((ch_to-ch_from)+1, sizeof(char)), *f_str=NULL;
	if (p) {
		f_str=p;
		p=NULL;
	}
	else {
		fprintf(stderr, "%s: %s: Memory allocation failure\n", PROGRAM_NAME, 
				__func__);
		return NULL;
	}	
	for(int i=ch_from+1;i<ch_to;i++)
		f_str[j++]=str[i];
	f_str[j]=0x00;
	return f_str;
}

/* The following four functions (from_hex, to_hex, url_encode, and
 * url_decode) were taken from "http://www.geekhideout.com/urlcode.shtml"
 * and modified to comform to RFC 2395, as recommended by the freedesktop
 * trash specification */
char
from_hex(char c)
/* Converts a hex char to its integer value */
{
	return isdigit(c) ? c-'0' : tolower(c)-'a'+10;
}

char
to_hex(char c)
/* Converts an integer value to its hex form*/
{
	static char hex[]="0123456789ABCDEF";
	return hex[c & 15];
}

char *
url_encode(char *str)
/* Returns a url-encoded version of str */
{
	if (!str || str[0] == 0x00)
		return NULL;
	char *p=calloc((strlen(str)*3)+1, sizeof(char));
	/* The max lenght of our buffer is 3 times the length of srt plus 1 extra 
	 * byte for the null byte terminator: each char in str will be, if encoded, 
	 * %XX (3 chars) */
	if (!p)
		return NULL;
	char *buf=p;
	p=NULL;

	char *pstr=str, *pbuf=buf; /* Copies of str and buf pointers to be able
	* to increase and/or decrease them without loosing the original memory 
	* location */
	for (;*pstr;pstr++) {
		if (isalnum(*pstr) || *pstr == '-' || *pstr == '_' 
					|| *pstr == '.' || *pstr == '~' || *pstr == '/')
			/* Do not encode the above chars */
			*pbuf++=*pstr;
		else {
			/* Encode char to URL format. Example: space char to %20 */
			*pbuf++='%';
			*pbuf++=to_hex(*pstr >> 4); /* Right shift operation */
			*pbuf++=to_hex(*pstr & 15); /* Bitwise AND operation */
		}
	}
	return buf;
}

char *
url_decode(char *str)
/* Returns a url-decoded version of str */
{
	if (!str || str[0] == 0x00)
		return NULL;
	char *p=calloc(strlen(str)+1, sizeof(char));
	/* The decoded string will be at most as long as the encoded string */
	if (!p)
		return NULL;
	char *buf=p;
	p=NULL;
	
	char *pstr=str, *pbuf=buf;
	for (;*pstr;pstr++) {
		if (*pstr == '%') {
			if (pstr[1] && pstr[2]) {
				/* Decode URL code. Example: %20 to space char */  
				/* Left shift and bitwise OR operations */  
				*pbuf++=from_hex(pstr[1]) << 4 | from_hex(pstr[2]);
				pstr+=2;
			}
		}
		else
			*pbuf++=*pstr;
	}
	return buf;
}

char *
get_date(void)
{
	time_t rawtime=time(NULL);
	struct tm *tm=localtime(&rawtime);
	size_t date_max=128;
	char *p=calloc(date_max+1, sizeof(char)), *date;
	if (p) {
		date=p;
		p=NULL;
	}
	else
		return NULL;
	strftime(date, date_max, "%a, %b %d, %Y, %T", tm);
	return date;
}

char *
get_size_unit(off_t file_size)
/* Convert FILE_SIZE to human readeable form */
{
	size_t max_size_type_len=11;
	/* Max size type length == 10 == "1024.1KiB\0",
	 * or 11 == "1023 bytes\0" */
	char *size_type=calloc(max_size_type_len+1, sizeof(char));
	short units_n=0;
	float size=file_size;
	while (size > 1024) {
		size=size/1024;
		++units_n;
	}
	/* If bytes */
	if (!units_n)
		snprintf(size_type, max_size_type_len, "%.0f bytes", size);
	else {
		static char units[]={ 'b', 'K', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y' };		
		snprintf(size_type, max_size_type_len, "%.1f%ciB", size, 
				 units[units_n]);
	}
	return size_type;
}

/* ###FUNCTIONS PROTOTYPES### */

void signal_handler(int sig_num);
void *xcalloc(void *ptr, size_t nmemb, size_t size);
void *xrealloc(void *ptr, size_t size);
void free_stuff(void);
void set_signals_to_ignore(void);
void set_signals_to_default(void);
void init_shell(void);
void xdg_open_check(void);
void splash(void);
char **my_rl_completion(const char *text, int start, int end);
char *my_rl_quote(char *text, int m_t, char *qp);
char *dequote_str(char *text, int m_t);
int quote_detector(char *line, int index);
int is_quote_char(char c);
char *filenames_generator(const char *text, int state);
char *bin_cmd_generator(const char *text, int state);
void get_path_programs(void);
int get_path_env(void);
int get_sel_files(void);
void init_config(void);
void exec_profile(void);
void external_arguments(int argc, char **argv);
void get_aliases_n_prompt_cmds(void);
void check_log_file_size(char *log_file);
char **check_for_alias(char **comm);
int get_history(void);
void add_to_dirhist(const char *dir_path);
char *home_tilde(const char *new_path);
char **parse_input_str(char *str);
int list_dir(void);
char *prompt(void);
int exec_cmd(char **comm);
int launch_execve(char **cmd);
int launch_execle(const char *cmd);
char *get_cmd_path(const char *cmd);
int brace_expansion(const char *str);
int run_in_foreground(pid_t pid);
void run_in_background(pid_t pid);
int copy_function(char **comm);
int run_and_refresh(char **comm);
int properties_function(char **comm);
int get_properties(char *filename, int _long, int max);
int save_sel(void);
int sel_function(char **comm);
void show_sel_files(void);
int deselect(char **comm);
int search_function(char **comm);
int bookmarks_function(char **cmd);
char **get_bookmarks(char *bookmarks_file);
char **bm_prompt(void);
int count_dir(const char *dir_path);
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
void readline_kbinds();
int readline_kbind_action (int count, int key);
void surf_hist(char **comm);
int trash_function(char **comm);
int trash_element(const char *suffix, struct tm *tm, char *file);
int wx_parent_check(char *file);
void remove_from_trash (void);
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
void set_colors(void);
int is_color_code(char *str);
char **get_substr(char *str, const char ifs);
char *get_sys_shell(void);
int set_shell(const char *str);

void exec_chained_cmds(char *cmd);
int is_internal_c(const char *cmd);

int del_bookmark(void);
int add_bookmark(char *file);
char *savestring (const char *str, size_t size);
char *my_rl_path_completion(const char *text, int state);
int is_link_to_dir(const char *link);

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

/* ################## GLOBAL VARIABLES ##################### */

/* These variables, unlike local or automatic variables, are accessible to any 
 * part of the program and hold their values throughout the entire lifetime of 
 * the program. If not initialized, they're initialized automatically by the 
 * compiler in the following way:
 int 		0
 char 		'\0'
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

/* Uncomment this line for TCC compilation: without this variable, TCC 
 * complains that __dso_handle is an undefined symbol and won't compile */
/* void* __dso_handle; */

/* Struct to store user defined variables */
struct usrvar_t {
	char *name;
	char *value;
};
/* Always initialize variables, to NULL if string, to zero if int; otherwise 
 * they may contain garbage, and an access to them may result in a crash or 
 * some invalid data being read. However, non-initialized variables are 
 * automatically initialized by the compiler */

char splash_screen=-1, welcome_message=-1, ext_cmd_ok=-1, show_hidden=-1, 
	clear_screen=-1, shell_terminal=0, pager=-1, no_log=0, internal_cmd=0, 
	shell_is_interactive=0, list_folders_first=-1, case_sensitive=-1, 
	cd_lists_on_the_fly=-1, recur_perm_error_flag=0, is_sel=0, sel_is_last=0, 
	print_msg=0, long_view=-1, kbind_busy=0, error_msg=0, 
	warning_msg=0, notice_msg=0, unicode=-1, cont_bt=0, dequoted=0,
	home_ok=1, config_ok=1, trash_ok=1, selfile_ok=1;
	/* -1 means non-initialized or unset. Once initialized, these variables
	 * are either zero or one */
	//sel_no_sel=0
/* A short int accepts values from -32,768 to 32,767, and since all the 
 * above variables will take -1, 0, and 1 as values, a short int is more 
 * than enough. Now, since short takes 2 bytes of memory, using int for these 
 * variables would be a waste of 2 bytes (since an int takes 4 bytes). I can 
 * even use char (or signed char), which takes only 1 byte (8 bits) and 
 * accepts negative numbers (-128 -> +127).
* NOTE: If the value passed to a variable is bigger than what the variable can 
* hold, the result of a comparison with this variable will always be true */

int files=0, args_n=0, sel_n=0, max_hist=-1, max_log=-1, path_n=0, 
	current_hist_n=0, dirhist_total_index=0, dirhist_cur_index=0, 
	argc_bk=0, usrvar_n=0, aliases_n=0, prompt_cmds_n=0, trash_n=0, 
	msgs_n=0, longest=0, term_cols=0, bm_n=0;

size_t user_home_len=0;
struct termios shell_tmodes;
pid_t own_pid=0;
char **dirlist=NULL;
struct usrvar_t *usr_var=NULL;
char *user=NULL, *path=NULL, **old_pwd=NULL, **sel_elements=NULL, *qc=NULL,
	*sel_file_user=NULL, **paths=NULL, **bin_commands=NULL, **history=NULL, 
	*xdg_open_path=NULL, **braces=NULL, *alt_profile=NULL,
	**prompt_cmds=NULL, **aliases=NULL, **argv_bk=NULL, *user_home=NULL, 
	**messages=NULL, *msg=NULL, *CONFIG_DIR=NULL, *CONFIG_FILE=NULL, 
	*BM_FILE=NULL, hostname[HOST_NAME_MAX]="", *LOG_FILE=NULL, 
	*LOG_FILE_TMP=NULL, *HIST_FILE=NULL, *MSG_LOG_FILE=NULL, 
	*PROFILE_FILE=NULL, *TRASH_DIR=NULL, *TRASH_FILES_DIR=NULL, 
	*TRASH_INFO_DIR=NULL, *sys_shell=NULL,
	/* This is not a comprehensive list of commands. It only lists commands
	 * long version for TAB completion */
	*INTERNAL_CMDS[]={ "alias", "open", "prop", "back", "forth",
		"move", "paste", "sel", "selbox", "desel", "refresh", 
		"edit", "history", "hidden", "path", "help", "commands", 
		"colors", "version", "license", "splash", "folders first", 
		"exit", "quit", "pager", "trash", "undel", "messages", 
		"mountpoints", "bookmarks", "log", "untrash", "unicode", 
		"profile", "shell", NULL };

#define MAX_COLOR 45 
/* 45 == \x1b[00;00;00;000;000;000;00;00;000;000;000m (24bit, RGB true color 
 * format including foreground and background colors, the SGR (Select Graphic
 * Rendition) parameter, and, of course, the terminating null char.

 * To store all the 29 color variables I use, with 43 bytes each, I need a 
 * total of 1,2Kb. It's not much but it could be less if I'd use dynamically 
 * allocated arrays for them */

/* Some interface colors */
char text_color[MAX_COLOR+8]="", eln_color[MAX_COLOR]="", 
	 prompt_color[MAX_COLOR+8]="", default_color[MAX_COLOR]="",
	 dir_count_color[MAX_COLOR]="", div_line_color[MAX_COLOR]="",
	 welcome_msg_color[MAX_COLOR]="";
/* text_color and prompt_color are used in the command line, and readline
 * needs to know that color codes are not printable chars. For this I need
 * to add "\001" at the beginning of the color code and "\002" at the end.
 * Both taken together sum up 8 bytes */

/* Filetypes colors */
char di_c[MAX_COLOR]="", /* Directory */
	nd_c[MAX_COLOR]="", /* No read directory */
	ed_c[MAX_COLOR]="", /* Empty dir */
	ne_c[MAX_COLOR]="", /* No read empty dir */
	fi_c[MAX_COLOR]="", /* Reg file */
	ef_c[MAX_COLOR]="", /* Empty reg file */
	nf_c[MAX_COLOR]="", /* No read file */
	ln_c[MAX_COLOR]="",	/* Symlink */
	or_c[MAX_COLOR]="", /* Broken symlink */
	pi_c[MAX_COLOR]="", /* FIFO, pipe */
	so_c[MAX_COLOR]="", /* Socket */
	bd_c[MAX_COLOR]="", /* Block device */
	cd_c[MAX_COLOR]="",	/* Char device */
	su_c[MAX_COLOR]="", /* SUID file */
	sg_c[MAX_COLOR]="", /* SGID file */
	tw_c[MAX_COLOR]="", /* Sticky other writable */
	st_c[MAX_COLOR]="", /* Sticky (not ow)*/
	ow_c[MAX_COLOR]="", /* Other writable */
	ex_c[MAX_COLOR]="", /* Executable */
	ee_c[MAX_COLOR]="", /* Empty executable */
	ca_c[MAX_COLOR]="", /* Cap file */
	no_c[MAX_COLOR]=""; /* Unknown */

/** 
 * #############################
 * #           MAIN            # 
 * #############################
 * */

int
main(int argc, char **argv)
{
	/* ##### BASIC CONFIG AND VARIABLES ###### */

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
		unicode=1;

	/* Initialize gettext() for translations */
	bindtextdomain("clifm", "/usr/share/locale");
	textdomain("clifm");

	/* Save external arguments to be able to rerun external_arguments() in 
	 * case the user edits the config file, in which case the program must 
	 * rerun init_config(), get_aliases_n_prompt_cmds(), and then 
	 * external_arguments() */
	argc_bk=argc;
	argv_bk=xcalloc(argv_bk, (size_t)argc, sizeof(char *));
	for (size_t i=0;i<(size_t)argc;i++) {
		argv_bk[i]=xcalloc(argv_bk[i], strlen(argv[i])+1, sizeof(char));
		strcpy(argv_bk[i], argv[i]);
	}

	/* Register the function to be called at normal exit, either via exit()
	 * or main's return. The registered function will not be executed when
	 * abnormally exiting the program, eg., via KILL signal */
	atexit(free_stuff);
	
	/* Get user's home directory */
	user_home=get_user_home();
	if (!user_home || access(user_home, W_OK) == -1) {
		/* If no user's home, or if it's not writable, there won't be any 
		 * config nor trash directory either. These flags are used to prevent 
		 * functions from trying to access any of these directories */
		home_ok = config_ok = trash_ok = 0;
		/* Print message: trash, bookmarks, command logs. Commands history and program
		 * messages won't be stored */
		asprintf(&msg, _("%s: Cannot access the home directory. Trash, "
						 "bookmarks, commands logs, and commands history are "
						 "disabled. Program messages and selected files "
						 "won't be persistent. Using default options\n"), 
				 PROGRAM_NAME);
		if (msg) {
			error_msg=1;
			log_msg(msg, PRINT_PROMPT);
			free(msg);
		}
		else
			fprintf(stderr, _("%s: Cannot access the home directory. Trash, "
							  "bookmarks, commands logs, and commands "
							  "history are disabled. Program messages and "
							  "selected files won't be persistent. Using "
							  "default options\n"), 
					PROGRAM_NAME);
	}
	else
		user_home_len=strlen(user_home);

	/* Get user name */
	user=get_user();
	if (!user) {
		user=xcalloc(user, 4, sizeof(char));
		strcpy(user, "???");
		asprintf(&msg, _("%s: Error getting username\n"), PROGRAM_NAME);
		if (msg) {
			error_msg=1;
			log_msg(msg, PRINT_PROMPT);
			free(msg);
		}
		else
			fprintf(stderr, _("%s: Error getting username\n"), PROGRAM_NAME);		
	}
	if (strcmp(user, "root") == 0)
		flags |= ROOT_USR;

	/* Running in X? */
	if (getenv("DISPLAY") != NULL && strncmp(getenv("TERM"), "linux", 5) != 0)
		flags |= GRAPHICAL;

	/* Get paths from PATH environment variable. These paths will be used 
	 * later by get_path_programs (for the autocomplete function) and 
	 * get_cmd_path() */
	path_n=get_path_env();

	/* Manage external arguments, but only if any: argc == 1 equates to no 
	 * argument, since this '1' is just the program invokation name. External 
	 * arguments will override initialization values (init_config) */
	if (argc > 1)
		external_arguments(argc, argv);

	/* Initialize program paths and files, set options from the config file, 
	 * if they were not already set via external arguments, and load sel 
	 * elements, if any. All these configurations are made per 
	 * user basis */
	init_config();

	if (!config_ok)
		set_default_options();

	/* Check whether we have a working shell */
	if (access(sys_shell, X_OK) == -1) {
		asprintf(&msg, _("%s: %s: System shell not found. Please edit the "
						 "configuration file to specify a working shell.\n"),
						 PROGRAM_NAME, sys_shell);
		if (msg) {
			warning_msg=1;
			log_msg(msg, PRINT_PROMPT);
			free(msg);
		}
		else
		fprintf(stderr, _("%s: %s: System shell not found. Please edit the "
						 "configuration file to specify a working shell.\n"),
						 PROGRAM_NAME, sys_shell);
	}

	get_aliases_n_prompt_cmds();

	get_sel_files();

	if (trash_ok) {
		trash_n=count_dir(TRASH_FILES_DIR);
		if (trash_n <= 2) trash_n=0;
	}

	/* Get hostname */
	if (gethostname(hostname, sizeof(hostname)) == -1) {
		strcpy(hostname, "???");
		asprintf(&msg, _("%s: Error getting hostname\n"), PROGRAM_NAME);
		if (msg) {
			error_msg=1;
			log_msg(msg, PRINT_PROMPT);
			free(msg);
		}
		else
			fprintf(stderr, _("%s: Error getting hostname\n"), PROGRAM_NAME);
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
		if (stat(HIST_FILE, &file_attrib) == 0 && file_attrib.st_size != 0) {
		/* If the size condition is not included, and in case of a zero size 
		 * file, read_history() gives malloc errors */
			/* Recover history from the history file */
			read_history(HIST_FILE); /* This line adds more leaks to 
			readline */
			/* Limit the size of the history file to max_hist lines */
			history_truncate_file(HIST_FILE, max_hist);
		}
		else { /* If the history file doesn't exist, create it */
			FILE *hist_fp=fopen(HIST_FILE, "w+");
			if (!hist_fp) {
				asprintf(&msg, "%s: fopen: '%s': %s\n", PROGRAM_NAME, 
						 HIST_FILE, strerror(errno));
				if (msg) {
					error_msg=1;
					log_msg(msg, PRINT_PROMPT);
					free(msg);
				}
				else
					fprintf(stderr, "%s: fopen: '%s': %s\n", PROGRAM_NAME, 
							HIST_FILE, strerror(errno));
			}
			else {
				/* To avoid malloc errors in read_history(), do not create 
				 * an empty file */
				fprintf(hist_fp, "edit\n");
				/* There is no need to run read_history() here, since the 
				 * history file is still empty */
				fclose(hist_fp);
			}
		}
	}
	
	/* Store history into an array to be able to manipulate it */
	get_history();

	/* Check if xdg-open is available */
	xdg_open_check();

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
	rl_completion_entry_function=my_rl_path_completion;

	/* Pointer to alternative function to create matches.
	 * Function is called with TEXT, START, and END.
	 * START and END are indices in RL_LINE_BUFFER saying what the boundaries
	 * of TEXT are.
	 * If this function exists and returns NULL then call the value of
	 * rl_completion_entry_function to try to match, otherwise use the
	 * array of strings returned. */
	rl_attempted_completion_function=my_rl_completion;
	rl_ignore_completion_duplicates=1;
	/* I'm using here a custom quoting function. If not specified, readline
	 * uses the default internal function. */
	rl_filename_quoting_function=my_rl_quote;
	/* Tell readline what char to use for quoting. This is only the the
	 * readline internal quoting function, and for custom ones, like the one 
	 * I use above. However, custom quoting functions, though they need to
	 * define their own quoting chars, won't be called at all if this
	 * variable isn't set. */
	rl_completer_quote_characters="\"'";
	rl_completer_word_break_characters=" ";
	/* Whenever readline finds any of these chars, it will call the
	 * quoting function */
	rl_filename_quote_characters=" \t\n\"\\'`@$><=,;|&{[()]}?!*^";
	/* According to readline documentation, the following string is
	 * the default and the one used by Bash: " \t\n\"\\'`@$><=;|&{(" */
	
	/* Copy this list of quote chars to a global variable to be used
	 * later by some of the program functions like split_str(), my_rl_quote(), 
	 * is_quote_char(), and my_rl_dequote() */
	qc=xcalloc(qc, strlen(rl_filename_quote_characters)+1, sizeof(char));
	strcpy(qc, rl_filename_quote_characters);
	/* Executed immediately before calling the completer function, it tells
	 * readline if a space char, which is a word break character (see the 
	 * above rl_completer_word_break_characters variable) is quoted or not. 
	 * If it is, readline then passes the whole string to the completer 
	 * function (ex: "user\ file"), and if not, only wathever it found after 
	 * the space char (ex: "file") 
	 * Thanks to George Brocklehurst for pointing out this function:
	 * https://thoughtbot.com/blog/tab-completion-in-gnu-readline*/
	rl_char_is_quoted_p=quote_detector;
	/* This function is executed inmediately before path completion. So,
	 * if the string to be completed is, for instance, "user\ file" (see the 
	 * above comment), this function should return the dequoted string so
	 * it won't conflict with system filenames: you want "user file",
	 * because "user\ file" does not exist, and, in this latter case,
	 * readline won't find any matches */
	rl_filename_dequoting_function=dequote_str;
	/* Initialize the keyboard bindings function */
	readline_kbinds();

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
		char cwd[PATH_MAX]="";
		getcwd(cwd, sizeof(cwd));
		/* getenv() returns an address, so that pwd is this address and
		 * *pwd is its value */
		if (!*cwd || strlen(cwd) == 0) {
			if (user_home) {
				path=xcalloc(path, strlen(user_home)+1, sizeof(char));
				strcpy(path, user_home);
			}
			else {
				if (access("/", R_OK|X_OK) == -1) {
					fprintf(stderr, "%s: '/': %s\n", PROGRAM_NAME, 
							strerror(errno));
					exit(EXIT_FAILURE);
				}
				else {
					path=xcalloc(path, 2, sizeof(char));
					strcpy(path, "/");
				}
			}
		}
		else {
			path=xcalloc(path, strlen(cwd)+1, sizeof(char));
			strcpy(path, cwd);		
		}
	}

	/* Make path the CWD */
	/* If chdir() fails, set path to cwd, list files and print the error 
	 * message. If not access to cwd dir either, exit */
	if (chdir(path) == -1) {
		asprintf(&msg, "%s: chdir: '%s': %s\n", PROGRAM_NAME, path, 
				 strerror(errno));
		if (msg) {
			error_msg=1;
			log_msg(msg, PRINT_PROMPT);
			free(msg);
		}
		else
			fprintf(stderr, "%s: chdir: '%s': %s\n", PROGRAM_NAME, path, 
					strerror(errno));
		char cwd[PATH_MAX]="";
		if (getcwd(cwd, sizeof(cwd)) == NULL) {
			asprintf(&msg, _("%s: Fatal error! Failed retrieving current "
							 "working directory\n"), PROGRAM_NAME);
			if (msg) {
				log_msg(msg, NOPRINT_PROMPT);
				free(msg);
			}
			else
				fprintf(stderr, _("%s: Fatal error! Failed retrieving "
								  "current working directory\n"), 
								  PROGRAM_NAME);
			exit(EXIT_FAILURE);
		}
		if (path)
			free(path);
		path=xcalloc(path, strlen(cwd)+1, sizeof(char));
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

	/* ### MAIN PROGRAM LOOP #### */

	/* This is the main structure of any basic shell 
	1 - Infinite loop
	2 - Grab user input
	3 - Parse user input
	4 - Execute command
	See https://brennan.io/2015/01/16/write-a-shell-in-c/
	*/
	while (1) { /* Or: for (;;) --> Infinite loop to keep the program 
		running */
		char *input=prompt(); /* Get input string from the prompt */
		if (input) {
			char **cmd=parse_input_str(input); /* Parse input string */
			free(input);
			if (cmd) {
				char **alias_cmd=check_for_alias(cmd);
				if (alias_cmd) {
					/* If an alias is found, the function frees cmd and 
					 * returns alias_cmd in its place to be executed by 
					 * exec_cmd() */
					exec_cmd(alias_cmd);
					for (int i=0;alias_cmd[i];i++)
						free(alias_cmd[i]);
					free(alias_cmd);
				}
				else {
					exec_cmd(cmd); /* Execute command */
					for (int i=0;i<=args_n;i++) 
						free(cmd[i]);
					free(cmd);
				}
			}
		}
	}
	
	return EXIT_SUCCESS; /* Never reached */
}

/* ###FUNCTIONS DEFINITIONS### */

int
is_link_to_dir(const char *link)
/* Check if symlink (LINK) is link to dir. Returns zero if true and one if
 * false */
{
	if (!link)
		return EXIT_FAILURE;

	char *linkname=realpath(link, NULL);
	if (linkname) {
		struct stat file_attrib;
		stat(linkname, &file_attrib);
		free(linkname);
		linkname=NULL;
		if ((file_attrib.st_mode & S_IFMT) == S_IFDIR)
			return EXIT_SUCCESS;
	}

	return EXIT_FAILURE;
}

int
is_internal_c(const char *cmd)
/* Check cmd against a list of internal commands. Used by parse_input_str()
 * when running chained commands */
{
	char *int_cmds[]={ "o", "open", "cd", "p", "pr", "prop", "t", "tr", 
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
					   "fs", NULL };
	char found=0;
	for (size_t i=0;int_cmds[i];i++) {
		if (strcmp(cmd, int_cmds[i]) == 0) {
			found=1;
			break;
		}
	}
	if (found) /* Check for the search and history functions as well */
		return 1;
	else if ((cmd[0] == '/' && access(cmd, F_OK) != 0) || cmd[0] == '!')
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

	size_t i=0, cmd_len=strlen(cmd);
	for (i=0;i<cmd_len;i++) {
		char *str=NULL;
		size_t len=0, cond_exec=0;

		/* Get command */
		str=xcalloc(str, strlen(cmd)+1, sizeof(char));
		while (cmd[i] && cmd[i] != '&' && cmd[i] != ';') {
			str[len++]=cmd[i++];
		}

		/* Should we execute conditionally? */
		if (cmd[i] == '&')
			cond_exec=1;

		/* Execute the command */
		if (str) {
			char **tmp_cmd=parse_input_str(str);
			free(str);
			if (tmp_cmd) {
				int exit_code=0;
				char **alias_cmd=check_for_alias(tmp_cmd);
				if (alias_cmd) {
					if (exec_cmd(alias_cmd) != 0)
						exit_code=1;
					for (size_t j=0;alias_cmd[j];j++)
						free(alias_cmd[j]);
					free(alias_cmd);
				}
				else {
					if (exec_cmd(tmp_cmd) != 0)
						exit_code=1;
					for (size_t j=0;j<=args_n;j++) 
						free(tmp_cmd[j]);
					free(tmp_cmd);
				}
				/* Do not continue if the execution was condtional and
				 * the previous command failed */
				if (cond_exec && exit_code)
					break;
			}
		}
	}
}

int
set_shell(const char *str)
{
	if (!*str)
		return EXIT_FAILURE;
	
	if (access(str, X_OK) == -1) {
		fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, str, strerror(errno));
		return EXIT_FAILURE;
	}

	if (sys_shell)
		free(sys_shell);
	sys_shell=xcalloc(sys_shell, strlen(str)+1, sizeof(char));
	strcpy(sys_shell, str);
	printf(_("Successfully set '%s' as %s default shell\n"), sys_shell, 
		   PROGRAM_NAME);

	return EXIT_SUCCESS;
}

char *
get_sys_shell(void)
/* Returns the user's default shell (from /etc/passwd) or NULL if not found */

{
	if (!user)
		return NULL;

	FILE *fp;
	fp=fopen("/etc/passwd", "r");
	if (!fp)
		return NULL;

	size_t user_len=strlen(user);
	char *userline=NULL;
	userline=xcalloc(userline, user_len+2, sizeof(char));
	sprintf(userline, "%s=", user);

	char *line=NULL, *shellpath=NULL;
	size_t line_size=0;
	ssize_t line_len=0;
	while ((line_len=getline(&line, &line_size, fp)) > 0) {
		if (strncmp(line, userline, user_len+1)) {
			shellpath=straftlst(line, ':');
			break;
		}
	}

	free(userline);
	userline=NULL;
	free(line);
	line=NULL;
	fclose(fp);

	if (shellpath) {
		size_t shell_len=strlen(shellpath);
		if (shellpath[shell_len-1] == '\n')
			shellpath[shell_len-1] = 0x00;
		return shellpath;
	}

	return NULL;
}

char **
get_substr(char *str, const char ifs)
/* Get all substrings from STR using IFS as substring separator, and, if there
 * is a range, expand it. Returns an array containing all substrings in STR 
 * plus expandes ranges or NULL if: STR is NULL or empty, STR contains only 
 * IFS(s), or in case of memory allocation error */
{
	if (!str || *str == 0x00)
		return NULL;
	
	/* ############## SPLIT THE STRING #######################*/
	
	char **substr=NULL;
	void *p=NULL;
	size_t str_len=strlen(str);
	char buf[str_len+1];
	memset(buf, 0x00, str_len+1);
	size_t length=0, substr_n=0;
	while (*str) {
		while (*str != ifs && *str != 0x00 && length < sizeof(buf))
			buf[length++]=*(str++);
		if (length) {
			buf[length]=0x00;
			p=realloc(substr, (substr_n+1)*sizeof(char *));
			if (!p) {
				/* Free whatever was allocated so far */
				for (size_t i=0;i<substr_n;i++)
					free(substr[i]);
				free(substr);
				return NULL;
			}
			substr=p;
			p=calloc(length+1, sizeof(char));
			if (!p) {
				for (size_t i=0;i<substr_n;i++)
					free(substr[i]);
				free(substr);
				return NULL;
			}
			substr[substr_n]=p;
			p=NULL;
			strncpy(substr[substr_n++], buf, length);
			length=0;
		}
		else
			str++;
	}
	if (!substr_n)
		return NULL;

	size_t i=0, j=0;
	p=realloc(substr, (substr_n+1)*sizeof(char *));
	if (!p) {
		for (i=0;i<substr_n;i++)
			free(substr[i]);
		free(substr);
		return NULL;
	}
	substr=p;
	p=NULL;
	substr[substr_n]=NULL;

	/* ################### EXPAND RANGES ######################*/
	
	int afirst=0, asecond=0, ranges_ok=0;
	//rsize=0

	for (i=0;substr[i];i++) {
		/* Check if substr is a valid range */
		ranges_ok=0;
		/* If range, get both extremes of it */
		for (j=1;substr[i][j];j++) {
			if (substr[i][j] == '-') {
				/* Get strings before and after the dash */
				char *first=strbfr(substr[i], '-');
				if (!first)
					break;
				char *second=straft(substr[i], '-');
				if (!second) {
					free(first);
					break;
				}
				/* Make sure it is a valid range */
				if (is_number(first) && is_number(second)) {
					afirst=atoi(first), asecond=atoi(second);
					if (asecond <= afirst) {
						free(first);
						free(second);
						break;
					}
					/* We have a valid range */
					ranges_ok=1;
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
		size_t k=0, next=0;
		char **rbuf=NULL;
		rbuf=xcalloc(rbuf, (substr_n + (asecond-afirst) 
					 + 1), sizeof(char *));
		/* Copy everything before the range expression
		 * into the buffer */
		for (j=0;j<i;j++) {
			rbuf[k]=xcalloc(rbuf[k], strlen(substr[j])+1,
							sizeof(char));
			strcpy(rbuf[k++], substr[j]);
		}
		/* Copy the expanded range into the buffer */
		for (j=afirst;j<=asecond;j++) {
			rbuf[k]=xcalloc(rbuf[k], digits_in_num(j) + 1, sizeof(char));
			sprintf(rbuf[k++], "%zu", j);
		}
		/* Copy everything after the range expression into 
		 * the buffer, if anything */
		if (substr[i+1]) {
			next=k;
			for (j=i+1;substr[j];j++) {
				rbuf[k]=xcalloc(rbuf[k], strlen(substr[j]) + 1,
								sizeof(char));
				strcpy(rbuf[k++], substr[j]);
			}
		}
		else /* If there's nothing after last range, there's no next either */
			next=0;

		/* Repopulate the original array with the expanded range and
		 * remaining strings */
		substr_n=k;
		for (j=0;substr[j];j++)
			free(substr[j]);
		substr=xrealloc(substr, (substr_n + 1) * sizeof(char *));
		for (j=0;j<substr_n;j++) {
			substr[j]=xcalloc(substr[j], strlen(rbuf[j]) + 1, 
							  sizeof(char));
			strcpy(substr[j], rbuf[j]);
			free(rbuf[j]);
		}
		free(rbuf);

		substr[j]=NULL;

		/* Proceede only if there's something after the last range */
		if (next)
			i=next;
		else
			break;
	}
	
	/* ############## REMOVE DUPLICATES ###############*/

	char **dstr=NULL;
	size_t len=0;
	for (i=0;i<substr_n;i++) {
		int duplicate=0;
		for (size_t d=i+1;d<substr_n;d++) {
			if (strcmp(substr[i], substr[d]) == 0) {
				duplicate=1;
				break;
			}
		}
		if (duplicate) {
			free(substr[i]);
			continue;
		}
		dstr=xrealloc(dstr, (len+1)*sizeof(char *));
		dstr[len]=xcalloc(dstr[len], strlen(substr[i])+1, sizeof(char));
		strcpy(dstr[len++], substr[i]);
		free(substr[i]);
	}
	free(substr);

	dstr=xrealloc(dstr, (len+1)*sizeof(char *));
	dstr[len]=NULL;
	
	return dstr;
}

int
is_color_code(char *str)
/* A really weak color codes test, true, but handles the most common input 
 * errors, like empty string, only spaces, and invalid chars. Returns zero 
 * if the string contains some char that is not a number or a semicolon. 
 * Otherwise returns 1. 
 * It will accept this: 110;;;;;;;;00, which is not a valid color code, and 
 * will reject this: 34, which is valid */
{
	while (*str) {
		if ((*str < 48 || *str > 57) && *str != ';' && *str != '\n')
			return 0;
		str++;
	}
	return 1;
}

void
set_colors(void)
/* Open the config file, get values for filetype colors and copy these values
 * into the corresponnding filetype variable. If some value is not found, or 
 * if it's a wrong value, the default is set. */
{
	char *dircolors=NULL;
	
	/* Get the colors line from the config file */
	FILE *fp_colors=fopen(CONFIG_FILE, "r");
	if (fp_colors) {
		char *line=NULL;
		ssize_t line_len=0;
		size_t line_size=0;
		while ((line_len=getline(&line, &line_size, fp_colors)) > 0) {
			if (strncmp(line, "Filetype colors=", 16) == 0) {
				char *opt_str=straft(line, '=');
				if (!opt_str)
					continue;
				size_t str_len=strlen(opt_str);
				dircolors=xcalloc(dircolors, str_len+1, sizeof(char));
				if (opt_str[str_len-1]== '\n')
					opt_str[str_len-1]=0x00;
				strcpy(dircolors, opt_str);
				free(opt_str);
				break;
			}
		}
		free(line);
		line=NULL;
		fclose(fp_colors);
	}

	if (dircolors) {
		/* Split the colors line into substrings (one per color) */
		char *p=dircolors, *buf=NULL, **colors=NULL;
		int len=0, words=0;
		while(*p) {
			switch (*p) {
				case '\'':
				case '"':
					p++;
				break;
				case ':':
					buf[len]=0x00;
					colors=xrealloc(colors, (words+1)*sizeof(char *));
					colors[words]=xcalloc(colors[words], len+1, sizeof(char));
					strcpy(colors[words++], buf);
					memset(buf, 0x00, len);
					len=0;
					p++;
				break;
				default:
					buf=xrealloc(buf, (len+2)*sizeof(char));
					buf[len++]=*(p++);
				break;
			}
		}
		p=NULL;
		free(dircolors);

		if (len) {
			buf[len]=0x00;
			colors=xrealloc(colors, (words+1)*sizeof(char *));
			colors[words]=xcalloc(colors[words], len+1, sizeof(char));
			strcpy(colors[words++], buf);
		}

		if (buf)
			free(buf);

		if (colors) {
			colors=xrealloc(colors, (words+1)*sizeof(char *));
			colors[words]=NULL;
		}

		/* Set the color variables */
		for (size_t i=0;colors[i];i++) {
			if (strncmp(colors[i], "di=", 3) == 0)
				if (!is_color_code(colors[i]+3))
					/* zero the corresponding variable as a flag for the
					 * check after this for loop and to prepare the variable
					 * to hold the default color */
					memset(di_c, 0x00, MAX_COLOR);
				else
					snprintf(di_c, MAX_COLOR-1, "\x1b[%sm", colors[i]+3);
			else if (strncmp(colors[i], "nd=", 3) == 0)
				if (!is_color_code(colors[i]+3))
					memset(nd_c, 0x00, MAX_COLOR);
				else
					snprintf(nd_c, MAX_COLOR-1, "\x1b[%sm", colors[i]+3);
			else if (strncmp(colors[i], "ed=", 3) == 0)
				if (!is_color_code(colors[i]+3))
					memset(ed_c, 0x00, MAX_COLOR);
				else
					snprintf(ed_c, MAX_COLOR-1, "\x1b[%sm", colors[i]+3);
			else if (strncmp(colors[i], "ne=", 3) == 0)
				if (!is_color_code(colors[i]+3))
					memset(ne_c, 0x00, MAX_COLOR);
				else
					snprintf(ne_c, MAX_COLOR-1, "\x1b[%sm", colors[i]+3);
			else if (strncmp(colors[i], "fi=", 3) == 0)
				if (!is_color_code(colors[i]+3))
					memset(fi_c, 0x00, MAX_COLOR);
				else
					snprintf(fi_c, MAX_COLOR-1, "\x1b[%sm", colors[i]+3);
			else if (strncmp(colors[i], "ef=", 3) == 0)
				if (!is_color_code(colors[i]+3))
					memset(ef_c, 0x00, MAX_COLOR);
				else
					snprintf(ef_c, MAX_COLOR-1, "\x1b[%sm", colors[i]+3);
			else if (strncmp(colors[i], "nf=", 3) == 0)
				if (!is_color_code(colors[i]+3))
					memset(nf_c, 0x00, MAX_COLOR);
				else
					snprintf(nf_c, MAX_COLOR-1, "\x1b[%sm", colors[i]+3);
			else if (strncmp(colors[i], "ln=", 3) == 0)
				if (!is_color_code(colors[i]+3))
					memset(ln_c, 0x00, MAX_COLOR);
				else
					snprintf(ln_c, MAX_COLOR-1, "\x1b[%sm", colors[i]+3);
			else if (strncmp(colors[i], "or=", 3) == 0)
				if (!is_color_code(colors[i]+3))
					memset(or_c, 0x00, MAX_COLOR);
				else
					snprintf(or_c, MAX_COLOR-1, "\x1b[%sm", colors[i]+3);
			else if (strncmp(colors[i], "ex=", 3) == 0)
				if (!is_color_code(colors[i]+3))
					memset(ex_c, 0x00, MAX_COLOR);
				else
					snprintf(ex_c, MAX_COLOR-1, "\x1b[%sm", colors[i]+3);
			else if (strncmp(colors[i], "ee=", 3) == 0)
				if (!is_color_code(colors[i]+3))
					memset(ee_c, 0x00, MAX_COLOR);
				else
					snprintf(ee_c, MAX_COLOR-1, "\x1b[%sm", colors[i]+3);
			else if (strncmp(colors[i], "bd=", 3) == 0)
				if (!is_color_code(colors[i]+3))
					memset(bd_c, 0x00, MAX_COLOR);
				else
					snprintf(bd_c, MAX_COLOR-1, "\x1b[%sm", colors[i]+3);
			else if (strncmp(colors[i], "cd=", 3) == 0)
				if (!is_color_code(colors[i]+3))
					memset(cd_c, 0x00, MAX_COLOR);
				else
					snprintf(cd_c, MAX_COLOR-1, "\x1b[%sm", colors[i]+3);
			else if (strncmp(colors[i], "pi=", 3) == 0)
				if (!is_color_code(colors[i]+3))
					memset(pi_c, 0x00, MAX_COLOR);
				else
					snprintf(pi_c, MAX_COLOR-1, "\x1b[%sm", colors[i]+3);
			else if (strncmp(colors[i], "so=", 3) == 0)
				if (!is_color_code(colors[i]+3))
					memset(so_c, 0x00, MAX_COLOR);
				else
					snprintf(so_c, MAX_COLOR-1, "\x1b[%sm", colors[i]+3);
			else if (strncmp(colors[i], "su=", 3) == 0)
				if (!is_color_code(colors[i]+3))
					memset(su_c, 0x00, MAX_COLOR);
				else
					snprintf(su_c, MAX_COLOR-1, "\x1b[%sm", colors[i]+3);
			else if (strncmp(colors[i], "sg=", 3) == 0)
				if (!is_color_code(colors[i]+3))
					memset(sg_c, 0x00, MAX_COLOR);
				else
					snprintf(sg_c, MAX_COLOR-1, "\x1b[%sm", colors[i]+3);
			else if (strncmp(colors[i], "tw=", 3) == 0)
				if (!is_color_code(colors[i]+3))
					memset(tw_c, 0x00, MAX_COLOR);
				else
					snprintf(tw_c, MAX_COLOR-1, "\x1b[%sm", colors[i]+3);
			else if (strncmp(colors[i], "st=", 3) == 0)
				if (!is_color_code(colors[i]+3))
					memset(st_c, 0x00, MAX_COLOR);
				else
					snprintf(st_c, MAX_COLOR-1, "\x1b[%sm", colors[i]+3);
			else if (strncmp(colors[i], "ow=", 3) == 0)
				if (!is_color_code(colors[i]+3))
					memset(ow_c, 0x00, MAX_COLOR);
				else
					snprintf(ow_c, MAX_COLOR-1, "\x1b[%sm", colors[i]+3);
			else if (strncmp(colors[i], "ca=", 3) == 0)
				if (!is_color_code(colors[i]+3))
					memset(ca_c, 0x00, MAX_COLOR);
				else
					snprintf(ca_c, MAX_COLOR-1, "\x1b[%sm", colors[i]+3);
			else if (strncmp(colors[i], "no=", 3) == 0) {
				if (!is_color_code(colors[i]+3))
					memset(no_c, 0x00, MAX_COLOR);
				else
					snprintf(no_c, MAX_COLOR-1, "\x1b[%sm", colors[i]+3);
			}
			free(colors[i]);
		}
		free(colors);
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
set_default_options (void)
/* Set the default options. Used when the config file isn't accessible */
{
	splash_screen=0; /* -1 means not set */
	welcome_message=1;
	show_hidden=1;
	long_view=0;
	ext_cmd_ok=0;
	pager=0;
	max_hist=500;
	max_log=1000;
	clear_screen=0;
	list_folders_first=1;
	cd_lists_on_the_fly=1;	
	case_sensitive=0;
	unicode=0;

	strcpy(prompt_color, "\001\x1b[00;36m\002");
	strcpy(text_color, "\001\x1b[00;39m\002");
	strcpy(eln_color, "\x1b[01;33m");
	strcpy(dir_count_color, "\x1b[00;97m");
	strcpy(default_color, "\x1b[00;39;49m");
	strcpy(div_line_color, "\x1b[00;34m");
	strcpy(welcome_msg_color, "\x1b[01;35m");
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
	
	if (sys_shell)
		free(sys_shell);
	sys_shell=NULL;
	sys_shell=get_sys_shell();
	if (!sys_shell) {
		sys_shell=xcalloc(sys_shell, 8, sizeof(char));
		strcpy(sys_shell, "/bin/sh");
	}
			
	/* Handle white color for different kinds of terminals: linux (8 colors), 
	 * (r)xvt (88 colors), and the rest (256 colors). This block is completely 
	 * subjective: in my opinion, some white colors that do look good on some 
	 * terminals, look bad on others */
	// 88 colors terminal //
/*	if (strcmp(getenv("TERM"), "xvt") == 0 
	|| strcmp(getenv("TERM"), "rxvt") == 0) {
		strcpy(default_color, "\001\e[97m\002");
		if (strcmp(text_color, "white") == 0)
			strcpy(text_color, "\001\e[97m\002");
	}
	// 8 colors terminal
	else if (strcmp(getenv("TERM"), "linux") == 0) {
		strcpy(default_color, "\001\e[37m\002");
		if (strcmp(text_color, "white") == 0)
			strcpy(text_color, "\001\e[37m\002");
	}
	// 256 colors terminal
	else {
		strcpy(default_color, "\001\e[38;5;253m\002");
		if (strcmp(text_color, "white") == 0)
			strcpy(text_color, "\001\e[38;5;253m\002");
	} */
}

char *
escape_str(char *str)
/* Take a string, supposedly a path or a filename, and returns the same
 * string escaped */
{
	if (!str)
		return NULL;
	size_t len=0;
	char *buf=NULL;
	buf=xcalloc(buf, strlen(str)*2+1, sizeof(char));
	while(*str) {
		if (is_quote_char(*str))
			buf[len++]='\\';
		buf[len++]=*(str++);
	}
	if (buf)
		return(buf);
	return NULL;
}

int
is_internal(const char *cmd)
/* Check cmd against a list of internal commands. Used by parse_input_str()
 * to know if it should perform additional expansions */
{
	char *int_cmds[]={ "o", "open", "cd", "p", "pr", "prop", "t", "tr", 
					   "trash", "s", "sel", NULL };
	char found=0;
	for (size_t i=0;int_cmds[i];i++) {
		if (strcmp(cmd, int_cmds[i]) == 0) {
			found=1;
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
	char *p=qc;
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
		return NULL;

	size_t buf_len=0, words=0, str_len=0;
	char *buf=NULL;
	buf=xcalloc(buf, 1, sizeof(char));;
	char quote=0;
	char **substr=NULL;
	while (*str) {
		switch (*str) {
			case '\'':
			case '"':
				if (str_len && *(str-1) == '\\') {
					buf=xrealloc(buf, (buf_len+1)*sizeof(char *));
					buf[buf_len++]=*str;
					break;					
				}
				quote=*str;
				str++;
				while (*str && *str != quote) {
					if (is_quote_char(*str)) {
						buf=xrealloc(buf, (buf_len+1)*sizeof(char *));
						buf[buf_len++]='\\';
					}
					buf=xrealloc(buf, (buf_len+1)*sizeof(char *));
					buf[buf_len++]=*(str++);
				}
				if (!*str) {
					fprintf(stderr, _("%s: Missing '%c'\n"), PROGRAM_NAME, 
							quote);
					/* Free the current buffer and whatever was already
					 * allocated */
					free(buf);
					for (size_t i=0;i<words;i++)
						free(substr[i]);
					free(substr);
					return NULL;
				}
			break;
			case '\t': /* TAB, new line char, and space are taken as word
			breaking characters */
			case '\n':
			case 0x20:
				if (str_len && *(str-1) == '\\') {
					buf=xrealloc(buf, (buf_len+1)*sizeof(char *));
					buf[buf_len++]=*str;
				}
				else {
					/* Add a terminating null byte */
					buf[buf_len]=0x00;
					if (buf_len > 0) {
						substr=xrealloc(substr, (words+1)*sizeof(char *));
						substr[words]=xcalloc(substr[words], buf_len+1, 
											  sizeof(char));
						strcpy(substr[words], buf);
						words++;
					}
					/* Clear te buffer to get a new string */
					memset(buf, 0x00, buf_len);
					buf_len=0;
				}
			break;
			default:
				buf=xrealloc(buf, (buf_len+1)*sizeof(char *));
				buf[buf_len++]=*str;
			break;
		}
		str++;
		str_len++;
	}
	
	/* The while loop stops when the null byte is reached, so that the last
	 * substring is not printed, but still stored in the buffer. Therefore,
	 * we need to add it, if not empty, to our subtrings array */
	buf[buf_len]=0x00;
	if (buf_len > 0) {
		if (!words)
			substr=xcalloc(substr, words+1, sizeof(char *));
		else
			substr=xrealloc(substr, (words+1)*sizeof(char *));
		substr[words]=xcalloc(substr[words], buf_len+1, sizeof(char));
		strcpy(substr[words], buf);
		words++;
	}
	free(buf);

	if (words) {
		/* Add a final null string to the array */
		substr=xrealloc(substr, (words+1)*sizeof(char *));
		substr[words]=NULL;
		args_n=words-1;
		return(substr);
	}
	else {
		args_n=0; /* Just in case, but I think it's not needed */
		return NULL;
	}
}

void
print_license(void)
{
	time_t rawtime=time(NULL);
	struct tm *tm=localtime(&rawtime);
	printf(_("%s, 2017-%d, %s\n\n\
This program is free software; you can redistribute it and/or modify \
it under the terms of the GNU General Public License (version 2 or later) \
as published by the Free Software Foundation.\n\n\
This program is distributed in the hope that it will be useful, but \
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY \
or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License \
for more details.\n\n\
You should have received a copy of the GNU General Public License \
along with this program. If not, write to the Free Software \
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA \
02110-1301, USA, or visit <http://www.gnu.org/licenses/>\n\n\
NOTE: For more information about the meaning of 'free software' run 'fs'.\n"), 
		   PROGRAM_NAME, tm->tm_year+1900, AUTHOR);
}

size_t
u8_xstrlen(const char *str)
/* An strlen implementation able to handle unicode characters. Taken from: 
 * https://stackoverflow.com/questions/5117393/number-of-character-cells-used-by-string
 * Explanation: strlen() counts bytes, not chars. Now, since ASCII chars take 
 * each 1 byte, the amount of bytes equals the amount of chars. However, 
 * non-ASCII chars are multibyte chars, that is, one char takes more than 1 
 * byte, and this is why strlen() does not work as expected for this kind of 
 * chars: a 6 chars string might take 12 or more bytes */
{
	size_t len=0;
	cont_bt=0;
	while (*(str++))
		if ((*str & 0xc0) != 0x80) /* Do not count continuation bytes (used 
		by multibyte, that is, non-ASCII characters) */
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

	char *deq_path=dequote_str(cmd[1], 0);
	if (!deq_path) {
		fprintf(stderr, _("%s: %s: Error dequoting filename\n"), 
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
	char *linkname=NULL, file_tmp[PATH_MAX]="", is_reg=0, is_link=0,
		 no_open_file=1, filetype[128]="";
		 /* Reserve a good amount of bytes for filetype: it cannot be known
		  * beforehand how many bytes the translated string will need */
	switch (file_attrib.st_mode & S_IFMT) {
		case S_IFBLK:
			/* Store file type to compose and print the error message, if 
			 * necessary */
			strcpy(filetype, _("block device"));
		break;
		case S_IFCHR:
			strcpy(filetype, _("character device"));
		break;
		case S_IFSOCK:
			strcpy(filetype, _("socket"));
		break;
		case S_IFIFO:
			strcpy(filetype, _("FIFO/pipe"));
		break;
		case S_IFDIR:
			/* Set the no_open_file flag to false, since dirs (and regular
			 * files) will be opened */
			no_open_file=0;
			cd_function(cmd[1]);
		break;
		/* If a symlink, find out whether it is a symlink to dir or to file */
		case S_IFLNK:
			linkname=realpath(deq_path, NULL);
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
			if (stat(linkname, &file_attrib) == -1) {
				/* Never reached: if linked file does not exist, realpath()
				 * returns NULL */
				fprintf(stderr, "%s: open: '%s -> %s': %s\n", PROGRAM_NAME, 
						deq_path, linkname, strerror(errno));
				free(linkname);
				free(deq_path);
				return EXIT_FAILURE;
			}
			is_link=1;
			switch (file_attrib.st_mode & S_IFMT) {
				/* Realpath() will never return a symlink, but an absolute 
				 * path, so that there is no need to check for symlinks */
				case S_IFBLK:
					strcpy(filetype, _("block device"));
				break;
				case S_IFCHR:
					strcpy(filetype, _("character device"));
				break;
				case S_IFSOCK:
					strcpy(filetype, _("socket"));
				break;
				case S_IFIFO:
					strcpy(filetype, _("FIFO/pipe"));
				break;
				case S_IFDIR:
					no_open_file=0;
					cd_function(linkname);
				break;
				case S_IFREG:
					no_open_file=0;
					is_reg=1;
					strncpy(file_tmp, linkname, PATH_MAX);
				break;
				default:
					strcpy(filetype, _("unknown file type"));
				break;
			}
		break;

		case S_IFREG:
			no_open_file=0;
			is_reg=1;
		break;
		default:
			strcpy(filetype, _("unknown file type"));
		break;
	}

	/* If neither directory nor regular file nor symlink (to directory or 
	 * regular file), print the corresponding error message */
	if (no_open_file) {
		if (is_link)
			fprintf(stderr, _("%s: '%s -> %s' (%s): \
Cannot open file. Try 'application filename'.\n"), PROGRAM_NAME, deq_path, 
					linkname, filetype);
		else
			fprintf(stderr, _("%s: '%s' (%s): Cannot open file. Try \
'application filename'.\n"), PROGRAM_NAME, deq_path, filetype);
	}
	if (linkname)
		free(linkname);

	/* At this point we know the first argument is a regular file or a symlink
	 * to a regular file */
	/* Open the file */
	if (is_reg) {
		/* If no application was specified as second argument, use xdg-open */
		if (!cmd[2] || strcmp(cmd[2], "&") == 0) {
			if (!(flags & XDG_OPEN_OK)) {
				fprintf(stderr, _("%s: xdg-open not found. Specify an \
application to open the file\nUsage: open ELN/filename [application] [&]\n"), 
						PROGRAM_NAME);
				free(deq_path);
				return EXIT_FAILURE;
			}
			/* If xdg-open exists */
			pid_t pid_open=fork();
			if (pid_open == 0) {
				set_signals_to_default();
				execle(xdg_open_path, "xdg-open", 
					   (is_link) ? file_tmp : deq_path, NULL, __environ);
				fprintf(stderr, "%s: open: %s\n", PROGRAM_NAME, 
						strerror(errno));
				exit(EXIT_FAILURE);
			}
			else {
				if (cmd[2] && strcmp(cmd[2], "&") == 0)
					run_in_background(pid_open);
				else
					run_in_foreground(pid_open);
			}		
		}
		/* If application has been passed as second argument */
		else {
			/* Check that application exists */
			char *cmd_path=get_cmd_path(cmd[2]);
			if (cmd_path) {
				pid_t pid_open=fork();
				if (pid_open == 0) {
					set_signals_to_default();
					execle(cmd_path, cmd[2], 
						   (is_link) ? file_tmp : deq_path, NULL, __environ);
				}
				else {
					/* If last argument is "&", run in background */
					if (cmd[args_n] && strcmp(cmd[args_n], "&") == 0)
						run_in_background(pid_open);
					else
						run_in_foreground(pid_open);
				}
				free(cmd_path);
			}
			else { /* If application not found */
				fprintf(stderr, _("%s: open: '%s': Command not found\n"), 
						PROGRAM_NAME, cmd[2]);
				free(deq_path);
				return EXIT_FAILURE;
			}
		}
	}
	free(deq_path);
	return EXIT_SUCCESS;
}

int
cd_function(char *new_path)
{
	int ret=-1;

	char *deq_path=dequote_str(new_path, 0);

	char buf[PATH_MAX]=""; /* Temporarily store new_path */
	/* If "cd" */
	if (!deq_path || deq_path[0] == 0x00) {
		if (user_home) {
			ret=chdir(user_home);
			if (ret == 0)
				strncpy(buf, user_home, PATH_MAX);
		}
		else {
			fprintf(stderr, _("%s: Home directory not found\n"), 
					PROGRAM_NAME);
			return EXIT_FAILURE;
		}
	}
	else {
		/* If "cd ." or "cd .." or "cd ../..", etc */
		if (strcmp(deq_path, ".") == 0 || strncmp(deq_path, "..", 2) == 0) {
			char *real_path=realpath(deq_path, NULL);
			if (!real_path) {
				fprintf(stderr, "%s: cd: '%s': %s\n", PROGRAM_NAME, deq_path,
						strerror(errno));
				free(deq_path);
				return EXIT_FAILURE;
			}
			ret=chdir(real_path);
			if (ret == 0)
				strncpy(buf, real_path, PATH_MAX);
			free(real_path);
		}
		/* If absolute path */
		else if (deq_path[0] == '/') {
			ret=chdir(deq_path);
			if (ret == 0)
				strncpy(buf, deq_path, PATH_MAX);
		}
		/* If relative path, add CWD to the beginning of new_path, except if 
		 * CWD is root (/). Otherwise, the resulting path would be "//path" */
		else {
			char tmp_path[PATH_MAX]="";
			snprintf(tmp_path, PATH_MAX, "%s/%s", 
					 (path[0] == '/' && path[1] == 0x00) ? "" : path, 
					 deq_path);
			ret=chdir(tmp_path);
			if (ret == 0)
				strncpy(buf, tmp_path, PATH_MAX);
		}
	}
	
	/* If chdir() was successful */
	if (ret == 0) {
		free(path);
		path=xcalloc(path, strlen(buf)+1, sizeof(char));
		strcpy(path, buf);
		add_to_dirhist(path);
		if (cd_lists_on_the_fly) {
			while (files--) free(dirlist[files]);
			list_dir();
		}
		free(deq_path);
		return EXIT_SUCCESS;
	}
	else { /* If chdir() failed */
		fprintf(stderr, "%s: cd: '%s': %s\n", PROGRAM_NAME, deq_path, 
				strerror(errno));
		free(deq_path);
		return EXIT_FAILURE;
	}
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

	int ret=chdir(old_pwd[dirhist_cur_index-1]);
	if (ret == 0) {
		free(path);
		path=xcalloc(path, strlen(old_pwd[dirhist_cur_index-1])+1, 
					 sizeof(char));
		strcpy(path, old_pwd[--dirhist_cur_index]);
		if (cd_lists_on_the_fly) {
			while (files--) free(dirlist[files]);
			list_dir();
		}
	}
	else
		fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, 
				old_pwd[dirhist_cur_index-1], strerror(errno));

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
	if (dirhist_cur_index+1 >= dirhist_total_index)
		return EXIT_SUCCESS;
	dirhist_cur_index++;

	int ret=chdir(old_pwd[dirhist_cur_index]);
	if (ret == 0) {
		free(path);
		path=xcalloc(path, strlen(old_pwd[dirhist_cur_index])+1, 
					 sizeof(char));
		strcpy(path, old_pwd[dirhist_cur_index]);
		if (cd_lists_on_the_fly) {
			while (files--) free(dirlist[files]);
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
	
	FILE *mp_fp=fopen("/proc/mounts", "r");
	if (mp_fp) {
		printf(_("%sMountpoints%s\n\n"), white, NC);
		/* The line variable should be able to store the device name,
		 * the mount point (PATH_MAX) and mount options. PATH_MAX*2 
		 * should be more than enough */
		char **mountpoints=NULL;
		int mp_n=0, exit_code=0;

		size_t line_size=0;
		char *line=NULL;
		ssize_t line_len=0;
		while ((line_len=getline(&line, &line_size, mp_fp)) > 0) {
			/* Do not list all mountpoints, but only those corresponding to 
			 * a block device (/dev) */
			if (strncmp(line, "/dev/", 5) == 0) {
				char *str=NULL;
				size_t counter=0;
				/* use strtok() to split LINE into tokens using space as
				 * IFS */
				str=strtok(line, " ");
				size_t dev_len=strlen(str);
				char device[dev_len+1];
				memset(device, 0x00, dev_len+1);
				strcpy(device, str);
				/* Print only the first two fileds of each /proc/mounts line */
				while (str && counter < 2) {
					if (counter == 1) { /* 1 == second field */
						printf("%s%d%s %s%s%s (%s)\n", eln_color, mp_n+1, NC, 
							   (access(str, R_OK|X_OK) == 0) ? blue : red, 
							   str, default_color, device);
						/* Store the second field (mountpoint) into an
						 * array */
						mountpoints=xrealloc(mountpoints, 
											 (mp_n+1)*sizeof(char *));
						mountpoints[mp_n]=xcalloc(mountpoints[mp_n], 
												  strlen(str)+1, 
												  sizeof(char));					
						strcpy(mountpoints[mp_n++], str);
					}
					str=strtok(NULL, " ,");
					counter++;
				}
			}
		}
		free(line);
		line=NULL;
		fclose(mp_fp);

		/* This should never happen: There should always be a mountpoint,
		 * at least "/" */
		if (mp_n == 0) {
			printf(_("mp: There are no available mountpoints\n"));
			return EXIT_SUCCESS;
		}
		
		/* Ask the user and chdir into the selected mountpoint */
		char *input=rl_no_hist(_("\nChoose a mountpoint ('q' to quit): "));
			
		if (input && input[0] != 0x00 && strcmp(input, "q") != 0) {
			int atoi_num=atoi(input);
			if (atoi_num > 0 && atoi_num <= mp_n) {
				int ret=chdir(mountpoints[atoi_num-1]);
				if (ret == 0) {
					free(path);
					path=xcalloc(path, strlen(mountpoints[atoi_num-1])+1, 
								 sizeof(char));
					strcpy(path, mountpoints[atoi_num-1]);
					add_to_dirhist(path);
					if (cd_lists_on_the_fly) {
						while (files--) free(dirlist[files]);
						list_dir();						
					}
				}
				else {
					fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, 
							mountpoints[atoi_num-1], strerror(errno));
					exit_code=1;
				}
			}
			else {
				printf(_("mp: '%s': Invalid mountpoint\n"), input);
				exit_code=1;
			}
		}
		
		/* Free stuff and exit */
		if (input)
			free(input);
		for (size_t i=0;i<mp_n;i++) {
			free(mountpoints[i]);
		}
		free(mountpoints);
		return exit_code;
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
	int max=term_cols-70;
	/* Do not allow max to be less than 20 (this is a more or less arbitrary
	 * value), specially because the result of the above operation could be 
	 * negative */
	if (max < 20) max=20;
	if (longest < max) max=longest;
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

	size_t msg_len=strlen(_msg);
	if (msg_len == 0)
		return;

	/* Store messages (for current session only) in an array, so that
	 * the user can check them via the 'msg' command */
	msgs_n++;
	messages=xrealloc(messages, (msgs_n+1)*sizeof(char *));
	messages[msgs_n-1]=xcalloc(messages[msgs_n-1], msg_len+1, 
							   sizeof(char));
	strcpy(messages[msgs_n-1], _msg);
	messages[msgs_n]=NULL;
	if (print) /* PRINT_PROMPT */
		/* The next prompt will take care of printing the message */
		print_msg=1;
	else /* NOPRINT_PROMPT */
		/* Print the message directly here */
		fprintf(stderr, "%s", _msg);
	
	if (!config_ok)
		return;

	/* If msg log file isn't set yet... This will happen if an error occurs
	 * before running init_config(), for example, if the user's home cannot be
	 * found */
	if (MSG_LOG_FILE && MSG_LOG_FILE[0] == 0x00) 
		return;

	FILE *msg_fp=fopen(MSG_LOG_FILE, "a");
	if (!msg_fp) {
		/* Do not log this error: I might incur in an infinite loop
		 * trying to access a file that cannot be accessed */
		fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, MSG_LOG_FILE, 
				strerror(errno));
		xgetchar(); puts("");
	}
	else {
		/* Write message to messages file: [date] msg */
		time_t rawtime=time(NULL);
		struct tm *tm=localtime(&rawtime);
		char date[64]="";
		strftime(date, sizeof(date), "%c", tm);
		fprintf(msg_fp, "[%d-%d-%dT%d:%d:%d] ", tm->tm_year+1900, 
				tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, 
				tm->tm_sec);
		fprintf(msg_fp, "%s", _msg);
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
		return NULL;
	char *first=NULL;
	first=strbfr(str, '-');
	if (!first) 
		return NULL;
	if (!is_number(first)) {
		free(first);
		return NULL;
	}
	char *second=NULL;
	second=straft(str, '-');
	if (!second) {
		free(first);
		return NULL;
	}
	if (!is_number(second)) {
		free(first);
		free(second);
		return NULL;
	}
	int afirst=atoi(first), asecond=atoi(second);
	free(first);
	free(second);

	if (listdir) {
		if (afirst <= 0 || afirst > files || asecond <= 0 || asecond > files
		|| afirst >= asecond)
			return NULL;
	}
	else
		if (afirst >= asecond)
			return NULL;
			
	int *buf=NULL;
	buf=xcalloc(buf, (asecond-afirst)+2, sizeof(int));
	size_t j=0;
	for (size_t i=afirst;i<=asecond;i++)
		buf[j++]=i;
	return buf;
}

int
recur_perm_check(const char *dirname)
/* Recursively check directory permissions (write and execute). Returns zero 
 * if OK, and one if at least one subdirectory does not have write/execute 
 * permissions */
{
	#ifndef PATH_MAX
		#define PATH_MAX 4096
	#endif

	DIR *dir;
	struct dirent *entry;

	if (!(dir=opendir(dirname)))
		return EXIT_FAILURE;

	while ((entry=readdir(dir)) != NULL) {
		if (entry->d_type == DT_DIR) {
			char dirpath[PATH_MAX]="";
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
				recur_perm_error_flag=1;
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
	short exit_code=-1; 
	int ret=-1;
	size_t file_len=strlen(file);
	if (file[file_len-1] == '/')
		file[file_len-1]=0x00;
	if (lstat(file, &file_attrib) == -1) {
		fprintf(stderr, _("'%s': No such file or directory\n"), file);
		return EXIT_FAILURE;
	}
	char *parent=strbfrlst(file, '/');
	if (!parent) {
		/* strbfrlst() will return NULL if file's parent is root (/), simply
		 * because in this case there's nothing before the last slash. 
		 * So, check if file's parent dir is root */
		if (file[0] == '/' && strcntchr(file+1, '/') == -1) {
			parent=xcalloc(parent, 2, sizeof(char));
			sprintf(parent, "/");
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
			ret=check_immutable_bit(file);
			if (ret == -1) {
				/* Error message is printed by check_immutable_bit() itself */
				exit_code=EXIT_FAILURE;
			}
			else if (ret == 1) {
				fprintf(stderr, _("'%s': Directory is immutable\n"), file);
				exit_code=EXIT_FAILURE;
			}
			/* Check the parent for appropriate permissions */
			else if (access(parent, W_OK|X_OK) == 0) {
				int files_n=count_dir(parent);
				if (files_n > 2) {
					/* I manually check here subdir because recur_perm_check() 
					 * will only check the contents of subdir, but not subdir 
					 * itself*/
					/* If the parent is ok and not empty, check subdir */
					if (access(file, W_OK|X_OK) == 0) {
						/* If subdir is ok and not empty, recusivelly check 
						 * subdir */
						files_n=count_dir(file);
						if (files_n > 2) {
							/* Reset the recur_perm_check() error flag. See 
							 * the note in the function block. */
							recur_perm_error_flag=0;
							if (recur_perm_check(file) == 0) {
								exit_code=EXIT_SUCCESS;
							}
							else
								/* recur_perm_check itself will print the 
								 * error messages */
								exit_code=EXIT_FAILURE;
						}
						else /* Subdir is ok and empty */
							exit_code=EXIT_SUCCESS;
					}
					else { /* No permission for subdir */
						fprintf(stderr, _("'%s': Permission denied\n"), file);
						exit_code=EXIT_FAILURE;
					}			
				}
				else
					exit_code=EXIT_SUCCESS;
			}
			else { /* No permission for parent */
				fprintf(stderr, _("'%s': Permission denied\n"), parent);
				exit_code=EXIT_FAILURE;
			}
			break;

		/* REGULAR FILE */
		case S_IFREG:
			ret=check_immutable_bit(file);
			if (ret == -1) {
				/* Error message is printed by check_immutable_bit() itself */
				exit_code=EXIT_FAILURE;
			}
			else if (ret == 1) {
				fprintf(stderr, _("'%s': File is immutable\n"), file);
				exit_code=EXIT_FAILURE;
			}
			else if (parent) {
				if (access(parent, W_OK|X_OK) == 0)
					exit_code=EXIT_SUCCESS;
				else {
					fprintf(stderr, _("'%s': Permission denied\n"), parent);
					exit_code=EXIT_FAILURE;
				}
			}
			break;

		/* SYMLINK */
		case S_IFLNK:
			/* Symlinks do not support immutable bit */
			if (parent) {
				if (access(parent, W_OK|X_OK) == 0)
					exit_code=EXIT_SUCCESS;
				else {
					fprintf(stderr, _("'%s': Permission denied\n"), parent);
					exit_code=EXIT_FAILURE;
				}
			}
			break;

		/* DO NOT TRASH ANYTHING WHICH IS NOT DIR, REG FILE OR SYMLINK */
		default: 
			fprintf(stderr, _("%s: trash: '%s': Unsuported file type\n"), 
					PROGRAM_NAME, file);
			exit_code=EXIT_FAILURE;
			break;
	}
	if (parent)
		free(parent);
	return exit_code;
}

int
trash_element (const char *suffix, struct tm *tm, char *file)
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
	char full_path[PATH_MAX]="";
	if (file[0] != '/') {
		/* Construct absolute path for file */
		snprintf(full_path, PATH_MAX, "%s/%s", path, file);
		if (wx_parent_check(full_path) != 0)
			return EXIT_FAILURE;
	}
	/* If absolute path */
	else if (wx_parent_check(file) != 0)
		return EXIT_FAILURE;
	
	int ret=-1;

	/* Create the trashed file name: orig_filename.suffix, where SUFFIX is
	 * current date and time */
	char *filename=NULL;
	if (file[0] != '/') /* If relative path */
		filename=straftlst(full_path, '/');
	else /* If absolute path */
		filename=straftlst(file, '/');
	if (!filename) {
		fprintf(stderr, _("%s: trash: '%s': Error getting filename\n"), 
				PROGRAM_NAME, file);
		return EXIT_FAILURE;
	}
	/* If the length of the trashed file name (orig_filename.suffix) is 
	 * longer than NAME_MAX (255), trim the original filename, so that
	 * (original_filename_len + 1 (dot) + suffix_len) won't be longer than
	 * NAME_MAX */
	size_t filename_len=strlen(filename), suffix_len=strlen(suffix);
	int size=(filename_len+suffix_len+1)-NAME_MAX;
	if (size > 0) {
		/* If SIZE is a positive value, that is, the trashed file name 
		 * exceeds NAME_MAX by SIZE bytes, reduce the original file name 
		 * SIZE bytes. Terminate the original file name (FILENAME) with a
		 * tilde (~), to let the user know it is trimmed */
		filename[filename_len-size-1]='~';
		filename[filename_len-size]=0x00;
	}
	size_t file_suffix_len=filename_len+suffix_len+2; /* 2 = dot + null byte */
	char file_suffix[file_suffix_len];
	memset(file_suffix, 0x00, file_suffix_len);
	snprintf(file_suffix, file_suffix_len, "%s.%s", filename, suffix);

	/* Copy the original file into the trash files directory */
	char *dest=NULL;
	dest=xcalloc(dest, strlen(TRASH_FILES_DIR)+strlen(file_suffix)+2, 
					   sizeof(char));
	sprintf(dest, "%s/%s", TRASH_FILES_DIR, file_suffix);
	char *tmp_cmd[]={ "cp", "-ra", file, dest, NULL };
	free(filename);
	ret=launch_execve(tmp_cmd);
	free(dest);
	if (ret != 0) {
		fprintf(stderr, _("%s: trash: '%s': Failed copying file to Trash\n"), 
				PROGRAM_NAME, file);
		return EXIT_FAILURE;
	}

	/* Generate the info file */
	size_t info_file_len=strlen(TRASH_INFO_DIR)+strlen(file_suffix)+12;
	char info_file[info_file_len];
	memset(info_file, 0x00, info_file_len);
	snprintf(info_file, info_file_len, "%s/%s.trashinfo", TRASH_INFO_DIR, 
			 file_suffix);
	FILE *info_fp=fopen(info_file, "w");
	if (!info_fp) { /* If error creating the info file */
		fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, info_file, 
				strerror(errno));
		/* Remove the trash file */
		char *trash_file=NULL;
		trash_file=xcalloc(trash_file, 
						   strlen(TRASH_FILES_DIR)+strlen(file_suffix)+2,
						   sizeof(char));
		sprintf(trash_file, "%s/%s", TRASH_FILES_DIR, file_suffix);
		char *tmp_cmd2[]={ "rm", "-r", trash_file, NULL };
		ret=launch_execve(tmp_cmd2);
		free(trash_file);
		if (ret != 0)
			fprintf(stderr, _("%s: trash: '%s/%s': Failed removing trash \
file\nTry removing it manually\n"), PROGRAM_NAME, TRASH_FILES_DIR, 
					file_suffix);
		return EXIT_FAILURE;
	}
	else { /* If info file was generated successfully */
		/* Encode path to URL format (RF 2396) */
		char *url_str=NULL;
		if (file[0] != '/')
			url_str=url_encode(full_path);
		else
			url_str=url_encode(file);			
		if (!url_str) {
			fprintf(stderr, _("%s: trash: '%s': Failed encoding path\n"), 
					PROGRAM_NAME, file);
			fclose(info_fp);
			return EXIT_FAILURE;
		}
		/* Write trashed file information into the info file */
		fprintf(info_fp, 
				"[Trash Info]\nPath=%s\nDeletionDate=%d%d%dT%d:%d:%d\n", 
				url_str, tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, 
				tm->tm_hour, tm->tm_min, tm->tm_sec);
		fclose(info_fp);
		free(url_str);
	}

	/* Remove the file to be trashed */
	char *tmp_cmd3[]={ "rm", "-r", file, NULL };
	ret=launch_execve(tmp_cmd3);
	/* If remove fails, remove trash and info files */
	if (ret != 0) {
		fprintf(stderr, _("%s: trash: '%s': Failed removing file\n"), 
				PROGRAM_NAME, file);
		char *trash_file=NULL;
		trash_file=xcalloc(trash_file, 
						   strlen(TRASH_FILES_DIR)+strlen(file_suffix)+2,
						   sizeof(char));
		sprintf(trash_file, "%s/%s", TRASH_FILES_DIR, file_suffix);
		char *tmp_cmd4[]={ "rm", "-r", trash_file, info_file, NULL };
		ret=launch_execve(tmp_cmd4);
		free(trash_file);
		if (ret != 0) {
			fprintf(stderr, _("%s: trash: Failed removing temporary files \
from Trash.\nTry removing them manually\n"), PROGRAM_NAME);
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}

void
remove_from_trash (void)
{
	/* List trashed files */
	/* Change CWD to the trash directory. Otherwise, scandir() will fail */
	if (chdir(TRASH_FILES_DIR) == -1) {
		asprintf(&msg, "%s: trash: '%s': %s\n", PROGRAM_NAME, 
				 TRASH_FILES_DIR, strerror(errno));
		if (msg) {
			log_msg(msg, NOPRINT_PROMPT);
			free(msg);
		}
		else
			fprintf(stderr, "%s: trash: '%s': %s\n", PROGRAM_NAME, 
					TRASH_FILES_DIR, strerror(errno));
		return;
	}
	size_t i=0;
	struct dirent **trash_files=NULL;
	int files_n=scandir(TRASH_FILES_DIR, &trash_files, skip_implied_dot, 
						(unicode) ? alphasort : (case_sensitive) ? xalphasort 
						: alphasort_insensitive);
	if (files_n) {
		printf(_("%sTrashed files%s%s\n\n"), white, NC, default_color);
		for (i=0;i<files_n;i++)
			colors_list(trash_files[i]->d_name, i+1, 0, 1);
	}
	else {
		printf(_("trash: There are no trashed files\n"));
		/* Do not use ch_dir() here, since it internally replaces path by
		 * the argument passed to it, but it happens that, here, both are 
		 * the same */
		if (chdir(path) == -1) { /* Restore CWD and return */
			asprintf(&msg, "%s: trash: '%s': %s\n", PROGRAM_NAME, 
					 path, strerror(errno));
			if (msg) {
				log_msg(msg, NOPRINT_PROMPT);
				free(msg);
			}
			else
				fprintf(stderr, "%s: trash: '%s': %s\n", PROGRAM_NAME, 
						path, strerror(errno));
		}
		return;
	}
	if (chdir(path) == -1) { /* Restore CWD and continue */
		asprintf(&msg, "%s: trash: '%s': %s\n", PROGRAM_NAME, 
				 path, strerror(errno));
		if (msg) {
			log_msg(msg, NOPRINT_PROMPT);
			free(msg);
		}
		else
			fprintf(stderr, "%s: trash: '%s': %s\n", PROGRAM_NAME, 
					path, strerror(errno));
		return;
	}
	
	/* Get user input */
	int rm_n=0, length=0;
	char c=0;
	printf(_("\n%s%sEnter 'q' to quit.\n"), NC, default_color);
	fputs(_("Elements to be removed (ex: 1 2 6, or *)? "), stdout);
	char **rm_elements=NULL;
	rm_elements=xcalloc(rm_elements, files_n, sizeof(char *));
	while (c != '\n') { /* Exit loop when Enter is pressed */
		/* Do not allow more removes than trashed files are */
		if (rm_n == files_n) break;
		rm_elements[rm_n]=xcalloc(rm_elements[rm_n], 5, sizeof(char)); 
		/* 4 = 4 digits num (max 9,999) */
		while (!isspace(c=getchar())) 
			rm_elements[rm_n][length++]=c;
		/* Add end of string char to the last position */
		rm_elements[rm_n++][length]=0x00;
		length=0;
	}
	rm_elements=xrealloc(rm_elements, sizeof(char **)*rm_n);

	/* Remove files */
	char rm_file[PATH_MAX]="", rm_info[PATH_MAX]="";
	int ret=-1;
	
	/* First check for exit, wildcard, and non-number args */
	for (i=0;i<rm_n;i++) {
		if (strcmp(rm_elements[i], "q") == 0) {
			for (size_t j=0; j<rm_n;j++)
				free(rm_elements[j]);
			free(rm_elements);
			for (size_t j=0; j<files_n;j++)
				free(trash_files[j]);
			free(trash_files);
			return;
		}
		else if (strcmp(rm_elements[i], "*") == 0) {
			for (size_t j=0;j<files_n;j++) {
				snprintf(rm_file, PATH_MAX, "%s/%s", TRASH_FILES_DIR, 
						trash_files[j]->d_name);
				snprintf(rm_info, PATH_MAX, "%s/%s.trashinfo", TRASH_INFO_DIR, 
						trash_files[j]->d_name);
				char *tmp_cmd[]={ "rm", "-r", rm_file, rm_info, NULL };
				ret=launch_execve(tmp_cmd);
				if (ret != 0) {
					fprintf(stderr, _("%s: trash: Error trashing %s\n"), 
							PROGRAM_NAME, trash_files[j]->d_name);
				}
				free(trash_files[j]);
			}
			free(trash_files);
			for (size_t j=0;j<rm_n;j++)
				free(rm_elements[j]);	
			free(rm_elements);
			return;
		}
		else if (!is_number(rm_elements[i])) {
			/* If I want to implement ranges, I should check here for it and 
			 * do what follows only if not range */
			fprintf(stderr, _("%s: trash: '%s': Invalid ELN\n"), PROGRAM_NAME, 
					rm_elements[i]);
			for (size_t j=0; j<rm_n;j++)
				free(rm_elements[j]);
			free(rm_elements);
			for (size_t j=0; j<files_n;j++)
				free(trash_files[j]);
			free(trash_files);
			return;
		}	
	}

	/* If all args are numbers, and neither 'q' nor wildcard */
	int rm_num=0;
	for (i=0;i<rm_n;i++) {
		rm_num=atoi(rm_elements[i]);
		if (rm_num <= 0 || rm_num > files_n) {
			fprintf(stderr, _("%s: trash: '%d': Invalid ELN\n"), PROGRAM_NAME, 
					rm_num);
			free(rm_elements[i]);
			continue;
		}
		snprintf(rm_file, PATH_MAX, "%s/%s", TRASH_FILES_DIR, 
				 trash_files[rm_num-1]->d_name);
		snprintf(rm_info, PATH_MAX, "%s/%s.trashinfo", TRASH_INFO_DIR, 
				 trash_files[rm_num-1]->d_name);
		char *tmp_cmd2[]={ "rm", "-r", rm_file, rm_info, NULL };
		ret=launch_execve(tmp_cmd2);
		if (ret != 0) {
			fprintf(stderr, _("%s: trash: Error trashing %s\n"), PROGRAM_NAME, 
					trash_files[rm_num-1]->d_name);
		}
		free(rm_elements[i]);
	}
	free(rm_elements);
	for (i=0;i<files_n;i++)
		free(trash_files[i]);
	free(trash_files);
}

int
untrash_element(char *file)
{
	if (!file)
		return EXIT_FAILURE;
	
	char undel_file[PATH_MAX]="", undel_info[PATH_MAX]="";
	snprintf(undel_file, PATH_MAX, "%s/%s", TRASH_FILES_DIR, file);
	snprintf(undel_info, PATH_MAX, "%s/%s.trashinfo", TRASH_INFO_DIR, file);
	FILE *info_fp;
	info_fp=fopen(undel_info, "r");
	if (info_fp) {
		/* (PATH_MAX*2)+14 = two paths plus 14 extra bytes for command */
		char *orig_path=NULL; //cmd[(PATH_MAX*2)+14]="";
		/* The max length for line is Path=(5) + PATH_MAX + \n(1) */
		char line[PATH_MAX+6];
		memset(line, 0x00, PATH_MAX+6);
		while (fgets(line, sizeof(line), info_fp))
			if (strncmp(line, "Path=", 5) == 0)
				orig_path=straft(line, '=');
		fclose(info_fp);
		
		/* If original path is NULL or empty, return */
		if (!orig_path)
			return EXIT_FAILURE;
		if (strcmp(orig_path, "") == 0) {
			free(orig_path);
			return EXIT_FAILURE;
		}
		
		/* Remove new line char from original path, if any */
		size_t orig_path_len=strlen(orig_path);
		if (orig_path[orig_path_len-1] == '\n')
			orig_path[orig_path_len-1]=0x00;
		
		/* Decode original path's URL format */
		char *url_decoded=url_decode(orig_path);
		if (!url_decoded) {
			fprintf(stderr, _("%s: undel: '%s': Failed decoding path\n"), 
					PROGRAM_NAME, orig_path);
			free(orig_path);
			return EXIT_FAILURE;
		}
		free(orig_path);
		
		/* Check existence and permissions of parent directory */
		char *parent=NULL;
		parent=strbfrlst(url_decoded, '/');
		if (!parent) {
			/* strbfrlst() returns NULL is file's parent is root (simply
			 * because there's nothing before last slash in this case). So, 
			 * check if file's parent is root. Else returns */
			if (url_decoded[0] == '/' && strcntchr(url_decoded+1, '/') == -1) {
				parent=xcalloc(parent, 2, sizeof(char));
				sprintf(parent, "/");
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
		char *tmp_cmd[]={ "cp", "-ra", undel_file, url_decoded, NULL };
		int ret=-1;
		ret=launch_execve(tmp_cmd);
		free(url_decoded);
		if (ret == 0) {
			char *tmp_cmd2[]={ "rm", "-r", undel_file, undel_info, NULL };
			ret=launch_execve(tmp_cmd2);
			if (ret != 0) {
				fprintf(stderr, _("%s: undel: '%s': Failed removing info \
file\n"), PROGRAM_NAME, undel_info);
				return EXIT_FAILURE;
			}
			else
				return EXIT_SUCCESS;
		}
		else {
			fprintf(stderr, _("%s: undel: '%s': Failed restoring trashed \
file\n"), PROGRAM_NAME, undel_file);
			return EXIT_FAILURE;
		}
	}
	else { /* !info_fp */
		fprintf(stderr, _("%s: undel: Info file for '%s' not found. \
Try restoring the file manually\n"), PROGRAM_NAME, file);
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
		asprintf(&msg, "%s: undel: '%s': %s\n", PROGRAM_NAME, 
				 TRASH_FILES_DIR, strerror(errno));
		if (msg) {
			log_msg(msg, NOPRINT_PROMPT);
			free(msg);
		}
		else
			fprintf(stderr, "%s: undel: '%s': %s\n", PROGRAM_NAME, 
					TRASH_FILES_DIR, strerror(errno));
		return EXIT_FAILURE;
	}

	/* Get trashed files */
	struct dirent **trash_files=NULL;
	int trash_files_n=scandir(TRASH_FILES_DIR, &trash_files, skip_implied_dot, 
							(unicode) ? alphasort : (case_sensitive) 
							? xalphasort : alphasort_insensitive);
	if (trash_files_n == 0) {
		puts(_("trash: There are no trashed files"));
		if (chdir(path) == -1) {
			asprintf(&msg, "%s: undel: '%s': %s\n", PROGRAM_NAME, path, 
					 strerror(errno));
			if (msg) {
				log_msg(msg, NOPRINT_PROMPT);
				free(msg);
			}
			else
				fprintf(stderr, "%s: undel: '%s': %s\n", PROGRAM_NAME, path, 
						strerror(errno));
			return EXIT_FAILURE;
		}
		return EXIT_SUCCESS;
	}

	int exit_code=0;
	/* if "undel all" (or "u a" or "u *") */
	if (comm[1] && (strcmp(comm[1], "*") == 0|| strcmp(comm[1], "a") == 0 
	|| strcmp(comm[1], "all") == 0)) {
		for (size_t j=0;j<trash_files_n;j++) {
			if (untrash_element(trash_files[j]->d_name) != 0)
				exit_code=1;
			free(trash_files[j]);
		}
		free(trash_files);
		if (chdir(path) == -1) {
			asprintf(&msg, "%s: undel: '%s': %s\n", PROGRAM_NAME, path, 
					 strerror(errno));
			if (msg) {
				log_msg(msg, NOPRINT_PROMPT);
				free(msg);
			}
			else
				fprintf(stderr, "%s: undel: '%s': %s\n", PROGRAM_NAME, path, 
						strerror(errno));
			return EXIT_FAILURE;
		}
		return EXIT_SUCCESS;
	}	
	
	/* List trashed files */
	printf(_("%sTrashed files%s%s\n\n"), white, NC, default_color);
	for (size_t i=0;i<trash_files_n;i++)
		colors_list(trash_files[i]->d_name, i+1, 0, 1);

	/* Go back to previous path */
	if (chdir(path) == -1) {
		asprintf(&msg, "%s: undel: '%s': %s\n", PROGRAM_NAME, path, 
				 strerror(errno));
		if (msg) {
			log_msg(msg, NOPRINT_PROMPT);
			free(msg);
		}
		else
			fprintf(stderr, "%s: undel: '%s': %s\n", PROGRAM_NAME, path, 
					strerror(errno));
		return EXIT_FAILURE;
	}

	/* Get user input */
	printf(_("\n%s%sEnter 'q' to quit.\n"), NC, default_color);
	int no_space=0, undel_n=0;
	char *line=NULL, **undel_elements=NULL;
	while (!line) {
		line=rl_no_hist(_("Element(s) to be undeleted (ex: 1 2-6, or *): "));
		if (!line)
			continue;
		for (size_t i=0;line[i];i++)
			if (line[i] != 0x20)
				no_space=1;
		if (line[0] == 0x00 || !no_space) {
			free(line);
			line=NULL;
		}
	}
	undel_elements=get_substr(line, ' ');
	free(line);
	if (undel_elements)
		for (size_t i=0;undel_elements[i];i++)
			undel_n++;
	else
		return EXIT_FAILURE;

	/* First check for quit, *, and non-number args */
	char free_and_return=0;
	for(size_t i=0;i<undel_n;i++) {
		if (strcmp(undel_elements[i], "q") == 0)
			free_and_return=1;
		else if (strcmp(undel_elements[i], "*") == 0) {
			for (size_t j=0;j<trash_files_n;j++)
				if (untrash_element(trash_files[j]->d_name) != 0)
					exit_code=1;
			free_and_return=1;
		}
		else if (!is_number(undel_elements[i])) {
			fprintf(stderr, _("undel: '%s': Invalid ELN\n"), 
					undel_elements[i]);
			exit_code=1;
			free_and_return=1;
		}
	} 
	/* Free and return if any of the above conditions is true */
	if (free_and_return) {
		size_t j=0;
		for (j=0;j<undel_n;j++)
			free(undel_elements[j]);
		free(undel_elements);
		for (j=0;j<trash_files_n;j++)
			free(trash_files[j]);
		free(trash_files);
		return exit_code;
	}

	/* Undelete trashed files */
	int undel_num=0;
	for(size_t i=0;i<undel_n;i++) {
		undel_num=atoi(undel_elements[i]);
		if (undel_num <= 0 || undel_num > trash_files_n) {
			fprintf(stderr, _("%s: undel: '%d': Invalid ELN\n"), PROGRAM_NAME, 
					undel_num);
			free(undel_elements[i]);
			continue;
		}
		
		/* If valid ELN */
		if (untrash_element(trash_files[undel_num-1]->d_name) != 0)
			exit_code=1;
		free(undel_elements[i]);
	}
	free(undel_elements);
	
	/* Free trashed files list */
	for (size_t i=0;i<trash_files_n;i++)
		free(trash_files[i]);
	free(trash_files);
	
	/* If some trashed file still remains, reload the undel screen */
	trash_n=count_dir(TRASH_FILES_DIR);
	if (trash_n <= 2)
		trash_n=0;
	if (trash_n)
		untrash_function(comm);

	return exit_code;
}

int
trash_clear(void)
{
	struct dirent **trash_files=NULL;
	int files_n=-1, exit_code=0;
	if (chdir(TRASH_FILES_DIR) == -1) {
		asprintf(&msg, "%s: trash: '%s': %s\n", PROGRAM_NAME, 
				 TRASH_FILES_DIR, strerror(errno));
		if (msg) {
			log_msg(msg, NOPRINT_PROMPT);
			free(msg);
		}
		else
			fprintf(stderr, "%s: trash: '%s': %s\n", PROGRAM_NAME, 
					TRASH_FILES_DIR, strerror(errno));		
		return EXIT_FAILURE;
	}
	files_n=scandir(TRASH_FILES_DIR, &trash_files, skip_implied_dot, 
					xalphasort);
	if (!files_n) {
		printf(_("trash: There are no trashed files\n"));
		return EXIT_SUCCESS;
	}

	for (size_t i=0;i<files_n;i++) {
		size_t info_file_len=strlen(trash_files[i]->d_name)+11;
		char info_file[info_file_len];
		memset(info_file, 0x00, info_file_len);
		snprintf(info_file, info_file_len, "%s.trashinfo", 
				trash_files[i]->d_name);
		char *file1=NULL;
		file1=xcalloc(file1, strlen(TRASH_FILES_DIR)+
							strlen(trash_files[i]->d_name)+2, 
							sizeof(char));
		sprintf(file1, "%s/%s", TRASH_FILES_DIR, trash_files[i]->d_name);
		char *file2=NULL;
		file2=xcalloc(file2, strlen(TRASH_INFO_DIR)+
							strlen(info_file)+2, sizeof(char));
		sprintf(file2, "%s/%s", TRASH_INFO_DIR, info_file);
		char *tmp_cmd[]={ "rm", "-r", file1, file2, NULL };
		int ret=launch_execve(tmp_cmd);
		free(file1);
		free(file2);
		if (ret != 0) {
			fprintf(stderr, _("%s: trash: '%s': Error removing trashed \
file\n"), PROGRAM_NAME, trash_files[i]->d_name);
			exit_code=1; /* If there is at least one error, return error */
		}
		free(trash_files[i]);
	}
	free(trash_files);

	if (chdir(path) == -1) {
		asprintf(&msg, "%s: trash: '%s': %s\n", PROGRAM_NAME, path, 
				strerror(errno));
			if (msg) {
				log_msg(msg, NOPRINT_PROMPT);
				free(msg);
			}
			else
				fprintf(stderr, "%s: trash: '%s': %s\n", PROGRAM_NAME, path, 
						strerror(errno));
			return EXIT_FAILURE;
	}
	return exit_code;
}

int
trash_function(char **comm)
{
	/* Create trash dirs, if necessary */
/*	struct stat file_attrib;
	if (stat(TRASH_DIR, &file_attrib) == -1) {
		char *trash_files=NULL;
		trash_files=xcalloc(trash_files, strlen(TRASH_DIR)+7, sizeof(char));
		sprintf(trash_files, "%s/files", TRASH_DIR);
		char *trash_info=NULL;
		trash_info=xcalloc(trash_info, strlen(TRASH_DIR)+6, sizeof(char));
		sprintf(trash_info, "%s/info", TRASH_DIR);		
		char *cmd[]={ "mkdir", "-p", trash_files, trash_info, NULL };
		int ret=launch_execve(cmd);
		free(trash_files);
		free(trash_info);
		if (ret != 0) {
			asprintf(&msg, 
					_("%s: mkdir: '%s': Error creating trash directory\n"), 
					PROGRAM_NAME, TRASH_DIR);
			if (msg) {
				log_msg(msg, NOPRINT_PROMPT);
				free(msg);
			}
			else
			fprintf(stderr,
					_("%s: mkdir: '%s': Error creating trash directory\n"), 
					PROGRAM_NAME, TRASH_DIR);
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
			asprintf(&msg, "%s: trash: '%s': %s\n", PROGRAM_NAME, 
					 TRASH_FILES_DIR, strerror(errno));
			if (msg) {
				log_msg(msg, NOPRINT_PROMPT);
				free(msg);
			}
			else
				fprintf(stderr, "%s: trash: '%s': %s\n", PROGRAM_NAME, 
						TRASH_FILES_DIR, strerror(errno));
			return EXIT_FAILURE;
		}
		struct dirent **trash_files=NULL;
		int files_n=scandir(TRASH_FILES_DIR, &trash_files, skip_implied_dot, 
							(unicode) ? alphasort : 
							(case_sensitive) ? xalphasort : 
							alphasort_insensitive);
		if (files_n) {
			for (size_t i=0;i<files_n;i++) {
				colors_list(trash_files[i]->d_name, i+1, 0, 1);
				free(trash_files[i]);
			}
			free(trash_files);
		}
		else
			printf(_("trash: There are no trashed files\n"));

		if (chdir(path) == -1) {
			asprintf(&msg, "%s: trash: '%s': %s\n", PROGRAM_NAME, path, 
					strerror(errno));
			if (msg) {
				log_msg(msg, NOPRINT_PROMPT);
				free(msg);
			}
			else
				fprintf(stderr, "%s: trash: '%s': %s\n", PROGRAM_NAME, path, 
						strerror(errno));
			return EXIT_FAILURE;
		}
		else
			return EXIT_SUCCESS;
	}
	else {
		/* Create suffix from current date and time to create unique filenames 
		 * for trashed files */
		int exit_code=0;
		time_t rawtime=time(NULL);
		struct tm *tm=localtime(&rawtime);
		char date[64]="";
		strftime(date, sizeof(date), "%c", tm);
		char suffix[68]="";
		snprintf(suffix, 67, "%d%d%d%d%d%d", tm->tm_year+1900, tm->tm_mon+1, 
				 tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
		
		if (strcmp(comm[1], "del") == 0 || strcmp(comm[1], "rm") == 0)
			remove_from_trash();
		else if (strcmp(comm[1], "clear") == 0)
			trash_clear();
		else {
			/* Trash files passed as arguments */
			for (size_t i=1;comm[i];i++) {
				char *deq_file=dequote_str(comm[i], 0);
				char tmp_comm[PATH_MAX]="";
				if (deq_file[0] == '/') /* If absolute path */
					strcpy(tmp_comm, deq_file);
				else { /* If relative path, add path to check against 
					TRASH_DIR */
					snprintf(tmp_comm, PATH_MAX, "%s/%s", path, deq_file);
				}
				/* Do not trash any of the parent directories of TRASH_DIR,
				 * that is, /, /home, ~/, ~/.local, ~/.local/share */
				if (strncmp(tmp_comm, TRASH_DIR, strlen(tmp_comm)) == 0) {
					fprintf(stderr, _("trash: Cannot trash '%s'\n"), tmp_comm);
					exit_code=1;
					continue;
				}
				/* Do no trash TRASH_DIR itself nor anything inside it, that 
				 * is, already trashed files */
				else if (strncmp(tmp_comm, TRASH_DIR, 
						 strlen(TRASH_DIR)) == 0) {
					printf(_("trash: Use 'trash del' to remove trashed \
files\n"));
					continue;
				}
				if (trash_element(suffix, tm, deq_file) != 0)
					exit_code=1;
				/* The trash_element() function will take care of printing
				 * error messages, if any */
				free(deq_file);
			}
		}
		return exit_code;
	}
}

void
add_to_dirhist(const char *dir_path)
/* Add new path (DIR_PATH) to visited directory history (old_pwd) */
{
	/* Do not add anything if new path equals last entry in directory history.
	 * Just update the current dihist index to reflect the path change */
	if (dirhist_total_index > 0) {
		if (strcmp(dir_path, old_pwd[dirhist_total_index-1]) == 0) {
			for (size_t i=dirhist_total_index-1;i>=0;i--) {
				if (strcmp(old_pwd[i], dir_path) == 0) {
					dirhist_cur_index=i;
					break;
				}
			}
			return;
		}
	}
	old_pwd=xrealloc(old_pwd, (dirhist_total_index+1)*sizeof(char *));
	old_pwd[dirhist_total_index]=xcalloc(old_pwd[dirhist_total_index], 
										 strlen(dir_path)+1, 
										 sizeof(char));
	dirhist_cur_index=dirhist_total_index;
	strcpy(old_pwd[dirhist_total_index++], dir_path);
}

void
readline_kbinds(void)
{
//	rl_command_func_t readline_kbind_action;
/* To get the keyseq value for a given key do this in an terminal:
 * C-v and then press the key (or the key combination). So, for example, 
 * C-v, C-right arrow gives "[[1;5C", which here should be written like this:
 * "\\e[1;5A" */

/* These keybindings work on all the terminals I tried: the linux built-in
 * console, aterm, urxvt, xterm, lxterminal, xfce4-terminal, gnome-terminal,
 * terminator, and st (with some patches, however, they might stop working in
 * st) */

	rl_bind_keyseq("\\M-c", readline_kbind_action); //key: 99
	rl_bind_keyseq("\\M-u", readline_kbind_action); //key: 117
	rl_bind_keyseq("\\M-j", readline_kbind_action); //key: 106
	rl_bind_keyseq("\\M-h", readline_kbind_action); //key: 104
	rl_bind_keyseq("\\M-k", readline_kbind_action); //key: 107
	rl_bind_keyseq("\\x1b[21~", readline_kbind_action); // F10: 126
	rl_bind_keyseq("\\M-e", readline_kbind_action); //key: 101
	rl_bind_keyseq("\\M-i", readline_kbind_action); //key: 105
	rl_bind_keyseq("\\M-f", readline_kbind_action); //key: 102
	rl_bind_keyseq("\\M-b", readline_kbind_action); //key: 98
	rl_bind_keyseq("\\M-l", readline_kbind_action); //key: 108
	rl_bind_keyseq("\\M-m", readline_kbind_action); //key: 109
	rl_bind_keyseq("\\M-a", readline_kbind_action); //key: 97
	rl_bind_keyseq("\\M-d", readline_kbind_action); //key: 100
	rl_bind_keyseq("\\M-r", readline_kbind_action); //key: 114
	rl_bind_keyseq("\\M-s", readline_kbind_action); //key: 115
	rl_bind_keyseq("\\C-r", readline_kbind_action); //key: 18

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
	int old_args=args_n;
	args_n=0;
	char **cmd=parse_input_str(str);
	puts("");
	if (cmd) {
		exec_cmd(cmd);
		/* While in the bookmarks or mountpoints screen, the kbind_busy 
		 * flag will be set to 1 and no keybinding will work. Once the 
		 * corresponding function exited, set the kbind_busy flag to zero, 
		 * so that keybindings work again */
		if (kbind_busy)
			kbind_busy=0;
		for (size_t i=0;i<=args_n;i++)
			free(cmd[i]);
		free(cmd);
		/* This call to prompt() just updates the prompt in case it was 
		 * modified, for example, in case of chdir, files selection, and 
		 * so on */
		char *buf=prompt();
		free(buf);
	}
	args_n=old_args;
}

int
readline_kbind_action (int count, int key) {
	/* Prevent Valgrind from complaining about unused variable */
	if (count) {}
/*	printf("Key: %d\n", key); */
	int status=0;
	static char long_status=0;
	/* Disable all keybindings while in the bookmarks or mountpoints screen */
	if (kbind_busy)
		return 0;
	switch (key) {
		/* A-c: Clear the current command line (== C-a, C-k). Very handy, 
		 * since C-c is currently disabled */
		case 99:
			puts("");
			rl_replace_line("", 0);
/*			puts("");
			rl_delete_text(0, rl_end);
			rl_end = rl_point = 0;
			rl_on_new_line(); */
		break;
		/* A-l: Toggle long view mode on/off */
		case 108:
			if(!long_status)
				/* If status == 0 set it to 1. In this way, the next time
				 * this function is called it will not be true, and the else
				 * clause will be executed instead */
				long_view=long_status=1;
			else
				long_view=long_status=0;
			keybind_exec_cmd("rf");
		break;

		/* A-m: List available mountpoints */
		case 109:
			/* Call the function only if it's not already running */
			if (!kbind_busy) {
				kbind_busy=1;
				keybind_exec_cmd("mp");
			}
			else
				return 0;
		break;

		/* A-a: Select all files in CWD */
		case 97:
			keybind_exec_cmd("sel * .*");
		break;

		/* A-d: Deselect all selected files */		
		case 100:
			keybind_exec_cmd("ds a");
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

		/* A-f: Toggle folders first on/off */
		case 102:
			status=list_folders_first;
			if (list_folders_first) list_folders_first=0;
			else list_folders_first=1;
			if (status != list_folders_first) {
				while (files--) free(dirlist[files]);
				/* Without this puts(), the first entries of the directories
				 * list are printed in the prompt line */
				puts("");
				list_dir();
			}
		break;
		
		/* A-i: Toggle hidden files on/off */
		case 105:
			status=show_hidden;
			if (show_hidden) show_hidden=0;
			else show_hidden=1;
			if (status != show_hidden) {
				while (files--) free(dirlist[files]);
				puts("");
				list_dir();
			}
		break;

		/* C-r: Refresh the screen */
		case 18:
			keybind_exec_cmd("rf");
		break;

		/* A-u: Change CWD to PARENT directory */
		case 117:
			/* If already root dir, do nothing */
			if (strcmp(path, "/") == 0)
				return 0;
			keybind_exec_cmd("cd ..");
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
			if (dirhist_cur_index+1 == dirhist_total_index)
				return 0;
			keybind_exec_cmd("forth");
		break;

		/* A-h: Change CWD to PREVIOUS directoy in history */
		case 104: /* Same as C-Down arrow (66) */
			/* If at the beginning of dir hist, do nothing */
			if (dirhist_cur_index == 0)
				return 0;
			keybind_exec_cmd("back");
		break;
		
		/* A-e: Change CWD to the HOME directory */
		case 101:
			/* If already in the home dir, do nothing */
			if (strcmp(path, user_home) == 0)
				return 0;
			keybind_exec_cmd("cd");
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
		
		/* F10: Open the config file */
		case 126:
			keybind_exec_cmd("edit");
		break;
	}
	rl_on_new_line();
	return 0;
}

char *
parse_usrvar_value(const char *str, const char c)
{
	if (c == 0x00 || !str) 
		return NULL;
	size_t str_len=strlen(str);
	for (size_t i=0;i<str_len;i++) {
		if (str[i] == c) {
			size_t index=i+1, j=0;
			if (i == str_len-1)
				return NULL;
			char *buf=NULL;
			buf=xcalloc(buf, (str_len-index)+1, sizeof(char));	
			for (i=index;i<str_len;i++) {
				if (str[i] != '"' && str[i] != '\'' && str[i] != '\\' 
				&& str[i] != 0x00) {
					buf[j++]=str[i];
				}
			}
			buf[j]=0x00;
			return buf;
		}
	}
	return NULL;
}

int
create_usr_var(char *str)
{
	char *name=strbfr(str, '=');
	char *value=parse_usrvar_value(str, '=');
	
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
	
	usr_var=xrealloc(usr_var, (usrvar_n+1)*sizeof(struct usrvar_t));
	usr_var[usrvar_n].name=xcalloc(usr_var[usrvar_n].name, strlen(name)+1, 
								   sizeof(char));
	usr_var[usrvar_n].value=xcalloc(usr_var[usrvar_n].value, strlen(value)+1, 
									sizeof(char));
	strcpy(usr_var[usrvar_n].name, name);
	strcpy(usr_var[usrvar_n++].value, value);
	free(name);
	free(value);
	
	return EXIT_SUCCESS;
}

void
free_stuff(void)
{
	size_t i=0;
	
	if (alt_profile)
		free(alt_profile);
	
	while (files--)
		free(dirlist[files]);
	free(dirlist);
	
	if (sel_n > 0) {
		for (i=0;i<(size_t)sel_n;i++)
			free(sel_elements[i]);
		free(sel_elements);
	}
	
	if (bin_commands) {
		for (i=0;bin_commands[i];i++)
			free(bin_commands[i]);
		free(bin_commands);
	}
	
	if (paths) {
		for (i=0;i<(size_t)path_n;i++)
			free(paths[i]);
		free(paths);
	}

	if (history) {
		for (i=0;i<(size_t)current_hist_n;i++)
			free(history[i]);
		free(history);
	}
	
	if (argv_bk) {
		for (i=0;i<(size_t)argc_bk;i++)
			free(argv_bk[i]);
		free(argv_bk);
	}
	
	if (dirhist_total_index) {
		for (i=0;i<(size_t)dirhist_total_index;i++)
			free(old_pwd[i]);
		free(old_pwd);
	}
	
	for (i=0;i<(size_t)usrvar_n;i++) {
		free(usr_var[i].name);
		free(usr_var[i].value);
	}
	free(usr_var);

	for (i=0;i<(size_t)aliases_n;i++)
		free(aliases[i]);
	free(aliases);

	for (i=0;i<(size_t)prompt_cmds_n;i++)
		free(prompt_cmds[i]);
	free(prompt_cmds);

	if (flags & XDG_OPEN_OK)
		free(xdg_open_path);
	
	if (user_home)
		free(user_home);

	if (user)
		free(user);
	
	if (msgs_n) {
		for (i=0;i<(size_t)msgs_n;i++)
			free(messages[i]);
		free(messages);
	}

	if (sys_shell)
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

	/* Restore the foreground color of the running terminal */
	printf("%s", NC);
}

void *
xrealloc(void *ptr, size_t size)
/*
Usage example:
	char **str=NULL; 
	str=xcalloc(str, 1, sizeof(char *));
	for (int i=0;i<10;i++)
		str=xrealloc(str, sizeof(char)*(i+1));
*/
{
	/* According to realloc manpage: "If ptr is NULL, then the call is 
	 * equivalent to malloc(size), for all values of size; if size is equal 
	 * to zero, and ptr is not NULL, then the call is equivalent to 
	 * free(ptr)" */
	if (size == 0)
		++size;
	void *new_ptr=realloc(ptr, size);
	if (!new_ptr) {
		new_ptr=realloc(ptr, size);
		if (!new_ptr) {
//			free(ptr);
			/* cppcheck complains about a double free here. However, the 
			 * realloc documentation says: "If realloc() fails, the original 
			 * block is left untouched; it is not freed or moved." */
			asprintf(&msg, _("%s: %s failed to allocate %zu bytes\n"), 
					 PROGRAM_NAME, __func__, size);
			if (msg) {
				log_msg(msg, NOPRINT_PROMPT);
				free(msg);
			}
			else
				fprintf(stderr, _("%s: %s failed to allocate %zu bytes\n"), 
						PROGRAM_NAME, __func__, size);
			exit(EXIT_FAILURE);
		}
	}
//	ptr=new_ptr;
//	new_ptr=NULL;
	return new_ptr;
}

char *
savestring (const char *str, size_t size)
{
	if (!str)
		return NULL;

	char *ptr=NULL;
	ptr=xcalloc(ptr, size+1, sizeof(char));
	strcpy(ptr, str);

	return ptr;
}

void *
xcalloc(void *ptr, size_t nmemb, size_t size)
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
	if (!new_ptr) {
		new_ptr=calloc(nmemb, size);
		if (!new_ptr) {
			asprintf(&msg, _("%s: %s failed to allocate %zu bytes\n"), 
					PROGRAM_NAME, __func__, nmemb*size);
			if (msg) {
				log_msg(msg, NOPRINT_PROMPT);
				free(msg);
			}
			else
				fprintf(stderr, _("%s: %s failed to allocate %zu bytes\n"), 
						PROGRAM_NAME, __func__, nmemb*size);
			exit(EXIT_FAILURE);
		}
	}
//	ptr=new_ptr;
//	new_ptr=NULL;
	return new_ptr;
}

void
xdg_open_check(void)
{
	xdg_open_path=get_cmd_path("xdg-open");
	if (!xdg_open_path) {
		flags &= ~XDG_OPEN_OK;
		asprintf(&msg, _("%s: 'xdg-open' not found. Without this program you \
will always need to specify an application when opening files.\n"), 
				 PROGRAM_NAME);
		if (msg) {
		warning_msg=1;
		log_msg(msg, PRINT_PROMPT);
		free(msg);
		}
		else
			printf(_("%s: 'xdg-open' not found. Without this program you \
will always need to specify an application when opening files.\n"), 
				   PROGRAM_NAME);
	}
	else
		flags |= XDG_OPEN_OK;
}

void
set_signals_to_ignore(void)
{
//	signal(SIGINT, signal_handler); /* C-c */
	signal(SIGINT, SIG_IGN); /* C-c */
	signal(SIGQUIT, SIG_IGN); /* C-\ */
	signal(SIGTSTP, SIG_IGN); /* C-z */
/*	signal(SIGTERM, SIG_IGN); // 'kill my_pid', even from outside this program
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
set_signals_to_default(void)
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
	shell_terminal=STDIN_FILENO;
	shell_is_interactive=isatty(shell_terminal); 
	/* isatty() returns 1 if the file descriptor, here STDIN_FILENO, outputs 
	 * to a terminal (is interactive). Otherwise, it returns zero */
	
	if (shell_is_interactive) {
		/* Loop until we are in the foreground */
		while (tcgetpgrp(shell_terminal) != (own_pid=getpgrp()))
			kill(- own_pid, SIGTTIN);

		/* Ignore interactive and job-control signals */
		set_signals_to_ignore();

		/* Put ourselves in our own process group */
		own_pid=get_own_pid();

		if (flags & ROOT_USR) {
			/* Make the shell pgid (process group id) equal to its pid */
			/* Without the setpgid line below, the program cannot be run with 
			* sudo, but it can be run nonetheless by the root user */
			if (setpgid(own_pid, own_pid) < 0) {
				asprintf(&msg, "%s: setpgid: %s\n", PROGRAM_NAME, 
						strerror(errno));
				if (msg) {
					log_msg(msg, NOPRINT_PROMPT);
					free(msg);
				}
				else
					fprintf(stderr, "%s: setpgid: %s\n", PROGRAM_NAME, 
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
	struct dirent **commands_bin[path_n];
	int	i, j, l=0, cmd_n[path_n], total_cmd=0;
	for (int k=0;k<path_n;k++) {
		cmd_n[k]=scandir(paths[k], &commands_bin[k], NULL, xalphasort);
		/* If paths[k] directory does not exist, scandir returns -1. Fedora,
		 * for example, adds $HOME/bin and $HOME/.local/bin to PATH disregarding
		 * if they exist or not. If paths[k] dir is empty do not use it 
		 * either */
		if (cmd_n[k] > 0)
			total_cmd+=cmd_n[k];
	}
	/* Add internal commands */
	/* Get amount of internal cmds (elements in INTERNAL_CMDS array) */
	int internal_cmd_n=(sizeof(INTERNAL_CMDS)/sizeof(INTERNAL_CMDS[0]))-1;
	bin_commands=xcalloc(bin_commands, total_cmd+internal_cmd_n+2, 
						 sizeof(char *));
	for (i=0;i<internal_cmd_n;i++) {
		bin_commands[l]=xcalloc(bin_commands[l], strlen(INTERNAL_CMDS[i])+1, 
								sizeof(char));
		strcpy(bin_commands[l++], INTERNAL_CMDS[i]);		
	}
	/* Add commands in PATH */
	for (i=0;i<path_n;i++) {
		if (cmd_n[i] > 0) {
			for (j=0;j<cmd_n[i];j++) {
				bin_commands[l]=xcalloc(bin_commands[l], 
										strlen(commands_bin[i][j]->d_name)+1, 
										sizeof(char));
				strcpy(bin_commands[l++], commands_bin[i][j]->d_name);
				free(commands_bin[i][j]);
			}
			free(commands_bin[i]);
		}
	}
//	bin_commands[l]=NULL;
}

char
*my_rl_quote(char *text, int m_t, char *qp)
/* Performs bash-style filename quoting for readline (put a backslash before
 * any char listed in rl_filename_quote_characters.
 * Modified version of:
 * https://utcc.utoronto.ca/~cks/space/blog/programming/ReadlineQuotingExample*/
{
	/* 
	 * How it works: p and r and pointers to the same memory location 
	 * initialized (calloced) twice as big as the line that needs to be 
	 * quoted (in case all chars in the line need to be quoted); tp is a 
	 * pointer to text, which contains the string to be quoted. We move 
	 * through tp to find all chars that need to be quoted ("a's" becomes 
	 * "a\'s", for example). At this point we cannot return p, since this 
	 * pointer is at the end of the string, so that we return r instead, 
	 * which is at the beginning of the same string pointed by p. 
	 * */
	char *r=NULL, *p=NULL, *tp=NULL;

	size_t text_len=strlen(text);
	/* Worst case: every character of text needs to be escaped. In this 
	 * case we need 2x text's bytes plus the NULL byte. */
	p=xcalloc(p, ((text_len*2)+1), sizeof(char));
	r=p;
	if (r == NULL)
		return NULL;

	/* Escape whatever char that needs to be escaped */
	for (tp=text; *tp; tp++) {
		if (is_quote_char(*tp))
			*p++='\\';
		*p++=*tp;
	}

	/* Add a final null byte to the string */
	*p++=0x00;
	return r;
}

char *
dequote_str(char *text, int m_t)
/* This function simply deescapes whatever escaped chars it founds in text,
 * so that readline can compare it to system filenames when completing paths. 
 * Returns a string containing text without escape sequences */
{
	if (!text)
		return NULL;
	
	/* At most, we need as many bytes as text (in case no escape sequence
	 * is found)*/
	char *buf=NULL;
	buf=xcalloc(buf, strlen(text)+1, sizeof(char));
	size_t len=0;

	while(*text) {
		switch (*text) {
			case '\\':
				if (*(text+1) && is_quote_char(*(text+1)))
					buf[len++]=*(++text);
				else
					buf[len++]=*text;
			break;
			default:
				buf[len++]=*text;
			break;
		}
		text++;
	}

	buf[len]=0x00;
	return buf;
}

char *
my_rl_path_completion(const char *text, int state)
/* Modified version of filename_completion_function() of an old Bash 
 * release (1.14.7) */
{
	/* state is zero before completion, and 1 ... n after getting possible
	 * completions. Example: 
	 * cd Do[TAB] -> state 0
	 * cuments/ -> state 1
	 * wnloads/ -> state 2 
	 * */

/*	if (strncmp(rl_line_buffer, "bm ", 3) == 0
	|| strncmp(rl_line_buffer, "bookmarks ", 10) == 0) {
		printf("\nBookmarks completion!\n");
		return NULL;
	} */

	int rl_complete_with_tilde_expansion=0;
	/* ~/Doc -> /home/user/Doc */

	static DIR *directory;
	static char *filename=(char *)NULL;
	static char *dirname=(char *)NULL;
	static char *users_dirname=(char *)NULL;
	static int filename_len;

	struct dirent *entry=(struct dirent *)NULL;

	/* If we don't have any state, then do some initialization. */
	if (!state) {
		char *temp;

		if (dirname)
			free(dirname);
		if (filename)
			free(filename);
		if (users_dirname)
			free(users_dirname);

		size_t text_len=strlen(text);
		if (text_len)
			filename=savestring(text, text_len);
		else
			filename=savestring("", 1);
		if (!*text)
			text = ".";
		if (text_len)
			dirname=savestring(text, text_len);
		else
			dirname=savestring("", 1);

		/* Get everything after last slash */
		temp=strrchr(dirname, '/');

		if (temp) { 
		  strcpy(filename, ++temp);
		  *temp=0x00;
		}
		else
			strcpy(dirname, ".");

		/* We aren't done yet.  We also support the "~user" syntax. */

		/* Save the version of the directory that the user typed. */
		size_t dirname_len=strlen(dirname);
		users_dirname=savestring(dirname, dirname_len);
//		{
			char *temp_dirname;
			int replace_dirname;

			temp_dirname=tilde_expand(dirname);
			free(dirname);
			dirname=temp_dirname;

			replace_dirname=0;
			if (rl_directory_completion_hook)
				replace_dirname=(*rl_directory_completion_hook) (&dirname);
			if (replace_dirname) {
				free(users_dirname);
				users_dirname=savestring(dirname, dirname_len);
			}
//		}
		directory=opendir(dirname);
		filename_len=strlen(filename);

		rl_filename_completion_desired=1;
    }

	/* At this point we should entertain the possibility of hacking wildcarded
	filenames, like /usr/man/man<WILD>/te<TAB>.  If the directory name
	contains globbing characters, then build an array of directories, and
	then map over that list while completing. */
	/* *** UNIMPLEMENTED *** */

	/* Now that we have some state, we can read the directory. */

	while (directory && (entry=readdir(directory))) {
		/* If the user entered nothing before TAB (ex: "cd [TAB]") */
		if (!filename_len) {
			/* If cmd is 'cd', match only dirs or symlinks to dir */
			if (strncmp(rl_line_buffer, "cd ", 3) == 0) {
				if (entry->d_type == DT_LNK) {
					if (is_link_to_dir(entry->d_name) == 0)
						break; /* Match */
				}
				else if (entry->d_type != DT_DIR)
					continue; /* No match */
			}
			/* All entries except "." and ".." are possible
			 * completions */
			if ((strcmp(entry->d_name, ".") != 0)
			&& (strcmp(entry->d_name, "..") != 0))
				break;
		}
		/* If there is at least one char to complete (ex: "cd .[TAB]")*/
		else {
			if (strncmp(rl_line_buffer, "cd ", 3) == 0) {
				if (entry->d_type == DT_LNK) {
					if (is_link_to_dir(entry->d_name) == 0)
						break;
				}
				if (entry->d_type != DT_DIR)
					continue;
			}
			/* Otherwise, if these match up to the length of filename, then
			it is a match. */
			if ((entry->d_name[0] == filename[0])
			&& (((int)strlen(entry->d_name)) >= filename_len)
			&& (strncmp(filename, entry->d_name, filename_len) == 0))
				break;
		}
	}

	if (!entry) {
		if (directory) {
			closedir(directory);
			directory=(DIR *)NULL;
		}
		if (dirname) {
			free(dirname);
			dirname=(char *)NULL;
		}
		if (filename) {
			free(filename);
			filename=(char *)NULL;
		}
		if (users_dirname) {
			free(users_dirname);
			users_dirname=(char *)NULL;
		}

		return (char *)NULL;
	}

	else {
		char *temp=NULL;

		/* dirname && (strcmp (dirname, ".") != 0) */
		if (dirname && (dirname[0] != '.' || dirname[1])) {
			if (rl_complete_with_tilde_expansion && *users_dirname == '~') {
				int dirlen=strlen(dirname);
				temp=xcalloc(temp, 2 + dirlen + strlen(entry->d_name), 
							 sizeof(char));
				strcpy(temp, dirname);
				/* Canonicalization cuts off any final slash present.  We 
				 * need to add it back. */
				if (dirname[dirlen-1] != '/') {
					temp[dirlen]='/';
					temp[dirlen+1]=0x00;
				}
			}
			else {
				  temp=xcalloc(temp, 1 + strlen(users_dirname)
							   + strlen(entry->d_name), sizeof(char));
				  strcpy(temp, users_dirname);
			}

			strcat(temp, entry->d_name);
		}
		else
			temp=savestring(entry->d_name, strlen(entry->d_name));

		return(temp);
	}
}

char **
my_rl_completion(const char *text, int start, int end)
{
	char **matches=NULL;
	if (start == 0) { /* Only for the first word entered in the prompt */
		/* Commands auto-completion */
		if (end == 0) { /* If text is empty, do nothing */
			/* Prevent readline from attempting path completion if 
			* rl_completion matches returns NULL */
			rl_attempted_completion_over=1;
			return NULL;
		}
		matches=rl_completion_matches(text, &bin_cmd_generator);
	}
	else { /* ELN auto-expansion !!! */
		int num_text=atoi(text);
		if (is_number(text) && num_text > 0 && num_text <= files)
			matches=rl_completion_matches(text, &filenames_generator);
	}
	/* If not first word and not a number, readline will attempt 
	 * path completion instead via my custom my_rl_path_completion () */
	return matches;
}

char *
filenames_generator(const char *text, int state)
{
	static int i;
	char *name;
	rl_filename_completion_desired=1;
	/* According to the GNU readline documention: "If it is set to a non-zero 
	 * value, directory names have a slash appended and Readline attempts to 
	 * quote completed filenames if they contain any embedded word break 
	 * characters." To make the quoting part work I had to specify a custom
	 * quoting function (my_rl_quote) */
	if (!state) /* state is zero only the first time readline is executed */
		i=0;
	int num_text=atoi(text);
	/* Check list of currently displayed files for a match */
	while (i < files && (name=dirlist[i++]) != NULL)
		if (strcmp(name, dirlist[num_text-1]) == 0)
			return strdup(name);

	return NULL;
}

char *
bin_cmd_generator(const char *text, int state)
{
	static int i;
	static size_t len;
	char *name;
	if (!state) {
		i=0;
		len=strlen(text);
	} /* The state variable is zero only the first time the function is 
	called, and a non-zero positive in later calls. This means that i and len 
	will be necessarilly initialized the first time */
	/* Look for files in PATH for a match */
	while ((name=bin_commands[i++])!=NULL) {
		if (strncmp(name, text, len) == 0)
			return strdup(name);
	}
	return NULL;
}

int
get_path_env(void)
/* Store all paths in the PATH environment variable into a globally declared 
 * array (paths) */
{
	size_t i=0;

	/* Get the value of the PATH env variable */
	char *path_tmp=NULL;
	for (i=0;__environ[i];i++) {
		if (strncmp(__environ[i], "PATH", 4) == 0) {
			path_tmp=straft(__environ[i], '=');
			break;
		}
	}
	if (!path_tmp)
		return 0;

	/* Get each path in PATH */
	int path_num=0, length=0;
	for (i=0;path_tmp[i];i++) {
		/* Store path in PATH in a tmp buffer */
		char buf[PATH_MAX]="";
		while (path_tmp[i] && path_tmp[i] != ':')
			buf[length++]=path_tmp[i++];
		/* Make room in paths for a new path */
		paths=xrealloc(paths, (path_num+1)*sizeof(char *));
		/* Allocate space (buf length) for the new path */
		paths[path_num]=xcalloc(paths[path_num], length+1, sizeof(char));
		/* Dump the buffer into the global paths array */
		strcpy(paths[path_num], buf);
		path_num++;
		length=0;
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
	size_t pnl_len=strlen(PNL);

	if (home_ok) {
		/* Set up program's directories and files (always per user) */

		/* alt_profile will not be NULL whenever the -P option is used
		 * to run the program under an alternative profile */
		if (alt_profile) {
			CONFIG_DIR=xcalloc(CONFIG_DIR, user_home_len + pnl_len + 10 
							   + strlen(alt_profile) + 1, sizeof(char));
			sprintf(CONFIG_DIR, "%s/.config/%s/%s", user_home, PNL, 
					alt_profile);
		}
		else {
			CONFIG_DIR=xcalloc(CONFIG_DIR, user_home_len + pnl_len + 17 + 1, 
							   sizeof(char));
			sprintf(CONFIG_DIR, "%s/.config/%s/default", user_home, PNL);
		}
		TRASH_DIR=xcalloc(TRASH_DIR, user_home_len+20, sizeof(char));
		sprintf(TRASH_DIR, "%s/.local/share/Trash", user_home);
		size_t trash_len=strlen(TRASH_DIR);
		TRASH_FILES_DIR=xcalloc(TRASH_FILES_DIR, trash_len+7, sizeof(char));
		sprintf(TRASH_FILES_DIR, "%s/files", TRASH_DIR);
		TRASH_INFO_DIR=xcalloc(TRASH_INFO_DIR, trash_len+6, sizeof(char));
		sprintf(TRASH_INFO_DIR, "%s/info", TRASH_DIR);
		size_t config_len=strlen(CONFIG_DIR);
		BM_FILE=xcalloc(BM_FILE, config_len+15, sizeof(char));
		sprintf(BM_FILE, "%s/bookmarks.cfm", CONFIG_DIR);
		LOG_FILE=xcalloc(LOG_FILE, config_len+9, sizeof(char));
		sprintf(LOG_FILE, "%s/log.cfm", CONFIG_DIR);
		LOG_FILE_TMP=xcalloc(LOG_FILE_TMP, config_len+13, sizeof(char));
		sprintf(LOG_FILE_TMP, "%s/log_tmp.cfm", CONFIG_DIR);
		HIST_FILE=xcalloc(HIST_FILE, config_len+13, sizeof(char));
		sprintf(HIST_FILE, "%s/history.cfm", CONFIG_DIR);
		CONFIG_FILE=xcalloc(CONFIG_FILE, config_len+pnl_len+4, sizeof(char));
		sprintf(CONFIG_FILE, "%s/%src", CONFIG_DIR, PNL);	
		PROFILE_FILE=xcalloc(PROFILE_FILE, config_len+pnl_len+10, 
							 sizeof(char));
		sprintf(PROFILE_FILE, "%s/%s_profile", CONFIG_DIR, PNL);
		MSG_LOG_FILE=xcalloc(MSG_LOG_FILE, config_len+14, sizeof(char));
		sprintf(MSG_LOG_FILE, "%s/messages.cfm", CONFIG_DIR);

		/* #### CHECK THE TRASH DIR #### */
		/* Create trash dirs, if necessary */
		int ret=-1;
		if (stat(TRASH_DIR, &file_attrib) == -1) {
			char *trash_files=NULL;
			trash_files=xcalloc(trash_files, strlen(TRASH_DIR)+7, 
								sizeof(char));
			sprintf(trash_files, "%s/files", TRASH_DIR);
			char *trash_info=NULL;
			trash_info=xcalloc(trash_info, strlen(TRASH_DIR)+6, sizeof(char));
			sprintf(trash_info, "%s/info", TRASH_DIR);		
			char *cmd[]={ "mkdir", "-p", trash_files, trash_info, NULL };
			ret=launch_execve(cmd);
			free(trash_files);
			free(trash_info);
			if (ret != 0) {
				trash_ok=0;
				asprintf(&msg, _("%s: mkdir: '%s': Error creating trash \
directory. Trash function disabled\n"), PROGRAM_NAME, TRASH_DIR);
				if (msg) {
					log_msg(msg, PRINT_PROMPT);
					free(msg);
				}
				else
					fprintf(stderr,	_("%s: mkdir: '%s': Error creating trash \
directory. Trash function disabled\n"), PROGRAM_NAME, TRASH_DIR);
			}
		}
		/* If it exists, check it is writable */
		else if (access(TRASH_DIR, W_OK) == -1) {
			trash_ok=0;
			asprintf(&msg, _("%s: '%s': Directory not writable. Trash \
function disabled\n"), PROGRAM_NAME, TRASH_DIR);
			if (msg) {
				log_msg(msg, PRINT_PROMPT);
				free(msg);
			}
			else
				fprintf(stderr, _("%s: '%s': Directory not writable. Trash \
function disabled\n"), PROGRAM_NAME, TRASH_DIR);
		}

		/* #### CHECK THE CONFIG DIR #### */
		/* If the config directory doesn't exist, create it */
		/* Use the GNU mkdir to let it handle parent directories */
		if (stat(CONFIG_DIR, &file_attrib) == -1) {
			char *tmp_cmd[]={ "mkdir", "-p", CONFIG_DIR, NULL }; 
			ret=launch_execve(tmp_cmd);
			if (ret != 0) {
				config_ok=0;
				asprintf(&msg, 
						 _("%s: mkdir: '%s': Error creating configuration \
directory. Bookmarks, commands logs, and command history are disabled. \
Program messages won't be persistent. Using default options\n"), 
						 PROGRAM_NAME, CONFIG_DIR);
				if (msg) {
					error_msg=1;
					log_msg(msg, PRINT_PROMPT);
					free(msg);
				}
				else
					fprintf(stderr, 
							_("%s: mkdir: '%s': Error creating configuration \
directory. Bookmarks, commands logs, and command history are disabled. \
Program messages won't be persistent. Using default options\n"), 
							PROGRAM_NAME, CONFIG_DIR);
			}
		}
		/* If it exists, check it is writable */
		else if (access(CONFIG_DIR, W_OK) == -1) {
			config_ok=0;
			asprintf(&msg, _("%s: '%s': Directory not writable. \
Bookmarks, commands logs, and commands history are disabled. Program \
messages won't be persistent. Using default options\n"), 
					 PROGRAM_NAME, CONFIG_DIR);
			if (msg) {
				error_msg=1;
				log_msg(msg, PRINT_PROMPT);
				free(msg);
			}
			else
				fprintf(stderr, _("%s: '%s': Directory not writable. \
Bookmarks, commands logs, and commands history are disabled. Program \
messages won't be persistent. Using default options\n"), 
						PROGRAM_NAME, CONFIG_DIR);			
		}

		/* #### CHECK THE PROFILE FILE #### */
		/* Open the profile file or create it, if it doesn't exist */
		if (config_ok && stat(PROFILE_FILE, &file_attrib) == -1) {
			FILE *profile_fp=fopen(PROFILE_FILE, "w");
			if (!profile_fp) {
				asprintf(&msg, "%s: fopen: '%s': %s\n", PROGRAM_NAME, 
						 PROFILE_FILE, strerror(errno));
				if (msg) {
					error_msg=1;
					log_msg(msg, PRINT_PROMPT);
					free(msg);
				}
				else
					fprintf(stderr, "%s: fopen: '%s': %s\n", PROGRAM_NAME, 
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
		FILE *config_fp;
		if (config_ok && stat(CONFIG_FILE, &file_attrib) == -1) {
			config_fp=fopen(CONFIG_FILE, "w");
			if (!config_fp) {
				asprintf(&msg, "%s: fopen: '%s': %s\n", PROGRAM_NAME, 
						 CONFIG_FILE, strerror(errno));
				if (msg) {
					error_msg=1;
					log_msg(msg, PRINT_PROMPT);
					free(msg);
				}
				else
					fprintf(stderr, "%s: fopen: '%s': %s\n", PROGRAM_NAME, 
							CONFIG_FILE, strerror(errno));
				config_ok=0;
			}
			else {
				/* Do not translate anything in the config file */
				fprintf(config_fp, "%s configuration file\n\
	########################\n\n", PROGRAM_NAME);
				fprintf(config_fp, "\
Filetype colors=\"di=01;34:nd=01;31:ed=00;34:ne=00;31:fi=00;39:ef=00;33:nf=00;31:ln=01;36:or=00;36:pi=40;33:so=01;35:bd=01;33;01:cd=01;37;01:su=37;41:sg=30;43:ca=30;41:tw=30;42:ow=34;42:st=37;44:ex=01;32:ee=00;32:ca=00;30;41:no=00;47;31\"\n\
Prompt color=00;36\n\
Text color=00;39;49\n\
ELN color=01;33\n\
Default color=00;39;49\n\
Dir counter color=00;39;49\n\
Dividing line color=00;34\n\
Welcome message color=01;35\n\
Welcome message=true\n\
Splash screen=false\n\
Show hidden files=true\n\
Long view mode=false\n\
External commands=false\n\
System shell=\n\
List folders first=true\n\
cd lists automatically=true\n\
Case sensitive list=false\n\
Unicode=false\n\
Pager=false\n\
Max history=500\n\
Max log=1000\n\
Clear screen=false\n\
Starting path=default\n");
			fprintf(config_fp, "#Default starting path is CWD\n");
			fprintf(config_fp, "#END OF OPTIONS\n\
\n###Aliases###\nalias ls='ls --color=auto -A'\n\
\n#PROMPT\n");
			fprintf(config_fp, "#Write below the commands you want to be \
executed before the prompt\n#Ex:\n");
			fprintf(config_fp, 
					"#date | awk '{print $1\", \"$2,$3\", \"$4}'\n\n#END \
OF PROMPT\n"); 
			fclose(config_fp);
			}
		}

		/* #### READ THE CONFIG FILE ##### */
		if (config_ok) {

			set_colors();

			char prompt_color_set=-1, text_color_set=-1, eln_color_set=-1, 
				 default_color_set=-1, dir_count_color_set=-1,
				 div_line_color_set=-1, welcome_msg_color_set=-1;
			config_fp=fopen(CONFIG_FILE, "r");
			if (!config_fp) {
				asprintf(&msg, _("%s: fopen: '%s': %s. Using default \
values.\n"), 
						 PROGRAM_NAME, CONFIG_FILE, strerror(errno));
				if (msg) {
					error_msg=1;
					log_msg(msg, PRINT_PROMPT);
					free(msg);
				}
				else
					fprintf(stderr, _("%s: fopen: '%s': %s. Using default \
values.\n"), 
							PROGRAM_NAME, CONFIG_FILE, strerror(errno));
			}
			else {
				#define MAX_BOOL 6
				/* starting path(14) + PATH_MAX + \n(1)*/
				char line[PATH_MAX+15];
				memset(line, 0x00, PATH_MAX+15);
				while (fgets(line, sizeof(line), config_fp)) {
					if (strncmp(line, "#END OF OPTIONS", 15) == 0)
						break;
					/* Check for the splas_screen flag. If -1, it was not
					 * set via command line, so that it must be set here */
					else if (splash_screen == -1 
					&& strncmp(line, "Splash screen=", 14) == 0) {
						char opt_str[MAX_BOOL]=""; /* false (5) + 1 */
						ret=sscanf(line, "Splash screen=%5s\n", opt_str);
						/* According to cppcheck: "sscanf() without field 
						 * width limits can crash with huge input data". 
						 * Field width limits = %5s */
						if (ret == -1)
							continue;
						if (strncmp(opt_str, "true", 4) == 0)
							splash_screen=1;
						else if (strncmp(opt_str, "false", 5) == 0)
							splash_screen=0;
						/* Default value (if option was found but value is 
						 * neither true nor false) */
						else
							splash_screen=0;
					}
					else if (strncmp(line, "Welcome message=", 16) == 0) {
						char opt_str[MAX_BOOL]="";
						ret=sscanf(line, "Welcome message=%5s\n", opt_str);
						if (ret == -1)
							continue;
						if (strncmp(opt_str, "true", 4) == 0)
							welcome_message=1;
						else if (strncmp(opt_str, "false", 5) == 0)
							welcome_message=0;
						else /* default */
							welcome_message=1;
					}
					else if (strncmp(line, "Clear screen=", 13) == 0) {
						char opt_str[MAX_BOOL]="";
						ret=sscanf(line, "Clear screen=%5s\n", opt_str);
						if (ret == -1)
							continue;
						if (strncmp(opt_str, "true", 4) == 0)
							clear_screen=1;
						else if (strncmp(opt_str, "false", 5) == 0)
							clear_screen=0;
						else /* default */
							clear_screen=0;
					}
					else if (show_hidden == -1 
					&& strncmp(line, "Show hidden files=", 18) == 0) {
						char opt_str[MAX_BOOL]="";
						ret=sscanf(line, "Show hidden files=%5s\n", opt_str);
						if (ret == -1)
							continue;
						if (strncmp(opt_str, "true", 4) == 0)
							show_hidden=1;
						else if (strncmp(opt_str, "false", 5) == 0)
							show_hidden=0;
						else /* default */
							show_hidden=1;
					}
					else if (long_view == -1 
					&& strncmp(line, "Long view mode=", 15) == 0) {
						char opt_str[MAX_BOOL]="";
						ret=sscanf(line, "Long view mode=%5s\n", opt_str);
						if (ret == -1)
							continue;
						if (strncmp(opt_str, "true", 4) == 0)
							long_view=1;
						else if (strncmp(opt_str, "false", 5) == 0)
							long_view=0;
						else /* default */
							long_view=0;
					}
					else if (ext_cmd_ok == -1 
					&& strncmp(line, "External commands=", 18) == 0) {
						char opt_str[MAX_BOOL]="";
						ret=sscanf(line, "External commands=%5s\n", opt_str);
						if (ret == -1)
							continue;
						if (strncmp(opt_str, "true", 4) == 0)
							ext_cmd_ok=1;
						else if (strncmp(opt_str, "false", 5) == 0)
							ext_cmd_ok=0;
						else /* default */
							ext_cmd_ok=0;
					}
					else if (strncmp(line, "System shell=", 13) == 0) {
						if (sys_shell) {
							free(sys_shell);
							sys_shell=NULL;
						}
						char opt_str[PATH_MAX]="";
						ret=sscanf(line, "System shell=%4095s\n", opt_str);
						if (ret == -1)
							continue;
						sys_shell=xcalloc(sys_shell, strlen(opt_str)+1, 
										  sizeof(char));
						strcpy(sys_shell, opt_str);
					}
					else if (list_folders_first == -1 
					&& strncmp(line, "List folders first=", 19) == 0) {
						char opt_str[MAX_BOOL]="";
						ret=sscanf(line, "List folders first=%5s\n", opt_str);
						if (ret == -1)
							continue;
						if (strncmp(opt_str, "true",4) == 0)
							list_folders_first=1;
						else if (strncmp(opt_str, "false", 5) == 0)
							list_folders_first=0;
						else /* default */
							list_folders_first=1;
					}
					else if (cd_lists_on_the_fly == -1 
					&& strncmp(line, "cd lists automatically=", 23) == 0) {
						char opt_str[MAX_BOOL]="";
						ret=sscanf(line, "cd lists automatically=%5s\n", 
								   opt_str);
						if (ret == -1)
							continue;
						if (strncmp(opt_str, "true", 4) == 0)
							cd_lists_on_the_fly=1;
						else if (strncmp(opt_str, "false", 5) == 0)
							cd_lists_on_the_fly=0;
						else /* default */
							cd_lists_on_the_fly=1;
					}
					else if (case_sensitive == -1 
					&& strncmp(line, "Case sensitive list=", 20) == 0) {
						char opt_str[MAX_BOOL]="";
						ret=sscanf(line, "Case sensitive list=%5s\n", 
								   opt_str);
						if (ret == -1)
							continue;
						if (strncmp(opt_str, "true", 4) == 0)
							case_sensitive=1;
						else if (strncmp(opt_str, "false", 5) == 0)
							case_sensitive=0;
						else /* default */
							case_sensitive=0;
					}
					else if (unicode == -1 
					&& strncmp(line, "Unicode=", 8) == 0) {
						char opt_str[MAX_BOOL]="";
						ret=sscanf(line, "Unicode=%5s\n", opt_str);
						if (ret == -1)
							continue;
						if (strncmp(opt_str, "true", 4) == 0)
							unicode=1;
						else if (strncmp(opt_str, "false", 5) == 0)
							unicode=0;
						else /* default */
							unicode=0;
					}
					else if (pager == -1 
					&& strncmp(line, "Pager=", 6) == 0) {
						char opt_str[MAX_BOOL]="";
						ret=sscanf(line, "Pager=%5s\n", opt_str);
						if (ret == -1)
							continue;
						if (strncmp(opt_str, "true", 4) == 0)
							pager=1;
						else if (strncmp(opt_str, "false", 5) == 0)
							pager=0;
						else /* Default */
							pager=0;
					}
					else if (strncmp(line, "Prompt color=", 13) == 0) {
						char *opt_str=NULL;
						opt_str=straft(line, '=');
						if (!opt_str)
							continue;
						if (!is_color_code(opt_str)) {
							free(opt_str);
							continue;
						}
						size_t opt_len=strlen(opt_str);
						/* Lines in files usually ends with a new line char.
						 * But this char brakes the color code, and 
						 * therefore needs to be removed */
						if (opt_str[opt_len-1] == '\n')
							opt_str[opt_len-1]=0x00;
						prompt_color_set=1;
						snprintf(prompt_color, sizeof(prompt_color), 
								 "\001\x1b[%sm\002", opt_str);
						free(opt_str);
					}
					else if (strncmp(line, "Text color=", 11) == 0) {
						char *opt_str=NULL;
						opt_str=straft(line, '=');
						if (!opt_str)
							continue;
						if (!is_color_code(opt_str)) {
							free(opt_str);
							continue;
						}
						size_t opt_len=strlen(opt_str);
						if (opt_str[opt_len-1] == '\n')
							opt_str[opt_len-1]=0x00;
						text_color_set=1;
						snprintf(text_color, sizeof(text_color), 
								 "\001\x1b[%sm\002", opt_str);
						free(opt_str);
					}

					else if (strncmp(line, "ELN color=", 10) == 0) {
						char *opt_str=NULL;
						opt_str=straft(line, '=');
						if (!opt_str)
							continue;
						if (!is_color_code(opt_str)) {
							free(opt_str);
							continue;
						}
						size_t opt_len=strlen(opt_str);
						if (opt_str[opt_len-1] == '\n')
							opt_str[opt_len-1]=0x00;
						eln_color_set=1;
						snprintf(eln_color, MAX_COLOR, "\x1b[%sm", opt_str);
						free(opt_str);
					}

					else if (strncmp(line, "Default color=", 14) == 0) {
						char *opt_str=NULL;
						opt_str=straft(line, '=');
						if (!opt_str)
							continue;
						if (!is_color_code(opt_str)) {
							free(opt_str);
							continue;
						}
						size_t opt_len=strlen(opt_str);
						if (opt_str[opt_len-1] == '\n')
							opt_str[opt_len-1]=0x00;
						default_color_set=1;
						snprintf(default_color, MAX_COLOR, "\x1b[%sm", 
								 opt_str);
						free(opt_str);
					}
					else if (strncmp(line, "Dir counter color=", 18) == 0) {
						char *opt_str=NULL;
						opt_str=straft(line, '=');
						if (!opt_str)
							continue;
						if (!is_color_code(opt_str)) {
							free(opt_str);
							continue;
						}
						size_t opt_len=strlen(opt_str);
						if (opt_str[opt_len-1] == '\n')
							opt_str[opt_len-1]=0x00;
						dir_count_color_set=1;
						snprintf(dir_count_color, MAX_COLOR, "\x1b[%sm", 
								 opt_str);
						free(opt_str);
					}
					else if (strncmp(line, "Welcome message color=", 
									 22) == 0) {
						char *opt_str=NULL;
						opt_str=straft(line, '=');
						if (!opt_str)
							continue;
						if (!is_color_code(opt_str)) {
							free(opt_str);
							continue;
						}
						size_t opt_len=strlen(opt_str);
						if (opt_str[opt_len-1] == '\n')
							opt_str[opt_len-1]=0x00;
						welcome_msg_color_set=1;
						snprintf(welcome_msg_color, MAX_COLOR, "\x1b[%sm", 
								 opt_str);
						free(opt_str);
					}
					else if (strncmp(line, "Dividing line color=", 20) == 0) {
						char *opt_str=NULL;
						opt_str=straft(line, '=');
						if (!opt_str)
							continue;
						if (!is_color_code(opt_str)) {
							free(opt_str);
							continue;
						}
						size_t opt_len=strlen(opt_str);
						if (opt_str[opt_len-1] == '\n')
							opt_str[opt_len-1]=0x00;
						div_line_color_set=1;
						snprintf(div_line_color, MAX_COLOR, "\x1b[%sm", 
								 opt_str);
						free(opt_str);
					}
					else if (strncmp(line, "Max history=", 12) == 0) {
						int opt_num=0;
						sscanf(line, "Max history=%d\n", &opt_num);
						if (opt_num <= 0)
							continue;
						max_hist=opt_num;
					}
					else if (strncmp(line, "Max log=", 8) == 0) {
						int opt_num=0;
						sscanf(line, "Max log=%d\n", &opt_num);
						if (opt_num <= 0)
							continue;
						max_log=opt_num;
					}
					else if (!path 
					&& strncmp(line, "Starting path=", 14) == 0) {
						char opt_str[PATH_MAX]="";
						ret=sscanf(line, "Starting path=%4095s\n", opt_str);				
						if (ret == -1)
							continue;
						/* If starting path is not "default", and exists, and 
						 * is a directory, and the user has appropriate 
						 * permissions, set path to starting path. If any of 
						 * these conditions is false, path will be set to 
						 * default, that is, CWD */
						if (strncmp(opt_str, "default", 7) != 0) {
							if (chdir(opt_str) == 0) {
								free(path);
								path=xcalloc(path, strlen(opt_str)+1, 
											 sizeof(char));
								strcpy(path, opt_str);
							}
							else {
								asprintf(&msg, "%s: '%s': %s\n", PROGRAM_NAME, 
										 opt_str, strerror(errno));
								if (msg) {
									warning_msg=1;
									log_msg(msg, PRINT_PROMPT);
									free(msg);
								}
								else
									printf("%s: '%s': %s\n", PROGRAM_NAME, 
										   opt_str, strerror(errno));
							}
						}
					}
				}
				fclose(config_fp);
			}

			/* If some option was not set, neither via command line nor via 
			 * the config file, or if this latter could not be read for any 
			 * reason, set the defaults */
			if (splash_screen == -1) splash_screen=0; /* -1 means not set */
			if (welcome_message == -1) welcome_message=1;
			if (show_hidden == -1) show_hidden=1;
			if (long_view == -1) long_view=0;
			if (ext_cmd_ok == -1) ext_cmd_ok=0;
			if (!sys_shell) {
				/* Get user's default shell */
				sys_shell=get_sys_shell();
				if (!sys_shell) { 
				/* Fallback to /bin/sh */
				sys_shell=xcalloc(sys_shell, 8, sizeof(char));
				strcpy(sys_shell, "/bin/sh");
				}
			}
			if (pager == -1) pager=0;
			if (max_hist == -1) max_hist=500;
			if (max_log == -1) max_log=1000;
			if (clear_screen == -1) clear_screen=0;
			if (list_folders_first == -1) list_folders_first=1;
			if (cd_lists_on_the_fly == -1) cd_lists_on_the_fly=1;	
			if (case_sensitive == -1) case_sensitive=0;
			if (unicode == -1) unicode=0;
			if (prompt_color_set == -1)
				strcpy(prompt_color, "\001\x1b[00;36m\002");
			if (text_color_set == -1)
				strcpy(text_color, "\001\x1b[00;39;49m\002");
			if (eln_color_set == -1)
				strcpy(eln_color, "\x1b[01;33m");
			if (default_color_set == -1)
				strcpy(default_color, "\x1b[00;39;49m");
			if (dir_count_color_set == -1)
				strcpy(dir_count_color, "\x1b[00;97m");
			if (div_line_color_set == -1)
				strcpy(div_line_color, "\x1b[00;34m");
			if (welcome_msg_color_set == -1)
				strcpy(welcome_msg_color, "\x1b[01;35");
			
			/* Handle white color for different kinds of terminals: linux 
			 * (8 colors), (r)xvt (88 colors), and the rest (256 colors). 
			 * This block is completely subjective: in my opinion, some 
			 * white colors that do look good on some terminals, look bad 
			 * on others */
			// 88 colors terminal
/*			if (strcmp(getenv("TERM"), "xvt") == 0 
						|| strcmp(getenv("TERM"), "rxvt") == 0) {
				strcpy(default_color, "\001\x1b[97m\002");
				if (strcmp(text_color, "white") == 0)
					strcpy(text_color, "\001\x1b[97m\002");
			}
			// 8 colors terminal
			else if (strcmp(getenv("TERM"), "linux") == 0) {
				strcpy(default_color, "\001\e[37m\002");
				if (strcmp(text_color, "white") == 0)
					strcpy(text_color, "\001\x1b[37m\002");
			}
			// 256 colors terminal
			else {
				strcpy(default_color, "\001\x1b[38;5;253m\002");
				if (strcmp(text_color, "white") == 0)
					strcpy(text_color, "\001\x1b[38;5;253m\002");
			} */
		}
		
		/* "XTerm*eightBitInput: false" must be set in HOME/.Xresources to 
		 * make some keybindings like Alt+letter work correctly in xterm-like 
		 * terminal emulators */
		/* However, there is no need to do this if using the linux console, 
		 * since we are not in a graphical environment */
		if (flags & GRAPHICAL) {
			/* Check ~/.Xresources exists and eightBitInput is set to false. 
			 * If not, create the file and set the corresponding value */
			char xresources[PATH_MAX]="";
			sprintf(xresources, "%s/.Xresources", user_home);
			FILE *xresources_fp=fopen(xresources, "a+");
			if (xresources_fp) {
				/* Since I'm looking for a very specific line, which is a 
				 * fixed line far below MAX_LINE, I don't care to get any 
				 * of the remaining lines truncated */
				char line[32]="", eight_bit_ok=0;
				while (fgets(line, sizeof(line), xresources_fp))
					if (strncmp(line, "XTerm*eightBitInput: false", 26) == 0)
						eight_bit_ok=1;
				if (!eight_bit_ok) {
					/* Set the file position indicator at the end of the 
					 * file */
					fseek(xresources_fp, 0L, SEEK_END);
					fprintf(xresources_fp, "\nXTerm*eightBitInput: false\n");
					fclose(xresources_fp);
					char *xrdb_path=get_cmd_path("xrdb");
					if (xrdb_path)
						launch_execle("xrdb -merge ~/.Xresources");
					asprintf(&msg, _("%s: Restart your %s for changes to \
~/.Xresources to take effect. Otherwise, %s keybindings might not work as \
expected.\n"), 
							 PROGRAM_NAME, (xrdb_path) ? _("terminal") 
							 : _("X session"), PROGRAM_NAME);
					if (msg) {
						warning_msg=1; /* Specify the kind of message */
						log_msg(msg, PRINT_PROMPT);
						free(msg);
					}
					else {
						printf(_("%s: Restart your %s for changes to \
~/.Xresources to take effect. Otherwise, %s keybindings might not work as \
expected.\n"), 
							   PROGRAM_NAME, (xrdb_path) ? _("terminal") 
							   : _("X session"), PROGRAM_NAME);
						printf(_("Press any key to continue... "));
						xgetchar(); puts("");
					}
					if (xrdb_path)
						free(xrdb_path);
				}
			}
			else {
				asprintf(&msg, "%s: fopen: '%s': %s\n", PROGRAM_NAME, 
						 xresources, strerror(errno));
				if (msg) {
					error_msg=1;
					log_msg(msg, PRINT_PROMPT);
					free(msg);
				}
				else
					fprintf(stderr, "%s: fopen: '%s': %s\n", PROGRAM_NAME, 
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
	if (stat(TMP_DIR, &file_attrib) == -1) {
/*		if (mkdir(TMP_DIR, 1777) == -1) { */
		char *tmp_cmd2[]={ "mkdir", "-m1777", TMP_DIR, NULL };
		int ret=launch_execve(tmp_cmd2);
		if (ret != 0) {
			selfile_ok=0;
			asprintf(&msg, "%s: mkdir: '%s': %s\n", PROGRAM_NAME, TMP_DIR, 
					 strerror(errno));
			if (msg) {
				error_msg=1;
				log_msg(msg, PRINT_PROMPT);
				free(msg);
			}
			else
				fprintf(stderr, "%s: mkdir: '%s': %s\n", PROGRAM_NAME, 
						TMP_DIR, strerror(errno));
		}
	}
	/* If the directory exists, check it is writable */
	else if (access(TMP_DIR, W_OK) == -1) {
		selfile_ok=0;
		asprintf(&msg, "%s: '%s': Directory not writable. Selected files \
won't be persistent\n", PROGRAM_NAME, TMP_DIR);
		if (msg) {
			error_msg=1;
			log_msg(msg, PRINT_PROMPT);
			free(msg);
		}
		else
			fprintf(stderr, "%s: '%s': Directory not writable. Selected \
files won't be persistent\n", PROGRAM_NAME, TMP_DIR);
	}

	if (selfile_ok) {
		/* Define the user's sel file. There will be one per user 
		 * (/tmp/clifm/sel_file_username) */
		size_t user_len=strlen(user);
		size_t tmp_dir_len=strlen(TMP_DIR);
		sel_file_user=xcalloc(sel_file_user, tmp_dir_len+pnl_len+user_len+8, 
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
		FILE *fp=fopen(PROFILE_FILE, "r");
		if (fp) {

			size_t line_size=0;
			char *line=NULL;
			ssize_t line_len=0;
			while ((line_len=getline(&line, &line_size, fp)) > 0) {
				if (strcntchr(line, '=') != -1 && !isdigit(line[0])) {
					create_usr_var(line);
				}
				if (strlen(line) != 0 && line[0] != '#') {
					args_n=0;
					char **cmds=parse_input_str(line);
					if (cmds) {
						no_log=1;
						exec_cmd(cmds);
						no_log=0;
						for (size_t i=0;i<=(size_t)args_n;i++)
							free(cmds[i]);
						free(cmds);
					}
					args_n=0;
				}
			}
			free(line);
			line=NULL;
			fclose(fp);
/*			char line[MAX_LINE]="";
			while(fgets(line, sizeof(line), fp)) {

			} */
		}
	}
}

char *
get_cmd_path(const char *cmd)
/* Get the path of a given command from the PATH environment variable. It 
 * basically does the same as the 'which' Linux command */
{
	char *cmd_path=NULL;
	cmd_path=xcalloc(cmd_path, PATH_MAX+1, sizeof(char));
	for (int i=0;i<path_n;i++) { /* Get the path from PATH env variable */
		snprintf(cmd_path, PATH_MAX, "%s/%s", paths[i], cmd);
		if (access(cmd_path, F_OK) == 0)
			return cmd_path;
	}
	/* If path was not found */
	free(cmd_path);
	return NULL;
}

void
external_arguments(int argc, char **argv)
/* Evaluate external arguments, if any, and change initial variables to its 
* corresponding value */
{
/* Link long (--option) and short options (-o) for the getopt_long function */
	/* Disable automatic error messages to be able to handle them myself via 
	* the '?' case in the switch */
	opterr=optind=0;
	static struct option longopts[]={
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
	int optc;
	/* Variables to store arguments to options (-p and -P) */
	char *path_value=NULL, *alt_profile_value=NULL;
	while ((optc=getopt_long(argc, argv, "+aAfFgGhiIlLoOp:P:sUuvx", longopts, 
		(int *)0)) != EOF) {
		/* ':' and '::' in the short options string means 'required' and 
		 * 'optional argument' respectivelly. Thus, 'p' and 'P' require an 
		 * argument here. The plus char (+) tells getopt to stop processing 
		 * at the first non-option (and non-argument) */
		switch (optc) {
			case 'a':
				flags &= ~HIDDEN; /* Remove HIDDEN from 'flags' */
				show_hidden=0;
				break;
			case 'A':
				flags |= HIDDEN; /* Add flag HIDDEN to 'flags' */
				show_hidden=1;
				break;
			case 'f':
				flags &= ~FOLDERS_FIRST;
				list_folders_first=0;
				break;
			case 'F':
				flags |= FOLDERS_FIRST;
				list_folders_first=1;
				break;
			case 'g':
				pager=1;
				break;
			case 'G':
				pager=0;
				break;
			case 'h':
				flags |= HELP;
				/* Do not display "Press any key to continue" */
				flags |= EXT_HELP;
				help_function();
				exit(EXIT_SUCCESS);
			case 'i':
				flags &= ~CASE_SENS;
				case_sensitive=0;
				break;
			case 'I':
				flags |= CASE_SENS;
				case_sensitive=1;
				break;
			case 'l':
				long_view=0;
				break;
			case 'L':
				long_view=1;
				break;
			case 'o':
				flags &= ~ON_THE_FLY;
				cd_lists_on_the_fly=0;
				break;
			case 'O':
				flags |= ON_THE_FLY;
				cd_lists_on_the_fly=1;
				break;
			case 'p':
				flags |= START_PATH;
				path_value=optarg;
				break;
			case 'P':
				flags |= ALT_PROFILE;				
				alt_profile_value=optarg;
				break;
			case 's':
				flags |= SPLASH;
				splash_screen=1;
				break;
			case 'u':
				unicode=0;
				break;
			case 'U':
				unicode=1;
				break;
			case 'v':
				flags |= PRINT_VERSION;
				version_function();
				print_license();
				exit(EXIT_SUCCESS);
			case 'x':
				ext_cmd_ok=1;
				break;
			case '?': /* If some unrecognized option was found... */
				/* Options that require an argument */
				if (optopt == 'p')
					fprintf(stderr, _("%s: option requires an argument -- \
'%c'\nTry '%s --help' for more information.\n"), PROGRAM_NAME, optopt, PNL);
				/* If unknown option is printable... */
				else if (isprint(optopt))
					fprintf(stderr, _("%s: invalid option -- '%c'\nUsage: %s \
[-aAfFgGhiIlLoOsuUvx] [-p path]\nTry '%s --help' for more information.\n"), 
							PROGRAM_NAME, optopt, PNL, PNL);
				else fprintf (stderr, _("%s: unknown option character \
'\\%x'\n"), PROGRAM_NAME, optopt);
				exit(EXIT_FAILURE);
		}
	}

	if ((flags & START_PATH) && path_value) {
		if (chdir(path_value) == 0) {
			if (path)
				free(path);
			path=xcalloc(path, strlen(path_value)+1, sizeof(char));
			strcpy(path, path_value);
		}
		else { /* Error changing directory */
			asprintf(&msg, "%s: '%s': %s\n", PROGRAM_NAME, path_value, 
					 strerror(errno));
			if (msg) {
				warning_msg=1;
				log_msg(msg, PRINT_PROMPT);
				free(msg);
			}
			else
				fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, 
						path_value, strerror(errno));		
		}
	}
	
	if ((flags & ALT_PROFILE) && alt_profile_value) {
		alt_profile=xcalloc(alt_profile, strlen(alt_profile_value) + 1,
							sizeof(char));
		strcpy(alt_profile, alt_profile_value);
		alt_profile_value=NULL;
	}
}

char *
home_tilde(const char *new_path)
/* Reduce "HOME" to tilde ("~"). The new_path variable is always either 
 * "HOME" or "HOME/file", that's why there's no need to check for "/file" */
{
	if (!home_ok)
		return NULL;
	
	char *path_tilde=NULL;	

	/* If path == HOME */
	if (strcmp(new_path, user_home) == 0) {
		path_tilde=xcalloc(path_tilde, 2, sizeof(char));		
		path_tilde[0]='~';
	}

	/* If path == HOME/file */
	else {
		path_tilde=xcalloc(path_tilde, strlen(new_path+user_home_len+1)+3, 
						   sizeof(char));
		sprintf(path_tilde, "~/%s", new_path+user_home_len+1);
	}
	
	return path_tilde;
}

int
brace_expansion(const char *str)
{
	size_t str_len=strlen(str);
	int j=0, k=0, l=0, initial_brace=0, closing_brace=-1;
	char *braces_root=NULL, *braces_end=NULL;
	for (int i=0;i<(int)str_len;i++) {
		if (str[i] == '{') {
			initial_brace=i;
			braces_root=xcalloc(braces_root, i+2, sizeof(char));			
			/* If any, store the string before the beginning of the braces */
			for (j=0;j<i;j++)
				braces_root[j]=str[j];
		}
		/* If any, store the string after the closing brace */
		if (initial_brace && str[i] == '}') {
			closing_brace=i;
			if ((str_len-1)-closing_brace > 0) {
				braces_end=xcalloc(braces_end, str_len-closing_brace, 
								   sizeof(char));
				for (k=closing_brace+1;str[k]!=0x00;k++)
					braces_end[l++]=str[k];
			}
		break;
		}
	}
	if (closing_brace == -1) {
		if (braces_root) 
			free(braces_root);
		return 0;
	}
	k=0;
	int braces_n=0;
	braces=xcalloc(braces, braces_n+1, sizeof(char *));
	for (int i=(int)j+1;i<closing_brace;i++) {
		char brace_tmp[closing_brace-initial_brace];
		memset(brace_tmp, 0x00, closing_brace-initial_brace+1);
		while (str[i] != '}' && str[i] != ',' && str[i] != 0x00)
			brace_tmp[k++]=str[i++];
		if (braces_end) {
			braces[braces_n]=xcalloc(braces[braces_n], strlen(braces_root)
									+strlen(brace_tmp)+strlen(braces_end)+1, 
									sizeof(char));
			sprintf(braces[braces_n], "%s%s%s", braces_root, brace_tmp, 
				braces_end);
		}
		else {
			braces[braces_n]=xcalloc(braces[braces_n], strlen(braces_root)
									+strlen(brace_tmp)+1, sizeof(char));
			sprintf(braces[braces_n], "%s%s", braces_root, brace_tmp);
		}
		if (str[i] == ',')
			braces=xrealloc(braces, (++braces_n+1)*sizeof(char *));
		k=0;
	}
	if (braces_root) 
		free(braces_root);
	if (braces_end) 
		free(braces_end);
	return braces_n+1;
}

char **
parse_input_str(char *str)
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
	/* #### 1) CHECK FOR SPECIAL FUNCTIONS #### */
	size_t i=0;
	char space_found=0, chaining=0, cond_cmd=0;
	for (i=0;str[i];i++) {
		/* Check for chained commands (cmd1;cmd2) */
		if (str[i] == ';' && i > 0 && str[i-1] != '\\') {
			chaining=1;
			break;
		}
		
		/* Check for conditional execution (cmd1 && cmd 2)*/
		if (str[i] == '&' && i > 0 && str[i-1] != '\\'
		&& str[i+1] && str[i+1] == '&') {
			cond_cmd=1;
			break;
		}
		
		/* If user defined variable or if invoking a command via ';' or ':' 
		 * send the whole string to exec_cmd(), in which case no expansion 
		 * is made: the command is literally send to the system shell. */
		else if (str[i] == '=' && i > 0 && str[i-1] != '\\') {
			for (size_t j=0;j<i;j++) {
				/* If there are no spaces before '=', take it as a variable. 
				 * This check is done in order to avoid taking as a variable 
				 * things like: 'ls -color=auto'*/
				if (str[j] == 0x20)
					space_found=1;
			}
			if (!space_found && !isdigit(str[0]))
				flags |= IS_USRVAR_DEF;
			break;
		}
	}

	/* If chained commands, check each of them. If at least one of them is
	 * internal, take care of the job (the system shell does not know our 
	 * internal commands and therefore cannot execute them); else, if no 
	 * internal command, let it to the system shell */
	if (chaining || cond_cmd) {
		size_t j=0, str_len=strlen(str), len=0, internal_ok=0;
		char *buf=NULL;
		/* Get each word (cmd) in STR */
		buf=xcalloc(buf, str_len+1, sizeof(char));
		for (j=0;j<str_len;j++) {
			while (str[j] && str[j] != 0x20 && str[j] != ';'
			&& str[j] != '&') {
				buf[len++]=str[j++];
			}
			if (buf && strcmp(buf, "&&") != 0) {
				if (is_internal_c(buf)) {
					internal_ok=1;
					break;
				}
			}
			memset(buf, 0x00, len);
			len=0;
		}

		if (buf)
			free(buf);
		
		if (internal_ok) {
			exec_chained_cmds(str);
			return NULL;
		}
	}
	
	if (flags & IS_USRVAR_DEF || str[0] == ':' 
	|| str[0] == ';') {
	/* If ";cmd" or ":cmd" the whole input line will be send to exec_cmd()
	 * and will be executed by the system shell via execle() */
		args_n=0;
		char **cmd=NULL;
		cmd=xcalloc(cmd, 2, sizeof(char *));
		cmd[0]=xcalloc(cmd[0], strlen(str)+1, sizeof(char));
		strcpy(cmd[0], str);
		cmd[1]=NULL;
		return cmd;
		/* Since we don't run split_str() here, dequoting and deescaping
		 * is performed directly by the system shell */
	}

	/* #### 2) SPLIT INPUT STRING INTO SUBSTRINGS #### */
	/* split_str() returns an array of strings without leading, terminating
	 * and double spaces. */
	char **substr=split_str(str);

	/* NOTE: isspace() not only checks for space, but also for new line, 
	 * carriage return, vertical and horizontal TAB. Be careful when replacing 
	 * this function. */

	if (!substr)
		return NULL;

	/* #### 3) CLIFM EXPANSIONS #### 
	 * Ranges, sel, ELN, and internal variables. 
	 * These expansions are specific to CliFM. To be able to use them even
	 * with external commands, they must be expanded here, before sending
	 * the input string, in case the command is external, to the system
	 * shell */
	is_sel=0, sel_is_last=0; //sel_no_sel=0;

	int int_array_max=10, range_array[int_array_max], ranges_ok=0;

	for (i=0;i<=(size_t)args_n;i++) {
		
		size_t substr_len=strlen(substr[i]);
		
		/* Check for ranges */
		for (size_t j=0;substr[i][j];j++) {
			/* If a range is found, store its index */
			if (j > 0 && j < substr_len && substr[i][j] == '-' && 
			isdigit(substr[i][j-1]) && isdigit(substr[i][j+1]))
				if (ranges_ok < int_array_max)
					range_array[ranges_ok++]=i;
		}
	
		/* Expand 'sel' only as an argument, not as command */
		if (i > 0 && strcmp(substr[i], "sel") == 0)
			is_sel=i;
	}
	
	/* ### 3.a) RANGES EXPANSION ### 
	 * Expand expressions like "1-3" to "1 2 3" if all the numbers in the
	 * range correspond to an ELN */

	if (ranges_ok) {
		int old_ranges_n=0;
		for (size_t r=0;r<ranges_ok;r++) {
			int ranges_n=0;
			int *ranges=expand_range(substr[range_array[r]+old_ranges_n], 1);
			if (ranges) {
				size_t j=0;
				for (ranges_n=0;ranges[ranges_n];ranges_n++);
				char **ranges_cmd=NULL;
				ranges_cmd=xcalloc(ranges_cmd, args_n+ranges_n+2, 
								   sizeof(char *));
				for (i=0;i<range_array[r]+old_ranges_n;i++) {
					ranges_cmd[j]=xcalloc(ranges_cmd[j], strlen(substr[i])+1, 
										  sizeof(char));
					strcpy(ranges_cmd[j++], substr[i]);
				}
				for (i=0;i<ranges_n;i++) {
					ranges_cmd[j]=xcalloc(ranges_cmd[j], 
										  digits_in_num(ranges[i])+1, 
										  sizeof(int));
					sprintf(ranges_cmd[j++], "%d", ranges[i]);
				}
				for (i=range_array[r]+old_ranges_n+1;i<=args_n;i++) {
					ranges_cmd[j]=xcalloc(ranges_cmd[j], strlen(substr[i])+1, 
										  sizeof(char));
					strcpy(ranges_cmd[j++], substr[i]);
				}
				ranges_cmd[j]=NULL;
				free(ranges);
				for (i=0;i<=args_n;i++) free(substr[i]);
				substr=xrealloc(substr, 
								(args_n+ranges_n+2)*sizeof(char *));
				for (i=0;i<j;i++) {
					substr[i]=xcalloc(substr[i], strlen(ranges_cmd[i])+1, 
									  sizeof(char));
					strcpy(substr[i], ranges_cmd[i]);
					free(ranges_cmd[i]);
				}
				free(ranges_cmd);
				args_n=j-1;
			}
			old_ranges_n+=(ranges_n-1);
		}
	}
	
	/* ### 3.b) SEL EXPANSION ### */
	if (is_sel) {
		if (is_sel == args_n) sel_is_last=1;
		if (sel_n) {
			size_t j=0;
			char **sel_array=NULL;
			sel_array=xcalloc(sel_array, args_n+sel_n+2, sizeof(char *));
			for (i=0;i<is_sel;i++) {
				sel_array[j]=xcalloc(sel_array[j], strlen(substr[i])+1, 
									 sizeof(char));
				strcpy(sel_array[j++], substr[i]);
			}
			for (i=0;i<sel_n;i++) {
				/* Escape selected filenames and copy them into tmp array */
				char *esc_str=escape_str(sel_elements[i]);
				if (esc_str) {
					sel_array[j]=xcalloc(sel_array[j], strlen(esc_str)+1,
										 sizeof(char));
					strcpy(sel_array[j++], esc_str);
					free(esc_str);
				}
				else {
					fprintf(stderr, _("%s: %s: Error quoting filename\n"), 
							PROGRAM_NAME, sel_elements[j]);
					/* Free elements selected thus far and and all the input
					 * substrings */
					size_t k=0;
					for (k=0;k<j;k++)
						free(sel_array[k]);
					free(sel_array);
					for (k=0;k<=args_n;k++)
						free(substr[k]);
					free(substr);
					return NULL;
				}
			}
			for (i=is_sel+1;i<=args_n;i++) {
				sel_array[j]=xcalloc(sel_array[j], strlen(substr[i])+1, 
									 sizeof(char));
				strcpy(sel_array[j++], substr[i]);
			}
			for (i=0;i<=args_n;i++)
				free(substr[i]);
			substr=xrealloc(substr, (args_n+sel_n+2)*sizeof(char *));
			for (i=0;i<j;i++) {
				substr[i]=xcalloc(substr[i], strlen(sel_array[i])+1, 
									  sizeof(char));
				strcpy(substr[i], sel_array[i]);
				free(sel_array[i]);
			}
			free(sel_array);
			substr[i]=NULL;
			args_n=j-1;
		}
		else {
			/* 'sel' is an argument, but there are no selected files. */ 
			fprintf(stderr, _("%s: There are no selected files\n"), 
					PROGRAM_NAME);
			for (size_t j=0;j<=args_n;j++)
				free(substr[j]);
			free(substr);
			return NULL;
		}
	}

	for (i=0;i<=(size_t)args_n;i++) {

		/* ### 3.c) ELN EXPANSION ### */
		/* i must be bigger than zero because the first string in comm_array, 
		 * the command name, should NOT be expanded, but only arguments. 
		 * Otherwise, if the expanded ELN happens to be a program name as 
		 * well, this program will be executed, and this, for sure, is to be 
		 * avoided */
		if (i > 0 && is_number(substr[i])) {
			int num=atoi(substr[i]);
			/* Expand numbers only if there is a corresponding ELN */
			if (num > 0 && num <= files) {
				/* Replace the ELN by the corresponding escaped filename */
				char *esc_str=escape_str(dirlist[num-1]);
				if (esc_str) {
					substr[i]=xrealloc(substr[i], 
									   (strlen(esc_str)+1)*sizeof(char));
					strcpy(substr[i], esc_str);
					free(esc_str);
				}
				else {
					fprintf(stderr, _("%s: %s: Error quoting filename\n"), 
							PROGRAM_NAME, dirlist[num-1]);
					/* Free whatever was allocated thus far */
					for (size_t j=0;j<=args_n;j++)
						free(substr[j]);
					free(substr);
					return NULL;
				}
			}
		}
		
		/* ### 3.d) USER DEFINED VARIABLES EXPANSION ### */
		if (substr[i][0] == '$' && substr[i][1] != '(') {
			char *var_name=straft(substr[i], '$');
			if (var_name) {
				for (unsigned j=0;(int)j<usrvar_n;j++) {
					if (strcmp(var_name, usr_var[j].name) == 0) {
						substr[i]=xrealloc(substr[i], 
								(strlen(usr_var[j].value)+1)*sizeof(char));
						strcpy(substr[i], usr_var[j].value);
					}
				}
				free(var_name);
			}
			else {
				fprintf(stderr, _("%s: %s: Error getting variable name\n"), 
						PROGRAM_NAME, substr[i]);
				for (size_t j=0;j<=args_n;j++)
					free(substr[j]);
				free(substr);
				return NULL;
			}
		}
	}
	
	/* #### 4) NULL TERMINATE THE INPUT STRING ARRAY #### */
	substr=xrealloc(substr, sizeof(char *)*(args_n+2));	
	substr[args_n+1]=NULL;

	/* If the command is internal, go on for more expansions; else, just
	 * return the input string array */
	if(!is_internal(substr[0]))
		return substr;

	/* ############## ONLY FOR INTERNAL COMMANDS ################## */

	/* Some functions of CliFM are purely internal, that is, they are not
	 * wrappers of a shell command and do not call the system shell at all.
	 * For this reason, some expansions normally made by the system shell
	 * must be made here (in the lobby [got it?]) in order to be able to 
	 * understand these expansions at all. These functions are properties,
	 * selection, and trash.
	 *  */

	int glob_array[int_array_max], braces_array[int_array_max], 
		glob_n=0, braces_n=0;

	for (i=0;substr[i];i++) {

		/* Do not perform any of the expansions below for selected elements: 
		 * they are full path filenames that, as such, do not need any 
		 * expansion */
		if (is_sel) { /* is_sel is true only for the current input and if
			there was some "sel" keyword in it */
			/* Strings between is_sel and sel_n are selected filenames */
			if (i >= is_sel && i <= sel_n)
				continue;
		}
		
		/* The search function admits a path as second argument. So, if the
		 * command is search, perform the expansions only for the first
		 * parameter, if any.
		 *  */
		if (substr[0][0] == '/' && i != 1)
			continue;

		/* 5) TILDE EXPANSION (replace "~/" by "/home/user") */
		if (strncmp(substr[i], "~/", 2) == 0) {
			/* tilde_expansion() is provided by the readline lib */
			char *exp_path=tilde_expand(substr[i]);
			if (exp_path) {
				substr[i]=xrealloc(substr[i], 
									   (strlen(exp_path)+1)*sizeof(char));
				strcpy(substr[i], exp_path);
			}
			free(exp_path);
		}

		for (size_t j=0;substr[i][j];j++) {
			
			/* If a brace is found, store its index */
			if (substr[i][j] == '{' && substr[i][j+1] != 0x00
			&& substr[i][j+1] != '}' && substr[i][j-1] != 0x00
			&& substr[i][j-1] != '\\') {
				for (size_t k=j+1;substr[i][k] && substr[i][k]!='}';k++) {
					if (substr[i][k] == ',' && substr[i][k-1] != '\\'
					&& braces_n < int_array_max) {
						braces_array[braces_n++]=i;
						break;
					}
				}
			}

			/* Check for glob chars */
			if (substr[i][j] == '*' || substr[i][j] == '?' 
			|| (substr[i][j] == '[' && substr[i][j+1] != 0x20))
			/* Strings containing these characters are taken as wildacard 
			 * patterns and are expanded by the glob function. See man (7) 
			 * glob */
				if (glob_n < int_array_max)
					glob_array[glob_n++]=i;
		}
	}

	/* #### 6) GLOB EXPANSION #### */
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
		int old_pathc=0;
		/* glob_array stores the index of the globbed strings. However, once 
		 * the first expansion is done, the index of the next globbed string 
		 * has changed. To recover the next globbed string, and more precisely, 
		 * its index, we only need to add the amount of files matched by the 
		 * previous instances of glob(). Example: if original indexes were 2 
		 * and 4, once 2 is expanded 4 stores now some of the files expanded 
		 * in 2. But if we add to 4 the amount of files expanded in 2 
		 * (gl_pathc), we get now the original globbed string pointed by 4.
		*/
		for (size_t g=0;g<glob_n;g++){
			glob_t globbuf;
			glob(substr[glob_array[g]+old_pathc], 0, NULL, &globbuf);
			if (globbuf.gl_pathc) {
				size_t j=0;
				char **glob_cmd=NULL;
				glob_cmd=xcalloc(glob_cmd, args_n+globbuf.gl_pathc+1, 
								 sizeof(char *));			
				for (i=0;i<(glob_array[g]+old_pathc);i++) {
					glob_cmd[j]=xcalloc(glob_cmd[j], strlen(substr[i])+1, 
										sizeof(char));
					strcpy(glob_cmd[j++], substr[i]);
				}
				for (i=0;i<globbuf.gl_pathc;i++) {
					/* Escape the globbed filename and copy it*/
					char *esc_str=escape_str(globbuf.gl_pathv[i]);
					if (esc_str) {
						glob_cmd[j]=xcalloc(glob_cmd[j], strlen(esc_str)+1,
										    sizeof(char));
						strcpy(glob_cmd[j++], esc_str);
						free(esc_str);
					}
					else {
						fprintf(stderr, _("%s: %s: Error quoting filename\n"), 
								PROGRAM_NAME, globbuf.gl_pathv[i]);
						size_t k=0;
						for (k=0;k<j;k++)
							free(glob_cmd[k]);
						free(glob_cmd);
						for (k=0;k<=args_n;k++)
							free(substr[k]);
						free(substr);
						return NULL;
					}
				}
				for (i=glob_array[g]+old_pathc+1;i<=args_n;i++) {
					glob_cmd[j]=xcalloc(glob_cmd[j], strlen(substr[i])+1, 
										sizeof(char));
					strcpy(glob_cmd[j++], substr[i]);
				}
				glob_cmd[j]=NULL;

				for (i=0;i<=args_n;i++) free(substr[i]);
				substr=xrealloc(substr, 
					(args_n+globbuf.gl_pathc+1)*sizeof(char *));
				for (i=0;i<j;i++) {
					substr[i]=xcalloc(substr[i], strlen(glob_cmd[i])+1, 
										  sizeof(char));
					strcpy(substr[i], glob_cmd[i]);
					free(glob_cmd[i]);
				}
				args_n=j-1;
				free(glob_cmd);
			}
			old_pathc+=(globbuf.gl_pathc-1);
			globfree(&globbuf);
		}
	}
	
	/* #### 7) BRACES EXPANSION #### */
	if (braces_n) { /* If there is some braced parameter... */
		/* We already know the indexes of braced strings (braces_array[]) */
		int old_braces_arg=0;
		for (size_t b=0;b<braces_n;b++) {
			/* Expand the braced parameter and store it into a new array */
			int braced_args=brace_expansion(substr[braces_array[b]
				+old_braces_arg]);
			/* Now we also know how many elements the expanded braced 
			 * parameter has */
			if (braced_args) {
				/* Create an array large enough to store parameters plus 
				 * expanded braces */
				char **comm_array_braces=NULL;
				comm_array_braces=xcalloc(comm_array_braces, 
										  args_n+braced_args, 
										  sizeof(char *));
				/* First, add to the new array the paramenters coming before 
				 * braces */
				for (i=0;i<(braces_array[b]+old_braces_arg);i++) {
					comm_array_braces[i]=xcalloc(comm_array_braces[i], 
												 strlen(substr[i])+1, 
												 sizeof(char));
					strcpy(comm_array_braces[i], substr[i]);
				}
				/* Now, add the expanded braces to the same array */
				for (size_t j=0;j<braced_args;j++) {
					/* Escape each filename and copy it */
					char *esc_str=escape_str(braces[j]);
					if (esc_str) {
						comm_array_braces[i]=xcalloc(comm_array_braces[i],
												strlen(esc_str)+1,
												sizeof(char));
						strcpy(comm_array_braces[i++], esc_str);
						free(esc_str);
					}
					else {
						fprintf(stderr, _("%s: %s: Error quoting filename\n"), 
								PROGRAM_NAME, braces[j]);
						size_t k=0;
						for (k=0;k<braces_n;k++)
							free(braces[k]);
						free(braces);
						for (k=0;k<i;k++)
							free(comm_array_braces[k]);
						free(comm_array_braces);
						for (k=0;k<args_n;k++)
							free(substr[k]);
						free(substr);
						return NULL;
					}
					free(braces[j]);
				}
				free(braces);
				/* Finally, add, if any, those parameters coming after the 
				 * braces */
				for (size_t j=braces_array[b]+old_braces_arg+1;j<=args_n;j++) {
					comm_array_braces[i]=xcalloc(comm_array_braces[i], 
												 strlen(substr[j])+1, 
												 sizeof(char));
					strcpy(comm_array_braces[i++], substr[j]);				
				}
				/* Now, free the old comm_array and copy to it our new array 
				 * containing all the parameters, including the expanded 
				 * braces */
				for (size_t j=0;j<=args_n;j++)
					free(substr[j]);
				substr=xrealloc(substr, 
					(args_n+braced_args+1)*sizeof(char *));			
				args_n=i-1;
				for (int j=0;j<i;j++) {
					substr[j]=xcalloc(substr[j], 
										  strlen(comm_array_braces[j])+1, 
										  sizeof(char));
					strcpy(substr[j], comm_array_braces[j]);
					free(comm_array_braces[j]);
				}
				old_braces_arg+=(braced_args-1);
				free(comm_array_braces);
			}
		}
	}

	/* #### 8) NULL TERMINATE THE INPUT STRING ARRAY (again) #### */
	substr=xrealloc(substr, (args_n+2)*sizeof(char *));	
	substr[args_n+1]=NULL;

	return substr;
}

char *
rl_no_hist(const char *prompt)
{
	stifle_history(0); /* Prevent readline from using the history setting */
	char *input=readline(prompt);
	unstifle_history(); /* Reenable history */
	read_history(HIST_FILE); /* Reload history lines from file */
	if (!input)
		return NULL;
	return input;
}

int
get_sel_files(void)
/* Get elements currently in the Selection Box, if any. */
{
	if (!selfile_ok)
		return EXIT_FAILURE;

	/* First, clear the sel array, in case it was already used */
	if (sel_n > 0)
		for (int i=0;i<sel_n;i++)
			free(sel_elements[i]);
/*	free(sel_elements); */
	sel_n=0;
	/* Open the tmp sel file and load its contents into the sel array */
	FILE *sel_fp=fopen(sel_file_user, "r");
/*	sel_elements=xcalloc(sel_elements, 1, sizeof(char *)); */
	if (sel_fp) {
		/* Since this file contains only paths, I can be sure no line length 
		 * will larger than PATH_MAX*/
		char sel_line[PATH_MAX]="";
		while (fgets(sel_line, sizeof(sel_line), sel_fp)) {
			size_t line_len=strlen(sel_line);
			sel_line[line_len-1]=0x00;
			sel_elements=xrealloc(sel_elements, (sel_n+1)*sizeof(char *));
			sel_elements[sel_n]=xcalloc(sel_elements[sel_n], line_len+1, 
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
/* Print the prompt and return the string entered by the user (to be parsed 
 * later by parse_input_str()) */
{
	/* I'm not sure why this works, but this line prevents the prompt 
	 * background color, if set, to become the background color of the
	 * command line text after running list_dir() */
	printf("\x1b[0m \r");

	/* Remove final slash(s) from path, if any */
	size_t path_len=strlen(path);
	for (size_t i=path_len-1;path[i] && i>0;i--) {
		if (path[i] != '/')
			break;
		else
			path[i] = 0x00;
	}
	
	if (welcome_message) {
		printf(_("%sCliFM, the anti-eye-candy, KISS file manager%s\n"), 
			   welcome_msg_color, NC);
		printf(_("%sType 'help' or '?' for instructions.%s\n"), 
			   default_color, NC);
		welcome_message=0;
	}
	/* Execute prompt commands, if any, only if external commands are 
	 * allowed */
	if (ext_cmd_ok && prompt_cmds_n > 0) 
		for (size_t i=0;i<(size_t)prompt_cmds_n;i++)
			launch_execle(prompt_cmds[i]);

	/* Update trash and sel file indicator on every prompt call */
	
	if (trash_ok) {
		trash_n=count_dir(TRASH_FILES_DIR);
		if (trash_n <= 2)
			trash_n=0;
	}
	
	get_sel_files();

	unsigned char max_prompt_path=40, home=0, path_too_long=0;
	char *input=NULL, *path_tilde=NULL, *short_path=NULL; /* cwd[PATH_MAX]=""; */
	args_n=0; /* Reset value */
	if (strncmp(path, user_home, user_home_len) == 0) {
		home=1;
		/* Reduce "/home/user/..." to "~/..." */
		if ((path_tilde=home_tilde(path)) == NULL) {
			path_tilde=xcalloc(path_tilde, 4, sizeof(char *));
			strncpy(path_tilde, "???\0", 4);
		}
	}
	if (strlen(path) > (size_t)max_prompt_path) {
		path_too_long=1;
		if ((short_path=straftlst(path, '/')) == NULL) {
			short_path=xcalloc(short_path, 4, sizeof(char));
			strcpy(short_path, "???");
		}
	}
	/* Unisgned char (0 to 255), max hostname 64 or 255, max username 32 */
	static unsigned char first_time=1, user_len=0, hostname_len=0; 
	/* Calculate the length of these variables just once. In Bash, even if 
	 * the user changes his/her user or hostname, the prompt will not be 
	 * updated til' the next login */
	if (first_time) {
		user_len=(unsigned char)strlen(user);
		hostname_len=(unsigned char)strlen(hostname);
		first_time=0;
	}
	
	/* Categorize messages in three groups: errors, warnings, and notices.
	 * The kind of message should be specified by the function printing
	 * the message itself via a global variable: error_msg, warning_msg, and
	 * notice_msg */
	char msg_str[20]="";
	if (msgs_n) {
		/* Errors take precedence over warnings, and warnings over notices */
		if (error_msg)
			sprintf(msg_str, "%sE", red_b);
		else if (warning_msg)
			sprintf(msg_str, "%sW", yellow_b);
		else if (notice_msg)
			sprintf(msg_str, "%sN", green_b);
		/* In case none of the above is set, which shouldn't happen, use
		 * the error indicator: Suppose the message is just a warning, but
		 * the warning flag has not been set. In this case, it's better to
		 * fail with an error than with a mere notice indicator */
		else
			sprintf(msg_str, "%sE", red_b);			
	}
	
	/* Compose the prompt string */
	/* unsigned short : 0 to 65535 (Max theoretical prompt length: PATH_MAX 
	 * (4096) + 32 (max user name) + 64 to 255 (max hostname) + 100 (colors) + 
	 * a few more bytes = 4500 bytes more or less )*/
	unsigned short prompt_length=(unsigned short)(((path_too_long) ? 
		strlen(short_path)+1 : (home) ? strlen(path_tilde)+1 : strlen(path)+1)
		+ sizeof(prompt_color) + 15 + 7 + ((sel_n) ? 19 : 0) 
		+ ((trash_n) ? 19 : 0) + ((msgs_n) ? 19: 0) + user_len
		+ hostname_len + 2 + sizeof(text_color) + 1 + 1);
	char shell_prompt[prompt_length];
	/* 15=length of NC_b
	 * 7=chars in the prompt: '[]', '@', '$' plus 3 spaces
	 * 19=length of green_b, yellow_b or red_b; 
	 * 1='*'; 1=null terminating char */
	memset(shell_prompt, 0x00, prompt_length);
	snprintf(shell_prompt, (size_t)prompt_length, 
		"%s%s%s%s%s%s[%s@%s] %s $%s%s ", 
		(msgs_n) ? msg_str : "", (trash_n) ? 
		yellow_b : "", (trash_n) ? "T" : "", (sel_n) ? green_b : "", (sel_n) ? 
		"*" : "", prompt_color, user, hostname, (path_too_long) ? short_path : 
		(home) ? path_tilde : path, NC_b, text_color);
	if (home) free(path_tilde);
	if (path_too_long) free(short_path);

	/* Print error messages, if any. 'print_errors' is set to true by 
	 * log_msg() with the PRINT_PROMPT flag. If NOPRINT_PROMPT is passed
	 * instead, 'print_msg' will be false and the message will be
	 * printed in place by log_msg() itself, without waiting for the next 
	 * prompt */
	if (print_msg) {
		for (size_t i=0;i<msgs_n;i++)
			fprintf(stderr, "%s", messages[i]);
		print_msg=0;
	}

	input=readline(shell_prompt);

	/* Enable commands history only if the input line is not void */
	if (input) {
		/* Do not record empty lines, exit, history commands, or 
		 * consecutively equal inputs */
		char no_space=0;
		size_t input_len=strlen(input);
		for (size_t i=0;i<input_len;i++)
			if (input[i] != 0x00 && input[i] != 0x20)
				no_space=1;
		if (strcmp(input, "q") != 0 && strcmp(input, "quit") != 0 
		&& strcmp(input, "exit") != 0 && strcmp(input, "zz") != 0
		&& strcmp(input, "salir") != 0 && strcmp(input, "chau") != 0
		&& input[0] != '!' && history && history[current_hist_n-1] 
		&& (strcmp(input, history[current_hist_n-1]) != 0)
		&& no_space) {
			add_history(input);
			if (config_ok)
				append_history(1, HIST_FILE);
			/* Add the new input to the history array */
			history=xrealloc(history, (current_hist_n+1)*sizeof(char *));
			history[current_hist_n]=xcalloc(history[current_hist_n], 
										    input_len+1, sizeof(char));
			strcpy(history[current_hist_n++], input);
		}
	}
	
	return input;
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

int
count_dir(const char *dir_path) /* Readdir version */
{
	struct stat file_attrib;
	if (lstat(dir_path, &file_attrib) == -1)
		return 0;
	int file_count=0;
	DIR *dir_p;
	struct dirent *entry;
	if ((dir_p=opendir(dir_path)) == NULL) {
		asprintf(&msg, "%s: opendir: '%s': %s\n", PROGRAM_NAME, 
				dir_path, strerror(errno));
		if (msg) {
			error_msg=1;
			log_msg(msg, PRINT_PROMPT);
			free(msg);
		}
		if (errno == ENOMEM) {
			fprintf(stderr, "%s: opendir: '%s': %s\n", PROGRAM_NAME, dir_path, 
					strerror(errno));
			exit(EXIT_FAILURE);
		}
		else
			return EXIT_FAILURE;
	}
	while ((entry=readdir(dir_p))) 
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
skip_implied_dot (const struct dirent *entry)
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
folder_select (const struct dirent *entry)
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
}

int
file_select (const struct dirent *entry)
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
}

void
colors_list(const char *entry, const int i, const int pad, 
			const int new_line)
/* Print ENTRY using color codes and I as ELN, right padding ENTRY PAD chars
 * and terminating ENTRY with or without a new line char (NEW_LINE 1 or 0
 * respectivelly) */
{
	int i_digits=digits_in_num(i);
	char index[i_digits+2]; /* Num (i) + space + null byte */
	memset(index, 0x00, i_digits+2);
	if (i > 0) /* When listing files in CWD */
		sprintf(index, "%d ", i);
	else if (i == -1) /* ELN for entry could not be found */
		sprintf(index, "? ");
	/* When listing files NOT in CWD (called from search_function() and first
	 * argument is a path: "/search_str /path") 'i' is zero. In this case, no 
	 * index should be shown at all */

	struct stat file_attrib;
	int ret=0;
	ret=lstat(entry, &file_attrib);
	if (ret == -1) {
		fprintf(stderr, "%s%-*s: %s%s", index, pad, entry, strerror(errno), 
			    new_line ? "\n" : "");
		return;
	}
	char *linkname=NULL;
	cap_t cap;
	switch (file_attrib.st_mode & S_IFMT) {
		case S_IFREG:
			if (!(file_attrib.st_mode & S_IRUSR))
				printf("%s%s%s%s%-*s%s%s", eln_color, index, NC, nf_c, pad, 
					   entry, NC, new_line ? "\n" : "");
			else if (file_attrib.st_mode & S_ISUID) /* set uid file */
				printf("%s%s%s%s%-*s%s%s", eln_color, index, NC, su_c, pad, 
					   entry, NC, new_line ? "\n" : "");
			else if (file_attrib.st_mode & S_ISGID) /* set gid file */
				printf("%s%s%s%s%-*s%s%s", eln_color, index, NC, sg_c, 
						pad, entry, NC, new_line ? "\n" : "");
			else {
				cap=cap_get_file(entry);
				if (cap) {
					printf("%s%s%s%s%-*s%s%s", eln_color, index, NC, ca_c, 
							pad, entry, NC, new_line ? "\n" : "");
					cap_free(cap);
				}
				else if (file_attrib.st_mode & S_IXUSR) {
					if (file_attrib.st_size == 0)
						printf("%s%s%s%s%-*s%s%s", eln_color, index, NC, ee_c, 
							   pad, entry, NC, new_line ? "\n" : "");
					else 
						printf("%s%s%s%s%-*s%s%s", eln_color, index, NC, ex_c, 
							   pad, entry, NC, new_line ? "\n" : "");
				}
				else if (file_attrib.st_size == 0)
					printf("%s%s%s%s%-*s%s%s", eln_color, index, NC, ef_c, 
						   pad, entry, NC, new_line ? "\n" : "");
				else
					printf("%s%s%s%s%-*s%s%s", eln_color, index, NC, fi_c, 
						   pad, entry, NC, new_line ? "\n" : "");
			}
		break;
		case S_IFDIR:
			if (access(entry, R_OK|X_OK) != 0)
				printf("%s%s%s%s%-*s%s%s", eln_color, index, NC, nd_c, pad, 
					   entry, NC, new_line ? "\n" : "");
			else {
				int is_oth_w=0;
				if (file_attrib.st_mode & S_IWOTH) is_oth_w=1;
				int files_dir=count_dir(entry);
				if (files_dir == 2 || files_dir == 0) { /* If folder is empty, it 
					contains only "." and ".." (2 elements). If not mounted 
					(ex: /media/usb) the result will be zero.*/
					/* If sticky bit dir: green bg. */
					printf("%s%s%s%s%-*s%s%s", eln_color, index, NC, 
						(file_attrib.st_mode & S_ISVTX) ? ((is_oth_w) ? 
						tw_c : st_c) : ((is_oth_w) ? 
						ow_c : ed_c), pad, entry, NC, 
						new_line ? "\n" : "");
				}
				else
					printf("%s%s%s%s%-*s%s%s", eln_color, index, NC,
						(file_attrib.st_mode & S_ISVTX) ? ((is_oth_w) ? 
						tw_c : st_c) : ((is_oth_w) ? 
						ow_c : di_c), pad, entry, NC, 
						new_line ? "\n" : "");
			}
		break;
		case S_IFLNK:
			linkname=realpath(entry, NULL);
			if (linkname) {
				printf("%s%s%s%s%-*s%s%s", eln_color, index, NC, ln_c, pad, 
					   entry, NC, new_line ? "\n" : "");
				free(linkname);
			}
			else printf("%s%s%s%s%-*s%s%s", eln_color, index, NC, or_c, pad, 
						entry, NC, new_line ? "\n" : "");
		break;
		case S_IFIFO: printf("%s%s%s%s%-*s%s%s", eln_color, index, NC, pi_c, 
							 pad, entry, NC, new_line ? "\n" : ""); break;
		case S_IFBLK: printf("%s%s%s%s%-*s%s%s", eln_color, index, NC, bd_c, 
							 pad, entry, NC, new_line ? "\n" : ""); break;
		case S_IFCHR: printf("%s%s%s%s%-*s%s%s", eln_color, index, NC, cd_c, 
							 pad, entry, NC, new_line ? "\n" : ""); break;
		case S_IFSOCK: printf("%s%s%s%s%-*s%s%s", eln_color, index, NC, so_c, 
							  pad, entry, NC, new_line ? "\n" : ""); break;
		/* In case all of the above conditions are false... */
		default: printf("%s%s%s%s%-*s%s%s", eln_color, index, NC, no_c, 
					    pad, entry, NC, new_line ? "\n" : "");
	}
}

int
list_dir(void)
/* List files in the current working directory */
/* To get a list of 'ls' colors run: 
$ dircolors --print-database */
{
/*	clock_t start=clock(); */

	/* This function lists files in the current working directory, that is, 
	 * the global variable 'path', that SHOULD be set before calling this
	 * function */
	if (!path || path[0] == 0x00) {
		fprintf(stderr, _("%s: Path variable is NULL or empty\n"), 
				PROGRAM_NAME);
		return EXIT_FAILURE;
	}
	
	files=0; /* Reset the files counter */
	int i=0;

	/* Remove final slash from path, if any */
	size_t strlen_path=strlen(path);
	if (path[strlen_path-1] == '/' && strcmp(path, "/") != 0)
		path[strlen_path-1]=0x00;
	
	/* If list directories first, first store directories, then files, and 
	 * finally copy everything into one single array (dirlist) */
	if (list_folders_first) {
		int files_files=0, files_folders=0;
		struct dirent **dirlist_folders, **dirlist_files;
		/* Store folders */
		files_folders=scandir(path, &dirlist_folders, folder_select, 
							  (unicode) ? alphasort : 
							  (case_sensitive) ? xalphasort : 
							  alphasort_insensitive);
		/* If unicode is set to true, use the standard alphasort(), since it
		 * uses strcoll(), which, unlike strcmp() (used by my xalphasort()), 
		 * is locale aware */
		if (files_folders == -1) {
			asprintf(&msg, "%s: scandir: '%s': %s\n", PROGRAM_NAME, 
					 path, strerror(errno));
			if (msg) {
				error_msg=1;
				log_msg(msg, PRINT_PROMPT);
				free(msg);
			}
			else
				fprintf(stderr, "%s: scandir: '%s': %s\n", PROGRAM_NAME, 
						path, strerror(errno));			
			if (errno == ENOMEM)
				exit(EXIT_FAILURE);
			else
				return EXIT_FAILURE;
		}
		/* Store files */
		files_files=scandir(path, &dirlist_files, file_select, 
  						    (unicode) ? alphasort : 
							(case_sensitive) ? xalphasort : 
							alphasort_insensitive);
		if (files_files == -1) {
			asprintf(&msg, "%s: scandir: '%s': %s\n", PROGRAM_NAME, 
					 path, strerror(errno));
			if (msg) {
				error_msg=1;
				log_msg(msg, PRINT_PROMPT);
				free(msg);
			}
			else
				fprintf(stderr, "%s: scandir: '%s': %s\n", PROGRAM_NAME, 
						path, strerror(errno));
			if (errno == ENOMEM)
				exit(EXIT_FAILURE);
			else
				return EXIT_FAILURE;
		}
		/* Reallocate the dirlist struct according to the amount of folders 
		* and files found */
		/* If empty folder... Note: realloc(ptr, 0) acts like free(ptr) */
		if (!files_folders && !files_files) /* If neither files nor folders */
			dirlist=xrealloc(dirlist, sizeof(char *));
		else if (files_folders > 0) {
			if (files_files > 0) { /* If files and folders */
				dirlist=xrealloc(dirlist, 
								 sizeof(char *)*
								 (files_folders+files_files));
			}
			else /* If only folders */
				dirlist=xrealloc(dirlist, 
								 sizeof(char *)*files_folders);
		}
		else if (files_files > 0) /* If only files */
			dirlist=xrealloc(dirlist, sizeof(char *)*files_files);
		
		/* Store both files and folders into the dirlist array */
		size_t str_len=0;
		if (files_folders > 0) {
			for(i=0;i<files_folders;i++) {
				str_len=(unicode) ? u8_xstrlen(dirlist_folders[i]->d_name)
						: strlen(dirlist_folders[i]->d_name);
				dirlist[files]=xcalloc(dirlist[files], str_len+1, 
									   sizeof(char));
				strcpy(dirlist[files++], dirlist_folders[i]->d_name);
				free(dirlist_folders[i]);
			}
			free(dirlist_folders);
		}
		if (files_files > 0) {
			for(i=0;i<files_files;i++) {
				str_len=(unicode) ? u8_xstrlen(dirlist_files[i]->d_name)
				: strlen(dirlist_files[i]->d_name);
				dirlist[files]=xcalloc(dirlist[files], str_len+1, 
									   sizeof(char));
				strcpy(dirlist[files++], dirlist_files[i]->d_name);
				free(dirlist_files[i]);
			}
			free(dirlist_files);
		}
	}
	
	/* If no list_folders_first */
	else {
		/* Completely free the dirlist array and copy the results of
		 * scandir into it */
		while (files--)
			free(dirlist[files]);
		free(dirlist);

		struct dirent **list=NULL;
		files=scandir(path, &list, skip_implied_dot, 
					  (unicode) ? alphasort : (case_sensitive) 
					  ? xalphasort : alphasort_insensitive);
		if (files == -1) {
			asprintf(&msg, "%s: scandir: '%s': %s\n", PROGRAM_NAME, 
					 path, strerror(errno));
			if (msg) {
				error_msg=1;
				log_msg(msg, PRINT_PROMPT);
				free(msg);
			}
			else
				fprintf(stderr, "%s: scandir: '%s': %s\n", PROGRAM_NAME, 
						path, strerror(errno));
			if (errno == ENOMEM)
				exit(EXIT_FAILURE);
			else
				return EXIT_FAILURE;
		} 
		else {
			dirlist=xcalloc(dirlist, files+1, sizeof(char *));
			size_t str_len=0;
			for (i=0;i<files;i++) {
				str_len=strlen(list[i]->d_name);
				dirlist[i]=xcalloc(dirlist[i], str_len+1, sizeof(char));
				strcpy(dirlist[i], list[i]->d_name);
				free(list[i]);
			}
			free(list);
		}
		
	}
	
	if (files == 0) {
		if (clear_screen)
			CLEAR;
		printf("%s. ..%s\n", blue, NC);
		return EXIT_SUCCESS;
	}

	int files_num=0;
	/* Get the longest element */
	longest=0; /* Global */
	struct stat file_attrib;
	for (i=files;i--;) {
		int ret=lstat(dirlist[i], &file_attrib);
		if (ret == -1)
			printf(_("Error: %s: %d: %s: %s\n"), path, i, dirlist[i], 
				   strerror(errno));
		/* file_name_width contains: ELN's number of digits + one space 
		 * between ELN and file name + filename length. Ex: '12 name' contains 
		 * 7 chars */
		int file_name_width=digits_in_num(i+1)+1+(
							(unicode) ? u8_xstrlen(dirlist[i]) 
							: strlen(dirlist[i]));
		/* If the file is a non-empty directory and the user has access 
		 * permision to it, add to file_name_width the number of digits of the 
		 * amount of files this directory contains (ex: 123 (files) contains 3 
		 * digits) + 2 for space and slash between the directory name and the 
		 * amount of files it contains. Ex: '12 name /45' contains 11 chars */
		if ((file_attrib.st_mode & S_IFMT) == S_IFDIR
		&& access(dirlist[i], R_OK|X_OK) == 0)
			if ((files_num=count_dir(dirlist[i])) > 2)
				file_name_width+=digits_in_num(files_num)+2;
		if (file_name_width > longest) {
			longest=file_name_width;
		}
	}

	/* Get terminal current amount of rows and columns */
	struct winsize w;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
	term_cols=w.ws_col; /* This one is global */
	short term_rows=w.ws_row;

	char reset_pager=0, c;

	/* Long view mode */
	if (long_view) {
		size_t counter=0;
		int max=get_max_long_view();
		for (i=0;i<files;i++) {
			if (pager) {
				/*	Check terminal amount of rows: if as many filenames as 
				 * the amount of available terminal rows has been printed, 
				 * stop */
				if (counter > (term_rows-2)) {
					switch(c=xgetchar()) {
						/* Advance one line at a time */
						case 66: /* Down arrow */
						break;
						case 10: /* Enter */
						break;
						case 32: /* Space */
						break;
						/* Advance one page at a time */
						case 126: counter=0; //Page Down
						break;
						/* Stop paging (and set a flag to reenable the pager 
						 * later) */
						case 99: pager=0, reset_pager=1; /* 'c' */
						break;
						case 112: pager=0, reset_pager=1; /* 'p' */
						break;
						case 113: pager=0, reset_pager=1; /* 'q' */
						break;
						/* If another key is pressed, go back one position. 
						 * Otherwise, some filenames won't be listed */
						default: i--; continue;
						break;
					}
				}
				counter++;
			}
			printf("%s%d%s ", eln_color, i+1, NC);
			get_properties(dirlist[i], (int)long_view, max);
		}
		if (reset_pager)
			pager=1;
		return EXIT_SUCCESS;
	}
	
	/* Normal view mode */
	
	/* Get possible amount of columns for the dirlist screen */
	int columns_n=term_cols/(longest+1); /* +1 for the space between file 
	names */
	/* If longest is bigger than terminal columns, columns_n will be 
	 * negative or zero. To avoid this: */
	if (columns_n < 1)
		columns_n=1;
	if (clear_screen)
		CLEAR;
	char last_column=0; /* c, reset_pager=0; */
	size_t counter=0;
	for (i=0;i<files;i++) {
		/* A basic pager for directories containing large amount of files
		* What's missing? It only goes downwards. To go backwards, use the 
		* terminal scrollback function */
		if (pager) {
			/* Run the pager only once all columns and rows fitting in the 
			 * screen are filled with the corresponding filenames */
			/*			Check rows					Check columns */
			if ((counter/columns_n)>(term_rows-2) && last_column) {
				switch(c=xgetchar()) {
					/* Advance one line at a time */
					case 66: /* Down arrow */
					break;
					case 10: /* Enter */
					break;
					case 32: /* Space */
					break;
					/* Advance one page at a time */
					case 126: counter=0; //Page Down
					break;
					/* Stop paging (and set a flag to reenable the pager 
					 * later) */
					case 99: pager=0, reset_pager=1; /* 'c' */
					break;
					case 112: pager=0, reset_pager=1; /* 'p' */
					break;
					case 113: pager=0, reset_pager=1; /* 'q' */
					break;
					/* If another key is pressed, go back one position. 
					 * Otherwise, some filenames won't be listed.*/
					default: i--; continue;
					break;
				}
			}
			counter++;
		}
		int is_dir=0, files_dir=0;
		char *linkname=NULL;
		cap_t cap;
		if ((i+1)%columns_n == 0)
			last_column=1;
		else
			last_column=0;
		lstat(dirlist[i], &file_attrib);
		switch (file_attrib.st_mode & S_IFMT) {
			case S_IFDIR:
				if (access(dirlist[i], R_OK|X_OK) != 0)
					printf("%s%d%s %s%s%s%s", eln_color, i+1, NC, nd_c, 
						   dirlist[i], NC, (last_column) ? "\n" : "");
				else {
					int is_oth_w=0;
					if (file_attrib.st_mode & S_IWOTH) is_oth_w=1;
					files_dir=count_dir(dirlist[i]);
					if (files_dir == 2 || files_dir == 0) { /* If folder is 
					* empty, it contains only "." and ".." (2 elements). If 
					* not mounted (ex: /media/usb) the result will be zero */
						/* If sticky bit dir: green bg */
						printf("%s%d%s %s%s%s%s", eln_color, i+1, NC,
							(file_attrib.st_mode & S_ISVTX) ? ((is_oth_w) ? 
							tw_c : st_c) : ((is_oth_w) 
							? ow_c : ed_c), dirlist[i], 
							NC, (last_column) ? "\n" : "");
					}
					else {
						printf("%s%d%s %s%s%s%s /%d%s%s", eln_color, i+1, NC,
							(file_attrib.st_mode & S_ISVTX) ? ((is_oth_w) ? 
							tw_c : st_c) : ((is_oth_w) 
							? ow_c : di_c), dirlist[i], 
							NC, dir_count_color, files_dir-2, NC, (last_column) 
							? "\n" : "");
						is_dir=1;
					}
				}
			break;
			case S_IFIFO:
				printf("%s%d%s %s%s%s%s", eln_color, i+1, NC, pi_c, 
						dirlist[i], NC, (last_column) ? "\n" : "");
			break;
			case S_IFLNK:
				linkname=realpath(dirlist[i], NULL);
				if (linkname) {
					printf("%s%d%s %s%s%s%s", eln_color, i+1, NC, ln_c, 
							dirlist[i], NC, (last_column) ? "\n" : "");
					free(linkname);
				}
				else
					printf("%s%d%s %s%s%s%s", eln_color, i+1, NC, or_c, 
							dirlist[i], NC, (last_column) ? "\n" : "");
			break;
			case S_IFBLK:
				printf("%s%d%s %s%s%s%s", eln_color, i+1, NC, bd_c, 
						dirlist[i], NC, (last_column) ? "\n" : "");
			break;
			case S_IFCHR:
				printf("%s%d%s %s%s%s%s", eln_color, i+1, NC, cd_c, 
						dirlist[i], NC, (last_column) ? "\n" : "");
			break;
			case S_IFSOCK:
				printf("%s%d%s %s%s%s%s", eln_color, i+1, NC, so_c, 
						dirlist[i], NC, (last_column) ? "\n" : "");
			break;
			case S_IFREG:
				if (!(file_attrib.st_mode & S_IRUSR))
					printf("%s%d%s %s%s%s%s", eln_color, i+1, NC, nf_c, 
							dirlist[i], NC, (last_column) ? "\n" : "");
				else if (file_attrib.st_mode & S_ISUID) /* set uid file */
					printf("%s%d%s %s%s%s%s", eln_color, i+1, NC, su_c, 
							dirlist[i], NC, (last_column) ? "\n" : "");
				else if (file_attrib.st_mode & S_ISGID) /* set gid file */
					printf("%s%d%s %s%s%s%s", eln_color, i+1, NC, sg_c, 
						dirlist[i], NC, (last_column) ? "\n" : "");
				else if ((cap=cap_get_file(dirlist[i]))) {
					printf("%s%d%s %s%s%s%s", eln_color, i+1, NC, ca_c, 
							dirlist[i], NC, (last_column) ? "\n" : "");				
					cap_free(cap);
				}
				else if (file_attrib.st_mode & S_IXUSR)
					if (file_attrib.st_size == 0)
						printf("%s%d%s %s%s%s%s", eln_color, i+1, NC, ee_c, 
								dirlist[i], NC, (last_column) 
								? "\n" : "");
					else
						printf("%s%d%s %s%s%s%s", eln_color, i+1, NC, ex_c, 
								dirlist[i], NC, (last_column) 
								? "\n" : "");
				else if (file_attrib.st_size == 0)
					printf("%s%d%s %s%s%s%s", eln_color, i+1, NC, ef_c, 
							dirlist[i], NC, (last_column) ? "\n" : "");
				else
					printf("%s%d%s %s%s%s%s", eln_color, i+1, NC, fi_c, 
							dirlist[i], NC, (last_column) ? "\n" : "");
			break;
			/* In case all of the above conditions are false, we have an
			 * unknown file type */
			default: 
				printf("%s%d%s %s%s%s%s", eln_color, i+1, NC, no_c, 
						dirlist[i], NC, (last_column) ? "\n" : "");
		}
		if (!last_column) {
			/* Get the difference between the length of longest and the 
			 * current element */
			int diff=longest-(digits_in_num(i+1)+1+
							  ((unicode) ? u8_xstrlen(dirlist[i]) 
							  : xstrlen(dirlist[i])));
			if (is_dir) { /* If a directory, make room for displaying the 
				* amount of files it contains */
				/* Get the amount of digits of files_dir */
				int dig_num=digits_in_num(files_dir-2);
				/* The amount of digits plus 2 chars for " /" */
				diff=diff-(dig_num+2);
			}
			/* Print the spaces needded to equate the length of the lines */
			for (int j=diff+1;j;j--) /* +1 is for the space between file names */
				printf(" ");
		}
	}
	/* If the pager was disabled during listing (by pressing 'c', 'p' or 'q'),
	reenable it */
	if (reset_pager)
		pager=1;

	/* If the last listed element was modulo (in the last column), don't print 
	 * anything, since it already has a new line char at the end. Otherwise, 
	 * if not modulo (not in the last column), print a new line, for it has 
	 * none */
	printf("%s", (last_column) ? "" : "\n");

	/* Print a dividing line between the files list and the prompt */
	for (i=term_cols;i--;)
		printf("%s=", div_line_color);
	printf("%s%s", NC, default_color);
	fflush(stdout);

/*	clock_t end=clock();
	printf("list_dir time: %f\n", (double)(end-start)/CLOCKS_PER_SEC);
	getchar(); */
	return EXIT_SUCCESS;
}

void
get_aliases_n_prompt_cmds(void)
{
	if (!config_ok)
		return;

	FILE *config_file_fp;
	config_file_fp=fopen(CONFIG_FILE, "r");
	if (!config_file_fp) {
		asprintf(&msg, "%s: alias: '%s': %s\n", PROGRAM_NAME, 
				 CONFIG_FILE, strerror(errno));
		if (msg) {
			error_msg=1;
			log_msg(msg, PRINT_PROMPT);
			free(msg);
		}
		else
			fprintf(stderr, "%s: alias: '%s': %s\n", PROGRAM_NAME, 
					CONFIG_FILE, strerror(errno));
		return;
	}

	int prompt_line_found=0;
	char *line=NULL;
	size_t line_size=0;
	ssize_t line_len=0;
	while ((line_len=getline(&line, &line_size, config_file_fp)) > 0) {
		if (strncmp(line, "alias", 5) == 0) {
			char *alias_line=straft(line, ' ');	
			if (alias_line) {
				aliases=xrealloc(aliases, (aliases_n+1)*sizeof(char *));
				aliases[aliases_n]=xcalloc(aliases[aliases_n], 
										   strlen(alias_line)+1, sizeof(char));
				strcpy(aliases[aliases_n++], alias_line);
				free(alias_line);
			}
		}
		else if (prompt_line_found) {
			if (strncmp(line, "#END OF PROMPT", 14) == 0) break;
			if (strncmp(line, "#", 1) != 0) {
				prompt_cmds=xrealloc(prompt_cmds, 
					(prompt_cmds_n+1)*sizeof(char *));
				prompt_cmds[prompt_cmds_n]=xcalloc(prompt_cmds[prompt_cmds_n], 
												   strlen(line)+1, 
												   sizeof(char));
				strcpy(prompt_cmds[prompt_cmds_n++], line);
			}
		}
		else if (strncmp(line, "#PROMPT", 7) == 0) 
			prompt_line_found=1;
	}
	free(line);
	line=NULL;
	fclose(config_file_fp);
}

char **
check_for_alias(char **comm)
/* If a matching alias is found, execute the corresponding command, if it 
 * exists. Returns one if matching alias is found, zero if not. */
{
	char *aliased_cmd=NULL;
	size_t comm_len=strlen(comm[0]);
	char comm_tmp[comm_len+2];
	memset(comm_tmp, 0x00, comm_len+2);
	/* Look for this string: "command=", in the aliases file */
	snprintf(comm_tmp, sizeof(comm_tmp), "%s=", comm[0]);
	for (size_t i=0;i<aliases_n;i++) {
		if (strncmp(aliases[i], comm_tmp, comm_len+1) == 0) {
			/* Get the aliased command */
			aliased_cmd=strbtw(aliases[i], '\'', '\'');
			if (!aliased_cmd) return NULL;
			if (aliased_cmd[0] == 0x00) { /* zero length */
				free(aliased_cmd);
				return NULL;
			}
			char *command_tmp=NULL;
			command_tmp=xcalloc(command_tmp, PATH_MAX+1, sizeof(char));
			strncpy(command_tmp, aliased_cmd, PATH_MAX); 
			free(aliased_cmd);
			args_n=0; /* Reset args_n to be used by parse_input_str() */
			/* Parse the aliased cmd */
			char **alias_comm=parse_input_str(command_tmp);
			free(command_tmp);
			/* Add input parameters, if any, to alias_comm */
			for (size_t j=1;comm[j];j++) {
				alias_comm=xrealloc(alias_comm, (++args_n+1)*sizeof(char *));
				alias_comm[args_n]=xcalloc(alias_comm[args_n], 
										   strlen(comm[j])+1, sizeof(char));
				strcpy(alias_comm[args_n], comm[j]);
			}
			/* Add a final null */
			alias_comm=xrealloc(alias_comm, (args_n+2)*sizeof(char *));
			alias_comm[args_n+1]=NULL;
			/* Free comm */
			for (size_t j=0;comm[j];j++) 
				free(comm[j]);
			free(comm);			
			return alias_comm;
		}
	}
	return NULL;
}

int
exec_cmd(char **comm)
/* Take the command entered by the user, already splitted into substrings by
 * parse_input_str(), and call the corresponding function. Return zero in 
 * case of success and one in case of error */
{

	printf("%s", default_color);

	/* Exit flag. exit_code is zero (sucess) by default. In case of error
	 * in any of the functions below, it will be turned into one (failure).
	 * This is the value returned at the end of this function */
	int exit_code=0;

	/* If a user defined variable */
	if (flags & IS_USRVAR_DEF) {
		flags &= ~IS_USRVAR_DEF;
		if (create_usr_var(comm[0]) != 0)
			exit_code=1;
		return exit_code;
	}

	/* If double semi colon or colon (or ";:" or ":;") */
	if (comm[0][0] == ';' || comm[0][0] == ':') {
		if (comm[0][1] == ';' || comm[0][1] == ':') {
			fprintf(stderr, _("%s: '%.2s': Syntax error\n"), PROGRAM_NAME, 
					comm[0]);
			return EXIT_FAILURE;
		}
	}

	/* The more often a function is used, the more on top should it be in this 
	 * if...else..if chain. It will be found faster this way. */

	if (strcmp(comm[0], "cd") == 0) {
		if (strcmp(comm[1], "--help") == 0)
			puts(_("Usage: cd [ELN/DIR]"));
		else
			exit_code=cd_function(comm[1]);
	}
	else if (strcmp(comm[0], "o") == 0	|| strcmp(comm[0], "open") == 0) {
		if (!comm[1] || strcmp(comm[1], "--help") == 0) {
			puts(_("Usage: open ELN/FILE [APPLICATION]"));
		}
		else
			exit_code=open_function(comm);
	}
	else if (strcmp(comm[0], "rf") == 0 || strcmp(comm[0], "refresh") == 0) {
		if (cd_lists_on_the_fly) {
			while (files--) free(dirlist[files]);
			if (list_dir() != 0)
				exit_code=1;
		}
	}
	else if (strcmp(comm[0], "bm") == 0 
	|| strcmp(comm[0], "bookmarks") == 0) {
		if (comm[1] && strcmp(comm[1], "--help") == 0) {
			puts(_("Usage: bookmarks, bm"));
			return EXIT_SUCCESS;
		}
		/* Disable keyboard shortcuts. Otherwise, the function will still
		 * be waiting for input while the screen have been taken by 
		 * another function */
		kbind_busy=1;
		/* Disable TAB completion while in Bookmarks */
		rl_attempted_completion_function=NULL;
		exit_code=bookmarks_function(comm);
		/* Reenable TAB completion */
		rl_attempted_completion_function=my_rl_completion;
		/* Reenable keyboard shortcuts */
		kbind_busy=0;
	}
	else if (strcmp(comm[0], "b") == 0 || strcmp(comm[0], "back") == 0)
		exit_code=back_function(comm);
	else if (strcmp(comm[0], "f") == 0 || strcmp(comm[0], "forth") == 0)
		exit_code=forth_function(comm);
	else if (strcmp(comm[0], "bh") == 0 || strcmp(comm[0], "fh") == 0) {
		for (int i=0;i<dirhist_total_index;i++) {
			if (i == dirhist_cur_index)
				printf("%d %s%s%s%s\n", i+1, green, old_pwd[i], NC, default_color);
			else 
				printf("%d %s\n", i+1, old_pwd[i]);	
		}
	}
	else if (strcmp(comm[0], "cp") == 0 || strcmp(comm[0], "paste") == 0 
	|| strcmp(comm[0], "mv") == 0 || strcmp(comm[0], "c") == 0 
	|| strcmp(comm[0], "m") == 0 || strcmp(comm[0], "v") == 0) {
		if (strcmp(comm[0], "c") == 0) {
			comm[0]=xrealloc(comm[0], 3*sizeof(char *));
			strncpy(comm[0], "cp", 3);
		}
		else if (strcmp(comm[0], "m") == 0) {
			comm[0]=xrealloc(comm[0], 3*sizeof(char *));
			strncpy(comm[0], "mv", 3);
		}
		else if (strcmp(comm[0], "v") == 0) {
			comm[0]=xrealloc(comm[0], 6*sizeof(char *));
			strncpy(comm[0], "paste", 6);
		}
		kbind_busy=1;
		exit_code=copy_function(comm);
		kbind_busy=0;
	}
	else if (strcmp(comm[0], "tr") == 0 || strcmp(comm[0], "t") == 0 
	|| strcmp(comm[0], "trash") == 0) {
		if (comm[1] && strcmp(comm[1], "--help") == 0) {
			puts(_("Usage: trash, tr, t ELN/FILE ... n "
				 "[ls, list] [clear] [del, rm]"));
			return EXIT_SUCCESS;
		}
		if (trash_function(comm) != 0)
			exit_code=1;
		if (is_sel) { /* If 'tr sel', deselect everything */
			for (size_t i=0;i<sel_n;i++)
				free(sel_elements[i]);
			sel_n=0;
			if (save_sel() != 0)
				exit_code=1;
		}
	}
	else if (strcmp(comm[0], "undel") == 0 || strcmp(comm[0], "u") == 0
	|| strcmp(comm[0], "untrash") == 0) {
		if (comm[1] && strcmp(comm[1], "--help") == 0) {
			puts(_("Usage: undel, untrash, u"));
			return EXIT_SUCCESS;
		}
		kbind_busy=1;
		rl_attempted_completion_function=NULL;
		exit_code=untrash_function(comm);
		rl_attempted_completion_function=my_rl_completion;
		kbind_busy=0;
	}
	else if (strcmp(comm[0], "s") == 0 || strcmp(comm[0], "sel") == 0) {
		if (!comm[1] || (comm[1] && strcmp(comm[1], "--help") == 0)) {
			puts(_("Usage: sel, s [ELN ELN-ELN FILE ... n]"));
			return EXIT_SUCCESS;
		}
		exit_code=sel_function(comm);
	}
	else if (strcmp(comm[0], "sb") == 0 || strcmp(comm[0], "selbox") == 0)
		show_sel_files();
	else if (strcmp(comm[0], "ds") == 0 || strcmp(comm[0], "desel") == 0) {
		if (comm[1] && strcmp(comm[1], "--help") == 0) {
			puts(_("Usage: desel, ds"));
			return EXIT_SUCCESS;
		}
		kbind_busy=1;
		rl_attempted_completion_function=NULL;
		exit_code=deselect(comm);
		rl_attempted_completion_function=my_rl_completion;
		kbind_busy=0;
	}
	else if (strcmp(comm[0], "rm") == 0 || strcmp(comm[0], "mkdir") == 0 
	|| strcmp(comm[0], "touch") == 0 || strcmp(comm[0], "ln") == 0 
	|| strcmp(comm[0], "unlink") == 0 || strcmp(comm[0], "r") == 0
	|| strcmp(comm[0], "l") == 0 || strcmp(comm[0], "md") == 0) {
		if (strcmp(comm[0], "l") == 0) {
			comm[0]=xrealloc(comm[0], 3*sizeof(char *));
			strncpy(comm[0], "ln", 3);
		}
		else if (strcmp(comm[0], "r") == 0) {
			comm[0]=xrealloc(comm[0], 3*sizeof(char *));
			strncpy(comm[0], "rm", 3);
		}
		else if (strcmp(comm[0], "md") == 0) {
			comm[0]=xrealloc(comm[0], 6*sizeof(char *));
			strncpy(comm[0], "mkdir", 6);
		}
		kbind_busy=1;
		exit_code=run_and_refresh(comm);
		kbind_busy=0;
	}
	else if (strcmp(comm[0], "pr") == 0 || strcmp(comm[0], "prop") == 0 
	|| strcmp(comm[0], "p") == 0) {
		if (!comm[1] || strcmp(comm[1], "--help") == 0) {
			puts(_("Usage: pr [ELN/FILE ... n] [a, all] [s, size]"));
			return EXIT_SUCCESS;
		}
		exit_code=properties_function(comm);
	}
	else if (comm[0][0] == '/' && access(comm[0], F_OK) != 0)
								/* If not absolute path */ 
		exit_code=search_function(comm);
	/* History function: if '!number' or '!-number' or '!!' */
	else if (comm[0][0] == '!' && comm[0][1] != 0x20
	&& comm[0][1] != 0x00)
		exit_code=run_history_cmd(comm[0]+1);
	else if (strcmp(comm[0], "ls") == 0 && !cd_lists_on_the_fly) {
		while (files--)
			free(dirlist[files]);
		if (list_dir() != 0)
			exit_code=1;
		if (get_sel_files() != 0)
			exit_code=1;
	}
	else if (strcmp(comm[0], "pf") == 0 || strcmp(comm[0], "prof") == 0 
	|| strcmp(comm[0], "profile") == 0) {
		if (comm[1] && strcmp(comm[1], "--help") == 0)
			puts(_("Usage: profile, prof, pf"));
		else if (!alt_profile)
			printf("%s: profile: default\n", PROGRAM_NAME);
		else
			printf("%s: profile: '%s'\n", PROGRAM_NAME, alt_profile);
	}
	else if (strcmp(comm[0], "mp") == 0 
	|| (strcmp(comm[0], "mountpoints") == 0)) {
		if (comm[1] && strcmp(comm[1], "--help") == 0)
			puts(_("Usage: mountpoints, mp"));
		else {
			kbind_busy=1;
			rl_attempted_completion_function=NULL;
			exit_code=list_mountpoints();
			rl_attempted_completion_function=my_rl_completion;
			kbind_busy=0;
		}
	}
	else if (strcmp(comm[0], "ext") == 0) {
		if (!comm[1] || strcmp(comm[1], "--help") == 0) 
			puts(_("Usage: ext [on, off, status]"));					
		else {
			if (strcmp(comm[1], "status") == 0)
				printf(_("%s: External commands %s\n"), PROGRAM_NAME, 
					   (ext_cmd_ok) ? _("enabled") : _("disabled"));
			else if (strcmp(comm[1], "on") == 0) {
				ext_cmd_ok=1;
				printf(_("%s: External commands enabled\n"), PROGRAM_NAME);
			}
			else if (strcmp(comm[1], "off") == 0) {
				ext_cmd_ok=0;
				printf(_("%s: External commands disabled\n"), PROGRAM_NAME);
			}
			else
				fputs(_("Usage: ext [on, off, status]"), stderr);
		}
	}
	else if (strcmp(comm[0], "pg") == 0 || strcmp(comm[0], "pager") == 0) {
		if (!comm[1] || strcmp(comm[1], "--help") == 0) 
			puts(_("Usage: pager, pg [on, off, status]"));
		else {
			if (strcmp(comm[1], "status") == 0)
				printf(_("%s: Pager %s\n"), PROGRAM_NAME, 
					   (pager) ? _("enabled") : _("disabled"));
			else if (strcmp(comm[1], "on") == 0) {
				pager=1;
				printf(_("%s: Pager enabled\n"), PROGRAM_NAME);
			}
			else if (strcmp(comm[1], "off") == 0) {
				pager=0;
				printf(_("%s: Pager disabled\n"), PROGRAM_NAME);
			}
			else
				fputs(_("Usage: pager, pg [on, off, status]"), stderr);
		}
	}
	else if (strcmp(comm[0], "uc") == 0 || strcmp(comm[0], "unicode") == 0) {
		if (!comm[1] || strcmp(comm[1], "--help") == 0)
			puts(_("Usage: unicode, uc [on, off, status]"));
		else {
			if (strcmp(comm[1], "status") == 0)
				printf(_("%s: Unicode %s\n"), PROGRAM_NAME, 
					   (unicode) ? _("enabled") : _("disabled"));
			else if (strcmp(comm[1], "on") == 0) {
				unicode=1;
				printf(_("%s: Unicode enabled\n"), PROGRAM_NAME);
			}
			else if (strcmp(comm[1], "off") == 0) {
				unicode=0;
				printf(_("%s: Unicode disabled\n"), PROGRAM_NAME);
			}
			else
				fputs(_("Usage: unicode, uc [on, off, status]"), stderr);
		}
	}
	else if ((strcmp(comm[0], "folders") == 0 
	&& strcmp(comm[1], "first") == 0) || strcmp(comm[0], "ff") == 0) {
		if (cd_lists_on_the_fly == 0) 
			return EXIT_SUCCESS;
		int n=0;
		if (strcmp(comm[0], "ff") == 0)
			n=1;
		else
			n=2;
		if (comm[n]) {
			if (strcmp(comm[n], "--help") == 0) {
				puts(_("Usage: folders first, ff [on, off, status]"));
				return EXIT_SUCCESS;
			}
			int status=list_folders_first;
			if (strcmp(comm[n], "status") == 0)
				printf(_("%s: Folders first %s\n"), PROGRAM_NAME, 
					   (list_folders_first) ? _("enabled") : _("disabled"));
			else if (strcmp(comm[n], "on") == 0)
				list_folders_first=1;
			else if (strcmp(comm[n], "off") == 0)
				list_folders_first=0;
			else {
				fputs(_("Usage: folders first, ff [on, off, status]\n"), stderr);
				return EXIT_SUCCESS;			
			}
			if (list_folders_first != status) {
				if (cd_lists_on_the_fly) {
					while (files--) free(dirlist[files]);
					if (list_dir() != 0)
						exit_code=1;
				}
			}
		}
		else {
			fputs(_("Usage: folders first, ff [on, off, status]\n"), stderr);
		}
	}
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
			return EXIT_FAILURE;
		}
		exit_code=log_function(comm);
	}
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
			for (size_t i=0;i<msgs_n;i++)
				free(messages[i]);
			msgs_n=0;
			error_msg = warning_msg = notice_msg = 0;
		}
		else {
			if (msgs_n) {
				for (size_t i=0;i<msgs_n;i++)
					printf("%s", messages[i]);
			}
			else
				printf(_("%s: There are no messages\n"), PROGRAM_NAME);
		}
	}
	else if (strcmp(comm[0], "alias") == 0) {
		if (comm[1] && strcmp(comm[1], "--help") == 0) {
			puts(_("Usage: alias"));
			return EXIT_SUCCESS;
		}
		if (aliases_n)
			for (size_t i=0;i<(size_t)aliases_n;i++)
				printf("%s", aliases[i]);	
	}
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
			exit_code=set_shell(comm[1]);
	}
	else if (strcmp(comm[0], "edit") == 0) 
		exit_code=edit_function(comm);
	else if (strcmp(comm[0], "history") == 0) 
		exit_code=history_function(comm);
	else if (strcmp(comm[0], "hf") == 0 
	|| strcmp(comm[0], "hidden") == 0) {
		if (!comm[1] || strcmp(comm[1], "--help") == 0) {
			/* The same message is in hidden_function(), and printed
			 * whenever an invalid argument is entered */
			puts(_("Usage: hidden, hf [on, off, status]")); 
			return EXIT_SUCCESS;
		}
		else
			exit_code=hidden_function(comm);
	}

	else if (strcmp(comm[0], "path") == 0 || strcmp(comm[0], "cwd") == 0) 
		printf("%s\n", path);
	else if (strcmp(comm[0], "help") == 0 || strcmp(comm[0], "?") == 0)
		help_function();
	else if (strcmp(comm[0], "cmd") == 0 || strcmp(comm[0], "commands") == 0) 
		list_commands();
	else if (strcmp(comm[0], "colors") == 0) color_codes();
	else if (strcmp(comm[0], "ver") == 0 || strcmp(comm[0], "version") == 0)
		version_function();
	else if (strcmp(comm[0], "license") == 0)
		print_license();
	else if (strcmp(comm[0], "fs") == 0)
		free_sotware();
	else if (strcmp(comm[0], "bonus") == 0)
		bonus_function();
	else if (strcmp(comm[0], "splash") == 0) splash();
	else if (strcmp(comm[0], "q") == 0 || strcmp(comm[0], "quit") == 0 
	|| strcmp(comm[0], "exit") == 0 || strcmp(comm[0], "zz") == 0 
	|| strcmp(comm[0], "salir") == 0 || strcmp(comm[0], "chau") == 0) {
		/* #####free everything##### */
		for (size_t i=0;i<=(size_t)args_n;i++)
			free(comm[i]);
		free(comm);
		exit(EXIT_SUCCESS);
	}
	
	/* #### EXTERNAL COMMANDS #### */
	else {
		/* IF NOT A COMMAND, BUT A DIRECTORY... */
		if (comm[0][0] == '/') {
			struct stat file_attrib;
			if (lstat(comm[0], &file_attrib) == 0) {
				if ((file_attrib.st_mode & S_IFMT) == S_IFDIR ) {
					fprintf(stderr, _("%s: '%s': Is a directory\n"), 
							PROGRAM_NAME, comm[0]);
					return EXIT_FAILURE;
				}
			}
		}

		/* LOG EXTERNAL COMMANDS 
		* 'no_log' will be true when running profile or prompt commands */
		if (!no_log)
			exit_code=log_function(comm);
		
		/* PREVENT UNGRACEFUL EXIT */
		/* Prevent the user from killing the program via the 'kill', 'pkill' 
		 * or 'killall' commands, from within CliFM itself. Otherwise, the 
		 * program will be forcefully terminated without freeing malloced 
		 * memory */
		if (strncmp(comm[0], "kill", 3) == 0 
		|| strncmp(comm[0], "killall", 6) == 0 
		|| strncmp(comm[0], "pkill", 5) == 0) {
			for (int i=1;i<=args_n;i++) {
				if ((strcmp(comm[0], "kill") == 0 
				&& atoi(comm[i]) == get_own_pid()) 
				|| ((strcmp(comm[0], "killall") == 0 
				|| strcmp(comm[0], "pkill") == 0) 
				&& strcmp(comm[i], program_invocation_short_name) == 0)) {
					fprintf(stderr, _("%s: To gracefully quit type 'quit'\n"), 
							PROGRAM_NAME);
					return exit_code;
				}
			}
		}
		
		/* CHECK WHETHER EXTERNAL COMMANDS ARE ALLOWED */
		if (!ext_cmd_ok) {
			fprintf(stderr, _("%s: External commands are not allowed. \
Run 'ext on' to enable them.\n"), 
					PROGRAM_NAME);
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
			char *comm_tmp=comm[0]+1;
			/* If string == ":" or ";" */
			if (!comm_tmp || comm_tmp[0] == 0x00) {
				fprintf(stderr, _("%s: '%c': Syntax error\n"), PROGRAM_NAME, 
						comm[0][0]);
				return EXIT_FAILURE;
			}
			else
				strcpy(comm[0], comm_tmp);
		}
		
		/* #### RUN THE EXTERNAL COMMAND #### */
		
		/* Store the command and each argument into a single array to be 
		 * executed by execle() using the system shell (/bin/sh -c) */
		char *ext_cmd=NULL;
		size_t ext_cmd_len=strlen(comm[0]);
		ext_cmd=xcalloc(ext_cmd, ext_cmd_len+1, sizeof(char));
		strcpy(ext_cmd, comm[0]);
		if (args_n) { /* This will be false in case of ";cmd" or ":cmd" */
			for (size_t i=1;i<=(size_t)args_n;i++) {
				ext_cmd_len+=strlen(comm[i])+1;
				ext_cmd=xrealloc(ext_cmd, (ext_cmd_len+1)*sizeof(char));
				strcat(ext_cmd, " ");
				strcat(ext_cmd, comm[i]);
			}
		}
		exit_code=launch_execle(ext_cmd);
		free(ext_cmd);

 		/* Reload the list of available commands in PATH for TAB completion.
 		 * Why? If this list is not updated, whenever some new program is 
 		 * installed or simply a new executable is added to some of the paths 
 		 * in PATH while in CliFM, this latter needs to be restarted in order 
 		 * to be able to recognize the new program for TAB completion */
 		size_t i=0;
		if (bin_commands) {
			for (i=0;bin_commands[i];i++)
				free(bin_commands[i]);
			free(bin_commands);
		}

		if (paths)
			for (i=0;i<(size_t)path_n;i++)
				free(paths[i]);

		path_n=get_path_env();
		get_path_programs();
	}

	return exit_code;
}

void
surf_hist(char **comm)
{
	if (strcmp(comm[1], "h") == 0 || strcmp(comm[1], "hist") == 0) {
		/* Show the list of already visited directories */
		for (int i=0;i<dirhist_total_index;i++) {
			if (i == dirhist_cur_index)
				printf("%d %s%s%s%s\n", i+1, green, old_pwd[i], NC, default_color);
			else
				printf("%d %s\n", i+1, old_pwd[i]);
		}
	}
	else if (strcmp(comm[1], "clear") == 0) {
		for (int i=0;i<dirhist_total_index;i++)
			free(old_pwd[i]);
		dirhist_cur_index=dirhist_total_index=0;
		add_to_dirhist(path);
	}
	else if (comm[1][0] == '!' && is_number(comm[1]+1) == 1) {
		/* Go the the specified directory (first arg) in the directory history 
		 * list */
		int atoi_comm=atoi(comm[1]+1);
		if (atoi_comm > 0 && atoi_comm <= dirhist_total_index) {
			int ret=chdir(old_pwd[atoi_comm-1]);
			if (ret == 0) {
				free(path);
				path=xcalloc(path, strlen(old_pwd[atoi_comm-1])+1, 
							 sizeof(char));
				strcpy(path, old_pwd[atoi_comm-1]);
/*				add_to_dirhist(path); */
				dirhist_cur_index=atoi_comm-1;
				if (cd_lists_on_the_fly) {
					while (files--) free(dirlist[files]);
						list_dir();
				}
			}
			else
				fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME,
						old_pwd[atoi_comm-1], strerror(errno));
		}
		else 
			fprintf(stderr, _("history: '%d': No such ELN\n"), 
					atoi(comm[1]+1));
	}
	else 
		fputs(_("history: Usage: b/f [hist] [clear] [!ELN]\n"), stderr);
	return;
}

int
launch_execle (const char *cmd)
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
	signal(SIGCHLD, SIG_DFL);

	int status;
	/* Create a new process via fork() */
	pid_t pid=fork();
	if (pid < 0) {
		fprintf(stderr, "%s: fork: %s\n", PROGRAM_NAME, strerror(errno));
		return EXFORKERR;
	}
	/* Run the command via execle */
	else if (pid == 0) {
		/* Reenable signals only for the child, in case they were disabled for
		* the parent */
		signal(SIGHUP, SIG_DFL);
		signal(SIGINT, SIG_DFL);
		signal(SIGQUIT, SIG_DFL);
		signal(SIGTERM, SIG_DFL);
		/* This is basically what the system() function does: */
		int i=0, index=0;
		/* Get shell filename */
		size_t shell_len=strlen(sys_shell);
		for (i=shell_len-1;i>=0;i--) {
			if (sys_shell[i] == '/') {
				index=i;
				break;
			}
		}
		if (!index)
			execle(sys_shell, sys_shell, "-c", cmd, NULL, __environ);
		else
			execle(sys_shell, sys_shell+index, "-c", cmd, NULL, __environ);
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
launch_execve(char **cmd)
/* Execute a command and return the corresponding exit status. The exit status
* could be: zero, if everything went fine, or a non-zero value in case of 
* error. The function takes as only arguement an array of strings containing 
* the command name to be executed and its arguments (cmd) */
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

	#ifndef _GNU_SOURCE
		#define _GNU_SOURCE /* Needed by execvpe() */
	#endif
	
	if (!cmd)
		return EXNULLERR;

	/* Reenable SIGCHLD, in case it was disabled. Otherwise, waitpid won't be 
	 * able to catch error codes coming from the child. */
	signal(SIGCHLD, SIG_DFL);

	/* Check if program is to be backgrounded. In that case, remove the
	 * final ampersand from the string */
	char is_bg=0;

	/* Get last argument's index */
	size_t last=0;
	for (last=0;cmd[last];last++);
	last--;
	
	if (cmd[last]) {
		if (strcmp(cmd[last], "&") == 0) {
			free(cmd[last]);
			cmd[last]=NULL;
			is_bg=1;
		}
		else {
			size_t last_len=strlen(cmd[last]);
			if (cmd[last][last_len-1] == '&') {
				cmd[last][last_len-1]=0x00;
				is_bg=1;
			}
		}
	}
	/* Create a new process via fork() */
	pid_t pid=fork();
	if (pid < 0) {
		fprintf(stderr, "%s: fork: %s\n", PROGRAM_NAME, strerror(errno));
		return errno;
	}
	/* Run the command via execvpe */
	else if (pid == 0) {
		/* Reenable signals only for the child, in case they were disabled for
		the parent */
		signal(SIGHUP, SIG_DFL);
		signal(SIGINT, SIG_DFL);
		signal(SIGQUIT, SIG_DFL);
		signal(SIGTERM, SIG_DFL);
		execvpe(cmd[0], cmd, __environ);
		/* This will only be reached is execvpe() fails, because the exec
		 * family of functions only returns on error */
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, cmd[0], 
				strerror(errno));
		exit(errno);
	}
	/* Get the command status */
	else { /* pid > 0 */
		if (is_bg) {
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
run_in_foreground(pid_t pid)
{
	int status=0;
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
	int status=0;
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

	FILE *sel_fp=fopen(sel_file_user, "w");
	if (!sel_fp) {
		asprintf(&msg, "%s: sel: '%s': %s\n", PROGRAM_NAME, 
				 sel_file_user, strerror(errno));
		if (msg) {
			log_msg(msg, NOPRINT_PROMPT);
			free(msg);
		}
		else
			fprintf(stderr, "%s: sel: '%s': %s\n", PROGRAM_NAME, 
					sel_file_user, strerror(errno));
		return EXIT_FAILURE;
	}
	
	for (int i=0;i<sel_n;i++) {
		fprintf(sel_fp, "%s", sel_elements[i]);
		fprintf(sel_fp, "\n");
	}
	fclose(sel_fp);

	return EXIT_SUCCESS;
}

int
sel_function(char **comm)
{
	if (!comm)
		return EXIT_FAILURE;

	char *sel_tmp=NULL;
	int i=0, j=0, exists=0, new_sel=0, exit_code=0;

	for (i=1;i<=args_n;i++) {
		char *deq_file=dequote_str(comm[i], 0);
		if (!deq_file) {
			for (j=0;j<sel_n;j++)
				free(sel_elements[j]);
			return EXIT_FAILURE;
		}
		size_t file_len=strlen(deq_file);
		/* Remove final slash from directories. No need to check if file is
		 * a directory, since in Linux only directories can contain a
		 * slash in its name */
		if (deq_file[file_len-1] == '/')
			deq_file[file_len-1]=0x00;
		if (strcmp(deq_file, ".") == 0 || strcmp(deq_file, "..") == 0) {
			free(deq_file);
			continue;
		}
		/* If a filename in CWD... */
		int sel_is_filename=0, sel_is_relative_path=0;
		for (j=0;j<files;j++) {
			if (strcmp(dirlist[j], deq_file) == 0) {
				sel_is_filename=1;
				break;
			}
		}
		if (!sel_is_filename) {
			/* If a path (contains a slash)... */
			if (strcntchr(deq_file, '/') != -1) {
				if (deq_file[0] != '/') /* If relative path */
					sel_is_relative_path=1;
				struct stat file_attrib;
				if (stat(deq_file, &file_attrib) != 0) {
					fprintf(stderr, "%s: sel: '%s': %s\n", PROGRAM_NAME, 
							deq_file, strerror(errno));
					free(deq_file);
					/* Return error when at least one error occur */
					exit_code=1;
					continue;
				}
			}
			else { /* If neither a filename in CWD nor a path... */
				fprintf(stderr, _("%s: sel: '%s': No such %s\n"), 
						PROGRAM_NAME, deq_file, 
						(is_number(deq_file)) ? "ELN" : "file or directory");
				free(deq_file);
				exit_code=1;
				continue;
			}
		}
		if (sel_is_filename || sel_is_relative_path) { 
			/* Add path to filename or relative path */
			sel_tmp=xcalloc(sel_tmp, strlen(path)+strlen(deq_file)+2, 
							sizeof(char));
			sprintf(sel_tmp, "%s/%s", path, deq_file);
		}
		else { /* If absolute path... */
			sel_tmp=xcalloc(sel_tmp, strlen(deq_file)+1, sizeof(char));		
			strcpy(sel_tmp, deq_file);
		}
		free(deq_file);
		/* Check if the selected element is already in the selection 
		 * box */
		exists=0; 
		for (j=0;j<sel_n;j++) {
			if (strcmp(sel_elements[j], sel_tmp) == 0) {
				exists=1;
				break;
			}
		}
		if (!exists) {
			sel_elements=xrealloc(sel_elements, (sel_n+1)*sizeof(char *));
			sel_elements[sel_n]=xcalloc(sel_elements[sel_n], 
										strlen(sel_tmp)+1, sizeof(char));
			strcpy(sel_elements[sel_n++], sel_tmp);
			new_sel++;
		}
		else fprintf(stderr, _("%s: sel: '%s': Already selected\n"), 
					 PROGRAM_NAME, sel_tmp);
		free(sel_tmp);
		continue;
	}

	if (!selfile_ok)
		return EXIT_FAILURE;
	
	if (!new_sel)
		return exit_code;

	/* At this point, we know there are new selected files and that the
	 * selection file is OK. So, write new selections into the selection 
	 * file*/
	if (save_sel() == 0) { /* If selected files were successfully written to
		sel file */
		if (sel_n > 10)
			printf(_("%d elements are now in the Selection Box\n"), sel_n);
		else if (sel_n > 0) {
			printf(_("%d selected %s:\n"), sel_n, (sel_n == 1) ? _("element") : 
				_("elements"));
			for (i=0;i<sel_n;i++)
				printf("  %s\n", sel_elements[i]);
		}
		return exit_code;
	}
	else {
		if (sel_n > 0) { /* In case of error, remove sel files from memory */
			for (i=0;i<sel_n;i++)
				free(sel_elements[i]);
			sel_n=0;
		}
//		save_sel();
		return EXIT_FAILURE;
	}
}

void
show_sel_files(void)
{
	if (clear_screen)
		CLEAR;
	printf(_("%sSelection Box%s%s\n"), white, NC, default_color);
	char reset_pager=0;
	if (sel_n == 0)
		puts(_("Empty"));
	else {
		puts("");
		struct winsize w;
		ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
		char c, counter=0;
		short term_rows=w.ws_row;
		term_rows-=2;
		for(int i=0;i<sel_n;i++) {
/*			if (pager && counter > (term_rows-2)) { */
			if (pager && counter > term_rows) {
				switch(c=xgetchar()) {
					/* Advance one line at a time */
					case 66: /* Down arrow */
					break;
					case 10: /* Enter */
					break;
					case 32: /* Space */
					break;
					/* Advance one page at a time */
					case 126: counter=0; /* Page Down */
					break;
					/* Stop paging (and set a flag to reenable the pager 
					 * later) */
					case 99: pager=0, reset_pager=1; /* 'c' */
					break;
					case 112: pager=0, reset_pager=1; /* 'p' */
					break;
					case 113: pager=0, reset_pager=1; /* 'q' */
					break;
					/* If another key is pressed, go back one position. 
					 * Otherwise, some filenames won't be listed.*/
					default: i--; continue;
					break;				
				}
			}
			counter++;
			colors_list(sel_elements[i], i+1, 0, 1);
		}
	}
	if (reset_pager)
		pager=1;
}

int
deselect (char **comm)
{
	if (!comm)
		return EXIT_FAILURE;

	if (comm[1] && (strcmp(comm[1], "*") == 0 || strcmp(comm[1], "a") == 0 
	|| strcmp(comm[1], "all") == 0)) {
		if (sel_n > 0) {
			for (int i=0;i<sel_n;i++)
				free(sel_elements[i]);
			sel_n=0;
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
	
	int i; 
	if (clear_screen)
		CLEAR;
	printf(_("%sSelection Box%s%s\n"), white, NC, default_color);
	if (sel_n == 0) {
		puts(_("Empty"));
		return EXIT_SUCCESS;
	}
	puts("");
	for (i=0;i<sel_n;i++)
		colors_list(sel_elements[i], i+1, 0, 1);

	printf(_("\n%s%sEnter 'q' to quit.\n"), NC, default_color);
	int no_space=0, desel_n=0;
	char *line=NULL, **desel_elements=NULL;
	while (!line) {
		line=rl_no_hist(_("Element(s) to be deselected (ex: 1 2-6, or *): "));
		if (!line)
			continue;
		for (i=0;line[i];i++)
			if (line[i] != 0x20)
				no_space=1;
		if (line[0] == 0x00 || !no_space) {
			free(line);
			line=NULL;
		}
	}
	desel_elements=get_substr(line, ' ');
	free(line);
	if (!desel_elements)
		return EXIT_FAILURE;

	for (i=0;desel_elements[i];i++)
		desel_n++;

	for (i=0;i<desel_n;i++) { /* Validation */
		/* If not a number */
		if (!is_number(desel_elements[i])) {
			if (strcmp(desel_elements[i], "q") == 0) {
				for (i=0;i<desel_n;i++)
					free(desel_elements[i]);
				free(desel_elements);
				return EXIT_SUCCESS;
			}
			else if (strcmp(desel_elements[i], "*") == 0) {
				/* Clear the sel array */
				for (i=0;i<sel_n;i++)
					free(sel_elements[i]);
				sel_n=0;
				for (i=0;i<desel_n;i++)
					free(desel_elements[i]);
				int exit_code=0;
				if (save_sel() != 0)
					exit_code=1;
				free(desel_elements);
				if (cd_lists_on_the_fly) {
					while (files--) free(dirlist[files]);
					if (list_dir() != 0)
						exit_code=1;
				}
				return exit_code;
			}
			else {
				printf(_("desel: '%s': Invalid element\n"), desel_elements[i]);
				for (size_t j=0;j<desel_n;j++)
					free(desel_elements[j]);
				free(desel_elements);
				return EXIT_FAILURE;
			}
		}
		/* If a number, check it's a valid ELN */
		else {
			int atoi_desel=atoi(desel_elements[i]);
			if (atoi_desel == 0 || atoi_desel > sel_n) {
				printf(_("desel: '%s': Invalid ELN\n"), desel_elements[i]);
				for (size_t j=0;j<desel_n;j++)
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
	char **desel_path=NULL;
	desel_path=xcalloc(desel_path, desel_n, sizeof(char **));
	for (i=0;i<desel_n;i++) {
		int desel_int=atoi(desel_elements[i]);
		desel_path[i]=xcalloc(desel_path[i], 
							  strlen(sel_elements[desel_int-1])+1, 
							  sizeof(char));
		strcpy(desel_path[i], sel_elements[desel_int-1]);
	}
	/* Search the sel array for the path of the element to deselect and store 
	its index */
	for (i=0;i<desel_n;i++) {
		int desel_index=0;
		for (int k=0;k<sel_n;k++) {
			if (strcmp(sel_elements[k], desel_path[i]) == 0) {
				desel_index=k;
				break;
			}
		}
		/* Once the index was found, rearrange the sel array removing the 
		 * deselected element (actually, moving each string after it to the 
		 * previous position) */
		for (int j=desel_index;j<sel_n-1;j++) {
			sel_elements[j]=xrealloc(sel_elements[j], 
									 (strlen(sel_elements[j+1])+1)
									 *sizeof(char));
			strcpy(sel_elements[j], sel_elements[j+1]);
		}
	}
	/* Free the last DESEL_N elements from the old sel array. They won't be 
	 * used anymore, for they contain the same value as the last non-deselected 
	 * element due to the above array rearrangement */
	for (i=1;i<=desel_n;i++) {
		if ((sel_n-i) >= 0 && sel_elements[sel_n-i])
			free(sel_elements[sel_n-i]);
	}
	/* Reallocate the sel array according to the new size */
	sel_n=sel_n-desel_n;
	if (sel_n < 0)
		sel_n=0;
	if (sel_n)
		sel_elements=xrealloc(sel_elements, sel_n*sizeof(char *));

	/* Deallocate local arrays */
	for (i=0;i<desel_n;i++) {
		free(desel_path[i]);
		free(desel_elements[i]);
	}
	free(desel_path);
	free(desel_elements);
	if (args_n > 0) {
		for (i=1;i<=args_n;i++)
			free(comm[i]);
		comm=xrealloc(comm, 1*sizeof(char *));
		args_n=0;
	}

	int exit_code=0;
	if (save_sel() != 0)
		exit_code=1;
	
	/* If there is still some selected file, reload the sel screen */
	if (sel_n)
		if (deselect(comm) != 0)
			exit_code=1;

	return exit_code;
}

int
run_and_refresh(char **comm)
/* Run a command via execle() and refresh the screen in case of success */
{
	if (!comm)
		return EXIT_FAILURE;

	log_function(comm);

	size_t i=0, total_len=0;
	for (i=0;i<=args_n;i++)
		total_len+=strlen(comm[i]);
		
	char *tmp_cmd=NULL;
	tmp_cmd=xcalloc(tmp_cmd, total_len+(i+1)+1, sizeof(char));
	for (i=0;i<=args_n;i++) {
		strcat(tmp_cmd, comm[i]);
		strcat(tmp_cmd, " ");
	}
	
	int ret=launch_execle(tmp_cmd);
	free(tmp_cmd);
	if (ret == 0) {
		/* If 'rm sel' and command is successful, deselect everything */
		if (is_sel && strcmp(comm[0], "rm") == 0) {
			for (i=0;i<sel_n;i++)
				free(sel_elements[i]);
			sel_n=0;
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
{
	if (!comm || !comm[0])
		return EXIT_FAILURE;

	/* If search string (comm[0]) is "/search", comm[0]+1 returns "search" */
	char *search_str=comm[0]+1, *deq_dir=NULL;
	
	/* If wildcards, use glob() */
	if (strcntchr(search_str, '*') != -1 
	|| strcntchr(search_str, '?') != -1 
	|| strcntchr(search_str, '[') != -1) {

		/* If second argument ("/search_str /path"), chdir into it, since 
		 * glob() works on CWD */
		if (comm[1] && comm[1][0] != 0x00) {
			deq_dir=dequote_str(comm[1], 0);
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
		int ret=glob(search_str, 0, NULL, &globbed_files);
		if (ret == 0) {
			if (deq_dir)
				for (size_t i=0;globbed_files.gl_pathv[i];i++)
					colors_list(globbed_files.gl_pathv[i], 0, 0, 1);
					/* Second argument to colors_list() is:
					 * 0: Do not print any ELN 
					 * Positive number: Print positive number as ELN
					 * -1: Print "?" instead of an ELN */
			else {
				size_t index=0;
				for (size_t i=0;globbed_files.gl_pathv[i];i++) {
					/* In case 'index' is not found in the next for loop, that 
					 * is, if the globbed file is not found in the current dir 
					 * list, 'index' value would be that of the previous file 
					 * if 'index' is not set to zero in each for iteration */
					index=0;
					for (size_t j=0;j<(size_t)files;j++)
						if (strcmp(globbed_files.gl_pathv[i], 
								   dirlist[j]) == 0)
							index=j;
					colors_list(globbed_files.gl_pathv[i], 
								(index) ? index+1 : -1, 0, 1);
				}
			}
		}
		else 
			printf(_("%s: No matches found\n"), PROGRAM_NAME);
		globfree(&globbed_files);

		/* Go back to the directory we came from */
		if (deq_dir) {
			if (chdir(path) == -1) {
				fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, path, 
						strerror(errno));
				return EXIT_FAILURE;
			}
			return EXIT_SUCCESS;
		}				
	}
	
	/* If no wildcards */
	else {
		char found=0;
		/* If /search_str /path */
		if (comm[1] && comm[1][0] != 0x00) {
			deq_dir=dequote_str(comm[1], 0);
			if (!deq_dir) {
				fprintf(stderr, _("%s: %s: Error dequoting filename\n"), 
						PROGRAM_NAME, comm[1]);
				return EXIT_FAILURE;
			}
			struct dirent **search_list;
			int search_files=0;
			search_files=scandir(deq_dir, &search_list, NULL, alphasort);
			if (search_files == -1) {
				fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, deq_dir, 
						strerror(errno));
				free(deq_dir);
				return EXIT_FAILURE;
			}
			if (chdir(deq_dir) == -1) {
				fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, deq_dir, 
						strerror(errno));
				for (size_t i=0;i<search_files;i++)
					free(search_list[i]);
				free(search_list);
				free(deq_dir);
				return EXIT_FAILURE;
			}
			for (size_t i=0;i<search_files;i++) {
				if (strstr(search_list[i]->d_name, search_str)) {
					colors_list(search_list[i]->d_name, 0, 0, 1);
					found=1;
				}
				free(search_list[i]);
			}
			free(search_list);
			if (chdir(path) == -1) {
				fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, path, 
						strerror(errno));
				free(deq_dir);
				return EXIT_FAILURE;
			}
		}
		
		/* If /search_str */
		else {
			for (size_t i=0;i<(size_t)files;i++) {
				/* strstr finds substr in STR, as if STR where "*substr*" */
				if (strstr(dirlist[i], search_str)) {
					colors_list(dirlist[i], i+1, 0, 1);
					found=1;
				}
			}
		}

		if (!found)
			printf(_("%s: No matches found\n"), PROGRAM_NAME);
	}
	
	if (deq_dir)
		free(deq_dir);
	
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
		char *tmp_cmd=NULL;
		size_t total_len=0, i=0;
		for (i=0;i<=args_n;i++) {
			total_len+=strlen(comm[i]);
		}
		tmp_cmd=xcalloc(tmp_cmd, total_len+(i+1)+2, sizeof(char));
		for (i=0;i<=args_n;i++) {			
			strcat(tmp_cmd, comm[i]);
			strcat(tmp_cmd, " ");
		}
		if (sel_is_last)
			strcat(tmp_cmd, ".");
		
		int ret=0;
		ret=launch_execle(tmp_cmd);
		free(tmp_cmd);
		
		if (ret == 0) {
			/* If 'mv sel' and command is successful deselect everything,
			 * since sel files are note there anymore */
			if (strcmp(comm[0], "mv") == 0) {
				for (i=0;i<sel_n;i++)
					free(sel_elements[i]);
				sel_n=0;
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
	bm_n=-1; /* This global variable stores the total amount of bookmarks */
	char **bookmarks=NULL;
	FILE *bm_fp;
	bm_fp=fopen(bookmarks_file, "r");
	if (!bm_fp) {
		asprintf(&msg, "%s: bookmarks: %s: %s\n", PROGRAM_NAME, 
				 bookmarks_file, strerror(errno));
		if (msg) {
			log_msg(msg, NOPRINT_PROMPT);
			free(msg);
		}
		else
			fprintf(stderr, "%s: bookmarks: %s: %s\n", PROGRAM_NAME, 
					bookmarks_file, strerror(errno));
		return NULL;
	}

	bm_n=0; /* bm_n is only zero if no file error. Otherwise, it is -1 */

	/* Get bookmarks from the bookmarks file */

	char *line=NULL;
	size_t line_size=0;
	ssize_t line_len=0;
	while ((line_len=getline(&line, &line_size, bm_fp)) > 0) {
		/* If a comment or empty line */
		if (*line == '#' || *line == '\n')
			continue;
		/* Since bookmarks are shortcuts to paths, there must be at least
		 * one slash */
		int slash=0;
		for (size_t i=0;i<line_len;i++) {
			if (line[i] == '/') {
				slash=1;
				break;
			}
		}
		if (!slash)
			continue;
		/* Allocate memory to store the bookmark */
		bookmarks=xrealloc(bookmarks, (bm_n+1)*sizeof(char *));
		bookmarks[bm_n]=xcalloc(bookmarks[bm_n], line_len+1, 
								sizeof(char));
		/* Remove terminating new line char and copy the entire line */
		if (line[line_len-1] == '\n')
			line[line_len-1] = 0x00;
		strcpy(bookmarks[bm_n], line);
		bm_n++;
	}
	free(line);
	line=NULL;
	fclose(bm_fp);

	return bookmarks;
}

char **
bm_prompt(void)
{
	char *bm_sel=NULL;
	printf("%s%s\nEnter 'e' to edit your bookmarks or 'q' to quit.\n", NC_b, 
		   default_color);
	while (!bm_sel) {
		bm_sel=rl_no_hist(_("Choose a bookmark: "));
		int no_space=0;
		for (size_t i=0;bm_sel[i];i++)
			if (bm_sel[i] != 0x20)
				no_space=1;
		/* If empty or only spaces */
		if (bm_sel[0] == 0x00 || !no_space) {
			free(bm_sel);
			bm_sel=NULL;
		}
	}
	char **comm_bm=get_substr(bm_sel, ' ');
	free(bm_sel);
	return comm_bm;	
}

int
del_bookmark(void)
{
	FILE *bm_fp=NULL;
	bm_fp=fopen(BM_FILE, "r");
	if (!bm_fp)
		return EXIT_FAILURE;
	
	size_t i=0;	
	
	/* Get bookmarks from file */
	size_t line_size=0;
	char *line=NULL, **bms=NULL;
	int bmn=0;
	ssize_t line_len=0;
	while ((line_len=getline(&line, &line_size, bm_fp)) > 0) {
		if (*line == '#' || *line == '\n')
			continue;
		int slash=0;
		for (i=0;i<line_len;i++) {
			if (line[i] == '/') {
				slash=1;
				break;
			}
		}
		if (!slash)
			continue;
		if (line[line_len-1] == '\n')
			line[line_len-1] = 0x00;
		bms=xrealloc(bms, (bmn+1)*sizeof(char *));
		bms[bmn]=xcalloc(bms[bmn], line_len+1, sizeof(char));
		strcpy(bms[bmn++], line);
	}
	free(line);

	if (!bmn) {
		puts(_("bookmarks: There are no bookmarks"));
		for (i=0;i<bmn;i++)
			free(bms[i]);
		free(bms);
		fclose(bm_fp);
		return EXIT_SUCCESS;
	}
	
	/* List bookmarks */
	printf("\033[1;37mBookmarks\033[0m\n\n");
	for (i=0;i<bmn;i++)
		printf("%s%ld \033[1;36m%s\033[0m\n", eln_color, i+1, bms[i]);

	/* Get user input */
	printf(_("\n%s%sEnter 'q' to quit.\n"), NC, default_color);
	char *input=NULL;
	while (!input) {
		input=rl_no_hist("Bookmark(s) to be deleted (ex: 1 2-6, or *): ");
		if (input) {
			int no_space=0;
			for (i=0;input[i];i++) {
				if (input[i] != 0x20)
					no_space=1;
			}
			if (input[0] == 0x00 || !no_space) {
				free(input);
				input=NULL;
			}
		}
	}

	char **del_elements=get_substr(input, ' ');
	free(input);
	if (!del_elements) {
		for (i=0;i<bmn;i++)
			free(bms[i]);
		free(bms);
		fclose(bm_fp);
		fprintf(stderr, _("bookmarks: Error parsing input\n"));
		return EXIT_FAILURE;
	}

	/* We have input */
	/* If quit */
	/* I inspect all substrings entered by the user for "q" before any
	 * other value to prevent some accidental deletion, like "1 q", or 
	 * worst, "* q" */
	for (i=0;del_elements[i];i++) {
		int quit=0;
		if (strcmp(del_elements[i], "q") == 0)
			quit=1;
		else if (is_number(del_elements[i]) 
		&& (atoi(del_elements[i]) <= 0 || atoi(del_elements[i]) > bmn)) {
			fprintf(stderr, "bookmarks: '%s': No such bookmark\n", 
					del_elements[i]);
			quit=1;
		}
		if (quit) {
			for (i=0;i<bmn;i++)
				free(bms[i]);
			free(bms);
			for (i=0;del_elements[i];i++)
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
	for (i=0;del_elements[i];i++) {
		if (strcmp(del_elements[i], "*") == 0) {
			/* Create a backup copy of the bookmarks file, just in case */
			char *bk_file=NULL;
			bk_file=xcalloc(bk_file, strlen(CONFIG_DIR)+14, sizeof(char));
			sprintf(bk_file, "%s/bookmarks.bk", CONFIG_DIR);
			char *tmp_cmd[]={ "cp", BM_FILE, bk_file, NULL };
			int ret=launch_execve(tmp_cmd);
			/* Remove the bookmarks file, free stuff, and exit */
			if (ret == 0) {
				remove(BM_FILE);
				printf(_("bookmarks: All bookmarks were deleted\n "
						 "However, a backup copy was created (%s)\n"),
						 bk_file);
				free(bk_file);
			}
			else
				printf(_("bookmarks: Error creating backup file. No "
						 "bookmark was deleted\n"));
			for (i=0;i<bmn;i++)
				free(bms[i]);
			free(bms);
			for (i=0;del_elements[i];i++)
				free(del_elements[i]);
			free(del_elements);
			fclose(bm_fp);
			return EXIT_SUCCESS;
		}
	}
	
	/* Remove single bookmarks */
	/* Open a temporary file */
	char *tmp_file=NULL;
	tmp_file=xcalloc(tmp_file, strlen(CONFIG_DIR)+8, sizeof(char));
	sprintf(tmp_file, "%s/bm_tmp", CONFIG_DIR);

	FILE *tmp_fp=fopen(tmp_file, "w+");
	if (!tmp_fp) {
		for (i=0;i<bmn;i++)
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
	
	line_len = line_size = 0;
	char *lineb=NULL;
	while ((line_len=getline(&lineb, &line_size, bm_fp)) > 0) {
		if (lineb[line_len-1] == '\n')
			lineb[line_len-1] = 0x00;
		int found=0;
		for (size_t j=0;del_elements[j];j++) {
			if (!is_number(del_elements[j]))
				continue;
			if (strcmp(bms[atoi(del_elements[j])-1], lineb) == 0)
				found=1;
		}
		if (found)
			continue;
		fprintf(tmp_fp, "%s\n", lineb);
	}
	free(lineb);

	/* Free stuff */
	for (i=0;del_elements[i];i++)
		free(del_elements[i]);
	free(del_elements);

	for (i=0;i<bmn;i++)
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

	return EXIT_SUCCESS;
}

int
add_bookmark(char *file)
{
	if (!file)
		return EXIT_FAILURE;

	int mod_file=0;
	/* If not absolute path, prepend current path to file */
	if (*file != '/') {
		char *tmp_file=NULL;
		tmp_file=xcalloc(tmp_file, (strlen(path) + strlen(file) + 2), 
						 sizeof(char));
		sprintf(tmp_file, "%s/%s", path, file);
		file=tmp_file;
		tmp_file=NULL;
		mod_file=1;
	}

	/* Check if FILE is an available path */
	
	FILE *bm_fp=fopen(BM_FILE, "r");	
	if (!bm_fp) {
		fprintf(stderr, _("bookmarks: Error opening the bookmarks file\n"));
		if (mod_file)
			free(file);
		return EXIT_FAILURE;
	}
	int bmn=0, dup=0;
	char **bms=NULL;
	size_t line_size=0;
	char *line=NULL;
	ssize_t line_len=0;
	while ((line_len=getline(&line, &line_size, bm_fp)) > 0) {
		if (*line == '#')
			continue;
		
		char *tmp_line=NULL;
		tmp_line=strchr(line, '/');
		if (tmp_line) {
			size_t tmp_line_len=strlen(tmp_line);
			if (tmp_line_len && tmp_line[tmp_line_len-1] == '\n')
				tmp_line[tmp_line_len-1] = 0x00;
			if (strcmp(tmp_line, file) == 0) {
				fprintf(stderr, _("bookmarks: '%s': Path already "
								  "bookmarked\n"), file);
				dup=1;
				break;
			}
			tmp_line=NULL;
		}
		/* Store lines: used later to check hotkeys */		
		bms=xrealloc(bms, (bmn+1)*sizeof(char *));
		bms[bmn]=xcalloc(bms[bmn], strlen(line)+1, sizeof(char));
		strcpy(bms[bmn++], line);
		
	}
	free(line);
	fclose(bm_fp);

	if (dup) {
		for (size_t i=0;i<bmn;i++)
			free(bms[i]);
		free(bms);
		return EXIT_FAILURE;
	}
	
	/* If path is available */	

	char *name=NULL, *hk=NULL, *tmp=NULL;

	/* Ask for data to construct the bookmark line. Both values could be
	 * NULL */
	puts("Bookmark line example: [sc]name:path");
	hk=rl_no_hist("Shortcut: ");

	/* Check if hotkey is available */
	if (hk) {
		char *tmp_line=NULL;
		for (size_t i=0;i<bmn;i++) {
			tmp_line=strbtw(bms[i], '[', ']');
			if (tmp_line) {
				if (strcmp(hk, tmp_line) == 0) {
					fprintf(stderr, _("bookmarks: '%s': This shortcut is "
									  "already assigned\n"), hk);					
					dup=1;
				}
				free(tmp_line);
				tmp_line=NULL;
			}
		}
	}
	if (dup) {
		if (hk)
			free(hk);
		return EXIT_FAILURE;
	}
	
	for (size_t i=0;i<bmn;i++)
		free(bms[i]);
	free(bms);

	/* Both path and hotkey are available */

	/* There is no need to check names for duplicates. The bookmarks
	 * function only uses ELN's and hotkeys to open bookmarked files */
	name=rl_no_hist("Name: ");

	/* Generate the bookmark line */
	if (name) {
		if (hk) { /* name AND hk */
			tmp=xcalloc(tmp, strlen(hk) + strlen(name) + strlen(file) + 5, 
						sizeof(char));
			sprintf(tmp, "[%s]%s:%s\n", hk, name, file);
			free(hk);
		}
		else { /* Only name */
			tmp=xcalloc(tmp, strlen(name) + strlen(file) + 3, sizeof(char));
			sprintf(tmp, "%s:%s\n", name, file);
		}
		free(name);
	}
	else if (hk) { /* Only hk */
		tmp=xcalloc(tmp, strlen(hk) + strlen(file) + 4, 
					sizeof(char));
		sprintf(tmp, "[%s]%s\n", hk, file);
		free(hk);
	}
	else /* None */
		tmp=file;
	
	if (!tmp) {
		fprintf(stderr, "bookmarks: Error generating the bookmark line\n");
		return EXIT_FAILURE;
	}

	/* Once we have the bookmark line, write it to the bookmarks file */

	bm_fp=fopen(BM_FILE, "a+");
	if (!bm_fp) {
		fprintf(stderr, _("bookmarks: Error opening the bookmarks file\n"));
		free(tmp);
		return EXIT_FAILURE;
	}

	if (mod_file) {
		free(file);
		file=NULL;
	}

	if (fseek(bm_fp, 0L, SEEK_END) == -1) {
		fprintf(stderr, _("bookmarks: Error opening the bookmarks file\n"));
		free(tmp);
		fclose(bm_fp);
		return EXIT_FAILURE;
	}
	
	/* Everything is fine: add the new bookmark to the bookmarks file */
	fprintf(bm_fp, tmp);
	fclose(bm_fp);
	printf(_("File succesfully bookmarked\n"));
	free(tmp);

	return EXIT_SUCCESS;
}

int
bookmarks_function(char **cmd)
{
	if (!config_ok) {
		fprintf(stderr, _("Bookmarks function disabled\n"));
		return EXIT_FAILURE;
	}

	/* If the bookmarks file doesn't exist, create it */
	struct stat file_attrib;
	if (stat(BM_FILE, &file_attrib) == -1) {
		FILE *fp;
		fp=fopen(BM_FILE, "w+");
		if (!fp) {
			asprintf(&msg, "bookmarks: '%s': %s\n", 
					 BM_FILE, strerror(errno));
			if (msg) {
				log_msg(msg, NOPRINT_PROMPT);
				free(msg);
			}
			else
				fprintf(stderr, "bookmarks: '%s': %s\n", 
						BM_FILE, strerror(errno));
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
				fprintf(stderr, "bookmarks: %s: %s\n", cmd[2], strerror(errno));
				return EXIT_FAILURE;
			}
			return (add_bookmark(cmd[2]));
		}
		/* Delete bookmarks */
		else if (strcmp(cmd[1], "d") == 0 || strcmp(cmd[1], "del") == 0)
			return (del_bookmark());
	}
	
	/* If no arguments */

	if (clear_screen)
		CLEAR;
	printf(_("%sBookmarks Manager%s\n\n"), white, NC);

	/* Get bookmarks */
	char **bookmarks=get_bookmarks(BM_FILE);
	/* get_bookmarks() not only returns an array containing the actual bookmarks,
	* but the total amount of bookmarks as well via the global variable bm_n */
	if (bm_n == -1) /* Error opening BM_FILE */
		return EXIT_FAILURE; /* Errors are printed by get_bookmarks() 
		itself */
	/* If no bookmarks */
	if (bm_n == 0) {
		printf("%s%s%s: ", NC_b, default_color, PROGRAM_NAME);
		char *answer=rl_no_hist(_("There are no bookmarks.\nDo you want to "
								  "edit the bookmarks file? [Y/n] "));
		if (!answer)
			return EXIT_FAILURE;
		if (strcmp(answer, "n") == 0 || strcmp(answer, "N") == 0) {
			free(answer);
			return EXIT_SUCCESS;
		}
		else if (strcmp(answer, "y") == 0 || strcmp(answer, "Y") == 0 
					|| strcmp(answer, "") == 0) {
			free(answer);
			char *edit_cmd=NULL;
			if (flags & XDG_OPEN_OK) {
				edit_cmd=xcalloc(edit_cmd, 9, sizeof(char));
				strcpy(edit_cmd, "xdg-open");
			}
			else {
				printf(_("%s: xdg-open: Command not found\n"), PROGRAM_NAME);
				char *editor=rl_no_hist(_("Text editor: "));
				if (!editor)
					return EXIT_FAILURE;
				if (editor[0] == 0x00) { /* If the user just pressed Enter */
					free(editor);
					return EXIT_SUCCESS;
				}
				edit_cmd=xcalloc(edit_cmd, strlen(editor)+1, sizeof(char));
				strcpy(edit_cmd, editor);
				free(editor);
			}
			char *tmp_cmd[]={ edit_cmd, BM_FILE, NULL };
			if (launch_execve(tmp_cmd) == 0)
				bookmarks_function(cmd);
			free(edit_cmd);
			return EXIT_SUCCESS;
		}
		else {
			fprintf(stderr, _("bookmarks: '%s': Wrong answer\n"), answer);
			free(answer);
			return EXIT_SUCCESS;
		}	
	}

	/* If there are bookmarks... */
	char **bm_paths=NULL;
	bm_paths=xcalloc(bm_paths, bm_n, sizeof(char *));
	char **hot_keys=NULL;
	hot_keys=xcalloc(hot_keys, bm_n, sizeof(char *));
	char **bm_names=NULL;
	bm_names=xcalloc(bm_names, bm_n, sizeof(char *));
	int i;

	/* Get bookmarks paths */
	for(i=0;i<bm_n;i++) {
		char *str;
		str=straft(bookmarks[i], ':');
		if (str) {
			bm_paths[i]=xcalloc(bm_paths[i], strlen(str)+1, sizeof(char));
			strcpy(bm_paths[i], str);
			free(str);
		}
		else {
			str=straft(bookmarks[i], ']');
			if (str) {
				bm_paths[i]=xcalloc(bm_paths[i], strlen(str)+1, sizeof(char));
				strcpy(bm_paths[i], str);
				free(str);
			}
			else {
				bm_paths[i]=xcalloc(bm_paths[i], strlen(bookmarks[i])+1, 
					sizeof(char));
				strcpy(bm_paths[i], bookmarks[i]);
			}
		}
	}

	/* Get bookmarks hot keys */
	for (i=0;i<bm_n;i++) {
		char *str_b=strbtw(bookmarks[i], '[', ']');
		if (str_b) {
			hot_keys[i]=xcalloc(hot_keys[i], strlen(str_b)+1, sizeof(char));
			strcpy(hot_keys[i], str_b);
			free(str_b);
		}
		else 
			hot_keys[i]=NULL;
	}

	/* Get bookmarks names */
	for (i=0;i<bm_n;i++) {
		char *str_name=strbtw(bookmarks[i], ']', ':');
		if (str_name) {
			bm_names[i]=xcalloc(bm_names[i], strlen(str_name)+1, 
								sizeof(char));
			strcpy(bm_names[i], str_name);
			free(str_name);
		}
		else {
			str_name=strbfr(bookmarks[i], ':');
			if (str_name) {
				bm_names[i]=xcalloc(bm_names[i], strlen(str_name)+1, 
					sizeof(char));
				strcpy(bm_names[i], str_name);
				free(str_name);
			}
			else bm_names[i]=NULL;
		}
	}

	/* Print results */
	for (i=0;i<bm_n;i++) {
		int path_ok=stat(bm_paths[i], &file_attrib);
		if (hot_keys[i]) {
			if (bm_names[i]) {
				if (path_ok == 0) {
					if ((file_attrib.st_mode & S_IFMT) == S_IFDIR)
						printf("%s%d %s[%s]%s %s%s%s\n", eln_color, i+1, 
							   white, hot_keys[i], NC, cyan, 
							   bm_names[i], NC);
					else
						printf("%s%d %s[%s]%s %s%s%s\n", eln_color, i+1, 
							   white, hot_keys[i], NC, default_color, 
							   bm_names[i], NC);					
				}
				else
					printf("%s%d [%s] %s%s\n", gray, i+1, hot_keys[i], 
						bm_names[i], NC);
			}
			else {
				if (path_ok == 0) {
					if ((file_attrib.st_mode & S_IFMT) == S_IFDIR)
						printf("%s%d %s[%s]%s %s%s%s\n", eln_color, i+1, 
							   white, hot_keys[i], NC, cyan, 
							   bm_paths[i], NC);
					else
						printf("%s%d %s[%s]%s %s%s%s\n", eln_color, i+1, 
							   white, hot_keys[i], NC, default_color, 
							   bm_paths[i], NC);		
				}
				else
					printf("%s%d [%s] %s%s\n", gray, i+1, hot_keys[i], 
						bm_paths[i], NC);
			}
		}
		else {
			if (bm_names[i]) {
				if (path_ok == 0) {
					if ((file_attrib.st_mode & S_IFMT) == S_IFDIR)
						printf("%s%d %s%s%s\n", eln_color, i+1, cyan, 
							bm_names[i], NC);
					else
						printf("%s%d %s%s%s%s\n", eln_color, i+1, NC, 
							   default_color, bm_names[i], NC);				
				}
				else
					printf("%s%d %s%s\n", gray, i+1, bm_names[i], NC);
			}
			else {
				if (path_ok == 0) {
					if ((file_attrib.st_mode & S_IFMT) == S_IFDIR)
						printf("%s%d %s%s%s\n", eln_color, i+1, cyan, 
							bm_paths[i], NC);
					else
						printf("%s%d %s%s%s%s\n", eln_color, i+1, NC, 
							   default_color, bm_paths[i], NC);	
				}
				else
					printf("%s%d %s%s\n", gray, i+1, bm_paths[i], NC);
			}
		}
	}

	/* Display the prompt */
	int reload_bm=0, go_dirlist=0; //, go_back_prompt=0;
	char **comm_bm=bm_prompt();
	if (!comm_bm)
		return EXIT_FAILURE;
	
	/* User selection */
	/* If string... */
	if (!is_number(comm_bm[0])) {
		int valid_hk=0, eln=0;
		if (strcmp(comm_bm[0], "e") == 0) {
			/* If no application has been specified and xdg-open doesn't 
			 * exist... */
			if (args_n == 0 && !(flags & XDG_OPEN_OK))
				fprintf(stderr, _("%s: xdg-open not found. Try "
								  "'e APPLICATION'\n"), PROGRAM_NAME);
			else {
				if (args_n > 0) {
					char *bm_comm_path=NULL;
					if ((bm_comm_path=get_cmd_path(comm_bm[1])) != NULL) {
						pid_t pid_edit_bm=fork();
						if (pid_edit_bm == 0) {
							set_signals_to_default();
							execle(bm_comm_path, comm_bm[1], BM_FILE, NULL, 
								__environ);
							fprintf(stderr, "%s: bookmarks: %s\n", 
									PROGRAM_NAME, strerror(errno));
							exit(EXIT_FAILURE);
						}
						else waitpid(pid_edit_bm, NULL, 0);
						reload_bm=1;
						free(bm_comm_path);
					}
					else fprintf(stderr, _("bookmarks: '%s': Application "
										   "not found\n"), comm_bm[1]);
				}
				else {
					pid_t pid_edit_bm=fork();
					if (pid_edit_bm == 0) {
						set_signals_to_default();
						execle(xdg_open_path, "xdg-open", BM_FILE, NULL, 
							__environ);
						fprintf(stderr, "%s: bookmarks: %s\n", PROGRAM_NAME,
								strerror(errno));
						exit(EXIT_FAILURE);
					}
					else waitpid(pid_edit_bm, NULL, 0);
					reload_bm=1;
				}
			}
		}
		
		else if (strcmp(comm_bm[0], "q") == 0) {
			go_dirlist=1;
		}
		
		else { /* If neither 'e' nor 'q'... */
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
					fprintf(stderr, _("bookmarks: %d: No such bookmark\n"), 
							bm_n);
				else fprintf(stderr, _("bookmarks: %s: No such bookmark\n"), 
							 comm_bm[0]);
/*				go_back_prompt=1; */
			}
			else { /* If a valid hotkey... */
				if (stat(bm_paths[eln], &file_attrib) == 0) {
					/* If a directory */
					if ((file_attrib.st_mode & S_IFMT) == S_IFDIR) {
						int ret=chdir(bm_paths[eln]);
						if (ret == 0) {
							free(path);
							path=xcalloc(path, strlen(bm_paths[eln])+1, 
										 sizeof(char));
							strcpy(path, bm_paths[eln]);
							add_to_dirhist(path);
							go_dirlist=1;
						}
						else {
							fprintf(stderr, "bookmarks: '%s': %s\n", 
									bm_paths[eln], strerror(errno));
						}
					}
					/* If a regular file... */
					else if ((file_attrib.st_mode & S_IFMT) == S_IFREG) {
						/* If no opening application has been passed... */
						if (args_n == 0) { 
							if (flags & XDG_OPEN_OK) {
								pid_t pid_bm=fork();
								if (pid_bm == 0) {
									set_signals_to_default();
									execle(xdg_open_path, "xdg-open", 
										bm_paths[eln], NULL, __environ);
									fprintf(stderr, "%s: bookmarks: %s\n", 
											PROGRAM_NAME, strerror(errno));
									exit(EXIT_FAILURE);
								}
								else waitpid(pid_bm, NULL, 0);
							}
							else fprintf(stderr, _("%s: xdg-open not found. "
											"Try 'shortcut APPLICATION'\n"), 
											PROGRAM_NAME);
						}
						/* If application has been passed (as 2nd arg)... */
						else {
							char *cmd_path=NULL;
							/* Check if application exists */
							if ((cmd_path=get_cmd_path(comm_bm[1])) != NULL) {
								pid_t pid_bm=fork();
								if (pid_bm == 0) {
									set_signals_to_default();
									execle(cmd_path, comm_bm[1], 
										   bm_paths[eln], NULL, __environ);
									fprintf(stderr, "%s: bookmarks: %s\n", 
											PROGRAM_NAME, strerror(errno));
									exit(EXIT_FAILURE);
								}
								else
									waitpid(pid_bm, NULL, 0);
							}
							else {
								fprintf(stderr, _("bookmarks: %s: Application "
												  "not found\n"), comm_bm[1]);
							}
							free(cmd_path);
						}
					}
					else /* If neither dir nor regular file */
						fprintf(stderr, _("bookmarks: '%s': Cannot open "
										  "file\n"), bm_paths[eln]);
				}
				else {
					fprintf(stderr, "bookmarks: '%s': %s\n",
						   bm_paths[eln], strerror(errno));
				}
			}
		}
	}
	
	/* If digit */
	else {
		size_t atoi_comm_bm=atoi(comm_bm[0]);
		if (atoi_comm_bm > 0 && atoi_comm_bm <= bm_n) {
			/* CHECK FILE EXISTENCE!! */
			stat(bm_paths[atoi_comm_bm-1], &file_attrib);
			if ((file_attrib.st_mode & S_IFMT) == S_IFDIR) { //if a directory...
				int ret=chdir(bm_paths[atoi_comm_bm-1]);
				if (ret == 0) {
					free(path);
					path=xcalloc(path, strlen(bm_paths[atoi_comm_bm-1])+1, 
								 sizeof(char));
					strcpy(path, bm_paths[atoi_comm_bm-1]);
					add_to_dirhist(path);
					go_dirlist=1;
				}
				else
					fprintf(stderr, "bookmarks: '%s': %s\n",  
							bm_paths[atoi_comm_bm-1], strerror(errno));
			}
			/* If a file... */
			else if ((file_attrib.st_mode & S_IFMT) == S_IFREG) {
				/* If no opening application has been passed... */
				if (args_n == 0) {
					if (!(flags & XDG_OPEN_OK))
						fprintf(stderr, _("bookmarks: xdg-open not found. Try "
										  "'ELN/hot-key application'\n"));
					else {
						pid_t pid_bm=fork();
						if (pid_bm == 0) {
							set_signals_to_default();
							execle(xdg_open_path, "xdg-open", 
								bm_paths[atoi_comm_bm-1], NULL, __environ);
							fprintf(stderr, "bookmarks: %s\n",
									strerror(errno));
							exit(EXIT_FAILURE);
						}
						else waitpid(pid_bm, NULL, 0);
					}
				}
				else { /* If application has been passed (as 2nd arg)... */
					/* Check if application exists */
					char *cmd_path=NULL;
					if ((cmd_path=get_cmd_path(comm_bm[1])) != NULL) {
						pid_t pid_bm=fork();
						if (pid_bm == 0) {
							set_signals_to_default();
							execle(cmd_path, comm_bm[1], 
								   bm_paths[atoi_comm_bm-1], NULL, __environ);
							fprintf(stderr, "bookmarks: %s\n",
									strerror(errno));
						}
						else waitpid(pid_bm, NULL, 0);
					}
					else {
						fprintf(stderr, _("bookmarks: '%s': Application not "
										  "found\n"), comm_bm[1]);
/*						puts("Press Enter key to continue... ");
						getchar();
						bookmarks_function(); */
					}
					free(cmd_path);
				}
			}
			else
				fprintf(stderr, _("bookmarks: '%s': Cannot open file\n"), 
						bm_paths[atoi_comm_bm-1]);
		}
		else {
			fprintf(stderr, _("bookmarks: '%s': No such bookmark\n"), 
					comm_bm[0]);
		}
	}
/*	if (go_back_prompt) {
		for (int i=0;i<=args_n;i++)
			free(comm_bm[i]);
		free(comm_bm);
		bm_prompt();
	}
	else {*/
		/* FREE EVERYTHING! */
		for (i=0;i<=args_n;i++)
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
		if (reload_bm)
			bookmarks_function(cmd);

		if (go_dirlist && cd_lists_on_the_fly) {
			while (files--) free(dirlist[files]);
			list_dir();
		}

	return EXIT_SUCCESS;
}

void
dir_size(char *dir)
{
	#ifndef DU_TMP_FILE
		#define DU_TMP_FILE "/tmp/.du_size"
	#endif

	if (!dir)
		return;

	/* Check for 'du' existence just once (the first time the function is 
	 * called) by storing the result in a static variable (whose value will 
	 * survive the function) */
	static char du_path[PATH_MAX]="";
	if (du_path[0] == 0x00) {
		char *du_path_tmp=NULL;
		if ((du_path_tmp=get_cmd_path("du")) != NULL) {
			strncpy(du_path, du_path_tmp, PATH_MAX);
			free(du_path_tmp);
		}
		else {
			puts(_("('du': not found)"));
			return;
		}
	}
	FILE *du_fp=fopen(DU_TMP_FILE, "w");
	FILE *du_fp_err=fopen("/dev/null", "w");
	int stdout_bk=dup(STDOUT_FILENO); /* Save original stdout */
	int stderr_bk=dup(STDERR_FILENO); /* Save original stderr */
	dup2(fileno(du_fp), STDOUT_FILENO); /* Redirect stdout to the desired file */	
	dup2(fileno(du_fp_err), STDERR_FILENO); /* Redirect stderr to /dev/null */
	fclose(du_fp);
	fclose(du_fp_err);

/*	char *tmp_dir=NULL;
	tmp_dir=xcalloc(tmp_dir, strlen(dir)+2, sizeof(char *));
	sprintf(tmp_dir, "'%s'", dir);
	char *cmd[]={ "du", "-sh", tmp_dir, NULL };
	int ret=launch_execve(cmd);
	free(tmp_dir); */

	char *cmd=NULL;
	cmd=xcalloc(cmd, strlen(dir)+10, sizeof(char));
	sprintf(cmd, "du -sh '%s'", dir);
	int ret=launch_execle(cmd);
	free(cmd);
	
	dup2(stdout_bk, STDOUT_FILENO); /* Restore original stdout */
	dup2(stderr_bk, STDERR_FILENO); /* Restore original stderr */
	close(stdout_bk);
	close(stderr_bk);

	if (ret != 0) {
		puts("???");
		return;
	}
	if (access(DU_TMP_FILE, F_OK) == 0) {
		du_fp=fopen(DU_TMP_FILE, "r");
		if (du_fp) {
			/* I only need here the first field of the line, which is a file
			 * size and could only take a few bytes, so that 32 bytes is
			 * more than enough */
			char line[32]="";
			fgets(line, sizeof(line), du_fp);
			char *file_size=strbfr(line, '\t');
			if (file_size)
				printf("%siB\n", file_size);
			else 
				puts("strbfr: error");
			free(file_size);
			fclose(du_fp);
		}
		else
			puts(_("unknown"));
		remove(DU_TMP_FILE);
	}
	else 
		puts(_("unknown"));
}

int
properties_function(char **comm)
{
	if(!comm)
		return EXIT_FAILURE;

	int exit_code=0;
	if (comm[1] && (strcmp(comm[1], "all") == 0
	|| strcmp(comm[1], "a") == 0)) {
		int status=long_view;
		long_view=1;
		int max=get_max_long_view();
		for (int i=0;i<files;i++) {
			printf("%s%d%s ", eln_color, i+1, NC);
			if (get_properties(dirlist[i], (int)long_view, max) != 0)
				exit_code=1;
		}
		long_view=status;
		return exit_code;
	}
	
	if (comm[1] && (strcmp(comm[1], "size") == 0
	|| strcmp(comm[1], "s") == 0)) { /* List files size */
		struct stat file_attrib;
		/* Get largest filename to format the output */
		int largest=0;
		for (int i=files;i--;) {
			size_t file_len=(unicode) ? u8_xstrlen(dirlist[i])
							: strlen(dirlist[i]);
			if (file_len > largest)
				largest=file_len+1;
		}
		for (int i=0;i<files;i++) {
			/* Get the amount of continuation bytes for each filename. This
			 * is necessary to properly format the output in case of non-ASCII
			 * strings */
			(unicode) ? u8_xstrlen(dirlist[i]) 
					  : strlen(dirlist[i]);
			lstat(dirlist[i], &file_attrib);
			char *size=get_size_unit(file_attrib.st_size);
			/* Print: 		filename		ELN		Padding		no new line*/
			colors_list(dirlist[i], i+1, largest+cont_bt, 0);
			printf("%s%s%s\n", NC, default_color, (size) ? size : "??");
			if (size)
				free(size);
		}
		return EXIT_SUCCESS;
	}
	
	/* If "pr file file..." */
	for (size_t i=1;i<=args_n;i++) {
		char *deq_file=dequote_str(comm[i], 0);
		if (!deq_file) {
			fprintf(stderr, _("%s: '%s': Error dequoting filename\n"), 
					PROGRAM_NAME, comm[i]);
			exit_code=1;
			continue;
		}
		if (get_properties(deq_file, 0, 0) != 0)
			exit_code=1;
		free(deq_file);
	}
	return exit_code;
}

int
get_properties(char *filename, int _long, int max)
/* This is my humble version of 'ls -hl' command plus most of the info from 
 * 'stat' command */
{
	struct statx file_attrib;
	/* Check file existence */
	/* statx(), introduced since kernel 4.11, is the improved version of 
	 * stat(): it's able to get creation time, unlike stat() */
	if (statx(AT_FDCWD, filename, AT_SYMLINK_NOFOLLOW, STATX_ALL, 
			  &file_attrib) == -1) {
		fprintf(stderr, "%s: pr: '%s': %s\n", PROGRAM_NAME, filename, 
				strerror(errno));
		return EXIT_FAILURE;
	}
	/* Get file size */
	char *size_type=get_size_unit(file_attrib.stx_size);

	/* Get file type (and color): */
	int sticky=0;
	char file_type=0, color[15]="";
	char *linkname=NULL;
	switch(file_attrib.stx_mode & S_IFMT) {
		case S_IFREG:
			file_type='-';
			if (!(file_attrib.stx_mode & S_IRUSR)) strcpy(color, nf_c);
			else if (file_attrib.stx_mode & S_ISUID)
				strcpy(color, su_c);
			else if (file_attrib.stx_mode & S_ISGID)
				strcpy(color, sg_c);
			else {
				cap_t cap=cap_get_file(filename);
				if (cap) {
					strcpy(color, ca_c);
					cap_free(cap);
				}
				else if (file_attrib.stx_mode & S_IXUSR) {
					if (file_attrib.stx_size == 0) strcpy(color, ee_c);
					else strcpy(color, ex_c);
				}
				else if (file_attrib.stx_size == 0) strcpy(color, ef_c);
				else strcpy(color, fi_c);
			}
		break;
		case S_IFDIR:
			file_type='d';
			if (access(filename, R_OK|X_OK) != 0)
				strcpy(color, nd_c);
			else {
				int is_oth_w=0;
				if (file_attrib.stx_mode & S_ISVTX) sticky=1;
				if (file_attrib.stx_mode & S_IWOTH) is_oth_w=1;
				int files_dir=count_dir(filename);
				strcpy(color, (sticky) ? ((is_oth_w) ? tw_c : 
					st_c) : ((is_oth_w) ? ow_c : 
					((files_dir == 2 || files_dir == 0) ? ed_c : di_c)));
			}
		break;
		case S_IFLNK:
			file_type='l';
			linkname=realpath(filename, NULL);
			if (linkname)
				strcpy(color, ln_c);
			else
				strcpy(color, or_c);
		break;
		case S_IFSOCK: file_type='s'; strcpy(color, so_c); break;
		case S_IFBLK: file_type='b'; strcpy(color, bd_c); break;
		case S_IFCHR: file_type='c'; strcpy(color, cd_c); break;
		case S_IFIFO: file_type='p'; strcpy(color, pi_c); break;
		default: file_type='?'; strcpy(color, no_c);
	}
	
	/* Get file permissions */
	char read_usr='-', write_usr='-', exec_usr='-', 
		 read_grp='-', write_grp='-', exec_grp='-',
		 read_others='-', write_others='-', exec_others='-';

	mode_t val=(file_attrib.stx_mode & ~S_IFMT);
	if (val & S_IRUSR) read_usr='r';
	if (val & S_IWUSR) write_usr='w'; 
	if (val & S_IXUSR) exec_usr='x';

	if (val & S_IRGRP) read_grp='r';
	if (val & S_IWGRP) write_grp='w';
	if (val & S_IXGRP) exec_grp='x';

	if (val & S_IROTH) read_others='r';
	if (val & S_IWOTH) write_others='w';
	if (val & S_IXOTH) exec_others='x';

	if (file_attrib.stx_mode & S_ISUID) 
		(val & S_IXUSR) ? (exec_usr='s') : (exec_usr='S');
	if (file_attrib.stx_mode & S_ISGID) 
		(val & S_IXGRP) ? (exec_grp='s') : (exec_grp='S');

	/* Get number of links to the file */
	nlink_t link_n=file_attrib.stx_nlink;

	/* Get modification time */
	time_t time=file_attrib.stx_mtime.tv_sec;
	struct tm *tm=localtime(&time);
	char mod_time[128]="";
	/* Store formatted (and localized) date-time string into mod_time */
	strftime(mod_time, sizeof(mod_time), "%b %d %H:%M:%S %Y", tm);

	/* Get owner and group names */
	uid_t owner_id=file_attrib.stx_uid; /* owner ID */
	gid_t group_id=file_attrib.stx_gid; /* group ID */
	struct group *group;
	struct passwd *owner;
	group=getgrgid(group_id);
	owner=getpwuid(owner_id); 
	
	/* Print file properties for long view mode */
	if (_long) {
		size_t filename_len=((unicode) ? u8_xstrlen(filename) : strlen(filename));
		/*	If filename length is greater than max, truncate it (max-1 = '~')
		 * to let the user know the filename isn't complete */
		 /* The value of max (global) is (or should be) already calculated by 
		  * get_max_long_view() before calling this function */
		char trim_filename[NAME_MAX]="";
		char trim=0;
		if (filename_len > max) {
			trim=1;
			strcpy(trim_filename, filename);
			trim_filename[(max+cont_bt)-1]='~';
			trim_filename[max+cont_bt]=0x00;
		}
		printf("%s%-*s %s%s (%04o) %c/%c%c%c/%c%c%c/%c%c%c %s %s %s %s\n", 
				color, max+cont_bt, (!trim) ? filename : trim_filename, NC, 
				default_color, file_attrib.stx_mode & 07777,
				file_type,
				read_usr, write_usr, exec_usr, 
				read_grp, write_grp, exec_grp,
				read_others, write_others, (sticky) ? 't' : exec_others,
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
	printf("(%04o)%c/%c%c%c/%c%c%c/%c%c%c %ld %s %s %s %s ", 
							file_attrib.stx_mode & 07777, file_type, 
							read_usr, write_usr, exec_usr, 
							read_grp, write_grp, exec_grp,
							read_others, write_others, (sticky) ? 't' 
							: exec_others, link_n, 
							(!owner) ? _("unknown") : owner->pw_name, 
							(!group) ? _("unknown") : group->gr_name, 
							(size_type) ? size_type : "??", 
							(mod_time[0] != 0x00) ? mod_time : "??");
	if (file_type && file_type != 'l')
		printf("%s%s%s%s\n", color, filename, NC, default_color);
	else if (linkname) {
		printf("%s%s%s%s -> %s\n", color, filename, NC, default_color, linkname);
		free(linkname);
	}
	else { /* Broken link */
		char link[PATH_MAX]="";
		ssize_t ret=readlink(filename, link, PATH_MAX);		
		if (ret) {
			printf("%s%s%s%s -> %s (broken link)\n", color, filename, NC, 
				   default_color, link);
		}
		else
			printf("%s%s%s%s -> ???\n", color, filename, NC, default_color);
	}
	
	/* Stat information */

	/* Last access time */
	time=file_attrib.stx_atime.tv_sec;
	tm=localtime(&time);
	char access_time[128]="";
	/* Store formatted (and localized) date-time string into access_time */
	strftime(access_time, sizeof(access_time), "%b %d %H:%M:%S %Y", tm);

	/* Creation time */
	time=file_attrib.stx_btime.tv_sec;
	tm=localtime(&time);
	char creation_time[128]="";
	strftime(creation_time, sizeof(creation_time), "%b %d %H:%M:%S %Y", tm);

	/* Last properties change time */
	time=file_attrib.stx_ctime.tv_sec;
	tm=localtime(&time);
	char change_time[128]="";
	strftime(change_time, sizeof(change_time), "%b %d %H:%M:%S %Y", tm);

	if (file_type == 'd') printf(_("Directory"));
	else if (file_type == 's') printf(_("Socket"));
	else if (file_type == 'l') printf(_("Symbolic link"));
	else if (file_type == 'b') printf(_("Block special file"));
	else if (file_type == 'c') printf(_("Character special file"));
	else if (file_type == 'p') printf(_("Fifo"));
	else if (file_type == '-') printf(_("Regular file"));

	printf(_("\tBlocks: %lld"), file_attrib.stx_blocks);
	printf(_("\tIO Block: %d"), file_attrib.stx_blksize);
	printf(_("\tInode: %lld\n"), file_attrib.stx_ino);
	printf(_("Device: %d:%d"), file_attrib.stx_dev_major, 
		   file_attrib.stx_dev_minor);
	printf(_("\tUid: %d (%s)"), file_attrib.stx_uid, (!owner) ? _("unknown") 
		: owner->pw_name);
	printf(_("\tGid: %d (%s)\n"), file_attrib.stx_gid, (!group) ? _("unknown") 
		: group->gr_name);

	/* Print file timestamps */
	printf(_("Access: \t%s\n"), access_time);
	printf(_("Modify: \t%s\n"), mod_time);
	printf(_("Change: \t%s\n"), change_time);
	printf(_("Birth: \t\t%s\n"), creation_time);

	/* Print file size */
	if ((file_attrib.stx_mode & S_IFMT) == S_IFDIR) {
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
	int exit_code=0;

	if (strcmp (comm[1], "status") == 0)
		printf(_("%s: Hidden files %s\n"), PROGRAM_NAME, 
			   (show_hidden) ? _("enabled") : _("disabled"));

	else if (strcmp(comm[1], "off") == 0) {
		if (show_hidden == 1) {
			show_hidden=0;
			if (cd_lists_on_the_fly) {
				while (files--) free(dirlist[files]);
				if (list_dir() != 0)
					exit_code=1;
			}
		}
	}

	else if (strcmp(comm[1], "on") == 0) {
		if (show_hidden == 0) {
			show_hidden=1;
			if (cd_lists_on_the_fly) {
				while (files--) free(dirlist[files]);
				if (list_dir() != 0)
					exit_code=1;
			}
		}
	}

	else 
		fputs(_("Usage: hidden, hf [on, off, status]\n"), stderr);
	
	return exit_code;
}

int
log_function(char **comm)
/* Log 'comm' into LOG_FILE */
{
	if (!config_ok)
		return EXIT_FAILURE;
	
	int clear_log=0;
	/* If the command was just 'log' */
	if (strcmp(comm[0], "log") == 0 && !comm[1]) {
		FILE *log_fp;
		log_fp=fopen(LOG_FILE, "r");
		if (!log_fp) {
			asprintf(&msg, "%s: log: '%s': %s\n", PROGRAM_NAME, 
					 LOG_FILE, strerror(errno));
			if (msg) {
				log_msg(msg, NOPRINT_PROMPT);
				free(msg);
			}
			else
				fprintf(stderr, "%s: log: '%s': %s\n", PROGRAM_NAME, 
						LOG_FILE, strerror(errno));
			return EXIT_FAILURE;
		}
		else {
			size_t line_size=0;
			char *line_buff=NULL;
			ssize_t line_len=0;
			while ((line_len=getline(&line_buff, &line_size, log_fp)) > 0)
				printf("%s", line_buff);
			free(line_buff);
			line_buff=NULL;

/*			char line[MAX_LINE]="";	
			while (fgets(line, sizeof(line), log_fp))
				printf("%s", line); */

			fclose(log_fp);
			return EXIT_SUCCESS;
		}
	}
	
	/* If 'log clear' */
	else if (strcmp(comm[0], "log") == 0 && args_n == 1) {
		if (strcmp(comm[1], "clear") == 0) {
			clear_log=1;
		}
	}

	/* Construct the log */
	/* Create a buffer big enough to hold the entire command */
	size_t com_len=0;
	int i=0;
	for (i=args_n;i>=0;i--)
		/* Argument length plus space plus null byte terminator */
		com_len+=(strlen(comm[i])+1);
	char full_comm[com_len];
	memset(full_comm, 0x00, com_len);
	strncpy(full_comm, comm[0], com_len);
	for (i=1;i<=args_n;i++) {
		strncat(full_comm, " ", com_len);
		strncat(full_comm, comm[i], com_len);
	}
	/* And now a buffer for the whole log line */
	char *date=get_date();
	size_t log_len=strlen(date)+strlen(user)+strlen(path)+com_len+7;
	char full_log[log_len];
	memset(full_log, 0x00, log_len);
	snprintf(full_log, log_len, "[%s] %s:%s:%s\n", date, user, path, 
			 full_comm);
	free(date);
	
	/* Write the log into LOG_FILE */
	FILE *log_fp;
	/* If not 'log clear', append the log to the existing logs */
	if (!clear_log)
		log_fp=fopen(LOG_FILE, "a");
	/* Else, overwrite the log file leaving only the 'log clear' command */
	else 
		log_fp=fopen(LOG_FILE, "w+");
	if (!log_fp) {
		asprintf(&msg, "%s: log: '%s': %s\n", PROGRAM_NAME, LOG_FILE,
				 strerror(errno));
		if (msg) {
			error_msg=1;
			log_msg(msg, PRINT_PROMPT);
			free(msg);
		}
		else
			fprintf(stderr, "%s: log: '%s': %s\n", PROGRAM_NAME, LOG_FILE,
					strerror(errno));
		return EXIT_FAILURE;
	}
	else { /* If LOG_FILE was correctly opened, write the log */
		fprintf(log_fp, "%s", full_log);
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
	FILE *log_fp=NULL;
	if (stat(log_file, &file_attrib) == -1) {
		log_fp=fopen(log_file, "w");
		if (!log_fp) {
			asprintf(&msg, "%s: '%s': %s\n", PROGRAM_NAME, log_file, 
					strerror(errno));
			if (msg) {
				log_msg(msg, NOPRINT_PROMPT);
				free(msg);
			}
			else
				fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, log_file, 
						strerror(errno));
		}
		else
			fclose(log_fp);
		return; /* Return anyway, for, being a new empty file, there's no need
		to truncate it */
	}
	
	/* Truncate the file, if needed */
	log_fp=fopen(log_file, "r");
	if (log_fp != NULL) {
		int logs_num=0;

		/* Count newline chars to get amount of lines in file */
		char c;
		for (c=getc(log_fp);c != EOF;c=getc(log_fp))
			if (c == '\n') 
				logs_num++;
		fclose(log_fp);

		if (logs_num > max_log) {
			log_fp=fopen(log_file, "r");
			if (log_fp == NULL) {
				perror("log");
				return;
			}
			FILE *log_fp_tmp=fopen(LOG_FILE_TMP, "w+");
			if (log_fp_tmp == NULL) {
				perror("log");
				fclose(log_fp);
				return;
			}

			int i=1;
			size_t line_size=0;
			char *line_buff=NULL;
			ssize_t line_len=0;
			while ((line_len=getline(&line_buff, &line_size, log_fp)) > 0)
				if (i++ >= logs_num-(max_log-1)) /* Delete oldest entries */
					fprintf(log_fp_tmp, "%s", line_buff);
			free(line_buff);
			line_buff=NULL;
			fclose(log_fp_tmp);
			fclose(log_fp);
			remove(log_file);
			rename(LOG_FILE_TMP, log_file);
		}
	}
	else {
		asprintf(&msg, "%s: log: %s: %s\n", PROGRAM_NAME, log_file, 
				strerror(errno));
		if (msg) {
			log_msg(msg, NOPRINT_PROMPT);
			free(msg);
		}
		else
			fprintf(stderr, "%s: log: %s: %s\n", PROGRAM_NAME, log_file, 
					strerror(errno));
	}
}

int
get_history(void)
{
	if (!config_ok)
		return EXIT_FAILURE;

	FILE *hist_fp=fopen(HIST_FILE, "r");
	if (current_hist_n == 0) /* Coming from main() */
		history=xcalloc(history, 1, sizeof(char *));
	else { /* Only true when comming from 'history clear' */
		for (size_t i=0;i<current_hist_n;i++)
			free(history[i]);
		history=xrealloc(history, 1*sizeof(char *));
		current_hist_n=0;
	}
	if (hist_fp) {

		size_t line_size=0;
		char *line_buff=NULL;
		ssize_t line_len=0;
		while ((line_len=getline(&line_buff, &line_size, hist_fp)) > 0) {
			line_buff[line_len-1]=0x00;
			history=xrealloc(history, (current_hist_n+1)*sizeof(char *));
			history[current_hist_n]=xcalloc(history[current_hist_n], 
											line_len+1, sizeof(char));
			strncpy(history[current_hist_n++], line_buff, line_len);
		}
		free(line_buff);
		line_buff=NULL;

/*		char line[MAX_LINE]="";
		size_t line_len=0;
		while (fgets(line, sizeof(line), hist_fp)) {
			line_len=strlen(line);
			line[line_len-1]=0x00;
			history=xrealloc(history, (current_hist_n+1)*sizeof(char **));
			history[current_hist_n]=xcalloc(history[current_hist_n], 
											line_len, sizeof(char *));
			strncpy(history[current_hist_n++], line, line_len);
		} */

		fclose(hist_fp);
		return EXIT_SUCCESS;
	}
	else {
		asprintf(&msg, "%s: history: '%s': %s\n", PROGRAM_NAME, 
				 HIST_FILE, strerror(errno));
		if (msg) {
			error_msg=1;
			log_msg(msg, PRINT_PROMPT);
			free(msg);
		}
		else
			fprintf(stderr, "%s: history: '%s': %s\n", PROGRAM_NAME, 
					HIST_FILE, strerror(errno));
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
		for (int i=0;i<current_hist_n;i++)
			printf("%d %s\n", i+1, history[i]);
		return EXIT_SUCCESS;
	}

	/* If 'history clear', guess what, clear the history list! */
	if (args_n == 1 && strcmp(comm[1], "clear") == 0) {
		FILE *hist_fp=fopen(HIST_FILE, "w+");
		if (!hist_fp) {
			asprintf(&msg, "%s: history: %s: %s\n", PROGRAM_NAME, 
					 HIST_FILE, strerror(errno));
			if (msg) {
				log_msg(msg, NOPRINT_PROMPT);
				free(msg);
			}
			else
				fprintf(stderr, "%s: history: %s: %s\n", PROGRAM_NAME, 
						HIST_FILE, strerror(errno));
			return EXIT_FAILURE;
		}
		/* Do not create an empty file */
		fprintf(hist_fp, "%s %s\n", comm[0], comm[1]);
		fclose(hist_fp);
		/* Update the history array */
		int exit_code=0;
		if (get_history() != 0)
			exit_code=1;
		if (log_function(comm) != 0)
			exit_code=1;
		return exit_code;
	}

	/* If 'history -n', print the last -n elements */
	if (args_n == 1 && comm[1][0] == '-' && is_number(comm[1]+1)) {
		int num=atoi(comm[1]+1);
		if (num < 0 || num > current_hist_n) {
			num=current_hist_n;
		}
		for (int i=current_hist_n-num;i<current_hist_n;i++)
			printf("%d %s\n", i+1, history[i]);
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
	if (is_number(cmd)) {
		int num=atoi(cmd);
		if (num > 0 && num < current_hist_n) {
			int old_args=args_n, exit_code=0;
			char **cmd_hist=parse_input_str(history[num-1]);
			if (exec_cmd(cmd_hist) != 0)
				exit_code=1;
			exec_cmd(cmd_hist);
			for (int i=0;i<=args_n;i++)
				free(cmd_hist[i]);
			free(cmd_hist);
			args_n=old_args;
			return exit_code;
		}
		else
			fprintf(stderr, _("%s: !%d: event not found\n"), 
					PROGRAM_NAME, num);
		return EXIT_FAILURE;
	}

	/* If "!!", execute the last command */
	else if (strcmp(cmd, "!") == 0) {
		int old_args=args_n, exit_code=0;
		char **cmd_hist=parse_input_str(history[current_hist_n-1]);
		if (exec_cmd(cmd_hist) != 0)
			exit_code=1;
		for (int i=0;i<=args_n;i++)
			free(cmd_hist[i]);
		free(cmd_hist);
		args_n=old_args;
		return exit_code;
	}

	/* If "!-n" */
	else if (cmd[0] == '-') {
		/* If not number or zero or bigger than max... */
		if (!is_number(cmd+1) || (atoi(cmd+1) == 0 
				|| atoi(cmd+1) > current_hist_n-1)) {
			fprintf(stderr, _("%s: !%s: event not found\n"), 
					PROGRAM_NAME, cmd);
			return EXIT_FAILURE;
		}
		int old_args=args_n, exit_code=0;		
		char **cmd_hist=parse_input_str(
			history[current_hist_n-atoi(cmd+1)-1]);
		if (exec_cmd(cmd_hist) != 0)
			exit_code=1;
		for (int i=0;i<=args_n;i++)
			free(cmd_hist[i]);
		free(cmd_hist);
		args_n=old_args;
		return exit_code;
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
edit_function(char **comm)
/* Edit the config file, either via xdg-open or via the first passed argument,
 * Ex: 'edit nano' */
{
	if (!config_ok) {
		fprintf(stderr, _("%s: Cannot access configuration file\n"), 
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
		free(BM_FILE);
		free(LOG_FILE);
		free(LOG_FILE_TMP);
		free(HIST_FILE);
		free(CONFIG_FILE);
		free(PROFILE_FILE);
		free(MSG_LOG_FILE);
		free(sel_file_user);
		if (alt_profile)
			free(alt_profile);
		/* Rerun external_arguments */
		if (argc_bk > 1)
			external_arguments(argc_bk, argv_bk);
		init_config();
		stat(CONFIG_FILE, &file_attrib);
	}
	time_t mtime_bfr=file_attrib.st_mtime;

	char *cmd_path=NULL;
	if (args_n > 0) { /* If there is an argument... */
		/* Check it is a valid program */
		if ((cmd_path=get_cmd_path(comm[1])) == NULL) {
			fprintf(stderr, _("%s: %s: Command not found\n"), PROGRAM_NAME, 
					comm[1]);
			return EXIT_FAILURE;
		}
	}
	/* If no application has been passed as 2nd argument, and if xdg-open 
	 * not found... */
	else if (!(flags & XDG_OPEN_OK)) {
		fprintf(stderr, 
				_("%s: 'xdg-open' not found. Try 'edit application_name'\n"), 
				PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	/* Either a valid program has been passed or xdg-open exists */
	pid_t pid_edit=fork();
	if (pid_edit < 0) {
		fprintf(stderr, "%s: fork: %s\n", PROGRAM_NAME, strerror(errno));
		if (cmd_path) 
			free(cmd_path);
		return EXIT_FAILURE;
	}
	else if (pid_edit == 0) {
		set_signals_to_default();
		/* If application has been passed */
		if (args_n > 0)
			execle(cmd_path, comm[1], CONFIG_FILE, NULL, __environ);		
		/* No application passed and xdg-open exists */
		else
			execle(xdg_open_path, "xdg-open", CONFIG_FILE, NULL, __environ);
		/* The program failed to start */
		fprintf(stderr, "%s: execle: %s: %s\n", PROGRAM_NAME, (cmd_path)
				? cmd_path : xdg_open_path, strerror(errno));
		exit(EXIT_FAILURE);
	}
	else
		run_in_foreground(pid_edit);
	
	if (cmd_path)
		free(cmd_path);
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
		free(BM_FILE);
		free(LOG_FILE);
		free(LOG_FILE_TMP);
		free(HIST_FILE);
		free(CONFIG_FILE);
		free(PROFILE_FILE);
		free(MSG_LOG_FILE);
		free(sel_file_user);
		if (alt_profile)
			free(alt_profile);
		if (argc_bk > 1)
			external_arguments(argc_bk, argv_bk);
		init_config();
		/* Free the aliases and prompt_cmds arrays to be allocated again */
		for (size_t i=0;i<aliases_n;i++)
			free(aliases[i]);
		for (size_t i=0;i<prompt_cmds_n;i++)
			free(prompt_cmds[i]);
		aliases_n = prompt_cmds_n = 0; /* Reset counters */
		get_aliases_n_prompt_cmds();
		if (cd_lists_on_the_fly) {
			while (files--) free(dirlist[files]);
			list_dir();
		}
	}
	return EXIT_SUCCESS;
}

void
color_codes(void)
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
(in the \"Filetypes colors\" line), using the same ANSI style color format \
used by dircolors. The same color code format is used for prompt, ELN, and \
command line text colors. By default, %s uses only 8 colors, but you can \
use 256 and RGB colors as well.\n\n"), PROGRAM_NAME);
}

void
list_commands(void)
{
	printf(_("\nNOTE: ELN = Element List Number. Example: In \
the line \"12 openbox\", 12 is the ELN corresponding to the 'openbox' \
file.\n"));
	printf(_("\n%scmd, commands%s%s: Show this list of commands.\n"), white, 
		   NC, default_color);
	printf(_("\n%s/%s%s* [DIR]: This is the quick search function. Just type '/' \
followed by the string you are looking for (you can use wildcards), and %s \
will list all matches in the current working directory. To search for files \
in any other directory, specify the directory name as second argument. This \
argument (DIR) could be an absolute path, a relative path, or an ELN.\n"), 
		   white, NC, default_color, PROGRAM_NAME);
	printf(_("\n%sbm, bookmarks%s%s [a, add PATH] [d, del]: With no argument, \
open the bookmarks menu. Here you can change the current directory to that \
specified by the corresponding bookmark by just typing either its ELN or \
its shortcut. In this screen you can also add, remove or edit your bookmarks \
by simply typing 'e' to edit the bookmarks file. To add or remove a bookmark \
directly from the command line, you can us the 'a' and 'd' arguments as \
follows: \"bm a PATH\" or \"bm d\" respectively.\n"), white, NC, 
		   default_color);
	printf(_("\n%so, open%s%s ELN/DIR/FILENAME [APPLICATION]: Open \
either a directory or a file. For example: 'o 12' or 'o filename'. By default, \
the 'open' function will open files with the default application associated to \
them (if xdg-open command is found). However, if you want to open a file with \
a different application, just add the application name as second argument, \
e.g. 'o 12 leafpad'. If you want to run the program in the background, simply \
add the ampersand character (&): 'o 12 &'. When it comes to directories, \
'open' works just like the 'cd' command.\n"), white, NC, default_color);
	printf(_("\n%scd%s%s [ELN/DIR]: When used with no argument, it changes the \
current directory to the default directory (HOME). Otherwise, 'cd' changes \
the current directory to the one specified by the first argument. You can use \
either ELN's or a string to indicate the directory you want. Ex: 'cd 12' or \
'cd ~/media'. Unlike the shell 'cd' command, %s's built-in 'cd' function doesn \
not only changes the current directory, but also lists its content \
(provided the option \"cd lists automatically\" is enabled, which is the \
default) according to a comprehensive list of color codes. By default, the \
output of 'cd' is much like this shell command: cd DIR && ls -A --color=auto \
--group-directories-first\n"), white, NC, default_color, PROGRAM_NAME);
	printf(_("\n%sb, back%s%s [h, hist] [clear] [!ELN]: Unlike 'cd ..', which \
will send you to the parent directory of the current directory, this command \
(with no argument) will send you back to the previously visited directory. %s \
keeps a record of all the visited directories. You can see this list by typing \
'b hist', 'b h' or 'bh', and you can access any element in this list by simply \
passing the corresponding ELN in this list (preceded by the exclamation mark) \
to the 'back' command. Example:\n\
	[user@hostname:S] ~ $ bh\n\
	1 /home/user\n\
	2 /etc\n\
	3 /proc\n\
	[user@hostname:S] ~ $ b !3\n\
	[user@hostname:S] /proc $ \n\
  Note: The line printed in green indicates the current position of the back \
function in the directory history list.\nFinally, you can also clear this \
history list by typing 'b clear'.\n"), white, NC, default_color, PROGRAM_NAME);
	printf(_("\n%sf, forth%s%s [h, hist] [clear] [!ELN]: It works just like \
the back function, but it goes forward in the history record. Of course, you \
can use 'f hist', 'f h', 'fh', and 'f !ELN'\n"), white, NC, default_color);
	printf(_("\n%sc, l, m, md, r%s%s: short for 'cp', 'ln', 'mv', 'mkdir', and \
'rm' commands respectivelly.\n"), white, NC, default_color);
	printf(_("\n%sp, pr, prop%s%s ELN/FILENAME(s) [a, all] [s, size]: Print \
file properties of FILENAME(s). Use 'all' to list properties of all files in \
the current working directory, and 'size' to list their corresponding \
sizes.\n"), white, NC, default_color);
	printf(_("\n%ss, sel%s%s ELN ELN-ELN FILENAME PATH... n: Send one or \
multiple elements to the Selection Box. 'Sel' accepts individual elements, \
range of elements, say 1-6, filenames and paths, just as wildcards. Ex: sel \
1 4-10 file* filename /path/to/filename\n"), white, NC, default_color);
	printf(_("\n%ssb, selbox%s%s: Show the elements contained in the \
Selection Box.\n"), white, NC, default_color);
	printf(_("\n%sds, desel%s%s [*, a, all]: Deselect one or more selected \
elements. You can also deselect all selected elements by typing 'ds *', \
'ds a' or 'ds all'.\n"), white, 
		   NC, default_color);
	printf(_("\n%st, tr, trash%s%s  [ELN's, FILE(s)] [ls, list] [clear] \
[del, rm]: With no argument (or by passing the 'ls' option), it prints a list \
of currently trashed files. The 'clear' option removes all files from the \
trash can, while the 'del' option lists trashed files allowing you to remove \
one or more of them. The trash directory is $XDG_DATA_HOME/Trash, that is, \
'~/.local/share/Trash'. Since this trash system follows the freedesktop \
specification, it is able to handle files trashed by different Trash \
implementations.\n"), white, NC, default_color);
	printf(_("\n%su, undel, untrash%s%s [*, a, all]: Print a list of currently \
trashed files allowing you to choose one or more of these files to be \
undeleted, that is to say, restored to their original location. You can also \
undelete all trashed files at once passing the '*', 'a' or 'all' option.\n"), 
		   white, NC, default_color);
	printf(_("\n%s;%s%scmd, %s:%s%scmd: Skip all %s expansions and send the \
input string (cmd) as it is to the system shell.\n"), white, NC, default_color, 
		   white, NC, default_color, PROGRAM_NAME);
	printf(_("\n%smp, mountpoints%s%s: List available mountpoints and change \
the current working directory into the selected mountpoint.\n"), white, NC, 
		   default_color);
	printf(_("\n%sv, paste%s%s [sel] [DESTINY]: The 'paste sel' command will \
copy the currently selected elements, if any, into the current working \
directory. If you want to copy these elements into another directory, you \
only need to tell 'paste' where to copy these files. Ex: paste sel \
/path/to/directory\n"), white, NC, default_color);
	printf(_("\n%spf, prof, profile%s%s: Print the currently used \
profile.\n"), white, NC, default_color);
	printf(_("\n%sshell%s%s [SHELL]: Print the current default shell for \
%s or set SHELL as the new default shell.\n"), white, NC, default_color, 
		   PROGRAM_NAME);
	printf(_("\n%slog%s%s [clear]: With no arguments, it shows the log file. \
If clear is passed as argument, it will delete all the logs.\n"), white, NC, 
		   default_color);
	printf(_("\n%smsg, messages%s%s [clear]: With no arguments, prints the \
list of messages in the current session. The 'clear' option tells %s to \
empty the messages list.\n"), white, NC, default_color, PROGRAM_NAME);
	printf(_("\n%shistory%s%s [clear] [-n]: With no arguments, it shows the \
history list. If 'clear' is passed as argument, it will delete all the entries \
in the history file. Finally, '-n' tells the history command to list only the \
last 'n' commands in the history list.\n"), white, NC, default_color);
	printf(_("You can use the exclamation character (!) to perform some \
history commands:\n\
  !!: Execute the last command.\n\
  !n: Execute the command number 'n' in the history list.\n\
  !-n: Execute the last-n command in the history list.\n"));
	printf(_("\n%sedit%s%s [EDITOR]: Edit the configuration file. If \
specified, use EDITOR, if available.\n"), white, NC, default_color);
	printf(_("\n%salias%s%s: Show aliases, if any. To write a new alias simpy \
type 'edit' to open the configuration file and add a line like this: \
alias alias_name='command_name args...'\n"), white, NC, default_color);
	printf(_("\n%ssplash%s%s: Show the splash screen.\n"), white, NC, default_color);
	printf(_("\n%spath, cwd%s%s: Print the current working directory.\n"), 
		   white, NC, default_color);
	printf(_("\n%srf, refresh%s%s: Refresh the screen.\n"), white, NC, 
		   default_color);
	printf(_("\n%scolors%s%s: Show the color codes used in the elements \
list.\n"), white, NC, default_color);
	printf(_("\n%shf, hidden%s%s [on/off/status]: Toggle hidden files \
on/off.\n"), white, NC, default_color);
	printf(_("\n%sff, folders first%s%s [on/off/status]: Toggle list folders \
first on/off.\n"), white, NC, default_color);
	printf(_("\n%spg, pager%s%s [on/off/status]: Toggle the pager on/off.\n"), 
			white, NC, default_color);
	printf(_("\n%suc, unicode%s%s [on/off/status]: Toggle unicode on/off.\n"), 
			white, NC, default_color);
	printf(_("\n%sext%s%s [on/off/status]: Toggle external commands \
on/off.\n"), white, NC, default_color);
	printf(_("\n%sver, version%s%s: Show %s version details.\n"), white, NC, 
			default_color, PROGRAM_NAME);
	printf(_("\n%slicense%s%s: Display the license notice.\n"), white, NC, 
			default_color);
	printf(_("\n%sfs%s%s: Print an extract from 'What is Free Software?', \
written by Richard Stallman.\n"), white, NC, 
			default_color);
	printf(_("\n%sq, quit, exit, zz%s%s: Safely quit %s.\n"), white, NC, 
			default_color, PROGRAM_NAME);
	printf(_("%s  \nKeyboard shortcuts%s%s:\n\
%s  A-c%s%s:	Clear the current command line\n\
%s  A-f%s%s:	Toggle list-folders-first on/off\n\
%s  C-r%s%s:	Refresh the screen\n\
%s  A-l%s%s:	Toggle long view mode on/off\n\
%s  A-m%s%s:	List mountpoints\n\
%s  A-b%s%s:	Launch the Bookmarks Manager\n\
%s  A-i%s%s:	Toggle hidden files on/off\n\
%s  A-s%s%s:	Open the Selection Box\n\
%s  A-a%s%s:	Select all files in the current working directory\n\
%s  A-d%s%s:	Deselect all selected files\n\
%s  A-r%s%s:	Go to the root directory\n\
%s  A-e%s%s:	Go to the home directory\n\
%s  A-u%s%s:	Go up to the parent directory of the current working directory\n\
%s  A-j%s%s:	Go to the previous directory in the directory history \
list\n\
%s  A-k%s%s:	Go to the next directory in the directory history list\n\
%s  F10%s%s:	Open the configuration file\n\n\
NOTE: Depending on the terminal emulator being used, some of these \
keybindings, like A-e, A-f, and F10, might conflict with some of the \
terminal keybindings.\n"), 
		white, NC, default_color, 
		white, NC, default_color, 
		white, NC, default_color,
		white, NC, default_color,
		white, NC, default_color,
		white, NC, default_color,
		white, NC, default_color,
		white, NC, default_color,
		white, NC, default_color,
		white, NC, default_color,
		white, NC, default_color,
		white, NC, default_color,
		white, NC, default_color,
		white, NC, default_color,
		white, NC, default_color,
		white, NC, default_color,
		white, NC, default_color);
}

void
help_function (void)
{
	if (clear_screen)
		CLEAR;

	printf(_("%s %s (%s), by %s\n"), PROGRAM_NAME, VERSION, DATE, AUTHOR);

	printf(_("\nUsage: %s [-aAfFgGhiIlLoOsuUvx] [-p PATH] [-P PROFILE]\n\
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

	printf(_("\nColor codes: Run the 'colors' command to see the list \
of currently used color codes.\n\n"));

	printf(_("Commands: Run the 'commands' command to see the list \
of commands and the corresponding description.\n\n"));

	printf(_("The configuration and profile files allow you to customize \
colors, define some prompt commands and aliases, and more. For a full \
description consult the man page.\n"));
}

void
free_sotware(void)
{
	printf(_("Excerpt from 'What is Free Software?', by Richard Stallman. \
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
consider them all equally unethical (...)\"\n"));
}

void
version_function()
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
	xgetchar();	puts("");
}

void
bonus_function(void)
{
	static char state=0;
	if (state > 12)
		state=0;
	switch (state) {
		case 0:
			printf("\"Vamos Boca Juniors Carajo!\" (La mitad+1) \n");
		break;
		case 1:
			printf("\"Hey! Look behind you! A three-headed monkey!\" "
				   "(G. Threepweed)\n");
		break;
		case 2:
			printf("\"Free as in free speech, not as in free beer\" "
			"(R. M. S)\n");
		break;
		case 3:
			printf("\"Nothing great has been made in the world without "
				   "passion\" (G. W. F. Hegel)\n");
		break;
		case 4:
			printf("\"Simplicity is the ultimate sophistication\" "
				  "(Leo Da Vinci)\n");
		break;
		case 5:
			printf("\"Yo vendí semillas de alambre de púa, al contado, y "
				   "me lo agradecieron\" (Marquitos, 9 Reinas)\n");
		break;
		case 6:
			printf("\"I'm so happy, because today I've found my friends, "
				   "they're in my head\" (K. D. Cobain)\n");
		break;
		case 7:
			printf("\"The best code is written with the delete key\" "
				   "(Someone, somewhere, sometime)\n");
		break;
		case 8:
			printf("\"I'm selling these fine leather jackets\" (Indy)\n");
		break;
		case 9:
			printf("\"I pray to God to make me free of God\" "
				   "(Meister Eckhart)\n");
		break;
		case 10:
			printf("Truco y quiero retruco mierda!\n");
		break;
		case 11:
			printf("The only truth is that there is no truth\n");
		break;
		case 12:
			printf("\"This is a lie\" (The liar paradox)\n");
		break;
	}
	state++;
}
