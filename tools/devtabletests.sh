#!/bin/bash

# Copyright (C) 2012, 2013, 2014, 2015, 2016, 2017
# Erik Edlund <erik.edlund@32767.se>
# 
# Redistribution and use in source and binary forms, with or
# without modification, are permitted provided that the
# following conditions are met:
# 
#  * Redistributions of source code must retain the above
#  copyright notice, this list of conditions and the following
#  disclaimer.
# 
#  * Redistributions in binary form must reproduce the above
#  copyright notice, this list of conditions and the following
#  disclaimer in the documentation and/or other materials
#  provided with the distribution.
# 
#  * Neither the name of Erik Edlund, nor the names of its
#  contributors may be used to endorse or promote products
#  derived from this software without specific prior written
#  permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
# CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
# INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
# OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

source "boilerplate.sh"

script_path=`readlink -f "$0"`
script_dir=`dirname "${script_path}"`

temp_dir=""
devtable_path=""
mk_cmd=""
filesystem=""

options="t:d:m:f:"
while getopts $options option
do
	case $option in
		t ) temp_dir=$OPTARG ;;
		d ) devtable_path=$OPTARG ;;
		m ) mk_cmd=$OPTARG ;;
		f ) filesystem=$OPTARG ;;
	esac
done

if [[ ! -d "${temp_dir}" || ! -f "${devtable_path}"  || -z "${mk_cmd}" || \
	-z "${filesystem}" ]] ; then
	cat <<EOF
Usage: `basename $0` -t:d:m:f:

Perform device table tests for microfs or cramfs.

    -t <str>    path to the temp dir
    -d <str>    path to the source dir
    -m <str>    make command
    -f <str>    filesystem to mount
EOF
	exit 1
fi

img_src="${temp_dir}/dev"
img_path="${img_src}.img"
img_mount="${img_path}.mount"

mkdir "${img_src}"
atexit_0 rmdir "${img_src}"
mkdir "${img_mount}"
atexit_0 rmdir "${img_mount}"

mk_cmd="${mk_cmd} -D \"${devtable_path}\" \"${img_src}\" \"${img_path}\""
mount_cmd="sudo mount -t ${filesystem} -o loop -r \"${img_path}\" \"${img_mount}\""
eval "${mk_cmd}"
atexit_0 rm "${img_path}"
eval "${mount_cmd}"
atexit sudo umount "${img_mount}"

eval "${script_dir}/devtck 24 \"${devtable_path}\" \"${img_mount}\""

exit 0
