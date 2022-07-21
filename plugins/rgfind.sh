#!/bin/sh

# Plugin to search files by content via Ripgrep and FZF for CliFM

# Written by L. Abramovich
# License: GPL3

if [ -n "$1" ] && { [ "$1" = "--help" ] || [ "$1" = "-h" ]; }; then
	name="${CLIFM_PLUGIN_NAME:-$(basename "$0")}"
	printf "Search files by content via Ripgrep and FZF\n"
	printf "Usage: %s STRING|REGEXP\n" "$name"
	exit 0
fi

if ! type rg > /dev/null 2>&1; then
	printf "clifm: rg: Command not found\nInstall ripgrep to use this plugin\n" >&2
	exit 127
fi

if ! type fzf > /dev/null 2>&1; then
	printf "clifm: fzf: Command not found\nInstall fzf to use this plugin\n" >&2
	exit 127
fi

# Source our plugins helper
if [ -z "$CLIFM_PLUGINS_HELPER" ] || ! [ -f "$CLIFM_PLUGINS_HELPER" ]; then
	printf "clifm: Unable to find plugins-helper file\n" >&2
	exit 1
fi
# shellcheck source=/dev/null
. "$CLIFM_PLUGINS_HELPER"

# shellcheck disable=SC2154
fzf_colors="$(get_fzf_colors)"
rg_colors="ansi"
[ "$fzf_colors" = "bw" ] && rg_colors="never"

args="$(echo "$@" | sed 's/\\//g')"

while true; do
	# shellcheck disable=SC2154
	file="$(rg --color="$rg_colors" --hidden --heading --line-number \
		--trim --ignore-case -- "$args" 2>/dev/null | \
		fzf --ansi --reverse --prompt="$fzf_prompt" \
		--height="$fzf_height" --color="$fzf_colors" \
		--no-clear --bind "right:accept" --no-unicode \
		--header="Select a file name and press Enter or Right to open it")"
	[ -z "$file" ] && break
	clifm --open "$PWD/$(printf "%s" "$file" | cut -d: -f1)"
done

# Erase the FZF window
_lines="${LINES:-100}"
printf "\033[%dM" "$_lines"

exit 0
