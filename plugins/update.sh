#!/bin/sh

# Check clifm's upstream version
# Dependencies: awk, grep

# Written by L. Abramovich
# License GPL3

if [ -n "$1" ] && { [ "$1" = "--help" ] || [ "$1" = "-h" ]; }; then
	name="${CLIFM_PLUGIN_NAME:-$(basename "$0")}"
	printf "Check Clifm's upstream version\n"
	printf "\n\x1b[1mUSAGE\x1b[0m\n  %s\n" "$name"
	exit 0
fi

if ! type clifm >/dev/null 2>&1 ; then
	printf "Clifm is not installed\n" >&2
	exit 1
fi

upstream="$(curl -s "https://api.github.com/repos/leo-arch/clifm/releases/latest" | grep tag_name | awk -F'"' '{print $4}')"

if [ -z "$upstream" ]; then
	printf "clifm: Error fetching upstream version" >&2
	exit 1
fi

cur="$(clifm -v)"

if [ "v$cur" = "$upstream" ]; then
	printf "You are up to date: %s is the latest release\n" "$cur"
else
	printf "%s: New release available (current version is %s)\n" "$upstream" "$cur"
fi

exit 0
