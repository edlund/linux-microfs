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

#ifdef MICROFS_DECOMPRESSOR_LZX

struct decompressor_lzx_data {
	char* lzx_inputbuf;
	__u32 lzx_inputbufsz;
	__u32 lzx_inputbufusedsz;
	char* lzx_outputbuf;
	__u32 lzx_outputbufsz;
	__u32 lzx_outputbufusedsz;
	struct page** lzx_pages;
	__u32 lzx_npages;
};

int decompressor_lzx_create(struct microfs_sb_info* sbi, __u32 upperbound)
{
	__u32 outputbufsz = max_t(__u32, sbi->si_blksz, PAGE_CACHE_SIZE);
	__u32 inputbufsz = max_t(__u32, upperbound, PAGE_CACHE_SIZE * 2);
	
	struct decompressor_lzx_data* data = kmalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		goto err_mem_data;
	
	data->lzx_pages = NULL;
	data->lzx_npages = 0;
	
#define LZX_DATA_BUF(Data, Name, Size) \
	do { \
		Data->lzx_##Name##bufsz = Size; \
		Data->lzx_##Name##bufusedsz = 0; \
		Data->lzx_##Name##buf = kmalloc(Size, GFP_KERNEL); \
		if (!Data->lzx_##Name##buf) \
			goto err_mem_data_##Name; \
	} while (0)
	
	pr_spam("decompressor_lzx_create: inputbufsz=%u, outputbufsz=%u\n",
		inputbufsz, outputbufsz);
	
	LZX_DATA_BUF(data, input, inputbufsz);
	LZX_DATA_BUF(data, output, outputbufsz);
	
#undef LZX_DATA_BUF
	
	sbi->si_decompressor_data = data;
	
	return 0;
	
err_mem_data_output:	
	kfree(data->lzx_inputbuf);
err_mem_data_input:
	kfree(data);
err_mem_data:
	return -ENOMEM;

}

int decompressor_lzx_destroy(struct microfs_sb_info* sbi)
{
	struct decompressor_lzx_data* data = sbi->si_decompressor_data;
	sbi->si_decompressor_data = NULL;
	
	kfree(data->lzx_outputbuf);
	kfree(data->lzx_inputbuf);
	kfree(data);
	
	return 0;
}

int decompressor_lzx_reset(struct microfs_sb_info* sbi)
{
	(void)sbi;
	return 0;
}

int decompressor_lzx_exceptionally_begin(struct microfs_sb_info* sbi)
{
	struct decompressor_lzx_data* data = sbi->si_decompressor_data;
	data->lzx_pages = NULL;
	data->lzx_npages = 0;
	return 0;
}

int decompressor_lzx_nominally_begin(struct microfs_sb_info* sbi,
	struct page** pages, __u32 npages)
{
	struct decompressor_lzx_data* data = sbi->si_decompressor_data;
	data->lzx_pages = pages;
	data->lzx_npages = npages;
	return 0;
}

int decompressor_lzx_nominally_strm_needpage(
	struct microfs_sb_info* sbi)
{
	(void)sbi;
	return 0;
}

int decompressor_lzx_nominally_strm_utilizepage(
	struct microfs_sb_info* sbi, struct page* page)
{
	(void)sbi;
	(void)page;
	return 0;
}

int decompressor_lzx_nominally_strm_releasepage(
	struct microfs_sb_info* sbi, struct page* page)
{
	(void)sbi;
	(void)page;
	return 0;
}

int decompressor_lzx_consumebhs(struct microfs_sb_info* sbi,
	struct buffer_head** bhs, __u32 nbhs, __u32* length,
	__u32* bh, __u32* bh_offset, __u32* inflated, int* implerr)
{
	__u32 bh_avail;
	__u32 buf_offset = 0;
	
	struct decompressor_lzx_data* data = sbi->si_decompressor_data;
	
	data->lzx_inputbufusedsz = *length;
	
	pr_spam("decompressor_lzx_consumebhs: data->lzx_inputbufusedsz=%u\n",
		data->lzx_inputbufusedsz);
	
	while (*bh < nbhs && *length > 0) {
		pr_spam("decompressor_lzx_consumebhs: *bh=%u, bhs[*bh]=0x%p, nbhs=%u\n",
			*bh, bhs[*bh], nbhs);
		
		bh_avail = min_t(__u32, *length, PAGE_CACHE_SIZE - *bh_offset);
		pr_spam("decompressor_lzx_consumebhs: *bh_offset=%u, *bh_avail=%u\n",
			*bh_offset, bh_avail);
		
		memcpy(data->lzx_inputbuf + buf_offset, bhs[*bh]->b_data
			+ *bh_offset, bh_avail);
		
		pr_spam("decompressor_lzx_consumebhs: *length=%u, *bh_offset=%u,"
				" buf_offset=%u\n",
			*length, *bh_offset, buf_offset);
		
		*bh_offset = 0;
		*bh += 1;
		*length -= bh_avail;
		buf_offset += bh_avail;
	}
	
	return 0;
}

int decompressor_lzx_continue(struct microfs_sb_info* sbi,
	int err, int implerr, __u32 length, int more_avail_out)
{
	(void)sbi;
	(void)err;
	(void)implerr;
	(void)length;
	(void)more_avail_out;
	return 0;
}

int decompressor_lzx_end(struct microfs_sb_info* sbi,
	int* err, int* implerr, __u32* decompressed,
	decompressor_lzx_end_consumer consumer)
{
	int i;
	
	__u32 avail;
	__u32 offset;
	
	struct decompressor_lzx_data* data = sbi->si_decompressor_data;
	
	__u32 outputsz = data->lzx_pages?
		data->lzx_outputbufsz: sbi->si_filedatabuf.d_size;
	char* output = data->lzx_pages?
		data->lzx_outputbuf: sbi->si_filedatabuf.d_data;
	
	if (*err) {
		goto err_decompress;
	}
	
	pr_spam("decompressor_lzx_end: data->lzx_pages=0x%p, data->lzx_npages=%u\n",
			data->lzx_pages, data->lzx_npages);
	pr_spam("decompressor_lzx_end: output=0x%p,"
			" data->lzx_outputbuf=0x%p, sbi->si_filedatabuf.d_data=0x%p\n",
		output, data->lzx_outputbuf, sbi->si_filedatabuf.d_data);
	
	*err = consumer(sbi, implerr,
		data->lzx_inputbuf, data->lzx_inputbufusedsz,
		output, &outputsz);
	if (*err < 0) {
		goto err_decompress;
	}
	
	*decompressed = outputsz;
	
	pr_spam("decompressor_lzx_end: outputsz=%zu\n", outputsz);
	
	if (data->lzx_pages) {
		/* Called by %__microfs_copy_filedata_nominally. Copy the data
		 * to the page cache pages.
		 */
		for (i = 0, avail = 0, offset = 0;
				i < data->lzx_npages && outputsz > 0;
				i += 1, offset += PAGE_CACHE_SIZE) {
			void* page_data = kmap(data->lzx_pages[i]);
			avail = min_t(__u32, outputsz, PAGE_CACHE_SIZE);
			
			pr_spam("decompressor_lzx_end: i=%d, offset=%u, avail=%u, outputsz=%zu\n",
				i, offset, avail, outputsz);
			
			outputsz -= avail;
			
			memcpy(page_data, output + offset, avail);
			kunmap(data->lzx_pages[i]);
		}
	} else {
		/* Called by %__microfs_copy_filedata_exceptionally. The data
		 * is stored in the correct buffer. Everything is fine.
		 */
		sbi->si_filedatabuf.d_used = outputsz;
	}
	
	pr_spam("decompressor_lzx_end: done\n");
	return 0;
	
err_decompress:
	pr_err("decompressor_lzx_end: failed to decompress data\n");
	return *err;
}

#endif

