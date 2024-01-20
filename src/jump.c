/* jump.c -- functions for Kangaroo, the directory jumper function */

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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h> /* unlink(3) */

#if defined(__OpenBSD__) || defined(__NetBSD__) \
|| defined(__FreeBSD__) || defined(__APPLE__) || defined(__HAIKU__)
# include <inttypes.h> /* intmax_t */
#endif /* BSD || __HAIKU__ */

#include "aux.h"
#include "checks.h"
#include "colors.h" /* get_dir_color() */
#include "file_operations.h"
#include "init.h"
#include "messages.h"
#include "misc.h"
#include "navigation.h"
#include "spawn.h"

/* Macros to calculate directories rank extra points */
#define BASENAME_BONUS 	300
#define BOOKMARK_BONUS  500
#define PINNED_BONUS    1000
#define WORKSPACE_BONUS 300
#define VISIT_BONUS     200

/* Calculate last directory access credit */
#define JHOUR(n)  ((n) *= 4) /* Within last hour */
#define JDAY(n)   ((n) *= 2) /* Within last day */
#define JWEEK(n)  ((n) / 2)  /* Within last week */
#define JOLDER(n) ((n) / 4)  /* More than a week */

#define IS_VALID_JUMP_ENTRY(n) ((!jump_db[(n)].path || !*jump_db[(n)].path \
|| jump_db[(n)].rank == JUMP_ENTRY_PURGED) ? 0 : 1)

struct jump_entry_t {
	char *match;
	char *needle;
	size_t visits;
#ifdef __arm__
	char *pad0;
#endif /* __arm__ */
	time_t first;
	time_t last;
};

#define FIRST_SEGMENT (1 << 0)
#define LAST_SEGMENT  (1 << 1)

/* Getting the total rank of an entry:
 * 1) rank = calculate_base_credit()
 * 2) rank += calculate_bonus_credit() */

/* Calcualte the base credit for a directory based on time data
 * (DAYS_SINCE_FIRST and HOURS_SINCE_LAST) and number of visits (VISITS).
 * If the directory has been visited within the last day, KEEP will be
 * set to 1, so that it won't be removed from the database, no matter
 * its current rank. */
static int
calculate_base_credit(const int days_since_first, const int hours_since_last,
	const int visits, int *keep)
{
	int rank = days_since_first > 1
		? ((int)visits * VISIT_BONUS) / days_since_first
		: ((int)visits * VISIT_BONUS);

	/* Credit or penalty based on last directory access. */
	int tmp_rank = rank;

	if (hours_since_last == 0) {          /* Last hour */
		*keep = 1;
		rank = JHOUR(tmp_rank);
	} else if (hours_since_last <= 24) {  /* Last day */
		*keep = 1;
		rank = JDAY(tmp_rank);
	} else if (hours_since_last <= 168) { /* Last week */
		rank = JWEEK(tmp_rank);
	} else { 							  /* More than a week */
		rank = JOLDER(tmp_rank);
	}

	return rank;
}

/* Calculate bonus credit for the entry ENTRY.
 * Matches in directory basename, bookmarked and pinned directories,
 * just as directories currently in a workspace, have bonus credit. */
static int
calculate_bonus_credit(const char *entry, const char *query, int *keep)
{
	if (!entry || !*entry)
		return 0;

	int bonus = 0;

	char *tmp = query ? strrchr(entry, '/') : (char *)NULL;
	if (tmp && *(++tmp)) {
		if (strstr(tmp, query))
			bonus += BASENAME_BONUS;
	}

	int i = (int)bm_n;
	while (--i >= 0) {
		if (bookmarks[i].path && *bookmarks[i].path
		&& bookmarks[i].path[1] == entry[1]
		&& strcmp(bookmarks[i].path, entry) == 0) {
			*keep = 1;
			bonus += BOOKMARK_BONUS;
			break;
		}
	}

	if (pinned_dir && *pinned_dir && pinned_dir[1] == entry[1]
	&& strcmp(pinned_dir, entry) == 0) {
		*keep = 1;
		bonus += PINNED_BONUS;
	}

	i = MAX_WS;
	while (--i >= 0) {
		if (workspaces[i].path && *workspaces[i].path
		&& workspaces[i].path[1] == entry[1]
		&& strcmp(workspaces[i].path, entry) == 0) {
			*keep = 1;
			bonus += WORKSPACE_BONUS;
			break;
		}
	}

	return bonus;
}

/* Calculate the rank as frecency. The algorithm is based
 * on Mozilla, zoxide, and z.lua. See:
 * "https://wiki.mozilla.org/User:Mconnor/Past/PlacesFrecency"
 * "https://github.com/ajeetdsouza/zoxide/wiki/Algorithm#aging"
 * "https://github.com/skywind3000/z.lua#aging" */
static int
rank_entry(const int i, const time_t now, int *days_since_first,
	int *hours_since_last)
{
	/* 86400 = 60 secs / 60 mins / 24 hours */
	*days_since_first = (int)(now - jump_db[i].first_visit) / 86400;
	/* 3600 = 60 secs / 60 mins */
	*hours_since_last = (int)(now - jump_db[i].last_visit) / 3600;

	int keep = 0;
	int rank = calculate_base_credit(*days_since_first, *hours_since_last,
		(int)jump_db[i].visits, &keep);

	rank += calculate_bonus_credit(jump_db[i].path, (char *)NULL, &keep);
	jump_db[i].keep = keep;

	return rank;
}

static void
free_jump_database(void)
{
	int i = (int)jump_n;

	while (--i >= 0)
		free(jump_db[i].path);

	free(jump_db);
	jump_db = (struct jump_t *)NULL;
	jump_n = 0;
}

static int
add_new_jump_entry(const char *dir, const size_t dir_len)
{
	jump_db = xnrealloc(jump_db, jump_n + 2, sizeof(struct jump_t));
	jump_db[jump_n].visits = 1;
	time_t now = time(NULL);
	jump_db[jump_n].first_visit = now;
	jump_db[jump_n].last_visit = now;
	jump_db[jump_n].rank = 0;
	jump_db[jump_n].keep = 0;
	jump_db[jump_n].len = dir_len;
	jump_db[jump_n].path = savestring(dir, dir_len);
	jump_n++;

	jump_db[jump_n].path = (char *)NULL;
	jump_db[jump_n].len = 0;
	jump_db[jump_n].visits = 0;
	jump_db[jump_n].rank = 0;
	jump_db[jump_n].keep = 0;
	jump_db[jump_n].first_visit = -1;
	jump_db[jump_n].last_visit = -1;

	return EXIT_SUCCESS;
}

/* Add DIR to the jump database. If already there, just update the number of
 * visits and the last visit time. */
int
add_to_jumpdb(char *dir)
{
	if (xargs.no_dirjump == 1 || !dir || !*dir)
		return EXIT_FAILURE;

	size_t dir_len = strlen(dir);
	if (dir_len > 1 && dir[dir_len - 1] == '/') {
		dir[dir_len - 1] = '\0';
		dir_len--;
	}

	if (!jump_db) {
		jump_db = xnmalloc(1, sizeof(struct jump_t));
		jump_n = 0;
	}

	int i = (int)jump_n, new_entry = 1;
	while (--i >= 0) {
		/* Jump entries are all absolute paths, so that they all start with
		 * a slash. Let's start comparing them from the second char. */
		if (!IS_VALID_JUMP_ENTRY(i) || dir_len != jump_db[i].len
		|| dir[1] != jump_db[i].path[1])
			continue;

		if (strcmp(jump_db[i].path, dir) == 0) {
			jump_db[i].visits++;
			jump_db[i].last_visit = time(NULL);
			new_entry = 0;
			break;
		}
	}

	if (new_entry == 0)
		return EXIT_SUCCESS;

	return add_new_jump_entry(dir, dir_len);
}

/* Store the jump database into a file. */
void
save_jumpdb(void)
{
	if (xargs.no_dirjump == 1 || config_ok == 0 || !config_dir || !jump_db
	|| jump_n == 0)
		return;

	char jump_file[PATH_MAX + 1];
	snprintf(jump_file, sizeof(jump_file), "%s/jump.clifm", config_dir);

	int fd = 0;
	FILE *fp = open_fwrite(jump_file, &fd);
	if (!fp)
		return;

	int i, reduce = 0, total_rank = 0;
	time_t now = time(NULL);

	/* Calculate both total rank sum, and rank for each entry */
	i = (int)jump_n;
	while (--i >= 0) {
		if (!IS_VALID_JUMP_ENTRY(i))
			continue;

		int days_since_first = 0, hours_since_last = 0;
		int rank = rank_entry(i, now, &days_since_first, &hours_since_last);
		UNUSED(days_since_first); UNUSED(hours_since_last);

		jump_db[i].rank = rank;
		total_rank += rank;
	}

	/* Once we have the total rank, check if we need to reduce ranks,
	 * and then write entries into the database. */
	if (total_rank > conf.max_jump_total_rank && conf.max_jump_total_rank > 0)
		reduce = (total_rank / conf.max_jump_total_rank) + 1;

	for (i = 0; i < (int)jump_n; i++) {
		if (total_rank > conf.max_jump_total_rank) {
			if (reduce) {
				int tmp_rank = jump_db[i].rank;
				jump_db[i].rank = tmp_rank / reduce;
			}
		}

		/* Forget directories ranked below MIN_JUMP_RANK */
		if (jump_db[i].keep != 1 && jump_db[i].rank < conf.min_jump_rank) {
			/* Discount from TOTAL_RANK the rank of the now forgotten
			 * directory to keep this total up to date */
			total_rank -= jump_db[i].rank;
			continue;
		}

		fprintf(fp, "%zu:%jd:%jd:%s\n", jump_db[i].visits,
			(intmax_t)jump_db[i].first_visit,
			(intmax_t)jump_db[i].last_visit, jump_db[i].path);
	}

	fprintf(fp, "@%d\n", total_rank);
	fclose(fp);
}

int
edit_jumpdb(char *app)
{
	if (config_ok == 0 || !config_dir) {
		xerror(_("je: Configuration directory not found\n"));
		return EXIT_FAILURE;
	}

	save_jumpdb();

	char jump_file[PATH_MAX + 1];
	snprintf(jump_file, sizeof(jump_file), "%s/jump.clifm", config_dir);

	struct stat attr;
	if (stat(jump_file, &attr) == -1) {
		xerror("jump: '%s': %s\n", jump_file, strerror(errno));
		return errno;
	}

	const time_t mtime_bfr = attr.st_mtime;

	int ret = EXIT_FAILURE;
	if (app && *app) {
		char *cmd[] = {app, jump_file, NULL};
		ret = launch_execv(cmd, FOREGROUND, E_NOFLAG);
	} else {
		open_in_foreground = 1;
		ret = open_file(jump_file);
		open_in_foreground = 0;
	}

	if (ret != EXIT_SUCCESS)
		return ret;

	if (stat(jump_file, &attr) == -1) {
		xerror("jump: '%s': %s\n", jump_file, strerror(errno));
		return errno;
	}

	if (mtime_bfr == (time_t)attr.st_mtime)
		return EXIT_SUCCESS;

	if (jump_db)
		free_jump_database();

	load_jumpdb();
	return EXIT_SUCCESS;
}

/* Save jump entry into the suggestions buffer. */
static int
save_jump_suggestion(char *str)
{
	if (!str || !*str)
		return EXIT_FAILURE;

	free(jump_suggestion);
	size_t len = strlen(str);

	int slash = 0;
	if (len > 0 && str[len - 1] == '/')
		slash = 1;

	size_t jump_sug_len = len + (slash == 1 ? 1 : 2);
	jump_suggestion = xnmalloc(jump_sug_len, sizeof(char));
	if (slash == 0)
		snprintf(jump_suggestion, jump_sug_len, "%s/", str);
	else
		xstrsncpy(jump_suggestion, str, jump_sug_len);

	return EXIT_SUCCESS;
}

static char *
get_directory_color(const char *filename, const struct stat *a)
{
	if (check_file_access(a->st_mode, a->st_uid, a->st_gid) == 0)
		return nd_c;

	if (S_ISLNK(a->st_mode)) {
		char *linkname = realpath(filename, (char *)NULL);
		if (linkname) {
			free(linkname);
			return ln_c;
		}
		return or_c;
	}

	return get_dir_color(filename, a->st_mode, a->st_nlink, -1);
}

/* Compare ranks A and B (used to sort jump entries by rank)*/
int
rank_cmp(const void *a, const void *b)
{
	struct jump_t *pa = (struct jump_t *)a;
	struct jump_t *pb = (struct jump_t *)b;

	return (pa->rank > pb->rank);
}

static void
print_jump_table_header(void)
{
	char item[10]; /* BOLD and NC are 4 bytes each */
	snprintf(item, sizeof(item), "%s*%s", conf.colorize == 1 ? BOLD : "", NC);

	printf(_("%s First time access is displayed in days, while last "
		"time access is displayed in hours.\n"), item);
	printf(_("%s An asterisk next rank values means that the "
		"corresponding directory will not be removed despite its rank, "
		"either because it was visited in the last 24 hours, or because "
		"it is bookmarked, pinned, or currently active in some "
		"workspace.\n"), item);

	if (conf.min_jump_rank <= 0) {
		printf(_("%s MinJumpRank is set to %d: entries will not be removed "
			"from the database (no matter their rank).\n"),
			item, conf.min_jump_rank);
	} else {
		printf(_("%s Entries ranked below MinJumpRank (currently %d) will be "
			"removed at program exit.\n"), item, conf.min_jump_rank);
	}

	printf(_("\n %sOrder\tVisits\tFirst\tLast\tRank\tDirectory%s\n"),
		conf.colorize == 1 ? BOLD : "", NC);
}

static int
print_jump_table(const int reduce, const time_t now)
{
	if (jump_n == 0) {
		printf("jump: Database still empty\n");
		return EXIT_SUCCESS;
	}

	HIDE_CURSOR;
	print_jump_table_header();

	size_t i;
	int ranks_sum = 0, visits_sum = 0;
	int max_rank = 0, max_visits = 0, max_first = 0, max_last = 0;
	int max_order = DIGINUM(jump_n);

	struct jump_t *tmp_jump = xnmalloc(jump_n + 1, sizeof(struct jump_t));

	for (i = 0; i < jump_n; i++) {
		if (!IS_VALID_JUMP_ENTRY(i)) {
			tmp_jump[i].path = (char *)NULL;
			continue;
		}

		int days_since_first = 0, hours_since_last = 0;
		int rank = rank_entry((int)i, now, &days_since_first, &hours_since_last);
		int keep = jump_db[i].keep;
		jump_db[i].keep = 0;

		if (reduce) {
			int tmp_rank = rank;
			rank = tmp_rank / reduce;
		}

		ranks_sum += rank;
		visits_sum += (int)jump_db[i].visits;

		tmp_jump[i].path = jump_db[i].path;
		tmp_jump[i].keep = keep;
		tmp_jump[i].rank = rank;
		tmp_jump[i].len = jump_db[i].len;
		tmp_jump[i].visits = jump_db[i].visits;
		tmp_jump[i].first_visit = (time_t)days_since_first;
		tmp_jump[i].last_visit = (time_t)hours_since_last;

		/* Get largest item for each field to correctly calculate pad */
		int n = DIGINUM(rank);
		if (n > max_rank)
			max_rank = n;
		n = DIGINUM(tmp_jump[i].visits);
		if (n > max_visits)
			max_visits = n;
		n = DIGINUM(tmp_jump[i].first_visit);
		if (n > max_first)
			max_first = n;
		n = DIGINUM(tmp_jump[i].last_visit);
		if (n > max_last)
			max_last = n;
	}

	/* Sort entries by rank (from less to more) */
	qsort(tmp_jump, jump_n, sizeof(*tmp_jump), rank_cmp);

	for (i = 0; i < jump_n; i++) {
		if (!tmp_jump[i].path)
			continue;

		char *color = (workspaces[cur_ws].path
		&& workspaces[cur_ws].path[1] == tmp_jump[i].path[1]
		&& strcmp(workspaces[cur_ws].path, tmp_jump[i].path) == 0) ? mi_c : df_c;

		struct stat a;
		if (lstat(tmp_jump[i].path, &a) == -1)
			color = uf_c;

		char *dir_color = color == uf_c ? uf_c : get_directory_color(tmp_jump[i].path, &a);

		printf(" %s%*zu\t%*zu\t%*d\t%*d\t%s%*d%s%s%c%s\t%c%s%s%s\n",
			color != uf_c ? color : df_c, max_order, i + 1,
			max_visits, tmp_jump[i].visits,
			max_first, (int)tmp_jump[i].first_visit,
			max_last, (int)tmp_jump[i].last_visit,
		    conf.colorize == 1 ? BOLD : "", max_rank, tmp_jump[i].rank,
		    color != uf_c ? color : "",
		    tmp_jump[i].keep == 1 ? li_c : "", tmp_jump[i].keep == 1
		    ? '*' : 0, (tmp_jump[i].keep == 1 && color != uf_c) ? color : "",
		    (conf.colorize == 0 && color == uf_c) ? '!' : 0,
		    dir_color, tmp_jump[i].path, df_c);
	}

	free(tmp_jump);

	printf(_("\nTotal rank: %d/%d\nTotal visits: %d\n"), ranks_sum,
	    conf.max_jump_total_rank, visits_sum);

	UNHIDE_CURSOR;
	return EXIT_SUCCESS;
}

static int
purge_invalid_entries(void)
{
	int i = (int)jump_n;
	int c = 0;

	struct stat a;
	while (--i >= 0) {
		if (!IS_VALID_JUMP_ENTRY(i))
			continue;

		if (stat(jump_db[i].path, &a) == -1) {
			printf("%s->%s %s%s%s\n", mi_c, df_c, uf_c, jump_db[i].path, df_c);
			jump_db[i].rank = JUMP_ENTRY_PURGED;
			c++;
		}
	}

	if (c == 0)
		puts(_("jump: No invalid entries"));
	else
		printf(_("\njump: Purged %d invalid %s\n"), c,
			c == 1 ? _("entry") : _("entries"));

	return EXIT_SUCCESS;
}

static int
purge_low_ranked_entries(const int limit)
{
	int i = (int)jump_n;
	int c = 0;
	time_t now = time(NULL);

	while (--i >= 0) {
		if (!IS_VALID_JUMP_ENTRY(i))
			continue;

		int days_since_first = 0, hours_since_last = 0;
		int rank = rank_entry(i, now, &days_since_first, &hours_since_last);
		UNUSED(days_since_first); UNUSED(hours_since_last);

		if (rank < limit) {
			if (jump_db[i].keep == 1) {
				jump_db[i].keep = 0;
				continue;
			}
			jump_db[i].keep = 0;
			printf("%s->%s %s (%d)\n", mi_c, df_c, jump_db[i].path, rank);
			jump_db[i].rank = JUMP_ENTRY_PURGED;
			c++;
		}
	}

	if (c == 0)
		printf(_("jump: No entry ranked below %d\n"), limit);
	else
		printf(_("\njump: Purged %d %s\n"),
			c, c == 1 ? _("entry") : _("entries"));

	return EXIT_SUCCESS;
}

static int
purge_jump_database(char *arg)
{
	if (!arg || !*arg)
		return purge_invalid_entries();

	if (!is_number(arg)) {
		puts(JUMP_USAGE);
		return EXIT_FAILURE;
	}

	int n = atoi(arg);
	if (n < 0) {
		xerror(_("jump: '%s': Invalid value\n"), arg);
		return EXIT_FAILURE;
	}

	return purge_low_ranked_entries(n);
}

/*
static inline int
handle_jump_order(char *arg, const int mode)
{
	if (!arg) {
		if (mode == NO_SUG_JUMP)
			fprintf(stderr, "%s\n", _(JUMP_USAGE));
		return EXIT_FAILURE;
	}

	if (!is_number(arg)) {
		if (mode == NO_SUG_JUMP)
			return cd_function(arg, CD_PRINT_ERROR);

		return EXIT_FAILURE;
	}

	int int_order = atoi(arg);
	if (int_order <= 0 || int_order > (int)jump_n
	|| !jump_db || !jump_db[int_order - 1].path) {
		if (mode == NO_SUG_JUMP)
			xerror(_("jump: %s: No such order number\n"), arg);
		return EXIT_FAILURE;
	}

	if (mode == NO_SUG_JUMP)
		return cd_function(jump_db[int_order - 1].path, CD_PRINT_ERROR);

	return save_jump_suggestion(jump_db[int_order - 1].path);
} */

static inline int
check_jump_params(char **args, const time_t now, const int reduce)
{
	if (args[0][1] == 'e')
		return edit_jumpdb(args[1]);

	if (!args[1])
		return print_jump_table(reduce, now);

	if (IS_HELP(args[1])) {
		puts(_(JUMP_USAGE));
		return EXIT_SUCCESS;
	}

	if (*args[1] == '-' && strcmp(args[1], "--edit") == 0)
		return edit_jumpdb(args[2]);

	if (*args[1] == '-' && strcmp(args[1], "--purge") == 0)
		return purge_jump_database(args[2]);

	return (-1);
}

static inline int
mark_target_segment(char *str)
{
	size_t len = strlen(str);
	int segment = 0;

	if (len > 0 && str[len - 1] == '/') {
		str[len - 1] = '\0';
		segment |= LAST_SEGMENT;
	} else if (len > 0 && str[len - 1] == '\\') {
		str[len - 1] = '\0';
		segment |= FIRST_SEGMENT;
	}

	return segment;
}

/* Return a pointer to the beggining of the string QUERY in the path NEEDLE.
 * If used for the first time (checking the first query string), NEEDLE is
 * the same as MATCH. Otherwise, NEEDLE points to a char in MATCH.
 * SEGMENT is a flag: LAST_SEGMENT or FIRST_SEGMENT. In both cases the
 * resulting string will be checked to match for the first or last segment
 * of the full match (MATCH). If not NULL is returned. */
static inline char *
get_needle(const char *needle, const char *match, const char *query,
	const int segment)
{
	char *ret = conf.case_sens_dirjump == 1 ? strstr(needle, query)
#ifdef _BE_POSIX
		/* xstrcasestr() calls x_strcasestr(), which does not take consts. */
		: xstrcasestr((char *)needle, (char *)query);
#else
		: xstrcasestr(needle, query);
#endif /* _BE_POSIX */

	if (!ret || ((segment & LAST_SEGMENT) && strchr(ret, '/')))
		return (char *)NULL;

	if (segment & FIRST_SEGMENT) {
		char p = *ret;
		*ret = '\0';

		if (strrchr(match, '/') != match) {
			*ret = p;
			return (char *)NULL;
		}

		*ret = p;
	}

	return ret;
}

static inline int
rank_tmp_entry(const struct jump_entry_t entry, const time_t now,
	const int reduce, const char *query)
{
	/* 86400 = 60 secs / 60 misc / 24 hours */
	int days_since_first = (int)(now - entry.first) / 86400;
	/* 3600 = 60 secs / 60 mins */
	int hours_since_last = (int)(now - entry.last) / 3600;

	int keep = 0;
	int rank = calculate_base_credit(days_since_first, hours_since_last,
		(int)entry.visits, &keep);

	rank += calculate_bonus_credit(entry.match, query, &keep);
	UNUSED(keep);

	if (reduce > 0) {
		int tmp_rank = rank;
		rank = tmp_rank / reduce;
	}

	return rank;
}

/* Found the best ranked directory matching query strings in ARGS.
 *
 * The rank is calculated as frecency. The algorithm is based on Mozilla,
 * zoxide, and z.lua. See:
 * https://wiki.mozilla.org/User:Mconnor/Past/PlacesFrecency
 * https://github.com/ajeetdsouza/zoxide/wiki/Algorithm#aging
 * https://github.com/skywind3000/z.lua#aging
 *
 * If MODE is NO_SUG_JUMP, we're running the 'j' command (or any of its
 * variants [je, jc, jp, --edit or --purge]), in which case matches are
 * handled according to the specific command.
 *
 * Otherwise, the function has been called from the suggestions system, in
 * which case the best ranked directory is stored in the suggestions
 * buffer (jump_suggestion) to be suggested by the suggestions system.
 *
 * Returns 1 (if matches are found) or 0. */
int
dirjump(char **args, const int mode)
{
	if (xargs.no_dirjump == 1 && mode == NO_SUG_JUMP) {
		printf(_("%s: Directory jumper function disabled\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	time_t now = time(NULL);
	int reduce = 0;

	/* If the sum total of ranks is greater than max, divide each entry
	 * to make the sum total less than or equal to max. */
	if (jump_total_rank > conf.max_jump_total_rank && conf.max_jump_total_rank > 0)
		reduce = (jump_total_rank / conf.max_jump_total_rank) + 1;

	if (mode == NO_SUG_JUMP) {
		int ret = check_jump_params(args, now, reduce);
		if (ret != -1)
			return ret;
	}

	enum jump jump_opt = NONE;

	switch (args[0][1]) {
	case 'c': jump_opt = JCHILD; break;
	case 'p': jump_opt = JPARENT; break;
//	case 'o': return handle_jump_order(args[1], mode);
	case 'l': jump_opt = JLIST; break;
	case '\0': jump_opt = NONE; break;
	default:
		xerror(_("jump: '%c': Invalid option\n"), args[0][1]);
		fprintf(stderr, "%s\n", _(JUMP_USAGE));
		return EXIT_FAILURE;
	}

	/* If ARG is an actual directory, just cd into it. */
	struct stat attr;
	if (args[1] && !args[2] && lstat(args[1], &attr) != -1) {
		if (mode == NO_SUG_JUMP)
			return cd_function(args[1], CD_PRINT_ERROR);
		return save_jump_suggestion(args[1]);
	}

	/* Find the best ranked directory using ARGS as filter(s). */
	size_t i;
	int j, match = 0;

	struct jump_entry_t *entry = xnmalloc(jump_n + 1, sizeof(struct jump_entry_t));

	for (i = 1; args[i]; i++) {
		/* 1) Using the first parameter, get a list of matches in the
		 * database. */

		/* If the query string ends with a slash, we want this query
		 * string to match only the LAST segment of the path, and if it ends
		 * with a backslash instead, we want this query to match only the
		 * FIRST segment of the path. */
		int segment = mark_target_segment(args[i]);

		if (match == 0) {
			j = (int)jump_n;
			while (--j >= 0) {
				if (!IS_VALID_JUMP_ENTRY(j))
					continue;

				/* Exclude CWD */
				if (workspaces[cur_ws].path
				&& jump_db[j].path[1] == workspaces[cur_ws].path[1]
				&& strcmp(jump_db[j].path, workspaces[cur_ws].path) == 0)
					continue;

				/* Pointer to the beginning of the search str in the
				 * jump entry. Used to search for subsequent search
				 * strings starting from this position in the entry
				 * and not before. */
				char *needle = get_needle(jump_db[j].path,
					jump_db[j].path, args[i], segment);

				if (!needle)
					continue;

				int exclude = 0;
				/* Filter matches according to parent or child options. */
				switch (jump_opt) {
				case JPARENT:
					if (workspaces[cur_ws].path
					&& !strstr(workspaces[cur_ws].path, jump_db[j].path))
						exclude = 1;
					break;

				case JCHILD:
					if (workspaces[cur_ws].path
					&& !strstr(jump_db[j].path, workspaces[cur_ws].path))
						exclude = 1;
					break;

				case NONE: /* fallthrough */
				default: break;
				}

				if (exclude == 1)
					continue;

				entry[match].visits = jump_db[j].visits;
				entry[match].first = jump_db[j].first_visit;
				entry[match].last = jump_db[j].last_visit;
				entry[match].needle = needle;
				entry[match].match = jump_db[j].path;
				match++;
			}
		}

		/* 2) Once we have the list of matches, perform a reverse
		 * matching process, that is, excluding non-matches,
		 * using subsequent parameters. */
		else {
			j = (int)match;
			while (--j >= 0) {
				if (!entry[j].match || !*entry[j].match) {
					entry[j].match = (char *)NULL;
					continue;
				}

				char *needle = get_needle(entry[j].needle + 1,
					entry[j].match, args[i], segment);

				if (!needle) {
					entry[j].match = (char *)NULL;
					continue;
				}

				entry[j].needle = needle;
			}
		}
	}

	/* If something remains, we have at least one match. */

	/* 3) Further filter the list of matches by frecency, so that only
	 * the best ranked directory will be returned. */

	int found = 0, exit_status = EXIT_FAILURE, max = -1;
	char *best_ranked = (char *)NULL;

	j = match;

	while (--j >= 0) {
		if (!entry[j].match)
			continue;

		found = 1;

		if (jump_opt == JLIST) {
			colors_list(entry[j].match, 0, 0 , 1);
			continue;
		}

		int rank = rank_tmp_entry(entry[j], now, reduce, args[args_n]);

		if (rank > max) {
			max = rank;
			best_ranked = entry[j].match;
		}
	}

	if (found == 0) {
		if (mode == NO_SUG_JUMP)
			puts(_("jump: No matches found"));
		exit_status = EXIT_FAILURE;
		goto END;
	}

	if (jump_opt == JLIST) {
		exit_status = EXIT_SUCCESS;
		goto END;
	}

	if (mode == NO_SUG_JUMP)
		exit_status = cd_function(best_ranked, CD_PRINT_ERROR);
	else
		exit_status = save_jump_suggestion(best_ranked);

END:
	free(entry);
	return exit_status;
}
