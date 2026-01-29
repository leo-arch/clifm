/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* translate_key.c -- Translate keyboard escape sequences into text form */

#include <stdlib.h> /* malloc, free, strtol */
#include <stdio.h>  /* snprintf */
#include <string.h> /* strlen, strcmp, strchr, memcpy, memset */
#include <limits.h> /* INT_MIN, INT_MAX */
#include <ctype.h>  /* toupper() */
#include <errno.h>  /* ENOMEM */

#include "translate_key.h"

/* See https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h3-PC-Style-Function-Keys */

/* When it comes to keyboard escape sequences, we have three kind of
 * terminating characters:
 *
 * 1. Defining the keycode. E.g. 'CSI 1;2D', where 'D' means the key pressed,
 * (Left), and '2' the modifier key (Shift).
 *
 * 2. Defining the modifier key. E.g.: 'CSI 11^', where '^' means the
 * modifier key (Control) and '11' the key pressed (F1).
 *
 * 3. Raw sequence terminator. E.g. 'CSI 15;3~', where '~' simply
 * ends the sequence, '15' is the pressed key (F5), and '3' the modifier
 * key (Alt). Under this category we also find Xterm's MOK (modifyOtherKeys)
 * and the Kitty keyboard protocol. */

/* This translation module supports the following encoding schemes:
 * 1. SCO (legacy)
 * 2. HP (legacy)
 * 3. Sun (CSI-z)
 * 4. Xterm
 * 5. Xterm (modifyOtherKeys)
 * 6. Rxvt
 * 7. Fixterms (CSI-u)
 * 8. Kitty (extended CSI-u)
 * 9. Foot (extended CSI-u)
 *
 * Also, it can handle quirks/execeptions via the exceptions struct. */

#define IS_DIGIT(c)            ((c) >= '0' && (c) <= '9')
#define IS_UTF8_LEAD_BYTE(c)   (((c) & 0xc0) == 0xc0)
#define IS_LOWER_ARROW_CHAR(c) ((c) >= 'a' && (c) <= 'd')
#define IS_UPPER_ARROW_CHAR(c) ((c) >= 'A' && (c) <= 'D')
#define IS_ARROW_CHAR(c) (IS_LOWER_ARROW_CHAR((c)) || IS_UPPER_ARROW_CHAR((c)))

/* Xterm, with modifyOtherKeys resource enabled */
#define IS_XTERM_MOK_SEQ(s, end) (*(s) == '2' && (s)[1] == '7' \
	&& (s)[2] == ';' && (end) == '~')
/* The kitty keyboard protocol */
#define IS_KITTY_END_CHAR(c)   ((c) == 'u')

#define IS_MODKEY_END_CHAR(c)  ((c) == '^' || (c) == '$' || (c) == '@')
/* ~: Xterm & others, z: SUN. */
#define IS_GENERIC_END_CHAR(c) ((c) == '~' || (c) == 'z')
#define IS_KEYCODE_END_CHAR(c) (  \
	IS_ARROW_CHAR((c))         || \
	((c) >= 'E' && (c) <= 'I') || \
	((c) >= 'P' && (c) <= 'S') || \
	((c) >= 'j' && (c) <= 'y') || \
	(c) == 'M' || (c) == 'X')

#define ESC_KEY 0x1b

/* Values for modifier keys.
 * See https://en.wikipedia.org/wiki/ANSI_escape_code */
#define MOD_SHIFT 1
#define MOD_ALT   2
#define MOD_CTRL  4
/* Unused
#define MOD_SUPER 8
#define MOD_HYPER 16
#define MOD_META  32 */

/* Some names for control keys. */
static const char *ctrl_keys[256] = {
	[0x7f] = "Delete", [0x0d] = "Return", [0x08] = "Backspace",
	[0x09] = "Tab", [0x20] = "Space", [0x1b] = "Escape"
};

/* VT100/SCO sequences predates ANSI X3.64 and ECMA-48, just as Xterm and Rxvrt
 * standardized sequences. They're emitted by the FreeBSD console, cons25
 * (DragonFly, for example), and Xterm in SCO keyboard mode ('xterm -kt sco').
 *
 * The format is: "CSI n", where n is a single byte.
 *
 * Modifier keys are not encoded in any way. Instead, the final CSI byte
 * represents both the key and the modifier.
 *
 * See https://github.com/freebsd/freebsd-src/blob/fb37e38fbe99039a479520b4b596f4bfc04e2a88/usr.sbin/kbdcontrol/kbdcontrol.c
 *     https://vt100.net/docs/vt510-rm/chapter6.html
 *     https://www.invisible-island.net/xterm/xterm-function-keys.html
 * and keyboard(4) in FreeBSD/Dragonfly. */
static const char *key_map_sco[256] = {
	['A'] = "Up", ['B'] = "Down", ['C'] = "Right", ['D'] = "Left",

	['E'] = "Begin", ['F'] = "End", ['G'] = "PageDown", ['H'] = "Home",
	['I'] = "PageUp", ['L'] = "Insert",
	['J'] = "LSuper", ['~'] = "RSuper", ['}'] = "Menu",

	['M'] = "F1", ['N'] = "F2", ['O'] = "F3", ['P'] = "F4", ['Q'] = "F5",
	['R'] = "F6", ['S'] = "F7", ['T'] = "F8", ['U'] = "F9", ['V'] = "F10",
	['W'] = "F11", ['X'] = "F12",

	['Y'] = "Shift+F1", ['Z'] = "Shift+F2", ['a'] = "Shift+F3",
	['b'] = "Shift+F4", ['c'] = "Shift+F5", ['d'] = "Shift+F6",
	['e'] = "Shift+F7", ['f'] = "Shift+F8", ['g'] = "Shift+F9",
	['h'] = "Shift+F10", ['i'] = "Shift+F11", ['j'] = "Shift+F12",

	['k'] = "Ctrl+F1", ['l'] = "Ctrl+F2", ['m'] = "Ctrl+F3",
	['n'] = "Ctrl+F4", ['o'] = "Ctrl+F5", ['p'] = "Ctrl+F6",
	['q'] = "Ctrl+F7", ['r'] = "Ctrl+F8", ['s'] = "Ctrl+F9",
	['t'] = "Ctrl+F10", ['u'] = "Ctrl+F11",  ['v'] = "Ctrl+F12",

	['w'] = "Ctrl+Shift+F1", ['x'] = "Ctrl+Shift+F2", ['y'] = "Ctrl+Shift+F3",
	['z'] = "Ctrl+Shift+F4", ['@'] = "Ctrl+Shift+F5", ['['] = "Ctrl+Shift+F6",
	['\\'] = "Ctrl+Shift+F7", [']'] = "Ctrl+Shift+F8", ['^'] = "Ctrl+Shift+F9",
	['_'] = "Ctrl+Shift+F10", ['`'] = "Ctrl+Shift+F11",
	['{'] = "Ctrl+Shift+F12"
};

static const char *key_map_hp[256] = {
	['h'] = "Home",

	['p'] = "F1", ['q'] = "F2", ['r'] = "F3", ['s'] = "F4", ['t'] = "F5",
	['u'] = "F6", ['v'] = "F7", ['w'] = "F8",

	['A'] = "Up", ['B'] = "Down", ['C'] = "Right", ['D'] = "Left",

	['F'] = "End", ['J'] = "Clear", ['P'] = "Delete", ['Q'] = "Insert",
	['S'] = "PageDown", ['T'] = "PageUp"
};

static const char *key_map_generic[256] = {
	/* According to Rxvt docs: 1:Find, 3:Execute (but also Delete), 4:Select
	 * However, 4 is End and 1 is Home in the Linux console and
	 * cygwin/tmux/screen. */
	[1] = "Home", [4] = "End",

	[2] = "Insert", [3] = "Delete",
	[5] = "PageUp", [6] = "PageDown", [7] = "Home", [8] = "End",
	/* 9 and 10 are normally not used. Some mappings found (on rare/old terms):
	9: F0, F3, F33, F34, and Clear
	10: F4, F10, F34, and F35 */
	[11] = "F1", [12] = "F2", [13] = "F3",  [14] = "F4", [15] = "F5",
	[17] = "F6", [18] = "F7", [19] = "F8", [20] = "F9", [21] = "F10",
	/* In Rxvt, Shift+[F1,F2] generates the same sequence as F11-F12.
	 * There's nothing we can do about it. */
	[23]= "F11", [24] = "F12",

	/* In Rxvt, these integers are mapped to either a function key above
	 * F12, or to the shifted number - 10. E.g., 25 is both F13 and Shift+F3.
	 * See https://pod.tst.eu/http://cvs.schmorp.de/rxvt-unicode/doc/rxvt.7.pod#Key_Codes
	 * We chose the Shift approach, since Shift+F3 is much more intuitive than
	 * F13. */
	[25] = "Shift+F3", [26] = "Shift+F4", [28] = "Shift+F5",
	[31] = "Shift+F7", [32] = "Shift+F8",
	[33] = "Shift+F9", [34] = "Shift+F10",
	[29]= "Menu",
	/* In Rxvt, Shift+F6 and Menu generate the same sequence. Again, there's
	 * nothing we can do about it. */

	[42] = "F21", [43] = "F22", [44] = "F23", [45] = "F24", [46] = "F25",
	[47] = "F26", [48] = "F27", [49] = "F28", [50] = "F29", [51] = "F30",
	[52] = "F31", [53] = "F32", [54] = "F33", [55] = "F34", [56] = "F35",
	[57] = "F36", [58] = "F37", [59] = "F38", [60] = "F39", [61] = "F40",
	[62] = "F41", [63] = "F42",
	/* See https://invisible-island.net/ncurses/terminfo.src.html#tic-xterm-new */

	['A'] = "Up", ['B'] = "Down", ['C'] = "Right", ['D'] = "Left",

	/* Rxvt */
	/* See https://cvs.schmorp.de/rxvt-unicode/doc/rxvt.7.pod */
	['a'] = "Up", ['b'] = "Down", ['c'] = "Right", ['d'] = "Left",
	['j'] = "KP_Multiply", ['k'] = "KP_Add", ['l'] = "KP_Separator",
	['m'] = "KP_Subtract", ['M'] = "KP_Enter", ['n'] = "KP_Delete",
	['o'] = "KP_Divide",
	['p'] = "KP_Insert", ['q'] = "KP_End", ['r'] = "KP_Down", ['s'] = "KP_PageDown",
	['t'] = "KP_Left", ['u'] = "KP_Begin", ['v'] = "KP_Right", ['w'] = "KP_Home",
	['x'] = "KP_Up", ['y'] = "KP_PageUp", ['X'] = "KP_Equal",
	/* Found in old VT keyboards (VT52, VT220) */
	['I'] = "KP_Tab",

	/* Xterm */
	['E'] = "Begin", ['F'] = "End", ['H'] = "Home",
	['P'] = "F1", ['Q'] = "F2", ['R'] = "F3", ['S'] = "F4",

	/* Linux console */
	['G'] = "Begin",

	/* Sun/Solaris */
	[192] = "F11", [193] = "F12", [194] = "F13", [195] = "F14", [196] = "F15",
	[198] = "F17", [199] = "F18", [200] = "F19", [201] = "F20",
	[208] = "F31", [209] = "F32", [210] = "F33", [211] = "F34", [212] = "F35",
	[213] = "F36",
	[215] = "F38", [217] = "F40", [219] = "F42", [221] = "F44",
	[234] = "F46", [235] = "F47",
	/* According to terminfo, kcpy is "CSI 197z", while kf16 is "CSI 29~" */
	[197] = "Menu",
/*  These ones conflict with the above function keys
 * 	[195] = "Undo", [196] = "Help", [200] = "Find", */
	[214] = "Home", [216] = "PageUp", [218] = "Begin", [220] = "End",
	[222] = "PageDown",
	[224] = "F1", [225] = "F2", [226] = "F3", [227] = "F4", [228] = "F5",
	[229] = "F6", [230] = "F7", [231] = "F8", [232] = "F9", [233] = "F10"
};

struct ext_keymap_t {
	const char *name;
	int code;
};

/* An extended list of key symbols and their corresponding key codes.
 * This includes control characters, just as XTerm, Kitty, and Foot
 * extended keys. */
static const struct ext_keymap_t ext_keymap[] = {
    {"NULL", 0}, {"SOH", 1}, {"STX", 2}, {"ETX", 3}, {"EOT", 4},
    {"ENQ", 5}, {"ACK", 6}, {"BELL", 7}, {"Backspace", 8}, {"Tab", 9},
    {"LF", 10}, {"VT", 11}, {"FF", 12}, {"Return", 13}, {"SO", 14},
    {"SI", 15}, {"DLE", 16}, {"DC1", 17}, {"DC2", 18}, {"DC3", 19},
    {"DC4", 20}, {"NAK", 21}, {"SYN", 22}, {"ETB", 23}, {"CAN", 24},
    {"EM", 25}, {"SUB", 26}, {"Escape", 27}, {"FS", 28}, {"GS", 29},
    {"RS", 30}, {"US", 31}, {"Space", 32}, {"Delete", 127},
    {"NBSP", 160}, {"SHY", 173},

    /* Xterm special keys */
    {"AudioLowerVolume", 24849}, {"AudioRaiseVolume", 24851},
    {"AudioMute", 24850}, {"AudioStop", 24853}, {"AudioPrev", 24854},
    {"AudioPlay", 24852}, {"AudioNext", 24855},
    {"Tools", 24961}, {"Mail", 24857}, {"HomePage", 24856},
    {"Search", 24859}, {"Calculator", 24861},

    {"Tab", 57632},
    {"Left", 57937}, {"Up", 57938}, {"Right", 57939}, {"Down", 57940},
    {"Insert", 57955}, {"Home", 57936}, {"End", 57943}, {"PageUp", 57941},
    {"PageDown", 57942}, {"Menu", 57959}, {"PrintScreen", 57953},

    {"KP_Enter", 57997},
    {"KP_0", 58014}, {"KP_Decimal", 58015}, {"KP_1", 58012}, {"KP_2", 58009},
    {"KP_3", 58011}, {"KP_4", 58006}, {"KP_5", 58013}, {"KP_6", 58008},
    {"KP_7", 58005}, {"KP_8", 58007}, {"KP_9", 58010},

    {"KP_Insert", 58032}, {"KP_Delete", 58030}, {"KP_End", 58033},
    {"KP_Down", 58034}, {"KP_PageDown", 58035},
    {"KP_Left", 58036}, {"KP_Begin", 58037}, {"KP_Right", 58038},
    {"KP_Home", 58039}, {"KP_Up", 58040}, {"KP_PageUp", 58041},
    {"KP_Divide", 58031}, {"KP_Multiply", 58026}, {"KP_Subtract", 58029},
    {"KP_Add", 58027},

    {"F1", 58046}, {"F2", 58047}, {"F3", 58048}, {"F4", 58049},
    {"F5", 58050}, {"F6", 58051}, {"F7", 58052}, {"F8", 58053},
    {"F9", 58054}, {"F10", 58055}, {"F11", 58056}, {"F12", 58057},

    /* Kitty CSI u extended keys */
    {"CapsLock", 57358}, {"ScrollLock", 57359}, {"NumLock", 57360},
    {"PrintScreen", 57361}, {"Pause", 57362}, {"Menu", 57363},
    {"F13", 57376}, {"F14", 57377}, {"F15", 57378}, {"F16", 57379},
    {"F17", 57380}, {"F18", 57381}, {"F19", 57382}, {"F20", 57383},
    {"F21", 57384}, {"F22", 57385}, {"F23", 57386}, {"F24", 57387},
    {"F25", 57388}, {"F26", 57389}, {"F27", 57390}, {"F28", 57391},
    {"F29", 57392}, {"F30", 57393}, {"F31", 57394}, {"F32", 57395},
    {"F33", 57396}, {"F34", 57397}, {"F35", 57398},
    {"KP_0", 57399}, {"KP_1", 57400}, {"KP_2", 57401}, {"KP_3", 57402},
    {"KP_4", 57403}, {"KP_5", 57404}, {"KP_6", 57405}, {"KP_7", 57406},
    {"KP_8", 57407}, {"KP_9", 57408}, {"KP_Decimal", 57409},
    {"KP_Divide", 57410}, {"KP_Multiply", 57411}, {"KP_Subtract", 57412},
    {"KP_Add", 57413}, {"KP_Enter", 57414}, {"KP_Equal", 57415},
    {"KP_Separator", 57416}, {"KP_Left", 57417}, {"KP_Right", 57418},
    {"KP_Up", 57419}, {"KP_Down", 57420}, {"KP_PageUp", 57421},
    {"KP_PageDown", 57422}, {"KP_Home", 57423}, {"KP_End", 57424},
    {"KP_Insert", 57425}, {"KP_Delete", 57426}, {"KP_Begin", 57427},
    {"AudioPlay", 57428}, {"MediaPause", 57429}, {"MediaPlayPause", 57430},
    {"MediaReverse", 57431}, {"AudioStop", 57432}, {"MediaFastForward", 57433},
    {"MediaRewind", 57434}, {"AudioNext", 57435}, {"AudioPrev", 57436},
    {"MediaRecord", 57437}, {"AudioLowerVolume", 57438},
    {"AudioRaiseVolume", 57439}, {"AudioMute", 57440},
    {"LShift", 57441}, {"LControl", 57442}, {"LAlt", 57443},
    {"LSuper", 57444}, {"LHyper", 57445}, {"LMeta", 57446},
    {"RShift", 57447}, {"RControl", 57448}, {"RAlt", 57449},
    {"RSuper", 57450}, {"RHyper", 57451}, {"RMeta", 57452},
    {"ISO_Level3_Shift", 57453}, {"ISO_Level5_Shift", 57454},

	/* Foot (modifyOtherKeys extensions) */
	{"KP_Enter", 65421}, {"KP_Multiply", 65450}, {"KP_Add", 65451},
	{"KP_Separator", 65452}, {"KP_Subtract", 65453},
	{"KP_Delete", 65454}, {"KP_Divide", 65455}, {"KP_Insert", 65456},
	{"KP_End", 65457}, {"KP_Down", 65458}, {"KP_PageDown", 65459},
	{"KP_Left", 65460}, {"KP_Begin", 65461}, {"KP_Right", 65462},
	{"KP_Home", 65463}, {"KP_Up", 65464}, {"KP_PageUp", 65465},

	{NULL, 0}
};

struct exceptions_t {
	const char *key;
	const char *name;
};

/* A list of escape sequences missed by our identifying algorithms, mostly
 * because they do not seem to follow any recognizable pattern. */
static const struct exceptions_t exceptions[] = {
	/* The Linux console uses CSI final bytes A-E for F1-F5, when A-D
	 * is almost universally used for arrow keys. */
	{"\x1b[[A", "F1"}, {"\x1b[[B", "F2"}, {"\x1b[[C", "F3"},
	{"\x1b[[D", "F4"}, {"\x1b[[E", "F5"},

	/* St
	 * Keycodes and modifiers are not used consistently. For example,
	 * "CSI 2J" is Shift+Home: '2' for Shift and 'J' for Home. But,
	 * "CSI J" is Ctrl+End: no modifier (it should be '5') and 'J' is not
	 * Home anymore, but Del.
	 * Also, while "CSI P", is Del, "CSI 2K" is Shift+Del and "CSI K" is Shift+End.
	 * Also, while "CSI L" is Ctrl+Ins, "CSI 4l" is Shift+Ins. */
	{"\x1b[4h", "Insert"}, {"\x1b[M", "Ctrl+Delete"}, {"\x1b[L", "Ctrl+Insert"},
	{"\x1b[2J", "Shift+Home"}, {"\x1b[K", "Shift+End"},
	{"\x1b[2K", "Shift+Delete"}, {"\x1b[J", "Ctrl+End"},
	{"\x1b[4l", "Shift+Insert"}, {"\x1b[P", "Delete"},
	{NULL, NULL}
};

/* The linux console uses CSI [25-33]~ for Shift+[F1-F8]: this conflicts with
 * rxvt, which uses the same codes for Shift+[F3-F10]. */
static const char *
fix_linux_func_keys(const int keycode, const int mod_key, const char *key)
{
	if (mod_key != 0)
		return key;

	switch (keycode) {
	case 25: return "Shift+F1"; case 26: return "Shift+F2";
	case 28: return "Shift+F3"; case 29: return "Shift+F4";
	case 31: return "Shift+F5"; case 32: return "Shift+F6";
	case 33: return "Shift+F7"; case 34: return "Shift+F8";
	default: return key;
	}
}

/* A safe atoi */
static int
xatoi(const char *str)
{
	if (!str || !*str)
		return 0;

	char *endptr = NULL;
	const long n = strtol(str, &endptr, 10);

	if (endptr == str)
		return 0;

	if (n > INT_MAX) return INT_MAX;
	if (n < INT_MIN) return INT_MIN;
	return (int)n;
}

/* Return the translated key for the escape sequence STR looking in the
 * exceptions list. If none is found, NULL is returned. */
static char *
check_exceptions(const char *str, const int term_type)
{
	if (term_type == TK_TERM_LEGACY_SCO || term_type == TK_TERM_LEGACY_HP)
		return NULL;

	/* Fix conflict with st: 'CSI P' is F1 in Kitty and Del in st. */
	if (term_type == TK_TERM_KITTY && *str == ESC_KEY
	&& str[1] == CSI_INTRODUCER && str[2] == 'P' && !str[3])
		return NULL;

	for (size_t i = 0; exceptions[i].key; i++) {
		if (strcmp(exceptions[i].key, str) == 0) {
			const size_t len = strlen(exceptions[i].name); /* flawfinder: ignore */
			char *p = malloc((len + 1) * sizeof(char));
			if (!p)
				return NULL;
			memcpy(p, exceptions[i].name, len + 1);
			return p;
		}
	}

	return NULL;
}

/* Return 1 if the byte C ends a keyboard escape sequence, or 0 otherwise. */
int
is_end_seq_char(unsigned char c)
{
	return (c != 0x1b /* First byte of an escape sequence */
		&& c != CSI_INTRODUCER && c != SS3_INTRODUCER
		&& ((c >= 0x40 && c <= 0x7e) /* ECMA-48 terminating bytes */
		|| c == '$')); /* Rxvt uses this (E.g. "CSI 24$" for Shift+F12) */
}

static int
is_sco_seq(const int csi_seq, const char *seq, const size_t end)
{
	return (csi_seq == 1 && seq[end] != '~');
}

static int
is_hp_seq(const unsigned char c)
{
	return ((c == 'h' || (c >= 'p' && c <= 'w')
	|| (c >= 'A' && c <= 'D') || c == 'F' || c == 'J' || c == 'P' || c == 'Q'
	|| c == 'S' || c == 'T'));
}

/* Rxvt uses '$', '^', and '@' to indicate the modifier key. */
static void
set_end_char_is_mod_key(char *str, const size_t end, int *keycode, int *mod_key)
{
	const char end_char = str[end];

	if (*str == ESC_KEY) {
		*mod_key += MOD_ALT;
		str += 2; /* Skip "ESC [" */
	}

	str[end] = '\0';
	*keycode = xatoi(str);

	/* Rxvt function keys F13-F20 (integers 25-34), excluding the Menu key
	 * (or F16, integer 29). */
	const char is_func_key =
		(*keycode >= 25 && *keycode <= 34 && *keycode != 29);

	if (end_char == '$')
		*mod_key += (!is_func_key ? MOD_SHIFT : 0);
	else /* Either '^' (Ctrl) or '@' (Ctrl+Shift) */
		*mod_key += MOD_CTRL + ((end_char == '@'
			&& !is_func_key) ? MOD_SHIFT : 0);
}

/* The terminating character just terminates the string. Mostly '~', but
 * also 'z' is Sun/Solaris terminals. In this case, the pressed key and
 * the modifier key are defined as parameters in the sequence. */
static void
set_end_char_is_generic(char *str, const size_t end, int *keycode, int *mod_key)
{
	str[end] = '\0';
	char *s = strchr(str, ';');

	if (*str == ESC_KEY) {
		if (!s) { /* Rxvt */
			*mod_key += MOD_ALT;
			*keycode = xatoi(str + 2);
			return;
		}
		/* Skip 0x1b and '[' (if any) */
		str += str[1] == CSI_INTRODUCER ? 2 : 1;
	}

	if (s) *s = '\0';
	/* "CSI >1;key;mod~" = Xterm with modifyFunctionKeys:3 */
	*keycode = xatoi(str + (*str == '>'));
	*mod_key += (s && s[1]) ? xatoi(s + 1) - 1 : 0;
}

static void
set_end_char_is_keycode_no_arrow(char *str, const size_t end, int *keycode,
	int *mod_key)
{
	*keycode = str[end];
	char *s = strchr(str, ';');
	str[end] = '\0';

	if (s) {
		*mod_key += s[1] ? xatoi(s + 1) - 1 : 0;
	} else {
		if (*str == ESC_KEY) { /* Rxvt */
			*mod_key += MOD_ALT;
			str++;
		}
		if (*str == SS3_INTRODUCER) /* Contour (SS3 mod key) */
			str++;
		*mod_key += *str ? xatoi(str) - 1 : 0;
	}
}

/* The terminating character desginates the key pressed. Mostly arrow keys
 * (e.g. CSI D) for the Left key.
 * Note: When set to application mode (CSI ?1h - DECCKM) arrow keys are
 * transmitted as SS3 sequences (otherwise, as CSI sequences, most of the time).
 * However, we don't care about this: we can parse both sequences equally well. */
static void
set_end_char_is_keycode(char *str, size_t end, int *keycode, int *mod_key)
{
	if (!IS_ARROW_CHAR(str[end])) {
		set_end_char_is_keycode_no_arrow(str, end, keycode, mod_key);
		return;
	}

	*keycode = str[end];
	char *s = strchr(str, ';');

	if (*str == ESC_KEY && !s) { /* Rxvt */
		*mod_key += MOD_ALT;
		str++;
		end--;
	}

	if (s) {
		str[end] = '\0';
		*mod_key += (s && s[1]) ? xatoi(s + 1) - 1 : 0;
	} else if (IS_LOWER_ARROW_CHAR(str[end])) { /* Rxvt */
		if (*str == SS3_INTRODUCER)
			*mod_key += MOD_CTRL;
		else
			*mod_key += MOD_SHIFT;
	} else if (IS_UPPER_ARROW_CHAR(str[end])) {
		str[end] = '\0';
		if (*str == SS3_INTRODUCER)
			str++;
		if (IS_DIGIT(*str))
			*mod_key += xatoi(str) - 1;
	}
}

/* Translate the modifier number MOD_KEY into human-readable form. */
static const char *
get_mod_symbol(const int mod_key)
{
	if (mod_key <= 0)
		return NULL;

	/* The biggest value mod_key can take is 255 (since
	 * 1 + 2 + 4 + 8 + 16 + 32 + 64 + 128 = 255). In this case, the modifier
	 * string would be "Shift+Alt+Ctrl+Super+Hyper+Meta+CapsLock+NumLock-",
	 * which is 50 bytes long, including the terminating NUL byte. */
	static char mod[64];
	memset(mod, '\0', sizeof(mod));

	const int m = mod_key;
	const size_t s = sizeof(mod);
	int l = 0;

	if (m & 128) l += snprintf(mod + l, s - (size_t)l, "NumLock+");
	if (m & 64)  l += snprintf(mod + l, s - (size_t)l, "CapsLock+");
	if (m & 32)  l += snprintf(mod + l, s - (size_t)l, "Meta+");
	if (m & 16)  l += snprintf(mod + l, s - (size_t)l, "Hyper+");
	if (m & 8)   l += snprintf(mod + l, s - (size_t)l, "Super+");
	if (m & 4)   l += snprintf(mod + l, s - (size_t)l, "Ctrl+");
	if (m & 2)   l += snprintf(mod + l, s - (size_t)l, "Alt+");
	if (m & 1)        snprintf(mod + l, s - (size_t)l, "Shift+");

	return mod;
}

/* Max output string length */
#define MAX_BUF 256
#define SYM(n) (get_mod_symbol((n)))

static char *
print_non_esc_seq(const char *str)
{
	char *buf = malloc(MAX_BUF * sizeof(char));
	if (!buf)
		return NULL;

	if (!str || !*str) {
		free(buf);
		return NULL;
	}

	const unsigned char *s = (const unsigned char *)str;

	if (s[1]) {
		snprintf(buf, MAX_BUF, "%s", s); /* A string, not a byte */
	} else if (ctrl_keys[*s]) { /* Backspace, Tab, Return, Space, Del */
		snprintf(buf, MAX_BUF, "%s", ctrl_keys[*s]);
	} else if (*s < 0x20) { /* Control characters */
		snprintf(buf, MAX_BUF, "%s%c", SYM(MOD_CTRL), *s + '@' + 0x20);
	} else {
		free(buf);
		return NULL;
	}

	return buf;
}

static char *
check_single_key(char *str, const int csi_seq, const int term_type)
{
	char *buf = malloc(MAX_BUF * sizeof(char));
	if (!buf)
		return NULL;

	while (*str == ESC_KEY) /* Skip extra leading 0x1b */
		str++;

	if (!*str) {
		snprintf(buf, MAX_BUF, "%s", ctrl_keys[(unsigned char)ESC_KEY]);
		return buf;
	}

	/* Alt+UTF-8 */
	if (IS_UTF8_LEAD_BYTE(*str) && csi_seq == 0) {
		snprintf(buf, MAX_BUF, "%s%s", SYM(MOD_ALT), (unsigned char *)str);
		return buf;
	}

	if (str[1]) { /* More than 1 byte after ESC */
		free(buf);
		return NULL;
	}

	if (*str == 'Z' && csi_seq == 1 && term_type != TK_TERM_LEGACY_SCO
	&& term_type != TK_TERM_LEGACY_HP) {
		snprintf(buf, MAX_BUF, "%sTab", SYM(MOD_SHIFT));
		return buf;
	}

	if (csi_seq == 0) {
		unsigned char *s = (unsigned char *)str;
		if (ctrl_keys[*s]) /* Backspace, Tab, Return, Space, Del */
			snprintf(buf, MAX_BUF, "%s%s", SYM(MOD_ALT), ctrl_keys[*s]);
		else if (*s < 0x20)
			snprintf(buf, MAX_BUF, "%s%c", SYM(MOD_CTRL + MOD_ALT),
				*s + '@' + 0x20);
		else if (term_type != TK_TERM_LEGACY_HP || !is_hp_seq(*s))
			snprintf(buf, MAX_BUF, "%s%c", SYM(MOD_ALT), *s);
		else
			return NULL;
		return buf;
	}

	free(buf);
	return NULL;
}
#undef MAX_BUF
#undef SYM

static const char *
get_ext_key_symbol(const int keycode, const int term_type)
{
	/* These are directly printable */
	if (keycode > 32 && keycode <= 126) {
		static char keysym_str[2] = {0};
		keysym_str[0] = (char)keycode;
		return keysym_str;
	}

	if (term_type == TK_TERM_XTERM && (keycode == 19 || keycode == 20))
		return keycode == 20 ? "ScrollLock" : "Pause";

	/* Linear search through ext_keymap */
	for (size_t i = 0; ext_keymap[i].name != NULL; i++) {
		if (ext_keymap[i].code == keycode)
			return ext_keymap[i].name;
	}

	/* Transform a UTF-8 codepoint into a string of bytes. */
	static char str[5] = "";
	memset(str, 0, sizeof(str));

	if (keycode <= 0x7ff) {
		str[0] = (char)(0xc0 | (keycode >> 6));
		str[1] = (char)(0x80 | (keycode & 0x3f));
	} else if (keycode <= 0x7fff) {
		str[0] = (char)(0xe0 | (keycode >> 12));
		str[1] = (char)(0x80 | ((keycode >> 6) & 0x3f));
		str[2] = (char)(0x80 | (keycode & 0x3f));
	} else if (keycode <= 0x10ffff) {
		str[0] = (char)(0xf0 | (keycode >> 18));
		str[1] = (char)(0x80 | ((keycode >> 12) & 0x3f));
		str[2] = (char)(0x80 | ((keycode >> 6) & 0x3f));
		str[3] = (char)(0x80 | (keycode & 0x3f));
	} else {
		return "Unknown";
	}

	return str;
}

static char *
write_kitty_keys(char *str, const size_t end, const int term_type)
{
	str[end] = '\0';

	int keycode = -1;
	int mod_key = 0;

	char *delim = strchr(str, ';');
	if (delim) {
		*delim = '\0';
		keycode = xatoi(str);
		mod_key = delim[1] ? xatoi(delim + 1) - 1 : 0;
	} else {
		keycode = xatoi(str);
	}

	const char *k = keycode != -1 ? get_ext_key_symbol(keycode, term_type) : NULL;
	const char *m = mod_key != 0 ? get_mod_symbol(mod_key) : NULL;

	if (!k)
		return NULL;

	const size_t buf_len = strlen(k) + (m ? strlen(m) : 0) + 1; /* flawfinder: ignore */
	char *buf = malloc(buf_len * sizeof(char));
	if (!buf)
		return NULL;

	snprintf(buf, buf_len, "%s%s", m ?  m : "", k);
	return buf;
}

/* An Xterm MOK (modifyOtherKeys) sequence is "CSI 27;mod;key~"
 * Note that, if formatOtherKeys is set to 1, "CSI u" sequences are used
 * instead, in which case they are covered by the kitty functions.
 * See https://xterm.dev/manpage-xterm/#VT100-Widget-Resources:modifyOtherKeys */
static char *
write_xterm_mok_seq(char *str, const size_t end, const int term_type)
{
	str[end] = '\0';
	str += 3; /* Skip "27;" */
	char *s = strchr(str, ';');
	if (!s)
		return NULL;

	*s = '\0';
	const int mod_key = xatoi(str) - 1;
	const int keycode = xatoi(s + 1);

	const char *k = get_ext_key_symbol(keycode, term_type);
	const char *m = (mod_key >= 0 && mod_key <= 255)
		? get_mod_symbol((int)mod_key) : NULL;

	const size_t len = (m ? strlen(m) : 0) + (k ? strlen(k) : 0) + 2; /* flawfinder: ignore */
	char *buf = malloc(len * sizeof(char));
	if (!buf)
		return NULL;

	snprintf(buf, len, "%s%s", m ? m : "", k);
	return buf;
}

static const char **
get_keymap(const int term_type)
{
	switch (term_type) {
	case TK_TERM_LEGACY_SCO: return key_map_sco;
	case TK_TERM_LEGACY_HP:  return key_map_hp;
	case TK_TERM_GENERIC: /* fallthrough */
	default:                 return key_map_generic;
	}
}

static char *
write_translation(const int keycode, const int mod_key, const int term_type)
{
	const char **keymap = get_keymap(term_type);
	const char *k = (keycode >= 0 && keycode <= 255)
		? keymap[(unsigned char)keycode] : NULL;
	const char *m = (mod_key >= 0 && mod_key <= 255)
		? get_mod_symbol((int)mod_key) : NULL;

	if (!k)
		return NULL;

	if (term_type == TK_TERM_LINUX)
		k = fix_linux_func_keys(keycode, mod_key, k);

	const size_t len = (m ? strlen(m) : 0) + strlen(k) + 1; /* flawfinder: ignore */
	char *buf = malloc(len * sizeof(char));
	if (!buf)
		return NULL;

	if (m)
		snprintf(buf, len, "%s%s", m, k);
	else
		snprintf(buf, len, "%s", k);

	return buf;
}

static int
normalize_seq(char **seq, const int term_type)
{
	char *s = *seq;

	const int csi_seq =
		(s[1] == CSI_INTRODUCER || (unsigned char)*s == ALT_CSI);
	s += s[1] == CSI_INTRODUCER ? 2 : 1;

	while ((unsigned char)*s == ALT_CSI) /* Skip extra 0x9b */
		s++;

	/* Skip extra '['. The Linux console, for example, is known to emit
	 * a double CSI introducer for function keys (e.g. "ESC [[A" for F1). */
	while ((unsigned char)*s == CSI_INTRODUCER && term_type != TK_TERM_LEGACY_SCO)
		s++;

	*seq = s;
	return csi_seq;
}

/* Legacy mode: either SCO or HP keyboard mode. */
static char *
write_legacy_keys(char *seq, const size_t end, const int term_type)
{
	char *s = strchr(seq, ';');
	const int mod_key = (s && s[1]) ? xatoi(s + 1) - 1 : 0;

	return write_translation(seq[end], mod_key, term_type);
}

/* Translate the escape sequence SEQ into the corresponding symbolic value.
 * E.g. "\x1b[1;7D" will return "Ctrl+Alt+Left". If no symbolic value is
 * found, NULL is returned.
 * The returned value, if not NULL, is dinamically allocated and must be
 * free'd by the caller.
 *
 * TERM_TYPE is one of the TK_TERM values (defined in translate_key.h).
 * It defines how SEQ will be translated. Unless running on a legacy terminal,
 * or using a legacy keyboard type (like SCO or HP), you most likely want to
 * set this value to TK_TERM_GENERIC. For more info consult the -kt option in
 * the XTerm manpage.
 *
 * The following encoding schemes are supported:
 * 1. SCO (legacy): set term_type to TK_TERM_LEGACY_SCO
 * 2. HP (legacy): set term_type to TK_TERM_LEGACY_HP
 *
 * The following encondings are automatically handled by this function
 * (set term_type to TK_TERM_GENERIC):
 *
 * 3. Sun (CSI-z)
 * 4. Xterm
 * 5. Xterm (modifyOtherKeys)
 * 6. Rxvt
 * 7. Fixterms (CSI-u)
 * 8. Kitty (extended CSI-u)
 * 9. Foot (extended CSI-u)
 *
 * NOTE: This function assumes SEQ comes directly from the terminal, i.e. by
 * reading terminal input in raw mode. User suplied input, therefore, may
 * return false positives. */
char *
translate_key(char *seq, const int term_type)
{
	if (!seq || !*seq)
		return NULL;

	if (*seq != ESC_KEY && (unsigned char)*seq != ALT_CSI)
		return print_non_esc_seq(seq);

	char *buf = check_exceptions(seq, term_type);
	if (buf)
		return buf;

	const int csi_seq = normalize_seq(&seq, term_type);

	buf = check_single_key(seq, csi_seq, term_type);
	if (buf)
		return buf;

	int keycode = -1;
	int mod_key = 0;

	const size_t len = strlen(seq); /* flawfinder: ignore */
	const size_t end = len > 0 ? len - 1 : len;

	const char end_char = seq[end];

	if (term_type == TK_TERM_LEGACY_HP && end_char != '~')
		return write_legacy_keys(seq, end, term_type);

	if (term_type == TK_TERM_LEGACY_SCO && is_sco_seq(csi_seq, seq, end))
		return write_legacy_keys(seq, end, term_type);

	if (IS_KITTY_END_CHAR(end_char) && csi_seq == 1)
		return write_kitty_keys(seq, end, term_type);

	if (IS_XTERM_MOK_SEQ(seq, end_char) && csi_seq == 1)
		return write_xterm_mok_seq(seq, end, term_type);

	else if (IS_MODKEY_END_CHAR(end_char))
		set_end_char_is_mod_key(seq, end, &keycode, &mod_key);
	else if (IS_KEYCODE_END_CHAR(end_char))
		set_end_char_is_keycode(seq, end, &keycode, &mod_key);
	else if (IS_GENERIC_END_CHAR(end_char))
		set_end_char_is_generic(seq, end, &keycode, &mod_key);
	else
		return NULL;

	const int tt =
		(term_type == TK_TERM_LEGACY_HP || term_type == TK_TERM_LEGACY_SCO)
		? TK_TERM_GENERIC : term_type;

	return write_translation(keycode, mod_key, tt);
}
