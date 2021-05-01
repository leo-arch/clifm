# shellcheck shell=sh

# CLiFM file picker function

# Usage: p CMD [ARGS ...], where CMD (plus its corresponding parameters)# is the command that should pick selected files. Example: p ls -l.
# CliFM will be executed: select the files you need and quit. Selected
# files will be passed to CMD

# Customize this function as you need and add it to your .bashrc file.
# Recall to restart your shell for changes to take effect.

p() {

	if [ -z "$1" ]; then
		printf "Usage: p CMD [ARGS ...]\n" >&2
		return
	fi

	CLIFM_OPTIONS=""

	# shellcheck disable=SC2086
	clifm $CLIFM_OPTIONS
	file="$(find "${XDG_CONFIG_HOME:=${HOME}/.config}/clifm" -name 'selbox*')"
	if [ -z "$file" ]; then
		file="$(find /tmp/clifm -name 'selbox*')"
	fi

	if [ -f "$file" ]; then
		"$@" "$(cat "$file")"
		rm -- "$file"
	else
		printf "No selected files\n" >&2
	fi
}
