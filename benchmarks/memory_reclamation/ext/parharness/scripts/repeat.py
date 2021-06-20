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
from os.path import dirname, realpath, sep, pardir
import shutil
import csv
from argparse import ArgumentParser
from subprocess import call
import os

os.environ['PATH'] = dirname(realpath(__file__))+\
":" + os.environ['PATH'] # scripts

if __name__ == "__main__":
	retVal = 0
	while(retVal==0):
		cmd = " "
		cmd = cmd.join(sys.argv[1:])
		print cmd
		retVal = os.system(cmd)













