#!/bin/bash

# Copyright (C) 2015 Erik Edlund <erik.edlund@32767.se>
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

dir_a=""
dir_b=""
write_err=""
write_path=""
diff_name=""
checksum_prog="sha512sum"

options="ew:a:b:n:c:"
while getopts $options option
do
	case $option in
		e ) write_err="1" ;;
		w ) write_path=$OPTARG ;;
		a ) dir_a=$OPTARG ;;
		b ) dir_b=$OPTARG ;;
		n ) diff_name=$OPTARG ;;
		c ) checksum_prog=$OPTARG ;;
	esac
done

if [[ ! -d "${dir_a}" || ! -d "${dir_b}" || \
	( "${write_path}" == "" && "${write_err}" != "" ) || \
	( "${write_path}" != "" && ! -d "${write_path}" ) || \
	"${checksum_prog}" == "" ]] ; then
	cat <<EOF
Usage: `basename $0` -a:b: [-c:w:e]

Diff two directory trees by comparing sha512 checksums and
stat info.

    -a <str>   path to the first dir
    -b <str>   path to the second dir
    -w <str>   path to where the result should be saved
    -n <str>   name for the diff file (without extension)
    -c <str>   name of the checksum program to use
    -e         only write info files when an error occurs (depends on -w)
EOF
	exit 1
fi

examine() {
	local wd="`pwd`"
	cd "$1"
	find . -type f -not -empty -exec ${checksum_prog} {} \; | sort
	find . -not -empty -exec stat -c '{} %A %U:%G' {} \; | sort
	cd "$wd"
}

dir_a_info="`examine "${dir_a}"`"
dir_b_info="`examine "${dir_b}"`"

untrap_ERR
diff="`diff -Nu <(echo "${dir_a_info}") <(echo "${dir_b_info}")`"
trap_ERR

if [[ ! -z "${write_path}" && ( "${write_err}" == "" || \
	( "${write_err}" != "" && "${diff}" != "" ) ) ]] ; then
	dir_a=`basename $dir_a`
	dir_b=`basename $dir_b`
	dir_a="${dir_a//[^-a-zA-Z0-9]/_}"
	dir_b="${dir_b//[^-a-zA-Z0-9]/_}"
	if [[ "${diff_name}" == "" ]] ; then
		diff_path="${write_path}/${dir_a}---${dir_b}.diff"
	else
		diff_name="${diff_name//[^-a-zA-Z0-9]/_}"
		diff_path="${write_path}/${diff_name}.diff"
	fi
	echo "${dir_a_info}" > "${write_path}/a-${dir_a}.txt"
	echo "${dir_b_info}" > "${write_path}/b-${dir_b}.txt"
	echo "${diff}" > "${diff_path}"
fi

if [ "${diff}" != "" ] ; then
	echo "${diff}"
	exit 1
fi

exit 0
