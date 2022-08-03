#!/bin/sh

# CliFM plugin to navigate the jump database via fzf/Rofi
# Written by L. Abramovich
# Lincese GPL3

if [ -n "$1" ] && { [ "$1" = "--help" ] || [ "$1" = "-h" ]; }; then
	name="${CLIFM_PLUGIN_NAME:-$(basename "$0")}"
	printf "Navigate CliFM's jump database via FZF or Rofi. Press Enter to cd into the selected directory\n"
	printf "Usage: %s\n" "$name"
	exit 0
fi

if type fzf > /dev/null 2>&1; then
	finder="fzf"

elif type rofi > /dev/null 2>&1; then
	finder="rofi"
else
	printf "clifm: No finder found. Install either FZF or Rofi\n" >&2
	exit 1
fi

FILE="${XDG_CONFIG_HOME:-${HOME}/.config}/clifm/profiles/$CLIFM_PROFILE/jump.clifm"

if ! [ -f "$FILE" ]; then
	exit 1
fi

if [ "$finder" = "fzf" ]; then
	# Source our plugins helper
	if [ -z "$CLIFM_PLUGINS_HELPER" ] || ! [ -f "$CLIFM_PLUGINS_HELPER" ]; then
		printf "clifm: Unable to find plugins-helper file\n" >&2
		exit 1
	fi
	# shellcheck source=/dev/null
	. "$CLIFM_PLUGINS_HELPER"

	# shellcheck disable=SC2154
	path="$(cut -d ":" -f4 "$FILE" | grep -v ^"@" |\
fzf --reverse --height "$fzf_height" \
--bind "tab:accept" --info=inline \
--color="$(get_fzf_colors)" \
--prompt="$fzf_prompt" --header "Jump to a directory in the jump database")"
else
	path="$(cut -d ":" -f4 "$FILE" | grep -v ^"@" | rofi -dmenu -p CliFM)"
fi

if [ -n "$path" ]; then
	printf "%s\n" "$path" > "$CLIFM_BUS"
fi

exit 0
