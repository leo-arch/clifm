/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* fs_events.c -- Monitor file system events */

#include "helpers.h"

#include <errno.h>
#include <stdio.h>
#include <unistd.h> /* read, close */
#include <fcntl.h> /* open */
#include <string.h> /* memset */
#include <sys/stat.h> /* stat */

#include "aux.h" /* count_dir */
#include "listing.h" /* reload_dirlist */
#include "misc.h" /* err (via xerror macro) */

#ifdef LINUX_INOTIFY
static void
reset_inotify(void)
{
	watch = 0;

	if (inotify_wd >= 0) {
		inotify_rm_watch(inotify_fd, inotify_wd);
		inotify_wd = -1;
	}

	if (inotify_fd != UNSET)
		close(inotify_fd);
	inotify_fd = inotify_init1(IN_NONBLOCK);
	if (inotify_fd < 0) {
		err('w', PRINT_PROMPT, "%s: inotify: %s\n",
			PROGRAM_NAME, strerror(errno));
		return;
	}

	/* If CWD is a symlink to a directory and it does not end with a slash,
	 * inotify_add_watch(3) fails with ENOTDIR. */
	char rpath[PATH_MAX + 1];
	snprintf(rpath, sizeof(rpath), "%s/", workspaces[cur_ws].path);

	inotify_wd = inotify_add_watch(inotify_fd, rpath, INOTIFY_MASK);
	if (inotify_wd > 0)
		watch = 1;
	else
		err('w', PRINT_PROMPT, "%s: inotify: '%s': %s\n",
			PROGRAM_NAME, rpath, strerror(errno));
}

static void
read_inotify(void)
{
	if (inotify_fd == UNSET)
		return;

	int i;
	struct inotify_event *event;
	char inotify_buf[EVENT_BUF_LEN];

	memset((void *)inotify_buf, '\0', sizeof(inotify_buf));
	i = (int)read(inotify_fd, inotify_buf, sizeof(inotify_buf)); /* flawfinder: ignore */

	if (i <= 0) {
# ifdef INOTIFY_DEBUG
		puts("INOTIFY_RETURN");
# endif /* INOTIFY_DEBUG */
		return;
	}

	int ignore_event = 0;
	int refresh = 0;

	for (char *ptr = inotify_buf;
	ptr + ((struct inotify_event *)ptr)->len < inotify_buf + i;
	ptr += sizeof(struct inotify_event) + event->len) {
		event = (struct inotify_event *)ptr;
		ignore_event = 0;

# ifdef INOTIFY_DEBUG
		printf("%s (%u:%d): ", *event->name
			? event->name : NULL, event->len, event->wd);
# endif /* INOTIFY_DEBUG */

		if (!event->wd) {
# ifdef INOTIFY_DEBUG
			puts("INOTIFY_BREAK");
# endif /* INOTIFY_DEBUG */
			break;
		}

		if (event->mask & IN_CREATE) {
# ifdef INOTIFY_DEBUG
			puts("IN_CREATE");
# endif /* INOTIFY_DEBUG */
			struct stat a;
			if (event->len && lstat(event->name, &a) != 0) {
				/* The file was created, but doesn't exist anymore */
				ignore_event = 1;
			}
		}

		/* A file was renamed */
		if (event->mask & IN_MOVED_TO) {
# ifdef INOTIFY_DEBUG
			puts("IN_MOVED_TO");
# endif /* INOTIFY_DEBUG */
			filesn_t j = files;
			while (--j >= 0) {
				if (*file_info[j].name == *event->name
				&& strcmp(file_info[j].name, event->name) == 0)
					break;
			}

			/* If destiny filename is already in the file list (j >= 0),
			 * ignore this event. */
			ignore_event = (j < 0) ? 0 : 1;
		}

		if (event->mask & IN_DELETE) {
# ifdef INOTIFY_DEBUG
			puts("IN_DELETE");
# endif /* INOTIFY_DEBUG */
			struct stat a;
			if (event->len && lstat(event->name, &a) == 0)
				/* The file was removed, but is still there (recreated) */
				ignore_event = 1;
		}

# ifdef INOTIFY_DEBUG
		if (event->mask & IN_DELETE_SELF)
			puts("IN_DELETE_SELF");
		if (event->mask & IN_MOVE_SELF)
			puts("IN_MOVE_SELF");
		if (event->mask & IN_MOVED_FROM)
			puts("IN_MOVED_FROM");
		if (event->mask & IN_MOVED_TO)
			puts("IN_MOVED_TO");
		if (event->mask & IN_IGNORED)
			puts("IN_IGNORED");
# endif /* INOTIFY_DEBUG */

		if (ignore_event == 0 && (event->mask & INOTIFY_MASK))
			refresh = 1;
	}

	if (refresh == 1 && exit_code == FUNC_SUCCESS) {
# ifdef INOTIFY_DEBUG
		puts("INOTIFY_REFRESH");
# endif /* INOTIFY_DEBUG */
		reload_dirlist();
	} else {
# ifdef INOTIFY_DEBUG
		puts("INOTIFY_RESET");
# endif /* INOTIFY_DEBUG */
		/* Reset the inotify watch list */
		reset_inotify();
	}
}
#elif defined(BSD_KQUEUE)
/* Insert the following lines in the for loop to debug kqueue:
if (event_data[i].fflags & NOTE_DELETE)
	puts("NOTE_DELETE");
if (event_data[i].fflags & NOTE_WRITE)
	puts("NOTE_WRITE");
if (event_data[i].fflags & NOTE_EXTEND)
	puts("NOTE_EXTEND");
if (event_data[i].fflags & NOTE_ATTRIB)
	puts("NOTE_ATTRIB");
if (event_data[i].fflags & NOTE_LINK)
	puts("NOTE_LINK");
if (event_data[i].fflags & NOTE_RENAME)
	puts("NOTE_RENAME");
if (event_data[i].fflags & NOTE_REVOKE)
	puts("NOTE_REVOKE"); */
static void
read_kqueue(void)
{
	struct kevent event_data[NUM_EVENT_SLOTS];
	memset((void *)event_data, '\0', sizeof(struct kevent) * NUM_EVENT_SLOTS);

	int i;
	const int count = kevent(kq, NULL, 0, event_data, 4096, &timeout);

	for (i = 0; i < count; i++) {
		if (event_data[i].fflags & KQUEUE_FFLAGS) {
			reload_dirlist();
			return;
		}
	}
}
#elif defined(GENERIC_FS_MONITOR)
/* Update the current list of files if the modification time of the current
 * directory changed and if the number of files differ. Sometimes, processes
 * create temporary files (changing modification time) that are immediately
 * deleted (resulting in the same number of files), in which case there is no
 * need to update the list of files (this happens for example with 'git pull',
 * or with a command along the lines of 'touch file && rm file'). */
static void
check_fs_changes(void)
{
	if (!workspaces || cur_ws < 0 || cur_ws >= MAX_WS
	|| !workspaces[cur_ws].path || curdir_mtime == 0)
		return;

	struct stat a;
	if (stat(workspaces[cur_ws].path, &a) == -1 || curdir_mtime == a.st_mtime)
		return;

	const filesn_t cur_files = count_dir(workspaces[cur_ws].path, 0);
	if (cur_files < 2) /* Error (-1) or empty (only self and parent dirs) */
		return;

	if ((cur_files - 2) != files)
		reload_dirlist();
}
#endif /* LINUX_INOTIFY */

void
set_events_checker(void)
{
	if (xargs.list_and_quit == 1)
		return;

#if defined(LINUX_INOTIFY)
	reset_inotify();

#elif defined(BSD_KQUEUE)
	if (event_fd >= 0) {
		close(event_fd);
		event_fd = -1;
		watch = 0;
	}

# if defined(O_EVTONLY)
	event_fd = open(workspaces[cur_ws].path, O_EVTONLY);
# else
	event_fd = open(workspaces[cur_ws].path, O_RDONLY);
# endif /* O_EVTONLY */
	if (event_fd >= 0) {
		/* Prepare for events */
		EV_SET(&events_to_monitor[0], (uintptr_t)event_fd, EVFILT_VNODE,
			EV_ADD | EV_CLEAR, KQUEUE_FFLAGS, 0, workspaces[cur_ws].path);
		watch = 1;
		/* Register events */
		kevent(kq, events_to_monitor, NUM_EVENT_SLOTS, NULL, 0, NULL);
	}

#elif defined(GENERIC_FS_MONITOR)
	struct stat a;
	if (stat(workspaces[cur_ws].path, &a) != -1)
		curdir_mtime = a.st_mtime;
	else
		curdir_mtime = 0;
#endif /* LINUX_INOTIFY */
}

void
check_fs_events(const int is_internal_cmd)
{
	if (conf.autols == 0 || (is_internal_cmd == 0
	&& conf.clear_screen == CLEAR_INTERNAL_CMD_ONLY))
		return;

#if defined(LINUX_INOTIFY)
	if (watch)
		read_inotify();
#elif defined(BSD_KQUEUE)
	if (watch && event_fd >= 0)
		read_kqueue();
#elif defined(GENERIC_FS_MONITOR)
	check_fs_changes();
#endif /* LINUX_INOTIFY */
}
