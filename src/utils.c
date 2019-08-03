#include <ctype.h>
#include "utils.h"
#include <string.h>
#include <stdio.h>

int isNumber(char *str) {
	for (int k = 0 ; str[k] != '\0' ; k++) {
		if (!isdigit(str[k])) {
			return(0);
		}
	}
	return(1);
}

void strUpper(char *str) {
	for (int k = 0 ; str[k] != '\0' ; k++) {
		if (islower(str[k])) {
			str[k] -= 'a' - 'A';
		}
	}
}


//The generated tags are of the form: ADDD where D are digits, and A an uppercase letter
void generateTag(char tag[TAG_SIZE]) {
	//Static variables are used to keep state

	static int num = 0; //From 0 to 999, represents the digit part
	static char character = 'A'; //From 'A' to 'Z' represents the character part

	bzero(tag, TAG_SIZE);
	sprintf(tag, "%c%03d", character, num); //Write into tag

	//Update the state for the next iteration
	num++;
	if (num == 1000) { //When the digit part surpasses 999 get to the next letter
		if (character == 'Z') { //Upon reaching 'Z' loop back to 'A'
			character = 'A';
		}
		else {
			character++;
		}
		num = 0;
	}
}