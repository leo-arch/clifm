#!/bin/sh

# FZF navigation/previewing plugin for CliFM
# Written by L. Abramovich
# License: GPL3

# Description: Navigate the filesystem (including file previews) with FZF.
# Previews are shown by just hovering on the file. Each generated preview is
# cached in /tmp/clifm/previews to speed up the previewing process

# Usage:
# Left: go to parent directory
# Right, Enter: cd into hovered directory or open hovered file and exit.
# Home/end: go to first/last file
# Shift+up/down: Move one line up/down in the preview window
# Alt+up/down: Move to the beginning/end in the preview window
# At exit (pressing q) CliFM will change to the last directory visited
# with FZF or open the last accepted file (Enter).
# Press Esc to cancel and exit.

# Preview dependencies (optional)
# atool or bsdtar or tar: archives
# convert (imagemagick), and ueberzug (recommended) or viu or catimg: images
# fontpreview: fonts
# libreoffice: office documents
# pdftoppm: PDF files
# ffmpegthumbnailer: videos
# w3m or linx or elinks: web content

uz_cleanup() {
    rm "$FIFO_UEBERZUG"
    pkill -P $$ >/dev/null
}

calculate_position() {
	read -r TERM_LINES TERM_COLS << HereDoc
	$(</dev/tty stty size)
HereDoc

     X=$((TERM_COLS - COLUMNS - 2))
     Y=1
}

start_ueberzug() {
    mkfifo "$FIFO_UEBERZUG"
	tail --follow "$FIFO_UEBERZUG" \
	| ueberzug layer --silent --parser json > /dev/null 2>&1 &
}

uz_image() {
	calculate_position
	printf '{"action": "add", "identifier": "clifm-preview", "x": "%s", "y": "%s", "width": "%s", "height": "%s", "path": "%s"}\n' "$X" "$Y" "$COLUMNS" "$LINES" "$1" > "$FIFO_UEBERZUG"
}

uz_clear() {
	printf '{"action": "remove", "identifier": "clifm-preview"}\n' > "$FIFO_UEBERZUG"
}

file_preview() {
	entry="$1"

	if [ "$entry" = ".." ]; then
		return
	fi

	uz_clear

	if [ -L "$entry" ]; then
		real_path="$(realpath "$entry")"
		printf "%s \033[1;36m->\033[0m " "$entry"
		if [ -e "$real_path" ]; then
			ls -d --color=always "$real_path"
		else
			printf "%s\n\033[1;37mBroken link\033[0m" "$real_path"
		fi
		return
	fi

	if [ -d "$entry" ]; then
		path="$(printf "%s" "$(pwd)/$entry" | sed "s;//;/;")"
#		printf  "%s\n "$path"
#		ls -p --color=always "${path}"
		tree -a "$path"
		return
	fi

	# Do not generate previews of previews
	[ "$PWD" = "${PREVIEWDIR}" ] && entry="${entry%.*}"

	# Use cached images whenever possible
	if [ -f "${PREVIEWDIR}/${entry}.jpg" ]; then
		[ -z "$IMG_VIEWER" ] && return
		$IMG_VIEWER "${PREVIEWDIR}/${entry}.jpg"
		return
	fi

	mimetype="$(file -bL --mime-type "$entry")"

	case "$mimetype" in

		*"officedocument"*|*"msword"|*"ms-excel"|"text/rtf")
			if [ ! "$(which libreoffice 2>/dev/null)" ]; then
				return
			fi
			libreoffice --headless --convert-to jpg "$entry" \
			--outdir "$PREVIEWDIR/${entry%/*}" > /dev/null 2>&1

			filename="$(printf "%s" "${entry##*/}" | cut -d. -f1)"

			mv "$PREVIEWDIR/${entry%/*}/${filename}.jpg" \
			"$PREVIEWDIR/${entry}.jpg"

#			echo "$PREVIEWDIR/${entry%/*}"
#			rm -r -- "$TMPDIR/lu"*".tmp"

			"$IMG_VIEWER" "${PREVIEWDIR}/${entry}.jpg"
		;;

		"inode/x-empty")
			printf -- "--- \033[0;30;47mEmpty file\033[0m ---" ;;

		"text/html")
			[ -z "$BROWSER" ] && return
			"$BROWSER" "$entry" ;;

		"text/"*)
			if [ "$(which highlight 2>/dev/null)" ]; then
				highlight -O ansi --force "$entry"
			else
				cat "$entry"
			fi
			;;

		*"/gif")
			[ -z "$IMG_VIEWER" ] || ! [ "$(which convert 2>/dev/null)" ] && return
			# Break down the gif into frames and show each frame, one each 0.1 secs
			if [ ! -d "$PREVIEWDIR/$entry" ]; then
				mkdir -p "$PREVIEWDIR/$entry"
				convert -coalesce -resize "$WIDTH"x"$HEIGHT"\> "$entry" \
				"$PREVIEWDIR/$entry/${entry##*/}.jpg"
			fi
			while true; do
				for frame in $(find "$PREVIEWDIR/$entry"/*.jpg | sort -V); do
					"$IMG_VIEWER" "$frame"
					sleep 0.1
				done
			done
			return
		;;

		"image/"*)
			[ -z "$IMG_VIEWER" ] && return
#			convert "$entry" -flatten -resize "$WIDTH"x"$HEIGHT"\> "$PREVIEWDIR/$entry.jpg"
			"$IMG_VIEWER" "${PWD}/$entry" ;;

		*"/pdf")
#			pdftotext -nopgbrk -layout "$entry" - | ${PAGER:=less} ;;
			[ -z "$IMG_VIEWER" ] || ! [ "$(which pdftoppm 2>/dev/null)" ] && return
			pdftoppm -jpeg -f 1 -singlefile "$entry" "${PREVIEWDIR}/$entry"
			"$IMG_VIEWER" "${PREVIEWDIR}/${entry}.jpg" ;;

		"audio"/*)
#			[ -z "$IMG_VIEWER" ] || ! [ "$(which ffmpeg 2>/dev/null)" ] && return

#			ffmpeg -i "$entry" -lavfi \
#			showspectrumpic=s="$WIDTH"x"$HEIGHT":mode=separate:legend=disabled \
#			"${PREVIEWDIR}/${entry}.jpg" > /dev/null 2>&1

#			uz_image "${PREVIEWDIR}/${entry}.jpg" ;;

			[ -z "$IMG_VIEWER" ] || ! [ "$(which mplayer 2>/dev/null)" ] && return
			[ "$(which mediainfo 2>/dev/null)" ] && mediainfo "$entry"
			mplayer -really-quiet "$entry" > /dev/null 2>&1 ;;

		"video/"*)
			[ -z "$IMG_VIEWER" ] || ! [ "$(which ffmpegthumbnailer 2>/dev/null)" ] && return
			ffmpegthumbnailer -i "$entry" -o "${PREVIEWDIR}/${entry}.jpg" -s 0 -q 5
			"$IMG_VIEWER" "${PREVIEWDIR}/${entry}.jpg" ;;

		"font/"*)
			[ -z "$IMG_VIEWER" ] || ! [ "$(which fontpreview 2>/dev/null)" ] && return
			fontpreview -i "$entry" -o "${PREVIEWDIR}/${entry}.jpg"
			"$IMG_VIEWER" "${PREVIEWDIR}/${entry}.jpg" ;;

		"application/zip"|"application/gzip"|"application/x-7z-compressed"|\
		"application/x-bzip2")
			[ -z "$ARCHIVER_CMD" ] && return
			"$ARCHIVER_CMD" "$ARCHIVER_OPTS" "$entry" ;;

		*)
			ext="${entry##*.}"

			if [ -n "$ext" ] && [ "$ext" != "$entry" ]; then
				ext="$(printf "%s" "${ext}" | tr '[:upper:]' '[:lower:]')"
				case "$ext" in
					xz)
						[ -z "$ARCHIVER_CMD" ] && return
						"$ARCHIVER_CMD" "$ARCHIVER_OPTS" "$entry"
					;;
					html|xhtml|htm)
						[ -z "$BROWSER" ] && return
						"$BROWSER" "$entry"
					;;
					*) not_found=1 ;;
				esac

				if [ $not_found = 0 ]; then
					return
				fi
			fi

			if [ "$(file -bL --mime-encoding "$entry")" = "binary" ]; then
				printf -- "--- \e[0;30;47mBinary file\e[0m ---\n"
			fi
			if [ "$(which mediainfo 2>/dev/null)" ]; then
				mediainfo "${PWD}/$entry" 2>/dev/null
			else
				file -b "$entry"
			fi
		;;
	esac
}

help() {
printf "Usage:
Type in the prompt to filter the current list of files. Regular expressions are allowed\n
At exit (pressing q) CliFM will change to the last directory visited
with FZF or open the last accepted file (Enter). Press Esc to cancel and exit\n
Keybindings:
Left: go to parent directory\n
Right, Enter: cd into hovered directory or open hovered file and exit\n
Home/end: go to first/last file in the files list\n
Shift+up/down: Move one line up/down in the preview window\n
Alt+up/down: Move to the beginning/end in the preview window\n"
}

fcd() {

	if [ "$#" != 0 ]; then
		cd "$@" || return
		return
	fi

	dir_color="$(dircolors -c | grep -o "[':]di=....." | cut -d';' -f2)"
	[ -z "$dir_color" ] && dir_color="34"

	while true; do
		lsd=$(printf "\033[0;%sm..\n" "$dir_color"; \
		ls -Ap --group-directories-first --color=always --indicator-style=none)
		dir="$(printf "%s\n" "$lsd" | fzf \
			--color="bg+:236,gutter:236,fg+:reverse,pointer:6,prompt:6,marker:2:bold,spinner:6:bold" \
			--bind "right:accept,left:first+accept" \
			--bind "home:first,end:last" \
			--bind "ctrl-h:preview(help)" \
			--bind "alt-p:toggle-preview" \
			--bind "shift-up:preview-up" \
			--bind "shift-down:preview-down" \
			--bind "alt-up:preview-page-up" \
			--bind "alt-down:preview-page-down" \
			--bind "esc:execute(rm $TMP)+abort" \
			--bind "q:abort" \
			--ansi --prompt="CliFM > " --reverse --no-clear \
			--no-info --keep-right --multi --header="Press 'Ctrl+h' for help
$PWD" --marker="+" --preview-window=:wrap \
			--preview 'printf "\033[2J"; file_preview {}')"

		[ ${#dir} = 0 ] && return 0
		printf "%s" "${PWD}/$dir" > "$TMP"
		cd "$dir" || return
	done
}

main() {

	if ! [ "$(which fzf)" ]; then
		printf "CliFM: fzf: Command not found" >&2
		exit 1
	fi

	UEBERZUG_OK=0
	TMPDIR="/tmp/clifm"
	PREVIEWDIR="${TMPDIR}/previews"
	WIDTH=1920
	HEIGHT=1080

	! [ -d "$PREVIEWDIR" ] && mkdir -p "$PREVIEWDIR"

	if [ "$(which ueberzug 2>/dev/null)" ]; then
		UEBERZUG_OK=1
		IMG_VIEWER="ueberzug"
	elif [ "$(which viu 2>/dev/null)" ]; then
		IMG_VIEWER="viu"
	elif [ "$(which catimg 2>/dev/null)" ]; then
		IMG_VIEWER="catimg"
	fi

	[ "$IMG_VIEWER" = "ueberzug" ] && IMG_VIEWER="uz_image"

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
	fi

	if [ $UEBERZUG_OK = 1 ]; then
		export FIFO_UEBERZUG="$HOME/.cache/clifm/ueberzug-${PPID}"
		trap uz_cleanup EXIT
		start_ueberzug
	fi

	TMP="$(mktemp /tmp/clifm.XXXXXX)"

	export -f file_preview uz_image calculate_position start_ueberzug uz_clear \
	help
	export TMP TMPDIR PREVIEWDIR IMG_VIEWER ARCHIVER_CMD ARCHIVER_OPTS BROWSER \
	WIDTH HEIGHT

	fcd "$@"

	tput rmcup

	if [ -f "$TMP" ]; then
		cat "$TMP" > "$CLIFM_BUS"
		rm -- "$TMP" 2>/dev/null
	fi
}

main "$@"
exit 0
