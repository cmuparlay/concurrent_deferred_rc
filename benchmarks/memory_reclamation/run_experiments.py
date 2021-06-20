

# Rideable 0 : Unused
# Rideable 1 : SortedUnorderedMap
# Rideable 2 : SortedUnorderedMapRC
# Rideable 3 : LinkList
# Rideable 4 : LinkedListRC
# Rideable 5 : NatarajanTree
# Rideable 6 : NatarajanTreeRC
# Rideable 7 : SortedUnorderedMapRCSS
# Rideable 8 : LinkedListRCSS
# Rideable 9 : NatarajanTreeRCSS
# Test Mode 3 : ObjRetire:u50:range=200K:prefill=100K
# Test Mode 7 : ObjRetire:u10:range=2000:prefill=1000
# Test Mode 8 : ObjRetire:u10:range=200K:prefill=100K
# Test Mode 13 : ObjRetire:u1:range=200K:prefill=100K
# Test Mode 20 : ObjRetire:u10:range=200M:prefill=100M

import os
import sys

import time
import create_graphs as graph

from time import gmtime, strftime

datastructures = ['hashtable', 'list', 'bst']

smr_datastructure = {'hashtable': 1,
                     'list': 3,                    
                     'bst': 5}
rc_datastructures = {'hashtable' : [2, 7],
                     'list' : [4, 8],
                     'bst' : [6, 9],}
wl_num =          {
                   '100-50':1,
                   '1000-50':2,
                   '100K-50':3,
                   '1M-50':4,
                   '10M-50':5,
                   '100-10':6,
                   '1000-10':7,
                   '100K-10':8,
                   '1M-10':9,
                   '10M-10':10,
                   '100-1':11,
                   '1000-1':12,
                   '100K-1':13,
                   '1M-1':14,
                   '10M-1':15,
                   '100M-50':19,
                   '100M-10':20,
                   '100M-1':21,
                   }
experiments = [("list", '1000-10'),
             ("hashtable", '100K-10'),
             ("bst", '100K-10'),
             ("bst", '100M-10'),
             ("bst", '100K-1'),
             ("bst", '100K-50'),]

def tostring(ds, wl):
  return 'exp-'+ds+'-'+wl

def printhelp():
  print("Usage: python3 run_experiments.py exp-[datastructure]-[size]-[update_percent]")
  print('\t[datastructure]: hashtable, list, bst')
  print('\t[size]: 100, 1000, 100K, 1M, 10M, 100M')
  print('\t[update_percent]: 1, 10, 50')
  # print("Possible values for exp-name:\nexp-list-1000-10\nexp-hashtable-100K-10\nexp-bst-100K-10\nexp-bst-100M-10\nexp-bst-100K-1\nexp-bst-100K-50\nall")
  exit(1)

threads = [1, 35, 70, 105, 140, 170, 200] # change this variable to change thread counts

trackers = ["RCU", "Hazard", "HazardOpt -d emptyf=1", "Range_new", "HE", "NIL"]

if len(sys.argv) == 1:
  printhelp()
exp_name = sys.argv[1]

exp_to_run = []
if exp_name == 'all':
  for exp in experiments:
    exp_to_run.append(exp)
else:
  ds = exp_name.split('-')[1]
  size = exp_name.split('-')[2]
  up = exp_name.split('-')[3]
  wl = size + '-' + up
  if wl not in wl_num:
    printhelp()
  exp_to_run.append((ds, wl))

if len(sys.argv) == 3 and sys.argv[2] == "test":
  runtime = 1
  repeats = 1
else:
  runtime = 5 # Change this variable to change runtime (measured in seconds)
  repeats = 3 # Change this variable to chage number of iterations

print("repeats: " + str(repeats))

binary = "./bin/release/main"
preamble = "LD_PRELOAD=`jemalloc-config --libdir`/libjemalloc.so.`jemalloc-config --revision` numactl -i all "

num_experiments = 0
estimated_time = 0

outdir = 'results/'
if not os.path.exists(outdir):
  os.mkdir(outdir)

def convert(seconds): 
    seconds = seconds % (24 * 3600) 
    hour = seconds // 3600
    seconds %= 3600
    minutes = seconds // 60
    seconds %= 60
    return "%dh:%02dm:%02ds" % (hour, minutes, seconds) 

def run(ds, wl, tr, count, outfile): 
  num_runs = 0;
  for th in threads:
    num_threads = max(32, th+1)
    cmd = "NUM_THREADS=" + str(num_threads) + " " + preamble + binary + " -i " + str(runtime) + " -m " + str(wl) + " -r " + str(ds) + " -t " + str(th) + " -v "
    if tr != "":
      cmd += "-d tracker=" + tr
    if not count:
      print(cmd)
    for i in range(repeats):
      if count:
        num_runs += 1
      else:
        os.system(cmd + " >> " + outdir+outfile)
        # if os.system(cmd + " >> " + outdir+outfile) != 0:
        #   print("")
        #   exit(1)          
  return num_runs

# start = time.time()
progress = 0

for count in (1, 0):
  if count == 0:
    print("Running " + str(num_experiments) + " experiments in " + convert(num_experiments*runtime))
    # break;
  for exp in exp_to_run:
    ds = exp[0]
    wl = exp[1]
    outfile = tostring(ds, wl) + '.out'
    os.system(" > " + outdir+outfile)
    for rc_ds in rc_datastructures[ds]:
      num_experiments += run(rc_ds, wl_num[wl], "", count, outfile)
      if count == 0:
        progress += len(threads)*repeats
        print(str(progress) + " out of " + str(num_experiments))
    for tr in trackers:
      num_experiments += run(smr_datastructure[ds], wl_num[wl], tr, count, outfile)
      if count == 0:
        progress += len(threads)*repeats
        print(str(progress) + " out of " + str(num_experiments))

# create graphs

for exp in exp_to_run:
  graph.graph_results_from_file(tostring(exp[0],exp[1]))