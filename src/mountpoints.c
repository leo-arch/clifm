/* mountpoints.c -- functions to manage local filesystems */

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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/mount.h>
#include <sys/sysctl.h>
#endif
#include <dirent.h>

#include "aux.h"
#include "readline.h"
#include "navigation.h"
#include "exec.h"
#include "listing.h"
#include "jump.h"
#include "checks.h"
#include "history.h"

struct mnt_t {
	char *mnt;
	char *dev;
	char *label;
};

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

#ifdef __linux__
static char **
get_block_devices(void)
{
	struct dirent **blockdev = (struct dirent **)NULL;
	int block_n = scandir("/dev", &blockdev, NULL, alphasort);
	if (block_n == - 1)
		return (char **)NULL;

	char **bd = (char **)NULL;
	size_t i = 0, n = 0;
	for (; (int)i < block_n; i++) {
#ifndef DIRENT_HAVE_D_TYPE
		char bpath[PATH_MAX];
		snprintf(bpath, PATH_MAX, "/dev/%s", blockdev[i]->d_name);
		struct stat a;
		if (stat(bpath, &a) == -1) {
			free(blockdev[i]);
			continue;
		}
		if (!S_ISBLK(a.st_mode)) {
#else
		if (blockdev[i]->d_type != DT_BLK) {
#endif // !DIRENT_HAVE_D_TYPE
			free(blockdev[i]);
			continue;
		}

		/* Skip /dev/ram and /dev/loop devices */
		if ((*blockdev[i]->d_name == 'l'
		&& strncmp(blockdev[i]->d_name, "loop", 4) == 0)
		|| (*blockdev[i]->d_name == 'r'
		&& strncmp(blockdev[i]->d_name, "ram", 3) == 0)) {
			free(blockdev[i]);
			continue;
		}

		size_t blen = strlen(blockdev[i]->d_name);
		if (blockdev[i]->d_name[blen - 1] < '1'
		|| blockdev[i]->d_name[blen - 1] > '9') {
			free(blockdev[i]);
			continue;
		}

		bd = (char **)xrealloc(bd, (n + 2) * sizeof(char *));
		bd[n] = (char *)xnmalloc(blen + 6, sizeof(char *));
		sprintf(bd[n], "/dev/%s", blockdev[i]->d_name);
		bd[++n] = (char *)NULL;
		free(blockdev[i]);
	}

	free(blockdev);
	return bd;
}

static int
unmount_dev(struct mnt_t *mountpoints, size_t i)
{
	char *input = (char *)NULL;
	char msg[64];
	snprintf(msg, 64, _("Choose mountpoint to be unmounted ('q' to quit) [1-%zu]: "), i);
	while (!input)
		input = rl_no_hist(msg);

	if (*input == 'q' && !*(input + 1)) {
		free(input);
		return EXIT_SUCCESS;
	}

	if (!is_number(input)) {
		fprintf(stderr, _("%s: %s: Invalid ELN\n"), PROGRAM_NAME, input);
		free(input);
		return EXIT_FAILURE;
	}

	int am = atoi(input);
	free(input);
	if (am < 1 || am > (int)i) {
		fprintf(stderr, _("%s: %d: Invalid ELN\n"), PROGRAM_NAME, am);
		return EXIT_FAILURE;
	}

	/* Get out of mountpoint before unmounting */
	size_t mlen = strlen(mountpoints[am - 1].mnt);
	if (strncmp(mountpoints[am - 1].mnt, ws[cur_ws].path, mlen) == 0) {
		if (mountpoints[am - 1].mnt[mlen - 1] == '/')
			mountpoints[am - 1].mnt[mlen - 1] = '\0';
		char *p = strrchr(mountpoints[am - 1].mnt, '/');
		if (!p) {
			fprintf(stderr, _("%s: %s: Error getting parent directory\n"),
					PROGRAM_NAME, mountpoints[am - 1].mnt);
			return EXIT_FAILURE;
		}

		*p = '\0';
		errno = 0;
		if (xchdir(mountpoints[am - 1].mnt, SET_TITLE) != EXIT_SUCCESS) {
			*p = '/';
			fprintf(stderr, "%s: %s: %s", PROGRAM_NAME, mountpoints[am - 1].mnt,
					strerror(errno));
			return EXIT_FAILURE;
		}

		free(ws[cur_ws].path);
		ws[cur_ws].path = savestring(mountpoints[am - 1].mnt,
						strlen(mountpoints[am - 1].mnt));
		*p = '/';
		add_to_dirhist(ws[cur_ws].path);
		add_to_jumpdb(ws[cur_ws].path);
	}

	char *cmd[] = {"udisksctl", "unmount", "-b", mountpoints[am - 1].dev, NULL};
	if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

static char *
get_dev_label(struct mnt_t *mountpoints, size_t n)
{
#define DISK_LABELS_PATH "/dev/disk/by-label"

	struct dirent **labels = (struct dirent **)NULL;
	int ln = scandir(DISK_LABELS_PATH, &labels, NULL, alphasort);
	if (ln == - 1)
		return (char *)NULL;

	char *label = (char *)NULL;
	int i = 0;
	for (; i < ln; i++) {
		if (label) {
			free(labels[i]);
			continue;
		}

		char lpath[PATH_MAX];
		snprintf(lpath, PATH_MAX, "%s/%s", DISK_LABELS_PATH, labels[i]->d_name);
		char *rpath = realpath(lpath, NULL);
		if (!rpath) {
			free(labels[i]);
			continue;
		}

		int ret = strcmp(rpath, mountpoints[n].dev);
		free(rpath);
		if (ret == 0)
			label = savestring(labels[i]->d_name, strlen(labels[i]->d_name));

		free(labels[i]);
	}

	free(labels);
	return label;
}
#endif // __linux__

/* List available mountpoints and chdir into one of them */
int
list_mountpoints(void)
{
#if defined(__HAIKU__)
	fprintf(stderr, "%s: Mountpoints: This feature is not available on Haiku\n",
			PROGRAM_NAME);
	return EXIT_FAILURE;
#endif

#if defined(__linux__)
	FILE *mp_fp = fopen("/proc/mounts", "r");
	if (!mp_fp) {
		fprintf(stderr, "%s: mp: fopen: /proc/mounts: %s\n",
				PROGRAM_NAME, strerror(errno));
		return EXIT_FAILURE;
	}
#endif

	printf(_("%sMountpoints%s\n\n"), BOLD, df_c);

	struct mnt_t *mountpoints = (struct mnt_t *)NULL;
	size_t mp_n = 0;
	int i, exit_status = EXIT_SUCCESS;

#ifdef __linux__
	size_t line_size = 0;
	char *line = (char *)NULL;

	while (getline(&line, &line_size, mp_fp) > 0) {
		/* Do not list all mountpoints, but only those corresponding
		 * to a block device (/dev) */
		if (strncmp(line, "/dev/", 5) == 0) {
			char *str = (char *)NULL;
			size_t counter = 0;

			/* use strtok() to split LINE into tokens using space as
			 * IFS */
			str = strtok(line, " ");
			size_t dev_len = strlen(str);

			mountpoints = (struct mnt_t *)xrealloc(mountpoints,
				    (mp_n + 2) * sizeof(struct mnt_t));
			mountpoints[mp_n].dev = savestring(str, dev_len);
			mountpoints[mp_n].label = (char *)NULL;
			/* Print only the first two fileds of each /proc/mounts
			 * line */
			while (str && counter < 2) {
				if (counter == 1) { /* 1 == second field */
					printf("%s%zu%s %s%s%s (%s)\n", el_c, mp_n + 1,
					    df_c, (access(str, R_OK | X_OK) == 0) ? di_c : nd_c,
					    str, df_c, mountpoints[mp_n].dev);
					/* Store the second field (mountpoint) into an
					 * array */
//					mountpoints = (mnt_t **)xrealloc(mountpoints,
//					    (mp_n + 2) * sizeof(char *));
//					mountpoints[mp_n++] = savestring(str, strlen(str));
					mountpoints[mp_n++].mnt = savestring(str, strlen(str));
				}

				str = strtok(NULL, " ,");
				counter++;
			}

//			free(device);
		}
	}

	free(line);
	line = (char *)NULL;
	fclose(mp_fp);

	mountpoints[mp_n].dev = (char *)NULL;
	mountpoints[mp_n].mnt = (char *)NULL;
	mountpoints[mp_n].label = (char *)NULL;

	size_t k = mp_n;

	/* Now list unmounted devices */
	char **unm_devs = get_block_devices();
	if (unm_devs) {
		printf(_("\n%sUnmounted devices%s\n\n"), BOLD, df_c);
		i = 0;
		for (; unm_devs[i]; i++) {
			int skip = 0;
			size_t j = 0;
			// Skip already mounted devices
			for (; j < k; j++) {
				if (strcmp(mountpoints[j].dev, unm_devs[i]) == 0)
					skip = 1;
			}
			if (skip) {
				free(unm_devs[i]);
				continue;
			}

			mountpoints = (struct mnt_t *)xrealloc(mountpoints,
				    (mp_n + 2) * sizeof(struct mnt_t));
			mountpoints[mp_n].dev = savestring(unm_devs[i], strlen(unm_devs[i]));
			mountpoints[mp_n].mnt = (char *)NULL;
#ifdef __linux__
			mountpoints[mp_n].label = get_dev_label(mountpoints, mp_n);
#else
			mountpoints[mp_n].label = (char *)NULL;
#endif // __linux__
			if (mountpoints[mp_n].label)
				printf("%s%zu %s%s (%s)\n", el_c, mp_n + 1, df_c,
						mountpoints[mp_n].dev, mountpoints[mp_n].label);
			else
				printf("%s%zu %s%s\n", el_c, mp_n + 1, df_c, mountpoints[mp_n].dev);
			mp_n++;
			free(unm_devs[i]);
		}
		free(unm_devs);

		mountpoints[mp_n].dev = (char *)NULL;
		mountpoints[mp_n].mnt = (char *)NULL;
		mountpoints[mp_n].label = (char *)NULL;
	}


#elif defined(__FreeBSD__) || defined(__OpenBSD__)
	struct statfs *fslist;
	mp_n = (size_t)getmntinfo(&fslist, MNT_NOWAIT);
#elif defined(__NetBSD__)
	struct statvfs *fslist;
	mp_n = (size_t)getmntinfo(&fslist, MNT_NOWAIT);
#endif /* __linux__ */

	/* This should never happen: There should always be a mountpoint,
	 * at least "/" */
	// cppcheck-suppress knownConditionTrueFalse
	if (mp_n == 0) {
		fputs(_("mp: There are no available mountpoints\n"), stdout);
		return EXIT_SUCCESS;
	}
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
	int j;
	for (i = j = 0; i < (int)mp_n; i++) {
		/* Do not list all mountpoints, but only those corresponding
		 * to a block device (/dev) */
		if (strncmp(fslist[i].f_mntfromname, "/dev/", 5) == 0) {
			printf("%s%d%s %s%s%s (%s)\n", el_c, j + 1, df_c,
			    (access(fslist[i].f_mntonname, R_OK | X_OK) == 0)
			    ? di_c : nd_c, fslist[i].f_mntonname,
			    df_c, fslist[i].f_mntfromname);
			/* Store the mountpoint into the mounpoints struct */
			mountpoints = (struct mnt_t *)xrealloc(mountpoints,
				    (j + 2) * sizeof(struct mnt_t));
			mountpoints[j].mnt = savestring(fslist[i].f_mntonname,
					strlen(fslist[i].f_mntonname));
			mountpoints[j].label = (char *)NULL;
			mountpoints[j++].dev = (char *)NULL;

/*			mountpoints = (char **)xrealloc(mountpoints,
			    (size_t)(j + 1) * sizeof(char *));
			mountpoints[j++] = savestring(fslist[i].f_mntonname,
			    strlen(fslist[i].f_mntonname)); */
		}
	}

	mountpoints[j].dev = (char *)NULL;
	mountpoints[j].mnt = (char *)NULL;
	mountpoints[j].label = (char *)NULL;
	/* Update filesystem counter as it would be used to free() the
	 * mountpoints entries later (below) */
	mp_n = (size_t)j;
#endif

	putchar('\n');
	/* Ask the user and chdir into the selected mountpoint */
#ifdef __linux__
	puts(_("Enter 'q' to quit and 'u' to unmount"));
#endif

	char *input = (char *)NULL;
	while (!input)
#ifdef __linux__
		input = rl_no_hist(_("Choose a mountpoint/device: "));
#else
		input = rl_no_hist(_("Choose a mountpoint ('q' to quit): "));
#endif

	if (*input == 'q' && *(input + 1) == '\0')
		goto EXIT;

	if (*input == 'u' && *(input + 1) == '\0') {
		unmount_dev(mountpoints, k);
		goto EXIT;
	}

	int atoi_num = atoi(input);
	if (atoi_num > 0 && atoi_num <= (int)mp_n) {
#ifdef __linux__
		if (!mountpoints[atoi_num - 1].mnt) {
			/* Here we should mount the device */
			char file[PATH_MAX];
			snprintf(file, PATH_MAX, "%s/clifm.XXXXXX", P_tmpdir);

			int fd = mkstemp(file);
			if (fd == -1)
				goto EXIT;

			int stdout_bk = dup(STDOUT_FILENO); /* Save original stdout */
			dup2(fd, STDOUT_FILENO); /* Redirect stdout to the desired file */
			close(fd);

			char *cmd[] = {"udisksctl", "mount", "-b",
						mountpoints[atoi_num - 1].dev, NULL};
			launch_execve(cmd, FOREGROUND, E_NOFLAG);

			dup2(stdout_bk, STDOUT_FILENO); /* Restore original stdout */
			close(stdout_bk);

			FILE *fp = open_fstream_r(file, &fd);
			if (!fp) {
				unlink(file);
				goto EXIT;
			}

			char out_line[PATH_MAX];
			if (fgets(out_line, (int)sizeof(out_line), fp) == NULL) {
				close_fstream(fp, fd);
				unlink(file);
				goto EXIT;
			}

			close_fstream(fp, fd);
			unlink(file);
			char *p = strrchr(out_line, ' ');
			if (!p || !*(p + 1) || *(p + 1) != '/')
				goto EXIT;
			++p;

			size_t plen = strlen(p);
			if (p[plen - 1] == '\n')
				p[plen - 1] = '\0';

			mountpoints[atoi_num - 1].mnt = savestring(p, strlen(p));
		}
#endif // __linux__
		if (xchdir(mountpoints[atoi_num - 1].mnt, SET_TITLE) == EXIT_SUCCESS) {
			free(ws[cur_ws].path);
			ws[cur_ws].path = savestring(mountpoints[atoi_num - 1].mnt,
			    strlen(mountpoints[atoi_num - 1].mnt));

			if (autols) {
				free_dirlist();
				if (list_dir() != EXIT_SUCCESS)
					exit_status = EXIT_FAILURE;
			}

			add_to_dirhist(ws[cur_ws].path);
			add_to_jumpdb(ws[cur_ws].path);
		} else {
			fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME,
			    mountpoints[atoi_num - 1].mnt, strerror(errno));
			exit_status = EXIT_FAILURE;
		}
	} else {
		fprintf(stderr, "%s: %s: Invalid ELN\n", PROGRAM_NAME, input);
		exit_status = EXIT_FAILURE;
	}

EXIT:
	/* Free stuff and exit */
	free(input);

	i = (int)mp_n;
	while (--i >= 0) {
		free(mountpoints[i].mnt);
		free(mountpoints[i].dev);
		free(mountpoints[i].label);
	}
	free(mountpoints);

	return exit_status;
}
