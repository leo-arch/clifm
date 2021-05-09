#pragma once

/* file_operations.c */
int batch_link(char **args);
char *export(char **filenames, int open);
int bulk_rename(char **args);
int remove_file(char **args);
int copy_function(char **comm);
int edit_link(char *link);
int open_function(char **cmd);
int xchmod(const char *file, mode_t mode);

