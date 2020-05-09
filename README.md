# CLiFM
> A simple CLI, KISS, and completely keyboard-driven file manager written in C

This new version (0.11.6-3) brings a lot (really, a lot) of bug fixes and some new features like: a trash system (following the freedesktop specification), keyboard shorcuts (very handy), a little messages system to keep track of important messages and errors, "sel" keyword expansion and ranges expansion (use the help function for more information). I also added support for translations (using `gettext`).

![alt_tag](https://github.com/leo-arch/clifm/blob/master/images/clifm.png)

CliFM is a completely text-based file manager able to perform all the basic operations you may expect from any other FM. Besides these common operations, such as copy, move, remove, etc, CLiFM most distinctive features are:

* Bookmarks: Let's suppose you constantly need to access `/media/data/misc/iso_images`. With CLiFM bookmarks function, to access this location could be as easy as this: `bm`, to call the bookmarks function, and then `1`, provided `/media/data/misc/iso_images` is your first bookmark.

![alt_tag](https://github.com/leo-arch/clifm/blob/master/images/bookmarks.png)

* Files selection: the ability to select (and deselect) files from here and there, even in different instances of the program, and then operate on them as you whish via the Selection Box. Example: `sel 1 4 56 33` will send the files corresponding to these element list numbers to the Selection Box. Then, by typing `sb` you can check the contents of the Selection Box. Let's suppose you want to copy a couple of files from you home directory to some distant path, say `/media/data/misc`. Instead of copying all these files individually, you just select the files and then tell the `paste` command where to paste them:
 
`sel 1 2 3 6` (or `sel 1-3 6`) and then `paste /media/data/misc`

![alt_tag](https://github.com/leo-arch/clifm/blob/master/images/sel_box.png)
 
 * Open files without the need to specify any program. Via `xdg-open`, if no program was specified, CLiFM will open the file with the deafult program associated to that kind of files. To open a file may be as simple as this: `o 12`, or `o 12 &` if you want it
running in the background.
* The use of list numbers instead of file names. So, instead of:

      $ mv file1 dir1

you can just type:

      $ mv 12 1 

* Quick search: type `/string` and CliFM will list of the files containing `string` in its name. This function also support wildcards. Example: `/*.png` will list all the .png files.

![alt_tag](https://github.com/leo-arch/clifm/blob/master/images/quick_search.png)

* Just as the `ls` command, CLiFM uses color codes to identify file types. However, and unlike `ls`, CLiFM is also able to distinguish between empty and non-empty files. Once in CliFM, type `colors` to see the entire list of color codes.

![alt_tag](https://github.com/leo-arch/clifm/blob/master/images/colors.png)

* It also displays the amount of files contained by listed directories.

![alt_tag](https://github.com/leo-arch/clifm/blob/master/images/dirs.png)

Because half file manager and half terminal emulator, CLiFM also offers the following features:

* Auto-completion
* History function
* The ability to run any command just as if you were using your preferred terminal emulator (which makes it a basic shell)
* Aliases
* Logs
* Prompt and profile commands

Finally, all CLiFM options could be handled directly by command line, by passing parameters to the program, or via plain
text configuration files, located in `~/.config/clifm/`.

Insofar as it is heavily inspired in Arch Linux and its KISS principle, CLiFM is fundamentally aimed to be lightweight, fast, and simple. Weighing approximately 5 MB (!), it is the most lightweight and fastest file manager out there. 
On Arch's notion of simplcity see: https://wiki.archlinux.org/index.php/Arch_Linux#Simplicity

## Dependencies:

`glibc`, `ncurses`, `libcap`, `readline`, `coreutils` (providing basic programs such as rm, cp, mkdir, etc). For Archers: All these deps are part of the `core` repo, and `glibc` is also part of the `base` metapackage. In Debian systems two packages must be installed before compilation: `libcap-dev` and `libreadline-dev`. Otional dependencies: `du` (to check directory sizes), `xdg-utils` and `which` (without `which`, `xdg-open` will fail to get the default application for files).

## Compiling and Running CliFM:

1. Use `gcc` to compile the program. Don't forget to link the readline and cap libraries: 

       $ gcc -O2 -march=native -fstack-protector-strong -s -lcap -lreadline -o clifm clifm.c

2. Run the binary file produced by `gcc`:

       $ ./clifm`

Of course, you can copy this binary to `/usr/bin` or `/usr/local/bin` and then run the program as follows:

       $ clifm

Just try it and run the `help` command to learn more about CliFM. Once in the CliFM prompt, which looks like any terminal prompt, type `help` or `?`:

      [user@hostname:S] ~/ $ help

## Translating CliFM

This last version includes a spanish translation. To use it, simply copy `translations/spanish/clifm.mo` to
 `/usr/share/locale/es/LC_MESSAGES/` in your local machine. New translations are welcome. You can find the .pot file in `translations/clifm.pot`.
 
I you find some bug, and you will, try to fix it. It basically works, howerver; I myself use it as my main, and indeed only, file manager; it couldn't be so bad, isn't it?
