/* sort.c -- functions used to sort files */

/*
 * This file is part of CliFM
 *
 * Copyright (C) 2016-2024, L. Abramovich <leo.clifm@outlook.com>
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

#include "helpers.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <strings.h> /* str(n)casecmp() */

#include "checks.h"
#include "aux.h" /* xatoi */
#include "listing.h"
#include "messages.h" /* SORT_USAGE */

int
skip_nonexec(const struct dirent *ent)
{
#ifdef _DIRENT_HAVE_D_TYPE
	if (ent->d_type == DT_REG && access(ent->d_name, X_OK) == -1)
#else
	if (access(ent->d_name, X_OK) == -1)
#endif /* _DIRENT_HAVE_D_TYPE */
		return 0;

	return 1;
}

int
skip_files(const struct dirent *ent)
{
	/* In case a directory isn't reacheable, like a failed
	 * mountpoint... */
	/*  struct stat file_attrib;

	if (lstat(entry->d_name, &file_attrib) == -1) {
		fprintf(stderr, _("stat: cannot access '%s': %s\n"),
				entry->d_name, strerror(errno));
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
 * first character if no alphanumeric character is found */
static inline void
skip_name_prefixes(char **name)
{
	char *s = *name;

	while (*s) {
		if (IS_ALNUM(*s))
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
	return strcoll(*s2, *s2);
#else
	int ret = **s1 - **s2;
	if (ret == 0)
		ret = strcmp(*s1, *s2);

	return ret;
#endif /* HAVE_STRCOLL */
}

static int
namecmp(char *s1, char *s2)
{
	if (conf.skip_non_alnum_prefix == 1) {
		skip_name_prefixes(&s1);
		skip_name_prefixes(&s2);
	}

	/* If both strings start with number, sort them as numbers, not as strings */
	if (IS_DIGIT(*s1) && IS_DIGIT(*s2)) {
		char *p1, *p2;
		const long long n1 = strtoll(s1, &p1, 10);
		const long long n2 = strtoll(s2, &p2, 10);
		if (n2 > n1)
			return -1;
		if (n2 < n1)
			return 1;
	}

	char ac = *s1, bc = *s2;

	if ((*s1 & 0xc0) != 0xc0 && (*s2 & 0xc0) != 0xc0) {
	/* None of the strings starts with a unicode char: compare the first
	 * byte of both strings */
		if (!conf.case_sens_list) {
			ac = (char)TOLOWER(*s1);
			bc = (char)TOLOWER(*s2);
		}

		if (bc > ac)
			return -1;

		if (bc < ac)
			return 1;
	}

	if (!conf.case_sens_list || (*s1 & 0xc0) == 0xc0 || (*s2 & 0xc0) == 0xc0)
		return strcoll(s1, s2);

	return strcmp(s1, s2);
}

static inline int
sort_by_extension(const char *n1, const char *n2, const int n1dir,
	const int n2dir)
{
	int ret = 0;
	char *aext = (char *)NULL, *bext = (char *)NULL, *val;

	/* If the entry is a directory, we don't take it as having an extension,
	 * even if it has a dot in its name. */
	val = n1dir == 0 ? strrchr(n1, '.') : (char *)NULL;
	if (val && val != n1)
		aext = val + 1;

	val = n2dir == 0 ? strrchr(n2, '.') : (char *)NULL;
	if (val && val != n2)
		bext = val + 1;

	if (aext || bext) {
		if (!aext)
			ret = -1;
		else if (!bext)
			ret = 1;
		else
			ret = strcasecmp(aext, bext);
	}

	return ret;
}

static inline int
sort_by_size(struct fileinfo *pa, struct fileinfo *pb)
{
	const off_t as = pa->size, bs = pb->size;

	if (as > bs)
		return 1;

	if (as < bs)
		return (-1);

	return 0;
}

static inline int
sort_by_time(const time_t a, const time_t b)
{
	int ret = 0;

	if (a > b) {
		ret = 1;
	} else {
		if (a < b)
			ret = -1;
	}

	return ret;
}

static inline int
sort_by_inode(const ino_t a, const ino_t b)
{
	int ret = 0;

	if (a > b) {
		ret = 1;
	} else {
		if (a < b)
			ret = -1;
	}

	return ret;
}

static inline int
sort_by_owner(const uid_t a, const uid_t b)
{
	int ret = 0;

	if (a > b) {
		ret = 1;
	} else {
		if (a < b)
			ret = -1;
	}

	return ret;
}

static inline int
sort_by_group(const gid_t a, const gid_t b)
{
	int ret = 0;

	if (a > b) {
		ret = 1;
	} else {
		if (a < b)
			ret = -1;
	}

	return ret;
}

static inline int
sort_by_blocks(const blkcnt_t a, const blkcnt_t b)
{
	int ret = 0;

	if (a > b) {
		ret = 1;
	} else {
		if (a < b)
			ret = -1;
	}

	return ret;
}

static inline int
sort_by_links(const nlink_t a, const nlink_t b)
{
	int ret = 0;

	if (a > b) {
		ret = 1;
	} else {
		if (a < b)
			ret = -1;
	}

	return ret;
}

static inline int
sort_dirs(const int a, const int b)
{
	if (b != a) {
		if (b)
			return 1;
		return (-1);
	}

	return 0;
}

int
entrycmp(const void *a, const void *b)
{
	struct fileinfo *pa = (struct fileinfo *)a;
	struct fileinfo *pb = (struct fileinfo *)b;
	int ret = 0;
	int st = conf.sort;

	if (conf.list_dirs_first == 1) {
		ret = sort_dirs(pa->dir, pb->dir);
		if (ret != 0)
			return ret;
	}

	if (conf.light_mode == 1 && !ST_IN_LIGHT_MODE(st))
		st = SNAME;

	switch (st) {
	case STSIZE: ret = sort_by_size(pa, pb); break;
	case SATIME: /* fallthrough */
	case SBTIME: /* fallthrough */
	case SCTIME: /* fallthrough */
	case SMTIME: ret = sort_by_time(pa->time, pb->time); break;
	case SVER: ret = xstrverscmp(pa->name, pb->name); break;
	case SEXT:
		ret = sort_by_extension(pa->name, pb->name, pa->dir, pb->dir); break;
	case SINO: ret = sort_by_inode(pa->inode, pb->inode); break;
	case SOWN: ret = sort_by_owner(pa->uid, pb->uid); break;
	case SGRP: ret = sort_by_group(pa->gid, pb->gid); break;
	case SBLK: ret = sort_by_blocks(pa->blocks, pb->blocks); break;
	case SLNK: ret = sort_by_links(pa->linkn, pb->linkn); break;
	default: break;
	}

	if (!ret)
		ret = namecmp(pa->name, pb->name);
	if (!conf.sort_reverse)
		return ret;

	return (-ret);
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

	if (!conf.sort_reverse)
		return ret;

	/* If sort_reverse, return the opposite value */
	return (-ret);
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

	if (!conf.sort_reverse)
		return ret;

	return (-ret);
}

static char *
num_to_sort_name(const int n)
{
	switch (n) {
	case SNONE:	 return "none";
	case SNAME:  return "name";
	case STSIZE: return "size";
	case SATIME: return "atime";
	case SBTIME: return "btime";
	case SCTIME: return "ctime";
	case SMTIME: return "mtime";
	case SVER:   return "version";
	case SEXT:   return "extension";
	case SINO:   return "inode";
	case SOWN:   return "owner";
	case SGRP:   return "group";
	case SBLK:   return "blocks";
	case SLNK:   return "links";
	default:     return "unknown";
	}
}

void
print_sort_method(void)
{
	char *name = num_to_sort_name(conf.sort);

	printf("%s%s%s%s", BOLD, name, NC,
		(conf.sort_reverse == 1) ? " [rev]" : "");

	if (conf.light_mode == 1 && !ST_IN_LIGHT_MODE(conf.sort))
		printf(_(" (not available in light mode: using %sname%s)\n"), BOLD, NC);
	else
		putchar('\n');
}

static inline void
toggle_sort_reverse(void)
{
	if (conf.sort_reverse)
		conf.sort_reverse = 0;
	else
		conf.sort_reverse = 1;
}

static inline int
re_sort_files_list(void)
{
	if (conf.autols == 0)
		return FUNC_SUCCESS;

	/* sort_switch just tells list_dir() to print a line with the current
	 * sorting order at the end of the files list. */
	sort_switch = 1;
	free_dirlist();
	const int ret = list_dir();
	sort_switch = 0;

	return ret;
}

/* If ARG is a string, write the corresponding integer to ARG itself.
 * Return zero if ARG corresponds to a valid sorting method or one
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

			*arg = xnrealloc(*arg, 32, sizeof(char));
			snprintf(*arg, 32, "%d", sort_methods[i].num);
			return FUNC_SUCCESS;
		}
	}

	fprintf(stdout, _("st: %s: No such sorting order\n"), *arg);
	return FUNC_FAILURE;
}

int
sort_function(char **arg)
{
	/* No argument: Just print current sorting order */
	if (!arg[1]) {
		fputs(_("Sorting order: "), stdout);
		print_sort_method();
		return FUNC_SUCCESS;
	}

	/* Argument is alphanumerical string */
	if (!is_number(arg[1])) {
		if (*arg[1] == 'r' && strcmp(arg[1], "rev") == 0) {
			toggle_sort_reverse();
			return re_sort_files_list();
		}

		if (set_sort_by_name(&arg[1]) == FUNC_FAILURE)
			return FUNC_FAILURE;
	}

	/* Argument is a number */
	const int n = atoi(arg[1]);

	if (conf.light_mode == 1 && !ST_IN_LIGHT_MODE(n)) {
		fprintf(stderr, _("st: %d (%s): Not available in light mode\n"),
			n, num_to_sort_name(n));
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
			toggle_sort_reverse();

		return re_sort_files_list();
	}

	/* If arg1 is a number but is not in the range 0-SORT_TYPES, error */
	fprintf(stderr, "%s\n", _(SORT_USAGE));
	return FUNC_FAILURE;
}
