# Coding suggestions for CliFM


## 1) Coding style

### C source

FreeBSD compatibility: We try to keep CliFM working on FreeBSD (at the very least). So, when calling a function make sure it exists in FreeBSD. Check its syntax as well: GNU functions are not always identical to BSD ones.

Generally, try to stick as closely as possible to the `Linux kernel coding style`. See https://www.kernel.org/doc/html/v4.10/process/coding-style.html

Indentation: TABS

Comments: C style only. Ex:

	/* This is a single line comment */

	/* This is a muti-line
	* comment */

Non-function statement blocks: Ex:

	if (condition) {
		...
	}

	else {
		...
	}

NOTE: Regarding the else clauses, I do not follow here the kernel style. I just prefer it this way.

Functions definition:

	return type
	function_name(argument, ... n)
	{
		...
	}

NOTE: Writing the function name and arguments in a separate line makes it more easily searchable

Assignements:

	x = y

Proper casting. For example, do not write:

	return NULL;

But,

	return (char *)NULL;

Prefer ASCII instead of Hex: Ex: `'\0'` instead of `0x00`

Spacing: Write easily readable code. Generally, use blank lines between code blocks (this, however, depends on the code written so far). Just make it readable (the code won't be better for being tightly written)

Max line legnth: `80 characters/columns`. If an statement exceeds this number, split it into multiple lines as follows:

	if (condition)
		printk(KERN_WARNING "Warning this is a long printk with "
							"3 parameters a: %u b: %u "
							"c: %u \n", a, b, c);

Make sure blank/empty lines do not contains TABS or spaces. In the same way, remove ending TABS and spaces.

### Plugins

We mostly use `POSIX shell scripts`. In this case, always use `shellcheck` to check your plugins.

## 2) General code structure

CliFM source code consists of multiple C source files, being main.c the starting point and helpers.h the main header file. In main.c you'll find:

**A)** Initialization stuff, like loading config files (see config.c), command line options (parsed by the external_arguments() function, in init.c), readline and keybindings initialization (see readline.c and keybindings.c), bookmarks, workspaces, history, and the like.

**B)** Once everything is correctly initialized, an infinite loop, structured as a basic shell, takes place:
 1) Take input
 2) Parse input
 3) Execute command
 And take more input...

**C)** The last step above (3) calls the exec_cmd() function (in exec.c) to find out what needs to be done based on the user's input. The structure of the exec_cmd function is a big if-else chain: if the command is internal, that is, one of CliFM's built-in commands, the corresponding function will be called and executed; if not, if it is rather an external command, it will be executed by the system shell (via launch_execle(), also in exec.c).

**D)** Listing
 This is the basic structure of CliFM: generally speaking, it is just a shell. In between, however, lots of things happen. Leaving aside the taforementioned functions, the most important one is listdir(), defined in listing.c. Everything related to listing files happens here: reading files in the current directory (via readdir(3)), getting file information (via the dirent struct returned by readdir(3) itself and stat(3)), sorting files (via qsort(3)), and storing all these information in a global struct (file_info) for future access, for example, to get file properties of a given entry.

**E)** Whatever happens later, is just some function or operation invoked by the user and happening on top of the steps described above: opening a file or directory (via the open_function() and cd_function() functions, in file_operations.c and navigation.c respectivelly), opening a bookmark (bookmarks.c), opearting on files (file_operations.c), switching to a different profile (profiles.c), trashing a file (trash.c), searching for a file (search.c), running a plugin (actions.c), and so on.

## 3) Hacking (work in progress)

Add a new command: exec.c

Add a new prompt feature: prompt.c

Modify/add keybindings: keybindings.c

Icons: icons.h and listing.c

TAB completion: readline.c

Interface: listing.c

Jumper: jump.c

## 4) Plugins

CliFM plugins, that is, commands or set of commands executed by CLiFM, could be any executable file, be it a shell script, a binary file (C, Python, Go, Rust, or whatever programming language you like).

