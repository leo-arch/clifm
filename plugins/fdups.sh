#!/bin/bash

# Plugin to find/remove duplicate files for CliFM
#
# Usage: fdups.sh [DIR]
#
# Description: List non-empty duplicate files (based on size and followed
# by MD5) in DIR (current directory if omitted) and allow the user to remove
# one or more of them
#
# Dependencies: find md5sum sort uniq xargs sed stat
#
# Notes:
# 1. If the file size exceeds SIZE_DIGITS digits the file will be misplaced.
#    (12 digits amount to sizes up to 931GiB)
# 2. Bash compatible shell required for mktemp(1)
#
# Shell: Bash
# Based on https://github.com/jarun/nnn/blob/master/plugins/dups and modified
# to fit our needs
#
# Authors: syssyphus, KlzXS, leo-arch
# License: GPL3

me="clifm"
no_dep=0

if ! [ "$(type -P find 2>/dev/null)" ]; then
	printf "%s: find: command not found\n" "$me"; no_dep=1
elif ! [ "$(type -P md5sum 2>/dev/null)" ]; then
	printf "%s: md5sum: command not found\n" "$me"; no_dep=1
elif ! [ "$(type -P sort 2>/dev/null)" ]; then
	printf "%s: sort: command not found\n" "$me"; no_dep=1
elif ! [ "$(type -P uniq 2>/dev/null)" ]; then
	printf "%s: uniq: command not found\n" "$me"; no_dep=1
elif ! [ "$(type -P xargs 2>/dev/null)" ]; then
	printf "%s: xargs: command not found\n" "$me"; no_dep=1
elif ! [ "$(type -P sed 2>/dev/null)" ]; then
	printf "%s: sed: command not found\n" "$me"; no_dep=1
elif ! [ "$(type -P stat 2>/dev/null)" ]; then
	printf "%s: stat: command not found\n" "$me"; no_dep=1
fi

[ "$no_dep" = 1 ] && exit 127

if [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
	printf "Usage: fdups.sh [DIR]\n"
	printf "List non-empty duplicated files in DIR (current directory \
if omitted) and allow the user to delete one or more of them\n"
	exit 0
fi

if [ -n "$1" ] && ! [ -d "$1" ]; then
	printf "%s: %s: Not a directory\n" "$me" "$1"
	exit 1
fi

dir="${1:-.}"
EDITOR="${EDITOR:-nano}"
TMPDIR="${TMPDIR:-/tmp}"
tmp_file=$(mktemp "$TMPDIR/.${me}XXXXXX")
size_digits=12

printf "\
## This is a list of all duplicates found.
## Comment out the files you want to remove. You will be given the
## chance to cancel the operation.
## Lines with double comments (##) are ignored.
## Files can be removed forcefully or interactively.\n
" > "$tmp_file"

# shellcheck disable=SC2016
find "$dir" -size +0 -type f -printf "%${size_digits}s %p\n" | sort -rn | uniq -w"${size_digits}" -D | sed -e '                                s/^ \{0,12\}\([0-9]\{0,12\}\) \(.*\)$/printf "%s %s\\n" "$(md5sum "\2")" "d\1"/                                                                       ' | tr '\n' '\0' | xargs -0 -n1 -r sh -c | sort | { uniq -w32 --all-repeated=separate; echo; } | sed -ne '
h                                                           
s/^\(.\{32\}\).* d\([0-9]*\)$/## md5sum: \1 size: \2 bytes/p
g    
:loop
N             
/.*\n$/!b loop                               
p' | sed -e 's/^.\{32\}  \(.*\) d[0-9]*$/\1/' >> "$tmp_file"

time_pre="$(stat -c '%Y' "$tmp_file")"

"$EDITOR" "$tmp_file"

time_post="$(stat -c '%Y' "$tmp_file")"

if [ "$time_pre" = "$time_post" ]; then
	printf "%s: Nothing to do\n" "$me"
	exit 0
fi

printf "Remove commented files? [y/N] "
read -r answer

if [ "$answer" = "y" ] || [ "$answer" = "Y" ]; then
	sedcmd="/^##.*/d; /^[^#].*/d; /^$/d; s/^# *\(.*\)$/\1/"
else
	exit 0
fi

printf "Remove forcefully or interactively? [f/I] "
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
