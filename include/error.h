#include <stdio.h>

#ifndef ERROR_GUARD

	#define ERROR_GUARD
	
	//Error codes used throughout the program
	#define SUCCESS 0
	#define MEM_ERROR -1 //For memory allocation failure
	#define PARSE_ERROR -2 //For parsing errors (the server's fault)
	#define SOCKET_ERROR -3 //In case the server disconnects before the user attempts to logout
	#define COMMAND_ERROR -4 //An invalid command was sent (BAD tag)
	#define SYSCALL_ERROR -5 //If a system call fails

	int isError(int retVal);
	void printError(char *message, int retVal);
#endif
