#!/bin/sh

# Plugin to search files by content via Ripgrep and FZF for CliFM

# Written by L. Abramovich
# License: GPL3

# Find the helper file
get_helper_file()
{
	file1="${XDG_CONFIG_HOME:-$HOME/.config}/clifm/plugins/plugins-helper"
	file2="/usr/share/clifm/plugins/plugins-helper"
	file3="/usr/local/share/clifm/plugins/plugins-helper"
	file4="/boot/system/non-packaged/data/clifm/plugins/plugins-helper"
	file5="/boot/system/data/clifm/plugins/plugins-helper"

	[ -f "$file1" ] && helper_file="$file1" && return

	[ -f "$file2" ] && helper_file="$file2" && return

	[ -f "$file3" ] && helper_file="$file3" && return

	[ -f "$file4" ] && helper_file="$file4" && return

	[ -f "$file5" ] && helper_file="$file5" && return

	printf "CliFM: plugins-helper: File not found. Copy this file to ~/.config/clifm/plugins to fix this issue\n" >&2
	exit 1
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
# shellcheck source=/dev/null
. "$helper_file"

# shellcheck disable=SC2154
fzf_colors="$(get_fzf_colors)"
rg_colors="ansi"
[ "$fzf_colors" = "bw" ] && rg_colors="never"

while true; do
	# shellcheck disable=SC2154
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
