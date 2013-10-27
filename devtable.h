/* microfs - Minimally Improved Compressed Read Only File System
 * Copyright (C) 2013 Erik Edlund <erik.edlund@32767.se>
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

#ifndef DEVTABLE_H
#define DEVTABLE_H

#ifndef HOSTPROGS_H
#error "#include hostprogs.h first"
#endif
#ifndef __MICROFS_H__
#error "#include microfs.h first"
#endif

struct devtable_entry {
	char dt_type;
	char* dt_path;
	size_t dt_pathlen;
	unsigned long dt_mode;
	unsigned long dt_uid;
	unsigned long dt_gid;
	unsigned long dt_dev_major;
	unsigned long dt_dev_minor;
	unsigned long dt_dev_start;
	unsigned long dt_dev_incr;
	unsigned long dt_dev_count;
};

struct devtable_dentry {
	char* de_path;
	size_t de_pathlen;
	dev_t de_dev;
	unsigned long de_mode;
	unsigned long de_uid;
	unsigned long de_gid;
};

/* Callback signature for %devtable_parse, do not try to %free
 * any of the pointers in the callback.
 */
typedef void (*devtable_process_dentry_t)(struct devtable_dentry* const ent,
	void* data, const char* file, const char* line, const size_t linenumber);

static inline void devtable_interpret_entry(devtable_process_dentry_t callback,
	void* data, const char* path, const char* line, const size_t linenumber,
	const int devbits)
{
	struct devtable_entry devt_ent = {
		.dt_type = '?',
		.dt_path = NULL,
		.dt_pathlen = 0,
		.dt_mode = 0755,
		.dt_uid = 0,
		.dt_gid = 0,
		.dt_dev_major = 0,
		.dt_dev_minor = 0,
		.dt_dev_start = 0,
		.dt_dev_incr = 0,
		.dt_dev_count = 0
	};
	
/* Mute the warning about the "m"-modifier not being part
 * of the C11 standard.
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
	if (sscanf(line, "%ms %c %lo %lu %lu %lu %lu %lu %lu %lu",
			&devt_ent.dt_path, &devt_ent.dt_type, &devt_ent.dt_mode,
			&devt_ent.dt_uid, &devt_ent.dt_gid,
			&devt_ent.dt_dev_major, &devt_ent.dt_dev_minor,
			&devt_ent.dt_dev_start, &devt_ent.dt_dev_incr,
			&devt_ent.dt_dev_count) < 0) {
		error("failed to read the device table entry \"%s\" at line %zu: %s",
			line, linenumber, strerror(errno));
	}
#pragma GCC diagnostic pop
	
	devt_ent.dt_pathlen = strlen(devt_ent.dt_path);
	if (devt_ent.dt_pathlen == 0) {
		error("entry path not properly parsed, giving up at line %zu",
			linenumber);
	} else if (devt_ent.dt_path[0] != '/') {
		error("device table entries require absolute paths,"
			" \"%s\" is not at line %zu", devt_ent.dt_path, linenumber);
	}
	
	switch (devt_ent.dt_type) {
		case 'd':
			devt_ent.dt_mode |= S_IFDIR;
			break;
		case 'f':
			devt_ent.dt_mode |= S_IFREG;
			break;
		case 'p':
			devt_ent.dt_mode |= S_IFIFO;
			break;
		case 'c':
			devt_ent.dt_mode |= S_IFCHR;
			break;
		case 'b':
			devt_ent.dt_mode |= S_IFBLK;
			break;
		default:
			error("unsupported file type '%c' at line %zu",
				devt_ent.dt_type, linenumber);
	}
	
	struct devtable_dentry devt_dent = {
		.de_mode = devt_ent.dt_mode,
		.de_uid = devt_ent.dt_uid,
		.de_gid = devt_ent.dt_gid
	};
	
	if (devt_ent.dt_dev_count > 0) {
		for (unsigned long i = devt_ent.dt_dev_start; i < devt_ent.dt_dev_count; i++) {
			if (asprintf(&devt_dent.de_path, "%s%lu", devt_ent.dt_path, i) < 0)
				error("failed to format the device path: %s", strerror(errno));
			devt_dent.de_pathlen = strlen(devt_dent.de_path);
			devt_dent.de_dev = makedev_lim(devt_ent.dt_dev_major,
				minor_n(devt_ent.dt_dev_major, devt_ent.dt_dev_start,
				devt_ent.dt_dev_incr, i), devbits);
			
			callback(&devt_dent, data, path, line, linenumber);
			
			free(devt_dent.de_path);
		}
	} else {
		devt_dent.de_path = devt_ent.dt_path;
		devt_dent.de_pathlen = devt_ent.dt_pathlen;
		devt_dent.de_dev = makedev_lim(devt_ent.dt_dev_major, devt_ent.dt_dev_minor,
			devbits);
		
		callback(&devt_dent, data, path, line, linenumber);
	}
	
	free(devt_ent.dt_path);
}

static inline void devtable_parse(devtable_process_dentry_t callback,
	void* data, const char* path, const int devbits)
{
	FILE* devtable = fopen(path, "r");
	if (!devtable) {
		error("failed to open device table \"%s\": %s",
			path, strerror(errno));
	}
	
	char* line = NULL;
	size_t length = 0;
	size_t leading = 0;
	size_t linenumber = 1;
	
	while (getline(&line, &length, devtable) != -1) {
		length = strlen(line);
		
		/* First trim trailing, then trim leading whitespace.
		 */
		while (length > 0 && isspace(line[length - 1]))
			line[--length] = '\0';
		leading = strspn(line, " \n\r\t\v");
		length -= leading;
		memmove(line, line + leading, length);
		
		if (length && *line != '#') {
			devtable_interpret_entry(callback, data, path,
				line, linenumber++, devbits);
		}
		
		free(line);
		line = NULL;
		length = 0;
	}
}

static inline int devtable_writable(const struct stat* const st)
{
	return (
		S_ISDIR(st->st_mode) ||
		S_ISCHR(st->st_mode) ||
		S_ISBLK(st->st_mode) ||
		S_ISFIFO(st->st_mode)
	);
}

static inline void devtable_write(FILE* const devtable, const int devbits,
	struct hostprog_path* const rootpath, struct hostprog_path* const dirpath,
	struct stat* const dirpathst)
{
	size_t rootpath_lvl = hostprog_path_lvls(rootpath);
	dev_t dev = makedev_lim(
		major(dirpathst->st_rdev),
		minor(dirpathst->st_rdev),
		devbits
	);
	char dev_start = '-';
	char dev_incr = '-';
	char dev_count = '-';
	
	if (hostprog_path_append(rootpath, dirpath->p_path) < 0)
		error("failed to append \"%s\" to the root path", dirpath->p_path);
	
	int err = fprintf(devtable,
		"%s\t"
		"%c\t"
		"%lo\t"
		"%lu\t"
		"%lu\t"
		"%d\t"
		"%d\t"
		"%c\t"
		"%c\t"
		"%c\n",
		rootpath->p_path,
		nodtype(dirpathst->st_mode),
		(unsigned long)dirpathst->st_mode,
		(unsigned long)dirpathst->st_uid,
		(unsigned long)dirpathst->st_gid,
		major(dev),
		minor(dev),
		dev_start,
		dev_incr,
		dev_count);
	
	if (err < 0) {
		error("failed to write a devtable row for \"%s\": %s",
			rootpath->p_path, strerror(errno));
	}
	hostprog_path_dirnamelvl(rootpath, rootpath_lvl);
}

#endif

