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

script_path=`readlink -f "$0"`
script_dir=`dirname "${script_path}"`
source "${script_dir}/sh/boilerplate.sh"

action="install"
location="/usr/local/bin"

options="iup:"
while getopts $options option
do
	case $option in
		i ) action="install" ;;
		u ) action="uninstall" ;;
		p ) location=$OPTARG ;;
	esac
done

if [[ ! ( "${action}" == "install" || "${action}" == "uninstall" ) || \
		! -d "${location}" ]] ; then
	cat <<EOF
Usage: `basename $0` -iup:

Install/Uninstall lut.

    -i          install lut
    -u          uninstall lut
    -p <str>    path to install to or uninstall from
EOF
	exit 1
fi

tools=(
	"fs/cmptrees.sh"
	"fs/mkemptydir.sh"
	"fs/mkholedir.py"
	"fs/mkhuskdir.sh"
	"fs/mklndir.sh"
	"fs/mkmbdentdir.sh"
	"fs/mkpow2dir.py"
	"fs/mkrandtree.py"
	"fs/mktpldir.sh"
	"lk/kbuild.sh"
	"lk/logck.sh"
	"lk/logwatch.sh"
	"sh/boilerplate.sh"
)

for tool in "${tools[@]}" ; do
	if [[ "${action}" == "install" ]] ; then
		cp "${script_dir}/${tool}" "${location}"
	else
		rm "${location}/`basename ${tool}`"
	fi
done

exit 0

