#!/bin/sh

# Commands history plugin for Clifm
# Dependencies: fzf, tac, sed, awk

# Written by L. Abramovich
# License GPL2+

if [ -n "$1" ] && { [ "$1" = "--help" ] || [ "$1" = "-h" ]; }; then
	name="${CLIFM_PLUGIN_NAME:-$(basename "$0")}"
	printf "Navigate/execute the Clifm commands history via FZF\n"
	printf "\n\x1b[1mUSAGE\x1b[0m\n  %s\n" "$name"
	exit 0
fi

if ! type fzf > /dev/null 2>&1; then
	printf "clifm: fzf: Command not found" >&2
	exit 127
fi

HIST_FILE="${XDG_CONFIG_HOME:=$HOME/.config}/clifm/profiles/$CLIFM_PROFILE/history.clifm"

# Source our plugins helper
if [ -z "$CLIFM_PLUGINS_HELPER" ] || ! [ -f "$CLIFM_PLUGINS_HELPER" ]; then
	printf "clifm: Unable to find plugins-helper file\n" >&2
	exit 1
fi
# shellcheck source=/dev/null
. "$CLIFM_PLUGINS_HELPER"

# shellcheck disable=SC2154
sed '/^#/d' "$HIST_FILE" | tac | awk '!x[$0]++' | fzf --prompt="$fzf_prompt" \
--reverse --height 15 --info=inline \
--bind "tab:accept" --header "Run a command from history" \
--color="$(get_fzf_colors)" > "$CLIFM_BUS"
printf "\n"

exit 0
