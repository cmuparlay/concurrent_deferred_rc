import sqlite3
import os.path
from os.path import isdir
from os import mkdir
import numpy as np
import matplotlib as mpl
import statistics as st
import matplotlib as mpl
# mpl.use('Agg')
mpl.rcParams['grid.linestyle'] = ':'
mpl.rcParams.update({'font.size': 18})

import matplotlib.pyplot as plt
#plt.style.use('ggplot')
import sys
import math
import re
import copy

# font = {'weight' : 'normal',
#       'size'   : 16}
# mpl.rc('font', **font)

names = {
    'RCU' : 'EBR',
    'RC'  : 'DRC',
    'RCSS'  : 'DRC (+ snapshot)',
    'Range_new'    : 'IBR',
    'Hazard' : 'HP',
    'HazardOpt' : 'HPopt',
    'HE' : 'HE',
    'NIL' : 'No MM',
}

colors = {
    'RCU' : 'C0',
    'RC'  : 'C1',
    'Range_new'    : 'C2',
    'Hazard' : 'C3',
    'HazardOpt' : 'C4',
    'HE' : 'C7',
    'NIL' : 'C6',
    'RCSS'  : 'C5',
}

markers = {
    'RCU' : 'o',
    'RC'  : 'v',
    'Range_new'    : '^',
    'Hazard' : 's',
    'HazardOpt' : '*',
    'HE' : 'x',
    'NIL' : '1',
    'RCSS'  : 'D',
}

# if len(sys.argv) == 1:
#   results_file = 'ppopp_results_5'
# else:
#   results_file = sys.argv[1]
indir = 'results/'

# results/2020-11-17_08-40-22

# if not os.path.exists(outdir + '/140_threads'):
#   os.mkdir(outdir + '/140_threads')

def avg(l):
  s = 0.0
  for x in l:
    s += x;
  return s/len(l)


def to_string_3(bench, mm, th, result_type):
  return str(mm) + ', ' + str(bench) + ', ' + str(th) + ', ' + str(result_type)

def to_string_benchmark(d):
  return str(d['ds']) + ', ' + 'size:' +  str(d['size']) + ', ' + str(d['workload'])

def to_string2(ds, mm, size, workload, th, result_type):
  return str(mm) + ', ' + str(ds) + ', ' + 'size:' +  str(size) + ', ' + str(workload) + ', ' + str(th) + ', ' + str(result_type)

def to_string(d):
  return to_string2(d['ds'], d['mm'], d['size'], d['workload'], d['th'], d['result_type'])

def to_string_list(int_list):
  return [str(x) for x in int_list]

def firstInt(s):
  for t in s.split():
    try:
      return int(t)
    except ValueError:
      pass
  print(s)
  print('no number in string')

def getAllInts(s):
  ints = []
  for t in s.split():
    try:
      ints.append(int(t))
    except ValueError:
      pass
  # print(s)
  return ints

def firstFloat(s):
  for t in s.split():
    try:
      return float(t)
    except ValueError:
      pass
  print('no number in string')

def getAllFloats(s):
  floats = []
  for t in s.split():
    try:
      floats.append(float(t))
    except ValueError:
      pass
  return floats

def add_result(results, key_dict, val):
  key = to_string(key_dict)
  if key not in results:
    results[key] = []
  results[key].append(val)

def add_benchmark(benchmarks, key_dict):
  key = to_string_benchmark(key_dict)
  if key not in benchmarks:
    benchmarks.append(key)

def format_name(name):
  return name

def create_graph(exp_name, results, stddev, bench, memory_managers, threads, graph_title, yaxis, printLegend=False, errorbar=False, onlyrc=False):
  outdir = 'graphs/'
  if not os.path.exists(outdir):
    os.mkdir(outdir)
  
  this_graph_title = graph_title + '-' + exp_name
  # print()
  print(this_graph_title)

  series = {}
  error = {}
  for alg in memory_managers:
    if yaxis != 'throughput' and alg == 'NIL':
      continue
    if onlyrc and (alg != 'RC' and alg != 'RCSS'):
      continue
    if to_string_3(bench, alg, threads[0], yaxis) not in results:
      continue
    series[alg] = []
    error[alg] = []
    for th in threads:
      if to_string_3(bench, alg, th, yaxis) not in results:
        del series[alg]
        del error[alg]
        break
      if yaxis == 'throughput':
        series[alg].append(results[to_string_3(bench, alg, th, yaxis)])
      else:
        series[alg].append(results[to_string_3(bench, alg, th, yaxis)]/1000.0)
      error[alg].append(stddev[to_string_3(bench, alg, th, yaxis)])
  mn = 99999999
  mx = 0

  # create plot
  fig, axs = plt.subplots(figsize=(6.0, 4.5))
  opacity = 0.8
  rects = {}
   
  offset = 0
  for alg in series:
    if errorbar:
      rects[alg] = axs.errorbar(threads, series[alg], 
                                error[alg], capsize=3, 
                                alpha=opacity,
                                color=colors[alg],
                                #hatch=hatch[ds],
                                # linestyle=linestyles[alg],
                                markersize=10,
                                marker='o',
                                label=names[alg])
    else:
      rects[alg] = axs.plot(threads, series[alg],
                            alpha=opacity,
                            color=colors[alg],
                            #hatch=hatch[ds],
                            markersize=10,
                            # marker='o',
                            marker=markers[alg],
                            label=names[alg])
  
  if yaxis == 'throughput':
    axs.set(xlabel='Number of threads', ylabel='Throughput (Mop/s)')
  elif yaxis == 'allocated':
    axs.set(xlabel='Number of threads', ylabel='Number of allocated nodes (Thousands)')
  elif yaxis == 'retired':
    axs.set(xlabel='Number of threads', ylabel='Extra nodes (Thousands)')

  plt.axvline(144, linestyle='--', color='grey') 
  # axs.set_ylim(0)
  legend_x = 1
  legend_y = 0.5 
  plt.xticks(threads)
  # xticks = []
  # for i in range(129):
  #   if i % 32 == 0:
  #     xticks.append(i)
  # plt.xticks(xticks)
  # if printLegend:
  plt.grid()
  plt.legend(loc='center left', bbox_to_anchor=(legend_x, legend_y))
  plt.title(this_graph_title)
  plt.savefig(outdir+this_graph_title.replace(' ', '-').replace(':', '-').replace('%','')+'.png', bbox_inches='tight')
  plt.close('all')

  # if benchS == 0:
  #   plt.title('N = ' + str(benchN) + ', ' + str(benchS) + 'load only')
  # else
  #   plt.title('N = ' + str(benchN) + ', ' + str(benchS) + 'load only')


# plt.legend(loc='center left', bbox_to_anchor=(legend_x, legend_y))
#plt.xlabel('Number of threads')
#plt.ylabel('Throughput (Mop/s)')   
#plt.tight_layout()
#plt.show()


# print 'done'



def readFile(filename, results, stddev, threads, benchmarks, memory_managers):
  resultsRaw = {}
  key = {}
  val = {}

  with open(indir + filename) as f:
    for line in f:
      if 'Testing:' in line:
        line = line.replace('LinkList', 'LinkedList')
        num = getAllInts(line)
        key['th'] = num[0]
        if num[0] not in threads:
          threads.append(num[0])
        ds = line.split()[-1]
        if 'RCSS' in ds:
          key['mm'] = 'RCSS'
          key['ds'] = ds[:-4]
        elif 'RC' in ds:
          key['mm'] = 'RC'
          key['ds'] = ds[:-2]          
        else:
          key['ds'] = ds
      if 'Prefilled' in line:
        key['size'] = firstInt(line)
      if 'tracker = ' in line:
        key['mm'] = line.replace('\"', '').split()[-1]
      if 'Gets:' in line:
        key['workload'] = line.strip()
      if 'Average allocated nodes:' in line:
        val['allocated'] = firstFloat(line)
        # print(line)
        # print(val['allocated'])
      if 'Throughput: ' in line:
        val['throughput'] = firstFloat(line)
        val['retired'] = val['allocated'] - key['size']
        if key['ds'] == 'NatarajanTree':
          val['retired'] -= (key['size']+5)
        for result_type in val:
          key['result_type'] = result_type
          add_result(resultsRaw, key, val[result_type])
        if key['mm'] not in memory_managers:
          memory_managers.append(key['mm'])
        key['result_type'] = ''
        key['mm'] = ''
        add_benchmark(benchmarks, key)

  for key in resultsRaw:
    # print(key + ' : ' + str(len(resultsRaw[key])))
    results[key] = avg(resultsRaw[key][0:])
    stddev[key] = st.pstdev(resultsRaw[key][0:])
  # print(results)

def convert(exp_name):
  convert_ds = {'hashtable':'SortedUnorderedMap',
                'list':'LinkedList',
                'bst':'NatarajanTree',
  }
  ds = convert_ds[exp_name.split('-')[1]]
  size = exp_name.split('-')[2].replace('M', '000K').replace('K','000')
  up = int(exp_name.split('-')[3])
  return ds + ', size:' + str(size) + ', Gets:' + str(100-up) + ' Updates:' + str(up) + ' RQs: 0'

# def tostring(ds, wl):
#   return 'exp-'+ds+'-'+wl

# def testconvert():
#   toBenchmark = {'exp-hashtable-100K-10':'SortedUnorderedMap, size:100000, Gets:90 Updates:10 RQs: 0',
#                  'exp-list-1000-10':'LinkedList, size:1000, Gets:90 Updates:10 RQs: 0',
#                  'exp-bst-100K-50':'NatarajanTree, size:100000, Gets:50 Updates:50 RQs: 0',
#                  'exp-bst-100K-10':'NatarajanTree, size:100000, Gets:90 Updates:10 RQs: 0',
#                  'exp-bst-100K-1':'NatarajanTree, size:100000, Gets:99 Updates:1 RQs: 0',
#                  'exp-bst-100M-10':'NatarajanTree, size:100000000, Gets:90 Updates:10 RQs: 0',
#   }

#   experiments = [("list", '1000-10'),
#              ("hashtable", '100K-10'),
#              ("bst", '100K-10'),
#              ("bst", '100M-10'),
#              ("bst", '100K-1'),
#              ("bst", '100K-50'),]
#   for exp in experiments:
#     s = tostring(exp[0], exp[1])
#     if convert(s) != toBenchmark[s]:
#       return
#     else:
#       print(convert(s))
#   print('tests passed')

def graph_results_from_file(exp_name):
  results = {}
  stddev = {}
  threads = []
  benchmarks = []
  memory_managers = []

  # testconvert()

  readFile(exp_name+'.out', results, stddev, threads, benchmarks, memory_managers)
  threads.sort()
  # threads = threads[:-3]
  print(threads)
  print(benchmarks)
  print(memory_managers)

  memory_managers = ['RCU', 'Hazard', 'HazardOpt', 'Range_new', 'HE', 'NIL', 'RC', 'RCSS']





  create_graph(exp_name, results, stddev, convert(exp_name), memory_managers, threads, 'throughput', 'throughput', False, False)
  # create_graph(results, stddev, benchmarks, memory_managers, threads, 'allocated', 'allocated', True, True)
  create_graph(exp_name, results, stddev, convert(exp_name), memory_managers, threads, 'memory', 'retired', False, False)
  # create_graph(results, stddev, benchmarks, memory_managers, threads, 'retired-rc', 'retired', True, True, True)

  # create_graph(results, stddev, benchmarks, algs, threads, graph_title, False, False)

# graph_results_from_file('results_bench_trivial.txt', 'ARC Load-Store Benchmark')
# graph_results_from_file('results_intel_dec2_2019.txt', 'Benchmark (aware)')

# for th in threads:
#   s = str(th) + '\t'
#   d = {}
#   d['th'] = th
#   for alg in algs:
#     d['alg'] = alg
#     s += str(results[to_string(d)][0]) + '\t'
#   print(s)