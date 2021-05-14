#!/bin/sh

# FZF navigation/previewing plugin for CliFM
# Written by L. Abramovich
# License: GPL3

# Description: Navigate the filesystem (including file previews) with FZF.
# Previews are shown by just hovering on the file. Each generated preview is
# cached in $HOME/.cache/clifm/previews to speed up the previewing process. A
# separate script is used for file previewing: $HOME/.config/clifm/plugins/BFG.sh

# Usage:
# Left: go to parent directory
# Right, Enter: cd into hovered directory or open hovered file and exit.
# Home/end: go to first/last file
# Shift+up/down: Move one line up/down in the preview window
# Alt+up/down: Move to the beginning/end in the preview window
# At exit (pressing q) CliFM will change to the last directory visited
# with FZF or open the last accepted file (Enter).
# Press Esc to cancel and exit.

if [ -n "$1" ] && ([ "$1" = "--help" ] || [ "$1" = "help" ]); then
	name="$(basename "$0")"
	printf "Navigate/preview files via FZF\n"
	printf "Usage: %s\n" "$name"
	exit 0
fi

uz_cleanup() {
    rm "$FIFO_UEBERZUG" 2>/dev/null
    pkill -P $$ >/dev/null
}

start_ueberzug() {
	mkfifo "$FIFO_UEBERZUG"
	tail --follow "$FIFO_UEBERZUG" \
	| ueberzug layer --silent --parser json > /dev/null 2>&1 &
}

HELP="Usage:
Type in the prompt to filter the current list of files. Regular expressions are \
allowed.

At exit (pressing q) CliFM will change to the last directory visited with FZF or \
open the last accepted file (Enter). Press Esc to cancel and exit.

Keybindings:
Left: go to parent directory
Right or Enter: cd into hovered directory or open hovered file and exit
Home/end: go to first/last file in the files list
Shift-up/down: Move one line up/down in the preview window
Alt-up/down: Move to the beginning/end in the preview window"

fcd() {
	if [ "$#" -ne 0 ]; then
		cd "$@" || return
	fi

	dir_color="$(dircolors -c | grep -o "[\':]di=....." | cut -d';' -f2)"
	[ -z "$dir_color" ] && dir_color="34"

	# Make sure FZF interface won't be messed up when running on an 8 bit terminal
	# emulator
	if [ "$COLORS" -eq 256 ]; then
		BORDERS="--border=left"
	else
		BORDERS="--no-unicode"
	fi

	[ -n "$CLIFM" ] && fzf_prompt="CliFM "

	# Keep FZF running until the user presses Esc or q
	while true; do
		lsd=$(printf "\033[0;%sm..\n" "$dir_color"; \
		ls -Ap --group-directories-first --color=always --indicator-style=none)
		file="$(printf "%s\n" "$lsd" | fzf \
			--color="bg+:236,gutter:236,fg+:reverse,pointer:6,prompt:6,marker:2:bold,spinner:6:bold" \
			--bind "right:accept,left:first+accept" \
			--bind "home:first,end:last" \
			--bind "ctrl-h:preview(printf %s \"$HELP\")" \
			--bind "alt-p:toggle-preview" \
			--bind "shift-up:preview-up" \
			--bind "shift-down:preview-down" \
			--bind "alt-up:preview-page-up" \
			--bind "alt-down:preview-page-down" \
			--bind "esc:execute(rm $TMP)+abort" \
			--bind "q:abort" \
			--ansi --prompt="${fzf_prompt}> " --reverse --no-clear \
			--no-info --keep-right --multi --header="Press 'Ctrl+h' for help
$PWD" --marker="+" --preview-window=:wrap "$BORDERS" \
			--preview "printf \"\033[2J\"; $BFG_FILE {}")"

		# If FZF returned no file, exit
		[ ${#file} -eq 0 ] && return 0
		# If the returned file is a directory, just cd into it. Otherwise, open
		# it via OPENER
		if [ -d "${PWD}/$file" ]; then
			[ -n "$CLIFM" ] && printf "cd %s" "${PWD}/$file" > "$TMP"
			cd "$file" || return
		elif [ -f "${PWD}/$file" ]; then
			if [ "$OPENER" = "clifm" ]; then
				clifm --open "${PWD}/$file"
			else
				"$OPENER" "${PWD}/$file"
			fi
		fi
	done
}

main() {

	if ! [ "$(which fzf 2>/dev/null)" ]; then
		printf "CliFM: fzf: Command not found" >&2
		exit 1
	fi

	# This is the previewer script, similar to Ranger's scope.sh
	BFG_FILE="${XDG_CONFIG_HOME:-$HOME/.config}/clifm/plugins/BFG.sh"

	UEBERZUG_OK=0
	COLORS="$(tput colors)"
	OPENER="clifm"
	DIR_PREVIEWER="tree" # ls is another alternative

	# We check here, at startup, for available applications so that we don't need
	# to do it once and again each time a file is hovered
	if [ -n "$DISPLAY" ]; then
		if [ "$(which ueberzug 2>/dev/null)" ]; then
			UEBERZUG_OK=1
			IMG_VIEWER="ueberzug"
		elif [ "$(which viu 2>/dev/null)" ]; then
			IMG_VIEWER="viu"
		elif [ "$(which catimg 2>/dev/null)" ]; then
			IMG_VIEWER="catimg"
		elif [ "$(which img2txt 2>/dev/null)" ]; then
			IMG_VIEWER="img2txt"
		fi
		# Überzug is not run directly, but through a function
		[ "$IMG_VIEWER" = "ueberzug" ] && IMG_VIEWER="uz_image"
	fi

	if [ "$(which atool 2>/dev/null)" ]; then
		ARCHIVER_CMD="atool"
		ARCHIVER_OPTS="-l"
	elif [ "$(which bsdtar 2>/dev/null)" ]; then
		ARCHIVER_CMD="bsdtar"
		ARCHIVER_OPTS="-tvf"
	elif [ "$(which tar 2>/dev/null)" ]; then
		ARCHIVER_CMD="tar"
		ARCHIVER_OPTS="-tvf"
	fi

	if [ "$(which w3m 2>/dev/null)" ]; then
		BROWSER="w3m"
	elif [ "$(which linx 2>/dev/null)" ]; then
		BROWSER="linx"
	elif [ "$(which elinks 2>/dev/null)" ]; then
		BROWSER="elinks"
	elif [ "$(which pandoc 2>/dev/null)" ]; then
		PANDOC_OK=1
	fi

	if [ "$(which ffplay 2>/dev/null)" ]; then
		FFPLAY_OK=1
	elif [ "$(which mplayer 2>/dev/null)" ]; then
		MPLAYER_OK=1
	elif [ "$(which mpv 2>/dev/null)" ]; then
		MPV_OK=1
	fi

	if [ "$(which exiftool 2>/dev/null)" ]; then
		EXIFTOOL_OK=1
	fi
	if [ "$(which mediainfo 2>/dev/null)" ]; then
		MEDIAINFO_OK=1
	fi

	if [ "$(which pdftoppm 2>/dev/null)" ]; then
		PDFTOPPM_OK=1
	elif [ "$(which pdftotext 2>/dev/null)" ]; then
		PDFTOTEXT_OK=1
	elif [ "$(which mutool 2>/dev/null)" ]; then
		MUTOOL_OK=1
	fi

	if [ "$(which libreoffice 2>/dev/null)" ]; then
		LIBREOFFICE_OK=1
	elif [ "$(which catdoc 2>/dev/null)" ]; then
		CATDOC_OK=1
	fi

	if [ "$(which bat 2>/dev/null)" ]; then
		BAT_OK=1
	elif [ "$(which highlight 2>/dev/null)" ]; then
		HIGHLIGHT_OK=1
	elif [ "$(which pygmentize 2>/dev/null)" ]; then
		PYGMENTIZE_OK=1
	fi

	if [ "$(which ddjvu 2>/dev/null)" ]; then
		DDJVU_OK=1
	elif [ "$(which djvutxt 2>/dev/null)" ]; then
		DJVUTXT_OK=1
	fi

	if [ "$(which fontpreview 2>/dev/null)" ]; then
		FONTPREVIEW_OK=1
	elif [ "$(which fontimage 2>/dev/null)" ]; then
		FONTIMAGE_OK=1
	fi

	[ "$(which ffmpegthumbnailer 2>/dev/null)" ] && FFMPEGTHUMB_OK=1
	[ "$(which convert 2>/dev/null)" ] && CONVERT_OK=1
	[ "$(which glow 2>/dev/null)" ] && GLOW_OK=1
	[ "$(which epub-thumbnailer 2>/dev/null)" ] && EPUBTHUMB_OK=1

	if [ "$UEBERZUG_OK" -eq 1 ]; then
		export FIFO_UEBERZUG="${XDG_CACHE_HOME:-$HOME/.cache}/clifm/ueberzug-${PPID}"
		trap uz_cleanup EXIT
		start_ueberzug
	fi

	TMP="$(mktemp /tmp/clifm.XXXXXX)"

	# These variables are exported to the environment so that the previewer script:
	# BFG.sh, executed from within FZF, can catch them all.
	export IMG_VIEWER ARCHIVER_CMD ARCHIVER_OPTS BROWSER \
	FFPLAY_OK MEDIAINFO_OK PDFTOPPM_OK FFMPEGTHUMB_OK CONVERT_OK \
	LIBREOFFICE_OK HIGHLIGHT_OK FONTPREVIEW_OK BAT_OK EPUBTHUMB_OK DDJVU_OK \
	MPLAYER_OK EXIFTOOL_OK GLOW_OK MPV_OK PDFTOTEXT_OK CATDOC_OK \
	MUTOOL_OK PANDOC_OK COLORS PYGMENTIZE_OK DJVUTXT_OK UEBERZUG_OK \
	HELP BFG_FILE DIR_PREVIEWER FONTIMAGE_OK

	fcd "$@"

	if [ -z "$DISPLAY" ]; then
		clear
	else
		tput rmcup
	fi

	[ -n "$CLIFM" ] && cat "$TMP" 2>/dev/null > "$CLIFM_BUS"
	rm -f -- "$TMP" 2>/dev/null
}

main "$@"
exit 0
