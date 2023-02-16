#!/bin/sh

# Wallpaper setter plugin for CliFM
# Dependencies:
#  file, and
#    feh, xsetbg (xloadimage), hsetroot, or nitrogen (for X)
#    swww (https://github.com/Horus645/swww) or swaybg (for Wayland)

# Written by L. Abramovich
# License: GPL3

if [ -z "$1" ] || [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
	name="${CLIFM_PLUGIN_NAME:-$(basename "$0")}"
	printf "Set IMAGE as wallpaper\n"
	printf "\n\x1b[1mUSAGE\x1b[0m\n  %s IMAGE\n" "$name"
	exit 0
fi

if [ -z "$DISPLAY" ]; then
	printf "clifm: A graphical environment is required\n" >&2
	exit 1
fi


if ! type file >/dev/null 2>&1; then
	printf "clifm: file: Command not found\n" >&2
	exit 127
fi

if ! file -bi "$1" | grep -q "image/"; then
	printf "clifm: %s: Not an image file\n" "$1" >&2
	exit 1
fi

if [ -n "$WAYLAND_DISPLAY" ]; then
	if type swww >/dev/null 2>&1; then
		swww img "$1" && exit 0
	elif type swaybg >/dev/null 2>&1; then
		killall swaybg 2>/dev/null
		swaybg -m fill -i "$1" 2>/dev/null &
		exit 0
	fi
else
	if type feh >/dev/null 2>&1; then
		feh --no-fehbg --bg-fill "$1" && exit 0
	elif type xsetbg >/dev/null 2>&1; then
		xsetbg -center "$1" && exit 0
	elif type hsetroot >/dev/null 2>&1; then
		hsetroot -center "$1" && exit 0
	elif type nitrogen >/dev/null 2>&1; then
		nitrogen --set-zoom-fill --save "$1" && exit 0
	fi
fi

printf "clifm: Could not set wallpaper\n" >&2
exit 1
