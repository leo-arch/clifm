#pragma once

int exec_cmd(char **comm);
void exec_chained_cmds(char *cmd);
void exec_profile(void);
int run_in_foreground(pid_t pid);
void run_in_background(pid_t pid);
int launch_execve(char **cmd, int bg, int xflags);
int launch_execle(const char *cmd);
int run_and_refresh(char **comm);

