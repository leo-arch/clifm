/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* listing.c -- functions controlling what is listed on the screen */

#include "helpers.h"

#include <sys/statvfs.h>
#include <unistd.h> /* open(2), readlinkat(2) */
#include <errno.h>
#include <string.h>
#if defined(__OpenBSD__)
# include <strings.h>
# include <inttypes.h> /* uintmax_t */
#endif /* __OpenBSD__ */

#if defined(LINUX_FILE_CAPS)
# include <sys/capability.h>
#endif /* LINUX_FILE_CAPS */

#if defined(LINUX_FILE_XATTRS)
# include <sys/xattr.h>
#endif /* LINUX_FILE_XATTRS */

#if defined(LINUX_FSINFO) || defined(HAVE_STATFS) || defined(__sun)
# include "fsinfo.h"
#endif /* LINUX_FSINFO || HAVE_STATFS || __sun */

#if defined(LIST_SPEED_TEST)
# include <time.h>
#endif /* LIST_SPEED_TEST */

#if defined(LINUX_FSINFO)
# include <sys/sysmacros.h> /* major() macro */
#endif /* LINUX_FSINFO */

#if defined(TOURBIN_QSORT)
# include "qsort.h"
# define ENTLESS(i, j) (entrycmp(file_info + (i), file_info + (j)) < 0)
# define ENTSWAP(i, j) (swap_ent((i), (j)))
# define ENTSORT(file_info, n, entrycmp) QSORT((n), ENTLESS, ENTSWAP)
#else
# define ENTSORT(file_info, n, entrycmp) qsort((file_info), (n), sizeof(*(file_info)), (entrycmp))
#endif /* TOURBIN_QSORT */

/* Check for temporary files:
 * 1. "*~"   Gral purpose temp files (mostly used by text editors)
 * 2. "#*#"  Emacs auto-save temp files
 * 3. ".~*#" Libreoffice lock files
 * 4. "~$*"  MS Office temp files
 * NOTE: MC seems to take only "*~" and "#*" as temp files. */
#define IS_TEMP_FILE(n, l) ( (l) > 0               \
	&& ((n)[(l) - 1] == '~'                        \
	|| ((*(n) == '#' || (*(n) == '.'               \
		&& (n)[1] == '~')) && (n)[(l) - 1] == '#') \
	|| (*(n) == '~' && (n)[1] == '$') ) )

#define IS_EXEC(m) (((m) & S_IXUSR) || ((m) & S_IXGRP) || ((m) & S_IXOTH))

#include "autocmds.h"
#include "aux.h"
#include "checks.h"
#include "colors.h"
#include "dothidden.h" /* load_dothidden, check_dothidden, free_dothidden */
#include "fs_events.h" /* set_events_checker */
#ifndef _NO_ICONS
# include "icons.h"
#endif /* !_NO_ICONS */
#include "messages.h"
#include "misc.h"
#include "properties.h" /* print_analysis_stats() */
#include "long_view.h"  /* print_entry_props() */
#include "sanitize.h"
#include "sort.h"
#include "spawn.h"
#include "xdu.h"        /* dir_size() */

/* Macros for the return value of the pager_run function */
#define PAGER_RET_OK   0
#define PAGER_RET_BACK 1
#define PAGER_RET_HELP 2
#define PAGER_RET_QUIT 3

/* Macros for run_dir_cmd function */
#define AUTOCMD_DIR_IN  0
#define AUTOCMD_DIR_OUT 1
#define AUTOCMD_DIR_IN_FILE  ".cfm.in"
#define AUTOCMD_DIR_OUT_FILE ".cfm.out"

#define ENTRY_N 64

#ifdef TIGHT_COLUMNS
# define COLUMNS_GAP 2
#endif

#define ICONS_ELN       0
#define ICONS_NO_ELN    1
#define NO_ICONS_ELN    2
#define NO_ICONS_NO_ELN 3

/* Information about the longest filename in the curent list of files. */
struct longest_t {
	size_t fc_len;   /* Length of the file counter (if a directory). */
	size_t name_len; /* Length of the longest name. */
};

static struct longest_t longest;

struct checks_t {
	char *icons_gap;
	int autocmd_files;
	int birthtime;
	int classify;
	int file_counter;
	int filter_name;
	int filter_type;
	int icons_use_file_color;
	int id_names;
	int lnk_char;
	int min_name_trunc;
	int scanning;
	int time_follows_sort;
	int xattr;
	int list_format;
};

static struct checks_t checks;

/* Struct to store information about truncated filenames. */
struct wtrunc_t {
	char *wname; /* Address to store filename with replaced control chars */
	int type; /* Truncation type: with or without file extension. */
	int diff; /* */
};

/* Hold default values for the fileinfo struct. */
static struct fileinfo default_file_info;

static int pager_bk = 0;
static int dir_out = 0;
static int pager_quit = 0;
static int pager_help = 0;
static int long_view_bk = UNSET;

/* A version of the loop-unswitching optimization: move loop-invariant
 * conditions out of the loop to reduce the number of conditions in each
 * loop pass.
 * In this case, instead of checking multiple loop-invariant conditions
 * in each pass of the main file listing loop (in list_dir()), we check
 * only one (i.e. the appropriate member of the checks struct).
 * The most important here is checks.lnk_char: instead of checking 4
 * conditions per listed file, we check only one. Thus, if we have 100 files,
 * this saves 300 condition checks! */
static void
init_checks_struct(void)
{
	checks.autocmd_files = (conf.read_autocmd_files == 1 && dir_changed == 1);
	checks.birthtime = (conf.sort == SBTIME || (conf.long_view == 1
		&& prop_fields.time == PROP_TIME_BIRTH));
	checks.classify = (conf.long_view == 0 && conf.classify == 1);

	checks.file_counter = (conf.file_counter == 1
		&& ((conf.long_view == 1 && prop_fields.counter == 1)
		|| (conf.long_view == 0 && conf.classify == 1)));

	checks.filter_name = (filter.str && filter.type == FILTER_FILE_NAME);
	checks.filter_type = (filter.str && filter.type == FILTER_FILE_TYPE);

	checks.icons_gap = conf.icons_gap <= 0 ? ""
		: ((conf.icons_gap == 1) ? " " : "  ");

	checks.icons_use_file_color =
#ifndef _NO_ICONS
		(xargs.icons_use_file_color == 1 && conf.icons == 1);
#else
		0;
#endif /* !_NO_ICONS */

	checks.id_names = (prop_fields.ids == PROP_ID_NAME
		&& (conf.long_view == 1 || conf.sort == SOWN || conf.sort == SGRP));
	checks.lnk_char = (conf.colorize_lnk_as_target == 1 && conf.follow_symlinks == 1
		&& conf.icons == 0 && conf.light_mode == 0 && conf.colorize == 1);
	checks.min_name_trunc = (conf.long_view == 1 && conf.max_name_len != UNSET
		&& conf.min_name_trunc > conf.max_name_len);
	checks.scanning = (xargs.disk_usage_analyzer == 1
		|| (conf.long_view == 1 && conf.full_dir_size == 1));
	checks.time_follows_sort = (conf.time_follows_sort == 1
		&& conf.sort >= SATIME && conf.sort <= SMTIME);
	checks.xattr = (conf.long_view == 1 && prop_fields.xattr == 1);

	if (conf.icons == 1)
		checks.list_format = conf.no_eln == 1 ? ICONS_NO_ELN : ICONS_ELN;
	else
		checks.list_format = conf.no_eln == 1 ? NO_ICONS_NO_ELN : NO_ICONS_ELN;
}

#if !defined(_NO_ICONS)
/* Create a list of hashes for file names associated to icons. */
static void
set_icon_name_hashes(void)
{
	size_t i = sizeof(icon_filenames) / sizeof(icon_filenames[0]);
	name_icon_hashes = xnmalloc(i + 1, sizeof(size_t));

	for (; i-- > 0;)
		name_icon_hashes[i] = hashme(icon_filenames[i].name, 0);
}

/* Create a list of hashes for directory names associated to icons. */
static void
set_dir_name_hashes(void)
{
	size_t i = sizeof(icon_dirnames) / sizeof(icon_dirnames[0]);
	dir_icon_hashes = xnmalloc(i + 1, sizeof(size_t));

	for (; i-- > 0;)
		dir_icon_hashes[i] = hashme(icon_dirnames[i].name, 0);
}

static void
/* Create a list of hashes for file extensions associated to icons. */
set_ext_name_hashes(void)
{
	size_t i = sizeof(icon_ext) / sizeof(icon_ext[0]);
	ext_icon_hashes = xnmalloc(i + 1,  sizeof(size_t));

	for (; i-- > 0;)
		ext_icon_hashes[i] = hashme(icon_ext[i].name, 0);

#ifdef CHECK_ICONS
	const size_t total = sizeof(icon_ext) / sizeof(icon_ext[0]);
	size_t conflicts = 0;
	for (i = 0; i < total; i++) {
		for (size_t j = i + 1; j < total; j++) {
			if (ext_icon_hashes[i] != ext_icon_hashes[j])
				continue:
			printf("%s conflicts with %s\n", icon_ext[i].name, icon_ext[j].name);
			conflicts++;
		}
	}

	printf("Number of icons: %zu\n", total);
	printf("Icon conflicts:  %zu\n", conflicts);
	press_any_key_to_continue(0);
#endif
}

/* Set the icon field to the corresponding icon for the file file_info[N].name */
static int
get_name_icon(const filesn_t n)
{
	if (!file_info[n].name)
		return 0;

	const size_t name_hash = hashme(file_info[n].name, 0);

	/* This division will be replaced by a constant integer at compile
	 * time, so that it won't even be executed at runtime. */
	size_t i = sizeof(icon_filenames) / sizeof(icon_filenames[0]);
	for (; i-- > 0;) {
		if (name_hash != name_icon_hashes[i])
			continue;
		file_info[n].icon = icon_filenames[i].icon;
		file_info[n].icon_color = icon_filenames[i].color;
		return 1;
	}

	return 0;
}

/* Set the icon field to the corresponding icon for the directory
 * file_info[N].name. If not found, set the default icon. */
static void
get_dir_icon(const filesn_t n)
{
	if (file_info[n].user_access == 0)
		/* Icon already set by load_file_gral_info() */
		return;

	/* Default values for directories */
	file_info[n].icon = DEF_DIR_ICON;
	/* DIR_ICO_C is set from the color scheme file */
	file_info[n].icon_color = *dir_ico_c ? dir_ico_c : DEF_DIR_ICON_COLOR;

	if (!file_info[n].name)
		return;

	const size_t dir_hash = hashme(file_info[n].name, 0);

	size_t i = sizeof(icon_dirnames) / sizeof(icon_dirnames[0]);
	for (; i-- > 0;) {
		if (dir_hash != dir_icon_hashes[i])
			continue;
		file_info[n].icon = icon_dirnames[i].icon;
		file_info[n].icon_color =
			*dir_ico_c ? dir_ico_c : icon_dirnames[i].color;
		return;
	}
}

#define TABLE_LOAD_FACTOR 0.75
/* 0.75: Do not allow the table to be loaded beyond 75% (i.e. make sure there
 * is always at least 25% free slots). This value balances space vs. probe
 * length for linear probing: keeps average probe counts low (good cache/branch
 * behavior) while using memory efficiently. */

/* Multiplicative mixing constant (Knuth for 32-bit, Fibonacci for 64-bit),
 * used to quickly scramble hashes before masking; improves bit diffusion
 * for power-of-two tables. */
#if SIZE_MAX > UINT32_MAX
# define HASH_MULTIPLIER 11400714819323198485ULL /* 64-bit Fibonacci constant */
#else
# define HASH_MULTIPLIER 2654435761ULL /* Knuth's 32-bit constant */
#endif

static size_t ext_table_mask = 0;
static size_t ext_table_size = 0;

/* Return the power-of-two value closest to V + 1. */
static size_t
next_pow2(size_t v) {
	if (v == 0)
		return 1;

	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
#if SIZE_MAX > UINT32_MAX
	v |= v >> 32;
#endif
	return ++v;
}

/* Build an open-addressed lookup table mapping extension name hashes (in
 * ext_icons_hashes[]) to an index in icon_ext[].
 *
 * Hashes are reduced to a valid index in icon_ext [0, table_size - 1] by first
 * mixing and then masking it: (hash * HASH_MULTIPLIER) & ext_table_mask.
 * A slot contains SIZE_MAX when empty.
 *
 * Hashes are computed at startup from the static icon_ext[] table and stored
 * in ext_icons_hashes[] by set_ext_name_hashes(). We validate uniqueness at
 * init; for the current static key set the hashes are unique, so lookup will
 * never encounter a collision at runtime.
 *
 * This method is much faster (100-150%) than a serial scan lookup: while
 * finding an icon (with ext_table_lookup()) takes a single iteration most of
 * the time (~20 at most), it takes dozens and even 100 or more using the
 * serial scan method. */
static void
ext_table_init(void)
{
	if (ext_table)
		return;

	const size_t n = sizeof(icon_ext) / sizeof(icon_ext[0]);
	if (n == 0)
		return;

	const size_t needed = (size_t)((double)n / TABLE_LOAD_FACTOR) + 1;
	size_t table_size = next_pow2(needed);

	/* Ensure table_size >= n+1 to guarantee at least one empty slot */
	if (table_size <= n) /* cppcheck-suppress knownConditionTrueFalse */
		table_size = next_pow2(n + 1);

	ext_table_size = table_size;
	ext_table_mask = table_size - 1;

	ext_table = xnmalloc(table_size, sizeof(size_t));

	for (size_t i = 0; i < table_size; i++)
		ext_table[i] = SIZE_MAX;

	for (size_t i = 0; i < n; i++) {
		size_t h = ext_icon_hashes[i];
		/* Mix, then mask */
		size_t idx = (h * (size_t)HASH_MULTIPLIER) & ext_table_mask;
		while (ext_table[idx] != SIZE_MAX)
			idx = (idx + 1) & ext_table_mask;
		ext_table[idx] = i;
	}
}

/* Fast lookup: return the index into icon_ext for the file extension whose
 * hash is EXT_HASH, or SIZE_MAX if not found. */
static inline size_t
ext_table_lookup(size_t ext_hash)
{
	if (!ext_table || ext_table_size == 0)
		return SIZE_MAX;

	size_t idx = (ext_hash * (size_t)HASH_MULTIPLIER) & ext_table_mask;

	for (size_t i = 0; i < ext_table_size; i++) {
		size_t val = ext_table[idx];
		if (val == SIZE_MAX)
			return SIZE_MAX; /* Not found */

		if (ext_icon_hashes[val] == ext_hash)
			return val;

		idx = (idx + 1) & ext_table_mask;
	}

	return SIZE_MAX; /* Table exhausted. Not found. */
}

/* Set the icon and color fields of the file_info[N] struct to the corresponding
 * icon for EXT. If not found, set the default icon and color. */
static void
get_ext_icon(const char *restrict ext, const filesn_t n)
{
	if (!file_info[n].icon) {
		file_info[n].icon = DEF_FILE_ICON;
		file_info[n].icon_color = DEF_FILE_ICON_COLOR;
	}

	if (!ext || !*(++ext))
		return;

	const size_t ext_hash = hashme(ext, 0);

	const size_t i = ext_table_lookup(ext_hash);
	if (i != SIZE_MAX) {
		file_info[n].icon = icon_ext[i].icon;
		file_info[n].icon_color = icon_ext[i].color;
	}
}

void
init_icons_hashes(void)
{
	set_icon_name_hashes();
	set_dir_name_hashes();

	set_ext_name_hashes();
	ext_table_init();
}
#endif /* !_NO_ICONS */

#if defined(TOURBIN_QSORT)
static inline void
swap_ent(const size_t id1, const size_t id2)
{
    struct fileinfo tmp = file_info[id1];
    file_info[id1] = file_info[id2];
    file_info[id2] = tmp;
}
#endif /* TOURBIN_QSORT */

/* Return 1 if NAME contains at least one non-ASCII/control character, or 0
 * otherwise. BYTES is updated to the number of bytes needed to read the
 * entire name (excluding the terminating NUL char). EXT_INDEX, if not NULL,
 * is updated to the index of the last dot character in NAME, provided it is
 * neither the first nor the last character in NAME (we assume this is a
 * file extension).
 *
 * This check is performed over filenames to be listed. If the filename is
 * pure ASCII, we get its visible length from BYTES, instead of running
 * wc_xstrlen(). This gives us a little performance improvement: 3% faster
 * over 100,000 files. */
static uint8_t
is_utf8_name(const char *filename, size_t *bytes, size_t *ext_index)
{
	static const uint8_t utf8_chars[256] = {
		/* 0x00 - 0x1F (Control chars) */
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 0x00 - 0x0F */
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 0x10 - 0x1F */
		/* 0x20 - 0x7E (ASCII chars) */
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x20 - 0x2F */
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x30 - 0x3F */
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x40 - 0x4F */
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x50 - 0x5F */
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x60 - 0x6F */
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,	 /* 0x70 - 0x7E */
		1, /* 0x7F (DEL) */
		/* 0x80 - 0xFF (non-ASCII chars) */
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 0x80-0x8F */
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 0x90-0x9F */
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 0xA0-0xAF */
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 0xB0-0xBF */
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 0xC0-0xCF */
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 0xD0-0xDF */
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 0xE0-0xEF */
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1  /* 0xF0-0xFF */
	};

	uint8_t is_utf8 = 0;
	const unsigned char *start = (const unsigned char *)filename;
	const unsigned char *name = start;
	const unsigned char *ext = NULL;

	while (*name) {
		if (utf8_chars[*name]) {
			is_utf8 = 1;
		} else {
			if (*name == '.')
				ext = name;
		}

		name++;
	}

	if (ext && ext != start && ext[1] && ext_index)
		*ext_index = (size_t)(ext - start);

	if (bytes)
		*bytes = (size_t)(name - start);

	return is_utf8;
}

/* Return the number of ASCII/UTF-8 characters in the string S. */
static size_t
count_utf8_chars(const char *s)
{
	size_t count = 0;

	for (; *s; s++)
		count += !IS_UTF8_CONT_BYTE(*s);

	return count;
}

/* Set the color of the dividing line: DL, if the color code is set,
 * or the color of the current workspace if not. */
static void
set_div_line_color(void)
{
	if (*dl_c) {
		fputs(dl_c, stdout);
		return;
	}

	const char *def_color = term_caps.color >= 256 ? DEF_DL_C256 : DEF_DL_C;

	/* If div line color isn't set, use the current workspace color. */
	switch (cur_ws) {
	case 0: fputs(*ws1_c ? ws1_c : def_color, stdout); break;
	case 1: fputs(*ws2_c ? ws2_c : def_color, stdout); break;
	case 2: fputs(*ws3_c ? ws3_c : def_color, stdout); break;
	case 3: fputs(*ws4_c ? ws4_c : def_color, stdout); break;
	case 4: fputs(*ws5_c ? ws5_c : def_color, stdout); break;
	case 5: fputs(*ws6_c ? ws6_c : def_color, stdout); break;
	case 6: fputs(*ws7_c ? ws7_c : def_color, stdout); break;
	case 7: fputs(*ws8_c ? ws8_c : def_color, stdout); break;
	default: fputs(def_color, stdout); break;
	}
}

static inline void
print_box_drawing_line(void)
{
	fputs("\x1b(0m", stdout);

	int i;
	const int cols = (int)term_cols - 2;

	for (i = 0; i < cols; i++)
		putchar('q');

	fputs("\x1b(0j\x1b(B", stdout);
	putchar('\n');
}

static inline void
print_extended_line(void)
{
	const size_t c = count_utf8_chars(div_line);
	if (c > 1) {
		puts(div_line);
		return;
	}

	const char *dl = (*div_line == '-' && !div_line[1]
		&& term_caps.unicode == 1) ? DEF_DIV_LINE_U : div_line;

	/* Extend DIV_LINE to the end of the screen - 1.
	 * We subtract 1 to prevent an extra empty line after the
	 * dividing line on some terminals (e.g. cons25). */
	const size_t len = !dl[1] ? 1 : wc_xstrlen(dl);
	int cols = c > 0 ? (int)(term_cols / (len > 0 ? len : 1)) : 0;

	for (; cols > 1; cols--)
		fputs(dl, stdout);

	putchar('\n');
}

/* Print the line dividing files and prompt using DIV_LINE.
 * If DIV_LINE is unset, draw a line using box-drawing characters.
 * If it contains exactly one character, print DIV_LINE up to the
 * right screen edge.
 * If it contains more than one character, print exactly the
 * content of DIV_LINE.
 * If it is "0", print an empty line. */
static void
print_div_line(void)
{
#ifdef RUN_CMD
	if (cmd_line_cmd)
		return;
#endif /* RUN_CMD */

	if (conf.colorize == 1)
		set_div_line_color();

	if (!*div_line)
		print_box_drawing_line();
	else if (*div_line == '0' && !div_line[1])
		putchar('\n'); /* Empty line. */
	else
		print_extended_line();

	fputs(df_c, stdout);
	fflush(stdout);
}

#ifdef LINUX_FSINFO
static char *
get_devname(const char *file)
{
	struct stat b;
	if (stat(file, &b) == -1)
		return DEV_NO_NAME;

#if defined(__CYGWIN__) || defined(__ANDROID__)
	/* There's no sys filesystem on Cygwin/Termux (used by get_dev_name()),
	 * so let's try with the proc filesystem. */
	return get_dev_name_mntent(file);
#else
	if (major(b.st_dev) == 0)
		return get_dev_name_mntent(file);

	return get_dev_name(b.st_dev);
#endif /* __CYGWIN__ || __ANDROID__ */
}
#endif /* LINUX_FSINFO */

/* Print free/total space for the filesystem where the current directory
 * resides, plus device name and filesystem type name if available. */
static void
print_disk_usage(void)
{
	if (!workspaces || !workspaces[cur_ws].path || !*workspaces[cur_ws].path)
		return;

	struct statvfs a;
	if (statvfs(workspaces[cur_ws].path, &a) != FUNC_SUCCESS) {
		err('w', PRINT_PROMPT, "statvfs: %s\n", strerror(errno));
		return;
	}

	const off_t free_s = (off_t)a.f_bavail * (off_t)a.f_frsize;
	const off_t total = (off_t)a.f_blocks * (off_t)a.f_frsize;
/*	if (total == 0) return; // This is what MC does */

	char *p_free_space = construct_human_size(free_s);
	char *free_space = savestring(p_free_space, strlen(p_free_space));
	char *size = construct_human_size(total);

	const int free_percentage = (int)((free_s * 100) / (total > 0 ? total : 1));

	char *devname = NULL;
	char *fstype = NULL;

#ifdef _BE_POSIX
	fstype = DEV_NO_NAME;
	devname = DEV_NO_NAME;
#elif defined(__NetBSD__)
	fstype = a.f_fstypename;
	devname = a.f_mntfromname;
#elif defined(__sun)
	fstype = a.f_basetype;
	devname = get_dev_mountpoint(workspaces[cur_ws].path);
#elif defined(LINUX_FSINFO)
	int remote = 0;
	fstype = get_fs_type_name(workspaces[cur_ws].path, &remote);
	devname = get_devname(workspaces[cur_ws].path);
#elif defined(HAVE_STATFS)
	get_dev_info(workspaces[cur_ws].path, &devname, &fstype);
#else
	fstype = DEV_NO_NAME;
	devname = DEV_NO_NAME;
#endif /* _BE_POSIX */

	print_reload_msg(NULL, NULL, _("%d%% free (%s/%s) %s %s\n"),
		free_percentage, free_space ? free_space : "?", size ? size : "?",
		fstype, devname);

/* NOTE: If the f_blocks, f_bfree, f_files, f_ffree, f_bavail, and f_favail
 * fields of the statvfs struct are all zero, the filesystem is likely
 * virtual (for example, /proc, /sys, or /dev/pts). */

	free(free_space);
}

static void
print_sel_files(const unsigned short t_rows)
{
	int limit = conf.max_printselfiles;

	if (conf.max_printselfiles == 0) {
		/* Never take more than half terminal height */
		limit = (t_rows / 2) - 4;
		/* 4 = 2 div lines, 2 prompt lines */
		if (limit <= 0)
			limit = 1;
	}

	const int int_sel_n = sel_n > INT_MAX ? INT_MAX : (int)sel_n;
	if (limit > int_sel_n)
		limit = int_sel_n;

	int i;
	for (i = 0; i < (conf.max_printselfiles != UNSET ? limit : int_sel_n)
	&& sel_elements[i].name; i++) {
		char *p = abbreviate_file_name(sel_elements[i].name);
		if (!p)
			continue;
		colors_list(p, 0, NO_PAD, PRINT_NEWLINE);
		if (p != sel_elements[i].name)
			free(p);
	}

	if (conf.max_printselfiles != UNSET && limit < int_sel_n)
		printf("... (%d/%zu)\n", i, sel_n);

	print_div_line();
}

static void
print_dirhist_map(void)
{
	const int i = dirhist_cur_index;
	if (i < 0 || i >= dirhist_total_index)
		return;

	const int pad = DIGINUM(1 + (dirhist_cur_index + 1 < dirhist_total_index
		? dirhist_cur_index + 1 : dirhist_cur_index));

	if (i > 0 && old_pwd[i - 1])
		printf("%s%*d%s %s\n", el_c, pad, i, df_c, old_pwd[i - 1]);

	printf("%s%*d%s %s%s%s\n", el_c, pad, i + 1,
		df_c, mi_c, old_pwd[i], df_c);

	if (i + 1 < dirhist_total_index && old_pwd[i + 1])
		printf("%s%*d%s %s\n", el_c, pad, i + 2, df_c, old_pwd[i + 1]);
}

static void
print_cdpath(void)
{
	if (workspaces && workspaces[cur_ws].path && *workspaces[cur_ws].path)
		print_reload_msg(NULL, NULL, "cdpath: %s\n", workspaces[cur_ws].path);

	is_cdpath = 0;
}

/* Restore the original value of long-view (taken from LONG_VIEW_BK)
 * after switching mode for the pager (according to PagerView). */
static void
restore_pager_view(void)
{
	if (long_view_bk != UNSET) {
		conf.long_view = long_view_bk;
		long_view_bk = UNSET;
	}
}

/* If running the pager, set long-view according to the value of PagerView in
 * the config file. The original value of long-view will be restored after
 * list_dir() by restore_pager_view() according to the value of LONG_VIEW_BK. */
static void
set_pager_view(const filesn_t columns_n)
{
	if (conf.pager <= 0 || conf.pager_view == PAGER_AUTO)
		return;

	const filesn_t lines = (filesn_t)term_lines - 2;
	/* This is not perfect: COLUMNS_N may be modified after this function
	 * by get_longest_per_col() in list_files_vertical(), modifying thus
	 * whether the pager will be executed or not. */
	const int pager_will_run =
		(g_files_num > ((conf.long_view == 1 || conf.pager_view == PAGER_LONG)
		? lines : (columns_n * lines)));

	if (pager_will_run == 0)
		return;

	if (conf.pager == 1 || g_files_num >= (filesn_t)conf.pager) {
		long_view_bk = conf.long_view;
		conf.long_view = (conf.pager_view == PAGER_LONG);
	}
}

static void
print_dir_cmds(void)
{
	if (!history || dir_cmds.first_cmd_in_dir > (int)current_hist_n)
		return;

	const char *ptr = term_caps.unicode ? DIR_CMD_PTR_U : DIR_CMD_PTR;
	int i = dir_cmds.first_cmd_in_dir - (dir_cmds.first_cmd_in_dir > 0 ? 1 : 0);
	for (; history[i].cmd; i++)
		printf("%s%s%s %s\n", dn_c, ptr, df_c, history[i].cmd);
}

static int
post_listing(DIR *dir, const int reset_pager, const int autocmd_ret)
{
	restore_pager_view();

	if (dir && closedir(dir) == -1)
		return FUNC_FAILURE;

	if (xargs.list_and_quit == 1)
		exit(exit_code);

	if (conf.pager_once == 0) {
		if (reset_pager == 1 && (conf.pager < 2
		|| g_files_num < (filesn_t)conf.pager))
			conf.pager = pager_bk;
	} else {
		conf.pager_once = 0;
		conf.pager = 0;
	}

	const size_t s_files = (size_t)g_files_num;

	if (pager_quit == 0 && conf.max_files != UNSET
	&& g_files_num > (filesn_t)conf.max_files)
		printf("... (%d/%zu)\n", conf.max_files, s_files);

	print_div_line();

	if (conf.dirhist_map == 1) { /* Print current, previous, and next entries */
		print_dirhist_map();
		print_div_line();
	}

	if (sel_n > 0 && conf.print_selfiles == 1)
		print_sel_files(term_lines);

	if (is_cdpath == 1)
		print_cdpath();

	if (conf.disk_usage == 1)
		print_disk_usage();

	if (sort_switch == 1) {
		print_reload_msg(NULL, NULL, _("Sorted by "));
		print_sort_method();
	}

	if (switch_cscheme == 1)
		print_reload_msg(NULL, NULL, _("Color scheme: %s%s%s\n"),
			BOLD, cur_cscheme, df_c);

	if (virtual_dir == 1)
		print_reload_msg(NULL, NULL, _("Virtual directory\n"));

	if (stats.excluded > 0)
		print_reload_msg(NULL, NULL, _("Showing %zu/%zu files\n"),
			s_files, s_files + stats.excluded);

	if (filter.str && *filter.str)
		print_reload_msg(NULL, NULL, _("Active filter: %s%s%s%s\n"),
			BOLD, filter.rev == 1 ? "!" : "", filter.str, df_c);

	if (autocmd_ret == 1 && conf.autocmd_msg != AUTOCMD_MSG_NONE
	&& conf.autocmd_msg != AUTOCMD_MSG_PROMPT)
		print_autocmd_msg();

	if (dir_changed == 1) {
		dir_cmds.first_cmd_in_dir = UNSET;
		dir_changed = 0;
	}

	if (conf.print_dir_cmds == 1 && dir_cmds.first_cmd_in_dir != UNSET)
		print_dir_cmds();

	return FUNC_SUCCESS;
}

/* A basic pager for directories containing large number of files.
 * What's missing? It only goes downwards. To go backwards, use the
 * terminal scrollback function */
static int
run_pager(const int columns_n, int *reset_pager, filesn_t *i, size_t *counter)
{
	fputs(PAGER_LABEL, stdout);

	switch (xgetchar()) {
	/* Advance one line at a time */
	case 66: /* fallthrough */ /* Down arrow */
	case 10: /* fallthrough */ /* Enter (LF) */
	case 13: /* fallthrough */ /* Enter (CR) */
	case ' ':
		break;

	/* Advance one page at a time */
	case 126: /* Page Down */
		*counter = 0;
		break;

	/* h: Print pager help */
	case '?': /* fallthrough */
	case 'h': {
		CLEAR;

		fputs(_(PAGER_HELP), stdout);
		int l = (int)term_lines - 6;
		MOVE_CURSOR_DOWN(l);
		fputs(PAGER_LABEL, stdout);

		xgetchar();
		CLEAR;

		pager_help = conf.long_view == 0;

		if (columns_n == -1) { /* Long view */
			*i = 0;
		} else { /* Normal view */
			if (conf.listing_mode == HORLIST)
				*i = 0;
			else
				return PAGER_RET_HELP;
		}

		*counter = 0;
		if (*i < 0)
			*i = 0;
	} break;

	/* Stop paging (and set a flag to reenable the pager later) */
	case 'c': /* fallthrough */
	case 'p': /* fallthrough */
	case 'Q':
		pager_bk = conf.pager; conf.pager = 0; *reset_pager = 1;
		break;

	case 'q':
		pager_bk = conf.pager; conf.pager = 0; *reset_pager = 1;
		putchar('\r');
		ERASE_TO_RIGHT;
		if (conf.long_view == 0 && conf.columned == 1
		&& conf.max_name_len != UNSET)
			MOVE_CURSOR_UP(1);
		return PAGER_RET_QUIT;

	/* If another key is pressed, go back one position.
	 * Otherwise, some filenames won't be listed.*/
	default:
		putchar('\r');
		ERASE_TO_RIGHT;
		return PAGER_RET_BACK;
	}

	putchar('\r');
	ERASE_TO_RIGHT;
	return PAGER_RET_OK;
}

static int
has_file_type_char(const filesn_t i)
{
	switch (file_info[i].type) {
	case DT_REG:  return (file_info[i].exec == 1);
	case DT_BLK:  /* fallthrough */
	case DT_CHR:  /* fallthrough */
#ifdef SOLARIS_DOORS
	case DT_DOOR: /* fallthrough */
	case DT_PORT: /* fallthrough */
#endif /* SOLARIS_DOORS */
	case DT_LNK:  /* fallthrough */
	case DT_SOCK: /* fallthrough */
	case DT_FIFO: /* fallthrough */
	case DT_UNKNOWN: return 1;
	default: return 0;
	}
}

static void
get_longest_filename(const filesn_t n, const size_t eln_len)
{
	const int conf_no_eln = conf.no_eln;
	const int checks_classify = checks.classify;
	const int conf_file_counter = conf.file_counter;
	const int conf_colorize = conf.colorize;
	const int conf_listing_mode = conf.listing_mode;
	const int conf_max_files = conf.max_files;

	const filesn_t c_max_files = (filesn_t)conf_max_files;
	filesn_t i = (conf_max_files != UNSET && c_max_files < n) ? c_max_files : n;
	filesn_t longest_index = -1;

	const size_t max = checks.min_name_trunc == 1
		? (size_t)conf.min_name_trunc : (size_t)conf.max_name_len;

	while (--i >= 0) {
		file_info[i].eln_n = conf_no_eln == 1 ? -1 : DIGINUM(i + 1);

		size_t file_len = file_info[i].len;
		if (file_len == 0) {
			/* Invalid chars found. Reconstruct and recalculate length. */
			char *wname = replace_invalid_chars(file_info[i].name);
			file_len = wname ? wc_xstrlen(wname) : 0;
			free(wname);
		}

		if (file_len > max)
			file_len = max;

		size_t total_len = eln_len + 1 + file_len;

		if (checks_classify == 1) {
			if (file_info[i].filesn > 0 && conf_file_counter == 1)
				total_len += DIGINUM(file_info[i].filesn);

			if (file_info[i].dir == 1 || (conf_colorize == 0
			&& has_file_type_char(i)))
				total_len++;
		}

		if (total_len > longest.name_len) {
			longest_index = i;
			if (conf_listing_mode == VERTLIST || conf_max_files == UNSET
			|| i < c_max_files)
				longest.name_len = total_len;
		}
	}

	if (conf.icons == 1 && conf.long_view == 0 && conf.columned == 1)
		longest.name_len += (size_t)ICON_LEN;

	/* LONGEST.FC_LEN stores the number of digits taken by the file counter of
	 * the longest filename, provided it is a directory.
	 * We use this to truncate filenames up to MAX_NAME_LEN + LONGEST.FC_LEN,
	 * so that we can make use of the space taken by the file counter.
	 * Example:
	 *    longest_dirname/13
	 *    very_long_file_na~
	 * instead of
	 *    longest_dirname/13
	 *    very_long_file_~
	 * */

	longest.fc_len = 0;
	if (longest_index >= 0 && file_info[longest_index].dir == 1
	&& file_info[longest_index].filesn > 0
	&& conf.max_name_len != UNSET && conf.file_counter == 1) {
		longest.fc_len = DIGINUM(file_info[longest_index].filesn) + 1;
		const size_t t = eln_len + (size_t)conf.max_name_len
			+ 1 + longest.fc_len;
		if (t > longest.name_len)
			longest.fc_len -= t - longest.name_len;
		if ((int)longest.fc_len < 0)
			longest.fc_len = 0;
	}
}

/* Set a few extra properties needed for long view mode */
static void
set_long_attribs(const filesn_t n, const struct stat *a)
{
	if (conf.light_mode == 1) {
		switch (prop_fields.time) {
		case PROP_TIME_ACCESS: file_info[n].ltime = a->st_atime; break;
		case PROP_TIME_CHANGE: file_info[n].ltime = a->st_ctime; break;
		case PROP_TIME_MOD: file_info[n].ltime = a->st_mtime; break; /* NOLINT */
		case PROP_TIME_BIRTH:
#ifdef ST_BTIME_LIGHT
			file_info[n].ltime = a->ST_BTIME.tv_sec; break;
#else
			file_info[n].ltime = a->st_mtime; break;
#endif /* ST_BTIME_LIGHT */
		default: file_info[n].ltime = a->st_mtime; break; /* NOLINT */
		}

		file_info[n].blocks = a->st_blocks;
		file_info[n].linkn = a->st_nlink;
		file_info[n].mode = a->st_mode;
		file_info[n].uid = a->st_uid;
		file_info[n].gid = a->st_gid;
	}

	if (conf.full_dir_size == 1 && file_info[n].dir == 1
	&& file_info[n].type == DT_DIR) {
		file_info[n].size = dir_size(file_info[n].name, 1,
			&file_info[n].du_status);
	} else {
		file_info[n].size = FILE_SIZE_PTR(a);
	}
}

/* Return a pointer to the indicator char color and update IND_CHR to the
 * corresponding indicator character for the file whose index is INDEX. */
static inline char *
get_ind_char(const filesn_t index, char **ind_chr)
{
	if (file_info[index].sel == 1) {
		*ind_chr = term_caps.unicode == 1 ? SELFILE_STR_U : SELFILE_STR;
		return li_cb;
	}

	if (file_info[index].symlink == 1 && checks.lnk_char == 1) {
		*ind_chr = term_caps.unicode == 1 ? LINK_STR_U : LINK_STR;
		return lc_c;
	}

	if (file_info[index].user_access == 0 && conf.icons == 0) {
		if ((file_info[index].type != DT_DIR && !*nf_c)
		|| (file_info[index].type == DT_DIR && !*nd_c)) {
			*ind_chr = NO_PERM_STR;
			return xf_cb;
		}
	}

	*ind_chr = " ";
	return "";
}

/* Return a struct maxes_t with the following information: the longest file
 * counter, user ID, group ID, file size, inode, and file links in the
 * current list of files.
 * This information is required to build the properties line for each entry
 * based on each field's length. */
static struct maxes_t
compute_maxes(void)
{
	struct maxes_t maxes = {0};

	filesn_t i = xargs.max_files > 0 ? (filesn_t)xargs.max_files
		: (conf.max_files > 0 ? conf.max_files : g_files_num);

	const int conf_file_counter = conf.file_counter;
	const int prop_fields_size = prop_fields.size;
	const int prop_fields_ids = prop_fields.ids;
	const int prop_fields_inode = prop_fields.inode;
	const int prop_fields_links = prop_fields.links;
	const int prop_fields_blocks = prop_fields.blocks;

	if (i > g_files_num)
		i = g_files_num;

	while (--i >= 0) {
		int t = 0;
		if (file_info[i].dir == 1 && conf_file_counter == 1) {
			t = DIGINUM_BIG(file_info[i].filesn);
			if (t > maxes.file_counter)
				maxes.file_counter = t;
		}

		if (prop_fields_size == PROP_SIZE_BYTES) {
			t = DIGINUM_BIG(file_info[i].size);
			if (t > maxes.size)
				maxes.size = t;
		} else {
			if (prop_fields_size == PROP_SIZE_HUMAN) {
				t = (int)file_info[i].human_size.len;
				if (t > maxes.size)
					maxes.size = t;
			}
		}

		if (prop_fields_ids == PROP_ID_NUM) {
			const int u = DIGINUM(file_info[i].uid);
			const int g = DIGINUM(file_info[i].gid);
			if (g > maxes.id_group)
				maxes.id_group = g;
			if (u > maxes.id_user)
				maxes.id_user = u;
		} else {
			if (prop_fields_ids == PROP_ID_NAME) {
				const int g = file_info[i].gid_i.name
					? (int)file_info[i].gid_i.namlen : DIGINUM(file_info[i].gid);
				if (g > maxes.id_group)
					maxes.id_group = g;

				const int u = file_info[i].uid_i.name
					? (int)file_info[i].uid_i.namlen : DIGINUM(file_info[i].uid);
				if (u > maxes.id_user)
					maxes.id_user = u;
			}
		}

		if (prop_fields_inode == 1) {
			t = DIGINUM(file_info[i].inode);
			if (t > maxes.inode)
				maxes.inode = t;
		}

		if (prop_fields_links == 1) {
			t = DIGINUM((unsigned int)file_info[i].linkn);
			if (t > maxes.links)
				maxes.links = t;
		}

		if (prop_fields_blocks == 1) {
			t = DIGINUM_BIG(file_info[i].blocks);
			if (t > maxes.blocks)
				maxes.blocks = t;
		}
	}


	if (conf.full_dir_size != 1 || prop_fields_size == PROP_SIZE_HUMAN)
		return maxes;

	/* If at least one directory size length equals the maximum size lenght
	 * in the current directory, and we have a du(1) error for this directory,
	 * we need to make room for the du error char (!). */
	i = g_files_num;
	while (--i >= 0) {
		if (file_info[i].du_status == 0)
			continue;

		const int t = prop_fields_size == PROP_SIZE_BYTES
			? DIGINUM_BIG(file_info[i].size)
			: (int)file_info[i].human_size.len;

		if (t == maxes.size) {
			maxes.size++;
			break;
		}
	}

	return maxes;
}

static void
print_long_mode(int *reset_pager, const int eln_len)
{
	struct maxes_t maxes = compute_maxes();

	const int have_xattr = (stats.extended > 0 && prop_fields.xattr != 0);

	/* Available space (term cols) to print the filename. */
	int space_left = (int)term_cols - (prop_fields.len + have_xattr
		+ maxes.file_counter + maxes.size + maxes.links + maxes.inode
		+ maxes.id_user + (prop_fields.no_group == 0 ? maxes.id_group : 0)
		+ maxes.blocks + (conf.icons == 1 ? ICON_LEN : 0));

	if (space_left < conf.min_name_trunc)
		space_left = conf.min_name_trunc;

	if (conf.min_name_trunc != UNSET && longest.name_len > (size_t)space_left)
		longest.name_len = (size_t)space_left;

	if (longest.name_len < (size_t)space_left)
		space_left = (int)longest.name_len;

	maxes.name = space_left + (conf.icons == 1 ? ICON_LEN : 0);
	pager_quit = pager_help = 0;

	/* Cache conf struct values for faster access. */
	const filesn_t conf_max_files = conf.max_files;
	const int conf_no_eln = conf.no_eln;

	filesn_t i;
	const filesn_t f = g_files_num; /* Cache global variable. */
	const size_t s_term_lines = term_lines > 2 ? (size_t)(term_lines - 2) : 0;
	size_t pager_counter = 0;

	for (i = 0; i < f; i++) {
		if (conf_max_files != UNSET && i == conf_max_files)
			break;

		if (conf.pager == 1 || (*reset_pager == 0 && conf.pager > 1
		&& g_files_num >= (filesn_t)conf.pager)) {
			if (pager_counter > s_term_lines) {
				const int ret = run_pager(-1, reset_pager, &i, &pager_counter);

				if (ret == PAGER_RET_QUIT) {
					pager_quit = 1;
					break;
				}

				if (ret == PAGER_RET_BACK || ret == PAGER_RET_HELP) {
					i--;
					if (ret == PAGER_RET_HELP)
						pager_counter = 0;
					continue;
				}
			}
			pager_counter++;
		}

		char *ind_chr = NULL;
		const char *ind_chr_color = get_ind_char(i, &ind_chr);

		if (conf_no_eln == 0) {
			printf("%s%*zd%s%s%s%s", el_c, eln_len, i + 1, df_c,
				ind_chr_color, ind_chr, df_c);
		} else {
			printf("%s%s%s", ind_chr_color, ind_chr, df_c);
		}

		/* Print the remaining part of the entry. */
		print_entry_props(&file_info[i], &maxes, have_xattr);
	}

	if (pager_quit == 1)
		printf("... (%zd/%zd)\n", i, g_files_num);
}

/* Return the minimal number of columns we can use for the current list
 * of files. If possible, this number will be increased later by the
 * get_longest_per_col() function. */
static size_t
get_columns(void)
{
	/* LONGEST.NAME_LEN is size_t: it will never be less than zero. */
#ifdef TIGHT_COLUMNS
	size_t n = (size_t)term_cols / (longest.name_len + COLUMNS_GAP);
#else
	size_t n = (size_t)term_cols / (longest.name_len + 1);
#endif
	/* +1 for the space between filenames. */

	/* If LONGEST.NAME_LEN is larger than the number of terminal columns,
	 * N will zero. To avoid this: */
	if (n < 1)
		n = 1;

	/* If we have only three files, we don't want four columns. */
	if (n > (size_t)g_files_num)
		n = (size_t)g_files_num > 0 ? (size_t)g_files_num : 1;

	return n;
}

static size_t
get_ext_info(const filesn_t i, int *trunc_type)
{
	*trunc_type = TRUNC_EXT;

	size_t ext_len = 0;
	size_t bytes = 0;
	const char *ext = file_info[i].ext_name;

	if (file_info[i].utf8 == 0) {
		/* Extension names are usually short. Let's call strlen(2) only
		 * when it is longer than 6 (including the initial dot). */
		ext_len = !ext[1] ? 1
			: !ext[2] ? 2
			: !ext[3] ? 3
			: !ext[4] ? 4
			: !ext[5] ? 5
			: !ext[6] ? 6
			: strlen(ext);
	} else if (is_utf8_name(ext, &bytes, NULL) == 0) {
		ext_len = bytes;
	} else {
		ext_len = wc_xstrlen(ext);
	}

	const size_t max_allowed = conf.max_name_len > 0
		? (size_t)(conf.max_name_len - 1) : 0;

	if (ext_len >= max_allowed || ext_len == 0) {
		ext_len = 0;
		*trunc_type = TRUNC_NO_EXT;
	}

	return ext_len;
}

/* Construct the filename to be displayed.
 * The filename is truncated if longer than MAX_NAMELEN (if conf.max_name_len
 * is set). */
static void *
construct_filename(const filesn_t i, struct wtrunc_t *wtrunc,
	const int max_namelen)
{
	/* When quitting the help screen of the pager, the list of files is
	 * reprinted from the first entry. Now, when printing the list for the
	 * first time the LEN field of the file_info struct is set to the displayed
	 * length, which, if the name is truncated, is the same as MAX_NAME_LEN
	 * (causing thus subsequent prints of the list to never truncate long file
	 * names, because the value is already <= MAX_NAME_LEN).
	 * Because of this, we need to recalculate the length of the current entry
	 * whenever we're coming from the pager help screen, in order to know
	 * whether we should truncate the name or not. */
	size_t namelen = pager_help == 1
		? (file_info[i].utf8 == 1 ? wc_xstrlen(file_info[i].name)
		: file_info[i].bytes) : file_info[i].len;

	char *name = file_info[i].name;

	/* file_info[i].len is zero whenever an invalid character was found in
	 * the filename. Let's recalculate the name length. */
	if (namelen == 0) {
		wtrunc->wname = replace_invalid_chars(file_info[i].name);
		namelen = file_info[i].len = wc_xstrlen(wtrunc->wname);
		name = wtrunc->wname;
	}

	if ((int)namelen <= max_namelen || conf.max_name_len == UNSET
	|| conf.long_view != 0 || g_files_num <= 1)
		return (void *)name;

	/* Let's truncate the filename (at MAX_NAMELEN, in this example, 11).
	 * A. If no file extension, just truncate at 11:  "long_filen~"
	 * B. If we have an extension, keep it:       "long_f~.ext"
	 *
	 * This is the place to implement an alternative truncation procedure, say,
	 * to truncate names at the middle, like MC does: "long_~ename"
	 * or, if we have an extension:               "long_~e.ext"
	 *
	 * We only need a function similar to get_ext_info(), say, get_suffix_info()
	 * wtrunc->trunc should be set to TRUNC_EXT
	 * ext_len to the length of the suffix (either "ename" or "e.ext", i.e. 5)
	 * file_info[i].ext_name should be a pointer to the beggining of SUFFIX
	 * in file_info[i].name (this might impact on some later operation!) */

	size_t ext_len = 0;
	/* Avoid truncating dir names by extension. */
	if (!file_info[i].ext_name || file_info[i].dir == 1)
		wtrunc->type = TRUNC_NO_EXT;
	else
		ext_len = get_ext_info(i, &wtrunc->type);

	const int trunc_len = max_namelen - 1 - (int)ext_len;

	if (file_info[i].utf8 == 1) {
		const char *name_str = wtrunc->wname ? wtrunc->wname : name;
		mbstowcs(g_wcs_name_buf, name_str, NAME_BUF_SIZE);
		wtrunc->diff = wctruncstr(g_wcs_name_buf, (size_t)trunc_len);
	} else {
		/* If not UTF-8, let's avoid wctruncstr(). It's a bit faster this way. */
		const char c = name[trunc_len];
		name[trunc_len] = '\0';
		mbstowcs(g_wcs_name_buf, name, NAME_BUF_SIZE);
		name[trunc_len] = c;
	}

	file_info[i].len = (size_t)max_namelen;

	/* At this point we have truncated name. "~" and ".ext" will be appended
	 * later by one of the print_entry functions. */

	return (void *)g_wcs_name_buf;
}

static void
print_entry_color(int *ind_char, const filesn_t i, const int pad,
	const int max_namelen)
{
	*ind_char = 0;
	const char *end_color =
		(file_info[i].dir == 1 && conf.classify == 1) ? fc_c : df_c;

	struct wtrunc_t wtrunc = (struct wtrunc_t){0};
	const void *ptr = construct_filename(i, &wtrunc, max_namelen);
	const wchar_t *w = wtrunc.type > 0 ? (const wchar_t *)ptr : NULL;
	const char *n = wtrunc.type == 0 ? (const char *)ptr : NULL;

	const char *trunc_diff = wtrunc.diff > 0 ? gen_diff_str(wtrunc.diff) : "";

	char *ind_chr = NULL;
	const char *ind_chr_color = get_ind_char(i, &ind_chr);

	const int trunc = wtrunc.type;
	const char *ext_color = trunc == TRUNC_EXT ? file_info[i].color : "";
	const char *ext_name  = trunc == TRUNC_EXT ? file_info[i].ext_name : "";

	switch (checks.list_format) {
#ifndef _NO_ICONS
	case ICONS_NO_ELN:
		if (trunc > 0) {
			printf("%s%s%s%s%s%s%s%ls%s\x1b[0m%s%c\x1b[0m%s%s%s",
				ind_chr_color, ind_chr, df_c,
				file_info[i].icon_color, file_info[i].icon,
				checks.icons_gap, file_info[i].color, w,
				trunc_diff, tt_c, TRUNC_FILE_CHR,
				ext_color, ext_name, end_color);
		} else {
			printf("%s%s%s%s%s%s%s%s%s", ind_chr_color, ind_chr, df_c,
				file_info[i].icon_color, file_info[i].icon,
				checks.icons_gap, file_info[i].color, n, end_color);
		}
		break;
	case ICONS_ELN:
		if (trunc > 0) {
			printf("%s%*jd%s%s%s%s%s%s%s%s%ls%s\x1b[0m%s%c\x1b[0m%s%s%s",
				el_c, pad, (intmax_t)i + 1, df_c, ind_chr_color, ind_chr,
				df_c, file_info[i].icon_color, file_info[i].icon,
				checks.icons_gap, file_info[i].color, w,
				trunc_diff, tt_c, TRUNC_FILE_CHR,
				ext_color, ext_name, end_color);
		} else {
			printf("%s%*jd%s%s%s%s%s%s%s%s%s%s", el_c, pad,
				(intmax_t)i + 1, df_c, ind_chr_color, ind_chr, df_c,
				file_info[i].icon_color, file_info[i].icon,
				checks.icons_gap, file_info[i].color, n, end_color);
		}
		break;
#endif /* !_NO_ICONS */
	case NO_ICONS_NO_ELN:
		if (trunc > 0) {
			printf("%s%s%s%s%ls%s\x1b[0m%s%c\x1b[0m%s%s%s", ind_chr_color,
				ind_chr, df_c, file_info[i].color, w, trunc_diff,
				tt_c, TRUNC_FILE_CHR, ext_color, ext_name, end_color);
		} else {
			printf("%s%s%s%s%s%s", ind_chr_color, ind_chr,
				df_c, file_info[i].color, n, end_color);
		}
		break;
	case NO_ICONS_ELN:
		if (trunc > 0) {
			printf("%s%*jd%s%s%s%s%s%ls%s\x1b[0m%s%c\x1b[0m%s%s%s",
				el_c, pad, (intmax_t)i + 1, df_c, ind_chr_color, ind_chr,
				df_c, file_info[i].color, w,
				trunc_diff, tt_c, TRUNC_FILE_CHR,
				ext_color, ext_name, end_color);
		} else {
			printf("%s%*jd%s%s%s%s%s%s%s", el_c, pad, (intmax_t)i + 1,
				df_c, ind_chr_color, ind_chr, df_c,
				file_info[i].color, n, end_color);
		}
		break;
	default: break;
	}

	if (end_color == fc_c) {
		/* We have a directory and classification is on: append directory
		 * indicator and file counter. */
		putchar(DIR_CHR);
		if (file_info[i].filesn > 0 && conf.file_counter == 1)
			fputs(xitoa(file_info[i].filesn), stdout);
		fputs(df_c, stdout);
	}

	if (wtrunc.wname) /* This is NULL most of the time. */
		free(wtrunc.wname);
}

static void
print_entry_nocolor(int *ind_char, const filesn_t i, const int pad,
	const int max_namelen)
{
	struct wtrunc_t wtrunc = (struct wtrunc_t){0};
	const void *ptr = construct_filename(i, &wtrunc, max_namelen);
	const wchar_t *w = wtrunc.type > 0 ? (const wchar_t *)ptr : NULL;
	const char *n = wtrunc.type == 0 ? (const char *)ptr : NULL;

	const char *trunc_diff = wtrunc.diff > 0 ? gen_diff_str(wtrunc.diff) : "";
	char *ind_chr = NULL;
	(void)get_ind_char(i, &ind_chr);

	switch (checks.list_format) {
#ifndef _NO_ICONS
	case ICONS_NO_ELN:
		if (wtrunc.type > 0) {
			printf("%s%s%s%ls%s%c%s", ind_chr, file_info[i].icon,
				checks.icons_gap, w, trunc_diff,
				TRUNC_FILE_CHR, wtrunc.type == TRUNC_EXT
				? file_info[i].ext_name : "");
		} else {
			printf("%s%s%s%s", ind_chr, file_info[i].icon,
				checks.icons_gap, n);
		}
		break;
	case ICONS_ELN:
		if (wtrunc.type > 0) {
			printf("%s%*jd%s%s%s%s%ls%s%c%s", el_c, pad, (intmax_t)i + 1,
				df_c, ind_chr, file_info[i].icon, checks.icons_gap,
				w, trunc_diff, TRUNC_FILE_CHR,
				wtrunc.type == TRUNC_EXT ? file_info[i].ext_name : "");
		} else {
			printf("%s%*jd%s%s%s%s%s", el_c, pad, (intmax_t)i + 1, df_c,
				ind_chr, file_info[i].icon,	checks.icons_gap, n);
		}
		break;
#endif /* !_NO_ICONS */
	case NO_ICONS_NO_ELN:
		if (wtrunc.type > 0) {
			printf("%s%ls%s%c%s", ind_chr, w,
				trunc_diff, TRUNC_FILE_CHR,
				wtrunc.type == TRUNC_EXT ? file_info[i].ext_name : "");
		} else {
			printf("%s%s", ind_chr, n);
		}
		break;
	case NO_ICONS_ELN:
		if (wtrunc.type > 0) {
			printf("%s%*jd%s%s%ls%s%c%s", el_c, pad, (intmax_t)i + 1,
				df_c, ind_chr, w, trunc_diff, TRUNC_FILE_CHR,
				wtrunc.type == TRUNC_EXT ? file_info[i].ext_name : "");
		} else {
			printf("%s%*jd%s%s%s", el_c, pad, (intmax_t)i + 1,
				df_c, ind_chr, n);
		}
		break;
	default: break;
	}

	if (conf.classify == 1) {
		/* Append file type indicator */
		switch (file_info[i].type) {
		case DT_DIR:
			*ind_char = 0;
			putchar(DIR_CHR);
			if (file_info[i].filesn > 0 && conf.file_counter == 1)
				fputs(xitoa(file_info[i].filesn), stdout);
			break;

		case DT_LNK:
			if (file_info[i].color == or_c) {
				putchar(BRK_LNK_CHR);
			} else if (file_info[i].dir == 1) {
				*ind_char = 0;
				putchar(DIR_CHR);
				if (file_info[i].filesn > 0 && conf.file_counter == 1)
					fputs(xitoa(file_info[i].filesn), stdout);
			} else {
				putchar(LINK_CHR);
			}
			break;

		case DT_REG:
			if (file_info[i].exec == 1)
				putchar(EXEC_CHR);
			else
				*ind_char = 0;
			break;

		case DT_BLK: putchar(BLK_CHR); break;
		case DT_CHR: putchar(CHR_CHR); break;
#ifdef SOLARIS_DOORS
		case DT_DOOR: putchar(DOOR_CHR); break;
//		case DT_PORT: break;
#endif /* SOLARIS_DOORS */
		case DT_FIFO: putchar(FIFO_CHR); break;
		case DT_SOCK: putchar(SOCK_CHR); break;
#ifdef S_IFWHT
		case DT_WHT: putchar(WHT_CHR); break;
#endif /* S_IFWHT */
		case DT_UNKNOWN: putchar(UNK_CHR); break;
		default: *ind_char = 0;
		}
	}

	if (wtrunc.wname) /* This is NULL most of the time. */
		free(wtrunc.wname);
}

static void
print_entry_color_light(int *ind_char, const filesn_t i,
	const int pad, const int max_namelen)
{
	*ind_char = 0;
	const char *end_color = file_info[i].dir == 1 ? fc_c : df_c;

	struct wtrunc_t wtrunc = (struct wtrunc_t){0};
	const void *ptr = construct_filename(i, &wtrunc, max_namelen);
	const wchar_t *w = wtrunc.type > 0 ? (const wchar_t *)ptr : NULL;
	const char *n = wtrunc.type == 0 ? (const char *)ptr : NULL;

	const char *trunc_diff = wtrunc.diff > 0 ? gen_diff_str(wtrunc.diff) : "";

	const int trunc = wtrunc.type;
	const char *ext_color = trunc == TRUNC_EXT ? file_info[i].color : "";
	const char *ext_name  = trunc == TRUNC_EXT ? file_info[i].ext_name : "";

	switch (checks.list_format) {
#ifndef _NO_ICONS
	case ICONS_NO_ELN:
		if (trunc > 0) {
			printf("%s%s%s%s%ls%s\x1b[0m%s%c\x1b[0m%s%s%s",
				file_info[i].icon_color, file_info[i].icon,
				checks.icons_gap, file_info[i].color, w,
				trunc_diff, tt_c, TRUNC_FILE_CHR,
				ext_color, ext_name, end_color);
		} else {
			printf("%s%s%s%s%s%s", file_info[i].icon_color,
				file_info[i].icon, checks.icons_gap,
				file_info[i].color, n, end_color);
		}
		break;
	case ICONS_ELN:
		if (trunc > 0) {
			printf("%s%*jd%s %s%s%s%s%ls%s\x1b[0m%s%c\x1b[0m%s%s%s",
				el_c, pad, (intmax_t)i + 1, df_c, file_info[i].icon_color,
				file_info[i].icon, checks.icons_gap, file_info[i].color,
				w, trunc_diff, tt_c, TRUNC_FILE_CHR,
				ext_color, ext_name, end_color);
		} else {
			printf("%s%*jd%s %s%s%s%s%s%s", el_c, pad, (intmax_t)i + 1,
				df_c, file_info[i].icon_color, file_info[i].icon,
				checks.icons_gap, file_info[i].color, n, end_color);
		}
		break;
#endif /* !_NO_ICONS */
	case NO_ICONS_NO_ELN:
		if (trunc > 0) {
			printf("%s%ls%s\x1b[0m%s%c\x1b[0m%s%s%s",
				file_info[i].color, w,
				trunc_diff, tt_c, TRUNC_FILE_CHR,
				ext_color, ext_name, end_color);
		} else {
			printf("%s%s%s", file_info[i].color, n, end_color);
		}
		break;
	case NO_ICONS_ELN:
		if (trunc > 0) {
			printf("%s%*jd%s %s%ls%s\x1b[0m%s%c\x1b[0m%s%s%s",
				el_c, pad, (intmax_t)i + 1, df_c, file_info[i].color,
				w, trunc_diff, tt_c, TRUNC_FILE_CHR,
				ext_color, ext_name, end_color);
		} else {
			printf("%s%*jd%s %s%s%s", el_c, pad, (intmax_t)i + 1, df_c,
				file_info[i].color, n, end_color);
		}
		break;
	default: break;
	}

	if (file_info[i].dir == 1 && conf.classify == 1) {
		putchar(DIR_CHR);
		if (file_info[i].filesn > 0 && conf.file_counter == 1)
			fputs(xitoa(file_info[i].filesn), stdout);
	}

	if (end_color == fc_c)
		fputs(df_c, stdout);

	if (wtrunc.wname) /* This is NULL most of the time. */
		free(wtrunc.wname);
}

static void
print_entry_nocolor_light(int *ind_char, const filesn_t i,
	const int pad, const int max_namelen)
{
	struct wtrunc_t wtrunc = (struct wtrunc_t){0};
	const void *ptr = construct_filename(i, &wtrunc, max_namelen);
	const wchar_t *w = wtrunc.type > 0 ? (const wchar_t *)ptr : NULL;
	const char *n = wtrunc.type == 0 ? (const char *)ptr : NULL;

	const char *trunc_diff = wtrunc.diff > 0 ? gen_diff_str(wtrunc.diff) : "";

	switch (checks.list_format) {
#ifndef _NO_ICONS
	case ICONS_NO_ELN:
		if (wtrunc.type > 0) {
			printf("%s%s%ls%s%c%s", file_info[i].icon,
				checks.icons_gap, w, trunc_diff, TRUNC_FILE_CHR,
				wtrunc.type == TRUNC_EXT ? file_info[i].ext_name : "");
		} else {
			printf("%s%s%s", file_info[i].icon, checks.icons_gap, n);
		}
		break;
	case ICONS_ELN:
		if (wtrunc.type > 0) {
			printf("%s%*jd%s %s%s%ls%s%c%s", el_c, pad, (intmax_t)i + 1,
				df_c, file_info[i].icon, checks.icons_gap, w,
				trunc_diff, TRUNC_FILE_CHR, wtrunc.type == TRUNC_EXT
				? file_info[i].ext_name : "");
		} else {
			printf("%s%*jd%s %s%s%s", el_c, pad, (intmax_t)i + 1, df_c,
				file_info[i].icon, checks.icons_gap, n);
		}
		break;
#endif /* !_NO_ICONS */
	case NO_ICONS_NO_ELN:
		if (wtrunc.type > 0) {
			printf("%ls%s%c%s", w, trunc_diff, TRUNC_FILE_CHR,
				wtrunc.type == TRUNC_EXT ? file_info[i].ext_name : "");
		} else {
			fputs(file_info[i].name, stdout);
		}
		break;
	case NO_ICONS_ELN:
		if (wtrunc.type > 0) {
			printf("%s%*jd%s %ls%s%c%s", el_c, pad, (intmax_t)i + 1,
				df_c, w, trunc_diff, TRUNC_FILE_CHR,
				wtrunc.type == TRUNC_EXT ? file_info[i].ext_name : "");
		} else {
			printf("%s%*jd%s %s", el_c, pad, (intmax_t)i + 1, df_c, n);
		}
		break;
	default: break;
	}

	if (conf.classify == 1) {
		switch (file_info[i].type) {
		case DT_DIR:
			*ind_char = 0;
			putchar(DIR_CHR);
			if (file_info[i].filesn > 0 && conf.file_counter == 1)
				fputs(xitoa(file_info[i].filesn), stdout);
			break;

		case DT_BLK: putchar(BLK_CHR); break;
		case DT_CHR: putchar(CHR_CHR); break;
#ifdef SOLARIS_DOORS
		case DT_DOOR: putchar(DOOR_CHR); break;
//		case DT_DOOR: break;
#endif /* SOLARIS_DOORS */
		case DT_FIFO: putchar(FIFO_CHR); break;
		case DT_LNK: putchar(LINK_CHR); break;
		case DT_SOCK: putchar(SOCK_CHR); break;
#ifdef S_IFWHT
		case DT_WHT: putchar(WHT_CHR); break;
#endif /* S_IFWHT */
		case DT_UNKNOWN: putchar(UNKNOWN_CHR); break;
		default: *ind_char = 0; break;
		}
	}

	if (wtrunc.wname) /* This is NULL most of the time. */
		free(wtrunc.wname);
}

#ifdef TIGHT_COLUMNS
static size_t
calc_item_length(const int eln_len, const int icon_len, const filesn_t i)
{
	size_t file_len = file_info[i].len;
	if (file_len == 0) {
		/* Invalid chars found. Reconstruct and recalculate length. */
		char *wname = replace_invalid_chars(file_info[i].name);
		file_len = wname ? wc_xstrlen(wname) : 0;
		free(wname);
	}

	const size_t max_namelen = (size_t)conf.max_name_len
		+ (file_info[i].dir != 1 ? longest.fc_len : 0);

	const size_t name_len = (max_namelen > 0 && file_len > max_namelen)
		? max_namelen : file_len;

	int item_len = eln_len + 1 + (int)name_len + icon_len;

	if (conf.classify != 1)
		return (size_t)item_len;

	if (file_info[i].dir == 1) {
		item_len++;
		if (file_info[i].filesn > 0 && conf.file_counter == 1
		&& file_info[i].user_access == 1)
			item_len += DIGINUM((int)file_info[i].filesn);
	} else {
		if (conf.colorize == 0 && has_file_type_char(i) == 1)
			item_len++;
	}

	return (size_t)item_len;
}

static size_t *
get_longest_per_col(size_t *columns_n, filesn_t *rows, const filesn_t files_n)
{
	if (conf.columned == 0) {
		*columns_n = 1;
		*rows = g_files_num;
		size_t *longest_per_col = xnmalloc(2, sizeof(size_t));
		longest_per_col[0] = term_cols;
		return longest_per_col;
	}

	size_t used_cols = 0;
	size_t longest_index = 0;

	if (*columns_n == 0)
		*columns_n = 1;
	if (*rows <= 0)
		*rows = 1;

	/* Make enough room to hold columns information. We'll never get more
	 * columns for the current file list than the number of terminal columns. */
	size_t *longest_per_col = xnmalloc((size_t)term_cols + 1, sizeof(size_t));

	/* Hold info about the previous columns state */
	size_t *prev_longest_per_col = NULL;
	filesn_t prev_rows = *rows;

	const int longest_eln = conf.no_eln != 1 ? DIGINUM(files_n + 1) : 1;
	const int icon_len = (conf.icons == 1 ? ICON_LEN : 0);

#define LONGEST_PLUS_GAP (longest_per_col[longest_index] + COLUMNS_GAP)

	while (1) {
		/* Calculate the number of rows that will be in each column except
		 * possibly for a short column on the right. */
		*rows = files_n / (filesn_t)*columns_n
			+ (files_n % (filesn_t)*columns_n != 0);

		/* Get longest filename per column */
		filesn_t i;
		filesn_t counter = 1;
		size_t longest_name_len = 0;

		/* Cache the value referenced by the pointer once here instead of
		 * dereferencing it hundreds of times in the below for-loop. */
		const filesn_t cached_rows = *rows;

		for (i = 0; i < files_n; i++) {
			size_t len = 0;
			if (file_info[i].total_entry_len > 0) {
				len = file_info[i].total_entry_len;
			} else {
				len = file_info[i].total_entry_len =
					calc_item_length(longest_eln, icon_len, i);
			}

			if (len > longest_name_len)
				longest_name_len = len;

			if (counter == cached_rows) {
				counter = 1;
				longest_per_col[longest_index] = longest_name_len;
				used_cols += LONGEST_PLUS_GAP;
				longest_index++;
				longest_name_len = 0;
			} else {
				counter++;
			}
		}

		/* Count last column as well: If the number of files in the last
		 * column is less than the number of rows (in which case
		 * LONGEST_NAME_LEN is bigger than zero), the longest name in this
		 * column isn't taken into account: let's do it here. */
		if (longest_name_len > 0) {
			longest_per_col[longest_index] = longest_name_len;
			used_cols += LONGEST_PLUS_GAP;
		} else {
			if (longest_index > 0)
				longest_index--;
			else
				break;
		}

		const int rest = (int)term_cols - (int)used_cols;

		if ((*rows == 1 && (filesn_t)*columns_n + 1 >= files_n)
		|| rest < (int)LONGEST_PLUS_GAP) {
			if (rest < 0 && *columns_n > 1) {
				if (!prev_longest_per_col)
					return longest_per_col;

				(*columns_n)--;
				*rows = prev_rows;
				free(longest_per_col);
				return prev_longest_per_col;
			} else {
				break;
			}
		}

		/* Keep a copy of the current state, in case the next iteration
		 * returns too many columns, in which case the current state is
		 * what we want. */
		prev_rows = *rows;
		free(prev_longest_per_col);
		prev_longest_per_col = xnmalloc(*columns_n + 1, sizeof(size_t));
		memcpy(prev_longest_per_col, longest_per_col,
			(*columns_n + 1) * sizeof(size_t));

		/* There is space left for one more column: increase columns_n. */
		(*columns_n)++;
		longest_index = 0;
		used_cols = 0;
	}

#undef LONGEST_PLUS_GAP

	free(prev_longest_per_col);
	return longest_per_col;
}

static void
pad_filename_new(const filesn_t i, const int termcap_move_right,
	const size_t longest_in_col)
{
	const int diff = ((int)longest_in_col + COLUMNS_GAP)
		- ((int)file_info[i].total_entry_len + (conf.no_eln == 1));

	if (termcap_move_right == 1) {
		MOVE_CURSOR_RIGHT(diff);
	} else {
		int j = diff;
		while (--j >= 0)
			putchar(' ');
	}
}
#endif /* TIGHT_COLUMNS */

/* Right pad the current filename (adding spaces) to equate the longest
 * filename length. */
static void
pad_filename(const int ind_char, const filesn_t i, const int eln_len,
	const int termcap_move_right)
{
	int cur_len =  eln_len + 1 + (conf.icons == 1 ? ICON_LEN : 0)
		+ (int)file_info[i].len + (ind_char ? 1 : 0);

	if (file_info[i].dir == 1 && conf.classify == 1) {
		cur_len++;
		if (file_info[i].filesn > 0 && conf.file_counter == 1
		&& file_info[i].user_access == 1)
			cur_len += DIGINUM((int)file_info[i].filesn);
	}

	const int diff = (int)longest.name_len - cur_len;
	if (termcap_move_right == 1) {
		MOVE_CURSOR_RIGHT(diff + 1);
	} else {
		int j = diff + 1;
		while (--j >= 0)
			putchar(' ');
	}
}

/* List files horizontally:
 * 1 AAA	2 AAB	3 AAC
 * 4 AAD	5 AAE	6 AAF */
static void
list_files_horizontal(int *reset_pager, const int eln_len, size_t columns_n)
{
	const filesn_t nn = (conf.max_files != UNSET
		&& (filesn_t)conf.max_files < g_files_num)
		? (filesn_t)conf.max_files : g_files_num;

/*	size_t *longest_per_col = get_longest_per_col(&columns_n, nn);
	size_t cur_col = 0; */

	void (*print_entry_function)(int *, const filesn_t, const int, const int);
	if (conf.colorize == 1)
		print_entry_function = conf.light_mode == 1
			? print_entry_color_light : print_entry_color;
	else
		print_entry_function = conf.light_mode == 1
			? print_entry_nocolor_light : print_entry_nocolor;

	const int termcap_move_right = (xargs.list_and_quit == 1
		|| term_caps.suggestions == 0) ? 0 : 1;

	const int int_longest_fc_len = (int)longest.fc_len;
	size_t cur_cols = 0;
	filesn_t i;
	int last_column = 0;
	int backup_last_column = last_column;

	pager_quit = pager_help = 0;
	size_t pager_counter = 0;

	for (i = 0; i < nn; i++) {
		/* If current entry is in the last column, we need to print a
		 * new line char. */
		size_t bcur_cols = cur_cols;
		if (++cur_cols != columns_n) {
			last_column = 0;
		} else {
			cur_cols = 0;
			last_column = 1;
		}

		int ind_char = (conf.classify != 0);

				/* ##########################
				 * #  MAS: A SIMPLE PAGER   #
				 * ########################## */

		if (conf.pager == 1 || (*reset_pager == 0 && conf.pager > 1
		&& g_files_num >= (filesn_t)conf.pager)) {
			/* Run the pager only once all columns and rows fitting in
			 * the screen are filled with the corresponding filenames */
			int ret = 0;
			filesn_t backup_i = i;
			if (backup_last_column
			&& pager_counter > columns_n * ((size_t)term_lines - 2))
				ret = run_pager((int)columns_n, reset_pager, &i, &pager_counter);

			if (ret == PAGER_RET_QUIT) {
				pager_quit = 1;
				goto END;
			}

			if (ret == PAGER_RET_BACK) {
				i = backup_i ? backup_i - 1 : backup_i;
				cur_cols = bcur_cols;
				last_column = backup_last_column;
				continue;
			}
			pager_counter++;
		}

		backup_last_column = last_column;

			/* #################################
			 * #    PRINT THE CURRENT ENTRY    #
			 * ################################# */

		const int fc = file_info[i].dir != 1 ? int_longest_fc_len : 0;
		/* Displayed filename will be truncated to MAX_NAME_LEN. */
		const int max_namelen = conf.max_name_len + fc;

		file_info[i].eln_n = conf.no_eln == 1 ? -1 : DIGINUM(i + 1);

		print_entry_function(&ind_char, i, eln_len, max_namelen);

		if (last_column == 0) {
/*			pad_filename_new(i, termcap_move_right, longest_per_col[cur_col]);
			cur_col++; */
			pad_filename(ind_char, i, eln_len, termcap_move_right);
		} else {
			putchar('\n');
//			cur_col = 0;
		}
	}

END:
//	free(longest_per_col);
	if (last_column == 0)
		putchar('\n');
	if (pager_quit == 1)
		printf("... (%zd/%zd)\n", i, g_files_num);
}

/* List files vertically, like ls(1) would
 * 1 AAA	3 AAC	5 AAE
 * 2 AAB	4 AAD	6 AAF */
static void
list_files_vertical(int *reset_pager, const int eln_len, size_t num_columns)
{
	/* Total number of files to be listed. */
	const filesn_t total_files = (conf.max_files != UNSET
		&& (filesn_t)conf.max_files < g_files_num)
		? (filesn_t)conf.max_files : g_files_num;

#ifdef TIGHT_COLUMNS
	filesn_t num_rows = 0;
	size_t *longest_per_col =
		get_longest_per_col(&num_columns, &num_rows, total_files);
	size_t cur_col = 0;
#else
	/* How many lines (rows) do we need to print TOTAL_FILES files? */
	/* Division/modulo is slow, true. But the compiler will make a much
	 * better job than us at optimizing this code. */
	/* NUM_COLUMNS is guarranteed to be >0 by get_columns() */
	filesn_t num_rows = total_files / (filesn_t)num_columns;
	if (total_files % (filesn_t)num_columns > 0)
		num_rows++;
#endif

	int last_column = 0;
	/* The previous value of LAST_COLUMN. We need this value to run the pager. */
	int backup_last_column = last_column;

	void (*print_entry_function)(int *, const filesn_t, const int, const int);
	if (conf.colorize == 1)
		print_entry_function = conf.light_mode == 1
			? print_entry_color_light : print_entry_color;
	else
		print_entry_function = conf.light_mode == 1
			? print_entry_nocolor_light : print_entry_nocolor;

	const int termcap_move_right = (xargs.list_and_quit == 1
		|| term_caps.suggestions == 0) ? 0 : 1;

	const int int_longest_fc_len = (int)longest.fc_len;
	size_t column_count = num_columns; // Current column number
	filesn_t file_index = 0; // Index of the file to be actually printed
	filesn_t row_index = 0; // Current line number
	filesn_t i = 0; // Index of the current entry being analyzed

	const int conf_max_name_len = conf.max_name_len;
	const int conf_no_eln = conf.no_eln;
	const int conf_classify = conf.classify;

	pager_quit = pager_help = 0;
	size_t pager_counter = 0;

	for ( ; ; i++) {
		/* Copy current values to restore them if necessary. */
		filesn_t backup_row_index = row_index;
		filesn_t backup_file_index = file_index;
		size_t backup_column_count = column_count;

		if (column_count != num_columns) {
			file_index += num_rows;
			column_count++;
		} else {
			file_index = row_index;
			row_index++;
			column_count = 1;
		}

		if (row_index > num_rows)
			break;

		/* If current entry is in the last column, print a new line char */
		last_column = (column_count == num_columns);

		int ind_char = (conf_classify != 0);

		if (file_index >= total_files || !file_info[file_index].name) {
			if (last_column == 1) {
				/* Last column is empty. E.g.:
				 * 1 file  3 file3  5 file5
				 * 2 file2 4 file4  HERE
				 * ... */
				putchar('\n');
#ifdef TIGHT_COLUMNS
				cur_col = 0;
#endif
			}
			continue;
		}

				/* ##########################
				 * #  MAS: A SIMPLE PAGER   #
				 * ########################## */

		if (conf.pager == 1 || (*reset_pager == 0 && conf.pager > 1
		&& g_files_num >= (filesn_t)conf.pager)) {
			int ret = 0;
			filesn_t backup_i = i;
			/* Run the pager only once all columns and rows fitting in
			 * the screen are filled with the corresponding filenames. */
			if (backup_last_column
			&& pager_counter > num_columns * ((size_t)term_lines - 2))
				ret = run_pager((int)num_columns,
					reset_pager, &file_index, &pager_counter);

			if (ret == PAGER_RET_QUIT) {
				pager_quit = 1;
				goto END;
			}

			if (ret == PAGER_RET_BACK) {
				/* Restore previous values */
				i = backup_i ? backup_i - 1: backup_i;
				file_index = backup_file_index;
				row_index = backup_row_index;
				column_count = backup_column_count;
				continue;
			} else {
				if (ret == PAGER_RET_HELP) {
					i = file_index = row_index = 0;
					last_column = backup_last_column = 0;
					pager_counter = 0;
					column_count = num_columns;
					continue;
				}
			}
			pager_counter++;
		}

		backup_last_column = last_column;

			/* #################################
			 * #    PRINT THE CURRENT ENTRY    #
			 * ################################# */

		const int fc = file_info[file_index].dir != 1 ? int_longest_fc_len : 0;
		/* Displayed filename will be truncated to MAX_NAMELEN. */
		const int max_namelen = conf_max_name_len + fc;

		file_info[file_index].eln_n = conf_no_eln == 1
			? -1 : DIGINUM(file_index + 1);

		print_entry_function(&ind_char, file_index, eln_len, max_namelen);

		if (last_column == 0) {
#ifdef TIGHT_COLUMNS
			pad_filename_new(file_index, termcap_move_right,
				longest_per_col[cur_col]);
			cur_col++;
#else
			pad_filename(ind_char, file_index, eln_len, termcap_move_right);
#endif
		} else {
			/* Last column is populated. Example:
			 * 1 file  3 file3  5 file5HERE
			 * 2 file2 4 file4  6 file6HERE
			 * ... */
			putchar('\n');
#ifdef TIGHT_COLUMNS
			cur_col = 0;
#endif
		}
	}

END:
#ifdef TIGHT_COLUMNS
	free(longest_per_col);
#endif
	if (last_column == 0)
		putchar('\n');
	if (pager_quit == 1)
		printf("... (%zd/%zd)\n", i, g_files_num);
}

/* Execute commands in either AUTOCMD_DIR_IN_FILE or AUTOCMD_DIR_OUT_FILE files.
 * MODE (either AUTOCMD_DIR_IN or AUTOCMD_DIR_OUT) tells the function
 * whether to check for AUTOCMD_DIR_IN_FILE or AUTOCMD_DIR_OUT_FILE files.
 * DIR holds the absolute path to the .cfm.in file whenever we come from
 * check_autocmd_in_file(). Otherwise, it must be NULL. */
static void
run_dir_cmd(const int mode, const char *dir)
{
	char dpath[PATH_MAX + 1];
	const char *path = NULL;

	if (mode == AUTOCMD_DIR_IN) {
		if (!dir || !*dir)
			return;
		path = dir;
	} else { /* AUTOCMD_DIR_OUT */
		if (dirhist_cur_index <= 0 || !old_pwd
		|| !old_pwd[dirhist_cur_index - 1])
			return;
		snprintf(dpath, sizeof(dpath), "%s/%s",
			old_pwd[dirhist_cur_index - 1], AUTOCMD_DIR_OUT_FILE);
		path = dpath;
	}

	int fd = -1;
	FILE *fp = open_fread(path, &fd);
	if (!fp)
		return;

	char buf[PATH_MAX + 1];
	*buf = '\0';
	const char *ret = fgets(buf, sizeof(buf), fp);
	size_t buf_len = *buf ? strnlen(buf, sizeof(buf)) : 0;
	if (buf_len > 0 && buf[buf_len - 1] == '\n') {
		buf[buf_len - 1] = '\0';
		buf_len--;
	}

	if (!ret || buf_len == 0 || memchr(buf, '\0', buf_len)) {
		/* Empty line, or it contains a NUL byte (probably binary): reject it. */
		fclose(fp);
		return;
	}

	fclose(fp);

	if (xargs.secure_cmds == 0
	|| sanitize_cmd(buf, SNT_AUTOCMD) == FUNC_SUCCESS)
		launch_execl(buf);
}

/* If the file at offset I is largest than SIZE, store information about this
 * file in NAME and COLOR, and update SIZE to the size of the new largest file.
 * Also, TOTAL is increased with the size of the file under analysis. */
static void
get_largest_file_info(const filesn_t i, off_t *size, char **name,
	char **color, off_t *total)
{
	/* Only directories and regular files should be counted. */
	if (file_info[i].type != DT_DIR && file_info[i].type != DT_REG
	&& (file_info[i].type != DT_LNK || conf.apparent_size != 1))
		return;

	if (file_info[i].size > *size) {
		*size = file_info[i].size;
		*name = file_info[i].name;
		*color = file_info[i].color;
	}

	/* Do not recount hardlinks in the same directory. */
	if (file_info[i].linkn > 1 && i > 0) {
		filesn_t j = i;
		while (--j >= 0) {
			if (file_info[i].inode == file_info[j].inode)
				return;
		}
	}

	*total += file_info[i].size;
}

static int
exclude_file_type_light(const unsigned char type)
{
	if (!filter.str[1])
		return FUNC_FAILURE;

	int match = 0;

	switch (filter.str[1]) {
	case 'd': if (type == DT_DIR)  match = 1; break;
	case 'f': if (type == DT_REG)  match = 1; break;
	case 'l': if (type == DT_LNK)  match = 1; break;
	case 's': if (type == DT_SOCK) match = 1; break;
	case 'c': if (type == DT_CHR)  match = 1; break;
	case 'b': if (type == DT_BLK)  match = 1; break;
	case 'p': if (type == DT_FIFO) match = 1; break;
#ifdef SOLARIS_DOORS
	case 'O': if (type == DT_DOOR) match = 1; break;
	case 'P': if (type == DT_PORT) match = 1; break;
#endif /* SOLARIS_DOORS */
	default: return FUNC_FAILURE;
	}

	if (match == 1)
		return filter.rev == 1 ? FUNC_SUCCESS : FUNC_FAILURE;
	else
		return filter.rev == 1 ? FUNC_FAILURE : FUNC_SUCCESS;
}

/* Returns FUNC_SUCCESS if the file with mode MODE and LINKS number
 * of links must be excluded from the file list, or FUNC_FAILURE. */
static int
exclude_file_type(const char *restrict name, const mode_t mode,
	const nlink_t links, const off_t size)
{
/* ADD: C = Files with capabilities */

	if (!filter.str[1])
		return FUNC_FAILURE;

	struct stat a;
	int match = 0;

	switch (filter.str[1]) {
	case 'b': if (S_ISBLK(mode)) match = 1; break;
	case 'd': if (S_ISDIR(mode)) match = 1; break;
	case 'D':
		if (S_ISDIR(mode) && links <= 2 && count_dir(name, CPOP) <= 2)
			match = 1;
		break;
	case 'c': if (S_ISCHR(mode)) match = 1; break;
	case 'f': if (S_ISREG(mode)) match = 1; break;
	case 'F': if (S_ISREG(mode) && size == 0) match = 1; break;
	case 'l': if (S_ISLNK(mode)) match = 1; break;
	case 'L': if (S_ISLNK(mode) && stat(name, &a) == -1) match = 1; break;
#ifdef SOLARIS_DOORS
	case 'O': if (S_ISDOOR(mode)) match = 1; break;
	case 'P': if (S_ISPORT(mode)) match = 1; break;
#endif /* SOLARIS_DOORS */
	case 'p': if (S_ISFIFO(mode)) match = 1; break;
	case 's': if (S_ISSOCK(mode)) match = 1; break;

	case 'g': if (mode & S_ISGID) match = 1; break; /* SGID */
	case 'h': if (links > 1 && !S_ISDIR(mode)) match = 1; break;
	case 'o': if (mode & S_IWOTH) match = 1; break; /* Other writable */
	case 't': if (mode & S_ISVTX) match = 1; break; /* Sticky */
	case 'u': if (mode & S_ISUID) match = 1; break; /* SUID */
	case 'x': if (S_ISREG(mode) && IS_EXEC(mode)) match = 1; break; /* Exec */
	default: return FUNC_FAILURE;
	}

	if (match == 1)
		return filter.rev == 1 ? FUNC_SUCCESS : FUNC_FAILURE;
	else
		return filter.rev == 1 ? FUNC_FAILURE : FUNC_SUCCESS;
}

/* Initialize the file_info struct with default values. */
static void
init_default_file_info(void)
{
	static int set = 0;
	if (set == 1) /* Initialize the struct only once. */
		return;
	set = 1;

	default_file_info = (struct fileinfo){0};
	default_file_info.color = df_c;
#ifdef _NO_ICONS
	default_file_info.icon_color = df_c;
#else
	default_file_info.icon = DEF_FILE_ICON;
	default_file_info.icon_color = DEF_FILE_ICON_COLOR;
#endif /* _NO_ICONS */
	default_file_info.linkn = 1;
	default_file_info.user_access = 1;
	default_file_info.size = 1;
}

static inline void
get_id_names(const filesn_t n)
{
	size_t i;

	if (sys_users) {
		for (i = 0; sys_users[i].name; i++) {
			if (file_info[n].uid != sys_users[i].id)
				continue;
			file_info[n].uid_i.name = sys_users[i].name;
			file_info[n].uid_i.namlen = sys_users[i].namlen;
		}
	}

	if (sys_groups) {
		for (i = 0; sys_groups[i].name; i++) {
			if (file_info[n].gid != sys_groups[i].id)
				continue;
			file_info[n].gid_i.name = sys_groups[i].name;
			file_info[n].gid_i.namlen = sys_groups[i].namlen;
		}
	}
}

/* Construct human readable sizes for all files in the current directory
 * and store them in the human_size struct field of the file_info struct. The
 * length of each human size is stored in the human_size.len field of the
 * same struct. */
static void
construct_human_sizes(void)
{
	const off_t ibase = xargs.si == 1 ? 1000 : 1024;
	const float base = (float)ibase;
	/* R: Ronnabyte, Q: Quettabyte. It's highly unlikely to have files of
	 * such huge sizes (and even less) in the near future, but anyway... */
	static const char *const u_iec = "BKMGTPEZYRQ";
	/* Let's follow du(1) in using 'k' (lowercase) instead of 'K'
	 * (uppercase) when using powers of 1000 (--si). */
	static const char *const u_si =  "BkMGTPEZYRQ";
	const char *units = xargs.si == 1 ? u_si : u_iec;

	static float mult_factor = 0;
	if (mult_factor == 0)
		mult_factor = 1.0f / base;

	filesn_t i = g_files_num;
	while (--i >= 0) {
		if (file_info[i].size < ibase) { /* This includes negative values */
			const int ret = snprintf(file_info[i].human_size.str,
				MAX_HUMAN_SIZE, "%jd", (intmax_t)file_info[i].size);
			file_info[i].human_size.len = ret > 0 ? (size_t)ret : 0;
			file_info[i].human_size.unit = units[0];
			continue;
		}

		size_t n = 0;
		float s = (float)file_info[i].size;

		while (s >= base) {
			s = s * mult_factor; /* == (s = s / base), but faster */
			n++;
		}

		/* If s == (float)(int)s, then S has no reminder (zero):
		 * We don't want to print the reminder when it is zero. */
		const int ret =
			snprintf(file_info[i].human_size.str, MAX_HUMAN_SIZE, "%.*f",
				s == (float)(int)s ? 0 : 2, (double)s);

		file_info[i].human_size.len = ret > 0 ? (size_t)ret : 0;
		file_info[i].human_size.unit = units[n];
	}
}

#define LIST_SCANNING_MSG "Scanning... "
static void
print_scanning_message(void)
{
	UNHIDE_CURSOR;
	fputs(LIST_SCANNING_MSG, stdout);
	fflush(stdout);
	if (xargs.list_and_quit != 1)
		HIDE_CURSOR;
}

static void
print_scanned_file(const char *name)
{
	printf("\r\x1b[%zuC\x1b[0K%s%s%s/", sizeof(LIST_SCANNING_MSG) - 1,
		di_c, name, df_c);
	fflush(stdout);
}
#undef LIST_SCANNING_MSG

static void
erase_scanning_message(void)
{
	fputs("\r\x1b[0K", stdout);
	fflush(stdout);
}

static void
check_autocmd_files(void)
{
	struct stat a;
	char buf[PATH_MAX + 1];

	snprintf(buf, sizeof(buf), "%s/%s", workspaces[cur_ws].path,
		AUTOCMD_DIR_IN_FILE);

	/* Non-regular files, empty regular files, or bigger than PATH_MAX bytes,
	 * are rejected. */
	if (lstat(buf, &a) != -1 && S_ISREG(a.st_mode)
	&& a.st_size > 0 && a.st_size <= PATH_MAX)
		run_dir_cmd(AUTOCMD_DIR_IN, buf);

	snprintf(buf, sizeof(buf), "%s/%s", workspaces[cur_ws].path,
		AUTOCMD_DIR_OUT_FILE);

	if (lstat(buf, &a) != -1 && S_ISREG(a.st_mode)
	&& a.st_size > 0 && a.st_size <= PATH_MAX)
		dir_out = 1;
}

/* List files in the current working directory (global variable 'path').
 * Unlike list_dir(), however, this function uses no color and runs
 * neither stat() nor count_dir(), which makes it quite faster. Returns
 * zero on success and one on error. */
static int
list_dir_light(const int autocmd_ret)
{
#ifdef LIST_SPEED_TEST
	struct timespec t1, t2;
	clock_gettime(CLOCK_MONOTONIC, &t1);
#endif /* LIST_SPEED_TEST */

	struct dothidden_t *hidden_list =
		(conf.read_dothidden == 1 && conf.show_hidden == 0)
		? load_dothidden() : NULL;

	DIR *dir;
	struct dirent *ent;
	int reset_pager = 0;
	int close_dir = 1;

	/* Let's store information about the largest file in the list for the
	 * disk usage analyzer mode. */
	off_t largest_name_size = 0, total_size = 0;
	char *largest_name = NULL, *largest_color = NULL;

	if ((dir = opendir(workspaces[cur_ws].path)) == NULL) {
		xerror("%s: %s: %s\n", PROGRAM_NAME, workspaces[cur_ws].path,
			strerror(errno));
		close_dir = 0;
		goto END;
	}

#ifdef POSIX_FADV_SEQUENTIAL
	const int fd = dirfd(dir);
	if (fd == -1) {
		xerror(_("%s: Error getting file descriptor for the current "
			"directory: %s\n"), PROGRAM_NAME, workspaces[cur_ws].path,
			strerror(errno));
		goto END;
	}

	posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
#endif /* POSIX_FADV_SEQUENTIAL */

	if (checks.autocmd_files == 1)
		check_autocmd_files();

	set_events_checker();

	errno = 0;
	longest.name_len = 0;
	filesn_t n = 0, count = 0;
	size_t total_dents = 0;

	file_info = xnmalloc(ENTRY_N + 2, sizeof(struct fileinfo));

	while ((ent = readdir(dir))) {
		const char *ename = ent->d_name;
		/* Skip self and parent directories */
		if (SELFORPARENT(ename))
			continue;

		/* Skip files according to a regex filter */
		if (checks.filter_name == 1) {
			if (regexec(&regex_exp, ename, 0, NULL, 0) == FUNC_SUCCESS) {
				if (filter.rev == 1) {
					stats.excluded++;
					continue;
				}
			} else {
				if (filter.rev == 0) {
					stats.excluded++;
					continue;
				}
			}
		}

		if (*ename == '.') {
			if (conf.show_hidden == 0) {
				stats.excluded++;
				continue;
			}
			stats.hidden++;
		}

		if (hidden_list	&& check_dothidden(ename, &hidden_list) == 1) {
			stats.excluded++;
			continue;
		}

#ifndef _DIRENT_HAVE_D_TYPE
		struct stat attr;
		if (lstat(ename, &attr) == -1)
			continue;
		if (conf.only_dirs == 1 && !S_ISDIR(attr.st_mode))
#else
		if (conf.only_dirs == 1 && ent->d_type != DT_DIR)
#endif /* !_DIRENT_HAVE_D_TYPE */
		{
			if (*ename == '.' && stats.hidden > 0)
				stats.hidden--;
			stats.excluded++;
			continue;
		}

		/* Filter files according to file type */
		if (checks.filter_type == 1
#ifndef _DIRENT_HAVE_D_TYPE
		&& exclude_file_type_light((unsigned char)get_dt(attr.st_mode))
		== FUNC_SUCCESS)
#else
		&& exclude_file_type_light(ent->d_type) == FUNC_SUCCESS)
#endif /* !_DIRENT_HAVE_D_TYPE */
		{
			if (*ename == '.' && stats.hidden > 0)
				stats.hidden--;
			stats.excluded++;
			continue;
		}

		if (count > ENTRY_N) {
			count = 0;
			total_dents = (size_t)n + ENTRY_N;
			file_info = xnrealloc(file_info, total_dents + 2,
				sizeof(struct fileinfo));
		}

		file_info[n] = default_file_info;

		size_t ext_index = 0;
		file_info[n].utf8 = is_utf8_name(ename, &file_info[n].bytes, &ext_index);
		file_info[n].name = xnmalloc(file_info[n].bytes + 1, sizeof(char));
		memcpy(file_info[n].name, ename, file_info[n].bytes + 1);

		file_info[n].len = file_info[n].utf8 == 0
			? file_info[n].bytes : wc_xstrlen(ename);

		file_info[n].ext_name =
			ext_index > 0 ? file_info[n].name + ext_index : NULL;

		/* ################  */
#ifndef _DIRENT_HAVE_D_TYPE
		file_info[n].type = get_dt(attr.st_mode);
#else
		/* If type is unknown, we might be facing a filesystem not
		 * supporting d_type, for example, loop devices. In this case,
		 * try falling back to lstat(2). */
		if (ent->d_type != DT_UNKNOWN) {
			file_info[n].type = ent->d_type;
		} else {
			struct stat a;
			if (lstat(ename, &a) == -1)
				continue;
			file_info[n].type = get_dt(a.st_mode);
		}
#endif /* !_DIRENT_HAVE_D_TYPE */

		file_info[n].dir = (file_info[n].type == DT_DIR);
		file_info[n].symlink = (file_info[n].type == DT_LNK);
		file_info[n].inode = ent->d_ino;

		if (checks.scanning == 1 && file_info[n].dir == 1)
			print_scanned_file(file_info[n].name);

		switch (file_info[n].type) {
		case DT_DIR:
#ifndef _NO_ICONS
			if (conf.icons == 1) {
				file_info[n].icon = DEF_DIR_ICON;
				file_info[n].icon_color = DEF_DIR_ICON_COLOR;
				/* If set from the color scheme file */
				if (*dir_ico_c)
					file_info[n].icon_color = dir_ico_c;
			}
#endif /* !_NO_ICONS */

			stats.dir++;
			if (conf.file_counter == 1)
				file_info[n].filesn = count_dir(ename, NO_CPOP) - 2;
			else
				file_info[n].filesn = 1;

			if (file_info[n].filesn > 0) {
				file_info[n].color = di_c;
			} else if (file_info[n].filesn == 0) {
				file_info[n].color = ed_c;
			} else {
				file_info[n].color = *nd_c ? nd_c : di_c;
#ifndef _NO_ICONS
				file_info[n].icon = ICON_LOCK;
				file_info[n].icon_color = YELLOW;
#endif /* !_NO_ICONS */
			}

			break;

		case DT_LNK:
#ifndef _NO_ICONS
			file_info[n].icon = ICON_LINK;
#endif /* !_NO_ICONS */
			file_info[n].color = ln_c;
			stats.link++;
			break;

		case DT_REG: file_info[n].color = fi_c; stats.reg++; break;
		case DT_SOCK: file_info[n].color = so_c; stats.socket++; break;
		case DT_FIFO: file_info[n].color = pi_c; stats.fifo++; break;
		case DT_BLK: file_info[n].color = bd_c; stats.block_dev++; break;
		case DT_CHR: file_info[n].color = cd_c; stats.char_dev++; break;
#ifndef _BE_POSIX
# ifdef SOLARIS_DOORS
		case DT_DOOR: file_info[n].color = oo_c; stats.door++; break;
		case DT_PORT: file_info[n].color = oo_c; stats.port++; break;
# endif /* SOLARIS_DOORS */
# ifdef S_ARCH1
		case DT_ARCH1: file_info[n].color = fi_c; stats.arch1++; break;
		case DT_ARCH2: file_info[n].color = fi_c; stats.arch2++; break;
# endif /* S_ARCH1 */
# ifdef S_IFWHT
		case DT_WHT: file_info[n].color = fi_c; stats.whiteout++; break;
# endif /* DT_WHT */
#endif /* !_BE_POSIX */
		case DT_UNKNOWN: file_info[n].color = no_c; stats.unknown++; break;
		default: file_info[n].color = df_c; break;
		}

#ifndef _NO_ICONS
		if (checks.icons_use_file_color == 1)
			file_info[n].icon_color = file_info[n].color;
#endif /* !_NO_ICONS */

		if (conf.long_view == 1) {
#ifndef _DIRENT_HAVE_D_TYPE
			set_long_attribs(n, &attr);
#else
			struct stat a;
			if (lstat(file_info[n].name, &a) != -1)
				set_long_attribs(n, &a);
			else
				file_info[n].stat_err = 1;
#endif /* !_DIRENT_HAVE_D_TYPE */
			if (prop_fields.ids == PROP_ID_NAME && file_info[n].stat_err == 0)
				get_id_names(n);
		}

		if (xargs.disk_usage_analyzer == 1) {
			get_largest_file_info(n, &largest_name_size, &largest_name,
				&largest_color, &total_size);
		}

		n++;
		if (n > FILESN_MAX - 1) {
			err('w', PRINT_PROMPT, _("%s: Integer overflow detected "
				"(showing only %jd files)\n"), PROGRAM_NAME, (intmax_t)n);
			break;
		}

		count++;
	}

	file_info[n].name = NULL;
	g_files_num = n;

	if (checks.scanning == 1)
		erase_scanning_message();

	if (n == 0) {
		printf("%s. ..%s\n", di_c, df_c);
		free(file_info);
		goto END;
	}

	const int eln_len = conf.no_eln == 1 ? 0
		: ((conf.max_files != UNSET && g_files_num > (filesn_t)conf.max_files)
		? DIGINUM(conf.max_files) : DIGINUM(g_files_num));

	if (conf.sort != SNONE)
		ENTSORT(file_info, (size_t)n, entrycmp);

	/* Get the longest filename */
	if (conf.columned == 1 || conf.long_view == 1)
		get_longest_filename(n, (size_t)eln_len);

	/* Get possible number of columns for the dirlist screen */
	const size_t columns_n = (conf.pager_view == PAGER_AUTO
		&& (conf.columned == 0 || conf.long_view == 1)) ? 1 : get_columns();

	set_pager_view((filesn_t)columns_n);

				/* ########################
				 * #    LONG VIEW MODE    #
				 * ######################## */

	if (conf.long_view == 1) {
		if (prop_fields.size == PROP_SIZE_HUMAN)
			construct_human_sizes();
		print_long_mode(&reset_pager, eln_len);
	}

				/* ########################
				 * #   NORMAL VIEW MODE   #
				 * ######################## */

	else if (conf.listing_mode == VERTLIST) { /* ls(1)-like listing */
		list_files_vertical(&reset_pager, eln_len, columns_n);
	} else {
		list_files_horizontal(&reset_pager, eln_len, columns_n);
	}

				/* #########################
				 * #   POST LISTING STUFF  #
				 * ######################### */

END:
	if (hidden_list)
		free_dothidden(&hidden_list);

	exit_code =
		post_listing(close_dir == 1 ? dir : NULL, reset_pager, autocmd_ret);

#ifndef ST_BTIME_LIGHT
	if (conf.long_view == 1 && prop_fields.time == PROP_TIME_BIRTH)
		print_reload_msg(NULL, NULL, _("Long view: Birth time not available "
			"in light mode. Using %smodification time%s.\n"), BOLD, NC);
#endif /* !ST_BTIME_LIGHT */

	if (xargs.disk_usage_analyzer == 1 && conf.long_view == 1
	&& conf.full_dir_size == 1) {
		print_analysis_stats(total_size, largest_name_size,
			largest_color, largest_name);
	}

#ifdef LIST_SPEED_TEST
	clock_gettime(CLOCK_MONOTONIC, &t2);
	double secs = (double)(t2.tv_sec - t1.tv_sec)
		+ (double)(t2.tv_nsec - t1.tv_nsec) * 1e-9;
	printf("list_dir time: %f\n", secs);
#endif /* LIST_SPEED_TEST */

	return exit_code;
}

/* Check whether the file in the device DEV with inode INO is selected.
 * Used to mark selected files in the file list. */
static int
check_seltag(const dev_t dev, const ino_t ino, const nlink_t links,
	const filesn_t index)
{
	if (sel_n == 0 || !sel_devino)
		return 0;

	for (size_t j = sel_n; j-- > 0;) {
		if (sel_devino[j].dev != dev || sel_devino[j].ino != ino)
			continue;
		/* Only check hardlinks in case of regular files */
		if (file_info[index].type != DT_DIR && links > 1) {
			const char *p = strrchr(sel_elements[j].name, '/');
			if (!p || !*(++p))
				continue;
			if (*p == *file_info[index].name
			&& strcmp(p, file_info[index].name) == 0)
				return 1;
		} else {
			return 1;
		}
	}

	return 0;
}

/* Get the color of a link target NAME, whose file attributes are ATTR,
 * and write the result into the file_info array at index I. */
static inline void
set_link_target_color(const char *name, const struct stat *attr,
	const filesn_t i)
{
	switch (attr->st_mode & S_IFMT) {
	case S_IFSOCK: file_info[i].color = so_c; break;
	case S_IFIFO:  file_info[i].color = pi_c; break;
	case S_IFBLK:  file_info[i].color = bd_c; break;
	case S_IFCHR:  file_info[i].color = cd_c; break;
#ifndef _BE_POSIX
# ifdef SOLARIS_DOORS
	case S_IFDOOR: file_info[i].color = oo_c; break;
	case S_IFPORT: file_info[i].color = oo_c; break;
# endif /* SOLARIS_DOORS */
# ifdef S_ARCH1
	case S_ARCH1: file_info[i].color = fi_c; break;
	case S_ARCH2: file_info[i].color = fi_c; break;
# endif /* S_ARCH1 */
# ifdef S_IFWHT
	case S_IFWHT: file_info[i].color = fi_c; break;
# endif /* S_IFWHT */
#endif /* !_BE_POSIX */
	case S_IFREG: {
		size_t clen = 0;
		char *color = get_regfile_color(name, attr, &clen);

		if (!color) {
			file_info[i].color = fi_c;
		} else if (clen > 0) { /* We have an extension color */
			file_info[i].ext_color = savestring(color, clen);
			file_info[i].color = file_info[i].ext_color;
		} else {
			file_info[i].color = color;
		}
		}
		break;

	default: file_info[i].color = df_c; break;
	}
}

static inline void
check_extra_file_types(mode_t *mode, const struct stat *a)
{
	/* If all the below macros are originally undefined, they all expand to
	 * zero, in which case A is never used. Let's avoid a compiler warning. */
	UNUSED(a);

	if (S_TYPEISMQ(a)) {
		*mode = DT_MQ;
	} else if (S_TYPEISSEM(a)) {
		*mode = DT_SEM;
	} else if (S_TYPEISSHM(a)) {
		*mode = DT_SHM;
	} else {
		if (S_TYPEISTMO(a))
			*mode = DT_TPO;
	}
}

static inline void
set_long_view_time(const filesn_t n, const struct stat *a,
	const time_t birth_time)
{
	if (checks.time_follows_sort == 1) {
		switch (conf.sort) {
		case SATIME: file_info[n].ltime = a->st_atime; return;
		case SBTIME: file_info[n].ltime = birth_time; return;
		case SCTIME: file_info[n].ltime = a->st_ctime; return;
		case SMTIME: file_info[n].ltime = a->st_mtime; return;
		default:
			/* Not sorting by time: fallthrough to the time field
			 * in PropFields. */
			break;
		}
	}

	switch (prop_fields.time) {
	case PROP_TIME_ACCESS: file_info[n].ltime = a->st_atime; break;
	case PROP_TIME_BIRTH: file_info[n].ltime = birth_time; break;
	case PROP_TIME_CHANGE: file_info[n].ltime = a->st_ctime; break;
	case PROP_TIME_MOD: /* fallthrough */
	default: file_info[n].ltime = a->st_mtime; break;
	}
}

static inline time_t
get_birth_time(const filesn_t n, const struct stat *a)
{
	time_t birth_time = (time_t)-1;

#if defined(ST_BTIME)
# ifdef LINUX_STATX
	UNUSED(a);
	struct statx attx;
	if (statx(AT_FDCWD, file_info[n].name, AT_SYMLINK_NOFOLLOW,
	STATX_BTIME, &attx) == 0 && (attx.stx_mask & STATX_BTIME))
		birth_time = attx.ST_BTIME.tv_sec;
# elif defined(__sun)
	UNUSED(a);
	struct timespec birthtim = get_birthtime(file_info[n].name);
	birth_time = birthtim.tv_sec;
# else
	birth_time = a->ST_BTIME.tv_sec;
# endif /* LINUX_STATX */
#endif /* ST_BTIME */

	return birth_time;
}

static inline void
load_file_gral_info(const struct stat *a, const filesn_t n)
{
	if (check_file_access(a->st_mode, a->st_uid, a->st_gid) == 0) {
		file_info[n].user_access = 0;
#ifndef _NO_ICONS
		file_info[n].icon = DEF_NOPERM_ICON;
		file_info[n].icon_color = S_ISDIR(a->st_mode)
			? DEF_NOPERM_ICON_COLOR_DIR : DEF_NOPERM_ICON_COLOR_FILE;
#endif /* !_NO_ICONS */
	}

	switch (a->st_mode & S_IFMT) {
	case S_IFREG: file_info[n].type = DT_REG; stats.reg++; break;
	case S_IFDIR: file_info[n].type = DT_DIR; stats.dir++; break;
	case S_IFLNK: file_info[n].type = DT_LNK; stats.link++; break;
	case S_IFIFO: file_info[n].type = DT_FIFO; stats.fifo++; break;
	case S_IFSOCK: file_info[n].type = DT_SOCK; stats.socket++; break;
	case S_IFBLK: file_info[n].type = DT_BLK; stats.block_dev++; break;
	case S_IFCHR: file_info[n].type = DT_CHR; stats.char_dev++; break;
#ifndef _BE_POSIX
# ifdef SOLARIS_DOORS
	case S_IFDOOR: file_info[n].type = DT_DOOR; stats.door++; break;
	case S_IFPORT: file_info[n].type = DT_PORT; stats.port++; break;
# endif /* SOLARIS_DOORS */
# ifdef S_ARCH1
	case S_ARCH1: file_info[n].type = DT_ARCH1; stats.arch1++; break;
	case S_ARCH2: file_info[n].type = DT_ARCH2; stats.arch2++; break;
# endif /* S_ARCH1 */
# ifdef S_IFWHT
	case S_IFWHT: file_info[n].type = DT_WHT; stats.whiteout++; break;
# endif /* S_IFWHT */
#endif /* !_BE_POSIX */
	default: file_info[n].type = DT_UNKNOWN; stats.unknown++; break;
	}

	check_extra_file_types(&file_info[n].type, a);

	file_info[n].blocks = a->st_blocks;
	file_info[n].inode = a->st_ino;
	file_info[n].linkn = a->st_nlink;
	file_info[n].mode = a->st_mode;
	file_info[n].sel = check_seltag(a->st_dev, a->st_ino, a->st_nlink, n);
	file_info[n].size = FILE_TYPE_NON_ZERO_SIZE(a->st_mode) ? FILE_SIZE(*a) : 0;
	file_info[n].uid = a->st_uid;
	file_info[n].gid = a->st_gid;

	if (checks.id_names == 1)
		get_id_names(n);

#if defined(LINUX_FILE_XATTRS)
	if (file_info[n].type != DT_LNK
	&& (checks.xattr == 1 || conf.check_cap == 1)
	&& listxattr(file_info[n].name, NULL, 0) > 0) {
		file_info[n].xattr = 1;
		stats.extended++;
	}
#endif /* LINUX_FILE_XATTRS */

	const time_t birth_time =
		checks.birthtime == 1 ? get_birth_time(n, a) : (time_t)-1;

	switch (conf.sort) {
	case SATIME: file_info[n].time = a->st_atime; break;
	case SBTIME: file_info[n].time = birth_time; break;
	case SCTIME: file_info[n].time = a->st_ctime; break;
	case SMTIME: file_info[n].time = a->st_mtime; break;
	default: file_info[n].time = 0; break;
	}

	/* Set the time field (ltime) for long view. */
	if (conf.long_view == 1)
		set_long_view_time(n, a, birth_time);
}

static inline void
load_dir_info(const mode_t mode, const filesn_t n)
{
	file_info[n].dir = 1;

#ifndef _NO_ICONS
	if (conf.icons == 1)
		get_dir_icon(n);
#endif /* !_NO_ICONS */

	if (checks.file_counter == 1) {
		/* Avoid count_dir() if we have no access to the current directory. */
		file_info[n].filesn = file_info[n].user_access == 0 ? -1
			: count_dir(file_info[n].name, NO_CPOP) - 2;
	} else {
		file_info[n].filesn = 1;
	}

	if (*nd_c && (file_info[n].user_access == 0 || file_info[n].filesn < 0)) {
		file_info[n].color = nd_c;
	} else {
		file_info[n].color = mode != 0 ? ((mode & S_ISVTX)
			? ((mode & S_IWOTH) ? tw_c : st_c)
			: ((mode & S_IWOTH) ? ow_c
			: (file_info[n].filesn == 0 ? ed_c : di_c)))
			: uf_c; /* stat error. */
	}

	stats.empty_dir += (file_info[n].filesn == 0);

	/* Let's gather some file statistics based on the file type color */
	if (file_info[n].color == tw_c) {
		stats.other_writable++;
		stats.sticky++;
	} else if (file_info[n].color == ow_c) {
		stats.other_writable++;
	} else {
		if (file_info[n].color == st_c)
			stats.sticky++;
	}
}

static inline void
set_long_attribs_link_target(const filesn_t n, const struct stat *a)
{
	file_info[n].blocks = a->st_blocks;
	file_info[n].inode = a->st_ino;
	file_info[n].linkn = a->st_nlink;
	file_info[n].mode = a->st_mode;
	file_info[n].uid = a->st_uid;
	file_info[n].gid = a->st_gid;
	if (checks.id_names == 1)
		get_id_names(n);

	/* While we do want to show info about the link target, it is
	 * misleading to show the size of the file it points to, because that
	 * space is not really consumed by the link. This is what nnn and vifm do,
	 * and I think they're right: If we display the target size, we would end
	 * up showing duplicate/mirrored sizes: once for the link, another one for
	 * its target. */
/*	if (conf.full_dir_size == 1 && S_ISDIR(a->st_mode)) {
		file_info[n].size = dir_size(file_info[n].name, 1,
			&file_info[n].du_status);
	} else {
	file_info[n].size = FILE_SIZE_PTR(a);
	} */

	const time_t birth_time =
		checks.birthtime == 1 ? get_birth_time(n, a) : (time_t)-1;
	set_long_view_time(n, a, birth_time);
}

static inline void
load_link_info(const int fd, const filesn_t n)
{
	file_info[n].symlink = 1;

#ifndef _NO_ICONS
	file_info[n].icon = DEF_LINK_ICON;
	file_info[n].icon_color = conf.colorize_lnk_as_target == 1 ?
		DEF_LINK_ICON_COLOR : DEF_FILE_ICON_COLOR;
#endif /* !_NO_ICONS */

	if (conf.follow_symlinks == 0) {
		file_info[n].color = ln_c;
		return;
	}

	struct stat a;
	if (fstatat(fd, file_info[n].name, &a, 0) == -1) {
		file_info[n].color = or_c;
		file_info[n].xattr = 0;
		stats.broken_link++;
		return;
	}

	if (conf.long_view == 1)
		set_long_attribs_link_target(n, &a);

	/* We only need the symlink target name provided the target is not a
	 * directory, because set_link_target_color() will check the filename
	 * extension. get_dir_color() only needs this name to run count_dir(),
	 * but we have already executed this function. */
	static char tmp[PATH_MAX + 1]; *tmp = '\0';
	const ssize_t ret =
		(conf.colorize_lnk_as_target == 1 && !S_ISDIR(a.st_mode))
		? readlinkat(XAT_FDCWD, file_info[n].name, tmp, sizeof(tmp) - 1) : 0;
	if (ret > 0)
		tmp[ret] = '\0';

	const char *lname = *tmp ? tmp : file_info[n].name;

	if (!S_ISDIR(a.st_mode)) {
		if (conf.colorize_lnk_as_target == 1)
			set_link_target_color(lname, &a, n);
		else
			file_info[n].color = ln_c;
	} else {
		file_info[n].dir = 1;
		file_info[n].filesn = conf.file_counter == 1
			? count_dir(file_info[n].name, NO_CPOP) - 2
			: 1;

		const filesn_t files_in_dir = conf.file_counter == 1
			? (file_info[n].filesn > 0 ? 3 : file_info[n].filesn)
			: 3;
			/* 3 == populated (we don't care how many files the directory
			 * actually contains). */

		if (files_in_dir < 0 && *nd_c) { /* count_dir() failed. */
			file_info[n].color = conf.colorize_lnk_as_target == 1 ? nd_c : ln_c;
		} else {
			file_info[n].color = conf.colorize_lnk_as_target == 1
				? get_dir_color(lname, &a, files_in_dir) : ln_c;
		}
	}
}

static inline void
load_regfile_info(const mode_t mode, const filesn_t n)
{
#ifdef LINUX_FILE_CAPS
	cap_t cap;
#endif /* !LINUX_FILE_CAPS */

	if (file_info[n].user_access == 0 && *nf_c) {
		file_info[n].color = nf_c;
	} else if (mode & S_ISUID) {
		file_info[n].exec = 1;
		stats.exec++;
		stats.suid++;
		file_info[n].color = su_c;
	} else if (mode & S_ISGID) {
		file_info[n].exec = 1;
		stats.exec++;
		stats.sgid++;
		file_info[n].color = sg_c;
	}

#ifdef LINUX_FILE_CAPS
	/* Capabilities are stored by the system as extended attributes.
	 * No xattrs, no caps. */
	else if (file_info[n].xattr == 1
	&& (cap = cap_get_file(file_info[n].name))) {
		file_info[n].color = ca_c;
		stats.caps++;
		cap_free(cap);
		if (IS_EXEC(mode)) {
			file_info[n].exec = 1;
			stats.exec++;
		}
	}
#endif /* LINUX_FILE_CAPS */

	else if (IS_EXEC(mode)) {
		file_info[n].exec = 1;
		stats.exec++;
		file_info[n].color = file_info[n].size == 0 ? ee_c : ex_c;
	} else if (file_info[n].linkn > 1) { /* Multi-hardlink */
		file_info[n].color = mh_c;
		stats.multi_link++;
	} else if (file_info[n].size == 0) {
		file_info[n].color = ef_c;
		stats.empty_reg++;
	} else { /* Regular file */
		file_info[n].color = fi_c;
	}

#ifndef _NO_ICONS
	if (file_info[n].exec == 1) {
		file_info[n].icon = DEF_EXEC_ICON;
		file_info[n].icon_color = DEF_EXEC_ICON_COLOR;
	}
#endif /* !_NO_ICONS */

	/* Try temp and extension color only provided the file is a non-empty
	 * regular file. */
	const int override_color = (file_info[n].color == fi_c);

	if (override_color == 1
	&& IS_TEMP_FILE(file_info[n].name, file_info[n].bytes)) {
		file_info[n].color = bk_c;
		return;
	}

#ifndef _NO_ICONS
	/* The icons check precedence order is this:
	 * 1. filename or filename.extension
	 * 2. extension
	 * 3. file type */
	/* Check icons for specific filenames */
	const int name_icon_found = conf.icons == 1 ? get_name_icon(n) : 0;
#endif /* !_NO_ICONS */

	if (!file_info[n].ext_name || conf.check_ext == 0)
		return;

	/* Check file extension */
	const char *ext = file_info[n].ext_name;

#ifndef _NO_ICONS
	if (conf.icons == 1 && name_icon_found == 0)
		get_ext_icon(ext, n);
#endif /* !_NO_ICONS */

	size_t color_len = 0;
	const char *extcolor =
		override_color == 1 ? get_ext_color(ext, &color_len) : NULL;
	if (!extcolor)
		return;

	char *t = xnmalloc(color_len + 4, sizeof(char));
	*t = '\x1b'; t[1] = '[';
	memcpy(t + 2, extcolor, color_len);
	t[color_len + 2] = 'm';
	t[color_len + 3] = '\0';
	file_info[n].ext_color = file_info[n].color = t;
}

static int
vt_stat(const int fd, char *restrict path, struct stat *attr)
{
	static char buf[PATH_MAX + 1];
	*buf = '\0';

	if (xreadlink(fd, path, buf, sizeof(buf)) == -1)
		return (-1);

	if (!*buf || fstatat(fd, buf, attr, AT_SYMLINK_NOFOLLOW) == -1)
		return (-1);

	return 0;
}

/* List files in the current working directory. Uses file type colors
 * and columns. Return 0 on success or 1 on error. */
int
list_dir(void)
{
#ifdef LIST_SPEED_TEST
	struct timespec t1, t2;
	clock_gettime(CLOCK_MONOTONIC, &t1);
#endif /* LIST_SPEED_TEST */

	if (conf.clear_screen > 0) {
		CLEAR;
		fflush(stdout);
	}

	/* Hide the cursor to minimize flickering: it will be unhidden immediately
	 * before printing the next prompt (prompt.c) */
	if (xargs.list_and_quit != 1)
		HIDE_CURSOR;

	int autocmd_ret = 0;
	if (autocmds_n > 0 && dir_changed == 1) {
		if (autocmd_set == 1)
			revert_autocmd_opts();
		autocmd_ret = check_autocmds();
	}

	if (dir_changed == 1 && dir_out == 1) {
		run_dir_cmd(AUTOCMD_DIR_OUT, NULL);
		dir_out = 0;
	}

	if (conf.clear_screen > 0) {
		/* For some reason we need to clear the screen twice to prevent
		 * a garbage first line when scrolling up. */
		CLEAR;
		fflush(stdout);
	}

	get_term_size();

	virtual_dir =
		(stdin_tmp_dir && strcmp(stdin_tmp_dir, workspaces[cur_ws].path) == 0);

	stats = (struct stats_t){0}; /* Reset the stats struct */
	init_checks_struct();
	init_default_file_info();

	if (checks.scanning == 1)
		print_scanning_message();

	if (conf.long_view == 1)
		props_now = time(NULL);

	if (conf.light_mode == 1)
		return list_dir_light(autocmd_ret);

	struct dothidden_t *hidden_list =
		(conf.read_dothidden == 1 && conf.show_hidden == 0)
		? load_dothidden() : NULL;

	DIR *dir;
	struct dirent *ent;
	struct stat attr;
	int reset_pager = 0;
	int close_dir = 1;

	/* Let's store information about the largest file in the list for the
	 * disk usage analyzer mode. */
	off_t largest_name_size = 0, total_size = 0;
	char *largest_name = NULL;
	char *largest_color = NULL;

	if ((dir = opendir(workspaces[cur_ws].path)) == NULL) {
		xerror("%s: %s: %s\n", PROGRAM_NAME, workspaces[cur_ws].path,
			strerror(errno));
		close_dir = 0;
		goto END;
	}

	set_events_checker();

	const int fd = dirfd(dir);
	if (fd == -1) {
		xerror(_("%s: Error getting file descriptor for the current "
			"directory: %s\n"), PROGRAM_NAME, workspaces[cur_ws].path,
			strerror(errno));
		goto END;
	}

#ifdef POSIX_FADV_SEQUENTIAL
	/* A hint to the kernel to optimize current dir for reading */
	posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
#endif /* POSIX_FADV_SEQUENTIAL */

	if (checks.autocmd_files == 1)
		check_autocmd_files();

		/* ##########################################
		 * #    GATHER AND STORE FILE INFORMATION   #
		 * ########################################## */

	errno = 0;
	longest.name_len = 0;
	filesn_t n = 0, count = 0;
	size_t total_dents = 0;

	file_info = xnmalloc(ENTRY_N + 2, sizeof(struct fileinfo));

	/* Cache used values in local variables for faster access. */
	const int checks_filter_name = checks.filter_name;
	const int checks_filter_type = checks.filter_type;
	const int checks_scanning = checks.scanning;
#ifndef _NO_ICONS
	const int checks_icons_use_file_color = checks.icons_use_file_color;
#endif /* !_NO_ICONS */
	const int conf_only_dirs = conf.only_dirs;
	const int conf_show_hidden = conf.show_hidden;
	const int conf_follow_symlinks = conf.follow_symlinks;
	const int conf_long_view = conf.long_view;
	const int xargs_disk_usage_analyzer = xargs.disk_usage_analyzer;

	while ((ent = readdir(dir))) {
		const char *ename = ent->d_name;
		/* Skip self and parent directories */
		if (SELFORPARENT(ename))
			continue;

		/* Filter files according to a regex filter */
		if (checks_filter_name == 1) {
			if (regexec(&regex_exp, ename, 0, NULL, 0) == 0) {
				if (filter.rev == 1) {
					stats.excluded++;
					continue;
				}
			} else {
				if (filter.rev == 0) {
					stats.excluded++;
					continue;
				}
			}
		}

		if (*ename == '.') {
			if (conf_show_hidden == 0) {
				stats.excluded++;
				continue;
			}
			stats.hidden++;
		}

		if (hidden_list && check_dothidden(ename, &hidden_list) == 1) {
			stats.excluded++;
			continue;
		}

		const int stat_ok =
			((virtual_dir == 1 ? vt_stat(fd, ent->d_name, &attr)
			: fstatat(fd, ename, &attr, AT_SYMLINK_NOFOLLOW)) == 0);

		if (stat_ok == 0) {
			if (virtual_dir == 1)
				continue;
		} else {
			/* Filter files according to file type. */
			if ((checks_filter_type == 1
			&& exclude_file_type(ename, attr.st_mode, attr.st_nlink,
			attr.st_size) == FUNC_SUCCESS)
			/* Filter non-directory files. */
			|| (conf_only_dirs == 1 && !S_ISDIR(attr.st_mode)
			&& (conf_follow_symlinks == 0 || !S_ISLNK(attr.st_mode)
			|| get_link_ref(ename) != S_IFDIR))) {
				/* Decrease the counter: the file won't be displayed. */
				if (*ename == '.' && stats.hidden > 0)
					stats.hidden--;
				stats.excluded++;
				continue;
			}
		}

		if (count > ENTRY_N) {
			count = 0;
			total_dents = (size_t)n + ENTRY_N;
			file_info = xnrealloc(file_info, total_dents + 2,
				sizeof(struct fileinfo));
		}

		file_info[n] = default_file_info;

		/* Both is_utf8_name() and wc_xstrlen() calculate the number of
		 * columns needed to display the current filename on the screen
		 * (the former for ASCII names, where 1 char = 1 byte = 1 column, and
		 * the latter for UTF-8 names, i.e. containing at least one non-ASCII
		 * character).
		 * Now, since is_utf8_name() is ~8 times faster than wc_xstrlen()
		 * (10,000 entries, optimization O3), we only run wc_xstrlen() in
		 * case of a UTF-8 name.
		 * However, since is_utf8_name() will be executed anyway, this ends
		 * up being actually slower whenever the current directory contains
		 * more UTF-8 than ASCII names. The assumption here is that ASCII
		 * names are far more common than UTF-8 names. */
		size_t ext_index = 0;
		file_info[n].utf8 =
			is_utf8_name(ename, &file_info[n].bytes, &ext_index);

		file_info[n].name = xnmalloc(file_info[n].bytes + 1, sizeof(char));
		memcpy(file_info[n].name, ename, file_info[n].bytes + 1);

		/* Columns needed to display filename */
		file_info[n].len = file_info[n].utf8 == 0
			? file_info[n].bytes : wc_xstrlen(ename);

		file_info[n].ext_name = ext_index == 0
			? NULL : file_info[n].name + ext_index;

		if (stat_ok == 1) {
			load_file_gral_info(&attr, n);
		} else {
			file_info[n].type = DT_UNKNOWN;
			file_info[n].stat_err = 1;
			attr.st_mode = 0;
			stats.unknown++;
			stats.unstat++;
		}

		switch (file_info[n].type) {
		case DT_DIR: load_dir_info(attr.st_mode, n); break;
		case DT_LNK: load_link_info(fd, n); break;
		case DT_REG: load_regfile_info(attr.st_mode, n); break;
		case DT_SOCK: file_info[n].color = so_c; break;
		case DT_FIFO: file_info[n].color = pi_c; break;
		case DT_BLK: file_info[n].color = bd_c; break;
		case DT_CHR: file_info[n].color = cd_c; break;
#ifdef SOLARIS_DOORS
		case DT_DOOR: file_info[n].color = oo_c; break;
		case DT_PORT: file_info[n].color = oo_c; break;
#endif /* SOLARIS_DOORS */
		case DT_UNKNOWN: file_info[n].color = no_c; break;
		default: file_info[n].color = df_c; break;
		/* For the time being, we have no specific colors for DT_ARCH1,
		 * DT_ARCH2, and DT_WHT. */
		}

		if (checks_scanning == 1 && file_info[n].dir == 1)
			print_scanned_file(file_info[n].name);

#ifndef _NO_ICONS
		if (checks_icons_use_file_color == 1)
			file_info[n].icon_color = file_info[n].color;
#endif /* !_NO_ICONS */
		/* In the case of symlinks, info has already been gathered, by
		 * load_file_gral_info (if not following symlinks), or by
		 * load_link_info otherwise. */
		if (conf_long_view == 1 && stat_ok == 1 && !S_ISLNK(attr.st_mode))
			set_long_attribs(n, &attr);

		if (xargs_disk_usage_analyzer == 1) {
			get_largest_file_info(n, &largest_name_size, &largest_name,
				&largest_color, &total_size);
		}

		n++;
		if (n > FILESN_MAX - 1) {
			err('w', PRINT_PROMPT, _("%s: Integer overflow detected "
				"(showing only %jd files)\n"), PROGRAM_NAME, (intmax_t)n);
			break;
		}

		count++;
	}

	/* Since we allocate memory by chunks, we probably allocated more
	 * than required. Let's free unused memory.
	 * Up to 18Kb can be freed this way. */
/*	filesn_t tdents = total_dents > 0 ? (filesn_t)total_dents : ENTRY_N + 2;
	if (tdents > n)
		file_info =
			xnrealloc(file_info, (size_t)n + 1, sizeof(struct fileinfo)); */

	file_info[n].name = NULL;
	g_files_num = n;

	if (checks.scanning == 1)
		erase_scanning_message();

	if (n == 0) {
		printf("%s. ..%s\n", di_c, df_c);
		free(file_info);
		goto END;
	}

	const int eln_len = conf.no_eln == 1 ? 0
		: ((conf.max_files != UNSET && g_files_num > (filesn_t)conf.max_files)
		? DIGINUM(conf.max_files) : DIGINUM(g_files_num));

		/* #############################################
		 * #    SORT FILES ACCORDING TO SORT METHOD    #
		 * ############################################# */

	if (conf.sort != SNONE)
		ENTSORT(file_info, (size_t)n, entrycmp);

		/* ##########################################
		 * #    GET INFO TO PRINT COLUMNED OUTPUT   #
		 * ########################################## */

	/* Get the longest filename. */
	if (conf.columned == 1 || conf.long_view == 1
	|| conf.pager_view != PAGER_AUTO)
		get_longest_filename(n, (size_t)eln_len);

	/* Get the number of columns required to print all filenames. */
	const size_t columns_n = (conf.pager_view == PAGER_AUTO
		&& (conf.columned == 0 || conf.long_view == 1)) ? 1 : get_columns();

	set_pager_view((filesn_t)columns_n);

				/* ########################
				 * #    LONG VIEW MODE    #
				 * ######################## */

	if (conf.long_view == 1) {
		if (prop_fields.size == PROP_SIZE_HUMAN)
			construct_human_sizes();
		print_long_mode(&reset_pager, eln_len);
	}

				/* ########################
				 * #   NORMAL VIEW MODE   #
				 * ######################## */

	else if (conf.listing_mode == VERTLIST) { /* ls(1) like listing */
		list_files_vertical(&reset_pager, eln_len, columns_n);
	} else {
		list_files_horizontal(&reset_pager, eln_len, columns_n);
	}

				/* #########################
				 * #   POST LISTING STUFF  #
				 * ######################### */

END:
	if (hidden_list)
		free_dothidden(&hidden_list);

	exit_code =
		post_listing(close_dir == 1 ? dir : NULL, reset_pager, autocmd_ret);

	if (xargs.disk_usage_analyzer == 1 && conf.long_view == 1
	&& conf.full_dir_size == 1) {
		print_analysis_stats(total_size, largest_name_size,
			largest_color, largest_name);
	}

#ifdef LIST_SPEED_TEST
	clock_gettime(CLOCK_MONOTONIC, &t2);
	double secs = (double)(t2.tv_sec - t1.tv_sec)
		+ (double)(t2.tv_nsec - t1.tv_nsec) * 1e-9;
	printf("list_dir time: %f\n", secs);
#endif /* LIST_SPEED_TEST */

	return exit_code;
}

void
free_dirlist(void)
{
	if (!file_info || g_files_num == 0)
		return;

	filesn_t i = g_files_num;
	while (--i >= 0) {
		free(file_info[i].name);
		free(file_info[i].ext_color);
	}

	free(file_info);
	file_info = NULL;
}

void
reload_dirlist(void)
{
#ifdef RUN_CMD
	if (cmd_line_cmd)
		return;
#endif /* RUN_CMD */

	free_dirlist();
	const int bk = exit_code;
	list_dir();
	exit_code = bk;
}

void
refresh_screen(void)
{
	if (conf.autols == 0) {
		CLEAR;
		return;
	}

	const int bk = conf.clear_screen;
	conf.clear_screen = 1;
	reload_dirlist();
	conf.clear_screen = bk;
}
