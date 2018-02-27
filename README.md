# CLiFM
> A simple CLI file manager written in C

CliFM is a completely text-based file manager able to perform all the basic operations you may expect from any other FM. Because inspired in Arch Linux and its KISS principle, it is fundamentally aimed to be lightweight, fast, and simple. On Arch's notion of 
simplcity see: https://wiki.archlinux.org/index.php/Arch_Linux#Simplicity

## Running CliFM:

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
