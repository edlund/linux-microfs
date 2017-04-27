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

#if defined(DEBUG) && defined(DEBUG_READS) && !defined(pr_read)
#define pr_read(fmt, ...) pr_devel(fmt, ##__VA_ARGS__)
#else
#ifndef pr_read
#define pr_read(fmt, ...)
#endif
#endif

struct microfs_readpage_request {
	struct page** rr_pages;
	__u32 rr_npages;
	__u32 rr_bhoffset;
};

/* The caller must hold the appropriate buffer lock.
 */
static int __microfs_find_block(struct super_block* const sb,
	struct inode* const inode, __u32 blk_ptrs, __u32 blk_nr,
	__u32* const blk_data_offset,
	__u32* const blk_data_length)
{
	void* buf_data;
	
	struct microfs_sb_info* sbi = MICROFS_SB(sb);
	
	int err = 0;
	
	__u32 blk_ptr_length = MICROFS_IOFFSET_WIDTH / 8;
	__u32 blk_ptr_offset = microfs_get_offset(inode)
		+ blk_nr * blk_ptr_length;
	
	pr_devel_once("microfs_find_block: first call\n");
	
	buf_data = __microfs_read(sb, &sbi->si_metadata_blkptrbuf,
		blk_ptr_offset, blk_ptr_length);
	if (unlikely(IS_ERR(buf_data))) {
		err = PTR_ERR(buf_data);
		goto err_io;
	}
	*blk_data_offset = __le32_to_cpu(*(__le32*)buf_data);
	
	buf_data = __microfs_read(sb, &sbi->si_metadata_blkptrbuf,
		blk_ptr_offset + blk_ptr_length, blk_ptr_length);
	if (unlikely(IS_ERR(buf_data))) {
		err = PTR_ERR(buf_data);
		goto err_io;
	}
	
	*blk_data_length = __le32_to_cpu(*(__le32*)buf_data)
		- *blk_data_offset;
	
	pr_read("microfs_find_block: blk_data_offset=0x%x, blk_data_length=%u\n",
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
	
	struct microfs_data_buffer* destbuf = data;
	
	(void)sb;
	(void)nbhs;
	(void)offset;
	
	pr_read("__microfs_copy_metadata: offset=0x%x, length=%u\n",
		offset, length);
	
	for (
		i = 0, buf_offset = 0, destbuf->d_used = 0;
		i < nbhs && buf_offset < length;
		i += 1, buf_offset += PAGE_SIZE
	) {
		destbuf->d_used += PAGE_SIZE;
		memcpy(destbuf->d_data + buf_offset, bhs[i]->b_data, PAGE_SIZE);
	}
	
	return 0;
}

static int __microfs_recycle_metadata(struct super_block* sb,
	void* data, __u32 offset, __u32 length,
	microfs_read_blks_consumer consumer)
{
	__u32 buf_offset;
	
	struct microfs_data_buffer* destbuf = data;
	
	(void)sb;
	(void)consumer;
	
	if (offset >= destbuf->d_offset) {
		buf_offset = offset - destbuf->d_offset;
		if (buf_offset + length <= destbuf->d_size)
			return 0;
	}
	return -EIO;
}

static int __microfs_copy_filedata_exceptionally(struct super_block* sb,
	void* data, struct buffer_head** bhs, __u32 nbhs,
	__u32 offset, __u32 length)
{
	__u32 decompressed = 0;
	__u32 remaining;
	__u32 available;
	__u32 unused;
	__u32 page;
	__u32 buf_offset;
	
	void* decompressor = NULL;
	struct microfs_sb_info* sbi = MICROFS_SB(sb);
	struct microfs_readpage_request* rdreq = data;
	
	int err = 0;
	int implerr = 0;
	
	if (bhs) {
		mutex_lock(&sbi->si_filedatabuf.d_mutex);
	}
	
	err = sbi->si_decompressor_data->dd_get(sbi, &decompressor);
	if (err) {
		pr_err("__microfs_copy_filedata_exceptionally:"
			" failed to get the decompressor data\n");
		goto err_dd_get;
	}
	
	pr_read("__microfs_copy_filedata_exceptionally: offset=0x%x, length=%u\n",
		offset, length);
	
	if (bhs) {
		__u32 bh = 0;
		
		int repeat = 0;
		
		sbi->si_decompressor->dc_reset(sbi, decompressor);
		sbi->si_decompressor->dc_exceptionally_begin(sbi, decompressor);
		
		do {
			err = sbi->si_decompressor->dc_consumebhs(sbi, decompressor,
				bhs, nbhs, &length, &bh, &rdreq->rr_bhoffset,
				&decompressed, &implerr);
			repeat = sbi->si_decompressor->dc_continue(sbi, decompressor,
				err, implerr, length, 0);
		} while (repeat);
		
		if (sbi->si_decompressor->dc_end(sbi, decompressor, &err, &implerr, &decompressed) < 0)
			goto err_inflate;
		
		sbi->si_filedatabuf.d_offset = offset;
		sbi->si_filedatabuf.d_used = decompressed;
	} else {
		decompressed = sbi->si_filedatabuf.d_used;
		pr_read("__microfs_copy_filedata_exceptionally: cache hit for offset 0x%x"
			" - %u bytes already decompressed in sbi->si_filedatabuf\n",
				offset, decompressed);
	}
	
	for (page = 0, buf_offset = 0, remaining = decompressed; page < rdreq->rr_npages;
			page += 1, buf_offset += PAGE_SIZE) {
		available = min_t(__u32, remaining, PAGE_SIZE);
		unused = PAGE_SIZE - available;
		remaining -= available;
		
		if (rdreq->rr_pages[page]) {
			void* page_data = kmap(rdreq->rr_pages[page]);
			pr_read("__microfs_copy_filedata_exceptionally: buf_offset=%u, remaining=%u\n",
				buf_offset, remaining);
			pr_read("__microfs_copy_filedata_exceptionally: copying %u bytes to page %u\n",
				available, page);
			pr_read("__microfs_copy_filedata_exceptionally: zeroing %u bytes for page %u\n",
				unused, page);
			memcpy(page_data, sbi->si_filedatabuf.d_data + buf_offset, available);
			memset(page_data + available, 0, unused);
			kunmap(rdreq->rr_pages[page]);
		}
	}
	
err_inflate:
	WARN_ON(sbi->si_decompressor_data->dd_put(sbi, &decompressor));
err_dd_get:
	if (bhs) {
		mutex_unlock(&sbi->si_filedatabuf.d_mutex);
	}
	return err;
}

static int __microfs_recycle_filedata_exceptionally(struct super_block* sb,
	void* data, __u32 offset, __u32 length,
	microfs_read_blks_consumer consumer)
{
	int err = 0;
	int cached = 0;
	
	struct microfs_sb_info* sbi = MICROFS_SB(sb);
	
	if (sbi->si_filedatabuf.d_offset == offset) {
		mutex_lock(&sbi->si_filedatabuf.d_mutex);
		if (likely(sbi->si_filedatabuf.d_offset == offset)) {
			cached = 1;
			err = consumer(sb, data, NULL, 0, offset, length);
		} else {
			pr_read("__microfs_recycle_filedata_exceptionally:"
				" near cache miss at offset 0x%x (hit stolen)\n", offset);
		}
		mutex_unlock(&sbi->si_filedatabuf.d_mutex);
	}
	return !err && cached? 0: -EIO;
}

static int __microfs_copy_filedata_nominally(struct super_block* sb,
	void* data, struct buffer_head** bhs, __u32 nbhs,
	__u32 offset, __u32 length)
{
	__u32 bh;
	__u32 page;
	__u32 unused;
	__u32 decompressed = 0;
	
	void* decompressor = NULL;
	struct microfs_sb_info* sbi = MICROFS_SB(sb);
	struct microfs_readpage_request* rdreq = data;
	
	int err = 0;
	int repeat = 0;
	int implerr = 0;
	
	int strm_release = 0;
	
	err = sbi->si_decompressor_data->dd_get(sbi, &decompressor);
	if (err) {
		pr_err("__microfs_copy_filedata_nominally:"
			" failed to get the decompressor data\n");
		goto err_dd_get;
	}
	
	pr_read("__microfs_copy_filedata_nominally: offset=0x%x, length=%u\n",
		offset, length);
	
	sbi->si_decompressor->dc_reset(sbi, decompressor);
	sbi->si_decompressor->dc_nominally_begin(sbi, decompressor,
		rdreq->rr_pages, rdreq->rr_npages);
	
	bh = 0;
	page = 0;
	
	do {
		if (sbi->si_decompressor->dc_copy_nominally_needpage(sbi, decompressor)) {
			if (strm_release) {
				strm_release = sbi->si_decompressor->dc_copy_nominally_releasepage(
					sbi, decompressor, rdreq->rr_pages[page++]);
			}
			strm_release = sbi->si_decompressor->dc_copy_nominally_utilizepage(
				sbi, decompressor, page < rdreq->rr_npages? rdreq->rr_pages[page]: NULL);
		}
		
		err = sbi->si_decompressor->dc_consumebhs(sbi, decompressor,
			bhs, nbhs, &length, &bh, &rdreq->rr_bhoffset, &decompressed, &implerr);
		
		repeat = sbi->si_decompressor->dc_continue(sbi, decompressor, err, implerr,
			length, page + 1 < rdreq->rr_npages);
		
	} while (repeat);
	
	if (strm_release) {
		sbi->si_decompressor->dc_copy_nominally_releasepage(sbi,
			decompressor, rdreq->rr_pages[page]);
	}
	
	if (sbi->si_decompressor->dc_end(sbi, decompressor, &err, &implerr, &decompressed) < 0)
		goto err_inflate;
	
	unused = (rdreq->rr_npages * PAGE_SIZE) - decompressed;
	if (unused) {
		page = rdreq->rr_npages - 1;
		do {
			void* page_data = kmap(rdreq->rr_pages[page]);
			__u32 page_avail = min_t(__u32, unused, PAGE_SIZE);
			pr_read("__microfs_copy_filedata_nominally: zeroing %u bytes for page %u\n",
				page_avail, page);
			memset(page_data + (PAGE_SIZE - page_avail), 0, page_avail);
			kunmap(rdreq->rr_pages[page]);
			page -= 1;
			unused -= page_avail;
		} while (unused);
	}
	
err_inflate:
	WARN_ON(sbi->si_decompressor_data->dd_put(sbi, &decompressor));
err_dd_get:
	return err;
}

static int __microfs_recycle_filedata_nominally(struct super_block* sb,
	void* data, __u32 offset, __u32 length,
	microfs_read_blks_consumer consumer)
{
	(void)sb;
	(void)data;
	(void)offset;
	(void)length;
	(void)consumer;
	
	return -EIO;
}

int __microfs_read_blks(struct super_block* sb,
	struct address_space* mapping, void* data,
	microfs_read_blks_recycler recycler,
	microfs_read_blks_consumer consumer,
	__u32 offset, __u32 length)
{
	__u32 i;
	__u32 n;
	__u32 dev_blks;
	
	int err = 0;
	
	__u32 blk_nr;
	__u32 blk_offset;
	
	__u32 nbhs;
	struct buffer_head** bhs;
	
	if (recycler(sb, data, offset, length, consumer) == 0)
		goto out_cachehit;
	
	blk_offset = offset - (offset & PAGE_MASK);
	
	nbhs = i_blks(blk_offset + length, PAGE_SIZE);
	bhs = kmalloc(nbhs * sizeof(void*), GFP_KERNEL);
	if (!bhs) {
		pr_err("__microfs_read_blks: failed to allocate bhs (%u slots)\n", nbhs);
		err = -ENOMEM;
		goto err_mem;
	}
	
	blk_nr = offset >> PAGE_SHIFT;
	dev_blks = sb->s_bdev->bd_inode->i_size >> PAGE_SHIFT;
	
	pr_read("__microfs_read_blks: offset=0x%x, blk_offset=%u, length=%u\n",
		offset, blk_offset, length);
	pr_read("__microfs_read_blks: nbhs=%u, blk_nr=%u, dev_blks=%u\n",
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
				pr_read("__microfs_read_blks: got bh 0x%p for block %u\n",
					bhs[n - 1], blk_nr + i);
			}
		} else {
			/* It is not possible to fill the entire read buffer this
			 * time. "Welcome to the end of the image."
			 */
			bhs[i] = NULL;
		}
	}
	
	ll_rw_block(REQ_OP_READ, 0, n, bhs);
	
	pr_read("__microfs_read_blks: bhs submitted for reading\n");
	
	for (i = 0; i < n; ++i) {
		wait_on_buffer(bhs[i]);
		if (unlikely(!buffer_uptodate(bhs[i]))) {
			pr_err("__microfs_read_blks: bh 0x%p (#%u) is not up-to-date\n", bhs[i], i);
			err = -EIO;
			goto err_bhs;
		}
	}
	
	pr_read("__microfs_read_blks: reading complete\n");
	
	err = consumer(sb, data, bhs, n, offset, length);
	
	pr_read("__microfs_read_blks: processing complete\n");
	
err_bhs:
	for (i = 0; i < n; ++i) {
		pr_read("__microfs_read_blks: releasing bh 0x%p\n", bhs[i]);
		put_bh(bhs[i]);
	}
	kfree(bhs);
err_mem:
out_cachehit:
	return err;
}

/* The caller must hold the appropriate buffer lock.
 */
void* __microfs_read(struct super_block* sb,
	struct microfs_data_buffer* destbuf, __u32 offset, __u32 length)
{
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
	ABORT_READ_ON(length > destbuf->d_size);
	ABORT_READ_ON(offset + length > mapping->host->i_size);
	
#undef ABORT_READ_ON
	
	data_offset = offset & PAGE_MASK;
	buf_offset = offset - data_offset;
	
	/* %destbuf will not be updated if an error is encountered.
	 */
	err = __microfs_read_blks(sb, mapping, destbuf,
		__microfs_recycle_metadata, __microfs_copy_metadata,
		offset, destbuf->d_size);
	
	if (unlikely(err))
		return ERR_PTR(err);
	
	destbuf->d_offset = data_offset;
	return destbuf->d_data + buf_offset;
}

int __microfs_readpage(struct file* file, struct page* page)
{
	struct inode* inode = page->mapping->host;
	struct super_block* sb = inode->i_sb;
	struct microfs_sb_info* sbi = MICROFS_SB(sb);
	
	int err = 0;
	int small_blks = sbi->si_blksz <= PAGE_SIZE;
	
	__u32 i;
	__u32 j;
	
	__u32 data_offset = 0;
	__u32 data_length = 0;
	__u32 blk_data_offset = 0;
	__u32 blk_data_length = 0;
	
	__u32 pgholes = 0;
	
	__u32 blk_ptrs = i_blks(i_size_read(inode), sbi->si_blksz);
	__u32 blk_nr = small_blks?
		page->index * (PAGE_SIZE >> sbi->si_blkshift):
		page->index / (sbi->si_blksz / PAGE_SIZE);
	
	int index_mask = small_blks?
		0: (1 << (sbi->si_blkshift - PAGE_SHIFT)) - 1;
	
	__u32 max_index = i_blks(i_size_read(inode), PAGE_SIZE);
	__u32 start_index = (small_blks? page->index: page->index & ~index_mask);
	__u32 end_index = (small_blks? page->index: start_index | index_mask) + 1;
	
	struct microfs_readpage_request rdreq;
	
	if (end_index > max_index)
		end_index = max_index;
	
	pr_read("__microfs_readpage: sbi->si_blksz=%u, blk_ptrs=%u, blk_nr=%u\n",
		sbi->si_blksz, blk_ptrs, blk_nr);
	pr_read("__microfs_readpage: start_index=%u, end_index=%u, max_index=%u\n",
		start_index, end_index, max_index);
	
	mutex_lock(&sbi->si_metadata_blkptrbuf.d_mutex);
	for (i = 0; (data_length < PAGE_SIZE && blk_nr + i < blk_ptrs) &&
			(i == 0 || sbi->si_blksz < PAGE_SIZE); ++i) {
		err = __microfs_find_block(sb, inode, blk_ptrs, blk_nr + i,
			&blk_data_offset, &blk_data_length);
		if (unlikely(err)) {
			mutex_unlock(&sbi->si_metadata_blkptrbuf.d_mutex);
			goto err_find_block;
		}
		if (!data_offset)
			data_offset = blk_data_offset;
		data_length += blk_data_length;
	}
	mutex_unlock(&sbi->si_metadata_blkptrbuf.d_mutex);
	
	pr_read("__microfs_readpage: data_offset=0x%x, data_length=%u\n",
		data_offset, data_length);
	
	rdreq.rr_bhoffset = data_offset - (data_offset & PAGE_MASK);
	rdreq.rr_npages = end_index - start_index;
	rdreq.rr_pages = kmalloc(rdreq.rr_npages * sizeof(void*), GFP_KERNEL);
	if (!rdreq.rr_pages) {
		pr_err("__microfs_readpage: failed to allocate rdreq.rr_pages (%u slots)\n",
			rdreq.rr_npages);
		err = -ENOMEM;
		goto err_mem;
	}
	
	pr_read("__microfs_readpage: rdreq.rr_pages=0x%p, rdreq.rr_npages=%u\n",
		rdreq.rr_pages, rdreq.rr_npages);
	
	for (i = 0, j = start_index; j < end_index; ++i, ++j) {
		rdreq.rr_pages[i] = (j == page->index)?
			page: grab_cache_page_nowait(page->mapping, j);
		if (rdreq.rr_pages[i] == page) {
			pr_read("__microfs_readpage: target page 0x%p at index %u\n",
				page, j);
		} else if (rdreq.rr_pages[i] == NULL) {
			pgholes++;
			pr_read("__microfs_readpage: busy page at index %u\n", j);
		} else if (PageUptodate(rdreq.rr_pages[i])) {
			unlock_page(rdreq.rr_pages[i]);
			put_page(rdreq.rr_pages[i]);
			rdreq.rr_pages[i] = NULL;
			pgholes++;
			pr_read("__microfs_readpage: page up to date at index %u\n", j);
		} else {
			pr_read("__microfs_readpage: new page 0x%p added for index %u\n",
				rdreq.rr_pages[i], j);
		}
	}
	
	pr_read("__microfs_readpage: pgholes=%u\n", pgholes);
	
	if (pgholes) {
		/* It seems that one or more pages have been reclaimed, but
		 * it is also possible that another thread is trying to read
		 * the same data.
		 */
		err = __microfs_read_blks(sb, page->mapping, &rdreq,
			__microfs_recycle_filedata_exceptionally,
			__microfs_copy_filedata_exceptionally,
			data_offset, data_length);
	} else {
		/* It is possible to uncompress the file data directly into
		 * the page cache. Neat.
		 */
		err = __microfs_read_blks(sb, page->mapping, &rdreq,
			__microfs_recycle_filedata_nominally,
			__microfs_copy_filedata_nominally,
			data_offset, data_length);
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
				put_page(rdreq.rr_pages[i]);
		}
	}
	
	kfree(rdreq.rr_pages);
	
	return 0;
	
err_io:
	pr_read("__microfs_readpage: failure\n");
	for (i = 0; i < rdreq.rr_npages; ++i) {
		if (rdreq.rr_pages[i]) {
			flush_dcache_page(rdreq.rr_pages[i]);
			SetPageError(rdreq.rr_pages[i]);
			unlock_page(rdreq.rr_pages[i]);
			if (rdreq.rr_pages[i] != page)
				put_page(rdreq.rr_pages[i]);
		}
	}
	kfree(rdreq.rr_pages);
err_mem:
	/* Fall-trough. */
err_find_block:
	return err;
}

