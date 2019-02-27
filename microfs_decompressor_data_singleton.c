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

struct microfs_decompressor_data_singleton {
	void* dc_data;
	struct mutex dc_mutex;
};

static int microfs_decompressor_data_singleton_get(struct microfs_sb_info* sbi,
	void** dest)
{
	struct microfs_decompressor_data_singleton* singleton = sbi
		->si_decompressor_data->dd_private;
	BUG_ON(*dest != NULL);
	mutex_lock(&singleton->dc_mutex);
	*dest = singleton->dc_data;
	
	return 0;
}

static int microfs_decompressor_data_singleton_put(struct microfs_sb_info* sbi,
	void** src)
{
	struct microfs_decompressor_data_singleton* singleton = sbi
		->si_decompressor_data->dd_private;
	BUG_ON(*src == NULL);
	mutex_unlock(&singleton->dc_mutex);
	*src = NULL;
	
	return 0;
}

static void microfs_decompressor_data_singleton_destroy(struct microfs_sb_info* sbi,
	void* data)
{
	struct microfs_decompressor_data_singleton* singleton = data;
	if (singleton) {
		WARN_ON(sbi->si_decompressor->dc_destroy(sbi, singleton->dc_data));
		kfree(singleton);
	}
}

int microfs_decompressor_data_singleton_create(struct microfs_sb_info* sbi,
	struct microfs_decompressor_data* data)
{
	int err;
	struct microfs_decompressor_data_singleton* singleton = kmalloc(
		sizeof(*singleton), GFP_KERNEL);
	
	if (!singleton) {
		pr_err("microfs_decompressor_singleton_create:"
			" failed to allocate the decompressor instance");
		err = -ENOMEM;
		goto err_mem_singleton;
	}
	
	mutex_init(&singleton->dc_mutex);
	
	err = sbi->si_decompressor->dc_create(sbi, &singleton->dc_data);
	if (err) {
		pr_err("microfs_decompressor_singleton_create:"
			" failed to create the decompressor instance");
		goto err_create;
	}
	
	data->dd_private = singleton;
	data->dd_get = microfs_decompressor_data_singleton_get;
	data->dd_put = microfs_decompressor_data_singleton_put;
	data->dd_destroy = microfs_decompressor_data_singleton_destroy;
	
	return 0;
	
err_create:
	kfree(singleton);
err_mem_singleton:
	return err;
}

