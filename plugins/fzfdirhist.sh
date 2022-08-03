#!/bin/sh

# Directory history plugin via FZF for CliFM
# Written by L. Abramovich
# License GPL2+

if [ -n "$1" ] && { [ "$1" = "--help" ] || [ "$1" = "-h" ]; }; then
	name="${CLIFM_PLUGIN_NAME:-$(basename "$0")}"
	printf "Navigate and execute CliFM directory history via FZF\n"
	printf "Usage: %s\n" "$name"
	exit 0
fi

if ! type fzf > /dev/null 2>&1; then
	printf "clifm: fzf: Command not found" >&2
	exit 127
fi

FILE="${XDG_CONFIG_HOME:=$HOME/.config}/clifm/profiles/$CLIFM_PROFILE/dirhist.clifm"

# Source our plugins helper
if [ -z "$CLIFM_PLUGINS_HELPER" ] || ! [ -f "$CLIFM_PLUGINS_HELPER" ]; then
	printf "clifm: Unable to find plugins-helper file\n" >&2
	exit 1
fi
# shellcheck source=/dev/null
. "$CLIFM_PLUGINS_HELPER"

# shellcheck disable=SC2154
sort -u "$FILE" | fzf --prompt="$fzf_prompt" \
--reverse --height "$fzf_height" \
--bind "tab:accept" --info=inline \
--header "Change to a visited directory" \
--color="$(get_fzf_colors)" > "$CLIFM_BUS"
printf "\n"

exit 0
