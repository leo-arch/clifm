#!/bin/sh

# FZF deselection plugin for CliFM
# Description: Deselect currently selected files using FZF

# Written by L. Abramovich

# License: GPL3

if [ -n "$1" ] && ([ "$1" = "--help" ] || [ "$1" = "help" ]); then
	name="$(basename "$0")"
	printf "Deselect CliFM selected files using FZF\n"
	printf "Usage: %s\n" "$name"
	exit $SUCCESS
fi

if ! [ -f "$CLIFM_SELFILE" ]; then
	printf "CliFM: There are no selected files\n" >&2
	exit 1
fi

if ! [ "$(which fzf 2>/dev/null)" ]; then
	printf "CliFM: fzf: Command not found\n" >&2
	exit 1
fi

HELP="Usage:

Alt-h: Toggle this help screen

TAB, Alt-down: Mark + down

Alt-up: Mark + up

Alt-right: Mark all files

Alt-left: Unmark all files

Alt-Enter: Invert marked files

Enter: Confirm and deselect all marked files

Esc: Cancel and exit"

TMPFILE="$(mktemp /tmp/clifm_desel.XXXXXX)"

if [ "$(tput colors)" -eq 256 ]; then
	BORDERS="--border=left"
else
	BORDERS="--no-unicode"
fi

fzf --multi --marker='*' --info=inline \
	--color "prompt:6,fg+:reverse,marker:2:bold,pointer:6,header:7" \
	--bind "alt-down:toggle+down" \
	--bind "alt-up:toggle+up" \
	--bind "alt-right:select-all,alt-left:deselect-all" \
	--bind "alt-h:toggle-preview" --preview-window=:wrap \
	--bind "alt-enter:toggle-all" --preview "printf %s \"$HELP\"" \
	--reverse "$BORDERS" --no-sort --ansi --prompt "CliFM> " > "$TMPFILE" \
	< "$CLIFM_SELFILE"

# If no file was marked for deselection (that is, if TMPFILE is empty), exit
if ! [ -s "$TMPFILE" ]; then
	rm -f -- "$TMPFILE" > /dev/null 2>&1
	exit 0
fi

# shellcheck disable=SC1007
#while ISF= read -r line; do
#	safe_line=$(printf '%s\n' "$line" | sed 's/[[\.*_^$/]/\\&/g')
#	sed "${safe_line}d" "$CLIFM_SELFILE"
#done < "$TMPFILE"

BK_FILE="$(mktemp /tmp/clifm_sel.XXXXXX)"

# shellcheck disable=SC1007
while IFS= read -r sel_line; do
	remove=0

	# shellcheck disable=SC1007
	while ISF= read -r desel_line; do
		if [ "$sel_line" = "$desel_line" ]; then
			remove=1
			break
		fi
	done < "$TMPFILE"

	if [ "$remove" = 0 ]; then
		printf "%s\n" "$sel_line" >> "$BK_FILE"
	fi
done < "$CLIFM_SELFILE"

rm -f -- "$TMPFILE" > /dev/null 2>&1
cp -u -- "$BK_FILE" "$CLIFM_SELFILE"  > /dev/null 2>&1
rm -f -- "$BK_FILE" > /dev/null 2>&1

if [ -z "$DISPLAY" ]; then
	clear
else
	tput rmcup
fi

exit 0
