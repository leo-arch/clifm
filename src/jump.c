/* jump.c -- functions for Kangaroo, the directory jumper function */

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

#include <time.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

#include "aux.h"
#include "file_operations.h"
#include "checks.h"
#include "navigation.h"
#include "init.h"

int add_to_jumpdb(const char *dir)
{
	if (xargs.no_dirjump == 1 || !dir || !*dir)
		return EXIT_FAILURE;

	if (!jump_db) {
		jump_db = (struct jump_t *)xnmalloc(1, sizeof(struct jump_t));
		jump_n = 0;
	}

	int i = (int)jump_n, new_entry = 1;
	while (--i >= 0) {

		if (dir[1] == jump_db[i].path[1]
		&& strcmp(jump_db[i].path, dir) == 0) {
			jump_db[i].visits++;
			jump_db[i].last_visit = time(NULL);
			new_entry = 0;
			break;
		}
	}

	if (!new_entry)
	return EXIT_SUCCESS;

	jump_db = (struct jump_t *)xrealloc(jump_db, (jump_n + 2)
									* sizeof(struct jump_t));
	jump_db[jump_n].visits = 1;
	time_t now = time(NULL);
	jump_db[jump_n].first_visit = now;
	jump_db[jump_n].last_visit = now;
	jump_db[jump_n].rank = 0;
	jump_db[jump_n].keep = 0;
	jump_db[jump_n++].path = savestring(dir, strlen(dir));

	jump_db[jump_n].path = (char *)NULL;
	jump_db[jump_n].visits = 0;
	jump_db[jump_n].rank = 0;
	jump_db[jump_n].keep = 0;
	jump_db[jump_n].first_visit = -1;
	jump_db[jump_n].last_visit = -1;

	return EXIT_SUCCESS;
}

/* Store the jump database into a file */
void save_jumpdb(void) {
	if (xargs.no_dirjump == 1 || !config_ok || !CONFIG_DIR
	|| !jump_db || jump_n == 0)
		return;

	char *JUMP_FILE = (char *)xnmalloc(strlen(CONFIG_DIR) + 10, sizeof(char));
	sprintf(JUMP_FILE, "%s/jump.cfm", CONFIG_DIR);

	FILE *fp = fopen(JUMP_FILE, "w+");

	if (!fp) {
		free(JUMP_FILE);
		return;
	}

	int i, reduce = 0, tmp_rank = 0, total_rank = 0;
	time_t now = time(NULL);

	/* Calculate both total rank sum and rank for each entry */
	i = (int)jump_n;
	while (--i >= 0) {

		int days_since_first = (int)(now - jump_db[i].first_visit) / 60 / 60 / 24;
		int rank = days_since_first > 1 ? ((int)jump_db[i].visits * 100)
							/ days_since_first : ((int)jump_db[i].visits * 100);

		int hours_since_last = (int)(now - jump_db[i].last_visit) / 60 / 60;

		tmp_rank = rank;
		if (hours_since_last == 0)          /* Last hour */
			rank = JHOUR(tmp_rank);
		else if (hours_since_last <= 24)    /* Last day */
			rank = JDAY(tmp_rank);
		else if (hours_since_last <= 168)   /* Last week */
			rank = JWEEK(tmp_rank);
		else                                 /* More than a week */
			rank = JOLDER(tmp_rank);

		/* Do not remove bookmarked, pinned, or workspaced directories */
		/* Add bonus points */
		int j = (int)bm_n;
		while (--j >= 0) {
			if (bookmarks[j].path[1] == jump_db[i].path[1]
			&& strcmp(bookmarks[j].path, jump_db[i].path) == 0) {
				jump_db[i].rank += BOOKMARK_BONUS;
				jump_db[i].keep = 1;
				break;
			}
		}

		if (pinned_dir && pinned_dir[1] == jump_db[i].path[1]
		&& strcmp(pinned_dir, jump_db[i].path) == 0) {
			jump_db[i].rank += PINNED_BONUS;
			jump_db[i].keep = 1;
		}

		j = MAX_WS;
		while (--j >= 0) {
			if (ws[j].path && ws[j].path[1] == jump_db[i].path[1]
			&& strcmp(jump_db[i].path, ws[j].path) == 0) {
				jump_db[i].rank += WORKSPACE_BONUS;
				jump_db[i].keep = 1;
				break;
			}
		}

		jump_db[i].rank = rank;
		total_rank += rank;
	}

	/* Once we have the total rank, check if we need to recude ranks,
	 * and write entries into the database */
	if (total_rank > max_jump_total_rank)
		reduce = (total_rank / max_jump_total_rank) + 1;

	int jump_num = 0;

	for (i = 0; i < (int)jump_n; i++) {

		if (reduce) {
			tmp_rank = jump_db[i].rank;
			jump_db[i].rank = tmp_rank / reduce;
		}

		/* Forget directories ranked below MIN_JUMP_RANK */
		if (jump_db[i].keep != 1 && (jump_db[i].rank <= 0
		|| jump_db[i].rank < min_jump_rank))
			continue;

		jump_num++;

		fprintf(fp, "%zu:%ld:%ld:%s\n", jump_db[i].visits,
				jump_db[i].first_visit, jump_db[i].last_visit,
				jump_db[i].path);
	}

	fprintf(fp, "@%d\n", total_rank);

	fclose(fp);
	free(JUMP_FILE);
}

int edit_jumpdb(void)
{
	if (!config_ok || !CONFIG_DIR)
		return EXIT_FAILURE;

	save_jumpdb();

	char *JUMP_FILE = (char *)xnmalloc(strlen(CONFIG_DIR) + 10,
									   sizeof(char));
	sprintf(JUMP_FILE, "%s/jump.cfm", CONFIG_DIR);

	struct stat attr;

	if (stat(JUMP_FILE, &attr) == -1) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, JUMP_FILE,
				strerror(errno));
		free(JUMP_FILE);
		return EXIT_FAILURE;
	}

	time_t mtime_bfr = (time_t)attr.st_mtime;

	char *cmd[] = { "o", JUMP_FILE, NULL };
	open_function(cmd);

	stat(JUMP_FILE, &attr);

	if (mtime_bfr == (time_t)attr.st_mtime) {
		free(JUMP_FILE);
		return EXIT_SUCCESS;
	}

	if (jump_db) {
		int i = (int)jump_n;

		while (--i >= 0)
			free(jump_db[i].path);

		free(jump_db);
		jump_db = (struct jump_t *)NULL;
		jump_n = 0;
	}

	load_jumpdb();

	free(JUMP_FILE);

	return EXIT_SUCCESS;
}

/* Jump into best ranked directory matched by ARGS */
int dirjump(char **args) {
	if (xargs.no_dirjump == 1) {
		printf(_("%s: Directory jumper function disabled\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	time_t now = time(NULL);

	int reduce = 0;

	/* If the sum total of ranks is greater than max, divide each entry
	 * to make the sum total less than or equal to max */
	if (jump_total_rank > max_jump_total_rank)
		reduce = (jump_total_rank / max_jump_total_rank) + 1;

	/* If no parameter, print the list of entries in the jump
	 * database together with the corresponding information */
	if (!args[1] && args[0][1] != 'e') {

		if (!jump_n) {
			printf("%s: Database still empty\n", PROGRAM_NAME);
			return EXIT_SUCCESS;
		}

		puts(_("NOTE: First time access is displayed in days, while last "
			 "time access is displayed in hours"));
		puts(_("NOTE 2: An asterisk next rank values means that the "
			 "corresponding directory is bookmarked, pinned, or currently "
			 "used in some workspace\n"));
		puts(_("Order\tVisits\tFirst\tLast\tRank\tDirectory"));

		size_t i, ranks_sum = 0, visits_sum = 0;

		for (i = 0; i < jump_n; i++) {

			int days_since_first = (int)(now - jump_db[i].first_visit) / 60 / 60 / 24;
			int hours_since_last = (int)(now - jump_db[i].last_visit) / 60 / 60;

			int rank;
			rank = days_since_first > 1
				   ? ((int)jump_db[i].visits * 100) / days_since_first
				   : (int)jump_db[i].visits * 100;

			int tmp_rank = rank;
			if (hours_since_last == 0)          /* Last hour */
				rank = JHOUR(tmp_rank);
			else if (hours_since_last <= 24)    /* Last day */
				rank = JDAY(tmp_rank);
			else if (hours_since_last <= 168)   /* Last week */
				rank = JWEEK(tmp_rank);
			else                                /* More than a week */
				rank = JOLDER(tmp_rank);

			int j = (int)bm_n, BPW = 0; /* Bookmarked, pinned or workspace */
			while (--j >= 0) {
				if (bookmarks[j].path[1] == jump_db[i].path[1]
				&& strcmp(bookmarks[j].path, jump_db[i].path) == 0) {
					rank += BOOKMARK_BONUS;
					BPW = 1;
					break;
				}
			}

			if (pinned_dir && pinned_dir[1] == jump_db[i].path[1]
			&& strcmp(pinned_dir, jump_db[i].path) == 0) {
				rank += PINNED_BONUS;
				BPW = 1;
			}

			j = MAX_WS;
			while (--j >= 0) {
				if (ws[j].path && ws[j].path[1] == jump_db[i].path[1]
				&& strcmp(jump_db[i].path, ws[j].path) == 0) {
					rank += WORKSPACE_BONUS;
					BPW = 1;
					break;
				}
			}

			if (reduce) {
				tmp_rank = rank;
				rank = tmp_rank / reduce;
			}

			ranks_sum += rank;
			visits_sum += jump_db[i].visits;

			if (ws[cur_ws].path[1] == jump_db[i].path[1]
			&& strcmp(ws[cur_ws].path, jump_db[i].path) == 0) {
				printf("  %s%zu\t %zu\t %d\t %d\t%d%c\t%s%s \n", mi_c,
					   i + 1, jump_db[i].visits, days_since_first,
					   hours_since_last, rank, BPW ? '*' : 0,
					   jump_db[i].path, df_c);
			}

			else
				printf("  %zu\t %zu\t %d\t %d\t%d%c\t%s \n", i + 1,
					   jump_db[i].visits, days_since_first,
					   hours_since_last, rank,
					   BPW ? '*' : 0, jump_db[i].path);
		}

		printf("\nTotal rank: %zu/%d\nTotal visits: %zu\n", ranks_sum,
				max_jump_total_rank, visits_sum);

		return EXIT_SUCCESS;
	}

	if (args[1] && *args[1] == '-' && strcmp(args[1], "--help") == 0) {
		puts(_("Usage: j, jc, jp, jl [STRING ...], jo [NUM], je"));
		return EXIT_SUCCESS;
	}

	enum jump jump_opt = none;

	switch(args[0][1]) {

	case 'e': return edit_jumpdb();
	case 'c': jump_opt = jchild; break;
	case 'p': jump_opt = jparent; break;
	case 'o': jump_opt = jorder; break;
	case 'l': jump_opt = jlist; break;
	case '\0':
		jump_opt = none;
		break;

	default:
		fprintf(stderr, _("%s: '%c': Invalid option\n"), PROGRAM_NAME,
				args[0][1]);
		fputs(_("Usage: j, jc, jp, jl [STRING ...], jo [NUM], je\n"),
			  stderr);
		return EXIT_FAILURE;
	break;
	}

	if (jump_opt == jorder) {

		if (!args[1]) {
			fputs(_("Usage: j, jc, jp, jl [STRING ...], jo [NUM], je\n"),
				  stderr);
			return EXIT_FAILURE;
		}

		if (!is_number(args[1]))
				return cd_function(args[1]);

		else {

			int int_order = atoi(args[1]);
			if (int_order <= 0 || int_order > (int)jump_n) {
				fprintf(stderr, _("%s: %d: No such order number\n"),
						PROGRAM_NAME, int_order);
				return EXIT_FAILURE;
			}

			return cd_function(jump_db[int_order - 1].path);
		}
	}

	/* If ARG is an actual directory, just cd into it */
	struct stat attr;

	if (!args[2] && lstat(args[1], &attr) != -1)
		return cd_function(args[1]);

	/* Jump into a visited directory using ARGS as filter(s) */
	size_t i;
	int j, match = 0;

	char **matches = (char **)xnmalloc(jump_n + 1, sizeof(char *));
	size_t *visits = (size_t *)xnmalloc(jump_n + 1, sizeof(size_t));
	time_t *first = (time_t *)xnmalloc(jump_n + 1, sizeof(time_t));
	time_t *last = (time_t *)xnmalloc(jump_n + 1, sizeof(time_t));

	for (i = 1; args[i]; i++) {

		/* 1) Using the first parameter, get a list of matches in the
		 * database */
		if (!match) {

			j = (int)jump_n;
			while (--j >= 0) {

				if (case_sens_dirjump ? !strstr(jump_db[j].path, args[i])
				: !strcasestr(jump_db[j].path, args[i]))
					continue;

				/* Exclue CWD */
				if (jump_db[j].path[1] == ws[cur_ws].path[1]
				&& strcmp(jump_db[j].path, ws[cur_ws].path) == 0)
					continue;

				int exclude = 0;

				/* Filter matches according to parent or
				 * child options */
				switch(jump_opt) {
				case jparent:
					if (!strstr(ws[cur_ws].path, jump_db[j].path))
						exclude = 1;
				break;

				case jchild:
					if (!strstr(jump_db[j].path, ws[cur_ws].path))
						exclude = 1;

				case none:
				default:
					break;
				}

				if (exclude)
					continue;

				visits[match] = jump_db[j].visits;
				first[match] = jump_db[j].first_visit;
				last[match] = jump_db[j].last_visit;
				matches[match++] = jump_db[j].path;
			}
		}

		/* 2) Once we have the list of matches, perform a reverse
		 * matching process, that is, excluding non-matches,
		 * using subsequent parameters */
		else {
			j = (int)match;
			while (--j >= 0) {

				if (!matches[j] || !*matches[j]
				|| (case_sens_dirjump ? !strstr(matches[j], args[i])
				: !strcasestr(matches[j], args[i]))) {

					matches[j] = (char *)NULL;
					continue;
				}
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

		if (jump_opt == jlist)
			printf("%s\n", matches[j]);

		else {
			int days_since_first = (int)(now - first[j]) / 60 / 60 / 24;

			/* Calculate the rank as frecency. The algorithm is based
			 * on Mozilla, zoxide, and z.lua. See:
			 * "https://wiki.mozilla.org/User:Mconnor/Past/PlacesFrecency"
			 * "https://github.com/ajeetdsouza/zoxide/wiki/Algorithm#aging"
			 * "https://github.com/skywind3000/z.lua#aging" */
			int rank;
			rank = days_since_first > 0 ? ((int)visits[j] * 100) / days_since_first
							: ((int)visits[j] * 100);

			int hours_since_last = (int)(now - last[j]) / 60 / 60;

			/* Credit or penalty based on last directory access */
			int tmp_rank = rank;
			if (hours_since_last == 0)          /* Last hour */
				rank = JHOUR(tmp_rank);
			else if (hours_since_last <= 24)    /* Last day */
				rank = JDAY(tmp_rank);
			else if (hours_since_last <= 168)   /* Last week */
				rank = JWEEK(tmp_rank);
			else                                /* More than a week */
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
				if (bookmarks[k].path[1] == matches[j][1]
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
				if (ws[k].path && ws[k].path[1] == matches[j][1]
				&& strcmp(ws[k].path, matches[j]) == 0) {
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
		printf(_("%s: No matches found\n"), PROGRAM_NAME);
		exit_status = EXIT_FAILURE;
	}

	else if (jump_opt != jlist)
		exit_status = cd_function(matches[best_ranked]);

	free(matches);
	free(first);
	free(last);
	free(visits);

	return exit_status;
}
