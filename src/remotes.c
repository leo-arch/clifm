/* remote.c -- function wrappers for smb, ftp, and ssh */

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

#include "clifm.h"

int
remote_ftp(char *address, char *options)
{
	if (xargs.stealth_mode == 1) {
		printf("%s: Access to remote filesystems is disabled in "
			   "stealth mode\n", PROGRAM_NAME);
		return EXIT_SUCCESS;
	}

#if __FreeBSD__
	fprintf(stderr, _("%s: FTP is not yet supported on FreeBSD\n"),
			PROGRAM_NAME);
	return EXIT_FAILURE;
#endif

	if (!address || !*address)
		return EXIT_FAILURE;

	char *tmp_addr = savestring(address, strlen(address));

	char *p = tmp_addr;
	while (*p) {
		if (*p == '/')
			*p = '_';
		p++;
	}

	char *rmountpoint = (char *)xnmalloc(strlen(TMP_DIR)
									 + strlen(tmp_addr) + 9, sizeof(char));

	sprintf(rmountpoint, "%s/remote/%s", TMP_DIR, tmp_addr);
	free(tmp_addr);

	struct stat file_attrib;

	if (stat(rmountpoint, &file_attrib) == -1) {

		char *mkdir_cmd[] = { "mkdir", "-p", rmountpoint, NULL };

		if (launch_execve(mkdir_cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS) {
			fprintf(stderr, _("%s: %s: Cannot create mountpoint\n"),
					PROGRAM_NAME, rmountpoint);
			free(rmountpoint);
			return EXIT_FAILURE;
		}
	}

	else if (count_dir(rmountpoint) > 2) {
		fprintf(stderr, _("%s: %s: Mounpoint not empty\n"),
				PROGRAM_NAME, rmountpoint);
		free(rmountpoint);
		return EXIT_FAILURE;
	}

	/* CurlFTPFS does not require sudo */
	char *cmd[] = { "curlftpfs", address, rmountpoint, (options) ? "-o"
					: NULL, (options) ? options: NULL, NULL };
	int error_code = launch_execve(cmd, FOREGROUND, E_NOFLAG);

	if (error_code) {
		free(rmountpoint);
		return EXIT_FAILURE;
	}

	if (xchdir(rmountpoint, SET_TITLE) != 0) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, rmountpoint,
				strerror(errno));
		free(rmountpoint);
		return EXIT_FAILURE;
	}

	free(ws[cur_ws].path);
	ws[cur_ws].path = savestring(rmountpoint, strlen(rmountpoint));

	free(rmountpoint);

	if (cd_lists_on_the_fly) {
		free_dirlist();
		if (list_dir() != EXIT_SUCCESS)
			error_code = EXIT_FAILURE;
	}

	return error_code;

}

int
remote_smb(char *address, char *options)
{
	if (xargs.stealth_mode == 1) {
		printf("%s: Access to remote filesystems is disabled in "
			   "stealth mode\n", PROGRAM_NAME);
		return EXIT_SUCCESS;
	}

#if __FreeBSD__
	fprintf(stderr, _("%s: SMB is not yet supported on FreeBSD\n"),
			PROGRAM_NAME);
	return EXIT_FAILURE;
#endif

	/* smb://[USER@]HOST[/SERVICE][/REMOTE-DIR] */

	if (!address || !*address)
		return EXIT_FAILURE;

	int free_address = 0;
	char *ruser = (char *)NULL, *raddress = (char *)NULL;
	char *tmp = strchr(address, '@');

	if (tmp) {
		*tmp = '\0';
		ruser = savestring(address, strlen(address));

		raddress = savestring(tmp + 1, strlen(tmp + 1));
		free_address = 1;
	}

	else
		raddress = address;

	char *addr_tmp = (char *)xnmalloc(strlen(raddress) + 3, sizeof(char));
	sprintf(addr_tmp, "//%s", raddress);

	char *p = raddress;

	while (*p) {
		if (*p == '/')
			*p = '_';
		p++;
	}

	char *rmountpoint = (char *)xnmalloc(strlen(raddress)
									 + strlen(TMP_DIR) + 9, sizeof(char));
	sprintf(rmountpoint, "%s/remote/%s", TMP_DIR, raddress);

	int free_options = 0;
	char *roptions = (char *)NULL;

	if (ruser) {
		roptions = (char *)xnmalloc(strlen(ruser) + strlen(options)
									+ 11, sizeof(char));
		sprintf(roptions, "username=%s,%s", ruser, options);
		free_options = 1;
	}
	else
		roptions = options;

	/* Create the mountpoint, if it doesn't exist */
	struct stat file_attrib;

	if (stat(rmountpoint, &file_attrib) == -1) {
		char *mkdir_cmd[] = { "mkdir", "-p", rmountpoint, NULL };

		if (launch_execve(mkdir_cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS) {

			if (free_options)
				free(roptions);

			if (ruser)
				free(ruser);

			if (free_address)
				free(raddress);

			free(rmountpoint);
			free(addr_tmp);

			fprintf(stderr, _("%s: %s: Cannot create mountpoint\n"),
					PROGRAM_NAME, rmountpoint);

			return EXIT_FAILURE;
		}
	}

	/* If the mountpoint already exists, check if it is empty */
	else if (count_dir(rmountpoint) > 2) {
		fprintf(stderr, _("%s: %s: Mountpoint not empty\n"),
				PROGRAM_NAME, rmountpoint);

		if (free_options)
			free(roptions);

		if (ruser)
			free(ruser);

		if (free_address)
			free(raddress);

		free(rmountpoint);
		free(addr_tmp);

		return EXIT_FAILURE;
	}

	int error_code = 1;

	/* Create and execute the SMB command */
	if (!(flags & ROOT_USR)) {
		char *cmd[] = { "sudo", "-u", "root", "mount.cifs", addr_tmp,
						rmountpoint, (roptions) ? "-o" : NULL,
						(roptions) ? roptions : NULL, NULL };
		error_code = launch_execve(cmd, FOREGROUND, E_NOFLAG);
	}

	else {
		char *cmd[] = { "mount.cifs", addr_tmp, rmountpoint, (roptions)
						? "-o" : NULL, (roptions) ? roptions : NULL, NULL };
		error_code = launch_execve(cmd, FOREGROUND, E_NOFLAG);
	}

	if (free_options)
		free(roptions);

	if (ruser)
		free(ruser);

	if (free_address)
		free(raddress);

	free(addr_tmp); 

	if (error_code) {
		free(rmountpoint);
		return EXIT_FAILURE;
	}

	/* If successfully mounted, chdir into mountpoint */
	if (xchdir(rmountpoint, SET_TITLE) != 0) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, rmountpoint,
				strerror(errno));
		free(rmountpoint);
		return EXIT_FAILURE;
	}

	free(ws[cur_ws].path);
	ws[cur_ws].path = savestring(rmountpoint, strlen(rmountpoint));

	free(rmountpoint);

	if (cd_lists_on_the_fly) {
		free_dirlist();
		if (list_dir() != EXIT_SUCCESS)
			error_code = EXIT_FAILURE;
	}

	return error_code;
}

int
remote_ssh(char *address, char *options)
{
	if (xargs.stealth_mode == 1) {
		printf("%s: Access to remote filesystems is disabled in "
			   "stealth mode\n", PROGRAM_NAME);
		return EXIT_SUCCESS;
	}

#if __FreeBSD__
	fprintf(stderr, _("%s: SFTP is not yet supported on FreeBSD"),
			PROGRAM_NAME);
	return EXIT_FAILURE;
#endif

	if (!config_ok)
		return EXIT_FAILURE;

/*  char *sshfs_path = get_cmd_path("sshfs");
	if (!sshfs_path) {
		fprintf(stderr, _("%s: sshfs: Program not found.\n"),
				PROGRAM_NAME);
		return EXIT_FAILURE;
	} */

	if(!address || !*address)
		return EXIT_FAILURE;

	/* Create mountpoint */
	char *rname = savestring(address, strlen(address));

	/* Replace all slashes in address by underscore to construct the
	 * mounpoint name */
	char *p = rname;
	while (*p) {
		if (*p == '/')
			*p = '_';
		p++;
	}

	char *rmountpoint = (char *)xnmalloc(strlen(TMP_DIR) + strlen(rname)
										 + 9, sizeof(char));
	sprintf(rmountpoint, "%s/remote/%s", TMP_DIR, rname);
	free(rname);

	/* If the mountpoint doesn't exist, create it */
	struct stat file_attrib;
	if (stat(rmountpoint, &file_attrib) == -1) {
		char *mkdir_cmd[] = { "mkdir", "-p", rmountpoint, NULL };

		if (launch_execve(mkdir_cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS) {
			fprintf(stderr, _("%s: %s: Cannot create mountpoint\n"),
					PROGRAM_NAME, rmountpoint);
			free(rmountpoint);
			return EXIT_FAILURE;
		}
	}

	/* If it exists, make sure it is not populated */
	else if (count_dir(rmountpoint) > 2) {
		fprintf(stderr, _("%s: %s: Mounpoint not empty\n"),
				PROGRAM_NAME, rmountpoint);
		free(rmountpoint);
		return EXIT_FAILURE;
	}

	/* Construct the command */

	int error_code = 1;

	if ((flags & ROOT_USR)) {
		char *cmd[] = { "sshfs", address, rmountpoint, (options) ? "-o"
						: NULL, (options) ? options: NULL, NULL };
		error_code = launch_execve(cmd, FOREGROUND, E_NOFLAG);
	}

	else {
		char *cmd[] = { "sudo", "sshfs", address, rmountpoint, "-o",
						"allow_other", (options) ? "-o" : NULL,
						(options) ? options : NULL, NULL};
		error_code = launch_execve(cmd, FOREGROUND, E_NOFLAG);
	}

	if (error_code != EXIT_SUCCESS) {
		free(rmountpoint);
		return EXIT_FAILURE;
	}

	/* If successfully mounted, chdir into mountpoint */
	if (xchdir(rmountpoint, SET_TITLE) != 0) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, rmountpoint,
				strerror(errno));
		free(rmountpoint);
		return EXIT_FAILURE;
	}

	free(ws[cur_ws].path);
	ws[cur_ws].path = savestring(rmountpoint, strlen(rmountpoint));
	free(rmountpoint);

	if (cd_lists_on_the_fly) {
		free_dirlist();
		if (list_dir() != EXIT_SUCCESS)
			error_code = EXIT_FAILURE;
	}

	return error_code;
}
