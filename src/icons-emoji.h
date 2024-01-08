/* icons-emoji.h - Icon definitions for CliFM */

/* Taken from
 * https://github.com/jarun/nnn, licensed under BSD-2-Clause.
 * All changes are licensed under GPL-2.0-or-later.
*/

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

/* For a list of emojis consult:
 * https://unicode.org/Public/emoji/5.0/emoji-test.txt */

/* For Arch based systems you need noto-fonts-emoji, for Debian-based
 * fonts-noto-color-emoji, and for RedHat-based google-noto-emoji-fonts
 * Once installed, test emojis on your terminal issuing this command:
 * wget https://unicode.org/Public/emoji/5.0/emoji-test.txt -qO - | less */

/* Most emojis here are two chars wide. If using one char wide emojis, add
 * a space to get consistent padding. Ex: "X ", or to the left: " X" */

#ifndef ICONS_EMOJI
#define ICONS_EMOJI

#define EMOJI_ARCHIVE "📦"
#define EMOJI_ASM EMOJI_CODE
#define EMOJI_AUDIO "🎵"
#define EMOJI_BINARY "📓"
#define EMOJI_BRIEFCASE "💼"
#define EMOJI_C "🇨 "
#define EMOJI_CLOJURE EMOJI_CODE
#define EMOJI_CODE "📑"
#define EMOJI_CHANGELOG "🔺"
#define EMOJI_CHESS EMOJI_FILE
#define EMOJI_CONF "🔧"
#define EMOJI_COFFEE EMOJI_JAVA
#define EMOJI_CPP EMOJI_C
#define EMOJI_CSHARP EMOJI_CODE
#define EMOJI_CSS "🦋"
#define EMOJI_DATABASE EMOJI_FILE
#define EMOJI_DESKTOP "💻"
#define EMOJI_DART EMOJI_CODE
#define EMOJI_DIFF "📋"
#define EMOJI_DISK "💿"
#define EMOJI_DJVU "📎"
#define EMOJI_DOCKER "🐋"
#define EMOJI_DOWNLOAD "📥"
#define EMOJI_ELECTRON "⚛ "
#define EMOJI_ELIXIR "💧"
#define EMOJI_ELM EMOJI_CODE
#define EMOJI_ENCRYPTED "🔒"
#define EMOJI_ERLANG EMOJI_CODE
#define EMOJI_EXEC "⚙ "
#define EMOJI_FILE "📄"
#define EMOJI_FOLDER "📂"
#define EMOJI_FONT "🔤"
#define EMOJI_FSHARP EMOJI_CODE
#define EMOJI_HASKELL EMOJI_CODE
#define EMOJI_GAMES "🎮"
#define EMOJI_GIT "🌱"
#define EMOJI_GO "🐹"
#define EMOJI_HOME "🏠"
#define EMOJI_IMAGE "🎨"
#define EMOJI_JAVA "☕"
#define EMOJI_JAVASCRIPT EMOJI_CODE
#define EMOJI_JULIA EMOJI_CODE
#define EMOJI_JSON EMOJI_CONF
#define EMOJI_KEY "🔑"
#define EMOJI_KOTLIN "🇰 "
#define EMOJI_LICENSE "⚖ "
#define EMOJI_LINK "🔗"
#define EMOJI_LINUX "🐧"
#define EMOJI_LIST "✅"
#define EMOJI_LOCK "🔐"
#define EMOJI_LUA "🌘"
#define EMOJI_MAKE "🛠 "
#define EMOJI_MANUAL "❔"
#define EMOJI_MARKDOWN "⬇ "
#define EMOJI_MOVIE "🎬"
#define EMOJI_MUSIC "🎧"
#define EMOJI_NOTE "📝"
#define EMOJI_OCAML "🐫"
#define EMOJI_PATCH "🩹"
#define EMOJI_PDF "🔖"
#define EMOJI_PERL "🐪"
#define EMOJI_PHOTO "📸"
#define EMOJI_PICTURE "📷"
#define EMOJI_PLAYLIST EMOJI_MUSIC
#define EMOJI_POSTSCRIPT EMOJI_PDF
#define EMOJI_PRESENTATION "📙"
#define EMOJI_PUBLIC "👁 "
#define EMOJI_PYTHON "🐍"
#define EMOJI_R "🇷 "
#define EMOJI_RSS "📡"
#define EMOJI_RUBY "💎"
#define EMOJI_RUST "🦀"
#define EMOJI_SASS EMOJI_CODE
#define EMOJI_SCALA EMOJI_CODE
#define EMOJI_SCRIPT "📜"
#define EMOJI_SHARE "🖇 "
#define EMOJI_STEAM EMOJI_GAMES
#define EMOJI_STYLESHEET "📗"
#define EMOJI_SUBTITLES "💬"
#define EMOJI_SWIFT EMOJI_CODE
#define EMOJI_TEMPLATE "📎"
#define EMOJI_TEX EMOJI_FILE
#define EMOJI_TEXT EMOJI_FILE
#define EMOJI_TYPESCRIPT EMOJI_CODE
#define EMOJI_TRASH EMOJI_FOLDER
#define EMOJI_VIDEOS "📽 "
#define EMOJI_VIM EMOJI_TEXT
#define EMOJI_VISUALSTUDIO EMOJI_CODE
#define EMOJI_WEB "🌐"
#define EMOJI_WINDOWS "🪟"
#define EMOJI_WORD "📘"

#endif /* ICONS_EMOJI */
