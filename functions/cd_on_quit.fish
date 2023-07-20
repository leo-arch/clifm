# Copy this into $HOME/.config/fish/functions/c.fish
# Note that conventionally fish functions are named after the file they are in.
# This works best with the fish built-in funced, funcsave, etc.
function c --wraps=clifm --description="Change directory after exiting clifm"
    clifm "--cd-on-quit" $argv
    set --query XDG_CONFIG_HOME; and set --function config_home "$XDG_CONFIG_HOME"; or set --function config_home "$HOME/.config"
    set --function dir "$(grep "^\*" "$config_home/clifm/.last" 2>/dev/null | cut -d':' -f2)"
    if test "$dir"
        cd -- "$dir"; or return 1
    end
end
