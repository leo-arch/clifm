#pragma once

/* selection.c */
int deselect(char **comm);
int sel_function(char **args);
void show_sel_files(void);
int sel_glob(char *str, const char *sel_path, mode_t filetype);
int sel_regex(char *str, const char *sel_path, mode_t filetype);
int select_file(char *file);
int save_sel(void);

