#!/bin/sh

# CliFM plugin to create multiple files and/or dirs at once
# Written by L. Abramovich

if [ -n "$1" ] && [ "$1" = "--help" ]; then
	printf "%s\n" "Usage: $0"
	exit 0
fi

TMP="$(mktemp /tmp/clifm_bn.XXXXXX)"
FILES="$(mktemp /tmp/clifm_bn.XXXXXX)"
DIRS="$(mktemp /tmp/clifm_bn.XXXXXX)"

dirsn=0
filesn=0

printf "# Write here file names to be created \n\
# End file name with a slash to create a directory \n\
# Empty and commented lines are omitted\n\n" > "$TMP"

"${EDITOR:-nano}" "$TMP"

while read -r line; do
	if [ ${#line} = 0 ] || [ "$(printf "%.1s" "$line")" = '#' ]; then
		continue
	fi

	if [ "$(printf "%s" "$line" | tail -c1)" = "/" ]; then
		printf "%s\n" "$line" >> "$DIRS"
		dirsn=$((dirsn + 1))
	else
		printf "%s\n" "$line" >> "$FILES"
		case "$line" in
			*/*) mkdir -p "${line%/*}" ;;
		esac
		filesn=$((filesn + 1))
	fi

done < "$TMP"

if [ $dirsn -gt 0  ]; then
#	cat "$DIRS" | xargs -I{} mkdir -p {}
	xargs -I{} mkdir -p {} < "$DIRS"
fi

if [ $filesn -gt 0 ]; then
	xargs -I{} touch {} < "$FILES"
fi

rm -f -- "$TMP" "$FILES" "$DIRS" 2>/dev/null

exit 0
