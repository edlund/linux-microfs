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
#include "microfs_fs.h"

#include "libinfo_lz4.h"

#ifdef HOSTPROGS_LIB_LZ4

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <lz4.h>
#include <lz4hc.h>

typedef int (*hostprog_lib_lz4_compressor)(const char* source, char* dest,
	int inputSize, int maxOutputSize);

struct hostprog_lib_lz4_data {
	hostprog_lib_lz4_compressor d_compressor;
};

static int hostprog_lib_lz4_init(void** data, __u32 blksz)
{
	struct hostprog_lib_lz4_data* lz4_data;
	
	(void)blksz;
	
	if (!(*data = lz4_data = malloc(sizeof(*lz4_data)))) {
		errno = ENOMEM;
		return -1;
	}
	
	lz4_data->d_compressor = LZ4_compress_limitedOutput;
	
	return 0;
}


static void hostprog_lib_lz4_mk_usage(FILE* const dest)
{
	fprintf(dest,
		" compression=<str>     select compression level (default, high)\n"
	);
}

static int hostprog_lib_lz4_mk_option(void* data,
	const char* name, const char* value)
{
	struct hostprog_lib_lz4_data* lz4_data = data;
	
	if (strcmp(name, "compression") == 0) {
		if (!value) {
			goto err_args;
		} else if (strcmp(value, "default") == 0) {
			lz4_data->d_compressor = LZ4_compress_limitedOutput;
		} else if (strcmp(value, "high") == 0) {
			lz4_data->d_compressor = LZ4_compressHC_limitedOutput;
		} else {
			goto err_args;
		}
		return 0;
	}
err_args:
	errno = EINVAL;
	return -1;
}


static int hostprog_lib_lz4_compress(void* data, void* destbuf, __u32* destbufsz,
	void* srcbuf, __u32 srcbufsz, int* implerr)
{
	struct hostprog_lib_lz4_data* lz4_data = data;
	
	*implerr = lz4_data->d_compressor(srcbuf, destbuf, srcbufsz, *destbufsz);
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
	.hl_mk_usage = hostprog_lib_lz4_mk_usage,
	.hl_mk_option = hostprog_lib_lz4_mk_option,
	.hl_mk_dd = hostprog_lib_mk_dd,
	.hl_ck_dd = hostprog_lib_ck_dd,
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

