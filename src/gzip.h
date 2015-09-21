#ifndef INCLUDE_GZIP_H
#define INCLUDE_GZIP_H


#ifdef __cplusplus
extern "C" {
#endif


/**
 * Reads the contents of a gzip-compressed file into buffer; enlarges buffer on demand; returns resulting size
 *
 * file_path = path to the gzip file including possible extensions
 * buffer    = pointer to the buffer region; can be modified with realloc; data will also be null-terminated!
 * allocated = pointer to allocated size; can be modified
 * returns -1 on failure, uncompressed content size otherwise
 */
int read_gzip_file(const char * file_path, char ** buffer, unsigned int * allocated);


#ifdef __cplusplus
}
#endif


#endif
