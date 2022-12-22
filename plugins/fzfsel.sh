#!/bin/sh

# Files selection plugin for CliFM
# Dependencies: fzf, find

# Author: L. Abramovich
# License: GPL3

if [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
	name="${CLIFM_PLUGIN_NAME:-$(basename "$0")}"
	printf "List files in the current directory allowing \
the user to select one or more of them.\n"
	printf "\n\x1b[1mUSAGE\x1b[0m\n  %s [-f, --flat] [\PATTERN] [-h, --help]

With the -f or --flat option files are listed recursively \
starting from the current directory (aka flat or branch view).

\PATTERN is a glob expression used to filter files. Ex: '-f \*.pdf' \
will recursively list all .pdf files in the current directory.

At exit, selected files are sent to CliFM's Selection Box.

Dependencies: fzf(1), find(1)\n" "$name"
	exit 0
fi

if ! type fzf > /dev/null 2>&1; then
	printf "clifm: fzf: Command not found\n" >&2
	exit 127
fi

TMP_DIR="${TMPDIR:-/tmp}/clifm/$CLIFM_PROFILE"
TMPFILE="$TMP_DIR/${CLIFM_PROFILE}.fzfsel"

# Source our plugins helper
if [ -z "$CLIFM_PLUGINS_HELPER" ] || ! [ -f "$CLIFM_PLUGINS_HELPER" ]; then
	printf "clifm: Unable to find plugins-helper file\n" >&2
	exit 1
fi
# shellcheck source=/dev/null
. "$CLIFM_PLUGINS_HELPER"

! [ -d "$TMP_DIR" ] && mkdir -p "$TMP_DIR"

HELP="Usage:

Alt-h: Toggle this help screen

TAB, Alt-down: Toggle select down

Alt-up: Toggle select up

Ctrl-s: Select all files

Ctrl-d: Deselect all files

Ctrl-t: Invert selection

Enter: Confirm selection, exit, and send selected files to CliFM

Esc: Cancel and exit"

exit_status=0
pattern=""
flat_view=0

if [ "$1" = "-f" ] || [ "$1" = "--flat" ]; then
	flat_view=1
	shift
fi

if [ -n "$1" ]; then
	if [ "$(printf "%c" "$1")" = "\\" ]; then
		pattern="$(echo "$1" | cut -c2-)"
	else
		pattern="$1"
	fi
fi

if [ "$flat_view" -eq 1 ]; then
	if [ -n "$pattern" ]; then
		ls_cmd="find . -name \"$pattern\" 2>/dev/null | cut -c3-"
	else
		ls_cmd="find . 2>/dev/null | cut -c3-"
	fi
else
	if ls --version >/dev/null 2>&1; then
		ls_cmd="ls -Ap --group-directories-first --color=always $pattern"
	else
		ls_cmd="ls -Ap $pattern"
	fi
fi

# shellcheck disable=SC2012
# shellcheck disable=SC2154
eval "$ls_cmd" | fzf --multi --marker='*' --info=inline --keep-right \
	--height "$fzf_height" --header "Select files. Press Alt-h for help" \
	--color "$(get_fzf_colors)" \
	--bind "alt-down:toggle+down,insert:toggle+down" \
	--bind "alt-up:toggle+up" \
	--bind "alt-h:toggle-preview" \
	--bind "ctrl-s:select-all,ctrl-d:deselect-all,ctrl-t:toggle-all" \
	--preview-window=:wrap \
	--bind "alt-enter:toggle-all" --reverse "$(fzf_borders)" \
	--preview "printf %s \"$HELP\"" \
	--no-sort --ansi --prompt "$fzf_prompt" > "$TMPFILE"
exit_status=$?

[ "$exit_status" -eq 130 ] && exit_status=0

# shellcheck disable=SC1007
while ISF= read -r line; do
	printf "%s\n" "$PWD/$line" >> "$CLIFM_SELFILE"
done < "$TMPFILE"

rm -f -- "$TMPFILE" > /dev/null 2>&1

# Erase the FZF window
_lines="${LINES:-100}"
printf "\033[%dM" "$_lines"

exit "$exit_status"
