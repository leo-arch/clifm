/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* icons-emoji.h - Icon definitions for Clifm */

/* Taken from
 * https://github.com/jarun/nnn, licensed under BSD-2-Clause.
 * All changes are licensed under GPL-2.0-or-later.
*/

/* For a list of emojis consult:
 * https://unicode.org/Public/emoji/5.0/emoji-test.txt */

/* For Arch based systems you need noto-fonts-emoji, for Debian-based
 * fonts-noto-color-emoji, and for RedHat-based google-noto-emoji-fonts
 * Once installed, test emojis on your terminal issuing this command:
 * wget https://unicode.org/Public/emoji/5.0/emoji-test.txt -qO - | less */

/* Most emojis here are two chars wide. If using one char wide emojis, add
 * a space to get consistent padding. E.g., "X ", or to the left: " X" */

#ifndef ICONS_EMOJI
#define ICONS_EMOJI

#define EMOJI_3D_FILE "🧊"
#define EMOJI_ARCHIVE "📦"
#define EMOJI_ARDUINO EMOJI_CODE
#define EMOJI_ASM EMOJI_CODE
#define EMOJI_AUDIO "🎵"
#define EMOJI_BINARY "📓"
#define EMOJI_BOOK_OPEN "📖"
#define EMOJI_BRIEFCASE "💼"
#define EMOJI_CACHE EMOJI_FILE
#define EMOJI_CALENDAR "📅"
#define EMOJI_CD "💿"
#define EMOJI_CERT EMOJI_KEY
#define EMOJI_CLOJURE EMOJI_CODE
#define EMOJI_CODE "📑"
#define EMOJI_CHANGELOG "🔺"
#define EMOJI_CONF "🔧"
#define EMOJI_COFFEE EMOJI_JAVA
#define EMOJI_CPP EMOJI_C
#define EMOJI_CSHARP EMOJI_CODE
#define EMOJI_CSS "🦋"
#define EMOJI_D EMOJI_CODE
#define EMOJI_DATABASE EMOJI_FILE
#define EMOJI_DESKTOP "💻"
#define EMOJI_DART EMOJI_CODE
#define EMOJI_DIFF "🩹"
#define EMOJI_DISK "💾"
#define EMOJI_DOCKER "🐋"
#define EMOJI_DOWNLOAD "📥"
#define EMOJI_ELIXIR "💧"
#define EMOJI_ELM EMOJI_CODE
#define EMOJI_ENCRYPTED "🔒"
#define EMOJI_ERLANG EMOJI_CODE
#define EMOJI_FILE "📄"
#define EMOJI_FOLDER "📁"
#define EMOJI_FONT "🔤"
#define EMOJI_FORTRAN EMOJI_CODE
#define EMOJI_FSHARP EMOJI_CODE
#define EMOJI_GLEAM "⭐"
#define EMOJI_HASKELL EMOJI_CODE
#define EMOJI_GAMES "🎮"
#define EMOJI_GIT "🌱"
#define EMOJI_GO "🐹"
#define EMOJI_GODOT "🤖"
#define EMOJI_GRADLE EMOJI_CODE
#define EMOJI_GRAPHQL EMOJI_CODE
#define EMOJI_GRAPHVIZ EMOJI_CODE
#define EMOJI_GROOVY EMOJI_CODE
#define EMOJI_HAML EMOJI_CODE
#define EMOJI_HEROKU EMOJI_CONF
#define EMOJI_HOME "🏠"
#define EMOJI_HTML "🌐"
#define EMOJI_IMAGE "🎨"
#define EMOJI_JAVA "☕"
#define EMOJI_JAVASCRIPT EMOJI_CODE
#define EMOJI_JENKINS EMOJI_CONF
#define EMOJI_JULIA EMOJI_CODE
#define EMOJI_JUPYTER EMOJI_CODE
#define EMOJI_JSON EMOJI_CONF
#define EMOJI_KEY "🔑"
#define EMOJI_KICAD EMOJI_CODE
#define EMOJI_KOTLIN EMOJI_CODE
#define EMOJI_LINK "🔗"
#define EMOJI_LINUX "🐧"
#define EMOJI_LIST "✅"
#define EMOJI_LIVESCRIPT EMOJI_SHELL
#define EMOJI_LOCK "🔐"
#define EMOJI_LUA "🌘"
#define EMOJI_MAGNET "🧲"
#define EMOJI_MANUAL "❔"
#define EMOJI_MATLAB EMOJI_CODE
#define EMOJI_MOVIE "🎬"
#define EMOJI_MUSIC "🎧"
#define EMOJI_MUSTACHE "〰️"
#define EMOJI_NODEJS EMOJI_CODE
#define EMOJI_NIM EMOJI_CODE
#define EMOJI_NOTE "📝"
#define EMOJI_OCAML "🐫"
#define EMOJI_PACKAGE "📦"
#define EMOJI_PDF "🔖"
#define EMOJI_PERL "🐪"
#define EMOJI_PHOTO "📸"
#define EMOJI_PHP "🐘"
#define EMOJI_PICTURE "📷"
#define EMOJI_PLAYLIST EMOJI_MUSIC
#define EMOJI_POSTSCRIPT EMOJI_PDF
#define EMOJI_POWERSHELL EMOJI_SHELL
#define EMOJI_PRESENTATION "📙"
#define EMOJI_PURESCRIPT EMOJI_CODE
#define EMOJI_PYTHON "🐍"
#define EMOJI_ROM EMOJI_FILE
#define EMOJI_RSS "📡"
#define EMOJI_RUBY "💎"
#define EMOJI_RUBYRAILS EMOJI_RUBY
#define EMOJI_RUST "🦀"
#define EMOJI_SASS EMOJI_CODE
#define EMOJI_SCALA EMOJI_CODE
#define EMOJI_SCHEME EMOJI_CODE
#define EMOJI_SHELL "📜"
#define EMOJI_STEAM EMOJI_GAMES
#define EMOJI_STYLESHEET "📗"
#define EMOJI_STYLUS EMOJI_CODE
#define EMOJI_SUBLIME EMOJI_FILE
#define EMOJI_SUBTITLES "💬"
#define EMOJI_SVELTE EMOJI_CODE
#define EMOJI_SWIFT EMOJI_CODE
#define EMOJI_TCL "🪶"
#define EMOJI_TEMPLATE "📎"
#define EMOJI_TEX EMOJI_FILE
#define EMOJI_TEXT EMOJI_FILE
#define EMOJI_LOG EMOJI_NOTE
#define EMOJI_TOML EMOJI_CONF
#define EMOJI_TYPESCRIPT "🔷"
#define EMOJI_TRASH EMOJI_FOLDER
#define EMOJI_TWIG "🌱"
#define EMOJI_V EMOJI_CODE
#define EMOJI_VAGRANT EMOJI_FILE
#define EMOJI_VALA EMOJI_CODE
#define EMOJI_VIM EMOJI_TEXT
#define EMOJI_VISUALSTUDIO EMOJI_CODE
#define EMOJI_VUE EMOJI_CODE
#define EMOJI_WORD "📘"
#define EMOJI_XAML EMOJI_CODE
#define EMOJI_XML EMOJI_CODE
#define EMOJI_YAML EMOJI_CONF

#define EMOJI_MARKDOWN "📃"
#define EMOJI_EXEC "🛞"
#define EMOJI_LICENSE "🔏"
#define EMOJI_SHARE "📎"
#define EMOJI_C EMOJI_CODE
#define EMOJI_CHECKSUM "✅"
#define EMOJI_DROPBOX EMOJI_FOLDER
#define EMOJI_ELECTRON EMOJI_CODE
#define EMOJI_GIMP EMOJI_IMAGE
#define EMOJI_ILLUSTRATOR EMOJI_IMAGE
#define EMOJI_KRITA EMOJI_IMAGE
#define EMOJI_MAKE "🚧"
#define EMOJI_ONEDRIVE EMOJI_FOLDER
#define EMOJI_PHOTOSHOP EMOJI_IMAGE
#define EMOJI_PUBLIC "📂"
#define EMOJI_R EMOJI_CODE
#define EMOJI_VIDEOS "📼"

#endif /* ICONS_EMOJI */
