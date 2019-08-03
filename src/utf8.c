#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"
#include "parsing.h"

//For reading on decoding base 64, check https://en.wikipedia.org/wiki/Base64

#define B64_BATCH 63 //Each base 64 quartet decodes to 3 bytes, so the batch size is divisible by 3 

char b64DigitToSextet(char digit) {

	/* For each base 64 digit, return the matching value (sextet of bits),
	   in order to not write a big switch-case statement, or implement 
	   dictionaries in C, character arithmetic was used */
	if (isupper(digit)) {
		return(digit - 'A');
	}
	else if (islower(digit)) {
		return(digit - 'a' + 26);
	}
	else if (isdigit(digit)) {
		return(52 + digit - '0');
	}
	else if (digit == '+') {
		return(62);
	}
	else if (digit == '/') {
		return(63);
	}

	return(-1); //Not a base 64 digit
}

char *createDecodedB64(char *b64Str, int *posPtr) {
	int pos = *posPtr;
	unsigned char sextet;
	char *decodedStr, *temp;
	int bytes = 0; //Number of bytes in decodedStr

	if (!b64Str) { //If NULL, something went wrong outside
		return(NULL);
	}

	decodedStr = malloc(B64_BATCH); //Allocate buffer using a batch size, to decrease allocations
	if (!decodedStr) {
		return(NULL);
	}

	do {
		//Decoding first byte
		sextet = b64DigitToSextet(b64Str[pos]);
		if (sextet < 0) { //If illegal char
			return(NULL);
		}
		decodedStr[bytes] = (sextet << 2); //First six bits of first byte
		pos++;
		sextet = b64DigitToSextet(b64Str[pos]); //Next sextet
		if (sextet < 0) { //If illegal char
			return(NULL);
		}
		decodedStr[bytes] |= (sextet >> 4); //Next two bits
		bytes++;

		//Second byte
		decodedStr[bytes] = (sextet << 4); //First four bits
		pos++;	

		//If the pad character '=' is encountered, only one bytes is to be decoded
		if (b64Str[pos] == '=') { 
			pos += 2; //Pass the second padding char as well
			break;
		}
		sextet = b64DigitToSextet(b64Str[pos]);
		if (sextet < 0) {
			return(NULL);
		}
		decodedStr[bytes] |= (sextet >> 2); //Next four bits
		bytes++;

		//third byte
		decodedStr[bytes] = (sextet << 6); //First two bits
		pos++;
		if (b64Str[pos] == '=') { //if padding is encountered, only two bytes are to be decoded
			pos++;
			break;
		}
		sextet = b64DigitToSextet(b64Str[pos]);
		if (sextet < 0) { //illegal char
			return(NULL);
		}
		decodedStr[bytes] |= sextet; //Last six bits
		bytes++;
		pos++;	//Pass fourth char

		if (bytes % B64_BATCH == 0) { //Resize using batch size, to reduce reallocs
			temp = realloc(decodedStr, bytes+B64_BATCH);
			if (!temp) {
				free(decodedStr);
				return(NULL);
			}
			decodedStr = temp;
		}

		//If either is encountered, the MIME encoded-word is over
		if (b64Str[pos] == '\0' || b64Str[pos] == '?') {
			break;
		}
	} while(1);

	//Resize to final size
	temp = realloc(decodedStr, bytes+1);
	if (!temp) {
		free(decodedStr);
		return(NULL);
	}
	decodedStr = temp;
	decodedStr[bytes] = '\0'; //Terminate string

	*posPtr = pos; //Return new position outside

	return(decodedStr);
}


/* Checks if the charset of encoded-word matches the one supplied as an arguement.
   Is case-insensitive, as RFC 2045 does not imply otherwise */
int matchCharset(char *string, int *posPtr, char *charsetStr, int charsetLen) {
	int strPos = *posPtr, subPos = 0;

	//While both strings are not voer
	for (strPos = *posPtr ; string[strPos] != '\0' && subPos < charsetLen ; strPos++, subPos++) {
		if (tolower(string[strPos]) != charsetStr[subPos]) {
			break;
		}
	}
	/* If it stopped before all of the charsetStr's length being checked,
	 or the charset name wasn't finished, it didn't match */
	if (subPos < charsetLen || string[strPos] != '?') { //Den htan idio akribws
		return(0); 
	}

	*posPtr = strPos; //Return new position outside

	return(1);
}

//Concatenate two dynamically allocated strings, producing another dynamically allocated string
char *allocStrcat(char *str1, char *str2) {
	char *catStr;

	if (!str1) { //Degenerates to strdup() if the first string is NULL
		catStr = strdup(str2);
		if (!catStr) {
			return(NULL);
		}
	}
	else {
		catStr = realloc(str1, strlen(str1) + strlen(str2) + 1); //+1 for '/0'
		if (!catStr) {
			return(NULL);
		}

		strcat(catStr, str2); //Concatenate the strings
	}

	return(catStr);
}

//Appends a character to a dynamically allocated string
char *appendChar(char *str, char c) {
	char *temp;

	if (!str) {
		str = calloc(2, 1); 
		if (!str) {
			return(NULL);
		}
		str[0] = c; //Create an one-char stirng
	}
	else {
		int len = strlen(str);

		temp = realloc(str, len+1+1); //Resize the string
		if (!temp) {
			return(NULL);
		}
		str = temp;

		str[len] = c; //Append the char
		str[len+1] = '\0'; //Terminate the new string
	}

	return(str);
}

char *decodeUtf8Str(char *encodedStr) {
	int pos = 0;
	char *decodedStr = NULL, *temp, *retStr;

	do {
		if (encodedStr[pos] == '=' && encodedStr[pos+1] == '?') {
			pos += 2; //Skip the "=?"
			if (!matchCharset(encodedStr, &pos, "utf-8", 5)) {
				//Inform the user that the charset is not supported (only utf-8 is)
				temp = allocStrcat(decodedStr, "??Unknown charset??"); 
				if (!temp) {
					free(decodedStr);
					return(NULL);
				}
				decodedStr = temp;
				break;
			}
			pos++; //Skip the '?'
			if (encodedStr[pos] == 'b' || encodedStr[pos] == 'B') {
				pos += 2; //Skip "b?"

				//Create the base 64 decoded string
				retStr = createDecodedB64(encodedStr, &pos); 
				if (!retStr) {
					free(decodedStr);
					return(NULL);
				}

				//Concatenate the utf-8 string with the new part
				temp = allocStrcat(decodedStr, retStr);
				if (!temp) {
					free(decodedStr);
					free(retStr);
					return(NULL);
				}
				free(retStr); //Free what is not used anymore
				decodedStr = temp;
			}
			else {
				//Inform the user that the encoding is not supported (only BASE 64 is)
				temp = allocStrcat(decodedStr, "??Unknown encoding??"); 
				if (!temp) {
					free(decodedStr);
					return(NULL);
				}
				decodedStr = temp;
				break;
			}
			if (encodedStr[pos] != '?' || encodedStr[pos+1] != '=') {
				//Inform the user that the string the server sent was malformed
				temp = allocStrcat(decodedStr, "?? not ended with ?= ??"); 
				if (!temp) {
					free(decodedStr);
					return(NULL);
				}
				decodedStr = temp;
				break;
			}
			pos += 2; //Pass the "?="
			/* Skip space, as it just seperates the mime encoded lines 
			   (server sends non-ascii strings in lines of 75 b64 digits) seperated by space.
			   Actual ' ' is encoded in Base 64 anyway. */
			if (encodedStr[pos] == ' ') { 
				pos++;
			}
		} 
		else { //If an ASCII char, append it to string
			temp = appendChar(decodedStr, encodedStr[pos]); 
			if (!temp) {
				free(decodedStr);
				return(NULL);
			}
			decodedStr = temp;
			pos++; 
		}
	} while(encodedStr[pos] != '\0'); //While the end of the string has not been reached

	return(decodedStr);
}

void printNChars(char *str, int chars) {
	int pos, printedChars;
	int k;

	for (pos = 0, printedChars = 0 ; str[pos] != '\0' && printedChars < chars ; pos++) {
		
		//For each byte, check the first bit

		//If it is 0, the character is ASCII, so one byte long
		if (!(str[pos] & 128)) {
			putchar(str[pos]);
			printedChars++;
		} 
		else if (str[pos] & 64) {
			if (str[pos] & 32) {
				if (str[pos] & 16) {
					//If the first four bits are set, the char is four bytes long
					for (k = 0 ; k < 3 ; k++) {
						putchar(str[pos]);
						pos++;
					}
					putchar(str[pos]);
				}
				else { //Only the first three bits are set, so the char is three bytes long
					for (k = 0 ; k < 2 ; k++) {
						putchar(str[pos]);
						pos++;
					}
					putchar(str[pos]);
				}
			}
			else { //Only the first two are set, so the char is two bytes long
				putchar(str[pos]);
				pos++;
				putchar(str[pos]);
			}
			printedChars++;
		}
		/* I do not check the case of the server sending a continuation byte without sending a coutning byte,
		 which is not a valid UTF-8 sequence, because I assume that server implementations are sane. */
	}
}

int utf8StrLen(char *str) {
	int pos, len;
	int k;

	for (pos = 0, len = 0 ; str[pos] != '\0' ; pos++) {
		
		//For each byte, check the first bit

		//If it is 0, the character is ASCII, so one byte long
		if (!(str[pos] & 128)) {
			len++;
		} 
		else if (str[pos] & 64) {
			if (str[pos] & 32) {
				if (str[pos] & 16) {
					//If the first four bits are set, the char is four bytes long
					for (k = 0 ; k < 3 ; k++) {
						pos++;
					}
				}
				else { //Only the first three bits are set, so the char is three bytes long
					for (k = 0 ; k < 2 ; k++) {
						pos++;
					}
				}
			}
			else { //Only the first two are set, so the char is two bytes long
				pos++;
			}
			len++;
		}
		/* I do not check the case of the server sending a byte with only one MSB bit set,
		 which is not a valid UTF-8, because I assume that the server implementations are sane. */
	}

	return(len);
}

int decodedCopyFromObject(char **decodedPtr, imapObjectHandleT imapHandle) {
	char *temp, *str;
	int retVal;

	retVal = copyStrFromObject(&str, imapHandle, NULLABLE);
	if (isError(retVal)) {
		return(retVal);
	}

	if (!str) {
		*decodedPtr = NULL;
		return(SUCCESS); //No decoding needed, because the object was NIL
	}

	//Try to decode the string
	temp = decodeUtf8Str(str);
	free(str); //Not needed anymore
	if (!temp) {
		return(MEM_ERROR);
	}

	*decodedPtr = temp; //Return the decoded string
	
	return(SUCCESS); 
}
