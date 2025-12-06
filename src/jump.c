/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2025 L. Abramovich <leo.clifm@outlook.com>
*/

/* jump.c -- functions for Kangaroo, the directory jumper function */

#include "helpers.h"

#include <errno.h>
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
#define PERMANENT_BONUS 300
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
	size_t keep;
	size_t visits;
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
	const size_t visits, int *keep)
{
	const int ivisits = visits > INT_MAX ? INT_MAX : (int)visits;
	int rank = days_since_first > 1
		? (ivisits * VISIT_BONUS) / days_since_first
		: (ivisits * VISIT_BONUS);

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

	const char *tmp = query ? strrchr(entry, '/') : (char *)NULL;
	if (tmp && *(++tmp)) {
		if (strstr(tmp, query))
			bonus += BASENAME_BONUS;
	}

	for (size_t i = bm_n; i-- > 0;) {
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

	for (size_t i = MAX_WS; i-- > 0;) {
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
rank_entry(const size_t i, const time_t now, int *days_since_first,
	int *hours_since_last)
{
	/* 86400 = 60 secs / 60 mins / 24 hours */
	*days_since_first = (int)(now - jump_db[i].first_visit) / 86400;
	/* 3600 = 60 secs / 60 mins */
	*hours_since_last = (int)(now - jump_db[i].last_visit) / 3600;

	int keep = 0;
	int rank = calculate_base_credit(*days_since_first, *hours_since_last,
		jump_db[i].visits, &keep);

	rank += calculate_bonus_credit(jump_db[i].path, (char *)NULL, &keep);
	if (jump_db[i].keep == JUMP_ENTRY_PERMANENT)
		rank += PERMANENT_BONUS;
	else
		jump_db[i].keep = keep;

	return rank;
}

static void
free_jump_database(void)
{
	for (size_t i = jump_n; i-- > 0;)
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
	const time_t now = time(NULL);
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

	return FUNC_SUCCESS;
}

/* Add DIR to the jump database. If already there, just update the number of
 * visits and the last visit time. */
int
add_to_jumpdb(char *dir)
{
	if (xargs.no_dirjump == 1 || !dir || !*dir)
		return FUNC_FAILURE;

	size_t dir_len = strlen(dir);
	if (dir_len > 1 && dir[dir_len - 1] == '/') {
		dir[dir_len - 1] = '\0';
		dir_len--;
	}

	if (!jump_db) {
		jump_db = xnmalloc(1, sizeof(struct jump_t));
		jump_n = 0;
	}

	int new_entry = 1;
	for (size_t i = jump_n; i-- > 0;) {
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
		return FUNC_SUCCESS;

	return add_new_jump_entry(dir, dir_len);
}

/* Save the jump database into a file. */
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

	int reduce = 0, total_rank = 0;
	const time_t now = time(NULL);

	/* Calculate both total rank sum, and rank for each entry. */
	for (size_t i = jump_n; i-- > 0;) {
		if (!IS_VALID_JUMP_ENTRY(i))
			continue;

		int days_since_first = 0, hours_since_last = 0;
		const int rank = rank_entry(i, now, &days_since_first, &hours_since_last);
		UNUSED(days_since_first); UNUSED(hours_since_last);

		jump_db[i].rank = rank;
		total_rank += rank;
	}

	/* Once we have the total rank, check if we need to reduce ranks,
	 * and then write entries into the database. */
	if (total_rank > conf.max_jump_total_rank && conf.max_jump_total_rank > 0)
		reduce = (total_rank / conf.max_jump_total_rank) + 1;

	char perm_chr_str[2] = "";

	for (size_t i = 0; i < jump_n; i++) {
		if (total_rank > conf.max_jump_total_rank) {
			if (reduce) {
				const int tmp_rank = jump_db[i].rank;
				jump_db[i].rank = tmp_rank / reduce;
			}
		}

		/* Forget directories ranked below MIN_JUMP_RANK. */
		if (jump_db[i].keep < 1 && jump_db[i].rank < conf.min_jump_rank) {
			/* Discount from TOTAL_RANK the rank of the now forgotten
			 * directory to keep this total up to date. */
			total_rank -= jump_db[i].rank;
			continue;
		}

		perm_chr_str[0] = jump_db[i].keep == JUMP_ENTRY_PERMANENT
			? JUMP_ENTRY_PERMANENT_CHR : '\0';
		fprintf(fp, "%s%zu:%jd:%jd:%s\n",
			perm_chr_str, jump_db[i].visits,
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
		return FUNC_FAILURE;
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

	const int ret = open_config_file(app, jump_file);
	if (ret != FUNC_SUCCESS)
		return ret;

	if (stat(jump_file, &attr) == -1) {
		xerror("jump: '%s': %s\n", jump_file, strerror(errno));
		return errno;
	}

	if (mtime_bfr == (time_t)attr.st_mtime)
		return FUNC_SUCCESS;

	if (jump_db)
		free_jump_database();

	load_jumpdb();
	return FUNC_SUCCESS;
}

/* Save jump entry into the suggestions buffer. */
static int
save_jump_suggestion(const char *str)
{
	if (!str || !*str)
		return FUNC_FAILURE;

	free(jump_suggestion);
	const size_t len = strlen(str);

	const int slash = (len > 0 && str[len - 1] == '/');

	const size_t jump_sug_len = len + (slash == 1 ? 1 : 2);
	jump_suggestion = xnmalloc(jump_sug_len, sizeof(char));
	if (slash == 0)
		snprintf(jump_suggestion, jump_sug_len, "%s/", str);
	else
		xstrsncpy(jump_suggestion, str, jump_sug_len);

	return FUNC_SUCCESS;
}

static char *
get_directory_color(const char *filename)
{
	struct stat a;
	if (lstat(filename, &a) == -1)
		return uf_c;

	if (S_ISLNK(a.st_mode)) {
		char *linkname = xrealpath(filename, NULL);
		if (linkname) {
			free(linkname);
			return ln_c;
		}
		return or_c;
	}

	return get_dir_color(filename, &a, -1);
}

/* Compare ranks A and B (used to sort jump entries by rank). */
int
rank_cmp(const void *a, const void *b)
{
	struct jump_t *pa = (struct jump_t *)a;
	struct jump_t *pb = (struct jump_t *)b;

	if (!pa->path)
		return 0;

	if (!pb->path)
		return 1;

	return (pa->rank > pb->rank);
}

static void
print_jump_table_header(void)
{
	char item[16]; /* BOLD and NC are 4 bytes each */
	snprintf(item, sizeof(item), "%s%s%s", BOLD,
		term_caps.unicode == 1 ? "•" : "*", NC);

	printf(_("%s First time access is displayed in days, while last "
		"time access is displayed in hours.\n"), item);
	printf(_("%s An asterisk next rank values means that the "
		"corresponding directory will not be removed despite its rank, "
		"either because it was visited in the last 24 hours, or because "
		"it is bookmarked, pinned, or currently active in some "
		"workspace.\n"), item);
	printf(_("%s A plus sign next rank values means that the "
		"corresponding directory is marked as permanent (it will not be "
		"removed).\n"), item);

	if (conf.min_jump_rank <= 0) {
		printf(_("%s MinJumpRank is set to %d: entries will not be removed "
			"from the database (no matter their rank).\n"),
			item, conf.min_jump_rank);
	} else {
		printf(_("%s Entries ranked below MinJumpRank (currently %d) will be "
			"removed at program exit.\n"), item, conf.min_jump_rank);
	}

	printf(_("\n%sVisits\tFirst\tLast\tRank\tDirectory%s\n"), BOLD, NC);
}

/* Print the jump database, field by field, incuding the current rank.
 * If REDUCE is greater than zero, each rank is divided by this value to keep
 * the total database rank below MaxJumpTotalRank.
 * NOW is the current time in seconds since epoch, and is used to calculate
 * each rank.
 * It always returns FUNC_SUCESS. */
static int
print_jump_table(const int reduce, const time_t now)
{
	if (jump_n == 0) {
		puts(_("jump: Database still empty"));
		return FUNC_SUCCESS;
	}

	HIDE_CURSOR;
	print_jump_table_header();

	int ranks_sum = 0;
	size_t visits_sum = 0;
	int max_rank = 0, max_visits = 0, max_first = 0, max_last = 0;

	struct jump_t *tmp_jump = xnmalloc(jump_n + 1, sizeof(struct jump_t));

	for (size_t i = 0; i < jump_n; i++) {
		if (!IS_VALID_JUMP_ENTRY(i)) {
			tmp_jump[i].path = (char *)NULL;
			continue;
		}

		int days_since_first = 0, hours_since_last = 0;
		int rank = rank_entry(i, now, &days_since_first, &hours_since_last);

		if (reduce) {
			const int tmp_rank = rank;
			rank = tmp_rank / reduce;
		}

		ranks_sum += rank;
		visits_sum += jump_db[i].visits;

		tmp_jump[i].path = jump_db[i].path;
		tmp_jump[i].keep = jump_db[i].keep;
		tmp_jump[i].rank = rank;
		tmp_jump[i].len = jump_db[i].len;
		tmp_jump[i].visits = jump_db[i].visits;
		tmp_jump[i].first_visit = (time_t)days_since_first;
		tmp_jump[i].last_visit = (time_t)hours_since_last;

		/* Get longest item for each field to correctly calculate padding. */
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

	/* Sort entries by rank (from less to more). */
	qsort(tmp_jump, jump_n, sizeof(*tmp_jump), rank_cmp);

	for (size_t i = 0; i < jump_n; i++) {
		if (!tmp_jump[i].path)
			continue;

		const char *color = (workspaces[cur_ws].path
		&& workspaces[cur_ws].path[1] == tmp_jump[i].path[1]
		&& strcmp(workspaces[cur_ws].path, tmp_jump[i].path) == 0) ? mi_c : df_c;

		const char *dir_color = get_directory_color(tmp_jump[i].path);
		const int keep = tmp_jump[i].keep;
		const char keep_char = keep == 1 ? '*'
			: ((keep == JUMP_ENTRY_PERMANENT)
			? JUMP_ENTRY_PERMANENT_CHR : 0);

		printf("%s%*zu\t%*jd\t%*jd\t%s%*d%s%s%c%s\t%c%s%s%s\n",
			color != uf_c ? color : df_c, max_visits, tmp_jump[i].visits,
			max_first, (intmax_t)tmp_jump[i].first_visit,
			max_last, (intmax_t)tmp_jump[i].last_visit,
		    conf.colorize == 1 ? BOLD : "", max_rank, tmp_jump[i].rank,
		    color != uf_c ? color : "",
		    keep == 1 ? li_c : (keep == 2 ? mi_c : ""),
		    keep > 0 ? keep_char : 0,
		    (keep > 0 && color != uf_c) ? color : "",
		    (conf.colorize == 0 && color == uf_c) ? '!' : 0,
		    dir_color, tmp_jump[i].path, df_c);
	}

	free(tmp_jump);

	printf(_("\nTotal rank: %d/%d\nTotal visits: %zu\n"), ranks_sum,
	    conf.max_jump_total_rank, visits_sum);

	UNHIDE_CURSOR;
	return FUNC_SUCCESS;
}

static int
purge_invalid_entries(void)
{
	int c = 0;

	struct stat a;
	for (size_t i = jump_n; i-- > 0;) {
		if (!IS_VALID_JUMP_ENTRY(i))
			continue;

		if (jump_db[i].keep == JUMP_ENTRY_PERMANENT)
			continue;

		if (stat(jump_db[i].path, &a) == -1) {
			printf("%s%s%s %s%s%s\n", mi_c, SET_MSG_PTR, df_c, uf_c,
				jump_db[i].path, df_c);
			jump_db[i].rank = JUMP_ENTRY_PURGED;
			c++;
		}
	}

	if (c == 0)
		puts(_("jump: No invalid entries"));
	else
		printf(_("\njump: Purged %d invalid %s\n"), c,
			c == 1 ? _("entry") : _("entries"));

	return FUNC_SUCCESS;
}

static int
purge_low_ranked_entries(const int limit)
{
	int c = 0;
	const time_t now = time(NULL);

	for (size_t i = jump_n; i-- > 0;) {
		if (!IS_VALID_JUMP_ENTRY(i))
			continue;

		if (jump_db[i].keep == JUMP_ENTRY_PERMANENT)
			continue;

		int days_since_first = 0, hours_since_last = 0;
		const int rank =
			rank_entry(i, now, &days_since_first, &hours_since_last);
		UNUSED(days_since_first); UNUSED(hours_since_last);

		if (rank < limit) {
			if (jump_db[i].keep == 1) {
				jump_db[i].keep = 0;
				continue;
			}
			jump_db[i].keep = 0;
			printf("%s%s%s %s (%d)\n", mi_c, SET_MSG_PTR, df_c,
				jump_db[i].path, rank);
			jump_db[i].rank = JUMP_ENTRY_PURGED;
			c++;
		}
	}

	if (c == 0)
		printf(_("jump: No entry ranked below %d\n"), limit);
	else
		printf(_("\njump: Purged %d %s\n"),
			c, c == 1 ? _("entry") : _("entries"));

	return FUNC_SUCCESS;
}

static int
purge_jump_database(const char *arg)
{
	if (!arg || !*arg)
		return purge_invalid_entries();

	if (!is_number(arg)) {
		puts(JUMP_USAGE);
		return FUNC_FAILURE;
	}

	const int n = atoi(arg);
	if (n < 0) {
		xerror(_("jump: '%s': Invalid value\n"), arg);
		return FUNC_FAILURE;
	}

	return purge_low_ranked_entries(n);
}

static int
check_jump_params(char **args, const time_t now, const int reduce)
{
	if (args[0][1] == 'e')
		return edit_jumpdb(args[1]);

	if (!args[1])
		return print_jump_table(reduce, now);

	if (IS_HELP(args[1])) {
		puts(_(JUMP_USAGE));
		return FUNC_SUCCESS;
	}

	if (*args[1] == '-' && strcmp(args[1], "--edit") == 0)
		return edit_jumpdb(args[2]);

	if (*args[1] == '-' && strcmp(args[1], "--purge") == 0)
		return purge_jump_database(args[2]);

	return (-1);
}

static int
mark_target_segment(char *str)
{
	const size_t len = strlen(str);
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
static char *
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
		const char p = *ret;
		*ret = '\0';

		if (strrchr(match, '/') != match) {
			*ret = p;
			return (char *)NULL;
		}

		*ret = p;
	}

	return ret;
}

static int
rank_tmp_entry(const struct jump_entry_t *entry, const time_t now,
	const int reduce, const char *query)
{
	/* 86400 = 60 secs / 60 misc / 24 hours */
	const int days_since_first = (int)(now - entry->first) / 86400;
	/* 3600 = 60 secs / 60 mins */
	const int hours_since_last = (int)(now - entry->last) / 3600;

	int keep = 0;
	int rank = calculate_base_credit(days_since_first, hours_since_last,
		entry->visits, &keep);

	rank += calculate_bonus_credit(entry->match, query, &keep);
	if (entry->keep == JUMP_ENTRY_PERMANENT)
		rank += PERMANENT_BONUS;

	UNUSED(keep);

	if (reduce > 0) {
		const int tmp_rank = rank;
		rank = tmp_rank / reduce;
	}

	return rank;
}

static int
check_dir(char *param, const int mode, int *ret)
{
	char *dir = (mode == NO_SUG_JUMP && strchr(param, '\\'))
		? unescape_str(param, 0) : param;

	struct stat a;
	if (lstat(dir, &a) == -1) {
		if (dir != param)
			free(dir);
		return 0;
	}

	*ret = mode == NO_SUG_JUMP
		? cd_function(dir, CD_NO_PRINT_ERROR)
		: save_jump_suggestion(param);

	if (dir != param)
		free(dir);

	return 1;
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
		return FUNC_FAILURE;
	}

	const time_t now = time(NULL);
	int reduce = 0;

	/* If the sum total of ranks is greater than max, divide each entry
	 * to make the sum total less than or equal to max. */
	if (jump_total_rank > conf.max_jump_total_rank && conf.max_jump_total_rank > 0)
		reduce = (jump_total_rank / conf.max_jump_total_rank) + 1;

	if (mode == NO_SUG_JUMP) {
		const int ret = check_jump_params(args, now, reduce);
		if (ret != -1)
			return ret;
	}

	enum jump jump_opt = NONE;

	switch (args[0][1]) {
	case 'c': jump_opt = JCHILD; break;
	case 'p': jump_opt = JPARENT; break;
	case 'l': jump_opt = JLIST; break;
	case '\0': jump_opt = NONE; break;
	default:
		xerror(_("jump: '%c': Invalid option\n"), args[0][1]);
		fprintf(stderr, "%s\n", _(JUMP_USAGE));
		return FUNC_FAILURE;
	}

	/* If ARG is an actual directory, just change to it. */
	int ret = 0;
	if (args[1] && !args[2] && check_dir(args[1], mode, &ret) == 1)
		return ret;

	/* Find the best ranked directory using ARGS as filter(s). */
	size_t j, match = 0;

	struct jump_entry_t *entry =
		xnmalloc(jump_n + 1, sizeof(struct jump_entry_t));

	for (size_t i = 1; args[i]; i++) {
		/* 1) Using the first parameter, get a list of matches in the
		 * database. */

		/* If the query string ends with a slash, we want this query
		 * string to match only the LAST segment of the path, and if it ends
		 * with a backslash instead, we want this query to match only the
		 * FIRST segment of the path. */
		const int segment = mark_target_segment(args[i]);

		if (match == 0) {
			for (j = jump_n; j-- > 0;) {
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
				entry[match].keep = (size_t)jump_db[j].keep;
				match++;
			}
		}

		/* 2) Once we have the list of matches, perform a reverse
		 * matching process, that is, excluding non-matches,
		 * using subsequent parameters. */
		else {
			for (j = match; j-- > 0;) {
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

	int found = 0, exit_status = FUNC_FAILURE, max = -1;
	char *best_ranked = (char *)NULL;

	for (j = match; j-- > 0;) {
		if (!entry[j].match)
			continue;

		found = 1;

		if (jump_opt == JLIST) {
			colors_list(entry[j].match, 0, 0 , 1);
			continue;
		}

		const int rank = rank_tmp_entry(&entry[j], now, reduce, args[args_n]);

		if (rank > max) {
			max = rank;
			best_ranked = entry[j].match;
		}
	}

	if (found == 0) {
		if (mode == NO_SUG_JUMP)
			puts(_("jump: No matches found"));
		exit_status = FUNC_FAILURE;
		goto END;
	}

	if (jump_opt == JLIST) {
		exit_status = FUNC_SUCCESS;
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
