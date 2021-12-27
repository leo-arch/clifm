/* sanitize.c -- functions for commands sanitization */

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

#include <string.h>
#include <paths.h>
#include <stdio.h>

#include "sanitize.h"
#include "strings.h"
#include "aux.h"

char **env_bk = (char **)NULL; 
char **new_env = (char **)NULL;

void
sanitize_environ(void)
{
	/* Pointer to environ original address */
	env_bk = environ;

	/* Create a controlled environment to securely run shell commands */
	new_env = (char **)xnmalloc(7, sizeof(char *));
	char p[PATH_MAX];
	snprintf(p, PATH_MAX, "PATH=%s", _PATH_STDPATH);
	new_env[0] = savestring(p, strlen(p));
	new_env[1] = savestring("IFS=\" \n\t\"", 9);
	snprintf(p, PATH_MAX, "USER=%s", user.name);
	new_env[2] = savestring(p, strlen(p));
	snprintf(p, PATH_MAX, "HOME=%s", user.home);
	new_env[3] = savestring(p, strlen(p));
	snprintf(p, PATH_MAX, "SHELL=%s", user.shell);
	new_env[4] = savestring(p, strlen(p));
	if ((flags & GUI))
		new_env[5] = savestring("TERM=xterm", 10);
	else
		new_env[5] = savestring("\0", 1);
	new_env[6] = (char *)NULL;

	/* Set environ to our controlled environment */
	environ = new_env;
}

void
restore_environ(void)
{
	/* Free the buffer holding the controlled environ */
	size_t i;
	for (i = 0; new_env[i]; i++)
		free(new_env[i]);
	free(new_env);
	new_env = (char **)NULL;

	/* Restore environ to its original value */
	environ = env_bk;
	env_bk = (char **)NULL;
}

/* Sanitize cmd string coming from the mimelist file */
static inline int
sanitize_mime(char *cmd)
{
	/* Only %f is allowed */
	char *p = strchr(cmd, '%');
	if (p && *(p + 1) != 'f')
		return EXIT_FAILURE;

	/* Disallow double ampersand */
	p = strchr(cmd, '&');
	if (p && *(p + 1) == '&')
		return EXIT_FAILURE;

	if (strlen(cmd) > strspn(cmd, ALLOWED_CHARS_MIME))
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

/* Sanitize cmd string coming from the net file */
static inline int
sanitize_net(char *cmd)
{
	if (strlen(cmd) > strspn(cmd, ALLOWED_CHARS_NET))
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

/* Sanitize cmd string coming from profile, prompt, and autocmds */
static inline int
sanitize_gral(char *cmd)
{
	if (strlen(cmd) > strspn(cmd, ALLOWED_CHARS_GRAL))
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

/* Sanitize CMD according to TYPE. Returns zero if safe command and
 * one if not */
int
sanitize_cmd(char *cmd, int type)
{
	if (!cmd || !*cmd)
		return EXIT_FAILURE;

	switch(type) {
	case SNT_MIME:
		return sanitize_mime(cmd);
	case SNT_NET:
		return sanitize_net(cmd);
	case SNT_PROFILE: /* fallthrough */
	case SNT_PROMPT: /* fallthrough */
	case SNT_AUTOCMD:
		return sanitize_gral(cmd);
	case SNT_NONE: break;
	default: break;
	}

	return EXIT_SUCCESS;
}
