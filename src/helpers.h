/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2016-2025, L. Abramovich <leo.clifm@outlook.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
*/

/* helpers.h -- main header file */

#ifndef HELPERS_H
#define HELPERS_H

#define PROGRAM_NAME_UPPERCASE "Clifm"
#define PROGRAM_NAME "clifm"
#define PROGRAM_DESC "The command line file manager"
#define VERSION "1.25.8"
#define DATE "Jul 6, 2025"
#define AUTHOR "L. Abramovich"
#define CONTACT "https://github.com/leo-arch/clifm"
#define LICENSE "GPL2+"
#define COLORS_REPO "https://github.com/leo-arch/clifm-colors"

/* POSIX_STRICT: A more descriptive name for _BE_POSIX */
#if defined(POSIX_STRICT) || defined(_POSIX_C_SOURCE) || defined(_XOPEN_SOURCE)
# define _BE_POSIX
#endif /* POSIX_STRICT || _POSIX_C_SOURCE || _XOPEN_SOURCE */

#ifdef _BE_POSIX
# ifndef _POSIX_C_SOURCE
#  define _POSIX_C_SOURCE 200809L
# endif /* !_POSIX_C_SOURCE */
# ifndef _XOPEN_SOURCE
#  define _XOPEN_SOURCE 700
# endif /* !_XOPEN_SOURCE */

# if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) \
|| defined(__DragonFly__) || defined(__APPLE__)
   typedef unsigned char  u_char;
   typedef unsigned short u_short;
   typedef unsigned int   u_int;
   typedef unsigned long  u_long;
#  ifdef __NetBSD__
/* scandir, alphasort, and dirfd are POSIX (not even XSI extensions). Why
 * hidden behind _NETBSD_SOURCE? */
#   define _NETBSD_SOURCE
#  endif /* __NetBSD__ */
# elif defined(__sun)
#  define __EXTENSIONS__
# endif /* BSD */

#else /* !POSIX */
# if defined(__linux__) || defined(__CYGWIN__)
#  define _GNU_SOURCE
# elif defined(__APPLE__)
#  define _DARWIN_C_SOURCE
# endif /* __linux__ || __CYGWIN__ */
#endif /* _BE_POSIX */

#ifndef __BEGIN_DECLS
# ifdef __cpluplus
#  define __BEGIN_DECLS extern "C" {
# else
#  define __BEGIN_DECLS
# endif /* __cplusplus */
#endif /* !__BEGIN_DECLS */

#ifndef __END_DECLS
# ifdef __cpluplus
#  define __END_DECLS }
# else
#  define __END_DECLS
# endif /* __cplusplus */
#endif /* !__END_DECLS */

#ifdef __TINYC__
# define __STDC_NO_VLA__ 1
//# include <sys/cdefs.h>
#endif /* __TINYC__ */

#define _FILE_OFFSET_BITS 64 /* Support large files */
#define _TIME_BITS 64 /* Address Y2038 problem in 32-bit machines */

#define xstrcasestr strcasestr

#ifdef _BE_POSIX
# undef  xstrcasestr
# define xstrcasestr x_strcasestr
# ifndef _NO_GETTEXT
#  define _NO_GETTEXT
# endif /* !_NO_GETTEXT */
# ifndef _NO_MAGIC
#  define _NO_MAGIC
# endif /* !_NO_MAGIC */
#endif /* _BE_POSIX */

/* _NO_LIRA implies _NO_MAGIC */
#if defined(_NO_LIRA) && !defined(_NO_MAGIC)
# define _NO_MAGIC
#endif /* _NO_LIRA && !_NO_MAGIC */

#if defined(USE_DU1)
# if (defined(__linux__) || defined(__CYGWIN__) || defined(__HAIKU__)) \
&& !defined(_BE_POSIX)
/* du(1) can report sizes in bytes, apparent sizes, and take custom block sizes */
#  define HAVE_GNU_DU
# endif /* (__linux__ || __CYGWIN__ || __HAIKU__) && !_BE_POSIX */
#endif /* USE_DU1 */

#ifndef _NO_GETTEXT
# include <libintl.h>
#endif /* !_NO_GETTEXT*/

#include <glob.h>
#include <limits.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>  /* S_BLKSIZE */
#include <sys/types.h> /* ssize_t */
#include <fcntl.h>     /* AT_* constants (like AT_FDCWD) */
/* Included here to test _DIRENT_HAVE_D_TYPE and DT macros. */
#include <dirent.h>

#if !defined(__GLIBC__) && !defined(_DIRENT_HAVE_D_TPYE)
# if !defined(__sun) && !defined(__HAIKU__)
#  define _DIRENT_HAVE_D_TYPE
# endif /* !__sun && !__HAIKU__ */
#endif /* !__GLIBC__ && !_DIRENT_HAVE_D_TYPE */

#if defined(_BE_POSIX) && defined(_DIRENT_HAVE_D_TYPE)
# undef _DIRENT_HAVE_D_TYPE
#endif /* _BE_POSIX && _DIRENT_HAVE_D_TYPE */

#if !defined(TIGHT_COLUMNS) && !defined(NO_TIGHT_COLUMNS)
# define TIGHT_COLUMNS
#endif /* !TIGHT_COLUMNS && !NO_TIGHT_COLUMNS */

#ifdef CLIFM_LEGACY
/* Replace functions not available before POSIX-1.2008. More precisely,
 * let's try to be POSIX-1.2001 compliant. This is still experimental:
 * a crash was reported due to x_scandir(), the scandir(3) replacement
 * (see https://github.com/leo-arch/clifm/discussions/254). */
# include "compat.h"
# define xrealpath old_realpath
#else
# define xrealpath realpath
#endif /* CLIFM_LEGACY */

#ifdef __sun
/* Solaris/Illumos defines AT_FDCWD as 0xffd19553 (-3041965); without the int
 * cast, the value gets interpreted as uint (4291925331), which throws compiler
 * warnings. See https://github.com/python/cpython/issues/60169 */
# define XAT_FDCWD (int)AT_FDCWD
# ifndef _BE_POSIX
#  define SOLARIS_DOORS
# endif /* !_BE_POSIX */
#else
# define XAT_FDCWD AT_FDCWD /* defined in fcntl.h */
#endif /* __sun */

#if !defined(_DIRENT_HAVE_D_TYPE) || !defined(DT_DIR)
/* Systems not providing a d_type member for the stat struct do not provide
 * these macros either. We use them to convert an st_mode value to the
 * appropriate d_type value (via get_dt()). */
# define DT_UNKNOWN 0
# define DT_FIFO    1
# define DT_CHR     2
# define DT_DIR     4
# define DT_BLK     6
# define DT_REG     8
# define DT_LNK     10
# define DT_SOCK    12
# define DT_WHT     14
# ifdef __sun
#  define DT_DOOR   16
#  define DT_PORT   18 /* Event port */
# endif /* __sun */
#endif /* !_DIRENT_HAVE_D_TYPE || !DT_DIR */

/* Some extra file types */
#define DT_SHM      100 /* Shared memory object file */
#define DT_SEM      102 /* Semaphore file */
#define DT_MQ       104 /* Message queue file */
#define DT_TPO      106 /* Typed memory object file */
#ifdef S_ARCH1
# define DT_ARCH1   108 /* Archive state 1 (NetBSD) */
# define DT_ARCH2   110 /* Archive state 2 (NetBSD) */
#endif /* S_ARCH1 */

#ifndef DT_WHT
# define DT_WHT     14 /* Whiteout (FreeBSD/NetBSD/DragonFly/MacOS)*/
#endif /* DT_WHT */

/* If any of these file type checks isn't available, fake it */
#ifndef S_TYPEISMQ
# define S_TYPEISMQ(s)  (0)
#endif /* !S_TYPEISMQ */
#ifndef S_TYPEISSEM
# define S_TYPEISSEM(s) (0)
#endif /* !S_TYPEISSEM */
#ifndef S_TYPEISSHM
# define S_TYPEISSHM(s) (0)
#endif /* !S_TYPEISSHM */
#ifndef S_TYPEISTMO
# define S_TYPEISTMO(s) (0)
#endif /* !S_TYPEISTMO */
/* About these extra file types see
 * https://pubs.opengroup.org/onlinepubs/007904875/basedefs/sys/stat.h.html
 * NOTE: On Linux/Solaris, all these macros always evaluate to zero (false).
 * They don't seem to be present on BSD systems either.
 * However, the following coreutils programs perform checks for these
 * file types (mostly S_TYPEISSHM): split(1), sort(1), shred(1), dd(1),
 * du(1), head(1), tail(1), truncate(1), od(1), wc(1), and shuf(1). */
/* ASCII representation of these file types:
 * 'F': semaphore
 * 'Q': message queue
 * 'S': Shared memory object
 * 'T': Typed memory object */

/* Other non-standard file types:
if (S_ISCTG(mode)) return 'C'; high performance (contiguous data) file
if (S_ISMPB(mode) || S_ISMPC (bits) || S_ISMPX (bits)) return 'm'; // V7
if (S_ISNWK(mode)) return 'n'; // HP/UX: network special file
* S_IFNAM // Solaris/XENIX: special named file
* S_INSEM // Solaris/XENIX: semaphore subtype of IFNAM
* S_INSHD // Solaris/XENIX: shared data subtype of IFNAM
* S_IFDB  // DragonFly: Database record file (DT_DBF). Recognized by neither ls nor find
*
* For a quite comphensive list of non-standard file types see:
* https://github.com/python/cpython/issues/55225#issuecomment-1093532804 */

/* Setting GLOB_BRACE to ZERO disables support for GLOB_BRACE if not
 * available on current platform. Same for GLOB_TILDE. */
#ifndef GLOB_BRACE
# define GLOB_BRACE 0
#endif /* GLOB_BRACE */
#ifndef GLOB_TILDE
# define GLOB_TILDE 0
#endif /* GLOB_TILDE */

#if defined(__linux__)
# include <linux/version.h>
# include <linux/limits.h>
# if !defined(__GLIBC__) || (__GLIBC__ > 2 \
|| (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 9)) \
&& LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
#  define HAVE_INOTIFY
# endif /* GLIBC >= 2.9 && linux >= 2.6.27 */
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) \
|| defined(__DragonFly__) || defined(__APPLE__)
# include <sys/time.h>
# include <sys/param.h>
# include <sys/syslimits.h>
# if defined(__FreeBSD__) && __FreeBSD_version >= 410000
#  define HAVE_KQUEUE
# elif defined(__NetBSD__)
#  if __NetBSD_Prereq__(2,0,0)
#   define HAVE_KQUEUE
#  endif /* NetBSD >= 2.0 */
# elif defined(__OpenBSD__) && OpenBSD >= 200106 /* version 2.9 */
#  define HAVE_KQUEUE
# elif defined(__DragonFly__) && __DragonFly_version >= 200800 /* At least 2.8*/
#  define HAVE_KQUEUE
# elif defined(__APPLE__)
#  define HAVE_KQUEUE
# endif /* FreeBSD >= 4.1 */
#elif defined(__sun) || defined(__CYGWIN__)
# include <sys/time.h>
#elif defined(__HAIKU__)
# include <stdint.h> /* uint8_t */
#endif /* __linux__ */

#if !defined(_BE_POSIX)
# if defined(__GLIBC__) \
|| (defined(__FreeBSD__) && __FreeBSD_version >= 604000) \
|| (defined(__OpenBSD__) && OpenBSD >= 200805) \
|| (defined(__sun) && defined(SUN_VERSION) && SUN_VERSION >= 511)
#  define HAVE_MEMRCHR /* OpenBSD 200805 == version 4.3 */
# elif defined(__NetBSD__)
#  if __NetBSD_Prereq__(6,0,0)
#   define HAVE_MEMRCHR
#  endif
# endif /* __GLIBC__ */
#endif /* !_BE_POSIX */

#if defined(__OpenBSD__) || defined(__NetBSD__) \
|| defined(__FreeBSD__) || defined(__APPLE__)
# include <inttypes.h> /* uintmax_t, intmax_t */
#endif /* BSD */

/* Filesystem event monitors (inotify and kqueue) are OS-specific.
 * Let's fallback to our own generic monitor. */
#if defined(_BE_POSIX) && !defined(USE_GENERIC_FS_MONITOR)
# define USE_GENERIC_FS_MONITOR
#endif /* _BE_POSIX && !USE_GENERIC_FS_MONITOR */

#if defined(__linux__)
# if defined(HAVE_INOTIFY) && !defined(USE_GENERIC_FS_MONITOR)
#  include <sys/inotify.h>
#  define LINUX_INOTIFY
# else
#  include <stdint.h> /* uint8_t */
# endif /* HAVE_INOTIFY && !USE_GENERIC_FS_MONITOR */
#endif /* __linux__ */

#if defined(HAVE_KQUEUE) && !defined(USE_GENERIC_FS_MONITOR)
# include <sys/event.h>
# define BSD_KQUEUE
#endif /* HAVE_KQUEUE && !USE_GENERIC_FS_MONITOR */

/* Before MacOS X 10.10, renameat(2) is declared in sys/stdio.h */
#if defined(__APPLE__) && !defined(CLIFM_LEGACY)
# include <AvailabilityMacros.h>
# if MAC_OS_X_VERSION_MIN_REQUIRED < 101000
#  define MAC_OS_X_RENAMEAT_SYS_STDIO_H
# endif /* MACOS_X < 10.10 */
#endif /* __APPLE__ */

#include "strings.h"
#include "settings.h"
#include "term.h"

/* General exit codes for functions */
#define FUNC_SUCCESS 0 /* True/success */
#define FUNC_FAILURE 1 /* False/error */

#ifndef PATH_MAX
# ifdef __linux__
#  define PATH_MAX 4096
# else
#  ifdef MAXPATHLEN
#   define PATH_MAX MAXPATHLEN
#  else
#   define PATH_MAX 1024
#  endif /* MAXPATHLEN */
# endif /* __linux__ */
#endif /* !PATH_MAX */

#ifndef HOST_NAME_MAX
# if defined(__ANDROID__)
#  define HOST_NAME_MAX 255
# else
#  define HOST_NAME_MAX 64
# endif /* __ANDROID__ */
#endif /* !HOST_NAME_MAX */

#ifndef NAME_MAX
# define NAME_MAX 255
#endif /* !NAME_MAX */

/* Used by FILE_SIZE and FILE_SIZE_PTR macros to calculate file sizes */
#ifndef S_BLKSIZE /* Not defined in Termux/Solaris */
# include <sys/param.h>
# ifdef DEV_BSIZE
#  define S_BLKSIZE DEV_BSIZE
# else
#  define S_BLKSIZE 512
# endif /* DEV_BSIZE */
#endif /* !S_BLKSIZE */
/* Linux-glibc/FreeBSD/NetBSD/OpenBSD: S_BLKSIZE/DEV_BSIZE = 512.
 * CYGWIN: S_BLKSIZE/DEV_BSIZE = 1024.
 * Solaris/Termux/Linux-musl: S_BLKSIZE is unset, DEV_BSIZE = 512.
 * DragonFly/MacOS/Haiku: S_BLKSIZE = 512 (DEV_BSIZE is unset). */

#ifndef ARG_MAX
# ifdef __linux__
#  define ARG_MAX (128 * 1024)
# else
#  define ARG_MAX (512 * 1024)
# endif /* __linux__ */
#endif /* !ARG_MAX */

#if defined(__linux__) && !defined(_BE_POSIX)
# if defined(STATX_TYPE) && (!defined(__ANDROID__) || __ANDROID_API__ >= 30)
#  define LINUX_STATX
# endif /* STATX_TYPE */
# if !defined(__GLIBC__) || (__GLIBC__ > 2 \
|| (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 3))
#  if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
#   define LINUX_FILE_XATTRS
#  endif /* LINUX_VERSION (2.4) */
# endif /* !__GLIBC__ || __GLIBC__ >= 2.3 */
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
#  define LINUX_FILE_CAPS
# endif /* LINUX_VERSION (2.6.24)*/
# ifndef __TERMUX__
#  define LINUX_FILE_ATTRS
# endif /* !__TERMUX__ */
#endif /* __linux__ && !_BE_POSIX */

/* Do we have files birth time? If yes, define ST_BTIME. */
/* ST_BTIME is the timespec struct for files creation time. Valid fields are
 * tv_sec and tv_nsec. */
#ifndef _BE_POSIX
# ifdef LINUX_STATX
#  define ST_BTIME stx_btime
/* OpenBSD defines the interface (see sys/stat.h), but the filesystem doesn't
 * actually store creation times: the value of __st_birthtim is always zero.
#elif defined(__OpenBSD__)
# define ST_BTIME __st_birthtim */
# elif defined(__NetBSD__)
#  define ST_BTIME st_birthtimespec
# elif defined(__APPLE__)
#  include <AvailabilityMacros.h>
#  if MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
#   define ST_BTIME st_birthtimespec
#  endif
# elif defined(__FreeBSD__) || defined(__CYGWIN__)
#  define ST_BTIME st_birthtim
# elif defined(__HAIKU__)
#  define ST_BTIME st_crtim
# elif defined(__sun) && !defined(_NO_SUN_BIRTHTIME)
/* In the case of Solaris, ST_BTIME is just a flag telling we should run
 * get_birthtime() to get files creation time. */
#  define ST_BTIME
# endif /* LINUX_STATX */
#endif /* !_BE_POSIX */

/* Do we have birth time when running in long view AND light mode? Only if
 * the timestamp is provided directly by stat(2).
 * For the time being, we need an extra call (statx(2)) on Linux. This should
 * be fixed. */
#if defined(ST_BTIME) && !defined(LINUX_STATX) && !defined(__sun)
# define ST_BTIME_LIGHT
#endif /* ST_BTIME && !LINUX_STATX && !__sun */

/* Filesystem events handling */
#if defined(LINUX_INOTIFY)
# define NUM_EVENT_SLOTS 32 /* Make room for 32 events */
# define EVENT_SIZE (sizeof(struct inotify_event))
# define EVENT_BUF_LEN (EVENT_SIZE * NUM_EVENT_SLOTS)
extern int inotify_fd, inotify_wd;
extern unsigned int INOTIFY_MASK;
extern int watch;

#elif defined(BSD_KQUEUE)
# define NUM_EVENT_SLOTS 10
# define NUM_EVENT_FDS   10
extern int kq, event_fd;
extern struct kevent events_to_monitor[];
extern unsigned int KQUEUE_FFLAGS;
extern struct timespec timeout;
extern int watch;

#else
# define GENERIC_FS_MONITOR
extern time_t curdir_mtime;
#endif /* LINUX_INOTIFY */

/* Do we have arc4random_uniform(3). If not, fallback to random(3). */
#if !defined(_NO_ARC4RANDOM) && !defined(_BE_POSIX) && !defined(__HAIKU__)
# if defined(__FreeBSD__) && __FreeBSD_version >= 800041
#  define HAVE_ARC4RANDOM
# elif defined(__NetBSD__)
#  if __NetBSD_Prereq__(6,0,0)
#   define HAVE_ARC4RANDOM
#  endif /* NetBSD >= 6.0 */
# elif defined(__OpenBSD__) && OpenBSD >= 200806 /* version 4.4 */
#  define HAVE_ARC4RANDOM
# elif defined(__DragonFly__) && __DragonFly_version >= 400600
/* At least since 4.6 (Sep 2016). See https://gitweb.dragonflybsd.org/dragonfly.git/commitdiff/a2cdfb90273ff84696f4103580173cce21c12e2b
 * It might be less though. */
#  define HAVE_ARC4RANDOM
# elif defined(__sun) && defined(SUN_VERSION) && SUN_VERSION >= 511
/* 5.11 at least: it might be less. */
#  define HAVE_ARC4RANDOM
# elif defined(__linux__) && defined(__GLIBC__) && (__GLIBC__ > 2 \
|| (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 36))
#  define HAVE_ARC4RANDOM
# elif defined(__CYGWIN__) || (defined(__linux__) && defined(__ANDROID__))
#  define HAVE_ARC4RANDOM
# endif /* __FreeBSD__ */
#endif /* !_NO_ARC4RANDOM && !_BE_POSIX && !__HAIKU__ */

#if !defined(_BE_POSIX) && (defined(__FreeBSD__) || defined(__OpenBSD__) \
|| defined(__DragonFly__) || defined(__APPLE__))
# define HAVE_STATFS
#endif

#if !defined(_BE_POSIX) && !defined(NO_PLEDGE) \
&& defined(__OpenBSD__) && OpenBSD >= 201603 /* 5.9? CHECK! */
# define HAVE_PLEDGE
#endif

#if !defined(_BE_POSIX) && (defined(__linux__) || defined(__CYGWIN__))
# define LINUX_FSINFO
#endif

#define DEV_NO_NAME "-" /* String used when no filesystem name/type is found */

#define TRUECOLOR_NUM 16777216

/* This is a more or less arbitrary value, but better than some huge value
 * like INT_MAX (which most likely will cause problems long before reaching
 * this value). */
#define MAX_SHELL_LEVEL 1000 /* This is the value used by bash 5.2 */

#define MAX_UMASK 0777

#define MAX_SEL       INT_MAX
#define MAX_TRASH     INT_MAX
#define MAX_BOOKMARKS INT_MAX

/* Max length of a file size in human format */
#define MAX_HUMAN_SIZE 10 /* "1023.99YB\0" */

/* The following flags are used via an integer (FLAGS). If an integer has
 * 4 bytes, then we can use a total of 32 flags (0-31)
 * 4 * 8 == 32 bits == (1 << 31)
 * NOTE: setting (1 << 31) gives a negative value: DON'T USE
 * NOTE 2: What if int size isn't 4 bytes or more, but 2 (16 bits)? In this
 * case, if we want to support old 16 bit machines, we shouldn't use more than
 * 16 bits per flag, that is (1 << 15) */

/* Internal flags */
#define GUI                 (1 << 0)
#define IS_USRVAR_DEF       (1 << 1)

/* Used by the refresh on resize feature */
#define DELAYED_REFRESH     (1 << 2)
#define FIRST_WORD_IS_ELN   (1 << 3)
#define IN_BOOKMARKS_SCREEN (1 << 4)
#define STATE_COMPLETING    (1 << 5)
/* Instead of a completion for the current query, a BAEJ suggestion points to
 * a possible completion as follows: QUERY > COMPLETION */
#define BAEJ_SUGGESTION     (1 << 6)
#define STATE_SUGGESTING    (1 << 7)
#define IN_SELBOX_SCREEN    (1 << 8)
#define MULTI_SEL           (1 << 9)
#define PREVIEWER           (1 << 10)
#define NO_FIX_RL_POINT     (1 << 11)
#define FAILED_ALIAS        (1 << 12)
#define ALT_PREVIEW_FILE    (1 << 13)
#define UEBERZUG_IMG_PREV   (1 << 14)

/* Flags for third party binaries */
#define FZF_BIN_OK     (1 << 0)
#define FNF_BIN_OK     (1 << 1)
#define SMENU_BIN_OK   (1 << 2)
#ifdef USE_DU1
 #define GNU_DU_BIN_DU  (1 << 3)
 /* 'gdu' is the GNU version of 'du' used by BSD systems */
 #define GNU_DU_BIN_GDU (1 << 4)
#endif /* USE_DU1 */

/* In BSD/Solaris systems, GNU utilities are provided as gcp, gmv, grm,
 * gdu, etc.
 * NOTE: Both FreeBSD and DragonFly native utilities provide all the
 * functionalities we need, so that there's no need to check for the
 * GNU versions. */
#define BSD_HAVE_COREUTILS (1 << 5)
#if (defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__) \
|| defined(__sun)) && !defined(_BE_POSIX)
# define CHECK_COREUTILS
#endif /* (__OpenBSD__ || __NetBSD__ || __APPLE__ || __sun) && !_BE_POSIX */

/* Flag to control the search function behavior */
#define NO_GLOB_CHAR (1 << 0)

/* Control show hidden files mode (true, false, first, last) */
#define HIDDEN_FALSE 0
#define HIDDEN_TRUE  1
#define HIDDEN_FIRST 2
#define HIDDEN_LAST  3

#define DESKTOP_NOTIF_NONE   0
#define DESKTOP_NOTIF_SYSTEM 1
#define DESKTOP_NOTIF_KITTY  2

/* Search strategy */
#define GLOB_ONLY  0
#define REGEX_ONLY 1
#define GLOB_REGEX 2

#define GLOB_CHARS "*?[{"
#define GLOB_REGEX_CHARS "*?[{|^+$."

/* Possible values for PagerView (conf.pager_view) */
#define PAGER_AUTO  0
#define PAGER_LONG  1
#define PAGER_SHORT 2

/* ClearScreen=internal: clear screen only if command is internal */
#define CLEAR_INTERNAL_CMD_ONLY 2

/* Used by log_msg() to know wether to tell prompt() to print messages or not */
#define PRINT_PROMPT   1
#define NOPRINT_PROMPT 0

/* Terminal columns taken by the last line of the default prompt */
#define FALLBACK_PROMPT_OFFSET 6

/* A few macros for the err function */
#define ERR_NO_LOG   (-1) /* err prints but doesn't log */
#define ERR_NO_STORE (-2) /* err prints and logs, but doesn't store the msg into the messages array */

/* Macros for xchdir (for setting term title or not) */
#define SET_TITLE 1
#define NO_TITLE  0

/* Macros for cd_function */
#define CD_PRINT_ERROR    1
#define CD_NO_PRINT_ERROR 0

/* Macros for the count_dir function. CPOP tells the function to only
 * check if a given directory is populated (it has at least 3 files) */
#define CPOP    1
#define NO_CPOP 0

#define BACKGROUND 1
#define FOREGROUND 0

#define EXEC_BG_PROC 0
#define EXEC_FG_PROC 1

#define E_NOEXEC    126
#define E_NOTFOUND  127
#define E_SIGINT    128

#define NOTFOUND_MSG "Command not found"
#define NOEXEC_MSG   "Permission denied"

/* A few fixed colors */
#define BOLD ((xargs.no_bold != 1 && conf.colorize == 1) ? "\x1b[1m" : "")
/* NC: Reset color attributes to terminal defaults */
#define NC   (conf.colorize == 1 ? "\x1b[0m" : "")
/* \001 and \002 tell readline that color codes between them are
 * non-printing chars. This is specially useful for the prompt, i.e.,
 * when passing color codes to readline. */
#define RL_NC "\001\x1b[0m\002"

#define SET_MSG_PTR (term_caps.unicode == 1 ? MSG_PTR_STR_U : MSG_PTR_STR)
#define SET_SUCCESS_PTR (term_caps.unicode == 1 ? SUCCESS_PTR_STR_U : SUCCESS_PTR_STR)
#define SET_MISC_PTR (term_caps.unicode == 1 ? MISC_PTR_U : MISC_PTR)
/* Unused
#define SET_ERROR_PTR (term_caps.unicode == 1 ? ERROR_PTR_STR_U : ERROR_PTR_STR)
*/

#define UNSET (-1)

/* Macros for the update_autocmd_opts() function */
#define AC_COLOR_SCHEME  0
#define AC_FILES_COUNTER 1
#define AC_FILTER        2
#define AC_FULL_DIR_SIZE 3
#define AC_LIGHT_MODE    4
#define AC_LONG_VIEW     5
#define AC_MAX_FILES     6
#define AC_MAX_NAME_LEN  7
#define AC_ONLY_DIRS     8
#define AC_PAGER         9
#define AC_SHOW_HIDDEN   10
#define AC_SORT          11

/* Macros for the cp and mv cmds */
#define CP_CP            0 /* cp -Rp */
#define CP_CP_FORCE      1 /* cp -Rp */
#define CP_ADVCP         2 /* advcp -gRp */
#define CP_ADVCP_FORCE   3 /* advcp -gRp */
#define CP_WCP           4 /* wcp */
#define CP_RSYNC         5 /* rsync -avP */
#define CP_CMD_AVAILABLE 6

#define MV_MV            0 /* mv */
#define MV_MV_FORCE      1 /* mv */
#define MV_ADVMV         2 /* advmv -g */
#define MV_ADVMV_FORCE   3 /* advmv -g */
#define MV_CMD_AVAILABLE 4

/* Macros for LinkCreationMode */
#define LNK_CREAT_REG 0 /* Like ln -s */
#define LNK_CREAT_REL 1 /* Like ln -rs */
#define LNK_CREAT_ABS 2 /* Create absolute target */

/* Macros for listing_mode */
#define VERTLIST 0 /* ls-like listing mode */
#define HORLIST  1

/* Sort macros */
#define SNONE      0
#define SNAME      1
#define STSIZE     2
#define SATIME     3
#define SBTIME     4
#define SCTIME     5
#define SMTIME     6
#define SVER       7
#define SEXT       8
#define SINO       9
#define SOWN       10
#define SGRP       11
#define SBLK       12
#define SLNK       13
#define STYPE      14
#define SORT_TYPES 14

/* Macros for the colors_list function */
#define NO_ELN        0
#define NO_NEWLINE    0
#define NO_PAD        0
#define PRINT_NEWLINE 1

/* A few key macros used by the auto-suggestions system */
#define KEY_ESC       27
#define KEY_TAB       9
#define KEY_BACKSPACE 8
#define KEY_DELETE    127
#define KEY_ENTER     13

/* Macros to specify suggestions type */
#define NO_SUG         0
#define HIST_SUG       1
#define FILE_SUG       2
#define CMD_SUG        3
#define INT_CMD        4
#define COMP_SUG       5
#define BOOKMARK_SUG   6
#define ALIAS_SUG      7
#define ELN_SUG        8
#define FIRST_WORD     9
#define JCMD_SUG       10
/*#define JCMD_SUG_NOACD 11 // No auto-cd */
#define VAR_SUG	       12
#define SEL_SUG	       13
#define BACKDIR_SUG	   14
#define TAGT_SUG       15 /* t:TAG (expand tag TAG) */
#define TAGC_SUG       16 /* :TAG (param to tag command) */
#define TAGS_SUG       17 /* TAG  (param to tag command) */
#define BM_NAME_SUG    18 /* Bookmark names */
#define SORT_SUG       19
#define PROMPT_SUG     20
#define USER_SUG       21
#define WS_NUM_SUG     22 /* Workspace number */
#define WS_NAME_SUG    23 /* Workspace name */
#define FASTBACK_SUG   24
#define FUZZY_FILENAME 25
#define CMD_DESC_SUG   26
#define NET_SUG        27
#define CSCHEME_SUG    28
#define INT_HELP_SUG   29
#define PROFILE_SUG    30
#define BM_PREFIX_SUG  31 /* Bookmark name (b:NAME) */
#define DIRHIST_SUG    32
#define WS_PREFIX_SUG  33 /* Workspace name (w:NAME) */
#define WS_NUM_PREFIX_SUG 34

/* Macros to control file descriptors in exec functions. */
#define E_NOFLAG   0
#define E_NOSTDIN  (1 << 1)
#define E_NOSTDOUT (1 << 2)
#define E_NOSTDERR (1 << 3)
#define E_SETSID   (1 << 4)
#define E_MUTE     (E_NOSTDOUT | E_NOSTDERR)

/* Number of available suggestion strategies (a,b,c,e,f,h,j).
 * 'b' is deprecated (kept only for compatibility with old versions) */
#define SUG_STRATS 7

#define FZF_INTERNAL_PREVIEWER 1 /* clifm itself */
/* --preview is set either from FzfOpts in the color scheme file or from
 * FZF_DEFAULT_OPTS environment variable. */
#define FZF_EXTERNAL_PREVIEWER 3

/* Macros for the backdir (bd) function */
#define BD_TAB    1
#define BD_NO_TAB 0

/* Macros for the clear_suggestion function */
#define CS_FREEBUF 1
#define CS_KEEPBUF 0

/* Macros for the get_file_type function */
#define MIME_TYPE 1
#define TEXT_DESC 0

/* Macros for the dirjump function */
#define SUG_JUMP    0
#define NO_SUG_JUMP 1

/* Macros for the media_menu function */
#define MEDIA_LIST 	0
#define MEDIA_MOUNT	1

/* Macros for the rl_highlight function */
#define SET_COLOR    1
#define INFORM_COLOR 0

/* Are we truncating a filename with an extension? */
#define TRUNC_NO_EXT 1
#define TRUNC_EXT    2

/* OpenBSD recommends the use of 10 trailing X's. See mkstemp(3). */
#if defined(__OpenBSD__)
# define TMP_FILENAME ".tempXXXXXXXXXX"
#else
# define TMP_FILENAME ".tempXXXXXX"
#endif /* __OpenBSD__ */

#ifndef P_tmpdir
# define P_tmpdir "/tmp"
#endif /* P_tmpdir */

/* Length of random suffix appended to temp files. Used by gen_rand_str(). */
#define RAND_SUFFIX_LEN 10

/* Macros for the get_sys_shell function. */
#define SHELL_NONE 0
#define SHELL_BASH 1
#define SHELL_DASH 2
#define SHELL_FISH 3
#define SHELL_KSH  4
#define SHELL_TCSH 5
#define SHELL_ZSH  6
#define SHELL_POSIX SHELL_DASH

#define BELL_NONE    0
#define BELL_AUDIBLE 1
#define BELL_VISIBLE 2
#define BELL_FLASH   3

#define SECURE_ENV_FULL   1
#define SECURE_ENV_IMPORT 0

/* Macros for the sanitization function. */
/* Commands send to the system shell and taken from an untrusted source,
 * mostly config files, need to be sanitized first. */
#define SNT_MIME      0
#define SNT_PROMPT    1
#define SNT_PROFILE   2
#define SNT_AUTOCMD   3
#define SNT_NET       4
#define SNT_GRAL      5
#define SNT_DISPLAY   6 /* Sanitize DISPLAY environment variable */
#define SNT_MISC      7 /* Used to sanitize a few environment variables */
#define SNT_NONE      8 /* Trusted command: do not sanitize */
#define SNT_BLACKLIST 9

/* Macros for the TYPE field of the filter_t struct. */
#define FILTER_NONE      0
#define FILTER_FILE_NAME 1 /* Regex */
#define FILTER_FILE_TYPE 2 /* =x */
#define FILTER_MIME_TYPE 3 /* @query */

/* Macros for properties string fields in long view. */
#define PROP_FIELDS_SIZE 10 /* Ten available fields */

#define PERM_SYMBOLIC 1
#define PERM_NUMERIC  2

#define PROP_ID_NUM  1
#if defined(__ANDROID__)
# define PROP_ID_NAME PROP_ID_NUM
#else
# define PROP_ID_NAME 2
#endif /* __ANDROID__ */

#define PROP_TIME_ACCESS 1
#define PROP_TIME_MOD    2
#define PROP_TIME_CHANGE 3
#define PROP_TIME_BIRTH  4

#define PROP_SIZE_BYTES  1
#define PROP_SIZE_HUMAN  2

/* Macros for fzf_preview_border_type. */
#define FZF_BORDER_BOLD      0
#define FZF_BORDER_BOTTOM    1
#define FZF_BORDER_DOUBLE    2
#define FZF_BORDER_HORIZ     3
#define FZF_BORDER_LEFT      4
#define FZF_BORDER_NONE      5
#define FZF_BORDER_ROUNDED   6
#define FZF_BORDER_SHARP     7
#define FZF_BORDER_TOP       8
#define FZF_BORDER_VERT      9
#define FZF_BORDER_BLOCK     10
#define FZF_BORDER_THINBLOCK 11
#define FZF_BORDER_RIGHT     12

/* Flags to skip fuzzy matching based on what we're comparing. */
#define FUZZY_FILES_ASCII 0
#define FUZZY_FILES_UTF8  1
/*#define FUZZY_BM_NAMES    2 */
#define FUZZY_HISTORY     3
#define FUZZY_ALGO_MAX    2 /* We have two fuzzy algorithms */

#define JUMP_ENTRY_PURGED        (-1)
#define JUMP_ENTRY_PERMANENT     2
#define JUMP_ENTRY_PERMANENT_CHR '+'

#define MAX_TIME_STR 256

#define SHADE_TYPE_UNSET     0
#define SHADE_TYPE_8COLORS   1
#define SHADE_TYPE_256COLORS 2
#define SHADE_TYPE_TRUECOLOR 3
#define NUM_SHADES 6

#define QUOTING_STYLE_BACKSLASH     0
#define QUOTING_STYLE_SINGLE_QUOTES 1
#define QUOTING_STYLE_DOUBLE_QUOTES 2

/* Macros used for the --stat and --stat-full command line options. */
#define SIMPLE_STAT 1
#define FULL_STAT   2

/* Length to be added to a filename length if icons are enabled. */
#define ICON_LEN (2 + conf.icons_gap) /* Fixed icon width + gap */

/* Macros for alternative prompts (alt_prompt) */
#define FILES_PROMPT       1 /* Allow filenames completion */
#define PERMISSIONS_PROMPT 2 /* pc command */
#define OWNERSHIP_PROMPT   3 /* oc command */
#define BOOKMARKS_PROMPT   4 /* bookmarks screen */

#define AUTOCMD_MSG_NONE   0
#define AUTOCMD_MSG_MINI   1
#define AUTOCMD_MSG_SHORT  2
#define AUTOCMD_MSG_LONG   3
#define AUTOCMD_MSG_FULL   4
#define AUTOCMD_MSG_PROMPT 5

/* Function macros */
#define atoi xatoi /* xatoi is just a secure atoi */

#ifndef _NO_GETTEXT
# define _(str) gettext(str)
#else
# define _(str) str
#endif /* !_GETTEXT */

/* Log the message and print it to STDERR, but do not store it into the
 * messages array. */
#define xerror(...) err(ERR_NO_STORE, NOPRINT_PROMPT, __VA_ARGS__)

/* Macros to calculate file sizes */
/* Directories, regular files, and symbolic links report meaningful sizes
 * (when the st_size or st_blocks field of the corresponding stat struct is
 * queried). Other file types, like sockets, block/character devices, etc,
 * should always report a size of zero. However, this is not guarranteed:
 * in Solaris, for example, the value of the st_size field for block devices
 * is unspecified (it might be a big number).
 * See https://docs.oracle.com/cd/E36784_01/html/E36872/stat-2.html
 *
 * Let's use this macro to make sure we report file sizes only for the
 * appropriate file types.
 * NOTE: In case of directories themselves (excluding their content), their
 * apparent size is always zero as well (see 'info du'). */
#define FILE_TYPE_NON_ZERO_SIZE(m) (S_ISDIR((m)) || S_ISREG((m)) \
	|| S_ISLNK((m)))
/* du(1) also checks for
 * S_TYPEISSHM(struct stat *) // shared memory objects
 * S_TYPEISTMO(struct stat *) // typed memory objects */

#define FILE_SIZE_PTR(s) (conf.apparent_size == 1 ? (s)->st_size \
	: (s)->st_blocks * S_BLKSIZE)
#define FILE_SIZE(s) (conf.apparent_size == 1 ? (s).st_size \
	: (s).st_blocks * S_BLKSIZE)

/* Do we have the sort method S in light mode? */
#define ST_IN_LIGHT_MODE(s) ((s) == SNAME || (s) == SVER || (s) == SINO \
	|| (s) == SEXT || (s) == SNONE || (s) == STYPE)

#define UNUSED(x) (void)(x) /* Just silence the compiler's warning */

/* A yottabyte takes 26 digits, 28 if we count the negative sign and the NULL
 * terminator, so that 32 bytes is more than enough to represent any given
 * integer as a string. */
#define MAX_INT_STR 32
/* Bytes required to transform an integer of type TYPE into a string,
 * including the ending NULL char and the negative sign. */
/* (CHAR_BIT is defined in limits.h). */
/*#define INT_STR_SIZE(type) (((CHAR_BIT * sizeof(type) - 1) * 10 / 33) + 3) */
/* INT_STR_SIZE() is much more accurate than MAX_INT_STR (which is just a
 * big number) but not fully tested. */

/* UINT_MAX is 4294967295 == 10 digits */
#define DIGINUM(n) (((n) < 10) ? 1 \
	: ((n) < 100)        ? 2   \
	: ((n) < 1000)       ? 3   \
	: ((n) < 10000)      ? 4   \
	: ((n) < 100000)     ? 5   \
	: ((n) < 1000000)    ? 6   \
	: ((n) < 10000000)   ? 7   \
	: ((n) < 100000000)  ? 8   \
	: ((n) < 1000000000) ? 9   \
				             : 10)

/* SIZE_MAX == 18446744073709551615 == 20 digits */
#define DIGINUM_BIG(n) (((n) < 10) ? 1 \
	: ((n) < 100)        ? 2   \
	: ((n) < 1000)       ? 3   \
	: ((n) < 10000)      ? 4   \
	: ((n) < 100000)     ? 5   \
	: ((n) < 1000000)    ? 6   \
	: ((n) < 10000000)   ? 7   \
	: ((n) < 100000000)  ? 8   \
	: ((n) < 1000000000) ? 9   \
	: ((off_t)(n) < 10000000000) ? 10 \
	: ((off_t)(n) < 100000000000) ? 11 \
	: ((off_t)(n) < 1000000000000) ? 12 \
	: ((off_t)(n) < 10000000000000) ? 13 \
	: ((off_t)(n) < 100000000000000) ? 14 \
	: ((off_t)(n) < 1000000000000000) ? 15 \
	: ((off_t)(n) < 10000000000000000) ? 16 \
	: ((off_t)(n) < 100000000000000000) ? 17 \
	: ((off_t)(n) < 1000000000000000000) ? 18 \
                                             : 19)

#define IS_DIGIT(c)     ((unsigned int)(c) >= '0' && (unsigned int)(c) <= '9')
#define IS_ALPHA_LOW(c) ((unsigned int)(c) >= 'a' && (unsigned int)(c) <= 'z')
#define IS_ALPHA_UP(c)  ((unsigned int)(c) >= 'A' && (unsigned int)(c) <= 'Z')
#define IS_ALNUM(c)     (IS_ALPHA_LOW((c)) || IS_ALPHA_UP((c)) || IS_DIGIT((c)))
#define IS_HEX_DIGIT(c) (IS_DIGIT((c)) || ((c) >= 'a' && (c) <= 'f') \
	|| ((c) >= 'A' && (c) <= 'F'))

#define TOUPPER(c) (IS_ALPHA_LOW((c)) ? ((c) & ~0x20) : (c))
#define TOLOWER(c) (IS_ALPHA_UP((c)) ? ((c) | 0x20) : (c))

#define IS_UTF8_LEAD_BYTE(c) (((c) & 0xc0) == 0xc0)
#define IS_UTF8_CONT_BYTE(c) (((c) & 0xc0) == 0x80)
#define IS_UTF8_CHAR(c)      (IS_UTF8_LEAD_BYTE((c)) || IS_UTF8_CONT_BYTE((c)))

#define IS_COMMENT(c)  ((unsigned int)(c) == '#' || (unsigned int)(c) == ';')
#define IS_CTRL_CHR(c) ((unsigned int)(c) < ' ')
#define IS_NEWLINE(c)  ((unsigned int)(c) == '\n')
#define SKIP_LINE(c)   (IS_COMMENT((c)) || IS_NEWLINE((c)) || IS_CTRL_CHR((c)))

#define SELFORPARENT(s) (*(s) == '.' && (!(s)[1] || ((s)[1] == '.' && !(s)[2])))

#define FILE_URI_PREFIX_LEN 7
#define IS_FILE_URI(file, len) ((len) > FILE_URI_PREFIX_LEN     \
	&& (file)[4] == ':' && (file)[FILE_URI_PREFIX_LEN]          \
	&& strncmp((file), "file://", FILE_URI_PREFIX_LEN) == 0)

#define IS_HELP(s) (*(s) == '-' && (((s)[1] == 'h' && !(s)[2]) \
	|| strcmp((s), "--help") == 0))

/* Get the maximum value for a given data type.
 * Taken from coreutils (https://github.com/coreutils/gnulib/blob/master/lib/intprops.h) */
/*
#include <limits.h> // CHAR_BIT
#define TYPE_SIGNED(t)  (!((t)0 < (t)-1))
#define TYPE_WIDTH(t)   (sizeof(t) * CHAR_BIT)
#define TYPE_MAXIMUM(t)                                   \
        ((t)(!TYPE_SIGNED(t)                              \
        ? (t)-1                                           \
        : ((((t)1 << (TYPE_WIDTH(t) - 2)) - 1) * 2 + 1)))

// Safely cast the number N to the type T. The type T is never overflowed:
// if N is bigger than what T can hold, the maximum value T can hold is
// returned. What about underflow?
#define SAFECAST(n, t) ((t)(n) < 0 ? TYPE_MAXIMUM(t) : (t)(n)) */

/*
// See https://bestasciitable.com
#define __CTRL(n) ((n) & ~(1 << 6)) // Just unset (set to zero) the seventh bit
#define ___CTRL(n) ((n) & 0x3f) // Same as __CTRL
#define _SHIFT(n) ((n) & ~(1 << 5)) // Unset the sixth bit

// As defined by readline. Explanation:
// 0x1ff == 0011111
// So, this bitwise AND operation: 'n & 0x1f', means: set to zero whatever
// bits in N that are zero in 0x1f, that is, the sixth and seventh bits
#define _CTRL(n)  ((n) & 0x1f)
// As defined by readline: set to one the eight bit in N
#define _META(n)  ((n) | 0x80)
// E.g.: 'A' == 01000001
//     ('A' | 0x80) == 11000001 == 193 == Á
 */

				/** #########################
				 *  #    GLOBAL VARIABLES   #
				 *  ######################### */

/* filesn_t: Let's use this to count files */
/* ssize_t is a quite convenient data type:
 * 1. Unlike intmax_t, it's never bigger than size_t (in 32-bit/ARM machines,
 * INTMAX_MAX is bigger than SIZE_MAX).
 * 2. Can be safely casted to both intmax_t and size_t (something
 * we do sometimes).
 * 3. It's signed; at least one negative number (-1) (something we need for
 * decrementing loops).
 *
 * 32-bit/ARM: INT_MAX == LONG_MAX == SSIZE_MAX < SIZE_MAX < INTMAX_MAX == LLONG_MAX
 * 64-bit:     INT_MAX < LONG_MAX == LLONG_MAX == INTMAX_MAX == SSIZE_MAX < SIZE_MAX */
#define FILESN_MAX SSIZE_MAX
typedef ssize_t filesn_t;
extern filesn_t files;

struct default_answer_t {
	char remove;
	char trash;
	char bulk_rename;
	char overwrite;
	char default_;
	char default_all;
	char pad[2];
};

/* User settings (mostly from the config file) */
struct config_t {
	char *opener;
	char *dirhistignore_regex;
	char *histignore_regex;
	char *encoded_prompt;
	char *term;
	char *time_str;
	char *priority_sort_char;
	char *ptime_str;
	char *rprompt_str;
	char *welcome_message_str;
	char *wprompt_str;
#ifndef _NO_SUGGESTIONS
	char *suggestion_strategy;
#else
	char *pad0; /* Keep the struct alignment */
#endif /* !_NO_SUGGESTIONS */
	char *usr_cscheme;
	char *fzftab_options;

	struct default_answer_t default_answer;
	int apparent_size;
	int auto_open;
	int autocd;
	int autocmd_msg;
	int autols;
	int bell_style;
	int case_sens_dirjump;
	int case_sens_path_comp;
	int case_sens_search;
	int case_sens_list; /* File list */
	int cd_on_quit;
	int check_cap;
	int check_ext;
	int classify;
	int clear_screen;
	int cmd_desc_sug;
	int colorize;
	int color_lnk_as_target;
	int columned;
	int cp_cmd;
	int desktop_notifications;
	int dirhist_map;
	int disk_usage;
	int ext_cmd_ok;
	int files_counter;
	int follow_symlinks;
	int follow_symlinks_long;
	int full_dir_size;
	int fuzzy_match;
	int fuzzy_match_algo;
	int fzf_preview;
	int highlight;
	int icons;
	int icons_gap;
	int int_vars;
	int light_mode;
	int link_creat_mode;
	int list_dirs_first;
	int listing_mode;
	int log_cmds;
	int log_msgs;
	int long_view;
	int max_dirhist;
	int max_hist;
	int max_jump_total_rank;
	int max_files;
	int max_log;
	int max_name_len;
	int max_name_len_bk;
	int max_printselfiles;
	int min_jump_rank;
	int min_name_trunc;
	int mv_cmd;
	int no_eln;
	int only_dirs;
	int pager;
	int pager_once;
	int pager_view;
	int purge_jumpdb;
	int preview_max_size;
	int print_dir_cmds;
	int print_selfiles;
	int private_ws_settings;
	int prompt_b_is_set;
	int prompt_b_min;
	int prompt_b_precision;
	int prompt_f_dir_len;
	int prompt_f_full_len_dirs;
	int prompt_p_max_path;
	int prompt_is_multiline;
	int prop_fields_gap;
	int quoting_style;
	int read_autocmd_files;
	int read_dothidden;
	int readonly;
	int relative_time;
	int restore_last_path;
	int rm_force;
	int search_strategy;
	int share_selbox;
	int show_hidden;
	int skip_non_alnum_prefix;
	int sort;
	int sort_reverse;
	int splash_screen;
	int suggest_filetype_color;
	int suggestions;
	int time_follows_sort;
	int timestamp_mark;
	int tips;
	int trunc_names;
#ifndef _NO_TRASH
	int tr_as_rm;
	int trash_force;
#else
	int pad1; /* Keep the struct alignment */
	int pad2;
#endif /* !_NO_TRASH */
	int warning_prompt;
	int welcome_message;
	int pad3;
};

extern struct config_t conf;

/* Store information about the current files filter */
struct filter_t {
	char *str;
	int rev;
	int type;
	int env;
	int pad0;
};

extern struct filter_t filter;

/* Struct to store information about the current user. */
struct user_t {
	char *home;
	char *name;
	char *shell;
	char *shell_basename;
	size_t home_len;
	uid_t uid;
	gid_t gid;     /* Primary user group */
	gid_t *groups; /* Secondary groups ID's */
	int ngroups;   /* Number of secondary groups */
	int pad0;
};

extern struct user_t user;

/* Struct to store user defined variables */
struct usrvar_t {
	char *name;
	char *value;
};

extern struct usrvar_t *usr_var;

/* Struct to store user defined actions */
struct actions_t {
	char *name;
	char *value;
};

extern struct actions_t *usr_actions;

/* Workspaces information */
struct ws_t {
	char *path;
	char *name;
	int num;
	int pad0;
};

extern struct ws_t *workspaces;

/* Struct to store user defined keybindings */
struct kbinds_t {
	char *function;
	char *key;
};

extern struct kbinds_t *kbinds;

/* Struct to store the dirjump database values */
struct jump_t {
	char *path;
#if defined(__arm__) && !defined(__ANDROID__)
	char *pad0;
#endif /* __arm__ && !__ANDROID__ */
	size_t len;
	size_t visits;
	time_t first_visit;
	time_t last_visit;
	int keep;
	int rank;
};

extern struct jump_t *jump_db;

/* Struct to store bookmarks */
struct bookmarks_t {
	char *shortcut;
	char *name;
	char *path;
};

extern struct bookmarks_t *bookmarks;

struct alias_t {
	char *name;
	char *cmd;
};

extern struct alias_t *aliases;

struct groups_t {
	char *name;
	size_t namlen;
	gid_t id;
	int pad0;
};

extern struct groups_t *sys_users;
extern struct groups_t *sys_groups;

struct human_size_t {
	char   str[MAX_HUMAN_SIZE + 6];
	size_t len;
	int    unit;
	int    pad0;
};

/* Struct to store files information */
struct fileinfo {
	struct human_size_t human_size;
	struct groups_t uid_i;
	struct groups_t gid_i;
	char *color;
	char *ext_color;
	char *ext_name;
	char *icon;
	char *icon_color;
	char *name;
	filesn_t filesn;
	blkcnt_t blocks;
	size_t len;    /* Filename len (columns needed to display filename) */
	size_t bytes;  /* Bytes consumed by filename */
#ifdef TIGHT_COLUMNS
	size_t total_entry_len;
#endif
	time_t ltime;  /* For long view mode */
	time_t time;
	ino_t inode;
	off_t size;
	nlink_t linkn; /* 4 bytes on Solaris/BSD/HAIKU; 8 on Linux */
	uid_t uid;
	gid_t gid;
	mode_t mode;   /* Store st_mode (for long view mode) */
	mode_t type;   /* Store d_type value */
	int dir;
	int eln_n;     /* Number of digits in ELN */
	int exec;
	int user_access; /* Read-exec for dirs and read for files */
	int symlink;
	int sel;
	int xattr;
	int du_status; /* Exit status of du(1) for dir full sizes */
	int utf8;      /* Name contains at least one UTF-8 character */
	int stat_err;  /* stat(2) failed for this entry */
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) \
|| defined(__APPLE__) || defined(__sun) || defined(__HAIKU__) \
|| (defined(__arm__) && !defined(__ANDROID__))
	int pad0;
#endif
};

extern struct fileinfo *file_info;

/* Struct to store the length of the longest values in the current list of
 * files (needed to properly construct columns in long view). */
struct maxes_t {
	int id_group;
	int id_user;
	int inode;
	int files_counter;
	int links;
	int name;
	int size;
	int blocks;
};

struct devino_t {
	ino_t ino;
	dev_t dev; /* 4 bytes on OpenBSD, DragonFly, and Haiku */
	char mark;
	char pad0[3];
#if !defined(__OpenBSD__) && !defined(__DragonFly__) && !defined(__HAIKU__)
	int pad1;
#endif /* !__OpenBSD__ && !__DragonFly__ && !__HAIKU__ */
};

extern struct devino_t *sel_devino;

struct autocmds_t {
	struct filter_t filter;
	char *pattern;
	char *color_scheme;
	char *cmd;
	int files_counter;
	int full_dir_size;
	int long_view;
	int light_mode;
	int match;
	int max_files;
	int max_name_len;
	int only_dirs;
	int pager;
	int pattern_rev;
	int show_hidden;
	int sort;
	int sort_reverse;
	int temp;
};

extern struct autocmds_t *autocmds;

struct opts_t {
	struct filter_t filter;
	char *color_scheme;
	int files_counter;
	int full_dir_size;
	int light_mode;
	int list_dirs_first;
	int long_view;
	int max_files;
	int max_name_len;
	int only_dirs;
	int pager;
	int show_hidden;
	int sort;
	int sort_reverse;
};

extern struct opts_t opts;
extern struct opts_t workspace_opts[MAX_WS];

/* Struct to specify which parameters have been set from the command
 * line, to avoid overriding them with init_config(). While no command
 * line parameter will be overriden, the user still can modifiy on the
 * fly (editing the config file) any option not specified in the command
 * line. */
struct param_t {
	int apparent_size;
	int auto_open;
	int autocd;
	int autols;
	int bell_style;
	int bm_file;
	int case_sens_dirjump;
	int case_sens_path_comp;
	int case_sens_list;
	int clear_screen;
	int color_lnk_as_target;
	int colorize;
	int columned;
	int config;
	int cwd_in_title;
	int desktop_notifications;
	int dirhist_map;
	int disk_usage;
	int cd_on_quit;
	int check_cap;
	int check_ext;
	int classify;
	int color_scheme;
	int disk_usage_analyzer;
	int eln_use_workspace_color;
	int ext_cmd_ok;
	int files_counter;
	int follow_symlinks;
	int follow_symlinks_long;
	int full_dir_size;
	int fuzzy_match;
	int fuzzy_match_algo;
	int fzf_preview;
#ifndef _NO_FZF
	int fzftab;
	int fnftab;
	int smenutab;
#endif /* !_NO_FZF */
#ifndef _NO_HIGHLIGHT
	int highlight;
#endif /* !_NO_HIGHLIGHT */
	int history;
	int horizontal_list;
#ifndef _NO_ICONS
	int icons;
#endif /* !_NO_ICONS */
	int icons_use_file_color;
	int int_vars;
	int kitty_keys; /* kitty keyboard protocol */
	int list_and_quit;
	int light_mode;
	int list_dirs_first;
	int long_view;
	int lscolors;
	int max_dirhist;
	int max_files;
	int mount_cmd;
	int no_bold;
	int no_dirjump;
	int no_eln;
	int only_dirs;
	int open;
	int pager;
	int pager_view;
	int path;
	int preview;
	int print_selfiles;
	int prompt_p_max_path;
	int prop_fields_str;
	int ptime_style;
	int readonly;
	int refresh_on_empty_line;
	int refresh_on_resize;
	int report_cwd;
	int restore_last_path;
	int rl_vi_mode;
	int secure_cmds;
	int secure_env;
	int secure_env_full;
	int sel_file;
	int share_selbox;
	int show_hidden;
	int si; /* Sizes in powers of 1000 instead of 1024 */
	int sort;
	int sort_reverse;
	int splash_screen;
	int stat;
	int stealth_mode;
#ifndef _NO_SUGGESTIONS
	int suggestions;
#endif /* !_NO_SUGGESTIONS */
	int time_style;
	int tips;
#ifndef _NO_TRASH
	int trasrm;
#endif /* !_NO_TRASH */
	int trunc_names;
	int unicode;
	int virtual_dir_full_paths;
	int vt100;
	int welcome_message;
	int warning_prompt;
};

extern struct param_t xargs;

/* Struct to store remotes information */
struct remote_t {
	char *desc;
	char *name;
	char *mount_cmd;
	char *mountpoint;
	char *unmount_cmd;
	int auto_mount;
	int auto_unmount;
	int mounted;
	int pad0;
};

extern struct remote_t *remotes;

/* Store information about the current suggestion */
struct suggestions_t {
	char *color;
	size_t full_line_len;
	size_t nlines;
	int filetype;
	int printed;
	int type;
	int offset;
};

extern struct suggestions_t suggestion;

/* Hold information about selected files */
struct sel_t {
	char *name;
#ifdef __arm__
	char *pad0;
#endif /* __arm__ */
	off_t size;
};

extern struct sel_t *sel_elements;

/* File statistics for the current directory. Used by the 'stats' command */
struct stats_t {
	size_t dir;
	size_t empty_dir;
	size_t reg;
	size_t empty_reg;
	size_t exec;
	size_t hidden;
	size_t suid;
	size_t sgid;
	size_t fifo;
	size_t socket;
	size_t block_dev;
	size_t char_dev;
	size_t caps;
	size_t link;
	size_t broken_link;
	size_t multi_link;
	size_t other_writable;
	size_t sticky;
	size_t extended;
	size_t unknown;
	size_t unstat; /* Non-statable file */
	size_t excluded; /* Files not displayed */
#ifdef __sun
	size_t door;
	size_t port;
#endif /* __sun */
#ifdef S_ARCH1
	size_t arch1;
	size_t arch2;
#endif /* S_ARCH1 */
#ifdef S_IFWHT
	size_t whiteout;
#endif /* S_IFWHT */
};

extern struct stats_t stats;

struct sort_t {
	const char *name;
	int num;
	int pad0;
};

extern const struct sort_t sort_methods[];

/* Prompts and prompt settings */
struct prompts_t {
	char *name;
	char *regular;
	char *warning;
	char *right;
	int notifications;
	int warning_prompt_enabled;
	int multiline; /* Regular prompt is multiline */
	int pad0;
};

extern struct prompts_t *prompts;

/* System messages */
struct msgs_t {
	size_t error;
	size_t warning;
	size_t notice;
};
extern struct msgs_t msgs;

/* A few termcap values to know whether we can use some terminal features */
struct termcaps_t {
	int color;
	int suggestions;
	int pager;
	int hide_cursor;
	int home;  /* Move cursor to line 1, column 1 */
	int clear; /* ED (erase display) */
	int del_scrollback; /* E3 */
	int req_cur_pos; /* CPR (cursor position request) */
	int req_dev_attrs; /* Primary DA (device attributes request) */
	int unicode;
};
extern struct termcaps_t term_caps;

/* Data to be displayed in the properties string in long mode */
struct props_t {
	int counter; /* File counter */
	int ids; /* User/group IDs: either NUMBER or NAME */
	int inode; /* File inode number */
	int len; /* Approx len of the entire properties string taking into account
			  * all fields and their length. */
	int links; /* File links */
	int blocks;
	int no_group; /* Should we display group if IDS is set? */
	int perm; /* File permissions: either NUMERIC or SYMBOLIC */
	int size; /* File size: either HUMAN or BYTES */
	int time; /* Time: either ACCESS, MOD, CHANGE, or BIRTH */
	int xattr; /* Extended attributes */
	int pad0;
};
extern struct props_t prop_fields;

#define ALL_CMDS        (1 << 0) // Check all commands (ignore parameters)
#define NO_PARAM        (1 << 1) // Command takes no parameter
#define PARAM_STR       (1 << 2) // Command takes a string (not filename)
#define PARAM_FNAME     (1 << 3) // Command takes filenames
#define PARAM_NUM       (1 << 4) // Command takes numbers
#define NO_FNAME_NUM    (NO_PARAM | PARAM_STR) // Neither filename nor number
#define PARAM_FNAME_NUM (PARAM_FNAME | PARAM_NUM) // Either filename or number

struct cmdslist_t {
	char *name;
	size_t len;
	int flag;
	int pad;
};

extern const struct cmdslist_t internal_cmds[];

struct nameslist_t {
	char *name;
	size_t len;
};

extern const struct nameslist_t param_str[];
extern const struct nameslist_t kb_cmds[];

extern size_t internal_cmds_n;

struct history_t {
	char *cmd;
	size_t len;
	time_t date;
};
extern struct history_t *history;

/* Structs to hold color info for size and date fields in file properties */
struct rgb_t {
	uint8_t attr;
	uint8_t R;
	uint8_t G;
	uint8_t B;
};

struct shades_t {
	uint8_t type;
	struct rgb_t shades[NUM_SHADES];
};
extern struct shades_t date_shades;
extern struct shades_t size_shades;

struct paths_t {
	char *path;
#ifdef __arm__
	char *pad0;
#endif /* __arm__ */
	time_t mtime;
};
extern struct paths_t *paths;

struct ext_t {
	char  *name;
	char  *value;
	size_t len; /* Name length */
	size_t value_len;
	size_t hash;
};
extern struct ext_t *ext_colors;

#ifdef LINUX_FSINFO
#define EXT2_FSTYPE 0x002
#define EXT3_FSTYPE 0x003
#define EXT4_FSTYPE 0x004
struct ext_mnt_t {
	char *mnt_point;
	int  type; /* One of EXTN_FSTYPE macros */
	int  pad0;
};

extern struct ext_mnt_t *ext_mnt;
#endif /* LINUX_FSINFO */

/* State info for the PrintDirCmds function. */
struct dircmds_t {
	int first_cmd_in_dir; /* History index of first cmd exec'ed in the cur dir */
	int last_cmd_ignored; /* Ignored cmd (via HistIgnore) */
};
extern struct dircmds_t dir_cmds;

struct pmsgs_t {
	char *text;
	int read;
	int pad0;
};
extern struct pmsgs_t *messages;

struct dir_info_t {
	unsigned long long dirs;
	unsigned long long files;
	unsigned long long links;
	off_t size;
	blkcnt_t blocks;
	int status;
	int pad0;
};

/* Store user defined mimetypes */
struct mime_t {
	char *ext;
	char *mimetype;
	size_t ext_hash;
};
extern struct mime_t *user_mimetypes;

enum tab_mode {
	STD_TAB =   0,
	FZF_TAB =   1,
	FNF_TAB =   2,
	SMENU_TAB = 3
};

extern enum tab_mode tabmode;

/* A list of possible program messages. Each value tells the prompt what
 * to do with error messages: either to print an E, W, or N char at the
 * beginning of the prompt, or nothing (nomsg) */
enum prog_msg {
	NOMSG =   0,
	ERROR =   1,
	WARNING = 2,
	NOTICE =  4
};

/* pmsg holds the current program message type */
extern enum prog_msg pmsg;

/* Enumeration for the dirjump function options */
enum jump {
	NONE = 	  0,
	JPARENT = 1,
	JCHILD =  2,
	JORDER =  4,
	JLIST =	  8
};

enum comp_type {
	TCMP_BOOKMARK =	  0,
	TCMP_CMD =        1,
	TCMP_CSCHEME = 	  2,
	TCMP_DESEL =      3,
	TCMP_ELN =        4,
	TCMP_HIST =       5,
	TCMP_JUMP =       6,
	TCMP_NET =        7,
	TCMP_NONE =       8,
	TCMP_OPENWITH =   9,
	TCMP_PATH =       10,
	TCMP_PROF =       11,
	TCMP_RANGES =     12,
	TCMP_SEL =        13,
	TCMP_SORT =       14,
	TCMP_TRASHDEL =	  15,
	TCMP_UNTRASH =    16,
	TCMP_BACKDIR =    17,
	TCMP_ENVIRON =    18,
	TCMP_TAGS_T =     19, /* T keyword: 't:TAG' */
	TCMP_TAGS_C =     20, /* Colon: 'tag file :TAG' */
	TCMP_TAGS_S =     21, /* Simple completion: 'tag rm TAG' */
	TCMP_TAGS_F =     22, /* Tagged files completion: 't:FULL_TAG_NAME' */
	TCMP_TAGS_U =     23, /* Tagged files for the untag function */
	TCMP_ALIAS =      24,
	TCMP_PROMPTS =    25,
	TCMP_USERS =      26,
	TCMP_GLOB =       27,
	TCMP_FILE_TYPES_OPTS = 28,
	TCMP_FILE_TYPES_FILES = 29,
	TCMP_WORKSPACES = 30,
	TCMP_BM_PATHS =   31, /* 'b:FULLNAME' keyword expansion */
	TCMP_BM_PREFIX =  32, /* 'b:' keyword expansion */
	TCMP_CMD_DESC =   33,
	TCMP_OWNERSHIP =  34,
	TCMP_DIRHIST =    35,
	TCMP_MIME_LIST =  36,
	TCMP_WS_PREFIX =  37, /* 'w:' keyword expansion */
	TCMP_MIME_FILES = TCMP_FILE_TYPES_FILES, /* Same behavior */
	TCMP_FILE_TEMPLATES = 38
};

extern enum comp_type cur_comp_type;

/* Bit flag holders */
extern int
	flags,
	bin_flags,
	search_flags;

extern int
	date_shades_old_style,
	size_shades_old_style;

/* Internal state flags */
extern int
	alt_prompt, /* We're running a secondary prompt */
	argc_bk, /* A copy of argc taken from main() */
	autocmd_set,
	bg_proc,
	cmdhist_flag,
	config_ok,
	cur_ws,
	curcol,
	dequoted,
	dir_changed, /* flag to know if dir was changed: used by autocmds */
	dirhist_cur_index,
	dirhist_total_index,
	exit_code,
	fzftab,
	fzf_ext_border,
	fzf_border_type,
	fzf_height_value,
	fzf_preview_border_type,
	hist_status,
	home_ok,
	internal_cmd,
	is_sel,
	is_cdpath,
	jump_total_rank,
	kbind_busy,
	nesting_level, /* Is this a nested instance? */
	no_log,
	open_in_foreground, /* Overrides mimelist file value: used by mime_open */
	prev_ws,
	print_msg,
	print_removed_files,
	prompt_offset,
	prompt_notif,
	recur_perm_error_flag,
	rl_nohist,
	rl_notab,
	sel_is_last,
	selfile_ok,
	shell,
	shell_is_interactive,
	shell_terminal,
	sort_switch,
	switch_cscheme,
#ifndef _NO_TRASH
	trash_ok,
#endif /* !_NO_TRASH */
	virtual_dir,
	wrong_cmd;

extern unsigned short term_cols, term_lines;
extern double last_cmd_time;

extern size_t
	actions_n,
	aliases_n,
	args_n,
	autocmds_n,
	bm_n,
	cdpath_n,
	config_dir_len,
	cschemes_n,
	current_hist_n,
	curhistindex,
	ext_colors_n,
	jump_n,
	kbinds_n,
	msgs_n,
	P_tmpdir_len,
	path_n,
	path_progsn,
	prompt_cmds_n,
	prompts_n,
	remotes_n,
	sel_n,
	tab_offset,
	tags_n,
	trash_n,
	usrvar_n,
	words_num,
	zombies;

#ifndef _NO_ICONS
extern size_t *name_icons_hashes;
extern size_t *dir_icons_hashes;
extern size_t *ext_icons_hashes;
#endif /* !_NO_ICONS */

extern pid_t own_pid;
extern time_t props_now;

extern char
	cur_prompt_name[NAME_MAX + 1],
	div_line[NAME_MAX + 1],
	hostname[HOST_NAME_MAX + 1],
	fz_match[PATH_MAX + 1], /* First regular match if fuzzy matching is enabled */
	prop_fields_str[PROP_FIELDS_SIZE + 1],
	invalid_time_str[MAX_TIME_STR],

#ifdef RUN_CMD
	*cmd_line_cmd,
#endif /* RUN_CMD */

	*actions_file,
	*alt_config_dir,
	*alt_trash_dir,
	*alt_bm_file,
	*alt_config_file,
	*alt_kbinds_file,
	*alt_mimelist_file,
	*alt_preview_file,
	*alt_profile,
	*bm_file,
	*cmds_log_file,
	*colors_dir,
	*config_dir,
	*config_dir_gral,
	*config_file,
	*cur_color,
	*cur_tag,
	*data_dir,
	*cur_cscheme,
	*dirhist_file,
	*file_cmd_path,
	*hist_file,
	*kbinds_file,
	*jump_suggestion,
	*last_cmd,
	*mime_file,
	*msgs_log_file,
	*pinned_dir,
	*plugins_dir,
	*plugins_helper_file,
	*profile_file,
	*prompts_file,
	*quote_chars,
	*rl_callback_handler_input,
	*remotes_file,
	*sel_file,
	*smenutab_options_env,
	*stdin_tmp_dir,
	*sudo_cmd,
#ifndef _NO_SUGGESTIONS
	*suggestion_buf,
#endif /* !_NO_SUGGESTIONS */
	*tags_dir,
	*templates_dir,
	*thumbnails_dir,
	*tmp_rootdir,
	*tmp_dir,
#ifndef _NO_TRASH
	*trash_dir,
	*trash_files_dir,
	*trash_info_dir,
#endif /* !_NO_TRASH */


	**argv_bk,
	**bin_commands,
	**cdpaths,
	**color_schemes,
	**file_templates,
	**old_pwd,
	**profile_names,
	**prompt_cmds,
	**tags;

extern regex_t regex_exp;     /* Files list */
extern regex_t regex_hist;    /* Commands history */
extern regex_t regex_dirhist; /* Directory history */
extern char **environ;

/* A buffer to store filenames to be displayed (wide string) */
#define NAME_BUF_SIZE (NAME_MAX + 1)
extern char name_buf[NAME_BUF_SIZE * sizeof(wchar_t)];

/* Longest supported color:
 * \x1b[4:4;38;2;000;000;000;48;2;000;000;000;58;2;000;000;000m\0
 * This is a complete SGR sequence, with 24bit colors: foreground, background,
 * and underline (kitty). */
#define MAX_COLOR 64

/* For the almost 100 color variables we use, we need more or less 6Kb.
 * It's not much, but it could be less if we'd use dynamically allocated
 * arrays for them (which, on the other side, would make the whole thing
 * slower and more tedious). */

/* Colors */
extern char
	/* File types */
	bd_c[MAX_COLOR], /* Block device */
	bk_c[MAX_COLOR], /* Backup/temp files */
	ca_c[MAX_COLOR], /* Cap file */
	cd_c[MAX_COLOR], /* Char device */
	di_c[MAX_COLOR], /* Directory */
	ed_c[MAX_COLOR], /* Empty dir */
	ee_c[MAX_COLOR], /* Empty executable */
	ex_c[MAX_COLOR], /* Executable */
	ef_c[MAX_COLOR], /* Empty reg file */
	fi_c[MAX_COLOR], /* Reg file */
	ln_c[MAX_COLOR], /* Symlink */
	mh_c[MAX_COLOR], /* Multi-hardlink file */
	nd_c[MAX_COLOR], /* No read directory */
	nf_c[MAX_COLOR], /* No read file */
	no_c[MAX_COLOR], /* Unknown */
#ifdef __sun
	oo_c[MAX_COLOR], /* Solaris door/port */
#endif /* __sun */
	or_c[MAX_COLOR], /* Broken symlink */
	ow_c[MAX_COLOR], /* Other writable */
	pi_c[MAX_COLOR], /* FIFO, pipe */
	sg_c[MAX_COLOR], /* SGID file */
	so_c[MAX_COLOR], /* Socket */
	st_c[MAX_COLOR], /* Sticky (not ow)*/
	su_c[MAX_COLOR], /* SUID file */
	tw_c[MAX_COLOR], /* Sticky other writable */
	uf_c[MAX_COLOR], /* Non-'stat'able file */

	/* Interface */
	ac_c[MAX_COLOR + 2], /* Autocmd indicator */
	df_c[MAX_COLOR], /* Default color */
	dl_c[MAX_COLOR], /* Dividing line */
	el_c[MAX_COLOR], /* ELN color */
	fc_c[MAX_COLOR], /* File counter */
	lc_c[MAX_COLOR], /* Symlink character (ColorLinkAsTarget only) */
	mi_c[MAX_COLOR], /* Misc indicators */
	ts_c[MAX_COLOR], /* Tab completion suffix */
	tt_c[MAX_COLOR], /* Tilde for truncated filenames */
	wc_c[MAX_COLOR], /* Welcome message */
	wp_c[MAX_COLOR], /* Warning prompt */

	/* Suggestions */
	sb_c[MAX_COLOR], /* Auto-suggestions: shell builtins */
	sc_c[MAX_COLOR], /* Auto-suggestions: external commands and aliases */
	sd_c[MAX_COLOR], /* Auto-suggestions: internal commands description */
	sf_c[MAX_COLOR], /* Auto-suggestions: filenames */
	sh_c[MAX_COLOR], /* Auto-suggestions: history */
	sp_c[MAX_COLOR], /* Auto-suggestions: BAEJ suggestions pointer */
	sx_c[MAX_COLOR], /* Auto-suggestions: internal commands and parameters */
	sz_c[MAX_COLOR], /* Auto-suggestions: filenames (fuzzy) */

#ifndef _NO_ICONS
    dir_ico_c[MAX_COLOR], /* Directories icon color */
#endif /* !_NO_ICONS */

	/* Syntax highlighting */
	hb_c[MAX_COLOR], /* Brackets () [] {} */
	hc_c[MAX_COLOR], /* Comments */
	hd_c[MAX_COLOR], /* Paths (slashes) */
	he_c[MAX_COLOR], /* Expansion operators: * ~ */
	hn_c[MAX_COLOR], /* Numbers */
	hp_c[MAX_COLOR], /* Parameters: - */
	hq_c[MAX_COLOR], /* Quoted strings */
	hr_c[MAX_COLOR], /* Redirection > */
	hs_c[MAX_COLOR], /* Process separators | & ; */
	hv_c[MAX_COLOR], /* Variables $ */
	hw_c[MAX_COLOR], /* Backslash (aka whack) */

	/* File properties */
	db_c[MAX_COLOR],  /* File allocated blocks */
	dd_c[MAX_COLOR],  /* Date (fixed color: no shading) */
	de_c[MAX_COLOR],  /* Inode number */
	dg_c[MAX_COLOR],  /* Group ID */
	dk_c[MAX_COLOR],  /* Number of links */
	dn_c[MAX_COLOR],  /* dash (none) */
	do_c[MAX_COLOR],  /* Octal perms */
	dp_c[MAX_COLOR],  /* Special files (SUID, SGID, etc) */
	dr_c[MAX_COLOR],  /* Read */
	dt_c[MAX_COLOR],  /* Timestamp mark */
	du_c[MAX_COLOR],  /* User ID */
	dw_c[MAX_COLOR],  /* Write */
	dxd_c[MAX_COLOR], /* Execute (dirs) */
	dxr_c[MAX_COLOR], /* Execute (reg files) */
	dz_c[MAX_COLOR],  /* Size (dirs) */

    /* Colors used in the prompt, so that \001 and \002 needs to
	 * be added. This is why MAX_COLOR + 2 */
	/* Workspaces */
	ws1_c[MAX_COLOR + 2],
	ws2_c[MAX_COLOR + 2],
	ws3_c[MAX_COLOR + 2],
	ws4_c[MAX_COLOR + 2],
	ws5_c[MAX_COLOR + 2],
	ws6_c[MAX_COLOR + 2],
	ws7_c[MAX_COLOR + 2],
	ws8_c[MAX_COLOR + 2],

	em_c[MAX_COLOR + 2], /* Error msg */
	li_c[MAX_COLOR + 2], /* Sel indicator */
	li_cb[MAX_COLOR],    /* Sel indicator (for the file list) */
	nm_c[MAX_COLOR + 2], /* Notice msg */
	ti_c[MAX_COLOR + 2], /* Trash indicator */
	tx_c[MAX_COLOR + 2], /* Text */
	ro_c[MAX_COLOR + 2], /* Read-only indicator */
	si_c[MAX_COLOR + 2], /* Stealth indicator */
	wm_c[MAX_COLOR + 2], /* Warning msg */
	xs_c[MAX_COLOR + 2], /* Exit code: success */
	xs_cb[MAX_COLOR],    /* Exit code: success (Unicode success indicator) */
	xf_c[MAX_COLOR + 2], /* Exit code: failure */
	xf_cb[MAX_COLOR],    /* Exit code: failure (dir read) */

	tmp_color[MAX_COLOR + 2], /* A temp buffer to store color codes */
	dim_c[5]; /* Dimming code: not supported by all terminals */

#endif /* HELPERS_H */
