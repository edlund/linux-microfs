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

#include "libinfo_lz4.h"

#ifdef MICROFS_DECOMPRESSOR_LZ4

#include <linux/lz4.h>

struct decompressor_lz4_data {
	char* lz4_inputbuf;
	__u32 lz4_inputbufsz;
	__u32 lz4_inputbufusedsz;
	char* lz4_outputbuf;
	__u32 lz4_outputbufsz;
	__u32 lz4_outputbufusedsz;
	struct page** lz4_pages;
	__u32 lz4_npages;
};

static int decompressor_lz4_create(struct microfs_sb_info* sbi)
{
	__u32 outputbufsz = max_t(__u32, sbi->si_blksz, PAGE_CACHE_SIZE);
	__u32 inputbufsz = max_t(__u32, lz4_compressbound(sbi->si_blksz),
		PAGE_CACHE_SIZE * 2);
	
	struct decompressor_lz4_data* lz4data = kmalloc(sizeof(*lz4data), GFP_KERNEL);
	if (!lz4data)
		goto err_mem_lz4data;
	
	lz4data->lz4_pages = NULL;
	lz4data->lz4_npages = 0;
	
#define LZ4DATA_BUF(Data, Name, Size) \
	do { \
		Data->lz4_##Name##bufsz = Size; \
		Data->lz4_##Name##bufusedsz = 0; \
		Data->lz4_##Name##buf = kmalloc(Size, GFP_KERNEL); \
		if (!Data->lz4_##Name##buf) \
			goto err_mem_lz4data_##Name; \
	} while (0)
	
	pr_spam("decompressor_lz4_create: inputbufsz=%u, outputbufsz=%u\n",
		inputbufsz, outputbufsz);
	
	LZ4DATA_BUF(lz4data, input, inputbufsz);
	LZ4DATA_BUF(lz4data, output, outputbufsz);
	
#undef LZ4DATA_BUF
	
	sbi->si_decompressor_data = lz4data;
	
	return 0;
	
err_mem_lz4data_output:	
	kfree(lz4data->lz4_inputbuf);
err_mem_lz4data_input:
	kfree(lz4data);
err_mem_lz4data:
	return -ENOMEM;
}

static int decompressor_lz4_destroy(struct microfs_sb_info* sbi)
{
	struct decompressor_lz4_data* lz4data = sbi->si_decompressor_data;
	sbi->si_decompressor_data = NULL;
	
	kfree(lz4data->lz4_outputbuf);
	kfree(lz4data->lz4_inputbuf);
	kfree(lz4data);
	
	return 0;
}

static int decompressor_lz4_reset(struct microfs_sb_info* sbi)
{
	(void)sbi;
	return 0;
}

static int decompressor_lz4_exceptionally_begin(struct microfs_sb_info* sbi)
{
	struct decompressor_lz4_data* lz4data = sbi->si_decompressor_data;
	lz4data->lz4_pages = NULL;
	lz4data->lz4_npages = 0;
	return 0;
}

static int decompressor_lz4_nominally_begin(struct microfs_sb_info* sbi,
	struct page** pages, __u32 npages)
{
	struct decompressor_lz4_data* lz4data = sbi->si_decompressor_data;
	lz4data->lz4_pages = pages;
	lz4data->lz4_npages = npages;
	return 0;
}

static int decompressor_lz4_nominally_strm_needpage(
	struct microfs_sb_info* sbi)
{
	(void)sbi;
	return 0;
}

static int decompressor_lz4_nominally_strm_utilizepage(
	struct microfs_sb_info* sbi, struct page* page)
{
	(void)sbi;
	(void)page;
	return 0;
}

static int decompressor_lz4_nominally_strm_releasepage(
	struct microfs_sb_info* sbi, struct page* page)
{
	(void)sbi;
	(void)page;
	return 0;
}

static int decompressor_lz4_consumebhs(struct microfs_sb_info* sbi,
	struct buffer_head** bhs, __u32 nbhs, __u32* length,
	__u32* bh, __u32* bh_offset, __u32* inflated, int* implerr)
{
	__u32 bh_avail;
	__u32 buf_offset = 0;
	
	struct decompressor_lz4_data* lz4data = sbi->si_decompressor_data;
	
	lz4data->lz4_inputbufusedsz = *length;
	
	pr_spam("decompressor_lz4_consumebhs: lz4data->lz4_inputbufusedsz=%u\n",
		lz4data->lz4_inputbufusedsz);
	
	while (*bh < nbhs && *length > 0) {
		pr_spam("decompressor_lz4_consumebhs: *bh=%u, bhs[*bh]=0x%p, nbhs=%u\n",
			*bh, bhs[*bh], nbhs);
		
		bh_avail = min_t(__u32, *length, PAGE_CACHE_SIZE - *bh_offset);
		pr_spam("decompressor_lz4_consumebhs: *bh_offset=%u, *bh_avail=%u\n",
			*bh_offset, bh_avail);
		
		memcpy(lz4data->lz4_inputbuf + buf_offset, bhs[*bh]->b_data
			+ *bh_offset, bh_avail);
		
		pr_spam("decompressor_lz4_consumebhs: *length=%u, *bh_offset=%u,"
				" buf_offset=%u\n",
			*length, *bh_offset, buf_offset);
		
		*bh_offset = 0;
		*bh += 1;
		*length -= bh_avail;
		buf_offset += bh_avail;
	}
	
	return 0;
}

static int decompressor_lz4_continue(struct microfs_sb_info* sbi,
	int err, int implerr, __u32 length, int more_avail_out)
{
	(void)sbi;
	(void)err;
	(void)implerr;
	(void)length;
	(void)more_avail_out;
	return 0;
}

static int decompressor_lz4_end(struct microfs_sb_info* sbi,
	int* err, int* implerr, __u32* decompressed)
{
	int i;
	
	__u32 avail;
	__u32 offset;
	
	struct decompressor_lz4_data* lz4data = sbi->si_decompressor_data;
	
	size_t outputsz = lz4data->lz4_pages?
		lz4data->lz4_outputbufsz: sbi->si_filedatabuf.d_size;
	char* output = lz4data->lz4_pages?
		lz4data->lz4_outputbuf: sbi->si_filedatabuf.d_data;
	
	if (*err) {
		goto err_decompress;
	}
	
	pr_spam("decompressor_lz4_end: lz4data->lz4_pages=0x%p, lz4data->lz4_npages=%u\n",
			lz4data->lz4_pages, lz4data->lz4_npages);
	pr_spam("decompressor_lz4_end: output=0x%p,"
			" lz4data->lz4_outputbuf=0x%p, sbi->si_filedatabuf.d_data=0x%p\n",
		output, lz4data->lz4_outputbuf, sbi->si_filedatabuf.d_data);
	
	*implerr = lz4_decompress_unknownoutputsize(
		lz4data->lz4_inputbuf, lz4data->lz4_inputbufusedsz,
		output, &outputsz);
	
	if (*implerr < 0) {
		*err = -EIO;
		goto err_decompress;
	}
	
	*decompressed = outputsz;
	
	pr_spam("decompressor_lz4_end: outputsz=%zu\n", outputsz);
	
	if (lz4data->lz4_pages) {
		/* Called by %__microfs_copy_filedata_nominally. Copy the data
		 * to the page cache pages.
		 */
		for (i = 0, avail = 0, offset = 0;
				i < lz4data->lz4_npages && outputsz > 0;
				i += 1, offset += PAGE_CACHE_SIZE) {
			void* page_data = kmap(lz4data->lz4_pages[i]);
			avail = min_t(__u32, outputsz, PAGE_CACHE_SIZE);
			
			pr_spam("decompressor_lz4_end: i=%d, offset=%u, avail=%u, outputsz=%zu\n",
				i, offset, avail, outputsz);
			
			outputsz -= avail;
			
			memcpy(page_data, output + offset, avail);
			kunmap(lz4data->lz4_pages[i]);
		}
	} else {
		/* Called by %__microfs_copy_filedata_exceptionally. The data
		 * is stored in the correct buffer. Everything is fine.
		 */
		sbi->si_filedatabuf.d_used = outputsz;
	}
	
	pr_spam("decompressor_lz4_end: done\n");
	return 0;
	
err_decompress:
	pr_err("decompressor_lz4_end: failed to decompress data\n");
	return *err;
}

const struct microfs_decompressor decompressor_lz4 = {
	.dc_info = &libinfo_lz4,
	.dc_compiled = 1,
	.dc_create = decompressor_lz4_create,
	.dc_destroy = decompressor_lz4_destroy,
	.dc_reset = decompressor_lz4_reset,
	.dc_exceptionally_begin = decompressor_lz4_exceptionally_begin,
	.dc_nominally_begin = decompressor_lz4_nominally_begin,
	.dc_nominally_strm_needpage = decompressor_lz4_nominally_strm_needpage,
	.dc_nominally_strm_utilizepage = decompressor_lz4_nominally_strm_utilizepage,
	.dc_nominally_strm_releasepage = decompressor_lz4_nominally_strm_releasepage,
	.dc_consumebhs = decompressor_lz4_consumebhs,
	.dc_continue = decompressor_lz4_continue,
	.dc_end = decompressor_lz4_end
};

#else

const struct microfs_decompressor decompressor_lz4 = {
	.dc_info = &libinfo_lz4,
	.dc_compiled = 0
};

#endif

