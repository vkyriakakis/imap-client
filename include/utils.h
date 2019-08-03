#ifndef UTILS_GUARD

	#define UTILS_GUARD
	#define TAG_SIZE 5

	int isNumber(char *str); //Detect whether str consists of digit characters only
	void strUpper(char *str); //Convert str to uppercase
	void generateTag(char tag[TAG_SIZE]); //Generate a tag to use with IMAP commands
#endif
