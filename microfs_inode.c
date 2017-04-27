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

static const struct inode_operations microfs_dir_i_ops;
static const struct file_operations microfs_dir_i_fops;

static const struct address_space_operations microfs_i_a_ops;

struct inode* microfs_get_inode(struct super_block* sb,
	const struct microfs_inode* const minode, const __u32 offset)
{
	struct inode* vinode;
	struct timespec vinode_xtime = {
		.tv_sec = __le32_to_cpu(MICROFS_SB(sb)->si_ctime),
		.tv_nsec = 0
	};
	
	vinode = iget_locked(sb, microfs_get_ino(minode, offset));
	if (!vinode)
		return ERR_PTR(-ENOMEM);
	
	pr_spam("vfs inode allocated for ino %lu for super block 0x%p\n",
		vinode->i_ino, sb);
	
	if (!(vinode->i_state & I_NEW))
		return vinode;
	
	switch (minode->i_mode & S_IFMT) {
		case S_IFREG:
			vinode->i_fop = &generic_ro_fops;
			vinode->i_data.a_ops = &microfs_i_a_ops;
			break;
		case S_IFDIR:
			vinode->i_op = &microfs_dir_i_ops;
			vinode->i_fop = &microfs_dir_i_fops;
			break;
		case S_IFLNK:
			inode_nohighmem(vinode);
			vinode->i_op = &page_symlink_inode_operations;
			vinode->i_data.a_ops = &microfs_i_a_ops;
			break;
		default:
			init_special_inode(vinode, __le16_to_cpu(minode->i_mode),
				old_decode_dev(i_getsize(minode)));
	}
	
	vinode->i_mode = __le16_to_cpu(minode->i_mode);
	i_uid_write(vinode, __le16_to_cpu(minode->i_uid));
	i_gid_write(vinode, __le16_to_cpu(minode->i_gid));
	
	if (minode->i_offset) {
		const __u32 size = i_getsize(minode);
		vinode->i_size = size;
		vinode->i_blocks = (size - 1) / 512 + 1;
	}
	
	/* Well, the time stamps are perhaps not super sane, they
	 * should however be sane enough for practical purposes.
	 */
	vinode->i_ctime = vinode_xtime;
	vinode->i_mtime = vinode_xtime;
	vinode->i_atime = vinode_xtime;
	
	unlock_new_inode(vinode);
	return vinode;
}

/* Fill the given page with data.
 */
static int microfs_readpage(struct file* file, struct page* page)
{
	struct inode* inode = page->mapping->host;
	if (page->index < i_blks(i_size_read(inode), PAGE_SIZE)) {
		return __microfs_readpage(file, page);
	} else {
		void* page_data = kmap(page);
		memset(page_data, 0, PAGE_SIZE);
		kunmap(page);
		flush_dcache_page(page);
		SetPageUptodate(page);
		unlock_page(page);
		return 0;
	}
}

static struct dentry* microfs_lookup(struct inode* dinode,
	struct dentry* dentry, unsigned int flags)
{
	__u32 offset = 0;
	
	void* err = NULL;
	
	struct inode* vinode = NULL;
	struct super_block* sb = dinode->i_sb;
	struct microfs_sb_info* sbi = MICROFS_SB(sb);
	
	pr_devel_once("microfs_lookup: first call\n");
	
	mutex_lock(&sbi->si_metadata_dentrybuf.d_mutex);
	
	while (offset < i_size_read(dinode)) {
		struct microfs_inode* minode;
		__u32 minodelen = sizeof(*minode);
		
		char* name;
		__u8 namelen = MICROFS_MAXNAMELEN;
		
		__u32 dir_offset = microfs_get_offset(dinode) + offset;
		
		int diff;
		
		minode = (struct microfs_inode*)__microfs_read(sb,
			&sbi->si_metadata_dentrybuf, dir_offset, minodelen + namelen);
		if (unlikely(IS_ERR(minode))) {
			err = minode;
			minode = NULL;
			goto err_io;
		}
		
#if defined(DEBUG) && defined(DEBUG_INODES)
		print_hex_dump(KERN_DEBUG, pr_fmt("microfs_lookup: inode: "),
			DUMP_PREFIX_OFFSET, 16, 1, minode, sizeof(*minode), true);
#endif
		
		name = (char*)(minode + 1);
		
		if (dentry->d_name.name[0] < name[0])
			break;
		
		namelen = minode->i_namelen;
		offset += sizeof(*minode) + namelen;
		
		if (dentry->d_name.len != namelen)
			continue;
		
		diff = memcmp(dentry->d_name.name, name, namelen);
		if (!diff) {
			vinode = microfs_get_inode(sb, minode, dir_offset);
			if (unlikely(IS_ERR(vinode))) {
				err = vinode;
				vinode = NULL;
				goto err_inode;
			}
			break;
		} else if (diff > 0) {
			continue;
		} else if (diff < 0) {
			break;
		} else {
			/* Never reached.
			 */
		}
	}
	
err_inode:
err_io:
	mutex_unlock(&sbi->si_metadata_dentrybuf.d_mutex);
	if (unlikely(IS_ERR(err)))
		return err;
	
	d_add(dentry, vinode);
	
	return NULL;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)

static int microfs_iterate(struct file* file, struct dir_context* ctx)
{
	struct inode* vinode = file_inode(file);
	struct super_block* sb = vinode->i_sb;
	struct microfs_sb_info* sbi = MICROFS_SB(sb);
	
	char* fillbuf;
	
	int err = 0;
	
	__u32 offset = ctx->pos;
	
	pr_devel_once("microfs_iterate: first call\n");
	
	if (offset >= i_size_read(vinode))
		return 0;
	
	fillbuf = kmalloc(MICROFS_MAXNAMELEN, GFP_KERNEL);
	if (unlikely(!fillbuf)) {
		pr_err("microfs_iterate: failed to allocate the fillbuf\n");
		err = -ENOMEM;
		goto err_fillbuf;
	}
	
	while (offset < i_size_read(vinode)) {
		struct microfs_inode* minode;
		
		char* name;
		__u8 namelen = MICROFS_MAXNAMELEN;
		
		__u32 next_offset;
		__u32 dentry_offset;
		
		ino_t ino;
		umode_t mode;
		
		int err;
		
		mutex_lock(&sbi->si_metadata_dentrybuf.d_mutex);
		
		dentry_offset = microfs_get_offset(vinode) + offset;
		minode = (struct microfs_inode*)__microfs_read(sb,
			&sbi->si_metadata_dentrybuf, dentry_offset, sizeof(*minode) + namelen);
		if (unlikely(IS_ERR(minode))) {
			pr_err("microfs_iterate:"
				" failed to read the inode at offset 0x%x\n", dentry_offset);
			err = PTR_ERR(minode);
			goto err_io;
		}
		
#if defined(DEBUG) && defined(DEBUG_INODES)
		print_hex_dump(KERN_DEBUG, pr_fmt("microfs_iterate: inode: "),
			DUMP_PREFIX_OFFSET, 16, 1, minode, sizeof(*minode), true);
#endif
		
		name = (char*)(minode + 1);
		namelen = minode->i_namelen;
		
#if defined(DEBUG) && defined(DEBUG_INODES)
		pr_spam("microfs_iterate: inode name: %*.*s\n",
			namelen, namelen, name);
#endif
		
		memcpy(fillbuf, name, namelen);
		
		next_offset = offset + sizeof(*minode) + namelen;
		dentry_offset = microfs_get_offset(vinode) + offset;
		ino = microfs_get_ino(minode, dentry_offset);
		mode = __le16_to_cpu(minode->i_mode);
		minode = NULL;
		
		mutex_unlock(&sbi->si_metadata_dentrybuf.d_mutex);
		
		if (!dir_emit(ctx, fillbuf, namelen, ino, mode >> 12))
			break;
		
		ctx->pos = offset = next_offset;
	}
	
	kfree(fillbuf);
	
	return 0;
	
err_io:
	mutex_unlock(&sbi->si_metadata_dentrybuf.d_mutex);
	kfree(fillbuf);
err_fillbuf:
	return err;
}

#else

static int microfs_readdir(struct file* file, void* dirent,
	filldir_t filldir)
{
	struct inode* vinode = file_inode(file);
	struct super_block* sb = vinode->i_sb;
	struct microfs_sb_info* sbi = MICROFS_SB(sb);
	
	char* fillbuf;
	
	int err = 0;
	
	__u32 offset = file->f_pos;
	
	pr_devel_once("microfs_readdir: first call\n");
	
	if (offset >= i_size_read(vinode))
		return 0;
	
	fillbuf = kmalloc(MICROFS_MAXNAMELEN, GFP_KERNEL);
	if (unlikely(!fillbuf)) {
		pr_err("microfs_readdir: failed to allocate the fillbuf\n");
		err = -ENOMEM;
		goto err_fillbuf;
	}
	
	while (offset < i_size_read(vinode)) {
		struct microfs_inode* minode;
		
		char* name;
		__u8 namelen = MICROFS_MAXNAMELEN;
		
		__u32 next_offset;
		__u32 dentry_offset;
		
		ino_t ino;
		umode_t mode;
		
		int err;
		
		mutex_lock(&sbi->si_metadata_dentrybuf.d_mutex);
		
		dentry_offset = microfs_get_offset(vinode) + offset;
		minode = (struct microfs_inode*)__microfs_read(sb,
			&sbi->si_metadata_dentrybuf, dentry_offset, sizeof(*minode)
				+ namelen);
		if (unlikely(IS_ERR(minode))) {
			pr_err("microfs_readdir:"
				" failed to read the inode at offset 0x%x\n", dentry_offset);
			err = PTR_ERR(minode);
			goto err_io;
		}
		
#if defined(DEBUG) && defined(DEBUG_INODES)
		print_hex_dump(KERN_DEBUG, pr_fmt("microfs_readdir: inode: "),
			DUMP_PREFIX_OFFSET, 16, 1, minode, sizeof(*minode), true);
#endif
		
		name = (char*)(minode + 1);
		namelen = minode->i_namelen;
		
#if defined(DEBUG) && defined(DEBUG_INODES)
		pr_spam("microfs_readdir: inode name: %*.*s\n",
			namelen, namelen, name);
#endif
		
		memcpy(fillbuf, name, namelen);
		
		next_offset = offset + sizeof(*minode) + namelen;
		dentry_offset = microfs_get_offset(vinode) + offset;
		ino = microfs_get_ino(minode, dentry_offset);
		mode = __le16_to_cpu(minode->i_mode);
		minode = NULL;
		
		mutex_unlock(&sbi->si_metadata_dentrybuf.d_mutex);
		
		err = filldir(dirent, fillbuf, namelen, offset, ino, mode >> 12);
		if (unlikely(err)) {
			pr_err("microfs_readdir: filldir failed: %d\n", err);
			break;
		}
		
		file->f_pos = offset = next_offset;
	}
	
	kfree(fillbuf);
	
	return 0;
	
err_io:
	mutex_unlock(&sbi->si_metadata_dentrybuf.d_mutex);
	kfree(fillbuf);
err_fillbuf:
	return err;
}

#endif

static const struct inode_operations microfs_dir_i_ops = {
	.lookup = microfs_lookup
};

static const struct file_operations microfs_dir_i_fops = {
	.llseek = generic_file_llseek,
	.read = generic_read_dir,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
	.iterate = microfs_iterate
#else
	.readdir = microfs_readdir
#endif
};

static const struct address_space_operations microfs_i_a_ops = {
	.readpage = microfs_readpage
};

