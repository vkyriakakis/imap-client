#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parsing.h"
#include "addresses.h"
#include "cache.h"
#include "utils.h"
#include "untagged.h"
#include "utf8.h"
#include "error.h"
#include "printing.h"

int interpretList(FILE *imapStream); 
int interpretExists(FILE *imapStream, msgCacheT *cachePtr, size_t newSize, int context);
int interpretExpunge(msgCacheT *cachePtr, size_t expungeNum);
int interpretFetch(FILE *imapStream, msgCacheT *cachePtr, size_t msgNum);
void interpretRecent(msgCacheT *cachePtr, size_t recentNum);

//Interprets untagged responses of the form, response := "*" SP <number> <data> CRLF, such as EXISTS, or FETCH
int interpretNumberResponse(FILE *imapStream, msgCacheT *cachePtr, size_t msgNum, int context);


 //Untagged responses are of the form: "*" SP <data> CRLF
int interpretUntagged(FILE *imapStream, msgCacheT *cachePtr, int context) {
	imapObjectHandleT strHandle; //A handle to a string imapObject
	size_t msgNum;
	int retVal; //Used for error checking and propagation of error codes

	//Skip space, so the next object must be a string
	if (isError(retVal = skipSpace(imapStream))) {
		return(retVal);
	}

	retVal = getStringObject(&strHandle, imapStream);
	if (isError(retVal)) {
		return(retVal);
	}
	strUpper(strHandle->content.string); //To uppercase, in case the server uses lowercase
	//If the string is numeric, the response is of the form "*" SP <number> SP <data> CRLF
	if (isNumber(strHandle->content.string)) { 
		msgNum = atoi(strHandle->content.string); //Get the number

		freeImapObject(strHandle); //Free what isn't needed anymore

		retVal = interpretNumberResponse(imapStream, cachePtr, msgNum, context);
		if (isError(retVal)) {
			return(retVal);
		}	
	}
	//If the string is LIST, the response is a LIST response
	else if (!strcmp(strHandle->content.string, "LIST")) {
		freeImapObject(strHandle);

		/*If the untagged response was sent as a response to a LIST command (IN_LIST context),
		  interpret it, else ignore it */
		if (context == IN_LIST) { 
			retVal = interpretList(imapStream);
			if (isError(retVal)) {
				return(retVal);
			}
		}
		else {
			if (isError(retVal = skipLine(imapStream))) { //Skip the rest of the line
				return(retVal);
			}
		}
	}
	//If the response is an untagged NO response 
	else if (!strcmp(strHandle->content.string, "NO")) {
		freeImapObject(strHandle);

		if (isError(retVal = printLine(stderr, imapStream))) { //Print the rest of the line, to alert the user
			return(retVal);
		}
	}
	//If the response is an untagged BAD response 
	else if (!strcmp(strHandle->content.string, "BAD")) {
		 /* Print the line to alert the user, and consider this a COMMMAND_ERROR
		   (according to RFC 1176, untagged BAD responses occur on a fatal server-side error */
		freeImapObject(strHandle);
		printLine(stderr, imapStream);
		return(COMMAND_ERROR);
	}
	else { //If the response is not recognized, it is ignored
		freeImapObject(strHandle);
	}

	if (isError(retVal = skipLine(imapStream))) { //Skip thn rest of the line
		return(retVal);
	}

	return(SUCCESS);
}

int interpretNumberResponse(FILE *imapStream, msgCacheT *cachePtr, size_t msgNum, int context) {
	int retVal;
	imapObjectHandleT strHandle;

	if (isError(retVal = skipSpace(imapStream))) { //Skip space
		return(retVal);
	}

	/* Depending on the value of the string contained in the string object,
	  interpret a different type of response */
	retVal = getStringObject(&strHandle, imapStream);
	if (isError(retVal)) {
		return(retVal);
	}
	strUpper(strHandle->content.string);
	if (!strcmp(strHandle->content.string, "RECENT")) {
		interpretRecent(cachePtr, msgNum);
	}
	else if (!strcmp(strHandle->content.string, "EXPUNGE")) {
		retVal = interpretExpunge(cachePtr, msgNum);
		if (isError(retVal)) {
			freeImapObject(strHandle);
			return(retVal);
		}
	}
	//STORE and FETCH are equivalent when it comes to fetching flags
	else if (!strcmp(strHandle->content.string, "FETCH") || !strcmp(strHandle->content.string, "STORE")) {
		retVal = interpretFetch(imapStream, cachePtr, msgNum);
		if (isError(retVal)) {
			freeImapObject(strHandle);
			return(retVal);
		}
	}
	else if (!strcmp(strHandle->content.string, "EXISTS")) {
		retVal = interpretExists(imapStream, cachePtr, msgNum, context);
		if (isError(retVal)) {
			freeImapObject(strHandle);
			return(retVal);
		}
	}

	freeImapObject(strHandle);

	return(SUCCESS);
}

int interpretList(FILE *imapStream) { 
	//Response of the form: "LIST" SP <attributes> SP <hierarchy-delimiter> SP <mailbox-name> CRLF
	imapObjectHandleT mailboxNameHandle; 
	int retVal;

	if (isError(retVal = skipSpace(imapStream))) { //Skip space
		return(retVal);
	}

	//Skip the attributes
	if (isError(retVal = skipObject(imapStream))) { 
		return(retVal);
	}

	if (isError(retVal = skipSpace(imapStream))) {
		return(retVal);
	}

	//Skip the hierarchy delimiter
	if (isError(retVal = skipObject(imapStream))) {
		return(retVal);
	}

	if (isError(retVal = skipSpace(imapStream))) {
		return(retVal);
	}

	//Get the mailbox name
	retVal = getStringObject(&mailboxNameHandle, imapStream); 
	if (isError(retVal)) {
		return(retVal);
	}

	printf("> %s\n", mailboxNameHandle->content.string); //Print that mailbox-name

	freeImapObject(mailboxNameHandle);

	return(SUCCESS);
}

int interpretExists(FILE *imapStream, msgCacheT *cachePtr, size_t newSize, int context) {
	msgT **msgPtrArray = cachePtr->msgPtrArray;
	size_t oldSize = cachePtr->cacheSize;

	//If the EXISTS select was sent in response to a SELECT command
	if (context == IN_SELECT) {

		//Empty the message pointer array
		if (msgPtrArray != NULL) {
			for (int k = 0 ; k < oldSize ; k++) {
				freeMsgData(msgPtrArray[k]);
			}
			free(msgPtrArray);
		}
		//Mhdenise ta gia na ftiaxei neo array to resize
		cachePtr->msgPtrArray = NULL;
		cachePtr->cacheSize = cachePtr->prevSize = 0;
	}

	//In any case resize it (resizing an empty array is equivalent to allocating)
	if (cacheResize(cachePtr, newSize) < 0) {
		return(MEM_ERROR);
	}

	return(SUCCESS);
}

int interpretExpunge(msgCacheT *cachePtr, size_t expungeNum) {
	//Remove the message with expungeNum from the cache
	if (cacheRemove(cachePtr, expungeNum-1) < 0) {
		return(MEM_ERROR);
	}
	return(SUCCESS);
}

void interpretRecent(msgCacheT *cachePtr, size_t recentNum) {
	cachePtr->recent = recentNum; //Update the recent number stored in cache
}

int getMsgSize(imapObjectHandleT sizeHandle, int *sizePtr) {
	char *sizeStr;
	int retVal;

	retVal = copyStrFromObject(&sizeStr, sizeHandle, NOT_NULLABLE);
	if (isError(retVal)) {
		return(retVal);
	}

	*sizePtr = atoi(sizeStr);
	free(sizeStr);

	return(SUCCESS);
}

int parseEnvelope(imapObjectHandleT envelopeHandle, struct envelope *envPtr) {
	imapObjectHandleT *elemArray = envelopeHandle->content.list.elemArray;
	struct envelope envelope;
	int retVal;

	/*Skip zeroth element (date, not to be confused with internal date),
	 and get decoded subject string */
	retVal = decodedCopyFromObject(&envelope.subject, elemArray[1]);
	if (isError(retVal)) {
		return(retVal);
	}

	//Get From address list
	retVal = getAddressList(&envelope.fromList, elemArray[2]);
	if (isError(retVal)) {
		free(envelope.subject);
		return(retVal);
	}
	//Get To address list
	retVal = getAddressList(&envelope.toList, elemArray[5]);
	if (isError(retVal)) {
		freeAddressList(envelope.fromList);
		free(envelope.subject);
		return(retVal);
	}
	//Get CC address list
	retVal = getAddressList(&envelope.ccList, elemArray[6]);
	if (isError(retVal)) {
		freeAddressList(envelope.toList);
		freeAddressList(envelope.fromList);
		free(envelope.subject);
		return(retVal);
	}

	*envPtr = envelope;

	return(SUCCESS); 
}

//Parse a list of flags, for example (\Deleted \Seen \Recent), and store the flags into an integer 
int parseFlags(imapObjectHandleT flagsHandle) {
	struct imapObject **elemArray = flagsHandle->content.list.elemArray;
	int flags = 0, elems;
	char *flagStr;

	//If the flag list is NIL, don't bother with parsing
	if (flagsHandle->tag == NIL) {
		return(0);
	}

	/* Iterate over the flag list, and store the flags into an integer by ORing
	 (the symbolic constants used are defined in cache.h) */
	elems = flagsHandle->content.list.elems;

	for (int k = 0 ; k < elems ; k++) {
		flagStr = elemArray[k]->content.string;
		strUpper(flagStr); //To uppercase to make a case-insensitive comparison

		//Depending on the flag, OR with a symbolic constant
		if (!strcmp(flagStr, "\\SEEN")) {
			flags |= SEEN;
		}
		else if (!strcmp(flagStr, "\\RECENT")) {
			flags |= RECENT;
		}
		else if (!strcmp(flagStr, "\\DELETED")) {
			flags |= DELETED;
		}
		else if (!strcmp(flagStr, "\\ANSWERED")) {
			flags |= ANSWERED;
		}
		else if (!strcmp(flagStr, "\\FLAGGED")) {
			flags |= FLAGGED;
		}
	}

	return(flags);
}

/* If the position msgNum of the cache is empty (NULL), a msgT struct is allocated,
 and the pointer is inserted at the position, if not empty, the pointer at the position
 is returned */
msgT *initCurrMsg(msgCacheT *cachePtr, size_t msgNum) {
	msgT *currMsg;

	if (!cachePtr->msgPtrArray[msgNum-1]) { 
		/* Because all functions in this program that free a pointer
		 do nothing if it is NULL, and I wanted to have less cleanup 
		 calls at points where something can go wrong in interpretFetch(),
		 I used calloc to initialize the msgT pointer, so that it can
		 be freed with the rest of the cache in main() (main.c), 
		 without issues. For context on this, check the cleanup functions
		 in cache.h and addresses.h */
		currMsg = calloc(1, sizeof(msgT));
		if (!currMsg) {
			return(NULL);
		}
		cacheInsert(cachePtr, currMsg, msgNum-1);
	}
	else {
		currMsg = cachePtr->msgPtrArray[msgNum-1];
	}

	return(currMsg);
}

int interpretFetch(FILE *imapStream, msgCacheT *cachePtr, size_t msgNum) {
	imapObjectHandleT fetchList; //The list of things that FETCH returned

	/* fetchElems, fetchStr and fetchElemArray are used for readability and to
	  limit access to struct fields via pointer */
	int fetchElems;
	imapObjectHandleT *fetchElemArray; 
	char *fetchStr;

	msgT *currMsg;
	int retVal;

	if (isError(retVal = skipSpace(imapStream))) { //Skip a space
		return(retVal);
	}

	//Fill fetchList
	retVal = getListObject(&fetchList, imapStream); 
	if (isError(retVal)) {
		return(retVal);
	}
	fetchElems = fetchList->content.list.elems; 
	fetchElemArray = fetchList->content.list.elemArray;


	//Give currMsg a suitable value, check initCurrMsg() for more
	currMsg = initCurrMsg(cachePtr, msgNum);
	if (!currMsg) {
		freeImapObject(fetchList);
		return(MEM_ERROR);
	}

	/* Interate over the fetch list, and depending on the string encountered
	  fetch the message's text, flags, internal date, size or envelope. After
	  fetching the corresponding data (the next element), skip the string by incrementing the counter
	  k, and the for loop will skip the fetched data by incrementing k as well */

	for (int k = 0 ; k < fetchElems ; k++) {

		/* The string checked at the start of the loop is always
         a string that specifies the type of data to be fetched, e.g. FLAGS,
         so if the list element is not a STRING object, return PARSE_ERROR */
		if (fetchElemArray[k]->tag != STRING) {
			freeImapObject(fetchList);
			return(PARSE_ERROR);
		}
		fetchStr = fetchElemArray[k]->content.string;

		strUpper(fetchStr); //Ta thelw case insensitive
		if (!strcmp(fetchStr, "RFC822.TEXT")) { //Fetch the text
			retVal = copyStrFromObject(&currMsg->text, fetchElemArray[k+1], NOT_NULLABLE);
			if (!currMsg->text) {
				freeImapObject(fetchList);
				return(retVal);
			}
			k++; //Skip the word RFC822.TEXT
		}
		else if (!strcmp(fetchStr, "FLAGS")) { //Fetch flags
			currMsg->flags = parseFlags(fetchElemArray[k+1]);
			k++; //Skip the word FLAGS
		}
		else if (!strcmp(fetchStr, "INTERNALDATE")) { //Fetch date
			retVal = copyStrFromObject(&currMsg->internalDate , fetchElemArray[k+1], NOT_NULLABLE); 
			if (isError(retVal)) {
				freeImapObject(fetchList);
				return(retVal);
			}
			k++; //Skip the word INTERNALDATE
		}
		else if (!strcmp(fetchStr, "RFC822.SIZE")) { //Fetch size
			retVal = getMsgSize(fetchElemArray[k+1], &currMsg->size);
			if (isError(retVal)) {
				freeImapObject(fetchList);
				return(retVal);
			}
			k++; //Skip the word RFC822.SIZE
		}
		else if (!strcmp(fetchStr, "ENVELOPE")) { //Fetch the envelope
			retVal = parseEnvelope(fetchElemArray[k+1], &currMsg->envelope);
			if (isError(retVal)) {
				freeImapObject(fetchList);
				return(retVal);
			}
			k++; //Skip the word ENVELOPE
		}
	}

	freeImapObject(fetchList);

	return(SUCCESS);
}