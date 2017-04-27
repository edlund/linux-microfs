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

#ifndef __MICROFS_FS_H__
#define __MICROFS_FS_H__

#include "microfs_compat.h"

__MICROFS_BEGIN_EXTERN_C

#include <asm/byteorder.h>
#include <linux/fs.h>
#include <linux/types.h>

/* Just a random nice looking integer (which at the moment of
 * writing does not yield any search results on Google).
 */
#define MICROFS_MAGIC 0x28211407
#define MICROFS_CIGAM 0x07142128

#define MICROFS_DD_XZ_MAGIC 0x377a585a
#define MICROFS_DD_XZ_CIGAM 0x5a587a37

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

#define MICROFS_DEFAULBLKSHIFT 17
#define MICROFS_DEFAULBLKSZ (1 << MICROFS_DEFAULBLKSHIFT)

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

/* "On-disk" inode.
 * 
 * Compared to cramfs_inode this representation is slightly
 * more verbose. It comes with four benefits:
 * 
 * #1: GIDs are not truncated.
 * #2: Images can be much larger (2^32 bytes).
 * #3: File names can be slightly longer.
 * #4: Files can be much larger (2^32 - 1 bytes).
 * 
 * (Disclaimer: #2, #3 and #4 might not be _that_ useful.)
 */
struct microfs_inode {
	/* File mode. */
	__le16 i_mode;
	/* Low 16 bits of User ID. */
	__le16 i_uid;
	/* Low 16 bits of Group ID. */
	__le16 i_gid;
	/* File size. */
	__le32 i_size;
	/* Filename length. */
	__u8 i_namelen;
	/* Offset for the block pointers. */
	__le32 i_offset;
} __attribute__ ((packed));

/* "On-disk" superblock.
 */
struct microfs_sb {
	/* MICROFS_MAGIC. */
	__le32 s_magic;
	/* Image size. */
	__le32 s_size;
	/* Feature flags. */
	__le32 s_flags;
	/* Image CRC checksum. */
	__le32 s_crc;
	/* Number of blocks. */
	__le32 s_blocks;
	/* Number of files. */
	__le16 s_files;
	/* Image creation time. */
	__le32 s_ctime;
	/* Block size left shift. */
	__le16 s_blkshift;
	/* Reserved. */
	 __le16 s_future;
	/* MICROFS_SIGNATURE. */
	__u8 s_signature[MICROFS_SBSIGNATURE_LENGTH];
	/* User defined image name. */
	__u8 s_name[MICROFS_SBNAME_LENGTH];
	/* Root inode. */
	struct microfs_inode s_root;
} __attribute__ ((packed));

/* "On-disk" xz decompressor data.
 */
struct microfs_dd_xz {
	/* MICROFS_DD_XZ_MAGIC. */
	__le32 dd_magic;
	/* Dictionary size. */
	__le32 dd_dictsz;
}  __attribute__ ((packed));

static inline __u32 i_getsize(const struct microfs_inode* const ino)
{
	return __le32_to_cpu(ino->i_size);
}

static inline void i_setsize(struct microfs_inode* const ino, const __u32 size)
{
	ino->i_size = __cpu_to_le32(size);
}

/* Determine if %sb->s_flags specifies unknown flags.
 */
static inline int sb_unsupportedflags(const struct microfs_sb* const sb)
{
	return __le32_to_cpu(sb->s_flags) & ~(MICROFS_SUPPORTED_FLAGS);
}

__MICROFS_END_EXTERN_C

#endif

