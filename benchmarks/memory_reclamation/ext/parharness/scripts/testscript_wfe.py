#!/usr/bin/python


# Copyright 2019 Ruslan Nikolaev

# Based on original testscript.py from IBR
# Copyright 2017 University of Rochester

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

# http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License. 



from os.path import dirname, realpath, sep, pardir
import sys
import os

# path loading ----------
#print dirname(realpath(__file__))
os.environ['PATH'] = dirname(realpath(__file__))+\
":" + os.environ['PATH'] # scripts
os.environ['PATH'] = dirname(realpath(__file__))+\
"/..:" + os.environ['PATH'] # bin
os.environ['PATH'] = dirname(realpath(__file__))+\
"/../../../bin:" + os.environ['PATH'] # metacmd

#"/../../cpp_harness:" + os.environ['PATH'] # metacmd

# execution ----------------
for i in range(0,5):
	cmd = "metacmd.py main -i 10 -m 3 -v -r 1"+\
	" --meta t:1:8:16:24:32:40:48:56:64:72:80:88:96:104:112:120"+\
	" --meta d:tracker=NIL:tracker=RCU:tracker=Range_new:tracker=HE:tracker=Hazard:tracker=WFE"+\
	" -o data/final/hashmap_result.csv"
	os.system(cmd)
	cmd = "metacmd.py main -i 10 -m 3 -v -r 2"+\
	" --meta t:1:8:16:24:32:40:48:56:64:72:80:88:96:104:112:120"+\
	" --meta d:tracker=NIL:tracker=RCU:tracker=Range_new:tracker=HE:tracker=Hazard:tracker=WFE"+\
	" -o data/final/list_result.csv"
	os.system(cmd)
	cmd = "metacmd.py main -i 10 -m 3 -v -r 4"+\
	" --meta t:1:8:16:24:32:40:48:56:64:72:80:88:96:104:112:120"+\
	" --meta d:tracker=NIL:tracker=RCU:tracker=Range_new:tracker=HE:tracker=Hazard:tracker=WFE"+\
	" -o data/final/natarajan_result.csv"
	os.system(cmd)
	cmd = "metacmd.py main -i 10 -m 2 -v -r 1"+\
	" --meta t:1:8:16:24:32:40:48:56:64:72:80:88:96:104:112:120"+\
	" --meta d:tracker=NIL:tracker=RCU:tracker=Range_new:tracker=HE:tracker=Hazard:tracker=WFE"+\
	" -o data/final/hashmap_result_read.csv"
	os.system(cmd)
	cmd = "metacmd.py main -i 10 -m 2 -v -r 2"+\
	" --meta t:1:8:16:24:32:40:48:56:64:72:80:88:96:104:112:120"+\
	" --meta d:tracker=NIL:tracker=RCU:tracker=Range_new:tracker=HE:tracker=Hazard:tracker=WFE"+\
	" -o data/final/list_result_read.csv"
	os.system(cmd)
	cmd = "metacmd.py main -i 10 -m 2 -v -r 4"+\
	" --meta t:1:8:16:24:32:40:48:56:64:72:80:88:96:104:112:120"+\
	" --meta d:tracker=NIL:tracker=RCU:tracker=Range_new:tracker=HE:tracker=Hazard:tracker=WFE"+\
	" -o data/final/natarajan_result_read.csv"
	os.system(cmd)
	cmd = "metacmd.py main -i 10 -m 3 -v -r 9"+\
	" --meta t:1:8:16:24:32:40:48:56:64:72:80:88:96:104:112:120"+\
	" --meta d:tracker=NIL:tracker=RCU:tracker=Range_new:tracker=HE:tracker=Hazard:tracker=WFE"+\
	" -o data/final/wfqueue_result.csv"
	os.system(cmd)
	cmd = "metacmd.py main -i 10 -m 3 -v -r 10"+\
	" --meta t:1:8:16:24:32:40:48:56:64:72:80:88:96:104:112:120"+\
	" --meta d:tracker=NIL:tracker=RCU:tracker=Range_new:tracker=HE:tracker=Hazard:tracker=WFE"+\
	" -o data/final/crturnqueue_result.csv"
	os.system(cmd)
