# Coding suggestions for **Clifm**

**NOTE**: To keep a consintent style, run `clang-format` over all source files, including header files, using the `_clang-format` file (in `/src`) as the formatting model:

```sh
clang-format -i -style=file *.[hc]
```

This command will reformat all C source and header files (`*.[hc]`) in place (`-i`) using the `_clang-format` file as model (`-style=file`).

Once this is done, you might want to check and modify a few things from the resulting files, specially long `printf`'s and multi-line statements. Automatic formating in these cases is still not optimal.

## 1) Coding style

### C source

Portability: Though mostly developed in Linux (and this always means better support), we try to keep **clifm** working on \*BSD, MacOS, Solaris, and Haiku. So, when calling a function make sure it exists on these platforms as well, and, if possible, make sure it is POSIX. Check its syntax as well: some functions might take different parameters on different platforms. For example, **getmntinfo**(3) does not exist in Linux, and, while it takes a `statfs` struct in FreeBsd, OpenBSD, and MacOS, it takes a `statvfs` struct instead in NetBSD.

Generally, try to stick as closely as possible to the [Linux kernel coding style](https://www.kernel.org/doc/html/v4.10/process/coding-style.html). Also, though we don't follow it strictly, we also like the [Suckless coding style](https://suckless.org/coding_style/).

Indentation: TABS (I use a width of 4, but you can use 8 if you like)

Naming convention: Linux kernel style

Comments: C style only. E.g.:

```c
/* This is a single line comment */

/* This is a muti-line
* comment */
```

Non-function statement blocks: E.g.:
```c
if (condition) {
	...
} else {
	...
}
```

Functions definition:

```c
return type
function_name(argument, ... n)
{
	...
}
```

**NOTE**: Writing the function name and arguments in a separate line makes it more easily searchable

Assignements and comparissons (spaces around equal sign):

```c
x = y
```

Proper casting. For example, if initializing a pointer to a string, do not write:

```c
char *str = NULL;
```

But,

```c
char *str = (char *)NULL;
```

Prefer ASCII instead of Hex: E.g.: `'\0'` instead of `0x00`

Spacing: Write easily readable code. Generally, use blank lines between code blocks (this, however, depends on the code written so far). Just make it readable (the code won't be better for being more tightly written)

Max line legnth: `80 characters/columns`. If an statement exceeds this number, split it into multiple lines as follows:

```c
if (condition)
	printf("Warning: this is a long printf with "
		"3 parameters a: %u b: %u "
		"c: %u \n", a, b, c);
```

Make sure blank/empty lines do not contains TABS or spaces. In the same way, remove ending TABS and spaces.

## 2) Code quality

A program should not only provide working features, but also well written, performant, and easily understandable code:

### 1. Performance and security

**a)** Use the proper tool (and in the proper way) for the job: be as simple as possible and do not waste resources (they are highly valuable). For example: `strcmp(3)` is a standard and quite useful function. However, it is a good idea to prevent calling this function (possibily hundreds of times in a loop) if not needed. Before calling `strcmp` compare the first byte of the strings to be compared. If they do not match, there is no need to call the function:

```c
if (*str == *str2 && strcmp(str + 1, str2 + 1) == 0)
```

**b)** In the same way, and for the same reason, use the cheapest function. If you do not need formatting, prefer `write(3)` or `fputs(3)` over `printf(3)`: they're faster.

**c)** Pass structs by address (`function(&my_struct)`) intead of by value (`function(my_struct)`): passing an address to a function is faster than duplicating the value of each struct member and then pass it to the function. Also, try not to pass more than 4 parameters to functions.

**d)** Use `static` and `const` as much as possible. For example, if a variable won't be modified after the initial assignment, make it constant, i.e. reead-only (`const int var = 12`). Likewise, if a function won't be invoked outside the current compilation unit (source file and its corresponding header file), declare it as `static`: `static int my_func()`. In the same line, limit the scope of your variables as much as possible. For example, do not write:

```c
int n = 0;
while (cond) {
    n = atoi(str);
    printf("%d\n", n);
}
```

but,

```c
while (cond) {
    const int n = atoi(str);
    printf("%d\n", n);
}
```

**e)** Use pointers whenever possible (they're fast): this is one of the greatest advantages of C. For instance, if you need to get the basename of a path, there is no need to copy the string in any way, neither manually nor via some other function. Just get a pointer to the last slash in the original string using `strrchr(3)`:

```c
char *ret = strrchr(path, '/');
if (ret && *(++ret))
	/* RET now points to the path basename */
```

**f)** **Always** perform bound checks:

```c
/* Dinamically allocate enough memory to hold the source string */
char *buf = xnmalloc(strlen(src) + 1, sizeof(char));
strcpy(buf, src);
```

or
```c
/* If the buffer has a fixed size, make sure it won't be overflowed by a longer source string */
buf[PATH_MAX + 1];
xstrsncpy(buf, src, sizeof(buf));
```

**Note**: Both `xstrsncpy` and `xnmalloc` are safe implementations of `strcpy(3)` and `malloc(3)` respectively and are provided by **clifm** itself (see `strings.c` and `mem.c` respectively).

**g)** Also, be careful not to under/overflow your data types. For example, the following code:

```c
int c = 0;
while (cond)
    c++;
```
might easily overflow `c` _if `cond` allows `c` going beyond **INT_MAX**_ (in which case `c` becomes negative). A safer way of writing this is:

```c
int c = 0;
while (cond && c < INT_MAX)
    c++;
```

**h)** **Always, always** check the return value of functions. This code, for instance:

```c
char *file_ext = strrchr(filename, '.');
size_t len = strlen(file_ext);
```
will quite probably crash your program (if there's no dot in `filename`).

Rewrite it as follows to avoid a NULL pointer dereference (and probably a crash).

```c
char *file_ext = strrchr(filename, '.');
size_t len = file_ext ? strlen(file_ext) : 0;
```

**i)** **Validate input**: when it comes to untrsuted input (user supplied parameters, config files, etc), be extra distrustful. Sooner or later the user will enter exactly what you never expected (and the program will misbehave, or directly crash). For example, do not do this:
```c
const char *param = get_param();
return atoi(param);
```
but this
```c
const char *param = get_param();
const int n = param ? atoi(param) : -1;
if (n >= PARAM_MIN && n <= PARAM_MAX) /* Let's say PARM_MIN = 1 and PARAM_MAX = 10 */
	return n;
else
	/* Handle error */
```


**j)** Keep your functions short (ideally, make them do one thing -and do it well). This helps to make more readable and deduggable code. For the same reason, split code into separate source files whenever possible.

**k)** Do not repeat yourself: reuse code.

These are just a few advices (most of which I learned the hard way). There are plenty of resources out there on how to write secure/performant code: consult them.

### 2. Portability

We do care about portability. If possible, avoid non-portable functions, or, if absolutely required, use it only on the supported platform and use some replacement for the remaining ones. For example, in the case of a glibc-only function:

```c
#if !defined(__GLIBC__)
    portable_func();
#else
   non_portable_func();
#endif /* !__GLIBC__ */
```

Also, make sure to write [POSIX.1-2008](https://pubs.opengroup.org/onlinepubs/9699919799.2008edition/basedefs/V1_chap01.html) compliant code.

### 3. Memory

Manual memory management is another of the greatest (dis)advantages of C. Use a tool like `valgrind` to make sure your code is not leaking memory. Free `malloc`'ed memory as soon as you don't need it any more. As a plus, compile with `clang` using the following flags to detect undefined behavior and integer overflow at run-time: `-fsanitize=integer -fsanitize=undefined`.

### 4. Static analysis

Static analysis tools are an invaluable resource: use them! **Always** check your code via tools like `cppcheck`, GCC's analyzer (`-fanalyzer`),<sup>1</sup> Clang's `scan-build`, `flawfinder`, or `splint`. Use them all if necessary. Once done, fix all warnings and errors (provided they are not false positives, of course). This said, we use three online platforms to check our code quality: [Codacy](https://app.codacy.com/gh/leo-arch/clifm/dashboard), [~~Codiga~~](https://app.codiga.io/project/30518/dashboard), and [CodeQL](https://github.com/leo-arch/clifm/actions/workflows/codeql-analysis.yml).

<sup>1</sup> On the state of the built-in static analyzer provided by GCC see https://www.reddit.com/r/C_Programming/comments/u1ylub/static_analysis_of_c_code_in_gcc_12/

When it comes to plugins, we mostly use `POSIX shell scripts`. In this case, always use `shellcheck` to check your plugins.

### 5. Compiling

Submitted code must be compiled without any warning/error using the following compiler flags:

```sh
-O2 -D_FORTIFY_SOURCE=2 -Wall -Wextra -Werror -pedantic -Wshadow -Wformat=2 -Wformat-security -Wconversion -Wsign-conversion -Wunused -Wnull-dereference -fstack-protector-strong -fstack-clash-protection -fcf-protection -Wvla -std=c99
```

To make sure your structs are properly aligned, add `-Wpadded` to detect misalignments and correct them by adding some padding if necessary. 

As a plus, make sure to fix all errors/warnings emitted by a security hardened compilation, like the one [recommended by Red Hat](https://developers.redhat.com/blog/2018/03/21/compiler-and-linker-flags-gcc):

```sh
gcc -O2 -flto=auto -ffat-lto-objects -fexceptions -g -grecord-gcc-switches -pipe -Wall -Werror=format-security -Wp,-D_FORTIFY_SOURCE=2 -Wp,-D_GLIBCXX_ASSERTIONS -fstack-protector-strong -mtune=generic -fasynchronous-unwind-tables -fstack-clash-protection -fcf-protection -Wl,-z,relro -Wl,--as-needed  -Wl,-z,now -Wl,--build-id=sha1
```

Finally, purge the code from unused headers. An execellent tool specifically desinged for this purpose is [include-what-you-use](https://github.com/include-what-you-use/include-what-you-use).

### 6. Comments

If not obvious, comment what your code is trying to achieve: there is no good software without good documentation.

## 3) General code structure

**Clifm**'s source code consists of multiple C source files, being `main.c` the starting point and `helpers.h` the main header file. In `main.c` you'll find:

**A)** Initialization stuff, like loading config files (see `config.c` and `init.c`), command line options (parsed by the `parse_cmdline_args()`, in `args.c`), readline and keybindings initialization (see `readline.c` and `keybindings.c`), bookmarks, workspaces, history, and the like.

**B)** Once everything is correctly initialized, an infinite loop (`run_main_loop()`, in `main.c`), structured as a basic shell, takes place:
1)  Take input

2)  Parse input

3)  Execute command
 And take more input...

**C)** The last step above (3) calls the `exec_cmd()` function (`in exec.c`) to find out what needs to be done based on the user's input. The structure of the `exec_cmd` function is a big if-else chain: if the command is internal, that is, one of **clifm**'s built-in commands, the corresponding function will be called and executed; if not, if it is rather an external command, it will be executed by the system shell (via `launch_execl()`, also in `exec.c`).<sup>1</sup>

<sup>1</sup> The shell used to launch external commands is taken from: 1) **CLIFM_SHELL** environment variable, 2) **SHELL** environment variable, or 3) the `password` database (via **getpwuid**(3)), in this order. Note that if running with `--secure-cmds`, `--secure-env`, or `--secure-env-full`, steps 1 and 2 are skipped. 

**D)** Listing

This is the basic structure of **clifm**: generally speaking, it is just a shell. In between, however, lots of things happen. Leaving aside the above mentioned functions, the most important one is `listdir()`, defined in `listing.c`. Everything related to listing files happens here: reading files in the current directory (via **readdir**(3)), getting file information (via the dirent struct returned by **readdir**(3) itself and **stat**(2)), sorting files (via **qsort**(3)), and storing all this information into a global struct (`file_info`) for future access, for example, to get file properties of a given entry.

**E)** Whatever happens later, is just some function or operation invoked by the user and happening on top of the steps described above: opening a file or directory (via the `open_function()` and `cd_function()` functions, in `file_operations.c` and `navigation.c` respectivelly), opening a bookmark (`bookmarks.c`), operating on files (`file_operations.c`), switching to a different profile (`profiles.c`), trashing a file (`trash.c`), searching for a file (`search.c`), running a plugin (`actions.c`), and so on.

## 4) Hacking

| Target | File(s) | Main function | Observation |
| --- | --- | --- | --- |
| Initialization | `main.c` | `main` | See also `init.c` and `config.c` |
| Default settings | `settings.h` | | See also `messages.h` and the icons header files |
| Command line arguments | `args.c` | `parse_cmdline_args` | |
| Make our memory management better | `mem.c` | `xnmalloc` `xcalloc`, and `xnrealloc` | |
| Add a new command | `exec.c` | `exec_cmd` | Most of the time you want your command to be available for TAB completion and suggestions. See below. |
| External commands execution | `exec.c` | `launch_execv` and `launch_execl` | |
| Add a new prompt feature | `prompt.c` | `prompt` | |
| Tweak how we open files | `mime.c` | `mime_open` | |
| Tweak how we bookmark files | `bookmarks.c` | `bookmarks_function` | |
| Tweak how we trash files | `trash.c` | `trash_function` | |
| Tweak how we select files | `selection.c` | `sel_function` and `deselect` | |
| Tweak the quick search function | `search.c` | `search_glob` and `search_regex` | |
| Tweak how we handle profiles | `profiles.c` | `profile_function` |
| Tweak how we process file properties | `properties.c` | `properties_function` (`p/pp` command) | This file also handles the `pc`, `po`, and `stats` commands |
| Modify/add keybindings | `keybindings.c` | `readline_kbinds` |
| Icons | `icons.h`, `listing.c` | `list_dir` | Consult the [customizing icons](https://github.com/leo-arch/clifm/wiki/Advanced#customizing-icons) section |
| TAB completion (including alternative completers) | `readline.c` and `tabcomp.c` | `my_rl_completion` and `tab_complete` respectively | |
| Interface | `listing.c`, `long_view.c` and `colors.c` | `list_dir`, `print_entry_props` and `set_colors` respectively | See also `sort.c` for our files sorting algorithms |
| Directory jumper | `jump.c` | `dirjump` | |
| Suggestions | `suggestions.c` and `keybinds.c` | `rl_suggestions` and `rl_accept_suggestion` respectively | |
| Syntax highlighting | `highlight.c` | `rl_highlight` | See also `readline.c` and `keybinds.c` |
| Autocommands | `autocmds.c` | `check_autocmds` | |
| File names cleaner(`bleach`) | `name_cleaner.c` and `cleaner_table.h` | `bleach_files` | |
| Improve my security | `sanitize.c` | `sanitize_cmd`, `sanitize_cmd_environ`, and `xsecure_env` | |
| The tags system | `tags.c` | `tags_function` | |
| `mounpoint` and `media` commands | `media.c` | `media_menu` | |
| `net` command | `remotes.c` | `remotes_function` | |
| Most file operation functions | `file_operations.c` | | |
| Navigation stuff | `navigation.c` | | |
| History and logs | `history.c` | | |
| Plugins | `actions.c` | `run_action` | |
| Miscellaneous/auxiliary functions | `aux.c`, `checks.c`,`misc.c`, and `strings.c` | | |

## 5) Compilation

**Note**: For the list of dependencies, see the [installation page](https://github.com/leo-arch/clifm/wiki/Introduction#installation).

**CliFM** is compiled using `gcc` (or `clang`) as follows:

1)  _Linux_:
```sh
gcc -O3 -s -fstack-protector-strong -march=native -Wall -o clifm *.c -lreadline -lcap -lacl -lmagic
```

2)  _FreeBSD_ / _DragonFly_:

```sh
gcc -I/usr/local/include -L/usr/local/lib -O3 -s -fstack-protector-strong -march=native -Wall -o clifm *.c -lreadline -lintl -lmagic
```

3)  _NetBSD_:

```sh
gcc -I/usr/pkg/include -L/usr/pkg/lib -Wl,-R/usr/pkg/lib -O3 -s -fstack-protector-strong -march=native -Wall -o clifm *.c -lintl -lreadline -lmagic -lutil
```

4)  _OpenBSD_:

```sh
cc -I/usr/local/include -L/usr/local/lib -O3 -s -fstack-protector-strong -march=native -Wall -o clifm *.c -lereadline -lintl -lmagic
```

5)  _Haiku_:

```sh
gcc -o clifm *.c -lreadline -lintl -lmagic
```

6) _Solaris/Illumos_:

```sh
gcc -o clifm *.c -lreadline -ltermcap -lmagic -lnvpair
```

**NOTE**: Since compiling in this way only produces a binary file, it is necessary to manually copy the remaining files. See the `install` block of the [Makefile](https://github.com/leo-arch/clifm/blob/master/Makefile).

**NOTE 2**: You can drop `-lmagic` if compiling with `_NO_MAGIC`. In the same way, you can drop `-lintl` if compiling with `_NO_GETTEXT`. See below.

**NOTE 3**: If running on _Solaris/Illumos_ consult the [troubleshooting section](https://github.com/leo-arch/clifm/wiki/Troubleshooting#warning-prompt) in case of issues with the warning prompt. The `nvpair` library is used to get files creation time. You can drop `-lnvpair`, however, if compiling with `_NO_SUN_BIRTHTIME`.

**NOTE 4**: If the binary size is an issue, it is recommended to use [upx(1)](https://linux.die.net/man/1/upx) to significantly reduce (50-70%) the size of the executable file (at the expense of some performance):

```sh
upx clifm
```

You can also use **strip**(1) as follows:

```sh
strip --strip-unneeded --remove-section=.comment --remove-section=.note clifm
```

### Compiling features in/out

**Clifm** allows you to enable or disable some features at compile time. If for whatever reason you don't plan to use a certain feature, it is better to remove this feature from the resulting binary: you'll get a (bit) faster and smaller executable. To do this, pass one or more of the following options to the compiler using the `-D` parameter. For example, to get a binary without icons and translation support:

```sh
gcc ... -D_NO_GETTEXT -D_NO_ICONS ...
```

You can also use the `CPPFLAGS` variable. For example:

```sh
export CPPFLAGS="$CPPFLAGS -D_NO_GETTEXT -D_NO_ICONS"
make
```

If using GNU Make you can pass these options directly to **make**(1) via a GNU specific Makefile:
```sh
make -f misc/GNU/Makefile _NO_GETTEXT=1 _NO_ICONS=1
```

| Option | Description |
| --- | --- |
| `POSIX_STRICT` | Build a `POSIX.1-2008` compliant executable<sup>1</sup>. Note: combine with `CLIFM_LEGACY` for `POSIX.1-2001` compliance (experimental). |
| `CLIFM_SUCKLESS` | Remove all code aimed at parsing config files. Configuration is done either via `settings.h` (and recompilation) or via [environment variables](https://github.com/leo-arch/clifm/wiki/Specifics#environment)<sup>2</sup> |
| `_ICONS_IN_TERMINAL` | Use icons-in-terminal for [icons](https://github.com/leo-arch/clifm/wiki/Advanced/#icons-smirk) instead of the default (emoji-icons) |
| `_NERD` | Use Nerdfonts for [icons](https://github.com/leo-arch/clifm/wiki/Advanced/#icons-smirk) instead of the default (emoji-icons) |
| `_NO_ARCHIVING` | Disable [archiving](https://github.com/leo-arch/clifm/wiki/Advanced#archives) support |
| `_NO_ARC4RANDOM` | Disable support for [**arc4random**(3)](https://man.openbsd.org/arc4random.3) (**random**(3) will be used instead) |
| `_NO_BLEACH` | Disable support for [`Bleach`, the built-in file names cleaner](https://github.com/leo-arch/clifm/wiki/Introduction#bb-bleach-elnfile--n) |
| `_NO_GETTEXT` | Disable translations support (via `gettext`) |
| `_NO_FZF` | Disable support for [alternative TAB completers](https://github.com/leo-arch/clifm/wiki/Specifics#tab-completion) (fzf, fnf, and smenu) |
| `_NO_HIGHLIGHT`| Disable [syntax highlighting](https://github.com/leo-arch/clifm/wiki/Specifics#syntax-highlighting) support |
| `_NO_ICONS` | Disable [icons](https://github.com/leo-arch/clifm/wiki/Advanced/#icons-smirk) support |
| `_NO_LIRA` | Disable [Lira](https://github.com/leo-arch/clifm/wiki/Specifics#resource-opener) support. Implies `_NO_MAGIC` |
| `_NO_MAGIC` | Allow compilation without `libmagic` dependency<sup>3</sup> |
| `NO_MEDIA_FUNC` | Disable the [mountpoints](https://github.com/leo-arch/clifm/wiki/Introduction#mp-mountpoints) and [media](https://github.com/leo-arch/clifm/wiki/Introduction#media) commands. |
| `_NO_NETBSD_FFLAGS` | Disable support for BSD file flags (NetBSD only). You can drop the `-lutil` compilation flag. |
| `NO_PLEDGE` | Disable support for [**pledge**(2)](https://man.openbsd.org/pledge.2) (OpenBSD only) |
| `_NO_SUGGESTIONS` | Disable [suggestions](https://github.com/leo-arch/clifm/wiki/Specifics#auto-suggestions) support |
| `_NO_TAGS` | Disable support for [`Etiqueta`, the tags system](https://github.com/leo-arch/clifm/wiki/Common-Operations#tagging-files) |
| `_NO_PROFILES` | Disable [user profiles](https://github.com/leo-arch/clifm/wiki/Specifics#profiles) support |
| `_NO_SUN_BIRTHTIME` | Compile without files birthtime support (Solaris only). You can drop the `-lnvpair` compilation flag. Use only with the [Solaris-specific Makefile](https://github.com/leo-arch/clifm/blob/master/misc/solaris/Makefile), appending this flag to `CPPFLAGS`. | 
| `_NO_TRASH` | Disable [trash](https://github.com/leo-arch/clifm/wiki/Common-Operations#trashing-files) support |
| `_TOURBIN_QSORT` | Use Alexey Tourbin faster [qsort implementation](https://github.com/svpv/qsort) instead of [qsort(3)](https://www.man7.org/linux/man-pages/man3/qsort.3.html) |
| `ALLOW_COREDUMPS` | If running in [secure mode](https://github.com/leo-arch/clifm/wiki/Specifics#security), core dumps are disabled. Compile with this flag to allow them. |
| `SECURITY_PARANOID=1-3` | If compiled with this flag, **clifm** runs always in [secure mode](https://github.com/leo-arch/clifm/wiki/Specifics#security). If the value is `1`, the following flags are set: `--secure-cmds --secure-env`; if the value is `2`: `--secure-cmds --secure-env-full`; if the value is `3`: `--secure-cmds --secure-env-full --stealth-mode`. A value of `0` has no effect at all. |
| `USE_GENERIC_FS_MONITOR` | Use the generic file system events monitor instead of inotify (Linux) or kqueue (BSD) |
| `USE_DU1` | Use [**du**(1)](https://www.man7.org/linux/man-pages/man1/du.1.html) instead of our built-in trimmed down implementation ([xdu](https://github.com/leo-arch/clifm/blob/master/src/xdu.c)) |
| `VANILLA_READLINE` | Disable all **clifm** specific features added to readline: syntax highlighting, autosuggestions, TAB completion for **clifm** specific features/commands, and alternative TAB completion modes (fzf, fnf, and smenu) |

<sup>1</sup> By POSIX-compliance `strict POSIX-1.2008/XSI compliance` is understood: features/fuctions _not_ specified in the POSIX standard are avoided _as much as possible_ (mostly because we still rely heavily on **readline**(3), which is not POSIX). Features/functions disabled in POSIX mode: ACLs, files birth time, capabilities, attributes, and extended attributes, BSD file flags, Solaris doors, non-POSIX options for shell utilities (rm, cp, mv, etc), `inotify`/`kqueue` (replaced by a built-in file system events monitor), non-POSIX C functions (like **strcasestr**(3) and **memrchr**(3)) are replaced by built-in functions, and **arc4random**(3) is replaced by the older and less secure **random**(3). For more information about POSIX compliance see https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap02.html#tag_02_01_04.

<sup>2</sup> The [stealth mode](https://github.com/leo-arch/clifm/wiki/Specifics#stealth-mode) achieves basically the same functionality: disabling access to config files. However, there is an important difference: if compiled with `CLIFM_SUCKLESS`, functions handling configuration files are directly removed from the source code, resulting in a smaller binary.

<sup>3</sup> Without `libmagic`, querying files MIME type implies grabbing the output of the [file(1)](https://www.man7.org/linux/man-pages/man1/file.1.html) command, which of course is not as optimal as directly querying the `libmagic` database itself (we need to run the command, redirect its output to a file, open the file, read it, close it, and then delete it). Though perhaps unnoticiable, this is an important difference.

## 6) Plugins

**Clifm**'s plugins, that is, commands or set of commands executed by **Clifm**, could be any executable file, be it a shell script, a binary file (C, Python, Go, Rust, or whatever programming language you like). See the [plugins section](https://github.com/leo-arch/clifm/wiki/Advanced#plugins).
