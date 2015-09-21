#ifndef INCLUDE_STRING_H
#define INCLUDE_STRING_H


#include "list.h"


#ifdef __cplusplus
extern "C" {
#endif


/**
 * Splits a string into a list of newly allocated lines seperated by "\n", "\r" or "\r\n".
 * Every line has to be deallocated with free().
 */
alpm_list_t * split_lines(const char * str);

/**
 * Splits a string into a list of newly allocated "words" seperated spaces and tabs.
 * Trailing whitespace will be removed.
 * Every word has to be deallocated with free().
 */
alpm_list_t * split_words(const char * str);

/**
 * Converts backspaces followed by exactly three digits representing an octal value inplace to bytes.
 * The string might get shorter, but will be correctly null-terminated.
 */
void convert_octal(char * str);


#ifdef __cplusplus
}
#endif


#endif
