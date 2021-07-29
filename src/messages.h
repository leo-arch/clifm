/* help.h - Usage mesages for CliFM */

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

#define GRAL_USAGE "[-aAefFgGhiIlLmoOsSuUvxy] [-b FILE] [-c FILE] [-D DIR] \
[-k FILE] [-P PROFILE] [-z METHOD] [PATH]"

#define ACTIONS_USAGE "Usage: actions [edit]"
#define ALIAS_USAGE "Usage: alias [import FILE]"
#define ARCHIVE_USAGE "Usage: ac, ad ELN/FILE ..."
#define AUTOCD_USAGE "Usage: acd, autocd [on, off, status]"
#define AUTO_OPEN_USAGE "Usage: ao, auto-open [on, off, status]"
#define BACK_USAGE "Usage: back, b [h, hist] [clear] [!ELN]"
#define BL_USAGE "Usage: bl FILE(s)"
#define BOOKMARKS_USAGE "Usage: bm, bookmarks [a, add FILE] [d, del] [edit]"
#define BULK_USAGE "Usage: br, bulk ELN/FILE ..."
#define CD_USAGE "Usage: cd [ELN/DIR]"
#define COLUMNS_USAGE "Usage: cl, columns [on, off]"
#define CS_USAGE "Usage: cs, colorschemes [edit] [COLORSCHEME]"
#define DESEL_USAGE "Usage: desel, ds [*, a, all]"
#define DIRHIST_USAGE "Usage: b/f [hist] [clear] [!ELN]"
#define DUP_USAGE "Usage: d, dup SOURCE [DEST]"
#define EDIT_USAGE "Usage: edit [reset] [APPLICATION]"
#define EXT_USAGE "Usage: ext [on, off, status]"
#define EXPORT_USAGE "Usage: exp, export [FILE(s)]"
#define FC_USAGE "Usage: fc, filescounter [on, off, status]"
#define FF_USAGE "Usage: ff, folders-first [on, off, status]"
#define FILTER_USAGE "Usage: ft, filter [unset] [REGEX]"
#define FORTH_USAGE "Usage: forth, f [h, hist] [clear] [!ELN]"
#define HF_USAGE "Usage: hidden, hf [on, off, status]"
#define HISTEXEC_USAGE "Usage: \
!!: Execute the last command. \
!n: Execute the command number 'n' in the history list. \
!-n: Execute the last-n command in the history list."
#define HISTORY_USAGE "Usage: history [clear] [-n]"
#define ICONS_USAGE "Usage: icons [on, off]"
#define JUMP_USAGE "Usage: j, jc, jp, jl [STRING ...], jo [NUM], je"
#define KB_USAGE "Usage: kb, keybinds [edit] [reset] [readline]"
#define LE_USAGE "Usage: le SYMLINK"
#define LM_USAGE "Usage: lm [on, off]"
#define LOG_USAGE "Usage: ff, folders-first [on, off, status]"
#define MF_USAGE "Usage: mf [NUM]"
#define MIME_USAGE "Usage: mm, mime [info ELN/FILENAME] [edit] [import]"
#define MSG_USAGE "Usage: messages, msg [clear]"
#define MOUNPOINTS_USAGE "Usage: mp, mountpoints"
#define NET_USAGE "Usage: net [NAME] [edit] [m, mount NAME] [u, unmount NAME]"
#define NEW_USAGE "Usage: n, new [FILE DIR/ ...n]"
#define OPEN_USAGE "Usage: o, open ELN/FILE [APPLICATION]"
#define OPENER_USAGE "Usage: opener APPLICATION"
#define PAGER_USAGE "Usage: pager, pg [on, off, status]"
#define PIN_USAGE "Usage: pin FILE/DIR"
#define PROFILES_USAGE "Usage: pf, prof, profile [ls, list] [set, add, del PROFILE]"
#define PROP_USAGE "Usage: p, pr, pp, prop [ELN/FILE ... n]"
#define SEL_USAGE "Usage: s, sel ELN/FILE... [[!]PATTERN] [-FILETYPE] [:PATH]"
#define SHELL_USAGE "Usage: shell [SHELL]"
#define SORT_USAGE "Usage: st [METHOD] [rev]\nMETHOD: 0 = none, \
1 = name, 2 = size, 3 = atime, 4 = btime, \
5 = ctime, 6 = mtime, 7 = version, 8 = extension, \
9 = inode, 10 = owner, 11 = group\nBoth numbers and names are allowed"
#define TE_USAGE "Usage: te FILE(s)"
#define TRASH_USAGE "Usage: t, tr, trash [ELN/FILE ... n] [ls, list] [clear] [del, rm]"
#define UNICODE_USAGE "Usage: unicode, uc [on, off, status]"
#define UNTRASH_USAGE "Usage: u, undel, untrash [*, a, all]"
#define WS_USAGE "Usage: ws [NUM, +, -]"
#define X_USAGE "Usage: x, X [DIR]"
