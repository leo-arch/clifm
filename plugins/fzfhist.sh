#!/bin/sh

# Command history plugin via FZF for CliFM
# Written by L. Abramovich
# License GPL2+

if [ -n "$1" ] && ([ "$1" = "--help" ] || [ "$1" = "help" ]); then
	name="$(basename "$0")"
	printf "Navigate and execute CliFM commands history via FZF\n"
	printf "Usage: %s\n" "$name"
	exit 0
fi

if ! [ "$(which fzf 2>/dev/null)" ]; then
	printf "CliFM: fzf: Command not found" >&2
	exit 1
fi

FILE="${XDG_CONFIG_HOME:=$HOME/.config}/clifm/profiles/$CLIFM_PROFILE/history.cfm"

fzf --prompt="CliFM > " \
--reverse --height 15 \
--bind "tab:accept" \
--color="fg+:reverse,bg+:236,prompt:6,pointer:2,marker:2:bold,spinner:6:bold" \
 < "$FILE" > "$CLIFM_BUS"
printf "\n"

exit 0
