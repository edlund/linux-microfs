#!/bin/bash

# Copyright (C) 2013 Erik Edlund <erik.edlund@32767.se>
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
source "${script_dir}/boilerplate.sh"

mount_type=""
mk_cmd=""
ck_cmd=""
src_dir=""
dest_dir=""
img_prefix=""
extract_arg=""

options="t:m:c:s:d:p:x:"
while getopts $options option
do
	case $option in
		t ) mount_type=$OPTARG ;;
		m ) mk_cmd=$OPTARG ;;
		c ) ck_cmd=$OPTARG ;;
		s ) src_dir=$OPTARG ;;
		d ) dest_dir=$OPTARG ;;
		p ) img_prefix=$OPTARG ;;
		x ) extract_arg=$OPTARG ;;
	esac
done

if [[ ! -d "${src_dir}" || ! -d "${dest_dir}" || \
		-z "${mk_cmd}" || -z "${ck_cmd}" || \
		-z "${mount_type}" ]] ; then
	cat <<EOF
Usage: `basename $0` -t:m:c:s:d:x:

Create a file system image, check it, mount it and compare
it against its source, all in one (long) command.

    -t <str>    file system type (as given to mount)
    -m <str>    image make command
    -c <str>    image check command
    -s <str>    source directory for the image
    -d <str>    destination to write all files to
    -p <str>    prefix for the image filename
    -x <str>    extract arg name to give to the check command (if any)
EOF
	exit 1
fi

# eval is used as it will give better error messages if
# something goes wrong.

img_file="${dest_dir}/${img_prefix}${mk_cmd//[^-a-zA-Z0-9]/_}.img"
cmd_log="${img_file}.log"

atexit_0 rm -f "${img_file}"
atexit_0 rm -f "${cmd_log}*"

if [ "${extract_arg}" != "" ] ; then
	extract_dir="${img_file}.ext"
	ck_cmd="${ck_cmd} ${extract_arg} \"${extract_dir}\""
else
	extract_dir=""
fi

eval "${mk_cmd} \"${src_dir}\" \"${img_file}\" 1>\"${cmd_log}.mk.1\" 2>\"${cmd_log}.mk.2\""
eval "${ck_cmd} \"${img_file}\" 1>\"${cmd_log}.ck.1\" 2>\"${cmd_log}.ck.2\""

img_mount="${img_file}.mount"

mkdir "${img_mount}"
atexit rmdir "${img_mount}"
eval "sudo mount -r -o loop -t ${mount_type} \"${img_file}\" \"${img_mount}\""
atexit sudo umount "${img_mount}"

eval "${script_dir}/cmptrees.sh -a \"${src_dir}\" -b \"${img_mount}\" -e -w \"${dest_dir}\""
eval "${script_dir}/rofstests.py \"${img_mount}\""

if [ -d "${extract_dir}" ] ; then
	eval "${script_dir}/cmptrees.sh -a \"${src_dir}\" -b \"${extract_dir}\""
	rm -rf "${extract_dir}"
fi

exit 0
