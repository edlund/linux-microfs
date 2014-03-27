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

#ifdef MICROFS_DECOMPRESSOR_ZLIB

#include <linux/zlib.h>

static int decompressor_zlib_init(struct microfs_sb_info* sbi)
{
	struct z_stream_s* zstrm = kmalloc(sizeof(*zstrm), GFP_KERNEL);
	if (!zstrm)
		goto err_mem_zstrm;
	
	zstrm->workspace = kmalloc(zlib_inflate_workspacesize(), GFP_KERNEL);
	if (!zstrm->workspace)
		goto err_mem_workspace;
	
	zstrm->next_in = NULL;
	zstrm->avail_in = 0;
	zstrm->next_out = NULL;
	zstrm->avail_out = 0;
	zlib_inflateInit(zstrm);
	
	sbi->si_decompressor_data = zstrm;
	
	return 0;
	
err_mem_workspace:
	kfree(zstrm);
err_mem_zstrm:
	return -ENOMEM;
}

static int decompressor_zlib_end(struct microfs_sb_info* sbi)
{
	struct z_stream_s* zstrm = sbi->si_decompressor_data;
	sbi->si_decompressor_data = NULL;
	zlib_inflateEnd(zstrm);
	kfree(zstrm->workspace);
	kfree(zstrm);
	
	return 0;
}

static int decompressor_zlib_reset(struct microfs_sb_info* sbi)
{
	struct z_stream_s* zstrm = sbi->si_decompressor_data;
	
	if (unlikely(zlib_inflateReset(zstrm) != Z_OK)) {
		pr_err("failed to reset the inflate stream: %s\n", zstrm->msg);
		pr_err("reinitializing the stream\n");
		zlib_inflateEnd(zstrm);
		zlib_inflateInit(zstrm);
	}
	
	return 0;
}

static int decompressor_zlib_2step_prepare(struct microfs_sb_info* sbi)
{
	struct z_stream_s* zstrm = sbi->si_decompressor_data;
	
	sbi->si_filedatabuf.d_offset = MICROFS_MAXIMGSIZE - 1;
	sbi->si_filedatabuf.d_used = 0;
	zstrm->avail_in = 0;
	zstrm->next_in = NULL;
	zstrm->avail_out = sbi->si_filedatabuf.d_size;
	zstrm->next_out = sbi->si_filedatabuf.d_data;
	
	return 0;
}

static int decompressor_zlib_direct_prepare(struct microfs_sb_info* sbi)
{
	struct z_stream_s* zstrm = sbi->si_decompressor_data;
	
	zstrm->avail_in = 0;
	zstrm->next_in = NULL;
	zstrm->avail_out = 0;
	zstrm->next_out = NULL;
	
	return 0;
}

static int decompressor_zlib_direct_nextpage(struct microfs_sb_info* sbi)
{
	struct z_stream_s* zstrm = sbi->si_decompressor_data;
	return zstrm->avail_out == 0;
}

static int decompressor_zlib_direct_pagedata(struct microfs_sb_info* sbi, void* page_data)
{
	struct z_stream_s* zstrm = sbi->si_decompressor_data;
	if (page_data) {
		zstrm->avail_out = PAGE_CACHE_SIZE;
		zstrm->next_out = page_data;
	} else {
		zstrm->next_out = NULL;
	}
	return 0;
}

static int decompressor_zlib_erroneous(struct microfs_sb_info* sbi,
	int* err, int* implerr)
{
	if (*err) {
		return 0;
	} else if (!*err && *implerr != Z_STREAM_END) {
		pr_err("decompressor_zlib_erroneous: zlib not at streams end"
			" but no error is reported by decompressor_zlib_decompress\n");
		*err = -EIO;
		return 0;
	}
	return 1;
}

static int decompressor_zlib_decompress(struct microfs_sb_info* sbi,
	struct buffer_head** bhs, __u32 nbhs, __u32* length,
	__u32* bh, __u32* bh_offset, __u32* inflated, int* implerr)
{
	int err = 0;
	
	struct z_stream_s* zstrm = sbi->si_decompressor_data;
	
	pr_spam("__microfs_inflate_bhs: sbi=0x%p, bhs=0x%p, nbhs=%u\n", sbi, bhs, nbhs);
	pr_spam("__microfs_inflate_bhs: ztream=0x%p\n", zstrm);
	pr_spam("__microfs_inflate_bhs: *length=%u, *bh=%u, *bh_offset=%u, *inflated=%u\n",
		*length, *bh, *bh_offset, *inflated);
	
	do {
		if (zstrm->avail_in == 0) {
			pr_spam("__microfs_inflate_bhs: *bh=%u, bhs[*bh]=0x%p\n", *bh, bhs[*bh]);
			zstrm->avail_in = min_t(__u32, *length, PAGE_CACHE_SIZE - *bh_offset);
			zstrm->next_in = bhs[*bh]->b_data + *bh_offset;
			*bh += 1;
			*length -= zstrm->avail_in;
			*bh_offset = 0;
		}
		
		pr_spam("__microfs_inflate_bhs: *length=%u\n", *length);
		
		pr_spam("__microfs_inflate_bhs: pre; zstrm->avail_out=%u, zstrm->next_out=0x%p\n",
			zstrm->avail_out, zstrm->next_out);
		pr_spam("__microfs_inflate_bhs: pre; zstrm->avail_in=%u, zstrm->next_in=0x%p\n",
			zstrm->avail_in, zstrm->next_in);
		
		*implerr = zlib_inflate(zstrm, Z_SYNC_FLUSH);
		
		pr_spam("__microfs_inflate_bhs: post; zstrm->avail_out=%u, zstrm->next_out=0x%p\n",
			zstrm->avail_out, zstrm->next_out);
		pr_spam("__microfs_inflate_bhs: post; zstrm->avail_in=%u, zstrm->next_in=0x%p\n",
			zstrm->avail_in, zstrm->next_in);
		
		pr_spam("__microfs_inflate_bhs: *zerr=%d\n", *implerr);
		
		if (zstrm->avail_out == 0 && zstrm->next_out != NULL) {
			/* zstrm->avail_out can be zero when zstrm->next_out is NULL.
			 * If it is not, the output buffer must be refilled (it is
			 * also possible that everything is inflated, but let the
			 * caller worry about that).
			 */
			break;
		}
		
	} while (*implerr == Z_OK);
	
	if (*implerr == Z_STREAM_END) {
		*inflated += zstrm->total_out;
		pr_spam("__microfs_inflate_bhs: at streams end, %u bytes inflated, %u bytes total",
			(__u32)zstrm->total_out, *inflated);
		sbi->si_decompressor->dc_reset(sbi);
	} else if (*implerr != Z_OK) {
		pr_err("__microfs_inflate_bhs: failed to inflate data: %s\n", zstrm->msg);
		err = -EIO;
	}
	
	return err;
}	

static int decompressor_zlib_continue(struct microfs_sb_info* sbi,
	int err, int implerr, __u32 length, int more_avail_out)
{
	struct z_stream_s* zstrm = sbi->si_decompressor_data;
	return !err && (
		implerr == Z_OK || (
			implerr == Z_STREAM_END && (
				zstrm->avail_in > 0 || length > 0
			) && (
				zstrm->avail_out > 0 || more_avail_out > 0
			)
		)
	);
}

const struct microfs_decompressor decompressor_zlib = {
	.dc_id = MICROFS_FLAG_DECOMPRESSOR_ZLIB,
	.dc_compiled = 1,
	.dc_name = "zlib",
	.dc_init = decompressor_zlib_init,
	.dc_end = decompressor_zlib_end,
	.dc_reset = decompressor_zlib_reset,
	.dc_2step_prepare = decompressor_zlib_2step_prepare,
	.dc_direct_prepare = decompressor_zlib_direct_prepare,
	.dc_direct_nextpage = decompressor_zlib_direct_nextpage,
	.dc_direct_pagedata = decompressor_zlib_direct_pagedata,
	.dc_erroneous = decompressor_zlib_erroneous,
	.dc_decompress = decompressor_zlib_decompress,
	.dc_continue = decompressor_zlib_continue
};

#else

const struct microfs_decompressor decompressor_zlib = {
	.dc_id = MICROFS_FLAG_DECOMPRESSOR_ZLIB,
	.dc_compiled = 0,
	.dc_name = "zlib"
};

#endif
