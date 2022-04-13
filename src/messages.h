/* help.h - Usage messages for CliFM */

/*
 * This file is part of CliFM
 *
 * Copyright (C) 2016-2022, L. Abramovich <johndoe.arch@outlook.com>
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

/* Usage messages*/
#define GRAL_USAGE "[-aAefFgGhHiIlLmoOsStuUvxy] [-b FILE] [-c FILE] [-D DIR] \
[-k FILE] [-P PROFILE] [-z METHOD] [PATH]"

#define ACTIONS_USAGE "List or edit actions/plugins\n\
Usage:\n\
  actions [edit [APP]]\n\n\
Examples:\n\
- List available actions/plugins:\n\
    actions\n\
- Open/edit the configuration file with nano:\n\
    actions edit nano\n\
- Open/edit the configuration file with the default associated application:\n\
    actions edit"

#define ALIAS_USAGE "List, print, or import aliases\n\
Usage:\n\
  alias [import FILE] [ls, list] [NAME]\n\n\
Example:\n\
- List available aliases:\n\
    alias\n\
  or\n\
    alias ls\n\
- Print a specific alias:\n\
    alias my_alias\n\
- Import aliases from ~/.bashrc:\n\
    alias import ~/.bashrc"

#define ARCHIVE_USAGE "Compress/archive files\n\
Usage:\n\
  ac, ad ELN/FILE ...\n\n\
Examples:\n\
- Compress/archive all selected files:\n\
    ac sel\n\
- Compress/archive a range of files:\n\
    ac 12-24\n\
- Decompress/dearchive a file:\n\
    ad file.tar.gz"

#define AUTOCD_USAGE "Turn autocd on-off\n\
Usage:\n\
  acd, autocd [on, off, status]"

#define AUTO_OPEN_USAGE "Turn auto-open on-off\n\
Usage:\n\
  ao, auto-open [on, off, status]"

#define BACK_USAGE "Change to the previous directory in the directory \
history list\n\
Usage:\n\
  b, back [h, hist] [clear] [!ELN]\n\n\
Examples:\n\
- Just change to the previously visited directory:\n\
    b (Alt-j or Shift-Left also work)\n\
- Print the directory history list:\n\
    bh\n\
- Change to the directory whose ELN in the list is 24:\n\
    b !24"

#define BD_USAGE "Quickly change to a parent directory matching NAME. If \
NAME is not specified, print the list of all parent directories\n\
Usage:\n\
  bd [NAME]\n\n\
Example:\n\
- Supposing you are in ~/Documents/misc/some/deep/folder, change to ~/Documents/misc:\n\
    bd mi"

#define BL_USAGE "Create multiple symbolic links at once\n\
Usage:\n\
  bl FILE(s)\n\n\
Example:\n\
- Symlink files file1 file2 file3 and file4 at once:\n\
    bl file*"

#define BLEACH_USAGE "Clean up file names from non-ASCII characters\n\
Usage:\n\
  bb, bleach FILE(s)\n\n\
Example:\n\
- Bleach file names in your Downloads directory:\n\
    bb ~/Downloads/*"

#define BOOKMARKS_USAGE "Handle bookmarks\n\
Usage:\n\
  bm, bookmarks [a, add FILE] [d, del] [edit [APP]]\n\n\
Examples:\n\
- Open the bookmarks screen:\n\
    bm (Alt-b)\n\
- Bookmark the directory /media/mount:\n\
    bm a /media/mount\n\
  Note: Make sure to create a simple shortcut, like 'mnt'. Then you can change to it as follows:\n\
    bm mnt\n\
- Remove a bookmark:\n\
    bm d mnt\n\
- Edit the bookmarks file manually:\n\
    bm edit (or F11)\n\
- Edit the bookmarks file using vi:\n\
    bm edit vi"

#define BULK_USAGE "Bulk rename files\n\
Usage:\n\
  br, bulk ELN/FILE ...\n\n\
Examples:\n\
- Bulk rename all files ending with .pdf in the current directory:\n\
    br *.pdf\n\
- Bulk rename all selected files:\n\
    br sel"

#define CD_USAGE "Change the current working directory\n\
Usage:\n\
  cd [ELN/DIR]\n\n\
Examples:\n\
- Change to /var:\n\
    cd /var\n\
  Or, if autocd is enabled (default):\n\
    /var"

#define COLORS_USAGE "Print the list of currently used color codes\n\
Usage:\n\
  cc, colors"

#define COLUMNS_USAGE "Set columned list of files on-off\n\
Usage:\n\
  cl, columns [on, off]"

#define CS_USAGE "Switch color schemes\n\
Usage:\n\
  cs, colorschemes [edit [APP]] [COLORSCHEME]\n\n\
Examples:\n\
- Edit the current color scheme:\n\
    cs edit\n\
- Edit the current color scheme using vi:\n\
    cs edit vi\n\
- Switch to the color scheme named 'mytheme':\n\
    cs mytheme"

#define DESEL_USAGE "Deselect one or more selected files\n\
Usage:\n\
  ds, desel [*, a, all]\n\n\
Examples:\n\
- Deselect all selected files:\n\
    ds * (or Alt-d)\n\
- Deselect files from a menu:\n\
    ds"

#define DIRHIST_USAGE "List or access entries in the directory history list\n\
Usage:\n\
  b/f [hist] [clear] [!ELN]\n\n\
Examples:\n\
- Print the directory history list:\n\
    bh (or 'b hist')\n\
- Change to the directory whose ELN is 12 in the directory history list:\n\
    b !12\n\
- Remove all entries from the directory history list:\n\
    b clear\n"

#define DUP_USAGE "Duplicate files\n\
Usage:\n\
  d, dup FILE(s)\n\n\
Example:\n\
- Duplicate files whose ELN's are 12 through 20:\n\
    d 12-20\n"

#define EDIT_USAGE "Edit the main configuration file\n\
Usage:\n\
  edit [reset] [APPLICATION]\n\n\
Examples:\n\
- Open/edit the configuration file:\n\
    edit (or F10)\n\
- Open/edit the configuration file using nano:\n\
    edit nano\n\
- Create a fresh configuration file (making a backup of the old one):\n\
    edit reset"

#define EXT_USAGE "Turn the use of external commands on-off\n\
Usage:\n\
  ext [on, off, status]"

#define EXPORT_USAGE "Export files to a temporary file\n\
Usage:\n\
  exp [FILE(s)]\n\n\
Examples:\n\
- Export all selected files:\n\
    exp sel\n\
- Export all PDF files in the current directory:\n\
    exp *.pdf"

#define FC_USAGE "Turn the files counter for directories on-off\n\
Usage:\n\
  fc, filescounter [on, off, status]"

#define FF_USAGE "Set list folders first on-off\n\
Usage:\n\
  ff, folders-first [on, off, status]\n\
Example:\n\
- Disable list folders-first:\n\
    ff off\n\
  Note: Toggle folders-first on/off pressing Alt-g"

#define FILTER_USAGE "Set a filter for the files list\n\
Usage:\n\
  ft, filter [unset] [REGEX]\n\n\
Examples:\n\
- Print the current filter, if any:\n\
    ft\n\
- Do not list hidden files:\n\
    ft !^\\.\n\
- Do not list backup files (ending with tilde):\n\
    ft .*~$\n\
- Unset the current filter:\n\
    ft unset"

#define FORTH_USAGE "Change to the next directory in the directory history list\n\
Usage:\n\
  f, forth [h, hist] [clear] [!ELN]\n\n\
Examples:\n\
- Just change to the next visited directory:\n\
    f (Alt-k or Shift-Right also work)\n\
- Print the directory history list:\n\
    fh\n\
- Change to the directory whose ELN in the list is 24:\n\
    f !24"

#define FZ_USAGE "Toggle full directory size on/off (only for long view mode)\n\
Usage:\n\
  fz [on, off]"

#define HF_USAGE "Set hidden files on-off\n\
Usage:\n\
  hf, hidden [on, off, status]\n\
Example:\n\
- Show hidden files:\n\
    hf on\n\
  Note: Press Alt-. to toggle hidden files on/off"

#define HISTEXEC_USAGE "Access commands history entries\n\
Usage: \
!!: Execute the last command. \
!n: Execute the command number 'n' in the history list. \
!-n: Execute the last - n command in the history list."

#define HISTORY_USAGE "List or access commands history entries\n\
Usage:\n\
  history [edit [APP]] [clear] [-n] [on, off, status]\n\n\
Examples:\n\
- Edit the commands history list:\n\
    history edit\n\
- Edit the commands history list using vi\n\
    history edit vi\n\
- Clear the history list:\n\
    history clear\n\
- Print the complete list of commands in history:\n\
    history\n\
- Print the last 4 commands in history:\n\
    history -4\n\n\
You can also access the commands history via the exclamation mark (!):\n\
- Execute the last command: \n\
    !!\n\
- Execute the command number 'n' in the history list:\n\
    !n\n\
- Execute the last - n command in the history list:\n\
    !-n\n\
- Prevent subsequent entries from being written to the history file:\n\
    history off\n\n\
Note: If FZF TAB completion mode is enabled, you can easily navigate the \
commands history list by typing '!' and then pressing TAB"

#define ICONS_USAGE "Set icons on-off\n\
Usage:\n\
  icons [on, off]"

#define JUMP_USAGE "Change to a directory in the jump database\n\
Usage:\n\
  j, jc, jp, jl [STRING ...], jo [NUM], je\n\
Note: Consult the manpage to know how Kangaroo, this directory jumper, works.\n\n\
Examples:\n\
- Print the list of entries in the jump database (visited directories):\n\
    j (or jl)\n\
- Change to any visited directory containing the string 'bui':\n\
    j bui\n\
- If not enough, use multiple query strings:\n\
    j ho bui\n\
    Note: Most likey, this will take you to /home/build\n\
- Change to any visited directory that is PARENT of the current directory and contains the string 'str':\n\
    jp str\n\
- Change to any visited directory that is CHILD of the current directory and contains the string 'str':\n\
    jc str\n\
- Open/Edit the jump database:\n\
    je"

#define KB_USAGE "Handle keybindings\n\
Usage:\n\
  kb, keybinds [edit [APP]] [reset] [readline]\n\n\
Examples:\n\
- List your current key bindings:\n\
    kb\n\
- Open/edit the key bindings file:\n\
    kb edit\n\
- Open/edit the key bindings file using mousepad:\n\
    kb edit mousepad\n\
- List the current key bindings for readline:\n\
    kb readline\n\
- Reset your key bindings settings:\n\
    kb reset"

#define LE_USAGE "Edit a symbolic link\n\
Usage:\n\
  le SYMLINK\n\n\
Example:\n\
- Edit the symbolic link named file.link:\n\
    le file.link"

#define LM_USAGE "Set light mode on-off\n\
Usage:\n\
  lm [on, off]"

#define LOG_USAGE "List or clear CliFM logs\n\
Usage:\n\
  log [clear]"

#define MEDIA_USAGE "List available media devices, allowing to mount or \
unmount them\n\n\
Usage:\n\
  media\n\n\
The list of mounted and unmounted devices will be displayed. Choose the \
device you want. If mounted, it will be unmounted; if unmounted, it will \
be mounted.\n\
To get information about a device, enter iELN. For example: i12"

#define MF_USAGE "Limit the amount of files listed on the screen to NUM \
(valid range: >= 0). Use 'unset' to remove the files limit.\n\
Usage:\n\
  mf [NUM, unset]"

#define MIME_USAGE "Set default opening applications based on MIME types or file names\n\
Usage:\n\
  mm, mime [info ELN/FILENAME] [edit [APP]] [import]\n\n\
Examples:\n\
- Get MIME information for the file whose ELN is 12:\n\
    mm info 12\n\
- Open/edit the MIME configuration file:\n\
    mm edit (or F6)\n\
- Open/edit the MIME configuration file using vim:\n\
    mm edit vim\n\
- Try to import MIME file associations from the system:\n\
    mm import\n\
- Add/modify default opening application for myfile\n\
    1) Find out the MIME type (or file name) of the file\n\
      mm info myfile\n\
    2) Edit the mimelist file:\n\
      mm edit (or F6)\n\
    Once in the file, find the appropriate entry and add the opening application you want\n\
  For more information consult the manpage"

#define MSG_USAGE "List available CliFM messages\n\
Usage:\n\
  msg, messages [clear]\n\n\
Examples:\n\
- List available messages:\n\
    msg\n\
- Clear the current list of messages:\n\
    msg clear (or Alt-t)"

#define MOUNTPOINTS_USAGE "List and change to a mountpoint\n\
Usage:\n\
  mp, mountpoints\n\n\
Example:\n\
- List available mountpoints:\n\
    mp\n\
  Once here, just select the mountpoint you want to change to"

#define NET_USAGE "Handle network resources\n\
Usage:\n\
  net [NAME] [edit [APP]] [m, mount NAME] [u, unmount NAME]\n\n\
Examples:\n\
- List available remote resources (like SSHFS or samba):\n\
    net\n\
- Mount the remote resource named 'work'\n\
    net work (or 'net m work' or 'net mount work')\n\
- Unmount the remote resource named 'work'\n\
    net u work (or 'net unmount work')\n\
- Open/edit the net configuration file:\n\
    net edit\n\
- Open/edit the net configuration file using nano:\n\
    net edit nano"

#define NEW_USAGE "Create a new file or directory\n\
Usage:\n\
  n, new [FILE DIR/ ...n]\n\n\
Examples:\n\
- Create two files named file1 and file2:\n\
    n file1 file2\n\
- Create two directories named dir1 and dir2:\n\
    n dir1/ dir2/\n\
    Note: Note the ending slashes\n\
- Both of the above at once:\n\
    n file1 file2 dir1/ dir2/"

#define OPEN_USAGE "Open a file\n\
Usage:\n\
  o, open ELN/FILE [APPLICATION]\n\n\
Examples:\n\
- Open the file whose ELN is 12 with the default associated application (see the mime command):\n\
    o 12\n\
- Open the file whose ELN is 12 with vi:\n\
    o 12 vi\n\
  Note: If auto-open is enabled (default), 'o' could be just omitted:\n\
    12\n\
    12 vi"

#define OW_USAGE "Open a file with a specific application\n\
Usage:\n\
  ow ELN/FILE\n\n\
Example:\n\
- Choose opening application for test.c from a menu:\n\
    ow test.c\n\
- Open the file test.c with geany:\n\
    ow test.c geany\n\
   Note: Type 'ow test.c' and then press TAB to get a list of applications able to open this file"

#define OPENER_USAGE "Set the resource opener\n\
Usage:\n\
  opener APPLICATION\n\n\
Example:\n\
- Set the resources opener to xdg-open (instead of the default, Lira):\n\
    opener xdg-open\n\
- Set the resources opener back to the default (Lira):\n\
    opener default"

#define PAGER_USAGE "Set the files list pager on-off\n\
Usage:\n\
  pg, pager [on, off, status]"

#define PIN_USAGE "Pin a file or directory\n\
Usage:\n\
  pin FILE/DIR\n\n\
Examples:\n\
- Pin the directory ~/my_important_dir:\n\
    pin ~/my_important_dir\n\
- Change to the pinned directory:\n\
    , (yes, just a comma)\n\
- Unpin the currently pinned directory:\n\
    unpin"

#define PROFILES_USAGE "Manage profiles\n\
Usage:\n\
  pf, prof, profile [ls, list] [set, add, del PROFILE]\n\n\
Examples:\n\
- Print the current profile name:\n\
    pf\n\
- List available profiles:\n\
    pf ls\n\
- Set profile to the profile named myprofile:\n\
    pf set myprofile\n\
- Add a new profile named new_profile:\n\
    pf add new_profile\n\
- Remove the profile named myprofile:\n\
    pf del myprofile"

#define PROP_USAGE "Print files properties\n\
Usage:\n\
  p, pr, pp, prop [ELN/FILE ... n]\n\n\
Examples:\n\
- Print the properties of the file whose ELN is 12:\n\
    p 12\n\
- Print the properties of all selected files:\n\
    p sel\n\
- Print the properties of the directory 'dir' (including its total size):\n\
    pp dir"

#define RR_USAGE "Remove files in bulk using a text editor\n\
Usage:\n\
  rr [DIR] [EDITOR]\n\n\
The list of files in DIR (current directory if omitted) is opened via \
EDITOR (default associated application if omitted). Remove the lines \
corresponding to the files you want to delete, save, and exit the editor.\n\n\
Examples:\n\
- Bulk remove files/dirs in the current directory using the default editor:\n\
    rr\n\
- Bulk remove files/dirs in the current directory using nano:\n\
    rr nano\n\
- Bulk remove files/dirs in mydir/ using vi:\n\
    rr mydir vi"

#define SEL_USAGE "Select one or multiple files\n\
Usage:\n\
  s, sel ELN/FILE... [[!]PATTERN] [-FILETYPE] [:PATH]\n\n\
Examples:\n\
- Select the file whose ELN is 12:\n\
    s 12\n\
- Select all files ending with .odt:\n\
    s *.odt\n\
- Select multiple files at once:\n\
    s 12 15-21 *.pdf\n\
- Select all regular files in /etc starting with 'd':\n\
    s ^d.* -r :/etc\n\
- Select all files in the current directory (including hidden files):\n\
    s * .* (or Alt-a)\n\
- List currently selected files:\n\
    sb\n\
- Copy selected files into the current directory:\n\
    c sel\n\
- Move selected files into the directory whose ELN is 24:\n\
    m sel 24\n\
- Deselect all selected files\n\
    ds * (or Alt-d)"

#define SORT_USAGE "Change sort method for the files list\n\
Usage:\n\
  st [METHOD] [rev]\nMETHOD: 0 = none, \
1 = name, 2 = size, 3 = atime, 4 = btime, \
5 = ctime, 6 = mtime, 7 = version, 8 = extension, \
9 = inode, 10 = owner, 11 = group\n\
Note: Both numbers and names are allowed\n\n\
Examples:\n\
- List files by size:\n\
    st size\n\
- Revert the current sorting method (i.e. z-a instead of a-z):\n\
    st rev"

#define TAG_USAGE "(Un)tag files and/or directories\n\
Usage:\n\
  tag [ls, list] [new] [rm, remove] [mv, rename] [untag] [merge] [FILE(s)] [[:]TAG]\n\n\
Instead of the long format described above, you can use any of the \
following shortcuts as well:\n\
  ta: Tag files\n\
  td: Delete tag(s)\n\
  tl: List tags or tagged files\n\
  tm: Rename (mv) tag\n\
  tn: Create new tag(s)\n\
  tu: Untag file(s)\n\
  ty: Merge two tags\n\n\
Examples:\n\
- List available tags:\n\
    tl\n\
- Tag all .PNG files in the current directory as both 'images' and 'png':\n\
    ta *.png :images :png\n\
    NOTE: Tags are created if they do not exist\n\
- Tag all selected files as 'special':\n\
    ta sel :special\n\
- List all files tagged as 'work' and all files tagged as 'documents':\n\
    tl work documents\n\
- Rename tag 'documents' as 'docs':\n\
    tm documents docs\n\
- Merge tag 'png' into 'images':\n\
    ty png images\n\
    NOTE: All files tagged as 'png' will be now tagged as 'images', \
and the 'png' tag will be removed\n\
- Remove the tag 'images' (untag all files tagged as 'images'):\n\
    td images\n\
- Untag a few files from the 'work' tag:\n\
    tu :work file1 image.png dir2\n\
Operating on tagged files (t:TAG)\n\
- Print the file properties of all files tagged as 'docs':\n\
    p t:docs\n\
- Remove all files tagged as 'images':\n\
    r t:images\n\
- Run stat(1) over all files tagged as 'work' and all files tagged as 'docs':\n\
    stat t:work t:docs\n\n\
To operate only on some tagged files use TAB as follows:\n\
    t:TAG<TAB>\n\
Mark the files you need, and then press Enter or right"

#define TE_USAGE "Toggle the executable bit on files\n\
Usage:\n\
  te FILE(s)\n\n\
Examples:\n\
- Set the executable bit on all shell scripts in the current directory:\n\
    te *.sh\n\
- Set the executable bit on all selected files:\n\
   te sel"

#define TRASH_USAGE "Send one or multiple files to the trash can\n\
Usage:\n\
  t, tr, trash [ELN/FILE ... n] [ls, list] [clear] [del, rm]\n\n\
Examples:\n\
- Trash the file whose ELN is 12:\n\
    t 12\n\
- Trash all files ending with .sh\n\
    t *.sh\n\
- List currently trashed files:\n\
    t (or 't ls' or 't list')\n\
- Remove/delete trashed files using a menu:\n\
    t del\n\
- Untrash all trashed files (restore them to their original location):\n\
    u *\n\
- Untrash files using a menu:\n\
    u"

#define UNICODE_USAGE "Set unicode on-off\n\
Usage:\n\
  uc, unicode [on, off, status]"

#define UNTRASH_USAGE "Restore files from the trash can\n\
Usage:\n\
  u, undel, untrash [FILE(s)] [*, a, all]\n\n\
Examples:\n\
- Untrash all trashed files (restore them to their original location):\n\
    u *\n\
- Untrash files using a menu:\n\
    u"

#define VV_USAGE "Copy selected files into a directory and rename them at once\n\
Usage:\n\
  vv sel DIR\n\n\
Example:\n\
- Copy selected files to the directory 'mydir' and rename them at once:\n\
    vv sel mydir"

#define WRAPPERS_USAGE "c (v, paste), l, m, md, and r commands are wrappers \
for cp, ln, mv, mkdir, and rm shell commands respectivelly.\n\n\
\n\
c -> cp -iRp\n\
l -> ln -sn\n\
m -> mv -i\n\
md -> mkdir -p\n\
r (for directories) -> rm -dIr (\"rm -r\" on NetBSD and OpenBSD)\n\
r (for non-directories) -> rm -I (\"rm -f\" on NetBSD and OpenBSD)\n\
v, paste FILE -> c FILE .\n\n\
Note: To use different parameters, just invoke the corresponding utility, as usual. For example:\n\
  cp -abf ...\n\n\
Examples:\n\
- Create a copy of file1 named file2\n\
    c file1 file2\n\
- Create a copy of file1 in the directory dir1 named file2\n\
    c file1 dir1/file2\n\
- Copy all selected files into the current directory:\n\
    c sel\n\
- Move all selected files into the directory named testdir:\n\
    m sel testdir\n\
- Remove all selected files:\n\
    r sel\n\
- Rename file1 as file_test:\n\
    m file1 file_test\n\
- Create a symbolic link pointing to the directory whose ELN is 12 named link:\n\
    l 12 link\n\
- Create a directory named mydir:\n\
    md mydir\n\n\
Note: To create files and directories you can use the 'n' command as well\n\
- Edit the symbolic link named mylink:\n\
    le mylink"

#define WS_USAGE "Switch workspaces\n\
Usage:\n\
  ws [NUM, +, -]\n\n\
Examples:\n\
- Switch to the first worksapce\n\
    ws 1 (or Alt-1)\n\
- Switch to the next workspace:\n\
    ws +\n\
- Switch to the previous workspace:\n\
    ws -"

#define X_USAGE "Launch a new instance of CliFM on a new terminal window\n\
Usage:\n\
  x, X [DIR]\n\n\
Examples:\n\
- Open a new instance of CliFM in the current directory:\n\
    x\n\
- Open the directory mydir in a new instance of CliFM:\n\
    x mydir\n\
- Open a new instance of CliFM as root:\n\
    X"

/* Misc messages */
#define PAGER_HELP "?, h: help\nDown arrow, Enter, Space: Advance one line\n\
Page Down: Advance one page\nq: Stop pagging\n"
#define PAGER_LABEL "\x1b[7;97m--Mas--\x1b[0;49m"
#define NOT_AVAILABLE "This feature has been disabled at compile time"
#define STEALTH_DISABLED "This function is disabled in stealth mode"

#ifndef __HAIKU__
#define HELP_MESSAGE "Enter '?' or press F1-F3 for instructions"
#else
#define HELP_MESSAGE "Enter '?' or press F5-F7 for instructions"
#endif

#define SHORT_OPTIONS "\
\n -a, --no-hidden\t\t Do not show hidden files (default)\
\n -A, --show-hidden\t\t Show hidden files\
\n -b, --bookmarks-file=FILE\t Specify an alternative bookmarks file\
\n -c, --config-file=FILE\t\t Specify an alternative configuration file\
\n -D, --config-dir=DIR\t\t Specify an alternative configuration directory\
\n -e, --no-eln\t\t\t Do not print ELN's (entry list number) \
\n -f, --no-folders-first\t\t Do not list folders first\
\n -F, --folders-first\t\t List folders first (default)\
\n -g, --pager\t\t\t Enable the pager\
\n -G, --no-pager\t\t\t Disable the pager (default)\
\n -h, --help\t\t\t Show this help and exit\
\n -H, --horizontal-list\t\t List files horizontally\
\n -i, --no-case-sensitive\t No case-sensitive files listing (default)\
\n -I, --case-sensitive\t\t Case-sensitive files listing\
\n -k, --keybindings-file=FILE\t Specify an alternative keybindings file\
\n -l, --no-long-view\t\t Disable long view mode (default)\
\n -L, --long-view\t\t Enable long view mode\
\n -m, --dihist-map\t\t Enable the directory history map\
\n -o, --no-autols\t\t Do not list files automatically\
\n -O, --autols\t\t\t List files automatically (default)\
\n -p, --path=PATH\t\t Use PATH as CliFM's starting path (deprecated: use positional \
parameters instead)\
\n -P, --profile=PROFILE\t\t Use (or create) PROFILE as profile\
\n -s, --splash \t\t\t Enable the splash screen\
\n -S, --stealth-mode \t\t Leave no trace on the host system (see the manpage)\
\n -t, --disk-usage-analyzer \t Run in disk usage analyzer mode\
\n -u, --no-unicode \t\t Disable Unicode\
\n -U, --unicode \t\t\t Enable Unicode support (default)\
\n -v, --version\t\t\t Show version details and exit\
\n -w, --workspace=NUM\t\t Start in workspace NUM\
\n -x, --no-ext-cmds\t\t Disallow the use of external commands\
\n -y, --light-mode\t\t Enable the light mode\
\n -z, --sort=METHOD\t\t Sort files by METHOD (see the manpage)"

#define LONG_OPTIONS "\
\n     --case-sens-dirjump\t Do not ignore case when consulting the jump \
database (via the 'j' command)\
\n     --case-sens-path-comp\t Enable case sensitive path completion\
\n     --cd-on-quit\t\t Enable cd-on-quit functionality (see the manpage)\
\n     --color-scheme=NAME\t Use color scheme NAME\
\n     --cwd-in-title\t\t Print current directory in terminal window title\
\n     --disk-usage\t\t Show disk usage (free/total)\
\n     --enable-logs\t\t Enable program logs\
\n     --expand-bookmarks\t\t Expand bookmark names into the corresponding \
bookmark paths\
\n     --full-dir-size\t\t Print the size of directories and their contents \
in long view mode\
\n     --fzftab\t\t\t Enable FZF mode for TAB completion\
\n     --icons\t\t\t Enable icons\
\n     --icons-use-file-color\t Icons color follows file color\
\n     --int-vars\t\t\t Enable internal variables\
\n     --list-and-quit\t\t List files and quit. It may be used in conjunction with -p\
\n     --max-dirhist\t\t Maximum number of visited directories to recall\
\n     --max-files=NUM\t\t List only up to NUM files\
\n     --max-path=NUM\t\t Set the maximun number of characters \
after which the current directory in the prompt line will be abreviated to the \
directory base name (if \\z is used in the prompt)\
\n     --no-dir-jumper\t\t Disable the directory jumper function\
\n     --no-cd-auto\t\t Disable the autocd function\
\n     --no-classify\t\t Do not append file type indicators\
\n     --no-clear-screen\t\t Do not clear the screen when listing directories\
\n     --no-color\t\t\t Disable colors \
\n     --no-columns\t\t Disable columned files listing\
\n     --no-control-d-exit\t\t Do not allow exit on EOF (Control-d)\
\n     --no-file-cap\t\t Do not check files capabilities when listing files\
\n     --no-file-ext\t\t Do not check files extension when listing files\
\n     --no-files-counter\t\t Disable the files counter for directories\
\n     --no-follow-symlink\t Do not follow symbolic links when listing files\
\n     --no-highlight\t\t Disable syntax highlighting\
\n     --no-history\t\t Do not write commands into the history file\
\n     --no-open-auto\t\t Same as no-cd-auto, but for files\
\n     --no-restore-last-path\t Save last visited directory to be restored \
in the next session\
\n     --no-suggestions\t\t Disable auto-suggestions\
\n     --no-tips\t\t\t Disable startup tips\
\n     --no-warning-prompt\t Disable the warning prompt\
\n     --no-welcome-message\t Disable the welcome message\
\n     --only-dirs\t\t List only directories and symbolic links to directories\
\n     --open=FILE\t\t Run as a stand-alone resource opener: open FILE and exit\
\n     --opener=APPLICATION\t Resource opener to use instead of 'lira',\
CliFM's built-in opener\
\n     --print-sel\t\t Keep the list of selected files in sight\
\n     --rl-vi-mode\t\t Set readline to vi editing mode (defaults to emacs mode)\
\n     --secure-cmds\t\t Filter commands to prevent command injection\
\n     --secure-env\t\t Run in a sanitized environment (regular mode)\
\n     --secure-env-full\t\t Run in a sanitized environment (full mode)\
\n     --share-selbox\t\t Make the Selection Box common to different profiles\
\n     --si\t\t\t Print sizes in powers of 1000, as defined in the \
International System of Units (SI), instead of 1024 (Linux only)\
\n     --sort-reverse\t\t Sort in reverse order, e.g. z-a instead of a-z \
(default order)\
\n     --trash-as-rm\t\t The 'r' command executes 'trash' instead of \
'rm' to prevent accidental deletions\n"

#define CLIFM_COMMANDS "\
\nBUILT-IN COMMANDS:\n\nFor a complete description of each of these \
commands run 'cmd' (or press F2) or consult the manpage (F1).\n\n\
You can also try the 'ih' action to run the interactive help plugin (it \
depends on FZF). Just enter 'ih', that's it.\n\n\
It is also recommended to consult the project's wiki \
(https://github.com/leo-arch/clifm/wiki)\n\n\
The following is just a brief list of available commands and possible \
parameters.\n\n\
 ELN/FILE/DIR (auto-open and autocd functions)\n\
 /PATTERN [DIR] [-filetype] [-x] (quick search)\n\
 ;[CMD], :[CMD] (run CMD via the system shell)\n\
 ac, ad ELN/FILE ... (archiving functions)\n\
 acd, autocd [on, off, status]\n\
 actions [edit [APP]]\n\
 alias [import FILE] [ls, list] [NAME]\n\
 ao, auto-open [on, off, status]\n\
 b, back [h, hist] [clear] [!ELN]\n\
 bb, bleach ELN/FILE ... (file names cleaner)\n\
 bd [NAME] ... (backdir function)\n\
 bl ELN/FILE ... (batch links)\n\
 bm, bookmarks [a, add PATH] [d, del] [edit] [SHORTCUT or NAME]\n\
 br, bulk ELN/FILE ...\n\
 c, l [e, edit], m, md, r (copy, link, move, makedir, and remove)\n\
 cc, colors\n\
 cd [ELN/DIR]\n\
 cl, columns [on, off]\n\
 cmd, commands\n\
 cs, colorscheme [edit [APP]] [COLORSCHEME]\n\
 d, dup FILE(s)\n\
 ds, desel [*, a, all]\n\
 edit [APPLICATION] [reset]\n\
 exp [ELN/FILE ...]\n\
 ext [on, off, status]\n\
 f, forth [h, hist] [clear] [!ELN]\n\
 fc, filescounter [on, off, status]\n\
 ff, folders-first [on, off, status]\n\
 fs\n\
 ft, filter [unset] [REGEX]\n\
 fz\n\
 hf, hidden [on, off, status]\n\
 history [edit [APP]] [clear] [-n] [on, off, status]\n\
 icons [on, off]\n\
 j, jc, jp, jl [STRING ...] jo [NUM], je (directory jumper function)\n\
 kb, keybinds [edit [APP]] [reset] [readline]\n\
 lm [on, off] (lightmode)\n\
 log [clear]\n\
 mf [NUM, unset] (List up to NUM files)\n\
 mm, mime [info ELN/FILE] [edit] [import] (resource opener)\n\
 mp, mountpoints\n\
 msg, messages [clear]\n\
 n, new FILE DIR/ ...n\n\
 net [NAME] [edit [APP]] [m, mount NAME] [u, unmount NAME]\n\
 o, open [ELN/FILE] [APPLICATION]\n\
 ow [ELN/FILE] [APPLICATION] (open with ...)\n\
 opener [default] [APPLICATION]\n\
 p, pr, pp, prop [ELN/FILE ... n]\n\
 path, cwd\n\
 pf, prof, profile [ls, list] [set, add, del PROFILE]\n\
 pg, pager [on, off, status]\n\
 pin [FILE/DIR]\n\
 q, quit, exit\n\
 Q\n\
 rf, refresh\n\
 rl, reload\n\
 s, sel ELN/FILE... [[!]PATTERN] [-FILETYPE] [:PATH]\n\
 sb, selbox\n\
 splash\n\
 st, sort [METHOD] [rev]\n\
 stats\n\
 t, tr, trash [ELN/FILE ... n] [ls, list] [clear] [del, rm]\n\
 te [FILE(s)]\n\
 tips\n\
 u, undel, untrash [*, a, all]\n\
 uc, unicode [on, off, status]\n\
 unpin\n\
 v, vv, paste sel [DESTINY]\n\
 ver, version\n\
 ws [NUM, +, -] (workspaces)\n\
 x, X [ELN/DIR] (new instance)\n"

#define CLIFM_KEYBOARD_SHORTCUTS "DEFAULT KEYBOARD SHORTCUTS:\n\n\
 Right, C-f: Accept the entire suggestion\n\
 M-Right, M-f: Accept the first suggested word\n\
 M-c: Clear the current command line buffer\n\
 M-g: Toggle list-folders-first on/off\n\
 C-r: Refresh the screen\n\
 M-l: Toggle long view mode on/off\n\
 M-m: List mountpoints\n\
 M-t: Clear messages\n\
 M-h: Show directory history\n\
 M-i, M-.: Toggle hidden files on/off\n\
 M-s: Open the Selection Box\n\
 M-a: Select all files in the current working directory\n\
 M-d: Deselect all selected files\n\
 M-r: Change to the root directory\n\
 M-e, Home: Change to the home directory\n\
 M-u, S-Up: Change to the parent directory\n\
 M-j, S-Left: Change to previous visited directory\n\
 M-k, S-Right: Change to next visited directory\n\
 M-o: Lock terminal\n\
 M-p: Change to pinned directory\n\
 M-v: Toggle prepend sudo\n\
 M-1: Switch to workspace 1\n\
 M-2: Switch to workspace 2\n\
 M-3: Switch to workspace 3\n\
 M-4: Switch to workspace 4\n\
 C-M-j: Change to first visited directory\n\
 C-M-k: Change to last visited directory\n\
 C-M-o: Switch to previous profile\n\
 C-M-p: Switch to next profile\n\
 C-M-a: Archive selected files\n\
 C-M-e: Export selected files\n\
 C-M-r: Rename selected files\n\
 C-M-d: Remove selected files\n\
 C-M-t: Trash selected files\n\
 C-M-u: Restore trashed files\n\
 C-M-b: Bookmark last selected file or directory\n\
 C-M-g: Open/change-into last selected file/directory\n\
 C-M-n: Move selected files into the current working directory\n\
 C-M-v: Copy selected files into the current working directory\n\
 C-M-l: Toggle max name length on/off\n\
 M-y: Toggle light mode on/off\n\
 M-z: Switch to previous sorting method\n\
 M-x: Switch to next sorting method\n\
 C-x: Launch a new instance\n\
 F1: Manual page\n\
 F2: Commands help\n\
 F3: Keybindings help\n\
 F6: Open the MIME list file\n\
 F7: Open the jump database file\n\
 F8: Open the current color scheme file\n\
 F9: Open the keybindings file\n\
 F10: Open the configuration file\n\
 F11: Open the bookmarks file\n\
 F12: Quit\n\n\
NOTE: C stands for Ctrl, S for Shift, and M for Meta (Alt key in \
most keyboards)\n"

#define HELP_END_NOTE "Run the 'colors' or 'cc' command to see the list \
of currently used color codes.\n\n\
The color schemes file, just as the configuration and profile \
files, allow you to customize colors, the prompt string, define \
some prompt and profile commands, aliases, autocommands, and more.\n\
For a full description consult the manpage."

#define ASCII_LOGO "\
                            _______     _ \n\
                           | ,---, |   | |\n\
                           | |   | |   | |\n\
                           | |   | |   | |\n\
                           | |   | |   | |\n\
                           | !___! !___! |\n\
                           `-------------'\n"

#define QUICK_HELP "\
This is only a quick help. For more information and advanced tricks \n\
consult the manpage and/or the Wiki (https://github.com/leo-arch/clifm/wiki)\n\
\n\
NAVIGATION\n\
----------\n\
/etc                     Change the current directory to '/etc'\n\
5                        Change to the directory whose ELN is 5\n\
b | Shift-left | Alt-j   Go back in the directory history list\n\
f | Shift-right | Alt-k  Go forth in the directory history list\n\
dh                       Navigate the directory history list (needs fzf)\n\
.. | Shift-up | Alt-u    Change to the parent directory\n\
bd media                 Change to the parent directory matching 'media'\n\
bm | Alt-b               Open the bookmarks screen\n\
bm mybm                  Change to bookmark named 'mybm'\n\
ws2 | Alt-2              Switch to the second workspace\n\
j xproj                  Jump to the best ranked directory matching 'xproj'\n\
mp                       Change to a mountpoint\n\
pin mydir                Pin the directory 'mydir'\n\
,                        Change to pinned directory\n\
x                        Run new instance in the current directory\n\
-                        Navigate the file system with files preview (needs fzf)\n\
\n\
BASIC FILE OPERATIONS\n\
---------------------\n\
myfile.txt         Open 'myfile.txt' with the default associated application\n\
myfile.txt vi      Open 'myfile.txt' with vi\n\
12                 Open the file whose ELN is 12\n\
12&                Open the file whose ELN is 12 in the background\n\
ow myfile.txt      Choose opening application for 'myfile.txt' from a menu\n\
n myfile           Create a new file named 'myfile'\n\
n mydir/           Create a new directory named 'mydir'\n\
p4                 Print the properties of the file whose ELN is 4\n\
/*.png             Search for files ending with .png in the current directory\n\
s *.c              Select all C files\n\
s 1-4 8 19-26      Select multiple files by ELN\n\
sb                 List currently selected files\n\
ds                 Deselect a few selected files\n\
ds *               Deselect all selected files\n\
c sel              Copy selected files into the current directory\n\
r sel              Remove all selected files\n\
br sel             Bulk rename selected files\n\
d sel              Duplicate all selected files\n\
c 34 file_copy     Copy the file whose ELN is 34 as 'file_copy' in the CWD\n\
d myfile           Duplicate 'myfile' (via rsync)\n\
m 45 3             Move the file whose ELN is 45 to the dir whose ELN is 3\n\
m myfile.txt       Rename 'myfile.txt'\n\
l myfile mylink    Create a symbolic link named 'mylink' to 'myfile'\n\
le mylink          Edit the symbolic link 'mylink'\n\
bl sel             Create symbolic links for all selected files\n\
te *.sh            Toggle the executable bit on all .sh files\n\
t 12-18            Send the files whose ELN's are 12-18 to the trash can\n\
t del              Select trashed files and remove them permanently\n\
u                  Undelete trashed files\n\
bm a mydir         Bookmark the directory named mydir\n\
bm d mybm          Remove the bookmark named 'mybm'\n\
ac sel             Compress/archive selected files\n\
\n\
MISC\n\
----\n\
cmd --help     Get help for command 'cmd'\n\
F1             Open the manpage\n\
ih             Run the interactive help plugin (needs fzf)\n\
edit | F10     View and/or edit the configuration file\n\
Alt-l          Toggle detail/long view mode on/off\n\
Alt-.          Toggle hidden files on/off\n\
kb edit | F9   Edit keybindings\n\
mm edit | F6   Change default associated applications\n\
mm info 12     Get MIME information for the file whose ELN is 12\n\
cs             Manage color schemes\n\
Right          Accept the entire suggestion\n\
Alt-f          Accept the first/next word of the current suggestion\n\
rf | .         Reprint the current list of files\n\
pf set test    Change to the profile named 'test'\n\
st size rev    Sort files by size in reverse order\n\
Alt-x | Alt-z  Toggle sort method\n\
media          (Un)mount storage devices\n\
net work       Mount the network resource named 'work'\n\
actions        List available actions/plugins\n\
icons on       Enable icons\n\
q              I'm tired, quit\n\
Q              cd on quit\n"

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
being free, we consider them all equally unethical (...)\""

#endif /* MESSAGES_H */
