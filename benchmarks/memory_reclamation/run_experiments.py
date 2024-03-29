# Rideable 0 : Unused
# Rideable 1 : SortedUnorderedMap
# Rideable 2 : SortedUnorderedMapRC
# Rideable 3 : SortedUnorderedMapRCHP
# Rideable 4 : SortedUnorderedMapRCEBR
# Rideable 5 : SortedUnorderedMapRCIBR
# Rideable 6 : SortedUnorderedMapRCHyaline
# Rideable 7 : LinkList
# Rideable 8 : LinkedListRC
# Rideable 9 : LinkListRCHP
# Rideable 10 : LinkListRCEBR
# Rideable 11 : LinkListRCIBR
# Rideable 12 : LinkListRCHyaline
# Rideable 13 : NatarajanTree
# Rideable 14 : NatarajanTreeRC
# Rideable 15 : NatarajanTreeRCHP
# Rideable 16 : NatarajanTreeRCEBR
# Rideable 17 : NatarajanTreeRCIBR
# Rideable 18 : NatarajanTreeRCHyaline

# Test Mode 0 : SequentialRemoveTest:prefill=20K
# Test Mode 1 : ObjRetire:u50:range=200:prefill=100
# Test Mode 2 : ObjRetire:u50:range=2000:prefill=1000
# Test Mode 3 : ObjRetire:u50:range=200K:prefill=100K
# Test Mode 4 : ObjRetire:u50:range=2M:prefill=1M
# Test Mode 5 : ObjRetire:u50:range=20M:prefill=10M
# Test Mode 6 : ObjRetire:u10:range=200:prefill=100
# Test Mode 7 : ObjRetire:u10:range=2000:prefill=1000
# Test Mode 8 : ObjRetire:u10:range=200K:prefill=100K
# Test Mode 9 : ObjRetire:u10:range=2M:prefill=1M
# Test Mode 10 : ObjRetire:u10:range=20M:prefill=10M
# Test Mode 11 : ObjRetire:u1:range=200:prefill=100
# Test Mode 12 : ObjRetire:u1:range=2000:prefill=1000
# Test Mode 13 : ObjRetire:u1:range=200K:prefill=100K
# Test Mode 14 : ObjRetire:u1:range=2M:prefill=1M
# Test Mode 15 : ObjRetire:u1:range=20M:prefill=10M
# Test Mode 16 : ObjRetire:u50:range=8192:prefill=4096
# Test Mode 17 : ObjRetire:u10:range=8192:prefill=4096
# Test Mode 18 : ObjRetire:u1:range=8192:prefill=4096
# Test Mode 19 : ObjRetire:u50:range=200M:prefill=100M
# Test Mode 20 : ObjRetire:u10:range=200M:prefill=100M
# Test Mode 21 : ObjRetire:u1:range=200M:prefill=100M
# Test Mode 22 : SequentialRemoveTest:prefill=0
# Test Mode 23 : SequentialRemoveTest:prefill=1
# Test Mode 24 : ObjRetire:u50rq50:range=200K:prefill=100K
# Test Mode 25 : ObjRetire:u50rq50:range=2M:prefill=1M
# Test Mode 26 : ObjRetire:u50rq50:range=20M:prefill=10M
# Test Mode 27 : ObjRetire:u50rq50:range=200M:prefill=100M




import multiprocessing
import os
import sys
import subprocess

import create_graphs as graph

datastructures = ['hashtable', 'list', 'bst']

smr_datastructure = {'hashtable': 1,
                     'list': 7,
                     'bst': 13}
rc_datastructures = {'hashtable' : [3, 4, 5, 6],
                     'list' : [9,10,11,12],
                     'bst' : [15,16,17,18],}
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
                   '100K-50rq':24,
                   '1M-50rq':25,
                   '10M-50rq':26,
                   '100M-50rq':27,
                   }
experiments = [("list", '1000-10'),
             ("hashtable", '100K-10'),
             ("bst", '100K-10'),
             ("bst", '100M-10'),
             ("bst", '100K-1'),
             ("bst", '100K-50'),]

graphs_only = False
test_mode = False

if len(sys.argv) == 3 and sys.argv[2] == "test":
  runtime = 1
  repeats = 1
  test_mode = True
else:
  runtime = 5 # Change this variable to change runtime (measured in seconds)
  repeats = 3 # Change this variable to chage number of iterations


def tostring(ds, wl):
  return 'exp-'+ds+'-'+wl

def printhelp():
  print("Usage: python3 run_experiments.py exp-[datastructure]-[size]-[update_percent] optional_filename_tag")
  print('\t[datastructure]: hashtable, list, bst')
  print('\t[size]: 100, 1000, 100K, 1M, 10M, 100M')
  print('\t[update_percent]: 1, 10, 50')
  # print("Possible values for exp-name:\nexp-list-1000-10\nexp-hashtable-100K-10\nexp-bst-100K-10\nexp-bst-100M-10\nexp-bst-100K-1\nexp-bst-100K-50\nall")
  exit(1)

# Compute thread counts to use for experiments
P = multiprocessing.cpu_count()
if not test_mode:
  p_step_size = P // 5
  threads = [1] + [k*p_step_size for k in range(1,5)] + [P] + [P + k*p_step_size for k in range(1,3)]
else:
  threads = [1, P // 2, P, P + P // 2]


trackers = ["HazardOpt -d emptyf=2", "Hyaline -d emptyf=2", "RCU -d epochf=10 -d emptyf=2", "Range_new -d epochf=40 -d emptyf=2"]

if len(sys.argv) == 1:
  printhelp()
exp_name = sys.argv[1]
filename_tag = ''
if len(sys.argv) > 2:
  filename_tag = '-' + sys.argv[2]

exp_to_run = []
if exp_name == 'all':
  for exp in experiments:
    exp_to_run.append(exp)
else:
  ds = exp_name.split('-')[1]
  size = exp_name.split('-')[2]
  up = exp_name.split('-')[3]
  if 'rq' in up:
    trackers_copy = []
    for tr in trackers:
      if 'HazardOpt' not in tr:
        trackers_copy.append(tr)
    trackers = trackers_copy
  wl = size + '-' + up
  if wl not in wl_num:
    printhelp()
  exp_to_run.append((ds, wl))


# Returns True if the given command returns a non-zero return code,
# i.e., is probably a valid command
def valid_command(cmd):
  DEVNULL = open(os.devnull, 'r+b', 0)
  returncode = subprocess.call([cmd], stdin=DEVNULL, stdout=DEVNULL, stderr=DEVNULL, shell=True)
  return returncode == 0

binary = "./bin/release/main"
preamble = ""

if valid_command('jemalloc-config --libdir'):
    preamble += "LD_PRELOAD=`jemalloc-config --libdir`/libjemalloc.so.`jemalloc-config --revision` "
else:
    print('Note: Not dynamically linking jemalloc because jemalloc could not be found')
if valid_command('numactl -i all echo'):
    preamble += " numactl -i all "
else:
    print('Note: Not running with NUMA since numactl is not installed or is not valid on this system')

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
  return num_runs


progress = 0

if not graphs_only:
  for count in (1, 0):
    if count == 0:
      print("Running " + str(num_experiments) + " experiments in " + convert(num_experiments*runtime))
      print('\tthread counts: {}'.format(', '.join(map(str, threads))))
      print("\truntime: " + str(runtime))
      print("\trepeats: " + str(repeats))
    for exp in exp_to_run:
      ds = exp[0]
      wl = exp[1]
      outfile = tostring(ds, wl) + filename_tag + '.out'
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
  graph.graph_results_from_file(tostring(exp[0],exp[1]), filename_tag)
