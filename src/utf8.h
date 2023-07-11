/* utf8.h - Unicode aware string functions */

/* This library is a trimmed down version of https://github.com/sheredom/utf8.h,
 * released into the public domain.
 * Modified code is licensed GPL2+. */

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

#ifndef SHEREDOM_UTF8_H_INCLUDED
#define SHEREDOM_UTF8_H_INCLUDED

#if defined(_MSC_VER)
#pragma warning(push)

/* disable warning: no function prototype given: converting '()' to '(void)' */
#pragma warning(disable : 4255)

/* disable warning: '__cplusplus' is not defined as a preprocessor macro,
 * replacing with '0' for '#if/#elif' */
#pragma warning(disable : 4668)

/* disable warning: bytes padding added after construct */
#pragma warning(disable : 4820)
#endif

//#include <stddef.h>
//#include <stdlib.h>

#if defined(_MSC_VER)
# pragma warning(pop)
#endif

#if defined(_MSC_VER) && (_MSC_VER < 1920)
typedef __int32 utf8_int32_t;
#else
#include <stdint.h>
typedef int32_t utf8_int32_t;
#endif

#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wold-style-cast"
# pragma clang diagnostic ignored "-Wcast-qual"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_MSC_VER)
# define utf8_nonnull
# define utf8_pure
# define utf8_restrict __restrict
# define utf8_weak __inline
#elif defined(__clang__) || defined(__GNUC__) || defined(__TINYC__)
# define utf8_nonnull __attribute__((nonnull))
# define utf8_pure __attribute__((pure))
# define utf8_restrict __restrict__
# define utf8_weak __attribute__((weak))
#else
# error Non clang, non gcc, non MSVC compiler found!
#endif

#ifdef __cplusplus
# define utf8_null NULL
#else
# define utf8_null 0
#endif

#if (defined(__cplusplus) && __cplusplus >= 201402L)
# define utf8_constexpr14 constexpr
# define utf8_constexpr14_impl constexpr
#else
/* constexpr and weak are incompatible. so only enable one of them */
# define utf8_constexpr14 utf8_weak
# define utf8_constexpr14_impl
#endif

#if defined(__cplusplus) && __cplusplus >= 202002L
using utf8_int8_t = char8_t; /* Introduced in C++20 */
#else
typedef char utf8_int8_t;
#endif

/* Find the first match of the utf8 codepoint chr in the utf8 string src. */
utf8_constexpr14 utf8_nonnull utf8_pure utf8_int8_t *
utf8chr(const utf8_int8_t *src, utf8_int32_t chr);

/* The position of the utf8 string needle in the utf8 string haystack. */
utf8_constexpr14 utf8_nonnull utf8_pure utf8_int8_t *
utf8str(const utf8_int8_t *haystack, const utf8_int8_t *needle);

/* The position of the utf8 string needle in the utf8 string haystack, case
 * insensitive. */
utf8_constexpr14 utf8_nonnull utf8_pure utf8_int8_t *
utf8casestr(const utf8_int8_t *haystack, const utf8_int8_t *needle);

/* Sets out_codepoint to the current utf8 codepoint in str, and returns the
 * address of the next utf8 codepoint after the current one in str. */
utf8_constexpr14 utf8_nonnull utf8_int8_t *
utf8codepoint(const utf8_int8_t *utf8_restrict str,
              utf8_int32_t *utf8_restrict out_codepoint);

/* Returns 1 if the given character is uppercase, or 0 if it is not. */
utf8_constexpr14 int utf8isupper(utf8_int32_t chr);

/* Make a codepoint lower case if possible. */
utf8_constexpr14 utf8_int32_t utf8lwrcodepoint(utf8_int32_t cp);

/* Make a codepoint upper case if possible. */
utf8_constexpr14 utf8_int32_t utf8uprcodepoint(utf8_int32_t cp);

/* Sets out_codepoint to the current utf8 codepoint in str, and returns the
 * address of the previous utf8 codepoint before the current one in str. */
utf8_constexpr14 utf8_nonnull utf8_int8_t *
utf8rcodepoint(const utf8_int8_t *utf8_restrict str,
               utf8_int32_t *utf8_restrict out_codepoint);

#undef utf8_weak
#undef utf8_pure
#undef utf8_nonnull

utf8_constexpr14_impl utf8_int8_t *
utf8chr(const utf8_int8_t *src, utf8_int32_t chr)
{
	utf8_int8_t c[5] = {'\0', '\0', '\0', '\0', '\0'};

	if (0 == chr) {
		/* Being asked to return position of null terminating byte, so
		* just run s to the end, and return! */
		while ('\0' != *src)
			src++;
		return (utf8_int8_t *)src;
	} else if (0 == ((utf8_int32_t)0xffffff80 & chr)) {
		/* 1-byte/7-bit ascii
		* (0b0xxxxxxx) */
		c[0] = (utf8_int8_t)chr;
	} else if (0 == ((utf8_int32_t)0xfffff800 & chr)) {
		/* 2-byte/11-bit utf8 code point
		* (0b110xxxxx 0b10xxxxxx) */
		c[0] = (utf8_int8_t)(0xc0 | (utf8_int8_t)(chr >> 6));
		c[1] = (utf8_int8_t)(0x80 | (utf8_int8_t)(chr & 0x3f));
	} else if (0 == ((utf8_int32_t)0xffff0000 & chr)) {
		/* 3-byte/16-bit utf8 code point
		* (0b1110xxxx 0b10xxxxxx 0b10xxxxxx) */
		c[0] = (utf8_int8_t)(0xe0 | (utf8_int8_t)(chr >> 12));
		c[1] = (utf8_int8_t)(0x80 | (utf8_int8_t)((chr >> 6) & 0x3f));
		c[2] = (utf8_int8_t)(0x80 | (utf8_int8_t)(chr & 0x3f));
	} else { /* if (0 == ((int)0xffe00000 & chr)) { */
		/* 4-byte/21-bit utf8 code point
		* (0b11110xxx 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx) */
		c[0] = (utf8_int8_t)(0xf0 | (utf8_int8_t)(chr >> 18));
		c[1] = (utf8_int8_t)(0x80 | (utf8_int8_t)((chr >> 12) & 0x3f));
		c[2] = (utf8_int8_t)(0x80 | (utf8_int8_t)((chr >> 6) & 0x3f));
		c[3] = (utf8_int8_t)(0x80 | (utf8_int8_t)(chr & 0x3f));
	}

	/* we've made c into a 2 utf8 codepoint string, one for the chr we are
	* seeking, another for the null terminating byte. Now use utf8str to
	* search */
	return utf8str(src, c);
}

utf8_constexpr14_impl utf8_int8_t *
utf8str(const utf8_int8_t *haystack, const utf8_int8_t *needle)
{
	utf8_int32_t throwaway_codepoint = 0;

	/* if needle has no utf8 codepoints before the null terminating
	* byte then return haystack */
	if ('\0' == *needle)
		return (utf8_int8_t *)haystack;

	  while ('\0' != *haystack) {
		const utf8_int8_t *maybeMatch = haystack;
		const utf8_int8_t *n = needle;

		while (*haystack == *n && (*haystack != '\0' && *n != '\0')) {
			n++;
			haystack++;
		}

		if ('\0' == *n) {
			/* we found the whole utf8 string for needle in haystack at
			* maybeMatch, so return it */
			return (utf8_int8_t *)maybeMatch;
		} else {
			/* h could be in the middle of an unmatching utf8 codepoint,
			* so we need to march it on to the next character beginning
			* starting from the current character */
			haystack = utf8codepoint(maybeMatch, &throwaway_codepoint);
		}
	}

	/* no match */
	return utf8_null;
}

utf8_constexpr14_impl utf8_int8_t *
utf8casestr(const utf8_int8_t *haystack, const utf8_int8_t *needle)
{
  /* if needle has no utf8 codepoints before the null terminating
   * byte then return haystack */
	if ('\0' == *needle)
		return (utf8_int8_t *)haystack;

	for (;;) {
		const utf8_int8_t *maybeMatch = haystack;
		const utf8_int8_t *n = needle;
		utf8_int32_t h_cp = 0, n_cp = 0;

		/* Get the next code point and track it */
		const utf8_int8_t *nextH = haystack = utf8codepoint(haystack, &h_cp);
		n = utf8codepoint(n, &n_cp);

		while ((0 != h_cp) && (0 != n_cp)) {
			h_cp = utf8lwrcodepoint(h_cp);
			n_cp = utf8lwrcodepoint(n_cp);

			/* if we find a mismatch, bail out! */
			if (h_cp != n_cp)
				break;

			haystack = utf8codepoint(haystack, &h_cp);
			n = utf8codepoint(n, &n_cp);
		}

		if (0 == n_cp) {
			/* we found the whole utf8 string for needle in haystack at
			* maybeMatch, so return it */
			return (utf8_int8_t *)maybeMatch;
		}

		if (0 == h_cp) {
			/* no match */
			return utf8_null;
		}

		/* Roll back to the next code point in the haystack to test */
		haystack = nextH;
	}
}

utf8_constexpr14_impl utf8_int8_t *
utf8codepoint(const utf8_int8_t *utf8_restrict str,
              utf8_int32_t *utf8_restrict out_codepoint)
{
	if (0xf0 == (0xf8 & str[0])) {
		/* 4 byte utf8 codepoint */
		*out_codepoint = ((0x07 & str[0]) << 18) | ((0x3f & str[1]) << 12) |
						((0x3f & str[2]) << 6) | (0x3f & str[3]);
		str += 4;
	} else if (0xe0 == (0xf0 & str[0])) {
		/* 3 byte utf8 codepoint */
		*out_codepoint =
			((0x0f & str[0]) << 12) | ((0x3f & str[1]) << 6) | (0x3f & str[2]);
		str += 3;
	} else if (0xc0 == (0xe0 & str[0])) {
		/* 2 byte utf8 codepoint */
		*out_codepoint = ((0x1f & str[0]) << 6) | (0x3f & str[1]);
		str += 2;
	} else {
		/* 1 byte utf8 codepoint otherwise */
		*out_codepoint = str[0];
		str += 1;
	}

	return (utf8_int8_t *)str;
}

utf8_constexpr14_impl int
utf8isupper(utf8_int32_t chr)
{
	return chr != utf8lwrcodepoint(chr);
}

utf8_constexpr14_impl utf8_int32_t
utf8uprcodepoint(utf8_int32_t cp)
{
	if (((0x0061 <= cp) && (0x007a >= cp)) ||
	((0x00e0 <= cp) && (0x00f6 >= cp)) ||
	((0x00f8 <= cp) && (0x00fe >= cp)) ||
	((0x03b1 <= cp) && (0x03c1 >= cp)) ||
	((0x03c3 <= cp) && (0x03cb >= cp)) ||
	((0x0430 <= cp) && (0x044f >= cp))) {
		cp -= 32;
	} else if ((0x0450 <= cp) && (0x045f >= cp)) {
		cp -= 80;
	} else if (((0x0100 <= cp) && (0x012f >= cp)) ||
	((0x0132 <= cp) && (0x0137 >= cp)) ||
	((0x014a <= cp) && (0x0177 >= cp)) ||
	((0x0182 <= cp) && (0x0185 >= cp)) ||
	((0x01a0 <= cp) && (0x01a5 >= cp)) ||
	((0x01de <= cp) && (0x01ef >= cp)) ||
	((0x01f8 <= cp) && (0x021f >= cp)) ||
	((0x0222 <= cp) && (0x0233 >= cp)) ||
	((0x0246 <= cp) && (0x024f >= cp)) ||
	((0x03d8 <= cp) && (0x03ef >= cp)) ||
	((0x0460 <= cp) && (0x0481 >= cp)) ||
	((0x048a <= cp) && (0x04ff >= cp))) {
		cp &= ~0x1;
	} else if (((0x0139 <= cp) && (0x0148 >= cp)) ||
	((0x0179 <= cp) && (0x017e >= cp)) ||
	((0x01af <= cp) && (0x01b0 >= cp)) ||
	((0x01b3 <= cp) && (0x01b6 >= cp)) ||
	((0x01cd <= cp) && (0x01dc >= cp))) {
		cp -= 1;
		cp |= 0x1;
	} else {
		switch (cp) {
		default: break;
		case 0x00ff:
			cp = 0x0178;
			break;
		case 0x0180:
			cp = 0x0243;
			break;
		case 0x01dd:
			cp = 0x018e;
			break;
		case 0x019a:
			cp = 0x023d;
			break;
		case 0x019e:
			cp = 0x0220;
			break;
		case 0x0292:
			cp = 0x01b7;
			break;
		case 0x01c6:
			cp = 0x01c4;
			break;
		case 0x01c9:
			cp = 0x01c7;
			break;
		case 0x01cc:
			cp = 0x01ca;
			break;
		case 0x01f3:
			cp = 0x01f1;
			break;
		case 0x01bf:
			cp = 0x01f7;
			break;
		case 0x0188:
			cp = 0x0187;
			break;
		case 0x018c:
			cp = 0x018b;
			break;
		case 0x0192:
			cp = 0x0191;
			break;
		case 0x0199:
			cp = 0x0198;
			break;
		case 0x01a8:
			cp = 0x01a7;
			break;
		case 0x01ad:
			cp = 0x01ac;
			break;
		case 0x01b0:
			cp = 0x01af;
			break;
		case 0x01b9:
			cp = 0x01b8;
			break;
		case 0x01bd:
			cp = 0x01bc;
			break;
		case 0x01f5:
			cp = 0x01f4;
			break;
		case 0x023c:
			cp = 0x023b;
			break;
		case 0x0242:
			cp = 0x0241;
			break;
		case 0x037b:
			cp = 0x03fd;
			break;
		case 0x037c:
			cp = 0x03fe;
			break;
		case 0x037d:
			cp = 0x03ff;
			break;
		case 0x03f3:
			cp = 0x037f;
			break;
		case 0x03ac:
			cp = 0x0386;
			break;
		case 0x03ad:
			cp = 0x0388;
			break;
		case 0x03ae:
			cp = 0x0389;
			break;
		case 0x03af:
			cp = 0x038a;
			break;
		case 0x03cc:
			cp = 0x038c;
			break;
		case 0x03cd:
			cp = 0x038e;
			break;
		case 0x03ce:
			cp = 0x038f;
			break;
		case 0x0371:
			cp = 0x0370;
			break;
		case 0x0373:
			cp = 0x0372;
			break;
		case 0x0377:
			cp = 0x0376;
			break;
		case 0x03d1:
			cp = 0x0398;
			break;
		case 0x03d7:
			cp = 0x03cf;
			break;
		case 0x03f2:
			cp = 0x03f9;
			break;
		case 0x03f8:
			cp = 0x03f7;
			break;
		case 0x03fb:
			cp = 0x03fa;
			break;
		}
	}

	return cp;
}

utf8_constexpr14_impl utf8_int32_t
utf8lwrcodepoint(utf8_int32_t cp)
{
	if (((0x0041 <= cp) && (0x005a >= cp)) ||
	((0x00c0 <= cp) && (0x00d6 >= cp)) ||
	((0x00d8 <= cp) && (0x00de >= cp)) ||
	((0x0391 <= cp) && (0x03a1 >= cp)) ||
	((0x03a3 <= cp) && (0x03ab >= cp)) ||
	((0x0410 <= cp) && (0x042f >= cp))) {
		cp += 32;
	} else if ((0x0400 <= cp) && (0x040f >= cp)) {
		cp += 80;
	} else if (((0x0100 <= cp) && (0x012f >= cp)) ||
	((0x0132 <= cp) && (0x0137 >= cp)) ||
	((0x014a <= cp) && (0x0177 >= cp)) ||
	((0x0182 <= cp) && (0x0185 >= cp)) ||
	((0x01a0 <= cp) && (0x01a5 >= cp)) ||
	((0x01de <= cp) && (0x01ef >= cp)) ||
	((0x01f8 <= cp) && (0x021f >= cp)) ||
	((0x0222 <= cp) && (0x0233 >= cp)) ||
	((0x0246 <= cp) && (0x024f >= cp)) ||
	((0x03d8 <= cp) && (0x03ef >= cp)) ||
	((0x0460 <= cp) && (0x0481 >= cp)) ||
	((0x048a <= cp) && (0x04ff >= cp))) {
		cp |= 0x1;
	} else if (((0x0139 <= cp) && (0x0148 >= cp)) ||
	((0x0179 <= cp) && (0x017e >= cp)) ||
	((0x01af <= cp) && (0x01b0 >= cp)) ||
	((0x01b3 <= cp) && (0x01b6 >= cp)) ||
	((0x01cd <= cp) && (0x01dc >= cp))) {
		cp += 1;
		cp &= ~0x1;
	} else {
		switch (cp) {
		default: break;
		case 0x0178:
			cp = 0x00ff;
			break;
		case 0x0243:
			cp = 0x0180;
			break;
		case 0x018e:
			cp = 0x01dd;
			break;
		case 0x023d:
			cp = 0x019a;
			break;
		case 0x0220:
			cp = 0x019e;
			break;
		case 0x01b7:
			cp = 0x0292;
			break;
		case 0x01c4:
			cp = 0x01c6;
			break;
		case 0x01c7:
			cp = 0x01c9;
			break;
		case 0x01ca:
			cp = 0x01cc;
			break;
		case 0x01f1:
			cp = 0x01f3;
			break;
		case 0x01f7:
			cp = 0x01bf;
			break;
		case 0x0187:
			cp = 0x0188;
			break;
		case 0x018b:
			cp = 0x018c;
			break;
		case 0x0191:
			cp = 0x0192;
			break;
		case 0x0198:
			cp = 0x0199;
			break;
		case 0x01a7:
			cp = 0x01a8;
			break;
		case 0x01ac:
			cp = 0x01ad;
			break;
		case 0x01af:
			cp = 0x01b0;
			break;
		case 0x01b8:
			cp = 0x01b9;
			break;
		case 0x01bc:
			cp = 0x01bd;
			break;
		case 0x01f4:
			cp = 0x01f5;
			break;
		case 0x023b:
			cp = 0x023c;
			break;
		case 0x0241:
			cp = 0x0242;
		  break;
		case 0x03fd:
			cp = 0x037b;
			break;
		case 0x03fe:
			cp = 0x037c;
			break;
		case 0x03ff:
			cp = 0x037d;
			break;
		case 0x037f:
			cp = 0x03f3;
			break;
		case 0x0386:
			cp = 0x03ac;
			break;
		case 0x0388:
			cp = 0x03ad;
			break;
		case 0x0389:
			cp = 0x03ae;
			break;
		case 0x038a:
			cp = 0x03af;
			break;
		case 0x038c:
			cp = 0x03cc;
			break;
		case 0x038e:
			cp = 0x03cd;
			break;
		case 0x038f:
			cp = 0x03ce;
			break;
		case 0x0370:
			cp = 0x0371;
			break;
		case 0x0372:
			cp = 0x0373;
			break;
		case 0x0376:
			cp = 0x0377;
			break;
		case 0x03f4:
			cp = 0x03b8;
			break;
		case 0x03cf:
			cp = 0x03d7;
			break;
		case 0x03f9:
			cp = 0x03f2;
			break;
		case 0x03f7:
			cp = 0x03f8;
			break;
		case 0x03fa:
			cp = 0x03fb;
			break;
		}
	}

	return cp;
}

utf8_constexpr14_impl utf8_int8_t *
utf8rcodepoint(const utf8_int8_t *utf8_restrict str,
               utf8_int32_t *utf8_restrict out_codepoint)
{
  const utf8_int8_t *s = (const utf8_int8_t *)str;

  if (0xf0 == (0xf8 & s[0])) {
    /* 4 byte utf8 codepoint */
    *out_codepoint = ((0x07 & s[0]) << 18) | ((0x3f & s[1]) << 12) |
                     ((0x3f & s[2]) << 6) | (0x3f & s[3]);
  } else if (0xe0 == (0xf0 & s[0])) {
    /* 3 byte utf8 codepoint */
    *out_codepoint =
        ((0x0f & s[0]) << 12) | ((0x3f & s[1]) << 6) | (0x3f & s[2]);
  } else if (0xc0 == (0xe0 & s[0])) {
    /* 2 byte utf8 codepoint */
    *out_codepoint = ((0x1f & s[0]) << 6) | (0x3f & s[1]);
  } else {
    /* 1 byte utf8 codepoint otherwise */
    *out_codepoint = s[0];
  }

  do {
    s--;
  } while ((0 != (0x80 & s[0])) && (0x80 == (0xc0 & s[0])));

  return (utf8_int8_t *)s;
}

#undef utf8_restrict
#undef utf8_constexpr14
#undef utf8_null

#ifdef __cplusplus
} /* extern "C" */
#endif

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif /* SHEREDOM_UTF8_H_INCLUDED */
