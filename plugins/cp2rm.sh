#!/bin/sh

# Description: Copy a regular file or a directory to a remote machine
# Dependencies: scp, fzf
# Author: L. Abramovich
# License: GPL3

if ! type scp >/dev/null 2>&1; then
	printf "scp: Command not found\n" >&2
	exit 127
fi

if [ -z "$1" ] || [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
	name="${CLIFM_PLUGIN_NAME:-$(basename "$0")}"
	printf "Copy a regular file or a directory to a remote machine
Usage: %s [-e,--edit] [FILE/DIR]\n" "$name"
	exit 0
fi

CONFIG_FILE="${XDG_CONFIG_HOME:-${HOME}/.config}/clifm/plugins/cp2rm.cfg"
if ! [ -f "$CONFIG_FILE" ]; then
	echo "### cp2rm configuration file ####
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

file="$1"
is_dir=0

if [ -e "$file" ]; then
	[ -d "$file" ] && is_dir=1
else
	printf "%s: No such file or directory\n" "$file" >&2
	exit 2
fi

remotes="$(grep "^\[" "$CONFIG_FILE" | tr -d '[]')"
if [ -z "$remotes" ]; then
	printf "No remotes found. Use the -e option to edit the configuration \
file\n" >&2
	exit 1
fi

remote="$(printf "%s" "$remotes" | fzf)"

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
