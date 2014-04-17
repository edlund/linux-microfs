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

#ifdef __LIBINFO_XZ_H__
#error "multiple includes"
#endif
#define __LIBINFO_XZ_H__

#define LIBINFO_XZ_DICTSZ (1 << 15)

static const struct libinfo libinfo_xz = {
	.li_id = MICROFS_FLAG_DECOMPRESSOR_XZ,
	.li_streaming = 1,
	.li_min_blksz = MICROFS_MINBLKSZ,
	.li_max_blksz = MICROFS_MAXBLKSZ,
	.li_name = "xz"
};

