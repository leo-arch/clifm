/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* safe_names.c -- Validate filenames */

#include "helpers.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "checks.h"     /* is_number */
#include "misc.h"       /* err (xerror) */
#include "safe_names.h" /* UNSAFE macros */
#include "strings.h"    /* unescape_str */

static char *const unsafe_name_msgs[] = {
	"Starts with a dash (-): command option flags collision",
	"Reserved (internal: MIME/file type expansion)",
	"Reserved (internal: ELN/range expansion)",
	"Reserved (internal: fastback expansion)",
	"Reserved (internal: bookmarks, tags, and selected files constructs)",
	"Contains control/non-printable characters",
	"Contains shell metacharacters",
	"Contains a leading tilde",
	"Contains a leading whitespace",
	"Contains a trailing whitespace",
	"Contains illegal UTF-8 bytes",
	"Name is too long",
	"Contains characters not in the Portable Filename Character Set"
};

/* Return 0 if NAME is a portable filename. Otherwise, return 1. */
static int
is_portable_filename(const char *name, const size_t len)
{
	return len > strspn(name, PORTABLE_CHARSET);
}

static int
is_range(const char *str)
{
	if (!str || !*str)
		return 0;

	char *dash = strchr(str, '-');
	if (!dash)
		return 0;

	*dash = '\0';
	if (is_number(str) && (!dash[1] || is_number(dash + 1))) {
		*dash = '-';
		return 1;
	}

	*dash = '-';
	return 0;
}

static void
print_val_err(const char *name, const int msg_type, int *safe, int *printed)
{
	if (printed && *printed == 1)
		return;

	printf("'%s': %s\n", name, unsafe_name_msgs[msg_type]);
	*safe = 0;
	if (printed)
		*printed = 1;
}

static int
is_whitespace(const char c)
{
	return (c == 0x20 /* Space */
		|| c == 0x09  /* Horizontal TAB */
		|| c == 0x0a  /* Line feed */
		|| c == 0x0b  /* Vertical TAB */
		|| c == 0x0c  /* Form feed */
		|| c == 0x0d  /* Carriage return */
		|| (unsigned char)c == 0xa0 /* Non-breaking space (NBSP) */
	);
}

/* Return 1 if NAME is a safe filename, or 0 if not.
 * See https://dwheeler.com/essays/fixing-unix-linux-filenames.html */
static int
is_safe_filename(const char *name)
{
	if (!name || !*name)
		return 0;

	if (conf.safe_filenames == SAFENAMES_NOCHECK) /* This check is disabled. */
		return 1;

	int safe = 1;
	const char *n = name;
	const size_t namelen = strlen(name);

	if (is_whitespace(*name))
		print_val_err(name, UNSAFE_LEADING_WHITESPACE, &safe, NULL);

	if (namelen > 1 && is_whitespace(name[namelen - 1]))
		print_val_err(name, UNSAFE_TRAILING_WHITESPACE, &safe, NULL);

	/* Starting with dash */
	if (*n == '-')
		print_val_err(name, UNSAFE_DASH, &safe, NULL);

	/* Starting with a tilde */
	if (namelen > 1 && *n == '~')
		print_val_err(name, UNSAFE_LEADING_TILDE, &safe, NULL);

	/* Reserved keyword (internal: MIME type and file type expansions) */
	if ((*n == '=' && strchr(FILE_TYPE_CHARS, n[1]) && !n[2]) || *n == '@')
		print_val_err(name, UNSAFE_MIME, &safe, NULL);

	/* Reserved keyword (internal: bookmarks, tags, workspaces, and
	 * selected files constructs) */
	if (((*n == 'b' || *n == 's') && n[1] == ':')
	|| strcmp(n, "sel") == 0)
		print_val_err(name, UNSAFE_BTS_CONST, &safe, NULL);

	if ((*n == 't' || *n == 'w') && n[1] == ':' && n[2])
		print_val_err(name, UNSAFE_BTS_CONST, &safe, NULL);

	/* Reserved (internal: ELN/range expansion) */
	if ((*n > '0' && is_number(n)) || is_range(n))
		print_val_err(name, UNSAFE_ELN, &safe, NULL);

	struct flags_t {
		int control;
		int illegal_utf8;
		int meta;
	};

	struct flags_t printed = (struct flags_t){0};
	int only_dots = 1;
	const char *s = name;

	while (*s) {
		/* Contains control characters (being not UTF-8 bytes) or the
		 * DEL character (0x7f). */
		if ((*s < ' ' && !IS_UTF8_CHAR(*s)) || *s == 0x7f)
			print_val_err(name, UNSAFE_CONTROL, &safe, &printed.control);

		/* Illegal bytes in well‑formed UTF‑8 sequences (RFC 3629) */
		if ((unsigned char)*s == 0xc0 || (unsigned char)*s == 0xc1
		|| (unsigned char)*s >= 0xf5)
			print_val_err(name, UNSAFE_ILLEGAL_UTF8, &safe, &printed.illegal_utf8);

		/* Contains shell meta-characters (only in strict mode) */
		if (conf.safe_filenames == SAFENAMES_STRICT
		&& strchr(SHELL_META_CHARS, *s))
			print_val_err(name, UNSAFE_META, &safe, &printed.meta);

		/* Only dots: Reserved keyword (internal: fastback expansion) */
		if (*s != '.')
			only_dots = 0;

		s++;
	}

	if (only_dots == 1 && namelen > 2)
		print_val_err(name, UNSAFE_FASTBACK, &safe, NULL);

	/* Name too long */
	if (s - name >= NAME_MAX)
		print_val_err(name, UNSAFE_TOO_LONG, &safe, NULL);

	if (conf.safe_filenames == SAFENAMES_POSIX
	&& is_portable_filename(name, (size_t)(s - name)) != FUNC_SUCCESS)
		print_val_err(name, UNSAFE_NOT_PORTABLE, &safe, NULL);

	return safe;
}

/* Return 0 if the file path NAME (or any component in it) does not exist and
 * is an invalid/unsafe name. Otherwise, it returns 1.
 * If NAME is escaped, it is replaced by the unescaped name. */
int
validate_filename(char **name, const int is_md)
{
	if (!*name || !*(*name))
		return 0;

	char *deq = unescape_str(*name, 0);
	if (!deq) {
		xerror(_("%s: '%s': Error unescaping filename\n"),
			is_md ? "md" : "new", *name);
		return 0;
	}

	free(*name);
	*name = deq;

	/* Skip initial slash and "~/" */
	char *tmp = *deq != '/' ? deq : deq + 1;
	if (*tmp == '~' && tmp[1] == '/' && tmp[2])
		tmp += 2;

	char *p = tmp, *q = tmp;
	struct stat a;
	int ret = 1;

	while (1) {
		if (!*q) { /* Basename */
			if (lstat(deq, &a) == -1)
				ret = is_safe_filename(p);
			break;
		}

		if (*q != '/')
			goto NEXT;

		/* Path component */
		*q = '\0';

		if (lstat(deq, &a) == -1 && (ret = is_safe_filename(p)) == 0) {
			*q = '/';
			break;
		}

		*q = '/';

		if (!q[1])
			break;

		p = q + 1;

NEXT:
		++q;
	}

	return ret;
}
