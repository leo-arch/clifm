#!/bin/sh

# CliFM plugin to navigate the jump database via fzf/Rofi
# Written by L. Abramovich
# Lincese GPL3

get_helper_file()
{
	helper_file="${XDG_CONFIG_HOME:-$HOME/.config}/clifm/plugins/plugins-helper"
	if ! [ -f "$helper_file" ]; then
		helper_file="/usr/share/clifm/plugins/plugins-helper"
		if ! [ -f "$helper_file" ]; then
			printf "CliFM: plugins-helper: File not found\n" >&2
			exit 1
		fi
	fi
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
