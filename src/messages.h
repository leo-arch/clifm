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
Usage: actions [edit [APP]]"
#define ALIAS_USAGE "List or import aliases\n\
Usage: alias [import FILE]"
#define ARCHIVE_USAGE "Compress/archive files\n\
Usage: ac, ad ELN/FILE ..."
#define AUTOCD_USAGE "Turn autocd on-off\n\
Usage: acd, autocd [on, off, status]"
#define AUTO_OPEN_USAGE "Turn auto-open on-off\n\
Usage: ao, auto-open [on, off, status]"
#define BACK_USAGE "Change to the previous directory in the directory \
history list\n\
Usage: b, back [h, hist] [clear] [!ELN]"
#define BD_USAGE "Quickly change to a parent directory matching NAME. If \
NAME is not specified, print the list of all parent directories\n\
Usage: bd [NAME]"
#define BL_USAGE "Create multiple symbolic links at once\n\
Usage: bl FILE(s)"
#define BLEACH_USAGE "Clean up file names from non-ASCII characters\n\
Usage: bb, bleach FILE(s)"
#define BOOKMARKS_USAGE "Handle bookmarks\n\
Usage: bm, bookmarks [a, add FILE] [d, del] [edit]"
#define BULK_USAGE "Bulk rename files\n\
Usage: br, bulk ELN/FILE ..."
#define CD_USAGE "Change current working directory\n\
Usage: cd [ELN/DIR]"
#define COLORS_USAGE "Print a list of currently used color codes\n\
Usage: cc, colors"
#define COLUMNS_USAGE "Set columned list of files on-off\n\
Usage: cl, columns [on, off]"
#define CS_USAGE "Switch color schemes\n\
Usage: cs, colorschemes [edit [APP]] [COLORSCHEME]"
#define DESEL_USAGE "Deselect one or more selected files\n\
Usage: ds, desel [*, a, all]"
#define DIRHIST_USAGE "List or access entries in the directory history list\n\
Usage: b/f [hist] [clear] [!ELN]"
#define DUP_USAGE "Duplicate files\n\
Usage: d, dup FILE(s)"
#define EDIT_USAGE "Edit the main configuration file\n\
Usage: edit [reset] [APPLICATION]"
#define EXT_USAGE "Turn the use of external commands on-off\n\
Usage: ext [on, off, status]"
#define EXPORT_USAGE "Export files to a temporary file\n\
Usage: exp [FILE(s)]"
#define FC_USAGE "Turn the files counter for directories on-off\n\
Usage: fc, filescounter [on, off, status]"
#define FF_USAGE "Set show folders first on-off\n\
Usage: ff, folders-first [on, off, status]"
#define FILTER_USAGE "Set a filter for the files list\n\
Usage: ft, filter [unset] [REGEX]"
#define FORTH_USAGE "Change to the next directory in the directory history list\n\
Usage: f, forth [h, hist] [clear] [!ELN]"
#define HF_USAGE "Set hidden files on-off\n\
Usage: hf, hidden [on, off, status]"
#define HISTEXEC_USAGE "Access commands history entries\n\
Usage: \
!!: Execute the last command. \
!n: Execute the command number 'n' in the history list. \
!-n: Execute the last-n command in the history list."
#define HISTORY_USAGE "List or access commands history entries\n\
Usage: history [edit [APP]] [clear] [-n]"
#define ICONS_USAGE "Set icons on-off\n\
Usage: icons [on, off]"
#define JUMP_USAGE "Change to a directory in the jump database\n\
Usage: j, jc, jp, jl [STRING ...], jo [NUM], je"
#define KB_USAGE "Handle keybindings\n\
Usage: kb, keybinds [edit [APP]] [reset] [readline]"
#define LE_USAGE "Edit a symbolic link\n\
Usage: le SYMLINK"
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
Usage: mm, mime [info ELN/FILENAME] [edit] [import]"
#define MSG_USAGE "List available CliFM messages\n\
Usage: msg, messages [clear]"
#define MOUNPOINTS_USAGE "List and change to a mountpoint\n\
Usage: mp, mountpoints"
#define NET_USAGE "Handle network resources\n\
Usage: net [NAME] [edit [APP]] [m, mount NAME] [u, unmount NAME]"
#define NEW_USAGE "Create a new file or directory\n\
Usage: n, new [FILE DIR/ ...n]"
#define OPEN_USAGE "Open a file\n\
Usage: o, open ELN/FILE [APPLICATION]"
#define OW_USAGE "Open a file with...\n\
Usage: ow ELN/FILE"
#define OPENER_USAGE "Set the resource opener\n\
Usage: opener APPLICATION"
#define PAGER_USAGE "Set the files list pager on-off\n\
Usage: pg, pager [on, off, status]"
#define PIN_USAGE "Pin a file or directory\nUsage: pin FILE/DIR"
#define PROFILES_USAGE "Manage profiles\n\
Usage: pf, prof, profile [ls, list] [set, add, del PROFILE]"
#define PROP_USAGE "Print files properties\n\
Usage: p, pr, pp, prop [ELN/FILE ... n]"
#define SEL_USAGE "Select one or multiple files\n\
Usage: s, sel ELN/FILE... [[!]PATTERN] [-FILETYPE] [:PATH]"
#define SHELL_USAGE "Set the shell used to run external commands\n\
Usage: shell [SHELL]"
#define SORT_USAGE "Change sort method for the files list\n\
Usage: st [METHOD] [rev]\nMETHOD: 0 = none, \
1 = name, 2 = size, 3 = atime, 4 = btime, \
5 = ctime, 6 = mtime, 7 = version, 8 = extension, \
9 = inode, 10 = owner, 11 = group\nBoth numbers and names are allowed"
#define TE_USAGE "Toggle the executable bit on files\n\
Usage: te FILE(s)"
#define TRASH_USAGE "Send one or multiple files to the trash can\n\
Usage: t, tr, trash [ELN/FILE ... n] [ls, list] [clear] [del, rm]"
#define UNICODE_USAGE "Set unicode on-off\n\
Usage: uc, unicode [on, off, status]"
#define UNTRASH_USAGE "Restore files from the trash can\n\
Usage: u, undel, untrash [*, a, all]"
#define VV_USAGE "Copy selected files to a directory and rename them at once\n\
Usage: vv sel DIR"
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
The 'le' command is available to edit symbolic links: le LINK"
#define WS_USAGE "Switch workspaces\n\
Usage: ws [NUM, +, -]"
#define X_USAGE "Launch a new instance of CliFM on a new terminal window\n\
Usage: x, X [DIR]"

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
