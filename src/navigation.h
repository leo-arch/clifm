#pragma once

int back_function(char **comm);
int forth_function(char **comm);
int xchdir(const char *dir, const int set_title);
int cd_function(char *new_path);
int surf_hist(char **comm);
char *fastback(const char *str);
int workspaces(char *str);

