#ifndef CACHE_GUARD

	#define CACHE_GUARD

	/* Message data is cached by this application, in order to 
          minimize server requests, speeding up most of the functionality.
          The cache is implemented as a dynamically allocated array of
          pointers to message data. 
           If NULL is assigned to a position, it
          is considered empty, so it should be filled by a pointer to
          dynamically allocated message data.
           Also the current size of the cache, and its size before an
          expansion are stored to help determining when new data needs to be fetched. */
	
	/* The standard flags a message can have, they are powers of 2, 
         in order to be able to be stored in a single variable by ORing */
	#define SEEN 16
	#define RECENT 1
	#define ANSWERED 2
	#define DELETED 4
	#define FLAGGED 8
	
	//A struct containing all the useful data of a message
	typedef struct {
		//The date of the message's arrival to the IMAP server
		char *internalDate;
		//The size of the message in octets
		int size; 
		int flags;
		//Only the needed parts of the envelope are saved
		struct envelope {
			char *subject; 
			addressNodeT fromList; 
			addressNodeT toList;
			addressNodeT ccList;
		} envelope;
		char *text; //Only fetched when needed, so when the text is to be printed
	} msgT;

	typedef struct {
		msgT **msgPtrArray; //An array of pointers to msgT
		size_t cacheSize; //The number of messages (and size of the array)
		/* After resizing the cache, the previous size is stored in prevSize, this is useful,
                 as it enables the fetching of data for the new messages, after the resize,
                 outside the resizing logic */
		size_t prevSize; 
		size_t recent; //The number of recent messages
	} msgCacheT;

	//Initialize a pointer to msgCacheT
	msgCacheT *cacheInit(void); 
	//Insert a message pointer at position pos of the cache's message pointer array
	int cacheInsert(msgCacheT *cachePtr, msgT *msgPtr, size_t pos); 
	//Resize the message pointer array
	int cacheResize(msgCacheT *cachePtr, size_t newSize);
	//Remove a message pointer, after freeing it, and reduce the cache's size by one
	int cacheRemove(msgCacheT *cachePtr, size_t pos);
	//Free the contents of the cache, and the pointer itself
	void freeMsgCache(msgCacheT *cachePtr);
	//Free the contents of a message, and the pointer itself
	void freeMsgData(msgT *msgPtr);
#endif
