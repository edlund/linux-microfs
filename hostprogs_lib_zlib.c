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

#include "hostprogs_lib.h"
#include "microfs_constants.h"
#include "microfs_flags.h"

#include "libinfo_zlib.h"

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <zlib.h>

__u32 hostprog_lib_zlib_crc32(char* data, __u64 sz)
{
	__u32 crc = crc32(0L, Z_NULL, 0);
	return crc32(crc, (Bytef*)data, sz);
}

static int hostprog_lib_zlib_init(void** data, __u32 blksz)
{
	(void)blksz;
	
	if (!(*data = malloc(sizeof(int)))) {
		errno = ENOMEM;
		return -1;
	}
	
	*((int*)*data) = Z_DEFAULT_COMPRESSION;
	
	return 0;
}

static int hostprog_lib_zlib_mk_usage(FILE* const dest)
{
	fprintf(dest,
		" compression=<str>     select compression level (default, size, speed, none)\n"
	);
	return 0;
}

static int hostprog_lib_zlib_mk_option(void* data,
	const char* name, const char* value)
{
	int* compression = data;
	
	if (strcmp(name, "compression") == 0) {
		if (!value) {
			goto err_args;
		} else if (strcmp(value, "default") == 0) {
			*compression = Z_DEFAULT_COMPRESSION;
		} else if (strcmp(value, "size") == 0) {
			*compression = Z_BEST_COMPRESSION;
		} else if (strcmp(value, "speed") == 0) {
			*compression = Z_BEST_SPEED;
		} else if (strcmp(value, "none") == 0) {
			*compression = Z_NO_COMPRESSION;
		} else {
			goto err_args;
		}
		return 0;
	}
err_args:
	errno = EINVAL;
	return -1;
}

static int hostprog_lib_zlib_compress(void* data, void* destbuf, __u32* destbufsz,
	void* srcbuf, __u32 srcbufsz, int* implerr)
{
	*implerr = compress2((Bytef*)destbuf, (uLongf*)destbufsz,
		(Bytef*)srcbuf, (uLongf)srcbufsz, *(int*)data);
	return *implerr == Z_OK? 0: -1;
}

static int hostprog_lib_zlib_decompress(void* data, void* destbuf, __u32* destbufsz,
	void* srcbuf, __u32 srcbufsz, int* implerr)
{
	(void)data;
	
	*implerr = uncompress((Bytef*)destbuf, (uLongf*)destbufsz,
		(Bytef*)srcbuf, (uLongf)srcbufsz);
	return *implerr == Z_OK? 0: -1;
}

static __u32 hostprog_lib_zlib_upperbound(void* data, __u32 size)
{
	(void)data;
	
	return compressBound(size);
}

static const char* hostprog_lib_zlib_strerror(void* data, int implerr)
{
	(void)data;
	
	return zError(implerr);
}

const struct hostprog_lib hostprog_lib_zlib = {
	.hl_info = &libinfo_zlib,
	.hl_compiled = 1,
	.hl_init = hostprog_lib_zlib_init,
	.hl_mk_usage = hostprog_lib_zlib_mk_usage,
	.hl_mk_option = hostprog_lib_zlib_mk_option,
	.hl_compress = hostprog_lib_zlib_compress,
	.hl_decompress = hostprog_lib_zlib_decompress,
	.hl_upperbound = hostprog_lib_zlib_upperbound,
	.hl_strerror = hostprog_lib_zlib_strerror
};

