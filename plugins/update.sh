#!/bin/sh

# Plugins to check for CliFM's updates
# Written by L. Abramovich
# License GPL3

if [ -n "$1" ] && { [ "$1" = "--help" ] || [ "$1" = "-h" ]; }; then
	name="${CLIFM_PLUGIN_NAME:-$(basename "$0")}"
	printf "Check for CliFM updates\n"
	printf "Usage: %s\n" "$name"
	exit 0
fi

upstream="$(curl -s "https://github.com/leo-arch/clifm/releases/latest" | grep -Eo "[0-9]+\.[0-9]+.*" | cut -d'"' -f1)"

if [ -z "$upstream" ]; then
	printf "clifm: Error getting upstream version"
	exit 1
fi

cur="$(clifm -v | awk 'NR==1{print $2}')"

if [ "$cur" = "$upstream" ]; then
	printf "You are up to date: %s is the latest release\n" "$cur"
else
	printf "%s: New release available\n" "$upstream"
fi

exit 0
