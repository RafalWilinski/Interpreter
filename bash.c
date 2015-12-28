#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <string.h>
#include <stdlib.h>

typedef int bool;
#define true 1
#define false 0

char **args;
bool batch_mode = false;

// @TODOS
// -Override ctrl+c signal to stop child task - executor
// -Add input from file 

int get_line_length(char *line) {
	int i = 0;
	while(line[i]) {
		i++;
	}
	return i;
}

char** isolate_arguments(char* line) {
	args = (char**) calloc(sizeof(char*), 100);
	int argc = 0;
	int i = 0;
	int j = 0;

	while(line[i]) {
		if(line[i] == ' ') {
			line[i]='\0';
			args[argc] = &line[j];
			j = i+1;
			argc++;
		}
		i++;
	}
	args[argc] = &line[j];

	return args;
}

char** isolate_command_arguments(char** arguments) {
	int i = 0;
	int arguments_count = 1;
	int iter = 1;

	// Find how many arguments are there, +1 every sign with is not < or >
	while(arguments[i]) {
		if(arguments[i][0] != '<' || arguments[i][0] != '>') {
			arguments_count++;
		}
		i++;
	}


	char **cmd_args = (char**) calloc(sizeof(char*), arguments_count + 1);
	cmd_args[0] = arguments[0];
	i = 1;

	// Until there are more arguments
	while(arguments[i]) {

		// If string begins with < or > ignore it and also next argument
		if(arguments[i][0] != '<' || arguments[i][0] != '>') {

			//Put it into array as iter-th argument
			cmd_args[iter] = arguments[i];
			iter++;
			i++;
		}

		i++;
	}

	// Terminate array with NULL
	cmd_args[iter] = '\0';

	return cmd_args;
}

int execute_line(char **args) {
	int file_descriptor;
	int argc = 0;

	if(strcmp(args[0], "exit") == 0) exit(0);

	if(fork() == 0) {

		// Copy reference to STDIN and STDOUT
		int stdin_copy = dup(0);
		int stdout_copy = dup(1);

		while(args[argc]) {
			if(strcmp(args[argc], ">") == 0) { // Print output to file
				if(args[argc+1]) {
					close(1);
					file_descriptor = open(args[argc+1], O_WRONLY | O_CREAT, 0644);
				}
			} else if (strcmp(args[argc], ">>") == 0)  { // Append output to file
				if(args[argc+1]) {
					close(1);
					file_descriptor = open(args[argc+1], O_WRONLY | O_APPEND, 0644);
				}
			} else if (strcmp(args[argc], "<") == 0) { // Input from file
				if(args[argc+1]) {
					close(0);

					//@todo
				}
			}
			argc++;
		}

		fprintf(stdout, "Executing...");

		char **command_args = isolate_command_arguments(args);

		execvp(command_args[0], command_args);
		close(file_descriptor);
		perror("execvp");
		exit(0);
	}

	wait(NULL);
	printf("------------------------------\n");
	return 0;
}

int main(int argc, char** argv) {

	if(argc >= 2) {
		// Batch mode
		printf("Batch mode... Args: %d\n", argc);

		int i = 0;
		char** arguments = (char**) calloc(sizeof(char*), argc);

		batch_mode = true;

		for(i = 0; i < argc - 1; i++) {
			printf("%s, ", argv[i + 1]);
			arguments[i] = argv[i + 1];
		}
		printf("\n");
		arguments[argc] = '\0';

		execute_line(argv);
	}
	else {
		// Interactive mode
		char* line;

		while((line=readline("sh > ")) != NULL) {
			if(*line) {
				add_history(line);
				execute_line(isolate_arguments(line));
				free(line);
			}
		}
		printf("\nInterpreter closed.\n");
		exit(0);
	}
}
