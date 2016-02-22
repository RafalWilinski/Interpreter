#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "list.h"
#include "main.h"

const char *builtin_cmds[] = { "cd", "echo", "exit", "help", "jobs" };

bool iterate_bck(void *data) {
    BackgroundJob* job = *(BackgroundJob **) data;
    printf("[%d]: %s\n", job->job_pid, job->command);
    return TRUE;
}

int _cmds_count() {
    return sizeof(builtin_cmds) / sizeof(builtin_cmds[0]);
}

int __echo(char ** string) {
    printf("%s\n", string[0]);
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

int __jobs(char ** args) {
    generic_list bck_jobs = get_bck_jobs();
    if(bck_jobs.logicalLength != 0) {
        printf("Background jobs: \n");
        list_for_each(&bck_jobs, &iterate_bck);
    }
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
  &__help,
  &__jobs
};

