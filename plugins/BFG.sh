#!/bin/sh

# BFG: This is CliFM's file previewer script. It is called by the fzfnav.sh
# plugin for each hovered file. Variables for applications are exported by fzfnav
# to prevent this script from checking the same applications once and again

# Written by L. Abramovich
# License: GPL3

# Previewing dependencies (optional)
# atool or bsdtar or tar: archives
# convert (imagemagick), and ueberzug (recommended) or viu or catimg: images
# fontpreview: fonts
# libreoffice, catdoc, odt2txt, pandoc: office documents
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

calculate_position() {
	# shellcheck disable=SC2034
	read -r TERM_LINES TERM_COLS << EOF
	$(</dev/tty stty size)
EOF

     X=$((TERM_COLS - COLUMNS - 2))
     Y=1
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

	if [ "$USE_PISTOL" = 1 ]; then
		pistol "$PWD/$entry"
		return
	fi

	if [ "$USE_SCOPE" = 1 ] && [ -f "$HOME/.config/ranger/scope.sh" ]; then
		calculate_position
		"$SCOPE_FILE" "$PWD/$entry" "$X" "$Y" "$PREVIEWDIR" "True"
		return
	fi

	[ "$UEBERZUG_OK" = 1 ] && uz_clear

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
		if [ "$COLORS" = 256 ]; then
			tree -a "$path"
		else
			tree -aA "$path"
		fi
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
				if [ "$MEDIAINFO_OK" = 1 ]; then
					mediainfo "$PWD/$entry"
				elif [ "$EXIFTOOL_OK" = 1 ]; then
					exiftool "$EXIFTOOL_OK"
				fi
			;;
			md)
				if [ "$GLOW_OK" = 1 ]; then
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
			elif [ "$ext" = "odt" ] || [ "$ext" = "ods" ] || [ "$ext" = "odp" ] || [ "$ext" = "sxw" ]; then
				if [ "$(which odt2txt 2>/dev/null)" ]; then
					odt2txt "$PWD/$entry"
				elif [ "$(which pandoc 2>/dev/null)" ]; then
					pandoc -s -t markdown -- "$PWD/$entry"
				fi
			elif [ "$ext" != "docx" ] && [ "$(which catdoc 2>/dev/null)" ]; then
				catdoc "$entry"
			elif [ "$(which unzip 2>/dev/null)" ]; then
				unzip -p "$entry" | grep --text '<w:r' | sed 's/<w:p[^<\/]*>/ /g' \
				| sed 's/<[^<]*>//g' | grep -v '^[[:space:]]*$' | sed G
			fi
		;;

		"inode/x-empty")
			printf -- "--- \033[0;30;47mEmpty file\033[0m ---" ;;

		"text/html")
			if [ -n "$BROWSER" ]; then
				"$BROWSER" "$entry"
			elif [ "$PANDOC_OK" = 1 ]; then
				pandoc -s -t markdown -- "$entry"
			fi
		;;

		"text/"*|"application/x-setupscript"|*"/xml")
			if [ "$BAT_OK" = 1 ]; then
				bat -pp --color=always "$entry"
			elif [ "$HIGHLIGHT_OK" = 1 ]; then
				if [ "$COLORS" = 256 ]; then
					highlight -O xterm256 "$entry"
				else
					highlight -O ansi "$entry"
				fi
			elif [ "$PYGMENTIZE_OK" = 1 ]; then
				if [ "$COLORS" = 256 ]; then
					pigmentize -f terminal256 "$entry"
				else
					pigmentize -f terminal "$entry"
				fi
			else
				cat "$entry"
			fi
		;;

		*"/vnd.djvu")
			if [ -n "$IMAGE_VIEWER" ] && [ "$DDJVU_OK" = 1 ]; then
				ddjvu --format=tiff --page=1 "$entry" "$PREVIEWDIR/${entry}.jpg"
				"$IMG_VIEWER" "$PREVIEWDIR/${entry}.jpg"
			elif [ "$DJVUTXT" = 1 ]; then
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
			if [ -n "$IMG_VIEWER" ] && [ "$PDFTOPPM_OK" = 1 ]; then
				pdftoppm -jpeg -f 1 -singlefile "$entry" "${PREVIEWDIR}/$entry"
				"$IMG_VIEWER" "${PREVIEWDIR}/${entry}.jpg"
			elif [ "$PDFTOTEXT_OK" = 1 ]; then
				pdftotext -nopgbrk -layout -- "$entry" - | ${PAGER:=less}
			elif [ "$MUTOOL_OK" = 1 ]; then
				mutool draw -F txt -- "$entry"
			fi
		;;

		"audio"/*)
#			[ -z "$IMG_VIEWER" ] || ! [ "$(which ffmpeg 2>/dev/null)" ] && return

#			ffmpeg -i "$entry" -lavfi \
#			showspectrumpic=s="$WIDTH"x"$HEIGHT":mode=separate:legend=disabled \
#			"${PREVIEWDIR}/${entry}.jpg" > /dev/null 2>&1

#			uz_image "${PREVIEWDIR}/${entry}.jpg" ;;

			if [ "$FFPLAY_OK" = 1 ]; then
				ffplay -nodisp -autoexit "$entry" &
			elif [ "$MPLAYER_OK" = 1 ]; then
				mplayer "$entry" &
			elif [ "$MPV_OK" = 1 ]; then
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
			if [ "$EXIFTOOL_OK" = 1 ]; then
				exiftool "$PWD/$entry" 2>/dev/null
			elif [ "$MEDIAINFO_OK" = 1 ]; then
				mediainfo "${PWD}/$entry" 2>/dev/null
			else
				file -b "$entry"
			fi
		;;
	esac
}

file_preview "$1"
