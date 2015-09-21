#include "list.h"


void alpm_list_each(alpm_list_t * list, alpm_list_fn_each fn) {
	for(alpm_list_t * it = list; it != NULL; it = alpm_list_next(it))
		fn(it->data);
}
