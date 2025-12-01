/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2025 L. Abramovich <leo.clifm@outlook.com>
*/

/* sort.c -- functions used to sort files */

#include "helpers.h"

#include <string.h>
#include <unistd.h>
#include <strings.h> /* str(n)casecmp() */

#include "autocmds.h" /* update_autocmd_opts() */
#include "aux.h"      /* xatoi */
#include "checks.h"
#include "listing.h"
#include "messages.h" /* SORT_USAGE */

#define F_SORT(a, b)      ((a) == (b) ? 0 : ((a) > (b) ? 1 : -1))
#define F_SORT_DIRS(a, b) ((a) == (b) ? 0 : ((a) < (b) ? 1 : -1))

int
skip_files(const struct dirent *ent)
{
	/* In case a directory isn't reacheable, like a failed
	 * mountpoint... */
	/*  struct stat a;

	if (lstat(ent->d_name, &a) == -1) {
		fprintf(stderr, _("lstat: cannot access '%s': %s\n"),
			ent->d_name, strerror(errno));
		return 0;
	} */

	if (SELFORPARENT(ent->d_name))
		return 0;

	/* Skip files matching FILTER */
	// ADD FILTER TYPE CHECK!
	if (filter.str
	&& regexec(&regex_exp, ent->d_name, 0, NULL, 0) == FUNC_SUCCESS)
		return 0;

	/* If not hidden files */
	if (conf.show_hidden == 0 && *ent->d_name == '.')
		return 0;

	return 1;
}

/* Return a pointer to the first alphanumeric character in NAME, or to the
 * first character if no alphanumeric character is found.
 * This function is not UTF8 aware, meaning that UTF8 non-alphanemeric
 * characters are not supported. */
static inline void
skip_name_prefixes(char **name)
{
	char *s = *name;

	while (*s) {
		if (IS_ALNUM(*s) || IS_UTF8_LEAD_BYTE(*s))
			break;
		s++;
	}

	if (!*s)
		s = *name;

	*name = s;
}

/* Simple comparison routine for qsort()ing strings */
int
compare_strings(char **s1, char **s2)
{
#if defined(HAVE_STRCOLL)
	return strcoll(*s1, *s2);
#else
	const int ret = **s1 - **s2;
	return ret == 0 ? strcmp(*s1, *s2) : ret;
#endif /* HAVE_STRCOLL */
}

static inline int
check_hidden_file(const char c1, const char c2)
{
	const int ret = ((c1 == '.' && c2 != '.') ? -1
		: ((c1 != '.' && c2 == '.') ? 1 : 0));

	return (ret == 0 ? 0
		: (conf.show_hidden == HIDDEN_FIRST ? ret : -ret));
}

static int
check_priority_sort_char(const char c1, const char c2)
{
	const char *psch = conf.priority_sort_char;

	if (!psch[1]) {  /* Single char */
		return ((c1 == *psch && c2 != *psch) ? -1
			: ((c1 != *psch && c2 == *psch) ? 1 : 0));
	}

	while (*psch) {
		if (c1 == *psch && c2 != *psch)
			return -1;
		if (c1 != *psch && c2 == *psch)
			return 1;
		psch++;
	}

	return 0;
}

static int
namecmp(char *s1, char *s2)
{
	if (conf.skip_non_alnum_prefix == 1) {
		skip_name_prefixes(&s1);
		skip_name_prefixes(&s2);
	}

	if (!IS_UTF8_LEAD_BYTE(*s1) && !IS_UTF8_LEAD_BYTE(*s2)) {
	/* None of the strings begins with a unicode char: compare the first
	 * byte of both strings. */
		char ac = *s1, bc = *s2;

		if (conf.case_sens_list == 0) {
			ac = (char)TOLOWER(*s1);
			bc = (char)TOLOWER(*s2);
		}

		if (bc > ac)
			return -1;

		if (bc < ac)
			return 1;
	}

	if (conf.case_sens_list == 0)
		return strcoll(s1, s2);

	return strcmp(s1, s2);
}

static inline int
sort_by_extension(struct fileinfo *pa, struct fileinfo *pb)
{
	const char *e1 = (pa->dir == 0 && pa->ext_name)
		? pa->ext_name + (pa->ext_name[1] != '\0') : NULL;

	const char *e2 = (pb->dir == 0 && pb->ext_name)
		? pb->ext_name + (pb->ext_name[1] != '\0') : NULL;

	if (e1 || e2) {
		if (!e1)
			return (-1);
		if (!e2)
			return 1;

		return conf.case_sens_list == 1
			? strcmp(e1, e2) : strcasecmp(e1, e2);
	}

	return 0;
}

static inline int
sort_by_owner(struct fileinfo *pa, struct fileinfo *pb)
{
	if (pa->uid_i.name && pb->uid_i.name)
		return namecmp(pa->uid_i.name, pb->uid_i.name);

	return F_SORT(pa->uid, pb->uid);
}

static inline int
sort_by_group(struct fileinfo *pa, struct fileinfo *pb)
{
	if (pa->gid_i.name && pb->gid_i.name)
		return namecmp(pa->gid_i.name, pb->gid_i.name);

	return F_SORT(pa->gid, pb->gid);
}

static inline int
sort_by_type(struct fileinfo *pa, struct fileinfo *pb)
{
	const mode_t m1 = pa->type;
	const mode_t m2 = pb->type;

	const int e1 = (pa->type == DT_REG && pa->exec == 1);
	const int e2 = (pb->type == DT_REG && pb->exec == 1);

	if (e1 > e2)
		return 1;
	if (e2 > e1)
		return (-1);

	if (m1 > m2)
		return 1;
	if (m2 > m1)
		return (-1);

	return sort_by_extension(pa, pb);
}

static int
sort_by_version(char *s1, char *s2, const int have_utf8)
{
	if (have_utf8 == 1)
		return namecmp(s1, s2);

	/* xstrverscmp is not UTF-8 aware. */
	if (conf.skip_non_alnum_prefix == 1) {
		skip_name_prefixes(&s1);
		skip_name_prefixes(&s2);
	}

	return xstrverscmp(s1, s2);
}

int
entrycmp(const void *a, const void *b)
{
	struct fileinfo *pa = (struct fileinfo *)a;
	struct fileinfo *pb = (struct fileinfo *)b;
	int st = conf.sort;

	int ret = conf.list_dirs_first == 1 ? F_SORT_DIRS(pa->dir, pb->dir) : 0;
	if (ret != 0)
		return ret;

	ret = (conf.priority_sort_char && *conf.priority_sort_char)
		? check_priority_sort_char(*pa->name, *pb->name) : 0;
	if (ret != 0)
		return ret;

	/* > 1 = Either HIDDEN_FIRST or HIDDEN_LAST */
	ret = conf.show_hidden > 1 ? check_hidden_file(*pa->name, *pb->name) : 0;
	if (ret != 0)
		return ret;

	if (conf.light_mode == 1 && !ST_IN_LIGHT_MODE(st))
		st = SNAME;

	const int have_utf8 = (pa->utf8 == 1 || pb->utf8 == 1);

	switch (st) {
	case STSIZE: ret = F_SORT(pa->size, pb->size); break;
	case SATIME: /* fallthrough */
	case SBTIME: /* fallthrough */
	case SCTIME: /* fallthrough */
	case SMTIME: ret = F_SORT(pa->time, pb->time); break;
	case SVER: ret = sort_by_version(pa->name, pb->name, have_utf8); break;
	case SEXT: ret = sort_by_extension(pa, pb); break;
	case SINO: ret = F_SORT(pa->inode, pb->inode); break;
	case SOWN: ret = sort_by_owner(pa, pb); break;
	case SGRP: ret = sort_by_group(pa, pb); break;
	case SBLK: ret = F_SORT(pa->blocks, pb->blocks); break;
	case SLNK: ret = F_SORT(pa->linkn, pb->linkn); break;
	case STYPE: ret = sort_by_type(pa, pb); break;
	default: break;
	}

	if (ret == 0)
		ret = namecmp(pa->name, pb->name);

	return conf.sort_reverse == 0 ? ret : -ret;
}

/* Same as alphasort, but is uses strcmp instead of sctroll, which is
 * slower. However, bear in mind that, unlike strcmp(), strcoll() is locale
 * aware. Use only with C and english locales */
int
xalphasort(const struct dirent **a, const struct dirent **b)
{
	int ret = 0;

	/* The if statements prevent strcmp from running in every
	 * call to the function (it will be called only if the first
	 * character of the two strings is the same), which makes the
	 * function faster */
	if ((*a)->d_name[0] > (*b)->d_name[0])
		ret = 1;
	else if ((*a)->d_name[0] < (*b)->d_name[0])
		ret = -1;
	else
		ret = strcmp((*a)->d_name, (*b)->d_name);

	return conf.sort_reverse == 0 ? ret : -ret;
}

/* This is a modification of the alphasort function that makes it case
 * insensitive. It also sorts without taking the initial dot of hidden
 * files into account. Note that strcasecmp() isn't locale aware. Use
 * only with C and english locales */
int
alphasort_insensitive(const struct dirent **a, const struct dirent **b)
{
	const int ret = strcasecmp(((*a)->d_name[0] == '.') ? (*a)->d_name + 1
	: (*a)->d_name, ((*b)->d_name[0] == '.') ? (*b)->d_name + 1 : (*b)->d_name);

	return conf.sort_reverse == 0 ? ret : -ret;
}

char *
num_to_sort_name(const int n, const int abbrev)
{
	switch (n) {
	case SNONE:	 return "none";
	case SNAME:  return "name";
	case STSIZE: return "size";
	case SATIME: return "atime";
	case SBTIME: return "btime";
	case SCTIME: return "ctime";
	case SMTIME: return "mtime";
	case SVER:   return abbrev ? "ver" : "version";
	case SEXT:   return abbrev ? "ext" : "extension";
	case SINO:   return abbrev ? "ino" : "inode";
	case SOWN:   return abbrev ? "own" : "owner";
	case SGRP:   return abbrev ? "grp" : "group";
	case SBLK:   return abbrev ? "blk" : "blocks";
	case SLNK:   return abbrev ? "lnk" : "links";
	case STYPE:  return "type";
	default:     return abbrev ? "unk" : "unknown";
	}
}

void
print_sort_method(void)
{
	char *name = num_to_sort_name(conf.sort, 0);

	printf("%s%s%s%s", BOLD, name, NC,
		(conf.sort_reverse == 1) ? " [rev]" : "");

	if (conf.light_mode == 1 && !ST_IN_LIGHT_MODE(conf.sort))
		printf(_(" (not available in light mode: using %sname%s)\n"), BOLD, NC);
	else
		putchar('\n');
}

static inline int
re_sort_files_list(void)
{
	if (conf.autols == 0)
		return FUNC_SUCCESS;

	/* sort_switch just tells list_dir() to print a line with the current
	 * sort order at the end of the file list. */
	sort_switch = 1;
	free_dirlist();
	const int ret = list_dir();
	sort_switch = 0;

	return ret;
}

/* If ARG is a string, write the corresponding integer to ARG itself.
 * Return zero if ARG corresponds to a valid sort method or one
 * otherwise. */
static inline int
set_sort_by_name(char **arg)
{
	size_t i;
	for (i = 0; i <= SORT_TYPES; i++) {
		if (*(*arg) == *sort_methods[i].name
		&& strcmp(*arg, sort_methods[i].name) == 0) {
			if (conf.light_mode == 1
			&& !ST_IN_LIGHT_MODE(sort_methods[i].num)) {
				fprintf(stderr, _("st: '%s': Not available in light mode\n"),
					sort_methods[i].name);
				return FUNC_FAILURE;
			}

			*arg = xnrealloc(*arg, MAX_INT_STR, sizeof(char));
			snprintf(*arg, MAX_INT_STR, "%d", sort_methods[i].num);
			return FUNC_SUCCESS;
		}
	}

	fprintf(stdout, _("st: %s: No such sort order\n"), *arg);
	return FUNC_FAILURE;
}

int
sort_function(char **arg)
{
	/* No argument: Just print the current sort order. */
	if (!arg[1]) {
		fputs(_("Sorted by "), stdout);
		print_sort_method();
		return FUNC_SUCCESS;
	}

	/* Argument is an alphanumerical string */
	if (!is_number(arg[1])) {
		if (*arg[1] == 'r' && strcmp(arg[1], "rev") == 0) {
			conf.sort_reverse = !conf.sort_reverse;
			return re_sort_files_list();
		}

		if (set_sort_by_name(&arg[1]) == FUNC_FAILURE)
			return FUNC_FAILURE;
	}

	/* Argument is a number */
	const int n = atoi(arg[1]);

	if (conf.light_mode == 1 && !ST_IN_LIGHT_MODE(n)) {
		fprintf(stderr, _("st: %d (%s): Not available in light mode\n"),
			n, num_to_sort_name(n, 0));
		return FUNC_FAILURE;
	}

#ifndef ST_BTIME
	if (n == SBTIME) {
		fputs(_("st: Birth time is not available on this platform"), stderr);
		return FUNC_FAILURE;
	}
#endif /* !ST_BTIME */

	if (n >= 0 && n <= SORT_TYPES) {
		conf.sort = n;

		if (arg[2] && *arg[2] == 'r' && strcmp(arg[2], "rev") == 0)
			conf.sort_reverse = !conf.sort_reverse;

		update_autocmd_opts(AC_SORT);

		return re_sort_files_list();
	}

	/* If arg1 is a number but is not in the range 0-SORT_TYPES, err. */
	fprintf(stderr, "%s\n", _(SORT_USAGE));
	return FUNC_FAILURE;
}
