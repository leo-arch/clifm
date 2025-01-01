/* mem.c -- Some memory wrapper functions */

/*
 * This file is part of Clifm
 *
 * Copyright (C) 2016-2025, L. Abramovich <leo.clifm@outlook.com>
 * All rights reserved.

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

#include "helpers.h"

#include <errno.h> /* ENOMEM */
#include <stdlib.h> /* malloc(), calloc(), realloc() */

#if defined(__has_builtin) && !defined(NO_BUILTIN_MUL_OVERFLOW)
# if __has_builtin(__builtin_mul_overflow)
#  define HAVE_BUILTIN_MUL_OVERFLOW
# endif
#endif

void *
xnrealloc(void *ptr, const size_t nmemb, const size_t size)
{
#ifdef HAVE_BUILTIN_MUL_OVERFLOW
	/* This is what reallocarray(3) does: if nmemb*size overflows, err. */
	size_t r;
	if (__builtin_mul_overflow(nmemb, size, &r))
		r = SIZE_MAX;

	void *p = r == SIZE_MAX ? NULL : realloc(ptr, r);
#else
	void *p = realloc(ptr, nmemb * size);
#endif /* HAVE_BUILTIN_MUL_OVERFLOW */

	if (!p) {
		fprintf(stderr, _("%s: %s failed to allocate %zux%zu bytes\n"),
			PROGRAM_NAME, __func__, nmemb, size);
		exit(ENOMEM);
	}

	return p;
}

void *
xcalloc(const size_t nmemb, const size_t size)
{
	/* Most modern calloc implementations, at least Glibc and OpenBSD,
	 * use __builtin_mul_overflow internally, so that we don't need to
	 * do it here manually. */
	void *p = calloc(nmemb, size);

	if (!p) {
		fprintf(stderr, _("%s: %s failed to allocate %zux%zu bytes\n"),
			PROGRAM_NAME, __func__, nmemb, size);
		exit(ENOMEM);
	}

	return p;
}

void *
xnmalloc(const size_t nmemb, const size_t size)
{
#ifdef HAVE_BUILTIN_MUL_OVERFLOW
	size_t r;
	if (__builtin_mul_overflow(nmemb, size, &r))
		r = SIZE_MAX;

	void *p = r == SIZE_MAX ? NULL : malloc(r);
#else
	void *p = malloc(nmemb * size);
#endif /* HAVE_BUILTIN_MUL_OVERFLOW */

	if (!p) {
		fprintf(stderr, _("%s: %s failed to allocate %zux%zu bytes\n"),
			PROGRAM_NAME, __func__, nmemb, size);
		exit(ENOMEM);
	}

	return p;
}
