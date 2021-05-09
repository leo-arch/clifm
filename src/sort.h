#pragma once

#include <dirent.h>

/* sort.c */
int alphasort_insensitive(const struct dirent **a, const struct dirent **b);
int xalphasort(const struct dirent **a, const struct dirent **b);
int sort_function(char **arg);
int entrycmp(const void *a, const void *b);
int namecmp(const char* s1, const char* s2);
int skip_nonexec(const struct dirent *ent);
int skip_files(const struct dirent *ent);

