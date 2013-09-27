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

fs_type=""
fs_image=""

rd_seed="`date +%s`"

options="t:i:s:"
while getopts $options option
do
	case $option in
		t ) fs_type=$OPTARG ;;
		i ) fs_image=$OPTARG ;;
		s ) rd_seed=$OPTARG ;;
	esac
done

if [[ -z "${fs_type}" || ! -f "${fs_image}" ||
	! ( "${rd_seed}" =~ ^[0-9]+$ ) ]] ; then
	cat <<EOF
Usage: `basename $0` -t:i: [-s:]

Run simple benchmarks for a given read-only filesystem.

    -t <str>   filesystem type
    -i <str>   path to the filesystem image
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

PATH="${PATH}:`dirname "${script_dir}"`"
echo "$0: ${fs_type}"

mnt_cmd="sudo mount -r -o loop -t ${fs_type} \"${fs_image}\" \"${fs_mount}\""
echo "$0: ${mnt_cmd}"
eval "${mnt_cmd}"
atexit sudo umount "${fs_mount}"

stopwatch() {
	local cmd="$*"
	local fmt=">>> %E real, %U user, %S sys"
	eval "/usr/bin/time -f \"${fmt}\" ${cmd}"
}

frd_cmd="${script_dir}/frd -e -s ${rd_seed}"

all_paths="${fs_temp}/all-paths.txt"
file_paths="${fs_temp}/file-paths.txt"

echo "$0: directory listing"
stopwatch ls -lAR "${fs_mount}" | cat > /dev/null
stopwatch find "${fs_mount}" > "${all_paths}"
stopwatch find "${fs_mount}" -type f > "${file_paths}"

echo "$0: sequential file access, sequential reading"
stopwatch "${frd_cmd} -i \"${file_paths}\""
stopwatch "${frd_cmd} -N -i \"${all_paths}\""

echo "$0: random file access, sequential reading"
stopwatch "${frd_cmd} -R -i \"${file_paths}\""
stopwatch "${frd_cmd} -R -N -i \"${all_paths}\""

echo "$0: sequential file access, random reading"
stopwatch "${frd_cmd} -r -i \"${file_paths}\""

echo "$0: random file access, random reading"
stopwatch "${frd_cmd} -R -r -i \"${file_paths}\""

echo ""
echo ""

exit 0
