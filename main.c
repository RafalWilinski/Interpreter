#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <string.h>
#include <unistd.h>

#include "list.h"
#include "builtin.h"

bool batch_mode = FALSE;
generic_list background_jobs;
generic_list env_variables;

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

/*
 * Helper functions (signal handling, freeing memory, printing, iterating
 */
void SIGINT_handler(int signal_number) {
    printf("Signal: %d\n", signal_number);
}

void free_string(void *data) {
    free(*(char **)data);
}

void free_input_redirect(void *data) {
    free(*(struct _input_redirect **)data);
}

void free_bck_job(void *data) {
    free(*(struct _background_job_struct **)data);
}

void free_env_var(void *data) {
    free(*(struct _kvp **)data);
}

void print_input_redirect(InputRedirect* redirect) {
    printf("Type: %d, file: %s\n", redirect->TYPE, redirect->file);
}

void print_args(char** args) {
    int _iter = 0;
    while(args[_iter]) {
        printf("[%d]: %s\n", _iter, args[_iter]);
        _iter++;
    }
}

int num_or_args(char** args) {
    int _iter = 0;
    while(args[_iter]) {
        _iter++;
    }
    return _iter;
}

bool iterate_string(void *data) {
    printf("%s ", *(char **)data);
    return TRUE;
}

bool iterate_redirect(void *data) {
    print_input_redirect(*(InputRedirect **)data);
    return TRUE;
}

char** list_to_args(generic_list string_list) {
    int _list_size = list_size(&string_list), _iter = 0;
    char** _args = (char**) calloc((size_t) _list_size, sizeof(char*));

    listNode* elem = string_list.head;
    while(elem != NULL) {
        _args[_iter] = *(char **) elem->data;
        elem = elem->next;
        _iter++;
    }
    return _args;
}

void print_command(Command* cmd) {
    printf("\nExec: %s\nArguments (%d): ", cmd->command, list_size(&cmd->arguments));
    list_for_each(&cmd->arguments, iterate_string);
    printf("\nRedirects (%d): \n", list_size(&cmd->redirect));
    list_for_each(&cmd->redirect, iterate_redirect);
    printf("Is background? %d\n", cmd->is_background);
    printf("-----------------\n");

    if(cmd->next) print_command(cmd->next);
}

size_t get_words_count(char *line) {
    size_t _iter = 0, _cnt = 0;
    while(line[_iter]) {
        if (line[_iter] == ' ') _cnt++;
        _iter++;
    }
    return _iter;
}

char** isolate_words_from_stream(char *line) {
    int _iter = 0;
    char * _part;
    char ** _words = (char**) calloc(get_words_count(line), sizeof(char*));
    _part = strtok (line, " ");

    while (_part != NULL) {
        _words[_iter] = _part;
        _part = strtok (NULL, " ");
        _iter++;
    }
    return _words;
}

/*
 * Background jobs
 */

void add_background_job(char* name, pid_t pid) {
    BackgroundJob* bck_job = (BackgroundJob*) malloc(sizeof(BackgroundJob));
    bck_job->command = name;
    bck_job->job_pid = pid;
}


/*
 * Environment Variables
 */

bool iterate_env_var(void *data) {
    EnvVariable* _cur = *(EnvVariable **) data;
}

void change_or_set_env_var(char* key, char* value) {
    bool _found = FALSE;
    EnvVariable* _cur;
    listNode* elem = env_variables.head;
    while(elem != NULL) {
        _cur = *(EnvVariable **) (elem->data);

        if(strcmp(_cur->key, key) == 0) {
            _found = TRUE;
            printf("Key found: %s : %s\n", _cur->key, _cur->value);
            _cur->value = value;
        }
        elem = elem->next;
    }

    if(!_found) {
        printf("Value absent, adding...\n");
        _cur = (EnvVariable*) malloc(sizeof(EnvVariable));
        _cur->key = key;
        _cur->value = value;
        list_append(&env_variables, &_cur);
    }
}

void print_env_vars() {
    EnvVariable* _cur;
    printf("%d\n",list_size(&env_variables));
    list_head(&env_variables, &_cur, FALSE);
    listNode* elem = env_variables.head;
    while(elem != NULL) {
        _cur = (*(EnvVariable **) elem->data);
        printf("%s:%s\n", _cur->key, _cur->value);
        elem = elem->next;
    }
}

char* get_env_var(char* key) {
    EnvVariable* _cur;
    listNode* elem = env_variables.head;
    while(elem != NULL) {
        _cur = *(EnvVariable **) (elem->data);
//        printf("K[%s] = %s vs %s", _cur->key, _cur->value, key);
        if(strcmp(_cur->key, key) == 0) {
            return _cur->value;
        }
        elem = elem->next;
    }
    return NULL;
}

/*
 * Core functions (parsing, executing)
 */
Command* assemble_commands(char ** _parts) {
    int _iter = 0, last_end = 0;
    bool _input_redirect_file = FALSE;

    Command* _current_command;
    Command* _cmds_list_head = (Command*) malloc(sizeof(Command));
    InputRedirect* _inputRedirect = (InputRedirect*) malloc(sizeof(InputRedirect));

    _current_command = _cmds_list_head;
    _current_command->next = NULL;
    list_new(&_current_command->arguments, sizeof(char*), free_string);
    list_new(&_current_command->redirect, sizeof(InputRedirect*), free_input_redirect);

    while (_parts[_iter]) {

        // Check if it's the beginning of the redirect object
        if (_parts[_iter][0] == '<') {
            _input_redirect_file = TRUE;
            _inputRedirect->TYPE = 0;
        } else if (_parts[_iter][0] == '>') {
            _input_redirect_file = TRUE;
            if(_parts[_iter][1] && _parts[_iter][1] == '>') _inputRedirect->TYPE = 2;
            _inputRedirect->TYPE = 1;
        }

        // If predecessor was redirect, second part must be a file
        else if(_input_redirect_file) {
            char *tmp = strdup(_parts[_iter]);

            _input_redirect_file = FALSE;
            _inputRedirect->file = tmp;
            list_append(&_current_command->redirect, &_inputRedirect);

            _inputRedirect = (InputRedirect*) malloc(sizeof(InputRedirect));
            last_end = _iter + 1;
        }

        // Stream - it's end of current command, create new one and assign pointer
        else if(_parts[_iter][0] == '|') {
            Command* _new_command = (Command*) malloc(sizeof(Command));
            _current_command->next = _new_command;
            _current_command = _new_command;

            list_new(&_current_command->arguments, sizeof(char*), free_string);
            list_new(&_current_command->redirect, sizeof(InputRedirect*), free_input_redirect);
            last_end = _iter + 1;
        }

        // &amp - announces it's background command
        else if(_parts[_iter][0] == '&') {
            _current_command->is_background = TRUE;
        }

        // Base command
        else if(_iter == last_end) {
            _current_command->command = strdup(_parts[_iter]);
        }

        // Else it's just command supplier
        else {
            char* tmp = strdup(_parts[_iter]);
            list_append(&_current_command->arguments, &tmp);
        }
        _iter++;
    }

    print_command(_cmds_list_head);
    return _cmds_list_head;
}

void execute_command(Command* cmd) {
    int result = 0, builtin_num = -1, chld_status = 0;
    pid_t pid;


    // Check if entered command isn't already implemented
    builtin_num = is_builtin(cmd->command);
    if (builtin_num > -1) {
        chld_status = (*builtin_func[builtin_num])(list_to_args(cmd->arguments));
    } else {
        // Check if it's env variable related command
        if(cmd->command[0] == '$' && cmd->command[1] != NULL) {
            char* key = cmd->command;
            key++;
            char* value = get_env_var(key);
            if(value != NULL) {
                printf("%s\n", value);
            } else {
                printf("(null)\n");
            }
        }

        // If it's not, just do your things
        else {
            printf("Execvp... ");
            pid = fork();
            if (pid == 0) {
                char **args = list_to_args(cmd->arguments);

                if (num_or_args(args) == 0) {
                    args[0] = strdup(cmd->command);
                }
                printf("Child process pid %d\n", getpid());
                chld_status = execvp(cmd->command, args);
            } else {
                waitpid(pid, &chld_status, 0);
                printf("Main process pid %d\n", getpid());
            }
        }
    }

    printf("Status: %d\n", chld_status>>8);
    free(cmd);
}

int main(int argc, char** argv) {
    Command *cmd;
    list_new(&background_jobs, sizeof(BackgroundJob*), free_bck_job);
    list_new(&env_variables, sizeof(EnvVariable*), free_env_var);
    change_or_set_env_var("?", "0");

    if(argc >= 2) {
        int i = 0;
        char** arguments = (char**) calloc(sizeof(char*), (size_t) argc);
        batch_mode = TRUE;

        for(i = 0; i < argc - 1; i++) {
            arguments[i] = argv[i + 1];
        }
        arguments[argc] = '\0';
        cmd = assemble_commands(arguments);

        execute_command(cmd);

    } else {
        char* line;
        signal(SIGINT, SIGINT_handler);

        while((line = readline("sh $ ")) != NULL) {
            if(*line) {
                add_history(line);
                cmd = assemble_commands(isolate_words_from_stream(line));
                free(line);

                execute_command(cmd);
            }
        }
        printf("\nInterpreter closed.\n");
        exit(0);
    }
}