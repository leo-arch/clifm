/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* translate_key.h */

#ifndef TRANSLATE_KEY_H
#define TRANSLATE_KEY_H

/* Terminal types */
#define TK_TERM_GENERIC    0
#define TK_TERM_LEGACY_SCO (1 << 0)
#define TK_TERM_LEGACY_HP  (1 << 1)
#define TK_TERM_KITTY      (1 << 2)
#define TK_TERM_LINUX      (1 << 3)
/* For the time being, these ones are unused */
#define TK_TERM_XTERM      (1 << 4)
#define TK_TERM_RXVT       (1 << 5)
#define TK_TERM_ST         (1 << 6)

#define ALT_CSI        0x9b /* 8-bit CSI (alternate sequence) */
#define CSI_INTRODUCER 0x5b /* [ */
#define SS3_INTRODUCER 0x4f /* O */

#ifdef __cplusplus
extern "C" {
#endif

char *translate_key(char *str, const int term_type);
int is_end_seq_char(unsigned char c);

#ifdef __cplusplus
}
#endif

#endif /* TRANSLATE_KEY_H */
