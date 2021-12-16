#!/bin/sh

# Command history plugin via FZF for CliFM
# Written by L. Abramovich
# License GPL3

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
	printf "Navigate and execute CliFM commands history via FZF\n"
	printf "Usage: %s\n" "$name"
	exit 0
fi

if ! type fzf > /dev/null 2>&1; then
	printf "CliFM: fzf: Command not found" >&2
	exit 1
fi

FILE="${XDG_CONFIG_HOME:=$HOME/.config}/clifm/profiles/$CLIFM_PROFILE/history.cfm"

get_helper_file
# shellcheck source=/dev/null
. "$helper_file"

# shellcheck disable=SC2154
fzf --prompt="$fzf_prompt" \
--reverse --height 15 --info=inline \
--bind "tab:accept" \
--color="$(get_fzf_colors)" \
 < "$FILE" > "$CLIFM_BUS"
printf "\n"

exit 0
