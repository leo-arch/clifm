#pragma once

char **split_str(const char *str);
char *split_fusedcmd(char *str);
char **parse_input_str(char *str);
char *savestring(const char *restrict str, size_t size);
char **get_substr(char *str, const char ifs);
char *dequote_str(char *text, int mt);
char *escape_str(const char *str);
int *expand_range(char *str, int listdir);
char *home_tilde(const char *new_path);
int strcntchr(const char *str, const char c);
char *straft(char *str, const char c);
char *straftlst(char *str, const char c);
char *strbfr(char *str, const char c);
char *strbfrlst(char *str, const char c);
char *strbtw(char *str, const char a, const char b);
char *gen_rand_str(size_t len);
char *xstrcpy(char *buf, const char *restrict str);
size_t xstrlen(const char *restrict s);
int xstrncmp(const char *s1, const char *s2, size_t n);
char *xstrncpy(char *buf, const char *restrict str, size_t n);
size_t xstrsncpy(char *restrict dst, const char *restrict src, size_t n);
size_t wc_xstrlen(const char *restrict str);
int u8truncstr(char *restrict str, size_t n);
size_t u8_xstrlen(const char *restrict str);
char *remove_quotes(char *str);

