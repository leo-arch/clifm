#!/bin/sh

# Git status plugin for Clifm
# Written by L. Abramovich
# License: GPL2+

# Description: Check if the current directory is inside a git work
# tree, and, if true, print status, i.e., branch name and non
# commited/tracked files

# NOTE: This script is not intended to be used as a normal plugin,
# that is, called via an action name, but rather to be executed as
# a prompt command (see the configuration file)

# Some useful git commands for a more complex output:

#am I inside git repo? [ "$(git rev-parse --is-inside-work-tree)" = "true" ] && echo "Yes"
#current remote/repo: git rev-parse --abbrev-ref @{upstream}
#current repo: git describe --contains --all HEAD
#total upstream commits: git rev-list --count @{upstream}
#last local commit short hash: git rev-parse --short HEAD
#last upstream commit short hash: git rev-parse --short @{upstream}
#Is something stashed: git rev-parse --verify --quiet refs/stash
#Non-commited/tracked local changes: git status | grep -q "nothing to commit" && echo "No" || echo "Yes" or git status -sb

if [ -n "$1" ] && { [ "$1" = "--help" ] || [ "$1" = "-h" ]; }; then
	printf "Print current git status (if in a git repository). This script is intended to be executed as a prompt command. Add the absolute path to this plugin to the PROMPT COMMANDS section in the configuration file.\n"
	exit 0
fi

if [ -n "$CLIFM_COLORLESS" ]; then
	git -c color.status=false status -sb 2>/dev/null
else
	git status -sb 2>/dev/null
fi

exit 0
