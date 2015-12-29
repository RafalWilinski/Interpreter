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
bool batch_mode = false;														// Indicates whether interpreter had supplied runtime arguments	

// @TODOS
// Override ctrl+c signal to stop child task - executor
// Simplify functions such as array length
// Fix warnings generated during compilation with -Wall flag

// Scope:
// DONE - Możliwość uruchamiania poleceń z dowolną liczbą argumentów w trybie pierwszoplanowym (interpreter oczekuje na zakończenie wykonania procesu). Wymagane na ocenę 3.0.
// DONE - Obsługa przekierowań strumieni wejściowych i wyjściowych do i z plików. Wymagane na ocenę 3.0.
// ???  - Obsługa komentarzy.
// TODO - Uruchamianie poleceń z dowolną liczbą argumentów w tle wraz z powiadamianiem o zakończeniu tych procesów (PID + status). Wymagane na ocenę 3.5.
// DONE - Obsługa trybu interaktywnego i wsadowego. Wymagane na ocenę 3.5.
// TODO - Obsługa procesów zombie. Wymagane na ocenę 4.0.

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

    /* Move the front and back pointers to address the first non-whitespace
     * characters from each end. */
    while( isspace(*frontp) ) { 
    	++frontp; 
    }

    if( endp != frontp ) {
        while( isspace(*(--endp)) && endp != frontp ) {}
    }

    if( str + len - 1 != endp )
        *(endp + 1) = '\0';
    else if( frontp != str &&  endp == frontp )
        *str = '\0';

    /* Shift the string so that it starts at str so that if it's dynamically
     * allocated, we can still free it on the returned pointer.  Note the reuse
     * of endp to mean the front of the string buffer now.
     */
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
		if((strcmp(arguments[i], "<") != 0) && (strcmp(arguments[i], ">") != 0)) {
			arguments_count++;
		} else i++;																// Skip this and also next sign
		i++;
	}

	char **cmd_args = (char**) calloc(sizeof(char*), arguments_count + 1);
	i = 0;

	while(arguments[i]) {
		if((strcmp(arguments[i], "<") != 0) && (strcmp(arguments[i], ">") != 0)) { // If string begins with < or > ignore it and also next argument
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

				printf("Arguments from file:\n");

				for(n = 0; n < size; n++) {
					printf("%s\n", in_arguments[n]);
				}

				printf("-------------------\n");
			}
		}

		i++;
	}

	//Section: Append potential array from file into passed as argument
	i = 0;

	int final_length = 0;
	while(args[i]) {
		i++;
	}

	final_length = i;
	i = 0;

	if(has_input_file) {														// If there was < (command supplied from file) append to final args array
		while(in_arguments[i]) {
			i++;
		}
		final_length += i;
	}

	char **final_array = (char**) calloc(sizeof(char*), final_length + 1);		// Allocate space for it

	i=0;

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

	if(strcmp(args[0], "exit") == 0) exit(0);

	if(fork() == 0) {									

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

		// Just for debug purpouses
		// printf("Final array: \n");
		// i = 0;
		// while(command_args[i]) {
		// 	printf("* %s\n", command_args[i]);
		// 	i++;
		// }

		// printf("----------\n");

		execvp(command_args[0], command_args);
		close(file_descriptor);
		perror("execvp");
		exit(0);
	}

	wait(NULL);
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