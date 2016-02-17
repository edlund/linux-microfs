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
import functools
import itertools
import os
import random
import string
import sys
import time

# http://stackoverflow.com/a/3590105
def constrained_sum_sample_pos(n, total):
	"""Return a randomly chosen list of n positive integers
	summing to total. Each such list is equally likely to occur."""
	dividers = sorted(random.sample(xrange(1, total), n - 1))
	return [a - b for a, b in zip(dividers + [total], [0] + dividers)]

# http://stackoverflow.com/a/3590105
def constrained_sum_sample_nonneg(n, total):
	"""Return a randomly chosen list of n nonnegative integers
	summing to total. Each such list is equally likely to occur."""
	return [x - 1 for x in constrained_sum_sample_pos(n, total + n)]

def generate_name(glyphs, max_length, alpha, beta):
	"""Return a randomly generated name using the given set
	of glyphs. Use alpha and beta to adjust the distribution."""
	part = 1 / max_length
	length = int(random.betavariate(alpha, beta) / part) + 1
	return "".join([random.choice(glyphs) for _ in xrange(0, length)])

def compressable_bytes(n):
	"""Generate n bytes which will compress relatively nicely."""
	randbytes = []
	while len(randbytes) < n:
		randbytes.append(ord(random.choice(string.hexdigits)))
	return bytearray(randbytes)

def uncompressable_bytes(n):
	"""Generate n random bytes which are difficult to compress."""
	randbytes = []
	for i in [random.getrandbits(32) for _ in xrange(int(n / 4) + 1)]:
		for shift in [0, 8, 16, 24]:
			if len(randbytes) < n:
				randbytes.append((i >> shift) & 0xff)
	return bytearray(randbytes)

def create_directories(base_path, current_level, max_level,
		max_subdirs, get_name, verbose=False):
	"""Create a random number of subdirectories (maybe none at
	all) in the given base_path. The probability of subdirectory
	creation decreases as the level of recursion increases."""
	if current_level < 1:
		raise ValueError("current_level must be greater than or equal to 1")
	subdir_paths = [] if current_level > 1 else [base_path]
	if current_level > max_level or 1 != random.randint(1,
			current_level * 2):
		return subdir_paths
	for _ in range(0, random.randint(1, max_subdirs)):
		subdir_path = os.path.join(base_path, get_name())
		while subdir_path in subdir_paths:
			subdir_path = os.path.join(base_path, get_name())
		subdir_paths.append(subdir_path)
		os.mkdir(subdir_path)
		subdir_paths += create_directories(subdir_path, current_level + 1,
			max_level, max_subdirs, get_name)
		if verbose:
			print("dir: {name}".format(name=subdir_path))
	return subdir_paths

def create_files(directories, size_budget, max_file_size,
		get_name, verbose=False):
	"""Create at least one nonempty file in each of the given
	directories. The sum of the size of all created files
	(excluding metadata) will be equal to the given size_budget."""
	if size_budget / len(directories) < 1:
		raise ValueError("The given size_budget is too small")
	file_paths = []
	dir_size_budgets = constrained_sum_sample_pos(len(directories),
		size_budget) if len(directories) > 1 else [size_budget]
	for dir_budget, dir_name in itertools.izip(dir_size_budgets,
			directories):
		while dir_budget:
			maximum = max_file_size if max_file_size < dir_budget else dir_budget
			file_size = random.randint(0, maximum)
			dir_budget -= file_size
			existing_paths = directories + file_paths
			file_path = os.path.join(dir_name, get_name())
			while file_path in existing_paths:
				file_path = os.path.join(dir_name, get_name())
			file_paths.append(file_path)
			file_content = globals()[args.file_content](file_size)
			with open(file_path, 'w') as f:
				f.write(file_content)
			if verbose:
				print("file: {name} ({size} bytes)".format(name=file_path,
					size=file_size))
	return file_paths

if __name__ == "__main__":
	argsparser = argparse.ArgumentParser(
		description="Create a pseudo-random filesystem hierarchy.",
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
		"--name-max-length",
		dest="name_max_length",
		metavar="",
		help="maximim name length returned by generate_name(...)",
		default=255,
		type=int
	)
	argsparser.add_argument(
		"--name-alpha",
		dest="name_alpha",
		metavar="",
		help="alpha value for generate_name(...)",
		default=1.0,
		type=float
	)
	argsparser.add_argument(
		"--name-beta",
		dest="name_beta",
		metavar="",
		help="beta value for generate_name(...)",
		default=7.0,
		type=float
	)
	argsparser.add_argument(
		"--name-glyphs",
		dest="name_glyps",
		metavar="",
		help="available glyphs for generate_name(...)",
		default="abcdefghijklmnopqrstuvwxyz0123456789",
		type=str
	)
	
	argsparser.add_argument(
		"--levels",
		dest="levels",
		metavar="",
		help="the maximum level of sub-directory recursion",
		default=2,
		type=int
	)
	
	argsparser.add_argument(
		"--size-budget",
		dest="size_budget",
		metavar="",
		help="size in bytes for all created files",
		default=134217728,
		type=int
	)
	
	argsparser.add_argument(
		"--max-file-size",
		dest="max_file_size",
		metavar="",
		help="maximum size in bytes a single created file",
		default=16777215,
		type=int
	)
	argsparser.add_argument(
		"--max-sub-dirs",
		dest="max_subdirs",
		metavar="",
		help="maximum number of sub-directories per level",
		default=16,
		type=int
	)
	
	argsparser.add_argument(
		"--file-content",
		dest="file_content",
		metavar="",
		help="the file content generator",
		default="uncompressable_bytes",
		type=str,
		choices=["uncompressable_bytes", "compressable_bytes"]
	)
	
	argsparser.add_argument(
		"dirname",
		help="output directory",
		type=str
	)
	
	args = argsparser.parse_args()
	
	print("dirname: {name}".format(name=args.dirname))
	print("random seed: {seed}".format(seed=args.random_seed))
	print("file content: {generator}".format(generator=args.file_content))
	
	os.mkdir(args.dirname)
	random.seed(args.random_seed)
	
	get_name = functools.partial(generate_name, args.name_glyps,
		args.name_max_length, args.name_alpha, args.name_beta)
	
	directories = create_directories(args.dirname, 1,
		args.levels, args.max_subdirs, get_name, args.verbose)
	files = create_files(directories, args.size_budget,
		args.max_file_size, get_name, args.verbose)
	
	exit(0)
