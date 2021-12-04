/* sort.c -- functions used to sort files */

/*
 * This file is part of CliFM
 * 
 * Copyright (C) 2016-2021, L. Abramovich <johndoe.arch@outlook.com>
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

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#ifdef __OpenBSD__
#include <strings.h>
#endif
//#include <sys/stat.h>

#include "checks.h"
//#include "aux.h" /* For xatoi */
#include "listing.h"
//#include "messages.h"

int
skip_nonexec(const struct dirent *ent)
{
	if (access(ent->d_name, X_OK) == -1)
		return 0;
	return 1;

/*	int f = 0; // Hold file ownership flags

	struct stat a;
	if (stat(ent->d_name, &a) == -1)
		return 0;

	mode_t val = (a.st_mode & (mode_t)~S_IFMT);
	if (val & S_IXUSR) f |= X_USR;
	if (val & S_IXGRP) f |= X_GRP;
	if (val & S_IXOTH) f |= X_OTH;

	if ((f & X_USR) && a.st_uid == user.uid)
		return 1;
	if ((f & X_GRP) && a.st_gid == user.gid)
		return 1;
	if (f & X_OTH)
		return 1;

	return 0; */
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

	/* Skip "." and ".." */
	if (*ent->d_name == '.' && (!ent->d_name[1] || (ent->d_name[1] == '.' && !ent->d_name[2])))
		return 0;

	/* Skip files matching FILTER */
	if (_filter && regexec(&regex_exp, ent->d_name, 0, NULL, 0) == EXIT_SUCCESS)
		return 0;

	/* If not hidden files */
	if (!show_hidden && *ent->d_name == '.')
		return 0;

	return 1;
}

int
namecmp(const char *s1, const char *s2)
{
	/* Do not take initial dot into account */
	if (*s1 == '.')
		s1++;

	if (*s2 == '.')
		s2++;

	/* If both string starts with number, sort them as numbers, not as strings */
	if (_ISDIGIT(*s1) && _ISDIGIT(*s2)) {
		char *p1, *p2;
		long long n1 = strtoll(s1, &p1, 10);
		long long n2 = strtoll(s2, &p2, 10);
		if (n2 > n1)
			return -1;
		if (n2 < n1)
			return 1;
	}

	char ac = *s1, bc = *s2;

	if ((*s1 & 0xc0) != 0xc0 && (*s1 & 0xc0) != 0xc0) {
	/* None of the strings starts with a unicode char: compare the first
	 * byte of both strings */
		if (!case_sensitive) {
			ac = (char)TOUPPER(*s1);
			bc = (char)TOUPPER(*s2);
		}

		if (bc > ac)
			return -1;

		if (bc < ac)
			return 1;
	}

	if (!case_sensitive || (*s1 & 0xc0) == 0xc0 || (*s1 & 0xc0) == 0xc0)
		return strcoll(s1, s2);

	return strcmp(s1, s2);
}

int
entrycmp(const void *a, const void *b)
{
	const struct fileinfo *pa = (struct fileinfo *)a;
	const struct fileinfo *pb = (struct fileinfo *)b;

	if (list_folders_first) {
		if (pb->dir != pa->dir) {
			if (pb->dir)
				return 1;

			return -1;
		}
	}

	int ret = 0, st = sort;

#ifndef _GNU_SOURCE
	if (st == SVER)
		st = SNAME;
#endif

	if (light_mode && (st == SOWN || st == SGRP))
		st = SNAME;

	switch (st) {

	case SSIZE:
		if (pa->size > pb->size)
			ret = 1;
		else if (pa->size < pb->size)
			ret = -1;
		break;

	case SATIME: /* fallthrough */
	case SBTIME: /* fallthrough */
	case SCTIME: /* fallthrough */
	case SMTIME:
		if (pa->time > pb->time)
			ret = 1;
		else if (pa->time < pb->time)
			ret = -1;
		break;

#ifdef _GNU_SOURCE
	case SVER:
		ret = strverscmp(pa->name, pb->name);
		break;
#endif

	case SEXT: {
		char *aext = (char *)NULL, *bext = (char *)NULL, *val;
		val = strrchr(pa->name, '.');
		if (val && val != pa->name)
			aext = val + 1;

		val = strrchr(pb->name, '.');
		if (val && val != pb->name)
			bext = val + 1;

		if (aext || bext) {
			if (!aext)
				ret = -1;
			else if (!bext)
				ret = 1;

			else
				ret = strcasecmp(aext, bext);
		}
	} break;

	case SINO:
		if (pa->inode > pb->inode)
			ret = 1;
		else if (pa->inode < pb->inode)
			ret = -1;
		break;

	case SOWN:
		if (pa->uid > pb->uid)
			ret = 1;
		else if (pa->uid < pb->uid)
			ret = -1;
		break;

	case SGRP:
		if (pa->gid > pb->gid)
			ret = 1;
		else if (pa->gid < pb->gid)
			ret = -1;
		break;
	}

	if (!ret)
		ret = namecmp(pa->name, pb->name);

	if (!sort_reverse)
		return ret;

	return (ret - (ret * 2));
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

	if (!sort_reverse)
		return ret;

	/* If sort_reverse, return the opposite value */
	return (ret - (ret * 2));
}

void
print_sort_method(void)
{
	switch (sort) {
	case SNONE:	puts(_("none")); break;

	case SNAME:
		printf(_("name %s\n"), (sort_reverse) ? "[rev]" : "");
		break;

	case SSIZE:
		printf(_("size %s\n"), (sort_reverse) ? "[rev]" : "");
		break;

	case SATIME:
		printf(_("atime %s\n"), (sort_reverse) ? "[rev]" : "");
		break;

	case SBTIME:
#if defined(HAVE_ST_BIRTHTIME) || defined(__BSD_VISIBLE) || defined(_STATX)
		printf(_("btime %s\n"), (sort_reverse) ? "[rev]" : "");
#else
		printf(_("btime (not available: using 'ctime') %s\n"),
		    (sort_reverse) ? "[rev]" : "");
#endif
		break;

	case SCTIME:
		printf(_("ctime %s\n"), (sort_reverse) ? "[rev]" : "");
		break;

	case SMTIME:
		printf(_("mtime %s\n"), (sort_reverse) ? "[rev]" : "");
		break;

	case SVER:
#if __FreeBSD__ || __NetBSD__ || __OpenBSD__ || _BE_POSIX
		printf(_("version (not available: using 'name') %s\n"),
		    (sort_reverse) ? "[rev]" : "");
#else
		printf(_("version %s\n"), (sort_reverse) ? "[rev]" : "");
#endif
		break;

	case SEXT:
		printf(_("extension %s\n"), (sort_reverse) ? "[rev]" : "");
		break;

	case SINO:
		printf(_("inode %s\n"), (sort_reverse) ? "[rev]" : "");
		break;

	case SOWN:
		if (light_mode) {
			printf(_("owner (not available: using 'name') %s\n"),
			    (sort_reverse) ? "[rev]" : "");
		} else {
			printf(_("owner %s\n"), (sort_reverse) ? "[rev]" : "");
		}
		break;

	case SGRP:
		if (light_mode) {
			printf(_("group (not available: using 'name') %s\n"),
			    (sort_reverse) ? "[rev]" : "");
		} else {
			printf(_("group %s\n"), (sort_reverse) ? "[rev]" : "");
		}
		break;

	default: fputs("unknown sorting method\n", stdout);
	}
}

int
sort_function(char **arg)
{
	int exit_status = EXIT_FAILURE;

	/* No argument: Just print current sorting method */
	if (!arg[1]) {
		fputs(_("Sorting method: "), stdout);
		print_sort_method();
		return EXIT_SUCCESS;
	}

	/* Argument is alphanumerical string */
	if (!is_number(arg[1])) {
		struct sort_t {
			const char *name;
			int num;
			int padding; /* Used only to properly align the struct */
		};

		static struct sort_t sorts[] = {
		    {"none", 0, 0},
		    {"name", 1, 0},
		    {"size", 2, 0},
		    {"atime", 3, 0},
		    {"btime", 4, 0},
		    {"ctime", 5, 0},
		    {"mtime", 6, 0},
		    {"version", 7, 0},
		    {"extension", 8, 0},
		    {"inode", 9, 0},
		    {"owner", 10, 0},
		    {"group", 11, 0},
		};

		size_t i;
		for (i = 0; i < sizeof(sorts) / sizeof(struct sort_t); i++) {
			if (strcmp(arg[1], sorts[i].name) == 0) {
				sprintf(arg[1], "%d", sorts[i].num);
				break;
			}
		}

		if (strcmp(arg[1], "rev") == 0) {
			if (sort_reverse)
				sort_reverse = 0;
			else
				sort_reverse = 1;

			if (autols) {
				/* sort_switch just tells list_dir() to print a line
				 * with the current sorting method at the end of the
				 * files list */
				sort_switch = 1;
				free_dirlist();
				exit_status = list_dir();
				sort_switch = 0;
			}

			return exit_status;
		}

		/* If arg1 is not a number and is not "rev", the fputs()
		 * above is executed */
	}

	/* Argument is a number */
	int int_arg = atoi(arg[1]);

	if (int_arg >= 0 && int_arg <= SORT_TYPES) {
		sort = int_arg;

		if (arg[2] && strcmp(arg[2], "rev") == 0) {
			if (sort_reverse)
				sort_reverse = 0;
			else
				sort_reverse = 1;
		}

		if (autols) {
			sort_switch = 1;
			free_dirlist();
			exit_status = list_dir();
			sort_switch = 0;
		}

		return exit_status;
	}

	/* If arg1 is a number but is not in the range 0-SORT_TYPES,
	 * error */
	fprintf(stderr, "%s\n", _(SORT_USAGE));
	return EXIT_FAILURE;
}

/* This is a modification of the alphasort function that makes it case
 * insensitive. It also sorts without taking the initial dot of hidden
 * files into account. Note that strcasecmp() isn't locale aware. Use
 * only with C and english locales */
int
alphasort_insensitive(const struct dirent **a, const struct dirent **b)
{
	int ret = strcasecmp(((*a)->d_name[0] == '.') ? (*a)->d_name + 1
	: (*a)->d_name, ((*b)->d_name[0] == '.') ? (*b)->d_name + 1 : (*b)->d_name);

	if (!sort_reverse)
		return ret;

	return (ret - (ret * 2));
}
