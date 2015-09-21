#ifndef INCLUDE_MTREE_H
#define INCLUDE_MTREE_H


#include <stdbool.h>

#include "list.h"


#ifdef __cplusplus
extern "C" {
#endif


/**
 * Opaque struct representing a single entry in a mtree file.
 */
struct mtree_entry_t;

/**
 * We only support the following fixed, hardcoded list of properties.
 */
typedef enum {
	MTREE_KEYWORD_UNKNOWN, // error value
	MTREE_KEYWORD_TIME,
	MTREE_KEYWORD_MODE,
	MTREE_KEYWORD_SIZE,
	MTREE_KEYWORD_TYPE,
	MTREE_KEYWORD_UID,
	MTREE_KEYWORD_GID,
	MTREE_KEYWORD_LINK,
	MTREE_KEYWORD_MD5DIGEST,
	MTREE_KEYWORD_SHA256DIGEST
} mtree_keyword_t;

/**
 * Helper functions to parse and stringify keywords.
 */
mtree_keyword_t mtree_parse_keyword(const char * keyword, int keyword_length);
const char *    mtree_keyword_to_string(mtree_keyword_t keyword);

/**
 * Parses the contents of an mtree string and returns a resolved and cleaned-up list of entries.
 *
 * Free the resulting list with:
 *   alpm_list_free_inner(list, (alpm_list_fn_free)mtree_entry_destroy);
 *   alpm_list_free(list);
 */
alpm_list_t * mtree_parse(const char * str);

/**
 * Allocation, cloning and destruction of entries.
 */
struct mtree_entry_t * mtree_entry_create();
struct mtree_entry_t * mtree_entry_clone(const struct mtree_entry_t * entry);
void                   mtree_entry_destroy(struct mtree_entry_t * entry);

/**
 * Accessors and modifiers for the filepath stored in each entry.
 */
bool         mtree_entry_has_filepath(const struct mtree_entry_t * entry);
const char * mtree_entry_get_filepath(const struct mtree_entry_t * entry);
void         mtree_entry_set_filepath(struct mtree_entry_t * entry, const char * filepath);
void         mtree_entry_unset_filepath(struct mtree_entry_t * entry);

/**
 * Accessors and modifiers for the keywords stored in each entry.
 */
bool         mtree_entry_has_keyword(const struct mtree_entry_t * entry, mtree_keyword_t keyword);
const char * mtree_entry_get_keyword(const struct mtree_entry_t * entry, mtree_keyword_t keyword);
void         mtree_entry_set_keyword(struct mtree_entry_t * entry, mtree_keyword_t keyword, const char * value);
void         mtree_entry_unset_keyword(struct mtree_entry_t * entry, mtree_keyword_t keyword);


#ifdef __cplusplus
}
#endif


#endif
