#!/usr/bin/env python
# coding: UTF-8

# Copyright (C) 2012, 2013, 2014, 2015, 2016, 2017
# Erik Edlund <erik.edlund@32767.se>
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
import datetime
import errno
import os
import time

def newchild(basepath):
	if not os.path.exists(basepath):
		raise ValueError("basepath does not exist")
	path = basepath
	while os.path.exists(path):
		path = os.path.join(basepath, "{sec}.{usec}".format(sec=time.time(),
			usec=datetime.datetime.now().microsecond))
	return path

def rofail(path, func):
	try:
		func(path)
	except (IOError, OSError) as error:
		if error.errno != errno.EROFS:
			raise
	else:
		raise IOError("func {name} did not fail".format(name=func))

argsparser = argparse.ArgumentParser(
	description="Try to modify a read-only mounted image.",
	formatter_class=argparse.ArgumentDefaultsHelpFormatter)

argsparser.add_argument(
	"mntdir",
	help="mount directory",
	type=str
)

args = argsparser.parse_args()
basepath, dirs, files = os.walk(args.mntdir).next()

rofail(newchild(basepath), lambda p: open(p, 'w').close())
rofail(newchild(basepath), os.mkdir)

if len(dirs): rofail(os.path.join(basepath, dirs[0]), os.rmdir)
if len(files): rofail(os.path.join(basepath, files[0]), os.unlink)
