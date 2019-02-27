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

#include "hostprogs.h"
#include "hostprogs_lib.h"
#include "microfs_fs.h"

#include "libinfo_zstd.h"

#ifdef HOSTPROGS_LIB_ZSTD

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <zstd.h>

struct hostprog_lib_zstd_data {
	int d_compression;
	ZSTD_CCtx* d_cctx;
};

static int hostprog_lib_zstd_init(void** data, __u32 blksz)
{
	struct hostprog_lib_zstd_data* zstd_data;
	
	(void)blksz;
	
	if (!(*data = zstd_data = malloc(sizeof(*zstd_data)))) {
		goto err_mem;
	}
	
	zstd_data->d_compression = 15;
	zstd_data->d_cctx = ZSTD_createCCtx();
	if (zstd_data->d_cctx == NULL) {
		goto err_mem;
	}
	
	return 0;

err_mem:
	errno = ENOMEM;
	return -1;
}


static void hostprog_lib_zstd_mk_usage(FILE* const dest)
{
	fprintf(dest,
		" compression=<int>     select compression level\n"
	);
}

static int hostprog_lib_zstd_mk_option(void* data,
	const char* name, const char* value)
{
	struct hostprog_lib_zstd_data* zstd_data = data;
	
	if (strcmp(name, "compression") == 0) {
		if (!value) {
			goto err_args;
		}
		opt_strtolx(l, "compression", value, zstd_data->d_compression);
		if (zstd_data->d_compression < 1 || zstd_data->d_compression > ZSTD_maxCLevel()) {
			goto err_args;
		}
		return 0;
	}

err_args:
	errno = EINVAL;
	return -1;
}

static int hostprog_lib_zstd_compress(void* data, void* destbuf, __u32* destbufsz,
	void* srcbuf, __u32 srcbufsz, int* implerr)
{
	struct hostprog_lib_zstd_data* zstd_data = data;

	const size_t result = ZSTD_compressCCtx(zstd_data->d_cctx,
		destbuf, *destbufsz, srcbuf, srcbufsz, zstd_data->d_compression);
	
	if (ZSTD_isError(result)) {
		*implerr = (int)result;
		*destbufsz = 0;
	} else {
		*implerr = 0;
		*destbufsz = result;
	}
	return *implerr == 0 ? 0 : -1;
}

static int hostprog_lib_zstd_decompress(void* data, void* destbuf, __u32* destbufsz,
	void* srcbuf, __u32 srcbufsz, int* implerr)
{
	(void)data;
	
	const size_t result = ZSTD_decompress(destbuf, *destbufsz, srcbuf, srcbufsz);

	if (ZSTD_isError(result)) {
		*implerr = -1;
		*destbufsz = 0;
	} else {
		*implerr = 0;
		*destbufsz = result;
	}
	return *implerr == 0 ? 0 : -1;
}

static __u32 hostprog_lib_zstd_upperbound(void* data, __u32 size)
{
	(void)data;
	
	return ZSTD_compressBound(size);
}

static const char* hostprog_lib_zstd_strerror(void* data, int implerr)
{
	(void)data;
	return ZSTD_getErrorName((size_t)implerr);
}

const struct hostprog_lib hostprog_lib_zstd = {
	.hl_info = &libinfo_zstd,
	.hl_compiled = 1,
	.hl_init = hostprog_lib_zstd_init,
	.hl_mk_usage = hostprog_lib_zstd_mk_usage,
	.hl_mk_option = hostprog_lib_zstd_mk_option,
	.hl_mk_dd = hostprog_lib_mk_dd,
	.hl_ck_dd = hostprog_lib_ck_dd,
	.hl_compress = hostprog_lib_zstd_compress,
	.hl_decompress = hostprog_lib_zstd_decompress,
	.hl_upperbound = hostprog_lib_zstd_upperbound,
	.hl_strerror = hostprog_lib_zstd_strerror
};

#else

const struct hostprog_lib hostprog_lib_zstd = {
	.hl_info = &libinfo_zstd,
	.hl_compiled = 0
};

#endif

