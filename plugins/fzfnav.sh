#!/bin/sh

# FZF navigation plugin for CLiFM

# Taken from https://github.com/junegunn/fzf/wiki/Examples#interactive-cd and adapted to CliFM by L. Abramovich

# License: GPL2+

# Description: Navigate the filesystem with FZF (Left: go to parent;
# Right: cd into hovered directory). At exit (pressing q) CLiFM will
# change to the last directory visited with FZF or open the last
# hovered file. Press Esc to cancel and exit

if ! [ $(which fzf 2>/dev/null) ]; then
	echo "CLiFM: fzf: Command not found" >&2
	exit 1
fi

TMP="$(mktemp /tmp/clifm.XXXXXX)"

function fcd() {
    if [[ "$#" != 0 ]]; then
        cd "$@";
        return
    fi
    while true; do
#        local lsd=$(echo ".." && ls -Ap | grep '/$' | sed 's;/$;;')
        local lsd=$(echo ".." && ls -Ap)
        local dir="$(printf '%s\n' "${lsd[@]}" |
            fzf --bind "right:accept,left:execute(cd ..),left:+accept,esc:execute(rm $TMP),esc:+abort,q:abort" --reverse --preview '
                __cd_nxt="$(echo {})";
                __cd_path="$(echo $(pwd)/${__cd_nxt} | sed "s;//;/;")";
                echo $__cd_path;
                echo;
                ls -p --color=always "${__cd_path}";
        ')"
        [[ ${#dir} != 0 ]] || return 0
		echo "${PWD}/$dir" > "$TMP"
        cd "$dir" &> /dev/null
    done
}

fcd

if [ -f "$TMP" ]; then
	cat "$TMP" > "$CLIFM_BUS"
	rm -- "$TMP" 2>/dev/null
fi

exit 0
