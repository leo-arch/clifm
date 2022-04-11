#!/bin/sh

# Description: Copy a regular file or a directory to a remote machine
# Dependencies: scp, fzf
# Author: L. Abramovich
# License: GPL3

if ! type scp >/dev/null 2>&1; then
	printf "scp: Command not found\n" >&2
	exit 127
fi

if ! type fzf >/dev/null 2>&1; then
	printf "fzf: Command not found\n" >&2
	exit 127
fi

if [ -z "$1" ] || [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
	name="${CLIFM_PLUGIN_NAME:-$(basename "$0")}"
	printf "Copy a regular file or directory to a remote machine via scp
Usage: %s [-e,--edit] [FILE/DIR]\n" "$name"
	exit 0
fi

CONFIG_FILE="${XDG_CONFIG_HOME:-${HOME}/.config}/clifm/plugins/cprm.cfg"
if ! [ -f "$CONFIG_FILE" ]; then
	echo "### cprm configuration file ####
# Ex:
# [Desktop machine]
# Options=\"-P 1046\" # See scp(1)
# Target=\"test@192.168.0.23:\"
#
# [Android phone]
# Options=\"-P 2222 -o HostKeyAlgorithms=+ssh-dss\"
# Target=\"android@192.168.0.24:/storage/emulated/0/Download\"" > "$CONFIG_FILE"
fi

if [ "$1" = "-e" ] || [ "$1" = "--edit" ]; then
	if "${EDITOR:-nano}" "$CONFIG_FILE"; then
		exit 0
	fi
	exit 1
fi

if [ -n "$2" ]; then
	printf "Multiple files are not allowed.
To copy more than one file, either archive them (via 'ac') or move them \
to a single directory (then use that directory here).\n"
	exit 1
fi

file="$1"
is_dir=0

if [ -e "$file" ]; then
	[ -d "$file" ] && is_dir=1
else
	printf "%s: No such file or directory\n" "$file" >&2
	exit 2
fi

# Source our plugins helper
if [ -z "$CLIFM_PLUGINS_HELPER" ] || ! [ -f "$CLIFM_PLUGINS_HELPER" ]; then
	printf "CliFM: Unable to find plugins-helper file\n" >&2
	exit 1
fi
# shellcheck source=/dev/null
. "$CLIFM_PLUGINS_HELPER"

remotes="$(grep "^\[" "$CONFIG_FILE" | tr -d '[]')"
if [ -z "$remotes" ]; then
	printf "No remotes found. Use the -e option to edit the configuration \
file\n" >&2
	exit 1
fi

remotes_num="$(echo "$remotes" | grep -c '^')"
remotes_num="$((remotes_num + 2))"
# shellcheck disable=SC2154
remote="$(printf "%s" "$remotes" | fzf \
	--ansi --prompt "$fzf_prompt" \
	--reverse --height "$remotes_num" --info=inline \
	--header "Choose a remote" \
	--bind "tab:accept" --info=inline --color="$(get_fzf_colors)")"

[ -z "$remote" ] && exit 0

opts="$(grep -A2 "$remote" "$CONFIG_FILE" | grep ^"Options" \
| cut -d= -f2-10 | sed 's/\"//g')"
target="$(grep -A2 "$remote" "$CONFIG_FILE" | grep ^"Target" \
| cut -d= -f2 | sed 's/\"//g')"

if [ -z "$target" ]; then
	printf "No target specified\n" >&2;
	exit 1
fi

if [ "$is_dir" -eq 1 ]; then
	opts="-r $opts"
fi

# shellcheck disable=SC2086
if scp $opts "$file" "$target"; then
	exit 0
fi

exit 1
