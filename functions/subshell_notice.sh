# shellcheck shell=sh

# Description: Change the shell prompt to notify the user that this
# shell has been spawned from within CliFM

# Copy the following line or source this file from your shell
# configuration file (ex: .bashrc, .zshrc, etc)

[ -n "$CLIFM" ] && PS1="$PS1"'(clifm) '
