#pragma once

/* init.c */
void check_env_filter(void);
void get_prompt_cmds(void);
void get_aliases(void);
int get_last_path(void);
void check_options(void);
int load_dirhist(void);
size_t get_path_env(void);
void get_prompt_cmds(void);
int get_sel_files(void);
void init_shell(void);
void load_jumpdb(void);
int load_bookmarks(void);
int load_actions(void);
int load_pinned_dir(void);
void get_path_programs(void);
void unset_xargs(void);
void external_arguments(int argc, char **argv);
char *get_date (void);
pid_t get_own_pid(void);
struct user_t get_user(void);

// stores information regarding the user
struct user_t {
	char * home;
	size_t home_len;
	char * name;
	char * shell;
};
