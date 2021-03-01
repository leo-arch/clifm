#!/bin/env bash

# Drag and drop plugin for CLiFM
# Written by L. Abramovich

SUCCESS=0
ERROR=1

DRAGON=""

if [[ $(type -P dragon-drag-and-drop) ]]; then
	DRAGON="dragon-drag-and-drop"
elif [[ $(type -P dragon) ]]; then
	DRAGON="dragon"
else
	echo "CLiFM: Neither dragon nor dragon-drag-and-drop was found. Exiting... " >&2
	exit $ERROR
fi

if [[ -z $1 ]]; then
	$DRAGON --print-path --target | while read -r r;
	do echo "$r" >> $HOME/.config/clifm/$CLIFM_PROFILE/selbox;
	done

else
	$DRAGON "$@"
fi

if [[ $? -eq 0 ]]; then
	exit $SUCCESS
fi

exit $ERROR
