#pragma once

/* bookmarks.c */
char **bm_prompt(void);
int bookmark_add(char *file);
int bookmark_del(char *name);
int bookmarks_function(char **cmd);
int edit_bookmarks(char *cmd);
int open_bookmark(void);
void free_bookmarks(void);

