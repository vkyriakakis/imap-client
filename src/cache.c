#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "parsing.h"
#include "addresses.h"
#include "cache.h"
#include "error.h"

msgCacheT *cacheInit(void) {
	msgCacheT *cachePtr;

	cachePtr = malloc(sizeof(msgCacheT));
	if (!cachePtr) {
		return(NULL);
	}

	//Set everything to zero
	cachePtr->msgPtrArray = NULL;
	cachePtr->cacheSize = cachePtr->prevSize = 0;
	cachePtr->recent = 0;

	return(cachePtr);
}


int cacheInsert(msgCacheT *cachePtr, msgT *msgPtr, size_t pos) {
	//If the array is empty
	if (!cachePtr->msgPtrArray) {
		return(SUCCESS);
	}
	//If the given position is out of bounds
	if (pos >= cachePtr->cacheSize || pos < 0) {
		return(SUCCESS);
	}
	cachePtr->msgPtrArray[pos] = msgPtr;

	return(SUCCESS);
}


void freeMsgData(msgT *msgPtr) {
	if (!msgPtr) {
		return;
	}
	free(msgPtr->internalDate);
	free(msgPtr->envelope.subject);
	//freeAddressList is defined in addresses.h
	freeAddressList(msgPtr->envelope.fromList);
	freeAddressList(msgPtr->envelope.toList);
	freeAddressList(msgPtr->envelope.ccList);
	free(msgPtr->text);
	free(msgPtr);
}

void freeMsgCache(msgCacheT *cachePtr) {
	size_t cacheSize;
	msgT **msgPtrArray;

	if (!cachePtr) {
		return;
	}

	//Variables are used to avoid access to the struct fields by pointer
	cacheSize = cachePtr->cacheSize;
	msgPtrArray = cachePtr->msgPtrArray;

	if (msgPtrArray != NULL) {
		for (int k = 0 ; k < cacheSize ; k++) {
			freeMsgData(msgPtrArray[k]);
		}
		free(msgPtrArray);
	}
	free(cachePtr);
}


int cacheResize(msgCacheT *cachePtr, size_t newSize) {
	msgT **temp;
	msgT **msgPtrArray;

	if (newSize == cachePtr->cacheSize) { //If the size stays the same, do nothing
		return(SUCCESS);
	}

	if (newSize == 0) { //If the size is to be set to 0, empty the cache array
		free(cachePtr->msgPtrArray);
		cachePtr->msgPtrArray = NULL;
		cachePtr->cacheSize = cachePtr->prevSize = 0;
		return(SUCCESS);
	}

	//Resize the message pointer array
	temp = realloc(cachePtr->msgPtrArray, newSize*sizeof(msgT*));
	if (!temp) {
		return(MEM_ERROR);
	}
	msgPtrArray = temp;

	//Mark any new positions as empty, by assigning NULL to them
	for (int k = cachePtr->cacheSize ; k < newSize ; k++) {
		msgPtrArray[k] = NULL;
	}

	//Update the cache struct fields via pointer
	cachePtr->msgPtrArray = msgPtrArray;
	cachePtr->cacheSize = newSize;

	return(SUCCESS);
}

int cacheRemove(msgCacheT *cachePtr, size_t pos) {	
	msgT **msgPtrArray, **temp;
	size_t cacheSize;

	if (!cachePtr) { //If the cache hasn't been initialized
		return(SUCCESS);
	}

	//Variables are used to avoid access to the struct fields by pointer
	msgPtrArray = cachePtr->msgPtrArray;
	cacheSize = cachePtr->cacheSize;

	if (pos >= cacheSize || pos < 0) { //Out of bounds
		return(SUCCESS); 
	}

	freeMsgData(msgPtrArray[pos]); //Free the message pointer to be deleted

	if (cacheSize == 1) { //If the message to be deleted was the last
		//Empty cache
		free(msgPtrArray);
		cachePtr->msgPtrArray = NULL;
		cachePtr->cacheSize = cachePtr->prevSize = 0;
		return(SUCCESS);
	}

	//Overwrite the deleted message pointer
	for (int k = pos ; k < cacheSize-1 ; k++) {
		msgPtrArray[k] = msgPtrArray[k+1];
	}

	//Downsize the array by one
	temp = realloc(msgPtrArray, (cachePtr->cacheSize-1)*sizeof(msgT*));
	if (!temp) {
		return(MEM_ERROR);
	}

	//Update the cache struct fields
	cachePtr->msgPtrArray = temp;
	
	//Make them equal, because if the cache decreases in size, no new messages need to be fetched
	cachePtr->prevSize = cachePtr->cacheSize -= 1;

	return(SUCCESS);
}