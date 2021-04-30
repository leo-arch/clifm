# CliFM CD on quit function

# Run CliFM and, after exit, read the .last file and cd into it

# 1) Customize this function as you need and add it to your shell configuration
# file (e.g., $HOME/.bashrc)
# 2) Restart your shell (for changes to take effect)
# 3) Run clifm using the name of the function below: c [ARGS ...]

# CliFM CD on quit function
c() {
	clifm "--cd-on-quit" "$@"
	dir="$(grep ^"*" "${XDG_CONFIG_HOME:=${HOME}/.config}/clifm/.last" 2>/dev/null | cut -d':' -f2)";
	if [ -d "$dir" ]; then
		cd "$dir"
	else
		printf "No directory specified\n" >&2
	fi
}
