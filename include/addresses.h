#ifndef ADDRESS_GUARD
	
	#define ADDRESS_GUARD

	//A list of addresses is modelled as a singly linked list
	
	/* If an address list node contains the address "John Smith <john@mail.com>",
           personalName contains the string "John Smith", mailboxName contains "john",
           and hostName contains "mail.com" */
	struct addressNode {
		char *personalName;
		char *mailboxName;
		char *hostName;
		struct addressNode *next; //Pointer to the next node of the list
	};
	typedef struct addressNode* addressNodeT;
	
	//Initialize an address list
	addressNodeT addrListInit(void); 

	//Add a node to an address list
	int addrListAdd(addressNodeT *headPtr, char *personalName, char *mailboxName, char *hostName);

	/* Print an address list, with the addresses in the format 
           <personal-name> (<mailbox-name>"@"<host-name>) */
	void printAddressList(addressNodeT head);
	
	//Free an address list
	void freeAddressList(addressNodeT head);
	
	/* Parse an address list in IMAP format (so "(personal-name source-route mailbox-name host-name)",
          with source-route being relevant only to SMTP, so ignored in this application */
	int parseAddress(imapObjectHandleT addressHandle, char **personalNamePtr, char **mailboxNamePtr, char **hostNamePtr);

	//Convert an imapObject of type LIST into a list of addresses
	int getAddressList(addressNodeT *headPtr, imapObjectHandleT addressListHandle);
#endif
