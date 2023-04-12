#!/bin/sh

# Description: Copy the contents of the line buffer to the clipboard
# Dependencies:
#  Any of the following: cb, wl-copy, xclip, xsel, pbcopy, termux-clipboard-set, clipboard, or clip
# License: GPL3
# Author: L. Abramovich

# How to use
# 1. Drop this plugin into the plugins directory (~/.config/clifm/plugins)
# 2. Run 'actions edit' and bind this plugin to an action adding this line: "plugin1=xclip.sh"
# 3. Bind a keyboard shortcut (say Ctrl-y) to the newly created action:
#    Run 'kb edit' and add this line: "plugin1=\C-y"
# 4. Restart clifm

if [ -z "$CLIFM_LINE" ]; then
	printf "clifm: Empty line buffer\n" 2>&1
	exit 1
fi

line="$CLIFM_LINE"
# Manipulate LINE as you wish here, for example, to get only the last field
#line="$(echo "$CLIFM_LINE" | awk '{print $NF}')"

if type cb >/dev/null 2>&1; then
	printf "%s" "$line" | cb --copy
elif [ -n "$WAYLAND_DISPLAY" ]; then
	printf "%s" "$line" | wl-copy
elif type xclip >/dev/null 2>&1; then
	printf "%s" "$line" | xclip -sel clip
elif type xsel >/dev/null 2>&1; then
	printf "%s" "$line" | xsel -bi
elif type pbcopy >/dev/null 2>&1; then # MacOS
	printf "%s" "$line" | pbcopy
elif type termux-clipboard-set >/dev/null 2>&1; then
	printf "%s" "$line" | termux-clipboard-set
elif type clipboard >/dev/null 2>&1; then # Haiku
	printf "%s" "$line" | clipboard --stdin
elif type clip >/dev/null 2>&1; then # Cygwin
	printf "%s" "$line" | clip
else
	printf "clifm: No clipboard application found\n" 2>&1
	exit 1
fi

printf "\x1b[0mclifm: Line buffer copied to the clipboard\n"
