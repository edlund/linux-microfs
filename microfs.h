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

#ifndef __MICROFS_H__
#define __MICROFS_H__

#ifdef __KERNEL__
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#endif

#include "microfs_compat.h"
#include "microfs_fs.h"

__MICROFS_BEGIN_EXTERN_C

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

#if defined(DEBUG) && defined(DEBUG_SPAM) && !defined(pr_spam)
#define pr_spam(fmt, ...) pr_devel(fmt, ##__VA_ARGS__)
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

struct microfs_decompressor;
struct microfs_decompressor_data;

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
	/* Buffer lock. */
	struct mutex d_mutex;
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
	/* Block data decompressor. */
	const struct microfs_decompressor* si_decompressor;
	/* Block data decompressor private storage. */
	struct microfs_decompressor_data* si_decompressor_data;
};

typedef int (*microfs_decompressor_data_creator)(struct microfs_sb_info* sbi,
	struct microfs_decompressor_data* data);

typedef int (*microfs_decompressor_data_acquirer)(struct microfs_sb_info* sbi,
	char* dd, struct microfs_decompressor_data** dest,
	microfs_decompressor_data_creator creator);

/* A data block decompression abstraction.
 */
struct microfs_decompressor {
	/* Decompressor library info. */
	const struct libinfo* dc_info;
	/* Decompressor compiled? */
	const int dc_compiled;
	/* Called by microfs_decompressor_data_manager_get(). */
	int (*dc_data_init)(struct microfs_sb_info* sbi, void* dd,
		struct microfs_decompressor_data* data);
	/* Cleans up after %dc_data_init(). */
	int (*dc_data_exit)(struct microfs_sb_info* sbi,
		struct microfs_decompressor_data* data);
	/* Allocate the necessary private data. */
	int (*dc_create)(struct microfs_sb_info* sbi, void** dest);
	/* Free private data, see %microfs_decompressor_data.dd_destroy(). */
	int (*dc_destroy)(struct microfs_sb_info* sbi, void* data);
	/* Reset the decompressor. */
	int (*dc_reset)(struct microfs_sb_info* sbi, void* data);
	/* Prepare the decompressor for %__microfs_copy_filedata_exceptionally. */
	int (*dc_exceptionally_begin)(struct microfs_sb_info* sbi, void* data);
	/* Prepare the decompressor for %__microfs_copy_filedata_nominally. */
	int (*dc_nominally_begin)(struct microfs_sb_info* sbi, void* data,
		struct page** pages, __u32 npages);
	/* Decompression stream: Is a new page cache page needed? */
	int (*dc_copy_nominally_needpage)(struct microfs_sb_info* sbi, void* data);
	/* Decompression stream: Use the given page cache page. */
	int (*dc_copy_nominally_utilizepage)(struct microfs_sb_info* sbi,
		void* data, struct page* page);
	/* Decompression stream: Release the given page cache page. */
	int (*dc_copy_nominally_releasepage)(struct microfs_sb_info* sbi,
		void* data, struct page* page);
	/* Use the data stored in the given buffer heads. */
	int (*dc_consumebhs)(struct microfs_sb_info* sbi, void* data,
		struct buffer_head** bhs, __u32 nbhs, __u32* length,
		__u32* bh, __u32* bh_offset, __u32* inflated, int* implerr);
	/* Continue consuming bhs? */
	int (*dc_continue)(struct microfs_sb_info* sbi, void* data,
		int err, int implerr, __u32 length, int more_avail_out);
	/* Complete a decompression operation. */
	int (*dc_end)(struct microfs_sb_info* sbi, void* data, int* err,
		int* implerr, __u32* decompressed);
};

/* %microfs_decompressor private data and access methods.
 * 
 * Each decompressor needs some private data in order to do
 * its job, and %microfs_decompressor_data is what is used
 * to store this data. Traditionally each mounted image
 * received its own instance of %microfs_decompressor_data,
 * where the actual data fed to the decompressor could be
 * stored in different ways (singleton, queue, percpu).
 * 
 * That solution works well but tends to use a big amount
 * of memory for buffers when there are many images mounted
 * with big block sizes at the same time.
 * 
 * To allow users (which know that they will use the same
 * decompressor and block size for multiple images that will
 * be mounted simultaneously) to save memory one can mount
 * them and request that they use "public" decompressor data.
 * 
 * Public decompressor data is shared among all images mounted
 * with the same options, the images will then compete for
 * access to the decompressor data when they need to have blocks
 * decompressed.
 * 
 * Private decompressor data is used exclusively for the image
 * that it is created for, meaning that mounting images with
 * options that could allow them share decompressor data will
 * still result in each mounted image getting its own data.
 * 
 * %microfs_decompressor_init(%sbi, %dd, %acquirer, %creator)
 * is used to get a decompressor instance for a %microfs_sb_info
 * instance. It needs two callbacks, %acquirer and %creator.
 * 
 * %acquirer deals with getting either a private or a public
 * decompressor data instance (as explained above). It can
 * be one of:
 * 
 * - %microfs_decompressor_data_manager_acquire_private()
 * - %microfs_decompressor_data_manager_acquire_public()
 * 
 * %creator deals with how the data that the decompressor
 * actually need (that is; its buffers, state and other
 * decompressor specific stuff) is managed. By using different
 * creators one can control how microfs will behave during
 * concurrent requests for decompression (the chosen %acquirer
 * will obviously also play a role here). Creator can be one
 * of:
 * 
 * - %microfs_decompressor_data_singleton_create()
 * - %microfs_decompressor_data_percpu_create()
 * - %microfs_decompressor_data_queue_create()
 * 
 * %acquirer and %creator can be mixed up according to user
 * need. The following combination could be used to get a
 * decompression behaviour as close as possible to that of
 * cramfs:
 * 
 * > %acquirer = %microfs_decompressor_data_manager_acquire_public()
 * > %creator = %microfs_decompressor_data_singleton_create()
 * 
 * When using public decompressor data, it is worth remembering
 * that %microfs_decompressor, %acquirer, %creator and block size
 * for the mounted images must be the same in order to have them
 * successfully share a decompressor data instance.
 */
struct microfs_decompressor_data {
	/* The block size used. */
	unsigned int dd_blksz;
	/* Number of users (mounted images) of this decompressor data. */
	unsigned int dd_users;
	/* List of public instances, if this instance is public. */
	struct list_head dd_sharelist;
	/* The decompressor that needs this decompressor data. */
	const struct microfs_decompressor* dd_decompressor;
	/* The %microfs_decompressor_data_creator that created this instance. */
	microfs_decompressor_data_creator dd_creator;
	/* Decompressor private data, see %dd_get() and %dd_put(). */
	void* dd_private;
	/* Decompressor info from the image. */
	void* dd_info;
	/* Get the decompressor data for use. */
	int (*dd_get)(struct microfs_sb_info* sbi, void** dest);
	/* Put the decompressor data after use. */
	int (*dd_put)(struct microfs_sb_info* sbi, void** src);
	/* Destroy decompressor data. */
	void (*dd_destroy)(struct microfs_sb_info* sbi, void* data);
	/* Release decompressor data, might not %dd_destroy() it. */
	void (*dd_release)(struct microfs_sb_info* sbi);
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

/* Read PAGE_SIZEd blocks from the image. %consumer will be
 * called once the requested blocks have been read and is
 * responsible for doing something useful with them. %recycler
 * will check if it is possible to give %consumer the requested
 * data from a cache instead of reading it from the image.
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

/* Init a decompressor for %sbi.
 * 
 * See %microfs_decompressor_data.
 */
int microfs_decompressor_init(struct microfs_sb_info* sbi, char* dd,
	microfs_decompressor_data_acquirer acquirer,
	microfs_decompressor_data_creator creator);

/* Used by decompressors without any persistant decompressor
 * data.
 */
int microfs_decompressor_data_init_noop(struct microfs_sb_info* sbi, void* dd,
	struct microfs_decompressor_data* data);
int microfs_decompressor_data_exit_noop(struct microfs_sb_info* sbi,
	struct microfs_decompressor_data* data);

/* A single decompressor data instance.
 */
int microfs_decompressor_data_singleton_create(struct microfs_sb_info* sbi,
	struct microfs_decompressor_data* data);

/* Multiple decompressor data instances available using
 * percpu pointers.
 */
int microfs_decompressor_data_percpu_create(struct microfs_sb_info* sbi,
	struct microfs_decompressor_data* data);

/* Multiple decompressor data instances available. If no
 * instances are available and no new ones can be created,
 * the calling thread will have to wait for another thread
 * stop using its decompressor data instance.
 * 
 * The maximum number of instances is 2 * num_online_cpus().
 */
int microfs_decompressor_data_queue_create(struct microfs_sb_info* sbi,
	struct microfs_decompressor_data* data);

/* init and exit for the decompressor data manager.
 */
void microfs_decompressor_data_manager_init(void);
void microfs_decompressor_data_manager_exit(void);

/* Acquire a private instance of decompressor data, i.e. an
 * instance that will only be used by a single mounted image.
 */
int microfs_decompressor_data_manager_acquire_private(struct microfs_sb_info* sbi,
	char* dd, struct microfs_decompressor_data** dest,
	microfs_decompressor_data_creator creator);

/* Acquire a public instance of decompressor data, i.e. an
 * instance that will be used by all mounted images which
 * requests public decompressor data of the same type.
 */
int microfs_decompressor_data_manager_acquire_public(struct microfs_sb_info* sbi,
	char* dd, struct microfs_decompressor_data** dest,
	microfs_decompressor_data_creator creator);

/* %microfs_decompressor op implementations used by LZ4
 * and LZO.
 */
#if defined(MICROFS_DECOMPRESSOR_LZ4) || defined(MICROFS_DECOMPRESSOR_LZO)
#define MICROFS_DECOMPRESSOR_LZ

typedef int (*decompressor_lz_end_consumer)(struct microfs_sb_info* sbi,
	void* data, int* implerr,
	char* input, __u32 inputsz,
	char* output, __u32* outputsz);

int decompressor_lz_create(struct microfs_sb_info* sbi, void** dest, __u32 upperbound);
int decompressor_lz_destroy(struct microfs_sb_info* sbi, void* data);
int decompressor_lz_reset(struct microfs_sb_info* sbi, void* data);
int decompressor_lz_exceptionally_begin(struct microfs_sb_info* sbi, void* data);
int decompressor_lz_nominally_begin(struct microfs_sb_info* sbi,
	void* data, struct page** pages, __u32 npages);
int decompressor_lz_copy_nominally_needpage(struct microfs_sb_info* sbi,
	void* data);
int decompressor_lz_copy_nominally_utilizepage(struct microfs_sb_info* sbi,
	void* data, struct page* page);
int decompressor_lz_copy_nominally_releasepage(struct microfs_sb_info* sbi,
	void* data, struct page* page);
int decompressor_lz_consumebhs(struct microfs_sb_info* sbi, void* data,
	struct buffer_head** bhs, __u32 nbhs, __u32* length,
	__u32* bh, __u32* bh_offset, __u32* inflated, int* implerr);
int decompressor_lz_continue(struct microfs_sb_info* sbi, void* data,
	int err, int implerr, __u32 length, int more_avail_out);
int decompressor_lz_end(struct microfs_sb_info* sbi, void* data,
	int* err, int* implerr, __u32* decompressed,
	decompressor_lz_end_consumer consumer);

#endif

/* Get the insert ID for this module insertion. Used by tests
 * to have additional information written to the syslog.
 */
int __debug_insid(void);

#endif

__MICROFS_END_EXTERN_C

#endif

