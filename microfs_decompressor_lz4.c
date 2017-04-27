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

#include "microfs.h"

#include "libinfo_lz4.h"

#ifdef MICROFS_DECOMPRESSOR_LZ4

#include <linux/lz4.h>

static int decompressor_lz4_create(struct microfs_sb_info* sbi, void** dest)
{
	return decompressor_lz_create(sbi, dest, lz4_compressbound(sbi->si_blksz));
}

static int decompressor_lzo_end_consumer(struct microfs_sb_info* sbi, void* data,
	int* implerr, char* input, __u32 inputsz, char* output, __u32* outputsz)
{
	size_t lz4_outputsz = *outputsz;
	
	(void)sbi;
	(void)data;
	
	*implerr = lz4_decompress_unknownoutputsize(input, inputsz, output, &lz4_outputsz);
	*outputsz = lz4_outputsz;
	
	return *implerr < 0? -EIO: 0;
}

static int decompressor_lz4_end(struct microfs_sb_info* sbi, void* data,
	int* err, int* implerr, __u32* decompressed)
{
	return decompressor_lz_end(sbi, data, err, implerr, decompressed,
		decompressor_lzo_end_consumer);
}

const struct microfs_decompressor decompressor_lz4 = {
	.dc_info = &libinfo_lz4,
	.dc_compiled = 1,
	.dc_data_init = microfs_decompressor_data_init_noop,
	.dc_data_exit = microfs_decompressor_data_exit_noop,
	.dc_create = decompressor_lz4_create,
	.dc_destroy = decompressor_lz_destroy,
	.dc_reset = decompressor_lz_reset,
	.dc_exceptionally_begin = decompressor_lz_exceptionally_begin,
	.dc_nominally_begin = decompressor_lz_nominally_begin,
	.dc_copy_nominally_needpage = decompressor_lz_copy_nominally_needpage,
	.dc_copy_nominally_utilizepage = decompressor_lz_copy_nominally_utilizepage,
	.dc_copy_nominally_releasepage = decompressor_lz_copy_nominally_releasepage,
	.dc_consumebhs = decompressor_lz_consumebhs,
	.dc_continue = decompressor_lz_continue,
	.dc_end = decompressor_lz4_end
};

#else

const struct microfs_decompressor decompressor_lz4 = {
	.dc_info = &libinfo_lz4,
	.dc_compiled = 0
};

#endif

