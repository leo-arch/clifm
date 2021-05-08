#pragma once

/* checks.c */
void file_cmd_check(void);
void check_file_size(char *log_file, int max);
char **check_for_alias(char **args);
int check_regex(char *str);
int check_immutable_bit(char *file);
int is_bin_cmd(const char *str);
int digit_found(const char *str);
int is_internal(const char *cmd);
int is_internal_c(const char *restrict cmd);
int is_number(const char *restrict str);
int is_acl(char *file);

