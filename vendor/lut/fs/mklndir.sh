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

Create a directory containing hard and symbolic links.
EOF
	exit 1
fi

mkdir -p "$1"

dd count=1 bs=1024 "if=/dev/urandom" "of=$1/rand-0.dat"
ln "$1/rand-0.dat" "$1/hardlink-rand-0.dat"
ln -s "$1/rand-0.dat" "$1/symboliclink-rand-0.dat"

mkdir -p "$1/a"
mkdir -p "$1/b"

ln "$1/rand-0.dat" "$1/a/hardlink-rand-0.dat"
ln "$1/rand-0.dat" "$1/b/hardlink-rand-0.dat"

ln -s "$1/a" "$1/symboliclink-a"
ln -s "$1/b" "$1/symboliclink-b"

dd count=1 bs=1024 "if=/dev/urandom" "of=$1/a/rand-1.dat"
dd count=1 bs=1024 "if=/dev/urandom" "of=$1/b/rand-2.dat"

ln "$1/a/rand-1.dat" "$1/hardlink-rand-1.dat"
ln -s "$1/a/rand-1.dat" "$1/symboliclink-rand-1.dat"

ln "$1/b/rand-2.dat" "$1/a/hardlink-rand-2.dat"
ln -s "$1/b/rand-2.dat" "$1/a/symboliclink-rand-2.dat"
ln -s "$1/b/rand-2.dat" "$1/b/symboliclink-rand-2.dat"

ln -P "$1/symboliclink-rand-0.dat" "$1/hardlink-symboliclink-rand-0.dat"
ln -P "$1/symboliclink-a" "$1/hardlink-symboliclink-a"
ln -P "$1/symboliclink-b" "$1/hardlink-symboliclink-b"

content="The content of this file is not important."

echo "${content}" > "$1/c-1.txt"
echo "${content}" > "$1/c-2.txt"
echo "${content}" > "$1/a/c-3.txt"
echo "${content}" > "$1/b/c-4.txt"
