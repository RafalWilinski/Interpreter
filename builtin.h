#ifndef SHELL_BUILTIN_H
#define SHELL_BUILTIN_H

int __cd(char** args);
int __echo(char** args);
int __exit(char** args);
int __help(char** args);
int is_builtin(char* cmd_name);
int (*builtin_func[]) (char **);
char* builtin_cmds[];

#endif //SHELL_BUILTIN_H