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

if (( $# != 1 )) ; then
	cat <<EOF
Usage: `basename $0` dirname

Create a directory containing only empty directories.
EOF
	exit 1
fi

mkdir -p "$1" && cd "$1"
mkdir -p {a,b}/{c,d,e}/{f,g,h,i}
