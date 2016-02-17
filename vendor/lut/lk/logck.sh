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

log=""
script=""
mountid=""

options="l:s:m:"
while getopts $options option
do
	case $option in
		l ) log=$OPTARG ;;
		s ) script=$OPTARG ;;
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
suffix="${script}"
suffix="`echo "${suffix}" | sed -r 's#/?([-\+\._@ a-z0-9]*/)+##i'`"
suffix="`echo "${suffix}" | grep -Eoi '^([-_a-z0-9]+)'`"

if [[ ! -f "${mki_std_log}" || ! -f "${mki_err_log}" || \
		! -f "${cki_std_log}" || ! -f "${cki_err_log}" || \
		! ( "${mountid}" =~ ^[[:space:][:punct:]a-zA-Z0-9]+$ ) || \
		-z "${suffix}" ]] ; then
	cat <<EOF
Usage: `basename $0` -l:s:m:

Perform more testing

    -l <str>    base log path
    -s <str>    script suffix, often a shell command
    -m <int>    mount id or other unique identifier
EOF
	exit 1
fi

# Grab everything from the syslog starting from the notice
# about the given mount id.
dmesg | sed -n -e "/${mountid}/,\$p" > "${sys_log}"

ck_path="${script_dir}/logck_${suffix}.sh"

if [ -f "${ck_path}" ] ; then
	source "${ck_path}"
fi

exit 0
