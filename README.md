# CLiFM
> The KISS file manager: text-based, ultra-lightweight, lightning fast, and written in C

This new version (0.16.2) brings a lot (really, a lot) of bug fixes and some new features like: a trash system (freedesktop compliant), keyboard shorcuts (very handy), a little messages system to keep track of important messages and errors, "sel" keyword auto-expansion and ranges auto-expansion, ELN expansion with the TAB key, including a Bash-like quoting system (absolutely great), colors customization, and commands sequential and conditional execution (very useful). Consult the manpage for more information). I also added support for translations (using `gettext`).

![alt_tag](https://github.com/leo-arch/clifm/blob/master/images/clifm.png)

CliFM is a completely text-based file manager able to perform all the basic operations you may expect from any other FM. Besides these common operations, such as copy, move, remove, etc, CLiFM most distinctive features are:

* The use of short (and even one-letter) commands, and list numbers (ELN's) for filenames. For example, instead of typing: `cp file1 file2 file3 file4 dir/`, you can do this: `c 1-4 7`. Shorter and quicker. As a plus, ELN's can also be used with external commands. Example: `diff 1 5` or `ls -l 12 14`. If numbers are a bit tricky to you, as they are to me, you can use the TAB key to expand the ELN to the corresponding filename. So, type `r 56`, then TAB, and it becomes `r filename`.

* Bookmarks: Let's suppose you constantly need to access `/media/data/misc/iso_images`. With CLiFM bookmarks function, to access this location could be as easy as this: `bm` (or `Alt-b`), to call the bookmarks function, and then `1`, provided `/media/data/misc/iso_images` is your first bookmark.

![alt_tag](https://github.com/leo-arch/clifm/blob/master/images/bookmarks.png)

* Files selection: the ability to select (and deselect) files from here and there, even in different instances of the program, and then operate on them as you whish via the Selection Box or the `sel` keyword. Example: `s 1 4 56 33` will send the files corresponding to these element list numbers to the Selection Box. Then, by typing `sb` you can check the contents of the Selection Box. Let's suppose you want to copy a couple of files from your home directory to some distant path, say `/media/data/misc`. Instead of copying all these files individually, you just select the files and then tell the `paste` command where to copy them:
 
`s 1 2 3 6` (or `s 1-3 6`) and then `paste sel /media/data/misc`

You can also use the 'sel' keyword with external commands. Example: `s 1-4 7 10 && file sel`.

![alt_tag](https://github.com/leo-arch/clifm/blob/master/images/sel_box.png)
 
 * Open files without the need to specify any program. Via `xdg-open`, if no program was specified, CLiFM will open the file with the deafult program associated to that kind of files. To open a file may be as simple as this: `o 12`, or `o 12 &` if you want it running in the background.

* Quick search: type `/string` and CliFM will list all the files containing `string` in its name. This function also support wildcards. Example: `/*.png` will list all the .png files in the current directory. If you want to search for files in another directory, just tell the search function where to search: `/*.png /media/images`. Of course, you can deselect some or all selected files whenever you want with the `desel` or `ds` command.

![alt_tag](https://github.com/leo-arch/clifm/blob/master/images/quick_search.png)

* A `trash system` to be able to recover deleted files.

* Just like the `ls` command, CLiFM uses (customizable) color codes to identify file types. However, and unlike `ls`, CLiFM is also able to distinguish between empty and non-empty files. Once in CliFM, type `colors` to see the entire list of color codes.

![alt_tag](https://github.com/leo-arch/clifm/blob/master/images/colors.png)

* It also displays the amount of files contained by listed directories.

![alt_tag](https://github.com/leo-arch/clifm/blob/master/images/dirs.png)

Because file manager, but also half shell, CLiFM also offers the following features:

* TAB-completion for commands, paths, and ELN's
* Bash-like quoting system
* History function
* Shell commands execution 
* Aliases
* Logs
* Prompt and profile commands
* Sequential and conditional execution of commands 
* User profiles
* Keyboard shortcuts

Finally, all CLiFM options could be handled directly by command line, by passing parameters to the program, or via plain
text configuration files, located in `~/.config/clifm/`.

Insofar as it is heavily inspired in Arch Linux and its KISS principle, CLiFM is fundamentally aimed to be lightweight, fast, and simple. Weighing approximately 5 MB (!), it is the most lightweight and fastest file manager out there. 
On Arch's notion of simplcity see: https://wiki.archlinux.org/index.php/Arch_Linux#Simplicity

## Dependencies:

`glibc`, `ncurses`, `libcap`, `readline`, `coreutils` (providing basic programs such as rm, cp, mkdir, etc). For Archers: All these deps are part of the `core` repo, and `glibc` is also part of the `base` metapackage. In Debian systems two packages must be installed before compilation: `libcap-dev` and `libreadline-dev`. In Fedora based systems you need `libcap-devel` and `readline-devel`. Optional dependencies: `xdg-utils`.

## Compiling and Running CliFM:

If using Arch, just clone, build, and install the package using the PKGBUILD file:

       $ git clone https://github.com/leo-arch/clifm.git
       $ cd clifm
       $ makepkg --install

If using another distro:

1. Clone the repository

       $ git clone https://github.com/leo-arch/clifm.git
       $ cd clifm

2. Use `gcc` to compile the program. Don't forget to link the readline and cap libraries: 

       $ gcc -O3 -march=native -fstack-protector-strong -s -lcap -lreadline -o clifm clifm.c

3. Run the binary file produced by `gcc`:

       $ ./clifm`

Of course, you can copy this binary to `/usr/bin` or `/usr/local/bin`, or anywhere in your PATH, and then run the program as always:

       $ clifm

Just try it and run the `help` command to learn more about CliFM. Once in the CliFM prompt, which looks like any terminal prompt, type `help` or `?`:

      [user@hostname] ~/ $ help

## Translating CliFM

This last version includes a spanish translation. To use it, simply copy `translations/spanish/clifm.mo` to
 `/usr/share/locale/es/LC_MESSAGES/` in your local machine. New translations are welcome. You can find the .pot file in `translations/clifm.pot`.
 
I you find some bug, and you will, please let me know it. It gets better and better. I myself use it as my main, and indeed only, file manager; it couldn't be so bad, isn't it?
