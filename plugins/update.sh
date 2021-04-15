#!/bin/sh

# Plugins to check for CliFM's updates
# Written by L. Abramovich

upstream="$(curl -s "https://github.com/leo-arch/clifm/releases/latest" | grep -Eo "[0-9]+\.[0-9]+.*" | cut -d'"' -f1)"

if [ -z "$upstream" ]; then
	printf "Error getting upstream version"
	exit 1
fi

cur="$(clifm -v | awk 'NR==1{print $2}')"

if [ "$cur" = "$upstream" ]; then
	printf "You are up to date: %s is the latest release\n" "$cur"
else
	printf "%s: New release available\n" "$upstream"
fi

exit 0
