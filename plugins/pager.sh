#!/bin/sh

# Description: Print the current list of files on a pager (less(1))
# Dependencies: less, column
# Author: L. Abramovich
# License: GPL3

if [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
	name="${CLIFM_PLUGIN_NAME:-$(dirname "$0")}"
	printf "List the current list of files on a pager (less)
Usage: %s\n" "$name"
	exit 0
fi

if ! type less >/dev/null 2>&1; then
	printf "less: command not found\n" >&2
	exit 127
fi

if ! type column >/dev/null 2>&1; then
	printf "column: command not found\n" >&2
	exit 127
fi

eln=1

for entry in *; do
	printf "%d %s\n" "$eln" "$entry"
	eln=$((eln+1))
done | column | less -ncs -P"CliFM (Press 'h' for help)" --tilde

exit 0
