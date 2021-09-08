///////////////////////////////////////////////////////////////////////////////
// \author (c) Marco Paland (info@paland.com)
//             2014-2019, PALANDesign Hannover, Germany
//
// \license The MIT License (MIT)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// \brief Tiny printf, sprintf and (v)snprintf implementation, optimized for speed on
//        embedded systems with a very limited resources. These routines are thread
//        safe and reentrant!
//        Use this instead of the bloated standard/newlib printf cause these use
//        malloc for printf (and may not be thread safe).
//
///////////////////////////////////////////////////////////////////////////////

#include <stdbool.h>
#include <stdint.h>

#include "printf.h"


#include <stdio.h>

// define this globally (e.g. gcc -DPRINTF_INCLUDE_CONFIG_H ...) to include the
// printf_config.h header file
// default: undefined
#ifdef PRINTF_INCLUDE_CONFIG_H
#include "printf_config.h"
#endif


// 'ntoa' conversion buffer size, this must be big enough to hold one converted
// numeric number including padded zeros (dynamically created on stack)
// default: 32 byte
#ifndef PRINTF_NTOA_BUFFER_SIZE
#define PRINTF_NTOA_BUFFER_SIZE    32U
#endif

// 'ftoa' conversion buffer size, this must be big enough to hold one converted
// float number including padded zeros (dynamically created on stack)
// default: 32 byte
#ifndef PRINTF_FTOA_BUFFER_SIZE
#define PRINTF_FTOA_BUFFER_SIZE    32U
#endif

// support for the floating point type (%f)
// default: activated
#ifndef PRINTF_DISABLE_SUPPORT_FLOAT
#define PRINTF_SUPPORT_FLOAT
#endif

// support for exponential floating point notation (%e/%g)
// default: activated
#ifndef PRINTF_DISABLE_SUPPORT_EXPONENTIAL
#define PRINTF_SUPPORT_EXPONENTIAL
#endif

// define the default floating point precision
// default: 6 digits
#ifndef PRINTF_DEFAULT_FLOAT_PRECISION
#define PRINTF_DEFAULT_FLOAT_PRECISION  6U
#endif

// define the largest float suitable to print with %f
// default: 1e9
#ifndef PRINTF_MAX_FLOAT
#define PRINTF_MAX_FLOAT  1e9
#endif

// support for the long long types (%llu or %p)
// default: activated
#ifndef PRINTF_DISABLE_SUPPORT_LONG_LONG
#define PRINTF_SUPPORT_LONG_LONG
#endif

// support for the ptrdiff_t type (%t)
// ptrdiff_t is normally defined in <stddef.h> as long or long long type
// default: activated
#ifndef PRINTF_DISABLE_SUPPORT_PTRDIFF_T
#define PRINTF_SUPPORT_PTRDIFF_T
#endif

///////////////////////////////////////////////////////////////////////////////

// internal flag definitions
#define FLAGS_ZEROPAD   (1U <<  0U)
#define FLAGS_LEFT      (1U <<  1U)
#define FLAGS_PLUS      (1U <<  2U)
#define FLAGS_SPACE     (1U <<  3U)
#define FLAGS_HASH      (1U <<  4U)
#define FLAGS_UPPERCASE (1U <<  5U)
#define FLAGS_CHAR      (1U <<  6U)
#define FLAGS_SHORT     (1U <<  7U)
#define FLAGS_LONG      (1U <<  8U)
#define FLAGS_LONG_LONG (1U <<  9U)
#define FLAGS_PRECISION (1U << 10U)
#define FLAGS_ADAPT_EXP (1U << 11U)


// import float.h for DBL_MAX
#if defined(PRINTF_SUPPORT_FLOAT)
#include <float.h>
#endif


// output function type
typedef void (*out_fct_type)(char character, void* buffer, size_t idx, size_t maxlen);


// wrapper (used as buffer) for output function type
typedef struct {
  void  (*fct)(char character, void* arg);
  void* arg;
} out_fct_wrap_type;


// internal null output
static inline void _out_null(char character, void* buffer, size_t idx, size_t maxlen)
{
  (void)character; (void)buffer; (void)idx; (void)maxlen;
}
/*
static inline void _putchar(char c)
{
	fputc(c, stdout);
} */

// internal _putchar wrapper
static inline void _out_char(char character, void* buffer, size_t idx, size_t maxlen)
{
  (void)buffer; (void)idx; (void)maxlen;
  if (character) {
//    _putchar(character);
    fputc(character, stdout);
  }
}

// output the specified string in reverse, taking care of any zero-padding
static size_t _out_rev(out_fct_type out, char* buffer, size_t idx, size_t maxlen, const char* buf, size_t len, unsigned int width)
{
  // pad spaces up to given width
  for (size_t i = len; i < width; i++)
    out(' ', buffer, idx++, maxlen);

  // reverse string
  while (len) {
    out(buf[--len], buffer, idx++, maxlen);
  }

  return idx;
}

// internal itoa format
static size_t _ntoa_format(out_fct_type out, char* buffer, size_t idx, size_t maxlen, char* buf, size_t len, bool negative, unsigned int base, unsigned int prec, unsigned int width, unsigned int flags)
{
  // handle hash
  if (flags & FLAGS_HASH) {
    if (!(flags & FLAGS_PRECISION) && len && ((len == prec) || (len == width))) {
      len--;
      if (len && (base == 16U)) {
        len--;
      }
    }
    if ((base == 16U) && !(flags & FLAGS_UPPERCASE) && (len < PRINTF_NTOA_BUFFER_SIZE)) {
      buf[len++] = 'x';
    }
    else if ((base == 16U) && (flags & FLAGS_UPPERCASE) && (len < PRINTF_NTOA_BUFFER_SIZE)) {
      buf[len++] = 'X';
    }
    else if ((base == 2U) && (len < PRINTF_NTOA_BUFFER_SIZE)) {
      buf[len++] = 'b';
    }
    if (len < PRINTF_NTOA_BUFFER_SIZE) {
      buf[len++] = '0';
    }
  }

  if (len < PRINTF_NTOA_BUFFER_SIZE) {
    if (negative) {
      buf[len++] = '-';
    }
    else if (flags & FLAGS_PLUS) {
      buf[len++] = '+';  // ignore the space if the '+' exists
    }
    else if (flags & FLAGS_SPACE) {
      buf[len++] = ' ';
    }
  }

  return _out_rev(out, buffer, idx, maxlen, buf, len, width);
}


// internal itoa for 'long' type
static size_t _ntoa_long(out_fct_type out, char* buffer, size_t idx, size_t maxlen, unsigned long value, bool negative, unsigned long base, unsigned int prec, unsigned int width, unsigned int flags)
{
  char buf[PRINTF_NTOA_BUFFER_SIZE];
  size_t len = 0U;

  // no hash for 0 values
  if (!value) {
    flags &= ~FLAGS_HASH;
  }

  // write if precision != 0 and value is != 0
  if (!(flags & FLAGS_PRECISION) || value) {
    do {
      const char digit = (char)(value % base);
      buf[len++] = digit < 10 ? '0' + digit : 'a' + digit - 10;
      value /= base;
    } while (value && (len < PRINTF_NTOA_BUFFER_SIZE));
  }

  return _ntoa_format(out, buffer, idx, maxlen, buf, len, negative, (unsigned int)base, prec, width, flags);
}

// internal vsnprintf
static void __vsnprintf(out_fct_type out, char* buffer, const size_t maxlen, const char* format, va_list va)
{
  unsigned int flags, width = 0, precision = 0;
  size_t idx = 0U;

  if (!buffer)
    // use null output function
    out = _out_null;

  while (*format) {
    // format specifier?  %[flags][width][.precision][length]
    if (*format != '%') {
      // no
      out(*format, buffer, idx++, maxlen);
      format++;
      continue;
    } else {
      // yes, evaluate it
      format++;
    }

    // evaluate flags
    flags = 0U;

    // evaluate specifier
    switch (*format) {
      case 'd' : {
        // set the base
        unsigned int base = 10U;
        flags &= ~FLAGS_HASH;   // no hash for dec format
        const int value = va_arg(va, int);
        idx = _ntoa_long(out, buffer, idx, maxlen, (unsigned int)(value > 0 ? value : 0 - value), value < 0, base, precision, width, flags);
        }
        format++;
        break;

      case 's' : {
        const char* p = va_arg(va, char*);
        // string output
        while ((*p != 0))
            out(*(p++), buffer, idx++, maxlen);
        format++;
        break;
      }

      case '%' :
        out('%', buffer, idx++, maxlen);
        format++;
        break;

      default :
        out(*format, buffer, idx++, maxlen);
        format++;
        break;
    }
  }

  // termination
  out((char)0, buffer, idx < maxlen ? idx : maxlen - 1U, maxlen);

  // return written chars without terminating \0
  return;
}

///////////////////////////////////////////////////////////////////////////////

void printf_(const char* format, ...)
{
  va_list va;
  va_start(va, format);
  char buffer[1];
  __vsnprintf(_out_char, buffer, (size_t)-1, format, va);
  va_end(va);
}
