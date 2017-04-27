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

#ifndef HOSTPROGS_H
#define HOSTPROGS_H

#if ( \
	!defined(__STDC__) || __STDC__ == 0 || \
	!defined(__STDC_VERSION__) || __STDC_VERSION__ < 201112L \
)
#error "An ISO C11 compiler and preprocessor is required."
#endif

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
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

#include <linux/types.h>

#ifndef HOSTPROG_PRINT_LOCK
#define HOSTPROG_PRINT_LOCK 0
#endif

void hostprog_print_lock(void);
void hostprog_print_unlock(void);

#define do_error(Exit, CanExit, ...) \
	do { \
		if (HOSTPROG_PRINT_LOCK) hostprog_print_lock(); \
		fprintf(stderr, "!!! error: "); \
		fprintf(stderr, __VA_ARGS__); \
		fprintf(stderr, " (%s:%d)", __FILE__, __LINE__); \
		fprintf(stderr, "\n"); \
		if (HOSTPROG_PRINT_LOCK) hostprog_print_unlock(); \
		if (CanExit) \
			Exit; \
	} while (0)

#define error(...) do_error(exit(EXIT_FAILURE), 1, __VA_ARGS__)

#define do_warning(Exit, CanExit, ...) \
	do { \
		if (HOSTPROG_PRINT_LOCK) hostprog_print_lock(); \
		fprintf(stderr, "*** warning: "); \
		fprintf(stderr, __VA_ARGS__); \
		fprintf(stderr, " (%s:%d)", __FILE__, __LINE__); \
		fprintf(stderr, "\n"); \
		if (HOSTPROG_PRINT_LOCK) hostprog_print_unlock(); \
		if (hostprog_werror) \
			do_error(Exit, CanExit, "warnings treated as errors"); \
	} while (0)

#define warning(...) do_warning(exit(EXIT_FAILURE), 1, __VA_ARGS__)

enum {
	VERBOSITY_0 = 0,
	VERBOSITY_1,
	VERBOSITY_2
};

#define message(Level, ...) \
	do { \
		if (hostprog_verbosity >= Level) { \
			if (HOSTPROG_PRINT_LOCK) hostprog_print_lock(); \
			fprintf(stdout, __VA_ARGS__); \
			fprintf(stdout, "\n"); \
			if (HOSTPROG_PRINT_LOCK) hostprog_print_unlock(); \
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

#define max(a, b) \
	({ \
		__typeof__ (a) a_ = a; \
		__typeof__ (b) b_ = b; \
		a_ < b_? b_: a_; \
	})

#define min(a, b) \
	({ \
		__typeof__ (a) a_ = a; \
		__typeof__ (b) b_ = b; \
		a_ < b_? a_: b_; \
	})

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

/* A naive implementation of the Fisher-Yates(-Knuth) shuffle.
 */
int fykshuffle(void** slots, size_t length);

/* Skewed number in [min, max].
 */
static inline int rand_nonuniform_range(int min, int max)
{
    return rand() % (max - min + 1) + min;
}

extern int hostprog_werror;
extern int hostprog_verbosity;

int hostprog_scandirsort(const struct dirent** a, const struct dirent** b);

/* A simplistic stack implementation.
 */
struct hostprog_stack {
	/* Next free slot. */
	__u64 st_index;
	/* Size of %st_slots. */
	__u64 st_size;
	/* Factor to grow %st_slots with. */
	__u64 st_growth;
	/* The objects held by the stack. */
	void** st_slots;
};

/* An integer type which can be stored on a stack without
 * a wrapper structure.
 */
typedef ptrdiff_t hostprog_stack_int_t;

#if __WORDSIZE == 64
#define HOSTPROG_STACK_INT_T_ZERO 0ULL
#else
#define HOSTPROG_STACK_INT_T_ZERO 0UL
#endif
#define HOSTPROG_STACK_INT_T_MAX \
	((hostprog_stack_int_t)(~HOSTPROG_STACK_INT_T_ZERO >> 1))
#define HOSTPROG_STACK_INT_T_MIN \
	(-HOSTPROG_STACK_INT_T_MAX - 1)

int hostprog_stack_create(struct hostprog_stack** const s,
	const __u64 size, const __u64 growth);
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
	__u64 p_pathsz;
	/* Number of characters in %p_path. */
	__u64 p_pathlen;
	/* Maximum allowed name length. */
	__u64 p_maxnamelen;
	/* Minimi growth of %p_path when it runs out space. */
	__u64 p_mingrowth;
	/* All path separator indexes (excluding the root dir). */
	struct hostprog_stack* p_separators;
};

int hostprog_path_create(struct hostprog_path** dpath, const char* path,
	const __u64 maxnamelen, const __u64 mingrowth);
int hostprog_path_append(struct hostprog_path* const dpath, const char* dname);
int hostprog_path_lvls(struct hostprog_path* const dpath);
int hostprog_path_dirname(struct hostprog_path* const dpath);
int hostprog_path_dirnamelvl(struct hostprog_path* const dpath, const int lvl);
int hostprog_path_reset(struct hostprog_path* const dpath);
int hostprog_path_destroy(struct hostprog_path* const dpath);

int hostprog_path_dotdir(const char* dname);

#endif

