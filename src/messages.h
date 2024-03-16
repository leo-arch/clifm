/* messages.h - Usage and help messages for CliFM */

/*
 * This file is part of CliFM
 *
 * Copyright (C) 2016-2024, L. Abramovich <leo.clifm@outlook.com>
 * All rights reserved.

 * CliFM is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * CliFM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
*/

#ifndef MESSAGES_H
#define MESSAGES_H

/* Usage messages */
# define GRAL_USAGE "[OPTIONS] [DIR]"

#define ACTIONS_USAGE "List or edit actions/plugins\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  actions [list | edit [APP]]\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
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
\x1b[1mUSAGE\x1b[0m\n\
  alias [import FILE | list | NAME]\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- List available aliases\n\
    alias\n\
  or\n\
    alias list (or 'alias <TAB>')\n\
- Print a specific alias definition\n\
    alias my_alias\n\
- Import aliases from ~/.bashrc\n\
    alias import ~/.bashrc\n\
  Note: Only aliases following the POSIX specification (NAME=\"STR\")\n\
  will be imported."

#define ARCHIVE_USAGE "Compress/archive files\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  ac, ad ELN/FILE...\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- Compress/archive all selected files\n\
    ac sel\n\
- Compress/archive a range of files\n\
    ac 12-24\n\
  or\n\
    'ac <TAB>' to choose from a list (multi-selection is allowed)\n\
- Decompress/dearchive a file\n\
    ad file.tar.gz\n\
  or just open the file (the appropriate menu will be displayed)\n\
    o file.tar.gz (or just 'file.tar.gz')\n\n\
\x1b[1mDEPENDENCIES\x1b[0m\n\
zstd(1)           Everything related to Zstandard\n\
mkisofs(1)        Create ISO 9660 files\n\
7z(1) / mount(1)  Operate on ISO 9660 files\n\
archivemount(1)   Mount archives\n\
atool(1)          Extraction/decompression, listing, and repacking of archives"

#define AUTOCD_USAGE "Turn autocd on/off\n\
\x1b[1mUSAGE\x1b[0m\n\
  acd, autocd [on | off | status]"

#define AUTOCMDS_USAGE "Tweak settings or run custom commands on a per directory basis\n\n\
There are two ways to set autocommands:\n\
  1) Via the 'autocmd' keyword in the configuration file\n\
  2) Via specifically named files in the corresponding directory\n\n\
Example using the first method:\n\
Edit the configuration file ('config' or F10) and add the following line:\n\n\
  autocmd /media/remotes/** fc=0,lm=1\n\n\
This instructs clifm to always disable the files counter and to run in\n\
light mode whenever you enter the '/media/remotes' directory (or any\n\
subdirectory).\n\n\
Example using the second method:\n\
a. Set 'ReadAutocmdFiles' to 'true' in the configuration file.\n\
b. Create a '.cfm.in' file in the '~/Important' directory with the following\n\
content:\n\n\
  echo \"Please keep me in sync with work files\" && read -n1\n\n\
This little reminder will be printed every time you enter the 'Important'\n\
directory.\n\n\
If the file is named rather '.cfm.out', the command will be executed when\n\
leaving, instead of entering, the directory.\n\n\
Note: Only single-line commands are allowed. If you need more advanced\n\
stuff, set here the path to a script doing whatever needs to be done.\n\n\
Note 2: Codes to modify clifm's settings (as described in the first method)\n\
are not available here."

#define AUTO_OPEN_USAGE "Turn auto-open on/off\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  ao, auto-open [on | off | status]"

#define BACK_USAGE "Change to the previous directory in the directory \
history list\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  b, back [h, hist | clear | !ELN]\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- Just change to the previously visited directory\n\
    b (also Alt-j or Shift-Left)\n\
- Print the directory history list\n\
    b hist (or 'dh')\n\
- Change to the directory whose ELN in the list is 24\n\
    b !24\n\
- Use the 'f' command to go forward\n\
    f (also Alt-k or Shift-Right)"

#define BD_USAGE "Quickly change to a parent directory matching NAME. If \
NAME is not specified, print the list of all parent directories\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  bd [NAME]\n\n\
\x1b[1mEXAMPLE\x1b[0m\n\
- Supposing you are in ~/Documents/misc/some/deep/dir, change to\n\
~/Documents/misc\n\
    bd mi (or 'bd <TAB>' to choose from a list)"

#define BL_USAGE "Create multiple symbolic links at once\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  bl FILE...\n\n\
\x1b[1mEXAMPLE\x1b[0m\n\
- Symlink files 'file1', 'file2', 'file3', and 'file4' at once\n\
    bl file* (or 'bl <TAB>' to choose from a list - multi-selection is\n\
  allowed)\n\
- Create symbolic links in the directory 'dir' for all .png files\n\
    s *.png\n\
    cd dir\n\
    bl sel\n\n\
Note: Links are always created in the current directory."

#define BLEACH_USAGE "Clean up file names from non-ASCII characters\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  bb, bleach ELN/FILE...\n\n\
\x1b[1mEXAMPLE\x1b[0m\n\
- Bleach file names in your Downloads directory\n\
    bb ~/Downloads/*"

#define BOOKMARKS_USAGE "Manage bookmarks\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  bm, bookmarks [a, add FILENAME NAME | d, del [NAME] | e, edit [APP] | NAME]\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- List bookmarks\n\
    bm (also Alt-b, 'bm <TAB>' or 'b:<TAB>')\n\
- Bookmark the directory /media/mount as 'mnt'\n\
    bm add /media/mount mnt\n\
  Note: Regular files can be bookmarked too\n\
- Access the bookmark named 'mnt'\n\
    bm mnt (also 'b:mnt', 'b:<TAB>' or 'bm <TAB>' to choose from a list)\n\
- Remove the bookmark named 'mnt'\n\
    bm del mnt (or 'bm del <TAB>' to choose from a list)\n\
- Edit the bookmarks file manually\n\
    bm edit (or F11)\n\
- Edit the bookmarks file using vi\n\
    bm edit vi\n\
- Print file properties of specific bookmarks using the 'b:' construct\n\
    p b:<TAB> (multi-selection is allowed)\n\
- Select all bookmarks at once\n\
    s b:"

#define BULK_RENAME_USAGE "Bulk rename files\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  br, bulk ELN/FILE... [:EDITOR]\n\n\
The list of files to be renamed is opened via EDITOR (default associated\n\
application for text files if omitted). Edit the file names you want to \n\
rename, save, and quit the editor (quit without saving to cancel the \n\
operation).\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- Bulk rename all files ending with .pdf in the current directory\n\
    br *.pdf\n\
  or\n\
    'br <TAB>' to choose from a list (mutli-selection is allowed)\n\
- Bulk rename all selected files\n\
    br sel\n\
  or, using vi:\n\
    br sel :vi"

#define CD_USAGE "Change the current working directory\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  cd [ELN/DIR]\n\n\
\x1b[1mEXAMPLE\x1b[0m\n\
- Change to /var\n\
    cd /var\n\
  or, if autocd is enabled (default)\n\
    /var"

#define COLORS_USAGE "Print the list of currently used color codes\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  colors"

#define COLUMNS_USAGE "Toggle columned list of files on/off\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  cl, columns [on | off]"

#define CS_USAGE "Switch color schemes\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  cs, colorschemes [COLORSCHEME | edit [APP] | n, name]\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- Print current color scheme\n\
    cs name (or 'cs n')\n\
- List available color schemes\n\
    cs (or 'cs <TAB>')\n\
- Edit the current color scheme\n\
    cs edit\n\
- Edit the current color scheme using vi\n\
    cs edit vi\n\
- Switch to the color scheme named 'mytheme'\n\
    cs mytheme\n\n\
Tip: Theming via LS_COLORS is also possible.\n\
Run with --lscolors. Consult the Wiki for details:\n\
https://github.com/leo-arch/clifm/wiki/Customization#ls_colors-support"

#define DESEL_USAGE "Deselect one or more selected files\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  ds, desel [*, a, all]\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- Deselect all selected files\n\
    ds * (or Alt-d)\n\
- Deselect files from a menu\n\
    ds (or 'ds <TAB>' to choose from a list - multi-selection is allowed)"

#define DESKTOP_NOTIFICATIONS_USAGE "Errors, warnings, and notices are send \
to the notification daemon instead of\n\
being printed immediately before the next prompt\n\n\
To enable this feature use the --desktop-notifications command line flag or\n\
set DesktopNotifications to true in the configuration file (F10).\n\n\
Notifications are sent using the following command:\n\n\
Linux/BSD: notify-send -u \"TYPE\" \"TITLE\" \"MSG\"\n\
MacOS:     osascript -e 'display notification \"MSG\" subtitle \"TYPE\" with title \"TITLE\"'\n\
Haiku:     notify --type \"TYPE\" --title \"TITLE\" \"MSG\"\n\n\
Note: It is the notification daemon itself who takes care of actually printing\n\
notifications on your screen. For troubleshoting, consult your \
daemon's documentation.\n\n\
Tip: You can always check notifications using the 'msg' command."

#define DH_USAGE "Query the directory history list\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  dh [STRING] [PATH] [!ELN]\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- Print the directory history list\n\
    dh (also 'dh <TAB>')\n\
- Print directory history entries matching \"query\"\n\
    dh query (also 'dh query<TAB>')\n\
- Change to the entry number (ELN) 12\n\
    dh !12\n\
  Note: Entry numbers are not displayed when using TAB completion.\n\n\
Note: If the first argument is an absolute path, 'dh' works just as 'cd'.\n\
Tip: Take a look at the 'j' command as well."

#define DIRHIST_USAGE "List or access entries in the directory history list\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  b/f [hist | clear | !ELN]\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- Print the directory history list\n\
    b hist\n\
- Change to the directory whose ELN is 12 in the directory history list\n\
    b !12\n\
- Remove all entries from the directory history list\n\
    b clear\n\n\
Tip: See also the 'dh' and 'j' commands."

#define DUP_USAGE "Duplicate files via rsync(1) (cp(1) if rsync is not found)\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  d, dup ELN/FILE...\n\n\
\x1b[1mEXAMPLE\x1b[0m\n\
- Duplicate files whose ELN's are 12 through 20\n\
    d 12-20\n\n\
You will be asked for a destiny directory.\n\
Duplicated files are created as SRC.copy, and, if SRC.copy exists, as \n\
SRC.copy-n, where n is an positive integer (starting at 1).\n\n\
Parameters passed to rsync: --aczvAXHS --progress\n\n\
Parameters passed to cp: -a"

#define EDIT_USAGE "Edit the main configuration file\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  config [reset | dump | APPLICATION]\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- Open/edit the configuration file\n\
    config (or F10)\n\
- Open/edit the configuration file using nano\n\
    config nano\n\
- Print current values, highlighting those deviating from default values\n\
    config dump\n\
- Create a fresh configuration file (making a backup of the old one)\n\
    config reset"

#define EXT_USAGE "Turn on/off the use of external commands\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  ext [on, off, status]"

#define EXPORT_FILES_USAGE "Export files to a temporary file\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  exp [ELN/FILE...]\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- Export all selected files\n\
    exp sel\n\
- Export all PDF files in the current directory\n\
    exp *.pdf"

#define EXPORT_VAR_USAGE "Add one or more variables to the environment\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  export NAME=VALUE..."

#define FC_USAGE "Toggle the files counter for directories on/off\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  fc [on | off | status]"

#define FILE_DETAILS "List file details\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- Toggle long/detail view mode\n\
    ll (also Alt-l)\n\
  Note: Use PropFields in the configuration file to customize output\n\
  fields (and TimeStyle for custom timestamp formats).\n\
- Print properties of the file whose ELN is 4\n\
    p4\n\
- Print file properties, including directory full size\n\
    pp DIR\n\n\
Note: An exclamation mark (!) before directory sizes means that an\n\
error ocurred while reading a subdirectory, so sizes may not be accurate\n\n\
Note 2: Unlink 'p', 'pp' always follows symlinks to their target file."

#define FILE_SIZE_USAGE "File sizes/disk usage\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- Enable full directory size (long view)\n\
    fz on (or --full-dir-size)\n\
- Toggle the disk usage analyzer mode on/off\n\
    Alt-TAB (or -t,--disk-usage-analyzer)\n\
- Print files sizes as used blocks instead of used bytes (apparent size)\n\
    Run with --no-apparent-size or set ApparentSize to false in the\n\
    configuration file.\n\
- Use powers of 1000 instead of 1024 for file sizes\n\
    Run with --si"

#define FF_USAGE "Set list-directories-first on/off\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  ff, dirs-first [on | off | status]\n\
\x1b[1mEXAMPLE\x1b[0m\n\
- Disable list directories-first\n\
    ff off\n\
  Note: Toggle directories-first on/off pressing Alt-g."

#define FILE_PREVIEWS "Use the 'view' command to preview files in the current \
directory (depends on fzf).\n\n\
To add file previews to TAB completion (fzf mode only), use the --fzfpreview\n\
command line option, or set FzfPreview to true in the configuration file\n\
('config' or F10).\n\n\
Enabling image previews (either ueberzug (X11 only) or the Kitty terminal\n\
are required)\n\
\n\
1. Copy 'clifmrun' and 'clifmimg' scripts to somewhere in you $PATH \n\
(say /usr/local/bin). You can find them in DATADIR/clifm/plugins (usually\n\
/usr/local/share/clifm/plugins).\n\n\
2. Edit shotgun's configuration file ('view edit' or F7) and add the\n\
following lines at the top of the file (to make sure they won't be\n\
overriden by previous directives):\n\
\n\
X:^application/.*(officedocument|msword|ms-excel|opendocument).*=clifmimg doc;\n\
X:^text/rtf$=clifmimg doc;\n\
X:^application/epub\\+zip$=clifmimg epub;\n\
X:^appliaction/pdf$=clifmimg pdf;\n\
X:^image/vnd.djvu=clifmimg djvu;\n\
X:^image/svg\\+xml$=clifmimg svg;\n\
X:^image/.*=clifmimg image;\n\
X:^video/.*=clifmimg video;\n\
X:^audio/.*=clifmimg audio;\n\
X:^application/postscript$=clifmimg postscript;\n\
X:N:.*\\.otf$=clifmimg font;\n\
X:font/.*=clifmimg font;\n\
\n\
Comment out whatever you want to exclude from the image preview function.\n\
\n\
3. Run clifm via the 'clifmrun' script:\n\
clifmrun --fzfpreview\n\
\n\
Note on Kitty and Wayland:\n\
If running on the kitty terminal you can force the use of the kitty image\n\
protocol (instead of ueberzug) as follows:\n\
\n\
CLIFM_KITTY_NO_UEBERZUG=1 clifmrun --fzfpreview\n\
\n\
Note that on Wayland the kitty image protocol will be used by default, so\n\
that there is no need to set this variable."

#define FILTER_USAGE "Set a filter for the files list\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  ft, filter [unset | [!]REGEX,=FILE-TYPE-CHAR]\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
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
You can also filter files in the current directory using TAB\n\
completion via wildcards and the file type filter:\n\
- List PDF files\n\
    /*.pdf<TAB>\n\
- List executable files\n\
    =x<TAB>\n\n\
Available file type characters:\n\
  b: Block devices\n\
  c: Character devices\n\
  d: Directories\n\
  D: Doors (Solaris only)\n\
  f: Regular files\n\
  h: Multi-hardlink files\n\
  l: Symbolic links\n\
  p: FIFO/pipes\n\
  s: Sockets\n\
  C: Files with capabilities (1)(2)\n\
  o: Other-writable files (2)\n\
  t: Files with the sticky bit set (2)\n\
  u: SUID files (2)\n\
  g: SGID files (2)\n\
  x: Executable files (2)\n\n\
(1) Only via TAB completion\n\
(2) Not available in light mode\n\n\
Type '=<TAB>' to get the list of available file type filters.\n\n\
Other ways of filtering files in the current directory:\n\n\
* @<TAB>       List all MIME-types found\n\
* @query<TAB>  MIME-type filter. Ex: @pdf<TAB> to list all PDF files\n\
* /query       Quick search function: consult the 'search' help topic\n\
* Alt-.        Toggle hidden files\n\
* Alt-,        Toggle list-only-dirs\n\
* Just press TAB (fzf/fnf mode) and perform a fuzzy search\n\n\
You can also operate on files filtered by file type and/or MIME type as\n\
follows:\n\n\
    CMD =file-type-char @query\n\n\
For example, to select all executable files, symbolic links, and image\n\
files in the current directory:\n\n\
    s =x =l @image"

#define FORTH_USAGE "Change to the next directory in the directory history list\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  f, forth [h, hist | clear | !ELN]\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- Just change to the next visited directory\n\
    f (also Alt-k or Shift-Right)\n\
- Print the directory history list\n\
    f hist (or 'dh')\n\
- Change to the directory whose ELN in the list is 24\n\
    f !24\n\
- Use the 'b' command to go backwards\n\
    b (also Alt-j or Shift-Left)"

#define FZ_USAGE "Toggle full directory size on/off (only for long view mode)\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  fz [on, off]"

#define HELP_USAGE "Get help\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  help [TOPIC]\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- Print the help screen\n\
    help\n\
- Get help about the 'bookmarks' topic\n\
    help bookmarks\n\
- Print the list of available help topics\n\
    help <TAB>"

#define HF_USAGE "Set hidden files on/off\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  hf, hh, hidden [on | off | status]\n\n\
\x1b[1mEXAMPLE\x1b[0m\n\
- Show hidden files\n\
    hh on\n\
- Toggle hidden files\n\
    hh (or Alt-.)"

#define HISTEXEC_USAGE "Access commands history entries\n\n\
\x1b[1mUSAGE\x1b[0m\n\
history or !<TAB>: List available commands\n\
!!: Execute the last command\n\
!n: Execute the command number 'n' in the history list\n\
!-n: Execute the last - n command in the history list"

#define HISTORY_USAGE "List or access commands history entries\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  history [edit [APP] | clear | -N | on | off | status | show-time [-N]]\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
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
You can also access the commands history via the exclamation mark (!).\n\
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

#define ICONS_USAGE "Set icons on/off\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  icons [on, off]"

#define JUMP_USAGE "Change to a directory in the jump database (visited directories)\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  j [--purge [NUM] | --edit [APP]], jc, jp, jl [STRING]..., je\n\n\
For information about the matching algorithm consult the manpage\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- Print the list of entries in the jump database\n\
    j (or jl)\n\
- List all entries matching \"str\"\n\
    jl str\n\
- Jump (cd) to the best ranked directory matching \"bui\"\n\
    j bui\n\
  Note: Hit TAB to get a list of possible matches: 'j bui<TAB>'.\n\
- If not enough, use multiple query strings\n\
    j ho bui\n\
  Note: Most likey, this will take you to '/home/build'\n\
- Jump to the best ranked PARENT directory matching \"str\"\n\
    jp str\n\
- Jump to the best ranked CHILD directory matching \"str\"\n\
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
- Purge the database from non-existent directories\n\
    j --purge\n\
  Note: To automatically purge the database from non-existent directories\n\
  at startup, set PurgeJumpDB to true in the configuration file.\n\
- Purge the database from entries ranked below 100\n\
    j --purge 100\n\
  Note: To remove a specific entry, delete the corresponding line\n\
  from the database ('je' or 'j --edit'). Note that if the directory\n\
  is in the directory history, it won't be removed from the database."

#define K_USAGE "Toggle follow-links in long view mode\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  k\n\n\
If enabled, when running in long view information for the file a symbolic\n\
link points to (instead of for the link itself) is displayed."

#define KK_USAGE "Toggle max-filename-len on/off\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  kk"

#define KB_USAGE "Manage key bindings\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  kb, keybinds [list | edit [APP] | reset | readline]\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- List your current key bindings\n\
    kb (or 'kb list')\n\
- Open/edit the key bindings file\n\
    kb edit\n\
- Open/edit the key bindings file using mousepad\n\
    kb edit mousepad\n\
- List the current key bindings for readline\n\
    kb readline\n\
- Reset your key bindings settings\n\
    kb reset"

#define LE_USAGE "Edit a symbolic link\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  le SYMLINK\n\n\
The user is prompted to enter a new link target using the current\n\
target as template.\n\n\
\x1b[1mEXAMPLE\x1b[0m\n\
- Edit the symbolic link named file.link\n\
    le file.link"

#define LINK_USAGE "Create a symbolic link\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  l TARGET [LINK_NAME]\n\n\
If LINK_NAME is omitted, it is created as TARGET_BASENAME.link in\n\
the current directory.\n\n\
\x1b[1mEXAMPLE\x1b[0m\n\
- Create a symbolic link to file.zst named mylink\n\
    l file.zst mylink\n\n\
Note: Use the 'le' command to edit a symbolic link. Try 'le --help'."

#define LL_USAGE "Toggle long view mode\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  ll, lv [on | off]\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- Change to long view\n\
    ll on\n\
- Toggle long view\n\
    ll (or Alt-l)"

#define LM_USAGE "Set light mode on/off\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  lm [on, off]"

#define LOG_USAGE "Manage Clifm logs\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  log [cmd | msg] [list | on | off | status | clear]\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- Print message logs\n\
    log msg list (or just 'log msg')\n\
- Enable command logs\n\
    log cmd on\n\
- Disable message logs\n\
    log msg off\n\
- Clear message logs\n\
    log msg clear"

#define MD_USAGE "Create one or more directories\n\
(parent directories are created if they do not exist)\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  md DIR...\n\n\
\x1b[1mEXAMPLE\x1b[0m\n\
  md dir1 dir2 dir3/subdir\n\n\
Note: Use the 'n' command to create both files and directories.\n\
Try 'n --help' for more details."

#define MEDIA_USAGE "List available media devices, allowing you to mount or \
unmount them\n\
Note: Either udevil(1) or udisks2(1) is required\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  media\n\n\
The list of mounted and unmounted devices will be displayed.\n\
Choose the device you want using ELN's.\n\
If the device is mounted, it will be unmounted; if unmounted, it will \
be mounted.\n\
If mounting a device, CliFM will change automatically to the corresponding\n\
mountpoint.\n\n\
To get information about a device, enter iELN. For example: 'i12'."

#define MF_USAGE "Limit the amount of files listed on the screen to NUM \
(valid range: >= 0). Use 'unset' to remove the files limit.\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  mf [NUM | unset]"

#define MIME_USAGE "Set default opening applications based on MIME types or file names\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  mm, mime [open FILE | info FILE | edit [APP] | import]\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
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
- Add/modify default opening application for myfile\n\
    1) Find out the MIME type (or file name) of the file\n\
      mm info myfile\n\
    2) Edit the mimelist file\n\
      mm edit (or F6)\n\
    Once in the file, find the appropriate entry and add the desired\n\
    opening application.\n\
  For more information consult the manpage."

#define MSG_USAGE "List available Clifm messages\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  msg, messages [clear]\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- List available messages\n\
    msg\n\
- Clear the current list of messages\n\
    msg clear (or Alt-t)"

#define MOUNTPOINTS_USAGE "List and change to a mountpoint\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  mp, mountpoints\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- List available mountpoints\n\
    mp (or Alt-m)\n\
  Once there, select the mountpoint you want to change to."

#define NET_USAGE "Manage network resources\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  net [NAME] [list | edit [APP] | m, mount NAME | u, unmount NAME]\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
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
- Copy a file to a remote location via the 'cr' plugin\n\
    cr FILE (run 'cr --edit' before to set up your remotes)"

#define NEW_USAGE "Create new files and/or directories\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  n, new [FILE...] [DIR/...]\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- Create two files named file1 and file2\n\
    n file1 file2\n\
- Create two directories named dir1 and dir2\n\
    n dir1/ dir2/\n\
  Note: Note the ending slashes.\n\
- Both of the above at once:\n\
    n file1 file2 dir1/ dir2/\n\n\
Parent directories are created if necessary. For example, if you run\n\
    n dir/subdir/file\n\
both 'dir' and 'subdir' directories will be created if they do not exist."

#define OC_USAGE "Interactively change files ownership\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  oc FILE...\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- Change ownership of selected files\n\
    oc sel\n\
- Change ownership of all .iso files\n\
    oc *.iso\n\n\
\x1b[1mNOTES\x1b[0m\n\
A template is presented to the user to be edited.\n\n\
Only user and primary group common to all files passed as\n\
parameters are set in the ownership template.\n\n\
Ownership (both user and primary group, if specified) is\n\
changed for all files passed as parameters.\n\n\
Both names and ID numbers are allowed (TAB completion is available).\n\n\
If only a name/number is entered, it is taken as user.\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- Change user to root\n\
    root (or \"0\")\n\
- Change primary group to video\n\
    :video (or \":981\")\n\
- Change user to peter and primary group to audio\n\
    peter:audio (or \"1000:986\" or \"peter:986\" or \"1000:audio\")\n\n\
Note: Use the 'pc' command to edit files permissions."

#define OPEN_USAGE "Open a file\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  o, open ELN/FILE [APPLICATION]\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- Open the file whose ELN is 12 with the default associated application\n\
  (see the 'mime' command)\n\
    o 12\n\
- Open the file whose ELN is 12 with vi\n\
    o 12 vi\n\
  Note: If auto-open is enabled (default), 'o' could be just omitted\n\
    12\n\
    12 vi"

#define OPENER_USAGE "Set the resource opener\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  opener APPLICATION\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- Set the resources opener to xdg-open (instead of the default, Lira)\n\
    opener xdg-open\n\
- Set the resources opener back to the default (Lira)\n\
    opener default"

#define OW_USAGE "Open a file with a specific application\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  ow FILE [APP]\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- Choose opening application for the file 'test.c' from a menu\n\
    ow test.c\n\
  or\n\
    'ow test.c <TAB>' (to get a list of available applications)\n\
  Note: Available applications are taken from the mimelist file (see the\n\
  'mime' command), and only valid and installed applications are listed.\n\
- Open the file 'test.c' with geany\n\
    ow test.c geany"

#define PAGER_USAGE "Set the files list pager on/off\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  pg, pager [on | off | once | status | NUM]\n\n\
If set to 'on', run the pager whenever the list of files does no fit on\n\
the screen.\n\n\
If set to any positive integer greater than 1, run the pager whenever\n\
the amount of files in the current directory is greater than or equal to\n\
this value (say, 1000). 1 amounts to 'on' and 0 to 'off'.\n\n\
Set to 'once' to run the pager only once. Since this is the default\n\
parameter, 'pg' (with no parameter) is equivalent to 'pg once'.\n\n\
Note: You can also try the 'pager' plugin running 'gg'.\n\n\
While paging, the following keys are available:\n\n\
?, h: Help\n\
Down arrow, Enter, Space: Advance one line\n\
Page down: Advance one page\n\
q: Stop paging (without printing remaining files)\n\
c: Stop paging (printing remaining files)\n\n\
Note: For upwards scrolling, use whatever your terminal emulator\n\
has to offer (ex: mouse scrolling or some keybinding)"

#define PC_USAGE "Interactively edit files permissions\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  pc FILE...\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- Change permissions of file named file.txt\n\
    pc file.txt\n\
- Change permissions of all selected files at once\n\
    pc sel\n\n\
When editing multiple files with different permissions at once,\n\
only shared permission bits will be set in the permissions template.\n\
Bear in mind that the new permissions set will be applied to all files\n\
passed as arguments.\n\n\
Both symbolic and octal notation for the new permissions set are allowed.\n\n\
Note: Use the 'oc' command to edit files ownership."

#define PIN_USAGE "Pin a file or directory\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  pin FILE/DIR\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- Pin the directory ~/my_important_dir\n\
    pin ~/my_important_dir\n\
- Change to the pinned directory\n\
    , (yes, just a comma)\n\
- Unpin the currently pinned directory\n\
    unpin"

#define PROFILES_USAGE "Manage profiles\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  pf, profile [list | set, add, del PROFILE | rename PROFILE NEW_NAME]\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- Print the current profile name\n\
    pf\n\
- List available profiles\n\
    pf list\n\
- Switch to the profile 'myprofile'\n\
    pf set myprofile (or 'pf set <TAB>' to choose from a list)\n\
- Add a new profile named new_profile\n\
    pf add new_profile\n\
- Remove the profile 'myprofile'\n\
    pf del myprofile (or 'pf del <TAB>' to choose from a list)\n\
- Rename the profile 'myprofile' as 'cool_name'\n\
    pf rename myprofile cool_name"

#define PROMPT_USAGE "Change current prompt\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  prompt [set NAME | list | unset | edit [APP] | reload]\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- List available prompts\n\
    prompt list (or 'prompt set <TAB>' to choose from a list)\n\
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

#define PROP_USAGE "Print files properties\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  p, pp, prop [ELN/FILE...]\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- Print the properties of the file whose ELN is 12\n\
    p 12 (or 'p <TAB>' to choose from a list)\n\
- Print the properties of all selected files\n\
    p sel\n\
- Print the properties of the directory 'dir' (including total size)\n\
    pp dir\n\n\
Note that, in case of symbolic links to directories, the 'p' command displays\n\
information about the link target if the provided file name ends with a slash.\n\
Otherwise, information about the link itself is displayed.\n\
Unlike 'p', however, 'pp' always follows symlinks to their target file."

#define PWD_DESC "Print the name of the current working directory\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  pwd [-L | -P]\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- Print the logical working directory (do not resolve symlinks)\n\
    pwd (or 'pwd -L')\n\
- Print the physical working directory (resolve all symlinks)\n\
    pwd -P"

#define QUIT_HELP "Exit clifm\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  q, quit, exit\n\n\
To enable the cd-on-quit function consult the manpage."

#define RR_USAGE "Remove files in bulk using a text editor\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  rr [DIR] [:EDITOR]\n\n\
The list of files in DIR (current directory if omitted) is opened via\n\
EDITOR (default associated application for text files if omitted). Remove\n\
the lines corresponding to the files you want to delete, save, and quit\n\
the editor (quit without saving to cancel the operation).\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- Bulk remove files/dirs in the current directory using the default editor\n\
    rr\n\
- Bulk remove files/dirs in the current directory using nano\n\
    rr :nano\n\
- Bulk remove files/dirs in the directory 'mydir' using vi\n\
    rr mydir :vi"

#define SEARCH_USAGE "Search for files using either glob or regular expressions\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  /PATTERN [-filetype] [-x] [DIR]\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- List all PDF files in the current working directory\n\
    /*.pdf (or, as a regular expression, '/.*\\.pdf$')\n\
- List all files starting with 'A' in the directory whose ELN is 7\n\
    /A* 7\n\
- List all .conf files in /etc\n\
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
with 'd' in the directory named Documents\n\
    /[.-].*d$ -d Documents/\n\n\
To perform a recursive search, use the -x modifier (file types not allowed)\n\
    /str -x /boot\n\n\
To search for files by content instead of names use the rgfind plugin, bound\n\
by default to the \"//\" action name. For example:\n\
    // content I\\'m looking for\n\n\
Note: This plugin depends on fzf(1) and rg(1) (ripgrep)."

#define SECURITY_USAGE "CliFM provides three different security mechanisms:\n\n\
1. Stealth mode (aka incognito/private mode): No file is read nor written\n\
to the file system (unless explicitly required by the user via a command).\n\
Default values are used.\n\
Enable this mode via the -S,--stealth-mode command line switch.\n\n\
2. Secure environment: CliFM runs on a sanitized environment (most\n\
environment variables are cleared and a few of them set to sane defaults).\n\
Enable this mode via the --secure-env or --secure-env-full command line\n\
switches.\n\n\
3. Secure commands: Automatically executed shell commands (autocommands,\n\
(un)mount, opening applications, just as prompt and profile commands) are\n\
sanitized before being executed: a secure environment is set and the\n\
command is validated using a whitelist to prevent unexpected/insecure\n\
behavior and command injection. Enable this mode using the --secure-cmds\n\
command line switch."

#define SEL_USAGE "Select one or multiple files\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  s, sel ELN/FILE... [[!]PATTERN] [-FILETYPE] [:PATH]\n\n\
Recognized file types: (d)irectory, regular (f)ile, symbolic (l)ink,\n\
(s)ocket, fifo/(p)ipe, (b)lock device, (c)haracter device\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- Select the file whose ELN is 12\n\
    s 12 (or 's <TAB>' to choose from a list - multi-selection is allowed)\n\
- Select all files ending with .odt:\n\
    s *.odt\n\
- Select multiple files at once\n\
    s 12 15-21 *.pdf\n\
- Select all regular files in /etc starting with 'd'\n\
    s ^d.* -f :/etc\n\
- Select all files in the current directory (including hidden files)\n\
    s * .* (or Alt-a)\n\
- Interactively select files in '/media' (requires fzf, fnf, or smenu\n\
  TAB completion mode)\n\
    s /media/*<TAB>\n\
- List currently selected files\n\
    sb\n\
- Copy selected files into the current directory:\n\
    c sel\n\
- Move selected files into the directory whose ELN is 24\n\
    m sel 24\n\
- Deselect all selected files\n\
    ds * (or Alt-d)\n\
- Deselect files selectively\n\
    ds <TAB> (multi-selection is allowed)"

#define SORT_USAGE "Change files sorting order\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  st [METHOD] [rev]\nMETHOD: 0 = none, \
1 = name, 2 = size, 3 = atime, 4 = btime, \n5 = ctime, \
6 = mtime, 7 = version, 8 = extension, 9 = inode,\n\
10 = owner, 11 = group, 12 = blocks, 13 = links\n\
Note: Both numbers and names are allowed.\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- List files by size\n\
    st size (or 'st <TAB>' to choose from a list)\n\
- Revert the current sorting order (i.e. z-a instead of a-z)\n\
    st rev"

#define TAG_USAGE "(Un)tag files and/or directories\n\n\
\x1b[1mUSAGE\x1b[0m\n\
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
\x1b[1mEXAMPLES\x1b[0m\n\
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
- Rename tag 'documents' as 'docs'\n\
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
    p t:docs (or 'p t:<TAB>' to choose from a list)\n\
- Remove all files tagged as 'images'\n\
    r t:images\n\
- Run stat(1) over all files tagged as 'work' and all files tagged as\n\
  'docs'\n\
    stat t:work t:docs\n\n\
To operate only on some tagged files use TAB as follows:\n\
    t:TAG<TAB> (multi-selection is allowed)\n\
Mark the files you need via TAB and then press Enter or Right."

#define TE_USAGE "Toggle the executable bit on files\n\n\
\x1b[1mUSAGE\x1b[0m\n\
    te ELN/FILE...\n\
  or\n\
    'te <TAB>' to choose from a list (multi-selection is allowed)\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- Set the executable bit on all shell scripts in the current directory\n\
    te *.sh\n\
- Set the executable bit on all selected files\n\
   te sel"

#define TRASH_USAGE "Send one or multiple files to the trash can\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  t, trash [FILE... | del | empty | list]\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- Trash the file whose ELN is 12\n\
    t 12\n\
  or\n\
    't <TAB>' to choose from a list (multi-selection is allowed)\n\
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
    'u <TAB>' to choose from a list (multi-selection is allowed)\n\n\
Note: For more information try 'u --help'."

#define UMASK_USAGE "Print/set the file mode creation mask\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  umask [MODE]\n\n\
Note: MODE is an octal value from 000 to 777.\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- Print the current value\n\
    umask\n\
- Set mask to 077\n\
    umask 077\n\n\
Note: To permanently set the umask, use the Umask option in the config file."

#define UNSET_USAGE "Delete variables from the environment\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  unset NAME..."

#define UNTRASH_USAGE "Restore files from the trash can\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  u, undel, untrash [FILE... | *, a, all]\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- Restore all trashed files (to their original location)\n\
    u *\n\
- Restore trashed files selectively using a menu\n\
    u\n\
  or\n\
    'u <TAB>' to choose from a list (multi-selection is allowed)\n\n\
Note: Use the 'trash' command to trash files. Try 'trash --help'."

#define VV_USAGE "Copy files into a directory and bulk rename them at once\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  vv FILE... DIR\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- Copy selected files into 'mydir' and rename them\n\
    vv sel mydir\n\
- Copy all PDF files into the directory whose ELN is 4 and rename them\n\
    vv *.pdf 4"

#define VIEW_USAGE "Preview files in the current directory (requires fzf)\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  view [edit [APP]]\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- Just run the previewer\n\
    view (or Alt+-)\n\
- Edit the configuration file\n\
    view edit (or F7)\n\
- Edit the configuration file using vi\n\
    view edit vi\n\n\
Enter 'help file-previews' for more information."

#define WRAPPERS_USAGE "c, m, and r commands are wrappers for \
cp(1), mv(1), and rm(1) shell\ncommands respectively.\n\n\
\x1b[1mUSAGE\x1b[0m\n\
c  -> cp -iRp\n\
m  -> mv -i\n\
r  -> rm -f ('-rf' for directories) (1)\n\n\
(1) The user is asked for confirmation if the list of files contains:\n\
a. At least one directory\n\
b. Three or more files\n\
c. At least one non-explicitly-expanded ELN (ex: 'r 12')\n\n\
The 'paste' command is equivalent to 'c' and exists only for semantic\n\
reasons. For example, if you want to copy selected files into the current\n\
directory, it makes sense to write 'paste sel'.\n\n\
By default, both the 'c' and 'm' commands run cp(1)/mv(1) interactively\n\
(-i), i.e. prompting before overwriting a file. To run non-interactivelly\n\
instead, use the -f,--force parameter (see the examples below). You can\n\
also permanently run in non-interactive mode using the cpCmd/mvCmd options\n\
in the configuration file ('config' or F10).\n\n\
Just as 'c' and 'm', the 'r' command accepts -f,--force as paramater to\n\
prevent 'r' from prompting before removals. Set rmForce to true in the\n\
configuration file to make this option permanent.\n\n\
To use different parameters, run the corresponding utility, as usual.\n\
Example: cp -abf ...\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- Create a copy of file1 named file2\n\
    c file1 file2\n\
- Create a copy of file1 in the directory dir1 named file2\n\
    c file1 dir1/file2\n\
- Copy all selected files into the current directory\n\
    c sel\n\
  Note: If destiny directory is omitted, the current directory is assumed.\n\
- Copy all selected files into the current directory (non-interactively):\n\
    c -f sel\n\
- Move all selected files into the directory named testdir\n\
    m sel testdir\n\
- Rename 'file1' as 'file_test'\n\
    m file1 file_test\n\
- Interactively rename 'file1'\n\
    m file1\n\
  Note: The user is prompted to enter a new name using the old name as\n\
  template.\n\
- Move all selected files into the current directory (non-interactively)\n\
    m -f sel\n\
- Remove all selected files\n\
    r sel\n\
- Remove all selected files (non-interactively)\n\
    r -f sel\n\
  Note: Use the 't' command to send files to the trash can. Try 't --help'.\n\n\
To create files and directories you can use the 'md' and 'n' commands.\n\
Try 'md --help' and 'n --help' for more details.\n\n\
Use the 'vv' command to copy files into a directory and bulk rename them\n\
at once. Try 'vv --help'.\n\n\
Use the 'cr' plugin to send a file to a remote location:\n\
    cr FILE (run 'cr --edit' before to set up your remotes)\n\n\
Use the 'l' command to create symbolic links, and 'le' to edit them."

#define WS_USAGE "Switch workspaces\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  ws [NUM/NAME [unset] | + | -]\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- List available workspaces\n\
    ws (or 'ws <TAB>')\n\
- Switch to the first workspace\n\
    ws 1 (or Alt-1)\n\
- Switch to worksapce named 'main'\n\
    ws main\n\
- Switch to the next workspace\n\
    ws +\n\
- Switch to the previous workspace\n\
    ws -\n\
- Unset the workspace number 2\n\
    ws 2 unset\n\n\
Note: Use the WorkspaceNames option in the configuration file to name\n\
your workspaces."

#define X_USAGE "Launch a new instance of CliFM on a new terminal window\n\n\
\x1b[1mUSAGE\x1b[0m\n\
  x, X [DIR]\n\n\
\x1b[1mEXAMPLES\x1b[0m\n\
- Launch a new instance in the current directory\n\
    x\n\
- Open the directory mydir in a new instance\n\
    x mydir\n\
- Launch a new instance as root\n\
    X\n\n\
Note: By default xterm(1) is used. Set your preferred terminal\n\
emulator using the TerminalCmd option in the configuration file."

/* Misc messages */
#define PAGER_HELP "?, h: help\nDown arrow, Enter, Space: Advance one line\n\
Page Down: Advance one page\nq: Stop paging (without printing remaining files)\n\
c: Stop paging (printing remaining files)\n"
#define PAGER_LABEL "\x1b[7;97m--Mas--\x1b[0;49m"
#define NOT_AVAILABLE "This feature has been disabled at compile time"
#define STEALTH_DISABLED "Access to configuration files is not allowed in stealth mode"
#define CONFIG_FILE_UPDATED "File modified. Settings updated\n"

#ifndef __HAIKU__
# define HELP_MESSAGE "Enter '?' or press F1-F3 for instructions"
#else
# define HELP_MESSAGE "Enter '?' or press F5-F7 for instructions"
#endif /* !__HAIKU__ */

#ifdef _BE_POSIX
#define OPTIONS_LIST "\
\n -a       List hidden files\
\n -A       Do not list hidden files\
\n -b FILE  Set an alternative bookmarks file\
\n -B NUM   Set TAB-completion mode (NUM=[0-3])\
\n -c FILE  Set an alternative main configuration file\
\n -C       Do not clear the screen when changing directories\
\n -d       Print disk usage (free/total)\
\n -D       List directories only\
\n -e       Force the use of the 'o/open' command to open files\
\n -E       Force the use of 'cd' to change directories\
\n -f       Print full directories size (long view mode only)\
\n -F       Disable the files counter (directories)\
\n -g       Print file sizes in powers of 1000 instead of 1024\
\n -G       Print file sizes as used blocks instead of apparent size (used bytes)\
\n -h       Print this help and exit\
\n -H       Disable syntax-highlighting\
\n -i       Enable icons\
\n -I DIR   Set an alternative trash directory\
\n -j FILE  Run the 'p' command on FILE and exit\
\n -J FILE  Run the 'pp' command on FILE and exit\
\n -k FILE  Set an alternative keybindings file\
\n -l       Display extended file metadata (long listing format)\
\n -L       Follow symbolic links when running in long view\
\n -m       Enable fuzzy matching\
\n -M       Disable colors\
\n -n       Disable commands history\
\n -N       Disable bold colors\
\n -o PATH  Set a custom resource opener (instead of the built-in Lira)\
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
\n -T       Do not trim file names\
\n -u       Run in disk usage analyzer mode\
\n -v       Print version information and exit\
\n -V DIR   Set a custom virtual directory\
\n -w NUM   Start in workspace NUM (1-8)\
\n -W       Keep the list of selected files in sight\
\n -x       Run in secure-environment mode\
\n -X       Run in secure-environment mode (paranoid)\
\n -y       Run in light mode\
\n -Y       Run in secure-commands mode\
\n -z NUM   Set a files sorting method (NUM=[1-13])\
\n -Z NUM   List only up to NUM files"
#else
#define SHORT_OPTIONS "\
\n  -a, --show-hidden\t\t Show hidden files\
\n  -A, --no-hidden\t\t Do not show hidden files\
\n  -b, --bookmarks-file=FILE\t Set an alternative bookmarks file\
\n  -c, --config-file=FILE\t Set an alternative configuration file\
\n  -D, --config-dir=DIR\t\t Set an alternative configuration directory\
\n  -e, --no-eln\t\t\t Do not print ELN's (entry list number)\
\n  -E, --eln-use-workspace-color\t ELN's use the current workspace color\
\n  -f, --dirs-first\t\t List directories first (default)\
\n  -F, --no-dirs-first\t\t Do not list directories first\
\n  -g, --pager\t\t\t Enable the pager\
\n  -G, --no-pager\t\t Disable the pager (default)\
\n  -h, --help\t\t\t Show this help and exit\
\n  -H, --horizontal-list\t\t List files horizontally\
\n  -i, --no-case-sensitive\t Ignore case distinctions when listing files (default)\
\n  -I, --case-sensitive\t\t Do not ignore case distinctions when listing files\
\n  -k, --keybindings-file=FILE\t Set an alternative keybindings file\
\n  -l, --long-view\t\t Display extended file metadata (long listing format)\
\n  -L, --follow-symlinks-long\t Follow symbolic links when running in long view\
\n  -m, --dirhist-map\t\t Enable the directory history map\
\n  -o, --autols\t\t\t List files automatically (default)\
\n  -O, --no-autols\t\t Do not list files automatically\
\n  -P, --profile=PROFILE\t\t Use (or create) PROFILE as profile\
\n  -r, --no-refresh-on-empty-line Do not refresh the list of files when pressing Enter \
on an empty line\
\n  -s, --splash\t\t\t Enable the splash screen\
\n  -S, --stealth-mode\t\t Run in incognito/private mode\
\n  -t, --disk-usage-analyzer\t Run in disk usage analyzer mode\
\n  -T, --trash-dir=DIR\t\t Set an alternative trash directory\
\n  -v, --version\t\t\t Show version details and exit\
\n  -w, --workspace=NUM\t\t Start in workspace NUM\
\n  -x, --no-ext-cmds\t\t Disallow the use of external commands\
\n  -y, --light-mode\t\t Run in light mode\
\n  -z, --sort=METHOD\t\t Sort files by METHOD (see the manpage)"

#define LONG_OPTIONS_A "\
\n      --bell=STYLE\t\t Set terminal bell style to: 0 (none), 1 (audible), 2 (visible), 3 (flash)\
\n      --case-sens-dirjump\t Do not ignore case when consulting the jump \
database (via the 'j' command)\
\n      --case-sens-path-comp\t Enable case sensitive path completion\
\n      --cd-on-quit\t\t Enable cd-on-quit functionality (see the manpage)\
\n      --color-scheme=NAME\t Use color scheme NAME\
\n      --color-links-as-target\t Use the target file color for symbolic links\
\n      --cwd-in-title\t\t Print current directory in the terminal window title\
\n      --data-dir=PATH\t\t Use PATH as data directory\
\n      --desktop-notifications\t Enable desktop notifications\
\n      --disk-usage\t\t Show disk usage (free/total)\
\n      --full-dir-size\t\t Print the size of directories and their contents \
(long view only)\
\n      --fuzzy-algo\t\t Set fuzzy algorithm for fuzzy matching to '1' or '2'\
\n      --fuzzy-matching\t\t Enable fuzzy TAB completion/suggestions for file names \
and paths\
\n      --fzfpreview-hidden\t Enable file previews for TAB completion (fzf mode only) with the preview window hidden (toggle with Alt-p)\
\n      --fzftab\t\t\t Use fzf to display completion matches (default if fzf binary is found)\
\n      --fnftab\t\t\t Use fnf to display completion matches\
\n      --icons\t\t\t Enable icons\
\n      --icons-use-file-color\t Icon colors follow file colors\
\n      --int-vars\t\t Enable internal variables\
\n      --list-and-quit\t\t List files and quit\
\n      --ls\t\t\t Short for --list-and-quit\
\n      --lscolors\t\t Read file colors from LS_COLORS\
\n      --max-dirhist=NUM\t\t Maximum number of visited directories to recall\
\n      --max-files=NUM\t\t List only up to NUM files\
\n      --max-path=NUM\t\t Abbreviate current directory in prompt after NUM characters \
(if \\z is used in the prompt line)\
\n      --mnt-udisks2\t\t Use 'udisks2' instead of 'udevil' for the 'media' command\
\n      --no-apparent-size\t Inform file sizes as used blocks instead of used bytes (apparent size)\
\n      --no-bold\t\t\t Disable bold colors (applies to all color schemes)\
\n      --no-cd-auto\t\t Disable the autocd function\
\n      --no-classify\t\t Do not append file type indicators\
\n      --no-clear-screen\t\t Do not clear the screen when listing files\
\n      --no-color\t\t Disable colors \
\n      --no-columns\t\t Disable columned files listing\
\n      --no-dir-jumper\t\t Disable the directory jumper function\
\n      --no-file-cap\t\t Do not check file capabilities when listing files\
\n      --no-file-ext\t\t Do not check file extensions when listing files\
\n      --no-files-counter\t Disable the files counter for directories\
\n      --no-follow-symlink\t Do not follow symbolic links when listing files (overrides -L and --color-links-as-target)\
\n      --no-fzfpreview\t\t Disable file previews for TAB completion (fzf mode only)\
\n      --no-highlight\t\t Disable syntax highlighting\
\n      --no-history\t\t Do not write commands into the history file\
\n      --no-open-auto\t\t Same as no-cd-auto, but for files\
\n      --no-refresh-on-resize\t Do not attempt to refresh the files list upon window's resize\
\n      --no-restore-last-path\t Do not record the last visited directory\
\n      --no-suggestions\t\t Disable auto-suggestions\
\n      --no-tips\t\t\t Disable startup tips\
\n      --no-trim-names\t\t Do not trim file names\
\n      --no-warning-prompt\t Disable the warning prompt\
\n      --no-welcome-message\t Disable the welcome message\
\n      --only-dirs\t\t List only directories and symbolic links to directories\
\n      --open=FILE\t\t Open FILE (via Lira) and exit\
\n      --opener=APPLICATION\t Resource opener to use instead of Lira, \
CliFM's built-in opener\
\n      --preview=FILE\t\t Display a preview of FILE (via Shotgun) and exit\
\n      --print-sel\t\t Keep the list of selected files in sight\
\n      --prop-fields=FORMAT\t Set a custom format string for the long view (see \
PropFields in the config file)\
\n      --ptime-style=STYLE\t Time/date style used by the 'p/pp' command (see PTimeStyle in the config file)\
\n      --readonly\t\t Disable internal commands able to modify the file system\
\n      --rl-vi-mode\t\t Set readline to vi editing mode (defaults to emacs mode)\n"

#define LONG_OPTIONS_B "\
      --secure-cmds\t\t Filter commands to prevent command injection\
\n      --secure-env\t\t Run in a sanitized environment (regular mode)\
\n      --secure-env-full\t\t Run in a sanitized environment (full mode)\
\n      --sel-file=FILE\t\t Set FILE as custom selections file\
\n      --share-selbox\t\t Make the Selection Box common to different profiles\
\n      --shotgun-file=FILE\t Set FILE as shotgun configuration file\
\n      --si\t\t\t Print sizes in powers of 1000 instead of 1024\
\n      --smenutab\t\t Use smenu to display completion matches\
\n      --sort-reverse\t\t Sort in reverse order, e.g. z-a instead of a-z\
\n      --stat FILE...\t\t Run the 'p' command on FILE(s) and exit\
\n      --stat-full FILE...\t Run the 'pp' command on FILE(s) and exit\
\n      --stdtab\t\t\t Force the use of the standard TAB completion mode (readline)\
\n      --time-style=STYLE\t Time/date style used in long view (see TimeStyle in the config file)\
\n      --trash-as-rm\t\t The 'r' command executes 'trash' instead of \
rm(1) to prevent accidental deletions\
\n      --virtual-dir-full-paths\t Files in virtual directories are listed as full paths instead of target base names\
\n      --virtual-dir=PATH\t Absolute path to a directory to be used as virtual directory\
\n      --vt100\t\t\t Run in vt100 compatibility mode\n"
#endif /* _BE_POSIX */

#define CLIFM_COMMANDS_HEADER "\
For a complete description of all the below \
commands run 'cmd' (or press F2) or consult the manpage (F1).\n\
You can also try the interactive help plugin (it depends on FZF): just \
enter 'ih', that's it.\n\
Help topics are available as well. Type 'help <TAB>' to get a list of topics.\n\n\
The following is just a list of available commands and a brief description.\n\
For more information about a specific command run 'CMD -h' or 'CMD --help'.\n"

#define CLIFM_COMMANDS "\
 ELN/FILE/DIR       Auto-open/autocd files/directories\n\
 /PATTERN           Search for files\n\
 ;[CMD], :[CMD]     Run CMD via the system shell\n\
 ac, ad             (De)archive files\n\
 acd, autocd        Set auto-cd on/off\n\
 actions            Manage actions/plugins\n\
 alias              Manage aliases\n\
 ao, auto-open      Set auto-open on/off\n\
 b, back            Go back in the directory history list\n\
 bb, bleach         Clean up non-ASCII file names\n\
 bd                 Go back to a parent directory\n\
 bl                 Create symbolic links in bulk\n\
 bm, bookmarks      Manage bookmarks\n\
 br, bulk           Rename files in bulk\n\
 c, l, m, md, r     Copy, link, move, makedir, and remove\n\
 colors             List current file type colors\n\
 cd                 Change directory\n\
 cl, columns        Set columns on/off\n\
 cmd, commands      Jump to the COMMANDS section in the manpage\n\
 config             Open/edit the main configuration file\n\
 cs, colorscheme    Switch/edit color schemes\n\
 d, dup             Duplicate files\n\
 dh                 Access the directory history list\n\
 ds, desel          Deselect selected files\n\
 exp                Export file names to a temporary file\n\
 ext                Set external/shell commands on/off\n\
 f, forth           Go forth in the directory history list\n\
 fc                 Set the files counter on/off\n\
 ff, dirs-first     Toggle list-directories-first on/off\n\
 fs                 What is free software?\n\
 ft, filter         Set a files filter\n\
 fz                 Print directories full size (long view mode only)\n\
 hh, hidden         Toggle hidden files on/off\n\
 history            Manage the commands history\n\
 icons              Set icons on/off\n\
 k                  Toggle follow-links (long view only)\n\
 kk                 Toggle max-filename-len\n\
 j                  Jump to a visited directory\n\
 kb, keybinds       Manage keybindings\n\
 le                 Edit symbolic link target\n\
 ll, lv             Toggle long view mode on/off\n\
 lm                 Toggle the light mode on/off\n\
 log                Manage your logs\n\
 media              (Un)mount storage devices\n\
 mf                 Limit the number of listed files\n\
 mm, mime           Manage default opening applications\n\
 mp, mountpoints    Change to a mountpoint\n\
 msg, messages      Print system messages\n\
 n, new             Create new files/directories\n\
 net                Manage network/remote resources\n\
 o, open            Open a file\n\
 oc                 Change files ownership interactively\n\
 ow                 Open a file with ...\n\
 opener             Set a custom resource opener\n\
 p, pp, prop        Print files properties\n\
 pc                 Change files permissions interactively\n\
 pf, profile        Manage profiles\n\
 pg, pager          Set the files pager on/off\n\
 pin                Pin a directory\n\
 prompt             Switch/edit the prompt\n\
 q, quit, exit      Quit clifm\n\
 Q                  CD on quit\n\
 rf, refresh        Refresh/clear the screen\n\
 rl, reload         Reload the main configuration file\n\
 rr                 Remove files in bulk\n\
 s, sel             Select files\n\
 sb, selbox         Access the Selection Box\n\
 splash             Print the splash screen\n\
 st, sort           Change files sorting order\n\
 stats              Print files statistics\n\
 t, trash           Send files to the trash can\n\
 tag                Tag files\n\
 te                 Toggle the executable bit on files\n\
 tips               Print tips\n\
 u, undel, untrash  Restore trashed files (via a menu)\n\
 unpin              Unpin the pinned directory\n\
 vv                 Copy and rename files at once\n\
 ver, version       Print version information\n\
 view               Preview files in the current directory\n\
 ws                 Switch workspaces\n\
 x, X               Launch a new instance of clifm (as root if 'X')\n\n\
 Shell-builtins implementations\n\
 export             Export variables to the environment\n\
 pwd                Print the current working directory\n\
 umask              Print/set umask\n\
 unset              Remove variables from the environment\n"

#define CLIFM_KEYBOARD_SHORTCUTS "DEFAULT KEYBOARD SHORTCUTS:\n\n\
 Right, C-f    Accept the entire suggestion\n\
 M-Right, M-f  Accept the first suggested word\n\
 M-c           Clear the current command line buffer\n\
 M-q           Delete the last entered word\n\
 M-g           Toggle list directories first on/off\n\
 M-l           Toggle long/detail view mode on/off\n\
 M-+           Toggle follow links (long view only) on/off\n\
 M-.           Toggle hidden files on/off\n\
 M-,           Toggle list only directories on/off\n\
 M--           Preview files in the current directory (requires fzf)\n\
 M-m           List mountpoints\n\
 M-h           Show directory history\n\
 M-t           Clear messages\n\
 C-l           Clear the screen\n\
 C-y           Copy the current line buffer to the clipboard\n\
 M-s           Open the Selection Box\n\
 M-a           Select all files in the current working directory\n\
 M-d           Deselect all selected files\n\
 M-r           Change to the root directory\n\
 M-e, Home     Change to the home directory\n\
 M-u, S-Up     Change to the parent directory\n\
 M-j, S-Left   Change to previous visited directory\n\
 M-k, S-Right  Change to next visited directory\n\
 M-o           Lock terminal\n\
 M-p           Change to pinned directory\n\
 M-v           Toggle prepend sudo\n\
 M-0           Run the files pager\n\
 M-[1-4]       Switch to workspace 1-4\n\
 C-M-o         Switch to previous profile\n\
 C-M-p         Switch to next profile\n\
 C-M-a         Archive selected files\n\
 C-M-e         Export selected files\n\
 C-M-r         Rename selected files\n\
 C-M-d         Remove selected files\n\
 C-M-t         Trash selected files\n\
 C-M-u         Restore trashed files\n\
 C-M-g         Open/change-into last selected file/directory\n\
 C-M-n         Move selected files into the current working directory\n\
 C-M-v         Copy selected files into the current working directory\n\
 C-M-l         Toggle max name length on/off\n\
 M-y           Toggle light mode on/off\n\
 M-z           Switch to previous sorting method\n\
 M-x           Switch to next sorting method\n\
 C-x           Launch a new instance\n\
 F1            Manual page\n\
 F2            Commands help\n\
 F3            Keybindings help\n\
 F6            Open the MIME list file\n\
 F7            Open the shotgun configuration file\n\
 F8            Open the current color scheme file\n\
 F9            Open the keybindings file\n\
 F10           Open the configuration file\n\
 F11           Open the bookmarks file\n\
 F12           Quit\n\n\
NOTE: C stands for Ctrl, S for Shift, and M for Meta (Alt key in \
most keyboards)\n"

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
b | Shift-left | Alt-j   Go back in the directory history list\n\
f | Shift-right | Alt-k  Go forth in the directory history list\n\
.. | Shift-up | Alt-u    Change to the parent directory\n\
bd media                 Change to the parent directory matching 'media'\n\
j <TAB> | dh <TAB>       Navigate the directory history list\n\
j xproj                  Jump to the best ranked directory matching 'xproj'\n\
bm | b:<TAB> | Alt-b     List bookmarks\n\
bm mybm | b:mybm         Change to the bookmark named 'mybm'\n\
ws2 | Alt-2              Switch to the second workspace\n\
mp                       Change to a mountpoint\n\
pin mydir                Pin the directory 'mydir'\n\
,                        Change to pinned directory\n\
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
ow 10 | ow 10 <TAB>  Choose opening application for the file whose ELN is 10\n\
p 4                  Print the properties of the file whose ELN is 4\n\
/*.png               Search for files ending with .png in the current directory\n\
s *.c                Select all C files\n\
s 1-4 8 19-26        Select multiple files by ELN\n\
sb | s:<TAB>         List currently selected files\n\
ds | ds <TAB>        Deselect a few selected files\n\
ds * | Alt-d         Deselect all selected files\n\
bm add mydir mybm    Bookmark the directory named 'mydir' as 'mybm'\n\
bm del mybm          Remove the bookmark named 'mybm'\n\
tag --help           Learn about file tags\n\
n myfile             Create a new file named 'myfile'\n\
n mydir/             Create a new directory named 'mydir'\n\
c sel                Copy selected files into the current directory (1)\n\
r sel                Remove all selected files (1)\n\
br sel               Bulk rename selected files (1)\n\
c 34 file_copy       Copy the file whose ELN is 34 as 'file_copy' in the CWD\n\
cr myfile            Copy 'myfile' to a remote location\n\
m 45 3               Move the file whose ELN is 45 to the dir whose ELN is 3\n\
m myfile.txt         Interactively rename 'myfile.txt'\n\
l myfile mylink      Create a symbolic link named 'mylink' pointing to 'myfile'\n\
le mylink            Edit the symbolic link 'mylink'\n\
oc myfile            Edit file ownership of the file 'myfile'\n\
pc myfile            Edit file properties of the file 'myfile'\n\
te *.sh              Toggle the executable bit on all .sh files\n\
t 12-18              Send the files whose ELN's are 12-18 to the trash can\n\
t del | t del <TAB>  Permanently remove trashed files using a menu\n\
t empty              Empty the trash can\n\
u | u <TAB>          Undelete trashed files using a menu\n\
ac sel               Compress/archive selected files (1)\n\n\
(1) 's:' can be used instead of the 'sel' keyword"

#define QUICK_HELP_MISC "\
MISC\n\
----\n\
CMD --help      Get help for command CMD\n\
help <TAB>      List available help topics\n\
F1              Open the manpage\n\
ih              Run the interactive help plugin (requires fzf)\n\
ll | Alt-l      Toggle detail/long view mode\n\
hh | Alt-.      Toggle hidden files\n\
rf | Ctrl-l     Clear the screen (also Enter on empty line)\n\
config | F10    View/edit the configuration file\n\
mm edit | F6    Change default associated applications\n\
kb edit | F9    Edit keybindings\n\
view edit | F7  Change previewing applications\n\
mm info 12      Get MIME information for the file whose ELN is 12\n\
Alt-TAB         Toggle the disk usage analyzer mode\n\
cs              Manage color schemes\n\
Right           Accept the entire suggestion\n\
Alt-f           Accept the first/next word of the current suggestion\n\
pf set test     Change to the profile named 'test'\n\
st size rev     Sort files by size in reverse order\n\
Alt-x | Alt-z   Switch sort order\n\
media           (Un)mount storage devices\n\
net work        Mount the network resource named 'work'\n\
actions         List available actions/plugins\n\
icons on        Enable icons\n\
q | F12         I'm tired, quit\n\
Q               cd on quit (consult the manpage)"

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

#define FREE_SOFTWARE "Excerpt from 'What is Free Software?', by Richard Stallman. \
Source: https://www.gnu.org/philosophy/free-sw.html\n \
\n\"'Free software' means software that respects users' freedom and \
community. Roughly, it means that the users have the freedom to run, \
copy, distribute, study, change and improve the software. Thus, 'free \
software' is a matter of liberty, not price. To understand the concept, \
you should think of 'free' as in 'free speech', not as in 'free beer'. \
We sometimes call it 'libre software', borrowing the French or Spanish \
word for 'free' as in freedom, to show we do not mean the software is \
gratis.\n\
\nWe campaign for these freedoms because everyone deserves them. With \
these freedoms, the users (both individually and collectively) control \
the program and what it does for them. When users don't control the \
program, we call it a 'nonfree' or proprietary program. The nonfree \
program controls the users, and the developer controls the program; \
this makes the program an instrument of unjust power. \n\
\nA program is free software if the program's users have the four \
essential freedoms:\n\n\
- The freedom to run the program as you wish, for any purpose \
(freedom 0).\n\
- The freedom to study how the program works, and change it so it does \
your computing as you wish (freedom 1). Access to the source code is a \
precondition for this.\n\
- The freedom to redistribute copies so you can help your neighbor \
(freedom 2).\n\
- The freedom to distribute copies of your modified versions to others \
(freedom 3). By doing this you can give the whole community a chance to \
benefit from your changes. Access to the source code is a precondition \
for this. \n\
\nA program is free software if it gives users adequately all of these \
freedoms. Otherwise, it is nonfree. While we can distinguish various \
nonfree distribution schemes in terms of how far they fall short of \
being free, we consider them all equally unethical [...]\""

/* Brief commands description */
#define AC_DESC      " (archive/compress files)"
#define ACD_DESC     " (set autocd on/off)"
#define ACTIONS_DESC " (manage actions/plugins)"
#define AD_DESC      " (dearchive/decompress files)"
#define ALIAS_DESC   " (list aliases)"
#define AO_DESC      " (set auto-open on/off)"
#define B_DESC       " (go back in the directory history list)"
#define BD_DESC      " (change to a parent directory)"
#define BL_DESC      " (create symbolic links in bulk)"
#define BB_DESC      " (clean up non-ASCII file names)"
#define BM_DESC      " (manage bookmarks)"
#define BR_DESC      " (rename files in bulk)"
#define C_DESC       " (copy files)"
#define CD_DESC      " (change directory)"
#define CL_DESC      " (set columns on/off)"
#define CMD_DESC     " (jump to the COMMANDS section in the manpage)"
#define COLORS_DESC  " (print currently used file type colors)"
#define CONFIG_DESC  " (edit the main configuration file)"
#define CS_DESC      " (manage color schemes)"
#define CWD_DESC     " (print the current directory)"
#define D_DESC       " (duplicate files)"
#define DH_DESC      " (query the directory history list)"
#define DS_DESC      " (deselect files)"
#define EDIT_DESC    " (edit the main configuration file)"
#define EXP_DESC     " (export file names to a temporary file)"
#define EXT_DESC     " (set external/shell commands on/off)"
#define F_DESC       " (go forth in the directory history list)"
#define FC_DESC      " (set the files counter on/off)"
#define FF_DESC      " (toggle list-directories-first on/off)"
#define FS_DESC      " (what is free software?)"
#define FT_DESC      " (set a files filter)"
#define FZ_DESC      " (print directories full size - long view only)"
#define HF_DESC      " (toggle show-hidden-files on/off)"
#define HIST_DESC    " (manage the commands history)"
#define ICONS_DESC   " (set icons on/off)"
#define J_DESC       " (jump to a visited directory)"
#define K_DESC       " (toggle follow-links - long view only)"
#define KK_DESC      " (toggle max-filename-len)"
#define KB_DESC      " (manage keybindings)"
#define L_DESC       " (create a symbolic link)"
#define LE_DESC      " (edit a symbolic link)"
#define LL_DESC      " (toggle long view on/off)"
#define LM_DESC      " (toggle light mode on/off)"
#define LOG_DESC     " (manage logs)"
#define M_DESC       " (move files)"
#define MD_DESC      " (create directories)"
#define MEDIA_DESC   " (mount/unmount storage devices)"
#define MF_DESC      " (limit the number of listed files)"
#define MM_DESC      " (manage default opening applications)"
#define MP_DESC      " (change to a mountpoint)"
#define MSG_DESC     " (print system messages)"
#define N_DESC       " (create files)"
#define NET_DESC     " (manage remote resources)"
#define O_DESC       " (open file)"
#define OC_DESC      " (change files ownership)"
#define OPENER_DESC  " (set a custom resource opener)"
#define OW_DESC      " (open file with...)"
#define P_DESC       " (print files properties)"
#define PC_DESC      " (change files permissions)"
#define PF_DESC      " (manage profiles)"
#define PG_DESC      " (set the files pager on/off)"
#define PIN_DESC     " (pin a directory)"
#define PP_DESC      " (print files properties - follow links/full dir size)"
#define PROMPT_DESC  " (switch/edit prompt)"
#define Q_DESC       " (quit)"
#define QU_DESC      " (exit - cd on quit)"
#define R_DESC       " (remove files)"
#define RF_DESC      " (refresh/clear the screen)"
#define RL_DESC      " (reload the configuration file)"
#define RR_DESC      " (remove files in bulk)"
#define SB_DESC      " (access the selection box)"
#define SEL_DESC     " (select files)"
#define SPLASH_DESC  " (print the splash screen)"
#define ST_DESC      " (change files sorting order)"
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
#define X_DESC       " (launch a new instance of clifm)"
#define XU_DESC      " (launch a new instance of clifm as root)"

#endif /* MESSAGES_H */
