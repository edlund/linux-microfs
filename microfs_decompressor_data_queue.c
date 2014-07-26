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

struct microfs_decompressor_data_queue {
	int dc_avail;
	struct mutex dc_mutex;
	struct list_head dc_freelist;
	struct list_head dc_busylist;
	wait_queue_head_t dc_waitqueue;
};

struct microfs_decompressor_data_queue_node {
	void* dc_data;
	struct list_head dc_list;
};

static inline int microfs_decompressor_data_queue_ceil(void)
{
	return num_online_cpus() * 2;
}

static int microfs_decompressor_data_queue_get(struct microfs_sb_info* sbi,
	void** data)
{
	int err;
	struct microfs_decompressor_data_queue_node* node;
	struct microfs_decompressor_data_queue* queue = sbi
		->si_decompressor_data->dd_private;
	
	BUG_ON(*data != NULL);
	
	while (1) {
		mutex_lock(&queue->dc_mutex);
		
		if (!list_empty(&queue->dc_freelist)) {
			node = list_entry(queue->dc_freelist.prev, typeof(*node), dc_list);
			list_del(&node->dc_list);
			goto out;
		}
		
		if (queue->dc_avail >= microfs_decompressor_data_queue_ceil())
			goto wait;
		
		node = kmalloc(sizeof(*node), GFP_KERNEL);
		if (!node) {
			goto wait;
		}
		
		err = sbi->si_decompressor->dc_create(sbi, &node->dc_data);
		if (err) {
			kfree(node);
			goto wait;
		}
		
		WARN_ON(++queue->dc_avail > microfs_decompressor_data_queue_ceil());
out:
		*data = node->dc_data;
		node->dc_data = NULL;
		list_add(&node->dc_list, &queue->dc_busylist);
		mutex_unlock(&queue->dc_mutex);
		break;
wait:
		mutex_unlock(&queue->dc_mutex);
		wait_event(queue->dc_waitqueue, !list_empty(&queue->dc_freelist));
		continue;
	}
	
	return 0;
}

static int microfs_decompressor_data_queue_put(struct microfs_sb_info* sbi,
	void** data)
{
	struct microfs_decompressor_data_queue_node* node;
	struct microfs_decompressor_data_queue* queue = sbi
		->si_decompressor_data->dd_private;
	
	mutex_lock(&queue->dc_mutex);
	
	BUG_ON(list_empty(&queue->dc_busylist)); 
	BUG_ON(*data == NULL);
	
	node = list_entry(queue->dc_busylist.prev, typeof(*node), dc_list);
	node->dc_data = *data;
	
	list_del(&node->dc_list);
	list_add(&node->dc_list, &queue->dc_freelist);
	
	mutex_unlock(&queue->dc_mutex);
	wake_up(&queue->dc_waitqueue);
	
	*data = NULL;
	
	return 0;
}

static void microfs_decompressor_data_queue_destroy(struct microfs_sb_info* sbi,
	void* data)
{
	struct microfs_decompressor_data_queue_node* node;
	struct microfs_decompressor_data_queue* queue = data;
	
	if (queue) {
		while (!list_empty(&queue->dc_freelist)) {
			node = list_entry(queue->dc_freelist.prev, typeof(*node), dc_list);
			list_del(&node->dc_list);
			WARN_ON(sbi->si_decompressor->dc_destroy(sbi, node->dc_data));
			kfree(node);
			
			queue->dc_avail--;
		}
		
		WARN_ON(queue->dc_avail);
		WARN_ON(!list_empty(&queue->dc_busylist));
		
		kfree(queue);
	}
}

int microfs_decompressor_data_queue_create(struct microfs_sb_info* sbi,
	struct microfs_decompressor_data* data)
{
	int err = 0;
	
	struct microfs_decompressor_data_queue* queue = NULL;
	struct microfs_decompressor_data_queue_node* node = NULL;
	
	queue = kmalloc(sizeof(*queue), GFP_KERNEL);
	if (!queue) {
		pr_err("microfs_decompressor_queue_create:"
			" failed to allocate the decompressor queue");
		err = -ENOMEM;
		goto err_mem_queue;
	}
	
	INIT_LIST_HEAD(&queue->dc_freelist);
	INIT_LIST_HEAD(&queue->dc_busylist);
	
	mutex_init(&queue->dc_mutex);
	init_waitqueue_head(&queue->dc_waitqueue);
	
	node = kmalloc(sizeof(*node), GFP_KERNEL);
	if (!node) {
		pr_err("microfs_decompressor_queue_create:"
			" failed to allocate the decompressor list node");
		err = -ENOMEM;
		goto err_mem_node;
	}
	
	err = sbi->si_decompressor->dc_create(sbi, &node->dc_data);
	if (err) {
		pr_err("microfs_decompressor_queue_create:"
			" failed to create the decompressor for the list node");
		goto err_create;
	}
	
	queue->dc_avail = 1;
	list_add(&node->dc_list, &queue->dc_freelist);
	
	data->dd_private = queue;
	data->dd_get = microfs_decompressor_data_queue_get;
	data->dd_put = microfs_decompressor_data_queue_put;
	data->dd_destroy = microfs_decompressor_data_queue_destroy;
	
	return 0;
	
err_create:
	kfree(node);
err_mem_node:
	kfree(queue);
err_mem_queue:
	return err;
}

