#!/bin/sh

# Terminal colors test script for CliFM
# Written by L. Abramovich
# License GPL2+

USAGE="Usage: $0 [16, 256, test]"

if [ -z "$1" ] || [ $1 = "--help" ]; then
	printf "%s\n" "$USAGE" >&2
	exit 1
fi

opt="$1"

if [ "$opt" = "256" ]; then

	printf "256 Colors\n"
	for fgbg in $(seq 38 48) ; do
		for color in $(seq 0 256); do #Colors
			for attr in 0 1 2 4 5 7; do
				printf "\e[${attr};${fgbg};5;${color}m${attr};${fgbg};5;${color}\e[0;39;49m "
			done
			printf "\n"
	        done
	        printf "\n"
	done

elif [ "$opt" = "16" ]; then

	printf "16 colors\n"
	for clbg in $(seq 40 47) $(seq 100 107) 49 ; do
	        for clfg in $(seq 30 37) $(seq 90 97) 39 ; do
	                for attr in 0 1 2 4 5 7 ; do
	                        printf "\e[${attr};${clbg};${clfg}m${attr};${clbg};${clfg}\e[0;39;49m "
	                done
	                printf "\n"
	        done
	done

elif [ $opt = "test" ]; then
	printf "The word 'test' should be printed in green (using the current terminal background color as background color). If not, then X colors are not supported by this terminal (emulator).\n"

	printf "8 colors (3-bit)\n"
	printf "\e[00;32mtest\e[0m\n\n"

	printf "16 colors (4-bit)\n"
	printf "\e[01;32mtest\e[0m\n\n"

	printf "256 colors (8-bit)\n"
	printf "\e[01;38;5;82mtest\e[0m\n\n"

	printf "RGB, true colors (24-bit)\n"
	printf "\e[01;38;2;0;255;0mtest\e[0m\n\n"

else
	printf "%s\n" "$USAGE" >&2
	exit 1
fi

exit 0
