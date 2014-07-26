#!/bin/bash

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

log=""
src=""
mountid=""

options="l:s:m:"
while getopts $options option
do
	case $option in
		l ) log=$OPTARG ;;
		s ) src=$OPTARG ;;
		m ) mountid=$OPTARG ;;
	esac
done

mki_std_log="${log}.mk.1"
mki_err_log="${log}.mk.2"

cki_std_log="${log}.ck.1"
cki_err_log="${log}.ck.2"

sys_log="${log}.syslog"

# Step 1: Strip the path to the script (will fail with strange paths).
# Step 2: Strip command line arguments and suffix.
suffix="${src}"
suffix="`echo "${suffix}" | sed -r 's#/?([-\+\._@ a-z0-9]*/)+##i'`"
suffix="`echo "${suffix}" | grep -Eoi '^([-_a-z0-9]+)'`"

if [[ ! -f "${mki_std_log}" || ! -f "${mki_err_log}" || \
	 ! -f "${cki_std_log}" || ! -f "${cki_err_log}" || \
	 ! ( "${mountid}" =~ ^[-a-zA-Z0-9]+$ ) || \
	 -z "${suffix}" ]] ; then
	cat <<EOF
Usage: `basename $0` -l:s:m:

Perform more testing

    -l <str>    base log path
    -s <str>    source command
    -m <int>    mount id
EOF
	exit 1
fi

# Grab everything from the syslog starting from the notice
# about the given mount id.
dmesg | sed -n -e "/microfs: debug_mountid=${mountid}/,\$p" > "${sys_log}"

ck_path="${script_dir}/logck_${suffix}.sh"

if [ -f "${ck_path}" ] ; then
	source "${ck_path}"
fi

exit 0
