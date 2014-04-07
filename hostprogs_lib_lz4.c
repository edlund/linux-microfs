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

#include "libinfo_lz4.h"

#ifdef HOSTPROGS_LIB_LZ4

#include <lz4.h>

static int hostprog_lib_lz4_init(void** data, __u32 blksz)
{
	(void)data;
	(void)blksz;
	
	return 0;
}

static int hostprog_lib_lz4_compress(void* data, void* destbuf, __u32* destbufsz,
	void* srcbuf, __u32 srcbufsz, int* implerr)
{
	(void)data;
	
	*implerr = LZ4_compress_limitedOutput(srcbuf, destbuf,
		srcbufsz, *destbufsz);
	*destbufsz = *implerr? *implerr: 0;
	return *implerr != 0? 0: -1;
}

static int hostprog_lib_lz4_decompress(void* data, void* destbuf, __u32* destbufsz,
	void* srcbuf, __u32 srcbufsz, int* implerr)
{
	(void)data;
	
	*implerr = LZ4_decompress_safe_partial(srcbuf, destbuf,
		srcbufsz, *destbufsz, *destbufsz);
	*destbufsz = *implerr? *implerr: 0;
	return *implerr != 0? 0: -1;
}

static __u32 hostprog_lib_lz4_upperbound(void* data, __u32 size)
{
	(void)data;
	
	return LZ4_compressBound(size);
}

static const char* hostprog_lib_lz4_strerror(void* data, int implerr)
{
	(void)data;
	(void)implerr;
	return "unknown error";
}

const struct hostprog_lib hostprog_lib_lz4 = {
	.hl_info = &libinfo_lz4,
	.hl_compiled = 1,
	.hl_init = hostprog_lib_lz4_init,
	.hl_compress = hostprog_lib_lz4_compress,
	.hl_decompress = hostprog_lib_lz4_decompress,
	.hl_upperbound = hostprog_lib_lz4_upperbound,
	.hl_strerror = hostprog_lib_lz4_strerror
};

#else

const struct hostprog_lib hostprog_lib_lz4 = {
	.hl_info = &libinfo_lz4,
	.hl_compiled = 0
};

#endif

