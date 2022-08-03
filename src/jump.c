/* jump.c -- functions for Kangaroo, the directory jumper function */

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

#include "helpers.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "aux.h"
#include "checks.h"
#include "file_operations.h"
#include "init.h"
#include "navigation.h"
#include "messages.h"
#include "misc.h"

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

	jump_db[jump_n].path = savestring(dir, strlen(dir));
	jump_n++;

	jump_db[jump_n].path = (char *)NULL;
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
		if (dir[1] == jump_db[i].path[1] && strcmp(jump_db[i].path, dir) == 0) {
			jump_db[i].visits++;
			jump_db[i].last_visit = time(NULL);
			new_entry = 0;
			break;
		}
	}

	if (!new_entry)
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

//	char *jump_file = (char *)xnmalloc(config_dir_len + 10, sizeof(char));
//	sprintf(jump_file, "%s/jump.cfm", config_dir);
	char *jump_file = (char *)xnmalloc(config_dir_len + 12, sizeof(char));
	sprintf(jump_file, "%s/jump.clifm", config_dir);

	FILE *fp = fopen(jump_file, "w+");
	if (!fp) {
		free(jump_file);
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

		/* Do not remove directories visited in the last 24 hours, no
		 * matter what their rank is */
		tmp_rank = rank;
		if (hours_since_last == 0) {			/* Last hour */
			rank = JHOUR(tmp_rank);
			jump_db[i].keep = 1;
		} else if (hours_since_last <= 24) {	/* Last day */
			rank = JDAY(tmp_rank);
			jump_db[i].keep = 1;
		} else if (hours_since_last <= 168) {	/* Last week */
			rank = JWEEK(tmp_rank);
		} else {								/* More than a week */
			rank = JOLDER(tmp_rank);
		}

		jump_db[i].rank = rank;

		/* Do not remove bookmarked, pinned, or workspaced directories */
		/* Add bonus points */
		int j = (int)bm_n;
		while (--j >= 0) {
			if (bookmarks[j].path && bookmarks[j].path[1] == jump_db[i].path[1]
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
			if (workspaces[j].path && workspaces[j].path[1] == jump_db[i].path[1]
			&& strcmp(jump_db[i].path, workspaces[j].path) == 0) {
				jump_db[i].rank += WORKSPACE_BONUS;
				jump_db[i].keep = 1;
				break;
			}
		}

/*		jump_db[i].rank = rank;
		total_rank += rank; */
		total_rank += jump_db[i].rank;
	}

	/* Once we have the total rank, check if we need to reduce ranks,
	 * and then write entries into the database */
	if (total_rank > max_jump_total_rank)
		reduce = (total_rank / max_jump_total_rank) + 1;

	int jump_num = 0;

	for (i = 0; i < (int)jump_n; i++) {
		if (total_rank > max_jump_total_rank) {
			/* Once we reach MAX_JUMP_TOTAL_RANK, start forgetting */
			if (reduce) {
				tmp_rank = jump_db[i].rank;
				jump_db[i].rank = tmp_rank / reduce;
			}

			/* Forget directories ranked below MIN_JUMP_RANK */
			if (jump_db[i].keep != 1 && jump_db[i].rank < min_jump_rank) {
				/* Discount from TOTAL_RANK the rank of the now forgotten
				 * directory to keep this total up to date */
				total_rank -= jump_db[i].rank;
				continue;
			}
		}

		jump_num++;
#ifndef __OpenBSD__
		fprintf(fp, "%zu:%ld:%ld:%s\n", jump_db[i].visits, jump_db[i].first_visit,
			jump_db[i].last_visit, jump_db[i].path);
#else
		fprintf(fp, "%zu:%lld:%lld:%s\n", jump_db[i].visits, jump_db[i].first_visit,
			jump_db[i].last_visit, jump_db[i].path);
#endif
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

/*	char *jump_file = (char *)xnmalloc(config_dir_len + 10, sizeof(char));
	sprintf(jump_file, "%s/jump.cfm", config_dir); */
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
	if (str[len - 1] == '/')
		slash = 1;

	jump_suggestion = xnmalloc(len + (slash ? 1 : 2), sizeof(char));
	if (!slash)
		sprintf(jump_suggestion, "%s/", str);
	else
		strcpy(jump_suggestion, str);

	return EXIT_SUCCESS;
}

/* Jump into best ranked directory matched by ARGS */
int
dirjump(char **args, int mode)
{
/*	if (!args || !*args[0])
		return EXIT_FAILURE; */

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
	if (mode == NO_SUG_JUMP && !args[1] && args[0][1] != 'e') {
		if (!jump_n) {
			printf("jump: Database still empty\n");
			return EXIT_SUCCESS;
		}

		puts(_("NOTE: First time access is displayed in days, while last "
		       "time access is displayed in hours"));
		puts(_("NOTE 2: An asterisk next rank values means that the "
		       "corresponding directory is bookmarked, pinned, or currently "
		       "used in some workspace\n"));
		puts(_("Order\tVisits\tFirst\tLast\tRank\tDirectory"));

		size_t i;
		int ranks_sum = 0, visits_sum = 0;

		for (i = 0; i < jump_n; i++) {

			int days_since_first = (int)(now - jump_db[i].first_visit) / 60 / 60 / 24;
			int hours_since_last = (int)(now - jump_db[i].last_visit) / 60 / 60;

			int rank;
			rank = days_since_first > 1
				   ? ((int)jump_db[i].visits * 100) / days_since_first
				   : (int)jump_db[i].visits * 100;

			int tmp_rank = rank;
			if (hours_since_last == 0) /* Last hour */
				rank = JHOUR(tmp_rank);
			else if (hours_since_last <= 24) /* Last day */
				rank = JDAY(tmp_rank);
			else if (hours_since_last <= 168) /* Last week */
				rank = JWEEK(tmp_rank);
			else /* More than a week */
				rank = JOLDER(tmp_rank);

			int j = (int)bm_n, bpw = 0; /* Bookmarked, pinned or workspace */
			while (--j >= 0) {
				if (bookmarks[j].path && bookmarks[j].path[1] == jump_db[i].path[1]
				&& strcmp(bookmarks[j].path, jump_db[i].path) == 0) {
					rank += BOOKMARK_BONUS;
					bpw = 1;
					break;
				}
			}

			if (pinned_dir && pinned_dir[1] == jump_db[i].path[1]
			&& strcmp(pinned_dir, jump_db[i].path) == 0) {
				rank += PINNED_BONUS;
				bpw = 1;
			}

			j = MAX_WS;
			while (--j >= 0) {
				if (workspaces[j].path && workspaces[j].path[1] == jump_db[i].path[1]
				&& strcmp(jump_db[i].path, workspaces[j].path) == 0) {
					rank += WORKSPACE_BONUS;
					bpw = 1;
					break;
				}
			}

			if (reduce) {
				tmp_rank = rank;
				rank = tmp_rank / reduce;
			}

			ranks_sum += rank;
			visits_sum += (int)jump_db[i].visits;

			if (workspaces[cur_ws].path && workspaces[cur_ws].path[1] == jump_db[i].path[1]
			&& strcmp(workspaces[cur_ws].path, jump_db[i].path) == 0) {
				printf("  %s%zu\t %zu\t %d\t %d\t%d%c\t%s%s \n", mi_c,
				    i + 1, jump_db[i].visits, days_since_first,
				    hours_since_last, rank, bpw ? '*' : 0,
				    jump_db[i].path, df_c);
			} else {
				printf("  %zu\t %zu\t %d\t %d\t%d%c\t%s \n", i + 1,
				    jump_db[i].visits, days_since_first,
				    hours_since_last, rank,
				    bpw ? '*' : 0, jump_db[i].path);
			}
		}

		printf("\nTotal rank: %d/%d\nTotal visits: %d\n", ranks_sum,
		    max_jump_total_rank, visits_sum);

		return EXIT_SUCCESS;
	}

	if (mode == NO_SUG_JUMP && args[1] && IS_HELP(args[1])) {
		puts(_(JUMP_USAGE));
		return EXIT_SUCCESS;
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
		if (args[i][_len - 1] == '/') {
			args[i][_len - 1] = '\0';
			last_segment = 1;
			first_segment = 0;
		} else if (args[i][_len - 1] == '\\') {
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
				/* Pointer to the beginning of the search str in the
				 * jump entry. Used to search for subsequent search
				 * strings starting from this position in the entry
				 * and not before */
				char *needle = case_sens_dirjump
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
					if (!strstr(workspaces[cur_ws].path, jump_db[j].path))
						exclude = 1;
					break;

				case JCHILD:
					if (!strstr(jump_db[j].path, workspaces[cur_ws].path))
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

				char *_needle = case_sens_dirjump
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
			int days_since_first = (int)(now - first[j]) / 60 / 60 / 24;

			/* Calculate the rank as frecency. The algorithm is based
			 * on Mozilla, zoxide, and z.lua. See:
			 * "https://wiki.mozilla.org/User:Mconnor/Past/PlacesFrecency"
			 * "https://github.com/ajeetdsouza/zoxide/wiki/Algorithm#aging"
			 * "https://github.com/skywind3000/z.lua#aging" */
			int rank;
			rank = days_since_first > 0 ? ((int)visits[j] * 100)
					/ days_since_first : ((int)visits[j] * 100);

			int hours_since_last = (int)(now - last[j]) / 60 / 60;

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
