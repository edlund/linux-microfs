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

#include <zlib.h>

__u32 hostprog_lib_zlib_crc32(char* data, __u64 sz)
{
	__u32 crc = crc32(0L, Z_NULL, 0);
	return crc32(crc, (Bytef*)data, sz);
}

static int hostprog_lib_zlib_init(void** data, __u32 blksz)
{
	(void)data;
	(void)blksz;
	
	return 0;
}

static int hostprog_lib_zlib_compress(void* data, void* destbuf, __u32* destbufsz,
	void* srcbuf, __u32 srcbufsz, int* implerr)
{
	(void)data;
	
	*implerr = compress((Bytef*)destbuf, (uLongf*)destbufsz,
		(Bytef*)srcbuf, (uLongf)srcbufsz);
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
	.hl_compress = hostprog_lib_zlib_compress,
	.hl_decompress = hostprog_lib_zlib_decompress,
	.hl_upperbound = hostprog_lib_zlib_upperbound,
	.hl_strerror = hostprog_lib_zlib_strerror
};

