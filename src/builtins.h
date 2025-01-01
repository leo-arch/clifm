/* builtins.h - Built-in commands for several shells */

/*
 * This file is part of Clifm
 *
 * Copyright (C) 2016-2025, L. Abramovich <leo.clifm@outlook.com>
 * All rights reserved.

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

/* A list of different shells builtins to be recognized as valid command
 * names. Only useful for the warning prompt */

#ifndef BUILTINS_H
#define BUILTINS_H

/* Bash */
char *bash_builtins[] = {
	"{",
	"case",
	"continue",
	"do",
	"done",
	"elif",
	"else",
	"esac",
	"fi",
	"for",
	"if",
	"in",
	"return",
	"then",
	"until",
	"while",

	"[",
	"[[",
	"bg",
	"bind",
	"break",
	"builtin",
	"caller",
	"command",
	"compgen",
	"compopt",
	"complete",
	"contrinue",
	"coproc",
	"declare",
	"dirs",
	"disown",
	"echo",
	"enable",
	"eval",
	"exec",
	"false",
	"fg",
	"function",
	"getopts",
	"hash",
	"help",
	"jobs",
	"kill",
	"let",
	"local",
	"logout",
	"mapfile",
	"popd",
	"printf",
	"pushd",
	"read",
	"readarray",
	"readonly",
	"select",
	"set",
	"shift",
	"shopt",
	"source",
	"suspend",
	"test",
	"time",
	"trap",
	"true",
	"type",
	"typeset",
	"ulimit",
	"unalias",
	"unset",
	"variables",
	"wait",
	NULL
};

/* Dash */
char *dash_builtins[] = {
	"case",
	"continue",
	"do",
	"done",
	"elif",
	"else",
	"esac",
	"fi",
	"for",
	"if",
	"in",
	"return",
	"then",
	"until",
	"while",

	"bg",
	"command",
	"echo",
	"eval",
	"exec",
	"fg",
	"getopts",
	"hash",
	"read",
	"readonly",
	"printf",
	"set",
	"shift",
	"test",
	"times",
	"trap",
	"type",
	"ulimit",
	"unalias",
	"unset",
	"wait",
	NULL
};

/* Fish */
char *fish_builtins[] = {
	"{",
	"case",
	"continue",
	"do",
	"done",
	"elif",
	"else",
	"esac",
	"fi",
	"for",
	"if",
	"in",
	"return",
	"then",
	"until",
	"while",

	"_",
	"abbr",
	"and",
	"argparse",
	"begin",
	"bg",
	"bind",
	"block",
	"break",
	"breakpoint",
	"builtin",
	"case",
	"cdh",
	"command",
	"commandline",
	"complete",
	"contains",
	"count",
	"dirh",
	"dirs",
	"disown",
	"echo",
	"emit",
	"end",
	"eval",
	"exec",
	"false",
	"fg",
	"fish_add_path",
	"fish_breakpoint_prompt",
	"fish_command_not_found",
	"fish_config",
	"fish_git_prompt",
	"fish_greeting",
	"fish_hg_prompt",
	"fish_indent",
	"fish_is_root_user",
	"fish_key_reader",
	"fish_mode_prompt",
	"fish_opt",
	"fish_prompt",
	"fish_right_prompt",
	"fish_status_to_signal",
	"fish_svn_prompt",
	"fish_title",
	"fish_update_completions",
	"fish_vcs_prompt",
	"funced",
	"funcsave",
	"function",
	"functions",
	"isatty",
	"jobs",
	"math",
	"nextd",
	"not",
	"open",
	"or",
	"popd",
	"prevd",
	"printf",
	"prompt_login",
	"prompt_pwd",
	"psub",
	"pushd",
	"random",
	"read",
	"realpath",
	"set",
	"set_color",
	"source",
	"status",
	"string",
	"string-collect",
	"string-escape",
	"string-join",
	"string-join0",
	"string-length",
	"string-lower",
	"string-match",
	"string-pad",
	"string-repeat",
	"string-replace",
	"string-split",
	"string-split0",
	"string-sub",
	"string-trim",
	"string-unescape",
	"string-upper",
	"suspend",
	"switch",
	"test",
	"time",
	"trap",
	"true",
	"type",
	"ulimit",
	"vared",
	"wait",
	NULL
};

/* KSH */
char *ksh_builtins[] = {
	"{",
	"case",
	"continue",
	"do",
	"done",
	"elif",
	"else",
	"esac",
	"fi",
	"for",
	"if",
	"in",
	"return",
	"then",
	"until",
	"while",

	"bg",
	"break",
	"builtin",
	"command",
	"disown",
	"echo",
	"enum",
	"eval",
	"exec",
	"false",
	"fg",
	"getopts",
	"hist",
	"jobs",
	"kill",
	"let",
	"newgrp",
	"print",
	"printf",
	"read",
	"readonly",
	"set",
	"shift",
	"sleep",
	"times",
	"trap",
	"true",
	"typeset",
	"ulimit",
	"unalias",
	"unset",
	"wait",
	"whence",
	NULL
};

/* TCSH */
char *tcsh_builtins[] = {
	"{",
	"case",
	"continue",
	"do",
	"done",
	"elif",
	"else",
	"esac",
	"fi",
	"for",
	"if",
	"in",
	"return",
	"then",
	"until",
	"while",

	"alloc",
	"bg",
	"bindkey",
	"bs2cmd",
	"breaksw",
	"builtins",
	"chdir",
	"complete",
	"dirs",
	"echo",
	"echotc",
	"endsw",
	"exec",
	"fg",
	"filetest",
	"foreach",
	"end",
	"getspath",
	"getxvers",
	"glob",
	"goto",
	"hashstat",
	"hup",
	"inlib",
	"jobs",
	"kill",
	"limit",
	"log",
	"ls-F",
	"migrate",
	"newgrp",
	"nice",
	"nohup",
	"notify",
	"onintr",
	"popd",
	"printenv",
	"pushd",
	"rehash",
	"repeat",
	"rootnode",
	"sched",
	"set",
	"setenv",
	"setpath",
	"setspath",
	"settc",
	"setty",
	"setxvers",
	"shift",
	"source",
	"stop",
	"suspend",
	"telltc",
	"termname",
	"time",
	"unalias",
	"uncomplete",
	"unhash",
	"universe",
	"unlimit",
	"unset",
	"unsetenv",
	"ver",
	"wait",
	"warp",
	"watchlog",
	"where",
	"which",
	NULL
};

/* ZSH */
char *zsh_builtins[] = {
	"{",
	"case",
	"continue",
	"do",
	"done",
	"elif",
	"else",
	"esac",
	"fi",
	"for",
	"if",
	"in",
	"return",
	"then",
	"until",
	"while",

	"autoload",
	"bg",
	"bindkey",
	"break",
	"builtin",
	"chdir",
	"clone",
	"command",
	"comparguments",
	"compcall",
	"compctl",
	"compdescribe",
	"compfiles",
	"compgroups",
	"compquote",
	"comptags",
	"comptry",
	"compvalues",
	"declare",
	"dirs",
	"disable",
	"disown",
	"echo",
	"echotc",
	"echoti",
	"emulate",
	"enable",
	"eval",
	"exec",
	"false",
	"fg",
	"float",
	"functions",
	"getcap",
	"getln",
	"getopts",
	"hash",
	"integer",
	"jobs",
	"kill",
	"let",
	"limit",
	"local",
	"log",
	"logout",
	"noglob",
	"popd",
	"print",
	"printf",
	"pushd",
	"pushln",
	"read",
	"readonly",
	"rehash",
	"sched",
	"set",
	"setcap",
	"setopt",
	"shift",
	"source",
	"stat",
	"suspend",
	"test",
	"times",
	"trap",
	"true",
	"ttyctl",
	"type",
	"typeset",
	"ulimit",
	"unalias",
	"unfunction",
	"unhash",
	"unlimit",
	"unset",
	"unsetopt",
	"vared",
	"wait",
	"whence",
	"where",
	"which",
	"zcompile",
	"zformat",
	"zftp",
	"zle",
	"zmodload",
	"zparseopts",
	"zprof",
	"zpty",
	"zregexparse",
	"zsocket",
	"zstyle",
	"ztcp",
	NULL
};

#endif /* BUILTINS_H */
