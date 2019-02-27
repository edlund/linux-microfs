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

#include "libinfo_zlib.h"

#ifdef MICROFS_DECOMPRESSOR_ZLIB

#include <linux/zlib.h>

struct decompressor_zlib_data {
	void* z_pageaddr;
	struct z_stream_s z_strm;
};

static int decompressor_zlib_create(struct microfs_sb_info* sbi, void** dest)
{
	struct decompressor_zlib_data* zdat = kmalloc(sizeof(*zdat), GFP_KERNEL);
	if (!zdat)
		goto err_mem_zdat;
	
	zdat->z_strm.workspace = kmalloc(zlib_inflate_workspacesize(), GFP_KERNEL);
	if (!zdat->z_strm.workspace)
		goto err_mem_workspace;
	
	zdat->z_strm.next_in = NULL;
	zdat->z_strm.avail_in = 0;
	zdat->z_strm.next_out = NULL;
	zdat->z_strm.avail_out = 0;
	zlib_inflateInit(&zdat->z_strm);
	
	*dest = zdat;
	
	return 0;
	
err_mem_workspace:
	kfree(zdat);
err_mem_zdat:
	return -ENOMEM;
}

static int decompressor_zlib_destroy(struct microfs_sb_info* sbi, void* data)
{
	struct decompressor_zlib_data* zdat = data;
	
	if (zdat) {
		zlib_inflateEnd(&zdat->z_strm);
		kfree(zdat->z_strm.workspace);
		kfree(zdat);
	}
	
	return 0;
}

static int decompressor_zlib_reset(struct microfs_sb_info* sbi, void* data)
{
	struct decompressor_zlib_data* zdat = data;
	
	if (unlikely(zlib_inflateReset(&zdat->z_strm) != Z_OK)) {
		pr_err("failed to reset the inflate stream: %s\n", zdat->z_strm.msg);
		pr_err("reinitializing the stream\n");
		zlib_inflateEnd(&zdat->z_strm);
		zlib_inflateInit(&zdat->z_strm);
	}
	
	return 0;
}

static int decompressor_zlib_exceptionally_begin(struct microfs_sb_info* sbi,
	void* data)
{
	struct decompressor_zlib_data* zdat = data;
	
	pr_spam("decompressor_zlib_exceptionally_begin: zdat=0x%p\n", zdat);

	sbi->si_filedatabuf.d_offset = MICROFS_MAXIMGSIZE - 1;
	sbi->si_filedatabuf.d_used = 0;
	zdat->z_strm.avail_in = 0;
	zdat->z_strm.next_in = NULL;
	zdat->z_strm.avail_out = sbi->si_filedatabuf.d_size;
	zdat->z_strm.next_out = sbi->si_filedatabuf.d_data;
	
	return 0;
}

static int decompressor_zlib_nominally_begin(struct microfs_sb_info* sbi,
	void* data, struct page** pages, __u32 npages)
{
	struct decompressor_zlib_data* zdat = data;
	
	pr_spam("decompressor_zlib_nominally_begin: zdat=0x%p\n", zdat);

	(void)sbi;
	(void)pages;
	(void)npages;
	
	zdat->z_strm.avail_in = 0;
	zdat->z_strm.next_in = NULL;
	zdat->z_strm.avail_out = 0;
	zdat->z_strm.next_out = NULL;
	
	return 0;
}

static int decompressor_zlib_copy_nominally_needpage(struct microfs_sb_info* sbi,
	void* data)
{
	struct decompressor_zlib_data* zdat = data;
	(void)sbi;
	return zdat->z_strm.avail_out == 0;
}

static int decompressor_zlib_copy_nominally_utilizepage(struct microfs_sb_info* sbi,
	void* data, struct page* page)
{
	struct decompressor_zlib_data* zdat = data;
	(void)sbi;
	if (page) {
		zdat->z_pageaddr = kmap_atomic(page);
		zdat->z_strm.next_out = zdat->z_pageaddr;
		zdat->z_strm.avail_out = PAGE_SIZE;
	} else {
		zdat->z_strm.next_out = NULL;
	}
	return zdat->z_strm.next_out != NULL;
}

static int decompressor_zlib_copy_nominally_releasepage(struct microfs_sb_info* sbi,
	void* data, struct page* page)
{
	struct decompressor_zlib_data* zdat = data;
	(void)sbi;
	(void)page;
	kunmap_atomic(zdat->z_pageaddr);
	return 0;
}

static int decompressor_zlib_consumebhs(struct microfs_sb_info* sbi,
	void* data, struct buffer_head** bhs, __u32 nbhs, __u32* length,
	__u32* bh, __u32* bh_offset, __u32* inflated, int* implerr)
{
	int err = 0;
	
	struct decompressor_zlib_data* zdat = data;
	
	pr_spam("decompressor_zlib_consumebhs: sbi=0x%p, bhs=0x%p, nbhs=%u\n", sbi, bhs, nbhs);
	pr_spam("decompressor_zlib_consumebhs: zdat=0x%p\n", zdat);
	pr_spam("decompressor_zlib_consumebhs: *length=%u, *bh=%u, *bh_offset=%u, *inflated=%u\n",
		*length, *bh, *bh_offset, *inflated);
	
	do {
		if (zdat->z_strm.avail_in == 0) {
			pr_spam("decompressor_zlib_consumebhs: *bh=%u, bhs[*bh]=0x%p\n", *bh, bhs[*bh]);
			zdat->z_strm.avail_in = min_t(__u32, *length, PAGE_SIZE - *bh_offset);
			zdat->z_strm.next_in = bhs[*bh]->b_data + *bh_offset;
			*bh += 1;
			*length -= zdat->z_strm.avail_in;
			*bh_offset = 0;
		}
		
		pr_spam("decompressor_zlib_consumebhs: *length=%u\n", *length);
		
		pr_spam("decompressor_zlib_consumebhs: pre; zstrm->avail_out=%u, zstrm->next_out=0x%p\n",
			zdat->z_strm.avail_out, zdat->z_strm.next_out);
		pr_spam("decompressor_zlib_consumebhs: pre; zstrm->avail_in=%u, zstrm->next_in=0x%p\n",
			zdat->z_strm.avail_in, zdat->z_strm.next_in);
		
		*implerr = zlib_inflate(&zdat->z_strm, Z_SYNC_FLUSH);
		
		pr_spam("decompressor_zlib_consumebhs: post; zstrm->avail_out=%u, zstrm->next_out=0x%p\n",
			zdat->z_strm.avail_out, zdat->z_strm.next_out);
		pr_spam("decompressor_zlib_consumebhs: post; zstrm->avail_in=%u, zstrm->next_in=0x%p\n",
			zdat->z_strm.avail_in, zdat->z_strm.next_in);
		
		pr_spam("decompressor_zlib_consumebhs: *implerr=%d\n", *implerr);
		
		if (zdat->z_strm.avail_out == 0 && zdat->z_strm.next_out != NULL) {
			/* z_strm.avail_out can be zero when z_strm.next_out is NULL.
			 * If it is not, the output buffer must be refilled (it is
			 * also possible that everything is inflated, but let the
			 * caller worry about that).
			 */
			break;
		}
		
	} while (*implerr == Z_OK);
	
	if (*implerr == Z_STREAM_END) {
		*inflated += zdat->z_strm.total_out;
		pr_spam("decompressor_zlib_consumebhs: at streams end, %u bytes inflated, %u bytes total",
			(__u32)zdat->z_strm.total_out, *inflated);
		sbi->si_decompressor->dc_reset(sbi, data);
	} else if (*implerr != Z_OK) {
		pr_err("decompressor_zlib_consumebhs: failed to inflate data: (%d) %s\n",
			*implerr, zdat->z_strm.msg);
		err = -EIO;
	}
	
	return err;
}	

static int decompressor_zlib_continue(struct microfs_sb_info* sbi,
	void* data, int err, int implerr, __u32 length, int more_avail_out)
{
	struct decompressor_zlib_data* zdat = data;
	return !err && (implerr == Z_OK || (
		implerr == Z_STREAM_END && (
			zdat->z_strm.avail_in > 0 || length > 0
		) && (
			zdat->z_strm.avail_out > 0 || more_avail_out > 0
		)
	));
}

static int decompressor_zlib_end(struct microfs_sb_info* sbi,
	void* data, int* err, int* implerr, __u32* decompressed)
{
	(void)sbi;
	(void)data;
	(void)decompressed;
	
	if (*err) {
		return -1;
	} else if (!*err && *implerr != Z_STREAM_END) {
		pr_err("decompressor_zlib_end: zlib not at streams end"
			" but no error is reported by decompressor_zlib_consumebhs\n");
		*err = -EIO;
		return -1;
	}
	return 0;
}

const struct microfs_decompressor decompressor_zlib = {
	.dc_info = &libinfo_zlib,
	.dc_compiled = 1,
	.dc_streamed = 1,
	.dc_data_init = microfs_decompressor_data_init_noop,
	.dc_data_exit = microfs_decompressor_data_exit_noop,
	.dc_create = decompressor_zlib_create,
	.dc_destroy = decompressor_zlib_destroy,
	.dc_reset = decompressor_zlib_reset,
	.dc_exceptionally_begin = decompressor_zlib_exceptionally_begin,
	.dc_nominally_begin = decompressor_zlib_nominally_begin,
	.dc_copy_nominally_needpage = decompressor_zlib_copy_nominally_needpage,
	.dc_copy_nominally_utilizepage = decompressor_zlib_copy_nominally_utilizepage,
	.dc_copy_nominally_releasepage = decompressor_zlib_copy_nominally_releasepage,
	.dc_consumebhs = decompressor_zlib_consumebhs,
	.dc_continue = decompressor_zlib_continue,
	.dc_end = decompressor_zlib_end
};

#else

const struct microfs_decompressor decompressor_zlib = {
	.dc_info = &libinfo_zlib,
	.dc_compiled = 0
};

#endif
