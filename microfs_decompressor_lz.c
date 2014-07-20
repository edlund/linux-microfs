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

#ifdef MICROFS_DECOMPRESSOR_LZ

struct decompressor_lz_data {
	char* lz_inputbuf;
	__u32 lz_inputbufsz;
	__u32 lz_inputbufusedsz;
	char* lz_outputbuf;
	__u32 lz_outputbufsz;
	__u32 lz_outputbufusedsz;
	struct page** lz_pages;
	__u32 lz_npages;
};

int decompressor_lz_create(struct microfs_sb_info* sbi,
	void** dest, __u32 upperbound)
{
	__u32 outputbufsz = max_t(__u32, sbi->si_blksz, PAGE_CACHE_SIZE);
	__u32 inputbufsz = max_t(__u32, upperbound, PAGE_CACHE_SIZE * 2);
	
	struct decompressor_lz_data* data = kmalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		goto err_mem_data;
	
	data->lz_inputbuf = NULL;
	data->lz_outputbuf = NULL;
	
	data->lz_pages = NULL;
	data->lz_npages = 0;
	
#define LZ_DATA_BUF(Data, Name, Size) \
	do { \
		Data->lz_##Name##bufsz = Size; \
		Data->lz_##Name##bufusedsz = 0; \
		Data->lz_##Name##buf = kmalloc(Size, GFP_KERNEL); \
		if (!Data->lz_##Name##buf) \
			goto err_mem_data_##Name; \
	} while (0)
	
	pr_spam("decompressor_lz_create: inputbufsz=%u, outputbufsz=%u\n",
		inputbufsz, outputbufsz);
	
	LZ_DATA_BUF(data, input, inputbufsz);
	LZ_DATA_BUF(data, output, outputbufsz);
	
#undef LZ_DATA_BUF
	
	*dest = data;
	
	return 0;
	
err_mem_data_output:	
	kfree(data->lz_inputbuf);
err_mem_data_input:
	kfree(data);
err_mem_data:
	return -ENOMEM;

}

int decompressor_lz_destroy(struct microfs_sb_info* sbi, void* data)
{
	struct decompressor_lz_data* lzdata = data;
	
	if (lzdata) {
		kfree(lzdata->lz_outputbuf);
		kfree(lzdata->lz_inputbuf);
		kfree(lzdata);
	}
	
	return 0;
}

int decompressor_lz_reset(struct microfs_sb_info* sbi, void* data)
{
	(void)sbi;
	(void)data;
	return 0;
}

int decompressor_lz_exceptionally_begin(struct microfs_sb_info* sbi, void* data)
{
	struct decompressor_lz_data* lzdata = data;
	lzdata->lz_pages = NULL;
	lzdata->lz_npages = 0;
	return 0;
}

int decompressor_lz_nominally_begin(struct microfs_sb_info* sbi, void* data,
	struct page** pages, __u32 npages)
{
	struct decompressor_lz_data* lzdata = data;
	lzdata->lz_pages = pages;
	lzdata->lz_npages = npages;
	return 0;
}

int decompressor_lz_copy_nominally_needpage(struct microfs_sb_info* sbi,
	void* data)
{
	(void)sbi;
	(void)data;
	return 0;
}

int decompressor_lz_copy_nominally_utilizepage(struct microfs_sb_info* sbi,
	void* data, struct page* page)
{
	(void)sbi;
	(void)data;
	(void)page;
	return 0;
}

int decompressor_lz_copy_nominally_releasepage(struct microfs_sb_info* sbi,
	void* data, struct page* page)
{
	(void)sbi;
	(void)data;
	(void)page;
	return 0;
}

int decompressor_lz_consumebhs(struct microfs_sb_info* sbi, void* data,
	struct buffer_head** bhs, __u32 nbhs, __u32* length,
	__u32* bh, __u32* bh_offset, __u32* inflated, int* implerr)
{
	__u32 bh_avail;
	__u32 buf_offset = 0;
	
	struct decompressor_lz_data* lzdata = data;
	
	lzdata->lz_inputbufusedsz = *length;
	
	pr_spam("decompressor_lz_consumebhs: data->lz_inputbufusedsz=%u\n",
		lzdata->lz_inputbufusedsz);
	
	while (*bh < nbhs && *length > 0) {
		pr_spam("decompressor_lz_consumebhs: *bh=%u, bhs[*bh]=0x%p, nbhs=%u\n",
			*bh, bhs[*bh], nbhs);
		
		bh_avail = min_t(__u32, *length, PAGE_CACHE_SIZE - *bh_offset);
		pr_spam("decompressor_lz_consumebhs: *bh_offset=%u, *bh_avail=%u\n",
			*bh_offset, bh_avail);
		
		memcpy(lzdata->lz_inputbuf + buf_offset, bhs[*bh]->b_data
			+ *bh_offset, bh_avail);
		
		pr_spam("decompressor_lz_consumebhs: *length=%u, *bh_offset=%u,"
				" buf_offset=%u\n",
			*length, *bh_offset, buf_offset);
		
		*bh_offset = 0;
		*bh += 1;
		*length -= bh_avail;
		buf_offset += bh_avail;
	}
	
	return 0;
}

int decompressor_lz_continue(struct microfs_sb_info* sbi, void* data,
	int err, int implerr, __u32 length, int more_avail_out)
{
	(void)sbi;
	(void)data;
	(void)err;
	(void)implerr;
	(void)length;
	(void)more_avail_out;
	return 0;
}

int decompressor_lz_end(struct microfs_sb_info* sbi, void* data,
	int* err, int* implerr, __u32* decompressed,
	decompressor_lz_end_consumer consumer)
{
	int i;
	
	__u32 avail;
	__u32 offset;
	
	struct decompressor_lz_data* lzdata = data;
	
	__u32 outputsz = lzdata->lz_pages?
		lzdata->lz_outputbufsz: sbi->si_filedatabuf.d_size;
	char* output = lzdata->lz_pages?
		lzdata->lz_outputbuf: sbi->si_filedatabuf.d_data;
	
	if (*err) {
		goto err_decompress;
	}
	
	pr_spam("decompressor_lz_end: data->lz_pages=0x%p, data->lz_npages=%u\n",
			lzdata->lz_pages, lzdata->lz_npages);
	pr_spam("decompressor_lz_end: output=0x%p,"
			" data->lz_outputbuf=0x%p, sbi->si_filedatabuf.d_data=0x%p\n",
		output, lzdata->lz_outputbuf, sbi->si_filedatabuf.d_data);
	
	*err = consumer(sbi, data, implerr,
		lzdata->lz_inputbuf, lzdata->lz_inputbufusedsz,
		output, &outputsz);
	if (*err < 0) {
		goto err_decompress;
	}
	
	*decompressed = outputsz;
	
	pr_spam("decompressor_lz_end: outputsz=%u\n", outputsz);
	
	if (lzdata->lz_pages) {
		/* Called by %__microfs_copy_filedata_nominally. Copy the data
		 * to the page cache pages.
		 */
		for (i = 0, avail = 0, offset = 0;
				i < lzdata->lz_npages && outputsz > 0;
				i += 1, offset += PAGE_CACHE_SIZE) {
			void* page_data = kmap_atomic(lzdata->lz_pages[i]);
			avail = min_t(__u32, outputsz, PAGE_CACHE_SIZE);
			
			pr_spam("decompressor_lz_end: i=%d, offset=%u, avail=%u, outputsz=%u\n",
				i, offset, avail, outputsz);
			
			outputsz -= avail;
			
			memcpy(page_data, output + offset, avail);
			kunmap_atomic(page_data);
		}
	} else {
		/* Called by %__microfs_copy_filedata_exceptionally. The data
		 * is stored in the correct buffer. Everything is fine.
		 */
		sbi->si_filedatabuf.d_used = outputsz;
	}
	
	pr_spam("decompressor_lz_end: done\n");
	return 0;
	
err_decompress:
	pr_err("decompressor_lz_end: failed to decompress data\n");
	return *err;
}

#endif

