#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <string.h>
#include <stdlib.h>

char **args;

int get_line_length(char *line) {
	int i = 0;
	while(line[i]) {
		i++;
	}
	return i;
}

char* isolate_command(char* line) {
	unsigned int i = 0;
	while(line[i] && line[i] != ' ') {
		i++;
	}
	char* command = calloc(sizeof(char), i+1);
	memcpy(command, line, i);
	command[i] = '\0';
	return command;
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

int execute_line(char *line) {
	char* command = isolate_command(line);
	printf("Command isolated: %s\n", command);

	char** args = isolate_arguments(line);
	printf("Isolated arguments: \n");

	int argc = 0;

	int i = 1;
	while(args[i]) {
		printf("Arg[%d]: %s\n", i, args[i]);
		i++;
	}

	if(strcmp(command, "exit") == 0) exit(0);

	// Copy reference to STDIN and STDOUT
		int stdin_copy = dup(0);
		int stdout_copy = dup(1);
		int file_descriptor;

		while(args[argc]) {
			if(args[argc][0] == '>') { // If this sign was detected, print output to file (next arg)
				if(args[argc+1]) {
					close(1);
					file_descriptor = open(args[argc+1], O_WRONLY | O_CREAT, 0644);
				}
			}
			argc++;
		}


	if(fork() == 0) {

		printf("Execution... \n");
		execvp(command, args);

		close(file_descriptor);
		dup2(stdout_copy, 1);
		close(stdout_copy);

		exit(0);
	}

	wait(NULL);
	return 0;
}

int main() {
	char* line;
	while((line=readline("sh > ")) != NULL) {
		printf("%s\n", line);
		if(*line) {
			add_history(line);
			execute_line(line);
			free(line);
		}
	}
	printf("\nInterpreter closed.\n");
	exit(0);
}
