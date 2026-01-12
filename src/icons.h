/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* icons.h - Icon definitions for Clifm */

/* Taken from
 * https://github.com/jarun/nnn, licensed under BSD-2-Clause.
 * All changes are licensed under GPL-2.0-or-later.
*/

#ifndef ICONS_H
#define ICONS_H

#if defined(EMOJI_ICONS)
# include "icons-emoji.h"
#elif defined(_ICONS_IN_TERMINAL)
# include "icons-in-terminal.h"
#else
# define NERDFONTS
# include "icons-nerdfont.h"
#endif /* _NERD */

struct icons_t {
	char *name;
	char *icon;
	char *color;
};

/* Icon macros are defined in icons-in-terminal.h (provided by the
 * 'icons-in-terminal' project), icons-nerdfont.h, and icons-emoji.h */
char
#if defined(EMOJI_ICONS)
	/* File types */
	ICON_DIR[] = EMOJI_FOLDER,
	ICON_EXEC[] = EMOJI_EXEC,
	ICON_LINK[] = EMOJI_LINK,
	ICON_LOCK[] = EMOJI_LOCK,
	ICON_REG[] = EMOJI_FILE,

	/* Extensions */
	ICON_ACCESS[] = EMOJI_DATABASE,
	ICON_ARCHIVE[] = EMOJI_ARCHIVE,
	ICON_ARDUINO[] = EMOJI_ARDUINO,
	ICON_ASM[] = EMOJI_ASM,
	ICON_AUDIO[] = EMOJI_AUDIO,
	ICON_BINARY[] = EMOJI_BINARY,
	ICON_BOOK[] = EMOJI_BOOK_OPEN,
	ICON_C[] = EMOJI_C,
	ICON_CACHE[] = EMOJI_CACHE,
	ICON_CALENDAR[] = EMOJI_CALENDAR,
	ICON_CD[] = EMOJI_CD,
	ICON_CERT[] = EMOJI_CERT,
	ICON_CHECKSUM[] = EMOJI_CHECKSUM,
	ICON_CLOJURE[] = EMOJI_CLOJURE,
	ICON_CMAKE[] = EMOJI_MAKE,
	ICON_CODE[] = EMOJI_CODE,
	ICON_COFFEE[] = EMOJI_COFFEE,
	ICON_CONFIG[] = EMOJI_CONF,
	ICON_COPYRIGHT[] = EMOJI_LICENSE,
	ICON_CPP[] = EMOJI_CPP,
	ICON_CSHARP[] = EMOJI_CSHARP,
	ICON_CSS[] = EMOJI_CSS,
	ICON_CUDA[] = EMOJI_C,
	ICON_D[] = EMOJI_D,
	ICON_DART[] = EMOJI_DART,
	ICON_DATABASE[] = EMOJI_DATABASE,
	ICON_DIFF[] = EMOJI_DIFF,
	ICON_DISK[] = EMOJI_DISK,
	ICON_DOCKER[] = EMOJI_DOCKER,
	ICON_ELECTRON[] = EMOJI_ELECTRON,
	ICON_ELF[] = EMOJI_BINARY,
	ICON_ELIXIR[] = EMOJI_ELIXIR,
	ICON_ELM[] = EMOJI_ELM,
	ICON_EMACS[] = EMOJI_SHELL,
	ICON_ERLANG[] = EMOJI_ERLANG,
	ICON_EXCEL[] = EMOJI_STYLESHEET,
	ICON_FONT[] = EMOJI_FONT,
	ICON_FORTRAN[] = EMOJI_FORTRAN,
	ICON_FSHARP[] = EMOJI_FSHARP,
	ICON_GIMP[] = EMOJI_GIMP,
	ICON_GLEAM[] = EMOJI_GLEAM,
	ICON_GO[] = EMOJI_GO,
	ICON_GODOT[] = EMOJI_GODOT,
	ICON_GRADLE[] = EMOJI_GRADLE,
	ICON_GRAPHQL[] = EMOJI_GRAPHQL,
	ICON_GRAPHVIZ[] = EMOJI_GRAPHVIZ,
	ICON_GROOVY[] = EMOJI_GROOVY,
	ICON_HAML[] = EMOJI_HAML,
	ICON_HASKELL[] = EMOJI_HASKELL,
	ICON_HEROKU[] = EMOJI_HEROKU,
	ICON_HISTORY[] = EMOJI_CHANGELOG,
	ICON_HTML[] = EMOJI_HTML,
	ICON_ILLUSTRATOR[] = EMOJI_ILLUSTRATOR,
	ICON_IMG[] = EMOJI_IMAGE,
	ICON_JAVA[] = EMOJI_JAVA,
	ICON_JAVASCRIPT[] = EMOJI_JAVASCRIPT,
	ICON_JENKINS[] = EMOJI_JENKINS,
	ICON_JSON[] = EMOJI_JSON,
	ICON_JULIA[] = EMOJI_JULIA,
	ICON_JUPYTER[] = EMOJI_JUPYTER,
	ICON_KEY[] = EMOJI_KEY,
	ICON_KICAD[] = EMOJI_KICAD,
	ICON_KOTLIN[] = EMOJI_KOTLIN,
	ICON_KRITA[] = EMOJI_KRITA,
	ICON_LIBREOFFICE_BASE[] = EMOJI_DATABASE,
	ICON_LIBREOFFICE_DRAW[] = EMOJI_IMAGE,
	ICON_LIBREOFFICE_CALC[] = EMOJI_STYLESHEET,
	ICON_LIBREOFFICE_IMPRESS[] = EMOJI_PRESENTATION,
	ICON_LIBREOFFICE_MATH[] = EMOJI_FILE,
	ICON_LIBREOFFICE_WRITER[] = EMOJI_WORD,
	ICON_LIST[] = EMOJI_LIST,
	ICON_LIVESCRIPT[] = EMOJI_LIVESCRIPT,
	ICON_LOG[] = EMOJI_LOG,
	ICON_LUA[] = EMOJI_LUA,
	ICON_MAGNET[] = EMOJI_MAGNET,
	ICON_MAKEFILE[] = EMOJI_MAKE,
	ICON_MANPAGE[] = EMOJI_MANUAL,
	ICON_MARKDOWN[] = EMOJI_MARKDOWN,
	ICON_MATLAB[] = EMOJI_MATLAB,
	ICON_MUSTACHE[] = EMOJI_MUSTACHE,
	ICON_MYSQL[] = EMOJI_DATABASE,
	ICON_NIM[] = EMOJI_NIM,
	ICON_NIX[] = EMOJI_SHELL,
	ICON_NODEJS[] = EMOJI_NODEJS,
	ICON_OCAML[] = EMOJI_OCAML,
	ICON_PACKAGE[] = EMOJI_PACKAGE,
	ICON_PDF[] = EMOJI_PDF,
	ICON_PERL[] = EMOJI_PERL,
	ICON_PHOTOSHOP[] = EMOJI_PHOTOSHOP,
	ICON_PHP[] = EMOJI_PHP,
	ICON_POSTSCRIPT[] = EMOJI_POSTSCRIPT,
	ICON_POWERSHELL[] = EMOJI_POWERSHELL,
	ICON_POWERPOINT[] = EMOJI_PRESENTATION,
	ICON_PURESCRIPT[] = EMOJI_PURESCRIPT,
	ICON_PYTHON[] = EMOJI_PYTHON,
	ICON_R[] = EMOJI_R,
	ICON_REACT[] = EMOJI_ELECTRON,
	ICON_README[] = EMOJI_TEXT,
	ICON_ROM[] = EMOJI_ROM,
	ICON_RSS[] = EMOJI_RSS,
	ICON_RUBY[] = EMOJI_RUBY,
	ICON_RUBYRAILS[] = EMOJI_RUBYRAILS,
	ICON_RUST[] = EMOJI_RUST,
	ICON_SASS[] = EMOJI_SASS,
	ICON_SCALA[] = EMOJI_SCALA,
	ICON_SCHEME[] = EMOJI_SCHEME,
	ICON_SHARE[] = EMOJI_SHARE,
	ICON_SHELL[] = EMOJI_SHELL,
	ICON_SIG[] = EMOJI_LIST,
	ICON_SQLITE[] = EMOJI_DATABASE,
	ICON_STYLUS[] = EMOJI_STYLUS,
	ICON_SUBLIME[] = EMOJI_SUBLIME,
	ICON_SVELTE[] = EMOJI_SVELTE,
	ICON_SWIFT[] = EMOJI_SWIFT,
	ICON_TCL[] = EMOJI_TCL,
	ICON_TEX[] = EMOJI_TEX,
	ICON_TOML[] = EMOJI_TOML,
	ICON_TS[] = EMOJI_TYPESCRIPT,
	ICON_TXT[] = EMOJI_NOTE,
	ICON_TWIG[] = EMOJI_TWIG,
	ICON_V[] = EMOJI_V,
	ICON_VAGRANT[] = EMOJI_VAGRANT,
	ICON_VALA[] = EMOJI_VALA,
	ICON_VIM[] = EMOJI_VIM,
	ICON_VID[] = EMOJI_MOVIE,
	ICON_VISUALSTUDIO[] = EMOJI_VISUALSTUDIO,
	ICON_VUE[] = EMOJI_VUE,
	ICON_WORD[] = EMOJI_WORD,
	ICON_XAML[] = EMOJI_XAML,
	ICON_XML[] = EMOJI_XML,
	ICON_YAML[] = EMOJI_YAML,
	ICON_ZIG[] = EMOJI_FILE,

	/* Dir names */
	ICON_CONFIG_DIR[] = EMOJI_CONF,
	ICON_DESKTOP[] = EMOJI_DESKTOP,
	ICON_DOCUMENTS[] = EMOJI_BRIEFCASE,
	ICON_DOWNLOADS[] = EMOJI_DOWNLOAD,
	ICON_DROPBOX[] = EMOJI_DROPBOX,
	ICON_FAV[] = EMOJI_FOLDER,
	ICON_GAMES[] = EMOJI_GAMES,
	ICON_GIT[] = EMOJI_GIT,
	ICON_HOME[] = EMOJI_HOME,
	ICON_MUSIC[] = EMOJI_MUSIC,
	ICON_ONEDRIVE[] = EMOJI_ONEDRIVE,
	ICON_PLAYLIST[] = EMOJI_PLAYLIST,
	ICON_PICTURES[] = EMOJI_PICTURE,
	ICON_PUBLIC[] = EMOJI_PUBLIC,
	ICON_STEAM[] = EMOJI_STEAM,
	ICON_SUBTITLES[] = EMOJI_SUBTITLES,
	ICON_TEMPLATES[] = EMOJI_TEMPLATE,
	ICON_TRASH[] = EMOJI_TRASH,
	ICON_VIDEOS[] = EMOJI_VIDEOS;

#elif defined(_ICONS_IN_TERMINAL)
	/* File types */
	ICON_DIR[] = FA_FOLDER,
	ICON_EXEC[] = FA_COG,
	ICON_LINK[] = OCT_FILE_SYMLINK_FILE,
	ICON_LOCK[] = FA_LOCK,
	ICON_REG[] = FA_FILE_O,

	/* Extensions */
	ICON_ACCESS[] = FILE_ACCESS,
	ICON_ARCHIVE[] = FA_ARCHIVE,
	ICON_ARDUINO[] = FILE_ARDUINO,
	ICON_ASM[] = FILE_NASM,
	ICON_AUDIO[] = FA_FILE_AUDIO_O,
	ICON_BINARY[] = OCT_FILE_BINARY,
	ICON_BOOK[] = OCT_BOOK,
	ICON_C[] = MFIZZ_C,
	ICON_CACHE[] = MD_CACHED,
	ICON_CALENDAR[] = FA_CALENDAR,
	ICON_CD[] = LINEA_MUSIC_CD,
	ICON_CERT[] = MD_VPN_KEY,
	ICON_CHECKSUM[] = MD_VERIFIED_USER,
	ICON_CLOJURE[] = MFIZZ_CLOJURE,
	ICON_CMAKE[] = FILE_CMAKE,
	ICON_CODE[] = FA_FILE_CODE_O,
	ICON_COFFEE[] = DEV_COFFEESCRIPT,
	ICON_CONFIG[] = FA_SLIDERS,
	ICON_COPYRIGHT[] = OCT_LAW,
	ICON_CPP[] = MFIZZ_CPLUSPLUS,
	ICON_CSHARP[] = MFIZZ_CSHARP,
	ICON_CSS[] = MFIZZ_CSS3_ALT,
	ICON_CUDA[] = MFIZZ_C,
	ICON_D[] = DEV_DLANG,
	ICON_DART[] = DEV_DART,
	ICON_DATABASE[] = FA_DATABASE,
	ICON_DIFF[] = OCT_DIFF,
	ICON_DISK[] = FA_FLOPPY_O,
	ICON_DOCKER[] = MFIZZ_DOCKER,
	ICON_ELECTRON[] = FILE_ELECTRON,
	ICON_ELF[] = OCT_FILE_BINARY,
	ICON_ELIXIR[] = MFIZZ_ELIXIR,
	ICON_ELM[] = MFIZZ_ELM,
	ICON_EMACS[] = FILE_EMACS,
	ICON_ERLANG[] = DEV_ERLANG,
	ICON_EXCEL[] = FILE_EXCEL,
	ICON_FONT[] = FILE_FONT,
	ICON_FORTRAN[] = FILE_FORTRAN,
	ICON_FSHARP[] = DEV_FSHARP,
	ICON_GIMP[] = FA_FILE_IMAGE_O, /* No Gimp icon */
	ICON_GLEAM[] = FA_STAR,
	ICON_GO[] = FILE_GO,
	ICON_GODOT[] = FILE_GODOT,
	ICON_GRADLE[] = FILE_GRADLE,
	ICON_GRAPHQL[] = FILE_GRAPHQL,
	ICON_GRAPHVIZ[] = FILE_GRAPHVIZ,
	ICON_GROOVY[] = FILE_GROOVY,
	ICON_HAML[] = FILE_HAML,
	ICON_HASKELL[] = MFIZZ_HASKELL,
	ICON_HEROKU[] = MFIZZ_HEROKU,
	ICON_HISTORY[] = FA_HISTORY,
	ICON_HTML[] = MFIZZ_HTML5_ALT,
	ICON_ILLUSTRATOR[] = DEV_ILLUSTRATOR,
	ICON_IMG[] = FA_FILE_IMAGE_O,
	ICON_JAVA[] = MFIZZ_JAVA,
	ICON_JAVASCRIPT[] = DEV_JAVASCRIPT,
	ICON_JENKINS[] = FILE_JENKINS,
	ICON_JSON[] = FILE_SWAGGER, /* No json icon */
	ICON_JULIA[] = FILE_JULIA,
	ICON_JUPYTER[] = FILE_JUPYTER,
	ICON_KEY[] = MD_VPN_KEY,
	ICON_KICAD[] = FILE_KICAD,
	ICON_KOTLIN[] = FILE_KOTLIN,
	ICON_KRITA[] = FA_FILE_IMAGE_O, /* No krita icon */
	ICON_LIBREOFFICE_BASE[] = FILE_OPENOFFICE,
	ICON_LIBREOFFICE_CALC[] = FILE_OPENOFFICE,
	ICON_LIBREOFFICE_DRAW[] = FILE_OPENOFFICE,
	ICON_LIBREOFFICE_IMPRESS[] = FILE_OPENOFFICE,
	ICON_LIBREOFFICE_MATH[] = FILE_OPENOFFICE,
	ICON_LIBREOFFICE_WRITER[] = FILE_OPENOFFICE,
	ICON_LIST[] = FA_CHECK_SQUARE_O,
	ICON_LIVESCRIPT[] = FILE_LS,
	ICON_LOG[] = MD_FORMAT_ALIGN_LEFT,
	ICON_LUA[] = FILE_LUA,
	ICON_MAGNET[] = FA_MAGNET,
	ICON_MANPAGE[] = MD_HELP_OUTLINE,
	ICON_MAKEFILE[] = FILE_MAPBOX,
	ICON_MATLAB[] = FILE_MATLAB,
	ICON_MARKDOWN[] = OCT_MARKDOWN,
	ICON_MUSTACHE[] = FILE_MUSTACHE,
	ICON_MYSQL[] = MFIZZ_MYSQL_ALT,
	ICON_NIM[] = FILE_NIM,
	ICON_NIX[] = LINUX_NIXOS,
	ICON_NODEJS[] = MFIZZ_NODEJS,
	ICON_OCAML[] = FILE_OCAML,
	ICON_PACKAGE[] = OCT_PACKAGE,
	ICON_PDF[] = FA_FILE_PDF_O,
	ICON_PERL[] = MFIZZ_PERL,
	ICON_PHOTOSHOP[] = DEV_PHOTOSHOP,
	ICON_PHP[] = MFIZZ_PHP_ALT,
	ICON_POSTSCRIPT[] = FILE_POSTSCRIPT,
	ICON_POWERSHELL[] = FILE_POWERSHELL,
	ICON_POWERPOINT[] = FILE_POWERPOINT,
	ICON_PURESCRIPT[] = FILE_PURESCRIPT,
	ICON_PYTHON[] = MFIZZ_PYTHON,
	ICON_R[] = FILE_R,
	ICON_REACT[] = FILE_REACT,
	ICON_README[] = OCT_BOOK,
	ICON_ROM[] = FA_MICROCHIP,
	ICON_RSS[] = MD_RSS_FEED,
	ICON_RUBY[] = MFIZZ_RUBY,
	ICON_RUBYRAILS[] = DEV_RUBY_ON_RAILS,
	ICON_RUST[] = MFIZZ_RUST,
	ICON_SASS[] = MFIZZ_SASS,
	ICON_SCALA[] = MFIZZ_SCALA,
	ICON_SCHEME[] = FILE_SCHEME,
	ICON_SHARE[] = MD_SHARE,
	ICON_SHELL[] = OCT_TERMINAL,
	ICON_SIG[] = MD_PLAYLIST_ADD_CHECK,
	ICON_SQLITE[] = MFIZZ_MYSQL_ALT,
	ICON_STYLUS[] = FILE_STYLUS,
	ICON_SUBLIME[] = DEV_SUBLIME,
	ICON_SVELTE[] = FILE_POLYMER, /* No svelte icon. */
	ICON_SWIFT[] = DEV_SWIFT,
	ICON_TCL[] = FILE_TCL,
	ICON_TEX[] = FILE_TEX,
	ICON_TOML[] = FA_SLIDERS, /* No toml icon */
	ICON_TS[] = FILE_TS,
	ICON_TXT[] = FA_FILE_TEXT_O,
	ICON_TWIG[] = FILE_TWIG,
	ICON_V[] = FILE_VAGRANT, /* No V lang icon. */
	ICON_VAGRANT[] = FILE_VAGRANT,
	ICON_VALA[] = FA_VIMEO,
	ICON_VID[] = FA_FILE_MOVIE_O,
	ICON_VIM[] = DEV_VIM,
	ICON_VISUALSTUDIO[] = DEV_VISUALSTUDIO,
	ICON_VUE[] = FILE_VUE,
	ICON_WORD[] = FILE_WORD,
	ICON_XAML[] = FA_CODE, /* No xaml icon */
	ICON_XML[] = FA_CODE,
	ICON_YAML[] = FA_SLIDERS, /* No yaml icon */
	ICON_ZIG[] = FA_FILE_O,

	/* Dir names */
	ICON_CONFIG_DIR[] = FA_SLIDERS,
	ICON_DESKTOP[] = FA_DESKTOP,
	ICON_DOCUMENTS[] = FA_BRIEFCASE,
	ICON_DOWNLOADS[] = MD_FILE_DOWNLOAD,
	ICON_DROPBOX[] = FA_DROPBOX,
	ICON_FAV[] = FA_STAR_O,
	ICON_GAMES[] = MD_GAMES,
	ICON_GIT[] = MFIZZ_GIT,
	ICON_HOME[] = FA_HOME,
	ICON_MUSIC[] = FA_MUSIC,
	ICON_ONEDRIVE[] = DEV_ONEDRIVE,
	ICON_PLAYLIST[] = MD_QUEUE_MUSIC,
	ICON_PICTURES[] = MD_CAMERA_ALT,
	ICON_PUBLIC[] = FA_INBOX,
	ICON_STEAM[] = FA_STEAM,
	ICON_SUBTITLES[] = MD_SUBTITLES,
	ICON_TEMPLATES[] = FA_PAPERCLIP,
	ICON_TRASH[] = FA_TRASH,
	ICON_VIDEOS[] = FA_FILM;

#else /* Nerdfonts */
	/* File types */
	ICON_DIR[] = NERD_DIRECTORY,
	ICON_EXEC[] = NERD_EXEC,
	ICON_LOCK[] = NERD_LOCK,
	ICON_LINK[] = NERD_LINK,
	ICON_REG[] = NERD_FILE,

	/* Extensions */
	ICON_ACCESS[] = NERD_ACCESS,
	ICON_ARCHIVE[] = NERD_ARCHIVE,
	ICON_ARDUINO[] = NERD_ARDUINO,
	ICON_ASM[] = NERD_ASM,
	ICON_AUDIO[] = NERD_MUSICFILE,
	ICON_BINARY[] = NERD_BINARY,
	ICON_BOOK[] = NERD_BOOK,
	ICON_C[] = NERD_C,
	ICON_CACHE[] = NERD_CACHE,
	ICON_CALENDAR[] = NERD_CALENDAR,
	ICON_CD[] = NERD_OPTICALDISK,
	ICON_CERT[] = NERD_CERT,
	ICON_CHECKSUM[] = NERD_CHECKSUM,
	ICON_CLOJURE[] = NERD_CLOJURE,
	ICON_CMAKE[] = NERD_CMAKE,
	ICON_CODE[] = NERD_FILE,
	ICON_COFFEE[] = NERD_COFFEE,
	ICON_CONFIG[] = NERD_CONFIG_FILE,
	ICON_COPYRIGHT[] = NERD_COPYRIGHT,
	ICON_CPP[] = NERD_CPLUSPLUS,
	ICON_CSHARP[] = NERD_CSHARP,
	ICON_CSS[] = NERD_CSS3,
	ICON_CUDA[] = NERD_CUDA,
	ICON_D[] = NERD_D,
	ICON_DART[] = NERD_DART,
	ICON_DATABASE[] = NERD_DATABASE,
	ICON_DIFF[] = NERD_DIFF,
	ICON_DISK[] = NERD_DISK,
	ICON_DOCKER[] = NERD_DOCKER,
	ICON_ELECTRON[] = NERD_ELECTRON,
	ICON_ELF[] = NERD_BINARY,
	ICON_ELM[] = NERD_ELM,
	ICON_ELIXIR[] = NERD_ELIXIR,
	ICON_EMACS[] = NERD_EMACS,
	ICON_ERLANG[] = NERD_ERLANG,
	ICON_EXCEL[] = NERD_EXCELDOC,
	ICON_FONT[] = NERD_FONT,
	ICON_FORTRAN[] = NERD_FORTRAN,
	ICON_FSHARP[] = NERD_FSHARP,
	ICON_GIMP[] = NERD_GIMP,
	ICON_GLEAM[] = NERD_GLEAM,
	ICON_GO[] = NERD_GO,
	ICON_GODOT[] = NERD_GODOT,
	ICON_GRADLE[] = NERD_GRADLE,
	ICON_GRAPHQL[] = NERD_GRAPHQL,
	ICON_GRAPHVIZ[] = NERD_GRAPHVIZ,
	ICON_GROOVY[] = NERD_GROOVY,
	ICON_HAML[] = NERD_HAML,
	ICON_HASKELL[] = NERD_HASKELL,
	ICON_HEROKU[] = NERD_HEROKU,
	ICON_HISTORY[] = NERD_HISTORY,
	ICON_HTML[] = NERD_HTML,
	ICON_ILLUSTRATOR[] = NERD_ILLUSTRATOR,
	ICON_IMG[] = NERD_PICTUREFILE,
	ICON_JAVA[] = NERD_JAVA,
	ICON_JAVASCRIPT[] = NERD_JAVASCRIPT,
	ICON_JENKINS[] = NERD_JENKINS,
	ICON_JSON[] = NERD_JSON,
	ICON_JULIA[] = NERD_JULIA,
	ICON_JUPYTER[] = NERD_JUPYTER,
	ICON_KEY[] = NERD_KEY,
	ICON_KICAD[] = NERD_KICAD,
	ICON_KOTLIN[] = NERD_KOTLIN,
	ICON_KRITA[] = NERD_KRITA,
	ICON_LIBREOFFICE_BASE[] = NERD_LIBREOFFICE_BASE,
	ICON_LIBREOFFICE_CALC[] = NERD_LIBREOFFICE_CALC,
	ICON_LIBREOFFICE_DRAW[] = NERD_LIBREOFFICE_DRAW,
	ICON_LIBREOFFICE_IMPRESS[] = NERD_LIBREOFFICE_IMPRESS,
	ICON_LIBREOFFICE_MATH[] = NERD_LIBREOFFICE_MATH,
	ICON_LIBREOFFICE_WRITER[] = NERD_LIBREOFFICE_WRITER,
	ICON_LIST[] = NERD_CHECKLIST,
	ICON_LIVESCRIPT[] = NERD_LIVESCRIPT,
	ICON_LOG[] = NERD_LOG,
	ICON_LUA[] = NERD_LUA,
	ICON_MAGNET[] = NERD_MAGNET,
	ICON_MAKEFILE[] = NERD_MAKEFILE,
	ICON_MANPAGE[] = NERD_MANUAL,
	ICON_MARKDOWN[] = NERD_MARKDOWN,
	ICON_MATLAB[] = NERD_MATLAB,
	ICON_MUSTACHE[] = NERD_MUSTACHE,
	ICON_MYSQL[] = NERD_MYSQL,
	ICON_NIM[] = NERD_NIM,
	ICON_NIX[] = NERD_NIX,
	ICON_NODEJS[] = NERD_NODEJS,
	ICON_OCAML[] = NERD_OCAML,
	ICON_PACKAGE[] = NERD_PACKAGE,
	ICON_PDF[] = NERD_PDF,
	ICON_PERL[] = NERD_PERL,
	ICON_PHOTOSHOP[] = NERD_PHOTOSHOP,
	ICON_PHP[] = NERD_PHP,
	ICON_POSTSCRIPT[] = NERD_POSTSCRIPT,
	ICON_POWERSHELL[] = NERD_POWERSHELL,
	ICON_POWERPOINT[] = NERD_PPTDOC,
	ICON_PURESCRIPT[] = NERD_PURESCRIPT,
	ICON_PYTHON[] = NERD_PYTHON,
	ICON_R[] = NERD_R,
	ICON_REACT[] = NERD_REACT,
	ICON_README[] = NERD_BOOK,
	ICON_ROM[] = NERD_ROM,
	ICON_RSS[] = NERD_RSS,
	ICON_RUBY[] = NERD_RUBY,
	ICON_RUBYRAILS[] = NERD_RUBYRAILS,
	ICON_RUST[] = NERD_RUST,
	ICON_SASS[] = NERD_SASS,
	ICON_SCALA[] = NERD_SCALA,
	ICON_SCHEME[] = NERD_SCHEME,
	ICON_SHARE[] = NERD_SHARE,
	ICON_SHELL[] = NERD_SHELL,
	ICON_SIG[] = NERD_SIG,
	ICON_SQLITE[] = NERD_SQLITE,
	ICON_STYLUS[] = NERD_STYLUS,
	ICON_SUBLIME[] = NERD_SUBLIME,
	ICON_SVELTE[] = NERD_SVELTE,
	ICON_SWIFT[] = NERD_SWIFT,
	ICON_TCL[] = NERD_TCL,
	ICON_TEX[] = NERD_TEX,
	ICON_TOML[] = NERD_TOML,
	ICON_TS[] = NERD_TS,
	ICON_TXT[] = NERD_TXT,
	ICON_TWIG[] = NERD_TWIG,
	ICON_V[] = NERD_V,
	ICON_VAGRANT[] = NERD_VAGRANT,
	ICON_VALA[] = NERD_VALA,
	ICON_VID[] = NERD_VIDEOFILE,
	ICON_VIM[] = NERD_VIM,
	ICON_VISUALSTUDIO[] = NERD_VISUALSTUDIO,
	ICON_VUE[] = NERD_VUE,
	ICON_WORD[] = NERD_WORDDOC,
	ICON_XAML[] = NERD_XAML,
	ICON_XML[] = NERD_XML,
	ICON_YAML[] = NERD_YAML,
	ICON_ZIG[] = NERD_ZIG,

	/* Dir names */
	ICON_CONFIG_DIR[] = NERD_CONFIG_DIR,
	ICON_DESKTOP[] = NERD_DESKTOP,
	ICON_DOCUMENTS[] = NERD_BRIEFCASE,
	ICON_DOWNLOADS[] = NERD_DOWNLOADS,
	ICON_DROPBOX[] = NERD_DROPBOX,
	ICON_FAV[] = NERD_FAV,
	ICON_GAMES[] = NERD_GAMES,
	ICON_GIT[] = NERD_GIT,
	ICON_HOME[] = NERD_HOME,
	ICON_MUSIC[] = NERD_MUSIC,
	ICON_ONEDRIVE[] = NERD_ONEDRIVE,
	ICON_PLAYLIST[] = NERD_PLAYLIST,
	ICON_TRASH[] = NERD_TRASH,
	ICON_PICTURES[] = NERD_PICTURES,
	ICON_PUBLIC[] = NERD_PUBLIC,
	ICON_STEAM[] = NERD_STEAM,
	ICON_SUBTITLES[] = NERD_SUBTITLES,
	ICON_TEMPLATES[] = NERD_TEMPLATES,
	ICON_VIDEOS[] = NERD_VIDEOS;
#endif /* _EMOJI_ICONS */

#if !defined(EMOJI_ICONS)
# define BLUE "\x1b[0;34m"
# define B_BLUE "\x1b[1;34m"
# define WHITE "\x1b[0;37m"
# define YELLOW "\x1b[0;33m"
# define B_YELLOW "\x1b[1;33m"
# define GREEN "\x1b[0;32m"
# define B_GREEN "\x1b[1;32m"
# define CYAN "\x1b[0;36m"
# define MAGENTA "\x1b[0;35m"
# define RED "\x1b[0;31m"
#else
# define EMPTY_STR ""
# define BLUE EMPTY_STR
# define B_BLUE EMPTY_STR
# define WHITE EMPTY_STR
# define YELLOW EMPTY_STR
# define B_YELLOW EMPTY_STR
# define GREEN EMPTY_STR
# define B_GREEN EMPTY_STR
# define CYAN EMPTY_STR
# define MAGENTA EMPTY_STR
# define RED EMPTY_STR
#endif /* _ICONS_IN_TERMINAL || !_ICONS_EMOJI */

/* Default icons and colors */
#define DEF_DIR_ICON ICON_DIR /* Directory */
#define DEF_DIR_ICON_COLOR YELLOW

#define DEF_LINK_ICON ICON_LINK /* Symbolic link */
#define DEF_LINK_ICON_COLOR CYAN

#define DEF_EXEC_ICON ICON_EXEC /* Executable file */
#define DEF_EXEC_ICON_COLOR WHITE

#define DEF_NOPERM_ICON ICON_LOCK /* File without access permission */
#define DEF_NOPERM_ICON_COLOR_DIR  YELLOW
#define DEF_NOPERM_ICON_COLOR_FILE RED

#define DEF_FILE_ICON ICON_REG /* None of the above */
#define DEF_FILE_ICON_COLOR WHITE

/* Per file extension icons */
/* Criteria for icon assignment:
* 1. If the file/extension is associated to a specific program (e.g. kra files),
*    use the icon for that program.
* 2. Use a generic file type icon (e.g. an image icon for PNG files, or a C
*    icon for C files).
* 3. Fallback to a generic file icon.
*
* The general idea is to give the user a hint about how to open/handle the file. */

struct icons_t const icon_ext[] = {
	{"7z", ICON_ARCHIVE, YELLOW},
	{"3gp", ICON_VID, BLUE},

	{"a", ICON_BINARY, WHITE},
	{"aac", ICON_AUDIO, YELLOW},
	{"abw", ICON_LIBREOFFICE_WRITER, BLUE},
	{"aiff", ICON_AUDIO, B_YELLOW}, // Lossless
	{"alac", ICON_AUDIO, B_YELLOW}, // Lossless
	{"arj", ICON_ARCHIVE, YELLOW},
	{"asc", ICON_LOCK, GREEN},
	{"asm", ICON_ASM, BLUE},
	{"avi", ICON_VID, BLUE},
	{"avif", ICON_IMG, GREEN},
	{"awk", ICON_SHELL, WHITE},
	{"azw", ICON_BOOK, WHITE},
	{"azw3", ICON_BOOK, WHITE},

	{"bash", ICON_SHELL, WHITE},
	{"bat", ICON_SHELL, WHITE},
#ifdef _ICONS_IN_TERMINAL
	{"bib", FILE_BIBTEX, WHITE},
#else
	{"bib", ICON_TEX, WHITE},
#endif
	{"bin", ICON_BINARY, WHITE},
	{"bmp", ICON_IMG, GREEN},
	{"bz2", ICON_ARCHIVE, YELLOW},
	{"bz3", ICON_ARCHIVE, YELLOW},

	{"c", ICON_C, BLUE},
	{"c++", ICON_CPP, B_BLUE},
	{"cab", ICON_ARCHIVE, YELLOW},
	{"cbr", ICON_BOOK, YELLOW},
	{"cert", ICON_CERT, RED},
	{"crt", ICON_CERT, RED},
	{"cbz", ICON_BOOK, YELLOW},
	{"cc", ICON_CPP, B_BLUE},
	{"cfg", ICON_CONFIG, WHITE},
	{"cjs", ICON_JAVASCRIPT, WHITE},
	{"clifm", ICON_CONFIG, WHITE},
	{"class", ICON_JAVA, WHITE},
	{"clj", ICON_CLOJURE, GREEN},
	{"cljc", ICON_CLOJURE, GREEN},
	{"cljs", ICON_CLOJURE, B_BLUE},
	{"cls", ICON_TEX, WHITE},
	{"cmake", ICON_CMAKE, WHITE},
	{"coffee", ICON_COFFEE, WHITE},
	{"conf", ICON_CONFIG, WHITE},
	{"cpio", ICON_ARCHIVE, WHITE}, // Uncompressed
	{"cpp", ICON_CPP, B_BLUE},
	{"crdownload", ICON_DOWNLOADS, WHITE},
	{"crate", ICON_ARCHIVE, YELLOW},
	{"csh", ICON_SHELL, WHITE},
	{"cs", ICON_CSHARP, MAGENTA},
	{"css", ICON_CSS, BLUE},
	{"csx", ICON_CSHARP, MAGENTA},
	{"cts", ICON_TS, BLUE},
	{"cue", ICON_PLAYLIST, YELLOW},
	{"cvs", ICON_CONFIG, WHITE},
	{"cxx", ICON_CPP, B_BLUE},

	{"d", ICON_D, RED},
	{"dat", ICON_BINARY, WHITE},
	{"dart", ICON_DART, BLUE},
	{"db", ICON_DATABASE, WHITE},
	{"db3", ICON_SQLITE, BLUE},
	{"deb", ICON_PACKAGE, YELLOW},
	{"desktop", ICON_CONFIG, WHITE},
	{"di", ICON_D, RED},
	{"diff", ICON_DIFF, WHITE},
	{"djvu", ICON_BOOK, WHITE},
	{"dll", ICON_SHARE, BLUE},
	{"dmg", ICON_DISK, WHITE},
	{"doc", ICON_WORD, BLUE},
	{"docx", ICON_WORD, BLUE},

	{"eex", ICON_ELIXIR, BLUE},
	{"ejs", ICON_JAVASCRIPT, WHITE},
	{"el", ICON_EMACS, BLUE},
	{"elf", ICON_ELF, WHITE},
	{"elm", ICON_ELM, GREEN},
	{"epub", ICON_BOOK, WHITE},
	{"erb", ICON_RUBYRAILS, RED},
	{"erl", ICON_ERLANG, RED},
	{"ex", ICON_ELIXIR, BLUE},
	{"exe", ICON_EXEC, WHITE},
	{"exs", ICON_ELIXIR, BLUE},

	{"f", ICON_FORTRAN, MAGENTA},
	{"f90", ICON_FORTRAN, MAGENTA},
	{"f#", ICON_FSHARP, CYAN},
	{"fish", ICON_SHELL, WHITE},
	{"flac", ICON_AUDIO, B_YELLOW}, // Lossless
	{"flv", ICON_VID, BLUE},
	{"for", ICON_FORTRAN, MAGENTA},
	{"fs", ICON_FSHARP, CYAN},
	{"fsi", ICON_FSHARP, CYAN},
	{"fsscript", ICON_FSHARP, CYAN},
	{"fsx", ICON_FSHARP, CYAN},

	{"gem", ICON_ARCHIVE, YELLOW},
	{"gemspec", ICON_RUBY, RED},
	{"gif", ICON_IMG, GREEN},
	{"gleam", ICON_GLEAM, MAGENTA},
	{"gpg", ICON_LOCK, GREEN},
	{"go", ICON_GO, B_BLUE},
	{"gz", ICON_ARCHIVE, YELLOW},
	{"gzip", ICON_ARCHIVE, YELLOW},

	{"h", ICON_C, BLUE},
	{"haml", ICON_HAML, YELLOW},
	{"hbs", ICON_MUSTACHE, WHITE},
	{"heic", ICON_IMG, GREEN},
	{"heif", ICON_IMG, GREEN},
	{"hh", ICON_CPP, B_BLUE},
	{"hpp", ICON_CPP, B_BLUE},
	{"hrl", ICON_ERLANG, RED},
	{"hs", ICON_HASKELL, BLUE},
	{"htm", ICON_HTML, YELLOW},
	{"html", ICON_HTML, YELLOW},
	{"hxx", ICON_CPP, B_BLUE},

	{"ico", ICON_IMG, GREEN},
	{"img", ICON_DISK, WHITE},
	{"ini", ICON_CONFIG, WHITE},
	{"ino", ICON_ARDUINO, GREEN},
	{"ipynb", ICON_JUPYTER, RED},
	{"iso", ICON_CD, WHITE},

	{"jar", ICON_ARCHIVE, YELLOW}, // Java archive
	{"java", ICON_JAVA, WHITE},
	{"jl", ICON_JULIA, BLUE},
	{"jpeg", ICON_IMG, GREEN},
	{"jpg", ICON_IMG, GREEN},
	{"js", ICON_JAVASCRIPT, WHITE},
	{"json", ICON_JSON, WHITE},
	{"jsx", ICON_ELECTRON, B_GREEN},
	{"jxl", ICON_IMG, GREEN},

	{"key", ICON_KEY, YELLOW},
	{"kfx", ICON_BOOK, WHITE},
	{"ko", ICON_BINARY, WHITE},
	{"kra", ICON_KRITA, MAGENTA},
	{"kt", ICON_KOTLIN, BLUE},
	{"kts", ICON_KOTLIN, BLUE},
	{"ksh", ICON_SHELL, WHITE},

	{"leex", ICON_ELIXIR, BLUE},
	{"lhs", ICON_HASKELL, BLUE},
	{"lock", ICON_LOCK, WHITE},
	{"log", ICON_LOG, WHITE},
	{"ls", ICON_LIVESCRIPT, CYAN},
	{"lua", ICON_LUA, BLUE},
	{"lz", ICON_ARCHIVE, YELLOW},
	{"lz4", ICON_ARCHIVE, YELLOW},
	{"lzh", ICON_ARCHIVE, YELLOW},
	{"lzma", ICON_ARCHIVE, YELLOW},

	{"m", ICON_C, BLUE},
	{"m4a", ICON_AUDIO, YELLOW},
	{"m4v", ICON_VID, BLUE},
	{"markdown", ICON_MARKDOWN, WHITE},
	{"mat", ICON_MATLAB, YELLOW},
	{"md", ICON_MARKDOWN, WHITE},
	{"mdb", ICON_ACCESS, RED},
	{"mjs", ICON_JAVASCRIPT, WHITE},
	{"mk", ICON_MAKEFILE, B_BLUE},
	{"mkv", ICON_VID, BLUE},
	{"ml", ICON_OCAML, WHITE},
	{"mli", ICON_OCAML, WHITE},
	{"mm", ICON_CPP, B_BLUE},
	{"mov", ICON_VID, BLUE},
	{"mp3", ICON_AUDIO, YELLOW},
	{"mp4", ICON_VID, BLUE},
	{"mpeg", ICON_VID, BLUE},
	{"mpg", ICON_VID, BLUE},
	{"mts", ICON_TS, BLUE},
	{"mustache", ICON_MUSTACHE, WHITE},

	{"nim", ICON_NIM, YELLOW},
	{"nimble", ICON_NIM, YELLOW},
	{"nims", ICON_NIM, YELLOW},
	{"nix", ICON_NIX, BLUE},
	{"nu", ICON_SHELL, WHITE},

	{"o", ICON_BINARY, WHITE},
	{"odb", ICON_LIBREOFFICE_BASE, RED},
	{"odg", ICON_LIBREOFFICE_DRAW, MAGENTA},
	{"odp", ICON_LIBREOFFICE_IMPRESS, YELLOW},
	{"ods", ICON_LIBREOFFICE_CALC, GREEN},
	{"odt", ICON_LIBREOFFICE_WRITER, BLUE},
	{"ogg", ICON_AUDIO, YELLOW},
	{"ogv", ICON_VID, BLUE},
	{"opus", ICON_AUDIO, YELLOW},
	{"otf", ICON_FONT, BLUE},
	{"out", ICON_ELF, WHITE},

	{"part", ICON_DOWNLOADS, WHITE},
	{"patch", ICON_DIFF, WHITE},
	{"pem", ICON_KEY, YELLOW},
	{"pdf", ICON_PDF, RED},
	{"php", ICON_PHP, MAGENTA},
	{"pl", ICON_PERL, YELLOW},
	{"plx", ICON_PERL, YELLOW},
	{"pm", ICON_PERL, YELLOW},
	{"png", ICON_IMG, GREEN},
	{"pod", ICON_PERL, YELLOW},
	{"ppt", ICON_POWERPOINT, YELLOW},
	{"pptx", ICON_POWERPOINT, YELLOW},
	{"ps", ICON_POSTSCRIPT, RED},
	{"psd", ICON_PHOTOSHOP, B_BLUE},
	{"pub", ICON_KEY, GREEN},
	{"py", ICON_PYTHON, GREEN},
	{"pyc", ICON_PYTHON, GREEN},
	{"pyd", ICON_PYTHON, GREEN},
	{"pyo", ICON_PYTHON, GREEN},

	{"qcow", ICON_DISK, WHITE},
	{"qcow2", ICON_DISK, WHITE},

	{"r", ICON_R, BLUE},
	{"rar", ICON_ARCHIVE, YELLOW},
	{"rdata", ICON_R, BLUE},
	{"rb", ICON_RUBY, RED},
	{"rc", ICON_CONFIG, WHITE},
	{"rlib", ICON_RUST, WHITE},
	{"rpm", ICON_PACKAGE, YELLOW},
	{"rs", ICON_RUST, WHITE},
	{"rss", ICON_RSS, BLUE},
	{"rtf", ICON_WORD, CYAN},

	{"s", ICON_ASM, BLUE},
	{"sass", ICON_SASS, B_BLUE},
	{"scala", ICON_SCALA, RED},
	{"scss", ICON_SASS, B_BLUE},
	{"service", ICON_SHELL, WHITE},
	{"sh", ICON_SHELL, WHITE},
	{"slim", ICON_RUBYRAILS, RED},
	{"sln", ICON_VISUALSTUDIO, BLUE},
	{"so", ICON_BINARY, WHITE},
	{"sql", ICON_MYSQL, CYAN},
	{"sqlite", ICON_SQLITE, BLUE},
	{"srt", ICON_SUBTITLES, WHITE},
	{"styl", ICON_STYLUS, B_GREEN},
	{"sub", ICON_SUBTITLES, WHITE},
	{"svelte", ICON_SVELTE, RED},
	{"svg", ICON_IMG, GREEN},
	{"swift", ICON_SWIFT, GREEN},

	{"t", ICON_PERL, YELLOW},
	{"tar", ICON_ARCHIVE, WHITE}, // Uncompressed
	{"tbz2", ICON_ARCHIVE, YELLOW}, // .tar.bz2 (bzip2)
	{"tcl", ICON_TCL, GREEN},
	{"tex", ICON_TEX, WHITE},
	{"tgz", ICON_ARCHIVE, YELLOW}, // .tar.gz (gzip)
	{"tif", ICON_IMG, GREEN},
	{"tiff", ICON_IMG, GREEN},
	{"toml", ICON_TOML, WHITE},
	{"torrent", ICON_DOWNLOADS, WHITE},
	{"ts", ICON_TS, BLUE},
	{"tsx", ICON_REACT, B_BLUE},
	{"ttf", ICON_FONT, BLUE},
	{"twig", ICON_TWIG, GREEN},
	{"txt", ICON_TXT, WHITE},
	{"txz", ICON_ARCHIVE, YELLOW}, // .tar.xz

	{"vdi", ICON_DISK, WHITE},
	{"vhd", ICON_DISK, WHITE},
	{"vhdx", ICON_DISK, WHITE},
	{"vmdk", ICON_DISK, WHITE},
	{"vim", ICON_VIM, GREEN},
	{"vue", ICON_VUE, GREEN},

	{"wav", ICON_AUDIO, B_YELLOW}, // Lossless
	{"webm", ICON_VID, BLUE},
	{"webp", ICON_IMG, GREEN},
	{"wma", ICON_AUDIO, YELLOW},
	{"wmv", ICON_VID, BLUE},

	{"xbps", ICON_PACKAGE, YELLOW}, // Void Linux package
	{"xcf", ICON_GIMP, WHITE},
	{"xhtml", ICON_HTML, YELLOW},
	{"xls", ICON_EXCEL, GREEN},
	{"xlsx", ICON_EXCEL, GREEN},
	{"xml", ICON_XML, YELLOW},
	{"xz", ICON_ARCHIVE, YELLOW},

	{"yaml", ICON_YAML, WHITE},
	{"yml", ICON_YAML, WHITE},

	{"zig", ICON_ZIG, YELLOW},
	{"zip", ICON_ARCHIVE, YELLOW},
	{"zsh", ICON_SHELL, WHITE},
	{"zst", ICON_ARCHIVE, YELLOW},

/* Extra extensions added for the new lookup algorithm. If for some reason we
 * need to go back to the previous algorithm, these extensions should be removed.
 *
 * Do not exceed 768 extensions. Otherwise, ext_table will raise from 1024
 * to 2048, probably making the lookup slower. Even more, keep the list
 * below 650 entries, to avoid overloading the table and reducing performance. */
	{"ai", ICON_ILLUSTRATOR, YELLOW},
	{"dcm", ICON_IMG, GREEN}, // DICOM: Medical images
	{"emf", ICON_IMG, GREEN}, // Windows: wmf replacement
	{"eps", ICON_IMG, GREEN},
	{"exr", ICON_IMG, GREEN},
	{"fts", ICON_IMG, GREEN},
	{"hdr", ICON_IMG, GREEN},
	{"j2c", ICON_IMG, GREEN},
	{"j2k", ICON_IMG, GREEN},
	{"jfi", ICON_IMG, GREEN},
	{"jfif", ICON_IMG, GREEN},
	{"jif", ICON_IMG, GREEN},
	{"jp2", ICON_IMG, GREEN},
	{"jpe", ICON_IMG, GREEN},
	{"jpf", ICON_IMG, GREEN},
	{"jps", ICON_IMG, GREEN},
	{"jpx", ICON_IMG, GREEN},
	{"kpp", ICON_KRITA, MAGENTA},
	{"krz", ICON_KRITA, MAGENTA},
	{"miff", ICON_IMG, GREEN}, // ImageMagick: Uncommon
	{"mng", ICON_IMG, GREEN},
	{"ora", ICON_IMG, GREEN},
	{"pbm", ICON_IMG, GREEN},
	{"pcx", ICON_IMG, GREEN},
	{"pdd", ICON_IMG, GREEN}, // Adobe PhotoDeluxe
	{"pgm", ICON_IMG, GREEN},
	{"pnm", ICON_IMG, GREEN},
	{"ppm", ICON_IMG, GREEN},
	{"psb", ICON_PHOTOSHOP, B_BLUE},
	{"pxm", ICON_IMG, GREEN},
	{"qoi", ICON_IMG, GREEN},
	{"svgz", ICON_IMG, GREEN},
	{"tga", ICON_IMG, GREEN},
	{"wmf", ICON_IMG, GREEN}, // Legacy: Windows
	{"xbm", ICON_IMG, GREEN},
	{"xpm", ICON_IMG, GREEN},
	{"xwd", ICON_IMG, GREEN},
/*	{"cgm", ICON_IMG, GREEN}, // Legacy: vector graphics
	{"dpx", ICON_IMG, GREEN}, // Uncommon: highly specialized
	{"ff", ICON_IMG, GREEN}, // Farbfeld: Never took off
	{"pam", ICON_IMG, GREEN}, // Portable Arbitrary Map: Uncommon
	{"pic", ICON_IMG, GREEN}, // Legacy: generic
	{"pict", ICON_IMG, GREEN}, // Legacy: Apple Macintosh
	{"sgi", ICON_IMG, GREEN}, // Legacy */

	/* Raw image formats */
	{"ari", ICON_IMG, B_GREEN}, // Sony/Arri Alexa
	{"arw", ICON_IMG, B_GREEN}, // Sony
	{"crw", ICON_IMG, B_GREEN}, // Canon: older than cr2/cr3
	{"cr2", ICON_IMG, B_GREEN}, // Canon
	{"cr3", ICON_IMG, B_GREEN}, // Canon
	{"dcs", ICON_IMG, GREEN}, // Kodak
	{"dcr", ICON_IMG, GREEN}, // Kodak
	{"dng", ICON_IMG, B_GREEN}, // Adobe Digital Negative (several brands)
	{"drf", ICON_IMG, GREEN}, // Kodak
	{"erf", ICON_IMG, GREEN}, // Epson
	{"iiq", ICON_IMG, B_GREEN}, // Phase One/Capture One
	{"k25", ICON_IMG, GREEN}, // Kodac
	{"kdc", ICON_IMG, GREEN}, // Kodak
	{"mef", ICON_IMG, GREEN}, // Mamiya
	{"nef", ICON_IMG, B_GREEN}, // Nikon
	{"nrw", ICON_IMG, B_GREEN}, // Nikon
	{"orf", ICON_IMG, B_GREEN}, // Olympus
	{"pef", ICON_IMG, B_GREEN}, // Pentax
	{"r3d", ICON_IMG, B_GREEN}, // Fujifilm
	{"raf", ICON_IMG, B_GREEN}, // Fujifilm
	{"rw2", ICON_IMG, B_GREEN}, // Panasonic
	{"rwl", ICON_IMG, B_GREEN}, // Leica
	{"rwz", ICON_IMG, B_GREEN}, // Panasonic
	{"sr2", ICON_IMG, B_GREEN}, // Sony
	{"srf", ICON_IMG, B_GREEN}, // Sony
/*	{"3fr", ICON_IMG, GREEN}, // Hasselblad
	{"bay", ICON_IMG, GREEN}, // Casio
	{"braw", ICON_IMG, GREEN}, // BlackMagic
	{"cap", ICON_IMG, GREEN},
	{"cin", ICON_IMG, GREEN}, // Kodak
	{"crf", ICON_IMG, GREEN}, // Casio
	{"data", ICON_IMG, GREEN},
	{"eip", ICON_IMG, GREEN}, // Epson
	{"fff", ICON_IMG, GREEN}, // Hasselblad
	{"gpr", ICON_IMG, GREEN}, // GoPro
	{"mdc", ICON_IMG, GREEN}, // Minolta/Agfa
	{"mos", ICON_IMG, GREEN}, // Leaf/Mamiya
	{"mrw", ICON_IMG, GREEN}, // Minolta
	{"obm", ICON_IMG, GREEN}, // Olympus
	{"ptx", ICON_IMG, GREEN}, // Pentax
	{"pxn", ICON_IMG, GREEN}, // Logitech
	{"raw", ICON_IMG, GREEN}, // Generic (may be audio, disk image, etc)
	{"srw", ICON_IMG, GREEN}, // Samsumg
	{"x3f", ICON_IMG, GREEN}, // Sigma */

	{"3g2", ICON_VID, BLUE},
	{"3gp2", ICON_VID, BLUE},
	{"3gpp", ICON_VID, BLUE},
	{"3gpp2", ICON_VID, BLUE},
	{"asf", ICON_VID, BLUE},
	{"divx", ICON_VID, BLUE}, // Legacy: common before mp4/mkv
	{"h264", ICON_VID, BLUE},
	{"heics", ICON_VID, BLUE},
	{"m2ts", ICON_VID, BLUE},
	{"m2v", ICON_VID, BLUE},
	{"mjpeg", ICON_VID, BLUE},
	{"mjpg", ICON_VID, BLUE},
	{"mp4v", ICON_VID, BLUE}, // Legacy: very common before H.264 dominance
	{"qt", ICON_VID, BLUE},
	{"rm", ICON_VID, BLUE},
	{"video", ICON_VID, BLUE},
	{"vob", ICON_VID, BLUE},
	{"ogm", ICON_VID, BLUE},
	{"swf", ICON_VID, BLUE}, // Legacy: typical Flash container
/*	{"f4v, ICON_VID, BLUE"} // Legacy: Flash video (quite uncommon)
	{"dv", ICON_VID, BLUE}, // Legacy (digital cameras 1990s-2000s)
	{"rmvb", ICON_VID, BLUE}, // Legacy: RealMedia Variable Bit Rate
	{"flc", ICON_VID, BLUE}, // Legacy: Autodesk
	{"fli", ICON_VID, BLUE}, // Legacy: Autodesk
	{"gl", ICON_VID, BLUE}, // Legacy: GRASP animation (MS-DOS)
	{"nuv", ICON_VID, BLUE},
	{"y4m", ICON_VID, BLUE}, // YUV4MPEG2: Uncompressed, intermediate processing
	{"yuv", ICON_VID, BLUE}, // Uncompressed: may be both video or image */

	{"ac3", ICON_AUDIO, YELLOW},
	{"aif", ICON_AUDIO, B_YELLOW}, // Lossless
	{"aifc", ICON_AUDIO, B_YELLOW}, // Lossless
	{"ape", ICON_AUDIO, B_YELLOW}, // Lossless
	{"au", ICON_AUDIO, YELLOW}, // Audacity Audio File
	{"aup", ICON_AUDIO, YELLOW}, // Audacity Project File
	{"aup3", ICON_AUDIO, YELLOW}, // Audacity 3 Project File
	{"m2a", ICON_AUDIO, YELLOW}, // Legacy
	{"m4b", ICON_AUDIO, YELLOW}, // MPEG-4 Audiobook
	{"mid", ICON_AUDIO, YELLOW},
	{"midi", ICON_AUDIO, YELLOW},
	{"mka", ICON_AUDIO, YELLOW},
	{"mp2", ICON_AUDIO, YELLOW},
	{"mpga", ICON_AUDIO, YELLOW}, // Legacy, but widely used back then
	{"oga", ICON_AUDIO, YELLOW}, // OGG Vorbis audio
	{"pcm", ICON_AUDIO, B_YELLOW}, // Lossless
	{"ra", ICON_AUDIO, YELLOW}, // RealAudio File
	{"sf2", ICON_AUDIO, YELLOW},
	{"spx", ICON_AUDIO, YELLOW},
	{"tta", ICON_AUDIO, B_YELLOW}, // True Audio File (lossless)
	{"wv", ICON_AUDIO, B_YELLOW}, // Lossless
/*	{"axa", ICON_AUDIO, YELLOW}, // Legacy: Annodex Audio File
	{"mpc", ICON_AUDIO, YELLOW}, // Legacy/Niche: Musepack Compressed Audio
	{"snd", ICON_AUDIO, YELLOW}, // Generic
	{"sou", ICON_AUDIO, YELLOW}, // Legacy
	{"voc", ICON_AUDIO, YELLOW}, // Legacy */

	{"ace", ICON_ARCHIVE, YELLOW}, // Legacy
	{"alz", ICON_ARCHIVE, YELLOW}, // ALZip file
	{"ar", ICON_ARCHIVE, WHITE}, // Uncompressed
	{"arc", ICON_ARCHIVE, YELLOW}, // Legacy
	{"br", ICON_ARCHIVE, YELLOW},
	{"bz", ICON_ARCHIVE, YELLOW}, // bzip
	{"ear", ICON_ARCHIVE, YELLOW}, // Java enterprise archive
	{"lbr", ICON_ARCHIVE, WHITE}, // Uncompressed
	{"lrz", ICON_ARCHIVE, YELLOW}, // lrzip
	{"lzo", ICON_ARCHIVE, YELLOW}, // lzop
	{"pak", ICON_ARCHIVE, YELLOW},
	{"phar", ICON_ARCHIVE, YELLOW}, /* PHP archive */
	{"pk3", ICON_ARCHIVE, YELLOW}, // ZIP-based
	{"par", ICON_ARCHIVE, YELLOW},
	{"pyz", ICON_ARCHIVE, YELLOW}, // Python Application Zip File
	{"rz", ICON_ARCHIVE, YELLOW}, // rzip
	{"t7z", ICON_ARCHIVE, YELLOW}, // .tar.7z
	{"taz", ICON_ARCHIVE, YELLOW}, // .tar.Z (also .tz)
	{"tbz", ICON_ARCHIVE, YELLOW}, // .tar.bz (bzip)
	{"tlz", ICON_ARCHIVE, YELLOW}, // .tar.lz (lzip)
	{"tz", ICON_ARCHIVE, YELLOW}, // .tar.Z (compress)
	{"tzo", ICON_ARCHIVE, YELLOW}, // .tar.lzo (lzop)
	{"tzst", ICON_ARCHIVE, YELLOW}, // .tar.zst
	{"war", ICON_ARCHIVE, YELLOW}, /* Java web archive */
	{"z", ICON_ARCHIVE, YELLOW}, // Compress
	{"zoo", ICON_ARCHIVE, YELLOW}, // Legacy
	{"zpaq", ICON_ARCHIVE, YELLOW},
/*	{"drpm", ICON_ARCHIVE, YELLOW}, // Delta RPM archive
	{"dz", ICON_ARCHIVE, YELLOW}, // Dzip file
	{"egg", ICON_ARCHIVE, YELLOW}, // ALZip file (commonly .alz)
	{"lha", ICON_ARCHIVE, YELLOW}, // Legacy: Amiga (like .lzh)
	{"lzx", ICON_ARCHIVE, YELLOW}, // Legacy: Amiga
	{"pax", ICON_ARCHIVE, WHITE}, // Uncompressed
	{"pea", ICON_ARCHIVE, YELLOW},
	{"pk7", ICON_ARCHIVE, YELLOW}, // GZDoom archive file
	{"shar", ICON_ARCHIVE, WHITE}, // Uncompressed
	{"warc", ICON_ARCHIVE, WHITE}, // Uncompressed: Web archive */

	{"apk", ICON_PACKAGE, YELLOW},
	{"Appimage", ICON_PACKAGE, YELLOW},
	{"crx", ICON_PACKAGE, YELLOW}, // Chrome add-ons
	{"flatpak", ICON_PACKAGE, YELLOW},
	{"msi", ICON_PACKAGE, YELLOW}, // Windows installer
	{"pkg", ICON_PACKAGE, YELLOW},
	{"snap", ICON_PACKAGE, YELLOW},
	{"xpi", ICON_PACKAGE, YELLOW}, // Mozilla add-ons
/*	{"udeb", ICON_PACKAGE, YELLOW}, // Debian micro package
	{"whl", ICON_PACKAGE, YELLOW}, // Python Wheel package
	{"appx", ICON_PACKAGE, YELLOW} // Windows 8 app installer
	{"appxbundle", ICON_PACKAGE, YELLOW} // Windows 8 app installer
	{"msix", ICON_PACKAGE, YELLOW} // Windows 10 app installer
	{"msixbundle", ICON_PACKAGE, YELLOW} // Windows 10 app installer */

	{"aff", ICON_DISK, WHITE}, // Advanced Forensics Format disk image
	{"image", ICON_DISK, WHITE},
/*	{"adf", ICON_DISK, WHITE}, // Amiga disk file (also adz, hdf, hdz)
	{"ima", ICON_DISK, WHITE},
	{"wim", ICON_DISK, WHITE}, // Windows Imaging Format File */

	{"cdi", ICON_CD, WHITE},
	{"isz", ICON_CD, WHITE},
	{"nrg", ICON_CD, WHITE},
	{"tc", ICON_CD, WHITE},
	{"toast", ICON_CD, WHITE},
	{"vcd", ICON_CD, WHITE},

	{"bdf", ICON_FONT, BLUE}, // X11 bitmap font
	{"eot", ICON_FONT, BLUE},
	{"flf", ICON_FONT, BLUE}, // Plain‑text FIGlet font
	{"fnt", ICON_FONT, BLUE}, // Legacy: DOS/Windows bitmap font
	{"fon", ICON_FONT, BLUE}, // Legacy: DOS/Windows bitmap font
	{"font", ICON_FONT, BLUE}, // Generic
	{"lff", ICON_FONT, BLUE}, // Legacy
	{"psf", ICON_FONT, BLUE}, // Unix console font
	{"ttc", ICON_FONT, BLUE}, // TrueType Font Collection
	{"woff", ICON_FONT, BLUE}, // Web font
	{"woff2", ICON_FONT, BLUE}, // Web font

	{"ass", ICON_SUBTITLES, WHITE},
	{"lrc", ICON_SUBTITLES, WHITE},
	{"sbt", ICON_SUBTITLES, WHITE},
	{"ssa", ICON_SUBTITLES, WHITE},

	{"avro", ICON_JSON, WHITE},
	{"json5", ICON_JSON, WHITE},
	{"jsonc", ICON_JSON, WHITE},
	{"webmanifest", ICON_JSON, WHITE},

	/* Programming */
	{"asp", ICON_HTML, YELLOW},
	{"aspx", ICON_HTML, YELLOW},
	{"bst", ICON_TEX, WHITE},
	{"cp", ICON_CPP, B_BLUE},
	{"csproj", ICON_CSHARP, MAGENTA},
	{"cu", ICON_CUDA, BLUE},
	{"dot", ICON_GRAPHVIZ, WHITE},
	{"elc", ICON_EMACS, BLUE},
	{"fsproj", ICON_FSHARP, CYAN},
	{"gd", ICON_GODOT, CYAN},
	{"godot", ICON_GODOT, CYAN},
	{"gql", ICON_GRAPHQL, MAGENTA},
	{"gradle", ICON_GRADLE, WHITE},
	{"graphql", ICON_GRAPHQL, MAGENTA},
	{"groovy", ICON_GROOVY, CYAN},
	{"gv", ICON_GRAPHVIZ, WHITE},
	{"gvy", ICON_GROOVY, CYAN},
	{"h++", ICON_CPP, B_BLUE},
	{"inl", ICON_CPP, B_BLUE},
	{"jad", ICON_JAVA, WHITE},
	{"jsp", ICON_HTML, YELLOW},
	{"jspx", ICON_HTML, YELLOW},
	{"latex", ICON_TEX, WHITE},
	{"ltx", ICON_TEX, WHITE},
	{"luac", ICON_LUA, BLUE},
	{"luau", ICON_LUA, BLUE},
	{"mll", ICON_OCAML, WHITE},
	{"mly", ICON_OCAML, WHITE},
	{"node", ICON_NODEJS, GREEN},
	{"opml", ICON_XML, YELLOW},
	{"ps1", ICON_POWERSHELL, WHITE},
	{"psd1", ICON_POWERSHELL, WHITE},
	{"psm1", ICON_POWERSHELL, WHITE},
	{"purs", ICON_PURESCRIPT, WHITE},
	{"pxd", ICON_PYTHON, GREEN},
	{"pyi", ICON_PYTHON, GREEN},
	{"pyw", ICON_PYTHON, GREEN},
	{"rdf", ICON_XML, YELLOW},
	{"rds", ICON_R, BLUE},
	{"rkt", ICON_SCHEME, WHITE},
	{"rmd", ICON_RUST, WHITE},
	{"rmeta", ICON_RUST, WHITE},
	{"scm", ICON_SCHEME, WHITE},
	{"sgml", ICON_XML, YELLOW},
	{"shtml", ICON_HTML, YELLOW},
	{"sld", ICON_SCHEME, WHITE},
	{"ss", ICON_SCHEME, WHITE},
	{"sty", ICON_TEX, WHITE},
	{"stylus", ICON_STYLUS, B_GREEN},
	{"suo", ICON_VISUALSTUDIO, BLUE},
	{"tbc", ICON_TCL, GREEN},
	{"tld", ICON_XML, YELLOW},
	{"tres", ICON_GODOT, CYAN},
	{"tscn", ICON_GODOT, CYAN},
	{"v", ICON_V, B_BLUE},
	{"vala", ICON_VALA, MAGENTA},
	{"vsix", ICON_VISUALSTUDIO, BLUE},
	{"xaml", ICON_XAML, WHITE},
	{"xbel", ICON_XML, YELLOW},
	{"xmp", ICON_XML, YELLOW},
	{"xsd", ICON_XML, YELLOW},
	{"xsl", ICON_XML, YELLOW},
	{"xslt", ICON_XML, YELLOW},
	{"xul", ICON_XML, YELLOW},
	{"whl", ICON_PYTHON, GREEN},
/*	{"iml", ICON_INTELLIJ, BLUE or RED},
	{"rdf", ICON_REDIS, RED},
	{"aof", ICON_REDIS, RED},
	{"cr", ICON_CRYSTAL, WHITE}, nf-dev-crystal
	{"fnl", ICON_FENNEL, GREEN}, // No IIT icon
	{"hack", ICON_HACK, YELLOW}, nf-seti-hacklang
	{"qml", ICON_QT, GREEN}, // No IIT icon
	{"qrc", ICON_QT, GREEN},
	{"qss", ICON_QT, GREEN},
	{"raku", ICON_?, ?} // Icon: a colorful butterfly
	{"sv", ICON_LANG_HDL, ?},
	{"svh", ICON_LANG_HDL, ?},
	{"vhdl", ICON_LANG_HDL, ?},
	{"typ", ICON_TYPST, GREEN}, // No IIT icon
	{"unity, ICON_UNITY, WHITE"},
	{"unity3d, ICON_UNITY, WHITE"},
//	applescript, scptd, scpt, ICON_APPLE, WHITE */

	/* Office */
	{"csv", ICON_EXCEL, GREEN},
	{"dif", ICON_EXCEL, GREEN},
/*	{"dot", ICON_WORD, BLUE} // Old: conflicts with Graphviz files */
	{"docm", ICON_WORD, BLUE},
	{"dotm", ICON_WORD, BLUE},
	{"dotx", ICON_WORD, BLUE},
	{"dps", ICON_POWERPOINT, YELLOW}, /* Kingsoft WPS Office */
	{"dpt", ICON_POWERPOINT, YELLOW}, /* Kingsoft WPS Office */
	{"et", ICON_EXCEL, GREEN}, /* Kingsoft WPS Office */
	{"ett", ICON_EXCEL, GREEN}, /* Kingsoft WPS Office */
	{"fodg", ICON_LIBREOFFICE_DRAW, MAGENTA},
	{"fodp", ICON_LIBREOFFICE_IMPRESS, YELLOW},
	{"fods", ICON_LIBREOFFICE_CALC, GREEN},
	{"fodt", ICON_LIBREOFFICE_WRITER, BLUE},
	{"gdoc", ICON_WORD, BLUE},
	{"gdocx", ICON_WORD, BLUE},
	{"gsheet", ICON_EXCEL, GREEN},
	{"gslides", ICON_POWERPOINT, YELLOW},
/*	{"key", ICON_POWERPOINT, YELLOW}, // Apple. Conflicts with .key security files */
	{"numbers", ICON_EXCEL, GREEN}, /* Apple */
	{"odf", ICON_LIBREOFFICE_MATH, WHITE},
	{"otg", ICON_LIBREOFFICE_DRAW, MAGENTA},
	{"otp", ICON_LIBREOFFICE_IMPRESS, YELLOW},
	{"ots", ICON_LIBREOFFICE_CALC, GREEN},
	{"ott", ICON_LIBREOFFICE_WRITER, BLUE},
	{"pages", ICON_WORD, BLUE}, /* Apple */
/*	{"pot", ICON_POWERPOINT, YELLOW}, // Conflicts with .pot translation files */
	{"potm", ICON_POWERPOINT, YELLOW},
	{"potx", ICON_POWERPOINT, YELLOW},
	{"pps", ICON_POWERPOINT, YELLOW},
	{"ppsx", ICON_POWERPOINT, YELLOW},
	{"pptm", ICON_POWERPOINT, YELLOW},
	{"slk", ICON_EXCEL, GREEN},
	{"tsv", ICON_EXCEL, GREEN},
	{"uop", ICON_LIBREOFFICE_IMPRESS, YELLOW},
	{"uos", ICON_LIBREOFFICE_CALC, GREEN},
	{"uot", ICON_LIBREOFFICE_WRITER, BLUE},
	{"wps", ICON_WORD, BLUE}, /* Kingsoft WPS Office */
	{"wpt", ICON_WORD, BLUE}, /* Kingsoft WPS Office */
	{"xlr", ICON_EXCEL, GREEN},
	{"xlsb", ICON_EXCEL, GREEN},
	{"xlsm", ICON_EXCEL, GREEN},
	{"xlt", ICON_EXCEL, GREEN},
	{"xltm", ICON_EXCEL, GREEN},
	{"xltx", ICON_EXCEL, GREEN},

/*	{"sublime-build", ICON_SUBLIME, YELLOW},
	{"sublime-keymap", ICON_SUBLIME, YELLOW},
	{"sublime-menu", ICON_SUBLIME, YELLOW},
	{"sublime-options", ICON_SUBLIME, YELLOW},
	{"sublime-package", ICON_SUBLIME, YELLOW},
	{"sublime-project", ICON_SUBLIME, YELLOW},
	{"sublime-session", ICON_SUBLIME, YELLOW},
	{"sublime-settings", ICON_SUBLIME, YELLOW},
	{"sublime-snippet", ICON_SUBLIME, YELLOW},
	{"sublime-theme", ICON_SUBLIME, YELLOW},

	{"kicad_dru", ICON_KICAD, BLUE},
	{"kicad_mod", ICON_KICAD, BLUE},
	{"kicad_pcb", ICON_KICAD, BLUE},
	{"kicad_prl", ICON_KICAD, BLUE},
	{"kicad_pro", ICON_KICAD, BLUE},
	{"kicad_sch", ICON_KICAD, BLUE},
	{"kicad_sym", ICON_KICAD, BLUE},
	{"kicad_wks", ICON_KICAD, BLUE}, */

	{"ical", ICON_CALENDAR, WHITE},
	{"icalendar", ICON_CALENDAR, WHITE}, // Rare
	{"ics", ICON_CALENDAR, WHITE},
	{"ifb", ICON_CALENDAR, WHITE}, // Niche

	{"md5", ICON_CHECKSUM, GREEN},
	{"sha1", ICON_CHECKSUM, GREEN},
	{"sha224", ICON_CHECKSUM, GREEN},
	{"sha256", ICON_CHECKSUM, GREEN},
	{"sha384", ICON_CHECKSUM, GREEN},
	{"sha512", ICON_CHECKSUM, GREEN},

	{"rom", ICON_ROM, WHITE},
/*	{"3ds", ICON_ROM, WHITE},
	{"32x", ICON_ROM, WHITE},
	{"cci", ICON_ROM, WHITE},
	{"chd", ICON_ROM, WHITE},
	{"cia", ICON_ROM, WHITE},
	{"dsk", ICON_ROM, WHITE},
	{"fds", ICON_ROM, WHITE},
	{"gb", ICON_ROM, WHITE},
	{"gba", ICON_ROM, WHITE},
	{"gbc", ICON_ROM, WHITE},
	{"gg", ICON_ROM, WHITE},
	{"n64", ICON_ROM, WHITE},
	{"nes", ICON_ROM, WHITE},
	{"nro", ICON_ROM, WHITE},
	{"nsf", ICON_ROM, WHITE},
	{"nsp", ICON_ROM, WHITE},
	{"pce", ICON_ROM, WHITE},
	{"sap", ICON_ROM, WHITE},
	{"sfc", ICON_ROM, WHITE},
	{"smc", ICON_ROM, WHITE},
	{"smd", ICON_ROM, WHITE},
	{"sms", ICON_ROM, WHITE},
	{"v64", ICON_ROM, WHITE},
	{"xci", ICON_ROM, WHITE},
	{"xex", ICON_ROM, WHITE},
	{"z64", ICON_ROM, WHITE},
//	{"md", ICON_ROM, WHITE}, // Conflicts with markdown
//	{"wad", ICON_ROM, WHITE}, // If Doom, this isn't a rom file */

	{"jmd", ICON_MARKDOWN, WHITE},
	{"mdown", ICON_MARKDOWN, WHITE},
	{"mdx", ICON_MARKDOWN, WHITE},
	{"mkd", ICON_MARKDOWN, WHITE},
	{"rdoc", ICON_MARKDOWN, WHITE},

	{"download", ICON_DOWNLOADS, WHITE},
	{"fdmdownload", ICON_DOWNLOADS, WHITE},
	{"opdownload", ICON_DOWNLOADS, WHITE},

	{"m3u", ICON_PLAYLIST, YELLOW},
	{"m3u8", ICON_PLAYLIST, YELLOW},
	{"pls", ICON_PLAYLIST, YELLOW},
//	{"xspf", ICON_PLAYLIST, YELLOW},

	{"s3db", ICON_SQLITE, BLUE},
	{"sqlite3", ICON_SQLITE, BLUE},
	{"sl3", ICON_SQLITE, BLUE},

	{"dbf", ICON_DATABASE, WHITE},
	{"dconf", ICON_DATABASE, WHITE},
	{"dump", ICON_DATABASE, WHITE},
	{"fdb", ICON_DATABASE, WHITE},
	{"gdb", ICON_DATABASE, WHITE},
	{"kdb", ICON_DATABASE, WHITE},
	{"kdbx", ICON_DATABASE, WHITE},
	{"mdf", ICON_DATABASE, WHITE},
	{"ndf", ICON_DATABASE, WHITE},
	{"sdf", ICON_DATABASE, WHITE},

	{"age", ICON_LOCK, GREEN},
	{"lck", ICON_LOCK, WHITE},
	{"kbx", ICON_KEY, RED},
	{"p12", ICON_KEY, YELLOW},
	{"pfx", ICON_KEY, YELLOW},

	{"azw4", ICON_BOOK, WHITE},
	{"cb7", ICON_BOOK, YELLOW},
	{"cba", ICON_BOOK, YELLOW},
	{"djv", ICON_BOOK, WHITE},
	{"fb2", ICON_BOOK, WHITE},
	{"kf8", ICON_BOOK, WHITE},
	{"mobi", ICON_BOOK, WHITE},
	{"oxps", ICON_BOOK, WHITE}, // Open XML Paper Specification
	{"xps", ICON_BOOK, WHITE}, // XML Paper Specification (Microsoft)
/*	{"cbt", ICON_BOOK, YELLOW}, // tar'ed comic book
	{"iba", ICON_BOOK, YELLOW} // Legacy: Apple iBooks Author
	{"ibooks", ICON_BOOK, YELLOW} // Legacy: Apple iBooks Author
	{"lrf", ICON_BOOK, YELLOW} // Legacy: Sony BBeB
	{"opf", ICON_BOOK, YELLOW}, // Legacy: Open eBook
	{"pdb", ICON_BOOK, YELLOW}, // Legacy: PalmDoc
	{"prc", ICON_BOOK, YELLOW}, // Legacy: PalmDoc
	{"tcr", ICON_BOOK, YELLOW}, // Legacy: Psion Series 3 palmtop */

	{"dylib", ICON_SHARE, BLUE},
	{"lib", ICON_SHARE, BLUE},

	{"cache", ICON_CACHE, WHITE},
	{"magnet", ICON_MAGNET, RED},
	{"rst", ICON_TXT, WHITE},
	{"chm", ICON_MANPAGE, WHITE},
	{"com", ICON_EXEC, WHITE},
	{"sig", ICON_SIG, GREEN},
	{"tml", ICON_CONFIG, WHITE},

/* Legacy
 * Amiga: nfo, diz, mod, pic, lbm, ilbm, dib, rle, adf, dms
 * MS-DOS: voc, sou, bas, sys, vxd, drv, hlp, ffn, qfs (archive),
 * tgv (video), mdi (image), pif (info, binary) */

/*	{"tf", ICON_TERRAFORM, MAGENTA},
	{"tfstate", ICON_TERRAFORM, MAGENTA},
	{"tfvars", ICON_TERRAFORM, MAGENTA}, */

/* SoftMaker Office
	{"tmd", ICON_WORD, BLUE},
	{"tmdx", ICON_WORD, BLUE},
	{"tmv", ICON_WORD, BLUE},
	{"tmvx", ICON_WORD, BLUE},
	{"pmd", ICON_EXCEL, GREEN},
	{"pmdx", ICON_EXCEL, GREEN},
	{"pmv", ICON_EXCEL, GREEN},
	{"pmvx", ICON_EXCEL, GREEN},
	{"prd", ICON_POWERPOINT, YELLOW},
	{"prdx", ICON_POWERPOINT, YELLOW},
	{"prv", ICON_POWERPOINT, YELLOW},
	{"prvx", ICON_POWERPOINT, YELLOW}, */

/* StarOffice (legacy)
	{"sxw", ICON_WORD, BLUE},
	{"sxc", ICON_EXCEL, GREEN},
	{"sxi", ICON_POWERPOINT, YELLOW},
	{"sxd", ICON_LIBREOFFICE_DRAW, MAGENTA},
	{"sdb", ICON_DATABASE, WHITE}, */

/*	{"one", ICON_ONENOTE, MAGENTA}, */
/*	{"blend", ICON_BLENDER, YELLOW}, */
/*	{"eml", ICON_MAIL, WHITE}, */

/* Calligra Plan/Gnome Planner
	{"plan", ICON_PLAN, WHITE} // icon Nerd: chart_gantt, IIT: chart */

/*	{"sch", ICON_EDA_SCH, ??}, // Nerd: resistor
	{"schdoc", ICON_EDA_SCH, ??},
	{"brd", ICON_EDA_PCB, ??}, // Nerd: circuit, IIT: circuit_board
	{"gbl", ICON_EDA_PCB, ??},
	{"gbo", ICON_EDA_PCB, ??},
	{"gbp", ICON_EDA_PCB, ??},
	{"gbr", ICON_EDA_PCB, ??},
	{"gbs", ICON_EDA_PCB, ??},
	{"gm1", ICON_EDA_PCB, ??},
	{"gml", ICON_EDA_PCB, ??},
	{"gtl", ICON_EDA_PCB, ??},
	{"gto", ICON_EDA_PCB, ??},
	{"gtp", ICON_EDA_PCB, ??},
	{"gts", ICON_EDA_PCB, ??},
	{"lpp", ICON_EDA_PCB, ??},
	{"pcbdoc", ICON_EDA_PCB, ??},
	{"prjpcb", ICON_EDA_PCB, ??},

	{"123dx", ICON_CAD, ?},    Nerd: md_file_cad
	{"3dm", ICON_CAD, ?},
	{"brep", ICON_CAD, ?},
	{"catpart", ICON_CAD, ?},
	{"catproduct", ICON_CAD, ?},
	{"dwg", ICON_CAD, ?},
	{"dxf", ICON_CAD, ?},
	{"f3d", ICON_CAD, ?},
	{"f3z", ICON_CAD, ?},
	{"iam", ICON_CAD, ?},
	{"ifc", ICON_CAD, ?},
	{"ige", ICON_CAD, ?},
	{"iges", ICON_CAD, ?},
	{"igs", ICON_CAD, ?},
	{"ipt", ICON_CAD, ?},
	{"psm", ICON_CAD, ?},
	{"skp", ICON_CAD, ?},
	{"sldasm", ICON_CAD, ?},
	{"sldprt", ICON_CAD, ?},
	{"slvs", ICON_CAD, ?},
	{"ste", ICON_CAD, ?},
	{"step", ICON_CAD, ?},
	{"stp", ICON_CAD, ?},
	{"x_b", ICON_CAD, ?},
	{"x_t", ICON_CAD, ?},
	{"fcbak", ICON_FREECAD, RED}, Nerd: nf_linux_freecad
	{"fcmacro", ICON_FREECAD, RED},
	{"fcmat", ICON_FREECAD, RED},
	{"fcparam", ICON_FREECAD, RED},
	{"fcscript", ICON_FREECAD, RED},
	{"fcstd", ICON_FREECAD, RED},
	{"fcstd1", ICON_FREECAD, RED},
	{"fctb", ICON_FREECAD, RED},
	{"fctl", ICON_FREECAD, RED},
	{"scad", ICON_OPENSCAD, YELLOW}, // icon scad

	{"3mf", ICON_FILE3D, ?}, // icon cube
	{"fbx", ICON_FILE3D, ?},
	{"obj", ICON_FILE3D, ?},
	{"ply", ICON_FILE3D, ?},
	{"stl", ICON_FILE3D, ?},
	{"wrl", ICON_FILE3D, ?},
	{"wrz", ICON_FILE3D, ?}, */
};

/* Icons for some specific dir names */
struct icons_t const icon_dirnames[] = {
/* Translated version for these dirs should be added here
 * See https://github.com/alexanderjeurissen/ranger_devicons/blob/main/devicons.py*/
	{"home", ICON_HOME, DEF_DIR_ICON_COLOR},
/*    {".bundle", ICON_RUBY, DEF_DIR_ICON_COLOR},
 *    {".cargo", ICON_RUST, DEF_DIR_ICON_COLOR}, */
	{".config", ICON_CONFIG_DIR, DEF_DIR_ICON_COLOR},
	{".git", ICON_GIT, DEF_DIR_ICON_COLOR},
	{"Desktop", ICON_DESKTOP, DEF_DIR_ICON_COLOR},
	{"Documents", ICON_DOCUMENTS, DEF_DIR_ICON_COLOR},
	{"Downloads", ICON_DOWNLOADS, DEF_DIR_ICON_COLOR},
	{"Dropbox", ICON_DROPBOX, DEF_DIR_ICON_COLOR},
	{"etc", ICON_CONFIG_DIR, DEF_DIR_ICON_COLOR},
	{"games", ICON_GAMES, DEF_DIR_ICON_COLOR},
	{"Music", ICON_MUSIC, DEF_DIR_ICON_COLOR},
	{"OneDrive", ICON_ONEDRIVE, DEF_DIR_ICON_COLOR},
	{"Pictures", ICON_PICTURES, DEF_DIR_ICON_COLOR},
	{"Public", ICON_PUBLIC, DEF_DIR_ICON_COLOR},
	{"Steam", ICON_STEAM, DEF_DIR_ICON_COLOR},
	{"Templates", ICON_TEMPLATES, DEF_DIR_ICON_COLOR},
	{"Trash", ICON_TRASH, DEF_DIR_ICON_COLOR},
	{"Videos", ICON_VIDEOS, DEF_DIR_ICON_COLOR}
};

/* Icons for some specific filenames */
struct icons_t const icon_filenames[] = {
/* More specific filenames from here https://github.com/alexanderjeurissen/ranger_devicons/blob/main/devicons.py */
	{".bash_aliases", ICON_CONFIG, WHITE},
	{".bash_history", ICON_CONFIG, WHITE},
	{".bash_logout", ICON_CONFIG, WHITE},
	{".bash_profile", ICON_CONFIG, WHITE},
	{".bashrc", ICON_CONFIG, WHITE},
	{".gitattributes", ICON_GIT, WHITE},
	{".gitconfig", ICON_GIT, WHITE},
	{".gitignore", ICON_GIT, WHITE},
	{".gitmodules", ICON_GIT, WHITE},
	{".htaccess", ICON_CONFIG, WHITE},
	{".htpasswd", ICON_CONFIG, WHITE},
	{".inputrc", ICON_CONFIG, WHITE},
	{".mailmap", ICON_GIT, WHITE},
	{".mime.types", ICON_CONFIG, WHITE},
	{".vimrc", ICON_CONFIG, WHITE},
	{".Xdefaults", ICON_CONFIG, WHITE},
	{".xinitrc", ICON_CONFIG, WHITE},
	{".Xresources", ICON_CONFIG, WHITE},
	{".zshrc", ICON_CONFIG, WHITE},
	{"CHANGELOG", ICON_HISTORY, DEF_FILE_ICON_COLOR},
	{"clifmrc", ICON_CONFIG, WHITE},
	{"CMakeLists.txt", ICON_CMAKE, WHITE},
	{"configure", ICON_CONFIG, DEF_FILE_ICON_COLOR},
	{"configure.ac", ICON_CONFIG, DEF_FILE_ICON_COLOR},
	{"configure.in", ICON_CONFIG, DEF_FILE_ICON_COLOR},
	{"Dockerfile", ICON_DOCKER, B_BLUE},
	{"Gemfile", ICON_RUBY, RED},
	{"go.mod", ICON_GO, B_BLUE},
	{"go.sum", ICON_GO, B_BLUE},
	{"Jenkinsfile", ICON_JENKINS, WHITE},
	{"LICENSE", ICON_COPYRIGHT, YELLOW},
	{"Makefile", ICON_MAKEFILE, B_BLUE},
	{"Makefile.ac", ICON_MAKEFILE, B_BLUE},
	{"Makefile.am", ICON_MAKEFILE, B_BLUE},
	{"Makefile.in", ICON_MAKEFILE, B_BLUE},
	{"mimeapps.list", ICON_CONFIG, WHITE},
	{"PKGBUILD", ICON_CONFIG, WHITE},
	{"Procfile", ICON_HEROKU, MAGENTA},
	{"Rakefile", ICON_RUBY, RED},
	{"README", ICON_README, CYAN},
	{"TODO", ICON_LIST, GREEN},
	{"Vagrantfile", ICON_VAGRANT, CYAN}
};

#endif /* ICONS_H */
