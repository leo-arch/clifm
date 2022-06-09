#!/bin/sh

# Disk usage analyzer plugin for CliFM
# Written by L. Abramovich
# License: GPL3
# Description: Navigate the filesystem with FZF and display directories size.
# Dependencies: du, fzf

# Usage:
# Left: go to parent directory
# Right, Enter: cd into hovered directory or open hovered file and exit.
# Home/end: go to first/last file
# TAB: Select files
# Ctrl-s: Confirm selection: send selected files to CliFM Selbox
# Shift+up/down: Move one line up/down in the preview window
# Alt+up/down: Move to the beginning/end in the preview window
# Press Esc or Ctrl-q to exit.

if [ -n "$1" ] && { [ "$1" = "--help" ] || [ "$1" = "-h" ]; }; then
	name="${CLIFM_PLUGIN_NAME:-$(basename "$0")}"
	printf "Analiyze disk usage via du and FZF\n"
	printf "Usage: %s [DIR]\n" "$name"
	exit 0
fi

if ! type fzf 2>/dev/null; then
	printf "clifm: fzf: Command not found\n" 2>&1
	exit 127
elif ! type du 2>/dev/null; then
	printf "clifm: du: Command not found\n" 2>&1
	exit 127
fi

HELP="Usage:
Type in the prompt to filter the current list of files. Regular expressions are \
allowed.

Keybindings:
Left: go to parent directory
Right or Enter: cd into hovered directory or open hovered file and exit
Home/end: go to first/last file in the files list
Shift-up/down: Move one line up/down in the preview window
Alt-up/down: Move to the beginning/end in the preview window
Esc/Ctrl-q: Exit"

fcd() {
	if [ "$#" -ne 0 ]; then
		cd "$@" || return
	fi

	if type dircolors > /dev/null 2>&1; then
		dir_color="$(dircolors -c | grep -o "[\':]di=....." | cut -d';' -f2)"
	fi
	[ -z "$dir_color" ] && dir_color="34"

	# Source our plugins helper
	if [ -z "$CLIFM_PLUGINS_HELPER" ] || ! [ -f "$CLIFM_PLUGINS_HELPER" ]; then
		printf "clifm: Unable to find plugins-helper file\n" >&2
		exit 1
	fi

	# shellcheck source=/dev/null
	. "$CLIFM_PLUGINS_HELPER"

	_borders="$(fzf_borders)"
	_colors="$(get_fzf_colors)"

	# Keep FZF running until the user presses Esc or C-q
	while true; do
		lsd=$(printf "\033[0;%sm..\n" "$dir_color"; ls --color=always --group-directories-first --indicator-style=none -A .)
		# shellcheck disable=SC2154
		file="$(printf "%s\n" "$lsd" | fzf \
			--color="$_colors" --height "$fzf_height" \
			--bind "right:accept,left:first+accept" \
			--bind "insert:clear-query" \
			--bind "home:first,end:last" \
			--bind "alt-h:preview(printf %s \"$HELP\")" \
			--bind "alt-p:toggle-preview" \
			--bind "shift-up:preview-up" \
			--bind "shift-down:preview-down" \
			--bind "alt-up:preview-page-up" \
			--bind "alt-down:preview-page-down" \
			--bind "esc:abort" \
			--bind "ctrl-q:abort" \
			--ansi --prompt="$fzf_prompt" --reverse --no-clear \
			--no-info --keep-right --multi --header="Press 'Alt-h' for help
$PWD
$FZF_HEADER" --marker="*" --preview-window=:wrap "$_borders" \
--preview="printf \"Total: \"; du $DU_OPTS1 {} 2>/dev/null | cut -f1; du $DU_OPTS2 {}/* 2>/dev/null | sort $SORT_OPTS")"

		# If FZF returned no file, exit
		[ ${#file} -eq 0 ] && return 0

		# If the returned file is a directory, just cd into it. Otherwise, exit
		if [ -d "${PWD}/$file" ]; then
			cd "$file" || return
		fi
	done
}

main() {
	if ! type fzf > /dev/null 2>&1; then
		printf "clifm: fzf: Command not found\n" >&2
		exit 127
	fi

	OS="$(uname -s)"
	if [ "$OS" = "NetBSD" ]; then
		# NetBSD sort(1) cannot sort human readable sizes (-h)
		export DU_OPTS1="-s"
		export DU_OPTS2="-d0"
		export SORT_OPTS="-nr"
	else
		export DU_OPTS1="-hs"
		export DU_OPTS2="-hd0"
		export SORT_OPTS="-hr"
	fi
	fcd "$@"
}

main "$@"

# Erase the FZF window
_lines="${LINES:-100}"
printf "\033[%dM" "$_lines"

exit 0
