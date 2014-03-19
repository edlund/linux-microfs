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

# max: 2^27 + 2^26 + 2^25 bytes (224 MB), this is an approximate
# max size for cramfs which leaves a comfortable 32 MB for
# metadata.
# 
# min: 2^20 bytes (1 MB), why not this value?
max_src_budget=234881024
min_src_budget=1048576

lim="1"
blk_sz="4096"
src_seed="`date +%s`"
src_budget="${max_src_budget}"
work_dir=""
log_file_truncate=1
log_file_view=1

mkopts_cramfs=""
mkopts_microfs=""
mkopts_squashfs="-noI"

load_cramfs=1
load_squashfs=1

options="w:n:b:r:B:m:c:s:TVCS"
while getopts $options option
do
	case $option in
		n ) lim=$OPTARG ;;
		w ) work_dir=$OPTARG ;;
		r ) src_seed=$OPTARG ;;
		b ) blk_sz=$OPTARG ;;
		B ) src_budget=$OPTARG ;;
		m ) mkopts_microfs=$OPTARG ;;
		c ) mkopts_cramfs=$OPTARG ;;
		s ) mkopts_squashfs=$OPTARG ;;
		T ) log_file_truncate=0 ;;
		V ) log_file_view=0 ;;
		C ) load_cramfs=0 ;;
		S ) load_squashfs=0 ;;
	esac
done

if [[ ! -d "${work_dir}" || \
	! ( "${lim}" =~ ^[0-9]+$ || $lim -le 0 ) || \
	! ( "${blk_sz}" =~ ^[0-9]+$ ) || \
	! ( "${src_seed}" =~ ^[0-9]+$ ) || \
	! ( "${src_budget}" =~ ^[0-9]+$ && \
		$src_budget -le $max_src_budget && \
		$src_budget -ge $min_src_budget ) ]] ; then
	cat <<EOF
Usage: `basename $0` -w: [-b:r:B:m:c:s:TV]

Run some simple benchmarks using microfs, cramfs and squashfs.

    -w <str>   path to the working directory
    -n <int>   repeat the benchmark
    -b <int>   block size to use
    -r <int>   random seed to give mkrandtree.py
    -B <int>   size budget to give mkrandtree.py
               (min=${min_src_budget}, max=${max_src_budget})
    -m <str>   args to microfsmki
    -c <str>   args to mkcramfs
    -s <str>   args to mksquashfs
    -T         do NOT truncate the log file
    -V         do NOT generate a view of the log file
    -C         do NOT load cramfs
    -S         do NOT load squashfs
EOF
	exit 1
fi

mkoptsblksz() {
	local cmd=$1
	local sz=$2
	if [[ $cmd == *-b* ]] ; then
		echo "$cmd"
	else
		echo "$cmd -b $sz"
	fi
}

mkopts_microfs=`mkoptsblksz "${mkopts_microfs}" "${blk_sz}"`
mkopts_squashfs=`mkoptsblksz "${mkopts_squashfs}" "${blk_sz}"`

echo "$0: `uname -s -r -i` benchmark @ `date "+%Y-%m-%d %H:%M:%S"`"
echo "$0: root required"
sudo true

if [[ "`lsmod | grep 'microfs'`" == "" ]] ; then
	echo "$0: inserting microfs.ko"
	sudo insmod "`dirname "${script_dir}"`/microfs.ko"
	atexit sudo rmmod microfs
else
	echo "$0: microfs.ko already inserted"
fi

if [[ $load_cramfs -eq 1 ]] ; then
	modprobe cramfs
fi
if [[ $load_squashfs -eq 1 ]] ; then
	modprobe squashfs
fi

# Trim any trailing slashes.
work_dir=`echo "${work_dir}" | sed 's:/*$::'`

echo "$0: working directory: ${work_dir}"

src_name="${src_budget}-${src_seed}"
src_dir="${work_dir}/${src_name}"
src_cmd=(
	"${script_dir}/mkrandtree.py"
	"--random-seed=${src_seed}"
	"--size-budget=${src_budget}"
	"\"${src_dir}\""
)
src_cmd="${src_cmd[@]}"

if [ ! -d "${src_dir}" ] ; then
	echo "$0: creating source dir \"${src_dir}\" using \"${src_cmd}\""
	eval "${src_cmd}"
else
	echo "$0: using existing source dir \"${src_dir}\""
fi

# The filesystem image and metadata files created by
# %rofsbenchrun.sh should be able to fit in 256M.
tmpfs_conf="size=256M,nr_inodes=32k,mode=0755"
tmpfs_dir="${work_dir}/tmpfs"
if [ ! -d "${tmpfs_dir}" ] ; then
	mkdir "${tmpfs_dir}"
fi

echo "$0: mounting tmpfs on \"${tmpfs_dir}\" with options \"${tmpfs_conf}\""
eval "sudo mount -t tmpfs -o \"${tmpfs_conf}\" tmpfs \"${tmpfs_dir}\""
atexit sudo umount "${tmpfs_dir}"
eval "sudo chown -R --reference=\"${HOME}\" \"${tmpfs_dir}\""

PATH="${PATH}:`dirname "${script_dir}"`"
echo "$0: using PATH=${PATH}"

declare -A filesystems

filesystems[cramfs]="mkcramfs ${mkopts_cramfs} @FILES"
filesystems[microfs]="microfsmki ${mkopts_microfs} @FILES"
filesystems[squashfs]="mksquashfs @FILES ${mkopts_squashfs}"

log_file="${work_dir}/${src_budget}-${src_seed}-${blk_sz}-all.csv"
if [[ ! -f "${log_file}" || $log_file_truncate -eq 1 ]] ; then
	truncate --size=0 "${log_file}"
fi

for i in $(seq 1 $lim) ; do
	for filesystem in "${!filesystems[@]}" ; do
		echo ""
		echo ""
		echo "$0: ${filesystem} ---- pass $i"
		echo ""
		echo ""
		img_hostprog="${filesystems[$filesystem]}"
		img_name="${filesystem}-${img_hostprog//[^-a-zA-Z0-9]/-}-${src_name}.img"
		img_file="${work_dir}/${img_name}"
		img_files="\"${src_dir}\" \"${img_file}\""
		img_cmd="${img_hostprog/@FILES/$img_files}"
		
		if [ ! -f "${img_file}" ] ; then
			echo "$0: running \"${img_cmd}\" to create \"${img_file}\""
			eval "${img_cmd}"
		else
			echo "$0: using existing image \"${img_file}\""
		fi
		
		tmp_img_file="${tmpfs_dir}/${img_name}"
		cp "${img_file}" "${tmp_img_file}"
		
		echo "$0: benchmark run for ${filesystem}"
		bench_cmd=(
			"${script_dir}/rofsbenchrun.sh"
			"-s ${src_seed}"
			"-t ${filesystem}"
			"-i \"${tmp_img_file}\""
			"-l \"${log_file}\""
		)
		bench_cmd="${bench_cmd[@]}"
		echo "$0: ${bench_cmd}"
		eval "${bench_cmd}"
		
		rm -f "${tmp_img_file}"
	done
	echo ""
	echo "$0: pass $i done"
done

if [ $log_file_view -eq 1 ] ; then
	echo "$0: benchmark data:"
	"${script_dir}/rofsbenchview.py" "${log_file}"
fi

echo "$0: done"

exit 0
