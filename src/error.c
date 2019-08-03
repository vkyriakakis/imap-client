#include <stdio.h>
#include "error.h"

//A small function to improve readability
int isError(int retVal) {
	if (retVal < 0) {
		return(1);
	}
	return(0);
}

//Similar to perror, only using the error codes define in "error.h"
void printError(char *message, int retVal) {
	fprintf(stderr, "%s: ", message);
	switch(retVal) {
		case SUCCESS: 
			fputs("Success.", stderr);
			break;
		case MEM_ERROR: 
			fputs("Memory allocation error.", stderr);
			break;
		case PARSE_ERROR:
			fputs("Parsing error.", stderr);
			break;
		case SOCKET_ERROR:
			fputs("Connection error.", stderr);
			break;
		case COMMAND_ERROR:
			fputs("Invalid command error.", stderr);
			break;
		case SYSCALL_ERROR:
			perror(NULL);
			break;
	}
}