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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/parser.h>

MODULE_DESCRIPTION("microfs - Minimally Improved Compressed Read Only File System");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Erik Edlund <erik.edlund@32767.se>");
MODULE_ALIAS_FS("microfs");

#if !( \
		defined(MICROFS_DECOMPRESSOR_ZLIB) || \
		defined(MICROFS_DECOMPRESSOR_LZ4)  || \
		defined(MICROFS_DECOMPRESSOR_LZO)  || \
		defined(MICROFS_DECOMPRESSOR_XZ)      \
	)
#error "pointless build, see README"
#endif

static int debug_insid = 0;
module_param(debug_insid, int, 0000);
MODULE_PARM_DESC(debug_insid, "Debug ID for the module insertion");

int __debug_insid(void)
{
	return debug_insid;
}

static const struct super_operations microfs_s_ops;

enum {
	Opt_metadata_blkptrbufsz,
	Opt_metadata_dentrybufsz,
	Opt_decompressor_data_acquirer,
	Opt_decompressor_data_creator,
	Opt_debug_mountid,
	Opt_debug_cksig
};

static const match_table_t microfs_tokens = {
	{ Opt_metadata_blkptrbufsz, "metadata_blkptrbufsz=%u" },
	{ Opt_metadata_dentrybufsz, "metadata_dentrybufsz=%u" },
	{ Opt_decompressor_data_acquirer, "decompressor_data_acquirer=%s" },
	{ Opt_decompressor_data_creator, "decompressor_data_creator=%s" },
	{ Opt_debug_mountid, "debug_mountid=%u" },
	{ Opt_debug_cksig, "debug_cksig=%u" }
};

struct microfs_mount_options {
	__u64 mo_metadata_blkptrbufsz;
	__u64 mo_metadata_dentrybufsz;
	microfs_decompressor_data_creator mo_decompressor_data_creator;
	microfs_decompressor_data_acquirer mo_decompressor_data_acquirer;
	int mo_debug_cksig;
};

static __u64 select_bufsz(const char* const name, int requested,
	const __u64 minimum)
{
	if (requested <= minimum) {
		pr_warn("%s=%d, it must be larger than %llu to be usable\n",
			name, requested, minimum);
		return minimum;
	}
	if (requested % PAGE_SIZE != 0) {
		pr_warn("%s=%d, but it must be a multiple of %u\n",
			name, requested, (__u32)PAGE_SIZE);
	}
	return sz_blkceil(requested, PAGE_SIZE);
}

static int microfs_parse_options(char* options, struct microfs_sb_info* const sbi,
	struct microfs_mount_options* const mount_opts)
{
	char* part;
	char* creator;
	char* acquirer;
	substring_t args[MAX_OPT_ARGS];
	
	(void)sbi;
	
	pr_spam("microfs_parse_options: options=(%s)\n", options);

	while (options && (part = strsep(&options, ",")) != NULL) {
		int token;
		int option;
		
		if (!*part)
			continue;
		
		token = match_token(part, microfs_tokens, args);
		switch (token) {
			
#define OPT_SZ(Name) \
	case Opt_##Name: \
		if (match_int(&args[0], &option)) \
			return 0; \
		mount_opts->mo_##Name = select_bufsz(#Name, \
			option, mount_opts->mo_##Name); \
		break
			
			OPT_SZ(metadata_blkptrbufsz);
			OPT_SZ(metadata_dentrybufsz);
			
			case Opt_decompressor_data_acquirer:
				acquirer = match_strdup(&args[0]);
				if (strcmp(acquirer, "private") == 0) {
					mount_opts->mo_decompressor_data_acquirer
						= microfs_decompressor_data_manager_acquire_private;
				} else if (strcmp(acquirer, "public") == 0) {
					mount_opts->mo_decompressor_data_acquirer
						= microfs_decompressor_data_manager_acquire_public;
				} else {
					pr_warn("unknown decompressor_data_acquirer requested"
						" - the default will be used\n");
				}
				kfree(acquirer);
				break;
			case Opt_decompressor_data_creator:
				creator = match_strdup(&args[0]);
				if (strcmp(creator, "singleton") == 0) {
					mount_opts->mo_decompressor_data_creator
						= microfs_decompressor_data_singleton_create;
				} else if (strcmp(creator, "percpu") == 0) {
					mount_opts->mo_decompressor_data_creator
						= microfs_decompressor_data_percpu_create;
				} else if (strcmp(creator, "queue") == 0) {
					mount_opts->mo_decompressor_data_creator
						= microfs_decompressor_data_queue_create;
				} else {
					pr_warn("unknown decompressor_data_creator requested"
						" - the default will be used\n");
				}
				kfree(creator);
				break;
			case Opt_debug_mountid:
				if (match_int(&args[0], &option))
					return 0;
				pr_info("debug_mountid=%d\n", option);
				break;
			case Opt_debug_cksig:
				if (match_int(&args[0], &option))
					return 0;
				mount_opts->mo_debug_cksig = 1 && option;
				break;
			default:
				return 0;
			
#undef OPT_SZ
			
		}
	}
	
	return 1;
}

static int create_data_buffer(struct microfs_data_buffer* dbuf,
	__u32 sz, const char* const name)
{
	dbuf->d_offset = MICROFS_MAXIMGSIZE - 1;
	dbuf->d_size = sz;
	dbuf->d_data = kmalloc(dbuf->d_size, GFP_KERNEL);
	if (!dbuf->d_data) {
		pr_err("could not allocate data buffer %s\n", name);
		return -ENOMEM;
	}
	mutex_init(&dbuf->d_mutex);
	return 0;
}

static void destroy_data_buffer(struct microfs_data_buffer* dbuf)
{
	kfree(dbuf->d_data);
}

static int microfs_fill_super(struct super_block* sb, void* data, int silent)
{
	int err = 0;
	
	struct inode* root = NULL;
	struct buffer_head* bh = NULL;
	struct microfs_sb* msb = NULL;
	struct microfs_sb_info* sbi = NULL;
	
	__u32 sb_actual_root_offset;
	__u32 sb_expected_root_offset;
	__u32 sb_padding = 0;
	
	__u64 sb_blksz = 0;
	
	struct microfs_mount_options mount_opts;
	
	sb->s_flags |= MS_RDONLY;
	
	sbi = kzalloc(sizeof(*sbi), GFP_KERNEL);
	if (!sbi) {
		pr_err("failed to allocate memory for the sb info\n");
		err = -ENOMEM;
		goto err_sbi;
	}
	sb->s_fs_info = sbi;
	
/* As far as I know, this should never happen, but check it
 * anyway, what I do not know could fill a mid-sized space
 * station... This is mostly here so that it is possible to
 * easily read the super block using a single sb_bread()-call.
 */
#if PAGE_SIZE < 1024
#error "PAGE_SIZE is too small"
#endif

/* It is unnecessary to stop someone who really want this to
 * work, but do warn them.
 */
#if PAGE_SIZE > MICROFS_MAXBLKSZ
#warning "PAGE_SIZE greater than MICROFS_MAXBLKSZ is not supported"
#endif
	
	/* The metadata buffers must span at least two PAGE_SIZE
	 * sized VFS "blocks" so that poor data alignment does not
	 * cause oob errors (data starting in one VFS block and ending
	 * at the start of the adjoining block).
	 */
	mount_opts.mo_metadata_blkptrbufsz = PAGE_SIZE * 2;
	mount_opts.mo_metadata_dentrybufsz = PAGE_SIZE * 2;
	mount_opts.mo_decompressor_data_creator = microfs_decompressor_data_singleton_create;
	mount_opts.mo_decompressor_data_acquirer = microfs_decompressor_data_manager_acquire_private;
	mount_opts.mo_debug_cksig = 0;
	
	if (!microfs_parse_options(data, sbi, &mount_opts)) {
		pr_err("failed to parse mount options\n");
		err = -EINVAL;
		goto err_sbi;
	}
	
	if (sb->s_bdev->bd_inode->i_size % PAGE_SIZE != 0) {
		pr_err("device size is not a multiple of PAGE_SIZE\n");
		err = -EINVAL;
		goto err_sbi;
	}
	
	sb_blksz = sb_set_blocksize(sb, PAGE_SIZE);
	if (!sb_blksz) {
		pr_err("failed to set the block size to PAGE_SIZE"
			" (%llu bytes)\n", (__u64)PAGE_SIZE);
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
	
	if (mount_opts.mo_debug_cksig) {
		if (memcmp(msb->s_signature, MICROFS_SIGNATURE, sizeof(msb->s_signature)) != 0) {
			pr_err("superblock at offset 0x%x has an invalid signature"
					" (it should be \"%s\")\n",
				sb_padding, MICROFS_SIGNATURE);
			err = -EINVAL;
			goto err_sb;
		}
		if (__debug_insid()) {
			pr_info("[insid=%d] microfs_fill_super:"
				" good superblock signature\n", __debug_insid());
		}
	}
	
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
	
	/* The filedata buffer must be big enough to fit either an
	 * entire block or a whole page, depending on the used block
	 * size.
	 */
	if ((err = create_data_buffer(&sbi->si_filedatabuf,
			max_t(__u32, sbi->si_blksz, PAGE_SIZE), "sbi->si_filedatabuf")) < 0)
		goto err_filedatabuf;
	
	if ((err = create_data_buffer(&sbi->si_metadata_blkptrbuf,
			mount_opts.mo_metadata_blkptrbufsz, "sbi->si_metadata_blkptrbuf")) < 0)
		goto err_metadata_blkptrbuf;
	if ((err = create_data_buffer(&sbi->si_metadata_dentrybuf,
			mount_opts.mo_metadata_dentrybufsz, "sbi->si_metadata_dentrybuf")) < 0)
		goto err_metadata_dentrybuf;
	
	err = microfs_decompressor_init(sbi, bh->b_data + sb_padding + sizeof(*msb),
		mount_opts.mo_decompressor_data_acquirer, mount_opts.mo_decompressor_data_creator);
	if (err < 0) {
		pr_err("failed to init the decompressor\n");
		goto err_decompressor_init;
	}
	
	sb_actual_root_offset = __le32_to_cpu(msb->s_root.i_offset);
	sb_expected_root_offset = sb_padding + sizeof(*msb)
		+ sbi->si_decompressor->dc_info->li_dd_sz;
	
	if (sb_actual_root_offset == 0) {
		pr_info("this image is empty\n");
	} else if (sb_actual_root_offset != sb_expected_root_offset) {
		pr_err("bad root offset: 0x%x, expected 0x%x\n",
			sb_actual_root_offset, sb_expected_root_offset);
		err = -EINVAL;
		goto err_root_offset;
	}
	
	msb = NULL;
	brelse(bh);
	bh = NULL;
	
	pr_devel("successfully filled super block 0x%p\n", sb);
	
	return 0;
	
err_root_offset:
	/* Fall-through. */
err_decompressor_init:
	if (sbi->si_decompressor_data)
		sbi->si_decompressor_data->dd_release(sbi);
err_metadata_dentrybuf:
	destroy_data_buffer(&sbi->si_metadata_dentrybuf);
err_metadata_blkptrbuf:
	destroy_data_buffer(&sbi->si_metadata_blkptrbuf);
err_filedatabuf:
	destroy_data_buffer(&sbi->si_filedatabuf);
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
	struct microfs_sb_info* sbi = MICROFS_SB(sb);
	
	destroy_data_buffer(&sbi->si_filedatabuf);
	destroy_data_buffer(&sbi->si_metadata_blkptrbuf);
	destroy_data_buffer(&sbi->si_metadata_dentrybuf);
	
	if (sbi->si_decompressor_data)
		sbi->si_decompressor_data->dd_release(sbi);
	
	sbi->si_decompressor_data = NULL;
	sbi->si_decompressor = NULL;
	
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
	
	if (__debug_insid())
		pr_info("[insid=%d] microfs_statfs: ok\n", __debug_insid());
	
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
	
	if (__debug_insid())
		pr_info("[insid=%d] microfs_init\n", __debug_insid());
	
	pr_info("microfs comes with ABSOLUTELY NO WARRANTY;"
		" without even the implied warranty of MERCHANTABILITY"
		" or FITNESS FOR A PARTICULAR PURPOSE. See the GNU"
		" General Public License for more details.\n");
	
	err = register_filesystem(&microfs_fs_type);
	microfs_decompressor_data_manager_init();
	
	return err;
} module_init(microfs_init);

static void __exit microfs_exit(void)
{
	microfs_decompressor_data_manager_exit();
	unregister_filesystem(&microfs_fs_type);
	if (__debug_insid())
		pr_info("[insid=%d] microfs_exit\n", __debug_insid());
} module_exit(microfs_exit);

