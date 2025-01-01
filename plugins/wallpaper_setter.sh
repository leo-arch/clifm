#!/bin/sh

# Wallpaper setter plugin for Clifm
# Dependencies:
#  file, sed, and
#    feh, xsetbg (xloadimage), hsetroot, or nitrogen (for X)
#    swww (https://github.com/Horus645/swww) or swaybg (for Wayland)

# Written by L. Abramovich
# License: GPL2+

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

file="$(echo "$1" | sed 's/\\//g')"

if ! file -bi "$file" | grep -q "image/"; then
	printf "clifm: %s: Not an image file\n" "$file" >&2
	exit 1
fi

if [ -n "$WAYLAND_DISPLAY" ]; then
	if type swww >/dev/null 2>&1; then
		swww img "$file" && exit 0
	elif type swaybg >/dev/null 2>&1; then
		killall swaybg 2>/dev/null
		swaybg -m fill -i "$file" 2>/dev/null &
		exit 0
	fi
else
	if type feh >/dev/null 2>&1; then
		feh --no-fehbg --bg-fill "$file" && exit 0
	elif type xsetbg >/dev/null 2>&1; then
		xsetbg -center "$file" && exit 0
	elif type hsetroot >/dev/null 2>&1; then
		hsetroot -center "$file" && exit 0
	elif type nitrogen >/dev/null 2>&1; then
		nitrogen --set-zoom-fill --save "$file" && exit 0
	fi
fi

printf "clifm: Could not set wallpaper\n" >&2
exit 1
