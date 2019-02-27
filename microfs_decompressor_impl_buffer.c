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

struct decompressor_impl_buffer_data {
	char* ib_inputbuf;
	__u32 ib_inputbufsz;
	__u32 ib_inputbufusedsz;
	char* ib_outputbuf;
	__u32 ib_outputbufsz;
	__u32 ib_outputbufusedsz;
	struct page** ib_pages;
	__u32 ib_npages;
};

int decompressor_impl_buffer_create(struct microfs_sb_info* sbi,
	void** dest, __u32 upperbound)
{
	__u32 outputbufsz = max_t(__u32, sbi->si_blksz, PAGE_SIZE);
	__u32 inputbufsz = max_t(__u32, upperbound, PAGE_SIZE * 2);
	
	struct decompressor_impl_buffer_data* dat = kmalloc(sizeof(*dat), GFP_KERNEL);
	if (!dat)
		goto err_mem_data;
	
	dat->ib_inputbuf = NULL;
	dat->ib_outputbuf = NULL;
	
	dat->ib_pages = NULL;
	dat->ib_npages = 0;
	
#define DATA_BUF(Data, Name, Size) \
	do { \
		Data->ib_##Name##bufsz = Size; \
		Data->ib_##Name##bufusedsz = 0; \
		Data->ib_##Name##buf = kmalloc(Size, GFP_KERNEL); \
		if (!Data->ib_##Name##buf) \
			goto err_mem_data_##Name; \
	} while (0)
	
	pr_spam("decompressor_impl_buffer_create: inputbufsz=%u, outputbufsz=%u\n",
		inputbufsz, outputbufsz);
	
	DATA_BUF(dat, input, inputbufsz);
	DATA_BUF(dat, output, outputbufsz);
	
#undef DATA_BUF
	
	*dest = dat;
	
	return 0;
	
err_mem_data_output:	
	kfree(dat->ib_inputbuf);
err_mem_data_input:
	kfree(dat);
err_mem_data:
	return -ENOMEM;

}

int decompressor_impl_buffer_destroy(struct microfs_sb_info* sbi, void* data)
{
	struct decompressor_impl_buffer_data* ibdat = data;
	
	if (ibdat) {
		kfree(ibdat->ib_outputbuf);
		kfree(ibdat->ib_inputbuf);
		kfree(ibdat);
	}
	
	return 0;
}

int decompressor_impl_buffer_reset(struct microfs_sb_info* sbi, void* data)
{
	(void)sbi;
	(void)data;
	return 0;
}

int decompressor_impl_buffer_exceptionally_begin(struct microfs_sb_info* sbi, void* data)
{
	struct decompressor_impl_buffer_data* ibdat = data;
	pr_spam("decompressor_impl_buffer_exceptionally_begin: ibdat=0x%p\n", ibdat);
	ibdat->ib_pages = NULL;
	ibdat->ib_npages = 0;
	return 0;
}

int decompressor_impl_buffer_nominally_begin(struct microfs_sb_info* sbi, void* data,
	struct page** pages, __u32 npages)
{
	struct decompressor_impl_buffer_data* ibdat = data;
	pr_spam("decompressor_impl_buffer_nominally_begin: ibdat=0x%p\n", ibdat);
	ibdat->ib_pages = pages;
	ibdat->ib_npages = npages;
	return 0;
}

int decompressor_impl_buffer_copy_nominally_needpage(struct microfs_sb_info* sbi,
	void* data)
{
	(void)sbi;
	(void)data;
	return 0;
}

int decompressor_impl_buffer_copy_nominally_utilizepage(struct microfs_sb_info* sbi,
	void* data, struct page* page)
{
	(void)sbi;
	(void)data;
	(void)page;
	return 0;
}

int decompressor_impl_buffer_copy_nominally_releasepage(struct microfs_sb_info* sbi,
	void* data, struct page* page)
{
	(void)sbi;
	(void)data;
	(void)page;
	return 0;
}

int decompressor_impl_buffer_consumebhs(struct microfs_sb_info* sbi, void* data,
	struct buffer_head** bhs, __u32 nbhs, __u32* length,
	__u32* bh, __u32* bh_offset, __u32* inflated, int* implerr)
{
	__u32 bh_avail;
	__u32 buf_offset = 0;
	
	struct decompressor_impl_buffer_data* ibdat = data;
	
	ibdat->ib_inputbufusedsz = *length;
	
	pr_spam("decompressor_impl_buffer_consumebhs: data->ib_inputbufusedsz=%u\n",
		ibdat->ib_inputbufusedsz);
	
	while (*bh < nbhs && *length > 0) {
		pr_spam("decompressor_impl_buffer_consumebhs: *bh=%u, bhs[*bh]=0x%p, nbhs=%u\n",
			*bh, bhs[*bh], nbhs);
		
		bh_avail = min_t(__u32, *length, PAGE_SIZE - *bh_offset);
		pr_spam("decompressor_impl_buffer_consumebhs: *bh_offset=%u, *bh_avail=%u\n",
			*bh_offset, bh_avail);
		
		memcpy(ibdat->ib_inputbuf + buf_offset, bhs[*bh]->b_data
			+ *bh_offset, bh_avail);
		
		pr_spam("decompressor_impl_buffer_consumebhs: *length=%u, *bh_offset=%u,"
				" buf_offset=%u\n",
			*length, *bh_offset, buf_offset);
		
		*bh_offset = 0;
		*bh += 1;
		*length -= bh_avail;
		buf_offset += bh_avail;
	}
	
	return 0;
}

int decompressor_impl_buffer_continue(struct microfs_sb_info* sbi, void* data,
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

int decompressor_impl_buffer_end(struct microfs_sb_info* sbi, void* data,
	int* err, int* implerr, __u32* decompressed,
	decompressor_impl_buffer_end_consumer consumer)
{
	int i;
	
	__u32 avail;
	__u32 offset;
	
	struct decompressor_impl_buffer_data* ibdat = data;
	
	__u32 outputsz = ibdat->ib_pages?
		ibdat->ib_outputbufsz: sbi->si_filedatabuf.d_size;
	char* output = ibdat->ib_pages?
		ibdat->ib_outputbuf: sbi->si_filedatabuf.d_data;
	
	if (*err) {
		goto err_decompress;
	}
	
	pr_spam("decompressor_impl_buffer_end: data->ib_pages=0x%p, data->ib_npages=%u\n",
			ibdat->ib_pages, ibdat->ib_npages);
	pr_spam("decompressor_impl_buffer_end: output=0x%p,"
			" data->ib_outputbuf=0x%p, sbi->si_filedatabuf.d_data=0x%p\n",
		output, ibdat->ib_outputbuf, sbi->si_filedatabuf.d_data);
	
	*err = consumer(sbi, data, implerr,
		ibdat->ib_inputbuf, ibdat->ib_inputbufusedsz,
		output, &outputsz);
	if (*err < 0) {
		goto err_decompress;
	}
	
	*decompressed = outputsz;
	
	pr_spam("decompressor_impl_buffer_end: outputsz=%u\n", outputsz);
	
	if (ibdat->ib_pages) {
		/* Called by %__microfs_copy_filedata_nominally. Copy the data
		 * to the page cache pages.
		 */
		for (i = 0, avail = 0, offset = 0;
				i < ibdat->ib_npages && outputsz > 0;
				i += 1, offset += PAGE_SIZE) {
			void* page_data = kmap_atomic(ibdat->ib_pages[i]);
			avail = min_t(__u32, outputsz, PAGE_SIZE);
			
			pr_spam("decompressor_impl_buffer_end: i=%d, offset=%u, avail=%u, outputsz=%u\n",
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
	
	pr_spam("decompressor_impl_buffer_end: done\n");
	return 0;
	
err_decompress:
	pr_err("decompressor_impl_buffer_end: failed to decompress data\n");
	return *err;
}

