#pragma once

int _err(int, int, const char *, ...);
int alias_import(char *file);
void bonus_function (void);
int create_usr_var(char *str);
int filter_function(const char *arg);
void free_software (void);
void free_stuff(void);
void handle_stdin(void);
void help_function (void);
int hidden_function(char **comm);
int list_commands (void);
int list_mountpoints(void);
int new_instance(char *dir, int sudo);
char *parse_usrvar_value(const char *str, const char c);
int pin_directory(char *dir);
void print_tips(int all);
void save_last_path(void);
void save_pinned_dir(void);
int set_shell(char *str);
void set_signals_to_ignore(void);
void set_term_title(const char *str);
void splash (void);
int unpin_dir(void);
void version_function (void);

