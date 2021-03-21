#!/bin/bash

# Interactive help plugin for CliFM
# Written by L. Abramovich

manpage="/usr/share/man/man1/clifm.1.gz"

if ! [[ -f $manpage ]]; then
	echo "CliFM: no manpage found" >&2
	exit 1
fi

finder=""

if [[ $(type -P fzf) ]]; then
	filter="fzf"

elif [[ $(type -P rofi) ]]; then
	filter="rofi"
else
	echo "CliFM: No finder found. Install fzf or rofi" >&2
	exit 1
fi

CMDS=(
	"FILE/DIR@"
	"/PATTERN@"
	";\[CMD\], :\[CMD\]@"
	"ac, ad@"
	"acd, autocd@"
	"actions@"
	"alias@"
	"ao, auto-open@"
	"b, back@"
	"bl@"
	"bm, bookmarks@"
	"br, bulk@"
	"c, l@"
	"cc, colors@"
	"cd@"
	"cl, columns@"
	"cmd, commands@"
	"cs, colorscheme@"
	"ds, desel@"
	"edit@"
	"exp, export@"
	"ext@"
	"f, forth@"
	"fc, filescounter@"
	"ff, folders-first@"
	"ft, filter@"
	"fs@"
	"hf, hidden@"
	"history@"
	"icons@"
	"jump@"
	"kb, keybinds@"
	"log@"
	"lm@"
	"mf@"
	"mm, mime@"
	"mp, mountpoints@"
	"msg, messages@"
	"o, open@"
	"opener@"
	"path, cwd@"
	"pin@"
	"pf, prof, profile@"
	"pg, pager@"
	"p, pr, prop@"
	"q, quit, exit, zz@"
	"Q@"
	"rl, reload@"
	"rf, refresh@"
	"s, sel@"
	"sb, selbox@"
	"shell@"
	"splash@"
	"st, sort@"
	"t, tr, trash@"
	"tips@"
	"u, undel, untrash@"
	"uc, unicode@"
	"unpin@"
	"v, paste@"
	"ver, version@"
    "ws@"
	"x, X@"
)

a="-"

while [[ -n "$a" ]]; do

	if [[ $filter == "fzf" ]]; then
		a="$(echo "${CMDS[*]}" | sed 's/@/\n/g' | fzf --prompt "CliFM> ")"

	else
		a="$(echo "${CMDS[*]}" | sed 's/@/\n/g' | rofi -dmenu -p "CliFM")"
	fi

	if [[ -n "$a" ]]; then
		man -P "less -gp \"      $a\"" "$manpage"
	fi

done

exit 0
