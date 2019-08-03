#include <stdlib.h>
#include <stdio.h>
#include "parsing.h"
#include "utf8.h"
#include "addresses.h"
#include "error.h"

addressNodeT addrListInit(void) {
	return(NULL);
}

//Add a new address node to an address list
int addrListAdd(addressNodeT *headPtr, char *personalName, char *mailboxName, char *hostName) {
	struct addressNode *new;

	new = malloc(sizeof(struct addressNode));
	if (!new) {
		return(MEM_ERROR);
	}
	new->personalName = personalName;
	new->mailboxName = mailboxName;
	new->hostName = hostName;
	new->next = *headPtr;

	*headPtr = new;

	return(SUCCESS);
}

void freeAddressList(addressNodeT head) {
	addressNodeT curr, next;

	for (curr = head ; curr != NULL ; curr = next) {
		next = curr->next;
		free(curr->personalName);
		free(curr->mailboxName);
		free(curr->hostName);
		free(curr);
	}
}

int parseAddress(imapObjectHandleT addressHandle, char **personalNamePtr, char **mailboxNamePtr, char **hostNamePtr) {
	imapObjectHandleT *elemArray = addressHandle->content.list.elemArray;
	char *personalName, *mailboxName, *hostName;
	int retVal;

	if (addressHandle->content.list.elems != 4) {
		//Server didn't follow the protocol, which specifies 4 fields for addresses, not my fault
		return(PARSE_ERROR);
	}

	//Get personal name from the first handle of elemArray, after decoding it (for more, check utf8.h)
	retVal = decodedCopyFromObject(&personalName, elemArray[0]);
	if (isError(retVal)) {
		return(retVal);
	}

	//Skip the second field (source-route, only relevant to SMTP)

	//Get mailbox name from third field
	retVal = decodedCopyFromObject(&mailboxName, elemArray[2]);
	if (isError(retVal)) {
		free(personalName);
		return(retVal);
	}

	//Get host name from fourth field
	retVal = decodedCopyFromObject(&hostName, elemArray[3]); 
	if (isError(retVal)) {
		free(personalName);
		free(mailboxName);
		return(retVal);
	}

	//Output via pointers
	*personalNamePtr = personalName;
	*mailboxNamePtr = mailboxName;
	*hostNamePtr = hostName;

	return(SUCCESS);
}

int getAddressList(addressNodeT *headPtr, imapObjectHandleT addrListHandle) {
	char *hostName, *personalName, *mailboxName;
	imapObjectHandleT *elemArray = addrListHandle->content.list.elemArray;
	int elems = addrListHandle->content.list.elems;
	addressNodeT addrList = NULL;
	int retVal;

	if (addrListHandle->tag == NIL) {
		*headPtr = NULL;
		return(SUCCESS); //Empty list represented by handle to a NIL object, not an error
	}

	//Iterate over the handle list (the elements are the addresses), in order to add the addresses to the list
	for (int k = 0 ; k < elems ; k++) {
		retVal = parseAddress(elemArray[k], &personalName, &mailboxName, &hostName);
		if (isError(retVal)) {
			freeAddressList(addrList);
			return(retVal);
		}

		if (isError(addrListAdd(&addrList, personalName, mailboxName, hostName))) {
			freeAddressList(addrList);
			free(hostName);
			free(personalName);
			free(mailboxName);
			return(MEM_ERROR);
		}
	}

	*headPtr = addrList;

	return(SUCCESS);
}

void printAddressList(addressNodeT head) {
	addressNodeT curr;

	for (curr = head ; curr != NULL ; curr = curr->next) {
		if (curr->personalName != NULL) {
			printf("%s <%s@%s>", curr->personalName, curr->mailboxName, curr->hostName);
		}
		else {
			printf("<%s@%s>", curr->mailboxName, curr->hostName);
		}
		if (curr->next != NULL) {
			printf(", ");
		}
	}
}