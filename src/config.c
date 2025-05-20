/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2016-2025, L. Abramovich <leo.clifm@outlook.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
*/

/* config.c -- functions to define, create, and set configuration files */

#include "helpers.h"

#include <errno.h>
#include <readline/readline.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#if defined(MAC_OS_X_RENAMEAT_SYS_STDIO_H)
# include <sys/stdio.h> /* renameat(2) */
#endif /* MAC_OS_X_RENAMEAT_SYS_STDIO_H */

#include "autocmds.h"
#include "aux.h"
#include "checks.h"
#include "colors.h"
#include "config.h"
#include "file_operations.h"
#include "init.h"
#include "listing.h"
#include "messages.h"
#include "misc.h"
#include "navigation.h"
#include "sort.h" /* num_to_sort_name() */
#include "spawn.h"

/* Predefined time styles */
#define ISO_TIME           "%Y-%m-%d"
#define LONG_ISO_TIME      "%Y-%m-%d %H:%M"
#define FULL_ISO_TIME      "%Y-%m-%d %H:%M:%S %z"
#define FULL_ISO_NANO_TIME "%Y-%m-%d %H:%M:%S.%N %z"

/* Regenerate the configuration file and create a back up of the old one. */
static int
regen_config(void)
{
	int config_found = 1;
	struct stat attr;

	if (stat(config_file, &attr) == -1) {
		puts(_("Configuration file not found"));
		config_found = 0;
	}

	if (config_found == 1) {
		char *backup = gen_backup_file(config_file, 1);
		if (!backup)
			return FUNC_FAILURE;

		if (renameat(XAT_FDCWD, config_file, XAT_FDCWD, backup) == -1) {
			xerror(_("Cannot rename '%s' to '%s': %s\n"),
				config_file, backup, strerror(errno));
			free(backup);
			return errno;
		}

		char *abbrev = abbreviate_file_name(backup);
		printf(_("Old configuration file saved as '%s'\n"),
			abbrev ? abbrev : backup);
		free(backup);
		free(abbrev);
	}

	if (create_main_config_file(config_file) != FUNC_SUCCESS)
		return FUNC_FAILURE;

	printf(_("New configuration file written to '%s'\n"), config_file);
	reload_config();
	return FUNC_SUCCESS;
}

static void
print_config_value(const char *option, void *cur_value, void *def_value,
		const int type)
{
	const char *ptr = SET_MISC_PTR;

	if (type == DUMP_CONFIG_STR) {
		char *cv = (char *)cur_value;
		char *dv = (char *)def_value;
		if (!cv || (dv && strcmp(cv, dv) == 0))
			printf("  %s: \"%s\"\n", option, dv);
		else
			printf("%s%s%s %s%s: \"%s\" [\"%s\"]%s\n", mi_c, ptr, df_c,
				BOLD, option, cv, dv, df_c);
	}

	else if (type == DUMP_CONFIG_BOOL) {
		int cv = *((int *)cur_value), dv = *((int *)def_value);
		if (cv == dv)
			printf("  %s: %s\n", option, cv == 1 ? "true" : "false");
		else
			printf("%s%s%s %s%s: %s [%s]%s\n", mi_c, ptr, df_c, BOLD, option,
				cv == 1 ? "true" : "false", dv == 1 ? "true" : "false", df_c);
	}

	else if (type == DUMP_CONFIG_CHR) {
		char cv = *((char *)cur_value), dv = *((char *)def_value);
		if (cv == dv)
			printf("  %s: '%c'\n", option, cv);
		else
			printf("%s%s%s %s%s: '%c' ['%c']%s\n", mi_c, ptr, df_c, BOLD, option,
				cv, dv, df_c);
	}

	else { /* CONFIG_BOOL_INT */
		int cv = *((int *)cur_value), dv = *((int *)def_value);
		if (cv == dv)
			printf("  %s: %d\n", option, cv);
		else
			printf("%s%s%s %s%s: %d [%d]%s\n", mi_c, ptr, df_c, BOLD, option,
				cv, dv, df_c);
	}
}

/* Return a string representing the value for TabCompletionMode in
 * the config file. */
static char *
get_tab_comp_mode_str(void)
{
	switch (tabmode) {
	case FZF_TAB: return "fzf";
	case FNF_TAB: return "fnf";
	case SMENU_TAB: return "smenu";
	case STD_TAB: /* fallthrough */
	default: return "standard";
	}
}

static void
get_start_path_and_ws_names(char **sp, char **ws)
{
	if (config_ok == 0 || !config_file)
		return;

	int fd;
	FILE *fp = open_fread(config_file, &fd);
	if (!fp)
		return;

	char line[PATH_MAX + 16]; *line = '\0';
	while (fgets(line, (int)sizeof(line), fp)) {
		if (*line == 'W' && strncmp(line, "WorkspaceNames=", 15) == 0
		&& *(line + 15)) {
			char *tmp = remove_quotes(line + 15);
			if (!tmp || !*tmp)
				continue;

			*ws = savestring(tmp, strlen(tmp));
		}

		if (*line == 'S' && strncmp(line, "StartingPath=", 13) == 0
		&& *(line + 13)) {
			char *tmp = remove_quotes(line + 13);
			if (!tmp || !*tmp)
				continue;

			*sp = savestring(tmp, strlen(tmp));
		}
	}

	fclose(fp);
}

static char *
get_quoting_style(const int style)
{
	switch (style) {
	case QUOTING_STYLE_BACKSLASH: return "backslash";
	case QUOTING_STYLE_DOUBLE_QUOTES: return "double";
	case QUOTING_STYLE_SINGLE_QUOTES: return "single";
	default: return "backslash";
	}
}

static char *
get_link_creat_mode(const int mode)
{
	switch (mode) {
	case LNK_CREAT_ABS: return "absolute";
	case LNK_CREAT_REL: return "relative";
	case LNK_CREAT_REG: /* fallthrough */
	default: return "literal";
	}
}

static char *
get_ia_value_str(const int val)
{
	switch (val) {
	case AUTOCMD_MSG_FULL: return "full";
	case AUTOCMD_MSG_SHORT: return "short";
	case AUTOCMD_MSG_LONG: return "long";
	case AUTOCMD_MSG_PROMPT: return "prompt";
	case AUTOCMD_MSG_NONE: return "none";
	default: return "prompt";
	}
}

static char *
gen_default_answer_value_str(const int cfg)
{
	char val_d = cfg ? conf.default_answer.default_ : DEF_ANSWER_DEFAULT;
	char val_D = cfg ? conf.default_answer.default_all : DEF_ANSWER_DEFAULT_ALL;
	char val_o = cfg ? conf.default_answer.overwrite : DEF_ANSWER_OVERWRITE;
	char val_r = cfg ? conf.default_answer.remove : DEF_ANSWER_REMOVE;
	char val_R = cfg ? conf.default_answer.bulk_rename : DEF_ANSWER_BULK_RENAME;
	char val_t = cfg ? conf.default_answer.trash : DEF_ANSWER_TRASH;

#define DA_BUFSIZE 32
	static char bufa[DA_BUFSIZE];
	static char bufb[DA_BUFSIZE];
	char *p = cfg ? bufa : bufb;
	int l = 0;
	memset(p, '\0', DA_BUFSIZE);

	l += snprintf(p + l, DA_BUFSIZE - (size_t)l, "d:%c,", val_d ? val_d : '-');
	l += snprintf(p + l, DA_BUFSIZE - (size_t)l, "D:%c,", val_D ? val_D : '-');
	l += snprintf(p + l, DA_BUFSIZE - (size_t)l, "o:%c,", val_o ? val_o : '-');
	l += snprintf(p + l, DA_BUFSIZE - (size_t)l, "r:%c,", val_r ? val_r : '-');
	l += snprintf(p + l, DA_BUFSIZE - (size_t)l, "R:%c,", val_R ? val_R : '-');
	l += snprintf(p + l, DA_BUFSIZE - (size_t)l, "t:%c,", val_t ? val_t : '-');
#undef DA_BUFSIZE

	if (l > 0 && p[l - 1] == ',')
		p[l - 1] = '\0';

	return p;
}

static char *
gen_desktop_notif_str(const int value)
{
	switch (value) {
	case DESKTOP_NOTIF_NONE: return "false";
	case DESKTOP_NOTIF_SYSTEM: return "system";
	case DESKTOP_NOTIF_KITTY: return "kitty";
	default: return "unknown";
	}
}

/* Dump current value of config options (as defined in the config file),
 * highlighting those that differ from default values.
 * Note that values displayed here represent the CURRENT status of the
 * corresponding option, and not necessarily that of the config file:
 * some of these options can be changed on the fly via commands */
static int
dump_config(void)
{
	puts(_("The following is the list of options (as defined in the configuration "
		"file) and their current values. Whenever a value differs from the "
		"default, the entry is highlighted and the default value is displayed "
		"in square brackets.\n"));

	char *start_path = (char *)NULL, *ws_names = (char *)NULL;
	get_start_path_and_ws_names(&start_path, &ws_names);

	char *s = (char *)NULL;
	int n = 0;

	n = DEF_APPARENT_SIZE;
	print_config_value("ApparentSize", &conf.apparent_size, &n,
		DUMP_CONFIG_BOOL);

	n = DEF_AUTOCD;
	print_config_value("Autocd", &conf.autocd, &n, DUMP_CONFIG_BOOL);

	n = DEF_AUTOLS;
	print_config_value("AutoLs", &conf.autols, &n, DUMP_CONFIG_BOOL);

	n = DEF_AUTO_OPEN;
	print_config_value("AutoOpen", &conf.auto_open, &n, DUMP_CONFIG_BOOL);

#ifndef _NO_SUGGESTIONS
	n = DEF_SUGGESTIONS;
	print_config_value("AutoSuggestions", &conf.suggestions, &n,
		DUMP_CONFIG_BOOL);
#endif /* !_NO_SUGGESTIONS */

	n = DEF_CASE_SENS_DIRJUMP;
	print_config_value("CaseSensitiveDirjump", &conf.case_sens_dirjump,
		&n, DUMP_CONFIG_BOOL);

	n = DEF_CASE_SENS_LIST;
	print_config_value("CaseSensitiveList", &conf.case_sens_list, &n,
		DUMP_CONFIG_BOOL);

	n = DEF_CASE_SENS_PATH_COMP;
	print_config_value("CaseSensitivePathComp", &conf.case_sens_path_comp,
		&n, DUMP_CONFIG_BOOL);

	n = DEF_CASE_SENS_SEARCH;
	print_config_value("CaseSensitiveSearch", &conf.case_sens_search, &n,
		DUMP_CONFIG_BOOL);

	n = DEF_CD_ON_QUIT;
	print_config_value("CdOnQuit", &conf.cd_on_quit, &n, DUMP_CONFIG_BOOL);

	n = DEF_CLASSIFY;
	print_config_value("Classify", &conf.classify, &n, DUMP_CONFIG_BOOL);

	n = DEF_CLEAR_SCREEN;
	print_config_value("ClearScreen", &conf.clear_screen, &n, DUMP_CONFIG_BOOL);

	n = DEF_COLOR_LNK_AS_TARGET;
	print_config_value("ColorLinksAsTarget", &conf.color_lnk_as_target,
		&n, DUMP_CONFIG_BOOL);

	s = term_caps.color < 256 ? DEF_COLOR_SCHEME : DEF_COLOR_SCHEME_256;
	print_config_value("ColorScheme", cur_cscheme, s, DUMP_CONFIG_STR);

	n = DEF_CP_CMD;
	print_config_value("cpCmd", &conf.cp_cmd, &n, DUMP_CONFIG_INT);

	s = gen_default_answer_value_str(0);
	char *cur_da_value = gen_default_answer_value_str(1);
	print_config_value("DefaultAnswer", cur_da_value, s, DUMP_CONFIG_STR);

	print_config_value("DesktopNotifications",
		gen_desktop_notif_str(conf.desktop_notifications),
		gen_desktop_notif_str(DEF_DESKTOP_NOTIFICATIONS), DUMP_CONFIG_STR);

	s = "";
	print_config_value("DirhistIgnore", &conf.dirhistignore_regex,
		s, DUMP_CONFIG_STR);

	n = DEF_DIRHIST_MAP;
	print_config_value("DirhistMap", &conf.dirhist_map, &n, DUMP_CONFIG_BOOL);

	n = DEF_DISK_USAGE;
	print_config_value("DiskUsage", &conf.disk_usage, &n, DUMP_CONFIG_BOOL);

	n = DEF_EXT_CMD_OK;
	print_config_value("ExternalCommands", &conf.ext_cmd_ok, &n,
		DUMP_CONFIG_BOOL);

	n = DEF_FILES_COUNTER;
	print_config_value("FilesCounter", &conf.files_counter, &n,
		DUMP_CONFIG_BOOL);

	s = "";
	print_config_value("Filter", filter.str, s, DUMP_CONFIG_STR);

	n = DEF_FULL_DIR_SIZE;
	print_config_value("FullDirSize", &conf.full_dir_size, &n, DUMP_CONFIG_BOOL);

#ifndef _NO_FZF
	n = DEF_FUZZY_MATCH;
	print_config_value("FuzzyMatching", &conf.fuzzy_match, &n,
		DUMP_CONFIG_BOOL);

	n = DEF_FUZZY_MATCH_ALGO;
	print_config_value("FuzzyAlgorithm", &conf.fuzzy_match_algo, &n,
		DUMP_CONFIG_INT);

	n = DEF_FZF_PREVIEW;
	print_config_value("FzfPreview", &conf.fzf_preview, &n, DUMP_CONFIG_BOOL);
#endif /* !_NO_FZF */

	s = DEF_HISTIGNORE;
	print_config_value("HistIgnore", conf.histignore_regex,
		s, DUMP_CONFIG_STR);

#ifndef _NO_ICONS
	n = DEF_ICONS;
	print_config_value("Icons", &conf.icons, &n, DUMP_CONFIG_BOOL);

	n = DEF_ICONS_GAP;
	print_config_value("IconsGap", &conf.icons_gap, &n, DUMP_CONFIG_INT);
#endif /* !_NO_ICONS */

	print_config_value("InformAutocmd", get_ia_value_str(conf.autocmd_msg),
		get_ia_value_str(DEF_AUTOCMD_MSG), DUMP_CONFIG_STR);

	n = DEF_LIGHT_MODE;
	print_config_value("LightMode", &conf.light_mode, &n, DUMP_CONFIG_BOOL);

	print_config_value("LinkCreationMode",
		get_link_creat_mode(conf.link_creat_mode),
		get_link_creat_mode(DEF_LINK_CREATION_MODE), DUMP_CONFIG_STR);

	n = DEF_LIST_DIRS_FIRST;
	print_config_value("ListDirsFirst", &conf.list_dirs_first, &n,
		DUMP_CONFIG_BOOL);

	n = DEF_LISTING_MODE;
	print_config_value("ListingMode", &conf.listing_mode, &n, DUMP_CONFIG_INT);

	n = DEF_LOG_CMDS;
	print_config_value("LogCmds", &conf.log_cmds, &n, DUMP_CONFIG_BOOL);

	n = DEF_LOG_MSGS;
	print_config_value("LogMsgs", &conf.log_msgs, &n, DUMP_CONFIG_BOOL);

	n = DEF_LONG_VIEW;
	print_config_value("LongViewMode", &conf.long_view, &n, DUMP_CONFIG_BOOL);

	n = DEF_MAX_DIRHIST;
	print_config_value("MaxDirhist", &conf.max_dirhist, &n, DUMP_CONFIG_INT);

	n = DEF_MAX_NAME_LEN;
	print_config_value("MaxFilenameLen", &conf.max_name_len, &n,
		DUMP_CONFIG_INT);

	n = DEF_MAX_HIST;
	print_config_value("MaxHistory", &conf.max_hist, &n, DUMP_CONFIG_INT);

	n = DEF_MAX_JUMP_TOTAL_RANK;
	print_config_value("MaxJumpTotalRank", &conf.max_jump_total_rank,
		&n, DUMP_CONFIG_INT);

	n = DEF_MAX_LOG;
	print_config_value("MaxLog", &conf.max_log, &n, DUMP_CONFIG_INT);

	n = DEF_PROMPT_P_MAX_PATH;
	print_config_value("MaxPath", &conf.prompt_p_max_path, &n, DUMP_CONFIG_INT);

	n = DEF_MAX_PRINTSEL;
	print_config_value("MaxPrintSelfiles", &conf.max_printselfiles, &n,
		DUMP_CONFIG_INT);

	n = DEF_MIN_NAME_TRUNC;
	print_config_value("MinNameTruncate", &conf.min_name_trunc, &n,
		DUMP_CONFIG_INT);

	n = DEF_MIN_JUMP_RANK;
	print_config_value("MinJumpRank", &conf.min_jump_rank, &n, DUMP_CONFIG_INT);

	n = DEF_MV_CMD;
	print_config_value("mvCmd", &conf.mv_cmd, &n, DUMP_CONFIG_INT);

	s = "";
	print_config_value("Opener", conf.opener, s, DUMP_CONFIG_STR);

	n = DEF_PAGER;
	print_config_value("Pager", &conf.pager, &n, conf.pager > 1
		? DUMP_CONFIG_INT : DUMP_CONFIG_BOOL);

	n = DEF_PREVIEW_MAX_SIZE;
	print_config_value("PreviewMaxSize (in KiB)", &conf.preview_max_size, &n,
		DUMP_CONFIG_INT);

	n = DEF_PRINT_DIR_CMDS;
	print_config_value("PrintDirCmds", &conf.print_dir_cmds, &n,
		DUMP_CONFIG_BOOL);

	n = DEF_PRINTSEL;
	print_config_value("PrintSelfiles", &conf.print_selfiles, &n,
		DUMP_CONFIG_BOOL);

	s = DEF_PRIORITY_SORT_CHAR;
	print_config_value("PrioritySortChar", conf.priority_sort_char,
		s, DUMP_CONFIG_STR);

	n = DEF_PRIVATE_WS_SETTINGS;
	print_config_value("PrivateWorkspaceSettings", &conf.private_ws_settings,
		&n, DUMP_CONFIG_BOOL);

	s = DEF_PROP_FIELDS;
	print_config_value("PropFields", prop_fields_str, s, DUMP_CONFIG_STR);

	n = DEF_PROP_FIELDS_GAP;
	print_config_value("PropFieldsGap", &conf.prop_fields_gap, &n, DUMP_CONFIG_INT);

	s = "";
	print_config_value("PTimeStyle", conf.ptime_str, s, DUMP_CONFIG_STR);

	n = DEF_PURGE_JUMPDB;
	print_config_value("PurgeJumpDB", &conf.purge_jumpdb, &n, DUMP_CONFIG_BOOL);

	print_config_value("QuotingStyle", get_quoting_style(conf.quoting_style),
		get_quoting_style(DEF_QUOTING_STYLE), DUMP_CONFIG_STR);

	n = DEF_READ_AUTOCMD_FILES;
	print_config_value("ReadAutocmdFiles", &conf.read_autocmd_files, &n,
		DUMP_CONFIG_BOOL);

	n = DEF_READ_DOTHIDDEN;
	print_config_value("ReadDotHidden", &conf.read_dothidden, &n,
		DUMP_CONFIG_BOOL);

	n = DEF_RESTORE_LAST_PATH;
	print_config_value("RestoreLastPath", &conf.restore_last_path, &n,
		DUMP_CONFIG_BOOL);

	n = DEF_RL_EDIT_MODE;
	print_config_value("RlEditMode", &rl_editing_mode, &n, DUMP_CONFIG_INT);

	n = DEF_RM_FORCE;
	print_config_value("rmForce", &conf.rm_force, &n, DUMP_CONFIG_BOOL);

	n = DEF_SEARCH_STRATEGY;
	print_config_value("SearchStrategy", &conf.search_strategy, &n,
		DUMP_CONFIG_INT);

	n = DEF_SHARE_SELBOX;
	print_config_value("ShareSelbox", &conf.share_selbox, &n, DUMP_CONFIG_BOOL);

	n = DEF_SHOW_HIDDEN;
	print_config_value("ShowHiddenFiles", &conf.show_hidden, &n,
		DUMP_CONFIG_BOOL);

	n = DEF_SKIP_NON_ALNUM_PREFIX;
	print_config_value("SkipNonAlnumPrefix", &conf.skip_non_alnum_prefix, &n,
		DUMP_CONFIG_BOOL);

	print_config_value("Sort", num_to_sort_name(conf.sort, 0),
		num_to_sort_name(DEF_SORT, 0), DUMP_CONFIG_STR);

	n = DEF_SORT_REVERSE;
	print_config_value("SortReverse", &conf.sort_reverse, &n, DUMP_CONFIG_BOOL);

	n = DEF_SPLASH_SCREEN;
	print_config_value("SplashScreen", &conf.splash_screen, &n,
		DUMP_CONFIG_BOOL);

	s = "";
	print_config_value("StartingPath", start_path, s, DUMP_CONFIG_STR);
	free(start_path);

#ifndef _NO_SUGGESTIONS
	n = DEF_CMD_DESC_SUG;
	print_config_value("SuggestCmdDesc", &conf.cmd_desc_sug, &n,
		DUMP_CONFIG_BOOL);

	n = DEF_SUG_FILETYPE_COLOR;
	print_config_value("SuggestFiletypeColor", &conf.suggest_filetype_color,
		&n, DUMP_CONFIG_BOOL);

	s = DEF_SUG_STRATEGY;
	print_config_value("SuggestionStrategy", conf.suggestion_strategy,
		s, DUMP_CONFIG_STR);
#endif /* !_NO_SUGGESTIONS */
#ifndef _NO_HIGHLIGHT
	n = DEF_HIGHLIGHT;
	print_config_value("SyntaxHighlighting", &conf.highlight, &n,
		DUMP_CONFIG_BOOL);
#endif /* !_NO_HIGHLIGHT */

	char *ss = get_tab_comp_mode_str();
	print_config_value("TabCompletionMode", ss,
#ifndef _NO_FZF
		(bin_flags & FZF_BIN_OK) ? "fzf" : "standard",
#else
		"standard",
#endif /* !_NO_FZF */
		DUMP_CONFIG_STR);

	s = DEF_TERM_CMD;
	print_config_value("TerminalCmd", conf.term, s, DUMP_CONFIG_STR);

	n = DEF_TIME_FOLLOWS_SORT;
	print_config_value("TimeFollowsSort", &conf.time_follows_sort,
		&n, DUMP_CONFIG_BOOL);

	n = DEF_TIMESTAMP_MARK;
	print_config_value("TimestampMark", &conf.timestamp_mark,
		&n, DUMP_CONFIG_BOOL);

	s = "";
	print_config_value("TimeStyle", conf.time_str, s, DUMP_CONFIG_STR);

	n = DEF_TIPS;
	print_config_value("Tips", &conf.tips, &n, DUMP_CONFIG_BOOL);

#ifndef _NO_TRASH
	n = DEF_TRASRM;
	print_config_value("TrashAsRm", &conf.tr_as_rm, &n, DUMP_CONFIG_BOOL);

	n = DEF_TRASH_FORCE;
	print_config_value("TrashForce", &conf.trash_force, &n, DUMP_CONFIG_BOOL);
#endif /* !_NO_TRASH */

	n = DEF_TRUNC_NAMES;
	print_config_value("TruncateNames", &conf.trunc_names, &n, DUMP_CONFIG_BOOL);

	n = DEF_WELCOME_MESSAGE;
	print_config_value("WelcomeMessage", &conf.welcome_message, &n,
		DUMP_CONFIG_BOOL);

	s = DEF_WELCOME_MESSAGE_STR;
	print_config_value("WelcomeMessageStr", conf.welcome_message_str,
		s, DUMP_CONFIG_STR);

	s = "";
	print_config_value("WorkspaceNames", ws_names, s, DUMP_CONFIG_STR);
	free(ws_names);

	return FUNC_SUCCESS;
}

/* Edit the config file, either via the mime function or via the first
 * passed argument (e.g.: 'edit nano'). The 'reset' option regenerates
 * the configuration file and creates a back up of the old one. */
int
config_edit(char **args)
{
	if (xargs.stealth_mode == 1) {
		printf("%s: %s\n", PROGRAM_NAME, STEALTH_DISABLED);
		return FUNC_SUCCESS;
	}

	if (args[1] && IS_HELP(args[1])) {
		printf("%s\n", CONFIG_USAGE);
		return FUNC_SUCCESS;
	}

	if (args[1] && *args[1] == 'd' && strcmp(args[1], "dump") == 0)
		return dump_config();

	if (args[1] && *args[1] == 'r' && strcmp(args[1], "reload") == 0)
		return config_reload(args[2]);

	if (args[1] && *args[1] == 'r' && strcmp(args[1], "reset") == 0)
		return regen_config();

	if (config_ok == 0) {
		xerror(_("%s: Cannot access the configuration file\n"), PROGRAM_NAME);
		return FUNC_FAILURE;
	}

	char *opening_app =
		(args[1] && strcmp(args[1], "edit") == 0) ? args[2] : args[1];

	/* Get modification time of the config file before opening it */
	struct stat attr;

	/* If, for some reason (like someone erasing the file while the program
	 * is running) clifmrc doesn't exist, recreate the configuration file.
	 * Then run 'stat' again to reread the attributes of the file. */
	if (stat(config_file, &attr) == -1) {
		create_main_config_file(config_file);
		if (stat(config_file, &attr) == -1) {
			xerror("%s: '%s': %s\n", PROGRAM_NAME, config_file, strerror(errno));
			return errno;
		}
	}

	const time_t prev_mtime = attr.st_mtime;

	const int ret = open_config_file(opening_app, config_file);
	if (ret != FUNC_SUCCESS)
		return ret;

	/* Get modification time after opening the config file */
	if (stat(config_file, &attr) == -1) {
		xerror("%s: %s: %s\n", PROGRAM_NAME, config_file, strerror(errno));
		return errno;
	}

	/* If modification times differ, the file was modified after being opened */
	if (prev_mtime != attr.st_mtime) {
		/* Reload configuration only if the config file was modified */
		reload_config();

		if (conf.autols == 1)
			reload_dirlist();
		print_reload_msg(NULL, NULL, _(CONFIG_FILE_UPDATED));
	}

	return ret;
}

int
config_reload(const char *arg)
{
	if (arg && IS_HELP(arg)) {
		puts(RELOAD_USAGE);
		return FUNC_SUCCESS;
	}

	const int exit_status = reload_config();
	conf.welcome_message = 0;

	if (conf.autols == 1)
		reload_dirlist();

	print_reload_msg(NULL, NULL, "Configuration file reloaded\n");
	return exit_status;
}

/* Find the plugins-helper file and store its path in the PLUGINS_HELPER_FILE
 * global variable. It will used to set CLIFM_PLUGINS_HELPER environment
 * variable when running a plugin.
 * Returns zero on success or one on error.
 * 1. Try first the plugins directory.
 * 2. If not there, try the data dir.
 * 3. Else, try some standard locations. */
static int
set_plugins_helper_file(void)
{
	if (getenv("CLIFM_PLUGINS_HELPER"))
		return FUNC_SUCCESS;

	struct stat attr;
	if (plugins_dir && *plugins_dir) {
		char helper_path[PATH_MAX + 1];
		snprintf(helper_path, sizeof(helper_path),
			"%s/plugins-helper", plugins_dir);

		if (stat(helper_path, &attr) != -1) {
			plugins_helper_file = savestring(helper_path, strlen(helper_path));
			return FUNC_SUCCESS;
		}
	}

	if (data_dir && *data_dir) {
		char helper_path[PATH_MAX + 1];
		snprintf(helper_path, sizeof(helper_path),
			"%s/%s/plugins/plugins-helper", data_dir, PROGRAM_NAME);

		if (stat(helper_path, &attr) != -1) {
			plugins_helper_file = savestring(helper_path, strlen(helper_path));
			return FUNC_SUCCESS;
		}
	}

	char home_local[PATH_MAX + 1];
	*home_local = '\0';
	if (user.home && *user.home) {
		snprintf(home_local, sizeof(home_local),
			"%s/.local/share/clifm/plugins/plugins-helper", user.home);
	}

	const char *const std_paths[] = {
		home_local,
#if defined(__HAIKU__)
		"/boot/system/non-packaged/data/clifm/plugins/plugins-helper",
		"/boot/system/data/clifm/plugins/plugins-helper",
#elif defined(__TERMUX__)
		"/data/data/com.termux/files/usr/share/clifm/plugins/plugins-helper",
		"/data/data/com.termux/files/usr/local/share/clifm/plugins/plugins-helper",
#else
		"/usr/share/clifm/plugins/plugins-helper",
		"/usr/local/share/clifm/plugins/plugins-helper",
		"/opt/local/share/clifm/plugins/plugins-helper",
		"/opt/share/clifm/plugins/plugins-helper",
#endif /* __HAIKU__ */
		NULL};

	size_t i;
	for (i = 0; std_paths[i]; i++) {
		if (stat(std_paths[i], &attr) != -1) {
			plugins_helper_file = savestring(std_paths[i], strlen(std_paths[i]));
			return FUNC_SUCCESS;
		}
	}

	return FUNC_FAILURE;
}

static void
set_shell_level(void)
{
	char *shlvl = getenv("SHLVL");

	int shlvl_n = shlvl ? atoi(shlvl) : -1;
	if (shlvl_n > 0 && shlvl_n < MAX_SHELL_LEVEL)
		shlvl_n++;
	else
		shlvl_n = 1;

	char tmp[MAX_INT_STR];
	snprintf(tmp, sizeof(tmp), "%d", shlvl_n);
	setenv("SHLVL", tmp, 1);

	char *lvl = getenv("CLIFMLVL");
	if (!lvl) {
		setenv("CLIFMLVL", "1", 1);
		return;
	}

	int lvl_n = atoi(lvl);
	if (lvl_n > 0 && lvl_n < (MAX_SHELL_LEVEL - shlvl_n))
		lvl_n++;
	else
		lvl_n = 1;

	nesting_level = lvl_n;
	snprintf(tmp, sizeof(tmp), "%d", lvl_n);
	setenv("CLIFMLVL", tmp, 1);
}

/* Set a few environment variables. */
void
set_env(const int reload)
{
	if (xargs.stealth_mode == 1)
		return;

	setenv("CLIFM", (config_dir && *config_dir) ? config_dir : "1", 1);
	if (config_file && *config_file)
		setenv("CLIFMRC", config_file, 1);
	setenv("CLIFM_PID", xitoa((long long)own_pid), 1);
	setenv("CLIFM_VERSION", VERSION, 1);

	set_plugins_helper_file();

	if (reload == 0)
		set_shell_level();
}

/* Define the file for the Selection Box */
void
set_sel_file(void)
{
	if (xargs.sel_file == 1) /* Already set via --sel-file flag */
		return;

	if (sel_file) {
		free(sel_file);
		sel_file = (char *)NULL;
	}

	if (!config_dir)
		return;

	size_t len = 0;
	if (conf.share_selbox == 0) {
		/* Private selection box is stored in the profile directory */
		len = config_dir_len + 14;
		sel_file = xnmalloc(len, sizeof(char));
		snprintf(sel_file, len, "%s/selbox.clifm", config_dir);
	} else {
		/* Shared selection box is stored in the general config dir */
		len = config_dir_len + 23;
		sel_file = xnmalloc(len, sizeof(char));
		snprintf(sel_file, len, "%s/.config/%s/selbox.clifm", user.home,
			PROGRAM_NAME);
	}
}

/* Copy the file SRC_FILENAME from the data directory (DATA_DIR) to the
 * directory DEST, and make DEST executable if EXEC is set to 1.
 * The original file will be copied into DEST with AT MOST
 * 600 permissions (via umask(3)), making sure the user has read/write
 * permissions (via xchmod()), just in case the original file has no such
 * permissions. */
static int
import_from_data_dir(const char *src_filename, char *dest, const int exec)
{
	if (!data_dir || !src_filename || !dest
	|| !*data_dir || !*src_filename || !*dest)
		return FUNC_FAILURE;

	struct stat a;
	char sys_file[PATH_MAX + 1];
	snprintf(sys_file, sizeof(sys_file), "%s/%s/%s", data_dir,
		PROGRAM_NAME, src_filename);
	if (stat(sys_file, &a) == -1)
		return FUNC_FAILURE;

	const mode_t old_umask = umask(exec == 1 ? 0077 : 0177); /* flawfinder: ignore */
	char *cmd[] = {"cp", "--", sys_file, dest, NULL};
	const int ret = launch_execv(cmd, FOREGROUND, E_NOSTDERR);
	umask(old_umask); /* flawfinder: ignore */

	if (ret == FUNC_SUCCESS) {
#ifndef __MSYS__
		/* chmod(2) does not work on MSYS2. See
		 * https://github.com/msys2/MSYS2-packages/issues/2612 */
		xchmod(dest, exec == 1 ? "0700" : "0600", 1);
#endif /* __MSYS__ */
		return FUNC_SUCCESS;
	}

	return FUNC_FAILURE;
}

/* Create the keybindings file. */
int
create_kbinds_file(void)
{
	if (config_ok == 0 || !kbinds_file)
		return FUNC_FAILURE;

	struct stat attr;
	/* If the file already exists, do nothing */
	if (stat(kbinds_file, &attr) == FUNC_SUCCESS)
		return FUNC_SUCCESS;

	/* If not, try to import it from DATADIR */
	if (import_from_data_dir("keybindings.clifm", kbinds_file, 0) == FUNC_SUCCESS)
		return FUNC_SUCCESS;

	/* Else, create it */
	int fd = 0;
	FILE *fp = open_fwrite(kbinds_file, &fd);
	if (!fp) {
		err('e', PRINT_PROMPT, "%s: Cannot create keybindings file "
			"'%s': %s\n", PROGRAM_NAME, kbinds_file, strerror(errno));
		return FUNC_FAILURE;
	}

	fprintf(fp, "# Keybindings file for %s\n\n\
# Do not edit this file directly. Use the 'kb bind' command instead.\n\
\n\
# Alt+j\n\
previous-dir:\\ej\n\
# Shift+Left (rxvt)\n\
previous-dir:\\e[d\n\
# Shift+Left (xterm)\n\
previous-dir:\\e[1;2D\n\
# Shift+Left (others)\n\
previous-dir:\\e[2D\n\
\n\
# Alt+k\n\
next-dir:\\ek\n\
# Shift+Right (rxvt)\n\
next-dir:\\e[c\n\
# Shift+Right (xterm)\n\
next-dir:\\e[1;2C\n\
# Shift+Right (others)\n\
next-dir:\\e[2C\n\
first-dir:\\e\\C-j\n\
last-dir:\\e\\C-k\n\
\n\
# Alt+u\n\
parent-dir:\\eu\n\
# Shift+Up (rxvt)\n\
parent-dir:\\e[a\n\
# Shift+Up (xterm)\n\
parent-dir:\\e[1;2A\n\
# Shift+Up (others)\n\
parent-dir:\\e[2A\n\
\n\
# Alt+e\n\
home-dir:\\ee\n\
# Home key (rxvt)\n\
#home-dir:\\e[7~\n\
# Home key (xterm)\n\
#home-dir:\\e[H\n\
# Home key (Emacs term)\n\
#home-dir:\\e[1~\n\
\n\
# Alt+r\n\
root-dir:\\er\n\
# Alt+/\n\
root-dir:\\e/\n\
\n\
pinned-dir:\\ep\n\
workspace1:\\e1\n\
workspace2:\\e2\n\
workspace3:\\e3\n\
workspace4:\\e4\n\
\n\
# Help\n\
# F1-F3\n\
show-manpage:\\eOP\n\
show-manpage:\\e[11~\n\
show-cmds:\\eOQ\n\
show-cmds:\\e[12~\n\
show-kbinds:\\eOR\n\
show-kbinds:\\e[13~\n\n\
archive-sel:\\e\\C-a\n\
bookmarks:\\eb\n\
clear-line:\\ec\n\
clear-msgs:\\et\n\
#cmd-hist:\n\
copy-sel:\\e\\C-v\n\
create-file:\\en\n\
deselect-all:\\ed\n\
export-sel:\\e\\C-e\n\
dirs-first:\\eg\n\
launch-view:\\e-\n\
#lock:\\eo\n\
mountpoints:\\em\n\
#move-sel:\\e\\C-n\n\
new-instance:\\e\\C-x\n\
next-profile:\\e\\C-p\n\
only-dirs:\\e,\n\
#open-sel:\\e\\C-g\n\
prepend-sudo:\\ev\n\
previous-profile:\\e\\C-o\n\
rename-sel:\\e\\C-r\n\
remove-sel:\\e\\C-d\n\
refresh-screen:\\C-r\n\
run-pager:\\e0\n\
selbox:\\es\n\
select-all:\\ea\n\
show-dirhist:\\eh\n\
sort-previous:\\ez\n\
sort-next:\\ex\n\
toggle-hidden:\\ei\n\
toggle-hidden:\\e.\n\
toggle-light:\\ey\n\
toggle-long:\\el\n\
toggle-follow-links-long:\\e+\n\
toggle-max-name-len:\\e\\C-l\n\
toggle-disk-usage:\\e\\C-i\n\
toggle-virtualdir-full-paths:\\ew\n\
toggle-vi-mode:\\e\\C-j\n\
trash-sel:\\e\\C-t\n\
#untrash-all:\\e\\C-u\n\n\
# F6-F12\n\
open-mime:\\e[17~\n\
open-preview:\\e[18~\n\
#open-jump-db:\\e[18~\n\
edit-color-scheme:\\e[19~\n\
open-keybinds:\\e[20~\n\
open-config:\\e[21~\n\
open-bookmarks:\\e[23~\n\
quit:\\e[24~\n\
\n\
# Keybindings for plugins\n\
# 1) Make sure your plugin is in the plugins directory (or use any of the\n\
# plugins in there)\n\
# 2) Link pluginx to your plugin using the 'actions edit' command. E.g.:\n\
# \"plugin1=myplugin.sh\"\n\
# 3) Set a keybinding here for pluginx. E.g.: \"plugin1:\\C-y\"\n\n\
# Note: Up to 16 plugins are supported, i.e. from plugin1 through plugin16\n\n\
\n\
# Bound to the xclip plugin\n\
plugin1:\\C-y\n",
	    PROGRAM_NAME);

	fclose(fp);
	return FUNC_SUCCESS;
}

/* Create the preview.clifm file. If possible, it is imported from the
 * data directory. */
static int
create_preview_file(void)
{
	char file[PATH_MAX + 1];
	snprintf(file, sizeof(file), "%s/preview.clifm", config_dir);

	struct stat attr;
	if (stat(file, &attr) == FUNC_SUCCESS)
		return FUNC_SUCCESS;

#if !defined(__FreeBSD__) && !defined(__OpenBSD__) && !defined(__NetBSD__) \
&& !defined(__DragonFly__) && !defined(__APPLE__)
	if (import_from_data_dir("preview.clifm", file, 0) == FUNC_SUCCESS)
		return FUNC_SUCCESS;
#endif /* !BSD */

	int fd = 0;
	FILE *fp = open_fwrite(file, &fd);
	if (!fp) {
		err('e', PRINT_PROMPT, "%s: Cannot create preview file "
			"'%s': %s\n", PROGRAM_NAME, file, strerror(errno));
		return FUNC_FAILURE;
	}

#if defined(_BE_POSIX)
	char lscmd[] = "ls -Ap;";
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) \
|| defined(__DragonFly__) || defined(__sun) || defined(__APPLE__)
	char lscmd[] = "gls -Ap --color=always --indicator-style=none;ls -Ap;";
#else
	char lscmd[] = "ls -Ap --color=always --indicator-style=none;";
#endif /* !BSD */

	fprintf(fp, "                  ######################################\n\
                  #   Configuration file for Shotgun   #\n\
                  #       Clifm's file previewer       #\n\
                  ######################################\n\
\n\
# Commented and blank lines are ignored\n\
\n\
# It is recommended to edit this file setting your preferred applications\n\
# first: the previewing process will be smoother and faster this way\n\
# You can even remove whatever applications you don't use\n\
\n\
# For syntax details consult the mimelist.clifm file\n\
\n\
# Uncomment this line to use pistol (or any other previewing program)\n\
#.*=pistol\n\
\n\
# Uncomment and edit this line to use Ranger's scope script:\n\
#.*=~/.config/ranger/scope.sh %%f 120 80 /tmp/clifm/ True\n\
\n\
# Uncomment to enable image previews for the corresponding file types:\n\
;^text/rtf$|^application/.*(officedocument|msword|ms-excel|ms-powerpoint|opendocument).*=~/.config/clifm/clifmimg doc %%f %%u;\n\
;^application/epub\\+zip$=~/.config/clifm/clifmimg epub %%f %%u;\n\
;^application/x-mobipocket-ebook$=~/.config/clifm/clifmimg mobi %%f %%u;\n\
;^application/pdf$=~/.config/clifm/clifmimg pdf %%f %%u;\n\
;^image/vnd.djvu$=~/.config/clifm/clifmimg djvu %%f %%u;\n\
;^image/svg\\+xml$=~/.config/clifm/clifmimg svg %%f %%u;\n\n\
# Display images directly via the 'image' method\n\
;^image/(jpeg|png|tiff|webp|x-xwindow-dump)$=~/.config/clifm/clifmimg image %%f %%u;\n\
# Convert and display via the 'gif' method\n\
;^image/.*=~/.config/clifm/clifmimg gif %%f %%u;\n\
;N:.*\\.ora$=~/.config/clifm/clifmimg gif %%f %%u;\n\
;N:.*\\.(kra|krz)$=~/.config/clifm/clifmimg krita %%f %%u;\n\n\
;^video/.*|^application/(mxf|x-shockwave-flash|vnd.rn-realmedia)$=~/.config/clifm/clifmimg video %%f %%u;\n\
;^audio/.*=~/.config/clifm/clifmimg audio %%f %%u;\n\
;^application/postscript$=~/.config/clifm/clifmimg postscript %%f %%u;\n\
;^font/.*|^application/(font.*|.*opentype)=~/.config/clifm/clifmimg font %%f %%u;\n\
;N:.*\\.(cbz|cbr|cbt)$=~/.config/clifm/clifmimg comic %%f %%u;\n\
\n\
# Directories\n\
inode/directory=exa -a --tree --level=1;lsd -A --tree --depth=1 --color=always;tree -a -L 1;%s\n\
\n\
# Web content\n\
^text/html$=w3m -dump;lynx -dump;elinks -dump;pandoc -s -t markdown;\n\
\n\
# Text\n\
^text/rtf=catdoc;\n\
N:.*\\.json$=jq --color-output . ;python -m json.tool;\n\
N:.*\\.md$=glow -s dark;mdcat;\n\
^text/.*|^application/javascript$=highlight -f --out-format=xterm256 --force;bat --style=plain --color=always;cat;\n\
\n\
# Office documents\n\
N:.*\\.xlsx$=xlsx2csv;file -b;\n\
N:.*\\.(odt|ods|odp|sxw)$=odt2txt;pandoc -s -t markdown;\n\
^application/(.*wordprocessingml.document|.*epub+zip|x-fictionbook+xml)=pandoc -s -t markdown;\n\
^application/msword=catdoc;\n\
^application/ms-excel=xls2csv;\n\
\n\
# Archives\n\
N:.*\\.rar=unrar lt -p-;\n\
application/(zstd|x-rpm|debian.binary-package)=bsdtar --list --file;\n\
application/(zip|gzip|x-7z-compressed|x-xz|x-bzip*|x-tar)=atool --list;bsdtar --list --file;\n\
\n\
# PDF\n\
^application/pdf$=pdftotext -l 10 -nopgbrk -q -- %%f -;mutool draw -F txt -i;exiftool;\n\
\n\
# Image, video, and audio\n\
^image/vnd.djvu=djvutxt;exiftool;\n\
^image/.*=exiftool;\n\
^video/.*=mediainfo;exiftool;\n\
^audio/.*=mediainfo;exiftool;\n\
\n\
# Torrent:\n\
application/x-bittorrent=transmission-show;\n\
\n\
# Fallback\n\
.*=file -b;\n",
	lscmd);

	fclose(fp);
	return FUNC_SUCCESS;
}

static int
create_actions_file(char *file)
{
	struct stat attr;
	/* If the file already exists, do nothing */
	if (stat(file, &attr) == FUNC_SUCCESS)
		return FUNC_SUCCESS;

	/* If not, try to import it from DATADIR */
	if (import_from_data_dir("actions.clifm", file, 0) == FUNC_SUCCESS)
		return FUNC_SUCCESS;

	/* Else, create it */
	int fd = 0;
	FILE *fp = open_fwrite(file, &fd);
	if (!fp) {
		err('e', PRINT_PROMPT, "%s: Cannot create actions file "
			"'%s': %s\n", PROGRAM_NAME, file, strerror(errno));
		return FUNC_FAILURE;
	}

	fprintf(fp, "######################\n"
		"# Actions file for %s #\n"
		"######################\n\n"
		"# Define here your custom actions. Actions are "
		"custom command names\n"
		"# bound to an executable file located either in "
		"DATADIR/clifm/plugins\n"
		"# (usually /usr/local/share/clifm/plugins) or in "
		"$XDG_CONFIG_HOME/clifm/plugins (usually ~/.config/clifm/plugins).\n"
		"# Actions can be executed directly from "
		"%s command line, as if they\n"
		"# were any other command, and the associated "
		"file will be executed\n"
		"# instead. All parameters passed to the action "
		"command will be passed\n"
		"# to the corresponding plugin as well.\n\n"
		"+=finder.sh\n"
		"++=jumper.sh\n"
		"-=fzfnav.sh\n"
		"*=fzfsel.sh\n"
		"**=fzfdesel.sh\n"
		"//=rgfind.sh\n"
		"_=fzcd.sh\n"
		"bcp=batch_copy.sh\n"
		"bmi=bm_import.sh\n"
		"bn=batch_create.sh\n"
		"clip=clip.sh\n"
		"cr=cprm.sh\n"
		"da=disk_analyzer.sh\n"
		"dr=dragondrop.sh\n"
		"fdups=fdups.sh\n"
		"gg=pager.sh\n"
		"h=fzfhist.sh\n"
		"i=img_viewer.sh\n"
		"ih=ihelp.sh\n"
		"kd=decrypt.sh\n"
		"ke=encrypt.sh\n"
		"music=music_player.sh\n"
		"ml=mime_list.sh\n"
		"plugin1=xclip.sh\n"
		"ptot=pdf_viewer.sh\n"
		"update=update.sh\n"
		"vid=vid_viewer.sh\n"
		"vt=virtualize.sh\n"
		"wall=wallpaper_setter.sh\n",
	    PROGRAM_NAME, PROGRAM_NAME);

	fclose(fp);
	return FUNC_SUCCESS;
}

static char *
define_tmp_rootdir(int *from_env)
{
	char *temp = (char *)NULL;
	*from_env = 1;

	if (xargs.secure_env != 1 && xargs.secure_env_full != 1) {
		temp = getenv("CLIFM_TMPDIR");
		if (!temp || !*temp)
			temp = getenv("TMPDIR");
	}

	if (!temp || !*temp) {
		*from_env = 0;
		temp = P_tmpdir;
	}

	char *p = temp;
	if (*temp != '/') {
		p = normalize_path(temp, strlen(temp));
		if (!p)
			p = temp;
	}

	size_t len = strlen(p);
	char *tmp_root_dir = savestring(p, len);
	if (p != temp)
		free(p);

	if (len > 1 && tmp_root_dir[len - 1] == '/')
		tmp_root_dir[len - 1] = '\0';

	return tmp_root_dir;
}

/* Define and create the root of our temporary directory. Checks are made in
 * this order: CLIFM_TMPDIR, TMPDIR, P_tmpdir, and /tmp */
static char *
create_tmp_rootdir(void)
{
	int from_env = 0;
	char *tmp_root_dir = define_tmp_rootdir(&from_env);
	if (!tmp_root_dir || !*tmp_root_dir)
		goto END;

	struct stat a;

	if (stat(tmp_root_dir, &a) != -1)
		return tmp_root_dir;

	int ret = FUNC_SUCCESS;
	char *cmd[] = {"mkdir", "-p", "--", tmp_root_dir, NULL};
	if ((ret = launch_execv(cmd, FOREGROUND, E_NOSTDERR)) == FUNC_SUCCESS)
		return tmp_root_dir;

	if (from_env == 0) /* Error creating P_tmpdir */
		goto END;

	err('w', PRINT_PROMPT, _("%s: '%s': %s.\n"
		"Cannot create temporary directory. Falling back to '%s'.\n"),
		PROGRAM_NAME, tmp_root_dir, strerror(ret), P_tmpdir);

	free(tmp_root_dir);
	tmp_root_dir = savestring(P_tmpdir, P_tmpdir_len);

	if (stat(tmp_root_dir, &a) != -1)
		return tmp_root_dir;

	char *cmd2[] = {"mkdir", "-p", "--", tmp_root_dir, NULL};
	if (launch_execv(cmd2, FOREGROUND, E_NOSTDERR) == FUNC_SUCCESS)
		return tmp_root_dir;

END: /* If everything fails, fallback to /tmp */
	free(tmp_root_dir);
	return savestring("/tmp", 4);
}

static void
define_selfile(void)
{
	/* SEL_FILE should have been set before by set_sel_file(). If not,
	 * we do not have access to the config dir, in which case we use TMP_DIR. */
	if (sel_file)
		return;

	const size_t tmpdir_len = strlen(tmp_dir);
	size_t len = 0;
	/* If the config directory isn't available, define an alternative
	 * selection file in TMP_ROOTDIR (if available). */
	if (conf.share_selbox == 0) {
		size_t prof_len = alt_profile ? strlen(alt_profile) : 7;
		/* 7 == lenght of "default" */

		len = tmpdir_len + prof_len + 15;
		sel_file = xnmalloc(len, sizeof(char));
		snprintf(sel_file, len, "%s/selbox_%s.clifm", tmp_dir,
		    alt_profile ? alt_profile : "default");
	} else {
		len = tmpdir_len + 14;
		sel_file = xnmalloc(len, sizeof(char));
		snprintf(sel_file, len, "%s/selbox.clifm", tmp_dir);
	}

	err('w', PRINT_PROMPT, _("%s: %s: Using a temporary directory for "
		"the Selection Box. Selected files will not be persistent across "
		"reboots.\n"), PROGRAM_NAME, tmp_dir);
}

void
create_tmp_files(void)
{
	if (xargs.stealth_mode == 1)
		return;

	free(tmp_rootdir); /* In case we come from reload_config() */
	tmp_rootdir = create_tmp_rootdir();

	const size_t tmp_rootdir_len = strlen(tmp_rootdir);
	const size_t pnl_len = sizeof(PROGRAM_NAME) - 1;
	const size_t user_len = user.name ? strlen(user.name) : 7;
                                       /* 7: len of "unknown" */

#define MAX_TRIES 100

	const size_t tmp_len = tmp_rootdir_len + pnl_len + user_len
		+ 3 + DIGINUM(MAX_TRIES) + 1;
	tmp_dir = xnmalloc(tmp_len, sizeof(char));
	snprintf(tmp_dir, tmp_len, "%s/%s-%s", tmp_rootdir,
		PROGRAM_NAME, user.name ? user.name : "unknown");

	int suffix = 0;
	struct stat a;

	/* Loop until we get a valid tmp directory or MAX_TRIES is reached. */
	while (1) {
		const int ret = lstat(tmp_dir, &a);
		if (ret == 0 && S_ISDIR(a.st_mode)
		&& check_file_access(a.st_mode, a.st_uid, a.st_uid) == 1)
			break;

		if (ret == -1 && xmkdir(tmp_dir, S_IRWXU) == FUNC_SUCCESS)
			break;

		suffix++;
		if (suffix > MAX_TRIES) {
			snprintf(tmp_dir, tmp_len, "%s", tmp_rootdir);
			err('e', PRINT_PROMPT, _("%s: Cannot create temporary directory. "
				"Falling back to '%s'.\n"), PROGRAM_NAME, tmp_rootdir);
			break;
		}

		/* Append suffix and try again. */
		snprintf(tmp_dir, tmp_len, "%s/%s-%s.%d", tmp_rootdir,
			PROGRAM_NAME, user.name ? user.name : "unknown", suffix);
	}

#undef MAX_TRIES

	define_selfile();
}

/* Set the main configuration directory. Three sources are examined:
 * 1. Alternative config dir, as set via -D,--config-dir.
 * 2. XDG_CONFIG_HOME/clifm (if not secure-env).
 * 3. user.home/.config/clifm: $HOME if not secure-env or pw_dir from the
 * passwd database. */
static void
set_main_config_dir(const int secure_mode)
{
	if (alt_config_dir && *alt_config_dir) {
		config_dir_gral = savestring(alt_config_dir, strlen(alt_config_dir));
		return;
	}

	char *env = secure_mode == 0 ? getenv("XDG_CONFIG_HOME") : (char *)NULL;
	char *home_env = (env && *env) ? normalize_path(env, strlen(env))
		: (char *)NULL;

	if (home_env && *home_env) {
		const size_t len = strlen(home_env) + (sizeof(PROGRAM_NAME) - 1) + 2;
		config_dir_gral = xnmalloc(len, sizeof(char));
		snprintf(config_dir_gral, len, "%s/%s", home_env, PROGRAM_NAME);
		free(home_env);
		return;
	}

	free(home_env);

	const size_t len = user.home_len + (sizeof(PROGRAM_NAME) - 1) + 10;
	config_dir_gral = xnmalloc(len, sizeof(char));
	snprintf(config_dir_gral, len, "%s/.config/%s", user.home, PROGRAM_NAME);
}

/* Set the profile directory. Two sources are examined:
 * 1. CONFIG_DIR/profiles/PROF (if PROF is set via -P,--profile)
 * 2. CONFIG_DIR/profiles/default
 * CONFIG_DIR is set by set_main_config_dir() */
static void
set_profile_dir(const size_t config_gral_len)
{
	/* alt_profile will not be NULL whenever the -P option is used
	 * to run clifm under an alternative profile. */
	if (alt_profile && *alt_profile) {
		const size_t len = config_gral_len + strlen(alt_profile) + 11;
		config_dir = xnmalloc(len, sizeof(char));
		snprintf(config_dir, len, "%s/profiles/%s",
			config_dir_gral, alt_profile);
	} else {
		const size_t len = config_gral_len + 18;
		config_dir = xnmalloc(len, sizeof(char));
		snprintf(config_dir, len, "%s/profiles/default", config_dir_gral);
	}

	config_dir_len = strlen(config_dir);
}

/* Set the main configuration file. Two sources are examined:
 * 1. Alternative config file, from the command line (-c,--config-file).
 * 3. PROFILE_DIR/clifmrc: PROFILE_DIR is set by set_profile_dir() */
static void
set_main_config_file(void)
{
	if (alt_config_file && *alt_config_file) {
		config_file = savestring(alt_config_file, strlen(alt_config_file));
		return;
	}

	const size_t len = config_dir_len + (sizeof(PROGRAM_NAME) - 1) + 4;
	config_file = xnmalloc(len, sizeof(char));
	snprintf(config_file, len, "%s/%src", config_dir, PROGRAM_NAME);
}

/* Set the history file, as defined in CLIFM_HISTFILE environment variable
 * or, if unset, using the main configuration directory. */
static void
set_hist_file(const int secure_mode, const size_t tmp_len)
{
	char *env_val = secure_mode == 0 ? getenv("CLIFM_HISTFILE") : (char *)NULL;
	char *hist_env = (env_val && *env_val)
		? normalize_path(env_val, strlen(env_val)) : (char *)NULL;

	const size_t hist_len =
		(hist_env && *hist_env) ? strlen(hist_env) + 1 : tmp_len;
	hist_file = xnmalloc(hist_len, sizeof(char));

	if (hist_env && *hist_env)
		xstrsncpy(hist_file, hist_env, hist_len);
	else
		snprintf(hist_file, hist_len, "%s/history.clifm", config_dir);

	free(hist_env);
}

static void
check_file_safety(const char *name)
{
	int safe = 1;
	struct stat a;

	if (!name || !*name || lstat(name, &a) == -1)
		return;

	else if (S_ISLNK(a.st_mode)) {
		err('w', PRINT_PROMPT, _("%s: Is a symbolic link\n"), name);
		safe = 0;
	}

	else if (!S_ISREG(a.st_mode)) {
		err('w', PRINT_PROMPT, _("%s: Is not a regular file\n"), name);
		safe = 0;
	}

	else if (a.st_nlink > 1) {
		err('w', PRINT_PROMPT, _("%s: There is another name hard linked "
			"to this file\n"), name);
		safe = 0;
	}

	if (safe == 0)
		err('w', PRINT_PROMPT, _("%s: File may be unsafe\n"), name);
}

static void
check_config_files_integrity(void)
{
	check_file_safety(config_file);
	check_file_safety(hist_file);
	check_file_safety(kbinds_file);
	check_file_safety(dirhist_file);
	check_file_safety(bm_file);
	check_file_safety(msgs_log_file);
	check_file_safety(cmds_log_file);
	check_file_safety(profile_file);
	check_file_safety(mime_file);
	check_file_safety(actions_file);
	check_file_safety(remotes_file);
	check_file_safety(prompts_file);

	char jump_file[PATH_MAX + 1];
	snprintf(jump_file, sizeof(jump_file), "%s/jump.clifm", config_dir);
	check_file_safety(jump_file);

	char preview_file[PATH_MAX + 1];
	snprintf(preview_file, sizeof(preview_file),
		"%s/preview.clifm", config_dir);
	check_file_safety(preview_file);
}

static void
set_thumbnails_dir(void)
{
	if (thumbnails_dir)
		return;

	const int se = (xargs.secure_env == 1 || xargs.secure_env_full == 1);

	const char *p = se == 0 ? getenv("XDG_CACHE_HOME") : (char *)NULL;
	if ((!p || !*p) && (!user.home || !*user.home))
		return;

	if (p && *p) {
		const size_t len = strlen(p) + sizeof(PROGRAM_NAME) + 30;
		thumbnails_dir = xnmalloc(len, sizeof(char));
		snprintf(thumbnails_dir, len, "%s/%s/thumbnails", p, PROGRAM_NAME);
	} else {
		const size_t len = user.home_len + sizeof(PROGRAM_NAME) + 26;
		thumbnails_dir = xnmalloc(len, sizeof(char));
		snprintf(thumbnails_dir, len, "%s/.cache/%s/thumbnails",
			user.home, PROGRAM_NAME);
	}
}

static void
define_config_file_names(void)
{
	size_t tmp_len = 0;
	const int secure_mode =
		(xargs.secure_env == 1 || xargs.secure_env_full == 1);

	set_main_config_dir(secure_mode); /* config_dir_gral is set here. */
	const size_t config_gral_len = strlen(config_dir_gral);
	set_profile_dir(config_gral_len); /* config_dir is set here */
	set_main_config_file();

	tmp_len = config_dir_len + 6;
	tags_dir = xnmalloc(tmp_len, sizeof(char));
	snprintf(tags_dir, tmp_len, "%s/tags", config_dir);

	if (alt_kbinds_file) {
		kbinds_file = savestring(alt_kbinds_file, strlen(alt_kbinds_file));
	} else {
		/* Keybindings per user, not per profile. */
		tmp_len = config_gral_len + 19;
		kbinds_file = xnmalloc(tmp_len, sizeof(char));
		snprintf(kbinds_file, tmp_len, "%s/keybindings.clifm", config_dir_gral);
	}

	tmp_len = config_gral_len + 8;
	colors_dir = xnmalloc(tmp_len, sizeof(char));
	snprintf(colors_dir, tmp_len, "%s/colors", config_dir_gral);

	tmp_len = config_gral_len + 9;
	plugins_dir = xnmalloc(tmp_len, sizeof(char));
	snprintf(plugins_dir, tmp_len, "%s/plugins", config_dir_gral);

	tmp_len = config_dir_len + 15;
	dirhist_file = xnmalloc(tmp_len, sizeof(char));
	snprintf(dirhist_file, tmp_len, "%s/dirhist.clifm", config_dir);

	if (!alt_bm_file) {
		tmp_len = config_dir_len + 17;
		bm_file = xnmalloc(tmp_len, sizeof(char));
		snprintf(bm_file, tmp_len, "%s/bookmarks.clifm", config_dir);
	} else {
		bm_file = savestring(alt_bm_file, strlen(alt_bm_file));
	}

	if (!alt_mimelist_file) {
		tmp_len = config_dir_len + 16;
		mime_file = xnmalloc(tmp_len, sizeof(char));
		snprintf(mime_file, tmp_len, "%s/mimelist.clifm", config_dir);
	} else {
		mime_file = savestring(alt_mimelist_file, strlen(alt_mimelist_file));
	}

	tmp_len = config_dir_len + 15;
	msgs_log_file = xnmalloc(tmp_len, sizeof(char));
	snprintf(msgs_log_file, tmp_len, "%s/msglogs.clifm", config_dir);

	cmds_log_file = xnmalloc(tmp_len, sizeof(char));
	snprintf(cmds_log_file, tmp_len, "%s/cmdlogs.clifm", config_dir);

	set_hist_file(secure_mode, tmp_len);
	set_thumbnails_dir();

	tmp_len = config_dir_len + 15;
	profile_file = xnmalloc(tmp_len, sizeof(char));
	snprintf(profile_file, tmp_len, "%s/profile.clifm", config_dir);

	tmp_len = config_dir_len + 15;
	actions_file = xnmalloc(tmp_len, sizeof(char));
	snprintf(actions_file, tmp_len, "%s/actions.clifm", config_dir);

	tmp_len = config_dir_len + 12;
	remotes_file = xnmalloc(tmp_len, sizeof(char));
	snprintf(remotes_file, tmp_len, "%s/nets.clifm", config_dir);
}

static int
import_data_file(const char *src, const char *dst, const int exec)
{
	if (!data_dir || !config_dir_gral || !src || !dst)
		return FUNC_FAILURE;

	char dest_file[PATH_MAX + 1];
	snprintf(dest_file, sizeof(dest_file), "%s/%s", config_dir_gral, dst);
	struct stat a;
	if (lstat(dest_file, &a) == 0)
		return FUNC_SUCCESS;

	return import_from_data_dir(src, dest_file, exec);
}

/* Create the main configuration file and save it into FILE. */
int
create_main_config_file(char *file)
{
	/* First, try to import it from DATADIR */
	char src_filename[NAME_MAX];
	snprintf(src_filename, sizeof(src_filename), "%src", PROGRAM_NAME);
	if (import_from_data_dir(src_filename, file, 0) == FUNC_SUCCESS)
		return FUNC_SUCCESS;

	/* If not found, create it */
	int fd;
	FILE *config_fp = open_fwrite(file, &fd);

	if (!config_fp) {
		err('e', PRINT_PROMPT, "%s: Cannot create configuration "
			"file '%s': %s\nFalling back to default values\n",
			PROGRAM_NAME, file, strerror(errno));
		return FUNC_FAILURE;
	}

	fprintf(config_fp,

"\t\t###########################################\n\
\t\t#                  CLIFM                  #\n\
\t\t#      The Command Line File Manager      #\n\
\t\t###########################################\n\n"

	    "# This is the configuration file for Clifm\n\n"

		"# Lines starting with '#' or ';' are commented out (ignored).\n\
# Uncomment an option to override the default value.\n\n"

	    "# Set the color scheme.\n\
;ColorScheme=%s\n\n"

	    "# Display number of files contained by directories when listing files.\n\
;FilesCounter=%s\n\n"

		"# How to list files: 0 = vertically (like ls(1) would), 1 = horizontally.\n\
;ListingMode=%d\n\n"

		"# List files automatically after changing the current directory.\n\
;AutoLs=%s\n\n"

		"# Display errors, warnings, and notices using desktop notifications ('kitty', 'system', or 'false')\n\
;DesktopNotifications=%s\n\n"

	    "# Print a map of the current position in the directory\n\
# history list, showing previous, current, and next entries.\n\
;DirhistMap=%s\n\n"

		"# Use a regex expression to filter filenames when listing files.\n\
# Run 'help file-filters' for more information.\n\
;Filter=""\n\n"

	    "# Set the default copy command. Available options are:\n\
# 0: 'cp -Rp', 1: 'cp -Rp' (force), 2: 'advcp -gRp', 3: 'advcp -gRp' (force),\n\
# 4: 'wcp', and 5: 'rsync -avP'\n\
# Note: 2-5 include a progress bar\n\
;cpCmd=%d\n\n"

	    "# Set the default move command. Available options are:\n\
# 0: 'mv', 1: 'mv' (force), 2: 'advmv -g', and 3: 'advmv -g' (force)\n\
# Note: 2 and 3 include a progress bar\n\
;mvCmd=%d\n\n"

		"# If set to true, the 'r' command will never prompt before removals.\n\
;rmForce=%s\n\n",

		"# Default answers for specific confirmation prompts:\n\
;DefaultAnswer=\"\"\n\n"

		DEF_COLOR_SCHEME,
		DEF_FILES_COUNTER == 1 ? "true" : "false",
		DEF_LISTING_MODE,
		DEF_AUTOLS == 1 ? "true" : "false",
		DEF_DESKTOP_NOTIFICATIONS == DESKTOP_NOTIF_SYSTEM ? "system"
			: (DEF_DESKTOP_NOTIFICATIONS == DESKTOP_NOTIF_KITTY ? "kitty"
			: "false"),
		DEF_DIRHIST_MAP == 1 ? "true" : "false",
		DEF_CP_CMD,
		DEF_MV_CMD,
		DEF_RM_FORCE == 1 ? "true" : "false");

	fprintf(config_fp,
		"# How 'l' creates symlinks (absolute, relative, literal).\n\
;LinkCreationMode=%s\n\n"
		"# Enable fuzzy matching for filename/path completions and suggestions.\n\
;FuzzyMatching=%s\n\n"

		"# Fuzzy matching algorithm: 1 (faster, non-Unicode), 2 (slower, Unicode).\n\
;FuzzyAlgorithm=%d\n\n"

		"# letion mode: 'standard', 'fzf', 'fnf', or 'smenu'. Defaults to\n\
# 'fzf' if the binary is found in PATH. Otherwise, the standard mode is used.\n\
;TabCompletionMode=\n\n"

		"# File previews for tab completion (fzf mode only). Possible values:\n\
# true, false, hidden (enabled, but hidden; toggle it with Alt+p)\n\
;FzfPreview=%s\n\n"

		"# Do not preview files larger than this value (-1 for unlimited).\n\
;PreviewMaxSize=%d\n\n"

	    ";WelcomeMessage=%s\n\
;WelcomeMessageStr=\"\"\n\n\
# Print %s's logo screen at startup.\n\
;SplashScreen=%s\n\n\
# Show hidden files (true, false, first, last)\n\
;ShowHiddenFiles=%s\n\n\
# Display extended file metadata next to filenames (long listing format).\n\
;LongViewMode=%s\n\
# Properties fields to be printed in long view mode.\n\
# f = file counter for directories\n\
# B = file allocated blocks\n\
# d = inode number\n\
# l = number of hard links\n\
# p|n = permissions: either symbolic (p) or numeric/octal (n)\n\
# i|I = user/group IDs: as number (i) or name (I)\n\
# G = if i|I is set, don't print group\n\
# a|b|m|c = last (a)ccess, (b)irth, (m)odification, or status (c)hange time\n\
# s|S = size (either human readable (s) or bytes (S))\n\
# x = extended attributes/ACL/capabilities (marked as '@') (requires p|n)\n\
# A single dash (\'-\') disables all fields\n\
;PropFields=\"%s\"\n\
# Append a mark to timestamps in long view to identify the kind of timestamp.\n\
;TimestampMark=%s\n\
# Make time field in long view follow sort if sorting by time.\n\
;TimeFollowsSort=%s\n\
# Number of spaces between fields in long view (1-2).\n\
;PropFieldsGap=%d\n\
# Format used to print timestamps in long view. Available options:\n\
# default, relative, iso, long-iso, full-iso, +FORMAT (see strftime(3))\n\
;TimeStyle=\"\"\n\
# Same as TimeStyle, but for the 'p/pp' command (relative time not supported)\n\
;PTimeStyle=\"\"\n\
# Print files apparent size instead of actual device usage\n\
;ApparentSize=%s\n\
# If running in long view, print directories total size\n\
;FullDirSize=%s\n\n\
# Log errors and warnings\n\
;LogMsgs=%s\n\
# Log commands entered in the command line\n\
;LogCmds=%s\n\n"

	    "# Minimum length at which a filename can be truncated in long view mode.\n\
# If running in long mode, this setting overrides MaxFilenameLen whenever\n\
# this latter is smaller than MINNAMETRUNCATE.\n\
;MinNameTruncate=%d\n\n"

	    "# When a directory rank in the jump database is below MinJumpRank, it\n\
# is removed. If set to 0, directories are kept indefinitely.\n\
;MinJumpRank=%d\n\n"

	    "# When the sum of all ranks in the jump database reaches MaxJumpTotalRank,\n\
# all ranks will be reduced using a dynamic factor so that the total sum falls\n\
# below MaxJumpTotalRank again. Those entries falling below MinJumpRank will\n\
# be deleted.\n\
;MaxJumpTotalRank=%d\n\n"

		"# Automatically purge the jump database from non-existing directories.\n\
;PurgeJumpDB=%s\n\n"

	    "# Should Clifm be allowed to run external, shell commands?\n\
;ExternalCommands=%s\n\n"

	    "# Write the last visited directory to ~/.config/clifm/.last to be\n\
# later accessed by the corresponding shell function at program exit.\n\
# To enable this feature consult the manpage.\n\
;CdOnQuit=%s\n\n",

		DEF_LINK_CREATION_MODE == LNK_CREAT_ABS ? "absolute"
			: (DEF_LINK_CREATION_MODE == LNK_CREAT_REL ? "relative"
			: "literal"),
		DEF_FUZZY_MATCH == 1 ? "true" : "false",
		DEF_FUZZY_MATCH_ALGO,
		DEF_FZF_PREVIEW == 1 ? "true" : "false",
		DEF_PREVIEW_MAX_SIZE,
		DEF_WELCOME_MESSAGE == 1 ? "true" : "false",
		PROGRAM_NAME,
		DEF_SPLASH_SCREEN == 1 ? "true" : "false",
		DEF_SHOW_HIDDEN == 1 ? "true" : "false",
		DEF_LONG_VIEW == 1 ? "true" : "false",
		DEF_PROP_FIELDS,
		DEF_TIMESTAMP_MARK == 1 ? "true" : "false",
		DEF_TIME_FOLLOWS_SORT == 1 ? "true" : "false",
		DEF_PROP_FIELDS_GAP,
		DEF_APPARENT_SIZE == 1 ? "true" : "false",
		DEF_FULL_DIR_SIZE == 1 ? "true" : "false",
		DEF_LOG_MSGS == 1 ? "true" : "false",
		DEF_LOG_CMDS == 1 ? "true" : "false",
		DEF_MIN_NAME_TRUNC,
		DEF_MIN_JUMP_RANK,
		DEF_MAX_JUMP_TOTAL_RANK,
		DEF_PURGE_JUMPDB == 1 ? "true" : "false",
		DEF_EXT_CMD_OK == 1 ? "true" : "false",
		DEF_CD_ON_QUIT == 1 ? "true" : "false"
		);

	fprintf(config_fp,
	    "# If set to true, a command name that is the name of a directory or a\n\
# file is executed as if it were the argument to the the 'cd' or the \n\
# 'open' commands respectively: 'cd DIR' works the same as just 'DIR'\n\
# and 'open FILE' works the same as just 'FILE'.\n\
;Autocd=%s\n\
;AutoOpen=%s\n\n"

		"# Enable autocommand files (.cfm.in and .cfm.out).\n\
;ReadAutocmdFiles=%s\n\n"

		"# How to display autocommand notifications (none, mini, short, long,\n\
# full, prompt)\n\
;InformAutocmd=prompt\n\n"

		"# Read .hidden files.\n\
;ReadDotHidden=%s\n\n"

	    "# Enable auto-suggestions.\n\
;AutoSuggestions=%s\n\n"

	    "# The following checks will be performed in the order specified\n\
# by SuggestionStrategy. Available checks:\n\
# a = Aliases names\n\
# c = Path completion\n\
# e = ELN's\n\
# f = Filenames in current directory\n\
# h = Commands history\n\
# j = Jump database\n\
;SuggestionStrategy=%s\n\n"

	    "# Suggest filenames using the corresponding file type color\n\
# (set via the color scheme file).\n\
;SuggestFiletypeColor=%s\n\n"

		"# Suggest a brief decription for internal commands.\n\
;SuggestCmdDesc=%s\n\n"

";SyntaxHighlighting=%s\n\n"

	"# How to quote expanded ELN's (regular files only): backslash, single, double.\n\
;QuotingStyle=%s\n\n"

		"# We have three search strategies: 0 = glob-only, 1 = regex-only,\n\
# and 2 = glob-regex.\n\
;SearchStrategy=%d\n\n",

		DEF_AUTOCD == 1 ? "true" : "false",
		DEF_AUTO_OPEN == 1 ? "true" : "false",
		DEF_READ_AUTOCMD_FILES == 1 ? "true" : "false",
		DEF_READ_DOTHIDDEN == 1 ? "true" : "false",
		DEF_SUGGESTIONS == 1 ? "true" : "false",
		DEF_SUG_STRATEGY,
		DEF_SUG_FILETYPE_COLOR == 1 ? "true" : "false",
		DEF_CMD_DESC_SUG == 1 ? "true" : "false",
		DEF_HIGHLIGHT == 1 ? "true" : "false",
		DEF_QUOTING_STYLE == QUOTING_STYLE_BACKSLASH ? "backslash"
			: (DEF_QUOTING_STYLE == QUOTING_STYLE_DOUBLE_QUOTES ? "double"
			: "single"),
		DEF_SEARCH_STRATEGY
		);

	fprintf(config_fp,
	    "# In light mode, extra file type checks (except those provided by\n\
# the d_type field of the dirent structure (see readdir(3))\n\
# are disabled to speed up the listing process. Because of this, we cannot\n\
# know in advance if a file is readable by the current user, if it is executable,\n\
# SUID, SGID, if a symlink is broken, and so on. The file extension check is\n\
# ignored as well, so that the color per extension feature is disabled.\n\
;LightMode=%s\n\n"

	    "# If running with colors, append directory indicator\n\
# to directories. If running without colors (via the --no-color option),\n\
# append file type indicator at the end of filenames.\n\
;Classify=%s\n\n"

		"# Color links as target filename.\n\
;ColorLinksAsTarget=%s\n\n"

	    "# Should the Selection Box be shared among different profiles?\n\
;ShareSelbox=%s\n\n"

	    "# Choose the file opener for opening files with their default associated\n\
# application. If not set, Lira (Clifm's builtin opener) is used.\n\
;Opener=\n\n"

	    "# Only used when opening a directory via a new Clifm instance (with the 'x'\n\
# command), this option specifies the command to be used to launch a\n\
# terminal emulator to run Clifm on it.\n\
;TerminalCmd='%s'\n\n"

	    "# How to sort files: none, name, size, atime, btime, ctime, mtime,\n\
# version, extension, inode, owner, group, blocks, links, type.\n\
;Sort=version\n\
# Sort in reverse order\n\
;SortReverse=%s\n\
# Files starting with PrioritySortChar are listed at top of the file list.\n\
;PrioritySortChar:%s\n\n"

	"# If set to true, settings changed in the current workspace (only via\n\
# the command line or keyboard shortcuts) are kept private to that workspace\n\
# and made persistent (for the current session only), even when switching\n\
# workspaces.\n\
;PrivateWorkspaceSettings=%s\n\n"

		"# A comma separated list of workspace names in the form NUM=NAME\n\
# Example: \"1=MAIN,2=EXTRA,3=GIT,4=WORK\" or \"1=α,2=β,3=γ,4=δ\".\n\
;WorkspaceNames=\"\"\n\n"

	    "# Print a usage tip at startup.\n\
;Tips=%s\n\n\
;ListDirsFirst=%s\n\n\
# Enable case sensitive listing for files in the current directory.\n\
;CaseSensitiveList=%s\n\n\
# Enable case sensitive lookups for the directory jumper function (via \n\
# the 'j' command).\n\
;CaseSensitiveDirJump=%s\n\n\
# Enable case sensitive completion for filenames.\n\
;CaseSensitivePathComp=%s\n\n\
# Enable case sensitive search.\n\
;CaseSensitiveSearch=%s\n\
# Skip non-alphanumeric characters when sorting files ('version' or 'name').\n\
;SkipNonAlnumPrefix=%s\n\n\
# Mas, the file list pager. Possible values are:\n\
# 0/false: Disable the pager\n\
# 1/true: Run the pager whenever the list of files does not fit on the screen\n\
# >1: Run the pager whenever the number of files in the current directory is\n\
# greater than or equal to this value (e.g., 1000)\n\
;Pager=%s\n\
# Define how to list files in the pager: auto (default), short, long\n\
;PagerView=%s\n\n"

	"# Maximum filename length for listed files. If TruncateNames is set to\n\
# true, names larger than MAXFILENAMELEN will be truncated at MAXFILENAMELEN\n\
# using a tilde.\n\
# Set it to -1 (or leave empty) to remove this limit.\n\
# When running in long mode, this setting is overriden by MinNameTruncate\n\
# whenever MAXFILENAMELEN is smaller than MINNAMETRUNCATE.\n\
;MaxFilenameLen=%d\n\n\
# Truncate filenames longer than MAXFILENAMELEN.\n\
;TruncateNames=%s\n\n",

		DEF_LIGHT_MODE == 1 ? "true" : "false",
		DEF_CLASSIFY == 1 ? "true" : "false",
		DEF_COLOR_LNK_AS_TARGET == 1 ? "true" : "false",
		DEF_SHARE_SELBOX == 1 ? "true" : "false",
		DEF_TERM_CMD,
		DEF_SORT_REVERSE == 1 ? "true" : "false",
		DEF_PRIORITY_SORT_CHAR,
		DEF_PRIVATE_WS_SETTINGS == 1 ? "true" : "false",
		DEF_TIPS == 1 ? "true" : "false",
		DEF_LIST_DIRS_FIRST == 1 ? "true" : "false",
		DEF_CASE_SENS_LIST == 1 ? "true" : "false",
		DEF_CASE_SENS_DIRJUMP == 1 ? "true" : "false",
		DEF_CASE_SENS_PATH_COMP == 1 ? "true" : "false",
		DEF_CASE_SENS_SEARCH == 1 ? "true" : "false",
		DEF_SKIP_NON_ALNUM_PREFIX == 1 ? "true" : "false",
		DEF_PAGER == 1 ? "true" : "false",
		DEF_PAGER_VIEW == PAGER_AUTO ? "auto"
			: (DEF_PAGER_VIEW == PAGER_LONG ? "long" : "short"),
		DEF_MAX_NAME_LEN,
		DEF_TRUNC_NAMES == 1 ? "true" : "false"
		);

	fprintf(config_fp,
	";MaxHistory=%d\n\
;MaxDirhist=%d\n\
;MaxLog=%d\n\
;HistIgnore=%s\n\
;DirhistIgnore=\"\"\n\
;Icons=%s\n\
;IconPad=%d\n\
;Umask=077\n\
;DiskUsage=%s\n\n"

		"# Print the list of selected files. You can limit the number of printed \n\
# entries using the MaxPrintSelfiles option (-1 = no limit, 0 = auto (never\n\
# print more than half terminal height), or any custom value).\n\
;PrintSelfiles=%s\n\
;MaxPrintSelfiles=%d\n\n"

	    "# Clear the screen before listing files.\n\
;ClearScreen=%s\n\n"

	    "# If not specified, StartingPath defaults to the current working\n\
# directory. If set, it overrides RestoreLastPath.\n\
;StartingPath=\n\n"

	    "# If set to true, start Clifm in the last visited directory (and in the\n\
# last used workspace). This option is overriden by StartingPath (if set).\n\
;RestoreLastPath=%s\n\n"

	    "# If set to true, the 'r' command executes 'trash' instead of 'rm' to\n\
# prevent accidental deletions.\n\
;TrashAsRm=%s\n\n"

	    "# If set to true, do not ask for confirmation before trashing files.\n\
;TrashForce=%s\n\n"

	    "# Set readline editing mode: 0 for vi and 1 for emacs (default).\n\
;RlEditMode=%d\n\n",

		DEF_MAX_HIST,
		DEF_MAX_DIRHIST,
		DEF_MAX_LOG,
		DEF_HISTIGNORE,
		DEF_ICONS == 1 ? "true" : "false",
		DEF_ICONS_GAP,
		DEF_DISK_USAGE == 1 ? "true" : "false",
		DEF_PRINTSEL == 1 ? "true" : "false",
		DEF_MAX_PRINTSEL,
		DEF_CLEAR_SCREEN == 1 ? "true" : "false",
		DEF_RESTORE_LAST_PATH == 1 ? "true" : "false",
		DEF_TRASRM == 1 ? "true" : "false",
		DEF_TRASH_FORCE == 1 ? "true" : "false",
		DEF_RL_EDIT_MODE
		);

	fputs(

	    "# ALIASES\n\
#alias ls='ls --color=auto -A'\n\n"

	    "# PROMPT COMMANDS\n\
# Write below the commands you want to be executed before the prompt. E.g.:\n\
#promptcmd /usr/share/clifm/plugins/git_status.sh\n\
#promptcmd date | awk '{print $1\", \"$2,$3\", \"$4}'\n\n"

		"# AUTOCOMMANDS\n\
# Control Clifm settings on a per directory basis. For more information\n\
# consult the manpage.\n\
#autocmd /media/remotes/** lm=1,fc=0\n\
#autocmd @ws3 lv=1\n\
#autocmd ~/important !printf \"Keep your fingers outta here!\\n\" && read -n1\n\
#autocmd ~/Downloads !/usr/share/clifm/plugins/fzfnav.sh\n\n",
	    config_fp);

	fclose(config_fp);
	return FUNC_SUCCESS;
}

static int
create_def_color_scheme256(void)
{
	char cscheme_file[PATH_MAX + 1];
	snprintf(cscheme_file, sizeof(cscheme_file), "%s/%s.clifm", colors_dir,
		DEF_COLOR_SCHEME_256);

	/* If the file already exists, do nothing */
	struct stat attr;
	if (stat(cscheme_file, &attr) != -1)
		return FUNC_SUCCESS;

	/* Try to import it from data dir */
	if (import_color_scheme(DEF_COLOR_SCHEME_256) == FUNC_SUCCESS)
		return FUNC_SUCCESS;

	return FUNC_FAILURE;
}

static void
create_def_color_scheme(void)
{
	if (!colors_dir || !*colors_dir)
		return;

	if (term_caps.color >= 256)
		create_def_color_scheme256();

	char cscheme_file[PATH_MAX + 1];
	snprintf(cscheme_file, sizeof(cscheme_file), "%s/%s.clifm", colors_dir,
		DEF_COLOR_SCHEME);

	/* If the file already exists, do nothing */
	struct stat attr;
	if (stat(cscheme_file, &attr) != -1)
		return;

	/* Try to import it from data dir */
	if (import_color_scheme(DEF_COLOR_SCHEME) == FUNC_SUCCESS)
		return;

	/* If cannot be imported either, create it with default values */
	int fd;
	FILE *fp = open_fwrite(cscheme_file, &fd);
	if (!fp) {
		err('w', PRINT_PROMPT, "%s: Cannot create default color scheme "
			"file: '%s' : %s\n", PROGRAM_NAME, cscheme_file, strerror(errno));
		return;
	}

	fprintf(fp, "# Default color scheme for %s\n\n\
# FiletypeColors defines the color used for file types when listing files,\n\
# just as InterfaceColors defines colors for Clifm's interface and ExtColors\n\
# for file extensions. They all make use of the same format used by the\n\
# LS_COLORS environment variable. Thus, \"di=1;34\" means that (non-empty)\n\
# directories will be listed in bold blue.\n\
# Color codes are traditional ANSI escape sequences less the escape char and\n\
# the final 'm'. 8 bit, 256 colors, RGB, and hex (#rrggbb) colors are supported.\n\
# A detailed explanation of all these codes can be found in the manpage.\n\n"

			"FiletypeColors=\"%s\"\n\n"

			"InterfaceColors=\"%s\"\n\n"

			"# Same as FiletypeColors, but for file extensions. The format is always\n\
# *.EXT=COLOR (extensions are case insensitive)\n"
			"ExtColors=\"%s\"\n\n"

			"# Color shades used to colorize timestamps and file sizes. Consult the\n\
# manpage for more information\n"
			"DateShades=\"%s\"\n"
			"SizeShades=\"%s\"\n\n"

			"DirIconColor=\"00;33\"\n\n"
			"DividingLine=\"%s\"\n\n"

			"# If set to 'true', automatically print notifications at the left\n\
# of the prompt. If set to 'false', let the prompt string handle these notifications\n\
# itself via escape codes. See the manpage for more information.\n"
			"Notifications=\"%s\"\n\n"
			"Prompt=\"%s\"\n\n"

			"# An alternative prompt to warn the user about invalid command names\n"
			"EnableWarningPrompt=\"%s\"\n\n"
			"WarningPrompt=\"%s\"\n\n"
			"FzfTabOptions=\"%s\"\n\n",

		PROGRAM_NAME,
		term_caps.color >= 256 ? DEF_FILE_COLORS_256 : DEF_FILE_COLORS,
		term_caps.color >= 256 ? DEF_IFACE_COLORS_256 : DEF_IFACE_COLORS,
		term_caps.color >= 256 ? DEF_EXT_COLORS_256 : DEF_EXT_COLORS,
		term_caps.color >= 256 ? DEF_DATE_SHADES_256 : DEF_DATE_SHADES_8,
		term_caps.color >= 256 ? DEF_SIZE_SHADES_256 : DEF_SIZE_SHADES_8,
		DEF_DIV_LINE,
		DEF_PROMPT_NOTIF == 1 ? "true" : "false",
		DEFAULT_PROMPT,
		DEF_WARNING_PROMPT == 1 ? "true" : "false",
		DEF_WPROMPT_STR,
		DEF_FZFTAB_OPTIONS);

	fclose(fp);
}

static int
create_remotes_file(void)
{
	if (!remotes_file || !*remotes_file)
		return FUNC_FAILURE;

	struct stat attr;

	/* If the file already exists, do nothing */
	if (stat(remotes_file, &attr) == FUNC_SUCCESS)
		return FUNC_SUCCESS;

	/* Let's try to copy the file from DATADIR */
	if (import_from_data_dir("nets.clifm", remotes_file, 0) == FUNC_SUCCESS)
		return FUNC_SUCCESS;

	/* If not in DATADIR, let's create a minimal file here */
	int fd = 0;
	FILE *fp = open_fwrite(remotes_file, &fd);
	if (!fp) {
		err('e', PRINT_PROMPT, "%s: Cannot create remotes file "
			"'%s': %s\n", PROGRAM_NAME, remotes_file, strerror(errno));
		return FUNC_FAILURE;
	}

	fprintf(fp, "#####################################\n"
		"# Remotes management file for %s #\n"
		"#####################################\n\n"
		"# Blank and commented lines are omitted\n\n"
		"# Example:\n"
		"# A name for this remote. It will be used by the 'net' command\n"
		"# and will be available for tab completion\n"
		"# [work_smb]\n\n"
		"# Comment=My work samba server\n"
		"# Mountpoint=/home/user/.config/clifm/mounts/work_smb\n\n"
		"# Use %%m as a placeholder for Mountpoint\n"
		"# MountCmd=mount.cifs //WORK_IP/shared %%m -o OPTIONS\n"
		"# UnmountCmd=umount %%m\n\n"
		"# Automatically mount this remote at startup\n"
		"# AutoMount=true\n\n"
		"# Automatically unmount this remote at exit\n"
		"# AutoUnmount=true\n\n", PROGRAM_NAME);

	fclose(fp);
	return FUNC_SUCCESS;
}

static int
create_main_config_dir(void)
{
	/* Use mkdir(1) to let it handle parent directories. */
	char *tmp_cmd[] = {"mkdir", "-p", "--", config_dir, NULL};
	if (launch_execv(tmp_cmd, FOREGROUND, E_NOSTDERR) != FUNC_SUCCESS) {
		config_ok = 0;
		err('e', PRINT_PROMPT, _("%s: Cannot create configuration "
			"directory '%s': Bookmarks, commands logs, and "
			"command history are disabled. Program messages will not be "
			"persistent. Falling back to default settings.\n"),
			PROGRAM_NAME, config_dir);
		return FUNC_FAILURE;
	}

	return FUNC_SUCCESS;
}

static void
create_profile_file(void)
{
	int fd = 0;
	FILE *profile_fp = open_fwrite(profile_file, &fd);
	if (!profile_fp) {
		err('e', PRINT_PROMPT, "%s: Cannot create profile file "
			"'%s': %s\n", PROGRAM_NAME, profile_file, strerror(errno));
	} else {
		fprintf(profile_fp, "# This is %s's profile file\n#\n"
			"# Write here the commands you want to be executed at startup.\n"
			"# E.g.:\n#echo \"%s, the command line file manager\"; read -r\n#\n"
			"# Uncommented, non-empty lines are executed line by line. If you\n"
			"# want a multi-line command, just write a script for it:\n"
			"#sh /path/to/my/script.sh\n",
			PROGRAM_NAME_UPPERCASE, PROGRAM_NAME_UPPERCASE);
		fclose(profile_fp);
	}
}

static void
create_config_files(const int just_listing)
{
	struct stat attr;

	if (stat(config_dir, &attr) == -1
	&& create_main_config_dir() == FUNC_FAILURE) {
		config_ok = 0;
		return;
	}

	if (access(config_dir, W_OK) == -1) {
		config_ok = 0;
		err('e', PRINT_PROMPT, _("%s: '%s': Directory not writable. Bookmarks, "
			"commands logs, and commands history are disabled. Program messages "
			"will not be persistent. Falling back to default settings.\n"),
		    PROGRAM_NAME, config_dir);
		return;
	}

	if (stat(config_file, &attr) == -1)
		config_ok = create_main_config_file(config_file) == FUNC_SUCCESS ? 1 : 0;

	if (config_ok == 0)
		return;

	if (stat(profile_file, &attr) == -1)
		create_profile_file();

	if (stat(colors_dir, &attr) == -1
	&& xmkdir(colors_dir, S_IRWXU) == FUNC_FAILURE)
		err('w', PRINT_PROMPT, _("%s: Cannot create colors "
		"directory '%s': Falling back to the default color scheme\n"),
		PROGRAM_NAME, colors_dir);

	create_def_color_scheme();

	if (just_listing == 1)
		return;

	if (stat(tags_dir, &attr) == -1 && xmkdir(tags_dir, S_IRWXU) == FUNC_FAILURE)
		err('w', PRINT_PROMPT, _("%s: %s: Cannot create tags directory. "
			"Tag function disabled\n"),	PROGRAM_NAME, tags_dir);

	if (stat(plugins_dir, &attr) == -1
	&& xmkdir(plugins_dir, S_IRWXU) == FUNC_FAILURE)
		err('e', PRINT_PROMPT, _("%s: Cannot create plugins "
			"directory '%s': The actions function is disabled\n"),
			PROGRAM_NAME, plugins_dir);

	import_data_file("readline.clifm", "readline.clifm", 0);
	import_data_file("plugins/clifmimg", "clifmimg", 1);
	import_data_file("plugins/clifmrun", "clifmrun", 1);
	create_actions_file(actions_file);
	create_mime_file(mime_file, 0);
	create_preview_file();
	create_remotes_file();
}

/* Create a completely new mimelist file and store it in FILE. */
static int
create_mime_file_anew(char *file)
{
	int fd;
	FILE *fp = open_fwrite(file, &fd);
	if (!fp) {
		err('e', PRINT_PROMPT, "%s: Cannot create mimelist file "
			"'%s': %s\n", PROGRAM_NAME, file, strerror(errno));
		return FUNC_FAILURE;
	}

	fprintf(fp, "                  ###################################\n\
                  #   Configuration File for Lira   #\n\
                  #       Clifm's File Opener       #\n\
                  ###################################\n\
\n\
# Commented and blank lines are ignored.\n\
\n\
# The below settings cover the most common filetypes.\n\
# It is recommended to edit this file placing your prefered applications\n\
# at the beginning of the apps list to speed up the opening process.\n\
\n\
# The file is read top to bottom and left to right; the first existent\n\
# application found will be used.\n\
\n\
# Applications defined here are NOT desktop files, but commands (arguments\n\
# can be used as well). Bear in mind that these commands will be executed\n\
# directly without shell intervention, so that no shell goodies (like pipes,\n\
# conditions, loops, etc) are available. In case you need something more\n\
# complex than a single command (including shell capabilities) write your\n\
# own script and place the path to the script in place of the command.\n\
# For example: X:^text/.*:~/scripts/my_cool_script.sh\n\
\n\
# Use 'X' to specify a GUI environment and '!X' for non-GUI environments,\n\
# like the kernel builtin console or a remote SSH session.\n\
\n\
# Use 'N' to match filenames instead of MIME types.\n\
\n\
# Regular expressions are allowed for both file types and filenames.\n\
\n\
# Use the %%f placeholder to specify the position of the filename to be\n\
# opened in the command. Example:\n\
# 'mpv %%f --terminal=no' -> 'mpv FILE --terminal=no'\n\
#\n\
# The %%u placeholder is expanded to the file URI for the original file.\n\
#\n\
# If neither %%f nor %%u are specified, the filename will be appended to the\n\
# end of the command. E.g.: 'mpv --terminal=no' -> 'mpv --terminal=no FILE'\n\
#\n\
# The %%m placeholder is expanded to the file's MIME type.\n\
\n\
# To silence STDERR and/or STDOUT use !E and !O respectively (they can\n\
# be used together). Examples:\n\
# Silence STDERR only and run in the foreground:\n\
#    mpv %%f !E\n\
# Silence both (STDERR and STDOUT) and run in the background:\n\
#    mpv %%f !EO &\n\
# or\n\
#    mpv %%f !E !O &\n\
# For Ranger users, \"!EO\" is equivalent to \"flag f\" in rifle.conf\n\
\n\
# The '%%x' flag can be used as a shorthand for \"%%f !EO &\". Example:\n\
#    mpv %%x\n\
\n\
# Running the opening application in the background:\n\
# For GUI applications:\n\
#    APP %%x\n\
# For terminal applications:\n\
#    TERM -e APP %%x\n\
# Replace 'TERM' and 'APP' by the corresponding values. The -e option\n\
# might vary depending on the terminal emulator used (TERM).\n\
\n\
# Note on graphical applications: If the opening application is already running\n\
# the file might be opened in a tab, and Clifm will not wait for the file to be\n\
# closed (because the process already returned). To avoid this, instruct the\n\
# application to run a new instance: e.g.: geany -i, gedit -s, kate -n,\n\
# pluma --new-window, and so on.\n\
\n\
# Environment variables can be used as well. Example:\n\
# X:text/plain=$EDITOR %%f &;$VISUAL;nano;vi\n\
\n\
# Use Ranger's rifle (or whatever opener you prefer) to open all files\n\
#.*=rifle\n\
\n\
###########################\n\
#  File names/extensions  #\n\
###########################\n\
\n\
# Match a full filename\n\
#X:N:some_filename=cmd\n\
\n\
# Match all filenames starting with 'str'\n\
#X:N:^str.*=cmd\n\
\n\
# Match files with extension 'ext'\n\
#X:N:.*\\.ext$=cmd\n\
\n\
X:N:.*\\.djvu$=djview %%x;zathura %%x;xreader %%x;evince %%x;atril %%x\n\
X:N:.*\\.(fb2|epub)$=mupdf %%x;zathura %%x;xreader %%x;ebook-viewer %%x;FBReader %%x;foliate %%x\n\
X:N:.*\\.mobi$=mupdf %%x;ebook-viewer %%x;FBReader %%x;foliate %%x\n\
X:N:.*\\.(cbr|cbz|cb7|cbt|cba)$=mcomix %%x;xreader %%x;YACReader %%x;qcomicbook %%x;zathura %%x;foliate %%x\n\
X:N:.*\\.chm$=xchm %%x\n\
X:N:.*\\.abw$=abiword %%x;libreoffice %%x;soffice %%x;ooffice %%x\n\
X:N:.*\\.kra$=krita %%x\n\
X:N:.*\\.sla$=scribus %%x\n\
X:N:.*\\.ora$=mypaint %%x;krita %%x;gimp %%x;pinta %%x;scribus %%x;display %%x\n\
X:N:(.*\\.clifm$|clifmrc)=$EDITOR;$VISUAL;kak;micro;nvim;vim;vi;mg;emacs;nano;mili;leafpad;mousepad;featherpad;gedit;kate;pluma\n\
!X:N:(.*\\.clifm$|clifmrc)=$EDITOR;$VISUAL;kak;micro;nvim;vim;vi;mg;emacs;nano\n\
\n\n");

	fprintf(fp, "##################\n\
#   MIME types   #\n\
##################\n\
\n\
# Directories - only for the open-with (ow) command and the --open command\n\
# line switch\n\
# In graphical environments directories will be opened in a new window\n\
X:inode/directory=xterm -e clifm %%x;xterm -e vifm %%x;pcmanfm %%x;thunar %%x;xterm -e ncdu %%x\n\
!X:inode/directory=vifm;ranger;nnn;ncdu\n\
\n\
# Web content\n\
X:^text/html$=$BROWSER;surf %%x;vimprobable %%x;vimprobable2 %%x;qutebrowser %%x;dwb %%x;jumanji %%x;luakit %%x;uzbl %%x;uzbl-tabbed %%x;uzbl-browser %%x;uzbl-core %%x;iceweasel %%x;midori %%x;opera %%x;firefox %%x;seamonkey %%x;brave %%x;chromium-browser %%x;chromium %%x;google-chrome %%x;epiphany %%x;konqueror %%x;elinks;links2;links;lynx;w3m\n\
!X:^text/html$=$BROWSER;elinks;links2;links;lynx;w3m\n\
\n\
# Text\n\
X:^text/rtf$=libreoffice %%x;soffice %%x;ooffice %%x\n\
X:(^text/.*|application/(json|javascript)|inode/x-empty)=$EDITOR;$VISUAL;kak;micro;dte;nvim;vim;vi;mg;emacs;nano;mili;leafpad %%x;mousepad %%x;featherpad %%x;nedit %%x;kate %%x;gedit %%x;pluma %%x;io.elementary.code %%x;liri-text %%x;xed %%x;atom %%x;nota %%x;gobby %%x;kwrite %%x;xedit %%x\n\
!X:(^text/.*|application/(json|javascript)|inode/x-empty)=$EDITOR;$VISUAL;kak;micro;dte;nvim;vim;vi;mg;emacs;nano\n\
\n\
# Office documents\n\
^application/.*(open|office)document\\.spreadsheet.*=sc-im\n\
X:^application/(msword|vnd.ms-excel|vnd.ms-powerpoint|.*(open|office)document.*)=libreoffice %%x;soffice %%x;ooffice %%x\n\
\n\
# Archives\n\
# Note: 'ad' is Clifm's builtin archives utility (based on atool). Remove it if you\n\
# prefer another application.\n\
X:^application/(gzip|java-archive|vnd.(debian.binary-package|ms-cab-compressed|rar)|x-(7z-compressed|arj|bzip*|compress|iso9660-image|lzip|lzma|lzop|rpm|tar|xz)|zip|zstd)=ad;xarchiver %%x;lxqt-archiver %%x;ark %%x\n\
!X:^application/(gzip|java-archive|vnd.(debian.binary-package|ms-cab-compressed|rar)|x-(7z-compressed|arj|bzip*|compress|iso9660-image|lzip|lzma|lzop|rpm|tar|xz)|zip|zstd)=ad\n\
\n\
# PDF\n\
X:.*/pdf$=mupdf %%x;sioyek %%x;llpp %%x;lpdf %%x;zathura %%x;mupdf-x11 %%x;apvlv %%x;xpdf %%x;xreader %%x;evince %%x;atril %%x;okular %%x;epdfview %%x;qpdfview %%x\n\
\n\
# Images\n\
X:^image/gif$=animate %%x;pqiv %%x;sxiv -a %%x;nsxiv -a %%x;feh %%x\n\
X:^image/svg$=display;inkscape %%x\n\
X:^image/x-xcf$=gimp %%x\n\
X:^image/.*=imv %%x;nsxiv %%x;sxiv %%x;pqiv %%x;qview %%x;qimgv %%x;viewnior %%x;oculante %%x;mirage %%x;ristretto %%x;xviewer %%x;nomacs %%x;geeqie %%x;gpicview %%x;gthumb %%x;gwenview %%x;shotwell %%x;loupe %%x;eog %%x;eom %%x;mcomix %%x;feh %%x;display %%x;gimp %%x;krita %%x\n\
!X:^image/.*=fim;img2txt;cacaview;fbi;fbv\n\
\n\
# Video and audio\n\
X:^video/.*=ffplay %%x;mplayer %%x;mplayer2 %%x;mpv %%x;vlc %%x;gmplayer %%x;smplayer %%x;celluloid %%x;qmplayer2 %%x;haruna %%x;totem %%x\n\
X:^audio/.*=mpv %%x;gmplayer %%x;smplayer %%x;vlc %%x;totem %%x;ffplay %%x;mplayer;mplayer2\n\
!X:^audio/.*=ffplay -nodisp -autoexit %%f !EO;mplayer %%f !EO;mpv --no-terminal\n\
\n\
# Fonts\n\
X:^font/.*|^application/(font.*|.*opentype)=fontforge;fontpreview\n\
\n\
# Torrent:\n\
X:application/x-bittorrent=rtorrent;transimission-gtk %%x;transmission-qt %%x;deluge-gtk %%x;ktorrent %%x\n\
\n\
# Fallback to an external opener as last resource\n\
.*=handlr open;mimeopen -n;rifle;mimeo;xdg-open;open;\n");

	fclose(fp);
	return FUNC_SUCCESS;
}

int
create_mime_file(char *file, const int new_prof)
{
	/* This variable was used to print a notice message when running for the
	 * first time. Too much information: removed. */
	UNUSED(new_prof);

	if (!file || !*file)
		return FUNC_FAILURE;

	struct stat attr;
	if (stat(file, &attr) == FUNC_SUCCESS)
		return FUNC_SUCCESS;

	int ret = import_from_data_dir("mimelist.clifm", file, 0);
	if (ret != FUNC_SUCCESS)
		ret = create_mime_file_anew(file);

	return ret;
}

int
create_bm_file(void)
{
	if (!bm_file)
		return FUNC_FAILURE;

	struct stat attr;
	if (stat(bm_file, &attr) != -1)
		return FUNC_SUCCESS;

	int fd = 0;
	FILE *fp = open_fwrite(bm_file, &fd);
	if (!fp) {
		err('e', PRINT_PROMPT, "%s: Cannot create bookmarks file "
			"'%s': %s\n", PROGRAM_NAME, bm_file, strerror(errno));
		return FUNC_FAILURE;
	}

	fprintf(fp, "### This is the bookmarks file for %s ###\n\n"
		"# Empty and commented lines are ignored.\n"
		"# Make your changes, save, and exit.\n"
		"# To remove a bookmark, delete the corresponding line, save, and exit\n"
		"# Changes are applied automatically at exit (to cancel just quit the editor).\n\n"
		"# The bookmarks syntax is: name:path\n"
		"# Example:\n"
		"clifm:%s\n",
		PROGRAM_NAME, config_dir ? config_dir : "/path/to/file");

	fclose(fp);
	return FUNC_SUCCESS;
}

static char *
get_line_value(char *line)
{
	if (!line || *line < ' ') /* Skip non-printable chars */
		return (char *)NULL;

	return remove_quotes(line);
}

/* Read time format from LINE and store the appropriate value in STR.
 * PTIME specifies whether we're dealing with PTimeStyle or just TimeStyle
 * options (the former does not support relative times). */
void
set_time_style(char *line, char **str, const int ptime)
{
	if (!line || !*line)
		return;

	char *tmp = get_line_value(line);
	if (!tmp)
		return;

	free(*str);
	*str = (char *)NULL;

	if (*tmp == 'r' && strcmp(tmp + 1, "elative") == 0) {
		if (ptime == 0) conf.relative_time = 1;
	} else if (*tmp == 'd' && strcmp(tmp + 1, "efault") == 0) {
		return;
	} else if (*tmp == 'i' && strcmp(tmp + 1, "so") == 0) {
		*str = savestring(ISO_TIME, sizeof(ISO_TIME) - 1);
	} else if (*tmp == 'f' && strcmp(tmp + 1, "ull-iso") == 0) {
		*str = savestring(FULL_ISO_TIME, sizeof(FULL_ISO_TIME) - 1);
	} else if (ptime == 1
	&& *tmp == 'f' && strcmp(tmp + 1, "ull-iso-nano") == 0) {
		*str = savestring(FULL_ISO_NANO_TIME, sizeof(FULL_ISO_NANO_TIME) - 1);
	} else if (*tmp == 'l' && strcmp(tmp + 1, "ong-iso") == 0) {
		*str = savestring(LONG_ISO_TIME, sizeof(LONG_ISO_TIME) - 1);
	} else if (*tmp == '+') {
		if (*(++tmp)) {
			/* We don't support FORMAT1<newline>FORMAT2, like ls(1) does. */
			char *n = strchr(tmp, '\n');
			if (n) *n = '\0';
			*str = savestring(tmp, strlen(tmp));
		}
	} else {
		char *n = strchr(tmp, '\n');
		if (n) *n = '\0';
		*str = savestring(tmp, strlen(tmp));
	}
}

static void
set_time_style_env(void)
{
	if (xargs.secure_env == 1 || xargs.secure_env_full == 1)
		return;

	char *p = getenv("CLIFM_TIME_STYLE");
	if (!p || !*p)
		p = getenv("TIME_STYLE");
	if (!p || !*p)
		return;

	set_time_style(p, &conf.time_str, 0);
}

static void
set_ptime_style_env(void)
{
	if (xargs.secure_env == 1 || xargs.secure_env_full == 1)
		return;

	char *p = getenv("PTIME_STYLE");
	if (!p || !*p)
		return;

	set_time_style(p, &conf.ptime_str, 1);
}

#ifndef CLIFM_SUCKLESS
static int
set_fzf_preview_value(const char *line, int *var)
{
#ifdef _NO_LIRA
	UNUSED(line);
	*var = 0;
#endif /* _NO_LIRA */

	if (!line || !*line)
		return (-1);

	if (*line == 't' && strncmp(line, "true\n", 5) == 0) {
		*var = 1;
	} else if (*line == 'h' && strncmp(line, "hidden\n", 7) == 0) {
		*var = 2;
	} else {
		if (*line == 'f' && strncmp(line, "false\n", 6) == 0)
			*var = 0;
	}

	return FUNC_SUCCESS;
}

static void
set_pager_value(char *line, int *var, const size_t buflen)
{
	char *p = line;
	if (!p || *p < '0')
		return;

	if (*p >= '0' && *p <= '9') {
		const size_t l = strnlen(p, buflen);
		if (l > 0 && p[l - 1] == '\n')
			p[l - 1] = '\0';

		int n = atoi(p);
		if (n == INT_MIN)
			return;

		*var = n;
		return;
	}

	if (*p == 't' && strncmp(p, "true\n", 5) == 0) {
		*var = 1;
	} else {
		if (*p == 'f' && strncmp(p, "false\n", 6) == 0)
			*var = 0;
	}
}

static void
set_pager_view_value(char *line)
{
	if (!line || !*line)
		return;

	char *p = remove_quotes(line);
	if (!p)
		return;

	if (*p == 'a' && strcmp(p, "auto") == 0)
		conf.pager_view = PAGER_AUTO;
	else if (*p == 'l' && strcmp(p, "long") == 0)
		conf.pager_view = PAGER_LONG;
	else if (*p == 's' && strcmp(p, "short") == 0)
		conf.pager_view = PAGER_SHORT;
}

/* Get boolean value from LINE and set VAR accordingly. */
static void
set_config_bool_value(char *line, int *var)
{
	char *p = line;
	if (!p || !*p || *p < 'f')
		return;

	if (*p == 't' && strncmp(p, "true\n", 5) == 0) {
		*var = 1;
	} else {
		if (*p == 'f' && strncmp(p, "false\n", 6) == 0)
			*var = 0;
	}
}

/* Get an integer (between MIN and MAX inclusively) from LINE and store
 * it in VAR. In case of error or out of range integer, VAR is left
 * unchanged. */
static void
set_config_int_value(char *line, int *var, const int min, const int max)
{
	if (!line || !*line)
		return;

	int num = 0;
	const int ret = sscanf(line, "%d\n", &num);
	if (ret > 0 && num >= min && num <= max)
		*var = num;
}

static void
set_colorscheme(char *line)
{
	if (!line || *line < ' ')
		return;

	char *p = remove_quotes(line);
	if (!p)
		return;

	free(conf.usr_cscheme);
	conf.usr_cscheme = savestring(p, strlen(p));
}

void
set_div_line(char *line)
{
	if (!line || *line < ' ') {
		*div_line = *DEF_DIV_LINE;
		return;
	}

	char *tmp = remove_quotes(line);
	if (!tmp) {
		*div_line = '\0';
		return;
	}

	xstrsncpy(div_line, tmp, sizeof(div_line));
}

static int
set_files_filter(const char *line)
{
	char *p = strchr(line, '=');
	if (!p || !*(++p))
		return (-1);

	char *q = remove_quotes(p);
	if (!q)
		return (-1);

	size_t l = strlen(q);
	if (l > 0 && q[l - 1] == '\n') {
		q[l - 1] = '\0';
		l--;
	}

	if (*q == '!') {
		filter.rev = 1;
		q++;
		l--;
	} else {
		filter.rev = 0;
	}

	set_filter_type(*q);
	free(filter.str);
	filter.str = savestring(q, l);

	return FUNC_SUCCESS;
}

static void
set_listing_mode(char *line)
{
	if (!line || !*line)
		return;

	line[1] = '\0';

	const int n = atoi(line);
	if (n == INT_MIN)
		goto END;

	switch (n) {
	case VERTLIST: conf.listing_mode = VERTLIST; return;
	case HORLIST: conf.listing_mode = HORLIST; return;
	default: break;
	}

END:
	conf.listing_mode = DEF_LISTING_MODE;
}

static void
set_search_strategy(const char *line)
{
	if (!line || *line < '0' || *line > '2')
		return;

	switch (*line) {
	case '0': conf.search_strategy = GLOB_ONLY; break;
	case '1': conf.search_strategy = REGEX_ONLY; break;
	case '2': conf.search_strategy = GLOB_REGEX; break;
	default: break;
	}
}

static void
free_workspaces_names(void)
{
	if (workspaces) {
		int i = MAX_WS;
		while (--i >= 0) {
			free(workspaces[i].name);
			workspaces[i].name = (char *)NULL;
		}
	}
}

/* Get workspaces names from the WorkspacesNames line in the configuration
 * file and store them in the workspaces array. */
void
set_workspace_names(char *line)
{
	char *e = (char *)NULL;
	char *t = remove_quotes(line);
	if (!t || !*t)
		return;

	char *p = (char *)NULL;
	while (*t) {
		p = strchr(t, ',');
		if (p)
			*p = '\0';
		else
			p = t;

		e = strchr(t, '=');
		if (!e || !*(e + 1))
			goto CONT;

		*e = '\0';
		if (!is_number(t))
			goto CONT;

		const int a = atoi(t);
		if (a <= 0 || a > MAX_WS)
			goto CONT;

		free(workspaces[a - 1].name);
		workspaces[a - 1].name = savestring(e + 1, strlen(e + 1));

CONT:
		t = p + 1;
	}
}

#ifndef _NO_SUGGESTIONS
static void
set_sug_strat(char *line)
{
	char *tmp = remove_quotes(line);
	if (!tmp)
		return;

	const size_t len = strlen(tmp);
	if (len > SUG_STRATS)
		tmp[SUG_STRATS] = '\0';

	free(conf.suggestion_strategy);
	conf.suggestion_strategy = savestring(tmp, strlen(tmp));
}
#endif /* _NO_SUGGESTIONS */

#ifndef _NO_FZF
static void
set_tabcomp_mode(char *line)
{
	if (!line || !*line)
		return;

	char *tmp = remove_quotes(line);
	if (!tmp)
		return;

	if (strcmp(tmp, "standard") == 0) {
		fzftab = 0; tabmode = STD_TAB;
	} else if (strcmp(tmp, "fzf") == 0) {
		fzftab = 1; tabmode = FZF_TAB;
	} else if (strcmp(tmp, "fnf") == 0) {
		fzftab = 1; tabmode = FNF_TAB;
	} else if (strcmp(tmp, "smenu") == 0) {
		fzftab = 1; tabmode = SMENU_TAB;
	}
}
#endif /* !_NO_FZF */

static void
set_starting_path(char *line)
{
	char *tmp = get_line_value(line);
	if (!tmp)
		return;

	/* If starting path is not NULL, and exists, and is a directory, and the
	 * user has appropriate permissions, set path to starting path. If any of
	 * these conditions is false, path will be set to default, that is, CWD */
	if (xchdir(tmp, SET_TITLE) == 0) {
		if (cur_ws < 0)
			cur_ws = 0;
		free(workspaces[cur_ws].path);
		workspaces[cur_ws].path = savestring(tmp, strlen(tmp));
		return;
	}

	err('w', PRINT_PROMPT, _("%s: chdir: '%s': %s. Using the "
		"current working directory as starting path\n"),
		PROGRAM_NAME, tmp, strerror(errno));
}

static void
set_quoting_style(char *str)
{
	conf.quoting_style = DEF_QUOTING_STYLE;
	if (!str || !*str)
		return;

	char *val = get_line_value(str);
	if (!val)
		return;

	if (*val == 'b' && strcmp(val + 1, "ackslash") == 0)
		conf.quoting_style = QUOTING_STYLE_BACKSLASH;
	else if (*val == 's' && strcmp(val + 1, "ingle") == 0)
		conf.quoting_style = QUOTING_STYLE_SINGLE_QUOTES;
	else if(*val == 'd' && strcmp(val + 1, "ouble") == 0)
		conf.quoting_style = QUOTING_STYLE_DOUBLE_QUOTES;
	else
		return;
}

static void
set_histignore_pattern(char *str)
{
	if (!str || !*str)
		return;

	char *pattern = get_line_value(str);
	if (!pattern) {
		conf.histignore_regex = savestring("", 0);
		return;
	}

	if (conf.histignore_regex) {
		regfree(&regex_hist);
		free(conf.histignore_regex);
		conf.histignore_regex = (char *)NULL;
	}

	int ret = regcomp(&regex_hist, pattern, REG_NOSUB | REG_EXTENDED);
	if (ret != FUNC_SUCCESS) {
		xregerror("histignore", pattern, ret, regex_hist, 1);
		regfree(&regex_hist);
		return;
	}

	conf.histignore_regex = savestring(pattern, strlen(pattern));
}

static void
set_dirhistignore_pattern(char *str)
{
	if (!str || !*str)
		return;

	char *pattern = get_line_value(str);
	if (!pattern) {
		conf.dirhistignore_regex = savestring("", 0);
		return;
	}

	if (conf.dirhistignore_regex) {
		regfree(&regex_dirhist);
		free(conf.dirhistignore_regex);
		conf.dirhistignore_regex = (char *)NULL;
	}

	int ret = regcomp(&regex_dirhist, pattern, REG_NOSUB | REG_EXTENDED);
	if (ret != FUNC_SUCCESS) {
		xregerror("dirhistignore", pattern, ret, regex_dirhist, 1);
		regfree(&regex_dirhist);
		return;
	}

	conf.dirhistignore_regex = savestring(pattern, strlen(pattern));
}

static void
set_sort_name(char *line)
{
	const char *name = get_line_value(line);
	if (!name || !*name)
		return;

	size_t i;
	for (i = 0; i <= SORT_TYPES; i++) {
		if (*name == *sort_methods[i].name
		&& strcmp(name, sort_methods[i].name) == 0) {
			conf.sort = sort_methods[i].num;
			return;
		}
	}
}

static void
set_clear_screen(char *line)
{
	if (*line == 'i' && strncmp(line, "internal\n", 9) == 0)
		conf.clear_screen = CLEAR_INTERNAL_CMD_ONLY;
	else
		set_config_bool_value(line, &conf.clear_screen);
}

static void
set_link_creation_mode(const char *val)
{
	if (!val || !*val) {
		conf.link_creat_mode = DEF_LINK_CREATION_MODE;
		return;
	}

	if (*val == 'a' && strncmp(val, "absolute\n", 9) == 0)
		conf.link_creat_mode = LNK_CREAT_ABS;
	else if (*val == 'r' && strncmp(val, "relative\n", 9) == 0)
		conf.link_creat_mode = LNK_CREAT_REL;
	else
		conf.link_creat_mode = DEF_LINK_CREATION_MODE;
}

static void
set_autocmd_msg_value(const char *val)
{
	if (!val || !*val)
		return;

	if (*val == 'n' && strncmp(val, "none\n", 5) == 0)
		conf.autocmd_msg = AUTOCMD_MSG_NONE;
	else if (*val == 'm' && strncmp(val, "mini\n", 5) == 0)
		conf.autocmd_msg = AUTOCMD_MSG_MINI;
	else if (*val == 's' && strncmp(val, "short\n", 6) == 0)
		conf.autocmd_msg = AUTOCMD_MSG_SHORT;
	else if (*val == 'l' && strncmp(val, "long\n", 5) == 0)
		conf.autocmd_msg = AUTOCMD_MSG_LONG;
	else if (*val == 'f' && strncmp(val, "full\n", 5) == 0)
		conf.autocmd_msg = AUTOCMD_MSG_FULL;
	else if (*val == 'p' && strncmp(val, "prompt\n", 7) == 0)
		conf.autocmd_msg = AUTOCMD_MSG_PROMPT;
	else
		conf.autocmd_msg = DEF_AUTOCMD_MSG;
}

static char *
get_term_env(const char *cmd)
{
	if (!cmd || !*cmd)
		return (char *)NULL;

	char *s = strchr(cmd, ' ');
	if (s)
		*s = '\0';

	char *env = getenv(cmd);

	if (s)
		*s = ' ';

	char *buf = (char *)NULL;
	if (env && *env) {
		const size_t len = strlen(env) + (s ? strlen(s) : 0) + 1;
		buf = xnmalloc(len, sizeof(char));
		snprintf(buf, len, "%s%s", env, s ? s : "");
	}

	return buf;
}

static void
set_term_cmd(char *cmd)
{
	if (!cmd || !*cmd)
		return;

	char *tmp = remove_quotes(cmd);
	if (!tmp)
		return;

	char *val = tmp;
	char *buf = (char *)NULL;

	if (*tmp == '$' && tmp[1] && xargs.secure_env != 1
	&& xargs.secure_env_full != 1) {
		buf = get_term_env(tmp + 1);
		if (buf)
			val = buf;
	}

	free(conf.term);
	conf.term = savestring(val, strlen(val));

	free(buf);
}

static void
set_default_answers_to_default(void)
{
	conf.default_answer.remove = DEF_ANSWER_REMOVE;
	conf.default_answer.trash = DEF_ANSWER_TRASH;
	conf.default_answer.bulk_rename = DEF_ANSWER_BULK_RENAME;
	conf.default_answer.overwrite = DEF_ANSWER_OVERWRITE;
	conf.default_answer.default_ = DEF_ANSWER_DEFAULT;
	conf.default_answer.default_all = DEF_ANSWER_DEFAULT_ALL;
}

static void
set_default_answer(const char *str)
{
	if (!str || !*str || str[1] != ':'
	|| (str[2] != 'y' && str[2] != 'n' && str[2] != 'u'))
		return;

	switch (*str) {
	case 'd': conf.default_answer.default_ = str[2]; break;
	case 'D': conf.default_answer.default_all = str[2]; break;
	case 'o': conf.default_answer.overwrite = str[2]; break;
	case 'r': conf.default_answer.remove = str[2]; break;
	case 'R': conf.default_answer.bulk_rename = str[2]; break;
	case 't': conf.default_answer.trash = str[2]; break;
	default: break;
	}
}

static void
set_default_answers(char *val)
{
	if (!val || !*val)
		return;

	char *v = remove_quotes(val);
	if (!v || !*v)
		return;

	char *str = strtok(v, ",");
	if (!str || !*str)
		return;

	set_default_answer(str);

	while ((str = strtok(NULL, ",")) != NULL)
		set_default_answer(str);
}

static void
set_rl_edit_mode(const char *val)
{
	if (val && ((*val == '0' && val[1] == '\n')
	|| (*val == 'v' && val[1] == 'i' && val[2] == '\n')) )
		rl_vi_editing_mode(1, 0);
	else
		rl_emacs_editing_mode(1, 0);
}

static void
set_preview_max_size(char *val)
{
	char *tmp;
	if (!val || !*val || !(tmp = remove_quotes(val)))
		return;

	const size_t l = strlen(tmp);
	if (l > 1 && tmp[l - 1] == '\n')
		tmp[l - 1] = '\0';

	char *u = tmp;
	while (*u && IS_DIGIT(*u))
		u++;

	char unit = 'B'; /* If no unit is specified, we fallback to bytes. */
	if (*u && u != tmp) {
		unit = (char)TOUPPER(*u);
		*u = '\0';
	}

	const long n = strtol(tmp, NULL, 10);
	if (n < 0 || n > INT_MAX)
		return;

	/* Transform the given value to KiB. */
	switch (unit) {
	case 'B': conf.preview_max_size = n <= 1024 ? 1 : (int)n / 1024; break;
	case 'K': conf.preview_max_size = (int)n; break;
	case 'M': conf.preview_max_size = (int)n * 1024; break;
	case 'G': conf.preview_max_size = (int)n * 1048576; break;
	case 'T': conf.preview_max_size = (int)n * 1073741824; break;
	default:
		err('w', PRINT_PROMPT, _("PreviewMaxSize: '%c': Invalid unit.\n"),
			unit);
		return;
	}

	if (conf.preview_max_size < 0) {
		err('w', PRINT_PROMPT, _("PreviewMaxSize: Value too large "
			"(max %dGiB).\n"), INT_MAX / 1048576);
		conf.preview_max_size = INT_MAX; /* Max supported size (in KiB). */
	}
}

static void
set_show_hidden_files(const char *val)
{
	if (!val || !*val)
		return;

	if (*val == 't' && strncmp(val, "true\n", 5) == 0) {
		conf.show_hidden = HIDDEN_TRUE;
	} else if (*val == 'f') {
		if (strncmp(val, "false\n", 6) == 0) {
			conf.show_hidden = HIDDEN_FALSE;
		} else {
			if (strncmp(val, "first\n", 6) == 0)
				conf.show_hidden = HIDDEN_FIRST;
		}
	} else {
		if (*val == 'l' && strncmp(val, "last\n", 5) == 0)
			conf.show_hidden = HIDDEN_LAST;
	}
}

static void
set_priority_sort_char(char *val)
{
	char *v;
	if (!val || !*val || !(v = remove_quotes(val)))
		return;

	const size_t l = strlen(v);
	if (l > 1 && v[l - 1] == '\n')
		v[l - 1] = '\0';

	char *buf = xnmalloc(l + 1, sizeof(char));
	char *ptr = buf;
	while (*v) {
		if (*v >= ' ' && *v <= '~') { /* Same as isprint(c) */
			*ptr = *v;
			ptr++;
		}
		v++;
	}

	*ptr = '\0';

	if (buf && *buf)
		conf.priority_sort_char = buf;
	else
		free(buf);
}

static void
set_desktop_notis(char *val)
{
	char *v;
	if (!val || !*val || !(v = remove_quotes(val)))
		return;

	const size_t l = strlen(v);
	if (l > 1 && v[l - 1] == '\n')
		v[l - 1] = '\0';

	if (*v == 'k' && strcmp(v, "kitty") == 0) {
		conf.desktop_notifications = DESKTOP_NOTIF_KITTY;
	} else if ((*v == 's' && strcmp(v, "system") == 0)
	|| (*v == 't' && strcmp(v, "true") == 0)) {
		conf.desktop_notifications = DESKTOP_NOTIF_SYSTEM;
	} else {
		if (*v == 'f' && strcmp(v, "false") == 0)
			conf.desktop_notifications =  DESKTOP_NOTIF_NONE;
	}
}

/* Read the main configuration file and set options accordingly */
static void
read_config(void)
{
	int fd;
	FILE *config_fp = open_fread(config_file, &fd);
	if (!config_fp) {
		err('e', PRINT_PROMPT, _("%s: '%s': %s. Using default "
			"values.\n"), PROGRAM_NAME, config_file, strerror(errno));
		return;
	}

	free_workspaces_names();

	if (xargs.rl_vi_mode == 1)
		rl_vi_editing_mode(1, 0);

	int default_answers_set = 0;
	int ret = -1;
	conf.max_name_len = DEF_MAX_NAME_LEN;
	*div_line = *DEF_DIV_LINE;
	/* The longest possible line in the config file is StartingPath="PATH" */
	char line[PATH_MAX + 16]; *line = '\0';

	if (xargs.prop_fields_str != 1)
		*prop_fields_str = '\0';

	while (fgets(line, (int)sizeof(line), config_fp)) {
		if (*line < 'A' || *line > 'z')
			continue;

		if (xargs.apparent_size == UNSET && *line == 'A'
		&& strncmp(line, "ApparentSize=", 13) == 0) {
			set_config_bool_value(line + 13, &conf.apparent_size);
		}

		else if (*line == 'a' && strncmp(line, "autocmd ", 8) == 0) {
			parse_autocmd_line(line + 8, sizeof(line) - 8);
		}

		else if (xargs.autocd == UNSET && *line == 'A'
		&& strncmp(line, "Autocd=", 7) == 0) {
			set_config_bool_value(line + 7, &conf.autocd);
		}

		else if (xargs.autols == UNSET && *line == 'A'
		&& strncmp(line, "AutoLs=", 7) == 0) {
			set_config_bool_value(line + 7, &conf.autols);
		}

		else if (xargs.auto_open == UNSET && *line == 'A'
		&& strncmp(line, "AutoOpen=", 9) == 0) {
			set_config_bool_value(line + 9, &conf.auto_open);
		}

#ifndef _NO_SUGGESTIONS
		else if (xargs.suggestions == UNSET && *line == 'A'
		&& strncmp(line, "AutoSuggestions=", 16) == 0) {
			set_config_bool_value(line + 16, &conf.suggestions);
		}
#endif /* !_NO_SUGGESTIONS */

		else if (xargs.case_sens_dirjump == UNSET && *line == 'C'
		&& strncmp(line, "CaseSensitiveDirJump=", 21) == 0) {
			set_config_bool_value(line + 21, &conf.case_sens_dirjump);
		}

		else if (*line == 'C'
		&& strncmp(line, "CaseSensitiveSearch=", 20) == 0) {
			set_config_bool_value(line + 20, &conf.case_sens_search);
		}

		else if (xargs.case_sens_list == UNSET && *line == 'C'
		&& strncmp(line, "CaseSensitiveList=", 18) == 0) {
			set_config_bool_value(line + 18, &conf.case_sens_list);
		}

		else if (xargs.case_sens_path_comp == UNSET && *line == 'C'
		&& strncmp(line, "CaseSensitivePathComp=", 22) == 0) {
			set_config_bool_value(line + 22, &conf.case_sens_path_comp);
		}

		else if (xargs.cd_on_quit == UNSET && *line == 'C'
		&& strncmp(line, "CdOnQuit=", 9) == 0) {
			set_config_bool_value(line + 9, &conf.cd_on_quit);
		}

		else if (xargs.classify == UNSET && *line == 'C'
		&& strncmp(line, "Classify=", 9) == 0) {
			set_config_bool_value(line + 9, &conf.classify);
		}

		else if (xargs.clear_screen == UNSET && *line == 'C'
		&& strncmp(line, "ClearScreen=", 12) == 0) {
			set_clear_screen(line + 12);
		}

		else if (xargs.color_lnk_as_target == UNSET && *line == 'C'
		&& strncmp(line, "ColorLinksAsTarget=", 19) == 0) {
			set_config_bool_value(line + 19, &conf.color_lnk_as_target);
		}

		else if (!conf.usr_cscheme && *line == 'C'
		&& strncmp(line, "ColorScheme=", 12) == 0) {
			set_colorscheme(line + 12);
		}

		else if (*line == 'c' && strncmp(line, "cpCmd=", 6) == 0) {
			set_config_int_value(line + 6, &conf.cp_cmd,
				0, CP_CMD_AVAILABLE - 1);
		}

		else if (*line == 'D' && strncmp(line, "DefaultAnswer=", 14) == 0) {
			default_answers_set = 1;
			set_default_answers(line + 14);
		}

		else if (xargs.desktop_notifications == UNSET && *line == 'D'
		&& strncmp(line, "DesktopNotifications=", 21) == 0) {
			set_desktop_notis(line + 21);
		}

		else if (xargs.dirhist_map == UNSET && *line == 'D'
		&& strncmp(line, "DirhistMap=", 11) == 0) {
			set_config_bool_value(line + 11, &conf.dirhist_map);
		}

		else if (*line == 'D' && strncmp(line, "DirhistIgnore=", 14) == 0) {
			set_dirhistignore_pattern(line + 14);
		}

		else if (xargs.disk_usage == UNSET && *line == 'D'
		&& strncmp(line, "DiskUsage=", 10) == 0) {
			set_config_bool_value(line + 10, &conf.disk_usage);
		}

		else if (*line == 'D' && strncmp(line, "DividingLine=", 13) == 0) {
			set_div_line(line + 13);
		}

		else if (xargs.ext_cmd_ok == UNSET && *line == 'E'
		&& strncmp(line, "ExternalCommands=", 17) == 0) {
			set_config_bool_value(line + 17, &conf.ext_cmd_ok);
		}

		else if (xargs.files_counter == UNSET && *line == 'F'
		&& strncmp(line, "FilesCounter=", 13) == 0) {
			set_config_bool_value(line + 13, &conf.files_counter);
		}

		else if (!filter.str && *line == 'F'
		&& strncmp(line, "Filter=", 7) == 0) {
			if (set_files_filter(line) == -1)
				continue;
		}

		else if (xargs.full_dir_size == UNSET && *line == 'F'
		&& strncmp(line, "FullDirSize=", 12) == 0) {
			set_config_bool_value(line + 12, &conf.full_dir_size);
		}

		else if (xargs.fuzzy_match_algo == UNSET && *line == 'F'
		&& strncmp(line, "FuzzyAlgorithm=", 15) == 0) {
			set_config_int_value(line + 15, &conf.fuzzy_match_algo,
				1, FUZZY_ALGO_MAX);
		}

		else if (xargs.fuzzy_match == UNSET && *line == 'F'
		&& strncmp(line, "FuzzyMatching=", 14) == 0) {
			set_config_bool_value(line + 14, &conf.fuzzy_match);
		}

		else if (xargs.fzf_preview == UNSET && *line == 'F'
		&& strncmp(line, "FzfPreview=", 11) == 0) {
			if (set_fzf_preview_value(line + 11, &conf.fzf_preview) == -1)
				continue;
		}

		else if (*line == 'H' && strncmp(line, "HistIgnore=", 11) == 0) {
			set_histignore_pattern(line + 11);
		}

#ifndef _NO_ICONS
		else if (xargs.icons == UNSET && *line == 'I'
		&& strncmp(line, "Icons=", 6) == 0) {
			set_config_bool_value(line + 6, &conf.icons);
		}

		else if (*line == 'I' && strncmp(line, "IconsGap=", 9) == 0) {
			set_config_int_value(line + 9, &conf.icons_gap, 0, 2);
		}
#endif /* !_NO_ICONS */

		else if (*line == 'I' && strncmp(line, "InformAutocmd=", 14) == 0) {
			set_autocmd_msg_value(line + 14);
		}

		else if (xargs.light_mode == UNSET && *line == 'L'
		&& strncmp(line, "LightMode=", 10) == 0) {
			set_config_bool_value(line + 10, &conf.light_mode);
		}

		else if (*line == 'L' && strncmp(line, "LinkCreationMode=", 17) == 0) {
			set_link_creation_mode(line + 17);
		}

		else if (xargs.list_dirs_first == UNSET && *line == 'L'
		&& strncmp(line, "ListDirsFirst=", 14) == 0) {
			set_config_bool_value(line + 14, &conf.list_dirs_first);
		}

		else if (xargs.horizontal_list == UNSET && *line == 'L'
		&& strncmp(line, "ListingMode=", 12) == 0) {
			set_listing_mode(line + 12);
		}

		else if (xargs.long_view == UNSET && *line == 'L'
		&& strncmp(line, "LongViewMode=", 13) == 0) {
			set_config_bool_value(line + 13, &conf.long_view);
		}

		else if (*line == 'L' && strncmp(line, "LogMsgs=", 8) == 0) {
			set_config_bool_value(line + 8, &conf.log_msgs);
		}

		else if (*line == 'L' && strncmp(line, "LogCmds=", 8) == 0) {
			set_config_bool_value(line + 8, &conf.log_cmds);
		}

		else if (xargs.max_dirhist == UNSET && *line == 'M'
		&& strncmp(line, "MaxDirhist=", 11) == 0) {
			set_config_int_value(line + 11, &conf.max_dirhist, 0, INT_MAX);
		}

		else if (*line == 'M' && strncmp(line, "MaxFilenameLen=", 15) == 0) {
			if (line[15] == '\n' && !line[16]) /* Empty == -1 == UNSET */
				conf.max_name_len = UNSET;
			else
				set_config_int_value(line + 15, &conf.max_name_len,
					-1, NAME_BUF_SIZE);
		}

		else if (*line == 'M' && strncmp(line, "MaxHistory=", 11) == 0) {
			set_config_int_value(line + 11, &conf.max_hist, 1, INT_MAX);
		}

		else if (*line == 'M' && strncmp(line, "MaxJumpTotalRank=", 17) == 0) {
			set_config_int_value(line + 17, &conf.max_jump_total_rank,
				1, INT_MAX);
		}

		else if (*line == 'M' && strncmp(line, "MaxLog=", 7) == 0) {
			set_config_int_value(line + 7, &conf.max_log, 1, INT_MAX);
		}

		else if (xargs.prompt_p_max_path == UNSET && *line == 'M'
		&& strncmp(line, "MaxPath=", 8) == 0) {
			err('n', PRINT_PROMPT, _("%s: MaxPath: This option is "
				"deprecated. Use the CLIFM_PROMPT_P_MAX_PATH environment "
				"variable instead.\n"), PROGRAM_NAME);
			set_config_int_value(line + 8, &conf.prompt_p_max_path, 1, INT_MAX);
		}

		else if (*line == 'M' && strncmp(line, "MaxPrintSelfiles=", 17) == 0) {
			set_config_int_value(line + 17, &conf.max_printselfiles,
				-1, INT_MAX);
		}

		else if (*line == 'M' && strncmp(line, "MinFilenameTrim=", 16) == 0) {
			err('n', PRINT_PROMPT, _("%s: MinFilenameTrim: This option is "
				"deprecated. Use MinNameTruncate instead.\n"), PROGRAM_NAME);
			set_config_int_value(line + 16, &conf.min_name_trunc, 1, INT_MAX);
		}

		else if (*line == 'M' && strncmp(line, "MinNameTruncate=", 16) == 0) {
			set_config_int_value(line + 16, &conf.min_name_trunc, 1, INT_MAX);
		}

		else if (*line == 'M' && strncmp(line, "MinJumpRank=", 12) == 0) {
			set_config_int_value(line + 12, &conf.min_jump_rank, -1, INT_MAX);
		}

		else if (*line == 'm' && strncmp(line, "mvCmd=", 6) == 0) {
			set_config_int_value(line + 6, &conf.mv_cmd,
				0, MV_CMD_AVAILABLE - 1);
		}

		else if (!conf.opener && *line == 'O'
		&& strncmp(line, "Opener=", 7) == 0) {
			char *tmp = get_line_value(line + 7);
			if (!tmp)
				continue;
			free(conf.opener);
			conf.opener = savestring(tmp, strlen(tmp));
		}

		else if (xargs.pager == UNSET && *line == 'P'
		&& strncmp(line, "Pager=", 6) == 0) {
			set_pager_value(line + 6, &conf.pager, sizeof(line) - 6);
		}

		else if (xargs.pager_view == UNSET && *line == 'P'
		&& strncmp(line, "PagerView=", 10) == 0) {
			set_pager_view_value(line + 10);
		}

		else if (*line == 'P' && strncmp(line, "PreviewMaxSize=", 15) == 0) {
			set_preview_max_size(line + 15);
		}

		else if (*line == 'P' && strncmp(line, "PrintDirCmds=", 13) == 0) {
			set_config_bool_value(line + 13, &conf.print_dir_cmds);
		}

		else if (xargs.print_selfiles == UNSET && *line == 'P'
		&& strncmp(line, "PrintSelfiles=", 14) == 0) {
			set_config_bool_value(line + 14, &conf.print_selfiles);
		}

		else if (*line == 'P' && strncmp(line, "PrioritySortChar=", 17) == 0) {
			set_priority_sort_char(line + 17);
		}

		else if (*line == 'P'
		&& strncmp(line, "PrivateWorkspaceSettings=", 25) == 0) {
			set_config_bool_value(line + 25, &conf.private_ws_settings);
		}

		else if (!*prop_fields_str && *line == 'P'
		&& strncmp(line, "PropFields=", 11) == 0) {
			char *tmp = get_line_value(line + 11);
			if (tmp)
				xstrsncpy(prop_fields_str, tmp, sizeof(prop_fields_str));
		}

		else if (*line == 'P' && strncmp(line, "PropFieldsGap=", 14) == 0) {
			set_config_int_value(line + 14, &conf.prop_fields_gap, 1, 2);
		}

		else if (xargs.ptime_style == UNSET && *line == 'P'
		&& strncmp(line, "PTimeStyle=", 11) == 0) {
			set_time_style(line + 11, &conf.ptime_str, 1);
		}

		else if (*line == 'P' && strncmp(line, "PurgeJumpDB=", 12) == 0) {
			set_config_bool_value(line + 12, &conf.purge_jumpdb);
		}

		else if (*line == 'Q' && strncmp(line, "QuotingStyle=", 13) == 0) {
			set_quoting_style(line + 13);
		}

		else if (*line == 'R' && strncmp(line, "ReadAutocmdFiles=", 17) == 0) {
			set_config_bool_value(line + 17, &conf.read_autocmd_files);
		}

		else if (*line == 'R' && strncmp(line, "ReadDotHidden=", 14) == 0) {
			set_config_bool_value(line + 14, &conf.read_dothidden);
		}

		else if (xargs.readonly == UNSET && *line == 'R'
		&& strncmp(line, "Readonly=", 9) == 0) {
			set_config_bool_value(line + 9, &conf.readonly);
		}

		else if (xargs.restore_last_path == UNSET && *line == 'R'
		&& strncmp(line, "RestoreLastPath=", 16) == 0) {
			set_config_bool_value(line + 16, &conf.restore_last_path);
		}

		else if (xargs.rl_vi_mode == UNSET && *line == 'R'
		&& strncmp(line, "RlEditMode=", 11) == 0) {
			set_rl_edit_mode(line + 11);
		}

		else if (*line == 'r' && strncmp(line, "rmForce=", 8) == 0) {
			set_config_bool_value(line + 8, &conf.rm_force);
		}

		else if (*line == 'S' && strncmp(line, "SearchStrategy=", 15) == 0) {
			set_search_strategy(line + 15);
		}

		else if (xargs.share_selbox == UNSET && *line == 'S'
		&& strncmp(line, "ShareSelbox=", 12) == 0) {
			set_config_bool_value(line + 12, &conf.share_selbox);
		}

		else if (xargs.show_hidden == UNSET && *line == 'S'
		&& strncmp(line, "ShowHiddenFiles=", 16) == 0) {
			set_show_hidden_files(line + 16);
		}

		else if (*line == 'S'
		&& strncmp(line, "SkipNonAlnumPrefix=", 19) == 0) {
			set_config_bool_value(line + 19, &conf.skip_non_alnum_prefix);
		}

		else if (xargs.sort == UNSET && *line == 'S'
		&& strncmp(line, "Sort=", 5) == 0) {
			if (!IS_DIGIT(line[5]))
				set_sort_name(line + 5);
			else
				set_config_int_value(line + 5, &conf.sort, 0, SORT_TYPES);
		}

		else if (xargs.sort_reverse == UNSET && *line == 'S'
		&& strncmp(line, "SortReverse=", 12) == 0) {
			set_config_bool_value(line + 12, &conf.sort_reverse);
		}

		else if (xargs.splash_screen == UNSET && *line == 'S'
		&& strncmp(line, "SplashScreen=", 13) == 0) {
			set_config_bool_value(line + 13, &conf.splash_screen);
		}

		else if (xargs.path == UNSET && cur_ws == UNSET && *line == 'S'
		&& strncmp(line, "StartingPath=", 13) == 0) {
			set_starting_path(line + 13);
		}

#ifndef _NO_SUGGESTIONS
		else if (*line == 'S' && strncmp(line, "SuggestCmdDesc=", 15) == 0) {
			set_config_bool_value(line + 15, &conf.cmd_desc_sug);
		}

		else if (*line == 'S'
		&& strncmp(line, "SuggestFiletypeColor=", 21) == 0) {
			set_config_bool_value(line + 21, &conf.suggest_filetype_color);
		}

		else if (*line == 'S'
		&& strncmp(line, "SuggestionStrategy=", 19) == 0) {
			set_sug_strat(line + 19);
		}
#endif /* !_NO_SUGGESTIONS */

#ifndef _NO_HIGHLIGHT
		else if (xargs.highlight == UNSET && *line == 'S'
		&& strncmp(line, "SyntaxHighlighting=", 19) == 0) {
			set_config_bool_value(line + 19, &conf.highlight);
		}
#endif /* !_NO_HIGHLIGHT */

#ifndef _NO_FZF
		else if (xargs.fzftab == UNSET
		&& xargs.fnftab == UNSET && xargs.smenutab == UNSET
		&& *line == 'T' && strncmp(line, "TabCompletionMode=", 18) == 0) {
			set_tabcomp_mode(line + 18);
		}
#endif /* !_NO_FZF */

		else if (*line == 'T' && strncmp(line, "TerminalCmd=", 12) == 0) {
			set_term_cmd(line + 12);
		}

		else if (*line == 'T' && strncmp(line, "TimeFollowsSort=", 16) == 0) {
			set_config_bool_value(line + 16, &conf.time_follows_sort);
		}

		else if (*line == 'T' && strncmp(line, "TimestampMark=", 14) == 0) {
			set_config_bool_value(line + 14, &conf.timestamp_mark);
		}

		else if (xargs.time_style == UNSET && *line == 'T'
		&& strncmp(line, "TimeStyle=", 10) == 0) {
			set_time_style(line + 10, &conf.time_str, 0);
		}

		else if (xargs.tips == UNSET && *line == 'T'
		&& strncmp(line, "Tips=", 5) == 0) {
			set_config_bool_value(line + 5, &conf.tips);
		}

#ifndef _NO_TRASH
		else if (xargs.trasrm == UNSET && *line == 'T'
		&& strncmp(line, "TrashAsRm=", 10) == 0) {
			set_config_bool_value(line + 10, &conf.tr_as_rm);
		}

		else if (*line == 'T' && strncmp(line, "TrashForce=", 11) == 0) {
			set_config_bool_value(line + 11, &conf.trash_force);
		}
#endif /* !_NO_TRASH */

		else if (xargs.trunc_names == UNSET && *line == 'T'
		&& strncmp(line, "TrimNames=", 10) == 0) {
			err('n', PRINT_PROMPT, _("%s: TrimNames: This option is "
				"deprecated. Use TruncateNames instead.\n"), PROGRAM_NAME);
			set_config_bool_value(line + 10, &conf.trunc_names);
		}

		else if (xargs.trunc_names == UNSET && *line == 'T'
		&& strncmp(line, "TruncateNames=", 14) == 0) {
			set_config_bool_value(line + 14, &conf.trunc_names);
		}

		else if (xargs.secure_env != 1 && xargs.secure_env_full != 1
		&& *line == 'U' && strncmp(line, "Umask=", 6) == 0) {
			unsigned int opt_num = MAX_UMASK + 1;
			ret = sscanf(line + 6, "%o\n", &opt_num);
			if (ret == -1 || opt_num > MAX_UMASK)
				continue;
			umask((mode_t)opt_num); /* flawfinder: ignore */
		}

		else if (xargs.welcome_message == UNSET && *line == 'W'
			&& strncmp(line, "WelcomeMessage=", 15) == 0) {
			set_config_bool_value(line + 15, &conf.welcome_message);
		}

		else if (*line == 'W' && strncmp(line, "WelcomeMessageStr=", 18) == 0) {
			char *tmp = get_line_value(line + 18);
			if (!tmp)
				continue;
			free(conf.welcome_message_str);
			conf.welcome_message_str = savestring(tmp, strlen(tmp));
		}

		else {
			if (*line == 'W' && strncmp(line, "WorkspaceNames=", 15) == 0)
				set_workspace_names(line + 15);
		}
	}

	fclose(config_fp);

	if (default_answers_set == 0)
		set_default_answers_to_default();

	if (xargs.disk_usage_analyzer == 1) {
		conf.sort = STSIZE;
		conf.long_view = conf.full_dir_size = 1;
		conf.list_dirs_first = conf.welcome_message = 0;
	}

	if (filter.str && filter.type == FILTER_FILE_NAME) {
		regfree(&regex_exp);
		ret = regcomp(&regex_exp, filter.str, REG_NOSUB | REG_EXTENDED);
		if (ret != FUNC_SUCCESS) {
			err('w', PRINT_PROMPT, _("%s: '%s': Invalid regular "
				"expression\n"), PROGRAM_NAME, filter.str);
			free(filter.str);
			filter.str = (char *)NULL;
			regfree(&regex_exp);
		}
	}
}
#endif /* CLIFM_SUCKLESS */

static int
set_force_color(const char *val)
{
	const int fallback = 8;

	if (!val || !*val)
		return fallback;

	if ((*val == '2' && strcmp(val + 1, "4bit") == 0)
	|| (*val == 't' && strcmp(val + 1, "ruecolor") == 0))
		return TRUECOLOR_NUM;

	if (!is_number(val))
		return fallback;

	const int n = atoi(val);
	if (n == 8 || n == 16 || n == 256 || n == TRUECOLOR_NUM)
		return n;

	return fallback;
}

static void
check_colors(void)
{
	char *nc = getenv("NO_COLOR");
	char *cnc = getenv("CLIFM_NO_COLOR");

	char *ccf = getenv("CLICOLOR_FORCE"); /* See https://bixense.com/clicolors */
	char *cfc = getenv("CLIFM_FORCE_COLOR");

	if (xargs.colorize == UNSET && !nc && !cnc && (ccf || cfc)) {
		if (term_caps.color == 0)
			/* The user is forcing the use of colors even when the terminal
			 * reports no color capability. */
			term_caps.color = cfc ? set_force_color(cfc) : 8;
		conf.colorize = 1;
	} else if (xargs.colorize == 0 || term_caps.color == 0 || nc || cnc) {
		conf.colorize = 0;
	} else {
		conf.colorize = xargs.colorize == 1 ? 1 : DEF_COLORS; /* NOLINT */
	}

	if (conf.colorize == 1) {
		set_colors(conf.usr_cscheme ? conf.usr_cscheme
			: (term_caps.color >= 256
			? DEF_COLOR_SCHEME_256 : DEF_COLOR_SCHEME), 1);
		cur_color = tx_c;
		return;
	}

	reset_filetype_colors();
	reset_iface_colors();
	cur_color = tx_c;
}

#ifndef _NO_FZF
static char
first_non_blank(const char *line)
{
	if (!line || !*line)
		return 0;

	while (*line) {
		if (*line > ' ')
			return *line;
		line++;
	}

	return 0;
}

/* Return a macro describing the border type set for the '--border' option
 * in a fzf command (either FZF_DEFAULT_OPTS or FzfTabOptions). */
static int
get_fzf_border(char *line)
{
	if (!line || !*line || (*line != ' ' && *line != '='))
		/* No value for "--border". It defaults to "rounded". */
		return FZF_BORDER_ROUNDED;

	line++;

	char *p = line + (*line == '\'' || *line == '"');
	char c = first_non_blank(p);
	if (!*p || *p == '-' || c == '-' || c == '\0')
		return FZF_BORDER_ROUNDED;

	if (*p == 'b') {
		if (strncmp(p, "block", 5) == 0) return FZF_BORDER_BLOCK;
		if (strncmp(p, "bold", 4) == 0) return FZF_BORDER_BOLD;
		if (strncmp(p, "bottom", 6) == 0) return FZF_BORDER_BOTTOM;
	}
	if (*p == 'd' && strncmp(p, "double", 6) == 0) return FZF_BORDER_DOUBLE;
	if (*p == 'l' && strncmp(p, "left", 4) == 0) return FZF_BORDER_LEFT;
	if (*p == 'n' && strncmp(p, "none", 4) == 0) return FZF_BORDER_NONE;
	if (*p == 'r') {
		if (strncmp(p, "right", 5) == 0) return FZF_BORDER_RIGHT;
		if (strncmp(p, "rounded", 7) == 0) return FZF_BORDER_ROUNDED;
	}
	if (*p == 's' && strncmp(p, "sharp", 5) == 0) return FZF_BORDER_SHARP;
	if (*p == 't') {
		if (strncmp(p, "top", 3) == 0) return FZF_BORDER_TOP;
		if (strncmp(p, "thinblock", 9) == 0) return FZF_BORDER_THINBLOCK;
	}
	if (*p == 'v' && strncmp(p, "vertical", 8) == 0) return FZF_BORDER_VERT;

/*	return FZF_BORDER_ROUNDED; */
	return FZF_BORDER_NONE; /* == 0 */
}

/* Set fzf_border_type (global) to a value describing the value of fzf
 * --border option: 0 (no vertical border), 1 (left or right border),
 * or 2 (left and right border). We need this value to properly calculate
 * the width of the fzf preview window (get_preview_win_width(), in tabcomp.c). */
int
get_fzf_border_type(char *line)
{
	fzf_ext_border = get_fzf_border(line);

	switch (fzf_ext_border) {
	case FZF_BORDER_ROUNDED: /* fallthrough */
	case FZF_BORDER_SHARP:   /* fallthrough */
	case FZF_BORDER_BOLD:    /* fallthrough */
	case FZF_BORDER_DOUBLE:  /* fallthrough */
	case FZF_BORDER_BLOCK:   /* fallthrough */
	case FZF_BORDER_THINBLOCK:
	case FZF_BORDER_VERT: return 2;
	case FZF_BORDER_LEFT:    /* fallthrough */
	case FZF_BORDER_RIGHT: return 1;
	default: return 0;
	}
}

int
get_fzf_height(char *line)
{
	/* For the time being, the '~' prefix is not supported for "--height" */
	if (!line || !*line || !*(++line) || *line == '~')
		return 0;

	int val = 0;
	int percent = 0;
	size_t len = 0;

	get_term_size();

	char *s = strchr(line, ' ');
	if (s)
		*s = '\0';

	if (*line == '-') {
		const int n = atoi(line + 1);
		if (n >= 0 && n < term_lines)
			val = term_lines - n;
		goto END;
	}

	len = strlen(line);
	if (len > 1 && line[len - 1] == '%') {
		line[len - 1] = '\0';
		percent = 1;
	}

	if (!is_number(line))
		goto END;

	if (percent == 0) {
		val = atoi(line);
		goto END;
	} else {
		line[len - 1] = '\0';
		const int n = atoi(line);
		if (n > 0 && n <= 100)
			val = n * term_lines / 100;
	}

END:
	if (s)
		*s = ' ';
	if (percent == 1)
		line[len - 1] = '%';

	return val;
}

/* Just check if --height, --border, and --preview are set in FZF_DEFAULT_OPTS. */
static void
get_fzf_win_height_and_preview(void)
{
	char *p = xargs.secure_env_full == 1 ? NULL : getenv("FZF_DEFAULT_OPTS");
	if (!p || !*p)
		return;

	if (conf.fzf_preview == UNSET && strstr(p, "--preview "))
		conf.fzf_preview = FZF_EXTERNAL_PREVIEWER;

	char *b = (char *)NULL;
	if ((b = strstr(p, "--height")) != NULL)
		fzf_height_value = get_fzf_height(b + (sizeof("--height") - 1));

	if (fzf_border_type == UNSET && (b = strstr(p, "--border")) != NULL)
		fzf_border_type = get_fzf_border_type(b + (sizeof("--border") - 1));
}
#endif /* !_NO_FZF */

#ifndef _NO_TRASH
static void
create_trash_dirs(void)
{
	struct stat a;
	if (stat(trash_files_dir, &a) != -1 && S_ISDIR(a.st_mode)
	&& stat(trash_info_dir, &a) != -1 && S_ISDIR(a.st_mode))
		return;

	if (xargs.stealth_mode == 1) {
		err('w', PRINT_PROMPT, _("%s: '%s': %s. Trash function disabled. "
			"If needed, create the directories manually and restart %s.\n"
			"E.g.: mkdir -p ~/.local/share/Trash/{files,info}\n"),
			PROGRAM_NAME, trash_dir, strerror(errno), PROGRAM_NAME);
		trash_ok = 0;
		return;
	}

	char *cmd[] = {"mkdir", "-p", "--", trash_files_dir, trash_info_dir, NULL};
	const int ret = launch_execv(cmd, FOREGROUND, E_NOSTDERR);

	if (ret != FUNC_SUCCESS) {
		trash_ok = 0;
		err('w', PRINT_PROMPT, _("%s: '%s': Cannot create the trash "
			"directory (or one of its subdirectories: 'files' and 'info').\n"
			"Try creating them manually and restart %s.\n"
			"E.g.: mkdir -p ~/.local/share/Trash/{files,info}\n"),
			PROGRAM_NAME, trash_dir, PROGRAM_NAME);
		return;
	}
}

static void
set_trash_dirs(void)
{
	size_t len = 0;

	if (alt_trash_dir) {
		len = strlen(alt_trash_dir);
		trash_dir = savestring(alt_trash_dir, len);
	} else {
		if (!user.home) {
			trash_ok = 0;
			return;
		}

		char *env = (char *)NULL;
		char *data_home = (char *)NULL;
		if (xargs.secure_env != 1 && xargs.secure_env_full != 1
		&& (env = getenv("XDG_DATA_HOME")) && *env
		&& (data_home = normalize_path(env, strlen(env))) && *data_home) {
			len = strlen(data_home) + 7;
			trash_dir = xnmalloc(len, sizeof(char));
			snprintf(trash_dir, len, "%s/Trash", data_home);
			free(data_home);
		} else {
			free(data_home); /* In case it is not NULL, but starts with NUL. */
			len = user.home_len + 20;
			trash_dir = xnmalloc(len, sizeof(char));
			snprintf(trash_dir, len, "%s/.local/share/Trash", user.home);
		}
	}

	const size_t trash_len = strlen(trash_dir);

	len = trash_len + 7;
	trash_files_dir = xnmalloc(len, sizeof(char));
	snprintf(trash_files_dir, len, "%s/files", trash_dir);

	len = trash_len + 6;
	trash_info_dir = xnmalloc(len, sizeof(char));
	snprintf(trash_info_dir, len, "%s/info", trash_dir);

	create_trash_dirs();
}
#endif /* _NO_TRASH */

static void
undef_config_file_names(void)
{
	free(config_dir_gral); config_dir_gral = (char *)NULL;
	free(config_dir); config_dir = (char *)NULL;
	free(tags_dir); tags_dir = (char *)NULL;
	free(kbinds_file); kbinds_file = (char *)NULL;
	free(colors_dir); colors_dir = (char *)NULL;
	free(plugins_dir); plugins_dir = (char *)NULL;
	free(dirhist_file); dirhist_file = (char *)NULL;
	free(bm_file); bm_file = (char *)NULL;
	free(msgs_log_file); msgs_log_file = (char *)NULL;
	free(cmds_log_file); cmds_log_file = (char *)NULL;
	free(hist_file); hist_file = (char *)NULL;
	free(config_file); config_file = (char *)NULL;
	free(profile_file); profile_file = (char *)NULL;
	free(mime_file); mime_file = (char *)NULL;
	free(actions_file); actions_file = (char *)NULL;
	free(remotes_file); remotes_file = (char *)NULL;
}

/* Set up Clifm directories and config files. Load the user's
 * configuration from the 'clifmrc' file. */
void
init_config(void)
{
	/* If just listing (--list-and-quit or --stat(-full)), we only need to
	 * load the main config file and colors. */
	const int just_listing = (xargs.stat > 0 || xargs.list_and_quit == 1);

#ifndef _NO_TRASH
	if (just_listing == 0)
		set_trash_dirs();
#endif /* _NO_TRASH */

#ifdef LINUX_FSINFO
	if (just_listing == 0)
		get_ext_mountpoints();
#endif /* LINUX_FSINFO */

	if (xargs.stealth_mode == 1) {
		err(ERR_NO_LOG, PRINT_PROMPT, _("%s: Running in stealth mode: "
			"persistent selection, bookmarks, jump database and directory "
			"history, just as plugins, logs and configuration files, are "
			"disabled.\n"), PROGRAM_NAME);
		config_ok = 0;
		check_colors();
		return;
	}

	if (home_ok == 0) {
		check_colors();
		return;
	}

	define_config_file_names();
	create_config_files(just_listing);

	if (config_ok == 0) {
		undef_config_file_names();
		check_colors();
		return;
	}

#ifndef CLIFM_SUCKLESS
	cschemes_n = get_colorschemes();
	read_config();
#else
	xstrsncpy(div_line, DEF_DIV_LINE, sizeof(div_line));
#endif /* !CLIFM_SUCKLESS */

	if (!conf.time_str)
		set_time_style_env();
	if (!conf.ptime_str)
		set_ptime_style_env();

	if (just_listing == 0)
		load_prompts();

	check_colors();

	if (xargs.secure_env == 1 || xargs.secure_env_full == 1)
		check_config_files_integrity();

#ifndef _NO_FZF
	/* If FZF win height was not defined in the config file,
	 * check whether it is present in FZF_DEFAULT_OPTS. Same thing for
	 * the previewer (--preview option). */
	if (fzftab && just_listing == 0)
		get_fzf_win_height_and_preview();
#endif /* !_NO_FZF */

	if (just_listing == 1)
		return;

	char *t = getenv("TERM");
	if (t && *t == 'x' && strncmp(t, "xterm", 5) == 0)
		/* If running Xterm, instruct it to send an escape code (27) for
		 * Meta (Alt) key sequences. Otherwise, Alt keybindings won't work. */
		META_SENDS_ESC;
}

static void
free_regex_filters(void)
{
	if (filter.str && filter.env == 0) {
		regfree(&regex_exp);
		free(filter.str);
		filter.str = (char *)NULL;
		filter.rev = 0;
		filter.type = FILTER_NONE;
	}

	if (conf.histignore_regex) {
		regfree(&regex_hist);
		free(conf.histignore_regex);
		conf.histignore_regex = (char *)NULL;
	}

	if (conf.dirhistignore_regex) {
		regfree(&regex_dirhist);
		free(conf.dirhistignore_regex);
		conf.dirhistignore_regex = (char *)NULL;
	}
}

static void
free_msgs(void)
{
	size_t i;
	for (i = 0; i < msgs_n; i++)
		free(messages[i].text);

	msgs_n = msgs.error = msgs.warning = msgs.notice = 0;
	pmsg = NOMSG;
}

static void
reset_variables(void)
{
	/* Free everything */
	free(conf.time_str);
	free(conf.ptime_str);
	free(conf.priority_sort_char);
	conf.time_str = conf.ptime_str = conf.priority_sort_char = (char *)NULL;

	free(conf.usr_cscheme);
	conf.usr_cscheme = (char *)NULL;

	free(config_dir_gral);
	free(config_dir);
	config_dir = config_dir_gral = (char *)NULL;

#ifndef _NO_TRASH
	free(trash_dir);
	free(trash_files_dir);
	free(trash_info_dir);
	trash_dir = trash_files_dir = trash_info_dir = (char *)NULL;
#endif /* !_NO_TRASH */

	free(bm_file);
	free(msgs_log_file);
	free(cmds_log_file);
	bm_file = msgs_log_file = cmds_log_file = (char *)NULL;

	free(hist_file);
	free(dirhist_file);
	hist_file = dirhist_file = (char *)NULL;

	free(config_file);
	free(profile_file);
	config_file = profile_file = (char *)NULL;

	free(mime_file);
	free(plugins_dir);
	free(actions_file);
	free(kbinds_file);
	mime_file = plugins_dir = actions_file = kbinds_file = (char *)NULL;

	free(plugins_helper_file);
	plugins_helper_file = (char *)NULL;

	free(colors_dir);
	free(tmp_dir);
	free(sel_file);
	free(remotes_file);
	tmp_dir = colors_dir = sel_file = remotes_file = (char *)NULL;

#ifndef _NO_SUGGESTIONS
	free(suggestion_buf);
	free(conf.suggestion_strategy);
	suggestion_buf = conf.suggestion_strategy = (char *)NULL;
#endif /* !_NO_SUGGESTIONS */

	free(conf.fzftab_options);
	free(tags_dir);
	free(conf.wprompt_str);
	conf.fzftab_options = tags_dir = conf.wprompt_str = (char *)NULL;
	free(conf.welcome_message_str);
	conf.welcome_message_str = (char *)NULL;

	free_autocmds(1);
	free_tags();
	free_remotes(0);
	free_regex_filters();
	free_msgs();

	free(conf.opener);
	free(conf.encoded_prompt);
	free(conf.term);
	conf.opener = conf.encoded_prompt = conf.term = (char *)NULL;

	init_conf_struct();
	free_workspaces_filters();

#ifndef _NO_FZF
	fzftab = UNSET;
#endif /* !_NO_FZF */
	hist_status = UNSET;
	print_removed_files = UNSET;
	prompt_offset = UNSET;
	prompt_notif = UNSET;

	dir_changed = 0;
	dequoted = 0;
	internal_cmd = 0;
	is_sel = 0;
	kbind_busy = 0;
	no_log = 0;
	print_msg = 0;
	recur_perm_error_flag = 0;
	sel_is_last = 0;
	shell_is_interactive = 0;
	shell_terminal = 0;
	conf.sort_reverse = 0;
	sort_switch = 0;

	config_ok = 1;
	home_ok = 1;
	selfile_ok = 1;
#ifndef _NO_TRASH
	trash_ok = 1;
#endif /* !_NO_TRASH */

	pmsg = NOMSG;
}

#ifndef _NO_FZF
static void
update_finder_binaries_status(void)
{
	if (!(bin_flags & FZF_BIN_OK) && is_cmd_in_path("fzf") == 1)
		bin_flags |= FZF_BIN_OK;

	if (!(bin_flags & FNF_BIN_OK) && is_cmd_in_path("fnf") == 1)
		bin_flags |= FNF_BIN_OK;

	if (!(bin_flags & SMENU_BIN_OK) && is_cmd_in_path("smenu") == 1)
		bin_flags |= SMENU_BIN_OK;
}
#endif /* !_NO_FZF */

int
reload_config(void)
{
#ifndef _NO_FZF
	enum tab_mode tabmode_bk = tabmode;
#endif /* !_NO_FZF */

	reset_variables();

	/* Set up config files and options */
	init_config();

	/* If some option was not set, set it to the default value */
	check_options();
	set_sel_file();
	create_tmp_files();

#ifndef _NO_FZF
	if (tabmode_bk != tabmode)
		update_finder_binaries_status();
	check_completion_mode();
#endif /* !_NO_FZF */

	/* Free the aliases and prompt_cmds arrays to be allocated again */
	int i = dirhist_total_index;
	while (--i >= 0)
		free(old_pwd[i]);
	free(old_pwd);
	old_pwd = (char **)NULL;

	if (jump_db) {
		for (i = 0; jump_db[i].path; i++)
			free(jump_db[i].path);

		free(jump_db);
		jump_db = (struct jump_t *)NULL;
	}
	jump_n = 0;

	i = (int)aliases_n;
	while (--i >= 0) {
		free(aliases[i].name);
		free(aliases[i].cmd);
	}
	free(aliases);
	aliases = (struct alias_t *)NULL;
	aliases_n = 0;

	i = (int)prompt_cmds_n;
	while (--i >= 0)
		free(prompt_cmds[i]);

	dirhist_total_index = 0;
	prompt_cmds_n = 0;

	get_aliases();
	get_prompt_cmds();
	load_dirhist();
	load_jumpdb();
	load_tags();
	load_remotes();
	init_workspaces_opts();

	/* Set the current position of the dirhist index to the last entry. */
	dirhist_cur_index = dirhist_total_index - 1;

	set_env(1);

	dir_changed = (autocmds_n > 0);

	return FUNC_SUCCESS;
}
