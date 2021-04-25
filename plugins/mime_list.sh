#!/bin/sh

# Plugin to list files in CWD by a given mime type
# Written by L. Abramovich

mime=""

while [ -z "$mime" ]; do
	printf "Mime type: "
	read -r mime
done

printf "%s" "ls -d --color=always $(find . -maxdepth 1 -mindepth 1 | cut -d"/" -f2 | file -if- | grep "$mime" | awk -F: '{print $1}')" > "$CLIFM_BUS"

exit 0
