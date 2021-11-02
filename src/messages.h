/* help.h - Usage messages for CliFM */

/*
 * This file is part of CliFM
 *
 * Copyright (C) 2016-2021, L. Abramovich <johndoe.arch@outlook.com>
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

#define GRAL_USAGE "[-aAefFgGhHiIlLmoOsSuUvxy] [-b FILE] [-c FILE] [-D DIR] \
[-k FILE] [-P PROFILE] [-z METHOD] [PATH]"

#define ACTIONS_USAGE "List or edit actions/plugins\nUsage: actions [edit]"
#define ALIAS_USAGE "List or import aliases\nUsage: alias [import FILE]"
#define ARCHIVE_USAGE "Compress/archive files\nUsage: ac, ad ELN/FILE ..."
#define AUTOCD_USAGE "Turn autocd on-off\nUsage: acd, autocd [on, off, status]"
#define AUTO_OPEN_USAGE "Turn auto-open on-off\nUsage: ao, auto-open [on, off, status]"
#define BACK_USAGE "Change to the previous directory in the directory history list\nUsage: back, b [h, hist] [clear] [!ELN]"
#define BL_USAGE "Create multiple symbolic links at once\nUsage: bl FILE(s)"
#define BOOKMARKS_USAGE "Handle bookmarks\nUsage: bm, bookmarks [a, add FILE] [d, del] [edit]"
#define BULK_USAGE "Bulk rename files\nUsage: br, bulk ELN/FILE ..."
#define CD_USAGE "Change current working directory\nUsage: cd [ELN/DIR]"
#define COLORS_USAGE "Print a list of currently used color codes\nUsage: cc, colors"
#define COLUMNS_USAGE "Set columned list of files on-off\nUsage: cl, columns [on, off]"
#define CS_USAGE "Switch color schemes\nUsage: cs, colorschemes [edit] [COLORSCHEME]"
#define DESEL_USAGE "Deselect one or more selected files\nUsage: desel, ds [*, a, all]"
#define DIRHIST_USAGE "List or access entries in the directory history list\nUsage: b/f [hist] [clear] [!ELN]"
#define DUP_USAGE "Duplicate files\nUsage: d, dup SOURCE [DEST]"
#define EDIT_USAGE "Edit the main configuration file\nUsage: edit [reset] [APPLICATION]"
#define EXT_USAGE "Turn the use of external commands on-off\nUsage: ext [on, off, status]"
#define EXPORT_USAGE "Export files to a temporary file\nUsage: exp [FILE(s)]"
#define FC_USAGE "Turn the files counter for directories on-off\nUsage: fc, filescounter [on, off, status]"
#define FF_USAGE "Set show folders first on-off\nUsage: ff, folders-first [on, off, status]"
#define FILTER_USAGE "Set a filter for the files list\nUsage: ft, filter [unset] [REGEX]"
#define FORTH_USAGE "Change to the next directory in the directory history list\nUsage: forth, f [h, hist] [clear] [!ELN]"
#define HF_USAGE "Set hidden files on-off\nUsage: hidden, hf [on, off, status]"
#define HISTEXEC_USAGE "Access commands history entries\nUsage: \
!!: Execute the last command. \
!n: Execute the command number 'n' in the history list. \
!-n: Execute the last-n command in the history list."
#define HISTORY_USAGE "List or access commands history entries\nUsage: history [edit] [clear] [-n]"
#define ICONS_USAGE "Set icons on-off\nUsage: icons [on, off]"
#define JUMP_USAGE "Change to a directory in the jump database\nUsage: j, jc, jp, jl [STRING ...], jo [NUM], je"
#define KB_USAGE "Handle keybindings\nUsage: kb, keybinds [edit] [reset] [readline]"
#define LE_USAGE "Edit a symbolic link\nUsage: le SYMLINK"
#define LM_USAGE "Set light mode on-off\nUsage: lm [on, off]"
#define LOG_USAGE "List or clear CliFM logs\nUsage: log [clear]"
#define MF_USAGE "Limit the amount of files listed on the screen to NUM (valid range: >= 0). Use 'unset' to remove the files limit.\nUsage: mf [NUM, unset]"
#define MIME_USAGE "Set default opening applications based on MIME types\nUsage: mm, mime [info ELN/FILENAME] [edit] [import]"
#define MSG_USAGE "List available CliFM messages\nUsage: messages, msg [clear]"
#define MOUNPOINTS_USAGE "List and change to a mountpoint\nUsage: mp, mountpoints"
#define NET_USAGE "Handle network resources\nUsage: net [NAME] [edit] [m, mount NAME] [u, unmount NAME]"
#define NEW_USAGE "Create a new file or directory\nUsage: n, new [FILE DIR/ ...n]"
#define NOT_AVAILABLE "This feature has been disabled at compile time"
#define OPEN_USAGE "Open a file\nUsage: o, open ELN/FILE [APPLICATION]"
#define OW_USAGE "Open a file with...\nUsage: ow ELN/FILE"
#define OPENER_USAGE "Set the resource opener\nUsage: opener APPLICATION"
#define PAGER_HELP "?, h: help\nDown arrow, Enter, Space: Advance one line\nPage Down: Advance one page\nq: Stop pagging\n"
#define PAGER_LABEL "\x1b[7;97m--Mas--\x1b[0;49m"
#define PAGER_USAGE "Set the files list pager on-off\nUsage: pager, pg [on, off, status]"
#define PIN_USAGE "Pin a file or directory\nUsage: pin FILE/DIR"
#define PROFILES_USAGE "Manage profiles\nUsage: pf, prof, profile [ls, list] [set, add, del PROFILE]"
#define PROP_USAGE "Print files properties\nUsage: p, pr, pp, prop [ELN/FILE ... n]"
#define SEL_USAGE "Select one or multiple files\nUsage: s, sel ELN/FILE... [[!]PATTERN] [-FILETYPE] [:PATH]"
#define SHELL_USAGE "Set the shell used to run external commands\nUsage: shell [SHELL]"
#define SORT_USAGE "Change sort method for the files list\nUsage: st [METHOD] [rev]\nMETHOD: 0 = none, \
1 = name, 2 = size, 3 = atime, 4 = btime, \
5 = ctime, 6 = mtime, 7 = version, 8 = extension, \
9 = inode, 10 = owner, 11 = group\nBoth numbers and names are allowed"
#define TE_USAGE "Toggle the executable bit on files\nUsage: te FILE(s)"
#define TRASH_USAGE "Send one or multiple files to the trash can\nUsage: t, tr, trash [ELN/FILE ... n] [ls, list] [clear] [del, rm]"
#define UNICODE_USAGE "Set unicode on-off\nUsage: unicode, uc [on, off, status]"
#define UNTRASH_USAGE "Restore files from the trash can\nUsage: u, undel, untrash [*, a, all]"
#define WRAPPERS_USAGE "c (v, paste), l, m, md, and r commands are wrappers for cp, ln, mv, mkdir, and rm shell commands respectivelly.\n\nWithout option parameters:\nc -> cp -iRp\nl -> ln -sn\nm -> mv -i\nmd -> mkdir -p\nr (for directories) -> rm -dIr (\"rm -r\" on NetBSD and OpenBSD)\nr (for non-directories) -> rm -I (\"rm -f\" on NetBSD and OpenBSD)\nv, paste FILE -> c FILE .\n\nThe 'le' command is available to edit symbolic links: le LINK"
#define WS_USAGE "Switch workspaces\nUsage: ws [NUM, +, -]"
#define X_USAGE "Launch a new instance of CliFM on a new terminal window\nUsage: x, X [DIR]"

#ifndef __HAIKU__
#define HELP_MESSAGE "Enter '?' or press F1-F3 for instructions"
#else
#define HELP_MESSAGE "Enter '?' or press F5-F7 for instructions"
#endif
