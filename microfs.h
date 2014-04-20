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

#ifndef __MICROFS_H__
#define __MICROFS_H__

#ifdef __cplusplus
#define __MICROFS_BEGIN_EXTERN_C extern "C" {
#define __MICROFS_END_EXTERN_C }
#else
#define __MICROFS_BEGIN_EXTERN_C
#define __MICROFS_END_EXTERN_C
#endif

__MICROFS_BEGIN_EXTERN_C

#ifdef __KERNEL__
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#endif

#include <asm/byteorder.h>
#include <linux/fs.h>
#include <linux/types.h>

#include "microfs_constants.h"
#include "microfs_flags.h"

#ifdef __KERNEL__

#include <linux/blkdev.h>
#include <linux/buffer_head.h>
#include <linux/dcache.h>
#include <linux/errno.h>
#include <linux/pagemap.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/vfs.h>

#include "libinfo.h"

#include "microfs_compat.h"

#ifdef DEBUG_SPAM
#ifndef pr_spam
#define pr_spam(fmt, ...) pr_devel(fmt, ##__VA_ARGS__)
#endif
#else
#ifndef pr_spam
#define pr_spam(fmt, ...)
#endif
#endif

#endif

#define __MICROFS_ISPOW2(N) \
	((N) > 1 && !((N) & ((N) - 1)))

static inline int microfs_ispow2(const __u64 n)
{
	return __MICROFS_ISPOW2(n);
}

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

/* Get the actual size stored in the %i_size*-fields of the
 * given inode.
 */
static inline __u32 i_getsize(const struct microfs_inode* const ino)
{
	return __le32_to_cpu(ino->i_size);
}

/* Store the given size in the %i_size*-fields of the given
 * inode. Pretty much only used by `microfsmki`.
 */
static inline void i_setsize(struct microfs_inode* const ino, const __u32 size)
{
	ino->i_size = __cpu_to_le32(size);
}

/* Get the number of blocks required for a file of %size
 * bytes with the given %blksz.
 */
static inline __u32 i_blks(const __u32 sz, const __u32 blksz)
{
	return sz == 0? 0: (sz - 1) / blksz + 1;
}

/* Round up the given %size to a multiple of the given block
 * size. The given %blksz must be a power of two.
 */
static inline __u32 sz_blkceil(const __u32 size, const __u32 blksz)
{
	return size == 0? blksz: ((size - 1) | (blksz - 1)) + 1;
}

#ifdef __KERNEL__

/* Buffer used to hold data read from the image.
 */
struct microfs_data_buffer {
	/* The data held by the buffer. */
	char* d_data;
	/* Number of bytes allocated for %d_data. */
	__u32 d_size;
	/* Number of bytes of %d_data that is used. */
	__u32 d_used;
	/* The offset that the data was read from. */
	__u32 d_offset;
};

/* In-memory super block.
 */
struct microfs_sb_info {
	/* Image size. */
	__u32 si_size;
	/* Feature flags. */
	__u32 si_flags;
	/* Number of blocks. */
	__u32 si_blocks;
	/* Number of files. */
	__u16 si_files;
	/* Image creation time. */
	__u32 si_ctime;
	/* Block size left shift. */
	__u16 si_blkshift;
	/* Block size. */
	__u32 si_blksz;
	/* Metadata block pointer buffer. */
	struct microfs_data_buffer si_metadata_blkptrbuf;
	/* Metadata dentry/inode buffer. */
	struct microfs_data_buffer si_metadata_dentrybuf;
	/* Compressed file data buffer. */
	struct microfs_data_buffer si_filedatabuf;
	/* Read mutex. */
	struct mutex si_rdmutex;
	/* Block data decompressor. */
	const struct microfs_decompressor* si_decompressor;
	/* Block data decompressor private storage. */
	void* si_decompressor_data;
};

/* A data block decompression abstraction.
 */
struct microfs_decompressor {
	/* Decompressor library info. */
	const struct libinfo* dc_info;
	/* Decompressor compiled? */
	const int dc_compiled;
	/* Allocate the necessary private data. */
	int (*dc_create)(struct microfs_sb_info* sbi);
	/* Free private data. */
	int (*dc_destroy)(struct microfs_sb_info* sbi);
	/* Reset the decompressor. */
	int (*dc_reset)(struct microfs_sb_info* sbi);
	/* Prepare the decompressor for %__microfs_copy_filedata_exceptionally. */
	int (*dc_exceptionally_begin)(struct microfs_sb_info* sbi);
	/* Prepare the decompressor for %__microfs_copy_filedata_nominally. */
	int (*dc_nominally_begin)(struct microfs_sb_info* sbi,
		struct page** pages, __u32 npages);
	/* Decompression stream: Is a new page cache page needed? */
	int (*dc_copy_nominally_needpage)(struct microfs_sb_info* sbi);
	/* Decompression stream: Use the given page cache page. */
	int (*dc_copy_nominally_utilizepage)(struct microfs_sb_info* sbi,
		struct page* page);
	/* Decompression stream: Release the given page cache page. */
	int (*dc_copy_nominally_releasepage)(struct microfs_sb_info* sbi,
		struct page* page);
	/* Use the data stored in the given buffer heads. */
	int (*dc_consumebhs)(struct microfs_sb_info* sbi,
		struct buffer_head** bhs, __u32 nbhs, __u32* length,
		__u32* bh, __u32* bh_offset, __u32* inflated, int* implerr);
	/* Continue consuming bhs? */
	int (*dc_continue)(struct microfs_sb_info* sbi,
		int err, int implerr, __u32 length, int more_avail_out);
	/* Complete a decompression operation. */
	int (*dc_end)(struct microfs_sb_info* sbi, int* err,
		int* implerr, __u32* decompressed);
};

extern const struct microfs_decompressor decompressor_zlib;
extern const struct microfs_decompressor decompressor_lz4;
extern const struct microfs_decompressor decompressor_lzo;
extern const struct microfs_decompressor decompressor_xz;

static inline struct microfs_sb_info* MICROFS_SB(struct super_block* sb)
{
	return sb->s_fs_info;
}

/* Get the inode number for the given on-disk inode.
 */
static inline unsigned long microfs_get_ino(const struct microfs_inode*
	const inode, const __u32 offset)
{
	const __u32 ioffset = __le32_to_cpu(inode->i_offset);
	return ioffset? ioffset: offset + 1;
}

static inline __u32 microfs_get_offset(const struct inode* const inode)
{
	return inode->i_ino;
}

/* Get a VFS inode for the given on-disk inode.
 */
struct inode* microfs_get_inode(struct super_block* sb,
	const struct microfs_inode* const microfs_ino, const __u32 offset);

typedef int (*microfs_read_blks_consumer)(struct super_block* sb,
	void* data, struct buffer_head** bhs, __u32 nbhs,
	__u32 offset, __u32 length);

typedef int (*microfs_read_blks_recycler)(struct super_block* sb,
	void* data, __u32 offset, __u32 length,
	microfs_read_blks_consumer consumer);

/* Read PAGE_CACHE_SIZEd blocks from the image. %consumer will
 * be called once the requested blocks have been read and is
 * responsible for doing something useful with them.
 */
int __microfs_read_blks(struct super_block* sb,
	struct address_space* mapping, void* data,
	microfs_read_blks_recycler recycler,
	microfs_read_blks_consumer consumer,
	__u32 offset, __u32 length);

/* Read data from the image, a pointer to the data at the
 * given offset is returned.
 */
void* __microfs_read(struct super_block* sb,
	struct microfs_data_buffer* destbuf, __u32 offset, __u32 length);

/* Fill the given page with data, if possible by inflating
 * it directly from the buffer head(s) to the page cache page(s).
 */
int __microfs_readpage(struct file* file, struct page* page);

/* Init the decompressor for %sbi.
 */
int microfs_decompressor_init(struct microfs_sb_info* sbi);

/* %microfs_decompressor op implementations used by LZ4
 * and LZO.
 */
#if defined(MICROFS_DECOMPRESSOR_LZ4) || defined(MICROFS_DECOMPRESSOR_LZO)
#define MICROFS_DECOMPRESSOR_LZ
int decompressor_lz_create(struct microfs_sb_info* sbi, __u32 upperbound);
int decompressor_lz_destroy(struct microfs_sb_info* sbi);
int decompressor_lz_reset(struct microfs_sb_info* sbi);
int decompressor_lz_exceptionally_begin(struct microfs_sb_info* sbi);
int decompressor_lz_nominally_begin(struct microfs_sb_info* sbi,
	struct page** pages, __u32 npages);
int decompressor_lz_copy_nominally_needpage(
	struct microfs_sb_info* sbi);
int decompressor_lz_copy_nominally_utilizepage(
	struct microfs_sb_info* sbi, struct page* page);
int decompressor_lz_copy_nominally_releasepage(
	struct microfs_sb_info* sbi, struct page* page);
int decompressor_lz_consumebhs(struct microfs_sb_info* sbi,
	struct buffer_head** bhs, __u32 nbhs, __u32* length,
	__u32* bh, __u32* bh_offset, __u32* inflated, int* implerr);
int decompressor_lz_continue(struct microfs_sb_info* sbi,
	int err, int implerr, __u32 length, int more_avail_out);
typedef int (*decompressor_lz_end_consumer)(struct microfs_sb_info* sbi,
	int* implerr, char* input, __u32 inputsz, char* output, __u32* outputsz);
int decompressor_lz_end(struct microfs_sb_info* sbi,
	int* err, int* implerr, __u32* decompressed,
	decompressor_lz_end_consumer consumer);
#endif

#endif

/* Determine if %sb->s_flags specifies unknown flags.
 */
static inline int sb_unsupportedflags(const struct microfs_sb* const sb)
{
	return __le32_to_cpu(sb->s_flags) & ~(MICROFS_SUPPORTED_FLAGS);
}

__MICROFS_END_EXTERN_C

#endif

