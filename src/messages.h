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
#define GRAL_USAGE "[-aAefFgGhHiIlLmoOsSuUvxy] [-b FILE] [-c FILE] [-D DIR] \
[-k FILE] [-P PROFILE] [-z METHOD] [PATH]"

#define ACTIONS_USAGE "List or edit actions/plugins\n\
Usage: actions [edit [APP]]\n\
Examples:\n\
- List available actions/plugins:\n\
    actions\n\
- Open/edit the configuration file with nano:\n\
    actions edit nano\n\
- Open/edit the configuration file with the default associated application:\n\
    actions edit"

#define ALIAS_USAGE "List or import aliases\n\
Usage: alias [import FILE]\n\
Example:\n\
  alias import ~/.bashrc"

#define ARCHIVE_USAGE "Compress/archive files\n\
Usage: ac, ad ELN/FILE ...\n\
Examples:\n\
- Compress archive all selected files:\n\
    ac sel\n\
- Compress/archive a range of files:\n\
    ac 12-24\n\
- Decompress a file\n\
    ad file.tar.gz"

#define AUTOCD_USAGE "Turn autocd on-off\n\
Usage: acd, autocd [on, off, status]"

#define AUTO_OPEN_USAGE "Turn auto-open on-off\n\
Usage: ao, auto-open [on, off, status]"

#define BACK_USAGE "Change to the previous directory in the directory \
history list\n\
Usage: b, back [h, hist] [clear] [!ELN]\n\
Examples:\n\
- Just change to the previously visited directory:\n\
    b (Alt-j or Shift-Left also work)\n\
- Print the directory history list:\n\
    bh\n\
- Change to directory whose ELN in the list is 24:\n\
    b !24"

#define BD_USAGE "Quickly change to a parent directory matching NAME. If \
NAME is not specified, print the list of all parent directories\n\
Usage: bd [NAME]\n\
Example:\n\
- Supposing you are in ~/Documents/misc/some/deep/folder, change to ~/Documents/misc:\n\
    bd mi"

#define BL_USAGE "Create multiple symbolic links at once\n\
Usage: bl FILE(s)\n\
Example:\n\
- Symlink files file1 file2 file3 and file4 at once:\n\
  bl file*\n\
  Note: Choose a suffix for the new symbolic links"

#define BLEACH_USAGE "Clean up file names from non-ASCII characters\n\
Usage: bb, bleach FILE(s)\n\
Example:\n\
- Bleach file names in your Downloads directory:\n\
    bb ~/Downloads/*"

#define BOOKMARKS_USAGE "Handle bookmarks\n\
Usage: bm, bookmarks [a, add FILE] [d, del] [edit [APP]]\n\
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
Usage: br, bulk ELN/FILE ...\n\
Examples:\n\
- Bulk rename all files ending with .pdf in the current directory:\n\
    br *.pdf\n\
- Bulk rename all selected files:\n\
    br sel"

#define CD_USAGE "Change current working directory\n\
Usage: cd [ELN/DIR]\n\
Examples:\n\
- Change to /var:\n\
    cd /var\n\
  Or, if autocd is enabled (default):\n\
    /var"

#define COLORS_USAGE "Print a list of currently used color codes\n\
Usage: cc, colors"

#define COLUMNS_USAGE "Set columned list of files on-off\n\
Usage: cl, columns [on, off]"

#define CS_USAGE "Switch color schemes\n\
Usage: cs, colorschemes [edit [APP]] [COLORSCHEME]\n\
Examples:\n\
- Edit the current color scheme:\n\
    cs edit\n\
- Edit the current color scheme using vi:\n\
    cs edit vi\n\
- Switch to the color scheme named 'mytheme':\n\
    cs mytheme"

#define DESEL_USAGE "Deselect one or more selected files\n\
Usage: ds, desel [*, a, all]\n\
Examples:\n\
- Deselect all selected files:\n\
    ds * (or Alt-d)\n\
- Deselect files from a menu:\n\
    ds"

#define DIRHIST_USAGE "List or access entries in the directory history list\n\
Usage: b/f [hist] [clear] [!ELN]"

#define DUP_USAGE "Duplicate files\n\
Usage: d, dup FILE(s)\n\
Example:\n\
- Duplicate files whose ELN's are 12 through 20:\n\
    d 12-20\n"

#define EDIT_USAGE "Edit the main configuration file\n\
Usage: edit [reset] [APPLICATION]\n\
Examples:\n\
- Open/edit the configuration file:\n\
    edit\n\
- Open/edit the configuration file using nano:\n\
    edit nano\n\
- Create a fresh configuration file (making abackup of the old one):\n\
    edit reset"

#define EXT_USAGE "Turn the use of external commands on-off\n\
Usage: ext [on, off, status]"

#define EXPORT_USAGE "Export files to a temporary file\n\
Usage: exp [FILE(s)]\n\
Example:\n\
- Export all selected files:\n\
    exp sel\n\
- Export all PDF files in the current directory:\n\
    exp *.pdf"

#define FC_USAGE "Turn the files counter for directories on-off\n\
Usage: fc, filescounter [on, off, status]"

#define FF_USAGE "Set show folders first on-off\n\
Usage: ff, folders-first [on, off, status]"

#define FILTER_USAGE "Set a filter for the files list\n\
Usage: ft, filter [unset] [REGEX]\n\
Example:\n\
- Print the current filter, if any:\n\
    ft\n\
- Prevent hidden files from being listed:\n\
    ft ^.\n\
- Do not list backup files:\n\
    ft .*~$\n\
- Unset the current filter:\n\
    ft unset"

#define FORTH_USAGE "Change to the next directory in the directory history list\n\
Usage: f, forth [h, hist] [clear] [!ELN]\n\
Examples:\n\
- Just change to the next visited directory:\n\
    f (Alt-k or Shift-Right also work)\n\
- Print the directory history list:\n\
    fh\n\
- Change to the directory whose ELN in the list is 24:\n\
    f !24"

#define HF_USAGE "Set hidden files on-off\n\
Usage: hf, hidden [on, off, status]"

#define HISTEXEC_USAGE "Access commands history entries\n\
Usage: \
!!: Execute the last command. \
!n: Execute the command number 'n' in the history list. \
!-n: Execute the last-n command in the history list."

#define HISTORY_USAGE "List or access commands history entries\n\
Usage: history [edit [APP]] [clear] [-n]\n\
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
    history -4"

#define ICONS_USAGE "Set icons on-off\n\
Usage: icons [on, off]"

#define JUMP_USAGE "Change to a directory in the jump database\n\
Usage: j, jc, jp, jl [STRING ...], jo [NUM], je\n\
Note: Consult the manpage to knwo how Kangaroo, this directory jumper, work.\n\
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
Usage: kb, keybinds [edit [APP]] [reset] [readline]\n\
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
Usage: le SYMLINK\n\
Example:\n\
- Edit the symbolic link named file.link:\n\
    le file.link"

#define LM_USAGE "Set light mode on-off\n\
Usage: lm [on, off]"

#define LOG_USAGE "List or clear CliFM logs\n\
Usage: log [clear]"

#define MEDIA_USAGE "List available media devices, allowing to mount or \
unmount them\n\
Usage: media"
#define MF_USAGE "Limit the amount of files listed on the screen to NUM \
(valid range: >= 0). Use 'unset' to remove the files limit.\n\
Usage: mf [NUM, unset]"

#define MIME_USAGE "Set default opening applications based on MIME types\n\
Usage: mm, mime [info ELN/FILENAME] [edit [APP]] [import]\n\
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
    1) Find out the MIME type (or extension) of the file\n\
      mm info myfile\n\
    2) Edit the mimelist file:\n\
      mm edit (or F6)\n\
      Once in the file, find the appropriate entry and add the opening application you want\n\
    For more information consult the manpage"

#define MSG_USAGE "List available CliFM messages\n\
Usage: msg, messages [clear]\n\
Examples:\n\
- List available messages:\n\
    msg\n\
- Clear the current list of messages:\n\
    msg clear (or Alt-t)"

#define MOUNPOINTS_USAGE "List and change to a mountpoint\n\
Usage: mp, mountpoints\n\
Example:\n\
- List available mountpoints:\n\
    mp\n\
    Once here, just select the mountpoint you want to change to"

#define NET_USAGE "Handle network resources\n\
Usage: net [NAME] [edit [APP]] [m, mount NAME] [u, unmount NAME]\n\
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
Usage: n, new [FILE DIR/ ...n]\n\
Examples:\n\
- Create two files named file1 and file2:\n\
    n file1 file2\n\
- Create two directories named dir1 and dir2:\n\
    n dir1/ dir2/\n\
    Note: Note the ending slash\n\
- Both of the above at once:\n\
    n file1 file2 dir1/ dir2/"

#define OPEN_USAGE "Open a file\n\
Usage: o, open ELN/FILE [APPLICATION]\n\
Examples:\n\
- Open the file whose ELN is 12 with the default associated application (see the mime command):\n\
    o 12\n\
- Open the file whose ELN is 12 with vi:\n\
    o 12 vi\n\
  Note: if auto-open is enabled (default), 'o' could be just omitted:\n\
  12\n\
  12 vi"

#define OW_USAGE "Open a file with...\n\
Usage: ow ELN/FILE\n\
Example:\n\
- Open the file test.c with geany:\n\
    ow test.c geany\n\
   Note: type 'ow test.c' and then press TAB to get a list of applications able to open this file"

#define OPENER_USAGE "Set the resource opener\n\
Usage: opener APPLICATION\n\
Example:\n\
- Set the resources opener to xdg-open (instead of the default, Lira):\n\
    opener xdg-open\n\
- Set the resources opener back to the default (Lira):\n\
    opener default"

#define PAGER_USAGE "Set the files list pager on-off\n\
Usage: pg, pager [on, off, status]"
#define PIN_USAGE "Pin a file or directory\n\
Usage: pin FILE/DIR\n\
Example:\n\
- Pin the directory ~/my_important_dir:\n\
    pin ~/my_important_dir\n\
- Change to the pinned directory:\n\
    , (yes, just a comma)\n\
- Unpin the currently pinned directory:\n\
    unpin"

#define PROFILES_USAGE "Manage profiles\n\
Usage: pf, prof, profile [ls, list] [set, add, del PROFILE]"

#define PROP_USAGE "Print files properties\n\
Usage: p, pr, pp, prop [ELN/FILE ... n]\n\
Examples:\n\
- Print the properties of the file whose ELN is 12:\n\
    p 12\n\
- Print the properties of the directory 'dir' (inclusing its total size):\n\
    pp dir"

#define SEL_USAGE "Select one or multiple files\n\
Usage: s, sel ELN/FILE... [[!]PATTERN] [-FILETYPE] [:PATH]\n\
Examples:\n\
- Select the file whose ELN is 12:\n\
    s 12\n\
- Select all files ending with .odt:\n\
    s *.odt\n\
- Select multiple files at once:\n\
    s 12 15-21 *.pdf\n\
- Select all regular files in /etc ending starting with 'd':\n\
    s ^d.* -r :/etc\n\
- Select all files in the current directory (including hidden files):\n\
    s * .*\n\
- List currently selected files:\n\
    sb\n\
- Copy selected files to the current directory:\n\
    c sel\n\
- Move selected files to the directory whose ELN is 24:\n\
    m sel 24\n\
- Deselect all selected files\n\
    ds * (or Alt-d)"

#define SHELL_USAGE "Set the shell used to run external commands\n\
Usage: shell [SHELL]"

#define SORT_USAGE "Change sort method for the files list\n\
Usage: st [METHOD] [rev]\nMETHOD: 0 = none, \
1 = name, 2 = size, 3 = atime, 4 = btime, \
5 = ctime, 6 = mtime, 7 = version, 8 = extension, \
9 = inode, 10 = owner, 11 = group\nBoth numbers and names are allowed\n\
Examples:\n\
- List files by size:\n\
    st size\n\
- Revert the current sorting method (i.e. z-a instead of a-z):\n\
    st rev"

#define TE_USAGE "Toggle the executable bit on files\n\
Usage: te FILE(s)\n\
Example:\n\
- Set the executable bit on all shell scripts in the current directory:\n\
    te *.sh"

#define TRASH_USAGE "Send one or multiple files to the trash can\n\
Usage: t, tr, trash [ELN/FILE ... n] [ls, list] [clear] [del, rm]\n\
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
Usage: uc, unicode [on, off, status]"

#define UNTRASH_USAGE "Restore files from the trash can\n\
Usage: u, undel, untrash [FILE(s)] [*, a, all]\n\
Examples:\n\
- Untrash all trashed files (restore them to their original location):\n\
    u *\n\
- Untrash files using a menu:\n\
    u"

#define VV_USAGE "Copy selected files to a directory and rename them at once\n\
Usage: vv sel DIR\n\
Example:\n\
- Copy selected files to the directory 'mydir' and rename them at once:\n\
    vv sel mydir"

#define WRAPPERS_USAGE "c (v, paste), l, m, md, and r commands are wrappers \
for cp, ln, mv, mkdir, and rm shell commands respectivelly.\n\n\
Without option parameters:\n\
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
- Copy all selected files in the current directory:\n\
    c sel\n\
- Move all selected files to the directory named testdir:\n\
    m sel testdir\n\
- Remove all selected files:\n\
    r sel\n\
- Rename file1 into file_test:\n\
    m file1 file_test\n\
- Create a symbolic link pointing to the directory whose ELN is 12 named link:\n\
    l 12 link\n\
- Create a directory named mydir:\n\
    md mydir\n\
    Note: To create files and directories you can use the 'n' command as well\n\
- Edit the symbolic link named mylink:\n\
    le mylink"

#define WS_USAGE "Switch workspaces\n\
Usage: ws [NUM, +, -]\n\
Examples:\n\
- Switch to the first worksapce\n\
    ws 1 (or Alt-1)\n\
- Switch to the next workspace:\n\
    ws +\n\
- Switch to the previous workspace:\n\
    ws -"

#define X_USAGE "Launch a new instance of CliFM on a new terminal window\n\
Usage: x, X [DIR]\n\
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

#ifndef __HAIKU__
#define HELP_MESSAGE "Enter '?' or press F1-F3 for instructions"
#else
#define HELP_MESSAGE "Enter '?' or press F5-F7 for instructions"
#endif

#endif /* MESSAGES_H */
