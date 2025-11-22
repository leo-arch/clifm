<p align="center">
	<a href="https://github.com/leo-arch/clifm">
		<img src="misc/logo/clifm-banner.png" alt="Clifm logo" width="500" height="300">
	</a>
</p>
<h2 align="center">The Command Line File Manager</h2>
<h4 align="center">No GUI, no TUI, and no menus: just you and a powerful, file-management-oriented command line.</h4>
<h4 align="center"><a
href="https://github.com/leo-arch/clifm/#floppy_disk-installation">Install</a> · <a
href="https://github.com/leo-arch/clifm/wiki">Browse the documentation</a> · <a
href="https://github.com/leo-arch/clifm/issues">Request feature</a> · <a
href="https://github.com/leo-arch/clifm/issues">Report bug</a></h4>

---

<p align="center">
<a href="https://github.com/leo-arch/clifm/blob/master/LICENSE"><img src="https://img.shields.io/badge/license-GPL2%2B-red?style=flat"/></a>
<a href="https://github.com/leo-arch/clifm/releases"><img alt="GitHub release (latest by date)" src="https://img.shields.io/github/v/release/leo-arch/clifm"></a>
<a><img src="https://img.shields.io/github/commits-since/leo-arch/clifm/latest"></a>
<a><img src="https://img.shields.io/github/last-commit/leo-arch/clifm/master?color=blue&style=flat"/></a>
	<a href="https://software.opensuse.org//download.html?project=home%3Aarchcrack&package=clifm"><img src="https://img.shields.io/badge/CD-OBS-red?logo=opensuse&logoColor=white"/></a>
</p>

<p align="center">
<a href="https://github.com/leo-arch/clifm/actions/workflows/codeql-analysis.yml"><img src="https://github.com/leo-arch/clifm/actions/workflows/codeql-analysis.yml/badge.svg?branch=master"></a>
<a href="https://www.codacy.com/gh/leo-arch/clifm/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=leo-arch/clifm&amp;utm_campaign=Badge_Grade"><img src="https://app.codacy.com/project/badge/Grade/c2c24860fce64d2aa6ca8e1dd0981d6d"/></a>
<a href="https://build.opensuse.org/package/show/home:archcrack/CliFM"><img src="https://build.opensuse.org/projects/home:archcrack/packages/CliFM/badge.svg?type=default"></a>
</p>

---

[![demo](https://asciinema.org/a/ov5HnwdlPvnR7ucfDrQMfeg2N.svg)](https://asciinema.org/a/ov5HnwdlPvnR7ucfDrQMfeg2N?autoplay=1)

---

## :space_invader: Brief description

**Clifm** is a shell-like, text-based terminal file manager that sits on the command line.

Some of its key features include: bookmarks, search, file selection, file tags, file filters, file previews (including image previews), bulk rename, archiving, trash, file opener, directory jumper, autocommands, workspaces, plugins, autosuggestions, specific command line expansions, and more. Consult the [Features](#heavy_check_mark-features) section below for a more comprehensive list.

While feature-rich, powerful, and extensible, its true strength lies in its design philosophy. Unlike traditional terminal file managers, based on the text user interface (TUI), **clifm** embraces the command-line interface (CLI). This means you can interact with your filesystem by typing commands directly, just as you would in a regular shell, but with greater ease, speed, and advanced capabilities.

For this reason, **clifm** does not need to be better: it's **just different!** :wink:

To get started, consult the [Getting started](#bulb-getting-started) section below.

---

## :heavy_check_mark: Features

<details>
<summary>Click here to expand</summary>

In addition to common file operations, such as copy, move, remove, etc., **clifm** provides the following features:
- Specific
  - [Really CLI-based](https://github.com/leo-arch/clifm/wiki/Introduction#main-design-and-goals). No GUI nor TUI at all, but just a command-line
  - It can run on the kernel built-in console and even on a SSH or any other remote session
  - Highly compatible with old VT102-only terminal emulators like Rxvt and Rxvt-based ones: even on a terminal with only 8 colors and no Unicode support, **clifm** will just work. [It can run even on an old DEC-VT100 terminal!](https://github.com/leo-arch/clifm/wiki/Extra#clifm-running-on-a-dec-vt100-terminal-1978)
  - [High performance](https://github.com/leo-arch/clifm/wiki/Performance). Incredibly lightweight and fast even on really old hardware
  - [Short (and even one-character) commands](https://github.com/leo-arch/clifm/wiki/Introduction#commands-short-summary)
  - [Entry list numbers (ELNs)](https://github.com/leo-arch/clifm/wiki/Common-Operations#elns) for filenames
  - [Extended color codes](https://github.com/leo-arch/clifm/wiki/Customization#colors) for file-types and -extensions
  - [File counter](https://github.com/leo-arch/clifm/wiki/Introduction#interface) for directories and symlinks to directories
  - Support for files attributes, extended attributes, birth time, BSD flags, and Solaris doors.
  - Privacy: Zero data collection and no connection to the outside world at all
  - Security: [Secure environment](https://github.com/leo-arch/clifm/wiki/Specifics#security) and [secure commands](https://github.com/leo-arch/clifm/wiki/Specifics#security). See also the [stealth mode section](https://github.com/leo-arch/clifm/wiki/Specifics#stealth-mode)
- Navigation and file operations
  - [Bookmarks](https://github.com/leo-arch/clifm/wiki/Common-Operations#bookmarks)
  - [File selection](https://github.com/leo-arch/clifm/wiki/Common-Operations#selection) (supports both glob and regular expressions and works even across multiple instances of the program)
  - [File opener](https://github.com/leo-arch/clifm/wiki/Specifics#file-opener) (supports regular expressions and is able to discern between GUI and non-GUI environments)
  - [File previews](https://github.com/leo-arch/clifm/wiki/Advanced#files-preview) (including [image previews](https://github.com/leo-arch/clifm/tree/master/misc/tools/imgprev))
  - [File tags](https://github.com/leo-arch/clifm/wiki/Common-Operations#tagging-files)
  - [File filters](https://github.com/leo-arch/clifm/wiki/Advanced#files-filters) (including support for [`.hidden` files](https://github.com/leo-arch/clifm/wiki/Advanced#1b-hidden-files))
  - [File search](https://github.com/leo-arch/clifm/wiki/Common-Operations#searching) (supports both glob and regular expressions)
  - [File templates](https://github.com/leo-arch/clifm/wiki/Introduction#n-new)
  - [Filenames sanitizer](https://github.com/leo-arch/clifm/wiki/Introduction#bb-bleach)
  - [A Freedesktop-compliant trash system](https://github.com/leo-arch/clifm/wiki/Common-Operations#trashing-files)
  - [copy(-as), move(-as)](https://github.com/leo-arch/clifm/wiki/Introduction#c-l-e-edit-m-md-r), [interactive rename](https://github.com/leo-arch/clifm/wiki/Introduction#c-l-e-edit-m-md-r), and [open-with](https://github.com/leo-arch/clifm/wiki/Introduction#ow) functions
  - [Bulk operations](https://github.com/leo-arch/clifm/wiki/Advanced#bulk-operations): rename, create, remove, and create symbolik links in bulk
  - [File encryption/decryption (plugin)](https://github.com/leo-arch/clifm/wiki/Advanced#plugins)
  - [Archiving and compression](https://github.com/leo-arch/clifm/wiki/Advanced#archives) support (including Zstandard and ISO 9660)
  - [Symlinks editor](https://github.com/leo-arch/clifm/wiki/Introduction#l-le)
  - File permissions/ownership editor via the [`pc`](https://github.com/leo-arch/clifm/wiki/Introduction#pc) and [`oc`](https://github.com/leo-arch/clifm/wiki/Introduction#oc) commands respectively
  - [Advanced Copy](https://github.com/leo-arch/clifm/wiki/Advanced#cpmv-with-a-progress-bar) support (just `cp` and `mv` with a nice progress bar)
  - [Copy files to your smart phone (plugin)](https://github.com/leo-arch/clifm/wiki/Advanced#plugins)
  - [Autocommands](https://github.com/leo-arch/clifm/wiki/Specifics#autocommands)
  - [Auto-cd](https://github.com/leo-arch/clifm/wiki/Introduction#acd-autocd), [auto-open](https://github.com/leo-arch/clifm/wiki/Introduction#ao-auto-open), and [autols](https://github.com/leo-arch/clifm/wiki/Common-Operations#navigation)
  - [Directory jumper](https://github.com/leo-arch/clifm/wiki/Specifics#kangaroos-frecency-algorithm) (similar to [autojump](https://github.com/wting/autojump), [z.lua](https://github.com/skywind3000/z.lua), and [zoxide](https://github.com/ajeetdsouza/zoxide))
  - [Virtual directories](https://github.com/leo-arch/clifm/wiki/Advanced#virtual-directories)
  - [Fastback](https://github.com/leo-arch/clifm/wiki/Introduction#fastback) (quickly change to any parent directory)
  - [Up to eight workspaces](https://github.com/leo-arch/clifm/wiki/Specifics#workspaces)
  - [More than a dozen sort methods](https://github.com/leo-arch/clifm/wiki/Introduction#st-sort)
  - [Remote filesystems management](https://github.com/leo-arch/clifm/wiki/Introduction#net)
  - [Mount/unmount storage devices](https://github.com/leo-arch/clifm/wiki/Introduction#media)
  - Directory history map (keep in sight previous, current, and next entries in the directory history list)
- Shell
  - [Tab completion](https://github.com/leo-arch/clifm/wiki/Specifics#expansions-completions-and-suggestions), with _fzf_ integration (including [file previews](https://github.com/leo-arch/clifm/wiki/Advanced#files-preview))
  - [Command line expansions](https://github.com/leo-arch/clifm/wiki/Introduction#command-line-expansions) (including bookmarks, selected files, tags, MIME types, and file types)
  - [Auto-suggestions](https://github.com/leo-arch/clifm/wiki/Specifics#auto-suggestions)
  - [Syntax highlighting](https://github.com/leo-arch/clifm/wiki/Specifics#syntax-highlighting)
  - [Warning prompt](https://github.com/leo-arch/clifm/wiki/Customization#the-warning-prompt) (for invalid command names)
  - [Fused parameters for ELNs](https://github.com/leo-arch/clifm/wiki/Introduction#fused-parameters)
  - [Fuzzy completion for filenames and paths](https://github.com/leo-arch/clifm/wiki/Specifics#fuzzy-match)
  - Bash-like quoting system
  - Shell commands execution
  - Sequential and conditional commands execution
  - [Directory](https://github.com/leo-arch/clifm/wiki/Introduction#b-back) and [commands](https://github.com/leo-arch/clifm/wiki/Introduction/#commands-history) history
  - [Glob and regular expressions](https://github.com/leo-arch/clifm/wiki/Advanced#wildcards-and-regex) (including inverse matching)
  - [Aliases](https://github.com/leo-arch/clifm/wiki/Customization#aliases)
  - [Logs](https://github.com/leo-arch/clifm/wiki/Introduction#log)
  - [Prompt and profile commands](https://github.com/leo-arch/clifm/wiki/Customization#profile-and-prompt-commands) (run commands with each new prompt or at program startup)
- Modes
  - [Stealth mode](https://github.com/leo-arch/clifm/wiki/Specifics#stealth-mode) (also known as incognito or private mode)
  - [Light mode](https://github.com/leo-arch/clifm/wiki/Specifics#light-mode) (just in case it is not fast enough for you)
  - [File opener](https://github.com/leo-arch/clifm/wiki/Specifics#using-clifm-as-a-standalone-file-opener) (similar to `xdg` and Ranger's `rifle`)
  - [File previewer](https://github.com/leo-arch/clifm/wiki/Advanced#shotgun) (similar to [`pistol`](https://github.com/yonasBSD/pistol) and Ranger's `scope.sh`)
  - [Disk usage analyzer](https://github.com/leo-arch/clifm/wiki/Specifics#disk-usage-analyzer)
  - [File lister (ls-mode)](https://github.com/leo-arch/clifm/wiki/Advanced#files-lister-ls-mode)
  - [Stat mode](https://github.com/leo-arch/clifm/wiki/Advanced/#stat1-replacement) (just like the shell command **stat**)
- Customization
  - [Custom commands](https://github.com/leo-arch/clifm/wiki/Customization#custom-commands)
  - [User profiles](https://github.com/leo-arch/clifm/wiki/Specifics#profiles)
  - [Customizable keyboard shortcuts](https://github.com/leo-arch/clifm/wiki/Customization#keybindings)
  - [Theming](https://github.com/leo-arch/clifm/wiki/Customization#theming) (including [bundled color schemes](https://github.com/leo-arch/clifm/tree/master/misc/colors) and [**LS_COLORS** support](https://github.com/leo-arch/clifm/wiki/Customization#ls_colors-support))
  - [Prompt customization](https://github.com/leo-arch/clifm/wiki/Customization#the-prompt)
  - [Customizable keybindings for custom plugins](https://github.com/leo-arch/clifm/wiki/Customization#keybindings)
  - [Compile features in/out](https://github.com/leo-arch/clifm/blob/master/src/README.md#compiling-features-inout)
- Misc
  - [Plugins](https://github.com/leo-arch/clifm/wiki/Advanced#plugins)
  - [Icons support](https://github.com/leo-arch/clifm/wiki/Advanced#icons-smirk) (including emoji-icons :smirk:)
  - [Git integration](https://github.com/leo-arch/clifm/wiki/Advanced#git-integration)
  - [Desktop notifications](https://github.com/leo-arch/clifm/wiki/Specifics#desktop-notifications)
  - Unicode support.
  - [CD on quit](https://github.com/leo-arch/clifm/wiki/Advanced#cd-on-quit) and [file picker](https://github.com/leo-arch/clifm/wiki/Advanced#file-picker) functions
  - [A built-in pager](https://github.com/leo-arch/clifm/wiki/Introduction#pg-pager) for files listing
  - Read and list files from [STDIN (standard input)](https://github.com/leo-arch/clifm/wiki/Advanced#standard-input)
<h4 align="center"><br><i>Auto-suggestions in action</i></h4>
<p align="center"><img src="https://i.postimg.cc/1XSKBRh8/suggestions.gif"></a></p>

---
For a detailed explanation of each of these features, follow the corresponding links or consult the [Wiki](https://github.com/leo-arch/clifm/wiki).
</details>

---

## :clapper: Introduction video

[![Alt text](https://img.youtube.com/vi/CJmcisw9F90/0.jpg)](https://www.youtube.com/watch?v=CJmcisw9F90)

---

## :floppy_disk: Installation

### From a package manager

<details>
<summary>Packaging status <a href="https://repology.org/project/clifm/versions"><img src="https://repology.org/badge/tiny-repos/clifm.svg" alt="Packaging status"></a></summary>
<a href="https://repology.org/project/clifm/versions">
    <img src="https://repology.org/badge/vertical-allrepos/clifm.svg?columns=3" alt="Packaging status">
</a>
</details>

If running on Linux, [binary packages](https://software.opensuse.org//download.html?project=home%3Aarchcrack&package=clifm) are available for most major distributions via the OpenSUSE Build System.

### From source (Linux/BSD)

**Note**: Dependencies are most likely already satisfied, but, in any case, consult the [dependencies section](https://github.com/leo-arch/clifm/wiki/Introduction#1-satisfy-dependencies).

```sh
git clone https://github.com/leo-arch/clifm.git
cd clifm
sudo make install
```

For more information/supported platforms, consult the [installation page](https://github.com/leo-arch/clifm/wiki/Introduction#installation).

---

## :bulb: Getting started

To start using **clifm**, _you don't need to learn anything new_: the usual shell commands will just work. However, there is much more than just shell commands... \
✓ The `help` command gives you a quick introduction to **clifm**: once in the command prompt, enter `help` or `?`. \
✓ Type `cmd<TAB>` to get the list of available commands and a brief description. \
✓ Type `help <TAB>` to get the list of available _help topics_. Select the one you want and press <kbd>Enter</kbd>. \
✓ To jump into the **COMMANDS** section in the [manpage](https://github.com/leo-arch/clifm/blob/master/misc/clifm.1.pdf), simply enter `cmd` or press <kbd>F2</kbd>. \
✓ Press <kbd>F1</kbd> to access the full manpage and <kbd>F3</kbd> to access the keybindings help-page. \
✓ To get help about some specific command, just enter `CMD -h`. For instance, `s -h`.

You can also take a look at our [FAQ](https://github.com/leo-arch/clifm/wiki/FAQ) and these [basic usage-examples](https://github.com/leo-arch/clifm/wiki/Common-Operations#basic-usage-examples). \
For a complete description, please consult our [Wiki](https://github.com/leo-arch/clifm/wiki).

> [!TIP]
> Start **clifm** and just press a number (technically, an ELN or [Entry List Number](https://github.com/leo-arch/clifm/wiki/Common-Operations#elns)).

---

## :newspaper: What's new?

Consult the [changelog file](https://github.com/leo-arch/clifm/blob/master/CHANGELOG).

---

## Support

**Clifm** runs on Linux, Termux (Android), FreeBSD, NetBSD, OpenBSD, DragonFly, MacOS, Solaris/Illumos, Haiku, and Cygwin/MinGW, on x86, ARM, PowerPC, and RISC-V architectures.

---

## License
This project is licensed GPL version 2 (or later).
See the [LICENSE file](https://github.com/leo-arch/clifm/blob/master/LICENSE) for details.

---

## Contributing
Contributions are kindly welcome! Please see our [contribution guidelines](https://github.com/leo-arch/clifm/wiki/CONTRIBUTING) for details.

---

## Community

Visit the [Discussions section](https://github.com/leo-arch/clifm/discussions) of this repo and let us know what you think: ideas, comments, observations and questions are always useful.

---

## Developer
[Leo Abramovich](https://github.com/leo-arch) <<leo.clifm@outlook.com>>.

Special thanks to [all those who have contributed to this project](https://github.com/leo-arch/clifm/graphs/contributors).
