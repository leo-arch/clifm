#!/bin/sh

# Clipboard plugin for CliFM
# Written by L. Abramovich
# License: GPL3
# Dependencies: xclip

# Use the 's' parameter to send files to the X primary clipboard, and
# the 'i' parameter to import files in the clipboard to the Selection
# box
# Examples:
# clip s file1 file2 ... (or sel)
# clip i

# When importing files from the clipboard, files should be absolute
# paths.

if [ -z "$1" ] || [ "$1" = "--help" ] || [ "$1" = "-h" ] ; then
	name="${CLIFM_PLUGIN_NAME:-$(basename "$0")}"
	printf "Interact with the primary clipboard via xclip\n"
	printf "\nUsage: %s [s, i] FILE... n\n" "$name"
	printf "\nUse the 's' parameter to send files to the X primary clipboard, and the 'i' parameter to import files in the clipboard to CliFM's Selection box"
	exit 0
fi

if ! [ "$(which xclip 2>/dev/null)" ]; then
	printf "clifm: xclip: Command not found" >&2
	exit 127
fi

case $1 in
	's')
		shift 1
		# Send all files to xclip with no new line char

		# Clipboard values = primary, secondary, clipboard
		clipboard="primary"
		printf "%s" "$@" | xclip -selection "$clipboard"
	;;

	'i')
		# Replace non-escaped spaces by new line char
		xclip -o | sed 's/\([^\\]\) /\1\n/g' >> "$CLIFM_SELFILE"
	;;

	*)
		printf "Usage: clip [s, i] FILE(s)\n" >&2
		exit 1
	;;
esac

exit 0
