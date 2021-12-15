#!/bin/sh

# Directory history plugin via FZF for CliFM
# Written by L. Abramovich
# License GPL2+

if [ -n "$1" ] && { [ "$1" = "--help" ] || [ "$1" = "help" ]; }; then
	name="$(basename "$0")"
	printf "Navigate and execute CliFM directory history via FZF\n"
	printf "Usage: %s\n" "$name"
	exit 0
fi

if ! [ "$(which fzf 2>/dev/null)" ]; then
	printf "CliFM: fzf: Command not found" >&2
	exit 1
fi

FILE="${XDG_CONFIG_HOME:=$HOME/.config}/clifm/profiles/$CLIFM_PROFILE/dirhist.cfm"

if [ -n "$CLIFM_NO_COLOR" ] || [ -n "$NO_COLOR" ]; then
	color_opt="bw"
else
	color_opt="fg+:reverse,bg+:236,prompt:6,pointer:2,marker:2:bold,spinner:6:bold"
fi

sort -u "$FILE" | fzf --prompt="CliFM > " \
--reverse --height "${CLIFM_FZF_HEIGHT:-80}%" \
--bind "tab:accept" --info=inline \
--color="$color_opt" > "$CLIFM_BUS"
printf "\n"

exit 0
