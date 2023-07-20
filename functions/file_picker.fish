# Copy this into $HOME/.config/fish/functions/p.fish
# Note that conventionally fish functions are named after the file they are in.
# This works best with the fish built-in funced, funcsave, etc.
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
