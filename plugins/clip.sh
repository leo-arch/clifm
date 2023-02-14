#!/bin/sh

# Clipboard plugin for clifm
# Author: L. Abramovich
# License: GPL3

# Dependencies:
#  xclip | xsel (Linux)
#  wl-copy/wl-paste (Wayland)
#  clipboard (Haiku)
#  clip (Cygwin)
#  pbcopy/pbget (MacOS)
#  termux-clipboard-set/termux-clipboard-get (Termux)
#  cb (cross-platform: https://github.com/Slackadays/Clipboard)

# Use the 'e,export' parameter to send selected files to the system clipboard (new line separated list),
# and the 'i,import' parameter to import files (new line separated list) in the clipboard to the Selection box


get_from_clip() {
	if type cb >/dev/null 2>&1; then
		cb | cat 2>/dev/null # slackadays implementation
	elif [ -n "$WAYLAND_DISPLAY" ]; then
		wl-paste 2>/dev/null
	elif type xclip >/dev/null 2>&1; then
		xclip -sel clip -o 2>/dev/null
	elif type xsel >/dev/null 2>&1; then
		xsel -bo 2>/dev/null
	elif type pbpaste >/dev/null 2>&1; then # MacOS
		pbpaste 2>/dev/null
	elif type termux-clipboard-get >/dev/null 2>&1; then
		termux-clipboard-get 2>/dev/null
	elif type clipboard >/dev/null 2>&1; then # Haiku
		clipboard --print 2>/dev/null
	elif [ -r /dev/clipboard ]; then # Cygwin
		cat /dev/clipboard
	fi
}

send_to_clip() {
	if type cb >/dev/null 2>&1; then
		cb --copy < "$CLIFM_SELFILE" # slackadays implementation
	elif [ -n "$WAYLAND_DISPLAY" ]; then
		wl-copy < "$CLIFM_SELFILE"
	elif type xclip >/dev/null 2>&1; then
		xclip -sel clip "$CLIFM_SELFILE"
	elif type xsel >/dev/null 2>&1; then
		xsel -bi < "$CLIFM_SELFILE"
	elif type pbcopy >/dev/null 2>&1; then # MacOS
		pbcopy < "$CLIFM_SELFILE"
	elif type termux-clipboard-set >/dev/null 2>&1; then
		termux-clipboard-set < "$CLIFM_SELFILE"
	elif type clipboard >/dev/null 2>&1; then # Haiku
		clipboard --stdin < "$CLIFM_SELFILE"
	elif type clip >/dev/null 2>&1; then # Cygwin
		clip < "$CLIFM_SELFILE"
	fi
}

print_help() {
	name="${CLIFM_PLUGIN_NAME:-$(basename "$0")}"
	printf "Interact with the system clipboard\n"
	printf "\n\x1b[1mUSAGE\x1b[0m\n  %s [e, export] [i, import]\n" "$name"
	printf "\nUse the 'e, export' parameter to send selected files to the system clipboard (as a newline separated list of entries), and the 'i, import' parameter to import files (as a newline separated list) from the system clipboard to CliFM's Selection Box\n"
}

if [ -z "$1" ] || [ "$1" = "--help" ] || [ "$1" = "-h" ] ; then
	print_help
	exit 0
fi

case $1 in
	"e"|"export")
		if ! [ -f "$CLIFM_SELFILE" ]; then
			printf "clifm: No selected files\n" >&2
			exit 0
		fi

		send_to_clip
	;;

	"i"|"import")
		shift 1

		files=$(get_from_clip)
		if [ -z "$files" ]; then
			printf "clifm: Empty clipboard\n" 2>&1
			exit 0
		fi

		for file in $files; do
			if ! [ -e "$file" ]; then
				printf "clifm: %s: No such file or directory\n" "$file" 2>&1
				exit 1;
			fi
		done

		printf "%s" "$files" >> "$CLIFM_SELFILE"
	;;

	*)
		print_help
		exit 1
	;;
esac

exit 0
