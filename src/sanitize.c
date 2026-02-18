/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* sanitize.c -- functions for commands and environment sanitization */

#include "helpers.h"

#include <string.h>
#ifndef _BE_POSIX
# include <paths.h> /* _PATH_STDPATH */
#endif /* _BE_POSIX */
#include <errno.h>
#include <sys/resource.h> /* getrlimit(3), setrlimit(3) */
#include <unistd.h> /* close(2), sysconf(3) */

#include "aux.h"
#include "checks.h" /* is_number() */
#include "misc.h"
#include "sanitize.h"

#define UNSAFE_CMD "Unsafe command. Consult the manpage for more information"
/* If PATH cannot be retrieved from any other source, let's use this value. */
#if !defined(_PATH_STDPATH) && !defined(_CS_PATH)
# define MINIMAL_PATH "/usr/local/bin:/bin:/usr/bin:/sbin:/usr/sbin"
#endif

/* Unset environ: little implementation of clearenv(3), not available
 * on some systems (not POSIX). */
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
			namebuf = xnrealloc(namebuf, len + 1, sizeof(char));
			lastlen = len;
		}
		memcpy(namebuf, environ[0], len);
		namebuf[len] = '\0';
		if (unsetenv(namebuf) == -1) {
			xerror("%s: unsetenv: '%s': %s\n", PROGRAM_NAME,
				namebuf, strerror(errno));
			exit(FUNC_FAILURE);
		}
	}
#endif /* !__NetBSD__ && !__HAIKU__ && !__APPLE__ && !__TERMUX__ */
}

static void
set_path_env(void)
{
#if defined(_PATH_STDPATH)
	const int ret = setenv("PATH", _PATH_STDPATH, 1);
#elif defined(_CS_PATH)
	const size_t size = confstr(_CS_PATH, NULL, 0); /* Get value's size */
	char *buf = xnmalloc(size, sizeof(char));
	confstr(_CS_PATH, buf, size);             /* Get value */
	const int ret = setenv("PATH", buf, 1);   /* Set it */
	free(buf);
#else
	const int ret = setenv("PATH", MINIMAL_PATH, 1);
#endif /* _PATH_STDPATH */

	if (ret == -1) {
		xerror("%s: setenv: PATH: %s\n", PROGRAM_NAME, strerror(errno));
		exit(FUNC_FAILURE);
	}
}

static void
xsetenv(const char *name, const char *value)
{
	if (setenv(name, value, 1) == -1)
		xerror("%s: setenv: '%s': %s\n", PROGRAM_NAME, name, strerror(errno));
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
	/* This is what getdtablesize(3) does. */
	struct rlimit rlim;
	if (getrlimit(RLIMIT_NOFILE, &rlim) != -1)
		return (int)rlim.rlim_cur;
#endif /* _SC_OPEN_MAX */

#ifdef OPEN_MAX /* Not defined in Linux. */
	return OPEN_MAX;
#elif defined(__linux__) && defined(NR_OPEN)
	return NR_OPEN;
#else
	return 256; /* Let's fallback to a sane default. */
#endif /* OPEN_MAX */
}

/* Close all non-standard file descriptors (> 2) to avoid FD exhaustion. */
static void
sanitize_file_descriptors(void)
{
	const int fds = get_open_max();

	for (int fd = 3; fd < fds; fd++)
		close(fd);
}

static int
sanitize_shell_level(const char *str)
{
	if (!str || !*str || !is_number(str))
		return FUNC_FAILURE;

	const int a = xatoi(str);
	if (a < 1 || a > MAX_SHELL_LEVEL)
		return FUNC_FAILURE;

	return FUNC_SUCCESS;
}

/* Drop SUID/SGID privileges, if set. */
static void
drop_privs(void)
{
	const uid_t ruid = getuid();
	const uid_t euid = geteuid();
	const gid_t rgid = getgid();
	const gid_t egid = getegid();

	if (rgid != egid) {
		int error = 0;
#ifndef __linux__
		setegid(rgid);
		if (setgid(rgid) == -1) error = 1;
#else
		if (setregid(rgid, rgid) == -1) error = 1;
#endif /* __linux__ */
		if (error == 1 || setegid(egid) != -1 || getegid() != rgid) {
			fprintf(stderr, _("%s: Error dropping group privileges. "
				"Aborting.\n"), PROGRAM_NAME);
			exit(FUNC_FAILURE);
		}
	}

	if (ruid != euid) {
		int error = 0;
#ifndef __linux__
		seteuid(ruid);
		if (setuid(ruid) == -1) error = 1;
#else
		if (setreuid(ruid, ruid) == -1) error = 1;
#endif /* __linux__ */
		if (error == 1 || seteuid(euid) != -1 || geteuid() != ruid) {
			fprintf(stderr, _("%s: Error dropping user privileges. "
				"Aborting.\n"), PROGRAM_NAME);
			exit(FUNC_FAILURE);
		}
	}
}

/* Sanitize the environment: set environ to NULL and then set a few
 * environment variables to get a minimally working environment.
 * Non-standard file descriptors are closed.
 * Core dumps are disabled.
 * SUID/SGID privileges, if any, are dropped.
 * Umask is set to the most restrictive value: 0077. */
int
xsecure_env(const int mode)
{
	sanitize_file_descriptors();
	disable_coredumps();
	drop_privs();
	umask(0077); /* flawfinder: ignore */

	char *display = NULL;
	char *wayland_display = NULL;
	char *term_env = NULL;
	char *tz = NULL;
	char *lang = NULL;
	char *fzfopts = NULL;
	char *clifm_level = NULL;

	clifm_level = xgetenv("CLIFMLVL", 1);

	if (mode != SECURE_ENV_FULL) {
		/* Let's keep these values from the current environment. */
		display = xgetenv("DISPLAY", 1);
		if (!display)
			wayland_display = xgetenv("WAYLAND_DISPLAY", 1);
		term_env = xgetenv("TERM", 1);
		tz = xgetenv("TZ", 1);
		lang = xgetenv("LANG", 1);
		if (fzftab)
			fzfopts = xgetenv("FZF_DEFAULT_OPTS", 1);
	} else {
		if (clifm_level)
			nesting_level = 2; /* This is a nested instance. */
	}

	xclearenv();
	set_path_env();
	xsetenv("IFS", " \t\n");

	if (user.name)
		xsetenv("USER", user.name);
	if (user.home)
		xsetenv("HOME", user.home);
	if (user.shell)
		xsetenv("SHELL", user.shell);

	if (mode == SECURE_ENV_FULL)
		goto END;

	if (display && sanitize_cmd(display, SNT_DISPLAY) == FUNC_SUCCESS) {
		xsetenv("DISPLAY", display);
	} else {
		if (wayland_display)
			xsetenv("WAYLAND_DISPLAY", wayland_display);
	}

	if (clifm_level && sanitize_shell_level(clifm_level) == FUNC_SUCCESS)
		xsetenv("CLIFMLVL", clifm_level);

	if (term_env && sanitize_cmd(term_env, SNT_MISC) == FUNC_SUCCESS)
		xsetenv("TERM", term_env);

	if (tz && sanitize_cmd(tz, SNT_MISC) == FUNC_SUCCESS)
		xsetenv("TZ", tz);

	if (lang && sanitize_cmd(lang, SNT_MISC) == FUNC_SUCCESS) {
		xsetenv("LANG", lang);
		xsetenv("LC_ALL", lang);
	} else {
		xsetenv("LC_ALL", "C");
	}

	if (fzfopts)
		xsetenv("FZF_DEFAULT_OPTS", fzfopts);

END:
	free(display);
	free(wayland_display);
	free(clifm_level);
	free(term_env);
	free(tz);
	free(lang);
	free(fzfopts);

	return FUNC_SUCCESS;
}

/* Sanitize cmd string coming from the mimelist file. */
static int
sanitize_mime(const char *cmd)
{
	const char *p = cmd;
	while (*p) {
		/* Only %[fxum] is allowed. */
		if (*p == '%' && p[1] != 'f' && p[1] != 'x'
		&& p[1] != 'u' && p[1] != 'm')
			return FUNC_FAILURE;

		/* Disallow double ampersand. */
		if (*p == '&' && p[1] == '&')
			return FUNC_FAILURE;

		p++;
	}

	const size_t cmd_len = (size_t)(p - cmd);
	if (cmd_len > strspn(cmd, ALLOWED_CHARS_MIME))
		return FUNC_FAILURE;

	return FUNC_SUCCESS;
}

/* Sanitize the string CMD using WHITELIST as whitelist. */
static int
sanitize_whitelist(const char *cmd, const char *whitelist)
{
	if (strlen(cmd) > strspn(cmd, whitelist))
		return FUNC_FAILURE;

	return FUNC_SUCCESS;
}

/* Return FUNC_SUCCESS if at least one character in CMD is a non-escaped
 * blacklisted char (<>|;&$). Oterwise, return FUNC_FAILURE. */
static int
sanitize_blacklist(const char *cmd)
{
	if (!cmd || !*cmd)
		return FUNC_SUCCESS;

	for (size_t i = 0; cmd[i]; i++) {
		switch (cmd[i]) {
		case '<': /* fallthrough */
		case '>': /* fallthrough */
		case '|': /* fallthrough */
		case ';': /* fallthrough */
		case '&': /* fallthrough */
		case '$': /* fallthrough */
		case '`':
			if (i == 0 || cmd[i - 1] != '\\')
				return FUNC_FAILURE;
			break;
		default: break;
		}
	}

	return FUNC_SUCCESS;
}

/* Check if command name in STR contains slashes. Return 1 if found,
 * zero otherwise. This means: do not allow custom scripts or binaries,
 * but only whatever can be found in the sanitized PATH variable. */
static int
clean_cmd(char *str)
{
	if (!str || !*str)
		return FUNC_FAILURE;

	char *space = strchr(str, ' ');
	if (space)
		*space = '\0';

	const char *slash = strchr(str, '/');
	if (space)
		*space = ' ';

	if (slash) {
		err('w', PRINT_PROMPT, _("%s: '%s': Only command base names "
			"are allowed. E.g., 'nano' instead of '/usr/bin/nano'\n"),
			PROGRAM_NAME, str);
		return FUNC_FAILURE;
	}

	return FUNC_SUCCESS;
}

/* Sanitize CMD according to TYPE. Returns FUNC_SUCCESS if command is safe or
 * FUNC_FAILURE if not. */
int
sanitize_cmd(const char *cmd, const int type)
{
	if (!cmd || !*cmd)
		return FUNC_FAILURE;

	char *str = strdup(cmd);
	if (!str)
		return FUNC_FAILURE;

	int ret = FUNC_FAILURE;

	switch (type) {
	case SNT_MIME:
		if (clean_cmd(str) != FUNC_SUCCESS) {
			/* Error message already printed by clean_cmd() */
			free(str);
			return FUNC_FAILURE;
		}
		ret = sanitize_mime(str);
		break;
	case SNT_NET:
		ret = sanitize_whitelist(str, ALLOWED_CHARS_NET); break;
	case SNT_DISPLAY:
		ret = sanitize_whitelist(str, ALLOWED_CHARS_DISPLAY); break;
	case SNT_MISC: ret = sanitize_whitelist(str, ALLOWED_CHARS_MISC); break;
	case SNT_PROFILE: /* fallthrough */
	case SNT_PROMPT:  /* fallthrough */
	case SNT_AUTOCMD: /* fallthrough */
	case SNT_GRAL:
		if (clean_cmd(str) != FUNC_SUCCESS) {
			free(str);
			return FUNC_FAILURE;
		}
		ret = sanitize_whitelist(str, ALLOWED_CHARS_GRAL);
		break;
	case SNT_BLACKLIST: ret = sanitize_blacklist(str); break;
	case SNT_NONE: /* fallthrough */
	default: free(str); return FUNC_SUCCESS;
	}

	if (ret == FUNC_FAILURE) {
		err('w', PRINT_PROMPT, "%s: '%s': %s\n",
			PROGRAM_NAME, str, _(UNSAFE_CMD));
		free(str);
		return FUNC_FAILURE;
	}

	free(str);
	return FUNC_SUCCESS;
}
