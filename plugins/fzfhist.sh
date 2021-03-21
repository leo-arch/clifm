#!/bin/bash

# Command history plugin via FZF for CliFM
# Written by L. Abramovich
# License GPL2+

FILE="${XDG_CONFIG_HOME:=$HOME/.config}/clifm/profiles/$CLIFM_PROFILE/history.cfm"

cat "$FILE" | fzf --prompt="CliFM > " > "$CLIFM_BUS"
echo ""

exit 0
