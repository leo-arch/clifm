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
	printf "column: command not found\n" >&2
	exit 127
fi

_pager="${PAGER:-less}"

if ! type "$_pager" >/dev/null 2>&1; then
	printf "%s: command not found\n" "$_pager" >&2
	exit 127
fi

if [ "$_pager" = "less" ]; then
	_pager_opts="-ncs -PCliFM --tilde"
fi

TMP_DIR="${TMPDIR:-/tmp}"
TMP_FILE=$(mktemp "$TMP_DIR/clifm_pager.XXXXXX")
clifm -y --no-color --no-columns --list-and-quit --no-clear-screen "$PWD" > "$TMP_FILE"

# shellcheck disable=SC2086
while read -r entry; do
	printf "%s\n" "$entry"
done < "$TMP_FILE" | column | "$_pager" $_pager_opts

rm -- "$TMP_FILE"

exit 0
