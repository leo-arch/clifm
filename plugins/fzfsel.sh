#!/bin/sh

# FZF selection plugin for CliFM
# Description: Select files in the current working directory using FZF

# Written by L. Abramovich

# License: GPL3

# Usage: select files using Space or x keys (a to select all), and
# deselect using the z key (s to deselect all). Once done, press Enter.
# At exit, selected files will be sent to CliFM Selbox

USAGE="[CMD [ARGS]] [--help, help]
Without arguments, it prints the list of files in the current directory allowing \
the user to select one or more of them. At exit, selected files are send to CliFM's \
Selection Box.
If a command is passed, either internal to CliFM or external, the list of \
currently selected files is printed, allowing the user to mark one or more of \
them. At exit, marked files will be passed to CMD and executed by CliFM."

if [ -n "$1" ]; then
	if [ "$1" == "--help" ] || [ "$1" == "help" ]; then
		printf "Usage: %s %s\n" "$(basename "$0")" "$USAGE"
		exit 0
	fi
fi

if ! [ "$(which fzf 2>/dev/null)" ]; then
	printf "CLiFM: fzf: Command not found\n" >&2
	exit 1
fi

TMPDIR="/tmp/clifm/$CLIFM_PROFILE"
TMPFILE="$TMPDIR/${CLIFM_PROFILE}.fzfsel"

! [ -d "$TMPDIR" ] && mkdir -p "$TMPDIR"

HELP="Usage:

Alt-h: Toggle this help screen

TAB, Alt-down: Toggle select down

Alt-up: Toggle select up

Alt-right: Select all files

Alt-left: Deselect all files

Alt-Enter: Invert selection

Enter: Confirm selection, exit, and send selected files to CliFM

Esc: Cancel and exit"

if [ "$(tput colors)" -eq 256 ]; then
	BORDERS="--border=left"
else
	BORDERS="--no-unicode"
fi

marksel_mode=0
cmd="$1"

if [ -n "$cmd" ]; then

	if ! [ -f "$CLIFM_SELFILE" ]; then
		printf "CliFM: There are no selected files\n" >&2
		exit 1
	fi

	int_cmds=" cd o open p pr prop r t tr trash mm mime bm bookmarks br bulk ac ad exp export pin jc jp bl le te "

	is_int=0

	if echo "$int_cmds" | grep -q " $cmd "; then
		is_int=1
	fi
#	for x in $int_cmds; do
#		if [ "$cmd" == "$x" ]; then
#			is_int=1
#			break
#		fi
#	done

	if [ "$is_int" -eq 0 ] && ! [ "$(which "$cmd" 2>/dev/null)" ]; then
		printf "CliFM: %s: Command not found\n" "$cmd" >&2
		exit 1
	fi

	marksel_mode=1
	ls --color=always --indicator=none $(cat "$CLIFM_SELFILE") | \
	fzf --multi --marker='*' --info=inline --keep-right \
		--color "prompt:6,fg+:reverse,marker:2:bold,pointer:6,header:7" \
		--bind "alt-down:toggle+down,insert:toggle+down" \
		--bind "alt-up:toggle+up" \
		--bind "alt-right:select-all,alt-left:deselect-all" \
		--bind "alt-h:toggle-preview" --preview-window=:wrap \
		--bind "alt-enter:toggle-all" --preview "printf %s \"$HELP\"" \
		--reverse "$BORDERS" --ansi --prompt "CliFM> " > "$TMPFILE"

else
	# shellcheck disable=SC2012
	ls -A --group-directories-first --color=always | \
	fzf --multi --marker='*' --info=inline \
		--color "prompt:6,fg+:reverse,marker:2:bold,pointer:6,header:7" \
		--bind "alt-down:toggle+down,insert:toggle+down" \
		--bind "alt-up:toggle+up" \
		--bind "alt-right:select-all,alt-left:deselect-all" \
		--bind "alt-h:toggle-preview" --preview-window=:wrap \
		--bind "alt-enter:toggle-all" --preview "printf %s \"$HELP\"" \
		--reverse "$BORDERS" --no-sort --ansi --prompt "CliFM> " > "$TMPFILE"
fi

if [ "$marksel_mode" -eq 1 ]; then
#	printf "CMD: %s\n" "$(echo $@ | sed 's/\n/ /g')"
	printf "%s %s" "$(echo $@ | sed 's/\n/ /g')" "$(cat $TMPFILE | sed 's/\n/ /g')" > "$CLIFM_BUS"
else
	# shellcheck disable=SC1007
	while ISF= read -r line; do
		printf "%s\n" "$PWD/$line" >> "$CLIFM_SELFILE"
	done < "$TMPFILE"
fi


#if [ -z "$DISPLAY" ]; then
#	clear
#else
#	tput rmcup
#fi

rm -f -- "$TMPFILE" > /dev/null 2>&1

#clear

exit 0
