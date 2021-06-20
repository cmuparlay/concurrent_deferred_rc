#!/usr/bin/python


# Copyright 2015 University of Rochester
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
# http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License. 




import sys 
import shutil
import csv
from argparse import ArgumentParser
from subprocess import call
import os

def isInt(s):
    try:
        int(s)
        return True
    except ValueError:
        return False

def parseCommandLine():
	parser = ArgumentParser("metacmd.py CMD OPTION1 OPTION2 [--meta METAOPTION:VALUE1:VALUE2:INT1...INT2] [-h] [--printOnly]\
\nMetacmd will in invoke CMD on all combinations of meta options and their values.  Non meta options will be passed as is to the command.")	
	parser.add_argument("--meta", action="append",dest="metas",help="meta options to iterate over. Flag comes first in colon separated list.  Can use ellipses in between integers to represent all integers in the range.", metavar="OPTION:VALUE1,VALUE2,VALUE3", default = [])
	parser.add_argument("--printOnly", action="store_true",dest="printOnly",help="show commands, but don't actually execute them", default = False)
	(options, remains) = parser.parse_known_args()

	print "Metas ", options.metas
	print "Remaining arguments (passed to command) ", remains

	for meta in options.metas:
		idx = meta.find(":")
		if(idx==-1):
			print "Error on: ", meta
			sys.exit("Every meta-option must have colon to separate option from values.")
		if(idx==len(meta)-1):
			print "Error on: ", meta
			sys.exit("Meta-option cannot end with colon.")

	return options, remains

def parseMetaOption(meta):
	idx = meta.find(":")
	opt = meta[:idx]
	if(idx==0):
		opt = ""
	if(len(opt)==1):
		opt = "-"+opt
	if(len(opt)>=1 and opt.find("-")==-1):
		opt = "--"+opt
	return opt

def parseMetaValues(meta):
	idx = meta.find(":")
	vals = meta[idx+1:]
	vals = vals.split(":")
	nextVals = []
	for val in vals:
		eidx = val.find("...")
		if(eidx!=-1):
			if(eidx==len(val)-3):
				print "Error on: ", meta
				sys.exit("Ellipses(...) cannot end metavalue list.")
			if(eidx==0):
				print "Error on: ", meta
				sys.exit("Ellipses(...) cannot start metavalue list.")
			before = val[:eidx]
			after = val[eidx+3:]
			if(isInt(before) and isInt(after)):
				for i in range(int(before),int(after)+1):
					nextVals.append(str(i))
			else:
				print "Error on: ", meta
				sys.exit("Ellipses(...) must be in between two integers.")
		else:
			nextVals.append(val)
	return nextVals

if __name__ == "__main__":

	# parse command line and populate meta options
	options, remains = parseCommandLine()
	metaDict = {}
	for meta in options.metas:
		idx = meta.find(":")
		opt = parseMetaOption(meta)
		vals = parseMetaValues(meta)

		if(opt in metaDict):
			metaDict[opt].append(vals)
		else:
			metaDict[opt]=vals

	# iterate over metaDict for all combos
	combos = [""]
	nextCombos = []
	for opt, vals in metaDict.iteritems():
		for s in combos:
			for v in vals:
				nextCombos.append(s+' '+opt+' '+v)
		combos = nextCombos
		nextCombos = []

	# finish all combos with common options
	commands = []
	for combo in combos:
		s = " ".join(remains)
		s = s + ' '+combo
		commands.append(s)

	if(options.printOnly):
		print "These commands would be run if --printOnly flag was dropped:"

	for command in commands:
		print command
		if(not options.printOnly):
			result = os.system(command)
			if(result!=0):
				sys.exit(0)
			print '\n'














