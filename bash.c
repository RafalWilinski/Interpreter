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

#define FILE_BUFFER_SIZE 1000

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
	int iter = 0;

	// Find how many arguments are there, +1 every sign with is not < or >
	while(arguments[i]) {
		if(arguments[i][0] != '<' || arguments[i][0] != '>') {
			arguments_count++;
		}
		i++;
	}


	char **cmd_args = (char**) calloc(sizeof(char*), arguments_count + 1);
	// cmd_args[0] = arguments[0];
	i = 0;

	// Until there are more arguments
	while(arguments[i]) {

		// If string begins with < or > ignore it and also next argument
		if(arguments[i][0] != '<' || arguments[i][0] != '>') {

			//Put it into array as iter-th argument
			cmd_args[iter] = arguments[i];
			iter++;
		}
		else {
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
	int i = 0;
	int n = 0;
	bool has_input_file = false;

	char **in_arguments = NULL;

	while(args[i]) {
		if (strcmp(args[i], "<") == 0) { 										// Input from file
			if(args[i+1]) {
				int stdin_fd = open(args[i+1], O_RDONLY, 0644);  				// Open file
				FILE* fp = fdopen(stdin_fd, "r");								// Convert to higher-level descriptor

				has_input_file = true;

				fseek(fp, 0, SEEK_END);  										// Seek to end of the file
				unsigned long size = ftell(fp);									// Get position of pointer, tell the difference
				fseek(fp, 0, SEEK_SET);											// Rewind pointer to beginning of file

				char *in_buffer = (char*) malloc(sizeof(char) * size + 1);		// Allocate array of that size

				fread(in_buffer, sizeof(char), size, fp);						// Read file contents into array
				in_buffer[size] = '\0';

				printf("Size: %lu, Array: %s\n", size, in_buffer);

				in_arguments = isolate_arguments(in_buffer); 					// Isolate arguments read from file

				printf("Arguments from file:\n");

				for(n = 0; n < size; n++) {
					printf("%s\n", in_arguments[n]);
				}
			}
		}

		i++;
	}

	//Append potential array from file into passed as argument
	i = 0;

	int final_length = 0;
	while(args[i]) {
		i++;
	}

	final_length = i;
	i = 0;

	if(has_input_file) {
		while(in_arguments[i]) {
			i++;
		}
		final_length += i;
	}

	printf("Final array length: %d\n", final_length);

	char **final_array = (char**) calloc(sizeof(char*), final_length + 1);

	i=0;

	if(has_input_file) {
		while(in_arguments[i]) {
			final_array[i] = in_arguments[i];
			i++;
		}
	}

	int offset = i;
	i = 0;

	while(args[i]) {
		final_array[offset + i] = args[i];
		i++;
	}

	final_array[offset + i] = '\0';

	if(strcmp(args[0], "exit") == 0) exit(0);

	if(fork() == 0) {

		// Copy reference to STDIN and STDOUT
		int stdin_copy = dup(0);
		int stdout_copy = dup(1);

		while(final_array[argc]) {
			if(strcmp(final_array[argc], ">") == 0) { // Print output to file
				if(final_array[argc+1]) {
					close(1);
					file_descriptor = open(final_array[argc+1], O_WRONLY | O_CREAT, 0644);
				}
			} else if (strcmp(final_array[argc], ">>") == 0)  { // Append output to file
				if(final_array[argc+1]) {
					close(1);
					file_descriptor = open(final_array[argc+1], O_WRONLY | O_APPEND, 0644);
				}
			} 

			argc++;
		}

		char **command_args = isolate_command_arguments(final_array);

		printf("Final array: \n");
		i = 0;
		while(command_args[i]) {
			printf("%s\n", command_args[i]);
			i++;
		}

		printf("\n\n------");

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

	int n;

	if(argc >= 2) {
		// Batch mode
		printf("Batch mode... Args: %d\n", argc - 1);

		int i = 0;
		char** arguments = (char**) calloc(sizeof(char*), argc);

		batch_mode = true;

		for(i = 0; i < argc - 1; i++) {
			arguments[i] = argv[i + 1];
		}

		arguments[argc] = '\0';

		execute_line(arguments);
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
