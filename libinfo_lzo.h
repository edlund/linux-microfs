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

#ifdef __LIBINFO_LZO_H__
#error "multiple includes"
#endif
#define __LIBINFO_LZO_H__

static const struct libinfo libinfo_lzo = {
	.li_id = MICROFS_FLAG_DECOMPRESSOR_LZO,
	.li_min_blksz = 0,
	.li_max_blksz = MICROFS_MAXBLKSZ,
	.li_dd_sz = 0,
	.li_name = "lzo"
};

