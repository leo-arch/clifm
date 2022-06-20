#!/bin/sh

# Description: Send a regular file or directory to a remote machine
# Dependencies: fzf, and one of these: scp, ffsend, croc
# Author: L. Abramovich
# License: GPL3

if [ -z "$1" ] || [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
	name="${CLIFM_PLUGIN_NAME:-$(basename "$0")}"
	printf "Copy a regular file or directory to a remote machine
Usage: %s [-e,--edit] [FILE/DIR]\n" "$name"
	exit 0
fi

CONFIG_FILE="${XDG_CONFIG_HOME:-${HOME}/.config}/clifm/plugins/cprm.cfg"
if ! [ -f "$CONFIG_FILE" ]; then
	echo "### cprm configuration file ####
# Ex:
# See scp(1) for information about scp options
#[Desktop machine]
#Options=\"-P 1046\"
#Target=\"test@192.168.0.23:\"
#
#[Android phone]
#Options=\"-P 2222 -o HostKeyAlgorithms=+ssh-dss\"
#Target=\"android@192.168.0.24:/storage/emulated/0/Download\"
#
# Just install ffsend. You'll get a download link
# Do not alter this remote name
#[ffsend]
#Options=\"--downloads 1 --copy\"
#Target=\"\"
#
# Just install croc. You'll get a download code
# Do not alter this remote name
#[croc]
#Options=\"\"
#Target=\"\"" > "$CONFIG_FILE"
fi

if ! type fzf >/dev/null 2>&1; then
	printf "clifm: fzf: Command not found\n" >&2
	exit 127
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

file="$(echo "$1" | sed 's/\\//g')"
is_dir=0

if [ -e "$file" ]; then
	[ -d "$file" ] && is_dir=1
else
	printf "%s: No such file or directory\n" "$file" >&2
	exit 2
fi

# Source our plugins helper
if [ -z "$CLIFM_PLUGINS_HELPER" ] || ! [ -f "$CLIFM_PLUGINS_HELPER" ]; then
	printf "clifm: Unable to find plugins-helper file\n" >&2
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

##### FFSEND #####
if [ "$remote" = "ffsend" ]; then
	if ! type ffsend >/dev/null 2>&1; then
		printf "clifm: ffsend: Command not found\n" >&2
		exit 127
	else
		# shellcheck disable=SC2086
		ffsend upload $opts "$file" && exit 0
		exit 1
	fi
fi

##### CROC #####
if [ "$remote" = "croc" ]; then
	if ! type croc >/dev/null 2>&1; then
		printf "clifm: croc: Command not found\n" >&2
		exit 127
	else
		croc send "$file" && exit 0
		exit 1
	fi
fi

##### SCP #####
if ! type scp >/dev/null 2>&1; then
	printf "clifm: scp: Command not found\n" >&2
	exit 127
fi

if [ -z "$target" ]; then
	printf "No target specified\n" >&2;
	exit 1
fi

if [ "$is_dir" -eq 1 ]; then
	opts="-r $opts"
fi

# shellcheck disable=SC2086
scp $opts "$file" "$target" && exit 0
exit 1
