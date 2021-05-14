#pragma once

#include <time.h>

/* trash.c */
int trash_clear(void);
int trash_function (char **comm);
int trash_element(const char *suffix, struct tm *tm, char *file);
int untrash_element(char *file);
int untrash_function(char **comm);
int remove_from_trash(void);
int wx_parent_check(char *file);
int recur_perm_check(const char *dirname);

