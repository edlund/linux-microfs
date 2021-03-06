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

source "boilerplate.sh"

script_path=`readlink -f "$0"`
script_dir=`dirname "${script_path}"`

mount_type=""
mk_cmd=""
ck_cmd=""
src_dir=""
dest_dir=""
img_prefix=""
extract_arg=""
img_src=""
rand_seed="`date +%s`"
checksum_prog=""
data_acquirer=""
data_creator=""
stress_test=""
reuse_forbidden=0

options="t:m:c:s:d:p:x:i:r:C:A:D:S:F"
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
		i ) img_src=$OPTARG ;;
		r ) rand_seed=$OPTARG ;;
		C ) checksum_prog=$OPTARG ;;
		A ) data_acquirer=$OPTARG ;;
		D ) data_creator=$OPTARG ;;
		S ) stress_test=$OPTARG ;;
		F ) reuse_forbidden=1 ;;
	esac
done

if [[ ! -d "${src_dir}" || ! -d "${dest_dir}" || \
		-z "${mk_cmd}" || -z "${ck_cmd}" || -z "${img_src}" || \
		-z "${mount_type}" || ! ( "${rand_seed}" =~ ^[0-9]+$ ) || \
		-z "${checksum_prog}" || -z "${data_creator}" || \
		-z "${data_acquirer}" ]] ; then
	cat <<EOF
Usage: `basename $0` -t:m:c:s:d:x:k:C:D: [-r:SF]

Create a file system image, check it, mount it and compare
it against its source, all in one (long) command.

    -t <str>    file system type (as given to mount)
    -m <str>    image make command
    -c <str>    image check command
    -s <str>    source directory for the image
    -d <str>    destination to write all files to
    -p <str>    prefix for the image filename
    -x <str>    extract arg name to give to the check command (if any)
    -i <str>    the source of -s, the command that created it
    -r <int>    seed for random generators (only matters with -S)
    -C <str>    checksum program to use
    -A <str>    data acquirer to use (see microfs_decompressor_data_manager_acquire_*)
    -D <str>    data creator to use (see microfs_decompressor_data_*)
    -S <str>    stress test (very time and resource consuming)
    -F          do not use existing image files, always create them
EOF
	exit 1
fi

# eval is used as it will give better error messages if
# something goes wrong.

img_file="${dest_dir}/${img_prefix}${mk_cmd//[^-a-zA-Z0-9]/_}.img"
cmd_log="${img_file}.log"

if [ "${extract_arg}" != "" ] ; then
	extract_dir="${img_file}.ext"
	ck_cmd="${ck_cmd} ${extract_arg} \"${extract_dir}\""
else
	extract_dir=""
fi

if [[ ! -f "${img_file}" || $reuse_forbidden -eq 1 ]] ; then
	eval "${mk_cmd} \"${src_dir}\" \"${img_file}\" 1>\"${cmd_log}.mk.1\" 2>\"${cmd_log}.mk.2\""
	eval "${ck_cmd} \"${img_file}\" 1>\"${cmd_log}.ck.1\" 2>\"${cmd_log}.ck.2\""
	if [ -d "${extract_dir}" ] ; then
		eval "cmptrees.sh -a \"${src_dir}\" -b \"${extract_dir}\" -c \"${checksum_prog}\""
		rm -rf "${extract_dir}"
	fi
fi

img_mount="${img_file}.mount"
img_mountid="`date +%s`"
img_mountopts=(
	"loop"
	"debug_mountid=${img_mountid}"
	"decompressor_data_acquirer=${data_acquirer}"
	"decompressor_data_creator=${data_creator}"
)
img_mountopts=(
	"-r"
	"-o `implode "," ${img_mountopts[@]}`"
	"-t ${mount_type}"
)

mkdir "${img_mount}"
atexit_0 rmdir "${img_mount}"
eval "sudo mount ${img_mountopts[@]} \"${img_file}\" \"${img_mount}\""
atexit sudo umount "${img_mount}"

if [[ $stress_test != "" ]] ; then
	stress_name="imgmkckver-${rand_seed}"
	stress_start_params=(
		"-o \"${dest_dir}\""
		"-s \"${rand_seed}\""
		"${stress_test}"
		"\"start\""
		"\"${stress_name}\""
		"\"${img_mount}\""
	)
	stress_stop_params=(
		"-o \"${dest_dir}\""
		"stop"
		"\"${stress_name}\""
	)
	stress_start_cmd="${script_dir}/stress.sh ${stress_start_params[@]}"
	stress_stop_cmd="${script_dir}/stress.sh ${stress_stop_params[@]}"
	eval "${stress_start_cmd}"
	eval "atexit ${stress_stop_cmd}"
fi

cmptreeopts=(
	"-a \"${src_dir}\""
	"-b \"${img_mount}\""
	"-e"
	"-w \"${dest_dir}\""
	"-c \"${checksum_prog}\""
	"-n \"${img_prefix}\""
)

eval "cmptrees.sh ${cmptreeopts[@]}"

eval "${script_dir}/rofstests.py \"${img_mount}\""

logckopts=(
	"-l \"${cmd_log}\""
	"-s \"${img_src}\""
	"-m \"microfs: debug_mountid=${img_mountid}\""
)

eval "logck.sh ${logckopts[@]}"

exit 0
