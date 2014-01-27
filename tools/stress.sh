#!/bin/bash

# microfs - Minimally Improved Compressed Read Only File System
# Copyright (C) 2014 Erik Edlund <erik.edlund@32767.se>
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

wd="/tmp"
seed="`date +%s`"
workers="32"

options="o:s:w:"
while getopts $options option
do
	case $option in
		o ) wd=$OPTARG ;;
		s ) seed=$OPTARG ;;
		w ) workers=$OPTARG ;;
	esac
done

shift $(($OPTIND - 1))

action="$1"
name="$2"
mount="$3"

if [[ ! ( "${action}" == "start" || "${action}" == "stop" ) || \
		( "${action}" == "start" && ! -d "${mount}" ) || \
		! ( "${seed}" =~ ^[0-9]+$  && "${workers}" =~ ^[0-9]+$ ) || \
		! ( "${name}" =~ ^[-a-zA-Z0-9]+$ ) || ! -d "${wd}" ]] ; then
	cat <<EOF
Usage: `basename $0` [-o:s:w:] start|stop "alnumname" "/path/to/mount"

Start or stop stress/load testing on the given mount (or
directory).

    -o <str>    path to work directory
    -s <int>    random seed to use
    -w <int>    number of worker processes to use
EOF
	exit 1
fi

marker="${wd}/${name}"

source "${script_dir}/stress_${action}.sh"

exit 0
