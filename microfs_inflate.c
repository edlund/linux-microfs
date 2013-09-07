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

int microfs_inflate_init(struct microfs_reader* rd)
{
	rd->rd_zstream.workspace = vmalloc(zlib_inflate_workspacesize());
	if (!rd->rd_zstream.workspace)
		return -ENOMEM;
	rd->rd_zstream.next_in = NULL;
	rd->rd_zstream.avail_in = 0;
	rd->rd_zstream.next_out = NULL;
	rd->rd_zstream.avail_out = 0;
	zlib_inflateInit(&rd->rd_zstream);
	return 0;
}

void microfs_inflate_end(struct microfs_reader* rd)
{
	zlib_inflateEnd(&rd->rd_zstream);
	vfree(rd->rd_zstream.workspace);
}

int __microfs_inflate_block(struct microfs_reader* rd,
	void* dest, __u32 destlen, void* src, __u32 srclen)
{
	int err;
	
	rd->rd_zstream.next_in = src;
	rd->rd_zstream.avail_in = srclen;
	
	rd->rd_zstream.next_out = dest;
	rd->rd_zstream.avail_out = destlen;
	
	pr_spam("__microfs_inflate_block: src=%p, srclen=%u,"
		" dest=%p, destlen=%u\n", src, srclen, dest, destlen);
	
	err = zlib_inflateReset(&rd->rd_zstream);
	if (unlikely(err != Z_OK)) {
		pr_err("failed to reset the inflate stream: %d\n", err);
		pr_err("reinitializing the stream\n");
		zlib_inflateEnd(&rd->rd_zstream);
		zlib_inflateInit(&rd->rd_zstream);
	}
	
	err = zlib_inflate(&rd->rd_zstream, Z_FINISH);
	if (unlikely(err != Z_STREAM_END))
		goto err_inflate;
	
	pr_spam("__microfs_inflate_block: inflated %lu bytes\n",
		rd->rd_zstream.total_out);
	
	return rd->rd_zstream.total_out;
	
err_inflate:
	pr_err("inflate error: %d\n", err);
	return -EIO;
}
