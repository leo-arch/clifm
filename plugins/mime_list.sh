#!/bin/sh

mime=""

while [ -z "$mime" ]; do
	read -rp "Mime type: " mime
done

#echo "ls -Ad | file -if- | grep "$mime" | awk -F':' '{pr#int $1}'" > $CLIFM_BUS

printf "%s" "ls -d --color=always $(ls -A | file -if- | grep "$mime" | awk -F: '{print $1}')" > "$CLIFM_BUS"

exit 0
