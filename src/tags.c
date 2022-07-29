/* tags.c -- functions to handle the files tag system */

/*
 * This file is part of CliFM
 * 
 * Copyright (C) 2016-2022, L. Abramovich <johndoe.arch@outlook.com>
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

#ifndef _NO_TAGS

#include "helpers.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <readline/readline.h>

#include "aux.h"
#include "misc.h"
#include "exec.h"
#include "strings.h"
#include "sort.h"

/* A few printing functions */
static int
print_tag_creation_error(const char *link, mode_t mode)
{
	if (S_ISLNK(mode)) {
		fprintf(stderr, _("tag: %s: File already tagged\n"), link);
	} else {
		fprintf(stderr, _("tag: %s: Cannot create tag: file already exists\n"), link);
	}

	return EXIT_FAILURE;
}

static int
print_symlink_error(const char *name)
{
	_err(ERR_NO_STORE, NOPRINT_PROMPT, "tag: %s: %s\n", name, strerror(errno));
	return errno;
}

static int
print_no_tags(void)
{
	printf(_("%s: No tags found, Use 'tag new' to create new tags\n"), PROGRAM_NAME);
	return EXIT_SUCCESS;
}

static int
print_no_such_tag(const char *name)
{
	fprintf(stderr, _("tag: %s: No such tag\n"), name);
	return EXIT_FAILURE;
}

static int
print_usage(int retval)
{
	puts(TAG_USAGE);
	return retval;
}

#ifndef _DIRENT_HAVE_D_TPYE
/* Check whether NAME is actually tagged as TAG
 * Returns 1 if true and zero otherwise */
static int
check_tagged_file(const char *tag, const char *name)
{
	struct stat a;
	char tmp[PATH_MAX];
	snprintf(tmp, PATH_MAX, "%s/%s/%s", tags_dir, tag, name);

	if (SELFORPARENT(name) || lstat(tmp, &a) == -1 || !S_ISLNK(a.st_mode))
		return 0;

	return 1;
}
#endif /* _DIRENT_HAVE_D_TPYE */

/* Print the tagged file NAME tagged as TAG */
static void
print_tagged_file(char *name, const char *tag)
{
	char dir[PATH_MAX], tmp[PATH_MAX];
	*tmp = '\0';
	snprintf(dir, PATH_MAX, "%s/%s/%s", tags_dir, tag, name);
	char *ret = realpath(dir, tmp);
	if (!ret)
		return;

	if (!*tmp) {
		printf(_("%s (error resolving link target)\n"), name);
		return;
	}

	int free_name = 0;
	char *q = tmp;
	if (strncmp(tmp, user.home, strlen(user.home)) == 0)
		q = home_tilde(tmp, &free_name);

	if (strchr(name, '\\')) {
		char *d = dequote_str(name, 0);
		if (d) {
			strcpy(name, d);
			free(d);
		}
	}

	char *p = strrchr(tmp, '/');
	if (p && p != q)
		*p = '\0';
	printf("%s%s%s [%s]\n", mi_c, (p && *(p + 1)) ? p + 1 : tmp, df_c, tmp);

	if (free_name == 1)
		free(q);
}

/* Print the list of all files tagged as NAME */
static int
list_files_in_tag(char *name)
{
	if (strchr(name, '\\')) {
		char *p = dequote_str(name, 0);
		if (p) {
			strcpy(name, p);
			free(p);
		}
	}
	char tmp[PATH_MAX];
	snprintf(tmp, PATH_MAX, "%s/%s", tags_dir, name);

	struct dirent **t = (struct dirent **)NULL;
	int n = scandir(tmp, &t, NULL, case_sensitive ? xalphasort : alphasort_insensitive);
	if (n == -1) {
		fprintf(stderr, _("tag: %s: No such tag\n"), name);
		return EXIT_FAILURE;
	}

	size_t i;
	if (n <= 2) {
		for (i = 0; i < (size_t)n; i++)
			free(t[i]);
		free(t);
		printf(_("No file tagged as '%s'\n"), name);
		return EXIT_SUCCESS;
	}

	for (i = 0; i < (size_t)n; i++) {
#ifndef _DIRENT_HAVE_D_TPYE
		if (check_tagged_file(name, t[i]->d_name) == 0) {
#else
		if (SELFORPARENT(t[i]->d_name) || t[i]->d_type != DT_LNK) {
#endif
			free(t[i]);
			continue;
		}
		print_tagged_file(t[i]->d_name, name);
		free(t[i]);
	}
	free(t);

	return EXIT_SUCCESS;
}

/* Return the longest tag name
 * Used to pad the output of 'tag ls' command */
static size_t
get_longest_tag(void)
{
	size_t i, _longest = 0;

	for (i = 0; i < tags_n; i++) {
		size_t l = strlen(tags[i]);
		if (l > (size_t)_longest)
			_longest = l;
	}

	return _longest;
}

/* Print the list of available tags, plus the amount of tagged files for
 * each tag */
static int
list_tags(char **args)
{
	if (tags_n == 0)
		return print_no_tags();

	if (args[1] && args[2]) {
		int exit_status = EXIT_SUCCESS;
		size_t i;
		for (i = 2; args[i]; i++) {
			if (args[3])
				printf("%s:\n", args[i]);
			if (list_files_in_tag(args[i]) == EXIT_FAILURE)
				exit_status = EXIT_FAILURE;
		}
		return exit_status;
	}

	int pad = (int)get_longest_tag();

	size_t i;
	for (i = 0; tags[i]; i++) {
		char p[PATH_MAX];
		snprintf(p, PATH_MAX, "%s/%s", tags_dir, tags[i]);
		int n = count_dir(p, NO_CPOP);
		if (n > 2)
			printf("%-*s [%s%d%s]\n", pad, tags[i], mi_c, n - 2, df_c);
		else
			printf("%-*s  -\n", pad, tags[i]);
	}

	return EXIT_SUCCESS;
}

static inline void
reload_tags(void)
{
	free_tags();
	load_tags();
}

/* Create tags according to ARGS + 2 */
static int
create_tags(char **args)
{
	if (!args || !args[1] || !args[2])
		return print_usage(EXIT_FAILURE);

	size_t i;
	int exit_status = EXIT_SUCCESS;

	for (i = 2; args[i]; i++) {
		char *p = strchr(args[i], '\\'), dir[PATH_MAX];
		if (p) {
			char *deq = dequote_str(args[i], 0);
			if (deq) {
				free(args[i]);
				args[i] = deq;
			}
		}

		snprintf(dir, PATH_MAX, "%s/%s", tags_dir, args[i]);

		struct stat a;
		if (lstat(dir, &a) != -1) {
			fprintf(stderr, _("tag: %s: Tag already exists\n"), args[i]);
			exit_status = EXIT_FAILURE;
			continue;
		}

		if (xmkdir(dir, S_IRWXU) == EXIT_FAILURE) {
			_err(ERR_NO_STORE, NOPRINT_PROMPT, _("tag: %s: Error creating tag: %s\n"),
				args[i], strerror(errno));
			exit_status = EXIT_FAILURE;
			continue;
		}

		printf(_("%s: Successfully created tag\n"), args[i]);
	}

	reload_tags();
	return exit_status;
}

/* Remove those tags specified in ARGS + 2 */
static int
remove_tags(char **args)
{
	if (tags_n == 0)
		return print_no_tags();

	int exit_status = EXIT_SUCCESS;
	size_t i;
	for (i = 2; args[i]; i++) {
		char *p = strchr(args[i], '\\');
		if (p) {
			char *deq = dequote_str(args[i], 0);
			if (deq) {
				free(args[i]);
				args[i] = deq;
			}
		}

		char dir[PATH_MAX];
		snprintf(dir, PATH_MAX, "%s/%s", tags_dir, args[i]);

		struct stat a;
		if (stat(dir, &a) == -1 || !S_ISDIR(a.st_mode))
			return print_no_such_tag(args[i]);

		char *cmd[] = {"rm", "-r", "--", dir, NULL};
		if (launch_execve(cmd, FOREGROUND, E_NOFLAG) == EXIT_SUCCESS) {
			printf(_("%s: %s: Successfully removed tag\n"), PROGRAM_NAME, args[i]);
			reload_tags();
		} else {
			exit_status = EXIT_FAILURE;
		}
	}

	return exit_status;
}

/* Check whether NAME is a valid and existent tag name
 * Returns 1 if true or zero otherwise */
int
is_tag(char *name)
{
	if (!name || !*name)
		return 0;

	if (strchr(name, '\\')) {
		char *deq = dequote_str(name, 0);
		if (deq) {
			strcpy(name, deq);
			free(deq);
		}
	}

	int i = (int)tags_n;
	while(--i >= 0) {
		if (*name == *tags[i] && strcmp(name, tags[i]) == 0)
			return 1;
	}

	return 0;
}

/* Tag the file named NAME as TAG */
static int
tag_file(char *name, char *tag)
{
	int new_tag = 0;
	char *p = (char *)NULL;
	if (strchr(tag, '\\'))
		p = dequote_str(tag, 0);
	char dir[PATH_MAX];
	snprintf(dir, PATH_MAX, "%s/%s", tags_dir, p ? p : tag);

	struct stat a;
	if (stat(dir, &a) == -1) {
		if (xmkdir(dir, S_IRWXU) != EXIT_SUCCESS) {
			_err(ERR_NO_STORE, NOPRINT_PROMPT, _("tag: %s: Cannot create tag: %s\n"),
				p ? p : tag, strerror(errno));
			free(p);
			return EXIT_FAILURE;
		}
		new_tag = 1;
	}
	free(p);

	if (new_tag) {
		printf(_("Created new tag %s%s%s\n"), colorize ? BOLD : "", tag, df_c);
		reload_tags();
	}

	char name_path[PATH_MAX];
	*name_path = '\0';
	if (*name != '/')
		snprintf(name_path, PATH_MAX, "%s/%s", workspaces[cur_ws].path, name);

	char link[PATH_MAX + NAME_MAX], *q = (char *)NULL;
	char *link_path = replace_slashes(*name_path ? name_path : name, ':');

//	if (*name == '/')
//		q = strrchr(name, '/');
//	snprintf(link, sizeof(link), "%s/%s", dir, (q && *(++q)) ? q : link_path);
	snprintf(link, sizeof(link), "%s/%s", dir, link_path);
	free(link_path);

	if (lstat(link, &a) != -1)
		return print_tag_creation_error((q && *(++q)) ? q : name, a.st_mode);

	if (symlink(*name_path ? name_path : name, link) == -1)
		return print_symlink_error(name);

	return EXIT_SUCCESS;
}

/* Return an array with indices of tag names (:TAG) found in the ARGS array
 * The indices array is terminated by a -1
 * Returns NULL if no index was found */
static int *
get_tags(char **args)
{
	int n, i, c = 0;

	for (n = 0; args[n]; n++);
	int *t = (int *)xnmalloc((size_t)n + 1, sizeof(int));

	for (i = 0; i < n; i++) {
		if (*args[i] == ':' && *(args[i] + 1)) {
			t[c] = i;
			c++;
		}
	}

	if (c == 0) {
		free(t);
		t = (int *)NULL;
	} else {
		t[c] = -1;
	}

	return t;
}

/* Tag file names found in ARGS as all tags (:TAG) found in ARGS */
static int
tag_files(char **args)
{
	int *tag_names = get_tags(args);
	if (!tag_names || tag_names[0] == -1) {
		free(tag_names);
		fprintf(stderr, _("tag: No tag specified. Specify a tag via :TAG. "
			"E.g. tag FILE1 FILE2 :TAG\n"));
		return EXIT_FAILURE;
	}

	size_t i, j, n = 0;
	for (i = 1; args[i]; i++) {
		if (*args[i] != ':')
			n++;
	}

	for (i = 0; tag_names[i] != -1; i++) {
		for (j = 1; args[j]; j++) {
			if (*args[j] == ':')
				continue;

			char *p = (char *)NULL;
			if (strchr(args[j], '\\'))
				p = dequote_str(args[j], 0);

			if (tag_file(p ? p : args[j], args[tag_names[i]] + 1) != EXIT_SUCCESS)
				if (n > 0) --n;
			free(p);
		}
	}

	free(tag_names);

	if (n > 0)
		printf(_("Successfully tagged %zu file(s)\n"), n);
	return EXIT_SUCCESS;
}

/* Untag file names found in ARGS tagged as ARGS[N] */
static int
untag(char **args, const size_t n, size_t *t)
{
	if (!args || !args[1])
		return EXIT_FAILURE;

	int exit_status = EXIT_SUCCESS;
	size_t i;
	for (i = 2; args[i]; i++) {
		if (i == n || (*args[i] == ':' && *(args[1] + 1)))
			continue;

		char dir[PATH_MAX];
		snprintf(dir, PATH_MAX, "%s/%s", tags_dir, args[n] + 1);

		struct stat a;
		if (lstat(dir, &a) == -1 || !S_ISDIR(a.st_mode))
			return print_no_such_tag(args[n] + 1);

		char f[PATH_MAX + NAME_MAX];
		char *deq = dequote_str(args[i], 0);
		char *p = deq ? deq : args[i];
		char *exp = (char *)NULL;
		if (*p == '~')
			exp = tilde_expand(p);
		char *q = exp ? exp : p;
		char *r = replace_slashes(q, ':');

		snprintf(f, PATH_MAX + NAME_MAX, "%s/%s", dir, r ? r : q);
		free(deq);
		free(exp);
		free(r);

		if (lstat(f, &a) != -1 && S_ISLNK(a.st_mode)) {
			errno = 0;
			if (unlinkat(AT_FDCWD, f, 0) == -1) {
				exit_status = errno;
				_err(ERR_NO_STORE, NOPRINT_PROMPT, "tag: %s: %s\n", args[i], strerror(errno));
			} else {
				(*t)++;
			}
		} else {
			fprintf(stderr, _("tag: %s: File not tagged as %s%s%s\n"),
				args[i], colorize ? BOLD : "", args[n] + 1, df_c);
			continue;
		}
	}

	return exit_status;
}

/* Untag file names found in ARGS as all tags (:TAG) found in ARGS */
static int
untag_files(char **args)
{
	int exit_status = EXIT_SUCCESS;
	size_t i, n = 0;

	for (i = 1; args[i]; i++) {
		if (*args[i] == ':' && *(args[i] + 1) && untag(args, i, &n) == EXIT_FAILURE)
			exit_status = EXIT_FAILURE;
	}

	if (n > 0)
		printf(_("Successfully untagged %zu file(s)\n"), n);

	return exit_status;
}

/* Rename tag ARGS[2] as ARGS[3] */
static int
rename_tag(char **args)
{
	if (!args || !args[1] || !args[2] || !args[3])
		return print_usage(EXIT_FAILURE);

	char *old = args[2], *new = args[3];
	if (!is_tag(old))
		return print_no_such_tag(old);

	if (*old == *new && strcmp(old, new) == 0) {
		fprintf(stderr, "tag: New and old file names are the same\n");
		return EXIT_FAILURE;
	}

	char old_dir[PATH_MAX];
	snprintf(old_dir, PATH_MAX, "%s/%s", tags_dir, old);

	char new_dir[PATH_MAX];
	snprintf(new_dir, PATH_MAX, "%s/%s", tags_dir, new);

	if (rename(old_dir, new_dir) == -1) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "tag: %s\n", strerror(errno));
		return errno;
	}

	puts("Successfully renamed tag");
	reload_tags();
	return EXIT_SUCCESS;
}

/* Move all (tagged) files (symlinks) in SRC into DST
 * Returns zero if success or the appropriate ERRNO in case of error */
static int
recursive_mv_tags(const char *src, const char *dst)
{
	int i, n, exit_status = EXIT_SUCCESS, ret;
	char src_dir[PATH_MAX], dst_dir[PATH_MAX];
	struct dirent **a = (struct dirent **)NULL;

	snprintf(src_dir, PATH_MAX, "%s/%s", tags_dir, src);

	n = scandir(src_dir, &a, NULL, alphasort);
	if (n == -1) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "tag: %s: %s\n", src_dir, strerror(errno));
		return errno;
	}

	snprintf(dst_dir, PATH_MAX, "%s/%s", tags_dir, dst);

	for (i = 0; i < n; i++) {
		if (SELFORPARENT(a[i]->d_name)) {
			free(a[i]);
			continue;
		}
		char src_file[PATH_MAX + NAME_MAX + 2];
		snprintf(src_file, sizeof(src_file), "%s/%s", src_dir, a[i]->d_name);
		char *cmd[] = {"mv", src_file, dst_dir, NULL};
		if ((ret = launch_execve(cmd, FOREGROUND, E_NOFLAG)) != EXIT_SUCCESS)
			exit_status = ret;
		free(a[i]);
	}

	free(a);
	return exit_status;
}

static int
merge_tags(char **args)
{
	if (!args || !args[1] || !args[2] || !args[3])
		return print_usage(EXIT_FAILURE);

	if (!is_tag(args[2]))
		return print_no_such_tag(args[2]);
	if (!is_tag(args[3]))
		return print_no_such_tag(args[3]);

	char *src = args[2], *dst = args[3];

	errno = 0;
	int exit_status = recursive_mv_tags(src, dst);
	if (exit_status != EXIT_SUCCESS) {
		fprintf(stderr, _("tag: Cannot merge tags: error moving tagged files\n"));
		return exit_status;
	}

	char src_dir[PATH_MAX];
	snprintf(src_dir, PATH_MAX, "%s/%s", tags_dir, src);
	if (rmdir(src_dir) == -1) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "tag: %s: %s\n", src_dir, strerror(errno));
		return errno;
	}

	reload_tags();
	printf(_("Successfully merged %s%s%s into %s%s%s\n"),
		colorize ? BOLD : "", src, df_c,
		colorize ? BOLD : "", dst, df_c);

	return EXIT_SUCCESS;
}

/* Perform the following expansions:
 * ta -> tag
 * td -> tag rm
 * tl -> tag list
 * tm -> tag rename (mv)
 * tn -> tag new
 * tu -> tag untag
 * ty -> tag merge
 * The first string in ARGS must always be one of the left values
 * Returns an array with the expanded values */
static char **
reconstruct_input(char **args)
{
	size_t n = 0, c;
	for(n = 0; args[n]; n++);
	char **a = (char **)xnmalloc(n + 2, sizeof(char *));

	a[0] = savestring("tag", 3);

	switch(args[0][1]) {
	case 'a': c = 1; break;
	case 'd': a[1] = savestring("rm", 2); c = 2; break;
	case 'l': a[1] = savestring("ls", 2); c = 2; break;
	case 'm': a[1] = savestring("mv", 2); c = 2; break;
	case 'n': a[1] = savestring("new", 3); c = 2; break;
	case 'u': a[1] = savestring("untag", 5); c = 2; break;
	case 'y': a[1] = savestring("merge", 5); c = 2; break;
	default: a[1] = savestring("-h", 2); c = 2; break;
	}

	for (n = 1; args[n]; n++) {
		a[c] = savestring(args[n], strlen(args[n]));
		c++;
	}
	a[c] = (char *)NULL;

	return a;
}

static int
end_tag_function(const int exit_status, char **a, const int free_args)
{
	if (free_args == 0)
		return exit_status;

	size_t i;
	for (i = 0; a[i]; i++)
		free(a[i]);
	free(a);

	return exit_status;
}

int
tags_function(char **args)
{
	int exit_status = EXIT_SUCCESS, free_args = 0;
	char **a = args;

	if ((a[1] && IS_HELP(a[1]))	|| (a[1] && a[2] && IS_HELP(a[2])))
		{ puts(_(TAG_USAGE)); goto END; }

	char b[] = "adlmnuy";
	if (strspn(a[0] + 1, b))
		{ a = reconstruct_input(args); free_args = 1; }

	if (!a[1] || (*a[1] == 'l' && (strcmp(a[1], "ls") == 0
	|| strcmp(a[1], "list") == 0)))
		{ exit_status = list_tags(a); goto END; }

	if (*a[1] == 'm' && strcmp(a[1], "merge") == 0)
		{ exit_status = merge_tags(a); goto END; }

	if (*a[1] == 'n' && strcmp(a[1], "new") == 0)
		{ exit_status = create_tags(a); goto END; }

	if ((*a[1] == 'r' && (strcmp(a[1], "rm") == 0 || strcmp(a[1], "remove") == 0))
	|| (*a[1] == 'd' && strcmp(a[1], "del") == 0))
		{ exit_status = remove_tags(a); goto END; }

	if ((*a[1] == 'r' && strcmp(a[1], "rename") == 0)
	|| (*a[1] == 'm' && strcmp(a[1], "mv") == 0))
		{ exit_status = rename_tag(a); goto END; }

	if (*a[1] == 'u' && strcmp(a[1], "untag") == 0)
		{ exit_status = untag_files(a); goto END; }

	exit_status = tag_files(a);

END:
	return end_tag_function(exit_status, a, free_args);
}

#else
void *__skip_me_tags;
#endif /* !_NO_TAGS */
