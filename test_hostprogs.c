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

#include "test.h"

START_TEST(test_hostprog_stack)
	_Static_assert(sizeof(hostprog_stack_int_t) == sizeof(void*),
		"unexcpected hostprog_stack_int_t size");
	
	struct hostprog_stack* stack = NULL;
	ck_assert(hostprog_stack_create(&stack, 4, 4) == 0);
	
	hostprog_stack_int_t* ngrabber = NULL;
	
	hostprog_stack_int_t i;
	hostprog_stack_int_t lim = 128;
	
	for (i = 0; i < lim; i++) {
		ck_assert(hostprog_stack_push(stack, i) == 0);
		ck_assert(hostprog_stack_size(stack) == i + 1);
	}
	
	for (i = lim; i > 0; i--) {
		ck_assert(hostprog_stack_pop(stack, &ngrabber) == 0);
		ck_assert(hostprog_stack_size(stack) == i - 1);
		ck_assert((hostprog_stack_int_t)ngrabber == i - 1);
	}
	
	ck_assert(hostprog_stack_destroy(stack) == 0);
END_TEST

START_TEST(test_hostprog_path)
	struct hostprog_path* dpath = NULL;
	ck_assert(hostprog_path_create(&dpath, NULL, 255, 16) == 0);
	ck_assert(dpath->p_maxnamelen == 255);
	ck_assert(dpath->p_mingrowth == 16);
	ck_assert_str_eq(dpath->p_path, "");
	
	ck_assert(hostprog_path_append(dpath, "/") == 0);
	ck_assert_str_eq(dpath->p_path, "/");
	
	ck_assert(hostprog_path_append(dpath, "/dev") == 0);
	ck_assert_str_eq(dpath->p_path, "/dev");
	
	ck_assert(hostprog_path_append(dpath, "urandom") == 0);
	ck_assert_str_eq(dpath->p_path, "/dev/urandom");
	
	ck_assert(hostprog_path_dirname(dpath) == 0);
	ck_assert_str_eq(dpath->p_path, "/dev");
	
	ck_assert(hostprog_path_append(dpath, "urandom") == 0);
	ck_assert_str_eq(dpath->p_path, "/dev/urandom");
	
	ck_assert(hostprog_path_dirname(dpath) == 0);
	ck_assert_str_eq(dpath->p_path, "/dev");
	
	ck_assert(hostprog_path_append(dpath, "random/") == 0);
	ck_assert_str_eq(dpath->p_path, "/dev/random");
	
	ck_assert(hostprog_path_dirname(dpath) == 0);
	ck_assert_str_eq(dpath->p_path, "/dev");
	
	ck_assert(hostprog_path_append(dpath, "/zero/") == 0);
	ck_assert_str_eq(dpath->p_path, "/dev/zero");
	
	ck_assert(hostprog_path_dirname(dpath) == 0);
	ck_assert_str_eq(dpath->p_path, "/dev");
	
	ck_assert(hostprog_path_dirname(dpath) == 0);
	ck_assert_str_eq(dpath->p_path, "/");
	
	ck_assert(hostprog_path_dirname(dpath) == 0);
	ck_assert_str_eq(dpath->p_path, "");
	
	ck_assert(hostprog_path_append(dpath, "/home/") == 0);
	ck_assert_str_eq(dpath->p_path, "/home");
	
	ck_assert(hostprog_path_dirname(dpath) == 0);
	ck_assert_str_eq(dpath->p_path, "/");
	
	ck_assert(hostprog_path_dirname(dpath) == 0);
	ck_assert_str_eq(dpath->p_path, "");
	
	ck_assert(hostprog_path_append(dpath, "usr/") == 0);
	ck_assert_str_eq(dpath->p_path, "usr");
	
	ck_assert(hostprog_path_dirname(dpath) == 0);
	ck_assert_str_eq(dpath->p_path, "");
	
	ck_assert(hostprog_path_append(dpath, "boot") == 0);
	ck_assert_str_eq(dpath->p_path, "boot");
	
	ck_assert(hostprog_path_append(dpath, "/grub") == 0);
	ck_assert_str_eq(dpath->p_path, "boot/grub");
	
	ck_assert(hostprog_path_append(dpath, "i386-pc/") == 0);
	ck_assert_str_eq(dpath->p_path, "boot/grub/i386-pc");
	
	ck_assert(hostprog_path_dirname(dpath) == 0);
	ck_assert_str_eq(dpath->p_path, "boot/grub");
	
	ck_assert(hostprog_path_append(dpath, "grub.cfg") == 0);
	ck_assert_str_eq(dpath->p_path, "boot/grub/grub.cfg");
	
	ck_assert(hostprog_path_dirname(dpath) == 0);
	ck_assert_str_eq(dpath->p_path, "boot/grub");
	
	ck_assert(hostprog_path_dirname(dpath) == 0);
	ck_assert_str_eq(dpath->p_path, "boot");
	
	ck_assert(hostprog_path_dirname(dpath) == 0);
	ck_assert_str_eq(dpath->p_path, "");
	
	ck_assert(hostprog_path_append(dpath, "tmp////") == 0);
	ck_assert_str_eq(dpath->p_path, "tmp");
	
	ck_assert(hostprog_path_append(dpath, "/.vbox-ipc////") == 0);
	ck_assert_str_eq(dpath->p_path, "tmp/.vbox-ipc");
	
	ck_assert(hostprog_path_dirname(dpath) == 0);
	ck_assert_str_eq(dpath->p_path, "tmp");
	
	ck_assert(hostprog_path_dirname(dpath) == 0);
	ck_assert_str_eq(dpath->p_path, "");
	
	ck_assert(hostprog_path_append(dpath, "//") == 0);
	ck_assert_str_eq(dpath->p_path, "/");
	
	ck_assert(hostprog_path_dirname(dpath) == 0);
	ck_assert_str_eq(dpath->p_path, "");
	
	ck_assert(hostprog_path_append(dpath, "boot/grub/grub.cfg") == 0);
	ck_assert_str_eq(dpath->p_path, "boot/grub/grub.cfg");
	
	ck_assert(hostprog_path_reset(dpath) == 0);
	ck_assert_str_eq(dpath->p_path, "");
	
	ck_assert(hostprog_path_append(dpath, "/boot/grub/grub.cfg") == 0);
	ck_assert_str_eq(dpath->p_path, "/boot/grub/grub.cfg");
	
	ck_assert(hostprog_path_reset(dpath) == 0);
	ck_assert_str_eq(dpath->p_path, "");
	
	ck_assert(hostprog_path_append(dpath, "/boot//grub///grub.cfg") == 0);
	ck_assert_str_eq(dpath->p_path, "/boot/grub/grub.cfg");
	
	ck_assert(hostprog_path_reset(dpath) == 0);
	ck_assert_str_eq(dpath->p_path, "");
	
	ck_assert(hostprog_path_append(dpath, "/tmp/testgrowth") == 0);
	ck_assert_str_eq(dpath->p_path, "/tmp/testgrowth");
	ck_assert(hostprog_path_append(dpath, "ofpath") == 0);
	ck_assert_str_eq(dpath->p_path, "/tmp/testgrowth/ofpath");
	ck_assert(hostprog_path_append(dpath, "/itshouldworkfine") == 0);
	ck_assert_str_eq(dpath->p_path, "/tmp/testgrowth/ofpath/"
		"itshouldworkfine");
	
	ck_assert(hostprog_path_reset(dpath) == 0);
	ck_assert_str_eq(dpath->p_path, "");
	
	ck_assert(hostprog_path_destroy(dpath) == 0);
	
	dpath = NULL;
	ck_assert(hostprog_path_create(&dpath, "/etc/hosts", 255, 16) == 0);
	ck_assert(dpath->p_maxnamelen == 255);
	ck_assert(dpath->p_mingrowth == 16);
	ck_assert_str_eq(dpath->p_path, "/etc/hosts");
	
	ck_assert(hostprog_path_lvls(dpath) == 3);
	ck_assert(hostprog_path_dirnamelvl(dpath, 2) == 0);
	ck_assert(hostprog_path_lvls(dpath) == 2);
	ck_assert_str_eq(dpath->p_path, "/etc");
	
	ck_assert(hostprog_path_dirnamelvl(dpath, 1) == 0);
	ck_assert(hostprog_path_lvls(dpath) == 1);
	ck_assert_str_eq(dpath->p_path, "/");
	
	ck_assert(hostprog_path_dirnamelvl(dpath, 0) == 0);
	ck_assert(hostprog_path_lvls(dpath) == 0);
	ck_assert_str_eq(dpath->p_path, "");
	
	ck_assert(hostprog_path_append(dpath, "etc/shadow") == 0);
	ck_assert_str_eq(dpath->p_path, "etc/shadow");
	ck_assert(hostprog_path_lvls(dpath) == 2);
	
	hostprog_path_reset(dpath);
	
	ck_assert(hostprog_path_destroy(dpath) == 0);
	
	ck_assert(hostprog_path_dotdir(".") == 1);
	ck_assert(hostprog_path_dotdir("..") == 1);
	ck_assert(hostprog_path_dotdir("/") == 0);
	ck_assert(hostprog_path_dotdir("./") == 0);
	ck_assert(hostprog_path_dotdir("../") == 0);
	ck_assert(hostprog_path_dotdir("/.") == 0);
	ck_assert(hostprog_path_dotdir("/dev") == 0);
	ck_assert(hostprog_path_dotdir(".dot") == 0);
	ck_assert(hostprog_path_dotdir("..dotdot") == 0);
END_TEST

Suite* create_hostprogs_suite(void)
{
	Suite* s;
	TCase* tc;
	
	s = suite_create("hostprogs");
	tc = tcase_create("hostprogs");
	
	suite_add_tcase(s, tc);
	
	tcase_add_test(tc, test_hostprog_stack);
	tcase_add_test(tc, test_hostprog_path);
	
	return s;
}

