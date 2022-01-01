/* sanitize.c -- functions for commands and environment sanitization */

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

#include <string.h>
#include <paths.h>
#include <stdio.h>
#include <errno.h>

#include "sanitize.h"
#include "strings.h"
#include "aux.h"

char **env_bk = (char **)NULL; 
char **new_env = (char **)NULL;

/* Unset environ: little implementation of clearenv(3), not available
 * on some system (not POSIX) */
static inline void
xclearenv(void)
{
	environ = NULL; /* This seems to be enough (it is it according to
	the Linux manpage for clearenv(3)) */
/*
//	static char *namebuf = NULL;
//	static size_t lastlen = 0;

 	while (environ != NULL && environ[0] != NULL) {
		size_t len = strcspn(environ[0], "=");
		if (len == 0) {
			// Handle empty variable name (corrupted environ[])
			continue;
		}
		if (len > lastlen) {
			namebuf = (char *)xrealloc(namebuf, (len + 1) * sizeof(char));
			lastlen = len;
		}
		memcpy(namebuf, environ[0], len);
		namebuf[len] = '\0';
		if (unsetenv(namebuf) == -1) {
			fprintf(stderr, "%s: unsetenv: %s: %s\n", PROGRAM_NAME,
				namebuf, strerror(errno));
			exit(EXIT_FAILURE);
		}
	} */
}

static inline void
set_path_env(void)
{
	int ret = -1;

#ifdef _PATH_STDPATH
	ret = setenv("PATH", _PATH_STDPATH, 1);
#else
	char *p = (char *)NULL;
	size_t n = confstr(_CS_PATH, NULL, 0); /* Get value's size */
	p = (char *)xnmalloc(n, sizeof(char)); /* Allocate space */
	confstr(_CS_PATH, p, n);               /* Get value */
	ret = setenv("PATH", p, 1);            /* Set it */
	free(p);
#endif

	if (ret == -1) {
		fprintf(stderr, "%s: setenv: PATH: %s\n", PROGRAM_NAME,
			strerror(errno));
		exit(EXIT_FAILURE);
	}
}

static inline void
set_ifs_env(void)
{
	if (setenv("IFS", " \t\n", 1) == -1) {
		fprintf(stderr, "%s: setenv: IFS: %s\n", PROGRAM_NAME,
			strerror(errno));
		exit(EXIT_FAILURE);
	}
}

/* Sanitize the environment: set environ to NULL and then set a few
 * environment variables to get a minimally working environment */
int
xsecure_env(const int mode)
{
#ifdef __HAIKU__
	fprintf(stderr, "%s: secure-env: This feature is not available "
		"on Haiku\n", PROGRAM_NAME);
	exit(EXIT_FAILURE);
#elif __NetBSD__
	fprintf(stderr, "%s: secure-env: This feature is not available "
		"on NetBSD\n", PROGRAM_NAME);
	exit(EXIT_FAILURE);
#endif

	char *display = (char *)NULL;
	char *wayland_display = (char *)NULL;
	char *_term = (char *)NULL;
	char *tz = (char *)NULL;
	char *lang = (char *)NULL;
	char *fzfopts = (char *)NULL;

	if (mode != SECURE_ENV_FULL) {
		/* Let's keep these values from the current environment */
		display = getenv("DISPLAY");
		if (!display)
			wayland_display = getenv("WAYLAND_DISPLAY");
		_term = getenv("TERM");
		tz = getenv("TZ");
		lang = getenv("LANG");
		if (fzftab)
			fzfopts = getenv("FZF_DEFAULT_OPTS");
	}

	xclearenv();
	set_path_env();
	set_ifs_env();

/*	n = confstr(_CS_V7_ENV, NULL, 0);
	char *p = (char *)xrealloc(p, n * sizeof(char));
	confstr(_CS_V7_ENV, p, n);
	char *e = strchr(p, '=');
	if (e && *(e + 1)) {
		*e = '\0';
		setenv(p, e + 1, 1);
		*e = '=';
	} 
	free(p); */

	if (mode == SECURE_ENV_FULL)
		return EXIT_SUCCESS;

	if (user.name)
		setenv("USER", user.name, 1);
	if (user.home)
		setenv("HOME", user.home, 1);
	if (user.shell)
		setenv("SHELL", user.shell, 1);

	if (display && sanitize_cmd(display, SNT_DISPLAY) == EXIT_SUCCESS) {
		if (setenv("DISPLAY", display, 1) == -1) {
			fprintf(stderr, "%s: setenv: DISPLAY: %s\n", PROGRAM_NAME,
				strerror(errno));
		}
	} else {
		if (wayland_display) {
			if (setenv("WAYLAND_DISPLAY", wayland_display, 1) == -1) {
				fprintf(stderr, "%s: setenv: WAYLAND_DISPLAY: %s\n",
					PROGRAM_NAME, strerror(errno));
			}
		}
	}

	if (_term && sanitize_cmd(_term, SNT_MISC) == EXIT_SUCCESS) {
		if (setenv("TERM", _term, 1) == -1) {
			fprintf(stderr, "%s: setenv: TERM: %s\n", PROGRAM_NAME,
				strerror(errno));
		}
	}

	if (tz && sanitize_cmd(tz, SNT_MISC) ==  EXIT_SUCCESS) {
		if (setenv("TZ", tz, 1) == -1) {
			fprintf(stderr, "%s: setenv: TZ: %s\n", PROGRAM_NAME,
				strerror(errno));
		}
	}

	if (lang && sanitize_cmd(lang, SNT_MISC) ==  EXIT_SUCCESS) {
		if (setenv("LANG", lang, 1) == -1) {
			fprintf(stderr, "%s: setenv: LANG: %s\n", PROGRAM_NAME,
				strerror(errno));
		}
		if (setenv("LC_ALL", lang, 1) == -1) {
			fprintf(stderr, "%s: setenv: LC_ALL: %s\n", PROGRAM_NAME,
				strerror(errno));
		}	
	} else {
		if (mode != SECURE_ENV_FULL && setenv("LC_ALL", "C", 1) == -1) {
			fprintf(stderr, "%s: setenv: LC_ALL: %s\n", PROGRAM_NAME,
				strerror(errno));
		}
	}

	if (fzfopts) {
		if (setenv("FZF_DEFAULT_OPTS", fzfopts, 1) == -1) {
			fprintf(stderr, "%s: setenv: FZF_DEFAULT_OPTS: %s\n", PROGRAM_NAME,
				strerror(errno));
		}
	}

	return 0;
}

/* Create a sanitized environment to run a single command */
void
sanitize_cmd_environ(void)
{
	/* Pointer to environ original address */
	env_bk = environ;

	/* Create a controlled environment to securely run shell commands */
	new_env = (char **)xnmalloc(12, sizeof(char *));
	size_t n = 0;
	char p[PATH_MAX];
#ifdef _PATH_STDPATH
	snprintf(p, PATH_MAX, "PATH=%s", _PATH_STDPATH);
	new_env[n] = savestring(p, strlen(p));
	n++;
#else
	char *q = (char *)NULL;
	size_t len = confstr(_CS_PATH, NULL, 0); /* Get value's size */
	q = (char *)xnmalloc(len, sizeof(char)); /* Allocate space */
	confstr(_CS_PATH, q, len);               /* Get value */
	new_env[n] = savestring(q, strlen(q));   /* Set it */
	n++;
	free(q);
#endif
	new_env[n] = savestring("IFS=\" \n\t\"", 9);
	n++;
	if (user.name) {
		snprintf(p, PATH_MAX, "USER=%s", user.name);
		new_env[n] = savestring(p, strlen(p));
		n++;
		snprintf(p, PATH_MAX, "LOGNAME=%s", user.name);
		new_env[n] = savestring(p, strlen(p));
		n++;
	}
	if (user.home) {
		snprintf(p, PATH_MAX, "HOME=%s", user.home);
		new_env[n] = savestring(p, strlen(p));
		n++;
	}
	if (user.shell) {
		snprintf(p, PATH_MAX, "SHELL=%s", user.shell);
		new_env[n] = savestring(p, strlen(p));
		n++;
	}

	char *e = (char *)NULL;
	if ((flags & GUI)) {
		e = getenv("DISPLAY");
		if (e && sanitize_cmd(e, SNT_DISPLAY) == EXIT_SUCCESS) {
			new_env[n] = (char *)xnmalloc(9 + strlen(e), sizeof(char));
			sprintf(new_env[n], "DISPLAY=%s", e);
			n++;
		}
		/* Import and sanitize */
		e = getenv("TERM");
		if (e && sanitize_cmd(e, SNT_MISC) == EXIT_SUCCESS) {
			new_env[n] = (char *)xnmalloc(6 + strlen(e), sizeof(char));
			sprintf(new_env[n], "TERM=%s", e);
			n++;
		}
		/* If running on Wayland and WAYLAND_DISPLAY isn't set, Wayland
		 * client will try a fallback dispay, usually wayland-0 or wayland-1.
		 * So, there's no need to set WAYLAND_DISPLAY */
	}

	/* Import and sanitize */
	e = getenv("TZ");
	if (e && sanitize_cmd(e, SNT_MISC) == EXIT_SUCCESS) {
		new_env[n] = (char *)xnmalloc(4 + strlen(e), sizeof(char));
		sprintf(new_env[n], "TZ=%s", e);
		n++;
	}

	e = getenv("LANG");
	if (e && sanitize_cmd(e, SNT_MISC) == EXIT_SUCCESS) {
		new_env[n] = (char *)xnmalloc(6 + strlen(e), sizeof(char));
		sprintf(new_env[n], "LANG=%s", e);
		n++;
		new_env[n] = (char *)xnmalloc(8 + strlen(e), sizeof(char));
		sprintf(new_env[n], "LC_ALL=%s", e);
		n++;
	}

	new_env[n] = (char *)NULL;

	/* Set environ to our controlled environment */
	environ = new_env;
}

/* Restore the environment after running a cmd with a sanitized environment
 * via sanitize_cmd_environ() */
void
restore_cmd_environ(void)
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

/* Sanitize value of DISPLAY env var */
static inline int
sanitize_misc(char *str)
{
	if (strlen(str) > strspn(str, ALLOWED_CHARS_MISC))
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

/* Sanitize value of DISPLAY env var */
static inline int
sanitize_display(char *str)
{
	if (strlen(str) > strspn(str, ALLOWED_CHARS_DISPLAY))
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
sanitize_cmd(char *str, int type)
{
	if (!str || !*str)
		return EXIT_FAILURE;

	switch(type) {
	case SNT_MIME:
		return sanitize_mime(str);
	case SNT_NET:
		return sanitize_net(str);
	case SNT_DISPLAY:
		return sanitize_display(str);
	case SNT_MISC:
		return sanitize_misc(str);
	case SNT_PROFILE: /* fallthrough */
	case SNT_PROMPT: /* fallthrough */
	case SNT_AUTOCMD: /* fallthrough */
	case SNT_GRAL:
		return sanitize_gral(str);
	case SNT_NONE: break;
	default: break;
	}

	return EXIT_SUCCESS;
}
