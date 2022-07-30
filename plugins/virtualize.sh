#!/bin/sh

# Virtual directories plugin for CliFM
# Author: L. Abramovich
# License: GPL3

# Dependencies: sed

# Note: A new instance of CliFM will be spawned on a new terminal window
# using $term_cmd, which defaults to 'xterm -e'. Edit this variable to
# use your favorite terminal emulator instead, for example:
# term_cmd="kitty sh -c"

term_cmd="xterm -e"
clifm_bin="clifm"
clifm_opts=""

if [ -z "$1" ] || [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
	name="${CLIFM_PLUGIN_NAME:-$(basename "$0")}"
	printf "Create a virtual directory for a set or collection of files\n\
Usage: %s [-d] [FILE...] \n\
\n\
Examples:\n\
  'vt sel'          Send all selected files to a virtual directory\n\
  'vt file1 file2'  Send file1 and file2 to a virtual directory\n\
  'vt -d'           If navigating the file system, use the -d option to quickly go back to the virtual directory\n" "$name" >&2

	exit 0
fi

if ! type sed > /dev/null 2>&1; then
	printf "clifm: sed: Command not found\n" >&2
	exit 127
fi

if [ -n "$CLIFM_VT_RUNNING" ]; then
	if [ -n "$1" ] && [ "$1" = "-d" ]; then
		if [ -n "$CLIFM_VIRTUAL_DIR" ]; then
			echo "$CLIFM_VIRTUAL_DIR" > "$CLIFM_BUS"
			exit 0
		fi
	fi

	printf "clifm: Only one virtual directory can be created at a time\n" >&2
	exit 1
fi

if [ -n "$1" ] && [ "$1" = "-d" ]; then
	printf "clifm: No virtual directory found\n" >&2
	exit 1
fi

export CLIFM_VT_RUNNING=1

if [ -n "$CLIFM_VIRTUAL_DIR" ]; then
	clifm_opts="$clifm_opts --virtual-dir=\"$CLIFM_VIRTUAL_DIR\""
fi

# 1. Replace escaped spaces by tabs
# 2. Replace non-escaped spaces by new line chars
# 3. Replace tabs (first step) by spaces
# 4. Remove remaining escape chars
files="$(echo "$*" | sed 's/\\ /\t/g;s/ /\n/g;s/\t/ /g;s/\\//g')"
cmd="$term_cmd 'echo \"$files\" | $clifm_bin $clifm_opts --no-restore-last-path'"

eval "$cmd"

exit 0
