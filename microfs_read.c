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

struct microfs_readpage_request {
	struct page** rr_pages;
	__u32 rr_npages;
	__u32 rr_bhoffset;
};

static int __microfs_find_block(struct super_block* const sb,
	struct inode* const inode, __u32 blk_ptrs, __u32 blk_nr,
	__u32* const blk_data_offset,
	__u32* const blk_data_length)
{
	void* buf_data;
	
	int err = 0;
	
	__u32 blk_ptr_length = MICROFS_IOFFSET_WIDTH / 8;
	__u32 blk_ptr_offset = microfs_get_offset(inode)
		+ blk_nr * blk_ptr_length;
	
	pr_devel_once("microfs_find_block: first call\n");
	
	if (blk_nr) {
		buf_data = __microfs_read(sb, blk_ptr_offset - blk_ptr_length,
			blk_ptr_length);
		if (unlikely(IS_ERR(buf_data))) {
			err = PTR_ERR(buf_data);
			goto err_io;
		}
		*blk_data_offset = __le32_to_cpu(*(__le32*)buf_data);
	} else {
		*blk_data_offset = microfs_get_offset(inode)
			+ blk_ptrs * blk_ptr_length;
	}
	
	buf_data = __microfs_read(sb, blk_ptr_offset, blk_ptr_length);
	if (unlikely(IS_ERR(buf_data))) {
		err = PTR_ERR(buf_data);
		goto err_io;
	}
	
	*blk_data_length = __le32_to_cpu(*(__le32*)buf_data)
		- *blk_data_offset;
	
	pr_spam("microfs_find_block: blk_data_offset=0x%x, blk_data_length=%u\n",
		*blk_data_offset, *blk_data_length);
	
err_io:
	return err;
}

static int __microfs_copy_metadata(struct super_block* sb,
	void* data, struct buffer_head** bhs, __u32 nbhs,
	__u32 offset, __u32 length)
{
	__u32 i;
	__u32 buf_offset;
	
	struct microfs_sb_info* sbi = MICROFS_SB(sb);
	
	(void)data;
	(void)nbhs;
	(void)offset;
	
	pr_spam("__microfs_copy_metadata: offset=0x%x, length=%u\n",
		offset, length);
	
	for (i = 0, buf_offset = 0; buf_offset < length;
			i += 1, buf_offset += PAGE_CACHE_SIZE) {
		memcpy(sbi->si_metadatabuf.d_data + buf_offset,
			bhs[i]->b_data, PAGE_CACHE_SIZE);
	}
	
	return 0;
}

static int __microfs_copy_filedata_2step(struct super_block* sb,
	void* data, struct buffer_head** bhs, __u32 nbhs,
	__u32 offset, __u32 length)
{
	__u32 inflated = 0;
	__u32 unused;
	__u32 page;
	__u32 buf_offset;
	
	struct microfs_sb_info* sbi = MICROFS_SB(sb);
	struct microfs_readpage_request* rdreq = data;
	struct z_stream_s* zstrm = &sbi->si_rdzstream;
	
	int err = 0;
	int zerr = 0;
	
	mutex_lock(&sbi->si_rdmutex);
	
	pr_spam("__microfs_copy_filedata_2step: offset=0x%x, length=%u\n",
		offset, length);
	
	if (sbi->si_filedatabuf.d_offset != offset) {
		__u32 bh = 0;
		
		int repeat = 0;
		
		zstrm->avail_in = 0;
		zstrm->next_in = NULL;
		zstrm->avail_out = 0;
		zstrm->next_out = NULL;
		
		__microfs_inflate_reset(sbi);
		
		zstrm->avail_out = sbi->si_filedatabuf.d_size;
		zstrm->next_out = sbi->si_filedatabuf.d_data;
		
		do {
			err = __microfs_inflate_bhs(sbi, bhs, nbhs, &length,
				&bh, &rdreq->rr_bhoffset, &inflated, &zerr);
			repeat = __microfs_inflate_more(err, zerr, zstrm,
				length, 0);
		} while (repeat);
		
		if (err) {
			goto err_inflate;
		} else if (!err && zerr != Z_STREAM_END) {
			pr_err("__microfs_copy_filedata_2step: zlib not at streams end"
				" but no error is reported by __microfs_inflate_bhs\n");
			err = -EIO;
			goto err_inflate;
		}
		
		sbi->si_filedatabuf.d_offset = offset;
	}
	
	for (page = 0, buf_offset = 0; page < rdreq->rr_npages;
			page += 1, buf_offset += PAGE_CACHE_SIZE) {
		if (rdreq->rr_pages[page]) {
			void* page_data = kmap(rdreq->rr_pages[page]);
			memcpy(page_data, sbi->si_filedatabuf.d_data + buf_offset, PAGE_CACHE_SIZE);
			kunmap(rdreq->rr_pages[page]);
		}
	}
	
	unused = buf_offset + PAGE_CACHE_SIZE - inflated;
	if (unused) {
		void* page_data = kmap(rdreq->rr_pages[rdreq->rr_npages - 1]);
		pr_spam("__microfs_copy_filedata_direct: zeroing %u bytes for page %u\n",
			unused, page);
		memset(page_data + (PAGE_CACHE_SIZE - unused), 0, unused);
		kunmap(rdreq->rr_pages[rdreq->rr_npages - 1]);
	}
	
err_inflate:
	mutex_unlock(&sbi->si_rdmutex);
	return err;
}

static int __microfs_copy_filedata_direct(struct super_block* sb,
	void* data, struct buffer_head** bhs, __u32 nbhs,
	__u32 offset, __u32 length)
{
	__u32 bh;
	__u32 page;
	__u32 unused;
	__u32 inflated = 0;
	
	void* page_data;
	
	struct microfs_sb_info* sbi = MICROFS_SB(sb);
	struct microfs_readpage_request* rdreq = data;
	struct z_stream_s* zstrm = &sbi->si_rdzstream;
	
	int err = 0;
	int zerr = Z_ERRNO;
	int repeat = 0;
	
	mutex_lock(&sbi->si_rdmutex);
	
	pr_spam("__microfs_copy_filedata_direct: offset=0x%x, length=%u\n",
		offset, length);
	
	zstrm->avail_in = 0;
	zstrm->next_in = NULL;
	zstrm->avail_out = 0;
	zstrm->next_out = NULL;
	
	__microfs_inflate_reset(sbi);
	
	bh = 0;
	page = 0;
	page_data = NULL;
	
	do {
		if (zstrm->avail_out == 0) {
			if (page_data) {
				page_data = NULL;
				kunmap(rdreq->rr_pages[page++]);
			}
			if (page < rdreq->rr_npages) {
				page_data = kmap(rdreq->rr_pages[page]);
				zstrm->avail_out = PAGE_CACHE_SIZE;
				zstrm->next_out = page_data;
			} else {
				zstrm->next_out = NULL;
			}
		}
		
		err = __microfs_inflate_bhs(sbi, bhs, nbhs, &length,
			&bh, &rdreq->rr_bhoffset, &inflated, &zerr);
		
		repeat = __microfs_inflate_more(err, zerr, zstrm,
			length, page + 1 < rdreq->rr_npages);
		
	} while (repeat);
	
	if (page_data) {
		page_data = NULL;
		kunmap(rdreq->rr_pages[page]);
	}
	
	if (err) {
		goto err_inflate;
	} else if (!err && zerr != Z_STREAM_END) {
		pr_err("__microfs_copy_filedata_direct: zlib not at streams end"
			" but no error is reported by __microfs_inflate_bhs\n");
		err = -EIO;
		goto err_inflate;
	}
	
	unused = zstrm->avail_out;
	if (unused) {
		void* page_data = kmap(rdreq->rr_pages[rdreq->rr_npages - 1]);
		pr_spam("__microfs_copy_filedata_direct: zeroing %u bytes for page %u\n",
			unused, page);
		memset(page_data + (PAGE_CACHE_SIZE - unused), 0, unused);
		kunmap(rdreq->rr_pages[rdreq->rr_npages - 1]);
	}
	
err_inflate:
	mutex_unlock(&sbi->si_rdmutex);
	return err;
}

int __microfs_read_blks(struct super_block* sb, struct address_space* mapping,
	void* data, microfs_read_blks_consumer consumer,
	__u32 offset, __u32 length)
{
	__u32 i;
	__u32 n;
	__u32 dev_blks;
	
	int err = 0;
	
	__u32 blk_nr;
	__u32 blk_offset = offset - (offset & PAGE_CACHE_MASK);
	
	__u32 nbhs = i_blkptrs(blk_offset + length, PAGE_CACHE_SIZE);
	struct buffer_head** bhs = kmalloc(nbhs * sizeof(void*), GFP_KERNEL);
	
	if (!bhs) {
		pr_err("__microfs_read_blks: failed to allocate bhs (%u slots)\n", nbhs);
		err = -ENOMEM;
		goto err_mem;
	}
	
	blk_nr = offset >> PAGE_CACHE_SHIFT;
	dev_blks = sb->s_bdev->bd_inode->i_size >> PAGE_CACHE_SHIFT;
	
	pr_spam("__microfs_read_blks: offset=0x%x, blk_offset=%u, length=%u\n",
		offset, blk_offset, length);
	pr_spam("__microfs_read_blks: nbhs=%u, blk_nr=%u, dev_blks=%u\n",
		nbhs, blk_nr, dev_blks);
	
	for (i = 0, n = 0; i < nbhs; ++i) {
		if (likely(blk_nr + i < dev_blks)) {
			bhs[n++] = sb_getblk(sb, blk_nr + i);
			if (unlikely(bhs[n - 1] == NULL)) {
				pr_err("__microfs_read_blks: failed to get a bh for block %u\n",
					blk_nr + i);
				err = -EIO;
				goto err_bhs;
			} else {
				pr_spam("__microfs_read_blks: got bh 0x%p for block %u\n",
					bhs[n - 1], blk_nr + i);
			}
		} else {
			/* It is not possible to fill the entire read buffer this
			 * time. "Welcome to the end of the image."
			 */
			bhs[i] = NULL;
		}
	}
	
	ll_rw_block(READ, n, bhs);
	
	pr_spam("__microfs_read_blks: bhs submitted for reading\n");
	
	for (i = 0; i < n; ++i) {
		wait_on_buffer(bhs[i]);
		if (unlikely(!buffer_uptodate(bhs[i]))) {
			pr_err("__microfs_read_blks: bh 0x%p (#%u) is not up-to-date\n", bhs[i], i);
			err = -EIO;
			goto err_bhs;
		}
	}
	
	pr_spam("__microfs_read_blks: reading complete\n");
	
	err = consumer(sb, data, bhs, n, offset, length);
	
	pr_spam("__microfs_read_blks: processing complete\n");
	
err_bhs:
	for (i = 0; i < n; ++i) {
		pr_spam("__microfs_read_blks: releasing bh 0x%p\n", bhs[i]);
		put_bh(bhs[i]);
	}
	kfree(bhs);
err_mem:
	return err;
}

/* The caller must hold %microfs_sb_info.si_rdmutex.
 */
void* __microfs_read(struct super_block* sb, __u32 offset, __u32 length)
{
	struct microfs_sb_info* sbi = MICROFS_SB(sb);
	struct address_space* mapping = sb->s_bdev->bd_inode->i_mapping;
	
	__u32 buf_offset;
	__u32 data_offset;
	
	int err = 0;
	
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
	ABORT_READ_ON(length > sbi->si_metadatabuf.d_size);
	ABORT_READ_ON(offset + length > mapping->host->i_size);
	
#undef ABORT_READ_ON
	
	/* Check the work buffer, it might contain the requested data.
	 */
	if (offset >= sbi->si_metadatabuf.d_offset) {
		buf_offset = offset - sbi->si_metadatabuf.d_offset;
		if (buf_offset + length <= sbi->si_metadatabuf.d_size) {
			pr_devel_once("__microfs_read: first cache hit\n");
			return sbi->si_metadatabuf.d_data + buf_offset;
		}
	}
	
	/* And it might not.
	 */
	pr_devel_once("__microfs_read: first cache miss\n");
	
	data_offset = offset & PAGE_CACHE_MASK;
	buf_offset = offset - data_offset;
	
	/* %microfs_sb_info.si_metadatabuf will not be updated if an
	 * error is encountered.
	 */
	err = __microfs_read_blks(sb, mapping, NULL,
		__microfs_copy_metadata, offset, sbi->si_metadatabuf.d_size);
	
	if (unlikely(err))
		return ERR_PTR(err);
	
	sbi->si_metadatabuf.d_offset = data_offset;
	return sbi->si_metadatabuf.d_data + buf_offset;
}

int __microfs_readpage(struct file* file, struct page* page)
{
	struct inode* inode = page->mapping->host;
	struct super_block* sb = inode->i_sb;
	struct microfs_sb_info* sbi = MICROFS_SB(sb);
	
	int err = 0;
	int small_blks = sbi->si_blksz <= PAGE_CACHE_SIZE;
	
	__u32 i;
	__u32 j;
	
	__u32 data_offset = 0;
	__u32 data_length = 0;
	__u32 blk_data_offset = 0;
	__u32 blk_data_length = 0;
	
	__u32 pgholes = 0;
	
	__u32 blk_ptrs = i_blkptrs(i_size_read(inode), sbi->si_blksz);
	__u32 blk_nr = small_blks?
		page->index * (PAGE_CACHE_SIZE >> sbi->si_blkshift):
		page->index / (sbi->si_blksz / PAGE_CACHE_SIZE);
	
	int index_mask = small_blks?
		0: (1 << (sbi->si_blkshift - PAGE_CACHE_SHIFT)) - 1;
	
	__u32 max_index = i_blkptrs(i_size_read(inode), PAGE_CACHE_SIZE);
	__u32 start_index = (small_blks? page->index: page->index & ~index_mask);
	__u32 end_index = (small_blks? page->index: start_index | index_mask) + 1;
	
	struct microfs_readpage_request rdreq;
	
	if (end_index > max_index)
		end_index = max_index;
	
	pr_spam("__microfs_readpage: sbi->si_blksz=%u, blk_ptrs=%u, blk_nr=%u\n",
		sbi->si_blksz, blk_ptrs, blk_nr);
	pr_spam("__microfs_readpage: start_index=%u, end_index=%u, max_index=%u\n",
		start_index, end_index, max_index);
	
	mutex_lock(&sbi->si_rdmutex);
	for (i = 0; data_length < PAGE_CACHE_SIZE && blk_nr + i < blk_ptrs; ++i) {
		err = __microfs_find_block(sb, inode, blk_ptrs, blk_nr + i,
			&blk_data_offset, &blk_data_length);
		if (unlikely(err)) {
			mutex_unlock(&sbi->si_rdmutex);
			goto err_find_block;
		}
		if (!data_offset)
			data_offset = blk_data_offset;
		data_length += blk_data_length;
	}
	mutex_unlock(&sbi->si_rdmutex);
	
	pr_spam("__microfs_readpage: data_offset=0x%x, data_length=%u\n",
		data_offset, data_length);
	
	rdreq.rr_bhoffset = data_offset - (data_offset & PAGE_CACHE_MASK);
	rdreq.rr_npages = end_index - start_index;
	rdreq.rr_pages = kmalloc(rdreq.rr_npages * sizeof(void*), GFP_KERNEL);
	if (!rdreq.rr_pages) {
		pr_err("__microfs_readpage: failed to allocate rdreq.rr_pages (%u slots)\n",
			rdreq.rr_npages);
		err = -ENOMEM;
		goto err_mem;
	}
	
	pr_spam("__microfs_readpage: rdreq.rr_pages=0x%p, rdreq.rr_npages=%u\n",
		rdreq.rr_pages, rdreq.rr_npages);
	
	for (i = 0, j = start_index; j < end_index; ++i, ++j) {
		rdreq.rr_pages[i] = (j == page->index)?
			page: grab_cache_page_nowait(page->mapping, j);
		if (rdreq.rr_pages[i] == page) {
			pr_spam("__microfs_readpage: target page 0x%p at index %u\n",
				page, j);
		} else if (rdreq.rr_pages[i] == NULL) {
			pgholes++;
			pr_spam("__microfs_readpage: busy page at index %u\n", j);
		} else if (PageUptodate(rdreq.rr_pages[i])) {
			unlock_page(rdreq.rr_pages[i]);
			page_cache_release(rdreq.rr_pages[i]);
			rdreq.rr_pages[i] = NULL;
			pgholes++;
			pr_spam("__microfs_readpage: page up to date at index %u\n", j);
		} else {
			pr_spam("__microfs_readpage: new page 0x%p added for index %u\n",
				rdreq.rr_pages[i], j);
		}
	}
	
	pr_spam("__microfs_readpage: pgholes=%u\n", pgholes);
	
	if (pgholes) {
		/* It seems that one or more pages have been reclaimed, but
		 * it is also possible that another thread is trying to read
		 * the same data.
		 */
		err = __microfs_read_blks(sb, page->mapping, &rdreq,
			__microfs_copy_filedata_2step, data_offset, data_length);
	} else {
		/* It is possible to uncompress the file data directly into
		 * the page cache. Neat.
		 */
		err = __microfs_read_blks(sb, page->mapping, &rdreq,
			__microfs_copy_filedata_direct, data_offset, data_length);
	}
	if (unlikely(err)) {
		pr_err("__microfs_readpage: __microfs_read_blks failed\n");
		goto err_io;
	}
	
	for (i = 0; i < rdreq.rr_npages; ++i) {
		if (rdreq.rr_pages[i]) {
			flush_dcache_page(rdreq.rr_pages[i]);
			SetPageUptodate(rdreq.rr_pages[i]);
			unlock_page(rdreq.rr_pages[i]);
			if (rdreq.rr_pages[i] != page)
				page_cache_release(rdreq.rr_pages[i]);
		}
	}
	
	kfree(rdreq.rr_pages);
	
	return 0;
	
err_io:
	pr_spam("__microfs_readpage: failure\n");
	for (i = 0; i < rdreq.rr_npages; ++i) {
		if (rdreq.rr_pages[i]) {
			flush_dcache_page(rdreq.rr_pages[i]);
			SetPageError(rdreq.rr_pages[i]);
			unlock_page(rdreq.rr_pages[i]);
			if (rdreq.rr_pages[i] != page)
				page_cache_release(rdreq.rr_pages[i]);
		}
	}
	kfree(rdreq.rr_pages);
err_mem:
	/* Fall-trough. */
err_find_block:
	return err;
}
