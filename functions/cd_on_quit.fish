# CliFM CD on quit

# Description
# Run CliFM and, if the exit command is "Q" (not "q"), read the .last file and cd into it.
#
# 1) Customize this function as needed and copy it into your fish functions directory as a .fish file with the same name.
#    For example, with the default name, the file should be c.fish.
# 2) Run clifm using the name of the function below: c [ARGS ...]

# Dependecies: clifm, cut, grep

# Author: Spenser Black
# License: GPL3

function c --wraps=clifm --description="Change directory after exiting clifm"
    clifm "--cd-on-quit" $argv
    set --query XDG_CONFIG_HOME; and set --function config_home "$XDG_CONFIG_HOME"; or set --function config_home "$HOME/.config"
    set --function dir "$(grep "^\*" "$config_home/clifm/.last" 2>/dev/null | cut -d':' -f2)"
    if test "$dir"
        cd -- "$dir"; or return 1
    end
end
