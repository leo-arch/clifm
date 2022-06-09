#!/bin/sh

# Wallpaper setter plugin for CliFM
# Written by L. Abramovich
# License: GPL3

# Dependencies: file, and feh, xsetbg (xloadimage), or hsetroot

if [ -z "$1" ] || [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
	name="${CLIFM_PLUGIN_NAME:-$(basename "$0")}"
	printf "Set IMAGE as wallpaper\n"
	printf "Usage: %s IMAGE\n" "$name"
	exit 0
fi


if ! type file >/dev/null 2>&1; then
	printf "clifm: file: Command not found\n" >&2
	exit 127
fi

if ! file -bi "$1" | grep -q "image/"; then
	printf "clifm: %s: Not an image file\n" "$1" >&2
	exit 1
fi

if type feh >/dev/null 2>&1; then
	if feh --no-fehbg --bg-fill "$1"; then
		exit 0
	fi
elif type xsetbg >/dev/null 2>&1; then
	if xsetbg -center "$1"; then
		exit 0
	fi
elif type hsetroot >/dev/null 2>&1; then
	if hsetroot -center "$1"; then
		exit 0
	fi
else
	printf "CliFM: No wallpaper setter found. Install either feh, \
xloadimage, or hsetroot\n" >&2
	exit 1
fi

exit 1
