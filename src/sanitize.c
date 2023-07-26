/* sanitize.c -- functions for commands and environment sanitization */

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

#include <string.h>
#include <paths.h> /* _PATH_STDPATH */
#include <stdio.h>
#include <errno.h>
#include <sys/resource.h> /* getrlimit(3), setrlimit(3) */
#include <unistd.h> /* close(2), sysconf(3) */
#include <limits.h> /* OPEN_MAX */

#include "sanitize.h"
#include "aux.h"
#include "checks.h" /* is_number() */
#include "misc.h"

#define UNSAFE_CMD "Unsafe command. Consult the manpage for more information"
/* If PATH cannot be retrieved from any other source, let's use this value */
#if !defined(_PATH_STDPATH) && !defined(_CS_PATH)
# define MINIMAL_PATH "/usr/local/bin:/bin:/usr/bin:/sbin:/usr/sbin"
#endif

/* Unset environ: little implementation of clearenv(3), not available
 * on some systems (not POSIX) */
static void
xclearenv(void)
{
#if !defined(__NetBSD__) && !defined(__HAIKU__) && !defined(__APPLE__) \
&& !defined(__TERMUX__)
	/* This seems to be enough (it is it according to the Linux/FreeBSD
	 * manpage for clearenv(3)). */
	environ = NULL;
#else
	static char *namebuf = NULL;
	static size_t lastlen = 0;

 	while (environ != NULL && environ[0] != NULL) {
		size_t len = strcspn(environ[0], "=");
		if (len == 0) {
			/* Handle empty variable name (corrupted environ[]) */
			continue;
		}
		if (len > lastlen) {
			namebuf = (char *)xrealloc(namebuf, (len + 1) * sizeof(char));
			lastlen = len;
		}
		memcpy(namebuf, environ[0], len);
		namebuf[len] = '\0';
		if (unsetenv(namebuf) == -1) {
			xerror("%s: unsetenv: %s: %s\n", PROGRAM_NAME,
				namebuf, strerror(errno));
			exit(EXIT_FAILURE);
		}
	}
#endif /* !__NetBSD__ && !__HAIKU__ && !__APPLE__ && !__TERMUX__ */
}

static void
set_path_env(void)
{
	int ret = -1;

#if defined(_PATH_STDPATH)
	ret = setenv("PATH", _PATH_STDPATH, 1);
#elif defined(_CS_PATH)
	char *p = (char *)NULL;
	size_t n = confstr(_CS_PATH, NULL, 0); /* Get value's size */
	p = (char *)xnmalloc(n, sizeof(char)); /* Allocate space */
	confstr(_CS_PATH, p, n);               /* Get value */
	ret = setenv("PATH", p, 1);            /* Set it */
	free(p);
#else
	ret = setenv("PATH", MINIMAL_PATH, 1);
#endif /* _PATH_STDPATH */

	if (ret == -1) {
		xerror("%s: setenv: PATH: %s\n", PROGRAM_NAME, strerror(errno));
		exit(EXIT_FAILURE);
	}
}

static void
xsetenv(const char *name, const char *value)
{
	if (setenv(name, value, 1) == -1)
		xerror("%s: setenv: %s: %s\n", PROGRAM_NAME, name, strerror(errno));
}

/* See https://www.oreilly.com/library/view/secure-programming-cookbook/0596003943/ch01s09.html */
static void
disable_coredumps(void)
{
#if defined(ALLOW_COREDUMPS)
	return;
#endif /* ALLOW_COREDUMPS */

	struct rlimit rlim;
	rlim.rlim_cur = rlim.rlim_max = 0;
	if (setrlimit(RLIMIT_CORE, &rlim) == -1)
		xerror("setrlimit: Cannot set RLIMIT_CORE: %s\n", strerror(errno));
}

/* Return the maximum number of files a process can have open. */
static int
get_open_max(void)
{
#ifdef _SC_OPEN_MAX
	return (int)sysconf(_SC_OPEN_MAX);
#else
	/* This is what getdtablesize(3) does */
	struct rlimit rlim;
	if (getrlimit(RLIMIT_NOFILE, &rlim) != -1)
		return (int)rlim.rlim_cur;
#endif /* _SC_OPEN_MAX */

#ifdef OPEN_MAX /* Not defined in Linux */
	return OPEN_MAX;
#elif defined(__linux__) && defined(NR_OPEN)
	return NR_OPEN;
#else
	return 256; /* Let's fallback to a sane default */
#endif /* OPEN_MAX */
}

/* Close all non-standard file descriptors (> 2) to avoid FD exhaustion. */
static void
sanitize_file_descriptors(void)
{
	int fd = 0, fds = get_open_max();

	for (fd = 3; fd < fds; fd++)
		close(fd);
}

static int
sanitize_shell_level(char *str)
{
	if (!str || !*str || !is_number(str))
		return EXIT_FAILURE;

	int a = atoi(str);
	if (a < 1 || a > MAX_SHELL_LEVEL)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

/* Sanitize the environment: set environ to NULL and then set a few
 * environment variables to get a minimally working environment.
 * Core dumps are disabled.
 * Umask is set to the most restrictive value: 0077. */
int
xsecure_env(const int mode)
{
	sanitize_file_descriptors();
	disable_coredumps();
	umask(0077);

	char *display = (char *)NULL,
		 *wayland_display = (char *)NULL,
		 *_term = (char *)NULL,
		 *tz = (char *)NULL,
		 *lang = (char *)NULL,
		 *fzfopts = (char *)NULL,
		 *clifm_level = (char *)NULL;

	clifm_level = getenv("CLIFMLVL");

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
	} else {
		if (clifm_level)
			nesting_level = 2; /* This is a nested instance */
	}

	xclearenv();
	set_path_env();
	xsetenv("IFS", " \t\n");

	if (mode == SECURE_ENV_FULL)
		return EXIT_SUCCESS;

	if (user.name)
		xsetenv("USER", user.name);
	if (user.home)
		xsetenv("HOME", user.home);
	if (user.shell)
		xsetenv("SHELL", user.shell);

	if (display && sanitize_cmd(display, SNT_DISPLAY) == EXIT_SUCCESS) {
		xsetenv("DISPLAY", display);
	} else {
		if (wayland_display)
			xsetenv("WAYLAND_DISPLAY", wayland_display);
	}

	if (clifm_level && sanitize_shell_level(clifm_level) == EXIT_SUCCESS)
		xsetenv("CLIFMLVL", clifm_level);

	if (_term && sanitize_cmd(_term, SNT_MISC) == EXIT_SUCCESS)
		xsetenv("TERM", _term);

	if (tz && sanitize_cmd(tz, SNT_MISC) == EXIT_SUCCESS)
		xsetenv("TZ", tz);

	if (lang && sanitize_cmd(lang, SNT_MISC) == EXIT_SUCCESS) {
		xsetenv("LANG", lang);
		xsetenv("LC_ALL", lang);
	} else {
		xsetenv("LC_ALL", "C");
	}

	if (fzfopts)
		xsetenv("FZF_DEFAULT_OPTS", fzfopts);

	return EXIT_SUCCESS;
}

/* Sanitize cmd string coming from the mimelist file */
static int
sanitize_mime(const char *cmd)
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
static int
sanitize_net(const char *cmd)
{
	if (strlen(cmd) > strspn(cmd, ALLOWED_CHARS_NET))
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

/* Sanitize value of DISPLAY env var */
static int
sanitize_misc(const char *str)
{
	if (strlen(str) > strspn(str, ALLOWED_CHARS_MISC))
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

/* Sanitize value of DISPLAY env var */
static int
sanitize_display(const char *str)
{
	if (strlen(str) > strspn(str, ALLOWED_CHARS_DISPLAY))
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

/* Sanitize cmd string coming from profile, prompt, and autocmds */
static int
sanitize_gral(const char *cmd)
{
	if (strlen(cmd) > strspn(cmd, ALLOWED_CHARS_GRAL))
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

/* Return EXIT_SUCCESS if at least one character in CMD is a non-escaped
 * blacklisted char (<>|;&$). Oterwise, return EXIT_FAILURE. */
static int
sanitize_blacklist(const char *cmd)
{
	if (!cmd || !*cmd)
		return EXIT_SUCCESS;

	size_t i;
	for (i = 0; cmd[i]; i++) {
		switch (cmd[i]) {
		case '<': /* fallthrough */
		case '>': /* fallthrough */
		case '|': /* fallthrough */
		case ';': /* fallthrough */
		case '&': /* fallthrough */
		case '$': /* fallthrough */
		case '`':
			if (i == 0 || cmd[i - 1] != '\\')
				return EXIT_FAILURE;
			break;
		default: break;
		}
	}

	return EXIT_SUCCESS;
}

/* Check if command name in STR contains slashes. Return 1 if found,
 * zero otherwise. This means: do not allow custom scripts or binaries,
 * but only whatever could be found in the sanitized PATH variable */
static int
clean_cmd(const char *str)
{
	if (!str || !*str)
		return EXIT_FAILURE;

	char *p = strchr(str, ' ');
	if (p)
		*p = '\0';

	char *q = strchr(str, '/');
	if (p)
		*p = ' ';

	if (q) {
		err('w', PRINT_PROMPT, _("%s: %s: Only command base names "
			"are allowed. Ex: 'nano' instead of '/usr/bin/nano'\n"),
			PROGRAM_NAME, str);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

/* Sanitize CMD according to TYPE. Returns EXIT_SUCCESS if command is safe or
 * EXIT_FAILURE if not. */
int
sanitize_cmd(const char *str, const int type)
{
	if (!str || !*str)
		return EXIT_FAILURE;

	int exit_status = EXIT_FAILURE;

	switch (type) {
	case SNT_MIME:
		if (clean_cmd(str) != EXIT_SUCCESS)
			/* Error message already pŕinted by clean_cmd() */
			return EXIT_FAILURE;
		exit_status = sanitize_mime(str);
		break;
	case SNT_NET: exit_status = sanitize_net(str); break;
	case SNT_DISPLAY: return sanitize_display(str);
	case SNT_MISC: return sanitize_misc(str);
	case SNT_PROFILE: /* fallthrough */
	case SNT_PROMPT:  /* fallthrough */
	case SNT_AUTOCMD: /* fallthrough */
	case SNT_GRAL:
		if (clean_cmd(str) != EXIT_SUCCESS)
			return EXIT_FAILURE;
		exit_status = sanitize_gral(str);
		break;
	case SNT_BLACKLIST: return sanitize_blacklist(str);
	case SNT_NONE: return EXIT_SUCCESS;
	default: return EXIT_SUCCESS;
	}

	if (exit_status == EXIT_FAILURE) {
		err('w', PRINT_PROMPT, "%s: %s: %s\n",
			PROGRAM_NAME, str, _(UNSAFE_CMD));
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
