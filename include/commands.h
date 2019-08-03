#ifndef COMMAND_GUARD
	
	#define COMMAND_GUARD

	#define SEND_AGAIN 1 /*If the command needs to be resent (e.g. SELECT,
                               a mailbox should be selected at all times */
	#define QUIT 1 //Special return value in case the user decides to exit the program
	
	//Request the server to fetch the text part of the message with message number <msgNum>
	int sendFetchText(FILE *imapStream, msgCacheT *cachePtr, size_t msgNum);

	/* Request the server to fetch the flags, size (in octets), internal date 
	 and envelope of the messages numbered in the range [startNum, endNum],
         if startNum == endNum, data for that single message is fetched */
	int sendFetchAll(FILE *imapStream, msgCacheT *cachePtr, size_t startNum, size_t endNum);

	//Send a SELECT command to the server (in order to select the mailbox with the given name)
	int sendSelect(FILE *imapStream, msgCacheT *cachePtr, char *mailboxName);

	/*Send a LIST command to the server (in order to list all mailboxes in the main 
	  directory (not recursively) */
	int listMailboxNames(FILE *imapStream, msgCacheT *cachePtr);

	/* Send a NOOP to the server, to trigger the sending of untagged responses, and reset any autologout 		  timer the server might use */
	int sendNoop(FILE *imapStream, msgCacheT *cachePtr);

	//Send a STORE command to the server, in order to flag a single message for deletion
	int deleteMsg(FILE *imapStream, msgCacheT *cachePtr, int msgNum);

	//Send a STORE command in order to undelete a single message
	int undeleteMsg(FILE *imapStream, msgCacheT *cachePtr, int msgNum);

	//Send an EXPUNGE command in order to purge all deleted messages
	int sendExpunge(FILE *imapStream, msgCacheT *cachePtr);

	//Send a LOGIN command in order to login to the server
	int sendLogin(FILE *imapStream, char *username, char *password);

	//Send a LOGOUT command to terminate the IMAP sesssion
	int logout(FILE *imapStream);
#endif
