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

dir_a=""
dir_b=""
write_err=""
write_path=""
checksum_prog="sha512sum"

options="ew:a:b:c:"
while getopts $options option
do
	case $option in
		e ) write_err="1" ;;
		w ) write_path=$OPTARG ;;
		a ) dir_a=$OPTARG ;;
		b ) dir_b=$OPTARG ;;
		c ) checksum_prog=$OPTARG ;;
	esac
done

if [[ ! -d "${dir_a}" || ! -d "${dir_b}" || \
	( "${write_path}" == "" && "${write_err}" != "" ) || \
	"${checksum_prog}" == "" ]] ; then
	cat <<EOF
Usage: `basename $0` -a:b: [-c:w:e]

Diff two directory trees by comparing sha512 checksums and
stat info.

    -a <str>   path to the first dir
    -b <str>   path to the second dir
    -w <str>   path to where the result should be saved
    -c <str>   name of the checksum program to use
    -e         only write info files when an error occurs (depends on -w)
EOF
	exit 1
fi

examine() {
	local wd="`pwd`"
	cd $1
	find . -type f -not -empty -exec ${checksum_prog} {} \; | sort
	find . -not -empty -exec stat -c '{} %A %U:%G' {} \; | sort
	cd $wd
}

dir_a_info=`examine ${dir_a}`
dir_b_info=`examine ${dir_b}`

untrap_ERR
diff=`diff -Nu <(echo "${dir_a_info}" ) <(echo "${dir_b_info}")`
trap_ERR

if [[ ! -z "${write_path}" && ( "${write_err}" == "" || \
	( "${write_err}" != "" && "${diff}" != "" ) ) ]] ; then
	dir_a=`basename $dir_a`
	dir_b=`basename $dir_b`
	dir_a="${dir_a//[^-a-zA-Z0-9]/_}"
	dir_b="${dir_b//[^-a-zA-Z0-9]/_}"
	echo "${dir_a_info}" > "${write_path}/${dir_a}.txt"
	echo "${dir_b_info}" > "${write_path}/${dir_b}.txt"
	echo "${diff}" > "${write_path}/${dir_a}---${dir_b}.diff"
fi

if [ "${diff}" != "" ] ; then
	echo "${diff}"
	exit 1
fi

exit 0
