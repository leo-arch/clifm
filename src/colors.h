#pragma once

int set_colors(const char *colorscheme, int env);
void color_codes (void);
size_t get_colorschemes(void);
int cschemes_function(char **args);
char *strip_color_line(const char *str, char mode);
void free_colors(void);
int is_color_code(const char *str);
void colors_list(const char *ent, const int i, const int pad, const int new_line);
char *get_ext_color(const char *ext);

