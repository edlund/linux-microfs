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

// microfs check image

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#define _FILE_OFFSET_BITS 64

#include "hostprogs.h"
#include "microfs.h"

#include <dirent.h>
#include <utime.h>

#include <sys/ioctl.h>

/* getopt() args, see usage().
 */
#define CKI_OPTIONS "hvqeMOUx:"

/* Description of an image.
 */
struct imgdesc {
	/* Input file. */
	const char* de_infile;
	/* Output directory, used if/when extracting. */
	const char* de_extractdir;
	/* Length of %de_extractdir. */
	size_t de_extractdirlen;
	/* Image file descriptor. */
	int de_fd;
	/* Memory mapping of the entire image. */
	unsigned char* de_image;
	/* Buffer used for decompressing blocks. */
	unsigned char* de_decompressionbuf;
	/* Size of de_decompressionbuf. */
	size_t de_decompressionbufsz;
	/* Outer size of the image. */
	size_t de_outersz;
	/* Inner size of the image. */
	size_t de_innersz;
	/* Image block size. */
	size_t de_blksz;
	/* Image block shift. */
	size_t de_blkshift;
	/* Quick access to the superblock. */
	struct microfs_sb* de_sb;
	/* zlib stream. */
	z_stream de_zstream;
	/* Do a quick check. */
	int de_quickie;
	/* Change mode to match the image when extracting. */
	int de_chmode;
	/* Change UID/GID to match the image when extracting. */
	int de_chownership;
	/* Change a/m time to match the image when extracting. */
	int de_chutime;
};

static void usage(const char* const exe, FILE* const dest)
{
	fprintf(dest,
		"\nUsage: %s [-%s] imgfile\n"
		"\nexample: %s boot.img\n\n"
		" -h          print this message (to stdout) and quit\n"
		" -v          be verbose\n"
		" -q          only check the superblock and the CRC32 checksum\n"
		" -e          turn warnings into errors\n"
		" -M          do NOT set the mode to match the image when extracting\n"
		" -O          do NOT set the ownership to match the image when extracting\n"
		" -U          do NOT set the utime to match the image when extracting\n"
		" -x <str>    directory to extract the image content to\n"
		" imgfile     image file to check\n"
		"\n", exe, CKI_OPTIONS, exe);
	
	exit(dest == stderr? EXIT_FAILURE: EXIT_SUCCESS);
}

static struct imgdesc* create_imgdesc(int argc, char* argv[])
{
	if (argc < 2)
		usage(argc > 0? argv[0]: "", stderr);
	
	struct imgdesc* desc = malloc(sizeof(*desc));
	if (!desc)
		error("failed to allocate an image desc");
	memset(desc, 0, sizeof(*desc));
	
	/* Set default values which are not zeroed.
	 */
	desc->de_chmode = 1;
	desc->de_chownership = 1;
	desc->de_chutime = 1;
	
	int option;
	while ((option = getopt(argc, argv, CKI_OPTIONS)) != EOF) {
		switch (option) {
			case 'h':
				usage(argv[0], stdout);
				break;
			case 'v':
				hostprog_verbosity++;
				break;
			case 'q':
				desc->de_quickie = 1;
				break;
			case 'e':
				hostprog_werror = 1;
				break;
			case 'M':
				desc->de_chmode = 0;
				break;
			case 'O':
				desc->de_chownership = 0;
				break;
			case 'U':
				desc->de_chutime = 0;
				break;
			case 'x':
				desc->de_extractdir = optarg;
				desc->de_extractdirlen = strlen(optarg);
				break;
			default:
				/* Ignore it.
				 */
				warning("unrecognized option -%c", option);
				break;
		}
	}
	
	if ((argc - optind) != 1)
		usage(argv[0], stderr);
	desc->de_infile = argv[optind];
	
	if (desc->de_quickie && desc->de_extractdir)
		warning("-q and -x can not coexist, -q will take priority");
	
	struct stat st;
	if (stat(desc->de_infile, &st) < 0)
		error("failed to stat \"%s\": %s", desc->de_infile,
			strerror(errno));
	
	if (S_ISBLK(st.st_mode)) {
		if (ioctl(desc->de_fd, BLKGETSIZE64, &desc->de_outersz) < 0) {
			error("failed get size in number of blocks for \"%s\": %s",
				desc->de_infile, strerror(errno));
		}
		desc->de_outersz *= 512;
	} else if (S_ISREG(st.st_mode)) {
		desc->de_outersz = st.st_size;
	} else {
		error("not a block dev or regular file \"%s\"", desc->de_infile);
	}
	
	if (desc->de_outersz < MICROFS_MAXBLKSZ)
		error("the given file/dev is too small to contain a microfs image");
	
	desc->de_fd = open(desc->de_infile, O_RDONLY);
	if (desc->de_fd < 0)
		error("failed to open \"%s\": %s", desc->de_infile,
			strerror(errno));
	
	return desc;
}

static void ck_sb(struct imgdesc* const desc)
{
	uoff_t padding = 0;
	
sb_retry:
	desc->de_sb = (struct microfs_sb*)(desc->de_image + padding);
	desc->de_sb->s_magic = __le32_to_cpu(desc->de_sb->s_magic);
	if (desc->de_sb->s_magic != MICROFS_MAGIC) {
		if (padding == MICROFS_PADDING)
			error("could not find the superblock");
		padding = MICROFS_PADDING;
		goto sb_retry;
	}
	
	if (memcmp(desc->de_sb->s_signature, MICROFS_SIGNATURE,
			sizeof(desc->de_sb->s_signature)) != 0) {
		error("bad superblock signature");
	}
	
	if (__le32_to_cpu(desc->de_sb->s_size) > desc->de_outersz) {
		error("superblock size > image outer size (s_size=%ul, de_outersz=%zu)",
			__le32_to_cpu(desc->de_sb->s_size), desc->de_outersz);
	}
	
	if (sb_unsupportedflags(desc->de_sb)) {
		warning("unsupported superblock flags detected");
		warning("forcing a quick check");
		desc->de_quickie = 1;
	}
	
	const uoff_t expected_root_offset = padding + sizeof(*desc->de_sb);
	const uoff_t actual_root_offset = __le32_to_cpu(desc->de_sb->s_root.i_offset);
	
	const int invalid_offset = (
		actual_root_offset != 0 &&
		actual_root_offset != expected_root_offset
	);
	
	if (invalid_offset) {
		error("invalid root offset value");
	}
	
	desc->de_blkshift = __le16_to_cpu(desc->de_sb->s_blkshift);
	desc->de_blksz = 1 << desc->de_blkshift;
	
	const int invalid_blkshift = (
		desc->de_blkshift < MICROFS_MINBLKSZ_SHIFT ||
		desc->de_blkshift > MICROFS_MAXBLKSZ_SHIFT
	);
	
	if (invalid_blkshift)
		error("invalid block shift: %zu", desc->de_blkshift);
	
	desc->de_decompressionbufsz = compressBound(desc->de_blksz);
	desc->de_decompressionbuf = malloc(desc->de_decompressionbufsz);
	if (!desc->de_decompressionbuf)
		error("failed to allocate the decompression buffer");
	
	int zlib_err = inflateInit(&desc->de_zstream);
	if (zlib_err != Z_OK)
		error("failed to init the zlib stream: %s", zError(zlib_err));
}

static void ck_crc(struct imgdesc* const desc)
{
	uoff_t padding = (unsigned char*)desc->de_sb - desc->de_image;
	
	__u32 sb_crc = __le32_to_cpu(desc->de_sb->s_crc);
	__u32 host_crc = crc32(0L, Z_NULL, 0);
	
	desc->de_sb->s_crc = 0;
	
	host_crc = crc32(host_crc, (Bytef*)desc->de_image + padding,
		desc->de_outersz - padding);
	
	if (sb_crc != host_crc) {
		error("CRC mismatch (sb_crc=%x, host_crc=%x)",
			sb_crc, host_crc);
	}
	message(VERBOSITY_0, "CRC: %x", sb_crc);
}

static void ck_compression(struct imgdesc* const desc,
	const struct microfs_inode* const inode, const uoff_t inode_offset,
	char* inode_data, size_t inode_sz)
{
	/* The offset can still be invalid, but it is difficult to
	 * tell untill we try to uncompress the file data.
	 */
	size_t blk_nr = 0;
	size_t blk_ptrs = i_blkptrs(inode_sz, desc->de_blksz);
	size_t blk_ptr_length = MICROFS_IOFFSET_WIDTH / 8;
	size_t blk_data_length = 0;
	
	size_t checked;
	size_t unchecked = inode_sz;
	
	uoff_t inode_data_offset = 0;
	uoff_t blk_ptr_offset = __le32_to_cpu(inode->i_offset);
	uoff_t blk_data_offset = __le32_to_cpu(inode->i_offset)
		+ blk_ptrs * blk_ptr_length;
	
	do {
		checked = 0;
		blk_data_length = __le32_to_cpu(*(__le32*)(desc->de_image
			+ blk_ptr_offset)) - blk_data_offset;
		
		if (blk_data_length > desc->de_decompressionbufsz) {
			error("the block data length is too big:"
				" unchecked=%zu, blk_nr=%zu, blk_ptrs=%zu,"
				" blk_data_length=%zu, de_decompressionbufsz=%zu,"
				" blk_ptr_offset=0x%x, blk_data_offset=0x%x,"
				" inode_data_offset=0x%x, inode_offset=0x%x,"
				" inode_sz=%zu",
				unchecked, blk_nr, blk_ptrs,
				blk_data_length, desc->de_decompressionbufsz,
				(__u32)blk_ptr_offset, (__u32)blk_data_offset,
				(__u32)inode_data_offset, (__u32)inode_offset,
				inode_sz);
		} else if (blk_data_length == 0) {
			/* File hole.
			 */
			checked = unchecked > desc->de_blksz? desc->de_blksz: unchecked;
			if (inode_data) {
				memset(inode_data + inode_data_offset, 0, checked);
			}
			inode_data_offset += checked;
		} else {
			desc->de_zstream.next_in = desc->de_image + blk_data_offset;
			desc->de_zstream.avail_in = blk_data_length;
			
			desc->de_zstream.next_out = desc->de_decompressionbuf;
			desc->de_zstream.avail_out = desc->de_decompressionbufsz;
			
			int zlib_err;
			
			zlib_err = inflateReset(&desc->de_zstream);
			if (zlib_err != Z_OK)
				error("failed to reset the zlib stream: %s", zError(zlib_err));
			
			zlib_err = inflate(&desc->de_zstream, Z_FINISH);
			if (zlib_err != Z_STREAM_END)
				error("decompression error: %s", zError(zlib_err));
			
			if (inode_data) {
				memcpy(inode_data + inode_data_offset,
					desc->de_decompressionbuf, desc->de_zstream.total_out);
			}
			
			checked = desc->de_zstream.total_out;
			blk_data_offset += blk_data_length;
			inode_data_offset += desc->de_zstream.total_out;
		}
		
		blk_nr += 1;
		blk_ptr_offset += blk_ptr_length;
		
		if (checked > unchecked) {
			error("too much data checked, something is wrong:"
				"unchecked=%zu, checked=%zu, blk_data_offset=0x%x"
				"inode_data_offset=0x%x, inode_offset=0x%x",
				unchecked, checked, (__u32)blk_data_offset,
				(__u32)inode_data_offset, (__u32)inode_offset);
		} else
			unchecked -= checked;
		
	} while (unchecked);
}

static void ck_file(struct imgdesc* const desc,
	const struct microfs_inode* const inode, const uoff_t offset,
	const uoff_t next, const struct hostprog_path* const path)
{
	char* i_dest = NULL;
	size_t i_size = i_getsize(inode);
	if (i_size == 0)
		error("zero file size for file \"%s\" at 0x%x",
			path->p_path + desc->de_extractdirlen, (__u32)offset);
	
	uoff_t i_offset = __le32_to_cpu(inode->i_offset);
	if (i_offset < offset + next)
		error("invalid offset for file \"%s\" at 0x%x",
			path->p_path + desc->de_extractdirlen, (__u32)offset);
	
	int i_fd = 0;
	
	if (desc->de_extractdir) {
		int flags = O_RDWR | O_CREAT | O_TRUNC;
		int mode = __le16_to_cpu(inode->i_mode);
		i_fd = open(path->p_path, flags, mode);
		if (i_fd < 0) {
			error("failed to open file \"%s\" from 0x%x: %s",
				path->p_path, (__u32)offset, strerror(errno));
		}
		if (ftruncate(i_fd, i_size) < 0) {
			error("failed to set file size for \"%s\" from 0x%x: %s",
				path->p_path, (__u32)offset, strerror(errno));
		}
		i_dest = mmap(NULL, i_size, PROT_READ | PROT_WRITE,
			MAP_SHARED, i_fd, 0);
		if (i_dest == MAP_FAILED) {
			error("failed to mmap file \"%s\" from 0x%x: %s",
				path->p_path, (__u32)offset, strerror(errno));
		}
	}
	
	ck_compression(desc, inode, offset, i_dest, i_size);
	
	if (desc->de_extractdir) {
		munmap(i_dest, i_size);
		close(i_fd);
	}
}

static void ck_symlink(struct imgdesc* const desc,
	const struct microfs_inode* const inode, const uoff_t offset,
	const uoff_t next, const struct hostprog_path* const path)
{
	char* i_dest = NULL;
	size_t i_size = i_getsize(inode);
	if (i_size == 0)
		error("zero file size for symlink \"%s\" at 0x%x",
			path->p_path + desc->de_extractdirlen, (__u32)offset);
	
	uoff_t i_offset = __le32_to_cpu(inode->i_offset);
	if (i_offset < offset + next)
		error("invalid offset for symlink \"%s\" at 0x%x",
			path->p_path + desc->de_extractdirlen, (__u32)offset);
	
	if (desc->de_extractdir) {
		i_dest = malloc(i_size + 1);
		if (!i_dest)
			error("failed to allocate the symlink buffer");
	}
	
	ck_compression(desc, inode, offset, i_dest, i_size);
	
	if (desc->de_extractdir) {
		i_dest[i_size] = '\0';
		if (symlink(i_dest, path->p_path) < 0) {
			error("failed to symlink \"%s\" to \"%s\" from 0x%x",
				path->p_path, i_dest, (__u32)offset);
		}
		free(i_dest);
	}
}

static void ck_nod(struct imgdesc* const desc,
	const struct microfs_inode* const inode, const uoff_t offset,
	const struct hostprog_path* const path)
{
	dev_t dev = 0;
	mode_t mode = __le16_to_cpu(inode->i_mode);
	
	if (inode->i_offset) {
		error("special file \"%s\" has non-zero offset at 0x%x",
			path->p_path + desc->de_extractdirlen, (__u32)offset);
	}
	
	if (S_ISCHR(mode) || S_ISBLK(mode)) {
		dev = i_getsize(inode);
	} else if (S_ISFIFO(mode)) {
		if (i_getsize(inode)) {
			error("fifo \"%s\" has non-zero size at 0x%x",
				path->p_path + desc->de_extractdirlen, (__u32)offset);
		}
	} else if (S_ISSOCK(mode)) {
		if (i_getsize(inode)) {
			error("socket \"%s\" has non-zero size at 0x%x",
				path->p_path + desc->de_extractdirlen, (__u32)offset);
		}
	} else {
		error("unsupported mode for \"%s\" at 0x%x",
			path->p_path + desc->de_extractdirlen, (__u32)offset);
	}
	
	if (desc->de_extractdir) {
		if (S_ISFIFO(mode)) {
			if (mkfifo(path->p_path, mode) < 0) {
				error("failed to create fifo \"%s\" from 0x%x: %s",
					path->p_path, (__u32)offset, strerror(errno));
			}
		} else {
			if (mknod(path->p_path, mode, dev) < 0) {
				error("failed to create special inode \"%s\" from 0x%x: %s",
					path->p_path, (__u32)offset, strerror(errno));
			}
		}
	}
}

static void ck_name(const char* const name, const size_t namelen,
	const uoff_t offset)
{
	for (size_t i = 0; i < namelen; i++) {
		if (name[i] == '\0')
			warning("dentry name contains NULLS ('\\0') at 0x%x",
				(__u32)offset);
	}
}

static void ck_metadata(struct imgdesc* const desc,
	const struct microfs_inode* const inode, const uoff_t offset,
	const struct hostprog_path* const path)
{
#define CK_ZERO(Value) \
	if ((Value) == 0) { \
		error("\"%s\" is zero for \"%s\" at 0x%x", #Value, \
			path->p_path + desc->de_extractdirlen, (__u32)offset); \
	}
	
	CK_ZERO(inode->i_mode);
	CK_ZERO(inode->i_namelen);
	
#undef CK_ZERO
	
	if (desc->de_extractdir) {
		__u16 uid = __le16_to_cpu(inode->i_uid);
		__u16 gid = __le16_to_cpu(inode->i_gid);
		__u16 mode = __le16_to_cpu(inode->i_mode);
		
		if (desc->de_chownership && lchown(path->p_path, uid, gid) < 0) {
			error("failed to change ownership for \"%s\" (origin 0x%x): %s",
				path->p_path, (__u32)offset, strerror(errno));
		}
		
		if (!S_ISLNK(mode)) {
			if (desc->de_chmode && chmod(path->p_path, mode) < 0) {
				error("failed to change mode for \"%s\" (origin 0x%x): %s",
					path->p_path, (__u32)offset, strerror(errno));
			}
			
			__u32 ctime = __le32_to_cpu(desc->de_sb->s_ctime);
			struct utimbuf thenish = {
				.actime = ctime,
				.modtime = ctime
			};
			if (desc->de_chutime && utime(path->p_path, &thenish) < 0) {
				error("failed set access and modification times for \"%s\""
					" (origin 0x%x): %s", path->p_path, (__u32)offset,
					strerror(errno));
			}
		}
	}
}

static void ck_dir(struct imgdesc* const desc,
	const struct microfs_inode* const inode,
	struct hostprog_path* const path)
{
	char* namebuf = malloc(MICROFS_MAXNAMELEN + 1);
	if (!namebuf)
		error("failed to allocate the name buffer");
	
	uoff_t offset = __le32_to_cpu(inode->i_offset);
	uoff_t dir_offset = 0;
	size_t dir_size = i_getsize(inode);
	size_t dir_lvl = hostprog_path_lvls(path);
	
	while (dir_offset < dir_size) {
		struct microfs_inode* dentry = (struct microfs_inode*)
			(desc->de_image + offset);
		
		char* name = (char*)(dentry + 1);
		size_t namelen = dentry->i_namelen;
		
		memcpy(namebuf, name, namelen);
		namebuf[namelen] = '\0';
		name = namebuf;
		
		ck_name(name, namelen, offset);
		if (hostprog_path_append(path, name) < 0)
			error("failed to add a filename to the hostprog_path");
		
		mode_t mode = __le16_to_cpu(dentry->i_mode);
		uoff_t next = sizeof(*dentry) + namelen;
		
		message(VERBOSITY_1, " ck %c %s (0x%x)",
			nodtype(mode), path->p_path + desc->de_extractdirlen,
			(__u32)offset);
		
		if (S_ISREG(mode)) {
			ck_file(desc, dentry, offset, next, path);
		} else if (S_ISLNK(mode)) {
			ck_symlink(desc, dentry, offset, next, path);
		} else if (S_ISDIR(mode)) {
			if (desc->de_extractdir && mkdir(path->p_path, mode) < 0) {
				if (errno != EEXIST) {
					error("failed to create directory \"%s\": %s",
						path->p_path, strerror(errno));
				}
			}
			ck_dir(desc, dentry, path);
		} else {
			ck_nod(desc, dentry, offset, path);
		}
		
		ck_metadata(desc, dentry, offset, path);
		
		offset += next;
		dir_offset += next;
		
		hostprog_path_dirnamelvl(path, dir_lvl);
	}
	
	free(namebuf);
}

int main(int argc, char* argv[])
{
	struct imgdesc* desc = create_imgdesc(argc, argv);
	
	desc->de_image = mmap(NULL, desc->de_outersz, PROT_READ | PROT_WRITE,
		MAP_PRIVATE, desc->de_fd, 0);
	if (desc->de_image == MAP_FAILED)
		error("failed to mmap the image file: %s", strerror(errno));
	
	ck_sb(desc);
	ck_crc(desc);
	
	if (!desc->de_quickie) {
		struct hostprog_path* path = NULL;
		if (hostprog_path_create(&path, desc->de_extractdir,
				MICROFS_MAXNAMELEN, MICROFS_MAXNAMELEN) != 0) {
			error("failed to create the path for the extract dir: %s",
				strerror(errno));
		}
		if (path->p_pathlen && mkdir(path->p_path,
				__le16_to_cpu(desc->de_sb->s_root.i_mode)) == -1) {
			if (errno != EEXIST)
				error("failed to create the extract dir: %s", strerror(errno));
		}
		ck_dir(desc, &desc->de_sb->s_root, path);
	}
	
	if (munmap(desc->de_image, desc->de_outersz) < 0)
		error("failed to unmap the image: %s", strerror(errno));
	
	exit(EXIT_SUCCESS);
}
