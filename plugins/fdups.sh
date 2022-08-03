#!/bin/sh

# Plugin to find/remove duplicate files for CliFM
#
# Usage: fdups.sh [DIR]
#
# Description: List non-empty duplicate files (based on size and followed
# by MD5) in DIR (current directory if omitted) and allow the user to remove
# one or more of them
#
# Dependencies:
#	find md5sum sort uniq xargs sed stat (on Linux/Haiku)
#	gfind md5 sort guniq xargs sed stat (on FreeBSD/NetBSD/OpenBSD/MacOS)
#
# On FreeBSD/NetBSD/OpenBSD/MacOS you need to install both coreutils (for guniq)
# and findutils (for gfind)
#
# Notes:
# If the file size exceeds SIZE_DIGITS digits the file will be misplaced.
# (12 digits amount to sizes up to 931GiB)
#
# Based on https://github.com/jarun/nnn/blob/master/plugins/dups and modified
# to fit our needs
#
# Authors: syssyphus, KlzXS, leo-arch, danfe
# License: GPL3

me="clifm"

OS="$(uname)"

case "$OS" in
Linux|Haiku)
	FIND="find"
	MD5="md5sum"
	UNIQ="uniq"
	STAT="stat -c %Y"
	;;
FreeBSD|NetBSD|OpenBSD|Darwin)
	FIND="gfind"
	if [ "$OS" = "NetBSD" ]; then
		MD5="md5 -n"
	else
		MD5="md5 -r"
	fi
	UNIQ="guniq"
	STAT="stat -f %m"
	;;
*)
	printf "%s: This plugin is not supported on $OS\n" "$me" >&2
	exit 1
esac

no_dep=0

if ! type "$FIND" > /dev/null 2>&1; then
	printf "%s: %s: command not found\n" "$me" "$FIND" >&2; no_dep=1
elif ! type "${MD5%% *}" > /dev/null 2>&1; then
	printf "%s: %s: command not found\n" "$me" "${MD5%% *}" >&2; no_dep=1
elif ! type sort > /dev/null 2>&1; then
	printf "%s: sort: command not found\n" "$me" >&2; no_dep=1
elif ! type "$UNIQ" > /dev/null 2>&1; then
	printf "%s: %s: command not found\n" "$me" "$UNIQ" >&2; no_dep=1
elif ! type xargs > /dev/null 2>&1; then
	printf "%s: xargs: command not found\n" "$me" >&2; no_dep=1
elif ! type sed > /dev/null 2>&1; then
	printf "%s: sed: command not found\n" "$me" >&2; no_dep=1
elif ! type "${STAT%% *}" > /dev/null 2>&1; then
	printf "%s: %s: command not found\n" "$me" "${STAT%% *}" >&2; no_dep=1
fi

[ "$no_dep" = 1 ] && exit 127

if [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
	name="${CLIFM_PLUGIN_NAME:-$(basename "$0")}"
	printf "List non-empty duplicated files in DIR (current directory \
if omitted) and allow the user to selectively delete them\n"
	printf "Usage: %s [DIR]\n" "$name"
	exit 0
fi

if [ -n "$1" ] && ! [ -d "$1" ]; then
	printf "%s: %s: Not a directory\n" "$me" "$1" >&2
	exit 1
fi


dir="${1:-.}"

# The find command below fails when file names contain single quotes
# Let's warn the user
SQF="$($FIND "$dir" -type f -name "*'*")"
if [ -n "$SQF" ]; then
#if $FIND "$dir" -type f -name "*'*"; then
	printf "Warning: Some files in this directory contain single quotes in their names.\n\
Rename them or they will be ignored.\n\n\
TIP: You can use the 'br' command to rename them in bulk:\n\
  s *\\\'*\n\
  br sel\n\n\
Ignore these files and continue? [y/N] "
	read -r answer
	if [ "$answer" != "y" ] && [ "$answer" != "Y" ]; then
		exit 0
	fi
	echo ""
fi

EDITOR="${EDITOR:-nano}"
TMPDIR="${TMPDIR:-/tmp}"
tmp_file=$(mktemp "$TMPDIR/.${me}XXXXXX")
size_digits=12

printf "\
## This is a list of all duplicates found (if empty, just exit).
## Comment out the files you want to remove (lines starting with double number
## sign (##) are ignored.
## Save and close this file to remove commented files (deletion approval will
## be asked before removing files)
## Files can be removed either forcefully or interactively.\n
" > "$tmp_file"

# shellcheck disable=SC2016
$FIND "$dir" -size +0 -type f -printf "%${size_digits}s %p\n" | sort -rn | $UNIQ -w"${size_digits}" -D \
| sed -e "s/^ \{0,12\}\([0-9]\{0,12\}\) \(.*\)\$/printf '%s %s\\\n' \"\$($MD5 '\2')\" 'd\1'/" \
| tr '\n' '\0' | xargs -0 -n1 -r sh -c 2>/dev/null | sort | { $UNIQ -w32 --all-repeated=separate; echo; } \
| sed -ne 'h
s/^\(.\{32\}\).* d\([0-9]*\)$/## md5sum: \1 size: \2 bytes/p
g
:loop
N
/.*\n$/!b loop
p' \
| sed -e 's/^.\{32\} *\(.*\) d[0-9]*$/\1/' >> "$tmp_file"

time_pre="$($STAT "$tmp_file")"

"$EDITOR" "$tmp_file"

time_post="$($STAT "$tmp_file")"

if [ "$time_pre" = "$time_post" ]; then
	printf "%s: Nothing to do\n" "$me"
	exit 0
fi

printf "Note: If you answer is yes, you will be given the option to remove them interactively\n\
Remove commented files? [y/N] "
read -r answer

if [ "$answer" = "y" ] || [ "$answer" = "Y" ]; then
	sedcmd="/^##.*/d; /^[^#].*/d; /^$/d; s/^# *\(.*\)$/\1/"
else
	exit 0
fi

printf "Remove files forcefully or interactively? [f/I] "
read -r force

if [ "$force" = "f" ] || [ "$force" = "F" ]; then
	#shellcheck disable=SC2016
	sed -e "$sedcmd" "$tmp_file" | tr '\n' '\0' | xargs -0 -r sh -c 'rm -f "$0" "$@" </dev/tty'
else
	#shellcheck disable=SC2016
	sed -e "$sedcmd" "$tmp_file" | tr '\n' '\0' | xargs -0 -r sh -c 'rm -i "$0" "$@" </dev/tty'
fi

rm -- "$tmp_file"

exit 0
