/* jump.c -- functions for Kangaroo, the directory jumper function */

/*
 * This file is part of CliFM
 * 
 * Copyright (C) 2016-2023, L. Abramovich <johndoe.arch@outlook.com>
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
#include <sys/stat.h>
#include <time.h>

#if defined(__OpenBSD__) || defined(__NetBSD__) \
|| defined(__FreeBSD__) || defined(__APPLE__) || defined(__HAIKU__)
# include <inttypes.h> /* intmax_t */
#endif

#include "aux.h"
#include "checks.h"
#include "colors.h" /* get_dir_color() */
#include "file_operations.h"
#include "init.h"
#include "navigation.h"
#include "messages.h"
#include "misc.h"

/* Macros to calculate directories rank extra points */
#define BASENAME_BONUS 	300
#define BOOKMARK_BONUS  500
#define PINNED_BONUS    1000
#define WORKSPACE_BONUS 300
//#define VISIT_BONUS     100
#define VISIT_BONUS     200

/* Calculate last directory access credit */
#define JHOUR(n)  ((n) *= 4) /* Within last hour */
#define JDAY(n)   ((n) *= 2) /* Within last day */
#define JWEEK(n)  ((n) / 2)  /* Within last week */
#define JOLDER(n) ((n) / 4)  /* More than a week */

#define IS_VALID_JUMP_ENTRY(n) ((!jump_db[(n)].path || !*jump_db[(n)].path \
|| jump_db[(n)].rank == JUMP_ENTRY_PURGED) ? 0 : 1)

/* Calculate the rank as frecency. The algorithm is based
 * on Mozilla, zoxide, and z.lua. See:
 * "https://wiki.mozilla.org/User:Mconnor/Past/PlacesFrecency"
 * "https://github.com/ajeetdsouza/zoxide/wiki/Algorithm#aging"
 * "https://github.com/skywind3000/z.lua#aging" */
static int
rank_entry(const int i, const time_t now, int *days_since_first, int *hours_since_last)
{
//	int days_since_first = (int)(now - jump_db[i].first_visit) / 60 / 60 / 24;
//	int hours_since_last = (int)(now - jump_db[i].last_visit) / 60 / 60;
	*days_since_first = (int)(now - jump_db[i].first_visit) / 86400;
	*hours_since_last = (int)(now - jump_db[i].last_visit) / 3600;

	int rank;
	rank = *days_since_first > 1
		   ? ((int)jump_db[i].visits * VISIT_BONUS) / *days_since_first
		   : (int)jump_db[i].visits * VISIT_BONUS;

	int tmp_rank = rank;
	if (*hours_since_last == 0) { /* Last hour */
		rank = JHOUR(tmp_rank);
		jump_db[i].keep = 1;
	} else if (*hours_since_last <= 24) { /* Last day */
		rank = JDAY(tmp_rank);
		jump_db[i].keep = 1;
	} else if (*hours_since_last <= 168) { /* Last week */
		rank = JWEEK(tmp_rank);
	} else { /* More than a week */
		rank = JOLDER(tmp_rank);
	}

	int j = (int)bm_n;//, bpw = 0; /* Bookmarked, pinned or workspace */
	while (--j >= 0) {
		if (bookmarks[j].path && bookmarks[j].path[1] == jump_db[i].path[1]
		&& strcmp(bookmarks[j].path, jump_db[i].path) == 0) {
			rank += BOOKMARK_BONUS;
			jump_db[i].keep = 1;
			break;
		}
	}

	if (pinned_dir && pinned_dir[1] == jump_db[i].path[1]
	&& strcmp(pinned_dir, jump_db[i].path) == 0) {
		rank += PINNED_BONUS;
		jump_db[i].keep = 1;
	}

	j = MAX_WS;
	while (--j >= 0) {
		if (workspaces[j].path && workspaces[j].path[1] == jump_db[i].path[1]
		&& strcmp(jump_db[i].path, workspaces[j].path) == 0) {
			rank += WORKSPACE_BONUS;
			jump_db[i].keep = 1;
			break;
		}
	}

	return rank;
}

static inline void
free_jump_database(void)
{
	int i = (int)jump_n;

	while (--i >= 0)
		free(jump_db[i].path);

	free(jump_db);
	jump_db = (struct jump_t *)NULL;
	jump_n = 0;
}

static inline int
write_jump_entry(const char *dir)
{
	jump_db = (struct jump_t *)xrealloc(jump_db, (jump_n + 2) * sizeof(struct jump_t));
	jump_db[jump_n].visits = 1;
	time_t now = time(NULL);
	jump_db[jump_n].first_visit = now;
	jump_db[jump_n].last_visit = now;
	jump_db[jump_n].rank = 0;
	jump_db[jump_n].keep = 0;

	jump_db[jump_n].len = strlen(dir);
	jump_db[jump_n].path = savestring(dir, jump_db[jump_n].len);
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

/* Add DIR to the jump database */
int
add_to_jumpdb(const char *dir)
{
	if (xargs.no_dirjump == 1 || !dir || !*dir)
		return EXIT_FAILURE;

	if (!jump_db) {
		jump_db = (struct jump_t *)xnmalloc(1, sizeof(struct jump_t));
		jump_n = 0;
	}

	int i = (int)jump_n, new_entry = 1;
	while (--i >= 0) {
		if (!IS_VALID_JUMP_ENTRY(i))
			continue;

		/* Jump entries are all absolute paths, so that they all start with
		 * a slash. Let's start comparing then the second char */
		if (dir[1] == jump_db[i].path[1] && strcmp(jump_db[i].path, dir) == 0) {
			jump_db[i].visits++;
			jump_db[i].last_visit = time(NULL);
			new_entry = 0;
			break;
		}
	}

	if (new_entry == 0)
		return EXIT_SUCCESS;

	return write_jump_entry(dir);
}

/* Store the jump database into a file */
void
save_jumpdb(void)
{
	if (xargs.no_dirjump == 1 || !config_ok || !config_dir || !jump_db
	|| jump_n == 0)
		return;

	char *jump_file = (char *)xnmalloc(config_dir_len + 12, sizeof(char));
	sprintf(jump_file, "%s/jump.clifm", config_dir);

	FILE *fp = fopen(jump_file, "w+");
	if (!fp) {
		free(jump_file);
		return;
	}

	int i, reduce = 0, total_rank = 0;
	time_t now = time(NULL);

	/* Calculate both total rank sum and rank for each entry */
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
	 * and then write entries into the database */
	if (total_rank > conf.max_jump_total_rank)
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
#ifdef JUMP_MONITOR
			printf("jump: %s: Forgotten directory (%d)\n", jump_db[i].path, jump_db[i].rank);
			fflush(stdout);
#endif
			continue;
		}

		fprintf(fp, "%zu:%jd:%jd:%s\n", jump_db[i].visits, (intmax_t)jump_db[i].first_visit,
			(intmax_t)jump_db[i].last_visit, jump_db[i].path);
	}

	fprintf(fp, "@%d\n", total_rank);
	fclose(fp);
	free(jump_file);
}

int
edit_jumpdb(void)
{
	if (!config_ok || !config_dir)
		return EXIT_FAILURE;

	save_jumpdb();

	char *jump_file = (char *)xnmalloc(config_dir_len + 12, sizeof(char));
	sprintf(jump_file, "%s/jump.clifm", config_dir);

	struct stat attr;
	if (stat(jump_file, &attr) == -1) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "jump: %s: %s\n", jump_file, strerror(errno));
		free(jump_file);
		return EXIT_FAILURE;
	}

	time_t mtime_bfr = (time_t)attr.st_mtime;

	char *cmd[] = {"o", jump_file, NULL};
	open_in_foreground = 1;
	open_function(cmd);
	open_in_foreground = 0;

	stat(jump_file, &attr);

	if (mtime_bfr == (time_t)attr.st_mtime) {
		free(jump_file);
		return EXIT_SUCCESS;
	}

	if (jump_db)
		free_jump_database();

	load_jumpdb();
	free(jump_file);
	return EXIT_SUCCESS;
}

/* Save jump entry into the suggestions buffer */
static int
save_suggestion(char *str)
{
	free(jump_suggestion);
	size_t len = strlen(str);

	int slash = 0;
	if (len > 0 && str[len - 1] == '/')
		slash = 1;

	jump_suggestion = xnmalloc(len + (slash ? 1 : 2), sizeof(char));
	if (!slash)
		sprintf(jump_suggestion, "%s/", str);
	else
		strcpy(jump_suggestion, str);

	return EXIT_SUCCESS;
}

static char *
_get_dir_color(const char *filename, const struct stat a)
{
	if (check_file_access(a.st_mode, a.st_uid, a.st_gid) == 0)
		return nd_c;

	if (S_ISLNK(a.st_mode)) {
		char *linkname = realpath(filename, (char *)NULL);
		if (linkname) {
			free(linkname);
			return ln_c;
		}
		return or_c;
	}

	return get_dir_color(filename, a.st_mode, a.st_nlink);
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
		"it is bookmarked, pinned, or currently active in some workspace.\n"), item);

	if (conf.min_jump_rank <= 0) {
		printf(_("%s MinJumpRank is set to %d: entries will not be removed "
			"from the database (no matter their rank).\n"), item, conf.min_jump_rank);
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

	print_jump_table_header();

	size_t i;
	int ranks_sum = 0, visits_sum = 0;
	int max_rank = 0, max_visits = 0, max_first = 0, max_last = 0;
	int max_order = DIGINUM(jump_n);

	struct jump_t *tmp_jump = (struct jump_t *)xnmalloc(jump_n + 1, sizeof(struct jump_t));

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

		char *dir_color = color == uf_c ? uf_c : _get_dir_color(tmp_jump[i].path, a);

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
		printf(_("\njump: Purged %d invalid %s\n"), c, c == 1 ? _("entry") : _("entries"));

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
		printf(_("\njump: Purged %d %s\n"), c, c == 1 ? _("entry") : _("entries"));

	return EXIT_SUCCESS;
}

static int
_purge_jumpdb(char *arg)
{
	if (!arg || !*arg)
		return purge_invalid_entries();

	if (!is_number(arg)) {
		puts(JUMP_USAGE);
		return EXIT_FAILURE;
	}

	int n = atoi(arg);
	if (n < 0) {
		fprintf(stderr, _("jump: %s: Invalid value\n"), arg);
		return EXIT_FAILURE;
	}

	return purge_low_ranked_entries(n);
}

/* Jump into best ranked directory matched by ARGS */
int
dirjump(char **args, int mode)
{
	if (xargs.no_dirjump == 1 && mode == NO_SUG_JUMP) {
		printf(_("%s: Directory jumper function disabled\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	time_t now = time(NULL);
	int reduce = 0;

	/* If the sum total of ranks is greater than max, divide each entry
	 * to make the sum total less than or equal to max */
	if (jump_total_rank > conf.max_jump_total_rank)
		reduce = (jump_total_rank / conf.max_jump_total_rank) + 1;

	/* If no parameter (and no 'je'), print the list of entries in the jump
	 * database together with the corresponding information */
	if (mode == NO_SUG_JUMP) {
		if (!args[1] && args[0][1] != 'e')
			return print_jump_table(reduce, now);

		if (args[1] && IS_HELP(args[1])) {
			puts(_(JUMP_USAGE));
			return EXIT_SUCCESS;
		}

		if (args[1] && *args[1] == '-' && strcmp(args[1], "--edit") == 0)
			return edit_jumpdb();

		if (args[1] && *args[1] == '-' && strcmp(args[1], "--purge") == 0)
			return _purge_jumpdb(args[2]);
	}

	enum jump jump_opt = NONE;

	switch (args[0][1]) {
	case 'e': return edit_jumpdb();
	case 'c': jump_opt = JCHILD; break;
	case 'p': jump_opt = JPARENT; break;
	case 'o': jump_opt = JORDER; break;
	case 'l': jump_opt = JLIST; break;
	case '\0': jump_opt = NONE; break;
	default:
		fprintf(stderr, _("jump: '%c': Invalid option\n"), args[0][1]);
		fprintf(stderr, "%s\n", _(JUMP_USAGE));
		return EXIT_FAILURE;
	}

	if (jump_opt == JORDER) {
		if (!args[1]) {
			if (mode == NO_SUG_JUMP)
				fprintf(stderr, "%s\n", _(JUMP_USAGE));
			return EXIT_FAILURE;
		}

		if (!is_number(args[1])) {
			if (mode == NO_SUG_JUMP)
				return cd_function(args[1], CD_PRINT_ERROR);
			else
				return EXIT_FAILURE;
		} else {
			int int_order = atoi(args[1]);
			if (int_order <= 0 || int_order > (int)jump_n) {
				if (mode == NO_SUG_JUMP) {
					fprintf(stderr, _("jump: %s: No such order number\n"), args[1]);
				}
				return EXIT_FAILURE;
			}

			if (mode == NO_SUG_JUMP)
				return cd_function(jump_db[int_order - 1].path, CD_PRINT_ERROR);
			return save_suggestion(jump_db[int_order - 1].path);
		}
	}

	/* If ARG is an actual directory, just cd into it */
	struct stat attr;
	if (args[1] && !args[2] && lstat(args[1], &attr) != -1) {
		if (mode == NO_SUG_JUMP)
			return cd_function(args[1], CD_PRINT_ERROR);
		return save_suggestion(args[1]);
	}

	/* Jump into a visited directory using ARGS as filter(s) */
	size_t i;
	int j, match = 0;
	char **matches = (char **)xnmalloc(jump_n + 1, sizeof(char *));
	char **needles = (char **)xnmalloc(jump_n + 1, sizeof(char *));
	size_t *visits = (size_t *)xnmalloc(jump_n + 1, sizeof(size_t));
	time_t *first = (time_t *)xnmalloc(jump_n + 1, sizeof(time_t));
	time_t *last = (time_t *)xnmalloc(jump_n + 1, sizeof(time_t));
	int last_segment = 0, first_segment = 0;

	for (i = 1; args[i]; i++) {
		/* 1) Using the first parameter, get a list of matches in the
		 * database */

		/* If the query string ends with a slash, we want this query
		 * string to match only the last segment of the path (i.e.,
		 * there must be no slash after the match) */
		size_t _len = strlen(args[i]);
		if (_len > 0 && args[i][_len - 1] == '/') {
			args[i][_len - 1] = '\0';
			last_segment = 1;
			first_segment = 0;
		} else if (_len > 0 && args[i][_len - 1] == '\\') {
			args[i][_len - 1] = '\0';
			last_segment = 0;
			first_segment = 1;
		} else {
			last_segment = 0;
			first_segment = 0;
		}

		if (!match) {
			j = (int)jump_n;
			while (--j >= 0) {
				if (!IS_VALID_JUMP_ENTRY(j))
					continue;

				/* Pointer to the beginning of the search str in the
				 * jump entry. Used to search for subsequent search
				 * strings starting from this position in the entry
				 * and not before */
				char *needle = conf.case_sens_dirjump
							? strstr(jump_db[j].path, args[i])
							: strcasestr(jump_db[j].path, args[i]);

				if (!needle || (last_segment && strchr(needle, '/')))
					continue;

				if (first_segment) {
					char p = *needle;
					*needle = '\0';
					if (strrchr(jump_db[j].path, '/') != jump_db[j].path) {
						*needle = p;
						continue;
					}
					*needle = p;
				}

				/* Exclue CWD */
				if (workspaces[cur_ws].path && jump_db[j].path[1] == workspaces[cur_ws].path[1]
				&& strcmp(jump_db[j].path, workspaces[cur_ws].path) == 0)
					continue;

				int exclude = 0;
				/* Filter matches according to parent or
				 * child options */
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

				case NONE: /* fallthrough */
				default: break;
				}

				if (exclude)
					continue;

				visits[match] = jump_db[j].visits;
				first[match] = jump_db[j].first_visit;
				last[match] = jump_db[j].last_visit;
				needles[match] = needle;
				matches[match++] = jump_db[j].path;
			}
		}

		/* 2) Once we have the list of matches, perform a reverse
		 * matching process, that is, excluding non-matches,
		 * using subsequent parameters */
		else {
			j = (int)match;
			while (--j >= 0) {
				if (!matches[j] || !*matches[j]) {
					matches[j] = (char *)NULL;
					continue;
				}

				char *_needle = conf.case_sens_dirjump
						? strstr(needles[j] + 1, args[i])
						: strcasestr(needles[j] + 1, args[i]);

				if (!_needle || (last_segment && strchr(_needle, '/'))){
					matches[j] = (char *)NULL;
					continue;
				}

				if (first_segment) {
					char p = *_needle;
					*_needle = '\0';
					if (strrchr(matches[j], '/') != matches[j]) {
						*_needle = p;
						matches[j] = (char *)NULL;
						continue;
					}
					*_needle = p;
				}

				/* Update the needle for the next search string */
				needles[j] = _needle;
			}
		}
	}

	/* 3) If something remains, we have at least one match */

	/* 4) Further filter the list of matches by frecency, so that only
	 * the best ranked directory will be returned */

	int found = 0, exit_status = EXIT_FAILURE,
	    best_ranked = 0, max = -1, k;

	j = match;
	while (--j >= 0) {
		if (!matches[j])
			continue;

		found = 1;

		if (jump_opt == JLIST) {
			printf("%s\n", matches[j]);
		} else {
//			int days_since_first = (int)(now - first[j]) / 60 / 60 / 24;
			int days_since_first = (int)(now - first[j]) / 86400;

			/* Calculate the rank as frecency. The algorithm is based
			 * on Mozilla, zoxide, and z.lua. See:
			 * "https://wiki.mozilla.org/User:Mconnor/Past/PlacesFrecency"
			 * "https://github.com/ajeetdsouza/zoxide/wiki/Algorithm#aging"
			 * "https://github.com/skywind3000/z.lua#aging" */
			int rank;
			rank = days_since_first > 0 ? ((int)visits[j] * VISIT_BONUS)
					/ days_since_first : ((int)visits[j] * VISIT_BONUS);

//			int hours_since_last = (int)(now - last[j]) / 60 / 60;
			int hours_since_last = (int)(now - last[j]) / 3600;

			/* Credit or penalty based on last directory access */
			int tmp_rank = rank;
			if (hours_since_last == 0) /* Last hour */
				rank = JHOUR(tmp_rank);
			else if (hours_since_last <= 24) /* Last day */
				rank = JDAY(tmp_rank);
			else if (hours_since_last <= 168) /* Last week */
				rank = JWEEK(tmp_rank);
			else /* More than a week */
				rank = JOLDER(tmp_rank);

			/* Matches in directory basename have extra credit */
			char *tmp = strrchr(matches[j], '/');
			if (tmp && *(++tmp)) {
				if (strstr(tmp, args[args_n]))
					rank += BASENAME_BONUS;
			}

			/* Bookmarked directories have extra credit */
			k = (int)bm_n;
			while (--k >= 0) {
				if (bookmarks[k].path && bookmarks[k].path[1] == matches[j][1]
				&& strcmp(bookmarks[k].path, matches[j]) == 0) {
					rank += BOOKMARK_BONUS;
					break;
				}
			}

			if (pinned_dir && pinned_dir[1] == matches[j][1]
			&& strcmp(pinned_dir, matches[j]) == 0)
				rank += PINNED_BONUS;

			k = MAX_WS;
			while (--k >= 0) {
				if (workspaces[k].path && workspaces[k].path[1] == matches[j][1]
				&& strcmp(workspaces[k].path, matches[j]) == 0) {
					rank += WORKSPACE_BONUS;
					break;
				}
			}

			if (reduce) {
				tmp_rank = rank;
				rank = tmp_rank / reduce;
			}

			if (rank > max) {
				max = rank;
				best_ranked = j;
			}
		}
	}

	if (!found) {
		if (mode == NO_SUG_JUMP)
			printf(_("jump: No matches found\n"));
		exit_status = EXIT_FAILURE;
	} else {
		if (jump_opt != JLIST) {
			if (mode == NO_SUG_JUMP)
				exit_status = cd_function(matches[best_ranked], CD_PRINT_ERROR);
			else
				exit_status = save_suggestion(matches[best_ranked]);
		}
	}

	free(matches);
	free(needles);
	free(first);
	free(last);
	free(visits);
	return exit_status;
}

/* This function is called if the autojump option is enabled. If the
 * first word in CMD is not a program in PATH, append the j command to
 * the current command line and run the dirjump function */
/*
int
run_autojump(char **cmd)
{
	if (!cmd || !cmd[0] || !*cmd[0] || is_internal_c(cmd[0]))
		return -1;

	char *ret = get_cmd_path(cmd[0]);
	if (ret) {
		free(ret);
		return -1;
	}

	int i;

	char **__cmd = (char **)xnmalloc(args_n + 3, sizeof(char *));
	__cmd[0] = (char *)xnmalloc(2, sizeof(char));
	*__cmd[0] = 'j';
	__cmd[0][1] = '\0';

	for (i = 0; i <= (int)args_n; i++) {
		__cmd[i + 1] = (char *)xnmalloc(strlen(cmd[i]) + 1,
						sizeof(char));
		strcpy(__cmd[i + 1], cmd[i]);
	}

	__cmd[args_n + 2] = (char *)NULL;

	args_n++;
	exit_code = dirjump(__cmd, NO_SUG_JUMP);
	args_n--;

	i = (int)args_n + 2;
	while (--i >= 0)
		free(__cmd[i]);
	free(__cmd);

	return exit_code;
} */
