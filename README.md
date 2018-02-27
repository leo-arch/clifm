# CLiFM
> A simple CLI, KISS file manager written in C

![alt_tag](https://github.com/leo-arch/clifm/blob/master/images/clifm.png)

CliFM is a completely text-based file manager able to perform all the basic operations you may expect from any other FM. Besides these common operations, such as copy, move, remove, etc, CLiFM most distinctive features are:

* Bookmarks: Let's suppose you constantly need to access `/media/data/misc/iso_images`. With CLiFM bookmarks function, to access this location could be as easy as this: `bm`, to call the bookmarks function, and then `1`, provided `/media/data/misc/iso_images` is your first bookmark. 

* Files selection: the ability to select file from here and there, even in different instances of the program, and then operate on them as you whish via the Selection Box. Example: `sel 1 4 56 33` will send the files corresponding to these element list numbers to the Selection Box. Then, by typing `sb` you can check the contents of the Selection Box. Let's suppose you want to copy a couple of files from you home directory to some distant path, say `/media/data/misc`. Instead of copying all these files
 individually, you just select the files and then tell the `paste` command where to paste them:
 
`sel 1 2 3 6` (or `sel 1-3 6`) and then `paste /media/data/misc`
 
 * Open files without the need to specify any program. Via `xdg-open`, if no program was specified, CLiFM will open the file with the deafult program associated to that kind of files. To open a file may be as simple as this: `o 12`, or `o 12 &` if you want it
running in the background.
* The use of list numbers instead of file names. So, instead of:

      $ mv file1 dir1

you can just type:

      $ mv 12 1 

* Quick search: type `/string` and CliFM will list of the files containing `string` in its name. This function also support wildcards. Example: `/*.png` will list all the .png files.

![alt_tag](https://github.com/leo-arch/clifm/blob/master/images/quick_search.png)

* Just as the `ls` command, CLiFM uses color codes to identify file types. However, and unlike `ls`, CLiFM is also able to distinguish between empty and non-empty files. Once in CliFM, type `colors` to see the entire list of color codes.

Because inspired in Arch Linux and its KISS principle, it is fundamentally aimed to be lightweight, fast, and simple. Weightng approximately 5 MB (!), it is the most lightweight and fastest file manager out there. 
On Arch's notion of simplcity see: https://wiki.archlinux.org/index.php/Arch_Linux#Simplicity

## Compiling and Running CliFM:

1. Use the `gcc` to compile the program. Don't forget to add the readline and cap libraries: 

       $ gcc -g -O2 -mtune=native -lcap -lreadline -o clifm clifm.c

2. Run the binary file produced by `gcc`:

       $ ./clifm`

Of course, you can copy this binary to `/usr/bin` or `/usr/local/bin` and then run the program as follows:

       $ clifm

Just try it and run the `help` command to learn more about CliFM. Once in the CliFM prompt, which looks as any terminal prompt, type `help` or `?`:

      [user@hostname:S] ~/ $ help

I you find some bug, and you will, try to fix it. It basically works, howerver; I myself use it as my main, and only, file manager;
I couldn't be so bad, isn't it?
