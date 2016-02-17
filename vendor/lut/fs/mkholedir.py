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
import random
import sys
import time

argsparser = argparse.ArgumentParser(
	description="Create a limited number of small \"zeroed\" files.",
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
	"--random-seed",
	dest="random_seed",
	metavar="",
	help="seed the random generator (default: time.time())",
	default=int(time.time()),
	type=int
)

argsparser.add_argument(
	"dirname",
	help="output directory",
	type=str
)

args = argsparser.parse_args()
parent = os.path.dirname(args.dirname)

os.mkdir(args.dirname)
random.seed(args.random_seed)

print("dirname: {0}".format(args.dirname))
print("random seed: {0}".format(args.random_seed))

with open('/dev/zero', 'r') as zero:
	for n in range(0, random.randint(8, 16)):
		file_size = random.randint(1, 1 << 22)
		file_dest = "{name}/{n}.zero".format(name=args.dirname, n=n)
		with open(file_dest, 'w') as dest:
			dest.write(zero.read(file_size))
		if args.verbose:
			print("{dest}: {size} bytes".format(dest=file_dest,
				size=file_size))

exit(0)
