#include "mtree.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "string.h"


/**
 * Internal entry struct. If you add more entries, don't forget to adjust the following definitions:
 * - const    MTREE_ENTRY_ALL_KEYWORDS
 * - function mtree_parse_keyword
 * - function mtree_keyword_to_string
 * - function mtree_entry_get_keyword_pointer
 * - function mtree_entry_create
 * - enum     mtree_keyword_t
 */
typedef struct {
	char * filepath;
	char * time;
	char * mode;
	char * size;
	char * type;
	char * uid;
	char * gid;
	char * link;
	char * md5digest;
	char * sha256digest;
} mtree_entry_internal_t;


/**
 * A static list of all supported keywords.
 */
static const mtree_keyword_t MTREE_ENTRY_ALL_KEYWORDS[] = {
	MTREE_KEYWORD_TIME,
	MTREE_KEYWORD_MODE,
	MTREE_KEYWORD_SIZE,
	MTREE_KEYWORD_TYPE,
	MTREE_KEYWORD_UID,
	MTREE_KEYWORD_GID,
	MTREE_KEYWORD_LINK,
	MTREE_KEYWORD_MD5DIGEST,
	MTREE_KEYWORD_SHA256DIGEST
};


mtree_keyword_t mtree_parse_keyword(const char * keyword, int keyword_length) {
	switch(keyword_length) {
		case 3:
			     if(!strncmp(keyword, "uid", 3)) return MTREE_KEYWORD_UID;
			else if(!strncmp(keyword, "gid", 3)) return MTREE_KEYWORD_GID;
			break;

		case 4:
			     if(!strncmp(keyword, "time", 4)) return MTREE_KEYWORD_TIME;
			else if(!strncmp(keyword, "mode", 4)) return MTREE_KEYWORD_MODE;
			else if(!strncmp(keyword, "size", 4)) return MTREE_KEYWORD_SIZE;
			else if(!strncmp(keyword, "type", 4)) return MTREE_KEYWORD_TYPE;
			else if(!strncmp(keyword, "link", 4)) return MTREE_KEYWORD_LINK;
			break;

		case 9:
			     if(!strncmp(keyword, "md5digest", 9)) return MTREE_KEYWORD_MD5DIGEST;
			break;

		case 12:
			     if(!strncmp(keyword, "sha256digest", 12)) return MTREE_KEYWORD_SHA256DIGEST;
			break;

		default:
			break;
	}
	return MTREE_KEYWORD_UNKNOWN;
}


const char * mtree_keyword_to_string(mtree_keyword_t keyword) {
	switch(keyword) {
		default:                         return "";
		case MTREE_KEYWORD_TIME:         return "time";
		case MTREE_KEYWORD_MODE:         return "mode";
		case MTREE_KEYWORD_SIZE:         return "size";
		case MTREE_KEYWORD_TYPE:         return "type";
		case MTREE_KEYWORD_UID:          return "uid";
		case MTREE_KEYWORD_GID:          return "gid";
		case MTREE_KEYWORD_LINK:         return "link";
		case MTREE_KEYWORD_MD5DIGEST:    return "md5digest";
		case MTREE_KEYWORD_SHA256DIGEST: return "sha256digest";
	}
}


/**
 * Helper function to map keywords to their respective struct fields.
 */
static char ** mtree_entry_get_keyword_pointer(struct mtree_entry_t * entry, mtree_keyword_t keyword) {
	mtree_entry_internal_t * priv = (mtree_entry_internal_t *)entry;
	switch(keyword) {
		case MTREE_KEYWORD_TIME:         return &priv->time;
		case MTREE_KEYWORD_MODE:         return &priv->mode;
		case MTREE_KEYWORD_SIZE:         return &priv->size;
		case MTREE_KEYWORD_TYPE:         return &priv->type;
		case MTREE_KEYWORD_UID:          return &priv->uid;
		case MTREE_KEYWORD_GID:          return &priv->gid;
		case MTREE_KEYWORD_LINK:         return &priv->link;
		case MTREE_KEYWORD_MD5DIGEST:    return &priv->md5digest;
		case MTREE_KEYWORD_SHA256DIGEST: return &priv->sha256digest;
		default:                         return NULL;
	}
}


/**
 * Const version of mtree_entry_get_keyword_pointer.
 */
static const char ** mtree_entry_get_keyword_const_pointer(const struct mtree_entry_t * entry, mtree_keyword_t keyword) {
	return (const char **)mtree_entry_get_keyword_pointer((struct mtree_entry_t *)entry, keyword);
}


struct mtree_entry_t * mtree_entry_create() {
	mtree_entry_internal_t * entry = (mtree_entry_internal_t *)malloc(sizeof(mtree_entry_internal_t));
	if(entry == NULL)
		return NULL;
	memset(entry, 0, sizeof(mtree_entry_internal_t));
	entry->time         = NULL;
	entry->mode         = NULL;
	entry->size         = NULL;
	entry->type         = NULL;
	entry->uid          = NULL;
	entry->gid          = NULL;
	entry->link         = NULL;
	entry->md5digest    = NULL;
	entry->sha256digest = NULL;
	return (struct mtree_entry_t *)entry;
}


struct mtree_entry_t * mtree_entry_clone(const struct mtree_entry_t * entry) {
	struct mtree_entry_t * clone = mtree_entry_create();

	if(mtree_entry_has_filepath(entry))
		mtree_entry_set_filepath(clone, mtree_entry_get_filepath(entry));

	for(int i = 0; i < sizeof(MTREE_ENTRY_ALL_KEYWORDS) / sizeof(MTREE_ENTRY_ALL_KEYWORDS[0]); i++) {
		if(mtree_entry_has_keyword(entry, MTREE_ENTRY_ALL_KEYWORDS[i]))
			mtree_entry_set_keyword(clone, MTREE_ENTRY_ALL_KEYWORDS[i], mtree_entry_get_keyword(entry, MTREE_ENTRY_ALL_KEYWORDS[i]));
	}

	return clone;
}


void mtree_entry_destroy(struct mtree_entry_t * entry) {
	mtree_entry_unset_filepath(entry);
	for(int i = 0; i < sizeof(MTREE_ENTRY_ALL_KEYWORDS) / sizeof(MTREE_ENTRY_ALL_KEYWORDS[0]); i++)
		mtree_entry_unset_keyword(entry, MTREE_ENTRY_ALL_KEYWORDS[i]);
	free(entry);
}


bool mtree_entry_has_filepath(const struct mtree_entry_t * entry) {
	const mtree_entry_internal_t * priv = (const mtree_entry_internal_t *)entry;
	return (priv->filepath != NULL);
}


const char * mtree_entry_get_filepath(const struct mtree_entry_t * entry) {
	const mtree_entry_internal_t * priv = (const mtree_entry_internal_t *)entry;
	if(priv->filepath == NULL)
		return "";
	else
		return priv->filepath;
}


void mtree_entry_set_filepath(struct mtree_entry_t * entry, const char * filepath) {
	mtree_entry_unset_filepath(entry);

	mtree_entry_internal_t * priv = (mtree_entry_internal_t *)entry;
	priv->filepath = strdup(filepath);
}


void mtree_entry_unset_filepath(struct mtree_entry_t * entry) {
	mtree_entry_internal_t * priv = (mtree_entry_internal_t *)entry;
	if(priv->filepath != NULL) {
		free(priv->filepath);
		priv->filepath = NULL;
	}
}


bool mtree_entry_has_keyword(const struct mtree_entry_t * entry, mtree_keyword_t keyword) {
	return (*mtree_entry_get_keyword_const_pointer(entry, keyword) != NULL);
}


const char * mtree_entry_get_keyword(const struct mtree_entry_t * entry, mtree_keyword_t keyword) {
	const char ** ptr = mtree_entry_get_keyword_const_pointer(entry, keyword);
	if(ptr == NULL)
		return "";
	else
		return *ptr;
}


void mtree_entry_set_keyword(struct mtree_entry_t * entry, mtree_keyword_t keyword, const char * value) {
	char ** ptr = mtree_entry_get_keyword_pointer(entry, keyword);
	if(*ptr != NULL) {
		free(*ptr);
		*ptr = NULL;
	}
	*ptr = strdup(value);
}


void mtree_entry_unset_keyword(struct mtree_entry_t * entry, mtree_keyword_t keyword) {
	char ** ptr = mtree_entry_get_keyword_pointer(entry, keyword);
	if(*ptr != NULL) {
		free(*ptr);
		*ptr = NULL;
	}
}


/**
 * Helper function to set a list of keywords at once. Each entry must start with a keyword, followed by a `=', followed by the value.
 */
static void mtree_entry_set_keyvalue_pairs(struct mtree_entry_t * entry, alpm_list_t * string_list) {
	for(alpm_list_t * it = string_list; it != NULL; it = alpm_list_next(it)) {
		const char * pair = it->data;
		const char * delimiter = strchr(pair, '=');
		if(delimiter == NULL) {
			fprintf(stderr, "Error: bad key-value pair `%s'\n", pair);
			continue;
		}

		mtree_keyword_t keyword = mtree_parse_keyword(pair, delimiter - pair);
		if(keyword == MTREE_KEYWORD_UNKNOWN) {
			fprintf(stderr, "Error: unknown keyword `%.*s'\n", (int)(delimiter - pair), pair);
			continue;
		}

		mtree_entry_set_keyword(entry, keyword, delimiter + 1);
	}
}


/**
 * Helper function to unset a list of keywords at once.
 */
static void mtree_entry_unset_keywords(struct mtree_entry_t * entry, alpm_list_t * string_list) {
	for(alpm_list_t * it = string_list; it != NULL; it = alpm_list_next(it)) {
		const char *    keyword_string = it->data;
		mtree_keyword_t keyword        = mtree_parse_keyword(keyword_string, strlen(keyword_string));
		if(keyword != MTREE_KEYWORD_UNKNOWN)
			mtree_entry_unset_keyword(entry, keyword);
	}
}


alpm_list_t * mtree_parse(const char * str) {
	alpm_list_t * entries = NULL;

	struct mtree_entry_t * defaults = mtree_entry_create();

	alpm_list_t * lines = split_lines(str);
	for(alpm_list_t * i_line = lines; i_line != NULL; i_line = alpm_list_next(i_line)) {
		const char * line = i_line->data;

		alpm_list_t * words = split_words(line);
		alpm_list_each(words, (alpm_list_fn_each)convert_octal);
		if(words == NULL) {
			// do nothing for empty lines
		}
		else {
			const char * first_word = (const char *)words->data;
			if(first_word[0] == '#') {
				// do nothing for comments
			}
			else if(first_word[0] == '/') { // special
				const char * special = first_word + 1;
				if(strcmp(special, "set") == 0)
					mtree_entry_set_keyvalue_pairs(defaults, alpm_list_nth(words, 1));
				else if(strcmp(special, "unset") == 0)
					mtree_entry_unset_keywords(defaults, alpm_list_nth(words, 1));
				else
					fprintf(stderr, "Error: unknown command `%s'\n", special);
			}
			else if(first_word[0] == '.' && first_word[1] == '/') {
				struct mtree_entry_t * entry = mtree_entry_clone(defaults);
				mtree_entry_set_filepath(entry, first_word + 1);
				mtree_entry_set_keyvalue_pairs(entry, alpm_list_nth(words, 1));
				entries = alpm_list_add(entries, entry);
			}
			else
				fprintf(stderr, "Error: unsupported line `%s'\n", line);
		}

		FREELIST(words);
	}

	FREELIST(lines);

	mtree_entry_destroy(defaults);

	return entries;
}
