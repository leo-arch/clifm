/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* selset.c -- keep track of selected files */

#include "helpers.h"

#include "selset.h"
#include <stdlib.h>

static inline uint64_t
splitmix64(uint64_t x)
{
	x += 0x9e3779b97f4a7c15ULL;
	x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
	x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
	return x ^ (x >> 31);
}

static inline uint64_t
hash_devino(devino_t k)
{
	/* Use uintmax_t to safely handle platform-specific signedness/width. */
	uintmax_t d = (uintmax_t)k.dev;
	uintmax_t i = (uintmax_t)k.ino;

	uint64_t h1 = splitmix64((uint64_t)d);
	uint64_t h2 = splitmix64((uint64_t)i);

	/* Combine */
	return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
}

static inline int
devino_equal(devino_t a, devino_t b)
{
	return (a.dev == b.dev && a.ino == b.ino);
}

static size_t
next_pow2(size_t x)
{
	size_t p = 1;
	while (p < x) p <<= 1;
	return p;
}

int
devino_set_init(devino_set_t *s, size_t initial_cap)
{
	if (!s) return 0;
	if (initial_cap < 8) initial_cap = 8;

	s->cap = next_pow2(initial_cap);
	s->size = 0;
	s->state = (unsigned char *)calloc(s->cap, sizeof(unsigned char));
	s->keys  = (devino_t *)calloc(s->cap, sizeof(devino_t));
	if (!s->state || !s->keys) {
		free(s->state);
		free(s->keys);
		s->state = NULL;
		s->keys = NULL;
		s->cap = 0;
		return 0;
	}
	return 1;
}

void
devino_set_destroy(devino_set_t *s)
{
	if (!s) return;
	free(s->state);
	free(s->keys);
	s->state = NULL;
	s->keys = NULL;
	s->cap = 0;
	s->size = 0;
}

static inline size_t
idx_for(const devino_set_t *s, uint64_t h, size_t probe)
{
	/* linear probing; cap is power of two */
	return (size_t)((h + probe) & (s->cap - 1));
}

int
devino_set_contains(const devino_set_t *s, dev_t dev, ino_t ino)
{
	if (!s || !s->state || s->cap == 0) return 0;

	devino_t key = { .dev = dev, .ino = ino };
	uint64_t h = hash_devino(key);

	/* Probe until we hit an empty slot. */
	for (size_t probe = 0; probe < s->cap; probe++) {
		size_t idx = idx_for(s, h, probe);
		unsigned char st = s->state[idx];

		if (st == 0) return 0;           /* Empty => not present */
		if (st == 1 && devino_equal(s->keys[idx], key)) return 1;
		/* Otherwise, continue probing */
	}
	return 0;
}

int
devino_set_insert(devino_set_t *s, dev_t dev, ino_t ino)
{
	if (!s || !s->state || !s->keys) return 0;

	devino_t key = { .dev = dev, .ino = ino };
	uint64_t h = hash_devino(key);

	size_t first_empty = (size_t)-1;

	for (size_t probe = 0; probe < s->cap; probe++) {
		size_t idx = idx_for(s, h, probe);
		unsigned char st = s->state[idx];

		if (st == 0) { /* Empty */
			first_empty = idx;
			break;
		}
		if (st == 1 && devino_equal(s->keys[idx], key))
			return 0; /* Already present */

		/* If you later add tombstones, you'd handle DELETED here. */
	}

	if (first_empty == (size_t)-1) {
		/* Table full (shouldn't happen if you size it reasonably). */
		return 0;
	}

	s->state[first_empty] = 1;
	s->keys[first_empty] = key;
	s->size++;
	return 1;
}
