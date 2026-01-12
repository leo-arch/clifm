/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* xdu.c -- Trimmed down implementation of du(1) */

#include "helpers.h"

#include <errno.h>
#ifdef USE_DU1
# include <string.h> /* strchr */
# include <unistd.h> /* close, dup, dup2, unlink, unlinkat */
#endif /* USE_DU1 */

#ifdef USE_DU1
# include "aux.h"   /* xnrealloc, open_fread */
# include "spawn.h" /* launch_execv */
#else
# include "mem.h"   /* xnrealloc */
#endif /* USE_DU1 */

/* According to 'info du', the st_size member of a stat struct is meaningful
 * only:
 * 1. When computing disk usage (not apparent sizes).
 * 2. If apparent sizes, only for symlinks and regular files.
 * NOTE: Here we add shared memory object and typed memory object just to
 * match the check made by du(1). These objects are not implemented on most
 * systems, but this might change in the future. */
#define USABLE_ST_SIZE(s) (conf.apparent_size != 1 || S_ISLNK((s)->st_mode) \
		|| S_ISREG((s)->st_mode) || S_TYPEISSHM((s)) || S_TYPEISTMO((s)))

struct hlink_t {
	dev_t dev;
	ino_t ino;
};

static struct hlink_t *xdu_hardlinks = {0};
static size_t xdu_hardlink_n = 0;

static inline int
check_xdu_hardlinks(const dev_t dev, const ino_t ino)
{
	if (!xdu_hardlinks || xdu_hardlink_n == 0)
		return 0;

	size_t i;
	for (i = 0; i < xdu_hardlink_n; i++) {
		if (dev == xdu_hardlinks[i].dev && ino == xdu_hardlinks[i].ino)
			return 1;
	}

	return 0;
}

static inline void
add_xdu_hardlink(const dev_t dev, const ino_t ino)
{
	xdu_hardlinks = xnrealloc(xdu_hardlinks, xdu_hardlink_n + 1,
		sizeof(struct hlink_t));

	xdu_hardlinks[xdu_hardlink_n].dev = dev;
	xdu_hardlinks[xdu_hardlink_n].ino = ino;
	xdu_hardlink_n++;
}

static inline void
free_xdu_hardlinks(void)
{
	free(xdu_hardlinks);
	xdu_hardlinks = (struct hlink_t *)NULL;
	xdu_hardlink_n = 0;
}

/* Trimmed down implementation of du(1) providing only those features
 * required by Clifm.
 *
 * Recursively count files and directories in the directory DIR and store
 * values in the INFO struct.
 *
 * The total size in bytes is stored in the SIZE field of the struct, and
 * the total number of used blocks in the BLOCKS field.
 * Translate this info into apparent and physical sizes of DIR as follows:
 *   apparent = info->size (same as 'du -s -B1 --apparent-size')
 *   physical = info->blocks * S_BLKSIZE (same as 'du -s -B1')
 *
 * The number of directories, symbolic links, and other file types is stored
 * in the DIRS, LINKS, and FILES fields respectively.
 * FIRST_LEVEL must be always 1 when calling this function (this value will
 * be zero whenever the function calls itself recursively).
 * If a directory cannot be read, or a file cannot be stat'ed, then the
 * STATUS field of the INFO struct is set to the appropriate errno value. */
void
dir_info(const char *dir, const int first_level, struct dir_info_t *info)
{
	if (!dir || !*dir) {
		info->status = ENOENT;
		return;
	}

	struct stat a;
	DIR *p;

	if ((p = opendir(dir)) == NULL) {
		info->status = errno;
		return;
	}

#ifdef POSIX_FADV_SEQUENTIAL
	/* A hint to the kernel to optimize the current dir for reading. */
	const int fd = dirfd(p);
	posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
#endif /* POSIX_FADV_SEQUENTIAL */

	/* Compute the PHYSICAL size of the base directory itself. */
	if (first_level == 1 && stat(dir, &a) != -1)
		info->blocks += a.st_blocks;

	struct dirent *ent;
	char buf[PATH_MAX + 1];

	while ((ent = readdir(p)) != NULL) {
		if (SELFORPARENT(ent->d_name))
			continue;

		snprintf(buf, sizeof(buf), "%s/%s", dir, ent->d_name);

		if (lstat(buf, &a) == -1) {
			info->status = errno;
#ifdef _DIRENT_HAVE_D_TYPE
			/* We cannot extract the file type from st_mode. Let's fallback
			 * to whatever d_type says. */
			switch (ent->d_type) {
			case DT_LNK: info->links++; break;
			case DT_DIR: info->dirs++; break;
			default: info->files++; break;
			}
#else
			info->files++;
#endif /* _DIRENT_HAVE_D_TYPE */
			continue;
		}

		if (S_ISLNK(a.st_mode)) {
			info->links++;
#ifdef __CYGWIN__
		/* This is because on Cygwin systems some regular files, maybe due to
		 * some permissions issue, are otherwise taken as directories. */
		} else if (S_ISREG(a.st_mode)) {
			info->files++;
#endif /* __CYGWIN__ */
		} else if (S_ISDIR(a.st_mode)) {
			/* Even if a subdirectory is unreadable or we can't chdir into
			 * it, do let its PHYSICAL size contribute to the total
			 * (provided we're not computing apparent sizes). */
			info->blocks += a.st_blocks;

			info->dirs++;
			dir_info(buf, 0, info);

			continue;
		} else {
			info->files++;
		}

		if (!USABLE_ST_SIZE(&a))
			continue;

		if (a.st_nlink > 1) {
			if (check_xdu_hardlinks(a.st_dev, a.st_ino) == 1)
				continue;
			else
				add_xdu_hardlink(a.st_dev, a.st_ino);
		}

		info->size += a.st_size;
		info->blocks += a.st_blocks;
	}

	closedir(p);

	if (first_level == 1)
		free_xdu_hardlinks();
}

#ifndef USE_DU1
off_t
dir_size(const char *dir, const int first_level, int *status)
{
	struct dir_info_t info = {0};
	dir_info(dir, first_level, &info);
	*status = info.status;
	return (conf.apparent_size == 1 ? info.size : (info.blocks * S_BLKSIZE));
}
#else /* USE_DU1 */
/* Return the full size of the directory DIR using du(1).
 * The size is reported in bytes if SIZE_IN_BYTES is set to 1.
 * Otherwise, human format is used.
 * STATUS is updated to the command exit code. */
off_t
dir_size(char *dir, const int size_in_bytes, int *status)
{
	if (!dir || !*dir)
		return (-1);

	char file[PATH_MAX + 1];
	snprintf(file, sizeof(file), "%s/%s", xargs.stealth_mode == 1
		? P_tmpdir : tmp_dir, TMP_FILENAME);

	int fd = mkstemp(file);
	if (fd == -1)
		return (-1);

	int stdout_bk = dup(STDOUT_FILENO); /* Save original stdout */
	if (stdout_bk == -1) {
		unlinkat(XAT_FDCWD, file, 0);
		close(fd);
		return (-1);
	}

	/* Redirect stdout to the desired file */
	dup2(fd, STDOUT_FILENO);
	close(fd);

	if (bin_flags & (GNU_DU_BIN_DU | GNU_DU_BIN_GDU)) {
		char *block_size = (char *)NULL;

		if (size_in_bytes == 1) {
			block_size = "--block-size=1";
		} else {
			if (xargs.si == 1)
				block_size = "--block-size=KB";
			else
				block_size = "--block-size=K";
		}

		char *bin = (bin_flags & GNU_DU_BIN_DU) ? "du" : "gdu";
		if (conf.apparent_size != 1) {
			char *cmd[] = {bin, "-s", block_size, "--", dir, NULL};
			*status = launch_execv(cmd, FOREGROUND, E_NOSTDERR);
		} else {
			char *cmd[] = {bin, "-s", "--apparent-size", block_size,
				"--", dir, NULL};
			*status = launch_execv(cmd, FOREGROUND, E_NOSTDERR);
		}
	} else {
		char *cmd[] = {"du", "-ks", "--", dir, NULL};
		*status = launch_execv(cmd, FOREGROUND, E_NOSTDERR);
	}

	dup2(stdout_bk, STDOUT_FILENO); /* Restore original stdout */
	close(stdout_bk);

	FILE *fp = open_fread(file, &fd);
	if (!fp) {
		unlinkat(XAT_FDCWD, file, 0);
		return (-1);
	}

	off_t retval = -1;
	/* We only need here the first field of the line, which is a file
	 * size and usually takes only a few digits: since a yottabyte takes 26
	 * digits, MAX_INT_STR (32) is more than enough. */
	char line[MAX_INT_STR]; *line = '\0';
	if (fgets(line, (int)sizeof(line), fp) == NULL)
		goto END;

	char *p = strchr(line, '\t');
	if (p && p != line) {
		*p = '\0';
		retval = (off_t)atoll(line);
	}

END:
	unlinkat(XAT_FDCWD, file, 0);
	fclose(fp);
	return retval;
}
#endif /* !USE_DU1 */
