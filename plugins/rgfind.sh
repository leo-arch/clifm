#!/bin/sh

# Plugin to search files by content via Ripgrep and FZF for CliFM

# Written by L. Abramovich
# License: GPL3

get_helper_file()
{
	helper_file="${XDG_CONFIG_HOME:-$HOME/.config}/clifm/plugins/.plugins-helper"
	if ! [ -f "$helper_file" ]; then
		helper_file="/usr/share/clifm/plugins/.plugins-helper"
		if ! [ -f "$helper_file" ]; then
			printf "CliFM: .plugins-helper: File not found\n" >&2
			exit 1
		fi
	fi
}

if [ -n "$1" ] && { [ "$1" = "--help" ] || [ "$1" = "help" ]; }; then
	name="$(basename "$0")"
	printf "Search files by content via Ripgrep and FZF\n"
	printf "Usage: %s STRING|REGEXP\n" "$name"
	exit 0
fi

if ! type rg > /dev/null 2>&1; then
	printf "CliFM: rg: Command not found\nInstall ripgrep to use this plugin\n" >&2
	exit 1
fi

if ! type fzf > /dev/null 2>&1; then
	printf "CliFM: fzf: Command not found\nInstall fzf to use this plugin\n" >&2
	exit 1
fi

get_helper_file
. "$helper_file"

fzf_colors="$(get_fzf_colors)"
rg_colors="ansi"
[ "$fzf_colors" = "bw" ] && rg_colors="never"

while true; do
	file="$(rg --color="$rg_colors" --hidden --heading --line-number \
		--trim -- "$1" 2>/dev/null | \
		fzf --ansi --reverse --prompt="$fzf_prompt" \
		--height="$fzf_height" --color="$fzf_colors" \
		--no-clear --bind "right:accept" --no-info \
		--header="Select a file name and press Enter or Right to open it")"
	[ -z "$file" ] && break
	clifm --open "$PWD/$(printf "%s" "$file" | cut -d: -f1)"
done

# Erase the FZF window
_lines="${LINES:-100}"
printf "\033[%dM" "$_lines"

exit 0
