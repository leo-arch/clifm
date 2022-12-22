#!/bin/sh

# Files decryption plugin for CliFM
# Decrypt a file passed as parameter using gpg(1)

# Author: L. Abramovich
# License: GPL3

# Dependencies: gpg, tar, sed, grep

if [ -z "$1" ] || [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
	name="${CLIFM_PLUGIN_NAME:-$(basename "$0")}"
	printf "Decrypt a GnuPG encrypted file\n" >&2
	printf "\n\x1b[1mUSAGE\x1b[0m\n  %s FILE\n\n" "$name" >&2
	printf "Note: You will be given the options to extract the unencrypted\n\
file (if archived) and then to remove the original encrypted file\n" >&2
	exit 0
fi

if [ -n "$2" ]; then
	printf "clifm: Only one file can be decrypted at a time\n" >&2
	exit 1
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

if ! type grep >/dev/null 2>&1; then
	printf "clifm: grep: Command not found\n" >&2
	exit 127
fi

# Fix backspace when taking input via read
stty erase ^H

# 2. Decrypt the file

file="$(echo "$1" | sed 's/\\ / /g')"

case "$file" in
*".gpg")
	if ! gpg --decrypt --output "${file}.dec" "$file"; then
		exit 1
	fi

	if echo "$file" | grep -q "\.tar"; then
		while [ "$answer" != "y" ] && [ "$answer" != "n" ]; do
			printf "Extract tar archive? [y/n] "
			read -r answer
		done

		if [ "$answer" = "y" ]; then
			tar -xf "${file}.dec"
			rm -f -- "${file}.dec"
		fi
	fi
;;
*)
	printf "clifm: %s: Not a GnuPG encrypted file\n" "$file" >&2
	exit 1
;;
esac

while [ "$answer" != "y" ] && [ "$answer" != "n" ]; do
	printf "Remove original encrypted file? [y/n] "
	read -r answer
done

if [ "$answer" = "y" ]; then
	rm -rf -- "$file"
fi

echo "rf" > "$CLIFM_BUS"

exit 0
