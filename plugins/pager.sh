#!/bin/sh

# Description: Print the current list of files through a pager (PAGER or less)
# Dependencies: less, column
# Author: L. Abramovich
# License: GPL3

if [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
	name="${CLIFM_PLUGIN_NAME:-$(dirname "$0")}"
	printf "List the current list of files through a pager (PAGER or less)
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

_pager="${PAGER:-less}"
if [ "$_pager" = "less" ]; then
	_pager_opts="-ncs --tilde"
fi

eln=1

# shellcheck disable=SC2086
for entry in *; do
	printf "%d %s\n" "$eln" "$entry"
	eln=$((eln+1))
done | column | "$_pager" $_pager_opts

exit 0
