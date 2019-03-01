/* microfs - Minimally Improved Compressed Read Only File System
 * Copyright (C) 2012, 2013, 2014, 2015, 2016, 2017, ..., +%Y
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

#include "libinfo_zstd.h"

#ifdef MICROFS_DECOMPRESSOR_ZSTD

#include <linux/zstd.h>

struct decompressor_zstd_data {
	void* z_pageaddr;
	void* z_workspace;
	size_t z_workspace_size;
	size_t z_window_size;
	ZSTD_inBuffer z_in_buf;
	ZSTD_outBuffer z_out_buf;
	ZSTD_DStream* z_stream;
	__u32 z_totalout;
};

static int decompressor_zstd_create(struct microfs_sb_info* sbi, void** dest)
{
	struct decompressor_zstd_data* zdat = kzalloc(sizeof(*zdat), GFP_KERNEL);
	if (!zdat)
		goto err_mem_zdat;
	
	zdat->z_window_size = max_t(size_t, sbi->si_blksz, MICROFS_ZSTD_MINWINSZ);
	zdat->z_workspace_size = ZSTD_DStreamWorkspaceBound(zdat->z_window_size);
	zdat->z_workspace = vmalloc(zdat->z_workspace_size);
	if (zdat->z_workspace == NULL)
		goto err_mem_workspace;
	zdat->z_stream = ZSTD_initDStream(zdat->z_window_size,
		zdat->z_workspace, zdat->z_workspace_size);
	
	*dest = zdat;
	
	return 0;

err_mem_workspace:
	kfree(zdat);
err_mem_zdat:
	return -ENOMEM;
}

static int decompressor_zstd_destroy(struct microfs_sb_info* sbi, void* data)
{
	struct decompressor_zstd_data* zdat = data;
	
	if (zdat) {
		vfree(zdat->z_workspace);
		kfree(zdat);
	}
	
	return 0;
}

static int decompressor_zstd_reset(struct microfs_sb_info* sbi, void* data)
{
	struct decompressor_zstd_data* zdat = data;
	
	const size_t result = ZSTD_resetDStream(zdat->z_stream);
	const size_t code = ZSTD_getErrorCode(result);
	if (unlikely(code != 0)) {
		pr_err("failed to reset the inflate stream: %zu\n", code);
		pr_err("reinitializing the stream\n");
		zdat->z_stream = ZSTD_initDStream(zdat->z_window_size,
			zdat->z_workspace, zdat->z_workspace_size);
	}
	
	return 0;
}

static int decompressor_zstd_exceptionally_begin(struct microfs_sb_info* sbi,
	void* data)
{
	struct decompressor_zstd_data* zdat = data;

	pr_spam("decompressor_zstd_exceptionally_begin: zdat=0x%p\n", zdat);
	
	sbi->si_filedatabuf.d_offset = MICROFS_MAXIMGSIZE - 1;
	sbi->si_filedatabuf.d_used = 0;
	zdat->z_in_buf.src = NULL;
	zdat->z_in_buf.size = 0;
	zdat->z_in_buf.pos = 0;
	zdat->z_out_buf.dst = sbi->si_filedatabuf.d_data;
	zdat->z_out_buf.size = sbi->si_filedatabuf.d_size;
	zdat->z_out_buf.pos = 0;
	
	return 0;
}

static int decompressor_zstd_nominally_begin(struct microfs_sb_info* sbi,
	void* data, struct page** pages, __u32 npages)
{
	struct decompressor_zstd_data* zdat = data;
	
	pr_spam("decompressor_zstd_nominally_begin: zdat=0x%p\n", zdat);

	(void)sbi;
	(void)pages;
	(void)npages;
	
	zdat->z_in_buf.src = NULL;
	zdat->z_in_buf.size = 0;
	zdat->z_in_buf.pos = 0;
	zdat->z_out_buf.dst = NULL;
	zdat->z_out_buf.size = 0;
	zdat->z_out_buf.pos = 0;
	
	return 0;
}

static int decompressor_zstd_copy_nominally_needpage(struct microfs_sb_info* sbi,
	void* data)
{
	struct decompressor_zstd_data* zdat = data;
	(void)sbi;
	return zdat->z_out_buf.pos == zdat->z_out_buf.size;
}

static int decompressor_zstd_copy_nominally_utilizepage(struct microfs_sb_info* sbi,
	void* data, struct page* page)
{
	struct decompressor_zstd_data* zdat = data;
	(void)sbi;
	if (page) {
		zdat->z_pageaddr = kmap_atomic(page);
		zdat->z_out_buf.dst = zdat->z_pageaddr;
		zdat->z_out_buf.size = PAGE_SIZE;
		zdat->z_out_buf.pos = 0;
	} else {
		zdat->z_out_buf.dst = NULL;
	}
	return zdat->z_out_buf.dst != NULL;
}

static int decompressor_zstd_copy_nominally_releasepage(struct microfs_sb_info* sbi,
	void* data, struct page* page)
{
	struct decompressor_zstd_data* zdat = data;
	(void)sbi;
	(void)page;
	kunmap_atomic(zdat->z_pageaddr);
	return 0;
}

static int decompressor_zstd_consumebhs(struct microfs_sb_info* sbi,
	void* data, struct buffer_head** bhs, __u32 nbhs, __u32* length,
	__u32* bh, __u32* bh_offset, __u32* inflated, int* implerr)
{
	int err = 0;
	size_t result;
	
	struct decompressor_zstd_data* zdat = data;
	
	__u32 prev_out_pos = zdat->z_out_buf.pos;
	
	pr_spam("decompressor_zstd_consumebhs: sbi=0x%p, bhs=0x%p, nbhs=%u\n", sbi, bhs, nbhs);
	pr_spam("decompressor_zstd_consumebhs: zdat=0x%p\n", zdat);
	pr_spam("decompressor_zstd_consumebhs: *length=%u, *bh=%u, *bh_offset=%u, *inflated=%u\n",
		*length, *bh, *bh_offset, *inflated);
	
	do {
		if (zdat->z_in_buf.size == zdat->z_in_buf.pos) {
			pr_spam("decompressor_zstd_consumebhs: *bh=%u, bhs[*bh]=0x%p\n", *bh, bhs[*bh]);
			zdat->z_in_buf.src = bhs[*bh]->b_data + *bh_offset;
			zdat->z_in_buf.size = min_t(__u32, *length, PAGE_SIZE - *bh_offset);
			zdat->z_in_buf.pos = 0;
			*bh += 1;
			*length -= zdat->z_in_buf.size;
			*bh_offset = 0;
		}
		
		pr_spam("decompressor_zstd_consumebhs: *length=%u\n", *length);
		
		pr_spam("decompressor_zstd_consumebhs: pre;"
			" zdat->z_out_buf.size=%zu, zdat->z_out_buf.dst=0x%p\n",
			zdat->z_out_buf.size, zdat->z_out_buf.dst);
		pr_spam("decompressor_zstd_consumebhs: pre;"
			" zdat->z_in_buf.size=%zu, zdat->z_in_buf.src=0x%p\n",
			zdat->z_in_buf.size, zdat->z_in_buf.src);
		pr_spam("decompressor_zstd_consumebhs: pre;"
			" prev_out_pos=%u, zdat->xz_totalout=%u\n",
			prev_out_pos, zdat->z_totalout);

		result = ZSTD_decompressStream(zdat->z_stream, &zdat->z_out_buf, &zdat->z_in_buf);
		
		zdat->z_totalout += zdat->z_out_buf.pos - prev_out_pos;
		prev_out_pos = zdat->z_out_buf.pos;
		
		pr_spam("decompressor_zstd_consumebhs: post;"
			" zdat->z_out_buf.size=%zu, zdat->z_out_buf.dst=0x%p\n",
			zdat->z_out_buf.size, zdat->z_out_buf.dst);
		pr_spam("decompressor_zstd_consumebhs: post;"
			" zdat->z_in_buf.size=%zu, zdat->z_in_buf.src=0x%p\n",
			zdat->z_in_buf.size, zdat->z_in_buf.src);
		pr_spam("decompressor_zstd_consumebhs: post;"
			" prev_out_pos=%u, zdat->xz_totalout=%u\n",
			prev_out_pos, zdat->z_totalout);
		
		pr_spam("decompressor_zstd_consumebhs: result=%zu\n", result);
		
		if (zdat->z_out_buf.size == zdat->z_out_buf.pos) {
			/* The output buffer must be refilled.
			 */
			break;
		}
		
	} while (result != 0 && !ZSTD_isError(result));
	
	if (result == 0) {
		*inflated += zdat->z_totalout;
		pr_spam("decompressor_zstd_consumebhs:"
			" at streams end, %u bytes inflated, %u bytes total",
			zdat->z_totalout, *inflated);
		zdat->z_totalout = 0;
		sbi->si_decompressor->dc_reset(sbi, data);
	} else if (ZSTD_isError(result)) {
		*implerr = ZSTD_getErrorCode(result);
		pr_err("decompressor_zstd_consumebhs:"
			" failed to inflate data, implerr %d, window_size %zu\n",
			*implerr, zdat->z_window_size);
		err = -EIO;
	}
	
	return err;
}

static int decompressor_zstd_continue(struct microfs_sb_info* sbi,
	void* data, int err, int implerr, __u32 length, int more_avail_out)
{
	struct decompressor_zstd_data* zdat = data;
	return !err && implerr == 0 && (
		zdat->z_in_buf.pos < zdat->z_in_buf.size || length > 0
	) && (
		zdat->z_out_buf.pos < zdat->z_out_buf.size || more_avail_out > 0
	);
}

static int decompressor_zstd_end(struct microfs_sb_info* sbi,
	void* data, int* err, int* implerr, __u32* decompressed)
{
	(void)sbi;
	(void)data;
	(void)decompressed;
	
	if (*err) {
		return -1;
	} else if (!*err && *implerr != 0) {
		pr_err("decompressor_zstd_end: zstd not at streams end"
			" but no error is reported by decompressor_zstd_consumebhs\n");
		*err = -EIO;
		return -1;
	}
	return 0;
}

const struct microfs_decompressor decompressor_zstd = {
	.dc_info = &libinfo_zstd,
	.dc_compiled = 1,
	.dc_streamed = 1,
	.dc_data_init = microfs_decompressor_data_init_noop,
	.dc_data_exit = microfs_decompressor_data_exit_noop,
	.dc_create = decompressor_zstd_create,
	.dc_destroy = decompressor_zstd_destroy,
	.dc_reset = decompressor_zstd_reset,
	.dc_exceptionally_begin = decompressor_zstd_exceptionally_begin,
	.dc_nominally_begin = decompressor_zstd_nominally_begin,
	.dc_copy_nominally_needpage = decompressor_zstd_copy_nominally_needpage,
	.dc_copy_nominally_utilizepage = decompressor_zstd_copy_nominally_utilizepage,
	.dc_copy_nominally_releasepage = decompressor_zstd_copy_nominally_releasepage,
	.dc_consumebhs = decompressor_zstd_consumebhs,
	.dc_continue = decompressor_zstd_continue,
	.dc_end = decompressor_zstd_end
};

#else

const struct microfs_decompressor decompressor_zstd = {
	.dc_info = &libinfo_zstd,
	.dc_compiled = 0
};

#endif
