<p align="center">
	<a href="https://github.com/leo/clifm">
		<img src="images/logo/clifm.svg" alt="CliFM logo" width="160" height="160">
	</a>
</p>
<h1 align="center">CliFM</h1>
<h2 align="center">The KISS, non-curses terminal file manager</h2>
<h3 align="center">Written in C, fast, extensible, and lightweight as hell</h3>
<h4 align="center"><a
href="https://github.com/leo-arch/clifm/blob/master/.github/ISSUE_TEMPLATE/bug_report.md">Report bug</a> · <a
href="https://github.com/leo-arch/clifm/blob/master/.github/ISSUE_TEMPLATE/feature_request.md">Request feature</a> · <a
href="https://github.com/leo-arch/clifm/wiki">Explore documentation</a></h4>

<br />
<br />

<p align="center">
<a href="https://aur.archlinux.org/packages/clifm/"><img src="https://img.shields.io/aur/version/clifm?color=1793d1&label=clifm&logo=arch-linux&style=for-the-badge"/></a>
<a href="https://aur.archlinux.org/packages/clifm/"><img src="https://img.shields.io/aur/version/clifm-git?color=1793d1&label=clifm-git&logo=arch-linux&style=for-the-badge"/></a>
<a href="https://aur.archlinux.org/packages/clifm/"><img src="https://img.shields.io/aur/version/clifm-colors-git?color=1793d1&label=clifm-colors-git&logo=arch-linux&style=for-the-badge"/></a>
<a href="https://en.wikipedia.org/wiki/Privacy-invasive_software"><img src="https://img.shields.io/badge/privacy-ok-green?style=for-the-badge"/></a>
<a href="https://github.com/leo-arch/clifm/blob/master/LICENSE"><img src="https://img.shields.io/github/license/leo-arch/clifm?color=red&style=for-the-badge"/></a>
<a href="https://gitter.im/leo-arch/clifm"><img src="https://img.shields.io/gitter/room/leo-arch/clifm?style=for-the-badge"/></a>
<a><img src="https://img.shields.io/github/last-commit/leo-arch/clifm/master?color=blue&style=for-the-badge"/></a>
</p>

<h3 align="center"><br><i>Did I say it's fast?</i></h3>
<p align="center"><a href="https://mega.nz/embed/J8hEkCZZ#fGp0JtcDvFIWKmTc4cOp0iMrWRlbqs99THg8F7EmQWI"><img src="images/vid_thumb.png"></a></p>

Music: "Quad Machine", by [Sonic Mayhem](https://en.wikipedia.org/wiki/Sascha_Dikiciyan)

---

## Table of contents
* [Why?](#why)
* [Description](#description)
* [Installing CliFM](#installing-clifm)
* [First steps](#first-steps)
* [Support](#support)
* [License](#license)
* [Contributing](#contributing)

## Why?

Why in this world do we need another file manager? In the first place, just because I can do it, write it, and learn (a lot) in the process, just because this is a free world, and very specially, a free community; and, needless to say, alternatives are at the heart of freedom.

Secondly, because I'm sure I'm not the only person in this world looking for a simple [KISS](https://en.wikipedia.org/wiki/KISS_principle) file manager for the terminal: it just does whatever needs to be done in the simplest possible way. No GUI, no curses, but just a command line, shell-like file manager: 5MiB of RAM and 400KiB of disk space (plus some willingness to try something different and new) is all you need.

In the third place, because this world deserves a **non-curses terminal file manager**. Unlike most terminal file managers out there, CliFM replaces the traditional curses interface by a simple command line interface. In this sense, CliFM is certainly a file manager, but also **a shell extension**. Almost everything you do on your shell can be done in this file manager as well: search for files, copy, rename, and trash some of them, but, at the same time, update/upgrade your system, add some cronjob, stop a service, and run nano (or vi, or emacs, if you like).

Those really used to the commands line (the very secret of Unix's power) will find themselves quite comfortable with a purely command line file manager. And this for a very simple reason: the command line is still there. We do not hide the shell!

### Should all terminal file managers be curses-based file managers? CliFM is the answer to this question: No.

---

## Description

<h4 align="center">CliFM's interface</h4>
<p align="center"><img src="images/icons_rounded.png"></p>

CliFM is a completely command-line-based, shell-like file manager able to perform all the basic operations you may expect from any other file manager. Besides these common operations, such as copy, move, remove, etc, CliFM provides the following features:

* It is [really CLI-based](https://github.com/leo-arch/clifm/wiki/Introduction#main-design-and-goals). No GUI nor TUI or curses at all, just a command line. Since it does not need any GUI, it can run on the kernel built-in console and even on a SSH or any other remote session.
* With a memory footprint below 5 MiB and a disk usage of 0.5 MiB, it is incredibly lightweight and fast, and as such, able to run on really old hardware.
* The use of [short (and even one-character) commands](https://github.com/leo-arch/clifm/wiki/Introduction#commands-short-summary), and list numbers ([ELN](https://github.com/leo-arch/clifm/wiki/Common-Operations)'s) for filenames. 
* [Bookmarks](https://github.com/leo-arch/clifm/wiki/Common-Operations#bookmarks)
* [Files selection](https://github.com/leo-arch/clifm/wiki/Common-Operations#selection) (supports both glob and regular expressions and works even across multiple instances of the program)
* [_Lira_](https://github.com/leo-arch/clifm/wiki/Specifics#resource-opener), a built-in resource opener (supports regular expressions)
* [Files search](https://github.com/leo-arch/clifm/wiki/Common-Operations#searching) (supports both glob and regular expressions)
* A Freedesktop compliant trash system
* Extended [color codes](https://github.com/leo-arch/clifm/wiki/Customization#colors) for file types and file extensions
* [Files counter](https://github.com/leo-arch/clifm/wiki/Introduction#interface) for directories and symlinks to directories
* Directory history map to keep in sight previous, current, and next entries in the directory history list
* [Plugins](https://github.com/leo-arch/clifm/wiki/Advanced#plugins)
* [Files preview](https://github.com/leo-arch/clifm/wiki/Advanced#files-preview) (via _BFG_, a native file previewer, but including support for [Ranger's scope.sh](https://github.com/ranger/ranger/blob/master/ranger/data/scope.sh) and [pistol](https://github.com/doronbehar/pistol) as well)
* [Stealth mode](https://github.com/leo-arch/clifm/wiki/Advanced#stealth-mode): Leave no trace on the host system. No file is read, no file is written.
* [_Kangaroo_](https://github.com/leo-arch/clifm/wiki/Specifics#kangaroo-frecency-algorithm), a built-in directory jumper function similar to [autojump](https://github.com/wting/autojump), [z.lua](https://github.com/skywind3000/z.lua), and [zoxide](https://github.com/ajeetdsouza/zoxide).
* Batch links
* [Icons](https://github.com/leo-arch/clifm/wiki/Advanced#icons-smirk) support :smirk:
* Unicode suppport
* [TAB-completion](https://github.com/leo-arch/clifm/wiki/Specifics#expansions-and-completions)
* Bash-like quoting system
* History function
* Shell commands execution
* [Glob and regular expressions](https://github.com/leo-arch/clifm/wiki/Advanced#wildcards-and-regex) (including inverse matching)
* [Aliases](https://github.com/leo-arch/clifm/wiki/Customization#aliases)
* Logs
* [Prompt and profile commands](https://github.com/leo-arch/clifm/wiki/Customization#profile-and-prompt-commands) (run commands with each new prompt or at program startup)
* Bash-like [prompt customization](https://github.com/leo-arch/clifm/wiki/Customization#the-prompt)
* Sequential and conditional commands execution 
* [User profiles](https://github.com/leo-arch/clifm/wiki/Specifics#profiles)
* Customizable [keyboard shortcuts](https://github.com/leo-arch/clifm/wiki/Customization#keybindings)
* _Mas_, a built-in pager for files listing
* Eleven sorting methods
* [Bulk rename](https://github.com/leo-arch/clifm/wiki/Advanced#bulk-rename)
* [Archiving and compression](https://github.com/leo-arch/clifm/wiki/Advanced#archives) support (including Zstandard and ISO 9660)
* Auto-cd and auto-open
* Symlinks editor
* Disk usage
* [CD on quit](https://github.com/leo-arch/clifm/wiki/Advanced#cd-on-quit) and [file picker](https://github.com/leo-arch/clifm/wiki/Advanced#file-picker) functions
* Read and list files form [standard input](https://github.com/leo-arch/clifm/wiki/Advanced#standard-input)
* [Files filter](https://github.com/leo-arch/clifm/wiki/Advanced#file-filters)
* Up to eight [workspaces](https://github.com/leo-arch/clifm/wiki/Specifics#workspaces)
* Fused parameters for ELN's (`s1`, for example, works just as `s 1` )
* [Advcpmv](https://github.com/jarun/advcpmv) support (just `cp` and `mv` with a nice progress bar)
* Light mode (just in case it is not fast enough for you)
* [Color schemes](https://github.com/leo-arch/clifm/wiki/Customization#colors)
* **NEW**: Four [customizable keybindings for custom plugins](https://github.com/leo-arch/clifm/wiki/Customization#keybindings)
* **NEW**: Fastback function
* **NEW**: [Git integration](https://github.com/leo-arch/clifm/wiki/Advanced#git-integration)

For a detailed explanation of each of these features follow the corresponding links or consult the [wiki](https://github.com/leo-arch/clifm/wiki).

---

## Installing CliFM

### Dependencies

`glibc` and `coreutils`, of course, but also `libcap`, `acl`, `file`, and `readline`. For Archlinux users, all these dependenciess are part of the `core` reposiroty. In Debian systems two packages must be installed before compilation: `libcap-dev` and `libreadline-dev`. In Fedora based systems you need `libcap-devel` and `readline-devel`.

Optional dependencies: `sshfs`, `curlftpfs`, and `cifs-utils` (for remote filesystems support); `atool`, `archivemount`, `genisoimage`, `p7zip`, and `cdrtools` (for archiving and compression support), and `icons-in-terminal` to enable the icons feature.

### Arch Linux

You'll find the corresponding packages on the AUR: the [stable](https://aur.archlinux.org/packages/clifm) and the [development](https://aur.archlinux.org/packages/clifm-git) version. 

Of course, you can also clone, build, and install the package using the PKGBUILD file:

```sh
$ git clone https://github.com/leo-arch/clifm.git
$ cd clifm
$ makepkg -si
```

### Debian-based systems
**NEW**: A .deb package (for x86_64) is now available in [Releases](https://github.com/leo-arch/clifm/releases).

### Other Linux distributions (or FreeBSD):

1. Clone the repository

```sh
$ git clone https://github.com/leo-arch/clifm.git
$ cd clifm
```

2. Run `make` as follows:

```sh
$ sudo make install
```

You should find the binary file in `/usr/bin`, so that you can run it as any other program:

	$ clifm

To uninstall `clifm` issue this command wherever the Makefile is located:

	$ sudo make uninstall

---

## First steps

Try the `help` command to learn more about CliFM. Once in the CliFM prompt, type `help` or `?`. To jump into the **COMMANDS** section in the manpage, simply enter `cmd` or press <kbd>F2</kbd>. Press <kbd>F1</kbd> to access the full manpage and <kbd>F3</kbd> to access the keybindings help page.

You can also take a look at some of these [basic usage examples](https://github.com/leo-arch/clifm/wiki/Common-Operations#basic-usage-examples) to get you started.

---

## Support

CliFM is C99 and POSIX-1.2008 compliant (if compiled with the `_BE_POSIX` flag). It works on Linux and FreeBSD, on i686, x86_64, and ARM architectures.

---

## License
This project is licensed under the GPL version 2 (or later) license. See the [LICENSE](https://github.com/leo-arch/clifm/blob/master/LICENSE) file for details.

---

## Contributing
We welcome community contributions. Please see the [CONTRIBUTING.md](https://github.com/leo-arch/clifm/blob/master/CONTRIBUTING.md) file for details.
