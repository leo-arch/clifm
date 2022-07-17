# shellcheck shell=sh

# CliFM file picker function

# Usage: p

# 1. CliFM will be executed: select the files you need and quit.
# 2. You will be asked for a command to execute over selected files,
# for example: ls -l

# Customize this function as you need and add it to your shell rc file.
# Recall to restart your shell for changes to take effect.

p() {
	# Options to be passed to clifm
	CLIFM_OPTIONS=""

	# shellcheck disable=SC2086
	clifm $CLIFM_OPTIONS
	file="$(find "${XDG_CONFIG_HOME:=${HOME}/.config}/clifm" -name 'selbox*')"
	if [ -z "$file" ]; then
		file="$(find /tmp/clifm -name 'selbox*')"
	fi

	if [ -f "$file" ]; then
		cmd=""
		while [ -z "$cmd" ]; do
			printf "Enter command ('q' to quit): "
			read -r cmd
		done
		# shellcheck disable=SC2046
		[ "$cmd" != "q" ] && $cmd $(sed 's/ /\\ /g' "$file")
		rm -- "$file"
	else
		printf "No selected files\n" >&2
	fi
}
