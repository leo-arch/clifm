#!/bin/sh

# Interactive help plugin for Clifm
# Written by L. Abramovich
# License: GPL2+

# Dependencies: man, sed, less, and fzf or rofi

if [ -n "$1" ] && { [ "$1" = "--help" ] || [ "$1" = "-h" ]; }; then
	name="${CLIFM_PLUGIN_NAME:-$(basename "$0")}"
	printf "Interactively browse the CliFM manpage via FZF or Rofi\n"
	printf "\n\x1b[1mUSAGE\x1b[0m\n  %s\n" "$name"
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
3. PARAMETERS@
POSITIONAL PARAMETERS@
OPTIONS@
4. COMMANDS@
5. FILE FILTERS@
6. KEYBOARD SHORTCUTS@
7. THEMING@
8. BUILT-IN EXPANSIONS@
9. RESOURCE OPENER@
10. SHOTGUN@
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
auto @
b, back@
bb, bleach@
bd @
bl @
bm, bookmarks@
br, bulk@
c, l@
colors@
cd @
cl, columns@
cmd, commands@
config @
cs, colorschemes@
d, dup@
ds, desel@
exp @
ext @
f, forth@
fc @
ff, dirs-first@
ft, filter@
fz @
hf, hh, hidden@
history @
icons @
j @
k@
kk@
kb, keybinds@
log @
ll, lv@
lm @
media@
mf @
mm, mime@
mp, mountpoints@
msg, messages@
n, new@
net \[@
o, open@
oc @
opener @
ow @
pc @
pin @
pf, profile@
pg, pager@
p, prop@
prompt@
q, quit, exit, Q@
rl, reload@
rf, refresh@
rr @
s, sel@
sb, selbox@
splash @
st, sort@
stats @
t, trash@
tag @
te @
tips @
u, undel, untrash@
unpin @
v, paste@
vv @
view@
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
