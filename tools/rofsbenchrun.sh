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

declare -r VM_KEEP_CACHE=0
declare -r VM_DROP_PAGECACHE=1
declare -r VM_DROP_INODECACHE=2
declare -r VM_DROP_CACHES=3

fs_type=""
fs_image=""
log_file=""
rd_seed="`date +%s`"

options="t:i:l:s:"
while getopts $options option
do
	case $option in
		t ) fs_type=$OPTARG ;;
		i ) fs_image=$OPTARG ;;
		l ) log_file=$OPTARG ;;
		s ) rd_seed=$OPTARG ;;
	esac
done

if [[ -z "${fs_type}" || ! -f "${fs_image}" || ! -f "${log_file}" ||
	! ( "${rd_seed}" =~ ^[0-9]+$ ) ]] ; then
	cat <<EOF
Usage: `basename $0` -t:i:l: [-s:]

Run simple benchmarks for a given read-only filesystem.

    -t <str>   filesystem type
    -i <str>   path to the filesystem image
    -l <str>   path to the log file
    -s <int>   seed to use for pseudo random reading
EOF
	exit 1
fi

work_dir=`dirname "${fs_image}"`

fs_temp="${work_dir}/tmp"
mkdir -p "${fs_temp}"
fs_mount="${work_dir}/mnt"
mkdir -p "${fs_mount}"

echo "$0: fs_temp: ${fs_temp}"
echo "$0: fs_mount: ${fs_mount}"

echo "$0: log_file: ${log_file}"

PATH="${PATH}:`dirname "${script_dir}"`"
echo "$0: ${fs_type}"

mnt_cmd="sudo mount -r -o loop -t ${fs_type} \"${fs_image}\" \"${fs_mount}\""
echo "$0: ${mnt_cmd}"
eval "${mnt_cmd}"
atexit sudo umount "${fs_mount}"

snore 1s 4 "$0: waiting for mount to settle"

stopwatch() {
	local cache="$1"
	local name="$2"
	local cmd="$3"
	local fmt=(
		"\"${fs_type}\""
		"\"${name}\""
		'"%C"' # Command
		'"%E"' # Real
		'"%U"' # User
		'"%S"' # Sys
	)
	fmt=$(printf ",%s" "${fmt[@]}")
	fmt="${fmt:1}"
	
	if [ "$cache" != "$VM_KEEP_CACHE" ] ; then
		sudo sync
		echo $cache | sudo tee /proc/sys/vm/drop_caches > /dev/null
		echo -n "(drop_caches=$cache) "
	fi
	
	echo "[${fs_type}] running \"${cmd}\""
	eval "/usr/bin/time -f '${fmt}' -o \"${log_file}\" --append ${cmd}"
}

frd_cmd="${script_dir}/frd -e -s ${rd_seed}"

all_paths="${fs_temp}/all-paths.txt"
file_paths="${fs_temp}/file-paths.txt"

echo "$0: directory listing"
stopwatch $VM_DROP_CACHES "0: list all recursively" "ls -lAR \"${fs_mount}\" | cat > /dev/null"
stopwatch $VM_DROP_CACHES "1: find all" "find \"${fs_mount}\" > \"${all_paths}\""
stopwatch $VM_DROP_CACHES "2: find files" "find \"${fs_mount}\" -type f > \"${file_paths}\""

echo "$0: sequential file access, sequential reading"
stopwatch $VM_DROP_CACHES "3: seq access, seq reading" "${frd_cmd} -i \"${file_paths}\""
stopwatch $VM_DROP_CACHES "4: seq access, stat-only" "${frd_cmd} -N -i \"${all_paths}\""

echo "$0: random file access, sequential reading"
stopwatch $VM_DROP_CACHES "5: rand access, seq reading" "${frd_cmd} -R -i \"${file_paths}\""
stopwatch $VM_DROP_CACHES "6: rand access, stat-only" "${frd_cmd} -R -N -i \"${all_paths}\""

echo "$0: sequential file access, random reading"
stopwatch $VM_DROP_CACHES "7: seq access, rand reading" "${frd_cmd} -r -i \"${file_paths}\""

echo "$0: random file access, random reading"
stopwatch $VM_DROP_CACHES "8: rand access, rand reading" "${frd_cmd} -R -r -i \"${file_paths}\""

echo ""
echo ""

exit 0
