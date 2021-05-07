#pragma once

#include <sys/types.h>
#include <stddef.h>

int xstrcmp(const char *s1, const char *s2);
int xstrncmp(const char *s1, const char *s2, size_t n);

char * xstrcpy(char *buf, const char *restrict str);
char * xstrncpy(char *buf, const char *restrict str, size_t n);
size_t xstrsncpy(char *restrict dst, const char *restrict src, size_t n);

size_t wc_xstrlen(const char *restrict str);
size_t u8_xstrlen(const char *restrict str);
size_t xstrlen(const char *restrict s);

int u8truncstr(char *restrict str, size_t n);

void * xrealloc(void *ptr, size_t size);
void * xcalloc(size_t nmemb, size_t size);
void * xnmalloc(size_t nmemb, size_t size);

int xchmod(const char *file, mode_t mode);

int xgetchar(void);
