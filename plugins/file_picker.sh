#!/bin/sh

# Files picker plugin for Clifm
# Written by L. Abramovich
# License: GPL3
#
# Description: Select files via Clifm and, at exit, write selected files to
# either stdout, if no file is specified as first parameter, or to the
# specified file otherwise.
#
# Usage example: ls -ld $(file_picker.sh)
#
# Dependencies: mktemp, cat, rm

if [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
	name="${CLIFM_PLUGIN_NAME:-$(basename "$0")}"
	printf "\x1b[1mUSAGE\x1b[0m:\n  %s [FILE]\n\n" "$name" >&2
	printf "Select files via Clifm, writing selected files to STDOUT, or to FILE if specified\n" >&2
	exit 0
fi

[ -z "$CLIFM_TERM" ] && CLIFM_TERM="xterm"

SEL_FILE="$1"
[ -z "$SEL_FILE" ] && SEL_FILE=$(mktemp "${TMPDIR:-/tmp}/clifm_sel.XXXXXX")

$CLIFM_TERM -e clifm --sel-file="$SEL_FILE"

! [ -f "$SEL_FILE" ] && exit 0

if [ -z "$1" ]; then
	cat -- "$SEL_FILE"
	rm -- "$SEL_FILE"
fi

exit 0
