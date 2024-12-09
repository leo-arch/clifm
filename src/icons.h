/* icons.h - Icon definitions for CliFM */

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

#ifndef ICONS_H
#define ICONS_H

#if defined(_NERD)
# include "icons-nerdfont.h"
#elif defined(_ICONS_IN_TERMINAL)
# include "icons-in-terminal.h"
#else
# include "icons-emoji.h"
#endif /* _NERD */

struct icons_t {
	char *name;
	char *icon;
	char *color;
};

/* Icon macros are defined in icons-in-terminal.h (provided by the
 * 'icons-in-terminal' project), icons-nerdfont.h, and icons-emoji.h */
char
#if defined(_NERD)
    /* File types */
    ICON_DIR[] = NERD_DIRECTORY,
    ICON_EXEC[] = NERD_EXEC,
    ICON_LOCK[] = NERD_LOCK,
    ICON_LINK[] = NERD_LINK,
    ICON_REG[] = NERD_FILE,

    /* Extensions */
    ICON_ACCESS[] = NERD_FILE,
    ICON_ARCHIVE[] = NERD_ARCHIVE,
    ICON_AUDIO[] = NERD_MUSICFILE,
    ICON_BINARY[] = NERD_BINARY,
    ICON_BOOK[] = NERD_BOOK,
    ICON_C[] = NERD_C,
    ICON_CD[] = NERD_OPTICALDISK,
	ICON_CHESS[] = NERD_CHESS,
    ICON_CLOJURE[] = NERD_CLOJURE,
    ICON_CMAKE[] = NERD_FILE,
    ICON_CODE[] = NERD_FILE,
    ICON_COFFEE[] = NERD_COFFEE,
    ICON_COMMENTS[] = NERD_COMMENTS,
    ICON_CONF[] = NERD_CONFIGURE,
    ICON_CONFIGURE[] = NERD_CONFIGURE,
    ICON_COPYRIGHT[] = NERD_COPYRIGHT,
    ICON_CPP[] = NERD_CPLUSPLUS,
    ICON_CSHARP[] = NERD_CSHARP,
    ICON_CSS[] = NERD_CSS3,
    ICON_DART[] = NERD_DART,
    ICON_DATABASE[] = NERD_DATABASE,
    ICON_DIFF[] = NERD_DIFF,
    ICON_DOCKER[] = NERD_DOCKER,
    ICON_ELECTRON[] = NERD_ELECTRON,
    ICON_ELF[] = NERD_BINARY,
    ICON_ELM[] = NERD_ELM,
    ICON_ELIXIR[] = NERD_ELIXIR,
    ICON_EMACS[] = NERD_EMACS,
    ICON_ERLANG[] = NERD_ERLANG,
    ICON_EXCEL[] = NERD_EXCELDOC,
    ICON_FONT[] = NERD_FONT,
    ICON_FSHARP[] = NERD_FSHARP,
    ICON_GO[] = NERD_GO,
    ICON_HASKELL[] = NERD_HASKELL,
    ICON_HISTORY[] = NERD_HISTORY,
    ICON_HTML[] = NERD_HTML,
    ICON_IMG[] = NERD_PICTUREFILE,
    ICON_JAVA[] = NERD_JAVA,
    ICON_JAVASCRIPT[] = NERD_JAVASCRIPT,
    ICON_JSON[] = NERD_JSON,
    ICON_JULIA[] = NERD_JULIA,
    ICON_KEY[] = NERD_KEY,
    ICON_KOTLIN[] = NERD_KOTLIN,
    ICON_LIST[] = NERD_CHECKLIST,
    ICON_LUA[] = NERD_LUA,
    ICON_MAKEFILE[] = NERD_MAKEFILE,
    ICON_MANPAGE[] = NERD_MANUAL,
    ICON_MARKDOWN[] = NERD_MARKDOWN,
    ICON_MATLAB[] = NERD_MATLAB,
    ICON_MYSQL[] = NERD_MYSQL,
    ICON_ASM[] = NERD_ASM,
    ICON_OCAML[] = NERD_OCAML,
    ICON_OPENOFFICE_P[] = NERD_PPTDOC,
    ICON_OPENOFFICE_S[] = NERD_EXCELDOC,
    ICON_OPENOFFICE_T[] = NERD_WORDDOC,
    ICON_PATCH[] = NERD_PATCH,
    ICON_PDF[] = NERD_PDF,
    ICON_PERL[] = NERD_PERL,
    ICON_PHOTOSHOP[] = NERD_PHOTOSHOP,
    ICON_PHP[] = NERD_PHP,
    ICON_POSTSCRIPT[] = NERD_FILE,
    ICON_POWERPOINT[] = NERD_PPTDOC,
    ICON_PYTHON[] = NERD_PYTHON,
    ICON_R[] = NERD_R,
    ICON_REACT[] = NERD_REACT,
    ICON_README[] = NERD_BOOK,
    ICON_ROM[] = NERD_ROM,
    ICON_RSS[] = NERD_RSS,
    ICON_RUBY[] = NERD_RUBY,
    ICON_RUST[] = NERD_RUST,
    ICON_SASS[] = NERD_SASS,
    ICON_SCALA[] = NERD_SCALA,
    ICON_SCRIPT[] = NERD_SCRIPT,
    ICON_SHARE[] = NERD_SHARE,
    ICON_SQLITE[] = NERD_FILE,
    ICON_SWIFT[] = NERD_SWIFT,
    ICON_TEX[] = NERD_TEX,
    ICON_TS[] = NERD_TS,
    ICON_TXT[] = NERD_TXT,
    ICON_VID[] = NERD_VIDEOFILE,
    ICON_VIM[] = NERD_VIM,
    ICON_VISUALSTUDIO[] = NERD_VISUALSTUDIO,
    ICON_WINDOWS[] = NERD_WINDOWS,
    ICON_WORD[] = NERD_WORDDOC,
    ICON_ZIG[] = NERD_ZIG,

    ICON_ARCH[] = NERD_ARCHLINUX,
    ICON_DEBIAN[] = NERD_DEBIAN,
	ICON_NIX[] = NERD_NIX,
    ICON_REDHAT[] = NERD_REDHAT,

    /* Dir names */
    ICON_DESKTOP[] = NERD_DESKTOP,
    ICON_DOCUMENTS[] = NERD_BRIEFCASE,
    ICON_DOTGIT[] = NERD_GIT,
    ICON_DOWNLOADS[] = NERD_DOWNLOADS,
    ICON_DROPBOX[] = NERD_DROPBOX,
    ICON_FAV[] = NERD_FAV,
    ICON_HOME[] = NERD_HOME,
    ICON_GAMES[] = NERD_GAMES,
    ICON_MUSIC[] = NERD_MUSIC,
    ICON_PLAYLIST[] = NERD_PLAYLIST,
    ICON_TRASH[] = NERD_TRASH,
    ICON_PICTURES[] = NERD_PICTURES,
    ICON_PUBLIC[] = NERD_PUBLIC,
    ICON_STEAM[] = NERD_STEAM,
    ICON_TEMPLATES[] = NERD_TEMPLATES,
    ICON_VIDEOS[] = NERD_VIDEOS;

#elif defined(_ICONS_IN_TERMINAL)
    /* File types */
    ICON_DIR[] = FA_FOLDER,
    ICON_EXEC[] = FA_COG,
    ICON_LINK[] = FA_CHAIN,
    ICON_LOCK[] = FA_LOCK,
    ICON_REG[] = FA_FILE_O,

    /* Extensions */
    ICON_ACCESS[] = FILE_ACCESS,
    ICON_ARCHIVE[] = FA_ARCHIVE,
    ICON_AUDIO[] = FA_FILE_AUDIO_O,
    ICON_BINARY[] = OCT_FILE_BINARY,
    ICON_BOOK[] = OCT_BOOK,
    ICON_C[] = MFIZZ_C,
    ICON_CD[] = LINEA_MUSIC_CD,
    ICON_CHESS[] = FA_FILE_O,
    ICON_CLOJURE[] = MFIZZ_CLOJURE,
    ICON_CMAKE[] = FILE_CMAKE,
    ICON_CODE[] = FA_FILE_CODE_O,
    ICON_COFFEE[] = DEV_COFFEESCRIPT,
    ICON_COMMENTS[] = FA_COMMENTS,
    ICON_CONF[] = FA_SLIDERS,
    ICON_CONFIGURE[] = FA_SLIDERS,
    ICON_COPYRIGHT[] = OCT_LAW,
    ICON_CPP[] = MFIZZ_CPLUSPLUS,
    ICON_CSHARP[] = MFIZZ_CSHARP,
    ICON_CSS[] = MFIZZ_CSS3_ALT,
    ICON_DART[] = DEV_DART,
    ICON_DATABASE[] = FA_DATABASE,
    ICON_DIFF[] = FILE_DIFF,
    ICON_DOCKER[] = MFIZZ_DOCKER,
    ICON_ELECTRON[] = FILE_ELECTRON,
    ICON_ELF[] = OCT_FILE_BINARY,
    ICON_ELIXIR[] = MFIZZ_ELIXIR,
    ICON_ELM[] = MFIZZ_ELM,
    ICON_EMACS[] = FILE_EMACS,
    ICON_ERLANG[] = DEV_ERLANG,
    ICON_EXCEL[] = FILE_EXCEL,
    ICON_FONT[] = FILE_FONT,
    ICON_FSHARP[] = DEV_FSHARP,
    ICON_GO[] = FILE_GO,
    ICON_HASKELL[] = MFIZZ_HASKELL,
    ICON_HISTORY[] = FA_HISTORY,
    ICON_HTML[] = FA_FILE_CODE_O,
    ICON_IMG[] = FA_FILE_IMAGE_O,
    ICON_JAVA[] = MFIZZ_JAVA,
    ICON_JAVASCRIPT[] = DEV_JAVASCRIPT,
    ICON_JSON[] = FILE_SWAGGER,
    ICON_JULIA[] = FILE_JULIA,
    ICON_KEY[] = MD_VPN_KEY,
    ICON_KOTLIN[] = FILE_KOTLIN,
    ICON_LIST[] = FA_CHECK_SQUARE_O,
    ICON_LUA[] = FILE_LUA,
    ICON_MANPAGE[] = FILE_MANPAGE,
    ICON_MAKEFILE[] = FILE_MAPBOX,
    ICON_MATLAB[] = FILE_MATLAB,
    ICON_MARKDOWN[] = OCT_MARKDOWN,
    ICON_MYSQL[] = MFIZZ_MYSQL,
    ICON_ASM[] = FILE_NASM,
    ICON_OCAML[] = FILE_OCAML,
    ICON_OPENOFFICE_P[] = FILE_OPENOFFICE,
    ICON_OPENOFFICE_S[] = FILE_OPENOFFICE,
    ICON_OPENOFFICE_T[] = FILE_OPENOFFICE,
    ICON_PATCH[] = FILE_PATCH,
    ICON_PDF[] = FA_FILE_PDF_O,
    ICON_PERL[] = MFIZZ_PERL,
    ICON_PHP[] = MFIZZ_PHP_ALT,
    ICON_PHOTOSHOP[] = DEV_PHOTOSHOP,
    ICON_POSTSCRIPT[] = FILE_POSTSCRIPT,
    ICON_POWERPOINT[] = FILE_POWERPOINT,
    ICON_PYTHON[] = MFIZZ_PYTHON,
    ICON_R[] = FILE_R,
    ICON_REACT[] = FILE_REACT,
    ICON_README[] = OCT_BOOK,
    ICON_ROM[] = FILE_VERILOG,
    ICON_RSS[] = MD_RSS_FEED,
    ICON_RUBY[] = MFIZZ_RUBY,
    ICON_RUST[] = MFIZZ_RUST,
    ICON_SASS[] = MFIZZ_SASS,
    ICON_SCALA[] = MFIZZ_SCALA,
    ICON_SCRIPT[] = OCT_TERMINAL,
    ICON_SHARE[] = MD_SHARE,
    ICON_SQLITE[] = FILE_SQLITE,
    ICON_SWIFT[] = DEV_SWIFT,
    ICON_TEX[] = FILE_TEX,
    ICON_TS[] = FILE_TS,
    ICON_TXT[] = MD_FORMAT_ALIGN_LEFT,
    ICON_VID[] = FA_FILE_MOVIE_O,
    ICON_VIM[] = DEV_VIM,
    ICON_VISUALSTUDIO[] = DEV_VISUALSTUDIO,
    ICON_WINDOWS[] = DEV_WINDOWS,
    ICON_WORD[] = FILE_WORD,
    ICON_ZIG[] = FA_FILE_O,

    ICON_ARCH[] = MFIZZ_ARCHLINUX,
    ICON_DEBIAN[] = MFIZZ_DEBIAN,
    ICON_NIX[] = LINUX_NIXOS,
    ICON_REDHAT[] = LINUX_REDHAT,

    /* Dir names */
    ICON_DESKTOP[] = FA_DESKTOP,
    ICON_DOCUMENTS[] = FA_BRIEFCASE,
    ICON_DOTGIT[] = MFIZZ_GIT,
    ICON_DOWNLOADS[] = MD_FILE_DOWNLOAD,
    ICON_DROPBOX[] = FA_DROPBOX,
    ICON_FAV[] = FA_STAR_O,
    ICON_GAMES[] = MD_GAMES,
    ICON_HOME[] = FA_HOME,
    ICON_MUSIC[] = FA_MUSIC,
    ICON_PLAYLIST[] = MD_QUEUE_MUSIC,
    ICON_PICTURES[] = MD_CAMERA_ALT,
    ICON_PUBLIC[] = FA_INBOX,
    ICON_STEAM[] = FA_STEAM,
    ICON_TEMPLATES[] = FA_PAPERCLIP,
    ICON_TRASH[] = FA_TRASH,
    ICON_VIDEOS[] = FA_FILM;

#else /* emoji-icons */
    /* File types */
    ICON_DIR[] = EMOJI_FOLDER,
    ICON_EXEC[] = EMOJI_EXEC,
    ICON_LINK[] = EMOJI_LINK,
    ICON_LOCK[] = EMOJI_LOCK,
    ICON_REG[] = EMOJI_FILE,

    /* Extensions */
    ICON_ACCESS[] = EMOJI_PRESENTATION,
    ICON_ARCHIVE[] = EMOJI_ARCHIVE,
    ICON_AUDIO[] = EMOJI_AUDIO,
    ICON_BINARY[] = EMOJI_BINARY,
    ICON_BOOK[] = EMOJI_BOOK_OPEN,
    ICON_C[] = EMOJI_C,
    ICON_CD[] = EMOJI_DISK,
    ICON_CHESS[] = EMOJI_CHESS,
    ICON_CLOJURE[] = EMOJI_CLOJURE,
    ICON_CMAKE[] = EMOJI_MAKE,
    ICON_CODE[] = EMOJI_CODE,
    ICON_COFFEE[] = EMOJI_COFFEE,
    ICON_COMMENTS[] = EMOJI_SUBTITLES,
    ICON_CONF[] = EMOJI_CONF,
    ICON_CONFIGURE[] = EMOJI_CONF,
    ICON_COPYRIGHT[] = EMOJI_LICENSE,
    ICON_CPP[] = EMOJI_CPP,
    ICON_CSHARP[] = EMOJI_CSHARP,
    ICON_CSS[] = EMOJI_CSS,
    ICON_DART[] = EMOJI_DART,
    ICON_DATABASE[] = EMOJI_DATABASE,
    ICON_DIFF[] = EMOJI_DIFF,
    ICON_DOCKER[] = EMOJI_DOCKER,
    ICON_ELECTRON[] = EMOJI_ELECTRON,
    ICON_ELF[] = EMOJI_BINARY,
    ICON_ELIXIR[] = EMOJI_ELIXIR,
    ICON_ELM[] = EMOJI_ELM,
    ICON_EMACS[] = EMOJI_SCRIPT,
    ICON_ERLANG[] = EMOJI_ERLANG,
    ICON_EXCEL[] = EMOJI_STYLESHEET,
    ICON_FONT[] = EMOJI_FONT,
    ICON_FSHARP[] = EMOJI_FSHARP,
    ICON_GO[] = EMOJI_GO,
    ICON_HASKELL[] = EMOJI_HASKELL,
    ICON_HISTORY[] = EMOJI_CHANGELOG,
    ICON_HTML[] = EMOJI_WEB,
    ICON_IMG[] = EMOJI_IMAGE,
    ICON_JAVA[] = EMOJI_JAVA,
    ICON_JAVASCRIPT[] = EMOJI_JAVASCRIPT,
    ICON_JSON[] = EMOJI_JSON,
    ICON_JULIA[] = EMOJI_JULIA,
    ICON_KEY[] = EMOJI_KEY,
    ICON_KOTLIN[] = EMOJI_KOTLIN,
    ICON_LIST[] = EMOJI_LIST,
    ICON_LUA[] = EMOJI_LUA,
    ICON_MAKEFILE[] = EMOJI_MAKE,
    ICON_MANPAGE[] = EMOJI_MANUAL,
    ICON_MARKDOWN[] = EMOJI_MARKDOWN,
    ICON_MATLAB[] = EMOJI_MATLAB,
    ICON_MYSQL[] = EMOJI_DATABASE,
    ICON_ASM[] = EMOJI_ASM,
    ICON_OCAML[] = EMOJI_OCAML,
    ICON_OPENOFFICE_P[] = EMOJI_PRESENTATION,
    ICON_OPENOFFICE_S[] = EMOJI_STYLESHEET,
    ICON_OPENOFFICE_T[] = EMOJI_WORD,
    ICON_PATCH[] = EMOJI_PATCH,
    ICON_PDF[] = EMOJI_PDF,
    ICON_PERL[] = EMOJI_PERL,
    ICON_PHOTOSHOP[] = EMOJI_PHOTOSHOP,
    ICON_PHP[] = EMOJI_WEB,
    ICON_POSTSCRIPT[] = EMOJI_POSTSCRIPT,
    ICON_POWERPOINT[] = EMOJI_PRESENTATION,
    ICON_PYTHON[] = EMOJI_PYTHON,
    ICON_R[] = EMOJI_R,
    ICON_REACT[] = EMOJI_ELECTRON,
    ICON_README[] = EMOJI_TEXT,
    ICON_ROM[] = EMOJI_FILE,
    ICON_RSS[] = EMOJI_RSS,
    ICON_RUBY[] = EMOJI_RUBY,
    ICON_RUST[] = EMOJI_RUST,
    ICON_SASS[] = EMOJI_SASS,
    ICON_SCALA[] = EMOJI_SCALA,
    ICON_SCRIPT[] = EMOJI_SCRIPT,
    ICON_SHARE[] = EMOJI_SHARE,
    ICON_SQLITE[] = EMOJI_DATABASE,
    ICON_SWIFT[] = EMOJI_SWIFT,
    ICON_TEX[] = EMOJI_TEX,
    ICON_TS[] = EMOJI_TYPESCRIPT,
    ICON_TXT[] = EMOJI_NOTE,
    ICON_VIM[] = EMOJI_VIM,
    ICON_VID[] = EMOJI_MOVIE,
    ICON_VISUALSTUDIO[] = EMOJI_VISUALSTUDIO,
    ICON_WINDOWS[] = EMOJI_WINDOWS,
    ICON_WORD[] = EMOJI_WORD,
	ICON_ZIG[] = EMOJI_FILE,

    ICON_ARCH[] = EMOJI_MAKE,
    ICON_REDHAT[] = EMOJI_ARCHIVE,
    ICON_NIX[] = EMOJI_SCRIPT,
    ICON_DEBIAN[] = EMOJI_ARCHIVE,

    /* Dir names */
    ICON_DESKTOP[] = EMOJI_DESKTOP,
    ICON_DOCUMENTS[] = EMOJI_BRIEFCASE,
    ICON_DOTGIT[] = EMOJI_GIT,
    ICON_DOWNLOADS[] = EMOJI_DOWNLOAD,
    ICON_DROPBOX[] = EMOJI_FOLDER,
    ICON_FAV[] = EMOJI_FOLDER,
    ICON_GAMES[] = EMOJI_GAMES,
    ICON_HOME[] = EMOJI_HOME,
    ICON_MUSIC[] = EMOJI_MUSIC,
    ICON_PLAYLIST[] = EMOJI_PLAYLIST,
    ICON_PICTURES[] = EMOJI_PICTURE,
    ICON_PUBLIC[] = EMOJI_PUBLIC,
    ICON_STEAM[] = EMOJI_STEAM,
    ICON_TEMPLATES[] = EMOJI_TEMPLATE,
    ICON_TRASH[] = EMOJI_TRASH,
    ICON_VIDEOS[] = EMOJI_VIDEOS;
#endif /* _NERD */

#if defined(_NERD) || defined(_ICONS_IN_TERMINAL)
# define BLUE "\x1b[0;34m"
# define B_BLUE "\x1b[1;34m"
# define WHITE "\x1b[0;37m"
# define B_WHITE "\x1b[1;37m"
# define YELLOW "\x1b[0;33m"
# define B_YELLOW "\x1b[1;33m"
# define GREEN "\x1b[0;32m"
# define B_GREEN "\x1b[1;32m"
# define CYAN "\x1b[0;36m"
# define B_CYAN "\x1b[1;36m"
# define MAGENTA "\x1b[0;35m"
# define B_MAGENTA "\x1b[1;35m"
# define RED "\x1b[0;31m"
# define B_RED "\x1b[1;31m"
# define BLACK "\x1b[0;30m"
# define B_BLACK "\x1b[1;30m"
#else
# define EMPTY_STR ""
# define BLUE EMPTY_STR
# define B_BLUE EMPTY_STR
# define WHITE EMPTY_STR
# define B_WHITE EMPTY_STR
# define YELLOW EMPTY_STR
# define B_YELLOW EMPTY_STR
# define GREEN EMPTY_STR
# define B_GREEN EMPTY_STR
# define CYAN EMPTY_STR
# define B_CYAN EMPTY_STR
# define MAGENTA EMPTY_STR
# define B_MAGENTA EMPTY_STR
# define RED EMPTY_STR
# define B_RED EMPTY_STR
# define BLACK EMPTY_STR
# define B_BLACK EMPTY_STR
#endif /* _NERD || _ICONS_IN_TERMINAL */

/* Default icons and colors */
#define DEF_DIR_ICON ICON_DIR /* Directory */
#define DEF_DIR_ICON_COLOR YELLOW

#define DEF_FILE_ICON ICON_REG /* Regular file */
#define DEF_FILE_ICON_COLOR WHITE

#define DEF_LINK_ICON ICON_LINK /* Symbolic link */
#define DEF_LINK_ICON_COLOR CYAN

#define DEF_EXEC_ICON ICON_EXEC /* Executable file */
#define DEF_EXEC_ICON_COLOR WHITE

#define DEF_NOPERM_ICON ICON_LOCK /* File without access permissions */
#define DEF_NOPERM_ICON_COLOR YELLOW

/* Per file extension icons */
struct icons_t const icon_ext[] = {
    {"7z", ICON_ARCHIVE, YELLOW},
    {"3gp", ICON_VID, BLUE},

    {"a", ICON_ARCHIVE, YELLOW},
    {"apk", ICON_ARCHIVE, YELLOW},
    {"asm", ICON_ASM, WHITE},
    {"aup", ICON_AUDIO, YELLOW},
    {"avi", ICON_VID, BLUE},
    {"awk", ICON_SCRIPT, WHITE},
    {"azw", ICON_BOOK, B_WHITE},
    {"azw3", ICON_BOOK, B_WHITE},

    {"bash", ICON_SCRIPT, WHITE},
    {"bat", ICON_SCRIPT, WHITE},
#if defined(_ICONS_IN_TERMINAL)
    {"bib", FILE_BIBTEX, B_WHITE},
#endif /* _ICONS_IN_TERMINAL */
    {"bin", ICON_BINARY, WHITE},
    {"bmp", ICON_IMG, GREEN},
    {"bz2", ICON_ARCHIVE, YELLOW},

    {"c", ICON_C, BLUE},
    {"c++", ICON_CPP, WHITE},
    {"cabal", ICON_HASKELL, BLUE},
    {"cab", ICON_ARCHIVE, YELLOW},
    {"cb7", ICON_ARCHIVE, YELLOW},
    {"cba", ICON_ARCHIVE, YELLOW},
    {"cbr", ICON_ARCHIVE, YELLOW},
    {"cbz", ICON_ARCHIVE, YELLOW},
    {"cc", ICON_CPP, WHITE},
    {"cfg", ICON_CONF, WHITE},
    {"clifm", ICON_CONF, WHITE},
    {"class", ICON_JAVA, B_WHITE},
    {"clj", ICON_CLOJURE, GREEN},
    {"cljc", ICON_CLOJURE, GREEN},
    {"cljs", ICON_CLOJURE, GREEN},
    {"cls", ICON_TEX, B_WHITE},
    {"cmake", ICON_CMAKE, WHITE},
    {"coffee", ICON_COFFEE, WHITE},
    {"conf", ICON_CONF, WHITE},
    {"cpio", ICON_ARCHIVE, YELLOW},
    {"cpp", ICON_CPP, WHITE},
    {"csh", ICON_SCRIPT, WHITE},
    {"css", ICON_CSS, BLUE},
    {"cue", ICON_PLAYLIST, YELLOW},
    {"cvs", ICON_CONF, WHITE},
    {"cxx", ICON_CPP, WHITE},

    {"dat", ICON_BINARY, WHITE},
    {"dart", ICON_DART, BLUE},
    {"db", ICON_DATABASE, WHITE},
    {"deb", ICON_DEBIAN, RED},
    {"diff", ICON_DIFF, WHITE},
    {"djvu", ICON_BOOK, B_WHITE},
    {"dll", ICON_SHARE, BLUE},
    {"doc", ICON_WORD, BLUE},
    {"docx", ICON_WORD, BLUE},

    {"eex", ICON_ELIXIR, BLUE},
    {"ejs", ICON_JAVASCRIPT, WHITE},
    {"el", ICON_EMACS, BLUE},
    {"elf", ICON_ELF, WHITE},
    {"elm", ICON_ELM, GREEN},
    {"epub", ICON_BOOK, B_WHITE},
    {"erb", ICON_RUBY, RED},
    {"erl", ICON_ERLANG, RED},
    {"ex", ICON_ELIXIR, BLUE},
    {"exe", ICON_WINDOWS, BLUE},
    {"exs", ICON_ELIXIR, BLUE},

    {"f#", ICON_FSHARP, WHITE},
    {"fen", ICON_CHESS, WHITE},
    {"fish", ICON_SCRIPT, WHITE},
    {"flac", ICON_AUDIO, YELLOW},
    {"flv", ICON_VID, BLUE},
    {"fodp", ICON_OPENOFFICE_P, YELLOW},
    {"fods", ICON_OPENOFFICE_S, GREEN},
    {"fodt", ICON_OPENOFFICE_T, BLUE},
    {"fs", ICON_FSHARP, WHITE},
    {"fsi", ICON_FSHARP, WHITE},
    {"fsscript", ICON_FSHARP, WHITE},
    {"fsx", ICON_FSHARP, WHITE},

    {"gem", ICON_ARCHIVE, YELLOW},
    {"gif", ICON_IMG, GREEN},
    {"gpg", ICON_LOCK, YELLOW},
    {"go", ICON_GO, YELLOW},
    {"gz", ICON_ARCHIVE, YELLOW},
    {"gzip", ICON_ARCHIVE, YELLOW},

    {"h", ICON_C, BLUE},
    {"haml", ICON_CODE, WHITE},
    {"heex", ICON_ELIXIR, BLUE},
    {"hh", ICON_CPP, WHITE},
    {"hpp", ICON_CPP, WHITE},
    {"htaccess", ICON_CONF, WHITE},
    {"htpasswd", ICON_CONF, WHITE},
    {"hrl", ICON_ERLANG, RED},
    {"hs", ICON_HASKELL, BLUE},
    {"htm", ICON_HTML, WHITE},
    {"html", ICON_HTML, WHITE},
    {"hxx", ICON_CPP, WHITE},

    {"ico", ICON_IMG, GREEN},
    {"img", ICON_CD, B_WHITE},
    {"ini", ICON_CONF, WHITE},
    {"iso", ICON_CD, B_WHITE},

    {"jar", ICON_JAVA, B_WHITE},
    {"java", ICON_JAVA, B_WHITE},
    {"jl", ICON_JULIA, BLUE},
    {"jpeg", ICON_IMG, GREEN},
    {"jpg", ICON_IMG, GREEN},
    {"js", ICON_JAVASCRIPT, WHITE},
    {"json", ICON_JSON, WHITE},
    {"jsx", ICON_ELECTRON, B_GREEN},
    {"jxl", ICON_IMG, GREEN},

    {"key", ICON_KEY, YELLOW},
    {"kf8", ICON_BOOK, B_WHITE},
    {"kfx", ICON_BOOK, B_WHITE},
    {"ko", ICON_BINARY, WHITE},
    {"kt", ICON_KOTLIN, BLUE},
    {"kts", ICON_KOTLIN, BLUE},
    {"ksh", ICON_SCRIPT, WHITE},

    {"lha", ICON_ARCHIVE, YELLOW},
    {"lhs", ICON_HASKELL, BLUE},
    {"log", ICON_TXT, WHITE},
    {"lua", ICON_LUA, WHITE},
    {"lz4", ICON_ARCHIVE, YELLOW},
    {"lzh", ICON_ARCHIVE, YELLOW},
    {"lzma", ICON_ARCHIVE, YELLOW},

    {"m", ICON_MATLAB, YELLOW},
    {"m4a", ICON_AUDIO, YELLOW},
    {"m4v", ICON_VID, BLUE},
    {"markdown", ICON_MARKDOWN, WHITE},
    {"mat", ICON_MATLAB, YELLOW},
    {"md", ICON_MARKDOWN, WHITE},
    {"mk", ICON_MAKEFILE, WHITE},
    {"mkv", ICON_VID, BLUE},
    {"ml", ICON_OCAML, WHITE},
    {"mli", ICON_OCAML, WHITE},
    {"mov", ICON_VID, BLUE},
    {"mp3", ICON_AUDIO, YELLOW},
    {"mp4", ICON_VID, BLUE},
    {"mpeg", ICON_VID, BLUE},
    {"mpg", ICON_VID, BLUE},
    {"msi", ICON_WINDOWS, BLUE},

    {"nix", ICON_NIX, BLUE},

    {"o", ICON_BINARY, WHITE},
    {"odp", ICON_OPENOFFICE_P, YELLOW},
    {"ods", ICON_OPENOFFICE_S, GREEN},
    {"odt", ICON_OPENOFFICE_T, BLUE},
    {"ogg", ICON_AUDIO, YELLOW},
    {"ogv", ICON_VID, BLUE},
    {"opdownload", ICON_DOWNLOADS, WHITE},
    {"opus", ICON_AUDIO, YELLOW},
    {"otf", ICON_FONT, WHITE},
    {"out", ICON_ELF, WHITE},

    {"part", ICON_DOWNLOADS, WHITE},
    {"patch", ICON_PATCH, WHITE},
    {"pem", ICON_KEY, YELLOW},
    {"pdf", ICON_PDF, RED},
    {"pgn", ICON_CHESS, WHITE},
    {"php", ICON_PHP, WHITE},
    {"pl", ICON_PERL, YELLOW},
    {"plx", ICON_PERL, YELLOW},
    {"pm", ICON_PERL, YELLOW},
    {"png", ICON_IMG, GREEN},
    {"pod", ICON_PERL, WHITE},
    {"ppt", ICON_POWERPOINT, YELLOW},
    {"pptx", ICON_POWERPOINT, YELLOW},
    {"ps", ICON_POSTSCRIPT, RED},
    {"psb", ICON_PHOTOSHOP, WHITE},
    {"psv", ICON_PHOTOSHOP, WHITE},
    {"psd", ICON_PHOTOSHOP, WHITE},
    {"py", ICON_PYTHON, GREEN},
    {"pyc", ICON_PYTHON, GREEN},
    {"pyd", ICON_PYTHON, GREEN},
    {"pyo", ICON_PYTHON, GREEN},

    {"qcow", ICON_CD, B_WHITE},
    {"qcow2", ICON_CD, B_WHITE},

    {"r", ICON_R, WHITE},
    {"rda", ICON_R, WHITE},
    {"rdata", ICON_R, WHITE},
    {"rar", ICON_ARCHIVE, YELLOW},
    {"rb", ICON_RUBY, RED},
    {"rc", ICON_CONF, WHITE},
    {"rlib", ICON_RUST, WHITE},
    {"rom", ICON_ROM, WHITE},
    {"rpm", ICON_REDHAT, RED},
    {"rs", ICON_RUST, WHITE},
    {"rss", ICON_RSS, BLUE},
    {"rtf", ICON_PDF, RED},

    {"sass", ICON_SASS, B_BLUE},
    {"scala", ICON_SCALA, WHITE},
    {"service", ICON_SCRIPT, WHITE},
    {"sh", ICON_SCRIPT, WHITE},
    {"slim", ICON_CODE, WHITE},
    {"sln", ICON_VISUALSTUDIO, BLUE},
    {"so", ICON_BINARY, WHITE},
    {"sql", ICON_MYSQL, B_WHITE},
    {"sqlite", ICON_SQLITE, WHITE},
    {"srt", ICON_COMMENTS, WHITE},
    {"sub", ICON_COMMENTS, WHITE},
    {"svg", ICON_IMG, GREEN},
    {"swift", ICON_SWIFT, GREEN},

    {"t", ICON_PERL, YELLOW},
    {"tar", ICON_ARCHIVE, YELLOW},
    {"tbz2", ICON_ARCHIVE, YELLOW},
    {"tex", ICON_TEX, B_WHITE},
    {"tgz", ICON_ARCHIVE, YELLOW},
    {"tiff", ICON_IMG, GREEN},
    {"toml", ICON_CONF, WHITE},
    {"torrent", ICON_DOWNLOADS, WHITE},
    {"ts", ICON_TS, BLUE},
    {"tsx", ICON_REACT, B_BLUE},
    {"ttf", ICON_FONT, WHITE},
    {"txt", ICON_TXT, WHITE},
    {"txz", ICON_ARCHIVE, YELLOW},

    {"vdi", ICON_CD, B_WHITE},
    {"vhd", ICON_CD, B_WHITE},
    {"vhdx", ICON_CD, B_WHITE},
    {"vmdk", ICON_CD, B_WHITE},
    {"vid", ICON_VID, BLUE},
    {"vim", ICON_VIM, GREEN},
    {"vimrc", ICON_VIM, GREEN},
    {"vtt", ICON_COMMENTS, WHITE},

    {"wav", ICON_AUDIO, YELLOW},
    {"webm", ICON_VID, BLUE},
    {"webp", ICON_IMG, GREEN},
    {"wma", ICON_AUDIO, YELLOW},
    {"wmv", ICON_VID, BLUE},

    {"xbps", ICON_ARCHIVE, YELLOW},
    {"xcf", ICON_IMG, GREEN},
    {"xthml", ICON_HTML, WHITE},
    {"xls", ICON_EXCEL, GREEN},
    {"xlsx", ICON_EXCEL, GREEN},
    {"xml", ICON_CODE, WHITE},
    {"xs", ICON_PERL, YELLOW},
    {"xz", ICON_ARCHIVE, RED},

    {"yaml", ICON_CONF, WHITE},
    {"yml", ICON_CONF, WHITE},

    {"zig", ICON_ZIG, YELLOW},
    {"zip", ICON_ARCHIVE, YELLOW},
    {"zsh", ICON_SCRIPT, WHITE},
    {"zst", ICON_ARCHIVE, YELLOW},
};

/* Icons for some specific dir names */
struct icons_t const icon_dirnames[] = {
/* Translated version for these dirs should be added here
 * See https://github.com/alexanderjeurissen/ranger_devicons/blob/main/devicons.py*/
    {"home", ICON_HOME, DEF_DIR_ICON_COLOR},
    {".git", ICON_DOTGIT, DEF_DIR_ICON_COLOR},
    {".config", ICON_CONFIGURE, DEF_DIR_ICON_COLOR},
    {"Desktop", ICON_DESKTOP, DEF_DIR_ICON_COLOR},
    {"Documents", ICON_DOCUMENTS, DEF_DIR_ICON_COLOR},
    {"Downloads", ICON_DOWNLOADS, DEF_DIR_ICON_COLOR},
    {"Dropbox", ICON_DROPBOX, DEF_DIR_ICON_COLOR},
    {"games", ICON_GAMES, DEF_DIR_ICON_COLOR},
    {"Music", ICON_MUSIC, DEF_DIR_ICON_COLOR},
    {"Pictures", ICON_PICTURES, DEF_DIR_ICON_COLOR},
    {"Public", ICON_PUBLIC, DEF_DIR_ICON_COLOR},
    {"Steam", ICON_STEAM, DEF_DIR_ICON_COLOR},
    {"Templates", ICON_TEMPLATES, DEF_DIR_ICON_COLOR},
    {"Videos", ICON_VIDEOS, DEF_DIR_ICON_COLOR},
};

/* Icons for some specific file names */
struct icons_t const icon_filenames[] = {
/* More specific filenames from here https://github.com/alexanderjeurissen/ranger_devicons/blob/main/devicons.py */
    {".bash_history", ICON_CONF, WHITE},
    {".bash_logout", ICON_CONF, WHITE},
    {".bash_profile", ICON_CONF, WHITE},
    {".bashrc", ICON_CONF, WHITE},
    {".gitconfig", ICON_CONF, WHITE},
    {".gitignore", ICON_CONF, WHITE},
    {".inputrc", ICON_CONF, WHITE},
    {".vimrc", ICON_CONF, WHITE},
    {".Xdefaults", ICON_CONF, WHITE},
    {".xinitrc", ICON_CONF, WHITE},
    {".Xresources", ICON_CONF, WHITE},
    {".zshrc", ICON_CONF, WHITE},
    {"CHANGELOG", ICON_HISTORY, DEF_FILE_ICON_COLOR},
    {"clifmrc", ICON_CONF, WHITE},
    {"CMakeLists.txt", ICON_CMAKE, WHITE},
    {"configure", ICON_CONFIGURE, DEF_FILE_ICON_COLOR},
    {"Dockerfile", ICON_DOCKER, BLUE},
    {"License", ICON_COPYRIGHT, DEF_FILE_ICON_COLOR},
    {"Makefile", ICON_MAKEFILE, DEF_FILE_ICON_COLOR},
    {"PKGBUILD", ICON_ARCH, CYAN},
    {"README", ICON_README, YELLOW},
    {"TODO", ICON_LIST, WHITE},
};

#endif /* ICONS_H */
