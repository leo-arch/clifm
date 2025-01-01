#!/bin/sh

# Description: Check clifm's upstream version
#
# Dependencies: awk, curl, grep
#
# Written by L. Abramovich
# License GPL2+

if [ -n "$1" ] && { [ "$1" = "--help" ] || [ "$1" = "-h" ]; }; then
	name="${CLIFM_PLUGIN_NAME:-$(basename "$0")}"
	printf "Check Clifm's upstream version\n"
	printf "\n\x1b[1mUSAGE\x1b[0m\n  %s\n" "$name"
	exit 0
fi

if ! type clifm >/dev/null 2>&1 ; then
	printf "Clifm is not installed\n" >&2
	exit 1
fi

upstream="$(curl -s "https://api.github.com/repos/leo-arch/clifm/releases/latest" | grep tag_name | awk -F'"' '{print $4}')"

if [ -z "$upstream" ]; then
	printf "clifm: Error fetching upstream version" >&2
	exit 1
fi

cur="$(clifm -v)"

if [ "v$cur" = "$upstream" ]; then
	printf "Clifm is up to date: %s is the latest release\n" "$cur"
else
	printf "%s: New release available (current version is %s)\n" "$upstream" "$cur"

	OS="$(uname -s)"
	if [ "$OS" != "Linux" ] && [ "$OS" != "FreeBSD" ] && [ "$OS" != "NetBSD" ] \
	&& [ "$OS" != "OpenBSD" ] && [ "$OS" != "DragonFly" ] && [ "$OS" != "Darwin" ]; then
		printf "\nTo manually build the latest release consult the documentation at\n\
https://github.com/leo-arch/clifm/wiki/Introduction#installation\n"
		exit 0
	fi

	sudo_cmd="sudo"
	[ "$OS" = "OpenBSD" ] && sudo_cmd="doas"

	printf "\nIf not provided by your package manager, you can build the latest\n\
release as follows:\n\n\
1) Clone the latest release:\n\
 git clone https://github.com/leo-arch/clifm --depth=1\n\
2) cd into the clifm directory:\n\
 cd clifm\n\
3) Build and install:\n\
 %s make install\n" "$sudo_cmd"
fi

exit 0
