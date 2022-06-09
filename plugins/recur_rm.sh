#!/bin/sh

# Description: Recursively search regular files in current directory
# (using find and fzf) and remove selected files

# Dependencies: find, fzf, cat, xargs, rm
# Author: L. Abramovich
# License: GPL3

if ! type fzf >/dev/null 2>&1; then
	printf "clifm: fzf: Command not found\n" >&2
	exit 127
fi

if ! type find >/dev/null 2>&1; then
	printf "clifm: find: Command not found\n" >&2
	exit 127
fi

if [ -z "$1" ] || [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
	name="${CLIFM_PLUGIN_NAME:-$(basename "$0")}"
	printf "Recursively remove regular files via FZF
Usage: %s [PATTERN]\n" "$name"
	exit 0
fi

pattern="$1"
if [ -z "$pattern" ]; then
	printf "clifm: A pattern must be specified as first argument\n" >&2
	exit 1
fi

# Source our plugins helper
if [ -z "$CLIFM_PLUGINS_HELPER" ] || ! [ -f "$CLIFM_PLUGINS_HELPER" ]; then
	printf "clifm: Unable to find plugins-helper file\n" >&2
	exit 1
fi
# shellcheck source=/dev/null
. "$CLIFM_PLUGINS_HELPER"

TMP_FILE="$(mktemp /tmp/clifm_rrm.XXXXXX)"

# shellcheck disable=SC2154
find . -type f -name "$pattern" 2>/dev/null | fzf \
	--ansi --prompt "$fzf_prompt" --multi --marker='*' \
	--reverse --height "$fzf_height" --info=inline \
	--header "Select files with TAB and press Enter to remove them" \
	--color="$(get_fzf_colors)" > "$TMP_FILE"

! [ -f "$TMP_FILE" ] && exit 0

num_files="$(printf "%d" "$(sed -n '$=' "$TMP_FILE")")"
if [ "$num_files" -eq 0 ]; then
	rm -- "$TMP_FILE" 2>/dev/null
	exit 0
fi

cat -- "$TMP_FILE"
printf "\n"
while [ "$answer" != "y" ] && [ "$answer" != "n" ]; do
	printf "Delete %d file(s)? [y/N] " "$num_files"
	read -r answer
	if [ -z "$answer" ]; then
		answer="n"
		break
	fi
done

if [ "$answer" = "y" ]; then
	xargs -d'\n' rm -r < "$TMP_FILE"
	printf "CliFM: %d file(s) removed\n" "$num_files"
#	echo "rf" > "$CLIFM_BUS"
fi

exit 0
