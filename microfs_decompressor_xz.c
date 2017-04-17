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

#include "libinfo_xz.h"

#ifdef MICROFS_DECOMPRESSOR_XZ

#include <linux/xz.h>

struct decompressor_xz_data {
	void* xz_pageaddr;
	struct xz_dec* xz_state;
	struct xz_buf xz_buf;
	__u32 xz_totalout;
};

static int decompressor_xz_data_init(struct microfs_sb_info* sbi, void* dd,
	struct microfs_decompressor_data* data)
{
	int err;
	struct microfs_dd_xz* dd_xz = dd;
	
	if (__le32_to_cpu(dd_xz->dd_magic) != MICROFS_DD_XZ_MAGIC) {
		pr_err("bad xz decompressor data magic\n");
		err = -EINVAL;
		goto err;
	}
	
	data->dd_info = kmalloc(sizeof(*dd_xz), GFP_KERNEL);
	if (!data->dd_info) {
		err = -ENOMEM;
		goto err;
	}
	
	memcpy(data->dd_info, dd_xz, sizeof(*dd_xz));
	
	return 0;
	
err:
	return err;
}

static int decompressor_xz_data_exit(struct microfs_sb_info* sbi,
	struct microfs_decompressor_data* data)
{
	(void)sbi;
	
	kfree(data->dd_info);
	
	return 0;
}

static int decompressor_xz_create(struct microfs_sb_info* sbi, void** dest)
{
	int err;
	struct microfs_dd_xz* dd_xz = sbi->si_decompressor_data->dd_info;
	struct decompressor_xz_data* xzdat = kmalloc(sizeof(*xzdat), GFP_KERNEL);
	
	if (!xzdat) {
		err = -ENOMEM;
		goto err_mem_xzdat;
	}
	
	xzdat->xz_state = xz_dec_init(XZ_PREALLOC, __le32_to_cpu(dd_xz->dd_dictsz));
	if (!xzdat->xz_state) {
		err = -ENOMEM;
		goto err_mem_state;
	}
	
	*dest = xzdat;
	
	return 0;
	
err_mem_state:
	kfree(xzdat);
err_mem_xzdat:
	return err;
}

static int decompressor_xz_destroy(struct microfs_sb_info* sbi, void* data)
{
	struct decompressor_xz_data* xzdat = data;
	
	if (xzdat) {
		xz_dec_end(xzdat->xz_state);
		kfree(xzdat);
	}
	
	return 0;
}

static int decompressor_xz_reset(struct microfs_sb_info* sbi, void* data)
{
	struct decompressor_xz_data* xzdat = data;
	
	xz_dec_reset(xzdat->xz_state);
	
	return 0;
}


static int decompressor_xz_exceptionally_begin(struct microfs_sb_info* sbi,
	void* data)
{
	struct decompressor_xz_data* xzdat = data;
	
	sbi->si_filedatabuf.d_offset = MICROFS_MAXIMGSIZE - 1;
	sbi->si_filedatabuf.d_used = 0;
	xzdat->xz_buf.in = NULL;
	xzdat->xz_buf.in_size = 0;
	xzdat->xz_buf.in_pos = 0;
	xzdat->xz_buf.out = sbi->si_filedatabuf.d_data;
	xzdat->xz_buf.out_size = sbi->si_filedatabuf.d_size;
	xzdat->xz_buf.out_pos = 0;
	
	xzdat->xz_totalout = 0;
	
	return 0;
}

static int decompressor_xz_nominally_begin(struct microfs_sb_info* sbi,
	void* data, struct page** pages, __u32 npages)
{
	struct decompressor_xz_data* xzdat = data;
	
	(void)pages;
	(void)npages;
	
	xzdat->xz_buf.in = NULL;
	xzdat->xz_buf.in_size = 0;
	xzdat->xz_buf.in_pos = 0;
	xzdat->xz_buf.out = NULL;
	xzdat->xz_buf.out_size = 0;
	xzdat->xz_buf.out_pos = 0;
	
	xzdat->xz_totalout = 0;
	
	return 0;
}

static int decompressor_xz_copy_nominally_needpage(struct microfs_sb_info* sbi,
	void* data)
{
	struct decompressor_xz_data* xzdat = data;
	(void)sbi;
	return xzdat->xz_buf.out_pos == xzdat->xz_buf.out_size;
}

static int decompressor_xz_copy_nominally_utilizepage(struct microfs_sb_info* sbi,
	void* data, struct page* page)
{
	struct decompressor_xz_data* xzdat = data;
	if (page) {
		xzdat->xz_pageaddr = kmap_atomic(page);
		xzdat->xz_buf.out = xzdat->xz_pageaddr;
		xzdat->xz_buf.out_size = PAGE_SIZE;
		xzdat->xz_buf.out_pos = 0;
	} else {
		xzdat->xz_buf.out = NULL;
	}
	return xzdat->xz_buf.out != NULL;
}

static int decompressor_xz_copy_nominally_releasepage(struct microfs_sb_info* sbi,
	void* data, struct page* page)
{
	struct decompressor_xz_data* xzdat = data;
	(void)sbi;
	(void)page;
	kunmap_atomic(xzdat->xz_pageaddr);
	return 0;
}


static int decompressor_xz_consumebhs(struct microfs_sb_info* sbi,
	void* data, struct buffer_head** bhs, __u32 nbhs, __u32* length,
	__u32* bh, __u32* bh_offset, __u32* inflated, int* implerr)
{
	int err = 0;
	
	struct decompressor_xz_data* xzdat = data;
	
	__u32 prev_out_pos = xzdat->xz_buf.out_pos;
	
	pr_spam("decompressor_xz_consumebhs: sbi=0x%p, bhs=0x%p, nbhs=%u\n", sbi, bhs, nbhs);
	pr_spam("decompressor_xz_consumebhs: xzdat=0x%p\n", xzdat);
	pr_spam("decompressor_xz_consumebhs: *length=%u, *bh=%u, *bh_offset=%u, *inflated=%u\n",
		*length, *bh, *bh_offset, *inflated);
	
	do {
		if (xzdat->xz_buf.in_size == xzdat->xz_buf.in_pos) {
			pr_spam("decompressor_xz_consumebhs: *bh=%u, bhs[*bh]=0x%p\n", *bh, bhs[*bh]);
			xzdat->xz_buf.in_pos = 0;
			xzdat->xz_buf.in_size = min_t(__u32, *length, PAGE_SIZE - *bh_offset);
			xzdat->xz_buf.in = bhs[*bh]->b_data + *bh_offset;
			*bh += 1;
			*length -= xzdat->xz_buf.in_size;
			*bh_offset = 0;
		}
		
		pr_spam("decompressor_xz_consumebhs: *length=%u\n", *length);
		
		pr_spam("decompressor_xz_consumebhs: pre; xzdat->xz_buf.out_size=%zu, xzdat->xz_buf.out=0x%p\n",
			xzdat->xz_buf.out_size, xzdat->xz_buf.out);
		pr_spam("decompressor_xz_consumebhs: pre; xzdat->xz_buf.in_size=%zu, xzdat->xz_buf.in=0x%p\n",
			xzdat->xz_buf.in_size, xzdat->xz_buf.in);
		pr_spam("decompressor_xz_consumebhs: pre: prev_out_pos=%u, xzdat->xz_totalout=%u\n",
			prev_out_pos, xzdat->xz_totalout);
		
		*implerr = xz_dec_run(xzdat->xz_state, &xzdat->xz_buf);
		
		xzdat->xz_totalout += xzdat->xz_buf.out_pos - prev_out_pos;
		prev_out_pos = xzdat->xz_buf.out_pos;
		
		pr_spam("decompressor_xz_consumebhs: post; xzdat->xz_buf.out_size=%zu, xzdat->xz_buf.out=0x%p\n",
			xzdat->xz_buf.out_size, xzdat->xz_buf.out);
		pr_spam("decompressor_xz_consumebhs: post; xzdat->xz_buf.in_size=%zu, xzdat->xz_buf.in=0x%p\n",
			xzdat->xz_buf.in_size, xzdat->xz_buf.in);
		pr_spam("decompressor_xz_consumebhs: post: prev_out_pos=%u, xzdat->xz_totalout=%u\n",
			prev_out_pos, xzdat->xz_totalout);
		
		pr_spam("decompressor_xz_consumebhs: *implerr=%d\n", *implerr);
		
		if (xzdat->xz_buf.out_size == xzdat->xz_buf.out_pos) {
			/* The output buffer must be refilled.
			 */
			break;
		}
		
	} while (*implerr == XZ_OK);
	
	if (*implerr == XZ_STREAM_END) {
		*inflated += xzdat->xz_totalout;
		pr_spam("decompressor_xz_consumebhs: at streams end, %u bytes inflated, %u bytes total",
			xzdat->xz_totalout, *inflated);
		xzdat->xz_totalout = 0;
		sbi->si_decompressor->dc_reset(sbi, data);
	} else if (*implerr != XZ_OK) {
		pr_err("decompressor_xz_consumebhs: failed to inflate data\n");
		err = -EIO;
	}
	
	return err;
}

static int decompressor_xz_continue(struct microfs_sb_info* sbi,
	void* data, int err, int implerr, __u32 length, int more_avail_out)
{
	struct decompressor_xz_data* xzdat = data;
	return !err && (
		implerr == XZ_OK || (
			implerr == XZ_STREAM_END && (
				xzdat->xz_buf.in_pos < xzdat->xz_buf.in_size ||
					length > 0
			) && (
				xzdat->xz_buf.out_pos < xzdat->xz_buf.out_size ||
					more_avail_out > 0
			)
		)
	);
}

static int decompressor_xz_end(struct microfs_sb_info* sbi, void* data,
	int* err, int* implerr, __u32* decompressed)
{
	(void)sbi;
	(void)decompressed;
	
	if (*err) {
		return -1;
	} else if (!*err && *implerr != XZ_STREAM_END) {
		pr_err("decompressor_xz_end: xz not at streams end"
			" but no error is reported by decompressor_xz_consumebhs\n");
		*err = -EIO;
		return -1;
	}
	return 0;
}

const struct microfs_decompressor decompressor_xz = {
	.dc_info = &libinfo_xz,
	.dc_compiled = 1,
	.dc_data_init = decompressor_xz_data_init,
	.dc_data_exit = decompressor_xz_data_exit,
	.dc_create = decompressor_xz_create,
	.dc_destroy = decompressor_xz_destroy,
	.dc_reset = decompressor_xz_reset,
	.dc_exceptionally_begin = decompressor_xz_exceptionally_begin,
	.dc_nominally_begin = decompressor_xz_nominally_begin,
	.dc_copy_nominally_needpage = decompressor_xz_copy_nominally_needpage,
	.dc_copy_nominally_utilizepage = decompressor_xz_copy_nominally_utilizepage,
	.dc_copy_nominally_releasepage = decompressor_xz_copy_nominally_releasepage,
	.dc_consumebhs = decompressor_xz_consumebhs,
	.dc_continue = decompressor_xz_continue,
	.dc_end = decompressor_xz_end
};

#else

const struct microfs_decompressor decompressor_xz = {
	.dc_info = &libinfo_xz,
	.dc_compiled = 0
};

#endif
