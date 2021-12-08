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
# TAB: Select files
# Ctrl-s: Confirm selection: send selected files to CliFM Selbox
# Shift+up/down: Move one line up/down in the preview window
# Alt+up/down: Move to the beginning/end in the preview window
# At exit (pressing q) CliFM will change to the last directory visited
# with FZF or open the last accepted file (Enter).
# Press Esc to cancel and exit.

if [ -n "$1" ] && { [ "$1" = "--help" ] || [ "$1" = "help" ]; }; then
	name="$(basename "$0")"
	printf "Navigate/preview files via FZF\n"
	printf "Usage: %s\n" "$name"
	exit 0
fi

uz_cleanup() {
    rm "$FIFO_UEBERZUG" 2>/dev/null
    pkill -P $$ >/dev/null
}

get_bfg_cfg_file() {
	FILE="${XDG_CONFIG_HOME:-$HOME/.config}/clifm/plugins/BFG.cfg"

	if [ -z "$FILE" ] || ! [ -f "$FILE" ]; then
		FILE=""
		if [ -n "$XDG_DATA_DIRS" ]; then
			dirs="$(echo "$XDG_DATA_DIRS" | sed 's/:/ /g')"
			for dir in $dirs; do
				if [ -f "$dir/clifm/plugins/BFG.cfg" ]; then
					FILE="$dir/clifm/plugins/BFG.cfg"
					break
				fi
			done
		fi

		if [ -z "$FILE" ]; then
			if [ -f /usr/share/clifm/plugins/BFG.cfg ]; then
				FILE="/usr/share/clifm/plugins/BFG.cfg"
			elif [ -f /usr/local/share/clifm/plugins/BFG.cfg ]; then
				FILE="/usr/local/share/clifm/plugins/BFG.cfg"
			elif [ -f /boot/system/data/clifm/plugins/BFG.cfg ]; then
				FILE="/boot/system/data/clifm/plugins/BFG.cfg"
			elif [ -f /boot/system/non-packaged/data/clifm/plugins/BFG.cfg ]; then
				FILE="/boot/system/non-packaged/data/clifm/plugins/BFG.cfg"
			fi
		fi
	fi

	[ -n "$FILE" ] && printf "%s\n" "$FILE"
}

start_ueberzug() {
	mkfifo "$FIFO_UEBERZUG"
	tail --follow "$FIFO_UEBERZUG" \
	| ueberzug layer --silent --parser json > /dev/null 2>&1 &
}

HELP="Usage:
Type in the prompt to filter the current list of files. Regular expressions are \
allowed.

At exit (Ctrl-q) CliFM will change to the last directory visited or \
open the last accepted file (Enter). Press Esc to cancel and exit.

Keybindings:
Left: go to parent directory
Right or Enter: cd into hovered directory or open hovered file and exit
Home/end: go to first/last file in the files list
TAB: Select files
Ctrl-s: Confirm selection (send them to CliFM Selection Box)
Shift-up/down: Move one line up/down in the preview window
Alt-up/down: Move to the beginning/end in the preview window"

fcd() {
	if [ "$#" -ne 0 ]; then
		cd "$@" || return
	fi

	if type dircolors > /dev/null 2>&1; then
		dir_color="$(dircolors -c | grep -o "[\':]di=....." | cut -d';' -f2)"
	fi
	[ -z "$dir_color" ] && dir_color="34"

	# Make sure FZF interface won't be messed up when running on an 8 bit terminal
	# emulator
	if [ "$COLORS" -eq 256 ]; then
		BORDERS="--border=left"
	else
		BORDERS="--no-unicode"
	fi

	[ -n "$CLIFM" ] && fzf_prompt="CliFM "

#--color="bg+:236,gutter:236,fg+:reverse,pointer:6,prompt:6,marker:2:bold,spinner:6:bold" \

	# Keep FZF running until the user presses Esc or q
	while true; do
		lsd=$(printf "\033[0;%sm..\n" "$dir_color"; $ls_cmd)
		file="$(printf "%s\n" "$lsd" | fzf --height='80%' \
			--color="bg+:236,gutter:236,fg+:reverse,pointer:6,prompt:6,marker:2:bold,spinner:6:bold" \
			--bind "ctrl-s:execute(touch $TMP_SEL)+accept" \
			--bind "right:accept,left:first+accept" \
			--bind "insert:clear-query" \
			--bind "home:first,end:last" \
			--bind "alt-h:preview(printf %s \"$HELP\")" \
			--bind "alt-p:toggle-preview" \
			--bind "shift-up:preview-up" \
			--bind "shift-down:preview-down" \
			--bind "alt-up:preview-page-up" \
			--bind "alt-down:preview-page-down" \
			--bind "esc:execute(rm $TMP)+abort" \
			--bind "ctrl-q:abort" \
			--ansi --prompt="${fzf_prompt}> " --reverse --no-clear \
			--no-info --keep-right --multi --header="Press 'Alt-h' for help
$PWD
$FZF_HEADER" --marker="*" --preview-window=:wrap "$BORDERS" \
			--preview "printf \"\033[2J\"; $BFG_FILE {}")"

		# If FZF returned no file, exit
		[ ${#file} -eq 0 ] && return 0

		# If we have selected files, send them to CliFM Selbox
		if [ -f "$TMP_SEL" ]; then
			echo "$file" > "$TMP_SEL"
			while read -r line; do
				if ! grep -q "$PWD/$line" "$CLIFM_SELFILE"; then
					printf "%s/%s\n" "$PWD" "$line" >> "$CLIFM_SELFILE"
					c=$((c+1))
				fi
			done < "$TMP_SEL"
			rm -rf -- "$TMP_SEL"
			export FZF_HEADER="$c selected file(s)"
			continue
		fi

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

	if ! type fzf > /dev/null 2>&1; then
		printf "CliFM: fzf: Command not found\n" >&2
		exit 1
	fi

	# The variables below are exported to the environment so that the
	# previewer script, BFG.sh, executed from within FZF, can make use of
	# them. Here we define which application should be used for different
	# file types. The implementation for each application is defined in the
	# BFG file.

	export TMP_SEL="/tmp/fzfnav.sel"
	rm -rf -- "$TMP_SEL"
	BFG_CFG_FILE="$(get_bfg_cfg_file)"
	if [ -z "$BFG_CFG_FILE" ]; then
		printf "CliFM: BFG.cfg: No such file or directory\n" >&2
		exit 1
	fi

			#################################################
			#	1. GET VALUES FROM THE CONFIGURATION FILE	#
			#################################################

	PREV_IMGS=1
	PLAY_MUSIC=1
	export ANIMATE_GIFS=1
	export FALLBACK_INFO=1

	while read -r LINE; do
		[ -z "$LINE" ] || [ "$(printf "%s" "$LINE" | cut -c1)" = "#" ] \
		&& continue

		option="$(printf "%s" "$LINE" | cut -d= -f1)"
		value="$(printf "%s" "$LINE" | cut -d= -f2 | tr -d "\"" )"

		case $option in
			# CHECK GENERAL OPTIONS
			LS)
				if [ "${value:-posix}" = posix ]; then
					export ls_cmd="ls -Ap"
					export POSIX_LS=1
				else
					export ls_cmd="ls -Ap --group-directories-first --color=always --indicator-style=none"
					export POSIX_LS=0
				fi ;;
			BFG_FILE)
				if [ -z "$value" ]; then
					BFG_FILE=""
				else
					export BFG_FILE="$value"
				fi ;;
			CACHEDIR)
				if [ -z "$value" ]; then
					CACHEDIR=""
				else
					export CACHEDIR="$value"
				fi ;;
			PREVIEWDIR)
				if [ -z "$value" ]; then
					PREVIEDIR=""
				else
					export PREVIEDIR="$value"
				fi ;;
			OPENER)
				if [ -z "$value" ]; then
					OPENER=""
				else
					export OPENER="$value"
				fi ;;
			USE_SCOPE)
				[ "$value" = 1 ] && export USE_SCOPE=1 ;;
			SCOPE_FILE)
				if [ -z "$value" ]; then
					SCOPE_FILE=""
				else
					export OPENER="$value"
				fi ;;
			USE_PISTOL)
				[ "$value" = 1 ] && export USE_PISTOL=1 ;;
			PREVIEW_IMAGES)
				[ "$value" != 1 ] && PREV_IMGS=0 ;;
			PLAY_MUSIC)
				[ "$value" != 1 ] && PLAY_MUSIC=0 ;;
			ANIMATE_GIFS)
				[ "$value" != 1 ] && ANIMATE_GIFS=0 ;;
			FALLBACK_INFO)
				[ "$value" != 1 ] && FALLBACK_INFO=0 ;;

			# CHECK FILE TYPES
			ARCHIVES)
				ARCHIVES="$value"
				case "$value" in
				atool)
					export ARCHIVER_CMD="atool"
					export ARCHIVER_OPTS="-l" ;;
				bsdtar)
					export ARCHIVER_CMD="bsdtar"
					export ARCHIVER_OPTS="-tvf" ;;
				tar)
					export ARCHIVER_CMD="tar"
					export ARCHIVER_OPTS="-tvf" ;;
				none) ;;
				*) ARCHIVES="" ;;
				esac
			;;
			BROWSER)
				case "$value" in
					w3m) export BROWSER="w3m" ;;
					elinks) export BROWSER="elinks" ;;
					linx) export BROWSER="linx" ;;
					cat) export CAT_OK=1 ;;
					none) export BROWSER="true";;
					*) BROWSER="" ;;
				esac
			;;
			DDJVU)
				DDJVU="$value"
				case "$value" in
					ddjvu) export DDJVU_OK=1 ;;
					ddjvutxt) export DDJVU_OK=1 ;;
					none) ;;
					*) DDJVU="";;
				esac
			;;
			DIR)
				case "$value" in
					tree) export DIR_CMD="tree" ;;
					ls) export DIR_CMD="ls" ;;
					exa) export DIR_CMD="exa" ;;
					exa-tree) export DIR_CMD="exa-tree" ;;
					lsd) export DIR_CMD="lsd" ;;
					lsd-tree) export DIR_CMD="lsd-tree" ;;
					none) export DIR_CMD="true" ;;
					*) DIR_CMD="" ;;
				esac
			;;
			DOC)
				DOC="$value"
				case "$value" in
					libreoffice) export LIBREOFFICE_OK=1 ;;
					text) DOC="" && export DOCASTEXT=1 ;;
					none) ;;
					*) DOC="";;
				esac
			;;
			EPUB)
				EPUB="$value"
				case "$value" in
					epub-thumbnailer) export EPUBTHUMB_OK=1 ;;
					none) ;;
					*) EPUB="";;
				esac
			;;
			FILEINFO)
				FILEINFO="$value"
				case "$value" in
					exiftool) export EXIFTOOL_OK=1 ;;
					none) ;;
					*) FILEINFO="";;
				esac
			;;
			FONTS)
				FONTS="$value"
				case "$value" in
					fontpreview) export FONTPREVIEW_OK=1 ;;
					fontimage) export FONTIMAGE_OK=1 ;;
					none) ;;
					*) FONTS="";;
				esac
			;;
			IMG)
				case "$value" in
					ueberzug) export UEBERZUG_OK=1 ;;
					w3m|kitty|viu|catimg|img2txt|chafa|pixterm)
#					kitty|viu|catimg|img2txt|chafa)
						export IMG_VIEWER="$value" ;;
					none) export IMG_VIEWER="true" ;;
					*) IMG_VIEWER="" ;;
				esac
			;;
			JSON)
				JSON="$value"
				case "$value" in
					python) export PYTHON_OK=1 ;;
					jq) export JQ_OK=1 ;;
					cat) export CAT_OK=1 ;;
					none) ;;
					*) JSON="" ;;
				esac
			;;
			MARKDOWN)
				MARKDOWN="$value"
				case "$value" in
					glow) export GLOW_OK=1 ;;
					mdcat) export MDCAT_OK=1 ;;
					cat) export CAT_OK=1 ;;
					none) ;;
					*) MARKDOWN="" ;;
				esac
			;;
			MEDIAINFO)
				MEDIAINFO="$value"
				case "$value" in
					mediainfo) export MEDIAINFO_OK=1 ;;
					none) ;;
					*) MEDIAINFO="" ;;
				esac
			;;
			MUSIC)
				MUSIC="$value"
				case "$value" in
					ffplay) export FFPLAY_OK=1 ;;
					mplayer) export MPLAYER_OK=1 ;;
					mpv) export MPV_OK=1 ;;
					none) ;;
					*) MUSIC="" ;;
				esac
			;;
			PDF)
				PDF="$value"
				case "$value" in
					pdftoppm) export PDFTOPPM_OK=1 ;;
					pdftotext) export PDFTOTEXT_OK=1 ;;
					mutool) export MUTOOL_OK=1 ;;
					none) ;;
					*) PDF="" ;;
				esac
			;;
			TEXT)
				TEXT="$value"
				case "$value" in
					bat) export BAT_OK=1 ;;
					highlight) export HIGHLIGHT_OK=1 ;;
					pygmentize) export PYGMENTIZE_OK=1 ;;
					cat) export CAT_OK=1 ;;
					none) ;;
					*) TEXT="" ;;
				esac
			;;
			VIDEO)
				VIDEO="$value"
				case "$value" in
					ffmpegthumbnailer) export FFMPEGTHUMBN_OK=1 ;;
					none) ;;
					*) VIDEO="" ;;
				esac
			;;
		esac
	done < "$BFG_CFG_FILE"

	export COLORS
	COLORS="$(tput colors)"

	if [ -z "$ls_cmd" ]; then
		export ls_cmd="ls -Ap --group-directories-first --color=always --indicator-style=none"
	fi

	# This is the previewer script, similar to Ranger's scope.sh
	if [ -z "$BFG_FILE" ]; then
		export BFG_FILE
		BFG_FILE="${XDG_CONFIG_HOME:-$HOME/.config}/clifm/plugins/BFG.sh"
		if ! [ -f "$BFG_FILE" ]; then
			BFG_FILE="${BFG_CFG_FILE%.*}.sh"
			if ! [ -f "$BFG_FILE" ]; then
				printf "CliFM: BFG.sh: No such file or directory\n" >&2
				exit 1
			fi
		fi
	fi

	[ -z "$OPENER" ] && export OPENER="clifm"

	[ -z "$PREVIEWDIR" ] && export PREVIEWDIR="${XDG_CACHE_HOME:-$HOME/.cache}/clifm/previews"

	! [ -d "$PREVIEWDIR" ] && mkdir -p "$PREVIEWDIR"

	if [ "$USE_SCOPE" = 1 ]; then
		[ -z "$SCOPE_FILE" ] && SCOPE_FILE="${XDG_CONFIG_HOME:-$HOME/.config}/ranger/scope.sh"
	fi

	# If some value was not set in the config file, check for available
	# applications

		###############################################
		#			2. CHECK INSTALLED APPS			  #
		###############################################

	# We check here, at startup, for available applications so that we
	# don't need to do it once and again each time a file is hovered

	# Directories

	if [ -z "$DIR_CMD" ]; then
		if type lsd > /dev/null 2>&1; then
			export DIR_CMD="lsd-tree"
		elif type exa > /dev/null 2>&1; then
			export DIR_CMD="exa-tree"
		elif type tree > /dev/null 2>&1; then
			export DIR_CMD="tree"
		else
			export DIR_CMD="ls"
		fi
	fi

	# Images
	if [ "$PREV_IMGS" = 1 ] && [ -z "$IMG_VIEWER" ] && \
	[ -n "$DISPLAY" ]; then
		if type ueberzug > /dev/null 2>&1; then
			export UEBERZUG_OK=1
		elif [ "$TERM" = "xterm-kitty" ]; then
			export IMG_VIEWER="kitty"
		elif type pixterm > /dev/null 2>&1; then
			export IMG_VIEWER="pixterm"
		elif type viu > /dev/null 2>&1; then
			export IMG_VIEWER="viu"
		elif type catimg > /dev/null 2>&1; then
			export IMG_VIEWER="catimg"
		elif type chafa > /dev/null 2>&1; then
			export IMG_VIEWER="chafa"
		elif type img2txt > /dev/null 2>&1; then
			export IMG_VIEWER="img2txt"
		fi
	fi

	# Überzug is not run directly, but through a function
	if [ "$PREV_IMGS" = 1 ] && [ "$UEBERZUG_OK" = 1 ]; then
		export IMG_VIEWER="uz_image"
	fi

	# Archives
	if [ -z "$ARCHIVES" ]; then
		if type atool > /dev/null 2>&1; then
			export ARCHIVER_CMD="atool"
			export ARCHIVER_OPTS="-l"
		elif type bsdtar > /dev/null 2>&1; then
			export ARCHIVER_CMD="bsdtar"
			export ARCHIVER_OPTS="-tvf"
		elif type tar > /dev/null 2>&1; then
			export ARCHIVER_CMD="tar"
			export ARCHIVER_OPTS="-tvf"
		fi
	fi

	# Web
	if [ -z "$BROWSER" ]; then
		if type w3m > /dev/null 2>&1; then
			export BROWSER="w3m"
		elif type linx > /dev/null 2>&1; then
			export BROWSER="linx"
		elif type elinks > /dev/null 2>&1; then
			export BROWSER="elinks"
		fi
	fi

	# Music
	if [ "$PLAY_MUSIC" = 1 ] && [ -z "$MUSIC" ]; then
		if type ffplay > /dev/null 2>&1; then
			export FFPLAY_OK=1
		elif type mplayer > /dev/null 2>&1; then
			export MPLAYER_OK=1
		elif type mpv > /dev/null 2>&1; then
			export MPV_OK=1
		fi
	fi

	# Video
	if [ -z "$VIDEO" ]; then
		if type ffmpegthumbnailer > /dev/null 2>&1; then
			export FFMPEGTHUMB_OK=1
		fi
	fi

	# File information
	if [ -z "$FILEINFO" ]; then
		if type exiftool > /dev/null 2>&1; then
			export EXIFTOOL_CMD=1
		else
			export FILE_OK=1
		fi
	fi

	if [ -z "$MEDIAINFO" ]; then
		if type mediainfo > /dev/null 2>&1; then
			export MEDIAINFO_OK=1
		else
			export FILE_OK=1
		fi
	fi

	# PDF
	if [ -z "$PDF" ]; then
		if type pdftoppm > /dev/null 2>&1; then
			export PDFTOPPM_OK=1
		elif type pdftotext > /dev/null 2>&1; then
			export PDFTOTEXT_OK=1
		elif type mutool > /dev/null 2>&1; then
			export MUTOOL_CMD=1
		fi
	fi

	# Office documents
	if [ -z "$DOC" ]; then
		if [ -z "$DOCASTEXT" ] && type libreoffice > /dev/null 2>&1; then
			export LIBREOFFICE_OK=1
		else
			type catdoc > /dev/null 2>&1 && export CATDOC_OK=1
			type odt2txt > /dev/null 2>&1 && export ODT2TXT_OK=1
			type xlsx2csv > /dev/null 2>&1 && export XLSX2CSV_OK=1
			type xls2csv > /dev/null 2>&1 && export XLS2CSV_OK=1
			type unzip > /dev/null 2>&1 && export UNZIP_OK=1
		fi
	fi

	type pandoc > /dev/null 2>&1 && export PANDOC_OK=1

	# Syntax highlighting
	if [ -z "$TEXT" ]; then
		if type bat > /dev/null 2>&1; then
			export BAT_OK=1
		elif type highlight > /dev/null 2>&1; then
			export HIGHLIGHT_OK=1
		elif type pygmentize > /dev/null 2>&1; then
			export PYGMENTIZE_OK=1
		else
			export CAT_OK=1
		fi
	fi

	if [ -z "$JSON" ]; then
		if type python > /dev/null 2>&1; then
			export PYTHON_OK=1
		elif type jq > /dev/null 2>&1; then
			export JQ_OK=1
		else
			export CAT_OK=1
		fi
	fi

	# Ddjvu
	if [ -z "$DDJVU" ]; then
		if type ddjvu > /dev/null 2>&1; then
			export DDJVU_OK=1
		elif type djvutxt > /dev/null 2>&1; then
			export DDJVUTXT_OK=1
		fi
	fi

	# Fonts
	if [ -z "$FONTS" ]; then
		if type fontpreview > /dev/null 2>&1; then
			export FONTPREVIEW_OK=1
		elif type fontimage > /dev/null 2>&1; then
			export FONTIMAGE_OK=1
		fi
	fi

	# Markdown
	if [ -z "$MARKDOWN" ]; then
		if type glow > /dev/null 2>&1; then
			export GLOW_OK=1
		elif type mdcat > /dev/null 2>&1; then
			export MDCAT_OK=1
		fi
	fi

	# Epub

	if [ -z "$EPUB" ]; then
		if type epub-thumbnailer > /dev/null 2>&1; then
			export EPUBTHUMB_OK=1
		fi
	fi

	# Torrent
	if type transmission-show > /dev/null 2>&1; then
		export TRANSMISSION_OK=1
	fi

	# Used to convert some file types to images
	type convert > /dev/null 2>&1 && export CONVERT_OK=1

	# Make sure we have file, use dto get files MIME type
	type file > /dev/null 2>&1 && export FILE_OK=1

	if [ "$UEBERZUG_OK" = 1 ] ; then
		CACHEDIR="${XDG_CACHE_HOME:-$HOME/.cache}/clifm"
		! [ -d "$CACHEDIR" ] && mkdir -p "$CACHEDIR"
		export FIFO_UEBERZUG="$CACHEDIR/ueberzug-${PPID}"
		trap uz_cleanup EXIT
		start_ueberzug
	fi

	TMP="$(mktemp /tmp/clifm.XXXXXX)"

				#####################################
				#	 3. RUN FZF, WHICH CALLS BFG	#
				#####################################

	fcd "$@"

#	if [ -z "$DISPLAY" ]; then
#		clear
#	else
#		tput clear
#	fi

	[ -n "$CLIFM" ] && cat "$TMP" 2>/dev/null > "$CLIFM_BUS"
	rm -f -- "$TMP" 2>/dev/null
}

main "$@"

# Reset terminal settings
printf "\033c"

exit 0
