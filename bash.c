#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>

typedef int bool;
#define true 1
#define false 0
#define builtin_cmds_length 3

char **args;
bool batch_mode = false; // Indicates whether interpreter had supplied runtime arguments	
const char *builtin_cmds[builtin_cmds_length];

int background_process_end_notification(int signum) {
	printf("Odebrano sygnal %d\n", signum);
    exit(0);
}

void signal_handler() {
	printf(" Killing process... \n");
}

int get_line_length(char *line) {
	int i = 0;
	while(line[i]) {
		i++;
	}
	return i;
}

// Remove whitespace characters from back and from of chars chain
char *trim(char *str) {
    size_t len = 0;
    char *frontp = str;
    char *endp = NULL;

    if( str == NULL ) { 
    	return NULL; 
    }
    if( str[0] == '\0' ) { 
    	return str; 
    }

    len = strlen(str);
    endp = str + len;

    while(isspace(*frontp)) { 
    	++frontp; 
    }

    if( endp != frontp ) {
        while(isspace(*(--endp)) && endp != frontp ) { /* do nothing */ }
    }

    if( str + len - 1 != endp )
        *(endp + 1) = '\0';
    else if( frontp != str &&  endp == frontp )
        *str = '\0';

    endp = str;
    if( frontp != str )
    {
        while( *frontp ) { *endp++ = *frontp++; }
        *endp = '\0';
    }

    return str;
}

// Basing on string from readline returns 2D array of arguments
char** isolate_arguments(char* line) {
	args = (char**) calloc(sizeof(char*), 100);
	int argc = 0;
	int i = 0;
	int j = 0;

	while(line[i]) {								
		if(line[i] == ' ') {													// If space sign was encountered, terminate array at this place and put predecessor to 2d array
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

// From 2D char array returns only valid for execvp arguments (stripping < and > signs with its descendant)
char** isolate_command_arguments(char** arguments) {
	int i = 0;
	int arguments_count = 1;
	int iter = 0;

	while(arguments[i]) {														// Find how many arguments are there, +1 every sign with is not < or >
		if((strcmp(arguments[i], "<") != 0) && (strcmp(arguments[i], ">") != 0) 
			&& (strcmp(arguments[i], "2>") != 0) && (strcmp(arguments[i], "&>") != 0)
			 && (strcmp(arguments[i], ">>") != 0) && (strcmp(arguments[i], "&") != 0)) {

			arguments_count++;
		} else i++;																// Skip this and also next sign
		i++;
	}

	char **cmd_args = (char**) calloc(sizeof(char*), arguments_count + 1);
	i = 0;

	while(arguments[i]) {
		if((strcmp(arguments[i], "<") != 0) && (strcmp(arguments[i], ">") != 0) // If string begins with < or > ignore it and also next argument
			&& (strcmp(arguments[i], "2>") != 0) && (strcmp(arguments[i], "&>") != 0) 
			&& (strcmp(arguments[i], ">>") != 0) && (strcmp(arguments[i], "&") != 0)) { 	

			cmd_args[iter] = arguments[i];										//Put it into array as iter-th argument
			iter++;
		} else  i++;

		i++;
	}

	cmd_args[iter] = '\0';														// Terminate array with NULL

	return cmd_args;
}

int execute_line(char **args) {
	int file_descriptor;
	int argc = 0;
	int i = 0;
	int n = 0;
	bool has_input_file = false;
	bool is_background_process = false;

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

				in_arguments = isolate_arguments(trim(in_buffer)); 				// Isolate arguments read from file, trim whitespace also
			}
		}

		i++;
	}

	//Section: Append potential array from file into passed as argument
	i = 0;

	int final_length = 0;
	while(args[i]) { i++; }

	final_length = i;
	i = 0;

	if(has_input_file) {														// If there was < (command supplied from file) append to final args array
		while(in_arguments[i]) { i++; }
		final_length += i;
	}

	i = 0;

	char **final_array = (char**) calloc(sizeof(char*), final_length + 1);		// Allocate space for it
	if(has_input_file) {														// If there was < (command supplied from file) append to final args array
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

	// Final array done, now process arguments
	i=0;
	bool is_builtin = false;
	for(i = 0; i < builtin_cmds_length; i++) {
		if(strcmp(args[0], builtin_cmds[i]) == 0) {
			is_builtin = true;
			if(i == 0) {
				exit(0);
			} else if (i == 1) {
				printf("Pomoc: \n");
			} else if (i == 2) {
				printf("%s\n", args[1]);
			}
			return 0;
		}
	}

	argc = 0;
	while(final_array[argc]) {
		if (strcmp(final_array[argc], "&") == 0) {
			is_background_process = true;
		}
		argc++;
	}

	int child_pid = fork();
	int background_task_executor;
	int child_status;
	int s = 0;

	if(child_pid == 0) {	
		argc = 0;

		while(final_array[argc]) {
			if(strcmp(final_array[argc], ">") == 0) { 							// Print output to file
				if(final_array[argc+1]) {
					close(1);
					file_descriptor = open(final_array[argc+1], O_WRONLY | O_CREAT, 0644);
				}
			} else if (strcmp(final_array[argc], ">>") == 0)  { 				// Append output to file
				if(final_array[argc+1]) {
					close(1);
					file_descriptor = open(final_array[argc+1], O_WRONLY | O_APPEND, 0644);
				}
			} 
			argc++;
		}

		char **command_args = isolate_command_arguments(final_array);

		if(is_background_process) {
			background_task_executor = fork();
			if(background_task_executor == 0) {
				if(!is_builtin) execvp(command_args[0], command_args);
			} else {
				waitpid(background_task_executor, &s, 0);
				printf("\n%d done\n", getpid());
			}
		} else {
			if(!is_builtin) execvp(command_args[0], command_args);
		}

		perror("execvp");
		exit(0);

	} else {
		if(!is_background_process) {
			waitpid(child_pid, &child_status, 0);
			return 0;
		} else {
			waitpid(background_task_executor, &s, 0);
			printf("%d\n", background_task_executor);
		}
		fflush(stdout);
		return 0;
	}
}

int main(int argc, char** argv) {

	// Register builting commands
	builtin_cmds[0] = "exit";
	builtin_cmds[1] = "help";
	builtin_cmds[2] = "echo";

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
		signal(SIGINT, signal_handler);

		while((line=readline("sh $ ")) != NULL) {
			if(*line) {
				add_history(line);
				execute_line(isolate_arguments(trim(line)));
				free(line);
			}
		}
		printf("\nInterpreter closed.\n");
		exit(0);
	}
}