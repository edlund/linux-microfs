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

workdir=""
insid=0

options="d:i:"
while getopts $options option
do
	case $option in
		d ) workdir=$OPTARG ;;
		i ) insid=$OPTARG ;;
	esac
done

if [[ ! -d "${workdir}" || ! ( "${insid}" =~ ^[-a-zA-Z0-9]+$ ) ]] ; then
	cat <<EOF
Usage: `basename $0` -d:i:

decompressor_data_manager specific tests.

    -d <str>    work dir
    -i <int>    module insert id
EOF
	exit 1
fi

img_src="${workdir}/decompressor_data_manager"
img_file="${img_src}.img"

"${top_dir}/tools/mklndir.sh" "${img_src}" > /dev/null
atexit_0 rm -rf "${img_src}"

"${top_dir}/microfsmki" "${img_src}" "${img_file}" > /dev/null
atexit_0 rm "${img_file}"

postfixes=(
	"a"
	"b"
)
for postfix in "${postfixes[@]}" ; do
	img_mount="${img_src}.mount-${postfix}"
	mkdir "${img_mount}"
	atexit_0 rmdir "${img_mount}"
	
	img_mountopts=(
		"loop"
		"decompressor_data_acquirer=public"
	)
	img_mountopts=(
		"-r"
		"-o `implode "," ${img_mountopts[@]}`"
		"-t microfs"
	)
	eval "sudo mount ${img_mountopts[@]} \"${img_file}\" \"${img_mount}\""
	atexit sudo umount "${img_mount}"
done

eval "dmesg | grep -P \" \[insid=${insid}\] \w+_acquire_public: successful share\""
