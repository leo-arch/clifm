#!/bin/sh

# BFG: This is CliFM's file previewer script. It is called by the fzfnav.sh
# plugin for each hovered file. Variables for applications are exported by fzfnav
# to prevent this script from checking the same applications once and again

# Written by L. Abramovich
# License: GPL3

# Previewing dependencies (optional)
# atool or bsdtar or tar: archives
# convert (imagemagick), and ueberzug (recommended) or viu or catimg or img2txt: images
# fontpreview or fontimage (fontforge): fonts
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

	if [ "$USE_SCOPE" = 1 ] && [ -x "$SCOPE_FILE" ]; then
		calculate_position
		"$SCOPE_FILE" "$PWD/$entry" "$X" "$Y" "$PREVIEWDIR" "True"
		return
	fi

	[ "$UEBERZUG_OK" = 1 ] && uz_clear

	# Symlink
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

	# Directory
	if [ -d "$entry" ]; then
		path="$(printf "%s" "$(pwd)/$entry" | sed "s;//;/;")"
		if [ "$DIR_PREVIEWER" = "tree" ]; then
			if [ "$COLORS" = 256 ]; then
				tree -a "$path"
			else
				tree -aA "$path"
			fi
		else
			printf  "%s\n" "$path"
			ls -p --color=always "${path}"
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
	elif [ -f "${PREVIEWDIR}/${entry}.png" ]; then
		[ -z "$IMG_VIEWER" ] && return
		"$IMG_VIEWER" "${PREVIEWDIR}/${entry}.png"
		return
	fi

	# Check a few extensions no correctly handled by file(1)
	ext="${entry##*.}"

	if [ -n "$ext" ] && [ "$ext" != "$entry" ]; then
		ext="$(printf "%s" "${ext}" | tr '[:upper:]' '[:lower:]')"

		case "$ext" in
			# Direct Stream Digital/Transfer (DSDIFF) and wavpack aren not
			# detected by file(1).
			dff|dsf|wv|wvc)
				if [ "$MEDIAINFO_OK" = 1 ]; then
					mediainfo "$PWD/$entry"
				elif [ "$EXIFTOOL_OK" = 1 ]; then
					exiftool "$EXIFTOOL_OK"
				fi
				return
			;;

			# Markdown files
			md)
				if [ "$GLOW_OK" = 1 ]; then
					glow -s dark "$PWD/$entry"
					return
				fi
			;;

			# XZ Compressed
			xz)
				[ -z "$ARCHIVER_CMD" ] && return
				"$ARCHIVER_CMD" "$ARCHIVER_OPTS" "$entry"
				return
			;;

			# Web
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
				return
			;;

			torrent)
				if [ "$(which transmission-show 2>/dev/null)" ]; then
					transmission-show -- "$PWD/$entry"
				fi
				return
			;;

#			stl|off|dxf|scad|csg)
#				if [ "$(which openscad 2>/dev/null)" ]; then
#					openscad "$PWD/$entry"
#				fi
#				return
#			;;
			*) ;;
		esac
	fi

	mimetype="$(file --brief --dereference --mime-type "$entry")"

	case "$mimetype" in

		# Office documents
		*officedocument*|*msword|*ms-excel|text/rtf|*.opendocument.*)

			if [ -n "$IMG_VIEWER" ] && [ "$LIBREOFFICE_OK" = 1 ]; then
				libreoffice --headless --convert-to jpg "$entry" \
				--outdir "$PREVIEWDIR" > /dev/null 2>&1

				mv "$PREVIEWDIR/${entry%.*}.jpg" "$PREVIEWDIR/${entry}.jpg"

				"$IMG_VIEWER" "${PREVIEWDIR}/${entry}.jpg"
				return
			fi

			case "$ext" in
				odt|ods|odp|sxw)
					if [ "$(which odt2txt 2>/dev/null)" ]; then
						odt2txt "$PWD/$entry"
					elif [ "$(which pandoc 2>/dev/null)" ]; then
						pandoc -s -t markdown -- "$PWD/$entry"
					fi
				;;
				xlsx)
					if [ "$(which xlsx2csv) 2>/dev/null" ]; then
						xlsx2csv -- "$PWD/$entry"
					fi
				;;
				xls)
					if [ "$(which xlsx2csv) 2>/dev/null" ]; then
						xls2csv -- "$PWD/$entry"
					fi
				;;
				docx)
					if [ "$(which unzip 2>/dev/null)" ]; then
						unzip -p "$entry" | grep --text '<w:r' | \
						sed 's/<w:p[^<\/]*>/ /g' | sed 's/<[^<]*>//g' | \
						grep -v '^[[:space:]]*$' | sed G
					fi
				;;
				*)
					catdoc "$entry" ;;
			esac
		;;

		# Empty file
		inode/x-empty)
			printf -- "--- \033[0;30;47mEmpty file\033[0m ---"
		;;

		# Web content
		text/html)
			if [ -n "$BROWSER" ]; then
				"$BROWSER" -dump "$entry"
			elif [ "$PANDOC_OK" = 1 ]; then
				pandoc -s -t markdown -- "$entry"
			fi
		;;

		# Plain text files
		text/*|application/x-setupscript|*/xml)
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

		# DjVu
		*/vnd.djvu)
			if [ -n "$IMAGE_VIEWER" ] && [ "$DDJVU_OK" = 1 ]; then
				ddjvu --format=tiff --page=1 "$entry" "$PREVIEWDIR/${entry}.jpg"
				"$IMG_VIEWER" "$PREVIEWDIR/${entry}.jpg"
			elif [ "$DJVUTXT_OK" = 1 ]; then
				djvutxt "$PWD/entry"
			fi
		;;

		# GIF files
		*/gif)
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
		;;

		# Images
		image/*)
			[ -z "$IMG_VIEWER" ] && return
			"$IMG_VIEWER" "$PWD/$entry"
		;;

		# Postscript
		application/postscript)
			! [ "$(which gs 2>/dev/null)" ] && return
			gs -sDEVICE=jpeg -dJPEGQ=100 -dNOPAUSE -dBATCH -dSAFER -r300 \
			-sOutputFile="$PREVIEWDIR/${entry}.jpg" "$entry" > /dev/null 2>&1
			"$IMG_VIEWER" "$PREVIEWDIR/${entry}.jpg"
		;;

		# Epub
		application/epub+zip|application/x-mobipocket-ebook|application/x-fictionbook+xml)
			[ -z "$EPUBTHUMB_OK" ] && return
			epub-thumbnailer "$entry" "$PREVIEWDIR/${entry}.jpg" "$WIDTH" "$HEIGHT"
			"$IMG_VIEWER" "$PREVIEWDIR/${entry}.jpg"
		;;

		# PDF
		*/pdf)
			if [ -n "$IMG_VIEWER" ] && [ "$PDFTOPPM_OK" = 1 ]; then
				pdftoppm -jpeg -f 1 -singlefile "$entry" "$PREVIEWDIR/$entry"
				"$IMG_VIEWER" "${PREVIEWDIR}/${entry}.jpg"
			elif [ "$PDFTOTEXT_OK" = 1 ]; then
				pdftotext -nopgbrk -layout -- "$entry" - | ${PAGER:=less}
			elif [ "$MUTOOL_OK" = 1 ]; then
				mutool draw -F txt -- "$entry"
			fi
		;;

		# Audio
		audio/*)
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

		# Video
		video/*)
			[ -z "$IMG_VIEWER" ] || [ -z "$FFMPEGTHUMB_OK" ] && return
			ffmpegthumbnailer -i "$entry" -o "$PREVIEWDIR/${entry}.jpg" -s 0 -q 5 2>/dev/null
			"$IMG_VIEWER" "$PREVIEWDIR/${entry}.jpg"
		;;

		# Fonts
		font/*|application/font*|application/*opentype)
			[ -z "$IMG_VIEWER" ] && return
			if [ "$FONTPREVIEW_OK" = 1 ]; then
				fontpreview -i "$entry" -o "$PREVIEWDIR/${entry}.jpg"
				"$IMG_VIEWER" "$PREVIEWDIR/${entry}.jpg"
			elif [ "$FONTIMAGE_OK" = 1 ]; then
				png_file="$PREVIEWDIR/${entry}.png"
				if fontimage -o "$png_file" \
						--width "500" --height "290" \
						--pixelsize "45" \
						--fontname \
						--pixelsize "30" \
						--text "  ABCDEFGHIJKLMNOPQRSTUVWXYZ  " \
						--text "  abcdefghijklmnopqrstuvwxyz  " \
						--text "     0123456789.:,;(*!?')  " \
						--text "        ff fl fi ffi ffl  " \
						--text " The quick brown fox jumps over the lazy dog." \
						"$PWD/$entry"  > /dev/null 2>&1;
				then
					"$IMG_VIEWER" "$png_file"
				fi
			fi
		;;

		# Archives
		application/zip|application/gzip|application/x-7z-compressed|\
		application/x-bzip2)
			[ -z "$ARCHIVER_CMD" ] && return
			"$ARCHIVER_CMD" "$ARCHIVER_OPTS" "$entry"
		;;

		# If none of the above, just printf file information
		*)
			if [ "$(file -bL --mime-encoding "$entry")" = "binary" ]; then
				printf -- "--- \e[0;30;47mBinary file\e[0m ---\n"
			fi
			if [ "$EXIFTOOL_OK" = 1 ]; then
				exiftool "$PWD/$entry" 2>/dev/null
			elif [ "$MEDIAINFO_OK" = 1 ]; then
				mediainfo "$PWD/$entry" 2>/dev/null
			else
				file -b "$entry"
			fi
		;;
	esac
}

CACHEDIR="${XDG_CACHE_HOME:-$HOME/.cache}/clifm"
PREVIEWDIR="$CACHEDIR/previews"

! [ -d "$PREVIEWDIR" ] && mkdir -p "$PREVIEWDIR"

# Default size for images
WIDTH=1920
HEIGHT=1080

USE_SCOPE=0
SCOPE_FILE="$HOME/.config/ranger/scope.sh"

USE_PISTOL=0

file_preview "$1"

exit 0
