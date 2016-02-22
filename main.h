#ifndef SHELL_MAIN_H
#define SHELL_MAIN_H

typedef struct _background_job_struct {
    pid_t job_pid;
    char* command;
} BackgroundJob;

typedef struct _input_redirect {
    int TYPE; // 0 - input, 1 - output, 2 - append
    char* file;
} InputRedirect;

typedef struct _cmd_struct {
    char* command;
    bool is_background;
    generic_list arguments;
    generic_list redirect;
    struct _cmd_struct* next;
} Command;

typedef struct _kvp {
    char* key;
    char* value;
} EnvVariable;

generic_list get_bck_jobs();

#endif //SHELL_MAIN_H
