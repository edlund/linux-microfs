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

#include "libinfo_lzo.h"

#ifdef MICROFS_DECOMPRESSOR_LZO

#include <linux/lzo.h>

static int decompressor_lzo_create(struct microfs_sb_info* sbi, void** dest)
{
	return decompressor_impl_buffer_create(sbi, dest, lzo1x_worst_compress(sbi->si_blksz));
}

static int decompressor_lzo_end_consumer(struct microfs_sb_info* sbi, void* data,
	int* implerr, char* input, __u32 inputsz, char* output, __u32* outputsz)
{
	size_t lzo_outputsz = *outputsz;
	
	(void)sbi;
	(void)data;
	
	*implerr = lzo1x_decompress_safe(input, inputsz, output, &lzo_outputsz);
	*outputsz = lzo_outputsz;
	
	return *implerr != LZO_E_OK ? -EIO : 0;
}

static int decompressor_lzo_end(struct microfs_sb_info* sbi,
	void* data, int* err, int* implerr, __u32* decompressed)
{
	return decompressor_impl_buffer_end(sbi, data, err, implerr, decompressed,
		decompressor_lzo_end_consumer);
}

const struct microfs_decompressor decompressor_lzo = {
	.dc_info = &libinfo_lzo,
	.dc_compiled = 1,
	.dc_streamed = 0,
	.dc_data_init = microfs_decompressor_data_init_noop,
	.dc_data_exit = microfs_decompressor_data_exit_noop,
	.dc_create = decompressor_lzo_create,
	.dc_destroy = decompressor_impl_buffer_destroy,
	.dc_reset = decompressor_impl_buffer_reset,
	.dc_exceptionally_begin = decompressor_impl_buffer_exceptionally_begin,
	.dc_nominally_begin = decompressor_impl_buffer_nominally_begin,
	.dc_copy_nominally_needpage = decompressor_impl_buffer_copy_nominally_needpage,
	.dc_copy_nominally_utilizepage = decompressor_impl_buffer_copy_nominally_utilizepage,
	.dc_copy_nominally_releasepage = decompressor_impl_buffer_copy_nominally_releasepage,
	.dc_consumebhs = decompressor_impl_buffer_consumebhs,
	.dc_continue = decompressor_impl_buffer_continue,
	.dc_end = decompressor_lzo_end
};

#else

const struct microfs_decompressor decompressor_lzo = {
	.dc_info = &libinfo_lzo,
	.dc_compiled = 0
};

#endif
