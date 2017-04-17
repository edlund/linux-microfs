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

#include "microfs.h"

#include <linux/cpumask.h>
#include <linux/sched.h>
#include <linux/wait.h>

static const struct microfs_decompressor decompressor_null = {
	.dc_info = NULL
};

static const struct microfs_decompressor* decompressors[] = {
	&decompressor_zlib,
	&decompressor_lz4,
	&decompressor_lzo,
	&decompressor_xz,
	&decompressor_null
};

int microfs_decompressor_init(struct microfs_sb_info* sbi, char* dd,
	microfs_decompressor_data_acquirer acquirer,
	microfs_decompressor_data_creator creator)
{
	int i;
	int err = 0;
	
	__u32 decompressor = sbi->si_flags & MICROFS_FLAG_MASK_DECOMPRESSOR;
	
	for (i = 0; decompressors[i]->dc_info; i++) {
		if (decompressors[i]->dc_info->li_id == decompressor)
			break;
	}
	
	if (!decompressors[i]->dc_info) {
		pr_err("failed to find a decompressor with id 0x%x\n", decompressor);
		err = -EINVAL;
		goto err;
	}
	if (!decompressors[i]->dc_compiled) {
		pr_err("%s: decompressor not compiled\n",
			decompressors[i]->dc_info->li_name);
		err = -ENOSYS;
		goto err;
	}
	
	if (decompressors[i]->dc_info->li_min_blksz == 0 &&
			sbi->si_blksz < PAGE_SIZE) {
		pr_err("%s: block size must be greater than or equal to PAGE_SIZE",
			decompressors[i]->dc_info->li_name);
		err = -ENOSYS;
		goto err;
	}
	
	sbi->si_decompressor = decompressors[i];
	sbi->si_decompressor_data = NULL;
	
	return acquirer(sbi, dd, &sbi->si_decompressor_data, creator);
	
err:
	return err;
}

