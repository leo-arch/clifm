# File picker function for CliFM

# Usage (assuming the function is not renamed): p [ARGS ...]
#
# 1. CliFM will be executed: select the files you need and quit.
# 2. You will be asked for a command to execute over selected files,
# for example: ls -l
#
# Customize this function as you need and copy it to your fish functions directory.
# Note that conventionally fish functions are named after the file they are in, so
# with the name below, the file should be named p.fish.

# Dependecies: clifm, sed, find, xargs

# Author: Spenser Black
# License: GPL3

function p --wraps=clifm --description="Execute a command on the files selected using CliFM"
    set --function clifm_selfile (mktemp "/tmp/clifm_selfile.XXXXXXXXXX")

    clifm $argv --sel-file="$clifm_selfile"

    # NOTE: -s returns true if the size of the file is greater than zero
    if test -s "$clifm_selfile"
        set --function cmd ""
        while test -z "$cmd"
            read --function --prompt-str "Enter command ('q' to quit): " cmd
        end
        test "$cmd" = "q"; and return
        sed 's/ /\\ /g' "$clifm_selfile" | xargs $cmd
        rm -- "$clifm_selfile"
    else
        printf "clifm: No selected files\n" >&2
    end
end
