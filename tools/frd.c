/* microfs - Minimally Improved Compressed Read Only File System
 * Copyright (C) 2013 Erik Edlund <erik.edlund@32767.se>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "../microfs.h"
#include "../hostprogs.h"

#define FRD_OPTIONS "hverRmNs:b:i:"

struct readoptions {
	/* Read file data blocks sequentially. */
	int ro_seqread;
	/* Read the given files sequentially. */
	int ro_seqfiles;
	/* srand()-seed. */
	unsigned int ro_seed;
	/* Buffer to read data into. */
	unsigned char* ro_blkbuf;
	/* Read data in chunks of this size. */
	size_t ro_blksz;
	/* Files which should be read. */
	struct hostprog_stack* ro_files;
	/* File data offsets. */
	struct hostprog_stack* ro_offsets;
	/* Callback to handle a single path. */
	void (*ro_handle)(struct readoptions* const rdopts, const char* path);
};

static void handle_stat(struct readoptions* const rdopts, const char* path)
{
	(void)rdopts;
	
	struct stat st;
	if (stat(path, &st) < 0) {
		warning("fail to stat \"%s\": %s, it will be skipped",
			path, strerror(errno));
	} else {
		message(VERBOSITY_1, " st %c %s", nodtype(st.st_mode), path);
	}
}

static void handle_read(struct readoptions* const rdopts, const char* path)
{
	struct stat st;
	if (stat(path, &st) < 0) {
		warning("fail to stat \"%s\": %s, it will be skipped",
			path, strerror(errno));
	} else if (S_ISREG(st.st_mode) && st.st_size) {
		int rdfd = open(path, O_RDONLY, 0);
		if (rdfd < 0)
			error("failed to open path \"%s\": %s", path, strerror(errno));
		
		const hostprog_stack_int_t max = HOSTPROG_STACK_INT_T_MAX;
		const hostprog_stack_int_t sz = st.st_size;
		const hostprog_stack_int_t blks = i_blkptrs(sz, rdopts->ro_blksz);
		
		hostprog_stack_int_t offset = (blks - 1) * rdopts->ro_blksz;
		
		if (offset > max) {
			error("achievement unlocked: the file \"%s\" is too big,"
				" offset=%td, max=%td", path, offset, max);
		}
		
		while (offset >= 0) {
			if (hostprog_stack_push(rdopts->ro_offsets, offset) < 0) {
				error("failed to push offset %td to the offset stack: %s",
					offset, strerror(errno));
			}
			offset -= rdopts->ro_blksz;
		}
		
		if (!rdopts->ro_seqread) {
			if (fykshuffle(rdopts->ro_offsets->st_slots,
					hostprog_stack_size(rdopts->ro_offsets)) < 0)
				error("failed to shuffle the offset stack: %s", strerror(errno));
		}
		
		while (hostprog_stack_size(rdopts->ro_offsets)) {
			if (hostprog_stack_pop(rdopts->ro_offsets, &offset) < 0) {
				error("failed to pop an offset off from the offset stack: %s",
					strerror(errno));
			}
			if (lseek(rdfd, offset, SEEK_SET) == ((off_t)-1)) {
				error("failed to set file pointer to offset %td: %s",
					offset, strerror(errno));
			}
			if (read(rdfd, rdopts->ro_blkbuf, rdopts->ro_blksz) < 0) {
				error("failed to read from file: %s", strerror(errno));
			}
		}
		
		close(rdfd);
		
		message(VERBOSITY_1, " rd[%c] %c %s", rdopts->ro_seqread? 's': 'r',
			nodtype(st.st_mode), path);
	} else {
		message(VERBOSITY_1, " skipping %c %s", nodtype(st.st_mode), path);
	}
}

static void walk_paths(struct readoptions* const rdopts)
{
	const int files = hostprog_stack_size(rdopts->ro_files);
	if (!rdopts->ro_seqfiles) {
		if (fykshuffle(rdopts->ro_files->st_slots, files) < 0)
			error("failed to shuffle the file stack: %s", strerror(errno));
	}
	for (int i = 0; i < files; i++) {
		rdopts->ro_handle(rdopts, rdopts->ro_files->st_slots[i]);
	}
}

static void add_paths_from(struct readoptions* const rdopts,
	const char* path)
{
	FILE* list = fopen(path, "r");
	if (!list)
		error("failed to open file list \"%s\": %s", path, strerror(errno));
	
	char* line = NULL;
	size_t length = 0;
	while (getline(&line, &length, list) != -1) {
		length = strlen(line);
		while (length > 0 && isspace(line[length - 1]))
			line[--length] = '\0';
		
		if (hostprog_stack_push(rdopts->ro_files, line) < 0)
			error("failed to push line \"%s\" from \"%s\" to the file stack"
				": %s", line, path, strerror(errno));
		
		line = NULL;
		length = 0;
	}
	
	fclose(list);
}

static void usage(const char* const exe, FILE* const dest)
{
	fprintf(dest,
		"\nUsage: %s [-%s] [infile1 infile2 ... infileN]\n"
		"\nexample: %s /mnt/cdrom1/boot/initrd.img-x.y.z-n-generic\n\n"
		" -h          print this message (to stdout) and quit\n"
		" -v          be more verbose\n"
		" -e          turn warnings into errors\n"
		" -r          read file data blocks in a random order\n"
		" -R          read given files in a random order\n"
		" -N          do NOT read the file content, just stat() the path\n"
		" -s <int>    seed to give to srand()\n"
		" -b <int>    block size to use when reading\n"
		" -i <str>    path to a list over files to read\n"
		" infileX     file(s) to read, see -i\n"
		"\n", exe, FRD_OPTIONS, exe);
	
	exit(dest == stderr? EXIT_FAILURE: EXIT_SUCCESS);
}

int main(int argc, char* argv[])
{
	if (argc < 2)
		usage(argc > 0? argv[0]: "", stderr);
	
	struct readoptions* rdopts = malloc(sizeof(*rdopts));
	if (!rdopts)
		error("failed to allocate read options");
	memset(rdopts, 0, sizeof(*rdopts));
	
	rdopts->ro_seqread = 1;
	rdopts->ro_seqfiles = 1;
	rdopts->ro_seed = time(NULL);
	rdopts->ro_blksz = sysconf(_SC_PAGESIZE);
	rdopts->ro_handle = handle_read;
	
	if (hostprog_stack_create(&rdopts->ro_files, 512, 512) < 0)
		error("failed to create the file stack");
	
	if (hostprog_stack_create(&rdopts->ro_offsets, 4096, 4096) < 0)
		error("failed to create the offset stack");
	
	int option;
	while ((option = getopt(argc, argv, FRD_OPTIONS)) != EOF) {
		switch (option) {
			case 'h':
				usage(argv[0], stdout);
				break;
			case 'v':
				hostprog_verbosity++;
				break;
			case 'e':
				hostprog_werror = 1;
				break;
			case 'r':
				rdopts->ro_seqread = 0;
				break;
			case 'R':
				rdopts->ro_seqfiles = 0;
				break;
			case 'N':
				rdopts->ro_handle = handle_stat;
				break;
			case 's':
				opt_strtolx(ul, option, optarg, rdopts->ro_seed);
				break;
			case 'b':
				opt_strtolx(ul, option, optarg, rdopts->ro_blksz);
				if (!microfs_ispow2(rdopts->ro_blksz))
					error("the block size must be a power of two");
				break;
			case 'i':
				add_paths_from(rdopts, optarg);
				break;
			default:
				/* Ignore it.
				 */
				warning("unrecognized option -%c", option);
				break;
		}
	}
	
	rdopts->ro_blkbuf = malloc(rdopts->ro_blksz);
	if (!rdopts->ro_blkbuf)
		error("failed to allocate the read buffer: %s", strerror(errno));
	
	/* Add command line listed input files (if present).
	 */
	for (int i = optind; i < argc; i++) {
		char* path = strdup(argv[optind]);
		if (!path) {
			error("failed to duplicate path \"%s\": %s", argv[optind],
				strerror(errno));
		} else if (hostprog_stack_push(rdopts->ro_files, path) < 0) {
			error("failed to push \"%s\" to the file stack: %s", path,
				strerror(errno));
		}
	}
	
	if (hostprog_stack_size(rdopts->ro_files) > RAND_MAX)
		error("%d files can at most be handled, but %d files were given",
			RAND_MAX, hostprog_stack_size(rdopts->ro_files));
	
	if (rdopts->ro_seqread == 0 && rdopts->ro_handle == handle_stat)
		warning("-r and -N can not coexist, -N will take priority");
	
	srand(rdopts->ro_seed);
	walk_paths(rdopts);
	
	exit(EXIT_SUCCESS);
}
