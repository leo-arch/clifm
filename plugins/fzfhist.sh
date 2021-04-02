#!/bin/bash

# Command history plugin via FZF for CliFM
# Written by L. Abramovich
# License GPL2+

if ! [[ $(type -P fzf) ]]; then
	echo "CliFM: fzf: Command not found" >&2
	exit 1
fi

FILE="${XDG_CONFIG_HOME:=$HOME/.config}/clifm/profiles/$CLIFM_PROFILE/history.cfm"

cat "$FILE" | fzf --prompt="CliFM > " > "$CLIFM_BUS"
echo ""

exit 0
