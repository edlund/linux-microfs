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

static const struct microfs_decompressor decompressor_null = {
	.dc_id = MICROFS_FLAG_DECOMPRESSOR_NULL,
	.dc_name = "NULL"
};

static const struct microfs_decompressor* decompressors[] = {
	&decompressor_zlib,
	&decompressor_null
};

int microfs_decompressor_init(struct microfs_sb_info* sbi)
{
	int i;
	int err = 0;
	
	__u32 decompressor = sbi->si_flags & MICROFS_FLAG_MASK_DECOMPRESSOR;
	
	for (i = 0; decompressors[i]->dc_id; i++) {
		if (decompressors[i]->dc_id == decompressor)
			break;
	}
	
	if (!decompressors[i]->dc_id) {
		pr_err("failed to find a decompressor with id 0x%x\n",
			decompressor);
		err = -EINVAL;
		goto err;
	} else if (!decompressors[i]->dc_compiled) {
		pr_err("support for the %s decompressor has not been compiled\n",
			decompressors[i]->dc_name);
		err = -ENOSYS;
		goto err;
	}
	
	sbi->si_decompressor = decompressors[i];
	return sbi->si_decompressor->dc_init(sbi);
	
err:
	return err;
}
