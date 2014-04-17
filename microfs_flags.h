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

#ifndef __MICROFS_FLAGS_H__
#define __MICROFS_FLAGS_H__

/* Feature flag ranges:
 * 
 * 0x00000000 - 0x000000ff: Features which will work with
 *                          all past kernels.
 * 0x00000100 - 0xffffffff: Features which will NOT work
 *                          with all past kernels.
 * 
 * 0x00000100 - 0x0000ff00: Decompressor types.
 */

#define MICROFS_FLAG_DECOMPRESSOR_NULL 0x00000000
#define MICROFS_FLAG_DECOMPRESSOR_ZLIB 0x00000100
#define MICROFS_FLAG_DECOMPRESSOR_LZ4  0x00000200
#define MICROFS_FLAG_DECOMPRESSOR_LZO  0x00000400
#define MICROFS_FLAG_DECOMPRESSOR_XZ   0x00000800

#define MICROFS_FLAG_MASK_OLDKERNELS   0x000000ff
#define MICROFS_FLAG_MASK_DECOMPRESSOR 0x0000ff00

#define MICROFS_SUPPORTED_FLAGS (MICROFS_FLAG_MASK_OLDKERNELS \
		| MICROFS_FLAG_DECOMPRESSOR_ZLIB \
		| MICROFS_FLAG_DECOMPRESSOR_LZ4  \
		| MICROFS_FLAG_DECOMPRESSOR_LZO  \
		| MICROFS_FLAG_DECOMPRESSOR_XZ   \
	)

#endif

