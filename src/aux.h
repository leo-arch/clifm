#pragma once

// some memory wrapper functions
void *xrealloc(void *ptr, size_t size);
void *xcalloc(size_t nmemb, size_t size);
void *xnmalloc(size_t nmemb, size_t size);

char xgetchar(void);

char from_hex(char c);
char to_hex(char c);
int hex2int(char *str);
int *get_hex_num(const char *str);

char *url_encode(char *str);
char *url_decode(char *str);

int read_octal(char *str);
int get_link_ref(const char *link);
off_t dir_size(char *dir);

char *get_size_unit(off_t size);
char *get_cmd_path(const char *cmd);

int count_dir(const char *dir_path);
char *xitoa(int n);

