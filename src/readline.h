#pragma once

int initialize_readline(void);
char *my_rl_path_completion(const char *text, int state);
char **my_rl_completion(const char *text, int start, int end);
char *my_rl_quote(char *text, int mt, char *qp);
char *rl_no_hist(const char *prompt);
int quote_detector(char *line, int index);
char *bin_cmd_generator(const char *text, int state);
char *cschemes_generator(const char *text, int state);
char *filenames_gen_eln(const char *text, int state);
char *filenames_gen_text(const char *text, int state);
char *profiles_generator(const char *text, int state);
char *hist_generator(const char *text, int state);
char *jump_generator(const char *text, int state);
char *jump_entries_generator(const char *text, int state);
char *bookmarks_generator(const char *text, int state);
int is_quote_char(const char c);


