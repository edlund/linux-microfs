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

export PATH="${PATH}:${script_dir}/sh"

temp_dir=`mktemp -d --tmpdir lut.test.XXXXXXXXXXXXXXXX`
atexit_0 rm -rf "${temp_dir}"
echo "$0: temporary directory used is \"${temp_dir}\""

check_cmptrees() {
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
	eval "${script_dir}/fs/cmptrees.sh -a \"${dir_a}\" -b \"${dir_b}\""
	rm "${dir_a}/fox.txt"
	untrap_ERR
	local diff=`${script_dir}/fs/cmptrees.sh -a \"${dir_a}\" -b \"${dir_b}\"`
	local exit_code=$?
	trap_ERR
	if [[ $exit_code -eq 0 || "${diff}" == "" ]] ; then
		test -z "diff unnoticed"
	fi
	rm -r "${dir_a}"
	rm -r "${dir_b}"
	true
}

check_mkrandtree() {
	local dir_a="${temp_dir}/check_util_mkrandtree-a"
	local dir_b="${temp_dir}/check_util_mkrandtree-b"
	local random_seed="--random-seed=1"
	local size_budget="--size-budget=16777216"
	"${script_dir}/fs/mkrandtree.py" $random_seed $size_budget "${dir_a}"
	"${script_dir}/fs/mkrandtree.py" $random_seed $size_budget "${dir_b}"
	"${script_dir}/fs/cmptrees.sh" -a "${dir_a}" -b "${dir_b}" -w "${temp_dir}" -e
	rm -r "${dir_a}"
	rm -r "${dir_b}"
}

check_mkholedir() {
	local dir_a="${temp_dir}/check_util_mkholedir-a"
	local dir_b="${temp_dir}/check_util_mkholedir-b"
	local random_seed="--random-seed=0"
	"${script_dir}/fs/mkholedir.py" $random_seed "${dir_a}"
	"${script_dir}/fs/mkholedir.py" $random_seed "${dir_b}"
	"${script_dir}/fs/cmptrees.sh" -a "${dir_a}" -b "${dir_b}" -w "${temp_dir}" -e
	rm -r "${dir_a}"
	rm -r "${dir_b}"
}

check_cmptrees
check_mkrandtree
check_mkholedir

exit 0

