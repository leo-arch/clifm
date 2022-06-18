#!/bin/sh

# FZF selection plugin for CliFM
# Written by L. Abramovich
# License: GPL3

if [ -n "$1" ]; then
	if [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
		name="${CLIFM_PLUGIN_NAME:-$(basename "$0")}"
		printf "Usage: %s [CMD [ARGS]] [-h, --help]
Without arguments, it prints the list of files in the current directory allowing \
the user to select one or more of them. At exit, selected files are send to CliFM's \
Selection Box.

If a command is passed (CMD), either internal to CliFM or external, the list of \
currently selected files is printed, allowing the user to mark one or more of \
them. At exit, marked files will be passed to CMD and executed by CliFM.

The %%f modifier can be used to specify the argument position in CMD for marked \
files. For example, the command '%s cp %%f mydir/' will copy all marked files to \
the directory mydir/. If %%f is not specified, marked files will be inserted at \
the end of CMD.\n" "$name" "$name"
		exit 0
	fi
fi

if ! type fzf > /dev/null 2>&1; then
	printf "clifm: fzf: Command not found\n" >&2
	exit 127
fi

TMPDIR="/tmp/clifm/$CLIFM_PROFILE"
TMPFILE="$TMPDIR/${CLIFM_PROFILE}.fzfsel"

# Source our plugins helper
if [ -z "$CLIFM_PLUGINS_HELPER" ] || ! [ -f "$CLIFM_PLUGINS_HELPER" ]; then
	printf "clifm: Unable to find plugins-helper file\n" >&2
	exit 1
fi
# shellcheck source=/dev/null
. "$CLIFM_PLUGINS_HELPER"

! [ -d "$TMPDIR" ] && mkdir -p "$TMPDIR"

HELP="Usage:

Alt-h: Toggle this help screen

TAB, Alt-down: Toggle select down

Alt-up: Toggle select up

Ctrl-s: Select all files

Ctrl-d: Deselect all files

Ctrl-t: Invert selection

Enter: Confirm selection, exit, and send selected files to CliFM

Esc: Cancel and exit"

marksel_mode=0
cmd="$1"

OS="$(uname -s)"
if [ -z "$OS" ]; then
	printf "clifm: Unable to detect operating system\n" >&2
	exit 1
fi

exit_status=0

if [ -n "$cmd" ]; then
	case "$OS" in
		Linux) ls_cmd="ls --color=always --indicator=none" ;;
		*) ls_cmd="ls" ;;
	esac

	if ! [ -f "$CLIFM_SELFILE" ]; then
		printf "clifm: No selected files\n" >&2
		exit 1
	fi

	int_cmds=" cd o open p pr prop r t tr trash mm mime bm bookmarks br bulk ac ad exp pin jc jp bl l le te tag ta v vv paste bb bleach d dup c cp m mv"

	is_int=0

	if echo "$int_cmds" | grep -q " $cmd "; then
		is_int=1
	fi

	if [ "$is_int" -eq 0 ] && ! [ "$(which "$cmd" 2>/dev/null)" ]; then
		printf "clifm: %s: Command not found\n" "$cmd" >&2
		exit 127
	fi

	marksel_mode=1
	# shellcheck disable=SC2046
	# shellcheck disable=SC2154
	$ls_cmd $(cat "$CLIFM_SELFILE") | \
	fzf --multi --marker='*' --info=inline --keep-right \
		--color="$(get_fzf_colors)" --header "Select files in the current directory" \
		--bind "alt-down:toggle+down,insert:toggle+down" \
		--bind "alt-up:toggle+up" \
		--bind "ctrl-s:select-all,ctrl-d:deselect-all,ctrl-t:toggle-all" \
		--bind "alt-h:toggle-preview" --preview-window=:wrap \
		--bind "alt-enter:toggle-all" --preview "printf %s \"$HELP\"" \
		--reverse "$(fzf_borders)" --ansi --prompt "$fzf_prompt" > "$TMPFILE"
	exit_status=$?

else
	case "$OS" in
		Linux) ls_cmd="ls -Ap --group-directories-first --color=always" ;;
		*) ls_cmd="ls -Ap"
	esac
	# shellcheck disable=SC2012
	# shellcheck disable=SC2154
	$ls_cmd | fzf --multi --marker='*' --info=inline \
		--height "$fzf_height" --header "Select files in the current directory" \
		--color "$(get_fzf_colors)" \
		--bind "alt-down:toggle+down,insert:toggle+down" \
		--bind "alt-up:toggle+up" \
		--bind "ctrl-s:select-all,ctrl-d:deselect-all,ctrl-t:toggle-all" \
		--bind "alt-h:toggle-preview" --preview-window=:wrap \
		--bind "alt-enter:toggle-all" --preview "printf %s \"$HELP\"" \
		--reverse "$(fzf_borders)" --no-sort --ansi --prompt "$fzf_prompt" > "$TMPFILE"
	exit_status=$?
fi

args="$*"

if [ "$marksel_mode" -eq 1 ]; then
	files="$(tr '\n' ' ' < "$TMPFILE")"
	if echo "$args" | grep -q "%f"; then
		echo "$args" | sed "s|\%f|$files|g" > "$CLIFM_BUS"
	else
		printf "%s %s" "$(echo "$args" | sed 's/\n/ /g')" "$files" > "$CLIFM_BUS"
	fi
else
	# shellcheck disable=SC1007
	while ISF= read -r line; do
		printf "%s\n" "$PWD/$line" >> "$CLIFM_SELFILE"
	done < "$TMPFILE"
fi

rm -f -- "$TMPFILE" > /dev/null 2>&1

# Erase the FZF window
_lines="${LINES:-100}"
printf "\033[%dM" "$_lines"

exit "$exit_status"
