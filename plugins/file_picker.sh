#!/bin/sh

# Files picker plugin for Clifm
# Written by L. Abramovich
# License: GPL3
#
# Description: Select files via Clifm and, at exit, write selected files to
# either stdout, if no file is specified as first parameter, or to the
# specified file otherwise.
#
# Dependencies: mktemp, cat, rm

[ -z "$CLIFM_TERM"] && CLIFM_TERM="xterm"

SEL_FILE="$1"
[ -z "$SEL_FILE" ] && SEL_FILE=$(mktemp "${TMPDIR:-/tmp}/clifm_sel.XXXXXX")

$CLIFM_TERM -e "$HOME"/build/git_repos/clifm/src/clifm --sel-file="$SEL_FILE"

! [ -f "$SEL_FILE" ] && exit 0

if [ -z "$1" ]; then
	cat -- "$SEL_FILE"
	rm -- "$SEL_FILE"
fi

exit 0
