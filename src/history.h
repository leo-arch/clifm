#pragma once

int save_dirhist(void);
void add_to_cmdhist(const char *cmd);
int record_cmd(char *input);
int get_history(void);
int history_function(char **comm);
int run_history_cmd(const char *cmd);
void add_to_dirhist(const char *dir_path);
int log_function(char **comm);
void log_msg(char *_msg, int print);

