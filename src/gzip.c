#include "gzip.h"

#include <stdlib.h>
#include <zlib.h>


int read_gzip_file(const char * file_path, char ** buffer, unsigned int * allocated) {
	gzFile file = gzopen(file_path, "r");
	if(file == NULL)
		return -1;

	unsigned int position = 0;
	while(1) {
		unsigned int allowed_bytes = *allocated - position;
		int bytes_read = gzread(file, *buffer + position, allowed_bytes);
		if(bytes_read == allowed_bytes) {
			position   += bytes_read;
			*allocated += 1024;
			*buffer     = (char *)realloc(*buffer, *allocated);
			if(*buffer == NULL) {
				gzclose(file);
				return -1;
			}
		}
		else if(bytes_read >= 0 && bytes_read < allowed_bytes && gzeof(file)) {
			position += bytes_read;
			(*buffer)[position] = '\0';
			break;
		}
		else {
			gzclose(file);
			return -1;
		}
	}

	gzclose(file);
	return position;
}
