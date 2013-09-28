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

#include <linux/module.h>
#include <linux/parser.h>

MODULE_DESCRIPTION("microfs - Minimally Improved Compressed Read Only File System");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Erik Edlund <erik.edlund@32767.se>");
MODULE_ALIAS_FS("microfs");

static const struct super_operations microfs_s_ops;

enum {
	Opt_databufsz,
	Opt_blkptrbufsz,
	Opt_dentbufsz
};

static const match_table_t microfs_tokens = {
	{ Opt_databufsz, "databufsz=%u" },
	{ Opt_blkptrbufsz, "blkptrbufsz=%u" },
	{ Opt_dentbufsz, "dentbufsz=%u" },
};

struct microfs_mount_options {
	size_t mo_databufsz;
	size_t mo_blkptrbufsz;
	size_t mo_dentbufsz;
};

static size_t microfs_select_bufsz(const char* const name, int requested,
	const size_t minimum)
{
	if (requested <= minimum) {
		pr_warn("%s=%d, it must be larger than %zu to be usable\n",
			name, requested, minimum);
		return minimum;
	}
	if (requested % PAGE_CACHE_SIZE != 0) {
		pr_warn("%s=%d, but it must be a multiple of %zu\n",
			name, requested, PAGE_CACHE_SIZE);
	}
	return sz_blkceil(requested, PAGE_CACHE_SIZE);
}

static int microfs_parse_options(char* options, struct microfs_sb_info* const sbi,
	struct microfs_mount_options* const mount_opts)
{
	char* part;
	substring_t args[MAX_OPT_ARGS];
	
	(void)sbi;
	
	while (options && (part = strsep(&options, ",")) != NULL) {
		int token;
		int option;
		
		if (!*part)
			continue;
		
		token = match_token(part, microfs_tokens, args);
		switch (token) {
			
#define BUFSZ_OPT(Name) \
	case Opt_##Name: \
		if (match_int(&args[0], &option)) \
			return 0; \
		mount_opts->mo_##Name = microfs_select_bufsz(#Name, \
			option, mount_opts->mo_##Name); \
		break
			
			BUFSZ_OPT(databufsz);
			BUFSZ_OPT(blkptrbufsz);
			BUFSZ_OPT(dentbufsz);
			
#undef BUFSZ_OPT
			
			default:
				return 0;
		}
	}
	
	return 1;
}

static int microfs_fill_super(struct super_block* sb, void* data, int silent)
{
	int err = 0;
	
	struct inode* root = NULL;
	struct buffer_head* bh = NULL;
	struct microfs_sb* msb = NULL;
	struct microfs_sb_info* sbi = NULL;
	
	__u32 sb_root_offset = 0;
	__u32 sb_padding = 0;
	
	size_t sb_blksz = 0;
	
	struct microfs_mount_options mount_opts;
	
	sb->s_flags |= MS_RDONLY;
	
	sbi = kzalloc(sizeof(*sbi), GFP_KERNEL);
	if (!sbi) {
		pr_err("failed to allocate memory for the sb info\n");
		err = -ENOMEM;
		goto err_sbi;
	}
	sb->s_fs_info = sbi;
	
	mutex_init(&sbi->si_read.rd_mutex);
	
/* As far as I know, this should never happen, but check it
 * anyway, what I do not know could fill a mid-sized space
 * station... This is mostly here so that it is possible to
 * easily read the super block using a single sb_bread()-call.
 */
#if PAGE_CACHE_SIZE < 1024
#error "PAGE_CACHE_SIZE is too small"
#endif
	
/* It is unnecessary to stop someone who really want this to
 * work, but do warn them.
 */
#if PAGE_CACHE_SIZE > MICROFS_MAXBLKSZ
#warning "PAGE_CACHE_SIZE greater than MICROFS_MAXBLKSZ is not supported"
#endif
	
	if (sb->s_bdev->bd_inode->i_size % PAGE_CACHE_SIZE != 0) {
		pr_err("device size is not a multiple of PAGE_CACHE_SIZE\n");
		err = -EINVAL;
		goto err_sbi;
	}
	
	sb_blksz = sb_set_blocksize(sb, PAGE_CACHE_SIZE);
	if (!sb_blksz) {
		pr_err("failed to set the block size to PAGE_CACHE_SIZE"
			" (%zu bytes)\n", PAGE_CACHE_SIZE);
		err = -EIO;
		goto err_sbi;
	}
	
	bh = sb_bread(sb, 0);
	if (!bh) {
		pr_err("failed to read block 0\n");
		err = -EIO;
		goto err_sbi;
	}
	
sb_retry:
	msb = (struct microfs_sb*)(bh->b_data + sb_padding);
	sb->s_magic = __le32_to_cpu(msb->s_magic);
	if (sb->s_magic != MICROFS_MAGIC) {
		if (sb_padding == MICROFS_PADDING) {
			pr_err("failed to find the superblock"
					" (looked for it at offset 0x%x and 0x%x)\n",
				0, sb_padding);
			err = -EINVAL;
			goto err_sb;
		}
		sb_padding = MICROFS_PADDING;
		goto sb_retry;
	}
	pr_devel("found superblock at offset 0x%x\n", sb_padding);
	
	if (sb_unsupportedflags(msb)) {
		pr_err("unsupported filesystem features\n");
		err = -EINVAL;
		goto err_sb;
	}
	
	if (!S_ISDIR(__le16_to_cpu(msb->s_root.i_mode))) {
		pr_err("the superblock root inode is not a directory\n");
		err = -EINVAL;
		goto err_sb;
	}
	
	sb_root_offset = __le32_to_cpu(msb->s_root.i_offset);
	if (sb_root_offset == 0) {
		pr_info("this image is empty\n");
	} else if (sb_root_offset != sb_padding + sizeof(struct microfs_sb)) {
		pr_err("bad root offset: 0x%x\n", sb_root_offset);
		err = -EINVAL;
		goto err_sb;
	}
	
	sbi->si_size = __le32_to_cpu(msb->s_size);
	sbi->si_flags = __le32_to_cpu(msb->s_flags);
	sbi->si_blocks = __le32_to_cpu(msb->s_blocks);
	sbi->si_files = __le16_to_cpu(msb->s_files);
	sbi->si_ctime = __le32_to_cpu(msb->s_ctime);
	sbi->si_blkshift = __le16_to_cpu(msb->s_blkshift);
	sbi->si_blksz = 1 << sbi->si_blkshift;
	
	msb->s_root.i_mode = __cpu_to_le16(
		__le16_to_cpu(msb->s_root.i_mode) | (
			S_IRUSR | S_IXUSR |
			S_IRGRP | S_IXGRP |
			S_IROTH | S_IXOTH
		)
	);
	
	sb->s_op = &microfs_s_ops;
	
	root = microfs_get_inode(sb, &msb->s_root, sb_padding);
	if (IS_ERR(root)) {
		pr_err("failed to get the root inode\n");
		err = PTR_ERR(root);
		goto err_sb;
	}
	
	sb->s_root = d_make_root(root);
	if (!sb->s_root) {
		pr_err("failed to set the root inode for the superblock\n");
		err = -ENOMEM;
		goto err_sb;
	}
	
	/* The data buffer must be big enough to fit a block with
	 * with poor alignment which also "compressed" to its upper
	 * bound size.
	 * 
	 * All buffers must span at least two PAGE_CACHE_SIZE sized
	 * VFS "blocks" so that poor data alignment does not cause oob
	 * errors (data starting in one VFS block and ending at the
	 * start of the adjoining block).
	 */
	mount_opts.mo_databufsz = sz_blkceil(sbi->si_blksz * 3,
		PAGE_CACHE_SIZE) + PAGE_CACHE_SIZE;
	mount_opts.mo_blkptrbufsz = PAGE_CACHE_SIZE * 2;
	mount_opts.mo_dentbufsz = PAGE_CACHE_SIZE * 2;
	
	if (!microfs_parse_options(data, sbi, &mount_opts)) {
		pr_err("failed to parse mount options\n");
		err = -EINVAL;
		goto err_opts;
	}
	
#define setup_rdbuf(Buf, Sz, Err) \
	do { \
		struct microfs_read_buffer* rdbuf = &Buf; \
		rdbuf->rb_size = Sz; \
		rdbuf->rb_offset = (1UL << 32) - 1; \
		rdbuf->rb_data = vmalloc(rdbuf->rb_size); \
		rdbuf->rb_npages = rdbuf->rb_size / PAGE_CACHE_SIZE; \
		rdbuf->rb_pages = kmalloc(rdbuf->rb_npages \
			* sizeof(*rdbuf->rb_pages), GFP_KERNEL); \
		if (!rdbuf->rb_data || !rdbuf->rb_pages) { \
			pr_err("could not allocate read buffer %s\n", #Buf); \
			err = -ENOMEM; \
			goto Err; \
		} \
		rdbuf = NULL; \
		pr_devel("%zu bytes allocated for %s\n", Sz, #Buf); \
	} while (0)
	
	setup_rdbuf(sbi->si_read.rd_databuf,
		mount_opts.mo_databufsz, err_rd_databuf);
	setup_rdbuf(sbi->si_read.rd_blkptrbuf,
		mount_opts.mo_blkptrbufsz, err_rd_blkptrbuf);
	setup_rdbuf(sbi->si_read.rd_dentbuf,
		mount_opts.mo_dentbufsz, err_rd_dentbuf);
	
	sbi->si_read.rd_rbs[MICROFS_READER_RB_DATA] = &sbi->si_read
		.rd_databuf;
	sbi->si_read.rd_rbs[MICROFS_READER_RB_BLKPTR] = &sbi->si_read
		.rd_blkptrbuf;
	sbi->si_read.rd_rbs[MICROFS_READER_RB_DENT] = &sbi->si_read
		.rd_dentbuf;
	
	if (sbi->si_blksz > PAGE_CACHE_SIZE) {
		sbi->si_read.rd_inflatebuf.ib_offset = (1UL << 32) - 1;
		sbi->si_read.rd_inflatebuf.ib_data = vmalloc(sbi->si_blksz);
		if (!sbi->si_read.rd_inflatebuf.ib_data) {
			pr_err("could not allocate the inflate buffer\n");
			err = -ENOMEM;
			goto err_rd_inflatebuf;
		}
		pr_devel("%u bytes allocated for sbi->si_read.rd_inflatebuf.ib_data",
			sbi->si_blksz);
	}
	
	err = microfs_inflate_init(&sbi->si_read);
	if (err != 0) {
		pr_err("failed to init the zlib stream\n");
		goto err_inflate_init;
	}
	
#undef setup_rdbuf
	
	msb = NULL;
	brelse(bh);
	bh = NULL;
	
	pr_devel("successfully filled super block 0x%p\n", sb);
	
	return 0;
	
err_inflate_init:
	vfree(sbi->si_read.rd_inflatebuf.ib_data);
err_rd_inflatebuf:
	/* Fall-through. */
err_rd_dentbuf:
	vfree(sbi->si_read.rd_dentbuf.rb_data);
	kfree(sbi->si_read.rd_dentbuf.rb_pages);
err_rd_blkptrbuf:
	vfree(sbi->si_read.rd_blkptrbuf.rb_data);
	kfree(sbi->si_read.rd_blkptrbuf.rb_pages);
err_rd_databuf:
	vfree(sbi->si_read.rd_databuf.rb_data);
	kfree(sbi->si_read.rd_databuf.rb_pages);
err_opts:
	/* Fall-through. */
err_sb:
	msb = NULL;
	brelse(bh);
	bh = NULL;
err_sbi:
	sb->s_fs_info = NULL;
	kfree(sbi);
	sbi = NULL;
	pr_err("failed to fill super block 0x%p\n", sb);
	return err;
}

static struct dentry* microfs_mount(struct file_system_type* fs_type,
	int flags, const char* dev_name, void* data)
{
	return mount_bdev(fs_type, flags, dev_name, data,
		microfs_fill_super);
}

static int microfs_remount_fs(struct super_block* sb, int* flags, char* data)
{
	*flags |= MS_RDONLY;
	return 0;
}

static void microfs_put_super(struct super_block* sb)
{
	vfree(MICROFS_SB(sb)->si_read.rd_inflatebuf.ib_data);
	
	vfree(MICROFS_SB(sb)->si_read.rd_dentbuf.rb_data);
	kfree(MICROFS_SB(sb)->si_read.rd_dentbuf.rb_pages);
	
	vfree(MICROFS_SB(sb)->si_read.rd_blkptrbuf.rb_data);
	kfree(MICROFS_SB(sb)->si_read.rd_blkptrbuf.rb_pages);
	
	vfree(MICROFS_SB(sb)->si_read.rd_databuf.rb_data);
	kfree(MICROFS_SB(sb)->si_read.rd_databuf.rb_pages);
	
	microfs_inflate_end(&MICROFS_SB(sb)->si_read);
	
	kfree(sb->s_fs_info);
	sb->s_fs_info = NULL;
	
	pr_devel("resources released for super block 0x%p\n", sb);
}

static int microfs_statfs(struct dentry* dentry, struct kstatfs* buf)
{
	struct super_block* sb = dentry->d_sb;
	__u64 dev_id = huge_encode_dev(sb->s_bdev->bd_dev);
	
	buf->f_type = MICROFS_MAGIC;
	buf->f_bsize = MICROFS_SB(sb)->si_blksz;
	buf->f_blocks = MICROFS_SB(sb)->si_blocks;
	buf->f_bfree = 0;
	buf->f_bavail = 0;
	buf->f_files = MICROFS_SB(sb)->si_files;
	buf->f_ffree = 0;
	buf->f_fsid.val[0] = (__u32)(dev_id >> 0);
	buf->f_fsid.val[1] = (__u32)(dev_id >> 32);
	buf->f_namelen = MICROFS_MAXNAMELEN;
	
	return 0;
}

static struct file_system_type microfs_fs_type = {
	.owner = THIS_MODULE,
	.name = "microfs",
	.mount = microfs_mount,
	.kill_sb = kill_block_super,
	.fs_flags = FS_REQUIRES_DEV
};

static const struct super_operations microfs_s_ops = {
	.put_super = microfs_put_super,
	.remount_fs = microfs_remount_fs,
	.statfs = microfs_statfs,
};

static int __init microfs_init(void)
{
	int err;
	
	pr_devel("microfs_init\n");
	pr_info("microfs comes with ABSOLUTELY NO WARRANTY;"
		" without even the implied warranty of MERCHANTABILITY"
		" or FITNESS FOR A PARTICULAR PURPOSE. See the GNU"
		" General Public License for more details.\n");
	
	err = register_filesystem(&microfs_fs_type);
	
	return err;
} module_init(microfs_init);

static void __exit microfs_exit(void)
{
	unregister_filesystem(&microfs_fs_type);
	pr_devel("microfs_exit\n");
} module_exit(microfs_exit);
