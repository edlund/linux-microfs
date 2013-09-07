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
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <fcntl.h>
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

#endif
