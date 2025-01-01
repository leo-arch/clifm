#!/bin/sh

# Clifm plugin to copy file(s) into multiple destinations
# Written by L. Abramovich
# License: GPL2+

# Description: Copy files passed as arguments to files specified via
# a text editor

# Dependencies: grep, xargs, text editor

if [ -z "$1" ] || [ "$1" = "--help" ]; then
	name="${CLIFM_PLUGIN_NAME:-$(basename "$0")}"
	printf "Copy files into multiple directories at once\n"
	printf "\n\x1b[1mUSAGE\x1b[0m\n  %s FILE...\n" "$name"

	printf "\n\x1b[1mEXAMPLE\x1b[0m\n- Copy file1 and file2 into dir1 and dir2 at once\n\
    %s file1 file2\n\n  Once in the editor, enter dir1 and dir2, save, and exit.\n" "$name"
	exit 0
fi

DEST=$(mktemp "${TMPDIR:-/tmp}/clifm_bcd.XXXXXX")

printf "# CliFM - Copy files in batch\n\
# Write here destinty files/directories, one per line.\n\
# Blank and commented lines are omitted.\n\
# Just quit the editor to cancel the operation.\n" > "$DEST"

"${EDITOR-:nano}" "$DEST"

if ! grep -qv "^$\|^#" "$DEST"; then
	rm -f -- "$DEST"
	exit 0
fi

for file in "$@"; do
	grep -v "^$\|^#" "$DEST" | xargs -n1 cp -v "$file"
done

rm -f -- "$DEST"

exit 0
