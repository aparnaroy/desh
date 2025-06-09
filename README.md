# Desh - A Simple Unix Shell
This is a simple shell (called "desh"). It has some "built-in" commands, but can also handle external commands. If run with no arguments, it works as an interactive shell, executing commands given by the user. If given a filename argument instead, it executes each line in the file as a script.

## Built-In Commands
- exit
- which
- list
- pwd
- cd
- pid
- prompt
- printenv
- setenv
- addacc

## Additional Features
- Supports wildcard characters `*` and `?`
- Ignores Ctrl-C, Ctrl-Z, SIGTERM, and Ctrl-D
- Supports environment variable substitution using `$ENV_VAR`
- Supports the `$?` shell variable (holds exit status of last executed command)
- Includes an accumulator environment variable, `$ACC`, which can be increased using the `addacc` command, which adds its argument to `$ACC`
- Includes conditional execution: if 1st character on command line is `?`, then the command will only be executed if the `$?` shell variable is 0
- If environment variable `NOECHO` exists and is non-empty, will not indicate command about to be executed, instead will just do it

## How to Run
1. Run `make`
2. Run `./desh` OR `./desh input-file.txt`
3. Enter in any "built-in" or external commands
