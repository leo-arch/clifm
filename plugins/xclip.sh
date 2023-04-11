#!/bin/sh

# Description: Copy the contents of the line buffer to the clipboard
# Dependencies: xclip(1)
# License: GPL3
# Author: L. Abramovich

# How to use
# 1. Drop this plugin into the plugins directory (~/.config/clifm/plugins)
# 2. Run 'actions edit' and bind this plugin to an action adding this line: "plugin1=xclip.sh"
# 3. Bind a keyboard shortcut (say Ctrl-y) to the newly created action:
#    Run 'kb edit' and add this line: "plugin1=\C-y"
# 4. Restart clifm

# NOTE: Replace xclip by your preferred X clipboard application

if ! type xclip >/dev/null 2>&1; then
	printf "clifm: xclip: Command not found" 2>&1
	exit 127
fi

if [ -z "$CLIFM_LINE" ]; then
	printf "clifm: xclip: Empty line buffer\n" 2>&1
	exit 1
fi

line="$CLIFM_LINE"
# Manipulate LINE as you wish here, for example, to get only the last field
#line="$(echo "$CLIFM_LINE" | awk '{print $NF}')"

printf "%s" "$line" | xclip -selection clipboard

printf "\x1b[0mclifm: Line buffer copied to the clipboard\n"
