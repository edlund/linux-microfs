/* microfs - Minimally Improved Compressed Read Only File System
 * Copyright (C) 2012 Erik Edlund <erik.edlund@32767.se>
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
	
	pr_devel("vfs inode allocated for ino %lu for super block 0x%p\n",
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
	
	/* minode->i_sizel || minode->i_sizeh */
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

/* The error handling is very simplistic; errors are only
 * handled if anyone actually requests data from an erroneous
 * page. Although a buffer could have multiple errors, only
 * the first will be reported.
 */
static inline void* __microfs_read_dataptr(struct super_block* sb,
	struct microfs_read_buffer* buf, __u32 offset, __u32 length)
{
	__u32 i = offset / PAGE_CACHE_SIZE;
	__u32 j = i + length / PAGE_CACHE_SIZE + 1;
	
	pr_spam("__microfs_read_dataptr: offset=0x%x, length=%u,"
		" i=%u, j=%u\n", offset, length, i, j);
	
	for (; i < j; ++i) {
		if (IS_ERR(buf->rb_pages[i])) {
			pr_err("__microfs_read_dataptr:"
					" I/O error (%ld) at page %d from offset 0x%x\n",
				PTR_ERR(buf->rb_pages[i]), i, buf->rb_offset);
			return ERR_PTR(-EIO);
		}
	}
	return buf->rb_data + offset;
}

/* I/O errors are handled by %__microfs_read_dataptr (if
 * necessary (that is; if someone asks for data not present)).
 */
void* __microfs_read(struct super_block* sb,
	struct microfs_read_buffer* destbuf, __u32 offset, __u32 length)
{
	struct address_space* mapping = sb->s_bdev->bd_inode->i_mapping;
	
	__u32 i;
	__u32 dev_blks;
	
	__u32 blk_nr;
	__u32 buf_offset;
	
	pr_devel_once("__microfs_read: first call\n");
	
#define ABORT_READ_ON(Condition) \
	do { \
		if (unlikely(Condition)) { \
			pr_err("__microfs_read: " #Condition "\n"); \
			BUG(); \
			return ERR_PTR(-EINVAL); \
		} \
	} while (0)
	
	ABORT_READ_ON(length == 0);
	ABORT_READ_ON(length > destbuf->rb_size);
	ABORT_READ_ON(offset + length > mapping->host->i_size);
	
#undef ABORT_READ_ON
	
	/* Is this a cache hit?
	 */
	if (offset >= destbuf->rb_offset) {
		buf_offset = offset - destbuf->rb_offset;
		if (buf_offset + length <= destbuf->rb_size) {
			pr_devel_once("__microfs_read: first cache hit\n");
			return __microfs_read_dataptr(sb, destbuf, buf_offset, length);
		}
	}
	
	/* Nope.
	 */
	pr_devel_once("__microfs_read: first cache miss\n");
	
	destbuf->rb_offset = offset & PAGE_CACHE_MASK;
	
	blk_nr = offset >> PAGE_CACHE_SHIFT;
	dev_blks = mapping->host->i_size >> PAGE_CACHE_SHIFT;
	
	pr_spam("__microfs_read: rb_offset=%x, rb_size=%u, rb_npages=%u\n",
		destbuf->rb_offset, destbuf->rb_size, destbuf->rb_npages);
	pr_spam("__microfs_read: blk_nr=%u, dev_blks=%u\n",
		blk_nr, dev_blks);
	
	for (i = 0; i < destbuf->rb_npages; ++i) {
		if (likely(blk_nr + i < dev_blks)) {
			pr_spam("__microfs_read: async read block %u\n", blk_nr + i);
			destbuf->rb_pages[i] = read_mapping_page_async(mapping,
				blk_nr + i, NULL);
		} else {
			/* It is not possible to fill the entire read buffer this
			 * time. "Welcome to the end of the image."
			 */
			destbuf->rb_pages[i] = NULL;
		}
	}
	
	pr_spam("__microfs_read: async reading in progress\n");
	
	for (i = 0, buf_offset = 0; i < destbuf->rb_npages;
			i += 1, buf_offset += PAGE_CACHE_SIZE) {
		struct page* page = destbuf->rb_pages[i];
		if (likely(!IS_ERR_OR_NULL(page))) {
			pr_spam("__microfs_read: i=%u, buf_offset=%u\n", i, buf_offset);
			wait_on_page_locked(page);
			if (likely(PageUptodate(page))) {
				memcpy(destbuf->rb_data + buf_offset, kmap(page), PAGE_CACHE_SIZE);
				kunmap(page);
				destbuf->rb_pages[i] = NULL;
			} else {
				memset(destbuf->rb_data + buf_offset, 0, PAGE_CACHE_SIZE);
				destbuf->rb_pages[i] = ERR_PTR(-EIO);
			}
			page_cache_release(page);
			pr_spam("__microfs_read: page released\n");
		} else {
			if (IS_ERR(page)) {
				pr_err("__microfs_read: page %u is erroneous, PTR_ERR=%ld\n",
					i, PTR_ERR(page));
			}
		}
	}
	
	pr_spam("__microfs_read: async reading completed\n");
	
	buf_offset = offset - destbuf->rb_offset;
	return __microfs_read_dataptr(sb, destbuf, buf_offset, length);
}

static int microfs_find_block(struct super_block* const sb,
	struct microfs_sb_info* const sbi, struct inode* const inode,
	__u32 blk_ptrs, __u32 blk_nr, __u32* const blk_data_offset,
	__u32* const blk_data_length)
{
	void* buf_data;
	
	int err = 0;
	
	__u32 blk_ptr_length = MICROFS_IOFFSET_WIDTH / 8;
	__u32 blk_ptr_offset = microfs_get_offset(inode)
		+ blk_nr * blk_ptr_length;
	
	pr_devel_once("microfs_find_block: first call\n");
	
	mutex_lock(&sbi->si_read.rd_mutex);
	
	if (blk_nr) {
		buf_data = __microfs_read(sb, &sbi->si_read.rd_blkptrbuf,
			blk_ptr_offset - blk_ptr_length, blk_ptr_length);
		if (unlikely(IS_ERR(buf_data))) {
			err = PTR_ERR(buf_data);
			goto err_io;
		}
		*blk_data_offset = __le32_to_cpu(*(__le32*)buf_data);
	} else {
		*blk_data_offset = microfs_get_offset(inode)
			+ blk_ptrs * blk_ptr_length;
	}
	
	buf_data = __microfs_read(sb, &sbi->si_read.rd_blkptrbuf,
		blk_ptr_offset, blk_ptr_length);
	if (unlikely(IS_ERR(buf_data))) {
		err = PTR_ERR(buf_data);
		goto err_io;
	}
	
	*blk_data_length = __le32_to_cpu(*(__le32*)buf_data)
		- *blk_data_offset;
	
	pr_spam("microfs_find_block: blk_data_offset=0x%x, blk_data_length=%u\n",
		*blk_data_offset, *blk_data_length);
	
err_io:
	mutex_unlock(&sbi->si_read.rd_mutex);
	return err;
}

/* Fill either a part of page or the whole page with a block
 * which uncompressed is smaller than or equal to PAGE_CACHE_SIZE.
 */
static int microfs_readpage_normal_block(struct super_block* const sb,
	struct microfs_sb_info* const sbi, const __u32 blk_data_offset,
	const __u32 blk_data_length, void* page_data, const __u32 page_used)
{
	void* buf_data;
	
	int err = 0;
	
	pr_devel_once("microfs_readpage_normal_block: first call\n");
	
	mutex_lock(&sbi->si_read.rd_mutex);
	
	buf_data = __microfs_read(sb, &sbi->si_read.rd_databuf,
		blk_data_offset, blk_data_length);
	if (unlikely(IS_ERR(buf_data))) {
		err = PTR_ERR(buf_data);
		goto err_io;
	}
	
	err = __microfs_inflate_block(&sbi->si_read,
		page_data + page_used, PAGE_CACHE_SIZE,
		buf_data, blk_data_length);
	
err_io:
	mutex_unlock(&sbi->si_read.rd_mutex);
	return err;
}

/* Fill a page with a part of a block which uncompressed is
 * larger than PAGE_CACHE_SIZE.
 */
static int microfs_readpage_huge_block(struct super_block* const sb,
	struct microfs_sb_info* const sbi, const __u32 blk_data_offset,
	const __u32 blk_data_length, void* page_data, const __u32 page_offset)
{
	void* buf_data;
	
	int err = 0;
	
	pr_devel_once("microfs_readpage_huge_block: first call\n");
	
	mutex_lock(&sbi->si_read.rd_mutex);
	
	if (blk_data_offset != sbi->si_read.rd_inflatebuf.ib_offset) {
		buf_data = __microfs_read(sb, &sbi->si_read.rd_databuf,
			blk_data_offset, blk_data_length);
		if (unlikely(IS_ERR(buf_data))) {
			err = PTR_ERR(buf_data);
			goto err_io;
		}
		
		err = __microfs_inflate_block(&sbi->si_read,
			sbi->si_read.rd_inflatebuf.ib_data, sbi->si_blksz,
			buf_data, blk_data_length);
		if (unlikely(err < 0)) {
			sbi->si_read.rd_inflatebuf.ib_offset = (1ULL << 32) - 1;
			sbi->si_read.rd_inflatebuf.ib_size = err;
			goto err_io;
		}
		sbi->si_read.rd_inflatebuf.ib_offset = blk_data_offset;
		sbi->si_read.rd_inflatebuf.ib_size = err;
	} else if(unlikely(sbi->si_read.rd_inflatebuf.ib_size < 0)) {
		err = sbi->si_read.rd_inflatebuf.ib_size;
		goto err_io;
	}
	
	err = (sbi->si_read.rd_inflatebuf.ib_size - page_offset) < PAGE_CACHE_SIZE?
		sbi->si_read.rd_inflatebuf.ib_size & ~PAGE_CACHE_MASK: PAGE_CACHE_SIZE;
	
	memcpy(page_data, sbi->si_read.rd_inflatebuf.ib_data + page_offset, err);
	
err_io:
	mutex_unlock(&sbi->si_read.rd_mutex);
	return err;
}

/* Fill the given page with data.
 */
static int microfs_readpage(struct file* file, struct page* page)
{
	struct inode* inode = page->mapping->host;
	struct super_block* sb = inode->i_sb;
	struct microfs_sb_info* sbi = MICROFS_SB(sb);
	
	void* page_data = kmap(page);
	
	__u32 page_used = 0;
	
	int err;
	int filled = 0;
	
	pr_devel_once("microfs_readpage: first call\n");
	pr_spam("microfs_readpage: inode->i_ino=%lu", inode->i_ino);
	
	if (page->index < (inode->i_size >> PAGE_CACHE_SHIFT) + 1) {
		__u32 blk_ptrs = i_blkptrs(inode->i_size, sbi->si_blksz);
		__u32 blk_nr = sbi->si_blksz <= PAGE_CACHE_SIZE?
			page->index * (PAGE_CACHE_SIZE >> sbi->si_blkshift):
			page->index / (sbi->si_blksz / PAGE_CACHE_SIZE);
		
		__u32 blk_data_offset;
		__u32 blk_data_length;
		
		pr_spam("microfs_readpage: page->index=%lu, blk_ptrs=%u\n",
			page->index, blk_ptrs);
		
		while (page_used < PAGE_CACHE_SIZE && blk_nr < blk_ptrs) {
			err = microfs_find_block(sb, sbi, inode, blk_ptrs,
				blk_nr, &blk_data_offset, &blk_data_length);
			if (unlikely(err)) {
				goto err_io;
			}
			
			pr_spam("microfs_readpage: blk_data_offset=0x%x, blk_data_length=%u,"
				" page_used=%u, blk_nr=%u\n", blk_data_offset, blk_data_length,
				page_used, blk_nr);
			
			if (blk_data_length == 0) {
				/* A file hole can be smaller than an entire page or block
				 * (when a file ends with a file hole), but that is not
				 * important in this context since a whole page is available.
				 */
				pr_spam("microfs_readpage: file hole found at 0x%x\n", blk_data_offset);
				filled = sbi->si_blksz <= PAGE_CACHE_SIZE?
					sbi->si_blksz: PAGE_CACHE_SIZE;
				memset(page_data + page_used, 0, filled);
			} else {
				filled = sbi->si_blksz <= PAGE_CACHE_SIZE?
					microfs_readpage_normal_block(sb, sbi, blk_data_offset, blk_data_length,
						page_data, page_used):
					microfs_readpage_huge_block(sb, sbi, blk_data_offset, blk_data_length,
						page_data, page->index * PAGE_CACHE_SIZE - blk_nr * sbi->si_blksz);
				if (unlikely(filled < 0)) {
					err = filled;
					goto err_io;
				}
			}
			
			blk_nr += 1;
			page_used += filled;
		}
	} else {
		pr_warn("microfs_readpage: page index %lu is out of bounds for inode %lu\n",
			page->index, inode->i_ino);
	}
	
	memset(page_data + page_used, 0, PAGE_CACHE_SIZE - page_used);
	flush_dcache_page(page);
	kunmap(page);
	SetPageUptodate(page);
	unlock_page(page);
	return 0;
	
err_io:
	pr_err("microfs_readpage: I/O error\n");
	kunmap(page);
	ClearPageUptodate(page);
	SetPageError(page);
	unlock_page(page);
	return 0;
}

static struct dentry* microfs_lookup(struct inode* dinode, struct dentry* dentry,
	unsigned int flags)
{
	__u32 offset = 0;
	
	void* err = NULL;
	
	struct inode* vinode = NULL;
	struct super_block* sb = dinode->i_sb;
	struct microfs_sb_info* sbi = MICROFS_SB(sb);
	
	pr_devel_once("microfs_lookup: first call\n");
	
	mutex_lock(&sbi->si_read.rd_mutex);
	
	while (offset < dinode->i_size) {
		struct microfs_inode* minode;
		__u32 minodelen = sizeof(*minode);
		
		char* name;
		__u32 namelen = MICROFS_MAXNAMELEN;
		
		__u32 dir_offset = microfs_get_offset(dinode) + offset;
		
		int diff;
		
		minode = (struct microfs_inode*)__microfs_read(sb,
			&sbi->si_read.rd_dentbuf, dir_offset, minodelen + namelen);
		if (unlikely(IS_ERR(minode))) {
			err = minode;
			minode = NULL;
			goto err_io;
		}
		
#if defined(DEBUG) && defined(DEBUG_SPAM)
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
	mutex_unlock(&sbi->si_read.rd_mutex);
	if (unlikely(IS_ERR(err)))
		return err;
	
	d_add(dentry, vinode);
	
	return NULL;
}

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
	
	if (offset >= vinode->i_size)
		return 0;
	
	fillbuf = kmalloc(MICROFS_MAXNAMELEN, GFP_KERNEL);
	if (unlikely(!fillbuf)) {
		pr_err("microfs_readdir: failed to allocate the fillbuf\n");
		err = -ENOMEM;
		goto err_fillbuf;
	}
	
	while (offset < vinode->i_size) {
		struct microfs_inode* minode;
		
		char* name;
		__u8 namelen = MICROFS_MAXNAMELEN;
		
		__u32 next_offset;
		__u32 dentry_offset;
		
		ino_t ino;
		umode_t mode;
		
		int err;
		
		mutex_lock(&sbi->si_read.rd_mutex);
		
		dentry_offset = microfs_get_offset(vinode) + offset;
		minode = (struct microfs_inode*)__microfs_read(sb,
			&sbi->si_read.rd_dentbuf, dentry_offset, sizeof(*minode)
				+ namelen);
		if (unlikely(IS_ERR(minode))) {
			pr_err("microfs_readdir:"
				" failed to read the inode at offset 0x%x\n", dentry_offset);
			err = PTR_ERR(minode);
			goto err_io;
		}
		
#if defined(DEBUG) && defined(DEBUG_SPAM)
		print_hex_dump(KERN_DEBUG, pr_fmt("microfs_readdir: inode: "),
			DUMP_PREFIX_OFFSET, 16, 1, minode, sizeof(*minode), true);
#endif
		
		name = (char*)(minode + 1);
		namelen = minode->i_namelen;
		
		pr_spam("microfs_readdir: inode name: %*.*s\n",
			namelen, namelen, name);
		
		memcpy(fillbuf, name, namelen);
		
		next_offset = offset + sizeof(*minode) + namelen;
		dentry_offset = microfs_get_offset(vinode) + offset;
		ino = microfs_get_ino(minode, dentry_offset);
		mode = __le16_to_cpu(minode->i_mode);
		minode = NULL;
		
		mutex_unlock(&sbi->si_read.rd_mutex);
		
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
	mutex_unlock(&sbi->si_read.rd_mutex);
	kfree(fillbuf);
err_fillbuf:
	return err;
}

static const struct inode_operations microfs_dir_i_ops = {
	.lookup = microfs_lookup
};

static const struct file_operations microfs_dir_i_fops = {
	.llseek = generic_file_llseek,
	.read = generic_read_dir,
	.readdir = microfs_readdir
};

static const struct address_space_operations microfs_i_a_ops = {
	.readpage = microfs_readpage
};
