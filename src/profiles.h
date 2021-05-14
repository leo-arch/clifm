#pragma once

int get_profile_names(void);
int profile_function(char **comm);
int profile_add(const char *prof);
int profile_del(const char *prof);
int profile_set(const char *prof);

