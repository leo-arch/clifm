#!/bin/sh

# CliFM plugin to navigate the jump database via fzf/Rofi
# Written by L. Abramovich
# Lincese GPL3

if [ -n "$1" ] && { [ "$1" = "--help" ] || [ "$1" = "help" ]; }; then
	name="$(basename "$0")"
	printf "Navigate CLiFM jump database via FZF or Rofi. Press Enter to cd into the selected directory\n"
	printf "Usage: %s\n" "$name"
	exit 0
fi

if [ "$(which fzf 2>/dev/null)" ]; then
	finder="fzf"

elif [ "$(which rofi)" ]; then
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
	path="$(cut -d ":" -f4 "$FILE" | grep -v ^"@" |\
fzf --reverse --height "${CLIFM_FZF_HEIGHT:-80}%" \
--bind "tab:accept" --info=inline \
--color=fg+:reverse,bg+:236,prompt:6,pointer:2,marker:2:bold,spinner:6:bold \
--prompt="CliFM> ")"
else
	path="$(cut -d ":" -f4 "$FILE" | grep -v ^"@" | rofi -dmenu -p CliFM)"
fi

if [ -n "$path" ]; then
	printf "%s\n" "$path" > "$CLIFM_BUS"
fi

exit 0
