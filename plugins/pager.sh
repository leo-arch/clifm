#!/bin/sh

# Description: Print the current list of files through a pager (less)
# Dependencies: less, tput
# Author: L. Abramovich
# License: GPL3

if [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
	name="${CLIFM_PLUGIN_NAME:-$(basename "$0")}"
	printf "List the current list of files through a pager (less)\n"
	printf "\n\x1b[1mUSAGE\x1b[0m\n  %s\n" "$name"
	exit 0
fi

if ! type less >/dev/null 2>&1; then
	printf "clifm: less: command not found\n" >&2
	exit 127
fi

if ! type tput >/dev/null 2>&1; then
	printf "clifm: tput: command not found\n" >&2
	exit 127
fi

if [ "$CLIFM_LONG_VIEW" = "1" ]; then
	clifm_opts="--ls -l --no-clear-screen"
else
	clifm_opts="--ls --no-clear-screen"
fi

# shellcheck disable=SC2086
CLIFM_COLUMNS="$(tput cols)" CLIFM_LINES="$(tput lines)" clifm $clifm_opts "$PWD" | less -rncs -P"LESS (clifm)\:" --tilde

exit 0
