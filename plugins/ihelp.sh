#!/bin/sh

# Interactive help plugin for CliFM
# Written by L. Abramovich

if [ -n "$1" ] && { [ "$1" = "--help" ] || [ "$1" = "help" ]; }; then
	name="$(basename "$0")"
	printf "Interactively browse the CliFM manpage via FZF or Rofi\n"
	printf "Usage: %s\n" "$name"
	exit 0
fi

if ! type man > /dev/null 2>&1; then
	printf "CliFM: man: Command not found\n" >&2
	exit 1
fi

manpage="$(man -w clifm)"

if ! [ -f "$manpage" ]; then
	printf "CliFM: no manpage found\n" >&2
	exit 1
fi

#finder=""

if [ "$(which fzf 2>/dev/null)" ]; then
	filter="fzf"

elif [ "$(which rofi 2>/dev/null)" ]; then
	filter="rofi"
else
	printf "CliFM: No finder found. Install either fzf or rofi\n" >&2
	exit 1
fi

tmp=""
if [ -n "$MANPAGER" ]; then
	tmp="$MANPAGER"
	unset MANPAGER
fi

CMDS="
1. GETTING HELP@
2. DESCRIPTION@
3. RATIONALE@
4. POSITIONAL PARAMETERS@
5. OPTIONS@
6. COMMANDS@
7. FILES FILTERS@
8. KEYBOARD SHORTCUTS@
9. COLOR CODES@
10. THE PROMPT@
11. BUILT-IN EXPANSIONS@
12. RESOURCE OPENER@
13. AUTO-SUGGESTIONS@
14. SHELL FUNCTIONS@
15. PLUGINS@
16. AUTCOMMANDS@
17. STANDARD INPUT@
18. NOTE ON SPEED
19. KANGAROO FRECENCY ALGORITHM@
20. ENVIRONMENT@
21. MISCELLANEOUS NOTES@
22. FILES@
23. EXAMPLES@
FILE/DIR@
/PATTERN@
;\[CMD\], :\[CMD\]@
ac, ad@
acd, autocd@
actions @
alias @
ao, auto-open@
b, back@
bb, bleach@
bd @
bl @
bm, bookmarks@
br, bulk@
c, l@
cc, colors@
cd @
cl, columns@
cmd, commands@
cs, colorscheme@
d, dup@
ds, desel@
edit @
exp, export@
ext @
f, forth@
fc, filescounter@
ff, folders-first@
ft, filter@
fs @
hf, hidden@
history @
icons @
j, jc, @
kb, keybinds@
log @
lm @
media@
mf @
mm, mime@
mp, mountpoints@
msg, messages@
n, new@
net \[@
o, open@
opener @
path, cwd@
pin @
pf, prof, profile@
pg, pager@
p, pr, prop@
q, quit, exit, Q@
rl, reload@
rf, refresh@
s, sel@
sb, selbox@
shell @
splash @
st, sort@
t, tr, trash@
te @
tips @
u, undel, untrash@
uc, unicode@
unpin @
v, vv, paste@
ver, version@
ws@
x, X@"

a="-"

# shellcheck disable=SC2046
while [ -n "$a" ]; do
	if [ "$filter" = "fzf" ]; then
		a="$(printf "%s\n" "$CMDS" | sed 's/@//g' | fzf --prompt "CliFM> " --layout=reverse-list)"
	else
		a="$(printf "%s\n" "$CMDS" | sed 's/@//g' | rofi -dmenu -p "CliFM")"
	fi

	if [ -n "$a" ]; then
		if echo "$a" | grep -q '^[1-9].*'; then
			export PAGER="less -gp \"$a\""
		else
			export PAGER="less -gp \"  $a\""
		fi
		man clifm
	fi
done

printf "\n"

if [ -n "$tmp" ]; then
	export MANPAGER="$tmp"
fi

exit 0
