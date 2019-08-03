#include <stdio.h>
#include <string.h>
#include "parsing.h"
#include "addresses.h"
#include "cache.h"
#include "error.h"
#include "untagged.h"
#include "utils.h"
#include "commands.h"

#define COMMAND_SIZE 300 //The size of a command string


/* Waiting for the tagged response while interpreting any untagged responses 
   is a pattern followed by many of the following functions */
int sendCommand(FILE *imapStream, msgCacheT *cachePtr, char *command, int context) {
	char commandTag[TAG_SIZE];
	imapObjectHandleT resTag, servResponse; //Handles used to access parts of the server's responses
	int retVal;

	generateTag(commandTag);

	//Send the command using the generated tag
	if (fprintf(imapStream, "%s %s\r\n", commandTag, command) < 0) {
		return(SOCKET_ERROR);
	}

	//All responses are in the form: <tag> SP <data> CRLF
	/* Loop until a tagged response is found (the tag matches that of the command,
	 as this application processes one command at a time) */
	do {
		retVal = getStringObject(&resTag, imapStream); //Get the response's tag
		if (isError(retVal)) {
			return(retVal);
		}
		//If the tag matches that of the command
		if (!strcmp(resTag->content.string, commandTag)) {
			freeImapObject(resTag);

			retVal = skipSpace(imapStream); //Skip a space
			if (isError(retVal)) {
				return(retVal);
			}

			//Get the next word of the response
			retVal = getStringObject(&servResponse, imapStream);
			if (isError(retVal)) {
				return(retVal);
			}

			//Case insenistive in case the server uses lowecase
			strUpper(servResponse->content.string);

			//If it was an OK response
			if (!strcmp(servResponse->content.string, "OK")) {
				freeImapObject(servResponse);
				break;
			}
			else if (!strcmp(servResponse->content.string, "NO")) { 
				/* If it was a NO response, print the rest, as it could be an error message,
				   but do not terminate (not fatal) */
				freeImapObject(servResponse);
				fprintf(stderr, "[SERVER]: ");
				if (isError(retVal = printLine(stderr, imapStream))) {
					return(retVal);
				} 
				break;
			}
			else { /* Assuming this application follows the IMAP protocol correctly,
				     a BAD response is the product of an incompatibility between implementations,
					 or the server does not follow the protocol. In any case, print the rest as
					 an error message and terminate (as an error code is returned) */
				freeImapObject(servResponse);
				fprintf(stderr, "[SERVER]: ");
				printLine(stderr, imapStream);
				return(PARSE_ERROR);
			}
		}
		/*This client operates on one command at a time, 
		  so if the tags don't match, the response is untagged */
		else {
			freeImapObject(resTag);

			retVal = interpretUntagged(imapStream, cachePtr, context);
			if (isError(retVal)) {
				return(retVal);
			}
		}
	} while(1);

	retVal = skipLine(imapStream);
	if (isError(retVal)) { //Skip the rest of the line
		return(retVal);
	}

	return(SUCCESS);
}

int sendFetchText(FILE *imapStream, msgCacheT *cachePtr, size_t msgNum) {
	char command[COMMAND_SIZE];
	int retVal;

	sprintf(command, "FETCH %lu RFC822.TEXT", msgNum);
	retVal = sendCommand(imapStream, cachePtr, command, NO_CONTEXT);
	if (isError(retVal)) {
		return(retVal);
	}

	return(SUCCESS);
}

int sendFetchAll(FILE *imapStream, msgCacheT *cachePtr, size_t startNum, size_t endNum) {
	char command[COMMAND_SIZE];
	int retVal;

	if (endNum == 0) { //If endNum is zero, no messages exist (as a convention)
		return(SUCCESS);
	}
	
	if (startNum == endNum) { //If startNum == endNum, fetch data for a single message
		sprintf(command, "FETCH %lu ALL", startNum);
	}
	else {
		sprintf(command, "FETCH %lu:%lu ALL", startNum, endNum);
	}

	retVal = sendCommand(imapStream, cachePtr, command, NO_CONTEXT);
	if (isError(retVal)) {
		return(retVal);
	}

	return(SUCCESS);
}

/* This does not use the general sendCommand() function, as it behaves differently on NO responses,
 as a mailbox must always be selected, so SEND_AGAIN is returned to indicate the
 need to retry selecting a mailbox in case of a non-fatal error (NO) */
int sendSelect(FILE *imapStream, msgCacheT *cachePtr, char *mailboxName) { 
	char commandTag[TAG_SIZE];
	imapObjectHandleT resTag, response;
	int retVal;

	generateTag(commandTag);

	if (fprintf(imapStream, "%s SELECT %s\r\n", commandTag, mailboxName) < 0) {
		return(SOCKET_ERROR);
	}
	
	//Loop until a tagged response (with tag == commandTag) is recieved
	do {
		retVal = getStringObject(&resTag, imapStream);
		if (isError(retVal)) {
			return(retVal);
		}
		//If the response tag matches commandTag
		if (!strcmp(resTag->content.string, commandTag)) {
			freeImapObject(resTag);

			if (isError(retVal = skipSpace(imapStream))) {
				return(retVal);
			}
			break;
		}

		//Else if untagged
		freeImapObject(resTag);

		//Interpret it
		retVal = interpretUntagged(imapStream, cachePtr, IN_SELECT);
		if (isError(retVal)) {
			return(retVal);
		}
	} while(1);

	//Else check the next word of the response
	retVal = getStringObject(&response, imapStream);
	if (isError(retVal)) {
		return(retVal);
	}

	strUpper(response->content.string); //Uppercase so that comparisons are case-insensitive 

	if (!strcmp(response->content.string, "OK")) {
		freeImapObject(response);

		if (isError(retVal = skipLine(imapStream))) { //Skip the rest of the line
			return(retVal);
		}

		/* In order to not waste time fetching the message data at the user's demand
		  fetch all data except the text for the messages of the selected mailbox */
		retVal = sendFetchAll(imapStream, cachePtr, 1, cachePtr->cacheSize);
		if (isError(retVal)) {
			return(retVal);
		}
		/* Update the cache data, the previous and current sizes being equal, 
		 indicates that no data needs to be fetched (for more on that, check interactionLoop() in main.c) */
		cachePtr->prevSize = cachePtr->cacheSize; 

		return(SUCCESS);
	}
	else if (!strcmp(response->content.string, "NO")) {
		freeImapObject(response);
		fprintf(stderr, "[SERVER]: ");
		if (isError(retVal = printLine(stderr, imapStream))) { //Print the server's error message and retry
			return(retVal);
		} 
		return(SEND_AGAIN);
	}
	//Something went terribly wrong (BAD or something unknown), print the rest of the response, and terminate
	freeImapObject(response);
	fprintf(stderr, "[SERVER]: ");
	printLine(stderr, imapStream);

	return(PARSE_ERROR);
}

int listMailboxNames(FILE *imapStream, msgCacheT *cachePtr) {
	/* With "" (in essence, the root of the mailbox hiererachy) as the reference name (first arguement), the names
	 are printed as they are supplied to SELECT, something that aids the user,
	 and with the wildcard "%" as the mailbox name pattern (second arguement), the mailbox names
	 that are printed, are those of the mailboxes that are directly under the root (so it does not
	 print the names of all the mailboxes recursively) */

	char command[COMMAND_SIZE] = "LIST \"\" %%"; 
	int retVal;

	//Send a LIST command, the mailbox names will be printed by the interpretList function in untagged.h
	retVal = sendCommand(imapStream, cachePtr, command, IN_LIST);
	if (isError(retVal)) {
		return(retVal);
	}
	putchar('\n');

	return(SUCCESS);
}

int sendNoop(FILE *imapStream, msgCacheT *cachePtr) {
	char command[COMMAND_SIZE] = "NOOP";
	int retVal;

	//Send a NOOP command
	retVal = sendCommand(imapStream, cachePtr, command, 0);
	if (isError(retVal)) {
		return(retVal);
	}

	return(SUCCESS);
}

int deleteMsg(FILE *imapStream, msgCacheT *cachePtr, int msgNum) {
	char command[COMMAND_SIZE];
	int retVal;

	//If the message number is out of bounds
	if (msgNum <= 0 || msgNum > cachePtr->cacheSize) {
		printf("[ERROR]: Message number is out of bounds, try 0 < msgNum =< %lu, next time.\n", cachePtr->cacheSize);
		return(SUCCESS);
	}

	/* Send a STORE command, in order to add the \DELETED flag to the message's flags, 
	   thus marking it for deletion */
	sprintf(command, "STORE %d +FLAGS (\\DELETED)", msgNum);
	retVal = sendCommand(imapStream, cachePtr, command, NO_CONTEXT);
	if (isError(retVal)) {
		return(retVal);
	}

	return(SUCCESS);
}

int undeleteMsg(FILE *imapStream, msgCacheT *cachePtr, int msgNum) {
	char command[COMMAND_SIZE];
	int retVal;

	if (msgNum <= 0 || msgNum > cachePtr->cacheSize) {
		printf("[ERROR]: Message number is out of bounds, try 0 < msgNum =< %lu, next time.\n", cachePtr->cacheSize);
		return(SUCCESS);
	}

	/* Send a STORE command, in order to remove the \DELETED flag from the message's flags, 
	   thus undeleting it */
	sprintf(command, "STORE %d -FLAGS (\\DELETED)", msgNum);
	retVal = sendCommand(imapStream, cachePtr, command, NO_CONTEXT);
	if (isError(retVal)) {
		return(retVal);
	}

	return(SUCCESS);
}

int sendExpunge(FILE *imapStream, msgCacheT *cachePtr) {
	int retVal;

	//Send an expunge command to purge all Deleted messages
	retVal = sendCommand(imapStream, cachePtr, "EXPUNGE", NO_CONTEXT);
	if (isError(retVal)) {
		return(retVal);
	}
	return(SUCCESS);
}

int sendLogin(FILE *imapStream, char *username, char *password) {
	int retVal;
	imapObjectHandleT resTag, response;
	char commandTag[TAG_SIZE];

	generateTag(commandTag);

	//Attempt to send a LOGIN command
	if (fprintf(imapStream, "%s LOGIN \"%s\" \"%s\"\r\n", commandTag, username, password) < 0) {
		return(SOCKET_ERROR);
	}

	/* A mailbox wasn't selected, so no unsolicited responses will be sent, so if the
	  tags don't match, a serious error has occured */
	retVal = getStringObject(&resTag, imapStream);
	if (isError(retVal)) {
		return(retVal);
	}
	else if (strcmp(resTag->content.string, commandTag)) { 
		fprintf(stderr, "Non matching tags, server-side protocol fail!\n");
		freeImapObject(resTag);
		return(PARSE_ERROR);
	}
	freeImapObject(resTag);

	if (isError(retVal = skipSpace(imapStream))) {
		return(retVal);
	}

	retVal = getStringObject(&response, imapStream);
	if (isError(retVal)) {
		return(retVal);
	}

	strUpper(response->content.string);

	//If there was an OK response
	if (!strcmp(response->content.string, "OK")) {
		freeImapObject(response);
		if (isError(retVal = skipLine(imapStream))) {
			return(retVal);
		}
		return(SUCCESS);
	}
	/* If a NO was returned, the user entered wrong credentials, 
	   so they must try again for the execution to progress (SEND_AGAIN) */
	else if (!strcmp(response->content.string, "NO")) {
		freeImapObject(response);
		fprintf(stderr, "[SERVER]: ");
		if (isError(retVal = printLine(stderr, imapStream))) {
			return(retVal);
		}
		return(SEND_AGAIN);
	}
	/* If BAD was sent, LOGIN is probably not supported (some IMAP servers require other means of
	 authentication), so execution cannot continue */

	fprintf(stderr, "[SERVER]: ");
	if (isError(retVal = printLine(stderr, imapStream))) {
		return(retVal);
	}

	freeImapObject(response);
	return(COMMAND_ERROR);
}

int logout(FILE *imapStream) { 
	char commandTag[TAG_SIZE];
	imapObjectHandleT resTag, response;
	int retVal;

	generateTag(commandTag);

	//Send a LOGOUT command
	if (fprintf(imapStream, "%s LOGOUT\r\n", commandTag) < 0) {
		return(SOCKET_ERROR);
	}

	//Loop until a tagged response is returned (the tag matching commandTag of course)
	do {
		retVal = getStringObject(&resTag, imapStream);
		if (isError(retVal)) {
			return(retVal);
		}
		strUpper(resTag->content.string);
		if (!strcmp(commandTag, resTag->content.string)) {
			freeImapObject(resTag);
			break;
		}
		//Else it will be an untagged reponse
		freeImapObject(resTag);
		if (isError(retVal = skipSpace(imapStream))) {
			return(retVal);
		}
		retVal = getStringObject(&response, imapStream);
		if (isError(retVal)) {
			return(retVal);
		}
		strUpper(response->content.string);

		//Upon the BYE response, print the server message
		if (!strcmp(response->content.string, "BYE")) {
			fprintf(stderr, "[SERVER]: ");
			if (isError(retVal = printLine(stderr, imapStream))) {
				freeImapObject(response);
				return(retVal);
			}
		}
		else { /* Any untagged response other than BYE is not of any 
		          interest at this stage, so ignore it */
			if (isError(retVal = skipLine(imapStream))) {
				freeImapObject(response);
				return(retVal);
			}
		}
		freeImapObject(response);
	} while(1);

	return(SUCCESS);
}