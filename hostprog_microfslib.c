/* microfs - Minimally Improved Compressed Read Only File System
 * Copyright (C) 2012, 2013, 2014, 2015, 2016, 2017
 * Erik Edlund <erik.edlund@32767.se>
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

// microfslib - Print supported compression options.

#include "hostprogs.h"
#include "hostprogs_lib.h"

#include <stdio.h>
#include <stdlib.h>

/* getopt() args, see usage().
 */
#define LIB_OPTIONS "hatTc:"

static int test_sz(__u32 blksz)
{
	return (
		blksz == 512 ||
		blksz == 1024 ||
		blksz == 2048 ||
		blksz == 4096 ||
		blksz == 8192 ||
		blksz == 16384 ||
		blksz == 32768 ||
		blksz == 65536 ||
		blksz == 131072 ||
		blksz == 1048576
	);
}

static int quicktest_sz(__u32 blksz)
{
	return (
		blksz == 512 ||
		blksz == 4096 ||
		blksz == 131072 ||
		blksz == 1048576
	);
}

static void usage(const char* const exe, FILE* const dest)
{
	fprintf(dest,
		"\nUsage: %s [-%s]\n"
		"\nexample 1: %s\n\n"
		"\nexample 2: %s -c zlib -b\n\n"
		" -h          print this message (to stdout) and quit\n"
		" -a          list all supported block sizes (for the host)\n"
		" -t          list test block sizes (for the host)\n"
		" -T          list quick test block sizes (for the host)\n"
		" -c <str>    the name of the compression library to use\n"
		"\n", exe, LIB_OPTIONS, exe, exe);
	
	exit(dest == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

static void print_blockszs(const struct hostprog_lib* lib, int (*filter)(__u32))
{
	const __u32 start = lib->hl_info->li_min_blksz == 0
		? sysconf(_SC_PAGESIZE)
		: lib->hl_info->li_min_blksz;
	
	for (__u32 sz = start; sz <= lib->hl_info->li_max_blksz; sz *= 2) {
		if (!filter || (filter && filter(sz)))
			message(VERBOSITY_0, "%u", sz);
	}
}

enum {
	LIST_LIBS = 0,
	LIST_BLKSZS_ALL = 1,
	LIST_BLKSZS_TEST = 2,
	LIST_BLKSZS_QUICKTEST = 3
};

int main(int argc, char* argv[])
{
	if (argc == 0)
		usage("microfslib", stderr);
	
	const char* name = NULL;
	const struct hostprog_lib* lib = NULL;
	
	int action = LIST_LIBS;
	int option;
	while ((option = getopt(argc, argv, LIB_OPTIONS)) != EOF) {
		switch (option) {
			case 'h':
				usage(argv[0], stdout);
				break;
			case 'c':
				name = optarg;
				break;
			case 'a':
				action = LIST_BLKSZS_ALL;
				break;
			case 't':
				action = LIST_BLKSZS_TEST;
				break;
			case 'T':
				action = LIST_BLKSZS_QUICKTEST;
				break;
		}
	}
	
	if (name) {
		if (action == LIST_LIBS) {
			error("missing -a, -t or -T");
		} else if (!(lib = hostprog_lib_find_byname(name))) {
			error("could not find a library named %s", name);
		}
	} else if (action != LIST_LIBS) {
		error("missing -c");
	}
	
	switch (action) {
		case LIST_LIBS:
			for (const struct hostprog_lib** libs = hostprog_lib_all();
					(*libs)->hl_info; libs++) {
				if ((*libs)->hl_compiled)
					message(VERBOSITY_0, "%s", (*libs)->hl_info->li_name);
			}
			break;
		case LIST_BLKSZS_ALL:
			print_blockszs(lib, NULL);
			break;
		case LIST_BLKSZS_TEST:
			print_blockszs(lib, test_sz);
			break;
		case LIST_BLKSZS_QUICKTEST:
			print_blockszs(lib, quicktest_sz);
			break;
		default:
			error("unrecognized action");
			break;
	}
	
	return EXIT_SUCCESS;
}

