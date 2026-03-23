/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* fast_magic.c -- fast MIME-type detection using magic bytes */
/* Note: For the time being, this module handles only binary files. */

#ifndef NO_FAST_MAGIC

#include "helpers.h"

#include <unistd.h> /* close() */
#include <string.h> /* memcmp() */

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

static size_t
id3v2_tag_size(const uint8_t *buf, size_t buflen)
{
	if (buflen < 10
	|| !(buf[0] == 'I' && buf[1] == 'D' && buf[2] == '3'))
		return 0;

	/* Bytes 6..9 are sync-safe size */
	const uint32_t b6 = buf[6], b7 = buf[7], b8 = buf[8], b9 = buf[9];
	/* Validate sync-safe (MSB must be zero) */
	if ((b6 & 0x80) || (b7 & 0x80) || (b8 & 0x80) || (b9 & 0x80))
		return 0; /* Malformed */

	const uint32_t size = (b6 << 21) | (b7 << 14) | (b8 << 7) | b9;
	return (size_t)size + 10;
}

/* Return the MIME-type recorded in a ZIP file or NULL if none is found. */
static const char *
get_mime_from_zip(const uint8_t *str, const size_t str_len)
{
	const uint8_t *s = str;
	const size_t l = str_len;

	/* OpenOffice/Libreoffice */
	if (l >= 41 && s[35] == 't'
	&& memcmp(s, "application/vnd.oasis.opendocument.text", 39) == 0) {
		if (s[39] == '-' && str[40] == 't')
			return "application/vnd.oasis.opendocument.text-template";
		if (s[39] == '-' && str[40] == 'm')
			return "application/vnd.oasis.opendocument.text-master";
		if (s[39] == '-' && str[40] == 'w')
			return "application/vnd.oasis.opendocument.text-web";
		return "application/vnd.oasis.opendocument.text";
	}

	if (l >= 48 && s[35] == 's'
	&& memcmp(s, "application/vnd.oasis.opendocument.spreadsheet", 46) == 0) {
		if (s[46] == '-' && s[47] == 't')
			return "application/vnd.oasis.opendocument.spreadsheet-template";
		if (s[46] == '-' && s[47] == 'm')
			return "application/vnd.oasis.opendocument.spreadsheet-master";
		if (s[46] == '-' && s[47] == 'w')
			return "application/vnd.oasis.opendocument.spreadsheet-web";
		return "application/vnd.oasis.opendocument.spreadsheet";
	}

	if (l >= 49 && s[35] == 'p'
	&& memcmp(s, "application/vnd.oasis.opendocument.presentation", 47) == 0) {
		if (s[47] == '-' && s[48] == 't')
			return "application/vnd.oasis.opendocument.presentation-template";
		if (s[47] == '-' && s[48] == 'm')
			return "application/vnd.oasis.opendocument.presentation-master";
		if (s[47] == '-' && s[48] == 'w')
			return "application/vnd.oasis.opendocument.presentation-web";
		return "application/vnd.oasis.opendocument.presentation";
	}

	if (l >= 45 && s[35] == 'g'
	&& memcmp(s, "application/vnd.oasis.opendocument.graphics", 43) == 0) {
		if (s[43] == '-' && s[44] == 't')
			return "application/vnd.oasis.opendocument.graphics-template";
		if (s[43] == '-' && s[44] == 'm')
			return "application/vnd.oasis.opendocument.graphics-master";
		if (s[43] == '-' && s[44] == 'w')
			return "application/vnd.oasis.opendocument.graphics-web";
		return "application/vnd.oasis.opendocument.graphics";
	}

	if (l >= 44 && s[35] == 'f'
	&& memcmp(s, "application/vnd.oasis.opendocument.formula", 42) == 0) {
		if (s[42] == '-' && s[43] == 't')
			return "application/vnd.oasis.opendocument.formula-template";
		if (s[42] == '-' && s[43] == 'm')
			return "application/vnd.oasis.opendocument.formula-master";
		if (s[42] == '-' && s[43] == 'w')
			return "application/vnd.oasis.opendocument.formula-web";
		return "application/vnd.oasis.opendocument.formula";
	}

	if (l >= 41 && s[35] == 'b'
	&& memcmp(s, "application/vnd.oasis.opendocument.base", 39) == 0) {
		if (s[39] == '-' && s[40] == 't')
			return "application/vnd.oasis.opendocument.base-template";
		if (s[39] == '-' && s[40] == 'm')
			return "application/vnd.oasis.opendocument.base-master";
		if (s[39] == '-' && s[40] == 'w')
			return "application/vnd.oasis.opendocument.base-web";
		return "application/vnd.oasis.opendocument.base";
	}

	if (l >= 40 && s[35] == 'c'
	&& memcmp(s, "application/vnd.oasis.opendocument.chart", 40) == 0)
		return "application/vnd.oasis.opendocument.chart";

	if (l >= 40 && s[35] == 'i'
	&& memcmp(s, "application/vnd.oasis.opendocument.image", 40) == 0)
		return "application/vnd.oasis.opendocument.image";

	/* Old OpenOffice format */
	if (l >= 30 && s[24] == 'w'
	&& memcmp(s, "application/vnd.sun.xml.writer", 30) == 0)
		return "application/vnd.sun.xml.writer";
	if (l >= 28 && s[24] == 'c'
	&& memcmp(s, "application/vnd.sun.xml.calc", 28) == 0)
		return "application/vnd.sun.xml.calc";
	if (l >= 31 && s[24] == 'i'
	&& memcmp(s, "application/vnd.sun.xml.impress", 31) == 0)
		return "application/vnd.sun.xml.impress";
	if (l >= 28 && s[24] == 'd'
	&& memcmp(s, "application/vnd.sun.xml.draw", 28) == 0)
		return "application/vnd.sun.xml.draw";
	if (l >= 28 && s[24] == 'm'
	&& memcmp(s, "application/vnd.sun.xml.math", 28) == 0)
		return "application/vnd.sun.xml.math";
	if (l >= 28 && s[24] == 'b'
	&& memcmp(s, "application/vnd.sun.xml.base", 28) == 0)
		return "application/vnd.sun.xml.base";

	if (l >= 16 && s[6] == 'o' && memcmp(s, "image/openraster", 16) == 0)
		return "image/openraster";

	if (l >= 19 && s[14] == 'k' && memcmp(s, "application/x-krita", 19) == 0)
		return "application/x-krita";

	if (l >= 20 && s[17] == 'z' && memcmp(s, "application/epub+zip", 20) == 0)
		return "application/epub+zip";

	/* KDE Office suit (KOffice) */
	if (l >= 26 && s[12] == 'x' && s[13] == '-' && s[14] == 'k') {
		if (s[15] == 'a' && memcmp(s, "application/x-karbon", 20) == 0)
			return "application/x-karbon";
		if (s[15] == 'c' && memcmp(s, "application/x-kchart", 20) == 0)
			return "application/x-kchart";
		if (s[15] == 'f' && memcmp(s, "application/x-kformula", 22) == 0)
			return "application/x-kformula";
		if (s[15] == 'i' && memcmp(s, "application/x-killustrator", 26) == 0)
			return "application/x-killustrator";
		if (s[15] == 'i' && memcmp(s, "application/x-kivio", 19) == 0)
			return "application/x-kivio";
		if (s[15] == 'o' && memcmp(s, "application/x-kontour", 21) == 0)
			return "application/x-kontour";
		if (s[15] == 'p' && memcmp(s, "application/x-kpresenter", 24) == 0)
			return "application/x-kpresenter";
		if (s[15] == 's' && memcmp(s, "application/x-kspread", 21) == 0)
			return "application/x-kspread";
		if (s[15] == 'w' && memcmp(s, "application/x-kword", 19) == 0)
			return "application/x-kword";
	}

	return NULL;
}

static const char *
check_zip_magic(const uint8_t *str, const size_t str_len)
{
	if (str_len >= 39 && str[30] == 'm' && str[31] == 'i' && str[32] == 'm'
	&& str[33] == 'e' && str[34] == 't' && str[35] == 'y' && str[36] == 'p'
	&& str[37] == 'e')
		return get_mime_from_zip(str + 38, str_len - 38);

	/* STR starts with "PK x03 x04" (4 bytes). Let's skip those bytes. */
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
		if (s[i] == 0xEC && s[i + 1] == 0xA5) { // FibBase (Word)
			const uint16_t nfib = (uint16_t)(s[i + 2] | (s[i + 3] << 8));
			if (nfib == 0x00A0 || nfib == 0x00A1 || nfib == 0x00B0
			|| nfib == 0x00C0 || nfib == 0x00C1 || nfib == 0x00D9)
				return "application/msword";
		}

		if (s[i] == 0x09 && s[i + 1] == 0x08) { // BOF (Excel)
			const uint16_t reclen = (uint16_t)(s[i + 2] | (s[i + 3] << 8));
			if (reclen < 4 || i + 4 + reclen > l)
				continue;

			const uint16_t version = (uint16_t)(s[i + 4] | (s[i + 5] << 8));
			const uint16_t type = (uint16_t)(s[i + 6] | (s[i + 7] << 8));
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
check_mp3_magic(const uint8_t *buf, const size_t buf_size)
{
	const size_t taglen = id3v2_tag_size(buf, buf_size);
	if (taglen == 0)
		return NULL;

	if (taglen + 2 <= buf_size) {
		if (buf[taglen] == 0xFF && (buf[taglen + 1] & 0xE0) == 0xE0)
			return "audio/mp3";
		return NULL;
	}

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

	if (l > 1 && s[0] == 'q' && s[1] == 't') // .mov, .qt */
		/* ADD: also odcf, opf2, opx2, pana, piff, pnvi */
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

/* See http://fileformats.archiveteam.org/wiki/EBML */
static const char *
check_ebml_magic(const uint8_t *s, const size_t nread)
{
	size_t n = 0;
	for (size_t i = 4; i < nread - 3; i++) {
		if (s[i] == 0x42 && s[i + 1] == 0x82) {
			n = i + 3; /* Skip 0x42, 0x82, and VINT-byte */
			break;
		}
	}

	if (n == 0)
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

	if (buf[8] == 'W' && buf[9] == 'E' && buf[10] == 'B' && buf[11] == 'P') // webp
		return "image/webp";
	if (buf[8] == 'A' && buf[9] == 'V' && buf[10] == 'I' && buf[11] == ' ') // avi
		return "video/x-msvideo";
	if (buf[8] == 'W' && buf[9] == 'A' && buf[10] == 'V' && buf[11] == 'E') // wav
		return "audio/x-wav";
	if (buf[8] == 'A' && buf[9] == 'M' && buf[10] == 'V' && buf[11] == ' ') // amv
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

	if (buf[8] == 's' && buf[9] == 'h') {
		if (buf[10] == 'w' && (buf[11] == '4' || buf[11] == '5'))
			return "application/x-corel-shw";
		if ((buf[10] == 'r' || buf[10] == 'l') && buf[11] == '5')
			return "application/x-corel-shw";
	}

	/* See http://justsolve.archiveteam.org/wiki/CorelDRAW */
	if (buf[8] == 'C' && buf[9] == 'M' && buf[10] == 'X' && buf[11] == '1')
		return "application/vnd.corel-draw";
	if (buf_len > 15 && buf[8] == 'C' && buf[9] == 'D' && buf[10] == 'R'
	&& buf[12] == 'v' && buf[13] == 'r' && buf[14] == 's' && buf[15] == 'n')
		return "application/vnd.corel-draw";

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
	&& memcmp(p + n, "playlist version=", 17) == 0)
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

static const char *
check_iff_magic(const uint8_t *s, const size_t slen)
{
	if (slen < 8)  /* S starts with "FORMxxxx" */
		return NULL;

	const uint8_t *p = s + 8;
	const size_t plen = slen - 8;

	if (plen > 3 && p[0] == 'A' && p[1] == 'I' && p[2] == 'F') {
		if (p[3] == 'F') return "audio/x-aiff";
		if (p[3] == 'C') return "audio/x-aifc";
	}

	if (plen > 3 && p[0] == '8' && p[1] == 'S' && p[2] == 'V' && p[3] == 'X')
		return "audio/x-aiff";

	if (plen > 2 && p[0] == 'P' && p[1] == 'B' && p[2] == 'M')
		return "image/x-ilbm";

	/* See http://fileformats.archiveteam.org/wiki/ILBM */
	if (plen > 3 && ((p[0] == 'I' && p[1] == 'L' && p[2] == 'B' && p[3] == 'M')
	|| (p[0] == 'A' && p[1] == 'C' && p[2] == 'B' && p[3] == 'M')
	|| (p[0] == 'R' && p[1] == 'G' && p[2] == 'B' && p[3] == 'N')
	|| (p[0] == 'R' && p[1] == 'G' && p[2] == 'B' && p[3] == '8')))
		return "image/x-ilbm";

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
		return "video/x-quicktime";

	if (slen > 3 && s[0] == 'p' && s[1] == 'c' && s[2] == 'k' && s[3] == 'g')
		return "application/x-quicktime-player";

	return "video/x-sgi-movie";
}

static const char *
get_ms_exec_type(const uint8_t *s, const size_t slen)
{
	if (slen < 0x3C + 4) /* Need bytes 0x3C..0x3F */
		return "application/x-dosexec";

	/* Read little-endian dword at 0x3C (e_lfanew pointer) */
	uint32_t n = (uint32_t)s[0x3C]
		| ((uint32_t)s[0x3D] << 8)
		| ((uint32_t)s[0x3E] << 16)
		| ((uint32_t)s[0x3F] << 24);

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
	// S is "push "
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

static char *
fs_type_check(const struct stat *a)
{
	if (!a)
		return NULL;

	if (a->st_size == 0 && S_ISREG(a->st_mode))
		return "inode/x-empty";

	switch (a->st_mode & S_IFMT) {
	case S_IFDIR: return "inode/directory";
	case S_IFLNK: return "inode/symlink";
	case S_IFIFO: return "inode/fifo";
	case S_IFSOCK: return "inode/socket";
	case S_IFBLK: return "inode/blockdevice";
	case S_IFCHR: return "inode/chardevice";
	/* Let non-standard file modes be handled by libmagic itself */
	default: break;
	}

	return NULL;
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
	case 1: return "application/x-object";
	case 2: /* Fallthrough */
	case 3: return "application/x-executable";
	case 4: return "application/x-coredump";
	default: return "application/octet-stream";
	}
}

static const char *
check_pre_ole2_office_docs(const uint8_t *s, const size_t slen)
{
	if (slen > 128 && s[0] == 0x31 && s[1] == 0xBE && s[2] == 0x00
	&& s[3] == 0x00 && s[128] > 0) { /* Pre OLE2 Word (DOS Word 5) */
		uint16_t b = (uint16_t)s[96] | (uint16_t)((uint16_t)s[97] << 8);
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

	struct stat st;

	int fd = open(file, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
	if (fd == -1) {
		if (lstat(file, &st) != -1)
			return fs_type_check(&st);
		return NULL;
	}

	if (fstat(fd, &st) == -1) {
		close(fd);
		return NULL;
	}

	/* Ignore non-regular files and empty regular files */
	if (!S_ISREG(st.st_mode) || st.st_size == 0) {
		close(fd);
		return fs_type_check(&st);
	}

	FILE *f = fdopen(fd, "rb");
	if (!f)
		return NULL;

#define BYTES_TO_READ 4096
	uint8_t sig[BYTES_TO_READ];
	const size_t nread = fread(sig, 1, BYTES_TO_READ, f);
	if (ferror(f) || nread == 0) {
		fclose(f);
		return NULL;
	}
#undef BYTES_TO_READ

	fclose(f);

/* Tier 1: Ubiquitous */
	if (nread > 2 && sig[0] == 0xFF && sig[1] == 0xD8 && sig[2] == 0xFF)
		return "image/jpeg";

	if (nread > 7 && sig[0] == 0x89 && sig[1] == 'P' && sig[2] == 'N'
	&& sig[3] == 'G' && sig[4] == 0x0D && sig[5] == 0x0A && sig[6] == 0x1A
	&& sig[7] == 0x0A) {
		/* It at offset 12 we find "CgBI" instead of "IHDR", we have an
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

	if (nread > 2 && sig[0] == 'I' && sig[1] == 'D' && sig[2] == '3')
		return check_mp3_magic(sig, nread);

	/* At least Adobe Reader versions 3 through 6 did support this header:
	 * %!PS-Adobe-N.n PDF-M.m
	 * However, both file(1) and mimetype(1) take this as a postscript file. */
	 /* Malformed PDFs may have leading garbage bytes */
	if (nread > 4 && sig[0] == '%' && sig[1] == 'P' && sig[2] == 'D'
	&& sig[3] == 'F' && sig[4] == '-')
		return "application/pdf";

	if (nread > 2 && sig[0] == 0x1F && sig[1] == 0x8B && sig[2] == 0x08)
		if (nread > 12 && sig[10] == 'K' && sig[11] == 'O' && sig[12] == 'f') {
			return check_gzipped_koffice(sig, nread);
		return "application/gzip";
	}

	if (nread > 2 && sig[0] == 'B' && sig[1] == 'Z' && sig[2] == 'h')
		return "application/x-bzip2";

////////////

	if (nread > 3 && sig[0] == 0x28 && sig[1] == 0xB5 && sig[2] == 0x2F
	&& sig[3] == 0xFD)
		return "application/zstd";

	if (nread > 5 && sig[0] == 'G' && sig[1] == 'I' && sig[2] == 'F'
	&& sig[3] == '8' && (sig[4] == '7' || sig[4] == '9') && sig[5] == 'a')
		return "image/gif"; /* Either "GIF87a" or "GIF89a" */

	if (nread > 3 && sig[0] == 'f' && sig[1] == 'L' && sig[2] == 'a'
	&& sig[3] == 'C') // flac
		return "audio/flac";

	if (nread >= 10 && sig[0] == '%' && memcmp(sig, "%!PS-Adobe", 10) == 0) {
		if (nread > 17 && sig[15] == 'E' && sig[16] == 'P' && sig[17] == 'S')
			return "image/x-eps";
		return "application/postscript";
	}

	if (nread > 4 && sig[0] == '<' && sig[1] == '?' && sig[2] == 'x'
	&& sig[3] == 'm' && sig[4] == 'l') // Mostly SVG
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
	&& (sig[2] == 0x09 || sig[2] == '\n' || sig[2] == 0x0D || sig[2] == '#')) {
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

	if (nread > 5 && sig[0] == 0x89 && sig[1] == 'H' && sig[2] == 'D'
	&& sig[3] == 'F' && sig[4] == 0x0D && sig[5] == 0x0a && sig[6] == 0x1A
	&& sig[7] == 0x0A)
		return "application/x-hdf5";

	if (nread >= 4 && ((sig[0] == 'I' && sig[1] == 'I'
	&& (sig[2] == '+' || sig[2] == '*') && sig[3] == 0x00)
	|| (sig[0] == 'M' && sig[1] == 'M' && sig[2] == 0x00
	&& (sig[3] == '+' || sig[3] == '*')))) {
		if (nread >= 12 && sig[8] == 'C' && sig[9] == 'R'
		&& ((sig[10] == '2' && sig[11] == 0x02) || sig[10] == 0x02))
			return "image/x-canon-cr2";
		return "image/tiff";
	}

	if (nread >= 5 && ((sig[0] == 'S' && sig[1] == 'D' && sig[2] == 'P'
	&& sig[3] == 'X') || (sig[0] == 'X' && sig[1] == 'P' && sig[2] == 'D'
	&& sig[3] == 'S')))
		return "image/x-dpx";

	if (nread >= 4 && sig[0] == 0x76 && sig[1] == 0x2F && sig[2] == 0x31
	&& sig[3] == 0x01)
		return "image/x-exr";

	if (nread >= 4 && sig[0] == 'B' && sig[1] == 'P' && sig[2] == 'G'
	&& sig[3] == 0xFB)
		return "image/bpg";

	if (nread > 8 && sig[0] == 'S' && sig[1] == 'I' && sig[2] == 'M'
	&& sig[3] == 'P' && sig[4] == 'L' && sig[5] == 'E' && sig[6] == ' '
	&& sig[7] == ' ' && sig[8] == '=')
		return "image/fits"; /* or application/fits (Shared MIME-info) */

	if (nread >= 8 && sig[0] == 0x8B && sig[1] == 'J' && sig[2] == 'N'
	&& sig[3] == 'G' && sig[4] == 0x0d && sig[5] == 0x0a && sig[6] == 0x1a
	&& sig[7] == 0x0a)
		return "image/x-jng";

	if (nread >= 10 && sig[0] == '#' && sig[1] == '?'
	&& memcmp(sig + 2, "RADIANCE", 8) == 0)
		return "image/vnd.radiance";

	if (nread >= 8 && sig[0] == '8' && sig[1] == 'B' && sig[2] == 'P'
	&& sig[3] == 'S' && sig[4] == 0x00 && (sig[5] == 0x01 || sig[5] == 0x02))
		return "image/vnd.adobe.photoshop";

	if (nread >= 8 && sig[0] == 0x8A && sig[1] == 'M' && sig[2] == 'N'
	&& sig[3] == 'G' && sig[4] == '\r' && sig[5] == '\n' && sig[6] == 0x1A
	&& sig[7] == '\n')
		return "video/x-mng";

	if (nread >= 20 && sig[0] == 'i' && sig[1] == 'd' && sig[2] == '='
	&& memcmp(sig + 3, "ImageMagick version=", 20) == 0)
		return "image/x-miff";

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
		if (sig[3] == 0xB3 || sig[3] == 0xBA) return "video/mpeg";
		if (sig[3] == 0xB0 || sig[3] == 0xB5) return "video/mpeg4-generic";
	}

	if (nread > 388 &&
	((sig[0] == 0x47 && sig[188] == 0x47 && sig[376] == 0x47)
	|| (sig[4] == 0x47 && sig[196] == 0x47 && sig[388] == 0x47)))
		return "video/MP2T";

	if (nread >= 14 && sig[0] == 0x06
	&& memcmp(sig, "\x06\x0E\x2B\x34\x02\x05\x01\x01\x0D\x01\x02\x01\x01\x02", 14) == 0)
		return "application/mxf";

	if (nread >= 16 && sig[0] == 'S' && sig[1] == 'Q'
	&& memcmp(sig, "SQLite format 3\0", 16) == 0)
		return "application/vnd.sqlite3";

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
	uint16_t word = nread >= 2 ? (uint16_t)((sig[0] << 8) | sig[1]) : 0;
	if ((word & 0xFFF6) == 0xFFF0)
		return "audio/x-hx-aac-adts";
	uint16_t wmasked = word & 0xFFFE;
	if (wmasked == 0xFFFA || wmasked == 0xFFFC || wmasked == 0xFFF2
	|| wmasked == 0xFFF4 || wmasked == 0xFFF6 || wmasked == 0xFFE2)
		return "audio/mpeg";

	if (nread > 4 && sig[0] == '#' && sig[1] == '!' && sig[2] == 'A'
	&& sig[3] == 'M' && sig[4] == 'R') {
		if (nread > 7 && sig[5] == '-' && sig[6] == 'W' && sig[7] == 'B')
			return "audio/AMR-WB";
		return "audio/AMR";
	}

	if (nread > 80 && sig[0] == 'D' && sig[1] == 'S' && sig[2] == 'D'
	&& sig[3] == ' ' && sig[28] == 'f' /* fmt */ && sig[80] == 'd' /* data */)
		return "audio/x-dsf";

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

	if (nread > 12 && ((sig[0] == 0x00 && sig[1] == 0x01 && sig[2] == 0x00
	&& sig[3] == 0x00 && sig[4] < 48)
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

	if (nread >= 2 && (sig[0] & 0x0F) == 8 && (sig[0] & 0x80) == 0
	&& (sig[1] & 0x20) == 0
	&& ((uint16_t)sig[0] << 8 | (uint16_t)sig[1]) % 31 == 0)
		return "application/zlib";

	if (nread > 6 && sig[0] == 'B' && sig[1] == 'L' && sig[2] == 'E'
	&& sig[3] == 'N' && sig[4] == 'D' && sig[5] == 'E' && sig[6] == 'R')
		return "application/x-blender";

	if (nread > 3 && sig[0] == 0xCA && sig[1] == 0xFE) {
		if (sig[2] == 0xBA && sig[3] == 0xBE) return "application/x-java-applet";
		if (sig[2] == 0xD0 && sig[3] == 0x0D) return "application/x-java-pack200";
	}

	if (nread >= 518 && sig[514] == 'H' && sig[515] == 'd' && sig[516] == 'r'
	&& sig[517] == 'S')
		return "application/x-linux-kernel";

	if (nread > 18 && sig[0] == 0x7f && sig[1] == 'E' && sig[2] == 'L'
	&& sig[3] == 'F')
		return check_elf_magic(sig, nread);

	if (nread > 105 && sig[102] == 'm' && sig[103] == 'B' && sig[104] == 'I'
	&& sig[105] == 'N')
		return "application/x-macbinary";

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
	if (nread > 6 && sig[0] == '#' && sig[1] == ' ' && sig[2] == 'B'
	&& memcmp(sig, "# Blender MTL File: '", 21) == 0)
		return "model/mtl";

	if (nread > 3 && sig[0] == 'h' && sig[1] == 's' && sig[2] == 'i'
	&& sig[3] == '1')
		return "image/x-hsi";

	if (nread > 3 && sig[0] == 'P' && sig[1] == 'D' && sig[2] == 'N'
	&& sig[3] == '3')
		return "image/x-paintnet";

	if (nread >= 12 && sig[0] == 'D' && sig[1] == 'V' && sig[2] == 'D'
	&& (memcmp(sig + 3, "VIDEO-VMG", 9) == 0
	|| memcmp(sig + 3, "VIDEO-VTS", 9) == 0))
		return "video/x-ifo";

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
		return "image/astc";

	if (nread > 3 && sig[0] == 'S' && ((sig[1] == 'I' && sig[2] == 'T'
	&& sig[3] == '!') || (nread > 6 && sig[1] == 't' && sig[2] == 'u'
	&& sig[3] == 'f' && sig[4] == 'f' && sig[5] == 'I' && sig[6] == 't')))
		return "application/x-stuffit";

	if (nread >= 14) {
		uint16_t v12 = (uint16_t)sig[12] | (uint16_t)((uint16_t)sig[13] << 8);
		if (v12 == 0 || v12 == 0xFF) {
			uint32_t lelong = (uint32_t)sig[0] | ((uint32_t)sig[1] << 8)
				| ((uint32_t)sig[2] << 16) | ((uint32_t)sig[3] << 24);
			if ((lelong & 0x00FFFFFF) == 0x5D)
				return "application/lzma";
		}
	}

			/* #############################
			 * #       LEGACY/OBSOLETE     #
			 * ############################# */

	if (nread > 7 && sig[0] == 0xD0 && sig[1] == 0xCF && sig[2] == 0x11
	&& sig[3] == 0xE0 && sig[4] == 0xA1 && sig[5] == 0xB1 && sig[6] == 0x1A
	&& sig[7] == 0xE1) /* MS Compound Document Format - OLE2 */
		return get_ole2_ms_office_type(sig, nread);

	const char *ret = check_pre_ole2_office_docs(sig, nread);
	if (ret)
		return ret;

	if (nread > 17 && sig[0] == 'B' && sig[1] == 'M') {
		const uint32_t dib_header = (uint32_t)sig[14] | (uint32_t)sig[15] << 8
			| (uint32_t)sig[16] << 16 | (uint32_t)sig[17] << 24;
		if (dib_header == 12 || dib_header == 40 || dib_header == 56
		|| dib_header == 64 || dib_header == 108 || dib_header == 124)
			return "image/bmp";
	}

	if (nread > 91 && sig[0] == 0x53 && sig[1] == 0x80 && sig[2] == 0xF6
	&& sig[3] == 0x34 && sig[88] == 'P' && sig[89] == 'I' && sig[90] == 'C'
	&& sig[91] == 'T') /* Softimage PIC (discontinuated in 2015) */
		return "image/x-pic";

	if (nread >= 16 && sig[0] == 0x30 && sig[1] == 0x26
	&& memcmp(sig, "\x30\x26\xB2\x75\x8E\x66\xCF\x11\xA6\xD9\x00\xAA\x00\x62\xCE\x6C", 16) == 0)
		return "video/x-ms-asf"; /* asf, wma, wmv */

	if (nread > 3 && sig[0] == 0x0A && sig[1] <= 0x05 && sig[2] == 0x01
	&& sig[3] >= 0x01 && sig[3] <= 0x08)
		return "image/vnd.zbrush.pcx";

	if (nread > 4 && sig[0] == '{' && sig[1] == '\\' && sig[2] == 'r'
	&& sig[3] == 't' && sig[4] == 'f')
		return "text/rtf"; /* application/rtf (MIME-info) */

	if (nread > 9 && sig[0] == 'M' && sig[1] == 'Z' && sig[7] == 0x00
	&& sig[9] == 0x00)
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
	&& sig[3] == 'M') /* Mostly AIFF */
		return check_iff_magic(sig, nread);

	if (nread > 4 && sig[0] == '.' && sig[1] == 's' && sig[2] == 'n'
	&& sig[3] == 'd' && sig[4] == 0x00)
		return "audio/basic";

	if (nread > 3 && sig[0] == 'T' && sig[1] == 'T' && sig[2] == 'A'
	&& sig[3] == '1')
		return "audio/x-tta";

	if (nread > 1 && sig[0] == 0x0B && sig[1] == 0x77)
		return "audio/ac3";

	if (nread > 3 && sig[0] == 'I' && sig[1] == 'M' && sig[2] == 'P'
	&& sig[3] == 'M')
		return "audio/x-it";

	if (nread > 3 && (sig[0] == 'I' || sig[0] == 'P') && sig[1] == 'W'
	&& sig[2] == 'A' && sig[3] == 'D')
		return "application/x-doom-wad";

	if (nread > 3 && sig[0] == 'D' && sig[1] == 'O' && sig[2] == 'S'
	&& sig[3] == 0x00)
		return "application/x-amiga-disk-format";

	if (nread > 3 && sig[0] == 'F' && sig[1] == 'L' && sig[2] == 'V'
	&& sig[3] == 0x01)
		return "video/x-flv";

	if (nread > 16 && sig[4] == 0x12 && sig[5] == 0xAF && sig[12] == 0x08)
		return "video/x-flc";

	if (nread > 3 && sig[0] == 'D' && sig[1] == 'K' && sig[2] == 'I'
	&& sig[3] == 'F')
		return "video/x-ivf";

	if (nread > 3 && sig[0] == 'N' && sig[1] == 'S' && sig[2] == 'V'
	&& sig[3] == 'f') /* Nullsoft video */
		return "video/x-nsv";

	if (nread > 13 && sig[5] == 0xAF && sig[12] == 0x08 && sig[13] == 0x00) {
		if (sig[4] == 0x11) return "video/x-fli";
		if (sig[4] == 0x12) return "video/x-flc";
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

	if (nread > 47 && sig[44] == 'S' && sig[45] == 'C' && sig[46] == 'R'
	&& sig[47] == 'M')
		return "audio/x-s3m";

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

	if (nread > 2 && (sig[2] == 1 || sig[2] == 2 || sig[2] == 3
	|| sig[2] == 9 || sig[2] == 10 || sig[2] == 11 || sig[2] == 32
	|| sig[2] == 33) && is_tga_image(sig, nread) == 1)
		return "image/x-tga";

	if (nread > 1 && sig[0] == 0x11 && (sig[1] == 0x06 || sig[1] == 0x09))
		return "image/x-award-bioslogo";

	if (nread > 7 && sig[4] == 'i' && sig[5] == 'd' && ((sig[6] == 's'
	&& sig[7] == 'c') || (sig[6] == 'a'	&& sig[7] == 't')))
		return "image/x-quicktime";

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
	&& sig[3] == 'C')
		return "application/vnd.wordperfect";

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

	if (nread > 4 && sig[0] == 0x00 && sig[1] == 0x00 && sig[2] == 0x01
	&& sig[3] == 0x00 && sig[4] == 0x00)
		return "application/x-dfont"; /* MacOS font */

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

	if (nread > 2 && sig[0] == 'R' && sig[1] == 'K' && sig[2] == 'A')
		return "application/x-rka";

	if (nread > 3 && sig[0] == 'N' && sig[1] == 'G' && sig[2] == 0x00
	&& sig[3] == 0x01)
		return "application/x-norton-guide";

	if (nread > 4 && sig[0] == 'A' && sig[1] == 'L' && sig[2] == 'Z'
	&& sig[3] == 0x01 && sig[4] == 0x0A)
		return "application/x-alz";

	if (nread > 3 && sig[0] == 0xB1 && sig[1] == 0x68 && sig[2] == 0xDE
	&& sig[3] == 0x3A)
		return "image/x-dcx";

	if (nread > 2 && sig[0] == 'D' && sig[1] == 'D' && sig[2] == 'S')
		return "image/x-dds";

	if (nread > 6 && sig[0] == 'P' && sig[1] == '7' && sig[2] == ' '
	&& sig[3] == '3' && sig[4] == '3' && sig[5] == '2' && sig[6] == 0x0A)
	return "image/x-xv-thumbnail";

	if (nread > 3 && sig[1] == 'F' && sig[2] == 'I' && sig[3] == 'G') {
		if (sig[0] == '#') return "image/x-xfig";
		if (sig[0] == 0x14) return "image/x-deskmate-fig";
	}

	if (nread > 7 && sig[0] == 0x97 && sig[1] == 'J' && sig[2] == 'B'
	&& sig[3] == '2' && sig[4] == 0x0D && sig[5] == 0x0A && sig[6] == 0x1A
	&& sig[7] == 0x0A)
		return "image/jbig2";

	if (nread > 12 && sig[0] == '(' && sig[1] == 0x00 && sig[2] == 0x00
	&& sig[3] == 0x00 && sig[12] == 0x01)
		return "image/x-ms-bmp"; /* Or image/x-dib (Shared MIME-info) */

	if (nread > 5 && sig[0] == 0xFF && sig[1] == 0xFF && sig[2] == '0'
	&& sig[3] == 'S' && sig[4] == 'O' && sig[5] == 0x7f)
		return "image/x-atari-ged";

	if (nread > 3 && sig[0] == 0x0E && sig[1] == 0x03 && sig[2] == 0x13
	&& sig[3] == 0x01)
		return "application/x-hdf";

	if (nread > 527 && ((sig[522] == 0x00 && sig[523] == 0x11 && sig[524] == 0x02
	&& sig[525] == 0xFF && sig[526] == 0x0C && sig[527] == 0x00) /* version 2 */
	|| (sig[522] == 0x11 && sig[523] == 0x00))) /* version 1 */
		return "image/x-pict"; /* Macintosh QuickDraw */

	if (nread > 8 && sig[0] == 'C' && sig[1] == 'P' && sig[2] == 'T'
	&& (sig[3] >= '0' && sig[3] <= '9') && sig[4] == 'F' && sig[5] == 'I'
	&& sig[6] == 'L' && sig[7] == 'E')
		return "image/x-corel-cpt";

	if (nread > 3 && sig[0] == 0x59 && sig[1] == 0xA6 && sig[2] == 0x6A
	&& sig[3] == 0x95)
		return "image/x-sun-raster";

	if (nread > 3 && sig[0] == 'I' && sig[1] == 'I' && sig[2] == 'N'
	&& sig[3] == '1') /* Navy Image File Format */
		return "image/x-niff";

	if (nread > 3 && sig[0] == 'N' && ((sig[1] == 'I' && sig[2] == 'T')
	|| (sig[1] == 'S' && sig[2] == 'I')) && sig[3] == 'F')
		return "image/vnd.nitf"; /* National Imagery Transmission Format */

	if (nread > 7 && sig[0] == 'I' && sig[1] == 'T' && sig[2] == 'O'
	&& sig[3] == 'L' && sig[4] == 'I' && sig[5] == 'T' && sig[6] == 'L'
	&& sig[7] == 'S')
		return "application/x-ms-reader";

	if (nread > 5 && sig[0] == 0x00 && sig[1] == 0x00 && sig[3] == 0x00
	&& sig[4] > 0x00 && sig[5] == 0x00) {
		if (sig[2] == 0x01)	return "image/vnd.microsoft.icon";
		if (sig[2] == 0x02)	return "image/x-win-bitmap";
	}

	if (nread > 82 && sig[34] == 'L' && sig[35] == 'P' && sig[82] != 0x00
	&& memcmp(sig + 64, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16) == 0)
		return "application/vnd.ms-fontobject";

	return NULL;
}

#else
void *skip_me_fast_magic;
#endif /* !NO_FAST_MAGIC */
