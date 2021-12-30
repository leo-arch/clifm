#!/bin/sh

# CliFM plugin to navigate the jump database via fzf/Rofi
# Written by L. Abramovich
# Lincese GPL3

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
	printf "Navigate CLiFM jump database via FZF or Rofi. Press Enter to cd into the selected directory\n"
	printf "Usage: %s\n" "$name"
	exit 0
fi

if type fzf > /dev/null 2>&1; then
	finder="fzf"

elif type rofi > /dev/null 2>&1; then
	finder="rofi"
else
	printf "CliFM: No finder found. Install either FZF or Rofi\n" >&2
	exit 1
fi

FILE="${XDG_CONFIG_HOME:-${HOME}/.config}/clifm/profiles/$CLIFM_PROFILE/jump.cfm"

if ! [ -f "$FILE" ]; then
	exit 1
fi

if [ "$finder" = "fzf" ]; then
	get_helper_file
	# shellcheck source=/dev/null
	. "$helper_file"

	# shellcheck disable=SC2154
	path="$(cut -d ":" -f4 "$FILE" | grep -v ^"@" |\
fzf --reverse --height "$fzf_height" \
--bind "tab:accept" --info=inline \
--color="$(get_fzf_colors)" \
--prompt="$fzf_prompt")"
else
	path="$(cut -d ":" -f4 "$FILE" | grep -v ^"@" | rofi -dmenu -p CliFM)"
fi

if [ -n "$path" ]; then
	printf "%s\n" "$path" > "$CLIFM_BUS"
fi

exit 0
