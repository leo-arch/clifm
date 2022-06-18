#!/bin/sh

# BFG: This is CliFM's file previewer script. It is called by the fzfnav.sh
# plugin for each hovered file. Variables for available applications are
# exported by fzfnav to prevent this script from checking the same
# applications once and again

# Written by L. Abramovich
# License: GPL3

# Previewing dependencies (optional)
# atool or bsdtar or tar: archives
# convert (imagemagick), and ueberzug (recommended) or viu or catimg or img2txt: images
# fontpreview or fontimage (fontforge): fonts
# libreoffice, catdoc, odt2txt, xls2csv, xls2xcsv, pandoc: office documents
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

# Default size for images
WIDTH=1920
HEIGHT=1080


calculate_position() {
	# shellcheck disable=SC2034
	# TERM_LINES and TERM_COLS hold the number of lines and columns
	# of the terminal window respectivelly, while LINES and COLUMNS
	# refer rather to the preview window's size
	read -r TERM_LINES TERM_COLS << EOF
	$(</dev/tty stty size)
EOF

	X=$((TERM_COLS - COLUMNS - 2))

	if [ -z "$fzfheight" ]; then
		if [ -n "$CLIFM_FZF_HEIGHT" ]; then
			fzf_height="$(echo "$CLIFM_FZF_HEIGHT" | cut -d'%' -f1)"
		else
			fzf_height=80
		fi
	else
		fzf_height="$(echo "$fzfheight" | cut -d'%' -f1)"
	fi

	Y=$((TERM_LINES - (fzf_height * TERM_LINES / 100) + 1))
}

uz_image() {
	calculate_position
	printf '{"action": "add", "identifier": "clifm-preview", "x": "%s", "y": "%s", "width": "%s", "height": "%s", "path": "%s"}\n' "$X" "$Y" "$COLUMNS" "$LINES" "$1" > "$FIFO_UEBERZUG"
}

uz_clear() {
	printf '{"action": "remove", "identifier": "clifm-preview"}\n' > "$FIFO_UEBERZUG"
}

kitty_clear() {
	kitty +kitten icat --clear --transfer-mode=stream --silent
}

preview_image() {
	[ -z "$IMG_VIEWER" ] || [ -z "$1" ] && return 1
	img="$1"
	case "$IMG_VIEWER" in
		kitty)
			calculate_position
			kitty +kitten icat --silent --place "$COLUMNS"x"$LINES"@"$X"x"$Y" \
				--transfer-mode=stream --align=left "$img" \
				&& return 0
		;;
		w3m)
			calculate_position
#			WIDTH=200
#			HEIGHT=200
#			X=360
#			Y=15
			W3IMG_PATH="/usr/lib/w3m/w3mimgdisplay"
			# Clear image
			printf '6;%s;%s;%s;%s\n3;' "$X" "$Y" "$WIDTH" "$HEIGHT" | "$W3IMG_PATH"
			printf '0;1;%s;%s;%s;%s;;;;;%s\n4;\n3;' "$X" "$Y" "$WIDTH" "$HEIGHT" \
			"$img" | "$W3IMG_PATH" && return 0
		;;
#		terminology)
#			[ "$TERM_PROGRAM" != "terminology" ] && return 1
#			tycat "$img" && return 0
#		;;
		pixterm) pixterm -s 2 -tc $((COLUMNS - 2)) "$img" && return 0 ;;
		catimg) catimg -H$LINES "$img" && return 0 ;;
		img2txt) img2txt -H$LINES -W$((COLUMNS - 2)) "$img" && return 0 ;;
		chafa) chafa -s "$((COLUMNS - 2))x$LINES" "$img" && return 0 ;;
		viu) viu -h$LINES -w$((COLUMNS - 2)) "$img" && return 0 ;;
		*) "$IMG_VIEWER" "$img" && return 0 ;;
	esac

	return 1
}

file_info() {
	[ "$FALLBACK_INFO" = 0 ] && exit 0

	[ -z "$1" ] && exit 1

	entry="$1"

	if [ "$FILE_OK" = 1 ] && [ "$FILE_HAS_MIME_ENCODING" = 1 ] && [ "$(file -bL --mime-encoding "$entry")" = "binary" ]; then
		printf -- "--- \e[0;30;47mBinary file\e[0m ---\n"
	fi

	if [ "$EXIFTOOL_OK" = 1 ]; then
		exiftool "$PWD/$entry" 2>/dev/null && exit 0
	elif [ "$MEDIAINFO_OK" = 1 ]; then
		mediainfo "$PWD/$entry" 2>/dev/null && exit 0
	elif [ "$FILE_OK" = 1 ]; then
		file -b "$entry" && exit 0
	fi
	exit 1
}

handle_ext() {
# Check a few extensions not correctly handled by file(1)
	entry="$1"

	ext="${entry##*.}"

	if [ -n "$ext" ] && [ "$ext" != "$entry" ]; then
		ext="$(printf "%s" "${ext}" | tr '[:upper:]' '[:lower:]')"

		case "$ext" in
			# Direct Stream Digital/Transfer (DSDIFF) and wavpack aren not
			# detected by file(1).
#			dff|dsf|wv|wvc)
#				file_info "$entry" && exit 0
#			;;

			# Markdown files
			md)
				if [ "$GLOW_OK" = 1 ]; then
					glow -s dark "$PWD/$entry" && exit 0
				elif [ "$MDCAT_OK" = 1 ]; then
					mdcat "$entry" && exit 0
				elif [ "$BAT_OK" = 1 ]; then
					bat "$entry" && exit 0
				elif [ "$CAT_OK" =  1 ]; then
					cat "$entry" && exit 0
				fi
			;;

			# XZ Compressed
			xz)
				if [ -n "$ARCHIVER_CMD" ]; then
					"$ARCHIVER_CMD" "$ARCHIVER_OPTS" "$entry" && exit 0
				fi
			;;

			# Web
			html|xhtml|htm)
				if [ -n "$BROWSER" ]; then
					"$BROWSER" -dump "$entry" && exit 0
				elif [ "$CAT_OK" = 1 ]; then
					cat "$entry" && exit 0
				fi
			;;

			json)
				if [ "$PYTHON_OK" = 1 ]; then
					python -m json.tool -- "$entry" && exit 0
				elif [ "$JQ_OK" = 1 ]; then
					jq --color-output . "$PWD/$entry" && exit 0
				elif [ "$CAT_OK" = 1 ]; then
					cat "$entry" && exit 0
				fi
			;;

			svg)
				[ -z "$IMG_VIEWER" ] && return
				if [ -f "$PREVIEWDIR/${entryhash}.png" ]; then
					preview_image "$PREVIEWDIR/${entryhash}.png" && exit 0
				fi

				[ -z "$CONVERT_OK" ] && return
				convert -background none -size "$WIDTH"x"$HEIGHT" "$entry" \
					"$PREVIEWDIR/${entryhash}.png"
				preview_image "${PREVIEWDIR}/${entryhash}.png" && exit 0
			;;

			torrent)
				if [ "$TRANSMISSION_OK" = 1 ]; then
					transmission-show -- "$PWD/$entry" && exit 0
				fi
			;;

#			stl|off|dxf|scad|csg)
#				if [ "$(which openscad 2>/dev/null)" ]; then
#					openscad "$PWD/$entry"
#				else
#					file_info "$entry"
#				fi
#				exit 1
#			;;
		esac
	fi
}

handle_mime() {

	[ -z "$FILE_OK" ] && return

	entry="$1"

	mimetype="$(file --brief --dereference --mime-type "$entry")"

	case "$mimetype" in

		# Office documents
		*officedocument*|*msword|*ms-excel|text/rtf|*.opendocument.*)

			if [ -n "$IMG_VIEWER" ] && [ -z "$DOCASTEXT" ]; then
				_type="png"
				if [ -f "$PREVIEWDIR/${entryhash}.$_type" ]; then
					preview_image "$PREVIEWDIR/${entryhash}.$_type" && exit 0
				fi

				if [ "$LIBREOFFICE_OK" = 1 ]; then
					libreoffice --headless --convert-to "$_type" "$entry" \
					--outdir "$PREVIEWDIR" > /dev/null 2>&1 && \

					mv "$PREVIEWDIR/${entry%.*}.$_type" "$PREVIEWDIR/${entryhash}.$_type" && \
					preview_image "${PREVIEWDIR}/${entryhash}.$_type" && exit 0
				fi
			fi

			case "$ext" in
				odt|ods|odp|sxw)
					# shellcheck disable=SC2153
					if [ "$ODT2TXT_OK" = 1 ]; then
						odt2txt "$PWD/$entry" && exit 0
					elif [ "$PANDOC_OK" = 1 ]; then
						pandoc -s -t markdown -- "$PWD/$entry" && exit 0
					fi
				;;
				xlsx)
					if [ "$XLSX2CSV_OK" = 1 ]; then
						xlsx2csv -- "$PWD/$entry" && exit 0
					fi
				;;
				xls)
					if [ "$XLSX2CSV_OK" = 1 ]; then
						xls2csv -- "$PWD/$entry" && exit 0
					fi
				;;
				docx)
					if [ "$UNZIP_OK" = 1 ]; then
						unzip -p "$entry" | grep --text '<w:r' | \
						sed 's/<w:p[^<\/]*>/ /g' | sed 's/<[^<]*>//g' | \
						grep -v '^[[:space:]]*$' | sed G && \
						exit 0
					fi
				;;
				*)
					if [ -n "$CATDOC_OK" ]; then
						catdoc "$entry" && exit 0
					fi
				;;
			esac
		;;

		# Empty file
		inode/x-empty)
			printf -- "--- \033[0;30;47mEmpty file\033[0m ---"
		;;

		# Web content
		text/html)
			if [ -n "$BROWSER" ]; then
				"$BROWSER" -dump "$entry" && exit 0
			elif [ "$PANDOC_OK" = 1 ]; then
				pandoc -s -t markdown -- "$entry" && exit 0
			elif [ "$CAT_OK" = 1 ]; then
				cat "$entry" && exit 0
			fi
		;;

		# Plain text files
		text/*|application/x-setupscript|*/xml)
			if [ "$BAT_OK" = 1 ]; then
				bat -pp --color=always "$entry" && exit 0
			elif [ "$HIGHLIGHT_OK" = 1 ]; then
				if [ "$COLORS" = 256 ]; then
					highlight -O xterm256 "$entry" && exit 0
				else
					highlight -O ansi "$entry" && exit 0
				fi
			elif [ "$PYGMENTIZE_OK" = 1 ]; then
				if [ "$COLORS" = 256 ]; then
					pigmentize -f terminal256 "$entry" && exit 0
				else
					pigmentize -f terminal "$entry" && exit 0
				fi
			elif [ "$CAT_OK" = 1 ]; then
				cat "$entry" && exit 0
			fi
		;;

		# DjVu
		*/vnd.djvu)
			if [ -n "$IMAGE_VIEWER" ] && [ "$DDJVU_OK" = 1 ]; then
				if [ -f "$PREVIEWDIR/${entryhash}.jpg" ]; then
					preview_image "$PREVIEWDIR/${entryhash}.jpg" && exit 0
				fi
				ddjvu --format=tiff --page=1 "$entry" "$PREVIEWDIR/${entryhash}.jpg"
				preview_image "$PREVIEWDIR/${entryhash}.jpg" && exit 0
			elif [ "$DJVUTXT_OK" = 1 ]; then
				djvutxt "$PWD/entry" && exit 0
			fi
		;;

		# GIF files
		*/gif)
			[ -z "$IMG_VIEWER" ] && return

			if [ "$ANIMATE_GIFS" = 0 ]; then
				if [ "$IMG_VIEWER" = "kitty" ]; then
					calculate_position
					kitty +kitten icat --silent --place "$COLUMNS"x"$LINES"@"$X"x"$Y" \
						--transfer-mode=stream --align=left --loop=0 "$PWD/$entry" \
						&& exit 0
				fi
				if [ -f "$PREVIEWDIR/${entryhash}.jpg" ]; then
					preview_image "$PREVIEWDIR/${entryhash}.jpg" && exit 0
				fi

				convert -coalesce -resize "$WIDTH"x"$HEIGHT"\> "$entry"[0] \
				"$PREVIEWDIR/$entryhash.jpg"
				preview_image "$PREVIEWDIR/${entryhash}.jpg" && exit 0
				exit 0
			fi

			if [ "$IMG_VIEWER" = "kitty" ] || [ "$IMG_VIEWER" = "catimg" ] \
			|| [ "$IMG_VIEWER" = "chafa" ]; then
				preview_image "$PWD/$entry" && exit 0
			fi

			# Break down the gif into frames and show each frame, one each 0.1 secs
#			printf "\n"
			filename="$(printf "%s" "$entry" | tr ' ' '_')"
			if [ ! -d "$PREVIEWDIR/$entryhash" ]; then
				mkdir -p "$PREVIEWDIR/$entryhash" && \
				convert -coalesce -resize "$WIDTH"x"$HEIGHT"\> "$entry" \
				"$PREVIEWDIR/$entryhash/${filename%.*}.jpg"
			fi
			while true; do
				for frame in $(find "$PREVIEWDIR/$entryhash"/*.jpg 2>/dev/null \
				| sort -V); do
					preview_image "$frame"
					sleep 0.1
				done
			done
			exit 0
		;;

		# Images
		image/*)
			preview_image "$PWD/$entry" && exit 0
		;;

		# Postscript
		application/postscript)
			[ -z "$IMG_VIEWER" ] && return

			if [ -f "$PREVIEWDIR/${entryhash}.jpg" ]; then
				preview_image "$PREVIEWDIR/${entryhash}.jpg" && exit 0
			fi

			if [ "$(which gs 2>/dev/null)" ]; then
				gs -sDEVICE=jpeg -dJPEGQ=100 -dNOPAUSE -dBATCH -dSAFER -r300 \
				-sOutputFile="$PREVIEWDIR/${entryhash}.jpg" "$entry" > /dev/null 2>&1 && \
				preview_image "$PREVIEWDIR/${entryhash}.jpg" && exit 0
			fi
		;;

		# Epub
		application/epub+zip|application/x-mobipocket-ebook|application/x-fictionbook+xml)
			[ -z "$IMG_VIEWER" ] && return

			if [ -f "$PREVIEWDIR/${entryhash}.jpg" ]; then
				preview_image "$PREVIEWDIR/${entryhash}.jpg" && exit 0
			fi

			if [ -n "$EPUBTHUMB_OK" ]; then
				epub-thumbnailer "$entry" "$PREVIEWDIR/${entryhash}.jpg" \
				"$WIDTH" "$HEIGHT" && \
				preview_image "$PREVIEWDIR/${entryhash}.jpg" && exit 0
			fi
		;;

		# PDF
		*/pdf)
			[ -z "$IMG_VIEWER" ] && return

			if [ -f "$PREVIEWDIR/${entryhash}.jpg" ] && \
			[ "$PDFTOPPM_OK" = 1 ]; then
				preview_image "$PREVIEWDIR/${entryhash}.jpg" && exit 0
			fi

			if [ "$PDFTOPPM_OK" = 1 ]; then
				pdftoppm -jpeg -f 1 -singlefile "$entry" "$PREVIEWDIR/$entryhash" && \
				preview_image "$PREVIEWDIR/${entryhash}.jpg" && exit 0
			elif [ "$PDFTOTEXT_OK" = 1 ]; then
				pdftotext -nopgbrk -layout -- "$entry" - | ${PAGER:=less} && exit 0
			elif [ "$MUTOOL_OK" = 1 ]; then
				mutool draw -F txt -- "$entry" && exit 0
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
				mpv --no-video --quiet "$entry" &
			fi

			if [ "$FALLBACK_INFO" = 1 ]; then
				[ "$MEDIAINFO_OK" = 1 ] && mediainfo "$entry"
				[ "$EXIFTOOL_OK" = 1 ] && exiftool "$entry"
			fi

			exit 0
		;;

		# Video
		video/*)
			[ -z "$IMG_VIEWER" ] && return

			if [ -f "$PREVIEWDIR/${entryhash}.jpg" ]; then
				preview_image "$PREVIEWDIR/${entryhash}.jpg" && exit 0
			fi

			if [ "$FFMPEGTHUMB_OK" = 1 ]; then
				ffmpegthumbnailer -i "$entry" -o "$PREVIEWDIR/${entryhash}.jpg" -s 0 \
				-q 5 2>/dev/null && \
				preview_image "$PREVIEWDIR/${entryhash}.jpg" && exit 0
			fi
		;;

		# Fonts
		font/*|application/font*|application/*opentype)
			[ -z "$IMG_VIEWER" ] && return

			if [ -f "${PREVIEWDIR}/${entryhash}.jpg" ]; then
				preview_image "${PREVIEWDIR}/${entryhash}.jpg" && exit 0
			elif [ -f "${PREVIEWDIR}/${entryhash}.png" ]; then
				preview_image "${PREVIEWDIR}/${entryhash}.png" && exit 0
			fi

			if [ "$FONTPREVIEW_OK" = 1 ]; then
				fontpreview -i "$entry" -o "$PREVIEWDIR/${entryhash}.jpg" && \
				preview_image "$PREVIEWDIR/${entryhash}.jpg" && exit 0
			elif [ "$FONTIMAGE_OK" = 1 ]; then
				png_file="$PREVIEWDIR/${entryhash}.png"
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
						"$PWD/$entry" > /dev/null 2>&1;
				then
					preview_image "$png_file" && exit 0
				fi
			fi
		;;

		# Archives
		application/zip|application/gzip|application/x-7z-compressed|\
		application/x-bzip2)
			if [ -n "$ARCHIVER_CMD" ]; then
				"$ARCHIVER_CMD" "$ARCHIVER_OPTS" "$entry" && exit 0
			fi
		;;
	esac
}

main() {
	entry="$1"

	[ "$entry" = ".." ] && return

	if [ "$USE_PISTOL" = 1 ]; then
		pistol "$PWD/$entry" && exit 0
		exit 1
	fi

	if [ "$USE_SCOPE" = 1 ] && [ -x "$SCOPE_FILE" ]; then
		calculate_position
		"$SCOPE_FILE" "$PWD/$entry" "$X" "$Y" "$PREVIEWDIR" "True" && exit 0
		exit 1
	fi

	[ "$UEBERZUG_OK" = 1 ] && uz_clear
	[ "$IMG_VIEWER" = "kitty" ] && kitty_clear

	# Symlink
	if [ -L "$entry" ]; then
		real_path="$(realpath "$entry")"
		printf "%s \033[1;36m->\033[0m " "$entry"
		if [ -e "$real_path" ]; then
			if [ "$POSIX_LS" = 0 ]; then
				ls -d --color=always "$real_path" && exit 0
			else
				ls -d "$real_path" && exit 0
			fi
		else
			printf "%s\n\033[1;37mBroken link\033[0m" "$real_path" && exit 0
		fi
		exit 1
	fi

	# Directory
	if [ -d "$entry" ]; then
		path="$(printf "%s" "$(pwd)/$entry" | sed "s;//;/;")"
		if [ "$DIR_CMD" = "tree" ]; then
			if [ "$COLORS" = 256 ]; then
				tree -a "$path" && exit 0
			else
				tree -aA "$path" && exit 0
			fi
		else
			printf  "%s\n" "$path"
			if [ "$DIR_CMD" = "lsd" ]; then
				lsd -A --group-dirs=first --color=always "$path" && exit 0
			elif [ "$DIR_CMD" = "lsd-tree" ]; then
				lsd -A --group-dirs=first --depth=1 --tree --color=always "$path" && exit 0
			elif [ "$DIR_CMD" = "exa" ]; then
				exa -G --group-directories-first --color=always "$path" && exit 0
			elif [ "$DIR_CMD" = "exa-tree" ]; then
				exa -G --group-directories-first --color=always --tree --level=1 "$path" && exit 0
			else
				if [ "$POSIX_LS" = 0 ]; then
					ls -Ap --color=always --indicator-style=none "$path" && exit 0
				else
					ls -Ap "$path" && exit 0
				fi
			fi
		fi
		exit 1
	fi

	printf "\n"

	# Use hashes instead of file names for cached files
	if type md5sum > /dev/null 2>&1; then
		entryhash="$(md5sum "$entry" | cut -d' ' -f1)"
	elif type md5 > /dev/null 2>&1; then
		entryhash="$(md5 -q "$entry")"
	else
		printf "clifm: No hashing application found. Either md5sum or md5 \
are required\n" >&2
		exit 1
	fi

	# Do not generate previews of previews
	[ "$PWD" = "${PREVIEWDIR}" ] && entry="${entry%.*}"

	handle_ext "$entry"
	handle_mime "$entry"
	file_info "$entry" # Fallback to file information
}

#calculate_position
main "$@"
exit 1
