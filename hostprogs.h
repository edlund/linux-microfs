/* microfs - Minimally Improved Compressed Read Only File System
 * Copyright (C) 2012 Erik Edlund <erik.edlund@32767.se>
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

#ifndef HOSTPROGS_H
#define HOSTPROGS_H

#if ( \
	!defined(__STDC__) || __STDC__ == 0 || \
	!defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L \
)
#error "An ISO C99 compiler and preprocessor is required."
#endif

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <fcntl.h>
#include <libgen.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#define error(...) \
	do { \
		fprintf(stderr, "!!! error: "); \
		fprintf(stderr, __VA_ARGS__); \
		fprintf(stderr, " (%s:%d)", __FILE__, __LINE__); \
		fprintf(stderr, "\n"); \
		exit(EXIT_FAILURE); \
	} while (0)

#define warning(...) \
	do { \
		fprintf(stderr, "*** warning: "); \
		fprintf(stderr, __VA_ARGS__); \
		fprintf(stderr, " (%s:%d)", __FILE__, __LINE__); \
		fprintf(stderr, "\n"); \
		if (hostprog_werror) \
			error("warnings treated as errors"); \
	} while (0)

enum {
	VERBOSITY_0 = 0,
	VERBOSITY_1,
	VERBOSITY_2
};

#define message(Level, ...) \
	do { \
		if (hostprog_verbosity >= Level) { \
			fprintf(stdout, __VA_ARGS__); \
			fprintf(stdout, "\n"); \
		} \
	} while (0)

#define opt_strtolx(Postfix, Opt, Arg, Var) \
	do { \
		char* endptr; \
		errno = 0; \
		Var = strto##Postfix(Arg, &endptr, 10); \
		if (*endptr != '\0') { \
			error("arg -%c is not an integer (%c=%s)", \
				Opt, Opt, Arg); \
		} else if (errno) { \
			error("arg -%c: %s (%c=%s)", Opt, strerror(errno), \
				Opt, Arg); \
		} \
	} while (0)

extern int hostprog_werror;
extern int hostprog_verbosity;

/* %alphasort() uses %strcoll(), which means that order can
 * change depending on the locale information.
 */
int hostprog_scandirsort(const struct dirent** a, const struct dirent** b);

static inline char nodtype(const mode_t mode)
{
	switch (mode & S_IFMT) {
		case S_IFREG:
			return 'f';
		case S_IFDIR:
			return 'd';
		case S_IFLNK:
			return 'l';
		case S_IFCHR:
			return 'c';
		case S_IFBLK:
			return 'b';
		case S_IFSOCK:
			return 's';
		case S_IFIFO:
			return 'p';
		default:
			return '?';
	}
}

/* A simplistic stack implementation.
 */
struct hostprog_stack {
	/* Next free slot. */
	size_t st_index;
	/* Size of %st_slots. */
	size_t st_size;
	/* Factor to grow %st_slots with. */
	size_t st_growth;
	/* The objects held by the stack. */
	void** st_slots;
};

/* An integer type which can be stored on a stack without
 * a wrapper structure.
 */
typedef ptrdiff_t hostprog_stack_int_t;

int hostprog_stack_create(struct hostprog_stack** const s,
	const size_t size, const size_t growth);
int hostprog_stack_destroy(struct hostprog_stack* const s);
int hostprog_stack_size(const struct hostprog_stack* const s);
int _hostprog_stack_push(struct hostprog_stack* s, void* v);
int _hostprog_stack_top(struct hostprog_stack* const s, void** v);
int _hostprog_stack_pop(struct hostprog_stack* const s, void** v);

#define hostprog_stack_push(Stack, Value) \
	_hostprog_stack_push(Stack, (void*)Value)
#define hostprog_stack_top(Stack, Value) \
	_hostprog_stack_top(Stack, (void**)Value)
#define hostprog_stack_pop(Stack, Value) \
	_hostprog_stack_pop(Stack, (void**)Value)

#define HOSTPROG_PATH_MAXNAMELEN 255

/* A very simplistic filesystem path representation.
 */
struct hostprog_path {
	/* The path, a NULL-terminated string. */
	char* p_path;
	/* Number of bytes allocated for %p_path. */
	size_t p_pathsz;
	/* Number of characters in %p_path. */
	size_t p_pathlen;
	/* Maximum allowed name length. */
	size_t p_maxnamelen;
	/* Minimi growth of %p_path when it runs out space. */
	size_t p_mingrowth;
	/* All path separator indexes (excluding the root dir). */
	struct hostprog_stack* p_separators;
};

int hostprog_path_create(struct hostprog_path** dpath, const char* path,
	const size_t maxnamelen, const size_t mingrowth);
int hostprog_path_append(struct hostprog_path* const dpath, const char* dname);
int hostprog_path_lvls(struct hostprog_path* const dpath);
int hostprog_path_dirname(struct hostprog_path* const dpath);
int hostprog_path_dirnamelvl(struct hostprog_path* const dpath, const int lvl);
int hostprog_path_reset(struct hostprog_path* const dpath);
int hostprog_path_destroy(struct hostprog_path* const dpath);

int hostprog_path_dotdir(const char* dname);

#endif
