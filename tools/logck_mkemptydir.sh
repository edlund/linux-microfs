#!/bin/bash

# Copyright (C) 2012, 2013, 2014, 2015, 2016, 2017, ..., +%Y
# Erik Edlund <erik.edlund@32767.se>
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

if [ "`basename $0`" == "logck_mkemptydir.sh" ] ; then
	exit 1
fi

source_ERR "logck_mkemptydir.sh"

grep -i 'microfs: this image is empty' "${sys_log}" > /dev/null
