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

#ifndef __MICROFS_CONSTANTS_H__
#define __MICROFS_CONSTANTS_H__

/* Just a random nice looking integer (which at the moment of
 * writing does not yield any search results on Google).
 */
#define MICROFS_MAGIC 0x28211407
#define MICROFS_CIGAM 0x07142128

/* microfs signature.
 */
#define MICROFS_SIGNATURE "MinIReadOnlyFSys"

/* microfs default image name.
 */
#define MICROFS_DEFAULTNAME "Spaaaaaaaaaaace!"

/* Room at the beginning of the image which can be reserved
 * for boot code.
 */
#define MICROFS_PADDING 512

/* Block size limits.
 */
#define MICROFS_MINBLKSZ_SHIFT 9
#define MICROFS_MINBLKSZ (1 << MICROFS_MINBLKSZ_SHIFT)
#define MICROFS_MAXBLKSZ_SHIFT 20
#define MICROFS_MAXBLKSZ (1 << MICROFS_MAXBLKSZ_SHIFT)

/* "Bitfield" widths in %microfs_inode.
 */
#define MICROFS_IMODE_WIDTH     16
#define MICROFS_IUID_WIDTH      16
#define MICROFS_IGID_WIDTH      16
#define MICROFS_ISIZE_WIDTH     32
#define MICROFS_INAMELEN_WIDTH  8
#define MICROFS_IOFFSET_WIDTH   32

/* %microfs_sb.s_size determines the upper limit but with a
 * small twist: if %microfs_sb.s_size is set to zero, then
 * the actual image size is 2^32 bytes.
 * 
 * Also note that unlike cramfs, files in microfs can not
 * extend past the last block.
 */
#define MICROFS_MAXIMGSIZE \
	(1ULL << 32)

#define MICROFS_MINIMGSIZE \
	(1ULL << 12)

/* %microfs_inode.i_namelen gives a maximum file name length
 * of 255 bytes (not actual characters (think UTF-8)).
 */
#define MICROFS_MAXNAMELEN \
	((1ULL << MICROFS_INAMELEN_WIDTH) - 1)

#define MICROFS_MAXFILESIZE \
	((1ULL << MICROFS_ISIZE_WIDTH) - 1)

#define MICROFS_MAXCRAMSIZE \
	((1ULL << 24) - 1)

/* The maximum size of the metadata stored by a directory.
 */
#define MICROFS_MAXDIRSIZE \
	MICROFS_MAXCRAMSIZE

/* %microfs_sb.s_files determines the upper limit.
 */
#define MICROFS_MAXFILES \
	((1ULL << 16) - 1)

#define MICROFS_SBSIGNATURE_LENGTH 16
#define MICROFS_SBNAME_LENGTH 16

#endif

