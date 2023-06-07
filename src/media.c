/* media.c -- functions to manage local filesystems */

/*
 * This file is part of CliFM
 * 
 * Copyright (C) 2016-2023, L. Abramovich <leo.clifm@outlook.com>
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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) \
|| defined(__DragonFly__)
# include <sys/mount.h>
# include <sys/sysctl.h>
#elif defined(__APPLE__)
# include <sys/param.h>
# include <sys/ucred.h>
# include <sys/mount.h>
#endif
#include <dirent.h>

#include "aux.h"
#include "readline.h"
#include "navigation.h"
#include "exec.h"
#include "listing.h"
#include "jump.h"
#include "misc.h"
#include "history.h"

#if defined(__linux__) || defined(__CYGWIN__)
# define HAVE_PROC_MOUNTS
# define DISK_LABELS_PATH "/dev/disk/by-label"
#endif

/* Information about devices */
struct mnt_t {
	char *mnt; /* Mountpoint */
	char *dev; /* Device name (ex: /dev/sda1) */
	char *label; /* Device label */
};

static struct mnt_t *media = (struct mnt_t *)NULL;
static size_t mp_n = 0;

/*
#ifdef __linux__
static struct udev_device*
get_child(struct udev* udev, struct udev_device* parent, const char* subsystem)
{
	struct udev_device* child = NULL;
	struct udev_enumerate *enumerate = udev_enumerate_new(udev);

	udev_enumerate_add_match_parent(enumerate, parent);
	udev_enumerate_add_match_subsystem(enumerate, subsystem);
	udev_enumerate_scan_devices(enumerate);

	struct udev_list_entry *devices = udev_enumerate_get_list_entry(enumerate);
	struct udev_list_entry *entry;

	udev_list_entry_foreach(entry, devices) {
		const char *path = udev_list_entry_get_name(entry);
		child = udev_device_new_from_syspath(udev, path);
		break;
	}

	udev_enumerate_unref(enumerate);
	return child;
}

// Return an array of block devices partitions
static char **
get_block_devices(void)
{
	struct udev* udev = udev_new();
	struct udev_enumerate* enumerate = udev_enumerate_new(udev);

	udev_enumerate_add_match_property(enumerate, "DEVTYPE", "partition");
	udev_enumerate_scan_devices(enumerate);

	struct udev_list_entry *devices = udev_enumerate_get_list_entry(enumerate);
	struct udev_list_entry *entry;

	char **mps = (char **)NULL;
	size_t n = 0;

	udev_list_entry_foreach(entry, devices) {
	const char* path = udev_list_entry_get_name(entry);
	struct udev_device* scsi = udev_device_new_from_syspath(udev, path);

	struct udev_device* block = get_child(udev, scsi, "block");

	const char *dev = udev_device_get_devnode(block);
	mps = (char **)xrealloc(mps, (n + 2) * sizeof(char *));
	mps[n++] = savestring(dev, strlen(dev));

	if (block)
		udev_device_unref(block);

	udev_device_unref(scsi);
	}
	mps[n] = (char *)NULL;

	udev_enumerate_unref(enumerate);
	udev_unref(udev);

	return mps;
}
#endif // __linux__
*/

#ifdef HAVE_PROC_MOUNTS
static char **
get_block_devices(void)
{
	struct dirent **blockdev = (struct dirent **)NULL;
	int block_n = scandir("/dev", &blockdev, NULL, alphasort);
	if (block_n == - 1)
		return (char **)NULL;

	char **bd = (char **)NULL;
	size_t i, n = 0;

	for (i = 0; (int)i < block_n; i++) {
# ifndef _DIRENT_HAVE_D_TYPE
		char bpath[PATH_MAX];
		snprintf(bpath, sizeof(bpath), "/dev/%s", blockdev[i]->d_name);
		struct stat a;
		if (stat(bpath, &a) == -1) {
			free(blockdev[i]);
			continue;
		}
		if (!S_ISBLK(a.st_mode)) {
# else
		if (blockdev[i]->d_type != DT_BLK) {
# endif /* !_DIRENT_HAVE_D_TYPE */
			free(blockdev[i]);
			continue;
		}

		char *name = blockdev[i]->d_name;

		/* Skip /dev/ram and /dev/loop devices */
		if ((*name == 'l' && strncmp(name, "loop", 4) == 0)
		|| (*name == 'r' && strncmp(name, "ram", 3) == 0)) {
			free(blockdev[i]);
			continue;
		}

		/* Get only partition names, normally ending with a number */
		size_t blen = strlen(name);
		if (blen > 0 && (name[blen - 1] < '1' || name[blen - 1] > '9')) {
			free(blockdev[i]);
			continue;
		}

		bd = (char **)xrealloc(bd, (n + 2) * sizeof(char *));
		bd[n] = (char *)xnmalloc(blen + 6, sizeof(char *));
		sprintf(bd[n], "/dev/%s", name);
		n++;
		bd[n] = (char *)NULL;
		free(blockdev[i]);
	}

	free(blockdev);
	return bd;
}

static int
unmount_dev(size_t i, const int n)
{
	if (xargs.mount_cmd == UNSET) {
		xerror(_("%s: No mount application found. Install either "
			"udevil or udisks2\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	if ((unsigned int)n + (unsigned int)1 < (unsigned int)1 || n + 1 > (int)i) {
		xerror(_("%s: %d: Invalid ELN\n"), PROGRAM_NAME, n + 1);
		return EXIT_FAILURE;
	}

	char *mnt = media[n].mnt;
	int exit_status = EXIT_SUCCESS;

	/* Get out of mountpoint before unmounting */
	size_t mlen = strlen(mnt);
	if (strncmp(mnt, workspaces[cur_ws].path, mlen) == 0) {
		char *cmd[] = {"b", NULL};
		if (back_function(cmd) == EXIT_FAILURE)
			cd_function(NULL, CD_PRINT_ERROR);
		exit_status = (-1);
	}

	char *cmd[] = {xargs.mount_cmd == MNT_UDISKS2 ? "udisksctl" : "udevil",
			"unmount", "-b", media[n].dev, NULL};
	if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
		exit_status = EXIT_FAILURE;

	if (exit_status != EXIT_FAILURE && xargs.mount_cmd == MNT_UDEVIL)
		printf(_("%s: Unmounted %s\n"), PROGRAM_NAME, media[n].dev);

	return exit_status;
}

static char *
get_dev_label(void)
{
	size_t n = mp_n;
	struct dirent **labels = (struct dirent **)NULL;
	int ln = scandir(DISK_LABELS_PATH, &labels, NULL, alphasort);
	if (ln == - 1)
		return (char *)NULL;

	char *label = (char *)NULL;
	int i;

	for (i = 0; i < ln; i++) {
		if (label) {
			free(labels[i]);
			continue;
		}

		char *name = labels[i]->d_name;
		char lpath[PATH_MAX];
		snprintf(lpath, sizeof(lpath), "%s/%s", DISK_LABELS_PATH, name);
		char *rpath = realpath(lpath, NULL);
		if (!rpath) {
			free(labels[i]);
			continue;
		}

		int ret = strcmp(rpath, media[n].dev);
		free(rpath);
		if (ret == 0) {
			/* Device label is encoded using hex. Let's decode it */
			char *p = strchr(name, '\\');
			if (p && *(p + 1) == 'x') {
				char pp = 0;
				pp = (char)(from_hex(*(p + 2)) << 4 | from_hex(*(p + 3)));
				*p = pp;
				strcpy(p + 1, p + 4);
			}
			label = savestring(name, strlen(name));
		}

		free(labels[i]);
	}

	free(labels);
	return label;
}

static void
list_unmounted_devs(void)
{
	int i;
	size_t k = mp_n;
	char **unm_devs = (char **)NULL;
	unm_devs = get_block_devices();

	if (!unm_devs)
		return;

	printf(_("\n%sUnmounted devices%s\n\n"), BOLD, df_c);

	for (i = 0; unm_devs[i]; i++) {
		int skip = 0;
		size_t j = 0;
		// Skip already mounted devices
		for (; j < k; j++) {
			if (strcmp(media[j].dev, unm_devs[i]) == 0)
				skip = 1;
		}
		if (skip) {
			free(unm_devs[i]);
			continue;
		}

		media = (struct mnt_t *)xrealloc(media,
			    (mp_n + 2) * sizeof(struct mnt_t));
		media[mp_n].dev = savestring(unm_devs[i], strlen(unm_devs[i]));
		media[mp_n].mnt = (char *)NULL;

		media[mp_n].label = get_dev_label();

		if (media[mp_n].label)
			printf("%s%zu %s%s [%s%s%s]\n", el_c, mp_n + 1, df_c,
				media[mp_n].dev, mi_c, media[mp_n].label, df_c);
		else
			printf("%s%zu %s%s\n", el_c, mp_n + 1, df_c, media[mp_n].dev);

		mp_n++;
		free(unm_devs[i]);
	}
	free(unm_devs);

	media[mp_n].dev = (char *)NULL;
	media[mp_n].mnt = (char *)NULL;
	media[mp_n].label = (char *)NULL;
}

static int
list_mounted_devs(int mode)
{
	FILE *fp = fopen("/proc/mounts", "r");
	if (!fp) {
		xerror("mp: fopen: /proc/mounts: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	size_t line_size = 0;
	char *line = (char *)NULL;

	while (getline(&line, &line_size, fp) > 0) {
		/* List only mountpoints corresponding to a block device (/dev) */
		if (strncmp(line, "/dev/", 5) != 0)
			continue;

		/* Use strtok(3) to split LINE into tokens using space as IFS */
		char *str = strtok(line, " ");
		if (!str)
			continue;

		size_t counter = 0, dev_len = strlen(str);

		media = (struct mnt_t *)xrealloc(media, (mp_n + 2) * sizeof(struct mnt_t));
		media[mp_n].dev = savestring(str, dev_len);
		media[mp_n].label = (char *)NULL;
		/* Print only the first two fields of each /proc/mounts line */
		while (str && counter < 2) {
			if (counter == 1) { /* 1 == second field */
				/* /proc/mounts encode special chars as octal.
				 * Let's decode it */
				char *p = strchr(str, '\\');
				if (p && *(p + 1)) {
					char *q = p;
					p++;
					char pp = 0;
					p += 3;
					pp = *p;
					*p = '\0';
					int d = read_octal(q + 1);
					*p = pp;
					*q = (char)d;
					strcpy(q + 1, q + 4);
				}

				if (mode == MEDIA_LIST) {
					printf("%s%zu%s %s%s%s [%s]\n", el_c, mp_n + 1,
						df_c, (access(str, R_OK | X_OK) == 0) ? di_c : nd_c,
						str, df_c, media[mp_n].dev);
				} else {
					printf("%s%zu%s %s [%s%s%s]\n", el_c, mp_n + 1,
						df_c, media[mp_n].dev,
						(access(str, R_OK | X_OK) == 0) ? di_c
						: nd_c, str, df_c);
				}

				/* Store the second field (mountpoint) into an array */
				media[mp_n].mnt = savestring(str, strlen(str));
				mp_n++;
			}

			str = strtok(NULL, " ,");
			counter++;
		}
	}

	free(line);
	fclose(fp);

	media[mp_n].dev = (char *)NULL;
	media[mp_n].mnt = (char *)NULL;
	media[mp_n].label = (char *)NULL;

	return EXIT_SUCCESS;
}

/* Mount device and store mountpoint */
static int
mount_dev(int n)
{
	if (xargs.mount_cmd == UNSET) {
		xerror(_("%s: No mount application found. Install either "
			"udevil or udisks2\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	char file[PATH_MAX];
	snprintf(file, sizeof(file), "%s/%s", xargs.stealth_mode == 1
		? P_tmpdir : tmp_dir, TMP_FILENAME);

	int fd = mkstemp(file);
	if (fd == -1)
		return EXIT_FAILURE;

	int stdout_bk = dup(STDOUT_FILENO); /* Save original stdout */
	dup2(fd, STDOUT_FILENO); /* Redirect stdout to the desired file */
	close(fd);

	if (xargs.mount_cmd == MNT_UDISKS2) {
		char *cmd[] = {"udisksctl", "mount", "-b", media[n].dev, NULL};
		launch_execve(cmd, FOREGROUND, E_NOFLAG);
	} else {
		char *cmd[] = {"udevil", "mount", media[n].dev, NULL};
		launch_execve(cmd, FOREGROUND, E_NOFLAG);
	}

	dup2(stdout_bk, STDOUT_FILENO); /* Restore original stdout */
	close(stdout_bk);

	FILE *fp = open_fstream_r(file, &fd);
	if (!fp) {
		unlink(file);
		return EXIT_FAILURE;
	}

	char out_line[PATH_MAX];
	if (fgets(out_line, (int)sizeof(out_line), fp) == NULL) {
		/* Error is printed by the mount command itself */
		fclose(fp);
		unlink(file);
		return EXIT_FAILURE;
	}

	fclose(fp);
	unlink(file);

	/* Recover the mountpoint used by the mounting command */
	char *p = strstr(out_line, " at ");
	if (!p || *(p + 4) != '/') {
		xerror(_("%s: Error retrieving mountpoint\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}
	p += 4;

	size_t plen = strnlen(p, sizeof(out_line) - 4);
	if (plen > 0 && p[plen - 1] == '\n')
		p[plen - 1] = '\0';

	media[n].mnt = savestring(p, strnlen(p, sizeof(out_line) - 4));

	return EXIT_SUCCESS;
}
#endif /* HAVE_PROC_MOUNTS */

#ifndef __HAIKU__
static void
free_media(void)
{
	int i = (int)mp_n;
	while (--i >= 0) {
		free(media[i].mnt);
		free(media[i].dev);
		free(media[i].label);
	}
	free(media);
	media = (struct mnt_t *)NULL;
}

/* Get device information via external application */
static int
print_dev_info(int n)
{
	if (!media[n].dev || xargs.mount_cmd == UNSET)
		return EXIT_FAILURE;

	int exit_status = EXIT_SUCCESS;
	if (xargs.mount_cmd == MNT_UDEVIL) {
		char *cmd[] = {"udevil", "info", media[n].dev, NULL};
		if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;
	} else {
		char *cmd[] = {"udisksctl", "info", "-b", media[n].dev, NULL};
		if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;
	}

	return exit_status;
}

# if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) \
|| defined(__DragonFly__) || defined(__APPLE__)
static size_t
#  ifdef __NetBSD__
list_mountpoints_bsd(struct statvfs *fslist)
#  else
list_mountpoints_bsd(struct statfs *fslist)
#  endif /* __NetBSD__ */
{
	size_t i, j = 0;

	for (i = 0; i < mp_n; i++) {
		// Do not list all mountpoints, but only those corresponding
		// to a block device (/dev)
		if (strncmp(fslist[i].f_mntfromname, "/dev/", 5) != 0)
			continue;

		printf("%s%zu%s %s%s%s (%s)\n", el_c, j + 1, df_c,
		    (access(fslist[i].f_mntonname, R_OK | X_OK) == 0)
		    ? di_c : nd_c, fslist[i].f_mntonname,
		    df_c, fslist[i].f_mntfromname);

		/* Store the mountpoint into the mounpoints struct */
		media = (struct mnt_t *)xrealloc(media, (j + 2) * sizeof(struct mnt_t));
		media[j].mnt = savestring(fslist[i].f_mntonname,
			strlen(fslist[i].f_mntonname));
		media[j].label = (char *)NULL;
		media[j].dev = (char *)NULL;
		j++;
	}

	media[j].dev = (char *)NULL;
	media[j].mnt = (char *)NULL;
	media[j].label = (char *)NULL;

	return j;
}
# endif /* BSD || APPLE */

static int
get_mnt_input(const int mode, int *info)
{
	int n = -1;
	/* Ask the user and mount/unmount or chdir into the selected
	 * device/mountpoint */
	puts(_("Enter 'q' to quit"));
	if (xargs.mount_cmd != UNSET)
		puts(_("Enter 'iELN' for device information. Ex: i4"));

	char *input = (char *)NULL;
	while (!input) {
# ifdef HAVE_PROC_MOUNTS
		if (mode == MEDIA_LIST)
			input = rl_no_hist(_("Choose a mountpoint: "));
		else
			input = rl_no_hist(_("Choose a mountpoint/device: "));
# else
		input = rl_no_hist(_("Choose a mountpoint: "));
# endif /* HAVE_PROC_MOUNTS */
		if (!input || !*input) {
			free(input);
			input = (char *)NULL;
			continue;
		}

		if (*input == 'q' && *(input + 1) == '\0') {
			if (conf.autols == 1)
				reload_dirlist();
			free(input);
			return (-1);
		}

		char *p = input;
		if (*p == 'i') {
			*info = 1;
			++p;
		}

		int atoi_num = atoi(p);
		if (atoi_num <= 0 || atoi_num > (int)mp_n) {
			xerror("%s: %s: Invalid ELN\n", PROGRAM_NAME, input);
			free(input);
			input = (char *)NULL;
			continue;
		}

		n = atoi_num - 1;
	}

	free(input);
	return n;
}

static int
print_mnt_info(const int n)
{
	int exit_status = print_dev_info(n);

	if (exit_status == EXIT_SUCCESS) {
		printf("\nPress any key to continue... ");
		xgetchar();
		putchar('\n');
	}

	free_media();
	return exit_status;
}
#endif /* !__HAIKU__ */

/* If MODE is MEDIA_MOUNT (used by the 'media' command) list mounted and
 * unmounted devices allowing the user to mount or unmount any of them.
 * If MODE is rather MEDIA_LIST (used by the 'mp' command), just list
 * available mountpoints and allow the user to cd into the selected one */
int
media_menu(const int mode)
{
#if defined(__HAIKU__)
	xerror(_("%s: This feature is not available on Haiku\n"),
		mode == MEDIA_LIST ? _("mountpoints") : _("media"));
	return EXIT_FAILURE;
#endif /* __HAIKU__ */

#ifndef HAVE_PROC_MOUNTS
	if (mode == MEDIA_MOUNT) {
		xerror("%s\n", _("media: Function only available on Linux systems"));
		return EXIT_FAILURE;
	}
#endif /* HAVE_PROC_MOUNTS */

	if (mode == MEDIA_MOUNT && xargs.mount_cmd == UNSET) {
		xerror("%s\n", _("media: No mount command found. Install either "
			"udevil or udisks2"));
		return EXIT_FAILURE;
	}

#ifdef HAVE_PROC_MOUNTS
	printf("%s%s%s\n\n", BOLD, mode == MEDIA_LIST ? _("Mountpoints")
			: _("Mounted devices"), df_c);
#else
	printf(_("%sMountpoints%s\n\n"), BOLD, df_c);
#endif /* HAVE_PROC_MOUNTS */

	media = (struct mnt_t *)xnmalloc(1, sizeof(struct mnt_t));
	mp_n = 0;

#ifdef HAVE_PROC_MOUNTS
	if (list_mounted_devs(mode) == EXIT_FAILURE) {
		free(media);
		media = (struct mnt_t *)NULL;
		return EXIT_FAILURE;
	}

	size_t mp_n_bk = mp_n;

	if (mode == MEDIA_MOUNT)
		list_unmounted_devs();

#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__APPLE__) \
|| defined(__DragonFly__)
	struct statfs *fslist;
	mp_n = (size_t)getmntinfo(&fslist, MNT_NOWAIT);
#elif defined(__NetBSD__)
	struct statvfs *fslist;
	mp_n = (size_t)getmntinfo(&fslist, MNT_NOWAIT);
#endif /* HAVE_PROC_MOUNTS */

	/* This should never happen: There should always be a mountpoint,
	 * at least "/" */
	// cppcheck-suppress knownConditionTrueFalse
	if (mp_n == 0) {
#ifdef HAVE_PROC_MOUNTS
		printf(_("%s: There are no available %s\n"), mode == MEDIA_LIST ? "mp"
			: "media", mode == MEDIA_LIST ? _("mountpoints") : _("devices"));
#else
		fputs(_("mp: There are no available mountpoints\n"), stdout);
#endif /* HAVE_PROC_MOUNTS */
		return EXIT_SUCCESS;
	}

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) \
|| defined(__APPLE__) || defined(__DragonFly__)
	mp_n = list_mountpoints_bsd(fslist);
#endif /* BSD || APPLE */

	putchar('\n');
	int info = 0;
	int n = get_mnt_input(mode, &info);

	int exit_status = EXIT_SUCCESS;
	if (n == -1)
		goto EXIT;

	if (info == 1) {
		exit_status = print_mnt_info(n);
		media_menu(mode);
		return exit_status;
	}

#ifdef HAVE_PROC_MOUNTS
	if (mode == MEDIA_MOUNT) {
		if (!media[n].mnt) {
			/* The device is unmounted: mount it */
			if (mount_dev(n) == EXIT_FAILURE) {
				exit_status = EXIT_FAILURE;
				goto EXIT;
			}
		} else {
			/* The device is mounted: unmount it */
			int ret = unmount_dev(mp_n_bk, n);
			if (ret == EXIT_FAILURE)
				exit_status = EXIT_FAILURE;
			goto EXIT;
		}
	}
#endif /* HAVE_PROC_MOUNTS */

	if (xchdir(media[n].mnt, SET_TITLE) != EXIT_SUCCESS) {
		xerror("%s: %s: %s\n", PROGRAM_NAME, media[n].mnt, strerror(errno));
		exit_status = EXIT_FAILURE;
		goto EXIT;
	}

	free(workspaces[cur_ws].path);
	workspaces[cur_ws].path = savestring(media[n].mnt, strlen(media[n].mnt));

	if (conf.autols == 1)
		reload_dirlist();

	add_to_dirhist(workspaces[cur_ws].path);
	add_to_jumpdb(workspaces[cur_ws].path);

EXIT:
	free_media();
	return exit_status;
}
