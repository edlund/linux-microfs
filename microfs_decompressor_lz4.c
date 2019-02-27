/* microfs - Minimally Improved Compressed Read Only File System
 * Copyright (C) 2012, 2013, 2014, 2015, 2016, 2017, ..., +%Y
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
	return decompressor_impl_buffer_create(sbi, dest, LZ4_compressBound(sbi->si_blksz));
}

static int decompressor_lz4_end_consumer(struct microfs_sb_info* sbi, void* data,
	int* implerr, char* input, __u32 inputsz, char* output, __u32* outputsz)
{
	int lz4_result = 0;
	int lz4_outputsz = *outputsz;
	
	(void)sbi;
	(void)data;
	
	lz4_result = LZ4_decompress_safe(input, output, inputsz, lz4_outputsz);
	if (lz4_result < 0) {
		pr_err("decompressor_lz4_end_consumer:"
			" failed to inflate data, implerr %d\n",
			lz4_result);
		*implerr = lz4_result;
		return -EIO;
	}
	*outputsz = lz4_result;
	return 0;
}

static int decompressor_lz4_end(struct microfs_sb_info* sbi, void* data,
	int* err, int* implerr, __u32* decompressed)
{
	return decompressor_impl_buffer_end(sbi, data, err, implerr, decompressed,
		decompressor_lz4_end_consumer);
}

const struct microfs_decompressor decompressor_lz4 = {
	.dc_info = &libinfo_lz4,
	.dc_compiled = 1,
	.dc_streamed = 0,
	.dc_data_init = microfs_decompressor_data_init_noop,
	.dc_data_exit = microfs_decompressor_data_exit_noop,
	.dc_create = decompressor_lz4_create,
	.dc_destroy = decompressor_impl_buffer_destroy,
	.dc_reset = decompressor_impl_buffer_reset,
	.dc_exceptionally_begin = decompressor_impl_buffer_exceptionally_begin,
	.dc_nominally_begin = decompressor_impl_buffer_nominally_begin,
	.dc_copy_nominally_needpage = decompressor_impl_buffer_copy_nominally_needpage,
	.dc_copy_nominally_utilizepage = decompressor_impl_buffer_copy_nominally_utilizepage,
	.dc_copy_nominally_releasepage = decompressor_impl_buffer_copy_nominally_releasepage,
	.dc_consumebhs = decompressor_impl_buffer_consumebhs,
	.dc_continue = decompressor_impl_buffer_continue,
	.dc_end = decompressor_lz4_end
};

#else

const struct microfs_decompressor decompressor_lz4 = {
	.dc_info = &libinfo_lz4,
	.dc_compiled = 0
};

#endif

