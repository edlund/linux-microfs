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

#include "../hostprogs.h"
#include "../microfs.h"

#include "../dev.h"
#include "../devtable.h"

static void translate_directory(FILE* const devtable, const int devbits,
	struct hostprog_path* const rootpath, struct hostprog_path* const dirpath,
	struct stat* dirst)
{
	struct stat direntst;
	struct dirent** dirlst;
	int dirlst_count;
	int dirlst_index;
	
	size_t dirpath_lvl = hostprog_path_lvls(dirpath);
	
	devtable_write(devtable, devbits, rootpath, dirpath, dirst);
	
	dirlst_count = scandir(dirpath->p_path, &dirlst, NULL, hostprog_scandirsort);
	if (dirlst_count < 0)
		error("failed to read \"%s\": %s", dirpath->p_path, strerror(errno));
	
	for (dirlst_index = 0; dirlst_index < dirlst_count; dirlst_index++) {
		struct dirent* dent = dirlst[dirlst_index];
		
		/* Skip "." and ".." directories for obvious reasons.
		 */
		if (hostprog_path_dotdir(dent->d_name))
			continue;
		
		if (hostprog_path_append(dirpath, dent->d_name))
			error("failed to append \"%s\" to the dir path", dent->d_name);
		
		if (lstat(dirpath->p_path, &direntst) < 0) {
			warning("skipping file \"%s\": %s", dirpath->p_path,
				strerror(errno));
		} else {
			if (S_ISDIR(direntst.st_mode)) {
				translate_directory(devtable, devbits, rootpath,
					dirpath, &direntst);
			} else {
				if (devtable_writable(&direntst)) {
					devtable_write(devtable, devbits, rootpath,
						dirpath, &direntst);
				} else {
					warning("skipping file %c \"%s\"",
						nodtype(direntst.st_mode), dirpath->p_path);
				}
			}
		}
		hostprog_path_dirnamelvl(dirpath, dirpath_lvl);
	}
}

static void usage(const char* const exe)
{
	fprintf(stderr, "\nUsage: %s devbits devtable rootdir"
		" [dir1, dir2, ... dirN]\n"
		"\nexample: %s 24 devtable.txt / /dev\n\n"
		"\n", exe, exe);
	exit(EXIT_FAILURE);
}

int main(int argc, char* argv[])
{
	if (argc < 5)
		usage(argc > 0? argv[0]: "");
	
	int argn = 1;
	int devbits = 0;
	opt_strtolx(l, '1', argv[argn], devbits);
	
	char* devtablefile = argv[++argn];
	char* rootdir = argv[++argn];
	
	FILE* devtable = fopen(devtablefile, "w");
	if (!devtable) {
		error("failed to open device table \"%s\": %s",
			devtablefile, strerror(errno));
	}
	
	if (rootdir[0] != '/')
		error("rootdir must be an absolute path");
	
	struct hostprog_path* dirpath = NULL;
	struct hostprog_path* rootpath = NULL;
	if (hostprog_path_create(&dirpath, NULL, 255, 255) < 0 ||
			hostprog_path_create(&rootpath, rootdir, 255, 255) < 0) {
		error("failed to init the directory paths: %s", strerror(errno));
	}
	
	message(VERBOSITY_0, "rootdir: %s", rootdir);
	
	while (++argn < argc) {
		message(VERBOSITY_0, " + %s", argv[argn]);
		
		size_t rootpath_lvls = hostprog_path_lvls(rootpath);
		
		if (hostprog_path_append(dirpath, argv[argn]) < 0) {
			error("failed to set the dir to \"%s\": %s",
				argv[argn], strerror(errno));
		}
		
		struct stat dirst;
		if (lstat(dirpath->p_path, &dirst) < 0) {
			error("could not lstat file \"%s\": %s", dirpath->p_path,
				strerror(errno));
		}
		
		if (devtable_writable(&dirst)) {
			translate_directory(devtable, devbits, rootpath,
				dirpath, &dirst);
		} else {
			warning("skipping file %c \"%s\"", nodtype(dirst.st_mode),
				dirpath->p_path);
		}
		
		hostprog_path_reset(dirpath);
		hostprog_path_dirnamelvl(rootpath, rootpath_lvls);
	}
	
	exit(EXIT_SUCCESS);
}

