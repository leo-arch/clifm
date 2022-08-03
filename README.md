<p align="center">
	<a href="https://github.com/leo-arch/clifm">
		<img src="https://i.postimg.cc/Gm5vxMLp/256x256c.png" alt="CliFM logo" width="160" height="160">
	</a>
</p>
<h1 align="center">CliFM</h1>
<h2 align="center">The Command Line File Manager</h2>
<h3 align="center">Fast, extensible, lightweight</h3>
<h3 align="center">Linux, FreeBSD, NetBSD, OpenBSD, MacOS, Haiku | x86, ARM</h3>
<h4 align="center"><a
href="https://software.opensuse.org//download.html?project=home%3Aarchcrack&package=clifm">Downloads</a> · <a
href="https://github.com/leo-arch/clifm/wiki">Browse the documentation</a> · <a
href="https://github.com/leo-arch/clifm/blob/master/.github/ISSUE_TEMPLATE/feature-request.md">Request feature</a> · <a
href="https://github.com/leo-arch/clifm/issues">Report bug</a></h4>

---

<p align="center">
<a href="https://github.com/leo-arch/clifm/blob/master/LICENSE"><img src="https://img.shields.io/github/license/leo-arch/clifm?color=red&style=flat"/></a>
<a href="https://github.com/leo-arch/clifm/releases"><img alt="GitHub release (latest by date)" src="https://img.shields.io/github/v/release/leo-arch/clifm"></a>
<a><img src="https://img.shields.io/github/commits-since/leo-arch/clifm/latest"></a>
<a><img src="https://img.shields.io/github/last-commit/leo-arch/clifm/master?color=blue&style=flat"/></a>
<a href="https://repology.org/project/clifm/versions"><img src="https://repology.org/badge/tiny-repos/clifm.svg" alt="Packaging status"></a>
<a href="https://en.wikipedia.org/wiki/Privacy-invasive_software"><img src="https://img.shields.io/badge/privacy-✓-green?style=flat"/></a>
<a href="https://gitter.im/leo-arch/clifm"><img src="https://img.shields.io/gitter/room/leo-arch/clifm?style=flat"/></a>
<a href="https://software.opensuse.org//download.html?project=home%3Aarchcrack&package=clifm"><img src="https://img.shields.io/badge/CD-OBS-red?logo=opensuse&logoColor=white"/></a>
</p>

<p align="center">
<a href="https://github.com/leo-arch/clifm/actions/workflows/codeql-analysis.yml"><img src="https://github.com/leo-arch/clifm/actions/workflows/codeql-analysis.yml/badge.svg?branch=master"></a>
<a href="https://www.codacy.com/gh/leo-arch/clifm/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=leo-arch/clifm&amp;utm_campaign=Badge_Grade"><img src="https://app.codacy.com/project/badge/Grade/c2c24860fce64d2aa6ca8e1dd0981d6d"/></a>
<a href="https://lgtm.com/projects/g/leo-arch/clifm/context:cpp"><img alt="Language grade: C/C++" src="https://img.shields.io/lgtm/grade/cpp/g/leo-arch/clifm.svg?logo=lgtm&logoWidth=18"/></a>
<a href="https://app.codiga.io/project/30518/dashboard"><img alt="Code grade" src="https://api.codiga.io/project/30518/status/svg"/></a>
<!---
<a href="https://bestpractices.coreinfrastructure.org/projects/4884"><img src="https://bestpractices.coreinfrastructure.org/projects/4884/badge"></a>
-->
</p>

<!---
<a href="https://codecov.io/gh/leo-arch/clifm"><img src="https://codecov.io/gh/leo-arch/clifm/branch/master/graph/badge.svg?token=YC3NIS180Z"/></a>
[![LGTM](https://img.shields.io/lgtm/grade/cpp/g/leo-arch/clifm.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/leo-arch/clifm/context:cpp)
-->

---

## Table of contents
🔸 [Brief description](#brief-description) \
🔸 [Introduction video](#clapper-introduction-video) \
🔸 [Features](#features) \
🔸 [Installation](#installation) \
🔸 [Getting started](#getting-started) \
🔸 [Support](#support) \
🔸 [License](#license) \
🔸 [Contributing](#contributing) \
🔸 [Community](#community)

---

## Brief description

_CliFM_ is a **C**ommand **L**ine **I**nterface **F**ile **M**anager: all input and interaction is performed via commands. This is its main feature and strength.

Unlike most terminal file managers out there, indeed, _CliFM_ replaces the traditional TUI interface (also known as curses or text-menu based interface) by a command-line interface (CLI),<sup>1</sup> also known as REPL.<sup>2</sup>

If working with the command-line, your workflow is not affected at all, but just enriched with file management functionalities: automatic files listing, files selection, bookmarks, tags, directory jumper, directory and commands history, auto-cd and auto-open, bulk rename, TAB completion, autosuggestions, and a trash system, among [other features](#features). In this sense, _CliFM_ is certainly a file manager, but also a shell extension.

Briefly put, with _CliFM_ the command-line is always already there, never hidden :godmode: :muscle:

---
<sup>1</sup> <i>For more information about these concepts see the [resources page](https://github.com/leo-arch/clifm/wiki/Resources#gui-tui-and-cli) </i>. \
<sup>2</sup> <i>For a detailed description consult the [introductory section](https://github.com/leo-arch/clifm/wiki/Introduction#what-is-clifm) of our Wiki</i>.

---

<h4 align="center">CliFM's interface</h4>
<p align="center"><img src="https://i.postimg.cc/Zqp4sgLK/clifm-interface8.png"></p>

---

## :clapper: Introduction video

[![Alt text](https://img.youtube.com/vi/CJmcisw9F90/0.jpg)](https://www.youtube.com/watch?v=CJmcisw9F90)

<!---
<details>
<summary>Watch me fly!</summary>

<h3 align="center"><br><i>Did I say it's fast?</i></h3>
<p align="center"><a href="https://mega.nz/embed/J8hEkCZZ#fGp0JtcDvFIWKmTc4cOp0iMrWRlbqs99THg8F7EmQWI"><img src="https://i.postimg.cc/CKx6zrvL/vid-thumb.png"></a></p>

Music: "Quad Machine", by [Sonic Mayhem](https://en.wikipedia.org/wiki/Sascha_Dikiciyan) \
**Note**: Icons and files preview depend on third-party software. Consult the [icons](https://github.com/leo-arch/clifm/wiki/Advanced#icons-smirk) and [files preview](https://github.com/leo-arch/clifm/wiki/Advanced#files-preview) sections.

</details>
-->

## Features

<details>
<summary>Click here to expand</summary>

Besides common file operations such as copy, move, remove, etc., _CliFM_ provides the following features:
- Specific
  - [Really CLI-based](https://github.com/leo-arch/clifm/wiki/Introduction#main-design-and-goals). No GUI nor TUI at all, but just a command-line
  - It can run on the kernel built-in console and even on a SSH or any other remote session
  - Highly compatible with old VT102-only terminal emulators (8-colors only and no Unicode support) like Rxvt and Rxvt-based ones
  - [High performance](https://github.com/leo-arch/clifm/wiki/Performance). Incredibly lightweight and fast even on really old hardware
  - [Short (and even one-character) commands](https://github.com/leo-arch/clifm/wiki/Introduction#commands-short-summary)
  - [Entry list numbers (ELN's)](https://github.com/leo-arch/clifm/wiki/Common-Operations) for file names
  - [Extended color codes](https://github.com/leo-arch/clifm/wiki/Customization#colors) for file-types and -extensions
  - [Files counter](https://github.com/leo-arch/clifm/wiki/Introduction#interface) for directories and symlinks to directories
  - Privacy: Zero data collection and no connection to the outside world at all
  - Security: [Secure environment](https://github.com/leo-arch/clifm/wiki/Specifics#security) and [secure commands](https://github.com/leo-arch/clifm/wiki/Specifics#security). See also the [stealth mode section](https://github.com/leo-arch/clifm/wiki/Specifics#stealth-mode)
- Navigation and file operations
  - [Bookmarks](https://github.com/leo-arch/clifm/wiki/Common-Operations#bookmarks)
  - [Files tagging](https://github.com/leo-arch/clifm/wiki/Common-Operations#tagging-files)
  - [Files selection](https://github.com/leo-arch/clifm/wiki/Common-Operations#selection) (supports both glob and regular expressions and works even across multiple instances of the program)
  - [Files search](https://github.com/leo-arch/clifm/wiki/Common-Operations#searching) (supports both glob and regular expressions)
  - [copy(-as), move(-as)](https://github.com/leo-arch/clifm/wiki/Introduction#c-l-e-edit-m-md-r), [interactive rename](https://github.com/leo-arch/clifm/wiki/Introduction#c-l-e-edit-m-md-r), and [open-with](https://github.com/leo-arch/clifm/wiki/Introduction#ow-elnfilename-application) functions
  - [File names cleaner](https://github.com/leo-arch/clifm/wiki/Introduction#bb-bleach-elnfile--n)
  - [Autocommands](https://github.com/leo-arch/clifm/wiki/Specifics#autocommands)
  - [Auto-cd](https://github.com/leo-arch/clifm/wiki/Introduction#acd-autocd-on-off-status), [auto-open](https://github.com/leo-arch/clifm/wiki/Introduction#ao-auto-open-on-off-status), and [autols](https://github.com/leo-arch/clifm/wiki/Common-Operations#navigation)
  - [Directory jumper](https://github.com/leo-arch/clifm/wiki/Specifics#kangaroos-frecency-algorithm), similar to [autojump](https://github.com/wting/autojump), [z.lua](https://github.com/skywind3000/z.lua), and [zoxide](https://github.com/ajeetdsouza/zoxide)
  - [Fastback function](https://github.com/leo-arch/clifm/wiki/Introduction#fastback-function)
  - [Backdir - Quickly change to a parent directory](https://github.com/leo-arch/clifm/wiki/Introduction#bd-name)
  - [A built-in resource opener](https://github.com/leo-arch/clifm/wiki/Specifics#resource-opener) (supports regular expressions and is able to discern between GUI and non-GUI environments)
  - [A built-in Freedesktop-compliant trash system](https://github.com/leo-arch/clifm/wiki/Common-Operations#trashing-files)
  - [Files filter](https://github.com/leo-arch/clifm/wiki/Advanced#files-filters)
  - [Up to eight workspaces](https://github.com/leo-arch/clifm/wiki/Specifics#workspaces)
  - [Eleven sorting methods](https://github.com/leo-arch/clifm/wiki/Introduction#st-sort-method-rev)
  - [Bulk rename](https://github.com/leo-arch/clifm/wiki/Advanced#bulk-rename) and [bulk remove](https://github.com/leo-arch/clifm/wiki/Introduction#rr-dir-editor)
  - [Batch links](https://github.com/leo-arch/clifm/wiki/Introduction#bl-elnfile--n)
  - [Copy files to your smart phone (plugin)](https://github.com/leo-arch/clifm/wiki/Advanced#plugins)
  - [Archiving and compression](https://github.com/leo-arch/clifm/wiki/Advanced#archives) support (including Zstandard and ISO 9660)
  - [Symlinks editor](https://github.com/leo-arch/clifm/wiki/Introduction#c-l-e-edit-m-md-r)
  - [Remote file systems management](https://github.com/leo-arch/clifm/wiki/Introduction#net-name-edit-m-mount-name-u-unmount-name)
  - [Mount/unmount storage devices](https://github.com/leo-arch/clifm/wiki/Introduction#media)
  - [Advanced Copy](https://github.com/leo-arch/clifm/wiki/Advanced#cpmv-with-a-progress-bar) support (just `cp` and `mv` with a nice progress bar)
  - Directory history map to keep in sight previous, current, and next entries in the directory history list
- Shell
  - [Auto-suggestions](https://github.com/leo-arch/clifm/wiki/Specifics#auto-suggestions)
  - [TAB completion](https://github.com/leo-arch/clifm/wiki/Specifics#expansions-completions-and-suggestions) (including an FZF mode)
  - [Syntax highlighting](https://github.com/leo-arch/clifm/wiki/Specifics#syntax-highlighting)
  - [Warning prompt for invalid command names](https://github.com/leo-arch/clifm/wiki/Customization#the-warning-prompt)
  - [Fused parameters for ELN's](https://github.com/leo-arch/clifm/wiki/Introduction#fused-parameters)
  - [Fuzzy completion for file names and paths](https://github.com/leo-arch/clifm/wiki/Specifics#fuzzy-match)
  - [Wildcards expansion via <kbd>TAB</kbd>](https://github.com/leo-arch/clifm/wiki/Introduction#filter-files-with-the-tab-key) (`s *.[ch]<TAB>`)
  - [File types expansion via <kbd>TAB</kbd>](https://github.com/leo-arch/clifm/wiki/Introduction#filter-files-with-the-tab-key) (`=l<TAB>` to list all symlinks in the current dir)
  - Bash-like quoting system
  - Shell commands execution
  - Sequential and conditional commands execution
  - [Directory](https://github.com/leo-arch/clifm/wiki/Introduction#b-back-h-hist-clear-eln) and [commands](https://github.com/leo-arch/clifm/wiki/Introduction/#commands-history) history
  - [Glob and regular expressions](https://github.com/leo-arch/clifm/wiki/Advanced#wildcards-and-regex) (including inverse matching)
  - [Aliases](https://github.com/leo-arch/clifm/wiki/Customization#aliases)
  - [Logs](https://github.com/leo-arch/clifm/wiki/Introduction#log-clear-on-off-status)
  - [Prompt and profile commands](https://github.com/leo-arch/clifm/wiki/Customization#profile-and-prompt-commands) (run commands with each new prompt or at program startup)
- Modes
  - [Stealth mode](https://github.com/leo-arch/clifm/wiki/Specifics#stealth-mode): Leave no trace on the host system. No file is read, no file is written.
  - [Light mode](https://github.com/leo-arch/clifm/wiki/Specifics#light-mode) (just in case it is not fast enough for you)
  - [Resource opener](https://github.com/leo-arch/clifm/wiki/Specifics#using-clifm-as-a-standalone-resource-opener)
  - [Disk usage analyzer mode](https://github.com/leo-arch/clifm/wiki/Specifics#disk-usage-analyzer)
  - [Files lister (ls-mode)](https://github.com/leo-arch/clifm/wiki/Advanced#files-lister-ls-mode)
- Customization
  - [User profiles](https://github.com/leo-arch/clifm/wiki/Specifics#profiles)
  - [Customizable keyboard shortcuts](https://github.com/leo-arch/clifm/wiki/Customization#keybindings)
  - [Theming support](https://github.com/leo-arch/clifm/wiki/Customization#theming) (more than a dozen color schemes)
  - [Prompt customization](https://github.com/leo-arch/clifm/wiki/Customization#the-prompt)
  - [Four customizable keybindings for custom plugins](https://github.com/leo-arch/clifm/wiki/Customization#keybindings)
  - [Compile features in/out](https://github.com/leo-arch/clifm/blob/master/src/README.md#compiling-features-inout)
- Misc
  - [Plugins](https://github.com/leo-arch/clifm/wiki/Advanced#plugins)
  - [Files preview](https://github.com/leo-arch/clifm/wiki/Advanced#files-preview) (via _BFG_, a native file previewer, but including support for [Ranger's scope.sh](https://github.com/ranger/ranger/blob/master/ranger/data/scope.sh) and [pistol](https://github.com/doronbehar/pistol) as well)
  - [Icons support](https://github.com/leo-arch/clifm/wiki/Advanced#icons-smirk), including emoji-icons :smirk:
  - [Git integration](https://github.com/leo-arch/clifm/wiki/Advanced#git-integration)
  - Unicode suppport
  - Disk usage
  - [CD on quit](https://github.com/leo-arch/clifm/wiki/Advanced#cd-on-quit) and [file picker](https://github.com/leo-arch/clifm/wiki/Advanced#file-picker) functions
  - [A built-in pager](https://github.com/leo-arch/clifm/wiki/Introduction#pg-pager-on-off-status) for files listing
  - Read and list files from [STDIN (standard input)](https://github.com/leo-arch/clifm/wiki/Advanced#standard-input)
<h4 align="center"><br><i>Auto-suggestions in action</i></h4>
<p align="center"><img src="https://i.postimg.cc/1XSKBRh8/suggestions.gif"></a></p>

---
For a detailed explanation of each of these features, follow the corresponding links or consult the [Wiki](https://github.com/leo-arch/clifm/wiki).
</details>

✔️ **Changelog**
* `version 1.6.1`  - Development:
  - [Configuration files renamed from `.cfm` to `.clifm`](https://github.com/leo-arch/clifm/wiki/Specifics#new-extension-for-configuration-files) (avoid conflict with [ColdFusion](https://en.wikipedia.org/wiki/ColdFusion_Markup_Language) files)
  - <kbd>Ctrl-l</kbd> added for screen refresh
  - `cc` command removed to avoid conflicts with `/bin/cc` (use `colors` instead)
  - `--std-tab-comp` option renamed to `--stdtab` (to match `--fzytab` and `--smenutab` options)
  - **NEW**: [Customize properties fields in long view mode](https://github.com/leo-arch/clifm/wiki/Common-Operations#file-details) 
  - **NEW**: [Desktop notifications](https://github.com/leo-arch/clifm/wiki/Specifics#desktop-notifications)
  - **NEW**: [Virtual directories](https://github.com/leo-arch/clifm/wiki/Advanced#virtual-directories)
  - **NEW**: [A better organized configuration file](https://github.com/leo-arch/clifm/blob/master/misc/clifmrc)
  - **NEW**: [Improved logs system](https://github.com/leo-arch/clifm/wiki/Introduction#log-clear-on-off-status)
  - **NEW**: [smenu support for TAB completion](https://github.com/leo-arch/clifm/wiki/Specifics#a-different-alternative-smenu)
  - **NEW**: [FZY support for TAB completion](https://github.com/leo-arch/clifm/wiki/Specifics#no-fzf-try-with-fzy)
  - **NEW**: [Operate on specific bookmarks](https://github.com/leo-arch/clifm/wiki/Common-Operations#operating-on-specific-bookmarks)
  - **NEW**: [Named workspaces](https://github.com/leo-arch/clifm/wiki/Specifics#workspaces)
* `version 1.6 (Guybrush)`:
  - ELN's color defaults now to cyan
  - `--no-folders-first` and `--folders-first` options renamed to `--no-dirs-first` and `--dirs-first` respectivelly. In the same way, the `folders-first` command was renamed to `dirs-first`.
  - `PromptStyle` option renamed as `Notifications` (taking `true` and `false` as values)
* `version 1.5`:
  - `Prompt`, `WarningPromptStr`, `DividingLine`, and `FfzTabOptions` options were moved from the config file to the color scheme file to get a **centralized and single theming file**. However, to keep backwards compatibility, the old location is still recognized. If any of these options is found in the color scheme file, values taken from the main configuration file will be overriden.
  - The [warning prompt](https://github.com/leo-arch/clifm/wiki/Customization#the-warning-prompt) color is set now via escape codes (exactly as the regular prompt). The `wp` color code is used now only for the _input text color_ of the warning prompt.
* `version 1.4`:
  - In order to make _Lira_ more powerful<sup>1</sup> it was necessary to introduce [a little syntax modification](https://github.com/leo-arch/clifm/wiki/Specifics#syntax) in its configuration file.

<sup>1</sup> _It can now match entire file names instead of just file extensions_

---

## Installation

Consult the [installation page](https://github.com/leo-arch/clifm/wiki/Introduction#installation).

---

## Getting started

To start using _CliFM_ **you don't need to learn anything new**: the usual shell commands will just work. However, there is much more than just shell commands... \
✓ The `help` command gives you a quick introduction to _CliFM_: once in the _CliFM_ prompt, type `help` or `?`. \
✓ Type `help` and then press <kbd>TAB</kbd> to get a list of _help topics_. Select the one you want and press <kbd>Enter</kbd>. \
✓ To jump into the **COMMANDS** section in the [manpage](https://github.com/leo-arch/clifm/blob/master/misc/clifm.1.pdf), simply enter `cmd` or press <kbd>F2</kbd>. \
✓ Press <kbd>F1</kbd> to access the full manpage and <kbd>F3</kbd> to access the keybindings help-page. \
✓ To get help about some specific command just type `CMD -h`. For instance, `s -h`.

You can also take a look at some of these [basic usage-examples](https://github.com/leo-arch/clifm/wiki/Common-Operations#basic-usage-examples) to get you started. \
For a complete description please consult our [Wiki](https://github.com/leo-arch/clifm/wiki).

---

## Support

_CliFM_ is C99 and POSIX-1.2008 compliant (if compiled with the `_BE_POSIX` flag). Consult the [compilation page](https://github.com/leo-arch/clifm/blob/master/src/README.md#5-compilation).\
It works on Linux, FreeBSD, NetBSD, OpenBSD, MacOS, and Haiku, on x86 and ARM architectures.

---

## License
This project is licensed GPL version 2 (or later). \
See the [LICENSE file](https://github.com/leo-arch/clifm/blob/master/LICENSE) for details.

---

## Contributing
Yes. Please see our [contribution guidelines](https://github.com/leo-arch/clifm/blob/master/CONTRIBUTING.md) for details.
[![Translation status](https://hosted.weblate.org/widgets/clifm/-/clifm/svg-badge.svg)](https://hosted.weblate.org/engage/clifm/?utm_source=widget) at Hosted Weblate.

---

## Community
Join our [Gitter discussion room](https://gitter.im/leo-arch/clifm) and let us know what you think: ideas, comments, observations and questions are always welcome. \
The [Discussions section](https://github.com/leo-arch/clifm/discussions) of this repo is also open to input.
