#!/bin/bash

# microfs - Minimally Improved Compressed Read Only File System
# Copyright (C) 2012 Erik Edlund <erik.edlund@32767.se>
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

# check.sh: Functional tests for microfs.

script_path=`readlink -f "$0"`
script_dir=`dirname "${script_path}"`
source "${script_dir}/tools/boilerplate.sh"

conf_tempmnt="size=768M,nr_inodes=10k,mode=0755"
conf_notempmnt=0
options="Mm:"
while getopts $options option
do
	case $option in
		m ) conf_tempmnt=$OPTARG ;;
		M ) conf_notempmnt=1 ;;
	esac
done

# Rely on the credentials being cached after the first sudo.
echo "$0: root required to proceed (last chance to change your mind)"
sudo true

_check_start_time=`date +%s`

build_dir=$script_dir

temp_dir=`mktemp -d --tmpdir microfs.test.XXXXXXXXXXXXXXXX`
atexit_0 rm -rf "${temp_dir}"
echo "$0: temporary directory used is \"${temp_dir}\""

if [[ $conf_notempmnt -eq 0 && "${conf_tempmnt}" != "" ]] ; then
	eval "sudo mount -t tmpfs -o \"${conf_tempmnt}\" tmpfs \"${temp_dir}\""
	atexit sudo umount "${temp_dir}"
	eval "sudo chown -R --reference=\"${HOME}\" \"${temp_dir}\""
	temp_err_dir=`mktemp -d --tmpdir microfs.test.fail.XXXXXXXXXXXXXXXX`
	atexit_0 rmdir "${temp_err_dir}"
	atexit_1 cp -r "${temp_dir}/" "${temp_err_dir}"
	echo "$0: tmpfs mounted on \"${temp_dir}\" with the options \"${conf_tempmnt}\""
	echo "$0: if anything goes wrong, inspect \"${temp_err_dir}\""
else
	echo "$0: tmpfs will not be used"
fi

echo "$0: running utility tests..."

check_util_cmptrees() {
	local dir_a="${temp_dir}/check_util_cmptrees-a"
	local dir_b="${temp_dir}/check_util_cmptrees-b"
	local dirs=("${dir_a}" "${dir_b}")
	for dir in "${dirs[@]}" ; do
		mkdir "${dir}"
		echo "The quick brown fox jumps over the lazy dog" > "${dir}/fox.txt"
		cat << 'EOF' > "${dir}/loremipsum.txt"
Lorem ipsum dolor sit amet, consectetur adipisicing elit,
sed do eiusmod tempor incididunt ut labore et dolore magna
aliqua. Ut enim ad minim veniam, quis nostrud exercitation
ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis
aute irure dolor in reprehenderit in voluptate velit esse
cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat
cupidatat non proident, sunt in culpa qui officia deserunt
mollit anim id est laborum.
EOF
	done
	eval "${script_dir}/tools/cmptrees.sh -a \"${dir_a}\" -b \"${dir_b}\""
	rm "${dir_a}/fox.txt"
	untrap_ERR
	local diff=`${script_dir}/tools/cmptrees.sh -a \"${dir_a}\" -b \"${dir_b}\"`
	local exit_code=$?
	trap_ERR
	if [[ $exit_code -eq 0 || "${diff}" == "" ]] ; then
		test -z "diff unnoticed"
	fi
	true
}

check_util_mkrandtree() {
	local dir_a="${temp_dir}/check_util_mkrandtree-a"
	local dir_b="${temp_dir}/check_util_mkrandtree-b"
	local random_seed="--random-seed=1"
	local size_budget="--size-budget=16777216"
	"${script_dir}/tools/mkrandtree.py" $random_seed $size_budget "${dir_a}"
	"${script_dir}/tools/mkrandtree.py" $random_seed $size_budget "${dir_b}"
	"${script_dir}/tools/cmptrees.sh" -a "${dir_a}" -b "${dir_b}" -w "${temp_dir}"
}

check_util_mkholedir() {
	local dir_a="${temp_dir}/check_util_mkholedir-a"
	local dir_b="${temp_dir}/check_util_mkholedir-b"
	local random_seed="--random-seed=0"
	"${script_dir}/tools/mkholedir.py" $random_seed "${dir_a}"
	"${script_dir}/tools/mkholedir.py" $random_seed "${dir_b}"
	"${script_dir}/tools/cmptrees.sh" -a "${dir_a}" -b "${dir_b}" -w "${temp_dir}"
}

check_util_cmptrees
check_util_mkrandtree
check_util_mkholedir

echo "$0: utility tests passed."
echo "$0: running lkm and hostprog tests..."

img_srcs=()

src_paths=()
src_cmds=()

options="p:c:"
while getopts $options option
do
	case $option in
		p ) src_paths=("${OPTARG}") ;;
		c ) src_cmds=("${OPTARG}") ;;
	esac
done

if [ "${#src_cmds[@]}" == 0 ] ; then
	src_cmds=(
		"${script_dir}/tools/mkemptydir.sh"
		"${script_dir}/tools/mklndir.sh"
		"${script_dir}/tools/mkmbdentdir.sh"
		"${script_dir}/tools/mkholedir.py"
		"${script_dir}/tools/mkpow2dir.py"
		"${script_dir}/tools/mkrandtree.py"
	)
fi

for src_path in "${src_paths[@]}" ; do
	echo "$0: Adding \"${src_path}\" as an image source dir..."
	src_dir=`basename ${src_path}`
	img_src="${temp_dir}/${src_dir}"
	img_srcs+=("${img_src}")
	sudo cp -r "${src_path}" "${img_src}"
	user=`whoami`
	group=`id -gn $user`
	sudo chown -hR $user:$group "${img_src}"
	echo "$0: ... ok."
done

for src_cmd in "${src_cmds[@]}" ; do
	src_dir=`basename "${src_cmd}"`
	src_dir="${src_dir//[^-a-zA-Z0-9]/_}"
	img_src="${temp_dir}/${src_dir}"
	img_srcs+=("${img_src}")
	echo "$0: Running \"${src_cmd}\" to create \"${img_src}\"..."
	eval "${src_cmd} \"${img_src}\""
	echo "$0: ... ok."
done

sudo insmod "${build_dir}/microfs.ko"
atexit sudo rmmod microfs
echo "$0: Kernel module loaded"

nonsense_mount() {
	local img_blks=$2
	local img_blksz=$1
	local img_path="${temp_dir}/nonsense-${img_blks}-${img_blksz}.img"
	local img_mount="${img_path}.mount"
	echo "$0: mounting ${img_blks} ${img_blksz} byte blocks of random data"
	dd count=$img_blks bs=$img_blksz "if=/dev/urandom" "of=${img_path}" \
		> /dev/null 2>&1
	mkdir "${img_mount}"
	untrap_ERR
	sudo mount -t microfs -o loop -r "${img_path}" "${img_mount}" \
		> /dev/null 2>&1
	local stat=$?
	trap_ERR
	test $stat -eq 1
	rmdir "${img_mount}"
	rm -f "${img_path}"
	true
}

# Try to mount something which is not a microfs image.
# It should fail without anything catching on fire.
nonsense_mount 512 1
nonsense_mount 512 2
nonsense_mount 512 8
nonsense_mount 512 64
nonsense_mount 512 128
nonsense_mount 512 4096

# Try every compression option with every block size and
# try that with and without padding.
compression_options=(
	"-c none"
	"-c default"
	"-c size"
	"-c speed"
)
blocksz_options=(
	"-b 512"
	"-b 1024"
	"-b 2048"
	"-b 4096"
	"-b 8192"
	"-b 16384"
	"-b 32768"
)
base_options=("${compression_options[@]/%/ -v}")
all_options=()
for blksz in "${blocksz_options[@]}" ; do
	all_options=("${all_options[@]}" "${base_options[@]/%/ ${blksz}}")
done
all_options=("${all_options[@]}" "${all_options[@]/%/ -p}")

for img_src in "${img_srcs[@]}" ; do
	for options in "${all_options[@]}" ; do
		mk_cmd="${build_dir}/microfsmki ${options}"
		ck_cmd="${build_dir}/microfscki -v -e"
		image_params=(
			"-t microfs"
			"-m \"${mk_cmd}\""
			"-c \"${ck_cmd}\""
			"-s \"${img_src}\""
			"-d \"${temp_dir}\""
			"-p \"`basename \"${img_src}\"`-\""
			"-x \"-x\""
		)
		image_cmd="${script_dir}/tools/imgmkckver.sh ${image_params[@]}"
		echo "$0: Running image command \"${image_cmd}\"..."
		eval "${image_cmd}"
		echo "$0: ... ok."
	done
done

echo "$0: lkm and hostprog tests passed."

_check_end_time=`date +%s`
_check_total_time=`expr $_check_end_time - $_check_start_time`

echo ""
echo "$0: functional tests passed."
echo "$0: execution time: ${_check_total_time} sec."
echo ""

exit 0
