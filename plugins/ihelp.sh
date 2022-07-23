#!/bin/sh

# Interactive help plugin for CliFM
# Written by L. Abramovich
# License: GPL3

# Dependencies: man, sed, less, fzf or rofi

if [ -n "$1" ] && { [ "$1" = "--help" ] || [ "$1" = "-h" ]; }; then
	name="${CLIFM_PLUGIN_NAME:-$(basename "$0")}"
	printf "Interactively browse the CliFM manpage via FZF or Rofi\n"
	printf "Usage: %s\n" "$name"
	exit 0
fi

if ! type man > /dev/null 2>&1; then
	printf "clifm: man: Command not found\n" >&2
	exit 127
fi

manpage="$(man -w clifm)"

if ! [ -f "$manpage" ]; then
	printf "clifm: no manpage found\n" >&2
	exit 1
fi

if type fzf >/dev/null 2>&1; then
	filter="fzf"

elif type rofi >/dev/null 2>&1; then
	filter="rofi"
else
	printf "clifm: No finder found. Install either fzf or rofi\n" >&2
	exit 1
fi

tmp=""
if [ -n "$MANPAGER" ]; then
	tmp="$MANPAGER"
	unset MANPAGER
fi

# Source our plugins helper
if [ -z "$CLIFM_PLUGINS_HELPER" ] || ! [ -f "$CLIFM_PLUGINS_HELPER" ]; then
	printf "CliFM: Unable to find plugins-helper file\n" >&2
	exit 1
fi
# shellcheck source=/dev/null
. "$CLIFM_PLUGINS_HELPER"

CMDS="1. GETTING HELP@
2. DESCRIPTION@
3. FEATURES@
4. PARAMETERS@
POSITIONAL PARAMETERS@
OPTIONS@
5. COMMANDS@
6. FILE FILTERS@
7. KEYBOARD SHORTCUTS@
8. THEMING@
9. BUILT-IN EXPANSIONS@
10. RESOURCE OPENER@
11. AUTO-SUGGESTIONS@
12. SHELL FUNCTIONS@
13. PLUGINS@
14. AUTOCOMMANDS@
15. FILE TAGS@
16. VIRTUAL DIRECTORIES@
17. NOTE ON SPEED
18. KANGAROO FRECENCY ALGORITHM@
19. ENVIRONMENT@
20. SECURITY@
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
exp @
ext @
f, forth@
fc, filescounter@
ff, folders-first@
ft, filter@
fz @
hf, hidden@
history @
icons @
j, @
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
ow @
path, cwd@
pin @
pf, prof, profile@
pg, pager@
p, pr, prop@
q, quit, exit, Q@
rl, reload@
rf, refresh@
rr @
s, sel@
sb, selbox@
splash @
st, sort@
stats @
t, tr, trash@
tag @
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

_colors="$(get_fzf_colors)"

# shellcheck disable=SC2046
while [ -n "$a" ]; do
	if [ "$filter" = "fzf" ]; then
		# shellcheck disable=SC2154
		a="$(printf "%s\n" "$CMDS" | sed 's/@//g' | fzf --prompt="$fzf_prompt" --header "Browse CliFM's manpage" --reverse --height="$fzf_height" --info=inline --color="$_colors")"
	else
		a="$(printf "%s\n" "$CMDS" | sed 's/@//g' | rofi -dmenu -p "CliFM")"
	fi

	if [ -n "$a" ]; then
		if echo "$a" | grep -q '^[1-9,A-Z].*'; then
			# shellcheck disable=SC2089
			export PAGER="less -gp \"$a\""
		else
			# shellcheck disable=SC2090
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
