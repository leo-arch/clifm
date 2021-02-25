# CLiFM CD on quit function
# Run CliFM and, after exit, read the .last file and cd into it
# Customize this function as you need and add it to your .bashrc file

c() {
	./clifm_test "--cd-on-quit" "$@"
	dir=$(cat "${XDG_CONFIG_HOME:=${HOME}/.config}/clifm/.last" 2>/dev/null);
	if [[ -d "$dir" ]]; then
		cd "$dir"
	else
		echo "No directory specified" >&2
	fi
}
