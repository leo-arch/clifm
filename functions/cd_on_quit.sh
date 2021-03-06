# CLiFM CD on quit function

# Run CliFM and, after exit, read the .last file and cd into it

# 1) Customize this function as you need and add it to your .bashrc file
# 2) Restart your shell (for changes to take effect)
# 3) Run clifm using the name of the function below: c [ARGS ...]

c() {
	clifm "--cd-on-quit" "$@"
	dir=$(cat "${XDG_CONFIG_HOME:=${HOME}/.config}/clifm/.last" 2>/dev/null);
	if [[ -d "$dir" ]]; then
		cd "$dir"
	else
		echo "No directory specified" >&2
	fi
}
