#pragma once

/* listing.c */
void free_dirlist(void);
int list_dir(void);
int list_dir_light(void);
void get_dir_icon(const char *dir, int n);
void get_ext_icon(const char *restrict ext, int n);
void get_file_icon(const char *file, int n);
void print_disk_usage(void);
void print_div_line(void);
void print_sort_method(void);
void print_dirhist_map(void);

