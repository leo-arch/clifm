#!/bin/sh

# Terminal colors test script for CliFM
# Written by L. Abramovich
# License GPL3

name="${CLIFM_PLUGIN_NAME:-$(basename "$0")}"

USAGE="Usage: $name [16, 256, test]"

if [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
	printf "Terminal colors test script for CliFM\n"
	printf "\n\x1b[1mUSAGE\x1b[0m\n  %s [16, 256, test]\n" "$name"
	exit 1
fi

opt="${1:-test}"

if [ "$opt" = "256" ]; then
	printf "256 Colors\n"
	for fgbg in $(seq 38 48) ; do
		for color in $(seq 0 256); do #Colors
			for attr in 0 1 2 4 5 7; do
				printf "\e[%s;%s;5;%sm%s;%s;5;%s\e[0;39;49m " "${attr}" "${fgbg}" "${color}" "${attr}" "${fgbg}" "${color}"
			done
			echo
	        done
	        echo
	done

elif [ "$opt" = "16" ]; then
	printf "16 colors\n"
	for clbg in $(seq 40 47) $(seq 100 107) 49 ; do
	        for clfg in $(seq 30 37) $(seq 90 97) 39 ; do
	                for attr in 0 1 2 4 5 7 ; do
						printf "\e[%s;%s;%sm%s;%s;%s\e[0;39;49m " "${attr}" "${clbg}" "${clfg}" "${attr}" "${clbg}" "${clfg}"
	                done
	                printf "\n"
	        done
	done

elif [ "$opt" = "test" ]; then
	printf "The word 'test' should be printed in green (using the current terminal background color as background color). If not, then X colors are not supported by this terminal (emulator).\n"

	printf "8 colors (3-bit)\n"
	printf "\e[00;32mtest\e[0m\n\n"

	printf "16 colors (4-bit)\n"
	printf "\e[00;32mtest\e[0m\n\n"

	printf "256 colors (8-bit)\n"
	printf "\e[00;38;5;2mtest\e[0m\n\n"

	printf "RGB, true colors (24-bit)\n"
	printf "\e[00;38;2;20;170;0mtest\e[0m\n\n"

else
	printf "%s\n" "$USAGE" >&2
	exit 1
fi

exit 0
