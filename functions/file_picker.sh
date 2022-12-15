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
	CLIFM_SELFILE="$(mktemp "/tmp/clifm_selfile.XXXXXXXXXX")"

	# shellcheck disable=SC2086
	clifm $CLIFM_OPTIONS --sel-file="$CLIFM_SELFILE"

	if [ -f "$CLIFM_SELFILE" ]; then
		cmd=""
		while [ -z "$cmd" ]; do
			printf "Enter command ('q' to quit): "
			read -r cmd
		done
		# shellcheck disable=SC2086
		[ "$cmd" != "q" ] && sed 's/ /\\ /g' "$CLIFM_SELFILE" | xargs $cmd
		rm -- "$CLIFM_SELFILE"
	else
		printf "clifm: No selected files\n" >&2
	fi
}
