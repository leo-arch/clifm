#!/bin/sh

# A PDF text viewer plugin for CLiFM
# Written by L. Abramovich
# License: GPL3

if [ -z "$1" ] || [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
	name="${CLIFM_PLUGIN_NAME:-$(basename "$0")}"
	printf "Visualize PDF files in the terminal\n"
	printf "Usage: %s FILE\n" "$name"
	exit 0
fi

if ! type pdftotext >/dev/null 2>&1; then
	printf "clifm: pdftotext: Command not found\n" >&2
	exit 127
fi

if [ "$(head -c4 "$1")" != "%PDF" ]; then
	printf "clifm: Not a PDF file\n" >&2
	exit 1
fi

pdftotext -nopgbrk -layout "$1" - | ${PAGER:=less}

exit 0
