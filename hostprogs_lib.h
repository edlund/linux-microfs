/* microfs - Minimally Improved Compressed Read Only File System
 * Copyright (C) 2014 Erik Edlund <erik.edlund@32767.se>
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

#ifndef HOSTPROGS_LIB_H
#define HOSTPROGS_LIB_H

#include <linux/types.h>

#include "libinfo.h"

struct hostprog_lib {
	/* Library info. */
	const struct libinfo* hl_info;
	/* Library support compiled? */
	const int hl_compiled;
	/* Compression library initialization, if necessary. */
	int (*hl_init)(void** data, __u32 blksz);
	/* Compress data. */
	int (*hl_compress)(void* data, void* destbuf, __u32* destbufsz,
		void* srcbuf, __u32 srcbufsz, int* implerr);
	/* Decompress data. */
	int (*hl_decompress)(void* data, void* destbuf, __u32* destbufsz,
		void* srcbuf, __u32 srcbufsz, int* implerr);
	/* Get the max size that data of size %size can compress to. */
	__u32 (*hl_upperbound)(void* data, __u32 size);
	/* Get a human readable description of the error (maybe). */
	const char* (*hl_strerror)(void* data, int implerr);
};

extern const struct hostprog_lib hostprog_lib_zlib;
extern const struct hostprog_lib hostprog_lib_lz4;
extern const struct hostprog_lib hostprog_lib_lzo;

const struct hostprog_lib* hostprog_lib_find_byid(const int id);
const struct hostprog_lib* hostprog_lib_find_byname(const char* name);

const struct hostprog_lib** hostprog_lib_all(void);

__u32 hostprog_lib_zlib_crc32(char* data, __u64 sz);

#endif
