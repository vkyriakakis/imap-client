#ifndef PARSER_GUARD
	#define PARSER_GUARD

	#define NULLABLE 1 /* For copyStrFromObject, if supplied, 
                              NULL can be returned (to denote an empty string field) */
	#define NOT_NULLABLE 0 //Supplied in place of the above, when NULL is not wanted
	

	/* Responses and commands in IMAP mainly consist of
         atoms, strings, NIL (a value to denote emptiness), and lists of all the above, 
	 or other lists, in the form (a b c d e f),  SP (space) and CRLF ("\r\n" used to end lines).
          In order to facilitate code reuse, a polymorphic "IMAP object" type, which represent
         any of the above types of data was needed. To that end, I used a tagged union. */

	//For an explanation of the various types of IMAP data check RFC 1176
	
	//The tag of the tagged union
	enum tag {NIL, STRING, LIST, CRLF, SP};
	
	/* The imapObject struct, when:
	   > The tag is NIL, CRLF or SP, it contains nothing, as the 
            presence of the tag alone is enough to pass the intended meaning.
           > When the tag is STRING, a dynamically allocated string is contained.
           > When the tag is LIST, a dynamically allocated array of imapObjects
             is the content (lists are naturally recursive after all) */
	struct imapObject {
		enum tag tag;
		union content {
			char *string;
			struct array {
				struct imapObject **elemArray;
				int elems;
			} list;
		} content;
	};
	
	//typedefs to improve code readability
	typedef struct imapObject imapObjectT;
	typedef struct imapObject* imapObjectHandleT;
	
	//Free an imap object through its handle
	void freeImapObject(imapObjectHandleT imapHandle);

	/* Access an imapObject, and if its tag is STRING, (or NIL if attribute == NULLABLE),
          dynamically allocate a copy of its content and return it */
	int copyStrFromObject(char **strPtr, imapObjectHandleT strHandle, int attribute);

	/* Parse the IMAP data sent by the server, and if the resulting object is a STRING,
          return a handle to it */
	int getStringObject(imapObjectHandleT *strHandlePtr, FILE *imapStream);

	//Same as getStringObject() but with LIST-tagged objects instead
	int getListObject(imapObjectHandleT *imapHandlePtr, FILE *imapStream);
	
	/* The reason both skipLine and skipSpace exist, is that in certain cases,
          an error might be reported if an SP was missing, whereas skipObject() does
          not check the tag of the skipped object */

	//Parse IMAP data, until a CRLF object is created
	int skipLine(FILE *imapStream);

	//Skip the next imap object if it is an SP, else return a PARSE_ERROR (defined in error.h)
	int skipSpace(FILE *imapStream);

	/*Print the data of all the objects that result from the 
          server data parsing, until a CRLF */
	int printLine(FILE *printStream, FILE *imapStream);

 	//Skip the next imap object, regardless of its tag
	int skipObject(FILE *imapStream);
#endif
