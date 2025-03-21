#!/bin/sh

# A PDF text viewer plugin for Clifm
# Dependencies: pdftotext

# Written by L. Abramovich
# License: GPL2+

if [ -z "$1" ] || [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
	name="${CLIFM_PLUGIN_NAME:-$(basename "$0")}"
	printf "Visualize PDF files in the terminal\n"
	printf "\n\x1b[1mUSAGE\x1b[0m\n  %s FILE\n" "$name"
	exit 0
fi

if ! type pdftotext >/dev/null 2>&1; then
	printf "clifm: pdftotext: Command not found\n" >&2
	exit 127
fi

file="$(echo "$1" | sed 's/\\//g')"

if [ "$(head -c4 "$file")" != "%PDF" ]; then
	printf "clifm: Not a PDF file\n" >&2
	exit 1
fi

pdftotext -nopgbrk -layout "$file" - | ${PAGER:=less}

exit 0
