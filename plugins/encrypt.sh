#!/bin/sh

# Files encryption plugin for CliFM
# Encrypt files passed as parameters using gpg(1)

# Authors: KlzXS, L. Abramovich
# License: GPL3

# Dependencies: gpg, tar, sed, fzf, awk, xargs

if [ -z "$1" ] || [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
	name="${CLIFM_PLUGIN_NAME:-$(basename "$0")}"
	printf "Encrypt one or more files and/or directories using GnuPG\n" >&2
	printf "\n\x1b[1mUSAGE\x1b[0m\n  %s FILE...\n\n" "$name" >&2
	printf "Note: Files are first archived into a single file via \x1b[1mtar\x1b[0m(1) and \n\
then encrypted with \x1b[1mgpg\x1b[0m(1), using either a passphrase or a public key.\n\
You will be given the option to remove original files.\n" >&2
	exit 0
fi

# 1. Check deps

if ! type gpg >/dev/null 2>&1; then
	printf "clifm: gpg: Command not found\n" >&2
	exit 127
fi

if ! type tar >/dev/null 2>&1; then
	printf "clifm: tar: Command not found\n" >&2
	exit 127
fi

if ! type sed >/dev/null 2>&1; then
	printf "clifm: sed: Command not found\n" >&2
	exit 127
fi

if ! type xargs >/dev/null 2>&1; then
	printf "clifm: xargs: Command not found\n" >&2
	exit 127
fi

# Fix backspace when taking input via read
stty erase ^H

# 2. Get destination file

if [ -n "$2" ]; then
	while [ "$out_file" = "" ]; do
		printf "Destiny file ('q' to quit): " >&2
		read -r out_file
	done
else
	out_file="$(echo "$1" | sed 's/\\ /_/g')"
fi

if [ -z "$out_file" ] || [ "$out_file" = "q" ]; then
	exit 0
fi

file="${out_file}.tar"

if [ -e "${file}.gpg" ]; then
	printf "clifm: %s: File exists\n" "${file}.gpg" >&2
	exit 1
fi

files="$(echo "$@" | sed 's/\\ /\t/g;s/ /\n/g;s/\t/ /g;s/\\//g')"

if ! echo "$files" | xargs -I{} tar -rf "$file" {}; then
	rm -rf -- "$file"
	exit 1
fi

while [ "$method" != "p" ] && [ "$method" != "k" ] && [ "$method" != "q" ]; do
	printf "Encrypt with passphrase, key, or quit? [p/k/q] "
	read -r method
done

if [ "$method" = "q" ]; then
	rm -f -- "$file"
	exit 0
fi

# 3. Encrypt

if [ "$method" = "p" ]; then
	# a. Symmetric encryption - Passphrase

	if gpg --symmetric "$file"; then
		rm -f -- "$file"
	fi

else
	# b. Asymmetric encryption - Key
	if ! type fzf >/dev/null 2>&1; then
		printf "clifm: fzf: Command not found\n" >&2
		exit 127
	fi

	if ! type awk >/dev/null 2>&1; then
		printf "clifm: awk: Command not found\n" >&2
		exit 127
	fi

	# Source our plugins helper
	if [ -z "$CLIFM_PLUGINS_HELPER" ] || ! [ -f "$CLIFM_PLUGINS_HELPER" ]; then
		printf "clifm: Unable to find plugins-helper file\n" >&2
		exit 1
	fi
	# shellcheck source=/dev/null
	. "$CLIFM_PLUGINS_HELPER"

	# The recipient code has been taken from KlzXS (https://github.com/jarun/nnn/blob/master/plugins/gpge) and modified to fit our needs
	keyids=$(gpg --list-public-keys --with-colons | grep -E "pub:(.*:){10}.*[eE].*:" | awk -F ":" '{print $5}')

	#shellcheck disable=SC2016
	keyuids=$(printf "%s" "$keyids" | xargs -I{} sh -c 'gpg --list-key --with-colons "{}" | grep "uid" | awk -F ":" '\''{printf "%s %s\n", "{}", $10}'\''')

	l=$(echo "$keyuids" | wc -l)

	# shellcheck disable=SC2154
	recipient=$(printf "%s" "$keyuids" | \
		fzf --reverse --info=inline --height $((l + 2)) --color "$(get_fzf_colors)" \
			--prompt "$fzf_prompt" --header "Select a key ID" | \
		awk '{print $1}')

	if [ -z "$recipient" ] || [ "$recipient" = "" ]; then
		rm -f -- "$file"
		exit 0
	fi

	if ! gpg --encrypt --recipient "$recipient" "$file"; then
		rm -f -- "$file"
		exit 1
	fi

	rm -f -- "$file"
fi

while [ "$answer" != "y" ] && [ "$answer" != "n" ]; do
	printf "Remove original files? [y/n] " >&2
	read -r answer
done

if [ "$answer" = "y" ]; then
	printf "%s" "$files" | xargs -I{} rm -rf -- {}

fi

echo "rf" > "$CLIFM_BUS"

exit 0
