#!/bin/bash

# microfs - Minimally Improved Compressed Read Only File System
# Copyright (C) 2014 Erik Edlund <erik.edlund@32767.se>
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

if [ "`basename $0`" == "stress_stop.sh" ] ; then
	exit 1
fi

source_ERR "stress_stop.sh"

if [[ -f "${marker}" ]] ; then
	untrap_ERR
	find "${wd}" -name "${name}-*.pid" -type f | while read line ; do
		pkill -TERM --pidfile "$line"
		while ps -p "`cat "$line"`" > /dev/null ; do
			sleep 0.1s
		done
	done
	trap_ERR
	rm -f "${marker}"
fi
