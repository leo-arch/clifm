/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2025 L. Abramovich <leo.clifm@outlook.com>
*/

/* name_cleaner.c -- functions to sanitize filenames */

/* check_width, unpack_start, and unpack_cont macros, just
 * as get_utf_8_width and get_uft8_dec_value functions are taken from
 * https://github.com/dharple/detox/blob/main/src/clean_utf_8.c, licensed
 * under BSD-3-Clause.
 * All changes are licensed under GPL-2.0-or-later. */

#ifndef _NO_BLEACH

#include "helpers.h"

#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#if defined(MAC_OS_X_RENAMEAT_SYS_STDIO_H)
# include <sys/stdio.h> /* renameat(2) */
#endif /* MAC_OS_X_RENAMEAT_SYS_STDIO_H */

#include "aux.h"
#include "cleaner_table.h"
#include "file_operations.h"
#include "history.h"
#include "listing.h" /* reload_dirlist() */
#include "messages.h"
#include "misc.h"
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

#define BLEACH_TMP_HEADER "# Clifm - Bleach\n\
# Edit replacement filenames as you wish, save, and close the editor.\n\
# You will be asked for confirmation at exit.\n\n"

#define check_width(chr, size) if (((chr) & UTF_8_ENCODED_ ## size ## _BYTES_MASK) == UTF_8_ENCODED_ ## size ## _BYTES) { return size; }
#define unpack_start(chr, size) ((unsigned char)(chr) & ~UTF_8_ENCODED_ ## size ## _BYTES_MASK)
#define unpack_cont(chr) ((unsigned char)(chr) & ~UTF_8_ENCODED_MASK)

struct bleach_t {
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

	if ((c & UTF_8_ENCODED_MASK) == UTF_8_ENCODED_CONT) /* UTF-8 continuation byte */
		return -1;

	return 1;
}

/* Replace unsafe characters by safe, portable ones.
 * a-zA-Z0-9._- (Portable Filename Character Set) are kept.
 * {[()]} are replaced by a dash (-).
 * Everything else is replaced by an underscore (_). */
static int
translate_unsafe_char(const unsigned char c)
{
	unsigned char t = 0;
	if (IS_ALNUM(c) || c == '.' || c == '_' || c == '-')
		t = c;
	else if (c == '(' || c == ')' || c == '[' || c == ']'
	|| c == '{' || c == '}')
		t = BRACKETS_TRANSLATION;
	else
		t = DEFAULT_TRANSLATION;

	return t;
}

static int
get_uft8_dec_value(size_t *i, const char *str)
{
	unsigned char c = (unsigned char)str[*i];
	int new_value = 0;

	const int utf8_width = get_utf_8_width((char)c);
	switch (utf8_width) {
		case 1: new_value = (unsigned char)str[0]; break;
		case 2: new_value = unpack_start(c, 2); break;
		case 3: new_value = unpack_start(c, 3); break;
		case 4: new_value = unpack_start(c, 4); break;
		case 5: new_value = unpack_start(c, 5); break;
		case 6: new_value = unpack_start(c, 6); break;
		default: return (-1);
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
 * characters (NUL and slash) will be simply removed. The filename length will
 * be trimmed to NAME_MAX (usually 255). If the replacement filename is
 * only one character long, "bleach" will be appended to avoid too short
 * filenames.
 *
 * @parameter: the filename to be sanitized. If it contains a slash, only
 * the string after the slash will be taken a the actual filename
 *
 * @return: the sanitized filename or NULL in case of error. If the translated
 * filename is empty, it will be replaced by "bleach.YYYYMMDDHHMMS"
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

		if (n <= 127) {
			const int ret = translate_unsafe_char(n);
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
		const int dec_value = get_uft8_dec_value(&i, name);
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

	/* Handle some filenames that should be avoided */

	if (!*p) { /* Empty filename */
		time_t rawtime = time(NULL);
		struct tm t;
		char *suffix = localtime_r(&rawtime, &t)
			? gen_date_suffix(t, 0) : (char *)NULL;
		if (!suffix) {
			free(p);
			return (char *)NULL;
		}
		snprintf(p, NAME_MAX, "%s.%s", FUNC_NAME, suffix);
		free(suffix);
	} else {
		if (!p[1]) {
			/* Avoid one character long filenames. Specially because files
			 * named with a single dot should be avoided */
			char c = *p;
			snprintf(p, NAME_MAX, "%c.%s", c, FUNC_NAME);
		}
	}

	/* Do not make hidden a file that wasn't */
	if (*name != '.' && *p == '.')
		*p = DEFAULT_TRANSLATION;

	/* Filenames shouldn't start with a dash/hyphen (reserved for command options)*/
	if (*p == '-')
		*p = DEFAULT_TRANSLATION;

	/* No filename should be named dot-dot (..) */
	if (cur_len == 3 && *p == '.' && *(p + 1) == '.' && !*(p + 2))
		*(p + 1) = DEFAULT_TRANSLATION;

	return p;
}

/* Let the user edit replacement filenames via a text editor and return
 * a new array with the updated filenames. If the temp file is not modified
 * the original list of files is returned instead. In case of error
 * returns NULL */
static struct bleach_t *
edit_replacements(struct bleach_t *bfiles, size_t *n, int *edited_names)
{
	if (!bfiles || !bfiles[0].original)
		return (struct bleach_t *)NULL;

	*edited_names = 1;

	char f[PATH_MAX + 1];
	snprintf(f, sizeof(f), "%s/%s", (xargs.stealth_mode == 1)
		? P_tmpdir : tmp_dir, TMP_FILENAME);

	int fd = mkstemp(f);
	if (fd == -1)
		goto ERROR;

	FILE *fp = fdopen(fd, "w");
	if (!fp)
		goto ERROR;

	fprintf(fp, BLEACH_TMP_HEADER);

	size_t i;
	/* Copy all files to be renamed to the temp file */
	for (i = 0; i < *n; i++) {
		fprintf(fp, "original: %s\nreplacement: %s\n\n",
			bfiles[i].original, bfiles[i].replacement);
	}
	size_t total_files = i;

	struct stat attr;
	if (fstat(fd, &attr) == -1)
		goto ERROR_CLOSE;
	const time_t mtime_bfr = attr.st_mtime;

	fclose(fp);

	/* Open the temp file */
	open_in_foreground = 1;
	const int exit_status = open_file(f);
	open_in_foreground = 0;
	if (exit_status != FUNC_SUCCESS)
		goto ERROR;

	fp = open_fread(f, &fd);
	if (!fp)
		goto ERROR;

	/* Compare the new modification time to the stored one: if they
	 * match, nothing has been modified. */
	if (fstat(fd, &attr) == -1)
		goto ERROR_CLOSE;

	if (mtime_bfr == attr.st_mtime) {
		fclose(fp);
		if (unlinkat(fd, f, 0) == -1)
			err('w', PRINT_PROMPT, "bleach: Cannot remove '%s': %s\n",
				f, strerror(errno));
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
		/* Do not store the replacement filename is there is no original */
		} else if (strncmp(line, "replacement: ", 13) == 0
		&& bfiles[i].original) {
			bfiles[i].replacement = savestring(p, strlen(p));
			printf("%s %s%s%s %s\n", bfiles[i].original, mi_c, SET_MSG_PTR,
				df_c, bfiles[i].replacement);
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
		err('w', PRINT_PROMPT, "bleach: Cannot remove '%s': %s\n",
			f, strerror(errno));
	fclose(fp);

	return bfiles;

ERROR_CLOSE:
	fclose(fp);
ERROR:
	*edited_names = -1;
	xerror("bleach: '%s': %s\n", f, strerror(errno));
	if (unlink(f) == -1)
		xerror("bleach: Cannot remove '%s': %s\n", f, strerror(errno));
	return bfiles;
}

/* Clean up the list of filenames (NAMES), print the list of the sanitized
 * filenames (allowing the user to edit this list), and finally
 * rename the original filenames into the clean ones */
int
bleach_files(char **names)
{
	if (!names || !names[1] || IS_HELP(names[1])) {
		puts(_(BLEACH_USAGE));
		return FUNC_SUCCESS;
	}

	int do_edit = 0, edited_names = 0;
	struct bleach_t *bfiles = (struct bleach_t *)NULL;

	size_t f = 0, i = 1;
	for (; names[i]; i++) {
		char *dstr = unescape_str(names[i], 0);
		if (!dstr) {
			xerror(_("bleach: '%s': Error unescaping filename\n"), names[i]);
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

		bfiles = xnrealloc(bfiles, f + 1, sizeof(struct bleach_t));
		bfiles[f].original = savestring(names[i], nlen);
		if (sl) {
			*sl = '\0';
			const size_t len = nlen + strlen(p) + 2;
			bfiles[f].replacement = xnmalloc(len, sizeof(char));
			snprintf(bfiles[f].replacement, len, "%s/%s", names[i], p);
			*sl = '/';
		} else {
			bfiles[f].replacement = savestring(p, strlen(p));
		}
		printf("%s %s%s%s %s\n",
			bfiles[f].original, mi_c, SET_MSG_PTR, df_c, bfiles[f].replacement);
		f++;
		free(p);
	}

	if (f == 0 || !bfiles) {
		printf(_("%s: Nothing to do\n"), FUNC_NAME);
		return FUNC_SUCCESS;
	}

	int rename = 0, quit_func = 0;
	char *input = (char *)NULL;

CONFIRM:
	while (!input) {
		input = rl_no_hist(_("Is this OK? [y/n/(e)dit] "), 0);
		if (!input)
			continue;

		if (strcmp(input, "yes") == 0 || strcmp(input, "no") == 0
		|| strcmp(input, "edit") == 0)
			input[1] = '\0';

		if (input[1] || !strchr("yYnNeEq", *input)) {
			free(input);
			input = (char *)NULL;
			continue;
		}

		switch (*input) {
		case 'y': /* fallthrough */
		case 'Y': rename = 1; break;
		case 'q': /* fallthrough */
		case 'n': /* fallthrough */
		case 'N': quit_func = 1; break;
		case 'e':
			do_edit = 1;
			bfiles = edit_replacements(bfiles, &f, &edited_names);
			if (!bfiles || edited_names == -1)
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
		/* Just in case either the original or the replacement filename
		 * was removed from the list by the user, leaving only one of the two. */
		if (bfiles) {
			free(bfiles[0].original);
			free(bfiles[0].replacement);
		}
		free(bfiles);
		printf(_("%s: Nothing to do\n"), FUNC_NAME);
		return FUNC_SUCCESS;
	}

	if (edited_names == -1 || quit_func == 1) { /* ERROR or quit */
		if (bfiles) {
			for (i = 0; i < f; i++) {
				free(bfiles[i].original);
				free(bfiles[i].replacement);
			}
		}
		free(bfiles);
		return (quit_func == 1 ? FUNC_SUCCESS : FUNC_FAILURE);
	}

	/* The user entered 'e' to edit the file, but nothing was modified
	 * Ask for confirmation in case the user just wanted to see what would
	 * be done. */
	if (do_edit == 1) {
		printf(_("%zu filename(s) will be bleached\n"), f);
		if (rl_get_y_or_n(_("Continue?"), 0) != 1) {
			if (bfiles) {
				for (i = 0; i < f; i++) {
					free(bfiles[i].original);
					free(bfiles[i].replacement);
				}
			}
			free(bfiles);
			return FUNC_SUCCESS;
		}
	}

	/* If renaming all selected files, deselect them */
	if (rename == 1 && is_sel)
		deselect_all();

	int total_rename = rename == 1 ? (int)f : 0;
	size_t rep_suffix = 1;
	int exit_status = FUNC_SUCCESS;

	for (i = 0; i < f; i++) {
		char *o = (bfiles && bfiles[i].original)
			? bfiles[i].original : (char *)NULL;
		char *r = (bfiles && bfiles[i].replacement)
			? bfiles[i].replacement : (char *)NULL;

		if (o && *o && r && *r && rename) {
			/* Make sure the replacement filename does not exist. If
			 * it does, append REP_SUFFIX and try again. */
			struct stat a;
			while (lstat(r, &a) == 0) {
				char tmp[PATH_MAX + 1];
				xstrsncpy(tmp, r, sizeof(tmp));
				const size_t len = PATH_MAX + MAX_INT_STR + 2;
				r = xnrealloc(r, len, sizeof(char));
				snprintf(r, len, "%s-%zu", tmp, rep_suffix);
				rep_suffix++;
			}

			if (renameat(XAT_FDCWD, o, XAT_FDCWD, r) == -1) {
				xerror(_("bleach: Cannot rename '%s' to '%s': %s\n"),
					o, r, strerror(errno));
				total_rename--;
				exit_status = FUNC_FAILURE;
			}
		}

		free(o);
		free(r);
	}

	free(bfiles);

	if (exit_status == FUNC_FAILURE || total_rename == 0) {
		printf(_("%s: %d filenames(s) bleached\n"), FUNC_NAME, total_rename);
	} else {
		if (conf.autols == 1)
			reload_dirlist();
		print_reload_msg(SET_SUCCESS_PTR, xs_cb,
			_("%d filename(s) bleached\n"), total_rename);
	}

	return exit_status;
}

#else
void *_skip_me_bleach;
#endif /* !_NO_BLEACH */
