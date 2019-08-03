#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <poll.h>
#include <unistd.h>
#include "error.h"
#include "parsing.h"
#include "utils.h"
#include "addresses.h"
#include "cache.h"
#include "untagged.h"
#include "printing.h"
#include "commands.h"

#define NAME_SIZE 64 //Used for user input
#define NOOP_INTERVAL 3000 //Interval before sending a NOOP to server in microseconds
#define MAX_LINE 128 //Used of user input

int establishConnection(char *hostname, char *port); //Establish connection with server
int getGreeting(FILE *imapStream); //Get the server greeting (according to the IMAP protocol)
int attemptLogin(FILE *imapStream); /* Try to login (with user inputted credentials), 
                                      until success, or the user chooses to quit */
int interactionLoop(FILE *imapStream, msgCacheT *cachePtr); //Polls stdin for input, and after a timeout sends NOOP to server
int handleUserInput(FILE *imapStream, msgCacheT *cachePtr); //Executes commands entered by the yser
int userSelectMailbox(FILE *imapStream, msgCacheT *cachePtr); /* Loops until the user selects an existing mailbox, 
                                                         or selects INBOX if the user stops trying */


int main(int argc, char *argv[]) {
	int imapSock, retVal;
	FILE *imapStream;
	msgCacheT *msgCache;

	if (argc < 3) {
		fprintf(stderr, "Run with <hostname> <port> next time.\n");
		return(1);
	}

	imapSock = establishConnection(argv[1], argv[2]);
	if (isError(imapSock)) {
		printError("Failed to establish connection", imapSock);
		return(1);
	}

	//Open the socket as a FILE stream in order to utilize functions from stdio.h
	imapStream = fdopen(imapSock, "r+b"); 
	if (!imapStream) {
		printError("Failed to establish connection", SYSCALL_ERROR);
		close(imapSock);
		return(1);
	}

	retVal = getGreeting(imapStream);
	if (isError(retVal)) {
		fclose(imapStream);
		printError("Problem with getting greeting", retVal);
		return(1);
	}

	retVal = attemptLogin(imapStream);
	if (isError(retVal)) { //BAD
		fclose(imapStream);
		printError("Login failed horribly", retVal);
		return(1);
	}
	//If QUIT was returned, user refused to retry, so don't select a mailbox, and skip to the end
	else if (retVal != QUIT) {
		printf("Login was successful!\n");

		msgCache = cacheInit();
		if (!msgCache) {
			fclose(imapStream);
			printError("Cache initialization failed", retVal);
			return(1);
		}

		retVal = sendSelect(imapStream, msgCache, "INBOX"); //Choose inbox by default
		if (isError(retVal)) {
			printError("Inbox selection failed", retVal);
			fclose(imapStream);
			freeMsgCache(msgCache);
			return(1);
		}
		printf("Mailbox was selected successfully!\n");

		retVal = interactionLoop(imapStream, msgCache); //Enter the interaction loop
		if (isError(retVal)) {
			printError("Something went wrong", retVal);
			freeMsgCache(msgCache);
			fclose(imapStream);
			return(1);
		}

		//Free cache
		freeMsgCache(msgCache);

		//Attempt to logout
		retVal = logout(imapStream);
		if (isError(retVal)) {
			printError("Logout didn't go well", retVal);
			fclose(imapStream);
			return(1);
		}
	}
	
	if (fclose(imapStream) < 0) { //Calling fclose() closes both imapSock and imapStream
		return(1);
	}

	printf("Connection closed. Now exiting.\n");

	return(0);
}

int establishConnection(char *hostname, char *port) {
	int sockFd;
	struct addrinfo hints = {0};
	struct addrinfo *addrHead, *iter;

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	if (getaddrinfo(hostname, port, &hints, &addrHead) != 0) {
		return(SYSCALL_ERROR);
	}

	/* Iterate through the address list, until connecting to an address,
	  or reaching the end of the list */
	for (iter = addrHead ; iter != NULL ; iter = iter->ai_next) {
		sockFd = socket(AF_INET, SOCK_STREAM, 0); //Create a socket
		if (sockFd < 0) {
			freeaddrinfo(addrHead);
			return(SYSCALL_ERROR);
		}
		//If connect() succeds, break
		if (connect(sockFd, iter->ai_addr, iter->ai_addrlen) == 0) {
			break;
		}
		close(sockFd);
	}

	if (!iter) { //Failed to connect to any of the addresses, or no addresses
		freeaddrinfo(addrHead);
		return(SOCKET_ERROR);
	}

	freeaddrinfo(addrHead);

	return(sockFd);
}

int getGreeting(FILE *imapStream) { //The successful greeting is "*" OK <text> CRLF
	imapObjectHandleT tag, response;
	int retVal;

	retVal = getStringObject(&tag, imapStream);
	if (isError(retVal)) {
		return(retVal); 
	}
	//If the response isn't untagged, then the server did not follow the protocol
	else if (strcmp(tag->content.string, "*")) {
		freeImapObject(tag);
		return(PARSE_ERROR);
	}
	freeImapObject(tag);

	if (isError(retVal = skipSpace(imapStream))) {
		return(retVal);
	}

	retVal = getStringObject(&response, imapStream);
	if (isError(retVal)) {
		return(retVal);
	}
	//if the response is OK
	else if (!strcmp(response->content.string, "OK")) {
		freeImapObject(response);
		if (isError(retVal = skipLine(imapStream))) { //Skip the restof the line
			return(retVal);
		}
		return(SUCCESS);
	}

	//If the response was not OK, something went wrong
	freeImapObject(response);
	fprintf(stderr, "[SERVER]: ");
	printLine(stderr, imapStream); //Prints the rest of the line as an error message

	return(PARSE_ERROR);
}

int attemptLogin(FILE *imapStream) {
	char username[NAME_SIZE], password[NAME_SIZE], format[10];
	char option;
	int retVal;

	sprintf(format, "%%%ds", NAME_SIZE-1);

	//Loop until LOGIN succeds, or user stops trying
	do {
		printf("Enter username: ");
		scanf(format, username);
		printf("Enter password: ");
		scanf(format, password);
		retVal = sendLogin(imapStream, username, password);
		if (isError(retVal)) {
			return(retVal);
		}
		else if (retVal == SUCCESS) { 
			if (fflush(stdin) < 0) { 
				return(SYSCALL_ERROR);
			}
			return(SUCCESS);
		}
		//If retVal is not an error code or success, then it is SEND_AGAIN, so retry 

		printf("Try again? (y/Y || n/N)\n");
		scanf(" %c", &option);
	} while(option == 'y' || option == 'Y');

	return(QUIT); //If the user stops trying, return QUIT in order to notify main() to close the program
}

int userSelectMailbox(FILE *imapStream, msgCacheT *cachePtr) {
	char mailboxName[NAME_SIZE];
	int retVal;
	char option;
	char mailFormat[10];

	sprintf(mailFormat, "%%%ds", NAME_SIZE-1);

	//Loop until SELECT succeds, or user stops trying
	do {
		scanf(mailFormat, mailboxName);
		retVal = sendSelect(imapStream, cachePtr, mailboxName);
		if (isError(retVal)) {
			return(retVal);
		}
		else if (retVal == SUCCESS) { 
			printf("Mailbox \"%s\" was selected!\n", mailboxName);
			return(SUCCESS);
		}
		
		//If retVal is not an error code or success, then it is SEND_AGAIN, so retry 

		printf("Try again? (y/Y || n/N)\n"); //Else don't try again
		scanf(" %c", &option);

		//Print "Mailbox name:" when the user retries, in order to improve user interaction
		if (option == 'y'|| option == 'Y') { 
			printf("Mailbox name: ");
		}
	} while (option == 'y' || option == 'Y'); 

	//If user stops trying, select inbox by default
	retVal = sendSelect(imapStream, cachePtr, "INBOX");
	if (isError(retVal)) {
		return(retVal);
	}

	printf("Back to inbox.\n");

	return(SUCCESS);
}

int handleUserInput(FILE *imapStream, msgCacheT *cachePtr) {
	char command[MAX_LINE], commandFormat[10];
	int retVal;

	sprintf(commandFormat, "%%%ds", MAX_LINE-1);

	scanf(commandFormat, command); //Get command, and depending on the command, get the other arguements
	if (!strcmp(command, "!delete")) {
		int msgNum; 

		/* Get msgNum, note that this is the number that the user sees, 
		  and IMAP commands utilize, so it is greater by 1 compared to the 
		  cache position of the message */
		scanf("%d", &msgNum);
		retVal = deleteMsg(imapStream, cachePtr, msgNum); 
		if (isError(retVal)) {
			return(retVal);
		}
	}
	else if (!strcmp(command, "!undelete")) {
		int msgNum;

		scanf("%d", &msgNum); 
		retVal = undeleteMsg(imapStream, cachePtr, msgNum); 
		if (isError(retVal)) {
			return(retVal);
		}
	}
	else if (!strcmp(command, "!expunge")) {
		retVal = sendExpunge(imapStream, cachePtr);
		if (isError(retVal)) {
			return(retVal);
		}
	}
	else if (!strcmp(command, "!read")) {
		int msgNum;

		scanf("%d", &msgNum);
		retVal = displayMsg(imapStream, cachePtr, msgNum);
		if (isError(retVal)) {
			return(retVal);
		}
	}
	else if (!strcmp(command, "!page")) {
		int pageNum;

		//Get pageNum, as the user sees it
		scanf("%d", &pageNum);
		displayMsgPage(imapStream, cachePtr, pageNum);
	}
	else if (!strcmp(command, "!logout")) {
		return(QUIT);
	}
	else if (!strcmp(command, "!select")) {
		retVal = userSelectMailbox(imapStream, cachePtr);
		if (isError(retVal)) {
			return(retVal);
		}
	}
	else if (!strcmp(command, "!list")) {
		retVal = listMailboxNames(imapStream, cachePtr);
		if (isError(retVal)) {
			return(retVal);
		}
	}
	else if (!strcmp(command, "!stats")) {
		printStat(cachePtr);
	}
	else if (!strcmp(command, "!clear")) {
		clearScreen();
	}
	else if (!strcmp(command, "!help")) {
		printHelp();
	}
	else { //If the command is not recognized, it is considered invalid and nothing happens
		printf("Invalid command. Try \"!help\" for a list of the supported commands.\n");
	}

	return(SUCCESS);
}

int interactionLoop(FILE *imapStream, msgCacheT *cachePtr) {
	struct pollfd pollfd = {0};
	int retVal;

	/* The variable inputFlag is used to control when the command prompt "=>>"
	  is printed. It is set at the start (so that the prompt can be printed at first),
	  and when the user enters input, so that it can't be printed again. If the 
	  user does not enter input (so timeout passes and the loop continues), 
	  it is not printed again */
	int inputFlag = 1;

	//Prepare struct pollfd
	pollfd.fd = STDIN_FILENO;
	pollfd.events = POLLIN;

	do {
		if (inputFlag) {
			printf("=>> "); 
			if (fflush(stdout) < 0) { //For the prompt to apperar despite stdio's line buffering
				return(SYSCALL_ERROR);
			}
			inputFlag = 0;
		}
		//NOOP_INTERVAL is muly
		if (poll(&pollfd, 1, NOOP_INTERVAL) < 0) { //If timeout elapses, send NOOP to server
			return(SYSCALL_ERROR);
		}
		else if (pollfd.revents & POLLIN) { //If the user entered a command
			retVal = handleUserInput(imapStream, cachePtr);
			if (isError(retVal)) {
				return(retVal);
			}
			else if (retVal == QUIT) { //If quit was returned, user asked to logout (so quit)
				break; 
			}
			inputFlag = 1; //User entered input, so set inputFlag
		}

		retVal = sendNoop(imapStream, cachePtr);
		if (isError(retVal)) {
			return(retVal);
		}
		if (cachePtr->cacheSize > cachePtr->prevSize) { 
			/* If the cache grew (check cacheResize()), then there are new messages to be
			 fetched */
			retVal = sendFetchAll(imapStream, cachePtr, cachePtr->prevSize+1, cachePtr->cacheSize);
			if (isError(retVal)) {
				return(retVal);
			}
			//No need to fetch those anymore, so set prevSize to cacheSize
			cachePtr->prevSize = cachePtr->cacheSize;
		}	
	} while(1);

	return(SUCCESS);
}