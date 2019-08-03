#ifndef PRINT_GUARD

	#define PRINT_GUARD

	/* Note that the msgNum arguement all these functions take, is the number
	 that the user sees, so the first message in the cache array has msgNum == 1, and not
	 0, its position in the array */ 

	/* About pages:
	 The messages of the mailbox are split into totalMessages / PAGE_MSGS + 1
         (PAGE_MSGS is defined in printing.c), so that their previews fit in a terminal
         window, and the user is able to display older or more recent messages. 
         The function displayMsgPage() can print messages by page, and by reading the 
         previews, the user can use the matching msgNum in commands such as !read or
         !delete (see main.c for those) */
	
	//Clear the screen (like the clear unix command)
	void clearScreen(void);

	/* Print information about the current mailbox (at the moment,
          the number of existing messages, the number of recent messages,
          and the number of pages (as used by displayMsgPage) */
	void printStat(msgCacheT *cachePtr);

	//Print a list of short descriptions, one for each user command
	void printHelp(void);
	
	//Display the contents of a message (subject, date, From, To, CC, text)
	int displayMsg(FILE *imapStream, msgCacheT *cachePtr, int msgNum);
	
	/* Display a preview (in the format <msg-number> <subject> <from> <date> <size>),
          if a field doesn't fit, only part of it is printed */
	void displayMsgPage(FILE *imapStream, msgCacheT *cachePtr, size_t pageNum);
#endif
