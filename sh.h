#include "get_path.h"

int pid;
int changed_path;   // "Boolean" for if the PATH env variable was changed
extern char **environ; // List of env variables
int sh(int argc, char **argv, char **envp);
char *which(char *command, struct pathelement *pathlist);
char *where(char *command, struct pathelement *pathlist);
void list(char *dir);
void print_pwd();
void cd_c(char **args, char *prev_dir);
void printenv(char **envp);
void setenv_c(char **args, struct pathelement **pathlist);
void update_path_list(char *new_path, struct pathelement **pathlist);
char **expand_wildcards(char **args);
void free_path(struct pathelement *pathlist);

char *sub_env_vars(char *arg, char **argv, int last_exit_code);

#define PROMPTMAX 32
#define MAXARGS 10