#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

const char *builtin_cmds[] = { "cd", "echo", "exit", "help", "jobs" };

int _cmds_count() {
    return sizeof(builtin_cmds) / sizeof(builtin_cmds[0]);
}

int __echo(char ** string) {
    printf("%s", string[0]);
    return 1;
}

int __cd(char ** args) {
    if (args[0] == NULL) {
        printf("Podaj lokalizacje!\n");
        return 0;
    } else {
        if (chdir(args[0]) != 0) {
            perror("cd error");
            return 0;
        }
    }
    return 1;
}

int __exit(char ** args) {
    exit(0);
}

int __help(char ** args) {
    printf("Rozpoznawane operatory: <, >, |, &, >>");
    return 1;
}

int is_builtin(char* cmd_name) {
    for (int i = 0; i < _cmds_count(); ++i) {
        if (strcmp(cmd_name, builtin_cmds[i]) == 0) {
            return i;
        }
    }
    return -1;
}

int (*builtin_func[]) (char**) = {
  &__cd,
  &__echo,
  &__exit,
  &__help
};

