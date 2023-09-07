/* name_cleaner.c -- functions to clean up file names
 * 
 * This file is part of CliFM
 *
 * Copyright (C) 2016-2023, L. Abramovich <leo.clifm@outlook.com>
 * All rights reserved.

 * CliFM is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * CliFM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
*/

/* check_width, unpack_start, and unpack_cont macros, just
 * as get_utf_8_width and get_uft8_dec_value functions are taken from
 * https://github.com/dharple/detox/blob/main/src/clean_utf_8.c, licensed
 * under BSD-3-Clause.
 * All changes are licensed under GPL-2.0-or-later. */

#ifndef _NO_BLEACH

#include "helpers.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "aux.h"
#include "file_operations.h"
#include "history.h"
#ifdef GENERIC_FS_MONITOR
# include "listing.h" /* reload_dirlist() */
#endif /* GENERIC_FS_MONITOR */
#include "messages.h"
#include "misc.h"
#include "cleaner_table.h"
#include "readline.h"
#include "selection.h"

#define FUNC_NAME "bleach"
#define DEFAULT_TRANSLATION  '_'
#define BRACKETS_TRANSLATION '-'

#define UTF_8_ENCODED_MASK  0xC0
#define UTF_8_ENCODED_START 0xC0
#define UTF_8_ENCODED_CONT  0x80

#define UTF_8_ENCODED_6_BYTES_MASK 0xFE
#define UTF_8_ENCODED_6_BYTES      0xFC

#define UTF_8_ENCODED_5_BYTES_MASK 0xFC
#define UTF_8_ENCODED_5_BYTES      0xF8

#define UTF_8_ENCODED_4_BYTES_MASK 0xF8
#define UTF_8_ENCODED_4_BYTES      0xF0

#define UTF_8_ENCODED_3_BYTES_MASK 0xF0
#define UTF_8_ENCODED_3_BYTES      0xE0

#define UTF_8_ENCODED_2_BYTES_MASK 0xE0
#define UTF_8_ENCODED_2_BYTES      0xC0

#define BLEACH_TMP_HEADER "# CliFM - Bleach\n\
# Edit replacement file names as you wish, save, and close the editor.\n\
# You will be asked for confirmation at exit.\n\n"

#define check_width(chr, size) if ((chr & UTF_8_ENCODED_ ## size ## _BYTES_MASK) == UTF_8_ENCODED_ ## size ## _BYTES) { return size; }
#define unpack_start(chr, size) ((unsigned char) chr & ~UTF_8_ENCODED_ ## size ## _BYTES_MASK)
#define unpack_cont(chr) ((unsigned char) chr & ~UTF_8_ENCODED_MASK)

struct  bleach_t {
	char *original;
	char *replacement;
};

static int
get_utf_8_width(const char c)
{
	if ((c & UTF_8_ENCODED_MASK) == UTF_8_ENCODED_START) { /* UTF-8 leading byte */
		check_width(c, 2);
		check_width(c, 3);
		check_width(c, 4);
		check_width(c, 5);
		check_width(c, 6);
    }

	if ((c & UTF_8_ENCODED_MASK) == UTF_8_ENCODED_CONT) { /* UTF-8 continuation byte */
		return -1;
	}

	return 1;
}

/* Replace unsafe characters by safe, portable ones.
 * a-zA-Z0-9._- (Portable Filename Character Set) are kept
 * {[()]} are replaced by a dash (-)
 * Everything else is replaced by an underscore (_) */
static int
translate_unsafe_char(const unsigned char c)
{
	unsigned char t = 0;
	if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z')
	|| (c >= 'A' && c <= 'Z') || c == '.' || c == '_' || c == '-')
		t = c;
	else if (c == '(' || c == ')' || c == '[' || c == ']'
	|| c == '{' || c == '}')
		t = BRACKETS_TRANSLATION;
	else
		t = DEFAULT_TRANSLATION;

	return t;
}

static int
get_uft8_dec_value(size_t *i, char *str)
{
	unsigned char c = (unsigned char)str[*i];
	int new_value = 0;

	int utf8_width = get_utf_8_width((char)c);
	switch (utf8_width) {
		case 1: new_value = (unsigned char)str[0]; break;
		case 2: new_value = unpack_start(c, 2); break;
		case 3: new_value = unpack_start(c, 3); break;
		case 4: new_value = unpack_start(c, 4); break;
		case 5: new_value = unpack_start(c, 5); break;
		case 6: new_value = unpack_start(c, 6); break;
		default: return (-1); /* -1 */
	}

	int expected_chars = utf8_width - 1;
	int failed = 0;
	while (expected_chars > 0) {
		(*i)++;

		if (str[*i] == '\0') {
			failed = 1;
			break;
		}

		if ((str[*i] & UTF_8_ENCODED_MASK) != UTF_8_ENCODED_CONT) { /* Not a UTF-8 continuation byte */
			failed = 1;
			break;
		}

		new_value <<= 6;
		new_value += unpack_cont(str[*i]);

		expected_chars--;
	}

	return failed == 1 ? (-2) : new_value;
}

/* Clean up NAME either by removing those (extended-ASCII/Unicode) characters
 * without an ASCII alternative/similar character, or by translating (based
 * on the unitable table (in cleaner_table.h)) extended-ASCII/Unicode characters
 * into an alternative ASCII character based on familiarity/similarity. Disallowed
 * characters (NUL and slash) will be simply removed. The file name length will
 * be trimmed to NAME_MAX (usually 255). If the replacement file name is
 * only one character long, "bleach" will be appended to avoid too short
 * file names.
 *
 * @parameter: the file name to be cleaned up. If it contains a slash, only
 * the string after the slash will be taken a the actual file name
 *
 * @return: the cleaned file name or NULL in case of error. If the translated
 * file name is empty, it will be replaced by "bleach.YYYYMMDDHHMMS"
 * */
static char *
clean_file_name(char *restrict name)
{
	if (!name || !*name)
		return (char *)NULL;

	char *p = xcalloc(NAME_MAX + 1, sizeof(char));
	char *q = p;

	size_t i = 0, cur_len = 0, too_long = 0;

	char *s = strrchr(name, '/');
	i = s ? (size_t)(s - name) + 1 : 0;

	unsigned char n = 0;
	for (; (n = (unsigned char)name[i]); i++) {
		if (cur_len > NAME_MAX) {
			too_long = 1;
			break;
		}

		/* ASCII chars */
		if (n == 38) { /* & */
			if (q == p || *(q - 1) != DEFAULT_TRANSLATION) {
				xstrncat(q, strlen(q), "_and_", NAME_MAX + 1);
				q += 5;
				cur_len += 5;
			} else {
				snprintf(q - 1, NAME_MAX, "_and_");
				q += 4;
				cur_len += 4;
			}
			continue;
		}

		if (n <= 126) {
			int ret = translate_unsafe_char(n);
			if (ret == -1)
				continue;

			if (ret == BRACKETS_TRANSLATION) {
				if (q == p || (*(q - 1) != BRACKETS_TRANSLATION
				&& *(q - 1) != DEFAULT_TRANSLATION)) {
					*q = (char)ret;
					q++;
					cur_len++;
				} else {
					if (*(q - 1) == DEFAULT_TRANSLATION) {
						q--;
						*q = (char)ret;
					}
				}
			}

			else if (ret == DEFAULT_TRANSLATION && (q == p
			|| (*(q - 1) != DEFAULT_TRANSLATION
			&& *(q - 1) != BRACKETS_TRANSLATION))) {
				*q = (char)ret;
				q++;
				cur_len++;
			}

			else {
				if (ret != BRACKETS_TRANSLATION
				&& ret != DEFAULT_TRANSLATION) {
					*q = (char)ret;
					q++;
					cur_len++;
				}
			}
			continue;
		}

		/* Extended ASCII and Unicode chars */
		int dec_value = get_uft8_dec_value(&i, name);
		if (!name[i])
			break;

		if (dec_value == -1)
			continue;
		if (dec_value == -2) {
			if (q != p && *(q - 1) != DEFAULT_TRANSLATION) {
				*q = DEFAULT_TRANSLATION;
				q++;
				cur_len++;
			}
			continue;
		}

		int j = (int)(sizeof(unitable) / sizeof(struct utable_t));
		char *t = (char *)NULL;
		while (--j >= 0) {
			if (dec_value == unitable[j].key && unitable[j].data)
				t = unitable[j].data;
		}

		if (!t)
			continue;

		size_t tlen = strlen(t);
		if (q == p || *(q - 1) != DEFAULT_TRANSLATION) {
			xstrncat(q, strlen(q), t, NAME_MAX + 1);
			q += tlen;
			cur_len += tlen;
		} else {
			snprintf(q - 1, NAME_MAX, "%s", t);
			q += (tlen > 0 ? tlen - 1 : 0);
			cur_len += (tlen > 0 ? tlen - 1 : 0);
		}
	}

	*q = '\0';

	if (too_long)
		p[NAME_MAX] = '\0';

	/* Handle some file names that should be avoided */

	if (!*p) { /* Empty file name */
		time_t rawtime = time(NULL);
		struct tm t;
		char *suffix = localtime_r(&rawtime, &t)
			? gen_date_suffix(t) : (char *)NULL;
		if (!suffix) {
			free(p);
			return (char *)NULL;
		}
		snprintf(p, NAME_MAX, "%s.%s", FUNC_NAME, suffix);
		free(suffix);
	} else {
		if (!*(p + 1)) {
			/* Avoid one character long file names. Specially because files
			 * named with a single dot should be avoided */
			char c = *p;
			snprintf(p, NAME_MAX, "%c.%s", c, FUNC_NAME);
		}
	}

	/* Do not make hidden a file that wasn't */
	if (*name != '.' && *p == '.')
		*p = DEFAULT_TRANSLATION;

	/* File names shouldn't start with a dash/hyphen (reserved for command options)*/
	if (*p == '-')
		*p = DEFAULT_TRANSLATION;

	/* No file name should be named dot-dot (..) */
	if (cur_len == 3 && *p == '.' && *(p + 1) == '.' && !*(p + 2))
		*(p + 1) = DEFAULT_TRANSLATION;

	return p;
}

/* Let the user edit replacement file names via a text editor and return
 * a new array with the updated file names. If the temp file is not modified
 * the original list of files is returned instead. In case of error
 * returns NULL */
static struct bleach_t *
edit_replacements(struct bleach_t *bfiles, size_t *n, int *edited_names)
{
	if (!bfiles || !bfiles[0].original)
		return (struct bleach_t *)NULL;

	*edited_names = 1;

	char f[PATH_MAX];
	snprintf(f, sizeof(f), "%s/%s", (xargs.stealth_mode == 1)
		? P_tmpdir : tmp_dir, TMP_FILENAME);

	int fd = mkstemp(f);
	if (fd == -1) {
		err('e', PRINT_PROMPT, "bleach: mkstemp: %s: %s\n", f, strerror(errno));
		return (struct bleach_t *)NULL;
	}

	FILE *fp = (FILE *)NULL;

#ifndef HAVE_DPRINTF
	fp = fopen(f, "w");
	if (!fp) {
		if (unlink(f) == -1)
			err('w', PRINT_PROMPT, "bleach: %s: %s\n", f, strerror(errno));
		err('e', PRINT_PROMPT, "bleach: %s: %s\n", f, strerror(errno));
		return (struct bleach_t *)NULL;
	}
#endif /* !HAVE_DPRINT */

#ifdef HAVE_DPRINTF
	dprintf(fd, BLEACH_TMP_HEADER);
#else
	fprintf(fp, BLEACH_TMP_HEADER);
#endif /* HAVE_DPRINTF */

	size_t i;
	/* Copy all files to be renamed to the temp file */
	for (i = 0; i < *n; i++) {
#ifdef HAVE_DPRINTF
		dprintf(fd, "original: %s\nreplacement: %s\n\n",
			bfiles[i].original, bfiles[i].replacement);
#else
		fprintf(fp, "original: %s\nreplacement: %s\n\n",
			bfiles[i].original, bfiles[i].replacement);
#endif /* HAVE_DPRINTF */
	}
	size_t total_files = i;

#if defined(__HAIKU__) || defined(__sun)
	fclose(fp);
#endif /* __HAIKU__ || __sun */
	close(fd);

	fp = open_fread(f, &fd);
	if (!fp) {
		if (unlink(f) == -1)
			err('w', PRINT_PROMPT, "bleach: %s: %s\n", f, strerror(errno));
		err('e', PRINT_PROMPT, "bleach: %s: %s\n", f, strerror(errno));
		return (struct bleach_t *)NULL;
	}

	struct stat attr;
	fstat(fd, &attr);
	time_t mtime_bfr = (time_t)attr.st_mtime;

	/* Open the temp file */
	open_in_foreground = 1;
	int exit_status = open_file(f);
	open_in_foreground = 0;
	if (exit_status != EXIT_SUCCESS) {
		xerror("bleach: %s: %s\n", f, strerror(errno));
		if (unlink(f) == -1)
			xerror("bleach: %s: %s\n", f, strerror(errno));
		fclose(fp);
		return (struct bleach_t *)NULL;
	}

	fclose(fp);
	fp = open_fread(f, &fd);
	if (!fp) {
		if (unlink(f) == -1)
			err('w', PRINT_PROMPT, "bleach: %s: %s\n", f, strerror(errno));
		err('e', PRINT_PROMPT, "bleach: %s: %s\n", f, strerror(errno));
		return (struct bleach_t *)NULL;
	}

	/* Compare the new modification time to the stored one: if they
	 * match, nothing has been modified. */
	fstat(fd, &attr);
	if (mtime_bfr == (time_t)attr.st_mtime) {
		if (unlinkat(fd, f, 0) == -1)
			err('w', PRINT_PROMPT, "bleach: %s: %s\n", f, strerror(errno));
		fclose(fp);
		*edited_names = 0;
		return bfiles; /* Return the original list of files */
	}

	/* Free the original list of files */
	for (i = 0; i < *n; i++) {
		free(bfiles[i].original);
		free(bfiles[i].replacement);
	}

	/* Allocate memory for the new list */
	free(bfiles);
	bfiles = (struct bleach_t *)xnmalloc(total_files + 1,
			sizeof(struct bleach_t));

	/* Initialize all values */
	for (i = 0; i < total_files; i++) {
		bfiles[i].original = (char *)NULL;
		bfiles[i].replacement = (char *)NULL;
	}

	size_t line_size = 0;
	char *line = (char *)NULL;
	ssize_t line_len = 0;

	i = 0;
	/* Read the temp file and store the new values */
	while ((line_len = getline(&line, &line_size, fp)) > 0) {
		if (i >= total_files)
			break;

		if (line[line_len - 1] == '\n')
			line[line_len - 1] = '\0';

		if (*line != 'o' && *line != 'r')
			continue;

		char *p = strchr(line, ' ');
		if (!p || !*(++p))
			continue;

		if (strncmp(line, "original: ", 10) == 0) {
			if (bfiles[i].original) {
				free(bfiles[i].original);
				bfiles[i].original = (char *)NULL;
			}
			bfiles[i].original = savestring(p, strlen(p));
		/* Do not store the replacement file name is there is no original */
		} else if (strncmp(line, "replacement: ", 13) == 0
		&& bfiles[i].original) {
			bfiles[i].replacement = savestring(p, strlen(p));
			printf("%s %s->%s %s\n", bfiles[i].original, mi_c, df_c,
				bfiles[i].replacement);
			i++;
		} else {
			continue;
		}
	}

	/* Make sure no field of the struct is empty/NULL*/
	size_t j = 0;
	for (i = 0; i < total_files; i++) {
		if (!bfiles[i].original || !*bfiles[i].original
		|| !bfiles[i].replacement || !*bfiles[i].replacement) {
			free(bfiles[i].original);
			free(bfiles[i].replacement);
			bfiles[i].original = (char *)NULL;
			bfiles[i].replacement = (char *)NULL;
		} else {
			j++;
		}
	}

	*n = j;
	free(line);

	if (unlinkat(fd, f, 0) == -1)
		err('w', PRINT_PROMPT, "bleach: %s: %s\n", f, strerror(errno));
	fclose(fp);

	return bfiles;
}

/* Clean up the list of file names (NAMES), print the list of the cleaned
 * file names (allowing the user to edit this list), and finally
 * rename the original file names into the clean ones */
int
bleach_files(char **names)
{
	if (!names || !names[1] || IS_HELP(names[1])) {
		puts(_(BLEACH_USAGE));
		return EXIT_SUCCESS;
	}

	struct bleach_t *bfiles = (struct bleach_t *)NULL;

	size_t f = 0, i = 1;
	for (; names[i]; i++) {
		char *dstr = dequote_str(names[i], 0);
		if (!dstr) {
			xerror(_("bleach: %s: Error dequoting file name\n"), names[i]);
			continue;
		}

		xstrsncpy(names[i], dstr, strlen(dstr) + 1);
		free(dstr);
		size_t nlen = strlen(names[i]);
		if (names[i][nlen - 1] == '/') {
			names[i][nlen - 1] = '\0';
			nlen--;
		}

		char *sl = strrchr(names[i], '/');
		char *p = clean_file_name((sl && *(sl + 1)) ? sl + 1 : names[i]);
		if (!p)
			continue;

		/* Nothing to clean. Skip this one */
		char *n = (sl && *(sl + 1)) ? sl + 1 : names[i];
		if (*n == *p && strcmp(n, p) == 0) {
			free(p);
			continue;
		}

		bfiles = (struct bleach_t *)xrealloc(bfiles, (f + 1) * sizeof(struct bleach_t));
		bfiles[f].original = savestring(names[i], nlen);
		if (sl) {
			*sl = '\0';
			size_t len = nlen + strlen(p) + 2;
			bfiles[f].replacement = (char *)xnmalloc(len, sizeof(char));
			snprintf(bfiles[f].replacement, len, "%s/%s", names[i], p);
			*sl = '/';
		} else {
			bfiles[f].replacement = savestring(p, strlen(p));
		}
		printf("%s %s->%s %s\n",
			bfiles[f].original, mi_c, df_c, bfiles[f].replacement);
		f++;
		free(p);
	}

	if (f == 0 || !bfiles) {
		printf(_("%s: Nothing to do\n"), FUNC_NAME);
		return EXIT_SUCCESS;
	}

	int rename = 0;
	char *input = (char *)NULL;

CONFIRM:
	putchar('\n');
	int _edit = 0, edited_names = 0;

	while (!input) {
		input = rl_no_hist(_("Is this OK? [y/n/(e)dit] "));

		if (!input)
			continue;

		if (input[1] || !strchr("yYnNeEq", *input)) {
			free(input);
			input = (char *)NULL;
			continue;
		}

		switch (*input) {
		case 'y': /* fallthrough */
		case 'Y': rename = 1; break;
		case 'e':
			_edit = 1;
			bfiles = edit_replacements(bfiles, &f, &edited_names);
			if (!bfiles)
				break;

			if (edited_names == 1 && f > 0) {
				free(input);
				input = (char *)NULL;
				goto CONFIRM;
			} else {
				rename = 1;
			}
			break;
		default: break;
		}
	}

	free(input);

	if (f == 0) {
		/* Just in case either the original or the replacement file name
		 * was removed from the list by the user, leaving only one of the two. */
		free(bfiles[0].original);
		free(bfiles[0].replacement);
		free(bfiles);
		printf(_("%s: Nothing to do\n"), FUNC_NAME);
		return EXIT_SUCCESS;
	}

	/* The user entered 'e' to edit the file, but nothing was modified
	 * Ask for confirmation in case the user just wanted to see what would
	 * be done. */
	if (_edit == 1) {
		printf(_("%zu %s will be bleached\n"), f, f > 1 ? _("files") : _("file"));
		if (rl_get_y_or_n(_("Continue? [y/n] ")) != 1) {
			for (i = 0; i < f; i++) {
				free(bfiles[i].original);
				free(bfiles[i].replacement);
			}
			free(bfiles);
			return EXIT_SUCCESS;
		}
	}

	/* If renaming all selected files, deselect them */
	if (rename == 1 && is_sel)
		deselect_all();

	int total_rename = rename == 1 ? (int)f : 0;
	size_t rep_suffix = 1;
	int exit_status = EXIT_SUCCESS;

	for (i = 0; i < f; i++) {
		char *o = bfiles[i].original ? bfiles[i].original : (char *)NULL;
		char *r = bfiles[i].replacement ? bfiles[i].replacement : (char *)NULL;
		if (o && *o && r && *r && rename) {
			/* Make sure the replacement file name does not exist. If
			 * it does, append REP_SUFFIX and try again. */
			struct stat a;
			while (lstat(r, &a) == 0) {
				char tmp[PATH_MAX];
				xstrsncpy(tmp, r, sizeof(tmp));
				r = (char *)xrealloc(r, (PATH_MAX + 32) * sizeof(char));
				snprintf(r, PATH_MAX + 31, "%s-%zu", tmp, rep_suffix);
				rep_suffix++;
			}

			if (renameat(XAT_FDCWD, o, XAT_FDCWD, r) == -1) {
				xerror("bleach: renameat: %s: %s\n", o, strerror(errno));
				total_rename--;
				exit_status = EXIT_FAILURE;
			}
		}

		free(o);
		free(r);
	}

	free(bfiles);

	if (exit_status == EXIT_FAILURE || total_rename == 0) {
		printf(_("%s: %d file(s) bleached\n"), FUNC_NAME, total_rename);
	} else {
#ifdef GENERIC_FS_MONITOR
		if (conf.autols == 1)
			reload_dirlist();
		print_reload_msg(_("%s: %d file(s) bleached\n"),
			FUNC_NAME, total_rename);
#else
		err(ERR_NO_LOG, PRINT_PROMPT, _("%s->%s %s: %d file(s) bleached\n"),
			mi_c, df_c, FUNC_NAME, total_rename);
#endif /* GENERIC_FS_MONITOR */
	}

	return exit_status;
}

#else
void *_skip_me_bleach;
#endif /* !_NO_BLEACH */
