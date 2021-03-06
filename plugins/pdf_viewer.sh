#!/bin/bash

# A PDF text viewer plugin for CLiFM
# Written by L. Abramovich

SUCCESS=0
ERROR=1

if [[ -z "$1" ]]; then
	echo "CLIFM: Missing argument" >&2
	exit $ERROR
fi

if ! [[ $(type -P pdftotext) ]]; then
	echo "CliFM: pdftotext: Command not found" >&2
	exit $ERROR
fi

if [[ "$(head -c4 "$1")" != "%PDF" ]]; then
	echo "CLiFM: Not a PDF file" >&2
	exit $ERROR
fi

pdftotext -nopgbrk -layout "$1" - | ${PAGER:=less}

exit $SUCCESS
