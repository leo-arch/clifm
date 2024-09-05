#!/usr/bin/env fish
# Install Clifm extension functions

# Description
# Execute this script to install the provided fish functions to the functions directory.
# Note that this will use the default, single-letter function names.

# Usage: install-extensions.fish [FUNCTIONS]
#
# FUNCTIONS: A list of the filenames of the functions to install.
#            For example, install-extensions cd_on_quit will install
#            the function with the default name c to the functions directory.
#            If not provided, all functions will be installed.

# Author: Spenser Black
# License: GPL3

set -l dir (dirname (status -f))
argparse --name=install-extensions 'h/help' -- $argv

if test $_flag_h
    echo "Usage: install-extensions.fish [FUNCTIONS]"
    echo ""
    echo "FUNCTIONS: A list of the filenames of the functions to install."
    echo "           For example, install-extensions cd_on_quit will install"
    echo "           the function with the default name c to the functions directory."
    echo "           If not provided, all functions will be installed."
    exit 0
end

set -l sources cd_on_quit file_picker
set -l destinations c p

test (count $argv) -eq 0; and set -l extensions $sources; or set -l extensions $argv

for source in $sources
    set -l index (contains -i -- $source $extensions)
    set -l destination $destinations[$index]
    cp "$dir/$source.fish" "$__fish_config_dir/functions/$destination.fish"
end
