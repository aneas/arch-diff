#ifndef INCLUDE_LIST_H
#define INCLUDE_LIST_H


#include <alpm_list.h>


#ifdef __cplusplus
extern "C" {
#endif


/**
 * Callback function type for alpm_list_each
 */
typedef void (*alpm_list_fn_each)(void *);

/**
 * Calls the function fn for every element in list
 */
void alpm_list_each(alpm_list_t * list, alpm_list_fn_each fn);


#ifdef __cplusplus
}
#endif


#endif
