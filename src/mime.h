#pragma once

/* mime.c */
int mime_open(char **args);
int mime_import(char *file);
int mime_edit(char **args);
char *get_app(const char *mime, const char *ext);
char *get_mime(char *file);

