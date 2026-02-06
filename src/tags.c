/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* tags.c -- functions to handle the file tagging system */

#ifndef _NO_TAGS

#include "helpers.h"

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <readline/tilde.h>

#include "aux.h"
#include "colors.h" /* colors_list() */
#include "init.h"
#include "messages.h"
#include "misc.h"
#include "sort.h"
#include "spawn.h"

/* A few printing functions */
static int
print_tag_creation_error(const char *link, const mode_t mode)
{
	if (S_ISLNK(mode))
		xerror(_("tag: '%s': File already tagged\n"), link);
	else
		xerror(_("tag: '%s': Cannot create tag: file already exists\n"), link);

	return FUNC_FAILURE;
}

static int
print_symlink_error(const char *name)
{
	xerror("tag: '%s': %s\n", name, strerror(errno));
	return errno;
}

static int
print_no_tags(void)
{
	printf(_("%s: No tags found. Use 'tag new' to create new "
		"tags.\n"), PROGRAM_NAME);
	return FUNC_SUCCESS;
}

static int
print_no_such_tag(const char *name)
{
	xerror(_("tag: '%s': No such tag\n"), name);
	return FUNC_FAILURE;
}

static int
print_usage(const int retval)
{
	puts(TAG_USAGE);
	return retval;
}

#ifndef _DIRENT_HAVE_D_TPYE
/* Check whether NAME is actually tagged as TAG.
 * Returns 1 if true or 0 otherwise. */
static int
check_tagged_file(const char *tag, const char *name)
{
	struct stat a;
	char tmp[PATH_MAX + 1];
	snprintf(tmp, sizeof(tmp), "%s/%s/%s", tags_dir, tag, name);

	if (SELFORPARENT(name) || lstat(tmp, &a) == -1 || !S_ISLNK(a.st_mode))
		return 0;

	return 1;
}
#endif /* _DIRENT_HAVE_D_TPYE */

/* Print the tagged file named NAME tagged as TAG. */
static void
print_tagged_file(char *name, const char *tag)
{
	char dir[PATH_MAX + 1];
	char tmp[PATH_MAX + 1];
	*tmp = '\0';

	snprintf(dir, sizeof(dir), "%s/%s/%s", tags_dir, tag, name);
	char *ret = xrealpath(dir, tmp);
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
		char *d = unescape_str(name);
		if (d) {
			xstrsncpy(name, d, strlen(d) + 1);
			free(d);
		}
	}

	putchar(' ');
	char *abbr_name = abbreviate_file_name(tmp);
	colors_list(abbr_name ? abbr_name : tmp, 0, 0, 1);
	if (abbr_name != tmp)
		free(abbr_name);

	if (free_name == 1)
		free(q);
}

/* Print the list of all files tagged as NAME. */
static int
list_files_in_tag(char *name)
{
	if (strchr(name, '\\')) {
		char *p = unescape_str(name);
		if (p) {
			xstrsncpy(name, p, strlen(p) + 1);
			free(p);
		}
	}

	char tmp[PATH_MAX + 1];
	snprintf(tmp, sizeof(tmp), "%s/%s", tags_dir, name);

	struct dirent **t = NULL;
	const int n = scandir(tmp, &t, NULL, conf.case_sens_list
		? xalphasort : alphasort_insensitive);
	if (n == -1) {
		xerror("tag: '%s': %s\n", tmp, strerror(errno));
		return errno;
	}

	size_t i;
	if (n <= 2) {
		for (i = 0; i < (size_t)n; i++)
			free(t[i]);
		free(t);
		return FUNC_SUCCESS;
	}

	for (i = 0; i < (size_t)n; i++) {
#ifndef _DIRENT_HAVE_D_TPYE
		if (check_tagged_file(name, t[i]->d_name) == 0) {
#else
		if (SELFORPARENT(t[i]->d_name) || t[i]->d_type != DT_LNK) {
#endif /* !_DIRENT_HAVE_D_TYPE */
			free(t[i]);
			continue;
		}
		print_tagged_file(t[i]->d_name, name);
		free(t[i]);
	}
	free(t);

	return FUNC_SUCCESS;
}

/* Return the length of longest tag name.
 * Used to pad the output of the 'tag list' command. */
static size_t
get_longest_tag(void)
{
	size_t longest_tag = 0;

	for (size_t i = 0; i < tags_n; i++) {
		const size_t l = strlen(tags[i]);
		if (l > longest_tag)
			longest_tag = l;
	}

	return longest_tag;
}

/* List all tags applied to the file whose device ID is DEV and inode number
 * is INO. */
static void
list_tags_having_file(const dev_t dev, const ino_t ino)
{
	if (!tags_dir || !tags)
		return;

	for (size_t i = 0; tags[i]; i++) {
		char tmp[PATH_MAX + 1];
		snprintf(tmp, sizeof(tmp), "%s/%s", tags_dir, tags[i]);

		DIR *dir = opendir(tmp);
		if (dir == NULL)
			continue;

		const struct dirent *ent;
		while ((ent = readdir(dir))) {
			char full_name[PATH_MAX + NAME_MAX + 2];
			snprintf(full_name, sizeof(full_name), "%s/%s", tmp, ent->d_name);

			struct stat a;
			if (stat(full_name, &a) == -1)
				continue;

			if (a.st_dev == dev && a.st_ino == ino) {
				printf(" %s%s%s\n", mi_c, tags[i], NC);
				break;
			}
		}

		closedir(dir);
	}
}

/* Check whether NAME is a valid and existent tag name.
 * Returns 1 if true or zero otherwise. */
int
is_tag(char *name)
{
	if (!name || !*name)
		return 0;

	if (strchr(name, '\\')) {
		char *deq = unescape_str(name);
		if (deq) {
			xstrsncpy(name, deq, strlen(deq) + 1);
			free(deq);
		}
	}

	for (size_t i = tags_n; i-- > 0;) {
		if (*name == *tags[i] && strcmp(name, tags[i]) == 0)
			return 1;
	}

	return 0;
}

/* Print the list of available tags and all files tagged as each tag. */
static int
list_tags_full(void)
{
	if (tags_n == 0) {
		puts(_("tag: No tags"));
		return FUNC_SUCCESS;
	}

	int exit_status = FUNC_SUCCESS;

	for (size_t i = 0; tags[i]; i++) {
		printf(_("Files tagged as %s%s%s:\n"), conf.colorize == 1
			? BOLD : "'", tags[i], conf.colorize == 1 ? NC : "'");
		if (list_files_in_tag(tags[i]) != FUNC_SUCCESS)
			exit_status = FUNC_FAILURE;

		if (tags[i + 1])
			putchar('\n');
	}

	return exit_status;
}

static int
list_tags(char **args)
{
	if (tags_n == 0)
		return print_no_tags();

	size_t i;
	int exit_status = FUNC_SUCCESS;

	if (!args || !args[0] || !args[1] || !args[2]) {
		/* 'tag list': list all tags */
		const int pad = (int)get_longest_tag();

		for (i = 0; tags[i]; i++) {
			char p[PATH_MAX + 1];
			snprintf(p, sizeof(p), "%s/%s", tags_dir, tags[i]);
			const filesn_t n = count_dir(p, NO_CPOP);
			if (n > 2)
				printf("%-*s [%s%jd%s]\n", pad, tags[i], mi_c,
					(intmax_t)n - 2, df_c);
			else
				printf("%-*s  -\n", pad, tags[i]);
		}

		return FUNC_SUCCESS;
	}

	for (i = 2; args[i]; i++) {
		if (!is_tag(args[i])) {
			/* 'tag list FILENAME' */
			char *p = unescape_str(args[i]);

			struct stat a;
			if (lstat(p ? p : args[i], &a) == -1) {
				exit_status = errno;
				xerror("%s: %s\n", p ? p : args[i], strerror(errno));
				free(p);
				continue;
			}

			printf(_("%s%s%s is tagged as:\n"), conf.colorize == 1 ? BOLD : "'",
				p ? p : args[i], conf.colorize == 1 ? NC : "'");
			free(p);
			list_tags_having_file(a.st_dev, a.st_ino);

		} else {
			/* 'tag list TAG' */
			printf(_("Files tagged as %s%s%s%s:\n"), conf.colorize == 1 ? BOLD : "'",
				args[i], conf.colorize == 1 ? BOLD : "'",
				conf.colorize == 1 ? NC : "'");
			if (list_files_in_tag(args[i]) == FUNC_FAILURE)
				exit_status = FUNC_FAILURE;
		}

		if (args[i + 1])
			putchar('\n');
	}

	return exit_status;
}

static void
reload_tags(void)
{
	free_tags();
	load_tags();
}

/* Create tags according to ARGS + 2. */
static int
create_tags(char **args)
{
	if (!args || !args[1] || !args[2])
		return print_usage(FUNC_FAILURE);

	int exit_status = FUNC_SUCCESS;

	for (size_t i = 2; args[i]; i++) {
		char dir[PATH_MAX + 1];
		const char *p = strchr(args[i], '\\');
		if (p) {
			char *deq = unescape_str(args[i]);
			if (deq) {
				free(args[i]);
				args[i] = deq;
			}
		}

		snprintf(dir, sizeof(dir), "%s/%s", tags_dir, args[i]);

		struct stat a;
		if (lstat(dir, &a) != -1) {
			xerror(_("tag: '%s': Tag already exists\n"), args[i]);
			exit_status = FUNC_FAILURE;
			continue;
		}

		if (xmkdir(dir, S_IRWXU) == FUNC_FAILURE) {
			xerror(_("tag: '%s': Error creating tag: %s\n"),
				args[i], strerror(errno));
			exit_status = FUNC_FAILURE;
			continue;
		}

		printf(_("%s: Successfully created tag\n"), args[i]);
	}

	reload_tags();
	return exit_status;
}

/* Remove those tags specified in ARGS + 2. */
static int
remove_tags(char **args)
{
	if (tags_n == 0)
		return print_no_tags();

	int exit_status = FUNC_SUCCESS;

	for (size_t i = 2; args[i]; i++) {
		const char *p = strchr(args[i], '\\');
		if (p) {
			char *deq = unescape_str(args[i]);
			if (deq) {
				free(args[i]);
				args[i] = deq;
			}
		}

		char dir[PATH_MAX + 1];
		snprintf(dir, sizeof(dir), "%s/%s", tags_dir, args[i]);

		struct stat a;
		if (stat(dir, &a) == -1 || !S_ISDIR(a.st_mode))
			return print_no_such_tag(args[i]);

		char *cmd[] = {"rm", "-r", "--", dir, NULL};
		if (launch_execv(cmd, FOREGROUND, E_NOFLAG) == FUNC_SUCCESS) {
			printf(_("'%s': Successfully removed tag\n"), args[i]);
			reload_tags();
		} else {
			exit_status = FUNC_FAILURE;
		}
	}

	return exit_status;
}

/* Tag the file named NAME as TAG. */
static int
tag_file(char *name, char *tag)
{
	struct stat a;
	if (lstat(name, &a) == -1) {
		xerror("tag: '%s': %s\n", name, strerror(errno));
		return FUNC_FAILURE;
	}

	int new_tag = 0;
	char *p = NULL;
	if (strchr(tag, '\\'))
		p = unescape_str(tag);
	char dir[PATH_MAX + 1];
	snprintf(dir, sizeof(dir), "%s/%s", tags_dir, p ? p : tag);

	if (stat(dir, &a) == -1) {
		if (xmkdir(dir, S_IRWXU) != FUNC_SUCCESS) {
			xerror(_("tag: '%s': Cannot create tag: %s\n"),
				p ? p : tag, strerror(errno));
			free(p);
			return FUNC_FAILURE;
		}
		new_tag = 1;
	}

	if (new_tag == 1) {
		printf(_("Created new tag %s%s%s\n"),
			conf.colorize ? BOLD : "", p ? p : tag, df_c);
		reload_tags();
	}
	free(p);

	char name_path[PATH_MAX + 1];
	*name_path = '\0';
	if (*name != '/') {
		snprintf(name_path, sizeof(name_path), "%s/%s",
			workspaces[cur_ws].path, name);
	}

	char link[PATH_MAX + NAME_MAX], *q = NULL;
	char *link_path = replace_slashes(*name_path ? name_path : name, ':');

	snprintf(link, sizeof(link), "%s/%s", dir, link_path);
	free(link_path);

	if (lstat(link, &a) != -1)
		return print_tag_creation_error((q && *(++q)) ? q : name, a.st_mode);

	if (symlink(*name_path ? name_path : name, link) == -1)
		return print_symlink_error(name);

	return FUNC_SUCCESS;
}

/* Return an array with indices of tag names (:TAG) found in the ARGS array.
 * The indices array is terminated by a -1.
 * Returns NULL if no index was found .*/
static int *
get_tags(char **args)
{
	int n, c = 0;

	for (n = 0; args[n]; n++);
	int *t = xnmalloc((size_t)n + 1, sizeof(int));

	for (int i = 0; i < n; i++) {
		if (*args[i] == ':' && *(args[i] + 1))
			t[c++] = i;
	}

	if (c == 0) {
		free(t);
		t = NULL;
	} else {
		t[c] = -1;
	}

	return t;
}

/* Tag filenames found in ARGS as all tags (:TAG) found in ARGS. */
static int
tag_files(char **args)
{
	int *tag_names = get_tags(args);
	if (!tag_names || tag_names[0] == -1) {
		free(tag_names);
		xerror("%s\n", _("tag: No tag specified. Specify a tag via :TAG. "
			"E.g. 'tag add FILE1 FILE2 :TAG'"));
		return FUNC_FAILURE;
	}

	size_t i, j, n = 0;
	const size_t start = (args[1] && strcmp(args[1], "add") == 0) ? 2 : 1;

	for (i = start; args[i]; i++) {
		if (*args[i] != ':')
			n++;
	}

	for (i = 0; tag_names[i] != -1; i++) {
		for (j = start; args[j]; j++) {
			if (*args[j] == ':')
				continue;

			char *p = NULL;
			if (strchr(args[j], '\\'))
				p = unescape_str(args[j]);

			if (tag_file(p ? p : args[j], args[tag_names[i]] + 1) != FUNC_SUCCESS)
				if (n > 0) --n;
			free(p);
		}
	}

	free(tag_names);

	if (n > 0)
		printf(_("Successfully tagged %zu file(s)\n"), n);
	return FUNC_SUCCESS;
}

/* Untag filenames found in ARGS tagged as ARGS[N]. */
static int
untag(char **args, const size_t n, size_t *t)
{
	if (!args || !args[1])
		return FUNC_FAILURE;

	int exit_status = FUNC_SUCCESS;

	for (size_t i = 2; args[i]; i++) {
		if (i == n || (*args[i] == ':' && *(args[1] + 1)))
			continue;

		char *ds = unescape_str(args[n] + 1);
		char dir[PATH_MAX + 1];
		snprintf(dir, sizeof(dir), "%s/%s", tags_dir, ds ? ds : args[n] + 1);
		free(ds);

		struct stat a;
		if (lstat(dir, &a) == -1 || !S_ISDIR(a.st_mode))
			return print_no_such_tag(args[n] + 1);

		char f[PATH_MAX + NAME_MAX];
		char *deq = unescape_str(args[i]);
		char *p = deq ? deq : args[i];
		char *exp = NULL;
		if (*p == '~')
			exp = tilde_expand(p);
		const char *q = exp ? exp : p;
		char *r = replace_slashes(q, ':');

		snprintf(f, sizeof(f), "%s/%s", dir, r ? r : q);
		free(deq);
		free(exp);
		free(r);

		if (lstat(f, &a) != -1 && S_ISLNK(a.st_mode)) {
			errno = 0;
			if (unlinkat(XAT_FDCWD, f, 0) == -1) {
				exit_status = errno;
				xerror("tag: '%s': %s\n", args[i], strerror(errno));
			} else {
				(*t)++;
			}
		} else {
			xerror(_("tag: '%s': File not tagged as %s%s%s\n"),
				args[i], conf.colorize ? BOLD : "", args[n] + 1, df_c);
			continue;
		}
	}

	return exit_status;
}

/* Untag filenames found in ARGS as all tags (:TAG) found in ARGS. */
static int
untag_files(char **args)
{
	int exit_status = FUNC_SUCCESS;
	size_t n = 0;

	for (size_t i = 1; args[i]; i++) {
		if (*args[i] == ':' && *(args[i] + 1)
		&& untag(args, i, &n) == FUNC_FAILURE)
			exit_status = FUNC_FAILURE;
	}

	if (n > 0)
		printf(_("Successfully untagged %zu file(s)\n"), n);

	return exit_status;
}

/* Rename tag ARGS[2] as ARGS[3]. */
static int
rename_tag(char **args)
{
	if (!args || !args[1] || !args[2] || !args[3])
		return print_usage(FUNC_FAILURE);

	char *old = args[2];
	const char *new = args[3];
	if (!is_tag(old))
		return print_no_such_tag(old);

	if (*old == *new && strcmp(old, new) == 0) {
		xerror("%s\n", _("tag: New and old filenames are the same"));
		return FUNC_FAILURE;
	}

	char old_dir[PATH_MAX + 1];
	snprintf(old_dir, sizeof(old_dir), "%s/%s", tags_dir, old);

	char new_dir[PATH_MAX + 1];
	snprintf(new_dir, sizeof(new_dir), "%s/%s", tags_dir, new);

	if (renameat(XAT_FDCWD, old_dir, XAT_FDCWD, new_dir) == -1) {
		xerror("tag: %s\n", strerror(errno));
		return errno;
	}

	puts(_("Successfully renamed tag"));
	reload_tags();
	return FUNC_SUCCESS;
}

/* Move all (tagged) files (symlinks) in SRC into DST.
 * Returns zero if success or the appropriate ERRNO in case of error. */
static int
recursive_mv_tags(const char *src, const char *dst)
{
	int n, exit_status = FUNC_SUCCESS, ret;
	char src_dir[PATH_MAX + 1];
	char dst_dir[PATH_MAX + 1];
	struct dirent **a = NULL;

	snprintf(src_dir, sizeof(src_dir), "%s/%s", tags_dir, src);

	n = scandir(src_dir, &a, NULL, alphasort);
	if (n == -1) {
		xerror("tag: '%s': %s\n", src_dir, strerror(errno));
		return errno;
	}

	snprintf(dst_dir, sizeof(dst_dir), "%s/%s", tags_dir, dst);

	for (int i = 0; i < n; i++) {
		if (SELFORPARENT(a[i]->d_name)) {
			free(a[i]);
			continue;
		}
		char src_file[PATH_MAX + NAME_MAX + 2];
		snprintf(src_file, sizeof(src_file), "%s/%s", src_dir, a[i]->d_name);
		char *cmd[] = {"mv", "--", src_file, dst_dir, NULL};
		if ((ret = launch_execv(cmd, FOREGROUND, E_NOFLAG)) != FUNC_SUCCESS)
			exit_status = ret;
		free(a[i]);
	}

	free(a);
	return exit_status;
}

/* Merge tags ARGS[2] and ARGS[3]. */
static int
merge_tags(char **args)
{
	if (!args || !args[1] || !args[2] || !args[3])
		return print_usage(FUNC_FAILURE);

	if (!is_tag(args[2]))
		return print_no_such_tag(args[2]);
	if (!is_tag(args[3]))
		return print_no_such_tag(args[3]);

	const char *src = args[2], *dst = args[3];

	if (strcmp(src, dst) == 0) {
		xerror("%s\n", _("tag: Source and destination are the same tag"));
		return FUNC_FAILURE;
	}

	errno = 0;
	int exit_status = recursive_mv_tags(src, dst);
	if (exit_status != FUNC_SUCCESS) {
		xerror("%s\n", _("tag: Cannot merge tags: error moving tagged files"));
		return exit_status;
	}

	char src_dir[PATH_MAX + 1];
	snprintf(src_dir, sizeof(src_dir), "%s/%s", tags_dir, src);
	if (rmdir(src_dir) == -1) {
		xerror("tag: '%s': %s\n", src_dir, strerror(errno));
		return errno;
	}

	reload_tags();
	printf(_("Successfully merged %s%s%s into %s%s%s\n"),
		conf.colorize == 1 ? BOLD : "", src, df_c,
		conf.colorize == 1 ? BOLD : "", dst, df_c);

	return FUNC_SUCCESS;
}

/* Perform the following expansions:
 * ta -> tag add
 * td -> tag del
 * tl -> tag list
 * tm -> tag rename
 * tn -> tag new
 * tu -> tag untag
 * ty -> tag merge
 * The first string in ARGS must always be one of the left values
 * Returns an array with the expanded values. */
static char **
reconstruct_input(char **args)
{
	size_t n = 0, c;
	for (n = 0; args[n]; n++);
	char **a = xnmalloc(n + 2, sizeof(char *));

	a[0] = savestring("tag", 3);

	switch (args[0][1]) {
	case 'a': a[1] = savestring("add", 3); c = 2; break;
	case 'd': a[1] = savestring("del", 3); c = 2; break;
	case 'l': a[1] = savestring("list", 4); c = 2; break;
	case 'm': a[1] = savestring("rename", 6); c = 2; break;
	case 'n': a[1] = savestring("new", 3); c = 2; break;
	case 'u': a[1] = savestring("untag", 5); c = 2; break;
	case 'y': a[1] = savestring("merge", 5); c = 2; break;
	default:  a[1] = savestring("-h", 2); c = 2; break;
	}

	for (n = 1; args[n]; n++)
		a[c++] = savestring(args[n], strlen(args[n]));
	a[c] = NULL;

	return a;
}

/* Free stuff in A (if needed) and exit the tag function. */
static int
end_tag_function(const int exit_status, char **a, const int free_args)
{
	if (free_args == 0)
		return exit_status;

	for (size_t i = 0; a[i]; i++)
		free(a[i]);
	free(a);

	return exit_status;
}

/* Check whether we should print help message or not. */
static int
is_tag_help(char **args)
{
	int first_is_help = (args[1] && IS_HELP(args[1]));

	if (strcmp(args[0], "tl") == 0)
		return first_is_help;

	return (!args[1] || first_is_help || (args[2] && IS_HELP(args[2])));
}

/* Handle tag actions according to ARGS. */
int
tags_function(char **args)
{
	int exit_status = FUNC_SUCCESS, free_args = 0;
	char **a = args;

	if (is_tag_help(a))
		{ puts(_(TAG_USAGE)); goto END; }

	const char b[] = "adlmnuy";
	if (strcmp(a[0], "tag") != 0 && strspn(a[0] + 1, b))
		{ a = reconstruct_input(args); free_args = 1; }

	if (*a[1] == 'l' && strcmp(a[1], "list") == 0)
		{ exit_status = list_tags(a); goto END; }

	if (*a[1] == 'l' && strcmp(a[1], "list-full") == 0)
		{ exit_status = list_tags_full(); goto END; }

	if (*a[1] == 'm' && strcmp(a[1], "merge") == 0)
		{ exit_status = merge_tags(a); goto END; }

	if (*a[1] == 'n' && strcmp(a[1], "new") == 0)
		{ exit_status = create_tags(a); goto END; }

	if (*a[1] == 'd' && strcmp(a[1], "del") == 0)
		{ exit_status = remove_tags(a); goto END; }

	if (*a[1] == 'r' && strcmp(a[1], "rename") == 0)
		{ exit_status = rename_tag(a); goto END; }

	if (*a[1] == 'u' && strcmp(a[1], "untag") == 0)
		{ exit_status = untag_files(a); goto END; }

	/* Either 'tag FILE :TAG' or 'tag add FILE :TAG' */
	exit_status = tag_files(a);

END:
	return end_tag_function(exit_status, a, free_args);
}

#else
void *_skip_me_tags;
#endif /* !_NO_TAGS */
