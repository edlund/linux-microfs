#!/bin/bash

# microfs - Minimally Improved Compressed Read Only File System
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

dummy=0
cpus="`getconf _NPROCESSORS_ONLN`"
wd="/tmp"
seed="`date +%s`"
workers="`expr 8 \* ${cpus}`"
threads="${cpus}"
mempercentage="70%"
memvalue=0

options="So:s:w:t:m:M:"
while getopts $options option
do
	case $option in
		S ) dummy=1 ;;
		o ) wd=$OPTARG ;;
		s ) seed=$OPTARG ;;
		w ) workers=$OPTARG ;;
		t ) threads=$OPTARG ;;
		m ) mempercentage=$OPTARG ;;
		M ) memvalue=$OPTARG ;;
	esac
done

shift $(($OPTIND - 1))

action="$1"
name="$2"
mount="$3"

if [[ ! ( "${action}" == "start" || "${action}" == "stop" ) || \
		( "${action}" == "start" && ! -d "${mount}" ) || \
		! ( "${seed}" =~ ^[0-9]+$  && "${workers}" =~ ^[0-9]+$ ) || \
		! ( "${name}" =~ ^[-a-zA-Z0-9]+$ ) || ! -d "${wd}" || \
		! ( "${mempercentage}" =~ ^[0-9]+%$ ) || \
		! ( "${memvalue}" =~ ^[0-9]+$ ) ]] ; then
	cat <<EOF
Usage: `basename $0` [-So:s:w:m:M:] start|stop "alnumname" "/path/to/mount"

Start or stop stress/load testing on the given mount (or
directory).

    -S          dummy flag
    -o <str>    path to work directory
    -s <int>    random seed to use
    -w <int>    number of worker processes to use
    -m <str>    memory drain percentage
    -M <int>    memory drain value (in bytes)
EOF
	exit 1
fi

marker="${wd}/${name}"

source "${script_dir}/stress_${action}.sh"

exit 0
