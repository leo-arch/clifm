/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2016-2025, L. Abramovich <leo.clifm@outlook.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
*/

/* messages.h - Usage and help messages for Clifm */

#ifndef MESSAGES_H
#define MESSAGES_H

/* Usage messages */
# define GRAL_USAGE "[OPTION]... [DIR]..."

#define ACTIONS_USAGE "List or edit actions/plugins\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  actions [list | edit [APP]]\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- List available actions/plugins\n\
    actions list\n\
  Note: Since 'list' is the default action, it can be omitted.\n\
- Open/edit the actions configuration file with nano\n\
    actions edit nano\n\
- Open/edit the actions configuration file with the default application\n\
    actions edit\n\n\
Actions are just names for plugins.\n\
An action definition has the following form: \"NAME=plugin\", for example:\n\
\"//=rgfind.sh\".\n\
To run a plugin enter the action name. For example, to run the rgfind.sh plugin,\n\
enter \"//\".\n\
Some plugins accept parameters. To get information about a specific plugin\n\
use the -h,--help flag. Example: \"- --help\"."

#define ALIAS_USAGE "List, print, or import aliases\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  alias [import FILE | list | NAME]\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- List available aliases\n\
    alias\n\
  or\n\
    alias list (or 'alias <TAB>')\n\
- Print a specific alias definition\n\
    alias my_alias\n\
- Import aliases from ~/.bashrc\n\
    alias import ~/.bashrc\n\
  Note: Only aliases following the POSIX specification (NAME=\"STR\")\n\
  will be imported.\n\
- Add a new alias\n\
    Run 'config', go to the aliases section, and write:\n\
      alias myalias=\"mycommand\"\n\
    Save and quit the editor."

#define ARCHIVE_USAGE "Compress/archive files\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  ac, ad FILE...\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- Compress/archive all selected files\n\
    ac sel\n\
- Compress/archive a range of files\n\
    ac 12-24\n\
  or\n\
    'ac <TAB>' to select from a list (multi-selection is allowed)\n\
- Decompress/dearchive a file\n\
    ad file.tar.gz\n\
  or just open the file (the appropriate menu will be displayed)\n\
    o file.tar.gz (or just 'file.tar.gz')\n\n\
\x1b[1mDEPENDENCIES\x1b[22m\n\
zstd(1)           Everything related to Zstandard\n\
mkisofs(1)        Create ISO 9660 files\n\
7z(1) / mount(1)  Operate on ISO 9660 files\n\
archivemount(1)   Mount archives\n\
atool(1)          Extraction/decompression, listing, and repacking of archives"

#define AUTOCD_USAGE "Turn autocd on/off\n\
\x1b[1mUSAGE\x1b[22m\n\
  acd, autocd [on | off | status]"

#define AUTO_USAGE "Set a temporary autocommand for the current directory\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  auto [list | none | unset | OPTION=VALUE,...]\n\n\
Unlike permanent autocommands (defined in the configuration file),\n\
options set via the 'auto' command are temporary, i.e. valid only for the\n\
current directory and the current session.\n\n\
Options set via this command take precedence over both permament autocommands\n\
and regular options (set via either the command line or the configuration file).\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- Print the list of available autocommands\n\
    auto list\n\
- List files in the current directory in long view\n\
    auto lv=1\n\
- List only PDF files, set the color scheme to nord, and sort files by size\n\
    auto ft=.*\\.pdf$,cs=nord,st=size\n\
- Same as above, but sequentially (previous values are preserved)\n\
    auto ft=.*\\.pdf$\n\
    auto cs=nord\n\
    auto st=size\n\
- Unset the file filter and the color scheme, and change sort to blocks\n\
    auto ft=,cs=,st=blocks\n\
- Unset all temporary options previously set for the current directory\n\
    auto unset\n\
- Reload the current directory ignoring all autocommands (includes permanent autocommands)\n\
    auto none\n\n\
For the list of available option codes enter 'help autocommands'."

#define AUTOCMDS_USAGE "Tweak settings or run custom commands on a per-directory basis\n\n\
There are two ways to set autocommands:\n\
  1) Via the 'autocmd' keyword in the configuration file\n\
  2) Via specifically named files in the corresponding directory\n\n\
1) Example using the first method:\n\n\
Edit the configuration file ('config' or F10) and add the following line:\n\n\
  autocmd /media/remotes/** fc=0,lm=1\n\n\
This instructs Clifm to always disable the file counter and to run in\n\
light mode whenever you enter the '/media/remotes' directory (or any\n\
subdirectory).\n\n\
Note: To match only '/media/remotes' write \"/media/remotes\" instead,\n\
and to match all subdirectories (excluding the parent directory itself),\n\
write \"/media/remotes/*\".\n\n\
The following codes are used to control Clifm's settings:\n\n\
  Code | Description     | Example\n\
  cs     Color scheme      cs=nord\n\
  fc     File counter      fc=0\n\
  ft     File filter       ft=.*\\.pdf$\n\
  fz     Full dir size     fz=1\n\
  hf,hh  Hidden files      hf=0\n\
  lm     Light mode        lm=1\n\
  lv,ll  Long view         lv=1\n\
  mf     Max files         mf=100\n\
  mn     Max name length   mn=30\n\
  od     Only directories  od=1\n\
  pg     Pager             pg=0\n\
  st     Sort method       st=size\n\
  sr     Sort reverse      sr=1\n\n\
To run a shell command or a script use the '!CMD' expression. For example:\n\n\
  autcomd ~/important !printf \"Get outta here!\" && read -n1\n\
  autcomd ~/Documents !~/my_script.sh\n\
\n\
Autocommand notifications are controlled by the InformAutocmd option in the\n\
configuration file.\n\
\n\
2) Example using the second method:\n\n\
a. Set 'ReadAutocmdFiles' to 'true' in the configuration file.\n\
b. Create a '.cfm.in' file in the '~/Important' directory with the following\n\
content:\n\n\
  echo \"Please keep me in sync with work files\" && read -n1\n\n\
This little reminder will be displayed every time you enter the 'Important'\n\
directory.\n\n\
If the file is named rather '.cfm.out', the command will be executed when\n\
leaving, instead of entering, the directory.\n\n\
Note 1: Only single-line commands are allowed. If you need more elaborated\n\
stuff, set here the path to a script doing whatever needs to be done.\n\n\
Note 2: Codes to modify Clifm's settings (as described in the first method)\n\
are not available here.\n\n\
Note 3: To set a temporary autocommand for the current directory use the\n\
'auto' command. Run 'auto --help' for details."

#define AUTO_OPEN_USAGE "Toggle auto-open\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  ao, auto-open [on | off | status]"

#define BACK_USAGE "Change to the previously visited directory\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  b, back [h, hist | clear | !ELN]\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- Just change to the previously visited directory\n\
    b (also Alt+j or Shift+Left)\n\
- Print the directory history list\n\
    b hist (or 'dh')\n\
- Change to the directory whose ELN in the list is 24\n\
    b !24\n\
- Use the 'f' command to go forward\n\
    f (also Alt+k or Shift+Right)"

#define BD_USAGE "Change to a parent directory matching NAME. If \
NAME is not specified, print the list of all parent directories\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  bd [NAME]\n\n\
\x1b[1mEXAMPLE\x1b[22m\n\
- Supposing you are in ~/Documents/misc/some/deep/dir, change to\n\
~/Documents/misc\n\
    bd mi (or 'bd <TAB>' to select from a list)"

#define BL_USAGE "Create multiple symbolic links at once\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  bl FILE...\n\n\
\x1b[1mEXAMPLE\x1b[22m\n\
- Symlink files 'file1', 'file2', 'file3', and 'file4' at once\n\
    bl file* (or 'bl <TAB>' to select from a list - multi-selection is\n\
  allowed)\n\
- Create symbolic links in the directory 'dir' for all .png files\n\
    s *.png\n\
    cd dir\n\
    bl sel\n\n\
Note: Links are always created in the current directory."

#define BLEACH_USAGE "Sanitize filenames by removing or converting non-ASCII characters\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  bb, bleach FILE...\n\n\
\x1b[1mEXAMPLE\x1b[22m\n\
- Sanitize filenames in your Downloads directory\n\
    bb ~/Downloads/*"

#define BOOKMARKS_USAGE "Manage bookmarks\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  bm, bookmarks [a, add FILENAME NAME | d, del NAME | e, edit [APP] | NAME]\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- List bookmarks\n\
    bm (also Alt+b, 'bm <TAB>' or 'b:<TAB>')\n\
- Bookmark the directory '/media/mount' as 'mnt'\n\
    bm add /media/mount mnt\n\
  Note: Regular files can also be bookmarked\n\
- Access the bookmark named 'mnt'\n\
    bm mnt (also 'b:mnt', 'b:<TAB>' or 'bm <TAB>' to select from a list)\n\
- Remove the bookmark named 'mnt'\n\
    bm del mnt (or 'bm del <TAB>' to select from a list)\n\
- Edit the bookmarks file\n\
    bm edit (or F11)\n\
- Edit the bookmarks file using vi\n\
    bm edit vi\n\
- Print file properties of specific bookmarks using the 'b:' construct\n\
    p b:<TAB> (multi-selection is allowed)\n\
- Select all bookmarks at once\n\
    s b:"

#define BULK_RENAME_USAGE "Rename files in bulk\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  br, bulk FILE... [:EDITOR]\n\n\
The list of files to be renamed is opened via EDITOR (default associated\n\
application for text files if omitted). Edit filenames as desired, rename,\n\
save changes, and quit the editor (quit without saving to cancel the operation).\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- Bulk rename all files ending with .pdf in the current directory\n\
    br *.pdf\n\
  or\n\
    'br <TAB>' to select from a list (mutli-selection is allowed)\n\
- Bulk rename all selected files\n\
    br sel\n\
  or, using vi:\n\
    br sel :vi"

#define CD_USAGE "Change the current working directory\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  cd [DIR]\n\n\
\x1b[1mEXAMPLE\x1b[22m\n\
- Change to /var\n\
    cd /var\n\
  or, if autocd is enabled (default)\n\
    /var"

#define COLORS_USAGE "Preview the current color scheme\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  colors\n\n\
Note: 'cs preview' can be used as well."

#define COLUMNS_USAGE "Toggle columned file listing\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  cl, columns [on | off]"

#define CS_USAGE "Switch color schemes\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  cs, colorschemes [COLORSCHEME | edit [APP] | preview | check-ext | name]\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- Print the name of the current color scheme\n\
    cs name (or 'cs n')\n\
- List available color schemes\n\
    cs (or 'cs <TAB>')\n\
- Preview the current color scheme\n\
    cs preview (or 'cs p')\n\
- Check for file extension conflicts\n\
    cs check-ext\n\
- Edit the current color scheme\n\
    cs edit\n\
- Edit the current color scheme using vi\n\
    cs edit vi\n\
- Switch to the color scheme named 'mytheme'\n\
    cs mytheme\n\n\
Tip: Theming via LS_COLORS is also possible. To enable this feature,\n\
run with the --lscolors option. Consult the Wiki for more information:\n\
https://github.com/leo-arch/clifm/wiki/Customization#ls_colors-support"

#define DESEL_USAGE "Deselect files\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  ds, desel [*, a, all]\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- Deselect all files\n\
    ds * (or Alt+d)\n\
- Deselect files from a menu\n\
    ds (or 'ds <TAB>' to select from a list - multi-selection is allowed)"

#define DESKTOP_NOTIFICATIONS_USAGE "Errors, warnings, and notices are displayed \
using desktop notifications instead of\n\
being printed immediately before the next prompt.\n\n\
To enable this feature use the --desktop-notifications command line flag or\n\
the DesktopNotifications option in the configuration file (F10). Supported values\n\
are: 'kitty', 'system', or 'false'.\n\n\
If set to 'kitty', notifications are displayed using the Kitty Notifications\n\
Protocol (requires the Kitty terminal or a terminal supporting this protocol).\n\n\
If set to 'system', notifications are displayed using one of the following commands:\n\n\
Linux/BSD: notify-send -u \"TYPE\" \"TITLE\" \"MSG\"\n\
MacOS:     osascript -e 'display notification \"MSG\" subtitle \"TYPE\" with title \"TITLE\"'\n\
Haiku:     notify --type \"TYPE\" --title \"TITLE\" \"MSG\"\n\n\
Note: It is the notifications daemon itself who takes care of actually displaying\n\
notifications on your screen. For troubleshoting, consult your \
daemon's documentation.\n\n\
Tip: You can always check notifications using the 'msg' command."

#define DH_USAGE "Query the directory history list\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  dh [STRING] [PATH] [!ELN]\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- Print the directory history list\n\
    dh (also 'dh <TAB>')\n\
- Print directory history entries matching \"query\"\n\
    dh query (also 'dh query<TAB>')\n\
- Change to the entry number (ELN) 12\n\
    dh !12\n\
  Note: Entry numbers are not displayed when using tab completion.\n\n\
Note: If the first argument is an absolute path, 'dh' works just like 'cd'.\n\
Tip: Take a look at the 'j' command as well."

#define DIRHIST_USAGE "List or access entries in the directory history list\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  b/f [hist | clear | !ELN]\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- Print the directory history list\n\
    b hist\n\
- Change to the directory whose ELN is 12 in the directory history list\n\
    b !12\n\
- Remove all entries from the directory history list\n\
    b clear\n\n\
Tip: See also the 'dh' and 'j' commands."

#define DUP_USAGE "Duplicate files via rsync(1) (cp(1) if rsync is not found)\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  d, dup FILE...\n\n\
\x1b[1mEXAMPLE\x1b[22m\n\
- Duplicate files whose ELNs are 12 through 20\n\
    d 12-20\n\n\
You will be prompted to enter a destination directory.\n\
Duplicated files are created as SRC.copy, and, if SRC.copy exists, as \n\
SRC.copy-n, where n is an positive integer (starting at 1).\n\n\
Parameters passed to rsync: --aczvAXHS --progress\n\n\
Parameters passed to cp:    -a"

#define CONFIG_USAGE "Edit the main configuration file\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  config [reload | reset | dump | APPLICATION]\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- Open/edit the configuration file\n\
    config (or F10)\n\
- Open/edit the configuration file using nano\n\
    config nano\n\
- Print current values, highlighting those deviating from default values\n\
    config dump\n\
- Reload the main configuration file and update settings accordingly\n\
    config reload\n\
- Reset the configuration file to its default state (keeping a backup of the existing file)\n\
    config reset"

#define EXT_USAGE "Turn on/off the use of external commands\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  ext [on, off, status]"

#define EXPORT_FILES_USAGE "Export filenames to a temporary file\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  exp [FILE...]\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- Export all selected files\n\
    exp sel\n\
- Export all PDF files in the current directory\n\
    exp *.pdf"

#define EXPORT_VAR_USAGE "Add variables to the environment\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  export NAME=VALUE..."

#define FC_USAGE "Toggle the file counter for directories\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  fc [on | off | status]"

#define FILE_DETAILS "List file details\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- Toggle the long view\n\
    ll (also Alt+l)\n\
  Note: Use PropFields in the configuration file to customize output\n\
  fields (and TimeStyle for custom timestamp formats).\n\
- Print file properties of the file whose ELN is 4\n\
    p4\n\
- Print file properties, using recursive directory sizes\n\
    pp DIR\n\n\
Note: An exclamation mark (!) before directory sizes means that an\n\
error ocurred while reading a subdirectory (most of the time due to\n\
insufficient permissions), so that sizes may not be accurate.\n\n\
Note 2: Unlike 'p', 'pp' always dereferences symbolic links."

#define FILE_SIZE_USAGE "File sizes/disk usage\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- Enable recursive directory sizes (long view)\n\
    fz on (or --full-dir-size)\n\
- Toggle the disk usage analyzer mode\n\
    Alt+Tab (or -t,--disk-usage-analyzer)\n\
- Display physical file sizes (disk usage) instead of logical sizes (apparent size)\n\
    Run with --physical-size or set ApparentSize to false in the\n\
    configuration file.\n\
- Display file sizes in SI units (powers of 1000) instead of IEC units (powers of 1024)\n\
    Run with --si"

#define FF_USAGE "Turn list-directories-first on/off\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  ff, dirs-first [on | off | status]\n\
\x1b[1mEXAMPLE\x1b[22m\n\
- Disable list-directories-first\n\
    ff off\n\
  Note: Toggle list-directories-first by pressing Alt+g."

#define FILE_PREVIEWS "\
File previews are enabled by default if running in fzf mode.\n\n\
To disable this feature, run with '--no-fzfpreview' or set 'FzfPreview' to\n\
false in the configuration file.\n\n\
Clifm operates in fzf mode if the fzf binary is found in your $PATH.\n\
You can also use '--fzftab' or 'TabCompletionMode' in the configuration file.\n\n\
File previews are generated using a configuration file, which can be edited to\n\
your liking by running 'view edit' (or pressing F7).\n\n\
Use the 'view' command to preview files in the current directory in full screen.\n\n\
To prevent large files from generating a preview, use the PreviewMaxSize option\n\
in the configuration file. For example: 'PreviewMaxSize=100M' (supported size\n\
units: K, M, G, T; supported range: 1K-2047G). By default, there is no limit.\n\n\
To learn how to enable image previews run 'help image-previews'."

#define IMAGE_PREVIEWS "\x1b[1mENABLING IMAGE PREVIEWS\x1b[22m\n\
\n\
Edit shotgun's configuration file ('view edit' or F7) and uncomment the\n\
'clifmimg' lines at the top of the file.\n\
\n\
This instructs Clifm to use the 'clifmimg' script (~/.config/clifm/clifmimg)\n\
to generate image previews (for both tab completion in fzf mode and the\n\
'view' command).\n\
\n\
By default, Clifm will try to guess the most suitable method. However, you\n\
can edit the 'clifmimg' script and set the 'method' variable to any of the\n\
available previewing methods: sixel, iterm, ueberzug, kitty, ansi (text mode).\n\
\n\
If using the 'ueberzug' method, you must start Clifm via the 'clifmrun' script\n\
(~/.config/clifm/clifmrun)"

#define FILTER_USAGE "Set a filter for the file list\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  ft, filter [unset | [!]REGEX,=FILE-TYPE-CHAR]\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- Print the current filter, if any\n\
    ft\n\
- Do not list hidden files\n\
    ft !^\\.\n\
- List only files ending with \".pdf\"\n\
    ft .*\\.pdf$\n\
- List only symbolic links\n\
    ft =l\n\
- Do not list socket files\n\
    ft !=s\n\
  Note: See below for the list of available file type characters.\n\
- Unset the current filter\n\
    ft unset\n\n\
You can also filter files in the current directory using tab\n\
completion via wildcards and the file type filter:\n\
- List PDF files\n\
    /*.pdf<TAB>\n\
- List executable files\n\
    =x<TAB>\n\n\
Available file type characters:\n\
  b: Block devices\n\
  c: Character devices\n\
  d: Directories\n\
  D: Empty directories (2)\n\
  f: Regular files\n\
  F: Empty regular files (2)\n\
  h: Multi-hardlink files (2)\n\
  l: Symbolic links\n\
  L: Broken symbolic links (2)\n\
  p: FIFO/pipes\n\
  s: Sockets\n\
  C: Files with capabilities (1)(2)\n\
  o: Other-writable files (2)\n\
  O: Doors (Solaris only)\n\
  P: Port (Solaris only)\n\
  t: Files with the sticky bit set (2)\n\
  u: SUID files (2)\n\
  g: SGID files (2)\n\
  x: Executable files (2)\n\n\
(1) Only via tab completion\n\
(2) Not available in light mode\n\n\
Type '=<TAB>' to get the list of available file type filters.\n\n\
Other ways of filtering files in the current directory:\n\n\
* @<TAB>       List all MIME types found\n\
* @query<TAB>  MIME type filter. E.g.: @pdf<TAB> to list all PDF files\n\
* /query       Quick search function: consult the 'search' help topic\n\
* Alt+.        Toggle hidden files\n\
* Alt+,        Toggle list-only-dirs\n\
* Just press TAB (fzf/fnf mode) and perform a fuzzy search\n\n\
You can also operate on files filtered by file type and/or MIME type as\n\
follows:\n\n\
    CMD =file-type-char @query\n\n\
For example, to select all executable files, symbolic links, and image\n\
files in the current directory:\n\n\
    s =x =l @image"

#define FORTH_USAGE "Change to the next visited directory\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  f, forth [h, hist | clear | !ELN]\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- Just change to the next visited directory\n\
    f (also Alt+k or Shift+Right)\n\
- Print the directory history list\n\
    f hist (or 'dh')\n\
- Change to the directory whose ELN in the list is 24\n\
    f !24\n\
- Use the 'b' command to change to the previously visited directory\n\
    b (also Alt+j or Shift+Left)"

#define FZ_USAGE "Toggle recursive directory sizes (long view only)\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  fz [on, off]"

#define HELP_USAGE "Get help\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  help [TOPIC]\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- Print the help screen\n\
    help\n\
- Get help about the 'bookmarks' topic\n\
    help bookmarks\n\
- Print the list of available help topics\n\
    help <TAB>"

#define HF_USAGE "Turn hidden files on/off\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  hf, hh, hidden [on | off | first | last | status]\n\n\
\x1b[1mEXAMPLE\x1b[22m\n\
- Show hidden files\n\
    hh on\n\
  Note: Use first/last to sort hidden files before/after non-hidden files.\n\
- Toggle hidden files\n\
    hh (or Alt+.)"

#define HISTEXEC_USAGE "Access command history entries\n\n\
\x1b[1mUSAGE\x1b[22m\n\
history or !<TAB>: List available commands\n\
!!: Execute the last command\n\
!n: Execute the command number 'n' in the history list\n\
!-n: Execute the last - n command in the history list"

#define HISTORY_USAGE "List or access command history entries\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  history [edit [APP] | clear | -N | on | off | status | show-time [-N]]\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- Print the complete list of commands in history\n\
    history\n\
- Print the complete list of commands in history (with timestamps)\n\
    history show-time\n\
- Print the last 4 commands in history\n\
    history -4\n\
  Note: 'history show-time -4' to add timestamps.\n\
- Prevent subsequent commands from being written to the history file\n\
    history off (then reenable it via 'history on')\n\
  Note: Starting a command by a space prevent it from being added to history.\n\
- Edit the commands history list\n\
    history edit\n\
- Edit the commands history list using vi\n\
    history edit vi\n\
- Clear the history list\n\
    history clear\n\n\
You can also access the command history via the exclamation mark (!).\n\
- List available commands\n\
    !<TAB>\n\
- List all history entries matching 'sudo'\n\
    !sudo<TAB>\n\
- Execute the last command\n\
    !!\n\
- Execute the command number 'n' in the history list\n\
    !n\n\
- Execute the 'last - n' command in the history list\n\
    !-n\n\n\
Tip: Use HistIgnore in the configuration file to exclude command lines\n\
from the history list."

#define ICONS_USAGE "Turn icons on/off\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  icons [on | off]\n\n\
Note: Depending on how the terminal renders icons, the apparent space\n\
between icons and filenames may not be optimal. This space can be adjusted\n\
using the IconsGap option in the configuration file (valid values: 0, 1, or 2)."

#define JUMP_USAGE "Change to a directory in the jump database (visited directories)\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  j [--purge [NUM] | --edit [APP]], jc, jp, jl [STRING]..., je\n\n\
For information about the matching algorithm consult the manpage\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- Print the list of entries in the jump database\n\
    j (or jl)\n\
- List all entries matching \"str\"\n\
    jl str\n\
- Jump (cd) to the best-ranked directory matching \"bui\"\n\
    j bui\n\
  Note: Hit TAB to get a list of possible matches: 'j bui<TAB>'.\n\
- If not enough, use multiple query strings\n\
    j ho bui\n\
  Note: Most likey, this will take you to '/home/build'\n\
- Jump to the best-ranked PARENT directory matching \"str\"\n\
    jp str\n\
- Jump to the best-ranked CHILD directory matching \"str\"\n\
    jc str\n\
- Mark an entry as permanent\n\
    You can accomplish this in two different ways:\n\
    a. Bookmark it.\n\
    b. Edit the database (see below) and prepend a plus sign (+) to the\n\
       corresponding entry.\n\
- Open/edit the jump database\n\
    je (also 'j --edit')\n\
- Open/edit the jump database using vim\n\
    j --edit vim\n\
- Purge the database of non-existent directories\n\
    j --purge\n\
  Note: To automatically purge the database of non-existent directories\n\
  at startup, set PurgeJumpDB to true in the configuration file.\n\
- Purge the database of entries ranked below 100\n\
    j --purge 100\n\
  Note: To remove a specific entry, delete the corresponding line\n\
  from the database ('je' or 'j --edit'). Note that if the directory\n\
  is in the directory history, it will not be removed from the database."

#define K_USAGE "Toggle follow-links (long view)\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  k\n\n\
When enabled, the long view displays information about the file a symbolic\n\
link points to, rather than the link itself."

#define KK_USAGE "Toggle max-filename-len\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  kk"

#define KB_USAGE "Manage keybindings\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  kb, keybinds [list | bind FUNC | edit [APP] | conflict | reset | readline]\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- List the current keybindings\n\
    kb (or 'kb list')\n\
- List available function names\n\
    kb bind <TAB>\n\
- Bind toggle-hidden to a key\n\
    kb bind toggle-hidden\n\
- Open/edit the keybindings file\n\
    kb edit\n\
- Open/edit the keybindings file using mousepad\n\
    kb edit mousepad\n\
- Unbind a function\n\
    Method 1: run 'kb bind FUNC' and bind it to '-'\n\
    Method 2: Run 'kb edit' and comment out the corresponding entry\n\
- Detect keybinding conflicts\n\
    kb conflict\n\
- List the current keybindings for readline specific functions\n\
    kb readline\n\
- Reset the keybindings file to its default state (keeping a backup of the\n\
  existing file)\n\
    kb reset"

#define LE_USAGE "Edit a symbolic link\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  le SYMLINK\n\n\
The user is prompted to enter a new link target using the current\n\
target as template.\n\n\
\x1b[1mEXAMPLE\x1b[22m\n\
- Edit the symbolic link named 'file.link'\n\
    le file.link"

#define LINK_USAGE "Create a symbolic link\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  l TARGET [LINK_NAME]\n\n\
If LINK_NAME is omitted, it is created as TARGET_BASENAME.link in\n\
the current directory.\n\n\
\x1b[1mEXAMPLE\x1b[22m\n\
- Create a symbolic link to 'file.zst' named 'mylink'\n\
    l file.zst mylink\n\n\
Note: The link creation mode (by default 'literal', like 'ln -s')\n\
can be set in the configuration file via the LinkCreationMode option\n\
(available modes are: absolute, literal, relative).\n\n\
Tip: Use the 'le' command to edit a symbolic link. Try 'le --help'."

#define LL_USAGE "Toggle the long view\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  ll, lv [on | off]\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- Switch to long view\n\
    ll on\n\
- Toggle long view\n\
    ll (or Alt+l)"

#define LM_USAGE "Turn the light mode on/off\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  lm [on, off]"

#define LOG_USAGE "Manage clifm logs\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  log [cmd | msg] [list | on | off | status | clear]\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- Print message logs\n\
    log msg list (or just 'log msg')\n\
- Enable command logs\n\
    log cmd on\n\
- Disable message logs\n\
    log msg off\n\
- Clear message logs\n\
    log msg clear"

#define MD_USAGE "Create directories\n\
(parent directories are created if they do not exist)\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  md DIR...\n\n\
\x1b[1mEXAMPLE\x1b[22m\n\
  md dir1 dir2 dir3/subdir\n\n\
Note: Use the 'n' command to create both files and directories.\n\
Try 'n --help' for more details."

#define MEDIA_USAGE "List, mount, and unmount storage devices\n\n\
Note: udevil(1) or udisks2(1) is required\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  media\n\n\
The list of mounted and unmounted devices will be displayed.\n\
Select a device using ELNs (Entry List Numbers).\n\n\
If the device is mounted, it will be unmounted, and, if unmounted,\n\
it will be mounted. Clifm will automatically change to the\n\
mountpoint after mounting.\n\n\
To get information about a device, enter 'i' followed by the ELN.\n\
For example: 'i12'."

#define MF_USAGE "Limit the number of listed files to NUM \
(valid range: >= 0). Use 'unset' to remove the file limit.\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  mf [NUM | unset]"

#define MIME_USAGE "Manage opening applications\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  mm, mime [open FILE | info FILE | edit [APP] | import]\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- Open the file 'book.pdf' with the default opening application\n\
    mm open book.pdf\n\
  Note: Since 'open' is the default action, it can be omitted: 'mm book.pdf'.\n\
  This command is the same as 'open book.pdf' or just 'book.pdf'.\n\
- Get MIME information for the file whose ELN is 12\n\
    mm info 12\n\
- Open/edit the MIME configuration file\n\
    mm edit (or F6)\n\
- Open/edit the MIME configuration file using vim\n\
    mm edit vim\n\
- Try to import MIME file associations from the system\n\
    mm import\n\
- Add/modify the default opening application for 'myfile'\n\
    1) Determine the MIME type (or filename) of the file\n\
      mm info myfile\n\
    2) Edit the mimelist file\n\
      mm edit (or F6)\n\
    Once in the file, find the appropriate entry and add/modify\n\
    the desired opening application.\n\
  For more information consult the manpage."

#define MSG_USAGE "List available program messages\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  msg, messages [clear]\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- List available messages\n\
    msg\n\
- Clear the current list of messages\n\
    msg clear (or Alt+t)"

#define MOUNTPOINTS_USAGE "List and change to a mountpoint\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  mp, mountpoints\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- List available mountpoints\n\
    mp (or Alt+m)\n\
  Once there, select the mountpoint you want to change to\n\
  (clifm will automatically change to the mountpoint)."

#define NET_USAGE "Manage remote resources\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  net [NAME] [list | edit [APP] | m, mount NAME | u, unmount NAME]\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- List available remote resources (like SSHFS or samba)\n\
    net (or 'net list')\n\
- Mount the remote resource named 'work'\n\
    net mount work (also 'net m work' or 'net m <TAB>')\n\
  Note: Since 'mount' is the default action, it can be omitted: 'net work'.\n\
- Unmount the remote resource named 'work'\n\
    net unmount work (or 'net u work' or 'net u <TAB>')\n\
- Open/edit the net configuration file\n\
    net edit\n\
- Open/edit the net configuration file using nano\n\
    net edit nano\n\
- Copy a file to a remote location using the 'cr' plugin\n\
    cr FILE (run 'cr --edit' before to set up your remotes)\n\n\
Note: This command can also be used to (un)mount storage devices, such as\n\
disk partitions or external drives. Run 'net edit' and consult the examples\n\
section for more information."

#define NEW_USAGE "Create new files and/or directories\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  n, new [FILE[@TEMPLATE]...] [DIR/...]\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- Create two files named 'file1' and 'file2'\n\
    n file1 file2\n\
- Create two directories named 'dir1' and 'dir2'\n\
    n dir1/ dir2/\n\
  Note: Be sure to include the ending slashes.\n\
- Create both files and directories at once:\n\
    n file1 file2 dir1/ dir2/\n\
- Create a file from a template (see below for more information)\n\
    n file1@my_template\n\n\
Parent directories will be created as needed. For example, if you run\n\
    n dir/subdir/file\n\
both the 'dir' and 'subdir' directories will be created if they do not\n\
already exist.\n\n\
\x1b[1mFILE TEMPLATES\x1b[22m\n\n\
\x1b[1m1. Automatic templates\x1b[22m\n\n\
New regular files can be created from a template file if:\n\n\
  a. The file to be created has a filename extension (e.g., 'file.html').\n\
  b. A file named like this extension (in this case 'html') exists in the\n\
     templates directory (1).\n\n\
If both conditions are satisfied, running 'n file.html' will create a new file\n\
named 'file.html' which is a copy of the 'html' file in the templates\n\
directory.\n\n\
Note that template names are not restricted to actual file extensions: you\n\
can name your templates as you wish (with any content you desire) as long as\n\
new files are created using the template name as the extension. E.g.:\n\
'n file.my_super_cool_template'.\n\n\
\x1b[1m2. Explicit templates\x1b[22m\n\n\
If a filename is followed by '@TEMPLATE', where TEMPLATE is any regular\n\
file found in the templates directory (1), the file will be created as a\n\
copy of the specified template. E.g.: 'n file.sh@my_script.sh'.\n\n\
Tab completion is available for explicit templates: simply type 'n file@<TAB>'.\n\n\
(1) The templates directory is $CLIFM_TEMPLATES_DIR, $XDG_TEMPLATES_DIR,\n\
or ~/Templates, in this precedence order."

#define OC_USAGE "Edit file ownership\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  oc FILE...\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- Change ownership of selected files\n\
    oc sel\n\
- Change ownership of all .iso files\n\
    oc *.iso\n\n\
\x1b[1mNOTES\x1b[22m\n\
A template is presented to the user to edit the ownership information.\n\n\
Only user and primary group common to all specified files are set in\n\
the ownership template.\n\n\
The new ownership (both user and primary group, if specified) will be\n\
applied to each specified file.\n\n\
Both names and ID numbers are allowed (tab completion is available).\n\n\
If only a name/number is entered, it is taken as user.\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- Change user to root\n\
    root (or \"0\")\n\
- Change primary group to video\n\
    :video (or \":981\")\n\
- Change user to peter and primary group to audio\n\
    peter:audio (or \"1000:986\" or \"peter:986\" or \"1000:audio\")\n\n\
Tip: Use the 'pc' command to edit file permissions."

#define OPEN_USAGE "Open a file\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  o, open FILE [APPLICATION]\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- Open the file whose ELN is 12 with the default associated application\n\
  (see the 'mime' command)\n\
    o 12\n\
- Open the file whose ELN is 12 with vi\n\
    o 12 vi\n\
  Note: If auto-open is enabled (default), 'o' can be omitted\n\
    12\n\
    12 vi"

#define OPENER_USAGE "Set the file opener\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  opener APPLICATION\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- Set the file opener to xdg-open (instead of the default, Lira)\n\
    opener xdg-open\n\
- Set the file opener back to the default\n\
    opener default"

#define OW_USAGE "Open a file using a specific application\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  ow FILE [APP]\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- Select the opening application for the file 'test.c' from a menu\n\
    ow test.c\n\
  or\n\
    'ow test.c <TAB>' (to get a list of available applications)\n\
  Note: Available applications are taken from the mimelist file (see the\n\
  'mime' command), and only valid and installed applications are listed.\n\
- Open the file 'test.c' with geany\n\
    ow test.c geany"

#define PAGER_USAGE "Toggle/run the file list pager\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  pg, pager [on | off | once | status | NUM]\n\n\
With no parameter, just run the pager.\n\n\
If set to 'on', run the pager whenever the list of files does no fit on\n\
the screen.\n\n\
If set to a positive integer greater than 1, run the pager whenever\n\
the number of files in the current directory is greater than or equal to\n\
that value (e.g., 1000). A value of 1 is equivalent to 'on', while 0 means 'off'.\n\n\
Set it to 'once' to run the pager only a single time.\n\n\
While paging, the following keys are available:\n\n\
?, h: Help\n\
Down arrow, Enter, Space: Advance one line\n\
Page down: Advance one page\n\
q: Stop paging (without printing remaining files)\n\
c: Stop paging (printing remaining files)\n\n\
Note: For upward scrolling, use the scrolling options available in your\n\
terminal emulator (e.g., mouse scrolling or specific keybindings)\n\n\
By default, the pager lists files using the current listing mode (long\n\
or short). You can specify a particular mode using the PagerView option in the\n\
configuration file or the --pager-view command line option. Possibles values:\n\n\
'auto': Use the current listing mode (default)\n\
'long': List files in long view\n\
'short': List files in short view\n\n\
Note: You can also try the 'gg' plugin (just enter 'gg')."

#define PC_USAGE "Edit file permissions\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  pc FILE...\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- Edit permissions of the file named 'file.txt'\n\
    pc file.txt\n\
- Edit permissions of all selected files at once\n\
    pc sel\n\n\
When editing multiple files with different permissions at once, only\n\
shared permission bits will be set in the permissions template.\n\n\
The new permissions set will be applied to each specified file.\n\n\
Both symbolic and octal notation for the new permissions set are\n\
allowed.\n\n\
Note: Use the 'oc' command to edit file ownership."

#define PIN_USAGE "Pin a file or directory\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  pin FILE/DIR\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- Pin the directory '~/my_important_dir'\n\
    pin ~/my_important_dir\n\
- Change to the pinned directory\n\
    , (yes, just a comma)\n\
- Unpin the currently pinned directory\n\
    unpin"

#define PROFILES_USAGE "Manage profiles\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  pf, profile [list | set, add, del PROFILE | rename PROFILE NEW_NAME]\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- Print the current profile name\n\
    pf\n\
- List available profiles\n\
    pf list\n\
- Switch to the profile 'myprofile'\n\
    pf set myprofile (or 'pf set <TAB>' to select from a list)\n\
- Add a new profile named new_profile\n\
    pf add new_profile\n\
- Remove the profile 'myprofile'\n\
    pf del myprofile (or 'pf del <TAB>' to select from a list)\n\
- Rename the profile 'myprofile' as 'cool_name'\n\
    pf rename myprofile cool_name"

#define PROMPT_USAGE "Change the current prompt\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  prompt [set NAME | list | unset | edit [APP] | reload]\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- List available prompts\n\
    prompt list (or 'prompt set <TAB>' to select from a list)\n\
- Change prompt to the prompt named MYPROMPT\n\
    prompt set MYPROMPT\n\
    Since 'set' is the default action, it can be omitted: 'prompt MYPROMPT'\n\
- Edit the prompts file\n\
    prompt edit\n\
- Edit the prompts file with vi\n\
    prompt edit vi\n\
- Set the default prompt\n\
    prompt unset\n\
- Reload available prompts\n\
    prompt reload\n\n\
Note: To permanently set a new prompt edit the current\n\
color scheme file ('cs edit'), and set the Prompt field to\n\
whatever prompt you like."

#define PROP_USAGE "Print file properties\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  p, pp, prop FILE...\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- Print the properties of the file whose ELN is 12\n\
    p 12 (or 'p <TAB>' to select from a list)\n\
- Print the properties of all selected files\n\
    p sel\n\
- Print the properties of the directory 'dir' (including recursive size)\n\
    pp dir\n\n\
Note that, in case of symbolic links to directories, the 'p' command displays\n\
information about the link target if the provided filename ends with a slash.\n\
Otherwise, information about the link itself is displayed.\n\
Unlike 'p', however, 'pp' always dereferences symbolic links."

#define PWD_DESC "Print the name of the current working directory\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  pwd [-L | -P]\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- Print the logical working directory (do not resolve symlinks)\n\
    pwd (or 'pwd -L')\n\
- Print the physical working directory (resolve all symlinks)\n\
    pwd -P"

#define QUIT_HELP "Exit Clifm\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  q, quit, exit\n\n\
To enable the cd-on-quit function consult the manpage."

#define RELOAD_USAGE "Reload the main configuration file and update settings accordingly\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  rl, reload (or 'config reload')"

#define RR_USAGE "Remove files in bulk using a text editor\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  rr [DIR] [:EDITOR]\n\n\
The list of files in DIR (current directory if omitted) is opened via\n\
EDITOR (default associated application for text files if omitted). Remove\n\
the lines corresponding to the files you want to delete, save, and quit\n\
the editor (quit without saving to cancel the operation).\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- Bulk remove files/dirs in the current directory using the default editor\n\
    rr\n\
- Bulk remove files/dirs in the current directory using nano\n\
    rr :nano\n\
- Bulk remove files/dirs in the directory 'mydir' using vi\n\
    rr mydir :vi"

#define SEARCH_USAGE "Search for files using either glob or regular expressions\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  /PATTERN [-filetype] [-x] [DIR]\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- List all PDF files in the current directory\n\
    /*.pdf (or, as a regular expression, '/.*\\.pdf$')\n\
- List all files starting with 'A' in the directory whose ELN is 7\n\
    /A* 7\n\
- List all .conf files in the directory '/etc'\n\
    /*.conf /etc\n\n\
You can further filter the search using a file type filter:\n\
  -b	block device\n\
  -c	character device\n\
  -d	directory\n\
  -f	regular file\n\
  -l	symlink\n\
  -p	FIFO/pipe\n\
  -s	socket\n\
- For example, to list all directories containing a dot or a dash and ending \
with 'd' in the directory named 'Documents'\n\
    /[.-].*d$ -d Documents/\n\n\
To perform a recursive search, use the -x modifier (file types not allowed)\n\
    /str -x /boot\n\n\
To search for files by content instead of names, use the rgfind plugin, bound\n\
by default to the \"//\" action name. For example:\n\
    // content I\\'m looking for\n\n\
Note: This plugin requires fzf and rg (ripgrep)."

#define SECURITY_USAGE "Clifm offers three distinct security mechanisms:\n\n\
1. Stealth Mode (Incognito/Private Mode): In this mode, no files are read\n\
from or written to the filesystem unless explicitly requested by the user\n\
through a command.\n\
Default values are used.\n\
You can enable this mode using the -S or --stealth-mode command line switch.\n\n\
2. Secure Environment: Clifm operates within a sanitized environment where most\n\
environment variables are cleared, and a select few are set to secure defaults.\n\
This mode can be activated with the --secure-env or --secure-env-full command\n\
line switches.\n\n\
3. Secure Commands: Automatically executed shell commands (such as autocommands,\n\
(un)mounting, and opening applications) are sanitized prior to execution. A\n\
secure environment is established, and commands are validated against a\n\
whitelist to prevent unexpected or insecure behavior, as well as command\n\
injection. To enable this mode, use the --secure-cmds command line switch."

#define SEL_USAGE "Select one or multiple files\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  s, sel FILE... [[!]PATTERN] [-FILETYPE] [:PATH]\n\n\
Recognized file types: (d)irectory, regular (f)ile, symbolic (l)ink,\n\
(s)ocket, fifo/(p)ipe, (b)lock device, (c)haracter device\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- Select the file whose ELN is 12\n\
    s 12 (or 's <TAB>' to select from a list - multi-selection is allowed)\n\
- Select all files ending with .odt:\n\
    s *.odt\n\
- Select multiple files at once\n\
    s 12 15-21 *.pdf\n\
- Select all regular files in the directory '/etc' starting with 'd'\n\
    s ^d.* -f :/etc\n\
- Select all files in the current directory (including hidden files)\n\
    s * .* (or Alt+a)\n\
- Interactively select files in '/media' (requires fzf, fnf, or smenu\n\
  tab completion modes)\n\
    s /media/*<TAB>\n\
- List currently selected files\n\
    sb\n\
- Copy selected files to the current directory:\n\
    c sel\n\
- Move selected files to the directory whose ELN is 24\n\
    m sel 24\n\
- Deselect all files\n\
    ds * (or Alt+d)\n\
- Deselect files selectively\n\
    ds <TAB> (multi-selection is allowed)"

#define SORT_USAGE "Change file sort preferences\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  st [METHOD] [rev]\n\n\
Available methods: 0=none, 1=name, 2=size, 3=atime, 4=btime,\n\
5=ctime, 6=mtime, 7=version, 8=extension, 9=inode, 10=owner,\n\
11=group, 12=blocks, 13=links, and 14=type.\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- List files by size\n\
    st size (or 'st <TAB>' to select from a list)\n\
    Numbers can be used as well (e.g. 'st 2')\n\
- Revert the current sort order (e.g. z-a instead of a-z)\n\
    st rev\n\
- Both of the above at once\n\
    st size rev\n\n\
Tip: Take a look at the configuration file for extra sort\n\
options (ListDirsFirst, PrioritySortChar, ShowHiddenFiles)."

#define TAG_USAGE "(Un)tag files and/or directories\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  tag [add | del | list | list-full | merge | new | rename | untag]\n\
      [FILE...] [[:]TAG]\n\n\
Instead of the long format described above, you can use any of the\n\
following shortcuts as well:\n\n\
  ta: Tag files as ...       (same as 'tag add')\n\
  td: Delete tag(s)          (same as 'tag del')\n\
  tl: List tags/tagged files (same as 'tag list')\n\
  tm: Rename tag             (same as 'tag rename')\n\
  tn: Create new tag(s)      (same as 'tag new')\n\
  tu: Untag file(s)          (same as 'tag untag')\n\
  ty: Merge two tags         (same as 'tag merge')\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- List available tags\n\
    tag list (or 't:<TAB>')\n\
- List available tags and each tagged file\n\
    tag list-full\n\
- List files tagged as 'pdf'\n\
    tag list pdf (or 't:pdf<TAB>')'\n\
- List tags applied to the file 'file.txt'\n\
    tag list file.txt\n\
- Tag all .PNG files in the current directory as both 'images' and 'png'\n\
    tag add *.png :images :png\n\
  Note: Tags are created if they do not exist.\n\
  Note 2: Since 'add' is the default action, it can be omitted.\n\
- Tag all selected files as 'special'\n\
    tag add sel :special\n\
- Rename tag 'documents' to 'docs'\n\
    tag rename documents docs\n\
- Merge tag 'png' into 'images'\n\
    tag merge png images\n\
  Note: All files tagged as 'png' will be now tagged as 'images',\n\
  and the 'png' tag will be removed.\n\
- Remove the tag 'images' (untag all files tagged as 'images')\n\
    tag del images\n\
- Untag a few files from the 'work' tag\n\
    tag untag :work file1 image.png dir2\n\
  or\n\
    tag untag :<TAB> (and then TAB again to select tagged files)\n\n\
Operating on tagged files (t:TAG)\n\
- Print the file properties of all files tagged as 'docs'\n\
    p t:docs (or 'p t:<TAB>' to select from a list)\n\
- Remove all files tagged as 'images'\n\
    r t:images\n\
- Run stat(1) over all files tagged as 'work' and all files tagged as\n\
  'docs'\n\
    stat t:work t:docs\n\n\
To operate only on some tagged files use TAB as follows:\n\
    t:TAG<TAB> (multi-selection is allowed)\n\
Mark the files you need via TAB and then press Enter or Right."

#define TE_USAGE "Toggle the executable bit on files\n\n\
\x1b[1mUSAGE\x1b[22m\n\
    te FILE...\n\
  or\n\
    'te <TAB>' to select from a list (multi-selection is allowed)\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- Set the executable bit on all shell scripts in the current directory\n\
    te *.sh\n\
- Set the executable bit on all selected files\n\
   te sel"

#define TRASH_USAGE "Move files to the trash can\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  t, trash [FILE... | del [FILE]... | empty | list]\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- Trash the file whose ELN is 12\n\
    t 12\n\
  or\n\
    't <TAB>' to select from a list (multi-selection is allowed)\n\
- Trash all files ending with .sh\n\
    t *.sh\n\
- List currently trashed files\n\
    t (or 't list', or 't <TAB>')\n\
- Remove/delete trashed files using a menu (permanent removal)\n\
    t del (or 't del <TAB>')\n\
- Remove/delete all files from the trash can (permanent removal)\n\
    t empty\n\
- Restore all trashed files (to their original location)\n\
    u *\n\
- Restore trashed files selectively using a menu\n\
    u\n\
  or\n\
    'u <TAB>' to select from a list (multi-selection is allowed)\n\n\
Note: For more information try 'u --help'."

#define UMASK_USAGE "Get/set the file mode creation mask\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  umask [MODE]\n\n\
Note: MODE is an octal value from 000 to 777.\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- Get the current value\n\
    umask\n\
- Set mask to 077\n\
    umask 077\n\n\
Note: To permanently set the umask, use the Umask option in the config file."

#define UNSET_USAGE "Delete variables from the environment\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  unset NAME..."

#define UNTRASH_USAGE "Restore files from the trash can\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  u, untrash [FILE... | *, a, all]\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- Restore all trashed files (to their original location)\n\
    u *\n\
- Restore trashed files selectively using a menu\n\
    u\n\
  or\n\
    'u <TAB>' to select from a list (multi-selection is allowed)\n\n\
Note: Use the 'trash' command to trash files. Try 'trash --help'."

#define VV_USAGE "Copy files to a directory and bulk rename them at once\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  vv FILE... DIR\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- Copy selected files to 'mydir' and rename them\n\
    vv sel mydir\n\
- Copy all PDF files to the directory whose ELN is 4 and rename them\n\
    vv *.pdf 4\n\n\
Note: If DIR does not exist, it will be created."

#define VIEW_USAGE "Preview files in the current directory (requires fzf)\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  view [edit [APP] | purge]\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- Just run the previewer\n\
    view (or Alt+-)\n\
  Note: Select files using the TAB key.\n\
- Edit the configuration file\n\
    view edit (or F7)\n\
- Edit the configuration file using vi\n\
    view edit vi\n\
- Purge the thumbnails directory(1) of dangling thumbnails\n\
    view purge\n\
- Add/modify the default previewing application for 'myfile'\n\
    1) Determine the MIME type (or filename) of the file\n\
      mm info myfile\n\
    2) Edit the mimelist file\n\
      view edit (or F7)\n\
    Once in the file, find the appropriate entry and add/modify\n\
    the desired previewing application.\n\
  For more information consult the manpage.\n\n\
(1) $XDG_CACHE_HOME/clifm/thumbnails\n\n\
Enter 'help file-previews' for more information."

#define WRAPPERS_USAGE "c, m, and r commands are wrappers for \
cp(1), mv(1), and rm(1) shell\ncommands, respectively.\n\n\
\x1b[1mUSAGE\x1b[22m\n\
c  -> cp -iRp\n\
m  -> mv -i\n\
r  -> rm ('rm -r' for directories) (1)\n\n\
(1) By default, the user will asked for confirmation (set rmForce to true\n\
in the configuration file to disable the confirmation prompt).\n\n\
The 'paste' command is equivalent to 'c' and exists only for semantic\n\
reasons: if you want to copy selected files to the current directory, it\n\
makes sense to write 'paste sel'.\n\n\
By default, both the 'c' and 'm' commands run cp(1)/mv(1) interactively\n\
(-i), i.e. prompting before overwriting a file. To run non-interactively\n\
instead, use the -f, --force parameter (see the examples below). You can\n\
also permanently run in non-interactive mode using the cpCmd/mvCmd options\n\
in the configuration file ('config' or F10).\n\n\
Just as 'c' and 'm', the 'r' command accepts -f, --force as paramater to\n\
prevent 'r' from prompting before removals. Set rmForce to true in the\n\
configuration file to make this option permanent.\n\n\
To use different parameters, run the corresponding utility as usual.\n\
For example: cp -abf ...\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- Create a copy of 'file1' named 'file2'\n\
    c file1 file2\n\
- Create a copy of 'file1' in the directory 'dir1' named 'file2'\n\
    c file1 dir1/file2\n\
- Copy all selected files to the current directory\n\
    c sel\n\
  Note: If destination directory is omitted, the current directory is assumed.\n\
- Copy all selected files to the current directory (non-interactively):\n\
    c -f sel\n\
- Move all selected files to the directory named 'testdir'\n\
    m sel testdir\n\
- Rename 'file1' as 'file_test'\n\
    m file1 file_test\n\
- Interactively rename 'file1'\n\
    m file1\n\
  Note: The user is prompted to enter a new name using the old name as\n\
  template.\n\
- Move all selected files to the current directory (non-interactively)\n\
    m -f sel\n\
- Remove all selected files\n\
    r sel\n\
- Remove all selected files (non-interactively)\n\
    r -f sel\n\
  Note: Use the 't' command to move files to the trash can. Try 't --help'.\n\n\
To create files and directories, you can use the 'md' and 'n' commands.\n\
Try 'md --help' and 'n --help' for more details.\n\n\
Use the 'vv' command to copy files to a directory and bulk rename them\n\
at once. Try 'vv --help'.\n\n\
Use the 'cr' plugin to copy a file to a remote location:\n\
    cr FILE (run 'cr --edit' before to set up your remotes)\n\n\
Use the 'l' command to create symbolic links, and 'le' to edit them."

#define WS_USAGE "Switch workspaces\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  ws [NUM/NAME [unset] | + | -]\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- List available workspaces\n\
    ws (or 'ws <TAB>')\n\
- Switch to the first workspace\n\
    ws 1 (or Alt+1)\n\
- Switch to the workspace named 'main'\n\
    ws main\n\
- Switch to the next workspace\n\
    ws +\n\
- Switch to the previous workspace\n\
    ws -\n\
- Unset the workspace number 2\n\
    ws 2 unset\n\n\
Note: Use the WorkspaceNames option in the configuration file to name\n\
your workspaces."

#define X_USAGE "Launch a new instance of Clifm in a new terminal window\n\n\
\x1b[1mUSAGE\x1b[22m\n\
  x, X [DIR]\n\n\
\x1b[1mEXAMPLES\x1b[22m\n\
- Launch a new instance in the current directory\n\
    x\n\
- Open the directory 'mydir' in a new instance\n\
    x mydir\n\
- Launch a new instance as root\n\
    X\n\n\
Note: By default, xterm is used. Set your preferred terminal\n\
emulator using the TerminalCmd option in the configuration file."

/* Misc messages */
#define PAGER_HELP "?, h: help\nDown arrow, Enter, Space: Advance one line\n\
Page Down: Advance one page\nq: Stop paging (without printing remaining files)\n\
c: Stop paging (printing remaining files)\n"
#define PAGER_LABEL "\x1b[7m--Mas-- (press 'h' for help)\x1b[0m"
#define NOT_AVAILABLE "This feature has been disabled at compile time"
#define STEALTH_DISABLED "Access to configuration files is not allowed in stealth mode"
#define CONFIG_FILE_UPDATED "File modified. Settings updated.\n"

#ifndef __HAIKU__
# define HELP_MESSAGE "Enter '?' or press F1-F3 for help"
#else
# define HELP_MESSAGE "Enter '?' or press F5-F7 for help"
#endif /* !__HAIKU__ */

#ifdef _BE_POSIX
#define OPTIONS_LIST "\
\n -a[VAL]  List hidden files ('first', 'last', 'true', or 'false')\
\n -A       Do not list hidden files\
\n -b FILE  Set an alternative bookmarks file\
\n -B NUM   Set TAB-completion mode (NUM=[0-3])\
\n -c FILE  Set an alternative main configuration file\
\n -C       Do not clear the screen when changing directories\
\n -d       Print disk usage (free/total)\
\n -D       List directories only\
\n -e       Force the use of the 'o/open' command to open files\
\n -E       Force the use of 'cd' to change directories\
\n -f       Display recursive directories sizes (long view only)\
\n -F       Disable the file counter (directories)\
\n -g       Display file sizes in SI units (powers of 1000) instead of 1024\
\n -G       Display physical file sizes (disk usage) instead of logical sizes (apparent size)\
\n -h       Print this help and exit\
\n -H       Disable syntax-highlighting\
\n -i       Enable icons\
\n -I DIR   Set an alternative trash directory\
\n -j FILE  Run the 'p' command on FILE and exit\
\n -J FILE  Run the 'pp' command on FILE and exit\
\n -k FILE  Set an alternative keybindings file\
\n -l       Display extended file metadata (long view)\
\n -L       Follow symbolic links when running in long view\
\n -m       Enable fuzzy matching\
\n -M       Disable colors\
\n -n       Disable the command history\
\n -N       Disable bold colors\
\n -o PATH  Set a custom file opener (instead of the builtin Lira)\
\n -O FILE  Open FILE (via Lira) and exit\
\n -p NAME  Set/create the profile NAME\
\n -P FILE  Generate a preview of FILE and exit\
\n -q       List files and quit\
\n -Q       Enable cd-on-quit (consult the manpage)\
\n -r       Files removed via the 'r' command are sent to the trash can\
\n -R       Do not append file type indicators\
\n -s       Run in stealth mode (incognito)\
\n -S       Disable suggestions\
\n -t NAME  Use the color scheme NAME\
\n -T       Do not truncate filenames\
\n -u       Run in disk usage analyzer mode\
\n -U       Disable Unicode decorations\
\n -v       Print version information and exit\
\n -V DIR   Set a custom virtual directory\
\n -w NUM   Start in workspace NUM (1-8)\
\n -W       Keep the list of selected files in sight\
\n -x       Run in secure-environment mode\
\n -X       Run in secure-environment mode (paranoid)\
\n -y       Run in light mode\
\n -Y       Run in secure-commands mode\
\n -z NUM   Set a file sort order (1-13)\
\n -Z NUM   List only up to NUM files"
#else
#define SHORT_OPTIONS "\
\n  -a, --show-hidden[=VAL]\t Show hidden files ('first', 'last', 'true', or 'false')\
\n  -A, --no-hidden\t\t Do not show hidden files\
\n  -b, --bookmarks-file=FILE\t Set an alternative bookmarks file\
\n  -c, --config-file=FILE\t Set an alternative configuration file\
\n  -D, --config-dir=DIR\t\t Set an alternative configuration directory\
\n  -e, --no-eln\t\t\t Do not print ELNs (Entry List Numbers)\
\n  -E, --eln-use-workspace-color\t ELNs use the current workspace color\
\n  -f, --dirs-first\t\t List directories first (default)\
\n  -F, --no-dirs-first\t\t Do not list directories first\
\n  -g, --pager\t\t\t Enable the pager\
\n  -G, --no-pager\t\t Disable the pager (default)\
\n  -h, --help\t\t\t Show this help and exit\
\n  -H, --horizontal-list\t\t List files horizontally\
\n  -i, --no-case-sensitive\t Ignore case distinctions when listing files (default)\
\n  -I, --case-sensitive\t\t Do not ignore case distinctions when listing files\
\n  -k, --keybindings-file=FILE\t Set an alternative keybindings file\
\n  -l, --long-view\t\t Display extended file metadata (long view)\
\n  -L, --follow-symlinks-long\t Follow symbolic links when running in long view\
\n  -m, --dirhist-map\t\t Enable the directory history map\
\n  -o, --autols\t\t\t List files automatically (default)\
\n  -O, --no-autols\t\t Do not list files automatically\
\n  -P, --profile=NAME\t\t Use (or create) NAME as profile\
\n  -r, --no-refresh-on-empty-line Do not refresh the list of files when pressing Enter \
on an empty line\
\n  -s, --splash\t\t\t Enable the splash screen\
\n  -S, --stealth-mode\t\t Run in incognito/private mode\
\n  -t, --disk-usage-analyzer\t Run in disk usage analyzer mode\
\n  -T, --trash-dir=DIR\t\t Set an alternative trash directory\
\n  -v, --version\t\t\t Show version details and exit\
\n  -w, --workspace=NUM\t\t Start in the workspace NUM\
\n  -x, --no-ext-cmds\t\t Disallow the use of external commands\
\n  -y, --light-mode\t\t Run in light mode\
\n  -z, --sort=METHOD\t\t Sort files by METHOD (for available methods see the manpage)"

#define LONG_OPTIONS_A "\
\n      --bell=STYLE\t\t Set terminal bell style to: 0 (none), 1 (audible), 2 (visible), 3 (flash)\
\n      --case-sens-dirjump\t Do not ignore case when consulting the jump \
database (via the 'j' command)\
\n      --case-sens-path-comp\t Enable case sensitive path completion\
\n      --cd-on-quit\t\t Enable cd-on-quit functionality (see the manpage)\
\n      --color-scheme=NAME\t Use the color scheme NAME\
\n      --color-links-as-target\t Colorize symbolic links according to the target file\
\n      --cwd-in-title\t\t Print the current directory in the terminal window title\
\n      --data-dir=PATH\t\t Use PATH as data directory (e.g.: /usr/local/share)\
\n      --desktop-notifications\t Set the desktop notifications style: 'kitty', 'system', or 'false' (default)\
\n      --disk-usage\t\t Show disk usage (FREE/TOTAL (FREE %) TYPE DEVICE)\
\n      --full-dir-size\t\t Display recursive directory sizes (long view only)\
\n      --fuzzy-algo=NUM\t\t Set fuzzy algorithm for fuzzy matching (1 or 2)\
\n      --fuzzy-matching\t\t Enable fuzzy tab completion/suggestions for filenames \
and paths\
\n      --fzfpreview-hidden\t Enable file previews for tab completion (fzf mode only) with the preview window hidden (toggle with Alt+p)\
\n      --fzftab\t\t\t Use fzf to display completion matches (default if the fzf binary is found in PATH)\
\n      --fnftab\t\t\t Use fnf to display completion matches\
\n      --icons\t\t\t Enable icons\
\n      --icons-use-file-color\t Icon colors follow file colors\
\n      --int-vars\t\t Enable the use internal variables\
\n      --kitty-keys\t\t Ask the terminal to enable the kitty keyboard protocol\
\n      --list-and-quit\t\t List files and quit\
\n      --ls\t\t\t Short for --list-and-quit\
\n      --lscolors\t\t Read file colors from LS_COLORS or LSCOLORS (FreeBSD style)\
\n      --max-dirhist=NUM\t\t Maximum number of visited directories to recall\
\n      --max-files=NUM\t\t List only up to NUM files\
\n      --mimelist-file=FILE\t Set FILE as Lira's configuration file\
\n      --mnt-udisks2\t\t Use udisks2(1) instead of udevil(1) for the 'media' command\
\n      --no-bold\t\t\t Disable bold colors (applies to all color schemes)\
\n      --no-cd-auto\t\t Disable the autocd function\
\n      --no-classify\t\t Do not append file type indicators\
\n      --no-clear-screen\t\t Do not clear the screen when listing files\
\n      --no-color\t\t Disable colors \
\n      --no-columns\t\t Disable columned file listing\
\n      --no-file-cap\t\t Do not check file capabilities when listing files\
\n      --no-file-ext\t\t Do not check file extensions when listing files\
\n      --no-file-counter\t\t Disable the file counter for directories\
\n      --no-follow-symlink\t Do not follow symbolic links when listing files (overrides -L and --color-links-as-target)\
\n      --no-fzfpreview\t\t Disable file previews for tab completion (fzf mode only)\
\n      --no-highlight\t\t Disable syntax highlighting\
\n      --no-history\t\t Do not write commands to the history file\
\n      --no-open-auto\t\t Same as no-cd-auto, but for files\
\n      --no-refresh-on-resize\t Do not attempt to refresh the file list upon window's resize\
\n      --no-restore-last-path\t Do not record the last visited directory\
\n      --no-suggestions\t\t Disable auto-suggestions\
\n      --no-tips\t\t\t Disable startup tips\
\n      --no-truncate-names\t Do not truncate filenames\
\n      --no-unicode\t\t Disable Unicode decorations\
\n      --no-warning-prompt\t Disable the warning prompt\
\n      --no-welcome-message\t Disable the welcome message\
\n      --only-dirs\t\t List only directories and symbolic links to directories\
\n      --open=FILE\t\t Open FILE (using Lira) and exit\
\n      --opener=APPLICATION\t Use APPLICATION as file opener (instead of Lira, \
the builtin opener)\
\n      --pager-view=MODE\t\t How to list files in the pager (auto, long, short)\
\n      --physical-size\t\t Display physical file sizes (disk usage) rather than logical sizes (apparent size)\
\n      --preview=FILE\t\t Display a preview of FILE (via Shotgun) and exit\
\n      --print-sel\t\t Keep the list of selected files in sight\n"

#define LONG_OPTIONS_B "\
      --prop-fields=FORMAT\t Set a custom format string for the long view (see \
PropFields in the config file)\
\n      --ptime-style=STYLE\t Time/date style used by the 'p/pp' command (see PTimeStyle in the config file)\
\n      --readonly\t\t Disable internal commands able to modify the filesystem\
\n      --report-cwd\t\t Report the current directory to the terminal (OSC-7)\
\n      --rl-vi-mode\t\t Set readline to vi editing mode (defaults to emacs mode)\
\n      --secure-cmds\t\t Sanitize commands to prevent command injection\
\n      --secure-env\t\t Run in a sanitized environment (regular mode)\
\n      --secure-env-full\t\t Run in a sanitized environment (full mode)\
\n      --sel-file=FILE\t\t Set FILE as custom selections file\
\n      --share-selbox\t\t Make the Selection Box common to different profiles\
\n      --shotgun-file=FILE\t Set FILE as shotgun's configuration file\
\n      --si\t\t\t Display file sizes in SI units (powers of 1000) instead of 1024\
\n      --smenutab\t\t Use smenu to display completion matches\
\n      --sort-reverse\t\t Sort in reverse order, e.g., z-a instead of a-z\
\n      --stat FILE...\t\t Run the 'p' command on FILE(s) and exit\
\n      --stat-full FILE...\t Run the 'pp' command on FILE(s) and exit\
\n      --stdtab\t\t\t Force the use of the standard tab completion mode (readline)\
\n      --time-style=STYLE\t Time/date style used in long view (see TimeStyle in the config file)\
\n      --trash-as-rm\t\t The 'r' command moves files to the trash instead of removing them\
\n      --unicode\t\t\t Force the use of Unicode decorations\
\n      --virtual-dir-full-paths\t Files in virtual directories are listed as full paths instead of target base names\
\n      --virtual-dir=PATH\t Absolute path to a directory to be used as virtual directory\
\n      --vt100\t\t\t Run in vt100 compatibility mode\n"
#endif /* _BE_POSIX */

#define CLIFM_COMMANDS_HEADER "\
For a complete description of the below \
commands run 'cmd' (or press F2) or consult the manpage (F1).\n\
You can also try the interactive help plugin (it depends on FZF): just \
enter 'ih', that's it.\n\
Help topics are available as well. Type 'help <TAB>' to get a list of topics.\n\n\
The following is just a list of available commands and a brief description.\n\
For more information about a specific command run 'CMD -h' or 'CMD --help'.\n"

#define CLIFM_COMMANDS "\
 ELN/FILE/DIR       Auto-open/autocd files/directories\n\
 /PATTERN           Search for files\n\
 ;[CMD], :[CMD]     Run CMD using the system shell\n\
 ac, ad             (De)archive files\n\
 acd, autocd        Turn auto-cd on/off\n\
 actions            Manage actions/plugins\n\
 alias              Manage aliases\n\
 ao, auto-open      Turn auto-open on/off\n\
 auto               Set an autocommand for the current directory\n\
 b, back            Change to the previously visited directory\n\
 bb, bleach         Sanitize non-ASCII filenames\n\
 bd                 Change to a parent directory\n\
 bl                 Create symbolic links in bulk\n\
 bm, bookmarks      Manage bookmarks\n\
 br, bulk           Rename files in bulk\n\
 c, l, m, md, r     Copy, link, move, makedir, and remove\n\
 colors             Preview the current color scheme\n\
 cd                 Change directory\n\
 cl, columns        Toggle columns\n\
 cmd, commands      Jump to the COMMANDS section in the manpage\n\
 config             Open/edit the main configuration file\n\
 cs, colorscheme    Switch/edit color schemes\n\
 d, dup             Duplicate files\n\
 dh                 Access the directory history list\n\
 ds, desel          Deselect files\n\
 exp                Export filenames to a temporary file\n\
 ext                Turn external/shell commands on/off\n\
 f, forth           Change to the next visited directory\n\
 fc                 Toggle the file-counter\n\
 ff, dirs-first     Toggle list-directories-first\n\
 ft, filter         Set a file filter\n\
 fz                 Display recursive directory sizes (long view only)\n\
 hh, hidden         Toggle hidden files\n\
 history            Manage the commands history\n\
 icons              Turn icons on/off\n\
 k                  Toggle follow-links (long view only)\n\
 kk                 Toggle max-filename-len\n\
 j                  Jump to a visited directory\n\
 kb, keybinds       Manage keybindings\n\
 le                 Edit symbolic link target\n\
 ll, lv             Toggle the long-view\n\
 lm                 Toggle the light-mode\n\
 log                Manage your logs\n\
 media              (Un)mount storage devices\n\
 mf                 Limit the number of listed files\n\
 mm, mime           Manage default opening applications\n\
 mp, mountpoints    Change to a mountpoint\n\
 msg, messages      Print system messages\n\
 n, new             Create new files/directories\n\
 net                Manage network/remote resources\n\
 o, open            Open a file\n\
 oc                 Change ownership of files interactively\n\
 ow                 Open a file with ...\n\
 opener             Set a custom file opener\n\
 p, pp, prop        Print file properties\n\
 pc                 Change permissions of files interactively\n\
 pf, profile        Manage profiles\n\
 pg, pager          Turn the file pager on/off\n\
 pin                Pin a directory\n\
 prompt             Switch/edit the prompt\n\
 q, quit, exit      Quit Clifm\n\
 rf, refresh        Refresh/clear the screen\n\
 rl, reload         Reload the main configuration file\n\
 rr                 Remove files in bulk\n\
 s, sel             Select files\n\
 sb, selbox         Access the Selection Box\n\
 st, sort           Change file sort order\n\
 stats              Print file statistics\n\
 t, trash           Move files to the trash can\n\
 tag                Tag files\n\
 te                 Toggle the executable bit on files\n\
 tips               Print tips\n\
 u, untrash         Restore trashed files (using a menu)\n\
 unpin              Unpin the pinned directory\n\
 vv                 Copy and rename files at once\n\
 ver, version       Print version information\n\
 view               Preview files in the current directory\n\
 ws                 Switch workspaces\n\
 x, X               Launch a new instance of Clifm (as root if 'X')\n\n\
 Shell-builtin implementations\n\
 export             Export variables to the environment\n\
 pwd                Print the current working directory\n\
 umask              Get/set umask\n\
 unset              Remove variables from the environment\n"

#define CLIFM_KEYBOARD_SHORTCUTS "DEFAULT KEYBOARD SHORTCUTS:\n\n\
 Right, Ctrl+f      Accept the entire suggestion\n\
 Alt+Right, Alt+f   Accept the first suggested word\n\
 Alt+c              Clear the current command line buffer\n\
 Alt+q              Delete the last entered word\n\
 Alt+g              Toggle list-directories-first\n\
 Alt+l              Toggle long-view\n\
 Alt++              Toggle follow-links (long view only)\n\
 Alt+.              Toggle hidden-files\n\
 Alt+,              Toggle list-only-directories\n\
 Alt+-              Preview files in the current directory (requires fzf)\n\
 Alt+m              List mountpoints\n\
 Alt+h              Show directory history\n\
 Alt+t              Clear messages\n\
 Ctrl+l             Clear the screen\n\
 Ctrl+y             Copy the current line buffer to the clipboard\n\
 Alt+s              Open the Selection Box\n\
 Alt+a              Select all files in the current directory\n\
 Alt+d              Deselect all files\n\
 Alt+r              Change to the root directory\n\
 Alt+e, Home        Change to the home directory\n\
 Alt+u, Shift+Up    Change to the parent directory\n\
 Alt+j, Shift+Left  Change to previously visited directory\n\
 Alt+k, Shift+Right Change to next visited directory\n\
 Alt+p              Change to the pinned directory\n\
 Alt+v              Toggle prepend sudo\n\
 Alt+0              Run the file pager\n\
 Alt+[1-4]          Switch to workspace 1-4\n\
 Ctrl+Alt+o         Switch to previous profile\n\
 Ctrl+Alt+p         Switch to next profile\n\
 Ctrl+Alt+a         Archive selected files\n\
 Ctrl+Alt+e         Export selected files\n\
 Ctrl+Alt+r         Rename selected files\n\
 Ctrl+Alt+d         Remove selected files\n\
 Ctrl+Alt+t         Trash selected files\n\
 Ctrl+Alt+n         Move selected files to the current directory\n\
 Ctrl+Alt+v         Copy selected files to the current directory\n\
 Ctrl+Alt+l         Toggle max-name-length\n\
 Alt+y              Toggle light-mode\n\
 Alt+z              Switch to the previous sort method\n\
 Alt+x              Switch to the next sort method\n\
 Ctrl+Alt+x         Launch a new instance\n\
 F1                 Manual page\n\
 F2                 Commands help\n\
 F3                 Keybindings help\n\
 F6                 Open the mimelist file\n\
 F7                 Open the shotgun configuration file\n\
 F8                 Open the current color scheme file\n\
 F9                 Open the keybindings file\n\
 F10                Open the configuration file\n\
 F11                Open the bookmarks file\n\
 F12                Quit\n"

#define HELP_END_NOTE "For a full description consult the manpage and/or the \
Wiki (https://github.com/leo-arch/clifm/wiki)."

#define ASCII_LOGO "\
                            _______     _ \n\
                           | ,---, |   | |\n\
                           | |   | |   | |\n\
                           | |   | |   | |\n\
                           | |   | |   | |\n\
                           | !___! !___! |\n\
                           `-------------'\n"

#define QUICK_HELP_HEADER "\
This is only a quick start guide. For more information and advanced tricks \n\
consult the manpage and/or the Wiki (https://github.com/leo-arch/clifm/wiki)\n\
For a brief description of available commands type 'cmd<TAB>'\n\
Help topics are also available: 'help <TAB>'"

#define QUICK_HELP_NAVIGATION "\
NAVIGATION\n\
----------\n\
/etc                     Change the current directory to '/etc'\n\
5                        Change to the directory whose ELN is 5\n\
b | Shift+left | Alt+j   Change to the previously visited directory\n\
f | Shift+right | Alt+k  Change to the next visited directory\n\
.. | Shift+up | Alt+u    Change to the parent directory\n\
bd media                 Change to the parent directory matching 'media'\n\
j <TAB> | dh <TAB>       Browse the directory history list\n\
j xproj                  Jump to the best ranked directory matching 'xproj'\n\
bm | b:<TAB> | Alt+b     List bookmarks\n\
bm mybm | b:mybm         Change to the bookmark named 'mybm'\n\
ws2 | Alt+2              Switch to the second workspace\n\
mp                       Change to a mountpoint\n\
pin mydir                Pin the directory 'mydir'\n\
,                        Change to the pinned directory\n\
x                        Run new instance in the current directory\n\
/*.pdf<TAB>              File-name filter: List all PDF files in the current directory\n\
=x<TAB>                  File-type filter: List all executable files in the current directory (1)\n\
@gzip<TAB>               MIME-type filter: List all gzip files in the current directory (1)\n\
view | Alt+-             Preview files in the current directory (requires fzf)\n\n\
(1) Run 'help file-filters' for more information"

#define QUICK_HELP_BASIC_OPERATIONS "\
BASIC FILE OPERATIONS\n\
---------------------\n\
myfile.txt           Open 'myfile.txt' with the default associated application\n\
myfile.txt vi        Open 'myfile.txt' with vi (also 'vi myfile.txt')\n\
12                   Open the file whose ELN is 12\n\
12&                  Open the file whose ELN is 12 in the background\n\
ow 10 | ow 10 <TAB>  Select opening application for the file whose ELN is 10\n\
p 4                  Print file properties of the file whose ELN is 4\n\
/*.png               Search for files ending with .png in the current directory\n\
s *.c                Select all C files\n\
s 1-4 8 19-26        Select multiple files by ELN\n\
sb | s:<TAB>         List currently selected files\n\
ds | ds <TAB>        Deselect a few files\n\
ds * | Alt+d         Deselect all files\n\
bm add mydir mybm    Bookmark the directory named 'mydir' as 'mybm'\n\
bm del mybm          Remove the bookmark named 'mybm'\n\
tag --help           Learn about file tags\n\
n myfile             Create a new file named 'myfile'\n\
n mydir/             Create a new directory named 'mydir'\n\
c sel                Copy selected files to the current directory (1)\n\
r sel                Remove all selected files (1)\n\
br sel               Bulk rename selected files (1)\n\
c 34 file_copy       Copy the file whose ELN is 34 to 'file_copy'\n\
cr myfile            Copy 'myfile' to a remote location\n\
m 45 3               Move the file whose ELN is 45 to the dir whose ELN is 3\n\
m myfile.txt         Interactively rename 'myfile.txt'\n\
l myfile mylink      Create a symbolic link named 'mylink' pointing to 'myfile'\n\
le mylink            Edit the symbolic link 'mylink'\n\
oc myfile            Edit ownership of the file 'myfile'\n\
pc myfile            Edit permissions of the file 'myfile'\n\
te *.sh              Toggle the executable bit on all .sh files\n\
t 12-18              Move files whose ELNs are 12-18 to the trash can\n\
t del | t del <TAB>  Permanently remove trashed files using a menu\n\
t empty              Empty the trash can\n\
u | u <TAB>          Untrash files using a menu\n\
ac sel               Compress/archive selected files (1)\n\n\
(1) 's:' can be used instead of the 'sel' keyword"

#define QUICK_HELP_MISC "\
MISC\n\
----\n\
CMD --help      Get help for the command CMD\n\
help <TAB>      List available help topics\n\
F1              Open the manpage\n\
ih              Run the interactive help plugin (requires fzf)\n\
ll | Alt+l      Toggle long view\n\
hh | Alt+.      Toggle hidden files\n\
rf | Ctrl+l     Clear the screen (also Enter on empty line)\n\
pg | Alt+0      Run the pager (builtin)\n\
gg              Run the pager (plugin)\n\
auto            Add an autocommand to the current directory\n\
config | F10    View/edit the configuration file\n\
mm edit | F6    Change default opening applications\n\
view edit | F7  Change default previewing applications\n\
kb bind <TAB>   List available functions for binding\n\
kb bind FUNC    Bind FUNC to a new key sequence\n\
kb edit | F9    Manually edit the keybindings file\n\
mm info 12      Get MIME information for the file whose ELN is 12\n\
Alt+Tab         Toggle the disk usage analyzer mode\n\
cs              Manage color schemes\n\
Right           Accept the entire suggestion\n\
Alt+f           Accept the first/next word of the current suggestion\n\
pf set test     Change to the profile named 'test'\n\
st size rev     Sort files by size in reverse order\n\
Alt+x | Alt+z   Switch to next/previous sort order\n\
media           (Un)mount storage devices\n\
net work        Mount the network resource named 'work'\n\
actions         List available actions/plugins\n\
icons on        Enable icons\n\
q | F12         I'm tired, quit"

#define ASCII_LOGO_BIG "\
     .okkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkd. \n\
    'kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkc\n\
    xkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk\n\
    xkkkkkc::::::::::::::::::dkkkkkkc:::::kkkkkk\n\
    xkkkkk'..................okkkkkk'.....kkkkkk\n\
    xkkkkk'..................okkkkkk'.....kkkkkk\n\
    xkkkkk'.....okkkkkk,.....okkkkkk'.....kkkkkk\n\
    xkkkkk'.....dkkkkkk;.....okkkkkk'.....kkkkkk\n\
    xkkkkk'.....dkkkkkk;.....okkkkkk'.....kkkkkk\n\
    xkkkkk'.....dkkkkkk;.....okkkkkk'.....kkkkkk\n\
    xkkkkk'.....dkkkkkk;.....okkkkkk'.....kkkkkk\n\
    xkkkkk'.....dkkkkkk;.....okkkkkk'.....kkkkkk\n\
    xkkkkk'.....dkkkkkk;.....okkkkkk'.....kkkkkk\n\
    xkkkkk'.....dkkkkkk;.....okkkkkk'.....kkkkkk\n\
    xkkkkk'.....dkkkkkk;.....okkkkkk'.....kkkkkk\n\
    xkkkkk'.....coooooo'.....:llllll......kkkkkk\n\
    xkkkkk'...............................kkkkkk\n\
    xkkkkk'...............................kkkkkk\n\
    xkkkkklccccccccccccccccccccccccccccccckkkkkk\n\
    lkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkx\n\
     ;kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkc \n\
        :c::::::::::::::::::::::::::::::::::."


/* Brief commands description */
#define AC_DESC      " (archive/compress files)"
#define ACD_DESC     " (toggle autocd)"
#define ACTIONS_DESC " (manage actions/plugins)"
#define AD_DESC      " (dearchive/decompress files)"
#define ALIAS_DESC   " (list aliases)"
#define AO_DESC      " (toggle auto-open)"
#define AUTO_DESC    " (set a temporary autocommand)"
#define B_DESC       " (change to the previously visited directory)"
#define BD_DESC      " (change to a parent directory)"
#define BL_DESC      " (create symbolic links in bulk)"
#define BB_DESC      " (sanitize non-ASCII filenames)"
#define BM_DESC      " (manage bookmarks)"
#define BR_DESC      " (rename files in bulk)"
#define C_DESC       " (copy files)"
#define CD_DESC      " (change directory)"
#define CL_DESC      " (toggle columns)"
#define CMD_DESC     " (jump to the COMMANDS section in the manpage)"
#define COLORS_DESC  " (preview the current color scheme)"
#define CONFIG_DESC  " (edit the main configuration file)"
#define CS_DESC      " (manage color schemes)"
#define CWD_DESC     " (print the current directory)"
#define D_DESC       " (duplicate files)"
#define DH_DESC      " (query the directory history list)"
#define DS_DESC      " (deselect files)"
#define EXP_DESC     " (export filenames to a temporary file)"
#define EXT_DESC     " (turn external/shell commands on/off)"
#define F_DESC       " (change to the next visited directory)"
#define FC_DESC      " (toggle the file-counter)"
#define FF_DESC      " (toggle list-directories-first)"
#define FT_DESC      " (set a file filter)"
#define FZ_DESC      " (display recursive directory sizes - long view only)"
#define HF_DESC      " (toggle hidden files)"
#define HIST_DESC    " (manage the command history)"
#define ICONS_DESC   " (toggle icons)"
#define J_DESC       " (jump to a visited directory)"
#define K_DESC       " (toggle follow-links - long view only)"
#define KK_DESC      " (toggle max-filename-len)"
#define KB_DESC      " (manage keybindings)"
#define L_DESC       " (create a symbolic link)"
#define LE_DESC      " (edit a symbolic link)"
#define LL_DESC      " (toggle the long-view)"
#define LM_DESC      " (toggle the light-mode)"
#define LOG_DESC     " (manage logs)"
#define M_DESC       " (move/rename files)"
#define MD_DESC      " (create directories)"
#define MEDIA_DESC   " (mount/unmount storage devices)"
#define MF_DESC      " (limit the number of listed files)"
#define MM_DESC      " (manage opening applications)"
#define MP_DESC      " (change to a mountpoint)"
#define MSG_DESC     " (print program messages)"
#define N_DESC       " (create files)"
#define NET_DESC     " (manage remote resources)"
#define O_DESC       " (open file)"
#define OC_DESC      " (change file ownership)"
#define OPENER_DESC  " (set a custom file opener)"
#define OW_DESC      " (open file with...)"
#define P_DESC       " (print file properties)"
#define PC_DESC      " (change file permissions)"
#define PF_DESC      " (manage user profiles)"
#define PG_DESC      " (toggle/run the file pager)"
#define PIN_DESC     " (pin a directory)"
#define PP_DESC      " (print file properties - follow links/total dir size)"
#define PROMPT_DESC  " (switch/edit the command prompt)"
#define Q_DESC       " (quit)"
#define R_DESC       " (remove files)"
#define RF_DESC      " (refresh/clear the screen)"
#define RL_DESC      " (reload the configuration file)"
#define RR_DESC      " (remove files in bulk)"
#define SB_DESC      " (access the selection box)"
#define SEL_DESC     " (select files)"
#define ST_DESC      " (change file sort order)"
#define STATS_DESC   " (print file statistics)"
#define TAG_DESC     " (manage file tags)"
#define TA_DESC      " (tag files as ...)"
#define TD_DESC      " (delete tags)"
#define TE_DESC      " (toggle the executable bit on files)"
#define TIPS_DESC    " (print tips)"
#define TL_DESC      " (list tags or tagged files)"
#define TM_DESC      " (rename tags)"
#define TN_DESC      " (create tags)"
#define TU_DESC      " (untag files)"
#define TY_DESC      " (merge tags)"
#define TRASH_DESC   " (trash files)"
#define U_DESC       " (restore trashed files using a menu)"
#define UNPIN_DESC   " (unpin the pinned directory)"
#define VER_DESC     " (print version information)"
#define VIEW_DESC    " (preview files in the current directory)"
#define VV_DESC      " (copy and rename files in bulk at once)"
#define WS_DESC      " (switch workspaces)"
#define X_DESC       " (launch a new instance of Clifm)"
#define XU_DESC      " (launch a new instance of Clifm as root)"

#endif /* MESSAGES_H */
