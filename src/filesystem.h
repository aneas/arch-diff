#ifndef INCLUDE_FILESYSTEM_H
#define INCLUDE_FILESYSTEM_H


#include <stdbool.h>
#include <sys/types.h>

#include "list.h"


#ifdef __cplusplus
extern "C" {
#endif


struct filesystem_t;
struct filesystem_entry_t;


typedef void (*filesystem_fn_free)(void *); /* user_data deallocation callback */


struct filesystem_t * filesystem_open();
void                  filesystem_close(struct filesystem_t * handle, filesystem_fn_free fn);

struct filesystem_entry_t * filesystem_get_path(struct filesystem_t * handle, const char * path);

bool filesystem_entry_is_block_device (const struct filesystem_entry_t * entry);
bool filesystem_entry_is_char_device  (const struct filesystem_entry_t * entry);
bool filesystem_entry_is_directory    (const struct filesystem_entry_t * entry);
bool filesystem_entry_is_fifo         (const struct filesystem_entry_t * entry);
bool filesystem_entry_is_regular_file (const struct filesystem_entry_t * entry);
bool filesystem_entry_is_symbolic_link(const struct filesystem_entry_t * entry);
bool filesystem_entry_is_socket       (const struct filesystem_entry_t * entry);

char *       filesystem_entry_get_path          (const struct filesystem_entry_t * entry);
const char * filesystem_entry_get_name          (const struct filesystem_entry_t * entry);
mode_t       filesystem_entry_get_mode          (const struct filesystem_entry_t * entry);
uid_t        filesystem_entry_get_uid           (const struct filesystem_entry_t * entry);
gid_t        filesystem_entry_get_gid           (const struct filesystem_entry_t * entry);
time_t       filesystem_entry_get_mtime         (const struct filesystem_entry_t * entry);
void *       filesystem_entry_get_user_data     (struct filesystem_entry_t * entry);
void         filesystem_entry_set_user_data     (struct filesystem_entry_t * entry, void * user_data);
const char * filesystem_symbolic_link_get_target(const struct filesystem_entry_t * entry);
off_t        filesystem_regular_file_get_size   (const struct filesystem_entry_t * entry);

bool                        filesystem_entry_is_root          (const struct filesystem_entry_t * entry);
bool                        filesystem_entry_has_prev         (const struct filesystem_entry_t * entry);
bool                        filesystem_entry_has_next         (const struct filesystem_entry_t * entry);
bool                        filesystem_entry_has_children     (struct filesystem_entry_t * entry);
struct filesystem_entry_t * filesystem_entry_get_parent       (struct filesystem_entry_t * entry);
struct filesystem_entry_t * filesystem_entry_get_prev         (struct filesystem_entry_t * entry);
struct filesystem_entry_t * filesystem_entry_get_next         (struct filesystem_entry_t * entry);
struct filesystem_entry_t * filesystem_entry_get_first_child  (struct filesystem_entry_t * entry);
struct filesystem_entry_t * filesystem_entry_get_child_by_name(struct filesystem_entry_t * entry, const char * name);


#ifdef __cplusplus
}
#endif


#endif
