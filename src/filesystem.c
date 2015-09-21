#define _GNU_SOURCE
#include "filesystem.h"

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/limits.h>


typedef enum {
	FILESYSTEM_ENTRY_TYPE_BLOCK,
	FILESYSTEM_ENTRY_TYPE_CHAR,
	FILESYSTEM_ENTRY_TYPE_DIR,
	FILESYSTEM_ENTRY_TYPE_FIFO,
	FILESYSTEM_ENTRY_TYPE_FILE,
	FILESYSTEM_ENTRY_TYPE_LINK,
	FILESYSTEM_ENTRY_TYPE_SOCKET
} filesystem_entry_type_t;


typedef struct {
	bool                                  children_evaluated;
	struct filesystem_entry_internal_t ** children;
	size_t                                children_count;
} filesystem_directory_t;


typedef struct {
	off_t size;
} filesystem_file_t;


typedef struct {
	char * target;
} filesystem_symlink_t;


typedef struct filesystem_entry_internal_t {
	struct filesystem_entry_internal_t * parent,
	                                   * prev,
	                                   * next;
	filesystem_entry_type_t              type;

	char *                  name;
	mode_t                  mode;
	uid_t                   uid;
	gid_t                   gid;
	time_t                  mtime;
	void *                  user_data;

	union {
		filesystem_directory_t   dir;
		filesystem_file_t        file;
		filesystem_symlink_t     link;
	} data;
} filesystem_entry_internal_t;


typedef struct {
	filesystem_entry_internal_t * root;
} filesystem_internal_t;


struct filesystem_t * filesystem_open() {
	filesystem_entry_internal_t * root = (filesystem_entry_internal_t *)malloc(sizeof(filesystem_entry_internal_t));
	if(root == NULL)
		return NULL;
	root->parent                      = NULL;
	root->prev                        = NULL;
	root->next                        = NULL;
	root->type                        = FILESYSTEM_ENTRY_TYPE_DIR;
	root->name                        = strdup("");
	root->mode                        = 0;
	root->uid                         = 0;
	root->gid                         = 0;
	root->mtime                       = 0;
	root->user_data                   = NULL;
	root->data.dir.children_evaluated = false;

	filesystem_internal_t * fs = (filesystem_internal_t *)malloc(sizeof(filesystem_internal_t));
	fs->root = root;
	return (struct filesystem_t *)fs;
}


static void filesystem_free_entry(filesystem_entry_internal_t * entry, filesystem_fn_free fn) {
	free(entry->name);
	if(fn != NULL && entry->user_data != NULL)
		fn(entry->user_data);

	switch(entry->type) {
		case FILESYSTEM_ENTRY_TYPE_DIR:
			if(entry->data.dir.children_evaluated) {
				for(size_t i = 0; i < entry->data.dir.children_count; i++)
					filesystem_free_entry(entry->data.dir.children[i], fn);
			}
			break;

		case FILESYSTEM_ENTRY_TYPE_LINK:
			free(entry->data.link.target);
			break;

		default:
			break;
	}

	free(entry);
}


void filesystem_close(struct filesystem_t * handle, filesystem_fn_free fn) {
	filesystem_internal_t * priv = (filesystem_internal_t *)handle;
	filesystem_free_entry(priv->root, fn);
	free(handle);
}


struct filesystem_entry_t * filesystem_get_path(struct filesystem_t * handle, const char * path) {
	if(path[0] != '/')
		return NULL;

	filesystem_internal_t * priv = (filesystem_internal_t *)handle;

	struct filesystem_entry_t * entry = (struct filesystem_entry_t *)priv->root;
	const char * left = path + 1;
	while(entry != NULL) {
		if(*left == '\0')
			return entry;

		const char * right = strchr(left, '/');
		if(right == NULL)
			return filesystem_entry_get_child_by_name(entry, left);

		char * name = strndup(left, right - left);
		entry = filesystem_entry_get_child_by_name(entry, name);
		free(name);

		left = right + 1; // skip '/'
	}

	return NULL;
}


static int compare_entries(const filesystem_entry_internal_t ** a, const filesystem_entry_internal_t ** b) {
	return strcmp((*a)->name, (*b)->name);
}


static char * readlink_malloc(const char * path, size_t probable_length) {
	size_t allocated = (probable_length != 0 ? probable_length : PATH_MAX);
	char * buffer    = (char *)malloc(allocated + 1); // 1 for terminating '\0'-byte
	assert(buffer != NULL);

	int result;
	while((result = readlink(path, buffer, allocated + 1)) == allocated + 1) {
		// we did not allocate enough space, so lets try again
		allocated *= 2;
		buffer     = (char *)realloc(buffer, allocated + 1); // 1 for terminating '\0'-byte
		assert(buffer != NULL);
	}

	if(result == -1) {
		if(errno == EACCES) {
			fprintf(stderr, "error: permission denied `%s'\n", path);
			buffer = (char *)realloc(buffer, 1); // 1 for terminating '\0'-byte
			assert(buffer != NULL);
			buffer[0] = '\0';
		}
		else {
			perror("readlink");
			exit(EXIT_FAILURE);
		}
	}

	if(result < allocated) {
		allocated = result;
		buffer    = (char *)realloc(buffer, allocated + 1); // 1 for terminating '\0'-byte
		assert(buffer != NULL);
	}

	buffer[result] = '\0';
	return buffer;
}


static void filesystem_entry_expand(filesystem_entry_internal_t * entry) {
	if(entry->type != FILESYSTEM_ENTRY_TYPE_DIR || entry->data.dir.children_evaluated)
		return;

	char * path = filesystem_entry_get_path((struct filesystem_entry_t *)entry);

	DIR * dirp = opendir(path);
	if(dirp == NULL) {
		if(errno == EACCES) {
			fprintf(stderr, "error: permission denied `%s'\n", path);
			return;
		}
		else {
			perror("opendir");
			exit(EXIT_FAILURE);
		}
	}

	// pre-allocate some memory for the child entries
	size_t children_allocated = 16;
	entry->data.dir.children       = (filesystem_entry_internal_t **)malloc(children_allocated * sizeof(filesystem_entry_internal_t *));
	entry->data.dir.children_count = 0;
	assert(entry->data.dir.children != NULL);

	while(true) {
		errno = 0;
		struct dirent * dp = readdir(dirp);
		if(dp == NULL) {
			assert(errno == 0);
			break; // end of directory
		}

		if(strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
			continue;

		char * childpath;
		int result = asprintf(&childpath, "%s/%s", path, dp->d_name);
		assert(result != -1);

		struct stat info;
		result = lstat(childpath, &info);
		if(result != 0) {
			perror("lstat");
			exit(EXIT_FAILURE);
		}

		filesystem_entry_internal_t * child = (filesystem_entry_internal_t *)malloc(sizeof(filesystem_entry_internal_t));
		child->parent = entry;
		// child->prev & child->next will be set further down, after sorting is done
		switch(info.st_mode & S_IFMT) {
			case S_IFBLK:  child->type = FILESYSTEM_ENTRY_TYPE_BLOCK;  break;
			case S_IFCHR:  child->type = FILESYSTEM_ENTRY_TYPE_CHAR;   break;
			case S_IFDIR:  child->type = FILESYSTEM_ENTRY_TYPE_DIR;    break;
			case S_IFIFO:  child->type = FILESYSTEM_ENTRY_TYPE_FIFO;   break;
			case S_IFLNK:  child->type = FILESYSTEM_ENTRY_TYPE_LINK;   break;
			case S_IFREG:  child->type = FILESYSTEM_ENTRY_TYPE_FILE;   break;
			case S_IFSOCK: child->type = FILESYSTEM_ENTRY_TYPE_SOCKET; break;
			default:       assert(false);                              break;
		}
		child->name      = strdup(dp->d_name);
		child->mode      = info.st_mode;
		child->uid       = info.st_uid;
		child->gid       = info.st_gid;
		child->mtime     = info.st_mtime;
		child->user_data = NULL;
		if(child->type == FILESYSTEM_ENTRY_TYPE_DIR)
			child->data.dir.children_evaluated = false;
		else if(child->type == FILESYSTEM_ENTRY_TYPE_FILE)
			child->data.file.size = info.st_size;
		else if(child->type == FILESYSTEM_ENTRY_TYPE_LINK)
			child->data.link.target = readlink_malloc(childpath, info.st_size);

		if(entry->data.dir.children_count == children_allocated) {
			children_allocated *= 2;
			entry->data.dir.children = (filesystem_entry_internal_t **)realloc(entry->data.dir.children, children_allocated * sizeof(filesystem_entry_internal_t *));
			assert(entry->data.dir.children != NULL);
		}
		assert(entry->data.dir.children_count < children_allocated);
		entry->data.dir.children[entry->data.dir.children_count] = child;
		entry->data.dir.children_count++;

		free(childpath);
	}

	// shrink the allocated memory to free up some space
	if(entry->data.dir.children_count == 0) {
		free(entry->data.dir.children);
		entry->data.dir.children = NULL;
	}
	else {
		entry->data.dir.children = (filesystem_entry_internal_t **)realloc(entry->data.dir.children, entry->data.dir.children_count * sizeof(filesystem_entry_internal_t *));
		assert(entry->data.dir.children != NULL);
	}

	// sort all children by name to allow for binary search
	qsort(entry->data.dir.children, entry->data.dir.children_count, sizeof(filesystem_entry_internal_t *), (int (*)(const void *, const void *))compare_entries);

	// establish sibling links between all children
	for(size_t i = 0; i < entry->data.dir.children_count; i++) {
		entry->data.dir.children[i]->prev = (i == 0                                  ? NULL : entry->data.dir.children[i - 1]);
		entry->data.dir.children[i]->next = (i == entry->data.dir.children_count - 1 ? NULL : entry->data.dir.children[i + 1]);
	}

	entry->data.dir.children_evaluated = true;

	closedir(dirp);

	free(path);
}


bool filesystem_entry_is_block_device(const struct filesystem_entry_t * entry) {
	const filesystem_entry_internal_t * priv = (const filesystem_entry_internal_t *)entry;
	return (priv->type == FILESYSTEM_ENTRY_TYPE_BLOCK);
}

bool filesystem_entry_is_char_device(const struct filesystem_entry_t * entry) {
	const filesystem_entry_internal_t * priv = (const filesystem_entry_internal_t *)entry;
	return (priv->type == FILESYSTEM_ENTRY_TYPE_CHAR);
}


bool filesystem_entry_is_directory(const struct filesystem_entry_t * entry) {
	const filesystem_entry_internal_t * priv = (const filesystem_entry_internal_t *)entry;
	return (priv->type == FILESYSTEM_ENTRY_TYPE_DIR);
}


bool filesystem_entry_is_fifo(const struct filesystem_entry_t * entry) {
	const filesystem_entry_internal_t * priv = (const filesystem_entry_internal_t *)entry;
	return (priv->type == FILESYSTEM_ENTRY_TYPE_FIFO);
}


bool filesystem_entry_is_regular_file(const struct filesystem_entry_t * entry) {
	const filesystem_entry_internal_t * priv = (const filesystem_entry_internal_t *)entry;
	return (priv->type == FILESYSTEM_ENTRY_TYPE_FILE);
}


bool filesystem_entry_is_symbolic_link(const struct filesystem_entry_t * entry) {
	const filesystem_entry_internal_t * priv = (const filesystem_entry_internal_t *)entry;
	return (priv->type == FILESYSTEM_ENTRY_TYPE_LINK);
}


bool filesystem_entry_is_socket(const struct filesystem_entry_t * entry) {
	const filesystem_entry_internal_t * priv = (const filesystem_entry_internal_t *)entry;
	return (priv->type == FILESYSTEM_ENTRY_TYPE_SOCKET);
}


char * filesystem_entry_get_path(const struct filesystem_entry_t * entry) {
	if(filesystem_entry_is_root(entry))
		return strdup("/");

	alpm_list_t * parts  = NULL;
	size_t        length = 0;
	while(entry != NULL && !filesystem_entry_is_root(entry)) {
		const char * name = filesystem_entry_get_name(entry);
		length += strlen(name) + 1; // 1 for each '/' delimiter
		parts = alpm_list_add(parts, (void *)name);
		entry = (const struct filesystem_entry_t *)filesystem_entry_get_parent((struct filesystem_entry_t *)entry);
	}

	char * str = (char *)malloc(length + 1); // 1 for '\0'-byte
	size_t pos = 0;
	for(alpm_list_t * it = alpm_list_last(parts); it != NULL; it = alpm_list_previous(it)) {
		assert(pos + 1 + strlen((const char *)it->data) <= length);
		str[pos] = '/';
		strcpy(str + pos + 1, (const char *)it->data);
		pos += 1 + strlen((const char *)it->data);
	}
	assert(pos == length);
	str[length] = '\0';

	return str;
}


const char * filesystem_entry_get_name(const struct filesystem_entry_t * entry) {
	const filesystem_entry_internal_t * priv = (const filesystem_entry_internal_t *)entry;
	return priv->name;
}


mode_t filesystem_entry_get_mode(const struct filesystem_entry_t * entry) {
	const filesystem_entry_internal_t * priv = (const filesystem_entry_internal_t *)entry;
	return priv->mode;
}


uid_t filesystem_entry_get_uid(const struct filesystem_entry_t * entry) {
	const filesystem_entry_internal_t * priv = (const filesystem_entry_internal_t *)entry;
	return priv->uid;
}


gid_t filesystem_entry_get_gid(const struct filesystem_entry_t * entry) {
	const filesystem_entry_internal_t * priv = (const filesystem_entry_internal_t *)entry;
	return priv->gid;
}


time_t filesystem_entry_get_mtime(const struct filesystem_entry_t * entry) {
	const filesystem_entry_internal_t * priv = (const filesystem_entry_internal_t *)entry;
	return priv->mtime;
}


void * filesystem_entry_get_user_data(struct filesystem_entry_t * entry) {
	filesystem_entry_internal_t * priv = (filesystem_entry_internal_t *)entry;
	return priv->user_data;
}


void filesystem_entry_set_user_data(struct filesystem_entry_t * entry, void * user_data) {
	filesystem_entry_internal_t * priv = (filesystem_entry_internal_t *)entry;
	priv->user_data = user_data;
}


const char * filesystem_symbolic_link_get_target(const struct filesystem_entry_t * entry) {
	const filesystem_entry_internal_t * priv = (const filesystem_entry_internal_t *)entry;
	if(priv->type == FILESYSTEM_ENTRY_TYPE_LINK)
		return priv->data.link.target;
	else
		return "";
}


off_t filesystem_regular_file_get_size(const struct filesystem_entry_t * entry) {
	const filesystem_entry_internal_t * priv = (const filesystem_entry_internal_t *)entry;
	if(priv->type == FILESYSTEM_ENTRY_TYPE_FILE)
		return priv->data.file.size;
	else
		return 0;
}


bool filesystem_entry_is_root(const struct filesystem_entry_t * entry) {
	const filesystem_entry_internal_t * priv = (const filesystem_entry_internal_t *)entry;
	return (priv->parent == NULL);
}


bool filesystem_entry_has_prev(const struct filesystem_entry_t * entry) {
	const filesystem_entry_internal_t * priv = (const filesystem_entry_internal_t *)entry;
	return (priv->prev == NULL);
}


bool filesystem_entry_has_next(const struct filesystem_entry_t * entry) {
	const filesystem_entry_internal_t * priv = (const filesystem_entry_internal_t *)entry;
	return (priv->next == NULL);
}


bool filesystem_entry_has_children(struct filesystem_entry_t * entry) {
	filesystem_entry_internal_t * priv = (filesystem_entry_internal_t *)entry;
	if(priv->type == FILESYSTEM_ENTRY_TYPE_DIR) {
		filesystem_entry_expand(priv);
		if(!priv->data.dir.children_evaluated)
			return false;
		return (priv->data.dir.children_count > 0);
	}
	return false;
}


struct filesystem_entry_t * filesystem_entry_get_parent(struct filesystem_entry_t * entry) {
	filesystem_entry_internal_t * priv = (filesystem_entry_internal_t *)entry;
	return (struct filesystem_entry_t *)priv->parent;
}


struct filesystem_entry_t * filesystem_entry_get_prev(struct filesystem_entry_t * entry) {
	filesystem_entry_internal_t * priv = (filesystem_entry_internal_t *)entry;
	return (struct filesystem_entry_t *)priv->prev;
}


struct filesystem_entry_t * filesystem_entry_get_next(struct filesystem_entry_t * entry) {
	filesystem_entry_internal_t * priv = (filesystem_entry_internal_t *)entry;
	return (struct filesystem_entry_t *)priv->next;
}


struct filesystem_entry_t * filesystem_entry_get_first_child(struct filesystem_entry_t * entry) {
	filesystem_entry_internal_t * priv = (filesystem_entry_internal_t *)entry;
	if(priv->type == FILESYSTEM_ENTRY_TYPE_DIR) {
		filesystem_entry_expand(priv);
		if(!priv->data.dir.children_evaluated)
			return NULL;
		if(priv->data.dir.children_count != 0)
			return (struct filesystem_entry_t *)priv->data.dir.children[0];
	}
	return NULL;
}


struct filesystem_entry_t * filesystem_entry_get_child_by_name(struct filesystem_entry_t * entry, const char * name) {
	filesystem_entry_internal_t * priv = (filesystem_entry_internal_t *)entry;

	if(priv->type != FILESYSTEM_ENTRY_TYPE_DIR)
		return NULL;

	filesystem_entry_expand(priv);
	if(!priv->data.dir.children_evaluated)
		return NULL;

	// binary search
	size_t left  = 0,
	       right = priv->data.dir.children_count;
	while(left < right) {
		size_t mid = (left + right) / 2;
		int    cmp = strcmp(name, priv->data.dir.children[mid]->name);
		if(cmp < 0)
			right = mid;
		else if(cmp > 0)
			left = mid + 1;
		else
			return (struct filesystem_entry_t *)priv->data.dir.children[mid];
	}

	return NULL;
}
