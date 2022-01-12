# Coding suggestions for CliFM

**NOTE**: To keep a consintent style, run `clang-format` over all source files, including header files, using the `_clang-format` file (in `/src`) as the formatting model:

```sh
clang-format -i -style=file *.[hc]
```

This command will reformat all C source and header files (`*.[hc]`) in place (`-i`) using the `_clang-format` file as model (`-style=file`).

Once this is done, you might want to check and modify a few things from the resulting files, specially long `printf`'s and multi-line statements. Automatic formating in these cases is still not optimal.

## 1) Coding style

### C source

BSD compatibility: We try to keep CliFM working on BSD systems (at the very least). So, when calling a function make sure it exists on BSD systems, and, if possible, make sure it is POSIX. Check its syntax as well: GNU functions are not always identical to BSD ones.

Generally, try to stick as closely as possible to the `Linux kernel coding style`. See https://www.kernel.org/doc/html/v4.10/process/coding-style.html

Indentation: TABS (I use a width of 4, but you can use 8 if you like)

Naming convention: Linux kernel style

Comments: C style only. Ex:

```c
/* This is a single line comment */

/* This is a muti-line
* comment */
```

Non-function statement blocks: Ex:
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

Proper casting. For example, do not write:

```c
return NULL;
```

But,

```c
return (char *)NULL;
```

Prefer ASCII instead of Hex: Ex: `'\0'` instead of `0x00`

Spacing: Write easily readable code. Generally, use blank lines between code blocks (this, however, depends on the code written so far). Just make it readable (the code won't be better for being more tightly written)

Max line legnth: `80 characters/columns`. If an statement exceeds this number, split it into multiple lines as follows:

```c
if (condition)
	printk(KERN_WARNING "Warning this is a long printk with "
						"3 parameters a: %u b: %u "
						"c: %u \n", a, b, c);
```

Make sure blank/empty lines do not contains TABS or spaces. In the same way, remove ending TABS and spaces.

## 2) Code quality

A program should not only provide working features, but also well written, performant, and easily understandable code:

**1)** Use the proper tool (and in the proper way) for the job: be as simple as possible and do not waste resources (they are highly valuable). For example: `strcmp(3)` is a standard and quite useful function. However, it is a good idea to prevent calling this function (possibily hundreds of times in a loop) if not needed. Before calling `strcmp` compare the first byte of the strings to be compared. If they do not match, there is no need to call the function:

```c
if (*str == *str2 && strcmp(str, str2) == 0)
```

In the same way, and for the same reason, use the cheapest function. If you do not need formatting, prefer `write(3)` or `fputs(3)` over `printf(3)`: they're faster.

Use pointers whenever possible: this is one of the greatest advantages of C. For instance, if you need to get the basename of a directory, there is no need to copy the string in any way, neither manually nor via some other function. Just get a pointer to the last slash in the original string using `strrchr(3)`:

```c
char *ret = strrchr(str, '/');
if (ret && *(++ret))
	/* We have the directory basename */
```

Always perform bound checks. Either make sure the destination buffer is big enough to hold the source string or truncate the source string to fit your buffer via some of the `n` functions (`strncpy(3)`, `strncat(3)`, `snprintf(3)`, etc):

```c
char *buf = (char *)xnmalloc(strlen(src) + 1, sizeof(char));
strcpy(buf, src);
```

or (using a safe version of `strncpy(3)`)
```c
buf[PATH_MAX];
xstrsncpy(buf, src, PATH_MAX);
```

**Note**: Both `xstrsncpy` and `xnmalloc` are safe implementations of `strcpy(3)` and `malloc(3)` respectively and are provided by CliFM itself.

These are just a few examples. There are plenty of resources out there on how to write secure code.

**2)** Manual memory management is another of the greatest (dis)advantages of C. Use a tool like `valgrind` to make sure your code is not leaking memory. Free `malloc`'ed memory as soon as you don't need it any more. As a plus, compile with `clang` using the following flags to detect undefined behavior and integer overflow at run-time: `-fsanitize=integer -fsanitize=undefined`.

**3)** Static analysis tools are an invaluable resource: use them! **Always** check your code via tools like `cppcheck`, `scan-build`, `flawdiner`, or `splint`. Use them all if necessary. Once done, fix all warnings and errors (provided they are not false positives, of course). This said, we use three online platforms to check our code quality: [Codacy](https://app.codacy.com/gh/leo-arch/clifm/dashboard), [Codiga](https://app.codiga.io/project/30518/dashboard), and [LGTM](https://lgtm.com/projects/g/leo-arch/clifm).

When it comes to plugins, we mostly use `POSIX shell scripts`. In this case, always use `shellcheck` to check your plugins.

**4**) Submitted code must be compiled without any warning/error using the following compiler flags:

```sh
-Wall -Wextra -Werror -Wpedantic -Wshadow -Wformat=2 -Wformat-security -Wconversion -Wsign-conversion -fstack-protector-strong -fstack-clash-protection -fcf-protection -Wvla -std=c99
```

To make sure your structs are properly aligned, add `-Wpadded` to detect misalignments and correct them by adding some padding if necessary. 

**5**) If not obvious, comment what your code is trying to achieve: there is no good software without good documentation.

## 3) CliFM's general code structure

CliFM source code consists of multiple C source files, being `main.c` the starting point and `helpers.h` the main header file. In `main.c` you'll find:

**A)** Initialization stuff, like loading config files (see `config.c`), command line options (parsed by the `external_arguments()` function, in `init.c`), readline and keybindings initialization (see `readline.c` and `keybindings.c`), bookmarks, workspaces, history, and the like.

**B)** Once everything is correctly initialized, an infinite loop, structured as a basic shell, takes place:
1)  Take input

2)  Parse input

3)  Execute command
 And take more input...

**C)** The last step above (3) calls the `exec_cmd()` function (`in exec.c`) to find out what needs to be done based on the user's input. The structure of the `exec_cmd` function is a big if-else chain: if the command is internal, that is, one of CliFM's built-in commands, the corresponding function will be called and executed; if not, if it is rather an external command, it will be executed by the system shell (via `launch_execle()`, also in `exec.c`).

**D)** Listing

This is the basic structure of CliFM: generally speaking, it is just a shell. In between, however, lots of things happen. Leaving aside the above mentioned functions, the most important one is `listdir()`, defined in `listing.c`. Everything related to listing files happens here: reading files in the current directory (via **readdir**(3)), getting file information (via the dirent struct returned by **readdir**(3) itself and **stat**(3)), sorting files (via **qsort**(3)), and storing all these information in a global struct (`file_info`) for future access, for example, to get file properties of a given entry.

**E)** Whatever happens later, is just some function or operation invoked by the user and happening on top of the steps described above: opening a file or directory (via the `open_function()` and `cd_function()` functions, in `file_operations.c` and `navigation.c` respectivelly), opening a bookmark (`bookmarks.c`), operating on files (`file_operations.c`), switching to a different profile (`profiles.c`), trashing a file (`trash.c`), searching for a file (`search.c`), running a plugin (`actions.c`), and so on.

## 4) Hacking

* Default settings: `settings.h`

* Add a new command: `exec.c`

* Add a new prompt feature: `prompt.c`

* Modify/add keybindings: `keybindings.c`

* Icons: `icons.h` and `listing.c`. Consult the [customizing icons](https://github.com/leo-arch/clifm/wiki/Advanced#customizing-icons) section

* TAB completion: `readline.c`, `tabcomp.c`

* Interface: `listing.c`

* Directory jumper: `jump.c`

* Suggestions: `suggestions.c` and `keybinds.c` (see the `rl_accept_suggestion` function)

* Syntax highlighting: `highlight.c` (see also `readline.c` and `keybinds.c`)

* Autocommands: `autocmds.c`

* File names cleaner(`bleach`): `name_cleaner.c` and `cleaner_table.h`

* Improve my security: `sanitize.c`

## 5) Compilation

**Note**: For the list of dependencies, see the [installation page](https://github.com/leo-arch/clifm/wiki/Introduction#installation).

CliFM is compiled using `(g)cc` (`clang` and `tcc` work as well) as follows:

1)  _Linux_:
```sh
gcc -O3 -s -fstack-protector-strong -march=native -Wall -o clifm *.c -lreadline -lcap -lacl -lmagic
```

2)  _FreeBSD_:

```sh
gcc -O3 -s -fstack-protector-strong -march=native -Wall -o clifm *.c -lreadline -lintl -lmagic
```

3)  _NetBSD_:

```sh
gcc -O3 -s -fstack-protector-strong -march=native -Wall -o clifm *.c -I/usr/pkg/include -L/usr/pkg/lib -Wl,-R/usr/pkg/lib -lintl -lreadline -lmagic
```

4)  _OpenBSD_:

```sh
cc -O3 -s -fstack-protector-strong -march=native -Wall -o clifm *.c -I/usr/local/include -L/usr/local/lib -lereadline -lintl -lmagic
```

5)  _Haiku_:

```sh
gcc -o clifm *.c -lreadline -lintl -lmagic
```

**NOTE**: Since compiling in this way only produces a binary file, it is necessary to manually copy the remaining files. See the `install` block of the [Makefile](https://github.com/leo-arch/clifm/blob/master/Makefile).

**NOTE 2**: You can drop `-lmagic` if compiling with `_NOMAGIC`. In the same way, you can drop `-lintl` if compiling with `_NO_GETTEXT`. See below.

**NOTE 3**: If the binary size is an issue, it is recommended to use [upx(1)](https://linux.die.net/man/1/upx) to significantly reduce (50-70%) the size of the executable file (at the expense of some performance):

```sh
upx clifm
```

### Compiling features in/out

CliFM allows you to enable or disable some features at compile time. If for whatever reason you don't plan to use a certain feature, it is better to remove this feature from the resulting binary: you'll get a (bit) faster and smaller executable. To do this, pass one or more of the following options to the compiler using the `-D` parameter. For example, to get a POSIX compliant executable without icons support:
```sh
clang ... -D_BE_POSIX -D_NO_ICONS ...
```

| Option | Description |
| --- | --- |
| `_BE_POSIX` | Build a fully `POSIX.1-2008` compliant executable<sup>1</sup> |
| `_NERD` | Enable Nerdfont support for icons |
| `_TOURBIN_QSORT` | Use Alexey Tourbin faster [qsort implementation](https://github.com/svpv/qsort) instead of [qsort(3)](https://www.man7.org/linux/man-pages/man3/qsort.3.html) |
| `_NO_ARCHIVING` | Disable archiving support |
| `_NO_BLEACH` | Disable support for `Bleach`, the built-in file names cleaner |
| `_NO_GETTEXT` | Disable translations support (via `gettext`) |
| `_NO_HIGHLIGHT`| Disable syntax highlighting support |
| `_NO_ICONS` | Disable icons support |
| `_NO_LIRA` | Disable [Lira](https://github.com/leo-arch/clifm/wiki/Specifics#resource-opener) support. Implies `_NO_MAGIC` |
| `_NO_MAGIC` | Allow compilation without `libmagic` dependency<sup>2</sup> |
| `_NO_SUGGESTIONS` | Disable suggestions support |
| `_NO_TRASH` | Disable trash support |

<sup>1</sup> Only one features is lost:
1)  Files birth time: We get this information via [statx(2)](https://man7.org/linux/man-pages/man2/statx.2.html), which is Linux specific.

<sup>2</sup> Without `libmagic`, querying files MIME type implies grabing the output of the [file(1)](https://www.man7.org/linux/man-pages/man1/file.1.html) command, which of course is not as optimal as directly querying the `libmagic` database itself (we need to run the command, redirect its output to a file, open the file, read it, close it, and then delete it). Though perhaps unnoticiable, this is an important difference.

## 6) Plugins

CliFM plugins, that is, commands or set of commands executed by CliFM, could be any executable file, be it a shell script, a binary file (C, Python, Go, Rust, or whatever programming language you like). See the [plugins section](https://github.com/leo-arch/clifm/wiki/Advanced#plugins).
