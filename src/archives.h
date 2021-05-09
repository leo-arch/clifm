#pragma once

int is_compressed(char *file, int test_iso);
int archiver(char **args, char mode);
int zstandard(char *in_file, char *out_file, char mode, char op);
int check_iso(char *file);
int create_iso(char *in_file, char *out_file);
int handle_iso(char *file);

