#!/bin/bash

# FZF navigation/previewing plugin for CliFM
# Written by L. Abramovich
# License: GPL3

# Description: Navigate the filesystem (including file previews) with FZF.
# Previews are shown by just hovering on the file. Each generated preview is
# cached in $HOME/.cache/clifm/previews to speed up the previewing process

# Usage:
# Left: go to parent directory
# Right, Enter: cd into hovered directory or open hovered file and exit.
# Home/end: go to first/last file
# Shift+up/down: Move one line up/down in the preview window
# Alt+up/down: Move to the beginning/end in the preview window
# At exit (pressing q) CliFM will change to the last directory visited
# with FZF or open the last accepted file (Enter).
# Press Esc to cancel and exit.

# Previewing dependencies (optional)
# atool or bsdtar or tar: archives
# convert (imagemagick), and ueberzug (recommended) or viu or catimg: images
# fontpreview: fonts
# libreoffice or catdoc: office documents
# pdftoppm or pdftotext or mutool: PDF files
# epub-thumbnailer: epub files
# ddjvu (djvulibre) or djvutxt: DjVu files
# ghostscript: postscript files (ps)
# ffmpegthumbnailer: videos
# ffplay (ffmpeg) or mplayer or mpv: audio
# w3m or linx or elinks: web content
# glow: markdown
# bat or highlight or pygmentize: syntax highlighthing for text files
# python or jq: json
# transmission-cli: torrent files
# exiftool or mediainfo or file: file information

uz_cleanup() {
    rm "$FIFO_UEBERZUG"
    pkill -P $$ >/dev/null
}

calculate_position() {
	read -r TERM_LINES TERM_COLS << EOF
	$(</dev/tty stty size)
EOF

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

	if [ "$USE_SCOPE" -eq 1 ] && [ -f "$HOME/.config/ranger/scope.sh" ]; then
		calculate_position
		"$SCOPE_FILE" "$PWD/$entry" "$X" "$Y" "$PREVIEWDIR" "True"
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

	printf "\n"

	# Do not generate previews of previews
	[ "$PWD" = "${PREVIEWDIR}" ] && entry="${entry%.*}"

	# Use cached images whenever possible
	if [ -f "${PREVIEWDIR}/${entry}.jpg" ]; then
		[ -z "$IMG_VIEWER" ] && return
		"$IMG_VIEWER" "${PREVIEWDIR}/${entry}.jpg"
		return
	fi

	ext="${entry##*.}"

	if [ -n "$ext" ] && [ "$ext" != "$entry" ]; then
		ext="$(printf "%s" "${ext}" | tr '[:upper:]' '[:lower:]')"

		case "$ext" in
			dff|dsf|wv|wvc)
				if [ "$MEDIAINFO_OK" -eq 1 ]; then
					mediainfo "$PWD/$entry"
				elif [ "$EXIFTOOL_OK" -eq 1 ]; then
					exiftool "$EXIFTOOL_OK"
				fi
			;;
			md)
				if [ "$GLOW_OK" -eq 1 ]; then
					glow -s dark "$PWD/$entry"
					return
				fi
			;;
			xz)
				[ -z "$ARCHIVER_CMD" ] && return
				"$ARCHIVER_CMD" "$ARCHIVER_OPTS" "$entry"
				return
			;;
			html|xhtml|htm)
				[ -z "$BROWSER" ] && return
				"$BROWSER" -dump "$entry"
				return
			;;
			json)
				if [ "$(which python 2>/dev/null)" ]; then
					python -m json.tool -- "$entry"
				elif [ "$(which jq 2>/dev/null)" ]; then
					jq --color-output . "$PWD/$entry"
				else
					cat "$entry"
				fi
			;;
			torrent)
				if [ "$(which transmission-show 2>/dev/null)" ]; then
					transmission-show -- "$PWD/$entry"
				fi
			;;
			*) ;;
		esac
	fi

	mimetype="$(file -bL --mime-type "$entry")"

	case "$mimetype" in

		*"officedocument"*|*"msword"|*"ms-excel"|"text/rtf"|*".opendocument."*)

			if [ -n "$IMG_VIEWER" ] && [ "$LIBREOFFICE_OK" -eq 1 ]; then
				libreoffice --headless --convert-to jpg "$entry" \
				--outdir "$PREVIEWDIR" > /dev/null 2>&1

				mv "$PREVIEWDIR/${entry%.*}.jpg" "$PREVIEWDIR/${entry}.jpg"

				"$IMG_VIEWER" "${PREVIEWDIR}/${entry}.jpg"
			elif [ "$ext" = "odt" || [ "$ext" = "ods" ] || [ "$ext" = "odp" ] || [ "$ext" = "sxw" ]; then
				if [ "$(which odt2txt 2>/dev/null)" ]; then
					odt2txt "$PWD/$entry"
				elif [ "$(which pandoc 2>/dev/null)" ]; then
					pandoc -s -t markdown -- "$PWD/$entry"
				fi
			elif [ "$ext" != "docx" ] && [ "$(which catdoc 2>/dev/null)" ]; then
				catdoc "$entry"
			elif [ "$(which unzip)" ]; then
				unzip -p "$entry" | grep --text '<w:r' | sed 's/<w:p[^<\/]*>/ /g' \
				| sed 's/<[^<]*>//g' | grep -v '^[[:space:]]*$' | sed G
			fi
		;;

		"inode/x-empty")
			printf -- "--- \033[0;30;47mEmpty file\033[0m ---" ;;

		"text/html")
			if [ -n "$BROWSER" ]; then
				"$BROWSER" "$entry"
			elif [ "$PANDOC_OK" -eq 1 ]; then
				pandoc -s -t markdown -- "$entry"
			fi
		;;

		"text/"*|"application/x-setupscript"|*"/xml")
			if [ "$BAT_OK" -eq 1 ]; then
				bat -pp --color=always "$entry"
			elif [ "$HIGHLIGHT_OK" -eq 1 ]; then
				if [ "$COLORS" -eq 256 ]; then
					highlight -O xterm256 "$entry"
				else
					highlight -O ansi "$entry"
				fi
			elif [ "$PIGMENTIZE_OK" -eq 1 ]; then
				if [ "$COLORS" -eq 256 ]; then
					pigmentize -f terminal256 "$entry"
				else
					pigmentize -f terminal "$entry"
				fi
			else
				cat "$entry"
			fi
		;;

		*"/vnd.djvu")
			if [ -n "$IMAGE_VIEWER" ] && [ "$DDJVU_OK" -eq 1 ]; then
				ddjvu --format=tiff --page=1 "$entry" "$PREVIEWDIR/${entry}.jpg"
				"$IMG_VIEWER" "$PREVIEWDIR/${entry}.jpg"
			elif [ "$DJVUTXT" -eq 1 ]; then
				djvutxt "$PWD/entry"
			fi
		;;

		*"/gif")
			[ -z "$IMG_VIEWER" ] || [ -z "$CONVERT_OK" ] && return
			# Break down the gif into frames and show each frame, one each 0.1 secs
#			printf "\n"
			filename="$(printf "%s" "$entry" | tr ' ' '_')"
			if [ ! -d "$PREVIEWDIR/$filename" ]; then
				mkdir -p "$PREVIEWDIR/$filename"
				convert -coalesce -resize "$WIDTH"x"$HEIGHT"\> "$entry" \
				"$PREVIEWDIR/$filename/${filename%.*}.jpg"
			fi
			while true; do
				for frame in $(find "$PREVIEWDIR/$filename"/*.jpg 2>/dev/null \
				| sort -V); do
					"$IMG_VIEWER" "$frame"
					sleep 0.1
				done
			done
			return
		;;

		"image/"*)
			[ -z "$IMG_VIEWER" ] && return
#			convert "$entry" -flatten -resize "$WIDTH"x"$HEIGHT"\> "$PREVIEWDIR/$entry.jpg"
			"$IMG_VIEWER" "${PWD}/$entry"
		;;

		"application/postscript")
			! [ "$(which gs 2>/dev/null)" ] && return
			gs -sDEVICE=jpeg -dJPEGQ=100 -dNOPAUSE -dBATCH -dSAFER -r300 \
			-sOutputFile="$PREVIEWDIR/${entry}.jpg" "$entry" > /dev/null 2>&1
			"$IMG_VIEWER" "$PREVIEWDIR/${entry}.jpg"
		;;

		application/epub+zip|application/x-mobipocket-ebook|application/x-fictionbook+xml)
			[ -z "$EPUBTHUMB_OK" ] && return
			epub-thumbnailer "$entry" "${PREVIEWDIR}/${entry}.jpg" "$WIDTH" "$HEIGHT"
			"$IMG_VIEWER" "${PREVIEWDIR}/${entry}.jpg"
		;;

		*"/pdf")
			if [ -n "$IMG_VIEWER" ] && [ "$PDFTOPPM_OK"  -eq 1 ]; then
				pdftoppm -jpeg -f 1 -singlefile "$entry" "${PREVIEWDIR}/$entry"
				"$IMG_VIEWER" "${PREVIEWDIR}/${entry}.jpg"
			elif [ "$PDFTOTEXT_OK" -eq 1 ]; then
				pdftotext -nopgbrk -layout -- "$entry" - | ${PAGER:=less}
			elif [ "$MUTOOL_OK" -eq 1 ]; then
				mutool draw -F txt -- "$entry"
			fi
		;;

		"audio"/*)
#			[ -z "$IMG_VIEWER" ] || ! [ "$(which ffmpeg 2>/dev/null)" ] && return

#			ffmpeg -i "$entry" -lavfi \
#			showspectrumpic=s="$WIDTH"x"$HEIGHT":mode=separate:legend=disabled \
#			"${PREVIEWDIR}/${entry}.jpg" > /dev/null 2>&1

#			uz_image "${PREVIEWDIR}/${entry}.jpg" ;;

			if [ "$FFPLAY_OK" -eq 1 ]; then
				ffplay -nodisp -autoexit "$entry" &
			elif [ "$MPLAYER_OK" -eq 1 ]; then
				mplayer "$entry" &
			elif [ "$MPV_OK" -eq 1 ]; then
				mpv --no-video "$entry" &
			fi
			[ "$MEDIAINFO_OK" -eq 1 ] && mediainfo "$entry"

		;;

		"video/"*)
			[ -z "$IMG_VIEWER" ] || [ -z "$FFMPEGTHUMB_OK" ] && return
			ffmpegthumbnailer -i "$entry" -o "${PREVIEWDIR}/${entry}.jpg" -s 0 -q 5 2>/dev/null
			"$IMG_VIEWER" "${PREVIEWDIR}/${entry}.jpg"
		;;

		"application/font"*|"application/"*"opentype")
			[ -z "$IMG_VIEWER" ] || [ -z "$FONTPREVIEW_OK" ] && return
			fontpreview -i "$entry" -o "${PREVIEWDIR}/${entry}.jpg"
			"$IMG_VIEWER" "${PREVIEWDIR}/${entry}.jpg" ;;

		"application/zip"|"application/gzip"|"application/x-7z-compressed"|\
		"application/x-bzip2")
			[ -z "$ARCHIVER_CMD" ] && return
			"$ARCHIVER_CMD" "$ARCHIVER_OPTS" "$entry"
		;;

		*)
			if [ "$(file -bL --mime-encoding "$entry")" = "binary" ]; then
				printf -- "--- \e[0;30;47mBinary file\e[0m ---\n"
			fi
			if [ "$EXIFTOOL_OK" -eq 1 ]; then
				exiftool "$PWD/$entry" 2>/dev/null
			elif [ "$MEDIAINFO_OK" -eq 1 ]; then
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

	dir_color="$(dircolors -c | grep -o "[\':]di=....." | cut -d';' -f2)"
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
		if [ -d "${PWD}/$dir" ]; then
			printf "cd %s" "${PWD}/$dir" > "$TMP"
			cd "$dir" || return
		elif [ -f "${PWD}/$dir" ]; then
			"$OPENER" "${PWD}/$dir" || return
		fi
	done
}

main() {

	if ! [ "$(which fzf)" ]; then
		printf "CliFM: fzf: Command not found" >&2
		exit 1
	fi

	# Preview files using scope, Ranger's file preview script
	USE_SCOPE=0
	SCOPE_FILE="$HOME/.config/ranger/scope.sh"

	UEBERZUG_OK=0
	CACHEDIR="${XDG_CACHE_HOME:-$HOME/.cache}/clifm"
	PREVIEWDIR="$CACHEDIR/previews"

	# Default size for images
	WIDTH=1920
	HEIGHT=1080

	COLORS="$(tput colors)"

	# In order to use CliFM built-in resource opener we need to exit
	# the script first, so that we cannot use it here
	OPENER="xdg-open" # Add 'open'

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
	elif [ "$(which mediainfo 2>/dev/null)" ]; then
		MEDIAINFO_OK=1
	fi

	if [ "$(which pdftoppm 2>/dev/null)" ]; then
		PDFTOPPM_OK=1
	elif [ "$which pdftotext 2>/dev/null" ]; then
		PDFTOTEXT_OK=1
	elif [ "$which mutool 2>/dev/null" ]; then
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

	[ "$(which ffmpegthumbnailer 2>/dev/null)" ] && FFMPEGTHUMB_OK=1
	[ "$(which convert 2>/dev/null)" ] && CONVERT_OK=1
	[ "$(which glow 2>/dev/null)" ] && GLOW_OK=1
	[ "$(which fontpreview 2>/dev/null)" ] && FONTPREVIEW_OK=1
	[ "$(which epub-thumbnailer 2>/dev/null)" ] && EPUBTHUMB_OK=1

	if [ $UEBERZUG_OK = 1 ]; then
		export FIFO_UEBERZUG="$HOME/.cache/clifm/ueberzug-${PPID}"
		trap uz_cleanup EXIT
		start_ueberzug
	fi

	TMP="$(mktemp /tmp/clifm.XXXXXX)"

	# We need to export functions and variables so that fzf's internal preview
	# function can recognize and run them
	# Exporting functions is not supported by POSIX sh, and that's why
	# we use bash here. However, there should be a POSIX alternative.
	export -f file_preview uz_image calculate_position start_ueberzug uz_clear \
	help
	export TMP CACHEDIR PREVIEWDIR IMG_VIEWER ARCHIVER_CMD ARCHIVER_OPTS BROWSER \
	WIDTH HEIGHT FFPLAY_OK MEDIAINFO_OK PDFTOPPM_OK FFMPEGTHUMB_OK CONVERT_OK \
	LIBREOFFICE_OK HIGHLIGHT_OK FONTPREVIEW_OK BAT_OK EPUBTHUMB_OK DDJVU_OK \
	MPLAYER_OK EXIFTOOL_OK GLOW_OK USE_SCOPE MPV_OK PDFTOTEXT_OK CATDOC_OK \
	MUTOOL_OK PANDOC_OK COLORS PYGMENTIZE_OK SCOPE_FILE

	fcd "$@"

	tput rmcup

	if [ -f "$TMP" ]; then
		cat "$TMP" > "$CLIFM_BUS"
		rm -rf -- "$TMP" 2>/dev/null
	fi
}

main "$@"
exit 0
