#!/bin/sh

# Interactive help plugin for CliFM
# Written by L. Abramovich
# License: GPL3

if [ -n "$1" ] && { [ "$1" = "--help" ] || [ "$1" = "help" ]; }; then
	name="${CLIFM_PLUGIN_NAME:-$(basename "$0")}"
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

if type fzf >/dev/null 2>&1; then
	filter="fzf"

elif type rofi >/dev/null 2>&1; then
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
4. POSITIONAL PARAMETERS@
5. OPTIONS@
6. COMMANDS@
7. FILE FILTERS@
8. KEYBOARD SHORTCUTS@
9. THEMING@
10. BUILT-IN EXPANSIONS@
11. RESOURCE OPENER@
12. AUTO-SUGGESTIONS@
13. SHELL FUNCTIONS@
14. PLUGINS@
15. AUTOCOMMANDS@
16. FILE TAGS@
17. STANDARD INPUT@
18. NOTE ON SPEED
19. KANGAROO FRECENCY ALGORITHM@
20. ENVIRONMENT@
21. SECURITY@
22. MISCELLANEOUS NOTES@
23. FILES@
24. EXAMPLES@
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
fs @
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
		if echo "$a" | grep -q '^[1-9].*'; then
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
