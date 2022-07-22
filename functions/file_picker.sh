# shellcheck shell=sh

# File picker function for CliFM

# Usage: p
#
# 1. CliFM will be executed: select the files you need and quit.
# 2. You will be asked for a command to execute over selected files,
# for example: ls -l
#
# Customize this function as you need and add it to your shell rc file.
# Recall to restart your shell for changes to take effect.

# Dependecies: clifm, sed, find, xargs

# Author: L. Abramovich
# License: GPL3

p() {
	# Options to be passed to clifm
	CLIFM_OPTIONS=""

	# shellcheck disable=SC2086
	clifm $CLIFM_OPTIONS
	sel_file="$(find "${XDG_CONFIG_HOME:=${HOME}/.config}/clifm" -name 'selbox*')"
	if [ -z "$sel_file" ]; then
		sel_file="$(find /tmp/clifm -name 'selbox*')"
	fi

	if [ -f "$sel_file" ]; then
		cmd=""
		while [ -z "$cmd" ]; do
			printf "Enter command ('q' to quit): "
			read -r cmd
		done
		# shellcheck disable=SC2086
		[ "$cmd" != "q" ] && sed 's/ /\\ /g' "$sel_file" | xargs $cmd
		rm -- "$sel_file"
	else
		printf "No selected files\n" >&2
	fi
}
