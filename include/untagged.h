#ifndef UNTAGGED_GUARD
	
	#define UNTAGGED_GUARD

	//Contexts (depending on the context, some responses are treated differently
	
	//Responses are to be treated normally
	#define NO_CONTEXT 0 
	//Is applied when a SELECT command is sent, affects the treatment of EXISTS responses
	#define IN_SELECT 1 
	/*Is applied when a LIST command is sent, enables the handling of LIST responses 
	  (they are ignored in any other case) */
	#define IN_LIST 2
	
	
	/* Interprets an untagged response, and does something depending 
         on the response and context. */
	int interpretUntagged(FILE *imapStream, msgCacheT *cachePtr, int context);
#endif
