#!/bin/sh

# A PDF text viewer plugin for CLiFM
# Written by L. Abramovich

SUCCESS=0
ERROR=1

if [ -z "$1" ]; then
	printf "CLIFM: Missing argument\n" >&2
	exit $ERROR
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
