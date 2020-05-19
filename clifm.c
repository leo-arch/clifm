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
 * to correctly get the amount of characters that these filenames contain. 
 * This is mainly because of how strlen() works and how non-ASCII chars are
 * construed: strlen() counts bytes, not chars. Now, since ASCII chars take 
 * each 1 byte, the amount of bytes equals the amount of chars. However, 
 * non-ASCII chars are multibyte chars, that is, one char takes more than 1 
 * byte, and this is why strlen() does not work as expected for this kind of 
 * chars. 
 * To fix this issue I should replace the two calls to strlen() in list_dir() 
 * by a custom implementation able to correctly get the length of non-ASCII 
 * (UTF-8) strings: u8_xstrlen(). Take a look at get_properties() as well, 
 * just as to any function depending on strlen() to print file names */

/* A note about my trash implementation: CliFM trash system can perfectly read 
 * and undelete files trashed by another trash implementation (I've tried 
 * Thunar via gvfs), but not the other way around: Thunar sees files trashed 
 * by CliFM, but it cannot undelete them (it says that it cannot determine the 
 * original path for the trashed file, though the corresponding trashinfo file 
 * exists and is correctly written). I think Thunar is doing something else 
 * besides the trashinfo file when trashing a file. */

/** 

							###############
							#  TODO LIST  #
							###############
*/
/*
 ** Add ranges to deselect and undel functions.
 ** Check compatibility with BSD Unixes.
 ** Add a help option for each internal command. Make sure this help system is
	consistent accross all commands: if you call help via the --help option, 
	do it this way for all commands. Try to unify the usage and description
	notice so that the same text is used for both the command help and the
	general help. I guess I should move the help for each command to a 
	separate function.
 ** A pager for displaying help would be great.
 ** Customizable keybindings would be a great feature!
 ** Take a look at the OFM (Orthodox File Manager) specification:
	http://www.softpanorama.org/OFM/Standards/ofm_standard1999.shtml
	The Midnight Commander could be a good source of ideas.
 ** Pay attention to the construction of error messages: they should be enough
	informative to allow the user/developer to track the origin of the error.
	This line, for example, is vague: 
	  fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
	Use __func__ or some string to help identifying the problem:
	  fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, __func__, strerror(errno));
	If a file or path is involved, add it to the message as well. If possible,
	follow this pattern: program: function: sub-function: file/path: message
 ** Make sure all error messages are visible: some of them will be quickly left
	behind by files listing. Pay special attention to errors in main() and 
	init_config(). Adding a "Press any key to continue" after 
	error messages will do the job.
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
 ** When using strncpy(dest, n, src), pay attention to this: if there is no 
	null byte among the 'n' bytes of 'src', the string placed in 'dest' won't 
	be null terminated.
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
 ** The glob, range, and braces expansion functions will expand a fixed amount 
	of non-expanded strings (10). I should use a dynamic array of integers 
	instead. However, 10 should be more than enough: this is not a shell!
 ** glob() can do brace expansion!!
 ** Rewrite 'ds *'. Since wildcards are now expanded before exec_cmd(), that is,
    before reaching the deselect function, what it gets instead is a bunch of 
    selected filenames. WORKAROUND: I replaced 'ds *' by 'ds a'.
 ** Check all functions for error, for example, chdir().
 ** run_in_foreground() returns now the exit status of the child. Use this 
    whenever needed.
 ** Replace system() by launch_execle(). Done only for external commands, rm, 
    aliases, and edit_function. Check open_function() and
	bookmarks_function().
 ** There's a function for tilde expansion called wordexp().
 ** Only global variables are automatically initialized by the compiler; local 
    variables must be initialized by the programer. Check there are no 
    uninitialized local variables.
 ** The list-on-the-fly check is not elegant at all! It's spread all around 
	the code. I'm not sure if there's an alternative to this: I have to prevent 
    listdir() from running, and it is this function itself which is spread all 
    around the code. I cannot make the on-the-fly test inside the dirlist 
    function itself, since the function is preceded by the free_dirlist 
    function, which frees the dirlist array to be filled again by listdir(). 
    So, this freeing must also be prevented, and thus, the check must be 
    previous to the call to listdir().
 ** Set a limit, say 100, to how much directories will the back function 
	recall.
 ** When refreshing the screen, there's no need to reload the whole array of 
    elements. Instead, I should simply print those elements again, wihtout 
    wasting resources in reloading them as if the array were empty. Perhaps I 
    could use some flag to do this.
 ** When creating a variable, make sure the new variable name doesn't exist. 
	If it does, replace the old position of the variable in the array of user 
    variables by the new one in order not to unnecessarily take more memory 
    for it.
 ** Add an unset function to remove user defined variables. The entire array 
	of user variables should be reordered every time a vairable is unset.
 ** The __func__ variable returns the name of the current function. It could 
	be useful to compose error logs.
 ** At the end of prompt() I have 7 strcmp. Try to reduce all this to just one 
    operation.
 ** THE STACK (FIXED ARRAYS) IS MUCH MORE FASTER THAN THE HEAP (DYNAMIC ARRAYS)!! 
    If a variable is only localy used, allocate it in the stack.
 ** Take a look at the capabilities() function.
 ** Add some keybinds, for instance, C-v to paste (see test3.c). Other 
	posibilities: folders first, case sensitive listing, hidden files, etc.
 ** Possible null pointer dereference (path_tmp) in get_path_env(). Same in 
    prompt() with path_tilde and short_path (using cppcheck).
 ** When the string is initialized as NULL, there's no need for a calloc before 
    the reallocation.
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
 **	Add support for wildcards and nested braces to the BRACE EXPANSION function, 
    like in Bash. 
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
	(SOLVED) d) -Wformat-truncation=2: some printf outputs may be truncated due 
	to wrong sizes.
	(SOLVED) e) -Wformat-y2k: The strftime manpage says we don't need to worry 
	about these warnings. It recommends indeed to use -Wno-format-y2k instead to 
	avoid these warnings.
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
 ** The logic of bookmarks and copy functions is crap! Rewrite it.
 ** Add the possibility to open files in the background when no application has 
    been passed (ex: o xx &). DONE, but now the pid shown is that of xdg-open, 
    so that when the user tries to kill that pid only xdg-open will be killed. 
    Find a way to kill xdg-open AND its child, OR make it show not xdg-open's 
    pid, but the program's instead. Drawback: xdg-open would remain alive.
 ** Check ALL the exec commands for: 1 - terminal control; 2 - signal handling.
 ** Replace foreground running processes (wait(NULL)) by run_in_foreground()
 ** Check size_t vs int in case of numeric variables. Calloc, malloc, strlen, 
    among others, dealt with size_t variables; In general, use size_t for sizes. 
    It is also recommended to use this variable type for array indexes. Finally, 
    if you compare or asign a size_t variable to another variable, this latter 
    should also be defined as size_t (or casted as unisgned [ex: (unsigned)var]). 
    Note: for printing a size_t (or pid_t) variable, use %ld (long double).
    Note2: size_t does not take negative values.
 ** Check the type of value returned by functions. Ex: strlen returns a size_t 
    value, so that in var=strlen(foo), var should be size_t.
 ** Check that, whenever a function returns, the variables it malloc(ed) are
	freed.
 ** Use remove() to delete files and empty directories whose name contains no 
    spaces.
 ** Be careful when removing symlinks. I remove them with 'rm -r', but perhaps 
	I should use the C unlink() function instead, for rm -r might remove those 
	files pointed by the symlink.
 ** Scalability/stress testing: Make a hard test of the program: big files 
    handling, large amount of files, heavy load/usage, etc. 
 ** Destructive testing: Try running wrong commands, MAKE IT FAIL! Otherwise, 
	it will never leave the beta state.


 * (DONE) Add ELN auto-expansion. Great!!!!
 * (DONE) Fix file sizes (from MB to MiB)
 * (DONE) The properties function prints "linkname: No such file or directory"
	in case of a broken symlink, which is vague: the link does exists; what 
	does not is the linked file. So, It would be better to write the error
	message as follows: "linkname -> linked_filename: No such file..."
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
 * (DONE) Add color for 'file with capability' (???) (\033[0;30;41). Some 
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
    line.
 ** 2 - When jumping from one directory to another with the 'b/f !ELN' command,
	for example, from 1 to 4 in the directories list, 'back' does not go back
	to 1, but to 3. This is not really a bug, but it feels ugly and 
	unintuitive.
 ** 3 - I'm using MAX_LINE (255) length for getting lines from files with 
	fgets(). A log line, for example, could perfectly contain a PATH_MAX (4096) 
	path, plus misc stuff, so that MAX_LINE is clearly not enough. I should 
	try the GNU getline() instead, which takes care itself of allocating 
	enough bytes to hold the entire line.
 ** 4 - If the user's home cannot be found, CliFM will attempt to create its
	config files in PWD/clifm_emergency. Now, if CWD is not writable, no option
	will be set, and CliFM will become very usntable. In this case I should 
	prevent the program from writing or reading anything and simply set the
	defaults.

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
	NAME_MAX-(suffix+1), adding the suffix to it will produce a filename 
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
	after running the history command. Otherwise, the comm array, which depends
	args_n, won't be correctly freed.
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
	exit.
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
 * (SOLVED) Do "cd path/to/dir/*" and CRASH!
 * (SOLVED) Specify a file (not a dir) as destiny of the 'paste sel' command, 
	and CRASH!!! 
 * (SOLVED) This command: "sel /usr/local/sbin/*" doesn't work, for it produces 
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


/* #define _POSIX_C_SOURCE 200809L  */
/* "if you define _GNU_SOURCE, then defining either _POSIX_SOURCE or 
 * _POSIX_C_SOURCE as well has no effect". If I define this macro without 
 * defining _GNU_SOURCE macro as well, the macros 'S_IFDIR' and the others 
 * cannot be used */
#define _GNU_SOURCE /* I need this macro to enable 
program_invocation_short_name variable and asprintf() */
#ifndef __linux__ /* && !defined(linux) && !defined(__linux) 
&& !defined(__gnu_linux__) */
/* What about unixes like BSD?*/
/* #ifndef __linux__
	while 'linux' is deprecated, '__linux__' is recommended */
	fprintf(stderr, "%s: This program runs only on GNU/Linux\n", PROGRAM_NAME); /* Get a real OS man! */
	exit(EXIT_FAILURE);
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
					fgetc, fputc, perror, rename, sscanf */
#include <string.h> /* str(n)cpy, str(n)cat, str(n)cmp, strlen, strstr, memset */
#include <stdlib.h> /* getenv, malloc, calloc, free, atoi, realpath, 
					EXIT_FAILURE and EXIT_SUCCESS macros */
#include <dirent.h> /* scandir */
#include <unistd.h> /* sleep, readlink, chdir, symlink, access, exec, isatty, 
					* setpgid, getpid, getuid, gethostname, tcsetpgrp, 
					* tcgetattr, char **__environ, STDIN_FILENO, STDOUT_FILENO, 
					* and STDERR_FILENO macros */
#include <sys/stat.h> /* stat, lstat, mkdir */
//#include <sys/types.h> */
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
/* #include <bsd/string.h> // strlcpy, strlcat */
/* #include "clifm.h" */
#include <fcntl.h> /* O_RDONLY, O_DIRECTORY, and AT_* macros */
#include <sys/syscall.h> /* SYS_* and __NR_* macros for syscall() */
#include <linux/fs.h> /* FS_IOC_GETFLAGS, S_IMMUTABLE_FL macros */
#include <libintl.h> /* gettext */

#define PROGRAM_NAME "CliFM"
#define PNL "clifm" /* Program name lowercase */
/* #define CONF_PATHS_MAX 68
 * According to useradd manpage, the max lenght of a username is 32. So, 
 * "/home/" (6) + 32 + "/.config/clifm/bookmarks.cfm" (28) + terminating 
 * null byte (1) == 67. This is then the max length I need for config dirs 
 * and files */
#define MAX_LINE 256
#define TMP_DIR "/tmp/clifm"
/* If no formatting, puts (or write) is faster than printf */
#define CLEAR puts("\033c")
/* #define CLEAR write(STDOUT_FILENO, "\033c", 3) */
#define VERSION "0.12.1.2"
#define AUTHOR "L. Abramovich"
#define CONTACT "johndoe.arch@outlook.com"
#define DATE "May 18, 2020"

/* Define flags for program options and internal use */
/* Variable to hold all the flags (int == 4 bytes == 32 bits == 32 flags). In
 * case more flags are needed, use a long double variable (16 bytes == 128 
 * flags) and even several of these */
static int flags;
/* options flags */
#define FOLDERS_FIRST 	(1 << 1) /* 4 dec, 0x04 hex, 00000100 binary */
#define HELP			(1 << 2) /* and so on... */
#define HIDDEN 			(1 << 3)
#define ON_THE_FLY 		(1 << 4)
#define SPLASH 			(1 << 5)
#define CASE_SENS 		(1 << 6)
#define START_PATH 		(1 << 7)
#define PRINT_VERSION	(1 << 8)
/* Internal flags */
#define ROOT_USR		(1 << 9)
#define EXT_HELP		(1 << 10)
#define XDG_OPEN_OK		(1 << 11)
#define GRAPHICAL		(1 << 12)
#define IS_USRVAR_DEF	(1 << 13) /* 18 dec, 0x12 hex, 00010010 binary */

/* Used by log_msg() to know wether to tell prompt() to print messages or 
 * not */
#define PRINT_PROMPT 1
#define NOPRINT_PROMPT 0

/* Error codes, to be used by exec() */
#define EXNULLERR 79
#define EXEXECERR 80
#define EXFORKERR 81
#define EXCRASHERR 82
#define EXWAITERR 83

/* ###COLORS### */
/* \001 and \002 tell readline that color codes between them are non-printing 
 * chars. This is specially useful for the prompt, i.e., when passing color 
 * codes to readline */

#define blue "\033[1;34m"
#define d_blue "\033[0;34m"
#define green "\033[1;32m"
#define d_green "\033[0;32m"
#define gray "\033[1;30m"
/* #define char d_gray "\033[0;30m" */
#define white "\033[1;37m"
/* #define char d_white "\033[0;37m" */
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
/* Colors for the prompt: */
#define red_b "\001\033[1;31m\002" /* error log indicator */
#define green_b "\001\033[1;32m\002" /* sel indicator */
#define yellow_b "\001\033[0;33m\002" /* trash indicator */
/* #define d_red_b "\001\033[0;31m\002"
#define d_cyan_b "\001\033[0;36m\002" */
#define NC_b "\001\033[0m\002"
/* NOTE: Do not use \001 prefix and \002 suffix for colors list: they produce 
 * a displaying problem in lxterminal (though not in aterm and xterm). */

/* Replace some functions by my custom (faster, I think: NO!!) 
 * implementations. */
#define strlen xstrlen /* All calls to strlen will be replaced by a call to 
xstrlen */
#define strcpy xstrcpy
#define strncpy xstrncpy 
#define strcmp xstrcmp  
#define strncmp xstrncmp
#define atoi xatoi
//#define alphasort xalphasort
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
	if (!str1 || !str2) return 256;
	size_t counter=0;
	while(*str1 && counter < n) {
		if (*str1 != *str2)
			return *str1-*str2;
		str1++;
		str2++;
		counter++;
	}
	if (counter == n) return 0;
	if (*str2) return 0-*str2;
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
	*buf='\0';
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
	*buf='\0';
	return buf;
}

size_t
xstrlen(const char *str)
{
	size_t len=0;
	while (*(str++)) /* Same as: str[len] != '\0' */
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

char *
quote_str(char *str)
/* Quote a given string if it contains spaces. Use double quotes if no double
 * quote is found in STR, or single quotes otherwise. Returns the quoted
 * string or NULL if str is NULL, empty, contains no spaces, or in case of 
 * memory allocation error (calloc) */
{
	if (!str || str[0] == '\0') 
		return NULL;

	char *str_b=str, is_space=0, double_quote=0;
	/* Check for spaces and double quotes */
	while (*str_b) {
		if (*str_b == 0x20) /* space */
			is_space=1;
		else if (*str_b == 0x22) /* double quote */
			double_quote=1;
		str_b++;
	}
	str_b=NULL;
	
	if (!is_space)
		return NULL;

	size_t str_len=strlen(str);
	char *buf=NULL, *p=calloc(str_len+3, sizeof(char *));
	if (!p)
		return NULL;
	buf=p;
	p=NULL;
	if (double_quote)
		sprintf(buf, "'%s'", str);
	else
		sprintf(buf, "\"%s\"", str);
	return buf;
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
	char *p=calloc(user_len+1, sizeof(char *));
	if (!p)
		return NULL;
	char *buf=p;
	p=NULL;
	strncpy(buf, pw->pw_name, user_len+1);
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
	char *p=calloc(home_len+1, sizeof(char *));
	if (!p)
		return NULL;
	char *home=p;
	p=NULL;
	strncpy(home, user_info->pw_dir, home_len+1);
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

char *
handle_spaces(const char *str)
/* Remove leading, terminating, and double spaces from str. Returns NULL if 
 * str: a) is NULL; b) contains only spaces; c) is empty; d) there was a 
 * memory allocation failure */
{
	/* If str is NULL or zero length */
	if (!str || strlen(str) == 0) return NULL;
	char *f_str=NULL;
	int first_non_space=0, counter=0;
	/* if I use while(*str), I need to increment str at the end of the loop */
	for (;*str;str++) {
		if (*str != 0x20)
			first_non_space=1;
		if (first_non_space) {
			if (*str != 0x20 || (*str == 0x20 && *(str+1) != 0x20 && 
			*(str+1) != 0x00)) {
				counter++;
				char *p=realloc(f_str, counter*sizeof(char *));
				if (p) {
					f_str=p;
					p=NULL;
					f_str[counter-1]=*str;
				}
				else { /* If memory allocation failure */
					if (f_str)
						free(f_str);
					return NULL;
				}
			}
		}
	}
	if (!counter) /* If only spaces */
		return NULL;
	f_str[counter]=0x00; /* Add a terminating null char */
	return f_str;
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
			char *p=calloc(str_len-counter, sizeof(char *));
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
	char *p=calloc(str_len, sizeof(char *));
	if (!p)
		return NULL;
	buf=p;
	p=NULL;
	strcpy(buf, str);
/*	if (index != str_len-1) // Why? This check shouldn't be here */
		buf[index]='\0';
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
	f_str[j]='\0';
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
	if (!str || str[0] == '\0')
		return NULL;
	char *p=calloc((strlen(str)*3)+1, sizeof(char *));
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
	if (!str || str[0] == '\0')
		return NULL;
	char *p=calloc(strlen(str)+1, sizeof(char *));
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
	char *p=calloc(128, sizeof(char *)), *date;
	if (p) {
		date=p;
		p=NULL;
	}
	else
		return NULL;
	strftime(date, 128, "%a, %b %d, %Y, %T", tm);
	return date;
}

char *
get_size_unit(off_t file_size)
/* Convert FILE_SIZE to human readeable form */
{
	char *size_type=calloc(20, sizeof(char *));
	short units_n=0;
	float size=file_size;
	while (size > 1024) {
		size=size/1024;
		++units_n;
	}
	/* If bytes */
	if (!units_n)
		sprintf(size_type, "%.0f bytes", size);
	else {
		static char units[]={ 'b', 'K', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y' };		
		sprintf(size_type, "%.1f%ciB", size, units[units_n]);
	}
	return size_type;
}

char **get_substr(char *str, const char ifs)
/* Get all substrings from str using IFS as substring separator. Returns an 
 * array containing all substrings in STR or NULL if: STR is NULL or empty, 
 * STR contains only IFS(s), or in case of memory allocation error */
{
	if (!str || *str == '\0')
		return NULL;
	char **substr=NULL;
	void *p=NULL;
	size_t str_len=strlen(str);
	char buf[str_len+1];
	memset(buf, 0x00, str_len+1);
	size_t length=0, substr_n=0;
	while (*str) {
		while (*str != ifs && *str != '\0' && length < sizeof(buf))
			buf[length++]=*(str++);
		if (length) {
			buf[length]='\0';
			p=realloc(substr, (substr_n+1)*sizeof(char **));
			if (!p) {
				/* Free whatever was allocated so far */
				for (size_t i=0;i<substr_n;i++)
					free(substr[i]);
				free(substr);
				return NULL;
			}
			substr=p;
			p=calloc(length, sizeof(char *));
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
	p=realloc(substr, sizeof(char **)*(substr_n+1));
	if (!p) {
		for (size_t i=0;i<substr_n;i++)
			free(substr[i]);
		free(substr);
		return NULL;
	}
	substr=p;
	p=NULL;
	substr[substr_n]=NULL;
	return substr;
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
char *filenames_generator(const char *text, int state);
char *bin_cmd_generator(const char *text, int state);
void get_path_programs(void);
int get_path_env(void);
void get_sel_files(void);
void init_config(void);
void exec_profile(void);
void external_arguments(int argc, char **argv);
void get_aliases_n_prompt_cmds(void);
void check_log_file_size(char *log_file);
char **check_for_alias(char **comm);
void get_history(void);
void add_to_dirhist(const char *dir_path);
char *home_tilde(const char *new_path);
char **parse_input_str(const char *str);
void list_dir(void);
char *prompt(void);
void exec_cmd(char **comm);
int launch_execve(char **cmd);
int launch_execle(const char *cmd);
char *get_cmd_path(const char *cmd);
int brace_expansion(const char *str);
int run_in_foreground(pid_t pid);
void run_in_background(pid_t pid);
void copy_function(char **comm);
void run_and_refresh(char **comm);
void properties_function(char **comm);
void get_properties(char *filename, int _long, int max);
int save_sel(void);
void sel_function(char **comm);
void show_sel_files(void);
void deselect(char **comm);
void search_function(char **comm);
void bookmarks_function(void);
char **get_bookmarks(char *bookmarks_file);
char **bm_prompt(void);
int count_dir(const char *dir_path);
char *rl_no_hist(const char *prompt);
void log_function(char **comm);
void history_function(char **comm);
void run_history_cmd(const char *cmd);
void edit_function(char **comm);
void colors_list(const char *entry, const int i, const int pad, 
				 const int new_line);
void hidden_function(char **comm);
void help_function(void);
void color_codes(void);
void list_commands(void);
int folder_select(const struct dirent *entry);
int file_select(const struct dirent *entry);
int skip_implied_dot(const struct dirent *entry);
void version_function(void);
void bonus_function(void);
char *parse_usrvar_value(const char *str, const char c);
void create_usr_var(char *str);
void readline_kbinds();
int readline_kbind_action (int count, int key);
void surf_hist(char **comm);
void trash_function(char **comm);
int trash_element(const char *suffix, struct tm *tm, char *file);
int wx_parent_check(char *file);
void remove_from_trash (void);
void untrash_function(char **comm);
void untrash_element(char *file);
void trash_clean(void);
int recur_perm_check(const char *dirname);
int *expand_range(char *str);
void log_msg(char *msg, int print);
void keybind_exec_cmd(char *str);
int get_max_long_view(void);
void list_mountpoints(void);
void back_function(char **comm);
void forth_function(char **comm);
void cd_function(char *new_path);
void open_function(char **cmd);
size_t u8_xstrlen(const char *str);
void print_license(void);
void free_sotware(void);

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

/* ###GLOBAL VARIABLES#### */
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
* declared, and loses their value once the function ended. The same goes to 
* for the block variables, only that these variables only work within the 
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
	clear_screen=-1, shell_terminal=0, pager=-1, no_log=0, 
	shell_is_interactive=0, list_folders_first=-1, case_sensitive=-1, 
	cd_lists_on_the_fly=-1, recur_perm_error_flag=0, is_sel=0, sel_is_last=0, 
	sel_no_sel=0, print_msg=0, long_view=-1, kbind_busy=0, error_msg=0, 
	warning_msg=0, notice_msg=0, unicode=-1, cont_bt=0;
/* A short int accepts values from -32,768 to 32,767, and since all these 
 * variables will take -1, 0, and 1 as values, a short int is more than 
 * enough. Now, since short takes 2 bytes of memory, using int for these 
 * variables would be a waste of 2 bytes (since an int takes 4 bytes). I can 
 * even use char (or signed char), which takes only 1 byte (8 bits) and 
 * accepts negative numbers (-128 -> +127).
* NOTE: If the value passed to a variable is bigger than what the variable can 
* hold, the result of a comparison with this variable will always be true */

int files=0, args_n=0, sel_n=0, max_hist=-1, max_log=-1, path_n=0, 
	current_hist_n=0, dirhist_total_index=0, dirhist_cur_index=0, 
	argc_bk=0, usrvar_n=0, aliases_n=0, prompt_cmds_n=0, trash_n=0, 
	msgs_n=0, longest=0, term_cols=0, bm_n=0;
	/* -1 means non-initialized */
size_t user_home_len=0;
struct termios shell_tmodes;
pid_t own_pid=0;
struct dirent **dirlist=NULL;
struct usrvar_t *usr_var=NULL;
char *user=NULL, *path=NULL, **old_pwd=NULL, **sel_elements=NULL, 
	*sel_file_user=NULL, **paths=NULL, **bin_commands=NULL, **history=NULL, 
	*xdg_open_path=NULL, **braces=NULL, prompt_color[19]="", white_b[23]="", 
	text_color[23]="", **prompt_cmds=NULL, **aliases=NULL, **argv_bk=NULL, 
	*user_home=NULL, **messages=NULL, *msg=NULL, *CONFIG_DIR=NULL, 
	*CONFIG_FILE=NULL, *BM_FILE=NULL, hostname[HOST_NAME_MAX]="", 
	*LOG_FILE=NULL, *LOG_FILE_TMP=NULL, *HIST_FILE=NULL, *MSG_LOG_FILE=NULL, 
	*PROFILE_FILE=NULL, *TRASH_DIR=NULL, *TRASH_FILES_DIR=NULL, 
	*TRASH_INFO_DIR=NULL, 
	*INTERNAL_CMDS[]={ "alias", "open", "prop", "back", "forth",
		"move", "paste", "sel", "selbox", "desel", "refresh", 
		"edit", "history", "hidden", "path", "help", "commands", 
		"colors", "version", "license", "splash", "folders first", 
		"exit", "quit", "pager", "trash", "undel", "messages", 
		"mountpoints", "bookmarks", "log", "untrash", "unicode", NULL };

/* ###MAIN#### */
int
main(int argc, char **argv)
{
	/* ###BASIC CONFIG AND VARIABLES###### */

	/* Use the locale specified by the environment. By default (ANSI C), 
	 * all programs start in the standard 'C' (== 'POSIX') locale. This 
	 * function makes the program portable to all locales (See man (3) 
	 * setlocale). It affects characters handling functions like toupper(),
	 * tolower(), alphasort() (since it uses strcoll()), and strcoll() itself,
	 * among others. Here, it's important to correctly  sort filenames 
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
	argv_bk=xcalloc(argv_bk, (size_t)argc, sizeof(char **));
	for (size_t i=0;i<(size_t)argc;i++) {
		argv_bk[i]=xcalloc(argv_bk[i], strlen(argv[i])+1, sizeof(char *));
		strcpy(argv_bk[i], argv[i]);
	}

	/* Get user's home directory */
	/* In case of failure, set a temporary home: cwd/clifm_emeregency, and 
	 * print the error message */
	user_home=get_user_home();
	if (!user_home) {
		char cwd[PATH_MAX]="";
		if (getcwd(cwd, sizeof(cwd)) == NULL) {
			asprintf(&msg, "%s: getcwd: %s\n", PROGRAM_NAME, 
					 strerror(errno));
			/* Tell the prompt not to print error message, since it will be 
			 * printed here */
			 if (msg) {
				log_msg(msg, NOPRINT_PROMPT);
				free(msg);
			}
			else
				fprintf(stderr, "%s: getcwd: %s\n", PROGRAM_NAME, 
						strerror(errno));
			atexit(free_stuff);
		}
		size_t cwd_len=strlen(cwd), pnl_len=strlen(PNL);
		user_home=xcalloc(user_home, cwd_len+pnl_len+12, sizeof(char *));
		snprintf(user_home, cwd_len+pnl_len+12, "%s/%s_emergency", cwd, PNL);
		asprintf(&msg, _("%s: '%s': Warning! Failure getting the home \
directory. Using '%s' as an emergency home\n"), PROGRAM_NAME, __func__, 
				 user_home);
		if (msg) {
			error_msg=1;
			log_msg(msg, PRINT_PROMPT);
			free(msg);
		}
		else
			fprintf(stderr, _("%s: '%s': Warning! Failure getting the home \
directory. Using '%s' as an emergency home\n"), PROGRAM_NAME, __func__, 
					user_home);
	}
	user_home_len=strlen(user_home);

	/* Get user name */
	user=get_user();
	if (!user) {
		user=xcalloc(user, 4, sizeof(char *));
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

	/* Initialize program paths and files, set options from the config file, 
	 * and load sel elements, if any. All these configurations are made per 
	 * user basis */
	init_config();
	get_aliases_n_prompt_cmds();
	get_sel_files();
	trash_n=count_dir(TRASH_FILES_DIR);
	if (trash_n <= 2) trash_n=0;

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
	
	/* Manage external arguments, but only if any: argc == 1 equates to no 
	 * argument, since this '1' is just the program invokation name. External 
	 * arguments will override initialization values (init_config) */
	if (argc > 1)
		external_arguments(argc, argv);
	exec_profile();	
	
	/* Limit the log files size */
	check_log_file_size(LOG_FILE);
	check_log_file_size(MSG_LOG_FILE);
	
	/* Get history */
	struct stat file_attrib;
	if (stat(HIST_FILE, &file_attrib) == 0 && file_attrib.st_size != 0) {
	/* If the size condition is not included, and in case of a zero size file, 
	 * read_history() gives malloc errors */
		/* Recover history from the history file */
		read_history(HIST_FILE); /* This line adds more leaks to readline */
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
			/* To avoid malloc errors in read_history(), do not create an 
			 * empty file */
			fprintf(hist_fp, "edit\n");
			/* There is no need to run read_history() here, since the history 
			 * file is still empty */
			fclose(hist_fp);
		}
	}
	
	/* Store history into an array to be able to manipulate it */
	get_history();

	/* Check whether xdg-open is available */
	xdg_open_check();

	/* Enable tab auto-completion for commands (in PATH) in case of first 
	 * entered string. The second and later entered string will be 
	 * autocompleted with filenames instead, just like in Bash */
	rl_attempted_completion_function=my_rl_completion;
	if (splash_screen) {
		splash();
		CLEAR;
	}

	/* Initialize the keyboard bindings function */
	readline_kbinds();

	/* If path was not set (neither in the config file nor via command 
	 * line, that is, as external argument), set the default (PWD), and
	 * if PWD is not set, use the user's home directory. Bear in mind that 
	 * if you launch CliFM through a terminal emulator, say xterm 
	 * (xterm -e clifm), xterm will run a shell, say bash, and the shell 
	 * will read its config file. Now, if this config file changes the PWD, 
	 * this will the PWD for CliFM */
	if (!path) {
		char pwd[PATH_MAX];
		memset(pwd, 0x00, PATH_MAX);
		strncpy(pwd, getenv("PWD"), PATH_MAX);
		if (!pwd || strlen(pwd) == 0) {
			path=xcalloc(path, strlen(user_home)+1, sizeof(char *));
			strcpy(path, user_home);
		}
		else {
			path=xcalloc(path, strlen(pwd)+1, sizeof(char *));
			strcpy(path, pwd);		
		}
	}

	/* Make path the CWD */
	/* If chdir() fails, set path to cwd, list files and print the error 
	 * message */
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
			asprintf(&msg, _("%s: Fatal error! Failed retrieving current \
working directory\n"), PROGRAM_NAME);
			if (msg) {
				log_msg(msg, NOPRINT_PROMPT);
				free(msg);
			}
			else
				fprintf(stderr, _("%s: Fatal error! Failed retrieving current \
working directory\n"), PROGRAM_NAME);
			atexit(free_stuff);
		}
		if (path)
			free(path);
		path=xcalloc(path, strlen(cwd)+1, sizeof(char *));
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

	/* ###MAIN PROGRAM LOOP#### */
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
	
	return 0; /* Never reached */
}

/* ###FUNCTIONS DEFINITIONS### */

void print_license(void)
{
	printf(_("%s, 2017-19, %s\n\n\
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
		   PROGRAM_NAME, AUTHOR);
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

void open_function(char **cmd)
{
	if (!cmd)
		return;
	/* If no argument or first argument is "--help" */
	if (!cmd[1] || strcmp(cmd[1], "--help") == 0) {
		puts(_("Usage: open ELN/filename [application] [&]"));
		return;
	}
	
	/* Check file existence */
	struct stat file_attrib;
	if (lstat(cmd[1], &file_attrib) == -1) {
		fprintf(stderr, "%s: open: '%s': %s\n", PROGRAM_NAME, cmd[1], 
				strerror(errno));
		return;
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
			linkname=realpath(cmd[1], NULL);
			if (!linkname) {
				if (errno == ENOENT)
					fprintf(stderr, _("%s: open: '%s': Broken link\n"), 
							PROGRAM_NAME, cmd[1]);
				else
					fprintf(stderr, "%s: open: '%s': %s\n", PROGRAM_NAME,
							cmd[1], strerror(errno));
				return;
			}
			if (stat(linkname, &file_attrib) == -1) {
				/* Never reached: if linked file does not exist, realpath()
				 * returns NULL */
				fprintf(stderr, "%s: open: '%s -> %s': %s\n", PROGRAM_NAME, 
						cmd[1], linkname, strerror(errno));
				free(linkname);
				return;
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
Cannot open file. Try 'application filename'.\n"), PROGRAM_NAME, cmd[1], 
					linkname, filetype);
		else
			fprintf(stderr, _("%s: '%s' (%s): Cannot open file. Try \
'application filename'.\n"), PROGRAM_NAME, cmd[1], filetype);
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
				return;				
			}
			/* If xdg-open exists */
			pid_t pid_open=fork();
			if (pid_open == 0) {
				set_signals_to_default();
				execle(xdg_open_path, "xdg-open", 
					   (is_link) ? file_tmp : cmd[1], NULL, __environ);
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
						   (is_link) ? file_tmp : cmd[1], NULL, __environ);
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
				return;
			}
		}
	}
}

void cd_function(char *new_path)
{
	int ret=-1;
	char buf[PATH_MAX]=""; /* Temporary store new_path */
	/* If "cd" */
	if (!new_path || new_path[0] == '\0') {
		ret=chdir(user_home);
		if (ret == 0)
			strncpy(buf, user_home, PATH_MAX);
	}
	else {
		/* If "cd --help" */
		if (strcmp(new_path, "--help") == 0) {
			puts(_("Usage: cd [ELN/directory]"));
			return;
		}
		/* If "cd ." or "cd .." or "cd ../..", etc */
		if (strcmp(new_path, ".") == 0 || strncmp(new_path, "..", 2) == 0) {
			char *real_path=realpath(new_path, NULL);
			if (!real_path) {
				fprintf(stderr, "%s: cd: '%s': %s\n", PROGRAM_NAME, new_path,
						strerror(errno));
				return;
			}
			ret=chdir(real_path);
			if (ret == 0)
				strncpy(buf, real_path, PATH_MAX);
			free(real_path);
		}
		/* If absolute path */
		else if (new_path[0] == '/') {
			ret=chdir(new_path);
			if (ret == 0)
				strncpy(buf, new_path, PATH_MAX);
		}
		/* If relative path, add CWD to the beginning of new_path, except if 
		 * CWD is root (/). Otherwise, the resulting path would be "//path" */
		else {
			char tmp_path[PATH_MAX]="";
			snprintf(tmp_path, PATH_MAX, "%s/%s", 
					 (path[0] == '/' && path[1] == '\0') ? "" : path, 
					 new_path);
			ret=chdir(tmp_path);
			if (ret == 0)
				strncpy(buf, tmp_path, PATH_MAX);
		}
	}
	
	/* If chdir() was successful */
	if (ret == 0) {
		free(path);
		path=xcalloc(path, strlen(buf)+1, sizeof(char *));
		strcpy(path, buf);
		add_to_dirhist(path);
		if (cd_lists_on_the_fly) {
			while (files--) free(dirlist[files]);
			list_dir();
		}
	}
	else /* If chdir() failed */
		fprintf(stderr, "%s: cd: '%s': %s\n", PROGRAM_NAME, new_path, 
				strerror(errno));
}

void back_function(char **comm)
/* Go back one element in dir hist */
{
	if (comm[1]) {
		if (strcmp(comm[1], "--help") == 0)
			puts(_("Usage: back [h, hist] [clear] [!ELN]"));
		else
			surf_hist(comm);
		return;
	}
	/* If just 'back', with no arguments */
	/* If first path in current dirhist was reached, do nothing */
	if (dirhist_cur_index <= 0) {
/*		dirhist_cur_index=dirhist_total_index; */
		return;
	}

	int ret=chdir(old_pwd[dirhist_cur_index-1]);
	if (ret == 0) {
		free(path);
		path=xcalloc(path, strlen(old_pwd[dirhist_cur_index-1])+1, 
					 sizeof(char *));
		strcpy(path, old_pwd[--dirhist_cur_index]);
		if (cd_lists_on_the_fly) {
			while (files--) free(dirlist[files]);
			list_dir();
		}
	}
	else
		fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, 
				old_pwd[dirhist_cur_index-1], strerror(errno));
}

void forth_function(char **comm)
/* Go forth one element in dir hist */
{
	if (comm[1]) {
		if (strcmp(comm[1], "--help") == 0)
			puts(_("Usage: forth [h, hist] [clear] [!ELN]"));
		else
			surf_hist(comm);
		return;
	}
	/* If just 'forth', with no arguments */
	/* If last path in dirhist was reached, do nothing */
	if (dirhist_cur_index+1 >= dirhist_total_index)
		return;
	dirhist_cur_index++;

	int ret=chdir(old_pwd[dirhist_cur_index]);
	if (ret == 0) {
		free(path);
		path=xcalloc(path, strlen(old_pwd[dirhist_cur_index])+1, 
					 sizeof(char *));
		strcpy(path, old_pwd[dirhist_cur_index]);
		if (cd_lists_on_the_fly) {
			while (files--) free(dirlist[files]);
			list_dir();
		}
	}
	else
		fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, 
				old_pwd[dirhist_cur_index], strerror(errno));
}

void list_mountpoints(void)
/* List available mountpoints and chdir into one of them */
{
	if (access("/proc/mounts", F_OK) != 0) {
		fprintf(stderr, "%s: mp: '/proc/mounts': %s\n", PROGRAM_NAME, 
				strerror(errno));
		return;
	}
	
	FILE *mp_fp=fopen("/proc/mounts", "r");
	if (mp_fp) {
		printf(_("%sMountpoints%s\n\n"), white, NC);
		/* The line variable should be able to store the device name,
		 * the mount point (PATH_MAX) and mount options. It should be 
		 * therefore at least PATH_MAX*2 */
		char line[PATH_MAX]="", **mountpoints=NULL;
		int mp_n=0;
		while (fgets(line, sizeof(line), mp_fp)) {
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
						printf("%s%d%s %s%s%s (%s)\n", yellow, mp_n+1, NC, 
							   (access(str, R_OK|X_OK) == 0) ? blue : red, 
							   str, NC, device);
						/* Store the second field (mountpoint) into an
						 * array */
						mountpoints=xrealloc(mountpoints, 
											 (mp_n+1)*sizeof(char **));
						mountpoints[mp_n]=xcalloc(mountpoints[mp_n], 
												  strlen(str)+1, 
												  sizeof(char *));					
						strcpy(mountpoints[mp_n++], str);
					}
					str=strtok(NULL, " ,");
					counter++;
				}
			}
		}
		fclose(mp_fp);
		
		/* This should never happen: There should always be a mountpoint,
		 * at least "/" */
		if (mp_n == 0) {
			printf(_("mp: There are no available mountpoints\n"));
			return;
		}
		
		/* Ask the user and chdir into the selected mountpoint */
		char *input=rl_no_hist(_("\nChoose a mountpoint ('q' to quit): "));
		if (input && input[0] != '\0' && strcmp(input, "q") != 0) {
			int atoi_num=atoi(input);
			if (atoi_num > 0 && atoi_num <= mp_n) {
				int ret=chdir(mountpoints[atoi_num-1]);
				if (ret == 0) {
					free(path);
					path=xcalloc(path, strlen(mountpoints[atoi_num-1])+1, 
								 sizeof(char *));
					strcpy(path, mountpoints[atoi_num-1]);
					add_to_dirhist(path);
					if (cd_lists_on_the_fly) {
						while (files--) free(dirlist[files]);
						list_dir();						
					}
				}
				else
					fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, 
							mountpoints[atoi_num-1], strerror(errno));
			}
			else
				printf(_("mp: '%s': Invalid mountpoint\n"), input);
		}
		
		/* Free stuff and exit */
		if (input)
			free(input);
		for (size_t i=0;i<mp_n;i++) {
			free(mountpoints[i]);
		}
		free(mountpoints);
	}
	else /* If fopen failed */
		fprintf(stderr, "%s: mp: fopen: '/proc/mounts': %s\n", PROGRAM_NAME, 
				strerror(errno));
}

int get_max_long_view(void)
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

void log_msg(char *_msg, int print)
/* Handle the error message 'msg'. Store 'msg' in an array of error messages, 
 * write it into an error log file, and print it immediately (if print is 
 * zero (NOPRINT_PROMPT) or tell the next prompt, if print is one to do it 
 * (PRINT_PROMPT)). Messages wrote to the error log file have the following 
 * format: "[date] msg", where 'date' is YYYY-MM-DDTHH:MM:SS */
{
	size_t msg_len=strlen(_msg);
	if (!_msg || msg_len == 0) return;
	/* Store messages (for current session only) in an array, so that
	 * the user can check them via the 'msg' command */
	msgs_n++;
	messages=xrealloc(messages, (msgs_n+1)*sizeof(char **));
	messages[msgs_n-1]=xcalloc(messages[msgs_n-1], msg_len+1, 
							   sizeof(char *));
	strcpy(messages[msgs_n-1], _msg);
	messages[msgs_n]=NULL;
	if (print) /* PRINT_PROMPT */
		/* The next prompt will take care of printing the message */
		print_msg=1;
	else /* NOPRINT_PROMPT */
		/* Print the message directly here */
		fprintf(stderr, "%s", _msg);
	
	/* If msg log file isn't set yet... This will happen if an error occurs
	 * before running init_config(), for example, if the user's home cannot be
	 * found */
	if (MSG_LOG_FILE[0] == '\0') 
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
expand_range(char *str)
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
	if (afirst <= 0 || afirst > files || asecond <= 0 || asecond > files
			|| afirst >= asecond)
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
		file[file_len-1]='\0';
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
			parent=xcalloc(parent, 2, sizeof(char *));
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
	
	char cmd[(PATH_MAX*2)+14]="";
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
		filename[filename_len-size]='\0';
	}
	size_t file_suffix_len=filename_len+suffix_len+2; /* 2 = dot + null byte */
	char file_suffix[file_suffix_len];
	memset(file_suffix, 0x00, file_suffix_len);
	snprintf(file_suffix, file_suffix_len, "%s.%s", filename, suffix);

	/* Copy the original file into the trash files directory */
	snprintf(cmd, sizeof(cmd), "cp -ra '%s' '%s/%s'", file, TRASH_FILES_DIR, 
		   	 file_suffix);
	free(filename);
	ret=launch_execle(cmd);
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
		snprintf(cmd, sizeof(cmd), "rm -r '%s/%s'", TRASH_FILES_DIR, 
				 file_suffix);
		ret=launch_execle(cmd);
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
	memset(cmd, 0x00, sizeof(cmd));
	snprintf(cmd, sizeof(cmd), "rm -r '%s'", file);
	ret=launch_execle(cmd);
	/* If remove fails, remove trash and info files */
	if (ret != 0) {
		fprintf(stderr, _("%s: trash: '%s': Failed removing file\n"), 
				PROGRAM_NAME, file);
		memset(cmd, 0x00, sizeof(cmd));
		snprintf(cmd, sizeof(cmd), "rm -r '%s/%s' '%s'", TRASH_FILES_DIR, 
				 file_suffix, info_file);
		ret=launch_execle(cmd);
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
		printf(_("%sTrashed files%s%s\n\n"), white, NC, white_b);
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
	printf(_("\n%s%sEnter 'q' to quit.\n"), NC, white_b);
	fputs(_("Elements to be removed (ex: 1 2 6, or *)? "), stdout);
	char **rm_elements=NULL;
	rm_elements=xcalloc(rm_elements, files_n, sizeof(char **));
	while (c != '\n') { /* Exit loop when Enter is pressed */
		/* Do not allow more removes than trashed files are */
		if (rm_n == files_n) break;
		rm_elements[rm_n]=xcalloc(rm_elements[rm_n], 4, sizeof(char *)); 
		/* 4 = 4 digits num (max 9,999) */
		while (!isspace(c=getchar())) 
			rm_elements[rm_n][length++]=c;
		/* Add end of string char to the last position */
		rm_elements[rm_n++][length]='\0';
		length=0;
	}
	rm_elements=xrealloc(rm_elements, sizeof(char **)*rm_n);

	/* Remove files */
	char rm_file[PATH_MAX]="", rm_info[PATH_MAX]="", cmd[(PATH_MAX*2)+12]="";
	/* (PATH_MAX*2)+12 = two paths and chars for command */
	int ret=-1;
	
	/* First check for exit, wildcard and non-number args */
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
				snprintf(cmd, sizeof(cmd), "rm -r '%s' '%s'", rm_file, rm_info);
				ret=launch_execle(cmd);
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
		/* Since it was used before, clean the cmd variable */
		memset(cmd, 0x00, sizeof(cmd));
		snprintf(cmd, sizeof(cmd), "rm -r '%s' '%s'", rm_file, rm_info);
		ret=launch_execle(cmd);
		if (ret != 0) {
			fprintf(stderr, _("%s: trash: Error trashing %s\n"), PROGRAM_NAME, 
					trash_files[rm_num-1]->d_name);
		}
		free(rm_elements[i]);
	}
	free(rm_elements);
	for (i=0;i<files_n;i++) free(trash_files[i]);
	free(trash_files);
}

void
untrash_element(char *file)
{
	char undel_file[PATH_MAX]="", undel_info[PATH_MAX]="";
	snprintf(undel_file, PATH_MAX, "%s/%s", TRASH_FILES_DIR, file);
	snprintf(undel_info, PATH_MAX, "%s/%s.trashinfo", TRASH_INFO_DIR, file);
	FILE *info_fp;
	info_fp=fopen(undel_info, "r");
	if (info_fp) {
		/* (PATH_MAX*2)+14 = two paths plus 14 extra bytes for command */
		char *orig_path=NULL, cmd[(PATH_MAX*2)+14]="";
		char line[MAX_LINE]="";
		while (fgets(line, sizeof(line), info_fp))
			if (strncmp(line, "Path=", 5) == 0)
				orig_path=straft(line, '=');
		fclose(info_fp);
		
		/* If original path is NULL or empty, return */
		if (!orig_path)
			return;
		if (strcmp(orig_path, "") == 0) {
			free(orig_path);
			return;
		}
		
		/* Remove new line char from original path, if any */
		size_t orig_path_len=strlen(orig_path);
		if (orig_path[orig_path_len-1] == '\n')
			orig_path[orig_path_len-1]='\0';
		
		/* Decode original path's URL format */
		char *url_decoded=url_decode(orig_path);
		if (!url_decoded) {
			fprintf(stderr, _("%s: undel: '%s': Failed decoding path\n"), 
					PROGRAM_NAME, orig_path);
			free(orig_path);
			return;
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
				parent=xcalloc(parent, 2, sizeof(char *));
				sprintf(parent, "/");
			}
			else {
				free(url_decoded);
				return;
			}
		}
		if (access(parent, F_OK) != 0) {
			fprintf(stderr, _("%s: undel: '%s': No such file or directory\n"), 
					PROGRAM_NAME, parent);
			free(parent);
			free(url_decoded);
			return;
		}
		if (access(parent, X_OK|W_OK) != 0) {
			fprintf(stderr, _("%s: undel: '%s': Permission denied\n"), 
					PROGRAM_NAME, parent);
			free(parent);
			free(url_decoded);
			return;
		}
		free(parent);
		snprintf(cmd, sizeof(cmd), "cp -ra '%s' '%s'", undel_file, 
				 url_decoded);
		free(url_decoded);
		int ret=-1;
		ret=launch_execle(cmd);
		if (ret == 0) {
			/* Clean the cmd variable: it was used above */
			memset(cmd, 0x00, sizeof(cmd));
			snprintf(cmd, sizeof(cmd), "rm -r '%s' '%s'", undel_file, 
					undel_info);
			ret=launch_execle(cmd);
			if (ret != 0) {
				fprintf(stderr, _("%s: undel: '%s': Failed removing info \
file\n"), PROGRAM_NAME, undel_info);
			}
		}
		else {
			fprintf(stderr, _("%s: undel: '%s': Failed restoring trashed \
file\n"), PROGRAM_NAME, undel_file);
		}
	}
	else {
		fprintf(stderr, _("%s: undel: Info file for '%s' not found. \
Try restoring the file manually\n"), PROGRAM_NAME, file);
	}
}

void
untrash_function(char **comm)
{
	if (comm[1] && strcmp(comm[1], "--help") == 0) {
		puts(_("Usage: undel"));
		return;
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
		return;
	}

	/* Get trashed files */
	struct dirent **trash_files=NULL;
	int trash_files_n=scandir(TRASH_FILES_DIR, &trash_files, skip_implied_dot, 
							(unicode) ? alphasort : (case_sensitive) 
							? xalphasort : alphasort_insensitive);
	if (trash_files_n == 0) {
		printf(_("trash: There are no trashed files\n"));
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
		}
		return;
	}

	/* if "undel all" (or "u a") */
	if (comm[1] && (strcmp(comm[1], "a") == 0 
	|| strcmp(comm[1], "all") == 0)) {
		for (size_t j=0;j<trash_files_n;j++) {
			untrash_element(trash_files[j]->d_name);
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
		}
		return;
	}	
	
	/* List trashed files */
	printf(_("%sTrashed files%s%s\n\n"), white, NC, white_b);
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
		return;
	}

	/* Get user input */
	printf(_("\n%s%sEnter 'q' to quit.\n"), NC, white_b);
	int no_space=0, undel_n=0;
	char *line=NULL, **undel_elements=NULL;
	while (!line) {
		line=rl_no_hist(_("Elements to be undeleted (ex: 1 2 6, or *)? "));
		if (!line) continue;
		for (size_t i=0;line[i];i++)
			if (line[i] != 0x20)
				no_space=1;
		if (line[0] == '\0' || !no_space) {
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
		return;

	/* First check for quit, *, and non-number args */
	short free_and_return=0;
	for(size_t i=0;i<undel_n;i++) {
		if (strcmp(undel_elements[i], "q") == 0)
			free_and_return=1;
		else if (strcmp(undel_elements[i], "*") == 0) {
			for (size_t j=0;j<trash_files_n;j++)
				untrash_element(trash_files[j]->d_name);
			free_and_return=1;
		}
		else if (!is_number(undel_elements[i])) {
			fprintf(stderr, _("undel: '%s': Invalid ELN\n"), 
					undel_elements[i]);
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
		return;
	}

	/* Undelete trashed files */
	int undel_num=0;
	for(size_t i=0;i<undel_n;i++) {
		undel_num=atoi(undel_elements[i]);
		if (undel_num <= 0 || undel_num > trash_files_n) {
			fprintf(stderr, _("%s: trash: '%d': Invalid ELN\n"), PROGRAM_NAME, 
					undel_num);
			free(undel_elements[i]);
			continue;
		}
		/* If valid ELN */
		untrash_element(trash_files[undel_num-1]->d_name);
		free(undel_elements[i]);
	}
	free(undel_elements);
	
	/* Free trashed files list */
	for (size_t i=0;i<trash_files_n;i++)
		free(trash_files[i]);
	free(trash_files);
	
	/* If some trashed file still remains, reload the undel screen */
	trash_n=count_dir(TRASH_FILES_DIR);
	if (trash_n <= 2) trash_n=0;
	if (trash_n)
		untrash_function(comm);
}

void
trash_clean(void)
{
	struct dirent **trash_files=NULL;
	int files_n=-1;
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
	files_n=scandir(TRASH_FILES_DIR, &trash_files, skip_implied_dot, 
					xalphasort);
	if (!files_n) {
		printf(_("trash: There are no trashed files\n"));
		return;
	}
	char cmd[(PATH_MAX*2)+14]="";
	for (size_t i=0;i<files_n;i++) {
		size_t info_file_len=strlen(trash_files[i]->d_name)+11;
		char info_file[info_file_len];
		memset(info_file, 0x00, info_file_len);
		snprintf(info_file, info_file_len, "%s.trashinfo", 
				trash_files[i]->d_name);
		snprintf(cmd, sizeof(cmd), "rm -r '%s/%s' '%s/%s'", TRASH_FILES_DIR, 
				trash_files[i]->d_name, TRASH_INFO_DIR, info_file);
		int ret=launch_execle(cmd);
		if (ret != 0) {
			fprintf(stderr, _("%s: trash: '%s': Error removing trashed \
file\n"), PROGRAM_NAME, trash_files[i]->d_name);
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
	}
}

void
trash_function(char **comm)
{
	/* Create trash dirs, if necessary */
	struct stat file_attrib;
	if (stat(TRASH_DIR, &file_attrib) == -1) {
		char cmd[PATH_MAX]="";
		snprintf(cmd, PATH_MAX, "mkdir -p %s/{files,info}", TRASH_DIR);
		int ret=launch_execle(cmd);
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
	}

	if (comm[1] && strcmp(comm[1], "--help") == 0) {
		puts(_("Usage: trash ELN/filename... [ls, list] [clean] [del, rm]"));
		return;
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
			return;
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
		}
	}
	else {
		/* Create suffix from current date and time to create unique filenames 
		 * for trashed files */
		time_t rawtime=time(NULL);
		struct tm *tm=localtime(&rawtime);
		char date[64]="";
		strftime(date, sizeof(date), "%c", tm);
		char suffix[68]="";
		snprintf(suffix, 67, "%d%d%d%d%d%d", tm->tm_year+1900, tm->tm_mon+1, 
				 tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
		
		if (strcmp(comm[1], "del") == 0 || strcmp(comm[1], "rm") == 0)
			remove_from_trash();
		else if (strcmp(comm[1], "clean") == 0)
			trash_clean();
		else {
			/* Trash files passed as arguments */
			for (size_t i=1;comm[i];i++) {
				char tmp_comm[PATH_MAX]="";
				if (comm[i][0] == '/') /* If absolute path */
					strcpy(tmp_comm, comm[i]);
				else { /* If relative path, add path to check against 
					TRASH_DIR */
					snprintf(tmp_comm, PATH_MAX, "%s/%s", path, comm[i]);
				}
				/* Do not trash any of the parent directories of TRASH_DIR,
				 * that is, /, /home, ~/, ~/.local, ~/.local/share */
				if (strncmp(tmp_comm, TRASH_DIR, strlen(tmp_comm)) == 0) {
					fprintf(stderr, _("trash: Cannot trash '%s'\n"), tmp_comm);
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
				trash_element(suffix, tm, comm[i]);
				/* The trash_element() function will take care of printing
				 * error messages, if any */
			}
		}
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
	old_pwd=xrealloc(old_pwd, sizeof(char **)*(dirhist_total_index+1));
	old_pwd[dirhist_total_index]=xcalloc(old_pwd[dirhist_total_index], 
										 strlen(dir_path), 
										 sizeof(char *));
	dirhist_cur_index=dirhist_total_index;
	strcpy(old_pwd[dirhist_total_index++], dir_path);
}

void
readline_kbinds(void)
{
	rl_command_func_t readline_kbind_action;
/* To get the keyseq value for a given key do this in an terminal:
 * C-v and then press the key (or the key combination). So, for example, 
 * C-v, C-right arrow gives "[[1;5C", which here should be written like this:
 * "\\e[1;5A" */

/* These keybindings work on all the terminals I tried: the linux built-in
 * console, aterm, urxvt, xterm, lxterminal, xfce4-terminal, gnome-terminal,
 * terminator, and st (with some patches, however, they might stop working in
 * st) */

	rl_bind_keyseq("\\M-u", readline_kbind_action); //key: 117
	rl_bind_keyseq("\\M-j", readline_kbind_action); //key: 106
	rl_bind_keyseq("\\M-h", readline_kbind_action); //key: 104
	rl_bind_keyseq("\\M-k", readline_kbind_action); //key: 107
	rl_bind_keyseq("\\e[21~", readline_kbind_action); // F10: 126
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

	rl_bind_keyseq("\\e[1;5A", readline_kbind_action); // C-up: 65
	rl_bind_keyseq("\\e[1;5B", readline_kbind_action); // C-down: 66
	rl_bind_keyseq("\\e[1;5C", readline_kbind_action); // C-right: 67
	rl_bind_keyseq("\\e[1;5D", readline_kbind_action); // C-left: 68
	rl_bind_keyseq("\\e[H", readline_kbind_action); // HOME: 72
	rl_bind_keyseq("\\e[21~", readline_kbind_action); // F10: 126
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
	if (c == '\0' || !str) 
		return NULL;
	size_t str_len=strlen(str);
	for (size_t i=0;i<str_len;i++) {
		if (str[i] == c) {
			size_t index=i+1, j=0;
			if (i == str_len-1)
				return NULL;
			char *buf=NULL;
			buf=xcalloc(buf, (str_len-index)+1, sizeof(char *));	
			for (i=index;i<str_len;i++) {
				if (str[i] != '"' && str[i] != '\'' && str[i] != '\\' 
				&& str[i] != '\0') {
					buf[j++]=str[i];
				}
			}
			buf[j]='\0';
			return buf;
		}
	}
	return NULL;
}

void
create_usr_var(char *str)
{
	char *name=strbfr(str, '=');
	char *value=parse_usrvar_value(str, '=');
	if (!name) {
		if (value)
			free(value);
		fprintf(stderr, _("%s: Error getting variable name\n"), PROGRAM_NAME);
		return;
	}
	if (!value) {
		free(name);
		fprintf(stderr, _("%s: Error getting variable value\n"), PROGRAM_NAME);
		return;
	}
	usr_var=xrealloc(usr_var, (usrvar_n+1)*sizeof(struct usrvar_t));
	usr_var[usrvar_n].name=xcalloc(usr_var[usrvar_n].name, strlen(name)+1, 
								   sizeof(char *));
	usr_var[usrvar_n].value=xcalloc(usr_var[usrvar_n].value, strlen(value)+1, 
									sizeof(char *));
	strcpy(usr_var[usrvar_n].name, name);
	strcpy(usr_var[usrvar_n++].value, value);
	free(name);
	free(value);
}

void
free_stuff(void)
{
	size_t i=0;
	
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
	
	if (history) {
		for (i=0;i<(size_t)current_hist_n;i++)
			free(history[i]);
		free(history);
	}
	
	if (paths) {
		for (i=0;i<(size_t)path_n;i++)
			free(paths[i]);
		free(paths);
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
		str=xrealloc(str, sizeof(char **)*(i+1));
*/
{
	/* According to realloc manpage: "If ptr is NULL, then the call is 
	 * equivalent to malloc(size), for all values of size; if size is equal 
	 * to zero, and ptr is not NULL, then the call is equivalent to 
	 * free(ptr)" */
	if (size == 0) ++size;
	void *new_ptr=realloc(ptr, size);
	if (!new_ptr) {
		new_ptr=realloc(ptr, size);
		if (!new_ptr) {
			/* ptr won't be correctly freed if it points to a multidimensional
			 * array */
			free(ptr);
			asprintf(&msg, _("%s: %s failed to allocate %zu bytes\n"), 
					 PROGRAM_NAME, __func__, size);
			if (msg) {
				log_msg(msg, NOPRINT_PROMPT);
				free(msg);
			}
			else
				fprintf(stderr, _("%s: %s failed to allocate %zu bytes\n"), 
						PROGRAM_NAME, __func__, size);
			atexit(free_stuff);
		}
	}
	ptr=new_ptr;
	new_ptr=NULL;
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
			atexit(free_stuff);
		}
	}
	ptr=new_ptr;
	new_ptr=NULL;
	return ptr;
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

		/* Make the shell pgid (process group id) equal to its pid */
		/* The lines below prevent CliFM from being called as argument for a 
		 * terminal, ex: xterm -e /path/to/clifm. However, without them input 
		 * freezes when running as root */
		if (flags & ROOT_USR) {
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
				atexit(free_stuff);
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
/* Get the list of files in PATH and send them into an array to be read by my 
 * readline custom auto-complete function (my_rl_completion)*/
{
	struct dirent **commands_bin[path_n];
	int	i, j, l=0, cmd_n[path_n], total_cmd=0;
	for (int k=0;k<path_n;k++) {
		cmd_n[k]=scandir(paths[k], &commands_bin[k], NULL, xalphasort);
		/* If paths[k] directory does not exist, scandir returns -1. Fedora,
		 * for example, add $HOME/bin and $HOME/.local/bin to PATH disregarding
		 * if they exist or not. If paths[k] dir is empty do not use it 
		 * either */
		if (cmd_n[k] > 0)
			total_cmd+=cmd_n[k];
	}
	/* Add internal commands */
	/* Get amount of internal cmds (elements in INTERNAL_CMDS array) */
	int internal_cmd_n=(sizeof(INTERNAL_CMDS)/sizeof(INTERNAL_CMDS[0]))-1;
	bin_commands=xcalloc(bin_commands, total_cmd+internal_cmd_n+2, 
		sizeof(char **));
	for (i=0;i<internal_cmd_n;i++) {
		bin_commands[l]=xcalloc(bin_commands[l], strlen(INTERNAL_CMDS[i])+3, 
								sizeof(char *));
		strcpy(bin_commands[l++], INTERNAL_CMDS[i]);		
	}
	/* Add commands in PATH */
	for (i=0;i<path_n;i++) {
		if (cmd_n[i] > 0) {
			for (j=0;j<cmd_n[i];j++) {
				bin_commands[l]=xcalloc(bin_commands[l], 
										strlen(commands_bin[i][j]->d_name)+3, 
										sizeof(char *));
				strcpy(bin_commands[l++], commands_bin[i][j]->d_name);
				free(commands_bin[i][j]);
			}
			free(commands_bin[i]);
		}
	}
}

char **
my_rl_completion(const char *text, int start, int end)
{
	char **matches=NULL;
	/* This line only prevents a Valgrind warning about unused variables */
	if (end) {}
	if (start == 0) { /* Only for the first word entered in the prompt */
		/* rl_attempted_completion_over=1; */
		/* Commands auto-completion */
		matches=rl_completion_matches(text, &bin_cmd_generator);
	}
	else { /* ELN auto-expansion !!! */
		int num_text=atoi(text);
		if (is_number(text) && num_text > 0 && num_text <= files) {
			matches=rl_completion_matches(text, &filenames_generator);
			/* If match is dir (ends in slash), remove final space from
			 * the readline buffer, so that the cursor will be next to
			 * the final slash */
			if (matches[0] && matches[0][strlen(matches[0])-1] == '/') {
				rl_point=end-1;
				rl_line_buffer[rl_point]=0x20;			
			}
		}
	}
	/* If none of the above, this function performs filename auto-completion */
	return matches;
}

char *
filenames_generator(const char *text, int state)
{
	static int i;
	char *name;
	if (!state)
		i=0;
	int num_text=atoi(text);
	/* Check list of currently displayed files for a match */
	while (i < files && (name=dirlist[i++]->d_name) != NULL) {
		if (strcmp(name, dirlist[num_text-1]->d_name) == 0) {
			/* If there is a match, and match is dir, add a final slash */
			struct stat file_attrib;
			lstat(name, &file_attrib);
			switch (file_attrib.st_mode & S_IFMT) {
				case S_IFDIR: name[strlen(name)]='/';
				break;
			}
			return strdup(name);
		}
	}
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
		paths=xrealloc(paths, sizeof(char **)*(path_num+1));
		/* Allocate space (buf length) for the new path */
		paths[path_num]=xcalloc(paths[path_num], length, sizeof(char *));
		/* Dump the buffer into the array */
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
	/* Set up program's directories and files (always per user) */
	size_t pnl_len=strlen(PNL);
	CONFIG_DIR=xcalloc(CONFIG_DIR, pnl_len+user_home_len+10, sizeof(char *));
	sprintf(CONFIG_DIR, "%s/.config/%s", user_home, PNL);
	TRASH_DIR=xcalloc(TRASH_DIR, user_home_len+20, sizeof(char *));
	sprintf(TRASH_DIR, "%s/.local/share/Trash", user_home);
	size_t trash_len=strlen(TRASH_DIR);
	TRASH_FILES_DIR=xcalloc(TRASH_FILES_DIR, trash_len+7, sizeof(char *));
	sprintf(TRASH_FILES_DIR, "%s/files", TRASH_DIR);
	TRASH_INFO_DIR=xcalloc(TRASH_INFO_DIR, trash_len+6, sizeof(char *));
	sprintf(TRASH_INFO_DIR, "%s/info", TRASH_DIR);
	size_t config_len=strlen(CONFIG_DIR);
	BM_FILE=xcalloc(BM_FILE, config_len+15, sizeof(char *));
	sprintf(BM_FILE, "%s/bookmarks.cfm", CONFIG_DIR);
	LOG_FILE=xcalloc(LOG_FILE, config_len+9, sizeof(char *));
	sprintf(LOG_FILE, "%s/log.cfm", CONFIG_DIR);
	LOG_FILE_TMP=xcalloc(LOG_FILE_TMP, config_len+13, sizeof(char *));
	sprintf(LOG_FILE_TMP, "%s/log_tmp.cfm", CONFIG_DIR);
	HIST_FILE=xcalloc(HIST_FILE, config_len+13, sizeof(char *));
	sprintf(HIST_FILE, "%s/history.cfm", CONFIG_DIR);
	CONFIG_FILE=xcalloc(CONFIG_FILE, config_len+pnl_len+4, sizeof(char *));
	sprintf(CONFIG_FILE, "%s/%src", CONFIG_DIR, PNL);	
	PROFILE_FILE=xcalloc(PROFILE_FILE, config_len+pnl_len+10, sizeof(char *));
	sprintf(PROFILE_FILE, "%s/%s_profile", CONFIG_DIR, PNL);
	MSG_LOG_FILE=xcalloc(MSG_LOG_FILE, config_len+14, sizeof(char *));
	sprintf(MSG_LOG_FILE, "%s/messages.cfm", CONFIG_DIR);

	/* Define the user's sel file. There will be one per user 
	 * (/tmp/clifm/sel_file_username) */
	size_t user_len=strlen(user);
	size_t tmp_dir_len=strlen(TMP_DIR);
	sel_file_user=xcalloc(sel_file_user, tmp_dir_len+pnl_len+user_len+8, 
						  sizeof(char *));
	sprintf(sel_file_user, "%s/.%s_sel_%s", TMP_DIR, PNL, user);

	/* Create trash dirs if necessary */
	int ret=-1;
	struct stat file_attrib;
	if (stat(TRASH_DIR, &file_attrib) == -1) {
		char cmd[trash_len+23];
		memset(cmd, 0x00, trash_len+23);
		snprintf(cmd, sizeof(cmd), "mkdir -p %s/{files,info}", 
				 TRASH_DIR);
		ret=launch_execle(cmd);
		if (ret != 0) {
			asprintf(&msg, 
					 _("%s: mkdir: '%s': Error creating trash directory\n"), 
					 PROGRAM_NAME, TRASH_DIR);
			if (msg) {
				error_msg=1; /* Specify the kind of message, but only if
				if needs to be printed by prompt() */
				log_msg(msg, PRINT_PROMPT);
				free(msg);
			}
			else
				fprintf(stderr, 
						_("%s: mkdir: '%s': Error creating trash directory\n"), 
						PROGRAM_NAME, TRASH_DIR);
		}
	}

	/* If the config directory doesn't exist, create it */
	/* Use the GNU mkdir to let it handle parent directories */
	if (stat(CONFIG_DIR, &file_attrib) == -1) {
		char cmd[config_len+10];
		memset(cmd, 0x00, config_len+10);
		snprintf(cmd, sizeof(cmd), "mkdir -p %s", CONFIG_DIR);
		ret=launch_execle(cmd);
		if (ret != 0) {
			asprintf(&msg, 
					 _("%s: mkdir: '%s': Error creating configuration \
directory\n"), PROGRAM_NAME, CONFIG_DIR);
			if (msg) {
				error_msg=1;
				log_msg(msg, PRINT_PROMPT);
				free(msg);
			}
			else
				fprintf(stderr, 
						_("%s: mkdir: '%s': Error creating configuration \
directory\n"), PROGRAM_NAME, CONFIG_DIR);
		}
	}

	/* If the temporary directory doesn't exist, create it */
	if (stat(TMP_DIR, &file_attrib) == -1) {
		if (mkdir(TMP_DIR, S_IRWXU) == -1) { /* 700 */
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

	/* "XTerm*eightBitInput: false" must be set in HOME/.Xresources to make
	 * some keybindings like Alt+letter work correctly in xterm-like 
	 * terminal emulators */
	/* However, there is no need to do this if using the linux console, 
	 * since we are not in a graphical environment */
	if (flags & GRAPHICAL) {
		/* Check ~/.Xresources exists and eightBitInput is set to false. If 
		 * not, create the file and set the corresponding value */
		char xresources[PATH_MAX]="";
		sprintf(xresources, "%s/.Xresources", user_home);
		FILE *xresources_fp=fopen(xresources, "a+");
		if (xresources_fp) {
			char line[MAX_LINE]="", eight_bit_ok=0;
			while (fgets(line, sizeof(line), xresources_fp)) {
				if (strncmp(line, "XTerm*eightBitInput: false", 26) == 0)
					eight_bit_ok=1;
			}
			if (!eight_bit_ok) {
				/* Set the file position indicator at the end of the file */
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
	
	/* Open the profile file or create it, if it doesn't exist */
	if (stat(PROFILE_FILE, &file_attrib) == -1) {	
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
#Ex:\n#echo -e \"%s, the anti-eye-candy/KISS file manager\"\n"), PROGRAM_NAME, 
					PROGRAM_NAME);
			fclose(profile_fp);
		}
	}

	/* Open the config file or create it, if it doesn't exist */
	FILE *config_fp;
	if (stat(CONFIG_FILE, &file_attrib) == -1) {
		config_fp=fopen(CONFIG_FILE, "w");
		if (config_fp == NULL) {
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
		}
		else {
			fprintf(config_fp, _("%s configuration file\n\
########################\n\n"), PROGRAM_NAME);
			/* Do not translate config options */
			fprintf(config_fp, "Splash screen=false\n\
Welcome message=true\n\
Show hidden files=true\n\
Long view mode=false\n\
External commands=false\n\
List folders first=true\n\
cd lists automatically=true\n\
Case sensitive list=false\n\
Unicode=false\n\
Pager=false\n\
Prompt color=6\n\
#0: black; 1: red; 2: green; 3: yellow; 4: blue;\n\
#5: magenta; 6: cyan; 7: white; 8: default terminal color\n\
#Bold:\n\
#10: black; 11: red; 12: green; 13: yellow; 14: blue;\n\
#15: magenta; 16: cyan; 17: white; 18: default terminal color\n\
Text color=7\n\
Max history=500\n\
Max log=1000\n\
Clear screen=false\n\
Starting path=default\n");
			fprintf(config_fp, _("#Default starting path is HOME\n"));
			fprintf(config_fp, "#END OF OPTIONS\n\
\n###Aliases###\nalias ls='ls --color=auto -A'\n\
\n#PROMPT\n");
			fprintf(config_fp, _("#Write below the commands you want to be \
executed before the prompt\n#Ex:\n"));
			fprintf(config_fp, "#date | awk '{print $1\", \"$2,$3\", \"$4}'\n\n#END OF PROMPT\n"); 
			fclose(config_fp);
		}
	}

	/* READ THE CONFIG FILE */
	char prompt_color_set=-1, text_color_set=-1;
	config_fp=fopen(CONFIG_FILE, "r");
	if (!config_fp) {
		asprintf(&msg, _("%s: fopen: '%s': %s. Using default values.\n"), 
				 PROGRAM_NAME, CONFIG_FILE, strerror(errno));
		if (msg) {
			error_msg=1;
			log_msg(msg, PRINT_PROMPT);
			free(msg);
		}
		else
			fprintf(stderr, _("%s: fopen: '%s': %s. Using default values.\n"), 
					PROGRAM_NAME, CONFIG_FILE, strerror(errno));
	}
	else {
		char line[MAX_LINE]="";
		while (fgets(line, sizeof(line), config_fp)) {
			if (strncmp(line, "#END OF OPTIONS", 15) == 0)
				break;
			else if (strncmp(line, "Splash screen=", 14) == 0) {
				char opt_str[MAX_LINE+1]="";
				ret=sscanf(line, "Splash screen=%5s\n", opt_str);
				/* According to cppcheck: "sscanf() without field width limits can crash with 
				huge input data". Field width limits = %5s */
				if (ret == -1)
					continue;
				if (strncmp(opt_str, "true", 4) == 0)
					splash_screen=1;
				else if (strncmp(opt_str, "false", 5) == 0)
					splash_screen=0;
				/* Default value (if option was found but value is neither 
				 * true nor false) */
				else
					splash_screen=0;
			}
			else if (strncmp(line, "Welcome message=", 16) == 0) {
				char opt_str[MAX_LINE+1]="";
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
				char opt_str[MAX_LINE+1]="";
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
			else if (strncmp(line, "Show hidden files=", 18) == 0) {
				char opt_str[MAX_LINE+1]="";
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
			else if (strncmp(line, "Long view mode=", 15) == 0) {
				char opt_str[MAX_LINE+1]="";
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
			else if (strncmp(line, "External commands=", 18) == 0) {
				char opt_str[MAX_LINE+1]="";
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
			else if (strncmp(line, "List folders first=", 19) == 0) {
				char opt_str[MAX_LINE+1]="";
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
			else if (strncmp(line, "cd lists automatically=", 23) == 0) {
				char opt_str[MAX_LINE+1]="";
				ret=sscanf(line, "cd lists automatically=%5s\n", opt_str);
				if (ret == -1)
					continue;
				if (strncmp(opt_str, "true", 4) == 0)
					cd_lists_on_the_fly=1;
				else if (strncmp(opt_str, "false", 5) == 0)
					cd_lists_on_the_fly=0;
				else /* default */
					cd_lists_on_the_fly=1;
			}
			else if (strncmp(line, "Case sensitive list=", 20) == 0) {
				char opt_str[MAX_LINE+1]="";
				ret=sscanf(line, "Case sensitive list=%5s\n", opt_str);
				if (ret == -1)
					continue;
				if (strncmp(opt_str, "true", 4) == 0)
					case_sensitive=1;
				else if (strncmp(opt_str, "false", 5) == 0)
					case_sensitive=0;
				else /* default */
					case_sensitive=0;
			}
			else if (strncmp(line, "Unicode=", 8) == 0) {
				char opt_str[MAX_LINE+1]="";
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
			else if (strncmp(line, "Pager=", 6) == 0) {
				char opt_str[MAX_LINE+1]="";
				ret=sscanf(line, "Pager=%5s\n", opt_str);
				if (ret == -1)
					continue;
				if (strncmp(opt_str, "true", 4) == 0)
					pager=1;
				else if (strncmp(opt_str, "false", 5) == 0)
					pager=0;
				else /* default */
					pager=0;
			}
			else if (strncmp(line, "Prompt color=", 13) == 0) {
				int opt_num=0;
				sscanf(line, "Prompt color=%d\n", &opt_num);
				if (opt_num <= 0)
					continue;
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
					case 8: strcpy(prompt_color, "\001\033[0;39m\002"); break;
					case 10: strcpy(prompt_color, "\001\033[1;30m\002"); break;
					case 11: strcpy(prompt_color, "\001\033[1;31m\002"); break;
					case 12: strcpy(prompt_color, "\001\033[1;32m\002"); break;
					case 13: strcpy(prompt_color, "\001\033[1;33m\002"); break;
					case 14: strcpy(prompt_color, "\001\033[1;34m\002"); break;
					case 15: strcpy(prompt_color, "\001\033[1;35m\002"); break;
					case 16: strcpy(prompt_color, "\001\033[1;36m\002"); break;
					case 17: strcpy(prompt_color, "\001\033[1;37m\002"); break;
					case 18: strcpy(prompt_color, "\001\033[1;39m\002"); break;
					default: strcpy(prompt_color, "\001\033[0;36m\002");
				}
			}
			else if (strncmp(line, "Text color=", 11) == 0) {
				int opt_num=0;
				sscanf(line, "Text color=%d\n", &opt_num);
				if (opt_num <= 0)
					continue;
				text_color_set=1;
				switch (opt_num) {
					case 0: strcpy(text_color, "\001\033[0;30m\002"); break;
					case 1: strcpy(text_color, "\001\033[0;31m\002"); break;
					case 2: strcpy(text_color, "\001\033[0;32m\002"); break;
					case 3: strcpy(text_color, "\001\033[0;33m\002"); break;
					case 4: strcpy(text_color, "\001\033[0;34m\002"); break;
					case 5: strcpy(text_color, "\001\033[0;35m\002"); break;
					case 6: strcpy(text_color, "\001\033[0;36m\002"); break;
					case 7: strcpy(text_color, "white"); break;
					case 8: strcpy(text_color, "\001\033[0;39m\002"); break;
					case 10: strcpy(text_color, "\001\033[1;30m\002"); break;
					case 11: strcpy(text_color, "\001\033[1;31m\002"); break;
					case 12: strcpy(text_color, "\001\033[1;32m\002"); break;
					case 13: strcpy(text_color, "\001\033[1;33m\002"); break;
					case 14: strcpy(text_color, "\001\033[1;34m\002"); break;
					case 15: strcpy(text_color, "\001\033[1;35m\002"); break;
					case 16: strcpy(text_color, "\001\033[1;36m\002"); break;
					case 17: strcpy(text_color, "\001\033[1;37m\002"); break;
					case 18: strcpy(text_color, "\001\033[1;39m\002"); break;
					default: strcpy(text_color, "white");
				}
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
			else if (strncmp(line, "Starting path=", 14) == 0) {
				char opt_str[PATH_MAX+1]="";
				ret=sscanf(line, "Starting path=%4096s\n", opt_str);				
				if (ret == -1)
					continue;
				/* If starting path is not "default", and exists, and is a 
				 * directory, and the user has appropriate permissions, set 
				 * path to starting path. If any of these conditions is false, 
				 * path will be set to default, that is, HOME */
				if (strncmp(opt_str, "default", 7) != 0) {
					if (chdir(opt_str) == 0) {
						free(path);
						path=xcalloc(path, strlen(opt_str)+1, sizeof(char *));
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

	/* If some option was not found in the config file, or if this latter 
	 * could not be read for any reason, set the defaults */
	if (splash_screen == -1) splash_screen=0; /* -1 means not set */
	if (welcome_message == -1) welcome_message=1;
	if (show_hidden == -1) show_hidden=1;
	if (long_view == -1) long_view=0;
	if (ext_cmd_ok == -1) ext_cmd_ok=0;
	if (pager == -1) pager=0;
	if (max_hist == -1) max_hist=500;
	if (max_log == -1) max_log=1000;
	if (clear_screen == -1) clear_screen=0;
	if (list_folders_first == -1) list_folders_first=1;
	if (cd_lists_on_the_fly == -1) cd_lists_on_the_fly=1;	
	if (case_sensitive == -1) case_sensitive=0;
	if (unicode == -1) unicode=0;
	if (prompt_color_set == -1)
		strcpy(prompt_color, "\001\033[0;36m\002");
	if (text_color_set == -1)
		strcpy(text_color, "white");
	
	/* Handle white color for different kinds of terminals: linux (8 colors), 
	 * (r)xvt (88 colors), and the rest (256 colors). This block is completely 
	 * subjective: in my opinion, some white colors that do look good on some 
	 * terminals, look bad on others */
	/* 88 colors terminal */
	if (strcmp(getenv("TERM"), "xvt") == 0 
				|| strcmp(getenv("TERM"), "rxvt") == 0) {
		strcpy(white_b, "\001\e[97m\002");
		if (strcmp(text_color, "white") == 0)
			strcpy(text_color, "\001\e[97m\002");
	}
	/* 8 colors terminal */
	else if (strcmp(getenv("TERM"), "linux") == 0) {
		strcpy(white_b, "\001\e[37m\002");
		if (strcmp(text_color, "white") == 0)
			strcpy(text_color, "\001\e[37m\002");
	}
	/* 256 colors terminal */
	else {
		strcpy(white_b, "\001\033[38;5;253m\002");
		if (strcmp(text_color, "white") == 0)
			strcpy(text_color, "\001\033[38;5;253m\002");
	}
}

void
exec_profile(void)
{
	struct stat file_attrib;
	if (stat(PROFILE_FILE, &file_attrib) == 0) {
		FILE *fp=fopen(PROFILE_FILE, "r");
		if (fp) {
			char line[MAX_LINE]="";
			while(fgets(line, sizeof(line), fp)) {
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
			fclose(fp);
		}
	}
}

char *
get_cmd_path(const char *cmd)
/* Get the path of a given command from the PATH environment variable. It 
 * basically does the same as the 'which' Linux command */
{
	char *cmd_path=NULL;
	cmd_path=xcalloc(cmd_path, PATH_MAX, sizeof(char *));
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
		{"starting-path", required_argument, 0, 'p'},
		{"unicode", no_argument, 0, 'U'},
		{"no-unicode", no_argument, 0, 'u'},
		{"version", no_argument, 0, 'v'},
		{"splash", no_argument, 0, 's'},
		{"ext-cmds", no_argument, 0, 'x'},
		{0, 0, 0, 0}
	};
	int optc;
	/* Variables to store arguments to options -c and -p */
	char *path_value=NULL;
	while ((optc=getopt_long(argc, argv, "+aAfFgGhiIlLoOp:sUuvx", longopts, 
		(int *)0)) != EOF) {
		/*':' and '::' in the short options string means 'required' and 
		 * 'optional argument' respectivelly. Thus, 'c' and 'p' require an 
		 * argument here. The plus char (+) tells getopt to stop processing 
		 * at the first non-option */
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

	if (flags & START_PATH) {
		if (chdir(path_value) == 0) {
			free(path);
			path=xcalloc(path, strlen(path_value)+1, sizeof(char *));
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
}

char *
home_tilde(const char *new_path)
/* Reduce "HOME" to tilde ("~"). The new_path variable is always either 
 * "HOME" or "HOME/file", that's why there's no need to check for "/file" */
{
	char *path_tilde=NULL;	

	/* If path == HOME */
	if (strcmp(new_path, user_home) == 0) {
		path_tilde=xcalloc(path_tilde, 2, sizeof(char *));		
		path_tilde[0]='~';
	}

	/* If path == HOME/file */
	else {
		path_tilde=xcalloc(path_tilde, 2+strlen(new_path+user_home_len+1)+1, 
						   sizeof(char *));
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
			braces_root=xcalloc(braces_root, i+1, sizeof(char *));			
			/* If any, store the string before the beginning of the braces */
			for (j=0;j<i;j++)
				braces_root[j]=str[j];
		}
		/* If any, store the string after the closing brace */
		if (initial_brace && str[i] == '}') {
			closing_brace=i;
			if ((str_len-1)-closing_brace > 0) {
				braces_end=xcalloc(braces_end, (str_len-1)-closing_brace, 
								   sizeof(char *));
				for (k=closing_brace+1;str[k]!='\0';k++)
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
		memset(brace_tmp, '\0', closing_brace-initial_brace+1);
		while (str[i] != '}' && str[i] != ',' && str[i] != '\0')
			brace_tmp[k++]=str[i++];
		if (braces_end) {
			braces[braces_n]=xcalloc(braces[braces_n], strlen(braces_root)
									+strlen(brace_tmp)+strlen(braces_end), 
									sizeof(char *));
			sprintf(braces[braces_n], "%s%s%s", braces_root, brace_tmp, 
				braces_end);
		}
		else {
			braces[braces_n]=xcalloc(braces[braces_n], strlen(braces_root)
									+strlen(brace_tmp), sizeof(char *));
			sprintf(braces[braces_n], "%s%s", braces_root, brace_tmp);
		}
		if (str[i] == ',')
			braces=xrealloc(braces, (++braces_n+1)*sizeof(char **));
		k=0;
	}
	if (braces_root) 
		free(braces_root);
	if (braces_end) 
		free(braces_end);
	return braces_n+1;
}

char **
parse_input_str(const char *str)
/* 
 * This function is one of the keys of CliFM. It will perform a series of 
 * actions:
 * 1) Take the string stored by readline and get its substrings without spaces. 
 * 2) Handle single and double quotes, just as '\' for file names containing 
 * spaces: it stores the quoted file name in one single variable;
 * 3) In case of user defined variable (var=value), it will pass the whole 
 * string to exec_cmd(), which will take care of storing the variable;
 * 4) If the input string begins by ';' or ':' the whole string is send to 
 * exec_cmd(), where it will be executed by launch_execle();
 * 5) The following expansions are performed here: ELN's, wildcards, ranges of 
 * numbers (ELN's), braces, tilde, user defined variables, and 'sel'. These 
 * expansions are the most import part of this function.
 */

/* NOTE: Though filenames could consist of everything except of slash and null 
 * characters, POSIX.1 recommends restricting filenames to consist of the 
 * following characters: letters (a-z, A-Z), numbers (0-9), period (.), 
 * dash (-), and underscore ( _ ).*/

{
	/* #### 1) HANDLE SPACES #### */
	/* handle_spaces() removes leading, terminating, and double 
	* spaces from str. It will return NULL if str: a) contains only spaces; 
	* b) is NULL; c) is empty; d) memory allocation error */
	char *string_b=handle_spaces(str); //Remove isspace from here too.
	/* NOTE: isspace() not only checks for space, but also for new line, 
	 * carriage return, vertical and horizontal TAB. Be careful when replacing 
	 * this function. */
	if (!string_b)
		return NULL;
	size_t str_len=strlen(string_b); /* Run strlen just once in order not to 
	do it once and again, possibily hundreds of times, in a loop */
	char **comm_array=NULL;

	/* #### 2) CHECK FOR SPECIAL FUNCTIONS #### */
	size_t i=0;
	/* If user defined variable or if invoking a command via ';' or ':' send 
	 * the whole string to exec_cmd() */
	short space_found=0;
	for (i=0;i<str_len;i++) {
		if (string_b[i] == '=') {
			for (size_t j=0;j<i;j++) {
				/* If there are no spaces before '=', take it as a variable. 
				 * This check is done in order to avoid taking as a variable 
				 * things like: 'ls -color=auto'*/
				if (string_b[j] == 0x20)
					space_found=1;
			}
			if (!space_found && !isdigit(string_b[0]))
				flags |= IS_USRVAR_DEF;
			break;
		}
	}
	if (flags & IS_USRVAR_DEF || string_b[0] == ':' || string_b[0] == ';') {
		comm_array=xcalloc(comm_array, 1, sizeof(char **));
		comm_array[args_n]=xcalloc(comm_array[args_n], str_len+1, 
			sizeof(char *));
		/* args_n is here always zero */
		strncpy(comm_array[args_n], string_b, str_len);
		free(string_b);
		return comm_array;
	}

	/* #### 3) GET SUBSTRINGS #### */
	char buf[PATH_MAX]="";
	int int_array_max=10, glob_array[int_array_max], 
		braces_array[int_array_max], range_array[int_array_max], 
		glob_n=0, braces_n=0, ranges_ok=0;
	size_t length=0;
	for (i=0;i<=str_len;i++) {
		/* ## HANDLE BACKSLASHED STRINGS ## */
		if (string_b[i]=='\\' && string_b[i+1] == 0x20)	{
			size_t j=0;
			for (j=i+1;j<=str_len;j++) {
				if ((string_b[j] == 0x20 && string_b[j-1] != '\\') 
				|| string_b[j] == '\0')
					break;
				if (!(string_b[j] == '\\' && string_b[j+1] == 0x20) 
				&& length < sizeof(buf)) 
					/* 'length < sizeof(buff)' is aimed to prevent a buffer 
					 * overflow */
					buf[length++]=string_b[j];
			}
			i=j;
		}
		/* ## HANDLE QUOTED STRINGS ## */
		if (string_b[i] == '"' || string_b[i] == '\'') {
			char quotes;
			if (string_b[i] == '"') quotes='"';
			else quotes='\'';
			int j=0, end_quote=0;
			/* Decrementing loops won't work when index is size_t (use int 
			 * instead) Look for the terminating quote */
			for (j=str_len;j>i;j--) {
				if (string_b[j] == quotes)
					end_quote=j;
			}
			if (end_quote) {
				//dump everything between the quotes into the buffer
				for (j=i+1;j<end_quote;j++) {
					if (length < sizeof(buf))
						buf[length++]=string_b[j];
				}
				i=end_quote+1;
			}
			else {
				fprintf(stderr, _("%s: Missing quote\n"), PROGRAM_NAME);
				/* In case something has been already dumped into comm_array */
				if (comm_array) {
					/* Do not take the last argument into account (i<args_n, 
					 * and not i<=args_m), for this argument will not be 
					 * copied precisely because some quote is missing */
					for (j=0;j<args_n;j++)
						free(comm_array[j]);
					free(comm_array);
				}
				free(string_b);
				return NULL;
			}
		}

		/* If a range is found, store its index */
		if (i > 0 && i < str_len && string_b[i] == '-' && 
		isdigit(string_b[i-1]) && isdigit(string_b[i+1]))
			if (ranges_ok < int_array_max)
				range_array[ranges_ok++]=args_n;

		/* If a brace is found, store its index */
		if (string_b[i] == '{')
			if (braces_n < int_array_max)
				braces_array[braces_n++]=args_n;

		if (string_b[i] == '*' || string_b[i] == '?' || string_b[i] == '[')
		/* Strings containing these characters are taken as wildacard patterns 
		 * and are expanded by the glob function. See man (7) glob */
			if (glob_n < int_array_max)
				glob_array[glob_n++]=args_n;

		/* The only stored spaces are those of quoted and backslahed strings */
		if (string_b[i] != 0x20 && length < sizeof(buf))
			buf[length++]=string_b[i];

		/* ## STORE THE SUBSTRING IN THE INPUT ARRAY ## */
		if (string_b[i] == 0x20 || string_b[i] == '\0') {
			comm_array=xrealloc(comm_array, sizeof(char **)*(args_n+1));
			comm_array[args_n]=xcalloc(comm_array[args_n], length+1, 
									   sizeof(char *));
			strncpy(comm_array[args_n], buf, length);
			/* Clean the buffer (0x00 == \0 == null char) */
			memset(buf, 0x00, length);
			length=0; /* Reset length for the next argument */
			if (string_b[i] == '\0') 
				break; /* End of input */
			else 
				args_n++;
		}
	}
	free(string_b);

	/* ### RANGES EXPANSION ### */
	if (ranges_ok) {
		int old_ranges_n=0;
		for (size_t r=0;r<ranges_ok;r++) {
			int ranges_n=0;
			int *ranges=expand_range(comm_array[range_array[r]+old_ranges_n]);
			if (ranges) {
				size_t j=0;
				for (ranges_n=0;ranges[ranges_n];ranges_n++);
				char **ranges_cmd=NULL;
				ranges_cmd=xcalloc(ranges_cmd, args_n+ranges_n+2, 
								   sizeof(char **));
				for (i=0;i<range_array[r]+old_ranges_n;i++) {
					ranges_cmd[j]=xcalloc(ranges_cmd[j], strlen(comm_array[i]), 
										  sizeof(char *));
					strcpy(ranges_cmd[j++], comm_array[i]);
				}
				for (i=0;i<ranges_n;i++) {
					ranges_cmd[j]=xcalloc(ranges_cmd[j], 1, sizeof(char *));
					sprintf(ranges_cmd[j++], "%d", ranges[i]);
				}
				for (i=range_array[r]+old_ranges_n+1;i<=args_n;i++) {
					ranges_cmd[j]=xcalloc(ranges_cmd[j], strlen(comm_array[i]), 
										  sizeof(char *));
					strcpy(ranges_cmd[j++], comm_array[i]);
				}
				ranges_cmd[j]=NULL;
				free(ranges);
				for (i=0;i<=args_n;i++) free(comm_array[i]);
				comm_array=xrealloc(comm_array, 
									(args_n+ranges_n+2)*sizeof(char **));
				for (i=0;i<j;i++) {
					comm_array[i]=xcalloc(comm_array[i], strlen(ranges_cmd[i]), 
										  sizeof(char *));
					strcpy(comm_array[i], ranges_cmd[i]);
					free(ranges_cmd[i]);
				}
				free(ranges_cmd);
				args_n=j-1;
			}
			old_ranges_n+=(ranges_n-1);
		}
	}
	
	/* #### GLOB EXPANSION #### */
	if (glob_n) {
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
			glob(comm_array[glob_array[g]+old_pathc], 0, NULL, &globbuf);
			if (globbuf.gl_pathc) {
				size_t j=0;
				char **glob_cmd=NULL;
				glob_cmd=xcalloc(glob_cmd, args_n+globbuf.gl_pathc+1, 
								 sizeof(char *));			
				for (i=0;i<(glob_array[g]+old_pathc);i++) {
					glob_cmd[j]=xcalloc(glob_cmd[j], strlen(comm_array[i]), 
										sizeof(char *));
					strcpy(glob_cmd[j++], comm_array[i]);
				}
				for (i=0;i<globbuf.gl_pathc;i++) {
					glob_cmd[j]=xcalloc(glob_cmd[j], 
						strlen(globbuf.gl_pathv[i]), sizeof(char *));
					strcpy(glob_cmd[j++], globbuf.gl_pathv[i]);
				}
				for (i=glob_array[g]+old_pathc+1;i<=args_n;i++) {
					glob_cmd[j]=xcalloc(glob_cmd[j], strlen(comm_array[i]), 
										sizeof(char *));
					strcpy(glob_cmd[j++], comm_array[i]);
				}
				glob_cmd[j]=NULL;

				for (i=0;i<=args_n;i++) free(comm_array[i]);
				comm_array=xrealloc(comm_array, 
					(args_n+globbuf.gl_pathc+1)*sizeof(char **));
				for (i=0;i<j;i++) {
					comm_array[i]=xcalloc(comm_array[i], strlen(glob_cmd[i]), 
										  sizeof(char *));
					strcpy(comm_array[i], glob_cmd[i]);
					free(glob_cmd[i]);
				}
				args_n=j-1;
				free(glob_cmd);
			}
			old_pathc+=(globbuf.gl_pathc-1);
			globfree(&globbuf);
		}
	}
	
	/* #### 4) BRACES EXPANSION #### */
	if (braces_n) { /* If there is some braced parameter... */
		/* We already know the indexes of braced strings (braces_array[]) */
		int old_braces_arg=0;
		for (size_t b=0;b<braces_n;b++) {
			/* Expand the braced parameter and store it into a new array */
			int braced_args=brace_expansion(comm_array[braces_array[b]
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
												 strlen(comm_array[i]), 
												 sizeof(char *));
					strcpy(comm_array_braces[i], comm_array[i]);
				}
				/* Now, add the expanded braces to the same array */
				for (size_t j=0;j<braced_args;j++) {
					comm_array_braces[i]=xcalloc(comm_array_braces[i], 
												 strlen(braces[j]), 
												 sizeof(char *));
					strcpy(comm_array_braces[i++], braces[j]);
					free(braces[j]);
				}
				free(braces);
				/* Finally, add, if any, those parameters coming after the 
				 * braces */
				for (size_t j=braces_array[b]+old_braces_arg+1;j<=args_n;j++) {
					comm_array_braces[i]=xcalloc(comm_array_braces[i], 
												 strlen(comm_array[j]), 
												 sizeof(char *));
					strcpy(comm_array_braces[i++], comm_array[j]);				
				}
				/* Now, free the old comm_array and copy to it our new array 
				 * containing all the parameters, including the expanded 
				 * braces */
				for (size_t j=0;j<=args_n;j++)
					free(comm_array[j]);
				comm_array=xrealloc(comm_array, 
					(args_n+braced_args+1)*sizeof(char **));			
				args_n=i-1;
				for (int j=0;j<i;j++) {
					comm_array[j]=xcalloc(comm_array[j], 
										  strlen(comm_array_braces[j]), 
										  sizeof(char *));
					strcpy(comm_array[j], comm_array_braces[j]);
					free(comm_array_braces[j]);
				}
				old_braces_arg+=(braced_args-1);
				free(comm_array_braces);
			}
		}
	}

	/* #### 5) NULL TERMINATE THE INPUT STRING ARRAY #### */
	comm_array=xrealloc(comm_array, sizeof(char **)*(args_n+2));	
	comm_array[args_n+1]=NULL;

	/* #### 6) FURTHER EXPANSIONS #### */
	is_sel=0, sel_is_last=0, sel_no_sel=0;
	for (i=0;i<=(size_t)args_n;i++) {
		
		/* Expand 'sel' only as an argument, not as command */
		if (i > 0 && strcmp(comm_array[i], "sel") == 0)
			is_sel=i;
		
		/* ELN EXPANSION */
		/* i must be bigger than zero because the first string in comm_array, 
		 * the command name, should NOT be expanded, but only arguments. 
		 * Otherwise, if the expanded ELN happens to be a program name as 
		 * well, this program will be executed, and this, for sure, is to be 
		 * avoided */
		if (i > 0 && is_number(comm_array[i])) {
			int num=atoi(comm_array[i]);
			/* Expand numbers only if there is a corresponding ELN */
			if (num > 0 && num <= files) {
				size_t file_len=strlen(dirlist[num-1]->d_name);
				comm_array[i]=xrealloc(comm_array[i], 
									   (file_len+1)*sizeof(char *));
				memset(comm_array[i], 0x00, file_len+1);
				strcpy(comm_array[i], dirlist[num-1]->d_name);
			}
		}
		
		/* TILDE EXPANSION (replace "~/" by "/home/user") */
		if (strncmp(comm_array[i], "~/", 2) == 0) {
			char *path_no_tilde=straft(comm_array[i], '/');
			if (path_no_tilde) {
				comm_array[i]=xrealloc(comm_array[i], (user_home_len+
									  strlen(path_no_tilde)+2)*sizeof(char *));
				sprintf(comm_array[i], "%s/%s", user_home, path_no_tilde);
			}
			else {
				comm_array[i]=xrealloc(comm_array[i], 
									   (user_home_len+2)*sizeof(char *));
				sprintf(comm_array[i], "%s/", user_home);
			}
			free(path_no_tilde);
		}
		
		/* USER DEFINED VARIABLES EXPANSION */
		if (comm_array[i][0] == '$' && comm_array[i][1] != '(') {
			char *var_name=straft(comm_array[i], '$');
			if (var_name) {
				for (unsigned j=0;(int)j<usrvar_n;j++) {
					if (strcmp(var_name, usr_var[j].name) == 0) {
						comm_array[i]=xrealloc(comm_array[i], 
								(strlen(usr_var[j].value)+1)*sizeof(char *));
						strcpy(comm_array[i], usr_var[j].value);
					}
				}
				free(var_name);
			}
		}
	}
	
	/* SEL EXPANSION */
	if (is_sel) {
		if (is_sel == args_n) sel_is_last=1;
		if (sel_n) {
			size_t j=0;
			char **sel_array=NULL;
			sel_array=xcalloc(sel_array, args_n+sel_n+2, sizeof(char **));
			for (i=0;i<is_sel;i++) {
				sel_array[j]=xcalloc(sel_array[j], strlen(comm_array[i]), 
									 sizeof(char *));
				strcpy(sel_array[j++], comm_array[i]);
			}
			for (i=0;i<sel_n;i++) {
				sel_array[j]=xcalloc(sel_array[j], strlen(sel_elements[i]), 
									 sizeof(char *));
				strcpy(sel_array[j++], sel_elements[i]);
			}
			for (i=is_sel+1;i<=args_n;i++) {
				sel_array[j]=xcalloc(sel_array[j], strlen(comm_array[i]), 
									 sizeof(char *));
				strcpy(sel_array[j++], comm_array[i]);
			}
			for (i=0;i<=args_n;i++)
				free(comm_array[i]);
			comm_array=xrealloc(comm_array, (args_n+sel_n+2)*sizeof(char **));
			for (i=0;i<j;i++) {
				comm_array[i]=xcalloc(comm_array[i], strlen(sel_array[i]), 
									  sizeof(char *));
				strcpy(comm_array[i], sel_array[i]);
				free(sel_array[i]);
			}
			free(sel_array);
			comm_array[i]=NULL;
			args_n=j-1;
		}
		else {
			/* 'sel' is an argument, but there are no selected files. 
			exec_cmd() will check this flag, and will do nothing if true.
			*/
			fprintf(stderr, _("%s: There are no selected files\n"), 
					PROGRAM_NAME);
			sel_no_sel=1;
		}
	}
	return comm_array;
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

void
get_sel_files(void)
/* Get elements currently in the Selection Box, if any. */
{
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
		char sel_line[PATH_MAX]="";
		while (fgets(sel_line, sizeof(sel_line), sel_fp)) {
			size_t line_len=strlen(sel_line);
			sel_line[line_len-1]='\0';
			sel_elements=xrealloc(sel_elements, sizeof(char **)*(sel_n+1));
			sel_elements[sel_n]=xcalloc(sel_elements[sel_n], line_len, 
										sizeof(char *));
			strcpy(sel_elements[sel_n++], sel_line);
		}
		fclose(sel_fp);
	}
}

char *
prompt(void)
/* Print the prompt and return the string entered by the user (to be parsed 
 * later by parse_input_str()) */
{
	/* Remove final slash from path, if any */
	if (path[strlen(path)-1] == '/')
		path[strlen(path)-1] = 0x00;

	if (welcome_message) {
		printf(_("%sCliFM, the anti-eye-candy, KISS file manager%s\n"), 
			   magenta, NC);
		printf(_("%sType 'help' or '?' for instructions.%s\n"), 
			   white_b, NC);
		welcome_message=0;
	}
	/* Execute prompt commands, if any, only if external commands are 
	 * allowed */
	if (ext_cmd_ok && prompt_cmds_n > 0) 
		for (size_t i=0;i<(size_t)prompt_cmds_n;i++)
			launch_execle(prompt_cmds[i]);

	/* Update trash and sel file indicator on every prompt call */
	trash_n=count_dir(TRASH_FILES_DIR);
	if (trash_n <= 2) trash_n=0;
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
			short_path=xcalloc(short_path, 4, sizeof(char *));
			strncpy(short_path, "???\0", 4);
		}
	}
	/* Unisgned char (0 to 255), max hostname 64 or 255, max username 32 */
	static unsigned char first_time=1, user_len=0, hostname_len=0; /* Calculate 
	the length of these variables just once. In Bash, even if the user changes 
	his/her user or hostname, the prompt will not be updated til' the next 
	login */
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
		+33+7+((sel_n) ? 19 : 0)+((trash_n) ? 19 : 0)+((msgs_n) ? 19: 0)
		+user_len+hostname_len+2+23+1);
	char shell_prompt[prompt_length];
	/* Add 1 to strlen, since this function doesn't count the terminating 
	 * null byte */
	/* 33=length of prompt_color + length of NC_b; 7=chars in the prompt: 
	 * '[]', '@', '$' plus 3 spaces; 19=length of green_b, yellow_b; 
	 * 23=text_color + '*'; 1=space for the null char written at the end of 
	 * the string by sprintf */
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
	if (!input)
		return NULL;
	/* Enable commands history only if the input line is not void */
	if (input && *input) {
		/* Do not record exit, history commands, or consecutively equal 
		* inputs */
		if (strcmp(input, "q") != 0 && strcmp(input, "quit") != 0 
		&& strcmp(input, "exit") != 0 && strcmp(input, "zz") != 0
		&& strcmp(input, "salir") != 0 && strcmp(input, "chau") != 0
		&& input[0] != '!' && (strcmp(input, history[current_hist_n-1]) != 0)) {
			add_history(input);
			append_history(1, HIST_FILE);
			/* Add the new input to the history array */
			history=xrealloc(history, sizeof(char **)*(current_hist_n+1));
			history[current_hist_n]=xcalloc(history[current_hist_n], 
				strlen(input)+1, sizeof(char *));
			strcpy(history[current_hist_n++], input);
		}
	}
	return input;
}

/*int count_dir(const char *dir_path) //Scandir version
//Count the amount of elements contained in a directory. Receives the path to 
* the directory as argument.
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
			atexit(free_stuff);
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
	/* In case a directory isn't reacheable, like a failed mountpoint...	*/
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
				printf("%s%s%s%-*s%s%s", yellow, index, d_red, pad, entry, NC, 
					   new_line ? "\n" : "");
			else if (file_attrib.st_mode & S_ISUID) /* set uid file */
				printf("%s%s%s%-*s%s%s", yellow, index, bg_red_fg_white, pad, 
					   entry, NC, new_line ? "\n" : "");
			else if (file_attrib.st_mode & S_ISGID) /* set gid file */
				printf("%s%s%s%-*s%s%s", yellow, index, bg_yellow_fg_black, 
						pad, entry, NC, new_line ? "\n" : "");
			else {
				cap=cap_get_file(entry);
				if (cap) {
					printf("%s%s%s%-*s%s%s", yellow, index, bg_red_fg_black, 
							pad, entry, NC, new_line ? "\n" : "");
					cap_free(cap);
				}
				else if (file_attrib.st_mode & S_IXUSR) {
					if (file_attrib.st_size == 0)
						printf("%s%s%s%-*s%s%s", yellow, index, d_green, pad, 
							   entry, NC, new_line ? "\n" : "");
					else 
						printf("%s%s%s%-*s%s%s", yellow, index, green, pad, 
							   entry, NC, new_line ? "\n" : "");
				}
				else if (file_attrib.st_size == 0)
					printf("%s%s%s%-*s%s%s", yellow, index, d_yellow, pad, 
						   entry, NC, new_line ? "\n" : "");
				else
					printf("%s%s%s%s%-*s%s%s", yellow, index, NC, white_b, 
						   pad, entry, NC, new_line ? "\n" : "");
			}
		break;
		case S_IFDIR:
			if (access(entry, R_OK|X_OK) != 0)
				printf("%s%s%s%-*s%s%s", yellow, index, red, pad, entry, NC, 
					   new_line ? "\n" : "");
			else {
				int is_oth_w=0;
				if (file_attrib.st_mode & S_IWOTH) is_oth_w=1;
				int files_dir=count_dir(entry);
				if (files_dir == 2 || files_dir == 0) { /* If folder is empty, it 
					contains only "." and ".." (2 elements). If not mounted 
					(ex: /media/usb) the result will be zero.*/
					/* If sticky bit dir: green bg. */
					printf("%s%s%s%-*s%s%s", yellow, index, 
						(file_attrib.st_mode & S_ISVTX) ? ((is_oth_w) ? 
						bg_green_fg_blue : bg_blue_fg_white) : ((is_oth_w) ? 
						bg_green_fg_black : d_blue), pad, entry, NC, 
						new_line ? "\n" : "");
				}
				else
					printf("%s%s%s%-*s%s%s", yellow, index, 
						(file_attrib.st_mode & S_ISVTX) ? ((is_oth_w) ? 
						bg_green_fg_blue : bg_blue_fg_white) : ((is_oth_w) ? 
						bg_green_fg_black : blue), pad, entry, NC, 
						new_line ? "\n" : "");
			}
		break;
		case S_IFLNK:
			linkname=realpath(entry, NULL);
			if (linkname) {
				printf("%s%s%s%-*s%s%s", yellow, index, cyan, pad, entry, NC, 
					   new_line ? "\n" : "");
				free(linkname);
			}
			else printf("%s%s%s%-*s%s%s", yellow, index, d_cyan, pad, entry, 
						NC, new_line ? "\n" : "");
		break;
		case S_IFIFO: printf("%s%s%s%-*s%s%s", yellow, index, d_magenta, pad, 
							 entry, NC, new_line ? "\n" : ""); break;
		case S_IFBLK: printf("%s%s%s%-*s%s%s", yellow, index, yellow, pad, 
							 entry, NC, new_line ? "\n" : ""); break;
		case S_IFCHR: printf("%s%s%s%-*s%s%s", yellow, index, white, pad, 
							 entry, NC, new_line ? "\n" : ""); break;
		case S_IFSOCK: printf("%s%s%s%-*s%s%s", yellow, index, magenta, pad, 
							  entry, NC, new_line ? "\n" : ""); break;
		/* In case all of the above conditions are false... */
		default: printf("%s%s%s%-*s%s%s", yellow, index, bg_white_fg_red, 
					    pad, entry, NC, new_line ? "\n" : "");
	}
}

void
list_dir(void)
/* List files in the current working directory */
/* To get a list of 'ls' colors run: 
$ dircolors --print-database */
{
/*	clock_t start=clock(); */

	/* This function lists files in the current working directory, that is, 
	 * the global variable 'path', that SHOULD be set before calling this
	 * function */
	if (!path || path[0] == '\0') {
		fprintf(stderr, _("%s: Path variable is NULL or empty\n"), 
				PROGRAM_NAME);
		return;
	}
	
	files=0; /* Reset the files counter */
	int i=0;

	/* Remove final slash from path, if any */
	size_t strlen_path=strlen(path);
	if (path[strlen_path-1] == '/' && strcmp(path, "/") != 0)
		path[strlen_path-1]='\0';
	
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
				atexit(free_stuff);
			else
				return;
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
				atexit(free_stuff);
			else
				return;
		}
		/* Reallocate the dirlist struct according to the amount of folders 
		* and files found */
		/* If empty folder... Note: realloc(ptr, 0) acts like free(ptr) */
		if (!files_folders && !files_files) /* If neither files nor folders */
			dirlist=xrealloc(dirlist, sizeof(struct dirent **));
		else if (files_folders > 0) {
			if (files_files > 0) { /* If files and folders */
				dirlist=xrealloc(dirlist, 
								 sizeof(struct dirent **)*
								 (files_folders+files_files));
			}
			else /* If only folders */
				dirlist=xrealloc(dirlist, 
								 sizeof(struct dirent **)*files_folders);
		}
		else if (files_files > 0) /* If only files */
			dirlist=xrealloc(dirlist, sizeof(struct dirent **)*files_files);
		
		/* Store both files and folders into the dirlist struct */
		size_t str_len=0;
		if (files_folders > 0) {
			for(i=0;i<files_folders;i++) {
				str_len=strlen(dirlist_folders[i]->d_name);
				dirlist[files]=xcalloc(dirlist[files], str_len+2, 
									   sizeof(char *));
				/* 1 - Every element stored in dirlist[files] will be a char 
				 * pointer, that's why I use sizeof(char *) instead of 
				 * sizeof(char). The sizeof operator returns the size of the 
				 * type passed as arg, in this case 'char *'. Strictly 
				 * speaking, I could also use here 'int *' or 'float *', and 
				 * even 'char **', since all pointer types equal to 8 (a 64 
				 * bits memory address, like 0xffffff6d346fad). Note: 32 bits 
				 * memory addresses are 4 bytes long instead of 8. However, 
				 * for code legibility I choose 'char *'.
				 * 2 - On the other side, I use 'str_len +1' to make room for 
				 * the NULL terminator, not counted by strlen */
				/* I don't need here strncpy, since I already reserved enough
				 * space in dest for source via calloc */
				strcpy(dirlist[files++]->d_name, dirlist_folders[i]->d_name);
				free(dirlist_folders[i]);
			}
			free(dirlist_folders);
		}
		if (files_files > 0) {
			for(i=0;i<files_files;i++) {
				str_len=strlen(dirlist_files[i]->d_name);
				dirlist[files]=xcalloc(dirlist[files], str_len+3, 
									   sizeof(char *));
				/* str_len +3: I originally used +1, which seems more logical,
				 * but needed to replace it by +2 to fix some error. However,
				 * when I tried it on my P4 machine, it still produced errors,
				 * so that I incremented the value again to 3, and the errors
				 * are gone. Here's the programmer's question: Why the heck 
				 * does it work?! */
				strcpy(dirlist[files++]->d_name, dirlist_files[i]->d_name);
				free(dirlist_files[i]);
			}
			free(dirlist_files);
		}
	}
	
	/* If no list_folders_first */
	else {
		/* Completely free the dirlist struct, since it will be 
		 * (re)allocated by scandir */
		while (files--) free(dirlist[files]);
		free(dirlist);
		files=scandir(path, &dirlist, skip_implied_dot, 
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
				atexit(free_stuff);
			else
				return;
		}
	}
	
	if (files == 0) {
		if (clear_screen)
			CLEAR;
		printf("%s. ..%s\n", blue, NC);
		return;
	}

	int files_num=0;
	/* Get the longest element */
	longest=0; /* Global */
	struct stat file_attrib;
	for (i=files;i--;) {
		int ret=lstat(dirlist[i]->d_name, &file_attrib);
		if (ret == -1)
			printf(_("Error: %s: %d: %s: %s\n"), path, i, dirlist[i]->d_name, 
				   strerror(errno));
		/* file_name_width contains: ELN's number of digits + one space 
		 * between ELN and file name + filename length. Ex: '12 name' contains 
		 * 7 chars */
		int file_name_width=digits_in_num(i+1)+1+(
							(unicode) ? u8_xstrlen(dirlist[i]->d_name) 
							: strlen(dirlist[i]->d_name));
		/* If the file is a non-empty directory and the user has access 
		 * permision to it, add to file_name_width the number of digits of the 
		 * amount of files this directory contains (ex: 123 (files) contains 3 
		 * digits) + 2 for space and slash between the directory name and the 
		 * amount of files it contains. Ex: '12 name /45' contains 11 chars */
		if ((file_attrib.st_mode & S_IFMT) == S_IFDIR
		&& access(dirlist[i]->d_name, R_OK|X_OK) == 0)
			if ((files_num=count_dir(dirlist[i]->d_name)) > 2)
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
				/*	Check terminal amount of rows: if as many filenames as the
				 * amount of available terminal rows has been printed, stop */
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
			printf("%s%d%s ", yellow, i+1, NC);
			get_properties(dirlist[i]->d_name, (int)long_view, max);
		}
		if (reset_pager)
			pager=1;
		return;
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
	char last_column=0; // c, reset_pager=0;
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
		if ((i+1)%columns_n == 0) last_column=1;
		else last_column=0;
		lstat(dirlist[i]->d_name, &file_attrib);
		switch (file_attrib.st_mode & S_IFMT) {
			case S_IFDIR:
				if (access(dirlist[i]->d_name, R_OK|X_OK) != 0)
					printf("%s%d %s%s%s%s", yellow, i+1, red, 
						   dirlist[i]->d_name, NC, (last_column) ? "\n" : "");
				else {
					int is_oth_w=0;
					if (file_attrib.st_mode & S_IWOTH) is_oth_w=1;
					files_dir=count_dir(dirlist[i]->d_name);
					if (files_dir == 2 || files_dir == 0) { /* If folder is 
					* empty, it contains only "." and ".." (2 elements). If 
					* not mounted (ex: /media/usb) the result will be zero */
						/* If sticky bit dir: green bg */
						printf("%s%d %s%s%s%s", yellow, i+1, 
							(file_attrib.st_mode & S_ISVTX) ? ((is_oth_w) ? 
							bg_green_fg_blue : bg_blue_fg_white) : ((is_oth_w) 
							? bg_green_fg_black : d_blue), dirlist[i]->d_name, 
							NC, (last_column) ? "\n" : "");
					}
					else {
						printf("%s%d %s%s%s%s /%d%s%s", yellow, i+1, 
							(file_attrib.st_mode & S_ISVTX) ? ((is_oth_w) ? 
							bg_green_fg_blue : bg_blue_fg_white) : ((is_oth_w) 
							? bg_green_fg_black : blue), dirlist[i]->d_name, 
							NC, white_b, files_dir-2, NC, (last_column) 
							? "\n" : "");
						is_dir=1;
					}
				}
			break;
			case S_IFIFO:
				printf("%s%d %s%s%s%s", yellow, i+1, d_magenta, 
						dirlist[i]->d_name, NC, (last_column) ? "\n" : "");
			break;
			case S_IFLNK:
				linkname=realpath(dirlist[i]->d_name, NULL);
				if (linkname) {
					printf("%s%d %s%s%s%s", yellow, i+1, cyan, 
							dirlist[i]->d_name, NC, (last_column) ? "\n" : "");
					free(linkname);
				}
				else
					printf("%s%d %s%s%s%s", yellow, i+1, d_cyan, 
							dirlist[i]->d_name, NC, (last_column) ? "\n" : "");
			break;
			case S_IFBLK:
				printf("%s%d %s%s%s%s", yellow, i+1, yellow, 
						dirlist[i]->d_name, NC, (last_column) ? "\n" : "");
			break;
			case S_IFCHR:
				printf("%s%d %s%s%s%s", yellow, i+1, white, 
						dirlist[i]->d_name, NC, (last_column) ? "\n" : "");
			break;
			case S_IFSOCK:
				printf("%s%d %s%s%s%s", yellow, i+1, magenta, 
						dirlist[i]->d_name, NC, (last_column) ? "\n" : "");
			break;
			case S_IFREG:
				if (!(file_attrib.st_mode & S_IRUSR))
					printf("%s%d %s%s%s%s", yellow, i+1, d_red, 
							dirlist[i]->d_name, NC, (last_column) ? "\n" : "");
				else if (file_attrib.st_mode & S_ISUID) //set uid file
					printf("%s%d %s%s%s%s", yellow, i+1, bg_red_fg_white, 
							dirlist[i]->d_name, NC, (last_column) ? "\n" : "");
				else if (file_attrib.st_mode & S_ISGID) //set gid file
					printf("%s%d %s%s%s%s", yellow, i+1, bg_yellow_fg_black, 
						dirlist[i]->d_name, NC, (last_column) ? "\n" : "");
				else if ((cap=cap_get_file(dirlist[i]->d_name))) {
					printf("%s%d %s%s%s%s", yellow, i+1, bg_red_fg_black, 
							dirlist[i]->d_name, NC, (last_column) ? "\n" : "");				
					cap_free(cap);
				}
				else if (file_attrib.st_mode & S_IXUSR)
					if (file_attrib.st_size == 0)
						printf("%s%d %s%s%s%s", yellow, i+1, d_green, 
								dirlist[i]->d_name, NC, (last_column) 
								? "\n" : "");
					else
						printf("%s%d %s%s%s%s", yellow, i+1, green, 
								dirlist[i]->d_name, NC, (last_column) 
								? "\n" : "");
				else if (file_attrib.st_size == 0)
					printf("%s%d %s%s%s%s", yellow, i+1, d_yellow, 
							dirlist[i]->d_name, NC, (last_column) ? "\n" : "");
				else
					printf("%s%d%s %s%s%s%s", yellow, i+1, NC, white_b, 
							dirlist[i]->d_name, NC, (last_column) ? "\n" : "");
			break;
			/* In case all of the above conditions are false, we have an
			 * unknown file type */
			default: 
				printf("%s%d %s%s%s%s", yellow, i+1, bg_white_fg_red, 
						dirlist[i]->d_name, NC, (last_column) ? "\n" : "");
		}
		if (!last_column) {
			/* Get the difference between the length of longest and the 
			 * current element */
			int diff=longest-(digits_in_num(i+1)+1+
							  ((unicode) ? u8_xstrlen(dirlist[i]->d_name) 
							  : xstrlen(dirlist[i]->d_name)));
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
		printf("%s=", d_blue);
	printf("%s%s", NC, white_b);
	fflush(stdout);

/*	clock_t end=clock();
	printf("list_dir time: %f\n", (double)(end-start)/CLOCKS_PER_SEC);
	getchar(); */
}

void
get_aliases_n_prompt_cmds(void)
/* Copy aliases found in clifmrc to a tmp file: /tmp/.clifm_aliases */
{
	FILE *config_file_fp;
	config_file_fp=fopen(CONFIG_FILE, "r");
	if (config_file_fp == NULL) {
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
	char line[MAX_LINE]="";
	int prompt_line_found=0;
	while (fgets(line, sizeof(line), config_file_fp)) {
		if (strncmp(line, "alias", 5) == 0) {
			char *alias_line=straft(line, ' ');	
			aliases=xrealloc(aliases, sizeof(char **)*(aliases_n+1));
			aliases[aliases_n]=xcalloc(aliases[aliases_n], 256, 
									   sizeof(char *));
			strcpy(aliases[aliases_n++], alias_line);
			free(alias_line);
		}
		else if (prompt_line_found) {
			if (strncmp(line, "#END OF PROMPT", 14) == 0) break;
			if (strncmp(line, "#", 1) != 0) {
				prompt_cmds=xrealloc(prompt_cmds, 
					sizeof(char **)*(prompt_cmds_n+1));
				prompt_cmds[prompt_cmds_n]=xcalloc(prompt_cmds[prompt_cmds_n], 
												   256, sizeof(char *));
				strcpy(prompt_cmds[prompt_cmds_n++], line);
			}
		}
		else if (strncmp(line, "#PROMPT", 7) == 0) 
			prompt_line_found=1;
	}
	fclose(config_file_fp);
}

char **
check_for_alias(char **comm)
/* If a matching alias is found, execute the corresponding command, if it 
 * exists. Returns one if matching alias is found, zero if not. */
{
	char *aliased_cmd=NULL;
	char comm_tmp[MAX_LINE]="";
	/* Look for this string: "command=", in the aliases file */
	snprintf(comm_tmp, MAX_LINE, "%s=", comm[0]);
	size_t comm_len=strlen(comm[0]);
	for (size_t i=0;i<aliases_n;i++) {
		if (strncmp(aliases[i], comm_tmp, comm_len+1) == 0) {
			/* Get the aliased command */
			aliased_cmd=strbtw(aliases[i], '\'', '\'');
			if (!aliased_cmd) return NULL;
			if (aliased_cmd[0] == '\0') { // zero length
				free(aliased_cmd);
				return NULL;
			}
			char *command_tmp=NULL;
			command_tmp=xcalloc(command_tmp, PATH_MAX, sizeof(char *));
			strncpy(command_tmp, aliased_cmd, PATH_MAX); 
			free(aliased_cmd);
			args_n=0; /* Reset args_n to be used by parse_input_str() */
			/* Parse the aliased cmd */
			char **alias_comm=parse_input_str(command_tmp);
			free(command_tmp);
			/* Add input parameters, if any, to alias_comm */
			for (size_t j=1;comm[j];j++) {
				alias_comm=xrealloc(alias_comm, sizeof(char **)*(++args_n+1));
				alias_comm[args_n]=xcalloc(alias_comm[args_n], 
										   strlen(comm[j])+1, sizeof(char *));
				strcpy(alias_comm[args_n], comm[j]);
			}
			/* Add a final null */
			alias_comm=xrealloc(alias_comm, sizeof(char **)*(args_n+2));			
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

void
exec_cmd(char **comm)
/* Take the command entered by the user and call the corresponding function. */
{
	/* If sel is an argument, but there are no selected files, do nothing. The 
	 * error message is printed by parse_input_str() */
	if (sel_no_sel)
		return;
	
	/* If a user defined variable */
	if (flags & IS_USRVAR_DEF) {
		flags &= ~IS_USRVAR_DEF;
		create_usr_var(comm[0]);
		return;
	}
	/* If double semi colon or colon */
	if (comm[0][0] == ';' || comm[0][0] == ':') {
		if (comm[0][1] == ';' || comm[0][1] == ':') {
			fprintf(stderr, _("%s: '%.2s': Syntax error\n"), PROGRAM_NAME, 
					comm[0]);
			return;
		}
	}

	/* The more often a function is used, the more on top should it be in this 
	 * if...else..if chain. It will be found faster this way. */

	if (strcmp(comm[0], "cd") == 0)
		cd_function(comm[1]);
	else if (strcmp(comm[0], "o") == 0	|| strcmp(comm[0], "open") == 0)
		open_function(comm);
	else if (strcmp(comm[0], "rf") == 0 || strcmp(comm[0], "refresh") == 0) {
		if (cd_lists_on_the_fly) {
			while (files--) free(dirlist[files]);
			list_dir();
		}
	}
	else if (strcmp(comm[0], "bm") == 0 || strcmp(comm[0], "bookmarks") == 0)
		bookmarks_function();
	else if (strcmp(comm[0], "b") == 0 || strcmp(comm[0], "back") == 0)
		back_function(comm);
	else if (strcmp(comm[0], "f") == 0 || strcmp(comm[0], "forth") == 0)
		forth_function(comm);
	else if (strcmp(comm[0], "bh") == 0 || strcmp(comm[0], "fh") == 0) {
		for (int i=0;i<dirhist_total_index;i++) {
			if (i == dirhist_cur_index)
				printf("%d %s%s%s%s\n", i+1, green, old_pwd[i], NC, white_b);
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
		copy_function(comm);
	}
	else if (strcmp(comm[0], "tr") == 0 || strcmp(comm[0], "t") == 0 
	|| strcmp(comm[0], "trash") == 0) {
		trash_function(comm);
		if (is_sel) { /* If 'tr sel', deselect everything */
			for (size_t i=0;i<sel_n;i++)
				free(sel_elements[i]);
			sel_n=0;
			save_sel();
		}
	}
	else if (strcmp(comm[0], "undel") == 0 || strcmp(comm[0], "u") == 0
	|| strcmp(comm[0], "untrash") == 0)
		untrash_function(comm);
	else if (strcmp(comm[0], "s") == 0 || strcmp(comm[0], "sel") == 0)
		sel_function(comm);
	else if (strcmp(comm[0], "sb") == 0 || strcmp(comm[0], "selbox") == 0)
		show_sel_files();
	else if (strcmp(comm[0], "ds") == 0 || strcmp(comm[0], "desel") == 0)
		deselect(comm);
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
		run_and_refresh(comm);
	}
	else if (strcmp(comm[0], "pr") == 0 || strcmp(comm[0], "prop") == 0 
	|| strcmp(comm[0], "p") == 0)
		properties_function(comm);
									/* If not absolute path */ 
	else if (comm[0][0] == '/' && access(comm[0], F_OK) != 0)
		search_function(comm);
	/* History function: if '!number' or '!-number' or '!!' */
	else if (comm[0][0] == '!') run_history_cmd(comm[0]+1);
	else if (strcmp(comm[0], "ls") == 0 && !cd_lists_on_the_fly) {
		while (files--) free(dirlist[files]);
		list_dir();
		get_sel_files();
	}
	else if (strcmp(comm[0], "mp") == 0 
	|| (strcmp(comm[0], "mountpoints") == 0))
		list_mountpoints();
	else if (strcmp(comm[0], "ext") == 0) {
		if (!comm[1] || strcmp(comm[1], "--help") == 0) 
			fprintf(stderr, _("%s: Usage: ext [on, off, status]\n"), 
					PROGRAM_NAME);					
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
				fprintf(stderr, _("%s: Usage: ext [on, off, status]\n"), 
						PROGRAM_NAME);
		}
	}
	else if (strcmp(comm[0], "pg") == 0 || strcmp(comm[0], "pager") == 0) {
		if (!comm[1] || strcmp(comm[1], "--help") == 0) 
			printf(_("%s: Usage: pager [on, off, status]\n"), PROGRAM_NAME);
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
				printf(_("%s: Usage: pager [on, off, status]\n"), 
					   PROGRAM_NAME);
		}
	}
	else if (strcmp(comm[0], "uc") == 0 || strcmp(comm[0], "unicode") == 0) {
		if (!comm[1] || strcmp(comm[1], "--help") == 0)
			printf(_("%s: Usage: unicode [on, off, status]\n"), PROGRAM_NAME);
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
				printf(_("%s: Usage: unicode [on, off, status]\n"), 
					   PROGRAM_NAME);
		}
	}
	else if ((strcmp(comm[0], "folders") == 0 
	&& strcmp(comm[1], "first") == 0) || strcmp(comm[0], "ff") == 0) {
		if (cd_lists_on_the_fly == 0) 
			return;
		int n=0;
		if (strcmp(comm[0], "ff") == 0) n=1;
		else n=2;
		if (comm[n]) {
			int status=list_folders_first;
			if (strcmp(comm[n], "status") == 0)
				printf(_("%s: Folders first %s\n"), PROGRAM_NAME, 
					   (list_folders_first) ? _("enabled") : _("disabled"));
			else if (strcmp(comm[n], "on") == 0)
				list_folders_first=1;
			else if (strcmp(comm[n], "off") == 0)
				list_folders_first=0;
			else {
				fputs(_("Usage: folders first [on, off, status]\n"), stderr);
				return;			
			}
			if (list_folders_first != status) {
				if (cd_lists_on_the_fly) {
					while (files--) free(dirlist[files]);
					list_dir();
				}
			}
		}
		else {
			fputs(_("Usage: folders first [on, off, status]\n"), stderr);
		}
	}
	else if (strcmp(comm[0], "log") == 0) log_function(comm);
/*	else if (strcmp(comm[0], "msg_test") == 0) {
		error_msg=1;
		asprintf(&msg, "Test error\n");
		if (msg) {
			error_msg=1;
			log_msg(msg, PRINT_PROMPT);
			free(msg);
		}
	}*/
	else if (strcmp(comm[0], "msg") == 0 || strcmp(comm[0], "messages") == 0) {
		if (comm[1] && strcmp(comm[1], "clean") == 0) {
			if (!msgs_n) {
				printf(_("%s: There are no messages\n"), PROGRAM_NAME);
				return;
			}
			for (size_t i=0;i<msgs_n;i++)
				free(messages[i]);
			msgs_n=0;
			error_msg=warning_msg=notice_msg=0;
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
		if (aliases_n == 0)
			return;
		for (size_t i=0;i<(size_t)aliases_n;i++)
			printf("%s", aliases[i]);	
	}
	else if (strcmp(comm[0], "edit") == 0) edit_function(comm);
	else if (strcmp(comm[0], "history") == 0) history_function(comm);
	else if (strcmp(comm[0], "hf") == 0 
	|| strcmp(comm[0], "hidden") == 0)
		hidden_function(comm);
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
	else if (strcmp(comm[0], "bonus") == 0 || strcmp(comm[0], "boca") == 0)
		bonus_function();
	else if (strcmp(comm[0], "splash") == 0) splash();
	else if (strcmp(comm[0], "q") == 0 || strcmp(comm[0], "quit") == 0 
	|| strcmp(comm[0], "exit") == 0 || strcmp(comm[0], "zz") == 0 
	|| strcmp(comm[0], "salir") == 0 || strcmp(comm[0], "chau") == 0) {
		/* #####free everything##### */
		free_stuff();
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
					return;
				}
			}
		}
		
		/* LOG EXTERNAL COMMANDS 
		* 'no_log' will be true when running profile or prompt commands */
		if (!no_log)
			log_function(comm);
		
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
					return;
				}
			}
		}
		
		/* CHECK WHETHER EXTERNAL COMMANDS ARE ALLOWED */
		if (!ext_cmd_ok) {
			fprintf(stderr, _("%s: External commands are not allowed\n"), 
					PROGRAM_NAME);
			return;
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
			if (!comm_tmp || comm_tmp[0] == '\0') {
				fprintf(stderr, _("%s: '%c': Syntax error\n"), PROGRAM_NAME, 
						comm[0][0]);
				return;
			}
			else
				strcpy(comm[0], comm_tmp);
		}
		
		/* External command are always executed by execle, that is to say, by
		 * the system shell, because in this way the user can make use of all 
		 * the shell features, like pipes, stream redirection and so on */
		
		/* Store the cmd and each argument into a single array to be executed 
		 * by execle() */
		char *ext_cmd=NULL;
		size_t ext_cmd_len=strlen(comm[0]);
		ext_cmd=xcalloc(ext_cmd, ext_cmd_len+1, sizeof(char *));
		strcpy(ext_cmd, comm[0]);
		if (args_n) { /* This will be false in case of ";cmd" or ":cmd" */
			for (size_t i=1;i<=(size_t)args_n;i++) {
				/* If some argument contains spaces, quote the whole argument.
				* In this way, the system shell will be able to run the command 
				* correctly. This is just a workaround, but it works most of 
				* the times */
				char *comm_tmp=quote_str(comm[i]);
				if (comm_tmp) {
					ext_cmd_len+=strlen(comm_tmp)+1;
					ext_cmd=xrealloc(ext_cmd, (ext_cmd_len+1)*sizeof(char *));
					strcat(ext_cmd, " ");
					strcat(ext_cmd, comm_tmp);
					free(comm_tmp);
				}
				else {
					ext_cmd_len+=strlen(comm[i])+1;
					ext_cmd=xrealloc(ext_cmd, (ext_cmd_len+1)*sizeof(char *));
					strcat(ext_cmd, " ");
					strcat(ext_cmd, comm[i]);
				}
			}
		}

		/* Run the command */
		launch_execle(ext_cmd);
		free(ext_cmd);
		return;
	}
}

void
surf_hist(char **comm)
{
	if (strcmp(comm[1], "h") == 0 || strcmp(comm[1], "hist") == 0) {
		/* Show the list of already visited directories */
		for (int i=0;i<dirhist_total_index;i++) {
			if (i == dirhist_cur_index)
				printf("%d %s%s%s%s\n", i+1, green, old_pwd[i], NC, white_b);
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
							 sizeof(char *));
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
		if (execle("/bin/sh", "sh", "-c", cmd, NULL, __environ) == -1) {
			fprintf(stderr, "%s: execle: %s\n", PROGRAM_NAME, strerror(errno));
			return EXEXECERR;
		}
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
			return EXWAITERR;
		}
	}

	/* Never reached */
	return EXIT_FAILURE;
}

int
launch_execve(char **cmd)
/* Execute a command and return the corresponding exit status. The exit status
* could be: zero, if everything went fine or a non-zero value in case of error. 
* The function takes as only arguement an array of strings containing the 
* command name to be executed and its arguments (cmd) */
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
	
	if (cmd == NULL) return EXNULLERR;

	/* Reenable SIGCHLD, in case it was disabled. Otherwise, waitpid won't be 
	 * able to catch error codes coming from the child. */
	signal(SIGCHLD, SIG_DFL);

	int status;
	/* Create a new process via fork() */
	pid_t pid=fork();
	if (pid < 0) {
		fprintf(stderr, "%s: fork: %s\n", PROGRAM_NAME, strerror(errno));
		return EXFORKERR;
	}
	/* Run the command via execvpe */
	else if (pid == 0) {
		/* Reenable signals only for the child, in case they were disabled for
		the parent */
		signal(SIGHUP, SIG_DFL);
		signal(SIGINT, SIG_DFL);
		signal(SIGQUIT, SIG_DFL);
		signal(SIGTERM, SIG_DFL);
		if (execvpe(cmd[0], cmd, __environ) == -1) {
			fprintf(stderr, "%s: execle: %s\n", PROGRAM_NAME, strerror(errno));
			return EXEXECERR;
		}
	}
	/* Get the command status */
	else if (pid > 0) {
		/* The parent process calls waitpid() on the child */
		if (waitpid(pid, &status, 0) > 0) {
			if (WIFEXITED(status) && !WEXITSTATUS(status)) {
				/* The program terminated normally and executed successfully */
				return EXIT_SUCCESS;
			}
			else if (WIFEXITED(status) && WEXITSTATUS(status)) {
				switch (WEXITSTATUS(status)) {
					case 126: fprintf(stderr, _("%s: '%s': Permission \
denied\n"), PROGRAM_NAME, cmd[0]);
						break;
					case 127: fprintf(stderr, _("%s: '%s': Command not \
found\n"), PROGRAM_NAME, cmd[0]);
						break;
					default: /* Program terminated normally, but returned a 
					non-zero status */
						break;
				}
				return WEXITSTATUS(status);
			}
			else {
				/* The program didn't terminate normally */
				return EXCRASHERR;
			}
		}
		else {
			/* waitpid() failed */
			fprintf(stderr, "%s: waitpid: %s\n", PROGRAM_NAME, 
					strerror(errno));
			return EXWAITERR;
		}
	}
	
	/* Never reached */
	return EXIT_FAILURE;
}

int
run_in_foreground(pid_t pid)
{
	int status;
	/* The parent process calls waitpid() on the child */
	if (waitpid(pid, &status, 0) > 0) {
		if (WIFEXITED(status) && !WEXITSTATUS(status)) {
			/* The program terminated normally and executed successfully */
			return EXIT_SUCCESS;
		}
		else if (WIFEXITED(status) && WEXITSTATUS(status)) {
			switch (WEXITSTATUS(status)) {
				case 126: fprintf(stderr, _("%s: Permission denied\n"), 
								PROGRAM_NAME);
					break;
				case 127: fprintf(stderr, _("%s: Command not found\n"), 
								PROGRAM_NAME);
					break;
				default: /* Program terminated normally, but returned a 
				non-zero status */
					break;
			}
			return WEXITSTATUS(status);
		}
		else {
			/* The program didn't terminate normally */
			return EXCRASHERR;
		}
	}
	else {
		/* waitpid() failed */
		fprintf(stderr, "%s: waitpid: %s\n", PROGRAM_NAME, strerror(errno));
		return EXWAITERR;
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
	if (sel_n == 0) {
		remove(sel_file_user);
		return 0;
	}
	else {
		FILE *sel_fp=fopen(sel_file_user, "w");
		if (sel_fp == NULL) {
			asprintf(&msg, "%s: sel: '%s': %s\n", PROGRAM_NAME, 
					 sel_file_user, strerror(errno));
			if (msg) {
				log_msg(msg, NOPRINT_PROMPT);
				free(msg);
			}
			else
				fprintf(stderr, "%s: sel: '%s': %s\n", PROGRAM_NAME, 
						sel_file_user, strerror(errno));
			return 0;
		}
		for (int i=0;i<sel_n;i++) {
			fprintf(sel_fp, "%s", sel_elements[i]);
			fprintf(sel_fp, "\n");
		}
		fclose(sel_fp);
		return 1;
	}
}

void
sel_function(char **comm)
{
	if (args_n == 0 || (comm[1] && strcmp(comm[1], "--help") == 0)) {
		puts(_("Usage: sel [ELN ELN-ELN filename path... n]"));
		return;
	}
	char *sel_tmp=NULL;
	int i=0, j=0, exists=0;

	for (i=1;i<=args_n;i++) {
		if (strcmp(comm[i], ".") == 0 || strcmp(comm[i], "..") == 0)
			continue;
		/* If a filename in the current directory... */
		int sel_is_filename=0, sel_is_relative_path=0;
		for (j=0;j<files;j++) {
			if (strcmp(dirlist[j]->d_name, comm[i]) == 0) {
				sel_is_filename=1;
				break;
			}
		}
		if (!sel_is_filename) {
			/* If a path (contains a slash)... */
			if (strcntchr(comm[i], '/') != -1) {
				if (comm[i][0] != '/') /* If relative path */
					sel_is_relative_path=1;
				struct stat file_attrib;
				if (stat(comm[i], &file_attrib) != 0) {
					fprintf(stderr, "%s: %s: '%s': %s\n", PROGRAM_NAME, 
							comm[0], comm[i], strerror(errno));
					continue;
				}
			}
			else { /* If neither a filename nor a valid path... */
				fprintf(stderr, _("%s: %s: '%s': No such file or directory\n"), 
						PROGRAM_NAME, comm[0], comm[i]);
				continue;
			}
		}
		if (sel_is_filename || sel_is_relative_path) { 
			/* Add path to filename or relative path */
			sel_tmp=xcalloc(sel_tmp, strlen(path)+strlen(comm[i])+2, 
				sizeof(char *));
			sprintf(sel_tmp, "%s/%s", path, comm[i]);
		}
		else { /* If absolute path... */
			sel_tmp=xcalloc(sel_tmp, strlen(comm[i])+1, sizeof(char *));		
			strcpy(sel_tmp, comm[i]);
		}
		/* Check whether the selected element is already in the selection 
		 * box */
		for (j=0;j<sel_n;j++) {
			if (strcmp(sel_elements[j], sel_tmp) == 0) {
				exists=1;
				break;
			}
		}
		if (!exists) {
			sel_elements=xrealloc(sel_elements, sizeof(char **)*(sel_n+1));
			sel_elements[sel_n]=xcalloc(sel_elements[sel_n], 
				strlen(sel_tmp)+1, sizeof(char *));
			strcpy(sel_elements[sel_n++], sel_tmp);
		}
		else fprintf(stderr, _("%s: %s: '%s': Already selected\n"), 
					 PROGRAM_NAME, comm[0], sel_tmp);
		free(sel_tmp);
		continue;
	}
	save_sel();
	if (sel_n > 10)
		printf(_("%d elements are now in the Selection Box\n"), sel_n);
	else if (sel_n > 0) {
		printf(_("%d selected %s:\n"), sel_n, (sel_n == 1) ? _("element") : 
			_("elements"));
		for (i=0;i<sel_n;i++)
			printf("  %s\n", sel_elements[i]);
	}
}

void
show_sel_files(void)
{
	if (clear_screen)
		CLEAR;
	printf(_("%sSelection Box%s%s\n"), white, NC, white_b);
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
	if (reset_pager) pager=1;
}

void
deselect (char **comm)
{
	/* This is the old 'ds *', not working now because of the autotamic 
	 * wildacards expansion */
	if (comm[1] && (strcmp(comm[1], "a") == 0 || strcmp(comm[1], "all") == 0)) {
		if (sel_n > 0) {
			for (int i=0;i<sel_n;i++)
				free(sel_elements[i]);
			sel_n=0;
			save_sel();
			return;
		}
		else {
			puts(_("desel: There are no selected files"));
			return;
		}
	}
	
	int i; 
	if (clear_screen)
		CLEAR;
	printf(_("%sSelection Box%s%s\n"), white, NC, white_b);
	if (sel_n == 0) {
		puts(_("Empty"));
		return;
	}
	puts("");
	for (i=0;i<sel_n;i++)
		colors_list(sel_elements[i], i+1, 0, 1);

	printf(_("\n%s%sEnter 'q' to quit.\n"), NC, white_b);
	int no_space=0, desel_n=0;
	char *line=NULL, **desel_elements=NULL;
	while (!line) {
		line=rl_no_hist(_("Elements to be deselected (ex: 1 2 6, or *)? "));
		if (!line) continue;
		for (i=0;line[i];i++)
			if (line[i] != 0x20)
				no_space=1;
		if (line[0] == '\0' || !no_space) {
			free(line);
			line=NULL;
		}
	}
	desel_elements=get_substr(line, ' ');
	free(line);
	if (desel_elements)
		for (i=0;desel_elements[i];i++)
			desel_n++;
	else
		return;

	for (i=0;i<desel_n;i++) { /* Validation */
		int atoi_desel=atoi(desel_elements[i]);
		if (atoi_desel == 0 || atoi_desel > sel_n) {
			if (strcmp(desel_elements[i], "q") == 0) {
				for (i=0;i<desel_n;i++)
					free(desel_elements[i]);
				free(desel_elements);
				return;
			}
			else if (strcmp(desel_elements[i], "*") == 0) {
				/* Clean the sel array */
				for (i=0;i<sel_n;i++)
					free(sel_elements[i]);
				sel_n=0;
				for (i=0;i<desel_n;i++)
					free(desel_elements[i]);
				save_sel();
				free(desel_elements);
				if (cd_lists_on_the_fly) {
					while (files--) free(dirlist[files]);
					list_dir();
				}
				return;
			}
			else {
				printf(_("desel: '%s': Invalid element\n"), desel_elements[i]);
				for (i=0;i<desel_n;i++)
					free(desel_elements[i]);
				free(desel_elements);
				return;
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
							  strlen(sel_elements[desel_int-1]), 
							  sizeof(char *));
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
		for (int j=desel_index;j<sel_n-1;j++)
			strcpy(sel_elements[j], sel_elements[j+1]);
	}
	/* Free the last DESEL_N elements from the old sel array. They won't be 
	 * used anymore, for they contain the same value as the last non-deselected 
	 * element due to the above array rearrangement */
	for (i=1;i<=desel_n;i++)
		free(sel_elements[sel_n-i]);

	/* Reallocate the sel array according to the new size */
	sel_n=sel_n-desel_n;
	if (sel_n)
		sel_elements=xrealloc(sel_elements, sizeof(char **)*sel_n);

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
		comm=xrealloc(comm, sizeof(char **)*1);
		args_n=0;
	}
	save_sel();
	
	/* If there is still some selected file, reload the sel screen */
	if (sel_n)
		deselect(comm);
}

void
run_and_refresh(char **comm)
/* Run a command via execle() and refresh the screen in case of success */
{
	if (!comm)
		return;
	log_function(comm);

	char cmd[PATH_MAX]="";
	strcat(cmd, comm[0]);
	for (size_t i=1;i<=args_n;i++) {
		strcat(cmd, " ");
		/* If string contains spaces, quote it */
		char *str=quote_str(comm[i]);
		if (str) {
			strcat(cmd, str);
			free(str);
		}
		else
			strcat(cmd, comm[i]);
	}

	int ret=launch_execle(cmd);
	if (ret == 0) {
		/* If 'rm sel' and command is successful, deselect everything */
		if (is_sel && strcmp(comm[0], "rm") == 0) {
			for (size_t i=0;i<sel_n;i++)
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
	}
	/* Error messages will be printed by launch_execve() itself */
}

void
search_function(char **comm)
{
	if (!comm || !comm[0])
		return;
	/* If search string (comm[0]) is "/search", comm[0]+1 returns "search" */
	char *search_str=comm[0]+1;
	
	/* If wildcards, use glob() */
	if (strcntchr(search_str, '*') != -1 
	|| strcntchr(search_str, '?') != -1 || strcntchr(search_str, '[') != -1) {
		/* If second argument ("/search_str /path"), chdir into it, since 
		 * glob() works on CWD */
		if (comm[1] && comm[1][0] != '\0') {
			if (chdir(comm[1]) == -1) {
				fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, comm[1], 
						strerror(errno));
				return;
			}
		}
		/* Get globbed files */
		glob_t globbed_files;		
		int ret=glob(search_str, 0, NULL, &globbed_files);
		if (ret == 0) {
			size_t index=0;
			if (comm[1])
				for (size_t i=0;globbed_files.gl_pathv[i];i++)
					colors_list(globbed_files.gl_pathv[i], 0, 0, 1);
					/* Second argument to colors_list() is:
					 * 0: Do not print any ELN 
					 * Positive number: Print positive number as ELN
					 * -1: Print "?" instead of an ELN */
			else {
				for (size_t i=0;globbed_files.gl_pathv[i];i++) {
					/* In case 'index' is not found in the next for loop, that 
					 * is, if the globbed file is not found in the current dir 
					 * list, 'index' value would be that of the previous file 
					 * if 'index' is not set to zero in each for iteration */
					index=0;
					for (size_t j=0;j<(size_t)files;j++)
						if (strcmp(globbed_files.gl_pathv[i], 
								   dirlist[j]->d_name) == 0)
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
		if (comm[1])
			if (chdir(path) == -1)
				fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, path, 
						strerror(errno));
	}
	
	/* If no wildcards */
	else {
		short found=0;
		/* If /search_str /path */
		if (comm[1] && comm[1][0] != '\0') {
			struct dirent **search_list;
			int search_files=0;
			search_files=scandir(comm[1], &search_list, NULL, alphasort);
			if (search_files == -1) {
				fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, comm[1], 
						strerror(errno));
				return;
			}
			if (chdir(comm[1]) == -1) {
				fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, comm[1], 
						strerror(errno));
				for (size_t i=0;i<search_files;i++)
					free(search_list[i]);
				free(search_list);
				return;
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
				return;
			}
		}
		
		/* If /search_str */
		else {
			for (size_t i=0;i<(size_t)files;i++) {
				/* strstr finds substr in STR, as if STR where "*substr*" */
				if (strstr(dirlist[i]->d_name, search_str)) {
					colors_list(dirlist[i]->d_name, i+1, 0, 1);
					found=1;
				}
			}
		}

		if (!found)
			printf(_("%s: No matches found\n"), PROGRAM_NAME);
	}
}

void
copy_function(char **comm)
{
	if (strcmp(comm[0], "paste") == 0)
		strncpy(comm[0], "cp\0", 3);

	log_function(comm);

	/* #####If SEL###### */
	if (is_sel) {
		char **cmd=NULL;
		if (sel_is_last) { /* IF "cmd sel", add "." to copy files into CWD */
			cmd=xcalloc(cmd, args_n+3, sizeof(char **));
			for (size_t i=0;i<=args_n;i++) {
				cmd[i]=xcalloc(cmd[i], strlen(comm[i]), sizeof(char *));
				strcpy(cmd[i], comm[i]);
			}
			cmd[args_n+1]=xcalloc(cmd[args_n+1], 2, sizeof(char *));
			strcpy(cmd[args_n+1], ".");
			cmd[args_n+2]=NULL;
		}

		int ret=0;
		if (sel_is_last)
			ret=launch_execve(cmd);
		else
			ret=launch_execve(comm);
		if (ret == 0) {
			/* If 'mv sel' and command is successful deselect everything,
			 * since sel files are note there anymore */
			if (strcmp(comm[0], "mv") == 0) {
				for (size_t i=0;i<sel_n;i++)
					free(sel_elements[i]);
				sel_n=0;
				save_sel();
			}
			if (cd_lists_on_the_fly) {
				while (files--) free(dirlist[files]);
				list_dir();
			}
		}
		if (cmd) { /* Or if (sel_is_last) */
			for (size_t i=0;cmd[i];i++)
				free(cmd[i]);
			free(cmd);
		}		
		return;
	}
	
	/* ####If NOT SEL#######... */
	run_and_refresh(comm);
}

char **
get_bookmarks(char *bookmarks_file)
/* Get bookmarks from BOOKMARKS_FILE, store them in the bookmarks array and
 * the amount of bookmarks in the global variable bm_n */
{
	bm_n=-1; /* This global variable stores the total amount of bookmarks */
	char **bookmarks=NULL;
	FILE *bm_fp;
	char line[MAX_LINE]="";
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
	int ret_nl;
	/* Get bookmarks from the bookmarks file */
	while (fgets(line, sizeof(line), bm_fp)) {
		if (line[0] != '#') { /* If not a comment */
			/* Allocate memory to store the bookmark */
			bookmarks=xrealloc(bookmarks, (bm_n+1)*sizeof(char **));
			bookmarks[bm_n]=xcalloc(bookmarks[bm_n], strlen(line), 
									sizeof(char *));
			/* Store the entire line */
			strcpy(bookmarks[bm_n], line);
			/* Remove new line char from bookmark line, if any */
			ret_nl=strcntchr(bookmarks[bm_n], '\n');
			if (ret_nl != -1)
				bookmarks[bm_n][ret_nl]='\0';
			bm_n++;
		}
	}
	fclose(bm_fp);
	return bookmarks;
}

char **
bm_prompt(void)
{
	char *bm_sel=NULL;
	printf("%s%s\n", NC_b, white_b);
	while (!bm_sel) {
		bm_sel=rl_no_hist(_("Choose a bookmark ([e]dit, [q]uit): "));
		int no_space=0;
		for (size_t i=0;bm_sel[i];i++)
			if (bm_sel[i] != 0x20)
				no_space=1;
		/* If empty or only spaces */
		if (bm_sel[0] == '\0' || !no_space) {
			free(bm_sel);
			bm_sel=NULL;
		}
	}
	char **comm_bm=parse_input_str(bm_sel);
	free(bm_sel);
	return comm_bm;	
}

void
bookmarks_function(void)
{
	if (clear_screen)
		CLEAR;
	printf(_("%sBookmarks Manager%s\n\n"), white, NC);
	struct stat file_attrib;

	// If the bookmarks file doesn't exist, create it
	if (stat(BM_FILE, &file_attrib) == -1) {
		FILE *fp;
		fp=fopen(BM_FILE, "w+");
		if (!fp) {
			asprintf(&msg, "%s: bookmarks: '%s': %s\n", PROGRAM_NAME, 
					 BM_FILE, strerror(errno));
			if (msg) {
				log_msg(msg, NOPRINT_PROMPT);
				free(msg);
			}
			else
				fprintf(stderr, "%s: bookmarks: '%s': %s\n", PROGRAM_NAME, 
						BM_FILE, strerror(errno));
			return;
		}
		else {
			fprintf(fp, "#Example: [t]test:/path/to/test\n");
			fclose(fp);
		}
	}

	// Get bookmarks
	char **bookmarks=get_bookmarks(BM_FILE);
	// get_bookmarks not only returns an array containing the actual bookmarks,
	// but the total amount of bookmarks as well via the global variable bm_n
	if (bm_n == -1) // Error opening BM_FILE
		return; // Errors are printed by get_bookmarks() itself
	// If no bookmarks
	if (bm_n == 0) {
		printf("%s%s%s: ", NC_b, white_b, PROGRAM_NAME);
		char *answer=rl_no_hist(_("There are no bookmarks.\nDo you want to \
edit the bookmarks file? [Y/n] "));
		if (!answer) return;
		if (strcmp(answer, "n") == 0 || strcmp(answer, "N") == 0) {
			free(answer);
			return;
		}
		else if (strcmp(answer, "y") == 0 || strcmp(answer, "Y") == 0 
					|| strcmp(answer, "") == 0) {
			free(answer);
			char edit_cmd[150]="";
			if (flags & XDG_OPEN_OK)
				snprintf(edit_cmd, 150, "xdg-open '%s'", BM_FILE);
			else {
				printf(_("%s: xdg-open: Command not found\n"), PROGRAM_NAME);
				char *editor=rl_no_hist(_("Text editor: "));
				if (!editor)
					return;
				if (editor[0] == '\0') { // If the user just pressed Enter
					free(editor);
					return;
				}
				snprintf(edit_cmd, 150, "%s '%s'", editor, BM_FILE);
				free(editor);
			}
			if (launch_execle(edit_cmd) == 0)
				bookmarks_function();
			return;
		}
		else {
			fprintf(stderr, _("bookmarks: '%s': Wrong answer\n"), answer);
			free(answer);
			return;
		}	
	}

	// If there are bookmarks...
	char **bm_paths=NULL;
	bm_paths=xcalloc(bm_paths, bm_n, sizeof(char **));
	char **hot_keys=NULL;
	hot_keys=xcalloc(hot_keys, bm_n, sizeof(char **));
	char **bm_names=NULL;
	bm_names=xcalloc(bm_names, bm_n, sizeof(char **));
	int i;

	// Get bookmarks paths
	for(i=0;i<bm_n;i++) {
		char *str;
		str=straft(bookmarks[i], ':');
		if (str) {
			bm_paths[i]=xcalloc(bm_paths[i], strlen(str), sizeof(char *));
			strcpy(bm_paths[i], str);
			free(str);
		}
		else {
			str=straft(bookmarks[i], ']');
			if (str) {
				bm_paths[i]=xcalloc(bm_paths[i], strlen(str), sizeof(char *));
				strcpy(bm_paths[i], str);
				free(str);
			}
			else {
				bm_paths[i]=xcalloc(bm_paths[i], strlen(bookmarks[i]), 
					sizeof(char *));
				strcpy(bm_paths[i], bookmarks[i]);
			}
		}
	}

	// Get bookmarks hot keys
	for (i=0;i<bm_n;i++) {
		char *str_b=strbtw(bookmarks[i], '[', ']');
		if (str_b) {
			hot_keys[i]=xcalloc(hot_keys[i], strlen(str_b), sizeof(char *));
			strcpy(hot_keys[i], str_b);
			free(str_b);
		}
		else 
			hot_keys[i]=NULL;
	}

	// Get bookmarks names
	for (i=0;i<bm_n;i++) {
		char *str_name=strbtw(bookmarks[i], ']', ':');
		if (str_name) {
			bm_names[i]=xcalloc(bm_names[i], strlen(str_name), sizeof(char *));
			strcpy(bm_names[i], str_name);
			free(str_name);
		}
		else {
			str_name=strbfr(bookmarks[i], ':');
			if (str_name) {
				bm_names[i]=xcalloc(bm_names[i], strlen(str_name), 
					sizeof(char *));
				strcpy(bm_names[i], str_name);
				free(str_name);
			}
			else bm_names[i]=NULL;
		}
	}

	// Print results
	for (i=0;i<bm_n;i++) {
		int path_ok=stat(bm_paths[i], &file_attrib);
		if (hot_keys[i]) {
			if (bm_names[i]) {
				if (path_ok == 0) {
					if ((file_attrib.st_mode & S_IFMT) == S_IFDIR)
						printf("%s%d %s[%s]%s %s%s%s\n", yellow, i+1, white, 
							hot_keys[i], NC, cyan, bm_names[i], NC);
					else
						printf("%s%d %s[%s]%s %s%s%s\n", yellow, i+1, white, 
							hot_keys[i], NC, white_b, bm_names[i], NC);					
				}
				else
					printf("%s%d [%s] %s%s\n", gray, i+1, hot_keys[i], 
						bm_names[i], NC);
			}
			else {
				if (path_ok == 0) {
					if ((file_attrib.st_mode & S_IFMT) == S_IFDIR)
						printf("%s%d %s[%s]%s %s%s%s\n", yellow, i+1, white, 
							hot_keys[i], NC, cyan, bm_paths[i], NC);
					else
						printf("%s%d %s[%s]%s %s%s%s\n", yellow, i+1, white, 
							hot_keys[i], NC, white_b, bm_paths[i], NC);		
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
						printf("%s%d %s%s%s\n", yellow, i+1, cyan, 
							bm_names[i], NC);
					else
						printf("%s%d %s%s%s%s\n", yellow, i+1, NC, white_b, 
							bm_names[i], NC);				
				}
				else
					printf("%s%d %s%s\n", gray, i+1, bm_names[i], NC);
			}
			else {
				if (path_ok == 0) {
					if ((file_attrib.st_mode & S_IFMT) == S_IFDIR)
						printf("%s%d %s%s%s\n", yellow, i+1, cyan, 
							bm_paths[i], NC);
					else
						printf("%s%d %s%s%s%s\n", yellow, i+1, NC, white_b, 
							bm_paths[i], NC);	
				}
				else
					printf("%s%d %s%s\n", gray, i+1, bm_paths[i], NC);
			}
		}
	}

	// Display the prompt
	int args_n_old=args_n, reload_bm=0, go_dirlist=0; //, go_back_prompt=0;
	args_n=0;
	char **comm_bm=bm_prompt();

	// User selection
	// If string...
	if (!is_number(comm_bm[0])) {
		int valid_hk=0, eln=0;
		if (strcmp(comm_bm[0], "e") == 0) {
			//if no application has been specified and xdg-open doesn't exist...
			if (args_n == 0 && !(flags & XDG_OPEN_OK))
				fprintf(stderr, _("%s: xdg-open not found. Try \
'e application'\n"), PROGRAM_NAME);
			else {
				if (args_n > 0) {
					char *bm_comm_path=NULL;
					if ((bm_comm_path=get_cmd_path(comm_bm[1])) != NULL) {
						pid_t pid_edit_bm=fork();
						if (pid_edit_bm == 0) {
							set_signals_to_default();
							execle(bm_comm_path, comm_bm[1], BM_FILE, NULL, 
								__environ);
							set_signals_to_ignore();
						}
						else waitpid(pid_edit_bm, NULL, 0);
						reload_bm=1;
						free(bm_comm_path);
					}
					else fprintf(stderr, _("bookmarks: '%s': Application not \
found\n"), comm_bm[1]);
				}
				else {
					pid_t pid_edit_bm=fork();
					if (pid_edit_bm == 0) {
						set_signals_to_default();
						execle(xdg_open_path, "xdg-open", BM_FILE, NULL, 
							__environ);
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
					fprintf(stderr, _("bookmarks: %d: No such bookmark\n"), 
							bm_n);
				else fprintf(stderr, _("bookmarks: %s: No such bookmark\n"), 
							 comm_bm[0]);
//				go_back_prompt=1;
			}
			else { //if a valid hotkey...
				if (stat(bm_paths[eln], &file_attrib) == 0) {
					// If a directory
					if ((file_attrib.st_mode & S_IFMT) == S_IFDIR) {
						int ret=chdir(bm_paths[eln]);
						if (ret == 0) {
							free(path);
							path=xcalloc(path, strlen(bm_paths[eln])+1, 
										 sizeof(char *));
							strcpy(path, bm_paths[eln]);
							add_to_dirhist(path);
							go_dirlist=1;
						}
						else {
							fprintf(stderr, "bookmarks: '%s': %s\n", 
									bm_paths[eln], strerror(errno));
						}
					}
					// If a regular file...
					else if ((file_attrib.st_mode & S_IFMT) == S_IFREG) {
						if (args_n == 0) { //if no opening application has been passed...
							if (flags & XDG_OPEN_OK) {
								pid_t pid_bm=fork();
								if (pid_bm == 0) {
									set_signals_to_default();
									execle(xdg_open_path, "xdg-open", 
										bm_paths[eln], NULL, __environ);
									set_signals_to_ignore();
								}
								else waitpid(pid_bm, NULL, 0);
							}
							else fprintf(stderr, _("%s: xdg-open not found. \
Try 'hotkey application'\n"), PROGRAM_NAME);
						}
						else { //if application has been passed (as 2nd arg)...
							char *cmd_path=NULL;
							if ((cmd_path=get_cmd_path(comm_bm[1])) != NULL) { //check if application exists
								pid_t pid_bm=fork();
								if (pid_bm == 0) {
									set_signals_to_default();
									execle(cmd_path, comm_bm[1], bm_paths[eln], 
										   NULL, __environ);
									set_signals_to_ignore();
								}
								else
									waitpid(pid_bm, NULL, 0);
							}
							else {
								fprintf(stderr, _("bookmarks: %s: Application \
not found\n"), comm_bm[1]);
							}
							free(cmd_path);
						}
					}
					else // If neither dir nor regular file
						fprintf(stderr, _("bookmarks: '%s': Cannot open \
file\n"), bm_paths[eln]);
				}
				else {
					fprintf(stderr, "bookmarks: '%s': %s\n",
						   bm_paths[eln], strerror(errno));
				}
			}
		}
	}
	
	// If digit
	else {
		size_t atoi_comm_bm=atoi(comm_bm[0]);
		if (atoi_comm_bm > 0 && atoi_comm_bm <= bm_n) {
			// CHECK FILE EXISTENCE!!
			stat(bm_paths[atoi_comm_bm-1], &file_attrib);
			if ((file_attrib.st_mode & S_IFMT) == S_IFDIR) { //if a directory...
				int ret=chdir(bm_paths[atoi_comm_bm-1]);
				if (ret == 0) {
					free(path);
					path=xcalloc(path, strlen(bm_paths[atoi_comm_bm-1])+1, 
								 sizeof(char *));
					strcpy(path, bm_paths[atoi_comm_bm-1]);
					add_to_dirhist(path);
					go_dirlist=1;
				}
				else
					fprintf(stderr, "bookmarks: '%s': %s\n",  
							bm_paths[atoi_comm_bm-1], strerror(errno));
			}
			else if ((file_attrib.st_mode & S_IFMT) == S_IFREG) { //if a file...
				if (args_n == 0) { //if no opening application has been passed...
					if (!(flags & XDG_OPEN_OK))
						fprintf(stderr, _("%s: xdg-open not found. Try \
'ELN/hot-key application'\n"), PROGRAM_NAME);
					else {
						pid_t pid_bm=fork();
						if (pid_bm == 0) {
							set_signals_to_default();
							execle(xdg_open_path, "xdg-open", 
								bm_paths[atoi_comm_bm-1], NULL, __environ);
							set_signals_to_ignore();
						}
						else waitpid(pid_bm, NULL, 0);
					}
				}
				else { //if application has been passed (as 2nd arg)...
					// Check if application exists
					char *cmd_path=NULL;
					if ((cmd_path=get_cmd_path(comm_bm[1])) != NULL) {
						pid_t pid_bm=fork();
						if (pid_bm == 0) {
							set_signals_to_default();
							execle(cmd_path, comm_bm[1], 
								bm_paths[atoi_comm_bm-1], NULL, __environ);
							set_signals_to_ignore();
						}
						else waitpid(pid_bm, NULL, 0);
					}
					else {
						fprintf(stderr, _("bookmarks: %s: Application not \
found\n"), comm_bm[1]);
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
			fprintf(stderr, _("bookmarks: %s: No such bookmark\n"), 
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
		args_n=args_n_old;
		if (reload_bm) bookmarks_function();

		if (go_dirlist && cd_lists_on_the_fly) {
			while (files--) free(dirlist[files]);
			list_dir();
		}
}

void
dir_size(char *dir)
{
	#ifndef DU_TMP_FILE
		#define DU_TMP_FILE "/tmp/.du_size"
	#endif
	/* Check for 'du' existence just once (the first time the function is 
	 * called) by storing the result in a static variable (whose value will 
	 * survive the function) */
	static char du_path[PATH_MAX]="";
	if (du_path[0] == '\0') {
		char *du_path_tmp=NULL;
		if ((du_path_tmp=get_cmd_path("du")) != NULL) {
			strncpy(du_path, du_path_tmp, strlen(du_path_tmp));
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
	size_t dir_len=strlen(dir);
	char cmd[dir_len+10];
	memset(cmd, 0x00, dir_len+10);
	snprintf(cmd, dir_len+10, "du -sh \"%s\"", dir);
/*	int ret=launch_execle(cmd); */
	launch_execle(cmd);
	dup2(stdout_bk, STDOUT_FILENO); /* Restore original stdout */
	dup2(stderr_bk, STDERR_FILENO); /* Restore original stderr */
	close(stdout_bk);
	close(stderr_bk);
/*	if (ret != 0) {
		puts("error");
		return;
	}*/
	if (access(DU_TMP_FILE, F_OK) == 0) {
		du_fp=fopen(DU_TMP_FILE, "r");
		if (du_fp) {
			char line[MAX_LINE]="";
			fgets(line, sizeof(line), du_fp);
			char *file_size=strbfr(line, '\t');
			if (file_size)
				printf("%siB\n", file_size);
			else puts("strbfr: error");
			free(file_size);
			fclose(du_fp);
		}
		else puts(_("unknown"));
		remove(DU_TMP_FILE);
	}
	else puts(_("unknown"));
}

void
properties_function(char **comm)
{
	if (!comm[1] || strcmp(comm[1], "--help") == 0) {
		puts(_("Usage: pr [ELN/filename(s)] [a, all] [s, size]"));
		return;
	}
	
	if (comm[1] && (strcmp(comm[1], "all") == 0
	|| strcmp(comm[1], "a") == 0)) {
		int status=long_view;
		long_view=1;
		int max=get_max_long_view();
		for (int i=0;i<files;i++) {
			printf("%s%d%s ", yellow, i+1, NC);
			get_properties(dirlist[i]->d_name, (int)long_view, max);
		}
		long_view=status;
		return;
	}
	
	if (comm[1] && (strcmp(comm[1], "size") == 0
	|| strcmp(comm[1], "s") == 0)) { /* List files size */
		struct stat file_attrib;
		/* Get largest filename to format the output */
		int largest=0;
		for (int i=files;i--;) {
			size_t file_len=(unicode) ? u8_xstrlen(dirlist[i]->d_name)
									  : strlen(dirlist[i]->d_name);
			if (file_len > largest)
				largest=file_len+1;
		}
		for (int i=0;i<files;i++) {
			/* Get the amount of continuation bytes for each filename. This
			 * is necessary to properly format the output in case of non-ASCII
			 * strings */
			(unicode) ? u8_xstrlen(dirlist[i]->d_name) 
					  : strlen(dirlist[i]->d_name);
			lstat(dirlist[i]->d_name, &file_attrib);
			char *size=get_size_unit(file_attrib.st_size);
			/* Print: 		filename		ELN		Padding		no new line*/
			colors_list(dirlist[i]->d_name, i+1, largest+cont_bt, 0);
			printf("%s%s%s\n", NC, white_b, (size) ? size : "??");
			if (size)
				free(size);
		}
		return;
	}
	
	/* If "pr file file..." */
	for (size_t i=1;i<=args_n;i++) {
		if (access(comm[i], F_OK) == 0)
			get_properties(comm[i], 0, 0);
		else {
			struct stat file_attrib;
			lstat(comm[1], &file_attrib);
			if ((file_attrib.st_mode & S_IFMT) == S_IFLNK) {
				char linkname[PATH_MAX]="";
				ssize_t ret=readlink(comm[1], linkname, PATH_MAX);
				if (ret)
					fprintf(stderr, "%s: pr: '%s -> %s': %s\n", PROGRAM_NAME, 
							comm[1], linkname, strerror(errno));
				else
					fprintf(stderr, "%s: pr: '%s': %s\n", PROGRAM_NAME, 
							comm[1], strerror(errno));
			}
			else
				fprintf(stderr, "%s: pr: %s: %s\n", PROGRAM_NAME, comm[i], 
						strerror(errno));
			continue;
		}
	}
}

void
get_properties(char *filename, int _long, int max)
/* This is my humble version of 'ls -hl' command plus most of the info from 
 * 'stat' command */
{
	struct statx file_attrib;
	/* Check file existence (though this isn't necessary in this program) */
	/* statx(), introduced since kernel 4.11, is the improved version of 
	 * stat(): it's able to get creation time, unlike stat() */
	if (statx(AT_FDCWD, filename, AT_SYMLINK_NOFOLLOW, STATX_ALL, 
			  &file_attrib) == -1) {
		fprintf(stderr, "%s: pr: '%s': %s\n", PROGRAM_NAME, filename, 
				strerror(errno));
		return;
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
			if (!(file_attrib.stx_mode & S_IRUSR)) strcpy(color, d_red);
			else if (file_attrib.stx_mode & S_ISUID)
				strcpy(color, bg_red_fg_white);
			else if (file_attrib.stx_mode & S_ISGID)
				strcpy(color, bg_yellow_fg_black);
			else {
				cap_t cap=cap_get_file(filename);
				if (cap) {
					strcpy(color, bg_red_fg_black);
					cap_free(cap);
				}
				else if (file_attrib.stx_mode & S_IXUSR) {
					if (file_attrib.stx_size == 0) strcpy(color, d_green);
					else strcpy(color, green);
				}
				else if (file_attrib.stx_size == 0) strcpy(color, d_yellow);
				else strcpy(color, white_b);
			}
		break;
		case S_IFDIR:
			file_type='d';
			if (access(filename, R_OK|X_OK) != 0)
				strcpy(color, red);
			else {
				int is_oth_w=0;
				if (file_attrib.stx_mode & S_ISVTX) sticky=1;
				if (file_attrib.stx_mode & S_IWOTH) is_oth_w=1;
				int files_dir=count_dir(filename);
				strcpy(color, (sticky) ? ((is_oth_w) ? bg_green_fg_blue : 
					bg_blue_fg_white) : ((is_oth_w) ? bg_green_fg_black : 
					((files_dir == 2 || files_dir == 0) ? d_blue : blue)));
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
		default: file_type='?'; strcpy(color, white_b);
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
			trim_filename[max+cont_bt]='\0';
		}
		printf("%s%-*s %s%s (%04o) %c/%c%c%c/%c%c%c/%c%c%c %s %s %s %s\n", 
				color, max+cont_bt, (!trim) ? filename : trim_filename, NC, 
				white_b, file_attrib.stx_mode & 07777,
				file_type,
				read_usr, write_usr, exec_usr, 
				read_grp, write_grp, exec_grp,
				read_others, write_others, (sticky) ? 't' : exec_others,
				(!owner) ? _("unknown") : owner->pw_name, 
				(!group) ? _("unknown") : group->gr_name,
				(size_type) ? size_type : "??", 
				(mod_time[0] != '\0') ? mod_time : "??");
		if (linkname)
			free(linkname);
		if (size_type) 
			free(size_type);
		return;
	}

	/* For some reason this block of code changes the value of the variable 
	 * user(????). Very dirty workaround: run get_user() at the end of 
	 * the function (It doesn't happen anymore, I think) */

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
							(mod_time[0] != '\0') ? mod_time : "??");
	if (file_type != 'l')
		printf("%s%s%s%s\n", color, filename, NC, white_b);
	else {
		printf("%s%s%s%s -> %s\n", color, filename, NC, white_b,
			   (linkname) ? linkname : "??");
		if (linkname)
			free(linkname);
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
/*	free(user);
	user=get_user(); */
}

void
hidden_function(char **comm)
{
	if (!comm[1] || strcmp(comm[1], "--help") == 0) {
		puts(_("Usage: hidden [on, off, status]")); 
		return;
	}	
	if (strcmp (comm[1], "status") == 0)
		printf(_("%s: Hidden files %s\n"), PROGRAM_NAME, 
			   (show_hidden) ? _("enabled") : _("disabled"));
	else if (strcmp(comm[1], "off") == 0) {
		if (show_hidden == 1) {
			show_hidden=0;
			if (cd_lists_on_the_fly) {
				while (files--) free(dirlist[files]);
				list_dir();
			}
		}
	}
	else if (strcmp(comm[1], "on") == 0) {
		if (show_hidden == 0) {
			show_hidden=1;
			if (cd_lists_on_the_fly) {
				while (files--) free(dirlist[files]);
				list_dir();
			}
		}
	}
	else 
		fprintf(stderr, _("Usage: hidden [on, off, status]\n"));
}

void
log_function(char **comm)
/* Log 'comm' into LOG_FILE */
{
	int clean_log=0;
	/* If the command was just 'log' */
	if (strcmp(comm[0], "log") == 0 && args_n == 0) {
		FILE *log_fp;
		log_fp=fopen(LOG_FILE, "r");
		if (log_fp == NULL) {
			asprintf(&msg, "%s: log: '%s': %s\n", PROGRAM_NAME, 
					 LOG_FILE, strerror(errno));
			if (msg) {
				log_msg(msg, NOPRINT_PROMPT);
				free(msg);
			}
			else
				fprintf(stderr, "%s: log: '%s': %s\n", PROGRAM_NAME, 
						LOG_FILE, strerror(errno));
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
	
	/* If 'log clean' */
	else if (strcmp(comm[0], "log") == 0 && args_n == 1) {
		if (strcmp(comm[1], "clean") == 0) {
			clean_log=1;
		}
	}

	/* Construct the log */
	/* Create a buffer big enough to hold the entire command */
	size_t com_len=0;
	int i=0;
	for (i=args_n;i--;)
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
	/* If not 'log clean', append the log to the existing logs */
	if (!clean_log)
		log_fp=fopen(LOG_FILE, "a");
	/* Else, overwrite the log file leaving only the 'log clean' command */
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
	}
	else { /* If LOG_FILE was correctly opened, write the log */
		fprintf(log_fp, "%s", full_log);
		fclose(log_fp);
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
		char line[MAX_LINE]="";
		int logs_num=0;
		while (fgets(line, sizeof(line), log_fp))
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
			while (fgets(line, sizeof(line), log_fp)) {
				if (i++ >= logs_num-(max_log-1)) { /* Delete old entries */
					line[strlen(line)-1]='\0';
					fprintf(log_fp_tmp, "%s", line);
					fprintf(log_fp_tmp, "\n");
				}
			}
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

void
get_history(void)
{
	FILE *hist_fp=fopen(HIST_FILE, "r");
	if (current_hist_n == 0) /* Coming from main() */
		history=xcalloc(history, 1, sizeof(char **));
	else { /* Only true when comming from 'history clean' */
		for (size_t i=0;i<current_hist_n;i++)
			free(history[i]);
		history=xrealloc(history, 1*sizeof(char **));
		current_hist_n=0;
	}
	if (hist_fp != NULL) {
		char line[MAX_LINE]="";
		size_t line_len=0;
		while (fgets(line, sizeof(line), hist_fp)) {
			line_len=strlen(line);
			line[line_len-1]='\0';
			history=xrealloc(history, (current_hist_n+1)*sizeof(char **));
			history[current_hist_n]=xcalloc(history[current_hist_n], 
											line_len, sizeof(char *));
			strncpy(history[current_hist_n++], line, line_len);
		}
		fclose(hist_fp);
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
	}
}

void
history_function(char **comm)
{
	/* If no arguments, print the history list */
	if (args_n == 0) {
		for (int i=0;i<current_hist_n;i++)
			printf("%d %s\n", i+1, history[i]);
	}

	/* If 'history clean', guess what, clean the history list! */
	else if (args_n == 1 && strcmp(comm[1], "clean") == 0) {
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
			return;
		}
		/* Do not create an empty file */
		fprintf(hist_fp, "%s %s\n", comm[0], comm[1]);
		fclose(hist_fp);
		/* Update the history array */
		get_history();
		log_function(comm);
	}

	/* If 'history -n', print the last -n elements */
	else if (args_n == 1 && comm[1][0] == '-' && is_number(comm[1]+1)) {
		int num=atoi(comm[1]+1);
		if (num < 0 || num > current_hist_n) {
			num=current_hist_n;
		}
		for (int i=current_hist_n-num;i<current_hist_n;i++)
			printf("%d %s\n", i+1, history[i]);
	}
	
	else
		puts(_("Usage: history [clean] [-n]"));
}

void
run_history_cmd(const char *cmd)
/* Takes as argument the history cmd less the first exclamation mark. 
 * Example: if exec_cmd() gets "!-10" it pass to this function "-10", that 
 * is, comm+1 */
{
	/* If !n */
	if (is_number(cmd)) {
		int num=atoi(cmd);
		if (num > 0 && num < current_hist_n) {
			int old_args=args_n;
			char **cmd_hist=parse_input_str(history[num-1]);
			exec_cmd(cmd_hist);
			for (int i=0;i<=args_n;i++)
				free(cmd_hist[i]);
			free(cmd_hist);
			args_n=old_args;
		}
		else
			fprintf(stderr, _("%s: !%d: event not found\n"), 
					PROGRAM_NAME, num);
	}

	/* If "!!", execute the last command */
	else if (strcmp(cmd, "!") == 0) {
		int old_args=args_n;
		char **cmd_hist=parse_input_str(history[current_hist_n-1]);
		exec_cmd(cmd_hist);
		for (int i=0;i<=args_n;i++)
			free(cmd_hist[i]);
		free(cmd_hist);
		args_n=old_args;
	}

	/* If !-n */
	else if (cmd[0] == '-') {
		/* If not number or zero or bigger than max... */
		if (!is_number(cmd+1) || (atoi(cmd+1) == 0 
				|| atoi(cmd+1) > current_hist_n-1)) {
			fprintf(stderr, _("%s: !%s: event not found\n"), 
					PROGRAM_NAME, cmd);
			return;
		}
		int old_args=args_n;		
		char **cmd_hist=parse_input_str(
			history[current_hist_n-atoi(cmd+1)-1]);
		exec_cmd(cmd_hist);
		for (int i=0;i<=args_n;i++)
			free(cmd_hist[i]);
		free(cmd_hist);
		args_n=old_args;
	}
	else {
		printf(_("Usage:\n\
!!: Execute the last command.\n\
!n: Execute the command number 'n' in the history list.\n\
!-n: Execute the last-n command in the history list.\n"));
	}
}

void
edit_function(char **comm)
/* Edit the config file, either via xdg-open or via the first passed argument,
 * Ex: 'edit nano' */
{
	/* Get modification time of the config file before opening it */
	struct stat file_attrib;
	/* If, for some reason (like someone erasing the file while the program is 
	 * running) clifmrc doesn't exist, call init_config() to recreate the 
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
			return;
		}
	}
	/* If no application has been passed as 2nd argument, and if xdg-open 
	 * is not installed... */
	else if (!(flags & XDG_OPEN_OK)) {
		fprintf(stderr, 
				_("%s: 'xdg-open' not found. Try 'edit application_name'\n"), 
				PROGRAM_NAME);
		return;
	}

	/* Either a valid program has been passed or xdg-open exists */
	pid_t pid_edit=fork();
	if (pid_edit < 0) {
		fprintf(stderr, "%s: fork: %s\n", PROGRAM_NAME, strerror(errno));
		if (cmd_path) 
			free(cmd_path);
		return;
	}
	else if (pid_edit == 0) {
		set_signals_to_default();
		/* If application has been passed */
		if (args_n > 0)
			execle(cmd_path, comm[1], CONFIG_FILE, NULL, __environ);		
		/* No application passed and xdg-open exists */
		else
			execle(xdg_open_path, "xdg-open", CONFIG_FILE, NULL, __environ);
		set_signals_to_ignore();
	}
	else {
		run_in_foreground(pid_edit);
/*		int status=run_in_foreground(pid_edit);
		if (status != 0)
			fprintf(stderr, "%s: edit: program returned error code %d\n", 
					PROGRAM_NAME, status); */
	}
	
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
		init_config();
		/* Free the aliases and prompt_cmds arrays to be allocated again */
		for (size_t i=0;i<aliases_n;i++) free(aliases[i]);
		for (size_t i=0;i<prompt_cmds_n;i++) free(prompt_cmds[i]);
		aliases_n=prompt_cmds_n=0; /* Reset counters */
		get_aliases_n_prompt_cmds();
		/* Rerun external_arguments. Otherwise, external arguments will be 
		overwritten by init_config(). */
		if (argc_bk > 1) external_arguments(argc_bk, argv_bk);
		if (cd_lists_on_the_fly) {
			while (files--) free(dirlist[files]);
			list_dir();
		}
	}
}

void
color_codes(void)
/* List color codes for file types used by the program */
{
	printf(_("%s file name%s%s: Directory with no read permission\n"), red, NC, 
			white_b);
	printf(_("%s file name%s%s: File with no read permission\n"), d_red, NC, 
			white_b);
	printf(_("%s file name%s%s: Directory*\n"), blue, NC, white_b);
	printf(_("%s file name%s%s: EMPTY directory\n"), d_blue, NC, white_b);
	printf(_("%s file name%s%s: Executable file\n"), green, NC, white_b);
	printf(_("%s file name%s%s: Empty executable file\n"), d_green, NC, white_b);
	printf(_("%s file name%s%s: Block special file\n"), yellow, NC, white_b);	
	printf(_("%s file name%s%s: Empty (zero-lenght) file\n"), d_yellow, NC, 
			white_b);
	printf(_("%s file name%s%s: Symbolic link\n"), cyan, NC, white_b);	
	printf(_("%s file name%s%s: Broken symbolic link\n"), d_cyan, NC, white_b);
	printf(_("%s file name%s%s: Socket file\n"), magenta, NC, white_b);
	printf(_("%s file name%s%s: Pipe or FIFO special file\n"), d_magenta, NC, 
		white_b);
	printf(_("%s file name%s%s: Character special file\n"), white, NC, white_b);
	printf(_("%s file name%s%s: Regular file\n"), white_b, NC, white_b);
	printf(_(" %s%sfile name%s%s: SUID file\n"), NC, bg_red_fg_white, NC, 
			white_b);
	printf(_(" %s%sfile name%s%s: SGID file\n"), NC, bg_yellow_fg_black, NC, 
			white_b);
	printf(_(" %s%sfile name%s%s: Sticky and NOT other-writable directory*\n"), 
			NC, bg_blue_fg_white, NC, white_b);
	printf(_(" %s%sfile name%s%s: Sticky and other-writable directory*\n"), NC, 
			bg_green_fg_blue, NC, white_b);
	printf(_(" %s%sfile name%s%s: Other-writable and NOT sticky directory*\n"), 
			NC, bg_green_fg_black, NC, white_b);
	printf(_(" %s%sfile name%s%s: Unknown file type\n"), NC, bg_white_fg_red, 
			NC, white_b);
	printf(_("\n*The slash followed by a number (/xx) after directory names \
indicates the amount of files contained by the corresponding directory.\n\n"));
}

void
list_commands(void)
{
	printf(_("\n%scmd, commands%s%s: Show this list of commands.\n"), white, 
		   NC, white_b);
	printf(_("\n%s/%s%s* [dir]: This is the quick search function. Just type '/' \
followed by the string you are looking for (you can use wildcards), and %s \
will list all matches in the current working directory. To search for files \
in any other directory, specify the directory name as second argument. This \
argument (dir) could be an absolute path, a relative path, or an ELN.\n"), 
		   white, NC, white_b, PROGRAM_NAME);
	printf(_("\n%sbm, bookmarks%s%s: Open the bookmarks menu. Here you can add, \
remove or edit your bookmarks to your liking, or simply change the current \
directory to that specified by the corresponding bookmark by just typing \
either its ELN or its hotkey.\n"), white, NC, white_b);
	printf(_("\n%so, open%s%s ELN/dir/filename [application name]: Open \
either a directory or a file. For example: 'o 12' or 'o filename'. By default, \
the 'open' function will open files with the default application associated to \
them (if xdg-open command is found). However, if you want to open a file with \
a different application, just add the application name as second argument, \
e.g. 'o 12 leafpad'. If you want to run the program in the background, simply \
add the ampersand character (&): 'o 12 &'. When it comes to directories, \
'open' works just like the 'cd' command.\n"), white, NC, white_b);
	printf(_("\n%scd%s%s [ELN/dir]: When used with no argument, it changes the \
current directory to the default directory (HOME). Otherwise, 'cd' changes \
the current directory to the one specified by the first argument. You can use \
either ELN's or a string to indicate the directory you want. Ex: 'cd 12' or \
'cd ~/media'. Unlike the shell 'cd' command, %s's built-in 'cd' function doesn \
not only changes the current directory, but also lists its content \
(provided the option \"cd lists automatically\" is enabled, which is the \
default) according to a comprehensive list of color codes. By default, the \
output of 'cd' is much like this shell command: cd DIR && ls -A --color=auto \
--group-directories-first\n"), white, NC, white_b, PROGRAM_NAME);
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
history list by typing 'b clear'.\n"), white, NC, white_b, PROGRAM_NAME);
	printf(_("\n%sf, forth%s%s [h, hist] [clear] [!ELN]: It works just like \
the back function, but it goes forward in the history record. Of course, you \
can use 'f hist', 'f h', 'fh', and 'f !ELN'\n"), white, NC, white_b);
	printf(_("\n%sc, l, m, md, r%s%s: short for 'cp', 'ln', 'mv', 'mkdir', and \
'rm' commands respectivelly.\n"), white, NC, white_b);
	printf(_("\n%sp, pr, prop%s%s ELN/filename(s) [a, all] [s, size]: Print \
file properties of FILENAME(s). Use 'all' to list properties of all files in \
the current working directory, and 'size' to list their corresponding \
sizes.\n"), white, NC, white_b);
	printf(_("\n%ss, sel%s%s ELN ELN-ELN filename path... n: Send one or \
multiple elements to the Selection Box. 'Sel' accepts individual elements, \
range of elements, say 1-6, filenames and paths, just as wildcards. Ex: sel \
1 4-10 file* filename /path/to/filename\n"), white, NC, white_b);
	printf(_("\n%ssb, selbox%s%s: Show the elements contained in the \
Selection Box.\n"), white, NC, white_b);
	printf(_("\n%sds, desel%s%s [a, all]: Deselect one or more selected \
elements. You can also deselect all selected elements by typing 'ds a' or \
'ds all'.\n"), white, 
		   NC, white_b);
	printf(_("\n%st, tr, trash%s%s  [ELN's, file(s)] [ls, list] [clean] \
[del, rm]: With no argument (or by passing the 'ls' option), it prints a list \
of currently trashed files. The 'clean' option removes all files from the \
trash can, while the 'del' option lists trashed files allowing you to remove \
one or more of them. The trash directory is $XDG_DATA_HOME/Trash, that is, \
'~/.local/share/Trash'. Since this trash system follows the freedesktop \
specification, it is able to handle files trashed by different Trash \
implementations.\n"), white, NC, white_b);
	printf(_("\n%su, undel, untrash%s%s [a, all]: Print a list of currently \
trashed files allowing you to choose one or more of these files to be \
undeleted, that is to say, restored to their original location. You can also \
undelete all trashed files at once using the 'a' or 'all' option.\n"), white, 
		   NC, white_b);
	printf(_("\n%smp, mountpoints%s%s: List available mountpoints and change \
the current working directory into the selected mountpoint.\n"), white, NC, 
		   white_b);
	printf(_("\n%sv, paste%s%s [sel] [destiny]: The 'paste sel' command will \
copy the currently selected elements, if any, into the current working \
directory. If you want to copy these elements into another directory, you \
only need to tell 'paste' where to copy these files. Ex: paste sel \
/path/to/directory\n"), 
		   white, NC, white_b);
	printf(_("\n%slog%s%s [clean]: With no arguments, it shows the log file. \
If clean is passed as argument, it will delete all the logs.\n"), white, NC, 
		   white_b);
	printf(_("\n%smsg, messages%s%s [clean]: With no arguments, prints the \
list of messages in the current session. The 'clean' option tells %s to \
empty the messages list.\n"), white, NC, white_b, PROGRAM_NAME);
	printf(_("\n%shistory%s%s [clean] [-n]: With no arguments, it shows the \
history list. If 'clean' is passed as argument, it will delete all the entries \
in the history file. Finally, '-n' tells the history command to list only the \
last 'n' commands in the history list.\n"), white, NC, white_b);
	printf(_("You can use the exclamation character (!) to perform some \
history commands:\n\
  !!: Execute the last command.\n\
  !n: Execute the command number 'n' in the history list.\n\
  !-n: Execute the last-n command in the history list.\n"));
	printf(_("\n%sedit%s%s: Edit the configuration file.\n"), white, NC, 
		   white_b);
	printf(_("\n%salias%s%s: Show aliases, if any. To write a new alias simpy \
type 'edit' to open the configuration file and add a line like this: \
alias alias_name='command_name args...'\n"), white, NC, white_b);
	printf(_("\n%ssplash%s%s: Show the splash screen.\n"), white, NC, white_b);
	printf(_("\n%spath, cwd%s%s: Print the current working directory.\n"), 
		   white, NC, white_b);
	printf(_("\n%srf, refresh%s%s: Refresh the screen.\n"), white, NC, 
		   white_b);
	printf(_("\n%scolors%s%s: Show the color codes used in the elements \
list.\n"), white, NC, white_b);
	printf(_("\n%shf, hidden%s%s [on/off/status]: Toggle hidden files \
on/off.\n"), white, NC, white_b);
	printf(_("\n%sff, folders first%s%s [on/off/status]: Toggle list folders \
first on/off.\n"), white, NC, white_b);
	printf(_("\n%spg, pager%s%s [on/off/status]: Toggle the pager on/off.\n"), 
			white, NC, white_b);
	printf(_("\n%suc, unicode%s%s [on/off/status]: Toggle unicode on/off.\n"), 
			white, NC, white_b);
	printf(_("\n%sext%s%s [on/off/status]: Toggle external commands \
on/off.\n"), white, NC, white_b);
	printf(_("\n%sver, version%s%s: Show %s version details.\n"), white, NC, 
			white_b, PROGRAM_NAME);
	printf(_("\n%slicense%s%s: Display the license notice.\n"), white, NC, 
			white_b);
	printf(_("\n%sfs%s%s: Print an extract from 'What is Free Software?', \
written by Richard Stallman.\n"), white, NC, 
			white_b);
	printf(_("\n%sq, quit, exit, zz%s%s: Safely quit %s.\n"), white, NC, 
			white_b, PROGRAM_NAME);
	printf(_("%s  \nKeyboard shortcuts%s%s:\n\
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
%s  A-h/A-j%s%s:	Go to the previous directory in the directory history \
list\n\
%s  A-k%s%s:	Go to the next directory in the directory history list\n\
%s  F10%s%s:	Open the configuration file\n\
  Some (and even all) of these keybindings might not work in some terminals, \
though they do work fine in the Linux built-in console, xvt-like terminal \
emulators like Urxvt and Aterm, and xterm-like ones.\n\
  %s will create HOME/.Xresources, if it doesn't already exist, \
for these keybindings to work correctly.\n"), 
		white, NC, white_b, 
		white, NC, white_b,
		white, NC, white_b,
		white, NC, white_b,
		white, NC, white_b,
		white, NC, white_b,
		white, NC, white_b,
		white, NC, white_b,
		white, NC, white_b,
		white, NC, white_b,
		white, NC, white_b,
		white, NC, white_b,
		white, NC, white_b,
		white, NC, white_b,
		white, NC, white_b,
		white, NC, white_b,
		PROGRAM_NAME);
}

void
help_function (void)
{
	if (clear_screen) CLEAR;
	printf(_("%s%s %s%s %s(%s), by %s\n\n"), white, PROGRAM_NAME, VERSION, 
			NC, white_b, DATE, AUTHOR);
	printf(_("%s is a completely text-based, KISS file manager able to perform \
all the basic operations you may expect from any other FM. However, the most \
salient features of %s are: \
\n  a) The use of short commands and numbers (ELN's), instead of \
filenames. Type 'o 12', for instance, to open a file with your default text \
editor or to change to the desired directory. With the automatic ELN's \
expansion feature you can use ELN's with external commands as well. \
'diff 12 5', for example, will run 'diff' over the files corresponding to \
ELN's 12 and 5. Ranges are also accepted, for example: rm 1-12.\
\n  b) Bookmarks: bookmark your favorite directories, and even files, to get \
easy access to them. Example: type 'bm' (or Alt-b) to open the bookmarks \
screen and then simply type a number or a hotkey to access the desired \
bookmark.\
\n  c) The Selection Box allows you to select files and directories from \
different parts of your filesystem and then operate on them with just one \
command. \
\n  d) The back and forth functions keep track of all the paths visited by \
you, so that you can go back and forth to any of them by just typing 'b' or \
'f' (Alt-h, Alt-k).\
\n  e) Keyboard shortcuts make it even easier and faster to navigate and \
operate on your files. For example, instead of typing 'cd ..' to go back to \
the parent directory, or 'sel *' to select all files in the current directory, \
you can simply press Alt-u and Alt-a respectivelly.\
\n  f) The quick search function makes it really easy to quickly find the \
files you are looking for.\
\n  g) It is blazing fast and incredibly lightweigth. With a memory footprint \
below 5Mb, it can run on really old hardware.\
\n  h) It is so simple that it doesn't require an X session nor any \
graphical environment at all, being able thus to perfectly run on the linux \
built-in console and even on a headless machine via a SSH or a telnet \
session. \
\nAll of these features (described in more detail below) are aimed to make \
easier and faster to move through and to operate on your files.\n\
Because inspired in Arch Linux and its KISS principle, %s is fundamentally \
aimed to be lightweight, fast, and simple. On Arch's notion of simplcity \
see: https://wiki.archlinux.org/index.php/Arch_Linux#Simplicity\n"), 
	PROGRAM_NAME, PROGRAM_NAME, PROGRAM_NAME);
	printf(_("\n%s also offers the following features:\
\n  1) Trash system\
\n  2) History function\
\n  3) TAB completion\
\n  4) TAB ELN-expansion\
\n  5) Keyboard shortcuts\
\n  6) Wildcards expansion\
\n  7) Braces expansion\
\n  8) ELN's expansion\
\n  9) 'sel' keyword expansion\
\n  10) Ranges expansion\
\n  11) Quoted strings\
\n  12) Aliases\n"), PROGRAM_NAME);
	printf(_("\nUsage: %s [-aAfFgGhiIlLoOsuUvx] [-p path]\n\
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
\n -p, --path /starting/path\t use /starting/path as %s starting path\
\n -s, --splash \t\t\t enable the splash screen\
\n -u, --no-unicode \t\t disable unicode\
\n -U, --unicode \t\t\t enable unicode to correctly list filenames containing \
accents, tildes, umlauts, non-latin letters, etc. This option is enabled by \
default for non-english locales\
\n -v, --version\t\t\t show version details and exit\
\n -x, --ext-cmds\t\t\t allow the use of external commands\n"), PNL, 
		PROGRAM_NAME);
	printf(_("\nCommand line arguments take precedence over both the \
configuration file and the default options, whereas the configuration file \
takes precedence only over default options.\n"));
	printf(_("\n%sColor codes:%s%s\n\n"), white, NC, white_b);
	color_codes();
	printf(_("%sCommands:\n%s%sNote: ELN = Element List Number. Example: In the \
line \"12 openbox\", 12 is the ELN corresponding to the 'openbox' file.\n%s"), 
	white, NC, white_b, NC);
	list_commands();
	printf(_("\n%sNotes%s%s:\n"), white, NC, white_b);
	printf(_("\nIt depends on the terminal emulator you use to correctly \
display non-ASCII characters and characters from the extended ASCII charset. \
If, for example, you create a file named \"nandu\" (the spanish word for \
'rhea'), it will be correctly displayed by the Linux console, Lxterminal, and Urxvt, but not thus \
by Xterm or Aterm.\n"));
	printf(_("\n%s is not limited to its own set of internal commands, like \
open, sel, trash, etc, but it can run any external command as well, provided \
external commands are allowed (use the -x option or the configuration file). \
By beginning the external command by a colon or a semicolon (':', ';') you \
tell %s not to parse the input string, but instead letting this task to the \
system shell, say bash. However, bear in mind that %s is not intended to be \
used as a shell, but as the file manager it is.\n"), PROGRAM_NAME, 
	PROGRAM_NAME, PROGRAM_NAME);
	printf(_("\nBesides the default TAB completion for paths, you can also \
expand ELN's using the TAB key.\n"));
	printf(_("\n%s will automatically expand the 'sel' keyword: 'sel' indeed \
amounts to 'file1 file2 file3 ...' In this way, you can use the 'sel' keyword \
with any command. If you want to set the executable bit on several files, for \
instance, simply select the files you want and then run this command: \
'chmod +x sel'. Or, if you want to copy or move several files into some \
directory: 'cp sel 12', or 'mv sel 12' (provided the ELN 12 corresponds to \
a directory), respectivelly. If the destiny directory is ommited, selected \
files are copied into the current working directory, that is to say, \
'mv sel' amounts to 'mv sel .'. To trash or remove selected files, simply \
run 'tr sel' or 'rm sel' respectivelly.\
The same goes for wildcards and braces: 'chmod +x *', for example, \
will set the excutable bit on all files in the current working directory, \
while 'chmod +x file{1,2,3}' will do it for file1, file2, and file3 \
respectivelly.\
\n\nELN's and ELN ranges will be also automatically expanded, provided the \
corresponding ELN's actually exist, that is to say, provided some filename is \
listed on the screen under those numbers. For example: 'diff 1 118' will only \
expand '1', but not '118', if there is no ELN 118. In the same way, the range \
1-118 will only be expanded provided there are 118 or more elements listed on \
the screen. If this feature somehow conflicts with the command you want to \
run, say, 'chmod 644 ...', because the current amount of files is equal or \
larger than 644 (in which case %s will expand that number), then you can \
simply run the command as external: ';chmod ...'\n\
\nOf course, combinations of all these features is also possbile. \
Example: 'cp sel file* 2 23-31 .' will copy all the selected files, plus all \
files whose name starts with \"file\", plus those files corresponding to the \
ELN's 2 and 23 through 31, into the current working directory.\n"), 
		   PROGRAM_NAME, PROGRAM_NAME);
	printf(_("\nWhen dealing with filenames containing spaces, you can use both \
single and double quotes (ex: \"this file\" or 'this file') plus escape \
sequences (ex: this\\ file)."));
	printf(_("\n\nBy default, %s starts in your home directory. However, you \
can always specify a different path by passing it as an argument. Ex: %s -p \
/home/user/misc. You can also permanently set up the starting path in the %s \
configuration file.\n"), PROGRAM_NAME, PNL, PROGRAM_NAME);
	printf(_("\n%sConfiguration file%s%s: ~/.config/%s/%src\n"), white, NC, 
		   white_b, PNL, PNL);
	printf(_("Here you can permanently set up %s options, add aliases and some \
prompt commands (which will be executed imediately before each new prompt \
line). Just recall that in order to use prompt commands you must allow the \
use of external commands. See the 'ext' command above.\n"), PROGRAM_NAME);
	printf(_("\n%sProfile file%s%s: ~/.config/%s/%s_profile\n"), white, NC, 
		   white_b, PNL, PNL);
	printf(_("In this file you can add those commands you want to be executed \
at startup. You can also permanently set here some custom variables, ex: \
'dir=\"/path/to/folder\"'. This variable may be used as a shorcut to that \
folder, for instance: 'cd $dir'. Custom variables could also be temporarily \
defined via the command prompt: Ex: user@hostname ~ $ var=\"This is a test\". \
Temporary variables will be removed at program exit.\n\n"));
	printf(_("%sLog file%s%s: ~/.config/%s/log.cfm\n"), white, NC, white_b, 
		   PNL);
	printf(_("The file contains a series of fields separated by a colon in \
the following way: 'date:user:cwd:cmd. All commands executed as \
external will be logged.\n\n"));
	printf(_("%sMessages log file%s%s: ~/.config/%s/messages.cfm\n"), white, 
		   NC, white_b, PNL);
	printf(_("A file containing a list of system messages, either errors, \
warnings, or simple notices. The messages log format is: [date] message.\n\n"));
	printf(_("A bold green asterisk at the left of the prompt indicates that \
there are elements in the Selection Box. In the same way, a yellow 'T' \
notices that there are currently files in the trash can. Finally, %s makes \
use of three kind of messages: errors (a red 'E' at the left of the prompt), \
warnings (a yellow 'W'), and simple notices (a green 'N').\n"), PROGRAM_NAME);

	if (flags & EXT_HELP) /* If called as external argument... */
		return;
	printf(_("\nPress any key to exit help..."));
	xgetchar();	puts("");
	if (cd_lists_on_the_fly) {
		while (files--) free(dirlist[files]);
		list_dir();
	}
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
bonus_function()
{
	printf("%sVamos %sBoca %sJuniors %sCarajo%s! %s*%s*%s*\n", blue, yellow, 
		   blue, yellow, blue, blue, yellow, blue);
}
