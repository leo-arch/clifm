#!/bin/sh

# Plugins to check for CliFM's updates
# Written by L. Abramovich

upstream="$(curl -s "https://github.com/leo-arch/clifm/releases/latest" | grep -Eo "[0-9]+\.[0-9]+\.[0-9]+")"

cur="$(clifm -v | awk 'NR==1{print $2'})"

if [ "$cur" = "$upstream" ]; then
	printf "You are up to date: %s is the latest version\n" "$cur"
else
	printf "%s: New release available\n" "$upstream"
fi

exit 0
