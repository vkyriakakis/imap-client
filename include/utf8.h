#ifndef UTF8_GUARD

	#define UTF8_GUARD
	
	/* Decodes a Base 64 encoded string to UTF-8 charset only, so
	 the Quoted-Printable encoding and other charsets are not
	 supported. */

	/* Also, your terminal might not support UTF-8, in that case,
	 some very bad things might happen. */

	//Creates a heap-allocated string of decoded Base 64 bytes, returns NULL on failure
	char *decodeUtf8Str(char *string);
	
	//Prints the first <chars> characters of a string (the characters can be UTF-8)
	void printNChars(char *str, int chars);

	//
	int decodedCopyFromObject(char **decodedPtr, imapObjectHandleT imapHandle);
	
	//Returns the length of a UTF-8 string
	int utf8StrLen(char *utf8Str);
#endif
