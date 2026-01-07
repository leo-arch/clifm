/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* prompt.h */

#ifndef PROMPT_H
#define PROMPT_H

/* Mode macros for the prompt() function */
/* First parameter */
#define PROMPT_UPDATE          0
#define PROMPT_SHOW            1
#define PROMPT_UPDATE_RUN_CMDS 2
/* Second parameter */
#define PROMPT_NO_SCREEN_REFRESH 0
#define PROMPT_SCREEN_REFRESH    1

#define CTLESC '\001'
#define CTLNUL '\177'

#ifdef __HAIKU__
/* No need for a root indicator on Haiku: it always runs as root. */
# define ROOT_IND ""
# define ROOT_IND_NO_COLOR ""
#else
# define ROOT_IND "\001\x1b[1;31m\002R\001\x1b[0m\002"
# define ROOT_IND_NO_COLOR "R"
#endif /* __HAIKU__ */
#define ROOT_IND_SIZE sizeof(ROOT_IND)
#define RDONLY_IND "RO\001\x1b[0m\002"
#define RDONLY_IND_SIZE (MAX_COLOR + sizeof(RDONLY_IND))
#define STEALTH_IND "S\001\x1b[0m\002"
#define STEALTH_IND_SIZE (MAX_COLOR + sizeof(STEALTH_IND))

#define EMERGENCY_PROMPT_MSG "Error decoding prompt line. Using an \
emergency prompt."
#define EMERGENCY_PROMPT "\001\x1b[0m\002> "

/* Flag macros to generate files statistic string for the prompt */
#define STATS_DIR      0
#define STATS_REG      1
#define STATS_EXE      2
#define STATS_HIDDEN   3
#define STATS_SUID     4
#define STATS_SGID     5
#define STATS_FIFO     6
#define STATS_SOCK     7
#define STATS_BLK      8
#define STATS_CHR      9
#define STATS_CAP      10
#define STATS_LNK      11
#define STATS_BROKEN_L 12 /* Broken link */
#define STATS_MULTI_L  13 /* Multi-link */
#define STATS_OTHER_W  14 /* Other writable */
#define STATS_STICKY   15
#define STATS_EXTENDED 16 /* Extended attributes (acl) */
#define STATS_UNKNOWN  17
#define STATS_UNSTAT   18
#ifdef SOLARIS_DOORS
# define STATS_DOOR    19
# define STATS_PORT    20
#endif /* SOLARIS_DOORS */
#define STATS_NON_DIR  21

#define NOTIF_SEL     0
#define NOTIF_TRASH   1
#define NOTIF_WARNING 2
#define NOTIF_ERROR   3
#define NOTIF_NOTICE  4
#define NOTIF_ROOT    5
#define NOTIF_AUTOCMD 6

#define RL_EMACS_MODE 1
#define RL_VI_MODE    0
#define RL_VI_INS_MODESTR "(ins)"
#define RL_VI_INS_MODESTR_LEN (sizeof(RL_VI_INS_MODESTR) - 1)
#define RL_VI_CMD_MODESTR "(cmd)"
#define RL_VI_CMD_MODESTR_LEN (sizeof(RL_VI_CMD_MODESTR) - 1)

/* Size of the indicator for msgs, trash, and sel. */
#define N_IND (MAX_COLOR + 1 + 21 + (sizeof(RL_NC) - 1) + 1)
/* Color + 1 letter + plus unsigned integer (20 digits max)
 * + RL_NC length + nul char. */

/* Set this variable to 1 to prevent an extra screen refresh, for example,
 * after running a keybinding. See handle_empty_line(). */
extern int g_prompt_ignore_empty_line;

__BEGIN_DECLS

char *prompt(const int prompt_flag, const int screen_refresh);
char *decode_prompt(char *line);
int  prompt_function(char **args);
char *gen_color(char **line);
void set_prompt_options(void);

__END_DECLS

#endif /* PROMPT_H */
