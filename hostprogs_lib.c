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

#include "hostprogs_lib.h"
#include "microfs_fs.h"

#include <stdlib.h>
#include <string.h>

static const struct hostprog_lib hostprog_lib_null = {
	.hl_info = NULL
};

static const struct hostprog_lib* hostprog_libs[] = {
	&hostprog_lib_zlib,
	&hostprog_lib_lz4,
	&hostprog_lib_lzo,
	&hostprog_lib_xz,
	&hostprog_lib_zstd,
	&hostprog_lib_null
};

const struct hostprog_lib* hostprog_lib_find_any(void)
{
	for (int i = 0; hostprog_libs[i]->hl_info; i++) {
		if (hostprog_libs[i]->hl_compiled)
			return hostprog_libs[i];
	}
	return NULL;
}

const struct hostprog_lib* hostprog_lib_find_byid(const int id)
{
	for (int i = 0; hostprog_libs[i]->hl_info; i++) {
		if (hostprog_libs[i]->hl_info->li_id == id) {
			if (!hostprog_libs[i]->hl_compiled)
				break;
			else
				return hostprog_libs[i];
		}
	}
	return NULL;
}

const struct hostprog_lib* hostprog_lib_find_byname(const char* name)
{
	for (int i = 0; hostprog_libs[i]->hl_info; i++) {
		if (strcmp(hostprog_libs[i]->hl_info->li_name, name) == 0) {
			if (!hostprog_libs[i]->hl_compiled)
				break;
			else
				return hostprog_libs[i];
		}
	}
	return NULL;
}

const struct hostprog_lib** hostprog_lib_all(void)
{
	return hostprog_libs;
}

void hostprog_lib_mk_usage(FILE* const dest)
{
	fprintf(dest, " no options available\n");
}

int hostprog_lib_mk_option(void* data, const char* name, const char* value)
{
	(void)data;
	(void)name;
	(void)value;
	return 0;
}

__u64 hostprog_lib_mk_dd(void* data, char* base, __u64 offset)
{
	(void)data;
	(void)base;
	return offset;
}

int hostprog_lib_ck_dd(void* data, char* dd)
{
	(void)data;
	(void)dd;
	return 0;
}

