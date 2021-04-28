#!/bin/sh

# Plugin to search files by content via Ripgrep and FZF for CliFM

# Written by L. Abramovich
# License: GPL3

if ! [ "$(which rg 2>/dev/null)" ]; then
	printf "CliFM: rg: Command not found\nInstall ripgrep to use this plugin\n" >&2
	exit 1
fi

if ! [ "$(which fzf 2>/dev/null)" ]; then
	printf "CliFM: fzf: Command not found\nInstall fzf to use this plugin\n" >&2
	exit 1
fi

if [ -z "$1" ] || [ "$1" = "--help" ]; then
	printf "Usage: %s STRING\nRegular expressions are supported\n" \
		   "$(basename "$0")"
	exit 0
fi

while true; do
	file="$(rg --color=ansi --hidden --heading --line-number \
			--trim -- "$1" 2>/dev/null | \
			fzf --ansi --reverse --prompt="CliFM > " \
			--no-clear --bind "right:accept" --no-info \
			--header="Select a file and press Enter or Right to open it")"
	[ -z "$file" ] && break
	clifm --open "$PWD/$(printf "%s" "$file" | cut -d: -f1)"
done

if [ -z "$DISPLAY" ]; then
	clear
else
	tput rmcup
fi

exit 0
