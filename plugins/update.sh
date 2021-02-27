#!/bin/env bash

# Plugins to check for CliFM's updates
# Written by L. Abramovich

upstream="$(curl -s "https://github.com/leo-arch/clifm/releases/latest" | grep -Eo "[0-9]+\.[0-9]+\.[0-9]+")"

cur="$(clifm -v | awk 'NR==1{print $2'})"

if [[ "$cur" == "$upstream" ]]; then
	echo "You are up to date: $cur is the latest version"
else
	echo "${upstream}: New release available"
fi

exit 0
