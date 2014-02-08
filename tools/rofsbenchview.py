#!/usr/bin/env python
# coding: UTF-8

# Copyright (C) 2014 Erik Edlund <erik.edlund@32767.se>
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals

import argparse
import csv
import datetime
import decimal
import itertools

class Benchmark(object):
	
	def _process(self, row):
		if len(row) != len(self.columns):
			raise RuntimeError("invalid row")
		filesystem = row[0]
		testname = row[1]
		command = row[2]
		values = { colkey: colval for colkey, colval in zip(self.columns[3:], row[3:]) }
		testkey = (testname, command)
		if not testkey in self.tests:
			self.tests[testkey] = { fs: {} for fs in self.filesystems }
		test = self.tests[testkey]
		for colkey, colval in values.items():
			if not colkey in test[filesystem]:
				test[filesystem][colkey] = []
			duration = datetime.datetime.strptime(colval, "%S.%f") \
				if colval.find(':') < 0 else \
					datetime.datetime.strptime(colval, "%M:%S.%f")
			test[filesystem][colkey].append(datetime.timedelta(
				minutes=duration.minute,
				seconds=duration.second,
				microseconds=duration.microsecond
			).total_seconds())
	
	def __init__(self, args):
		self.verbose = args.verbose == "yes"
		self.tests = {}
		self.filesystems = [
			"cramfs",
			"microfs",
			"squashfs"
		]
		self.columns = [
			"fstype",
			"name",
			"cmd",
			"real",
			"user",
			"sys"
		]
		with open(args.datafile, 'r') as f:
			reader = csv.reader(f)
			for row in reader:
				self._process(row)
	
	def view(self):
		for testkey, filesystems in sorted(self.tests.items()):
			testname, testcommand = testkey
			print("")
			print("Test {0}".format(testname))
			print("Command: `{0}`".format(testcommand))
			for filesystem, values in filesystems.items():
				print("\t{0}: ".format(filesystem), end="")
				for key, timestamps in sorted(values.items()):
					precision = decimal.Decimal(10) ** -4
					average = sum(timestamps) / len(timestamps)
					average = decimal.Decimal(average).quantize(precision)
					print("\t{0}={1}".format(key, average), end="")
				print("")

if __name__ == "__main__":
	argsparser = argparse.ArgumentParser(
		description="View the result of a read-only FS benchmark.",
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
		"datafile",
		help="path to the file generated by rofsbench.sh",
		type=str
	)

	bench = Benchmark(argsparser.parse_args())
	bench.view()
	
	exit(0)