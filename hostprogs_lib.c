/* microfs - Minimally Improved Compressed Read Only File System
 * Copyright (C) 2014 Erik Edlund <erik.edlund@32767.se>
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
#include "microfs_flags.h"

#include <stdlib.h>
#include <string.h>

static const struct hostprog_lib hostprog_lib_null = {
	.hl_id = MICROFS_FLAG_DECOMPRESSOR_NULL,
	.hl_name = "NULL"
};

static const struct hostprog_lib* hostprog_libs[] = {
	&hostprog_lib_zlib,
	&hostprog_lib_null
};

const struct hostprog_lib* hostprog_lib_find_byid(const int id)
{
	for (int i = 0; hostprog_libs[i]->hl_id; i++) {
		if (hostprog_libs[i]->hl_id == id) {
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
	for (int i = 0; hostprog_libs[i]->hl_id; i++) {
		if (strcmp(hostprog_libs[i]->hl_name, name) == 0) {
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
