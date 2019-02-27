/* microfs - Minimally Improved Compressed Read Only File System
 * Copyright (C) 2012, 2013, 2014, 2015, 2016, 2017, ..., +%Y
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

#include "test.h"

START_TEST(test_microfs_ispow2)
	const __u16 pow2[] = {
		/* 0: */ 0, /* 1: */ 0,
		/* 2: */ 1, /* 3: */ 0,
		/* 4: */ 1, /* 5: */ 0,
		/* 6: */ 0, /* 7: */ 0,
		/* 8: */ 1, /* 9: */ 0
	};
	for (size_t n = 0; n < sizeof(pow2) / sizeof(__u16); n++) {
		ck_assert(microfs_ispow2(n) == pow2[n]);
	}
END_TEST

START_TEST(test_packed_structs)
	_ck_assert_int(sizeof(struct microfs_inode), ==, 15);
	_ck_assert_int(sizeof(struct microfs_sb), ==, 77);
	
	_ck_assert_int(sizeof(struct microfs_dd_xz), ==, 8);
END_TEST

START_TEST(test_i_xsize)
	struct microfs_inode ino;
	memset(&ino, 0, sizeof(ino));
	
#define set_and_get(INode, Size) \
	do { \
		i_setsize(INode, Size); \
		_ck_assert_int(Size, ==, i_getsize(INode)); \
	} while (0)
	
	set_and_get(&ino, 65535);
	set_and_get(&ino, 16711680);
	set_and_get(&ino, MICROFS_MAXFILESIZE);
	
	set_and_get(&ino, 5614250);
	set_and_get(&ino, 11162965);
	
#undef set_and_get
	
END_TEST

START_TEST(test_i_blks)
	_ck_assert_int(i_blks(0, 512), ==, 0);
	_ck_assert_int(i_blks(8, 512), ==, 1);
	_ck_assert_int(i_blks(64, 512), ==, 1);
	_ck_assert_int(i_blks(512, 512), ==, 1);
	_ck_assert_int(i_blks(768, 512), ==, 2);
	_ck_assert_int(i_blks(8192, 512), ==, 16);
END_TEST

START_TEST(test_sz_blkceil)
	_ck_assert_int(sz_blkceil(42, 2048), ==, 2048);
	_ck_assert_int(sz_blkceil(3784, 4096), ==, 4096);
	_ck_assert_int(sz_blkceil(4095, 4096), ==, 4096);
	_ck_assert_int(sz_blkceil(0, 16384), ==, 16384);
	
	_ck_assert_int(sz_blkceil(512, 512), ==, 512);
	_ck_assert_int(sz_blkceil(1024, 1024), ==, 1024);
	_ck_assert_int(sz_blkceil(32768, 32768), ==, 32768);
END_TEST

Suite* create_master_suite(void)
{
	Suite* s;
	TCase* tc;
	
	s = suite_create("master");
	tc = tcase_create("master");
	
	suite_add_tcase(s, tc);
	tcase_add_test(tc, test_microfs_ispow2);
	tcase_add_test(tc, test_packed_structs);
	tcase_add_test(tc, test_i_xsize);
	tcase_add_test(tc, test_i_blks);
	tcase_add_test(tc, test_sz_blkceil);
	
	return s;
}

