#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <string.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/fcntl.h>

#include "list.h"
#include "builtin.h"
#include "main.h"

generic_list background_jobs;
generic_list env_variables;

/*
 * Helper functions (signal handling, freeing memory, printing, iterating
 */
void SIGINT_handler(int signal_number) {
    printf("\n[%d] Signal: %d\n", getpid(), signal_number);
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

generic_list get_bck_jobs() {
    return background_jobs;
}

int chain_length(Command* cmd) {
    int _iter = 0;
    Command* tmp = cmd;
    while(tmp) {
        _iter++;
        tmp = tmp->next;
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

void redir(generic_list redirects) {
    FILE* fd;
    int i = 0;
    listNode* _tmp = redirects.head;

    while(_tmp) {
        InputRedirect* _redir = *(InputRedirect **) _tmp->data;
        char* mode = (_redir->TYPE == 0) ? "r" : ((_redir->TYPE == 1) ? "w+" : "a");
        print_input_redirect(_redir);
        printf("Opening file (%s): %s\n", mode, _redir->file);
        fd = fopen(_redir->file, mode);
        if (fd == NULL) {
            perror("Error");
        } else {
            printf("File opened!\n");
            if (_redir->TYPE  > 0) dup2(fileno(fd), 1);
            else dup2(fileno(fd), STDIN_FILENO);
            fclose(fd);
        }
        _tmp = _tmp->next;
        i++;
    }
}

char** list_to_args(generic_list string_list) {
    int _list_size = list_size(&string_list), _iter = 0;
    char** _args = (char**) calloc((size_t) _list_size + 1, sizeof(char*));

    listNode* elem = string_list.head;
    while(elem != NULL) {
        _args[_iter] = *(char **) elem->data;
        elem = elem->next;
        _iter++;
    }
    _args[_list_size] = 0;
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
    list_append(&background_jobs, &bck_job);
}


/*
 * Environment Variables
 */

void change_or_set_env_var(char* key, char* value) {
    bool _found = FALSE;
    EnvVariable* _cur;
    listNode* elem = env_variables.head;
    while(elem != NULL) {
        _cur = *(EnvVariable **) (elem->data);

        if(strcmp(_cur->key, key) == 0) {
            _found = TRUE;
            _cur->value = strdup(value);
        }
        elem = elem->next;
    }

    if(!_found) {
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
        if(strcmp(_cur->key, key) == 0) {
            return strdup(_cur->value);
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
            else _inputRedirect->TYPE = 1;
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

    return _cmds_list_head;
}

int spawn_proc (int in, int out, Command* cmd) {
    pid_t pid;
    int status = 0;

    list_prepend(&cmd->arguments, &cmd->command);

    if ((pid = fork ()) == 0) {
        if (in != 0) {
            dup2 (in, 0);
            close (in);
        }
        if (out != 1) {
            dup2 (out, 1);
            close (out);
        }

        redir(cmd->redirect);

        execvp (cmd->command, (char * const *) list_to_args(cmd->arguments));
        exit(errno);
    }
    waitpid(pid, &status, WCONTINUED);
    return pid;
}

int fork_pipes (Command *cmd) {
    int i, n = chain_length(cmd), in = 0, fd [2];
    pid_t last_pid;

    Command* tmp = cmd;
    for (i = 0; i < n - 1; i++) {
        pipe (fd);
        /* f [1] is the write end of the pipe, we carry `in` from the prev iteration.  */
        spawn_proc (in, fd [1], tmp);
        /* No need for the write end of the pipe, the child will write here.  */
        close (fd [1]);
        /* Keep the read end of the pipe, the next child will read from there.  */
        in = fd [0];
        tmp = tmp->next;
    }

    int status = 0;
    list_prepend(&tmp->arguments, &tmp->command);
    if((last_pid = fork()) == 0) {
        if (in != 0 && n > 1) {
            printf("Reading from previous...");
            dup2 (in, 0);
        } // Read from previous job
        redir(tmp->redirect);

        execvp (tmp->command, list_to_args(tmp->arguments));
        exit(errno);
    }
    else {
        int _option = tmp->is_background ? WNOHANG : WCONTINUED;
        if(tmp->is_background) add_background_job(tmp->command, last_pid);

        waitpid(last_pid, &status, _option);
        free(cmd);
        return status;
    }
}


void execute_command(Command* cmd) {
    int builtin_num = -1, chld_status = 0;
    char str[15];

    // Check if entered command isn't already implemented
    builtin_num = is_builtin(cmd->command);
    if (builtin_num > -1) {
        chld_status = (*builtin_func[builtin_num]) (list_to_args(cmd->arguments));
    } else {
        // Check if it's get env variable related command
        if(cmd->command[0] == '$' && cmd->command[1]) {
            char* key = cmd->command;
            key++;
            char* value = get_env_var(key);
            if(value != NULL) {
                printf("%s\n", value);
            } else {
                printf("(null)\n");
            }
        }

        // If command has '=' sign, we're assuming it's env variable change
        else if(strchr(cmd->command, '=') != NULL) {
            const char breaking[2] = "=";
            char* key = strtok(cmd->command, breaking);
            char* value = strtok(cmd->command, breaking);
            change_or_set_env_var(key , value);
        }

        // Else we assume it's command to be execvp'ed
        else {
            chld_status = fork_pipes(cmd);
        }
    }

    chld_status = chld_status >> 8;
    printf("S: %d\n", chld_status);

    sprintf(str, "%d", chld_status);
    change_or_set_env_var("?", str);
}

int main(int argc, char** argv) {
    Command *cmd;
    list_new(&background_jobs, sizeof(BackgroundJob*), free_bck_job);
    list_new(&env_variables, sizeof(EnvVariable*), free_env_var);
    change_or_set_env_var("?", "0");

    if(argc >= 2) {
        int i = 0;
        char** arguments = (char**) calloc(sizeof(char*), (size_t) argc);

        for(i = 0; i < argc - 1; i++) {
            arguments[i] = argv[i + 1];
        }
        arguments[argc] = 0;
        cmd = assemble_commands(arguments);

        execute_command(cmd);

    } else {
        char* line;
        signal(SIGINT, SIGINT_handler);
        signal(SIGTSTP, SIGINT_handler);
        signal(SIGCONT, SIGINT_handler);

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