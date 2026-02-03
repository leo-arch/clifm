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
	ICON_3D_FILE[] = EMOJI_3D_FILE,
	ICON_ACCESS[] = EMOJI_DATABASE,
	ICON_ARCHIVE[] = EMOJI_ARCHIVE,
	ICON_ARDUINO[] = EMOJI_ARDUINO,
	ICON_ASM[] = EMOJI_ASM,
	ICON_AUDIO[] = EMOJI_AUDIO,
	ICON_BINARY[] = EMOJI_BINARY,
	ICON_BLENDER[] = EMOJI_BLENDER,
	ICON_BOOK[] = EMOJI_BOOK_OPEN,
	ICON_C[] = EMOJI_C,
	ICON_CACHE[] = EMOJI_CACHE,
	ICON_CAD[] = EMOJI_CAD,
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
	ICON_ELIXIR[] = EMOJI_ELIXIR,
	ICON_ELM[] = EMOJI_ELM,
	ICON_EMACS[] = EMOJI_SHELL,
	ICON_ERLANG[] = EMOJI_ERLANG,
	ICON_EXCEL[] = EMOJI_STYLESHEET,
	ICON_FONT[] = EMOJI_FONT,
	ICON_FORTRAN[] = EMOJI_FORTRAN,
	ICON_FREECAD[] = EMOJI_FREECAD,
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
	ICON_HELP[] = EMOJI_MANUAL,
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
	ICON_SCAD[] = EMOJI_SCAD,
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
	ICON_3D_FILE[] = FA_CUBE,
	ICON_ACCESS[] = FILE_ACCESS,
	ICON_ARCHIVE[] = FA_ARCHIVE,
	ICON_ARDUINO[] = FILE_ARDUINO,
	ICON_ASM[] = FILE_NASM,
	ICON_AUDIO[] = FA_FILE_AUDIO_O,
	ICON_BINARY[] = OCT_FILE_BINARY,
	ICON_BLENDER[] = FILE_BLENDER,
	ICON_BOOK[] = OCT_BOOK,
	ICON_C[] = MFIZZ_C,
	ICON_CACHE[] = MD_CACHED,
	ICON_CAD[] = FA_CUBE, /* No CAD icon */
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
	ICON_ELIXIR[] = MFIZZ_ELIXIR,
	ICON_ELM[] = MFIZZ_ELM,
	ICON_EMACS[] = FILE_EMACS,
	ICON_ERLANG[] = DEV_ERLANG,
	ICON_EXCEL[] = FILE_EXCEL,
	ICON_FONT[] = FILE_FONT,
	ICON_FORTRAN[] = FILE_FORTRAN,
	ICON_FREECAD[] = FA_CUBE, /* No FreeCAD icon */
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
	ICON_HELP[] = MD_HELP_OUTLINE,
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
	ICON_SCAD[] = FILE_SCAD,
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
	ICON_3D_FILE[] = NERD_3D_FILE,
	ICON_ACCESS[] = NERD_ACCESS,
	ICON_ARCHIVE[] = NERD_ARCHIVE,
	ICON_ARDUINO[] = NERD_ARDUINO,
	ICON_ASM[] = NERD_ASM,
	ICON_AUDIO[] = NERD_MUSICFILE,
	ICON_BINARY[] = NERD_BINARY,
	ICON_BLENDER[] = NERD_BLENDER,
	ICON_BOOK[] = NERD_BOOK,
	ICON_C[] = NERD_C,
	ICON_CACHE[] = NERD_CACHE,
	ICON_CAD[] = NERD_CAD,
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
	ICON_ELM[] = NERD_ELM,
	ICON_ELIXIR[] = NERD_ELIXIR,
	ICON_EMACS[] = NERD_EMACS,
	ICON_ERLANG[] = NERD_ERLANG,
	ICON_EXCEL[] = NERD_EXCELDOC,
	ICON_FONT[] = NERD_FONT,
	ICON_FORTRAN[] = NERD_FORTRAN,
	ICON_FREECAD[] = NERD_FREECAD,
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
	ICON_HELP[] = NERD_MANUAL,
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
	ICON_SCAD[] = NERD_SCAD,
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
/* Do not exceed 768 extensions. Otherwise, ext_table will raise from 1024
 * to 2048, probably making the lookup slower. Even more, keep the list
 * below 650 entries, to avoid overloading the table and reducing performance. */

	/* Graphics */
	{"ai", ICON_ILLUSTRATOR, YELLOW}, // Adobe Illustrator
	{"avif", ICON_IMG, GREEN}, // AVIF Image
	{"blend", ICON_BLENDER, YELLOW}, // Blender project
	{"bmp", ICON_IMG, GREEN}, // Bitmap Image
	{"dcm", ICON_IMG, GREEN}, // DICOM: Medical images
	{"emf", ICON_IMG, GREEN}, // Windows: wmf replacement
	{"eps", ICON_IMG, GREEN}, // Encapsulated PostScript File (vector graphic)
	{"exr", ICON_IMG, GREEN}, // OpenEXR raster image
	{"fts", ICON_IMG, GREEN}, // FITS (Flexible Image Transport System): Astronomy
	{"gif", ICON_IMG, GREEN}, // Graphical Interchange Format File
	{"hdr", ICON_IMG, GREEN}, // High Dynamic Range raster image
	{"heic", ICON_IMG, GREEN}, // High Efficiency Image Format
	{"heif", ICON_IMG, GREEN}, // High Efficiency Image Format
	{"ico", ICON_IMG, GREEN}, // Windows Icon File
	{"j2c", ICON_IMG, GREEN}, // JPEG 2000 Code Stream bitmap image
	{"j2k", ICON_IMG, GREEN}, // JPEG 2000 compressed bitmap image
	{"jfi", ICON_IMG, GREEN},  // JPEG File Interchange compressed bitmap (obsolete)
	{"jfif", ICON_IMG, GREEN}, // JPEG File Interchange compressed bitmap (obsolete)
	{"jif", ICON_IMG, GREEN}, // JPEG Interchange Format bitmap (obsolete)
	{"jp2", ICON_IMG, GREEN}, // JPEG 2000 Core Image (compressed bitmap)
	{"jpe", ICON_IMG, GREEN}, // 24-bit compressed JPEG graphic
	{"jpeg", ICON_IMG, GREEN}, // JPEG (Joint Photographic Experts Group) image
	{"jpf", ICON_IMG, GREEN}, // JPEG 2000 Image File
	{"jpg", ICON_IMG, GREEN}, // JPEG image
	{"jps", ICON_IMG, GREEN}, // Stereo JPEG Image
	{"jpx", ICON_IMG, GREEN}, // JPEG 2000 Image File
	{"jxl", ICON_IMG, GREEN}, // JPEG XL Image
	{"kpp", ICON_KRITA, MAGENTA}, // Krita image
	{"kra", ICON_KRITA, MAGENTA}, // Krita image
	{"krz", ICON_KRITA, MAGENTA}, // Krita image
	{"miff", ICON_IMG, GREEN}, // ImageMagick: Uncommon
	{"mng", ICON_IMG, GREEN}, // Multiple-Image Network Graphics: animated image
	{"ora", ICON_IMG, GREEN}, // OpenRaster Image
	{"pbm", ICON_IMG, GREEN}, // Portable Bitmap Image
	{"pcx", ICON_IMG, GREEN}, // Paintbrush Bitmap raster image (legacy)
	{"pdd", ICON_IMG, GREEN}, // Adobe PhotoDeluxe (legacy)
	{"pgm", ICON_IMG, GREEN}, // Portable Gray Map Image
	{"png", ICON_IMG, GREEN}, // Portable Network Graphic
	{"pnm", ICON_IMG, GREEN}, // Portable Any Map Image
	{"ppm", ICON_IMG, GREEN}, // Portable Pixmap Image
	{"psb", ICON_PHOTOSHOP, B_BLUE}, // Adobe Photoshop Big (PSB) format
	{"psd", ICON_PHOTOSHOP, B_BLUE}, // Adobe Photoshop Document
	{"pxm", ICON_IMG, GREEN}, // MacOS/iOS Pixelmator Image
	{"qoi", ICON_IMG, GREEN}, // Quite OK Image Format
	{"svg", ICON_IMG, GREEN}, // Scalable Vector Graphic
	{"svgz", ICON_IMG, GREEN}, // Compressed SVG
	{"tga", ICON_IMG, GREEN}, // Targa Graphic (Legacy)
	{"tif", ICON_IMG, GREEN}, // Tagged Image File Format
	{"tiff", ICON_IMG, GREEN}, // Tagged Image File Format
	{"webp", ICON_IMG, GREEN}, // WebP Image
	{"wmf", ICON_IMG, GREEN}, // Legacy: Windows
	{"xbm", ICON_IMG, GREEN}, // X11 BitMap Graphic
	{"xcf", ICON_GIMP, WHITE}, // GIMP Image
	{"xpm", ICON_IMG, GREEN}, // X11 Pixmap Graphic
	{"xwd", ICON_IMG, GREEN}, // X Windows Dump uncompressed bitmap
/*	{"ani", ICON_IMG, GREEN}, // Legacy: Animated icon
	{"apng", ICON_IMG, GREEN}, // Animated PNG (similar to GIF)
	{"cgm", ICON_IMG, GREEN}, // Legacy: vector graphics
	{"dib", ICON_IMG, GREEN}, // Device-Independent Bitmap Image (much like BMP)
	{"dpx", ICON_IMG, GREEN}, // Uncommon: highly specialized
	{"flif", ICON_IMG, GREEN}, // Free Lossless Image Format
	{"ff", ICON_IMG, GREEN}, // Farbfeld: Never took off
	{"hif", ICON_IMG, GREEN}, // Like HEIC, it is a HEIF variant (uncommon)
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

	/* 3D object files */
	{"3mf", ICON_3D_FILE, WHITE}, // 3D Manufacturing File
	{"fbx", ICON_3D_FILE, WHITE}, // Autodesk FBX Interchange File
	{"obj", ICON_3D_FILE, WHITE}, // Wavefront 3D Object File
	{"ply", ICON_3D_FILE, WHITE}, // Common for scanners
	{"stl", ICON_3D_FILE, WHITE}, // Stereolithography File
/*	{"3ds", ICON_3D_FILE, WHITE}, // Legacy
	{"dae", ICON_3D_FILE, WHITE}, // Collada: Legacy
	{"max", ICON_3D_FILE, WHITE}, // Proprietary to 3ds MAX
	{"off", ICON_3D_FILE, WHITE}, // Object File Format: uncommon/niche
	{"wrl", ICON_3D_FILE, WHITE}, // VRML variant: Rare
	{"wrz", ICON_3D_FILE, WHITE}, // VRML variant: Rare */

	{"dwg", ICON_CAD, RED}, // AutoCAD Drawing
	{"dxf", ICON_CAD, RED}, // AutoCAD Drawing
/*	{"123dx", ICON_CAD, RED}, // AutoCAD Drawing
	{"3dm", ICON_CAD, RED}, // AutoCAD Drawing
	{"brep", ICON_CAD, RED}, // AutoCAD Drawing
	{"catpart", ICON_CAD, RED}, // AutoCAD Drawing
	{"catproduct", ICON_CAD, RED}, // AutoCAD Drawing
	{"f3d", ICON_CAD, RED}, // AutoCAD Drawing
	{"f3z", ICON_CAD, RED}, // AutoCAD Drawing
	{"iam", ICON_CAD, RED}, // AutoCAD Drawing
	{"ifc", ICON_CAD, RED}, // AutoCAD Drawing
	{"ige", ICON_CAD, RED}, // IGES: CAD-legacy
	{"iges", ICON_CAD, RED}, // IGES: CAD-legacy
	{"igs", ICON_CAD, RED}, // IGES: CAD-legacy
	{"ipt", ICON_CAD, RED}, // AutoCAD Drawing
	{"psm", ICON_CAD, RED}, // AutoCAD Drawing
	{"skp", ICON_CAD, RED}, // AutoCAD Drawing
	{"sldasm", ICON_CAD, RED}, // AutoCAD Drawing
	{"sldprt", ICON_CAD, RED}, // AutoCAD Drawing
	{"slvs", ICON_CAD, RED}, // AutoCAD Drawing
	{"ste", ICON_CAD, RED}, // AutoCAD Drawing
	{"step", ICON_CAD, RED}, // AutoCAD Drawing
	{"stp", ICON_CAD, RED}, // AutoCAD Drawing
	{"x_b", ICON_CAD, RED}, // AutoCAD Drawing
	{"x_t", ICON_CAD, RED}, // AutoCAD Drawing */

	{"fcstd", ICON_FREECAD, RED}, // FreeCAD Drawing
/*	{"fcbak", ICON_FREECAD, RED}, // FreeCAD Drawing
	{"fcmacro", ICON_FREECAD, RED}, // FreeCAD Drawing
	{"fcmat", ICON_FREECAD, RED}, // FreeCAD Drawing
	{"fcparam", ICON_FREECAD, RED}, // FreeCAD Drawing
	{"fcscript", ICON_FREECAD, RED}, // FreeCAD Drawing
	{"fcstd", ICON_FREECAD, RED}, // FreeCAD Drawing
	{"fcstd1", ICON_FREECAD, RED}, // FreeCAD Drawing
	{"fctb", ICON_FREECAD, RED}, // FreeCAD Drawing
	{"fctl", ICON_FREECAD, RED}, // FreeCAD Drawing */

	{"scad", ICON_SCAD, YELLOW}, // OpenSCAD

	/* Video */
	{"3g2", ICON_VID, BLUE}, // 3GPP2 Multimedia File
	{"3gp", ICON_VID, BLUE}, // 3GPP Multimedia File
	{"3gp2", ICON_VID, BLUE}, // 3GPP Multimedia File
	{"3gpp", ICON_VID, BLUE}, // 3GPP Media File
	{"3gpp2", ICON_VID, BLUE}, // 3GPP2 Multimedia File
	{"asf", ICON_VID, BLUE}, // Advanced Systems Format File
	{"avi", ICON_VID, BLUE}, // Audio Video Interleave File
	{"divx", ICON_VID, BLUE}, // Legacy: common before mp4/mkv
	{"flv", ICON_VID, BLUE}, // Flash video
	{"h264", ICON_VID, BLUE}, // H.264 Encoded Video File
	{"heics", ICON_VID, BLUE}, // HEIF image sequence (video?)
	{"m2ts", ICON_VID, BLUE}, // Blu-ray BDAV Video File
	{"m2v", ICON_VID, BLUE}, // MPEG-2 Video
	{"m4v", ICON_VID, BLUE}, // iTunes Video File
	{"mjpeg", ICON_VID, BLUE}, // Motion JPEG Video
	{"mjpg", ICON_VID, BLUE}, // Motion JPEG Video
	{"mkv", ICON_VID, BLUE}, // Matroska Video
	{"mov", ICON_VID, BLUE}, // Apple QuickTime Movie
	{"mp4", ICON_VID, BLUE}, // MPEG-4 Video
	{"mp4v", ICON_VID, BLUE}, // Legacy: very common before H.264 dominance
	{"mpeg", ICON_VID, BLUE}, // MPEG Video
	{"mpg", ICON_VID, BLUE}, // MPEG Video
	{"ogv", ICON_VID, BLUE}, // Ogg Video
	{"qt", ICON_VID, BLUE}, // Apple QuickTime Movie
	{"rm", ICON_VID, BLUE}, // RealMedia File
	{"video", ICON_VID, BLUE}, // Generic (maybe aTube Catcher Video File)
	{"vob", ICON_VID, BLUE}, // DVD Video Object File
	{"ogm", ICON_VID, BLUE}, // Ogg Media File
	{"swf", ICON_VID, BLUE}, // Legacy: typical Flash container
	{"webm", ICON_VID, BLUE}, // WebM Video
	{"wmv", ICON_VID, BLUE}, // Windows Media Video
/*	{"f4v, ICON_VID, BLUE"} // Legacy: Flash MP4 Video File (quite uncommon)
	{"dv", ICON_VID, BLUE}, // Legacy (digital cameras 1990s-2000s)
	{"rmvb", ICON_VID, BLUE}, // Legacy: RealMedia Variable Bit Rate
	{"flc", ICON_VID, BLUE}, // Legacy: Autodesk
	{"fli", ICON_VID, BLUE}, // Legacy: Autodesk
	{"hevc", ICON_VID, BLUE}, // High Efficiency Video Coding
	{"gl", ICON_VID, BLUE}, // Legacy: GRASP animation (MS-DOS)
	{"m2t", ICON_VID, BLUE} // Legacy: HDV Video File
	{"mxf", ICON_VID, BLUE} // Material Exchange Format video
	{"nuv", ICON_VID, BLUE}, // NuppelVideo File
	{"wtv", ICON_VID, BLUE}, // Windows Recorded TV Show File
	{"y4m", ICON_VID, BLUE}, // YUV4MPEG2: Uncompressed, intermediate processing
	{"yuv", ICON_VID, BLUE}, // Uncompressed: may be both video or image */

	/* Audio */
//	{"3ga", ICON_AUDIO, YELLOW}, // 3GPP audio (mostly legacy)
	{"aac", ICON_AUDIO, YELLOW}, // Advanced Audio Coding File
	{"ac3", ICON_AUDIO, YELLOW}, // Audio Codec 3 File
	{"aif", ICON_AUDIO, B_YELLOW}, // Audio Interchange File Format (Lossless)
	{"aiff", ICON_AUDIO, B_YELLOW}, // Lossless
	{"aifc", ICON_AUDIO, B_YELLOW}, // Compressed Audio Interchange File (Lossless)
	{"alac", ICON_AUDIO, B_YELLOW}, // Lossless
	{"ape", ICON_AUDIO, B_YELLOW}, // Monkey's Audio (Lossless)
	{"au", ICON_AUDIO, YELLOW}, // Audacity Audio File
	{"aup", ICON_AUDIO, YELLOW}, // Audacity Project File
	{"aup3", ICON_AUDIO, YELLOW}, // Audacity 3 Project File
	{"flac", ICON_AUDIO, B_YELLOW}, // Lossless
	{"m2a", ICON_AUDIO, YELLOW}, // MPEG-1 Layer 2 Audio File: Legacy
	{"m4b", ICON_AUDIO, YELLOW}, // MPEG-4 Audiobook
	{"mid", ICON_AUDIO, YELLOW}, // MIDI (Musical Instrument Digital Interface)
	{"midi", ICON_AUDIO, YELLOW}, // MIDI
	{"mka", ICON_AUDIO, YELLOW}, // Matroska Audio
	{"m4a", ICON_AUDIO, YELLOW}, // MPEG-4 Audio
	{"mp2", ICON_AUDIO, YELLOW}, // MPEG Layer II Compressed Audio File
	{"mp3", ICON_AUDIO, YELLOW}, // MPEG Layer III Compressed Audio File
	{"mpga", ICON_AUDIO, YELLOW}, // Legacy, but widely used back then
	{"oga", ICON_AUDIO, YELLOW}, // OGG Vorbis (audio only)
	{"ogg", ICON_AUDIO, YELLOW}, // Ogg Vorbis
	{"opus", ICON_AUDIO, YELLOW}, // Opus audio (mostly used in streaming)
	{"pcm", ICON_AUDIO, B_YELLOW}, // Pulse Code Modulation File (Lossless)
	{"ra", ICON_AUDIO, YELLOW}, // RealAudio File
	{"sf2", ICON_AUDIO, YELLOW}, // SoundFont 2 Sound Bank
	{"spx", ICON_AUDIO, YELLOW}, // Ogg Vorbis Speex File
	{"tta", ICON_AUDIO, B_YELLOW}, // True Audio File (lossless)
	{"wav", ICON_AUDIO, B_YELLOW}, // Lossless
	{"wma", ICON_AUDIO, YELLOW}, // Windows Media Audio
	{"wv", ICON_AUDIO, B_YELLOW}, // WavPack Audio (lossless)
/*	{"axa", ICON_AUDIO, YELLOW}, // Legacy: Annodex Audio File
	{"bwf", ICON_AUDIO, YELLOW}, // Broadcast Wave Format (WAV extension)
	{"mpc", ICON_AUDIO, YELLOW}, // Legacy/Niche: Musepack Compressed Audio
	{"snd", ICON_AUDIO, YELLOW}, // Generic
	{"sou", ICON_AUDIO, YELLOW}, // Legacy
	{"voc", ICON_AUDIO, YELLOW}, // Legacy */

	/* Playlists */
	{"cue", ICON_PLAYLIST, YELLOW}, // Cue Sheet File (describes a CD layout)
	{"m3u", ICON_PLAYLIST, YELLOW}, // M3U Media Playlist
	{"m3u8", ICON_PLAYLIST, YELLOW}, // UTF-8 M3U Playlist
	{"pls", ICON_PLAYLIST, YELLOW}, // Multimedia Playlist File
//	{"wpl", ICON_PLAYLIST, YELLOW}, // Windows Media Player Playlist
//	{"xspf", ICON_PLAYLIST, YELLOW}, // XML Shareable Playlist File

	/* Archive/compressed */
	{"7z", ICON_ARCHIVE, YELLOW},
	{"a", ICON_ARCHIVE, WHITE}, // AR archive (like .ar): uncompressed
	{"ace", ICON_ARCHIVE, YELLOW}, // Legacy
	{"alz", ICON_ARCHIVE, YELLOW}, // ALZip file
	{"ar", ICON_ARCHIVE, WHITE}, // AR archive: uncompressed
	{"arc", ICON_ARCHIVE, YELLOW}, // Legacy: it could also be .ark
	{"arj", ICON_ARCHIVE, YELLOW}, // ARJ Compressed File Archive
	{"br", ICON_ARCHIVE, YELLOW}, // Brotli
	{"bz", ICON_ARCHIVE, YELLOW}, // bzip
	{"bz2", ICON_ARCHIVE, YELLOW}, // bzip version 2
	{"bz3", ICON_ARCHIVE, YELLOW}, // bzip version 3
	{"cab", ICON_ARCHIVE, YELLOW}, // Windows Cabinet File
	{"cpio", ICON_ARCHIVE, WHITE}, // Uncompressed
	{"ear", ICON_ARCHIVE, YELLOW}, // Java enterprise archive
	{"gz", ICON_ARCHIVE, YELLOW}, // Gnu Zipped File
	{"gzip", ICON_ARCHIVE, YELLOW}, // Gnu Zipped File
	{"jar", ICON_ARCHIVE, YELLOW}, // Java archive
	{"lbr", ICON_ARCHIVE, WHITE}, // Uncompressed
	{"lrz", ICON_ARCHIVE, YELLOW}, // lrzip
	{"lz", ICON_ARCHIVE, YELLOW}, // Lzip Compressed File
	{"lz4", ICON_ARCHIVE, YELLOW}, // LZ4 Compressed File
	{"lzh", ICON_ARCHIVE, YELLOW}, // Lempel-Ziv and Haruyasu Compressed File
	{"lzma", ICON_ARCHIVE, YELLOW}, // Lempel-Ziv-Markov chain Algorithm Compressed File
	{"lzo", ICON_ARCHIVE, YELLOW}, // Lempel-Ziv-Oberhume Compressed File
	{"pak", ICON_ARCHIVE, YELLOW}, // Game archive (ZIP) / PAK compressed (ARC variant)
	{"phar", ICON_ARCHIVE, YELLOW}, // PHP archive
	{"pk3", ICON_ARCHIVE, YELLOW}, // ZIP-based
	{"par", ICON_ARCHIVE, YELLOW}, // Parchive Index File
	{"pyz", ICON_ARCHIVE, YELLOW}, // Python Application Zip File
	{"rar", ICON_ARCHIVE, YELLOW}, // WinRAR Compressed Archive (proprietary)
	{"rz", ICON_ARCHIVE, YELLOW}, // rzip
	{"t7z", ICON_ARCHIVE, YELLOW}, // .tar.7z
	{"tar", ICON_ARCHIVE, WHITE}, // Tape Archive: Uncompressed
	{"taz", ICON_ARCHIVE, YELLOW}, // .tar.Z (also .tz)
	{"tbz", ICON_ARCHIVE, YELLOW}, // .tar.bz (bzip)
	{"tbz2", ICON_ARCHIVE, YELLOW}, // .tar.bz2 (bzip2)
	{"tbz3", ICON_ARCHIVE, YELLOW}, // .tar.bz3 (bzip3)
	{"tgz", ICON_ARCHIVE, YELLOW}, // .tar.gz (gzip)
	{"tlz", ICON_ARCHIVE, YELLOW}, // .tar.lz (lzip)
	{"txz", ICON_ARCHIVE, YELLOW}, // .tar.xz
	{"tz", ICON_ARCHIVE, YELLOW}, // .tar.Z (compress)
	{"tzo", ICON_ARCHIVE, YELLOW}, // .tar.lzo (lzop)
	{"tzst", ICON_ARCHIVE, YELLOW}, // .tar.zst
	{"war", ICON_ARCHIVE, YELLOW}, /* Java web archive */
	{"xz", ICON_ARCHIVE, YELLOW}, // XZ Compressed File
	{"z", ICON_ARCHIVE, YELLOW}, // Unix Compress'ed File
	{"zip", ICON_ARCHIVE, YELLOW}, // Zipped File
	{"zoo", ICON_ARCHIVE, YELLOW}, // Zoo Compressed File (legacy)
	{"zpaq", ICON_ARCHIVE, YELLOW}, // ZPAQ File
	{"zst", ICON_ARCHIVE, YELLOW}, // Zstandard File
/*	{"dar", ICON_ARCHIVE, YELLOW} // Disk archive
	{"drpm", ICON_ARCHIVE, YELLOW}, // Delta RPM archive
	{"dz", ICON_ARCHIVE, YELLOW}, // Dzip file
	{"egg", ICON_ARCHIVE, YELLOW}, // ALZip file (previously .alz)
	{"lha", ICON_ARCHIVE, YELLOW}, // Legacy: Amiga (like .lzh)
	{"lzx", ICON_ARCHIVE, YELLOW}, // Legacy: Amiga
	{"maff", ICON_ARCHIVE, YELLOW}, // Mozilla Archive File Format (ZIP-based)
	{"pax", ICON_ARCHIVE, WHITE}, // Uncompressed
	{"pea", ICON_ARCHIVE, YELLOW}, // PeaZip
	{"pk7", ICON_ARCHIVE, YELLOW}, // GZDoom archive file
	{"shar", ICON_ARCHIVE, WHITE}, // Uncompressed
	{"sqx", ICON_ARCHIVE, WHITE}, // SQX Archive
	{"uc2", ICON_ARCHIVE, WHITE}, // Legacy: Ultra Compressor II
	{"warc", ICON_ARCHIVE, WHITE}, // Uncompressed: Web archive
	{"zipx", ICON_ARCHIVE, YELLOW}, // Extended ZIP archive (Winzip 12.1) */

	/* Packages */
	{"apk", ICON_PACKAGE, YELLOW}, // Android package (ZIP-based)
	{"Appimage", ICON_PACKAGE, YELLOW}, // Linux Software Package
	{"crate", ICON_PACKAGE, YELLOW}, // Rust package
	{"crx", ICON_PACKAGE, YELLOW}, // Chrome add-ons
	{"deb", ICON_PACKAGE, YELLOW}, // Debian package
	{"flatpak", ICON_PACKAGE, YELLOW}, // Linux Flatpak Application Bundle
	{"gem", ICON_PACKAGE, YELLOW}, // RubyGems Package
	{"msi", ICON_PACKAGE, YELLOW}, // Windows installer
	{"pkg", ICON_PACKAGE, YELLOW}, // Generic package file (used by several vendors)
	{"rpm", ICON_PACKAGE, YELLOW}, // Red Hat Package Manager File
	{"snap", ICON_PACKAGE, YELLOW}, // Snap Application Package
	{"whl", ICON_PACKAGE, YELLOW}, // Python Wheel package
	{"xbps", ICON_PACKAGE, YELLOW}, // Void Linux package
	{"xpi", ICON_PACKAGE, YELLOW}, // Mozilla add-ons
/*	{"aar", ICON_PACKAGE, YELLOW}, // Android archive (ZIP-based)
	{"udeb", ICON_PACKAGE, YELLOW}, // Debian micro package
	{"appx", ICON_PACKAGE, YELLOW} // Windows 8 app installer
	{"appxbundle", ICON_PACKAGE, YELLOW} // Windows 8 app installer
	{"msix", ICON_PACKAGE, YELLOW} // Windows 10 app installer
	{"msixbundle", ICON_PACKAGE, YELLOW} // Windows 10 app installer */

	/* Disk images */
	{"aff", ICON_DISK, WHITE}, // Advanced Forensics Format disk image
	{"dmg", ICON_DISK, WHITE}, // Apple Disk Image
	{"image", ICON_DISK, WHITE}, // Apple Disk Image
	{"img", ICON_DISK, WHITE}, // Disc Image Data File
	{"qcow", ICON_DISK, WHITE}, // QEMU Copy On Write Disk Image
	{"qcow2", ICON_DISK, WHITE}, // QEMU Copy On Write Version 2
	{"vdi", ICON_DISK, WHITE}, // VirtualBox Virtual Disk Image
	{"vhd", ICON_DISK, WHITE}, // Virtual PC Virtual Hard Disk
	{"vhdx", ICON_DISK, WHITE}, // Windows 8 Virtual Hard Drive File
	{"vmdk", ICON_DISK, WHITE}, // Virtual Machine Disk File
/*	{"adf", ICON_DISK, WHITE}, // Amiga disk file (also adz, hdf, hdz)
	{"ima", ICON_DISK, WHITE}, // Disk Image
	{"wim", ICON_DISK, WHITE}, // Windows Imaging Format File */

	/* Optical disk images */
	{"cdi", ICON_CD, WHITE}, // DiscJuggler Disc Image
	{"iso", ICON_CD, WHITE}, // ISO-9660 Disk Image
	{"isz", ICON_CD, WHITE}, // Zipped ISO Disk Image
	{"nrg", ICON_CD, WHITE}, // Nero CD/DVD Image File
	{"tc", ICON_CD, WHITE}, // TrueCrypt Volume
	{"toast", ICON_CD, WHITE}, // Toast Disc Image
	{"vcd", ICON_CD, WHITE}, // Virtual CD

	/* Programming */
	{"asm", ICON_ASM, BLUE},
	{"awk", ICON_SHELL, WHITE},
	{"bash", ICON_SHELL, WHITE},
	{"bat", ICON_SHELL, WHITE},
	{"c", ICON_C, BLUE},
	{"c++", ICON_CPP, B_BLUE},
	{"cc", ICON_CPP, B_BLUE},
	{"cjs", ICON_JAVASCRIPT, WHITE},
	{"class", ICON_JAVA, WHITE},
	{"clj", ICON_CLOJURE, GREEN},
	{"cljc", ICON_CLOJURE, GREEN},
	{"cljs", ICON_CLOJURE, B_BLUE},
	{"cmake", ICON_CMAKE, WHITE},
	{"coffee", ICON_COFFEE, WHITE},
	{"cp", ICON_CPP, B_BLUE},
	{"cpp", ICON_CPP, B_BLUE},
	{"cs", ICON_CSHARP, MAGENTA},
	{"csh", ICON_SHELL, WHITE},
	{"csproj", ICON_CSHARP, MAGENTA},
	{"css", ICON_CSS, BLUE},
	{"csx", ICON_CSHARP, MAGENTA},
	{"cts", ICON_TS, BLUE},
	{"cu", ICON_CUDA, BLUE},
	{"cxx", ICON_CPP, B_BLUE},
	{"d", ICON_D, RED},
	{"dart", ICON_DART, BLUE},
	{"di", ICON_D, RED},
	{"dot", ICON_GRAPHVIZ, WHITE},
	{"eex", ICON_ELIXIR, BLUE},
	{"ejs", ICON_JAVASCRIPT, WHITE},
	{"el", ICON_EMACS, BLUE}, // Lisp source code
	{"elm", ICON_ELM, GREEN},
	{"erb", ICON_RUBYRAILS, RED},
	{"erl", ICON_ERLANG, RED},
	{"ex", ICON_ELIXIR, BLUE},
	{"exs", ICON_ELIXIR, BLUE},
	{"f", ICON_FORTRAN, MAGENTA}, // In uppercase, it may also be a Freeze compressed file
	{"f90", ICON_FORTRAN, MAGENTA},
	{"f#", ICON_FSHARP, CYAN},
	{"fish", ICON_SHELL, WHITE},
	{"for", ICON_FORTRAN, MAGENTA},
	{"fs", ICON_FSHARP, CYAN},
	{"fsi", ICON_FSHARP, CYAN},
	{"fsproj", ICON_FSHARP, CYAN},
	{"fsscript", ICON_FSHARP, CYAN},
	{"fsx", ICON_FSHARP, CYAN},
	{"gd", ICON_GODOT, CYAN},
	{"gemspec", ICON_RUBY, RED},
	{"gleam", ICON_GLEAM, MAGENTA},
	{"go", ICON_GO, B_BLUE},
	{"godot", ICON_GODOT, CYAN},
	{"gql", ICON_GRAPHQL, MAGENTA},
	{"gradle", ICON_GRADLE, WHITE},
	{"graphql", ICON_GRAPHQL, MAGENTA},
	{"groovy", ICON_GROOVY, CYAN},
	{"gv", ICON_GRAPHVIZ, WHITE},
	{"gvy", ICON_GROOVY, CYAN},
	{"h", ICON_C, BLUE},
	{"h++", ICON_CPP, B_BLUE},
	{"haml", ICON_HAML, YELLOW},
	{"hbs", ICON_MUSTACHE, WHITE},
	{"hh", ICON_CPP, B_BLUE},
	{"hpp", ICON_CPP, B_BLUE},
	{"hrl", ICON_ERLANG, RED},
	{"hs", ICON_HASKELL, BLUE},
	{"hxx", ICON_CPP, B_BLUE},
	{"inl", ICON_CPP, B_BLUE},
	{"ino", ICON_ARDUINO, GREEN},
	{"ipynb", ICON_JUPYTER, RED},
	{"jad", ICON_JAVA, WHITE},
	{"java", ICON_JAVA, WHITE},
	{"jl", ICON_JULIA, BLUE},
	{"js", ICON_JAVASCRIPT, WHITE},
	{"jsx", ICON_ELECTRON, B_GREEN},
	{"ksh", ICON_SHELL, WHITE},
	{"kt", ICON_KOTLIN, BLUE},
	{"kts", ICON_KOTLIN, BLUE},
	{"leex", ICON_ELIXIR, BLUE},
	{"lhs", ICON_HASKELL, BLUE},
	{"ls", ICON_LIVESCRIPT, CYAN},
	{"lua", ICON_LUA, BLUE},
	{"luac", ICON_LUA, BLUE},
	{"luau", ICON_LUA, BLUE},
	{"m", ICON_C, BLUE},
	{"mat", ICON_MATLAB, YELLOW},
	{"mjs", ICON_JAVASCRIPT, WHITE},
	{"mk", ICON_MAKEFILE, B_BLUE},
	{"ml", ICON_OCAML, WHITE},
	{"mli", ICON_OCAML, WHITE},
	{"mll", ICON_OCAML, WHITE},
	{"mly", ICON_OCAML, WHITE},
	{"mm", ICON_CPP, B_BLUE},
	{"mts", ICON_TS, BLUE},
	{"mustache", ICON_MUSTACHE, WHITE},
	{"nim", ICON_NIM, YELLOW},
	{"nimble", ICON_NIM, YELLOW},
	{"nims", ICON_NIM, YELLOW},
	{"nix", ICON_NIX, BLUE},
	{"node", ICON_NODEJS, GREEN},
	{"nu", ICON_SHELL, WHITE},
	{"php", ICON_PHP, MAGENTA},
	{"pl", ICON_PERL, YELLOW},
	{"plx", ICON_PERL, YELLOW},
	{"pm", ICON_PERL, YELLOW},
	{"pod", ICON_PERL, YELLOW},
	{"ps", ICON_POSTSCRIPT, RED},
	{"ps1", ICON_POWERSHELL, WHITE},
	{"psd1", ICON_POWERSHELL, WHITE},
	{"psm1", ICON_POWERSHELL, WHITE},
	{"purs", ICON_PURESCRIPT, WHITE},
	{"pxd", ICON_PYTHON, GREEN},
	{"py", ICON_PYTHON, GREEN},
	{"pyc", ICON_PYTHON, GREEN},
	{"pyd", ICON_PYTHON, GREEN},
	{"pyi", ICON_PYTHON, GREEN},
	{"pyo", ICON_PYTHON, GREEN},
	{"pyw", ICON_PYTHON, GREEN},
	{"r", ICON_R, BLUE},
	{"rb", ICON_RUBY, RED},
	{"rdata", ICON_R, BLUE},
	{"rds", ICON_R, BLUE},
	{"rkt", ICON_SCHEME, WHITE},
	{"rlib", ICON_RUST, WHITE},
	{"rmd", ICON_RUST, WHITE},
	{"rmeta", ICON_RUST, WHITE},
	{"rs", ICON_RUST, WHITE},
	{"rss", ICON_RSS, BLUE},
	{"s", ICON_ASM, BLUE},
	{"sass", ICON_SASS, B_BLUE},
	{"scala", ICON_SCALA, RED},
	{"scm", ICON_SCHEME, WHITE},
	{"scss", ICON_SASS, B_BLUE},
	{"service", ICON_SHELL, WHITE},
	{"sh", ICON_SHELL, WHITE},
	{"sld", ICON_SCHEME, WHITE},
	{"slim", ICON_RUBYRAILS, RED},
	{"sln", ICON_VISUALSTUDIO, BLUE},
	{"sql", ICON_MYSQL, CYAN},
	{"ss", ICON_SCHEME, WHITE},
	{"styl", ICON_STYLUS, B_GREEN}, // Stylus File
	{"stylus", ICON_STYLUS, B_GREEN}, // Stylus File
	{"suo", ICON_VISUALSTUDIO, BLUE},
	{"svelte", ICON_SVELTE, RED},
	{"swift", ICON_SWIFT, GREEN},
	{"t", ICON_PERL, YELLOW},
	{"tbc", ICON_TCL, GREEN},
	{"tcl", ICON_TCL, GREEN},
	{"tres", ICON_GODOT, CYAN},
	{"ts", ICON_TS, BLUE},
	{"tscn", ICON_GODOT, CYAN},
	{"tsx", ICON_REACT, B_BLUE},
	{"twig", ICON_TWIG, GREEN},
	{"v", ICON_V, B_BLUE},
	{"vala", ICON_VALA, MAGENTA},
	{"vim", ICON_VIM, GREEN},
	{"vsix", ICON_VISUALSTUDIO, BLUE},
	{"vue", ICON_VUE, GREEN},
	{"zig", ICON_ZIG, YELLOW},
	{"zsh", ICON_SHELL, WHITE},
/*	{"iml", ICON_INTELLIJ, BLUE or RED},
	{"rdf", ICON_REDIS, RED},
	{"aof", ICON_REDIS, RED},
	{"cr", ICON_CRYSTAL, WHITE}, // nf-dev-crystal
	{"fnl", ICON_FENNEL, GREEN}, // No IIT icon
	{"hack", ICON_HACK, YELLOW}, nf-seti-hacklang
	{"qml", ICON_QT, GREEN}, // No IIT icon
	{"qrc", ICON_QT, GREEN},
	{"qss", ICON_QT, GREEN},
	{"raku", ICON_?, ?} // Icon: a colorful butterfly
	{"sv", ICON_LANG_HDL, ?},
	{"svh", ICON_LANG_HDL, ?},
	{"vbs", ICON_?, ?} // Visual Basic script
	{"vhdl", ICON_LANG_HDL, ?},
	{"typ", ICON_TYPST, GREEN}, // No IIT icon
	{"unity, ICON_UNITY, WHITE"},
	{"unity3d, ICON_UNITY, WHITE"},
//	applescript, scptd, scpt, ICON_APPLE, WHITE */

	/* Markup */
	{"asp", ICON_HTML, YELLOW}, // Active Server Page
	{"aspx", ICON_HTML, YELLOW}, // Active Server Page Extended Webpage
#ifdef _ICONS_IN_TERMINAL
	{"bib", FILE_BIBTEX, WHITE}, // BibTeX Bibliography Database
#else
	{"bib", ICON_TEX, WHITE}, // BibTeX Bibliography Database
#endif
	{"bst", ICON_TEX, WHITE}, // BibTeX Style Document
	{"cls", ICON_TEX, WHITE}, // LaTeX Document Class File
	{"htm", ICON_HTML, YELLOW}, // Hypertext Markup Language File
	{"html", ICON_HTML, YELLOW}, // Hypertext Markup Language File
	{"jmd", ICON_MARKDOWN, WHITE}, // Jupyter Markdown File
	{"jsp", ICON_HTML, YELLOW}, // Jakarta Server Page
	{"jspx", ICON_HTML, YELLOW}, // XML Jakarta Server Page
	{"latex", ICON_TEX, WHITE}, // LaTeX Document
	{"ltx", ICON_TEX, WHITE}, // LaTeX Document
	{"markdown", ICON_MARKDOWN, WHITE}, // Markdown Documentation File
	{"md", ICON_MARKDOWN, WHITE}, // Markdown Documentation File
	{"mdown", ICON_MARKDOWN, WHITE}, // Markdown Documentation File
	{"mdx", ICON_MARKDOWN, WHITE}, // Markdown Documentation File
	{"mkd", ICON_MARKDOWN, WHITE}, // Markdown Documentation File
	{"opf", ICON_XML, YELLOW}, // OEB Package Format (FlipViewer FlipBook File)
	{"opml", ICON_XML, YELLOW}, // Outline Processor Markup Language File
	{"rdf", ICON_XML, YELLOW}, // Resource Description Framework File
	{"rdoc", ICON_MARKDOWN, WHITE}, // Ruby Document
	{"sgml", ICON_XML, YELLOW}, // Standard Generalized Markup Language File
	{"shtml", ICON_HTML, YELLOW}, // Server Side Include HTML File
	{"sty", ICON_TEX, WHITE}, // LaTeX Style Files
	{"tld", ICON_XML, YELLOW}, // Tag Library Descriptor File - used for Java Server Page (JSP)
	{"tex", ICON_TEX, WHITE}, // LaTeX Source Document
	{"xaml", ICON_XAML, WHITE}, // Extensible Application Markup Language File
	{"xbel", ICON_XML, YELLOW}, // XML Bookmark Exchange Language
	{"xhtml", ICON_HTML, YELLOW}, // Extensible Hypertext Markup Language File
	{"xml", ICON_XML, YELLOW}, // Extensible Markup Language File
	{"xmp", ICON_XML, YELLOW}, // Extensible Metadata Platform File
	{"xsd", ICON_XML, YELLOW}, // XML Schema Definition
	{"xsl", ICON_XML, YELLOW}, // XML Stylesheet
	{"xslt", ICON_XML, YELLOW}, // Extensible Stylesheet Language Transformations File
	{"xul", ICON_XML, YELLOW}, // XML User Interface Language File (Mozilla)
/*	{"mhtml", ICON_HTML, YELLOW}, // MIME HTML File */

	{"diff", ICON_DIFF, WHITE}, // Patch File
	{"patch", ICON_DIFF, WHITE}, // Patch File

	/* Binaries */
	{"bin", ICON_BINARY, WHITE}, // Generic Binary File
	{"com", ICON_EXEC, WHITE}, // DOS Command File
	{"dat", ICON_BINARY, WHITE}, // Generic binary data file
	{"dll", ICON_SHARE, BLUE}, // Dynamic Link Library (Windows)
	{"dylib", ICON_SHARE, BLUE}, // Mach-O Dynamic Library (MacOS)
	{"elc", ICON_EMACS, BLUE}, // Compiled bytecode from .el files (Emacs)
	{"elf", ICON_BINARY, WHITE}, // Executable and Linkable Format (Unix)
	{"exe", ICON_EXEC, WHITE}, // Windows Executable File
	{"ko", ICON_BINARY, WHITE}, // Linux Kernel Module File
	{"lib", ICON_SHARE, BLUE}, // Generic Data Library
	{"o", ICON_BINARY, WHITE}, // Compiled C Object File
	{"out", ICON_BINARY, WHITE}, // Compiled Executable File
	{"so", ICON_BINARY, WHITE}, // Unix Shared Library

	/* Config */
	{"avro", ICON_JSON, WHITE}, // Avro Data File (JSON binary encoding)
	{"cfg", ICON_CONFIG, WHITE}, // Generic Configuration File
	{"clifm", ICON_CONFIG, WHITE}, // Clifm config file
	{"conf", ICON_CONFIG, WHITE}, // Unix Configuration File
	{"desktop", ICON_CONFIG, WHITE}, // Desktop Entry File
	{"ini", ICON_CONFIG, WHITE}, // Initialization File
	{"json", ICON_JSON, WHITE}, // JSON: JavaScript Object Notation file
	{"json5", ICON_JSON, WHITE}, // JSON5 Data File
	{"jsonc", ICON_JSON, WHITE}, // JSON With Comments File
	{"rc", ICON_CONFIG, WHITE}, // Resource Configuration File
	{"theme", ICON_CONFIG, WHITE}, // GTK Theme Index File (INI format)
	{"tml", ICON_CONFIG, WHITE}, // Generic config file
	{"toml", ICON_TOML, WHITE}, // TOML Configuration File
	{"yaml", ICON_YAML, WHITE}, // YAML Document
	{"yml", ICON_YAML, WHITE}, // YAML Document
	{"webmanifest", ICON_JSON, WHITE}, // Progressive Web Application Manifest

	/* Databases */
	{"db", ICON_DATABASE, WHITE}, // Database File
	{"db3", ICON_SQLITE, BLUE}, // SQLite Database
	{"dbf", ICON_DATABASE, WHITE}, // Database File
	{"dconf", ICON_DATABASE, WHITE}, // Gnome DConf binary config file
	{"dump", ICON_DATABASE, WHITE}, // Plain-text export/backup/analyze data
	{"fdb", ICON_DATABASE, WHITE}, // Microsoft Dynamics NAV Database
	{"gdb", ICON_DATABASE, WHITE}, // InterBase Database / GPS Database
	{"kdb", ICON_DATABASE, WHITE}, // Keypass Database
	{"kdbx", ICON_DATABASE, WHITE}, // KeePass Password Database
	{"mdb", ICON_ACCESS, RED}, // Microsoft Access Database
	{"mdf", ICON_DATABASE, WHITE}, // SQL Server Database
	{"ndf", ICON_DATABASE, WHITE}, // SQL Server Secondary Database
	{"s3db", ICON_SQLITE, BLUE}, // SQLite Database
	{"sdf", ICON_DATABASE, WHITE}, // SQL Server Compact Database
	{"sl3", ICON_SQLITE, BLUE}, // SQLite Database
	{"sqlite", ICON_SQLITE, BLUE}, // SQLite Database
	{"sqlite3", ICON_SQLITE, BLUE}, // SQLite Database

	/* Security */
	{"asc", ICON_LOCK, GREEN},
	{"age", ICON_LOCK, GREEN},
	{"cert", ICON_CERT, RED},
	{"crt", ICON_CERT, RED},
	{"gpg", ICON_LOCK, GREEN},
	{"kbx", ICON_KEY, RED},
	{"key", ICON_KEY, YELLOW},
	{"lck", ICON_LOCK, WHITE},
	{"lock", ICON_LOCK, WHITE},
	{"md5", ICON_CHECKSUM, GREEN},
	{"p12", ICON_KEY, YELLOW},
	{"pem", ICON_KEY, YELLOW},
	{"pfx", ICON_KEY, YELLOW},
	{"pub", ICON_KEY, GREEN},
	{"sha1", ICON_CHECKSUM, GREEN},
	{"sha224", ICON_CHECKSUM, GREEN},
	{"sha256", ICON_CHECKSUM, GREEN},
	{"sha384", ICON_CHECKSUM, GREEN},
	{"sha512", ICON_CHECKSUM, GREEN},
	{"sig", ICON_SIG, GREEN},

	/* Office */
	{"abw", ICON_LIBREOFFICE_WRITER, BLUE},
	{"doc", ICON_WORD, BLUE},
	{"docx", ICON_WORD, BLUE},
	{"csv", ICON_EXCEL, GREEN}, // Comma-Separated Values File
	{"dif", ICON_EXCEL, GREEN}, // Data Interchange Format (ASCII text)
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
	{"gdoc", ICON_WORD, BLUE}, // Google Drive Document
	{"gdocx", ICON_WORD, BLUE}, // Google Drive Document
	{"gsheet", ICON_EXCEL, GREEN}, // Google Sheets Shortcut
	{"gslides", ICON_POWERPOINT, YELLOW}, // Google Slides Shortcut
	{"numbers", ICON_EXCEL, GREEN}, /* Apple */
	{"odb", ICON_LIBREOFFICE_BASE, RED},
	{"odf", ICON_LIBREOFFICE_MATH, WHITE},
	{"odg", ICON_LIBREOFFICE_DRAW, MAGENTA},
	{"odp", ICON_LIBREOFFICE_IMPRESS, YELLOW},
	{"ods", ICON_LIBREOFFICE_CALC, GREEN},
	{"odt", ICON_LIBREOFFICE_WRITER, BLUE},
	{"otg", ICON_LIBREOFFICE_DRAW, MAGENTA},
	{"otp", ICON_LIBREOFFICE_IMPRESS, YELLOW},
	{"ots", ICON_LIBREOFFICE_CALC, GREEN},
	{"ott", ICON_LIBREOFFICE_WRITER, BLUE},
	{"pages", ICON_WORD, BLUE}, /* Apple */
	{"potm", ICON_POWERPOINT, YELLOW},
	{"potx", ICON_POWERPOINT, YELLOW},
	{"pps", ICON_POWERPOINT, YELLOW},
	{"ppsx", ICON_POWERPOINT, YELLOW},
	{"ppt", ICON_POWERPOINT, YELLOW},
	{"pptm", ICON_POWERPOINT, YELLOW},
	{"pptx", ICON_POWERPOINT, YELLOW},
	{"rtf", ICON_WORD, CYAN},
	{"slk", ICON_EXCEL, GREEN}, // Symbolic Link File (semi-colons delimited text)
	{"tsv", ICON_EXCEL, GREEN}, // Tab-Separated Values File
	{"uop", ICON_LIBREOFFICE_IMPRESS, YELLOW},
	{"uos", ICON_LIBREOFFICE_CALC, GREEN},
	{"uot", ICON_LIBREOFFICE_WRITER, BLUE},
	{"wps", ICON_WORD, BLUE}, /* Kingsoft WPS Office */
	{"wpt", ICON_WORD, BLUE}, /* Kingsoft WPS Office */
	{"xlr", ICON_EXCEL, GREEN},
	{"xls", ICON_EXCEL, GREEN},
	{"xlsb", ICON_EXCEL, GREEN},
	{"xlsm", ICON_EXCEL, GREEN},
	{"xlsx", ICON_EXCEL, GREEN},
	{"xlt", ICON_EXCEL, GREEN},
	{"xltm", ICON_EXCEL, GREEN},
	{"xltx", ICON_EXCEL, GREEN},
/*	{"dot", ICON_WORD, BLUE} // Old: conflicts with Graphviz files
	{"key", ICON_POWERPOINT, YELLOW}, // Apple. Conflicts with .key security files
	{"pot", ICON_POWERPOINT, YELLOW}, // Conflicts with .pot translation files */

	/* Ebooks */
	{"azw", ICON_BOOK, WHITE},
	{"azw3", ICON_BOOK, WHITE},
	{"azw4", ICON_BOOK, WHITE},
	{"cb7", ICON_BOOK, YELLOW},
	{"cba", ICON_BOOK, YELLOW},
	{"cbr", ICON_BOOK, YELLOW},
	{"cbz", ICON_BOOK, YELLOW},
	{"djv", ICON_BOOK, WHITE},
	{"djvu", ICON_BOOK, WHITE},
	{"epub", ICON_BOOK, WHITE},
	{"fb2", ICON_BOOK, WHITE},
	{"kf8", ICON_BOOK, WHITE},
	{"kfx", ICON_BOOK, WHITE},
	{"mobi", ICON_BOOK, WHITE},
	{"pdf", ICON_PDF, RED},
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

	/* Fonts */
	{"bdf", ICON_FONT, BLUE}, // X11 bitmap font
	{"eot", ICON_FONT, BLUE}, // Embedded OpenType Font
	{"flf", ICON_FONT, BLUE}, // Plain‑text FIGlet font
	{"fnt", ICON_FONT, BLUE}, // Legacy: DOS/Windows bitmap font
	{"fon", ICON_FONT, BLUE}, // Legacy: DOS/Windows bitmap font
	{"font", ICON_FONT, BLUE}, // Generic
	{"lff", ICON_FONT, BLUE}, // Legacy
	{"otf", ICON_FONT, BLUE},
	{"psf", ICON_FONT, BLUE}, // Unix console font
	{"ttc", ICON_FONT, BLUE}, // TrueType Font Collection
	{"ttf", ICON_FONT, BLUE},
	{"woff", ICON_FONT, BLUE}, // Web font
	{"woff2", ICON_FONT, BLUE}, // Web font

	/* Subtitles */
	{"ass", ICON_SUBTITLES, WHITE}, // Aegisub Advanced SubStation Alpha
	{"lrc", ICON_SUBTITLES, WHITE}, // Lyrics File
	{"sbt", ICON_SUBTITLES, WHITE}, // SBT Subtitle File
	{"srt", ICON_SUBTITLES, WHITE},
	{"ssa", ICON_SUBTITLES, WHITE}, // Sub Station Alpha Subtitle File
	{"sub", ICON_SUBTITLES, WHITE},

	/* Misc */
	{"cache", ICON_CACHE, WHITE},
	{"chm", ICON_HELP, WHITE},
	{"log", ICON_LOG, WHITE},
	{"rst", ICON_TXT, WHITE},
	{"txt", ICON_TXT, WHITE},

	{"crdownload", ICON_DOWNLOADS, WHITE},
	{"download", ICON_DOWNLOADS, WHITE}, // Safari Partially Downloaded File
	{"fdmdownload", ICON_DOWNLOADS, WHITE}, // Free Download Manager Partially Downloaded File
	{"magnet", ICON_MAGNET, RED},
	{"opdownload", ICON_DOWNLOADS, WHITE}, // Opera Partially Downloaded File
	{"part", ICON_DOWNLOADS, WHITE},
	{"torrent", ICON_DOWNLOADS, WHITE},

	{"ical", ICON_CALENDAR, WHITE},
	{"icalendar", ICON_CALENDAR, WHITE}, // Rare
	{"ics", ICON_CALENDAR, WHITE},
	{"ifb", ICON_CALENDAR, WHITE}, // Niche

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

/* Legacy
 * Amiga: nfo, diz, mod, pic, lbm, ilbm, dib, rle, adf, dms
 * MS-DOS: voc, sou, bas, sys, vxd, drv, hlp, ffn, qfs (archive),
 * tgv (video), mdi (image), pif (info, binary) */

/*	{"tf", ICON_TERRAFORM, MAGENTA},
	{"tfstate", ICON_TERRAFORM, MAGENTA},
	{"tfvars", ICON_TERRAFORM, MAGENTA}, */

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
	{"prjpcb", ICON_EDA_PCB, ??}, */
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
	{".clang-format", ICON_CONFIG, WHITE},
	{".clang-tidy", ICON_CONFIG, WHITE},
	{".DS_Store", ICON_CONFIG, WHITE},
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
