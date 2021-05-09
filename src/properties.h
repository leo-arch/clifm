#pragma once

/* properties.c */
int get_properties(char *filename, int dsize);
int properties_function(char **comm);
int print_entry_props(struct fileinfo *props, size_t max);

