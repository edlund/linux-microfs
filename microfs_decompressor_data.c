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

/* %__manager_list is protected by %__manager_mutex.
 */
struct list_head __manager_list;
DEFINE_MUTEX(__manager_mutex);

void microfs_decompressor_data_manager_init(void)
{
	INIT_LIST_HEAD(&__manager_list);
}

void microfs_decompressor_data_manager_exit(void)
{
	WARN_ON(!list_empty(&__manager_list));
}

static void microfs_decompressor_data_manager_release_private(struct microfs_sb_info* sbi)
{
	if (sbi->si_decompressor_data->dd_destroy) {
		sbi->si_decompressor_data->dd_destroy(sbi, sbi->si_decompressor_data
			->dd_private);
	}
	kfree(sbi->si_decompressor_data);
}

static void microfs_decompressor_data_manager_release_public(struct microfs_sb_info* sbi)
{
	mutex_lock(&__manager_mutex);
	if (--sbi->si_decompressor_data->dd_users == 0) {
		list_del(&sbi->si_decompressor_data->dd_sharelist);
		microfs_decompressor_data_manager_release_private(sbi);
	}
	mutex_unlock(&__manager_mutex);
}

int microfs_decompressor_data_manager_acquire_private(struct microfs_sb_info* sbi,
	char* dd, struct microfs_decompressor_data** dest,
	microfs_decompressor_data_creator creator)
{
	int err;
	
	*dest = kzalloc(sizeof(**dest), GFP_KERNEL);
	if (!*dest) {
		pr_err("failed to alloc the decompressor data\n");
		err = -ENOMEM;
		goto err_alloc;
	}
	
	(*dest)->dd_blksz = sbi->si_blksz;
	(*dest)->dd_users = 1;
	(*dest)->dd_decompressor = sbi->si_decompressor;
	(*dest)->dd_creator = creator;
	(*dest)->dd_release = microfs_decompressor_data_manager_release_private;
	
	err = sbi->si_decompressor->dc_data_init(sbi, dd, *dest);
	if (err) {
		pr_err("%s: could not init the decompressor data",
			sbi->si_decompressor->dc_info->li_name);
		goto err_data;
	}
	
	return creator(sbi, *dest);
	
err_data:
err_alloc:
	return err;
}

int microfs_decompressor_data_manager_acquire_public(struct microfs_sb_info* sbi,
	char* dd, struct microfs_decompressor_data** dest,
	microfs_decompressor_data_creator creator)
{
	int err = 0;
	struct microfs_decompressor_data* walker = NULL;
	
	*dest = NULL;
	
	mutex_lock(&__manager_mutex);
	list_for_each_entry(walker, &__manager_list, dd_sharelist) {
		if (
			walker->dd_blksz == sbi->si_blksz &&
			walker->dd_decompressor == sbi->si_decompressor &&
			walker->dd_creator == creator
		) {
			*dest = walker;
			break;
		}
	}
	
	if (!*dest) {
		if (__debug_insid()) {
			pr_info("[insid=%d] microfs_decompressor_data_manager_acquire_public:"
				" share not possible\n", __debug_insid());
		}
		err = microfs_decompressor_data_manager_acquire_private(sbi, dd,
			dest, creator);
		if (err)
			goto err_get_unique;
		
		(*dest)->dd_release = microfs_decompressor_data_manager_release_public;
		list_add(&(*dest)->dd_sharelist, &__manager_list);
	} else {
		if (__debug_insid()) {
			pr_info("[insid=%d] microfs_decompressor_data_manager_acquire_public:"
				" successful share\n", __debug_insid());
		}
		(*dest)->dd_users++;
	}
	
err_get_unique:
	mutex_unlock(&__manager_mutex);
	return err;
}

int microfs_decompressor_data_init_noop(struct microfs_sb_info* sbi, void* dd,
	struct microfs_decompressor_data* data)
{
	(void)sbi;
	(void)dd;
	(void)data;
	
	return 0;
}

int microfs_decompressor_data_exit_noop(struct microfs_sb_info* sbi,
	struct microfs_decompressor_data* data)
{
	(void)sbi;
	(void)data;
	
	return 0;
}

