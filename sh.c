#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <pwd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <glob.h>
#include <ctype.h>
#include "sh.h"


int sh(int argc, char **argv, char **envp) {
  char *prompt = calloc(PROMPTMAX, sizeof(char));
  char *commandline = calloc(MAX_CANON, sizeof(char));
  char *command, *arg, *commandpath, *p, *pwd, *owd;
  char **args = calloc(MAXARGS, sizeof(char*));
  int uid, i, status, argsct, go = 1;
  struct passwd *password_entry;
  char *homedir;
  struct pathelement *pathlist;
  char cwd[PATH_MAX];
  char *path;
  int last_exit_code = 0;   // Last exit code before exiting
  char prev_dir[PATH_MAX];  // Previous directory

  changed_path = 0;

  getcwd(prev_dir, sizeof(prev_dir));

  FILE *input = stdin;  // Default input source is stdin

  // Error out if too many args are given
  if (argc > 2) {
    fprintf(stderr, "Too many arguments given.\nUsage: %s OR %s [filename]\n", argv[0], argv[0]);
    exit(1);
  }

  // If a file is provided, open it
  if (argc == 2) {
    input = fopen(argv[1], "r");
    if (!input) {
      perror("fopen");
      exit(1);
    }
  }

  uid = getuid();
  password_entry = getpwuid(uid);               /* get passwd info */
  homedir = password_entry->pw_dir;             /* Home directory to start out with */

  if ((pwd = getcwd(NULL, PATH_MAX+1)) == NULL) {
    perror("getcwd");
    exit(2);
  }
  owd = calloc(strlen(pwd) + 1, sizeof(char));
  memcpy(owd, pwd, strlen(pwd));
  prompt[0] = ' '; prompt[1] = '\0';

  /* Put PATH into a linked list */
  pathlist = get_path(&path);

  // Initialize ACC as an env variable if it doesn't exist
  if (getenv("ACC") == NULL) {
    setenv("ACC", "0", 1);  // Set ACC to 0 as default initial val
  }

  // MAIN SHELL LOOP
  while (go) {
    /* print your prompt if reading from terminal instead of from a file */
    if (input == stdin) {  // Only show prompt when reading from stdin/terminal
      if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd");
        continue;
      }
      printf("%s [%s]> ", prompt, cwd);
      fflush(stdout);
    }

    // Read command line input
    if (fgets(commandline, MAX_CANON, input) == NULL) {
        if (input == stdin) {
            // Ignore Ctrl-D (EOF)
            printf("\n");
            clearerr(input);
            continue;
        } else {
            break;  // Exit on EOF if reading from a file/script
        }
    }

    commandline[strcspn(commandline, "\n")] = 0;  // Remove newline from input

    // Tokenize input: assign each part/word of input to arg[] at i (ex: 0th arg at arg[0])
    int i = 0;
    command = strtok(commandline, " ");
    while (command != NULL && i < MAXARGS - 1) {
      args[i++] = strdup(sub_env_vars(command, argv, last_exit_code));  // Replace any $environment_vars
      command = strtok(NULL, " ");
    }
    args[i] = NULL;

    // If empty input, print prompt again
    if (args[0] == NULL) {
      continue;
    }

    // If command starts with ?, do conditional execution: only execute the command if $? (last_exit_code) is 0
    if (args[0][0] == '?') {
      // If last_exit_code is not 0, don't execute the command
      if (last_exit_code != 0) {
          continue;
      }
      // Otherwise, remove the ? char so we can run the command as normal
      args[0]++;
    }


    // Built-in command: exit
    if (strcmp(args[0], "exit") == 0) {
      if (!getenv("NOECHO") || getenv("NOECHO")[0] == '\0') {
        printf("Executing built-in \'%s\'\n", args[0]);  // Only print if env var NOECHO exists and is non-empty
      }
      if (args[1] != NULL) {
        char *end;
        last_exit_code = strtol(args[1], &end, 10);
      }

      free(prompt);
      free(commandline);
      free(args);
      free(owd);
      free(pwd);
      free(path);
      free_path(pathlist);

      if (input != stdin) {
        fclose(input);  // Close input file
      }

      printf("EC: %d\n", last_exit_code);
      exit(last_exit_code);
    }

    // Built-in command: which
    else if (strcmp(args[0], "which") == 0) {
      if (!getenv("NOECHO") || getenv("NOECHO")[0] == '\0') {
        printf("Executing built-in \'%s\'\n", args[0]);
      }
      // Print path of all commands entered as args
      for (int i = 1; args[i] != NULL; i++) {
        char *cmd_path = which(args[i], pathlist);
        if (cmd_path) {
          printf("%s\n", cmd_path);
          free(cmd_path);
        } else {
          printf("%s: Command not found\n", args[i]);
        }
      }
      continue;
    }

    // Built-in command: list
    else if (strcmp(args[0], "list") == 0) {
      if (!getenv("NOECHO") || getenv("NOECHO")[0] == '\0') {
        printf("Executing built-in \'%s\'\n", args[0]);
      }

      if (args[1] == NULL) {
        list(".");  // No arguments, list current directory
      } else {
        for (int i = 1; args[i] != NULL; i++) {
          printf("\n%s:\n", args[i]);  // Print directory name
          list(args[i]);  // List files in directory
        }
      }
      continue;
    }

    // Built-in command: pwd
    else if (strcmp(args[0], "pwd") == 0) {
      if (!getenv("NOECHO") || getenv("NOECHO")[0] == '\0') {
        printf("Executing built-in \'%s\'\n", args[0]);
      }
      print_pwd();
      continue;
    }

    // Built-in command: cd
    else if (strcmp(args[0], "cd") == 0) {
      if (args[1] != NULL && args[2] != NULL) {   // If > one arg is given to cd, print error message
        printf("Too many arguments\n");
        continue;
      } else if (args[1] != NULL) {   // If there's only one arg to cd, print it
        if (!getenv("NOECHO") || getenv("NOECHO")[0] == '\0') {
          printf("Executing built-in \'%s %s\'\n", args[0], args[1]);
        }
      } else {    // Else just print cd
        if (!getenv("NOECHO") || getenv("NOECHO")[0] == '\0') {
          printf("Executing built-in \'%s\'\n", args[0]);
        }
      }
      cd_c(args, prev_dir);  // Call my cd function (so we can go to correct HOME even if it's changed)
      continue;
    }

    // Built-in command: pid
    else if (strcmp(args[0], "pid") == 0) {
      if (!getenv("NOECHO") || getenv("NOECHO")[0] == '\0') {
        printf("Executing built-in \'%s\'\n", args[0]);
      }

      pid_t pid = getpid();  // Get the current process's ID
      printf("%d\n", pid);

      continue;
    }

    // Built-in command: prompt
    else if (strcmp(args[0], "prompt") == 0) {
      if (argc > 1) {
        printf("%s: Command not found\n", args[0]);
        continue;
      }
      if (!getenv("NOECHO") || getenv("NOECHO")[0] == '\0') {
        printf("Executing built-in \'%s\'\n", args[0]);
      }

      if (args[1] == NULL) {  // If no arg given, ask user again
        char new_prompt[PROMPTMAX];
        printf("  input prompt prefix: ");
        
        // Make sure to read no more than PROMPTMAX - 1 characters
        if (fgets(new_prompt, PROMPTMAX, stdin) != NULL) {
          new_prompt[strcspn(new_prompt, "\n")] = '\0';
          snprintf(prompt, PROMPTMAX, "%s", new_prompt);  // Change the prompt
        }
      } else {  // If arg given, update the prompt directly
        snprintf(prompt, PROMPTMAX, "%s", args[1]);
      }

      continue;
    }

    // Built-in command: printenv
    if (strcmp(args[0], "printenv") == 0) {
      if (!getenv("NOECHO") || getenv("NOECHO")[0] == '\0') {
        printf("Executing built-in '%s'\n", args[0]);
      }
      printenv(args);
      continue;
    }

    // Built-in command: setenv
    if (strcmp(args[0], "setenv") == 0) {
      if (!getenv("NOECHO") || getenv("NOECHO")[0] == '\0') {
        printf("Executing built-in '%s'\n", args[0]);
      }
      setenv_c(args, &pathlist);
      continue;
    }

    // New built-in command: addacc
    if (strcmp(args[0], "addacc") == 0) {
      if (!getenv("NOECHO") || getenv("NOECHO")[0] == '\0') {
        printf("Executing built-in \'%s\'\n", args[0]);
      }

      char *acc_str = getenv("ACC");   // Get current ACC value
      int acc_val = (acc_str && isdigit(acc_str[0])) ? atoi(acc_str) : 0;
  
      int val_to_add = 1;   // Get addacc arg (default to 1 if not given)
      if (args[1] != NULL) {
          char *end;
          val_to_add = strtol(args[1], &end, 10);
          if (*end != '\0') {   // If invalid number, print error
              fprintf(stderr, "addacc: Invalid number '%s'\n", args[1]);
              return 1;
          }
      }
  
      acc_val += val_to_add;   // Add given value to ACC
      char new_acc[20];
      sprintf(new_acc, "%d", acc_val);   // Convert acc_val into a str and put it in new_acc
      setenv("ACC", new_acc, 1);
      continue;
    }


    // If input is an External command, execute it
    int is_absolute = (args[0][0] == '/');
    int is_relative = (strchr(args[0], '/') != NULL);
    
    pid_t pid = fork();
    if (pid < 0) {
      perror("fork");
      continue;
    }
    else if (pid == 0) {  // Child process
      // If given full or relative path to executable for an external command, run it
      if (is_absolute || is_relative) {
        // Try to execute directly
        if (!getenv("NOECHO") || getenv("NOECHO")[0] == '\0') {
          printf("Executing %s\n", args[0]);
        }
        // execve(args[0], args, envp);
        char **expanded_args = expand_wildcards(args);
        execve(expanded_args[0], expanded_args, envp);

        // Free memory for expanded args
        for (int i = 0; expanded_args[i] != NULL; i++) {
            free(expanded_args[i]);
        }
        free(expanded_args);

        perror("execve"); // If execve fails
        exit(127);
      }
      // If not given path and is not a built-in command, search for command in PATH
      else {
        // Find the full path of the command with which()
        commandpath = which(args[0], pathlist);
        if (commandpath) {
          if (!getenv("NOECHO") || getenv("NOECHO")[0] == '\0') {
            printf("Executing %s\n", args[0]);
          }

          char **expanded_args = expand_wildcards(args);
          execve(commandpath, expanded_args, envp);

          // Free memory for expanded args
          for (int i = 0; expanded_args[i] != NULL; i++) {
              free(expanded_args[i]);
          }
          free(expanded_args);
          free(commandpath);
          continue;
        }

        // If command was not found
        fprintf(stderr, "%s: Command not found.\n", args[0]);
        exit(127); 
      }
    }
    else {  // Parent process
      waitpid(pid, &status, 0);
      last_exit_code = WEXITSTATUS(status); // Save last command's exit code
    }
  }

  // This is executed if exit command is not used at end of program use
  free(prompt);
  free(commandline);
  free(args);
  free(owd);
  free(pwd);
  free(path);
  free_path(pathlist);
  if (input != stdin) {
    fclose(input);  // Close input file
  }
  printf("EC: %d\n", last_exit_code);
  return last_exit_code;
}


char *which(char *command, struct pathelement *pathlist) {
   /* loop through pathlist until finding command and return it. Return NULL when not found. */
   while (pathlist) {
      char fullpath[PATH_MAX];
      snprintf(fullpath, sizeof(fullpath), "%s/%s", pathlist->element, command);
      if (access(fullpath, X_OK) == 0) {
        return strdup(fullpath);
      }
      pathlist = pathlist->next;
    }
    return NULL;
}


void list(char *dir) {
  // If arg contains a wildcard char (* or ?), expand it
  if (strchr(dir, '*') || strchr(dir, '?')) {
    // Use glob to expand the pattern
    glob_t glob_result;
    
    if (glob(dir, 0, NULL, &glob_result) != 0) {
      return;
    }

    for (int i = 0; i < glob_result.gl_pathc; i++) {
      printf("%s\n", glob_result.gl_pathv[i]);
    }

    globfree(&glob_result);
  } else {
    // Print out filenames for the directory passed using opendir() and readdir()
    DIR *dp;
    struct dirent *entry;

    dp = opendir(dir);
    if (dp == NULL) {
      perror("opendir");
      return;
    }

    while ((entry = readdir(dp)) != NULL) {
      printf("%s\n", entry->d_name);
    }

    closedir(dp);
  }
}


void print_pwd() {
    char cwd[PATH_MAX];

    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
    } else {
        perror("getcwd");
    }
}


void cd_c(char **args, char *prev_dir) {
    char new_dir[PATH_MAX];  // Buffer for new directory
    char temp_dir[PATH_MAX]; // Buffer for current directory

    if (getcwd(temp_dir, sizeof(temp_dir)) == NULL) {
        perror("getcwd");
        return;
    }

    if (args[1] == NULL) {  
        // If no argument, go to HOME directory
        char *home = getenv("HOME");
        if (home == NULL) {
            fprintf(stderr, "cd: HOME not set\n");
            return;
        }
        snprintf(new_dir, sizeof(new_dir), "%s", home);
    } else if (strcmp(args[1], "-") == 0) {  
        // If '-', go to previous directory
        snprintf(new_dir, sizeof(new_dir), "%s", prev_dir);
    } else {  
        // Otherwise, go to specified directory
        snprintf(new_dir, sizeof(new_dir), "%s", args[1]);
    }

    // Change directory
    if (chdir(new_dir) == 0) {
        snprintf(prev_dir, PATH_MAX, "%s", temp_dir); // Update prev_dir
    } else {
        perror("chdir");
    }
}


void printenv(char **envp) {
  if (envp[1] == NULL) {
    // If no args given, print all env variables
    for (char **env = environ; *env != NULL; env++) {
        printf("%s\n", *env);
    }
  } else {
    // If args are given, print each variable value
    for (int i = 1; envp[i] != NULL; i++) {
      char *value = getenv(envp[i]);
      if (value != NULL) {
        // If the variable exists, print its value
        printf("%s=%s\n", envp[i], value);
      } else {
        // Else, print a message
        printf("No variable: %s\n", envp[i]);
      }
    }
  }
}


void setenv_c(char **args, struct pathelement **pathlist) {
  if (args[1] == NULL) {
    // If no args, just print all env variables (same as printenv)
    for (char **env = environ; *env != NULL; env++) {
      printf("%s\n", *env);
    }
  } else if (args[2] == NULL) {
    // If 1 arg, set it as an empty env variable
    if (setenv(args[1], "", 1) != 0) {
        perror("setenv");
    }
  } else if (args[3] == NULL) {
    // If 2 args, set the env variable with the given value
    if (setenv(args[1], args[2], 1) != 0) {
      perror("setenv");
    }

    // If changing PATH variable
    if (strcmp(args[1], "PATH") == 0) {
      // Update the PATH environment variable and linked list of directories
      update_path_list(args[2], pathlist);
      if (setenv(args[1], args[2], 1) != 0) {
        perror("setenv");
      }
    }
  } else {
    // If more than 2 arguments provided, print an error
    fprintf(stderr, "Error: Too many arguments for setenv.\nUsage: setenv [VAR] [VALUE]\n");
  }
}


// Update PATH linked list from a new PATH string
void update_path_list(char *new_path, struct pathelement **pathlist) {
    struct pathelement *temp, *head = NULL;

    // Free old pathlist
    while (*pathlist) {
        temp = *pathlist;
        *pathlist = (*pathlist)->next;
        if (changed_path == 1) {
          free(temp->element);
        }
        free(temp);
    }
    
    char *token = strtok(new_path, ":");

    // Parse the new PATH string and update the pathlist
    while (token != NULL) {
        temp = malloc(sizeof(struct pathelement));
        temp->element = strdup(token);
	      temp->next = head;
        head = temp;
        token = strtok(NULL, ":");
    }

    *pathlist = head;
    changed_path = 1;
}


char **expand_wildcards(char **args) {
    glob_t wildcard;
    char **expanded_args = NULL;  // Dynamic List of expanded args (files) that match the wilcard pattern
    int num_expanded = 0;
    
    // Go through all args and expand them if possible
    for (int i = 0; args[i] != NULL; i++) {
        // Check if there is a '*' or '?' wildcard in the arg
        if (strchr(args[i], '*') || strchr(args[i], '?')) {
            // Expand the wildcard(s) by finding files in the curr dir that match the pattern
            if (glob(args[i], GLOB_NOCHECK, NULL, &wildcard) == 0) {
                // Go through all expanded results and put them into expanded_args list
                for (int j = 0; j < wildcard.gl_pathc; j++) {
                  // Resize the expanded_args list to be able to fit the new match in it
                    expanded_args = realloc(expanded_args, (num_expanded + 1) * sizeof(char *));
                    expanded_args[num_expanded] = strdup(wildcard.gl_pathv[j]);
                    num_expanded++;
                }
            }

            globfree(&wildcard);
        } 
        // If the arg(s) don't have a wildcard, keep it as is and copy it over to expanded_args var
        else {
            expanded_args = realloc(expanded_args, (num_expanded + 1) * sizeof(char *));
            expanded_args[num_expanded++] = strdup(args[i]); // Keep original arg if no wildcard
        }
    }

    expanded_args = realloc(expanded_args, (num_expanded + 1) * sizeof(char *));  // Expand it to be able to add a null-terminating char at the env
    expanded_args[num_expanded] = NULL;
    return expanded_args;
}


void free_path(struct pathelement *pathlist) {
    struct pathelement *temp;
    while (pathlist) {
        temp = pathlist;
        pathlist = pathlist->next;
        if (changed_path == 1) {
          free(temp->element);
        }
	      free(temp);
    }
    pathlist = NULL;
}


char *sub_env_vars(char *arg, char **argv, int last_exit_code) {
  if (!arg || arg[0] != '$') {
      return arg;  // No substitution needed cuz there's no $
  }

  // If arg given is $0, replace with argv[0]
  if (strcmp(arg, "$0") == 0) {
      return argv[0];
  }

  // If arg given is $?, replace with exit code of most recently executed command
  if (strcmp(arg, "$?") == 0) { 
    char *exit_code = malloc(12);
    sprintf(exit_code, "%d", last_exit_code);
    return exit_code;  // Return exit code
}

  char *env_var_name = arg + 1;  // Go past $ and get the var name
  char *env_value = getenv(env_var_name);
  return env_value ? env_value : "";  // Return value or empty string if not found
}

