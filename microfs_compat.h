/* microfs - Minimally Improved Compressed Read Only File System
 * Copyright (C) 2013 Erik Edlund <erik.edlund@32767.se>
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

#ifndef __MICROFS_COMPAT_H__
#define __MICROFS_COMPAT_H__

/* Small compatibility fixes.
 */

#include <linux/version.h>

#if defined(DEBUG)
#ifndef pr_devel_once
#define pr_devel_once(Fmt, ...) \
	pr_debug_once(Fmt, ##__VA_ARGS__)
#endif
#ifndef pr_devel_ratelimited
#define pr_devel_ratelimited(Fmt, ...) \
	printk_ratelimited(KERN_DEBUG pr_fmt(Fmt), ##__VA_ARGS__)
#endif
#else
#ifndef pr_devel_once
#define pr_devel_once(Fmt, ...) \
	no_printk(KERN_DEBUG pr_fmt(Fmt), ##__VA_ARGS__)
#endif
#ifndef pr_devel_ratelimited
#define pr_devel_ratelimited(Fmt, ...) \
	no_printk(KERN_DEBUG pr_fmt(Fmt), ##__VA_ARGS__)
#endif
#endif

#ifndef MODULE_ALIAS_FS
#define MODULE_ALIAS_FS(Name) MODULE_ALIAS("fs-" Name)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 9, 0)

static inline struct inode* file_inode(struct file* f)
{
	return f->f_path.dentry->d_inode;
}

#endif

#endif
