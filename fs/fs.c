#include "fs.h"

//-----------------------
// Path parsing functions
//-----------------------

char* fs_next_file(char** cur)
{
	char* ret = *cur;
	if(*ret == '\0')
		return NULL;

	for(; **cur != '/' && **cur != '\0'; ++(*cur))
		;
	**cur = '\0';
	++(*cur);

	return ret;
}
