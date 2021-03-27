#!/bin/bash

# Terminal colors test script for CliFM
# Written by L. Abramovich
# License GPL2+

USAGE="Usage $0 [16, 256, test]"

if [[ -z $1 ]]; then
	echo "$USAGE" >&2
	exit 1
fi

opt="$1"

if [[ "$opt" -eq 256 ]]; then

	echo -e "256 Colors\n"
	for fgbg in 38 48 ; do
		for color in {0..256} ; do #Colors
			for attr in 0 1 2 4 5 7; do
				echo -en "\e[${attr};${fgbg};5;${color}m${attr};${fgbg};5;${color}\e[0;39;49m "
			done
			echo
	        done
	        echo
	done

elif [[ "$opt" -eq 16 ]]; then

	echo "16 colors"
	for clbg in {40..47} {100..107} 49 ; do
	        for clfg in {30..37} {90..97} 39 ; do
	                for attr in 0 1 2 4 5 7 ; do
	                        echo -en "\e[${attr};${clbg};${clfg}m${attr};${clbg};${clfg}\e[0;39;49m "
	                done
	                echo
	        done
	done

elif [[ $opt == "test" ]]; then
	echo -e "The word 'test' should be printed in green (using the current terminal background color as background color). If not, then X colors are not supported by this terminal (emulator).\n"

	echo "8 colors (3-bit)"
	echo -e "\e[00;32mtest\e[0m\n"

	echo "16 colors (4-bit)"
	echo -e "\e[01;32mtest\e[0m\n"

	echo "256 colors (8-bit)"
	echo -e "\e[01;38;5;82mtest\e[0m\n"

	echo "RGB, true colors (24-bit)"
	echo -e "\e[01;38;2;0;255;0mtest\e[0m\n"

else
	echo "$USAGE" >&2
	exit 1
fi

exit 0
