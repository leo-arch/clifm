#!/bin/sh

# FZF navigation plugin for CLiFM

# Taken from https://github.com/junegunn/fzf/wiki/Examples#interactive-cd and adapted to CliFM by L. Abramovich

# License: GPL2+

dir_bk=""

function fcd() {
    if [[ "$#" != 0 ]]; then
        cd "$@";
        return
    fi
    while true; do
        local lsd=$(echo ".." && ls -Ap | grep '/$' | sed 's;/$;;')
        local dir="$(printf '%s\n' "${lsd[@]}" |
            fzf --reverse --preview '
                __cd_nxt="$(echo {})";
                __cd_path="$(echo $(pwd)/${__cd_nxt} | sed "s;//;/;")";
                echo $__cd_path;
                echo;
                ls -p --color=always "${__cd_path}";
        ')"
        [[ ${#dir} != 0 ]] || return 0
		dir_bk="${PWD}/$dir"
        cd "$dir" &> /dev/null
    done
}

fcd

if [ -x "$dir_bk" ]; then
	echo "$dir_bk" > "$CLIFM_BUS"
fi

exit 0
