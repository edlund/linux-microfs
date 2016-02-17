#!/usr/bin/env python
# coding: UTF-8

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

from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals

import argparse
import os

argsparser = argparse.ArgumentParser(
	description="Create power of two sized files.",
	formatter_class=argparse.ArgumentDefaultsHelpFormatter)

argsparser.add_argument(
	"--verbose",
	dest="verbose",
	metavar="",
	help="Be verbose",
	default=False,
	type=bool
)

argsparser.add_argument(
	"--from-shift",
	dest="from_shift",
	metavar="",
	help="Start at the given shift",
	default=9,
	type=int
)

argsparser.add_argument(
	"--to-shift",
	dest="to_shift",
	metavar="",
	help="Stop at the given shift",
	default=22,
	type=int
)

argsparser.add_argument(
	"dirname",
	help="output directory",
	type=str
)

args = argsparser.parse_args()
parent = os.path.dirname(args.dirname)

if args.from_shift > args.to_shift:
	raise ValueError("invalid file size shift range")

os.mkdir(args.dirname)

for shift in range(args.from_shift, args.to_shift):
	file_size = 1 << shift
	for file_src in ['/dev/zero', '/dev/urandom']:
		file_dest = os.path.join(args.dirname, "{size}-{src}.dat".format(
			size=file_size, src=file_src.replace('/', '-')))
		if args.verbose:
			print("{dest}: {size} bytes from {src}".format(dest=file_dest,
				size=file_size, src=file_src))
		with open(file_src, 'r') as src:
			with open(file_dest, 'w') as dest:
				dest.write(src.read(file_size))

exit(0)
