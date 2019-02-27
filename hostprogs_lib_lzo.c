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

#include "hostprogs_lib.h"
#include "microfs_fs.h"

#include "libinfo_lzo.h"

#ifdef HOSTPROGS_LIB_LZO

#include <errno.h>
#include <stdlib.h>

#include <lzo/lzoconf.h>
#include <lzo/lzo1x.h>

static int hostprog_lib_lzo_init(void** data, __u32 blksz)
{
	(void)blksz;
	
	if (!(*data = malloc(LZO1X_999_MEM_COMPRESS))) {
		errno = ENOMEM;
		return -1;
	}
	return 0;
}

static int hostprog_lib_lzo_compress(void* data, void* destbuf, __u32* destbufsz,
	void* srcbuf, __u32 srcbufsz, int* implerr)
{
	lzo_uint destsz = *destbufsz;
	lzo_voidp workmem = data;
	
	*implerr = lzo1x_999_compress(srcbuf, srcbufsz,
		destbuf, &destsz, workmem);
	*destbufsz = *implerr == LZO_E_OK ? destsz : 0;
	return *implerr == LZO_E_OK ? 0 : -1;
}

static int hostprog_lib_lzo_decompress(void* data, void* destbuf, __u32* destbufsz,
	void* srcbuf, __u32 srcbufsz, int* implerr)
{
	(void)data;
	
	lzo_uint destsz = *destbufsz;
	
	*implerr = lzo1x_decompress_safe(srcbuf, srcbufsz,
		destbuf, &destsz, NULL);
	*destbufsz = *implerr == LZO_E_OK? destsz: 0;
	return *implerr == LZO_E_OK ? 0 : -1;
}

static __u32 hostprog_lib_lzo_upperbound(void* data, __u32 size)
{
	(void)data;
	return size + (size / 16) + 64 + 3;
}

static const char* hostprog_lib_lzo_strerror(void* data, int implerr)
{
	(void)data;
	(void)implerr;
	return "unknown error";
}

const struct hostprog_lib hostprog_lib_lzo = {
	.hl_info = &libinfo_lzo,
	.hl_compiled = 1,
	.hl_init = hostprog_lib_lzo_init,
	.hl_mk_usage = hostprog_lib_mk_usage,
	.hl_mk_option = hostprog_lib_mk_option,
	.hl_mk_dd = hostprog_lib_mk_dd,
	.hl_ck_dd = hostprog_lib_ck_dd,
	.hl_compress = hostprog_lib_lzo_compress,
	.hl_decompress = hostprog_lib_lzo_decompress,
	.hl_upperbound = hostprog_lib_lzo_upperbound,
	.hl_strerror = hostprog_lib_lzo_strerror
};

#else

const struct hostprog_lib hostprog_lib_lzo = {
	.hl_info = &libinfo_lzo,
	.hl_compiled = 0
};

#endif

