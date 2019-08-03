#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "parsing.h"
#include "error.h"

/* Used when colling the parsing functions, as parentheses in atoms 
 are parsed differently depending on if the atom was standalone or inside a list */
#define IN_LIST 1 
#define NO_PARSE_CONTEXT 0 

//Used to decrease the number of allocations
#define BATCH 32
#define LIST_BATCH 32

//Frees an array of handles to imap objects
void freeElemArray(imapObjectHandleT *elemArray, int elems);

//Parse data, until an imapObject is formed and returned
int createImapObject(imapObjectHandleT *handlePtr, FILE *stream, char context);

//Print the contents of an imap object depending on its tag
void printImapObject(FILE *printStream, imapObjectHandleT imapHandle);

//"Fills" an atom object with data (a string, or NIL)
int getAtom(imapObjectHandleT atomHandle, FILE *stream, char context);

//Parses a list of elements, and returns a LIST object through the supplied handle (or NIL if the list is empty)
int parseList(imapObjectHandleT listHandle, FILE *stream);

/* Returns a STRING object (which may be a literal or a quoted string), or NIL. */
int parseString(imapObjectHandleT strHandle, FILE *stream, char prevChar);

//Parses an atom, and returns a string
int parseAtom(char **strPtr, FILE *stream, char context);

//Parses a quoted string, and returns a string (if NULL, it is the empty string "")
int parseQuoted(char **strPtr, FILE *stream);

//Parses a literal string, and returns a string, or NIL (if it is the empty literal {0}\r\n)
int parseLiteral(char **strPtr, FILE *stream);

//Allocate and initialize an imapHandle
int imapHandleInit(imapObjectHandleT *handlePtr);

/* EOF is checked in every place a character is read from a stream, as
  parsing an incomplete string due to the server disconnecting is an error */


/* Reads a character from the input stream, and returns it, without consuming it,
 so it is used instead of fgetc() when a character must be checked multiple
 times, or checked and then consumed. */

char peekChar(FILE *stream) {
	char c;

	c = fgetc(stream);
	if (c == EOF || (ungetc(c, stream) == EOF)) {
		return(EOF);
	}

	return(c);
}

//Improves readability
int match(char c, char target) {
	return(c == target);
}

void freeImapObject(imapObjectHandleT imapHandle) {
	enum tag tag  = imapHandle->tag;

	if (tag == NIL || tag == SP || tag == CRLF) { //free nothing but the handle itself
		free(imapHandle);
		return;
	}
	else if (tag == STRING) { //If the object is of tag STRING, free the string inside first
		free(imapHandle->content.string);
		free(imapHandle);
		return;
	}

	//Else the tag is LIST, so free all of the array elements
	freeElemArray(imapHandle->content.list.elemArray, imapHandle->content.list.elems);
	free(imapHandle);
}

//Freeing the array elements of a LIST object is done recursively
void freeElemArray(imapObjectHandleT *elemArray, int elems) {
	for (int k = 0 ; k < elems ; k++) {
		freeImapObject(elemArray[k]);
	}
	free(elemArray);
}

int imapHandleInit(imapObjectHandleT *handlePtr) {
	imapObjectHandleT imapHandle;

	imapHandle = malloc(sizeof(imapObjectT));
	if (!imapHandle) {
		return(MEM_ERROR);
	}
	imapHandle->tag = NIL; //So that in case of error, it gets freed without issue

	*handlePtr = imapHandle;

	return(SUCCESS);
}

int createImapObject(imapObjectHandleT *handlePtr, FILE *stream, char context) {
	char c;
	imapObjectHandleT imapHandle;
	int retVal;

	//Allocate the space for an imapObject, and get its handle
	retVal = imapHandleInit(&imapHandle);
	if (isError(retVal)) {
		return(retVal);
	}

	c = peekChar(stream);
	if (c == EOF) { //If end of file was reached, the connection was closed
		freeImapObject(imapHandle);
		return(SOCKET_ERROR);
	}
	//If the first character is a left parenthesis, the object is a list
	else if (match(c, '(')) { 
		if (fgetc(stream) == EOF) {
			free(imapHandle);
			return(SOCKET_ERROR);
		}
		retVal = parseList(imapHandle, stream);
		if (isError(retVal)) {
			free(imapHandle);
			return(retVal);
		}
	}
	/* Else if it is a double quote, or a left bracket the 
	   object is a string (quoted or literal respectively) */
	else if (match(c, '{') || match(c, '"')) {
		if (fgetc(stream) == EOF) {
			free(imapHandle);
			return(SOCKET_ERROR);
		}
		retVal = parseString(imapHandle, stream, c);
		if (isError(retVal)) {
			free(imapHandle);
			return(retVal);
		}
	}
	//If the character is ' ' then the object is a whitespace (SP)
	else if (match(c, ' ')) {
		c = fgetc(stream);
		if (c == EOF) {
			free(imapHandle);
			return(SOCKET_ERROR);
		}
		imapHandle->tag = SP;
	}
	//Else if it is a carriage return character ('\r'), it should be a CRLF
	else if (match(c, '\r')) {
		if (fgetc(stream) == EOF) {
			free(imapHandle);
			return(SOCKET_ERROR);
		}
		c = fgetc(stream);
		if (c == EOF) {
			free(imapHandle);
			return(SOCKET_ERROR);
		}
		if (match(c, '\n')) {
			imapHandle->tag = CRLF;
		}
		else { //Sketo '\r' den noeitai
			free(imapHandle);
			return(PARSE_ERROR);
		}
	}
	//Else it must be an atom (NIL or STRING)
	else {
		retVal = getAtom(imapHandle, stream, context);
		if (isError(retVal)) {
			free(imapHandle);
			return(retVal);
		}
	}

	*handlePtr = imapHandle; //Return the handle via pointer

	return(SUCCESS);
}

int parseList(imapObjectHandleT listHandle, FILE *stream) {
	int elems = 0, retVal;
	imapObjectHandleT *elemArray = NULL, *temp;
	char c;

	c = peekChar(stream);
	if (c == EOF) {
		return(SOCKET_ERROR);
	}
	//While a right parenthesis hasn't been reached, continue filling the list object with elements
	while(!match(c, ')')) {
		/* When the number of elements is a multiple of the BATCH, the array must be resized (by LIST_BATCH),
		  this is done in order to not resize the array for every new element added */
		if (elems % LIST_BATCH == 0) {
			temp = realloc(elemArray, (elems + LIST_BATCH)*sizeof(imapObjectHandleT));
			if (!temp) { 
				freeElemArray(elemArray, elems);
				return(MEM_ERROR);
			}
			elemArray = temp;
		}

		/* Get the next element (imapObject), the IN_LIST context is used, 
		  in order to differentiate between an atom containing parentheses, and
		  an atom which is the last element of a list */
		retVal = createImapObject(&elemArray[elems], stream, IN_LIST);
		if (isError(retVal)) {
			freeElemArray(elemArray, elems);
			return(retVal);
		}
		elems++;

		c = peekChar(stream); 
		if (c == EOF) {
			freeElemArray(elemArray, elems);
			return(SOCKET_ERROR);
		}
		if (match(c, ' ')) { //If the character is a whitespace, skip it (it delimits the list elements)
			c = fgetc(stream); 
			if (c == EOF) {
				freeElemArray(elemArray, elems);
				return(SOCKET_ERROR);
			}
		}
	}
	if (!elems) { //If the list was empty, it is a NIL-tagged object
		listHandle->tag = NIL;
	}
	else { //Else it is a LIST-tagged object
		listHandle->tag = LIST;

		//Do a final resize, in order to fit the number of elements exactly
		temp = realloc(elemArray, elems*sizeof(imapObjectHandleT));
		if (!temp) {
			freeElemArray(elemArray, elems);
			return(MEM_ERROR);
		}
		listHandle->content.list.elemArray = temp;
		listHandle->content.list.elems = elems;
	}

	fgetc(stream); //Consume the closing parenthesis

	return(SUCCESS);
}

/* prevChar is used in order to call peekChar() one less time,
  in the future, parseLiteral and parseQuoted might be called directly
  from createImapObject() */

int parseString(imapObjectHandleT strHandle, FILE *stream, char prevChar) {
	char *str;
	int retVal;

	if (match(prevChar, '"')) { //If the first character was a quote, it is a quoted string
		retVal = parseQuoted(&str, stream);
		if (isError(retVal)) {
			return(retVal);
		}
	}
	else { //If the first character was a left bracket, it is a literal string
		retVal = parseLiteral(&str, stream);
		if (isError(retVal)) {
			return(retVal);
		}
	}

	if (str == NULL) { //If the returned string is NULL, the parsed object is NIL-tagged
		strHandle->tag = NIL;
		return(SUCCESS);
	}

	//Else it is STRING-tagged
	strHandle->tag = STRING;
	strHandle->content.string = str;

	return(SUCCESS);
}

int getAtom(imapObjectHandleT atomHandle, FILE *stream, char context) {
	char *str;
	int retVal;

	//Try to parse the server data as an atom object
	retVal = parseAtom(&str, stream, context);
	if (isError(retVal)){
		return(retVal);
	}
	else if (!strcmp(str, "NIL")) { //The atom NIL corresponds to a NIL-tagged object
		free(str);
		atomHandle->tag = NIL;
	}
	else { //Any other atom, corresponds to a plain old string
		atomHandle->tag = STRING;
		atomHandle->content.string = str;
	}

	return(SUCCESS);
}

void printImapObject(FILE *printStream, imapObjectHandleT imapHandle) {

	//Print the object data in a different format depending on the tag
	switch(imapHandle->tag) {
		case NIL:	
			fprintf(printStream, "NIL");
			break;
		case STRING:
			fprintf(printStream, "%s", imapHandle->content.string);
			break;
		case LIST:
			fputc('(', printStream);
			for (int k = 0 ; k < imapHandle->content.list.elems ; k++) {
				if (k != 0) {
					fputc(' ', printStream);
				}
				//When printing a list (a recursive data structure), recursion is needed
				printImapObject(printStream, imapHandle->content.list.elemArray[k]);
			}
			fputc(')', printStream);
		default:
			break;
	}
}

//According to RFC 1176, those characters are not permitted to appear inside a quoted string or atom
int isIllegal(char c) {
	switch(c) {
		case '{':
		case '"':
		case '\r':
		case '\n':
		case '%':
			return(1);
		default:
			break;
	}
	return(0);
}

//Atoi() on FILE streams
int getNum(FILE *stream) {
	int num = 0;
	char c;

	do {
		c = peekChar(stream);
		if (c == EOF) {
			return(SOCKET_ERROR);
		}
		else if (!isdigit(c)) {
			break;
		}
		num = num*10 + c - '0';
		fgetc(stream); //Den to mplokarw afou xerw oti yparxei, kai den koitaw gia sfalma, afou den to brhka prin
	} while (1);
	
	return(num);
}

int parseLiteral(char **strPtr, FILE *stream) {
	char *result;
	int litSize, k;
	char c;

	/* A literal string is in the format {<octets>} CRLF <content>, if any of that is
	 missing, like a bracket, a PARSE_ERROR is returned */

	litSize = getNum(stream); //Get the octet count
	if (isError(litSize)) {
		return(litSize);
	}

	//No checking for EOF is done, as no errors occured during the peek in createImapObject() 
	c = fgetc(stream); 
	if (!match(c, '}')) {
		return(PARSE_ERROR);
	}
	c = fgetc(stream); 
	if (c == EOF) {
		return(SOCKET_ERROR);
	}
	if (!match(c, '\r')) {
		return(PARSE_ERROR);
	}
	c = fgetc(stream);
	if (c == EOF) {
		return(SOCKET_ERROR);
	}
	if (!match(c, '\n')) {
		return(PARSE_ERROR);
	}

	if (!litSize) { //{0}\r\n is the empty literal, so NULL is returned
		*strPtr = NULL;
		return(SUCCESS);
	}

	//Allocate enough memory to fit all the octets and '\0'
	result = malloc(litSize+1);
	if (!result) {
		return(MEM_ERROR);
	}

	//Fill string with litSize characters
	for (k = 0 ; k < litSize ; k++) {
		c = fgetc(stream);
		if (c == EOF) {
			free(result);
			return(SOCKET_ERROR);
		}
		result[k] = c;
	}
	result[k] = '\0'; //Terminate the result string

	*strPtr = result; //Return string via pointer

	return(SUCCESS);
}

int parseQuoted(char **strPtr, FILE *stream) {
	int strSize = 0;
	char *result = NULL, *temp;
	char c;

	//Fill result with characters until '"' is found again
	do {
		c = fgetc(stream);
		if (c == EOF) {
			free(result);
			return(SOCKET_ERROR);
		}
		if (match(c, '"')) { 
			break;
		}
		//The presence of an illegal character (RFC 1176) in a quoted string is considered a parsing error
		else if (isIllegal(c)) { 
			free(result);
			return(PARSE_ERROR);
		}
		if (strSize % BATCH == 0) { 
			//If the number of characters is multiple of the BATCH size, resize by the BATCH size
			temp = realloc(result, strSize+BATCH);
			if (!temp) {
				free(result);
				return(MEM_ERROR);
			}
			result = temp;
		}
		
		result[strSize] = c;
		strSize++;
	} while(1);

	if (!strSize) {
		//An empty quoted string ("") is equivalent to NIL
		*strPtr = NULL;
		return(SUCCESS);
	}

	//Resize the string to its final size (fits the characters exactly)
	temp = realloc(result, strSize+1); 
	if (!temp) {
		free(result);
		return(MEM_ERROR);
	}
	result = temp;
	result[strSize] = '\0'; //Terminate the string

	*strPtr = result;

	return(SUCCESS);
}


int parseAtom(char **strPtr, FILE *stream, char context) {
	int strSize = 0;
	char *result = NULL, *temp;
	char c;

	//Fill the atom string with characters, until the loop stops
	do {
		c = peekChar(stream);
		if (c == EOF) {
			free(result);
			return(SOCKET_ERROR);
		}

		/* According to RFC 1176, atoms are delimited by SP or CRLF,
		  so an atom is considered to have ended after encountering ' ' or '\r'.
		  They are not consumed, in order for SP and CRLF will be recognized as objects by
		  createImapObject() on the next call (this is useful, for example in skipLine(),
		  to skip all objects until CRLF, which would not have been possible if CRLF itself
		  wasn't an object */
		if (match(c, ' ') || match(c, '\r')) {
			break;
		}
		/* If a right parenthesis is found, if the atom was inside a list (so context == IN_LIST),
		 the parenthesis signifies the end of the list, so it is not consumed (so that parseList() detects it, 
		 and the loop is ended, else the parenthesis is added to the atom as normal */

		else if (match(c, ')')) {
			if (context == IN_LIST) {
				break;
			}
		}
		//The presence of an illegal character (RFC 1176) in a quoted string is considered a parsing error
		else if (isIllegal(c)) {
			free(result);
			return(PARSE_ERROR);
		}
		//If the number of characters is multiple of the BATCH size, resize by the BATCH size
		if (strSize % BATCH == 0) {
			temp = realloc(result, strSize+BATCH);
			if (!temp) {
				free(result);
				return(MEM_ERROR);
			}
			result = temp;
		}
		
		result[strSize] = c;
		strSize++;

		if (fgetc(stream) == EOF) {
			free(result);
			return(SOCKET_ERROR);
		}
	} while(1);

	//Resize string to the exact number of characters+1 (for '\0')
	temp = realloc(result, strSize+1);
	if (!temp) {
		free(result);
		return(MEM_ERROR);
	}
	result = temp;
	result[strSize] = '\0'; //Terminate the string

	*strPtr = result;

	return(SUCCESS);
}

int copyStrFromObject(char **strPtr, imapObjectHandleT strHandle, int attribute) {
	char *str;

	/* If the tag is NIL, and the attribute NULLABLE, return NULL to
	 signify the absence of a string, else if the attribute is NOT_NULLABLE,
	 consider the presence of NIL a parse error */
	if (strHandle->tag == NIL) {
		if (attribute == NULLABLE) {
			*strPtr = NULL;
			return(SUCCESS);
		}
		return(PARSE_ERROR);
	}
	//If not NIL or STRING 
	else if (strHandle->tag != STRING) {
		return(PARSE_ERROR);
	}

	//Duplicate the string contained in the object accessed through strHandle
	str = strdup(strHandle->content.string);
	if (!str) {
		return(MEM_ERROR);
	}

	*strPtr = str;

	return(SUCCESS);
}

int getStringObject(imapObjectHandleT *imapHandlePtr, FILE *imapStream) {
	imapObjectHandleT imapHandle;
	int retVal;

	//If the object is not STRING-tagged, return PARSE_ERROR
	retVal = createImapObject(&imapHandle, imapStream, NO_PARSE_CONTEXT);
	if (isError(retVal)) {
		return(retVal);
	}
	else if (imapHandle->tag != STRING) {
		freeImapObject(imapHandle);
		return(PARSE_ERROR);
	}

	*imapHandlePtr = imapHandle;

	return(SUCCESS);
}

int getListObject(imapObjectHandleT *imapHandlePtr, FILE *imapStream) {
	imapObjectHandleT imapHandle;
	int retVal;

	//If the object is not LIST-tagged or NIL-tagged (empty list), return PARSE_ERROR
	retVal = createImapObject(&imapHandle, imapStream, NO_PARSE_CONTEXT);
	if (isError(retVal)) {
		return(retVal);
	}
	else if (imapHandle->tag != LIST && imapHandle->tag != NIL) {
		freeImapObject(imapHandle);
		return(PARSE_ERROR);
	}

	*imapHandlePtr = imapHandle;

	return(SUCCESS);
}

int skipLine(FILE *imapStream) {
	imapObjectHandleT imapHandle;
	int retVal;
	enum tag objTag;

	//Create and free objects until a CRLF 
	do {
		retVal = createImapObject(&imapHandle, imapStream, NO_PARSE_CONTEXT);
		if (isError(retVal)) {
			return(retVal);
		}
		objTag = imapHandle->tag;
		freeImapObject(imapHandle);
	} while(objTag != CRLF);

	return(SUCCESS);
}

int skipSpace(FILE *imapStream) {
	imapObjectHandleT imapHandle;
	int retVal;

	//If the object is SP skip it, else it is a parse error
	retVal = createImapObject(&imapHandle, imapStream, NO_PARSE_CONTEXT);
	if (isError(retVal)){
		return(retVal);
	}
	else if (imapHandle->tag == SP) {
		freeImapObject(imapHandle);
		return(SUCCESS);
	}
	freeImapObject(imapHandle);

	return(PARSE_ERROR); //The object wasn't SP-tagged, so parse error
}

int skipObject(FILE *imapStream) {
	imapObjectHandleT imapHandle;
	int retVal;

	retVal = createImapObject(&imapHandle, imapStream, NO_PARSE_CONTEXT);
	if (isError(retVal)){
		return(retVal);
	}
	freeImapObject(imapHandle);

	return(SUCCESS);
}

int printLine(FILE *printStream, FILE *imapStream) {
	imapObjectHandleT imapHandle;
	int retVal;
	enum tag objTag;

	//Create, print and free objects until CRLF
	do {
		retVal = createImapObject(&imapHandle, imapStream, NO_PARSE_CONTEXT);
		if (isError(retVal)) {
			return(retVal);
		}
		objTag = imapHandle->tag;
		if (objTag == CRLF) {
			freeImapObject(imapHandle);
			break;
		}
		if (objTag != SP) {
			printImapObject(printStream, imapHandle);
			fputc(' ', printStream);
		}
		freeImapObject(imapHandle);
	} while(1);

	fputc('\n', printStream);

	return(SUCCESS);
}