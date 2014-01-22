/* microfs - Minimally Improved Compressed Read Only File System
 * Copyright (C) 2012 Erik Edlund <erik.edlund@32767.se>
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

int microfs_inflate_init(struct microfs_sb_info* sbi)
{
	sbi->si_rdzstream.workspace = vmalloc(zlib_inflate_workspacesize());
	if (!sbi->si_rdzstream.workspace)
		return -ENOMEM;
	sbi->si_rdzstream.next_in = NULL;
	sbi->si_rdzstream.avail_in = 0;
	sbi->si_rdzstream.next_out = NULL;
	sbi->si_rdzstream.avail_out = 0;
	zlib_inflateInit(&sbi->si_rdzstream);
	return 0;
}

void microfs_inflate_end(struct microfs_sb_info* sbi)
{
	zlib_inflateEnd(&sbi->si_rdzstream);
	vfree(sbi->si_rdzstream.workspace);
}

void __microfs_inflate_reset(struct microfs_sb_info* sbi)
{
	if (unlikely(zlib_inflateReset(&sbi->si_rdzstream) != Z_OK)) {
		pr_err("failed to reset the inflate stream: %s\n", sbi->si_rdzstream.msg);
		pr_err("reinitializing the stream\n");
		zlib_inflateEnd(&sbi->si_rdzstream);
		zlib_inflateInit(&sbi->si_rdzstream);
	}
}

int __microfs_inflate_bhs(struct microfs_sb_info* sbi,
	struct buffer_head** bhs, __u32 nbhs, __u32* length,
	__u32* bh, __u32* bh_offset, __u32* inflated, int* zerr)
{
	int err = 0;
	
	struct z_stream_s* zstrm = &sbi->si_rdzstream;
	
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
		
		*zerr = zlib_inflate(zstrm, Z_SYNC_FLUSH);
		
		pr_spam("__microfs_inflate_bhs: post; zstrm->avail_out=%u, zstrm->next_out=0x%p\n",
			zstrm->avail_out, zstrm->next_out);
		pr_spam("__microfs_inflate_bhs: post; zstrm->avail_in=%u, zstrm->next_in=0x%p\n",
			zstrm->avail_in, zstrm->next_in);
		
		pr_spam("__microfs_inflate_bhs: *zerr=%d\n", *zerr);
		
		if (zstrm->avail_out == 0 && zstrm->next_out != NULL) {
			/* zstrm->avail_out can be zero when zstrm->next_out is NULL.
			 * If it is not, the output buffer must be refilled (it is
			 * also possible that everything is inflated, but let the
			 * caller worry about that).
			 */
			break;
		}
		
	} while (*zerr == Z_OK);
	
	if (*zerr == Z_STREAM_END) {
		*inflated += zstrm->total_out;
		pr_spam("__microfs_inflate_bhs: at streams end, %u bytes inflated, %u bytes total"
			" - resetting stream\n", (__u32)zstrm->total_out, *inflated);
		__microfs_inflate_reset(sbi);
	} else if (*zerr != Z_OK) {
		pr_err("__microfs_inflate_bhs: failed to inflate data: %s\n", zstrm->msg);
		err = -EIO;
	}
	
	return err;
}	

int __microfs_inflate_more(int err, int zerr, struct z_stream_s* zstrm,
	__u32 length, int more_avail_out)
{
	return !err && (
		zerr == Z_OK || (
			zerr == Z_STREAM_END && (
				zstrm->avail_in > 0 || length > 0
			) && (
				zstrm->avail_out > 0 || more_avail_out > 0
			)
		)
	);
}
