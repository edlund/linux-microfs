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

/* Callback for %devtable_parse().
 */
static void devtable_process_dentry(struct devtable_dentry* const devt_dent,
	void* data, const char* file, const char* line, const __u64 linenumber)
{
	(void)data;
	(void)line;
	
	message(VERBOSITY_0, "ck \"%s\"", devt_dent->de_path);
	
	struct stat st;
	if (lstat(devt_dent->de_path + 1, &st) < 0) {
		error("failed to lstat \"%s\" from %s:%llu: %s", devt_dent->de_path,
			file, linenumber, strerror(errno));
	}
	
#define CK_VAL(Type, A, B) \
	if (A != B) { \
		error("%s does not match for \"%s\" from %s:%llu," \
				" " #A "=%d, " #B "=%d", \
			Type, devt_dent->de_path, file, linenumber, \
			(int)A, (int)B); \
	}
	
	CK_VAL("mode", devt_dent->de_mode, st.st_mode);
	CK_VAL("uid", devt_dent->de_uid, st.st_uid);
	CK_VAL("gid", devt_dent->de_gid, st.st_gid);
	CK_VAL("devicenum", devt_dent->de_dev, st.st_rdev);
	
#undef CK_VAL
}

static void usage(const char* const exe)
{
	fprintf(stderr, "\nUsage: %s devbits devtable rootdir\n"
		"\nexample: %s 24 devtable.txt /mnt/image\n\n"
		"\n", exe, exe);
	exit(EXIT_FAILURE);
}

int main(int argc, char* argv[])
{
	if (argc != 4)
		usage(argc > 0? argv[0]: "");
	
	int devbits = 0;
	opt_strtolx(l, '1', argv[1], devbits);
	
	char* devtable = argv[2];
	char* rootdir = argv[3];
	
	if (chdir(rootdir) < 0) {
		error("failed to change dir to \"%s\": %s",
			rootdir, strerror(errno));
	}
	message(VERBOSITY_0, "rootdir: %s", rootdir);
	
	devtable_parse(devtable_process_dentry, rootdir,
		devtable, devbits);
	
	exit(EXIT_SUCCESS);
}

