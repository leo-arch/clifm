#!/bin/sh

# A PDF text viewer plugin for CLiFM
# Written by L. Abramovich

SUCCESS=0
ERROR=1

if [ -z "$1" ] || [ "$1" = "--help" ] || [ "$1" = "help" ]; then
	name="$(basename "$0")"
	printf "View PDF files in the terminal\n"
	printf "Usage: %s FILE\n" "$name"
	exit $SUCCESS
fi

if ! [ "$(which pdftotext 2>/dev/null)" ]; then
	printf "CliFM: pdftotext: Command not found\n" >&2
	exit $ERROR
fi

if [ "$(head -c4 "$1")" != "%PDF" ]; then
	printf "CLiFM: Not a PDF file\n" >&2
	exit $ERROR
fi

pdftotext -nopgbrk -layout "$1" - | ${PAGER:=less}

exit $SUCCESS
