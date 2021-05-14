#pragma once

int regen_config(void);
int edit_function (char **comm);
void define_config_file_names(void);
int create_config(const char *file);
void create_config_files(void);
void create_def_cscheme(void);
int create_kbinds_file(void);
void init_config(void);
void read_config(void);
int create_bm_file(void);
int reload_config(void);
void copy_plugins(void);
void edit_xresources(void);
void create_tmp_files(void);
void set_sel_file(void);
void set_env(void);
