/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2016-2025, L. Abramovich <leo.clifm@outlook.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
*/

/* keybinds.c -- handle keybindings */

#include "helpers.h"

#ifdef __OpenBSD__
typedef char *rl_cpvfunc_t;
# include <ereadline/readline/readline.h>
#else
# include <readline/readline.h>
#endif /* __OpenBSD__ */

#ifdef __TINYC__
/* Silence a tcc warning. We don't use CTRL anyway */
# undef CTRL
#endif /* __TINYC */

#include <unistd.h>

#ifdef __NetBSD__
# include <string.h>
#endif /* __NetBSD__ */

#include <errno.h>

#if defined(MAC_OS_X_RENAMEAT_SYS_STDIO_H)
# include <sys/stdio.h> /* renameat(2) */
#endif /* MAC_OS_X_RENAMEAT_SYS_STDIO_H */

#include "aux.h"
#include "autocmds.h" /* update_autocmd_opts() */
#include "config.h"
#include "exec.h"
#include "file_operations.h"
#include "keybinds.h"
#include "listing.h"
#include "messages.h"
#include "misc.h"
#include "profiles.h"
#include "prompt.h"
#include "readline.h"
#include "sort.h" /* QSFUNC macro for qsort(3) */
#include "spawn.h"
#include "tabcomp.h" /* tab_complete() */

#ifndef _NO_SUGGESTIONS
# include "suggestions.h"
#endif /* !_NO_SUGGESTIONS */

#ifndef _NO_HIGHLIGHT
# include "highlight.h"
#endif /* !_NO_HIGHLIGHT */

#include "strings.h" /* quote_str() */

#ifndef _NO_SUGGESTIONS
static int accept_first_word = 0;
#endif /* !_NO_SUGGESTIONS */

/* This is just an ugly workaround. You've been warned!
 * Long story short: prompt commands are executed after SOME keybindings, but
 * not after others. I'm not sure why, and this irregularity is what is wrong
 * in the first place.
 * In the second case, when prompt commands are not executed, we sometimes do
 * want to run them (mostly when changing directories): in this case, we set
 * exec_prompt_cmds to 1. */
static int exec_prompt_cmds = 0;

static void
xrl_reset_line_state(void)
{
	UNHIDE_CURSOR;
	rl_reset_line_state();
}

static int
append_str(char *buf, const int buf_size, size_t *len, const char *str)
{
	const size_t length = strlen(str);
	if (length >= (size_t)buf_size - *len)
		return (-1); /* Buffer overflow */

	xstrsncpy(buf + *len, str, (size_t)buf_size - *len);
	*len += length;

	return 0;
}

static const char *
get_key_symbol(const int key)
{
	static char keysym_str[2] = {0};

	/* These are directly printable */
	if (key > 32 && key < 256 && key != 127  && key != 160 && key != 173) {
		keysym_str[0] = (char)key;
		return keysym_str;
	}

	switch (key) {
	/* Control keys */
	case 0: return "NULL"; case 1: return "SOH"; case 2: return "STX";
	case 3: return "ETX"; case 4: return "EOT"; case 5: return "ENQ";
	case 6: return "ACK"; case 7: return "BELL"; case 8: return "BS";
	case 9: return "Tab"; case 10: return "LF"; case 11: return "VT";
	case 12: return "FF"; case 13: return "CR"; case 14: return "SO";
	case 15: return "SI"; case 16: return "DLE"; case 17: return "DC1";
	case 18: return "DC2"; case 19: return "DC3"; case 20: return "DC4";
	case 21: return "NAK"; case 22: return "SYN"; case 23: return "ETB";
	case 24: return "CAN"; case 25: return "EM"; case 26: return "SUB";
	case 27: return "ESC"; case 28: return "FS"; case 29: return "GS";
	case 30: return "RS"; case 31: return "US";

	/* Non-printable regular keys */
	case 32: return "Space"; case 127: return "Del";
	case 160: return "NSBP"; case 173: return "SHY";

	/* Special keyboard keys */
	case 57358: return "CapsLock"; case 57359: return "ScrollLock";
	case 57360: return "NumLock"; case 57361: return "PrtScr";
	case 57362: return "Pause"; case 57363: return "Menu";
	case 57376: return "F13"; case 57377: return "F14";
	case 57378: return "F15"; case 57379: return "F16";
	case 57380: return "F17"; case 57381: return "F18";
	case 57382: return "F19"; case 57383: return "F20";
	case 57384: return "F21"; case 57385: return "F22";
	case 57386: return "F23"; case 57387: return "F24";
	case 57388: return "F25"; case 57389: return "F26";
	case 57390: return "F27"; case 57391: return "F28";
	case 57392: return "F29"; case 57393: return "F30";
	case 57394: return "F31"; case 57395: return "F32";
	case 57396: return "F33"; case 57397: return "F34";
	case 57398: return "F35"; case 57399: return "Numpad0";
	case 57400: return "Numpad1"; case 57401: return "Numpad2";
	case 57402: return "Numpad3"; case 57403: return "Numpad4";
	case 57404: return "Numpad5"; case 57405: return "Numpad6";
	case 57406: return "Numpad7"; case 57407: return "Numpad8";
	case 57408: return "Numpad9"; case 57409: return "NumpadDecimal";
	case 57410: return "NumpadDivide"; case 57411: return "NumpadMultiply";
	case 57412: return "NumpadSubtract"; case 57413: return "NumpadAdd";
	case 57414: return "NumpadEnter"; case 57415: return "NumpadEquals";
	case 57416: return "NumpadSeparator"; case 57417: return "NumpadLeft";
	case 57418: return "NumpadRight"; case 57419: return "NumpadUp";
	case 57420: return "NumpadDown"; case 57421: return "NumpadPageUp";
	case 57422: return "NumpadPageDown"; case 57423: return "NumpadHome";
	case 57424: return "NumpadEnd"; case 57425: return "NumpadInsert";
	case 57426: return "NumpadDelete"; case 57427: return "NumpadBegin";
	case 57428: return "MediaPlay"; case 57429: return "MediaPause";
	case 57430: return "MediaPlayPause"; case 57431: return "MediaReverse";
	case 57432: return "MediaStop"; case 57433: return "MediaFastForward";
	case 57434: return "MediaRewind"; case 57435: return "MediaTrackNext";
	case 57436: return "MediaTrackPrevious"; case 57437: return "MediaRecord";
	case 57438: return "VolumeDown"; case 57439: return "VolumeUp";
	case 57440: return "VolumeMute"; case 57441: return "LShift";
	case 57442: return "LControl"; case 57443: return "LAlt";
	case 57444: return "LSuper"; case 57445: return "LHyper";
	case 57446: return "LMeta"; case 57447: return "RShift";
	case 57448: return "RControl"; case 57449: return "RAlt";
	case 57450: return "RSuper"; case 57451: return "RHyper";
	case 57452: return "RMeta"; case 57453: return "ISO_Level3_Shift";
	case 57454: return "ISO_Level5_Shift";

	default: return "UNKNOWN";
	}
}

/* Translate the modifier number MOD_NUM into human-readable form. */
static const char *
get_mod_symbol(const int mod_num)
{
	/* The biggest value mod_num can take is 255 (since
	 * 1 + 2 + 4 + 8 + 16 + 32 + 64 + 128 = 255). In this case, the modifier
	 * string would be "Shift+Alt+Ctrl+Super+Hyper+Meta+CapsLock+NumLock-",
	 * which is 50 bytes long, including the terminating NUL char. */
	static char mod[64];
	memset(mod, '\0', sizeof(mod));

	const int modifiers = mod_num - 1;
	int len = 0;

	if (modifiers & 4)
		len += snprintf(mod + len, sizeof(mod) - (size_t)len, "Ctrl+");

	if (modifiers & 2)
		len += snprintf(mod + len, sizeof(mod) - (size_t)len, "Alt+");

	if (modifiers & 1)
		len += snprintf(mod + len, sizeof(mod) - (size_t)len, "Shift+");

	if (modifiers & 8)
		len += snprintf(mod + len, sizeof(mod) - (size_t)len, "Super+");

	if (modifiers & 16)
		len += snprintf(mod + len, sizeof(mod) - (size_t)len, "Hyper+");

	if (modifiers & 32)
		len += snprintf(mod + len, sizeof(mod) - (size_t)len, "Meta+");

	if (modifiers & 64)
		len += snprintf(mod + len, sizeof(mod) - (size_t)len, "CapsLock+");

	if (modifiers & 128)
		snprintf(mod + len, sizeof(mod) - (size_t)len, "NumLock+");

	return mod;
}

#define VALID_KITTY_KEY(n) (((n) >= 0 && (n) <= 255) \
	|| ((n) >= 57344 && (n) <= 63743))

static int
append_kitty_key_no_mod(char *buf, const int buf_size, size_t *len, char *str)
{
	char *key_end = strchr(str, 'u');
	if (!key_end)
		return 0;

	*key_end = '\0';
	/* STR is "\e[NNN", so that we move past '[' to get the number */
	const int key_num = atoi(str + 3);
	*key_end = 'u';

	if (!VALID_KITTY_KEY(key_num))
		return 0;

	const int cont = (key_end[1] != '\0');
	const char *keysym = get_key_symbol(key_num);
	if (!keysym)
		return 0;

	const int bytes = snprintf(buf + *len, (size_t)buf_size - *len,
		"%s%s", keysym, cont == 1 ? "," : "");

	*len += (size_t)bytes;

	return (int)((key_end + 1) - str);

}

static int
append_str_kitty(char *buf, const int buf_size, size_t *len, char *str)
{
	const size_t length = strlen(str);

	if (length >= (size_t)buf_size - *len)
		return (-1); /* Buffer overflow */

	if (length <= 2)
		return 0;

	char *orig_str = str;
	str += 3; /* Move past '[' */

	char *key_end = strchr(str, ';');
	if (!key_end || !key_end[1])
		/* We may have a key with no modifier. */
		return append_kitty_key_no_mod(buf, buf_size, len, orig_str);

	*key_end = '\0';
	const int key_num = atoi(str);
	*key_end = ';';
	if (!VALID_KITTY_KEY(key_num))
		return 0;

	char *mod_end = strchr(key_end + 1, 'u');
	if (!mod_end)
		return 0;

	*mod_end = '\0';
	const int mod_num = atoi(key_end + 1);
	*mod_end = 'u';
	if (mod_num < 0 || mod_num > 512)
		return 0;

	const char *mod = get_mod_symbol(mod_num);
	if (!mod || !*mod)
		return 0;

	/* Check whether there's another key sequence after the one currently
	 * analyzed, in which case we append an ending comma (,). */
	const int cont = (mod_end[1] != '\0');

	const char *keysym = get_key_symbol(key_num);

	const int bytes = snprintf(buf + *len, (size_t)buf_size - *len,
		"%s%s%s", mod, keysym, cont == 1 ? "," : "");

	*len += (size_t)bytes;

	return (int)((mod_end + 1) - orig_str);
}

#undef VALID_KITTY_KEY

static const char *
translate_key_nofunc(const char *key)
{
	if (!key || !*key || *key != '\\')
		return NULL;

#define KBUF_SIZE 256 /* This should be enough to handle most keybindings. */
#define END_KEYSEQ_CHAR ','
	static char buf[KBUF_SIZE] = {0};
	size_t buf_len = 0;

	while (*key) {
		if (*key == '\\') {

			if (key[1] == 'e' && key[2] == '[' && key[3]) {
				const int len =
					append_str_kitty(buf, KBUF_SIZE, &buf_len, (char *)key);
				if (len <= 0)
					return NULL;
				key += len;
				continue;
			}

			if (key[1] == 'e' || (key[1] == 'M' && key[2] ==  '-')) {
				if (append_str(buf, KBUF_SIZE, &buf_len, "Alt+") == -1)
					return NULL;
				/* If "\M-" we want to advance 3 bytes, not 2. */
				key += 2 + (key[1] ==  'M');
				if (!*key) /* Incomplete sequence: Alt without modified key. */
					return NULL;
				continue;
			}

			if (key[1] == 'C' && key[2] == '-') {
				if (append_str(buf, KBUF_SIZE, &buf_len, "Ctrl+") == -1)
					return NULL;
				key += 3;
				if (!*key) /* Incomplete sequence: Ctrl without modified key. */
					return NULL;
				continue;
			}

			// Additional escape sequences:
			// https://www.gnu.org/software/bash/manual/html_node/Readline-Init-File-Syntax.html

			/* Unrecognized escape sequence */
			return NULL;
		}

		if (!*key || buf_len >= sizeof(buf) - 2
		/* No keybinding starts with a non-modifier key. Skip it. */
		|| buf_len == 0)
			return NULL;

		/* Let's try to skip non-keyboard related escape sequences:
		 * CSI, OSC, DCS, APC, and PM escape sequences, plus
		 * character set switching sequences (e.g. "\e(A". */
		if ((*key == '[' || *key == ']' || *key == 'P' || *key == '_'
		|| *key == '^' || *key == '(' || *key == ')')
		&& key[1] && key[1] != '\\')
			return NULL;

		/* Append single character to the buffer. */
		buf[buf_len] = *key;
		buf_len++;
		key++;
		/* A character that is not a modifier key marks the end of the
		 * key sequence. Append an ',' in this case, provided it is
		 * not the end of the string. */
		if (*key) {
			buf[buf_len] = END_KEYSEQ_CHAR;
			buf_len++;
		}
	}
#undef KBUF_SIZE
#undef END_KEYSEQ_CHAR

	buf[buf_len] = '\0';

	if (*buf == 'A' && strncmp(buf, "Alt+Ctrl+", 9) == 0)
		memcpy(buf, "Ctrl+Alt+", 9);

	return *buf ? buf : NULL;
}

/* Translate the raw escape code KEY (sent by the terminal upon a key press)
 * into a human-readable format.
 * Return the translation, if found, or NULL otherwise.
 *
 * The function uses a simple lookup table and covers the most common cases,
 * but is far from complete: exotic terminals and complex key combinations are
 * not supported. */
static const char *
translate_key(const char *key)
{
	if (!key || !*key)
		return NULL;

	struct keys_t {
		const char *key;
		const char *translation;
	};

	struct keys_t keys[] = {
		{"-", _("not bound")},

		/* xterm */
		{"\\e[A", "Up"}, {"\\e[B", "Down"},
		{"\\e[C", "Right"}, {"\\e[D", "Left"},

		{"\\e[1;2A", "Shift+Up"}, {"\\e[1;2B", "Shift+Down"},
		{"\\e[1;2C", "Shift+Right"}, {"\\e[1;2D", "Shift+Left"},

		{"\\e[1;3A", "Alt+Up"}, {"\\e[1;3B", "Alt+Down"},
		{"\\e[1;3C", "Alt+Right"}, {"\\e[1;3D", "Alt+Left"},

		{"\\e[1;5A", "Ctrl+Up"}, {"\\e[1;5B", "Ctrl+Down"},
		{"\\e[1;5C", "Ctrl+Right"}, {"\\e[1;5D", "Ctrl+Left"},

		{"\\e[1;6A", "Ctrl+Shift+Up"}, {"\\e[1;6B", "Ctrl+Shift+Down"},
		{"\\e[1;6C", "Ctrl+Shift+Right"}, {"\\e[1;6D", "Ctrl+Shift+Left"},

		{"\\e[1;7A", "Ctrl+Alt+Up"}, {"\\e[1;7B", "Ctrl+Alt+Down"},
		{"\\e[1;7C", "Ctrl+Alt+Right"}, {"\\e[1;7D", "Ctrl+Alt+Left"},

		{"\\e[1;8A", "Ctrl+Alt+Shift+Up"}, {"\\e[1;8B", "Ctrl+Alt+Shift+Down"},
		{"\\e[1;8C", "Ctrl+Alt+Shift+Right"}, {"\\e[1;8D", "Ctrl+Alt+Shift+Left"},

		{"\\eOP", "F1"}, {"\\eOQ", "F2"}, {"\\eOR", "F3"},
		{"\\eOS", "F4"}, {"\\e[15~", "F5"}, {"\\e[17~", "F6"},
		{"\\e[18~", "F7"}, {"\\e[19~", "F8"}, {"\\e[20~", "F9"},
		{"\\e[21~", "F10"}, {"\\e[23~", "F11"}, {"\\e[24~", "F12"},

		{"\\e[1;2P", "Shift+F1"}, {"\\e[1;2Q", "Shift+F2"},
		{"\\e[1;2R", "Shift+F3"}, {"\\e[1;2S", "Shift+F4"},
		{"\\e[15;2~", "Shift+F5"}, {"\\e[17;2~", "Shift+F6"},
		{"\\e[18;2~", "Shift+F7"}, {"\\e[19;2~", "Shift+F8"},
		{"\\e[20;2~", "Shift+F9"}, {"\\e[21;2~", "Shift+F10"},
		{"\\e[23;2~", "Shift+F11"}, {"\\e[24;2~", "Shift+F12"},

		{"\\e[1;3P", "Alt+F1"}, {"\\e[1;3Q", "Alt+F2"},
		{"\\e[1;3R", "Alt+F3"}, {"\\e[1;3S", "Alt+F4"},
		{"\\e[15;3~", "Alt+F5"}, {"\\e[17;3~", "Alt+F6"},
		{"\\e[18;3~", "Alt+F7"}, {"\\e[19;3~", "Alt+F8"},
		{"\\e[20;3~", "Alt+F9"}, {"\\e[21;3~", "Alt+F10"},
		{"\\e[23;3~", "Alt+F11"}, {"\\e[24;3~", "Alt+F12"},

		{"\\e[1;4P", "Alt+Shift+F1"}, {"\\e[1;4Q", "Alt+Shift+F2"},
		{"\\e[1;4R", "Alt+Shift+F3"}, {"\\e[1;4S", "Alt+Shift+F4"},
		{"\\e[15;4~", "Alt+Shift+F5"}, {"\\e[17;4~", "Alt+Shift+F6"},
		{"\\e[18;4~", "Alt+Shift+F7"}, {"\\e[19;4~", "Alt+Shift+F8"},
		{"\\e[20;4~", "Alt+Shift+F9"}, {"\\e[21;4~", "Alt+Shift+F10"},
		{"\\e[23;4~", "Alt+Shift+F11"}, {"\\e[24;4~", "Alt+Shift+F12"},

		{"\\e[1;5P", "Ctrl+F1"}, {"\\e[1;5Q", "Ctrl+F2"},
		{"\\e[1;5R", "Ctrl+F3"}, {"\\e[1;5S", "Ctrl+F4"},
		{"\\e[15;5~", "Ctrl+F5"}, {"\\e[17;5~", "Ctrl+F6"},
		{"\\e[18;5~", "Ctrl+F7"}, {"\\e[19;5~", "Ctrl+F8"},
		{"\\e[20;5~", "Ctrl+F9"}, {"\\e[21;5~", "Ctrl+F10"},
		{"\\e[23;5~", "Ctrl+F11"}, {"\\e[24;5~", "Ctrl+F12"},

		{"\\e[1;6P", "Ctrl+Shift+F1"}, {"\\e[1;6Q", "Ctrl+Shift+F2"},
		{"\\e[1;6R", "Ctrl+Shift+F3"}, {"\\e[1;6S", "Ctrl+Shift+F4"},
		{"\\e[15;6~", "Ctrl+Shift+F5"}, {"\\e[17;6~", "Ctrl+Shift+F6"},
		{"\\e[18;6~", "Ctrl+Shift+F7"}, {"\\e[19;6~", "Ctrl+Shift+F8"},
		{"\\e[20;6~", "Ctrl+Shift+F9"}, {"\\e[21;6~", "Ctrl+Shift+F10"},
		{"\\e[23;6~", "Ctrl+Shift+F11"}, {"\\e[24;6~", "Ctrl+Shift+F12"},

		{"\\e[1;8P", "Ctrl+Alt+Shift+F1"}, {"\\e[1;8Q", "Ctrl+Alt+Shift+F2"},
		{"\\e[1;8R", "Ctrl+Alt+Shift+F3"}, {"\\e[1;8S", "Ctrl+Alt+Shift+F4"},
		{"\\e[15;8~", "Ctrl+Alt+Shift+F5"}, {"\\e[17;8~", "Ctrl+Alt+Shift+F6"},
		{"\\e[18;8~", "Ctrl+Alt+Shift+F7"}, {"\\e[19;8~", "Ctrl+Alt+Shift+F8"},
		{"\\e[20;8~", "Ctrl+Alt+Shift+F9"}, {"\\e[21;8~", "Ctrl+Alt+Shift+F10"},
		{"\\e[23;8~", "Ctrl+Alt+Shift+F11"}, {"\\e[24;8~", "Ctrl+Alt+Shift+F12"},

		{"\\e[H", "Home"}, {"\\e[F", "End"},
		{"\\e[2~", "Ins"}, {"\\e[3~", "Del"},
		{"\\e[5~", "PgUp"}, {"\\e[6~", "PgDn"},

		{"\\e[1;3H", "Alt+Home"}, {"\\e[1;3F", "Alt+End"},
		{"\\e[2;3~", "Alt+Ins"}, {"\\e[3;3~", "Alt+Del"},
		{"\\e[5;3~", "Alt+PgUp"}, {"\\e[6;3~", "Alt+PgDn"},

		{"\\e[1;5H", "Ctrl+Home"}, {"\\e[1;5F", "Ctrl+End"},
		{"\\e[2;5~", "Ctrl+Ins"}, {"\\e[3;5~", "Ctrl+Del"},
		{"\\e[5;5~", "Ctrl+PgUp"}, {"\\e[6;5~", "Ctrl+PgDn"},

		{"\\e[1;7H", "Ctrl+Alt+Home"}, {"\\e[1;7F", "Ctrl+Alt+End"},
		{"\\e[2;7~", "Ctrl+Alt+Ins"}, {"\\e[3;7~", "Ctrl+Alt+Del"},
		{"\\e[5;7~", "Ctrl+Alt+PgUp"}, {"\\e[6;7~", "Ctrl+Alt+PgDn"},

		{"\\e[1;4H", "Alt+Shift+Home"}, {"\\e[1;4F", "Alt+Shift+End"},
		{"\\e[2;4~", "Alt+Shift+Ins"}, {"\\e[3;4~", "Alt+Shift+Del"},
		{"\\e[5;4~", "Alt+Shift+PgUp"}, {"\\e[6;4~", "Alt+Shift+PgDn"},

		{"\\e[1;6H", "Ctrl+Shift+Home"}, {"\\e[1;6F", "Ctrl+Shift+End"},
		{"\\e[2;6~", "Ctrl+Shift+Ins"}, {"\\e[3;6~", "Ctrl+Shift+Del"},
		{"\\e[5;6~", "Ctrl+Shift+PgUp"}, {"\\e[6;6~", "Ctrl+Shift+PgDn"},

		{"\\e[1;8H", "Ctrl+Alt+Shift+Home"}, {"\\e[1;8F", "Ctrl+Alt+Shift+End"},
		{"\\e[2;8~", "Ctrl+Alt+Shift+Ins"}, {"\\e[3;8~", "Ctrl+Alt+Shift+Del"},
		{"\\e[5;8~", "Ctrl+Alt+Shift+PgUp"}, {"\\e[6;8~", "Ctrl+Alt+Shift+PgDn"},

		{"\\e[3;2~", "Shift+Del"}, {"\\e[1;2H", "Shift+Home"},
		{"\\e[1;2F", "Shift+End"},

		{"\\e", "Esc"},

		{"\\C-i", "Tab"}, {"\\e\\C-i", "Alt+Tab"},
		{"\\e[Z", "Shift+Tab"},

		/* Note: xterm sends \x7f for Ctrl+Backspace and \C-h for Backspace */
		{"\\x7f", "Backspace"}, {"\\e\\x7f", "Alt+Backspace"},

		/* rxvt-specific */
		{"\\e[11~", "F1"}, {"\\e[12~", "F2"},
		{"\\e[13~", "F3"}, {"\\e[14~", "F4"},

		{"\\e[11^", "Ctrl+F1"}, {"\\e[12^", "Ctrl+F2"},
		{"\\e[13^", "Ctrl+F3"}, {"\\e[14^", "Ctrl+F4"},
		{"\\e[15^", "Ctrl+F5"}, {"\\e[17^", "Ctrl+F6"},
		{"\\e[18^", "Ctrl+F7"}, {"\\e[19^", "Ctrl+F8"},
		{"\\e[20^", "Ctrl+F9"}, {"\\e[21^", "Ctrl+F10"},
		{"\\e[23^", "Ctrl+F11"}, {"\\e[24^", "Ctrl+F12"},

		{"\\e[23~", "Shift+F1"}, {"\\e[24~", "Shift+F2"},
		{"\\e[25~", "Shift+F3"}, {"\\e[26~", "Shift+F4"},
		{"\\e[28~", "Shift+F5"}, {"\\e[29~", "Shift+F6"},
		{"\\e[31~", "Shift+F7"}, {"\\e[32~", "Shift+F8"},
		{"\\e[33~", "Shift+F9"}, {"\\e[34~", "Shift+F10"},
		{"\\e[23$", "Shift+F11"}, {"\\e[24$", "Shift+F12"},

		{"\\e\\e[11~", "Alt+F1"}, {"\\e\\e[12~", "Alt+F2"},
		{"\\e\\e[13~", "Alt+F3"}, {"\\e\\e[14~", "Alt+F4"},
		{"\\e\\e[15~", "Alt+F5"}, {"\\e\\e[17~", "Alt+F6"},
		{"\\e\\e[18~", "Alt+F7"}, {"\\e\\e[19~", "Alt+F8"},
		{"\\e\\e[20~", "Alt+F9"}, {"\\e\\e[21~", "Alt+F10"},
		{"\\e\\e[23~", "Alt+F11"}, {"\\e\\e[24~", "Alt+F12"},

		{"\\e[23^", "Ctrl+Shift+F1"}, {"\\e[24^", "Ctrl+Shift+F2"},
		{"\\e[25^", "Ctrl+Shift+F3"}, {"\\e[26^", "Ctrl+Shift+F4"},
		{"\\e[28^", "Ctrl+Shift+F5"}, {"\\e[29^", "Ctrl+Shift+F6"},
		{"\\e[31^", "Ctrl+Shift+F7"}, {"\\e[32^", "Ctrl+Shift+F8"},
		{"\\e[33^", "Ctrl+Shift+F9"}, {"\\e[34^", "Ctrl+Shift+F10"},
		{"\\e[23@", "Ctrl+Shift+F11"}, {"\\e[24@", "Ctrl+Shift+F12"},

		{"\\e[a", "Shift+Up"}, {"\\e[b", "Shift+Down"},
		{"\\e[c", "Shift+Right"}, {"\\e[d", "Shift+Left"},

		{"\\e\\e[A", "Alt+Up"}, {"\\e\\e[B", "Alt+Down"},
		{"\\e\\e[C", "Alt+Right"}, {"\\e\\e[D", "Alt+Left"},

		{"\\eOa", "Ctrl+Up"}, {"\\eOb", "Ctrl+Down"},
		{"\\eOc", "Ctrl+Right"}, {"\\eOd", "Ctrl+Left"},

		{"\\e[7~", "Home"}, {"\\e[8~", "End"},

		{"\\e\\e[7~", "Alt+Home"}, {"\\e\\e[8~", "Alt+End"},
		{"\\e\\e[2~", "Alt+Ins"}, {"\\e\\e[3~", "Alt+Del"},
		{"\\e\\e[5~", "Alt+PgUp"}, {"\\e\\e[6~", "Alt+PgDn"},

		{"\\e[7^", "Ctrl+Home"}, {"\\e[8^", "Ctrl+End"},
		{"\\e[2^", "Ctrl+Ins"}, {"\\e[3^", "Ctrl+Del"},
		{"\\e[5^", "Ctrl+PgUp"}, {"\\e[6^", "Ctrl+PgDn"},

		{"\\e[7$", "Shift+Home"}, {"\\e[8$", "Shift+End"},
		{"\\e[5^", "Ctrl+PgUp"}, {"\\e[6^", "Ctrl+PgDn"},
		{"\\e[7^", "Ctrl+Home"}, {"\\e[8^", "Ctrl+End"},

		{"\\e\\e[7^", "Ctrl+Alt+Home"}, {"\\e\\e[8^", "Ctrl+Alt+End"},
		{"\\e\\e[2^", "Ctrl+Alt+Ins"}, {"\\e\\e[3^", "Ctrl+Alt+Del"},
		{"\\e\\e[5^", "Ctrl+Alt+PgUp"}, {"\\e\\e[6^", "Ctrl+Alt+PgDn"},

		{"\\e[2@", "Ctrl+Shift+Ins"},	{"\\e[3@", "Ctrl+Shift+Del"},
		{"\\e[7@", "Ctrl+Shift+Home"}, {"\\e[8@", "Ctrl+Shift+End"},

		/* Vte-specific */
		{"\\e[01;2P", "Shift+F1"}, {"\\e[01;2Q", "Shift+F2"},
		{"\\e[01;2R", "Shift+F3"}, {"\\e[01;2S", "Shift+F4"},

		{"\\e[01;3P", "Alt+F1"}, {"\\e[01;3Q", "Alt+F2"},
		{"\\e[01;3R", "Alt+F3"}, {"\\e[01;3S", "Alt+F4"},

		{"\\e[01;5P", "Ctrl+F1"}, {"\\e[01;5Q", "Ctrl+F2"},
		{"\\e[01;5R", "Ctrl+F3"}, {"\\e[01;5S", "Ctrl+F4"},

		{"\\eOH", "Home"}, {"\\eOF", "End"},

		/* kitty keyboard protocol */
		{"\\e[P", "F1"}, {"\\e[Q", "F2"}, {"\\e[S", "F4"},

		/* emacs and others */
		{"\\eOA", "Up"}, {"\\eOB", "Down"},
		{"\\eOC", "Right"}, {"\\eOD", "Left"},
		{"\\eO5A", "Ctrl+Up"}, {"\\eO5B", "Ctrl+Down"},
		{"\\eO5C", "Ctrl+Right"}, {"\\eO5D", "Ctrl+Left"},

		{"\\e[5A", "Ctrl+Up"},{"\\e[5B", "Ctrl+Down"},
		{"\\e[5C", "Ctrl+Right"},{"\\e[5D", "Ctrl+Left"},

		{"\\e[2A", "Shift+Up"}, {"\\e[2B", "Shift+Down"},
		{"\\e[2C", "Shift+Right"}, {"\\e[2D", "Shift+Left"},

		{"\\e[1~", "Home"}, {"\\e[4~", "End"},

		{"\\e[4h", "Ins"}, {"\\e[L", "Ctrl+Ins"}, /* st */
		{"\\e[M", "Ctrl+Del"},

		/* sun-color uses \e[224z-\e[235z for F1-F12 keys. */
		/* cons25 uses \e[M-\e[X for F1-F12 keys. */

		{NULL, NULL},
	};

	size_t i;
	for (i = 0; keys[i].key; i++) {
		if (strcmp(key, keys[i].key) == 0)
			return keys[i].translation;
	}

	return translate_key_nofunc(key);
}

static int
backup_and_create_kbinds_file(void)
{
	char *backup = gen_backup_file(kbinds_file, 1);
	if (!backup)
		return FUNC_FAILURE;

	if (renameat(XAT_FDCWD, kbinds_file, XAT_FDCWD, backup) != 0) {
		xerror(_("kb: Cannot rename '%s' to '%s': %s\n"),
			kbinds_file, backup, strerror(errno));
		free(backup);
		return FUNC_FAILURE;
	}

	char *abbrev = abbreviate_file_name(backup);
	printf(_("Old keybindings file saved as '%s'\n"),
		abbrev ? abbrev : backup);
	free(backup);
	free(abbrev);

	return create_kbinds_file();
}

int
kbinds_reset(void)
{
	int exit_status = FUNC_SUCCESS;
	struct stat attr;

	if (!kbinds_file || !*kbinds_file) {
		xerror(_("kb: No keybindings file found\n"));
		return FUNC_FAILURE;
	}

	if (stat(kbinds_file, &attr) == -1)
		exit_status = create_kbinds_file();
	else
		exit_status = backup_and_create_kbinds_file();

	if (exit_status == FUNC_SUCCESS)
		err('n', PRINT_PROMPT, _("kb: Restart %s for changes "
			"to take effect\n"), PROGRAM_NAME);

	return exit_status;
}

static int
kbinds_edit(char *app)
{
	if (xargs.stealth_mode == 1) {
		printf("%s: kb: %s\n", PROGRAM_NAME, STEALTH_DISABLED);
		return FUNC_SUCCESS;
	}

	if (!kbinds_file) {
		xerror(_("kb: No keybindings file found\n"));
		return FUNC_FAILURE;
	}

	struct stat attr;
	if (stat(kbinds_file, &attr) == -1) {
		create_kbinds_file();
		stat(kbinds_file, &attr);
	}

	const time_t mtime_bfr = attr.st_mtime;

	const int ret = open_config_file(app, kbinds_file);
	if (ret != FUNC_SUCCESS)
		return ret;

	stat(kbinds_file, &attr);
	if (mtime_bfr == attr.st_mtime)
		return FUNC_SUCCESS;

	err('n', PRINT_PROMPT, _("kb: Restart %s for changes to "
		"take effect\n"), PROGRAM_NAME);
	return FUNC_SUCCESS;
}

/* FUNC_NAME is not NULL when invokd by 'kb conflict' (in which case we're
 * check a readline key sequence). Otherwise, if invoked by 'kb bind',
 * FUNC_NAME isn't set.
 * We display different messages depending on the invoking command.
 * Returns the number of conflicts found. */
static int
check_clifm_kb(const char *kb, const char *func_name)
{
	int conflicts = 0;

	size_t i;
	for (i = 0; i < kbinds_n; i++) {
		if (!kbinds[i].key || strcmp(kb, kbinds[i].key) != 0)
			continue;

		if (func_name != NULL) {
			fprintf(stderr, _("kb: '%s' conflicts with '%s' (readline)\n"),
				kbinds[i].function ? kbinds[i].function : "unnamed", func_name);
		} else {
			const char *func = kbinds[i].function
				? kbinds[i].function : "unnamed";
			const char *t = translate_key(kbinds[i].key);

			fprintf(stderr, _("kb: %s: Key already in use by '%s'.\n"),
				t ? t : kbinds[i].key, func);
		}

		conflicts++;
	}

	return conflicts;
}

/* Check all readline key sequences against the key seqeunce KB, if not NULL
 * (this is the case when validating a key entered via 'kb bind').
 * Otherwise the check is made against all clifm key sequences (i.e., when
 * the invoking command is 'kb conflict').
 * Returns the number of conflicts found. */
static int
check_rl_kbinds(const char *kb)
{
	size_t i, j;
	char *name = (char *)NULL;
	char **names = (char **)rl_funmap_names();
	int conflicts = 0;

	if (!names)
		return FUNC_SUCCESS;

	for (i = 0; (name = names[i]); i++) {
		rl_command_func_t *function = rl_named_function(name);
		char **keys = rl_invoking_keyseqs(function);
		if (!keys)
			continue;

		for (j = 0; keys[j]; j++) {
			if (kb == NULL) {
				conflicts += check_clifm_kb(keys[j], name);
			} else {
				if (strcmp(kb, keys[j]) == 0) {
					const char *t = translate_key(kb);
					fprintf(stderr, _("kb: %s: Key already in use by '%s' "
						"(readline)\n"), t ? t : kb, name);
					conflicts++;
				}
			}
			free(keys[j]);
		}
		free(keys);
	}

	free(names);

	return conflicts;
}

static int
check_kbinds_conflict(void)
{
	if (kbinds_n == 0) {
		puts(_("kb: No keybindings defined"));
		return FUNC_SUCCESS;
	}

	int ret = FUNC_SUCCESS;
	size_t i, j;
	for (i = 0; i < kbinds_n; i++) {
		for (j = i + 1; j < kbinds_n; j++) {
			if (strcmp(kbinds[i].key, kbinds[j].key) == 0) {
				fprintf(stderr, _("kb: '%s' conflicts with '%s'\n"),
					kbinds[i].function, kbinds[j].function);
				ret = FUNC_FAILURE;
			}
		}
	}

	if (check_rl_kbinds(NULL) > 0)
		ret = FUNC_FAILURE;

	return ret;
}

/* Retrieve the key sequence associated to FUNCTION */
static char *
find_key(const char *function)
{
	if (kbinds_n == 0)
		return (char *)NULL;

	int n = (int)kbinds_n;
	while (--n >= 0) {
		if (*function != *kbinds[n].function)
			continue;
		if (strcmp(function, kbinds[n].function) == 0)
			return kbinds[n].key;
	}

	return (char *)NULL;
}

/* Read a key sequence from STDIN and return its value (memory to hold this
 * value is allocated via malloc(2)). */
static char *
get_new_keybind(void)
{
	char buf[64];
	size_t len = 0;
	int ret = 0;
	int result = 0;
	int ch = 0;
	int prev = 0;

	buf[0] = '\0';

	putchar(':');
	fflush(stdout);

	if (enable_raw_mode(STDIN_FILENO) == -1) {
		UNHIDE_CURSOR;
		return (char *)NULL;
	}

	while (1) {
		result = (int)read(STDIN_FILENO, &ch, sizeof(unsigned char)); /* flawfinder: ignore */
		if (result <= 0 || len >= sizeof(buf) - 1) {
			buf[len] = '\0';
			break;
		}

		if (result != sizeof(unsigned char))
			continue;

		const unsigned char c = (unsigned char)ch;

		if (prev != KEY_ESC) {
			if (c == KEY_ENTER)
				break;

			if (c == CTRL('D')) {
				buf[0] = '\0';
				break;
			}

			if (c == CTRL('C')) {
				putchar('\r');
				MOVE_CURSOR_RIGHT(1);
				ERASE_TO_RIGHT;
				fflush(stdout);
				buf[0] = '\0';
				len = 0;
				continue;
			}
		}

		const size_t remaining = sizeof(buf) - len - 1;

		if (c == KEY_ESC) {
			ret = snprintf(buf + len, remaining, "\\e");
		} else if (isprint(c) && c != ' ') {
			ret = snprintf(buf + len, remaining, "%c", c);
		} else if (c <= 31) {
			ret = snprintf(buf + len, remaining, "\\C-%c", c + '@' - 'A' + 'a');
		} else {
			ret = snprintf(buf + len, remaining, "\\x%x", c);
		}

		prev = ch;
		if (ret < 0 || (size_t)ret >= remaining)
			continue;

		/* Kitty keyboard protocol */
		if (*buf && strstr(buf, "\\e[100;5u")) { /* Ctrl+d */
			MOVE_CURSOR_LEFT(8);
			ERASE_TO_RIGHT;
			fflush(stdout);
			buf[0] = '\0';
			break;
		}

		if (*buf && strstr(buf, "\\e[99;5u")) { /* Ctrl+c */
			putchar('\r');
			MOVE_CURSOR_RIGHT(1);
			ERASE_TO_RIGHT;
			fflush(stdout);
			buf[0] = '\0';
			len = 0;
			continue;
		}

		if (*buf) {
			fputs(buf + len, stdout);
			fflush(stdout);
		}

		len += (size_t)ret;
	}

	disable_raw_mode(STDIN_FILENO);
	putchar('\n');

	if (!*buf)
		return (char *)NULL;

	return savestring(buf, strlen(buf));
}

/* Append the key sequence KB associated to the function name FUNC_NAME
 * to the keybingins file. */
static int
append_kb_to_file(const char *func_name, const char *kb)
{
	FILE *fp = open_fappend(kbinds_file);
	if (!fp) {
		xerror(_("kb: Cannot open '%s': %s\n"), kbinds_file, strerror(errno));
		return FUNC_FAILURE;
	}

	fprintf(fp, "\n%s:%s\n", func_name, kb);
	fclose(fp);

	return FUNC_SUCCESS;
}

/* Edit the keybindings file and update the key sequence bound to FUNC_NAME
 * with the key sequence KB. */
static int
rebind_kb(const char *func_name, const char *kb)
{
	int orig_fd = 0;
	FILE *orig_fp = open_fread(kbinds_file, &orig_fd);
	if (!orig_fp) {
		xerror(_("kb: Cannot open '%s': %s\n"), kbinds_file, strerror(errno));
		return FUNC_FAILURE;
	}

	const size_t len = strlen(config_dir_gral) + (sizeof(TMP_FILENAME) - 1) + 2;
	char *tmp_name = xnmalloc(len, sizeof(char));
	snprintf(tmp_name, len, "%s/%s", config_dir_gral, TMP_FILENAME);

	const int tmp_fd = mkstemp(tmp_name);
	if (tmp_fd == -1) {
		xerror(_("kb: Error creating temporary file: %s\n"), strerror(errno));
		goto ERROR;
	}

	FILE *tmp_fp = fdopen(tmp_fd, "w");
	if (!tmp_fp) {
		xerror(_("kb: Cannot open temporary file: %s\n"), strerror(errno));
		unlinkat(tmp_fd, tmp_name, 0);
		close(tmp_fd);
		goto ERROR;
	}

	int found = 0;
	const size_t func_len = strlen(func_name);
	char line[NAME_MAX];
	while (fgets(line, sizeof(line), orig_fp) != NULL) {
		if (found == 0 && strncmp(line, func_name, func_len) == 0
		&& line[func_len] == ':') {
			fprintf(tmp_fp, "%s:%s\n", func_name, kb);
			found = 1;
		} else {
			fputs(line, tmp_fp);
		}
	}

	if (found == 1) {
		if (renameat(tmp_fd, tmp_name, orig_fd, kbinds_file) == -1)
			xerror(_("kb: Cannot rename '%s' to '%s': %s\n"),
				tmp_name, kbinds_file, strerror(errno));
	} else {
		unlinkat(tmp_fd, tmp_name, 0);
	}

	fclose(orig_fp);
	fclose(tmp_fp);

	free(tmp_name);

	if (found == 0)
		return append_kb_to_file(func_name, kb);

	return FUNC_SUCCESS;

ERROR:
	free(tmp_name);
	fclose(orig_fp);
	return FUNC_FAILURE;
}

/* Check the key sequence KB against the list of clifm's key sequences.
 * Return FUNC_FAILURE in case of conflict, or FUNC_SUCCESS. */
static int
check_func_name(const char *func_name)
{
	const size_t len = strlen(func_name);

	for (size_t i = 0; kb_cmds[i].name; i++) {
		if (len == kb_cmds[i].len && *kb_cmds[i].name == *func_name
		&& strcmp(kb_cmds[i].name, func_name) == 0)
			return FUNC_SUCCESS;
	}

	return FUNC_FAILURE;
}

/* Check the key sequence KB against both clifm and readline key sequences.
 * Return the number of conflicts found. */
static int
check_kb_conflicts(const char *kb)
{
	if (!strchr(kb, '\\') && strcmp(kb, "-") != 0) {
		fprintf(stderr, _("kb: Invalid keybinding\n"));
		return FUNC_FAILURE;
	}

	if (*kb == '-' && !kb[1])
		return FUNC_SUCCESS;

	int conflicts = 0;
	conflicts += check_clifm_kb(kb, NULL);
	conflicts += check_rl_kbinds(kb);

	return conflicts;
}

/* Bind the function FUNC_NAME to a new key sequence. */
static int
bind_kb_func(const char *func_name)
{
	if (xargs.stealth_mode == 1) {
		printf("%s: kb: %s\n", PROGRAM_NAME, STEALTH_DISABLED);
		return FUNC_SUCCESS;
	}

	if (!kbinds_file || !*kbinds_file) {
		xerror(_("kb: No keybindings file found\n"));
		return FUNC_FAILURE;
	}

	if (!func_name || !*func_name) {
		puts(KB_USAGE);
		return FUNC_SUCCESS;
	}

	if (check_func_name(func_name) == FUNC_FAILURE) {
		xerror(_("kb: '%s': Invalid function name\nType 'kb bind <TAB>' "
			"to list available function names\n"), func_name);
		return FUNC_FAILURE;
	}

	struct stat a;
	if (stat(kbinds_file, &a) == -1 && create_kbinds_file() == FUNC_FAILURE)
		return FUNC_FAILURE;

	const char *func_key = find_key(func_name);
	const char *translated_key = func_key ? translate_key(func_key) : "unset";
	/* translated_key can only be NULL if func_key is not NULL. */
	const char *cur_key = translated_key ? translated_key : func_key;

	printf(_("Enter a keybinding for %s%s%s (current: %s%s%s)\n"),
		BOLD, func_name, df_c, BOLD, cur_key, df_c);
	puts(_("(Enter:accept, Ctrl+d:abort, Ctrl+c:clear-line)"));
	puts(_("To unset the function enter '-'"));

	char *kb = get_new_keybind();
	if (kb == NULL)
		return FUNC_SUCCESS;

	const int unset_key = (*kb == '-' && !kb[1]);
	if (unset_key == 0 && check_kb_conflicts(kb) == 0) {
		/* If any conflict was found (check_kb_conflicts() is greater than
		 * zero), the function already displayed the keybinding translation. */
		const char *translation = translate_key(kb);
		printf(_("New key: %s\n"), translation ? translation : kb);
	}

	const char *msg = unset_key == 1 ? _("Unset this function?")
		: _("Bind function to this new key?");

	if (rl_get_y_or_n(msg, 0) == 0) {
		free(kb);
		return FUNC_SUCCESS;
	}

	const int ret = rebind_kb(func_name, kb);
	free(kb);

	if (ret == FUNC_SUCCESS) {
		err('n', PRINT_PROMPT, _("kb: Restart %s for changes to "
			"take effect\n"), PROGRAM_NAME);
	}

	return ret;
}

static int
list_kbinds(void)
{
	if (kbinds_n == 0) {
		puts(_("kb: No keybindings defined\n"));
		return FUNC_SUCCESS;
	}

	size_t flen = 0;
	size_t i;
	for (i = 0; i < kbinds_n; i++) {
		const size_t l = kbinds[i].function ? strlen(kbinds[i].function) : 0;
		if (l > flen)
			flen = l;
	}

	for (i = 0; i < kbinds_n; i++) {
		if (!kbinds[i].key || !kbinds[i].function)
			continue;

		const char *translation = translate_key(kbinds[i].key);
		printf("%-*s (%s)\n", (int)flen, kbinds[i].function,
			translation ? translation : kbinds[i].key);
	}

	return FUNC_SUCCESS;
}

/* Print the list of readline functions and associated keys. */
static int
list_rl_kbinds(void)
{
	size_t i, j;
	char *name = (char *)NULL;
	char **names = (char **)rl_funmap_names();

	if (!names)
		return FUNC_SUCCESS;

	size_t flen = 0;
	for (i = 0; (name = names[i]); i++) {
		rl_command_func_t *function = rl_named_function(name);
		char **keys = rl_invoking_keyseqs(function);
		if (!keys)
			continue;

		for (j = 0; keys[j]; j++)
			free(keys[j]);
		free(keys);

		const size_t l = strlen(name);
		if (l > flen)
			flen = l;
	}

	for (i = 0; (name = names[i]); i++) {
		if ((*name == 's' && strcmp(name, "self-insert") == 0)
		|| (*name == 'd' && strcmp(name, "do-lowercase-version") == 0))
			continue;

		rl_command_func_t *function = rl_named_function(name);
		char **keys = rl_invoking_keyseqs(function);
		if (!keys)
			continue;

		printf("%-*s ", (int)flen, name);

		char *prev = NULL;
		for (j = 0; keys[j]; j++) {
			const char *t = translate_key(keys[j]);

			/* Skip consecutive duplicates. */
			if (prev && t && *prev == *t && strcmp(prev, t) == 0) {
				free(keys[j]);
				continue;
			}

			printf("(%s) ", t ? t : keys[j]);
			free(keys[j]);

			if (t)
				prev = (char *)t;
		}

		putchar('\n');
		free(keys);
	}

	free(names);

	puts(_("\nNote: Bear in mind that clifm's keybindings take precedence "
		"over readline's.\nTo modify readline's keybindings edit "
		"~/.config/clifm/readline.clifm"));

	return FUNC_SUCCESS;
}

int
kbinds_function(char **args)
{
	if (!args)
		return FUNC_FAILURE;

	if (!args[1] || strcmp(args[1], "list") == 0)
		return list_kbinds();

	if (IS_HELP(args[1])) {
		puts(_(KB_USAGE));
		return FUNC_SUCCESS;
	}

	if (*args[1] == 'b' && strcmp(args[1], "bind") == 0)
		return bind_kb_func(args[2]);

	if (*args[1] == 'c' && strcmp(args[1], "conflict") == 0)
		return check_kbinds_conflict();

	if (*args[1] == 'e' && strcmp(args[1], "edit") == 0)
		return kbinds_edit(args[2] ? args[2] : NULL);

	if (*args[1] == 'r' && strcmp(args[1], "reset") == 0)
		return kbinds_reset();

	if (*args[1] == 'r' && strcmp(args[1], "readline") == 0)
		return list_rl_kbinds();

	fprintf(stderr, "%s\n", _(KB_USAGE));
	return FUNC_FAILURE;
}

/* Store keybinds from the keybinds file in a struct. */
int
load_keybinds(void)
{
	if (config_ok == 0 || !kbinds_file)
		return FUNC_FAILURE;

	/* Free the keybinds struct array */
	if (kbinds_n > 0) {
		int i = (int)kbinds_n;

		while (--i >= 0) {
			free(kbinds[i].function);
			free(kbinds[i].key);
		}

		free(kbinds);
		kbinds = xnmalloc(1, sizeof(struct kbinds_t));
		kbinds_n = 0;
	}

	/* Open the keybinds file */
	FILE *fp = fopen(kbinds_file, "r");
	if (!fp)
		return FUNC_FAILURE;

	size_t line_size = 0;
	char *line = (char *)NULL;
	ssize_t line_len = 0;

	while ((line_len = getline(&line, &line_size, fp)) > 0) {
		if (!line || !*line || *line == '#' || *line == '\n')
			continue;

		if (line[line_len - 1] == '\n')
			line[line_len - 1] = '\0';

		char *tmp = (char *)NULL;
		tmp = strchr(line, ':');
		if (!tmp || !*(tmp + 1))
			continue;

		/* Now copy left and right values of each keybinding to the
		 * kbinds struct. */
		kbinds = xnrealloc(kbinds, kbinds_n + 1, sizeof(struct kbinds_t));
		kbinds[kbinds_n].key = savestring(tmp + 1, strlen(tmp + 1));

		*tmp = '\0';

		kbinds[kbinds_n].function = savestring(line, strlen(line));
		kbinds_n++;
	}

	fclose(fp);
	free(line);

	if (kbinds_n > 1)
		qsort(kbinds, kbinds_n, sizeof(*kbinds), (QSFUNC *)compare_strings);

	return FUNC_SUCCESS;
}

/* This call to prompt() just updates the prompt in case it was modified by
 * a keybinding, for example, chdir, files selection, trash, and so on.
 * The screen is not refreshed in any way. */
static void
rl_update_prompt(void)
{
	if (rl_line_buffer) {
		memset(rl_line_buffer, '\0', (size_t)rl_end);
		rl_point = rl_end = 0;
	}

	prompt(exec_prompt_cmds ? PROMPT_UPDATE_RUN_CMDS : PROMPT_UPDATE,
		PROMPT_NO_SCREEN_REFRESH);
	exec_prompt_cmds = 0;
	UNHIDE_CURSOR;
}

#if defined(__HAIKU__) || !defined(_NO_PROFILES)
/* Old version of rl_update_prompt(). Used only by rl_profile_previous() and
 * rl_profile_next(): if any of these functions use rl_update_prompt() instead,
 * no prompt is printed (not sure why). FIX. */
static void
rl_update_prompt_old(void)
{
	HIDE_CURSOR;
	char *input = prompt(PROMPT_SHOW, PROMPT_NO_SCREEN_REFRESH);
	free(input);
}
#endif /* __HAIKU__ || !_NO_PROFILES */

static void
xrl_update_prompt(void)
{
#ifdef __HAIKU__
	rl_update_prompt_old();
#else
	rl_update_prompt();
#endif /* __HAIKU__ */
}

/* Run any command recognized by Clifm via a keybind. Example:
 * keybind_exec_cmd("sel *") */
int
keybind_exec_cmd(char *str)
{
	const size_t old_args = args_n;
	args_n = 0;

#ifndef _NO_SUGGESTIONS
	if (suggestion.printed == 1 && suggestion_buf)
		clear_suggestion(CS_FREEBUF);
#endif /* !_NO_SUGGESTIONS */

	int exit_status = FUNC_FAILURE;
	char **cmd = parse_input_str(str);
	putchar('\n');

	if (cmd) {
		exit_status = exec_cmd_tm(cmd);

		/* While in the bookmarks or mountpoints screen, the kbind_busy
		 * flag will be set to 1 and no keybinding will work. Once the
		 * corresponding function exited, set the kbind_busy flag to zero,
		 * so that keybindings work again. */
		if (kbind_busy == 1)
			kbind_busy = 0;

		int i = (int)args_n + 1;
		while (--i >= 0)
			free(cmd[i]);
		free(cmd);

		xrl_update_prompt();
	}

	args_n = old_args;
	return exit_status;
}

static int
run_kb_cmd(char *cmd, const int ignore_empty_line)
{
	if (!cmd || !*cmd)
		return FUNC_FAILURE;

	if (kbind_busy == 1)
		return FUNC_SUCCESS;

#ifndef _NO_SUGGESTIONS
	if (conf.colorize == 1 && wrong_cmd == 1)
		recover_from_wrong_cmd();
#endif /* _NO_SUGGESTIONS */

	const int exit_code_bk = exit_code;

	keybind_exec_cmd(cmd);
	rl_reset_line_state();

	if (exit_code != exit_code_bk)
		/* The exit code was changed by the executed command. Force the
		 * input taking function (my_rl_getc) to update the value of
		 * prompt_offset to correctly calculate the cursor position. */
		prompt_offset = UNSET;

	g_prompt_ignore_empty_line = ignore_empty_line;
	return FUNC_SUCCESS;
}

int
rl_toggle_max_filename_len(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1)
		return FUNC_SUCCESS;

	static int mnl_bk = 0, flag = 0;

	if (flag == 0 || conf.trunc_names == 0) {
		mnl_bk = conf.max_name_len_bk;
		flag = 1;
	}

	if (conf.max_name_len == UNSET) {
		conf.max_name_len = mnl_bk;
		mnl_bk = UNSET;
	} else {
		mnl_bk = conf.max_name_len;
		conf.max_name_len = UNSET;
	}

	update_autocmd_opts(AC_MAX_NAME_LEN);

	if (conf.autols == 1) {
		if (conf.clear_screen == 0)
			putchar('\n');
		reload_dirlist();
	}

	if (conf.max_name_len == UNSET) {
		print_reload_msg(NULL, NULL, _("Max name length unset\n"));
	} else {
		print_reload_msg(NULL, NULL, _("Max name length set to %d\n"),
			conf.max_name_len);
	}

	xrl_reset_line_state();
	return FUNC_SUCCESS;
}

/* Prepend authentication program name (typically sudo or doas) to the current
 * input string. */
static int
rl_prepend_sudo(int count, int key)
{
	UNUSED(count); UNUSED(key);

	if (kbind_busy == 1)
		return FUNC_SUCCESS;

#ifndef _NO_SUGGESTIONS
	if (suggestion.printed && suggestion_buf) {
		clear_suggestion(CS_FREEBUF);
		fputs(df_c, stdout);
	}
#endif /* !_NO_SUGGESTIONS */

	int free_s = 1;
	size_t len = 0;
	char *t = sudo_cmd;
	char *s = (char *)NULL;

	if (t) {
		len = strlen(t);
		if (len > 0 && t[len - 1] != ' ') {
			s = xnmalloc(len + 2, sizeof(char));
			snprintf(s, len + 2, "%s ", t);
			len++;
		} else {
			s = t;
			free_s = 0;
		}
	} else {
		len = strlen(DEF_SUDO_CMD) + 1;
		s = xnmalloc(len + 1, sizeof(char));
		snprintf(s, len + 1, "%s ", DEF_SUDO_CMD);
	}

	char *c = (char *)NULL;
	if (conf.highlight == 1 && conf.colorize == 1
	&& cur_color && cur_color != tx_c) {
		c = cur_color;
		fputs(tx_c, stdout);
	}

	const int p = rl_point;
	if (*rl_line_buffer == *s
	&& strncmp(rl_line_buffer, s, len) == 0) {
		rl_delete_text(0, (int)len);
		rl_point = p - (int)len;
	} else {
		rl_point = 0;
		rl_insert_text(s);
		rl_point = p + (int)len;
		if (c)
			rl_redisplay();
	}

	if (c)
		fputs(c, stdout);

	if (free_s)
		free(s);

#ifndef _NO_SUGGESTIONS
	if (suggestion.offset == 0 && suggestion_buf) {
		int r = rl_point;
		rl_point = rl_end;
		clear_suggestion(CS_FREEBUF);
		rl_point = r;
	}
#endif /* !_NO_SUGGESTIONS */

#ifndef _NO_HIGHLIGHT
	if (conf.highlight == 1) {
		int r = rl_point;
		rl_point = 0;
		recolorize_line();
		rl_point = r;
	}
#endif /* !_NO_HIGHLIGHT */

	return FUNC_SUCCESS;
}

static int
rl_create_file(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_kb_cmd("n", 1);
}

#ifndef _NO_SUGGESTIONS
/* Insert the accepted suggestion into the current input line
 * (highlighting words and special chars if syntax highlighting is enabled) */
static void
my_insert_text(char *text, char *s, const char _s)
{
#ifdef _NO_HIGHLIGHT
	UNUSED(s); UNUSED(_s);
#endif /* !_NO_HIGHLIGHT */

	if (!text || !*text)
		return;
	{
	if (wrong_cmd == 1 || cur_color == hq_c)
		goto INSERT_TEXT;

#ifndef _NO_HIGHLIGHT
	if (conf.highlight == 1) {
		/* Hide the cursor to minimize flickering */
		HIDE_CURSOR;
		/* Set text color to default */
		fputs(tx_c, stdout);
		cur_color = tx_c;
		char *t = text;
		size_t i;

		/* We only need to redisplay first suggested word if it contains
		 * a highlighting char and it is not preceded by a space */
		int redisplay = 0;
		if (accept_first_word == 1) {
			for (i = 0; t[i]; i++) {
				if (t[i] >= '0' && t[i] <= '9') {
					if (i == 0 || t[i - 1] != ' ') {
						redisplay = 1;
						break;
					}
				}
				switch (t[i]) {
				case '/': /* fallthrough */
				case '"': /* fallthrough */
				case '\'': /* fallthrough */
				case '&': /* fallthrough */
				case '|': /* fallthrough */
				case ';': /* fallthrough */
				case '>': /* fallthrough */
				case '(': /* fallthrough */
				case '[': /* fallthrough */
				case '{': /* fallthrough */
				case ')': /* fallthrough */
				case ']': /* fallthrough */
				case '}': /* fallthrough */
				case '$': /* fallthrough */
				case '-': /* fallthrough */
				case '~': /* fallthrough */
				case '*': /* fallthrough */
				case '#':
					if (i == 0 || t[i - 1] != ' ')
						redisplay = 1;
					break;
				default: break;
				}
				if (redisplay == 1)
					break;
			}
		}

		char q[PATH_MAX + 1];
		int l = 0;
		for (i = 0; t[i]; i++) {
			rl_highlight(t, i, SET_COLOR);
			if ((signed char)t[i] < 0) {
				q[l] = t[i];
				l++;
				if ((signed char)t[i + 1] >= 0) {
					q[l] = '\0';
					l = 0;
					rl_insert_text(q);
				}
				continue;
			}
			q[0] = t[i];
			q[1] = '\0';
			rl_insert_text(q);
			if (accept_first_word == 0 || redisplay == 1)
				rl_redisplay();
		}

		if (s && redisplay == 1) {
			/* 1) rl_redisplay removes the suggestion from the current line
			 * 2) We need rl_redisplay to correctly print highlighting colors
			 * 3) We need to keep the suggestion when accepting only
			 * the first suggested word.
			 * In other words, if we correctly print colors, we lose the
			 * suggestion.
			 * As a workaround, let's reprint the suggestion */
			size_t slen = strlen(suggestion_buf);
			*s = _s ? _s : ' ';
			print_suggestion(suggestion_buf, slen, suggestion.color);
			*s = '\0';
		}

		UNHIDE_CURSOR;
	} else
#endif /* !_NO_HIGHLIGHT */
INSERT_TEXT:
	{
		rl_insert_text(text);
	}
	}
}

static int
rl_accept_suggestion(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1) {
		/* If not at the end of the typed string, just move the cursor
		 * forward one column */
		if (rl_point < rl_end)
			rl_point++;
		return FUNC_SUCCESS;
	}

	if (wrong_cmd == 0 && cur_color != hq_c) {
		cur_color = tx_c;
		fputs(tx_c, stdout);
	}

	/* Only accept the current suggestion if the cursor is at the end
	 * of the line typed so far */
	if (!conf.suggestions || rl_point != rl_end || !suggestion_buf
	|| suggestion.type == CMD_DESC_SUG) {
		if (rl_point < rl_end) {
			/* Just move the cursor forward one character */
			const int mlen = mblen(rl_line_buffer + rl_point, MB_CUR_MAX);
			rl_point += mlen;
		}
		return FUNC_SUCCESS;
	}

	/* If accepting the first suggested word, accept only up to next
	 * word delimiter. */
	char *s = (char *)NULL, truncated_char = 0;
	int truncated = 0, accept_first_word_last = 0;
	if (accept_first_word == 1) {
		char *p = suggestion_buf + (rl_point - suggestion.offset);
		/* Skip leading spaces */
		while (*p == ' ')
			p++;

		/* Skip all consecutive word delimiters from the beginning of the
		 * suggestion (P), except for slash and space. */
		while ((s = strpbrk(p, WORD_DELIMITERS)) == p && *s != '/' && *s != ' ')
			p++;
		if (s && s != p && *(s - 1) == ' ')
			s = strpbrk(p, WORD_DELIMITERS);

		while (s && IS_UTF8_CHAR(*s))
			s = strpbrk(s + 1, WORD_DELIMITERS);

		if (s && s[1]) { /* Truncate suggestion after word delimiter */
			if (*s == '/')
				++s;
			truncated_char = *s;
			*s = '\0';
			truncated = 1;
		} else { /* Last word: No word delimiter */
			const size_t len = strlen(suggestion_buf);
			if (len > 0 && suggestion_buf[len - 1] != '/'
			&& suggestion_buf[len - 1] != ' ')
				suggestion.type = NO_SUG;
			accept_first_word_last = 1;
			s = (char *)NULL;
		}
	}

	const int bypass_alias =
		(rl_line_buffer && *rl_line_buffer == '\\' && rl_line_buffer[1]);

	rl_delete_text(suggestion.offset, rl_end);
	rl_point = suggestion.offset;

	if (conf.highlight == 1 && accept_first_word == 0 && cur_color != hq_c) {
		cur_color = tx_c;
		rl_redisplay();
	}

	if (accept_first_word_last == 1)
		accept_first_word = 0;

	if (accept_first_word == 0 && (flags & BAEJ_SUGGESTION))
		clear_suggestion(CS_KEEPBUF);

	/* Complete according to the suggestion type */
	switch (suggestion.type) {
	case BACKDIR_SUG:    /* fallthrough */
	case JCMD_SUG:       /* fallthrough */
	case BOOKMARK_SUG:   /* fallthrough */
	case COMP_SUG:       /* fallthrough */
	case ELN_SUG:        /* fallthrough */
	case FASTBACK_SUG:   /* fallthrough */
	case FUZZY_FILENAME: /* fallthrough */
	case FILE_SUG: {
		char *tmp = (char *)NULL;
		size_t i, isquote = 0, backslash = 0;
		for (i = 0; suggestion_buf[i]; i++) {
			if (is_quote_char(suggestion_buf[i]))
				isquote = 1;

			if (suggestion_buf[i] == '\\') {
				backslash = 1;
				break;
			}
		}

		if (isquote == 1 && backslash == 0) {
			if (suggestion.type == ELN_SUG && suggestion.filetype == DT_REG
			&& conf.quoting_style != QUOTING_STYLE_BACKSLASH)
				tmp = quote_str(suggestion_buf);
			else
				tmp = escape_str(suggestion_buf);
		}

		my_insert_text(tmp ? tmp : suggestion_buf, NULL, 0);
		free(tmp);

		if (suggestion.type == FASTBACK_SUG) {
			if (conf.highlight == 0) {
				rl_insert_text("/");
			} else if (*suggestion_buf != '/' || suggestion_buf[1]) {
				fputs(hd_c, stdout);
				rl_insert_text("/");
				rl_redisplay();
				fputs(df_c, stdout);
			}
		} else if (suggestion.filetype != DT_DIR
		&& suggestion.type != BOOKMARK_SUG && suggestion.type != BACKDIR_SUG) {
			rl_stuff_char(' ');
		}
		suggestion.type = NO_SUG;
		}
		break;

	case FIRST_WORD:
		my_insert_text(suggestion_buf, s, truncated_char); break;

	case SEL_SUG:      /* fallthrough */
	case HIST_SUG:     /* fallthrough */
	case BM_NAME_SUG:  /* fallthrough */
	case PROMPT_SUG:   /* fallthrough */
	case NET_SUG:      /* fallthrough */
	case CSCHEME_SUG:  /* fallthrough */
	case WS_NAME_SUG:  /* fallthrough */
	case INT_HELP_SUG: /* fallthrough */
	case PROFILE_SUG:  /* fallthrough */
	case DIRHIST_SUG:
		my_insert_text(suggestion_buf, NULL, 0); break;

#ifndef _NO_TAGS
	case TAGT_SUG: /* fallthrough */
	case TAGC_SUG: /* fallthrough */
	case TAGS_SUG: /* fallthrough */
	case WS_PREFIX_SUG: /* fallthrough */
	case WS_NUM_PREFIX_SUG:
	case BM_PREFIX_SUG: {
		char prefix[3]; prefix[0] = '\0';
		if (suggestion.type == TAGC_SUG) {
			prefix[0] = ':'; prefix[1] = '\0';
		} else if (suggestion.type == TAGT_SUG) {
			prefix[0] = 't'; prefix[1] = ':'; prefix[2] = '\0';
		} else if (suggestion.type == BM_PREFIX_SUG) {
			prefix[0] = 'b'; prefix[1] = ':'; prefix[2] = '\0';
		} else if (suggestion.type == WS_PREFIX_SUG
		|| suggestion.type == WS_NUM_PREFIX_SUG) {
			prefix[0] = 'w'; prefix[1] = ':'; prefix[2] = '\0';
		}

		rl_insert_text(prefix);
		char *p = (suggestion.type != BM_PREFIX_SUG
			&& suggestion.type != WS_PREFIX_SUG)
			? escape_str(suggestion_buf) : (char *)NULL;

		my_insert_text(p ? p : suggestion_buf, NULL, 0);

		if (suggestion.type != BM_PREFIX_SUG
		&& (fzftab != 1 || (suggestion.type != TAGT_SUG)))
			rl_stuff_char(' ');
		free(p);
		}
		break;
#endif /* _NO_TAGS */

	case WS_NUM_SUG: /* fallthrough */
	case USER_SUG: {
		char *p = escape_str(suggestion_buf);
		my_insert_text(p ? p : suggestion_buf, NULL, 0);
		if (suggestion.type == USER_SUG)
			rl_stuff_char('/');
		free(p);
		break;
	}

	default:
		if (bypass_alias == 1)
			rl_insert_text("\\");
		my_insert_text(suggestion_buf, NULL, 0);
		rl_stuff_char(' ');
		break;
	}

	/* Move the cursor to the end of the line. */
	rl_point = rl_end;
	if (accept_first_word == 0) {
		suggestion.printed = 0;
		free(suggestion_buf);
		suggestion_buf = (char *)NULL;
	} else {
		if (s && truncated == 1)
			/* Reinsert the char we removed to print only the first word. */
			*s = truncated_char;
		accept_first_word = 0;
	}

	flags &= ~BAEJ_SUGGESTION;
	return FUNC_SUCCESS;
}

static int
rl_accept_first_word(int count, int key)
{
	if (rl_point < rl_end)
		return rl_forward_word(1, 0);

	/* Accepting the first suggested word is not supported for ELN's,
	 * bookmark and alias names. */
	const int t = suggestion.type;
	if (t != ELN_SUG && t != BOOKMARK_SUG && t != ALIAS_SUG && t != JCMD_SUG
	&& t != FUZZY_FILENAME && t != CMD_DESC_SUG
	&& t != BM_NAME_SUG && t != INT_HELP_SUG && t != TAGT_SUG
	&& t != BM_PREFIX_SUG) {
		accept_first_word = 1;
		suggestion.type = FIRST_WORD;
	}
	return rl_accept_suggestion(count, key);
}
#endif /* !_NO_SUGGESTIONS */

static int
rl_refresh(int count, int key)
{
	UNUSED(count); UNUSED(key);
	exec_prompt_cmds = 1;
	return run_kb_cmd("rf", 0);
}

static int
rl_dir_parent(int count, int key)
{
	UNUSED(count); UNUSED(key);
	/* If already root dir, do nothing */
	if (*workspaces[cur_ws].path == '/' && !workspaces[cur_ws].path[1])
		return FUNC_SUCCESS;

	exec_prompt_cmds = 1;
	return run_kb_cmd("cd ..", 0);
}

static int
rl_dir_root(int count, int key)
{
	UNUSED(count); UNUSED(key);
	/* If already root dir, do nothing */
	if (*workspaces[cur_ws].path == '/' && !workspaces[cur_ws].path[1])
		return FUNC_SUCCESS;

	return run_kb_cmd("cd /", 0);
}

static int
rl_dir_home(int count, int key)
{
	UNUSED(count); UNUSED(key);
	/* If already in home, do nothing */
	if (*workspaces[cur_ws].path == *user.home
	&& strcmp(workspaces[cur_ws].path, user.home) == 0)
		return FUNC_SUCCESS;

	exec_prompt_cmds = 1;
	return run_kb_cmd("cd", 0);
}

static int
rl_dir_previous(int count, int key)
{
	UNUSED(count); UNUSED(key);
	/* If already at the beginning of dir hist, do nothing */
	if (dirhist_cur_index == 0)
		return FUNC_SUCCESS;

	exec_prompt_cmds = 1;
	return run_kb_cmd("b", 0);
}

static int
rl_dir_next(int count, int key)
{
	UNUSED(count); UNUSED(key);

	/* If already at the end of dir hist, do nothing */
	if (dirhist_cur_index + 1 == dirhist_total_index)
		return FUNC_SUCCESS;

	exec_prompt_cmds = 1;
	return run_kb_cmd("f", 0);
}

int
rl_toggle_long_view(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1 || xargs.disk_usage_analyzer == 1)
		return FUNC_SUCCESS;

#ifndef _NO_SUGGESTIONS
	if (suggestion.printed && suggestion_buf)
		free_suggestion();
#endif /* !_NO_SUGGESTIONS */

	conf.long_view = !conf.long_view;

	update_autocmd_opts(AC_LONG_VIEW);

	if (conf.autols == 1) {
		if (conf.clear_screen == 0)
		/* Without this putchar(), the first entries of the directories
		 * list are printed in the prompt line. */
			putchar('\n');
		reload_dirlist();
	}

	print_reload_msg(NULL, NULL, _("Long view: %s\n"),
		conf.long_view == 1 ? _("on") : _("off"));
	xrl_reset_line_state();
	return FUNC_SUCCESS;
}

int
rl_toggle_follow_link_long(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1 || conf.long_view == 0 || conf.light_mode == 1)
		return FUNC_SUCCESS;

#ifndef _NO_SUGGESTIONS
	if (suggestion.printed && suggestion_buf)
		free_suggestion();
#endif /* !_NO_SUGGESTIONS */

	conf.follow_symlinks_long = !conf.follow_symlinks_long;

	if (conf.autols == 1) {
		if (conf.clear_screen == 0)
		/* Without this putchar(), the first entries of the directories
		 * list are printed in the prompt line. */
			putchar('\n');
		reload_dirlist();
	}

	print_reload_msg(NULL, NULL, _("Follow links: %s\n"),
		conf.follow_symlinks_long == 1 ? _("on") : _("off"));
	xrl_reset_line_state();
	return FUNC_SUCCESS;
}

int
rl_toggle_dirs_first(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1)
		return FUNC_SUCCESS;

#ifndef _NO_SUGGESTIONS
	if (suggestion.printed && suggestion_buf)
		free_suggestion();
#endif /* !_NO_SUGGESTIONS */

	conf.list_dirs_first = !conf.list_dirs_first;

	if (conf.autols == 1) {
		if (conf.clear_screen == 0)
			putchar('\n');
		reload_dirlist();
	}

	print_reload_msg(NULL, NULL, _("Directories first: %s\n"),
		conf.list_dirs_first ? _("on") : _("off"));
	xrl_reset_line_state();
	return FUNC_SUCCESS;
}

int
rl_toggle_light_mode(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1)
		return FUNC_SUCCESS;

#ifndef _NO_SUGGESTIONS
	if (suggestion.printed && suggestion_buf)
		free_suggestion();
#endif /* !_NO_SUGGESTIONS */

	conf.light_mode = !conf.light_mode;
	update_autocmd_opts(AC_LIGHT_MODE);

	if (conf.autols == 1) {
		if (conf.clear_screen == 0)
			putchar('\n');
		reload_dirlist();
	}

	print_reload_msg(NULL, NULL, _("Light mode: %s\n"),
		conf.light_mode == 1 ? _("on") : _("off"));
	xrl_reset_line_state();

	/* RL_DISPATCHING is zero when called from lightmode_function(),
	 * in exec.c. Otherwise, it is called from a keybinding and
	 * rl_update_prompt() must be executed. */
	if (rl_dispatching == 1)
		rl_update_prompt();

	return FUNC_SUCCESS;
}

int
rl_toggle_hidden_files(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1)
		return FUNC_SUCCESS;

#ifndef _NO_SUGGESTIONS
	if (suggestion.printed && suggestion_buf)
		free_suggestion();
#endif /* !_NO_SUGGESTIONS */

	static int hidden_bk = 0;
	if (conf.show_hidden != 0) {
		hidden_bk = conf.show_hidden;
	} else {
		if (hidden_bk == 0)
			hidden_bk = 1;
	}
	conf.show_hidden = conf.show_hidden > 0 ? 0 : hidden_bk;

	update_autocmd_opts(AC_SHOW_HIDDEN);

	if (conf.autols == 1) {
		if (conf.clear_screen == 0)
			putchar('\n');
		reload_dirlist();
	}

	print_reload_msg(NULL, NULL, _("Hidden files: %s\n"),
		(conf.show_hidden > 0) ? _("on") : _("off"));

	xrl_reset_line_state();
	return FUNC_SUCCESS;
}

static int
rl_open_config(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_kb_cmd("config", 0);
}

static int
rl_open_keybinds(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_kb_cmd("kb edit", 0);
}

static int
rl_open_cscheme(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_kb_cmd("cs edit", 0);
}

static int
rl_open_bm_file(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_kb_cmd("bm edit", 0);
}

static int
rl_open_jump_db(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_kb_cmd("je", 0);
}

static int
rl_open_preview(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (!config_dir || kbind_busy == 1)
		return FUNC_FAILURE;

	char *file = xnmalloc(config_dir_len + 15, sizeof(char));
	snprintf(file, config_dir_len + 15, "%s/preview.clifm", config_dir);

	const int ret = open_file(file);
	free(file);
	rl_on_new_line();
	return ret;
}

static int
rl_open_mime(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_kb_cmd("mm edit", 0);
}

static int
rl_mountpoints(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_kb_cmd("mp", 1);
}

static int
rl_select_all(int count, int key)
{
	UNUSED(count); UNUSED(key);
	exec_prompt_cmds = 1;
	return run_kb_cmd("s ^", 0);
}

static int
rl_deselect_all(int count, int key)
{
	UNUSED(count); UNUSED(key);
	exec_prompt_cmds = 1;
	return run_kb_cmd("ds *", 0);
}

static int
rl_bookmarks(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_kb_cmd("bm", 1);
}

static int
rl_selbox(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_kb_cmd("ds", 1);
}

static int
rl_clear_line(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1 && alt_prompt == 0)
		return FUNC_SUCCESS;

	words_num = 0;

#ifndef _NO_HIGHLIGHT
	if (cur_color != tx_c) {
		cur_color = tx_c;
		fputs(cur_color, stdout);
	}
#endif /* !_NO_HIGHLIGHT */

#ifndef _NO_SUGGESTIONS
	if (wrong_cmd) {
		if (!recover_from_wrong_cmd())
			rl_point = 0;
	}
	if (suggestion.nlines > term_lines) {
		rl_on_new_line();
		return FUNC_SUCCESS;
	}

	if (suggestion_buf) {
		clear_suggestion(CS_FREEBUF);
		suggestion.printed = 0;
		suggestion.nlines = 0;
	}
#endif /* !_NO_SUGGESTIONS */
	curhistindex = current_hist_n;
	rl_kill_text(0, rl_end);
	rl_point = rl_end = 0;
	return FUNC_SUCCESS;
}

static int
rl_sort_next(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1)
		return FUNC_SUCCESS;

#ifndef _NO_SUGGESTIONS
	if (suggestion.printed && suggestion_buf)
		free_suggestion();
#endif /* !_NO_SUGGESTIONS */

#ifndef ST_BTIME
	if (conf.sort + 1 == SBTIME)
		conf.sort++;
#endif /* !ST_BTIME */

	if (conf.light_mode == 1)
		while (!ST_IN_LIGHT_MODE(conf.sort + 1) && conf.sort + 1 <= SORT_TYPES)
			conf.sort++;

	conf.sort++;
	if (conf.sort > SORT_TYPES)
		conf.sort = 0;

	if (conf.autols == 1) {
		sort_switch = 1;
		if (conf.clear_screen == 0)
			putchar('\n');
		reload_dirlist();
		sort_switch = 0;
	}

	xrl_update_prompt();
	update_autocmd_opts(AC_SORT);

	xrl_reset_line_state();
	return FUNC_SUCCESS;
}

static int
rl_sort_previous(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1)
		return FUNC_SUCCESS;

#ifndef _NO_SUGGESTIONS
	if (suggestion.printed && suggestion_buf)
		free_suggestion();
#endif /* !_NO_SUGGESTIONS */

#ifndef ST_BTIME
	if (conf.sort - 1 == SBTIME)
		conf.sort--;
#endif /* !ST_BTIME */

	if (conf.light_mode == 1)
		while (!ST_IN_LIGHT_MODE(conf.sort - 1) && conf.sort - 1 >= 0)
			conf.sort--;

	conf.sort--;
	if (conf.sort < 0)
		conf.sort = conf.light_mode == 1 ? SINO : SORT_TYPES;

	if (conf.autols == 1) {
		sort_switch = 1;
		if (conf.clear_screen == 0)
			putchar('\n');
		reload_dirlist();
		sort_switch = 0;
	}


	xrl_update_prompt();
	update_autocmd_opts(AC_SORT);

	xrl_reset_line_state();
	return FUNC_SUCCESS;
}

static int
rl_lock(int count, int key)
{
	UNUSED(count); UNUSED(key);
	int ret = FUNC_SUCCESS;
#ifndef _NO_SUGGESTIONS
	if (suggestion.printed && suggestion_buf)
		free_suggestion();
#endif /* !_NO_SUGGESTIONS */
	rl_deprep_terminal();

#if defined(__APPLE__)
	char *cmd[] = {"bashlock", NULL}; /* See https://github.com/acornejo/bashlock */
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) \
|| defined(__DragonFly__)
	char *cmd[] = {"lock", "-p", NULL};
#elif defined(__HAIKU__)
	char *cmd[] = {"peaclock", NULL};
#else
	char *cmd[] = {"vlock", NULL};
#endif /* __APPLE__ */
	ret = launch_execv(cmd, FOREGROUND, E_NOFLAG);

	rl_prep_terminal(0);
	xrl_reset_line_state();

	if (ret != FUNC_SUCCESS)
		return FUNC_FAILURE;
	return FUNC_SUCCESS;
}

static int
handle_no_sel(const char *func)
{
#ifndef _NO_SUGGESTIONS
	if (conf.colorize == 1 && wrong_cmd == 1)
		recover_from_wrong_cmd();

	if (suggestion.printed == 1 && suggestion_buf)
		clear_suggestion(CS_FREEBUF);
#endif /* !_NO_SUGGESTIONS */

	if (rl_end > 0) {
		rl_delete_text(0, rl_end);
		rl_point = rl_end = 0;
	}

	printf(_("\n%s: No selected files\n"), func);
	rl_reset_line_state();

	return FUNC_SUCCESS;
}

static int
rl_archive_sel(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1)
		return FUNC_SUCCESS;

	if (sel_n == 0)
		return handle_no_sel("ac");

	fputs(_("\nReady to archive/compress selected files."), stdout);
	return run_kb_cmd("ac sel", 1);
}

static int
rl_remove_sel(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1)
		return FUNC_SUCCESS;

	if (sel_n == 0)
		return handle_no_sel("r");

	rl_deprep_terminal();
	char cmd[] = "r sel";
	keybind_exec_cmd(cmd);
	g_prompt_ignore_empty_line = 1;
	rl_prep_terminal(0);
	rl_reset_line_state();
	return FUNC_SUCCESS;
}

static int
rl_export_sel(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1)
		return FUNC_SUCCESS;

	if (sel_n == 0)
		return handle_no_sel("exp");

	fputs(_("\nReady to export selected filenames"), stdout);
	return run_kb_cmd("exp sel", 0);
}

static int
rl_move_sel(int count, int key)
{
	UNUSED(count); UNUSED(key);

	if (sel_n == 0)
		return handle_no_sel("m");

	exec_prompt_cmds = 1;
	return run_kb_cmd("m sel", 0);
}

static int
rl_rename_sel(int count, int key)
{
	UNUSED(count); UNUSED(key);

	if (sel_n == 0)
		return handle_no_sel("br");

	exec_prompt_cmds = 1;
	return run_kb_cmd("br sel", 0);
}

static int
rl_paste_sel(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1)
		return FUNC_SUCCESS;

	if (sel_n == 0)
		return handle_no_sel("c");

	exec_prompt_cmds = 1;
	rl_deprep_terminal();
	char cmd[] = "c sel";
	keybind_exec_cmd(cmd);
	rl_prep_terminal(0);
	rl_reset_line_state();
	return FUNC_SUCCESS;
}

int
rl_quit(int count, int key)
{
	UNUSED(count); UNUSED(key);
	puts("\n");

	/* Reset terminal attributes before exiting. */
	rl_deprep_terminal();
	exit(FUNC_SUCCESS);
}

#ifndef _NO_PROFILES
/* Get current profile and total number of profiles and store this info
 * in pointers CUR and TOTAL. */
static void
get_cur_prof(int *cur, int *total)
{
	int i;
	for (i = 0; profile_names[i]; i++) {
		(*total)++;

		if (!alt_profile) {
			if (*profile_names[i] == 'd'
			&& strcmp(profile_names[i], "default") == 0)
				*cur = i;
		} else {
			if (*alt_profile == *profile_names[i]
			&& strcmp(alt_profile, profile_names[i]) == 0)
				*cur = i;
		}
	}
}

static int
rl_profile_previous(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1)
		return FUNC_SUCCESS;

#ifndef _NO_SUGGESTIONS
	if (suggestion.printed && suggestion_buf)
		free_suggestion();
#endif /* !_NO_SUGGESTIONS */

	int prev_prof, cur_prof = -1, total_profs = 0;
	get_cur_prof(&cur_prof, &total_profs);

	if (cur_prof == -1 || !profile_names[cur_prof]
	|| total_profs <= 1)
		return FUNC_FAILURE;

	prev_prof = cur_prof - 1;
	total_profs--;

	if (prev_prof < 0 || !profile_names[prev_prof])
		prev_prof = total_profs;

	if (conf.clear_screen) {
		CLEAR;
	} else {
		putchar('\n');
	}

	profile_set(profile_names[prev_prof]);
	rl_update_prompt_old();

	return FUNC_SUCCESS;
}

static int
rl_profile_next(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1)
		return FUNC_SUCCESS;

#ifndef _NO_SUGGESTIONS
	if (suggestion.printed && suggestion_buf)
		free_suggestion();
#endif /* !_NO_SUGGESTIONS */

	int next_prof, cur_prof = -1, total_profs = 0;
	get_cur_prof(&cur_prof, &total_profs);

	if (cur_prof == -1 || !profile_names[cur_prof]
	|| total_profs <= 1)
		return FUNC_FAILURE;

	next_prof = cur_prof + 1;
	total_profs--;

	if (next_prof > total_profs || !profile_names[next_prof]
	|| total_profs <= 1)
		next_prof = 0;

	if (conf.clear_screen) {
		CLEAR;
	} else {
		putchar('\n');
	}

	profile_set(profile_names[next_prof]);
	rl_update_prompt_old();

	return FUNC_SUCCESS;
}
#endif /* _NO_PROFILES */

static int
rl_dirhist(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_kb_cmd("dh", 0);
}

static int
rl_new_instance(int count, int key)
{
	UNUSED(count); UNUSED(key);
	exec_prompt_cmds = 1;
	return run_kb_cmd("x .", 0);
}

static int
rl_clear_msgs(int count, int key)
{
	UNUSED(count); UNUSED(key);
	exec_prompt_cmds = 1;
	return run_kb_cmd("msg clear", 0);
}

static int
rl_trash_sel(int count, int key)
{
	UNUSED(count); UNUSED(key);

	if (sel_n == 0)
		return handle_no_sel("trash");

	exec_prompt_cmds = 1;
	return run_kb_cmd("t sel", 1);
}

static int
rl_untrash_all(int count, int key)
{
	UNUSED(count); UNUSED(key);
	exec_prompt_cmds = 1;
	return run_kb_cmd("u *", 0);
}

static int
rl_open_sel(int count, int key)
{
	UNUSED(count); UNUSED(key);

	if (sel_n == 0)
		return handle_no_sel("open");

	char cmd[PATH_MAX + 4];
	snprintf(cmd, sizeof(cmd), "o %s", sel_elements[sel_n - 1].name);

	return run_kb_cmd(cmd, 0);
}

static int
run_man_cmd(char *str)
{
	char *mp = (char *)NULL;
	const char *p = getenv("MANPAGER");
	if (p) {
		const size_t len = strlen(p);
		mp = xnmalloc(len + 1, sizeof(char *));
		xstrsncpy(mp, p, len + 1);
		unsetenv("MANPAGER");
	}

	const int ret = launch_execl(str) != FUNC_SUCCESS;

	if (mp) {
		setenv("MANPAGER", mp, 1);
		free(mp);
	}

	if (ret != 0) /* Restore prompt in case of failure. */
		rl_reset_line_state();

	return ret;
}

static int
rl_kbinds_help(int count, int key)
{
	if (kbind_busy == 1)
		return FUNC_SUCCESS;

	UNUSED(count); UNUSED(key);
#ifndef _NO_SUGGESTIONS
	if (suggestion.printed && suggestion_buf)
		free_suggestion();
#endif /* !_NO_SUGGESTIONS */

	char cmd[PATH_MAX];
	snprintf(cmd, sizeof(cmd),
		"export PAGER=\"less -p ^[0-9]+\\.[[:space:]]KEYBOARD[[:space:]]SHORTCUTS\"; man %s\n",
		PROGRAM_NAME);
	const int ret = run_man_cmd(cmd);
	if (!ret)
		return FUNC_FAILURE;
	return FUNC_SUCCESS;
}

static int
rl_cmds_help(int count, int key)
{
	if (kbind_busy == 1)
		return FUNC_SUCCESS;

	UNUSED(count); UNUSED(key);
#ifndef _NO_SUGGESTIONS
	if (suggestion.printed && suggestion_buf)
		free_suggestion();
#endif /* !_NO_SUGGESTIONS */

	char cmd[PATH_MAX];
	snprintf(cmd, sizeof(cmd),
		"export PAGER=\"less -p ^[0-9]+\\.[[:space:]]COMMANDS\"; man %s\n",
		PROGRAM_NAME);
	const int ret = run_man_cmd(cmd);
	if (!ret)
		return FUNC_FAILURE;
	return FUNC_SUCCESS;
}

static int
rl_manpage(int count, int key)
{
	if (kbind_busy == 1)
		return FUNC_SUCCESS;

	UNUSED(count); UNUSED(key);
#ifndef _NO_SUGGESTIONS
	if (suggestion.printed && suggestion_buf)
		free_suggestion();
#endif /* !_NO_SUGGESTIONS */
	char *cmd[] = {"man", PROGRAM_NAME, NULL};
	if (launch_execv(cmd, FOREGROUND, E_NOFLAG) != FUNC_SUCCESS) {
		rl_reset_line_state();
		return FUNC_FAILURE;
	}
	return FUNC_SUCCESS;
}

static int
rl_dir_pinned(int count, int key)
{
	UNUSED(count); UNUSED(key);

	if (pinned_dir == NULL) {
		printf(_("\n%s: No pinned file\n"), PROGRAM_NAME);
		rl_reset_line_state();
		return FUNC_SUCCESS;
	}

	return run_kb_cmd(",", 0);
}

/* Switch to workspace N. */
static int
rl_switch_workspace(const int n)
{
	if (kbind_busy == 1)
		return FUNC_SUCCESS;

	if (rl_line_buffer && *rl_line_buffer)
		rl_delete_text(0, rl_end);

	char t[MAX_INT_STR + 4];
	exec_prompt_cmds = 1;

	if (cur_ws == (n - 1)) {
		/* If the user attempts to switch to the same workspace they're
		 * currently in, switch rather to the previous workspace. */
		if (prev_ws != cur_ws) {
			snprintf(t, sizeof(t), "ws %d", prev_ws + 1);
			return run_kb_cmd(t, 0);
		}

		return FUNC_SUCCESS;
	}

	snprintf(t, sizeof(t), "ws %d", n);
	return run_kb_cmd(t, 0);
}

static int
rl_ws1(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return rl_switch_workspace(1);
}

static int
rl_ws2(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return rl_switch_workspace(2);
}

static int
rl_ws3(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return rl_switch_workspace(3);
}

static int
rl_ws4(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return rl_switch_workspace(4);
}

static int
run_plugin(const int num)
{
	if (kbind_busy == 1)
		return FUNC_SUCCESS;

	if (rl_line_buffer && *rl_line_buffer)
		setenv("CLIFM_LINE", rl_line_buffer, 1);

	char cmd[MAX_INT_STR + 7];
	snprintf(cmd, sizeof(cmd), "plugin%d", num);
	const int ret = run_kb_cmd(cmd, 0);

	unsetenv("CLIFM_LINE");

	return ret;
}

static int
rl_plugin1(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_plugin(1);
}

static int
rl_plugin2(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_plugin(2);
}

static int
rl_plugin3(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_plugin(3);
}

static int
rl_plugin4(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_plugin(4);
}

static int
rl_plugin5(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_plugin(5);
}

static int
rl_plugin6(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_plugin(6);
}

static int
rl_plugin7(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_plugin(7);
}

static int
rl_plugin8(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_plugin(8);
}

static int
rl_plugin9(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_plugin(9);
}

static int
rl_plugin10(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_plugin(10);
}

static int
rl_plugin11(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_plugin(11);
}

static int
rl_plugin12(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_plugin(12);
}

static int
rl_plugin13(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_plugin(13);
}

static int
rl_plugin14(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_plugin(14);
}

static int
rl_plugin15(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_plugin(15);
}

static int
rl_plugin16(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return run_plugin(16);
}

static int
rl_launch_view(int count, int key)
{
	UNUSED(count); UNUSED(key);
	exec_prompt_cmds = 1;
	return run_kb_cmd("view", 0);
}

static int
rl_toggle_only_dirs(int count, int key)
{
	UNUSED(count); UNUSED(key);

	if (kbind_busy == 1)
		return FUNC_SUCCESS;

	conf.only_dirs = !conf.only_dirs;
	update_autocmd_opts(AC_ONLY_DIRS);

	const int exit_status = exit_code;
	if (conf.autols == 1) {
		if (conf.clear_screen == 0)
			putchar('\n');
		reload_dirlist();
	}

	print_reload_msg(NULL, NULL, _("Only directories: %s\n"),
		conf.only_dirs > 0 ? _("on") : _("off"));
	xrl_reset_line_state();
	return exit_status;
}

#ifndef _NO_HIGHLIGHT
static void
print_highlight_string(char *s, const int insert_point)
{
	if (!s || !*s)
		return;

	size_t i, l = 0;

	rl_delete_text(insert_point, rl_end);
	rl_point = rl_end = insert_point;
	fputs(tx_c, stdout);
	cur_color = tx_c;

	char q[PATH_MAX + 1];
	for (i = 0; s[i]; i++) {
		rl_highlight(s, i, SET_COLOR);

		if ((signed char)s[i] < 0) {
			q[l] = s[i];
			l++;

			if ((signed char)s[i + 1] >= 0) {
				q[l] = '\0';
				l = 0;
				rl_insert_text(q);
				rl_redisplay();
			}

			continue;
		}

		q[0] = s[i];
		q[1] = '\0';
		rl_insert_text(q);
		rl_redisplay();
	}
}
#endif /* !_NO_HIGHLIGHT */

static int
print_cmdhist_line(int n, int beg_line)
{
#ifndef _NO_SUGGESTIONS
	if (wrong_cmd == 1)
		recover_from_wrong_cmd();
#endif /* !_NO_SUGGESTIONS */

	curhistindex = (size_t)n;

	HIDE_CURSOR;
	int rl_point_bk = rl_point;

#ifndef _NO_HIGHLIGHT
	if (conf.highlight == 1)
		print_highlight_string(history[n].cmd, 0);
	else
#endif /* !_NO_HIGHLIGHT */
	{
		rl_replace_line(history[n].cmd, 1);
	}

	UNHIDE_CURSOR;
	rl_point = (beg_line == 1) ? rl_end : rl_point_bk;
	cur_color = df_c;
	fputs(df_c, stdout);

	return FUNC_SUCCESS;
}

static inline int
handle_cmdhist_beginning(int key)
{
	int p = (int)curhistindex;
	cmdhist_flag = 1;

	if (key == 65) { /* Up arrow key */
		if (--p < 0)
			return FUNC_FAILURE;
	} else { /* Down arrow key */
		if (rl_end == 0)
			return FUNC_SUCCESS;
		if (++p >= (int)current_hist_n) {
			rl_replace_line("", 1);
			curhistindex++;
			return FUNC_SUCCESS;
		}
	}

	if (!history[p].cmd)
		return FUNC_FAILURE;

	curhistindex = (size_t)p;

	return print_cmdhist_line(p, 1);
}

static inline int
handle_cmdhist_middle(int key)
{
	int found = 0, p = (int)curhistindex;

	if (key == 65) { /* Up arrow key */
		if (--p < 0) return FUNC_FAILURE;

		while (p >= 0 && history[p].cmd) {
			if (strncmp(rl_line_buffer, history[p].cmd, (size_t)rl_point) == 0
			&& strcmp(rl_line_buffer, history[p].cmd) != 0) {
				found = 1; break;
			}
			p--;
		}
	} else { /* Down arrow key */
		if (++p >= (int)current_hist_n)	return FUNC_FAILURE;

		while (history[p].cmd) {
			if (strncmp(rl_line_buffer, history[p].cmd, (size_t)rl_point) == 0
			&& strcmp(rl_line_buffer, history[p].cmd) != 0) {
				found = 1; break;
			}
			p++;
		}
	}

	if (found == 0) {
		rl_ring_bell();
		return FUNC_FAILURE;
	}

	return print_cmdhist_line(p, 0);
}

/* Handle keybinds for the cmds history: UP/C-p and DOWN/C-n */
static int
rl_cmdhist(int count, int key)
{
	UNUSED(count);
	if (rl_nohist == 1)
		return FUNC_SUCCESS;

#ifndef _NO_SUGGESTIONS
	if (suggestion_buf) {
		free(suggestion_buf);
		suggestion_buf = (char *)NULL;
	}
#endif /* !_NO_SUGGESTIONS */

	if (key == 16) /* C-p  */
		key = 65;  /* Up   */
	if (key == 14) /* C-n  */
		key = 66;  /* Down */

	if (key != 65 && key != 66)
		return FUNC_FAILURE;

	/* If the cursor is at the beginning of the line */
	if (rl_point == 0 || cmdhist_flag == 1)
		return handle_cmdhist_beginning(key);

	return handle_cmdhist_middle(key);
}

static int
rl_toggle_disk_usage(int count, int key)
{
	UNUSED(count); UNUSED(key);

	if (kbind_busy == 1)
		return FUNC_SUCCESS;

	/* Default values */
	static int dsort = DEF_SORT, dlong = DEF_LONG_VIEW,
		ddirsize = DEF_FULL_DIR_SIZE, ddf = DEF_LIST_DIRS_FIRST;

	if (xargs.disk_usage_analyzer == 1) {
		xargs.disk_usage_analyzer = 0;
		conf.sort = dsort;
		conf.long_view = dlong;
		conf.full_dir_size = ddirsize;
		conf.list_dirs_first = ddf;
	} else {
		xargs.disk_usage_analyzer = 1;
		dsort = conf.sort;
		dlong = conf.long_view;
		ddirsize = conf.full_dir_size;
		ddf = conf.list_dirs_first;

		conf.sort = STSIZE;
		conf.long_view = conf.full_dir_size = 1;
		conf.list_dirs_first = 0;
	}

	const int exit_status = exit_code;
	if (conf.autols == 1) {
		if (conf.clear_screen == 0)
			putchar('\n');
		reload_dirlist();
	}

	print_reload_msg(NULL, NULL, _("Disk usage analyzer: %s\n"),
		xargs.disk_usage_analyzer == 1 ? _("on") : _("off"));
	xrl_reset_line_state();
	return exit_status;
}

static int
rl_tab_comp(int count, int key)
{
#ifndef _NO_SUGGESTIONS
	if (suggestion.printed && suggestion_buf)
		clear_suggestion(CS_FREEBUF);
#endif /* !_NO_SUGGESTIONS */
	UNUSED(count); UNUSED(key);

	tab_complete('!');
	return FUNC_SUCCESS;
}

static int
rl_del_last_word(int count, int key)
{
#ifndef _NO_SUGGESTIONS
	if (suggestion.printed && suggestion_buf)
		clear_suggestion(CS_FREEBUF);
#endif /* !_NO_SUGGESTIONS */
	UNUSED(count); UNUSED(key);

	if (rl_point == 0)
		return FUNC_SUCCESS;

	char *end_buf = (char *)NULL;
	if (rl_point < rl_end) { /* Somewhere before the end of the line */
		end_buf = rl_copy_text(rl_point, rl_end);
		rl_delete_text(rl_point, rl_end);
	}

	char *b = rl_line_buffer;

	if (b[rl_point - 1] == '/' || b[rl_point - 1] == ' ') {
		--rl_point;
		b[rl_point] = '\0';
		--rl_end;
	}

	int n = 0;
	char *p = xstrrpbrk(b, WORD_DELIMITERS);
	if (p)
		n = (int)(p - b) + (*(p + 1) ? 1 : 0);

	rl_begin_undo_group();
	rl_delete_text(n, rl_end);
	rl_end_undo_group();
	rl_point = rl_end = n;
	if (end_buf) {
		rl_insert_text(end_buf);
		rl_point = n;
		free(end_buf);
	}
	rl_redisplay();

#ifndef _NO_SUGGESTIONS
	if (conf.suggestions == 1 && n == 0 && wrong_cmd)
		recover_from_wrong_cmd();
#endif /* !_NO_SUGGESTIONS */

	return FUNC_SUCCESS;
}

static int
rl_toggle_virtualdir_full_paths(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (!stdin_tmp_dir || strcmp(stdin_tmp_dir, workspaces[cur_ws].path) != 0)
		return FUNC_SUCCESS;

	xchmod(stdin_tmp_dir, "0700", 1);
	xargs.virtual_dir_full_paths = !xargs.virtual_dir_full_paths;

	filesn_t i = files;
	while (--i >= 0) {
		char *rp = xrealpath(file_info[i].name, NULL);
		if (!rp) continue;

		char *p = (char *)NULL;
		if (xargs.virtual_dir_full_paths != 1) {
			if ((p = strrchr(rp, '/')) && p[1])
				++p;
		} else {
			p = replace_slashes(rp, ':');
		}

		if (!p || !*p) continue;

		if (renameat(XAT_FDCWD, file_info[i].name, XAT_FDCWD, p) == -1)
			err('w', PRINT_PROMPT, "renameat: %s: %s\n",
				file_info[i].name, strerror(errno));

		if (xargs.virtual_dir_full_paths == 1) free(p);
		free(rp);
	}

	xchmod(stdin_tmp_dir, "0500", 1);

	if (conf.clear_screen == 0)
		putchar('\n');

	reload_dirlist();
	print_reload_msg(NULL, NULL, _("Switched to %s names\n"),
		xargs.virtual_dir_full_paths == 1 ? _("long") : _("short"));
	xrl_reset_line_state();
	return FUNC_SUCCESS;
}

static int
rl_run_pager(int count, int key)
{
	UNUSED(count); UNUSED(key);
	if (kbind_busy == 1 || conf.pager == 1)
		return FUNC_SUCCESS;

	return run_kb_cmd("pg", 0);
}

/* Run "!<TAB>" to display the command history via finder. */
static int
rl_cmdhist_tab(int count, int key)
{
	UNUSED(count); UNUSED(key);
	rl_insert_text("!");
	rl_point = rl_end;

#ifndef _NO_SUGGESTIONS
	if (suggestion_buf)
		clear_suggestion(CS_FREEBUF);
#endif /* _NO_SUGGESTIONS */

	tab_complete('!');

	if (rl_end > 0 && rl_line_buffer[rl_end - 1] == '!') {
		rl_end--;
		rl_point--;
	}

	return FUNC_SUCCESS;
}

static int
rl_toggle_vi_mode(int count, int key)
{
	UNUSED(count); UNUSED(key);

	Keymap keymap = rl_get_keymap();
	if (keymap == vi_insertion_keymap) {
		keymap = rl_get_keymap_by_name("emacs-standard");
		rl_set_keymap(keymap);
		rl_editing_mode = RL_EMACS_MODE;
	} else if (keymap == emacs_standard_keymap) {
		keymap = rl_get_keymap_by_name("vi-insert");
		rl_set_keymap(keymap);
		rl_editing_mode = RL_VI_MODE;
	} else {
		return FUNC_SUCCESS;
	}

	const size_t n = rl_prompt ? count_chars(rl_prompt, '\n') : 0;
	if (n > 0 && n <= INT_MAX)
		MOVE_CURSOR_UP((int)n);
	putchar('\r');
	ERASE_TO_RIGHT_AND_BELOW;
	fflush(stdout);
	xrl_reset_line_state();
	rl_update_prompt();

	return FUNC_SUCCESS;
}

/* Used to disable keybindings. */
static int
do_nothing(int count, int key)
{
	UNUSED(count); UNUSED(key);
	return FUNC_SUCCESS;
}

/* Hold kebinding names and associated functions. */
struct keyfuncs_t {
	const char *name;
	int (*func)(int, int);
};

/* Return a pointer to the function associated to the keybinding name NAME. */
static int (*get_function(struct keyfuncs_t *s, const char *name))(int, int)
{
	while (s->name) {
		if (*name == *s->name && strncmp(name, s->name, strlen(s->name)) == 0)
			return s->func;
		s++;
	}

	return NULL;
}

static void
set_keybinds_from_file(void)
{
	struct keyfuncs_t keys[] = {
		{"show-manpage", rl_manpage}, {"show-cmds", rl_cmds_help},
		{"show-kbinds", rl_kbinds_help}, {"parent-dir", rl_dir_parent},
		{"previous-dir", rl_dir_previous}, {"next-dir", rl_dir_next},
		{"home-dir", rl_dir_home}, {"root-dir", rl_dir_root},
		{"workspace1", rl_ws1}, {"workspace2", rl_ws2},
		{"workspace3", rl_ws3}, {"workspace4", rl_ws4},

		{"create-file", rl_create_file}, {"archive-sel", rl_archive_sel},
		{"open-sel", rl_open_sel}, {"export-sel", rl_export_sel},
		{"move-sel", rl_move_sel}, {"rename-sel", rl_rename_sel},
		{"remove-sel", rl_remove_sel}, {"trash-sel", rl_trash_sel},
		{"untrash-all", rl_untrash_all}, {"paste-sel", rl_paste_sel},
		{"copy-sel", rl_paste_sel}, {"select-all", rl_select_all},
		{"deselect-all", rl_deselect_all},

		{"open-mime", rl_open_mime}, {"open-jump-db", rl_open_jump_db},
		{"open-preview", rl_open_preview}, {"open-config", rl_open_config},
		{"edit-color-scheme", rl_open_cscheme},
		{"open-keybinds", rl_open_keybinds}, {"open-bookmarks", rl_open_bm_file},

		{"toggle-virtualdir-full-paths", rl_toggle_virtualdir_full_paths},
		{"clear-msgs", rl_clear_msgs},
#ifndef _NO_PROFILES
		{"next-profile", rl_profile_next},
		{"previous-profile", rl_profile_previous},
#endif // _NO_PROFILES
		{"quit", rl_quit}, {"lock", rl_lock}, {"refresh-screen", rl_refresh},
		{"clear-line", rl_clear_line}, {"toggle-hidden", rl_toggle_hidden_files},
		{"toggle-long", rl_toggle_long_view},
		{"toggle-follow-links-long", rl_toggle_follow_link_long},
		{"toggle-light", rl_toggle_light_mode},
		{"dirs-first", rl_toggle_dirs_first},
		{"sort-previous", rl_sort_previous}, {"sort-next", rl_sort_next},
		{"only-dirs", rl_toggle_only_dirs},
		{"run-pager", rl_run_pager},

		{"launch-view", rl_launch_view}, {"new-instance", rl_new_instance},
		{"show-dirhist", rl_dirhist}, {"bookmarks", rl_bookmarks},
		{"mountpoints", rl_mountpoints}, {"selbox", rl_selbox},
		{"prepend-sudo", rl_prepend_sudo},
		{"toggle-disk-usage", rl_toggle_disk_usage},
		{"toggle-max-name-len", rl_toggle_max_filename_len},
		{"cmd-hist", rl_cmdhist_tab},

		{"plugin1", rl_plugin1}, {"plugin2", rl_plugin2},
		{"plugin3", rl_plugin3}, {"plugin4", rl_plugin4},
		{"plugin5", rl_plugin5}, {"plugin6", rl_plugin6},
		{"plugin7", rl_plugin7}, {"plugin8", rl_plugin8},
		{"plugin9", rl_plugin9}, {"plugin10", rl_plugin10},
		{"plugin11", rl_plugin11}, {"plugin12", rl_plugin12},
		{"plugin13", rl_plugin13}, {"plugin14", rl_plugin14},
		{"plugin15", rl_plugin15}, {"plugin16", rl_plugin16},
		{NULL, NULL}
	};

	for (size_t i = 0; i < kbinds_n; i++)
		rl_bind_keyseq(kbinds[i].key, get_function(keys, kbinds[i].function));

	const char *vi_mode_keyseq = find_key("toggle-vi-mode");
	if (vi_mode_keyseq) {
		rl_bind_keyseq(vi_mode_keyseq, rl_toggle_vi_mode);
		Keymap keymap = rl_get_keymap_by_name("vi-insert");
		if (keymap)
			rl_bind_keyseq_in_map(vi_mode_keyseq, rl_toggle_vi_mode, keymap);
	}
}

static void
set_default_keybinds(void)
{
	struct keyfuncs_t keys[] = {
		/* Help */
		{"\\eOP", rl_manpage}, {"\\e[11~", rl_manpage},
		{"\\eOQ", rl_cmds_help}, {"\\e[12~", rl_cmds_help},
		{"\\eOR", rl_kbinds_help}, {"\\e[13~", rl_kbinds_help},
		/* Navigation */
		{"\\M-u", rl_dir_parent}, {"\\e[a", rl_dir_parent},
		{"\\e[2A", rl_dir_parent}, {"\\e[1;2A", rl_dir_parent},
		{"\\M-j", rl_dir_previous}, {"\\e[d", rl_dir_previous},
		{"\\e[2D", rl_dir_previous}, {"\\e[1;2D", rl_dir_previous},
		{"\\M-k", rl_dir_next}, {"\\e[c", rl_dir_next},
		{"\\e[2C", rl_dir_next}, {"\\e[1;2C", rl_dir_next},
		{"\\M-e", rl_dir_home}, {"\\e[1~", rl_dir_home},
		{"\\e[7~", rl_dir_home}, {"\\e[H", rl_dir_home},
		{"\\M-r", rl_dir_root}, {"\\e/", rl_dir_root},
		{"\\M-p", rl_dir_pinned},
		{"\\M-1", rl_ws1}, {"\\M-2", rl_ws2},
		{"\\M-3", rl_ws3}, {"\\M-4", rl_ws4},
		/* Operations on files */
		{"\\M-n", rl_create_file}, {"\\C-\\M-a", rl_archive_sel},
		{"\\C-\\M-e", rl_export_sel}, {"\\C-\\M-r", rl_rename_sel},
		{"\\C-\\M-d", rl_remove_sel}, {"\\C-\\M-t", rl_trash_sel},
		{"\\C-\\M-v", rl_paste_sel}, {"\\M-a", rl_select_all},
		{"\\M-d", rl_deselect_all}, {"\\M-v", rl_prepend_sudo},
		/* Config files */
		{"\\e[17~", rl_open_mime}, {"\\e[18~", rl_open_preview},
		{"\\e[19~", rl_open_cscheme}, {"\\e[20~", rl_open_keybinds},
		{"\\e[21~", rl_open_config}, {"\\e[23~", rl_open_bm_file},
		/* Settings */
		{"\\M-w", rl_toggle_virtualdir_full_paths},
		{"\\M-t", rl_clear_msgs}, {"\\M-o", rl_lock},
		{"\\C-r", rl_refresh}, {"\\M-c", rl_clear_line},
		{"\\M-i", rl_toggle_hidden_files}, {"\\M-.", rl_toggle_hidden_files},
		{"\\M-l", rl_toggle_long_view}, {"\\M-+", rl_toggle_follow_link_long},
		{"\\M-y", rl_toggle_light_mode}, {"\\M-g", rl_toggle_dirs_first},
		{"\\M-z", rl_sort_previous}, {"\\M-x", rl_sort_next},
		{"\\M-,", rl_toggle_only_dirs}, {"\\M-0", rl_run_pager},
		/* Misc */
		{"\\M--", rl_launch_view}, {"\\C-\\M-x", rl_new_instance},
		{"\\M-h", rl_dirhist}, {"\\M-b", rl_bookmarks},
		{"\\M-m", rl_mountpoints}, {"\\M-s", rl_selbox},
		{"\\C-\\M-l", rl_toggle_max_filename_len},
		{"\\C-\\M-i", rl_toggle_disk_usage}, {"\\e[24~", rl_quit},
/*		{"\\C-\\M-g", rl_open_sel}, // Disabled by default
		{"\\C-\\M-n", rl_move_sel}, // Disabled by default
		{"\\C-\\M-u", rl_untrash_all}, // Disabled by default
		{"\\e[18~", rl_open_jump_db}, // Disabled by default */
		{NULL, NULL}
	};

	for (size_t i = 0; keys[i].name; i++)
		rl_bind_keyseq(keys[i].name, keys[i].func);

	rl_bind_keyseq("\\C-\\M-j", rl_toggle_vi_mode);
	Keymap keymap = rl_get_keymap_by_name("vi-insert");
	if (keymap)
		rl_bind_keyseq_in_map("\\C-\\M-j", rl_toggle_vi_mode, keymap);
}

static void
set_hardcoded_keybinds(void)
{
	struct keyfuncs_t keys[] = {
		{"\\M-*", do_nothing}, {"\x1b[42;3u", do_nothing},
#ifndef __HAIKU__
		{"\\C-l", rl_refresh}, {"\x1b[108;5u", rl_refresh},
		{"\\C-p", rl_cmdhist}, {"\\C-n", rl_cmdhist},
#endif /* !__HAIKU__ */
		{"\x1b[A", rl_cmdhist}, {"\x1b[B", rl_cmdhist},
		{"\\M-q", rl_del_last_word}, {"\x1b[113;3u", rl_del_last_word},
#ifndef _NO_SUGGESTIONS
# ifndef __HAIKU__
		{"\x1b[102;5u", rl_accept_suggestion}, {"\\C-f", rl_accept_suggestion},
		{"\x1b[C", rl_accept_suggestion}, {"\x1bOC", rl_accept_suggestion},
		{"\x1b[102;3u", rl_accept_first_word}, {"\\M-f", rl_accept_first_word},
		{"\x1b[3C", rl_accept_first_word}, {"\x1b\x1b[C", rl_accept_first_word},
		{"\x1b[1;3C", rl_accept_first_word},
# else
		{"\x1bOC", rl_accept_suggestion}, {"\\C-f", rl_accept_first_word},
# endif /* !__HAIKU__ */
#endif /* !_NO_SUGGESTIONS */
		{NULL, NULL}
	};

	for (size_t i = 0; keys[i].name; i++)
		rl_bind_keyseq(keys[i].name, keys[i].func);

	rl_bind_key('\t', rl_tab_comp);
}

/* Disable readline keybindings conflicting with clifm's.
 * This function is called before reading the readline config file (by
 * default ~/.inputrc), so that the user can rebind them using any of config
 * files (either readline.clifm or keybindings.clifm). */
void
disable_rl_conflicting_kbinds(void)
{
	const char *const keys[] = {"\\x1b\\xd", "\\C-x(", "\\C-x\\C-u",
		"\\C-x\\C-x", "\\C-x\\C-g", "\\C-x\\C-?","\\C-x\\C-r", "\\C-xe",
		"\\C-x", "\\C-q", "\\C-d", "\\C-]", "\\e\\C-]", "\\e\\C-i",
		"\\e\\", "\\e\\e", "\\eb", "\\e.", "\\et", "\\ey", "\\e-",
		"\\eu", "\\M-5", "\\M-6", "\\M-7", "\\M-8", "\\M-9", NULL};

	size_t i;
	for (i = 0; keys[i]; i++)
		rl_bind_keyseq(keys[i], do_nothing);
}

void
readline_kbinds(void)
{
	if (kbinds_file)
		set_keybinds_from_file();
	else
		set_default_keybinds();

	set_hardcoded_keybinds();
}
