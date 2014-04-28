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

#include "libinfo_xz.h"

#ifdef HOSTPROGS_LIB_XZ

#include <errno.h>
#include <stdlib.h>

#include <lzma.h>

static int hostprog_lib_xz_init(void** data, __u32 blksz)
{
	(void)data;
	(void)blksz;
	return 0;
}

/* @FIXME: Filter handling.
 * @FIXME: Dictionary size.
 */
static int hostprog_lib_xz_compress(void* data, void* destbuf, __u32* destbufsz,
	void* srcbuf, __u32 srcbufsz, int* implerr)
{
	(void)data;
	
	size_t destbufpos = 0;
	lzma_options_lzma opts;
	
	*implerr = lzma_lzma_preset(&opts, LZMA_PRESET_DEFAULT);
	if (*implerr) {
		goto err;
	}
	opts.dict_size = LIBINFO_XZ_DICTSZ;
	
	lzma_filter filters[] = {
		{ .id = LZMA_FILTER_X86, .options = NULL  },
		{ .id = LZMA_FILTER_LZMA2, .options = &opts },
		{ .id = LZMA_VLI_UNKNOWN, .options = NULL  }
	};
	
	*implerr = lzma_stream_buffer_encode(filters, LZMA_CHECK_CRC32,
		NULL, srcbuf, srcbufsz, destbuf, &destbufpos, *destbufsz);
	*destbufsz = destbufpos;
err:
	return *implerr == LZMA_OK? 0: -1;
}

static int hostprog_lib_xz_decompress(void* data, void* destbuf, __u32* destbufsz,
	void* srcbuf, __u32 srcbufsz, int* implerr)
{
	(void)data;
	
	size_t srcbufpos = 0;
	size_t destbufpos = 0;
	
	uint64_t memlimit = 1 << 26;
	
	*implerr = lzma_stream_buffer_decode(&memlimit, 0, NULL,
			srcbuf, &srcbufpos, srcbufsz, destbuf, &destbufpos, *destbufsz);
	*destbufsz = destbufpos;
	return *implerr == LZMA_OK && srcbufsz == srcbufpos? 0: -1;
}

static __u32 hostprog_lib_xz_upperbound(void* data, __u32 size)
{
	(void)data;
	
	return lzma_stream_buffer_bound(size);
}

static const char* hostprog_lib_xz_strerror(void* data, int implerr)
{
	(void)data;
	
	switch (implerr) {
		
#define XZ_STRERROR(Err) \
	case Err: return #Err;
		
		XZ_STRERROR(LZMA_OK);
		XZ_STRERROR(LZMA_FORMAT_ERROR);
		XZ_STRERROR(LZMA_OPTIONS_ERROR);
		XZ_STRERROR(LZMA_DATA_ERROR);
		XZ_STRERROR(LZMA_NO_CHECK);
		XZ_STRERROR(LZMA_UNSUPPORTED_CHECK);
		XZ_STRERROR(LZMA_MEM_ERROR);
		XZ_STRERROR(LZMA_MEMLIMIT_ERROR);
		XZ_STRERROR(LZMA_BUF_ERROR);
		XZ_STRERROR(LZMA_PROG_ERROR);
		
#undef XZ_STRERROR
		
		default:
			return "Unknown error";
	}
}

const struct hostprog_lib hostprog_lib_xz = {
	.hl_info = &libinfo_xz,
	.hl_compiled = 1,
	.hl_init = hostprog_lib_xz_init,
	.hl_compress_usage = hostprog_lib_compress_usage,
	.hl_compress_option = hostprog_lib_compress_option,
	.hl_compress = hostprog_lib_xz_compress,
	.hl_decompress = hostprog_lib_xz_decompress,
	.hl_upperbound = hostprog_lib_xz_upperbound,
	.hl_strerror = hostprog_lib_xz_strerror
};

#else

const struct hostprog_lib hostprog_lib_xz = {
	.hl_info = &libinfo_xz,
	.hl_compiled = 0
};

#endif


