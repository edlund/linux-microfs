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

script_path=`readlink -f "$0"`
script_dir=`dirname "${script_path}"`
top_dir=`dirname "${script_dir}"`
source "${top_dir}/tools/boilerplate.sh"

if [[ $# -ne 2 || ! -d "$1" || ! ( "$2" =~ ^[0-9]+$ ) ]] ; then
	cat <<EOF
Usage: `basename $0` dirname insid

Test the debug_cksig feature for microfs.
EOF
	exit 1
fi

workdir="$1"
insid="$2"

img_src="${workdir}/debug_cksig"
img_file="${img_src}.img"
img_mount="${img_src}.mount"

"${top_dir}/tools/mklndir.sh" "${img_src}" > /dev/null
atexit_0 rm -rf "${img_src}"

"${top_dir}/microfsmki" "${img_src}" "${img_file}" > /dev/null
atexit_0 rm "${img_file}"

mkdir "${img_mount}"
atexit_0 rmdir "${img_mount}"

img_mountopts=(
	"loop"
	"debug_cksig=1"
)
img_mountopts=(
	"-r"
	"-o `implode "," ${img_mountopts[@]}`"
	"-t microfs"
)
eval "sudo mount ${img_mountopts[@]} \"${img_file}\" \"${img_mount}\""
atexit sudo umount "${img_mount}"

eval "dmesg | grep -P \" \[insid=${insid}\] \w+: good superblock signature\""
