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

Create a directory containing dentries with multibyte
character names.
EOF
	exit 1
fi

mkdir -p "$1"

cat << 'EOF' > "$1/φυσική"
Physics (from Greek φυσική (ἐπιστήμη), i.e. "knowledge of
nature", from φύσις, physis, i.e. "nature") is the natural
science that involves the study of matter and its motion
through space and time, along with related concepts such as
energy and force. More broadly, it is the general analysis of
nature, conducted in order to understand how the universe
behaves.
EOF

cat << 'EOF' > "$1/verschränkung"
Quantum entanglement is a physical phenomenon that occurs
when particles such as photons, electrons, or molecules the
size of buckyballs or small diamonds, interact and then become
separated.
EOF

mkdir -p "$1/παράδοξος"

cat << 'EOF' > "$1/παράδοξος/schrödinger's_😻"
Schrödinger's cat is a thought experiment, sometimes described
as a paradox, devised by Austrian physicist Erwin Schrödinger
(12 August 1887 – 4 January 1961) in 1935.
EOF

mkdir -p "$1/²"

echo "1" > "$1/²/1²"
echo "4" > "$1/²/2²"
echo "9" > "$1/²/3²"
