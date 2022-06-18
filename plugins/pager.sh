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

if ! type column >/dev/null 2>&1; then
	printf "clifm: column: command not found\n" >&2
	exit 127
fi

_pager="${PAGER:-less}"

if ! type "$_pager" >/dev/null 2>&1; then
	printf "clifm: %s: command not found\n" "$_pager" >&2
	exit 127
fi

if [ "$_pager" = "less" ]; then
	_pager_opts="-ncs -Pclifm --tilde"
fi

# This produces a columned but uncolored list of files
# shellcheck disable=SC2086
clifm --no-color --no-columns --list-and-quit --no-clear-screen "$PWD" | column | "$_pager" $_pager_opts

# To get a colored but uncolumned list of files, uncomment this line and comment out the one above
#clifm --list-and-quit --no-columns --no-clear-screen "$PWD" | most

# Ideally, we want a columned AND colored list, I know, but this isn't working right now

exit 0
