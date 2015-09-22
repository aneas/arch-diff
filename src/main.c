#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fnmatch.h>
#include <getopt.h>

#include <alpm.h>

#include "filesystem.h"
#include "gzip.h"
#include "md5.h"
#include "mtree.h"


static const char * default_root      = "/";
static const char * default_db_path   = "/var/lib/pacman/";
static const char * default_ignores[] = {
	"/dev/*",
	"/etc/ssl/certs/*",
	"/proc/*",
	"/root/*",
	"/run/*",
	"/sys/*",
	"/tmp/*",
	"/srv/http/*",
	"/var/cache/fontconfig/*",
	"/var/cache/pacman/pkg/*",
	"/var/log/*",
	"/var/spool/*",
	"/var/tmp/*"
};


typedef struct {
	const char *  root_path;
	const char *  db_path;
	bool          ignore_md5;
	bool          ignore_mode;
	bool          ignore_uid;
	bool          ignore_gid;
	alpm_list_t * ignore_patterns;

	// color output
	const char *  RED;
	const char *  GREEN;
	const char *  YELLOW;
	const char *  RESET;
} options_t;


static void list_untracked_files(struct filesystem_entry_t * parent, size_t * counter, options_t * opts) {
	if(filesystem_entry_has_children(parent)) {
		struct filesystem_entry_t * child = filesystem_entry_get_first_child(parent);
		while(child != NULL) {
			if(filesystem_entry_get_user_data(child) == NULL) {
				char * path = filesystem_entry_get_path(child);

				bool ignore = false;
				for(alpm_list_t * it = opts->ignore_patterns; it != NULL; it = alpm_list_next(it)) {
					const char * pattern = (const char *)it->data;
					if(fnmatch(pattern, path, FNM_PATHNAME | FNM_LEADING_DIR) == 0) {
						ignore = true;
						break;
					}
				}

				if(!ignore) {
					if(counter != NULL)
						(*counter)++;
					printf("%s[untracked]%s %s%s\n", opts->GREEN, opts->RESET, path, filesystem_entry_is_directory(child) ? "/" : "");
				}

				free(path);
			}
			else
				list_untracked_files(child, counter, opts);

			child = filesystem_entry_get_next(child);
		}
	}
}


static const char * filesystem_entry_get_type_string(struct filesystem_entry_t * entry) {
	     if(filesystem_entry_is_regular_file (entry)) return "file";
	else if(filesystem_entry_is_directory    (entry)) return "dir";
	else if(filesystem_entry_is_symbolic_link(entry)) return "link";
	else if(filesystem_entry_is_fifo         (entry)) return "fifo";
	else if(filesystem_entry_is_block_device (entry)) return "block";
	else if(filesystem_entry_is_char_device  (entry)) return "char";
	else if(filesystem_entry_is_socket       (entry)) return "socket";
	else                                              return "unknown";
}


static int md5sum(const char * path, char * result) {
	MD5_CTX ctx;
	MD5_Init(&ctx);

	FILE * fp = fopen(path, "r");
	if(fp == NULL) {
		perror("file open");
		return -1;
	}

	while(!feof(fp)) {
		char buffer[4096];
		int result = fread(buffer, 1, sizeof(buffer), fp);
		if(result == 0)
			break;
		MD5_Update(&ctx, buffer, result);
	}

	fclose(fp);

	unsigned char checksum[16];
	MD5_Final(checksum, &ctx);

	for(int i = 0; i < 16; i++)
		sprintf(result + 2 * i, "%02x", checksum[i]);
	result[32] = '\0';

	return 0;
}


static bool perform_diff(const char * path, struct mtree_entry_t * db_entry, struct filesystem_entry_t * fs_entry, options_t * opts) {
	const char * db_type = mtree_entry_get_keyword(db_entry, MTREE_KEYWORD_TYPE);
	const char * fs_type = filesystem_entry_get_type_string(fs_entry);
	if(strcmp(db_type, fs_type) != 0) {
		printf("%s[modified]%s  type %s != %s: %s\n", opts->YELLOW, opts->RESET, db_type, fs_type, path);
		return true;
	}

	if(!opts->ignore_mode) {
		const char * db_mode = mtree_entry_get_keyword(db_entry, MTREE_KEYWORD_MODE);
		char         fs_mode[64];
		snprintf(fs_mode, sizeof(fs_mode), "%o", filesystem_entry_get_mode(fs_entry) & 07777);
		if(strcmp(db_mode, fs_mode) != 0) {
			printf("%s[modified]%s  mode %s != %s: %s\n", opts->YELLOW, opts->RESET, db_mode, fs_mode, path);
			return true;
		}
	}

	if(!opts->ignore_uid) {
		const char * db_uid = mtree_entry_get_keyword(db_entry, MTREE_KEYWORD_UID);
		char         fs_uid[64];
		snprintf(fs_uid, sizeof(fs_uid), "%u", filesystem_entry_get_uid(fs_entry));
		if(strcmp(db_uid, fs_uid) != 0) {
			printf("%s[modified]%s  uid %s != %s: %s\n", opts->YELLOW, opts->RESET, db_uid, fs_uid, path);
			return true;
		}
	}

	if(!opts->ignore_gid) {
		const char * db_gid = mtree_entry_get_keyword(db_entry, MTREE_KEYWORD_GID);
		char         fs_gid[64];
		snprintf(fs_gid, sizeof(fs_gid), "%u", filesystem_entry_get_gid(fs_entry));
		if(strcmp(db_gid, fs_gid) != 0) {
			printf("%s[modified]%s  gid %s != %s: %s\n", opts->YELLOW, opts->RESET, db_gid, fs_gid, path);
			return true;
		}
	}

	if(filesystem_entry_is_regular_file(fs_entry)) {
		const char * db_size = mtree_entry_get_keyword(db_entry, MTREE_KEYWORD_SIZE);
		char         fs_size[64];
		snprintf(fs_size, sizeof(fs_size), "%jd", (intmax_t)filesystem_regular_file_get_size(fs_entry));
		if(strcmp(db_size, fs_size) != 0) {
			printf("%s[modified]%s  size %s != %s: %s\n", opts->YELLOW, opts->RESET, db_size, fs_size, path);
			return true;
		}

		if(!opts->ignore_md5) {
			const char * db_md5checksum = mtree_entry_get_keyword(db_entry, MTREE_KEYWORD_MD5DIGEST);
			char         fs_md5checksum[33];
			if(md5sum(path, fs_md5checksum) == 0 && strcmp(db_md5checksum, fs_md5checksum) != 0) {
				printf("%s[modified]%s  md5 %s != %s: %s\n", opts->YELLOW, opts->RESET, db_md5checksum, fs_md5checksum, path);
				return true;
			}
		}
	}
	else if(filesystem_entry_is_symbolic_link(fs_entry)) {
		const char * db_link = mtree_entry_get_keyword(db_entry, MTREE_KEYWORD_LINK);
		const char * fs_link = filesystem_symbolic_link_get_target(fs_entry);
		if(strcmp(db_link, fs_link) != 0) {
			printf("%s[modified]%s  link %s != %s: %s\n", opts->YELLOW, opts->RESET, db_link, fs_link, path);
			return true;
		}
	}

	return false;
}


int main(int argc, char ** argv) {
	// process command line arguments
	options_t opts;
	opts.root_path       = default_root;
	opts.db_path         = default_db_path;
	opts.ignore_md5      = false;
	opts.ignore_mode     = false;
	opts.ignore_uid      = false;
	opts.ignore_gid      = false;
	opts.ignore_patterns = NULL;
	opts.RED             = "";
	opts.GREEN           = "";
	opts.YELLOW          = "";
	opts.RESET           = "";

	bool no_default_ignore = false;
	bool no_color          = false;
	bool print_usage       = false;
	bool print_version     = false;

	while(true) {
		int option_index = 0;
		static struct option long_options[] = {
			{ "root",               required_argument, NULL,  0 },
			{ "db",                 required_argument, NULL,  1 },
			{ "ignore-md5",         no_argument,       NULL,  2 },
			{ "ignore-mode",        no_argument,       NULL,  3 },
			{ "ignore-uid",         no_argument,       NULL,  4 },
			{ "ignore-gid",         no_argument,       NULL,  5 },
			{ "ignore",             required_argument, NULL,  6 },
			{ "no-color",           no_argument,       NULL,  7 },
			{ "no-default-ignores", no_argument,       NULL,  8 },
			{ "help",               no_argument,       NULL,  9 },
			{ "version",            no_argument,       NULL, 10 },
			{ 0, 0, 0, 0 }
		};
		int c = getopt_long(argc, argv, "", long_options, &option_index);
		if(c == -1)
			break;

		switch(c) {
			case   0: opts.root_path       = optarg;                                      break; // --root
			case   1: opts.db_path         = optarg;                                      break; // --db
			case   2: opts.ignore_md5      = true;                                        break; // --ignore-md5
			case   3: opts.ignore_mode     = true;                                        break; // --ignore-mode
			case   4: opts.ignore_uid      = true;                                        break; // --ignore-uid
			case   5: opts.ignore_gid      = true;                                        break; // --ignore-gid
			case   6: opts.ignore_patterns = alpm_list_add(opts.ignore_patterns, optarg); break; // --ignore
			case   7: no_color             = true;                                        break; // --no-color
			case   8: no_default_ignore    = true;                                        break; // --no-default-ignores
			case   9: print_usage          = true;                                        break; // --help
			case  10: print_version        = true;                                        break; // --version
			case '?': exit(EXIT_FAILURE);
			default:  break;
		}
	}

	if(optind < argc)
		print_usage = true;

	if(print_usage) {
		printf("Usage: %s [OPTION]...\n", basename(argv[0]));
		printf("Perform a full diff between all pacman packages and the file system.\n");
		printf("\n");
		printf("Options:\n");
		printf("  --db <path>           pacman db path (default %s)\n", default_db_path);
		printf("  --ignore <pattern>    ignore all entries matching this pattern\n");
		printf("  --ignore-md5          don't compare md5 checksums\n");
		printf("  --ignore-mode         don't compare modes\n");
		printf("  --ignore-gid          don't compare gids\n");
		printf("  --ignore-uid          don't compare uids\n");
		printf("  --no-color            disable colors in output\n");
		printf("  --no-default-ignores  don't ignore anything by default\n");
		printf("  --root <path>         installation root (default %s)\n", default_root);
		printf("  --help                display this help and exit\n");
		printf("  --version             output version information and exit\n");
		printf("\n");
		printf("Paths ignored by default (disable via --no-default-ignores):\n");
		for(size_t i = 0; i < sizeof(default_ignores) / sizeof(default_ignores[0]); i++)
			printf("  %s\n", default_ignores[i]);
		exit(EXIT_SUCCESS);
	}

	if(print_version) {
		printf("%s %s\n", NAME, VERSION);
		exit(EXIT_SUCCESS);
	}

	if(isatty(fileno(stdout)) && !no_color) {
		opts.RED    = "\x1b[31m";
		opts.GREEN  = "\x1b[32m";
		opts.YELLOW = "\x1b[33m";
		opts.RESET  = "\x1b[0m";
	}

	if(!no_default_ignore) {
		for(size_t i = 0; i < sizeof(default_ignores) / sizeof(default_ignores[0]); i++)
			opts.ignore_patterns = alpm_list_add(opts.ignore_patterns, (void *)default_ignores[i]);
	}

	// collect some stats
	size_t counter_untracked_files = 0,
	       counter_tracked_files   = 0,
	       counter_missing_files   = 0,
	       counter_modified_files  = 0;

	// initialize alpm library
	alpm_errno_t err;
	alpm_handle_t * handle = alpm_initialize(opts.root_path, opts.db_path, &err);
	if(handle == NULL) {
		fprintf(stderr, "error: %s\n", alpm_strerror(err));
		return 1;
	}

	// open the local database containing a list of all currently installed packages
	alpm_db_t * local_db = alpm_get_localdb(handle);
	assert(local_db != NULL);

	// initialize the filesystem handle
	struct filesystem_t * filesystem = filesystem_open();

	// create an initial buffer that will be re-used for all mtree files
	// it will grow on demand
	unsigned int allocated = 4096;
	char *       buffer    = (char *)malloc(allocated);

	// process the mtree files for all installed packages
	for(alpm_list_t * it = alpm_db_get_pkgcache(local_db); it != NULL; it = alpm_list_next(it)) {
		alpm_pkg_t * pkg = it->data;

		// for now i don't know a better way to locate the mtree file than to use a hardcoded location
		char * mtree_filepath;
		asprintf(&mtree_filepath, "%slocal/%s-%s/mtree", opts.db_path, alpm_pkg_get_name(pkg), alpm_pkg_get_version(pkg));

		// the mtree files, if existant, is gzipped
		int size = read_gzip_file(mtree_filepath, &buffer, &allocated);
		if(size < 0) {
			fprintf(stderr, "error: Package `%s-%s' does not have an mtree file. This happens for very old packages. Please update or rebuild this package and try again.\n", alpm_pkg_get_name(pkg), alpm_pkg_get_version(pkg));
			continue;
		}

		// get a list of all entries from the mtree file
		alpm_list_t * entries = mtree_parse(buffer);
		assert(entries != NULL);

		for(alpm_list_t * it_entry = entries; it_entry != NULL; it_entry = alpm_list_next(it_entry)) {
			// TODO compare all entries with the filesystem
			struct mtree_entry_t * db_entry = it_entry->data;
			assert(db_entry != NULL);

			const char * filepath = mtree_entry_get_filepath(db_entry);
			if(strcmp(filepath, "/.PKGINFO") == 0 || strcmp(filepath, "/.INSTALL") == 0 || strcmp(filepath, "/.CHANGELOG") == 0)
				continue;

			struct filesystem_entry_t * fs_entry = filesystem_get_path(filesystem, filepath);
			if(fs_entry == NULL) {
				printf("%s[missing]%s   %s\n", opts.RED, opts.RESET, filepath);
				counter_missing_files++;
				continue;
			}

			// mark the filesystem entry as 'tracked'
			if(filesystem_entry_get_user_data(fs_entry) == NULL) {
				counter_tracked_files++;
				filesystem_entry_set_user_data(fs_entry, malloc(1));
			}

			if(perform_diff(filepath, db_entry, fs_entry, &opts))
				counter_modified_files++;
		}

		// cleanup
		alpm_list_free_inner(entries, (alpm_list_fn_free)mtree_entry_destroy);
		alpm_list_free(entries);
		free(mtree_filepath);
	}

	// all tracked files have been marked, so we can list all unmarked files as untracked
	struct filesystem_entry_t * entry = filesystem_get_path(filesystem, "/");
	list_untracked_files(entry, &counter_untracked_files, &opts);

	// print some stats
	printf("%8zu tracked\n",   counter_tracked_files);
	printf("%8zu untracked\n", counter_untracked_files);
	printf("%8zu missing\n",   counter_missing_files);
	printf("%8zu modified\n",  counter_modified_files);

	// release all handles
	filesystem_close(filesystem, free);
	alpm_release(handle);

	return 0;
}
