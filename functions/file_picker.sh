# CLiFM file picker function
# Usage: p ARG ..., where ARG is the command that should pick selected files 
# and its corresponding parameters. Example: p ls -l
# Customize this function as you need and add it to your .bashrc file

p() {

	if [[ !"$1" ]]; then
		echo "Missing argument" >&2
	fi

	CLIFM_OPTIONS=""

	clifm $CLIFM_OPTIONS
	file="${XDG_CONFIG_HOME:=${HOME}/.config}/clifm/default/selbox"

	if [[ -f "$file" ]]; then
		"$@" $(cat "$file")
	else
		echo "No selected files" >&2
	fi
}
