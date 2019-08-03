#include <stdio.h>
#include "parsing.h"
#include "addresses.h"
#include "cache.h"
#include "error.h"
#include "utf8.h"
#include "commands.h"
#include "printing.h"

//ANSI escape codes 
#define BOLD_WHITE "\e[1;37m" //Used to emphasize the subject
#define RSET "\e[0m"
#define CLEAR "\033[2J" //Used to clear the screen

//Used in printing the message preview
#define NUM_CHARS 5 //Five digits for printing the message sequence number
#define FROM_CHARS 20 //Max characters to be printed for the From field
#define SUBJECT_CHARS 35 //Max charactes of the subject printed
#define DATE_CHARS 20 //DD-MMM-YYYY HH:MM:SS 

#define PAGE_MSGS  20 //The number of messages per page (displayMsgPage() displays messages by page)

//Used in printing the message size
#define KB 1024
#define MB 1024*KB

void clearScreen(void) {
	printf(CLEAR);
}

void printWhitespaces(int num) {
	for (int k = 0 ; k < num ; k++) {
		putchar(' ');
	}
}

void printStat(msgCacheT *cachePtr) {
	printf("Stats:\n");
	printf("\tMessages: %lu\n", cachePtr->cacheSize);
	printf("\tRecent: %lu\n", cachePtr->recent);
	if (cachePtr->cacheSize == 0) {
		printf("\tPages: 0\n");
	}
	else {
		printf("\tPages: %lu\n\n", cachePtr->cacheSize / PAGE_MSGS + 1);
	}
}

void printHelp(void) {
	printf("Commands:\n");
	printf("\tdelete <num> - Marks the message with message number <num> for deletion.\n");
	printf("\tundelete <num> - Unmarks the marked for deletion message <num>. If not marked, it does nothing.\n");
	printf("\texpunge - Deletes all messages that are marked for deletion.\n");
	printf("\tread <num> - Display the message with number <num>.\n");
	printf("\tpage <num> - Display all the messages on the page numbered <num>.\n");
	printf("\tlogout - Close the connection with the server, and close the program.\n");
	printf("\tselect <mailbox-name> - Select the mailbox named <mailbox-name>.\n");
	printf("\tlist - List mailbox names (not recursively).\n");
	printf("\tstats - Display information about the mailbox.\n");
	printf("\tclear - Clear the screen.\n");
	printf("\thelp - You are here.\n\n");
}

void printMsgContents(msgT *msgPtr) {
	if (msgPtr->envelope.subject != NULL) {
		printf(BOLD_WHITE"%s\n\n"RSET, msgPtr->envelope.subject);
	}
	else { //If the message didn't have a subject
		printf(BOLD_WHITE"(No Subject)\n\n"RSET);
	}
	printf("Date: %s\n", msgPtr->internalDate); 
	printf("From: ");
	printAddressList(msgPtr->envelope.fromList);
	putchar('\n');
	printf("To: ");
	printAddressList(msgPtr->envelope.toList);
	putchar('\n');

	//If no addresses were CC'd
	if (msgPtr->envelope.ccList != NULL) {
		printf("Cc: ");
		printAddressList(msgPtr->envelope.ccList);
		putchar('\n');
	}
	printf("\n\n");
	printf("%s\n", msgPtr->text);
}

//Print the message size, with the appropriate units
void printSize(int msgSize) {
	if (msgSize < KB) {
		printf("%d B", msgSize);
	}
	else if (msgSize >= KB && msgSize < MB) {
		printf("%.2f KB", (double)msgSize / KB);
	}
	else {
		printf("%.2f MB", (double)msgSize / MB);
	}
}

//Display a preview of a string, meaning that only the first <maxChars> characters are printed
void displayStrPreview(char *str, int maxChars) {
	int len;

	len = utf8StrLen(str); //Check utf8.h, strlen() can't be used with utf-8 chars
	if (len <= maxChars) {
		printf("%s", str);
		printWhitespaces(maxChars-len);
	}
	else {
		printNChars(str, maxChars-4); //The length of "(..)" is 4
		printf("(..)");
	}
}

void displayMsgPreview(msgT *msgPtr, size_t msgNum) {
	char format[10];

	//Print the message sequence number
	sprintf(format, "[%%0%dlu]", NUM_CHARS);
	printf(format, msgNum); //M<e 
	printWhitespaces(2);

	//Print subject (probably utf-8)
	if (msgPtr->envelope.subject != NULL) {
		displayStrPreview(msgPtr->envelope.subject, SUBJECT_CHARS);
	}
	else { //If the message does not have a subject
		printf("(No Subject)");
	}
	printWhitespaces(2);

	/*For the From field, print the personal-name ofthe first from-address, (head of list),
	 and if it does not exist, print the mailbox name of the head. */
	if (msgPtr->envelope.fromList->personalName != NULL) {
		displayStrPreview(msgPtr->envelope.fromList->personalName, FROM_CHARS);
	}
	else{
		displayStrPreview(msgPtr->envelope.fromList->mailboxName, FROM_CHARS);
	}

	printWhitespaces(2);

	//Print the Date field
	printNChars(msgPtr->internalDate, DATE_CHARS);
	printWhitespaces(2);

	//Print the Size field
	printSize(msgPtr->size);

	putchar('\n');
}

int displayMsg(FILE *imapStream, msgCacheT *cachePtr, int msgNum) {
	msgT **msgPtrArray = cachePtr->msgPtrArray;
	int retVal;

	if (cachePtr->cacheSize == 0) {
		printf("Mailbox is empty.\n");
		return(SUCCESS);
	}
	else if (msgNum > cachePtr->cacheSize || msgNum <= 0) {
		printf("Message number is out of bounds, try 0 < msgNum =< %lu, next time.\n", cachePtr->cacheSize);
		return(SUCCESS);
	}

	//If the text field is NULL, the text hasn't been fetched yet, so is here
	if (!msgPtrArray[msgNum-1]->text)  {
		retVal = sendFetchText(imapStream, cachePtr, msgNum);
		if (isError(retVal)) {
			return(retVal);
		}
	}

	printMsgContents(msgPtrArray[msgNum-1]);

	return(SUCCESS);
}

void printPageHeader(void) {
	printf("Msgnum:  ");
	printf("Subject:");
	printWhitespaces(SUBJECT_CHARS-6); //8 is the length of "Subject:", minus the 2 whitespaces in displayMsgPreview
	printf("From:");
	printWhitespaces(FROM_CHARS-3); //5 is the length of "From:", minus the 2 whitespaces in displayMsgPreview
	printf("Date:");
	printWhitespaces(DATE_CHARS-3); //5 is the length of "Date:", minus the 2 whitespaces in displayMsgPreview
	printf("Size:\n");
}

void displayMsgPage(FILE *imapStream, msgCacheT *cachePtr, size_t pageNum) {
	size_t msgs = cachePtr->cacheSize, loopLimit;
	msgT **msgPtrArray = cachePtr->msgPtrArray;

	if (msgs == 0) {
		printf("Mailbox is empty.\n");
		return;
	} 
	//msgs / PAGE_MSGS + 1 is the total number of pages
	else if (pageNum > msgs / PAGE_MSGS +1 || pageNum < 1) {
		printf("Page number is out of bounds, try 0 < pageNum =< %lu, next time.\n", msgs / PAGE_MSGS + 1);
		return;
	}

	/* If the page is not the last, or it has exactly PAGE_MSGS messages, then the limit 
	is the last element of the page, else it is the final message of the mailbox. */
	if (PAGE_MSGS * pageNum < msgs) {
		loopLimit = PAGE_MSGS * pageNum;
	}
	else {
		loopLimit = msgs;
	}

	printPageHeader();

	//Display all messages of the page
	for (size_t k = PAGE_MSGS * (pageNum-1) ; k < loopLimit ; k++) { 
		displayMsgPreview(msgPtrArray[k], k+1);
	}
}
