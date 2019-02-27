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

struct microfs_decompressor_data_percpu {
	void* dc_data;
};

static int microfs_decompressor_data_percpu_get(struct microfs_sb_info* sbi,
	void** dest)
{
	int cpu;
	struct microfs_decompressor_data_percpu* ptr;
	struct microfs_decompressor_data_percpu __percpu* percpu = sbi
		->si_decompressor_data->dd_private;
	
	BUG_ON(*dest != NULL);
	
	cpu = get_cpu();
	ptr = per_cpu_ptr(percpu, cpu);
	*dest = ptr->dc_data;
	
	return 0;
}

static int microfs_decompressor_data_percpu_put(struct microfs_sb_info* sbi,
	void** src)
{
	BUG_ON(*src == NULL);
	
	*src = NULL;
	
	put_cpu();
	
	return 0;
}

static void microfs_decompressor_data_percpu_destroy(struct microfs_sb_info* sbi,
	void* data)
{
	int cpu;
	struct microfs_decompressor_data_percpu* ptr;
	struct microfs_decompressor_data_percpu __percpu* percpu = data;
	
	if (percpu) {
		for_each_possible_cpu(cpu) {
			ptr = per_cpu_ptr(percpu, cpu);
			if (ptr->dc_data)
				WARN_ON(sbi->si_decompressor->dc_destroy(sbi, ptr->dc_data));
		}
		free_percpu(percpu);
	}
}

int microfs_decompressor_data_percpu_create(struct microfs_sb_info* sbi,
	struct microfs_decompressor_data* data)
{
	int cpu;
	int err;
	struct microfs_decompressor_data_percpu* ptr;
	struct microfs_decompressor_data_percpu __percpu* percpu;
	
	percpu = alloc_percpu(typeof(*ptr));
	if (!percpu) {
		pr_err("microfs_decompressor_percpu_create:"
			" failed to allocate percpu pointers");
		err = -ENOMEM;
		goto err_mem_percpu;
	}
	
	for_each_possible_cpu(cpu) {
		ptr = per_cpu_ptr(percpu, cpu);
		err = sbi->si_decompressor->dc_create(sbi, &ptr->dc_data);
		if (err) {
			pr_err("microfs_decompressor_percpu_create:"
				" failed to create a decompressor for cpu%d", cpu);
			goto err_create;
		}
	}
	
	data->dd_private = (__force void*)percpu;
	data->dd_get = microfs_decompressor_data_percpu_get;
	data->dd_put = microfs_decompressor_data_percpu_put;
	data->dd_destroy = microfs_decompressor_data_percpu_destroy;
	
	return 0;
	
err_create:
	microfs_decompressor_data_percpu_destroy(sbi, percpu);
err_mem_percpu:
	return err;
}

