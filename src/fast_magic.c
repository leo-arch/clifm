/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* fast_magic.c -- fast MIME-type detection using magic bytes */
/* Note: For the time being, this module handles only binary files. */

#ifndef NO_FAST_MAGIC

#include "helpers.h" /* IS_DIGIT, IS_ALPHA_UP, IS_ALPHA_LOW, IS_ALNUM */

#include <unistd.h> /* close() */
#include <string.h> /* memcmp() */
#include <errno.h>

/* How many bytes to read from the input file. */
#define BYTES_TO_READ 8192

/* See RFC-6838 (https://datatracker.ietf.org/doc/html/rfc6838#section-3)
 * for the naming requirements of a MIME-type string. */
#define IS_VALID_MIMETYPE_CHAR(c, subtype)                  \
	(IS_ALNUM((c)) || ((subtype) == 1 && ((c) == '!'        \
	|| (c) == '#' || (c) == '$' || (c) == '-' || (c) == '^' \
	|| (c) == '_' || (c) == '.' || (c) == '+')))

/* Convert S into a little-endian unsigned 32-bit value */
#define LE_U32(s) ((uint32_t)((uint32_t)(s)[0] | ((uint32_t)(s)[1] << 8) \
	| ((uint32_t)(s)[2] << 16) | ((uint32_t)(s)[3] << 24)))
/* Convert S into a big-endian unsigned 32-bit value */
#define BE_U32(s) ((uint32_t)((uint32_t)(s)[0] << 24 \
	| (uint32_t)(s)[1] << 16 | (uint32_t)(s)[2] << 8 | (uint32_t)(s)[3]))
/* Convert S into a little-endian unsigned 16-bit value */
#define LE_U16(s) ((uint16_t)((uint16_t)(s)[0] | ((uint16_t)(s)[1] << 8)))
/* Convert S into a big-endian unsigned 16-bit value */
#define BE_U16(s) ((uint16_t)((uint16_t)(s)[0] << 8) | (uint16_t)(s)[1])

#define GSM_MIN_BYTES 1056

/* Macros for VVC (H.266) file detection */
#define VVC_IDR_W_RADL    7
#define VVC_IDR_N_LP      8
#define VVC_CRA_NUT       9
#define VVC_GDR_NUT       10
#define VVC_RSV_IRAP_11   11
#define VVC_OPI_NUT       12
#define VVC_DCI_NUT       13
#define VVC_VPS_NUT       14
#define VVC_SPS_NUT       15
#define VVC_PPS_NUT       16
#define VVC_EOS_NUT       21
#define VVC_EOB_NUT       22

/* Macros for VC-1 file detection */
#define VC1_CODE_SLICE       0x0B
#define VC1_CODE_FIELD       0x0C
#define VC1_CODE_FRAME       0x0D
#define VC1_CODE_ENTRYPOINT  0x0E
#define VC1_CODE_SEQHDR      0x0F
#define VC1_PROFILE_ADVANCED 3

#define IS_VC1_NAL(n) ((n) == VC1_CODE_SEQHDR || (n) == VC1_CODE_ENTRYPOINT \
	|| (n) == VC1_CODE_FRAME || (n) == VC1_CODE_FIELD || (n) == VC1_CODE_SLICE)

/* Check wether the most significant bit is zero. */
#define MSB_IS_ZERO(b) (((b) >> 7) == 0x00)

static void *
xmemmem(const void *haystack, size_t haylen,
	const void *needle, size_t needlelen)
{
	if (needlelen == 0)
		return (void *)haystack;
	if (haylen < needlelen)
		return NULL;

	const unsigned char *h = (const unsigned char *)haystack;
	const unsigned char *n = (const unsigned char *)needle;

	for (size_t i = 0; i <= haylen - needlelen; i++) {
		if (h[i] == n[0] && memcmp(h + i, n, needlelen) == 0)
			return (void *)(h + i);
	}

	return NULL;
}

/* Return the MIME-type recorded in a ZIP file, or NULL if none is found.
 * This covers (at least):
 * Libreoffice:      application/vnd.oasis.opendocument.*
 * Old OpenOffice:   application/vnd.sun.xml.*
 * CorelDraw:        application/x-vnd.corel.*
 * KDE Office suite: application/x-k* (e.g. application/x-kwrite)
 * OpenRaster:       image/openraster
 * Krita:            application/x-krita
 * Epub:             application/epub+zip */
static const char *
extract_mimetype_from_zip(const uint8_t *str, const size_t str_len,
	const size_t name_offset)
{
	/* It is guaranteed that STR starts with "PK\x03\x04" and that
	 * "mimetype" is found at offset 0x1E (30). */
	if (!str || str_len < name_offset)
		return NULL;

	/* A buffer with enough room to hold a MIME-type string.
	 * See RFC-6838 (https://datatracker.ietf.org/doc/html/rfc6838#section-3)
	 * for the naming requirements of a MIME-type string. */
	static char buf[256];

	/* Length of the content of the "mimetype" tag. */
	const uint32_t uncomp_size = LE_U32(str + 22);
	const size_t mimetype_len = (size_t)uncomp_size;

	if (mimetype_len == 0 || mimetype_len > sizeof(buf) - 1
	|| mimetype_len + name_offset > str_len)
		return NULL;

	/* Position the pointer right after the 'mimetype' tag. */
	const uint8_t *s = str + name_offset;

	size_t slashes = 0;
	for (size_t i = 0; i < mimetype_len; i++) {
		const int is_subtype = (slashes > 0);
		if (s[i] != '/' && !IS_VALID_MIMETYPE_CHAR(s[i], is_subtype))
			return NULL;
		if (s[i] == '/' && i > 0 && i < mimetype_len - 1)
			slashes++;
		buf[i] = (char)s[i];
	}

	buf[mimetype_len] = '\0';

	/* A MIME type has the form "type/subtype": only a single slash is allowed. */
	if (slashes != 1)
		return NULL;

	return buf;
}

static const char *
check_zip_magic(const uint8_t *str, const size_t str_len)
{
	/* A ZIP header is exacly 30 bytes. If there is a 'mimetype' tag,
	 * it must be found at offset 30. */
	if (str_len >= 39 && str[30] == 'm' && str[31] == 'i' && str[32] == 'm'
	&& str[33] == 'e' && str[34] == 't' && str[35] == 'y' && str[36] == 'p'
	&& str[37] == 'e')
		return extract_mimetype_from_zip(str, str_len, 38);

	/* STR starts with "PK\x03\x04" (4 bytes). Let's skip these bytes. */
	const uint8_t *s = str + 4;
	const size_t l = str_len - 4;

	for (size_t i = 0; i < l - 1; i++) {
		const size_t rem = l - i;

		/* MS-Office */
		if (rem >= 5 && s[i] == 'w' && s[i + 1] == 'o' && s[i + 4] == '/'
		&& memcmp(s + i, "word/", 5) == 0)
			return "application/vnd.openxmlformats-officedocument.wordprocessingml.document";

		if (rem >= 15 && s[i] == 'x' && s[i + 1] == 'l' && s[i + 2] == '/'
		&& memcmp(s + i, "xl/workbook.xml", 15) == 0)
			return "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";

		if (rem >= 20 && s[i] == 'p' && s[i + 1] == 'p' && s[i + 3] == '/'
		&& memcmp(s + i, "ppt/presentation.xml", 20) == 0)
			return "application/vnd.openxmlformats-officedocument.presentationml.presentation";

		if (rem >= 5 && s[i] == 'v' && s[i + 1] == 'i' && s[i + 2] == 's'
		&& s[i + 3] == 'i' && s[i + 4] == 'o' && s[i + 5] == '/')
			return "application/vnd.ms-visio.drawing.main+xml";

		/* Android APK */
		if (rem >= 11 && s[i] == 'c' && s[i + 1] == 'l' && s[i + 7] == '.'
		&& memcmp(s + i, "classes.dex", 11) == 0)
			return "application/vnd.android.package-archive";
		if (rem >= 19 && s[i] == 'A' && s[i + 7] == 'M' && s[i + 15] == '.'
		&& memcmp(s + i, "AndroidManifest.xml", 19) == 0)
			return "application/vnd.android.package-archive";

		/* Java archives */
		if (rem >= 20 && s[i] == 'M' && s[i + 8] == '/' && s[i + 17] == '.'
		&& memcmp(s + i, "META-INF/MANIFEST.MF", 20) == 0)
			return "application/java-archive";

		if (rem >= 16 && s[i] == '3' && s[i + 1] == 'D' && s[i + 2] == '/'
		&& memcmp(s + i, "3D/3dmodel.model", 16) == 0)
			return "model/3mf";
	}

	return NULL;
}

static const char *
get_ole2_ms_office_type(const uint8_t *str, const size_t str_len)
{
	/* Check for "MSWordDoc" or "Microsoft Word document data" at offset
	 * 2112 (magic taken from the Shared MIME-info database) */
	size_t offset = 2112; /* 0x840 */
	if (str_len > offset + 9 && str[offset] == 'M' && str[offset + 2] == 'W'
	&& memcmp(str + offset, "MSWordDoc", 9) == 0)
		return "application/msword";
	if (str_len > offset + 14 && str[offset] == 'M' && str[offset + 2] == 'c'
	&& memcmp(str + offset, "Microsoft Word", 14) == 0)
		return "application/msword";

	/* Check for "MSWordDoc" at offset 2108 (Shared MIME-info) */
	offset = 2108; /* 0x83c */
	if (str_len > offset + 9 && str[offset] == 'M' && str[offset + 2] == 'W'
	&& memcmp(str + offset, "MSWordDoc", 9) == 0)
		return "application/msword";

	/* Check for "bjbj" or "jbjb" at offset 546 (Shared MIME-info) */
	offset = 546; /* 0x222 */
	if (str_len > offset + 4 && ((str[offset] == 'b' && str[offset + 1] == 'j'
	&& str[offset + 2] == 'b' && str[offset + 3] == 'j')
	|| (str[offset] == 'j' && str[offset + 1] == 'b'
	&& str[offset + 2] == 'j' && str[offset + 3] == 'b')))
		return "application/msword";

	/* Check for "MSWordDoc" at offset 3136 (MS Word 6.0, 1994) */
	offset = 3136; /* 0xc40 */
	if (str_len > offset + 9 && str[offset] == 'M' && str[offset + 2] == 'W'
	&& memcmp(str + offset, "MSWordDoc", 9) == 0)
		return "application/msword";

	offset = 2080; /* 0x820 */
	/* Check for "Microsoft Word" at offset 2080 */
	if (str_len > offset + 14 && str[offset] == 'M' && str[offset + 2] == 'c'
	&& memcmp(str + offset, "Microsoft Word", 14) == 0)
		return "application/msword";

	/* Check for "Microsoft Excel" at offset 2080 (MIME-info) */
	if (str_len > offset + 15 && str[offset] == 'M' && str[offset + 2] == 'c'
	&& memcmp(str + offset, "Microsoft Excel", 15) == 0)
		return "application/vnd.ms-excel";

	/* Check for "0x00 Calc " at offset 2090 (XLS created with LibreOffice) */
	if (str_len > offset + 16 && str[offset + 11] == 'C'
	&& str[offset + 14] == 'c' && memcmp(str + offset + 10, "\0Calc ", 6) == 0)
		return "application/vnd.ms-excel";

	/* Check for "MS PowerPoint" at offset 2080 */
	if (str_len > offset + 13 && str[offset] == 'M' && str[offset + 3] == 'P'
	&& memcmp(str + offset, "MS PowerPoint", 13) == 0)
		return "application/vnd.ms-powerpoint";

	/* Check for "Workbook" (16 bit) at offset 1152 */
	offset = 1152; /* 0x480 */
	if (str_len > offset + 16 && str[offset] == 'W' && str[offset + 6] == 'k'
	&& memcmp(str + offset, "W\0o\0r\0k\0b\0o\0o\0k\0", 16) == 0)
		return "application/vnd.ms-excel";

	if (str_len <= 8)
		return NULL;

	/* Skip Compound Document Format (CDF) bytes (8) */
	const uint8_t *s = str + 8;
	const size_t l = str_len - 8;

	for (size_t i = 0; i + 8 <= l; i++) {
		if (s[i] == 0xEC && s[i + 1] == 0xA5) { /* FibBase (Word) */
			const uint16_t nfib = LE_U16(s + i + 2);
			if (nfib == 0x00A0 || nfib == 0x00A1 || nfib == 0x00B0
			|| nfib == 0x00C0 || nfib == 0x00C1 || nfib == 0x00D9)
				return "application/msword";
		}

		if (s[i] == 0x09 && s[i + 1] == 0x08) { /* BOF (Excel) */
			const uint16_t reclen = LE_U16(s + i + 2);
			if (reclen < 4 || i + 4 + reclen > l)
				continue;

			const uint16_t version = LE_U16(s + i + 4);
			const uint16_t type = LE_U16(s + i + 6);
			if ((version == 0x0600 || version == 0x0500) &&
			(type == 0x0005 || type == 0x0010 || type == 0x0020))
				return "application/vnd.ms-excel";
		}
	}

	if (l >= 10 && (xmemmem(s, l, "PowerPoint", 10)
	|| xmemmem(s, l, "_\0P\0P\0T\0", 8)))
		return "application/vnd.ms-powerpoint";

	return NULL;
}

static const char *
check_ftyp_magic(const uint8_t *s, const size_t l)
{
	/* iso */
	if (l > 2 && s[0] == 'i' && s[1] == 's' && s[2] == 'o')
		return "video/mp4";

	/* mp41, mp42, avc1, MSNV, XACV, mobi, mmp4, NDSC, NDSH, NDSM,
	 * NDSP, NDSS, NDXC, NDXM, NDXP, NDXS, niko, bbxm, dash, isml, ARRI */
	if (l > 3 && ((s[0] == 'm' && s[1] == 'p' && s[2] == '4' &&
	(s[3] == '1' || s[3] == '2'))
	|| (s[0] == 'a' && s[1] == 'v' && s[2] == 'c' && s[3] == '1')
	|| (s[0] == 'M' && s[1] == 'S' && s[2] == 'N' && s[3] == 'V')
	|| (s[0] == 'X' && s[1] == 'A' && s[2] == 'V' && s[3] == 'C')
	|| (s[0] == 'm' && s[1] == 'o' && s[2] == 'b' && s[3] == 'i')
	|| (s[0] == 'm' && s[1] == 'm' && s[2] == 'p' && s[3] == '4')
	|| (s[0] == 'N' && s[1] == 'D' && s[2] == 'S' && s[3] == 'C')
	|| (s[0] == 'N' && s[1] == 'D' && s[2] == 'S' && s[3] == 'H')
	|| (s[0] == 'N' && s[1] == 'D' && s[2] == 'S' && s[3] == 'M')
	|| (s[0] == 'N' && s[1] == 'D' && s[2] == 'S' && s[3] == 'P')
	|| (s[0] == 'N' && s[1] == 'D' && s[2] == 'S' && s[3] == 'S')
	|| (s[0] == 'N' && s[1] == 'D' && s[2] == 'X' && s[3] == 'C')
	|| (s[0] == 'N' && s[1] == 'D' && s[2] == 'X' && s[3] == 'H')
	|| (s[0] == 'N' && s[1] == 'D' && s[2] == 'X' && s[3] == 'M')
	|| (s[0] == 'N' && s[1] == 'D' && s[2] == 'X' && s[3] == 'P')
	|| (s[0] == 'N' && s[1] == 'D' && s[2] == 'X' && s[3] == 'S')
	|| (s[0] == 'n' && s[1] == 'i' && s[2] == 'k' && s[3] == 'o')
	|| (s[0] == 'b' && s[1] == 'b' && s[2] == 'x' && s[3] == 'm')
	|| (s[0] == 'd' && s[1] == 'a' && s[2] == 's' && s[3] == 'h')
	|| (s[0] == 'i' && s[1] == 's' && s[2] == 'm' && s[3] == 'l')
	|| (s[0] == 'A' && s[1] == 'R' && s[2] == 'R' && s[3] == 'I')))
		return "video/mp4";

	/* M4P, F4V, F4P */
	if (l > 2 && ((s[0] == 'M' && s[1] == '4' && s[2] == 'P')
	|| (s[0] == 'F' && s[1] == '4' && s[2] == 'V')
	|| (s[0] == 'f' && s[1] == '4' && s[2] == 'v')
	|| (s[0] == 'F' && s[1] == '4' && s[2] == 'P')))
		return "video/mp4";

	if 	(l > 2 && ((s[0] == 'M' && s[1] == '4' && s[2] == 'B')
	|| (s[0] == 'F' && s[1] == '4' && s[2] == 'A')
	|| (s[0] == 'F' && s[1] == '4' && s[2] == 'B')))
		return "audio/mp4";

	if (l > 3 && s[0] == 'N' && s[1] == 'D' && s[2] == 'A' && s[3] == 'S')
		return "audio/mp4";

	/* 3ge, 3gg, 3gh, 3gm, 3gp, 3gr, 3gs, 3gt, 3g2, KDDI */
	if (l > 2 && s[0] == '3' && s[1] == 'g') {
		if (s[2] == 'e' || s[2] == 'g' || s[2] == 'h' || s[2] == 'm'
		|| s[2] == 'p' || s[2] == 'r' || s[2] == 's' ||s[2] == 't')
			return "video/3gpp";
		if (s[2] == '2')
			return "video/3gpp2";
	}
	if (l > 3 && s[0] == 'K' && s[1] == 'D' && s[2] == 'D' && s[3] == 'I')
		return "video/3gpp2";

	if (l > 2 && s[0] == 'j' && s[1] == 'x' && s[2] == 'l')
		return "image/jxl";

	if (l > 2 && s[0] == 'j' && s[1] == 'p' && s[2] == 'x')
		return "image/jpx";

	if (l > 2 && s[0] == 'j' && s[1] == 'p' && s[2] == 'm')
		return "image/jpm";

	if (l > 2 && ((s[0] == 'j' && s[1] == 'p' && s[2] == '2')
	|| (s[0] == 'J' && s[1] == 'P' && s[2] == '2')))
		return "image/jp2";

	/* mjp2, mj2s, MGSV */
	if (l > 3 && ((s[0] == 'm' && s[1] == 'j' && s[2] == 'p' && s[3] == '2')
	|| (s[0] == 'm' && s[1] == 'j' && s[2] == '2' && s[3] == 's')
	|| (s[0] == 'M' && s[1] == 'G' && s[2] == 'S' && s[3] == 'V')))
		return "video/mj2";

	if (l > 2 && s[0] == 'M' && s[1] == '4' && s[2] == 'V')
		return "video/x-m4v";

	if (l > 2 && s[0] == 'M' && s[1] == '4' && s[2] == 'A')
		return "audio/x-m4a";

	if (l > 3 && ((s[0] == 'm' && s[1] == 'i' && s[2] == 'f' && s[3] == '1')
	|| (s[0] == 'h' && s[1] == 'e' && s[2] == 'i' && s[3] == 'm')
	|| (s[0] == 'h' && s[1] == 'e' && s[2] == 'i' && s[3] == 's')
	|| (s[0] == 'a' && s[1] == 'v' && s[2] == 'i' && s[3] == 'c')))
		return "image/heif";

	if (l > 3 && ((s[0] == 'm' && s[1] == 's' && s[2] == 'f' && s[3] == '1')
	|| (s[0] == 'h' && s[1] == 'e' && s[2] == 'v' && s[3] == 'm')
	|| (s[0] == 'h' && s[1] == 'e' && s[2] == 'v' && s[3] == 's')
	|| (s[0] == 'a' && s[1] == 'v' && s[2] == 'c' && s[3] == 's')))
		return "image/heif-sequence";

	if (l > 3 && ((s[0] == 'h' && s[1] == 'e' && s[2] == 'i' && s[3] == 'c')
	|| (s[0] == 'h' && s[1] == 'e' && s[2] == 'i' && s[3] == 'x')))
		return "image/heic";

	if (l > 3 && ((s[0] == 'h' && s[1] == 'e' && s[2] == 'v' && s[3] == 'c')
	|| (s[0] == 'h' && s[1] == 'e' && s[2] == 'v' && s[3] == 'x')))
		return "image/heic-sequence";

	if (l > 2 && ((s[0] == 'q' && s[1] == 't')
	|| (s[0] == 'm' && s[1] == 'q' && s[2] == 't'))) /* .mov, .qt */
		return "video/quicktime";

	/* avif, avis */
	if (l > 3 && ((s[0] == 'a' && s[1] == 'v' && s[2] == 'i' && s[3] == 'f')
	|| (s[0] == 'a' && s[1] == 'v' && s[2] == 'i' && s[3] == 's')))
		return "image/avif";

	if (l > 2 && s[0] == 'c' && s[1] == 'r' && s[2] == 'x')
		return "image/x-canon-cr3";

	if (l > 2 && s[0] == 'a' && s[1] == 'a' && s[2] == 'x') {
		if (l > 3 && s[3] == 'c')
			return "audio/vnd.audible.aaxc";
		return "audio/vnd.audible.aax";
	}

	if (l > 3 && ((s[0] == 'd' && s[1] == 'v' && s[2] == 'r' && s[3] == '1')
	|| (s[0] == 'e' && s[1] == 'm' && s[2] == 's' && s[3] == 'g')))
		return "video/vnd.dvb.file";

	return NULL;
}

static const char *
check_ogg_magic(const uint8_t *s, const size_t slen)
{
	if (slen < 28)
		return NULL;

	const uint8_t *name = s + 28;
	const size_t l = slen - 28;
	if (l >= 7 && *name == 0x01 && memcmp(name, "\x01vorbis", 7) == 0)
		return "audio/ogg"; /* audio/x-vorbis+ogg (MIME-info) */
	if (l >= 8 && *name == 'O' && memcmp(name, "OpusHead", 8) == 0)
		return "audio/ogg"; /* audio/x-opus+ogg (MIME-info) */
	if (l >= 5 && *name == 0177 && memcmp(name, "\177FLAC", 5) == 0)
		return "audio/ogg"; /* audio/x-flac+ogg (MIME-info) */
	if (l >= 8 && *name == 'S' && memcmp(name, "Speex   ", 8) == 0)
		return "audio/ogg"; /* audio/x-speex+ogg (MIME-info) */
	if (l >= 8 && *name == 'f' && memcmp(name, "fishead\0", 8) == 0)
		return "video/ogg";
	if (l >= 7 && *name == 0200 && memcmp(name, "\200theora", 7) == 0)
		return "video/ogg"; /* video/x-theora+ogg (MIME-info) */
	if (l >= 9 && *name == 0x01 && memcmp(name, "\x01video\0\0\0", 9) == 0)
		return "video/ogg"; /* video/x-ogm+ogg (MIME-info) */
	if (l >= 9 && *name == 0200 && memcmp(name, "\200kate\0\0\0\0", 9) == 0)
		return "application/ogg";

	return NULL;
}

static size_t
get_ebml_vint_len(const uint8_t vint)
{
	size_t vint_len = 1;
	uint8_t mask = 0x80;
	while (vint_len < 8 && !(vint & mask)) {
		vint_len++;
		mask >>= 1;
	}

	return vint_len;
}

/* See http://fileformats.archiveteam.org/wiki/EBML */
static const char *
check_ebml_magic(const uint8_t *s, const size_t nread)
{
	/* In practice, docType appears within the first 128–512 bytes for
	 * valid files, so that 1K is more than enough. */
	const size_t max = nread > 1024 ? 1024 : nread;
	size_t n = 0;

	/* The EBML header takes 4 bytes (0..3): skip these bytes. */
	for (size_t i = 4; i + 2 < max; i++) {
		if (s[i] == 0x42 && s[i + 1] == 0x82) {
			const size_t vint_len = get_ebml_vint_len(s[i + 2]);
			if (i + 2 + vint_len > max)
				return NULL;
			n = i + 2 + vint_len; /* Skip ID (0x42 0x82) + VINT */
			break;
		}
	}

	if (n == 0 || n + 8 > nread)
		return NULL;

	const uint8_t *p = s + n;
	if (*p == 'w' && memcmp(p, "webm", 4) == 0)
		return "video/webm";
	if (*p == 'm' && memcmp(p, "matroska", 8) == 0)
		return "video/x-matroska";

	return NULL;
}

/* See http://fileformats.archiveteam.org/wiki/RIFF */
static const char *
check_riff_magic(const uint8_t *buf, const size_t buf_len)
{
	if (buf_len < 12)
		return NULL;

	if (buf[8] == 'W' && buf[9] == 'E' && buf[10] == 'B' && buf[11] == 'P')
		return "image/webp";
	/* FFmpeg: libavformat/avidec.c */
	if (buf[8] == 'A' && buf[9] == 'V' && buf[10] == 'I'
	&& (buf[11] == ' ' || buf[11] == 'X' || buf[11] == 0x19))
		return "video/x-msvideo";
	if (buf[8] == 'W' && buf[9] == 'A' && buf[10] == 'V' && buf[11] == 'E')
		return "audio/x-wav";
	if (buf[8] == 'A' && buf[9] == 'M' && buf[10] == 'V' && buf[11] == ' ')
		return "video/x-amv";
	if (buf[8] == 'A' && buf[9] == 'C' && buf[10] == 'O' && buf[11] == 'N')
		return "application/x-navi-animation";
	if (buf[8] == 'M' && buf[9] == 'I' && buf[10] == 'D' && buf[11] == 'S')
		return "audio/x-mids";
	if (buf[8] == 'R' && buf[9] == 'M' && buf[10] == 'M' && buf[11] == 'P')
		return "video/x-mmm";
	if (buf[8] == 'R' && buf[9] == 'M' && buf[10] == 'I' && buf[11] == 'D')
		return "audio/mid";
	if (buf[8] == 's' && buf[9] == 'f' && buf[10] == 'b' && buf[11] == 'k')
		return "audio/x-sfbk";
	if (buf[8] == 'E' && buf[9] == 'g' && buf[10] == 'g' && buf[11] == '!')
		return "application/x-aep";
	if (buf[8] == 'R' && buf[9] == 'D' && buf[10] == 'I' && buf[11] == 'B')
		return "image/x-rdib";
	if (buf[8] == 'A' && buf[9] == 'C' && buf[10] == 'I' && buf[11] == 'D')
		return "audio/x-acid";
	if (buf[8] == '4' && buf[9] == 'X' && buf[10] == 'M' && buf[11] == 'V')
		return "video/x-4xmv";
	if (buf[8] == 'C' && buf[9] == 'D' && buf[10] == 'X' && buf[11] == 'A')
		return "video/x-cdxa";
	if (buf[8] == 'V' && buf[9] == 'D' && buf[10] == 'R' && buf[11] == 'M')
		return "video/x-vdr";
	/* https://datatracker.ietf.org/doc/html/rfc3625 */
	if (buf[8] == 'Q' && buf[9] == 'L' && buf[10] == 'C' && buf[11] == 'M')
		return "audio/qcelp";
	if (buf[8] == 'X' && buf[9] == 'W' && buf[10] == 'M' && buf[11] == 'A')
		return "audio/vnd.ms-xwma";
	if (buf[8] == 'T' && buf[9] == 'R' && buf[10] == 'I' && buf[11] == 'D')
		return "application/x-trid-trd";
	if (buf[8] == 'C' && buf[9] == 'G' && buf[10] == 'F' && buf[11] == 'X')
		return "image/x-commodore-cgfx";

	/* http://fileformats.archiveteam.org/wiki/Notation_Interchange_File_Format */
	if (buf[8] == 'N' && buf[9] == 'I' && buf[10] == 'F' && buf[11] == 'F')
		return "application/vnd.music-niff";

	if (buf[8] == 'M' && buf[9] == 'C' && buf[10] == '9' && buf[11] == '5')
		return "video/x-shockwave-director";
	if (buf[8] == 'M' && buf[9] == 'V' && buf[10] == '9' && buf[11] == '3')
		return "video/x-shockwave-director";
	if (buf[8] == 'F' && buf[9] == 'G' && buf[10] == 'D' && buf[11] == 'M')
		return "video/x-shockwave-director";
	if (buf[8] == 'F' && buf[9] == 'G' && buf[10] == 'D' && buf[11] == 'C')
		return "video/x-shockwave-director";

	if (buf[8] == 's' && buf[9] == 'h') {
		if (buf[10] == 'w' && (buf[11] == '4' || buf[11] == '5'))
			return "application/x-corel-shw";
		if (buf[10] == 'r' && buf[11] == '5')
			return "application/x-corel-shr";
		if (buf[10] == 'l' && buf[11] == '5')
			return "application/x-corel-shb";
	}
	if (buf[8] == 'i' && buf[9] == 'm' && buf[10] == 'a' && buf[11] == 'g')
		return "video/x-corel-cif";
	/* See http://justsolve.archiveteam.org/wiki/CorelDRAW */
	if (buf[8] == 'C' && buf[9] == 'M' && buf[10] == 'X' && buf[11] == '1')
		return "application/vnd.corel-draw";
	if (buf_len > 15 && buf[8] == 'C' && buf[9] == 'D' && buf[10] == 'R'
	&& buf[12] == 'v' && buf[13] == 'r' && buf[14] == 's' && buf[15] == 'n')
		return "application/vnd.corel-draw";
	if (buf_len > 15 && buf[8] == 'c' && buf[9] == 'm' && buf[10] == 'o'
	&& buf[11] == 'v' && buf[12] == 'D' && buf[13] == 'I' && buf[14] == 'S'
	&& buf[15] == 'P')
		return "video/x-corel-move";

	if (buf_len > 15 && buf[8] == 'I' && buf[9] == 'D' && buf[10] == 'F'
	&& buf[11] == ' ' && buf[12] == 'L' && buf[13] == 'I' && buf[14] == 'S'
	&& buf[15] == 'T')
		return "audio/x-idf";

	/* https://www.cpcwiki.eu/index.php/Format:CPR_CPC_Plus_cartridge_file_format */
	if (buf_len > 11 && buf[8] == 'A' && (buf[9] == 'm' || buf[9] == 'M')
	&& (buf[10] == 's' || buf[10] == 'S') && buf[11] == '!')
		return "application/x-spectrum-cpr";

	/* https://mab.greyserv.net/f/sony_wave64.pdf */
	if (buf_len > 40 && buf[24] == 'w' && buf[28] == 0xF3
	&& memcmp(buf + 24, "wave\xF3\xAC\xD3\x11\x8C\xD1\x00\xC0\x4F\x8E\xDB\x8A", 16) == 0)
		return "audio/x-w64";

	return NULL;
}

static const char *
check_xml_magic(const uint8_t *s, const size_t slen)
{
	/* S starts with "<?xml" (5 bytes). Let's skip those bytes. */
	const uint8_t *p = s + 5;
	size_t rem = slen - 5;
	if (rem > 512)
		rem = 512; /* More than enough bytes to check for an ID tag */

	size_t n = SIZE_MAX; /* Offset */
	for (size_t i = 0; i < rem - 1; i++) {
		/* Skip DOCTYPE, comments, CDATA (!), and processing instructions (?) */
		if (p[i] == '<' && p[i + 1] != '!' && p[i + 1] != '?') {
			n = i + 1; /* start of tag name */
			break;
		}
	}

	if (n == SIZE_MAX) /* No ID tag */
		return "text/xml";

	if (n + 2 <= rem && p[n] == 's' && p[n + 1] == 'v' && p[n + 2] == 'g')
		return "image/svg+xml";

	if (n + 3 <= rem && ((p[n] == 'h' && memcmp(p + n, "html", 4) == 0)
	|| (p[n] == 'H' && memcmp(p + n, "HTML", 4) == 0))) {
		if (n + 7 <= rem && p[n + 4] == ' ' && p[n + 5] == 'x'
		&& p[n + 6] == 'm' && p[n + 7] == 'l')
			return "application/xhtml+xml";
		return "text/html";
	}

	if (n + 6 <= rem && p[n] == 'a' && memcmp(p + n, "abiword", 7) == 0)
		return "application/x-abiword";

	if (n + 3 <= rem && p[n] == 'b' && p[n + 1] == 'o' && p[n + 2] == 'o'
	&& p[n + 3] == 'k')
		return "application/x-docbook+xml";

	if (n + 3 <= rem && p[n] == 'f' && memcmp(p + n, "feed", 4) == 0)
		return "application/atom+xml";

	if (n + 2 <= rem && TOUPPER(p[n]) == 'R' && TOUPPER(p[n + 1]) == 'S'
	&& TOUPPER(p[n + 2]) == 'S')
		return "application/rss+xml";

	if (n + 13 <= rem && p[n] == 'x' && memcmp(p + n, "xsl:stylesheet", 14) == 0)
		return "application/xslt+xml";

	if (n + 10 <= rem && p[n] == 'F' && memcmp(p + n, "FictionBook", 11) == 0)
		return "application/x-fictionbook+xml";

	if (n + 4 <= rem && p[n] == 'x' && memcmp(p + n, "xliff", 5) == 0)
		return "application/xliff+xml";

	if (n + 3 <= rem && p[n] == 'o' && p[n + 1] == 'p' && p[n + 2] == 'm'
	&& p[n + 3] == 'l')
		return "application/x-opml+xml";

	if (n + 3 <= rem && p[n] == 's' && memcmp(p + n, "smil", 4) == 0)
		return "application/smil+xml";

	if (n + 2 <= rem && p[n] == 'i' && p[n + 1] == 't' && p[n + 2] == 's')
		return "application/its+xml";

	if (n + 7 <= rem && p[n] == 'O' && memcmp(p + n, "Ontology", 8) == 0)
		return "application/owl+xml";

	if (n + 2 <= rem && p[n] == 'p' && p[n + 1] == 'e' && p[n + 2] == 'f')
		return "application/x-pef+xml";

	if (n + 5 <= rem && p[n] == 'g' && memcmp(p + n, "gnc-v2", 6) == 0)
		return "application/x-gnucash";

	if (n + 2 <= rem && p[n] == 'X' && p[n + 1] == '3' && p[n + 2] == 'D')
		return "model/x3d+xml";

	if (n + 6 <= rem && p[n] == 'C' && p[n + 1] == 'O' && p[n + 2] == 'L'
	&& p[n + 3] == 'L' && p[n + 4] == 'A' && p[n + 5] == 'D' && p[n + 6] == 'A')
		return "model/vnd.collada+xml";

	if (n + 2 <= rem && p[n] == 'a' && p[n + 1] == 'm' && p[n + 2] == 'f')
		return "application/x-amf";

	if (n + 10 <= rem && p[n] == 'p' && p[n + 1] == 'l'
	&& memcmp(p + n, "playlist ", 9) == 0)
		return "application/xspf+xml";

	return "text/xml";
}

static const char *
check_doctype_magic(const uint8_t *s, const size_t slen)
{
	if (slen < 10)
		return NULL;

	/* The first two characters are "<!" */
	const int is_doctype = ((s[2] == 'D' && memcmp(s + 2, "DOCTYPE ", 8) == 0)
		|| (s[2] == 'd' && memcmp(s + 2, "doctype ", 8) == 0));
	if (!is_doctype)
		return NULL;

	if (slen > 12 && s[10] == 's' && s[11] == 'v' && s[12] == 'g')
		return "image/svg+xml";

	if (slen > 13 && ((s[10] == 'h' && s[11] == 't' && s[12] == 'm'
	&& s[13] == 'l') || (s[10] == 'H' && s[11] == 'T' && s[12] == 'M'
	&& s[13] == 'L'))) {
		if (slen > 100 && xmemmem(s + 14, 100, "//DTD XHTML ", 12))
			return "application/xhtml+xml";
		return "text/html";
	}

	if (slen >= 17 && s[10] == 'a' && s[11] == 'b' && s[12] == 'i'
	&& memcmp(s + 10, "abiword", 7) == 0)
		return "application/x-abiword";

	if (slen > 13 && s[10] == 'b' && s[11] == 'o' && s[12] == 'o'
	&& s[13] == 'k')
		return "application/x-docbook+xml";

	if (slen > 12 && s[10] == 'X' && s[11] == '3' && s[12] == 'D')
		return "model/x3d+xml";

	if (slen > 13 && s[10] == 'x' && s[11] == 'b' && s[12] == 'e'
	&& s[13] == 'l')
		return "application/x-xbel";

	return NULL;
}

/* Return the MIME type of a recognized IFF file, or NULL if none is found.
 * For more FOURCC types (i.e., 4-bytes ASCII ID strings) see
 * https://wiki.multimedia.cx/index.php/IFF */
static const char *
check_iff_magic(const uint8_t *s, const size_t slen)
{
	if (slen < 8)  /* S starts with "FORMxxxx" */
		return NULL;

	/* https://www.mmsp.ece.mcgill.ca/Documents/AudioFormats/CSL/CSL.html */
	if (s[4] == 'D' && s[5] == 'S' && s[6] == '1' && s[7] == '6')
		return "audio/x-nsp";

	const uint8_t *p = s + 8;
	const size_t plen = slen - 8;

	if (plen > 3 && p[0] == 'A' && p[1] == 'I' && p[2] == 'F') {
		if (p[3] == 'F') return "audio/x-aiff";
		if (p[3] == 'C') return "audio/x-aifc";
	}

	if (plen > 3 && p[0] == '8' && p[1] == 'S' && p[2] == 'V' && p[3] == 'X')
		/* Should be audio/x-8svx, but both file(1) and MIME-info use x-aiff */
		return "audio/x-aiff";

	if (plen > 3 && p[0] == 'M' && p[1] == 'A' && p[2] == 'U' && p[3] == 'D')
		return "audio/x-maud";

	if (plen > 3 && p[0] == 'C' && p[1] == 'M' && p[2] == 'U' && p[3] == 'S')
		return "audio/x-cmus";
	if (plen > 3 && p[0] == 'S' && p[1] == 'M' && p[2] == 'U' && p[3] == 'S')
		return "audio/x-smus";

	if (plen > 3 && p[0] == 'I' && p[1] == 'F' && p[2] == 'Z' && p[3] == 'S')
		return "application/x-quetzal";

	if (plen > 3 && p[0] == 'I' && p[1] == 'F' && p[2] == 'R' && p[3] == 'S')
		return "application/x-blorb";

	if (plen > 3 && p[0] == 'R' && p[1] == 'G' && p[2] == 'F' && p[3] == 'X')
		return "image/x-rgfx";

	if (plen > 3 && p[0] == 'F' && p[1] == 'A' && p[2] == 'X' && p[3] == 'X')
		return "image/x-faxx";

	if (plen > 3 && p[0] == 'A' && p[1] == 'M' && p[2] == 'F' && p[3] == 'F')
		return "image/x-amf";

	if (plen > 7 && p[0] == 'I' && p[1] == 'M' && p[2] == 'A' && p[3] == 'G'
	&& p[4] == 'I' && p[5] == 'H' && p[6] == 'D' && p[7] == 'R')
		return "image/x-cdi";

	if (plen > 3 && p[0] == 'T' && p[1] == 'D' && p[2] == 'D' && p[3] == 'D')
		return "model/x-tddd";
	if (plen > 3 && p[0] == 'F' && p[1] == 'A' && p[2] == 'N' && p[3] == 'T')
		return "video/x-fantavision";
	if (plen > 3 && p[0] == 'S' && p[1] == 'S' && p[2] == 'A' && p[3] == ' ')
		return "video/x-ssa";
	if (plen > 3 && p[0] == 'R' && p[1] == 'L' && p[2] == 'V'
	&& (p[3] == '2' || p[3] == '3'))
		return "video/x-rl2";

	/* See http://fileformats.archiveteam.org/wiki/ILBM */
	if (plen > 3 && ((p[0] == 'I' && p[1] == 'L' && p[2] == 'B' && p[3] == 'M')
	|| (p[0] == 'A' && p[1] == 'C' && p[2] == 'B' && p[3] == 'M')
	|| (p[0] == 'R' && p[1] == 'G' && p[2] == 'B' && p[3] == 'N')
	|| (p[0] == 'R' && p[1] == 'G' && p[2] == 'B' && p[3] == '8')))
		return "image/x-ilbm";
	if (plen > 2 && p[0] == 'P' && p[1] == 'B' && p[2] == 'M')
		return "image/x-ilbm";

	if (plen > 3 && p[0] == 'M' && p[1] == 'L' && p[2] == 'D' && p[3] == 'F')
		return "image/x-mldf";

	if (plen > 3 && p[0] == 'R' && p[1] == 'E' && p[2] == 'A' && p[3] == 'L')
		return "image/x-real3d";

	if (plen > 3 && p[0] == 'A' && p[1] == 'N' && p[2] == 'I' && p[3] == 'M')
		return "video/x-amiga-anim";

	/* https://wiki.amigaos.net/wiki/DEEP_IFF_Chunky_Pixel_Image */
	if (plen > 3 && ((p[0] == 'D' && p[1] == 'E' && p[2] == 'E' && p[3] == 'P')
	|| (p[0] == 'T' && p[1] == 'V' && p[2] == 'P' && p[3] == 'P')))
		return "image/x-amiga-deep";

	if (plen > 3 && p[0] == 'V' && p[1] == 'D' && p[2] == 'E' && p[3] == 'O')
		return "video/x-vdeo";

	if (plen > 3 && p[0] == 'V' && p[1] == 'A' && p[2] == 'X' && p[3] == 'L')
		return "video/x-vaxl";

	if (plen > 3 && p[0] == 'Y' && p[1] == 'A' && p[2] == 'F' && p[3] == 'A')
		return "video/x-yafa";

	/* FFmpeg: libavformat/westwood_vwa.c */
	if (plen > 3 && p[0] == 'W' && p[1] == 'V' && p[2] == 'Q' && p[3] == 'A')
		return "video/x-westwood-vqa";

	if (plen > 3 && p[0] == 'D' && p[1] == 'R' && p[2] == '2' && p[3] == 'D')
		return "image/x-dr2d";

	if (plen > 3 && p[0] == 'L' && p[1] == 'W') {
		if ((p[2] == 'O' && (p[3] == 'B' || p[3] == '2'))
		|| (p[2] == 'L' && p[3] == 'O'))
			return "image/x-lwo";
	}

	if (plen > 3 && p[0] == 'F' && p[1] == 'A' && p[2] == 'X' && p[3] == '3')
		return "image/x-gpfax";

	if (plen > 3 && p[0] == 'W' && p[1] == 'O' && p[2] == 'R' && p[3] == 'D')
		return "application/x-amiga-prowrite";

	if (plen > 3 && p[0] == 'C' && p[1] == 'T' && p[2] == 'L' && p[3] == 'G')
		return "application/x-amiga-catalog";

	if (plen > 3 && ((p[0] == 'W' && p[1] == 'O' && p[2] == 'W' && p[3] == 'O')
	|| (p[0] == 'W' && p[1] == 'T' && p[2] == 'X' && p[3] == 'T')))
		return "application/x-document.wordworth";

	if (plen > 7 && p[0] == 'F' && p[1] == 'R' && p[2] == 'A' && p[3] == 'Y'
	&& p[4] == 'M' && p[5] == 'A' && p[6] == 'T' && p[7] == '1')
		return "model/x-c4d";

	return NULL;
}

static const char *
check_movi_magic(const uint8_t *s, const size_t slen)
{
	if (slen > 3 && ((s[0] == 'm' && s[1] == 'o' && s[2] == 'o' && s[3] == 'v')
	|| (s[0] == 'm' && s[1] == 'd' && s[2] == 'a' && s[3] == 't')
	|| (s[0] == 'w' && s[1] == 'i' && s[2] == 'd' && s[3] == 'e')))
		return "video/quicktime";

	if (slen > 3 && s[0] == 'i' && s[1] == 'd' && s[2] == 's' && s[3] == 'c')
		return "image/x-quicktime";

	if (slen > 3 && s[0] == 'p' && s[1] == 'c' && s[2] == 'k' && s[3] == 'g')
		return "application/x-quicktime-player";

	return "video/x-sgi-movie";
}

static const char *
get_ms_exec_type(const uint8_t *s, const size_t slen)
{
	if (slen < 0x3C + 4) /* Need bytes 0x3C..0x3F */
		return "application/x-dosexec";

	/* Read little-endian dword (double word - 32bit) at 0x3C (e_lfanew pointer) */
	const uint32_t n = LE_U32(s + 0x3C);

	if (n + 4 > slen)
		return "application/x-dosexec";

	if (s[n] == 'P' && s[n + 1] == 'E' && s[n + 2] == 0x00 && s[n + 3] == 0x00)
		return "application/vnd.microsoft.portable-executable";

	if (s[n] == 'N' && s[n + 1] == 'E')
		return "application/x-ms-ne-executable";

	return "application/x-dosexec";
}

/* See http://fileformats.archiveteam.org/wiki/DWG */
static int
validate_dwg_version(const uint8_t *s, const size_t slen)
{
	/* S is either MC or AC */
	if (s[0] == 'M') /* MC0.0 */
		return (slen >= 5 && s[2] == '0' && s[3] == '.' && s[4] == '0');

	/* S is AC */
	if (slen < 6 || s[2] == '0' || s[2] >= '3') /* Only AC1/AC2 are valid */
		return 0;

	if (s[2] == '2') /* AC2.10 */
		return (s[3] == '.' && s[4] == '1' && s[5] == '0');

	if (s[3] == '.') /* AC1.20 AC1.40 AC1.50 */
		return ((s[4] == '2' || ((s[4] == '4' || s[4] == '5') && s[5] == '0')));

	if (s[3] != '0')
		return 0;

	if (s[4] == '0') /* AC1002 AC1003 AC1004 AC1006 AC1009 */
		return (s[5] == '1' || s[5] == '2' || s[5] == '3' || s[5] == '4'
			|| s[5] == '6' || s[5] == '9');

	if (s[4] == '1') /* AC1012 AC1013 AC1014 AC1015 AC1018 */
		return (s[5] == '2' || s[5] == '3' || s[5] == '4'
		|| s[5] == '5' || s[5] == '8');

	if (s[4] == '2') /* AC1021 AC1024 AC1027 */
		return (s[5] == '1' || s[5] == '4' || s[5] == '7');

	if (s[4] == '3')
		return (s[5] == '2' || s[5] == '5'); /* AC1032 AC1035 */

	return 0;
}

static const char *
check_key_magic(const uint8_t *s, const size_t slen)
{
	/* S is at least "-----BEGIN "  */
	if (slen < 12)
		return NULL;

	const uint8_t *p = s + 11;
	const size_t l = slen - 11;
	/* We're now right after "-----BEGIN " */

	if (p[0] == 'C') {
		if (l >= 8 && memcmp(p, "CERTIFICATE", 11) == 0
		&& (p[11] == ' ' || p[11] == '-'))
			return "application/x-pem-file";
	}

	if (p[0] == 'D') {
		if (l >= 11 && memcmp(p, "DSA PRIVATE", 11) == 0)
			return "text/x-pem-file";
	}

	if (p[0] == 'E') {
		if (l >= 13 && (memcmp(p, "EC PRIVATE", 10) == 0
		|| memcmp(p, "ECDSA PRIVATE", 13) == 0))
			return "text/x-pem-file";
	}

	if (p[0] == 'P') {
		if (l >= 22 && (memcmp(p, "PGP PRIVATE KEY BLOCK-", 22) == 0
		|| memcmp(p, "PGP PUBLIC KEY BLOCK-", 21) == 0))
			return "application/pgp-keys";
		if (l >= 19 && memcmp(p, "PGP SIGNED MESSAGE-", 19) == 0)
			return "text/PGP";
		if (l >= 14 && memcmp(p, "PGP SIGNATURE-", 14) == 0)
			return "application/pgp-signature";
		if (l >= 12 && memcmp(p, "PGP MESSAGE-", 12) == 0)
			return "application/pgp-encrypted";
	}

	if (p[0] == 'R') {
		if (l >= 16 && memcmp(p, "RSA PRIVATE KEY-", 16) == 0)
			return "text/x-ssl-private-key";
		if (l >= 15 && memcmp(p, "RSA PUBLIC KEY-", 15) == 0)
			return "text/x-ssl-public-key";
	}

	return NULL;
}

/* See elf(5) */
static const char *
check_elf_magic(const uint8_t *s, const size_t slen)
{
	if (slen <= 18 || s[17] != 0x00)
		return NULL;

	if (s[8] == 'A' && s[9] == 'I')
		return "application/vnd.appimage";

	switch (s[16]) {
	case 1:  return "application/x-object";
	case 2:  /* Fallthrough */
	case 3:  return "application/x-executable";
	case 4:  return "application/x-coredump";
	default: return "application/octet-stream";
	}
}

static const char *
check_pre_ole2_office_docs(const uint8_t *s, const size_t slen)
{
	if (slen > 128 && s[0] == 0x31 && s[1] == 0xBE && s[2] == 0x00
	&& s[3] == 0x00 && s[128] > 0) { /* Pre OLE2 Word (DOS Word 5) */
		const uint16_t b = LE_U16(s + 96);
		if (b != 0) return "application/mswrite";
		return "application/msword";
	}

	if (slen > 5 && s[0] == 0xDB && s[1] == 0xA5 && s[2] == '-'
	&& s[3] == 0x00 && s[4] == 0x00 && s[5] == 0x00)
		return "application/msword"; /* Word 2 (pre OLE2) */

	if (slen > 3 && s[0] == 0xDB && s[1] == 0xA5 && s[2] == 0x2D
	&& s[3] == 0x00)
		return "application/msword"; /* WinWord 2 (pre OLE2) */

	if (slen > 5 && s[0] == 'P' && s[1] == 'O' && s[2] == '^'
	&& s[3] == 'Q' && s[4] == '`' && s[5] <= 0x04)
		return "application/msword"; /* Word 6 (pre OLE2) */

	if (slen > 3 && s[0] == 0xFE && s[2] == 0x00) { /* Mac Word */
		if ((s[3] == 0x00 && (s[1] == 0x32 || s[1] == 0x34)) /* vers 1/3 */
		|| (s[1] == 0x37 && (s[3] == 0x1c || s[3] == 0x23))) /* vers 4/5 */
			return "application/msword";
	}

	/* Magic taken from libmagic */
	if (slen > 7 && s[0] == 0x09 && s[1] == 0x04 && s[2] == 0x06
	&& s[3] == 0x00 && s[4] == 0x00 && s[5] == 0x00 && s[6] == 0x10
	&& s[7] == 0x00) /* MS Excel Worksheet */
		return "application/vnd.ms-excel";

	if (slen >= 19 && s[4] == 'S' && s[7] == 'n'
	&& (memcmp(s + 4, "Standard Jet DB", 15) == 0
	|| memcmp(s + 4, "Standard ACE DB", 15) == 0))
		return "application/vnd.ms-access";

	if (slen > 4 && s[0] == 0xe7 && s[1] == 0x0ac && s[2] == 0x2c
	&& s[3] == 0x00) /* Microsoft Publisher (1.0) */
		return "application/vnd.ms-publisher";

	return NULL;
}

static const char *
check_gzipped_koffice(const uint8_t *s, const size_t slen)
{
	if (slen < 44 || s[30] != 'x' || s[31] != '-' || s[32] != 'k'
	|| memcmp(s + 10, "KOffice ", 8) != 0)
		return "application/gzip";

	const uint8_t *t = s + 18;
	if (t[15] == 'a' && memcmp(t, "application/x-karbon", 20) == 0)
		return "application/x-karbon";
	if (t[15] == 'c' && memcmp(t, "application/x-kchart", 20) == 0)
		return "application/x-kchart";
	if (t[15] == 'f' && memcmp(t, "application/x-kformula", 22) == 0)
		return "application/x-kformula";
	if (t[15] == 'i' && memcmp(t, "application/x-killustrator", 26) == 0)
		return "application/x-killustrator";
	if (t[15] == 'i' && memcmp(t, "application/x-kivio", 19) == 0)
		return "application/x-kivio";
	if (t[15] == 'o' && memcmp(t, "application/x-kontour", 21) == 0)
		return "application/x-kontour";
	if (t[15] == 'p' && memcmp(t, "application/x-kpresenter", 24) == 0)
		return "application/x-kpresenter";
	if (t[15] == 's' && memcmp(t, "application/x-kspread", 21) == 0)
		return "application/x-kspread";
	if (t[15] == 'w' && memcmp(t, "application/x-kword", 19) == 0)
		return "application/x-kword";

	return "application/gzip";
}

static const char *
check_cafebabe(const uint8_t *s, const size_t slen)
{
	if (slen < 8)
		return NULL;

	const uint32_t v = BE_U32(s + 4);

	if (v >= 1 && v < 20) return "application/x-mach-binary";
	if (v > 30) return "application/x-java-applet";

	return NULL;
}

static char *
fs_type_check(const struct stat *a)
{
	if (!a)
		return NULL;

	if (a->st_size == 0 && S_ISREG(a->st_mode))
		return "inode/x-empty";

	switch (a->st_mode & S_IFMT) {
	case S_IFDIR:  return "inode/directory";
	case S_IFLNK:  return "inode/symlink";
	case S_IFIFO:  return "inode/fifo";
	case S_IFSOCK: return "inode/socket";
	case S_IFBLK:  return "inode/blockdevice";
	case S_IFCHR:  return "inode/chardevice";
	/* Let non-standard file modes be handled by libmagic itself */
	default: break;
	}

	return NULL;
}

/* See http://fileformats.archiveteam.org/wiki/TGA */
static int
is_tga_image(const uint8_t *s, const size_t slen)
{
	if (slen < 18)
		return 0;

	/* Image type */
	const uint8_t it = s[2];
	if (it != 1 && it != 2 && it != 3 && it != 9 && it != 10
	&& it != 11 && it != 32 && it != 33)
		return 0;

	/* Pixel depth */
	const uint8_t pd = s[16];
	if (pd != 1 && pd != 8 && pd != 15 && pd != 16 && pd != 24 && pd != 32)
		return 0;

	if ((it == 1 || it == 9) && pd == 1)
		return 0; /* Indexed images cannot have a pixel depth of 1 */

	if ((it == 2 || it == 10) && pd < 15)
		return 0; /* TrueColor/RGB valid pixel depth: 15, 16, 24, 32 */

	if ((it == 3 || it == 11) && pd > 8)
		return 0; /* Grayscale/Monochrome valid pixel depth: 1 or 8 */

	const uint8_t ab = s[17]; /* Alpha bit */
	if ((pd == 32 && (ab & 0x0F) == 0)
	|| (pd == 24 && (ab & 0x0F) != 0))
		return 0;

	/* Color map type */
	const uint8_t cm = s[1];
	if (it == 1 || it == 9) {
		if (cm != 1) return 0; /* Indexed images color map must be 1 */
	} else {
		if (cm != 0) return 0; /* Non-indexed images color map must be 0 */
	}

	return 1;
}

static int
is_djvu(const uint8_t *s, const size_t slen)
{
	const size_t n = 12; /* "AT&TFORMxxxx" */

	if (slen < n + 4)
		return 0;

	/* DJVU, DJVM, DJVI, and THUM */
	if (s[n] == 'D' && s[n + 1] == 'J' && s[n + 2] == 'V'
	&& (s[n + 3] == 'U' || s[n + 3] == 'M' || s[n + 3] == 'I'))
		return 1;

	if (s[n] == 'T' && s[n + 1] == 'H' && s[n + 2] == 'U' && s[n + 3] == 'M')
		return 1;

	/* For these codecs see https://www.sndjvu.org/spec.html */
	if (s[n + 2] == '4' && s[n + 3] == '4') {
		if ((s[n] == 'B' && s[n + 1] == 'G') /* BG44 */
		|| (s[n] == 'F' && s[n + 1] == 'G') /* FG44 */
		|| (s[n] == 'P' && s[n + 1] == 'M') /* PM44 */
		|| (s[n] == 'B' && s[n + 1] == 'M') /* BM44 */
		|| (s[n] == 'T' && s[n + 1] == 'H')) /* TH44 */
			return 1;
	}

	return 0;
}

static int
is_imagemagick_mvg(const uint8_t *s, const size_t slen)
{
	/* S is "push " */
	if (slen < 5)
		return 0;
	const size_t max = slen > 10 ? 10 : slen;

	for (size_t i = 5; i < max && slen - i >= 15; i++) {
		if (s[i] == 'g' && memcmp(s + i, "graphic-context", 15) == 0)
			return 1;
	}

	return 0;
}

static int
is_bittorrent(const uint8_t *s, const size_t slen)
{
	if (s[0] != 'd')
		return 0;

	if ((slen >= 11 && s[1] == '8' && memcmp(s, "d8:announce", 11) == 0)
	|| (slen >= 17 && s[1] == '1' && memcmp(s, "d13:announce-list", 17) == 0)
	|| (slen >= 10 && s[1] == '7' && memcmp(s, "d7:comment", 10) == 0)
	|| (slen >= 7 && s[1] == '4' && memcmp(s, "d4:info", 7) == 0))
		return 1;

	return 0;
}

/* Return 1 if the string S, whose length in SLEN, has this format: "NUM NUM\n",
 * or 0 if not. */
static int
is_mtv_image(const uint8_t *s, const size_t slen)
{
	if (slen <= 7)
		return 0;

	size_t sp = 0;
	size_t nl = 0;
	for (size_t i = 0; i < 12; i++) {
		if (s[i] == ' ') sp = i;
		if (s[i] == 0x0A) nl = i;
	}

	if (sp == 0 || nl == 0 || nl < sp || !IS_DIGIT(s[sp - 1])
	|| !IS_DIGIT(s[nl - 1]))
		return 0;

	for (size_t i = 0; i < nl; i++) {
		if (i == sp)
			continue;
		if (!IS_DIGIT(s[i]))
			return 0;
	}

	return 1;
}

static int
is_sixel_image(const uint8_t *s, const size_t slen)
{
	/* Check for DCS in the first 32 bytes */
	const size_t max_dcs_offset = 32;
	/* 9 bytes for a complete sixel beginning sequence: "DCS n;n;n;q" */
	const size_t seq_max_len = 9;
	if (slen < max_dcs_offset + seq_max_len)
		return 0;

	size_t i;
	for (i = 0; i < max_dcs_offset; i++) {
		if (s[i] == 0x1B && s[i + 1] == 'P')
			break;
	}

	if (i == max_dcs_offset) /* No DCS found */
		return 0;

	const unsigned char *dcs = (const unsigned char *)s + i;

	size_t num = 0;
	size_t sc = 0;
	for (i = 2; i < seq_max_len && dcs[i] != 'q'; i++) {
		if (IS_DIGIT(dcs[i])) {
			if (dcs[i + 1] != 'q' && dcs[i + 1] != ';')
				return 0;
			num++;
		} else {
			if (dcs[i] == ';')
				sc++;
		}
	}

	if (dcs[i] != 'q' || sc > 3 || num > 3)
		return 0;

	return 1;
}

static int
check_vvc_temporal_id(const uint8_t nuh_temporal_id_plus1, const int type)
{
	if (nuh_temporal_id_plus1 == 0)
		return 0;

	if (nuh_temporal_id_plus1 != 1) {
		if ((type >= VVC_IDR_W_RADL && type <= VVC_RSV_IRAP_11)
		|| type == VVC_DCI_NUT || type == VVC_OPI_NUT
		|| type == VVC_VPS_NUT || type == VVC_SPS_NUT
		|| type == VVC_EOS_NUT || type == VVC_EOB_NUT)
			return 0;
	}

	return 1;
}

static void
check_vvc_nal2(const int type, size_t *sps, size_t *pps, size_t *irap,
	size_t *valid_pps, size_t *valid_irap)
{
	switch (type) {
	case VVC_SPS_NUT: (*sps)++; break;

	case VVC_PPS_NUT:
		(*pps)++;
		if (*sps)
			(*valid_pps)++;
		break;

	case VVC_IDR_N_LP:
	case VVC_IDR_W_RADL:
	case VVC_CRA_NUT:
	case VVC_GDR_NUT:
		(*irap)++;
		if (*valid_pps)
			(*valid_irap)++;
		break;

	default: break;
	}
}

static void
check_vc1_nal(const uint8_t type, const uint8_t i4, size_t *seq, size_t *frames,
	size_t *entry, size_t *invalid)
{
	switch (type) {
	case VC1_CODE_SEQHDR: {
		int profile = (i4 & 0xc0) >> 6;
		if (profile != VC1_PROFILE_ADVANCED) {
			*seq = 0;
			(*invalid)++;
			break;
		}
		int level = (i4 & 0x38) >> 3;
		if (level >= 5) {
			*seq = 0;
			(*invalid)++;
			break;
		}
		int chromaformat = (i4 & 0x6) >> 1;
		if (chromaformat != 1) {
			*seq = 0;
			(*invalid)++;
			break;
		}
		(*seq)++;
		break;
	}

	case VC1_CODE_ENTRYPOINT:
		if (!*seq) {
			(*invalid)++;
		}
		(*entry)++;
		break;

	case VC1_CODE_FRAME:
	case VC1_CODE_FIELD:
	case VC1_CODE_SLICE:
		if (*seq && *entry)
			(*frames)++;
		break;

	default: break;
	}
}

static void
check_hevc(const uint8_t unit, size_t *slice, size_t *vps_sps_pps)
{
	if (unit <= 31)
		(*slice)++;
	else if (unit >= 32 && unit <= 34)
		(*vps_sps_pps)++;
}

/* Heuristic single-pass scanner over a byte buffer (S, whose length is SLEN)
 * that looks for MPEG-style start-code prefix (0x000001) and attempts to
 * identify streams that use that framing: MPEG-1/2, HEVC/H.265, VC-1, and
 * VVC/H.266.
 * Returns a MIME type string for the detected format, or NULL if no match
 * is found. */
static const char *
detect_startcode_video_stream(const uint8_t *s, const size_t slen)
{
	return NULL; /* Disabled: too many false positives. */

	if (!s || slen < 4)
		return NULL;

	size_t vvc_sps = 0, vvc_pps = 0, vvc_irap = 0;
	size_t vvc_valid_pps = 0, vvc_valid_irap = 0;
	size_t vc1_seq = 0, vc1_entry = 0, vc1_invalid = 0, vc1_frames = 0;
	size_t hevc_slice = 0, hevc_vps_sps_pps = 0;
	size_t video_mpeg = 0, audio_mpeg = 0, video_mpeg4 = 0;

	for (size_t i = 0; i + 4 < slen; i++) {
		if (s[i] != 0x00 || s[i + 1] != 0x00 || s[i + 2] != 0x01)
			continue;

		const uint8_t nal = s[i + 3];
		const uint8_t nal2 = s[i + 4];

		/* MPEG */
		if (nal == 0xA5 || nal == 0xB6) { video_mpeg++; continue; }
		if (nal == 0xB3 || nal == 0xB8 || nal == 0xBA)
			{ video_mpeg++; continue; }
		if (nal >= 0xE0 && nal <= 0xEF) { video_mpeg++; continue; }
		if (nal >= 0xC0 && nal <= 0xDF) { audio_mpeg++; continue; }
		if (nal == 0xB0 || nal == 0xB5) { video_mpeg4++; continue; }

		/* VVC. See FFmpeg: libavformat/vvcdec.c */
		if (MSB_IS_ZERO(nal) && nal2 > 0x00) {
			const int type = (nal2 & 0xF8) >> 3;
			const int nuh_layer_id = (nal & 0x01) << 5 | type;
			const uint8_t nuh_temp_id_plus1 = nal2 & 0x7;
			if (nuh_layer_id <= 55
			&& check_vvc_temporal_id(nuh_temp_id_plus1, type)) {
				check_vvc_nal2(type, &vvc_sps, &vvc_pps, &vvc_irap,
					&vvc_valid_pps, &vvc_valid_irap);
				if (vvc_valid_irap) return "video/h266";
			}
		}

		/* VC-1. See FFmpeg: libavformat/vc1dec.c */
		if (IS_VC1_NAL(nal)) {
			check_vc1_nal(nal, nal2, &vc1_seq, &vc1_frames,
				&vc1_entry, &vc1_invalid);
			if (vc1_frames >= 1 && vc1_invalid == 0)
				return "video/x-vc1";
		}

		/* HEVC */
		const uint8_t hevc_unit = (nal >> 1) & 0x3F;
		if (hevc_unit <= 34 && MSB_IS_ZERO(nal)) {
			check_hevc(hevc_unit, &hevc_slice, &hevc_vps_sps_pps);
			if (hevc_vps_sps_pps > 2
			|| (hevc_vps_sps_pps > 1 && hevc_slice > 0))
				return "video/x-hevc";
		}

		i += 3;
	}

	if (video_mpeg > 1 || (video_mpeg == 1 && audio_mpeg >= 1))
		return "video/mpeg";
	if (audio_mpeg > 1) return "audio/mpeg";
	if (video_mpeg4 > 1) return "video/mpeg4-generic";

	return NULL;
}

/* See https://datatracker.ietf.org/doc/draft-ietf-opsawg-pcap/
 * and file(1) (magic/Magdir/sniffer) */
static int
is_pcap_file(const uint8_t *s, const size_t slen)
{
	if (slen < 4)
		return 0;

	const uint32_t v = s[0] == 0xA1 ? BE_U32(s) : LE_U32(s);
	if (v == 0xA1B2C3D4 || v == 0xA1B23C4D || v == 0xA1B2CD34)
		return 1;

	return 0;
}

/* https://dune-hd.com/support/misc/AAImageGen-README.txt */
static int
is_aai_image(const uint8_t *s, const size_t slen, const off_t file_size)
{
	const uint64_t aai_max_size = 2047 * 2047 * 4 + 8;
	if (!s || slen < 8 || (uint64_t)file_size > aai_max_size)
		return 0;

	const uint32_t width = LE_U32(s);
	const uint32_t height = LE_U32(s + 4);
	if (width == 0 || width > 2047 || height == 0 || height > 2047)
		return 0;

	const uint64_t pixels = (uint64_t)width * (uint64_t)height;
	const uint64_t expected_size = (pixels * 4) + 8;
	if (expected_size != (uint64_t)file_size)
		return 0;

	size_t i = 8; /* First pixel data is at offset 0x08 */
	while (i + 4 <= slen) {
		if (s[i + 3] == 255) /* Valid alpha 0-254 */
			return 0;
		i += 4;
	}

	return 1;
}

/* GSM-probe routine taken from FFmpeg (libavformat/gsmdec.c)
 * See https://wiki.multimedia.cx/index.php/GSM_06.10 */
static int
is_gsm_audio(const uint8_t *s, const size_t slen)
{
	/* We want at least 32 GSM block header bytes. Since each header has
	 * 33 byes, we need at least GSM_MIN_BYTES (1056) */
	if (slen < GSM_MIN_BYTES)
		return 0;

	int valid = 0, invalid = 0;
	const uint8_t *b = s;
	while (b < s + slen - 32) {
		if ((*b & 0xF0) == 0xD0) /* Valid values: 0xD0-0xDF */
			valid++;
		else
			invalid++;
		b += 33; /* GSM block size */
	}

	return (valid >> 5 > invalid); /* == (valid / 32) > invalid */
}

/* Implementation based on FFmpeg (libavformat/mvi.c). Haven't found any
 * public documentation about the MVI format. */
static int
is_mvi_video(const uint8_t *s, const size_t slen, const off_t file_size)
{
	const size_t header_size = 26;
	const size_t mvi_frac_bits = 10;
	const uint64_t fsize = (uint64_t)file_size;

	if (slen < header_size)
		return 0;

	const uint32_t frames_count = LE_U32(s + 3);
	const uint32_t msecs_per_frame = LE_U32(s + 7);
	const uint16_t width = LE_U16(s + 11);
	const uint16_t height = LE_U16(s + 13);
	const uint16_t sample_rate = LE_U16(s + 16);
	const uint32_t audio_size = LE_U32(s + 18);
	const uint32_t player_version = LE_U32(s + 23);
	const uint64_t audio_frame_size = frames_count > 0
		? ((uint64_t)audio_size << mvi_frac_bits) / frames_count : 0;

	/* All tested files are either 320x200 or 320x240.
	 * Also, all of them have a sample rate of 11025 or 22050. */
	if (frames_count > 0 && frames_count < 2000000
	&& msecs_per_frame > 0
	&& width == 320 && (height == 200 || height == 240)
	&& (sample_rate == 11025 || sample_rate == 22050)
	&& audio_size > 0 && (uint64_t)audio_size + header_size < fsize
	&& player_version <= 213 /* FFmpeg devs know why, I guess */
	&& audio_frame_size > ((1ULL << mvi_frac_bits) - 1))
		return 1;

	return 0;
}

/* Based on FFmpeg: libavformat/cdxl.c */
static int
is_cdxl_video(const uint8_t *s, const size_t slen)
{
	const uint32_t cdxl_header_size = 32;
	if (slen < cdxl_header_size)
		return 0;

	const uint8_t  plane_arrangement = (s[1] & 0x07);
	const uint8_t  video_encoding = (s[1] & 0xE0);
	const uint32_t chunk_size = BE_U32(s + 2);
	const uint16_t width = BE_U16(s + 14);
	const uint16_t height = BE_U16(s + 16);
	const uint16_t palette_size = BE_U16(s + 20);
	const uint16_t sound_size = BE_U16(s + 22);

	if (video_encoding > 3 || plane_arrangement > 6)
		return 0;

	if (width == 0 || width > 640 || height == 0 || height > 480)
		return 0;

	if (palette_size == 0 || (s[0] == 1 && palette_size > 512)
	|| (s[0] == 0 && palette_size > 768))
		return 0;

	if (chunk_size <= ((uint32_t)palette_size
	+ (uint32_t)sound_size * (1 + !!(s[1] & 0x10))
	+ cdxl_header_size))
		return 0;

	return 1;
}

/* See https://temlib.org/AtariForumWiki/index.php/Cyber_Paint_Sequence_file_format
 * and
 * https://wiki.preterhuman.net/Cyber_Paint_Sequence_Format_(.SEQ) */
static int
is_seq_video(const uint8_t *s, const size_t slen)
{
	if (slen < 256)
		return 0;

	const uint32_t frame_offset = BE_U32(s + 128);
	const uint32_t frame_length = 128;
	if (frame_offset > (uint32_t)slen - frame_length)
		return 0;

	const uint8_t *frame = s + frame_offset;

	const uint16_t resolution = BE_U16(frame + 2);
	const uint16_t X = BE_U16(frame + 54);
	const uint16_t Y = BE_U16(frame + 56);
	const uint16_t width = BE_U16(frame + 58);
	const uint16_t height = BE_U16(frame + 60);
	const uint8_t operation = frame[62];
	const uint8_t storage_method = frame[63];

	if (resolution != 0 || X > 319 || Y > 199 || operation > 1
	|| storage_method > 1 || width > 320 || height > 200)
		return 0;

	return 1;
}

static size_t
id3v2_tag_size(const uint8_t *buf, const size_t buflen, const off_t file_size)
{
	if (buflen < 10)
		return 0;

	const uint8_t ver = buf[3];       /* Version */
	const uint8_t id3_flags = buf[5]; /* Flags byte is at offset 5 */
	if (ver < 2 || ver > 4)           /* Accept v2.2..v2.4 */
		return 0;

	/* Bytes 6..9 are sync-safe size */
	const uint32_t b6 = buf[6], b7 = buf[7], b8 = buf[8], b9 = buf[9];
	/* Validate sync-safe (MSB must be zero) */
	if ((b6 & 0x80) || (b7 & 0x80) || (b8 & 0x80) || (b9 & 0x80))
		return 0; /* Malformed */

	uint32_t tag_payload = (b6 << 21) | (b7 << 14) | (b8 << 7) | b9; /* payload size */
	size_t total = (size_t)tag_payload + 10; /* header + payload */

	/* Footer for v2.4 adds 10 bytes */
	if (ver == 4 && (id3_flags & 0x10))
		total += 10;

	/* Sanity cap */
	const size_t MAX_TAG_BYTES = 1024 * 1024; /* 1 MiB */
	if (total > MAX_TAG_BYTES)
		return 0;

	if (file_size > 0 && total > (size_t)file_size)
		return 0;

	return total;
}

static const char *
check_modern_formats(const uint8_t *sig, const size_t nread,
	const off_t file_size)
{
	if (!sig || nread == 0)
		return NULL;

	if (nread > 2 && sig[0] == 0xFF && sig[1] == 0xD8 && sig[2] == 0xFF) {
		/* JPEG-LS files usually have 0xF7 in the 4th byte. However,
		 * 1. Conversion to JPG fails; 2. Most file type identification
		 * tools (file(1), exiftool, and the Shared MIME-info database)
		 * report plain image/jpeg for these files, or just fail (convert(1)).
		if (nread > 3 && sig[3] == 0xF7) return "image/jls"; */
		return "image/jpeg";
	}

	if (nread > 7 && sig[0] == 0x89 && sig[1] == 'P' && sig[2] == 'N'
	&& sig[3] == 'G' && sig[4] == 0x0D && sig[5] == 0x0A && sig[6] == 0x1A
	&& sig[7] == 0x0A) {
		/* If at offset 12 we find "CgBI" instead of "IHDR", we have an
		 * iOS optimized PNG. However, they both use the same MIME type, so
		 * there's no need to check. */
		if (nread > 40 && sig[37] == 'a' && sig[38] == 'c' && sig[39] == 'T'
		&& sig[40] == 'L')
			return "image/apng";
		return "image/png";
	}

	if (nread > 3 && sig[0] == '<' && sig[1] == 's' && sig[2] == 'v'
	&& sig[3] == 'g') /* SVG without XML declaration */
		return "image/svg+xml";

	/* At least Adobe Reader versions 3 through 6 did support this header:
	 * %!PS-Adobe-N.n PDF-M.m
	 * However, both file(1) and mimetype(1) take this as a postscript file. */
	 /* Malformed PDFs may have leading garbage bytes */
	if (nread > 4 && sig[0] == '%' && (sig[1] == 'P' || sig[1] == 'F')
	&& sig[2] == 'D' && sig[3] == 'F' && sig[4] == '-') {
		if (sig[1] == 'F') return "application/vnd.fdf";
		return "application/pdf";
	}

	if (nread > 2 && sig[0] == 0x1F && sig[1] == 0x8B && sig[2] == 0x08) {
		if (nread > 12 && sig[10] == 'K' && sig[11] == 'O' && sig[12] == 'f')
			return check_gzipped_koffice(sig, nread);
		return "application/gzip";
	}

	if (nread > 2 && sig[0] == 'B' && sig[1] == 'Z' && sig[2] == 'h')
		return "application/x-bzip2";

	if (nread > 3 && sig[0] == 0x28 && sig[1] == 0xB5 && sig[2] == 0x2F
	&& sig[3] == 0xFD)
		return "application/zstd";

	if (nread > 5 && sig[0] == 'G' && sig[1] == 'I' && sig[2] == 'F'
	&& sig[3] == '8' && (sig[4] == '7' || sig[4] == '9') && sig[5] == 'a')
		return "image/gif"; /* Either "GIF87a" or "GIF89a" */

	if (nread > 3 && sig[0] == 'f' && sig[1] == 'L' && sig[2] == 'a'
	&& sig[3] == 'C')
		return "audio/flac";

	if (nread >= 10 && sig[0] == '%' && memcmp(sig, "%!PS-Adobe", 10) == 0) {
		if (nread > 17 && sig[15] == 'E' && sig[16] == 'P' && sig[17] == 'S')
			return "image/x-eps";
		return "application/postscript";
	}

	if (nread > 4 && sig[0] == '<' && sig[1] == '?' && sig[2] == 'x'
	&& sig[3] == 'm' && sig[4] == 'l') /* Mostly SVG */
		return check_xml_magic(sig, nread);

	if (nread >= 13 && sig[0] == '<' && sig[1] == '!' && ((sig[2] == 'D'
	&& sig[3] == 'O' && sig[4] == 'C') || (sig[2] == 'd'
	&& sig[3] == 'o' && sig[4] == 'c'))) /* SVG and HTML */
		return check_doctype_magic(sig, nread);

	if (nread > 3 && sig[0] == 'P' && sig[1] == 'K' && sig[2] == 0x03
	&& sig[3] == 0x04) /* ZIP-based files (Mostly Office files) */
		return check_zip_magic(sig, nread);

	if (nread > 3 && sig[0] == 'R' && sig[1] == 'I' && sig[2] == 'F'
	&& (sig[3] == 'F' || sig[3] == 'X')) /* RIFF (mostly image/webp) */
		return check_riff_magic(sig, nread);
	if (nread > 3 && sig[0] == 'r' && sig[1] == 'i' && sig[2] == 'f'
	&& (sig[3] == 'f' || sig[3] == 'x')) /* RIFF (mostly image/webp) */
		return check_riff_magic(sig, nread);

	if (nread > 7 && sig[4] == 'f' && sig[5] == 't' && sig[6] == 'y'
	&& sig[7] == 'p')
		return check_ftyp_magic(sig + 8, nread - 8); /* ftyp (mostly mp4) */
	if (nread >= 20 && sig[16] == 'f' && memcmp(sig + 16, "ftyp", 4) == 0)
		return check_ftyp_magic(sig + 20, nread - 20); /* ftyp (mostly mp4) */

	if (nread > 3 && sig[0] == 'O' && sig[1] == 'g' && sig[2] == 'g'
	&& sig[3] == 'S') /* OGG */
		return check_ogg_magic(sig, nread);

	if (nread > 3 && sig[0] == 0x1A && sig[1] == 0x45 && sig[2] == 0xDF
	&& sig[3] == 0xA3) /* EBML ID: matroska and webm */
		return check_ebml_magic(sig, nread);

	if (nread > 5 && sig[0] == 0x37 && sig[1] == 0x7A && sig[2] == 0xBC
	&& sig[3] == 0xAF && sig[4] == 0x27 && sig[5] == 0x1C) /* 7-zip */
		return "application/x-7z-compressed";

	if (nread > 7 && sig[0] == 'A' && sig[1] == 'T' && sig[2] == '&'
	&& sig[3] == 'T' && sig[4] == 'F' && sig[5] == 'O' && sig[6] == 'R'
	&& sig[7] == 'M' && is_djvu(sig, nread) == 1)
		return "image/vnd.djvu";

	if (nread > 4 && sig[0] == '<' && sig[1] == '?' && sig[2] == 'p'
	&& sig[3] == 'h' && sig[4] == 'p')
		return "text/x-php";

	if (nread > 2 && sig[0] == 'P'
	&& (sig[2] == 0x09 || sig[2] == '\n' || sig[2] == 0x0D || sig[2] == '#'
	|| sig[2] == ' ')) {
		if (sig[1] == '1' || sig[1] == '4') return "image/x-portable-bitmap";
		if (sig[1] == '2' || sig[1] == '5') return "image/x-portable-graymap";
		if (sig[1] == '3' || sig[1] == '6') return "image/x-portable-pixmap";
	}

	if (nread > 2 && sig[0] == 'P' && sig[1] == '7' && sig[2] == '\n')
		return "image/x-portable-arbitrarymap";

	if (nread > 8 && sig[0] == '/' && sig[1] == '*' && sig[2] == ' '
	&& sig[3] == 'X' && sig[4] == 'P' && sig[5] == 'M' && sig[6] == ' '
	&& sig[7] == '*' && sig[8] == '/')
		return "image/x-xpixmap"; /* This check is lazy */

	if (nread > 23 && sig[20] == 'G' && sig[21] == 'I' && sig[22] == 'M'
	&& sig[23] == 'P')
		return "image/x-gimp-gbr";

	if (nread > 23 && sig[20] == 'G' && sig[21] == 'P' && sig[22] == 'A'
	&& sig[23] == 'T')
		return "image/x-gimp-pat";

	if (nread > 7 && sig[0] == 0x89 && sig[1] == 'H' && sig[2] == 'D'
	&& sig[3] == 'F' && sig[4] == 0x0D && sig[5] == 0x0a && sig[6] == 0x1A
	&& sig[7] == 0x0A)
		return "application/x-hdf5";

	if (nread > 3 && ((sig[0] == 'I' && sig[1] == 'I'
	&& (sig[2] == '+' || sig[2] == '*') && sig[3] == 0x00)
	|| (sig[0] == 'M' && sig[1] == 'M' && sig[2] == 0x00
	&& (sig[3] == '+' || sig[3] == '*')))) {
		if (nread > 11 && sig[8] == 'C' && sig[9] == 'R'
		&& ((sig[10] == '2' && sig[11] == 0x02) || sig[10] == 0x02))
			return "image/x-canon-cr2";
		return "image/tiff";
	}

	/* http://fileformats.archiveteam.org/wiki/DPX */
	if (nread > 3 && ((sig[0] == 'S' && sig[1] == 'D' && sig[2] == 'P'
	&& sig[3] == 'X') || (sig[0] == 'X' && sig[1] == 'P' && sig[2] == 'D'
	&& sig[3] == 'S')))
		return "image/x-dpx";

	if (nread > 3 && sig[0] == 0x76 && sig[1] == 0x2F && sig[2] == 0x31
	&& sig[3] == 0x01)
		return "image/x-exr";

	if (nread > 3 && sig[0] == 'B' && sig[1] == 'P' && sig[2] == 'G'
	&& sig[3] == 0xFB)
		return "image/bpg";

	if (nread > 8 && sig[0] == 'S' && sig[1] == 'I' && sig[2] == 'M'
	&& sig[3] == 'P' && sig[4] == 'L' && sig[5] == 'E' && sig[6] == ' '
	&& sig[7] == ' ' && sig[8] == '=')
		return "image/fits"; /* or application/fits (Shared MIME-info) */

	if (nread > 7 && sig[0] == 0x8B && sig[1] == 'J' && sig[2] == 'N'
	&& sig[3] == 'G' && sig[4] == 0x0d && sig[5] == 0x0a && sig[6] == 0x1a
	&& sig[7] == 0x0a)
		return "image/x-jng";

	if (nread >= 10 && sig[0] == '#' && sig[1] == '?'
	&& memcmp(sig + 2, "RADIANCE", 8) == 0)
		return "image/vnd.radiance";

	if (nread >= 8 && sig[0] == '8' && sig[1] == 'B' && sig[2] == 'P'
	&& sig[3] == 'S' && sig[4] == 0x00 && (sig[5] == 0x01 || sig[5] == 0x02))
		return "image/vnd.adobe.photoshop";

	if (nread >= 24 && sig[0] == 'i' && sig[1] == 'd' && sig[2] == '='
	&& memcmp(sig + 3, "ImageMagick", 11) == 0)
		return "image/x-miff";

	if (nread >= 19 && sig[0] == 'i' && sig[1] == 'd' && sig[2] == '='
	&& memcmp(sig + 3, "MagickPixelCache", 16) == 0)
		return "image/x-mpc";

	if (nread > 3 && ((sig[0] == 'I' && sig[1] == 'I' && sig[2] == 'R'
	&& (sig[3] == 'O' || sig[3] == 'S'))
	|| (sig[0] == 'M' && sig[1] == 'M' && sig[2] == 'O' && sig[3] == 'R')))
		return "image/x-olympus-orf";

	if (nread >= 15 && sig[0] == 'F' && sig[1] == 'U' && sig[2] == 'J'
	&& sig[3] == 'I' && memcmp(sig + 4, "FILMCCD-RAW", 11) == 0)
		return "image/x-fuji-raf";

	if (nread >= 4 && sig[0] == 0x80 && sig[1] == 0x2a && sig[2] == 0x5f
	&& sig[3] == 0xd7)
		return "image/x-kodak-cin";

	/* http://fileformats.archiveteam.org/wiki/Camera_Image_File_Format */
	if (nread >= 15 && sig[0] == 'I' && sig[1] == 'I' && sig[2] == 0x1A
	&& sig[3] == 0x00 && sig[4] == 0x00 && sig[5] == 0x00
	&& memcmp(sig + 6, "HEAPCCDR", 8) == 0)
		return "image/x-canon-crw";

	if (nread > 3 && sig[0] == 0x00 && sig[1] == 'M' && sig[2] == 'R'
	&& sig[3] == 'M')
		return "image/x-minolta-mrw";

	if (nread > 7 && sig[0] == 'I' && sig[1] == 'I' && sig[2] == 'U'
	&& sig[3] == 0x00 && sig[5] == 0x00 && sig[6] == 0x00 && sig[7] == 0x00) {
		if (sig[4] == 0x08) return "image/x-panasonic-rw";
		if (sig[4] == 0x18) return "image/x-panasonic-rw2";
	}

	if (nread >= 2 && ((sig[0] == 'A' && sig[1] == 'C')
	|| (sig[0] == 'M' && sig[1] == 'C'))
	&& validate_dwg_version(sig, nread) == 1)
		return "image/vnd.dwg";

	if (nread >= 8 && sig[0] == 0x8A && sig[1] == 'M' && sig[2] == 'N'
	&& sig[3] == 'G' && sig[4] == '\r' && sig[5] == '\n' && sig[6] == 0x1A
	&& sig[7] == '\n')
		return "video/x-mng";

	if (nread > 7 && sig[4] == 'R' && sig[5] == 'E' && sig[6] == 'D'
	&& sig[7] == '1')
		return "video/x-r3d";

	/* BINK video */
	if (nread > 4 && sig[0] == 'B' && sig[1] == 'I' && sig[2] == 'K') {
		if (sig[3] == 'b' || sig[3] == 'f' || sig[3] == 'g'
		|| sig[3] == 'h' || sig[3] == 'i')
			return "video/vnd.radgametools.bink";
	}
	if (nread > 4 && sig[0] == 'K' && sig[1] == 'B' && sig[2] == '2') {
		if (sig[3] == 'a' || sig[3] == 'd' || sig[3] == 'f' || sig[3] == 'g'
		|| sig[3] == 'h' || sig[3] == 'i' || sig[3] == 'j' || sig[3] == 'k')
			return "video/vnd.radgametools.bink";
	}
	if (nread > 3 && sig[0] == 'S' && sig[1] == 'M' && sig[2] == 'K'
	&& (sig[3] == '2' || sig[3] == '4'))
			return "video/vnd.radgametools.smacker";

	if (nread >= 132 && sig[128] == 'D' && sig[129] == 'I' && sig[130] == 'C'
	&& sig[131] == 'M')
		return "application/dicom";

	if (nread >= 5 && sig[0] == 0xFD && sig[1] == '7' && sig[2] == 'z'
	&& sig[3] == 'X' && sig[4] == 'Z' && sig[5] == 0x00)
		return "application/x-xz";

	if (nread >= 8 && sig[0] == 0xED && sig[1] == 0xAB && sig[2] == 0xEE
	&& sig[3] == 0xDB)
		return "application/x-rpm";

	if (nread > 6 && sig[0] == '!' && sig[1] == '<' && sig[2] == 'a'
	&& sig[3] == 'r' && sig[4] == 'c' && sig[5] == 'h' && sig[6] == '>') {
		if (nread >= 21 && sig[8] == 'd'
		&& memcmp(sig + 8, "debian-binary", 13) == 0)
			return "application/vnd.debian.binary-package";
		return "application/x-archive";
	}
	if (nread > 3 && sig[0] == '<' && sig[1] == 'a' && sig[2] == 'r'
	&& sig[3] == '>')
		return "application/x-archive";

	if (nread > 39 && sig[36] == 'a' && sig[37] == 'c' && sig[38] == 's'
	&& sig[39] == 'p')
		return "application/vnd.iccprofile";

	if (nread > 3 && sig[0] == 'd' && sig[1] >= '0' && sig[1] <= '9'
	&& (sig[2] == ':' || sig[3] == ':') && is_bittorrent(sig, nread) == 1)
		return "application/x-bittorrent";

	if (nread > 264 && sig[257] == 'u' && sig[258] == 's' && sig[259] == 't'
	&& sig[260] == 'a' && sig[261] == 'r'
	&& ((sig[262] == ' ' && sig[263] == ' ' && sig[264] == 0x00)
	|| (sig[262] == 0x00 && sig[263] == '0' && sig[264] == '0')))
		return "application/x-tar";

	if (nread > 5 && sig[0] == '0' && sig[1] == '7' && sig[2] == '0'
	&& sig[3] == '7' && sig[4] == '0' && (sig[5] == '1' || sig[5] == '2'
	|| sig[5] == '7'))
		return "application/x-cpio";

	if (nread > 4 && sig[0] == 'B' && sig[1] == 'Z' && sig[2] == '3'
	&& sig[3] == 'v' && sig[4] == '1')
		return "application/x-bzip3";

	if (nread > 3 && sig[0] == 0x04 && sig[1] == '"' && sig[2] == 'M'
	&& sig[3] == 0x18)
		return "application/x-lz4";

	if (nread > 3 && sig[0] == 'L' && sig[1] == 'R' && sig[2] == 'Z'
	&& sig[3] == 'I')
		return "application/x-lrzip";

	if (nread > 3 && sig[0] == 'L' && sig[1] == 'Z' && sig[2] == 'I'
	&& sig[3] == 'P')
		return "application/x-lzip";

	if (nread > 3 && sig[0] == 0x89 && sig[1] == 'L' && sig[2] == 'Z'
	&& sig[3] == 'O')
		return "application/x-lzop";

	if (nread > 10 && sig[0] == '-' && sig[1] == '-' && sig[2] == '-'
	&& sig[3] == '-' && sig[4] == '-' && sig[5] == 'B' && sig[6] == 'E'
	&& sig[7] == 'G' && sig[8] == 'I' && sig[9] == 'N' && sig[10] == ' ')
		return check_key_magic(sig, nread);

	if (nread > 3 && sig[0] == 0x00 && sig[1] == 0x00 && sig[2] == 0x01) {
		const uint8_t s3 = sig[3];
		if (s3 == 0xB3 || s3 == 0xB8 || s3 == 0xBA) return "video/mpeg";
		if (s3 >= 0xE0 && s3 <= 0xEF) return "video/mpeg"; /* MPEG PES video */
		if (s3 >= 0xC0 && s3 <= 0xDF) return "audio/mpeg"; /* MPEG PES audio */
		if (s3 == 0xB0 || s3 == 0xB5) return "video/mpeg4-generic";
	}

	if (nread > 772 &&
	((sig[0] == 0x47 && sig[188] == 0x47 && sig[376] == 0x47
	&& sig[564] == 0x47 && sig[752] == 0x47)
	|| (sig[4] == 0x47 && sig[196] == 0x47 && sig[388] == 0x47
	&& sig[580] == 0x47 && sig[772] == 0x47)))
		return "video/MP2T";

	if (nread >= 14 && sig[0] == 0x06
	&& memcmp(sig, "\x06\x0E\x2B\x34\x02\x05\x01\x01\x0D\x01\x02\x01\x01\x02", 14) == 0)
		return "application/mxf";

	if (nread >= 16 && sig[0] == 'S' && sig[1] == 'Q'
	&& memcmp(sig, "SQLite format 3\0", 16) == 0)
		return "application/vnd.sqlite3";

	if (nread > 5 && sig[0] == 0x93 && sig[1] == 'N' && sig[2] == 'U'
	&& sig[3] == 'M' && sig[4] == 'P' && sig[5] == 'Y')
		return "application/x-numpy-data";

	if (nread > 3 && sig[0] == 'A' && sig[1] == 'D' && sig[2] == 'I'
	&& sig[3] == 'F')
		return "audio/x-hx-aac-adif";
	/* Keep the next three checks in order to avoid conflicts. */
	if (nread > 3 && ((sig[0] == 0x7F && sig[1] == 0xFE && sig[2] == 0x80
	&& sig[3] == 0x01) || (sig[0] == 0x1F && sig[1] == 0xFF && sig[2] == 0xE8
	&& sig[3] == 0x00) || (sig[0] == 0x80 && sig[1] == 0x01 && sig[2] == 0x7F
	&& sig[3] == 0xFE) || (sig[0] == 0xE8 && sig[1] == 0x00 && sig[2] == 0x1F
	&& sig[3] == 0xFF)))
		return "audio/vnd.dts";
	const uint16_t word = nread >= 2 ? BE_U16(sig) : 0;
	if ((word & 0xFFF6) == 0xFFF0)
		return "audio/x-hx-aac-adts";
	const uint16_t wmasked = word & 0xFFFE;
	if (wmasked == 0xFFFA || wmasked == 0xFFFC || wmasked == 0xFFF2
	|| wmasked == 0xFFF4 || wmasked == 0xFFF6 || wmasked == 0xFFE2)
		return "audio/mpeg";

	if (nread > 4 && sig[0] == '#' && sig[1] == '!' && sig[2] == 'A'
	&& sig[3] == 'M' && sig[4] == 'R') {
		if (nread > 7 && sig[5] == '-' && sig[6] == 'W' && sig[7] == 'B')
			return "audio/AMR-WB";
		return "audio/AMR";
	}

	if (nread > 7 && sig[4] == 'W' && sig[5] == 0x90 && sig[6] == 'u'
	&& sig[7] == '6')
		return "audio/x-pn-audibleaudio";

	if (nread > 3 && sig[0] == 'X' && sig[1] == 'M' && sig[2] == 'F'
	&& sig[3] == '_')
		return "audio/x-xmf";

	if (nread > 3 && sig[0] == 't' && sig[1] == 'B' && sig[2] == 'a'
	&& sig[3] == 'K')
		return "audio/x-tak";

	if (nread > 2 && sig[0] == 'M' && sig[1] == 'O' && sig[2] == '3')
		return "audio/x-mo3";

	if (nread > 5 && sig[0] == 'P' && sig[1] == 'V' && sig[2] == 'F'
	&& sig[4] == 0x0A && IS_DIGIT(sig[3]) && IS_DIGIT(sig[5]))
		return "audio/x-pvf";

	if (nread > 12 && ((sig[0] == 0x00 && sig[1] == 0x01 && sig[2] == 0x00
	&& sig[3] == 0x00 && sig[4] < 48
	&& sig[8] != 0x07) /* Avoid conflict with .PI2 (Atari Degas) */
	|| (sig[0] == 't' && sig[1] == 't' && sig[2] == 'c' && sig[3] == 'f'
	&& sig[4] == 0x00))) /* TrueType collection */
		return "font/ttf";
	if (nread > 3 && sig[0] == 'O' && sig[1] == 'T' && sig[2] == 'T'
	&& sig[3] == 'O')
		return "font/otf";
	if (nread > 3 && sig[0] == 'w' && sig[1] == 'O' && sig[2] == 'F') {
		if (sig[3] == 'F') return "font/woff";
		if (sig[3] == '2') return "font/woff2";
	}

	if (nread > 67 && ((sig[60] == 'M' && sig[61] == 'O' && sig[62] == 'B'
	&& sig[63] == 'I')
	|| (sig[60] == 'B' && sig[61] == 'O' && sig[62] == 'O' && sig[63] == 'K'
	&& sig[64] == 'M' && sig[65] == 'O' && sig[66] == 'B' && sig[67] == 'I')))
		return "application/x-mobipocket-ebook";

	if (nread > 3 && sig[0] == 'w' && sig[1] == 'v' && sig[2] == 'p'
	&& sig[3] == 'k')
		return "audio/x-wavpack";

	if (nread > 1 && (BE_U16(sig) & 0xFFE0) == 0x56E0)
		return "audio/x-mp4a-latm";

	if (nread > 3 && sig[0] == 'i' && sig[1] == 'c' && sig[2] == 'n'
	&& sig[3] == 's') /* Mac OS X icon */
		return "image/x-icns";

	if (nread > 3 && sig[0] == 'C' && sig[1] == 'r'
	&& (sig[2] == '2' || sig[2] == '3') && sig[3] == '4')
		return "application/x-chrome-extension";

	if (nread > 7 && sig[0] == 'c' && sig[1] == 'o' && sig[2] == 'n'
	&& sig[3] == 'e' && sig[4] == 'c' && sig[5] == 't' && sig[6] == 'i'
	&& sig[7] == 'x')
		return "application/x-virtualbox-vhd";

	if (nread > 4 && sig[0] == 'Q' && sig[1] == 'F' && sig[2] == 'I'
	&& sig[3] == 0xFB && sig[4] == 0x00) /* .qcow2 */
		return "application/x-qemu-disk";

	if (nread > 3 && sig[0] == 'E' && sig[1] == 'R' && sig[2] == 0x02
	&& sig[3] == 0x00) /* .toast */
		return "application/x-apple-diskimage";

	if (nread > 3 && sig[0] == '7' && sig[1] == 'k' && sig[2] == 'S'
	&& sig[3] == 't')
		return "application/x-zpaq";

	if (nread > 3 && sig[0] == 'x' && sig[1] == 'a' && sig[2] == 'r'
	&& sig[3] == '!') /* MacOS eXtensible ARchiver */
		return "application/x-xar";

	/* See http://fileformats.archiveteam.org/wiki/JXL */
	if ((nread > 1 && sig[0] == 0xFF && sig[1] == 0x0A)
	|| (nread >= 12 && sig[0] == 0x00 && sig[1] == 0x00 && sig[2] == 0x00
	&& memcmp(sig, "\x00\x00\x00\x0cJXL\x20\x0d\x0a\x87\x0a", 12) == 0))
		return "image/jxl";

	if (nread > 3 && sig[0] == 'I' && sig[1] == 'I' && sig[2] == 0xBC
	&& sig[3] == 0x01)
		return "image/jxr";

	/* http://fileformats.archiveteam.org/wiki/PGF_(Progressive_Graphics_File) */
	if (nread > 3 && sig[0] == 'P' && sig[1] == 'G' && sig[2] == 'F'
	&& sig[3] < 0x0A) /* 3rd byte is version (currently 6) */
		return "image/x-pgf";

	if (nread >= 2 && (sig[0] & 0x0F) == 8 && (sig[0] & 0x80) == 0
	&& (sig[1] & 0x20) == 0 && BE_U16(sig) % 31 == 0)
		return "application/zlib";

	if (nread > 6 && sig[0] == 'B' && sig[1] == 'L' && sig[2] == 'E'
	&& sig[3] == 'N' && sig[4] == 'D' && sig[5] == 'E' && sig[6] == 'R')
		return "application/x-blender";

	if (nread > 3 && sig[0] == 0xCA && sig[1] == 0xFE) {
		if (sig[2] == 0xBA && sig[3] == 0xBE) return check_cafebabe(sig, nread);
		if (sig[2] == 0xD0 && sig[3] == 0x0D) return "application/x-java-pack200";
	}

	if (nread >= 518 && sig[514] == 'H' && sig[515] == 'd' && sig[516] == 'r'
	&& sig[517] == 'S')
		return "application/x-linux-kernel";

	if (nread > 18 && sig[0] == 0x7f && sig[1] == 'E' && sig[2] == 'L'
	&& sig[3] == 'F')
		return check_elf_magic(sig, nread);

	if (nread > 5 && sig[0] == 'M' && sig[1] == 'A' && sig[2] == 'T'
	&& sig[3] == 'L' && sig[4] == 'A' && sig[5] == 'B')
		return "application/x-matlab-data";

	if (nread >= 13 && sig[0] == 'g' && sig[1] == 'i' && sig[2] == 'm'
	&& sig[3] == 'p' && sig[4] == ' ' && memcmp(sig, "gimp xcf ", 9) == 0)
		return "image/x-xcf";

	if (nread > 3 && sig[0] == 0xC5 && sig[1] == 0xD0 && sig[2] == 0xD3
	&& sig[3] == 0xC6) /* Encapsulated postscript */
		return "image/x-eps";
	if (nread > 17 && sig[0] == '%' && sig[1] == '!' && sig[15] == 'E'
	&& sig[16] == 'P' && sig[17] == 'S')
		return "image/x-eps";
	if (nread > 18 && sig[0] == 0x04 && sig[1] == '%' && sig[2] == '!'
	&& sig[16] == 'E' && sig[17] == 'P' && sig[18] == 'S')
		return "image/x-eps";

	if (nread >= 12 && sig[0] == '*' && sig[1] == 'P' && sig[2] == 'P'
	&& sig[3] == 'D' && memcmp(sig, "*PPD-Adobe: ", 12) == 0)
		return "application/vnd.cups-ppd";

	if (nread > 8 && sig[0] == 'M' && sig[1] == 'M' && sig[6] == 0x02
	&& sig[8] == 0x0A)
		return "image/x-3ds";

	if (nread >= 4 && sig[0] == 'F' && sig[1] == 'O' && sig[2] == 'V'
	&& sig[3] == 'b')
		return "image/x-x3f"; /* image/x-sigma-x3f (Shared MIME-info) */

	if (nread > 4 && sig[0] == 'p' && sig[1] == 'u' && sig[2] == 's'
	&& sig[3] == 'h' && sig[4] == ' ' && is_imagemagick_mvg(sig, nread) == 1)
		return "image/x-mvg";

	if (nread >= 4 && sig[0] == 'q' && sig[1] == 'o' && sig[3] == 'f'
	&& sig[4] == 0x00) {
		if (sig[2] == 'i') return "image/x-qoi";
		if (sig[2] == 'a') return "audio/x-qoa";
		return NULL;
	}

	if (nread > 3 && sig[0] == 'h' && sig[1] == 's' && sig[2] == 'i'
	&& sig[3] == '1')
		return "image/x-hsi";

	if (nread > 3 && sig[0] == 'P' && sig[1] == 'D' && sig[2] == 'N'
	&& sig[3] == '3')
		return "image/x-paintnet";

	/* https://git.ffmpeg.org/gitweb/nut.git/blob_plain/HEAD:/docs/nut.txt */
	if (nread >= 25 && sig[0] == 'n' && sig[1] == 'u' && sig[2] == 't'
	&& sig[3] == '/' && memcmp(sig, "nut/multimedia container\0", 25) == 0)
		return "video/x-nut";

	if (nread >= 12 && sig[0] == 'D' && sig[1] == 'V' && sig[2] == 'D'
	&& (memcmp(sig + 3, "VIDEO-VMG", 9) == 0
	|| memcmp(sig + 3, "VIDEO-VTS", 9) == 0))
		return "video/x-ifo";

	if (nread >= 10 && sig[0] == 'Y' && sig[3] == '4' && sig[4] == 'M'
	&& memcmp(sig, "YUV4MPEG2 ", 10) == 0)
		return "video/x-y4m";

	if (nread >= 16 && sig[0] == 0xB7 && sig[1] == 0xD8 && sig[8] == 0xA6
	&& memcmp(sig, "\xB7\xd8\x00\x20\x37\x49\xda\x11\xa6\x4e\x00\x07\xe9\x5e\xad\x8d", 16) == 0)
		return "video/vnd.ms-wtv";

	/* http://www.amnoid.de/gc/thp.txt */
	if (nread > 3 && sig[0] == 'T' && sig[1] == 'H' && sig[2] == 'P'
	&& sig[3] == 0x00)
		return "video/x-thp";

	/* https://web.archive.org/web/20150503015104im_/http://diracvideo.org/download/specification/dirac-spec-latest.pdf */
	if (nread > 3 && sig[0] == 'B' && sig[1] == 'B' && sig[2] == 'C'
	&& sig[3] == 'D')
		return "video/x-dirac";

	/* https://wiki.multimedia.cx/index.php/Vividas_VIV */
	if (nread > 8 && sig[0] == 'v' && sig[1] == 'i' && sig[2] == 'v'
	&& sig[3] == 'i' && sig[4] == 'd' && sig[5] == 'a' && sig[6] == 's'
	&& sig[7] == '0' && sig[8] == '3')
		return "video/x-vividas";

	if (nread > 4 && sig[0] == '#' && sig[1] == 'E' && sig[2] == 'X'
	&& sig[3] == 'T' && sig[4] == 'M' && sig[6] == 'U') {
		if (sig[5] == '3') return "audio/x-mpegurl";
		if (sig[5] == '4') return "video/vnd.mpegurl";
	}

	/* http://fileformats.archiveteam.org/wiki/Codec2 */
	if (nread > 4 && sig[0] == 0xC0 && sig[1] == 0xDE && sig[2] == 0xC2
	&& sig[3] <= 0x10 && sig[4] <= 0x10	/* Bytes 3-4: version (curren 1.2 (0x01 0x02)) */
	&& (sig[3] > 0x00 || sig[4] >= 0x08)) /* Minimum version 0.8 */
		return "audio/x-codec2";

	if (nread > 10 && sig[0] == '[' && sig[9] == ']') {
		if ((sig[1] == 'p' || sig[1] == 'P') && memcmp(sig + 2, "laylist", 7) == 0)
			return "audio/x-scpls";
		if (sig[1] == 'P' && sig[2] == 'L' && memcmp(sig + 1, "PLAYLIST", 8) == 0)
			return "audio/x-scpls";
	}

	if (nread >= 4 && sig[0] == 'P' && sig[1] == 'S' && sig[2] == 'F') {
		if (sig[3] == 0x01 || sig[3] == 0x02 || sig[3] == 0x11
		|| sig[3] == 0x12 || sig[3] == 0x13 || sig[3] == 0x21
		|| sig[3] == 0x22 || sig[3] == 0x23 || sig[3] == 0x41)
			return "audio/x-psf"; /* Niche, but actively used */
	}

	if (nread >= 4 && sig[0] == 'M' && sig[1] == 'T' && sig[2] == 'h'
	&& sig[3] == 'd')
		return "audio/midi";

	if (nread >= 4 && sig[0] == 'M' && sig[1] == 'A' && sig[2] == 'C'
	&& sig[3] == ' ')
		return "audio/x-ape";

	if (nread >= 4 && sig[0] == 0xFF && sig[1] == 0x4f && sig[2] == 0xFF
	&& sig[3] == 0x51)
		return "image/x-jp2-codestream";

	/* See http://justsolve.archiveteam.org/wiki/PGX_(JPEG_2000) */
	if (nread >= 5 && sig[0] == 'P' && sig[1] == 'G' && sig[2] == ' '
	&& ((sig[3] == 'L' && sig[4] == 'M') || (sig[3] == 'M' && sig[4] == 'L')))
		return "image/x-pgx";

	if (nread > 7 && sig[0] == 'f' && sig[1] == 'a' && sig[2] == 'r'
	&& sig[3] == 'b' && sig[4] == 'f' && sig[5] == 'e' && sig[6] == 'l'
	&& sig[7] == 'd')
		return "image/x-farbfeld";

	if (nread > 16 && BE_U32(sig + 4) == 7 && BE_U32(sig) > 99
	&& BE_U32(sig + 8) < 3 && BE_U32(sig + 12) < 33)
		return "image/x-xwindowdump";
	if (nread > 36 && BE_U32(sig) == 0x000000028 && BE_U32(sig + 4) == 6)
		return "image/x-xwindowdump";

	if (nread >= 8 &&
	((sig[0] == 'R' && sig[1] == 'a' && sig[2] == 'r'
	&& sig[3] == '!' && sig[4] == 0x1A && sig[5] == 0x07
	&& (sig[6] == 0x00 || (sig[6] == 0x01 && sig[7] == 0x00)))
		/* pre 1.50 */
	|| (sig[0] == 'R' && sig[1] == 'E' && sig[2] == 0x7E && sig[3] == 0x5E)))
		return "application/vnd.rar";

	if (nread > 7 && sig[4] == 'M' && sig[5] == 0x00 && sig[6] == 0x00
	&& sig[7] == 0x00
	&& ((sig[0] == 'M' && sig[1] == 'S' && sig[2] == 'W' && sig[3] == 'I')
	|| (sig[0] == 'W' && sig[1] == 'L' && sig[2] == 'P' && sig[3] == 'W')))
		return "application/x-ms-wim";

	if (nread > 3 && sig[0] == 0x3D && sig[1] == 0xF3 && sig[2] == 0x13
	&& sig[3] == 0x14) /* SYSLINUX boot logo files */
		return "image/x-lss16";

	if (nread >= 12 && sig[0] == 0xAB && sig[1] == 'K' && sig[2] == 'T'
	&& sig[3] == 'X') {
		if (sig[5] == '1' && memcmp(sig, "\xabKTX 11\xbb\x0d\x0a\x1a\x0a", 12) == 0)
			return "image/ktx";
		if (sig[5] == '2' && memcmp(sig, "\xabKTX 20\xbb\x0d\x0a\x1a\x0a", 12) == 0)
			return "image/ktx2";
	}

	if (nread > 24 && sig[4] == 0xE0 && sig[5] == 0xA5 && !sig[20] && !sig[24])
		return "image/x-aseprite";

	if (nread > 4 && sig[0] == 0x13 && sig[1] == 0xAB && sig[2] == 0xA1
	&& sig[3] == 0x5C && sig[4] == 0x06)
		return "image/x-astc"; /* MIME-info: image/astc (but it's not registered)*/

	if (nread > 3 && sig[0] == 'X' && sig[1] == 'c' && sig[2] == 'u'
	&& sig[3] == 'r')
		return "image/x-xcursor";

	if (nread > 3 && sig[0] == 'S' && ((sig[1] == 'I' && sig[2] == 'T'
	&& sig[3] == '!') || (nread > 6 && sig[1] == 't' && sig[2] == 'u'
	&& sig[3] == 'f' && sig[4] == 'f' && sig[5] == 'I' && sig[6] == 't')))
		return "application/x-stuffit";

	if (nread > 3 && sig[0] == 0x6E && sig[1] == 0xC3 && sig[2] == 0xAF) {
		if (sig[3] == 'E') return "image/x-nie";
		if (sig[3] == 'I') return "image/x-nii";
		if (sig[3] == 'A') return "image/x-nia";
	}

	if (nread > 3 && sig[0] == 'C' && sig[1] == 'r'  && sig[2] == '2'
	&& sig[3] == '4')
		return "application/x-chrome-extension";

	if (nread >= 14 && (LE_U32(sig) & 0x00FFFFFF) == 0x5D) {
		const uint16_t v12 = LE_U16(sig + 12);
		if (v12 == 0 || v12 == 0xFF)
			return "application/x-lzma";
	}

	/* https://tinyvg.tech/download/specification.pdf */
	if (nread > 2 && sig[0] == 'r' && sig[1] == 'V' && sig[2] == 0x01)
		return "image/x-tinyvg";

	if (nread > 9 && sig[0] == 'C' && sig[3] == 'Z' && sig[9] == ' '
	&& memcmp(sig, "CntZImage ", 10) == 0)
		return "image/x-lerc1";
	if (nread > 5 && sig[0] == 'L' && sig[1] == 'e' && sig[2] == 'r'
	&& sig[3] == 'c' && sig[4] == '2' && sig[5] == ' ')
		return "image/x-lerc2";

	if (nread > 3 && sig[0] == 'M' && sig[1] == 'R' && sig[2] == 'F'
	&& sig[3] == '1')
		return "image/x-mrf";

	if (nread > 3 && sig[0] == 'P' && (sig[1] == 'H' || sig[1] == 'h')
	&& sig[2] == 0x0A)
		return "image/x-phm";

	if (nread > 3 && sig[0] == 'F' && sig[1] == 'L' && sig[2] == '3'
	&& sig[3] == '2')
		return "image/x-fl32";

	if (nread > 8 && LE_U32(sig) <= 2047 && LE_U32(sig + 4) <= 2047
	&& is_aai_image(sig, nread, file_size) == 1)
		return "image/x-aai";

	if (nread > 2 && ((sig[0] == '%' && sig[1] == '!')
	|| (sig[0] == 0x04 && sig[1] == '%' && sig[2] == '!')))
		return "application/postscript";

	if (nread > 32 && is_sixel_image(sig, nread) == 1)
		return "image/x-sixel";

	/* http://fileformats.archiveteam.org/wiki/PIK */
	if (nread > 4 && sig[0] == 0xD7 && sig[1] == 0x4C && sig[2] == 0x4D
	&& sig[3] == 0x0A)
		return "image/x-pik";

	/* http://fileformats.archiveteam.org/wiki/Xar_(vector_graphics) */
	if (nread > 7 && sig[0] == 'X' && sig[1] == 'A' && sig[2] == 'R'
	&& sig[3] == 'A' && sig[4] == 0xA3 && sig[5] == 0xA3
	&& sig[6] == 0x0D && sig[7] == 0x0A)
		return "image/vnd.xara";

	if (nread > 4 && sig[0] == 0x00 && sig[1] == 0x00 && sig[2] == 0x00
	&& sig[3] == 0x01 && (sig[4] & 0x80) == 0 && (sig[4] & 0x1F) == 7)
		return "video/h264";
	if (nread > 3 && sig[0] == 0x00 && sig[1] == 0x00 && sig[2] == 0x01
	&& (sig[3] & 0x80) == 0 && (sig[3] & 0x1F) == 7)
		return "video/h264";

	/* https://multimedia.cx/mirror/av_format_v1.pdf */
	if (nread > 3 && sig[0] == 'A' && sig[1] == 'V'
	&& (sig[2] == 0x01 || sig[2] == 0x02) && sig[4] == 0x55
	&& (sig[5] & 0xE0) == 0x00)
		return "video/x-pva";

	if (nread > 3 && sig[0] == 0xF5 && sig[1] == 0x46 && sig[2] == 0x7A
	&& sig[3] == 0xBD)
		return "video/x-tivo";

	if (nread > 3 && (sig[0] == 0xA1 || sig[0] == 0xD4 || sig[0] == 0x4D
	|| sig[0] == 0x34) && is_pcap_file(sig, nread) == 1)
		return "application/vnd.tcpdump.pcap";

	/* file(1): magic/Magdir/sniffer */
	if (nread > 12 && ((sig[8] == 0x1A && BE_U32(sig + 8) == 0x1A2B3C4D)
	|| (sig[8] == 0x4D && LE_U32(sig + 8) == 0x1A2B3C4D)))
		return "application/x-pcapng";

	/* http://fileformats.archiveteam.org/wiki/Vim_swap_file */
	if (nread > 5 && sig[0] == 'b' && sig[1] == '0' && sig[2] == 'V'
	&& sig[3] == 'I' && sig[4] == 'M' && sig[5] == ' ')
		return "application/x-vim-swap";

	/* http://fileformats.archiveteam.org/wiki/Property_List/Binary */
	if (nread > 7 && sig[0] == 'b' && sig[1] == 'p' && sig[2] == 'l'
	&& sig[3] == 'i' && sig[4] == 's' && sig[5] == 't' && sig[6] == '0'
	&& (sig[7] == '0' || sig[1] == '1')) {
		if (nread > 24
		&& xmemmem(sig + 8, nread > 512 ? 512 : nread, "WebMainResource", 15))
			return "application/x-webarchive";
		return "application/x-bplist";
	}

	if (nread > 3 && sig[0] == '.' && (IS_ALPHA_UP(sig[1]) || IS_ALPHA_LOW(sig[1]))
	&& IS_ALNUM(sig[2]) && (sig[3] == ' ' || sig[3] == '\t'))
		return "text/troff";
	if (nread > 3 && ((sig[0] == '.' && sig[1] == '\\' && sig[2] == '"')
	|| (sig[0] == '\'' && sig[1] == '\\' && sig[2] == '"')
	|| (sig[0] == '\'' && sig[1] == '.' && sig[2] == '\\' && sig[3] == '"')
	|| (sig[0] == '\\' && sig[1] == '"')))
		return "text/troff";

	if (nread > 6 && sig[0] == 'W' && sig[1] == 'E' && sig[2] == 'B'
	&& sig[3] == 'V' && sig[4] == 'T' && sig[5] == 'T'
	&& (sig[6] == '\n' || sig[6] == '\t' || sig[6] == ' ' || sig[6] == '\r'))
		return "text/vtt";

	if (nread > 5 && sig[0] == '#' && sig[1] == 'V' && sig[2] == 'R'
	&& sig[3] == 'M' && sig[4] == 'L' && sig[5] == ' ')
		return "model/vrml";

	if (nread > 3 && sig[0] == '#' && sig[1] == 'X' && sig[2] == '3'
	&& sig[3] == 'D')
		return "model/x3d+vrml";

	if (nread > 3 && sig[0] == 'S' && sig[1] == 'T' && sig[2] == 'L'
	&& sig[3] == ' ')
		return "model/stl";
	if (nread > 4 && sig[0] == 's' && sig[1] == 'o' && sig[2] == 'l'
	&& sig[3] == 'i' && sig[4] == 'd')
		return "model/stl";
	if (nread > 4 && sig[0] == 'S' && sig[1] == 'O' && sig[2] == 'L'
	&& sig[3] == 'I' && sig[4] == 'D')
		return "model/stl";

	if (nread > 6 && sig[0] == 'n' && sig[1] == 'e' && sig[2] == 'w'
	&& sig[3] == 'm' && sig[4] == 't' && sig[5] == 'l' && sig[6] == ' ')
		return "model/mtl";
	if (nread >= 21 && sig[0] == '#' && sig[1] == ' ' && sig[2] == 'B'
	&& memcmp(sig, "# Blender MTL File: '", 21) == 0)
		return "model/mtl";

	/* https://www.iana.org/assignments/media-types/model/u3d */
	if (nread > 3 && sig[0] == 'U' && sig[1] == '3' && sig[2] == 'D'
	&& sig[3] == 0x00)
		return "model/u3d";

	if (nread > 3 && sig[0] == 'p' && sig[1] == 'l' && sig[2] == 'y'
	&& sig[3] == 0x0A)
		return "model/x-ply";

	if (nread > 5 && sig[0] == '(' && sig[1] == 'D' && sig[2] == 'W'
	&& sig[3] == 'F' && sig[4] == ' ' && sig[5] == 'V')
		return "model/vnd.dwf";

	/* http://fileformats.archiveteam.org/wiki/Ray_Dream (legacy) */
	if (nread > 4 && sig[0] == '3' && sig[1] == 'D' && sig[2] == 'C'
	&& sig[3] == ' ' && sig[4] == '{')
		return "model/x-3dc";

	if (nread > 4 && sig[0] == '3' && sig[1] == 'D' && sig[2] == 'M'
	&& sig[3] == 'F' && sig[4] == 0x00)
		return "model/x-3dmf"; // legacy

	if (nread >= 21 && sig[8] == 'F' && sig[9] == 'B' && sig[10] == 'X'
	&& memcmp(sig, "Kaydara FBX Binary  \0", 21) == 0)
		return "model/x-kaydara-fbx";

	/* https://paulbourke.net/dataformats/ac3d/ */
	if (nread > 32 && sig[0] == 'A' && sig[1] == 'C' && sig[2] == '3'
	&& sig[3] == 'D' && xmemmem(sig + 4, 32, "MATERIAL", 8))
		return "model/x-ac3d";

	if (nread > 4 && sig[0] == 'g' && sig[1] == 'l' && sig[2] == 'T'
	&& sig[3] == 'F')
		return "model/gltf-binary";

	if (nread > 6 && sig[1] == 'C' && sig[2] == '4' && sig[3] == 'D'
	&& sig[4] == 'C' && sig[5] == '4' && sig[6] == 'D')
		return "model/x-c4d";

	/* http://fileformats.archiveteam.org/wiki/Maya_scene */
	if (nread >= 12 && sig[0] == '/' && sig[1] == '/' && sig[6] == ' '
	&& memcmp(sig, "//Maya ASCII", 12) == 0)
		return "model/x-maya";
	if (nread >= 16 && sig[0] == 'F' && sig[1] == 'O' && sig[2] == 'R'
	&& ((sig[3] == '4' && sig[8] == 'M') || (sig[3] == '8' && sig[16] == 'M')))
		return "model/x-maya";

	if (nread >= 32 && sig[0] == 0xFF && sig[1] == 0xFE && sig[4] == 'S'
	&& memcmp(sig, "\xff\xfe\xff\x0e\x53\x00\x6b\x00\x65\x00\x74\x00\x63\x00\x68\x00\x55\x00\x70\x00\x20\x00\x4d\x00\x6f\x00\x64\x00\x65\x00\x6c\x00", 32) == 0)
		return "application/vnd.sketchup.skp";
	if (nread >= 15 && sig[0] == 0x0E && sig[1] == 'S' && sig[7] == 'U'
	&& memcmp(sig + 1, "SketchUp Model", 14) == 0)
		return "application/vnd.sketchup.skp";

	return NULL;
}

/* https://mooncore.eu//bunny/txt/pi-pic.htm */
static int
is_pi_image(const uint8_t *s, const size_t slen)
{
	if (!s)
		return 0;

	const uint8_t *end = s + slen;
	const uint8_t *p = memchr(s + 2, 0x1a, 512);
	if (!p || p + 1 > end)
		return 0;

	const uint8_t *q = memchr(p + 1, 0x00, 256);
	if (!q || q + 11 > end)
		return 0;

	const uint8_t mode_byte = q[1];
	if (mode_byte != 0) /* Usually zero, but might be non-zero */
		return 0;

	const uint8_t p_ratio_x = q[2];
	const uint8_t p_ratio_y = q[3];
	if (p_ratio_x > 16 || p_ratio_y > 16)
		return 0;

	const uint8_t bitdepth =  q[4];
	if (bitdepth != 1 && bitdepth != 2 && bitdepth != 4 && bitdepth != 8
	&& bitdepth != 16 && bitdepth != 24 && bitdepth != 32)
		return 0;

	const uint16_t comp_data_size = BE_U16(q + 9);
	if (q + 11 + comp_data_size + 4 > end)
		return 0;

	q += 11 + comp_data_size;

	const uint16_t img_px_width = BE_U16(q);
	const uint16_t img_px_height = BE_U16(q + 2);

	if (img_px_height > 2048 || img_px_width > 2048)
		return 0;

	return 1;
}

/* https://temlib.org/AtariForumWiki/index.php/IMG_file */
static int
is_gem_image(const uint8_t *s, const size_t slen)
{
	if (slen < 16)
		return 0;

	const uint16_t num_planes = BE_U16(s + 4);
	const uint16_t pat_def_len = BE_U16(s + 6);
	const uint16_t microns_width = BE_U16(s + 8);
	const uint16_t microns_height = BE_U16(s + 10);
	const uint16_t img_width = BE_U16(s + 12);
	const uint16_t img_height = BE_U16(s + 14);

	if (num_planes != 1 && num_planes != 2 && num_planes != 4
	&& num_planes != 8)
		return 0;

	if (pat_def_len != 1 && pat_def_len != 3 && pat_def_len % 2 != 0)
		return 0;

	if (microns_width > 1024 || microns_height > 1024)
		return 0;

	if (img_width > 2048 || img_height > 4096)
		return 0;

	return 1;
}

static int
is_tinystuff_image(const uint8_t *s, const size_t slen, const off_t file_size)
{
	if (!s || slen < 42 || file_size > 32044)
		return 0;

	const uint8_t resolution = s[0];
	if (resolution > 3)
		return 0;

	const uint8_t *h = s + (resolution < 3 ? 1 : 5) + 32; /* Skip palette (16*2) */
	const size_t header_size = resolution < 3 ? 37 : 41;

	const uint16_t control_bytes = BE_U16(h);
	const uint16_t data_words    = BE_U16(h + 2);

	if (control_bytes < 3 || control_bytes > 10667
	|| data_words < 1 || data_words > 16000)
		return 0;

	const size_t expected_size =
		header_size + (size_t)control_bytes + ((size_t)data_words * 2);

	if (expected_size != (size_t)file_size)
		return 0;

	return 1;
}

static int
is_id_cin_video(const uint8_t *s, const size_t slen)
{
	if (!s || slen < 18)
		return 0;

	const uint32_t sample_rate = LE_U32(s + 8);
	const uint32_t sample_width = LE_U32(s + 12);
	const uint32_t channels = LE_U32(s + 16);
	if ((sample_rate == 22050 || sample_rate == 11050)
	&& (sample_width == 1 || sample_width == 2)
	&& (channels == 1 || channels == 2))
		return 1;

	return 0;
}

/* https://temlib.org/AtariForumWiki/index.php/Cyber_Paint_Cell_file_format */
static int
is_cel_image(const uint8_t *s, const size_t slen)
{
	if (!s || slen < 128)
		return 0;

	uint16_t x_offset = BE_U16(s + 54); /* 0-319 */
	uint16_t y_offset = BE_U16(s + 56); /* 0-199 */
	uint16_t width = BE_U16(s + 58); /* Max: 320 */
	uint16_t height = BE_U16(s + 60); /* Max: 200 */
	uint8_t operation = s[62]; /* Always 0 */
	uint8_t storage_method = s[63]; /* Always 0 */

	if (x_offset > 319 || y_offset > 199 || width == 0 || width > 320
	|| height == 0 || height > 200 || operation != 0 || storage_method != 0)
		return 0;

	return 1;
}

/* Atari XEX files begin with a 0xFFFF magic number followed by start and
 * end addresses (little-endian). This function parses those addresses and
 * returns the size of the data segment: (end - start + 1).
 * See https://www.vitoco.cl/atari/xex-filter/index.html */
static size_t
get_atari_exec_data_len(const uint8_t *s, const size_t slen)
{
	if (!s || slen < 6 || s[0] != 0xFF || s[1] != 0xFF)
		return 0;

	const uint16_t start = LE_U16(s + 2);
	const uint16_t end = LE_U16(s + 4);
	if (end < start)
		return 0;

	const size_t data_len = (size_t)(end - start + 1);
	return data_len;
}

/* Detection routine based on RECOIL (recoil.c:RECOIL_DecodeHip) */
static int
is_atari_hip_image(const uint8_t *s, const size_t slen, const off_t file_size)
{
	if (!s || slen < 80)
		return 0;

	const size_t header_len = 6;
	const size_t frame1_len = get_atari_exec_data_len(s, slen);

	if (slen < frame1_len + header_len + 4)
		return 0;

	if (frame1_len == 0 || frame1_len % 40 != 0
	|| 12 + frame1_len * 2 != (size_t)file_size)
		return 0;

	const size_t frame2_offset = frame1_len + header_len;
	const size_t rem = slen > frame2_offset ? slen - frame2_offset : 0;
	const size_t frame2_len = get_atari_exec_data_len(s + frame2_offset, rem);
	if (frame1_len == frame2_len) {
		const size_t height = frame1_len / 40;
		return (height > 0 && height <= 240);
	}

	return 0;
}

static int
is_nec_zim_image(const uint8_t *s, const size_t slen)
{
	if (!s || slen < 700)
		return 0;

	const size_t offset = 512 + (size_t)(LE_U16(s + 506) << 1);
	if (offset + 26 > slen || LE_U32(s + offset) != 0x00
	|| s[offset + 20] != 1 || s[offset + 21] != 0)
		return 0;

	const uint16_t width = LE_U16(s + offset + 4) + 1;
	const uint16_t height = LE_U16(s + offset + 6) + 1;

	return (width > 0 && width <= 2048 && height > 0 && height <= 2048);
}

static int
is_msx_screen(const uint16_t v3, const off_t file_size)
{
	const size_t vs = (size_t)v3, fs = (size_t)file_size;

	if (vs + 7 == fs || vs + 8 == fs)
		return 1;
	if (v3 == 0xFA9F || v3 == 0xFAFF || v3 == 0xD407) return 1; /* SC8 */
	if (v3 == 0x37FF) return 1; /* SC2/GRP */
	if (v3 > 0x769E && v3 < 0x8000) return 1; /* GE5/GE6 */
	if (v3 == 0xD3FF) return 1; /* Screen 7-12 */
	if (file_size == 14343 || file_size == 16391 || file_size == 27136
	|| file_size == 30375 || file_size == 54279 || file_size == 64008
	|| file_size == 64167 || file_size == 65543)
		return 1;

	return 0;
}

static size_t
get_amstrad_header_len(const uint8_t *s, const off_t file_size)
{
	if (!s || file_size < 128)
		return 0;

	if ((size_t)LE_U16(s + 24) != (size_t)file_size - 128
	|| s[64] != s[24] || s[65] != s[25] || s[66] != 0)
		return 0;

	size_t sum = 0;
	for (size_t i = 0; i < 67; i++)
		sum += s[i];

	return ((size_t)LE_U16(s + 67) == sum) ? 128 : 0;
}

static int
is_degas_brush(const uint8_t *s, const size_t slen)
{
	if (!s || slen != 64)
		return 0;

	for (size_t i = 0; i < 64; i++) {
		if (s[i] > 1)
			return 0;
	}

	return 1;
}

static int
is_msl_image(const uint8_t *s, const size_t slen)
{
	if (!s || slen < 3 || slen > 36)
		return 0;

	const size_t height = (size_t)s[0];
	for (size_t i = 2; i < height; i++) {
		if (s[i] > 3)
			return 0;
	}

	return 1;
}

/* See RECOIL: recoil.c:RECOIL_DecodeTx[0,s] */
static int
is_tx_image(const uint8_t *s, const size_t slen)
{
	if (!s || (slen != 257 && slen != 262))
		return 0;

	/* Skip the file header (probably less than 8, but we're not sure) */
	for (size_t i = 8; i < slen - 1; i++) {
		if (s[i] > 0x0D)
			return 0;
	}

	/* Trailing byte in .tx0 seems to be 0xA0 */
	return (s[slen - 1] == 0xA0 || s[slen - 1] <= 0x0D);
}

static int
is_pictor_clp_image(const uint8_t *s, const size_t slen)
{
	if (!s || slen <= 10)
		return 0;

	const uint16_t W = LE_U16(s + 2);
	const uint16_t H = LE_U16(s + 4);
	const uint16_t X = LE_U16(s + 6);
	const uint16_t Y = LE_U16(s + 8);
/*	s[10] holds the pixel depth (bits per pixel): usually 255 (0xFF) */

	if (W == 0 || W > 1024 || H == 0 || H > 1024 || X > 1024 || Y > 1024)
		return 0;

	return 1;
}

static int
is_ascii_art(const uint8_t *s, const size_t slen)
{
	size_t x = 0, y = 0;

	for (size_t i = 0; i < slen; i++) {
		if (s[i] < 0x02) /* Exclude NUL and SOH (not seen in samples) */
			return 0;
		if (s[i] == 0x9B) {
			x = 0;
			y++;
			if (y > 24) /* Max lines */
				return 0;
		} else {
			if (x >= 64) /* Max rows per line */
				return 0;
			x++;
		}
	}

	return 1;
}

static int
is_atari_hir_image(const uint8_t *s, const size_t slen, const off_t file_size)
{
	if (!s || slen < 10)
		return 0;

	const uint16_t width = BE_U16(s + 4);
	const uint16_t height = BE_U16(s + 6);
	if (((size_t)width * (size_t)height) + 10 == (size_t)file_size)
		return 1;

	return 0;
}

static int
is_art_studio(const uint8_t *s, const size_t width, const size_t height,
	const off_t file_size)
{
	const size_t bytes_per_line = (width + 7) >> 3;
	const size_t offset = s ? get_amstrad_header_len(s, file_size) : 0;

	if ((size_t)file_size == offset + (bytes_per_line * height) + 5)
		return 1;

	return 0;
}

static int
is_atari_wnd(const uint8_t *s, const size_t slen)
{
	if (!s || slen < 2)
		return 0;

	const uint8_t width = s[0] + 1;
	const uint8_t height = s[1];
	const uint8_t stride = (width + 3) >> 2;

	if (stride > 40 || height == 0 || height > 192
	|| (size_t)stride * (size_t)height > 3070)
		return 0;

	return 1;
}

static int
is_atari_paintshop(const uint8_t *s, const size_t slen)
{
	if (!s || slen < 14)
		return 0;

	const uint16_t width = BE_U16(s + 10) + 1;
	const uint16_t height = BE_U16(s + 12) + 1;
	if (width != 0 && width <= 640 && height != 0 && height <= 400)
		return 1;

	return 0;
}

static int
is_atari_pmg(const uint8_t *s, const size_t slen, const off_t file_size)
{
	if (!s || slen < 10)
		return 0;

	size_t sprites = (size_t)s[7];
	size_t shapes = (size_t)s[8] * (size_t)s[9];
	size_t total_shapes = sprites * shapes;
	size_t height = (size_t)s[10];
	if (sprites == 0 || sprites > 4 || shapes == 0 || shapes > 160
	|| height == 0 || height > 48
	|| 11 + total_shapes * height != (size_t)file_size)
		return 0;

	return 1;
}

static int
is_compressed_printfox(const uint8_t *s, const size_t slen)
{
	if (!s || slen < 36)
		return 0;

	/* Printfox files are RLE-compressed. Ten or more 0x9b bytes in the
	 * first 512 bytes are a strong evidence of an RLE-compressed file. */
	size_t count = 0;
	/* Read up to LIMIT bytes */
	const size_t limit = slen > 512 ? 512 : slen;

	for (size_t i = 6; i + 2 < limit && count < 10; i++)
		count += (s[i] == 0x9B);

	return (count >= 10);
}

/* See RECOIL: recoil.c:RECOIL_DecodeC64Shp */
static int
is_c64_loadstar(const uint8_t *s, const size_t slen)
{
	if (!s || slen < 10 || s[0] != 0x00 || s[1] != 0x40)
		return 0;

	if ((s[2] == 0xA7 || s[2] == 0xE8) && s[3] != 0x19)
		return 0;

	if (s[2] == 0xA8) {
		const size_t height = (size_t)s[3] << 3;
		if (height == 0 || height > 200)
			return 0;
	}

	if (s[2] == 0xE8 && s[3] == 0x19 && s[4] >= 0xF0)
		return 1;

	uint8_t escape = 0;
	if (s[2] == 0x00 || s[2] == 0x80) escape = s[3];
	else if (s[2] == 0xA7 || s[2] == 0xA8) escape = s[4];
	else if (s[2] == 0xE8) escape = s[5];
	else return 0;

	if (escape == 0x00)
		return 0;

	size_t count = 0;
	const size_t max = slen > 1024 ? 1024 : slen;
	for (size_t i = 6; i + 1 < max && count < 20; i++)
		count += (s[i] == escape && s[i + 1] > 0x00);

	return (count >= 20);
}

static int
is_delmpaint(const uint8_t *s, const size_t slen, const off_t file_size)
{
	if (!s || slen < 12)
		return 0;

	/* .del files have only 2 packed blocks, each taking 4 bytes. */
	size_t header_len = 8;
	const size_t len1 = (size_t)BE_U32(s); /* Length of 1st packed block */
	const size_t len2 = (size_t)BE_U32(s + 4); /* Length of 2nd packed block */
	if (len1 == 0 || len2 == 0 || len1 + len2 + header_len > (size_t)file_size)
		return 0;

	if (s[10] == 0x01 && s[11] == 0x40)
		return 1; /* .del */

	/* .dph files have 10 packed blocks, each taking 4 bytes. */
	header_len = 40;
	if (slen <= header_len)
		return 0;

	/* We already have blocks 1 and 2. Let's retrieve the next 8 blocks. */
	size_t dph_len = len1 + len2;
	for (size_t i = 8, blocks = 2; i + 4 < slen && blocks < 10; i += 4) {
		if (s[i] != 0x00 || s[i + 1] != 0x00)
			return 0;
		dph_len += (size_t)BE_U32(s + i);
		blocks++;
	}

	if (dph_len > 0 && dph_len + header_len <= (size_t)file_size)
		return 1; /* .dph */

	return 0;
}

static int
is_dali_compressed(const uint8_t *s, const size_t slen, const off_t file_size)
{
	if (!s || slen <= 47) /* 32 + "32000"\0x0D\0x0A"32000"\0x0D\0x0A */
		return 0;

	s += 32; /* Jump to the first ASCII number */

	char *ptr = NULL;
	const long n1 = strtol((const char *)s, &ptr, 10);
	if (ptr == (const char *)s || n1 <= 0 || n1 > 32000)
		return 0;

	const size_t n1len = DIGINUM(n1);
	if (s[n1len] != 0x0D || s[n1len + 1] != 0x0A || !IS_DIGIT(s[n1len + 2]))
		return 0;

	s += n1len + 2; /* Jump to the second ASCII number */

	const long n2 = strtol((const char *)s, &ptr, 10);
	if (ptr == (const char *)s || n2 <= 0 || n2 > 32000)
		return 0;

	const size_t n2len = DIGINUM(n2);
	if (s[n2len] != 0x0D || s[n2len + 1] != 0x0A)
		return 0;

	const size_t expected_size =
		32 + (size_t)n1 + 2 + (size_t)n2 + 2 + n1len + n2len;
	if (expected_size == (size_t)file_size)
		return 1;

	return 0;
}

static int
is_neochrome_image(const uint8_t *s, const size_t slen)
{
	if (slen <= 58)
		return 0;

	const uint16_t res = BE_U16(s + 2);
	const uint16_t X = BE_U16(s + 54);
	const uint16_t Y = BE_U16(s + 56);
	/* Width and height are at offsets 58 and 60 (both BE_U16), but
	 * are ignored (always 320x200). */
	return (res <= 2 && X == 0 && Y == 0);
}

/* See https://temlib.org/AtariForumWiki/index.php/NEOchrome_Animation_file_format */
static int
is_neochrome_animation(const uint8_t *s, const size_t slen)
{
	if (!s || slen < 22)
		return 0;

	const uint16_t width = BE_U16(s + 4);
	const uint16_t x_offset = BE_U16(s + 10) + 1;
	const uint32_t reserved = BE_U32(s + 18);
	if (width % 8 == 0 && x_offset % 16 == 0 && reserved == 0)
		return 1;

	return 0;
}

/* See https://temlib.org/AtariForumWiki/index.php/Animaster_Sprite_Bank_file_format */
static int
is_animaster(const uint8_t *s, const size_t slen)
{
	if (!s || slen <= 44)
		return 0;

	const uint16_t frames    = BE_U16(s);
	const uint16_t frame_len = BE_U16(s + 2);
	const uint8_t max_width  = s[4];
	const uint8_t max_height = s[5];
	const uint16_t frame_width  = BE_U16(s + 38) + 1;
	const uint16_t frame_height = BE_U16(s + 40) + 1;

	if (max_width > 0 && max_width == frame_width
	&& max_height > 0 && max_height == frame_height
	&& frames > 0 && frame_len > 0)
		return 1;

	return 0;
}

/* See RECOIL: recoil.c:RECOIL_DecodeSrt */
static int
is_srt_image(const char *file)
{
	int fd = open(file, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
	if (fd == -1)
		return 0;

	if (lseek(fd, 32000, SEEK_SET) == (off_t)-1) {
		close(fd);
		return 0;
	}

	uint8_t buf[6];
	const ssize_t bytes = read(fd, buf, sizeof(buf));
	close(fd);

	if (bytes > 0 && buf[0] == 'J' && buf[1] == 'H' && buf[2] == 'S'
	&& buf[3] == 'y' && buf[4] == 0x00 && buf[5] == 0x01)
		return 1;

	return 0;
}

static int
is_quantum_paint(const uint8_t *s, const size_t slen)
{
	if (!s || slen <= 176)
		return 0;

	const uint32_t mode = BE_U32(s + 4);
	if (mode == 0x80010000 || mode == 0x80000000 || mode == 0x03330000)
		return 1;

	const uint16_t palette_size = 32; /* Available colors */

	const uint16_t start = BE_U16(s + 160);
	const uint16_t active = BE_U16(s + 162);
	const uint16_t first_color = BE_U16(s + 164);
	const uint16_t last_color = BE_U16(s + 166);
	const uint16_t cycle_speed = BE_U16(s + 168);
	const uint16_t cycle = BE_U16(s + 170);
	const uint32_t reserved = BE_U32(s + 172);

	if (start == 0 && active > 0 && first_color <= palette_size
	&& last_color <= palette_size && cycle_speed <= 255
	&& (cycle <= 1 || cycle == 0xFFFF) && reserved == 0)
		return 1;

	return 0;
}

struct companion_t {
	const char *ext1;       /* Original file extension */
	const size_t ext1_len;  /* Length of the original extension */
	const char *ext2_lower; /* Companion file extension (lower) */
	const char *ext2_upper; /* Companion file extension (upper) */
	const size_t ext2_len;  /* Length of the companion file extension */
	const char *mime_type;
};

static struct companion_t companion[] = {
	{"CM5", 3, "gfx", "GFX", 3, "image/x-amstrad-mode5"},
	{"CMP", 3, "pl5", "PL5", 3, "image/x-msx-ddgraph"},
	{"CPT", 3, "hbl", "HBL", 3, "image/x-atari-canvas"},
	{"MIC", 3, "col", "COL", 3, "image/x-atari-graph2font"},
	{"MUR", 3, "pal", "PAL", 3, "image/x-atari-mur"},
	{"PIC0", 4, "pic1", "PIC1", 4, "image/x-commodore-picasso"},
	{"SCR", 3, "col", "COL", 3, "image/x-commodore-petscii-editor"},
	{"SCR", 3, "pal", "PAL", 3, "image/x-amstrad-art-studio"},
	{"SRI", 3, "pl7", "PL7", 3, "image/x-msx-graphsaurus"},
	{"WIN", 3, "pal", "PAL", 3, "image/x-amstrad-art-studio"},
	{NULL, 0, NULL, NULL, 0, NULL}
	/* https://recoil.sourceforge.net/formats.html list more formats
	 * requiring a compaion file. But most of them do not need to be added
	 * here, since we already identify them by pure magic.
	PPH+ODD+EVE          Amstrad PerfectPix
	LUM+COL              Atari Technicolor Dream
	MIC+PMG+RP+RP.INI    Atari RastaConverter
	VSC+G2F              Atari Graph2Font vertical scroll
	NEO+RST              Atari NEOchrome Master
	SH5+PL5, SH6+PL6, SH7+PL7 MSX
	SC5+S15, SC6+S16, SC7+S17, SC8+S18 MSX Screen 5-8 interlaced
	SCA+S1A                            MSX Screen 10 interlaced
	SCC+S1C                            MSX Screen 12 interlaced
	SR5+PL5, SR6+PL6, SR7+PL7          MSX GraphSaurus
	GLA/GLB/SHA/SHB+PLA                MSX */
};

static int
check_companion_file(const char *filename, const size_t namelen,
	struct companion_t *comp)
{
	if (!filename || !comp)
		return 0;

	const size_t ext_len = comp->ext2_len;
	char basename[PATH_MAX + 1];
	if (namelen + ext_len + 1 >= sizeof(basename))
		return 0;

	/* Copy filename up to and including the last dot */
	xstrsncpy(basename, filename, namelen + 1);
	basename[namelen] = '\0';

	/* Append extensions (those of the companion file) and check for
	 * file existence */
	struct stat a;
	xstrsncpy(basename + namelen, comp->ext2_upper, ext_len + 1);
	basename[namelen + ext_len + 1] = '\0';
	if (lstat(basename, &a) != -1 && S_ISREG(a.st_mode))
		return 1;

	xstrsncpy(basename + namelen, comp->ext2_lower, ext_len + 1);
	basename[namelen + ext_len + 1] = '\0';
	if (lstat(basename, &a) != -1 && S_ISREG(a.st_mode))
		return 1;

	return 0;
}

static const char *
get_mimetype_from_companion_file(const char *filename)
{
	if (!filename)
		return NULL;

	const char *ext = strrchr(filename, '.');
	if (!ext || ext == filename || !++ext)
		return NULL;

	const size_t ext_len = !ext[1] ? 1 : !ext[2] ? 2
		: !ext[3] ? 3 : strlen(ext);
	/* Length of filename up to and including the dot */
	const size_t name_len = (size_t)(ext - filename);
	const size_t comp_num = sizeof(companion) / sizeof(companion[0]) - 1;

	for (size_t i = 0; i < comp_num; i++) {
		if (ext_len != companion[i].ext1_len)
			continue;

		size_t j;
		for (j = 0; j < ext_len; j++) {
			if (TOUPPER(ext[j]) != companion[i].ext1[j])
				break;
		}
		if (j != ext_len) /* No match */
			continue;

		if (check_companion_file(filename, name_len, &companion[i]) == 1)
			return companion[i].mime_type;
	}

	return NULL;
}

static const char *
check_legacy_formats(const char *file, const uint8_t *sig, const size_t nread,
	const off_t file_size)
{
	if (!sig || nread == 0)
		return NULL;

	if (nread > 7 && sig[0] == 0xD0 && sig[1] == 0xCF && sig[2] == 0x11
	&& sig[3] == 0xE0 && sig[4] == 0xA1 && sig[5] == 0xB1 && sig[6] == 0x1A
	&& sig[7] == 0xE1) /* MS Compound Document Format - OLE2 */
		return get_ole2_ms_office_type(sig, nread);

	const char *ret = check_pre_ole2_office_docs(sig, nread);
	if (ret)
		return ret;

	if (nread > 17 && sig[0] == 'B' && sig[1] == 'M') {
		const uint32_t dib_header = LE_U32(sig + 14);
		if (dib_header == 12 || dib_header == 16 || dib_header == 24
		|| dib_header == 40 || dib_header == 48 || dib_header == 52
		|| dib_header == 56 || dib_header == 64 || dib_header == 66
		|| dib_header == 108 || dib_header == 124)
			return "image/bmp";
	}

	if (nread > 91 && sig[0] == 0x53 && sig[1] == 0x80 && sig[2] == 0xF6
	&& sig[3] == 0x34 && sig[88] == 'P' && sig[89] == 'I' && sig[90] == 'C'
	&& sig[91] == 'T') /* Softimage PIC (discontinuated in 2015) */
		return "image/x-softimage-pic";

	if (nread >= 16 && sig[0] == 0x30 && sig[1] == 0x26
	&& memcmp(sig, "\x30\x26\xB2\x75\x8E\x66\xCF\x11\xA6\xD9\x00\xAA\x00\x62\xCE\x6C", 16) == 0)
		return "video/x-ms-asf"; /* asf, wma, wmv */

	if (nread > 3 && sig[0] == 0x0A && sig[1] <= 0x05 && sig[2] == 0x01
	&& sig[3] >= 0x01 && sig[3] <= 0x08)
		return "image/vnd.zbrush.pcx";

	if (nread > 4 && sig[0] == '{' && sig[1] == '\\' && sig[2] == 'r'
	&& sig[3] == 't' && sig[4] == 'f')
		return "text/rtf"; /* application/rtf (MIME-info) */

	if (nread > 3 && sig[0] == 'M' && sig[1] == 'Z' && (sig[3] == 0x00
	|| sig[3] == 0x01))
		return get_ms_exec_type(sig, nread); /* .exe */

	if (nread > 384 && sig[369] == 'M'
	&& memcmp(sig + 369, "MICROSOFT PIFEX\0", 16) == 0)
		return "application/x-dosexec"; /* .pif */

	if (nread > 7 && sig[0] == 'M' && sig[1] == 'S' && sig[2] == 'C'
	&& sig[3] == 'F' && sig[4] == 0x00 && sig[5] == 0x00 && sig[6] == 0x00
	&& sig[7] == 0x00)
		return "application/vnd.ms-cab-compressed";

	if (nread >= 20 && sig[0] == 'L' && sig[1] == 0x00
	&& memcmp(sig, "L\0\0\0\001\024\002\0\0\0\0\0\300\0\0\0\0\0\0F", 20) == 0)
		return "application/x-ms-shortcut"; /* .lnk */

	if (nread > 3 && sig[0] == 0x3F && sig[1] == 0x5F && sig[2] == 0x03
	&& sig[3] == 0x00)
		return "application/x-winhelp";

	if (nread > 5 && sig[0] == 'I' && sig[1] == 'T' && sig[2] == 'S'
	&& sig[3] == 'F' && sig[4] == 0x03 && sig[5] == 0x00)
		return "application/vnd.ms-htmlhelp";

	if (nread > 7 && sig[0] == 0x4c && sig[1] == 0x4e && sig[2] == 0x02
	&& sig[3] == 0x00 && (sig[4] == 0x01 || sig[4] == 0x00) && sig[5] == 0x00
	&& sig[6] == 0x3a && sig[7] == 0x00)
		return "application/x-ms-hlp";

	if (nread > 7 && sig[0] == 'I' && sig[1] == 'D' && sig[2] == ';'
	&& sig[3] == 'P' && sig[4] > 0 && sig[5] > 0 && sig[6] > 0
	&& sig[7] > 0)
		return "application/x-sylk";

	if (nread > 44 && sig[0] == 0x01 && sig[1] == 0x00 && sig[40] == ' '
	&& sig[41] == 'E' && sig[42] == 'M' && sig[43] == 'F' && sig[44] == 0x00)
		return "image/emf";

	if (nread > 3 && ((sig[0] == 0xD7 && sig[1] == 0xCD && sig[2] == 0xC6
	&& sig[3] == 0x9A)
	|| ((sig[0] == 0x01 || sig[0] == 0x02) && sig[1] == 0x00
	&& sig[2] == 0x09 && sig[3] == 0x00)))
		return "image/wmf";

	if (nread > 3 && sig[0] == 0x01 && sig[1] == 0xDA &&
	(sig[2] == 0x00 || sig[2] == 0x01) && (sig[3] == 0x01 || sig[3] == 0x02))
		return "image/x-sgi";

	if (nread >= 12 && sig[0] == 'F' && sig[1] == 'O' && sig[2] == 'R'
	&& sig[3] == 'M'
	/* Skip NEC-PC98 ZIM image format, totally unrelated to IFF */
	&& (sig[4] != 'A' || sig[5] != 'T' || sig[6] != '-' || sig[7] != 'A'))
		return check_iff_magic(sig, nread); /* Mostly AIFF */

	if (nread >= 16 && sig[0] == 'R' && sig[1] == 'F' && sig[2] == '6'
	&& sig[3] == '4' && memcmp(sig, "RF64\xff\xff\xff\xffWAVEds64", 16) == 0)
		return "audio/x-wav";

	if (nread > 4 && sig[0] == '.' && sig[1] == 's' && sig[2] == 'n'
	&& sig[3] == 'd' && sig[4] == 0x00)
		return "audio/basic";

	if (nread > 3 && sig[0] == 'T' && sig[1] == 'T' && sig[2] == 'A'
	&& sig[3] == '1')
		return "audio/x-tta";

	if (nread > 5 && sig[0] == 0x0B && sig[1] == 0x77) {
		/* See FFmpeg source: libavcodec/ac3_parser.c */
		const uint8_t bsid = (sig[5] >> 3) & 0x1F;
		if (bsid <= 8) return "audio/ac3";
		if (bsid >= 11 && bsid <= 16) return "audio/eac3";
	}

	/* DSD Stream File (DSF)
	 * https://dsd-guide.com/sites/default/files/white-papers/DSFFileFormatSpec_E.pdf
	 * and FFmpeg: libavformat/dsfdec.c */
	if (nread > 80 && sig[0] == 'D' && sig[1] == 'S' && sig[2] == 'D'
	&& sig[3] == ' ' && sig[28] == 'f' /* fmt */ && sig[80] == 'd' /* data */)
		return "audio/x-dsf";
	if (nread > 11 && sig[0] == 'F' && sig[1] == 'R' && sig[2] == 'M'
	&& sig[3] == '8' && sig[8] == 'D' && sig[9] == 'S'
	&& sig[10] == 'D' && sig[11] == ' ')
		return "audio/x-dff"; /* DSDIFF */

	/* MIDI Sample Dump Standard File. FFmpeg: libavformat/sdsdec.c */
	if (nread > 20 && sig[0] == 0xF0 && sig[1] == 0x7E && sig[2] == 0x00
	&& sig[3] == 0x01 && (sig[6] >= 8 && sig[6] <= 28) && sig[20] == 0xF7)
		return "audio/x-sds";

	if (nread > 3 && sig[0] == 'I' && sig[1] == 'M' && sig[2] == 'P'
	&& (sig[3] == 'M' || sig[3] == 'S'))
		return "audio/x-it";

	if (nread > 3 && sig[0] == 'R' && sig[1] == 'S' && sig[2] == 'T'
	&& sig[3] == 'M') /* Nintendo Wii BRSTM audio file */
		return "audio/x-brstm";

	if (nread >= GSM_MIN_BYTES && (sig[0] & 0xF0) == 0xD0
	&& (sig[33] & 0xF0) == 0xD0 && is_gsm_audio(sig, nread) == 1)
		return "audio/x-gsm";

	if (nread >= 29 && sig[0] == 'S' && sig[1] == 'N' && sig[2] == 'E'
	&& sig[3] == 'S' && memcmp(sig, "SNES-SPC700 Sound File Data v", 29) == 0)
		return "audio/x-snes-spc";

	if (nread > 3 && (sig[0] == 'I' || sig[0] == 'P') && sig[1] == 'W'
	&& sig[2] == 'A' && sig[3] == 'D')
		return "application/x-doom-wad";
	/* http://fileformats.archiveteam.org/wiki/Doom_MUS */
	if (nread > 3 && sig[0] == 'M' && sig[1] == 'U'	&& sig[2] == 'S'
	&& sig[3] == 0x1A)
		return "audio/x-doom-music"; /* .mus */

	if (nread > 3 && sig[0] == 'D' && sig[1] == 'O' && sig[2] == 'S'
	&& sig[3] < 0x08)
		return "application/x-amiga-disk-format";

	/* http://fileformats.archiveteam.org/wiki/LIMIT */
	if (nread > 2 && sig[0] == 'L' && sig[1] == 'M' && sig[2] == 0x1A)
		return "application/x-lim";
	/* http://fileformats.archiveteam.org/wiki/BlakHole */
	if (nread > 6 && sig[0] == 'B' && sig[1] == 'H' && sig[2] == 0x05
	&& sig[3] == 0x07 && sig[5] == 0x00 && sig[6] == 0x25)
		return "application/x-blackhole";
	/* http://fileformats.archiveteam.org/wiki/AKT */
	if (nread > 4 && sig[0] == 'A' && sig[1] == 'K' && sig[2] == 'T'
	&& sig[3] >= 0x07 && sig[3] <= 0x0B && sig[4] == 0x09)
		return "application/x-akt";
	/* http://fileformats.archiveteam.org/wiki/FOXSQZ */
	if (nread > 24 && sig[0] == 'F' && sig[2] == 'X' && sig[5] == 'Z'
	&& sig[22] == 0x00 && sig[23] == 0x1A
	&& memcmp(sig, "FOXSQZ COMPRESSED FILE", 22) == 0)
		return "application/x-foxsqz";
	/* http://fileformats.archiveteam.org/wiki/Apple_Disk_Image */
	if (nread > 2 && sig[0] == '2' && sig[1] == 'I' && sig[2] == 'M'
	&& sig[3] == 'G')
		return "application/x-apple-diskimage";
	/* http://fileformats.archiveteam.org/wiki/ASD_Archiver */
	if (nread > 4 && sig[0] == 'A' && sig[1] == 'S' && sig[2] == 'D'
	&& sig[3] == '0' && (sig[4] == '1' || sig[4] == '2') && sig[5] == 0x1A)
		return "application/x-asd";
	/* http://fileformats.archiveteam.org/wiki/CRUSH */
	if (nread > 6 && sig[0] == 'C' && sig[1] == 'R' && sig[2] == 'U'
	&& sig[3] == 'S' && sig[4] == 'H' && sig[5] == ' ' && sig[6] == 'v')
		return "application/x-crush";
	/* http://fileformats.archiveteam.org/wiki/ESP_(compressed_archive) */
	if (nread > 3 && sig[0] == 'E' && sig[1] == 'S' && sig[2] == 'P'
	&& sig[3] == '>')
		return "application/x-esp";
	/* http://fileformats.archiveteam.org/wiki/HPACK_(compressed_archive) */
	if (nread > 3 && sig[0] == 'H' && sig[1] == 'P' && sig[2] == 'A'
	&& sig[3] == 'K')
		return "application/x-hpack";
	/* http://fileformats.archiveteam.org/wiki/UHARC */
	if (nread > 3 && sig[0] == 'U' && sig[1] == 'H' && sig[2] == 'A')
		return "application/x-uha";
	/* http://fileformats.archiveteam.org/wiki/PIM */
	if (nread > 3 && (sig[0] == 0x01 || sig[0] == 0x02) && sig[1] == 'P'
	&& sig[2] == 'I' && sig[3] == 'M')
		return "application/x-pim";
	if (nread > 3 && sig[0] == 'P' && sig[1] == 'I' && sig[2] == 'M'
	&& sig[3] == '2')
		return "application/x-pim";
	/* http://fileformats.archiveteam.org/wiki/EGG_(ALZip) */
	if (nread > 3 && sig[0] == 'E' && sig[1] == 'G' && sig[2] == 'G'
	&& sig[3] == 'A')
		return "application/x-egg";
	/* http://fileformats.archiveteam.org/wiki/BSArc_and_BSA */
	if (nread > 3 && sig[0] == 0xFF && sig[1] == 'B' && sig[2] == 'S'
	&& sig[3] == 'G')
		return "application/x-bsa";
	/* http://fileformats.archiveteam.org/wiki/UC2 */
	if (nread > 3 && sig[0] == 'U' && sig[1] == 'C' && sig[2] == '2'
	&& sig[3] == 0x1A)
		return "application/x-uc2";
	/* http://fileformats.archiveteam.org/wiki/SQX */
	if (nread > 11 && sig[7] == '-' && sig[8] == 's' && sig[9] == 'q'
	&& sig[10] == 'x' && sig[11] == '-')
		return "application/x-sqx";
	/* http://fileformats.archiveteam.org/wiki/TPK_(compressed_archive) */
	if (nread > 6 && sig[2] == '-' && sig[3] == 'T' && sig[4] == 'K'
	&& (sig[5] == '0' || sig[5] == '1') && sig[6] == '-')
		return "application/x-tpk";
	/* file(1): magic/Magdir/archives */
	if (nread > 4 && (BE_U32(sig) & 0xFFFFFFF0) == 0x797A3030)
		return "application/x-deepfreezer";
	/* file(1): magic/Magdir/archives */
	if (nread > 4 && sig[0] == 0x1F && sig[1] == 0x9F && sig[2] == 0x4A
	&& sig[3] == 0x10 && sig[4] == 0x0A)
		return "application/x-freeze";
	/* http://fileformats.archiveteam.org/wiki/PAKLEO */
	if (nread > 5 && sig[0] == 'L' && sig[1] == 'E' && sig[2] == 'O'
	&& sig[3] == 'L' && sig[4] == 'Z' && sig[5] == 'W')
		return "application/x-pakleo";
	/* file(1): magic/Magdir/archives */
	if (nread > 5 && sig[0] == 0xC2 && sig[1] == 0xA8 && sig[2] == 'M'
	&& sig[3] == 'P' && sig[4] == 0xC2 && sig[5] == 0xA8)
		return "application/x-kboom";
	/* file(1): magic/Magdir/archives */
	if (nread > 6 && sig[0] == 'D' && sig[1] == 'S' && sig[2] == 'I'
	&& sig[3] == 'G' && sig[4] == 'D' && sig[5] == 'C' && sig[6] == 'C')
		return "application/x-crossepac";
	/* http://fileformats.archiveteam.org/wiki/ChArc */
	if (nread > 3 && sig[0] == 'S' && sig[1] == 'C' && sig[2] == 'h'
	&& sig[3] == 'f')
		return "application/x-charc";
	/* http://fileformats.archiveteam.org/wiki/NuFX */
	if (nread > 5 && sig[0] == 0x4E && sig[1] == 0xF5 && sig[2] == 0x46
	&& sig[3] == 0xE9 && sig[4] == 0x6C && sig[5] == 0xE5)
		return "application/x-nufx";
	/* http://fileformats.archiveteam.org/wiki/JRchive */
	if (nread > 6 && sig[0] == 'J' && sig[1] == 'R' && sig[2] == 'c'
	&& sig[3] == 'h' && sig[4] == 'i' && sig[5] == 'v' && sig[6] == 'e')
		return "application/x-jrc";
	/* http://fileformats.archiveteam.org/wiki/Pretty_Simple_Archiver */
	if (nread > 6 && sig[0] == 'P' && sig[1] == 'S' && sig[2] == 'A'
	&& sig[3] == 0x01 && sig[4] == 0x03)
		return "application/x-psa";
	/* file(1): magic/Magdir/archives */
	if (nread >= 10 && sig[0] == 'D' && sig[4] == ' ' && sig[5] == 'P'
	&& memcmp(sig, "Dirk Paehl", 10) == 0)
		return "application/x-dpa";
	/* http://fileformats.archiveteam.org/wiki/SAR_(Streamline_Design) */
	if (nread > 3 && sig[3] == 'L' && sig[4] == 'H'
	&& (sig[5] == '5' || sig[5] == '0'))
		return "application/x-sar";
	/* http://fileformats.archiveteam.org/wiki/HAP */
	if (nread > 3 && sig[0] == 0x91 && sig[1] == '3' && sig[2] == 'H'
	&& sig[3] == 'F')
		return "application/x-hap";
	/* file(1): magic/Magdir/archives */
	if (nread > 4 && sig[2] == '-' && sig[3] == 'a' && sig[4] == 'h')
		return "application/x-mar";
	/* http://fileformats.archiveteam.org/wiki/MAR_Utility */
	if (nread > 3 && sig[0] == 'M' && sig[1] == 'A' && sig[2] == 'R'
	&& sig[3] == 0x80)
		return "application/x-mar";
	/* http://fileformats.archiveteam.org/wiki/MDCD */
	if (nread > 4 && sig[0] == 'M' && sig[1] == 'D' && sig[2] == 'm'
	&& sig[3] == 'd' && sig[4] == 0x01)
		return "application/x-mdcd";
	/* http://fileformats.archiveteam.org/wiki/SQWEZ */
	if (nread > 4 && sig[0] == 'S' && sig[1] == 'Q' && sig[2] == 'W'
	&& sig[3] == 'E' && sig[4] == 'Z')
		return "application/x-sqwez";
	/* http://fileformats.archiveteam.org/wiki/DRY */
	if (nread > 3 && sig[0] == '-' && sig[1] == 'H' && sig[2] == '2'
	&& sig[3] == 'O')
		return "application/x-dry";
	/* http://fileformats.archiveteam.org/wiki/Squeeze_It */
	if (nread > 4 && sig[0] == 'H' && sig[1] == 'L' && sig[2] == 'S'
	&& sig[3] == 'Q' && sig[4] == 'Z')
		return "application/x-squeeze-it";
	if (nread > 3 && sig[0] == 'C' && sig[1] == 'F' && sig[2] == 'L'
	&& sig[3] == '3')
		return "application/x-cfl";
	/* http://fileformats.archiveteam.org/wiki/ARQ */
	if (nread > 3 && sig[0] == 'g' && sig[1] == 'W' && sig[2] == 0x04
	&& sig[3] == 0x01)
		return "application/x-arq";
	/* http://fileformats.archiveteam.org/wiki/YAC */
	if (nread > 16 && sig[14] == 'Y' && sig[15] == 'C')
		return "application/x-yac";
	/* http://fileformats.archiveteam.org/wiki/ArcFS */
	if (nread > 7 && sig[0] == 0x1A && sig[1] == 'a' && sig[2] == 'r'
	&& sig[3] == 'c' && sig[4] == 'h' && sig[5] == 'i' && sig[6] == 'v'
	&& sig[7] == 'e')
		return "application/x-arcfs";
	if (nread > 7 && sig[0] == 'A' && sig[1] == 'r' && sig[2] == 'c'
	&& sig[3] == 'h' && sig[4] == 'i' && sig[5] == 'v' && sig[6] == 'e'
	&& sig[7] == 0x00)
		return "application/x-arcfs";
	/* http://fileformats.archiveteam.org/wiki/Sunzip */
	if (nread > 3 && sig[0] == 'S' && sig[1] == 'Z' && sig[2] == 0x0A
	&& sig[3] == 0x04)
		return "application/x-sunzip";
	/* file(1): magic/Magdir/archives */
	if (nread > 3 && sig[0] == 0xAD && sig[1] == '6' && sig[2] == '"')
		return "application/x-agmc";
	/* http://fileformats.archiveteam.org/wiki/WWPACK */
	if (nread > 3 && sig[0] == 'W' && sig[1] == 'W' && sig[2] == 'P')
		return "application/x-wwpack";
	/* http://fileformats.archiveteam.org/wiki/HKI */
	if (nread > 3 && sig[0] == 'H' && sig[1] == 'K' && sig[2] == 'I'
	&& sig[3] > 0x00 && sig[3] <= 0x03)
		return "application/x-winhki";
	/* file(1): magic/Magdir/archives */
	if (nread > 3 && sig[0] == 0x61 && sig[1] == 0x5C && sig[2] == 0x04
	&& sig[3] == 0x05)
		return "application/x-winhki";
	/* http://fileformats.archiveteam.org/wiki/SABDU */
	if (nread >= 20 && sig[0] == 'S' && sig[1] == 'A' && sig[2] == 'B'
	&& memcmp(sig, "SAB Diskette Utility\0", 21) == 0)
		return "application/x-sabdu";
	/* http://fileformats.archiveteam.org/wiki/Compressia */
	if (nread > 6 && sig[0] == 'C' && sig[1] == 'M' && sig[2] == 'P'
	&& sig[3] == '0' && sig[4] == 'C' && sig[5] == 'M' && sig[6] == 'P')
		return "application/x-compressia";
	/* http://fileformats.archiveteam.org/wiki/UHBC */
	if (nread > 3 && sig[0] == 'U' && sig[1] == 'H' && sig[2] == 'B')
		return "application/x-uhbc";
	/* http://fileformats.archiveteam.org/wiki/GCA */
	if (nread > 3 && sig[0] == 'G' && sig[1] == 'C' && sig[2] == 'A'
	&& sig[3] == 'X')
		return "application/x-gca";
	/* file(1): magic/Magdir/archives */
	if (nread >= 11 && sig[3] == 'W' && sig[4] == 'I' && sig[5] == 'N'
	&& memcmp(sig + 3, "WINIMAGE", 8) == 0)
		return "application/x-winimage";
	/* file(1): magic/Magdir/archives */
	if (nread > 3 && sig[0] == 'r' && sig[1] == 'd' && sig[2] == 'q'
	&& sig[3] == 'x')
		return "application/x-reduq";
	/* http://fileformats.archiveteam.org/wiki/Disk_Masher_System */
	if (nread > 3 && sig[0] == 'D' && sig[1] == 'M' && sig[2] == 'S'
	&& sig[3] == '!')
		return "application/x-dms";
	/* file(1): magic/Magdir/archives */
	if (nread > 3 && sig[0] == 0x8F && sig[1] == 0xAF && sig[2] == 0xAC
	&& sig[3] == 0x8C)
		return "application/x-epc";
	/* http://fileformats.archiveteam.org/wiki/PPMd */
	if (nread > 3 && sig[0] == 0x8F && sig[1] == 0xAF && sig[2] == 0xAC
	&& sig[3] == 0x84)
		return "application/x-ppmd";
	/* http://fileformats.archiveteam.org/wiki/Ai_Archiver */
	if (nread > 3 && sig[0] == 'A' && sig[1] == 'i'
	&& (sig[2] == 0x01 || sig[2] == 0x02) && sig[3] <= 0x01)
		return "application/x-compress-ai";
	/* http://fileformats.archiveteam.org/wiki/EA_archive */
	if (nread > 2 && sig[0] == 0x1A && sig[1] == 'E' && sig[2] == 'A')
		return "application/x-ea-archive";
	if (nread > 4) {
		/* file(1): magic/Magdir/archives */
		const uint32_t v = LE_U32(sig) & 0x8080FFFF;
		if (v == 0x00000021A || v == 0x00000031A || v == 0x00000041A
		|| v == 0x00000061A || v == 0x00000081A || v == 0x00000091A
		|| v == 0x000000a1A || v == 0x00000141A || v == 0x00000481A)
			return "application/x-arc";
	}

	/* http://fileformats.archiveteam.org/wiki/DrawPlus */
	if (nread >= 20 && sig[0] == 'V' && sig[1] == '{' && sig[2] == 'D'
	&& memcmp(sig, "V{DrawPlus Picture V", 20) == 0)
		return "image/x-drawplus";

	if (nread > 3 && sig[0] == 'F' && sig[1] == 'L' && sig[2] == 'V'
	&& sig[3] == 0x01)
		return "video/x-flv";

	if ((BE_U32(sig) & 0xffffff00) == 0x1f070000)
		return "video/x-dv";

	if (nread > 3 && sig[0] == 'D' && sig[1] == 'K' && sig[2] == 'I'
	&& sig[3] == 'F')
		return "video/x-ivf";

	if (nread > 3 && sig[0] == 'N' && sig[1] == 'S' && sig[2] == 'V'
	&& sig[3] == 'f') /* Nullsoft video */
		return "video/x-nsv";

	if (nread > 12 && sig[0] == 'N' && sig[1] == 'u' && sig[6] == 'V'
	&& sig[11] == 0x00 && memcmp(sig, "NuppelVideo\0", 12) == 0)
		return "video/x-nuv";

	if (nread > 12 && sig[0] == 'M' && sig[5] == 'V' && sig[6] == 'V'
	&& sig[11] == 0x00 && memcmp(sig, "MythTVVideo\0", 12) == 0)
		return "video/x-myth-tv";

	if (nread > 12 && sig[0] == 'M' && sig[1] == 'L' && sig[2] == 'V'
	&& sig[3] == 'I')
		return "video/x-mlv";

	if (nread > 7 && sig[0] == 'A' && sig[1] == 'R' && sig[2] == 'M'
	&& sig[3] == 'o' && sig[4] == 'v' && sig[5] == 'i' && sig[6] == 'e'
	&& sig[7] == 0x0A)
		return "video/x-acorn-replay";

	if (nread > 7 && sig[0] == 'W' && sig[1] == 'O' && sig[2] == 'T'
	&& sig[3] == 'F')
		return "video/x-webex-wrf";

	if (nread > 10 && sig[0] == 'Y' && sig[1] == 'O' && sig[2] < 10
	&& sig[3] < 10 && sig[6] && sig[7] && !(sig[8] & 1) && !(sig[10] & 1))
		return "video/x-psygnosis-yop";

	if (nread > 7 && sig[0] == 'C' && sig[1] == 'I' && sig[2] == ','
	&& sig[3] == 0x00 && sig[4] <= 0x02)
		return "video/x-cine";

	/* https://www.compuphase.com/flic.htm#FLICHEADER */
	if (nread > 21 && sig[5] == 0xAF && LE_U16(sig + 14) <= 3
	&& sig[20] == 0x00 && sig[21] == 0x00) {
		if (sig[4] == 0x12)
			return "video/x-flc";
		if (sig[4] == 0x11) { /* Standard FLI files are 320x200x8 */
			const uint16_t width = LE_U16(sig + 8);
			const uint16_t height = LE_U16(sig + 10);
			const uint16_t depth = LE_U16(sig + 12);
			if (width == 320 && height == 200 && depth == 8)
				return "video/x-fli";
		}
	}

	/* https://wiki.multimedia.cx/index.php/GXF */
	if (nread > 15 && (sig[0] + sig[1] + sig[2] + sig[3] + sig[10]
	+ sig[11] + sig[12] + sig[13]) == 0x00 && sig[4] == 0x01
	&& sig[14] == 0xE1 && sig[15] == 0xE2
	&& (sig[5] == 0xBC || sig[5] == 0xBF || sig[5] == 0xFB
	|| sig[5] == 0xFC || sig[5] == 0xFD))
		return "video/x-gxf";

	/* http://samples.mplayerhq.hu/game-formats/smjpeg/SMJPEG-format.txt */
	if (nread > 7 && sig[0] == 0x00 && sig[1] == 0x0A && sig[2] == 'S'
	&& sig[3] == 'M'  && sig[4] == 'J' && sig[5] == 'P' && sig[6] == 'E'
	&& sig[7] == 'G')
		return "video/x-mjpeg";

	if (nread > 7 && sig[0] == 'P' && sig[1] == 'S' && sig[2] == 'M'
	&& sig[3] == 'F') /* Playstation Portable Movie Format */
		return "video/x-sony-psmf";
	/* FFmpeg: libavformat/pmpdec.c */
	if (nread > 8 && sig[0] == 'p' && sig[1] == 'm' && sig[2] == 'p'
	&& sig[3] == 'm' && LE_U32(sig + 4) == 1)
		return "video/x-sony-pmp";
	if (nread > 8 && BE_U32(sig) == 0x10
	&& (LE_U32(sig + 4) & 0xFFFFFFF0) == 0x00) {
		const uint8_t v = (sig[4] & 0x0F);
		if (v == 2 || v == 3 || v == 8 || v == 9)
			return "image/x-sony-tim";
	}

	if (nread > 2 && (sig[0] == 'C' || sig[0] == 'F') && sig[1] == 'W'
	&& sig[2] == 'S')
		return "application/vnd.adobe.flash.movie";

	if (nread > 3 && sig[0] == '.' && sig[1] == 'r' && sig[2] == 'a'
	&& sig[3] == 0xFD)
		return "audio/x-pn-realaudio";

	if (nread > 3 && ((sig[0] == '.' && sig[1] == 'R' && sig[2] == 'M'
	&& sig[3] == 'F') || (sig[0] == 'R' && sig[1] == 'A' && sig[2] == 0xFF
	&& sig[3] == 0xFF)))
		return "application/vnd.rn-realmedia";

	/* https://wiki.multimedia.cx/index.php?title=Scream_Tracker_3_Module */
	if (nread > 47 && sig[44] == 'S' && sig[45] == 'C' && sig[46] == 'R'
	&& sig[47] == 'M' && sig[28] == 0x1A)
		return "audio/x-s3m";
	if (nread > 79 && sig[76] == 'S' && sig[77] == 'C' && sig[78] == 'R'
	&& (sig[79] == 'S' || sig[79] == 'I') && sig[0] <= 0x02)
		return "audio/x-st3";

	if (nread > 3 && sig[0] == 'U' && sig[1] == 'H' && sig[2] == 'S'
	&& sig[3] == 0x0D) /* Universal hint system */
		return "application/x-uhs";

	if (nread > 15 && sig[0] == 'B' && sig[1] == 'A' && (
	(sig[14] == 'B' && sig[15] == 'M')
	|| (sig[14] == 'C' && sig[15] == 'I')
	|| (sig[14] == 'I' && sig[15] == 'C')
	|| (sig[14] == 'C' && sig[15] == 'P')
	|| (sig[14] == 'P' && sig[15] == 'T') )) /* OS/2 bitmap array */
		return "image/x-os2-graphics";

	if (nread > 18 && sig[0] == 'C' && sig[1] == 'I'
	&& ((LE_U32(sig + 6) & 0xFF00FF00) == 0 || LE_U32(sig + 14) < 65))
		return "image/x-os2-ico";

	if (nread > 17 && (sig[2] == 1 || sig[2] == 2 || sig[2] == 3
	|| sig[2] == 9 || sig[2] == 10 || sig[2] == 11 || sig[2] == 32
	|| sig[2] == 33) && is_tga_image(sig, nread) == 1)
		return "image/x-tga";

	/* http://fileformats.archiveteam.org/wiki/Award_BIOS_logo */
	if (nread >= 17) {
		const size_t columns = (size_t)sig[0];
		const size_t rows = (size_t)sig[1];
		if (columns <= 80 && rows <= 25
		&& (size_t)file_size == 2 + columns * rows * 15 + 70)
			return "image/x-award-bioslogo";
	}
	if (nread > 6 && sig[0] == 'A' && sig[1] == 'W' && sig[2] == 'B'
	&& sig[3] == 'M' && LE_U16(sig + 4) < 1981)
		return "image/x-award-bioslogo2";

	if (nread > 7 && sig[4] == 'i' && sig[5] == 'd' && ((sig[6] == 's'
	&& sig[7] == 'c') || (sig[6] == 'a'	&& sig[7] == 't')))
		return "image/x-quicktime";

	if (nread > 7 && ((sig[4] == 'm' && sig[5] == 'o' && sig[6] == 'o'
	&& sig[7] == 'v')
	|| (sig[4] == 'm' && sig[5] == 'd' && sig[6] == 'a' && sig[7] == 't')
	|| (sig[4] == 'f' && sig[5] == 'r' && sig[6] == 'e' && sig[7] == 'e')
	|| (sig[4] == 's' && sig[5] == 'k' && sig[6] == 'i' && sig[7] == 'p')
	|| (sig[4] == 'w' && sig[5] == 'i' && sig[6] == 'd' && sig[7] == 'e')))
		return "video/quicktime";
	/* https://wiki.multimedia.cx/index.php/QuickTime_container#mvhd */
	/* Note: Should be preceded by "moov" at 0x04 */
	if (nread > 15 && sig[12] == 'm' && sig[13] == 'v' && sig[14] == 'h'
	&& sig[15] == 'd')
		return "video/quicktime";

	/* FFmpeg: libavformat/avidec.c (VP5 codec) */
	if (nread > 115 && sig[0] == 'O' && sig[1] == 'N' && sig[2] == '2'
	&& sig[3] == ' ' && sig[8] == 'O' && sig[9] == 'N'
	&& sig[10] == '2' && sig[11] == 'f')
		return "video/x-msvideo";

	/* http://www.textfiles.com/programming/FORMATS/animfile.txt */
	if (nread > 19 && sig[0] == 'L' && sig[1] == 'P' && sig[2] == 'F'
	&& sig[3] == ' ' && sig[16] == 'A' && sig[17] == 'N' && sig[18] == 'I'
	&& sig[19] == 'M')
		return "video/x-anim";

	if (nread > 2 && sig[0] == 'A' && sig[1] == 'M' && sig[2] == 'V')
		return "video/x-mtv";

	/* https://github.com/FFmpeg/FFmpeg/blob/master/libavformat/msnwc_tcp.c */
	if (nread > 15 && sig[12] == 'M' && sig[13] == 'L' && sig[14] == '2'
	&& sig[15] == '0') /* MSN Messenger webcam video */
		return "video/x-mimic-ml20";

	if (nread > 17 && sig[3] == 0x0D && sig[4] == 0x0A && sig[5] == 'V'
	&& memcmp(sig + 5, "Version:Vivo", 12) == 0)
		return "video/vnd.vivo";

	if (nread > 7 && sig[0] == 'N' && sig[1] == 'X' && sig[2] == 'V'
	&& sig[3] == ' ' && sig[4] == 'F' && sig[5] == 'i' && sig[6] == 'l'
	&& sig[7] == 'e')
		return "video/x-nxv";

	/* http://fileformats.archiveteam.org/wiki/Imageiio/imaginfo_(Ulead) */
	if (nread > 4 && sig[0] == 'I' && sig[1] == 'I' && sig[2] == 'O') {
		if (sig[3] == '1' && sig[4] == '$') return "image/x-ulead-pe3";
		if (sig[3] == '2' && sig[4] == 'H') return "image/x-ulead-pe4";
	}

	/* LucasArts SMUSH video (v1 and v2) */
	if (nread > 11 && sig[0] == 'A' && sig[1] == 'N' && sig[2] == 'I'
	&& sig[3] == 'M' && sig[8] == 'A' && sig[9] == 'H' && sig[10] == 'D'
	&& sig[11] == 'R')
		return "video/x-san";
	if (nread > 11 && sig[0] == 'S' && sig[1] == 'A' && sig[2] == 'N'
	&& sig[3] == 'M' && sig[8] == 'S' && sig[9] == 'H' && sig[10] == 'D'
	&& sig[11] == 'R')
		return "video/x-san2";

	/* http://fileformats.archiveteam.org/wiki/3D_Movie_Maker */
	if (nread > 7 && sig[0] == 'C' && sig[1] == 'H' && sig[2] == 'N'
	&& sig[3] == '2' && sig[4] == ' ' && sig[5] == 'C' && sig[6] == 'O'
	&& sig[7] == 'S')
		return "video/x-3d-movie-maker";

	if (nread > 3 && sig[0] == 'D' && sig[1] == 'E' && sig[2] == 'X'
	&& sig[3] == 'A')
		return "video/x-dxa";
	if (nread > 3 && sig[0] == 'D' && sig[1] == 'X' && sig[2] == 'P'
	&& sig[3] == '1')
		return "image/x-dxp";

	if (nread > 3 && sig[0] == 0x94 && sig[1] == 0x19 && sig[2] == 0x11
	&& sig[3] == 0x29)
		return "video/x-gdv";

	if (nread > 35 && sig[0] == 'C' && sig[1] == 'R' && sig[2] == 'I'
	&& sig[3] == 'D' && sig[32] == '@' && sig[33] == 'U' && sig[34] == 'T'
	&& sig[35] == 'F')
		return "video/x-scaleform";

	if (nread > 3 && sig[0] == 'M' && sig[1] == 'X' && sig[2] == 'R'
	&& sig[3] == 'I' && sig[4] == 'F' && sig[5] == 'F' && sig[6] == '6'
	&& sig[7] == '4')
		return "video/x-magix";

	/* https://temlib.org/AtariForumWiki/index.php/Video_Master_file_format */
	if (nread > 23 && sig[0] == 'V' && sig[1] == 'M' && sig[2] == 'A'
	&& sig[3] == 'S' && sig[4] == '1'
	&& (sig[20] + sig[21] + sig[22] + sig[23]) == 0x00)
		return "video/x-video-master";

	/* http://fileformats.archiveteam.org/wiki/BLP */
	if (nread > 3 && sig[0] == 'B' && sig[1] == 'L' && sig[2] == 'P'
	&& sig[3] == '2')
		return "image/x-blp";

	if (nread > 19 && sig[0] == 'F' && sig[1] == 'I' && sig[2] == 'L'
	&& sig[3] == 'M' && sig[16] == 'F' && sig[17] == 'D' && sig[18] == 'S'
	&& sig[19] == 'C')
		return "video/x-sega-film";

	/* https://multimedia.cx/mirror/idcin.html */
	if (nread > 32 && LE_U32(sig) == 320 && LE_U32(sig + 4) == 240
	&& is_id_cin_video(sig, nread) == 1)
		return "video/x-id-cin";

	/* https://multimedia.cx/mirror/idroq.txt */
	if (nread > 7 && sig[0] == 0x84 && sig[1] == 0x10 && sig[2] == 0xFF
	&& sig[3] == 0xFF && sig[4] == 0xFF && sig[5] == 0xFF
	&& sig[6] == 0x1E && sig[7] == 0x00)
		return "video/x-id-roq";

	/* https://wiki.multimedia.cx/index.php/HNM */
	if (nread > 3 && sig[0] == 'H' && sig[1] == 'N' && sig[2] == 'M'
	&& (sig[3] == '0' || sig[3] == '1' || sig[3] == '4' || sig[3] == '6'))
		return "video/x-cryo-hnm";

	if (nread > 13 && sig[7] == '*' && sig[8] == '*' && sig[9] == 'A'
	&& sig[10] == 'C' && sig[11] == 'E' && sig[12] == '*' && sig[13] == '*')
		return "application/x-ace-compressed";

	if (nread > 1 && sig[0] == 0x60 && sig[1] == 0xEA)
		return "application/x-arj";

	if (nread > 4 && sig[0] == 'A' && sig[1] == 'r' && sig[2] == 'C'
	&& sig[3] == 0x01 && sig[4] == 0x00)
		return "application/x-freearc";

	if (nread > 1 && sig[0] == 0x1F && sig[1] == 0x9D)
		return "application/x-compress";

	if (nread > 6 && sig[2] == '-' && sig[3] == 'l' && sig[4] == 'h'
	&& (sig[5] >= '0' && sig[5] <= '5') && sig[6] == '-')
		return "application/x-lzh-compressed";

	if (nread > 17 && sig[0] == 'Z' && sig[1] == 'O' && sig[2] == 'O'
	&& sig[3] == ' ' && sig[5] == '.' && sig[9] == 'A'
	&& memcmp(sig + 9, "Archive.", 8) == 0)
		return "application/x-zoo";

	if (nread > 3 && sig[0] == 'S' && sig[1] == 'Z' && sig[2] == 'D'
	&& sig[3] == 'D')
		return "application/x-ms-compress-szdd";

	if (nread > 3 && sig[0] == 0x01 && sig[1] == 'f' && sig[2] == 'c'
	&& sig[3] == 'p')
		return "application/x-font-pcf";

	if (nread > 3 && sig[0] == 'M' && sig[1] == 'O' && sig[2] == 'V'
	&& sig[3] == 'I')
		return check_movi_magic(sig + 4, nread - 4);

	if (nread > 3 && sig[0] == 0xCF && sig[1] == 0xAD && sig[2] == 0x12
	&& sig[3] == 0xFE) /* Outlook Express DBX file */
		return "application/x-ms-dbx";

	if (nread > 3 && sig[0] == 0xFF && sig[1] == 'W' && sig[2] == 'P'
	&& sig[3] == 'C') {
		// sig + 4 = "\x10\x00\x00\x00\x01\x16\x01\x00" -> image/x-wordperfect-graphics
		if (nread > 12 && sig[4] == 0x10 && sig[9] == 0x16
		&& memcmp(sig + 4, "\x10\x00\x00\x00\x01\x16\x01\x00", 8) == 0)
			return "image/x-wordperfect-graphics";
		return "application/vnd.wordperfect";
	}

	if (nread >= 7 && sig[0] == 'W' && memcmp(sig, "WordPro", 7) == 0)
		return "application/vnd.lotus-wordpro";

	if (nread > 7 && sig[0] == 0x00 && sig[1] == 0x00 && sig[2] == 0x02
	&& sig[6] > 0x00 && sig[7] == 0x00)
		return "application/vnd.lotus-1-2-3";

	if (nread > 113 && sig[0] == 0x01 && sig[1] == 0xFE && sig[112] == 0x01
	&& sig[113] == 0x00)
		return "application/vnd-ms-works";

	if (nread > 3 && sig[0] == 'S' && sig[1] == 'G' && sig[2] == 'A'
	&& sig[3] == '3')
		return "application/x-stargallery-sdg";

	if (nread > 19 && sig[0] == 0x00 && sig[1] == 0x00 && sig[2] == 0x01
	&& sig[3] == 0x00 && sig[4] == 0x00
	&& sig[19] != 0x1C /* Avoid conflict with JBIG1 files*/)
		return "application/x-dfont"; /* MacOS font */

	if (nread > 4 && sig[0] == 'M' && sig[1] == 'M' && sig[2] == 'M'
	&& sig[3] == 'D')
		return "application/vnd.smaf";

	/* See FFmpeg: libavformat/dss.c */
	if (nread > 4 && (sig[0] == 0x02 || sig[0] == 0x03) && sig[1] == 'd'
	&& sig[2] == 's' && sig[3] == 's')
		return "audio/x-dss";

	if (nread >= 19 && sig[0] == 'C'
	&& memcmp(sig, "Creative Voice File", 19) == 0)
		return "audio/x-voc";

	if (nread > 3 && sig[0] == 'C' && sig[1] == 'T' && sig[2] == 'M'
	&& sig[3] == 'F')
		return "audio/x-cmf";

	if (nread > 28 && ((sig[20] == 'B' && memcmp(sig + 20, "BMOD2STM", 8) == 0)
	|| (sig[20] == '!' && (memcmp(sig + 20, "!Scream!", 8) == 0
	|| memcmp(sig + 20, "!SCREAM!", 8) == 0))))
		return "audio/x-stm";

	if (nread > 3 && sig[0] == 'a' && sig[1] == 'j' && sig[2] == 'k'
	&& sig[3] == 'g')
		return "audio/x-shorten";

	if (nread > 3 && sig[0] == 'c' && sig[1] == 'a' && sig[2] == 'f'
	&& sig[3] == 'f')
		return "audio/x-caf";

	if (nread > 3 && sig[0] == 0x80 && sig[1] == 0x00) {
		const uint16_t l = BE_U16(sig + 2);
		const size_t v = l >= 2 ? (size_t)l - 2 : 0;
		if (nread >= v + 6 && memcmp(sig + v, "(c)CRI", 6) == 0)
			return "audio/x-adx";
	}

	if (nread >= 16 && sig[0] == 'E' && sig[8] == ' ' && sig[9] == 'M'
	&& memcmp(sig, "Extended Module:", 16) == 0)
		return "audio/x-mod";
	if (nread > 1083 && IS_ALNUM(sig[1080])) {
		const uint8_t *s = sig + 1080;
		if ((s[0] == 'M' && s[1] == '.' && s[2] == 'K' && s[3] == '.')
		|| (s[0] == 'M' && s[1] == '!' && s[2] == 'K' && s[3] == '!')
		|| (s[0] == 'F' && s[1] == 'L' && s[2] == 'T' && s[3] == '4')
		|| (s[0] == 'F' && s[1] == 'L' && s[2] == 'T' && s[3] == '8')
		|| (s[0] == '4' && s[1] == 'C' && s[2] == 'H' && s[3] == 'N')
		|| (s[0] == '4' && s[1] == 'C' && s[2] == 'H' && s[3] == 'N')
		|| (s[0] == '6' && s[1] == 'C' && s[2] == 'H' && s[3] == 'N')
		|| (s[0] == '8' && s[1] == 'C' && s[2] == 'H' && s[3] == 'N')
		|| (s[0] == 'C' && s[1] == 'D' && s[2] == '8' && s[3] == '1')
		|| (s[0] == 'O' && s[1] == 'K' && s[2] == 'T' && s[3] == 'A')
		|| (s[0] == '1' && s[1] == '6' && s[2] == 'C' && s[3] == 'N')
		|| (s[0] == '3' && s[1] == '2' && s[2] == 'C' && s[3] == 'N'))
			return "audio/x-mod";
	}
	if (nread > 1083 && sig[1080] == '!' && sig[1081] == 'P'
	&& sig[1082] == 'M' && sig[1083] == '!')
		return "audio/x-mod";

	/* https://www.mmsp.ece.mcgill.ca/Documents/AudioFormats/IRCAM/IRCAM.html */
	if (nread > 3 && ((sig[0] == 'd' && sig[1] == 0xA3 && sig[2] <= 0x04
	&& sig[3] == 0x00) || (sig[2] == 0xA3 && sig[3] == 'd' && sig[1] <= 0x03
	&& sig[0] == 0x00)))
		return "audio/x-ircam";

	if (nread > 3 && ((sig[0] == 'f' && sig[1] == 'a' && sig[2] == 'p'
	&& sig[3] == ' ') || (sig[0] == ' ' && sig[1] == 'p' && sig[2] == 'a'
	&& sig[3] == 'f'))) /* Ensoniq Paris audio file format */
		return "audio/x-paf";

	/* http://fileformats.archiveteam.org/wiki/Avr */
	if (nread > 15 && sig[0] == '2' && sig[1] == 'B' && sig[2] == 'I'
	&& sig[3] == 'T' && sig[14] == 0x00 && (sig[15] == 0x08 || sig[15] == 0x10))
		return "audio/x-avr";

	/* Sony ATRAC Advanced lossless (oma, aa3) */
	if (nread > 2 && sig[0] == 'e' && sig[1] == 'a' && sig[2] == '3') {
		const size_t v = id3v2_tag_size(sig, nread, file_size);
		if (v + 5 < nread && sig[v] == 'E' && sig[v + 1] == 'A'
		&& sig[v + 2] == '3' && sig[v + 4] == 0x00 && sig[v + 5] == 0x60)
			return "audio/x-oma"; /* MIME type used by FFmpeg */
	}
	if (nread > 5 && sig[0] == 'E' && sig[1] == 'A' && sig[2] == '3'
	&& sig[4] == 0x00 && sig[5] == 0x60)
		return "audio/x-oma";

	if (nread > 5 && sig[0] == 'D' && sig[1] == 'V' && sig[2] == 'S'
	&& sig[3] == 'M' && sig[4] == 0x00 && sig[5] == 0x00)
		return "audio/x-dvsm";

	/* http://svr-www.eng.cam.ac.uk/reports/ajr/TR192/node11.html */
	if (nread > 6 && sig[0] == 'N' && sig[1] == 'I' && sig[2] == 'S'
	&& sig[3] == 'T' && sig[4] == '_' && sig[5] == '1' && sig[6] == 'A')
		return "audio/x-nist";

	if (nread > 6 && sig[0] == 'L' && sig[1] == 'M' && sig[2] == '8'
	&& sig[3] == '9' && sig[4] == '5' && sig[5] == '3')
		return "audio/x-txw";

	if (nread >= 13 && sig[0] == 'A' && sig[1] == 'L' && sig[2] == 'a'
	&& memcmp(sig, "ALawSoundFile", 13) == 0)
		return "audio/x-psion-wve";

	if (nread >= 15 && sig[0] == 'S' && sig[1] == 'C' && sig[2] == '6'
	&& sig[3] == '8' && memcmp(sig, "SC68 Music-file", 15) == 0)
		return "audio/x-atari-sc68";

	if (nread > 3 && sig[0] == 'B' && sig[1] == 'u' && sig[2] == 'z'
	&& sig[3] == 'z')
		return "audio/x-buzz";

	/* https://ffmpeg.org/doxygen/7.1/libavformat_2hcom_8c_source.html */
	if (nread > 131 && sig[65] == 'F' && sig[66] == 'S' && sig[67] == 'S'
	&& sig[68] == 'D' && sig[128] == 'H' && sig[129] == 'C'
	&& sig[130] == 'O' && sig[131] == 'M')
		return "audio/x-hcom";

	if (nread > 3 && sig[0] == 'O' && sig[1] == 'F' && sig[2] == 'R'
	&& sig[3] == ' ')
		return "audio/x-optimfrog";

	if (nread > 16 && sig[0] == 'M' && sig[1] == 'S' && sig[2] == '_'
	&& sig[3] == 'V' && sig[4] == 'O' && sig[5] == 'I' && sig[6] == 'C'
	&& sig[7] == 'E')
		return "audio/x-dvf";

	if (nread >= 17 && sig[0] == 'S' && sig[5] == ' ' && sig[13] == 'D'
	&& memcmp(sig, "SOUND SAMPLE DATA", 17) == 0)
		return "audio/x-smp";

	if (nread > 4 && sig[0] == 'I' && sig[1] == 'R' && sig[2] == 'E'
	&& sig[3] == 'Z' && sig[4] == 0x00)
		return "audio/x-rmf";

	if (nread > 3 && sig[0] == 'M' && sig[1] == 'P' && (sig[2] == '+'
	|| (sig[2] == 'C' && sig[3] == 'K')))
		return "audio/x-musepack";

	/* https://wiki.multimedia.cx/index.php?title=La_Lossless_Audio */
	if (nread > 11 && sig[0] == 'L' && sig[1] == 'A' && sig[2] == '0'
	&& sig[8] == 'W' && sig[9] == 'A' && sig[10] == 'V' && sig[11] == 'E')
		return "audio/x-la";

	if (nread >= 20 && sig[0] == 'E' && sig[8] == ' ' && sig[9] == 'I'
	&& memcmp(sig, "Extended Instrument:", 20) == 0)
		return "audio/x-xi";

	/* https://www.purose.net/befis/download/nezplug/kssspec.txt */
	if (nread > 4 && sig[0] == 'K' && sig[1] == 'S' &&
	((sig[2] == 'C' && sig[3] == 'C') || (sig[2] == 'S' && sig[3] == 'X')))
		return "audio/x-kss";

	/* Sample Dump eXchange. See FFmpeg: libavformat/sdxdec.c */
	if (nread > 4 && sig[0] == 'S' && sig[1] == 'D' && sig[2] == 'X'
	&& sig[3] == ':')
		return "audio/x-sdx";

	/* http://fileformats.archiveteam.org/wiki/DiamondWare_Digitized */
	if (nread > 12 && sig[0] == 'D' && sig[7] == 'W' && sig[11] == ' '
	&& memcmp(sig, "DiamondWare ", 12) == 0) {
		if (sig[12] == 'D') return "audio/x-dwd";
		if (sig[12] == 'M') return "audio/x-dwm";
	}

	/* http://www.textfiles.com/programming/FORMATS/far-form.txt */
	if (nread > 3 && sig[0] == 'F' && ((sig[1] == 'A' && sig[2] == 'R')
	|| (sig[1] == 'S' && sig[2] == 'M')) && sig[3] == 0xFE)
		return "audio/x-far";

	if (nread > 15 && sig[0] == 'N' && sig[1] == 'V' && sig[2] == 'F'
	&& sig[3] == ' ' && sig[12] == 'V' && sig[13] == 'F' && sig[14] == 'M'
	&& sig[15] == 'T')
		return "audio/x-nvf";

	if (nread > 14 && sig[0] == 'T' && sig[1] == 'W' && sig[2] == 'I'
	&& sig[3] == 'N' && memcmp(sig, "TWIN97012000", 12) == 0)
		return "audio/x-vqf";

	/* http://lclevy.free.fr/exotica/ahx/ahxformat.txt */
	if (nread > 3 && sig[0] == 'T' && sig[1] == 'H' && sig[2] == 'X'
	&& (sig[3] == 0x00 || sig[3] == 0x01))
		return "audio/x-ahx";

	if (nread > 3 && sig[0] == 'D' && sig[1] == 'D' && sig[3] == 'F') {
		if (sig[2] == 'M') return "audio/x-xtracker-dmf";
		if (sig[2] == 'S') return "audio/x-dsf";
	}

	if (nread > 3 && sig[0] == 'D' && sig[1] == '.' && sig[2] == 'T'
	&& sig[3] == '.')
		return "audio/x-dtm";

	if (nread > 3 && sig[0] == 'S' && sig[1] == 'R' && sig[2] == 'F'
	&& sig[3] == 'S')
		return "audio/x-acid-sfr";

	/* https://wiki.multimedia.cx/index.php/Maxis_XA */
	if (nread > 3 && sig[0] == 'X' && sig[1] == 'A'
	&& (sig[2] == 'I' || sig[2] == 'J') && sig[3] == 0x00)
		return "audio/x-maxis-xa";

	/* Adlib Tracker II modules
	 * https://github.com/dmitrysmagin/a2t_play/blob/master/src/a2t.c */
	if (nread > 15 && sig[0] == '_' && sig[1] == 'A' && sig[2] == '2') {
		if (sig[3] == 'm' && memcmp(sig, "_A2module_", 10) == 0)
			return "audio/x-a2m";
		if (sig[3] == 't' && memcmp(sig, "_A2tiny_module_", 15) == 0)
			return "audio/x-a2t";
	}

	if (nread > 15 && sig[12] == 'S' && sig[13] == 'N' && sig[14] == 'D'
	&& sig[15] == 'H')
		return "audio/x-atari-sndh";

	if (nread > 14 && sig[0] == 'I' && sig[10] == 'A' && sig[19] == 0x1A
	&& memcmp(sig, "Interplay ACMP Data\x1A", 20) == 0)
		return "audio/x-interplay-acmp";

	/* https://wiki.multimedia.cx/index.php/Sierra_Audio */
	if (nread > 7 && (sig[0] == 0x8D || sig[0] == 0x0D || sig[0] == 0x16)
	&& sig[2] == 'S' && sig[3] == 'O' && sig[4] == 'L' && sig[5] == 0x00) {
		if (sig[0] == 0x16) return "audio/x-sierra-rbt";
		return "audio/x-sierra-sol";
	}
	/* https://wiki.multimedia.cx/index.php/VMD */
	if (nread > 805 && LE_U16(sig) == 0x330 - 2 && LE_U16(sig + 804) == 22050) {
		const uint16_t w = LE_U16(sig + 12);
		const uint16_t h = LE_U16(sig + 14);
		if (w > 0 && w <= 2048 && h > 0 && h <= 2048)
			return "video/x-sierra-vmd";
	}

	/* https://wiki.amigaos.net/wiki/Bars_and_Pipes_Professional */
	if (nread > 3 && sig[0] == 'B' && sig[1] == 'R' && sig[2] == 'P'
	&& sig[3] == 'P')
		return "video/x-brpp";

	/* https://wiki.multimedia.cx/index.php/SIFF */
	if (nread > 12 && sig[0] == 'S' && sig[1] == 'I' && sig[2] == 'F'
	&& sig[3] == 'F') {
		if (memcmp(sig + 8, "SOUN", 4) == 0) return "audio/x-siff";
		return "video/x-siff";
	}

	/* https://www.fileformat.info/format/atari/egff.h */
	if (nread > 128 && sig[0] == 0xFE && (sig[1] == 0xDB || sig[1] == 0xDC)
	&& is_seq_video(sig, nread) == 1)
		return "video/x-atari-cyber-paint"; /* .seq */

	/* https://wiki.multimedia.cx/index.php/IBM_PhotoMotion */
	if (nread > 0x1a + 6 && LE_U16(sig) == 0x00 && LE_U16(sig + 8) == 10
	&& LE_U16(sig + 10) == 0x13 && LE_U16(sig + 16) == 0x02
	&& LE_U16(sig + 18) == 0x05 && LE_U16(sig + 20) == 0x0C
	&& LE_U16(sig + 22) == 0x0D && LE_U16(sig + 24) == 0x0E
	&& LE_U16(sig + 26) == 0x0F)
		return "video/x-ibm-photomotion";

	/* https://wiki.multimedia.cx/index.php/Electronic_Arts_Formats */
	if (nread > 3 && sig[0] == 'S' && sig[1] == 'C'
	&& (sig[2] == 'H' || sig[2] == 'C' || sig[2] == 'D' || sig[2] == 'L'
	|| sig[2] == 'E') && sig[3] == 'l')
		return "audio/x-electronic-arts";
	if (nread > 3 && sig[0] == '1' && sig[1] == 'S' && sig[2] == 'N'
	&& (sig[3] == 'h' || sig[3] == 'd' || sig[3] == 'l' || sig[3] == 'e'))
		return "audio/x-electronic-arts";
	if (nread > 3 && sig[0] == 'M' && sig[1] == 'V' && sig[2] == 'I'
	&& (sig[3] == 'h' || sig[3] == 'f' || sig[3] == 'e'))
		return "video/x-electronic-arts";
	if (nread > 3 && sig[0] == 'M' && sig[1] == 'A' && sig[2] == 'D'
	&& (sig[3] == 'k' || sig[3] == 'm' || sig[3] == 'e'))
		return "video/x-electronic-arts";
	if (nread > 3 && sig[0] == 'M' && sig[1] == 'V' &&
	((sig[2] == 'h' && sig[3] == 'd') || (sig[2] == '0' && sig[3] == 'K')
	|| (sig[2] == '0' && sig[3] == 'F')))
		return "video/x-electronic-arts";
	if (nread > 3 && (sig[0] == 'k' || sig[0] == 'f') && sig[1] == 'V'
	&& sig[2] == 'G' && sig[3] == 'T')
		return "video/x-electronic-arts";
	if (nread > 3 && sig[0] == 'M' && sig[1] == 'P' && sig[2] == 'C'
	&& sig[3] == 'h')
		return "video/x-electronic-arts";
	if (nread > 3 && sig[0] == 'm' && sig[1] == 'T' && sig[2] == 'C'
	&& sig[3] == 'D')
		return "video/x-electronic-arts";
	if (nread > 3 && sig[0] == 'p' && sig[1] == 'I' && sig[2] == 'Q'
	&& sig[3] == 'T')
		return "video/x-electronic-arts";
	if (nread > 3 && sig[0] == 'p' && sig[1] == 'Q' && sig[2] == 'T'
	&& sig[3] == 'G')
		return "video/x-electronic-arts";
	if (nread > 3 && sig[0] == 'T' && sig[1] == 'G' && sig[2] == 'Q'
	&& sig[3] == 's')
		return "video/x-electronic-arts";

	if (nread >= 21 && sig[0] == 'I' && sig[10] == 'M' && sig[18] == 0x1A
	&& memcmp(sig, "Interplay MVE File\x1A\0\x1A", 21) == 0)
		return "video/x-interplay-mve";

	/* FFmpeg: libavformat/betsoftvid.c */
	if (nread > 4 && sig[0] == 'V' && sig[1] == 'I' && sig[2] == 'D'
	&& sig[3] == 0x00)
		return "video/x-bethsoftvid";

	/* https://web.archive.org/web/20150503020113/http://hackipedia.org/File%20formats/Music/Sample%20based/text/Digitrakker%203.0%20MDL%20module%20format.cp437.txt.utf-8.txt */
	if (nread > 3 && sig[0] == 'D' && sig[1] == 'M' && sig[2] == 'D'
	&& sig[3] == 'L')
		return "audio/x-digitrakker-mdl";
	if (nread > 3 && sig[0] == 'S' && sig[1] == 'O' && sig[2] == 'N'
	&& sig[3] == 'G')
		return "audio/x-digitrakker-dtm";

	/* http://fileformats.archiveteam.org/wiki/Epic_Megagames_MASI */
	if (nread > 3 && sig[0] == 'P' && sig[1] == 'S' && sig[2] == 'M'
	&& sig[3] == ' ')
		return "audio/x-protracker-psm";

	if (nread > 3 && sig[0] == 'M' && sig[1] == 'T' && sig[2] == '2'
	&& sig[3] == '0')
		return "audio/x-madtracker-mt2";

	if (nread > 3 && sig[0] == 'R' && sig[1] == 'T' && sig[2] == 'M'
	&& sig[3] == 'M')
		return "audio/x-realtracker-rtm";

	if (nread > 5 && sig[0] == 'A' && sig[1] == 'M' && sig[2] == 'S'
	&& sig[3] == 'h' && sig[4] == 'd' && sig[5] == 'r')
		return "audio/x-velvet-ams";

	if (nread > 6 && sig[0] == 'E' && sig[1] == 'x' && sig[2] == 't'
	&& sig[3] == 'r' && sig[4] == 'e' && sig[5] == 'm' && sig[6] == 'e')
		return "audio/x-extreme-tracker-ams";

	/* FFmpeg: libavformat/westwood_aud.c */
	if (nread >= 20 && sig[16] == 0xAF && sig[17] == 0xDE
	&& (sig[11] == 0x63 || sig[11] == 0x01) && !(sig[10] & 0xFC)) {
		const uint16_t sample_rate = LE_U16(sig);
		if (sample_rate >= 4000 && sample_rate <= 48000)
			return "audio/x-westwood-aud";
	}

	/* FFmpeg: libavformat/ipudec.c */
	if (nread > 3 && sig[0] == 'i' && sig[1] == 'p' && sig[2] == 'u'
	&& sig[3] == 'm')
		return "video/x-ipu";

	/* Machintosh files begin with XFIR (little endian) instead of RIFX. */
	if (nread > 12 && sig[0] == 'X' && sig[1] == 'F' && sig[2] == 'I'
	&& sig[3] == 'R'
	&& ((sig[8] == '3' && sig[9] == '9' && sig[10] == 'V' && sig[11] == 'M')
	|| (sig[8] == '5' && sig[9] == '9' && sig[10] == 'C' && sig[11] == 'M')
	|| (sig[8] == 'M' && sig[9] == 'D' && sig[10] == 'G' && sig[11] == 'F')
	|| (sig[8] == 'C' && sig[9] == 'D' && sig[10] == 'G' && sig[11] == 'F')))
		return "video/x-shockwave-director";

	/* https://temlib.org/AtariForumWiki/index.php/Animatic_Film_file_format */
	if (nread > 51 && sig[48] == 0x27 && sig[49] == 0x18 && sig[50] == 0x28
	&& sig[51] == 0x18 && BE_U16(sig + 34) <= 99 && BE_U16(sig + 38) <= 2)
		return "video/x-atari-animatic";

	/* https://wiki.multimedia.cx/index.php/ETV */
	if (nread > 4 && sig[0] == 'E' && sig[1] == 'T' && sig[2] == 'V'
	&& sig[3] == 0x0A)
		return "video/x-etv";

	/* http://justsolve.archiveteam.org/wiki/Lotus_ScreenCam_movie */
	if (nread > 4 && sig[0] == 0x80 && sig[1] == 'S' && sig[2] == 'C'
	&& sig[3] == 'M' && sig[4] == 'O' && sig[5] == 'V')
		return "video/x-lotus-screencam";

	/* https://wiki.multimedia.cx/index.php/CDXL */
	if (nread > 32 && sig[0] <= 0x02 && !BE_U32(sig + 24) && !BE_U32(sig + 28)
	&& (sig[19] == 4 || sig[19] == 6 || sig[19] == 8 || sig[19] == 24)
	&& is_cdxl_video(sig, nread) == 1)
			return "video/x-cdxl";

	if (nread > 3 && sig[0] == '2' && sig[1] == 'T' && sig[2] == 'S'
	&& sig[3] == 'F')
		return "video/x-future-vision";

	if (nread > 2 && sig[0] == 'R' && sig[1] == 'K' && sig[2] == 'A')
		return "application/x-rka";

	if (nread > 3 && sig[0] == 'N' && sig[1] == 'G' && sig[2] == 0x00
	&& sig[3] == 0x01 && LE_U32(sig + 2) == 0x00000100)
		return "application/x-norton-guide";

	if (nread > 4 && sig[0] == 'A' && sig[1] == 'L' && sig[2] == 'Z'
	&& sig[3] == 0x01 && sig[4] == 0x0A)
		return "application/x-alz";

	if (nread > 2 && sig[0] == 0x60 && (sig[1] == 0x1A || sig[1] == 0x1B)
	&& sig[2] == 0x00)
		return "application/x-atari-exec";
	if (nread > 6 && sig[0] == 0x0E && sig[1] == 0x0F && BE_U16(sig + 4) < 0x02)
		return "application/x-atari-msa";

	if (nread > 3 && sig[0] == 0xB1 && sig[1] == 0x68 && sig[2] == 0xDE
	&& sig[3] == 0x3A)
		return "image/x-dcx";

	if (nread > 3 && sig[0] == 'D' && sig[1] == 'D' && sig[2] == 'S'
	&& sig[3] == ' ')
		return "image/x-dds";

	if (nread > 120 && sig[0] == 0x07 && LE_U16(sig + 11) == 320
	&& is_mvi_video(sig, nread, file_size) == 1)
		return "video/x-mvi";

	if (nread > 6 && sig[0] == 'P' && sig[1] == '7' && sig[2] == ' '
	&& sig[3] == '3' && sig[4] == '3' && sig[5] == '2' && sig[6] == 0x0A)
	return "image/x-xv-thumbnail";

	if (nread > 3 && sig[1] == 'F' && sig[2] == 'I' && sig[3] == 'G') {
		if (sig[0] == '#') return "image/x-xfig";
		if (sig[0] == 0x14) return "image/x-deskmate-fig";
	}

	/* https://www.itu.int/rec/T-REC-T.88-201808-I/en */
	if (nread > 7 && sig[0] == 0x97 && sig[1] == 'J' && sig[2] == 'B'
	&& sig[3] == '2' && sig[4] == 0x0D && sig[5] == 0x0A && sig[6] == 0x1A
	&& sig[7] == 0x0A)
		return "image/x-jbig2";

	if (nread > 12 && sig[0] == '(' && sig[1] == 0x00 && sig[2] == 0x00
	&& sig[3] == 0x00 && sig[12] == 0x01)
		return "image/x-ms-bmp"; /* Or image/x-dib (Shared MIME-info) */

	if (nread > 5 && sig[0] == 0xFF && sig[1] == 0xFF && sig[2] == '0'
	&& sig[3] == 'S' && sig[4] == 'O' && sig[5] == 0x7f)
		return "image/x-atari-ged";
	/* http://fileformats.archiveteam.org/wiki/XL-Paint */
	if (nread > 3 && sig[0] == 'X' && sig[1] == 'L' && sig[2] == 'P') {
		if (sig[3] == 'B' && (file_size == 792 || file_size == 15372))
			return "image/x-atari-xl-paint";
		if (sig[3] == 'C' || sig[3] == 'M') return "image/x-atari-xl-paint";
	}
	/* http://fileformats.archiveteam.org/wiki/ColorViewSquash */
	if (nread > 3 && sig[0] == 'R' && sig[1] == 'G' && sig[2] == 'B'
	&& sig[3] == '1')
		return "image/x-atari-rgb";
	/* https://codeberg.org/zerkman/mpp#the-mpp-file-format */
	if (nread > 12 && sig[0] == 'M' && sig[1] == 'P' && sig[2] == 'P'
	&& sig[3] <= 0x03 && sig[5] + sig[6] + sig[7] == 0x00)
		return "image/x-atari-mpp";

	/* https://temlib.org/AtariForumWiki/index.php/QuantumPaint_file_format */
	if (nread > 128 && !sig[0] && !BE_U16(sig + 1)
	&& (sig[3] == 0x00 || sig[3] == 0x01 || sig[3] == 0x80 || sig[3] == 0x81)
	&& is_quantum_paint(sig, nread) == 1)
		return "image/x-atari-quantum-paint"; /* .pbx */

	/* http://fileformats.archiveteam.org/wiki/Graph2Font */
	if (nread > 6 && sig[0] == 'G' && sig[1] == '2' && sig[2] == 'F'
	&& sig[3] == 'Z' && sig[4] == 'L' && sig[5] == 'I' && sig[6] == 'B')
		return "image/x-atari-graph2font";

	/* http://fileformats.archiveteam.org/wiki/MAKIchan_Graphics */
	if (nread > 12 && sig[0] == 'M' && sig[1] == 'A' && sig[2] == 'K'
	&& sig[3] == 'I' && sig[4] == '0' && (sig[5] == '1' || sig[5] == '2')
	&& sig[7] == ' ')
		return "image/x-makichan";

	/* http://fileformats.archiveteam.org/wiki/MSP_(Microsoft_Paint_file) */
	if (nread > 32 && ((sig[0] == 'L' && sig[1] == 'i' && sig[2] == 'n'
	&& sig[3] == 'S')
	|| (sig[0] == 'D' && sig[1] == 'a' && sig[2] == 'n' && sig[3] == 'M')))
		return "image/x-mspaint";

	/* http://fileformats.archiveteam.org/wiki/Mapletown_Network */
	if (nread > 3 && sig[0] == '1' && sig[1] == '0' && sig[2] == '0'
	&& sig[3] == 0x1A)
		return "image/x-nec-ml1";
	if (nread > 3 && sig[0] == '@' && sig[1] == '@' && sig[2] == '@'
	&& sig[3] == ' ')
		return "image/x-nec-mx1";
	if (nread > 3 && sig[0] == ' ' && sig[1] == ' ' && sig[2] == 'x'
	&& sig[3] == '%')
		return "image/x-nec-nl3";
	/* RECOIL: recoil.c:RECOIL_DecodeZim */
	if (nread >= 700 && sig[0] == 'F' && sig[1] == 'O' && sig[2] == 'R'
	&& sig[3] == 'M' && sig[4] == 'A' && sig[5] == 'T' && sig[6] == '-'
	&& sig[7] == 'A' && is_nec_zim_image(sig, nread) == 1)
			return "image/x-nec-zim";

	/* http://fileformats.archiveteam.org/wiki/CompuServe_RLE */
	if (nread > 3 && sig[0] == 0x1B && sig[1] == 'G'
	&& (sig[2] == 'H' || sig[2] == 'M'))
		return "image/x-compuserve-rle";

	if (nread > 3 && sig[0] == 0x0E && sig[1] == 0x03 && sig[2] == 0x13
	&& sig[3] == 0x01)
		return "application/x-hdf";

	/* http://fileformats.archiveteam.org/wiki/PICT */
	if (nread > 527 && sig[522] == 0x00 && (sig[523] == 0x11
	|| sig[523] == 0x10) && sig[524] == 0x02 && sig[525] == 0xFF
	&& sig[526] == 0x0C && sig[527] == 0x00) /* version 2 */
		return "image/x-pict"; /* Macintosh QuickDraw */
	if (nread > 523 && (sig[522] == 0x11 || sig[522] == 0x10)
	&& sig[523] == 0x01 && !BE_U32(sig) && !BE_U32(sig + 4)) /* version 1 */
		return "image/x-pict"; /* Macintosh QuickDraw */

	/* https://www.fileformat.info/format/macpaint/egff.htm */
	if (nread > 128 && sig[0] == 0x00 && sig[65] == 'P' && sig[66] == 'N'
	&& sig[67] == 'T' && sig[68] == 'G')
		return "image/x-mac-macpaint";
	if (nread > 528 && BE_U32(sig) <= 0x000003) {
		if (BE_U32(sig + 512) == 0xB900B900 && BE_U32(sig + 516) == 0xB900B900
		&& BE_U32(sig + 520) == 0xB900B900 && BE_U32(sig + 524) == 0xB900B900)
			return "image/x-mac-macpaint";
	}

	if (nread > 8 && sig[0] == 'C' && sig[1] == 'P' && sig[2] == 'T'
	&& (sig[3] >= '0' && sig[3] <= '9') && sig[4] == 'F' && sig[5] == 'I'
	&& sig[6] == 'L' && sig[7] == 'E')
		return "image/x-corel-cpt";

	if (nread > 3 && sig[0] == 0x59 && sig[1] == 0xA6 && sig[2] == 0x6A
	&& sig[3] == 0x95)
		return "image/x-sun-raster";

	/* http://fileformats.archiveteam.org/wiki/NIFF_(Navy_Image_File_Format) */
	if (nread > 3 && sig[0] == 'I' && sig[1] == 'I' && sig[2] == 'N'
	&& sig[3] == '1')
		return "image/x-niff";

	if (nread > 3 && sig[0] == 'N' && ((sig[1] == 'I' && sig[2] == 'T')
	|| (sig[1] == 'S' && sig[2] == 'I')) && sig[3] == 'F')
		return "image/vnd.nitf"; /* National Imagery Transmission Format */

	if (nread > 4 && sig[0] == 'S' && sig[1] == 'V' && sig[2] == 'F'
	&& sig[3] == ' ' && sig[4] == 'v') /* Simple Vector Format */
		return "image/vnd.svf";

	if (nread > 2054 && sig[2048] == 'P' && sig[2049] == 'C' && sig[2050] == 'D'
	&& sig[2051] == '_' && sig[2052] == 'I' && sig[2053] == 'P'
	&& sig[2054] == 'I')
		return "image/x-photo-cd";

	if (nread > 4 && sig[0] == 0x01 && sig[1] == 0x40 && sig[2] == 0x00
	&& sig[3] == 0xC8)
		return "image/x-atari-photochrome";

	if (nread > 15 && sig[0] == 'J' && sig[1] == 'G') {
		/* http://fileformats.archiveteam.org/wiki/ART_(AOL_compressed_image) */
		if ((sig[2] == 0x03 || sig[2] == 0x04) && sig[3] == 0x0E
		&& !sig[4] && !sig[5] && !sig[6])
			return "image/x-aol-art";
		/* http://fileformats.archiveteam.org/wiki/Jigsaw_(Walter_A._Kuhn) */
		if (sig[4] <= 0x03 && !BE_U32(sig + 5) && !sig[9] && sig[10] == 0x36
		&& sig[11] == 0x04 && !sig[12] && !sig[13] && sig[14] == 0x28)
			return "image/x-jigzaw";
	}

	/* http://fileformats.archiveteam.org/wiki/JGF5_(image_format) */
	if (nread > 4 && sig[0] == 'J' && sig[1] == 'G' && sig[2] == 'F'
	&& sig[3] == '5' && sig[4] == 0x00)
		return "image/x-jgf5";

	/* https://temlib.org/AtariForumWiki/index.php/NEOchrome_file_format */
	if (file_size == 32128 && nread == BYTES_TO_READ && sig[0] == 0x00
	&& sig[1] == 0x00 && is_neochrome_image(sig, nread) == 1)
		return "image/x-atari-neochrome"; /* Standard .neo file */
	if (nread > 22 && sig[0] == 0xBA && sig[1] == 0xBE && sig[2] == 0xEB
	&& sig[3] == 0xEA && is_neochrome_animation(sig, nread) == 1)
			return "video/x-atari-neochrome"; /* Animation (.ani) */
	if (file_size == 128128 && nread > 1 && sig[0] == 0xBA && sig[1] == 0xBE)
		return "image/x-atari-neochrome"; /* Virtual canvas */

	/* http://fileformats.archiveteam.org/wiki/ComputerEyes */
	if (nread > 4 && (file_size == 192022 || file_size == 256022)
	&& sig[0] == 'E' && sig[1] == 'Y' && sig[2] == 'E' && sig[3] == 'S')
		return "image/x-atari-eyes";

	/* http://fileformats.archiveteam.org/wiki/ImageLab/PrintTechnic */
	if (nread > 5 && sig[0] == 'B' && sig[1] == '&' && sig[2] == 'W'
	&& sig[3] == '2' && sig[4] == '5' && sig[5] == '6')
		return "image/x-ilab";

	/* https://temlib.org/AtariForumWiki/index.php/Spectrum_512_Compressed_file_format */
	if (nread > 24 && sig[0] == 'S' && sig[1] == 'P' && sig[2] == 0x00
	&& sig[3] == 0x00) {
		const uint32_t data_len = BE_U32(sig + 4);
		const uint32_t color_len = BE_U32(sig + 8);
		if (data_len + color_len + 4 + 4 + 2 + 2 == file_size)
			return "image/x-spectrum-spc";
	}
	/*https://temlib.org/AtariForumWiki/index.php/Spectrum_512_file_format */
	if (file_size == 51104 && nread > 161 && sig[4] == 0x00 && sig[8] == 0x00
	&& sig[32] == 0x00) { /* First non-zero byte is at offset 160 */
		size_t i = (sig[0] == '5' && sig[1] == 'B' && sig[2] == 'I'
			&& sig[3] == 'T') ? 4 : 0;
		for (; i <= 160 && sig[i] == 0x00; i++);
		if (i == 160) return "image/x-spectrum-spu";
	}
	/* http://fileformats.archiveteam.org/wiki/SXG_(ZX_Spectrum) */
	if (nread > 3 && sig[0] == 0x7F && sig[1] == 'S' && sig[2] == 'X'
	&& sig[3] == 'G')
		return "image/x-spectrum-sxg";
	/* http://fileformats.archiveteam.org/wiki/ZX-Paintbrush */
	if (nread > 12 && sig[0] == 'Z' && sig[1] == 'X' && sig[2] == '-'
	&& memcmp(sig, "ZX-Paintbrush", 13) == 0)
		return "image/x-spectrum-zxp";
	if (nread > 3 && sig[0] == 'c' && sig[1] == 'h' && sig[2] == 'r'
	&& sig[3] == '$')
		return "image/x-spectrum-chr";
	if (nread > 3 && sig[0] == 'C' && sig[1] == 'H' && sig[2] == 'X'
	&& sig[3] == 0x00)
		return "image/x-spectrum-chx";
	/* http://fileformats.archiveteam.org/wiki/BSP_(ZX_Spectrum) */
	if (nread > 2 && sig[0] == 'b' && sig[1] == 's' && sig[2] == 'p')
		return "image/x-spectrum-bsp";
	if (file_size == 1628 && nread > 7 && sig[0] == 0x76 && sig[1] == 0xAF
	&& sig[2] == 0xD3 && sig[3] == 0xFE && sig[4] == 0x21 && sig[5] == 0x00
	&& sig[6] == 0x58 && sig[7] == 0x11)
		return "image/x-spectrum-hlr";
	/* No file sample found. Taken from RECOIL (DecodeZxs) */
	if (nread > 7 && file_size == 2452 && sig[0] == 'Z' && sig[1] == 'X'
	&& sig[2] == '_' && sig[3] == 'S' && sig[4] == 'S' && sig[5] == 'C'
	&& sig[6] == 'I' && sig[7] == 'I')
		return "image/x-spectrum-zxs";
	/* RECOIL: recoil.c:RECOIL_DecodeSev */
	if (nread >= 23 && sig[0] == 'S' && sig[1] == 'e' && sig[2] == 'v'
	&& sig[3] == 0x00 && sig[6] == 0x01 && sig[7] == 0x00)
		return "image/x-spectrum-sevenup"; /* .sev */

	/* https://temlib.org/AtariForumWiki/index.php/CrackArt_file_format */
	if (nread > 16 && sig[0] == 'C' && sig[1] == 'A' && sig[2] <= 0x01
	&& sig[3] <= 0x02)
		return "image/x-atari-crackart";

	if (nread > 7 && sig[0] == 'I' && sig[1] == 'T' && sig[2] == 'O'
	&& sig[3] == 'L' && sig[4] == 'I' && sig[5] == 'T' && sig[6] == 'L'
	&& sig[7] == 'S')
		return "application/x-ms-reader";

	if (nread > 5 && sig[0] == 0x00 && sig[1] == 0x00 && sig[3] == 0x00
	&& sig[4] > 0x00 && sig[5] == 0x00) {
		if (sig[2] == 0x01)	return "image/vnd.microsoft.icon";
		if (sig[2] == 0x02)	return "image/x-win-bitmap";
	}

	if (nread > 7 && IS_DIGIT(sig[0])
	&& (sig[1] == ' ' || sig[2] == ' ' || sig[3] == ' ' || sig[4] == ' ')
	&& (sig[4] == 0x0A || sig[5] == 0x0A || sig[6] == 0x0A || sig[7] == 0x0A
	|| sig[8] == 0x0A || sig[9] == 0x0A)
	&& is_mtv_image(sig, nread) == 1)
		return "image/x-mtv";

	if (nread > 3 && sig[0] == 'P' && (sig[1] == 'F' || sig[1] == 'f')
	&& (sig[2] == 0x0a || (sig[2] == '4' && sig[3] == 0x0a)))
		return "image/x-pfm"; /* PFM and Augmented PFM */

	if (nread > 3 && sig[0] == 'C' && sig[1] == 'P' && sig[2] == 'C'
	&& sig[3] == 0xB2) /* Cartesian Perceptual Compression image */
		return "image/x-cpc";

	if (nread > 3 && sig[0] == '_' && sig[1] == 'C' && sig[2] == 'D'
	&& sig[3] == '5')
		return "image/x-cd5";

	/* http://fileformats.archiveteam.org/wiki/DEGAS_image
	 * file(1): magic/Magdir/images */
	if (nread == BYTES_TO_READ) {
		if ((file_size == 32034 || file_size == 32066 || file_size == 32128
		|| file_size == 44834) && BE_U16(sig) <= 0x0002)
			return "image/x-atari-degas";
		if (file_size == 153634 && BE_U16(sig) == 0x0004)
			return "image/x-atari-degas";
		if (file_size == 154114 && BE_U16(sig) == 0x0007)
			return "image/x-atari-degas";
	}
	/* file(1): magic/Magdir/images */
	/* Degas low res compressed */
	if (nread > 12 && BE_U16(sig) == 0x8000 && (BE_U16(sig + 2) & 0xF000) == 0
	&& (BE_U16(sig + 10) & 0xF000) == 0 && (BE_U16(sig + 2) || BE_U16(sig + 4)))
		return "image/x-atari-degas";
	/* Degas mid res compressed */
	if (nread > 8 && BE_U16(sig) == 0x8001 && (BE_U16(sig + 2) & 0xF000) == 0
	&& (BE_U16(sig + 6) & 0xF000) == 0)
		return "image/x-atari-degas";
	/* Degas high res compressed */
	if (nread > 8 && BE_U16(sig) == 0x8002 && (BE_U16(sig + 2) & 0xF000) == 0)
		return "image/x-atari-degas";
	if (nread == 64 && file_size == 64 && is_degas_brush(sig, nread) == 1)
		return "image/x-atari-degas-brush";

	/* https://temlib.org/AtariForumWiki/index.php/PaintPro_ST/PlusPaint_ST */
	if (nread == BYTES_TO_READ && file_size == 64034 && BE_U16(sig) <= 0x0002)
		return "image/x-atari-paintpro";

	if (nread > 5 && sig[0] == 'S' && sig[1] == 'P' && sig[2] == 'X'
	&& (sig[3] == 0x01 || sig[3] == 0x02))
		return "image/x-atari-spx";

	if (nread > 5 && sig[0] == 'R' && sig[1] == 'I' && sig[2] == 'P'
	&& IS_DIGIT(sig[3]) && sig[4] == '.' && IS_DIGIT(sig[5]))
		return "image/x-atari-rip";

	/* https://temlib.org/AtariForumWiki/index.php/GodPaint_file_format */
	if (nread > 6 && ((sig[0] == 'G' && sig[1] == '4') || sig[0] == 0x04)) {
		const size_t width = (size_t)BE_U16(sig + 2);
		const size_t height = (size_t)BE_U16(sig + 4);
		if ((size_t)file_size == 6 + (width * height * 2))
			return "image/x-atari-god";
	}

	/* http://fileformats.archiveteam.org/wiki/Funny_Paint */
	if (nread > 3 && sig[0] == 0x00 && sig[1] == 0x0A && sig[2] == 0xCF
	&& sig[3] == 0xE2)
		return "image/x-atari-funny-paint";

	/* http://fileformats.archiveteam.org/wiki/ICDRAW_icon */
	if (nread > 3 && sig[0] == 'I' && sig[1] == 'C' && sig[2] == 'B'
	&& (sig[3] == 'I' || sig[3] == '3'))
		return "image/x-atari-icdraw";

	/* https://temlib.org/AtariForumWiki/index.php/TmS_Cranach_file_format */
	if (nread > 812 && sig[0] == 'T' && sig[1] == 'M' && sig[2] == 'S'
	&& sig[3] == 0x00)
		return "image/x-atari-esm";

	/* https://temlib.org/AtariForumWiki/index.php/DelmPaint_file_format */
	if (nread > 11 && !BE_U16(sig) && sig[2] && !BE_U16(sig + 4) && sig[6]
	&& (sig[10] == 0x01 || sig[10] == 0x03)
	&& is_delmpaint(sig, nread, file_size) == 1)
		return "image/x-atari-delm-paint";

	if (nread > 2 && file_size == 63054 && sig[0] == 'K' && sig[1] == 'D')
		return "image/x-atari-kid";

	if (nread > 5 && sig[0] == 'R' && sig[1] == 'A' && sig[2] == 'G'
	&& sig[3] == '-' && sig[4] == 'D' && sig[5] == '!')
		return "image/x-atari-ragd";

	if (nread > 7 && sig[0] == 'C' && sig[1] == 'I' && sig[2] == 'N'
	&& sig[3] == ' ' && IS_DIGIT(sig[4]) && sig[5] == '.' && IS_DIGIT(sig[6])
	&& sig[7] == ' ')
		return "image/x-atari-cin";

	/* http://fileformats.archiveteam.org/wiki/Spooky_Sprites */
	if (nread > 3 && sig[0] == 'T' && sig[1] == 'C' && sig[2] == 'S'
	&& sig[3] == 'F')
		return "image/x-atari-spooky";
	if (nread > 3 && sig[0] == 't' && sig[1] == 'r') {
		if (sig[2] == 'u' && sig[3] == '?') return "image/x-atari-spooky";
		if (sig[2] == 'e' && sig[3] == '1') return "image/x-atari-spooky";
	}

	/* http://fileformats.archiveteam.org/wiki/CharPad */
	if (nread > 3 && sig[0] == 'C' && sig[1] == 'T' && sig[2] == 'M'
	&& sig[3] == 0x05)
		return "image/x-commodore-charpad";

	if (nread > 4 && sig[0] == 0x08 && sig[1] == 0xF2 && sig[2] == 0xA6
	&& sig[3] == 0xB6)
		return "image/x-vips";

	if (nread > 6 && sig[0] == 'L' && sig[1] == 'B' && sig[2] == 'L'
	&& sig[3] == 'S' && sig[4] == 'I' && sig[5] == 'Z' && sig[6] == 'E')
		return "image/x-vicar";

	/* https://temlib.org/AtariForumWiki/index.php/Calamus_Raster_Graphic_file_format */
	if (nread > 9 && sig[0] == 'C' && sig[1] == 'A' && sig[2] == 'L'
	&& sig[3] == 'A' && sig[4] == 'M' && sig[5] == 'U' && sig[6] == 'S'
	&& sig[7] == 'C' && sig[9] == 'G') {
		if (sig[8] == 'R') return "image/x-atari-calamus-crg";
		if (sig[8] == 'V') return "image/x-atari-calamus-cvg";
	}

	/* http://fileformats.archiveteam.org/wiki/Paintworks */
	if (nread > 62 && sig[54] == 'A' && sig[55] == 'N' && sig[56] == 'v'
	&& sig[57] == 'i' && sig[58] == 's' && sig[59] == 'i' && sig[60] == 'o'
	&& sig[61] == 'n' && sig[62] == 'A')
		return "image/x-atari-paintworks";

	/* http://fileformats.archiveteam.org/wiki/PaintShop_(Atari_ST) */
	if (nread > 14 && sig[0] == 't' && sig[1] == 'm' && sig[2] == '8'
	&& sig[3] == '9' && sig[8] == 0x02 && sig[9] == 0x01
	&& is_atari_paintshop(sig, nread) == 1)
		return "image/x-atari-paintshop";

	/* https://temlib.org/AtariForumWiki/index.php/Signum!_file_format */
	if (nread > 8 && sig[0] == 'b' && sig[1] == 'i' && sig[2] == 'm'
	&& sig[3] == 'c' && sig[4] == '0' && sig[5] == '0' && sig[6] == '0'
	&& sig[7] == '2')
		return "image/x-atari-signum";

	/* http://fileformats.archiveteam.org/wiki/Picture_Packer */
	if (nread > 9 && sig[0] == 'L' && sig[1] == 'i' && sig[9] == 'k'
	&& memcmp(sig, "Lionpoubnk", 10) == 0)
		return "image/x-atari-picture-packer"; /* .mbk */
	if (nread > 5 && sig[0] == 0x06 && sig[1] == 0x07 && sig[2] == 0x19
	&& sig[3] == 0x63 && sig[4] == 0x00 && sig[5] <= 0x02)
		return "image/x-atari-picture-packer"; /* .pp1, .pp2, .pp3 */

	if (nread > 7 && sig[0] == 'I' && sig[1] == 'S' && sig[2] == '_'
	&& sig[3] == 'I' && sig[4] == 'M' && sig[5] == 'A' && sig[6] == 'G'
	&& sig[7] == 'E')
		return "image/x-atari-inshape";

	if (nread > 3 && sig[0] == 'I' && sig[1] == 'n' && sig[2] == 'd'
	&& sig[3] == 'y')
		return "image/x-atari-indypaint";

	if (nread > 3 && sig[0] == 'I' && sig[1] == 'M' && sig[2] == 'D'
	&& sig[3] == 'C')
		return "image/x-atari-imagic";

	/* http://fileformats.archiveteam.org/wiki/PabloPaint */
	if (nread >= 28 && sig[0] == 'P' && sig[1] == 'A' && sig[2] == 'B'
	&& sig[3] == 'L' && sig[4] == 'O'
	&& memcmp(sig + 5, " PACKED PICTURE: Groupe CDND", 28) == 0)
		return "image/x-atari-pablo-paint";

	if (nread > 83 && sig[80] == 'C' && sig[81] == 'T' && sig[82] == 0x00
	&& sig[83] == 0x00)
		return "image/x-scitex-ct";

	if (nread > 4 && sig[0] == 'R' && sig[1] == 'I' && sig[2] == 'X'
	&& sig[3] == '3')
		return "image/x-colorix";

	if (nread > 6 && sig[0] == 'P' && sig[1] == 'I' && sig[2] == 'X'
	&& sig[3] == 'T' && (BE_U16(sig + 4) == 0x01 || BE_U16(sig + 4) == 0x02)
	&& sig[6] <= 2)
		return "image/x-atari-pixart";

	/* https://netghost.narod.ru/gff/graphics/book/ch03_03.htm */
	if (nread > 5 && sig[0] == 0x2E && sig[1] == 0x4B && sig[2] == 0x46
	&& sig[3] == 0x68 && sig[4] == 0x80)
		return "image/x-kofax";

	/* http://fileformats.archiveteam.org/wiki/Prism_Paint */
	if (nread > 3 && sig[0] == 'P' && sig[1] == 'N' && sig[2] == 'T'
	&& sig[3] == 0x00)
		return "image/x-atari-prism-paint";

	if (nread > 3 && sig[0] == 'E' && sig[1] == 'Z' && sig[2] == 0x00
	&& sig[3] == 0xC8)
		return "image/x-atarti-ez-art";

	/* http://fileformats.archiveteam.org/wiki/Koala_MicroIllustrator */
	if (nread > 4 && sig[0] == 0xFF && sig[1] == 0x80 && sig[2] == 0xC9
	&& sig[3] == 0xC7 && sig[4] == 0x1A)
		return "image/x-atari-koala";

	if (file_size == 3845 && nread > 4 && sig[0] == 0xF4 && sig[1] == 0x0E
	&& sig[2] == 0x36 && sig[3] == 0x00)
		return "image/x-atari-magic-painter";

	/* http://fileformats.archiveteam.org/wiki/COKE_(Atari_Falcon) */
	if (nread > 3 && sig[0] == 'C' && sig[1] == 'O' && sig[2] == 'K'
	&& sig[3] == 'E' && memcmp(sig, "COKE format.", 12) == 0)
		return "image/x-atari-coke";

	if (nread >= 21 && sig[0] == 0xCC && sig[1] == 0xF5 && sig[2] == 0xE4
	&& memcmp(sig, "\xCC\xF5\xE4\xE5\xEB\xA0\xCD\xE1\xEB\xE5\xF2\xA0\xE4\xE1\xF4\xE1\xA0\xE6\xE9\xEC\xE5", 21) == 0)
		return "image/x-atari-ludek-maker";

	/* https://www.fileformat.info/format/cgm/egff.htm */
	if (nread > 128 && (BE_U16(sig) & 0xFFE0) == 0x0020
	&& xmemmem(sig + 2, 128, "\x10\x22", 2))
		return "image/cgm";
	if (nread > 4 && TOLOWER(sig[0]) == 'b' && TOLOWER(sig[1]) == 'e'
	&& TOLOWER(sig[2]) == 'g' && TOLOWER(sig[3]) == 'm'
	&& TOLOWER(sig[4]) == 'f')
		return "image/cgm";

	if (nread > 82 && sig[34] == 'L' && sig[35] == 'P' && sig[82] != 0x00
	&& memcmp(sig + 64, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16) == 0)
		return "application/vnd.ms-fontobject";

	/* https://applesaucefdc.com/a2r/ */
	if (nread > 7 && sig[0] == 'A' && sig[1] == '2' && sig[2] == 'R'
	&& sig[3] >= 0x31 && sig[3] <= 0x33 && sig[4] == 0xFF && sig[5] == 0x0A
	&& sig[6] == 0x0D && sig[7] == 0x0A)
		return "application/x-a2r-disk";

	if (nread > 5 && sig[2] == 'E' && sig[3] == 'Y' && sig[4] == 'W'
	&& sig[5] == 'R')
		return "application/x-amiga-writer";

	if (nread > 4 && sig[0] == 0xFF && sig[1] == 'F' && sig[2] == 'O'
	&& sig[3] == 'N' && sig[4] == 'T')
		return "font/x-dos-cpi";

	if (nread > 6 && sig[0] == 0x7F && sig[1] == 'D' && sig[2] == 'R'
	&& sig[3] == 'F' && sig[4] == 'O' && sig[5] == 'N' && sig[6] == 'T')
		return "font/x-drdos-cpi";

	if (nread > 3 && sig[0] == 'L' && sig[1] == 'W' && sig[2] == 'F'
	&& sig[3] == 'N')
		return "font/x-postscript-pfb";
	if (nread > 68 && sig[65] == 'L' && sig[66] == 'W' && sig[67] == 'F'
	&& sig[68] == 'N')
		return "font/x-postscript-pfb";
	if (nread >= 23 && sig[6] == '%' && sig[7] == '!'
	&& (sig[8] == 'P' || sig[8] == 'F')
	&& (memcmp(sig + 6, "%!PS-AdobeFont-1.", 17) == 0
	|| memcmp(sig + 6, "%!FontType1-1.", 14) == 0))
		return "font/x-postscript-pfb";
	if (nread >= 17 && sig[0] == '%' && sig[1] == '!'
	&& (sig[2] == 'P' || sig[2] == 'F')
	&& (memcmp(sig, "%!PS-AdobeFont-1.", 17) == 0
	|| memcmp(sig, "%!FontType1-1.", 14) == 0))
		return "font/x-postscript-pfb";

	if (nread > 3 && sig[0] == 0x0F && (sig[1] == 0x00 || sig[1] == 0x02)
	&& BE_U16(sig + 2) > 0x00)
		return "font/x-amiga-font";

	/* https://web.archive.org/web/20050305044255/http://www.lazerware.com:80/formats/macbinary/macbinary_iii.html */
	if (nread > 123 && !sig[0] && !sig[74] && !sig[82] && sig[1]
	&& sig[102] == 'm' && sig[103] == 'B' && sig[104] == 'I' && sig[105] == 'N'
	&& (sig[122] == 0x81 || sig[122] == 0x82) && sig[123] == 0x81)
		/* MacBinary III (version I and II produce false positives) */
		return "application/x-macbinary";

	if (nread > 5 && sig[0] == 'H' && sig[1] == 'S' && sig[2] == 'P'
	&& sig[4] == 0x9B && sig[5] == 0x00) {
		if (sig[3] == 0x01) return "application/x-os2-inf";
		if (sig[3] == 0x10) return "application/x-os2-hlp";
	}
	if (nread > 7 && sig[0] == 0xFF && sig[1] == 'M' && sig[2] == 'K'
	&& sig[3] == 'M' && sig[4] == 'S' && sig[5] == 'G' && sig[6] == 'F'
	&& sig[7] == 0x00)
		return "application/x-os2-msg";
	if (nread > 7 && sig[0] == 0xFF && sig[1] == 0xFF && sig[2] == 0xFF
	&& sig[3] == 0xFF && sig[4] == 0x14 && sig[5] == 0x00 && sig[6] == 0x00
	&& sig[7] == 0x00)
		return "application/x-os2-ini";

	if (nread > 7 && sig[0] == 'Z' && sig[1] == 'X' && sig[2] == 'T'
	&& sig[3] == 'a' && sig[4] == 'p' && sig[5] == 'e' && sig[6] == '!'
	&& sig[7] == 0x1A)
		return "application/x-spectrum-tzx";

	/* https://www.cpcwiki.eu/index.php/Format:DSK_disk_image_file_format */
	if (nread > 34 && sig[17] == 'F' && sig[18] == 'i' && sig[19] == 'l'
	&& sig[20] == 'e' && sig[21] == 0x0D && sig[22] == 0x0A
	&& memcmp(sig + 23, "Disk-Info\r\n", 11) == 0)
		return "application/x-spectrum-dsk";

	/* https://github.com/reyco2000/CoCo-Image-Viewer/blob/main/documentation/COCO-PICS-FORMATS.md#clp-format-max-10-clipboard */
	if (nread > 25 && nread == (size_t)file_size && sig[file_size - 1] == 0x64) {
		const size_t payload_len = (size_t)file_size - 0x19 - 1; /* Exclude final 0x64 */
		const size_t expected_len = (size_t)BE_U16(sig + 20) * (size_t)sig[24];
		if (payload_len == expected_len)
			return "image/x-coco-clp";
	}
	/* RECOIL: recoil.c (RECOIL_DecodeCocoMax)
	 * This is bad heuristic. Take a look at
	 * https://github.com/reyco2000/CoCo-Image-Viewer/blob/main/documentation/COCO-PICS-FORMATS.md#max-format-cocomax-12 */
	if (file_size == 6154 || file_size == 6155 || file_size == 6272
	|| file_size == 7168) {
		if (nread > 4 && sig[0] == 0x00 && sig[1] == 0x18 && sig[2] <= 0x01
		&& sig[3] == 0x0E && sig[4] == 0x00)
			return "image/x-coco-max";
	}
	/* RECOIL: recoil.c (RECOIL_DecodeP11) */
	if (nread > 4 && sig[0] == 0x00 && sig[1] == 0x0C && sig[3] == 0x0E
	&& sig[4] == 0x00 && (file_size == 3243 || file_size == 3083))
		return "image/x-coco-p11";

	/* http://fileformats.archiveteam.org/wiki/PaperPort_(MAX) */
	if (nread > 4 && sig[0] == 'V' && sig[1] == 'i' && sig[2] == 'G'
	&& (sig[3] == 'A' || sig[3] == 'B' || sig[3] == 'C' || sig[3] == 'E'
	|| sig[3] == 'F'))
		return "image/x-paperport";

	/* https://github-wiki-see.page/m/AlbanBedel/scummc/wiki/Scumm-6-data-format */
	if (nread > 4 && sig[0] == 'R' && sig[1] == 'N' && sig[2] == 'A'
	&& sig[3] == 'M' && sig[4] == 0x00)
		return "application/x-scumm-room";

	if (nread > 6 && sig[0] == 'R' && sig[1] == 'E' && sig[2] == 'G'
	&& sig[3] == 'E' && sig[4] == 'D' && sig[5] == 'I' && sig[6] == 'T')
		return "application/x-ms-regedit";

	/* https://sdif.sourceforge.net/standard/sdif-standard.html */
	if (nread > 4 && sig[0] == 'S' && sig[1] == 'D' && sig[2] == 'I'
	 && sig[3] == 'F')
		return "application/x-sdif";

	/* http://fileformats.archiveteam.org/wiki/Pack-Ice */
	if (nread > 3 && sig[0] == 'I' && TOUPPER(sig[1]) == 'C'
	&& TOUPPER(sig[2]) == 'E' && sig[3] == '!')
		return "application/x-ice";

	/* http://fileformats.archiveteam.org/wiki/LBR */
	if (nread > 15 && sig[0] == 0x00 && BE_U16(sig + 12) == 0x00
	&& BE_U16(sig + 14) != 0x00 && memcmp(sig + 1, "           ", 11) == 0)
		return "application/x-lbr";

	/* file(1): magic/Magdir/c64 */
	if (nread >= 2 && sig[0] == 'C' && sig[1] == '6' && sig[2] == '4') {
		if (nread >= 12 && memcmp(sig, "C64-TAPE-RAW", 12) == 0)
			return "application/x-commodore-tape-raw";
		if (nread > 78 && !(BE_U32(sig + 70) & 0x001FFF00)
		&& !(BE_U32(sig + 74) & 0xC0FFFFFF))
			return "application/x-commodore-tape-image";
		if (nread > 72 && sig[3] == ' ' && memcmp(sig, "C64 CARTRIDGE", 12) == 0
		&& BE_U32(sig + 68) > 0x10)
			return "application/x-commodore-crt";
	}
	/* Commodore-64 */
	if (nread > 6 && sig[0] == 0x01 && (sig[1] == 0x08 || sig[1] == 0x12)) {
		if (nread >= 11 && sig[1] == 0x08 && sig[4] == 0xEF && sig[6] == 0x9E
		&& memcmp(sig, "\x01\x08\x0b\x08\xef\x00\x9e\x32\x30\x36\x31", 11) == 0)
			return "application/x-compress-pucrunch";
		if (sig[6] == 0x9E) return "application/x-commodore-exec";
		return "application/x-commodore-basic";
	}
	/* Commodore-128 */
	if (nread > 6 && sig[0] == 0x01 && sig[1] == 0x1C
	&& LE_U16(sig + 2) < 0x1D02 && LE_U16(sig + 2) > 0x1C06) {
		if (sig[6] == 0x9E) return "application/x-commodore-exec";
		return "application/x-commodore-basic";
	}
	/* Commodore VIC-20 */
	if (nread > 6 && sig[0] == 0x01 && sig[1] == 0x10 && sig[6] > 0x7F
	&& LE_U16(sig + 2) > 0x1006 && LE_U16(sig + 2) < 0x1102) {
		if (sig[6] == 0x9E) return "application/x-commodore-exec";
		const uint16_t v = LE_U16(sig + 2);
		if (v < 0x1000) return "application/x-commodore-exec";
		const size_t offset = (size_t)v - 0x1000;
		if (offset < nread && sig[offset] == 0x00)
			return "application/x-commodore-basic";
		return "application/x-commodore-exec";
	}

	/* http://fileformats.archiveteam.org/wiki/BFLI */
	if (nread == BYTES_TO_READ && file_size == 33795 && sig[0] == 0xFF
	&& sig[1] == 0x3B && sig[2] == 'b')
		return "image/x-commodore-bfli";

	/* https://csbruce.com/cbm/postings/csc19950906-1.txt */
	if (nread > 8 && sig[0] == 'B' && sig[1] == 'M' && sig[2] == 0xCB
	&& (sig[3] == 0x02 || sig[3] == 0x03)) {
		const uint16_t W = BE_U16(sig + 4);
		const uint16_t H = BE_U16(sig + 6);
		if (W > 0 && W <= 2048 && H > 0 && H <= 2048)
			return "image/x-commodore-vbm";
	}

	/* http://fileformats.archiveteam.org/wiki/Drazlace */
	if (nread > 10 && sig[0] == 0x00 && sig[1] == 'X' && sig[10] == '!'
	&& memcmp(sig + 1, "XDRAZLACE!", 10) == 0)
		return "image/x-commodore-drazlace";

	/* http://justsolve.archiveteam.org/wiki/Drazpaint */
	if (nread > 10 && sig[0] == 0x00 && sig[1] == 'X' && sig[5] == 'Z'
	&& memcmp(sig + 1, "XDRAZPAINT", 10) == 0)
		return "image/x-commodore-drazpaint"; /* .drp */
	if (file_size == 10051 && nread > 1 && sig[0] == 0x00 && sig[1] == 0x58)
		return "image/x-commodore-drazpaint"; /* .drz */

	/* http://fileformats.archiveteam.org/wiki/GoDot */
	if (nread > 3 && sig[0] == 'G' && sig[1] == 'O' && sig[2] == 'D'
	&& (sig[3] == '0' || sig[3] == '1'))
		return "image/x-commodore-godot";

	if (nread >= 16 && sig[0] == 0xF0 && sig[2] == 'F' && sig[10] == ' '
	&& memcmp(sig, "\xf0\x3f\x46\x55\x4e\x50\x41\x49\x4e\x54\x20\x28\x4d\x54\x29\x20", 16) == 0)
		return "image/x-commodore-funpaint";

	/* http://justsolve.archiveteam.org/wiki/Vidcom_64 */
	if (nread == BYTES_TO_READ && file_size == 10050 && sig[0] == 0x00
	&& sig[1] == 0x58)
		return "image/x-commodore-vidcom64";

	/* https://www.c64-wiki.de/wiki/Printfox
	 * .gb and .bs in compressed form (.pg is undocumented). */
	if (nread > 36 && (sig[0] == 'B' || sig[0] == 'G') && sig[1] == 0x9B
	&& is_compressed_printfox(sig, nread) == 1)
		return "image/x-commodore-printfox";

	/* http://fileformats.archiveteam.org/wiki/Picasso_64 */
	if (file_size == 10050 && nread > 1 && sig[0] == 0x00 && sig[1] == 0x18)
		return "image/x-commodore-picasso64";

	/* http://fileformats.archiveteam.org/wiki/KoalaPainter */
	if ((file_size == 10003 || file_size == 10006) && nread > 1
	&& sig[0] == 0x00 && (sig[1] == 0x60 || sig[1] == 0x20))
		return "image/x-commodore-koalapaint";

	if (file_size == 10050 && nread > 1 && sig[0] == 0x00 && sig[1] == 0x78)
		return "image/x-commodore-p4i";

	if (file_size == 19434 && nread > 1 && sig[0] == 0x00 && sig[1] == 0x9C)
		return "image/x-commodore-truepaint";

	/* http://fileformats.archiveteam.org/wiki/Cheese */
	if (file_size == 20482 && nread > 1 && sig[0] == 0x00 && sig[1] == 0x80)
		return "image/x-commodore-cheese";

	if (file_size == 4098 && nread > 1 && sig[0] == 0x00 && sig[1] == 0x20)
		return "image/x-commodore-mle";

	if ((file_size == 10241 || file_size == 10242 || file_size == 10250)
	&& nread > 1 && sig[0] == 0x00 && sig[1] == 0x58)
		return "image/x-commodore-dol";

	if ((file_size == 10018 || file_size == 10219) && nread > 1
	&& !sig[0] && sig[1] == 0x78)
		return "image/x-commodore-sar";

	if (nread > 3 && file_size == 4105 && sig[0] == 0xD0 && sig[1] == 0xC5
	&& sig[2] == 0xD4 && sig[3] == 0x32)
		return "image/x-commodore-pet";
	if (nread > 1 && file_size == 2026 && !sig[0] && sig[1] == 0x30)
		return "image/x-commodore-pet";

	/* http://fileformats.archiveteam.org/wiki/Doodle!_(C64) */
	if (file_size == 9218 && nread > 1 && sig[0] == 0x00
	&& (sig[1] == 0x1C || sig[1] == 0x5C))
		return "image/x-commodore-doodle";

	if (file_size == 8170 && nread > 1 && sig[0] == 0x00 && sig[1] == 0x40)
		return "image/x-commodore-cfli";

	if ((file_size == 17218 || file_size == 17409 || file_size == 17474)
	&& nread > 1 && !sig[0] && (sig[1] == 0x3B || sig[1] == 0x3C))
		return "image/x-commodore-fli";

	if (file_size == 8194 && nread > 1 && sig[0] == 0x00 && sig[1] == 0x60)
		return "image/x-commodore-centauri";

	if (file_size == 10277 && nread > 3 && sig[0] == 0xEF && sig[1] == 0x7E
	&& sig[2] == 0x19 && sig[3] == 0x08)
		return "image/x-commodore-cdu";

	if (nread > 8 && sig[0] == 0xF0 && sig[1] == 0x38 && sig[5] == 0x3F
	&& sig[6] == 0x7F && sig[8] == 0x00)
		return "image/x-commodore-flip";

	/* http://fileformats.archiveteam.org/wiki/Art_Studio */
	if ((file_size == 9002 || file_size == 9003 || file_size == 9009)
	&& nread > 1 && sig[0] == 0x00 && sig[1] == 0x20)
		return "image/x-commodore-art";

	if (file_size == 10242 && nread > 1 && !sig[0] && sig[1] == 0x5C)
		return "image/x-commodore-rainbow-painter";

	/* http://justsolve.archiveteam.org/wiki/Micro_Illustrator */
	if (file_size == 10022 && nread > 7 && sig[0] == 0xDC && sig[1] == 0x18
	&& sig[7] == 0x00)
		return "image/x-commodore-mil";

	/* http://fileformats.archiveteam.org/wiki/Gunpaint */
	if (file_size == 33603 && nread > 1 && sig[0] == 0x00 && sig[1] == 0x40)
		return "image/x-commodore-gunpaint";

	/* http://fileformats.archiveteam.org/wiki/Wigmore_Artist_64 */
	if (file_size == 10242 && nread > 1 && sig[0] == 0x00 && sig[1] == 0x40)
		return "image/x-commodore-artist64";

	/* https://dn710204.ca.archive.org/0/items/bombjack-50-gigabytes-of-commodore-manuals-books-disks-assembly-and-more/Applications/The_Image_System_User_Guide.pdf */
	if (file_size == 10218 && nread > 1 && sig[0] == 0x00 && sig[1] == 0x3C)
		return "image/x-commodore-image-system"; /* .ism */
	if ((file_size == 9194 || file_size == 30738) && nread > 2
	&& sig[0] == 0x00 && sig[1] == 0x40)
		return "image/x-commodore-image-system"; /* .ish */

	if (file_size == 16194 && nread > 1 && !sig[0] && sig[1] == 0x20)
		return "image/x-commodore-hires-interlace"; /* .ihe */

	if (file_size == 9218 && nread > 1 && !sig[0]
	&& (sig[1] == 0x20 || sig[1] == 0x40))
		return "image/x-commodore-hed"; /* .hed */

	if (file_size == 16386 && nread > 1 && !sig[0] && sig[1] == 0x40)
		return "image/x-commodore-hfc"; /* .hfc */

	if (file_size == 16385 && nread > 1 && !sig[0] && sig[1] == 0x40)
		return "image/x-commodore-afl"; /* .afl */

	/* RECOIL: recoil.c (RECOIL_DecodeBp) */
	if (file_size == 4083 && nread > 1 && sig[0] == 0x00 && sig[1] == 0x11)
		return "image/x-commodore-bestpaint";

	if ((file_size == 4174 || file_size == 4098) && nread > 1
	&& sig[0] == 0x00 && sig[1] == 0x18)
		return "image/x-commodore-lp3";

	if (file_size == 24578 && nread > 1 && !sig[0] && sig[1] == 0x20)
		return "image/x-commodore-hlf";

	/* http://fileformats.archiveteam.org/wiki/Hi-Pic_Creator */
	if (file_size == 9003 && nread > 1 && !sig[0] && sig[1] == 0x60)
		return "image/x-commodore-hpc";

	/* http://fileformats.archiveteam.org/wiki/Shoot_%27Em_Up_Construction_Kit */
	if (file_size == 514 && nread > 1 && sig[0] == 'B' && sig[1] == 0x00)
		return "font/x-commodore-seuck";
	if (file_size == 8130 && nread > 1 && sig[0] == 'B' && sig[1] == 0x00)
		return "image/x-commodore-seuck";

	if (file_size == 10007 && nread > 1 && sig[0] == 0x00 && sig[1] == 0x80)
		return "image/x-commodore-cwg"; /* Create With Garfield */

	/* http://fileformats.archiveteam.org/wiki/FFLI */
	if (file_size == 26115 && nread > 2 && sig[0] == 0xFF && sig[1] == 0x3A
	&& sig[2] == 0x66)
		return "image/x-commodore-ffli";

	if (nread > 5 && sig[0] == 0x00 && sig[1] == 0x00 && sig[2] == 'B'
	&& sig[3] == 'R' && sig[4] == 'U' && sig[5] == 'S')
		return "image/x-commodore-ipaint";

	/* RECOIL: recoil.c:RECOIL_DecodeGr,DecodeZs */
	if (nread > 3 && file_size == 5378 && sig[0] == ' ' && sig[1] == 0x15
	&& !sig[3])
		return "image/x-commodore-starpainter"; /* .cs */
	if (nread > 3 && file_size == 1026 && sig[0] == 0xB0 && sig[1] == 0xF0
	&& !sig[3])
		return "image/x-commodore-starpainter"; /* .zs */
	if (nread > 1 && file_size == 2 + (sig[0] * (sig[1] << 3)))
		return "image/x-commodore-starpainter"; /* .gr */

	/* RECOIL: recoil.c (RECOIL_DecodeHim) */
	if (nread > 5 && sig[0] == 0x00 && sig[1] == 0x40) {
		if (sig[3] == 0xFF && file_size == 16385)
			return "image/x-commodore-him";
		if (sig[2] + (sig[3] << 8) == 16381 + file_size
		&& sig[4] == 0xF2 && sig[5] == 0x7F)
			return "image/x-commodore-him";
	}

	if (nread > 9 && sig[0] == 0x00 && sig[1] == 0x40
	&& is_c64_loadstar(sig, nread) == 1)
		return "image/x-commodore-loadstar";

	/* http://fileformats.archiveteam.org/wiki/Advanced_Art_Studio */
	if (file_size == 10018 && sig[0] == 0x00 && sig[1] == 0x20)
		return "image/x-commodore-art-studio"; /* .ocp */

	if (file_size == 10242 && !sig[0] && sig[1] == 0xA0)
		return "image/x-commodore-blazing-paddles"; /*.ip */

	/* RECOIL: recoil.c:RECOIL_DecodeSpd */
	if (nread > 4 && sig[0] == 'S' && sig[1] == 'P' && sig[2] == 'D'
	&& sig[3] == 0x01 && (size_t)file_size >= 9 + (((size_t)sig[4] + 1) << 6))
		return "image/x-commodore-spritepad";
	if (nread >= 67 && (file_size & 63) == 3 && sig[0] == 0x0B && !sig[1]
	&& sig[2] == 0x01 && !sig[3])
		return "image/x-commodore-spritepad";

	/* http://fileformats.archiveteam.org/wiki/Interpaint */
	if ((file_size == 9002 || file_size == 10003) && nread > 1 && !sig[0]
	&& sig[1] == 0x40)
		return "image/x-commodore-interpaint";

	if (file_size == 8002 && nread > 1 && !sig[0]
	&& (sig[1] == 0x20 || sig[1] == 0x60))
		return "image/x-commodore-hires";

	if (nread > 34 && sig[0] == 0x3D && (sig[1] == 0x3D || sig[1] == 0x02)
	&& BE_U16(sig + 4) <= 0x01 && BE_U16(sig + 10) <= 0x07
	&& BE_U16(sig + 2) <= (sig[1] == 0x3D ? 20 : 40))
		return "model/x-cad-3d";

	if (nread > 512 && sig[2] == 0xFE && (BE_U16(sig) & 0x3F73) == 0x0801
	&& !(BE_U32(sig + 508) & 0xFFFFFF00) && BE_U16(sig) == 0x0809) {
		const uint16_t v4 = LE_U16(sig + 4);
		if (v4 == 0x0009) return "image/x-intergraph-rle";
		if (v4 == 0x0018) return "image/x-intergraph-cit";
		if (v4 == 27)     return "image/x-intergraph-rgb";
		return "image/x-intergraph";
	}

	if (nread > 12 && sig[0] == '(' && sig[1] == 'c' && sig[2] == ')'
	&& sig[3] == 'F' && memcmp(sig, "(c)F.MARCHAL", 12) == 0)
		return "image/x-atari-rgh";

	if (nread > 6 && sig[0] == 'F' && sig[1] == 'L' && sig[2] == 'U'
	&& sig[3] == 'F' && sig[4] == 'F' && sig[5] == '6' && sig[6] == '4')
		return "image/x-amiga-flf";

	/* http://fileformats.archiveteam.org/wiki/SGX */
	if (nread > 17 && sig[0] == 'S' && ((sig[1] == 'V' && sig[2] == 'G')
	|| (sig[1] == 'G' && sig[2] == 'X')) && sig[3] == ' '
	&& memcmp(sig + 4, "Graphics File\0", 14) == 0)
		return "image/x-amiga-sgx";

	/* http://fileformats.archiveteam.org/wiki/ArtMaster88 */
	if (nread > 12 && sig[0] == 'S' && sig[1] == 'S' && sig[2] == '_'
	&& sig[3] == 'S' && sig[4] == 'I' && sig[5] == 'F'
	&& memcmp(sig + 6, "    0.0", 7) == 0)
		return "image/x-nec-artmaster88";

	if (nread > 8 && sig[0] == 'U' && sig[1] == 'I' && sig[2] == 'M'
	&& sig[3] == 'G' && sig[4] == 0x01 && sig[7] <= 0x03)
		return "image/x-atari-uimg";
	/* http://fileformats.archiveteam.org/wiki/STAD_PAC */
	if (nread > 3 && sig[0] == 'p' && sig[1] == 'M' && sig[2] == '8'
	&& (sig[3] == '5' || sig[3] == '6'))
		return "image/x-atari-stadpad";

	/* http://fileformats.archiveteam.org/wiki/DuneGraph */
	if (nread > 3 && sig[0] == 'D' && sig[1] == 'G'
	&& (sig[2] == 'U' || sig[2] == 'C') && ((sig[2] == 'U' && sig[3] == 0x01)
	|| (sig[2] == 'C' && sig[3] <= 0x03)))
		return "image/x-atari-dunegraph";

	/* http://fileformats.archiveteam.org/wiki/INT95a */
	if (nread > 5 && sig[0] == 'I' && sig[1] == 'N' && sig[2] == 'T'
	&& sig[3] == '9' && sig[4] == '5' && sig[5] == 'a')
		return "image/x-atari-int95a";

	/* http://fileformats.archiveteam.org/wiki/DeskPic */
	if (nread > 3 && sig[0] == 'G' && sig[1] == 'F' && sig[2] == '2'
	&& sig[3] == '5')
		return "image/x-atari-deskpic";

	/* http://fileformats.archiveteam.org/wiki/PowerGraphics */
	if (nread > 15 && sig[8] == 'P' && sig[9] == 'o' && sig[15] == 'X'
	&& memcmp(sig + 8, "PowerGFX", 8) == 0)
		return "image/x-atari-powergfx";

	/* http://fileformats.archiveteam.org/wiki/Cyber_Paint_Cell */
	if (nread > 128 && BE_U32(sig) == 0xFFFF0000 && !BE_U16(sig + 62)
	&& is_cel_image(sig, nread) == 1)
		return "image/x-atari-cyber-paint"; /* .cel */

	if (nread > 12 && sig[0] == 0xF0 && sig[1] == 0xED && sig[2] == 0xE4
	&& is_atari_pmg(sig, nread, file_size) == 1)
		return "image/x-atari-pmg-designer";

	/* http://fileformats.archiveteam.org/wiki/SFDN_Packer */
	if (nread > 2 && sig[0] == 'S' && sig[1] == '1' && sig[2] == '0'
	&& sig[3] == '1')
		return "application/x-atari-sfdn";

	if (file_size == 32038 && is_srt_image(file) == 1)
		return "image/x-atari-synthetic-arts";

	if (nread > 8 && sig[0] == 'P' && !sig[1] && !sig[2] && sig[3] == 'P'
	&& !sig[4] && !sig[5] && sig[6] == 'R' && !sig[7] && !sig[8])
		return "image/x-ppr";

	/* http://fileformats.archiveteam.org/wiki/MultiArtist */
	if (nread > 2 && sig[0] == 'M' && sig[1] == 'G' && sig[2] == 'H'
	&& (file_size == 19456 || file_size == 18688 || file_size == 15616
	|| file_size == 14080))
		return "image/x-spectrum-multiartist";

	/* http://fileformats.archiveteam.org/wiki/Pegasus_PIC */
	if (nread > 34 && sig[0] == 'B' && sig[1] == 'M' && sig[14] == 0x44
	&& (sig[15] + sig[16] + sig[17] == 0) && memcmp(sig + 30, "JPEG", 4) == 0)
		return "image/x-pegasus-pic";
	/* http://fileformats.archiveteam.org/wiki/Pegasus_PIC2 */
	if (nread > 4 && sig[0] == 'P' && sig[1] == 'I' && sig[2] == 'C'
	&& sig[3] == '2' && sig[4] == 0x01)
		return "image/x-pegasus-pic2";

	/* http://fileformats.archiveteam.org/wiki/AMOS_Picture_Bank */
	if (nread > 20 && sig[0] == 'A' && sig[1] == 'm' && sig[2] == 'B'
	&& sig[3] == 'k' && memcmp(sig + 12, "Pac.Pic.", 8) == 0)
		return "image/x-amiga-amos";
	if (nread > 3 && sig[0] == 'A' && sig[1] == 'm'
	&& ((sig[2] == 'S' && sig[3] == 'p') || (sig[2] == 'I' && sig[3] == 'c')))
		return "image/x-amiga-amos";

	/* http://fileformats.archiveteam.org/wiki/Graph_Saurus
	 * file(1): magic/Magdir/msx */
	if (nread > 7 && sig[0] == 0xFD && !LE_U16(sig + 1) && !LE_U16(sig + 5)
	&& LE_U16(sig + 3) > 0x013D) /* Compressed */
		return "image/x-msx-graphsaurus";

	/* http://fileformats.archiveteam.org/wiki/MSX_BASIC_graphics */
	if (nread > 7 && sig[0] == 0xFE && !LE_U16(sig + 1) && !LE_U16(sig + 5)) {
		const uint16_t v3 = LE_U16(sig + 3);
		if (is_msx_screen(v3, file_size) == 1)
			return "image/x-msx-screen";
		if (v3 == 0xD400) return "image/x-msx-graphsaurus"; /* GraphSaurus SR7/SR8/SRS */
		if (v3 == 0x6A00) return "image/x-msx-graphsaurus"; /* GraphSaurus SR5 */
	}

	if (nread > 4) {
		const size_t W = (size_t)LE_U16(sig);
		const size_t H = (size_t)LE_U16(sig + 2);
		if (W > 0 && W <= 2048 && H > 0 && H <= 2048) {
			/* RECOIL: recoil.c:RECOIL_DecodeGlN */
			if ((size_t)file_size == 4 + (W * H))
				return "image/x-msx-gl"; /* .gl8 */
			if ((size_t)file_size == 4 + ((W * H + 1) >> 1))
				return "image/x-msx-gl"; /* .gl5 and .gl7 */
			if ((size_t)file_size == 4 + ((W * H + 3) >> 2))
				return "image/x-msx-gl"; /* .gl6 */
			/* https://github.com/fgroen/msxconverter/blob/main/decoders/images/stp.go */
			if ((size_t)file_size == 8 + ((W * H + 3) >> 2))
				return "image/x-msx-stp"; /* Dynamic Publisher stamp */
		}
	}

	if (nread > 2 && sig[0] == 'G' && sig[1] == '9'	&& sig[2] == 'B'
	&& LE_U16(sig + 3) > 10 && LE_U16(sig + 5) > 0)
		return "image/x-msx-g9b";

	/* http://fileformats.archiveteam.org/wiki/MIG */
	if (nread > 6 && sig[0] == 'M' && sig[1] == 'S' && sig[2] == 'X'
	&& sig[3] == 'M' && sig[4] == 'I' && sig[5] == 'G')
		return "image/x-msx-mig";

	/* http://fileformats.archiveteam.org/wiki/Dynamic_Publisher */
	if (nread > 24 && sig[0] == 'D' && sig[1] == 'Y' && sig[7] == ' '
	&& memcmp(sig, "DYNAMIC PUBLISHER ", 18) == 0) {
		if (sig[18] == 'F' && memcmp(sig + 18, "FONT", 4) == 0)
			return "image/x-msx-fnt";
		if (sig[18] == 'S' && memcmp(sig + 18, "SCREEN", 6) == 0)
			return "image/x-msx-pct";
	}

	/* http://fileformats.archiveteam.org/wiki/GROB */
	if (nread > 9 && sig[0] == 'H' && sig[1] == 'P' && sig[2] == 'H'
	&& sig[3] == 'P' && sig[4] == '4' && (sig[5] == '8' || sig[5] == '9')
	&& sig[8] == 0x1E && sig[9] == 0x2B)
		return "image/x-grob";

	/* http://fileformats.archiveteam.org/wiki/XLD4 */
	if (nread > 15 && sig[11] == 'M' && sig[12] == 'A' && sig[13] == 'J'
	&& sig[14] == 'Y' && sig[15] == 'O')
		return "image/x-nec-q4";

	if (nread > 14 && sig[2] == 0x00 && sig[3] == 0x00 && sig[4] == 0x04
	&& sig[5] == 'M' && sig[6] == 'A' && sig[7] == 'I' && sig[8] == 'N'
	&& sig[14] == 0x00)
		return "image/x-apple2-graphics";

	/* RECOIL: recoil.c:RECOIL_Decode3201 */
	if (file_size >= 6654 && nread > 3 && sig[0] == 0xC1 && sig[1] == 0xD0
	&& sig[2] == 0xD0 && sig[3] == 0x00)
		return "image/x-apple2-graphics";

	/* RECOIL: recoil.c:RECOIL_DecodeMsl */
	if (file_size >= 3 && file_size <= 36
	&& (size_t)sig[0] + 2 == (size_t)file_size && is_msl_image(sig, nread) == 1)
		return "image/x-atari-mad-studio"; /* .msl */

	/* RECOIL: recoil.c:RECOIL_DecodeAn2/4 */
	if (nread > 1 && nread <= 967 && sig[0] < 40 && sig[1] < 24) {
		const uint8_t columns = sig[0] + 1;
		const uint8_t rows = sig[1] + 1;
		const size_t cr = (size_t)columns * (size_t)rows;
		if ((size_t)file_size == 2 + cr /* ANTIC 2 (.an2) */
		|| (size_t)file_size == 7 + cr) /* ANTIC 4-5 (.an4/5) */
			return "image/x-atari-mad-studio";
	}

	if (nread > 9 && ((nread == 22 && (sig[0] == 3 || sig[0] == 4))
	|| (sig[0] == 5 && nread == ((1 + (size_t)sig[5]) * 5)))) {
		const uint16_t W = LE_U16(sig + 1);
		const uint16_t H = LE_U16(sig + 3);
		if (W > 0 && W <= 384 && !(W & 0x03) && H > 0 && H <= 272)
			return "image/x-amstrad-perfectpix"; /* .pph */
	}

	/* https://www.fileformat.info/format/pictor/egff.htm */
	if (nread >= 10 && sig[0] == 0xFD && sig[1] == 0x00 && sig[2] == 0xB8
	&& !LE_U16(sig + 3)) {
		const size_t data_size = (size_t)LE_U16(sig + 5);
		if (data_size == 0 || data_size == 16384 || data_size == 32768
		|| sig[nread - 1] == 0x1A)
			return "image/x-pcpaint-bsave";
	}

	/* http://fileformats.archiveteam.org/wiki/BSAVE_Image */
	/* https://www.fileformat.info/format/pictor/egff.htm */
	if (nread >= 10 && sig[0] == 0xFD) {
		const size_t data_size = (size_t)LE_U16(sig + 5);
		if (data_size + 7 == (size_t)file_size /* 7 == header length */
		|| data_size + 8 == (size_t)file_size) /* +1 for trailing 0x1A */
			return "image/x-bsave";
		if (data_size + 7 < nread && sig[data_size + 7] == 0x1A)
			return "image/x-bsave";
	}

	/* https://netghost.narod.ru/gff/vendspec/pictor/pictor.txt */
	if (nread > 13 && sig[0] == 0x34 && sig[1] == 0x12 && sig[11] == 0xFF
	&& LE_U16(sig + 2) <= 2048 && LE_U16(sig + 4) <= 2048 && sig[13] <= 0x04)
		return "image/x-pcpaint-pic";

	/* https://www.fileformat.info/format/pictor/egff.htm */
	if (nread > 10 && (size_t)LE_U16(sig) == (size_t)file_size
	&& is_pictor_clp_image(sig, nread) == 1)
		return "image/x-pcpaint-clp";

	/* http://fileformats.archiveteam.org/wiki/Pi_(image_format) */
	if (nread > 32 && sig[0] == 'P' && sig[1] == 'i'
	&& is_pi_image(sig, nread) == 1)
		return "image/x-nec-pi";
	/* http://fileformats.archiveteam.org/wiki/PIC2 */
	if (nread > 3 && sig[0] == 'P' && sig[1] == '2' && sig[2] == 'D'
	&& sig[3] == 'T')
		return "image/x-nec-pic2";

	if (nread > 3 && sig[0] == 'S' && sig[1] == 'p' && sig[2] == 'r'
	 && sig[3] == '!')
		return "image/x-spr";

	/* http://fileformats.archiveteam.org/wiki/Seattle_FilmWorks */
	if (nread > 3 && sig[0] == 'S' && sig[1] == 'F' && sig[2] == 'W'
	&& sig[3] == '9' && (sig[4] == '3' || sig[4] == '4' || sig[4] == '5'
	||sig[4] == '8') && sig[5] == (sig[4] == '5' ? 'B' : 'A'))
		return "image/x-sfw";

	if (nread > 18 && sig[0] == 'D' && sig[5] == '-' && sig[12] == 'Q'
	&& memcmp(sig, "DAISY-DOT NLQ FONT", 18) == 0)
		return "font/x-atari-daisydot";

	if (nread > 24 && sig[0] == 'B' && sig[9] == 'A' && sig[18] == 'P'
	&& memcmp(sig, "BUGBITER_APAC239I_PICTURE", 25) == 0)
		return "image/x-atari-bugbiter";

	if (nread > 3 && sig[0] == 'T' && sig[1] == 'I' && sig[2] == 'P'
	&& sig[3] == 0x01)
		return "image/x-atari-taquart";

	if (nread > 3 && sig[0] == 'T' && sig[1] == 'R' && sig[2] == 'U'
	&& sig[3] == 'P')
		return "image/x-atari-eggpaint";

	if (nread > 8 && (sig[0] == 0x0F || (sig[0] >= 0x08 && sig[0] <= 0x0B))
	&& !BE_U32(sig + 1) && sig[8] == 0x00)
		return "image/x-atari-tools-800";

	if (file_size >= 3 && file_size <= 42
	&& 2 + (size_t)sig[0] == (size_t)file_size)
		return "image/x-atari-spr";

	/* http://fileformats.archiveteam.org/wiki/Rembrandt */
	if (nread > 10 && sig[0] == 'T' && sig[1] == 'R' && sig[2] == 'U'
	&& sig[3] == 'E' && sig[4] == 'C' && sig[5] == 'O' && sig[6] == 'L'
	&& sig[7] == 'R')
		return "image/x-atari-rembrandt";

	/* RECOIL: recoil.c:RECOIL_DecodeTl4 */
	if (nread > 4 && sig[0] > 0 && sig[0] <= 4 && sig[1] > 0 && sig[1] <= 5
	&& (size_t)file_size == (2 + ((size_t)sig[0] * (size_t)sig[1] * 9)))
		return "image/x-atari-tl4";

	if (nread > 8 && file_size == 1030 && nread > 6 && sig[0] == 0xFF
	&& sig[1] == 0xFF && sig[2] == 0x00 && sig[4] == 0xFF && sig[6] == 0x00)
		return "font/x-atari-sxs";

	/* RECOIL: recoil.c:RECOIL_DecodeImage72Fnt */
	if (nread > 3 && sig[0] == 0x00 && sig[1] == 0x08
	&& (size_t)file_size == 3 + ((size_t)sig[2] << 8))
		return "font/x-image72";

	if (nread > 66 && sig[64] == sig[24] && sig[65] == sig[25] && !sig[66]) {
		const size_t len = get_amstrad_header_len(sig, file_size);
		if (file_size == 896 && len > 0)
			return "font/x-amstrad-cpc";
		if (len > 0 && len + 16384 == (size_t)file_size)
			return "image/x-amstrad-hgb";
	}

	if (nread > 41 && file_size <= 32044 && sig[0] <= 0x03
	&& is_tinystuff_image(sig, nread, file_size) == 1)
		return "image/x-atari-tiny";

	/* http://fileformats.archiveteam.org/wiki/GEM_Raster */
	if (nread > 20 && (sig[16] == 'X' || sig[16] == 'T') && sig[17] == 'I'
	&& sig[18] == 'M' && sig[19] == 'G' && sig[20] == 0x00)
		return "image/x-atari-gem";
	if (nread > 20 && sig[16] == 'S' && sig[17] == 'T' && sig[18] == 'T'
	&& sig[19] == 'T' && sig[20] == 0x00)
		return "image/x-atari-gem";
	if (nread > 16 && BE_U16(sig) <= 3 && !sig[2]
	&& (sig[3] == 0x08 || sig[3] == 0x09 || sig[3] == 0x19)
	&& is_gem_image(sig, nread) == 1)
		return "image/x-atari-gem";
	/* http://fileformats.archiveteam.org/wiki/GEM_VDI_Metafile */
	if (nread > 4 && sig[0] == 0xFF && sig[1] == 0xFF && sig[3] == 0x00
	&& (sig[2] == 0x18 || sig[2] == 0x0E || sig[2] == 0x0F))
		return "image/x-atari-gem-vdi";

	/* http://fileformats.archiveteam.org/wiki/DeskMate_Paint */
	if (nread > 3 && sig[0] == 0x13 && sig[1] == 'P' && sig[2] == 'N'
	&& sig[3] == 'T')
		return "image/x-tandy-deskmate";

	/* http://fileformats.archiveteam.org/wiki/Storyboard_PIC/CAP */
	if (nread > 5 && sig[0] == 'E' && sig[1] == 'P' && sig[2] == '_'
	&& sig[3] == 'C' && sig[4] == 'A' && sig[5] == 'P')
		return "image/x-ibm-cap";
	if (nread > 5 && sig[1] == 0x84 && sig[2] == 0xC1 && sig[4] == 0x00
	&& (sig[3] == 1 || sig[3] == 3 || sig[3] == 7 || sig[3] == 8
	|| sig[3] == 10 || sig[3] == 11))
		return "image/x-ibm-cap";

	/* http://fileformats.archiveteam.org/wiki/Dir_Logo_Maker */
	if (file_size == 256 && nread > 3 && sig[0] == 'B' && sig[16] == 'B'
	&& sig[32] == 'B' && sig[48] == 'B')
		return "image/x-atari-dlm";

	if (file_size == 2054 && nread > 5 && sig[0] == 0xFF && sig[1] == 0xFF
	&& !sig[2] && sig[3] == 0xA0 && sig[4] == 0xFF && sig[5] == 0xA7)
		return "image/x-atari-jgp";

	/* RECOIL: recoil.c:REOCIL_DecodeHcm */
	if (file_size == 8208 && nread > 5 && sig[0] == 'H' && sig[1] == 'C'
	&& sig[2] == 'M' && sig[3] == 'A' && sig[4] == '8' && sig[5] == 0x01)
		return "image/x-atari-hcm";

	if (nread >= 92 && sig[0] == 0xFF && sig[1] == 0xFF
	&& file_size >= 92 && file_size <= 19212 && (file_size - 12) % 80 == 0
	&& is_atari_hip_image(sig, nread, file_size) == 1)
		return "image/x-atari-hip";

	/* RECOIL: recoil.c:REOCIL_DecodeWin */
	if ((size_t)file_size < BYTES_TO_READ) {
		const size_t W = (size_t)LE_U16(sig + file_size - 4);
		const size_t H = (size_t)sig[file_size - 2];
		if (W > 0 && W <= 640 && H > 0 && H <= 200
		&& is_art_studio(sig, W, H, file_size) == 1)
			return "image/x-amstrad-art-studio"; /* .win */
	}

	/* RECOIL: recoil.c:REOCIL_DecodeGhg */
	/* With a max dimension of 320x200, max size is 8003 */
	if (nread > 4 && file_size <= 8003) {
		const size_t W = (size_t)LE_U16(sig);
		const size_t H = (size_t)sig[2];
		if (W > 0 && W <= 320 && H > 0 && H <= 200
		&& (size_t)file_size == (3 + ((W + 7) >> 3) * H))
			return "image/x-atari-ghg";
	}

	/* http://fileformats.archiveteam.org/wiki/Dali */
	if (nread > 47 && IS_DIGIT(sig[32]) && IS_DIGIT(sig[33])
	&& is_dali_compressed(sig, nread, file_size) == 1)
		return "image/x-atari-dali";

	/* http://fileformats.archiveteam.org/wiki/Oric_HIRES_screen */
	if (nread >= 26 && BE_U32(sig) == 0x16161624 && !sig[4] && !sig[12])
		return "image/x-oric-hrs";

	/* http://fileformats.archiveteam.org/wiki/Psion_PIC */
	/* http://fileformats.archiveteam.org/wiki/PIC_(Yanagisawa) */
	if (nread > 3 && sig[0] == 'P' && sig[1] == 'I' && sig[2] == 'C') {
		if (sig[3] == 0x1A || sig[3] == 0xDC) return "image/x-psion-pic";
		if (sig[3] == '/') return "image/x-nec-pic";
	}

	if ((file_size == 262 || file_size == 257) && is_tx_image(sig, nread) == 1)
		return "image/x-atari-tx";

	if (file_size == 17351 && nread > 10 && sig[0] == 0x03 && !LE_U32(sig + 7))
		return "image/x-atari-din";

	/* RECOIL: recoil.c:RECOIL_DecodeMonoArt */
	if (nread > 3 && sig[0] <= 30 && sig[1] <= 64
	&& (size_t)file_size == (3 + (((size_t)sig[0] + 1) * ((size_t)sig[1] + 1))))
		return "image/x-atari-art";

	/* https://atarionline.pl/utils/2.%20Grafika/Interlace%20Graphics%20Editor/Interlace%20Graphics%20Editor%20-%20readme%20PL.txt */
	if (nread > 7 && file_size == 6160 && sig[0] == 0xFF && sig[1] == 0xFF
	&& sig[2] == 0xF6 && sig[3] == 0xA3 && sig[4] == 0xFF && sig[5] == 0xBB
	&& sig[6] == 0xFF && sig[7] == 0x5F)
		return "image/x-atari-ige";

	if (file_size == 3072 && is_atari_wnd(sig, nread) == 1)
		return "image/x-atari-blazing-paddles"; /*.wnd */

	/* RECOIL: recoil.c:RECOIL_AsciiArtEditor */
	/* Max: 64 bytes per row (plus 0x1b terminator) x 24 lines = 1560 */
	if (file_size <= 1560 && nread > 1 && sig[nread - 1] == 0x9B
	&& is_ascii_art(sig, nread) == 1)
		return "image/x-atari-ascii-art-editor";

	/* https://temlib.org/AtariForumWiki/index.php/Print-Technik_Raw_Data_file_format */
	if (nread > 10 && sig[0] == 0x0F && sig[1] == 0x0F && sig[2] == 0x00
	&& sig[3] == 0x01 && is_atari_hir_image(sig, nread, file_size) == 1)
		return "image/x-atari-hir";

	/* RECOIL: recoil.c:RECOIL_DecodeEnvision */
	if (nread > 1505 && sig[2] + 1 <= 204) {
		const uint8_t cols = sig[1] + 1;
		const uint8_t rows = sig[2] + 1;
		const size_t offset = 8 + cols * rows + 463;
		if (nread >= offset
		&& (size_t)file_size == offset + (size_t)sig[offset - 1] * 1033)
			return "image/x-atari-envision"; /* .map */
	}

	/* RECOIL: recoil.c:RECOIL_DecodeFwa */
	if (nread >= 7960 && sig[0] == 0xFE && sig[1] == 0xFE && sig[6] == 0x70
	&& sig[7] == 0x70 && sig[8] == 0x70 && sig[11] == 0x50 && sig[115] == 0x60
	&& sig[205] == 0x41 && 7960 + (size_t)LE_U16(sig + 7958) == (size_t)file_size)
		return "image/x-atari-fwa"; /* Fun With Art */

	/* https://www.idealine.info/portfolio/library/text/pgxspec.txt */
	if (nread > 8 && sig[0] == 'P' && sig[1] == 'G' && sig[2] == 'X'
	&& !BE_U32(sig + 4) && sig[3] <= 0x0A)
		return "image/x-atari-pgx";

	/* https://github.com/th-otto/zview/blob/master/zview/plugins/arabesqu/abm.c */
	if (nread > 6 && sig[0] == 'E' && sig[1] == 'S' && sig[2] == 'O'
	&& sig[3] == '8' && ((sig[4] == '8' && sig[5] == 'b')
	|| (sig[4] == '9' && sig[5] == 'a')))
		return "image/x-atari-arabesque";

	if (nread > 37 && sig[2] == 0x0E && !sig[3] && !sig[4]
	&& (sig[5] == 0x0E || sig[5] == 0xE0) && sig[34] == 0x0E
	&& !sig[35] && !sig[36] && (sig[37] == 0x0E || sig[37] == 0xE0))
		return "image/x-atari-colorburst2";

	/* RECOIL: recoil.c:RECOIL_DecodeGrx */
	if (nread >= 1588 && sig[0] == 'G' && sig[1] == 'R' && sig[2] == 'X'
	&& sig[3] == 'P' && sig[4] == 0x01 && sig[5] == 0x01)
		return "image/x-atari-grafix"; /* .grx */

	if (nread > 3 && sig[0] == 'M' && sig[1] == 'G' && sig[2] == 'F'
	&& (sig[3] == '1' || sig[3] == '2'))
		return "image/x-atari-mgf";

	if (nread > 44 && BE_U16(sig + 42) == 0x04 && is_animaster(sig, nread) == 1)
		return "video/x-atari-animaster"; /* .asb, .msk */

	/* http://fileformats.archiveteam.org/wiki/Picture_Publisher */
	if (nread > 7 && sig[0] == 'I' && sig[1] == 'I' && sig[2] == 0x02
	&& sig[3] == 0x01 && sig[4] == 0x01 && !sig[5] && sig[6] == 0x26 && !sig[7])
		return "image/x-micrografx-pp4";
	if (nread > 5 && sig[0] == 'P' && sig[1] == 'P' && sig[2] == 'U'
	&& sig[3] == 'B' && sig[4] == 'I' && sig[5] == 'I')
		return "image/x-micrografx-pp5";

	if (nread > 5 && sig[0] == 'T' && sig[1] == 'M' && sig[2] == '-'
	&& sig[3] == 'F' && sig[4] == 'a' && sig[5] == 'x')
		return "image/x-perfect-fax";

	/* =================================
	 * WEAK MAGIC! Only 2-3 conditions
	 * ================================= */
	if (nread > 2 && sig[0] == 'A' && sig[1] == 'G' && sig[2] == 'S')
		return "image/x-atari-ags";

	/* http://www.textfiles.com/programming/FORMATS/pgcspec.txt */
	if (nread > 2 && sig[0] == 'P' && sig[1] == 'G' && sig[2] == 0x01)
		return "image/x-atari-pgc";

	/* http://fileformats.archiveteam.org/wiki/Lum */
	if (file_size == 4766 && sig[0] == 0x04)
		return "image/x-atari-technicolor";

	/* ICE formats: IPN/IMN, IPC, and IP2 file sizes respectivelly */
	if ((file_size == 17350 || file_size == 17354 || file_size == 17358)
	&& sig[0] == 0x01)
		return "image/x-atari-ice";
	/* Super IRG: IR2 and IRG file sizes respectively */
	if ((file_size == 18310 || file_size == 18314) && sig[0] == 0x01)
		return "image/x-atari-irg";

	/* RECOIL: recoil.c:RECOIL_DecodeAtari8Artist */
	if (file_size == 3206 && sig[0] == 0x07)
		return "image/x-atari-artist";

	const char *mimetype = get_mimetype_from_companion_file(file);
	if (mimetype)
		return mimetype;

	return NULL;
}

static const char *
open_error(const char *file, const int err_no)
{
	if (!file)
		return NULL;

	struct stat st;
	if (lstat(file, &st) != -1) {
		/* We cannot read the file, but we can stat it */
		if (err_no == EACCES && S_ISREG(st.st_mode))
			return _("regular file, no read permission");

		return fs_type_check(&st);
	}

	return NULL;
}

static void
skip_id3_tag(const uint8_t **sig, size_t *nread, const off_t file_size)
{
	const size_t taglen = id3v2_tag_size(*sig, *nread, file_size);
	if (taglen > 0 && taglen < *nread) {
		*nread -= taglen;
		*sig += taglen;
	}
}

/* Read a few kilo bytes from the file FILE and attempt to find out an
 * appropiate MIME type based on the file's content.
 * Returns the found MIME type (as a constant string) or NULL if none is found.
 *
 * Fast magic is ~15x times faster than libmagic! However, note that
 * the magic checks performed here are precisely FAST magic, meaning that it
 * won't be as accurate as libmagic (false positives are expected). The idea
 * is to avoid calling libmagic in those cases where this fast magic is enough.
 * If NULL is returned, libmagic will be used anyway by the xmagic function. */
const char *
fast_magic(const char *file)
{
	if (!file || !*file)
		return NULL;

	int fd = open(file, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
	if (fd == -1)
		return open_error(file, errno);

	struct stat st;
	if (fstat(fd, &st) == -1) {
		close(fd);
		return NULL;
	}

	/* Ignore non-regular files and empty regular files. */
	if (!S_ISREG(st.st_mode) || st.st_size == 0) {
		close(fd);
		return fs_type_check(&st);
	}

	uint8_t buf[BYTES_TO_READ];
	const ssize_t bytes = pread(fd, buf, BYTES_TO_READ, 0);
	close(fd);

	if (bytes <= 0)
		return NULL;

	size_t nread = (size_t)bytes;
	const uint8_t *sig = buf;

	/* Skip the ID3 tag: actual file format data is immediately after the tag. */
	if (nread >= 10 && sig[0] == 'I' && sig[1] == 'D' && sig[2] == '3')
		skip_id3_tag(&sig, &nread, st.st_size);

	const char *mimetype = check_modern_formats(sig, nread, st.st_size);
	if (mimetype)
		return mimetype;

	mimetype = check_legacy_formats(file, sig, nread, st.st_size);
	if (mimetype)
		return mimetype;

	if (nread > 512)
		return detect_startcode_video_stream(sig, 512);

	return NULL;
}

#else
void *skip_me_fast_magic;
#endif /* !NO_FAST_MAGIC */
