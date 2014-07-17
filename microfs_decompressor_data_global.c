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

struct microfs_decompressor_data_global {
	__u32 dc_users;
	void* dc_data;
};

/* %__global is protected by %__global_mutex.
 */
struct microfs_decompressor_data_global __global;

DEFINE_MUTEX(__global_mutex);

static void microfs_decompressor_data_global_get(struct microfs_sb_info* sbi,
	void** dest)
{
	BUG_ON(*dest != NULL);
	mutex_lock(&__global_mutex);
	*dest = __global.dc_data;
}

static void microfs_decompressor_data_global_put(struct microfs_sb_info* sbi,
	void** src)
{
	BUG_ON(*src == NULL);
	mutex_unlock(&__global_mutex);
	*src = NULL;
}

static void microfs_decompressor_data_global_destroy(struct microfs_sb_info* sbi)
{
	mutex_lock(&__global_mutex);
	BUG_ON(__global.dc_users == 0);
	if (--__global.dc_users == 0) {
		WARN_ON(sbi->si_decompressor->dc_destroy(sbi, __global.dc_data));
	}
	mutex_unlock(&__global_mutex);
}

int microfs_decompressor_data_global_create(struct microfs_sb_info* sbi)
{
	int err;
	
	struct microfs_decompressor_data* dd = sbi->si_decompressor_data;
	
	mutex_lock(&__global_mutex);
	if (__global.dc_users++ == 0) {
		err = sbi->si_decompressor->dc_create(sbi, &__global.dc_data);
		if (err) {
			pr_err("microfs_decompressor_data_global_create:"
				" failed to create the decompressor instance");
			goto err_create;
		}
	}
	mutex_unlock(&__global_mutex);
	
	dd->dd_private = NULL;
	dd->dd_get = microfs_decompressor_data_global_get;
	dd->dd_put = microfs_decompressor_data_global_put;
	dd->dd_destroy = microfs_decompressor_data_global_destroy;
	
	return 0;
	
err_create:
	return err;
}

