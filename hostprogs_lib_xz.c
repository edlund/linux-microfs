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
#include <string.h>

#include <lzma.h>

/* Branch/Call/Jump (BCJ) conversion filter.
 */
struct hostprog_lib_xz_bcj {
	const char* const bcj_name;
	lzma_vli bcj_id;
};

/* BCJ filters available from liblzma.
 */
static struct hostprog_lib_xz_bcj bcjs[] = {
	{ "x86", LZMA_FILTER_X86 },
	{ "powerpc", LZMA_FILTER_POWERPC },
	{ "ia64", LZMA_FILTER_IA64 },
	{ "arm", LZMA_FILTER_ARM },
	{ "armthumb", LZMA_FILTER_ARMTHUMB },
	{ "sparc", LZMA_FILTER_SPARC },
	{ NULL, LZMA_VLI_UNKNOWN }
};

#define BCJS_SLOTS \
	(sizeof(bcjs) / sizeof(struct hostprog_lib_xz_bcj))

struct hostprog_lib_xz_filter {
	uint8_t* f_buf;
	size_t f_bufsz;
	lzma_filter f_backends[3];
};

struct hostprog_lib_xz_data {
	struct hostprog_lib_xz_filter d_filters[BCJS_SLOTS + 1];
	lzma_vli d_blksz;
	lzma_options_lzma d_opts;
};

static int hostprog_lib_xz_init(void** data, __u32 blksz)
{
	struct hostprog_lib_xz_data* xz_data;
	
	(void)blksz;
	
	if (!(*data = xz_data = malloc(sizeof(*xz_data)))) {
		errno = ENOMEM;
		return -1;
	}
	
	memset(xz_data->d_filters, 0, sizeof(xz_data->d_filters));
	lzma_lzma_preset(&xz_data->d_opts, LZMA_PRESET_DEFAULT);
	
	xz_data->d_blksz = blksz;
	xz_data->d_opts.dict_size = LIBINFO_XZ_DICTSZ;
	
	/* Default filter, the user might add more by specifying them
	 * using the -l param for `microfsmki`.
	 */
	xz_data->d_filters[0].f_backends[0].id = LZMA_FILTER_LZMA2;
	xz_data->d_filters[0].f_backends[0].options = &xz_data->d_opts;
	xz_data->d_filters[0].f_backends[1].id = LZMA_VLI_UNKNOWN;
	
	return 0;
}

static int hostprog_lib_xz_mk_usage(FILE* const dest)
{
	fprintf(dest,
		" filter=<str>          add a BCJ conversion filter"
		" (x86, powerpc, ia64, arm, armthumb, sparc)\n"
	);
	return 0;
}

static int hostprog_lib_xz_mk_option(void* data,
	const char* name, const char* value)
{
	struct hostprog_lib_xz_data* xz_data = data;
	
	if (strcmp(name, "filter") == 0) {
		if (!value) {
			errno = EINVAL;
			goto err;
		}
		
		size_t i;
		size_t j;
		int bcj_found = 0;
		int filter_found = 0;
		
		for (i = 0; i < BCJS_SLOTS; i++) {
			if (strcmp(bcjs[i].bcj_name, value) == 0) {
				bcj_found = 1;
				break;
			}
		}
		
		for (j = 1; j < BCJS_SLOTS; j++) {
			if (xz_data->d_filters[j].f_backends[0].id == 0) {
				filter_found = 1;
				break;
			}
		}
		
		if (!bcj_found || !filter_found) {
			errno = EINVAL;
			goto err;
		}
		
		xz_data->d_filters[j].f_backends[0].id = bcjs[i].bcj_id;
		xz_data->d_filters[j].f_backends[1].id = LZMA_FILTER_LZMA2;
		xz_data->d_filters[j].f_backends[1].options = &xz_data->d_opts;
		xz_data->d_filters[j].f_backends[2].id = LZMA_VLI_UNKNOWN;
		
		__u32 upperbound = lzma_stream_buffer_bound(xz_data->d_blksz);
		
		xz_data->d_filters[j].f_bufsz = 0;
		xz_data->d_filters[j].f_buf = malloc(upperbound);
		if (!xz_data->d_filters[j].f_buf) {
			errno = ENOMEM;
			goto err;
		}
		return 0;
	}
	
	/* It is possible to get here for unknown option names.
	 */
	errno = EINVAL;
err:
	return -1;
}

static int hostprog_lib_xz_compress(void* data, void* destbuf, __u32* destbufsz,
	void* srcbuf, __u32 srcbufsz, int* implerr)
{
	struct hostprog_lib_xz_data* xz_data = data;
	struct hostprog_lib_xz_filter* best_filter = NULL;
	
	xz_data->d_filters[0].f_buf = destbuf;
	
	for (int i = 0; xz_data->d_filters[i].f_buf; i++) {
		struct hostprog_lib_xz_filter* curr_filter = &xz_data->d_filters[i];
		
		curr_filter->f_bufsz = 0;
		
		*implerr = lzma_stream_buffer_encode(curr_filter->f_backends,
			LZMA_CHECK_CRC32, NULL,
			srcbuf, srcbufsz,
			curr_filter->f_buf, &curr_filter->f_bufsz,
			*destbufsz);
		
		if (*implerr == LZMA_OK) {
			if (!best_filter || curr_filter->f_bufsz < best_filter->f_bufsz)
				best_filter = curr_filter;
		} else {
			goto err;
		}
	}
	
	*destbufsz = best_filter->f_bufsz;
	if(best_filter->f_buf != destbuf)
		memcpy(destbuf, best_filter->f_buf, best_filter->f_bufsz);
	
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
	.hl_mk_usage = hostprog_lib_xz_mk_usage,
	.hl_mk_option = hostprog_lib_xz_mk_option,
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


