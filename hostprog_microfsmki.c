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

// microfs make image

#define _FILE_OFFSET_BITS 64

#include "hostprogs.h"
#include "microfs.h"

#include "dev.h"
#include "devtable.h"

/* getopt() args, see usage().
 */
#define MKI_OPTIONS "hvepqsZSb:u:n:c:D:"

/* Simple representation of an inode/dentry.
 */
struct entry {
	/* File name. */
	char* e_name;
	/* File path (only set for regular files and links). */
	char* e_path;
	/* File mode. */
	unsigned int e_mode;
	/* File size. */
	unsigned int e_size;
	/* File UID. */
	unsigned int e_uid;
	/* File GID. */
	unsigned int e_gid;
	/* File descriptor used when mapping the entry. */
	int e_fd;
	/* Uncompressed file data. */
	char* e_data;
	/* Offset of the inode. */
	uoff_t e_ioffset;
	/* Offset of the data. */
	uoff_t e_dataoffset;
	/* First child for an non-empty directory. */
	struct entry* e_firstchild;
	/* Next sibling in the directory that contains this entry. */
	struct entry* e_sibling;
	/* Linked list of other entries with the same file content. */
	struct entry* e_same;
};

/* Specification for the image.
 */
struct imgspec {
	/* Root dir. */
	const char* sp_rootdir;
	/* Output file. */
	const char* sp_outfile;
	/* Image name. */
	const char* sp_name;
	/* Device table file. */
	const char* sp_devtable;
	/* Buffer used for compressing blocks. */
	char* sp_compressionbuf;
	/* Size of sp_compressionbuf. */
	size_t sp_compressionbufsz;
	/* Image file descriptor when writing the file. */
	int sp_fd;
	/* Pad the image? */
	int sp_pad;
	/* Support file holes? */
	int sp_holes;
	/* Share data between duplicate files? */
	int sp_shareblocks;
	/* Include sockets when making an image. */
	int sp_incsocks;
	/* Make everything owned by root. */
	int sp_squashperms;
	/* zlib compression level. */
	int sp_compresslvl;
	/* Host page size. */
	size_t sp_pagesz;
	/* Left shift for the block size. */
	size_t sp_blkshift;
	/* Actual block size. */
	size_t sp_blksz;
	/* Number of files. */
	size_t sp_files;
	/* Estimated upper bound. */
	size_t sp_upperbound;
	/* User requested upper bound. */
	size_t sp_usrupperbound;
	/* Root entry. */
	struct entry* sp_root;
	/* Stack of all regular files, used to find duplicates. */
	struct hostprog_stack* sp_regstack;
};

/* Set the uid or gid for the given entry and check for
 * overflows.
 */
#define ENTRY_SET_XID(Imgspec, Entry, Dst, Src, Lim) \
	if (!(Imgspec)->sp_squashperms) { \
		(Entry)->Dst = Src; \
		if ((Entry)->Dst >= 1 << (Lim)) { \
			(Entry)->Dst = 0; \
			warning("\"" #Src "\" is too big to store in %d bits" \
				" - it has been set to 0 for \"%s\"", Lim, (Entry)->e_name); \
		} \
	}

/* Fail if the length of the given name is greater than
 * MICROFS_MAXNAMELEN.
 */
static size_t namelen(const char* const name)
{
	size_t namelen = strlen(name);
	if (namelen > MICROFS_MAXNAMELEN) {
		/* File names this long should be _very_ rare, and truncating
		 * too long file names seems like a bad idea (even though
		 * mkcramfs does it).
		 */
		error("file name \"%s\" is too long, it is %zu bytes and"
			" %llu bytes is the maximum length", name,
			namelen, MICROFS_MAXNAMELEN);
	}
	return namelen;
}

/* Comparison callback for %qsort().
 */
static int entryszcmp(const void* ent1, const void* ent2)
{
	return ((const struct entry*)ent1)->e_size
		- ((const struct entry*)ent2)->e_size;
}

/* Update the upperbound image size for the given spec and
 * get the size of the of the inode and its name in return,
 * which is handy for updating the size of a directory entry.
 */
static size_t update_upperbound(struct imgspec* const spec,
	struct entry* const ent, size_t namelen)
{
	const size_t inodesz = sizeof(struct microfs_inode) + namelen;
	spec->sp_upperbound += inodesz;
	if ((S_ISREG(ent->e_mode) || S_ISLNK(ent->e_mode)) && ent->e_size) {
		/* The size of a compressed file can never get bigger than
		 * it would be if all its blocks would compress to their
		 * worst-case sizes (as dictated by zlib). (Most likely this
		 * will rarely happen "naturally", but sometimes it is okay
		 * to be a pessimist.)
		 */
		size_t blks = i_blkptrs(ent->e_size, spec->sp_blksz);
		spec->sp_upperbound += (MICROFS_IOFFSET_WIDTH / 8) * blks
			+ compressBound(spec->sp_blksz) * (blks - 1)
			+ compressBound(ent->e_size % spec->sp_blksz);
	}
	
	if (++spec->sp_files > MICROFS_MAXFILES)
		error("too many files, the upper limit is %llu", MICROFS_MAXFILES);
	
	return inodesz;
}

static unsigned int walk_directory(struct imgspec* const spec,
	struct hostprog_path* const path, struct entry** previous)
{
	struct dirent** dirlst;
	int dirlst_count;
	int dirlst_index;
	
	size_t dir_sz = 0;
	size_t dir_lvl = hostprog_path_lvls(path);
	
	dirlst_count = scandir(path->p_path, &dirlst, NULL, hostprog_scandirsort);
	if (dirlst_count < 0)
		error("failed to read \"%s\": %s", path->p_path, strerror(errno));
	
	for (dirlst_index = 0; dirlst_index < dirlst_count; dirlst_index++) {
		struct dirent* dent = dirlst[dirlst_index];
		
		/* Skip "." and ".." directories, just like mkcramfs.
		 */
		if (hostprog_path_dotdir(dent->d_name))
			continue;
		
		hostprog_path_dirnamelvl(path, dir_lvl);
		if (hostprog_path_append(path, dent->d_name) != 0)
			error("failed to add a filename to the hostprog_path");
		
		struct stat st;
		if (lstat(path->p_path, &st) < 0) {
			/* Maybe this should be an error? Files missing in the image
			 * could possibly seem like an error to the user.
			 */
			warning("skipping \"unlstatable\" file \"%s\": %s",
				path->p_path, strerror(errno));
			continue;
		}
		
		if (S_ISREG(st.st_mode) || S_ISLNK(st.st_mode)) {
			/* If the file is a regular file which can not be read, then
			 * it might as well be skipped.
			 * 
			 * This should also possibly be an error.
			 */
			if (access(path->p_path, R_OK) < 0) {
				warning("skipping unreadable file \"%s\": %s",
					path->p_path, strerror(errno));
				continue;
			}
			/* Completely empty files seems pretty pointless to include
			 * in the image.
			 */
			if (!st.st_size) {
				warning("skipping empty file \"%s\"", path->p_path);
				continue;
			}
		} else if (!spec->sp_incsocks && S_ISSOCK(st.st_mode)) {
			warning("skipping socket \"%s\"", path->p_path);
			continue;
		}
		
		/* Files should never be skipped after this point since that
		 * would mess up the image size estimation.
		 */
		
		struct entry* ent = malloc(sizeof(*ent));
		if (!ent)
			error("failed to alloc an entry for \"%s\"", path->p_path);
		
		memset(ent, 0, sizeof(*ent));
		
		ent->e_name = strdup(dent->d_name);
		if (!ent->e_name)
			error("failed to copy the entry name for \"%s\"", path->p_path);
		ent->e_mode = st.st_mode;
		ent->e_size = st.st_size;
		ent->e_fd = -1;
		
		ENTRY_SET_XID(spec, ent, e_uid, st.st_uid, MICROFS_IUID_WIDTH);
		ENTRY_SET_XID(spec, ent, e_gid, st.st_gid, MICROFS_IGID_WIDTH);
		
		if (S_ISDIR(ent->e_mode)) {
			ent->e_size = walk_directory(spec, path, &ent->e_firstchild);
		} else if (S_ISREG(ent->e_mode) || S_ISLNK(ent->e_mode)) {
			if (ent->e_size > MICROFS_MAXFILESIZE) {
				error("\"%s\" is too big, max file size is %llu bytes",
					path->p_path, MICROFS_MAXFILESIZE);
			}
			ent->e_path = strdup(path->p_path);
			if (!ent->e_path) {
				error("failed to copy the entry path for \"%s\"",
					path->p_path);
			}
			if (spec->sp_shareblocks && S_ISREG(ent->e_mode)) {
				if (hostprog_stack_push(spec->sp_regstack, ent) < 0)
					error("failed to push an entry to the regular file stack: %s",
						strerror(errno));
			}
		} else if (S_ISCHR(ent->e_mode) || S_ISBLK(ent->e_mode)) {
			ent->e_size = makedev_lim(major(st.st_rdev), minor(st.st_rdev),
				MICROFS_ISIZEX_WIDTH);
		} else if (S_ISFIFO(st.st_mode) || S_ISSOCK(st.st_mode)) {
			ent->e_size = 0;
		} else {
			error("unexpected file mode encountered");
		}
		
		/* %namelen() could actually fail here if the d_name is too
		 * long, but that will terminate the program, so that is fine.
		 */
		dir_sz += update_upperbound(spec, ent, namelen(dent->d_name));
		
		message(VERBOSITY_1, "+ %c %s", nodtype(ent->e_mode), path->p_path);
		
		*previous = ent;
		previous = &ent->e_sibling;
	}
	
	free(dirlst);
	hostprog_path_dirnamelvl(path, dir_lvl);
	
	/* This should never happen, but if it ever does, it is
	 * clearly an error.
	 */
	if (dir_sz > MICROFS_MAXDIRSIZE) {
		error("achievement unlocked: the directory size for \"%s\""
			" is %zu bytes, the maximum supported size is %llu bytes"
			" - this is impressive in a very scary way",
			path->p_path, dir_sz, MICROFS_MAXDIRSIZE);
	}
	
	return dir_sz;
}

/* Return zero if the first %len bytes from %begin are all
 * NULLs.
 */
static inline int is_zero(const char* const begin, size_t len)
{
	return (len-- == 0 ||
		(begin[0] == '\0' &&
		 (len-- == 0 ||
		  (begin[1] == '\0' &&
		   (len-- == 0 ||
		    (begin[2] == '\0' &&
		     (len-- == 0 ||
		      (begin[3] == '\0' &&
		       memcmp(begin, begin + 4, len) == 0))))))));
}

static inline void set_dataoffset(struct entry* const ent, char* base,
	const uoff_t offset)
{
	struct entry* duplicate = ent->e_same;
	struct microfs_inode* inode = (struct microfs_inode*)(base + ent->e_ioffset);
	ent->e_dataoffset = offset;
	inode->i_offset = __cpu_to_le32(offset);
	while (duplicate) {
		inode = (struct microfs_inode*)(base + duplicate->e_ioffset);
		duplicate->e_dataoffset = offset;
		inode->i_offset = __cpu_to_le32(offset);
		duplicate = duplicate->e_same;
	}
}

static inline uoff_t superblock_offset(const struct imgspec* const spec)
{
	return spec->sp_pad? MICROFS_PADDING: 0;
}

static void write_superblock(struct imgspec* const spec, char* base,
	const size_t sz)
{
	uoff_t padding = superblock_offset(spec);
	uoff_t offset = padding + sizeof(struct microfs_sb);
	struct microfs_sb* sb = (struct microfs_sb*)(base + padding);
	
	sb->s_magic = __cpu_to_le32(MICROFS_MAGIC);
	sb->s_size = sz == MICROFS_MAXIMGSIZE? 0: __cpu_to_le32(sz);
	sb->s_crc = 0;
	sb->s_blocks = __cpu_to_le32((sz - 1) / spec->sp_blksz + 1);
	sb->s_files = __cpu_to_le16(spec->sp_files);
	sb->s_blkshift = __cpu_to_le16(spec->sp_blkshift);
	
	if (sb->s_size == 0) {
		warning("this image is exactly %llu bytes (as big as is possible),"
			" this special case is not well tested", MICROFS_MAXIMGSIZE);
	}
	
	struct timespec nowish;
	if (clock_gettime(CLOCK_REALTIME, &nowish) < 0) {
		error("failed to get the current time: %s", strerror(errno));
	}
	sb->s_ctime = __cpu_to_le32(nowish.tv_sec);
	
	__u32 flags = 0;
	if (spec->sp_holes)
		flags |= MICROFS_FLAG_HOLES;
	
	sb->s_flags = __cpu_to_le32(flags);
	
	memcpy(sb->s_signature, MICROFS_SIGNATURE, sizeof(sb->s_signature));
	memcpy(sb->s_name, spec->sp_name, sizeof(sb->s_name));
	
	sb->s_root.i_mode = __cpu_to_le16(spec->sp_root->e_mode);
	sb->s_root.i_uid = __cpu_to_le16(spec->sp_root->e_uid);
	sb->s_root.i_gid = __cpu_to_le16(spec->sp_root->e_gid);
	i_setsize(&sb->s_root, spec->sp_root->e_size);
	sb->s_root.i_offset = spec->sp_root->e_firstchild?
		__cpu_to_le32(offset): 0;
	
	/* With everything in place it is possible to calculate the
	 * crc32 checksum for the image.
	 */
	__u32 crc = crc32(0L, Z_NULL, 0);
	crc = crc32(crc, (Bytef*)base + padding, sz - padding);
	sb->s_crc = __cpu_to_le32(crc);
	
	message(VERBOSITY_0, "CRC: %x", crc);
}

/* Write metadata for the given entries, but not their actual
 * data (see write_data() for that).
 */
static uoff_t write_metadata(struct imgspec* const spec,
	char* base, uoff_t offset)
{
	struct hostprog_stack* metastack = NULL;
	if (hostprog_stack_create(&metastack, 64, 64))
		error("failed to create the meta stack: %s", strerror(errno));
	
	struct entry* ent = spec->sp_root->e_firstchild;
	
	/* The maximum possible size of the metadata is
	 *     maxsz = (sizeof(struct microfs_inode) + MICROFS_MAXNAMELEN)
	 *         * MICROFS_MAXFILES;
	 * which is approx 17MB.
	 */
	
	for (;;) {
		size_t dirstart = metastack->st_index;
		while (ent) {
			ent->e_ioffset = offset;
			struct microfs_inode* inode = (struct microfs_inode*)(base + offset);
			inode->i_mode = __cpu_to_le16(ent->e_mode);
			inode->i_uid = __cpu_to_le16(ent->e_uid);
			inode->i_gid = __cpu_to_le16(ent->e_gid);
			i_setsize(inode, ent->e_size);
			inode->i_namelen = strlen(ent->e_name);
			inode->i_offset = 0;
			
			/* Regular files, symlinks and non-empty directories will
			 * overwrite the offset later.
			 */
			
			offset += sizeof(*inode);
			memcpy(base + offset, ent->e_name, inode->i_namelen);
			offset += inode->i_namelen;
			
			if (ent->e_firstchild) {
				if (hostprog_stack_push(metastack, ent) < 0)
					error("failed to push an entry to the meta stack: %s",
						strerror(errno));
			}
			ent = ent->e_sibling;
		}
		
		if (!metastack->st_index)
			break;
		
		/* Reverse the order of the stack entries pushed for this
		 * directory.
		 */
		struct entry** lo = (struct entry**)metastack->st_slots + dirstart;
		struct entry** hi = (struct entry**)metastack->st_slots + metastack->st_index;
		while (lo < --hi) {
			struct entry* tmp = *lo;
			*lo++ = *hi;
			*hi = tmp;
		}
		if (hostprog_stack_pop(metastack, &ent) < 0)
			error("failed to pop an entry off the meta stack: %s",
				strerror(errno));
		
		set_dataoffset(ent, base, offset);
		ent = ent->e_firstchild;
	}
	hostprog_stack_destroy(metastack);
	
	return offset;
}

static void load_entry_data(struct entry* const ent)
{
	if (S_ISREG(ent->e_mode)) {
		ent->e_fd = open(ent->e_path, O_RDONLY);
		if (ent->e_fd < 0)
			error("failed to open \"%s\": %s", ent->e_path, strerror(errno));
		
		ent->e_data = mmap(NULL, ent->e_size, PROT_READ, MAP_PRIVATE, ent->e_fd, 0);
		if (ent->e_data == MAP_FAILED)
			error("failed to map \"%s\": %s", ent->e_path, strerror(errno));
		
	} else if (S_ISLNK(ent->e_mode)) {
		ent->e_data = malloc(ent->e_size);
		if (!ent->e_data)
			error("failed to allocate room for the data");
		
		if (readlink(ent->e_path, ent->e_data, ent->e_size) < 0) {
			error("failed read link \"%s\": %s",
				ent->e_path, strerror(errno));
		}
	} else {
		error("failed to load \"%s\": unexpected file mode '%c'",
			ent->e_path, nodtype(ent->e_mode));
	}
}

static void unload_entry_data(struct entry* const ent)
{
	if (S_ISREG(ent->e_mode)) {
		munmap(ent->e_data, ent->e_size);
		close(ent->e_fd);
		ent->e_fd = -1;
	} else if (S_ISLNK(ent->e_mode)) {
		free(ent->e_data);
	} else {
		error("well... this should be impossible:"
			" unexpected file mode '%c'", nodtype(ent->e_mode));
	}
	ent->e_data = NULL;
}

static uoff_t pack_data(struct imgspec* const spec, struct entry* ent,
	char* base)
{
	size_t ent_blks = i_blkptrs(ent->e_size, spec->sp_blksz); 
	size_t ent_sz = ent->e_size;
	
	char* ent_data = ent->e_data;
	
	const size_t blk_ptr_length = MICROFS_IOFFSET_WIDTH / 8;
	
	uoff_t blk_ptr_offset = ent->e_dataoffset;
	uoff_t blk_data_offset = ent->e_dataoffset + ent_blks * blk_ptr_length;
	
	const uoff_t orig_data_offset = blk_data_offset;
	
	do {
		size_t compr_sz = spec->sp_compressionbufsz;
		size_t compr_input = ent_sz > spec->sp_blksz? spec->sp_blksz: ent_sz;
		ent_sz -= compr_input;
		
		if (!(spec->sp_holes && is_zero(ent_data, compr_input))) {
			int err = compress2((Bytef*)spec->sp_compressionbuf, &compr_sz,
				(Bytef*)ent_data, compr_input, spec->sp_compresslvl);
			if (err != Z_OK) {
				error("compression failed for \"%s\": %s", ent->e_path,
					zError(err));
			}
			
			if (spec->sp_compresslvl != Z_NO_COMPRESSION && compr_sz >= compr_input) {
				message(VERBOSITY_2, ">>> data from offset %zu to %zu in \"%s\""
					" \"compressed\" from %zu bytes to %zu bytes",
					ent_data - ent->e_data, ent_data + compr_input - ent->e_data,
					ent->e_path, compr_input, compr_sz);
			}
			if (blk_data_offset + compr_sz > spec->sp_upperbound) {
				/* This can only happen if sp_upperbound was truncated to
				 * MICROFS_MAXIMGSIZE in create_imgspec(), otherwise it is
				 * guaranteed that the upper bound can fit all data even if
				 * we get the worst possible compression result for each block
				 * of data.
				 */
				error("out of space, the image can not hold more data");
			}
			
			memcpy(base + blk_data_offset, spec->sp_compressionbuf, compr_sz);
			blk_data_offset += compr_sz;
		}
		ent_data += compr_input;
		
		__le32* blkptr = (__le32*)(base + blk_ptr_offset);
		*blkptr = __cpu_to_le32(blk_data_offset);
		blk_ptr_offset += blk_ptr_length;
		
	} while (ent_sz);
	
	size_t newsz = blk_data_offset - orig_data_offset;
	int changesz = newsz - ent->e_size;
	message(VERBOSITY_1, "%6.2f%% (%+d bytes)\t\t%s",
		(changesz * 100) / (double)ent->e_size, changesz, ent->e_path);
	
	return blk_data_offset;
}

static uoff_t do_write_data(struct imgspec* const spec, struct entry* ent,
	char* base, uoff_t offset)
{
	do {
		if (ent->e_path && ent->e_dataoffset == 0) {
			set_dataoffset(ent, base, offset);
			load_entry_data(ent);
			offset = pack_data(spec, ent, base);
			unload_entry_data(ent);
		} else if (ent->e_firstchild) {
			offset = do_write_data(spec, ent->e_firstchild, base, offset);
		}
		ent = ent->e_sibling;
	} while (ent);
	return offset;
}

/* Write the actual data for the given entries, but not any
 * metadata (see write_metadata() for that).
 */
static uoff_t write_data(struct imgspec* const spec, char* base, uoff_t offset)
{
	return spec->sp_root->e_firstchild? do_write_data(spec,
		spec->sp_root->e_firstchild, base, offset): offset;
}

static void find_duplicates(struct imgspec* const spec)
{
	if (hostprog_stack_size(spec->sp_regstack) < 2)
		return;
	
	qsort(spec->sp_regstack->st_slots, hostprog_stack_size(spec->sp_regstack),
		sizeof(*spec->sp_regstack->st_slots), entryszcmp);
	
	for (int i = 0, j = 1; j < hostprog_stack_size(spec->sp_regstack);
			i++, j++) {
		struct entry* ent_i = (struct entry*)spec->sp_regstack->st_slots[i];
		struct entry* ent_j = (struct entry*)spec->sp_regstack->st_slots[j];
		if (ent_i->e_size == ent_j->e_size) {
			load_entry_data(ent_i);
			load_entry_data(ent_j);
			if (memcmp(ent_i->e_data, ent_j->e_data, ent_i->e_size) == 0) {
				message(VERBOSITY_1, "%s == %s", ent_i->e_path, ent_j->e_path);
				ent_i->e_same = ent_j;
			}
			unload_entry_data(ent_i);
			unload_entry_data(ent_j);
		}
	}
}

static struct entry* devtable_find_entry(struct entry* walker,
	const char* name, mode_t type)
{
	if (S_ISDIR(walker->e_mode))
		walker = walker->e_firstchild;
	
	while (walker) {
		/* Only check the name when the modes match.
		 */
		if (type == (walker->e_mode & S_IFMT) && walker->e_name) {
			if (S_ISDIR(walker->e_mode)) {
				/* Check if this is a parent of the path.
				 */
				size_t length = strlen(walker->e_name);
				if (strncmp(walker->e_name, name, length) == 0) {
					/* An exact match!
					 */
					if (strcmp(name, walker->e_name) == 0)
						break;
					/* Seems to be a parent of the path.
					 */
					if (name[length] == '/') {
						if (walker->e_firstchild) {
							walker = devtable_find_entry(walker, name + length + 1, type);
						} else {
							walker = NULL;
						}
						break;
					}
				}
			} else {
				if (strcmp(name, walker->e_name) == 0)
					break;
			}
		}
		walker = walker->e_sibling;
	}
	return walker;
}

static void devtable_modify_entry(struct imgspec* const spec,
	const char* path, unsigned long uid, unsigned long gid,
	mode_t mode, dev_t rdev)
{
	char* path_dir = strdup(path);
	char* path_name = strdup(path);
	
	if (!path_dir || !path_name)
		error("failed to duplicate the path: %s", strerror(errno));
	
	char* dir = dirname(path_dir);
	char* name = basename(path_name);
	
	struct entry* parent;
	struct entry* target;
	
	if (strcmp(dir, "/") == 0) {
		parent = spec->sp_root;
	} else if (!(parent = devtable_find_entry(spec->sp_root, dir + 1, S_IFDIR))) {
		error("could not find the parent for path \"%s\"", path);
	}
	
	if ((target = devtable_find_entry(parent, name, mode & S_IFMT))) {
		target->e_mode = mode;
		ENTRY_SET_XID(spec, target, e_uid, uid, MICROFS_IUID_WIDTH);
		ENTRY_SET_XID(spec, target, e_gid, gid, MICROFS_IGID_WIDTH);
		message(VERBOSITY_1, "(src:%s) %% %c %s", spec->sp_devtable,
			nodtype(target->e_mode), path);
	} else {
		target = malloc(sizeof(*target));
		if (!target)
			error("failed to alloc an entry for \"%s\"", path);
		
		memset(target, 0, sizeof(*target));
		
		if (S_ISREG(mode))
			error("regular file \"%s\" must exist on the disk", path);
		
		target->e_fd = -1;
		target->e_mode = mode;
		ENTRY_SET_XID(spec, target, e_uid, uid, MICROFS_IUID_WIDTH);
		ENTRY_SET_XID(spec, target, e_gid, gid, MICROFS_IGID_WIDTH);
		
		size_t targetnamelen = namelen(name);
		target->e_name = strdup(name);
		if (!target->e_name)
			error("failed to copy the entry name for \"%s\"", path);
		
		target->e_size = rdev;
		parent->e_size += update_upperbound(spec, target, targetnamelen);
		
		struct entry* prev = NULL;
		struct entry* curr = parent->e_firstchild;
		
		while (curr && strcmp(name, curr->e_name) > 0) {
			prev = curr;
			curr = curr->e_sibling;
		}
		if (!prev)
			parent->e_firstchild = target;
		else
			prev->e_sibling = target;
		
		target->e_sibling = curr;
		target->e_firstchild = NULL;
		
		message(VERBOSITY_1, "(src:%s) + %c %s", spec->sp_devtable,
			nodtype(target->e_mode), path);
	}
	
	free(path_name);
	free(path_dir);
}

/* Callback for %devtable_parse().
 */
static void devtable_process_dentry(struct devtable_dentry* const devt_dent,
	void* data, const char* file, const char* line, const size_t linenumber)
{
	(void)file;
	(void)line;
	(void)linenumber;
	
	struct imgspec* spec = (struct imgspec*)data;
	
	devtable_modify_entry(spec, devt_dent->de_path,
		devt_dent->de_uid, devt_dent->de_gid,
		devt_dent->de_mode, devt_dent->de_dev);
}

static void usage(const char* const exe, FILE* const dest)
{
	fprintf(dest,
		"\nUsage: %s [-%s] dirname outfile\n"
		"\nexample: %s /boot boot.img\n\n"
		" -h          print this message (to stdout) and quit\n"
		" -v          be more verbose\n"
		" -e          turn warnings into errors\n"
		" -p          pad by %d bytes to make room for boot code\n"
		" -q          squash permissions (make everything owned by root)\n"
		" -s          include sockets in the image\n"
		" -Z          do NOT use real file holes\n"
		" -S          do NOT eliminate regular file duplicates\n"
		" -b <int>    desired block size in bytes (power of two; min=%d, max=%d)\n"
		" -u <int>    artificial upper bound given in bytes\n"
		" -n <str>    give the image a name\n"
		" -c <str>    zlib compression level: none,default,speed,size\n"
		" -D <str>    use the given file as a device table\n"
		" dirname     root of the directory tree to be compressed\n"
		" outfile     image output file\n"
		"\n", exe, MKI_OPTIONS, exe, MICROFS_PADDING,
		MICROFS_MINBLKSZ, MICROFS_MAXBLKSZ);
	
	exit(dest == stderr? EXIT_FAILURE: EXIT_SUCCESS);
}

static struct imgspec* create_imgspec(int argc, char* argv[])
{
	if (argc < 2)
		usage(argc > 0? argv[0]: "", stderr);
	
	struct imgspec* spec = malloc(sizeof(*spec));
	if (!spec)
		error("failed to allocate an image spec");
	memset(spec, 0, sizeof(*spec));
	
	spec->sp_root = malloc(sizeof(*spec->sp_root));
	if (!spec->sp_root)
		error("failed to allocate the root entry");
	memset(spec->sp_root, 0, sizeof(*spec->sp_root));
	
	spec->sp_name = MICROFS_DEFAULTNAME;
	spec->sp_holes = 1;
	spec->sp_shareblocks = 1;
	spec->sp_compresslvl = Z_BEST_COMPRESSION;
	
	/* The page size of the host is a good default block size.
	 */
	spec->sp_pagesz = sysconf(_SC_PAGESIZE);
	spec->sp_blksz = spec->sp_pagesz;
	
	/* Check what the user want.
	 */
	
	int option;
	while ((option = getopt(argc, argv, MKI_OPTIONS)) != EOF) {
		size_t len;
		switch (option) {
			case 'h':
				usage(argv[0], stdout);
				break;
			case 'v':
				hostprog_verbosity++;
				break;
			case 'e':
				hostprog_werror = 1;
				break;
			case 'p':
				spec->sp_pad = 1;
				break;
			case 'q':
				spec->sp_squashperms = 1;
				break;
			case 's':
				spec->sp_incsocks = 1;
				break;
			case 'Z':
				spec->sp_holes = 0;
				break;
			case 'S':
				spec->sp_shareblocks = 0;
				break;
			case 'b':
				opt_strtolx(ul, option, optarg, spec->sp_blksz);
				if (!microfs_ispow2(spec->sp_blksz))
					error("the block size must be a power of two");
				if (spec->sp_blksz < MICROFS_MINBLKSZ ||
					spec->sp_blksz > MICROFS_MAXBLKSZ) {
					error("block size out of boundaries, %zu given;"
						" min=%d, max=%d", spec->sp_blksz,
						MICROFS_MINBLKSZ, MICROFS_MAXBLKSZ);
				}
				break;
			case 'u':
				opt_strtolx(ull, option, optarg, spec->sp_usrupperbound);
				break;
			case 'n':
				spec->sp_name = optarg;
				len = strlen(spec->sp_name);
				if (len > MICROFS_SBNAME_LENGTH) {
					warning("image name \"%s\" is too long"
						" it will be truncated from %zu to %d bytes",
						spec->sp_name, len, MICROFS_SBNAME_LENGTH);
				}
				break;
			case 'c':
#define Z_OPTARG(Spec, ArgValue, ZlibValue) \
	if (strcmp(optarg, ArgValue) == 0) { \
		(Spec)->sp_compresslvl = ZlibValue; \
		message(VERBOSITY_1, "Using compression: %s", optarg); \
	}
				
				Z_OPTARG(spec, "none", Z_NO_COMPRESSION);
				Z_OPTARG(spec, "default", Z_DEFAULT_COMPRESSION);
				Z_OPTARG(spec, "speed", Z_BEST_SPEED);
				Z_OPTARG(spec, "size", Z_BEST_COMPRESSION);
				
#undef Z_OPTARG
				break;
			case 'D':
				spec->sp_devtable = optarg;
				break;
			default:
				/* Ignore it.
				 */
				warning("unrecognized option -%c", option);
				break;
		}
	}
	
	/* The block size should now be correctly set, which means
	 * that the block left shift can be calculated.
	 */
	size_t blksz = spec->sp_blksz;
	while ((blksz >>= 1) > 0)
		spec->sp_blkshift++;
	
	if (spec->sp_usrupperbound % spec->sp_blksz != 0)
		error("upper bound must be a multiple of the block size");
	
	if (hostprog_stack_create(&spec->sp_regstack, 64, 64) < 0)
		error("failed to create the regular file stack");
	
	if (spec->sp_squashperms && spec->sp_devtable) {
		warning("both -q and -d are set, this might not be a good idea"
			" - the device table could override some or all permissions");
	}
	
	if ((argc - optind) != 2)
		usage(argv[0], stderr);
	spec->sp_rootdir = argv[optind + 0];
	spec->sp_outfile = argv[optind + 1];
	
	struct stat st;
	if (stat(spec->sp_rootdir, &st) == 0) {
		if (!S_ISDIR(st.st_mode))
			error("\"%s\" is not a directory", spec->sp_rootdir);
	} else
		error("can not stat \"%s\": %s", spec->sp_rootdir, strerror(errno));
	
	struct hostprog_path* path = NULL;
	if (hostprog_path_create(&path, spec->sp_rootdir,
			MICROFS_MAXNAMELEN,	MICROFS_MAXNAMELEN) != 0) {
		error("failed to create the path for the rootdir: %s", strerror(errno));
	}
	
	spec->sp_root->e_mode = st.st_mode;
	spec->sp_root->e_uid = st.st_uid;
	spec->sp_root->e_gid = st.st_gid;
	spec->sp_root->e_size = walk_directory(spec, path,
		&spec->sp_root->e_firstchild);
	
	hostprog_path_destroy(path);
	
	if (spec->sp_shareblocks)
		find_duplicates(spec);
	
	if (spec->sp_devtable)
		devtable_parse(devtable_process_dentry, spec,
			spec->sp_devtable, MICROFS_ISIZEX_WIDTH);
	
	if (spec->sp_usrupperbound && spec->sp_upperbound > spec->sp_usrupperbound) {
		warning("the estimated upper bound %zu is larger than the"
			" user requested upper bound %zu", spec->sp_upperbound,
			spec->sp_usrupperbound);
	}
	
	/* Allocate a max block size sized multiple of bytes.
	 */
	spec->sp_upperbound = sz_blkceil(spec->sp_upperbound,
		MICROFS_MAXBLKSZ);
	
	if (spec->sp_upperbound > MICROFS_MAXIMGSIZE) {
		warning("upper bound image size (absolute worst-case scenario)"
			" of %zu bytes is larger than the max image size of %llu bytes,"
			" there might not be room for everything", spec->sp_upperbound,
			MICROFS_MAXIMGSIZE);
		spec->sp_upperbound = MICROFS_MAXIMGSIZE;
	}
	
	int flags = O_RDWR | O_CREAT | O_TRUNC;
	spec->sp_fd = open(spec->sp_outfile, flags, 0666);
	if (spec->sp_fd < 0)
		error("failed to open \"%s\": %s", spec->sp_rootdir, strerror(errno));
	
	/* The worst case compression scenario will always fit in the
	 * buffer since the upper bound for a data size smaller than
	 * a block is always smaller than the upper bound for an entire
	 * block.
	 */
	spec->sp_compressionbufsz = compressBound(spec->sp_blksz);
	spec->sp_compressionbuf = malloc(spec->sp_compressionbufsz);
	if (!spec->sp_compressionbuf)
		error("failed to allocate the compression buffer");
	
	message(VERBOSITY_1, "Block size: %zu", spec->sp_blksz);
	message(VERBOSITY_1, "Block shift: %zu", spec->sp_blkshift);
	
	message(VERBOSITY_0, "Upper bound image size: %zu bytes", spec->sp_upperbound);
	message(VERBOSITY_0, "Number of files: %zu", spec->sp_files);
	
	return spec;
}

static void materialize_imgspec(struct imgspec* const spec)
{
	if (ftruncate(spec->sp_fd, spec->sp_upperbound) < 0)
		error("failed to truncate the image file: %s", strerror(errno));
	
	char* image = mmap(NULL, spec->sp_upperbound, PROT_READ | PROT_WRITE,
		MAP_SHARED, spec->sp_fd, 0);
	if (image == MAP_FAILED)
		error("failed to mmap the image file: %s", strerror(errno));
	
	uoff_t offset = superblock_offset(spec) + sizeof(struct microfs_sb);
	
	offset = write_metadata(spec, image, offset);
	offset = write_data(spec, image, offset);
	
	const size_t innersz = offset;
	const size_t outersz = sz_blkceil(offset, MICROFS_MAXBLKSZ);
	
	write_superblock(spec, image, outersz);
	
	if (munmap(image, spec->sp_upperbound) < 0)
		error("failed to unmap the image: %s", strerror(errno));
	
	if (ftruncate(spec->sp_fd, outersz) < 0)
		error("failed to set the final image size: %s", strerror(errno));
	
	message(VERBOSITY_1, "Inner image size: %zu bytes", innersz);
	message(VERBOSITY_0, "Outer image size: %zu bytes", outersz);
}

int main(int argc, char* argv[])
{
	struct imgspec* spec = create_imgspec(argc, argv);
	materialize_imgspec(spec);
	
	exit(EXIT_SUCCESS);
}
