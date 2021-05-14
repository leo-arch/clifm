#!/bin/sh

# Plugin to list files in CWD by a given mime type
# Written by L. Abramovich

mime=""

if [ -n "$1" ] && ([ "$1" = "--help" ] || [ "$1" = "help" ]); then
	name="$(basename "$0")"
	printf "List files in CWD by mime type\n"
	printf "Usage: %s\n" "$name"
	exit 0
fi

while [ -z "$mime" ]; do
	printf "Mime type: "
	read -r mime
done

printf "%s" "ls -d --color=always $(find . -maxdepth 1 -mindepth 1 | cut -d"/" -f2 | file -if- | grep "$mime" | awk -F: '{print $1}')" > "$CLIFM_BUS"

exit 0
