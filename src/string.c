#include "string.h"

#include <ctype.h>
#include <string.h>


alpm_list_t * split_lines(const char * str) {
	alpm_list_t * list = NULL;

	while(*str != '\0') {
		const char * begin = str;
		while(*str != '\0' && *str != '\r' && *str != '\n')
			str++;
		const char * end = str;
		list = alpm_list_add(list, strndup(begin, end - begin));

		// skip line end
		if(*str == '\r')
			str++;
		if(*str == '\n')
			str++;
	}

	return list;
}


alpm_list_t * split_words(const char * str) {
	alpm_list_t * list = NULL;

	// skip whitespace
	while(*str == ' ' || *str == '\t')
		str++;

	while(*str != '\0') {
		const char * begin = str;
		while(*str != '\0' && *str != ' ' && *str != '\t')
			str++;
		const char * end = str;
		list = alpm_list_add(list, strndup(begin, end - begin));

		// skip whitespace
		while(*str == ' ' || *str == '\t')
			str++;
	}

	return list;
}


void convert_octal(char * str) {
	char * source = str,
	     * dest   = str;
	while(*source != '\0') {
		if(source[0] == '\\' && isdigit(source[1]) && isdigit(source[2]) && isdigit(source[3])) {
			unsigned int value = (source[1] - '0') * 64 + (source[2] - '0') * 8  + (source[3] - '0');
			if(value <= '\xFF') {
				*dest++ = value;
				source += 4;
				continue;
			}
		}
		*dest++ = *source++;
	}
	*dest = 0;
}
