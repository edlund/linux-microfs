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

# test.sh: Functional tests for microfs.

source "boilerplate.sh"

script_path=`readlink -f "$0"`
script_dir=`dirname "${script_path}"`

conf_paddingtest="no"
conf_quicktest="no"
conf_stresstest="no"
conf_usetempmnt="yes"
conf_tempmnt="size=448M,nr_inodes=32k,mode=0755"
conf_sizebudget="134217728"
conf_filecontent="uncompressable"
conf_randomseed="`date +%s`"
conf_insid="`date +%s`"
conf_checksum="sha512sum"
conf_stressparams="-S"

data_options=()
src_cmds=()

options="PQSMm:b:f:r:C:d:c:s:"
while getopts $options option
do
	case $option in
		P ) conf_paddingtest="yes" ;;
		Q ) conf_quicktest="yes" ;;
		S ) conf_stresstest="yes" ;;
		M ) conf_usetempmnt="no" ;;
		m ) conf_tempmnt=$OPTARG ;;
		b ) conf_sizebudget=$OPTARG ;;
		f ) conf_filecontent=$OPTARG ;;
		r ) conf_randomseed=$OPTARG ;;
		C ) conf_checksum=$OPTARG ;;
		d ) data_options+=("${OPTARG}") ;;
		c ) src_cmds+=("${OPTARG}") ;;
		s ) conf_stressparams=$OPTARG ;;
	esac
done

if [[ ${#data_options[@]} -eq 0 ]] ; then
	data_options=(
		"singleton"
		"percpu"
		"queue"
	)
fi

data_options=("${data_options[@]/#/-D }")
data_options=(
	"${data_options[@]/%/ -A private}"
	"${data_options[@]/%/ -A public}"
)

src_cmds+=(
	"mkemptydir.sh"
	"mkhuskdir.sh"
	"mklndir.sh"
	"mkmbdentdir.sh"
	"mkpow2dir.py"
	"mkholedir.py \
--random-seed=${conf_randomseed}"
	"mkrandtree.py \
--random-seed=${conf_randomseed} \
--size-budget=${conf_sizebudget} \
--file-content=${conf_filecontent}_bytes"
)

echo ""
echo "$0: CONFIGURATION:"
echo "$0: padding test? ${conf_paddingtest}"
echo "$0: quick test? ${conf_quicktest}"
echo "$0: stress test? ${conf_stresstest}"
echo "$0: use tmpfs? ${conf_usetempmnt}"
echo "$0: insert id is \"${conf_insid}\""
echo "$0: random seed is \"${conf_randomseed}\""
echo "$0: size budget is \"${conf_sizebudget}\""
echo "$0: checksum program is \"${conf_checksum}\""
echo "$0: stress params are \"${conf_stressparams}\""
echo "$0: libraries tested: `${script_dir}/microfslib | tr '\n' ' '`"
echo "$0: data options tested:"
for data_option in "${data_options[@]}" ; do
	echo "$0: => \"${data_option}\""
done
echo ""

# Rely on the credentials being cached after the first sudo.
echo "$0: root required to proceed (last chance to change your mind)"
sudo true

_check_start_time=`date +%s`

build_dir=$script_dir

temp_dir=`mktemp -d --tmpdir microfs.test.XXXXXXXXXXXXXXXX`
atexit_0 rm -rf "${temp_dir}"
echo "$0: temporary directory used is \"${temp_dir}\""

if [[ "${conf_usetempmnt}" == "yes" && "${conf_tempmnt}" != "" ]] ; then
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

if [[ "${conf_quicktest}" == "yes" ]] ; then
	conf_quicktest="-T"
else
	conf_quicktest="-t"
fi

if [[ "${conf_stresstest}" == "yes" ]] ; then
	conf_stresstest="${conf_stressparams}"
else
	conf_stresstest=""
fi

echo "$0: running lkm and hostprog tests..."

sudo insmod "${build_dir}/microfs.ko" "debug_insid=${conf_insid}"
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

devtable_mount() {
	local mk_cmd="$1"
	echo "$0: running device table mk command: \"${mk_cmd}\""
	eval "${mk_cmd}"
	test $? -ne 0 && exit 1
	local test_options="$2"
	local test_cmd="${script_dir}/tools/devtabletests.sh ${test_options}"
	echo "$0: running device table test command: \"${test_cmd}\""
	eval "${test_cmd}"
}

# Try to use device tables.
devtable_host_cmd=(
	"${script_dir}/tools/devtmk"
	"32"
	"\"${temp_dir}/devtable_host.txt\""
	"/"
	"/dev"
	"1>devtable_host.txt.1"
	"2>devtable_host.txt.2"
)
devtable_host_cmd="${devtable_host_cmd[@]}"
devtable_host=(
	"-t \"${temp_dir}\""
	"-d \"${temp_dir}/devtable_host.txt\""
	"-m \"${script_dir}/microfsmki\""
	"-f \"microfs\""
)
devtable_host="${devtable_host[@]}"
devtable_simple_cmd=(
	"true"
)
devtable_simple_cmd="${devtable_simple_cmd[@]}"
devtable_simple=(
	"-t \"${temp_dir}\""
	"-d \"${script_dir}/extras/devtable.txt\""
	"-m \"${script_dir}/microfsmki\""
	"-f \"microfs\""
)
devtable_simple="${devtable_simple[@]}"

devtable_mount "${devtable_simple_cmd}" "${devtable_simple}"
devtable_mount "${devtable_host_cmd}" "${devtable_host}"

# Run highly specialized functional tests.
test_debug_cksig=(
	"\"${temp_dir}\""
	"\"${conf_insid}\""
)
test_debug_cksig="debug_cksig.sh ${test_debug_cksig[@]}"
test_decompressor_data_manager=(
	"-d \"${temp_dir}\""
	"-i \"${conf_insid}\""
)
test_decompressor_data_manager="decompressor_data_manager.sh ${test_decompressor_data_manager[@]}"
test_statfs=(
	"\"${temp_dir}\""
	"\"${conf_insid}\""
)
test_statfs="statfs.sh ${test_statfs[@]}"

spec_tests=(
	"${test_debug_cksig}"
	"${test_decompressor_data_manager}"
	"${test_statfs}"
)

for spec_test in "${spec_tests[@]}" ; do
	spec_test="${script_dir}/tests/${spec_test}"
	echo "$0: running specialized test \"${spec_test}\"..."
	eval "${spec_test}"
	echo "$0: ... ok (`date +'%H:%M:%S'`)."
	snore 1s 1 "$0: waiting for a new second"
done

# Run generic functional tests (compiled compression libs
# with different block sizes working with different sources).
"${script_dir}/microfslib" > "${temp_dir}/libs.txt"
readarray -t compression_options < "${temp_dir}/libs.txt"

all_options=()

for compression_option in "${compression_options[@]}" ; do
	temp_file="${temp_dir}/lib-${compression_option}.txt"
	"${script_dir}/microfslib" -c ${compression_option} ${conf_quicktest} \
		> "${temp_file}"
	readarray -t blksz_options \
		< "${temp_file}"
	
	blksz_options=("${blksz_options[@]/#/-b }")
	all_options=("${all_options[@]}" "${blksz_options[@]/#/-v -c ${compression_option} }")
	
	unset blksz_options
	unset temp_file
done

if [[ "${conf_paddingtest}" == "yes" ]] ; then
	all_options=("${all_options[@]}" "${all_options[@]/%/ -p}")
fi

for src_cmd in "${src_cmds[@]}" ; do
	src_dir="${src_cmd//[^-a-zA-Z0-9]/_}"
	img_src="${temp_dir}/${src_dir}"
	img_prefix="`basename \"${img_src}\"`-"
	echo "$0: running \"${src_cmd}\" to create \"${img_src}\"..."
	eval "${src_cmd} \"${img_src}\""
	echo "$0: ... ok, starting tests..."
	
	mk_path="${build_dir}/microfsmki"
	ck_path="${build_dir}/microfscki"
	
	for options in "${all_options[@]}" ; do
		mk_cmd="${mk_path} ${options}"
		ck_cmd="${ck_path} -v -e"
		for data_option in "${data_options[@]}" ; do
			image_params=(
				"-t microfs"
				"-m \"${mk_cmd}\""
				"-c \"${ck_cmd}\""
				"-s \"${img_src}\""
				"-d \"${temp_dir}\""
				"-p \"${img_prefix}\""
				"-x \"-x\""
				"-i \"${src_cmd}\""
				"-r \"${conf_randomseed}\""
				"-C \"${conf_checksum}\""
				"-S \"${conf_stresstest}\""
				"${data_option}"
			)
			image_cmd="${script_dir}/tools/imgmkckver.sh ${image_params[@]}"
			echo "$0: running image command \"${image_cmd}\"..."
			eval "${image_cmd}"
			echo "$0: ... ok (`date +'%H:%M:%S'`)."
			snore 1s 1 "$0: waiting for a new second"
		done
		find "${temp_dir}" -name "${img_prefix}*" -not -type d -exec rm -rf {} \;
	done
	
	echo "$0: tests passed for source generated by \"${src_cmd}\"."
	echo "$0: cleaning up and moving on."
	rm -rf "${img_src}"
done

echo "$0: lkm and hostprog tests passed."

_check_end_time=`date +%s`
_check_total_time=`expr $_check_end_time - $_check_start_time`

echo ""
echo "$0: functional tests passed."
echo "$0: execution time: ${_check_total_time} sec."
echo ""

exit 0

